#pragma once

#include "lvgl.h"
#include <functional>

namespace ui::screens {

/**
 * @brief Callback para voltar da tela "Sobre"
 */
extern std::function<void()> about_on_back_callback;

/**
 * @brief Mostra tela "Sobre" com informações do sistema
 */
void show_about_screen();

/**
 * @brief Limpa a tela "Sobre"
 */
void cleanup_about_screen();

} // namespace ui::screens
