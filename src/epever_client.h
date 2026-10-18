#pragma once
// HTTPS-Zugriff auf das Solar Guardian Portal (EPEVER-Cloud). Wird
// ausschliesslich vom Fetch-Task auf Core 0 aufgerufen (siehe
// fetch_task.cpp) - nie TFT/SD. Gibt NIE Zugangsdaten/Token/Antworttexte
// ueber den seriellen Monitor aus, nur Statuscodes (siehe CLAUDE.md).
#include <Arduino.h>

namespace EpeverClient {
    // true bei Erfolg (HTTP 200, status==0 in der Antwort, Token
    // vorhanden) - outToken wird dann gesetzt, sonst unveraendert
    // gelassen.
    bool login(const String& email, const String& password, String& outToken);

    // true bei Erfolg (HTTP 200, status==0) - die vier Werte (kWh) werden
    // dann gesetzt, sonst unveraendert gelassen.
    bool fetchEnergy(const String& token, float& outTodayKwh, float& outMonthKwh,
                      float& outYearKwh, float& outTotalKwh);

    // Ein einzelner Messwert aus der Port-7002-Abfrage - entweder Zahl
    // oder Text (siehe Auftrag), valid=false wenn der Datenpunkt in der
    // Antwort fehlte.
    struct BatteryPoint {
        bool valid = false;
        bool isText = false;
        float value = 0.0f;
        String text;
        time_t time = 0; // Sekunden (aus ms-Zeitstempel der API umgerechnet)
    };

    struct BatteryResult {
        BatteryPoint voltage, current, chargeStatus, temperature, soc, dailyGeneration;
    };

    // deviceNo ist keine geheime Information (Geraete-Seriennummer).
    // Authentifizierung laut Browser-Netzwerk-Tab: der JWT steht roh (ohne
    // "Bearer") im Header "token", KEIN Appid-Header, KEIN "token" im
    // Body (siehe epever_auth_headers.h fuer drei weitere, noch leere
    // Header-Konstanten). outHttpCode/outApiStatus fuer die
    // Fehlerbehandlung im Fetch-Task (erneuter Login bei 401/Fehler).
    bool fetchBatteryData(const String& deviceNo, const String& token,
                          BatteryResult& out, int* outHttpCode, int* outApiStatus);
}
