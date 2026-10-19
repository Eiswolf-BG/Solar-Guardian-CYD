#pragma once
// Neuer Hauptscreen (Seite 1 der Wisch-App, siehe pages.cpp): 2x2 Kacheln
// Wetter/Leistung/Heute/Batterie - Implementierung des Pages::Page-
// Interfaces (siehe pages.h).
#include <Arduino.h>

namespace HomePage {
    void draw(int16_t areaTop, int16_t areaHeight, bool forceRedraw);
    bool update();
    bool onTap(int16_t x, int16_t y, int16_t areaTop, int16_t areaHeight);
}
