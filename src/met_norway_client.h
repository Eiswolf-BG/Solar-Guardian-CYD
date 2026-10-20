#pragma once
// Aktuelles Wetter (Temperatur/Symbol/Bewoelkung) ueber MET Norway -
// trifft die Realitaet laut Quellenvergleich deutlich besser als
// Open-Meteo. Open-Meteo bleibt Rueckfall (siehe weather_client.h) sowie
// Quelle fuer Vorhersage/Standortsuche. Wird ausschliesslich vom
// Fetch-Task (Core 0) aufgerufen, nie TFT/SD. Ortsname/Koordinaten
// werden NIE geloggt.
#include <Arduino.h>

namespace MetNorwayClient {

    // true bei Erfolg - outTempC die Lufttemperatur, outWmoCode der auf
    // unsere vorhandenen Wettersymbole (siehe weather_icons.h, dieselben
    // Gruppen wie Open-Meteo) abgebildete Code, outCloudPercent die rohe
    // Bewoelkung in Prozent. Bei Misserfolg bleiben alle drei
    // unveraendert - der Aufrufer behaelt dann den zuletzt gueltigen
    // (Open-Meteo-)Wert.
    bool fetchCurrent(float lat, float lon, float& outTempC, int16_t& outWmoCode, float& outCloudPercent,
                      int* outHttpCode, size_t* outResponseBytes);
}
