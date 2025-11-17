#include "wifi_manager.hpp"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

namespace {
constexpr char TAG[] = "WiFiManager";
constexpr int WIFI_CONNECTED_BIT = BIT0;
constexpr int WIFI_FAIL_BIT = BIT1;
constexpr int WIFI_TIMEOUT_MS = 30000; // 30 segundos
}

namespace wifi {

static EventGroupHandle_t s_wifi_event_group = nullptr;
static WiFiManager* s_instance = nullptr;

void WiFiManager::wifi_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data) {
    WiFiManager* manager = static_cast<WiFiManager*>(arg);
    
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi iniciado, tentando conectar...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (manager) {
            manager->connected_ = false;
            memset(manager->ip_address_, 0, sizeof(manager->ip_address_));
        }
        esp_wifi_connect();
        ESP_LOGW(TAG, "WiFi desconectado, tentando reconectar...");
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        if (manager) {
            manager->connected_ = true;
            snprintf(manager->ip_address_, sizeof(manager->ip_address_), 
                    IPSTR, IP2STR(&event->ip_info.ip));
        }
        ESP_LOGI(TAG, "WiFi conectado! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

WiFiManager& WiFiManager::instance() {
    static WiFiManager manager;
    s_instance = &manager;
    return manager;
}

esp_err_t WiFiManager::init() {
    if (initialized_) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Inicializando WiFi Manager...");
    
    // Criar event group
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == nullptr) {
        ESP_LOGE(TAG, "Erro ao criar event group");
        return ESP_ERR_NO_MEM;
    }
    
    // Inicializar NVS (se ainda não foi inicializado)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Registrar event handlers
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                                &wifi_event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                                &wifi_event_handler, this));
    
    // Inicializar WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    initialized_ = true;
    ESP_LOGI(TAG, "WiFi Manager inicializado");
    
    // Tentar carregar credenciais e conectar
    if (load_credentials() == ESP_OK && strlen(config_.ssid) > 0) {
        ESP_LOGI(TAG, "Credenciais encontradas, tentando conectar...");
        return connect(config_.ssid, config_.password);
    }
    
    return ESP_OK;
}

esp_err_t WiFiManager::connect(const char* ssid, const char* password) {
    if (!initialized_) {
        ESP_LOGE(TAG, "WiFi Manager não inicializado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (ssid == nullptr || strlen(ssid) == 0) {
        ESP_LOGE(TAG, "SSID inválido");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copiar credenciais
    strncpy(config_.ssid, ssid, sizeof(config_.ssid) - 1);
    config_.ssid[sizeof(config_.ssid) - 1] = '\0';
    
    if (password != nullptr) {
        strncpy(config_.password, password, sizeof(config_.password) - 1);
        config_.password[sizeof(config_.password) - 1] = '\0';
    } else {
        config_.password[0] = '\0';
    }
    
    // Configurar WiFi
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, config_.ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (strlen(config_.password) > 0) {
        strncpy((char*)wifi_config.sta.password, config_.password, 
                sizeof(wifi_config.sta.password) - 1);
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Limpar bits anteriores
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    
    // Aguardar conexão (com timeout)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(WIFI_TIMEOUT_MS));
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado ao WiFi: %s", config_.ssid);
        save_credentials(); // Salvar credenciais no NVS
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Falha ao conectar ao WiFi");
        return ESP_FAIL;
    } else {
        ESP_LOGW(TAG, "Timeout ao conectar ao WiFi");
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t WiFiManager::disconnect() {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_wifi_disconnect();
    if (ret == ESP_OK) {
        connected_ = false;
        memset(ip_address_, 0, sizeof(ip_address_));
    }
    return ret;
}

esp_err_t WiFiManager::load_credentials() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }
    
    size_t required_size = sizeof(config_.ssid);
    err = nvs_get_str(handle, NVS_KEY_SSID, config_.ssid, &required_size);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }
    
    required_size = sizeof(config_.password);
    err = nvs_get_str(handle, NVS_KEY_PASSWORD, config_.password, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Senha não configurada (WiFi aberto)
        config_.password[0] = '\0';
        err = ESP_OK;
    }
    
    nvs_close(handle);
    return err;
}

esp_err_t WiFiManager::save_credentials() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    
    err = nvs_set_str(handle, NVS_KEY_SSID, config_.ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, NVS_KEY_PASSWORD, config_.password);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    
    nvs_close(handle);
    return err;
}

int WiFiManager::scan(wifi_ap_record_t* ap_list, uint16_t max_aps) {
    if (!initialized_) {
        ESP_LOGE(TAG, "WiFi Manager não inicializado");
        return -1;
    }
    
    if (ap_list == nullptr || max_aps == 0) {
        ESP_LOGE(TAG, "Parâmetros inválidos para scan");
        return -1;
    }
    
    ESP_LOGI(TAG, "Iniciando scan WiFi...");
    
    // Configurar scan passivo (mais rápido)
    wifi_scan_config_t scan_config = {};
    scan_config.ssid = nullptr;  // Escanear todas as redes
    scan_config.bssid = nullptr;
    scan_config.channel = 0;  // Todos os canais
    scan_config.show_hidden = false;
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan_config.scan_time.active.min = 100;  // 100ms mínimo
    scan_config.scan_time.active.max = 300;  // 300ms máximo
    
    // Iniciar scan
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);  // true = bloquear até completar
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao iniciar scan: %s", esp_err_to_name(ret));
        return -1;
    }
    
    // Aguardar conclusão do scan
    uint16_t ap_count = 0;
    ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao obter número de APs: %s", esp_err_to_name(ret));
        return -1;
    }
    
    ESP_LOGI(TAG, "Encontradas %d redes WiFi", ap_count);
    
    // Limitar ao máximo solicitado
    if (ap_count > max_aps) {
        ap_count = max_aps;
    }
    
    // Obter lista de APs
    ret = esp_wifi_scan_get_ap_records(&ap_count, ap_list);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao obter lista de APs: %s", esp_err_to_name(ret));
        return -1;
    }
    
    ESP_LOGI(TAG, "Scan concluído, retornando %d redes", ap_count);
    return static_cast<int>(ap_count);
}

} // namespace wifi

