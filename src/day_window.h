#pragma once
// Reine Zeitfenster-Logik fuer das Tagesfenster/Nachtmodus-Feature (siehe
// CLAUDE.md "Tagesfenster/Nachtmodus" und config.h DAY_START_HOUR/
// DAY_END_HOUR/FINAL_QUERY_DEADLINE_HOUR). Zustandslos, reine Funktionen
// von "struct tm"/time_t - KEINE SD-/TFT-Zugriffe, KEIN Mutex, deshalb
// unbedenklich von BEIDEN Cores aufrufbar (Fetch-Task auf Core 0 fuer die
// Abfrage-Pause, YieldLedger auf Core 1 fuer die Nachtmodus-Anzeige) ohne
// die in CLAUDE.md "Dual-Core-Architektur" beschriebenen Cross-Core-
// Bruecken zu brauchen.
#include <Arduino.h>
#include <time.h>
#include "config.h"

namespace DayWindow {
    // Minuten seit Mitternacht, lokal.
    inline uint16_t minutesOfDay(const struct tm& t) {
        return (uint16_t)(t.tm_hour * 60 + t.tm_min);
    }

    // [04:30, 22:30) - normaler Abfragetakt wie bisher.
    inline bool isDayWindow(const struct tm& nowTm) {
        uint16_t m = minutesOfDay(nowTm);
        uint16_t start = (uint16_t)(Config::DAY_START_HOUR * 60 + Config::DAY_START_MINUTE);
        uint16_t end = (uint16_t)(Config::DAY_END_HOUR * 60 + Config::DAY_END_MINUTE);
        return m >= start && m < end;
    }

    // [22:30, 22:55) - Abschlussabfrage-Fenster (einmal erfolgreich
    // abfragen, bei Fehlschlag alle paar Minuten wiederholen).
    inline bool isFinalQueryWindow(const struct tm& nowTm) {
        uint16_t m = minutesOfDay(nowTm);
        uint16_t start = (uint16_t)(Config::DAY_END_HOUR * 60 + Config::DAY_END_MINUTE);
        uint16_t deadline = (uint16_t)(Config::FINAL_QUERY_DEADLINE_HOUR * 60 + Config::FINAL_QUERY_DEADLINE_MINUTE);
        return m >= start && m < deadline;
    }

    // Alles ausserhalb von Tagesfenster und Abschlussfenster - [22:55,
    // 04:30) ueber Mitternacht hinweg. In dieser Zeit laeuft NIE eine
    // Abfrage.
    inline bool isNightWindow(const struct tm& nowTm) {
        return !isDayWindow(nowTm) && !isFinalQueryWindow(nowTm);
    }

    // "YYYY-MM-DD" des Abends, zu dem die aktuelle Nacht gehoert (eine
    // Nacht erstreckt sich ueber Mitternacht - vor 12:00 Uhr lokal ist das
    // der VORTag, ab 12:00 Uhr der heutige Tag). Puffer muss mindestens
    // 11 Byte gross sein. Wird gebraucht, um die Nachtmodus-Bestaetigung
    // (siehe yield_ledger.h) ueber Mitternacht hinweg demselben Abend
    // zuzuordnen, unabhaengig davon, ob "jetzt" vor oder nach Mitternacht
    // liegt.
    inline void currentNightEveningDate(time_t now, const struct tm& nowTm, char* outDateStr, size_t size) {
        if (nowTm.tm_hour >= 12) {
            strftime(outDateStr, size, "%Y-%m-%d", &nowTm);
        } else {
            time_t yesterday = now - 24L * 3600L;
            struct tm yTm;
            localtime_r(&yesterday, &yTm);
            strftime(outDateStr, size, "%Y-%m-%d", &yTm);
        }
    }

    // Epoch des naechsten Tagesbeginns (DAY_START_HOUR:DAY_START_MINUTE) -
    // falls "now" bereits nach dem heutigen Tagesbeginn liegt (z.B.
    // tagsueber oder abends), wird morgen frueh geliefert, sonst heute
    // (z.B. kurz nach Mitternacht, noch vor 04:30).
    inline time_t nextDayStartEpoch(time_t now) {
        struct tm nowTm;
        localtime_r(&now, &nowTm);
        struct tm startTm = nowTm;
        startTm.tm_hour = Config::DAY_START_HOUR;
        startTm.tm_min = Config::DAY_START_MINUTE;
        startTm.tm_sec = 0;
        time_t candidate = mktime(&startTm);
        if (candidate <= now) {
            time_t tomorrow = now + 24L * 3600L;
            struct tm tmrTm;
            localtime_r(&tomorrow, &tmrTm);
            tmrTm.tm_hour = Config::DAY_START_HOUR;
            tmrTm.tm_min = Config::DAY_START_MINUTE;
            tmrTm.tm_sec = 0;
            candidate = mktime(&tmrTm);
        }
        return candidate;
    }
}
