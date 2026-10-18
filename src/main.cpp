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
#include "fetch_task.h"
#include "account_bridge.h"
#include "yield_ledger.h"
#include "main_screen.h"
#include "i18n.h"

namespace {

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

        CredentialsScreen::run(); // Zugangsdaten, legt /SolarGuardian an, speichert, holt NTP-Zeit
    }

}

void setup() {
    Serial.begin(115200);

    Display::begin();
    TouchInput::begin();

    // SD-Karte pruefen, alle 2s erneut versuchen, bis eine Karte erkannt
    // wird - automatisches Weitermachen, sobald eine Karte steckt.
    // Bleibt fest zweisprachig (DE+EN) - es gibt an dieser Stelle noch
    // keine geladene Spracheinstellung.
    while (!SdStorage::begin()) {
        showMessage("SD Karte wird benötigt", "SD card required");
        delay(Config::SD_CHECK_RETRY_MS);
    }
    SdStorage::ensureRootDir();

    bool loadedOk = AppConfigStore::load();

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

    // Ertragsbuch (Core 1, benutzt die SD-Karte) - laedt yield.json.
    YieldLedger::init();

    // Core 0: eigener Fetch-Task fuer WLAN/Wiederverbindung/NTP und die
    // Solar-Guardian-Portal-Abfragen - siehe CLAUDE.md
    // "Dual-Core-Architektur". Nicht-blockierend: der Hauptbildschirm
    // erscheint sofort, egal wie lange die Verbindung braucht.
    FetchTask::start();

    MainScreen::enter();
}

void loop() {
    // Core 1: nur Anzeige/Touch/SD - niemals Netzwerk-Wartezeiten hier.
    YieldLedger::tick();
    MainScreen::tick();
    delay(10);
}
