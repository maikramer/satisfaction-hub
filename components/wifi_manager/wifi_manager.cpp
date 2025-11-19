#include "wifi_manager.hpp"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
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
constexpr int WIFI_AUTH_MAX_RETRY = 3;
constexpr int WIFI_AUTH_RETRY_DELAY_MS = 2000;
}

namespace wifi {

static EventGroupHandle_t s_wifi_event_group = nullptr;
static WiFiManager* s_instance = nullptr;
static bool s_auto_connect_enabled = false;  // Flag para controlar auto-connect
static bool s_netif_initialized = false;     // Flag para garantir que esp_netif só seja inicializado uma vez
static bool s_event_loop_created = false;    // Flag para garantir que event loop só seja criado uma vez
static int s_auth_retry_count = 0;           // Contador de tentativas após falha de autenticação

void WiFiManager::wifi_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data) {
    WiFiManager* manager = static_cast<WiFiManager*>(arg);
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA iniciado");
                // Só conectar automaticamente se a flag estiver habilitada
                // Criar task separada para fazer a configuração e conexão
                // (evita stack overflow no handler de eventos)
                       s_auth_retry_count = 0;
                if (s_auto_connect_enabled && manager != nullptr) {
                    // Criar task para fazer configuração e conexão fora do contexto do handler
                    xTaskCreate([](void* arg) {
                        WiFiManager* mgr = static_cast<WiFiManager*>(arg);
                        if (mgr == nullptr) {
                            vTaskDelete(nullptr);
                            return;
                        }
                        
                        // Obter credenciais usando método público
                        const WiFiConfig& wifi_cfg = mgr->config();
                        // Verificar se temos credenciais configuradas
                        if (strlen(wifi_cfg.ssid) > 0) {
                            // Pequeno delay para garantir que o WiFi está pronto
                            vTaskDelay(pdMS_TO_TICKS(200));
                            
                            // Configurar WiFi com as credenciais carregadas
                            wifi_config_t wifi_config = {};
                            memset(&wifi_config, 0, sizeof(wifi_config));
                            strncpy((char*)wifi_config.sta.ssid, wifi_cfg.ssid, sizeof(wifi_config.sta.ssid) - 1);
                            wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
                            
                            if (strlen(wifi_cfg.password) > 0) {
                                strncpy((char*)wifi_config.sta.password, wifi_cfg.password, 
                                        sizeof(wifi_config.sta.password) - 1);
                                wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
                            }
                            
                            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
                            wifi_config.sta.pmf_cfg.capable = true;
                            wifi_config.sta.pmf_cfg.required = false;
                            
                            esp_err_t config_ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
                            if (config_ret == ESP_OK) {
                                ESP_LOGI(TAG, "WiFi configurado - SSID: '%s'", wifi_config.sta.ssid);
                                // Pequeno delay antes de conectar
                                vTaskDelay(pdMS_TO_TICKS(100));
                                esp_err_t connect_ret = esp_wifi_connect();
                                if (connect_ret == ESP_OK) {
                                    ESP_LOGI(TAG, "Tentando conectar automaticamente...");
                                } else if (connect_ret == ESP_ERR_WIFI_CONN) {
                                    ESP_LOGD(TAG, "Já está conectando, ignorando nova tentativa");
                                } else {
                                    ESP_LOGW(TAG, "Erro ao iniciar conexão: %s", esp_err_to_name(connect_ret));
                                }
                            } else {
                                ESP_LOGE(TAG, "Erro ao configurar WiFi: %s", esp_err_to_name(config_ret));
                            }
                        } else {
                            ESP_LOGW(TAG, "Auto-connect habilitado mas sem credenciais configuradas");
                        }
                        
                        vTaskDelete(nullptr);
                    }, "wifi_auto_connect", 4096, manager, 5, nullptr);
                } else {
                    ESP_LOGI(TAG, "Auto-connect desabilitado (modo scan)");
                }
                break;
                
                   case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* disconnected = 
                    (wifi_event_sta_disconnected_t*) event_data;
                ESP_LOGW(TAG, "WiFi desconectado. Reason: %d", disconnected->reason);
                
                if (manager) {
                    manager->connected_ = false;
                    memset(manager->ip_address_, 0, sizeof(manager->ip_address_));
                }
                
                // Se estamos esperando conexão e houve falha, sinalizar
                       bool auth_related = (disconnected->reason == WIFI_REASON_AUTH_FAIL ||
                                            disconnected->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                                            disconnected->reason == WIFI_REASON_NO_AP_FOUND ||
                                            disconnected->reason == WIFI_REASON_AUTH_EXPIRE ||
                                            disconnected->reason == WIFI_REASON_AUTH_LEAVE);

                       if (auth_related) {
                           if (s_auth_retry_count < WIFI_AUTH_MAX_RETRY) {
                               s_auth_retry_count++;
                               int attempt = s_auth_retry_count;
                               ESP_LOGW(TAG,
                                        "Falha de autenticação (%d). Tentativa %d/%d - novo retry em %d ms",
                                        disconnected->reason,
                                        attempt,
                                        WIFI_AUTH_MAX_RETRY,
                                        WIFI_AUTH_RETRY_DELAY_MS);

                               xTaskCreate([](void* arg) {
                                   vTaskDelay(pdMS_TO_TICKS(WIFI_AUTH_RETRY_DELAY_MS));
                                   esp_err_t ret = esp_wifi_connect();
                                   if (ret == ESP_OK) {
                                       ESP_LOGI(TAG, "Reiniciando tentativa de conexão WiFi...");
                                   } else if (ret == ESP_ERR_WIFI_CONN) {
                                       ESP_LOGD(TAG, "Conexão já em andamento, aguardando resultado");
                                   } else {
                                       ESP_LOGE(TAG, "Erro ao reiniciar conexão: %s", esp_err_to_name(ret));
                                   }
                                   vTaskDelete(nullptr);
                               }, "wifi_auth_retry", 4096, nullptr, 5, nullptr);
                           } else {
                               ESP_LOGE(TAG, "Falha após %d tentativas de autenticação. Abortando.",
                                        WIFI_AUTH_MAX_RETRY);
                               s_auth_retry_count = 0;
                               if (s_wifi_event_group != nullptr) {
                                   xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                               }
                           }
                       } else {
                           s_auth_retry_count = 0;
                           ESP_LOGW(TAG, "Desconexão temporária (reason %d). Tentando reconectar...", disconnected->reason);
                           esp_wifi_connect();
                       }

                       if (s_wifi_event_group != nullptr) {
                           xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                       }
                       break;
                   }
            
            case WIFI_EVENT_STA_CONNECTED: {
                wifi_event_sta_connected_t* connected = 
                    (wifi_event_sta_connected_t*) event_data;
                       ESP_LOGI(TAG, "Conectado ao AP: %s, canal: %d",
                        connected->ssid, connected->channel);
                       s_auth_retry_count = 0;
                break;
            }
            
            default:
                ESP_LOGD(TAG, "Evento WiFi não tratado: %ld", event_id);
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        if (manager) {
            manager->connected_ = true;
            snprintf(manager->ip_address_, sizeof(manager->ip_address_), 
                    IPSTR, IP2STR(&event->ip_info.ip));
        }
                       ESP_LOGI(TAG, "WiFi conectado! IP: " IPSTR, IP2STR(&event->ip_info.ip));
                       s_auth_retry_count = 0;
        if (s_wifi_event_group != nullptr) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
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
    
    // Inicializar esp_netif (necessário para receber IP) - só uma vez
    if (!s_netif_initialized) {
        ESP_ERROR_CHECK(esp_netif_init());
        s_netif_initialized = true;
        ESP_LOGI(TAG, "esp_netif inicializado");
    }
    
    // Criar event loop - só uma vez (deve ser criado antes de inicializar WiFi)
    if (!s_event_loop_created) {
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        s_event_loop_created = true;
        ESP_LOGI(TAG, "Event loop criado");
    }
    
    // Inicializar WiFi ANTES de criar a interface de rede
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_LOGI(TAG, "WiFi inicializado");
    
    // Criar interface de rede WiFi STA - só uma vez (deve ser criada após esp_wifi_init)
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif == nullptr) {
        sta_netif = esp_netif_create_default_wifi_sta();
        if (sta_netif == nullptr) {
            ESP_LOGE(TAG, "Erro ao criar interface de rede WiFi STA");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Interface WiFi STA criada");
    } else {
        ESP_LOGI(TAG, "Interface WiFi STA já existe, reutilizando");
    }
    
    // Registrar event handlers (podem ser registrados múltiplas vezes, mas vamos evitar)
    static bool handlers_registered = false;
    if (!handlers_registered) {
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                                    &wifi_event_handler, this));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                                    &wifi_event_handler, this));
        handlers_registered = true;
        ESP_LOGI(TAG, "Event handlers registrados");
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "WiFi Manager inicializado");
    
    // Tentar carregar credenciais ANTES de iniciar WiFi
    // Isso garante que as credenciais estejam disponíveis quando o evento STA_START for disparado
    if (load_credentials() == ESP_OK && strlen(config_.ssid) > 0) {
        ESP_LOGI(TAG, "Credenciais encontradas: SSID '%s'", config_.ssid);
        // Habilitar auto-connect - o evento WIFI_EVENT_STA_START fará a conexão
        // Isso evita tentativas duplas de conexão
        s_auto_connect_enabled = true;
        ESP_LOGI(TAG, "Auto-connect habilitado - conexão será iniciada automaticamente");
    } else {
        // Se não há credenciais, manter auto-connect desabilitado
        s_auto_connect_enabled = false;
        ESP_LOGI(TAG, "Nenhuma credencial encontrada - auto-connect desabilitado");
    }
    
    // Iniciar WiFi - necessário para que o evento WIFI_EVENT_STA_START seja disparado
    // Isso permite que o auto-connect funcione quando há credenciais salvas
    // IMPORTANTE: Deve ser chamado DEPOIS de carregar credenciais e habilitar auto-connect
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao iniciar WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "WiFi iniciado - aguardando evento STA_START para auto-connect");
    
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
    
    ESP_LOGI(TAG, "Conectando ao WiFi - SSID: '%s', Senha: %s", 
             config_.ssid, strlen(config_.password) > 0 ? "***" : "(vazia)");
    
    // Habilitar auto-connect antes de iniciar WiFi
    s_auto_connect_enabled = true;
    s_auth_retry_count = 0;
    
    // Verificar se já está conectando antes de desconectar
    wifi_mode_t mode;
    esp_err_t mode_ret = esp_wifi_get_mode(&mode);
    if (mode_ret == ESP_OK) {
        // Desconectar se já estiver conectado/conectando
        esp_wifi_disconnect();
        // Aguardar desconexão completar
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // Configurar WiFi
    wifi_config_t wifi_config = {};
    memset(&wifi_config, 0, sizeof(wifi_config));
    strncpy((char*)wifi_config.sta.ssid, config_.ssid, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    
    if (strlen(config_.password) > 0) {
        strncpy((char*)wifi_config.sta.password, config_.password, 
                sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
    }
    
    // Configurar autenticação - tentar detectar automaticamente
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    ESP_LOGI(TAG, "Configurando WiFi - SSID: '%s', Auth: WPA2_PSK", 
             wifi_config.sta.ssid);
    
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao configurar WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Garantir que WiFi está iniciado
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        // Se já estiver iniciado, esp_wifi_start() retorna ESP_OK
        // Outros erros são tratados como falha
        ESP_LOGW(TAG, "esp_wifi_start() retornou: %s (continuando)", esp_err_to_name(ret));
    }
    
    // Limpar bits anteriores
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    
    // Iniciar conexão
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao iniciar conexão WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Aguardando conexão (timeout: %d ms)...", WIFI_TIMEOUT_MS);
    
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
        ESP_LOGE(TAG, "Falha ao conectar ao WiFi (autenticação ou rede não encontrada)");
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
    
    // Desabilitar auto-connect durante o scan
    s_auto_connect_enabled = false;
    
    // Desconectar se estiver conectando/conectado para permitir scan
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Garantir que o WiFi esteja iniciado antes de fazer scan
    // Se não estiver iniciado, iniciar agora
    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK) {
        // Se já estiver iniciado, esp_wifi_start() retorna ESP_OK
        // Se houver outro erro, logar mas continuar (pode já estar iniciado)
        ESP_LOGW(TAG, "esp_wifi_start() retornou: %s (continuando mesmo assim)", esp_err_to_name(ret));
    }
    
    // Aguardar um pouco para o WiFi inicializar completamente (se necessário)
    vTaskDelay(pdMS_TO_TICKS(200));
    
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
    ret = esp_wifi_scan_start(&scan_config, true);  // true = bloquear até completar
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

