#pragma once
// Zaehler/Zeitfenster-Pruefung fuer das Aktiv-Auslesen (Auftrag
// "Aktualisieren-Knopf soll echt etwas bewirken", Schutz gegen Dauer-
// Tippen, Korrektur B3) - reine Zustands-/Zeitlogik, "now" als Parameter
// statt echtem millis(), damit ein nativer Test (test/, gegen das
// projekteigene fake_arduino/Arduino.h-Attrappen-Setup kompiliert) diese
// Datei DIREKT einbindet und mit einer simulierten Uhr prueft, statt die
// Logik nur nachzubilden. Reiner RAM-Zustand (kein Flash/SD) - nach
// einem Neustart beginnt die Zaehlung bewusst wieder bei 0 (kein
// dauerhaftes Sperr-Gedaechtnis noetig, die Grenzen schuetzen die
// Anlage nur waehrend des laufenden Betriebs).
//
// WICHTIG (Korrektur B3): die Stunden-/Tagesobergrenze zaehlt BEFEHLE
// (jeder einzelne aktiv ausgeloeste Datenpunkt = ein eigener Eintrag im
// Operationsprotokoll der Anlage), NICHT Antippen - ein Tipp
// kann je nach Bereich mehrere Befehle ausloesen (siehe CLAUDE.md
// Tabelle). Der Mindestabstand (60s) gilt dagegen zwischen zwei TIPPEN,
// unabhaengig davon, wie viele Befehle der jeweilige Tipp ausloest.
#include <stdint.h>
#include "config.h"

namespace ActiveReadLimiter {
    // Kapazitaet = Tagesobergrenze an BEFEHLEN - mehr Eintraege braucht
    // der rollierende Tages-Zaehler ohnehin nie gleichzeitig.
    constexpr uint8_t HISTORY_CAPACITY = Config::EPEVER_ACTIVE_READ_MAX_PER_DAY;

    struct State {
        uint32_t lastTapMs = 0; // Mindestabstand-Pruefung, siehe allowTap()
        bool everTapped = false;
        uint32_t commandTimestamps[HISTORY_CAPACITY] = {0};
        uint8_t count = 0;    // Anzahl gueltig befuellter Eintraege (bis HISTORY_CAPACITY)
        uint8_t nextSlot = 0; // naechster zu ueberschreibender Ringpuffer-Platz
    };

    // Anzahl BEFEHLE, deren Zeitstempel hoechstens windowMs vor "now"
    // liegen - unsigned-Subtraktion ist bewusst gewaehlt (millis()-
    // Ueberlauf nach ~49 Tagen bleibt dadurch korrekt, gleiches Muster
    // wie an anderen Stellen im Projekt).
    inline uint8_t countSince(const State& s, uint32_t now, uint32_t windowMs) {
        uint8_t n = 0;
        for (uint8_t i = 0; i < s.count; i++) {
            if ((uint32_t)(now - s.commandTimestamps[i]) <= windowMs) n++;
        }
        return n;
    }

    // Darf EIN Tipp, der commandCount Befehle ausloesen wuerde, jetzt
    // gestartet werden? Prueft, ob NACH diesen commandCount zusaetzlichen
    // Befehlen die Stunden-/Tagesobergrenze noch eingehalten waere -
    // nicht nur, ob schon EIN weiterer Befehl passen wuerde.
    inline bool allowTap(const State& s, uint32_t now, uint8_t commandCount) {
        if (s.everTapped && (uint32_t)(now - s.lastTapMs) < Config::EPEVER_ACTIVE_READ_MIN_INTERVAL_MS) return false;
        if ((uint32_t)countSince(s, now, 60UL * 60UL * 1000UL) + commandCount > Config::EPEVER_ACTIVE_READ_MAX_PER_HOUR) return false;
        if ((uint32_t)countSince(s, now, 24UL * 60UL * 60UL * 1000UL) + commandCount > Config::EPEVER_ACTIVE_READ_MAX_PER_DAY) return false;
        return true;
    }

    // Nur aufrufen, wenn der Tipp tatsaechlich ALLE seine Befehle sendet
    // (nicht erst bei Erfolg) - jeder einzelne Operationsprotokoll-
    // Eintrag entsteht schon mit dem SENDEN, unabhaengig vom Ergebnis.
    inline void recordTap(State& s, uint32_t now, uint8_t commandCount) {
        s.lastTapMs = now;
        s.everTapped = true;
        for (uint8_t i = 0; i < commandCount; i++) {
            s.commandTimestamps[s.nextSlot] = now;
            s.nextSlot = (uint8_t)((s.nextSlot + 1) % HISTORY_CAPACITY);
            if (s.count < HISTORY_CAPACITY) s.count++;
        }
    }
}
