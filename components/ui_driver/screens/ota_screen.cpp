#include "screens/ota_screen.hpp"
#include "ui_common.hpp"
#include "ui_common_internal.hpp" // Para lvgl_lock() e lvgl_unlock()
#include "OtaManager.h"
#include "WiFiManager.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include <cstring>

// Declarar fonte Roboto customizada (suporta acentuação)
LV_FONT_DECLARE(roboto);

namespace {
constexpr char TAG[] = "OTA_SCREEN";

lv_obj_t* ota_screen = nullptr;
lv_obj_t* ota_title_label = nullptr;
lv_obj_t* ota_status_label = nullptr;
lv_obj_t* ota_progress_bar = nullptr;
lv_obj_t* ota_progress_label = nullptr;
lv_obj_t* ota_info_label = nullptr;

bool ota_in_progress = false;
} // namespace

namespace ui::screens {

void update_ota_progress(int progress) {
    if (ota_screen == nullptr || ota_progress_bar == nullptr) {
        return;
    }
    
    lvgl_lock();
    
    lv_bar_set_value(ota_progress_bar, progress, LV_ANIM_ON);
    
    if (ota_progress_label != nullptr) {
        char progress_text[32];
        snprintf(progress_text, sizeof(progress_text), "%d%%", progress);
        lv_label_set_text(ota_progress_label, progress_text);
    }
    
    lvgl_unlock();
    
    ESP_LOGI(TAG, "Progresso OTA: %d%%", progress);
}

void show_ota_error(const char* errorMsg) {
    if (ota_status_label == nullptr) {
        return;
    }
    
    lvgl_lock();
    
    lv_label_set_text(ota_status_label, errorMsg);
    lv_obj_set_style_text_color(ota_status_label, lv_color_hex(0xFF0000), 0);
    
    if (ota_progress_bar != nullptr) {
        lv_bar_set_value(ota_progress_bar, 0, LV_ANIM_OFF);
    }
    
    if (ota_progress_label != nullptr) {
        lv_label_set_text(ota_progress_label, "0%");
    }
    
    lvgl_unlock();
    
    ota_in_progress = false;
    ESP_LOGE(TAG, "Erro OTA: %s", errorMsg);
}

void cleanup_ota_screen() {
    if (ota_screen != nullptr) {
        lv_obj_del(ota_screen);
        ota_screen = nullptr;
        ota_title_label = nullptr;
        ota_status_label = nullptr;
        ota_progress_bar = nullptr;
        ota_progress_label = nullptr;
        ota_info_label = nullptr;
    }
    ota_in_progress = false;
}

static void start_ota_update(const char* otaUrl) {
    auto& wifi = WiFiManager::instance();
    if (!wifi.is_connected()) {
        show_ota_error("WiFi não conectado");
        return;
    }
    
    auto& otaManager = OtaManager::instance();
    otaManager.init();
    
    ESP_LOGI(TAG, "Iniciando atualização OTA...");
    
    // Usar URL padrão se não fornecida
    // Para desenvolvimento: usar IP da máquina local (ex: http://192.168.0.100:10234/ota)
    // Para produção: configurar via menuconfig ou storage
    const char* defaultUrl = otaUrl ? otaUrl : "http://192.168.0.100:10234/ota";
    
    ESP_LOGI(TAG, "URL OTA: %s", defaultUrl);
    
    ErrorCode result = otaManager.startUpdate(defaultUrl, nullptr);
    if (result != CommonErrorCodes::None) {
        char errorMsg[64];
        snprintf(errorMsg, sizeof(errorMsg), "Erro ao iniciar: %s", result.description().c_str());
        show_ota_error(errorMsg);
    }
}

void show_ota_screen(const char* otaUrl) {
    ESP_LOGI(TAG, "show_ota_screen chamado");
    
    if (ota_in_progress && ota_screen != nullptr) {
        ESP_LOGW(TAG, "OTA já em progresso");
        return;
    }
    
    // Verificar WiFi
    ESP_LOGI(TAG, "Verificando WiFi...");
    auto& wifi = WiFiManager::instance();
    if (!wifi.is_connected()) {
        ESP_LOGE(TAG, "WiFi não conectado para OTA");
        return;
    }
    ESP_LOGI(TAG, "WiFi conectado");
    
    // Inicializar OtaManager ANTES de pegar o mutex (pode fazer operações bloqueantes)
    ESP_LOGI(TAG, "Inicializando OtaManager...");
    auto& otaManager = OtaManager::instance();
    esp_err_t init_err = otaManager.init();
    if (init_err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao inicializar OtaManager: %s", esp_err_to_name(init_err));
        return;
    }
    ESP_LOGI(TAG, "OtaManager inicializado, criando tela...");
    
    // Usar lvgl_lock() que verifica se já estamos na task do LVGL
    lvgl_lock();
    
    // Limpar tela anterior se existir
    cleanup_ota_screen();
    
    // Criar nova tela
    ota_screen = lv_obj_create(nullptr);
    lv_obj_set_size(ota_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(ota_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_all(ota_screen, 20, 0);
    lv_obj_set_layout(ota_screen, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ota_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ota_screen, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Título
    ota_title_label = lv_label_create(ota_screen);
    lv_label_set_text(ota_title_label, "Atualização OTA");
    lv_obj_set_style_text_font(ota_title_label, &roboto, 0);
    lv_obj_set_style_text_color(ota_title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(ota_title_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // Status
    ota_status_label = lv_label_create(ota_screen);
    lv_label_set_text(ota_status_label, "Preparando atualização...");
    lv_obj_set_style_text_font(ota_status_label, &roboto, 0);
    lv_obj_set_style_text_color(ota_status_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(ota_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(ota_status_label, LV_PCT(90));
    lv_label_set_long_mode(ota_status_label, LV_LABEL_LONG_WRAP);
    
    // Barra de progresso
    ota_progress_bar = lv_bar_create(ota_screen);
    lv_obj_set_size(ota_progress_bar, LV_PCT(80), 30);
    lv_obj_set_style_bg_color(ota_progress_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ota_progress_bar, lv_color_hex(0x00AA00), LV_PART_INDICATOR);
    lv_bar_set_value(ota_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(ota_progress_bar, 5, 0);
    
    // Label de progresso
    ota_progress_label = lv_label_create(ota_screen);
    lv_label_set_text(ota_progress_label, "0%");
    lv_obj_set_style_text_font(ota_progress_label, &roboto, 0);
    lv_obj_set_style_text_color(ota_progress_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(ota_progress_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // Info label (device ID, etc)
    ota_info_label = lv_label_create(ota_screen);
    // OtaManager já foi inicializado antes do mutex
    char info_text[128];
    snprintf(info_text, sizeof(info_text), "Device ID: %s", otaManager.getDeviceId());
    lv_label_set_text(ota_info_label, info_text);
    lv_obj_set_style_text_font(ota_info_label, &roboto, 0);
    lv_obj_set_style_text_color(ota_info_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_align(ota_info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(ota_info_label, LV_PCT(90));
    lv_label_set_long_mode(ota_info_label, LV_LABEL_LONG_WRAP);
    
    ESP_LOGI(TAG, "Carregando tela OTA...");
    lv_screen_load(ota_screen);
    lv_obj_invalidate(ota_screen);
    
    ESP_LOGI(TAG, "Liberando mutex LVGL...");
    lvgl_unlock();
    ESP_LOGI(TAG, "Mutex LVGL liberado");
    
    // Configurar eventos do OtaManager (apenas uma vez)
    // IMPORTANTE: Configurar eventos FORA do contexto do mutex LVGL
    static bool events_configured = false;
    if (!events_configured) {
        ESP_LOGI(TAG, "Configurando eventos do OtaManager");
        otaManager.onUpdateStart.addHandler([]() {
            ESP_LOGI(TAG, "OTA iniciado");
            lvgl_lock();
            if (ota_status_label != nullptr) {
                lv_label_set_text(ota_status_label, "Baixando atualização...");
                lv_obj_set_style_text_font(ota_status_label, &roboto, 0); // Garantir fonte Roboto
                lv_obj_set_style_text_color(ota_status_label, lv_color_hex(0x00AAFF), 0);
            }
            if (ota_progress_bar != nullptr) {
                lv_bar_set_value(ota_progress_bar, 0, LV_ANIM_OFF);
            }
            if (ota_progress_label != nullptr) {
                lv_label_set_text(ota_progress_label, "0%");
            }
            lvgl_unlock();
            ota_in_progress = true;
        });
        
        otaManager.onProgress.addHandler([](int progress) {
            update_ota_progress(progress);
        });
        
        otaManager.onUpdateComplete.addHandler([]() {
            ESP_LOGI(TAG, "OTA concluído com sucesso");
            lvgl_lock();
            if (ota_status_label != nullptr) {
                lv_label_set_text(ota_status_label, "Atualização concluída!\nReiniciando...");
                lv_obj_set_style_text_font(ota_status_label, &roboto, 0); // Garantir fonte Roboto
                lv_obj_set_style_text_color(ota_status_label, lv_color_hex(0x00FF00), 0);
            }
            if (ota_progress_bar != nullptr) {
                lv_bar_set_value(ota_progress_bar, 100, LV_ANIM_ON);
            }
            if (ota_progress_label != nullptr) {
                lv_label_set_text(ota_progress_label, "100%");
            }
            lvgl_unlock();
            
            // Reiniciar após 2 segundos
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
        });
        
        otaManager.onUpdateFailed.addHandler([]() {
            show_ota_error("Falha na atualização");
        });
        
        events_configured = true;
    }
    
    // Iniciar atualização em uma task separada para não bloquear UI
    // Criar cópia da URL se fornecida, ou usar nullptr
    ESP_LOGI(TAG, "Criando task para iniciar OTA...");
    char* url_copy = nullptr;
    if (otaUrl != nullptr) {
        size_t url_len = strlen(otaUrl) + 1;
        url_copy = static_cast<char*>(pvPortMalloc(url_len));
        if (url_copy != nullptr) {
            strncpy(url_copy, otaUrl, url_len);
            url_copy[url_len - 1] = '\0';
            ESP_LOGI(TAG, "URL copiada: %s", url_copy);
        } else {
            ESP_LOGE(TAG, "Falha ao alocar memória para URL");
        }
    }
    
    BaseType_t task_result = xTaskCreate([](void* param) {
        char* url = static_cast<char*>(param);
        ESP_LOGI(TAG, "Task OTA iniciada, URL: %s", url ? url : "null");
        vTaskDelay(pdMS_TO_TICKS(500)); // Pequeno delay para UI aparecer
        start_ota_update(url);
        if (url != nullptr) {
            vPortFree(url);
        }
        vTaskDelete(nullptr);
    }, "ota_task", 4096, url_copy, 5, nullptr);
    
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar task OTA");
        if (url_copy != nullptr) {
            vPortFree(url_copy);
        }
    } else {
        ESP_LOGI(TAG, "Task OTA criada com sucesso");
    }
}

} // namespace ui::screens
