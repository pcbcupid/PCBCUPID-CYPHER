/**
 * @file sd_card.ino
 * @brief MicroSD card verification demo for the PCBCUPID Cypher board.
 *
 * Tests:
 *   - SD card initialisation (SDIO 4-bit mode)
 *   - Card info (type, size, free space)
 *   - Root directory listing
 *   - File write → read-back → content verify
 *
 * All results reported on Serial Monitor @ 115200 baud.
 * RGB LED:  BLUE = testing,  GREEN = all pass,  RED = fail.
 *
 * Hardware required: PCBCUPID Cypher V-2.0 + FAT32 MicroSD card
 */

#include <Wire.h>
#include <PCBCUPID_Cypher.h>
#include <SD_MMC.h>

// ── Objects ──────────────────────────────────────────────────────────
TwoWire I2Cbus(0);
PCBCUPID_Cypher cypher(I2Cbus);

// ── Test results ─────────────────────────────────────────────────────
bool test_mount   = false;
bool test_info    = false;
bool test_list    = false;
bool test_rw      = false;

// ── Setup ────────────────────────────────────────────────────────────
void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("=========================================="));
    Serial.println(F("  PCBCUPID Cypher – SD Card Verification"));
    Serial.println(F("=========================================="));

    // Board init
    cypher.begin();
    cypher.setRGBLed(CYPHER_COLOR_BLUE);  // BLUE = testing

    // ── Test 1: SDIO mount ─────────────────────────────────────────
    Serial.print(F("\n[1] SD card mount (SDIO 4-bit) ... "));
    if (cypher.beginSD())
    {
        Serial.println(F("PASS"));
        test_mount = true;
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

    // ── Test 2: Card info ──────────────────────────────────────────
    Serial.print(F("\n[2] Card info ... "));
    test_info = printCardInfo();

    // ── Test 3: Directory listing ──────────────────────────────────
    Serial.print(F("\n[3] Root directory ... "));
    test_list = listDir("/", 2);

    // ── Test 4: Write + Read + Verify ──────────────────────────────
    Serial.print(F("\n[4] File write/read/verify ... "));
    test_rw = writeReadVerify();

    // ── Summary ────────────────────────────────────────────────────
    printSummary();

    // LED: all-green if every test passed, red if any failed
    if (test_mount && test_info && test_list && test_rw)
    {
        cypher.setRGBLed(CYPHER_COLOR_GREEN);
        Serial.println(F("\nALL TESTS PASSED."));
    }
    else
    {
        cypher.setRGBLed(CYPHER_COLOR_RED);
        Serial.println(F("\nSOME TESTS FAILED – see above."));
    }
}

// ── Loop ─────────────────────────────────────────────────────────────
void loop()
{
    // Heartbeat: status LED blinks slowly to show we're alive
    cypher.toggleStatusLed();
    delay(1000);
}

// ── Helpers ──────────────────────────────────────────────────────────

bool printCardInfo()
{
    uint8_t  cardType = SD_MMC.cardType();
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);  // MB
    uint64_t total    = SD_MMC.totalBytes();
    uint64_t used     = SD_MMC.usedBytes();

    Serial.println(F("PASS"));
    Serial.print(F("     Card type : "));
    if      (cardType == CARD_MMC)  Serial.println(F("MMC"));
    else if (cardType == CARD_SD)   Serial.println(F("SDSC"));
    else if (cardType == CARD_SDHC) Serial.println(F("SDHC"));
    else                            Serial.println(F("UNKNOWN"));

    Serial.printf("     Size      : %llu MB\n", cardSize);
    Serial.printf("     Total     : %llu bytes\n", total);
    Serial.printf("     Used      : %llu bytes\n", used);
    Serial.printf("     Free      : %llu bytes\n", total - used);

    return (cardSize > 0);
}

bool listDir(const char *dirname, uint8_t levels)
{
    File root = SD_MMC.open(dirname);
    if (!root)
    {
        Serial.println(F("FAIL – cannot open root"));
        return false;
    }
    if (!root.isDirectory())
    {
        Serial.println(F("FAIL – root is not a directory"));
        root.close();
        return false;
    }

    Serial.println(F("PASS"));

    uint16_t fileCount = 0;
    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            Serial.printf("     [DIR]  %s/\n", file.name());
            if (levels > 0) listDir(file.path(), levels - 1);
        }
        else
        {
            Serial.printf("     [FILE] %-28s %8u B\n", file.name(), file.size());
            fileCount++;
        }
        file = root.openNextFile();
    }
    root.close();

    if (fileCount == 0)
        Serial.println(F("     (empty directory)"));
    else
        Serial.printf("     %u file(s) listed\n", fileCount);

    return true;
}

bool writeReadVerify()
{
    const char *path = "/cypher_test.txt";
    const char *msg  = "Hello from PCBCUPID Cypher!\nSD card working.\n";
    size_t msgLen    = strlen(msg);

    // Write
    File f = SD_MMC.open(path, FILE_WRITE);
    if (!f)
    {
        Serial.println(F("FAIL – cannot open file for writing"));
        return false;
    }
    size_t written = f.print(msg);
    f.close();

    if (written != msgLen)
    {
        Serial.printf("FAIL – wrote %u of %u bytes\n", written, msgLen);
        return false;
    }
    Serial.printf("wrote %u B ... ", written);

    // Read back
    f = SD_MMC.open(path);
    if (!f)
    {
        Serial.println(F("FAIL – cannot open file for reading"));
        return false;
    }

    char buf[256] = {0};
    size_t bytesRead = 0;
    while (f.available() && bytesRead < sizeof(buf) - 1)
    {
        buf[bytesRead++] = f.read();
    }
    f.close();

    // Verify content matches
    if (bytesRead != msgLen)
    {
        Serial.printf("FAIL – read %u B, expected %u B\n", bytesRead, msgLen);
        return false;
    }

    if (strncmp(buf, msg, msgLen) != 0)
    {
        Serial.println(F("FAIL – content mismatch!"));
        Serial.print(F("     Expected: "));
        Serial.println(msg);
        Serial.print(F("     Got:      "));
        Serial.println(buf);
        return false;
    }

    Serial.println(F("PASS – content verified"));

    // Clean up
    SD_MMC.remove(path);
    Serial.println(F("     (test file deleted)"));

    return true;
}

void printSummary()
{
    Serial.println(F("\n=========================================="));
    Serial.println(F("  VERIFICATION SUMMARY"));
    Serial.println(F("=========================================="));
    Serial.printf("  SD mount    : %s\n", pf(test_mount));
    Serial.printf("  Card info   : %s\n", pf(test_info));
    Serial.printf("  Dir listing : %s\n", pf(test_list));
    Serial.printf("  File R/W    : %s\n", pf(test_rw));
    Serial.println(F("=========================================="));
}

const char* pf(bool ok)
{
    return ok ? "PASS" : "FAIL";
}
