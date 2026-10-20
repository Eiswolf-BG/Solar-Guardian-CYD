#pragma once
// Kleiner Dateiexplorer fuer die SD-Karte (Menuepunkt "Dateien") - zeigt
// Wurzelverzeichnis und eine Ebene tiefer, erlaubt das Loeschen
// einzelner Dateien (mit Bestaetigung). /SolarHistory ist der zuletzt
// gesicherte Stand der Anlage und deshalb schreibgeschuetzt - siehe
// files_screen.cpp::isProtectedPath().
#include <Arduino.h>

namespace FilesScreen {
    void run();
}
