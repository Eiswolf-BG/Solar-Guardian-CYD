#pragma once
// Gibt die Portal-Zugangsdaten (und die nicht-geheime Geraete-
// Seriennummer) sicher von Core 1 (laedt appConfig von der SD) an den
// Fetch-Task auf Core 0 weiter - Core 0 greift NIE direkt auf appConfig
// zu. Eigener kleiner Mutex, gleiches Prinzip wie wifi_status.h.
#include <Arduino.h>

namespace AccountBridge {

    struct Snapshot {
        String email;
        String password;
        String deviceNo;
    };

    void init();

    // Core 1 ruft das beim Start (main.cpp) und nach jeder Kontoaenderung
    // (account_screen.cpp) auf, direkt bevor/nachdem Command::AccountChanged
    // gesendet wird.
    void set(const String& email, const String& password, const String& deviceNo);

    // Fetch-Task (Core 0) liest das beim eigenen Start und wenn
    // Command::AccountChanged ankommt.
    Snapshot get();
}
