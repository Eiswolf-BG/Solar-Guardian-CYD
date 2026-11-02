#pragma once
// Wisch-Seite "Wetter" (Wochenansicht) - Auftrag "Neuer CYD-Screen:
// Wetter-Wochenansicht". Letzte Seite in der Reihenfolge, siehe pages.cpp.
// Nutzt ausschliesslich WeatherData (dieselbe Quelle/Logik wie die
// Uebersicht und die Web-Oberflaeche) - keine eigene Abfrage.
#include <Arduino.h>

namespace WeekPage {
    void draw(int16_t areaTop, int16_t areaHeight, bool forceRedraw);
    bool update();
}
