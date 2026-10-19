#pragma once
// Ergebnis der automatischen IP-Standort-Ermittlung (siehe
// ip_geolocation_client.h) vom Fetch-Task (Core 0) zu Core 1 - gleiches
// Mutex/Momentaufnahme-Prinzip wie solar_data.h/weather_data.h. Core 1
// (main_screen.cpp::tick()) uebernimmt ein NEUES Ergebnis in appConfig
// (SD-Zugriff bleibt exklusiv auf Core 1) und speichert, AUSSER der
// Standort wurde von Hand gesetzt (appConfig.locationManual).
#include <Arduino.h>

namespace IpLocationData {

    struct Data {
        float lat = 0.0f;
        float lon = 0.0f;
        String name;
        bool valid = false;
        // Steigt bei jedem (auch erfolglosen) Ermittlungsversuch - Core 1
        // nutzt das nur zum Erkennen eines NEUEN erfolgreichen Ergebnisses
        // (valid wird dafuer zusaetzlich geprueft).
        uint32_t changeCounter = 0;
    };

    void init();
    Data snapshot();

    // NUR vom Fetch-Task (Core 0) aufrufen.
    void setResult(float lat, float lon, const String& name, bool valid);
}
