#pragma once
// Debug-Layout-Pruefer (siehe CLAUDE.md "Layout-Regel") - nur aktiv, wenn
// ENABLED true ist (eine Konstante zum Abschalten, kein Laufzeit-Schalter
// noetig). Screens melden ihre gezeichneten Rechtecke (Text/Button/
// Symbol) an, finish() prueft paarweise auf Ueberlappung und ob ein
// Rechteck ueber den Bildschirmrand ragt, und loggt bei einem Konflikt
// eine Warnung ueber den seriellen Monitor.
#include <Arduino.h>

namespace LayoutCheck {

    constexpr bool ENABLED = true;

    // Vor dem ersten rect()-Aufruf eines Bildschirm-Durchlaufs.
    void begin(const char* screenName);

    // Jedes sichtbare Element melden (Label frei waehlbar, z.B. "Titel",
    // "Kachel Heute: Wert").
    void rect(const char* label, int16_t x, int16_t y, int16_t w, int16_t h);

    // Prueft alle seit begin() gemeldeten Rechtecke und loggt Konflikte.
    void finish();
}
