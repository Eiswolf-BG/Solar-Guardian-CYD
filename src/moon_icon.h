#pragma once
// Kleine, selbst gezeichnete Halbmondsichel fuer die Nachtmodus-Anzeige
// (Auftrag "Tagesfenster mit Nachtmodus") - auf schwarzem Hintergrund per
// zwei ueberlappenden Kreisen (gefuellter Kreis in der gewuenschten Farbe,
// ein Teil davon in Schwarz "weggebissen"). Wird sowohl in der Kopfzeile
// (main_screen.cpp, klein) als auch auf den Seiten mit "Naechste Abfrage:"
// (power_page.cpp/battery_page.cpp, etwas groesser) verwendet - deshalb
// hier als gemeinsamer, winziger Helfer statt dreifach duplizierter
// Zeichenlogik.
#include <TFT_eSPI.h>

namespace MoonIcon {
    // cx/cy = Mittelpunkt, r = Aussenradius. Zeichnet nur die Sichel selbst,
    // keinen Rahmen/Hintergrund - der Aufrufer sorgt fuer einen schwarzen
    // Untergrund (im ganzen Projekt ohnehin immer der Fall).
    void draw(TFT_eSPI& t, int16_t cx, int16_t cy, int16_t r, uint16_t color);
}
