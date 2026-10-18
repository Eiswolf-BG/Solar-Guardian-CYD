#pragma once
// Die Swipe-Seite "Ertrag": 2x2-Kacheln (Heute/Monat/Jahr/Gesamt) aus
// SolarData, siehe pages.cpp fuer die Einbindung.
#include <Arduino.h>

namespace YieldPage {
    void draw(int16_t areaTop, int16_t areaHeight);
    // true = seit dem letzten Aufruf hat sich etwas geaendert (SolarData-
    // Aenderungszaehler oder die angezeigte Minute) - Core 1 zeichnet dann
    // neu, sonst nicht (kein Flackern, siehe CLAUDE.md).
    bool update();
    // Tipp auf die Statuszeile loest "jetzt aktualisieren" aus.
    bool onTap(int16_t x, int16_t y, int16_t areaTop, int16_t areaHeight);
}
