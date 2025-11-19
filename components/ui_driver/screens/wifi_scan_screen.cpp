#include "screens/wifi_scan_screen.hpp"
#include "ui_common.hpp"
#include "ui_common_internal.hpp"
#include "wifi_manager.hpp"
#include "esp_log.h"
#include "lvgl.h"
#include <cstring>
#include <algorithm>

namespace ui {
namespace screens {

static const char* TAG = "WiFiScanScreen";

// Usar o on_back_callback de wifi_config_screen
extern void (*on_back_callback)();

static lv_obj_t* scan_screen = nullptr;
static lv_obj_t* title_label = nullptr;
static lv_obj_t* status_label = nullptr;
static lv_obj_t* list_obj = nullptr;
static lv_obj_t* back_button = nullptr;
static WiFiScanCallback s_on_select = nullptr;

// Estrutura para armazenar informações das redes
struct NetworkInfo {
    char ssid[33];
    int8_t rssi;
    bool has_password;
};

static NetworkInfo networks[20];  // Máximo 20 redes
static int network_count = 0;

// Função para comparar redes por RSSI (mais forte primeiro)
static bool compare_networks(const NetworkInfo& a, const NetworkInfo& b) {
    return a.rssi > b.rssi;
}

static void back_button_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Botão voltar pressionado");
        if (on_back_callback) {
            on_back_callback();
        }
        hide_wifi_scan_screen();
    }
}

static void network_button_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_t* btn = lv_event_get_target_obj(e);
        int index = (int)(intptr_t)lv_obj_get_user_data(btn);
        
        if (index >= 0 && index < network_count && s_on_select) {
            ESP_LOGI(TAG, "Rede selecionada: %s", networks[index].ssid);
            s_on_select(networks[index].ssid);
            hide_wifi_scan_screen();
            if (on_back_callback) {
                on_back_callback();
            }
        }
    }
}

void show_wifi_scan_screen(WiFiScanCallback on_select) {
    ESP_LOGI(TAG, "show_wifi_scan_screen chamado");
    
    s_on_select = on_select;
    
    lvgl_lock();
    
    // Limpar tela anterior se existir
    if (scan_screen != nullptr) {
        lv_obj_del(scan_screen);
        scan_screen = nullptr;
    }
    
    // Criar nova tela
    scan_screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(scan_screen);
    common::apply_screen_style(scan_screen);
    lv_obj_set_flex_flow(scan_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scan_screen,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(scan_screen, 8, 0);
    lv_obj_set_style_pad_row(scan_screen, 6, 0);
    
    // Título
    title_label = lv_label_create(scan_screen);
    lv_label_set_text(title_label, "Escaneando WiFi...");
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title_label, common::COLOR_TEXT_BLACK(), 0);
    lv_obj_set_style_text_font(title_label, common::TITLE_FONT, 0);
    lv_obj_set_style_pad_top(title_label, 2, 0);
    lv_obj_set_style_pad_bottom(title_label, 2, 0);
    lv_obj_set_width(title_label, lv_pct(100));
    
    // Status label
    status_label = lv_label_create(scan_screen);
    lv_label_set_text(status_label, "Buscando redes...");
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_label, common::COLOR_TEXT_BLACK(), 0);
    lv_obj_set_style_text_font(status_label, common::CAPTION_FONT, 0);
    lv_obj_set_width(status_label, lv_pct(100));
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    
    // Container scrollável para lista de redes (será preenchida após o scan)
    list_obj = lv_obj_create(scan_screen);
    lv_obj_set_width(list_obj, lv_pct(100));
    lv_obj_set_style_flex_grow(list_obj, 1, 0);
    lv_obj_set_style_bg_color(list_obj, lv_color_white(), 0);
    lv_obj_set_style_border_color(list_obj, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_border_width(list_obj, 1, 0);
    lv_obj_set_style_radius(list_obj, 8, 0);
    lv_obj_set_style_pad_all(list_obj, 6, 0);
    lv_obj_set_style_pad_row(list_obj, 6, 0);
    lv_obj_set_flex_flow(list_obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_obj,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(list_obj, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_scroll_dir(list_obj, LV_DIR_VER);
    lv_obj_clear_flag(list_obj, LV_OBJ_FLAG_SCROLL_ELASTIC);  // Desabilitar scroll elástico
    
    // Botão voltar
    back_button = lv_button_create(scan_screen);
    lv_obj_set_width(back_button, lv_pct(60));
    lv_obj_set_height(back_button, 36);
    lv_obj_set_style_pad_ver(back_button, 4, 0);
    lv_obj_set_style_margin_top(back_button, 4, 0);
    lv_obj_set_style_bg_color(back_button, common::COLOR_BUTTON_GRAY(), 0);
    common::apply_common_button_style(back_button);
    
    lv_obj_t* back_label = lv_label_create(back_button);
    lv_label_set_text(back_label, "Voltar");
    lv_obj_center(back_label);
    lv_obj_set_style_text_font(back_label, common::TEXT_FONT, 0);
    
    lv_obj_add_event_cb(back_button, back_button_cb, LV_EVENT_CLICKED, nullptr);
    
    // Carregar a tela primeiro para mostrar "Escaneando..."
    lv_screen_load(scan_screen);
    lv_obj_invalidate(scan_screen);
    
    lvgl_unlock();
    
    // Fazer scan em uma task separada para não bloquear a UI
    // (mas vamos fazer de forma simples primeiro, depois podemos otimizar)
    ESP_LOGI(TAG, "Iniciando scan WiFi...");
    
    // Fazer scan
    wifi_ap_record_t ap_records[20];
    auto& wifi = wifi::WiFiManager::instance();
    network_count = wifi.scan(ap_records, 20);
    
    lvgl_lock();
    
    if (network_count < 0) {
        ESP_LOGE(TAG, "Erro ao fazer scan WiFi");
        lv_label_set_text(status_label, "Erro ao escanear redes");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF0000), 0);
    } else if (network_count == 0) {
        ESP_LOGW(TAG, "Nenhuma rede encontrada");
        lv_label_set_text(status_label, "Nenhuma rede encontrada");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF9800), 0);
    } else {
        ESP_LOGI(TAG, "Encontradas %d redes", network_count);
        
        // Copiar informações para array local
        for (int i = 0; i < network_count; i++) {
            strncpy(networks[i].ssid, (const char*)ap_records[i].ssid, sizeof(networks[i].ssid) - 1);
            networks[i].ssid[sizeof(networks[i].ssid) - 1] = '\0';
            networks[i].rssi = ap_records[i].rssi;
            networks[i].has_password = (ap_records[i].authmode != WIFI_AUTH_OPEN);
        }
        
        // Ordenar por RSSI (mais forte primeiro)
        std::sort(networks, networks + network_count, compare_networks);
        
        // Limpar container anterior
        lv_obj_clean(list_obj);
        
        // Adicionar redes ao container
        for (int i = 0; i < network_count; i++) {
            // Criar botão para cada rede
            lv_obj_t* btn = lv_button_create(list_obj);
            lv_obj_set_width(btn, lv_pct(100));
            lv_obj_set_height(btn, 34);
            lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(0xCCCCCC), 0);
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_radius(btn, 6, 0);
            lv_obj_set_style_pad_hor(btn, 8, 0);
            lv_obj_set_style_pad_ver(btn, 4, 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(btn,
                                  LV_FLEX_ALIGN_SPACE_BETWEEN,
                                  LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            
            // Criar label com SSID
            char ssid_display[50];
            strncpy(ssid_display, networks[i].ssid, sizeof(ssid_display) - 1);
            ssid_display[sizeof(ssid_display) - 1] = '\0';
            
            lv_obj_t* label = lv_label_create(btn);
            lv_label_set_text(label, ssid_display);
            lv_obj_set_style_text_color(label, common::COLOR_TEXT_BLACK(), 0);
            lv_obj_set_style_text_font(label, common::TEXT_FONT, 0);
            lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_width(label, lv_pct(65));
            
            // Indicador de senha + força do sinal (RSSI)
            const char* security_text = networks[i].has_password ? "Senha" : "Aberto";
            const char* rssi_text = nullptr;
            if (networks[i].rssi > -50) {
                rssi_text = "Excelente";
            } else if (networks[i].rssi > -70) {
                rssi_text = "Bom";
            } else {
                rssi_text = "Fraco";
            }
            
            char info_text[32];
            snprintf(info_text, sizeof(info_text), "%s | %s", security_text, rssi_text);
            
            lv_obj_t* info_label = lv_label_create(btn);
            lv_label_set_text(info_label, info_text);
            lv_obj_set_style_text_color(info_label, lv_color_hex(0x757575), 0);
            lv_obj_set_style_text_font(info_label, common::CAPTION_FONT, 0);
            lv_label_set_long_mode(info_label, LV_LABEL_LONG_CLIP);
            lv_obj_set_width(info_label, lv_pct(35));
            lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_RIGHT, 0);
            
            // Armazenar índice como user_data
            lv_obj_set_user_data(btn, (void*)(intptr_t)i);
            
            // Callback para seleção
            lv_obj_add_event_cb(btn, network_button_cb, LV_EVENT_CLICKED, nullptr);
        }
        
        // Atualizar status
        char status_text[64];
        snprintf(status_text, sizeof(status_text), "%d rede(s) encontrada(s)", network_count);
        lv_label_set_text(status_label, status_text);
        lv_obj_set_style_text_color(status_label, common::COLOR_TEXT_BLACK(), 0);
        lv_label_set_text(title_label, "Selecione uma rede");
    }
    
    lv_obj_invalidate(scan_screen);
    
    lvgl_unlock();
    
    ESP_LOGI(TAG, "Tela de scan WiFi criada");
}

void hide_wifi_scan_screen() {
    ESP_LOGI(TAG, "hide_wifi_scan_screen chamado");
    
    lvgl_lock();
    
    if (scan_screen != nullptr) {
        lv_obj_del(scan_screen);
        scan_screen = nullptr;
        title_label = nullptr;
        status_label = nullptr;
        list_obj = nullptr;
        back_button = nullptr;
    }
    
    network_count = 0;
    s_on_select = nullptr;
    
    lvgl_unlock();
}

bool is_wifi_scan_screen_visible() {
    return scan_screen != nullptr;
}

} // namespace screens
} // namespace ui

