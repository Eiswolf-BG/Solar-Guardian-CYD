#pragma once
// Die Swipe-Seite "PV-Status" (Auftrag "Neuer Bildschirm PV"): 2x2-Kacheln
// PV-Spannung/PV-Strom/PV-Leistung/heutiges Leistungsmaximum, letzte Seite
// im Seitenwechsel (nach "Batterie").
// Siehe pages.cpp fuer die Einbindung.
#include <Arduino.h>

namespace PvStatusPage {
    void draw(int16_t areaTop, int16_t areaHeight, bool forceRedraw);
    bool update();
    bool onTap(int16_t x, int16_t y, int16_t areaTop, int16_t areaHeight);
}
