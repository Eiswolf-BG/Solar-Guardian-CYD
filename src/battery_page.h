#pragma once
// Die Swipe-Seite "Batterie": Ladezustand gross + Balkengrafik, darunter
// 2x2-Kacheln (Spannung/Strom/Temperatur/Status), siehe pages.cpp fuer
// die Einbindung.
#include <Arduino.h>

namespace BatteryPage {
    void draw(int16_t areaTop, int16_t areaHeight);
    bool update();
    bool onTap(int16_t x, int16_t y, int16_t areaTop, int16_t areaHeight);
}
