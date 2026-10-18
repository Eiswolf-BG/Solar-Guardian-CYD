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

    // Steigt bei jeder gespeicherten Aenderung (Tages-/Monats-/
    // Jahreswechsel, manuelle Korrektur, Jahresabschluss in years.json) -
    // z.B. fuer years_page.cpp's update(), um zu wissen, wann neu
    // gezeichnet werden muss.
    uint32_t revision();
}
