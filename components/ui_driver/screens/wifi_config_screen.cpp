#include "screens/wifi_config_screen.hpp"
#include "screens/input_screen.hpp"
#include "screens/wifi_scan_screen.hpp"
#include "ui_common.hpp"
#include "ui_common_internal.hpp"
#include "WiFiManager.h"
#include "esp_log.h"
#include "lvgl.h"
#include "widgets/textarea/lv_textarea.h"
#include "display_driver.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

namespace ui {
namespace screens {

static const char* TAG = "WiFiConfigScreen";

// Callback para voltar (será definido externamente)
void (*on_back_callback)() = nullptr;

// Callback original salvo antes de abrir tela de scan
static void (*saved_back_callback)() = nullptr;

static lv_obj_t* wifi_screen = nullptr;
static lv_obj_t* ssid_label_display = nullptr;  // Label para mostrar o SSID atual
static lv_obj_t* password_label_display = nullptr;  // Label para mostrar a senha atual
static lv_obj_t* status_label = nullptr;
static lv_obj_t* connect_button = nullptr;
static lv_obj_t* back_button = nullptr;

// Valores atuais
static char current_ssid[33] = {0};
static char current_password[65] = {0};

static void connect_button_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Botão conectar WiFi pressionado");
        
        // Obter SSID e senha dos valores salvos
        const char* ssid = current_ssid;
        const char* password = current_password;
        
        ESP_LOGI(TAG, "Tentando conectar - SSID: '%s', Senha length: %zu", 
                 ssid ? ssid : "(null)", password ? strlen(password) : 0);
        
        if (ssid == nullptr || strlen(ssid) == 0) {
            ESP_LOGE(TAG, "SSID vazio!");
            lv_label_set_text(status_label, "Erro: SSID vazio");
            lv_obj_set_style_text_color(status_label, common::COLOR_ERROR(), 0);
            lv_obj_invalidate(status_label);
            return;
        }
        
        // Atualizar status
        lv_label_set_text(status_label, "Conectando...");
        lv_obj_set_style_text_color(status_label, common::COLOR_BUTTON_BLUE(), 0);
        lv_obj_invalidate(status_label);
        
        // Tentar conectar (em uma task separada para não bloquear a UI)
        xTaskCreate([](void* arg) {
            const char* ssid = (const char*)arg;
            char password[65];
            strncpy(password, current_password, sizeof(password) - 1);
            password[sizeof(password) - 1] = '\0';
            
            auto& wifi = WiFiManager::instance();
            esp_err_t err = wifi.connect(ssid, password);
            
            lvgl_lock();
            if (err == ESP_OK) {
                const char* ip = wifi.get_ip();
                ESP_LOGI(TAG, "Conexão bem-sucedida! IP: %s", ip ? ip : "(null)");
                if (status_label) {
                    if (ip) {
                        lv_label_set_text_fmt(status_label, "Conectado! IP: %s", ip);
                    } else {
                        lv_label_set_text(status_label, "Conectado!");
                    }
                    lv_obj_set_style_text_color(status_label, common::COLOR_SUCCESS(), 0);
                }
                // Atualizar ícone WiFi na tela principal será feito automaticamente pelo update()
            } else {
                ESP_LOGE(TAG, "Erro ao conectar: %s", esp_err_to_name(err));
                if (status_label) {
                    if (err == ESP_ERR_TIMEOUT) {
                        lv_label_set_text(status_label, "Timeout ao conectar");
                    } else if (err == ESP_FAIL) {
                        lv_label_set_text(status_label, "Senha incorreta?");
                    } else {
                        lv_label_set_text(status_label, "Erro ao conectar");
                    }
                    lv_obj_set_style_text_color(status_label, common::COLOR_ERROR(), 0);
                }
            }
            if (status_label) {
                lv_obj_invalidate(status_label);
            }
            lvgl_unlock();
            
            vTaskDelete(nullptr);
        }, "wifi_connect_task", 4096, (void*)ssid, 5, nullptr);
    }
}

static void back_button_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Botão voltar WiFi pressionado");
        if (on_back_callback) {
            on_back_callback();
        }
    }
}

// Helper para criar botão com estilo de input
static lv_obj_t* create_input_button(lv_obj_t* parent, lv_obj_t** label_out) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, 240, common::INPUT_HEIGHT); // Mesma altura do input real
    
    lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
    lv_obj_set_style_border_color(btn, common::COLOR_BORDER(), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    
    *label_out = lv_label_create(btn);
    lv_obj_set_style_text_color(*label_out, common::COLOR_TEXT_BLACK(), 0);
    lv_obj_set_style_text_font(*label_out, common::TEXT_FONT, 0);
    lv_obj_center(*label_out);
    
    return btn;
}

void create_wifi_config_screen() {
    ESP_LOGI(TAG, "create_wifi_config_screen() iniciado");
    if (wifi_screen != nullptr) {
        ESP_LOGW(TAG, "Tela WiFi já existe, deletando antes de recriar");
        lv_obj_del(wifi_screen);
    }
    
    ESP_LOGI(TAG, "Criando objeto wifi_screen...");
    wifi_screen = lv_obj_create(nullptr);
    ESP_LOGI(TAG, "wifi_screen criado: %p", wifi_screen);
    lv_obj_remove_style_all(wifi_screen);
    common::apply_screen_style(wifi_screen);
    
    // Título usando helper
    lv_obj_t* title_label = common::create_screen_title(wifi_screen, "Configurar WiFi");
    
    // Layout Y de referência
    int32_t current_y = common::HEADER_HEIGHT + 10;
    
    // Label SSID
    lv_obj_t* ssid_label = lv_label_create(wifi_screen);
    lv_label_set_text(ssid_label, "SSID:");
    common::apply_common_label_style(ssid_label);
    lv_obj_set_size(ssid_label, 50, common::INPUT_HEIGHT);
    lv_obj_set_style_text_align(ssid_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_top(ssid_label, 10, 0); // Centralizar verticalmente com o botão
    lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 10, current_y);
    
    // Botão SSID (input lookalike)
    lv_obj_t* ssid_button = create_input_button(wifi_screen, &ssid_label_display);
    lv_obj_align_to(ssid_button, ssid_label, LV_ALIGN_OUT_RIGHT_MID, 5, -5); // Ajuste fino
    lv_label_set_text(ssid_label_display, "Toque para escanear");
    
    // Carregar SSID salvo se existir
    auto& wifi = WiFiManager::instance();
    if (wifi.is_connected() || strlen(wifi.config().ssid) > 0) {
        strncpy(current_ssid, wifi.config().ssid, sizeof(current_ssid) - 1);
        lv_label_set_text(ssid_label_display, current_ssid);
    }
    
    // Evento SSID
    lv_obj_add_event_cb(ssid_button, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            ESP_LOGI(TAG, "SSID button clicked, opening scan screen");
            saved_back_callback = ui::screens::on_back_callback;
            ui::screens::on_back_callback = show_wifi_config_screen;
            show_wifi_scan_screen([](const char* ssid) {
                strncpy(current_ssid, ssid, sizeof(current_ssid) - 1);
                current_ssid[sizeof(current_ssid) - 1] = '\0';
                if (ssid_label_display) {
                    lv_label_set_text(ssid_label_display, current_ssid[0] ? current_ssid : "Toque para escanear");
                }
                ESP_LOGI(TAG, "SSID selecionado: %s", current_ssid);
            });
        }
    }, LV_EVENT_CLICKED, nullptr);
    
    // Próxima linha
    current_y += common::INPUT_HEIGHT + 15;
    
    // Label Senha
    lv_obj_t* password_label = lv_label_create(wifi_screen);
    lv_label_set_text(password_label, "Senha:");
    common::apply_common_label_style(password_label);
    lv_obj_set_size(password_label, 50, common::INPUT_HEIGHT);
    lv_obj_set_style_text_align(password_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_top(password_label, 10, 0);
    lv_obj_align(password_label, LV_ALIGN_TOP_LEFT, 10, current_y);
    
    // Botão Senha (input lookalike)
    lv_obj_t* password_button = create_input_button(wifi_screen, &password_label_display);
    lv_obj_align_to(password_button, password_label, LV_ALIGN_OUT_RIGHT_MID, 5, -5);
    lv_label_set_text(password_label_display, "Toque para digitar");
    
    // Evento Senha
    lv_obj_add_event_cb(password_button, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            ESP_LOGI(TAG, "Password button clicked, opening input screen");
            show_input_screen(
                "Senha WiFi",
                "Senha da rede",
                current_password[0] ? current_password : nullptr,
                64,
                true,
                [](const char* text, size_t len) {
                    strncpy(current_password, text, sizeof(current_password) - 1);
                    current_password[sizeof(current_password) - 1] = '\0';
                    if (password_label_display) {
                        char display[66] = {0};
                        for (size_t i = 0; i < len && i < sizeof(display) - 1; i++) {
                            display[i] = '*';
                        }
                        lv_label_set_text(password_label_display, display[0] ? display : "Toque para digitar");
                    }
                    ESP_LOGI(TAG, "Senha atualizada (tamanho: %zu)", len);
                },
                nullptr,
                show_wifi_config_screen
            );
        }
    }, LV_EVENT_CLICKED, nullptr);
    
    // Próxima linha (status)
    current_y += common::INPUT_HEIGHT + 20;
    
    // Status label
    status_label = lv_label_create(wifi_screen);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_label, common::COLOR_TEXT_BLACK(), 0);
    lv_obj_set_style_text_font(status_label, common::CAPTION_FONT, 0);
    lv_obj_set_width(status_label, 300);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, current_y);
    
    if (wifi.is_connected()) {
        lv_label_set_text_fmt(status_label, "Conectado: %s\nIP: %s", 
                             wifi.get_ssid(), wifi.get_ip());
        lv_obj_set_style_text_color(status_label, common::COLOR_SUCCESS(), 0);
    }
    
    // Botões na parte inferior
    // Conectar
    connect_button = common::create_button(wifi_screen, "Conectar", 140, common::COLOR_BUTTON_BLUE());
    lv_obj_align(connect_button, LV_ALIGN_BOTTOM_MID, -75, -common::SCREEN_PADDING); // Esquerda do centro
    lv_obj_add_event_cb(connect_button, connect_button_cb, LV_EVENT_CLICKED, nullptr);
    
    // Voltar
    back_button = common::create_button(wifi_screen, "Voltar", 140, common::COLOR_BUTTON_GRAY());
    lv_obj_align(back_button, LV_ALIGN_BOTTOM_MID, 75, -common::SCREEN_PADDING); // Direita do centro
    lv_obj_add_event_cb(back_button, back_button_cb, LV_EVENT_CLICKED, nullptr);
    
    ESP_LOGI(TAG, "create_wifi_config_screen() concluído");
}

void show_wifi_config_screen() {
    ESP_LOGI(TAG, "show_wifi_config_screen() chamado");
    
    // Se voltamos da tela de scan, restaurar callback original
    if (ui::screens::on_back_callback == show_wifi_config_screen && saved_back_callback != nullptr) {
        ESP_LOGI(TAG, "Restaurando callback original após voltar da tela de scan");
        ui::screens::on_back_callback = saved_back_callback;
        saved_back_callback = nullptr;
    }
    
    // Se o callback não está definido ou está incorreto, garantir que não seja show_wifi_config_screen
    if (ui::screens::on_back_callback == show_wifi_config_screen) {
        ESP_LOGW(TAG, "Callback incorreto detectado (show_wifi_config_screen), limpando para evitar loop");
        ui::screens::on_back_callback = nullptr;
    }
    
    if (ui::screens::on_back_callback == nullptr) {
        ESP_LOGW(TAG, "Callback não definido - botão voltar pode não funcionar corretamente");
    }
    
    lvgl_lock();
    
    if (wifi_screen == nullptr) {
        create_wifi_config_screen();
    }
    
    lv_screen_load(wifi_screen);
    lv_obj_invalidate(wifi_screen);
    
    lvgl_unlock();
    ESP_LOGI(TAG, "show_wifi_config_screen() concluído");
}

void destroy_wifi_config_screen() {
    if (wifi_screen != nullptr) {
        lvgl_lock();
        lv_obj_del(wifi_screen);
        wifi_screen = nullptr;
        ssid_label_display = nullptr;
        password_label_display = nullptr;
        status_label = nullptr;
        connect_button = nullptr;
        back_button = nullptr;
        current_ssid[0] = '\0';
        current_password[0] = '\0';
        lvgl_unlock();
    }
}

} // namespace screens
} // namespace ui
