# CYD Pin Reference

This document summarizes the pinout of the CYD board for quick reference.

---

## 📦 Connector Types

| Connector      | Type            | Function                          |
| -------------- | --------------- | --------------------------------- |
| [**P1**](#p1)  | 4P 1.25mm JST   | Serial                            |
| [**P3**](#p3)  | 4P 1.25mm JST   | GPIO                              |
| [**P4**](#p4)  | 2P 1.25mm JST   | Speaker                           |
| [**CN1**](#cn1)| 4P 1.25mm JST   | GPIO (I2C)                        |

---

## 🟢 Accessible GPIO Pins

> The following GPIO pins are directly usable:

| Pin   | Location              | Notes                                            |
|-------|-----------------------|--------------------------------------------------|
| IO35  | P3 JST connector      | *Input only*, no internal pull-ups               |
| IO22  | P3 & CN1 JST connectors|                                                  |
| IO27  | CN1 JST connector     |                                                  |

> For additional GPIOs, consider using an SD Card sniffer ([see Add-ons](/ADDONS.md)), or (as a last resort) hardware modifications.

---

## 🔌 Broken-Out Pinsets

### P3 (4P 1.25mm JST)
| Pin   | Usage         | Notes                                         |
|-------|--------------|-----------------------------------------------|
| GND   | Ground       |                                               |
| IO35  | GPIO (in)    | Input only; no internal pull-ups              |
| IO22  | GPIO         | Also present on **CN1**                       |
| IO21  | TFT BL       | TFT backlight; not generally usable as GPIO   |

---

### CN1 (4P 1.25mm JST, I2C-Friendly)
| Pin   | Usage         | Notes                             |
|-------|--------------|-----------------------------------|
| GND   | Ground       |                                   |
| IO22  | GPIO         | Also present on **P3**             |
| IO27  | GPIO         |                                   |
| 3.3V  | Power        |                                   |

---

### P1 (4P 1.25mm JST, Serial)
| Pin   | Usage       | Notes                                 |
|-------|------------|---------------------------------------|
| VIN   | Power In   |                                       |
| IO1(?)| TX         | Possibly usable as GPIO                |
| IO3(?)| RX         | Possibly usable as GPIO                |
| GND   | Ground     |                                       |

---

## 🔘 Buttons

| Pin  | Usage | Notes                                |
|------|-------|--------------------------------------|
| IO0  | BOOT  | Usable as input in firmware/sketches |

---

## 🔊 Speaker

> The speaker connector is **not usable as GPIO**; it is connected to the amplifier.

| Pin  | Usage              | Notes                                            |
|------|--------------------|--------------------------------------------------|
| IO26 | Connected to Amp   | See: `i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);`|

---

## 🌈 RGB LED

> The onboard RGB LED can be repurposed if extra pins are needed.  
> **Note:** LEDs are "active low" (HIGH=off, LOW=on)

| Pin  | Color  | Notes |
|------|--------|-------|
| IO4  | Red    |       |
| IO16 | Green  |       |
| IO17 | Blue   |       |

---

## 💾 SD Card (VSPI)

> VSPI is used for SD card. Pin names are predefined in `SPI.h`.

| Pin  | Function | Notes |
|------|----------|-------|
| IO5  | SS       |       |
| IO18 | SCK      |       |
| IO19 | MISO     |       |
| IO23 | MOSI     |       |

---

## 🖱️ Touch Screen

| Pin  | Function          |
|------|-------------------|
| IO25 | XPT2046_CLK       |
| IO32 | XPT2046_MOSI      |
| IO33 | XPT2046_CS        |
| IO36 | XPT2046_IRQ       |
| IO39 | XPT2046_MISO      |

---

## 🌞 LDR (Light Sensor)

| Pin  | Usage |
|------|-------|
| IO34 | LDR   |

---

## 🖥️ TFT Display (HSPI)

| Pin  | Function   | Notes                     |
|------|------------|---------------------------|
| IO2  | TFT_RS     | AKA: TFT_DC               |
| IO12 | TFT_SDO    | AKA: TFT_MISO             |
| IO13 | TFT_SDI    | AKA: TFT_MOSI             |
| IO14 | TFT_SCK    |                           |
| IO15 | TFT_CS     |                           |
| IO21 | TFT_BL     | Also on **P3** connector  |

---

## 📍 Test Points

| Pad                        | Function | Notes                       |
|----------------------------|----------|-----------------------------|
| S1                         | GND      | Near USB-Serial             |
| S2                         | 3.3V     | For ESP32                   |
| S3                         | 5V       | Near USB-Serial             |
| S4                         | GND      | For ESP32                   |
| S5                         | 3.3V     | For TFT                     |
| JP0 (nearest USB socket)   | 5V       | TFT LDO                     |
| JP0                        | 3.3V     | TFT LDO                     |
| JP3 (nearest USB socket)   | 5V       | ESP32 LDO                   |
| JP3                        | 3.3V     | ESP32 LDO                   |

---
