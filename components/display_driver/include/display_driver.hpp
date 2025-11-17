#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
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

private:
    DisplayDriver() = default;

    esp_err_t init_backlight();
    esp_err_t init_spi_bus();
    esp_err_t init_panel_io();
    esp_err_t init_panel_device();
    esp_err_t init_touch();
    esp_err_t init_lvgl();
    esp_err_t create_lvgl_display();
    esp_err_t add_touch_to_lvgl();
    static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data);

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

    void load_touch_calibration_from_nvs();
    void save_touch_calibration_to_nvs(const TouchCalibration &calibration);
    void apply_touch_calibration(const TouchCalibration &calibration);
};

