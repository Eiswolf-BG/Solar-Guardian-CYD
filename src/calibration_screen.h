#pragma once
// Klassische Touch-Kalibrierung mit Kreuzen in den Ecken. Setzt am Ende
// appConfig.touchSwapXY/touchScreenXRaw*/touchScreenYRaw* (noch NICHT
// gespeichert - main.cpp speichert die gesamte Konfiguration erst am Ende
// der Ersteinrichtung).
#include <Arduino.h>

namespace CalibrationScreen {
    void run();
}
