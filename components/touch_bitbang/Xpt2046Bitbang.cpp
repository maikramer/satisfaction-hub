#include "Xpt2046Bitbang.hpp"

#include "esp_log.h"

static const char *TAG = "XPT2046_BB";

Xpt2046Bitbang::Xpt2046Bitbang(gpio_num_t mosi,
                               gpio_num_t miso,
                               gpio_num_t clk,
                               gpio_num_t cs,
                               uint16_t screenWidth,
                               uint16_t screenHeight)
    : mosi_(mosi),
      miso_(miso),
      clk_(clk),
      cs_(cs),
      width_(screenWidth),
      height_(screenHeight),
      cal_{0, 4095, 0, 4095},
      invert_x_(false),
      invert_y_(false) {}

void Xpt2046Bitbang::begin() {
    gpio_config_t cfg = {};
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pin_bit_mask = BIT64(mosi_) | BIT64(clk_) | BIT64(cs_);
    ESP_ERROR_CHECK(gpio_config(&cfg));

    cfg.mode = GPIO_MODE_INPUT;
    cfg.pin_bit_mask = BIT64(miso_);
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&cfg));

    esp_rom_gpio_pad_select_gpio(mosi_);
    esp_rom_gpio_pad_select_gpio(miso_);
    esp_rom_gpio_pad_select_gpio(clk_);
    esp_rom_gpio_pad_select_gpio(cs_);

    gpio_set_level(cs_, 1);
    gpio_set_level(clk_, 0);

    ESP_LOGI(TAG, "XPT2046 bit-bang inicializado (MOSI=%d MISO=%d CLK=%d CS=%d)",
             mosi_, miso_, clk_, cs_);
}

void Xpt2046Bitbang::setCalibration(uint16_t xMin, uint16_t xMax,
                                    uint16_t yMin, uint16_t yMax) {
    cal_ = TouchCalibration{xMin, xMax, yMin, yMax};
}

void Xpt2046Bitbang::setInversion(bool invertX, bool invertY) {
    invert_x_ = invertX;
    invert_y_ = invertY;
}

void Xpt2046Bitbang::writeSpi(uint8_t command) {
    for (int i = 7; i >= 0; --i) {
        gpio_set_level(mosi_, (command >> i) & 0x01);
        gpio_set_level(clk_, 0);
        esp_rom_delay_us(DELAY_US);
        gpio_set_level(clk_, 1);
        esp_rom_delay_us(DELAY_US);
    }
    gpio_set_level(mosi_, 0);
    gpio_set_level(clk_, 0);
}

uint16_t Xpt2046Bitbang::readSpi(uint8_t command) {
    writeSpi(command);

    uint16_t result = 0;
    for (int i = 15; i >= 0; --i) {
        gpio_set_level(clk_, 1);
        esp_rom_delay_us(DELAY_US);
        gpio_set_level(clk_, 0);
        esp_rom_delay_us(DELAY_US);
        result |= static_cast<uint16_t>(gpio_get_level(miso_)) << i;
    }
    return result >> 4;
}

TouchPoint Xpt2046Bitbang::getTouch() {
    gpio_set_level(cs_, 0);

    uint16_t z1 = readSpi(CMD_READ_Z1);
    uint16_t z2 = readSpi(CMD_READ_Z2);
    uint16_t pressure = (z1 + 4095) - z2;

    if (pressure < 100) {
        ESP_LOGV(TAG, "touch release: z1=%u z2=%u pressure=%u", z1, z2, pressure);
        gpio_set_level(cs_, 1);
        return TouchPoint{0, 0, 0, 0, 0};
    }

    uint16_t rawX_original = readSpi(CMD_READ_X);
    uint16_t rawY_original = readSpi(CMD_READ_Y & ~static_cast<uint8_t>(1));
    gpio_set_level(cs_, 1);

    // Salvar valores RAW originais (para calibração)
    uint16_t rawX = rawX_original;
    uint16_t rawY = rawY_original;

    // Aplicar inversão nos valores RAW antes do mapeamento (se necessário)
    // XPT2046 retorna valores de 0 a 4095 (12 bits)
    constexpr uint16_t XPT2046_MAX_RAW = 4095;
    if (invert_x_) {
        rawX = XPT2046_MAX_RAW - rawX;
    }
    if (invert_y_) {
        rawY = XPT2046_MAX_RAW - rawY;
    }

    ESP_LOGD(TAG, "touch raw data: z1=%u z2=%u pressure=%u rawX=%u rawY=%u (original: %u,%u)",
             z1, z2, pressure, rawX, rawY, rawX_original, rawY_original);

    int32_t x = mapValue(rawX, cal_.xMin, cal_.xMax, 0, width_);
    int32_t y = mapValue(rawY, cal_.yMin, cal_.yMax, 0, height_);

    if (x < 0) x = 0;
    if (x > width_) x = width_;
    if (y < 0) y = 0;
    if (y > height_) y = height_;

    return TouchPoint{
        static_cast<uint16_t>(x),
        static_cast<uint16_t>(y),
        rawX_original,  // Retornar valores RAW originais (para calibração)
        rawY_original,  // Retornar valores RAW originais (para calibração)
        pressure};
}

int32_t Xpt2046Bitbang::mapValue(int32_t val, int32_t inMin, int32_t inMax,
                                 int32_t outMin, int32_t outMax) {
    if (inMax == inMin) {
        return outMin;
    }
    return (val - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}




