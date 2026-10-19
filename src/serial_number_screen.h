#pragma once
// Eingabe/Aenderung der Geraete-Seriennummer (device_no) - zwei
// Einstiegspunkte: Ersteinrichtung (eigener Screen nach den
// Zugangsdaten, mit Zurueck-Knopf) und Konto-Screen (zeigt/aendert die
// gespeicherte Nummer eines bereits eingerichteten Geraets - kein
// eigener Hauptmenuepunkt mehr, siehe account_screen.cpp).
#include <Arduino.h>

namespace SerialNumberScreen {
    // Ersteinrichtung: appConfig.deviceNo ist der Arbeitsspeicher fuer die
    // Eingabe (vom Aufrufer VOR dem ersten Aufruf auf "" gesetzt, siehe
    // main.cpp). Rueckgabe true = "Los geht's" bestaetigt (gueltige
    // Eingabe, appConfig.deviceNo gesetzt). Rueckgabe false + outBack=true
    // = "Zurueck" gedrueckt (zurueck zum Zugangsdaten-Screen, die bisher
    // eingegebene Seriennummer bleibt in appConfig.deviceNo erhalten).
    bool runFirstTimeSetup(bool& outBack);

    // Konto-Screen: zeigt die aktuell gespeicherte Seriennummer
    // (vorbelegt), speichert bei Aenderung atomar in config.json,
    // benachrichtigt den Fetch-Task (neue Abfrage, alte Batterie-/
    // Leistungs-/Ertragswerte verworfen) und zeigt vorher einen kurzen
    // Hinweis zum Ertragsbuch. Kehrt nach "Zurueck"/Speichern zum Aufrufer
    // (AccountScreen::run()) zurueck.
    void runFromAccount();
}
