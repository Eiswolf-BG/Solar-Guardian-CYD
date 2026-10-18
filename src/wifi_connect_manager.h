#pragma once
// Nicht-blockierendes Verbinden mit den bis zu 3 gespeicherten WLAN-
// Plaetzen (appConfig.wifiSlots). Versucht beim Start zuerst das zuletzt
// aktive Netz (appConfig.lastActiveWifiSlot), danach die uebrigen Plaetze
// der Reihe nach - jeweils mit Config::WIFI_CONNECT_TIMEOUT_MS Timeout,
// bevor der naechste Platz versucht wird.
//
// Laeuft ausschliesslich im Fetch-Task auf Core 0 (siehe fetch_task.h) -
// greift NIE auf TFT oder SD-Karte zu. Erfolgreiche Verbindungswechsel
// werden ueber WifiStatus (eigener Mutex) an Core 1 gemeldet, das dort
// appConfig.lastActiveWifiSlot aktualisiert und speichert.
#include <Arduino.h>

namespace WifiConnectManager {
    // Startet den Verbindungsversuch-Zyklus neu von vorne (zuletzt aktives
    // Netz zuerst). Vom Fetch-Task beim Start und nach einem
    // ReconnectNow-Befehl aufgerufen.
    void begin();

    // Versucht sofort einen bestimmten Platz (0-2), unabhaengig von der
    // normalen Reihenfolge - fuer den "mit Netzwerk X verbinden"-Befehl.
    void connectSlotNow(uint8_t slot);

    // Im Fetch-Task-Loop regelmaessig aufrufen. Treibt den Zyklus voran.
    void tick();

    // Index (0-2) des aktuell verbundenen Platzes, oder 255 wenn keine
    // Verbindung besteht - fuer den Haken im Netzwerke-Bildschirm.
    uint8_t connectedSlot();
}
