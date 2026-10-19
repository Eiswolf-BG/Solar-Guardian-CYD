#pragma once
// Wochenansicht: 7 Zeilen (Heute, Morgen, danach Wochentag) mit kleinem
// Wettersymbol, Max/Min-Temperatur und Sonnenscheindauer - kein Wisch-
// Seite, ein eigener Screen wie das Menue (siehe Auftrag "Hauptscreen/
// Wetter"). Erreichbar durch Antippen der Wetterkachel auf dem
// Hauptscreen (home_page.cpp).
#include <Arduino.h>

namespace WeekViewScreen {
    void run();
}
