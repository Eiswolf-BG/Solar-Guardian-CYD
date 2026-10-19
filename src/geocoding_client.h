#pragma once
// Orts-/Postleitzahlensuche - laeuft blockierend auf Core 1 direkt im
// location_screen.cpp (einmaliger, nutzerausgeloester Abruf, kein Dauer-
// Polling - gleiches Prinzip wie der blockierende WLAN-Scan in
// wifi_select_screen.cpp, siehe CLAUDE.md "Dual-Core-Architektur").
//
// Nutzt Nominatim (OpenStreetMap, kostenlos, ohne Zugangsschluessel)
// statt der Open-Meteo-Geocoding-API: ein Live-Test zeigte, dass Open-
// Meteo den "name"-Parameter rein als TEXT-Suche behandelt und bei einer
// deutschen Postleitzahl (z.B. "10115") irrtuemlich eine US-Stadt lieferte
// (deren postcodes-Liste zufaellig diese Ziffernfolge enthaelt) - keine
// echte Postleitzahlen-Unterstuetzung. Nominatims "postalcode="-Parameter
// lieferte im Test fuer deutsche PLZ UND fuer Orts-/Stadtnamen
// zuverlaessig korrekte Treffer (siehe CLAUDE.md "Standort").
#include <Arduino.h>

namespace GeocodingClient {

    constexpr uint8_t MAX_RESULTS = 5;

    struct Result {
        String name; // vollstaendiger Anzeigename (Ort, Region, Land)
        float lat = 0.0f;
        float lon = 0.0f;
    };

    enum class SearchStatus : uint8_t {
        Ok,            // count in out gueltig (kann 0 sein -> "Nichts gefunden")
        NetworkError,  // HTTP-/JSON-Fehler -> "Keine Verbindung"
    };

    // query = vom Nutzer eingegebener Ort/Postleitzahl (wird intern
    // URL-kodiert). Schreibt bis zu MAX_RESULTS Treffer in out, liefert
    // die tatsaechliche Anzahl ueber outCount zurueck.
    SearchStatus search(const String& query, Result* out, uint8_t& outCount);
}
