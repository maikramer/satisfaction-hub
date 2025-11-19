#include "screens/input_screen.hpp"
#include "ui_common.hpp"
#include "ui_common_internal.hpp"
#include "esp_log.h"
#include "lvgl.h"
#include "widgets/textarea/lv_textarea.h"
#if LV_USE_KEYBOARD != 0
#include "widgets/keyboard/lv_keyboard.h"
#endif
// LV_SYMBOL_OK e LV_SYMBOL_CLOSE estão definidos em lvgl.h
#include <cstring>

namespace ui {
namespace screens {

static const char* TAG = "InputScreen";

static lv_obj_t* input_screen = nullptr;
static lv_obj_t* title_label = nullptr;
static lv_obj_t* input_textarea = nullptr;
static lv_obj_t* ok_button = nullptr;
static lv_obj_t* cancel_button = nullptr;
#if LV_USE_KEYBOARD != 0
static lv_obj_t* keyboard = nullptr;
#endif

static InputCallback s_on_confirm = nullptr;
static CancelCallback s_on_cancel = nullptr;
static std::function<void()> s_on_close = nullptr;  // Callback para quando a tela fecha

static void ok_button_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Botão OK pressionado");
        
        if (s_on_confirm && input_textarea) {
            #if LV_USE_TEXTAREA != 0
            const char* text = lv_textarea_get_text(input_textarea);
            if (text) {
                s_on_confirm(text, strlen(text));
            } else {
                s_on_confirm("", 0);
            }
            #else
            s_on_confirm("", 0);
            #endif
        }
        
        hide_input_screen();
    }
}

static void cancel_button_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Botão Cancelar pressionado");
        
        if (s_on_cancel) {
            s_on_cancel();
        }
        
        hide_input_screen();
    }
}

#if LV_USE_KEYBOARD != 0
static void keyboard_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        ESP_LOGI(TAG, "Teclado: Ready/Cancel pressionado");
        if (code == LV_EVENT_READY && s_on_confirm && input_textarea) {
            #if LV_USE_TEXTAREA != 0
            const char* text = lv_textarea_get_text(input_textarea);
            if (text) {
                s_on_confirm(text, strlen(text));
            } else {
                s_on_confirm("", 0);
            }
            #else
            s_on_confirm("", 0);
            #endif
        }
        hide_input_screen();
    }
}
#endif

void show_input_screen(
    const char* title,
    const char* placeholder,
    const char* initial_value,
    size_t max_length,
    bool password_mode,
    InputCallback on_confirm,
    CancelCallback on_cancel,
    std::function<void()> on_close) {
    
    ESP_LOGI(TAG, "show_input_screen: title='%s', placeholder='%s'", title ? title : "null", placeholder ? placeholder : "null");
    
    lvgl_lock();
    
    // Limpar tela anterior se existir
    if (input_screen != nullptr) {
        lv_obj_del(input_screen);
        input_screen = nullptr;
    }
    
    // Criar nova tela
    input_screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(input_screen);
    common::apply_screen_style(input_screen);
    
    // Título usando helper
    title_label = common::create_screen_title(input_screen, title ? title : "Digite");
    
    // Campo de input (abaixo do título)
    input_textarea = lv_textarea_create(input_screen);
    lv_obj_set_size(input_textarea, 300, common::INPUT_HEIGHT);
    lv_obj_align(input_textarea, LV_ALIGN_TOP_MID, 0, common::HEADER_HEIGHT + 10);
    
    #if LV_USE_TEXTAREA != 0
    if (placeholder) {
        lv_textarea_set_placeholder_text(input_textarea, placeholder);
    }
    lv_textarea_set_max_length(input_textarea, max_length);
    lv_textarea_set_one_line(input_textarea, true);
    if (password_mode) {
        lv_textarea_set_password_mode(input_textarea, true);
    }
    if (initial_value) {
        lv_textarea_set_text(input_textarea, initial_value);
    }
    #endif
    lv_obj_set_style_bg_color(input_textarea, lv_color_white(), 0);
    lv_obj_set_style_border_color(input_textarea, common::COLOR_BORDER(), 0);
    lv_obj_set_style_border_width(input_textarea, 1, 0);
    lv_obj_set_style_text_color(input_textarea, common::COLOR_TEXT_BLACK(), 0);
    lv_obj_add_flag(input_textarea, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(input_textarea, LV_OBJ_FLAG_SCROLLABLE);
    
    // Salvar callbacks
    s_on_confirm = on_confirm;
    s_on_cancel = on_cancel;
    s_on_close = on_close;
    
    // Criar teclado PRIMEIRO (ocupando a parte inferior da tela, começando de baixo)
    #if LV_USE_KEYBOARD != 0
    keyboard = lv_keyboard_create(input_screen);
    if (keyboard == nullptr) {
        ESP_LOGE(TAG, "Falha ao criar teclado!");
    } else {
        ESP_LOGI(TAG, "Teclado criado com sucesso");
        // Teclado ocupa a parte inferior
        lv_obj_set_size(keyboard, 320, 120);
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_textarea(keyboard, input_textarea);
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_ALL, nullptr);
    }
    #endif
    
    // Botões OK e Cancelar pequenos e horizontais (com texto) - acima do teclado
    // Usar altura compacta de 28px
    ok_button = common::create_button(input_screen, "OK", 80, common::COLOR_BUTTON_BLUE(), 28);
    // Posicionar acima do teclado (que está no bottom)
    lv_obj_align(ok_button, LV_ALIGN_BOTTOM_MID, -50, -125);
    
    lv_obj_add_event_cb(ok_button, ok_button_cb, LV_EVENT_CLICKED, nullptr);
    
    // Botão Cancelar
    cancel_button = common::create_button(input_screen, "Cancelar", 80, common::COLOR_BUTTON_GRAY(), 28);
    lv_obj_align_to(cancel_button, ok_button, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    
    lv_obj_add_event_cb(cancel_button, cancel_button_cb, LV_EVENT_CLICKED, nullptr);
    
    // Callback para mostrar/esconder teclado quando o textarea ganha/perde foco
    lv_obj_add_event_cb(input_textarea, [](lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        #if LV_USE_KEYBOARD != 0
        lv_obj_t* kb = keyboard;
        #else
        lv_obj_t* kb = nullptr;
        #endif
        
        if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
            ESP_LOGI(TAG, "Input textarea focused/clicked, showing keyboard");
            if (kb) {
                lv_keyboard_set_textarea(kb, input_textarea);
                lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(kb);
                lv_obj_invalidate(kb);
            }
        } else if (code == LV_EVENT_DEFOCUSED) {
            ESP_LOGI(TAG, "Input textarea defocused, hiding keyboard");
            if (kb) {
                lv_keyboard_set_textarea(kb, nullptr);
                lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }, LV_EVENT_ALL, nullptr);
    
    // Carregar a tela
    lv_screen_load(input_screen);
    lv_obj_invalidate(input_screen);
    
    lvgl_unlock();
    
    ESP_LOGI(TAG, "Tela de input criada e exibida");
}

void hide_input_screen() {
    ESP_LOGI(TAG, "hide_input_screen chamado");
    
    // Salvar callback de fechamento antes de limpar
    auto on_close = s_on_close;
    
    lvgl_lock();
    
    if (input_screen != nullptr) {
        lv_obj_del(input_screen);
        input_screen = nullptr;
        title_label = nullptr;
        input_textarea = nullptr;
        ok_button = nullptr;
        cancel_button = nullptr;
        #if LV_USE_KEYBOARD != 0
        keyboard = nullptr;
        #endif
    }
    
    s_on_confirm = nullptr;
    s_on_cancel = nullptr;
    s_on_close = nullptr;
    
    lvgl_unlock();
    
    ESP_LOGI(TAG, "Tela de input escondida");
    
    // Chamar callback de fechamento se existir (para voltar à tela anterior)
    if (on_close) {
        on_close();
    }
}

bool is_input_screen_visible() {
    return input_screen != nullptr;
}

} // namespace screens
} // namespace ui
