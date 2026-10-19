#include "fetch_watchdog.h"
#include "fetch_heartbeat.h"
#include "yield_ledger.h"
#include "solar_data.h"
#include "config.h"
#include <esp_system.h>
#include <SD.h>
#include <ArduinoJson.h>

namespace FetchWatchdog {

namespace {
    bool warnedThisStall = false;
    bool loadedCounter = false;
    uint32_t consecutiveReboots = 0;

    // Baseline von SolarData::successfulFetchCount bei diesem Boot, plus
    // wie viele erfolgreiche Zyklen seitdem dazugekommen sind - erst nach
    // Config::FETCH_WATCHDOG_RESET_AFTER_SUCCESSFUL_CYCLES wird der
    // persistierte Neustart-Zaehler geloescht (Auftrag "Neustart-
    // Zaehler" - ein blosser Herzschlag allein bewies vorher faelschlich
    // schon "alles gut").
    bool successBaselineSet = false;
    uint32_t successBaseline = 0;

    // Zustand "aufgegeben" (max. Neustarts erreicht) - das Geraet startet
    // dann nicht mehr bei JEDEM weiteren Stall neu, sondern hoechstens
    // einmal pro Stunde (Auftrag). millis()-basiert, siehe config.h-
    // Kommentar: in diesem Zustand finden ja keine Neustarts mehr im
    // bisherigen Takt statt, millis() laeuft also ungestoert durch.
    bool givenUp = false;
    uint32_t giveUpSinceMs = 0;

    constexpr const char* WATCHDOG_FILE = "/SolarGuardian/watchdog.json";

    // Haelt fest, wie viele vom Watchdog ausgeloeste Neustarts HINTEREINANDER
    // (ohne dazwischen einen normalen Herzschlag) schon passiert sind -
    // verhindert eine Neustart-Schleife, falls die Ursache des Haengers
    // einen Neustart ueberlebt. Bewusst eine winzige SD-Datei statt
    // RTC_DATA_ATTR (ueberlebt dokumentiert einen esp_restart(), tat das
    // in einem Live-Test auf diesem Board aber nicht zuverlaessig) oder
    // time(nullptr)-Vergleich (direkt nach einem Haenger-Neustart oft
    // noch nicht per NTP synchronisiert, ein frueherer Versuch damit
    // griff deshalb in einem Live-Test ebenfalls nie - siehe Git-
    // Historie). Die SD-Karte ist ohnehin schon exklusiv Core 1
    // vorbehalten (siehe CLAUDE.md "Dual-Core-Architektur"), genau hier
    // laeuft dieser Code.
    void loadCounterOnce() {
        if (loadedCounter) return;
        loadedCounter = true;
        consecutiveReboots = 0;
        if (!SD.exists(WATCHDOG_FILE)) return;
        File f = SD.open(WATCHDOG_FILE, FILE_READ);
        if (!f) return;
        JsonDocument doc;
        if (!deserializeJson(doc, f)) {
            consecutiveReboots = doc["consecutive_reboots"] | 0;
        }
        f.close();
    }

    void writeCounter(uint32_t value) {
        // FILE_WRITE haengt beim ESP32-SD-Treiber an (kein Truncate) -
        // ohne das vorherige SD.remove() wuerden sich hier bei jedem
        // Aufruf weitere JSON-Objekte hinten anhaengen und
        // deserializeJson() wuerde beim naechsten Laden immer nur das
        // ERSTE (also den allerersten je geschriebenen Wert) zurueckgeben
        // - genau das in einem Live-Test gefundene Verhalten (siehe
        // app_config.cpp::save() fuer das gleiche, dort schon korrekt
        // angewendete Muster).
        if (SD.exists(WATCHDOG_FILE)) SD.remove(WATCHDOG_FILE);
        JsonDocument doc;
        doc["consecutive_reboots"] = value;
        File f = SD.open(WATCHDOG_FILE, FILE_WRITE);
        if (!f) return;
        serializeJson(doc, f);
        f.close();
    }
}

void tick() {
    loadCounterOnce();

    // Baseline beim ersten tick() nach diesem Boot festlegen - danach
    // zaehlen wir, wie viele NEUE erfolgreiche Zyklen seit dem Boot
    // dazugekommen sind (Auftrag "Neustart-Zaehler"). Ohne Konto/ohne
    // jemals einen erfolgreichen Zyklus bleibt successBaseline fix und
    // der persistierte Zaehler wird (bewusst) nie automatisch
    // zurueckgesetzt - ein blosser Herzschlag alleine beweist noch
    // keinen funktionierenden Abfragezyklus.
    uint32_t successNow = SolarData::snapshot().successfulFetchCount;
    if (!successBaselineSet) {
        successBaselineSet = true;
        successBaseline = successNow;
    }
    if (consecutiveReboots != 0 &&
        (successNow - successBaseline) >= Config::FETCH_WATCHDOG_RESET_AFTER_SUCCESSFUL_CYCLES) {
        Serial.printf("[fetch-watchdog] %u erfolgreiche Abfragezyklen seit dem letzten Neustart - "
                      "Neustart-Zaehler zurueckgesetzt.\n",
                      (unsigned)Config::FETCH_WATCHDOG_RESET_AFTER_SUCCESSFUL_CYCLES);
        consecutiveReboots = 0;
        writeCounter(0);
        givenUp = false;
    }

    uint32_t sinceBeatMs = FetchHeartbeat::msSinceLastBeat();

    if (sinceBeatMs < Config::FETCH_WATCHDOG_WARN_AFTER_MS) {
        warnedThisStall = false;
        return;
    }

    if (!warnedThisStall) {
        warnedThisStall = true;
        Serial.printf("[fetch-watchdog] Kein Herzschlag vom Fetch-Task seit %lu s - "
                      "Anzeige-Zeitstempel bleiben auf dem letzten Stand.\n",
                      (unsigned long)(sinceBeatMs / 1000));
    }

    if (sinceBeatMs < Config::FETCH_WATCHDOG_REBOOT_AFTER_MS) return;

    if (consecutiveReboots >= Config::FETCH_WATCHDOG_MAX_CONSECUTIVE_REBOOTS) {
        // Schon mehrfach direkt nacheinander neu gestartet, ohne dass es
        // half - ein weiterer Neustart im bisherigen (schnellen) Takt
        // waere nur eine Schleife. Das Geraet gibt deshalb NICHT fuer
        // immer auf (Auftrag): hoechstens noch EIN Neustart-Versuch pro
        // Stunde, solange der Herzschlag weiter ausbleibt - bedienbar
        // bleibt es in der Zwischenzeit die ganze Zeit, da diese Funktion
        // hier nur kurz prueft und sofort zurueckkehrt.
        if (!givenUp) {
            givenUp = true;
            giveUpSinceMs = millis();
            Serial.printf("[fetch-watchdog] Maximale Neustarts (%u) erreicht - Geraet bleibt bedienbar, "
                          "hoechstens noch ein Neustart-Versuch pro Stunde, solange der Herzschlag "
                          "ausbleibt.\n",
                          (unsigned)Config::FETCH_WATCHDOG_MAX_CONSECUTIVE_REBOOTS);
        }
        if (millis() - giveUpSinceMs < Config::FETCH_WATCHDOG_HOURLY_RETRY_MS) return;

        giveUpSinceMs = millis();
        Serial.println("[fetch-watchdog] Stuendlicher Wiederholungsversuch nach dauerhaftem "
                        "Herzschlag-Ausfall.");
        YieldLedger::flush();
        delay(200);
        esp_restart();
    }

    consecutiveReboots++;
    Serial.printf("[fetch-watchdog] Kein Herzschlag seit %lu s - sichere Ertragsdaten und starte neu "
                  "(Versuch %u von %u).\n",
                  (unsigned long)(sinceBeatMs / 1000), (unsigned)consecutiveReboots,
                  (unsigned)Config::FETCH_WATCHDOG_MAX_CONSECUTIVE_REBOOTS);
    YieldLedger::flush();
    writeCounter(consecutiveReboots);
    delay(200); // der SD-Karte Zeit zum Fertigschreiben geben
    esp_restart();
}

}
