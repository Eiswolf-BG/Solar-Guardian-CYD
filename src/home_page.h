#pragma once
// Neuer Hauptscreen (Seite 1 der Wisch-App, siehe pages.cpp): 2x2 Kacheln
// Wetter/Leistung/Heute/Batterie - Implementierung des Pages::Page-
// Interfaces (siehe pages.h).
#include <Arduino.h>

namespace HomePage {
    void draw(int16_t areaTop, int16_t areaHeight, bool forceRedraw);
    bool update();
    bool onTap(int16_t x, int16_t y, int16_t areaTop, int16_t areaHeight);
    // Langer Druck (~2s) auf die Wetterkachel oeffnet IMMER den
    // Standort-Screen (Eingabe PLZ/Ort, oder "Automatisch (IP)") - auch
    // wenn schon ein Standort bekannt ist (Auftrag "Standort jederzeit
    // neu einstellen koennen"). Ein kurzer Tipp bleibt die Wochenansicht/
    // der Standort-Screen bei noch unbekanntem Standort, siehe onTap().
    bool onLongPress(int16_t x, int16_t y, int16_t areaTop, int16_t areaHeight);
}
