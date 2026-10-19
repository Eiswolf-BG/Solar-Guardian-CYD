#include "sd_storage.h"
#include "config.h"
#include <SPI.h>
#include <SD.h>

namespace SdStorage {

namespace {
    SPIClass sdSpi(HSPI);
    bool mounted = false;
}

bool begin() {
    sdSpi.begin(Config::SD_SPI_CLK_PIN, Config::SD_SPI_MISO_PIN,
                Config::SD_SPI_MOSI_PIN, Config::SD_SPI_CS_PIN);
    mounted = SD.begin(Config::SD_SPI_CS_PIN, sdSpi, 4000000);
    return mounted;
}

bool isMounted() { return mounted; }

namespace {
    // Einmalige Migration: der History-Backup-Ordner lag frueher unter
    // /SolarGuardian/History, lebt jetzt eine Ebene hoeher unter
    // /SolarHistory (ausserhalb von /SolarGuardian, siehe config.h). Nur
    // noetig, wenn der alte Ordner noch existiert und der neue noch
    // nicht - kopiert die eine Backup-Datei und loescht danach den alten
    // Ordner.
    void migrateHistoryFolder() {
        if (!SD.exists(Config::SD_HISTORY_DIR_OLD)) return;
        if (SD.exists(Config::SD_HISTORY_DIR)) return; // neuer Ordner existiert schon - nichts zu tun

        SD.mkdir(Config::SD_HISTORY_DIR);
        if (SD.exists(Config::SD_HISTORY_BACKUP_FILE_OLD)) {
            File rf = SD.open(Config::SD_HISTORY_BACKUP_FILE_OLD, FILE_READ);
            if (rf) {
                // FILE_WRITE haengt beim ESP32-SD-Treiber an, statt zu
                // ueberschreiben (Auftrag "SD-Schreibfehler pruefen") -
                // in der Praxis nie erreichbar (der neue Ordner existiert
                // laut der Pruefung oben noch gar nicht), aber defensiv
                // trotzdem entfernen, falls eine fruehere Migration schon
                // einmal (unvollstaendig) hier geschrieben hat.
                if (SD.exists(Config::SD_HISTORY_BACKUP_FILE)) SD.remove(Config::SD_HISTORY_BACKUP_FILE);
                File wf = SD.open(Config::SD_HISTORY_BACKUP_FILE, FILE_WRITE);
                if (wf) {
                    while (rf.available()) wf.write(rf.read());
                    wf.close();
                }
                rf.close();
            }
        }
        SD.remove(Config::SD_HISTORY_BACKUP_FILE_OLD);
        SD.rmdir(Config::SD_HISTORY_DIR_OLD);
        Serial.println("[sd] History-Ordner migriert: /SolarGuardian/History -> /SolarHistory");
    }
}

void ensureRootDir() {
    if (!mounted) return;
    if (!SD.exists(Config::SD_ROOT_DIR)) SD.mkdir(Config::SD_ROOT_DIR);

    migrateHistoryFolder();

    // History-Backup-Ordner (siehe yield_ledger.h) - wird hier erzeugt,
    // damit er sowohl bei der Ersteinrichtung als auch bei jedem
    // normalen Start existiert (ensureRootDir() wird in beiden Faellen
    // aufgerufen).
    if (!SD.exists(Config::SD_HISTORY_DIR)) SD.mkdir(Config::SD_HISTORY_DIR);
}

}
