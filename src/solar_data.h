#pragma once
// Gemeinsame Solaranlagen-Daten zwischen Core 0 (Fetch-Task, schreibt) und
// Core 1 (Anzeige, liest). Schutz per Mutex - Core 1 holt sich mit
// snapshot() unter dem Mutex eine Kopie und zeichnet danach OHNE Mutex
// weiter (siehe CLAUDE.md "Dual-Core-Architektur").
//
// Aktuell befuellt der Fetch-Task diese Struktur noch NICHT mit echten
// Werten (Platzhalter, Teil 2 der Dual-Core-Umstellung) - die Felder sind
// aber bereits vollstaendig angelegt, damit spaetere API-Abfrage-Features
// direkt hier einhaengen koennen, ohne die Struktur nochmal umbauen zu
// muessen.
#include <Arduino.h>

namespace SolarData {

    // Grobstatus der Portal-Abfrage (Ertrag-Feature und spaetere
    // API-Abfragen) - Core 1 bildet daraus den angezeigten Text ueber die
    // zentrale Textliste (i18n.h), damit hier keine Sprache drinsteckt.
    enum class FetchStatus : uint8_t {
        NoAccount,
        NoDeviceNo,
        LoggingIn,
        LoginFailed,
        NetworkError,
        Ok,
    };

    constexpr uint8_t HISTORY_MAX_POINTS = 50;

    struct HistoryPoint {
        time_t epoch = 0;
        float valueW = 0.0f;
    };

    struct Data {
        // Aktuelle Leistung
        float currentPowerW = 0.0f;
        time_t powerMeasuredAt = 0;
        bool powerValid = false;

        // Verlauf (aelteste zuerst, einfacher Ringpuffer)
        HistoryPoint history[HISTORY_MAX_POINTS];
        uint8_t historyCount = 0;
        uint8_t historyHead = 0; // naechster Schreibindex
        bool historyValid = false;

        // Energie (kWh)
        float energyTodayKwh = 0.0f;
        float energyMonthKwh = 0.0f;
        float energyYearKwh = 0.0f;
        float energyTotalKwh = 0.0f;
        bool energyValid = false;

        // Batterie
        float batteryVoltage = 0.0f;
        float batteryCurrent = 0.0f;
        float batterySocPercent = 0.0f;
        float batteryTempC = 0.0f;
        bool batteryValid = false;
        time_t batteryMeasuredAt = 0;

        // PV-Spannung/-Strom (Datenpunkte 101596362/101596380), fuer die
        // Seite "Leistung" - kommen in derselben Anfrage wie die Batterie-
        // Werte mit (kleines sampleLimit), siehe CLAUDE.md "Seite Leistung".
        float pvVoltage = 0.0f;
        float pvCurrent = 0.0f;
        bool pvValid = false;
        time_t pvMeasuredAt = 0;
        // Ladestatus: entweder ein bekannter Zahlencode (0-3, siehe
        // i18n CHARGE_STATUS_*) oder ein roher Text von der API, falls sie
        // keine Zahl liefert. -1 = kein Ladestatus verfuegbar.
        int16_t chargeStatusCode = -1;
        bool chargeStatusIsText = false;
        char chargeStatusRawText[32] = {0};

        // Tagesertrag aus dem Datenpunkt "Daily Generation" (primaer) oder
        // aus getGeneratedEnergy (Fallback) - Rohwert fuer YieldLedger
        // (Core 1), das daraus Monat/Jahr/Gesamt selbst zaehlt (siehe
        // CLAUDE.md "Ertragsbuch"). energyTodayKwh/energyMonthKwh/
        // energyYearKwh/energyTotalKwh oben bleiben die reinen
        // Portal-Rohwerte (nur noch als Startwert fuer die erste
        // Inbetriebnahme genutzt).
        float dailySourceKwh = 0.0f;
        time_t dailySourceMeasuredAt = 0;
        bool dailySourceValid = false;
        bool dailySourceFromDatapoint = false;
        // Steigt bei JEDER erfolgreichen Tagesertrag-Abfrage (auch wenn
        // der Wert gleich bleibt) - YieldLedger (Core 1) nutzt das, um zu
        // wissen, wann die History-Backup-Datei neu geschrieben werden
        // muss (siehe CLAUDE.md "Backup-Ordner").
        uint32_t dayFetchSeq = 0;

        time_t lastSuccessfulFetch = 0;
        // Geplanter Zeitpunkt der naechsten Ertrags-/Batterie-/Leistungs-
        // Abfrage (Normal- oder Wiederholtakt, siehe fetch_task.cpp) -
        // ersetzt die vorher doppelt angezeigte "Messung HH:MM"-Zeile auf
        // den Seiten "Leistung"/"Batterie" durch "Naechste Abfrage: HH:MM"
        // (Auftrag). 0 = noch kein Zeitplan bekannt (keine Verbindung/kein
        // Konto) - Anzeige zeigt dann "--:--".
        time_t nextFetchAt = 0;
        char statusText[64] = {0}; // Klartext-Fehlermeldung, leer = kein Fehler
        FetchStatus fetchStatus = FetchStatus::NoAccount;

        // Steigt bei JEDEM erfolgreich abgeschlossenen Ertrags-/Batterie-
        // Abfragezyklus UNBEDINGT (anders als changeCounter unten, der nur
        // bei echter Wertaenderung steigt) - fetch_watchdog.cpp nutzt das,
        // um den Neustart-Zaehler nach ein paar echten erfolgreichen
        // Zyklen zurueckzusetzen, nicht schon bei einem blossen
        // Herzschlag (siehe CLAUDE.md "Fetch-Task-Absicherung").
        uint32_t successfulFetchCount = 0;

        // Steigt bei jeder Aktualisierung - Core 1 vergleicht ihn, um nur
        // bei echter Aenderung neu zu zeichnen statt bei jedem Tick.
        uint32_t changeCounter = 0;
    };

    // Einmal beim Start aufrufen (legt den Mutex an).
    void init();

    // Core 1: liefert eine konsistente Momentaufnahme.
    Data snapshot();

    // Ab hier NUR vom Fetch-Task (Core 0) aufrufen - jede Funktion sperrt
    // intern kurz den Mutex, schreibt, erhoeht changeCounter und gibt ihn
    // wieder frei.
    void setPower(float watts, time_t measuredAt);
    void pushHistoryPoint(time_t epoch, float watts);
    // Ersetzt den kompletten Verlauf auf einmal (ein Mutex-Zyklus statt
    // HISTORY_MAX_POINTS einzelner) - die Port-7002-Kurvenabfrage liefert
    // ohnehin jedes Mal das komplette 3-Stunden-Fenster neu, kein
    // inkrementelles Anhaengen noetig. points MUSS aeltester zuerst
    // sortiert sein (siehe CLAUDE.md "Seite Leistung").
    void setHistory(const HistoryPoint* points, uint8_t count);
    void setEnergy(float todayKwh, float monthKwh, float yearKwh, float totalKwh);
    void setBattery(float voltage, float current, float socPercent, float tempC, time_t measuredAt);
    void setPv(float voltage, float current, time_t measuredAt);
    // Wird nur vom Fetch-Task aufgerufen, wenn sich die Geraete-
    // Seriennummer geaendert hat (siehe fetch_task.cpp::
    // applyAccountSnapshot()) - verwirft Leistung/Batterie/PV/Verlauf/
    // Ertrag, damit nie veraltete Zahlen der alten Anlage angezeigt
    // werden, waehrend die erste Abfrage mit der neuen Nummer laeuft.
    void resetForDeviceChange();
    void setChargeStatusCode(int16_t code);
    void setChargeStatusText(const char* text);
    void setDailySource(float kwh, time_t measuredAt, bool fromDatapoint);
    void setLastSuccessfulFetch(time_t epoch);
    void setNextFetchAt(time_t epoch);
    void setStatusText(const char* text);
    void setFetchStatus(FetchStatus status);
}
