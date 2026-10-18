#pragma once
// SD-Karten-Anbindung (SPI) - Mounten und Ordnerstruktur anlegen.
#include <Arduino.h>

namespace SdStorage {
    // Versucht, die SD-Karte zu mounten. Kann beliebig oft erneut
    // aufgerufen werden (main.cpp ruft es alle 2s erneut auf, solange
    // keine Karte erkannt wurde).
    bool begin();

    bool isMounted();

    // Legt Config::SD_ROOT_DIR an, falls er noch nicht existiert.
    void ensureRootDir();
}
