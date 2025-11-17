#pragma once

#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include <cstdint>

struct TouchCalibration {
    uint16_t xMin;
    uint16_t xMax;
    uint16_t yMin;
    uint16_t yMax;
};

struct TouchPoint {
    uint16_t x;
    uint16_t y;
    uint16_t rawX;
    uint16_t rawY;
    uint16_t pressure;
};

class Xpt2046Bitbang {
public:
    Xpt2046Bitbang(gpio_num_t mosi,
                   gpio_num_t miso,
                   gpio_num_t clk,
                   gpio_num_t cs,
                   uint16_t screenWidth,
                   uint16_t screenHeight);

    void begin();
    void setCalibration(uint16_t xMin, uint16_t xMax,
                        uint16_t yMin, uint16_t yMax);
    void setInversion(bool invertX, bool invertY);
    TouchPoint getTouch();

private:
    static constexpr uint32_t DELAY_US = 2;
    static constexpr uint8_t CMD_READ_X  = 0b10010000;
    static constexpr uint8_t CMD_READ_Y  = 0b11010000;
    static constexpr uint8_t CMD_READ_Z1 = 0b10110000;
    static constexpr uint8_t CMD_READ_Z2 = 0b11000000;

    gpio_num_t mosi_;
    gpio_num_t miso_;
    gpio_num_t clk_;
    gpio_num_t cs_;
    uint16_t width_;
    uint16_t height_;
    TouchCalibration cal_;
    bool invert_x_;
    bool invert_y_;

    void writeSpi(uint8_t command);
    uint16_t readSpi(uint8_t command);
    static int32_t mapValue(int32_t val, int32_t inMin, int32_t inMax,
                            int32_t outMin, int32_t outMax);
};




