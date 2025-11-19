#include "ui_driver.hpp"
#include "ui_common.hpp"
#include "screens/wifi_config_screen.hpp"
#include "screens/brightness_screen.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include "display_driver.hpp"
#include "wifi_manager.hpp"
#include "supabase_driver.hpp"

// Declarar fonte Roboto customizada (suporta acentos portugueses)
LV_FONT_DECLARE(roboto);
// Declarar fonte Montserrat para ícones (padrão do LVGL)
LV_FONT_DECLARE(lv_font_montserrat_20);

// Mutex externo do display_driver
extern SemaphoreHandle_t lvgl_mutex;
extern TaskHandle_t lvgl_task_handle;

extern "C" {
}

#if LV_USE_IMGFONT
// Declarar tipos e funções do imgfont manualmente já que o include pode não funcionar
extern "C" {
typedef const void * (*lv_imgfont_get_path_cb_t)(const lv_font_t * font,
                                                 uint32_t unicode, uint32_t unicode_next,
                                                 int32_t * offset_y, void * user_data);
lv_font_t * lv_imgfont_create(uint16_t height, lv_imgfont_get_path_cb_t path_cb, void * user_data);
void lv_imgfont_destroy(lv_font_t * font);
}
#endif


// Helper para lock/unlock LVGL (exportado para uso em outros arquivos)
void lvgl_lock() {
    if (lvgl_mutex != nullptr) {
        if (xTaskGetCurrentTaskHandle() == lvgl_task_handle) {
            return; // Já estamos no task do LVGL, não precisa lock
        }
        xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    }
}

void lvgl_unlock() {
    if (lvgl_mutex != nullptr) {
        if (xTaskGetCurrentTaskHandle() == lvgl_task_handle) {
            return; // Task do LVGL não adquiriu lock, nada a fazer
        }
        xSemaphoreGive(lvgl_mutex);
    }
}

namespace {
constexpr char TAG[] = "UI";

// Declaração forward
static void update_wifi_status_icon();

// Flags de inversão de touch - devem corresponder aos valores em display_driver.cpp
// IMPORTANTE: Se mudar aqui, também mudar em display_driver.cpp
constexpr bool TOUCH_INVERT_X = true;    // X precisa ser invertido
constexpr bool TOUCH_INVERT_Y = true;   // Touch está invertido em Y - valores maiores = topo

// Estados da aplicação
enum class AppState {
    CALIBRATION,   // Tela de calibração inicial
    QUESTION,      // Mostrando pergunta de satisfação
    THANK_YOU,     // Mostrando agradecimento
    CONFIGURATION, // Tela de configurações
    WIFI_CONFIG,   // Tela de configuração WiFi
    BRIGHTNESS_CONFIG, // Tela de configuração de brilho
};

AppState current_state = AppState::CALIBRATION;
int selected_rating = 0;  // 0 = nenhuma, 1-5 = avaliação selecionada
bool pending_screen_transition = false;  // Flag para transição assíncrona de tela
uint32_t transition_delay_counter = 0;  // Contador para delay antes da transição
bool thank_you_return_pending = false;  // Controla retorno automático à tela principal
uint32_t thank_you_return_counter = 0;  // Contador para delay do retorno automático
constexpr uint32_t THANK_YOU_RETURN_DELAY_CYCLES = 100;  // 100 ciclos (~10s)
bool wifi_status_last_connected = false;  // Estado conhecido do WiFi para o ícone
bool wifi_status_update_pending = false;  // Flag para atualizar ícone após mudança de estado

// Objetos LVGL
lv_obj_t *question_screen = nullptr;
lv_obj_t *thank_you_screen = nullptr;
lv_obj_t *configuration_screen = nullptr;
lv_obj_t *rating_buttons[5] = {nullptr};
lv_obj_t *question_label = nullptr;
lv_obj_t *thank_you_label = nullptr;
lv_obj_t *thank_you_summary = nullptr;
lv_obj_t *settings_button = nullptr;  // Botão de configurações na tela principal
lv_obj_t *config_calibrate_button = nullptr;  // Botão de calibração na tela de configurações
lv_obj_t *wifi_status_icon = nullptr;  // Ícone de status WiFi na tela principal

lv_display_t *display_handle = nullptr;

// Declarações forward
void start_calibration();
void show_question_screen();
void show_thank_you_screen();
void show_configuration_screen();
void create_configuration_screen();
static void update_wifi_status_icon();  // Atualizar ícone de status WiFi

// Função helper para criar cores
static lv_color_t make_color(uint32_t hex) {
    return lv_color_hex(hex);
}

// Cores para os botões de avaliação
static const lv_color_t RATING_COLORS[] = {
    make_color(0xFF0000),  // Vermelho (1 - Muito Insatisfeito)
    make_color(0xFF6600),  // Laranja (2 - Insatisfeito)
    make_color(0xFFCC00),  // Amarelo (3 - Neutro)
    make_color(0x99FF00),  // Verde claro (4 - Satisfeito)
    make_color(0x00FF00),  // Verde (5 - Muito Satisfeito)
};

// Números grandes para os botões (1 a 5)
constexpr const char *RATING_NUMBERS[] = {
    "1",  // Muito insatisfeito
    "2",  // Insatisfeito
    "3",  // Neutro
    "4",  // Satisfeito
    "5",  // Muito satisfeito
};

struct CalibrationTarget {
    const char *instruction;
    lv_point_t position;
};

static const CalibrationTarget CALIBRATION_POINTS[] = {
    {"Toque no canto superior esquerdo", {30, 30}},
    {"Toque no canto superior direito", {290, 30}},
    {"Toque no canto inferior esquerdo", {30, 210}},
    {"Toque no canto inferior direito", {290, 210}},
    {"Toque no centro", {160, 120}},
};

static constexpr int CALIBRATION_POINT_COUNT = sizeof(CALIBRATION_POINTS) / sizeof(CALIBRATION_POINTS[0]);
static constexpr int CAL_TL = 0;
static constexpr int CAL_TR = 1;
static constexpr int CAL_BL = 2;
static constexpr int CAL_BR = 3;
static constexpr int CAL_CENTER = 4;
static constexpr int CAL_TARGET_SIZE = 28;
static constexpr uint16_t CAL_MIN_PRESSURE = 150;
static TouchPoint calibration_samples[CALIBRATION_POINT_COUNT];
static int current_calibration_index = 0;
static lv_obj_t *calibration_screen = nullptr;
static lv_obj_t *calibration_label = nullptr;
static lv_obj_t *calibration_target = nullptr;
static bool calibration_point_captured = false;
static AppState state_before_calibration = AppState::QUESTION;  // Estado antes de iniciar calibração

constexpr const char *RATING_MESSAGES[] = {
    "muito insatisfeito",
    "insatisfeito",
    "neutro",
    "satisfeito",
    "muito satisfeito"
};

static char DEVICE_ID_BUFFER[17] = {0};  // 12 hex chars + null (ex: A1B2C3D4E5F6)
static bool DEVICE_ID_INITIALIZED = false;

static const char* get_device_id_string() {
    if (!DEVICE_ID_INITIALIZED) {
        uint8_t mac[6] = {0};
        esp_err_t err = esp_efuse_mac_get_default(mac);
        if (err == ESP_OK) {
            snprintf(DEVICE_ID_BUFFER, sizeof(DEVICE_ID_BUFFER),
                     "%02X%02X%02X%02X%02X%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        } else {
            ESP_LOGE(TAG, "Falha ao ler EFUSE MAC: %s", esp_err_to_name(err));
            strncpy(DEVICE_ID_BUFFER, "UNKNOWN", sizeof(DEVICE_ID_BUFFER) - 1);
            DEVICE_ID_BUFFER[sizeof(DEVICE_ID_BUFFER) - 1] = '\0';
        }
        DEVICE_ID_INITIALIZED = true;
        ESP_LOGI(TAG, "Device ID definido: %s", DEVICE_ID_BUFFER);
    }
    return DEVICE_ID_BUFFER;
}

// Fontes principais - usando fonte Roboto customizada com suporte a acentos
// Nota: A fonte atual é 6px (muito pequena). Para melhor legibilidade, 
// considere gerar versões maiores (16px, 20px, 24px) usando o LVGL Font Converter
static const lv_font_t *TITLE_FONT = &roboto;   // Roboto customizada
static const lv_font_t *TEXT_FONT = &roboto;    // Roboto customizada
static const lv_font_t *CAPTION_FONT = &roboto; // Roboto customizada

// Fontes maiores para botões (definidas nas linhas abaixo)

// Função para enviar avaliação ao Supabase
static void send_rating_to_supabase(int rating) {
    auto& wifi = wifi::WiFiManager::instance();
    if (!wifi.is_connected()) {
        ESP_LOGW(TAG, "WiFi não conectado - avaliação não será enviada ao Supabase");
        return;
    }
    
    auto& supabase = supabase::SupabaseDriver::instance();
    
    // Verificar se o Supabase está configurado
    if (!supabase.is_configured()) {
        ESP_LOGW(TAG, "Supabase não configurado - avaliação não será enviada");
        return;
    }
    
    // Preparar dados da avaliação
    supabase::RatingData rating_data;
    rating_data.rating = static_cast<int32_t>(rating);
    rating_data.message = RATING_MESSAGES[rating - 1];  // Mensagem correspondente ao rating
    rating_data.timestamp = 0;  // 0 = usar timestamp do servidor
    rating_data.device_id = get_device_id_string();
    
    ESP_LOGI(TAG, "Enviando avaliação %d (%s) para Supabase...", rating, rating_data.message);
    
    // Enviar avaliação (não bloqueia - executa em background)
    esp_err_t err = supabase.submit_rating(rating_data);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Avaliação enviada com sucesso para Supabase!");
    } else {
        ESP_LOGE(TAG, "Erro ao enviar avaliação para Supabase: %s", esp_err_to_name(err));
    }
}

// Callback para botão de avaliação
static void rating_button_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    ESP_LOGI(TAG, "rating_button_cb: evento=%d, estado=%d", code, static_cast<int>(current_state));
    
    // Processar apenas eventos de clique
    if (code == LV_EVENT_CLICKED && current_state == AppState::QUESTION) {
        // Obter o índice do botão (1-5)
        int rating = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
        selected_rating = rating;
        
        ESP_LOGI(TAG, "Avaliação selecionada: %d", rating);
        
        // Enviar avaliação ao Supabase (se WiFi estiver conectado)
        send_rating_to_supabase(rating);
        
        // Marcar transição pendente (será processada no update() para evitar problemas de contexto)
        pending_screen_transition = true;
        transition_delay_counter = 0;  // Resetar contador de delay
    }
}

// Callback para botão de configurações
static void settings_button_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Abrindo tela de configurações...");
        show_configuration_screen();
    }
}

// Callback para botão de calibração na tela de configurações
static void config_calibrate_button_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Iniciando calibração da tela de configurações...");
        start_calibration();
    }
}

// Callback para botão de voltar na tela de configurações
static void config_back_button_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Voltando para tela principal...");
        show_question_screen();
    }
}


static void update_calibration_ui() {
    if (calibration_label == nullptr || calibration_target == nullptr) {
        return;
    }
    if (current_calibration_index >= CALIBRATION_POINT_COUNT) {
        return;
    }
    const CalibrationTarget &target = CALIBRATION_POINTS[current_calibration_index];
    lv_label_set_text_fmt(calibration_label, "Passo %d/%d\n%s",
                          current_calibration_index + 1, CALIBRATION_POINT_COUNT,
                          target.instruction);
    lv_obj_set_pos(calibration_target,
                   target.position.x - CAL_TARGET_SIZE / 2,
                   target.position.y - CAL_TARGET_SIZE / 2);
}

static void finish_calibration() {
    calibration_point_captured = false;

    auto avg = [](uint16_t a, uint16_t b) -> uint32_t {
        return (static_cast<uint32_t>(a) + static_cast<uint32_t>(b)) / 2;
    };

    // Log detalhado dos valores RAW capturados
    ESP_LOGI(TAG, "=== VALORES RAW CAPTURADOS ===");
    ESP_LOGI(TAG, "TL: rawX=%u rawY=%u", calibration_samples[CAL_TL].rawX, calibration_samples[CAL_TL].rawY);
    ESP_LOGI(TAG, "TR: rawX=%u rawY=%u", calibration_samples[CAL_TR].rawX, calibration_samples[CAL_TR].rawY);
    ESP_LOGI(TAG, "BR: rawX=%u rawY=%u", calibration_samples[CAL_BR].rawX, calibration_samples[CAL_BR].rawY);
    ESP_LOGI(TAG, "BL: rawX=%u rawY=%u", calibration_samples[CAL_BL].rawX, calibration_samples[CAL_BL].rawY);
    ESP_LOGI(TAG, "CENTER: rawX=%u rawY=%u", calibration_samples[CAL_CENTER].rawX, calibration_samples[CAL_CENTER].rawY);

    TouchCalibration new_cal = {};
    
    // Obter as posições reais dos pontos de calibração (não os cantos absolutos)
    const int screen_w = 320;
    const int screen_h = 240;
    
    // Posições reais dos pontos de calibração (centro do alvo)
    const int cal_tl_x = CALIBRATION_POINTS[CAL_TL].position.x;  // 30
    const int cal_tl_y = CALIBRATION_POINTS[CAL_TL].position.y;  // 30
    const int cal_tr_x = CALIBRATION_POINTS[CAL_TR].position.x;  // 290
    const int cal_tr_y = CALIBRATION_POINTS[CAL_TR].position.y;  // 30
    const int cal_bl_x = CALIBRATION_POINTS[CAL_BL].position.x;  // 30
    const int cal_bl_y = CALIBRATION_POINTS[CAL_BL].position.y;  // 210
    const int cal_br_x = CALIBRATION_POINTS[CAL_BR].position.x;  // 290
    const int cal_br_y = CALIBRATION_POINTS[CAL_BR].position.y;  // 210
    
    // Os valores RAW capturados são originais (não invertidos)
    // Mas a calibração precisa mapear valores RAW que serão invertidos antes do mapeamento
    // Então precisamos inverter os valores RAW durante a calibração se a inversão estiver ativa
    constexpr uint16_t XPT2046_MAX_RAW = 4095;
    
    uint32_t raw_left, raw_right, raw_top, raw_bottom;
    
    if (TOUCH_INVERT_X) {
        // Inverter X: valores maiores RAW = esquerda na tela, valores menores RAW = direita na tela
        // Mas na calibração, queremos que raw_left corresponda à esquerda física
        // Então invertemos: raw_left vem de TR/BR (que são direita física)
        raw_left = avg(XPT2046_MAX_RAW - calibration_samples[CAL_TR].rawX, 
                       XPT2046_MAX_RAW - calibration_samples[CAL_BR].rawX);
        raw_right = avg(XPT2046_MAX_RAW - calibration_samples[CAL_TL].rawX, 
                        XPT2046_MAX_RAW - calibration_samples[CAL_BL].rawX);
    } else {
        raw_left = avg(calibration_samples[CAL_TL].rawX, calibration_samples[CAL_BL].rawX);
        raw_right = avg(calibration_samples[CAL_TR].rawX, calibration_samples[CAL_BR].rawX);
    }
    
    if (TOUCH_INVERT_Y) {
        // Inverter Y: valores maiores RAW = topo na tela, valores menores RAW = base na tela
        raw_top = avg(XPT2046_MAX_RAW - calibration_samples[CAL_TL].rawY, 
                      XPT2046_MAX_RAW - calibration_samples[CAL_TR].rawY);
        raw_bottom = avg(XPT2046_MAX_RAW - calibration_samples[CAL_BL].rawY, 
                         XPT2046_MAX_RAW - calibration_samples[CAL_BR].rawY);
    } else {
        raw_top = avg(calibration_samples[CAL_TL].rawY, calibration_samples[CAL_TR].rawY);
        raw_bottom = avg(calibration_samples[CAL_BL].rawY, calibration_samples[CAL_BR].rawY);
    }

    ESP_LOGI(TAG, "=== VALORES CALCULADOS ===");
    ESP_LOGI(TAG, "raw_left=%u raw_right=%u", raw_left, raw_right);
    ESP_LOGI(TAG, "raw_top=%u raw_bottom=%u", raw_top, raw_bottom);
    ESP_LOGI(TAG, "Posições calibração: TL=(%d,%d) TR=(%d,%d) BL=(%d,%d) BR=(%d,%d)",
             cal_tl_x, cal_tl_y, cal_tr_x, cal_tr_y, cal_bl_x, cal_bl_y, cal_br_x, cal_br_y);

    // Mapear valores RAW para coordenadas de tela usando interpolação linear
    // Os valores RAW capturados correspondem às posições de calibração (30, 30) e (290, 30)
    // Precisamos extrapolar para os cantos absolutos (0 e 320 para X, 0 e 240 para Y)
    
    // Para X: raw_left corresponde a cal_tl_x (30), raw_right corresponde a cal_tr_x (290)
    // Queremos encontrar raw_xMin (que corresponde a X=0) e raw_xMax (que corresponde a X=320)
    // Usando interpolação linear: se raw_left -> 30 e raw_right -> 290
    // Então: raw_xMin = raw_left - (raw_right - raw_left) * (30 - 0) / (290 - 30)
    //        raw_xMax = raw_right + (raw_right - raw_left) * (320 - 290) / (290 - 30)
    const int32_t x_range_raw = raw_right - raw_left;
    const int32_t x_range_screen_cal = cal_tr_x - cal_tl_x;  // 290 - 30 = 260
    const int32_t x_left_offset = cal_tl_x;  // 30 (distância do canto esquerdo)
    const int32_t x_right_offset = screen_w - cal_tr_x;  // 320 - 290 = 30 (distância do canto direito)
    
    // Extrapolar para o canto esquerdo (X=0)
    // raw_xMin = raw_left - (raw_right - raw_left) * x_left_offset / x_range_screen_cal
    int32_t xMin_calc = raw_left - (x_range_raw * x_left_offset) / x_range_screen_cal;
    if (xMin_calc < 0) xMin_calc = 0;
    
    // Extrapolar para o canto direito (X=320)
    // raw_xMax = raw_right + (raw_right - raw_left) * x_right_offset / x_range_screen_cal
    int32_t xMax_calc = raw_right + (x_range_raw * x_right_offset) / x_range_screen_cal;
    
    new_cal.xMin = static_cast<uint16_t>(xMin_calc);
    new_cal.xMax = static_cast<uint16_t>(xMax_calc);

    // Para Y: similar, mas precisamos considerar inversão
    // raw_top corresponde a cal_tl_y (30), raw_bottom corresponde a cal_bl_y (210)
    // Queremos encontrar raw_yMin (que corresponde a Y=0) e raw_yMax (que corresponde a Y=240)
    const int32_t y_range_raw = (raw_bottom > raw_top) ? (raw_bottom - raw_top) : (raw_top - raw_bottom);
    const int32_t y_range_screen_cal = cal_bl_y - cal_tl_y;  // 210 - 30 = 180
    const int32_t y_top_offset = cal_tl_y;  // 30 (distância do topo)
    const int32_t y_bottom_offset = screen_h - cal_bl_y;  // 240 - 210 = 30 (distância da base)
    
    int32_t yMin_calc, yMax_calc;
    if (raw_top < raw_bottom) {
        // Y normal: valores menores = topo
        // Extrapolar para o topo (Y=0)
        yMin_calc = raw_top - (y_range_raw * y_top_offset) / y_range_screen_cal;
        if (yMin_calc < 0) yMin_calc = 0;
        // Extrapolar para a base (Y=240)
        yMax_calc = raw_bottom + (y_range_raw * y_bottom_offset) / y_range_screen_cal;
    } else {
        // Y invertido: valores maiores = topo
        // Extrapolar para o topo (Y=0) - raw_top é maior, então corresponde ao topo
        yMax_calc = raw_top + (y_range_raw * y_top_offset) / y_range_screen_cal;
        // Extrapolar para a base (Y=240) - raw_bottom é menor, então corresponde à base
        yMin_calc = raw_bottom - (y_range_raw * y_bottom_offset) / y_range_screen_cal;
        if (yMin_calc < 0) yMin_calc = 0;
    }
    new_cal.yMin = static_cast<uint16_t>(yMin_calc);
    new_cal.yMax = static_cast<uint16_t>(yMax_calc);

    if (new_cal.xMin > new_cal.xMax) {
        std::swap(new_cal.xMin, new_cal.xMax);
    }
    if (new_cal.yMin > new_cal.yMax) {
        std::swap(new_cal.yMin, new_cal.yMax);
    }

    ESP_LOGI(TAG, "=== CALIBRAÇÃO FINAL ===");
    ESP_LOGI(TAG, "xMin=%u xMax=%u yMin=%u yMax=%u", new_cal.xMin, new_cal.xMax, new_cal.yMin, new_cal.yMax);

    DisplayDriver::instance().update_touch_calibration(new_cal);

    lvgl_lock();
    if (calibration_screen != nullptr) {
        lv_obj_del(calibration_screen);
        calibration_screen = nullptr;
        calibration_label = nullptr;
        calibration_target = nullptr;
    }
    lvgl_unlock();

    // Voltar para o estado anterior (configuração ou pergunta)
    if (state_before_calibration == AppState::CONFIGURATION) {
        current_state = AppState::CONFIGURATION;
        show_configuration_screen();
    } else {
        current_state = AppState::QUESTION;
        show_question_screen();
    }
}

static void calibration_touch_event_cb(lv_event_t *e) {
    if (current_state != AppState::CALIBRATION) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_RELEASED) {
        calibration_point_captured = false;
        return;
    }

    if (code != LV_EVENT_PRESSED || calibration_point_captured) {
        return;
    }

    TouchPoint raw = DisplayDriver::instance().last_touch_point();
    if (raw.pressure < CAL_MIN_PRESSURE) {
        return;
    }

    calibration_samples[current_calibration_index] = raw;
    calibration_point_captured = true;
    current_calibration_index++;

    if (current_calibration_index >= CALIBRATION_POINT_COUNT) {
        finish_calibration();
    } else {
        update_calibration_ui();
    }
}

void start_calibration() {
    ESP_LOGI(TAG, "Iniciando calibração do touch");
    // Salvar estado atual para voltar após calibração
    state_before_calibration = current_state;
    current_state = AppState::CALIBRATION;
    current_calibration_index = 0;
    calibration_point_captured = false;

    lvgl_lock();

    if (calibration_screen != nullptr) {
        lv_obj_del(calibration_screen);
        calibration_screen = nullptr;
        calibration_label = nullptr;
        calibration_target = nullptr;
    }

    calibration_screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(calibration_screen);
    lv_obj_set_style_bg_color(calibration_screen, lv_color_hex(0xFFFFFF), 0); // Fundo branco limpo
    lv_obj_set_style_bg_opa(calibration_screen, LV_OPA_COVER, 0);
    lv_obj_add_flag(calibration_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(calibration_screen, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_screen_load(calibration_screen);

    calibration_label = lv_label_create(calibration_screen);
    lv_label_set_text(calibration_label, "Calibrando tela...");
    lv_obj_set_width(calibration_label, 280);
    lv_obj_set_style_text_color(calibration_label, lv_color_hex(0x000000), 0); // Texto preto
    // Usar fonte Roboto customizada
    lv_obj_set_style_text_font(calibration_label, TEXT_FONT, 0);
    // Adicionar padding vertical ao label para evitar corte do texto
    lv_obj_set_style_pad_top(calibration_label, 4, 0);
    lv_obj_set_style_pad_bottom(calibration_label, 4, 0);
    lv_obj_align(calibration_label, LV_ALIGN_TOP_MID, 0, 20);

    calibration_target = lv_obj_create(calibration_screen);
    lv_obj_remove_style_all(calibration_target);
    lv_obj_set_size(calibration_target, CAL_TARGET_SIZE, CAL_TARGET_SIZE);
    lv_obj_set_style_bg_color(calibration_target, lv_color_hex(0xFF5722), 0);
    lv_obj_set_style_bg_opa(calibration_target, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(calibration_target, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(calibration_target, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(calibration_target, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_add_event_cb(calibration_screen, calibration_touch_event_cb, LV_EVENT_ALL, nullptr);
    lv_obj_add_event_cb(calibration_target, calibration_touch_event_cb, LV_EVENT_ALL, nullptr);

    update_calibration_ui();
    lvgl_unlock();
}

void create_question_screen() {
    ESP_LOGI(TAG, "create_question_screen() iniciado");
    ESP_LOGI(TAG, "Chamando lvgl_lock()...");
    lvgl_lock();
    ESP_LOGI(TAG, "Mutex adquirido, continuando criação da tela...");
    
    if (question_screen != nullptr) {
        ESP_LOGI(TAG, "Deletando tela existente...");
        lv_obj_del(question_screen);
        question_screen = nullptr;
    }
    
    ESP_LOGI(TAG, "Criando nova tela...");
    // Criar tela base - sem padding, sem estilo extra
    question_screen = lv_obj_create(nullptr);
    if (question_screen == nullptr) {
        ESP_LOGE(TAG, "Falha ao criar question_screen");
        lvgl_unlock();
        return;
    }
    ESP_LOGI(TAG, "Tela criada: %p", question_screen);
    
    ESP_LOGI(TAG, "Carregando tela como ativa...");
    // Carregar a tela ANTES de criar elementos para garantir que seja a tela ativa
    lv_screen_load(question_screen);
    ESP_LOGI(TAG, "Tela carregada");
    
    lv_obj_remove_style_all(question_screen);
    ui::common::apply_screen_style(question_screen);
    // Garantir que a tela não bloqueie eventos (deixar eventos passarem para os filhos)
    lv_obj_clear_flag(question_screen, LV_OBJ_FLAG_CLICKABLE);
    ESP_LOGI(TAG, "Estilos da tela configurados");
    
    // Header Container (Barra de status)
    lv_obj_t* header = lv_obj_create(question_screen);
    lv_obj_set_size(header, LV_PCT(100), 40);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0); // Sem borda padrão
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Ícone de status WiFi (dentro do header, à esquerda)
    if (wifi_status_icon != nullptr) {
        lv_obj_del(wifi_status_icon);
        wifi_status_icon = nullptr;
    }
    // Botão do WiFi (pode ser clicável para abrir config direta no futuro se desejar)
    wifi_status_icon = lv_button_create(header);
    lv_obj_remove_style_all(wifi_status_icon);
    lv_obj_set_size(wifi_status_icon, 32, 32);
    lv_obj_align(wifi_status_icon, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_set_style_bg_opa(wifi_status_icon, LV_OPA_TRANSP, 0); // Transparente
    lv_obj_clear_flag(wifi_status_icon, LV_OBJ_FLAG_CLICKABLE); // Por enquanto não clicável

    // Label com o símbolo WiFi
    lv_obj_t *wifi_label = lv_label_create(wifi_status_icon);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    lv_obj_center(wifi_label);
    // Usar fonte Montserrat para ícones
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_20, 0);
    // Definir cor inicial conforme estado atual do WiFi
    auto& wifi_mgr = wifi::WiFiManager::instance();
    bool wifi_connected = wifi_mgr.is_connected();
    lv_color_t wifi_color = wifi_connected ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
    lv_obj_set_style_text_color(wifi_label, wifi_color, 0);
    wifi_status_last_connected = wifi_connected;
    wifi_status_update_pending = false;

    // Botão de Configurações (dentro do header, à direita)
    if (settings_button != nullptr) {
        lv_obj_del(settings_button);
        settings_button = nullptr;
    }
    settings_button = lv_button_create(header);
    lv_obj_set_size(settings_button, 32, 32);
    lv_obj_align(settings_button, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_bg_opa(settings_button, LV_OPA_TRANSP, 0); // Transparente
    lv_obj_set_style_shadow_width(settings_button, 0, 0); // Sem sombra
    
    lv_obj_t *settings_label = lv_label_create(settings_button);
    lv_label_set_text(settings_label, LV_SYMBOL_SETTINGS);
    lv_obj_center(settings_label);
    // Usar fonte Montserrat para ícones
    lv_obj_set_style_text_font(settings_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(settings_label, ui::common::COLOR_SETTINGS_BUTTON(), 0); // Cor original
    
    lv_obj_add_event_cb(settings_button, settings_button_cb, LV_EVENT_CLICKED, nullptr);
    
    // Título simples no topo (agora abaixo do header)
    ESP_LOGI(TAG, "Criando label...");
    question_label = lv_label_create(question_screen);
    lv_label_set_text(question_label, "Como você se sentiu hoje?");
    lv_label_set_long_mode(question_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(question_label, 300);
    lv_obj_set_style_text_align(question_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(question_label, lv_color_hex(0x000000), 0); // Texto preto
    // Usar fonte Roboto customizada
    lv_obj_set_style_text_font(question_label, TITLE_FONT, 0);
    // Adicionar padding vertical ao label para evitar corte do texto
    lv_obj_set_style_pad_top(question_label, 4, 0);
    lv_obj_set_style_pad_bottom(question_label, 4, 0);
    // Mover texto mais para baixo para dar espaço ao header
    lv_obj_align(question_label, LV_ALIGN_TOP_MID, 0, 60);

    // Botões posicionados manualmente - layout simples e direto
    // 5 botões de 50px cada = 250px
    // Espaço disponível: 320px - 20px (margens) = 300px
    // Espaçamento: (300 - 250) / 4 = 12.5px entre botões
    constexpr int BTN_SIZE = 50;
    constexpr int BTN_SPACING = 12;
    // Calcular posição inicial para centralizar os botões
    // Total de largura: 5 * BTN_SIZE + 4 * BTN_SPACING = 250 + 48 = 298px
    // Posição inicial: (320 - 298) / 2 = 11px
    constexpr int BTN_START_X = 11;
    constexpr int BTN_Y = 140; // Um pouco mais baixo para dar espaço ao texto
    
    ESP_LOGI(TAG, "Criando %d botões...", 5);
    ESP_LOGI(TAG, "Botões: tamanho=%d, espaçamento=%d, start_x=%d, y=%d", 
             BTN_SIZE, BTN_SPACING, BTN_START_X, BTN_Y);
    for (int i = 0; i < 5; i++) {
        int btn_x = BTN_START_X + i * (BTN_SIZE + BTN_SPACING);
        ESP_LOGI(TAG, "Criando botão %d em posição (%d, %d)...", i, btn_x, BTN_Y);
        // Criar botão diretamente na tela, sem container intermediário
        rating_buttons[i] = lv_button_create(question_screen);
        
        // Remover todos os estilos padrão antes de definir tamanho/posição
        lv_obj_remove_style_all(rating_buttons[i]);
        
        // Tamanho fixo e posição absoluta
        lv_obj_set_size(rating_buttons[i], BTN_SIZE, BTN_SIZE);
        lv_obj_set_pos(rating_buttons[i], btn_x, BTN_Y);
        
        // Estilo mínimo: apenas cor de fundo e borda
        lv_obj_set_style_bg_color(rating_buttons[i], RATING_COLORS[i], 0);
        lv_obj_set_style_bg_opa(rating_buttons[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(rating_buttons[i], BTN_SIZE / 2, 0); // Círculo perfeito
        
        // Borda simples
        lv_obj_set_style_border_width(rating_buttons[i], 2, 0);
        lv_obj_set_style_border_color(rating_buttons[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_opa(rating_buttons[i], LV_OPA_COVER, 0);
        
        // Garantir que o botão seja clicável
        lv_obj_clear_flag(rating_buttons[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(rating_buttons[i], LV_OBJ_FLAG_CLICKABLE);
        
        // Label com número
        lv_obj_t *btn_label = lv_label_create(rating_buttons[i]);
        lv_label_set_text(btn_label, RATING_NUMBERS[i]);
        lv_obj_center(btn_label);
        // Usar fonte Roboto customizada
        lv_obj_set_style_text_font(btn_label, TITLE_FONT, 0);
        lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
        // Adicionar padding ao botão para evitar corte do texto
        lv_obj_set_style_pad_all(rating_buttons[i], 4, 0);
        
        // Callback apenas para eventos de clique
        lv_obj_add_event_cb(rating_buttons[i], rating_button_cb, LV_EVENT_CLICKED,
                           reinterpret_cast<void*>(static_cast<intptr_t>(i + 1)));
        
        ESP_LOGI(TAG, "Botão %d criado: %p, posição (%d,%d), tamanho %dx%d", 
                 i, rating_buttons[i], btn_x, BTN_Y, BTN_SIZE, BTN_SIZE);
        
        // Invalidar cada botão para garantir renderização
        lv_obj_invalidate(rating_buttons[i]);
    }
    
    // Invalidar o label também
    lv_obj_invalidate(question_label);
    
    // Invalidar toda a tela para garantir renderização completa
    lv_obj_invalidate(question_screen);
    
    // Garantir que o layout seja atualizado antes do refresh
    lv_obj_update_layout(question_screen);
    
    ESP_LOGI(TAG, "Tela de pergunta criada: %p, Label: %p, Botões: %p %p %p %p %p", 
             question_screen, question_label,
             rating_buttons[0], rating_buttons[1], rating_buttons[2], 
             rating_buttons[3], rating_buttons[4]);
    
    lvgl_unlock();
    ESP_LOGI(TAG, "create_question_screen() concluído");
    
    // Não atualizar status WiFi aqui - será atualizado automaticamente pelo update() periodicamente
    // Isso evita chamadas no contexto de eventos WiFi que podem causar stack overflow
}

void create_thank_you_screen() {
    if (thank_you_screen != nullptr) {
        lv_obj_del(thank_you_screen);
    }
    
    thank_you_screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(thank_you_screen);
    lv_obj_set_style_bg_color(thank_you_screen, lv_color_hex(0xFFFFFF), 0); // Fundo branco limpo
    lv_obj_set_style_bg_opa(thank_you_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(thank_you_screen, 20, 0);
    lv_obj_set_style_pad_ver(thank_you_screen, 36, 0);
    lv_obj_clear_flag(thank_you_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Mensagem de agradecimento
    thank_you_label = lv_label_create(thank_you_screen);
    lv_label_set_text(thank_you_label, "Obrigado!");
    lv_obj_set_style_text_align(thank_you_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(thank_you_label, lv_color_hex(0x000000), 0); // Texto preto
    lv_obj_set_style_text_font(thank_you_label, TITLE_FONT, 0);
    // Adicionar padding vertical ao label para evitar corte do texto
    lv_obj_set_style_pad_top(thank_you_label, 4, 0);
    lv_obj_set_style_pad_bottom(thank_you_label, 4, 0);
    lv_obj_align(thank_you_label, LV_ALIGN_TOP_MID, 0, 0);
    
    thank_you_summary = lv_label_create(thank_you_screen);
    lv_label_set_text_fmt(thank_you_summary,
                          "Você registrou %d de 5 (%s).",
                          selected_rating,
                          RATING_MESSAGES[selected_rating - 1]);
    lv_obj_set_style_text_align(thank_you_summary, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(thank_you_summary, lv_color_hex(0x000000), 0); // Texto preto
    lv_obj_set_style_text_font(thank_you_summary, CAPTION_FONT, 0);
    lv_obj_set_width(thank_you_summary, LV_PCT(90));
    lv_label_set_long_mode(thank_you_summary, LV_LABEL_LONG_WRAP);
    // Adicionar padding vertical ao label para evitar corte do texto
    lv_obj_set_style_pad_top(thank_you_summary, 4, 0);
    lv_obj_set_style_pad_bottom(thank_you_summary, 4, 0);
    lv_obj_align(thank_you_summary, LV_ALIGN_TOP_MID, 0, 36);
    
}

void create_configuration_screen() {
    if (configuration_screen != nullptr) {
        lv_obj_del(configuration_screen);
    }
    
    configuration_screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(configuration_screen);
    lv_obj_set_style_bg_color(configuration_screen, lv_color_hex(0xF5F5F5), 0); // Fundo cinza muito claro
    lv_obj_set_style_bg_opa(configuration_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(configuration_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Container principal com layout flex
    lv_obj_t *main_cont = lv_obj_create(configuration_screen);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(main_cont, 10, 0);
    lv_obj_set_style_pad_row(main_cont, 10, 0);

    // Título
    lv_obj_t *title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, "Configurações");
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(title_label, TITLE_FONT, 0);
    
    // Container para os ícones (Grid horizontal)
    lv_obj_t *icons_cont = lv_obj_create(main_cont);
    lv_obj_remove_style_all(icons_cont);
    lv_obj_set_width(icons_cont, LV_PCT(100));
    lv_obj_set_flex_grow(icons_cont, 1); // Ocupar espaço disponível verticalmente
    lv_obj_set_flex_flow(icons_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(icons_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(icons_cont, 5, 0);

    // Estilo para botões de ícone (cards quadrados)
    static lv_style_t style_icon_btn;
    static bool style_icon_init = false;
    if (!style_icon_init) {
        lv_style_init(&style_icon_btn);
        lv_style_set_width(&style_icon_btn, 85); // Cabem 3 em 320px com espaço (85*3 = 255)
        lv_style_set_height(&style_icon_btn, 85);
        lv_style_set_bg_color(&style_icon_btn, lv_color_white());
        lv_style_set_bg_opa(&style_icon_btn, LV_OPA_COVER);
        lv_style_set_radius(&style_icon_btn, 12);
        lv_style_set_shadow_width(&style_icon_btn, 15);
        lv_style_set_shadow_color(&style_icon_btn, lv_color_hex(0x000000));
        lv_style_set_shadow_opa(&style_icon_btn, 20); // Suave
        lv_style_set_shadow_offset_y(&style_icon_btn, 4);
        
        // Layout interno: Flex Column (Ícone em cima, Texto embaixo)
        lv_style_set_layout(&style_icon_btn, LV_LAYOUT_FLEX);
        lv_style_set_flex_flow(&style_icon_btn, LV_FLEX_FLOW_COLUMN);
        lv_style_set_flex_main_place(&style_icon_btn, LV_FLEX_ALIGN_CENTER);
        lv_style_set_flex_cross_place(&style_icon_btn, LV_FLEX_ALIGN_CENTER);
        lv_style_set_pad_all(&style_icon_btn, 5);
        lv_style_set_pad_row(&style_icon_btn, 5);
        
        style_icon_init = true;
    }

    // Helper lambda para criar botão de ícone
    auto create_icon_btn = [&](lv_obj_t *parent, const char* icon, const char* text, lv_color_t icon_color, lv_event_cb_t cb) {
        lv_obj_t *btn = lv_button_create(parent);
        lv_obj_add_style(btn, &style_icon_btn, 0);
        lv_obj_set_style_pad_all(btn, 8, 0);
        lv_obj_set_style_pad_row(btn, 6, 0);
        lv_obj_set_size(btn, 85, 85);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        
        // Efeito de clique
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xF0F0F0), LV_STATE_PRESSED);
        // Leve deslocamento ao clicar para efeito tátil
        lv_obj_set_style_translate_y(btn, 2, LV_STATE_PRESSED);

        // Ícone grande
        lv_obj_t *lbl_icon = lv_label_create(btn);
        lv_obj_set_width(lbl_icon, LV_PCT(100));
        lv_label_set_text(lbl_icon, icon);
        lv_obj_set_style_text_font(lbl_icon, &lv_font_montserrat_20, 0); // Se tiver 24 ou 28 seria melhor, mas 20 ok
        lv_obj_set_style_text_align(lbl_icon, LV_TEXT_ALIGN_CENTER, 0);
        // Escalar o ícone se possível ou apenas mudar cor
        lv_obj_set_style_text_color(lbl_icon, icon_color, 0);

        // Texto
        lv_obj_t *lbl_text = lv_label_create(btn);
        lv_obj_set_width(lbl_text, LV_PCT(100));
        lv_label_set_text(lbl_text, text);
        lv_obj_set_style_text_font(lbl_text, TEXT_FONT, 0);
        lv_obj_set_style_text_color(lbl_text, lv_color_hex(0x555555), 0);
        lv_obj_set_style_text_align(lbl_text, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
        return btn;
    };

    // Botão WiFi
    create_icon_btn(icons_cont, LV_SYMBOL_WIFI, "WiFi", lv_color_hex(0x2196F3), [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            ESP_LOGI(TAG, "Abrindo configuração WiFi...");
            current_state = AppState::WIFI_CONFIG;
            ui::screens::on_back_callback = show_configuration_screen;
            ui::screens::show_wifi_config_screen();
        }
    });
    
    // Botão Brilho
    create_icon_btn(icons_cont, LV_SYMBOL_EYE_OPEN, "Brilho", lv_color_hex(0xFFC107), [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            ESP_LOGI(TAG, "Abrindo configuração de brilho...");
            current_state = AppState::BRIGHTNESS_CONFIG;
            ui::screens::brightness_on_back_callback = show_configuration_screen;
            ui::screens::show_brightness_screen();
        }
    });
    
    // Botão Calibração
    create_icon_btn(icons_cont, LV_SYMBOL_SETTINGS, "Calibrar", lv_color_hex(0x607D8B), config_calibrate_button_cb);

    // Botão de voltar
    lv_obj_t *back_button = lv_button_create(main_cont);
    lv_obj_set_size(back_button, LV_PCT(50), 40);
    lv_obj_set_style_bg_color(back_button, lv_color_hex(0xF44336), 0); // Vermelho suave
    lv_obj_set_style_bg_opa(back_button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(back_button, 20, 0);
    lv_obj_set_style_shadow_width(back_button, 0, 0);
    
    lv_obj_t *back_label = lv_label_create(back_button);
    lv_label_set_text(back_label, "Voltar");
    lv_obj_center(back_label);
    lv_obj_set_style_text_font(back_label, TEXT_FONT, 0);
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    
    lv_obj_add_event_cb(back_button, config_back_button_cb, LV_EVENT_CLICKED, nullptr);
}

void show_configuration_screen() {
    current_state = AppState::CONFIGURATION;
    
    lvgl_lock();
    
    // Sempre recriar a tela para garantir que os valores estejam atualizados
    if (configuration_screen != nullptr) {
        lv_obj_del(configuration_screen);
        configuration_screen = nullptr;
    }
    
    create_configuration_screen();
    
    lv_screen_load(configuration_screen);
    // Invalidar a tela para forçar refresh no próximo ciclo do timer handler
    // Não usar lv_refr_now() aqui para evitar stack overflow na task lvgl_timer
    lv_obj_invalidate(configuration_screen);
    
    lvgl_unlock();
}

void show_thank_you_screen() {
    current_state = AppState::THANK_YOU;
    thank_you_return_pending = true;
    thank_you_return_counter = 0;
    
    lvgl_lock();
    
    if (thank_you_screen == nullptr) {
        create_thank_you_screen();
    }
    
    if (thank_you_summary != nullptr) {
        lv_label_set_text_fmt(thank_you_summary,
                              "Você registrou %d de 5 (%s).",
                              selected_rating,
                              RATING_MESSAGES[selected_rating - 1]);
    }
    
    lv_screen_load(thank_you_screen);
    // Invalidar a tela para forçar refresh no próximo ciclo do timer handler
    // Não usar lv_refr_now() aqui para evitar stack overflow na task lvgl_timer
    lv_obj_invalidate(thank_you_screen);
    
    lvgl_unlock();
}

void show_question_screen() {
    ESP_LOGI(TAG, "show_question_screen() chamado");
    current_state = AppState::QUESTION;
    selected_rating = 0;
    thank_you_return_pending = false;
    thank_you_return_counter = 0;
    
    // Sempre recriar a tela para garantir estado limpo e evitar problemas de UI bagunçada
    // especialmente quando voltamos das configurações
    if (question_screen != nullptr) {
        ESP_LOGI(TAG, "Deletando tela existente para recriar...");
        lvgl_lock();
        lv_obj_del(question_screen);
        question_screen = nullptr;
        question_label = nullptr;
        settings_button = nullptr;
        wifi_status_icon = nullptr;
        // Limpar referências dos botões
        for (int i = 0; i < 5; i++) {
            rating_buttons[i] = nullptr;
        }
        lvgl_unlock();
    }
    
    // Criar nova tela limpa (create_question_screen() já faz lock internamente)
    create_question_screen();
}

static void update_wifi_status_icon() {
    if (wifi_status_icon == nullptr) {
        return;
    }
    
    // Verificar estado WiFi (operação leve, não precisa de lock)
    auto& wifi = wifi::WiFiManager::instance();
    bool connected = wifi.is_connected();
    
    // Atualizar UI em task separada para evitar stack overflow no contexto de eventos
    if (connected != wifi_status_last_connected) {
        wifi_status_update_pending = true;
        wifi_status_last_connected = connected;
        
        // Se WiFi acabou de conectar, criar task para testar Supabase
        if (connected) {
            ESP_LOGI(TAG, "WiFi conectado - verificando Supabase...");
            // Criar task separada para testar Supabase (evita stack overflow)
            xTaskCreate([](void* arg) {
                auto& supabase = supabase::SupabaseDriver::instance();
                if (supabase.is_configured()) {
                    // Testar conexão com Supabase
                    esp_err_t test_err = supabase.test_connection();
                    if (test_err == ESP_OK) {
                        ESP_LOGI(TAG, "Conexão com Supabase verificada com sucesso!");
                    } else {
                        ESP_LOGW(TAG, "Teste de conexão Supabase falhou: %s", esp_err_to_name(test_err));
                    }
                }
                vTaskDelete(nullptr);
            }, "supabase_test", 8192, nullptr, 5, nullptr);
        }
    }
    
    // Atualizar UI apenas se houver mudança pendente
    if (wifi_status_update_pending) {
        // Criar task separada para atualizar UI (evita stack overflow)
        xTaskCreate([](void* arg) {
            lvgl_lock();
            
            // Encontrar o label dentro do botão (primeiro filho)
            lv_obj_t* wifi_label = lv_obj_get_child(wifi_status_icon, 0);
            if (wifi_label != nullptr) {
                auto& wifi = wifi::WiFiManager::instance();
                bool connected = wifi.is_connected();
                
                // Atualizar cor: verde se conectado, vermelho se desconectado
                if (connected) {
                    lv_obj_set_style_text_color(wifi_label, lv_color_hex(0x00FF00), 0); // Verde
                } else {
                    lv_obj_set_style_text_color(wifi_label, lv_color_hex(0xFF0000), 0); // Vermelho
                }
                lv_obj_invalidate(wifi_label);
            }
            
            lv_obj_invalidate(wifi_status_icon);
            lvgl_unlock();
            
            vTaskDelete(nullptr);
        }, "wifi_ui_update", 2048, nullptr, 1, nullptr);
        
        wifi_status_update_pending = false;
    }
}

void update() {
    // Processar transição de tela pendente (feito de forma assíncrona para evitar problemas de contexto)
    if (pending_screen_transition && current_state == AppState::QUESTION) {
        transition_delay_counter++;
        // Aguardar 5 ciclos (500ms) antes de fazer a transição para garantir que o LVGL processou o evento
        if (transition_delay_counter >= 5) {
            pending_screen_transition = false;
            transition_delay_counter = 0;
            show_thank_you_screen();
        }
    }
    
    // Atualizar ícone WiFi periodicamente (a cada 10 ciclos = ~1 segundo)
    static uint32_t wifi_update_counter = 0;
    wifi_update_counter++;
    if (wifi_update_counter >= 10 && current_state == AppState::QUESTION) {
        wifi_update_counter = 0;
        update_wifi_status_icon();
    }
    
    // Retornar automaticamente para a tela de avaliações após agradecer
    if (current_state == AppState::THANK_YOU && thank_you_return_pending) {
        thank_you_return_counter++;
        if (thank_you_return_counter >= THANK_YOU_RETURN_DELAY_CYCLES) {
            thank_you_return_pending = false;
            thank_you_return_counter = 0;
            show_question_screen();
        }
    }
}

} // namespace

namespace ui {

void init(lv_display_t *display) {
    ESP_LOGI(TAG, "=== INICIANDO UI ===");
    
    if (display == nullptr) {
        ESP_LOGE(TAG, "Display LVGL inválido - display é nullptr!");
        return;
    }

    ESP_LOGI(TAG, "Display recebido: %p", display);
    display_handle = display;
    
    ESP_LOGI(TAG, "Definindo display padrão...");
    // Definir display padrão (não precisa de lock para isso)
    lvgl_lock();
    lv_display_set_default(display);
    lvgl_unlock();
    
    // Inicializar WiFi Manager
    auto& wifi = wifi::WiFiManager::instance();
    wifi.init();
    
    // Inicializar Supabase Driver
    auto& supabase = supabase::SupabaseDriver::instance();
    esp_err_t supabase_init_err = supabase.init();
    if (supabase_init_err == ESP_OK) {
        ESP_LOGI(TAG, "Supabase Driver inicializado");
        if (supabase.is_configured()) {
            ESP_LOGI(TAG, "Supabase configurado e pronto para uso");
        } else {
            ESP_LOGW(TAG, "Supabase não configurado - use set_credentials() para configurar");
        }
    } else {
        ESP_LOGW(TAG, "Erro ao inicializar Supabase Driver: %s", esp_err_to_name(supabase_init_err));
    }
    
    auto &driver = DisplayDriver::instance();
    if (driver.has_custom_calibration()) {
        ESP_LOGI(TAG, "Calibração existente detectada - pulando fluxo de calibração");
        current_state = AppState::QUESTION;
        show_question_screen();
    } else {
        ESP_LOGI(TAG, "Iniciando fluxo de calibração...");
        start_calibration();
    }
    
    ESP_LOGI(TAG, "=== UI INICIALIZADA COM SUCESSO ===");
    ESP_LOGI(TAG, "UI de pesquisa de satisfação inicializada");
}

void update() {
    ::update(); // Chamar função do namespace anônimo
}

int get_current_rating() {
    return selected_rating;
}

} // namespace ui
