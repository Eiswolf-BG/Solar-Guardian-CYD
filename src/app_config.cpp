#include "app_config.h"
#include "config.h"
#include <SD.h>
#include <ArduinoJson.h>

AppConfig appConfig;

namespace AppConfigStore {

bool load() {
    if (!SD.exists(Config::SD_CONFIG_FILE)) return false;

    File f = SD.open(Config::SD_CONFIG_FILE, FILE_READ);
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false; // defekte/unlesbare Datei - Aufrufer zeigt Fehler + startet Ersteinrichtung neu

    appConfig.touchSwapXY = doc["touch_swap_xy"] | false;
    appConfig.touchScreenXRawMin = doc["touch_x_raw_min"] | 0;
    appConfig.touchScreenXRawMax = doc["touch_x_raw_max"] | 4095;
    appConfig.touchScreenYRawMin = doc["touch_y_raw_min"] | 0;
    appConfig.touchScreenYRawMax = doc["touch_y_raw_max"] | 4095;
    appConfig.touchCalibrated = doc["touch_calibrated"] | false;

    uint8_t loadedVersion = doc["format_version"] | 1;

    for (uint8_t i = 0; i < Config::MAX_WIFI_SLOTS; i++) {
        appConfig.wifiSlots[i].ssid = "";
        appConfig.wifiSlots[i].password = "";
    }
    appConfig.lastActiveWifiSlot = 255;

    if (loadedVersion < 2) {
        // Migration von Version 1 (einzelnes WLAN) auf Version 2 (3
        // Plaetze) - das alte Netz wird Platz 1, ohne dass die
        // Ersteinrichtung erneut laufen muss.
        String oldSsid = String((const char*)(doc["wifi_ssid"] | ""));
        String oldPassword = String((const char*)(doc["wifi_password"] | ""));
        if (oldSsid.length() > 0) {
            appConfig.wifiSlots[0].ssid = oldSsid;
            appConfig.wifiSlots[0].password = oldPassword;
            appConfig.lastActiveWifiSlot = 0;
        }
    } else {
        JsonArray slots = doc["wifi_slots"];
        uint8_t i = 0;
        for (JsonObject s : slots) {
            if (i >= Config::MAX_WIFI_SLOTS) break;
            appConfig.wifiSlots[i].ssid = String((const char*)(s["ssid"] | ""));
            appConfig.wifiSlots[i].password = String((const char*)(s["password"] | ""));
            i++;
        }
        int lastActive = doc["last_active_wifi_slot"] | 255;
        appConfig.lastActiveWifiSlot = (lastActive >= 0 && lastActive < Config::MAX_WIFI_SLOTS) ? (uint8_t)lastActive : 255;
    }

    appConfig.portalEmail = String((const char*)(doc["portal_email"] | ""));
    appConfig.portalPassword = String((const char*)(doc["portal_password"] | ""));

    appConfig.systemColorIndex = doc["system_color_index"] | 0;
    if (appConfig.systemColorIndex >= Config::COLOR_COUNT) appConfig.systemColorIndex = 0;

    // Optionales Feld (wie die Standort-Felder oben) - bestehende Configs
    // ohne diesen Eintrag laufen einfach mit voller Helligkeit weiter.
    appConfig.brightnessPercent = doc["brightness_percent"] | 100;
    if (appConfig.brightnessPercent < Config::BRIGHTNESS_MIN_PERCENT) appConfig.brightnessPercent = Config::BRIGHTNESS_MIN_PERCENT;
    if (appConfig.brightnessPercent > 100) appConfig.brightnessPercent = 100;

    if (loadedVersion < 3) {
        // Migration auf Version 3: neues Feld "language", Standard Deutsch.
        appConfig.language = 0;
    } else {
        String lang = String((const char*)(doc["language"] | "de"));
        appConfig.language = (lang == "en") ? 1 : 0;
    }

    if (loadedVersion < 4) {
        // Migration auf Version 4: Feld "device_no" existierte noch nicht -
        // kein eingebauter Standardwert mehr (Auftrag "Seriennummer aus
        // dem Projekt entfernen"), bleibt leer bis zur Eingabe am Geraet.
        appConfig.deviceNo = "";
    } else {
        appConfig.deviceNo = String((const char*)(doc["device_no"] | ""));
    }

    // Optionale Standort-Felder (Auftrag "Standort automatisch, notfalls
    // einmalig per Eingabe") - bewusst OHNE formatVersion-Abfrage, einfach
    // mit Default lesen: bestehende Configs ohne diese Felder laufen so
    // unveraendert weiter (Wetterkachel zeigt "Standort waehlen"), bis die
    // automatische Ermittlung oder eine manuelle Eingabe sie befuellt.
    appConfig.locationName = String((const char*)(doc["location_name"] | ""));
    appConfig.locationLat = doc["location_lat"] | 0.0f;
    appConfig.locationLon = doc["location_lon"] | 0.0f;
    appConfig.locationSet = doc["location_set"] | false;
    appConfig.locationManual = doc["location_manual"] | false;

    appConfig.formatVersion = Config::CONFIG_FORMAT_VERSION; // nach dem Laden immer auf dem aktuellen Stand
    appConfig.setupComplete = doc["setup_complete"] | false;

    // Grundvoraussetzung fuer einen gueltigen, nutzbaren Zustand - ohne das
    // waere load() zwar technisch erfolgreich, aber main.cpp wuerde mit
    // leeren Zugangsdaten weitermachen. Behandelt wie eine defekte Datei.
    if (!appConfig.setupComplete) return false;

    // Eine Migration auf eine neuere Version sofort sichern, nicht erst
    // beim naechsten ohnehin anfallenden Speichern (z.B. Farbwechsel)
    // abwarten.
    if (loadedVersion < Config::CONFIG_FORMAT_VERSION) save();

    return true;
}

bool save() {
    JsonDocument doc;

    doc["touch_swap_xy"] = appConfig.touchSwapXY;
    doc["touch_x_raw_min"] = appConfig.touchScreenXRawMin;
    doc["touch_x_raw_max"] = appConfig.touchScreenXRawMax;
    doc["touch_y_raw_min"] = appConfig.touchScreenYRawMin;
    doc["touch_y_raw_max"] = appConfig.touchScreenYRawMax;
    doc["touch_calibrated"] = appConfig.touchCalibrated;

    JsonArray slots = doc["wifi_slots"].to<JsonArray>();
    for (uint8_t i = 0; i < Config::MAX_WIFI_SLOTS; i++) {
        JsonObject s = slots.add<JsonObject>();
        s["ssid"] = appConfig.wifiSlots[i].ssid;
        s["password"] = appConfig.wifiSlots[i].password;
    }
    doc["last_active_wifi_slot"] = appConfig.lastActiveWifiSlot;

    doc["portal_email"] = appConfig.portalEmail;
    doc["portal_password"] = appConfig.portalPassword;

    doc["system_color_index"] = appConfig.systemColorIndex;
    doc["brightness_percent"] = appConfig.brightnessPercent;
    doc["language"] = (appConfig.language == 1) ? "en" : "de";
    doc["device_no"] = appConfig.deviceNo;
    doc["location_name"] = appConfig.locationName;
    doc["location_lat"] = appConfig.locationLat;
    doc["location_lon"] = appConfig.locationLon;
    doc["location_set"] = appConfig.locationSet;
    doc["location_manual"] = appConfig.locationManual;
    doc["format_version"] = Config::CONFIG_FORMAT_VERSION;
    doc["setup_complete"] = appConfig.setupComplete;

    // Erst in die temporaere Datei schreiben, dann umbenennen - so bleibt
    // bei einem Stromausfall mitten im Schreiben immer entweder die alte
    // ODER die neue, aber nie eine halb geschriebene Datei uebrig.
    if (SD.exists(Config::SD_CONFIG_TMP_FILE)) SD.remove(Config::SD_CONFIG_TMP_FILE);
    File f = SD.open(Config::SD_CONFIG_TMP_FILE, FILE_WRITE);
    if (!f) return false;

    bool ok = serializeJson(doc, f) > 0;
    f.close();
    if (!ok) {
        SD.remove(Config::SD_CONFIG_TMP_FILE);
        return false;
    }

    if (SD.exists(Config::SD_CONFIG_FILE)) SD.remove(Config::SD_CONFIG_FILE);
    return SD.rename(Config::SD_CONFIG_TMP_FILE, Config::SD_CONFIG_FILE);
}

}
