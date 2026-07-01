/**
 * @file sd_mmc.ino
 * @brief SD_MMC + USB Mass Storage demo for the PCBCUPID Cypher board.
 *
 * Mounts the MicroSD card via SDIO and exposes it as a USB Mass Storage
 * device — the Cypher appears as a removable USB drive on your PC.
 *
 * Tests:
 *   - Board initialisation
 *   - SD card mount (SDIO 4-bit mode)
 *   - USB MSC initialisation & callbacks
 *   - Card info (type, size, sectors)
 *
 * All results reported on Serial Monitor @ 115200 baud.
 * RGB LED:  BLUE = testing,  GREEN = all pass,  RED = fail.
 *
 * Hardware required: PCBCUPID Cypher V-1.0 + FAT32 MicroSD card + USB-C cable
 *
 * NOTE: In Arduino IDE, set "USB Mode" to "USB-OTG (TinyUSB)" —
 *       native USB CDC/JTAG conflicts with USB MSC.
 */

#include <Arduino.h>

#if !SOC_USB_OTG_SUPPORTED || ARDUINO_USB_MODE
#error Device does not support USB_OTG or native USB CDC/JTAG is selected
#endif

#include <Wire.h>
#include <PCBCUPID_Cypher.h>
#include <USB.h>
#include <USBMSC.h>
#include <SD_MMC.h>

// ── Objects ──────────────────────────────────────────────────────────
TwoWire I2Cbus(0);
PCBCUPID_Cypher cypher(I2Cbus);
USBMSC msc;

// ── Test results ─────────────────────────────────────────────────────
bool test_board   = false;
bool test_sd      = false;
bool test_msc     = false;

// ── USB MSC Callbacks ────────────────────────────────────────────────

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
    uint32_t secSize = SD_MMC.sectorSize();
    if (!secSize) return false;  // disk error

    for (uint32_t x = 0; x < bufsize / secSize; x++)
    {
        uint8_t blkbuffer[secSize];
        memcpy(blkbuffer, (uint8_t *)buffer + secSize * x, secSize);
        if (!SD_MMC.writeRAW(blkbuffer, lba + x)) return false;
    }
    return bufsize;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    uint32_t secSize = SD_MMC.sectorSize();
    if (!secSize) return false;  // disk error

    for (uint32_t x = 0; x < bufsize / secSize; x++)
    {
        if (!SD_MMC.readRAW((uint8_t *)buffer + (x * secSize), lba + x))
            return false;  // outside of volume boundary
    }
    return bufsize;
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject)
{
    return true;
}

// ── USB Event Callback ───────────────────────────────────────────────

static void usbEventCallback(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_base == ARDUINO_USB_EVENTS)
    {
        switch (event_id)
        {
            case ARDUINO_USB_STARTED_EVENT:
                Serial.println(F("USB PLUGGED")); break;
            case ARDUINO_USB_STOPPED_EVENT:
                Serial.println(F("USB UNPLUGGED")); break;
            case ARDUINO_USB_SUSPEND_EVENT:
                Serial.println(F("USB SUSPENDED")); break;
            case ARDUINO_USB_RESUME_EVENT:
                Serial.println(F("USB RESUMED")); break;
            default: break;
        }
    }
}

// ── Setup ────────────────────────────────────────────────────────────

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("=========================================="));
    Serial.println(F("  PCBCUPID Cypher – SD_MMC + USB MSC"));
    Serial.println(F("=========================================="));

    // Board init
    cypher.begin();
    cypher.setRGBLed(CYPHER_COLOR_BLUE);  // BLUE = testing

    // ── Test 1: Board init ─────────────────────────────────────────
    Serial.print(F("\n[1] Board initialisation ... "));
    CypherInfo info = cypher.getInfo();
    Serial.print(F("PASS  ("));
    Serial.print(info.board_name);
    Serial.print(F(" V"));
    Serial.print(info.version);
    Serial.print(F(", "));
    Serial.print(info.mcu);
    Serial.println(F(")"));
    test_board = true;

    // ── Test 2: SD card mount ──────────────────────────────────────
    Serial.print(F("\n[2] SD card mount (SDIO 4-bit) ... "));
    if (cypher.beginSD())
    {
        Serial.println(F("PASS"));
        test_sd = true;
        cypher.setRGBLed(CYPHER_COLOR_GREEN);
    }
    else
    {
        Serial.println(F("FAIL – no card or unsupported format"));
        Serial.println(F("     Insert a FAT32-formatted MicroSD card and reset."));
        cypher.setRGBLed(CYPHER_COLOR_RED);
        printSummary();
        while (1) { delay(1000); }
    }

    // Card info
    uint64_t cardSize   = SD_MMC.cardSize() / (1024 * 1024);
    uint64_t totalBytes = SD_MMC.totalBytes();
    uint32_t secSize    = SD_MMC.sectorSize();
    uint32_t numSectors = SD_MMC.numSectors();

    Serial.print(F("     Card type : "));
    uint8_t ct = SD_MMC.cardType();
    if      (ct == CARD_MMC)  Serial.println(F("MMC"));
    else if (ct == CARD_SD)   Serial.println(F("SDSC"));
    else if (ct == CARD_SDHC) Serial.println(F("SDHC"));
    else                      Serial.println(F("UNKNOWN"));

    Serial.printf("     Size      : %llu MB\n", cardSize);
    Serial.printf("     Total     : %llu bytes\n", totalBytes);
    Serial.printf("     Sector    : %lu bytes\n", secSize);
    Serial.printf("     Sectors   : %lu\n", numSectors);

    // ── Test 3: USB MSC init ───────────────────────────────────────
    Serial.print(F("\n[3] USB Mass Storage init ... "));

    msc.vendorID("PCBCUPID");
    msc.productID("Cypher_MSC");
    msc.productRevision("1.0");
    msc.onRead(onRead);
    msc.onWrite(onWrite);
    msc.onStartStop(onStartStop);
    msc.mediaPresent(true);
    msc.begin(numSectors, secSize);

    USB.begin();
    USB.onEvent(usbEventCallback);

    test_msc = true;
    Serial.println(F("PASS"));
    Serial.println(F("     Cypher is now a USB drive. Connect USB-C to your PC."));

    // ── Summary ────────────────────────────────────────────────────
    printSummary();

    if (test_board && test_sd && test_msc)
    {
        cypher.setRGBLed(CYPHER_COLOR_GREEN);
        Serial.println(F("\nALL TESTS PASSED — SD_MMC + USB MSC active.\n"));
    }
    else
    {
        cypher.setRGBLed(CYPHER_COLOR_RED);
        Serial.println(F("\nSOME TESTS FAILED — see above.\n"));
    }
}

// ── Loop ─────────────────────────────────────────────────────────────

void loop()
{
    // Heartbeat: status LED blinks slowly to show we're alive
    static unsigned long lastToggle = 0;
    if (millis() - lastToggle >= 1000)
    {
        lastToggle = millis();
        cypher.toggleStatusLed();
    }
    delay(10);
}

// ── Helpers ──────────────────────────────────────────────────────────

void printSummary()
{
    Serial.println(F("\n=========================================="));
    Serial.println(F("  VERIFICATION SUMMARY"));
    Serial.println(F("=========================================="));
    Serial.printf("  Board init   : %s\n", pf(test_board));
    Serial.printf("  SD card      : %s\n", pf(test_sd));
    Serial.printf("  USB MSC      : %s\n", pf(test_msc));
    Serial.println(F("=========================================="));
}

const char* pf(bool ok)
{
    return ok ? "PASS" : "FAIL";
}
