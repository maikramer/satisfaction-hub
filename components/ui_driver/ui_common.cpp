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
    lv_obj_set_style_radius(button, BUTTON_RADIUS, 0);
    lv_obj_set_style_pad_all(button, 4, 0);
}

void apply_screen_style(lv_obj_t* screen) {
    lv_obj_set_style_bg_color(screen, COLOR_BG_WHITE(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* create_screen_title(lv_obj_t* parent, const char* text) {
    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, text);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title_label, COLOR_TEXT_BLACK(), 0);
    lv_obj_set_style_text_font(title_label, TITLE_FONT, 0);
    lv_obj_set_style_pad_top(title_label, 4, 0);
    lv_obj_set_style_pad_bottom(title_label, 4, 0);
    
    // Posicionamento padrão: Topo e Centro, com margem superior
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
    
    return title_label;
}

lv_obj_t* create_button(lv_obj_t* parent, const char* text, int32_t width, lv_color_t color, int32_t height) {
    lv_obj_t* button = lv_button_create(parent);
    
    if (width > 0) {
        lv_obj_set_width(button, width);
    }
    lv_obj_set_height(button, height);
    
    lv_obj_set_style_bg_color(button, color, 0);
    apply_common_button_style(button);
    
    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    
    // Ajustar fonte se o botão for muito pequeno
    if (height < 30) {
         lv_obj_set_style_text_font(label, CAPTION_FONT, 0);
    } else {
         lv_obj_set_style_text_font(label, TEXT_FONT, 0);
    }

    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    
    return button;
}

// Constantes padronizadas para botões
namespace {
    // Botões padrão (Voltar, Conectar, OK principal, etc.)
    constexpr int32_t STANDARD_BUTTON_WIDTH = 120;  // Largura padrão unificada
    constexpr int32_t STANDARD_BUTTON_HEIGHT = BUTTON_HEIGHT; // Altura padrão (38px)
    constexpr int32_t STANDARD_BUTTON_RADIUS = 18;   // Raio padrão unificado (arredondado)
    constexpr int32_t STANDARD_BUTTON_BOTTOM_OFFSET = 10; // Distância padrão da base (10px)
    
    // Botões compactos (para casos especiais como input_screen)
    constexpr int32_t COMPACT_BUTTON_WIDTH = 80;   // Largura compacta
    constexpr int32_t COMPACT_BUTTON_HEIGHT = 32;  // Altura compacta (mantém proporção)
    constexpr int32_t COMPACT_BUTTON_RADIUS = 16;  // Raio proporcionalmente menor
}

// Função auxiliar para aplicar estilo padrão de botão (raio, etc.)
void apply_standard_button_style(lv_obj_t* button, int32_t radius) {
    lv_obj_set_style_radius(button, radius, 0);
}

// Botão de ação principal (Conectar, OK principal, Confirmar, etc.)
lv_obj_t* create_action_button(lv_obj_t* parent, const char* text, lv_color_t color, lv_event_cb_t event_cb) {
    lv_obj_t* button = create_button(parent, text, STANDARD_BUTTON_WIDTH, color, STANDARD_BUTTON_HEIGHT);
    apply_standard_button_style(button, STANDARD_BUTTON_RADIUS);
    
    if (event_cb) {
        lv_obj_add_event_cb(button, event_cb, LV_EVENT_CLICKED, nullptr);
    }
    
    return button;
}

// Botão de ação principal com offset X customizado (para dois botões lado a lado)
lv_obj_t* create_action_button(lv_obj_t* parent, const char* text, lv_color_t color, lv_event_cb_t event_cb, int32_t offset_x) {
    lv_obj_t* button = create_button(parent, text, STANDARD_BUTTON_WIDTH, color, STANDARD_BUTTON_HEIGHT);
    apply_standard_button_style(button, STANDARD_BUTTON_RADIUS);
    lv_obj_align(button, LV_ALIGN_BOTTOM_MID, offset_x, -STANDARD_BUTTON_BOTTOM_OFFSET);
    
    if (event_cb) {
        lv_obj_add_event_cb(button, event_cb, LV_EVENT_CLICKED, nullptr);
    }
    
    return button;
}

// Botão compacto (para casos especiais como input_screen, dialogs pequenos)
lv_obj_t* create_compact_button(lv_obj_t* parent, const char* text, lv_color_t color, lv_event_cb_t event_cb) {
    lv_obj_t* button = create_button(parent, text, COMPACT_BUTTON_WIDTH, color, COMPACT_BUTTON_HEIGHT);
    apply_standard_button_style(button, COMPACT_BUTTON_RADIUS);
    
    if (event_cb) {
        lv_obj_add_event_cb(button, event_cb, LV_EVENT_CLICKED, nullptr);
    }
    
    return button;
}

lv_obj_t* create_back_button(lv_obj_t* parent, lv_event_cb_t event_cb) {
    // Botão Voltar PADRONIZADO: Tamanho, cor, raio e posição consistentes em todas as telas
    lv_obj_t* back_button = create_button(parent, "Voltar", STANDARD_BUTTON_WIDTH, COLOR_BUTTON_GRAY(), STANDARD_BUTTON_HEIGHT);
    
    // Raio padronizado (mais arredondado que botões normais)
    apply_standard_button_style(back_button, STANDARD_BUTTON_RADIUS);
    
    // Alinhamento padrão: Bottom Middle com offset consistente
    lv_obj_align(back_button, LV_ALIGN_BOTTOM_MID, 0, -STANDARD_BUTTON_BOTTOM_OFFSET);
    
    if (event_cb) {
        lv_obj_add_event_cb(back_button, event_cb, LV_EVENT_CLICKED, nullptr);
    }
    
    return back_button;
}

// Versão com offset X customizado (para casos especiais como dois botões lado a lado)
lv_obj_t* create_back_button(lv_obj_t* parent, lv_event_cb_t event_cb, int32_t offset_x) {
    // Botão Voltar com tamanho e estilo padronizados, mas com offset X customizado
    lv_obj_t* back_button = create_button(parent, "Voltar", STANDARD_BUTTON_WIDTH, COLOR_BUTTON_GRAY(), STANDARD_BUTTON_HEIGHT);
    
    // Raio padronizado
    apply_standard_button_style(back_button, STANDARD_BUTTON_RADIUS);
    
    // Alinhamento com offset X customizado
    lv_obj_align(back_button, LV_ALIGN_BOTTOM_MID, offset_x, -STANDARD_BUTTON_BOTTOM_OFFSET);
    
    if (event_cb) {
        lv_obj_add_event_cb(back_button, event_cb, LV_EVENT_CLICKED, nullptr);
    }
    
    return back_button;
}

} // namespace common
} // namespace ui
