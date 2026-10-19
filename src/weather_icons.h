#pragma once
// Zeichnet ein farbiges Wettersymbol rein mit TFT_eSPI-Grundformen (keine
// Bilddateien, spart Flash, siehe Auftrag "Hauptscreen/Wetter"). Eine
// Funktion mit Groessenparameter, damit dieselbe Zeichenroutine in der
// grossen Wetterkachel UND klein in der Wochenansicht genutzt werden kann.
#include <TFT_eSPI.h>

namespace WeatherIcons {

    // Ordnet einen WMO-Wettercode (siehe https://open-meteo.com/en/docs,
    // Feld weather_code) einer der im Auftrag festgelegten Symbolgruppen
    // zu und zeichnet sie zentriert in ein size x size-Quadrat um (cx,cy).
    // isDay steuert Sonne/Mond bei klarem/leicht bewölktem Himmel. Loescht
    // vorher den eigenen Bereich (TFT_BLACK) - der Aufrufer ruft das nur
    // bei tatsaechlicher Aenderung auf (kein Flackern, siehe Auftrag).
    void draw(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t size, int16_t weatherCode, bool isDay);
}
