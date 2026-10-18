#pragma once
// WLAN-Scan + Auswahl + Passwort-Abfrage + Verbindungstest - genutzt von
// der Ersteinrichtung UND vom "Netzwerke"-Bildschirm (networks_screen.cpp)
// beim Belegen eines freien Platzes.
#include <Arduino.h>

namespace WifiSelectScreen {
    // allowCancel=false blendet den Abbrechen-Knopf aus (Ersteinrichtung,
    // WLAN ist dort Pflicht). Bei Erfolg true und outSsid/outPassword
    // gesetzt (Verbindung steht zu diesem Zeitpunkt bereits, WiFi bleibt
    // verbunden). Bei Abbruch false.
    bool run(bool allowCancel, String& outSsid, String& outPassword);
}
