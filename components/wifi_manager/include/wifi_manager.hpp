#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"  // Para wifi_ap_record_t e wifi_auth_mode_t
#include <cstdint>
#include <cstring>

namespace wifi {

struct WiFiConfig {
    char ssid[33];      // SSID máximo 32 caracteres + null terminator
    char password[65];  // Senha WiFi máximo 64 caracteres + null terminator
};

class WiFiManager {
public:
    static WiFiManager& instance();
    
    // Inicializar WiFi Manager
    esp_err_t init();
    
    // Configurar credenciais WiFi e conectar
    esp_err_t connect(const char* ssid, const char* password);
    
    // Desconectar WiFi
    esp_err_t disconnect();
    
    // Verificar se está conectado
    bool is_connected() const { return connected_; }
    
    // Obter SSID atual
    const char* get_ssid() const { return config_.ssid; }
    
    // Obter IP atual (retorna nullptr se não conectado)
    const char* get_ip() const { return connected_ ? ip_address_ : nullptr; }
    
    // Carregar credenciais do NVS
    esp_err_t load_credentials();
    
    // Salvar credenciais no NVS
    esp_err_t save_credentials();
    
    // Obter configuração atual (read-only)
    const WiFiConfig& config() const { return config_; }
    
    // Estrutura para armazenar informações de uma rede WiFi
    struct WiFiAP {
        char ssid[33];
        int8_t rssi;
        wifi_auth_mode_t authmode;
    };
    
    // Escanear redes WiFi disponíveis
    // Retorna número de redes encontradas, ou negativo em caso de erro
    // ap_list deve ter espaço para max_aps elementos
    int scan(wifi_ap_record_t* ap_list, uint16_t max_aps);

private:
    WiFiManager() = default;
    ~WiFiManager() = default;
    WiFiManager(const WiFiManager&) = delete;
    WiFiManager& operator=(const WiFiManager&) = delete;
    
    bool initialized_ = false;
    bool connected_ = false;
    WiFiConfig config_ = {};
    char ip_address_[16] = {}; // IPv4: "xxx.xxx.xxx.xxx\0"
    
    static constexpr const char* NVS_NAMESPACE = "wifi";
    static constexpr const char* NVS_KEY_SSID = "ssid";
    static constexpr const char* NVS_KEY_PASSWORD = "password";
    
    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data);
};

} // namespace wifi

