# PCBCUPID Cypher Board-Support Library

An Arduino-compatible board-support library for the **PCBCUPID Cypher** development board (CYPHER V-1.0), built around the **ESP32-S3-MINI-1**. Provides a simple, unified API for all onboard peripherals.

---

## Overview

The **PCBCUPID Cypher** library wraps the most common Cypher board peripherals into a single class:

- **0.96" TFT Display** (ER-TFT0.96-4, ST7735 compatible) — SPI
- **MicroSD Card** — SDIO 4-bit & SPI fallback
- **Real-Time Clock** (PCF8563T) — I2C
- **RGB LED** — common-anode, active-low GPIO control
- **Boot Button** — GPIO0 with pull-up

Designed for ESP32 (tested on Cypher V-1.0), this library lets you focus on your application instead of memorising pin numbers.

---

## Key Features

### Simple API
- Single `begin()` call initialises the entire board
- `beginLight()` for power-sensitive applications (I2C + RTC only)
- `setRGBLed(CYPHER_COLOR_GREEN)` for quick LED control

### Timekeeping
- Read/write date & time from the onboard PCF8563T RTC
- BCD conversion handled internally
- Voltage-low detection and reporting

### Storage
- SDIO 4-bit mode for high-speed MicroSD access
- SPI fallback mode for compatibility
- Uses standard `SD_MMC` / `SD` Arduino libraries

### Display Ready
- Configures all TFT GPIOs and performs hardware reset
- Ready to use with Adafruit_ST7735 / Adafruit_GFX
- Pin mapping exported as preprocessor constants

---

## Hardware Support

**Supported Board:**
- PCBCUPID Cypher V-1.0 (ESP32-S3-MINI-1)

**Onboard Peripherals:**

| Peripheral | Interface | Chip / Part |
|------------|-----------|-------------|
| TFT Display | SPI | ER-TFT0.96-4 (ST7735) |
| MicroSD Card | SDIO / SPI | — |
| Real-Time Clock | I2C (0x51) | PCF8563T |
| RGB LED | GPIO | Common-anode LED |
| Status LED | GPIO | Red 0603 LED |

**Compatible Platforms:**
- ESP32-S3 (all variants)

---

## Pin Reference

| Function | GPIO | Notes |
|----------|------|-------|
| I2C SDA | 4 | RTC & expansion |
| I2C SCL | 5 | RTC & expansion |
| TFT RST | 8 | Display reset |
| TFT DC | 9 | Data / Command |
| TFT CS | 10 | Chip select |
| TFT MOSI | 11 | SPI data |
| TFT SCLK | 12 | SPI clock |
| SD CMD | 35 | SDIO / SPI CS |
| SD CLK | 36 | SDIO / SPI SCK |
| SD D0 | 37 | SDIO data 0 / SPI MISO |
| SD D1 | 38 | SDIO data 1 |
| SD D2 | 39 | SDIO data 2 |
| SD D3 | 40 | SDIO data 3 |
| RGB Red | 1 | Common-anode (LOW = ON) |
| RGB Green | 2 | Common-anode (LOW = ON) |
| RGB Blue | 3 | Common-anode (LOW = ON) |
| Status LED | 6 | Active-low |
| Boot Button | 0 | Pull-up, LOW = pressed |
| Reset Button | EN | Hard reset |
| USB D+ | 20 | Native USB |
| USB D- | 19 | Native USB |
| U0 TXD | 43 | Serial console |
| U0 RXD | 44 | Serial console |
| U1 TXD | 17 | Aux UART |
| U1 RXD | 18 | Aux UART |

---

## Quick Start

```cpp
#include <PCBCUPID_Cypher.h>
#include <Adafruit_ST7789.h>   // ST7735 is compatible

TwoWire I2Cbus(0);
PCBCUPID_Cypher cypher(I2Cbus);
Adafruit_ST7789 tft(CYPHER_TFT_CS, CYPHER_TFT_DC, CYPHER_TFT_RST);

void setup() {
    Serial.begin(115200);

    // 1. Initialise the board
    cypher.begin();

    // 2. Start the display
    cypher.beginTFT();
    tft.init(CYPHER_TFT_HEIGHT, CYPHER_TFT_WIDTH);
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);

    // 3. Light up the RGB LED green
    cypher.setRGBLed(CYPHER_COLOR_GREEN);

    // 4. Read the RTC
    uint16_t year;
    uint8_t month, day, wday, hour, minute, second;
    if (cypher.readRTC(year, month, day, wday, hour, minute, second)) {
        Serial.printf("RTC: %04u-%02u-%02u %02u:%02u:%02u\n",
                      year, month, day, hour, minute, second);
    }
}

void loop() {
    // Toggle status LED each second
    cypher.toggleStatusLed();
    delay(1000);
}
```

---

## API Reference

### Initialisation

```cpp
bool begin();          // Full board init (I2C, RTC, GPIOs, LEDs)
bool beginLight();     // I2C + RTC only (low-power)
```

### Board Info

```cpp
CypherInfo getInfo();  // Returns board name, version, MCU
```

### RGB LED

```cpp
void setRGBLed(CypherLedColor color);   // Preset colours
void setRGBLedRaw(uint8_t r, uint8_t g, uint8_t b); // 0=ON, 255=OFF (PWM)
```

Preset colours: `CYPHER_COLOR_OFF`, `CYPHER_COLOR_RED`, `CYPHER_COLOR_GREEN`, `CYPHER_COLOR_BLUE`, `CYPHER_COLOR_YELLOW`, `CYPHER_COLOR_CYAN`, `CYPHER_COLOR_MAGENTA`, `CYPHER_COLOR_WHITE`

### Status LED

```cpp
void setStatusLed(bool on);
void toggleStatusLed();
```

### Buttons

```cpp
bool isBootPressed();  // true when SW2 held down
```

### RTC

```cpp
bool readRTC(uint16_t &year, uint8_t &month, uint8_t &day,
             uint8_t &weekday, uint8_t &hour,
             uint8_t &minute, uint8_t &second);

bool setRTC(uint16_t year, uint8_t month, uint8_t day,
            uint8_t weekday, uint8_t hour,
            uint8_t minute, uint8_t second);
```

### Storage

```cpp
bool beginSD();                              // SDIO 4-bit mode
bool beginSD_SPI(uint8_t csPin = CYPHER_SD_CMD); // SPI fallback
```

### Display

```cpp
bool beginTFT();   // Configure GPIOs + hardware reset
```

### Utilities

```cpp
void scanI2C();    // Scan I2C bus, print results to Serial
TwoWire &getWire(); // Access the underlying I2C instance
```

---

## Examples

Included examples demonstrate:

- **`basic`** — Full board demo: RTC read, RGB LED cycling, button monitoring, SD card mount
- **`display_test`** — TFT initialisation with ST7735 + GFX drawing
- **`sd_card`** — SD card initialisation in SDIO and SPI modes with file listing
- **`sd_mmc`** — SD_MMC + USB Mass Storage: exposes the SD card as a USB drive

> Examples are located in the `examples/` directory of the library.

---

## Dependencies

- **Arduino-ESP32** core (v2.0+)
- **Wire** (built-in)
- **SPI** (built-in)
- **SD_MMC** (built-in, for SDIO)
- **Adafruit_ST7735** + **Adafruit_GFX** (optional, for display)

---

## Documentation & Resources

- **ESP32-S3-MINI-1 Datasheet**: [Espressif](https://www.espressif.com/sites/default/files/documentation/esp32-s3-mini-1_mini-1u_datasheet_en.pdf)
- **PCF8563 Datasheet**: [NXP](https://www.nxp.com/docs/en/data-sheet/PCF8563.pdf)
- **ST7735 Datasheet**: [Sitronix](https://www.displayfuture.com/Display/datasheet/controller/ST7735.pdf)
- **Community Support**: [PCBCUPID Forum](https://forum.pcbcupid.com/)

---

## Credits & Attribution

Developed and maintained by **PCBCUPID**, based on the Cypher V-1.0 board design.

**Original Author:**
- **Abhishek N** — @PCBCUPID
- **Version**: 1.0.0
- **License**: MIT License (refer to the LICENSE file for terms)

---

## Contributing

We welcome contributions! Please refer to the [Contributing Guidelines](CONTRIBUTING.md) before submitting changes.

---

## License

This library is licensed under the **MIT License**. See the `LICENSE` file for full details.

---

**Made with 🔧 by PCBCUPID**

---
