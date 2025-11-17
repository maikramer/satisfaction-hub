#include "ui_common.hpp"

// Declarar fonte Roboto customizada (definida em roboto.c)
extern const lv_font_t roboto;

namespace ui {
namespace common {

// Fontes padrão usando Roboto
const lv_font_t *TITLE_FONT = &::roboto;
const lv_font_t *TEXT_FONT = &::roboto;
const lv_font_t *CAPTION_FONT = &::roboto;

void apply_common_label_style(lv_obj_t* label) {
    lv_obj_set_style_text_color(label, COLOR_TEXT_BLACK(), 0);
    lv_obj_set_style_text_font(label, TEXT_FONT, 0);
    lv_obj_set_style_pad_top(label, 4, 0);
    lv_obj_set_style_pad_bottom(label, 4, 0);
}

void apply_common_button_style(lv_obj_t* button) {
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(button, lv_color_white(), 0);
    lv_obj_set_style_radius(button, 16, 0);
    lv_obj_set_style_pad_all(button, 4, 0);
}

void apply_screen_style(lv_obj_t* screen) {
    lv_obj_set_style_bg_color(screen, COLOR_BG_WHITE(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

} // namespace common
} // namespace ui

