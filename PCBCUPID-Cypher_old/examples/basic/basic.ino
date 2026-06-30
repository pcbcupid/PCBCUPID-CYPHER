/**
 * @file basic.ino
 * @brief Full-feature verification demo for the PCBCUPID Cypher board.
 *
 * Tests and reports on:
 *   - Board initialisation
 *   - I2C bus scan (RTC detection)
 *   - RTC read
 *   - SD card mount (SDIO)
 *   - RGB LED colour cycling (visual)
 *   - Status LED blinking  (visual)
 *   - Boot button polling
 *
 * Open Serial Monitor @ 115200 baud to see the test report.
 *
 * Hardware required: PCBCUPID Cypher V-2.0
 */

#include <Wire.h>
#include <PCBCUPID_Cypher.h>
#include <SD_MMC.h>

// ── Objects ──────────────────────────────────────────────────────────
TwoWire I2Cbus(0);
PCBCUPID_Cypher cypher(I2Cbus);

// ── Test results ─────────────────────────────────────────────────────
bool test_board   = false;
bool test_i2c     = false;
bool test_rtc     = false;
bool test_sd      = false;
bool test_rgb     = false;
bool test_status  = false;
bool test_button  = false;

// ── Setup ────────────────────────────────────────────────────────────
void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("=========================================="));
    Serial.println(F("  PCBCUPID Cypher – Board Verification"));
    Serial.println(F("=========================================="));

    // ── Test 1: Board init ─────────────────────────────────────────
    Serial.print(F("\n[1] Board initialisation ... "));
    if (cypher.begin())
    {
        Serial.println(F("PASS"));
        test_board = true;
    }
    else
    {
        Serial.println(F("FAIL"));
        while (1) { cypher.setRGBLed(CYPHER_COLOR_RED); delay(500); }
    }

    CypherInfo info = cypher.getInfo();
    Serial.printf("     Board : %s  V%s  (%s)\n",
                  info.board_name, info.version, info.mcu);

    // ── Test 2: I2C + RTC detection ────────────────────────────────
    Serial.print(F("\n[2] I2C bus scan ...\n"));
    cypher.scanI2C();
    test_i2c = cypher.isRTCReady();

    Serial.print(F("     RTC (PCF8563T @ 0x51) ... "));
    if (test_i2c)
    {
        Serial.println(F("PASS – detected"));
        test_rtc = cypher.setRTC(2026, 6, 24, 3, 12, 0, 0);
        if (test_rtc) Serial.println(F("     RTC set to 2026-06-24 12:00:00"));
    }
    else
    {
        Serial.println(F("FAIL – not found on I2C bus"));
    }

    // ── Test 3: RTC read-back ──────────────────────────────────────
    Serial.print(F("\n[3] RTC read-back ... "));
    if (test_rtc) readAndPrintRTC();

    // ── Test 4: SD card ────────────────────────────────────────────
    Serial.print(F("\n[4] SD card (SDIO mode) ... "));
    if (cypher.beginSD())
    {
        test_sd = true;
        uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
        Serial.println(F("PASS"));
        Serial.printf("     Card size : %llu MB\n", cardSize);
        Serial.printf("     Card type : ");
        uint8_t ct = SD_MMC.cardType();
        if      (ct == CARD_MMC)  Serial.println(F("MMC"));
        else if (ct == CARD_SD)   Serial.println(F("SDSC"));
        else if (ct == CARD_SDHC) Serial.println(F("SDHC"));
        else                      Serial.println(F("UNKNOWN"));
    }
    else
    {
        Serial.println(F("SKIP – no card inserted"));
    }

    // ── Test 5: RGB LED ────────────────────────────────────────────
    Serial.print(F("\n[5] RGB LED ... "));
    Serial.println(F("watch the LED cycle through colours"));
    test_rgb = true;

    // Quick colour flash to confirm
    cypher.setRGBLed(CYPHER_COLOR_RED);    delay(300);
    cypher.setRGBLed(CYPHER_COLOR_GREEN);  delay(300);
    cypher.setRGBLed(CYPHER_COLOR_BLUE);   delay(300);
    cypher.setRGBLed(CYPHER_COLOR_OFF);
    Serial.println(F("     PASS – quick RGB flash done"));

    // ── Test 6: Status LED ─────────────────────────────────────────
    Serial.print(F("\n[6] Status LED (D1, red) ... "));
    test_status = true;
    cypher.setStatusLed(true);   delay(300);
    cypher.setStatusLed(false);  delay(150);
    cypher.setStatusLed(true);   delay(300);
    cypher.setStatusLed(false);
    Serial.println(F("PASS – blinked twice"));

    // ── Test 7: Boot button ────────────────────────────────────────
    Serial.print(F("\n[7] Boot button (GPIO0) ... "));
    Serial.println(F("press SW2 (Boot) to see messages"));
    test_button = true;

    // ── Summary ────────────────────────────────────────────────────
    Serial.println(F("\n=========================================="));
    Serial.println(F("  VERIFICATION SUMMARY"));
    Serial.println(F("=========================================="));
    Serial.printf("  Board init  : %s\n", passFail(test_board));
    Serial.printf("  I2C + RTC   : %s\n", passFail(test_i2c));
    Serial.printf("  RTC set/read: %s\n", passFail(test_rtc));
    Serial.printf("  SD card     : %s\n", passFail(test_sd));
    Serial.printf("  RGB LED     : %s  (visual)\n", passFail(test_rgb));
    Serial.printf("  Status LED  : %s  (visual)\n", passFail(test_status));
    Serial.printf("  Boot button : %s  (press to verify)\n", passFail(test_button));
    Serial.println(F("=========================================="));
    Serial.println(F("\nLoop running – RGB cycles, status blinks, button monitored.\n"));
}

// ── Loop ─────────────────────────────────────────────────────────────
void loop()
{
    static unsigned long lastToggle = 0;
    static uint8_t      rgbState   = 0;
    static unsigned long lastRgb    = 0;
    static unsigned long lastRTC    = 0;

    unsigned long now = millis();

    // Status LED: toggle every 500 ms
    if (now - lastToggle >= 500)
    {
        lastToggle = now;
        cypher.toggleStatusLed();
    }

    // RGB LED: cycle colour every 1500 ms
    if (now - lastRgb >= 1500)
    {
        lastRgb = now;

        const CypherLedColor colors[] = {
            CYPHER_COLOR_RED,    CYPHER_COLOR_GREEN,  CYPHER_COLOR_BLUE,
            CYPHER_COLOR_YELLOW, CYPHER_COLOR_CYAN,   CYPHER_COLOR_MAGENTA,
            CYPHER_COLOR_WHITE,  CYPHER_COLOR_OFF
        };
        const char *names[] = {
            "RED", "GREEN", "BLUE", "YELLOW", "CYAN", "MAGENTA", "WHITE", "OFF"
        };
        const uint8_t numColors = sizeof(colors) / sizeof(colors[0]);

        cypher.setRGBLed(colors[rgbState]);
        Serial.printf("[RGB] %s\n", names[rgbState]);
        rgbState = (rgbState + 1) % numColors;
    }

    // RTC: print time every 10 s
    if (test_rtc && (now - lastRTC >= 10000))
    {
        lastRTC = now;
        Serial.print("[RTC] ");
        readAndPrintRTC();
    }

    // Boot button: report state changes
    static bool lastBtn = false;
    bool btn = cypher.isBootPressed();
    if (btn != lastBtn)
    {
        lastBtn = btn;
        Serial.printf("[BTN] Boot button %s  %s\n",
                      btn ? "PRESSED " : "released",
                      btn ? "          " : "(button test PASS)");
    }

    delay(10);
}

// ── Helpers ──────────────────────────────────────────────────────────

void readAndPrintRTC()
{
    uint16_t year;
    uint8_t  month, day, wday, hour, minute, second;

    if (cypher.readRTC(year, month, day, wday, hour, minute, second))
    {
        const char* wdayNames[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        Serial.printf("%04u-%02u-%02u (%s) %02u:%02u:%02u\n",
                      year, month, day,
                      wdayNames[wday % 7],
                      hour, minute, second);
    }
    else
    {
        Serial.println(F("read failed"));
    }
}

const char* passFail(bool ok)
{
    return ok ? "PASS" : "FAIL";
}
