# Migrations do Supabase

Este diretório contém as migrations SQL para configurar o banco de dados do Supabase.

## 📋 Como Aplicar as Migrations

### Método 1: Via SQL Editor do Supabase (Recomendado)

1. Acesse o [Supabase Dashboard](https://app.supabase.com)
2. Selecione seu projeto
3. Vá em **SQL Editor** no menu lateral
4. Abra o arquivo `001_create_ratings_table.sql`
5. Copie e cole todo o conteúdo no editor SQL
6. Clique em **Run** ou pressione `Ctrl+Enter` (ou `Cmd+Enter` no Mac)

### Método 2: Via Supabase CLI

Se você tem o Supabase CLI instalado:

```bash
# Aplicar migration específica
supabase db push --file migrations/001_create_ratings_table.sql

# Ou aplicar todas as migrations
supabase db push
```

## 📝 Migrations Disponíveis

### `001_create_ratings_table.sql`

Cria a tabela `ratings` para armazenar avaliações de satisfação.

**O que esta migration faz:**

- ✅ Cria a tabela `ratings` com os campos:
  - `id`: ID único (auto-incremento)
  - `rating`: Avaliação de 1 a 5 (obrigatório)
  - `message`: Mensagem opcional do cliente
  - `timestamp`: Timestamp Unix em segundos (BIGINT)
  - `created_at`: Data/hora de criação no banco (TIMESTAMPTZ)

- ✅ Cria índices para otimizar consultas:
  - Índice em `timestamp` (ordem decrescente)
  - Índice em `created_at` (ordem decrescente)
  - Índice em `rating`

- ✅ Configura Row Level Security (RLS):
  - Permite inserção anônima (para o ESP32)
  - Permite leitura pública (opcional)
  - Permite leitura para usuários autenticados

- ✅ Cria trigger para garantir timestamp válido:
  - Se `timestamp` for NULL ou 0, usa o timestamp atual

- ✅ Cria view de estatísticas (`ratings_stats`):
  - Total de avaliações
  - Média de avaliações
  - Contagem por rating (1 a 5)
  - Percentual de satisfação (ratings >= 4)

## 🔍 Verificando se a Migration Foi Aplicada

Após executar a migration, você pode verificar:

```sql
-- Verificar se a tabela existe
SELECT EXISTS (
  SELECT FROM information_schema.tables 
  WHERE table_schema = 'public' 
  AND table_name = 'ratings'
);

-- Ver estrutura da tabela
\d ratings

-- Ver políticas RLS
SELECT * FROM pg_policies WHERE tablename = 'ratings';

-- Ver índices
SELECT indexname, indexdef 
FROM pg_indexes 
WHERE tablename = 'ratings';

-- Testar inserção (deve funcionar com anon key)
INSERT INTO ratings (rating, message) 
VALUES (5, 'Teste de migration') 
RETURNING *;
```

## 🔄 Rollback (Desfazer Migration)

Se precisar desfazer a migration:

```sql
-- ⚠️ ATENÇÃO: Isso apagará todos os dados da tabela!
DROP VIEW IF EXISTS ratings_stats;
DROP TRIGGER IF EXISTS set_ratings_timestamp ON ratings;
DROP FUNCTION IF EXISTS set_timestamp_if_null();
DROP POLICY IF EXISTS "Allow anonymous inserts" ON ratings;
DROP POLICY IF EXISTS "Allow public read" ON ratings;
DROP POLICY IF EXISTS "Allow authenticated read" ON ratings;
DROP TABLE IF EXISTS ratings CASCADE;
```

## 📊 Usando a View de Estatísticas

Após aplicar a migration, você pode consultar estatísticas:

```sql
-- Ver estatísticas gerais
SELECT * FROM ratings_stats;

-- Exemplo de resultado:
-- total_ratings | average_rating | rating_5_count | rating_4_count | ...
-- 150          | 4.25          | 80             | 50             | ...
```

## 🔒 Segurança

A migration configura RLS (Row Level Security) que:

- ✅ Permite que o ESP32 insira dados usando a `anon` key
- ✅ Permite leitura pública (você pode remover esta política se necessário)
- ✅ Protege contra inserções maliciosas através de validação CHECK

**Recomendações adicionais:**

- Monitore o uso da API no dashboard do Supabase
- Considere adicionar rate limiting se necessário
- Revise as políticas RLS conforme suas necessidades de segurança

## 📚 Próximos Passos

Após aplicar a migration:

1. Configure as credenciais no ESP32 (veja `SUPABASE_SETUP.md`)
2. Teste a conexão usando `test_connection()`
3. Envie uma avaliação de teste
4. Verifique os dados no Table Editor do Supabase

