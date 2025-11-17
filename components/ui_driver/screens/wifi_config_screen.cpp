#include "screens/wifi_config_screen.hpp"
#include "screens/input_screen.hpp"
#include "screens/wifi_scan_screen.hpp"
#include "ui_common.hpp"
#include "ui_common_internal.hpp"
#include "wifi_manager.hpp"
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
            lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF0000), 0);
            lv_obj_invalidate(status_label);
            return;
        }
        
        // Atualizar status
        lv_label_set_text(status_label, "Conectando...");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x0000FF), 0);
        lv_obj_invalidate(status_label);
        
        // Tentar conectar (em uma task separada para não bloquear a UI)
        xTaskCreate([](void* arg) {
            const char* ssid = (const char*)arg;
            char password[65];
            strncpy(password, current_password, sizeof(password) - 1);
            password[sizeof(password) - 1] = '\0';
            
            auto& wifi = wifi::WiFiManager::instance();
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
                    lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF00), 0);
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
                    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF0000), 0);
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
    
    // Título
    lv_obj_t* title_label = lv_label_create(wifi_screen);
    lv_label_set_text(title_label, "Configurar WiFi");
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title_label, common::COLOR_TEXT_BLACK(), 0);
    lv_obj_set_style_text_font(title_label, common::TITLE_FONT, 0);
    lv_obj_set_style_pad_top(title_label, 4, 0);
    lv_obj_set_style_pad_bottom(title_label, 4, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Label SSID (alinhado horizontalmente com o input)
    lv_obj_t* ssid_label = lv_label_create(wifi_screen);
    lv_label_set_text(ssid_label, "SSID:");
    lv_obj_set_style_text_color(ssid_label, common::COLOR_TEXT_BLACK(), 0);
    lv_obj_set_style_text_font(ssid_label, common::TEXT_FONT, 0);
    lv_obj_set_size(ssid_label, 50, 35);
    lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 10, 50);
    
    // Botão/Label SSID (ao lado do label) - clicável para abrir tela de input
    lv_obj_t* ssid_button = lv_button_create(wifi_screen);
    lv_obj_set_size(ssid_button, 240, 35);
    lv_obj_align_to(ssid_button, ssid_label, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_set_style_bg_color(ssid_button, lv_color_white(), 0);
    lv_obj_set_style_border_color(ssid_button, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_border_width(ssid_button, 1, 0);
    lv_obj_set_style_radius(ssid_button, 4, 0);
    
    ssid_label_display = lv_label_create(ssid_button);
    lv_obj_set_style_text_color(ssid_label_display, common::COLOR_TEXT_BLACK(), 0);
    lv_obj_set_style_text_font(ssid_label_display, common::TEXT_FONT, 0);
    lv_label_set_text(ssid_label_display, "Toque para escanear");
    lv_obj_center(ssid_label_display);
    
    // Carregar SSID salvo se existir
    auto& wifi = wifi::WiFiManager::instance();
    if (wifi.is_connected() || strlen(wifi.config().ssid) > 0) {
        strncpy(current_ssid, wifi.config().ssid, sizeof(current_ssid) - 1);
        lv_label_set_text(ssid_label_display, current_ssid);
    }
    
    // Callback para abrir tela de scan quando clicar no SSID
    lv_obj_add_event_cb(ssid_button, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            ESP_LOGI(TAG, "SSID button clicked, opening scan screen");
            ui::screens::on_back_callback = show_wifi_config_screen;
            show_wifi_scan_screen([](const char* ssid) {
                // Callback quando uma rede é selecionada
                strncpy(current_ssid, ssid, sizeof(current_ssid) - 1);
                current_ssid[sizeof(current_ssid) - 1] = '\0';
                if (ssid_label_display) {
                    lv_label_set_text(ssid_label_display, current_ssid[0] ? current_ssid : "Toque para escanear");
                }
                ESP_LOGI(TAG, "SSID selecionado: %s", current_ssid);
            });
        }
    }, LV_EVENT_CLICKED, nullptr);
    
    // Label Senha (alinhado horizontalmente com o input)
    lv_obj_t* password_label = lv_label_create(wifi_screen);
    lv_label_set_text(password_label, "Senha:");
    lv_obj_set_style_text_color(password_label, common::COLOR_TEXT_BLACK(), 0);
    lv_obj_set_style_text_font(password_label, common::TEXT_FONT, 0);
    lv_obj_set_size(password_label, 50, 35);
    lv_obj_align_to(password_label, ssid_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 15);
    
    // Botão/Label Senha (ao lado do label) - clicável para abrir tela de input
    lv_obj_t* password_button = lv_button_create(wifi_screen);
    lv_obj_set_size(password_button, 240, 35);
    lv_obj_align_to(password_button, password_label, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_set_style_bg_color(password_button, lv_color_white(), 0);
    lv_obj_set_style_border_color(password_button, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_border_width(password_button, 1, 0);
    lv_obj_set_style_radius(password_button, 4, 0);
    
    password_label_display = lv_label_create(password_button);
    lv_obj_set_style_text_color(password_label_display, common::COLOR_TEXT_BLACK(), 0);
    lv_obj_set_style_text_font(password_label_display, common::TEXT_FONT, 0);
    lv_label_set_text(password_label_display, "Toque para digitar");
    lv_obj_center(password_label_display);
    
    // Callback para abrir tela de input quando clicar na senha
    lv_obj_add_event_cb(password_button, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            ESP_LOGI(TAG, "Password button clicked, opening input screen");
            show_input_screen(
                "Senha WiFi",
                "Senha da rede",
                current_password[0] ? current_password : nullptr,
                64,
                true,  // Modo senha
                [](const char* text, size_t len) {
                    // Callback quando confirmar
                    strncpy(current_password, text, sizeof(current_password) - 1);
                    current_password[sizeof(current_password) - 1] = '\0';
                    if (password_label_display) {
                        // Mostrar asteriscos para senha
                        char display[66] = {0};
                        for (size_t i = 0; i < len && i < sizeof(display) - 1; i++) {
                            display[i] = '*';
                        }
                        lv_label_set_text(password_label_display, display[0] ? display : "Toque para digitar");
                    }
                    ESP_LOGI(TAG, "Senha atualizada (tamanho: %zu)", len);
                },
                nullptr,  // Sem callback de cancelamento
                show_wifi_config_screen  // Voltar para tela WiFi quando fechar
            );
        }
    }, LV_EVENT_CLICKED, nullptr);
    
    // Status label
    status_label = lv_label_create(wifi_screen);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_label, common::COLOR_TEXT_BLACK(), 0);
    lv_obj_set_style_text_font(status_label, common::CAPTION_FONT, 0);
    lv_obj_set_width(status_label, 300);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    lv_obj_align_to(status_label, password_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
    
    // Mostrar status atual se conectado
    if (wifi.is_connected()) {
        lv_label_set_text_fmt(status_label, "Conectado: %s\nIP: %s", 
                             wifi.get_ssid(), wifi.get_ip());
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF00), 0);
    }
    
    // Botão conectar (ao lado do voltar)
    connect_button = lv_button_create(wifi_screen);
    lv_obj_set_size(connect_button, 140, 40);
    lv_obj_align(connect_button, LV_ALIGN_BOTTOM_MID, -80, -10);
    lv_obj_set_style_bg_color(connect_button, common::COLOR_BUTTON_BLUE(), 0);
    common::apply_common_button_style(connect_button);
    
    lv_obj_t* connect_label = lv_label_create(connect_button);
    lv_label_set_text(connect_label, "Conectar");
    lv_obj_center(connect_label);
    lv_obj_set_style_text_font(connect_label, common::TEXT_FONT, 0);
    
    lv_obj_add_event_cb(connect_button, connect_button_cb, LV_EVENT_CLICKED, nullptr);
    
    // Botão voltar (ao lado do conectar)
    back_button = lv_button_create(wifi_screen);
    lv_obj_set_size(back_button, 140, 40);
    lv_obj_align_to(back_button, connect_button, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_set_style_bg_color(back_button, common::COLOR_BUTTON_GRAY(), 0);
    common::apply_common_button_style(back_button);
    
    lv_obj_t* back_label = lv_label_create(back_button);
    lv_label_set_text(back_label, "Voltar");
    lv_obj_center(back_label);
    lv_obj_set_style_text_font(back_label, common::TEXT_FONT, 0);
    
    lv_obj_add_event_cb(back_button, back_button_cb, LV_EVENT_CLICKED, nullptr);
    
    ESP_LOGI(TAG, "create_wifi_config_screen() concluído");
}

void show_wifi_config_screen() {
    ESP_LOGI(TAG, "show_wifi_config_screen() chamado");
    lvgl_lock();
    
    if (wifi_screen == nullptr) {
        ESP_LOGI(TAG, "Criando tela WiFi (wifi_screen é nullptr)");
        create_wifi_config_screen();
        ESP_LOGI(TAG, "Tela WiFi criada: %p", wifi_screen);
    } else {
        ESP_LOGI(TAG, "Reutilizando tela WiFi existente: %p", wifi_screen);
    }
    
    ESP_LOGI(TAG, "Carregando tela WiFi...");
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

