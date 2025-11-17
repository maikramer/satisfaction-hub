#pragma once

#include "lvgl.h"

namespace ui {
namespace screens {

// Callback para voltar (definido externamente)
extern void (*on_back_callback)();

void create_wifi_config_screen();
void show_wifi_config_screen();
void destroy_wifi_config_screen();

} // namespace screens
} // namespace ui

