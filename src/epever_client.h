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
        // PV-Spannung/-Strom (Seite "Leistung") - laufen mit kleinem
        // sampleLimit in derselben Anfrage mit, siehe CLAUDE.md.
        BatteryPoint pvVoltage, pvCurrent;
        // PV-Leistung als eigener Datenpunkt (Auftrag "Aktiv-Auslesen",
        // Korrektur B3 - wird fuer die Uebersicht-Kachel "Leistung"
        // gebraucht) - laeuft seitdem ebenfalls in derselben Anfrage mit,
        // dieselbe dataPointId wie die separate 3h-Kurvenabfrage
        // (EpeverDatapoints::PV_POWER).
        BatteryPoint power;
        // Tages-Extremwerte der Batteriespannung (Auftrag "Max/Min-
        // Spannungskacheln") - laufen ebenfalls in derselben Anfrage mit.
        BatteryPoint voltageMax, voltageMin;
    };

    // deviceNo ist keine geheime Information (Geraete-Seriennummer).
    // Authentifizierung laut Browser-Netzwerk-Tab: der JWT steht roh (ohne
    // "Bearer") im Header "token", KEIN Appid-Header, KEIN "token" im
    // Body (siehe epever_auth_headers.h fuer drei weitere, noch leere
    // Header-Konstanten). outHttpCode/outApiStatus fuer die
    // Fehlerbehandlung im Fetch-Task (erneuter Login bei 401/Fehler).
    bool fetchBatteryData(const String& deviceNo, const String& token,
                          BatteryResult& out, int* outHttpCode, int* outApiStatus);

    // Aktiv-Auslesen beim Antippen (Auftrag "Aktualisieren-Knopf soll
    // echt etwas bewirken", Korrektur B "an die echte Portal-Anfrage
    // anpassen") - loest beim Regler fuer JEDEN der dataPointIds EINE
    // frische Messung aus ("userop/add" mit opType 0 = NUR Lesen, siehe
    // epever_active_read.h fuer die Whitelist-Pruefung, die das hart
    // durchsetzt - ein Befehl PRO Datenpunkt, nacheinander gesendet,
    // siehe CLAUDE.md Tabelle), und wartet dann GEMEINSAM kurz wiederholt
    // (Config::EPEVER_ACTIVE_READ_POLL_INTERVAL_MS) auf NEUERE Zeit-
    // stempel fuer ALLE angeforderten Datenpunkte - abgefragt ueber den
    // bereits bestehenden, portal-verifizierten passiven Abruf
    // (fetchBatteryData()), NICHT ueber einen separaten, unbestaetigten
    // Endpunkt. Zeitgrenze Config::EPEVER_ACTIVE_READ_WAIT_TIMEOUT_MS.
    // Blockiert dabei den Fetch-Task (Core 0) - unkritisch, da zeitlich
    // hart begrenzt und nur vom Fetch-Task selbst aufgerufen.
    // dataPointCount darf hoechstens FetchTask::ACTIVE_READ_MAX_DATAPOINTS
    // sein (wird intern zusaetzlich sicherheitshalber gedeckelt).
    // outFresh=true nur, wenn ALLE angeforderten Werte tatsaechlich einen
    // neueren Zeitstempel bekamen; bei Zeitgrenze liefert die Funktion
    // trotzdem true (kein Fehler), aber outFresh=false ("Keine neuen
    // Daten", kein echter Verbindungsfehler).
    bool activeReadBattery(const String& deviceNo, const String& token,
                           const int32_t* dataPointIds, uint8_t dataPointCount,
                           BatteryResult& out, bool& outFresh, int* outHttpCode, int* outApiStatus);

    constexpr uint8_t POWER_CURVE_MAX_POINTS = 50;

    struct PowerCurvePoint {
        time_t time = 0; // Sekunden
        float watts = 0.0f;
    };

    // Eigene Anfrage nur fuer den Leistungs-Datenpunkt (grosses
    // sampleLimit) - liefert bis zu POWER_CURVE_MAX_POINTS Punkte der
    // letzten 3 Stunden, NEUESTER ZUERST (wie von der API geliefert,
    // "timeSort":"desc") - der Aufrufer sortiert bei Bedarf um. points[0]
    // ist bei Erfolg zugleich die aktuelle Leistung samt Messzeit.
    bool fetchPowerCurve(const String& deviceNo, const String& token,
                        PowerCurvePoint* points, uint8_t& outCount,
                        int* outHttpCode, int* outApiStatus);

    // Wie fetchPowerCurve(), aber mit frei waehlbarem Zeitfenster/
    // sampleLimit statt der festen letzten 3 Stunden - fuer das einmalige
    // Nachladen des Tagesverlaufs (Seite "Tagesverlauf"), das das
    // 05:00-22:00-Fenster in mehreren kleinen Haeppchen abruft (siehe
    // fetch_task.cpp::runDailyCurveBackfillIfNeeded()). fetchPowerCurve()
    // selbst ruft diese Funktion intern mit dem festen 3h-Fenster auf.
    // points MUSS mindestens sampleLimit Eintraege gross sein.
    bool fetchPowerCurveWindow(const String& deviceNo, const String& token,
                              int64_t startMs, int64_t endMs, uint8_t sampleLimit,
                              PowerCurvePoint* points, uint8_t& outCount,
                              int* outHttpCode, int* outApiStatus);

    constexpr uint8_t EQUIPMENT_LIST_MAX_DEVICES = 9; // entspricht pageSize im Auftrag

    // Nur die Seriennummer (Feld "gatewayId") je Eintrag - die Antwort
    // enthaelt auch persoenliche Daten (Anschrift/Name/Konto), die hier
    // bewusst NICHT geparst werden (siehe Auftrag "automatische
    // Seriennummer-Ermittlung").
    struct EquipmentListResult {
        int32_t total = 0; // data.total laut Portal (Gesamtzahl Geraete)
        uint8_t count = 0; // tatsaechlich gelesene Eintraege (<= EQUIPMENT_LIST_MAX_DEVICES)
        String gatewayIds[EQUIPMENT_LIST_MAX_DEVICES];
    };

    // Geraeteliste der Anlage. NIE die Seriennummer oder das Token
    // loggen (siehe Aufrufer in fetch_task.cpp) - diese Funktion selbst
    // loggt bei Fehlern nur HTTP-/Portal-Statuscodes.
    bool fetchEquipmentList(const String& token, EquipmentListResult& out,
                            int* outHttpCode, int* outApiStatus);
}
