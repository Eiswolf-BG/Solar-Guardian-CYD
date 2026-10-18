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

void ensureRootDir() {
    if (!mounted) return;
    if (!SD.exists(Config::SD_ROOT_DIR)) SD.mkdir(Config::SD_ROOT_DIR);
    // History-Backup-Ordner (siehe yield_ledger.h) - wird hier erzeugt,
    // damit er sowohl bei der Ersteinrichtung als auch bei jedem
    // normalen Start existiert (ensureRootDir() wird in beiden Faellen
    // aufgerufen).
    if (!SD.exists(Config::SD_HISTORY_DIR)) SD.mkdir(Config::SD_HISTORY_DIR);
}

}
