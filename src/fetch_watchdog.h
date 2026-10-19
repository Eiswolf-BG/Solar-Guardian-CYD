#pragma once
// Wacht ueber den Fetch-Task (siehe CLAUDE.md "Fetch-Task-Absicherung")
// - Core 1 prueft hierueber regelmaessig den Herzschlag aus
// fetch_heartbeat.h. Bewusst ein eigener, selbst kontrollierter
// Mechanismus statt des System-Watchdogs (esp_task_wdt): nur so laesst
// sich zuerst eine laengere Karenzzeit mit reiner Log-Meldung geben,
// danach das Ertragsbuch sicher auf die SD-Karte schreiben, BEVOR
// esp_restart() aufgerufen wird - ein ausgeloester esp_task_wdt wuerde
// stattdessen sofort (ueblicherweise nach wenigen Sekunden, nicht
// Minuten) einen Panic/Reset ausloesen, ohne Gelegenheit fuer einen
// geordneten Abschluss, und seine globale Zeitschranke (sdkconfig)
// wuerde sich mit der des ohnehin schon ueberwachten Arduino-loop()-Tasks
// ueberschneiden.
#include <Arduino.h>

namespace FetchWatchdog {
    // Core 1, in main.cpp::loop() aufrufen. Benutzt die SD-Karte (ueber
    // YieldLedger::flush() im Ernstfall) - deshalb wie YieldLedger/
    // MainScreen nur von Core 1.
    void tick();
}
