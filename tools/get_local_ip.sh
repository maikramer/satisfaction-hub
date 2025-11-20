#!/bin/bash
# Script para obter IP local da máquina (útil para configurar OTA server)

echo "IPs disponíveis na rede local:"
echo ""

# Linux
if command -v ip &> /dev/null; then
    ip addr show | grep "inet " | grep -v 127.0.0.1 | awk '{print $2}' | cut -d/ -f1
elif command -v hostname &> /dev/null; then
    hostname -I | awk '{for(i=1;i<=NF;i++) print $i}'
fi

# Mac
if [[ "$OSTYPE" == "darwin"* ]]; then
    ifconfig | grep "inet " | grep -v 127.0.0.1 | awk '{print $2}'
fi

echo ""
echo "Use um desses IPs para configurar o servidor OTA no ESP32"
