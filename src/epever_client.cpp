#include "epever_client.h"
#include "epever_datapoints.h"
#include "epever_auth_headers.h"
#include "epever_active_read.h"
#include "http_deadline_guard.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <MD5Builder.h>
#include <ArduinoJson.h>
#include <time.h>

namespace EpeverClient {

namespace {
    struct PostResult {
        int httpCode = -1;
        int contentLength = -1;       // laut Server-Header, -1 = unbekannt/chunked
        bool jsonOk = false;          // true = deserializeJson() ohne Fehler
        const char* jsonErr = "";     // Kurzname des Fehlers (kein Antwortinhalt, sicher zu loggen)
    };

    struct ExtraHeader {
        const char* name;
        const char* value;
    };

    // Fuehrt einen einzelnen POST mit JSON-Body aus (eigene, frische
    // Verbindung je Aufruf, siehe CLAUDE.md). setInsecure() ist bewusst
    // gewaehlt - privates Geraet, keine Zertifikatspruefung noetig.
    // filterDoc == nullptr -> keine Filterung (kompletter Parse).
    // includeAppid/extraHeaders: Port 7002 (Batterie) braucht laut
    // Browser-Netzwerk-Tab KEINEN Appid-Header, dafuer einen rohen
    // JWT-Header "token" (siehe fetchBatteryData()) - Login/Ertrag
    // (Port 443) bleiben unveraendert mit Appid.
    PostResult httpPostJson(const char* url, const String& body, JsonDocument& outDoc, JsonDocument* filterDoc,
                             bool includeAppid = true, const ExtraHeader* extraHeaders = nullptr, uint8_t extraHeaderCount = 0) {
        PostResult result;

        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(Config::EPEVER_HTTP_TIMEOUT_MS);

        HTTPClient http;
        http.setConnectTimeout(Config::EPEVER_HTTP_TIMEOUT_MS);
        http.setTimeout(Config::EPEVER_HTTP_TIMEOUT_MS);
        if (!http.begin(client, url)) return result;

        http.addHeader("Content-Type", "application/json");
        if (includeAppid) http.addHeader("Appid", Config::EPEVER_APPID);
        http.addHeader("Connection", "close");
        for (uint8_t i = 0; i < extraHeaderCount; i++) {
            if (extraHeaders[i].value && extraHeaders[i].value[0] != '\0') {
                http.addHeader(extraHeaders[i].name, extraHeaders[i].value);
            }
        }

        // Gesamt-Zeitlimit (siehe http_deadline_guard.h) - deckt
        // Verbindungsaufbau, Senden UND Lesen zusammen ab, nicht nur
        // einzelne Socket-Operationen (siehe CLAUDE.md
        // "Fetch-Task-Absicherung").
        HttpDeadlineGuard::arm(&client, Config::HTTP_TOTAL_TIMEOUT_MS);
        result.httpCode = http.POST((uint8_t*)body.c_str(), body.length());
        if (result.httpCode == 200) {
            result.contentLength = http.getSize();
            // http.getStream() liefert bei "chunked" Transfer-Encoding
            // (contentLength == -1) die ROHEN Bytes inklusive der
            // Chunk-Groessen-Header - deserializeJson() daraus direkt
            // schlug deshalb mit "InvalidInput" fehl. getString() entpackt
            // sowohl Content-Length- als auch Chunked-Antworten korrekt zu
            // einem fertigen String, den wir danach parsen.
            String payload = http.getString();
            DeserializationError err = filterDoc
                ? deserializeJson(outDoc, payload, DeserializationOption::Filter(*filterDoc))
                : deserializeJson(outDoc, payload);
            result.jsonOk = !err;
            result.jsonErr = err.c_str();
        }
        if (HttpDeadlineGuard::lastCallTimedOut()) {
            Serial.printf("[fetch] Gesamt-Zeitlimit (%lus) ausgeloest - Verbindung abgebrochen\n",
                          (unsigned long)(Config::HTTP_TOTAL_TIMEOUT_MS / 1000));
            result.httpCode = -2; // eigener Sentinel-Wert, ungleich jedem echten HTTP-Code
        }
        HttpDeadlineGuard::disarm();
        http.end();
        return result;
    }

    // Diagnose ohne Datenschutz-Risiko: loggt NUR Feldnamen (nie Werte) -
    // damit man die tatsaechliche Antwortstruktur erkennt, ohne E-Mail/
    // Token/Meldungstexte auszugeben. Siehe CLAUDE.md Log-Regeln.
    void logJsonKeys(JsonVariantConst v, const String& label, uint8_t depth = 0) {
        if (depth > 2) return;
        if (v.is<JsonObjectConst>()) {
            String keys;
            for (JsonPairConst kv : v.as<JsonObjectConst>()) {
                if (keys.length()) keys += ",";
                keys += kv.key().c_str();
            }
            Serial.printf("[fetch] %s Felder: %s\n", label.c_str(), keys.c_str());
            for (JsonPairConst kv : v.as<JsonObjectConst>()) {
                if (kv.value().is<JsonObjectConst>()) {
                    logJsonKeys(kv.value(), label + "." + kv.key().c_str(), (uint8_t)(depth + 1));
                }
            }
        } else if (v.is<JsonArrayConst>()) {
            Serial.printf("[fetch] %s ist ein Array mit %u Eintraegen\n", label.c_str(), (unsigned)v.size());
        } else {
            Serial.printf("[fetch] %s ist kein JSON-Objekt\n", label.c_str());
        }
    }
}

bool login(const String& email, const String& password, String& outToken) {
    MD5Builder md5;
    md5.begin();
    md5.add(password);
    md5.calculate();
    String passwordMd5 = md5.toString(); // 32 Zeichen, Hex, klein - niemals loggen

    JsonDocument body;
    body["account"] = email;
    body["password"] = passwordMd5;
    body["captchaVerification"] = "";
    body["isPhone"] = 0;
    String bodyStr;
    serializeJson(body, bodyStr);

    // Bewusst OHNE Filter - solange die tatsaechliche Antwortstruktur
    // noch nicht zweifelsfrei bestaetigt ist, wuerde ein falsch
    // angenommener Feldpfad sonst alles herausfiltern. Siehe
    // logJsonKeys()-Diagnose unten (loggt nur Feldnamen, keine Werte).
    JsonDocument resp;
    PostResult res = httpPostJson(Config::EPEVER_LOGIN_URL, bodyStr, resp, nullptr);

    if (res.httpCode != 200) {
        Serial.printf("[fetch] Login HTTP-Fehler, Code=%d\n", res.httpCode);
        return false;
    }
    if (!res.jsonOk) {
        Serial.printf("[fetch] Login: Antwort kein gueltiges JSON (contentLength=%d, Fehler=%s)\n",
                      res.contentLength, res.jsonErr);
        return false;
    }

    int status = resp["status"] | -1;
    if (status != 0) {
        Serial.printf("[fetch] Login fehlgeschlagen, status=%d\n", status);
        logJsonKeys(resp, "Login-Antwort");
        return false;
    }

    String token = String((const char*)(resp["data"]["token"] | ""));
    if (token.length() == 0) {
        Serial.println("[fetch] Login: kein Token in der Antwort");
        return false;
    }

    outToken = token;
    Serial.println("[fetch] Login ok");
    return true;
}

bool fetchEnergy(const String& token, float& outTodayKwh, float& outMonthKwh,
                  float& outYearKwh, float& outTotalKwh) {
    JsonDocument body;
    body["loraFlag"] = 0;
    body["token"] = token;
    String bodyStr;
    serializeJson(body, bodyStr);

    // Bewusst OHNE Filter, aus demselben Grund wie bei login() oben.
    JsonDocument resp;
    PostResult res = httpPostJson(Config::EPEVER_ENERGY_URL, bodyStr, resp, nullptr);

    if (res.httpCode != 200) {
        Serial.printf("[fetch] Ertragsabfrage HTTP-Fehler, Code=%d\n", res.httpCode);
        return false;
    }
    if (!res.jsonOk) {
        Serial.printf("[fetch] Ertragsabfrage: Antwort kein gueltiges JSON (contentLength=%d, Fehler=%s)\n",
                      res.contentLength, res.jsonErr);
        return false;
    }

    int status = resp["status"] | -1;
    if (status != 0) {
        Serial.printf("[fetch] Ertragsabfrage fehlgeschlagen, status=%d\n", status);
        logJsonKeys(resp, "Ertrags-Antwort");
        return false;
    }

    outTodayKwh = resp["data"]["toDayGeneratingCapacity"] | 0.0f;
    outMonthKwh = resp["data"]["monthGeneratedEnergy"] | 0.0f;
    outYearKwh  = resp["data"]["yearGeneratedEnergy"] | 0.0f;
    outTotalKwh = resp["data"]["generatingCapacitySum"] | 0.0f;

    Serial.printf("[fetch] Ertrag ok: heute=%.2f monat=%.2f jahr=%.2f gesamt=%.2f kWh\n",
                  outTodayKwh, outMonthKwh, outYearKwh, outTotalKwh);
    return true;
}

namespace {
    // Sucht in data.list[] den Eintrag mit passender dataPointId und
    // uebernimmt dessen NEUESTEN Messwert (list[0], wegen "timeSort":"desc").
    // Wert kann Zahl oder Text sein (siehe Auftrag), beides wird erkannt.
    BatteryPoint extractPoint(JsonArray list, int32_t wantedId) {
        BatteryPoint p;
        for (JsonObject item : list) {
            long dpId = item["dataPointId"] | 0;
            if (dpId != wantedId) continue;
            JsonArray points = item["list"];
            if (points.size() == 0) break;
            JsonObject newest = points[0];
            // "0LL" (nicht "0") als Default - ArduinoJsons "|"-Operator
            // leitet den Zieltyp vom LITERAL des Defaults ab, nicht vom
            // Variablentyp links. Mit "0" (int) ist is<int>() fuer einen
            // ms-Epoch-Zeitstempel (~1,7*10^12, passt nicht in 32 Bit)
            // immer false, wodurch IMMER der Default (0) zurueckkommt -
            // der Zeitstempel war dadurch bislang immer 0 (01:00 CET).
            long long t = newest["time"] | 0LL; // ms
            p.time = (time_t)(t / 1000);
            JsonVariant val = newest["value"];
            if (val.is<float>() || val.is<int>() || val.is<long>()) {
                p.isText = false;
                p.value = val.as<float>();
            } else {
                p.isText = true;
                p.text = String((const char*)(val | ""));
            }
            p.valid = true;
            break;
        }
        return p;
    }
}

bool fetchBatteryData(const String& deviceNo, const String& token,
                      BatteryResult& out, int* outHttpCode, int* outApiStatus) {
    int64_t nowMs = (int64_t)time(nullptr) * 1000LL;
    // Endzeit = jetzt + kleiner Puffer, Start grosszuegig (aber schmal
    // genug, siehe config.h-Kommentar) davor - Auftrag "Fehler im Abruf
    // beheben". IMMER absteigend sortiert (unten, "timeSort":"desc"),
    // list[0] = erster/neuester Eintrag (siehe extractPoint()).
    int64_t endMs = nowMs + Config::EPEVER_BATTERY_END_BUFFER_MS;
    int64_t startMs = nowMs - Config::EPEVER_BATTERY_WINDOW_MS;

    constexpr int32_t ids[] = {
        EpeverDatapoints::BATTERY_VOLTAGE, EpeverDatapoints::BATTERY_CURRENT,
        EpeverDatapoints::CHARGE_STATUS, EpeverDatapoints::BATTERY_TEMP,
        EpeverDatapoints::BATTERY_SOC, EpeverDatapoints::DAILY_GENERATION,
        EpeverDatapoints::PV_VOLTAGE, EpeverDatapoints::PV_CURRENT,
        EpeverDatapoints::BATTERY_VOLTAGE_MAX, EpeverDatapoints::BATTERY_VOLTAGE_MIN,
        // PV_POWER dazu (Auftrag "Aktiv-Auslesen", Korrektur B3) - damit
        // das Aktiv-Auslesen der Uebersicht-Kachel "Leistung" denselben,
        // bereits bestehenden Abruf zum Nachfragen nutzen kann, statt
        // einen eigenen, zusaetzlichen Request zu brauchen.
        EpeverDatapoints::PV_POWER,
    };

    JsonDocument body;
    body["timeSort"] = "desc";
    JsonArray dps = body["devDatapoints"].to<JsonArray>();
    for (int32_t id : ids) {
        JsonObject o = dps.add<JsonObject>();
        o["deviceNo"] = deviceNo;
        o["dataPointId"] = id;
    }
    body["start"] = startMs;
    body["end"] = endMs;
    body["sampleLimit"] = Config::EPEVER_BATTERY_SAMPLE_LIMIT;
    // KEIN "token" im Body - laut Browser-Netzwerk-Tab steckt die
    // Anmeldung hier ausschliesslich im Header "token" (siehe unten).

    String bodyStr;
    serializeJson(body, bodyStr);

    // Port 7002 braucht keinen Appid-Header, dafuer den rohen JWT im
    // Header "token" (ohne "Bearer") - plus drei noch unbekannte,
    // standardmaessig leere Header (epever_auth_headers.h), die nur
    // gesendet werden, sobald Alex ihre Werte liefert.
    ExtraHeader headers[4];
    uint8_t headerCount = 0;
    headers[headerCount++] = {"token", token.c_str()};
    headers[headerCount++] = {"languagetype", EpeverAuthHeaders::LANGUAGETYPE};
    headers[headerCount++] = {"sdk-version", EpeverAuthHeaders::SDK_VERSION};
    headers[headerCount++] = {"usr-source", EpeverAuthHeaders::USR_SOURCE};

    JsonDocument resp;
    PostResult res = httpPostJson(Config::EPEVER_BATTERY_URL, bodyStr, resp, nullptr, false, headers, headerCount);

    if (outHttpCode) *outHttpCode = res.httpCode;
    if (outApiStatus) *outApiStatus = -1;

    if (res.httpCode != 200) {
        Serial.printf("[fetch] Batterieabfrage HTTP-Fehler, Code=%d\n", res.httpCode);
        return false;
    }
    if (!res.jsonOk) {
        Serial.printf("[fetch] Batterieabfrage: Antwort kein gueltiges JSON (contentLength=%d, Fehler=%s)\n",
                      res.contentLength, res.jsonErr);
        return false;
    }

    int status = resp["status"] | -1;
    if (outApiStatus) *outApiStatus = status;
    if (status != 0) {
        // "info" ist hier eine technische API-Fehlermeldung zur Anfrage
        // selbst (z.B. "invalid parameter"), keine Nutzer-/Kontodaten -
        // anders als bei login() unbedenklich zu loggen, hilft aber sehr
        // bei der Fehlersuche.
        String info = String((const char*)(resp["info"] | ""));
        Serial.printf("[fetch] Batterieabfrage fehlgeschlagen, status=%d, info=\"%s\"\n", status, info.c_str());
        logJsonKeys(resp, "Batterie-Antwort");
        return false;
    }

    JsonArray list = resp["data"]["list"];
    out.voltage = extractPoint(list, EpeverDatapoints::BATTERY_VOLTAGE);
    out.current = extractPoint(list, EpeverDatapoints::BATTERY_CURRENT);
    out.chargeStatus = extractPoint(list, EpeverDatapoints::CHARGE_STATUS);
    out.temperature = extractPoint(list, EpeverDatapoints::BATTERY_TEMP);
    out.soc = extractPoint(list, EpeverDatapoints::BATTERY_SOC);
    out.dailyGeneration = extractPoint(list, EpeverDatapoints::DAILY_GENERATION);
    out.pvVoltage = extractPoint(list, EpeverDatapoints::PV_VOLTAGE);
    out.pvCurrent = extractPoint(list, EpeverDatapoints::PV_CURRENT);
    out.voltageMax = extractPoint(list, EpeverDatapoints::BATTERY_VOLTAGE_MAX);
    out.voltageMin = extractPoint(list, EpeverDatapoints::BATTERY_VOLTAGE_MIN);
    out.power = extractPoint(list, EpeverDatapoints::PV_POWER);

    Serial.printf("[fetch] Batterie ok: U=%.2fV I=%.2fA SOC=%.1f%% T=%.1fC Tagesertrag=%.2fkWh PV U=%.1fV I=%.2fA\n",
                  out.voltage.valid ? out.voltage.value : 0.0f,
                  out.current.valid ? out.current.value : 0.0f,
                  out.soc.valid ? out.soc.value : 0.0f,
                  out.temperature.valid ? out.temperature.value : 0.0f,
                  out.dailyGeneration.valid ? out.dailyGeneration.value : 0.0f,
                  out.pvVoltage.valid ? out.pvVoltage.value : 0.0f,
                  out.pvCurrent.valid ? out.pvCurrent.value : 0.0f);
    // Ladestatus-Rohwert-Log NUR bei Aenderung - siehe fetch_task.cpp
    // (das sich den zuletzt gesehenen Wert merkt), nicht hier.

    return true;
}

namespace {
    // Ordnet einer dataPointId den passenden Messwert in einem bereits
    // geparsten BatteryResult zu - fuer den "frisch?"-Vergleich je
    // angefordertem Datenpunkt (Auftrag "Aktiv-Auslesen", Korrektur B3).
    // nullptr = Datenpunkt wird vom passiven Abruf (noch) nicht
    // mitgefuehrt.
    const BatteryPoint* pointForDataPointId(const BatteryResult& r, int32_t id) {
        if (id == EpeverDatapoints::BATTERY_VOLTAGE) return &r.voltage;
        if (id == EpeverDatapoints::BATTERY_CURRENT) return &r.current;
        if (id == EpeverDatapoints::CHARGE_STATUS) return &r.chargeStatus;
        if (id == EpeverDatapoints::BATTERY_TEMP) return &r.temperature;
        if (id == EpeverDatapoints::BATTERY_SOC) return &r.soc;
        if (id == EpeverDatapoints::DAILY_GENERATION) return &r.dailyGeneration;
        if (id == EpeverDatapoints::PV_VOLTAGE) return &r.pvVoltage;
        if (id == EpeverDatapoints::PV_CURRENT) return &r.pvCurrent;
        if (id == EpeverDatapoints::PV_POWER) return &r.power;
        if (id == EpeverDatapoints::BATTERY_VOLTAGE_MAX) return &r.voltageMax;
        if (id == EpeverDatapoints::BATTERY_VOLTAGE_MIN) return &r.voltageMin;
        return nullptr;
    }

    // EIN Befehl fuer GENAU EINEN Datenpunkt ("userop/add") - Alex' Live-
    // Mitschnitt des Portals: ein Klick auf einen Wert loest pro Wert
    // einen eigenen Befehl aus (bei der Spannung z.B. zwei - Wert plus
    // Alarmschwelle; wir senden hier bewusst NUR den Wert selbst).
    // Port 443 (NICHT 7002 wie der passive Abruf), gleicher Pfad-
    // Baustein "usrCloud/vn/..." und damit gleicher Auth-Stil wie
    // fetchEnergy() (Appid-Header per httpPostJson()-Standardwert, Token
    // im Body statt als Header). slaveIndex als TEXT (nicht als Zahl!),
    // value als JSON null - exakt wie im Mitschnitt. Die Antwort
    // (status/info/Kontoname) wird bewusst NICHT geloggt (Auftrag).
    bool triggerActiveReadOne(const String& deviceNo, const String& token, int32_t dataPointId) {
        constexpr int32_t opType = EpeverActiveRead::ALLOWED_OP_TYPE;
        if (!EpeverActiveRead::isOpTypeAllowed(opType)) return false; // nie erreicht (feste Konstante), rein defensiv

        JsonDocument body;
        body["dataPointRelId"] = dataPointId;
        body["deviceNo"] = deviceNo;
        body["opType"] = opType;
        body["slaveIndex"] = Config::EPEVER_ACTIVE_READ_SLAVE_INDEX;
        body["value"] = (const char*)nullptr;
        body["token"] = token;

        String bodyStr;
        serializeJson(body, bodyStr);

        JsonDocument resp;
        PostResult res = httpPostJson(Config::EPEVER_ACTIVE_READ_TRIGGER_URL, bodyStr, resp, nullptr);

        if (res.httpCode != 200 || !res.jsonOk) {
            Serial.printf("[fetch] Aktiv-Auslesen: Befehl fehlgeschlagen, HTTP-Code=%d\n", res.httpCode);
            return false;
        }
        int status = resp["status"] | -1;
        if (status != 0) {
            Serial.printf("[fetch] Aktiv-Auslesen: Befehl abgelehnt, status=%d\n", status);
            return false;
        }
        return true;
    }
}

bool activeReadBattery(const String& deviceNo, const String& token,
                       const int32_t* dataPointIds, uint8_t dataPointCount,
                       BatteryResult& out, bool& outFresh, int* outHttpCode, int* outApiStatus) {
    outFresh = false;
    if (outHttpCode) *outHttpCode = -1;
    if (outApiStatus) *outApiStatus = -1;
    if (dataPointCount == 0) return false;

    constexpr uint8_t MAX_DP = 8; // Sicherheitsnetz, Aufrufer deckelt bereits auf 4
    if (dataPointCount > MAX_DP) dataPointCount = MAX_DP;

    // Baseline: aktuelle Zeitstempel VOR dem Anstossen holen, ueber den
    // bereits bestehenden, portal-verifizierten passiven Abruf - so
    // braucht es keinen separaten, unbestaetigten "Status"-Endpunkt.
    BatteryResult baseline;
    int baseHttpCode = 0, baseApiStatus = 0;
    if (!fetchBatteryData(deviceNo, token, baseline, &baseHttpCode, &baseApiStatus)) {
        if (outHttpCode) *outHttpCode = baseHttpCode;
        if (outApiStatus) *outApiStatus = baseApiStatus;
        return false;
    }
    time_t previousTimes[MAX_DP];
    for (uint8_t i = 0; i < dataPointCount; i++) {
        const BatteryPoint* p = pointForDataPointId(baseline, dataPointIds[i]);
        previousTimes[i] = (p && p->valid) ? p->time : 0;
    }

    // Schritt 1: fuer JEDEN angeforderten Datenpunkt einen eigenen
    // Befehl senden, NACHEINANDER (Auftrag B3).
    bool anyTriggered = false;
    for (uint8_t i = 0; i < dataPointCount; i++) {
        if (triggerActiveReadOne(deviceNo, token, dataPointIds[i])) anyTriggered = true;
    }
    if (!anyTriggered) return false;

    // Schritt 2: GEMEINSAM auf neuere Zeitstempel fuer ALLE angeforderten
    // Datenpunkte warten (eine Gesamtgrenze, Korrektur B2) - nichts
    // bleibt haengen.
    uint32_t pollStart = millis();
    bool firstPoll = true;
    BatteryResult lastSeen = baseline;
    while ((uint32_t)(millis() - pollStart) < Config::EPEVER_ACTIVE_READ_WAIT_TIMEOUT_MS) {
        delay(firstPoll ? Config::EPEVER_ACTIVE_READ_FIRST_POLL_DELAY_MS : Config::EPEVER_ACTIVE_READ_POLL_INTERVAL_MS);
        firstPoll = false;

        BatteryResult polled;
        int pollHttpCode = 0, pollApiStatus = 0;
        bool ok = fetchBatteryData(deviceNo, token, polled, &pollHttpCode, &pollApiStatus);
        if (!ok) continue; // einzelner Nachfrage-Fehlschlag - einfach beim naechsten Versuch erneut pruefen

        lastSeen = polled;
        if (outHttpCode) *outHttpCode = pollHttpCode;
        if (outApiStatus) *outApiStatus = pollApiStatus;

        bool allFresh = true;
        for (uint8_t i = 0; i < dataPointCount; i++) {
            const BatteryPoint* p = pointForDataPointId(polled, dataPointIds[i]);
            if (!p || !p->valid || p->time <= previousTimes[i]) { allFresh = false; break; }
        }
        if (allFresh) {
            out = polled;
            outFresh = true;
            Serial.println("[fetch] Aktiv-Auslesen: alle angeforderten Werte frisch");
            return true;
        }
    }

    // Zeitgrenze erreicht - kein Fehler (die Befehle liefen durch), aber
    // nicht (alle) Werte wurden frisch.
    out = lastSeen;
    Serial.println("[fetch] Aktiv-Auslesen: Zeitgrenze erreicht, nicht alle Werte wurden frisch");
    return true;
}

bool fetchPowerCurve(const String& deviceNo, const String& token,
                    PowerCurvePoint* points, uint8_t& outCount,
                    int* outHttpCode, int* outApiStatus) {
    int64_t nowMs = (int64_t)time(nullptr) * 1000LL;
    int64_t startMs = nowMs - 3LL * 3600LL * 1000LL;
    return fetchPowerCurveWindow(deviceNo, token, startMs, nowMs, POWER_CURVE_MAX_POINTS,
                                  points, outCount, outHttpCode, outApiStatus);
}

bool fetchPowerCurveWindow(const String& deviceNo, const String& token,
                          int64_t startMs, int64_t endMs, uint8_t sampleLimit,
                          PowerCurvePoint* points, uint8_t& outCount,
                          int* outHttpCode, int* outApiStatus) {
    outCount = 0;

    JsonDocument body;
    body["timeSort"] = "desc";
    JsonArray dps = body["devDatapoints"].to<JsonArray>();
    JsonObject o = dps.add<JsonObject>();
    o["deviceNo"] = deviceNo;
    o["dataPointId"] = EpeverDatapoints::PV_POWER;
    body["start"] = startMs;
    body["end"] = endMs;
    body["sampleLimit"] = sampleLimit;

    String bodyStr;
    serializeJson(body, bodyStr);

    ExtraHeader headers[4];
    uint8_t headerCount = 0;
    headers[headerCount++] = {"token", token.c_str()};
    headers[headerCount++] = {"languagetype", EpeverAuthHeaders::LANGUAGETYPE};
    headers[headerCount++] = {"sdk-version", EpeverAuthHeaders::SDK_VERSION};
    headers[headerCount++] = {"usr-source", EpeverAuthHeaders::USR_SOURCE};

    // Filter, da die Antwort bei grossem sampleLimit deutlich groesser
    // ausfaellt als bei der Batterieabfrage - nur status/info und die
    // eigentlichen Messpunkte werden geparst.
    JsonDocument filter;
    filter["status"] = true;
    filter["info"] = true;
    JsonObject listFilterItem = filter["data"]["list"].add<JsonObject>();
    JsonObject pointFilterItem = listFilterItem["list"].add<JsonObject>();
    pointFilterItem["time"] = true;
    pointFilterItem["value"] = true;

    JsonDocument resp;
    PostResult res = httpPostJson(Config::EPEVER_BATTERY_URL, bodyStr, resp, &filter, false, headers, headerCount);

    if (outHttpCode) *outHttpCode = res.httpCode;
    if (outApiStatus) *outApiStatus = -1;

    if (res.httpCode != 200) {
        Serial.printf("[fetch] Leistungskurve HTTP-Fehler, Code=%d\n", res.httpCode);
        return false;
    }
    if (!res.jsonOk) {
        Serial.printf("[fetch] Leistungskurve: Antwort kein gueltiges JSON (contentLength=%d, Fehler=%s)\n",
                      res.contentLength, res.jsonErr);
        return false;
    }

    int status = resp["status"] | -1;
    if (outApiStatus) *outApiStatus = status;
    if (status != 0) {
        String info = String((const char*)(resp["info"] | ""));
        Serial.printf("[fetch] Leistungskurve fehlgeschlagen, status=%d, info=\"%s\"\n", status, info.c_str());
        return false;
    }

    JsonArray list = resp["data"]["list"];
    for (JsonObject item : list) {
        JsonArray pts = item["list"];
        for (JsonObject p : pts) {
            if (outCount >= POWER_CURVE_MAX_POINTS) break;
            // Gleicher Bugfix wie oben in extractPoint() - "0LL" statt
            // "0", sonst wird der ms-Epoch-Zeitstempel immer auf 0
            // (01:00 CET) zurueckgesetzt (ArduinoJsons "|" leitet den
            // Zieltyp vom Default-Literal ab, nicht vom Variablentyp).
            long long t = p["time"] | 0LL;
            float w = p["value"] | 0.0f;
            points[outCount].time = (time_t)(t / 1000);
            points[outCount].watts = w;
            outCount++;
        }
        break; // nur ein Datenpunkt angefragt - der erste Eintrag ist er
    }

    Serial.printf("[fetch] Leistungskurve ok: %u Punkte, aktuell=%.0fW\n",
                  (unsigned)outCount, outCount > 0 ? points[0].watts : 0.0f);
    return true;
}

bool fetchEquipmentList(const String& token, EquipmentListResult& out,
                        int* outHttpCode, int* outApiStatus) {
    out.total = 0;
    out.count = 0;

    JsonDocument body;
    body["equipmentName"] = "";
    body["status"] = "";
    body["powerStationId"] = "";
    body["gatewayId"] = "";
    body["projectId"] = nullptr;
    body["pageNo"] = 1;
    body["pageSize"] = EQUIPMENT_LIST_MAX_DEVICES;
    body["token"] = token;
    String bodyStr;
    serializeJson(body, bodyStr);

    // Filter: NUR status, data.total und data.list[].gatewayId parsen -
    // die Antwort enthaelt auch persoenliche Daten (Anschrift/Name/
    // Konto), die so gar nicht erst in den Speicher gelangen (Auftrag,
    // haelt zudem den RAM-Bedarf klein).
    JsonDocument filter;
    filter["status"] = true;
    filter["data"]["total"] = true;
    filter["data"]["list"][0]["gatewayId"] = true;

    ExtraHeader headers[2];
    headers[0] = {"Languagetype", "1"};
    headers[1] = {"Token", token.c_str()};

    JsonDocument resp;
    PostResult res = httpPostJson(Config::EPEVER_EQUIPMENT_LIST_URL, bodyStr, resp, &filter, true, headers, 2);

    if (outHttpCode) *outHttpCode = res.httpCode;
    if (outApiStatus) *outApiStatus = -1;

    if (res.httpCode != 200) {
        Serial.printf("[fetch] Geraeteliste HTTP-Fehler, Code=%d\n", res.httpCode);
        return false;
    }
    if (!res.jsonOk) {
        Serial.printf("[fetch] Geraeteliste: Antwort kein gueltiges JSON (contentLength=%d, Fehler=%s)\n",
                      res.contentLength, res.jsonErr);
        return false;
    }

    int status = resp["status"] | -1;
    if (outApiStatus) *outApiStatus = status;
    if (status != 0) {
        // Bewusst KEIN logJsonKeys() hier (anders als bei den anderen
        // Abfragen) - die Antwort kann persoenliche Daten enthalten,
        // auch Feldnamen sollen dafuer nicht ins Log.
        Serial.printf("[fetch] Geraeteliste fehlgeschlagen, status=%d\n", status);
        return false;
    }

    out.total = resp["data"]["total"] | 0;
    JsonArray list = resp["data"]["list"];
    for (JsonObject item : list) {
        if (out.count >= EQUIPMENT_LIST_MAX_DEVICES) break;
        out.gatewayIds[out.count] = String((const char*)(item["gatewayId"] | ""));
        out.count++;
    }

    Serial.printf("[fetch] Geraeteliste: %d Geraet(e)\n", (int)out.total);
    return true;
}

}
