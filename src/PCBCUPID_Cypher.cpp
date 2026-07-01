/**
 * @file  PCBCUPID_Cypher.cpp
 * @brief Board-support implementation for the PCBCUPID Cypher (ESP32-S3-MINI-1).
 *
 * Pin mapping extracted from xtra-cypher.kicad_sch / xtra-cypher.kicad_pcb
 * (Cypher V-1.0, Espressif:ESP32-S3-MINI-1).
 *
 * Reference for ESP32-S3 Box style simplicity:
 *   https://github.com/SuGlider/Adafruit_ESP32S3_BOX
 */

#include "PCBCUPID_Cypher.h"

#if defined(ESP32)
  #include "esp_log.h"
  #include "driver/rtc_io.h"
  #include "esp_sleep.h"
  #include <SD_MMC.h>
#endif

/*===========================================================================
 *  LOGGING FALLBACK (when esp_log.h is unavailable)
 *===========================================================================*/
#ifndef LOG_LOCAL_LEVEL
  #define LOG_LOCAL_LEVEL ESP_LOG_INFO
#endif

#ifndef log_i
  #define log_i(format, ...)  // no-op
#endif
#ifndef log_w
  #define log_w(format, ...)  // no-op
#endif
#ifndef log_e
  #define log_e(format, ...)  // no-op
#endif

/*===========================================================================
 *  LOCAL CONSTANTS  PCF8563T register map
 *===========================================================================*/

#define PCF8563_REG_CTRL1      0x00   // Control/status 1
#define PCF8563_REG_CTRL2      0x01   // Control/status 2
#define PCF8563_REG_SECONDS    0x02   // Seconds (BCD, VL flag @ bit 7)
#define PCF8563_REG_MINUTES    0x03   // Minutes (BCD)
#define PCF8563_REG_HOURS      0x04   // Hours   (BCD)
#define PCF8563_REG_DAYS       0x05   // Days    (BCD)
#define PCF8563_REG_WEEKDAYS   0x06   // Weekdays (0â€“6)
#define PCF8563_REG_MONTHS     0x07   // Months / Century (BCD)
#define PCF8563_REG_YEARS      0x08   // Years (BCD, 0â€“99)

#define PCF8563_CTRL1_STOP     0x20   // STOP bit  (1 = RTC stopped)
#define PCF8563_CTRL1_TESTC    0x08   // TESTC bit (0 = normal)

#define PCF8563_SEC_MASK       0x7F
#define PCF8563_VL_FLAG        0x80   // Voltage-low detected

#define PCF8563_TIME_BASE      PCF8563_REG_SECONDS
#define PCF8563_TIME_SIZE      7      // sec â€¦ year

/* ADC attenuation for battery sensing (0 dB = ~1.1 V full-scale) */
#define CYPHER_ADC_ATTENUATION ADC_0db

/*
 * Default battery voltage-divider ratio.
 * The Cypher board does NOT have an onboard divider; add one externally:
 *   R1 between BAT+ and ADC pin, R2 between ADC pin and GND.
 *   ratio = (R1 + R2) / R2   (default 1.0 = direct)
 */
#define CYPHER_BATTERY_DIVIDER 1.0f

/*===========================================================================
 *  CONSTRUCTOR
 *===========================================================================*/

PCBCUPID_Cypher::PCBCUPID_Cypher(TwoWire &wire)
    : _wire(wire)
    , _rtc_ok(false)
    , _sd_ok(false)
    , _tft_ok(false)
{
}

/*===========================================================================
 *  begin()   full initialisation
 *===========================================================================*/

bool PCBCUPID_Cypher::begin()
{
    log_i("[Cypher] begin() â€” CYPHER V-%s", CYPHER_BOARD_VERSION);
    log_i("[Cypher] MCU: ESP32-S3-MINI-1");

    /* ---- GPIO setup for LEDs ---------------------------------------- */
    pinMode(CYPHER_LED_R, OUTPUT);
    pinMode(CYPHER_LED_G, OUTPUT);
    pinMode(CYPHER_LED_B, OUTPUT);
    // Common-anode RGB LED â€” HIGH = off
    digitalWrite(CYPHER_LED_R, HIGH);
    digitalWrite(CYPHER_LED_G, HIGH);
    digitalWrite(CYPHER_LED_B, HIGH);

    pinMode(CYPHER_LED_RED, OUTPUT);
    digitalWrite(CYPHER_LED_RED, HIGH);

    /* ---- GPIO setup for buttons ------------------------------------- */
    pinMode(CYPHER_BTN_BOOT, INPUT_PULLUP);

    /* ---- I2C bus ---------------------------------------------------- */
    _wire.begin((int)CYPHER_I2C_SDA, (int)CYPHER_I2C_SCL);
    _wire.setClock(100000);          // 100 kHz standard-mode
    delay(10);                       // let the bus settle

    /* ---- RTC (PCF8563T @ 0x51) -------------------------------------- */
    _wire.beginTransmission(CYPHER_RTC_I2C_ADDR);
    _rtc_ok = (_wire.endTransmission() == 0);

    if (_rtc_ok)
    {
        /* Clear STOP and TESTC bits so the oscillator runs */
        uint8_t ctrl1;
        if (_rtcReadReg(PCF8563_REG_CTRL1, ctrl1))
        {
            ctrl1 &= ~(PCF8563_CTRL1_STOP | PCF8563_CTRL1_TESTC);
            _rtcWriteReg(PCF8563_REG_CTRL1, ctrl1);
        }
        log_i("[Cypher] RTC (PCF8563T) found at I2C 0x%02X", CYPHER_RTC_I2C_ADDR);
    }
    else
    {
        log_w("[Cypher] RTC not found at 0x%02X â€” time functions disabled",
              CYPHER_RTC_I2C_ADDR);
    }

    log_i("[Cypher] begin() complete");
    return true;
}

/*===========================================================================
 *  beginLight()   lightweight initialisation (I2C + RTC only)
 *===========================================================================*/

bool PCBCUPID_Cypher::beginLight()
{
    log_i("[Cypher] beginLight() â€” I2C + RTC only");

    _wire.begin((int)CYPHER_I2C_SDA, (int)CYPHER_I2C_SCL);
    _wire.setClock(100000);
    delay(10);

    _wire.beginTransmission(CYPHER_RTC_I2C_ADDR);
    _rtc_ok = (_wire.endTransmission() == 0);

    if (_rtc_ok)
    {
        uint8_t ctrl1;
        if (_rtcReadReg(PCF8563_REG_CTRL1, ctrl1))
        {
            ctrl1 &= ~(PCF8563_CTRL1_STOP | PCF8563_CTRL1_TESTC);
            _rtcWriteReg(PCF8563_REG_CTRL1, ctrl1);
        }
        log_i("[Cypher] RTC detected (lightweight init)");
    }
    return true;
}

/*===========================================================================
 *  BOARD INFORMATION
 *===========================================================================*/

CypherInfo PCBCUPID_Cypher::getInfo() const
{
    CypherInfo info;
    info.board_name = "PCBCUPID Cypher";
    info.version     = CYPHER_BOARD_VERSION;
    info.mcu         = "ESP32-S3-MINI-1";
    return info;
}

/*===========================================================================
 *  STATUS HELPERS
 *===========================================================================*/

bool PCBCUPID_Cypher::isRTCReady() const
{
    return _rtc_ok;
}

bool PCBCUPID_Cypher::isSDReady() const
{
    return _sd_ok;
}

bool PCBCUPID_Cypher::isTFTReady() const
{
    return _tft_ok;
}

/*===========================================================================
 *  RGB LED
 *
 *  The Cypher uses a common-anode RGB LED:
 *    - Anode  +3.3 V
 *    - R / G / B cathodes GPIO1 / GPIO2 / GPIO3
 *  LOW  LED ON
 *  HIGH  LED OFF
 *===========================================================================*/

void PCBCUPID_Cypher::setRGBLed(CypherLedColor color)
{
    switch (color)
    {
    case CYPHER_COLOR_OFF:
        digitalWrite(CYPHER_LED_R, HIGH);
        digitalWrite(CYPHER_LED_G, HIGH);
        digitalWrite(CYPHER_LED_B, HIGH);
        break;
    case CYPHER_COLOR_RED:
        digitalWrite(CYPHER_LED_R, LOW);
        digitalWrite(CYPHER_LED_G, HIGH);
        digitalWrite(CYPHER_LED_B, HIGH);
        break;
    case CYPHER_COLOR_GREEN:
        digitalWrite(CYPHER_LED_R, HIGH);
        digitalWrite(CYPHER_LED_G, LOW);
        digitalWrite(CYPHER_LED_B, HIGH);
        break;
    case CYPHER_COLOR_BLUE:
        digitalWrite(CYPHER_LED_R, HIGH);
        digitalWrite(CYPHER_LED_G, HIGH);
        digitalWrite(CYPHER_LED_B, LOW);
        break;
    case CYPHER_COLOR_YELLOW:
        digitalWrite(CYPHER_LED_R, LOW);
        digitalWrite(CYPHER_LED_G, LOW);
        digitalWrite(CYPHER_LED_B, HIGH);
        break;
    case CYPHER_COLOR_CYAN:
        digitalWrite(CYPHER_LED_R, HIGH);
        digitalWrite(CYPHER_LED_G, LOW);
        digitalWrite(CYPHER_LED_B, LOW);
        break;
    case CYPHER_COLOR_MAGENTA:
        digitalWrite(CYPHER_LED_R, LOW);
        digitalWrite(CYPHER_LED_G, HIGH);
        digitalWrite(CYPHER_LED_B, LOW);
        break;
    case CYPHER_COLOR_WHITE:
        digitalWrite(CYPHER_LED_R, LOW);
        digitalWrite(CYPHER_LED_G, LOW);
        digitalWrite(CYPHER_LED_B, LOW);
        break;
    default:
        break;
    }
}

void PCBCUPID_Cypher::setRGBLedRaw(uint8_t r, uint8_t g, uint8_t b)
{
    /*
     * Invert values because the LED is common-anode (active-low).
     * 0    full brightness (write 255 to the pin)
     * 255  off             (write 0   to the pin)
     */
    analogWrite(CYPHER_LED_R, 255 - r);
    analogWrite(CYPHER_LED_G, 255 - g);
    analogWrite(CYPHER_LED_B, 255 - b);
}

/*===========================================================================
 *  STATUS LED (D1, Red)  active-low on GPIO6
 *===========================================================================*/

void PCBCUPID_Cypher::setStatusLed(bool on)
{
    digitalWrite(CYPHER_LED_RED, on ? LOW : HIGH);
}

void PCBCUPID_Cypher::toggleStatusLed()
{
    digitalWrite(CYPHER_LED_RED, !digitalRead(CYPHER_LED_RED));
}

/*===========================================================================
 *  BUTTONS
 *===========================================================================*/

bool PCBCUPID_Cypher::isBootPressed() const
{
    return (digitalRead(CYPHER_BTN_BOOT) == LOW);
}

/*===========================================================================
 *  BATTERY VOLTAGE (ADC on GPIO45)
 *
 *  The Cypher board does NOT include an onboard voltage divider.
 *  Add one externally:
 *
 *  ratio = (R1 + R2) / R2
 *===========================================================================*/

float PCBCUPID_Cypher::readBatteryVoltage(float dividerRatio)
{
#if defined(ESP32)
    /*
     * On ESP32-S3, ADC1_CH0 maps to GPIO1 by default.
     * GPIO45 = ADC1_CH0 on the ESP32-S3 (check the TRM for your specific chip).
     * We use the legacy analogRead() which internally configures attenuation.
     */
    analogSetAttenuation(CYPHER_ADC_ATTENUATION);

    // Average several readings for stability
    uint32_t sum = 0;
    const uint8_t samples = 8;
    for (uint8_t i = 0; i < samples; i++)
    {
        sum += analogRead(CYPHER_PIN_BATTERY);
        delay(2);
    }
    uint16_t raw = sum / samples;

    // Convert raw ADC â†’ millivolts
    // ESP32-S3 ADC: 0â€“4095 maps to 0â€“~1100 mV at 0 dB attenuation
    float mv = (float)raw * (1100.0f / 4096.0f);

    // Scale by divider to get actual battery voltage
    return mv * dividerRatio / 1000.0f;   // return volts
#else
    (void)dividerRatio;
    return 0.0f;
#endif
}

float PCBCUPID_Cypher::readBatteryVoltage()
{
    return readBatteryVoltage(CYPHER_BATTERY_DIVIDER);
}

/*===========================================================================
 *  DEEP SLEEP
 *===========================================================================*/

void PCBCUPID_Cypher::deepSleep(uint64_t sleepMs, bool wakeOnBoot)
{
#if defined(ESP32)
    log_i("[Cypher] Entering deep sleep for %llu ms", sleepMs);

    // Turn off all LEDs before sleeping
    setRGBLed(CYPHER_COLOR_OFF);
    setStatusLed(false);

    // Configure wake-up source(s)
    if (wakeOnBoot)
    {
        // Wake when BOOT button (GPIO0) goes LOW
        esp_sleep_enable_ext0_wakeup((gpio_num_t)CYPHER_BTN_BOOT, 0);
    }

    if (sleepMs > 0)
    {
        esp_sleep_enable_timer_wakeup(sleepMs * 1000ULL);
    }

    esp_deep_sleep_start();
#else
    (void)sleepMs;
    (void)wakeOnBoot;
#endif
}

/*===========================================================================
 *  RESET
 *===========================================================================*/

void PCBCUPID_Cypher::reset()
{
    log_i("[Cypher] Software reset requested");
    ESP.restart();
}

/*===========================================================================
 *  RTC    PCF8563T  (I2C)
 *
 *  Read / write date & time via the RTC.
 *  Data is stored as BCD in the chip; conversion is handled internally.
 *===========================================================================*/

bool PCBCUPID_Cypher::readRTC(uint16_t &year, uint8_t &month, uint8_t &day,
                               uint8_t &weekday, uint8_t &hour,
                               uint8_t &minute, uint8_t &second)
{
    if (!_rtc_ok)
    {
        log_w("[Cypher] readRTC(): RTC not available");
        return false;
    }

    uint8_t buf[PCF8563_TIME_SIZE];
    if (!_rtcReadBurst(PCF8563_TIME_BASE, buf, PCF8563_TIME_SIZE))
        return false;

    second  = _bcd2dec(buf[0] & PCF8563_SEC_MASK);
    minute  = _bcd2dec(buf[1] & 0x7F);
    hour    = _bcd2dec(buf[2] & 0x3F);
    day     = _bcd2dec(buf[3] & 0x3F);
    weekday = _bcd2dec(buf[4] & 0x07);
    month   = _bcd2dec(buf[5] & 0x1F);
    year    = 2000 + _bcd2dec(buf[6]);

    // Voltage-low flag â†’ RTC time may be garbage after power loss
    if (buf[0] & PCF8563_VL_FLAG)
    {
        log_w("[Cypher] RTC voltage-low â€” time may be invalid");
    }

    return true;
}

bool PCBCUPID_Cypher::setRTC(uint16_t year, uint8_t month, uint8_t day,
                              uint8_t weekday, uint8_t hour,
                              uint8_t minute, uint8_t second)
{
    if (!_rtc_ok)
    {
        log_w("[Cypher] setRTC(): RTC not available");
        return false;
    }

    // 1. Stop the clock
    uint8_t ctrl1;
    if (!_rtcReadReg(PCF8563_REG_CTRL1, ctrl1))
        return false;
    _rtcWriteReg(PCF8563_REG_CTRL1, ctrl1 | PCF8563_CTRL1_STOP);

    // 2. Write time registers (BCD)
    uint8_t buf[PCF8563_TIME_SIZE];
    buf[0] = _dec2bcd(second);
    buf[1] = _dec2bcd(minute);
    buf[2] = _dec2bcd(hour);
    buf[3] = _dec2bcd(day);
    buf[4] = _dec2bcd(weekday);
    buf[5] = _dec2bcd(month);
    buf[6] = _dec2bcd((uint8_t)(year - 2000));

    _wire.beginTransmission(CYPHER_RTC_I2C_ADDR);
    _wire.write(PCF8563_TIME_BASE);
    _wire.write(buf, PCF8563_TIME_SIZE);
    if (_wire.endTransmission() != 0)
    {
        log_e("[Cypher] setRTC(): I2C write failed");
        // Try to restart the clock anyway
        _rtcWriteReg(PCF8563_REG_CTRL1, ctrl1 & ~PCF8563_CTRL1_STOP);
        return false;
    }

    // 3. Restart the clock
    _rtcWriteReg(PCF8563_REG_CTRL1, ctrl1 & ~PCF8563_CTRL1_STOP);

    log_i("[Cypher] RTC set: %04u-%02u-%02u %02u:%02u:%02u (wday=%u)",
          year, month, day, hour, minute, second, weekday);
    return true;
}

/* ----  RTC low-level I2C helpers  ---------------------------------------- */

bool PCBCUPID_Cypher::_rtcWriteReg(uint8_t reg, uint8_t val)
{
    _wire.beginTransmission(CYPHER_RTC_I2C_ADDR);
    _wire.write(reg);
    _wire.write(val);
    return (_wire.endTransmission() == 0);
}

bool PCBCUPID_Cypher::_rtcReadReg(uint8_t reg, uint8_t &val)
{
    _wire.beginTransmission(CYPHER_RTC_I2C_ADDR);
    _wire.write(reg);
    if (_wire.endTransmission(false) != 0)
        return false;
    if (_wire.requestFrom(CYPHER_RTC_I2C_ADDR, (uint8_t)1) != 1)
        return false;
    val = _wire.read();
    return true;
}

bool PCBCUPID_Cypher::_rtcReadBurst(uint8_t startReg, uint8_t *buf, uint8_t len)
{
    _wire.beginTransmission(CYPHER_RTC_I2C_ADDR);
    _wire.write(startReg);
    if (_wire.endTransmission(false) != 0)
        return false;
    if (_wire.requestFrom(CYPHER_RTC_I2C_ADDR, len) != len)
        return false;
    for (uint8_t i = 0; i < len; i++)
        buf[i] = _wire.read();
    return true;
}

uint8_t PCBCUPID_Cypher::_bcd2dec(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

uint8_t PCBCUPID_Cypher::_dec2bcd(uint8_t dec)
{
    return ((dec / 10) << 4) | (dec % 10);
}

/*===========================================================================
 *  SD CARD    MicroSD via SDIO (4-bit) or SPI fallback
 *
 *  SDIO pin mapping (from PCB silkscreen):
 *    CMD  = GPIO35    D0   = GPIO37    D2 = GPIO39
 *    CLK  = GPIO36    D1   = GPIO38    D3 = GPIO40
 *
 *  SPI fallback:
 *    SCK  = GPIO36    MISO = GPIO37    MOSI = GPIO35
 *    CS   = GPIO40    (DAT3 = CS in SPI mode)
 *===========================================================================*/

bool PCBCUPID_Cypher::beginSD()
{
#if defined(ESP32)
    log_i("[Cypher] beginSD() â€” SDIO 4-bit mode");

    /*
     * SD_MMC.setPins() signature (Arduino-ESP32 v2.x):
     *   void setPins(int clk, int cmd, int d0)
     *   void setPins(int clk, int cmd, int d0, int d1, int d2, int d3)
     */
    SD_MMC.setPins(CYPHER_SD_CLK, CYPHER_SD_CMD,
                   CYPHER_SD_D0, CYPHER_SD_D1,
                   CYPHER_SD_D2, CYPHER_SD_D3);

    /*
     * SD_MMC.begin(mountpoint, mode1bit, format_if_mount_failed, freq, max_files)
     *   mode1bit = false   4-bit SDIO  (what we want)
     */
    bool success = SD_MMC.begin("/sdcard", false, false, 40000);
    if (success)
    {
        _sd_ok = true;
        uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
        log_i("[Cypher] SD card mounted â€” %llu MB (%s mode)",
              cardSize,
              (SD_MMC.cardType() == CARD_SDHC) ? "SDHC" :
              (SD_MMC.cardType() == CARD_SD)   ? "SDSC" :
              (SD_MMC.cardType() == CARD_MMC)  ? "MMC"  : "?");
    }
    else
    {
        _sd_ok = false;
        log_w("[Cypher] SD card mount failed â€” check card / format");
    }
    return _sd_ok;
#else
    log_w("[Cypher] SDIO mode only supported on ESP32");
    _sd_ok = false;
    return false;
#endif
}

bool PCBCUPID_Cypher::beginSD_SPI(uint8_t csPin)
{
    log_i("[Cypher] beginSD_SPI() â€” SPI fallback (CS=GPIO%u)", csPin);

    /*
     * SPI pin mapping for SD card:
     *   SPI SCK   GPIO36 (CYPHER_SD_CLK)
     *   SPI MISO  GPIO37 (CYPHER_SD_D0)
     *   SPI MOSI  GPIO35 (CYPHER_SD_CMD)
     *   SPI CS    GPIO40 (CYPHER_SD_D3)  [DAT3 = CS in SPI mode]
     *
     * Note: calling SPI.begin() reconfigures the bus.
     * Call SPI.end() before switching back to TFT SPI.
     */
    SPI.begin(CYPHER_SD_CLK,   // SCK
              CYPHER_SD_D0,    // MISO
              CYPHER_SD_CMD,   // MOSI
              csPin);          // SS  (defaults to CYPHER_SD_CMD = GPIO35)

    pinMode(csPin, OUTPUT);
    digitalWrite(csPin, HIGH);
    delay(10);

    // Send at least 74 dummy clocks with CS high (SD spec requirement)
    digitalWrite(csPin, HIGH);
    for (uint8_t i = 0; i < 10; i++)
        SPI.transfer(0xFF);

    // Try CMD0 (GO_IDLE_STATE)
    digitalWrite(csPin, LOW);
    SPI.transfer(0xFF); // dummy
    uint8_t response = SPI.transfer(0xFF);
    digitalWrite(csPin, HIGH);

    // CMD0 should return 0x01 (idle)
    if (response == 0x01)
    {
        _sd_ok = true;
        log_i("[Cypher] SD card detected in SPI mode");
    }
    else
    {
        _sd_ok = false;
        log_w("[Cypher] SD card not responding on SPI (resp=0x%02X)", response);
    }
    return _sd_ok;
}

/*===========================================================================
 *  TFT DISPLAY    ER-TFT0.96-4 (ST7735-compatible, 80Ã—160)
 *
 *  Pin mapping:
 *    RST  = GPIO8     DC  = GPIO9     CS   = GPIO10
 *    MOSI = GPIO11    SCK = GPIO12
 *
 *  The Adafruit_ST7735 / Adafruit_GFX libraries handle the protocol.
 *  This method sets up the GPIOs, toggles the hardware reset line, and
 *  initialises the SPI bus. After calling beginTFT() you can construct
 *  an Adafruit_ST7735 and call its initR() method.
 *===========================================================================*/

bool PCBCUPID_Cypher::beginTFT()
{
    log_i("[Cypher] beginTFT() â€” ER-TFT0.96-4 (ST7735, 80Ã—160)");

    // ---- GPIO direction ----------------------------------------------
    pinMode(CYPHER_TFT_RST,  OUTPUT);
    pinMode(CYPHER_TFT_DC,   OUTPUT);
    pinMode(CYPHER_TFT_CS,   OUTPUT);
    pinMode(CYPHER_TFT_MOSI, OUTPUT);
    pinMode(CYPHER_TFT_SCLK, OUTPUT);

    // Idle state
    digitalWrite(CYPHER_TFT_CS,  HIGH);
    digitalWrite(CYPHER_TFT_DC,  HIGH);

    // ---- Hardware reset sequence -------------------------------------
    digitalWrite(CYPHER_TFT_RST, HIGH);
    delay(5);
    digitalWrite(CYPHER_TFT_RST, LOW);
    delay(20);
    digitalWrite(CYPHER_TFT_RST, HIGH);
    delay(150);   // wait for the controller to boot

    // ---- SPI bus -----------------------------------------------------
    /*
     * The TFT uses the ESP32's VSPI (default) pins.
     * SPI.begin() with explicit pin mapping to be safe.
     */
    SPI.begin(CYPHER_TFT_SCLK,  // SCK
              -1,                // MISO (not used by TFT)
              CYPHER_TFT_MOSI,  // MOSI
              CYPHER_TFT_CS);   // SS

    SPI.setFrequency(40000000);          // 40 MHz (ST7735 max)

    _tft_ok = true;
    log_i("[Cypher] TFT pins: RST=%d DC=%d CS=%d MOSI=%d SCK=%d",
          CYPHER_TFT_RST, CYPHER_TFT_DC, CYPHER_TFT_CS,
          CYPHER_TFT_MOSI, CYPHER_TFT_SCLK);

    return true;
}

/*===========================================================================
 *  scanI2C()   utility to discover devices on the I2C bus
 *===========================================================================*/

void PCBCUPID_Cypher::scanI2C()
{
    log_i("[Cypher] I2C scan (SDA=GPIO%d, SCL=GPIO%d) ...",
          CYPHER_I2C_SDA, CYPHER_I2C_SCL);

    uint8_t count = 0;
    for (uint8_t addr = 1; addr < 127; addr++)
    {
        _wire.beginTransmission(addr);
        if (_wire.endTransmission() == 0)
        {
            log_i("  [*] 0x%02X", addr);
            count++;
        }
        delay(1);  // avoid bus congestion
    }

    if (count == 0)
        log_i("  (no devices found)");
    else
        log_i("  %u device(s) on bus", count);
}


/*===========================================================================
 *  PCBCUPID_Cypher_tft  –  ST7735 Display Wrapper
 *  (only compiled when Adafruit ST7735 library is installed)
 *===========================================================================*/

#if CYPHER_HAS_TFT_LIBRARY

PCBCUPID_Cypher_tft::PCBCUPID_Cypher_tft()
    : Adafruit_ST7735(CYPHER_TFT_CS, CYPHER_TFT_DC, CYPHER_TFT_RST)
{
}

void PCBCUPID_Cypher_tft::init()
{
    /*
     * Configure hardware SPI with the Cypher TFT pins
     * before initialising the ST7735 controller.
     *
     *   SCK  = GPIO12   MOSI = GPIO11   CS = GPIO10
     *   DC   = GPIO9    RST  = GPIO8
     */
    SPI.begin(CYPHER_TFT_SCLK, -1, CYPHER_TFT_MOSI, CYPHER_TFT_CS);
    SPI.setFrequency(40000000);

    /*
     * initR() performs the hardware reset sequence and sends the
     * initialisation command table for the ST7735R controller.
     * INITR_MINI160x80 is the correct initialisation for the
     * 0.96-inch 80x160 display used on the Cypher board.
     */
    initR(INITR_MINI160x80);

    /*
     * Rotation 1 = portrait: 80 pixels wide × 160 pixels tall.
     * This matches the physical orientation of the Cypher board.
     */
    setRotation(1);

    log_i("[Cypher] TFT initialised (ST7735, 80x160, HW SPI)");
}

#endif  // CYPHER_HAS_TFT_LIBRARY
