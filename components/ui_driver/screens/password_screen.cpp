#include "screens/password_screen.hpp"
#include "ui_common.hpp"
#include "ui_common_internal.hpp"
#include "esp_log.h"
#include "lvgl.h"
#include <string>
#include <vector>

namespace ui {
namespace screens {

static const char* TAG = "PasswordScreen";

// Configuração da senha
// TODO: Mover para configuração persistente no futuro
static std::string s_current_password = "0523";

static lv_obj_t* password_screen = nullptr;
static lv_obj_t* display_label = nullptr;  // Mostra asteriscos
static lv_obj_t* error_dialog = nullptr;  // Diálogo de erro

static PasswordSuccessCallback s_on_success = nullptr;
static PasswordCancelCallback s_on_cancel = nullptr;

// Variável estática para armazenar o overlay do diálogo
static lv_obj_t* error_overlay = nullptr;

// Callback para fechar o diálogo
static void close_error_dialog_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Resetar timeout ao interagir
        reset_password_timeout();
        
        if (error_overlay != nullptr) {
            lv_obj_del(error_overlay);
            error_overlay = nullptr;
            error_dialog = nullptr;
        }
    }
}

// Callback do timer para auto-fechar
static void error_dialog_timer_cb(lv_timer_t* timer) {
    if (error_overlay != nullptr) {
        lv_obj_del(error_overlay);
        error_overlay = nullptr;
        error_dialog = nullptr;
    }
    lv_timer_del(timer);
}

// Função para mostrar diálogo de erro
static void show_error_dialog(const char* message) {
    // Remover diálogo anterior se existir
    if (error_overlay != nullptr) {
        lv_obj_del(error_overlay);
        error_overlay = nullptr;
        error_dialog = nullptr;
    }
    
    // Criar overlay escuro semi-transparente
    error_overlay = lv_obj_create(password_screen);
    lv_obj_set_size(error_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(error_overlay, 0, 0);
    lv_obj_set_style_bg_color(error_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(error_overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(error_overlay, 0, 0);
    lv_obj_set_style_radius(error_overlay, 0, 0);
    lv_obj_clear_flag(error_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(error_overlay);
    
    // Criar container do diálogo
    error_dialog = lv_obj_create(error_overlay);
    lv_obj_set_size(error_dialog, 260, 100);
    lv_obj_align(error_dialog, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(error_dialog, lv_color_white(), 0);
    lv_obj_set_style_border_color(error_dialog, common::COLOR_ERROR(), 0);
    lv_obj_set_style_border_width(error_dialog, 2, 0);
    lv_obj_set_style_radius(error_dialog, 8, 0);
    lv_obj_set_style_pad_all(error_dialog, 10, 0);
    
    // Mensagem de erro
    lv_obj_t* msg_label = lv_label_create(error_dialog);
    lv_label_set_text(msg_label, message);
    lv_obj_set_style_text_color(msg_label, common::COLOR_ERROR(), 0);
    lv_obj_set_style_text_font(msg_label, common::TEXT_FONT, 0);
    lv_obj_set_style_text_align(msg_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(msg_label, LV_PCT(100));
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(msg_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Botão OK
    lv_obj_t* ok_btn = common::create_button(error_dialog, "OK", 80, common::COLOR_BUTTON_GRAY(), 32);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(ok_btn, close_error_dialog_cb, LV_EVENT_CLICKED, nullptr);
    
    // Auto-fechar após 2 segundos usando timer
    lv_timer_t* timer = lv_timer_create(error_dialog_timer_cb, 2000, nullptr);
    lv_timer_set_repeat_count(timer, 1);
}

// Buffer para a senha digitada (armazena os caracteres reais que o usuário intencionou?)
// Não, como os botões são ambíguos, precisamos validar contra a senha esperada.
// Mas a validação de "1-2" contra "1" ou "2" é feita na hora?
// Abordagem:
// A senha real é "0523".
// Os botões são grupos: G1="12", G2="34", G3="56", G4="78", G5="90".
// Quando o usuário aperta G1, pode ser 1 ou 2.
// Se a senha na posição K é '1', e o usuário aperta G1, é válido.
// Se a senha na posição K é '2', e o usuário aperta G1, também é válido.
// Vamos armazenar a entrada como uma string de caracteres "representativos" de cada botão?
// Não, vamos armazenar apenas o comprimento da entrada e validar no final ou passo a passo.
// Para simplicidade: Vamos validar passo a passo.

static std::string s_input_buffer; // Armazena o que foi digitado (ex: '1' para o botão 1-2, '3' para 3-4...)
// Mas espere, para validar "0523", precisamos saber qual botão corresponde a cada dígito.

// Mapeamento Dígito -> Índice do Botão
// Botões: 
// 0: "1-2" (Dígitos '1', '2')
// 1: "3-4" (Dígitos '3', '4')
// 2: "5-6" (Dígitos '5', '6')
// 3: "7-8" (Dígitos '7', '8')
// 4: "9-0" (Dígitos '9', '0')

static int get_button_index_for_digit(char digit) {
    switch(digit) {
        case '1': return 0;
        case '2': return 0;
        case '3': return 1;
        case '4': return 1;
        case '5': return 2;
        case '6': return 2;
        case '7': return 3;
        case '8': return 3;
        case '9': return 4;
        case '0': return 4;
        default: return -1;
    }
}

static void update_display() {
    if (!display_label) return;
    
    std::string masked;
    for (size_t i = 0; i < s_input_buffer.length(); i++) {
        masked += "*";
    }
    
    if (masked.empty()) {
        lv_label_set_text(display_label, "Digite a senha");
        lv_obj_set_style_text_color(display_label, common::COLOR_TEXT_GRAY(), 0);
    } else {
        lv_label_set_text(display_label, masked.c_str());
        lv_obj_set_style_text_color(display_label, common::COLOR_TEXT_BLACK(), 0);
    }
}

static void check_password() {
    if (s_input_buffer.length() != s_current_password.length()) {
        // Limpar buffer após erro
        s_input_buffer.clear();
        update_display();
        show_error_dialog("Senha Incorreta");
        return;
    }

    bool correct = true;
    for (size_t i = 0; i < s_current_password.length(); i++) {
        char expected_digit = s_current_password[i];
        int expected_btn_index = get_button_index_for_digit(expected_digit);
        
        // s_input_buffer armazena o índice do botão como char ('0', '1', '2'...)
        int input_btn_index = s_input_buffer[i] - '0';
        
        if (input_btn_index != expected_btn_index) {
            correct = false;
            break;
        }
    }
    
    if (correct) {
        ESP_LOGI(TAG, "Senha correta!");
        if (s_on_success) {
            s_on_success();
        }
        hide_password_screen();
    } else {
        ESP_LOGW(TAG, "Senha incorreta!");
        s_input_buffer.clear();
        update_display();
        show_error_dialog("Senha Incorreta");
    }
}

static void btn_click_cb(lv_event_t* e) {
    int btn_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    // Resetar timeout ao interagir
    reset_password_timeout();
    
    // Limite de tamanho para segurança e UI
    if (s_input_buffer.length() < 8) {
        s_input_buffer += std::to_string(btn_index);
        update_display();
        
        // Auto-check se atingiu o tamanho da senha
        if (s_input_buffer.length() == s_current_password.length()) {
            // Pequeno delay para UX (opcional, mas aqui vamos direto)
            check_password();
        }
    }
}

static void del_click_cb(lv_event_t* e) {
    // Resetar timeout ao interagir
    reset_password_timeout();
    
    if (!s_input_buffer.empty()) {
        s_input_buffer.pop_back();
        update_display();
    }
}

static void back_click_cb(lv_event_t* e) {
    // Resetar timeout ao interagir (mas vamos fechar mesmo assim)
    reset_password_timeout();
    
    if (s_on_cancel) {
        s_on_cancel();
    }
    hide_password_screen();
}

void show_password_screen(PasswordSuccessCallback on_success, PasswordCancelCallback on_cancel) {
    ESP_LOGI(TAG, "show_password_screen chamado");
    
    s_on_success = on_success;
    s_on_cancel = on_cancel;
    s_input_buffer.clear();
    
    // Iniciar timeout automático (10 segundos)
    reset_password_timeout();
    
    lvgl_lock();
    
    if (password_screen != nullptr) {
        lv_obj_del(password_screen);
        password_screen = nullptr;
    }
    
    password_screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(password_screen);
    common::apply_screen_style(password_screen);
    
    // Display da senha (sem título, começa mais acima)
    lv_obj_t* display_container = lv_obj_create(password_screen);
    lv_obj_set_size(display_container, 240, 45);
    lv_obj_align(display_container, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_color(display_container, lv_color_white(), 0);
    lv_obj_set_style_border_color(display_container, common::COLOR_BORDER(), 0);
    lv_obj_set_style_border_width(display_container, 1, 0);
    lv_obj_set_style_radius(display_container, 4, 0);
    lv_obj_clear_flag(display_container, LV_OBJ_FLAG_SCROLLABLE);
    
    display_label = lv_label_create(display_container);
    lv_obj_center(display_label);
    lv_obj_set_style_text_font(display_label, common::TITLE_FONT, 0); // Fonte maior para asteriscos
    update_display();
    
    // Layout do teclado: posicionamento manual para controle preciso
    // 3 colunas x 2 linhas + botão Del na segunda linha
    // Largura total disponível: 300px
    // Espaçamento entre botões: 8px
    // Largura de cada botão: (300 - 2*8) / 3 = ~94px, mas vamos usar 90px para mais espaço
    // Altura dos botões: 42px (mais compacto)
    
    const int32_t BTN_WIDTH = 90;
    const int32_t BTN_HEIGHT = 42;
    const int32_t BTN_SPACING = 8;
    const int32_t KEYPAD_START_Y = 80; // Começa abaixo do display (20 + 45 + 15 de espaçamento)
    const int32_t KEYPAD_START_X = (common::SCREEN_WIDTH - (3 * BTN_WIDTH + 2 * BTN_SPACING)) / 2; // Centralizar
    
    // Botões Numéricos (1-2, 3-4, 5-6, 7-8, 9-0)
    const char* btn_labels[] = {"1-2", "3-4", "5-6", "7-8", "9-0"};
    
    // Primeira linha: 1-2, 3-4, 5-6
    for (int i = 0; i < 3; i++) {
        lv_obj_t* btn = lv_button_create(password_screen);
        lv_obj_set_size(btn, BTN_WIDTH, BTN_HEIGHT);
        lv_obj_set_pos(btn, KEYPAD_START_X + i * (BTN_WIDTH + BTN_SPACING), KEYPAD_START_Y);
        
        // Estilo compacto com menos padding
        lv_obj_set_style_bg_color(btn, common::COLOR_BUTTON_BLUE(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn, common::BUTTON_RADIUS, 0);
        lv_obj_set_style_pad_all(btn, 2, 0); // Padding mínimo
        
        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, btn_labels[i]);
        lv_obj_center(label);
        lv_obj_set_style_text_font(label, common::TEXT_FONT, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        
        lv_obj_add_event_cb(btn, btn_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
    
    // Segunda linha: 7-8, 9-0, Del
    int32_t second_row_y = KEYPAD_START_Y + BTN_HEIGHT + BTN_SPACING;
    
    // Botão 7-8
    lv_obj_t* btn_7_8 = lv_button_create(password_screen);
    lv_obj_set_size(btn_7_8, BTN_WIDTH, BTN_HEIGHT);
    lv_obj_set_pos(btn_7_8, KEYPAD_START_X, second_row_y);
    lv_obj_set_style_bg_color(btn_7_8, common::COLOR_BUTTON_BLUE(), 0);
    lv_obj_set_style_bg_opa(btn_7_8, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_7_8, common::BUTTON_RADIUS, 0);
    lv_obj_set_style_pad_all(btn_7_8, 2, 0);
    lv_obj_t* label_7_8 = lv_label_create(btn_7_8);
    lv_label_set_text(label_7_8, btn_labels[3]);
    lv_obj_center(label_7_8);
    lv_obj_set_style_text_font(label_7_8, common::TEXT_FONT, 0);
    lv_obj_set_style_text_color(label_7_8, lv_color_white(), 0);
    lv_obj_add_event_cb(btn_7_8, btn_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)3);
    
    // Botão 9-0
    lv_obj_t* btn_9_0 = lv_button_create(password_screen);
    lv_obj_set_size(btn_9_0, BTN_WIDTH, BTN_HEIGHT);
    lv_obj_set_pos(btn_9_0, KEYPAD_START_X + (BTN_WIDTH + BTN_SPACING), second_row_y);
    lv_obj_set_style_bg_color(btn_9_0, common::COLOR_BUTTON_BLUE(), 0);
    lv_obj_set_style_bg_opa(btn_9_0, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_9_0, common::BUTTON_RADIUS, 0);
    lv_obj_set_style_pad_all(btn_9_0, 2, 0);
    lv_obj_t* label_9_0 = lv_label_create(btn_9_0);
    lv_label_set_text(label_9_0, btn_labels[4]);
    lv_obj_center(label_9_0);
    lv_obj_set_style_text_font(label_9_0, common::TEXT_FONT, 0);
    lv_obj_set_style_text_color(label_9_0, lv_color_white(), 0);
    lv_obj_add_event_cb(btn_9_0, btn_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)4);
    
    // Botão Del
    lv_obj_t* del_btn = lv_button_create(password_screen);
    lv_obj_set_size(del_btn, BTN_WIDTH, BTN_HEIGHT);
    lv_obj_set_pos(del_btn, KEYPAD_START_X + 2 * (BTN_WIDTH + BTN_SPACING), second_row_y);
    lv_obj_set_style_bg_color(del_btn, common::COLOR_BUTTON_GRAY(), 0);
    lv_obj_set_style_bg_opa(del_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(del_btn, common::BUTTON_RADIUS, 0);
    lv_obj_set_style_pad_all(del_btn, 2, 0);
    lv_obj_t* del_label = lv_label_create(del_btn);
    lv_label_set_text(del_label, "Del");
    lv_obj_center(del_label);
    lv_obj_set_style_text_font(del_label, common::TEXT_FONT, 0);
    lv_obj_set_style_text_color(del_label, lv_color_white(), 0);
    lv_obj_add_event_cb(del_btn, del_click_cb, LV_EVENT_CLICKED, nullptr);
    
    // Botão Voltar (rodapé)
    common::create_back_button(password_screen, back_click_cb);
    
    lv_screen_load(password_screen);
    
    lvgl_unlock();
}

void hide_password_screen() {
    lvgl_lock();
    // Limpar diálogo de erro se existir
    if (error_overlay != nullptr) {
        lv_obj_del(error_overlay);
        error_overlay = nullptr;
        error_dialog = nullptr;
    }
    if (password_screen) {
        lv_obj_del(password_screen);
        password_screen = nullptr;
        display_label = nullptr;
    }
    s_on_success = nullptr;
    s_on_cancel = nullptr;
    lvgl_unlock();
}

bool is_password_screen_visible() {
    return password_screen != nullptr;
}

} // namespace screens
} // namespace ui

