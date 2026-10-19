#pragma once
// Einmaliger historischer Ertragsimport fuer dieses eine Geraet (Steffens
// Anlage, siehe CLAUDE.md) - eigene Datei, damit die site-spezifischen
// Zahlen nach der einmaligen Befuellung leicht wiedergefunden/entfernt
// werden koennen (gleiches Prinzip wie seed_years.h). Laeuft genau
// einmal, siehe yield_ledger.cpp::importHistoricalData() und die
// Markerdatei Config::SD_IMPORT_MARKER.
#include <Arduino.h>

namespace SteffenImport {
    struct DayEntry {
        const char* date; // "YYYY-MM-DD"
        float kwh;
    };

    // Tageswerte Oktober 2026 (der 18. laeuft zum Zeitpunkt des Imports
    // noch live und ist bewusst NICHT enthalten).
    constexpr DayEntry DAYS_OCT_2026[] = {
        {"2026-10-01", 10.47f}, {"2026-10-02", 10.23f}, {"2026-10-03", 10.01f},
        {"2026-10-04", 12.07f}, {"2026-10-05", 13.13f}, {"2026-10-06", 13.21f},
        {"2026-10-07", 11.34f}, {"2026-10-08",  2.34f}, {"2026-10-09", 11.32f},
        {"2026-10-10",  5.69f}, {"2026-10-11",  7.34f}, {"2026-10-12", 10.71f},
        {"2026-10-13",  7.22f}, {"2026-10-14", 12.29f}, {"2026-10-15", 11.16f},
        {"2026-10-16", 11.80f}, {"2026-10-17",  1.60f},
    };
    constexpr uint8_t DAYS_OCT_2026_COUNT = sizeof(DAYS_OCT_2026) / sizeof(DAYS_OCT_2026[0]);

    // Monatssummen Januar-September 2026 (Kontrollsumme: 3760.15 kWh).
    constexpr float MONTHS_JAN_SEP_2026[] = {
        118.05f, 222.45f, 340.31f, 509.11f, 473.47f, 553.99f, 633.76f, 452.16f, 456.85f,
    };
    constexpr uint8_t MONTHS_JAN_SEP_2026_COUNT = sizeof(MONTHS_JAN_SEP_2026) / sizeof(MONTHS_JAN_SEP_2026[0]);
}
