#include "supabase_driver.hpp"

#include <cstring>
#include <cstdint>
#include <cstdio>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "nvs.h"

// Incluir configuração se disponível (para desenvolvimento)
// O arquivo supabase_config.h está no .gitignore e pode não existir em todos os ambientes
#if __has_include("supabase_config.h")
#include "supabase_config.h"
#endif

namespace {
constexpr char TAG[] = "SupabaseDriver";

extern const uint8_t supabase_root_ca_pem_start[] asm("_binary_supabase_root_ca_pem_start");
extern const uint8_t supabase_root_ca_pem_end[] asm("_binary_supabase_root_ca_pem_end");
}

namespace supabase {

SupabaseDriver& SupabaseDriver::instance() {
    static SupabaseDriver driver;
    return driver;
}

esp_err_t SupabaseDriver::init() {
    if (initialized_) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Inicializando driver Supabase...");
    
    // Tentar carregar credenciais do NVS primeiro
    esp_err_t ret = load_credentials();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Credenciais carregadas do NVS");
        configured_ = true;
    } else {
        // Se não encontrou no NVS, tentar usar configuração de compilação (se disponível)
        #if defined(SUPABASE_URL) && defined(SUPABASE_ANON_KEY)
        ESP_LOGI(TAG, "Usando credenciais de configuração de compilação");
        strncpy(config_.url, SUPABASE_URL, sizeof(config_.url) - 1);
        config_.url[sizeof(config_.url) - 1] = '\0';
        strncpy(config_.api_key, SUPABASE_ANON_KEY, sizeof(config_.api_key) - 1);
        config_.api_key[sizeof(config_.api_key) - 1] = '\0';
        #ifdef SUPABASE_TABLE_NAME
        strncpy(config_.table_name, SUPABASE_TABLE_NAME, sizeof(config_.table_name) - 1);
        config_.table_name[sizeof(config_.table_name) - 1] = '\0';
        #else
        strncpy(config_.table_name, "ratings", sizeof(config_.table_name) - 1);
        config_.table_name[sizeof(config_.table_name) - 1] = '\0';
        #endif
        configured_ = true;
        #else
        ESP_LOGW(TAG, "Credenciais não encontradas. Use set_credentials() para configurar.");
        configured_ = false;
        #endif
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "Driver Supabase inicializado");
    return ESP_OK;
}

esp_err_t SupabaseDriver::set_credentials(const char* url, const char* api_key, const char* table_name) {
    if (url == nullptr || api_key == nullptr) {
        ESP_LOGE(TAG, "URL ou API key são nullptr");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(url) >= sizeof(config_.url) || strlen(api_key) >= sizeof(config_.api_key)) {
        ESP_LOGE(TAG, "URL ou API key muito longos");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copiar credenciais
    strncpy(config_.url, url, sizeof(config_.url) - 1);
    config_.url[sizeof(config_.url) - 1] = '\0';
    
    strncpy(config_.api_key, api_key, sizeof(config_.api_key) - 1);
    config_.api_key[sizeof(config_.api_key) - 1] = '\0';
    
    if (table_name != nullptr) {
        strncpy(config_.table_name, table_name, sizeof(config_.table_name) - 1);
        config_.table_name[sizeof(config_.table_name) - 1] = '\0';
    } else {
        strncpy(config_.table_name, "ratings", sizeof(config_.table_name) - 1);
        config_.table_name[sizeof(config_.table_name) - 1] = '\0';
    }
    
    // Salvar no NVS
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao abrir NVS: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_str(handle, NVS_KEY_URL, config_.url);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, NVS_KEY_API_KEY, config_.api_key);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, NVS_KEY_TABLE, config_.table_name);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    
    nvs_close(handle);
    
    if (err == ESP_OK) {
        configured_ = true;
        ESP_LOGI(TAG, "Credenciais salvas no NVS com sucesso");
        ESP_LOGI(TAG, "URL: %s", config_.url);
        ESP_LOGI(TAG, "Tabela: %s", config_.table_name);
        // Não logar a API key por segurança
    } else {
        ESP_LOGE(TAG, "Erro ao salvar credenciais no NVS: %s", esp_err_to_name(err));
    }
    
    return err;
}

esp_err_t SupabaseDriver::load_credentials() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }
    
    size_t required_size = sizeof(config_.url);
    err = nvs_get_str(handle, NVS_KEY_URL, config_.url, &required_size);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }
    
    required_size = sizeof(config_.api_key);
    err = nvs_get_str(handle, NVS_KEY_API_KEY, config_.api_key, &required_size);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }
    
    required_size = sizeof(config_.table_name);
    err = nvs_get_str(handle, NVS_KEY_TABLE, config_.table_name, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Tabela não configurada, usar padrão
        strncpy(config_.table_name, "ratings", sizeof(config_.table_name) - 1);
        config_.table_name[sizeof(config_.table_name) - 1] = '\0';
        err = ESP_OK;
    }
    
    nvs_close(handle);
    return err;
}

namespace {
esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
                if (evt->data) {
                    ESP_LOGD(TAG, "Response: %.*s", evt->data_len, (char*)evt->data);
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

} // namespace anônimo

esp_err_t SupabaseDriver::submit_rating(const RatingData& data) {
    if (!configured_) {
        ESP_LOGE(TAG, "Credenciais não configuradas. Use set_credentials() primeiro.");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Construir URL completa
    char url[256];
    snprintf(url, sizeof(url), "%s/rest/v1/%s", config_.url, config_.table_name);
    
    // Criar JSON payload manualmente (sem dependência externa)
    char json_string[512];
    int json_len;
    
    const char* safe_message = (data.message != nullptr) ? data.message : "";
    const char* safe_device_id = (data.device_id != nullptr) ? data.device_id : "";
    
    if (data.timestamp > 0) {
        json_len = snprintf(json_string, sizeof(json_string),
            "{\"rating\":%ld,\"message\":\"%s\",\"timestamp\":%llu,\"device_id\":\"%s\"}",
            (long)data.rating,
            safe_message,
            (unsigned long long)data.timestamp,
            safe_device_id);
    } else {
        json_len = snprintf(json_string, sizeof(json_string),
            "{\"rating\":%ld,\"message\":\"%s\",\"device_id\":\"%s\"}",
            (long)data.rating,
            safe_message,
            safe_device_id);
    }
    
    if (json_len < 0 || json_len >= (int)sizeof(json_string)) {
        ESP_LOGE(TAG, "Erro ao criar JSON: buffer muito pequeno");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Enviando avaliação para Supabase: %s", json_string);
    
    // Configurar cliente HTTP com certificado CA embedado
    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = http_event_handler;
    config.timeout_ms = 10000;
    config.cert_pem = reinterpret_cast<const char*>(supabase_root_ca_pem_start);
    config.cert_len = supabase_root_ca_pem_end - supabase_root_ca_pem_start;
    config.buffer_size = 1024;
    config.buffer_size_tx = 1024;
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(TAG, "Erro ao criar cliente HTTP");
        return ESP_ERR_NO_MEM;
    }
    
    // Configurar headers
    char auth_header[sizeof(config_.api_key) + 16];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", config_.api_key);
    
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "apikey", config_.api_key);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Prefer", "return=minimal");
    
    // Configurar dados POST
    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    
    // Executar requisição
    esp_err_t err = esp_http_client_perform(client);
    
    int status_code = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);
    
    if (err == ESP_OK) {
        if (status_code >= 200 && status_code < 300) {
            ESP_LOGI(TAG, "Avaliação enviada com sucesso! Status: %d", status_code);
        } else {
            ESP_LOGW(TAG, "Resposta HTTP: %d, Content-Length: %d", status_code, content_length);
            err = ESP_ERR_INVALID_RESPONSE;
        }
    } else {
        ESP_LOGE(TAG, "Erro ao executar requisição HTTP: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    
    return err;
}

esp_err_t SupabaseDriver::test_connection() {
    if (!configured_) {
        ESP_LOGE(TAG, "Credenciais não configuradas");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Fazer uma requisição GET simples para testar conexão
    char url[256];
    snprintf(url, sizeof(url), "%s/rest/v1/%s?select=count", config_.url, config_.table_name);
    
    // Configurar cliente HTTP com certificado CA embedado
    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = http_event_handler;
    config.timeout_ms = 5000;
    config.cert_pem = reinterpret_cast<const char*>(supabase_root_ca_pem_start);
    config.cert_len = supabase_root_ca_pem_end - supabase_root_ca_pem_start;
    config.buffer_size = 1024;
    config.buffer_size_tx = 1024;
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    
    char auth_header[sizeof(config_.api_key) + 16];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", config_.api_key);
    
    esp_http_client_set_method(client, HTTP_METHOD_HEAD);
    esp_http_client_set_header(client, "apikey", config_.api_key);
    esp_http_client_set_header(client, "Authorization", auth_header);
    
    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    
    esp_http_client_cleanup(client);
    
    if (err == ESP_OK && status_code >= 200 && status_code < 300) {
        ESP_LOGI(TAG, "Conexão com Supabase OK! Status: %d", status_code);
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Teste de conexão falhou. Status: %d, Erro: %s", status_code, esp_err_to_name(err));
        return ESP_FAIL;
    }
}

} // namespace supabase

