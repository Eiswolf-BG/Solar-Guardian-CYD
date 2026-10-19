#pragma once
// Zugangsdaten-Schritt der Ersteinrichtung: Benutzername (E-Mail) und
// Passwort fuer das Solar Guardian Portal abfragen.
#include <Arduino.h>

namespace CredentialsScreen {
    // isLastStep=true (Standard, wie bisher): Knopf heisst "Los geht's",
    // schreibt beim Antippen die komplette Konfiguration (Touch-
    // Kalibrierung, WLAN, Zugangsdaten) auf die SD-Karte und holt die
    // NTP-Zeit - blockiert, bis das erledigt ist.
    // isLastStep=false (Ersteinrichtung mit nachfolgendem Seriennummer-
    // Screen, siehe main.cpp): Knopf heisst "Weiter", speichert NICHTS
    // und holt KEINE NTP-Zeit - das uebernimmt finalizeSetup() unten,
    // nachdem auch die Seriennummer eingegeben wurde.
    void run(bool isLastStep = true);

    // Letzter Schritt der Ersteinrichtung (main.cpp, nach Konto UND
    // Seriennummer): legt /SolarGuardian an, setzt setupComplete, speichert
    // appConfig und holt die NTP-Zeit (gleiche Logik wie zuvor in run(),
    // jetzt ausgelagert, damit die Seriennummer dazwischen abgefragt
    // werden kann).
    void finalizeSetup();
}
