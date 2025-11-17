# Pinout Rápido - ESP32-CYD Satisfaction Hub

## Resumo Visual dos Pinos

### Display ILI9341 (SPI2_HOST - VSPI)

```
ESP32                    ILI9341 Display
┌─────────┐             ┌──────────────┐
│ GPIO 13 │─────────────│ MOSI (SDI)   │
│ GPIO 14 │─────────────│ CLK (SCK)    │
│ GPIO 15 │─────────────│ CS           │
│ GPIO 2  │─────────────│ DC (RS)      │
│ GPIO 4  │─────────────│ RST          │
│ GPIO 21 │─────────────│ BL (Backlight)│
│ GPIO 12 │─────────────│ MISO (SDO)   │ (não usado)
└─────────┘             └──────────────┘
```

**Configuração SPI**:
- Bus: SPI2_HOST (VSPI)
- Clock: 26 MHz
- Mode: SPI Mode 0
- CS: GPIO 15 (Active Low)
- DC: GPIO 2 (Data=High, Command=Low)

### Touch Screen XPT2046 (Software SPI - Bit-banging)

```
ESP32                    XPT2046 Touch
┌─────────┐             ┌──────────────┐
│ GPIO 32 │─────────────│ MOSI         │ (Output)
│ GPIO 25 │─────────────│ CLK          │ (Output)
│ GPIO 33 │─────────────│ CS           │ (Output, Active Low)
│ GPIO 39 │─────────────│ MISO         │ (Input-only)
│ GPIO 36 │─────────────│ IRQ          │ (Input-only, opcional)
└─────────┘             └──────────────┘
```

**Nota**: Touch usa software SPI (bit-banging) porque:
- SPI1_HOST está reservado para flash
- SPI2_HOST está em uso pelo display com pinos diferentes
- ESP32 não permite mesmo SPI bus com pinos diferentes

## Tabela de Referência Rápida

### Display

| GPIO | Função | Direção | Observações |
|------|--------|---------|-------------|
| 13   | MOSI   | Output  | SPI Data Out |
| 14   | CLK    | Output  | SPI Clock |
| 15   | CS     | Output  | Chip Select (Active Low) |
| 2    | DC     | Output  | Data/Command Select |
| 4    | RST    | Output  | Reset (Active Low) |
| 21   | BL     | Output  | Backlight Control |
| 12   | MISO   | Input   | Não usado neste projeto |

### Touch Screen

| GPIO | Função | Direção | Observações |
|------|--------|---------|-------------|
| 32   | MOSI   | Output  | Software SPI |
| 25   | CLK    | Output  | Software SPI Clock |
| 33   | CS     | Output  | Chip Select (Active Low) |
| 39   | MISO   | Input   | Input-only pad (sem pull-up) |
| 36   | IRQ    | Input   | Interrupt (opcional, não usado) |

## GPIOs Livres Disponíveis

| GPIO | Tipo | Observações |
|------|------|-------------|
| 0    | I/O  | Boot strapping pin (não usar durante boot) |
| 1    | I/O  | UART TX (pode conflitar com debug) |
| 3    | I/O  | UART RX (pode conflitar com debug) |
| 5    | I/O  | Livre |
| 16   | I/O  | Livre |
| 17   | I/O  | Livre |
| 18   | I/O  | Livre |
| 19   | I/O  | Livre |
| 22   | I/O  | Livre |
| 23   | I/O  | Livre |
| 26   | I/O  | Livre |
| 27   | I/O  | Livre |
| 34   | Input | Input-only pad |
| 35   | Input | Input-only pad |
| 37   | Input | Input-only pad |
| 38   | Input | Input-only pad |

## Configurações no Código

### Display Driver (`display_driver.cpp`)

```cpp
constexpr gpio_num_t PIN_NUM_MOSI = GPIO_NUM_13;
constexpr gpio_num_t PIN_NUM_CLK = GPIO_NUM_14;
constexpr gpio_num_t PIN_NUM_CS = GPIO_NUM_15;
constexpr gpio_num_t PIN_NUM_DC = GPIO_NUM_2;
constexpr gpio_num_t PIN_NUM_RST = GPIO_NUM_4;
constexpr gpio_num_t PIN_NUM_BK_LIGHT = GPIO_NUM_21;
constexpr spi_host_device_t LCD_HOST = SPI2_HOST;
```

### Touch Driver (`display_driver.cpp`)

```cpp
constexpr gpio_num_t PIN_NUM_TOUCH_MOSI = GPIO_NUM_32;
constexpr gpio_num_t PIN_NUM_TOUCH_CLK = GPIO_NUM_25;
constexpr gpio_num_t PIN_NUM_TOUCH_CS = GPIO_NUM_33;
constexpr gpio_num_t PIN_NUM_TOUCH_MISO = GPIO_NUM_39;
```

## Restrições e Limitações

### Input-Only Pads
- **GPIO 34, 35, 36, 37, 38, 39**: Não suportam pull-up/pull-down interno
- **GPIO 39 (MISO Touch)**: Deve ser configurado sem pull-up/pull-down

### Boot Strapping Pins
- **GPIO 0**: Usado para boot mode selection
- Não usar durante boot ou configurar com cuidado

### SPI Bus Conflicts
- **SPI1_HOST**: Reservado para flash (não pode ser usado)
- **SPI2_HOST**: Usado pelo display
- **Solução**: Touch usa software SPI (bit-banging)

## Referências

- [HARDWARE.md](HARDWARE.md) - Documentação técnica completa
- [ESP32-Cheap-Yellow-Display PINS.md](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/PINS.md)
- [ESP32 GPIO Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html)




