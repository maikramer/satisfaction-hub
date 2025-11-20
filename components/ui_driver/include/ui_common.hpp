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

// Constantes de Layout
constexpr int32_t SCREEN_WIDTH = 320;
constexpr int32_t SCREEN_HEIGHT = 240;
constexpr int32_t SCREEN_PADDING = 4; // Padding padrão reduzido para compactar
constexpr int32_t HEADER_HEIGHT = 40;
constexpr int32_t BUTTON_HEIGHT = 38;
constexpr int32_t BUTTON_RADIUS = 8;
constexpr int32_t INPUT_HEIGHT = 40;

// Cores padrão (funções inline para evitar problemas com constexpr)
inline lv_color_t COLOR_BG_WHITE() { return lv_color_hex(0xFFFFFF); }
inline lv_color_t COLOR_TEXT_BLACK() { return lv_color_hex(0x000000); }
inline lv_color_t COLOR_TEXT_GRAY() { return lv_color_hex(0x757575); }
inline lv_color_t COLOR_BUTTON_BLUE() { return lv_color_hex(0x2196F3); }
inline lv_color_t COLOR_BUTTON_GRAY() { return lv_color_hex(0x757575); }
inline lv_color_t COLOR_SETTINGS_BUTTON() { return lv_color_hex(0x607D8B); }
inline lv_color_t COLOR_BORDER() { return lv_color_hex(0xCCCCCC); }
inline lv_color_t COLOR_SUCCESS() { return lv_color_hex(0x4CAF50); }
inline lv_color_t COLOR_ERROR() { return lv_color_hex(0xF44336); }
inline lv_color_t COLOR_WARNING() { return lv_color_hex(0xFF9800); }

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

// Funções auxiliares de estilo e criação de widgets
void apply_common_label_style(lv_obj_t* label);
void apply_common_button_style(lv_obj_t* button);
void apply_screen_style(lv_obj_t* screen);

// Helpers para criação consistente de UI
lv_obj_t* create_screen_title(lv_obj_t* parent, const char* text);
lv_obj_t* create_button(lv_obj_t* parent, const char* text, int32_t width = 0, lv_color_t color = COLOR_BUTTON_GRAY(), int32_t height = BUTTON_HEIGHT);

// Botão de ação principal (Conectar, OK principal, Confirmar, etc.) - Padronizado
lv_obj_t* create_action_button(lv_obj_t* parent, const char* text, lv_color_t color, lv_event_cb_t event_cb);
// Versão com offset X customizado (para dois botões lado a lado)
lv_obj_t* create_action_button(lv_obj_t* parent, const char* text, lv_color_t color, lv_event_cb_t event_cb, int32_t offset_x);

// Botão compacto (para casos especiais como input_screen, dialogs pequenos) - Padronizado
lv_obj_t* create_compact_button(lv_obj_t* parent, const char* text, lv_color_t color, lv_event_cb_t event_cb);

// Versão padrão: alinhado ao centro inferior
lv_obj_t* create_back_button(lv_obj_t* parent, lv_event_cb_t event_cb);
// Versão com offset X customizado (para casos especiais como dois botões lado a lado)
lv_obj_t* create_back_button(lv_obj_t* parent, lv_event_cb_t event_cb, int32_t offset_x);

} // namespace common
} // namespace ui
