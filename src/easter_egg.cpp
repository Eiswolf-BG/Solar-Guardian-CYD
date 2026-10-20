#include "easter_egg.h"
#include "config.h"
#include <SD.h>

namespace EasterEgg {

namespace {
    // Unauffaelliger Dateiname, lebt in Config::SD_HISTORY_DIR (nicht
    // /SolarGuardian), damit "Zuruecksetzen" ihn nicht mit entfernt.
    constexpr const char* CERT_FILE = "/SolarHistory/sg.cert";
    constexpr const char* CERT_TMP  = "/SolarHistory/sg.cert.tmp";
    // Fester Text + Pruefsumme, damit eine leere/zufaellige Datei nicht
    // versehentlich greift.
    constexpr const char* MAGIC = "SGCERT1";
    constexpr uint32_t TAP_WINDOW_MS = 1500;
    constexpr uint8_t TAP_GOAL = 6;

    uint8_t tapCount = 0;
    uint32_t lastTapMs = 0;

    uint16_t simpleChecksum(const char* s) {
        uint16_t sum = 0;
        while (*s) {
            sum = (uint16_t)((sum << 1) ^ (uint8_t)*s);
            s++;
        }
        return sum;
    }

    // Sicher schreiben: erst temp-Datei, dann alte entfernen, dann
    // umbenennen (gleiches Muster wie yield.json/years.json).
    bool writeCertificateInternal() {
        if (SD.exists(CERT_TMP)) SD.remove(CERT_TMP);
        File f = SD.open(CERT_TMP, FILE_WRITE);
        if (!f) return false;
        uint16_t sum = simpleChecksum(MAGIC);
        char sumHex[8];
        snprintf(sumHex, sizeof(sumHex), "%04X", sum);
        f.printf("%s\n%s\n", MAGIC, sumHex);
        f.close();
        if (SD.exists(CERT_FILE)) SD.remove(CERT_FILE);
        return SD.rename(CERT_TMP, CERT_FILE);
    }

    bool removeCertificateInternal() {
        if (!SD.exists(CERT_FILE)) return true; // schon "aus"
        return SD.remove(CERT_FILE);
    }
}

bool certificateValid() {
    if (!SD.exists(CERT_FILE)) return false;
    File f = SD.open(CERT_FILE, FILE_READ);
    if (!f) return false;
    String magicLine = f.readStringUntil('\n');
    String sumLine = f.readStringUntil('\n');
    f.close();
    magicLine.trim();
    sumLine.trim();
    if (magicLine != MAGIC) return false;
    uint16_t expected = simpleChecksum(MAGIC);
    uint16_t actual = (uint16_t)strtoul(sumLine.c_str(), nullptr, 16);
    return actual == expected;
}

bool registerTap(bool& outNowUnlocked) {
    uint32_t now = millis();
    if (tapCount > 0 && now - lastTapMs > TAP_WINDOW_MS) tapCount = 0;
    lastTapMs = now;
    tapCount++;
    if (tapCount >= TAP_GOAL) {
        tapCount = 0;
        bool currentlyUnlocked = certificateValid();
        bool ok = currentlyUnlocked ? removeCertificateInternal() : writeCertificateInternal();
        if (!ok) return false; // Zustand bleibt unveraendert (Auftrag)
        outNowUnlocked = !currentlyUnlocked;
        return true;
    }
    return false;
}

void resetTapCount() {
    tapCount = 0;
}

}
