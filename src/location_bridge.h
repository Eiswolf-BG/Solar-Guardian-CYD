#pragma once
// Gibt den gespeicherten Wetter-Standort (Breite/Laenge) sicher von Core 1
// (laedt appConfig von der SD) an den Fetch-Task auf Core 0 weiter - Core 0
// greift NIE direkt auf appConfig zu. Eigener kleiner Mutex, gleiches
// Prinzip wie account_bridge.h/wifi_status.h. Bewusst getrennt von
// AccountBridge (fachlich unabhaengig vom Solar-Guardian-Konto).
#include <Arduino.h>

namespace LocationBridge {

    struct Snapshot {
        float lat = 0.0f;
        float lon = 0.0f;
        bool valid = false; // false = noch kein Standort gesetzt
        // true = von Hand gesetzt (siehe location_screen.cpp) - die
        // automatische IP-Ermittlung im Fetch-Task ueberschreibt dann
        // nichts mehr (Auftrag "Standort automatisch, notfalls einmalig
        // per Eingabe").
        bool manual = false;
    };

    void init();

    // Core 1 ruft das beim Start (main.cpp) und nach jeder Standort-
    // Aenderung (location_screen.cpp/main_screen.cpp nach einer neuen
    // automatischen Ermittlung) auf, direkt bevor Command::LocationChanged
    // gesendet wird.
    void set(float lat, float lon, bool valid, bool manual);

    // Fetch-Task (Core 0) liest das beim eigenen Start und wenn
    // Command::LocationChanged ankommt.
    Snapshot get();
}
