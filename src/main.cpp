#include <Arduino.h>
#include <SD.h>

#include "config.h"
#include "display.h"
#include "touch_input.h"
#include "sd_storage.h"
#include "app_config.h"
#include "calibration_screen.h"
#include "wifi_select_screen.h"
#include "credentials_screen.h"
#include "serial_number_screen.h"
#include "fetch_task.h"
#include "account_bridge.h"
#include "location_bridge.h"
#include "fetch_watchdog.h"
#include "yield_ledger.h"
#include "main_screen.h"
#include "i18n.h"
#include "web_server.h"
#include "boot_logo.h"
#include "easter_egg.h"
#include <esp_system.h>

namespace {

    // Bei JEDEM Neustart aufrufen (Auftrag "Fetch-Task-Absicherung") -
    // damit sich spaeter im Serial-Log nachvollziehen laesst, ob ein
    // Neustart vom Watchdog (fetch_watchdog.cpp, meldet sich dort selbst
    // zusaetzlich kurz VOR dem esp_restart()) oder von etwas anderem
    // (Stromausfall, manuelles Flashen, Absturz) ausgeloest wurde.
    void logResetReason() {
        esp_reset_reason_t r = esp_reset_reason();
        const char* name = "unbekannt";
        switch (r) {
            case ESP_RST_POWERON:   name = "Einschalten"; break;
            case ESP_RST_EXT:       name = "externer Reset"; break;
            case ESP_RST_SW:        name = "Software (esp_restart)"; break;
            case ESP_RST_PANIC:     name = "Absturz/Panic"; break;
            case ESP_RST_INT_WDT:   name = "interner Watchdog"; break;
            case ESP_RST_TASK_WDT:  name = "Task-Watchdog"; break;
            case ESP_RST_WDT:       name = "sonstiger Watchdog"; break;
            case ESP_RST_DEEPSLEEP: name = "Deep-Sleep-Aufwachen"; break;
            case ESP_RST_BROWNOUT:  name = "Unterspannung (Brownout)"; break;
            case ESP_RST_SDIO:      name = "SDIO"; break;
            default: break;
        }
        Serial.printf("[core] Reset-Grund: %s (%d)\n", name, (int)r);
    }

    void showMessage(const char* line1, const char* line2 = nullptr) {
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(line1, Config::SCREEN_WIDTH / 2, line2 ? Config::SCREEN_HEIGHT / 2 - 12 : Config::SCREEN_HEIGHT / 2);
        if (line2) tft.drawString(line2, Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT / 2 + 12);
        tft.setTextDatum(TL_DATUM);
    }

    void runFirstTimeSetup() {
        appConfig = AppConfig(); // Defaults - eine evtl. vorhandene defekte Konfiguration nicht weiterverwenden.

        CalibrationScreen::run(); // setzt appConfig.touch* und TouchInput::setCalibration() direkt

        String ssid, password;
        WifiSelectScreen::run(false, ssid, password); // Pflicht in der Ersteinrichtung, kein Abbrechen
        appConfig.wifiSlots[0].ssid = ssid;
        appConfig.wifiSlots[0].password = password;
        appConfig.lastActiveWifiSlot = 0;

        appConfig.portalEmail = "";
        appConfig.portalPassword = "";
        // Bei einer frischen Einrichtung bewusst LEER, NICHT der
        // eingebaute Standardwert (Config::DEFAULT_DEVICE_NO gehoert zu
        // einem bestimmten Geraet) - siehe serial_number_screen.h.
        appConfig.deviceNo = "";
        // Standort bleibt in der Ersteinrichtung aussen vor (Auftrag
        // "Standort automatisch, notfalls einmalig per Eingabe") - die
        // automatische IP-Ermittlung laeuft im Fetch-Task von selbst,
        // sobald WLAN steht (siehe fetch_task.cpp), eine manuelle Eingabe
        // ist jederzeit ueber die Wetterkachel moeglich.
        appConfig.locationName = "";
        appConfig.locationLat = 0.0f;
        appConfig.locationLon = 0.0f;
        appConfig.locationSet = false;
        appConfig.locationManual = false;

        // Konto -> Seriennummer -> "Los geht's" (schliesst ab, speichert,
        // holt NTP-Zeit). "Zurueck" auf dem Seriennummer-Screen fuehrt
        // zurueck zu den Zugangsdaten, ohne bereits eingegebene Werte zu
        // verlieren (CredentialsScreen::run() loescht die Felder nicht
        // mehr selbst, siehe credentials_screen.cpp).
        bool setupFinished = false;
        while (!setupFinished) {
            CredentialsScreen::run(false); // "Weiter" statt "Los geht's" - speichert hier noch nichts
            bool back = false;
            setupFinished = SerialNumberScreen::runFirstTimeSetup(back);
            if (back) continue;
        }
        CredentialsScreen::finalizeSetup(); // legt /SolarGuardian an, speichert, holt NTP-Zeit
    }

}

void setup() {
    Serial.begin(115200);
    logResetReason();

    Display::begin();
    TouchInput::begin();

    // Frueher, einmaliger SD-Mount-Versuch (ohne Wiederhol-Schleife) -
    // noetig, damit die Helligkeit und ein evtl. vorhandenes Zertifikat
    // (siehe easter_egg.h) schon VOR dem Logo feststehen (Auftrag). Ist
    // die Karte hier noch nicht da, laeuft weiter unten die normale
    // Wiederhol-Schleife, das Logo erscheint dann mit Standardhelligkeit
    // und ohne Zusatzschriftzug.
    bool sdReady = SdStorage::begin();
    bool loadedOk = false;
    if (sdReady) {
        SdStorage::ensureRootDir();
        loadedOk = AppConfigStore::load();
        Display::applyBrightness();
    }

    BootLogo::begin(sdReady && EasterEgg::certificateValid());

    if (!sdReady) {
        // Bleibt fest zweisprachig (DE+EN) - es gibt an dieser Stelle noch
        // keine geladene Spracheinstellung. Ueberschreibt das Logo
        // bewusst (Auftrag "Meldung darf nicht verdeckt werden").
        while (!SdStorage::begin()) {
            showMessage("SD Karte wird benötigt", "SD card required");
            delay(Config::SD_CHECK_RETRY_MS);
        }
        SdStorage::ensureRootDir();
        loadedOk = AppConfigStore::load();
        Display::applyBrightness();
    }

    if (!loadedOk && SD.exists(Config::SD_CONFIG_FILE)) {
        // Datei war da, liess sich aber nicht sinnvoll einlesen.
        showMessage(I18n::t(I18n::Str::CONFIG_CORRUPTED), I18n::t(I18n::Str::CONFIG_CORRUPTED_RESTART));
        delay(2000);
    }

    if (!loadedOk) {
        runFirstTimeSetup();
    } else {
        TouchInput::setCalibration(appConfig.touchSwapXY,
                                    appConfig.touchScreenXRawMin, appConfig.touchScreenXRawMax,
                                    appConfig.touchScreenYRawMin, appConfig.touchScreenYRawMax);
    }

    Serial.printf("[core] Anzeige laeuft auf Core %d\n", xPortGetCoreID());
    // device_no ist keine Geheimzahl, wird aber trotzdem nicht im
    // Klartext geloggt (Alex' Wunsch) - nur ob ueberhaupt ein Wert gesetzt
    // ist.
    Serial.printf("[core] device_no gesetzt: %s\n", appConfig.deviceNo.length() > 0 ? "ja" : "nein");

    // Zugangsdaten sicher an den Fetch-Task uebergeben (Mutex-Kopie,
    // siehe account_bridge.h) - MUSS vor FetchTask::start() passieren,
    // damit der Task beim eigenen Start sofort etwas vorfindet. Core 0
    // greift dafuer nie direkt auf appConfig zu.
    AccountBridge::init();
    AccountBridge::set(appConfig.portalEmail, appConfig.portalPassword, appConfig.deviceNo);
    LocationBridge::init();
    LocationBridge::set(appConfig.locationLat, appConfig.locationLon, appConfig.locationSet, appConfig.locationManual);

    // Ertragsbuch (Core 1, benutzt die SD-Karte) - laedt yield.json.
    YieldLedger::init();

    // Core 0: eigener Fetch-Task fuer WLAN/Wiederverbindung/NTP und die
    // Solar-Guardian-Portal-Abfragen - siehe CLAUDE.md
    // "Dual-Core-Architektur". Nicht-blockierend: der Hauptbildschirm
    // erscheint sofort, egal wie lange die Verbindung braucht.
    FetchTask::start();

    // Mindestanzeigezeit des Logos sicherstellen, ab dem Moment seines
    // Erscheinens (nicht zusaetzlich zur restlichen Startzeit, siehe
    // config.h) - der obige Ablauf (SD/Konfiguration/WLAN-Task) lief
    // bereits waehrenddessen. War eine Vollbild-Ersteinrichtung noetig,
    // ist die Zeit hier laengst abgelaufen, die Schleife endet sofort.
    while (!BootLogo::tick()) {
        delay(16);
    }

    MainScreen::enter();
}

void loop() {
    // Core 1: nur Anzeige/Touch/SD - niemals Netzwerk-Wartezeiten hier.
    YieldLedger::tick();
    MainScreen::tick();
    FetchWatchdog::tick();
    WebServer_::tick();
    delay(10);
}
