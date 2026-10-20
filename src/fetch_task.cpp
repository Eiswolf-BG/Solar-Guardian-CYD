#include "fetch_task.h"
#include "wifi_connect_manager.h"
#include "wifi_status.h"
#include "solar_data.h"
#include "account_bridge.h"
#include "location_bridge.h"
#include "epever_client.h"
#include "weather_client.h"
#include "met_norway_client.h"
#include "weather_data.h"
#include "ip_geolocation_client.h"
#include "ip_location_data.h"
#include "device_no_resolution.h"
#include "fetch_heartbeat.h"
#include "http_deadline_guard.h"
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

    volatile bool pauseRequested = false;
    volatile bool pausedAck = false;
    bool manualDisconnect = false;

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
    uint32_t lastEquipmentListAttemptMs = 0; // Drosselung, siehe tryResolveEquipmentList()
    time_t lastSentNextFetchAt = -1; // siehe runFetchCycle()-Aufrufstelle, Anti-Flacker-Bremse
    uint32_t lastManualRefreshMs = 0;
    bool forceFetchNow = false;
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
    bool runBatteryFetch(float& outDailyGenKwh, bool& outDailyGenValid) {
        outDailyGenValid = false;

        EpeverClient::BatteryResult result;
        int httpCode = 0, apiStatus = 0;
        bool ok = EpeverClient::fetchBatteryData(currentDeviceNo, currentToken, result, &httpCode, &apiStatus);

        if (!ok && (httpCode == 401 || apiStatus != 0)) {
            currentToken = "";
            if (EpeverClient::login(currentEmail, currentPassword, currentToken)) {
                needsLogin = false;
                ok = EpeverClient::fetchBatteryData(currentDeviceNo, currentToken, result, &httpCode, &apiStatus);
            } else {
                needsLogin = true;
            }
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

        if (result.pvVoltage.valid || result.pvCurrent.valid) {
            time_t pvMeasuredAt = result.pvVoltage.valid ? result.pvVoltage.time
                                                           : (result.pvCurrent.valid ? result.pvCurrent.time : time(nullptr));
            SolarData::setPv(result.pvVoltage.valid ? result.pvVoltage.value : 0.0f,
                              result.pvCurrent.valid ? result.pvCurrent.value : 0.0f,
                              pvMeasuredAt);
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
        if (nowTm.tm_hour < Config::DAILY_CURVE_START_HOUR) {
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
    void runFetchCycle() {
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
        runBatteryFetch(dailyFromDatapoint, dailyFromDatapointValid);
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

    void handleCommand(Command cmd) {
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
                uint32_t now = millis();
                if (hasAccount && now - lastManualRefreshMs >= Config::EPEVER_MANUAL_REFRESH_MIN_INTERVAL_MS) {
                    lastManualRefreshMs = now;
                    forceFetchNow = true;
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

            Command cmd;
            while (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
                handleCommand(cmd);
            }

            if (!manualDisconnect) WifiConnectManager::tick();

            bool wifiConnected = (WiFi.status() == WL_CONNECTED);
            bool justConnected = wifiConnected && !wifiWasConnected;
            if (hasAccount && wifiConnected) {
                uint32_t now = millis();
                uint32_t interval = (consecutiveFailures >= 1 && consecutiveFailures < Config::EPEVER_MAX_SHORT_RETRIES)
                                     ? Config::EPEVER_RETRY_INTERVAL_MS
                                     : Config::EPEVER_FETCH_INTERVAL_MS;
                bool due = forceFetchNow || justConnected || lastFetchAttemptMs == 0 ||
                           (now - lastFetchAttemptMs >= interval);
                if (due) {
                    lastFetchAttemptMs = now;
                    forceFetchNow = false;
                    if (hasDeviceNo) {
                        runFetchCycle();
                    } else {
                        runDeviceNoOnlyCycle();
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
                    uint32_t nextInterval = (consecutiveFailures >= 1 && consecutiveFailures < Config::EPEVER_MAX_SHORT_RETRIES)
                                             ? Config::EPEVER_RETRY_INTERVAL_MS
                                             : Config::EPEVER_FETCH_INTERVAL_MS;
                    int32_t msUntilNext = (int32_t)(lastFetchAttemptMs + nextInterval - now);
                    if (msUntilNext < 0) msUntilNext = 0;
                    time_t candidateNextAt = time(nullptr) + (time_t)(msUntilNext / 1000);
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
                    }
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
                Serial.printf("[fetch] freeHeap=%u minFreeHeap=%u stackHighWaterMark=%u\n",
                              (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
                              (unsigned)uxTaskGetStackHighWaterMark(nullptr));
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
    commandQueue = xQueueCreate(4, sizeof(Command));

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
    xQueueSend(commandQueue, &cmd, 0);
}

}
