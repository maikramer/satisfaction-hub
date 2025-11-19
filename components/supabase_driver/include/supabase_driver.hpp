#pragma once

#include <cstdint>
#include <string>
#include "esp_err.h"

namespace supabase {

struct SupabaseConfig {
    char url[128];        // URL do projeto Supabase (ex: https://xxxxx.supabase.co)
    char api_key[512];    // API Key (anon key ou service role key) - Supabase anon keys são longas
    char table_name[64];  // Nome da tabela para armazenar avaliações
};

struct RatingData {
    int32_t rating;      // Avaliação de 1 a 5
    const char* message; // Mensagem associada (ex: "muito satisfeito")
    uint64_t timestamp;  // Timestamp Unix (opcional, pode ser gerado no servidor)
};

class SupabaseDriver {
public:
    static SupabaseDriver& instance();
    
    // Inicializar driver (deve ser chamado após WiFi estar conectado)
    esp_err_t init();
    
    // Configurar credenciais e salvar no NVS
    esp_err_t set_credentials(const char* url, const char* api_key, const char* table_name = "ratings");
    
    // Carregar credenciais do NVS
    esp_err_t load_credentials();
    
    // Verificar se credenciais estão configuradas
    bool is_configured() const { return configured_; }
    
    // Enviar avaliação para o Supabase
    esp_err_t submit_rating(const RatingData& data);
    
    // Testar conexão com Supabase
    esp_err_t test_connection();
    
    // Obter configuração atual (read-only)
    const SupabaseConfig& config() const { return config_; }

private:
    SupabaseDriver() = default;
    ~SupabaseDriver() = default;
    SupabaseDriver(const SupabaseDriver&) = delete;
    SupabaseDriver& operator=(const SupabaseDriver&) = delete;
    
    bool initialized_ = false;
    bool configured_ = false;
    SupabaseConfig config_ = {};
    
    static constexpr const char* NVS_NAMESPACE = "supabase";
    static constexpr const char* NVS_KEY_URL = "url";
    static constexpr const char* NVS_KEY_API_KEY = "api_key";
    static constexpr const char* NVS_KEY_TABLE = "table";
};

} // namespace supabase

