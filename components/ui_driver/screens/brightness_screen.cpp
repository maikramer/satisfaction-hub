#include "screens/brightness_screen.hpp"
#include "ui_common.hpp"
#include "display_driver.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "src/widgets/slider/lv_slider.h"
#include "src/widgets/switch/lv_switch.h"
#include "src/widgets/bar/lv_bar.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <cstring>

// Callback para voltar (definido fora do namespace para acesso externo)
namespace ui {
namespace screens {
void (*brightness_on_back_callback)() = nullptr;
} // namespace screens
} // namespace ui

namespace ui {
namespace screens {

static const char* TAG = "BrightnessScreen";

static lv_obj_t* brightness_screen = nullptr;
static lv_obj_t* brightness_auto_switch = nullptr;
static lv_obj_t* brightness_slider = nullptr;
static lv_obj_t* brightness_value_label = nullptr;
static lv_obj_t* brightness_ldr_label = nullptr;
static esp_timer_handle_t brightness_save_timer = nullptr;

// Função helper para lock/unlock LVGL (definidas em ui_driver.cpp)
// Como estão no namespace anônimo, precisamos usar uma abordagem diferente
// Vamos usar um mutex externo diretamente ou criar wrappers
extern "C" {
    extern SemaphoreHandle_t lvgl_mutex;
    extern TaskHandle_t lvgl_task_handle;
}

static void brightness_lvgl_lock() {
    if (lvgl_mutex != nullptr) {
        if (xTaskGetCurrentTaskHandle() == lvgl_task_handle) {
            return; // Já estamos no task do LVGL, não precisa lock
        }
        xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    }
}

static void brightness_lvgl_unlock() {
    if (lvgl_mutex != nullptr) {
        if (xTaskGetCurrentTaskHandle() == lvgl_task_handle) {
            return; // Task do LVGL não adquiriu lock, nada a fazer
        }
        xSemaphoreGive(lvgl_mutex);
    }
}

// Declaração forward
static void update_brightness_labels();

static void brightness_auto_switch_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        bool enabled = lv_obj_has_state(brightness_auto_switch, LV_STATE_CHECKED);
        auto &display = DisplayDriver::instance();
        display.set_auto_brightness(enabled);
        
        // Mostrar/ocultar slider baseado no modo
        if (brightness_slider != nullptr) {
            if (enabled) {
                lv_obj_add_flag(brightness_slider, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(brightness_slider, LV_OBJ_FLAG_HIDDEN);
            }
        }
        
        ESP_LOGI(TAG, "Brilho automático %s", enabled ? "habilitado" : "desabilitado");
    }
}

// Callback do timer para salvar brilho após 1s sem modificação
static void brightness_save_timer_cb(void* arg) {
    auto &display = DisplayDriver::instance();
    if (!display.is_auto_brightness_enabled()) {
        display.save_brightness_settings();
        ESP_LOGI(TAG, "Brilho salvo após 1s sem modificação");
    }
}

static void brightness_slider_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        // Usar lv_slider_get_value para sliders no LVGL v9
        int32_t value = lv_slider_get_value(brightness_slider);
        auto &display = DisplayDriver::instance();
        display.set_brightness(static_cast<uint8_t>(value));
        
        // Atualizar label de valor
        if (brightness_value_label != nullptr) {
            lv_label_set_text_fmt(brightness_value_label, "Brilho: %ld%%", static_cast<long>(value));
        }
        
        ESP_LOGD(TAG, "Brilho manual ajustado para %ld%%", static_cast<long>(value));
        
        // Reiniciar timer de salvamento (debounce de 1s)
        if (brightness_save_timer != nullptr) {
            esp_timer_stop(brightness_save_timer);
            esp_timer_start_once(brightness_save_timer, 1000000); // 1 segundo em microsegundos
        }
    }
}

static void update_brightness_labels() {
    if (brightness_value_label == nullptr || brightness_ldr_label == nullptr) {
        return;
    }
    
    auto &display = DisplayDriver::instance();
    uint8_t brightness = display.get_brightness();
    uint16_t ldr_value = display.get_ldr_value();
    bool is_auto = display.is_auto_brightness_enabled();
    
    lv_label_set_text_fmt(brightness_value_label, "Brilho: %u%% %s", 
                          static_cast<unsigned int>(brightness), is_auto ? "(Auto)" : "(Manual)");
    lv_label_set_text_fmt(brightness_ldr_label, "LDR: %u", static_cast<unsigned int>(ldr_value));
}

// Task para atualizar labels periodicamente (só atualiza quando a tela está ativa)
static void brightness_update_task(void* pvParameters) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(500));  // Atualizar a cada 500ms
        
        // Só atualizar se a tela existir e estiver visível
        if (brightness_screen != nullptr && brightness_value_label != nullptr && brightness_ldr_label != nullptr) {
            brightness_lvgl_lock();
            update_brightness_labels();
            brightness_lvgl_unlock();
        }
    }
}

static TaskHandle_t brightness_update_task_handle = nullptr;

static void back_button_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Botão voltar brilho pressionado");
        if (brightness_on_back_callback) {
            brightness_on_back_callback();
        }
    }
}

void show_brightness_screen() {
    ESP_LOGI(TAG, "show_brightness_screen() iniciado");
    
    brightness_lvgl_lock();
    
    // Criar timer de salvamento se não existir
    if (brightness_save_timer == nullptr) {
        esp_timer_create_args_t timer_args = {
            .callback = brightness_save_timer_cb,
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "brightness_save",
            .skip_unhandled_events = false
        };
        esp_err_t ret = esp_timer_create(&timer_args, &brightness_save_timer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erro ao criar timer de salvamento: %s", esp_err_to_name(ret));
            brightness_save_timer = nullptr;
        }
    }
    
    // Limpar tela anterior se existir
    if (brightness_screen != nullptr) {
        lv_obj_del(brightness_screen);
        brightness_screen = nullptr;
    }
    
    // Criar nova tela
    brightness_screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(brightness_screen);
    common::apply_screen_style(brightness_screen);
    
    // Título usando helper
    common::create_screen_title(brightness_screen, "Brilho");
    
    // Layout base Y
    int32_t current_y = common::HEADER_HEIGHT + 20;
    
    // Label "Automático"
    lv_obj_t* auto_label = lv_label_create(brightness_screen);
    lv_label_set_text(auto_label, "Automático");
    common::apply_common_label_style(auto_label);
    lv_obj_align(auto_label, LV_ALIGN_TOP_LEFT, 20, current_y + 5); // +5 para centralizar com switch
    
    // Switch de brilho automático
    brightness_auto_switch = lv_switch_create(brightness_screen);
    lv_obj_set_size(brightness_auto_switch, 50, 25);
    lv_obj_align(brightness_auto_switch, LV_ALIGN_TOP_RIGHT, -20, current_y);
    
    // Configurar estado inicial do switch
    auto &display = DisplayDriver::instance();
    if (display.is_auto_brightness_enabled()) {
        lv_obj_add_state(brightness_auto_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(brightness_auto_switch, brightness_auto_switch_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    
    current_y += 45; // Espaço após switch
    
    // Slider de brilho manual
    brightness_slider = lv_slider_create(brightness_screen);
    lv_obj_set_size(brightness_slider, 280, 20);
    lv_obj_align(brightness_slider, LV_ALIGN_TOP_MID, 0, current_y);
    lv_slider_set_range(brightness_slider, 5, 100);
    lv_slider_set_value(brightness_slider, display.get_brightness(), LV_ANIM_OFF);
    
    // Ocultar slider se estiver em modo automático
    if (display.is_auto_brightness_enabled()) {
        lv_obj_add_flag(brightness_slider, LV_OBJ_FLAG_HIDDEN);
    }
    
    lv_obj_add_event_cb(brightness_slider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    
    current_y += 35; // Espaço após slider
    
    // Label com valor do brilho
    brightness_value_label = lv_label_create(brightness_screen);
    lv_obj_set_style_text_align(brightness_value_label, LV_TEXT_ALIGN_CENTER, 0);
    common::apply_common_label_style(brightness_value_label);
    lv_obj_align(brightness_value_label, LV_ALIGN_TOP_MID, 0, current_y);
    
    current_y += 25;
    
    // Label com valor do LDR (informação)
    brightness_ldr_label = lv_label_create(brightness_screen);
    lv_obj_set_style_text_align(brightness_ldr_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(brightness_ldr_label, common::COLOR_TEXT_GRAY(), 0);
    lv_obj_set_style_text_font(brightness_ldr_label, common::CAPTION_FONT, 0);
    lv_obj_align(brightness_ldr_label, LV_ALIGN_TOP_MID, 0, current_y);
    
    // Atualizar labels iniciais
    update_brightness_labels();
    
    // Criar task para atualizar labels periodicamente (apenas uma vez)
    if (brightness_update_task_handle == nullptr) {
        xTaskCreate(brightness_update_task, "brightness_update", 2048, nullptr, 1, &brightness_update_task_handle);
        ESP_LOGI(TAG, "Task de atualização de brilho criada");
    }
    
    // Botão de voltar usando helper
    common::create_back_button(brightness_screen, back_button_cb);
    
    // Carregar tela
    lv_screen_load(brightness_screen);
    lv_obj_invalidate(brightness_screen);
    
    brightness_lvgl_unlock();
    
    ESP_LOGI(TAG, "Tela de brilho criada e carregada");
}

} // namespace screens
} // namespace ui
