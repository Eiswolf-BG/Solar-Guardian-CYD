#pragma once
// Diagnose-Log fuer die Ursachenforschung beim "Flackern"-Auftrag -
// loggt NUR den GRUND (welches Feld/Zaehler/Zeitgeber eine Neuzeichnung
// ausgeloest hat), NIE Werte/Koordinaten/Zugangsdaten. Hinter
// Config::DEBUG_REDRAW_DIAGNOSTICS (siehe config.h), standardmaessig
// AUS - siehe CLAUDE.md.
#include <Arduino.h>

namespace RedrawDiag {
    void log(const char* page, const char* reason);
}
