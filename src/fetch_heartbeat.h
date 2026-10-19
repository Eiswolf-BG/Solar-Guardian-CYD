#pragma once
// Lebenszeichen des Fetch-Tasks (Core 0) fuer Core 1 - siehe CLAUDE.md
// "Fetch-Task-Absicherung" und fetch_watchdog.h. Haengt der Fetch-Task in
// einer blockierenden Abfrage fest (z.B. ein Server, der trotz
// bestehender Verbindung nie antwortet), bleibt dieser Zeitstempel
// stehen - Core 1 erkennt das ueber den wachsenden Abstand zu
// millis() und reagiert (siehe fetch_watchdog.cpp). Gleiches kleine
// Mutex-Prinzip wie wifi_status.h.
#include <Arduino.h>

namespace FetchHeartbeat {
    void init();

    // Fetch-Task (Core 0): nach jedem abgeschlossenen Durchlauf der
    // Haupt-Schleife aufrufen (auch wenn an diesem Durchlauf keine
    // Abfrage faellig war) - ein Haengenbleiben INNERHALB einer Abfrage
    // verhindert dann zuverlaessig den naechsten Herzschlag.
    void beat();

    // Core 1: Millisekunden seit dem letzten Herzschlag.
    uint32_t msSinceLastBeat();
}
