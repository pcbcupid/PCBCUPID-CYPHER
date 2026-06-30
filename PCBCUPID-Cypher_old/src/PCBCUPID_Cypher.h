#ifndef PCBCUPID_CYPHER_H
#define PCBCUPID_CYPHER_H

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>

/*
 * The PCBCUPID_Cypher_tft class (and its Adafruit_ST7735 dependency) is
 * only available when the Adafruit ST7735 library is installed.
 * Sketches that don't use the display (basic, sd_card) compile without it.
 */
#if __has_include(<Adafruit_ST7735.h>)
  #define CYPHER_HAS_TFT_LIBRARY 1
  #include <Adafruit_ST7735.h>
#endif

/*===========================================================================
 *  CYPHER BOARD VERSION
 *===========================================================================*/
#define CYPHER_BOARD_VERSION "2.0"

/*===========================================================================
 *  I2C BUS
 *  Connected: PCF8563T RTC (0x51)
 *===========================================================================*/
#define CYPHER_I2C_SDA  4
#define CYPHER_I2C_SCL  5

#define CYPHER_RTC_I2C_ADDR 0x51

/*===========================================================================
 *  TFT DISPLAY (ER-TFT0.96-4 / ST7735 80x160)
 *  SPI interface on 8-12
 *===========================================================================*/
#define CYPHER_TFT_RST   8
#define CYPHER_TFT_DC    9
#define CYPHER_TFT_CS    10
#define CYPHER_TFT_MOSI  11
#define CYPHER_TFT_SCLK  12

#define CYPHER_TFT_WIDTH   80
#define CYPHER_TFT_HEIGHT  160

/*===========================================================================
 *  SD CARD â€“ SDIO / SPI mode
 *  Connected via 35-40
 *===========================================================================*/
#define CYPHER_SD_CMD   35
#define CYPHER_SD_CLK   36
#define CYPHER_SD_D0    37
#define CYPHER_SD_D1    38
#define CYPHER_SD_D2    39
#define CYPHER_SD_D3    40

/*===========================================================================
 *  RGB LED â€“ Common Anode (CA to +3.3V)
 *  Cathode pins: Red=1, Green=2, Blue=3
 *  LOW = LED ON, HIGH = LED OFF (active-low)
 *===========================================================================*/
#define CYPHER_LED_R    1
#define CYPHER_LED_G    2
#define CYPHER_LED_B    3

/*===========================================================================
 *  RED STATUS LED (D1) â€“ active-low
 *===========================================================================*/
#define CYPHER_LED_RED  6

/*===========================================================================
 *  BUTTONS
 *===========================================================================*/
#define CYPHER_BTN_BOOT  0   // SW2 â€“ Boot button (pull-up, LOW = pressed)
#define CYPHER_PIN_EN    45       // SW1 â€“ Reset (pulled high, LOW = reset)

/*===========================================================================
 *  USB / UART
 *===========================================================================*/
#define CYPHER_USB_DN   19
#define CYPHER_USB_DP   20

#define CYPHER_U0_TXD   43
#define CYPHER_U0_RXD   44
#define CYPHER_U1_TXD   17
#define CYPHER_U1_RXD   18

/*===========================================================================
 *  POWER PINS
 *===========================================================================*/
#define CYPHER_PIN_BATTERY   45   // ADC-capable pin for battery voltage sensing

/*===========================================================================
 *  TYPES & ENUMS
 *===========================================================================*/

/**
 * @brief RGB LED colour preset helpers.
 */
enum CypherLedColor
{
    CYPHER_COLOR_OFF    = 0,
    CYPHER_COLOR_RED    = 1,
    CYPHER_COLOR_GREEN  = 2,
    CYPHER_COLOR_BLUE   = 3,
    CYPHER_COLOR_YELLOW = 4,
    CYPHER_COLOR_CYAN   = 5,
    CYPHER_COLOR_MAGENTA= 6,
    CYPHER_COLOR_WHITE  = 7
};

/**
 * @brief SD card interface mode.
 */
enum CypherSDMode
{
    CYPHER_SD_SPI   = 0,   // 1-bit SPI (compatibility)
    CYPHER_SD_SDIO  = 1    // 4-bit SDIO (high speed)
};

/*===========================================================================
 *  STRUCTS
 *===========================================================================*/

/**
 * @brief Board identity / revision.
 */
struct CypherInfo
{
    const char *board_name;
    const char *version;
    const char *mcu;
};

/*===========================================================================
 *  PCBCUPID_Cypher CLASS
 *===========================================================================*/

/**
 * @class PCBCUPID_Cypher
 * @brief Board-support library for the PCBCUPID Cypher (ESP32-S3-MINI-1).
 *
 * Wraps the common peripherals on the Cypher board:
 *   - 0.96" TFT ST7735 display  (SPI)
 *   - MicroSD card               (SDIO / SPI)
 *   - PCF8563T real-time clock   (I2C)
 *   - RGB LED                    (GPIO, active-low)
 *   - Red status LED             (GPIO, active-low)
 *   - Boot & Reset buttons       (GPIO)
 *
 * Usage:
 * @code
 *   #include <PCBCUPID_Cypher.h>
 *
 *   TwoWire I2Cbus(0);
 *   PCBCUPID_Cypher cypher(I2Cbus);
 *
 *   void setup() {
 *       Serial.begin(115200);
 *       cypher.begin();     // starts I2C, inits RTC, configures LEDs
 *       cypher.setRGBLed(CYPHER_COLOR_GREEN);
 *   }
 * @endcode
 */
class PCBCUPID_Cypher
{
public:
    /*---- constructor ------------------------------------------------*/
    PCBCUPID_Cypher(TwoWire &wire = Wire);

    /*---- initialisation ---------------------------------------------*/
    /**
     * @brief  Full board initialisation.
     * @return true on success, false if a critical peripheral failed.
     */
    bool begin();

    /**
     * @brief  Lightweight init â€“ only I2C, GPIOs, and RTC.
     *         Skips display and SD card (useful for low-power states).
     * @return true on success.
     */
    bool beginLight();

    /*---- board information ------------------------------------------*/
    CypherInfo getInfo() const;

    /** @return true if the RTC (PCF8563T) was detected and is usable. */
    bool isRTCReady() const;

    /** @return true if an SD card was successfully mounted. */
    bool isSDReady() const;

    /** @return true if the TFT was successfully initialised. */
    bool isTFTReady() const;

    /*---- RGB LED ----------------------------------------------------*/
    /**
     * @brief  Set the RGB LED to a predefined colour.
     * @note   The LED is common-anode, active-low.
     *         LOW = ON, HIGH = OFF.
     */
    void setRGBLed(CypherLedColor color);

    /**
     * @brief  Set the RGB LED with raw PWM values (8-bit each).
     * @param  r  Red   duty (0 = fully ON,  255 = OFF).
     * @param  g  Green duty (0 = fully ON,  255 = OFF).
     * @param  b  Blue  duty (0 = fully ON,  255 = OFF).
     */
    void setRGBLedRaw(uint8_t r, uint8_t g, uint8_t b);

    /*---- status LED -------------------------------------------------*/
    void setStatusLed(bool on);
    void toggleStatusLed();

    /*---- buttons ----------------------------------------------------*/
    /** @return true if Boot button (SW2 / 0) is currently pressed. */
    bool isBootPressed() const;

    /*---- power / sleep -----------------------------------------------*/
    /**
     * @brief  Read the battery voltage via ADC (45).
     * @param  dividerRatio  External voltage-divider ratio (R1+R2)/R2.
     *                       Default is 1.0 (direct connection).
     * @return Battery voltage in volts, or 0.0 on unsupported hardware.
     * @note   The Cypher board does NOT have an onboard divider.
     *         Add one externally if your battery exceeds ~1.1 V.
     */
    float readBatteryVoltage(float dividerRatio);

    /**
     * @brief  Read battery voltage with the default divider (1:1).
     * @return Battery voltage in volts.
     */
    float readBatteryVoltage();

    /**
     * @brief  Put the board into deep sleep.
     * @param  sleepMs      Sleep duration in milliseconds (0 = forever).
     * @param  wakeOnBoot   If true, BOOT button (0 LOW) will wake.
     */
    void deepSleep(uint64_t sleepMs = 0, bool wakeOnBoot = true);

    /** @brief  Trigger a software reset (ESP.restart()). */
    void reset();

    /*---- RTC (PCF8563T) ---------------------------------------------*/
    /**
     * @brief  Read the current date-time from the RTC.
     * @param  year    (out)  full year (e.g. 2026).
     * @param  month   (out)  1â€“12.
     * @param  day     (out)  1â€“31.
     * @param  weekday (out)  0â€“6 (Sun=0).
     * @param  hour    (out)  0â€“23.
     * @param  minute  (out)  0â€“59.
     * @param  second  (out)  0â€“59.
     * @return true if read succeeded.
     */
    bool readRTC(uint16_t &year, uint8_t &month, uint8_t &day,
                 uint8_t &weekday, uint8_t &hour, uint8_t &minute, uint8_t &second);

    /**
     * @brief  Set the RTC date-time.
     * @return true on success.
     */
    bool setRTC(uint16_t year, uint8_t month, uint8_t day,
                uint8_t weekday, uint8_t hour, uint8_t minute, uint8_t second);

    /*---- SD Card ----------------------------------------------------*/
    /**
     * @brief  Initialise the MicroSD card in SDIO (4-bit) mode.
     * @return true on success.
     */
    bool beginSD();

    /**
     * @brief  Initialise the MicroSD card in SPI mode (compatibility).
     * @param  csPin  Chip-select pin (default = 35).
     * @return true on success.
     */
    bool beginSD_SPI(uint8_t csPin = CYPHER_SD_CMD);

    /*---- TFT Display ------------------------------------------------*/
    /**
     * @brief  Initialise the TFT display.
     * @note   Requires Adafruit_ST7735 and Adafruit_GFX to be installed
     *         separately. This method provides the pin configuration
     *         ready-to-use.
     * @return true on success.
     */
    bool beginTFT();

    /*---- I2C helpers ------------------------------------------------*/
    TwoWire &getWire() { return _wire; }

    /** @brief Scan the I2C bus, print devices to Serial. */
    void scanI2C();

private:
    TwoWire &_wire;

    /* RTC low-level helpers (PCF8563T) */
    uint8_t _bcd2dec(uint8_t bcd);
    uint8_t _dec2bcd(uint8_t dec);
    bool    _rtcWriteReg(uint8_t reg, uint8_t val);
    bool    _rtcReadReg(uint8_t reg, uint8_t &val);
    bool    _rtcReadBurst(uint8_t startReg, uint8_t *buf, uint8_t len);

    /* init flags */
    bool _rtc_ok;
    bool _sd_ok;
    bool _tft_ok;
};


/*===========================================================================
 *  PCBCUPID_Cypher_tft CLASS  –  ST7735 Display Wrapper
 *  (only available when Adafruit ST7735 library is installed)
 *===========================================================================*/

#if CYPHER_HAS_TFT_LIBRARY

/**
 * @class PCBCUPID_Cypher_tft
 * @brief Pre-configured ST7735 TFT wrapper for the PCBCUPID Cypher board.
 *
 * Inherits all Adafruit_ST7735 and Adafruit_GFX drawing functions.
 * Pins (CS, DC, RST) are taken from the board definition automatically.
 *
 * Usage:
 * @code
 *   PCBCUPID_Cypher_tft tft;
 *   tft.init();
 *   tft.fillScreen(ST7735_BLACK);
 *   tft.setTextColor(ST7735_WHITE);
 *   tft.println("Hello Cypher!");
 * @endcode
 */
class PCBCUPID_Cypher_tft : public Adafruit_ST7735
{
public:
    /**
     * @brief  Construct with Cypher board pins (CS=10, DC=9, RST=8).
     */
    PCBCUPID_Cypher_tft();

    /**
     * @brief  Initialise the display.
     *         Configures hardware SPI (SCLK=12, MOSI=11), performs
     *         hardware reset, and initialises the ST7735R controller
     *         for the 0.96" 80x160 display.
     */
    void init();
};

#endif  // CYPHER_HAS_TFT_LIBRARY

#endif // PCBCUPID_CYPHER_H
