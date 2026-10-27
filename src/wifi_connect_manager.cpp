#include "wifi_connect_manager.h"
#include "app_config.h"
#include "wifi_status.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

// WICHTIG (Dual-Core-Regel, siehe CLAUDE.md): Dieses Modul laeuft
// ausschliesslich im Fetch-Task auf Core 0 und darf deshalb NIE die
// SD-Karte beruehren. Es liest appConfig.wifiSlots/lastActiveWifiSlot nur
// lesend (angelegt/geaendert wird das weiterhin auf Core 1, siehe
// networks_screen.cpp, das den Fetch-Task waehrenddessen pausiert - siehe
// fetch_task.h) und meldet einen erfolgreichen Verbindungswechsel ueber
// WifiStatus (eigener kleiner Mutex) an Core 1 zurueck, das dort dann
// appConfig.lastActiveWifiSlot aktualisiert und speichert.

namespace WifiConnectManager {

namespace {
    uint8_t order[Config::MAX_WIFI_SLOTS];
    uint8_t orderCount = 0;
    uint8_t orderPos = 0;
    uint32_t attemptStartMs = 0;
    bool attemptActive = false;
    uint8_t currentConnectedSlot = 255;
    bool ntpConfigured = false;

    void buildOrder() {
        orderCount = 0;
        bool used[Config::MAX_WIFI_SLOTS] = {false, false, false};

        if (appConfig.lastActiveWifiSlot < Config::MAX_WIFI_SLOTS &&
            appConfig.wifiSlots[appConfig.lastActiveWifiSlot].ssid.length() > 0) {
            order[orderCount++] = appConfig.lastActiveWifiSlot;
            used[appConfig.lastActiveWifiSlot] = true;
        }
        for (uint8_t i = 0; i < Config::MAX_WIFI_SLOTS; i++) {
            if (used[i]) continue;
            if (appConfig.wifiSlots[i].ssid.length() == 0) continue;
            order[orderCount++] = i;
        }
    }

    void startAttempt(uint8_t slot) {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        const AppConfig::WifiSlot& s = appConfig.wifiSlots[slot];
        if (s.password.length() > 0) {
            WiFi.begin(s.ssid.c_str(), s.password.c_str());
        } else {
            WiFi.begin(s.ssid.c_str());
        }
        attemptStartMs = millis();
        attemptActive = true;
    }
}

void begin() {
    buildOrder();
    orderPos = 0;
    currentConnectedSlot = 255;
    attemptActive = false;
    WifiStatus::set(false, 255);
    if (orderCount > 0) startAttempt(order[0]);
}

void connectSlotNow(uint8_t slot) {
    if (slot >= Config::MAX_WIFI_SLOTS || appConfig.wifiSlots[slot].ssid.length() == 0) return;
    buildOrder();
    for (uint8_t i = 0; i < orderCount; i++) {
        if (order[i] == slot) { orderPos = i; startAttempt(slot); return; }
    }
}

void tick() {
    if (WiFi.status() == WL_CONNECTED) {
        // Ermittelt den verbundenen Platz anhand der tatsaechlich aktiven
        // SSID, nicht anhand des eigenen Versuchs-Zyklus - deckt damit
        // auch Verbindungen ab, die ausserhalb dieses Moduls zustande
        // kamen (z.B. ein frisch im Netzwerke-Bildschirm getesteter neuer
        // Platz, siehe networks_screen.cpp).
        String activeSsid = WiFi.SSID();
        uint8_t matched = 255;
        for (uint8_t i = 0; i < Config::MAX_WIFI_SLOTS; i++) {
            if (appConfig.wifiSlots[i].ssid == activeSsid) { matched = i; break; }
        }
        if (matched != currentConnectedSlot) {
            currentConnectedSlot = matched;
            WifiStatus::set(true, matched);
#ifdef NIGHT_MODE_DIAG_TEST
            // Auftrag "Diagnosezeilen fuer die naechste Nacht" - Zustands-
            // wechsel mit Dauer (nur wenn ein eigener Verbindungsversuch
            // lief, sonst ist "Dauer" nicht aussagekraeftig, siehe
            // attemptActive).
            struct tm nt;
            char hhmm[6] = "--:--";
            if (getLocalTime(&nt, 5)) snprintf(hhmm, sizeof(hhmm), "%02d:%02d", nt.tm_hour, nt.tm_min);
            if (attemptActive) {
                Serial.printf("[wifi] Verbunden (Platz %u) um %s, Versuch dauerte %lu ms\n",
                               (unsigned)matched, hhmm, (unsigned long)(millis() - attemptStartMs));
            } else {
                Serial.printf("[wifi] Verbunden (Platz %u) um %s (ausserhalb eigenen Verbindungszyklus)\n",
                               (unsigned)matched, hhmm);
            }
#endif
        }
        attemptActive = false;
        if (!ntpConfigured) {
            ntpConfigured = true;
            configTzTime(Config::TIMEZONE_POSIX, Config::NTP_SERVER);
        }
        return;
    }

    // Verbindung verloren, nachdem sie stand - Zyklus von vorne beginnen.
    if (currentConnectedSlot != 255) {
#ifdef NIGHT_MODE_DIAG_TEST
        struct tm nt;
        char hhmm[6] = "--:--";
        if (getLocalTime(&nt, 5)) snprintf(hhmm, sizeof(hhmm), "%02d:%02d", nt.tm_hour, nt.tm_min);
        Serial.printf("[wifi] Verbindung verloren (war Platz %u) um %s - Versuch, neu zu verbinden\n",
                       (unsigned)currentConnectedSlot, hhmm);
#endif
        currentConnectedSlot = 255;
        begin();
        return;
    }

    if (!attemptActive || orderCount == 0) return;

    if (millis() - attemptStartMs >= Config::WIFI_CONNECT_TIMEOUT_MS) {
#ifdef NIGHT_MODE_DIAG_TEST
        struct tm nt;
        char hhmm[6] = "--:--";
        if (getLocalTime(&nt, 5)) snprintf(hhmm, sizeof(hhmm), "%02d:%02d", nt.tm_hour, nt.tm_min);
        Serial.printf("[wifi] Versuch auf Platz %u nach %lu ms ohne Erfolg (Zeitlimit) um %s - naechster Platz\n",
                       (unsigned)order[orderPos], (unsigned long)(millis() - attemptStartMs), hhmm);
#endif
        orderPos = (uint8_t)((orderPos + 1) % orderCount);
        startAttempt(order[orderPos]);
    }
}

uint8_t connectedSlot() { return currentConnectedSlot; }

}
