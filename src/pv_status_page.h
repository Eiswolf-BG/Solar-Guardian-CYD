#pragma once
// Die Swipe-Seite "PV-Status" (Auftrag "Neuer Bildschirm PV"): 2x2-Kacheln
// PV-Spannung/PV-Strom/PV-Leistung/heutiges Leistungsmaximum, im
// Seitenwechsel direkt nach "Batterie" (seit Auftrag "Seitenreihenfolge
// am CYD aendern" gefolgt von "Verlauf", nicht mehr die letzte Seite).
// Siehe pages.cpp fuer die Einbindung.
#include <Arduino.h>

namespace PvStatusPage {
    void draw(int16_t areaTop, int16_t areaHeight, bool forceRedraw);
    bool update();
    bool onTap(int16_t x, int16_t y, int16_t areaTop, int16_t areaHeight);
}
