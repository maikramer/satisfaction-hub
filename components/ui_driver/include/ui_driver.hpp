#pragma once

#include <cstdint>
#include "lvgl.h"

namespace ui {

/**
 * @brief Inicializa a aplicação de pesquisa de satisfação.
 *
 * @param display Display retornado pelo driver.
 */
void init(lv_display_t *display);

/**
 * @brief Atualiza a UI (chamado periodicamente no loop principal).
 */
void update();

/**
 * @brief Obtém a avaliação atual selecionada (0 = nenhuma, 1-5 = avaliação).
 */
int get_current_rating();

} // namespace ui
