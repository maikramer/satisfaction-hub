#include "screens/about_screen.hpp"
#include "ui_common.hpp"
#include "ui_common_internal.hpp" // Para lvgl_lock() e lvgl_unlock()
#include "OtaManager.h"
#include "WiFiManager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_chip_info.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include <cstring>
#include <cstdio>

// Declarar fonte Roboto customizada
LV_FONT_DECLARE(roboto);
// Declarar fonte Montserrat para ícones
LV_FONT_DECLARE(lv_font_montserrat_20);

namespace {
constexpr char TAG[] = "ABOUT_SCREEN";

lv_obj_t* about_screen = nullptr;
lv_obj_t* about_scroll = nullptr;

// Versão do firmware (pode ser definida via menuconfig ou constante)
constexpr const char* FIRMWARE_VERSION = "1.0.0";
} // namespace

namespace ui::screens {
std::function<void()> about_on_back_callback = nullptr;
} // namespace ui::screens

namespace ui::screens {

void cleanup_about_screen() {
    if (about_screen != nullptr) {
        lvgl_lock();
        
        lv_obj_del(about_screen);
        about_screen = nullptr;
        about_scroll = nullptr;
        
        lvgl_unlock();
    }
}

void show_about_screen() {
    ESP_LOGI(TAG, "Mostrando tela Sobre");
    
    // Limpar tela anterior se existir
    cleanup_about_screen();
    
    // Usar lvgl_lock() que verifica se já estamos na task do LVGL
    lvgl_lock();
    
    // Criar tela
    about_screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(about_screen);
    ::ui::common::apply_screen_style(about_screen);
    
    // Container principal com scroll
    about_scroll = lv_obj_create(about_screen);
    lv_obj_remove_style_all(about_scroll);
    lv_obj_set_size(about_scroll, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(about_scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(about_scroll, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(about_scroll, 10, 0);
    lv_obj_set_style_pad_row(about_scroll, 8, 0);
    
    // Título
    lv_obj_t* title_label = lv_label_create(about_scroll);
    lv_label_set_text(title_label, "Sobre");
    lv_obj_set_style_text_font(title_label, ::ui::common::TITLE_FONT, 0);
    lv_obj_set_style_text_color(title_label, ::ui::common::COLOR_TEXT_BLACK(), 0);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title_label, LV_PCT(100));
    lv_obj_set_style_pad_bottom(title_label, 10, 0);
    
    // Função helper para criar linha de informação
    auto create_info_line = [&](const char* label, const char* value) {
        lv_obj_t* cont = lv_obj_create(about_scroll);
        lv_obj_remove_style_all(cont);
        lv_obj_set_width(cont, LV_PCT(100));
        lv_obj_set_height(cont, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(cont, 0, 0);
        lv_obj_set_style_pad_row(cont, 2, 0);
        
        // Label (texto pequeno)
        lv_obj_t* lbl = lv_label_create(cont);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_font(lbl, ::ui::common::CAPTION_FONT, 0);
        lv_obj_set_style_text_color(lbl, ::ui::common::COLOR_TEXT_GRAY(), 0);
        lv_obj_set_width(lbl, LV_PCT(100));
        
        // Valor (texto compacto)
        lv_obj_t* val = lv_label_create(cont);
        lv_label_set_text(val, value);
        lv_obj_set_style_text_font(val, ::ui::common::TEXT_FONT, 0);
        lv_obj_set_style_text_color(val, ::ui::common::COLOR_TEXT_BLACK(), 0);
        lv_obj_set_width(val, LV_PCT(100));
        
        return cont;
    };
    
    // Obter informações do sistema
    char info_buffer[128];
    
    // Versão do firmware
    snprintf(info_buffer, sizeof(info_buffer), "%s", FIRMWARE_VERSION);
    create_info_line("Versão", info_buffer);
    
    // Device ID
    auto& otaManager = OtaManager::instance();
    otaManager.init(); // Garantir inicialização
    snprintf(info_buffer, sizeof(info_buffer), "%s", otaManager.getDeviceId());
    create_info_line("Device ID", info_buffer);
    
    // MAC Address
    uint8_t mac[6];
    esp_err_t mac_err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (mac_err == ESP_OK) {
        snprintf(info_buffer, sizeof(info_buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        create_info_line("MAC Address", info_buffer);
    }
    
    // Free heap
    uint32_t free_heap = esp_get_free_heap_size();
    snprintf(info_buffer, sizeof(info_buffer), "%lu bytes (%.1f KB)", 
             (unsigned long)free_heap, free_heap / 1024.0f);
    create_info_line("Memória Livre", info_buffer);
    
    // Largest free block
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    snprintf(info_buffer, sizeof(info_buffer), "%zu bytes (%.1f KB)",
             largest_block, largest_block / 1024.0f);
    create_info_line("Maior Bloco Livre", info_buffer);
    
    // WiFi Status
    auto& wifi = WiFiManager::instance();
    const char* wifi_status = wifi.is_connected() ? "Conectado" : "Desconectado";
    create_info_line("WiFi", wifi_status);
    
    // Chip Info
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    snprintf(info_buffer, sizeof(info_buffer), "ESP32 Rev %d (%d cores)",
             chip_info.revision, chip_info.cores);
    create_info_line("Chip", info_buffer);
    
    // Flash Size (do sdkconfig)
    uint32_t flash_size = 4 * 1024 * 1024; // padrão 4MB
    #ifdef CONFIG_ESPTOOLPY_FLASHSIZE_4MB
        flash_size = 4 * 1024 * 1024;
    #elif defined(CONFIG_ESPTOOLPY_FLASHSIZE_2MB)
        flash_size = 2 * 1024 * 1024;
    #elif defined(CONFIG_ESPTOOLPY_FLASHSIZE_8MB)
        flash_size = 8 * 1024 * 1024;
    #endif
    snprintf(info_buffer, sizeof(info_buffer), "%lu bytes (%.1f MB)",
             (unsigned long)flash_size, flash_size / (1024.0f * 1024.0f));
    create_info_line("Flash", info_buffer);
    
    // Uptime (aproximado)
    uint32_t uptime_sec = esp_timer_get_time() / 1000000ULL;
    uint32_t hours = uptime_sec / 3600;
    uint32_t minutes = (uptime_sec % 3600) / 60;
    uint32_t seconds = uptime_sec % 60;
    snprintf(info_buffer, sizeof(info_buffer), "%02lu:%02lu:%02lu",
             (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds);
    create_info_line("Uptime", info_buffer);
    
    // Botão de voltar
    lv_obj_t* back_button = ::ui::common::create_button(about_scroll, "Voltar", 150, ::ui::common::COLOR_ERROR());
    lv_obj_set_style_radius(back_button, 20, 0);
    lv_obj_set_style_margin_top(back_button, 15, 0);
    
    // Callback do botão voltar
    lv_obj_add_event_cb(back_button, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            if (about_on_back_callback) {
                about_on_back_callback();
            }
        }
    }, LV_EVENT_CLICKED, nullptr);
    
    // Carregar tela
    lv_screen_load(about_screen);
    
    // Liberar mutex
    lvgl_unlock();
    
    ESP_LOGI(TAG, "Tela Sobre criada");
}

} // namespace ui::screens
