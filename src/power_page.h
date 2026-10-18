#pragma once
// Die Swipe-Seite "Leistung": aktuelle PV-Leistung gross, PV-Spannung/
// -Strom als kleine Kacheln, Kurve der letzten 3 Stunden, siehe
// pages.cpp fuer die Einbindung.
#include <Arduino.h>

namespace PowerPage {
    void draw(int16_t areaTop, int16_t areaHeight);
    bool update();
    bool onTap(int16_t x, int16_t y, int16_t areaTop, int16_t areaHeight);
}
