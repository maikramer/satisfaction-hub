#pragma once

#include "lvgl.h"
#include <functional>
#include <string>

namespace ui {
namespace screens {

// Callback de sucesso (quando a senha está correta)
using PasswordSuccessCallback = std::function<void()>;
// Callback de cancelamento (quando clica em voltar)
using PasswordCancelCallback = std::function<void()>;

/**
 * @brief Mostra a tela de senha com teclado numérico agrupado (1-2, 3-4, etc.)
 * 
 * @param on_success Chamado quando a senha é digitada corretamente
 * @param on_cancel Chamado quando o usuário cancela/volta
 */
void show_password_screen(PasswordSuccessCallback on_success, PasswordCancelCallback on_cancel);

/**
 * @brief Esconde a tela de senha
 */
void hide_password_screen();

/**
 * @brief Verifica se a tela de senha está visível
 */
bool is_password_screen_visible();

/**
 * @brief Define uma nova senha e salva no NVS
 * @param new_password Nova senha a ser salva
 * @return true se a senha foi salva com sucesso, false caso contrário
 */
bool set_password(const std::string& new_password);

/**
 * @brief Obtém a senha atual (do cache, não do NVS)
 * @return Senha atual
 */
std::string get_password();

} // namespace screens
} // namespace ui



