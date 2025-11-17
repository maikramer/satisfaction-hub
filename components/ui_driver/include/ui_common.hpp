#pragma once

#include "lvgl.h"

namespace ui {
namespace common {

// Declarar fonte Roboto customizada (suporta acentos portugueses)
LV_FONT_DECLARE(roboto);

// Fontes padrão usando Roboto
extern const lv_font_t *TITLE_FONT;
extern const lv_font_t *TEXT_FONT;
extern const lv_font_t *CAPTION_FONT;

// Cores padrão (funções inline para evitar problemas com constexpr)
inline lv_color_t COLOR_BG_WHITE() { return lv_color_hex(0xFFFFFF); }
inline lv_color_t COLOR_TEXT_BLACK() { return lv_color_hex(0x000000); }
inline lv_color_t COLOR_BUTTON_BLUE() { return lv_color_hex(0x2196F3); }
inline lv_color_t COLOR_BUTTON_GRAY() { return lv_color_hex(0x757575); }
inline lv_color_t COLOR_SETTINGS_BUTTON() { return lv_color_hex(0x607D8B); }

// Cores dos botões de avaliação (função para obter cor por índice)
inline lv_color_t RATING_COLOR(int index) {
    constexpr uint32_t colors[] = {
        0xF44336, // Vermelho - Muito Insatisfeito
        0xFF9800, // Laranja - Insatisfeito
        0xFFEB3B, // Amarelo - Neutro
        0x8BC34A, // Verde claro - Satisfeito
        0x4CAF50, // Verde - Muito Satisfeito
    };
    if (index >= 0 && index < 5) {
        return lv_color_hex(colors[index]);
    }
    return lv_color_hex(0x000000);
}

// Mensagens dos botões de avaliação
constexpr const char* RATING_MESSAGES[] = {
    "Muito Insatisfeito",
    "Insatisfeito",
    "Neutro",
    "Satisfeito",
    "Muito Satisfeito",
};

// Funções auxiliares
void apply_common_label_style(lv_obj_t* label);
void apply_common_button_style(lv_obj_t* button);
void apply_screen_style(lv_obj_t* screen);

} // namespace common
} // namespace ui

