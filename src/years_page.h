#pragma once
// Die Swipe-Seite "Jahre": jetzt eine Uebersicht mit 3 Reitern (Tage/
// Monate/Jahre), je ein Balkendiagramm mit bis zu 7 Balken (laufende
// Periode + bis zu 6 fruehere, siehe Config::HISTORY_CHART_MAX_BARS).
// Tage/Monate kommen aus dem Ertragsbuch (yield_ledger.h), Jahre
// weiterhin aus years.json. Reiter werden per onTap() umgeschaltet,
// Wischen zwischen den Hauptseiten bleibt unberuehrt (siehe
// main_screen.cpp).
#include <Arduino.h>

namespace YearsPage {
    void draw(int16_t areaTop, int16_t areaHeight, bool forceRedraw);
    bool update();
    bool onTap(int16_t x, int16_t y, int16_t areaTop, int16_t areaHeight);
}
