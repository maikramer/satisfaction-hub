#include "display_driver.hpp"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs.h"
#include "lvgl.h"
#include <new>
#include <cstring>

namespace {
constexpr char TAG[] = "DisplayDriver";

// Pinout correto do CYD (Cheap Yellow Display) conforme documentação oficial
// Display usa HSPI (SPI1_HOST)
// Referência: https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/PINS.md
constexpr gpio_num_t PIN_NUM_MOSI = GPIO_NUM_13;      // TFT_SDI (HSPI MOSI) - IO13
constexpr gpio_num_t PIN_NUM_CLK = GPIO_NUM_14;       // TFT_SCK (HSPI CLK) - IO14
constexpr gpio_num_t PIN_NUM_CS = GPIO_NUM_15;        // TFT_CS - IO15
constexpr gpio_num_t PIN_NUM_DC = GPIO_NUM_2;         // TFT_RS/TFT_DC - IO2
constexpr gpio_num_t PIN_NUM_RST = GPIO_NUM_4;        // Reset (não especificado na doc, mas comum)
constexpr gpio_num_t PIN_NUM_BK_LIGHT = GPIO_NUM_21;  // TFT_BL (Backlight) - IO21
constexpr gpio_num_t PIN_NUM_MISO = GPIO_NUM_12;      // TFT_SDO (TFT_MISO) - IO12 (para display, mas não usado)

// Pinos do touch screen XPT2046 conforme documentação oficial
// Touch usa pinos diferentes do display (não compartilha SPI)
constexpr gpio_num_t PIN_NUM_TOUCH_MOSI = GPIO_NUM_32;  // XPT2046_MOSI - IO32
constexpr gpio_num_t PIN_NUM_TOUCH_CLK = GPIO_NUM_25;  // XPT2046_CLK - IO25
constexpr gpio_num_t PIN_NUM_TOUCH_CS = GPIO_NUM_33;   // XPT2046_CS - IO33
constexpr gpio_num_t PIN_NUM_TOUCH_MISO = GPIO_NUM_39; // XPT2046_MISO - IO39
constexpr uint32_t LCD_PIXEL_CLOCK_HZ = 26 * 1000 * 1000; // 26 MHz
constexpr int LCD_H_RES = 320;
constexpr int LCD_V_RES = 240;
// Display: usar SPI2 (VSPI) mas com pinos do HSPI (pinos podem ser remapeados)
// SPI1 está sendo usado pela flash, então usamos SPI2 com os mesmos pinos físicos
constexpr spi_host_device_t LCD_HOST = SPI2_HOST;  // VSPI com pinos remapeados para HSPI

// Calibração inicial (valores aproximados para o CYD; ajuste conforme necessário)
constexpr TouchCalibration TOUCH_CALIB = {300, 3800, 350, 3650};
constexpr bool TOUCH_INVERT_X = true;   // X precisa ser invertido
constexpr bool TOUCH_INVERT_Y = true;   // Touch está invertido em Y - valores maiores = topo
constexpr char TOUCH_CALIB_NVS_NAMESPACE[] = "touch_cal";
constexpr char TOUCH_CALIB_NVS_KEY[] = "cal";
} // namespace

// Mutex para proteger acesso ao LVGL (substitui lvgl_port_lock) - precisa estar fora do namespace
SemaphoreHandle_t lvgl_mutex = nullptr;
TaskHandle_t lvgl_task_handle = nullptr;
esp_timer_handle_t lvgl_tick_timer = nullptr;

// Task para LVGL timer handler
void lvgl_timer_task(void *pvParameters) {
    lvgl_task_handle = xTaskGetCurrentTaskHandle();
    const TickType_t delay_ms = pdMS_TO_TICKS(10); // 10ms = 100Hz (balance entre responsividade e CPU)
    static uint32_t handler_count = 0;
    while (1) {
        if (lvgl_mutex != nullptr) {
            // Tentar adquirir mutex sem timeout - se não conseguir, pular este ciclo
            // Isso garante que eventos sejam processados mesmo se houver bloqueio temporário
            if (xSemaphoreTake(lvgl_mutex, 0) == pdTRUE) {
                lv_timer_handler();
                xSemaphoreGive(lvgl_mutex);
                handler_count++;
            }
        } else {
            lv_timer_handler();
            handler_count++;
        }
        // Dar tempo ao IDLE task para evitar watchdog
        vTaskDelay(delay_ms);
    }
}

// Tick callback para LVGL (incrementa contador de tempo)
static void lvgl_tick_cb(void *arg) {
    lv_tick_inc(1);
}

// Flush callback para LVGL - envia dados para o painel LCD
// Precisa estar fora do namespace para ser acessível como callback
void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    static int flush_count = 0;
    flush_count++;
    
    DisplayDriver *driver = static_cast<DisplayDriver *>(lv_display_get_user_data(disp));
    if (driver == nullptr) {
        ESP_LOGE("DisplayDriver", "Flush callback: driver é nullptr");
        lv_display_flush_ready(disp);
        return;
    }

    // Acessar panel_handle através de método público ou friend
    esp_lcd_panel_handle_t panel_handle = driver->get_panel_handle();
    if (panel_handle == nullptr) {
        ESP_LOGE("DisplayDriver", "Flush callback: panel_handle é nullptr");
        lv_display_flush_ready(disp);
        return;
    }

    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;
    
    int32_t width = x2 - x1 + 1;
    int32_t height = y2 - y1 + 1;
    int32_t pixel_count = width * height;
    
    // Log mais frequente para debug inicial
    if (flush_count <= 20 || flush_count % 50 == 0) {
        ESP_LOGI("DisplayDriver", "Flush #%d: área (%d,%d) a (%d,%d), tamanho=%dx%d, px_map=%p", 
                 flush_count, x1, y1, x2, y2, width, height, px_map);
    }

    // Painel configurado como RGB, então podemos usar os dados diretamente do LVGL
    // LVGL gera RGB565, que é compatível com o painel RGB
    // Não precisamos fazer conversão, o que elimina artefatos e melhora performance
    uint16_t *pixels = (uint16_t *)px_map;

    // Enviar bitmap diretamente para o painel (sem conversão)
    // esp_lcd_panel_draw_bitmap espera coordenadas onde x_end e y_end são exclusivos
    esp_err_t err = esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, (void *)pixels);
    if (err != ESP_OK) {
        ESP_LOGE("DisplayDriver", "Erro ao desenhar bitmap: %s", esp_err_to_name(err));
    }

    // Informar LVGL que o flush está completo
    lv_display_flush_ready(disp);
}

DisplayDriver &DisplayDriver::instance() {
    static DisplayDriver driver;
    return driver;
}

esp_err_t DisplayDriver::init() {
    if (initialized_) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Iniciando display driver...");
    ESP_LOGI(TAG, "Pinos configurados:");
    ESP_LOGI(TAG, "  MOSI: GPIO %d", PIN_NUM_MOSI);
    ESP_LOGI(TAG, "  CLK:  GPIO %d", PIN_NUM_CLK);
    ESP_LOGI(TAG, "  CS:   GPIO %d", PIN_NUM_CS);
    ESP_LOGI(TAG, "  DC:   GPIO %d", PIN_NUM_DC);
    ESP_LOGI(TAG, "  RST:  GPIO %d", PIN_NUM_RST);
    ESP_LOGI(TAG, "  BL:   GPIO %d", PIN_NUM_BK_LIGHT);
    ESP_LOGI(TAG, "  SPI Host: SPI2 (VSPI) - pinos remapeados para HSPI (SPI1 em uso pela flash)");

    ESP_RETURN_ON_ERROR(init_backlight(), TAG, "Backlight init failed");
    
    // Tentar inicializar SPI para display primeiro
    esp_err_t spi_err = init_spi_bus();
    if (spi_err != ESP_OK && spi_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(spi_err));
        return spi_err;
    }
    
    ESP_RETURN_ON_ERROR(init_panel_io(), TAG, "Panel IO init failed");
    ESP_RETURN_ON_ERROR(init_panel_device(), TAG, "Panel device init failed");
    
    // Inicializar touch usando software SPI (bit-banging)
    // Isso resolve o problema de não poder usar SPI1 com pinos diferentes
    ESP_RETURN_ON_ERROR(init_touch(), TAG, "Touch init failed");
    
    ESP_RETURN_ON_ERROR(init_lvgl(), TAG, "LVGL init failed");
    ESP_RETURN_ON_ERROR(create_lvgl_display(), TAG, "LVGL display creation failed");
    
    // Adicionar touch ao LVGL
    ESP_RETURN_ON_ERROR(add_touch_to_lvgl(), TAG, "LVGL touch registration failed");

    initialized_ = true;
    ESP_LOGI(TAG, "Display driver inicializado com sucesso");
    return ESP_OK;
}

lv_display_t *DisplayDriver::lvgl_display() const {
    return lv_display_;
}

esp_err_t DisplayDriver::init_backlight() {
    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = 1ULL << PIN_NUM_BK_LIGHT,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bk_gpio_config), TAG, "Backlight gpio config failed");
    gpio_set_level(PIN_NUM_BK_LIGHT, 1);
    return ESP_OK;
}

esp_err_t DisplayDriver::init_spi_bus() {
    if (spi_initialized_) {
        return ESP_OK;
    }

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = PIN_NUM_MOSI;
    buscfg.miso_io_num = PIN_NUM_MISO;  // IO12 conforme documentação (TFT_SDO)
    buscfg.sclk_io_num = PIN_NUM_CLK;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);

    esp_err_t err = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err == ESP_ERR_INVALID_STATE) {
        // SPI já inicializado - tentar desinicializar e reinicializar
        // Isso pode ser necessário se o SPI foi inicializado com configuração diferente
        ESP_LOGW(TAG, "SPI bus já inicializado, tentando desinicializar e reinicializar...");
        
        // Não podemos desinicializar se estiver em uso pela flash
        // Então vamos apenas marcar como inicializado e tentar usar o handle existente
        spi_initialized_ = true;
        ESP_LOGW(TAG, "Usando SPI existente - pode falhar se pinos forem diferentes");
        return ESP_OK;
    }
    if (err == ESP_ERR_INVALID_ARG) {
        // Argumentos inválidos - pode ser conflito de pinos
        ESP_LOGE(TAG, "SPI bus init failed: argumentos inválidos. Verifique conflitos de pinos.");
        return err;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "spi_bus_initialize failed");
    spi_initialized_ = true;
    ESP_LOGI(TAG, "SPI bus inicializado com sucesso");
    return ESP_OK;
}

esp_err_t DisplayDriver::init_panel_io() {
    if (panel_io_ != nullptr) {
        return ESP_OK;
    }

    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = PIN_NUM_CS;
    io_config.dc_gpio_num = PIN_NUM_DC;
    io_config.spi_mode = 0;
    io_config.pclk_hz = LCD_PIXEL_CLOCK_HZ;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.trans_queue_depth = 10;
    io_config.on_color_trans_done = nullptr;

    // Tentar criar panel IO
    // Se o SPI já estiver inicializado, ainda podemos criar o panel IO usando o handle
    ESP_LOGI(TAG, "Criando panel IO SPI:");
    ESP_LOGI(TAG, "  CS: GPIO %d", PIN_NUM_CS);
    ESP_LOGI(TAG, "  DC: GPIO %d", PIN_NUM_DC);
    ESP_LOGI(TAG, "  Clock: %d Hz", LCD_PIXEL_CLOCK_HZ);
    
    esp_err_t err = esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &panel_io_);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_io_spi failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "Erro detalhado: 0x%x", err);
        ESP_LOGE(TAG, "Possíveis causas:");
        ESP_LOGE(TAG, "  1. SPI1 já inicializado com pinos diferentes");
        ESP_LOGE(TAG, "  2. Conflito de pinos com flash ou outros dispositivos");
        ESP_LOGE(TAG, "  3. Pinos incorretos para este modelo de CYD");
        return err;
    }
    
    ESP_LOGI(TAG, "Panel IO criado com sucesso");
    return ESP_OK;
}

esp_err_t DisplayDriver::init_panel_device() {
    if (panel_handle_ != nullptr) {
        return ESP_OK;
    }

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = PIN_NUM_RST;
    // ILI9341 - usar RGB para evitar conversão manual que pode causar artefatos
    // A conversão será feita no flush callback se necessário
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ili9341(panel_io_, &panel_config, &panel_handle_),
                        TAG, "esp_lcd_new_panel_ili9341 failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel_handle_), TAG, "panel reset failed");
    vTaskDelay(pdMS_TO_TICKS(120));  // Aguardar estabilização após reset
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel_handle_), TAG, "panel init failed");
    
    // Corrigir espelhamento horizontal (X) - usuário reportou que está espelhado
    ESP_LOGI(TAG, "Aplicando espelhamento horizontal para corrigir orientação...");
    esp_lcd_panel_mirror(panel_handle_, true, false);
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel_handle_, true), TAG, "panel on failed");
    
    // Teste básico: preencher tela com cor vermelha para verificar se o display funciona
    ESP_LOGI(TAG, "Testando display com cor sólida...");
    uint16_t test_color = 0xF800; // Vermelho em RGB565 (R=31, G=0, B=0)
    uint16_t *test_buffer = static_cast<uint16_t*>(heap_caps_malloc(LCD_H_RES * sizeof(uint16_t), MALLOC_CAP_DMA));
    if (test_buffer != nullptr) {
        // Preencher buffer com cor de teste
        for (int i = 0; i < LCD_H_RES; i++) {
            test_buffer[i] = test_color;
        }
        // Desenhar linha por linha
        for (int y = 0; y < LCD_V_RES; y++) {
            esp_lcd_panel_draw_bitmap(panel_handle_, 0, y, LCD_H_RES, y + 1, test_buffer);
        }
        heap_caps_free(test_buffer);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Mostrar cor de teste por 1s
        ESP_LOGI(TAG, "Teste de cor concluído - tela deve estar vermelha");
    } else {
        ESP_LOGW(TAG, "Não foi possível alocar buffer para teste");
    }
    
    return ESP_OK;
}

esp_err_t DisplayDriver::init_touch() {
    if (touch_controller_ != nullptr) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Inicializando touch screen XPT2046 (bit-banging)...");
    ESP_LOGI(TAG, "  Touch MOSI: GPIO %d", PIN_NUM_TOUCH_MOSI);
    ESP_LOGI(TAG, "  Touch CLK: GPIO %d", PIN_NUM_TOUCH_CLK);
    ESP_LOGI(TAG, "  Touch CS: GPIO %d", PIN_NUM_TOUCH_CS);
    ESP_LOGI(TAG, "  Touch MISO: GPIO %d", PIN_NUM_TOUCH_MISO);

    touch_controller_ = new (std::nothrow) Xpt2046Bitbang(
        PIN_NUM_TOUCH_MOSI,
        PIN_NUM_TOUCH_MISO,
        PIN_NUM_TOUCH_CLK,
        PIN_NUM_TOUCH_CS,
        LCD_H_RES,
        LCD_V_RES);

    if (touch_controller_ == nullptr) {
        ESP_LOGE(TAG, "Falha ao alocar Xpt2046Bitbang");
        return ESP_ERR_NO_MEM;
    }

    touch_controller_->begin();
    // Configurar inversão antes da calibração para que a inversão seja aplicada nos valores RAW
    touch_controller_->setInversion(TOUCH_INVERT_X, TOUCH_INVERT_Y);
    touch_controller_->setCalibration(
        TOUCH_CALIB.xMin, TOUCH_CALIB.xMax,
        TOUCH_CALIB.yMin, TOUCH_CALIB.yMax);
    current_touch_calibration_ = TOUCH_CALIB;
    touch_calibration_loaded_ = false;
    load_touch_calibration_from_nvs();

    ESP_LOGI(TAG, "Touch screen XPT2046 pronto (bit-bang)");
    return ESP_OK;
}

esp_err_t DisplayDriver::init_lvgl() {
    if (lvgl_port_initialized_) {
        ESP_LOGI(TAG, "LVGL já inicializado");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Criando mutex para LVGL...");
    // Criar mutex para proteger acesso ao LVGL
    lvgl_mutex = xSemaphoreCreateMutex();
    if (lvgl_mutex == nullptr) {
        ESP_LOGE(TAG, "Falha ao criar mutex LVGL");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Mutex criado: %p", lvgl_mutex);

    ESP_LOGI(TAG, "Inicializando LVGL...");
    // Inicializar LVGL
    lv_init();

    ESP_LOGI(TAG, "Criando timer de tick do LVGL...");
    esp_timer_create_args_t tick_timer_args = {
        .callback = &lvgl_tick_cb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick",
        .skip_unhandled_events = false
    };
    if (esp_timer_create(&tick_timer_args, &lvgl_tick_timer) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar timer de tick do LVGL");
        return ESP_FAIL;
    }
    if (esp_timer_start_periodic(lvgl_tick_timer, 1000) != ESP_OK) { // 1 ms
        ESP_LOGE(TAG, "Falha ao iniciar timer de tick do LVGL");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Criando task para timer handler do LVGL...");
    // Criar task para timer handler do LVGL
    // Prioridade 1 (acima do IDLE que é 0) para não bloquear o watchdog
    // Core 1 para não interferir com o IDLE do Core 0 (evita watchdog)
    // Stack size aumentado para 8192 para evitar stack overflow durante refresh de telas
    TaskHandle_t created_task_handle = nullptr;
    BaseType_t task_result = xTaskCreatePinnedToCore(
        lvgl_timer_task,
        "lvgl_timer",
        8192,  // Stack size aumentado de 4096 para 8192 para evitar overflow
        nullptr,
        1,     // Priority (reduzida de 5 para 1)
        &created_task_handle,
        1      // Core 1 para evitar watchdog no Core 0
    );
    
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar task lvgl_timer");
        return ESP_FAIL;
    }
    lvgl_task_handle = created_task_handle;
    ESP_LOGI(TAG, "Task lvgl_timer criada com sucesso");

    lvgl_port_initialized_ = true;
    ESP_LOGI(TAG, "LVGL inicializado com sucesso");
    return ESP_OK;
}

esp_err_t DisplayDriver::add_touch_to_lvgl() {
    if (lv_touch_indev_ != nullptr) {
        return ESP_OK;
    }

    if (touch_controller_ == nullptr || lv_display_ == nullptr) {
        ESP_LOGE(TAG, "Touch ou display não inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    if (lvgl_mutex != nullptr) {
        xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    }

    lv_touch_indev_ = lv_indev_create();
    if (lv_touch_indev_ != nullptr) {
        lv_indev_set_type(lv_touch_indev_, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(lv_touch_indev_, lvgl_touch_read_cb);
        lv_indev_set_disp(lv_touch_indev_, lv_display_);
        lv_indev_set_driver_data(lv_touch_indev_, this);
        ESP_LOGI(TAG, "LVGL indev para touch criado: %p", static_cast<void *>(lv_touch_indev_));
    }

    if (lvgl_mutex != nullptr) {
        xSemaphoreGive(lvgl_mutex);
    }

    if (lv_touch_indev_ == nullptr) {
        ESP_LOGE(TAG, "Falha ao criar LVGL indev para touch");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Touch screen registrado no LVGL com sucesso");
    return ESP_OK;
}

void DisplayDriver::apply_touch_calibration(const TouchCalibration &calibration) {
    current_touch_calibration_ = calibration;
    if (touch_controller_ != nullptr) {
        // Garantir que a inversão está configurada antes da calibração
        touch_controller_->setInversion(TOUCH_INVERT_X, TOUCH_INVERT_Y);
        touch_controller_->setCalibration(
            calibration.xMin, calibration.xMax,
            calibration.yMin, calibration.yMax);
    }
}

void DisplayDriver::load_touch_calibration_from_nvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(TOUCH_CALIB_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Nenhuma calibração de touch salva (%s)", esp_err_to_name(err));
        return;
    }

    TouchCalibration stored = {};
    size_t size = sizeof(stored);
    err = nvs_get_blob(handle, TOUCH_CALIB_NVS_KEY, &stored, &size);
    nvs_close(handle);

    if (err == ESP_OK && size == sizeof(stored) &&
        stored.xMax > stored.xMin && stored.yMax > stored.yMin) {
        apply_touch_calibration(stored);
        touch_calibration_loaded_ = true;
        ESP_LOGI(TAG, "Calibração carregada: x[%u-%u] y[%u-%u]",
                 stored.xMin, stored.xMax, stored.yMin, stored.yMax);
    } else {
        ESP_LOGW(TAG, "Falha ao carregar calibração (%s)", esp_err_to_name(err));
    }
}

void DisplayDriver::save_touch_calibration_to_nvs(const TouchCalibration &calibration) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(TOUCH_CALIB_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Não foi possível abrir NVS para salvar calibração (%s)", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(handle, TOUCH_CALIB_NVS_KEY, &calibration, sizeof(calibration));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Calibração salva: x[%u-%u] y[%u-%u]",
                 calibration.xMin, calibration.xMax,
                 calibration.yMin, calibration.yMax);
    } else {
        ESP_LOGE(TAG, "Falha ao salvar calibração (%s)", esp_err_to_name(err));
    }
}

void DisplayDriver::update_touch_calibration(const TouchCalibration &calibration) {
    apply_touch_calibration(calibration);
    touch_calibration_loaded_ = true;
    save_touch_calibration_to_nvs(calibration);
}

esp_err_t DisplayDriver::create_lvgl_display() {
    if (lv_display_ != nullptr) {
        return ESP_OK;
    }

    // Criar display LVGL
    lv_display_ = lv_display_create(LCD_H_RES, LCD_V_RES);
    if (lv_display_ == nullptr) {
        ESP_LOGE(TAG, "lv_display_create retornou nullptr");
        return ESP_FAIL;
    }

    // Configurar formato de cor
    // Se o painel é BGR e LVGL gera RGB565, precisamos trocar bytes ou usar formato swapped
    // Tentar RGB565 primeiro, se cores estiverem trocadas, mudar para RGB565_SWAPPED
    lv_display_set_color_format(lv_display_, LV_COLOR_FORMAT_RGB565);

    // Alocar buffers DMA-capable - usar PARTIAL mode (mais eficiente em memória)
    // PARTIAL mode usa buffers menores - aumentado para 1/10 da tela para evitar truncamento
    // 1/10 = ~7680 pixels = ~15KB por buffer (total ~30KB)
    // Isso ainda economiza RAM comparado ao tamanho total (76.8KB), mas é suficiente
    // para renderizar elementos maiores (botões, fontes) sem truncamento
    constexpr size_t buffer_size = LCD_H_RES * LCD_V_RES / 10; // 1/10 da tela (balance RAM/qualidade)
    constexpr size_t buffer_bytes = buffer_size * sizeof(uint16_t);
    
    void *buf1 = heap_caps_malloc(buffer_bytes, MALLOC_CAP_DMA);
    void *buf2 = heap_caps_malloc(buffer_bytes, MALLOC_CAP_DMA);
    
    if (buf1 == nullptr || buf2 == nullptr) {
        ESP_LOGE(TAG, "Falha ao alocar buffers LVGL (tentando alocar %d bytes cada)", buffer_bytes);
        if (buf1) heap_caps_free(buf1);
        if (buf2) heap_caps_free(buf2);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Buffers LVGL alocados: %d bytes cada (total: %d bytes)", buffer_bytes, buffer_bytes * 2);

    // Configurar buffers em modo PARTIAL (mais eficiente em memória)
    // A conversão RGB565->BGR565 no flush callback deve resolver os artefatos
    lv_display_set_buffers(lv_display_, buf1, buf2, buffer_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Configurar flush callback
    lv_display_set_flush_cb(lv_display_, lvgl_flush_cb);
    
    // Armazenar ponteiro para o driver nos dados do display
    lv_display_set_user_data(lv_display_, this);

    ESP_LOGI(TAG, "Display LVGL criado com sucesso (buffers: %d bytes cada)", buffer_bytes);
    return ESP_OK;
}

void DisplayDriver::lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    static uint32_t read_count = 0;
    static lv_indev_state_t last_state = LV_INDEV_STATE_RELEASED;
    read_count++;
    
    auto *driver = static_cast<DisplayDriver *>(lv_indev_get_driver_data(indev));
    if (driver == nullptr || driver->touch_controller_ == nullptr) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    TouchPoint point = driver->touch_controller_->getTouch();
    driver->last_touch_point_ = point;
    if (point.pressure > 0) {
        // A inversão já é aplicada nos valores RAW antes do mapeamento no Xpt2046Bitbang
        // Então point.x e point.y já estão na orientação correta
        int16_t mapped_x = static_cast<int16_t>(point.x);
        int16_t mapped_y = static_cast<int16_t>(point.y);

        if (mapped_x < 0) mapped_x = 0;
        if (mapped_x >= LCD_H_RES) mapped_x = LCD_H_RES - 1;
        if (mapped_y < 0) mapped_y = 0;
        if (mapped_y >= LCD_V_RES) mapped_y = LCD_V_RES - 1;

        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = mapped_x;
        data->point.y = mapped_y;

        if (last_state != LV_INDEV_STATE_PRESSED) {
            ESP_LOGI("TouchLVGL", "press raw=(%u,%u) mapped=(%d,%d) pressure=%u",
                     point.rawX, point.rawY, mapped_x, mapped_y, point.pressure);
        }
        last_state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        if (last_state != LV_INDEV_STATE_RELEASED) {
            ESP_LOGI("TouchLVGL", "release");
        }
        last_state = LV_INDEV_STATE_RELEASED;
    }
}

