#pragma once
// Gemeinsame Wetterdaten zwischen Core 0 (Fetch-Task, schreibt) und Core 1
// (Anzeige, liest) - gleiches Mutex/Momentaufnahme-Prinzip wie
// solar_data.h (siehe CLAUDE.md "Dual-Core-Architektur").
#include <Arduino.h>

namespace WeatherData {

    constexpr uint8_t FORECAST_DAYS = 7;

    struct DayForecast {
        // Lokales Mitternachts-Epoch des Tages (aus der Datumsangabe der
        // Antwort, NICHT aus der Uhr berechnet - siehe Auftrag) - daraus
        // leitet die Anzeige den Wochentagsnamen ab.
        time_t dateEpoch = 0;
        int16_t weatherCode = -1;
        float tempMaxC = 0.0f;
        float tempMinC = 0.0f;
        float sunshineHours = 0.0f;
        bool sunshineValid = false;
    };

    struct Data {
        float currentTempC = 0.0f;
        int16_t currentWeatherCode = -1;
        bool isDay = true;
        bool currentValid = false;
        time_t measuredAt = 0;

        DayForecast days[FORECAST_DAYS];
        uint8_t dayCount = 0;

        bool locationSet = false; // Standort ueberhaupt konfiguriert (siehe location_bridge.h)

        // Steigt bei jeder Aktualisierung - Core 1 vergleicht ihn, um nur
        // bei echter Aenderung neu zu zeichnen statt bei jedem Tick.
        uint32_t changeCounter = 0;
    };

    // Einmal beim Start aufrufen (legt den Mutex an).
    void init();

    // Core 1: liefert eine konsistente Momentaufnahme.
    Data snapshot();

    // Ab hier NUR vom Fetch-Task (Core 0) aufrufen.
    void setCurrent(float tempC, int16_t weatherCode, bool isDay, time_t measuredAt);
    void setDaily(const DayForecast* days, uint8_t count);
    void setLocationSet(bool set);
}
