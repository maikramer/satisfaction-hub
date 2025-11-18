#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "Xpt2046Bitbang.hpp"

/**
 * @brief Driver C++ que encapsula toda a inicialização do display ILI9341 + LVGL + Touch.
 */
class DisplayDriver {
public:
    static DisplayDriver &instance();

    /**
     * @brief Inicializa o barramento SPI, painel, touch e LVGL (executado apenas uma vez).
     */
    esp_err_t init();

    /**
     * @brief Retorna o display LVGL associado.
     */
    lv_display_t *lvgl_display() const;

    /**
     * @brief Obtém o último ponto cru lido do touch.
     */
    TouchPoint last_touch_point() const { return last_touch_point_; }

    /**
     * @brief Atualiza e persiste a calibração do touch.
     */
    void update_touch_calibration(const TouchCalibration &calibration);

    /**
     * @brief Indica se existe uma calibração persistida.
     */
    bool has_custom_calibration() const { return touch_calibration_loaded_; }

    /**
     * @brief Retorna o handle do painel LCD (para flush callback).
     */
    esp_lcd_panel_handle_t get_panel_handle() const { return panel_handle_; }

    /**
     * @brief Define o brilho manual do backlight (0-100).
     * @param brightness Brilho de 0 a 100 (0 = desligado, 100 = máximo).
     * @return ESP_OK em caso de sucesso.
     */
    esp_err_t set_brightness(uint8_t brightness);

    /**
     * @brief Obtém o brilho atual do backlight (0-100).
     * @return Brilho atual de 0 a 100.
     */
    uint8_t get_brightness() const { return current_brightness_; }

    /**
     * @brief Habilita ou desabilita o brilho automático baseado no LDR.
     * @param enabled true para habilitar brilho automático, false para manual.
     * @return ESP_OK em caso de sucesso.
     */
    esp_err_t set_auto_brightness(bool enabled);

    /**
     * @brief Verifica se o brilho automático está habilitado.
     * @return true se automático está habilitado, false caso contrário.
     */
    bool is_auto_brightness_enabled() const { return auto_brightness_enabled_; }

    /**
     * @brief Obtém a leitura atual do sensor LDR (0-4095).
     * @return Valor do ADC do LDR (0 = escuro, 4095 = claro).
     */
    uint16_t get_ldr_value() const { return last_ldr_value_; }

private:
    DisplayDriver() = default;

    esp_err_t init_backlight();
    esp_err_t init_ldr();
    esp_err_t init_spi_bus();
    esp_err_t init_panel_io();
    esp_err_t init_panel_device();
    esp_err_t init_touch();
    esp_err_t init_lvgl();
    esp_err_t create_lvgl_display();
    esp_err_t add_touch_to_lvgl();
    static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data);
    static void brightness_update_task(void *pvParameters);
    void update_auto_brightness();
    void load_brightness_settings();
    void save_brightness_settings();

    bool initialized_ = false;
    bool spi_initialized_ = false;
    bool lvgl_port_initialized_ = false;

    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_handle_ = nullptr;
    Xpt2046Bitbang *touch_controller_ = nullptr;
    lv_display_t *lv_display_ = nullptr;
    lv_indev_t *lv_touch_indev_ = nullptr;
    TouchCalibration current_touch_calibration_ = {};
    TouchPoint last_touch_point_ = {};
    bool touch_calibration_loaded_ = false;

    // Controle de brilho
    bool auto_brightness_enabled_ = true;  // Padrão: automático habilitado
    uint8_t current_brightness_ = 50;      // Brilho atual (0-100)
    uint8_t manual_brightness_ = 50;        // Brilho manual salvo (0-100)
    uint16_t last_ldr_value_ = 0;          // Última leitura do LDR
    TaskHandle_t brightness_task_handle_ = nullptr;
    adc_oneshot_unit_handle_t adc1_handle_ = nullptr;  // Handle do ADC oneshot
    static constexpr uint8_t MIN_BRIGHTNESS = 5;   // Brilho mínimo (evita tela completamente apagada)
    static constexpr uint8_t MAX_BRIGHTNESS = 100; // Brilho máximo
    static constexpr uint16_t LDR_MIN = 0;         // Valor mínimo do LDR (escuro)
    static constexpr uint16_t LDR_MAX = 4095;      // Valor máximo do LDR (claro)
    static constexpr const char* BRIGHTNESS_NVS_NAMESPACE = "brightness";
    static constexpr const char* BRIGHTNESS_NVS_KEY_AUTO = "auto";
    static constexpr const char* BRIGHTNESS_NVS_KEY_MANUAL = "manual";

    void load_touch_calibration_from_nvs();
    void save_touch_calibration_to_nvs(const TouchCalibration &calibration);
    void apply_touch_calibration(const TouchCalibration &calibration);
};

