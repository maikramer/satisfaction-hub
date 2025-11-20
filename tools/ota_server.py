#!/usr/bin/env python3
"""
Servidor OTA simples para desenvolvimento
Serve atualizações de firmware para dispositivos ESP32 via HTTP/HTTPS
"""

import os
import sys
import json
import hashlib
import argparse
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import mimetypes

class OtaRequestHandler(BaseHTTPRequestHandler):
    """Handler para requisições OTA"""
    
    def __init__(self, *args, firmware_path=None, firmware_version=None, **kwargs):
        self.firmware_path = firmware_path
        self.firmware_version = firmware_version
        super().__init__(*args, **kwargs)
    
    def log_message(self, format, *args):
        """Override para melhorar logs"""
        print(f"[OTA Server] {format % args}")
    
    def do_GET(self):
        """Processa requisições GET"""
        parsed_path = urlparse(self.path)
        query_params = parse_qs(parsed_path.query)
        
        # Extrair parâmetros
        device_id = query_params.get('device_id', [None])[0]
        action = query_params.get('action', [None])[0]
        current_version = query_params.get('current_version', ['0.0.0'])[0]
        
        self.log_message(f"Requisição: {parsed_path.path} | device_id={device_id} | action={action} | current_version={current_version}")
        
        # Rota de verificação de atualização
        if action == 'check':
            self.handle_check_update(device_id, current_version)
        # Rota de download do firmware
        elif parsed_path.path == '/ota' or parsed_path.path == '/firmware.bin':
            self.handle_firmware_download(device_id)
        else:
            self.send_error(404, "Not Found")
    
    def handle_check_update(self, device_id, current_version):
        """Responde verificação de atualização"""
        if not self.firmware_path or not os.path.exists(self.firmware_path):
            response = {
                "update_available": False,
                "message": "Nenhum firmware disponível"
            }
        else:
            # Comparar versões (simples - pode ser melhorado)
            update_available = current_version != self.firmware_version
            
            response = {
                "update_available": update_available,
                "version": self.firmware_version,
                "current_version": current_version,
                "device_id": device_id,
                "firmware_size": os.path.getsize(self.firmware_path) if self.firmware_path else 0
            }
        
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(response).encode('utf-8'))
    
    def handle_firmware_download(self, device_id):
        """Serve o arquivo de firmware"""
        if not self.firmware_path or not os.path.exists(self.firmware_path):
            self.send_error(404, "Firmware não encontrado")
            return
        
        try:
            # Ler arquivo de firmware
            with open(self.firmware_path, 'rb') as f:
                firmware_data = f.read()
            
            # Calcular hash MD5 (opcional, para verificação)
            md5_hash = hashlib.md5(firmware_data).hexdigest()
            
            # Enviar resposta
            self.send_response(200)
            self.send_header('Content-Type', 'application/octet-stream')
            self.send_header('Content-Length', str(len(firmware_data)))
            self.send_header('Content-Disposition', f'attachment; filename="firmware_{self.firmware_version}.bin"')
            self.send_header('X-Firmware-Version', self.firmware_version)
            self.send_header('X-Firmware-MD5', md5_hash)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            
            self.wfile.write(firmware_data)
            self.log_message(f"Firmware enviado para device_id={device_id} | size={len(firmware_data)} bytes | version={self.firmware_version}")
            
        except Exception as e:
            self.log_message(f"Erro ao servir firmware: {e}")
            self.send_error(500, f"Erro interno: {e}")
    
    def do_OPTIONS(self):
        """Handle CORS preflight"""
        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()


def create_handler_class(firmware_path, firmware_version):
    """Factory para criar handler com parâmetros"""
    class Handler(OtaRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, firmware_path=firmware_path, firmware_version=firmware_version, **kwargs)
    return Handler


def find_firmware_bin(build_dir='build'):
    """Encontra o arquivo .bin do firmware no diretório build"""
    if not os.path.exists(build_dir):
        return None
    
    # Procurar por satisfaction-hub.bin ou *.bin no build
    bin_files = []
    for root, dirs, files in os.walk(build_dir):
        for file in files:
            if file.endswith('.bin') and 'bootloader' not in file and 'partition' not in file:
                bin_files.append(os.path.join(root, file))
    
    # Priorizar satisfaction-hub.bin
    for bin_file in bin_files:
        if 'satisfaction-hub.bin' in bin_file:
            return bin_file
    
    # Retornar o primeiro .bin encontrado
    return bin_files[0] if bin_files else None


def main():
    parser = argparse.ArgumentParser(description='Servidor OTA para desenvolvimento ESP32')
    parser.add_argument('--port', type=int, default=10234, help='Porta do servidor (padrão: 10234)')
    parser.add_argument('--firmware', type=str, help='Caminho para o arquivo .bin do firmware')
    parser.add_argument('--version', type=str, default='1.0.0', help='Versão do firmware (padrão: 1.0.0)')
    parser.add_argument('--build-dir', type=str, default='build', help='Diretório build para procurar firmware (padrão: build)')
    parser.add_argument('--host', type=str, default='0.0.0.0', help='Host para bind (padrão: 0.0.0.0)')
    
    args = parser.parse_args()
    
    # Determinar caminho do firmware
    firmware_path = args.firmware
    if not firmware_path:
        firmware_path = find_firmware_bin(args.build_dir)
        if firmware_path:
            print(f"[OTA Server] Firmware encontrado automaticamente: {firmware_path}")
        else:
            print("[OTA Server] AVISO: Nenhum firmware encontrado. Use --firmware para especificar.")
    else:
        if not os.path.exists(firmware_path):
            print(f"[OTA Server] ERRO: Arquivo de firmware não encontrado: {firmware_path}")
            sys.exit(1)
    
    if firmware_path:
        firmware_size = os.path.getsize(firmware_path)
        print(f"[OTA Server] Firmware: {firmware_path}")
        print(f"[OTA Server] Tamanho: {firmware_size} bytes ({firmware_size / 1024:.2f} KB)")
        print(f"[OTA Server] Versão: {args.version}")
    
    # Criar handler
    handler_class = create_handler_class(firmware_path, args.version)
    
    # Criar servidor
    server_address = (args.host, args.port)
    httpd = HTTPServer(server_address, handler_class)
    
    print(f"[OTA Server] Servidor iniciado em http://{args.host}:{args.port}")
    print(f"[OTA Server] Endpoint de verificação: http://{args.host}:{args.port}/ota?action=check&device_id=XXX&current_version=X.X.X")
    print(f"[OTA Server] Endpoint de download: http://{args.host}:{args.port}/ota?device_id=XXX")
    print("[OTA Server] Pressione Ctrl+C para parar")
    
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[OTA Server] Parando servidor...")
        httpd.shutdown()


if __name__ == '__main__':
    main()
