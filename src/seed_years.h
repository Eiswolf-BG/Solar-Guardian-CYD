#pragma once
// Startwerte fuer years.json, falls die Datei noch nicht existiert (siehe
// yield_ledger.h/.cpp). Leicht hier aenderbar, ohne im Code suchen zu
// muessen.
#include <Arduino.h>

namespace SeedYears {
    struct Entry {
        const char* year;
        float kwh;
    };
    constexpr Entry SEEDS[] = {
        {"2024", 4452.04f},
        {"2025", 4453.23f},
    };
    constexpr uint8_t SEED_COUNT = sizeof(SEEDS) / sizeof(SEEDS[0]);
}
