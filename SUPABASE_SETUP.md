# Configuração do Supabase Driver

Este documento explica como configurar o driver Supabase para armazenar avaliações no seu banco de dados.

## 📋 Pré-requisitos

1. **Projeto Supabase criado**: Você precisa ter um projeto no Supabase (https://app.supabase.com)
2. **Tabela criada**: Crie uma tabela para armazenar as avaliações

### Criar tabela no Supabase

Execute este SQL no SQL Editor do Supabase:

```sql
CREATE TABLE IF NOT EXISTS ratings (
  id BIGSERIAL PRIMARY KEY,
  rating INTEGER NOT NULL CHECK (rating >= 1 AND rating <= 5),
  message TEXT,
  timestamp TIMESTAMPTZ DEFAULT NOW(),
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Criar índice para consultas por data
CREATE INDEX IF NOT EXISTS idx_ratings_timestamp ON ratings(timestamp DESC);

-- Habilitar Row Level Security (RLS) - opcional mas recomendado
ALTER TABLE ratings ENABLE ROW LEVEL SECURITY;

-- Política para permitir inserção anônima (usando anon key)
CREATE POLICY "Allow anonymous inserts" ON ratings
  FOR INSERT
  TO anon
  WITH CHECK (true);

-- Política para permitir leitura (opcional)
CREATE POLICY "Allow public read" ON ratings
  FOR SELECT
  TO anon
  USING (true);
```

## 🔐 Configuração Segura de Credenciais

### Método 1: Usando NVS (Recomendado - Mais Seguro)

O driver já está configurado para usar NVS (Non-Volatile Storage) do ESP-IDF. As credenciais são armazenadas na flash do ESP32 e **nunca** vão para o Git.

#### Passo 1: Obter suas credenciais do Supabase

1. Acesse https://app.supabase.com
2. Selecione seu projeto
3. Vá em **Settings** > **API**
4. Copie:
   - **Project URL** (ex: `https://xxxxx.supabase.co`)
   - **anon public** key (use esta, não a service_role)

#### Passo 2: Configurar no código

No seu código, após inicializar o WiFi, configure as credenciais:

```cpp
#include "supabase_driver.hpp"

// Após WiFi estar conectado
auto& supabase = supabase::SupabaseDriver::instance();
supabase.init();

esp_err_t err = supabase.set_credentials(
    "https://seu-projeto.supabase.co",  // URL do projeto
    "sua-anon-key-aqui",                 // anon/public key
    "ratings"                            // nome da tabela (opcional, padrão: "ratings")
);

if (err == ESP_OK) {
    ESP_LOGI("APP", "Credenciais do Supabase configuradas!");
} else {
    ESP_LOGE("APP", "Erro ao configurar Supabase: %s", esp_err_to_name(err));
}
```

**Importante**: As credenciais serão salvas na flash do ESP32 e persistirão mesmo após reset. Para alterar, chame `set_credentials()` novamente.

### Método 2: Usando arquivo de configuração (Alternativa)

Se preferir usar um arquivo de configuração (menos seguro, mas mais fácil para desenvolvimento):

1. Copie o arquivo de exemplo:
   ```bash
   cp supabase_config.example.h supabase_config.h
   ```

2. Edite `supabase_config.h` com suas credenciais:
   ```cpp
   #define SUPABASE_URL "https://seu-projeto.supabase.co"
   #define SUPABASE_API_KEY "sua-anon-key-aqui"
   #define SUPABASE_TABLE_NAME "ratings"
   ```

3. No código, inclua e use:
   ```cpp
   #ifdef SUPABASE_CONFIG_H
   #include "supabase_config.h"
   
   // Após WiFi conectado
   auto& supabase = supabase::SupabaseDriver::instance();
   supabase.init();
   supabase.set_credentials(SUPABASE_URL, SUPABASE_API_KEY, SUPABASE_TABLE_NAME);
   #endif
   ```

**⚠️ ATENÇÃO**: O arquivo `supabase_config.h` está no `.gitignore` e **não será commitado**. Nunca commite credenciais!

## 📤 Enviando Avaliações

Após configurar, você pode enviar avaliações assim:

```cpp
#include "supabase_driver.hpp"

// Quando o usuário selecionar uma avaliação
void on_rating_selected(int rating, const char* message) {
    auto& supabase = supabase::SupabaseDriver::instance();
    
    supabase::RatingData data = {
        .rating = rating,
        .message = message,
        .timestamp = 0  // 0 = usar timestamp do servidor
    };
    
    esp_err_t err = supabase.submit_rating(data);
    if (err == ESP_OK) {
        ESP_LOGI("APP", "Avaliação enviada com sucesso!");
    } else {
        ESP_LOGE("APP", "Erro ao enviar avaliação: %s", esp_err_to_name(err));
    }
}
```

## 🧪 Testando a Conexão

Para testar se a configuração está correta:

```cpp
auto& supabase = supabase::SupabaseDriver::instance();
supabase.init();

if (supabase.is_configured()) {
    esp_err_t err = supabase.test_connection();
    if (err == ESP_OK) {
        ESP_LOGI("APP", "Conexão com Supabase OK!");
    }
}
```

## 🔒 Segurança

### Boas Práticas

1. **Use sempre a anon/public key**, nunca a service_role key em dispositivos IoT
2. **Configure Row Level Security (RLS)** no Supabase para proteger seus dados
3. **Nunca commite credenciais** no Git
4. **Use HTTPS** (já habilitado por padrão)
5. **Monitore logs** para detectar tentativas de acesso não autorizadas

### O que está protegido

- ✅ Credenciais armazenadas em NVS (flash do ESP32)
- ✅ Arquivo `supabase_config.h` no `.gitignore`
- ✅ HTTPS habilitado por padrão
- ✅ Uso de anon key (sem privilégios administrativos)

### O que você precisa fazer

- ⚠️ Configurar RLS no Supabase
- ⚠️ Não compartilhar credenciais publicamente
- ⚠️ Monitorar uso da API no dashboard do Supabase

## 📊 Visualizando Dados

Após enviar avaliações, você pode visualizá-las:

1. No Supabase Dashboard: **Table Editor** > `ratings`
2. Via SQL:
   ```sql
   SELECT * FROM ratings ORDER BY timestamp DESC;
   ```
3. Via API REST:
   ```bash
   curl -X GET 'https://seu-projeto.supabase.co/rest/v1/ratings?select=*' \
     -H "apikey: sua-anon-key" \
     -H "Authorization: Bearer sua-anon-key"
   ```

## 🐛 Troubleshooting

### Erro: "Credenciais não configuradas"
- Certifique-se de chamar `set_credentials()` antes de usar
- Verifique se o NVS foi inicializado (`nvs_flash_init()`)

### Erro: "HTTP_INVALID_RESPONSE_STATUS"
- Verifique se a URL está correta
- Verifique se a API key está correta
- Verifique se a tabela existe no Supabase
- Verifique se RLS permite inserção anônima

### Erro: "WiFi não conectado"
- Certifique-se de que o WiFi está conectado antes de usar o driver
- O driver não gerencia WiFi, apenas usa a conexão existente

### Erro: "Timeout"
- Verifique sua conexão com a internet
- Aumente o timeout em `supabase_driver.cpp` se necessário

## 📚 Referências

- [Documentação Supabase REST API](https://supabase.com/docs/reference/javascript/introduction)
- [ESP-IDF HTTP Client](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_client.html)
- [ESP-IDF NVS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)

