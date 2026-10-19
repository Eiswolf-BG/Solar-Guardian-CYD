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

    // Diagnose-Log fuer die Ursachenforschung beim "Flackern"-Auftrag
    // (siehe redraw_diag.h) - loggt nur GRUENDE, nie Werte. Nach der
    // Messung wieder auf false setzen (Auftrag) - NICHT dauerhaft an
    // lassen wie den Heap-Log, da er bei jeder einzelnen Neuzeichnung
    // (potenziell mehrmals pro Sekunde) eine Zeile schreibt.
    constexpr bool DEBUG_REDRAW_DIAGNOSTICS = false;

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
    constexpr uint8_t CONFIG_FORMAT_VERSION  = 5;
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

    // ---- History-Backup-Ordner (tagesertrag.json, ueberschrieben bei
    // jeder erfolgreichen Abfrage, siehe yield_ledger.h). Bewusst AUSSERHALB
    // von /SolarGuardian (eigener Wurzelordner), damit "Zuruecksetzen"
    // (das nur /SolarGuardian rekursiv loescht, siehe reset_screen.cpp)
    // ihn nie mit entfernt - ein Reset+Neueinrichtung stellt den
    // Tagesertrag damit automatisch wieder her. Fruehere Version lag
    // unter /SolarGuardian/History, SD_HISTORY_DIR_OLD dient nur noch der
    // einmaligen Migration beim Start, siehe sd_storage.cpp.
    constexpr const char* SD_HISTORY_DIR         = "/SolarHistory";
    constexpr const char* SD_HISTORY_BACKUP_FILE = "/SolarHistory/tagesertrag.json";
    constexpr const char* SD_HISTORY_BACKUP_TMP  = "/SolarHistory/tagesertrag.json.tmp";
    constexpr const char* SD_HISTORY_DIR_OLD         = "/SolarGuardian/History";
    constexpr const char* SD_HISTORY_BACKUP_FILE_OLD = "/SolarGuardian/History/tagesertrag.json";

    // Markerdatei fuer den einmaligen historischen Ertragsimport (siehe
    // steffen_import.h/yield_ledger.cpp::importHistoricalData()) - lebt
    // bewusst in /SolarHistory (nicht /SolarGuardian), damit ein
    // Werksreset den Import NICHT erneut auslöst (nach einem Reset soll
    // sich das Ertragsbuch stattdessen ganz normal aus dem Backup oben
    // wiederherstellen, nicht die historischen Importwerte nochmal
    // aufaddieren).
    constexpr const char* SD_IMPORT_MARKER = "/SolarHistory/steffen_import_v1.done";

    // ---- Jahresuebersicht (Seite "Jahre", siehe seed_years.h) ----
    constexpr const char* SD_YEARS_FILE     = "/SolarGuardian/years.json";
    constexpr const char* SD_YEARS_TMP_FILE = "/SolarGuardian/years.json.tmp";
    constexpr uint8_t YEARS_CHART_MAX_BARS  = 6; // laufendes Jahr + bis zu 5 fruehere

    // ---- Wisch-Navigation zwischen den Hauptseiten (Auftrag "Wischgeste
    // empfindlicher" - vorher musste man "gewaltig druecken") ----
    // Mindestweg in Pixeln, ab dem eine Bewegung ueberhaupt als Wisch
    // zaehlt (vorher 50, deutlich herabgesetzt).
    constexpr int16_t SWIPE_MIN_DISTANCE_PX = 22;
    // Waagerecht muss mindestens so ein Vielfaches der senkrechten
    // Bewegung sein, damit ein leicht schraeger Wisch noch als
    // links/rechts gilt (statt komplett waagerecht zu verlangen).
    constexpr int16_t SWIPE_HORIZONTAL_RATIO = 2;
    // Unter dieser Wegstrecke (in jede Richtung) gilt ein Loslassen noch
    // sicher als Tippen - alles DAZWISCHEN (mehr als das hier, aber nicht
    // genug fuer einen Wisch) wird verworfen, statt faelschlich als Tap
    // einen Knopf/eine Kachel auszuloesen.
    constexpr int16_t TAP_MAX_DRIFT_PX = 8;
    // Kurze Druck-Aussetzer waehrend einer laufenden Beruehrung tolerieren
    // (XPT2046 bei leichtem Druck/beim Gleiten manchmal kurz unter der
    // Z-Schwelle, siehe lib/XPT2046_Touchscreen/README_PATCH.md) - erst
    // nach dieser Zeit ohne erneuten Touch gilt die Beruehrung wirklich
    // als beendet (TouchInput::wasTapped()).
    constexpr uint32_t TOUCH_DROPOUT_TOLERANCE_MS = 80;

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
    constexpr uint32_t EPEVER_HTTP_TIMEOUT_MS = 10000; // Verbindungsaufbau/TLS-Handshake-Teillimit, siehe http_deadline_guard.h
    constexpr uint32_t EPEVER_FETCH_INTERVAL_MS = 5UL * 60UL * 1000UL;   // normaler Takt
    constexpr uint32_t EPEVER_RETRY_INTERVAL_MS = 60UL * 1000UL;        // Takt bei den ersten Fehlschlaegen
    constexpr uint8_t  EPEVER_MAX_SHORT_RETRIES = 3;                    // danach zurueck auf den normalen Takt
    constexpr uint32_t EPEVER_MANUAL_REFRESH_MIN_INTERVAL_MS = 30UL * 1000UL;
    constexpr uint32_t EPEVER_STALE_AFTER_MS = 30UL * 60UL * 1000UL;    // Werte gelten danach als veraltet
    constexpr const char* EPEVER_BATTERY_URL = "https://hncloudapi.epsolarpv.com:7002/history/getSampleDataPoint";

    // ---- Wetter (Open-Meteo.com, kostenlos, ohne Zugangsschluessel) ----
    constexpr const char* WEATHER_API_HOST = "https://api.open-meteo.com/v1/forecast";
    constexpr const char* GEOCODING_API_HOST = "https://geocoding-api.open-meteo.com/v1/search";
    constexpr uint32_t WEATHER_HTTP_TIMEOUT_MS = 10000;
    constexpr uint32_t WEATHER_FETCH_INTERVAL_MS = 30UL * 60UL * 1000UL; // alle 30 Minuten
    constexpr uint32_t WEATHER_RETRY_INTERVAL_MS = 2UL * 60UL * 1000UL;  // bei Fehler alle 2 Minuten erneut
    constexpr uint8_t WEATHER_FORECAST_DAYS = 7;

    // ---- Automatische IP-Standort-Ermittlung (siehe ip_geolocation_client.h) ----
    constexpr uint32_t IP_LOCATION_DAILY_INTERVAL_MS = 24UL * 60UL * 60UL * 1000UL;

    // ---- Fetch-Task-Absicherung (siehe CLAUDE.md gleichen Namens) ----
    // Gesamt-Zeitlimit JEDER einzelnen HTTPS-Abfrage im Fetch-Task (Login,
    // Ertrag, Batterie, Leistungskurve, Wetter, Geocoding), gemessen vom
    // Beginn des Verbindungsaufbaus bis zum letzten gelesenen Byte - NICHT
    // nur pro Socket-Lesevorgang (siehe http_deadline_guard.h). Der
    // beobachtete mehrminuetige Haenger bei der Leistungskurve blieb
    // trotz der bereits gesetzten Pro-Operation-Timeouts (siehe
    // EPEVER_HTTP_TIMEOUT_MS/WEATHER_HTTP_TIMEOUT_MS oben, weiterhin als
    // Verbindungsaufbau-Teillimit aktiv) unentdeckt, weil ein Server, der
    // immer wieder (aber zu selten) ein paar Bytes sendet, jeden
    // Pro-Read-Timeout einzeln unterschreiten kann.
    constexpr uint32_t HTTP_TOTAL_TIMEOUT_MS = 20000;
    // Kein Herzschlag vom Fetch-Task seit... (siehe fetch_heartbeat.h/
    // fetch_watchdog.h): zuerst nur loggen + "Stand"-Zeit einfrieren,
    // danach Ertrag/Backup sichern und kontrolliert neu starten.
    constexpr uint32_t FETCH_WATCHDOG_WARN_AFTER_MS = 10UL * 60UL * 1000UL;
    constexpr uint32_t FETCH_WATCHDOG_REBOOT_AFTER_MS = 15UL * 60UL * 1000UL;
    // Hoechstens so viele vom Watchdog ausgeloeste Neustarts HINTEREINANDER
    // (ohne dazwischen mindestens einen normalen Herzschlag) - verhindert
    // eine Neustart-Schleife, falls die Ursache des Haengers einen
    // Neustart ueberlebt. Bewusst NICHT calendarisch (z.B. "1x pro
    // Stunde" per time(nullptr)) und NICHT per RTC_DATA_ATTR umgesetzt -
    // beide Ansaetze wurden in einem Live-Test ausprobiert und griffen
    // dort nie (Systemzeit nach einem Haenger-Neustart oft noch nicht
    // per NTP synchronisiert; RTC-Speicher ueberlebte den esp_restart()
    // auf diesem Board in der Praxis ebenfalls nicht zuverlaessig). Der
    // Zaehler steht deshalb stattdessen in einer winzigen Datei auf der
    // SD-Karte (siehe fetch_watchdog.cpp), garantiert zuverlaessig, und
    // wird nach so vielen ERFOLGREICHEN Abfragezyklen (SolarData::
    // successfulFetchCount) wieder auf 0 gesetzt, nicht schon beim
    // blossen Fortbestehen eines einzelnen Herzschlags - ein einzelner
    // Herzschlag beweist noch keinen funktionierenden Zyklus (Auftrag,
    // Bugfix: vorher wurde ausserhalb eines akuten Haengers nie
    // zurueckgesetzt).
    constexpr uint32_t FETCH_WATCHDOG_MAX_CONSECUTIVE_REBOOTS = 3;
    constexpr uint32_t FETCH_WATCHDOG_RESET_AFTER_SUCCESSFUL_CYCLES = 2;
    // Nach Erreichen der max. Neustarts gibt das Geraet NICHT fuer immer
    // auf (Auftrag) - stattdessen hoechstens ein weiterer Neustart-
    // Versuch pro Stunde, solange der Herzschlag weiter ausbleibt. Nutzt
    // bewusst millis() seit dem Erreichen dieses Zustands (nicht
    // time(nullptr)/RTC, siehe Kommentar oben) - in diesem Zustand
    // startet das Geraet ja gerade NICHT mehr neu, millis() laeuft also
    // ungestoert durch.
    constexpr uint32_t FETCH_WATCHDOG_HOURLY_RETRY_MS = 60UL * 60UL * 1000UL;

}
