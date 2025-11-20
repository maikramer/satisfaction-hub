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
    
    // OBTER TODAS AS INFORMAÇÕES DO SISTEMA ANTES DE ADQUIRIR O MUTEX LVGL
    // Isso evita bloqueios durante operações pesadas
    char info_buffer[128];
    char device_id_buffer[64];
    char mac_buffer[32];
    char heap_buffer[64];
    char block_buffer[64];
    char chip_buffer[64];
    char flash_buffer[64];
    char uptime_buffer[32];
    
    // Device ID (pode ser pesado - fazer antes do lock)
    auto& otaManager = OtaManager::instance();
    otaManager.init(); // Garantir inicialização
    snprintf(device_id_buffer, sizeof(device_id_buffer), "%s", otaManager.getDeviceId());
    
    // MAC Address
    uint8_t mac[6];
    esp_err_t mac_err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (mac_err == ESP_OK) {
        snprintf(mac_buffer, sizeof(mac_buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        snprintf(mac_buffer, sizeof(mac_buffer), "N/A");
    }
    
    // Free heap
    uint32_t free_heap = esp_get_free_heap_size();
    snprintf(heap_buffer, sizeof(heap_buffer), "%lu bytes (%.1f KB)", 
             (unsigned long)free_heap, free_heap / 1024.0f);
    
    // Largest free block
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    snprintf(block_buffer, sizeof(block_buffer), "%zu bytes (%.1f KB)",
             largest_block, largest_block / 1024.0f);
    
    // WiFi Status
    auto& wifi = WiFiManager::instance();
    const char* wifi_status = wifi.is_connected() ? "Conectado" : "Desconectado";
    
    // Chip Info
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    snprintf(chip_buffer, sizeof(chip_buffer), "ESP32 Rev %d (%d cores)",
             chip_info.revision, chip_info.cores);
    
    // Flash Size
    uint32_t flash_size = 4 * 1024 * 1024;
    #ifdef CONFIG_ESPTOOLPY_FLASHSIZE_4MB
        flash_size = 4 * 1024 * 1024;
    #elif defined(CONFIG_ESPTOOLPY_FLASHSIZE_2MB)
        flash_size = 2 * 1024 * 1024;
    #elif defined(CONFIG_ESPTOOLPY_FLASHSIZE_8MB)
        flash_size = 8 * 1024 * 1024;
    #endif
    snprintf(flash_buffer, sizeof(flash_buffer), "%lu bytes (%.1f MB)",
             (unsigned long)flash_size, flash_size / (1024.0f * 1024.0f));
    
    // Uptime
    uint32_t uptime_sec = esp_timer_get_time() / 1000000ULL;
    uint32_t hours = uptime_sec / 3600;
    uint32_t minutes = (uptime_sec % 3600) / 60;
    uint32_t seconds = uptime_sec % 60;
    snprintf(uptime_buffer, sizeof(uptime_buffer), "%02lu:%02lu:%02lu",
             (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds);
    
    // AGORA ADQUIRIR O MUTEX LVGL E CRIAR A UI RAPIDAMENTE
    // IMPORTANTE: Manter o mutex pelo menor tempo possível
    lvgl_lock();
    
    // Criar tela
    about_screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(about_screen);
    ::ui::common::apply_screen_style(about_screen);
    lv_obj_clear_flag(about_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Desabilitar atualizações automáticas durante criação para melhor performance
    lv_obj_set_style_anim_time(about_screen, 0, 0);
    
    // Botão "Voltar" FIXO na parte inferior da tela (fora do scroll)
    constexpr int32_t BACK_BUTTON_HEIGHT = 38;
    constexpr int32_t BACK_BUTTON_BOTTOM_OFFSET = 10;
    constexpr int32_t SCROLL_AREA_HEIGHT = 240 - BACK_BUTTON_HEIGHT - BACK_BUTTON_BOTTOM_OFFSET - 5; // Altura disponível para scroll
    
    // Container principal com scroll - altura reduzida para deixar espaço para o botão fixo
    about_scroll = lv_obj_create(about_screen);
    lv_obj_remove_style_all(about_scroll);
    // Largura fixa, altura reduzida para deixar espaço para botão fixo
    lv_obj_set_size(about_scroll, 320, SCROLL_AREA_HEIGHT);
    lv_obj_align(about_scroll, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Configurar scroll ANTES de qualquer layout - otimizações para performance
    lv_obj_set_scroll_dir(about_scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(about_scroll, LV_SCROLLBAR_MODE_ACTIVE);
    // NÃO usar flex layout - causa recálculos durante scroll
    lv_obj_set_style_pad_all(about_scroll, 0, 0); // Sem padding no container
    lv_obj_set_style_bg_opa(about_scroll, LV_OPA_TRANSP, 0); // Transparente para melhor performance
    
    // IMPORTANTE: LV_OBJ_FLAG_CLICKABLE deve estar ativo para capturar eventos de touch/drag para scroll
    lv_obj_add_flag(about_scroll, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_add_flag(about_scroll, LV_OBJ_FLAG_SCROLLABLE); // Garantir scrollable
    
    // Desabilitar layout automático COMPLETAMENTE para melhor performance
    lv_obj_set_layout(about_scroll, LV_LAYOUT_NONE);
    
    // Desabilitar recálculo automático durante scroll - CRÍTICO para evitar travamentos
    lv_obj_clear_flag(about_scroll, LV_OBJ_FLAG_SCROLL_ELASTIC); // Sem efeito elástico
    lv_obj_clear_flag(about_scroll, LV_OBJ_FLAG_SCROLL_MOMENTUM); // Sem momentum (pode causar travamentos)
    
    // Desabilitar animações durante scroll para melhor performance
    lv_obj_set_style_anim_time(about_scroll, 0, 0);
    
    // Padding e espaçamento melhorados
    constexpr int32_t PADDING_HOR = 16; // Padding horizontal
    constexpr int32_t PADDING_TOP = 8;  // Padding superior
    constexpr int32_t LABEL_VALUE_GAP = 18; // Espaço entre label e valor
    constexpr int32_t LINE_SPACING = 38; // Espaçamento entre linhas de informação (label + valor + gap)
    
    // Título - embelezado
    lv_obj_t* title_label = lv_label_create(about_scroll);
    lv_label_set_text(title_label, "Sobre");
    lv_obj_set_style_text_font(title_label, ::ui::common::TITLE_FONT, 0);
    lv_obj_set_style_text_color(title_label, ::ui::common::COLOR_TEXT_BLACK(), 0);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title_label, 320);
    lv_obj_set_pos(title_label, 0, PADDING_TOP);
    
    // Linha separadora abaixo do título
    lv_obj_t* separator = lv_obj_create(about_scroll);
    lv_obj_remove_style_all(separator);
    lv_obj_set_size(separator, 280, 1);
    lv_obj_set_style_bg_color(separator, ::ui::common::COLOR_BORDER(), 0);
    lv_obj_set_style_bg_opa(separator, LV_OPA_COVER, 0);
    lv_obj_set_pos(separator, (320 - 280) / 2, PADDING_TOP + 30);
    
    // Função helper EMBELEZADA - cria label e valor separados com cores diferentes
    int32_t y_pos = PADDING_TOP + 45; // Posição inicial abaixo do título e separador
    auto create_info_line = [&](const char* label, const char* value) {
        // Label (nome do campo) - cor cinza, fonte menor
        lv_obj_t* lbl = lv_label_create(about_scroll);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_font(lbl, ::ui::common::CAPTION_FONT, 0);
        lv_obj_set_style_text_color(lbl, ::ui::common::COLOR_TEXT_GRAY(), 0);
        lv_obj_set_width(lbl, 320 - (PADDING_HOR * 2));
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_pos(lbl, PADDING_HOR, y_pos);
        
        // Valor - cor preta, fonte normal, mais destacado
        lv_obj_t* val = lv_label_create(about_scroll);
        lv_label_set_text(val, value);
        lv_obj_set_style_text_font(val, ::ui::common::TEXT_FONT, 0);
        lv_obj_set_style_text_color(val, ::ui::common::COLOR_TEXT_BLACK(), 0);
        lv_obj_set_width(val, 320 - (PADDING_HOR * 2));
        lv_label_set_long_mode(val, LV_LABEL_LONG_WRAP);
        lv_obj_set_pos(val, PADDING_HOR, y_pos + LABEL_VALUE_GAP); // Espaço abaixo do label
        
        // Atualizar posição para próxima linha (label + gap + valor + espaçamento)
        y_pos += LINE_SPACING;
        
        return lbl;
    };
    
    // Criar todas as linhas rapidamente usando dados já preparados
    snprintf(info_buffer, sizeof(info_buffer), "%s", FIRMWARE_VERSION);
    create_info_line("Versão", info_buffer);
    create_info_line("Device ID", device_id_buffer);
    create_info_line("Endereço MAC", mac_buffer);
    create_info_line("Memória Livre", heap_buffer);
    create_info_line("Maior Bloco Livre", block_buffer);
    create_info_line("Status WiFi", wifi_status);
    create_info_line("Chip", chip_buffer);
    create_info_line("Memória Flash", flash_buffer);
    create_info_line("Tempo de Atividade", uptime_buffer);
    
    // Adicionar padding no fim do conteúdo para não cortar o último item
    y_pos += 10; // Espaço extra após último item
    
    // Botão de voltar FIXO na parte inferior da tela (fora do scroll)
    auto about_back_cb = [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            if (about_on_back_callback) {
                about_on_back_callback();
            }
        }
    };
    // Criar botão diretamente na tela (não no scroll)
    lv_obj_t* back_button = ::ui::common::create_back_button(about_screen, about_back_cb);
    
    // Desabilitar animações no botão para melhor performance
    lv_obj_set_style_anim_time(back_button, 0, 0);
    
    // Carregar tela ANTES de liberar mutex
    lv_screen_load(about_screen);
    
    // Forçar invalidação uma única vez após criar tudo
    lv_obj_invalidate(about_scroll);
    
    // Liberar mutex IMEDIATAMENTE após criar a UI
    lvgl_unlock();
    
    ESP_LOGI(TAG, "Tela Sobre criada");
}

} // namespace ui::screens
