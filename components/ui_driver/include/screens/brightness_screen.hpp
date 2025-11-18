#pragma once

#include "lvgl.h"

namespace ui {
namespace screens {

/**
 * @brief Mostra a tela de configuração de brilho.
 * Permite alternar entre modo automático e manual, e ajustar o brilho manual.
 */
void show_brightness_screen();

/**
 * @brief Callback para voltar (será definido externamente).
 */
extern void (*brightness_on_back_callback)();

} // namespace screens
} // namespace ui

