#pragma once
// Harte Projektregel (siehe CLAUDE.md "Layout-Regel"): kein Text/Button/
// Symbol darf sich ueberschneiden oder ueber den Bildschirmrand ragen -
// in BEIDEN Sprachen. fitText() waehlt dafuer die groesste passende
// Schriftgroesse (mind. 4px Innenabstand), kuerzt mit "..." falls selbst
// die kleinste Groesse nicht passt.
#include <TFT_eSPI.h>

namespace LayoutUtils {

    struct FitResult {
        uint8_t textSize;
        String text; // ggf. mit "..." gekuerzt
    };

    // sizes absteigend sortiert uebergeben (z.B. {2,1}) - die erste
    // passende Groesse gewinnt. Setzt tft.setTextSize() bereits auf das
    // Ergebnis (Aufrufer kann direkt danach zeichnen).
    FitResult fitText(TFT_eSPI& tft, const String& text, int16_t maxW, int16_t maxH,
                       const uint8_t* sizes, uint8_t sizeCount);
}
