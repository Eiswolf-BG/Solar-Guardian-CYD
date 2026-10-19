#pragma once
// Absichert jede HTTPS-Abfrage im Fetch-Task gegen einen Server, der auf
// Socket-Ebene "lebt" (einzelne Lese-/Verbindungsvorgaenge unterschreiten
// ihren jeweiligen Pro-Operation-Timeout), insgesamt aber viel zu lange
// braucht - genau der Fall, der beim Test der Leistungskurve beobachtet
// wurde (ein mehrminuetiger Haenger trotz gesetztem HTTPClient-Timeout,
// siehe CLAUDE.md "Fetch-Task-Absicherung"). Ein Software-Timer
// (esp_timer), der bei Ablauf WiFiClientSecure::stop() aufruft - das
// laesst jeden GERADE blockierenden Aufruf der HTTPClient-Bibliothek
// (connect()/read() ueber getString()) sofort mit einem Fehler
// zurueckkehren, den die aufrufende Funktion wie jeden anderen
// Verbindungsfehler behandelt (bestehender Wiederholungs-Ablauf greift
// unveraendert).
#include <Arduino.h>
#include <WiFiClientSecure.h>

namespace HttpDeadlineGuard {
    // Einmal beim Start aufrufen (legt den Timer an).
    void init();

    // Startet/erneuert die Frist fuer GENAU EINEN WiFiClientSecure.
    // NUR vom Fetch-Task aufrufen - der fuehrt alle HTTPS-Abfragen
    // sequenziell aus (nie zwei gleichzeitig), ein gemeinsamer Timer
    // reicht deshalb.
    void arm(WiFiClientSecure* client, uint32_t timeoutMs);

    // Nach einer abgeschlossenen Abfrage aufrufen, BEVOR "client" den
    // Gueltigkeitsbereich verlaesst - sonst koennte der Timer spaeter auf
    // eine bereits zerstoerte lokale Variable zugreifen.
    void disarm();

    // true, wenn der zuletzt bewachte Aufruf durch dieses Gesamt-
    // Zeitlimit (nicht durch einen normalen Verbindungsfehler) beendet
    // wurde - fuer ein praeziseres Diagnose-Log.
    bool lastCallTimedOut();
}
