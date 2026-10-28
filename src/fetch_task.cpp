#include "fetch_task.h"
#include "wifi_connect_manager.h"
#include "wifi_status.h"
#include "solar_data.h"
#include "account_bridge.h"
#include "location_bridge.h"
#include "epever_client.h"
#include "active_read_limiter.h"
#include "weather_client.h"
#include "met_norway_client.h"
#include "weather_data.h"
#include "ip_geolocation_client.h"
#include "ip_location_data.h"
#include "device_no_resolution.h"
#include "fetch_heartbeat.h"
#include "http_deadline_guard.h"
#include "day_window.h"
#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <WiFi.h>
#include <time.h>

namespace FetchTask {

namespace {
    constexpr uint32_t STACK_SIZE = 12288;
    constexpr UBaseType_t PRIORITY = 1;
    constexpr BaseType_t CORE_ID = 0;
    constexpr uint32_t HEAP_LOG_INTERVAL_MS = 60000;
    constexpr uint32_t TICK_DELAY_MS = 200; // nie busy-wait, siehe CLAUDE.md

    TaskHandle_t taskHandle = nullptr;
    QueueHandle_t commandQueue = nullptr;

    // Queue-Element (Auftrag "Aktiv-Auslesen beim Antippen", Korrektur
    // B3): Command::RefreshNow braucht zusaetzlich, WELCHE Datenpunkte
    // aktiv auszulesen sind (genau die, die der angetippte Bereich
    // zeigt) - alle anderen Befehle lassen refresh einfach leer
    // (dataPointCount==0).
    struct QueueItem {
        Command cmd;
        FetchTask::RefreshRequest refresh;
    };

    volatile bool pauseRequested = false;
    volatile bool pausedAck = false;
    bool manualDisconnect = false;

    // Plausibilitaetspruefung eines Epoch-Werts (gleiches Prinzip wie
    // yield_ledger.cpp's Datumsschutz: Jahr < 2024 ablehnen) - robuster
    // als sich auf getLocalTime(&t, 5) (5ms-Timeout, kann spontan/
    // voruebergehend false liefern, auch WEIT nach echter NTP-
    // Synchronisation - haengt an Systemlast, kein verlaesslicher
    // "Zeit noch nicht synchron"-Indikator) zu verlassen, um EINMALIG
    // einen Zeitstempel (z.B. lastFetchAttemptEpoch) zu uebernehmen.
    // Live beobachtet: ein auf hasValidTime gestuetzter Schutz blieb
    // teils minutenlang im Fallback haengen, obwohl die Uhr laengst
    // korrekt lief.
    bool epochPlausible(time_t t) {
        if (t <= 0) return false;
        struct tm tmCheck;
        localtime_r(&t, &tmCheck);
        int year = tmCheck.tm_year + 1900;
        return year >= 2024 && year <= 2099;
    }

    // Portal-Zustand - ausschliesslich vom Fetch-Task selbst gelesen und
    // geschrieben (Core 0). Die Zugangsdaten kommen NUR ueber
    // AccountBridge (Mutex-Kopie von Core 1), niemals per direktem Zugriff
    // auf appConfig. currentToken lebt nur im RAM dieses Tasks, wird nie
    // auf der SD-Karte gespeichert.
    String currentEmail;
    String currentPassword;
    String currentDeviceNo;
    String currentToken;
    bool hasAccount = false;
    // Ohne Seriennummer keine Solardaten-Anfragen (Auftrag "Seriennummer
    // aus dem Projekt entfernen") - Ertrag/Batterie/Leistungskurve laufen
    // nur mit hasDeviceNo, damit nie eine leere Nummer an die API geht.
    // Login + Geraeteliste duerfen aber trotzdem laufen (Auftrag
    // "Seriennummer automatisch ermitteln") - siehe
    // runDeviceNoOnlyCycle()/tryResolveEquipmentList() unten.
    bool hasDeviceNo = false;
    bool accountInitialized = false; // false bis zum ersten applyAccountSnapshot()-Aufruf
    bool needsLogin = false;
    uint8_t consecutiveFailures = 0;
    uint32_t lastFetchAttemptMs = 0;
    // Wall-Clock-Pendant zu lastFetchAttemptMs (Auftrag "Diagnosezeilen
    // fuer die naechste Nacht" deckte auf: die bisherige Berechnung von
    // candidateNextAt mischte millis()- und time()-Domaene
    // (nowEpoch + msUntilNext/1000) - die beiden Uhren laufen zwar
    // gleich schnell, werden aber zu leicht unterschiedlichen Zeitpunkten
    // gelesen, wodurch die runden Sekunden beim Teilen/Addieren fast
    // jeden Tick um +-1s kippen konnten. Das unterlief die eigentlich
    // vorgesehene Anti-Flacker-Bremse (siehe lastSentNextFetchAt unten) -
    // candidateNextAt aenderte sich dadurch faktisch fast jeden Tick,
    // nicht nur bei einer echten Aenderung. Mit lastFetchAttemptEpoch
    // (reine time()-Domaene) wird candidateNextAt jetzt komplett ohne
    // erneuten Bezug auf "jetzt" berechnet - stabil, bis der naechste
    // ECHTE Abruf stattfindet.
    time_t lastFetchAttemptEpoch = 0;
    uint32_t lastEquipmentListAttemptMs = 0; // Drosselung, siehe tryResolveEquipmentList()
    time_t lastSentNextFetchAt = -1; // siehe runFetchCycle()-Aufrufstelle, Anti-Flacker-Bremse

    // Naechtlicher Batterie-Abruf (Auftrag "Batterie auch nachts
    // abfragen") - eigener, von lastFetchAttemptMs/-Epoch komplett
    // getrennter Zeitplan-Zustand, gleiches Prinzip wie oben
    // (millis()-Takt fuer "due", EIGENE time()-Domaene fuer die stabile
    // Anzeige, Anti-Flacker-Bremse). lastNightBatteryDate erkennt einen
    // neuen Abend (wie finalQueryDoneDate) - dann wird sofort (nicht erst
    // nach 10 Minuten) einmal abgefragt.
    char lastNightBatteryDate[11] = {0};
    uint32_t lastNightBatteryFetchMs = 0;
    time_t lastNightBatteryFetchEpoch = 0;
    time_t lastSentNightBatteryNextFetchAt = -1;
    // Tagesfenster/Nachtmodus (Auftrag): Datum des Abends, fuer den die
    // Abschlussabfrage bereits erfolgreich war - leer = noch nicht. Wird
    // beim naechsten Tageswechsel automatisch "veraltet" (siehe
    // DayWindow::currentNightEveningDate(), Vergleich in taskFn()).
    char finalQueryDoneDate[11] = {0};
#ifdef NIGHT_MODE_DIAG_TEST
    // Logzeilen fuer die Abschlussabfrage/den Nachtmodus (Auftrag "knappe
    // Logzeilen fuer heute Abend", NUR in der lokalen Testversion aktiv) -
    // je ein Datum/Flag, damit die jeweilige Meldung nur EINMAL pro
    // Abend/Uebergang erscheint statt bei jedem einzelnen
    // Schleifendurchlauf.
    char finalQueryStartLoggedDate[11] = {0};
    bool nightPauseLogged = false;
#endif
    uint32_t lastManualRefreshMs = 0;
    // Schutz gegen Dauer-Tippen beim Aktiv-Auslesen (Auftrag) - eigener,
    // zusaetzlicher Zustand neben lastManualRefreshMs oben, siehe
    // active_read_limiter.h.
    ActiveReadLimiter::State activeReadLimiterState;
    bool forceFetchNow = false;
    // Auftrag "Refresh-Regeln und Rueckmeldung": fetchInProgress ist in
    // diesem einfaedigen Ablauf praktisch nie waehrend handleCommand()
    // beobachtbar (Befehle werden nur VOR einem Abruf abgearbeitet, nie
    // waehrenddessen) - bleibt trotzdem gesetzt, fuer den Fall, dass sich
    // das spaeter aendert, und zur Dokumentation. manualRefreshPending
    // merkt sich, ob der GERADE LAUFENDE Abruf durch einen manuellen Tipp
    // ausgeloest wurde (fuer die Done/Keine-neuen-Daten-Rueckmeldung
    // danach).
    bool fetchInProgress = false;
    bool manualRefreshPending = false;
    // Welche Datenpunkte der aktuell vorgemerkte Tipp aktiv auslesen soll
    // (Auftrag "Aktiv-Auslesen", Korrektur B3) - von handleCommand()
    // Command::RefreshNow gesetzt, von der due-Stelle unten konsumiert.
    RefreshRequest pendingActiveRead;
    bool firstFetchHeapLogged = false;
    bool firstBatteryHeapLogged = false;
    bool firstPowerCurveHeapLogged = false;
    bool wifiWasConnected = false;
    // Tagesverlauf (Seite "Tagesverlauf") - der Kurven-Tag, fuer den
    // zuletzt ein Nachladen (siehe runDailyCurveBackfillIfNeeded())
    // abgeschlossen wurde. Leer beim Start -> erzwingt das erste
    // Nachladen beim naechsten erfolgreichen Zyklus.
    char lastBackfilledCurveDayKey[11] = "";

    // Wetter-Standort (siehe location_bridge.h) - unabhaengig vom
    // Solar-Guardian-Konto, deshalb eigene Felder statt in
    // currentEmail/currentPassword/currentDeviceNo mitgefuehrt.
    float currentLat = 0.0f;
    float currentLon = 0.0f;
    bool hasLocation = false;
    bool locationIsManual = false;
    uint32_t lastWeatherFetchAttemptMs = 0;
    bool forceWeatherFetchNow = false;
    uint8_t weatherConsecutiveFailures = 0;

    // Automatische IP-Standort-Ermittlung (siehe ip_geolocation_client.h) -
    // eigener Zeitplan, unabhaengig vom Wetter-Takt: direkt nach dem
    // WLAN-Verbinden (erster Versuch), danach einmal pro Tag, bei jedem
    // WLAN-Netzwechsel sofort, bei Fehler alle 2 Minuten erneut - immer
    // NUR, wenn der Standort nicht von Hand gesetzt wurde.
    uint32_t lastIpLocationAttemptMs = 0;
    uint8_t ipLocationConsecutiveFailures = 0;
    uint8_t lastWifiSlotForLocation = 255;

    // Port-7002 (Batterie/History) - chargeStatus-Logging nur bei
    // Aenderung.
    bool haveLastChargeStatus = false;
    bool lastChargeStatusWasText = false;
    float lastChargeStatusValue = 0.0f;
    String lastChargeStatusText;

    // Vorwaertsdeklaration - Definition weiter unten, Aufrufer (u.a.
    // runBatteryFetch()/runPowerCurveFetch() bei status 5004) stehen im
    // Code davor.
    void tryResolveEquipmentList();

    void applyAccountSnapshot() {
        AccountBridge::Snapshot snap = AccountBridge::get();

        // Geaenderte Seriennummer (nicht beim allerersten Aufruf beim
        // Taskstart) verwirft die bisherigen Batterie-/Leistungs-/
        // Ertragswerte - sonst wuerden kurz noch die Zahlen der alten
        // Anlage angezeigt, waehrend die erste Abfrage mit der neuen
        // Nummer laeuft (siehe Auftrag "Seriennummer am Geraet aendern").
        if (accountInitialized && snap.deviceNo != currentDeviceNo) {
            SolarData::resetForDeviceChange();
            Serial.println("[fetch] Geraete-Seriennummer geaendert - alte Werte verworfen, neue Abfrage laeuft");
        }
        accountInitialized = true;

        currentEmail = snap.email;
        currentPassword = snap.password;
        currentDeviceNo = snap.deviceNo;
        hasAccount = currentEmail.length() > 0 && currentPassword.length() > 0;
        hasDeviceNo = currentDeviceNo.length() > 0;
        currentToken = ""; // altes Token verwerfen, siehe Auftrag
        needsLogin = hasAccount;
        consecutiveFailures = 0;
        lastFetchAttemptMs = 0; // beim naechsten Tick (mit WLAN) sofort faellig
        lastFetchAttemptEpoch = 0;
        SolarData::FetchStatus initialStatus;
        if (!hasAccount) initialStatus = SolarData::FetchStatus::NoAccount;
        else if (!hasDeviceNo) initialStatus = SolarData::FetchStatus::NoDeviceNo;
        else initialStatus = SolarData::FetchStatus::LoggingIn;
        SolarData::setFetchStatus(initialStatus);
    }

    // Wetter-Standort (siehe location_bridge.h) - unabhaengig vom
    // Solar-Guardian-Konto, deshalb eigene kleine Snapshot-Funktion statt
    // in applyAccountSnapshot() mitgefuehrt. Loest bei einer Aenderung
    // sofort eine neue Wetterabfrage aus (naechster Tick), statt auf den
    // 30-Minuten-Takt zu warten.
    void applyLocationSnapshot() {
        LocationBridge::Snapshot snap = LocationBridge::get();
        currentLat = snap.lat;
        currentLon = snap.lon;
        hasLocation = snap.valid;
        locationIsManual = snap.manual;
        WeatherData::setLocationSet(hasLocation);
        weatherConsecutiveFailures = 0;
        lastWeatherFetchAttemptMs = 0; // beim naechsten Tick (mit WLAN) sofort faellig
    }

    // Automatische IP-Standort-Ermittlung - Ortsname/Koordinaten werden
    // NIE geloggt (Auftrag), nur HTTP-Code und Erfolg/Fehlschlag.
    void runIpLocationDetect() {
        IpGeolocationClient::Result result;
        int httpCode = -1;
        bool ok = IpGeolocationClient::detect(result, &httpCode);
        Serial.printf("[fetch] IP-Standort-Ermittlung: HTTP-Code=%d erfolgreich=%s\n", httpCode, ok ? "ja" : "nein");
        if (ok) {
            IpLocationData::setResult(result.lat, result.lon, result.name, true);
            ipLocationConsecutiveFailures = 0;
        } else {
            ipLocationConsecutiveFailures++;
        }
    }

    // Wetterabfrage (Open-Meteo, siehe weather_client.h) - unabhaengig vom
    // Solar-Guardian-Konto, laeuft auch ohne eingerichtetes Portal-Konto.
    // Ortsname/Koordinaten werden NIE geloggt (Auftrag), nur HTTP-Code und
    // Antwortgroesse.
    void runWeatherFetch() {
        int httpCode = -1;
        size_t responseBytes = 0;
        bool ok = WeatherClient::fetch(currentLat, currentLon, &httpCode, &responseBytes);
        Serial.printf("[fetch] Wetter: HTTP-Code=%d Antwortgroesse=%u Byte\n", httpCode, (unsigned)responseBytes);
        weatherConsecutiveFailures = ok ? 0 : (uint8_t)(weatherConsecutiveFailures + 1);

        // Aktuelles Wetter zusaetzlich ueber MET Norway verfeinern (trifft
        // laut Quellenvergleich deutlich besser) - ueberschreibt Temperatur
        // UND Symbol. Schlaegt es fehl, bleiben die zuletzt gueltigen
        // (Open-Meteo-)Werte einfach stehen.
        float metTemp = 0.0f;
        int16_t metCode = -1;
        float metCloud = -1.0f;
        int metHttpCode = -1;
        size_t metResponseBytes = 0;
        bool metOk = MetNorwayClient::fetchCurrent(currentLat, currentLon, metTemp, metCode, metCloud, &metHttpCode, &metResponseBytes);
        Serial.printf("[fetch] MET Norway: HTTP-Code=%d Antwortgroesse=%u Byte\n", metHttpCode, (unsigned)metResponseBytes);
        if (metOk) WeatherData::setCurrentRefined(metTemp, metCode);
    }

    // Batterie-/History-Abfrage (Port 7002) - Authentifizierung ueber den
    // Header "token" (siehe epever_client.cpp::fetchBatteryData()). Bei
    // 401 oder jedem sonstigen Fehler wird das Token einmal erneuert
    // (neu einloggen) und die Abfrage wiederholt - genau wie bei der
    // Ertragsabfrage in runFetchCycle(). Schreibt Ergebnisse in SolarData
    // und liefert den Tagesertrag-Datenpunkt fuer das Ertragsbuch zurueck.
    // activeReadDataPointIds/activeReadCount (Auftrag "Aktualisieren-
    // Knopf soll echt etwas bewirken", Korrektur B3): activeReadCount==0
    // bedeutet "normaler, passiver Abruf" (geplanter 5-Minuten-Takt UND
    // naechtlicher Batterietakt - BEIDE nutzen NIE ein aktives Auslesen).
    // activeReadCount>0 NUR vom manuellen Tipp gesetzt (siehe
    // runFetchCycle()/handleCommand() Command::RefreshNow) - liest dann
    // GENAU die vom tippenden Bereich angeforderten Datenpunkte aktiv
    // aus (siehe CLAUDE.md Tabelle), nicht automatisch alle.
    // updateNonBatteryFields=false (Auftrag "Batterie auch nachts
    // abfragen", Korrektur A3): NUR vom naechtlichen Batterie-Pfad
    // gesetzt - der Nachtabruf darf laut Auftrag NUR die Batteriefelder
    // aendern, PV-Spannung/-Strom/-Leistung (zeigen sich sonst unbemerkt
    // auf den Seiten "Leistung"/"PV-Status", obwohl deren "Portaldaten X
    // Min alt"-Zeile nachts gar nicht aktualisiert wird) bleiben dadurch
    // unangetastet. Tagsueber immer true (Standardwert).
    bool runBatteryFetch(float& outDailyGenKwh, bool& outDailyGenValid,
                         const int32_t* activeReadDataPointIds, uint8_t activeReadCount,
                         bool updateNonBatteryFields = true) {
        outDailyGenValid = false;
        bool useActiveRead = activeReadCount > 0;

        EpeverClient::BatteryResult result;
        int httpCode = 0, apiStatus = 0;
        bool activeReadFresh = false;
        bool ok = useActiveRead
            ? EpeverClient::activeReadBattery(currentDeviceNo, currentToken, activeReadDataPointIds, activeReadCount, result, activeReadFresh, &httpCode, &apiStatus)
            : EpeverClient::fetchBatteryData(currentDeviceNo, currentToken, result, &httpCode, &apiStatus);

        if (!ok && (httpCode == 401 || apiStatus != 0)) {
            currentToken = "";
            if (EpeverClient::login(currentEmail, currentPassword, currentToken)) {
                needsLogin = false;
                ok = useActiveRead
                    ? EpeverClient::activeReadBattery(currentDeviceNo, currentToken, activeReadDataPointIds, activeReadCount, result, activeReadFresh, &httpCode, &apiStatus)
                    : EpeverClient::fetchBatteryData(currentDeviceNo, currentToken, result, &httpCode, &apiStatus);
            } else {
                needsLogin = true;
            }
        }

        if (useActiveRead) {
            Serial.printf("[fetch] Aktiv-Auslesen: ok=%d frisch=%d HTTP-Code=%d status=%d\n",
                          ok ? 1 : 0, activeReadFresh ? 1 : 0, httpCode, apiStatus);
        }
        Serial.printf("[fetch] Port 7002: HTTP-Code=%d status=%d\n", httpCode, apiStatus);

        if (!ok) {
            // 5004 = "Insufficient permissions", typisch bei einer
            // falschen/veralteten Seriennummer (Auftrag 2c) - sofort
            // pruefen (intern gedrosselt).
            if (apiStatus == 5004) tryResolveEquipmentList();
            return false;
        }

        time_t measuredAt = result.voltage.valid ? result.voltage.time : time(nullptr);
        SolarData::setBattery(result.voltage.valid ? result.voltage.value : 0.0f,
                               result.current.valid ? result.current.value : 0.0f,
                               result.soc.valid ? result.soc.value : 0.0f,
                               result.temperature.valid ? result.temperature.value : 0.0f,
                               measuredAt);

        // Max./Min.-Spannung (Auftrag) - nur bei BEIDEN gueltigen Werten
        // uebernehmen, sonst bleiben die zuletzt gueltigen Werte stehen.
        if (result.voltageMax.valid && result.voltageMin.valid) {
            SolarData::setBatteryVoltageExtremes(result.voltageMax.value, result.voltageMin.value);
        }

        if (result.chargeStatus.valid) {
            bool changed = !haveLastChargeStatus ||
                           lastChargeStatusWasText != result.chargeStatus.isText ||
                           (result.chargeStatus.isText ? lastChargeStatusText != result.chargeStatus.text
                                                        : lastChargeStatusValue != result.chargeStatus.value);
            if (changed) {
                haveLastChargeStatus = true;
                lastChargeStatusWasText = result.chargeStatus.isText;
                lastChargeStatusValue = result.chargeStatus.value;
                lastChargeStatusText = result.chargeStatus.text;
                if (result.chargeStatus.isText) {
                    Serial.printf("[fetch] ladestatus raw=\"%s\"\n", result.chargeStatus.text.c_str());
                } else {
                    Serial.printf("[fetch] ladestatus raw=%.0f\n", result.chargeStatus.value);
                }
            }
            // Zusaetzlich zum Nur-bei-Aenderung-Log oben: Rohwert +
            // Batteriestrom bei JEDER Abfrage, damit sich Faelle wie
            // "Ausgleich bei 50mA" im Log nachvollziehen lassen (siehe
            // battery_page.cpp effectiveStatus() - ab 1,0A gilt
            // Schnellladen/Ausgleich erst als bestaetigt).
            if (!result.chargeStatus.isText) {
                Serial.printf("[fetch] ladestatus raw=%.0f I=%.2fA\n",
                              result.chargeStatus.value,
                              result.current.valid ? result.current.value : 0.0f);
            }
            if (result.chargeStatus.isText) {
                SolarData::setChargeStatusText(result.chargeStatus.text.c_str());
            } else {
                SolarData::setChargeStatusCode((int16_t)result.chargeStatus.value);
            }
        }

        if (result.dailyGeneration.valid) {
            outDailyGenKwh = result.dailyGeneration.value;
            outDailyGenValid = true;
        }

        if (updateNonBatteryFields && (result.pvVoltage.valid || result.pvCurrent.valid)) {
            time_t pvMeasuredAt = result.pvVoltage.valid ? result.pvVoltage.time
                                                           : (result.pvCurrent.valid ? result.pvCurrent.time : time(nullptr));
#ifdef NIGHT_MODE_DIAG_TEST
            // Auftrag "veraltet erscheint obwohl Werte aktuell sind" -
            // pvMeasuredAt ist der vom PORTAL gemeldete Messzeitpunkt
            // dieses Datenpunkts (result.pvVoltage.time), NICHT der
            // Zeitpunkt UNSERES Abrufs - falls das Portal selbst einen
            // aelteren Zeitstempel fuer PV-Spannung/-Strom liefert als
            // fuer andere Werte (z.B. weil der Regler diese Register
            // seltener aktualisiert), waere "veraltet" trotz frischem
            // eigenen Abruf korrekt und kein Fehler. Diese Zeile macht
            // das Alter direkt sichtbar.
            long ageSec = (long)(time(nullptr) - pvMeasuredAt);
            struct tm pt; localtime_r(&pvMeasuredAt, &pt);
            Serial.printf("[fetch] PV-Messzeitpunkt (laut Portal): %02d:%02d, Alter=%lds\n",
                          pt.tm_hour, pt.tm_min, ageSec);
#endif
            SolarData::setPv(result.pvVoltage.valid ? result.pvVoltage.value : 0.0f,
                              result.pvCurrent.valid ? result.pvCurrent.value : 0.0f,
                              pvMeasuredAt);
        }

        // PV-Leistung (Auftrag "Aktiv-Auslesen", Korrektur B3) - nur bei
        // einem AKTIVEN Auslesen der Uebersicht-Kachel "Leistung" relevant
        // (der normale 5-Minuten-Takt aktualisiert currentPowerW weiterhin
        // ausschliesslich ueber die separate Leistungskurven-Abfrage,
        // siehe runPowerCurveFetch()) - gleiche Ausnahme wie bei PV-
        // Spannung/-Strom oben (nachts nie, siehe updateNonBatteryFields).
        if (updateNonBatteryFields && result.power.valid) {
            SolarData::setPower(result.power.value, result.power.time);
        }

        if (!firstBatteryHeapLogged) {
            firstBatteryHeapLogged = true;
            Serial.printf("[fetch] minFreeHeap nach erster Port-7002-Abfrage: %u\n",
                          (unsigned)ESP.getMinFreeHeap());
        }

        return true;
    }

    // Grenzen des aktuellen "Kurven-Tags" fuer die Seite "Tagesverlauf" -
    // der Tag beginnt erst um Config::DAILY_CURVE_START_HOUR, nicht um
    // Mitternacht (Auftrag): vor dieser Uhrzeit lokal gehoert "jetzt" noch
    // zum VORTag, dessen fertige Kurve bis zum naechsten Morgen stehen
    // bleibt. outDayKey muss mindestens 11 Byte gross sein.
    void curveDayBounds(char* outDayKey, size_t keySize, time_t* outStart, time_t* outEnd) {
        time_t now = time(nullptr);
        struct tm nowTm;
        localtime_r(&now, &nowTm);
        struct tm dayTm = nowTm;
        // Seit Auftrag "Tagesfenster mit Nachtmodus": der Kurven-Tag
        // wechselt schon um DAY_START_HOUR:DAY_START_MINUTE (04:30), nicht
        // erst um DAILY_CURVE_START_HOUR (05:00) - die Kurve startet damit
        // zeitgleich mit dem neuen Ertragsbuch-Tag wieder leer. Die
        // tatsaechlich dargestellte Achse (05-22 Uhr, siehe
        // daily_curve_page.cpp) bleibt davon unberuehrt, da start/end
        // weiterhin ueber DAILY_CURVE_START_HOUR/END_HOUR berechnet werden.
        if (DayWindow::minutesOfDay(nowTm) < (uint16_t)(Config::DAY_START_HOUR * 60 + Config::DAY_START_MINUTE)) {
            time_t yesterday = now - 24L * 3600L;
            localtime_r(&yesterday, &dayTm);
        }
        dayTm.tm_hour = Config::DAILY_CURVE_START_HOUR;
        dayTm.tm_min = 0;
        dayTm.tm_sec = 0;
        time_t start = mktime(&dayTm);
        struct tm endTm = dayTm;
        endTm.tm_hour = Config::DAILY_CURVE_END_HOUR;
        time_t end = mktime(&endTm);
        strftime(outDayKey, keySize, "%Y-%m-%d", &dayTm);
        *outStart = start;
        *outEnd = end;
    }

    // Reicht ein watts-Wert in den gueltigen Bereich fuer
    // SolarData::DailyCurvePoint (uint16, nie negativ).
    uint16_t clampWattsU16(float w) {
        if (w < 0.0f) return 0;
        if (w > 65535.0f) return 65535;
        return (uint16_t)w;
    }

    // Einmaliges Nachladen des bisherigen Tagesverlaufs (Seite
    // "Tagesverlauf") - beim Start (lastBackfilledCurveDayKey ist dann
    // leer) und bei jedem Wechsel des Kurven-Tags (05:00). Danach
    // schreibt die regulaere 3h-Kurvenabfrage die Kurve fort, siehe
    // runPowerCurveFetch() unten - KEINE weiteren Nachlade-Anfragen.
    // Teilt das Gesamtfenster in Haeppchen von
    // Config::DAILY_CURVE_CHUNK_SECONDS auf (Auftrag "zu grosse Antwort
    // aufteilen"). Fehlgeschlagene Haeppchen bleiben Luecken in der
    // Kurve, es wird nichts erfunden.
    void runDailyCurveBackfillIfNeeded() {
        char dayKey[11];
        time_t dayStart, dayEnd;
        curveDayBounds(dayKey, sizeof(dayKey), &dayStart, &dayEnd);
        if (strcmp(dayKey, lastBackfilledCurveDayKey) == 0) return;

        time_t now = time(nullptr);
        time_t fetchEnd = (now < dayEnd) ? now : dayEnd;
        if (fetchEnd <= dayStart) {
            // Direkt nach dem 05:00-Wechsel gibt es noch nichts
            // nachzuladen - Kurve fuer den neuen Tag leeren, sie waechst
            // ab jetzt ganz normal mit den regulaeren Abfragen.
            SolarData::setDailyCurve(nullptr, 0, dayKey);
            strncpy(lastBackfilledCurveDayKey, dayKey, sizeof(lastBackfilledCurveDayKey) - 1);
            return;
        }

        uint32_t heapBefore = ESP.getFreeHeap();

        SolarData::DailyCurvePoint buf[SolarData::DAILY_CURVE_MAX_POINTS];
        uint16_t total = 0;
        bool anyChunkFailed = false;
        for (time_t chunkStart = dayStart; chunkStart < fetchEnd && total < SolarData::DAILY_CURVE_MAX_POINTS;
             chunkStart += (time_t)Config::DAILY_CURVE_CHUNK_SECONDS) {
            time_t chunkEnd = chunkStart + (time_t)Config::DAILY_CURVE_CHUNK_SECONDS;
            if (chunkEnd > fetchEnd) chunkEnd = fetchEnd;

            EpeverClient::PowerCurvePoint pts[EpeverClient::POWER_CURVE_MAX_POINTS];
            uint8_t n = 0;
            int httpCode = 0, apiStatus = 0;
            bool ok = EpeverClient::fetchPowerCurveWindow(currentDeviceNo, currentToken,
                          (int64_t)chunkStart * 1000LL, (int64_t)chunkEnd * 1000LL,
                          EpeverClient::POWER_CURVE_MAX_POINTS, pts, n, &httpCode, &apiStatus);
            if (!ok && (httpCode == 401 || apiStatus != 0)) {
                currentToken = "";
                if (EpeverClient::login(currentEmail, currentPassword, currentToken)) {
                    needsLogin = false;
                    ok = EpeverClient::fetchPowerCurveWindow(currentDeviceNo, currentToken,
                              (int64_t)chunkStart * 1000LL, (int64_t)chunkEnd * 1000LL,
                              EpeverClient::POWER_CURVE_MAX_POINTS, pts, n, &httpCode, &apiStatus);
                } else {
                    needsLogin = true;
                }
            }
            if (!ok) { anyChunkFailed = true; continue; }

            // pts ist neuester zuerst - rueckwaerts durchlaufen fuer
            // aufsteigende Zeit, nur Punkte innerhalb des Anzeigefensters
            // und strikt neuer als der letzte uebernommene (monoton).
            for (int8_t i = (int8_t)n - 1; i >= 0 && total < SolarData::DAILY_CURVE_MAX_POINTS; i--) {
                struct tm pt;
                localtime_r(&pts[i].time, &pt);
                if (pt.tm_hour < Config::DAILY_CURVE_START_HOUR || pt.tm_hour >= Config::DAILY_CURVE_END_HOUR) continue;
                uint16_t minutes = (uint16_t)(pt.tm_hour * 60 + pt.tm_min);
                if (total > 0 && minutes <= buf[total - 1].minutesSinceMidnight) continue;
                buf[total].minutesSinceMidnight = minutes;
                buf[total].watts = clampWattsU16(pts[i].watts);
                total++;
            }
        }

        SolarData::setDailyCurve(buf, total, dayKey);
        strncpy(lastBackfilledCurveDayKey, dayKey, sizeof(lastBackfilledCurveDayKey) - 1);

        uint32_t heapAfter = ESP.getFreeHeap();
        Serial.printf("[fetch] Tagesverlauf nachgeladen: %u Punkte, Luecken=%s, freeHeap vorher=%u nachher=%u\n",
                      (unsigned)total, anyChunkFailed ? "ja" : "nein", (unsigned)heapBefore, (unsigned)heapAfter);
    }

    // Leistungs-Kurvenabfrage (Seite "Leistung") - eigene Anfrage mit
    // grossem sampleLimit, gleiche Login-/Wiederholungsregeln wie die
    // Batterieabfrage. points[0] der Antwort (neuester Punkt zuerst) ist
    // zugleich die aktuelle Leistung; SolarData::setHistory() braucht
    // aeltesten zuerst, daher wird hier umsortiert.
    bool runPowerCurveFetch() {
        EpeverClient::PowerCurvePoint points[EpeverClient::POWER_CURVE_MAX_POINTS];
        uint8_t count = 0;
        int httpCode = 0, apiStatus = 0;
        bool ok = EpeverClient::fetchPowerCurve(currentDeviceNo, currentToken, points, count, &httpCode, &apiStatus);

        if (!ok && (httpCode == 401 || apiStatus != 0)) {
            currentToken = "";
            if (EpeverClient::login(currentEmail, currentPassword, currentToken)) {
                needsLogin = false;
                ok = EpeverClient::fetchPowerCurve(currentDeviceNo, currentToken, points, count, &httpCode, &apiStatus);
            } else {
                needsLogin = true;
            }
        }

        Serial.printf("[fetch] Leistungskurve: HTTP-Code=%d status=%d Punkte=%u\n", httpCode, apiStatus, (unsigned)count);
        if (count > 0) {
            // points[] ist neuester zuerst (API-Reihenfolge) - points[0]
            // ist damit der juengste, points[count-1] der aelteste
            // Zeitstempel im 3-Stunden-Fenster (Auftrag: erster/letzter
            // Zeitstempel der Kurve mitloggen).
            struct tm tOld, tNew;
            localtime_r(&points[count - 1].time, &tOld);
            localtime_r(&points[0].time, &tNew);
            Serial.printf("[fetch] Leistungskurve Zeitfenster: %02d:%02d bis %02d:%02d (%u Punkte)\n",
                          tOld.tm_hour, tOld.tm_min, tNew.tm_hour, tNew.tm_min, (unsigned)count);
        }

        if (!ok) {
            if (apiStatus == 5004) tryResolveEquipmentList();
            return false;
        }

        if (count > 0) {
            SolarData::setPower(points[0].watts, points[0].time);

            SolarData::HistoryPoint oldestFirst[EpeverClient::POWER_CURVE_MAX_POINTS];
            for (uint8_t i = 0; i < count; i++) {
                oldestFirst[i].epoch = points[count - 1 - i].time;
                oldestFirst[i].valueW = points[count - 1 - i].watts;
            }
            SolarData::setHistory(oldestFirst, count);

            // Tagesverlauf fortschreiben (Seite "Tagesverlauf") - OHNE
            // Zusatzabfrage: der neueste Punkt dieser ohnehin laufenden
            // 3h-Kurve wird an die Tageskurve angehaengt, sofern er in
            // deren Anzeigefenster faellt und zum aktuell gespeicherten
            // Kurven-Tag passt (SolarData::appendDailyCurvePoint() ist
            // sonst ein No-Op, z.B. kurz waehrend eines Tageswechsels).
            struct tm pt;
            localtime_r(&points[0].time, &pt);
            if (pt.tm_hour >= Config::DAILY_CURVE_START_HOUR && pt.tm_hour < Config::DAILY_CURVE_END_HOUR) {
                char dayKey[11];
                time_t dsUnused, deUnused;
                curveDayBounds(dayKey, sizeof(dayKey), &dsUnused, &deUnused);
                uint16_t minutes = (uint16_t)(pt.tm_hour * 60 + pt.tm_min);
                SolarData::appendDailyCurvePoint(minutes, clampWattsU16(points[0].watts), dayKey);
            }
        }

        if (!firstPowerCurveHeapLogged) {
            firstPowerCurveHeapLogged = true;
            Serial.printf("[fetch] minFreeHeap waehrend erster Kurvenabfrage: %u\n",
                          (unsigned)ESP.getMinFreeHeap());
        }

        return true;
    }

    // Holt die Geraeteliste und wendet genau eins von drei Ergebnissen an
    // (siehe Auftrag "Seriennummer automatisch ermitteln"):
    // - genau 1 Geraet -> an Core 1 melden (DeviceNoResolution), dort
    //   uebernommen, falls abweichend
    // - mehrere Geraete, gespeicherte Nummer passt zu keinem -> nur
    //   loggen, NICHT raten
    // - 0 Geraete oder Fehler -> nichts tun
    // Hoechstens alle Config::EQUIPMENT_LIST_MIN_INTERVAL_MS tatsaechlich
    // abgefragt, unabhaengig davon, wie oft diese Funktion aufgerufen
    // wird (Auftrag "Portal nicht belasten").
    void tryResolveEquipmentList() {
        uint32_t now = millis();
        if (lastEquipmentListAttemptMs != 0 && now - lastEquipmentListAttemptMs < Config::EQUIPMENT_LIST_MIN_INTERVAL_MS) return;
        lastEquipmentListAttemptMs = now;

        if (currentToken.length() == 0) return;

        EpeverClient::EquipmentListResult result;
        int httpCode = -1, apiStatus = -1;
        bool ok = EpeverClient::fetchEquipmentList(currentToken, result, &httpCode, &apiStatus);
        if (!ok) return; // bereits in epever_client.cpp geloggt (nur Codes, keine Inhalte)

        if (result.total == 1 && result.count >= 1) {
            DeviceNoResolution::setResult(result.gatewayIds[0]);
        } else if (result.total > 1) {
            bool matches = false;
            for (uint8_t i = 0; i < result.count; i++) {
                if (result.gatewayIds[i] == currentDeviceNo) { matches = true; break; }
            }
            if (!matches) {
                Serial.println("[fetch] Geraeteliste: mehrere Geraete, gespeicherte Nummer passt zu keinem - keine automatische Korrektur");
            }
        }
        // total == 0: nichts zu tun, Zustand bleibt wie bisher.
    }

    // Login + Geraeteliste, OHNE Ertrag/Batterie/Leistungskurve - laeuft
    // anstelle von runFetchCycle(), solange keine Seriennummer bekannt
    // ist (Auftrag 2a: "nach jedem erfolgreichen Login, wenn keine
    // Seriennummer gespeichert ist").
    void runDeviceNoOnlyCycle() {
        if (needsLogin || currentToken.length() == 0) {
            SolarData::setFetchStatus(SolarData::FetchStatus::LoggingIn);
            if (!EpeverClient::login(currentEmail, currentPassword, currentToken)) {
                consecutiveFailures++;
                SolarData::setFetchStatus(SolarData::FetchStatus::LoginFailed);
                return;
            }
            needsLogin = false;
        }
        consecutiveFailures = 0;
        SolarData::setFetchStatus(SolarData::FetchStatus::NoDeviceNo);
        tryResolveEquipmentList();
    }

    // Ein kompletter Abfrage-Zyklus: bei Bedarf einloggen, dann Ertrag
    // abfragen. Schlaegt die Ertragsabfrage fehl, wird genau einmal neu
    // eingeloggt und wiederholt (siehe Auftrag). Blockiert waehrend der
    // HTTPS-Aufrufe, aber nur den Fetch-Task (Core 0) - Core 1 wartet nie
    // darauf.
    void runFetchCycle(const int32_t* activeReadDataPointIds = nullptr, uint8_t activeReadCount = 0) {
        if (needsLogin || currentToken.length() == 0) {
            SolarData::setFetchStatus(SolarData::FetchStatus::LoggingIn);
            if (!EpeverClient::login(currentEmail, currentPassword, currentToken)) {
                consecutiveFailures++;
                SolarData::setFetchStatus(SolarData::FetchStatus::LoginFailed);
                return;
            }
            needsLogin = false;
            // Neuer Login -> gespeicherte Seriennummer gegen die
            // Geraeteliste pruefen (Auftrag 2b, intern gedrosselt).
            tryResolveEquipmentList();
        }

        float today = 0, month = 0, year = 0, total = 0;
        bool ok = EpeverClient::fetchEnergy(currentToken, today, month, year, total);

        if (!ok) {
            // Einmal neu einloggen und die Abfrage wiederholen.
            currentToken = "";
            if (EpeverClient::login(currentEmail, currentPassword, currentToken)) {
                needsLogin = false;
                tryResolveEquipmentList(); // ebenfalls ein neuer Login
                ok = EpeverClient::fetchEnergy(currentToken, today, month, year, total);
            } else {
                needsLogin = true;
            }
        }

        if (!ok) {
            consecutiveFailures++;
            SolarData::setFetchStatus(SolarData::FetchStatus::NetworkError);
            return;
        }

        consecutiveFailures = 0;
        SolarData::setEnergy(today, month, year, total);
        SolarData::setLastSuccessfulFetch(time(nullptr));
        SolarData::setFetchStatus(SolarData::FetchStatus::Ok);

        if (!firstFetchHeapLogged) {
            firstFetchHeapLogged = true;
            Serial.printf("[fetch] minFreeHeap nach erster erfolgreicher Abfrage: %u\n",
                          (unsigned)ESP.getMinFreeHeap());
        }

        // Tagesertrag-Quelle fuer das Ertragsbuch (Core 1, siehe
        // yield_ledger.h): primaer der Datenpunkt "Daily Generation" aus
        // der Batterieabfrage, sonst der "Heute"-Wert von eben.
        float dailyFromDatapoint = 0.0f;
        bool dailyFromDatapointValid = false;
        runBatteryFetch(dailyFromDatapoint, dailyFromDatapointValid, activeReadDataPointIds, activeReadCount);
        runPowerCurveFetch();
        runDailyCurveBackfillIfNeeded();

        // Rohwerte dieser Abfrage gesammelt loggen (Auftrag "Seite
        // Leistung" Teil D) - keine Zugangsdaten, nur Messwerte.
        {
            SolarData::Data snap = SolarData::snapshot();
            Serial.printf("[fetch] Rohwerte: Leistung=%.0fW Spannung=%.1fV Strom=%.2fA\n",
                          (double)snap.currentPowerW, (double)snap.pvVoltage, (double)snap.pvCurrent);
        }

        if (dailyFromDatapointValid) {
            SolarData::setDailySource(dailyFromDatapoint, time(nullptr), true);
        } else {
            SolarData::setDailySource(today, time(nullptr), false);
        }
    }

    void handleCommand(Command cmd, const RefreshRequest& refresh) {
        switch (cmd) {
            case Command::ReconnectNow:
                manualDisconnect = false;
                WifiConnectManager::begin();
                break;
            case Command::DisconnectWifi:
                manualDisconnect = true;
                WiFi.disconnect(true, true);
                WifiStatus::set(false, 255);
                break;
            case Command::ConnectSlot0:
                manualDisconnect = false;
                WifiConnectManager::connectSlotNow(0);
                break;
            case Command::ConnectSlot1:
                manualDisconnect = false;
                WifiConnectManager::connectSlotNow(1);
                break;
            case Command::ConnectSlot2:
                manualDisconnect = false;
                WifiConnectManager::connectSlotNow(2);
                break;
            case Command::AccountChanged:
                applyAccountSnapshot();
                break;
            case Command::LocationChanged:
                applyLocationSnapshot();
                break;
            case Command::LanguageChanged:
                // Noch kein Effekt (betrifft nur Core 1, Sprache steckt
                // nicht in den API-Antworten) - Befehl bleibt fuer
                // spaetere Features verdrahtet.
                break;
            case Command::RefreshNow: {
                // Auftrag "Refresh-Regeln und Rueckmeldung": Reihenfolge
                // der Pruefungen wie vorgegeben - Tagesfenster/Nachtmodus
                // vor WLAN vor laufendem Abruf vor Mindestabstand. NIE
                // forceFetchNow setzen, wenn gesperrt - ein gesperrter
                // Tipp wird nicht vorgemerkt und faellt nicht
                // nachtraeglich beim naechsten erlaubten Zeitpunkt an
                // (vorheriges Verhalten, siehe Auftrag-Korrektur).
                if (!hasAccount) break; // kein Konto - wie bisher stillschweigend ignoriert
                uint32_t now = millis();
                struct tm nowTm;
                bool hasValidTime = getLocalTime(&nowTm, 5);
                if (!hasDeviceNo) {
                    // Auftrag "Seriennummer-Quelle fuer das Aktiv-
                    // Auslesen", Punkt 2: klare, eigene Meldung statt des
                    // sonst irrefuehrenden "Keine neuen Daten" (der Abruf
                    // liefe sonst still ueber runDeviceNoOnlyCycle() ins
                    // Leere, ohne dass je ein Befehl gesendet wird). Ruft
                    // dabei KEINE zusaetzliche Geraeteliste-Abfrage ab -
                    // die laeuft bereits unabhaengig vom Tipp im
                    // Hintergrund (siehe tryResolveEquipmentList()).
                    SolarData::setRefreshFeedback(SolarData::RefreshFeedback::NoDeviceNo);
                } else if (!hasValidTime || !DayWindow::isDayWindow(nowTm)) {
                    SolarData::setRefreshFeedback(SolarData::RefreshFeedback::LockedNight);
                } else if (WiFi.status() != WL_CONNECTED) {
                    SolarData::setRefreshFeedback(SolarData::RefreshFeedback::NoWifi);
                } else if (fetchInProgress) {
                    SolarData::setRefreshFeedback(SolarData::RefreshFeedback::Busy);
                } else if (now - lastManualRefreshMs < Config::EPEVER_MANUAL_REFRESH_MIN_INTERVAL_MS) {
                    SolarData::setRefreshFeedback(SolarData::RefreshFeedback::Cooldown);
                } else if (!ActiveReadLimiter::allowTap(activeReadLimiterState, now, refresh.dataPointCount)) {
                    // Schutz gegen Dauer-Tippen beim Aktiv-Auslesen
                    // (Auftrag, Korrektur B3: zaehlt BEFEHLE - refresh.
                    // dataPointCount - nicht Tipps) - Mindestabstand/
                    // Stunden-/Tagesobergrenze, ZUSAETZLICH zur 30s-Sperre
                    // oben. Im Sperrfenster wird KEIN Befehl an den
                    // Regler gesendet, nur eine kurze Rueckmeldung gezeigt
                    // (dieselbe Meldung wie bei der 30s-Sperre - kein
                    // neuer Text noetig).
                    SolarData::setRefreshFeedback(SolarData::RefreshFeedback::Cooldown);
                } else {
                    lastManualRefreshMs = now;
                    pendingActiveRead = refresh;
                    // Die Operationsprotokoll-Eintraege entstehen schon
                    // mit dem SENDEN der Befehle (nicht erst bei Erfolg) -
                    // deshalb hier und nicht erst nach runFetchCycle().
                    if (refresh.dataPointCount > 0) {
                        ActiveReadLimiter::recordTap(activeReadLimiterState, now, refresh.dataPointCount);
                    }
                    forceFetchNow = true;
                    manualRefreshPending = true;
                    SolarData::setRefreshFeedback(refresh.dataPointCount > 0
                        ? SolarData::RefreshFeedback::WorkingActiveRead
                        : SolarData::RefreshFeedback::Working);
                }
                break;
            }
        }
    }

    void taskFn(void*) {
        Serial.printf("[core] Fetch-Task laeuft auf Core %d\n", xPortGetCoreID());

        WifiConnectManager::begin();
        applyAccountSnapshot();
        applyLocationSnapshot();

        uint32_t lastHeapLogMs = millis();

        for (;;) {
            if (pauseRequested) {
                pausedAck = true;
                // Absichtlich pausiert (z.B. WLAN-Scan/Verbindungstest auf
                // Core 1, siehe FetchTask::pause()) ist kein Haengenbleiben
                // - Herzschlag trotzdem weitergeben, sonst koennte der
                // Watchdog einen laengeren, ganz normalen Scan faelschlich
                // als Haenger werten.
                FetchHeartbeat::beat();
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            pausedAck = false;

            QueueItem item;
            while (xQueueReceive(commandQueue, &item, 0) == pdTRUE) {
                handleCommand(item.cmd, item.refresh);
            }

            if (!manualDisconnect) WifiConnectManager::tick();

            bool wifiConnected = (WiFi.status() == WL_CONNECTED);
            bool justConnected = wifiConnected && !wifiWasConnected;
            if (hasAccount && wifiConnected) {
                uint32_t now = millis();
                time_t nowEpoch = time(nullptr);
                struct tm nowTm;
                // Ohne gueltige NTP-Zeit gibt es KEINEN Nachtmodus (Auftrag
                // 3g) - getLocalTime() schlaegt dann fehl, hasValidTime
                // bleibt false, der Rest verhaelt sich wie vorher (immer
                // Tagesfenster-Takt).
                bool hasValidTime = getLocalTime(&nowTm, 5);
                bool inNightWindow = hasValidTime && DayWindow::isNightWindow(nowTm);
                bool inFinalQueryWindow = hasValidTime && DayWindow::isFinalQueryWindow(nowTm);

                char eveningDate[11] = {0};
                if (hasValidTime) DayWindow::currentNightEveningDate(nowEpoch, nowTm, eveningDate, sizeof(eveningDate));
                bool finalQueryAlreadyDoneTonight = hasValidTime && finalQueryDoneDate[0] &&
                                                     strcmp(finalQueryDoneDate, eveningDate) == 0;

                bool allowFetch = !inNightWindow && !(inFinalQueryWindow && finalQueryAlreadyDoneTonight);

#ifdef NIGHT_MODE_DIAG_TEST
                // Logzeilen (Auftrag): Eintritt/Ende des Nachtmodus, je
                // nur einmal pro Uebergang.
                if (hasValidTime) {
                    if (inNightWindow && !nightPauseLogged) {
                        nightPauseLogged = true;
                        char hhmm[6];
                        struct tm nt; localtime_r(&nowEpoch, &nt);
                        snprintf(hhmm, sizeof(hhmm), "%02d:%02d", nt.tm_hour, nt.tm_min);
                        Serial.printf("[fetch] Nachtmodus-Pause beginnt um %s - keine Abfragen bis 04:30\n", hhmm);
                    } else if (!inNightWindow && nightPauseLogged) {
                        nightPauseLogged = false;
                        Serial.println("[fetch] Nachtmodus Ende - Abfragen ab 04:30 wieder aktiv");
                    }
                }

                if (inFinalQueryWindow && !finalQueryAlreadyDoneTonight &&
                    (!finalQueryStartLoggedDate[0] || strcmp(finalQueryStartLoggedDate, eveningDate) != 0)) {
                    strncpy(finalQueryStartLoggedDate, eveningDate, sizeof(finalQueryStartLoggedDate) - 1);
                    Serial.println("[fetch] Abschlussabfrage (ab 22:30) beginnt");
                }
#endif

                if (allowFetch) {
                    uint32_t interval;
                    if (inFinalQueryWindow) {
                        // Abschlussabfrage (22:30-22:55): schnellerer Takt
                        // als normal, unabhaengig vom Kurzfehler-Zaehler.
                        interval = Config::FINAL_QUERY_RETRY_INTERVAL_MS;
                    } else {
                        interval = (consecutiveFailures >= 1 && consecutiveFailures < Config::EPEVER_MAX_SHORT_RETRIES)
                                   ? Config::EPEVER_RETRY_INTERVAL_MS
                                   : Config::EPEVER_FETCH_INTERVAL_MS;
                    }
                    bool due = forceFetchNow || justConnected || lastFetchAttemptMs == 0 ||
                               (now - lastFetchAttemptMs >= interval);
                    if (due) {
#ifdef NIGHT_MODE_DIAG_TEST
                        // Auftrag "Diagnosezeilen fuer die naechste Nacht" -
                        // welches Intervall (in Sekunden) diesen Abruf
                        // ausgeloest hat, inkl. Grund (manuell/gerade erst
                        // verbunden/Intervall abgelaufen).
                        const char* dueReason = forceFetchNow ? "manueller Refresh"
                                                : justConnected ? "WLAN gerade verbunden"
                                                : lastFetchAttemptMs == 0 ? "noch nie/Neustart"
                                                : "Intervall abgelaufen";
                        Serial.printf("[fetch] Abruf startet - Intervall=%lus, Grund=%s\n",
                                      (unsigned long)(interval / 1000), dueReason);
#endif
                        lastFetchAttemptMs = now;
                        // NUR bei plausiblem Datum uebernehmen (siehe
                        // epochPlausible() oben) - sonst (z.B. ein Abruf
                        // in den ersten Sekunden nach dem Boot, bevor NTP
                        // synchronisiert ist) wuerde ein winziger/
                        // unplausibler Epoch-Wert (nahe 1970) haengen
                        // bleiben und "Naechste Abfrage" bis zum naechsten
                        // echten Abruf (5 Minuten) einen kompletten
                        // Unsinnswert wie "01:05" anzeigen - live am
                        // Geraet beobachtet, direkt nach dem Flashen.
                        // BEWUSST nicht auf hasValidTime gestuetzt (5ms-
                        // Timeout, kann auch lange nach echter NTP-
                        // Synchronisation spontan/voruebergehend false
                        // liefern - ebenfalls live beobachtet, blieb dann
                        // minutenlang im "--:--"/nowEpoch-Fallback haengen
                        // statt den stabilen Wert zu nutzen).
                        // candidateNextAt faellt bei Unplausibilitaet
                        // unten auf den bestehenden "--:--"-Pfad zurueck.
                        if (epochPlausible(nowEpoch)) lastFetchAttemptEpoch = nowEpoch;
                        bool wasManualRefresh = manualRefreshPending;
                        RefreshRequest activeReadForThisCycle = wasManualRefresh ? pendingActiveRead : RefreshRequest();
                        forceFetchNow = false;
                        manualRefreshPending = false;
                        pendingActiveRead = RefreshRequest();
                        fetchInProgress = true;
                        uint32_t changeCounterBefore = wasManualRefresh ? SolarData::snapshot().changeCounter : 0;
                        if (hasDeviceNo) {
                            // activeReadForThisCycle steuert das Aktiv-
                            // Auslesen (Auftrag) - der geplante Takt
                            // (wasManualRefresh == false) bleibt immer
                            // beim passiven Abruf (dataPointCount == 0).
                            runFetchCycle(activeReadForThisCycle.dataPointIds, activeReadForThisCycle.dataPointCount);
                        } else {
                            runDeviceNoOnlyCycle();
                        }
                        fetchInProgress = false;
                        // Rueckmeldung fuer einen manuellen Refresh-Tipp
                        // (Auftrag) - "Done" nur, wenn sich dabei
                        // TATSAECHLICH ein angezeigter Wert geaendert hat
                        // (changeCounter steigt nur bei echter Aenderung,
                        // siehe SolarData::mutate()), sonst "NoChange".
                        if (wasManualRefresh) {
                            uint32_t changeCounterAfter = SolarData::snapshot().changeCounter;
                            SolarData::setRefreshFeedback(changeCounterAfter != changeCounterBefore
                                ? SolarData::RefreshFeedback::Done
                                : SolarData::RefreshFeedback::NoChange);
                        }
                        // consecutiveFailures==0 nach dem Versuch heisst
                        // "erfolgreich" (jeder Fehlschlag zaehlt ihn hoch,
                        // siehe runFetchCycle()) - Abschlussabfrage dieses
                        // Abends ist damit erledigt, siehe Auftrag 3c/3d.
                        if (inFinalQueryWindow && consecutiveFailures == 0) {
                            strncpy(finalQueryDoneDate, eveningDate, sizeof(finalQueryDoneDate) - 1);
                            SolarData::setFinalQueryDoneDate(eveningDate);
#ifdef NIGHT_MODE_DIAG_TEST
                            SolarData::Data s = SolarData::snapshot();
                            char hhmm[6];
                            struct tm nt; localtime_r(&nowEpoch, &nt);
                            snprintf(hhmm, sizeof(hhmm), "%02d:%02d", nt.tm_hour, nt.tm_min);
                            Serial.printf("[fetch] Abschlussabfrage erfolgreich: %.2f kWh um %s\n",
                                          (double)s.dailySourceKwh, hhmm);
#endif
                        }
                    }
                }

                // Geplante naechste Abfrage fuer die Anzeige (Auftrag
                // "Naechste Abfrage" statt der doppelten "Messung"-Zeile,
                // siehe power_page.cpp/battery_page.cpp) - NACH dem
                // eigentlichen Versuch neu berechnen, da runFetchCycle()
                // consecutiveFailures aendern kann (Normal-/Retry-Takt).
                // lastFetchAttemptMs steht zu diesem Zeitpunkt schon auf
                // dem Stand des (ggf. gerade eben erfolgten) letzten
                // Versuchs - die Formel gilt damit fuer beide Faelle
                // (gerade abgefragt oder noch wartend) gleich.
                {
                    time_t candidateNextAt;
                    if (!allowFetch) {
                        // Nachtmodus (oder Abschlussabfrage dieses Abends
                        // schon erledigt) - naechste Abfrage ist erst der
                        // naechste Tagesbeginn.
                        candidateNextAt = hasValidTime ? DayWindow::nextDayStartEpoch(nowEpoch) : 0;
                    } else {
                        uint32_t nextInterval;
                        if (inFinalQueryWindow) {
                            nextInterval = Config::FINAL_QUERY_RETRY_INTERVAL_MS;
                        } else {
                            nextInterval = (consecutiveFailures >= 1 && consecutiveFailures < Config::EPEVER_MAX_SHORT_RETRIES)
                                           ? Config::EPEVER_RETRY_INTERVAL_MS
                                           : Config::EPEVER_FETCH_INTERVAL_MS;
                        }
                        // Komplett in der time()-Domaene gerechnet (siehe
                        // lastFetchAttemptEpoch-Kommentar oben) - haengt
                        // NICHT mehr von "jetzt" (now/nowEpoch) ab, bleibt
                        // also zwischen zwei echten Abrufen vollkommen
                        // stabil, keine +-1s-Rundungsjitter mehr moeglich.
                        if (lastFetchAttemptEpoch > 0) {
                            candidateNextAt = lastFetchAttemptEpoch + (time_t)(nextInterval / 1000);
                        } else if (epochPlausible(nowEpoch)) {
                            candidateNextAt = nowEpoch + (time_t)(nextInterval / 1000);
                        } else {
                            // Weder ein frueherer gueltiger Abruf noch
                            // aktuell gueltige NTP-Zeit - lieber "--:--"
                            // zeigen (candidateNextAt<=0, siehe Anzeige-
                            // Seiten) als einen erfundenen Unsinnswert.
                            candidateNextAt = 0;
                        }
                    }
                    // NUR bei echter Aenderung schreiben - SolarData::
                    // setNextFetchAt() erhoeht IMMER changeCounter, auch
                    // wenn der Wert gleich bleibt. Ohne diese Bremse lief
                    // das bei JEDEM Tick (alle ~200ms), liess alle Seiten,
                    // die SolarData lesen, im selben Takt flackern - war
                    // die Hauptursache des gemeldeten Flackerns (siehe
                    // CLAUDE.md "Nur bei echter Aenderung neu zeichnen").
                    if (candidateNextAt != lastSentNextFetchAt) {
                        lastSentNextFetchAt = candidateNextAt;
                        SolarData::setNextFetchAt(candidateNextAt);
#ifdef NIGHT_MODE_DIAG_TEST
                        // Auftrag "Diagnosezeilen fuer die naechste Nacht" -
                        // Planung des naechsten Abrufs mit Grund, besonders
                        // um 04:30 wichtig ("Tagesfenster beginnt...").
                        char nextHhmm[6] = "--:--";
                        if (candidateNextAt > 0) {
                            struct tm nt; localtime_r(&candidateNextAt, &nt);
                            snprintf(nextHhmm, sizeof(nextHhmm), "%02d:%02d", nt.tm_hour, nt.tm_min);
                        }
                        const char* reason = !allowFetch ? "Nachtsperre/Abschluss erledigt - Tagesbeginn"
                                              : inFinalQueryWindow ? "Abschlussfenster-Wiederholung"
                                              : (consecutiveFailures >= 1 && consecutiveFailures < Config::EPEVER_MAX_SHORT_RETRIES) ? "Kurzfehler-Wiederholung"
                                              : "normaler Takt";
                        long inSec = candidateNextAt > 0 ? (long)(candidateNextAt - nowEpoch) : -1;
                        Serial.printf("[fetch] Naechster Abruf geplant: %s (in %lds, Grund: %s)\n", nextHhmm, inSec, reason);
#endif
                    }
                }

                // Naechtlicher Batterie-Abruf (Auftrag "Batterie auch
                // nachts abfragen", KORRIGIERT auf Alex' Hinweis: die
                // Bestaetigung lebt NUR im RAM und geht bei jedem
                // Neustart verloren - ein Neustart mitten in der Nacht,
                // z.B. 00:30 oder 03:00 Uhr, darf den Abruf NICHT bis
                // zum naechsten Tag blockieren). Zwei Faelle:
                // - Ab 22:55 (inNightWindow): IMMER erlaubt, UNABHAENGIG
                //   von der Bestaetigung - erfasst auch einen Neustart
                //   mitten in der Nacht sofort.
                // - Vor 22:55, also noch im Abschlussfenster
                //   (inFinalQueryWindow, 22:30-22:55): weiterhin NUR nach
                //   bestaetigter Abschlussabfrage/Sicherung (startet dann
                //   ggf. schon VOR 22:55, sobald bestaetigt).
                // Komplett getrennt vom "allowFetch"-Pfad: runFetchCycle()
                // (Ertrag/Leistungskurve/Tageskurve/Ertragsbuch/SD-
                // Schreibzugriffe) wird hier NIE aufgerufen, nur der
                // reine, passive runBatteryFetch() (useActiveRead=false -
                // NIE ein aktives Auslesen nachts). allowFetch und dieses
                // Gate schliessen sich nachweislich (siehe
                // test_night_battery) immer gegenseitig aus - beide
                // Bloecke laufen nie im selben Tick.
                bool nightBatteryGate = inNightWindow || (inFinalQueryWindow && finalQueryAlreadyDoneTonight);
                if (nightBatteryGate) {
                    bool sameNight = lastNightBatteryDate[0] && strcmp(lastNightBatteryDate, eveningDate) == 0;
                    bool nightBatteryDue = !sameNight ||
                        (now - lastNightBatteryFetchMs >= Config::EPEVER_NIGHT_BATTERY_INTERVAL_MS);
                    if (nightBatteryDue) {
                        strncpy(lastNightBatteryDate, eveningDate, sizeof(lastNightBatteryDate) - 1);
                        lastNightBatteryDate[sizeof(lastNightBatteryDate) - 1] = 0;
                        lastNightBatteryFetchMs = now;
                        if (epochPlausible(nowEpoch)) lastNightBatteryFetchEpoch = nowEpoch;
                        float unusedDailyGenKwh = 0.0f;
                        bool unusedDailyGenValid = false;
                        bool ok = runBatteryFetch(unusedDailyGenKwh, unusedDailyGenValid, nullptr, 0, /*updateNonBatteryFields=*/false);
                        Serial.printf("[fetch] Naechtlicher Batterieabruf: ok=%d\n", ok ? 1 : 0);
                    }

                    // Gleiche stabile time()-Domaene-Berechnung wie
                    // candidateNextAt oben (siehe lastFetchAttemptEpoch-
                    // Kommentar) - eigenes Feld, zeigt NUR auf der
                    // Batterieseite statt "04:30 Uhr" (Auftrag, siehe
                    // battery_page.cpp/solar_data.h).
                    time_t nightCandidateNextAt = lastNightBatteryFetchEpoch > 0
                        ? lastNightBatteryFetchEpoch + (time_t)(Config::EPEVER_NIGHT_BATTERY_INTERVAL_MS / 1000)
                        : 0;
                    if (nightCandidateNextAt != lastSentNightBatteryNextFetchAt) {
                        lastSentNightBatteryNextFetchAt = nightCandidateNextAt;
                        SolarData::setNightBatteryNextFetchAt(nightCandidateNextAt);
                    }
                } else if (lastSentNightBatteryNextFetchAt != 0) {
                    // Tag, oder Nacht aber Abschlussabfrage/Sicherung noch
                    // nicht bestaetigt - Feld zuruecksetzen, sonst wuerde
                    // battery_page.cpp eine veraltete naechtliche Zeit
                    // zeigen statt korrekt auf nextFetchAt zurueckzufallen.
                    lastSentNightBatteryNextFetchAt = 0;
                    SolarData::setNightBatteryNextFetchAt(0);
                    lastNightBatteryFetchEpoch = 0;
                }
            }

            // Wetter: unabhaengig vom Solar-Guardian-Konto, laeuft sobald
            // ein Standort gewaehlt ist und WLAN steht. Eigener Takt (alle
            // 30 Minuten, siehe Config::WEATHER_FETCH_INTERVAL_MS), sofort
            // nach dem Verbinden und nach einer Standort-Aenderung
            // (lastWeatherFetchAttemptMs wird dafuer in
            // applyLocationSnapshot() auf 0 gesetzt).
            if (hasLocation && wifiConnected) {
                uint32_t now = millis();
                uint32_t interval = (weatherConsecutiveFailures >= 1)
                                     ? Config::WEATHER_RETRY_INTERVAL_MS
                                     : Config::WEATHER_FETCH_INTERVAL_MS;
                bool due = forceWeatherFetchNow || justConnected || lastWeatherFetchAttemptMs == 0 ||
                           (now - lastWeatherFetchAttemptMs >= interval);
                if (due) {
                    lastWeatherFetchAttemptMs = now;
                    forceWeatherFetchNow = false;
                    runWeatherFetch();
                }
            }

            // Automatische IP-Standort-Ermittlung (siehe Auftrag "Standort
            // automatisch, notfalls einmalig per Eingabe") - NIE, wenn von
            // Hand gesetzt. Ausloeser: erster Versuch sobald WLAN steht,
            // danach einmal pro Tag, sofort bei einem WLAN-Netzwechsel,
            // bei Fehler alle 2 Minuten erneut.
            if (wifiConnected && !locationIsManual) {
                uint8_t currentSlot = WifiStatus::get().slot;
                bool networkChanged = justConnected && lastWifiSlotForLocation != 255 &&
                                       currentSlot != lastWifiSlotForLocation;
                uint32_t now = millis();
                uint32_t interval = (ipLocationConsecutiveFailures >= 1)
                                     ? Config::WEATHER_RETRY_INTERVAL_MS
                                     : Config::IP_LOCATION_DAILY_INTERVAL_MS;
                bool due = lastIpLocationAttemptMs == 0 || networkChanged ||
                           (now - lastIpLocationAttemptMs >= interval);
                if (due) {
                    lastIpLocationAttemptMs = now;
                    runIpLocationDetect();
                }
                lastWifiSlotForLocation = currentSlot;
            }
            wifiWasConnected = wifiConnected;

            // Lebenszeichen fuer den Watchdog auf Core 1 (siehe
            // fetch_watchdog.h) - nach JEDEM abgeschlossenen
            // Schleifendurchlauf, auch wenn an diesem Durchlauf keine
            // Abfrage faellig war. Ein Haengenbleiben INNERHALB einer der
            // obigen Abfragen (trotz Gesamt-Zeitlimit theoretisch nie
            // laenger als Config::HTTP_TOTAL_TIMEOUT_MS) verhindert dann
            // zuverlaessig den naechsten Herzschlag.
            FetchHeartbeat::beat();

            uint32_t now = millis();
            if (now - lastHeapLogMs >= HEAP_LOG_INTERVAL_MS) {
                lastHeapLogMs = now;
#ifdef NIGHT_MODE_DIAG_TEST
                // Auftrag "Diagnosezeilen fuer die naechste Nacht" -
                // Laufzeit seit dem letzten Start, um einen Neustart auch
                // OHNE sichtbaren Bootbanner/logResetReason()-Treffer im
                // Mitschnitt an einem ploetzlich kleinen uptimeSec-Wert
                // erkennen zu koennen.
                Serial.printf("[fetch] freeHeap=%u minFreeHeap=%u stackHighWaterMark=%u uptimeSec=%lu\n",
                              (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
                              (unsigned)uxTaskGetStackHighWaterMark(nullptr), (unsigned long)(now / 1000));
#else
                Serial.printf("[fetch] freeHeap=%u minFreeHeap=%u stackHighWaterMark=%u\n",
                              (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
                              (unsigned)uxTaskGetStackHighWaterMark(nullptr));
#endif
            }

            vTaskDelay(pdMS_TO_TICKS(TICK_DELAY_MS));
        }
    }
}

void start() {
    SolarData::init();
    WeatherData::init();
    IpLocationData::init();
    DeviceNoResolution::init();
    WifiStatus::init();
    FetchHeartbeat::init();
    HttpDeadlineGuard::init();
    // AccountBridge::init()/LocationBridge::init() + der erste set()
    // MUESSEN von main.cpp VOR diesem Aufruf passiert sein, siehe
    // account_bridge.h/location_bridge.h - sonst wuerden
    // applyAccountSnapshot()/applyLocationSnapshot() im Task (unten)
    // mangels Mutex nichts lesen.
    commandQueue = xQueueCreate(4, sizeof(QueueItem));

    xTaskCreatePinnedToCore(taskFn, "fetch", STACK_SIZE, nullptr, PRIORITY, &taskHandle, CORE_ID);
}

void pause() {
    pauseRequested = true;
    uint32_t startMs = millis();
    while (!pausedAck && millis() - startMs < 500) { delay(10); }
}

void resume() {
    pauseRequested = false;
}

void sendCommand(Command cmd) {
    if (!commandQueue) return;
    QueueItem item{cmd, RefreshRequest()};
    xQueueSend(commandQueue, &item, 0);
}

void sendRefreshNow(const RefreshRequest& req) {
    if (!commandQueue) return;
    QueueItem item{Command::RefreshNow, req};
    xQueueSend(commandQueue, &item, 0);
}

}
