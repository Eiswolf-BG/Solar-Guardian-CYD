#pragma once
// Letzter Schritt der Ersteinrichtung: Benutzername (E-Mail) und Passwort
// fuer das Solar Guardian Portal abfragen. Beim Antippen von "Los geht's"
// wird die komplette Konfiguration (Touch-Kalibrierung, WLAN, Zugangsdaten)
// auf die SD-Karte geschrieben.
#include <Arduino.h>

namespace CredentialsScreen {
    // Blockiert, bis "Los geht's" erfolgreich ausgefuehrt wurde (also beide
    // Felder gefuellt UND appConfig gespeichert).
    void run();
}
