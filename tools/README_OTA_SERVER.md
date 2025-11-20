# Servidor OTA para Desenvolvimento

Este servidor Python permite testar atualizações OTA localmente durante o desenvolvimento.

## Requisitos

- Python 3.6+
- Firmware compilado (arquivo `.bin`)

## Uso Básico

### 1. Compilar o firmware

```bash
cd /home/maikeu/Embedded/ESP32/satisfaction-hub
idf.py build
```

### 2. Iniciar o servidor OTA

```bash
# Usando firmware encontrado automaticamente no diretório build/
python3 tools/ota_server.py --version 1.0.1

# Ou especificando o caminho do firmware manualmente
python3 tools/ota_server.py --firmware build/satisfaction-hub.bin --version 1.0.1

# Em outra porta (padrão: 10234)
python3 tools/ota_server.py --port 9000 --version 1.0.1
```

### 3. Configurar IP no ESP32

O código padrão usa `http://192.168.0.100:10234/ota`. Você precisa:

1. Descobrir o IP da sua máquina na rede local:
   ```bash
   # Linux/Mac
   ip addr show | grep "inet " | grep -v 127.0.0.1
   # ou
   hostname -I
   
   # Windows
   ipconfig
   ```

2. Atualizar a URL no código (`components/ui_driver/screens/ota_screen.cpp`):
   ```cpp
   const char* defaultUrl = otaUrl ? otaUrl : "http://SEU_IP_AQUI:10234/ota";
   ```

   Ou passar a URL ao chamar `show_ota_screen()`:
   ```cpp
   ::ui::screens::show_ota_screen("http://192.168.1.100:10234/ota");
   ```

## Endpoints

### Verificação de Atualização

```
GET http://SEU_IP:10234/ota?action=check&device_id=XXXXXX&current_version=X.X.X
```

Resposta JSON:
```json
{
  "update_available": true,
  "version": "1.0.1",
  "current_version": "1.0.0",
  "device_id": "XXXXXX",
  "firmware_size": 1234567
}
```

### Download do Firmware

```
GET http://SEU_IP:10234/ota?device_id=XXXXXX
```

Retorna o arquivo binário do firmware com headers:
- `Content-Type: application/octet-stream`
- `X-Firmware-Version: 1.0.1`
- `X-Firmware-MD5: <hash>`

## Exemplo de Uso Completo

1. **Compilar firmware versão 1.0.0:**
   ```bash
   idf.py build
   idf.py flash
   ```

2. **Iniciar servidor com nova versão:**
   ```bash
   python3 tools/ota_server.py --version 1.0.1
   ```

3. **No ESP32, acessar Configurações > Atualizar**

4. **O dispositivo verificará atualização e baixará se disponível**

## Opções Avançadas

```bash
# Usar host específico (útil para acessar de outros dispositivos)
python3 tools/ota_server.py --host 192.168.1.100 --port 10234 --version 1.0.1

# Especificar diretório build customizado
python3 tools/ota_server.py --build-dir /caminho/para/build --version 1.0.1
```

## Troubleshooting

### Erro: "Firmware não encontrado"
- Certifique-se de que compilou o projeto (`idf.py build`)
- Ou use `--firmware` para especificar o caminho manualmente

### ESP32 não consegue conectar
- Verifique se o IP está correto no código
- Verifique se o ESP32 está na mesma rede WiFi
- Verifique firewall da máquina (porta 10234 deve estar aberta)
- Tente usar `--host 0.0.0.0` para aceitar conexões de qualquer interface

### Atualização sempre disponível
- O servidor compara versões simplesmente. Se `current_version != server_version`, retorna `update_available: true`
- Para forçar atualização, mude a versão no servidor: `--version 1.0.2`

## Notas de Segurança

⚠️ **Este servidor é apenas para desenvolvimento!**

- Não use em produção
- Não tem autenticação
- Serve qualquer firmware para qualquer device_id
- Use HTTPS e autenticação em produção
