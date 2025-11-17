idf_component_register(SRCS "display_driver.cpp"
                      INCLUDE_DIRS "include"
                      REQUIRES driver esp_driver_spi esp_driver_gpio esp_lcd espressif__esp_lcd_ili9341 touch_bitbang lvgl esp_timer nvs_flash)

