-- Migration: Criar tabela de avaliações (ratings)
-- Descrição: Tabela para armazenar avaliações de satisfação do cliente
-- Data: 2025-01-XX
-- Autor: Sistema de Satisfaction Hub

-- Criar tabela de avaliações
CREATE TABLE IF NOT EXISTS ratings (
  id BIGSERIAL PRIMARY KEY,
  rating INTEGER NOT NULL CHECK (rating >= 1 AND rating <= 5),
  message TEXT,
  timestamp BIGINT DEFAULT EXTRACT(EPOCH FROM NOW())::BIGINT,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Comentários nas colunas para documentação
COMMENT ON TABLE ratings IS 'Tabela para armazenar avaliações de satisfação dos clientes';
COMMENT ON COLUMN ratings.id IS 'ID único da avaliação (auto-incremento)';
COMMENT ON COLUMN ratings.rating IS 'Avaliação numérica de 1 a 5 (1=muito insatisfeito, 5=muito satisfeito)';
COMMENT ON COLUMN ratings.message IS 'Mensagem opcional associada à avaliação';
COMMENT ON COLUMN ratings.timestamp IS 'Timestamp Unix em segundos (usado pelo ESP32)';
COMMENT ON COLUMN ratings.created_at IS 'Data e hora de criação do registro no banco';

-- Criar índices para melhorar performance de consultas
CREATE INDEX IF NOT EXISTS idx_ratings_timestamp ON ratings(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_ratings_created_at ON ratings(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_ratings_rating ON ratings(rating);

-- Habilitar Row Level Security (RLS) para segurança
ALTER TABLE ratings ENABLE ROW LEVEL SECURITY;

-- Política para permitir inserção anônima (usando anon key)
-- Necessário para que o ESP32 possa inserir dados sem autenticação
CREATE POLICY "Allow anonymous inserts" ON ratings
  FOR INSERT
  TO anon
  WITH CHECK (true);

-- Política para permitir leitura pública (opcional)
-- Remova esta política se quiser restringir leitura apenas para usuários autenticados
CREATE POLICY "Allow public read" ON ratings
  FOR SELECT
  TO anon
  USING (true);

-- Política para permitir leitura para usuários autenticados
CREATE POLICY "Allow authenticated read" ON ratings
  FOR SELECT
  TO authenticated
  USING (true);

-- Função para atualizar timestamp quando não fornecido pelo cliente
-- Garante que sempre haverá um timestamp válido
CREATE OR REPLACE FUNCTION set_timestamp_if_null()
RETURNS TRIGGER AS $$
BEGIN
  IF NEW.timestamp IS NULL OR NEW.timestamp = 0 THEN
    NEW.timestamp := EXTRACT(EPOCH FROM NOW())::BIGINT;
  END IF;
  RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Trigger para aplicar função de timestamp automático
CREATE TRIGGER set_ratings_timestamp
  BEFORE INSERT ON ratings
  FOR EACH ROW
  EXECUTE FUNCTION set_timestamp_if_null();

-- View para estatísticas de avaliações (opcional, útil para dashboards)
CREATE OR REPLACE VIEW ratings_stats AS
SELECT 
  COUNT(*) as total_ratings,
  AVG(rating)::NUMERIC(3,2) as average_rating,
  COUNT(*) FILTER (WHERE rating = 5) as rating_5_count,
  COUNT(*) FILTER (WHERE rating = 4) as rating_4_count,
  COUNT(*) FILTER (WHERE rating = 3) as rating_3_count,
  COUNT(*) FILTER (WHERE rating = 2) as rating_2_count,
  COUNT(*) FILTER (WHERE rating = 1) as rating_1_count,
  COUNT(*) FILTER (WHERE rating >= 4) * 100.0 / NULLIF(COUNT(*), 0) as satisfaction_percentage
FROM ratings;

COMMENT ON VIEW ratings_stats IS 'Estatísticas agregadas das avaliações';

