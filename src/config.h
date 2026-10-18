#pragma once
// Zentrale Konstanten fuer Solar Guardian CYD. Alles, was sich spaeter
// leicht aendern lassen soll (Farben, Groessen, Zeiten), steht hier statt
// irgendwo im Code verstreut.
#include <Arduino.h>

namespace Config {

    // ---- Bildschirm (Hochformat 240x320, native Panel-Ausrichtung, siehe
    // display.cpp::begin() -> tft.setRotation(0)) ----
    constexpr int16_t SCREEN_WIDTH  = 240;
    constexpr int16_t SCREEN_HEIGHT = 320;

    // Feste Fusszone fuer Seitenpunkte + "?"-Knopf, gemeinsam fuer ALLE
    // Wisch-Seiten (siehe main_screen.cpp DOTS_Y/HELP_BTN - muss mit deren
    // y-Werten synchron bleiben: HELP_BTN beginnt bei
    // SCREEN_HEIGHT-32-6=282, DOTS_Y bei SCREEN_HEIGHT-16=304, die Zone
    // deckt beides ab). Jede Seite muss ihren eigenen Inhalt mindestens
    // 8px darueber enden lassen: Grenze = SCREEN_HEIGHT -
    // PAGE_FOOTER_RESERVED_H - 8 (= 274, absolute Bildschirm-Y-Koordinate).
    // WICHTIG: IMMER von SCREEN_HEIGHT ausgehen, NICHT von
    // areaTop+areaHeight (= BODY_TOP+BODY_H, liegt bereits DOTS_H=16px
    // oberhalb SCREEN_HEIGHT) - sonst wird die Grenze faelschlich um
    // weitere 16px verschaerft (siehe years_page.cpp/battery_page.cpp als
    // Vorbild fuer die korrekte Verwendung).
    constexpr int16_t PAGE_FOOTER_RESERVED_H = 38;

    // ---- Touch (XPT2046, uebernommen aus eiswolfs-flightradar-CYD - dort
    // bewaehrte Pinbelegung fuer dasselbe Board, ESP32-2432S028) ----
    constexpr uint8_t TOUCH_CLK_PIN  = 25;
    constexpr uint8_t TOUCH_CS_PIN   = 33;
    constexpr uint8_t TOUCH_MOSI_PIN = 32;
    constexpr uint8_t TOUCH_MISO_PIN = 39;
    constexpr uint8_t TOUCH_IRQ_PIN  = 36;

    // ---- SD-Karte (SPI, ebenfalls aus dem Flightradar-Projekt uebernommen) ----
    constexpr uint8_t SD_SPI_CS_PIN   = 5;
    constexpr uint8_t SD_SPI_MOSI_PIN = 23;
    constexpr uint8_t SD_SPI_MISO_PIN = 19;
    constexpr uint8_t SD_SPI_CLK_PIN  = 18;
    constexpr uint32_t SD_CHECK_RETRY_MS = 2000; // Pruefintervall, solange keine Karte erkannt wird

    // ---- SD-Ordner/Dateien ----
    constexpr const char* SD_ROOT_DIR        = "/SolarGuardian";
    constexpr const char* SD_CONFIG_FILE     = "/SolarGuardian/config.json";
    constexpr const char* SD_CONFIG_TMP_FILE = "/SolarGuardian/config.json.tmp";
    constexpr uint8_t CONFIG_FORMAT_VERSION  = 4;
    constexpr uint8_t MAX_WIFI_SLOTS = 3;
    constexpr const char* DEFAULT_DEVICE_NO = "04510146093XRJTQ-00918";

    // ---- Ertragsbuch (siehe yield_ledger.h/.cpp und CLAUDE.md) ----
    constexpr const char* SD_YIELD_FILE     = "/SolarGuardian/yield.json";
    constexpr const char* SD_YIELD_TMP_FILE = "/SolarGuardian/yield.json.tmp";
    constexpr const char* SD_YIELD_DAYS_CSV = "/SolarGuardian/days.csv";
    constexpr uint32_t YIELD_SAVE_MIN_INTERVAL_MS = 60UL * 1000UL;
    constexpr float YIELD_DAILY_MIN_KWH = 0.0f;
    constexpr float YIELD_DAILY_MAX_KWH = 200.0f;
    constexpr uint8_t YIELD_IGNORE_WINDOW_END_HOUR = 4; // 00:00-04:00 lokal: Tageswerte ignorieren
    constexpr uint8_t YIELD_ROLLING_WEEK_DAYS = 6; // "letzteTage" - heute + diese Anzahl Vortage = 7-Tage-Fenster

    // ---- History-Backup-Ordner (Teil 2: tagesertrag.json, ueberschrieben
    // bei jeder erfolgreichen Abfrage, siehe yield_ledger.h) ----
    constexpr const char* SD_HISTORY_DIR         = "/SolarGuardian/History";
    constexpr const char* SD_HISTORY_BACKUP_FILE = "/SolarGuardian/History/tagesertrag.json";
    constexpr const char* SD_HISTORY_BACKUP_TMP  = "/SolarGuardian/History/tagesertrag.json.tmp";

    // ---- Jahresuebersicht (Seite "Jahre", siehe seed_years.h) ----
    constexpr const char* SD_YEARS_FILE     = "/SolarGuardian/years.json";
    constexpr const char* SD_YEARS_TMP_FILE = "/SolarGuardian/years.json.tmp";
    constexpr uint8_t YEARS_CHART_MAX_BARS  = 6; // laufendes Jahr + bis zu 5 fruehere

    // ---- Wisch-Navigation zwischen den Hauptseiten ----
    constexpr int16_t SWIPE_THRESHOLD_PX = 50;

    // ---- Touch-Ziele ----
    constexpr int16_t MIN_TOUCH_TARGET_PX = 40;

    // ---- WLAN ----
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
    constexpr uint32_t WIFI_SCAN_TIMEOUT_MS    = 10000;
    constexpr uint8_t MAX_WIFI_NETWORKS_SHOWN  = 12;

    // ---- Zeit (NTP, Zeitzone Europe/Berlin inkl. Sommerzeit) ----
    constexpr const char* NTP_SERVER = "pool.ntp.org";
    constexpr const char* TIMEZONE_POSIX = "CET-1CEST,M3.5.0,M10.5.0/3";

    // ---- Systemfarben-Palette (Menuepunkt "Systemfarbe") ----
    constexpr uint8_t COLOR_COUNT = 8;
    constexpr uint16_t COLOR_PALETTE[COLOR_COUNT] = {
        0xFFFF, // Weiss
        0xFFE0, // Gelb
        0x07E0, // Gruen
        0x07FF, // Cyan
        0xFD20, // Orange
        0xF800, // Rot
        0x001F, // Blau
        0xF81F, // Magenta
    };
    constexpr const char* COLOR_NAMES[COLOR_COUNT] = {
        "Weiss", "Gelb", "Gruen", "Cyan", "Orange", "Rot", "Blau", "Magenta"
    };
    constexpr uint8_t DEFAULT_COLOR_INDEX = 0; // Weiss

    // ---- Solar Guardian Portal (EPEVER-Cloud) ----
    // Appid ist eine feste App-Kennung, kein Geheimnis (steckt auch in der
    // offiziellen App, nicht pro Nutzer/Konto).
    constexpr const char* EPEVER_APPID = "cG8yNDYwODl6a2xqYWdpdXNkZ3E=";
    constexpr const char* EPEVER_LOGIN_URL  = "https://hncloudapi.epsolarpv.com/usrCloud/user/login";
    constexpr const char* EPEVER_ENERGY_URL = "https://hncloudapi.epsolarpv.com/usrCloud/vn/userView/getGeneratedEnergy";
    constexpr uint32_t EPEVER_HTTP_TIMEOUT_MS = 10000;
    constexpr uint32_t EPEVER_FETCH_INTERVAL_MS = 5UL * 60UL * 1000UL;   // normaler Takt
    constexpr uint32_t EPEVER_RETRY_INTERVAL_MS = 60UL * 1000UL;        // Takt bei den ersten Fehlschlaegen
    constexpr uint8_t  EPEVER_MAX_SHORT_RETRIES = 3;                    // danach zurueck auf den normalen Takt
    constexpr uint32_t EPEVER_MANUAL_REFRESH_MIN_INTERVAL_MS = 30UL * 1000UL;
    constexpr uint32_t EPEVER_STALE_AFTER_MS = 30UL * 60UL * 1000UL;    // Werte gelten danach als veraltet
    constexpr const char* EPEVER_BATTERY_URL = "https://hncloudapi.epsolarpv.com:7002/history/getSampleDataPoint";

}
