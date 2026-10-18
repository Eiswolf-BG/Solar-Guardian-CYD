#pragma once
// Winziger, mutex-geschuetzter Status "welcher WLAN-Platz ist aktuell
// verbunden", getrennt von SolarData (das geht nur die Solaranlage an).
// Der Fetch-Task (Core 0, siehe wifi_connect_manager.cpp) schreibt hier
// statt appConfig.lastActiveWifiSlot direkt zu setzen und selbst auf die
// SD-Karte zu speichern (das waere ein SD-Zugriff von Core 0 aus, nie
// erlaubt). Core 1 (main_screen.cpp::tick()) liest den Status periodisch
// und uebernimmt die eigentliche Speicherung in appConfig/config.json.
#include <Arduino.h>

namespace WifiStatus {
    struct Status {
        bool connected = false;
        uint8_t slot = 255; // 0-2, oder 255 = keiner/unbekannt
    };

    void init();
    void set(bool connected, uint8_t slot);
    Status get();
}
