/**
 * @file display_test.ino
 * @brief TFT display verification demo for the PCBCUPID Cypher board.
 *
 * Runs a visual test sequence on the 0.96" ST7735 display:
 *   1. Screen fill: black → red → green → blue → black
 *   2. Text rendering (various colours & sizes)
 *   3. Lines & rectangles
 *   4. Live RTC clock
 *
 * All test steps are reported on Serial Monitor @ 115200 baud.
 *
 * Hardware required: PCBCUPID Cypher V-2.0
 */

#include <Wire.h>
#include <SPI.h>
#include <PCBCUPID_Cypher.h>

// ── Objects ──────────────────────────────────────────────────────────
TwoWire I2Cbus(0);
PCBCUPID_Cypher cypher(I2Cbus);

// Pre-configured ST7735 — pins (CS, DC, RST) are baked in
PCBCUPID_Cypher_tft tft;

// ── Setup ────────────────────────────────────────────────────────────
void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("=========================================="));
    Serial.println(F("  PCBCUPID Cypher – Display Verification"));
    Serial.println(F("=========================================="));

    // ── Board init ─────────────────────────────────────────────────
    Serial.print(F("\n[1] Board initialisation ... "));
    cypher.begin();
    Serial.println(F("OK"));

    // ── TFT init ───────────────────────────────────────────────────
    Serial.print(F("[2] TFT initialisation (ST7735, HW SPI) ... "));
    tft.init();
    Serial.println(F("OK"));

    // ── Visual test sequence ───────────────────────────────────────
    Serial.println(F("\nRunning visual tests on the display...\n"));

    // Test A: Screen fills
    Serial.print(F("  [A] Screen fill colours ... "));
    tft.fillScreen(ST77XX_RED);     delay(400);
    tft.fillScreen(ST77XX_GREEN);   delay(400);
    tft.fillScreen(ST77XX_BLUE);    delay(400);
    tft.fillScreen(ST77XX_BLACK);   delay(200);
    Serial.println(F("PASS (check display)"));

    // Test B: Text rendering
    Serial.print(F("  [B] Text rendering ... "));
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(8, 4);
    tft.println(F("PCBCUPID"));
    tft.setCursor(18, 16);
    tft.println(F("Cypher"));
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(8, 36);
    tft.println(F("TFT OK!"));
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(2, 52);
    tft.setTextSize(1);
    tft.println(F("80x160 ST7735"));
    Serial.println(F("PASS (check display)"));
    delay(600);

    // Test C: Coloured rectangles
    Serial.print(F("  [C] Rectangle bars ... "));
    tft.fillRect(0,  68, 80, 12, ST77XX_RED);
    tft.fillRect(0,  80, 80, 12, ST77XX_GREEN);
    tft.fillRect(0,  92, 80, 12, ST77XX_BLUE);
    tft.fillRect(0, 104, 80, 12, ST77XX_CYAN);
    Serial.println(F("PASS (check display)"));
    delay(800);

    // Clear the bars area
    tft.fillRect(0, 68, 80, 52, ST77XX_BLACK);

    // Test D: Lines (cross pattern)
    Serial.print(F("  [D] Cross lines ... "));
    tft.drawLine(0,  68, 79, 119, ST77XX_WHITE);
    tft.drawLine(79, 68, 0,  119, ST77XX_WHITE);
    tft.drawRect(10, 73, 60, 41, ST77XX_MAGENTA);
    Serial.println(F("PASS (check display)"));
    delay(800);

    tft.fillRect(0, 68, 80, 52, ST77XX_BLACK);

    // Test E: Pixels / dots pattern
    Serial.print(F("  [E] Dot grid ... "));
    for (uint8_t x = 10; x < 75; x += 12)
        for (uint8_t y = 72; y < 116; y += 10)
            tft.drawPixel(x, y, ST77XX_GREEN);
    Serial.println(F("PASS (check display)"));
    delay(800);

    // ── Final screen: clock ready ──────────────────────────────────
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(4, 2);
    tft.println(F("Tests done!"));
    tft.setCursor(4, 14);
    tft.println(F("Clock starting"));

    // Set RTC
    cypher.setRTC(2026, 6, 24, 3, 12, 0, 0);
    Serial.println(F("\n  RTC set to 2026-06-24 12:00:00"));

    // ── Summary ────────────────────────────────────────────────────
    Serial.println(F("\n=========================================="));
    Serial.println(F("  VERIFICATION SUMMARY"));
    Serial.println(F("=========================================="));
    Serial.println(F("  Board init   : PASS"));
    Serial.println(F("  TFT init     : PASS"));
    Serial.println(F("  Fill colours : VISUAL  (red→green→blue)"));
    Serial.println(F("  Text         : VISUAL  (\"PCBCUPID Cypher\")"));
    Serial.println(F("  Rectangles   : VISUAL  (4 coloured bars)"));
    Serial.println(F("  Lines        : VISUAL  (cross + box)"));
    Serial.println(F("  Dot grid     : VISUAL  (green dots)"));
    Serial.println(F("=========================================="));
    Serial.println(F("\nLive clock running. Status LED = heartbeat.\n"));
}

// ── Loop ─────────────────────────────────────────────────────────────
void loop()
{
    static unsigned long lastUpdate = 0;
    static unsigned long lastLED    = 0;
    unsigned long now = millis();

    // Update clock every 1000 ms
    if (now - lastUpdate >= 1000)
    {
        lastUpdate = now;

        uint16_t year;
        uint8_t  month, day, wday, hour, minute, second;

        if (cypher.readRTC(year, month, day, wday, hour, minute, second))
        {
            // Clock area: rows 30–119
            tft.fillRect(0, 30, 80, 90, ST77XX_BLACK);

            // Time in large text
            char buf[24];
            snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                     hour, minute, second);
            tft.setTextColor(ST77XX_CYAN);
            tft.setTextSize(2);
            tft.setCursor(8, 50);
            tft.println(buf);

            // Date below
            tft.setTextSize(1);
            tft.setCursor(4, 80);
            snprintf(buf, sizeof(buf), "%04u-%02u-%02u", year, month, day);
            tft.println(buf);

            // Day of week
            const char* wdayNames[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
            tft.setCursor(4, 95);
            tft.println(wdayNames[wday % 7]);
        }
    }

    // Heartbeat: toggle status LED every 500 ms
    if (now - lastLED >= 500)
    {
        lastLED = now;
        cypher.toggleStatusLed();
    }

    delay(50);
}
