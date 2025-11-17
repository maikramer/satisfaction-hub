# Documentação de Hardware - Satisfaction Hub

## Visão Geral

Este projeto utiliza a placa **ESP32-CYD (Cheap Yellow Display)** modelo **ESP32-2432S028R**, uma placa de desenvolvimento com display TFT integrado e touch screen resistivo.

## Especificações Técnicas

### Microcontrolador
- **Chip**: ESP32 (Dual-core Xtensa LX6)
- **Frequência**: 160 MHz (configurável)
- **Flash**: 2MB (ou 4MB dependendo do modelo)
- **RAM**: 520KB SRAM
- **WiFi**: 802.11 b/g/n integrado
- **Bluetooth**: Bluetooth Classic + BLE

### Display LCD
- **Controlador**: ILI9341
- **Tamanho**: 2.8 polegadas
- **Resolução**: 320x240 pixels
- **Interface**: SPI (4-wire)
- **Cores**: 18-bit (262K cores)
- **Backlight**: LED controlado por GPIO

### Touch Screen
- **Controlador**: XPT2046
- **Tipo**: Resistivo (4-wire)
- **Resolução**: 12-bit (4096 níveis)
- **Interface**: SPI (software bit-banging)
- **Pressão**: Detectável (0-4095)

## Pinout Completo

### Display ILI9341 (SPI)

| Função | GPIO | Descrição | Direção | Notas |
|--------|------|-----------|---------|-------|
| **MOSI** | 13 | TFT_SDI (Serial Data In) | Output | HSPI MOSI |
| **CLK** | 14 | TFT_SCK (Serial Clock) | Output | HSPI CLK |
| **CS** | 15 | TFT_CS (Chip Select) | Output | Active Low |
| **DC** | 2 | TFT_RS/TFT_DC (Data/Command) | Output | Data=High, Cmd=Low |
| **RST** | 4 | Reset | Output | Active Low |
| **BL** | 21 | TFT_BL (Backlight) | Output | PWM ou Digital |
| **MISO** | 12 | TFT_SDO (Serial Data Out) | Input | Não usado neste projeto |

**SPI Bus**: SPI2_HOST (VSPI) - O SPI1_HOST está reservado para comunicação com a flash

**Clock**: 26 MHz (configurável, máximo recomendado: 40 MHz)

### Touch Screen XPT2046 (Software SPI)

| Função | GPIO | Descrição | Direção | Notas |
|--------|------|-----------|---------|-------|
| **MOSI** | 32 | XPT2046_MOSI | Output | Bit-banging |
| **CLK** | 25 | XPT2046_CLK | Output | Bit-banging |
| **CS** | 33 | XPT2046_CS | Output | Active Low |
| **MISO** | 39 | XPT2046_MISO | Input | Input-only pad (sem pull-up) |
| **IRQ** | 36 | XPT2046_IRQ (opcional) | Input | Não usado atualmente |

**Nota Importante**: O touch screen utiliza **software SPI (bit-banging)** porque:
- SPI1_HOST está em uso pela flash do sistema
- SPI2_HOST está em uso pelo display com pinos diferentes
- ESP32 não permite usar o mesmo SPI bus com pinos diferentes

### Outros GPIOs Disponíveis

| GPIO | Status | Observações |
|------|--------|-------------|
| 0 | Livre | Boot strapping pin (não usar durante boot) |
| 1 | Livre | UART TX (pode conflitar com debug) |
| 3 | Livre | UART RX (pode conflitar com debug) |
| 5 | Livre | - |
| 16 | Livre | - |
| 17 | Livre | - |
| 18 | Livre | - |
| 19 | Livre | - |
| 22 | Livre | - |
| 23 | Livre | - |
| 26 | Livre | - |
| 27 | Livre | - |
| 34 | Livre | Input-only pad |
| 35 | Livre | Input-only pad |
| 36 | Livre | Input-only pad (usado para IRQ do touch, opcional) |
| 37 | Livre | Input-only pad |
| 38 | Livre | Input-only pad |

## Configurações de Hardware

### Display ILI9341

```cpp
// Configurações em display_driver.cpp
constexpr uint32_t LCD_PIXEL_CLOCK_HZ = 26 * 1000 * 1000; // 26 MHz
constexpr int LCD_H_RES = 320;
constexpr int LCD_V_RES = 240;
constexpr spi_host_device_t LCD_HOST = SPI2_HOST;  // VSPI
```

**Orientação**: 
- RGB order: BGR (comum para ILI9341)
- Mirror: Horizontal (X) conforme necessário
- Rotação: 0° (portrait)

### Touch Screen XPT2046

```cpp
// Calibração inicial (ajustar conforme necessário)
constexpr TouchCalibration TOUCH_CALIB = {
    300,   // xMin
    3800,  // xMax
    350,   // yMin
    3650   // yMax
};
```

**Calibração**: Os valores de calibração podem variar entre unidades. Use um utilitário de calibração para ajustar.

**Inversão de Eixos**: 
- X e Y são invertidos no código (`LCD_H_RES - x`, `LCD_V_RES - y`)
- Isso compensa a orientação física do touch screen

## Limitações e Considerações

### SPI Bus Conflicts
- **SPI1_HOST**: Reservado para flash (não pode ser usado)
- **SPI2_HOST**: Usado pelo display com pinos específicos
- **Solução**: Touch screen usa software SPI (bit-banging)

### GPIO Constraints
- **GPIO 39 (MISO Touch)**: Input-only pad, não suporta pull-up/pull-down interno
- **GPIO 36 (IRQ Touch)**: Input-only pad, não suporta pull-up/pull-down interno
- **GPIO 0**: Boot strapping pin, não usar durante boot

### Stack Size
- **Main Task**: 9216 bytes (aumentado de 3584 para suportar LVGL)
- Configurado em `sdkconfig`: `CONFIG_ESP_MAIN_TASK_STACK_SIZE=9216`

### Memória
- **Heap Livre**: ~200KB disponível após inicialização
- **LVGL Buffer**: Alocado em DMA-capable memory (PSRAM se disponível)

## Referências

- [ESP32-Cheap-Yellow-Display GitHub](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
- [CYD Pins Documentation](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/PINS.md)
- [ILI9341 Datasheet](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf)
- [XPT2046 Datasheet](https://www.buydisplay.com/download/ic/XPT2046.pdf)
- [ESP-IDF SPI Master Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/spi_master.html)




