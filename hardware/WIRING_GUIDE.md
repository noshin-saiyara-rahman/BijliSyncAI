# Hardware Wiring & Safety Guide

> **Electrical Safety Warning:** The PZEM-004T v3.0 interfaces directly with 220V–240V AC mains voltage for voltage reference sensing. Always isolate the main circuit breaker before installing the split-core Current Transformer (CT) or connecting AC line/neutral reference wires.

## Pin-to-Pin Interconnection Table

| Peripheral Module | Module Pin | ESP32 DevKit V1 Pin | Interface | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **PZEM-004T v3.0** | `5V` / `VCC` | `VIN` (5V) | Power | Optocoupler supply |
| **PZEM-004T v3.0** | `GND` | `GND` | Ground | Common reference |
| **PZEM-004T v3.0** | `TX` | `GPIO 16 (RX2)` | UART2 | 9600 bps, 8N1 |
| **PZEM-004T v3.0** | `RX` | `GPIO 17 (TX2)` | UART2 | 9600 bps, 8N1 |
| **1.3" OLED (SH1106)** | `VCC` | `3V3` | Power | 3.3V logic |
| **1.3" OLED (SH1106)** | `GND` | `GND` | Ground | Common reference |
| **1.3" OLED (SH1106)** | `SDA` | `GPIO 21` | I2C | Default ESP32 SDA |
| **1.3" OLED (SH1106)** | `SCL` | `GPIO 22` | I2C | Default ESP32 SCL |
| **microSD Card Module** | `VCC` | `3V3` (or `VIN` if 5V LDO) | Power | Check module regulator |
| **microSD Card Module** | `GND` | `GND` | Ground | Common reference |
| **microSD Card Module** | `CS` | `GPIO 5` | SPI | Chip Select |
| **microSD Card Module** | `MOSI` | `GPIO 23` | SPI | Master Out Slave In |
| **microSD Card Module** | `MISO` | `GPIO 19` | SPI | Master In Slave Out |
| **microSD Card Module** | `SCK` | `GPIO 18` | SPI | SPI Clock |

## Split-Core CT Installation
1. Clamp the **100A split-core Current Transformer (PZCT-02)** around **only the Live (Phase) conductor** entering the residential distribution board—never clamp around both Live and Neutral together, as their magnetic fields cancel out.
2. Ensure the arrow (`K -> L`) on the CT housing points from the utility supply toward the household load.
