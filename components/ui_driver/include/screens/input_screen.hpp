#pragma once

#include "lvgl.h"
#include <functional>
#include <cstring>

namespace ui {
namespace screens {

// Callback quando o usuário confirma o input
// Parâmetros: texto digitado, tamanho do texto
using InputCallback = std::function<void(const char* text, size_t len)>;

// Callback quando o usuário cancela
using CancelCallback = std::function<void()>;

/**
 * @brief Mostra uma tela de input dedicada com teclado
 * @param title Título da tela
 * @param placeholder Texto placeholder para o campo de input
 * @param initial_value Valor inicial (pode ser nullptr)
 * @param max_length Tamanho máximo do texto
 * @param password_mode Se true, oculta o texto digitado
 * @param on_confirm Callback chamado quando o usuário pressiona OK
 * @param on_cancel Callback chamado quando o usuário cancela (pode ser nullptr)
 * @param on_close Callback chamado quando a tela fecha (para voltar à tela anterior, pode ser nullptr)
 */
void show_input_screen(
    const char* title,
    const char* placeholder,
    const char* initial_value,
    size_t max_length,
    bool password_mode,
    InputCallback on_confirm,
    CancelCallback on_cancel = nullptr,
    std::function<void()> on_close = nullptr
);

/**
 * @brief Esconde a tela de input
 */
void hide_input_screen();

/**
 * @brief Verifica se a tela de input está visível
 */
bool is_input_screen_visible();

} // namespace screens
} // namespace ui

