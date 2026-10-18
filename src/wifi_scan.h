#pragma once
// Duenne Hilfsschicht um WiFi.scanNetworks()/WiFi.begin() fuer den
// WLAN-Auswahlbildschirm (siehe wifi_select_screen.cpp).
#include <Arduino.h>

namespace WifiScan {

    struct Network {
        String ssid;
        int32_t rssi = 0;
        bool open = false; // true = kein Passwort noetig
    };

    // Blockierender Scan, nach Signalstaerke absteigend sortiert, auf
    // Config::MAX_WIFI_NETWORKS_SHOWN begrenzt. Doppelte SSIDs (mehrere
    // Access Points desselben Netzwerks) werden zusammengefasst, es bleibt
    // jeweils der staerkste Treffer.
    uint8_t scan(Network* outList, uint8_t maxCount);

    // Blockierender Verbindungsversuch mit Timeout
    // (Config::WIFI_CONNECT_TIMEOUT_MS). Gibt true bei Erfolg zurueck.
    bool connect(const String& ssid, const String& password);

    // 0-4, aus WiFi.RSSI() - gleiche Schwellen wie der Empfangsbalken auf
    // dem Hauptbildschirm (main_screen.cpp) benutzt dieselbe Funktion.
    uint8_t signalBars();
}
