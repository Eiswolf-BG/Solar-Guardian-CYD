#pragma once
// Zentrale Konstanten fuer Solar Guardian CYD. Alles, was sich spaeter
// leicht aendern lassen soll (Farben, Groessen, Zeiten), steht hier statt
// irgendwo im Code verstreut.
#include <Arduino.h>

// Rein lokale Diagnose-Messung (siehe portal_day_probe.h) - NIE
// committen/veroeffentlichen (siehe CLAUDE.md). Diese eine Zeile
// auskommentieren schaltet die Messung komplett ab (Code wird dann nicht
// mitgebaut).
// Messung (5 Naechte) ausgewertet und abgeschaltet - Ergebnis: Portal-
// Tageswechsel UTC+2 fest, 23:00 Uhr lokal seit der Winterzeit-Umstellung
// am 25.10. (siehe Tagesfenster-Logik unten).
// #define PORTAL_DAY_PROBE 1

// Knappe serielle Logzeilen fuer den ersten echten Nachtmodus-Testlauf
// (Abschlussabfrage/Bestaetigung/Nachtmodus-Ein-Aus/Tageswechsel/
// abgelehntes Datum) - NUR fuer eine lokale Testversion, NIE im Release
// (siehe CLAUDE.md "Standard-Workflow"). Diese Zeile bleibt im normalen
// Arbeitsstand auskommentiert; nur fuer den einen Testflash lokal
// aktivieren, nie committen.
// #define NIGHT_MODE_DIAG_TEST 1

namespace Config {

    // Firmware-Version, einzige zentrale Stelle (Auftrag "WebUI
    // erweitern" - Versions-/Verbindungsanzeige). MUSS bei jedem Release
    // mit der Versionsnummer im Web-Flasher-Manifest (manifest.json,
    // Wurzelverzeichnis) uebereinstimmen - beim naechsten echten Release
    // beide Stellen zusammen pflegen, nicht nur eine davon.
    constexpr const char* FW_VERSION = "2.1";

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

    // ---- Ertragsbuch (siehe yield_ledger.h/.cpp und CLAUDE.md) ----
    // Seit Auftrag "Ordneraufteilung SD-Karte" in /SolarHistory (reine
    // Solardaten, bleiben bei einem Zuruecksetzen erhalten) - vorher in
    // /SolarGuardian, SD_YIELD_FILE_OLD/SD_YIELD_DAYS_CSV_OLD/
    // SD_YEARS_FILE_OLD dienen nur noch der einmaligen Migration beim
    // Start, siehe sd_storage.cpp::migrateSolarDataFolder().
    constexpr const char* SD_YIELD_FILE     = "/SolarHistory/yield.json";
    constexpr const char* SD_YIELD_TMP_FILE = "/SolarHistory/yield.json.tmp";
    constexpr const char* SD_YIELD_DAYS_CSV = "/SolarHistory/days.csv";
    constexpr const char* SD_YIELD_FILE_OLD     = "/SolarGuardian/yield.json";
    constexpr const char* SD_YIELD_DAYS_CSV_OLD = "/SolarGuardian/days.csv";
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

    // ---- Jahresuebersicht (Seite "Jahre") ----
    // Seit Auftrag "Ordneraufteilung SD-Karte" in /SolarHistory, siehe
    // Kommentar bei SD_YIELD_FILE oben.
    constexpr const char* SD_YEARS_FILE     = "/SolarHistory/years.json";
    constexpr const char* SD_YEARS_TMP_FILE = "/SolarHistory/years.json.tmp";
    constexpr const char* SD_YEARS_FILE_OLD = "/SolarGuardian/years.json";

    // Monatsuebersicht (Reiter "Monate" auf der Seite "Jahre") - bewusst
    // in /SolarHistory (nicht /SolarGuardian), analog zum Tagesertrag-
    // Backup: ein Werksreset (loescht nur /SolarGuardian) soll die
    // Monatshistorie nicht mitreissen. Wird NUR bei einem echten
    // Kalendermonatswechsel geschrieben (siehe yield_ledger.cpp
    // handleDayChange()), nie rueckwirkend erraten.
    constexpr const char* SD_MONTHS_FILE     = "/SolarHistory/months.json";
    constexpr const char* SD_MONTHS_TMP_FILE = "/SolarHistory/months.json.tmp";

    // Balkenanzahl fuer alle 3 Reiter der Seite "Jahre" (Tage/Monate/
    // Jahre) - jeweils laufende Periode + bis zu 6 fruehere, siehe Auftrag
    // "immer die letzten 7".
    constexpr uint8_t HISTORY_CHART_MAX_BARS = 7;

    // ---- Wisch-Navigation zwischen den Hauptseiten (Auftrag "Wischgeste
    // empfindlicher" - vorher musste man "gewaltig druecken", dann in
    // einer zweiten Runde "5x wischen" wegen einer zu strengen toten
    // Zone zwischen Tap- und Wisch-Schwelle) ----
    // Mindestweg in Pixeln, ab dem eine ueberwiegend waagerechte Bewegung
    // als Wisch zaehlt (vorher 50, deutlich herabgesetzt). Gilt NUR noch
    // zusammen mit "adx > ady" (ueberwiegend waagerecht) - KEIN striktes
    // Verhaeltnis mehr, das erzeugte eine tote Zone, in der echte Wisch-
    // Gesten mit etwas Hoehenversatz komplett verworfen wurden (siehe
    // swipe_input.cpp fuer die genaue Begruendung).
    constexpr int16_t SWIPE_MIN_DISTANCE_PX = 22;
    // Unter dieser Wegstrecke (in jede Richtung) gilt ein Loslassen noch
    // sicher als Tippen - alles DARUEBER mit ueberwiegend waagerechter
    // Bewegung ist ein Wisch (Auftrag "alles darueber ist ein Wischen").
    constexpr int16_t TAP_MAX_DRIFT_PX = 8;
    // Temporaeres Diagnose-Log fuer die Wisch-Erkennung (Auftrag
    // "Wischen reagiert schlechter") - loggt NUR Zeitstempel, Druckstatus,
    // x/y und die Tipp/Wisch/Verworfen-Entscheidung mit Begruendung, NIE
    // andere Daten. Nach der Diagnose wieder auf false setzen.
    constexpr bool DEBUG_TOUCH_LOG = false;
    // Kurze Druck-Aussetzer waehrend einer laufenden Beruehrung tolerieren
    // (XPT2046 bei leichtem Druck/beim Gleiten manchmal kurz unter der
    // Z-Schwelle, siehe lib/XPT2046_Touchscreen/README_PATCH.md) - erst
    // nach dieser Zeit ohne erneuten Touch gilt die Beruehrung wirklich
    // als beendet (TouchInput::wasTapped()).
    constexpr uint32_t TOUCH_DROPOUT_TOLERANCE_MS = 80;

    // ---- Touch-Ziele ----
    constexpr int16_t MIN_TOUCH_TARGET_PX = 40;

    // ---- Helligkeit (Auftrag "Helligkeit stufenlos einstellbar") ----
    // Derselbe Pin wie platformio.ini's TFT_BL=21 (TFT_eSPI-Build-Flag) -
    // hier als eigene Konstante, weil display.cpp die Hintergrund-
    // beleuchtung NACH tft.init() selbst per LEDC/PWM uebernimmt (TFT_eSPI
    // kennt nur An/Aus, keine Dimmung).
    constexpr uint8_t TFT_BL_PIN = 21;
    // Nie 0% - sonst bleibt das Display komplett dunkel und man kann sich
    // aussperren (Auftrag), da es keinen Hardware-Knopf fuer die
    // Beleuchtung gibt.
    constexpr uint8_t BRIGHTNESS_MIN_PERCENT = 5;

    // ---- Startlogo (siehe boot_logo.h) ----
    // Mindestanzeigezeit ab dem Erscheinen des Logos, nicht zusaetzlich
    // zur restlichen Startzeit (siehe main.cpp::setup()).
    constexpr uint32_t BOOT_LOGO_MS = 7000;

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
    // Nachts (22:55-04:30, NUR nach bestaetigter Abschlussabfrage/
    // Sicherung) laeuft statt des vollen Abrufs nur ein reiner, passiver
    // Batterie-Abruf in diesem (laengeren) Takt - Auftrag "Batterie auch
    // nachts abfragen", siehe fetch_task.cpp::taskFn().
    constexpr uint32_t EPEVER_NIGHT_BATTERY_INTERVAL_MS = 10UL * 60UL * 1000UL;
    constexpr uint32_t EPEVER_RETRY_INTERVAL_MS = 60UL * 1000UL;        // Takt bei den ersten Fehlschlaegen
    constexpr uint8_t  EPEVER_MAX_SHORT_RETRIES = 3;                    // danach zurueck auf den normalen Takt
    constexpr uint32_t EPEVER_MANUAL_REFRESH_MIN_INTERVAL_MS = 30UL * 1000UL;
    // Harte Obergrenze fuer die "Aktualisiere..."-Rueckmeldung (Auftrag
    // "Fehler in der Refresh-Rueckmeldung") - die urspruengliche
    // "Working"-Anzeige hatte KEINE eigene Zeitgrenze, sondern wartete
    // unbegrenzt auf das naechste Ereignis (Done/NoChange/...). Blieb
    // dieses Ereignis aus (z.B. WLAN faellt direkt nach dem Tipp kurz
    // weg, bevor der geplante Abruf ueberhaupt starten konnte), stand
    // die Rueckmeldung fuer immer da UND verdeckte dabei den zuletzt
    // gueltigen Wert - beides beobachtet und behoben, siehe
    // refresh_feedback_text.h::FeedbackState.
    constexpr uint32_t REFRESH_WORKING_TIMEOUT_MS = 15UL * 1000UL;
    // "Aktualisierung ab 04:30 Uhr wieder moeglich" stand bisher nur die
    // normalen ~2s (wie jede andere Rueckmeldung) - laut Alex zu kurz,
    // um bewusst wahrgenommen zu werden (Auftrag). Nur diese EINE
    // Rueckmeldung bekommt deshalb eine laengere Anzeigedauer, siehe
    // refresh_feedback_text.h::FeedbackState::update().
    constexpr uint32_t REFRESH_LOCKED_NIGHT_SHOW_MS = 4UL * 1000UL;
    constexpr uint32_t EPEVER_STALE_AFTER_MS = 30UL * 60UL * 1000UL;    // Werte gelten danach als veraltet
    constexpr const char* EPEVER_BATTERY_URL = "https://hncloudapi.epsolarpv.com:7002/history/getSampleDataPoint";
    // Fenster/sampleLimit fuer den "neuesten Wert"-Abruf (Auftrag
    // "Fehler im Abruf beheben - Werte systematisch ~30 Min alt"):
    // VORHER 3h-Fenster mit sampleLimit=5 - bei einem Portal, das die
    // Batteriespannung JEDE MINUTE speichert (Alex' Live-Vergleich),
    // liefert ein so breites Fenster mit so wenigen erlaubten Werten
    // offenbar NICHT die letzten 5 Minutenwerte, sondern (wie ein
    // Chart-Downsampling) ca. 5 gleichmaessig ueber das Fenster verteilte
    // Stuetzstellen (3h/5 = 36 Min Abstand) - der neueste zurueckgegebene
    // Wert lag dadurch systematisch bis zu ~36 Min (beobachtet: 30-118
    // Min bei laengeren Nachtpausen) hinter "jetzt" zurueck, obwohl
    // frischere Minutenwerte im Portal laengst vorlagen. JETZT: deutlich
    // schmaleres Fenster UND mehr erlaubte Werte, damit selbst bei
    // gleicher Downsampling-Logik die juengste Stuetzstelle nur noch
    // wenige Minuten alt sein kann (20 Min / 10 Werte = 2 Min
    // Bucket-Abstand). END_BUFFER_MS faengt eine leichte Uhrabweichung
    // unseres Geraets gegenueber dem Portal ab (Endzeit minimal NACH
    // "jetzt", damit der aktuellste Portal-Wert nie knapp ausserhalb des
    // Fensters liegt).
    constexpr int64_t EPEVER_BATTERY_WINDOW_MS = 20LL * 60LL * 1000LL;
    constexpr int64_t EPEVER_BATTERY_END_BUFFER_MS = 2LL * 60LL * 1000LL;
    constexpr uint8_t EPEVER_BATTERY_SAMPLE_LIMIT = 10;

    // ---- Aktiv-Auslesen beim Antippen (Auftrag "Aktualisieren-Knopf
    // soll echt etwas bewirken", Korrektur B "an die echte Portal-
    // Anfrage anpassen") ----
    // EPEVER_BATTERY_URL oben liefert nur den zuletzt im Portal
    // GESPEICHERTEN Messwert (Historien-Abfrage), keine frische Messung
    // vom Regler - bestaetigter Befund, siehe CLAUDE.md. Fuer einen
    // ECHTEN Tipp-Effekt wird stattdessen beim Regler selbst eine neue
    // Messung angestossen: "userop/add" (Alex' Live-Mitschnitt, siehe
    // unten), danach wird mit dem BEREITS BESTEHENDEN, portal-
    // verifizierten EPEVER_BATTERY_URL oben (history/getSampleDataPoint)
    // kurz wiederholt nachgefragt, bis dessen Zeitstempel neuer ist -
    // KEIN separater, unbestaetigter zweiter Endpunkt mehr (der fruehere
    // "getStatusByEquipmentId" war nur eine Vermutung, siehe Git-
    // Historie - jetzt durch den bereits produktiv funktionierenden
    // Abruf ersetzt, das ist sicherer).
    // URL/Host wie die anderen "usrCloud/vn/..."-Aufrufe (Port 443, siehe
    // EPEVER_ENERGY_URL oben) - NICHT Port 7002 wie beim reinen Lesen.
    constexpr const char* EPEVER_ACTIVE_READ_TRIGGER_URL = "https://hncloudapi.epsolarpv.com/usrCloud/vn/devicelog/userop/add";
    // Der einzig erlaubte opType (0 = NUR Lesen) steht NICHT hier,
    // sondern als einzige Quelle der Wahrheit in epever_active_read.h
    // (EpeverActiveRead::ALLOWED_OP_TYPE) - dort auch die Whitelist-
    // Pruefungsfunktion dafuer, mit eigenem nativem Test.
    // slaveIndex: in Alex' Mitschnitt ein fester Text "1" - es gibt in
    // der Geraeteliste (tryResolveEquipmentList()) KEIN entsprechendes
    // Feld, deshalb hier als feste Konstante hinterlegt (nicht aus der
    // Geraeteliste geholt). Falls eine Anlage mehrere Regler/Slaves am
    // selben Gateway haben sollte, muesste das spaeter angepasst werden.
    constexpr const char* EPEVER_ACTIVE_READ_SLAVE_INDEX = "1";
    // Zeitgrenze fuer das WARTEN auf den neuen Zeitstempel (Korrektur B2:
    // live gemessen ca. 27s bis zur Antwort, 50s als Sicherheitsmarge).
    // Korrektur "Fehler im Abruf beheben", Punkt 3: Alex hat gemessen,
    // dass der neue Wert schon nach ca. 7s im Portal-Verlauf stand -
    // deshalb erst ca. 5s warten (statt sofort/zu frueh nachzufragen),
    // danach alle 3s.
    constexpr uint32_t EPEVER_ACTIVE_READ_WAIT_TIMEOUT_MS = 50UL * 1000UL;
    constexpr uint32_t EPEVER_ACTIVE_READ_FIRST_POLL_DELAY_MS = 5UL * 1000UL;
    constexpr uint32_t EPEVER_ACTIVE_READ_POLL_INTERVAL_MS = 3UL * 1000UL;
    // Harte Obergrenze der "Lade..."-RUECKMELDUNG selbst (Korrektur B2) -
    // bewusst GROESSER als EPEVER_ACTIVE_READ_WAIT_TIMEOUT_MS oben (deckt
    // zusaetzlich den Trigger-Request selbst und etwas Sicherheitsmarge
    // ab), damit die Anzeige nicht vorzeitig aufgibt, waehrend im
    // Hintergrund noch gewartet wird. Siehe refresh_feedback_text.h
    // (SolarData::RefreshFeedback::WorkingActiveRead).
    constexpr uint32_t EPEVER_ACTIVE_READ_FEEDBACK_TIMEOUT_MS = 55UL * 1000UL;
    // Schutz gegen Dauer-Tippen (Auftrag, Korrektur B3: die Grenzen
    // zaehlen BEFEHLE, nicht Antippen - jeder einzelne Datenpunkt-Befehl
    // erzeugt einen eigenen Eintrag im Operationsprotokoll der Anlage).
    // Der Mindestabstand gilt weiterhin zwischen zwei TIPPEN
    // (ein Tipp kann mehrere Befehle ausloesen, siehe CLAUDE.md Tabelle),
    // ZUSAETZLICH zur bestehenden allgemeinen 30s-Tippsperre
    // (EPEVER_MANUAL_REFRESH_MIN_INTERVAL_MS oben, gilt fuer JEDEN
    // Refresh-Tipp).
    constexpr uint32_t EPEVER_ACTIVE_READ_MIN_INTERVAL_MS = 60UL * 1000UL;
    constexpr uint8_t  EPEVER_ACTIVE_READ_MAX_PER_HOUR = 20;
    constexpr uint8_t  EPEVER_ACTIVE_READ_MAX_PER_DAY = 100;

    // ---- Tagesfenster/Nachtmodus (Auftrag "Tagesfenster mit Nachtmodus")
    // ----
    // Ergebnis der mehrtaegigen PORTAL_DAY_PROBE-Messung (siehe
    // portal_day_probe.h, Code bleibt abgeschaltet liegen): das Portal
    // wechselt seinen "aktuellen Tag" taeglich in einer festen Zeitzone
    // UTC+2 um 00:00 Uhr dort - das war bis zur Winterzeit-Umstellung am
    // 25.10.2026 exakt Mitternacht lokaler Zeit, seitdem 23:00 Uhr lokal.
    // Die App fragt deshalb nur noch zwischen DAY_START und DAY_END ab,
    // die Abschlussabfrage (DAY_END bis FINAL_QUERY_DEADLINE) liegt damit
    // sicher VOR diesem Portal-Wechsel, unabhaengig von Zeitzone/Sommer-
    // oder Winterzeit - siehe day_window.h.
    constexpr uint8_t DAY_START_HOUR = 4;
    constexpr uint8_t DAY_START_MINUTE = 30;
    constexpr uint8_t DAY_END_HOUR = 22;
    constexpr uint8_t DAY_END_MINUTE = 30;
    constexpr uint8_t FINAL_QUERY_DEADLINE_HOUR = 22;
    constexpr uint8_t FINAL_QUERY_DEADLINE_MINUTE = 55;
    // "alle paar Minuten" erneut versuchen, falls die Abschlussabfrage
    // zunaechst fehlschlaegt (schneller als der normale 5-Minuten-Takt,
    // aber nicht so aggressiv wie der normale Kurzfehler-Takt).
    constexpr uint32_t FINAL_QUERY_RETRY_INTERVAL_MS = 3UL * 60UL * 1000UL;

    // ---- Tagesverlauf (Seite "Tagesverlauf", ganztaegige Leistungskurve
    // ueber die gleiche Port-7002-Abfrage wie die 3h-Kurve, siehe
    // fetch_task.cpp::runDailyCurveBackfillIfNeeded()). Die Kurve "gehoert"
    // zum Kalendertag, der um START_HOUR beginnt - vor START_HOUR lokal
    // zeigt sie noch den VORTag (bleibt stehen bis zum naechsten Morgen).
    constexpr uint8_t DAILY_CURVE_START_HOUR = 5;  // 05:00 lokal
    constexpr uint8_t DAILY_CURVE_END_HOUR = 22;   // 22:00 lokal
    // Zeitfenster pro Nachlade-Anfrage - das Gesamtfenster (bis zu 17h)
    // wird in Haeppchen dieser Groesse aufgeteilt (Auftrag "zu grosse
    // Antwort aufteilen"), jedes Haeppchen nutzt dieselbe
    // EpeverClient::POWER_CURVE_MAX_POINTS-Begrenzung wie die 3h-Kurve.
    constexpr uint32_t DAILY_CURVE_CHUNK_SECONDS = 4UL * 3600UL;

    // ---- Geraeteliste (automatische Seriennummer-Ermittlung, siehe
    // device_no_resolution.h/fetch_task.cpp) ----
    constexpr const char* EPEVER_EQUIPMENT_LIST_URL = "https://hncloudapi.epsolarpv.com/usrCloud/vn/equipment/getEquipmentListPage";
    // Hoechstens einmal in dieser Zeitspanne abfragen (Auftrag) - egal ob
    // durch Login, fehlende Nummer oder einen 5004-Fehler ausgeloest.
    constexpr uint32_t EQUIPMENT_LIST_MIN_INTERVAL_MS = 10UL * 60UL * 1000UL;

    // ---- Wetter (Open-Meteo.com, kostenlos, ohne Zugangsschluessel) ----
    constexpr const char* WEATHER_API_HOST = "https://api.open-meteo.com/v1/forecast";
    constexpr const char* GEOCODING_API_HOST = "https://geocoding-api.open-meteo.com/v1/search";
    constexpr uint32_t WEATHER_HTTP_TIMEOUT_MS = 10000;
    constexpr uint32_t WEATHER_FETCH_INTERVAL_MS = 30UL * 60UL * 1000UL; // alle 30 Minuten
    constexpr uint32_t WEATHER_RETRY_INTERVAL_MS = 2UL * 60UL * 1000UL;  // bei Fehler alle 2 Minuten erneut
    constexpr uint8_t WEATHER_FORECAST_DAYS = 7;

    // ---- Aktuelles Wetter: Symbol/Bewoelkung ueber MET Norway (trifft
    // Realitaet laut Vergleich besser), Open-Meteo bleibt Rueckfall sowie
    // Quelle fuer Vorhersage/Standortsuche (siehe weather_client.cpp) ----
    constexpr const char* MET_NORWAY_API_HOST = "https://api.met.no/weatherapi/locationforecast/2.0/compact";
    constexpr const char* MET_NORWAY_USER_AGENT = "SolarGuardianCYD/1.4 github.com/Eiswolf-BG/Solar-Guardian-CYD";

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
