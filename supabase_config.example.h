/**
 * @file supabase_config.example.h
 * @brief Arquivo de exemplo para configuração do Supabase
 * 
 * INSTRUÇÕES:
 * 1. Copie este arquivo para supabase_config.h (não versionado)
 * 2. Preencha suas credenciais do Supabase
 * 3. O arquivo supabase_config.h está no .gitignore e não será commitado
 * 
 * Para obter suas credenciais:
 * 1. Acesse https://app.supabase.com
 * 2. Selecione seu projeto
 * 3. Vá em Settings > API
 * 4. Copie a URL do projeto e a anon/public key
 */

#ifndef SUPABASE_CONFIG_H
#define SUPABASE_CONFIG_H

// URL do seu projeto Supabase (ex: https://xxxxx.supabase.co)
#define SUPABASE_URL "https://seu-projeto.supabase.co"

// API Key (use a "anon" ou "public" key, NÃO a service_role key em produção)
#define SUPABASE_API_KEY "sua-api-key-aqui"

// Nome da tabela para armazenar avaliações (padrão: "ratings")
#define SUPABASE_TABLE_NAME "ratings"

#endif // SUPABASE_CONFIG_H

