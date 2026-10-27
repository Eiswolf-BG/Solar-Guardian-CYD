#pragma once
// Datenpunkt-IDs der Port-7002-Abfrage (getSampleDataPoint), siehe
// epever_client.h/.cpp und CLAUDE.md.
#include <Arduino.h>

namespace EpeverDatapoints {
    constexpr int32_t BATTERY_VOLTAGE    = 101596357; // V
    constexpr int32_t BATTERY_CURRENT    = 101596376; // A
    constexpr int32_t CHARGE_STATUS      = 101596385;
    constexpr int32_t BATTERY_TEMP       = 101596387; // Grad C
    constexpr int32_t BATTERY_SOC        = 101596397; // %
    constexpr int32_t DAILY_GENERATION   = 101596358; // kWh ("Daily Generation")

    // Seite "Leistung" (siehe CLAUDE.md) - PV_POWER bekommt eine eigene
    // Anfrage mit grossem sampleLimit (3-Stunden-Kurve), PV_VOLTAGE/
    // PV_CURRENT laufen in der bestehenden Batterieabfrage mit (kleines
    // Limit).
    constexpr int32_t PV_POWER           = 101596388; // W
    constexpr int32_t PV_VOLTAGE         = 101596362; // V
    constexpr int32_t PV_CURRENT         = 101596380; // A

    // Tages-Extremwerte der Batteriespannung (Auftrag "Max/Min-
    // Spannungskacheln") - laufen in der bestehenden Batterieabfrage mit
    // (kein zusaetzlicher HTTP-Aufruf), exakt derselbe Mechanismus wie
    // die Werte oben. Von Alex direkt aus der Portal-Spalte
    // "Parameter ID" uebernommen, stimmen mit den bisherigen 8 IDs
    // (gleiches Nummernschema) ueberein.
    constexpr int32_t BATTERY_VOLTAGE_MAX = 101596406; // V
    constexpr int32_t BATTERY_VOLTAGE_MIN = 101596413; // V
}
