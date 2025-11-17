extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

#include "display_driver.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ui_driver.hpp"

static const char *TAG = "CYD_APP";

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Inicializando componentes...");

    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    auto &display = DisplayDriver::instance();
    const esp_err_t init_result = display.init();
    if (init_result != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar display: %s", esp_err_to_name(init_result));
        return;
    }

    ui::init(display.lvgl_display());

    ESP_LOGI(TAG, "Sistema pronto. Aplicação de pesquisa de satisfação rodando...");

    // Loop principal - atualiza UI periodicamente
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));
        ui::update();
    }
}
