#pragma once

#include "lvgl.h"

namespace ui {
namespace screens {

// Callback quando uma rede é selecionada
// Parâmetro: SSID da rede selecionada
using WiFiScanCallback = void (*)(const char* ssid);

/**
 * @brief Mostra a tela de scan WiFi
 * @param on_select Callback chamado quando uma rede é selecionada
 */
void show_wifi_scan_screen(WiFiScanCallback on_select);

/**
 * @brief Esconde a tela de scan WiFi
 */
void hide_wifi_scan_screen();

/**
 * @brief Verifica se a tela de scan está visível
 */
bool is_wifi_scan_screen_visible();

} // namespace screens
} // namespace ui

