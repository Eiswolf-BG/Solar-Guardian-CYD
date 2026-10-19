#pragma once
// Die Swipe-Seite "Jahre": Balkengrafik mit dem Ertrag der letzten bis zu
// 6 Kalenderjahre (laufendes Jahr live, Vorjahre aus years.json, siehe
// yield_ledger.h). Kein eigener Tipp-Bereich.
#include <Arduino.h>

namespace YearsPage {
    void draw(int16_t areaTop, int16_t areaHeight, bool forceRedraw);
    bool update();
}
