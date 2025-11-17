# Satisfaction Hub - ESP32 CYD Display

Aplicação de pesquisa de satisfação desenvolvida para ESP32 usando a placa **CYD (Cheap Yellow Display)** com display LCD ILI9341 de 320x240 pixels e biblioteca gráfica **LVGL**.

## 📋 Índice

- [Visão Geral](#visão-geral)
- [Hardware](#hardware)
- [Funcionalidades](#funcionalidades)
- [Estrutura do Projeto](#estrutura-do-projeto)
- [Instalação e Compilação](#instalação-e-compilação)
- [Documentação](#documentação)
- [Desenvolvimento](#desenvolvimento)

## 🎯 Visão Geral

O **Satisfaction Hub** é uma aplicação interativa de pesquisa de satisfação que permite aos usuários avaliar sua experiência através de uma interface touch screen intuitiva. A aplicação utiliza:

- **ESP32-CYD** (ESP32-2432S028R) como plataforma
- **Display ILI9341** 2.8" (320x240) para visualização
- **Touch Screen XPT2046** para interação
- **LVGL** para interface gráfica moderna
- **ESP-IDF v6.0** como framework

## 🔧 Hardware

### Especificações

- **Microcontrolador**: ESP32 (Dual-core, 160MHz)
- **Display**: ILI9341 TFT LCD 2.8" (320x240 pixels)
- **Touch**: XPT2046 Resistivo (12-bit)
- **Flash**: 2MB
- **RAM**: 520KB SRAM

### Pinout Rápido

**Display (SPI2_HOST)**:
- MOSI: GPIO 13
- CLK: GPIO 14
- CS: GPIO 15
- DC: GPIO 2
- RST: GPIO 4
- Backlight: GPIO 21

**Touch (Software SPI)**:
- MOSI: GPIO 32
- CLK: GPIO 25
- CS: GPIO 33
- MISO: GPIO 39

📖 **Documentação completa**: Veja [HARDWARE.md](HARDWARE.md) para detalhes técnicos completos.

## ✨ Funcionalidades

### Interface de Pesquisa

- **Tela de Pergunta**: Exibe pergunta de satisfação com 5 opções de avaliação
- **Botões de Avaliação**: 5 botões coloridos (1-5) com descrições
- **Feedback Visual**: Destaque do botão selecionado
- **Tela de Agradecimento**: Confirmação da avaliação registrada
- **Reiniciar**: Botão para iniciar nova avaliação

### Características Técnicas

- ✅ Display ILI9341 funcionando
- ✅ Touch screen XPT2046 via software SPI
- ✅ LVGL integrado e otimizado
- ✅ Interface responsiva e moderna
- ✅ Suporte completo a acentuação
- ✅ Thread-safe com `lvgl_port_lock/unlock`

## 📁 Estrutura do Projeto

```
satisfaction-hub/
├── components/
│   ├── display_driver/      # Driver do display ILI9341 + LVGL
│   │   ├── display_driver.cpp
│   │   └── include/
│   │       └── display_driver.hpp
│   ├── touch_bitbang/       # Driver touch XPT2046 (software SPI)
│   │   ├── Xpt2046Bitbang.cpp
│   │   └── include/
│   │       └── Xpt2046Bitbang.hpp
│   └── ui_driver/           # Interface gráfica LVGL
│       ├── ui_driver.cpp
│       └── include/
│           └── ui_driver.hpp
├── main/
│   ├── main.cpp             # Aplicação principal
│   ├── CMakeLists.txt
│   └── idf_component.yml    # Dependências gerenciadas
├── HARDWARE.md              # Documentação técnica do hardware
├── DEVELOPMENT_RULES.md     # Regras e padrões de desenvolvimento
├── README.md                # Este arquivo
└── sdkconfig                # Configurações ESP-IDF
```

## 🚀 Instalação e Compilação

### Pré-requisitos

- **ESP-IDF v6.0** ou superior instalado e configurado
- **Python 3.x**
- **Toolchain ESP32** configurado
- **Placa ESP32-CYD** conectada via USB

### Passos de Instalação

1. **Clone o repositório** (se aplicável):
   ```bash
   git clone <repository-url>
   cd satisfaction-hub
   ```

2. **Configure o ambiente ESP-IDF**:
   ```bash
   . $HOME/esp/v6.0/esp-idf/export.sh
   ```

3. **Configure o target**:
   ```bash
   idf.py set-target esp32
   ```

4. **Instale dependências** (gerenciadas automaticamente):
   ```bash
   idf.py reconfigure
   ```

5. **Compile o projeto**:
   ```bash
   idf.py build
   ```

6. **Conecte a placa** e identifique a porta:
   ```bash
   ls /dev/ttyUSB*  # Linux
   # ou
   ls /dev/tty.*    # macOS
   ```

7. **Flashe e monitore**:
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```
   (Substitua `/dev/ttyUSB0` pela porta correta)

## 📚 Documentação

### Documentos Disponíveis

- **[HARDWARE.md](HARDWARE.md)**: Documentação técnica completa do hardware
  - Especificações detalhadas
  - Pinout completo
  - Configurações e limitações
  - Referências técnicas

- **[DEVELOPMENT_RULES.md](DEVELOPMENT_RULES.md)**: Regras e padrões de desenvolvimento
  - Convenções de código
  - Estrutura de componentes
  - Gerenciamento de memória
  - Thread safety
  - Tratamento de erros

### Componentes Gerenciados

Este projeto utiliza os seguintes componentes do ESP-IDF Component Manager:

- `espressif/esp_lvgl_port` (^2.6.3) - Port do LVGL para ESP-IDF
- `espressif/esp_lcd_ili9341` (^2.0.0) - Driver do display ILI9341
- `lvgl__lvgl` (transitivo) - Biblioteca LVGL

## 💻 Desenvolvimento

### Arquitetura

O projeto segue uma arquitetura modular:

- **`display_driver`**: Encapsula toda inicialização do hardware (display, touch, LVGL)
- **`touch_bitbang`**: Implementa driver de software SPI para XPT2046
- **`ui_driver`**: Gerencia a interface gráfica LVGL (telas, eventos)

### Adicionando Novas Funcionalidades

1. **Nova Tela**: Adicione em `ui_driver.cpp` seguindo o padrão existente
2. **Novo Componente**: Crie em `components/` seguindo estrutura padrão
3. **Nova Dependência**: Adicione em `main/idf_component.yml`

### Debugging

- Use `ESP_LOGD()` para logs de debug
- Monitore stack usage: `uxTaskGetStackHighWaterMark()`
- Use `idf.py monitor` para ver logs em tempo real

## ⚙️ Configurações Importantes

### Stack Size
- **Main Task**: 9216 bytes (configurado em `sdkconfig`)
- Necessário para suportar LVGL e criação de objetos

### Touch Calibration
- Valores padrão em `display_driver.cpp`
- Ajuste conforme necessário para sua unidade

### Display Orientation
- Configurado para portrait (320x240)
- Inversão de eixos aplicada no touch

## 🐛 Troubleshooting

### Display não funciona
- Verifique conexões dos pinos SPI
- Confirme que backlight está ligado (GPIO 21)
- Verifique logs para erros de inicialização

### Touch não responde
- Verifique pinos do touch (GPIO 32, 25, 33, 39)
- Ajuste calibração se necessário
- Verifique logs para erros de comunicação

### Stack overflow
- Aumente `CONFIG_ESP_MAIN_TASK_STACK_SIZE` em `sdkconfig`
- Use `idf.py menuconfig` para configurar

## 📖 Recursos Úteis

- [ESP32-Cheap-Yellow-Display GitHub](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
- [CYD Pins Documentation](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/PINS.md)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [LVGL Documentation](https://docs.lvgl.io/)
- [ESP32 LCD Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd.html)

## 📝 Licença

Este projeto é um exemplo de código aberto para fins educacionais.

## 🤝 Contribuindo

Ao contribuir, por favor:

1. Siga as regras em [DEVELOPMENT_RULES.md](DEVELOPMENT_RULES.md)
2. Mantenha documentação atualizada
3. Teste no hardware real antes de submeter
4. Use commits descritivos

---

**Desenvolvido para ESP32-CYD com ESP-IDF v6.0 e LVGL**
