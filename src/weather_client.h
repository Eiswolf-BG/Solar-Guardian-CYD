#pragma once
// Wetterabfrage ueber Open-Meteo.com (kostenlos, ohne Zugangsschluessel) -
// laeuft im Fetch-Task auf Core 0 (siehe fetch_task.cpp), schreibt das
// Ergebnis in WeatherData. Gleiches HTTP-Grundprinzip wie epever_client.h
// (eigene Verbindung je Aufruf, setInsecure(), ArduinoJson-Filter).
#include <Arduino.h>

namespace WeatherClient {

    // Fragt aktuelles Wetter + 7-Tage-Vorhersage fuer lat/lon ab und
    // schreibt das Ergebnis direkt in WeatherData (siehe weather_data.h).
    // Gibt false bei HTTP-/JSON-Fehler zurueck (WeatherData bleibt dabei
    // auf dem letzten bekannten Stand, siehe Auftrag "bei Abruffehler die
    // letzten Werte stehen lassen"). outResponseBytes = Groesse der
    // rohen Antwort (fuer das Diagnose-Log in fetch_task.cpp, siehe
    // Auftrag "wie gross eine Wetterantwort ist").
    bool fetch(float lat, float lon, int* outHttpCode, size_t* outResponseBytes);
}
