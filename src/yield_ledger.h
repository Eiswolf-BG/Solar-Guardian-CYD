#pragma once
// Das Ertragsbuch: zaehlt Woche (gleitendes 7-Tage-Fenster)/Monat/Jahr
// selbst, weil die entsprechenden Portal-Summen bekanntermassen falsch
// bzw. lueckenhaft sind (siehe CLAUDE.md "Ertragsbuch"). Laeuft komplett
// auf Core 1 (benutzt die SD-Karte), liest den Tagesertrag-Rohwert aus
// SolarData (vom Fetch-Task auf Core 0 befuellt). Schreibt auch die
// History-Backup-Datei (/SolarGuardian/History/tagesertrag.json) und
// years.json (Jahresuebersicht, siehe years_page.h).
#include <Arduino.h>

namespace YieldLedger {

    // Einmal aufrufen, nachdem die SD-Karte gemountet ist (main.cpp) -
    // laedt yield.json, startet bei fehlender/defekter Datei neu (wie bei
    // der allerersten Inbetriebnahme) und meldet das im seriellen Monitor.
    void init();

    // In jeder loop()-Iteration aufrufen (main.cpp), unabhaengig davon, ob
    // die Seite "Ertrag" gerade angezeigt wird.
    void tick();

    // Schreibt sofort auf die SD-Karte, falls ungespeicherte Aenderungen
    // anstehen (umgeht das normale Mindestintervall, siehe
    // Config::YIELD_SAVE_MIN_INTERVAL_MS) - vor einem erzwungenen Neustart
    // aufrufen (siehe fetch_watchdog.h), damit kein Tagesertrag verloren
    // geht. NUR von Core 1 aufrufen (wie tick(), benutzt die SD-Karte).
    void flush();

    struct Values {
        float todayKwh = 0.0f;
        float weekKwh = 0.0f;  // gleitendes 7-Tage-Fenster (heute + 6 Vortage)
        float monthKwh = 0.0f; // Kalendermonat
        float yearKwh = 0.0f;  // Kalenderjahr
        bool valid = false;    // false = noch nie initialisiert, Anzeige zeigt "--"
    };
    Values current();

    enum class SetTargetResult { Ok, Negative, NoDataYet };

    // "Ertragswerte"-Menue: der eingegebene Wert gilt INKLUSIVE des
    // heutigen Tages - die neue Basis wird intern als
    // (eingegebener Wert - aktueller Tageswert) gespeichert.
    SetTargetResult setMonthTarget(float desiredIncludingToday);
    SetTargetResult setYearTarget(float desiredIncludingToday);

    // Fuer die Seite "Jahre" (years_page.h): bis zu maxCount gespeicherte
    // VORJAHRE (ohne das laufende Jahr, das kommt live aus current()),
    // aufsteigend nach Jahr sortiert, neueste zuerst ausgewaehlt falls
    // mehr als maxCount Jahre gespeichert sind. Liest years.json frisch
    // bei jedem Aufruf (kleine Datei).
    struct YearEntry {
        int year = 0;
        float kwh = 0.0f;
    };
    uint8_t pastYears(YearEntry* out, uint8_t maxCount);

    // Fuer die Seite "Jahre", Reiter "Monate": bis zu maxCount gespeicherte
    // VORMONATE (ohne den laufenden Monat, der kommt live aus current()),
    // aufsteigend nach Monat sortiert, neueste zuerst ausgewaehlt falls
    // mehr als maxCount Monate gespeichert sind. Liest months.json frisch
    // bei jedem Aufruf (kleine Datei, siehe Config::SD_MONTHS_FILE).
    struct MonthEntry {
        char key[8] = {0}; // "YYYY-MM"
        float kwh = 0.0f;
    };
    uint8_t pastMonths(MonthEntry* out, uint8_t maxCount);

    // Fuer die Seite "Jahre", Reiter "Tage": bis zu maxCount abgeschlossene
    // Vortage (aeltester zuerst) PLUS der heutige, noch laufende Tag als
    // letzter Eintrag - zusammen das gleiche gleitende Fenster, das auch
    // current().weekKwh aufsummiert, hier aber als Einzelwerte statt nur
    // die Summe. Liefert 0, falls das Ertragsbuch noch nicht initialisiert
    // ist (siehe current().valid).
    struct DayValue {
        char date[11] = {0}; // "YYYY-MM-DD"
        float kwh = 0.0f;
    };
    uint8_t lastDaysIncludingToday(DayValue* out, uint8_t maxCount);

    // Steigt bei jeder gespeicherten Aenderung (Tages-/Monats-/
    // Jahreswechsel, manuelle Korrektur, Jahresabschluss in years.json) -
    // z.B. fuer years_page.cpp's update(), um zu wissen, wann neu
    // gezeichnet werden muss.
    uint32_t revision();

    // Tagesfenster/Nachtmodus (Auftrag "Tagesfenster mit Nachtmodus"):
    // true nur zwischen 22:55 und 04:30 lokal UND wenn die Abschlussabfrage
    // fuer den vergangenen Abend bestaetigt ist (siehe CLAUDE.md) - steuert
    // die Mondsichel+"Nachtmodus"-Anzeige auf den Seiten, die "Naechste
    // Abfrage:" zeigen (power_page.cpp/battery_page.cpp) und in der WebUI.
    // Rein aus Uhrzeit + gespeicherten Daten abgeleitet, funktioniert auch
    // direkt nach einem Neustart mitten in der Nacht, ohne neue Abfrage.
    bool isNightModeConfirmedNow();
}
