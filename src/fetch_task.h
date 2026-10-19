#pragma once
// Der Fetch-Task: ein eigener FreeRTOS-Task, fest auf Core 0 (siehe
// CLAUDE.md "Dual-Core-Architektur"). Kuemmert sich um WLAN-Verbindung/
// Wiederverbindung, NTP-Zeit und (spaeter) alle API-Abfragen. Greift NIE
// auf TFT oder SD-Karte zu - Ergebnisse laufen ueber SolarData (Mutex +
// Momentaufnahme) und WifiStatus, Befehle von Core 1 ueber eine Queue.
#include <Arduino.h>

namespace FetchTask {

    enum class Command : uint8_t {
        ReconnectNow,
        DisconnectWifi,
        ConnectSlot0,
        ConnectSlot1,
        ConnectSlot2,
        // Noch ohne eigene Wirkung (der Fetch-Task fuehrt noch keine
        // API-Abfragen aus) - fuer spaetere Features vorbereitet, siehe
        // credentials/account/language-Screens, die diese Befehle bereits
        // nach jeder Aenderung senden.
        AccountChanged,
        LanguageChanged,
        // Standort fuer die Wetterabfrage geaendert (siehe
        // location_screen.cpp) - loest sofort eine neue Wetterabfrage aus,
        // statt auf den naechsten 30-Minuten-Takt zu warten.
        LocationChanged,
        // "Jetzt aktualisieren" (siehe pages.cpp, Seite "Ertrag") - fuehrt
        // sofort eine Ertragsabfrage aus, aber hoechstens alle
        // Config::EPEVER_MANUAL_REFRESH_MIN_INTERVAL_MS.
        RefreshNow,
    };

    // Einmal aufrufen, sobald eine Konfiguration geladen ist (Core 1,
    // main.cpp). Legt Mutex/Queue an und startet den Task
    // (xTaskCreatePinnedToCore, Core 0, Stack 12288 Byte, Prioritaet 1).
    void start();

    // Von Core 1 aufrufen, BEVOR ein Bildschirm selbst blockierend scannt/
    // verbindet (siehe wifi_select_screen.cpp, networks_screen.cpp) - der
    // Fetch-Task fasst waehrenddessen das WLAN-Modul nicht an. Kehrt erst
    // zurueck, wenn der Task die Pause bestaetigt hat (kurzes Warten,
    // laeuft auf Core 1, kein Problem). resume() danach nicht vergessen.
    void pause();
    void resume();

    // Einen Befehl in die Queue legen (nicht-blockierend, wird verworfen
    // falls die Queue voll ist - das ist bei max. 4 Plaetzen und seltenen
    // Befehlen praktisch nie der Fall).
    void sendCommand(Command cmd);
}
