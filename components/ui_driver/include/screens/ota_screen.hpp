#pragma once

#include "lvgl.h"

namespace ui::screens {

/**
 * @brief Mostra tela de atualização OTA com barra de progresso
 * @param otaUrl URL do servidor OTA (opcional, nullptr para usar padrão)
 */
void show_ota_screen(const char* otaUrl = nullptr);

/**
 * @brief Atualiza o progresso da atualização OTA
 * @param progress Progresso de 0 a 100
 */
void update_ota_progress(int progress);

/**
 * @brief Mostra mensagem de erro na tela OTA
 * @param errorMsg Mensagem de erro
 */
void show_ota_error(const char* errorMsg);

/**
 * @brief Limpa a tela OTA
 */
void cleanup_ota_screen();

} // namespace ui::screens
