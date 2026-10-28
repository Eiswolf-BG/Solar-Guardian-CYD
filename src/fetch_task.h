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
        // Config::EPEVER_MANUAL_REFRESH_MIN_INTERVAL_MS. Fuer das
        // Aktiv-Auslesen (Auftrag "Aktualisieren-Knopf soll echt etwas
        // bewirken") IMMER ueber sendRefreshNow() mit einer passenden
        // RefreshRequest ausloesen, nicht ueber sendCommand(RefreshNow)
        // direkt (siehe dort).
        RefreshNow,
    };

    // Welche Portal-Datenpunkte ein Tipp aktiv neu auslesen soll (Auftrag
    // "Aktiv-Auslesen beim Antippen", Korrektur B3: NUR die Werte, die
    // der angetippte Bereich wirklich anzeigt - nicht automatisch alle
    // Batteriewerte). 4 Plaetze reichen fuer den groessten Fall
    // (Batterieseite: Spannung/Strom/Ladestand/Temperatur).
    constexpr uint8_t ACTIVE_READ_MAX_DATAPOINTS = 4;
    struct RefreshRequest {
        uint8_t dataPointCount = 0;
        int32_t dataPointIds[ACTIVE_READ_MAX_DATAPOINTS] = {0, 0, 0, 0};
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

    // Einen Befehl OHNE Zusatzdaten in die Queue legen (nicht-blockierend,
    // wird verworfen falls die Queue voll ist - das ist bei max. 4
    // Plaetzen und seltenen Befehlen praktisch nie der Fall). NICHT fuer
    // Command::RefreshNow verwenden, siehe sendRefreshNow() unten.
    void sendCommand(Command cmd);

    // Command::RefreshNow MIT der Liste der aktiv auszulesenden
    // Datenpunkte (Auftrag "Aktualisieren-Knopf soll echt etwas
    // bewirken", Korrektur B3) - jede Seite/Kachel ruft das mit genau den
    // Datenpunkt-IDs auf, die SIE selbst anzeigt (siehe CLAUDE.md Tabelle
    // "Aktiv-Auslesen pro Bereich").
    void sendRefreshNow(const RefreshRequest& req);
}
