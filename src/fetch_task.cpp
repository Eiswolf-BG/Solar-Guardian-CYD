#include "fetch_task.h"
#include "wifi_connect_manager.h"
#include "wifi_status.h"
#include "solar_data.h"
#include "account_bridge.h"
#include "epever_client.h"
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
    bool needsLogin = false;
    uint8_t consecutiveFailures = 0;
    uint32_t lastFetchAttemptMs = 0;
    uint32_t lastManualRefreshMs = 0;
    bool forceFetchNow = false;
    bool firstFetchHeapLogged = false;
    bool firstBatteryHeapLogged = false;
    bool firstPowerCurveHeapLogged = false;
    bool wifiWasConnected = false;

    // Port-7002 (Batterie/History) - chargeStatus-Logging nur bei
    // Aenderung.
    bool haveLastChargeStatus = false;
    bool lastChargeStatusWasText = false;
    float lastChargeStatusValue = 0.0f;
    String lastChargeStatusText;

    void applyAccountSnapshot() {
        AccountBridge::Snapshot snap = AccountBridge::get();
        currentEmail = snap.email;
        currentPassword = snap.password;
        currentDeviceNo = snap.deviceNo;
        hasAccount = currentEmail.length() > 0 && currentPassword.length() > 0;
        currentToken = ""; // altes Token verwerfen, siehe Auftrag
        needsLogin = hasAccount;
        consecutiveFailures = 0;
        lastFetchAttemptMs = 0; // beim naechsten Tick (mit WLAN) sofort faellig
        SolarData::setFetchStatus(hasAccount ? SolarData::FetchStatus::LoggingIn
                                              : SolarData::FetchStatus::NoAccount);
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

        if (!ok) return false;

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

        if (!ok) return false;

        if (count > 0) {
            SolarData::setPower(points[0].watts, points[0].time);

            SolarData::HistoryPoint oldestFirst[EpeverClient::POWER_CURVE_MAX_POINTS];
            for (uint8_t i = 0; i < count; i++) {
                oldestFirst[i].epoch = points[count - 1 - i].time;
                oldestFirst[i].valueW = points[count - 1 - i].watts;
            }
            SolarData::setHistory(oldestFirst, count);
        }

        if (!firstPowerCurveHeapLogged) {
            firstPowerCurveHeapLogged = true;
            Serial.printf("[fetch] minFreeHeap waehrend erster Kurvenabfrage: %u\n",
                          (unsigned)ESP.getMinFreeHeap());
        }

        return true;
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
        }

        float today = 0, month = 0, year = 0, total = 0;
        bool ok = EpeverClient::fetchEnergy(currentToken, today, month, year, total);

        if (!ok) {
            // Einmal neu einloggen und die Abfrage wiederholen.
            currentToken = "";
            if (EpeverClient::login(currentEmail, currentPassword, currentToken)) {
                needsLogin = false;
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

        uint32_t lastHeapLogMs = millis();

        for (;;) {
            if (pauseRequested) {
                pausedAck = true;
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
            if (hasAccount && wifiConnected) {
                uint32_t now = millis();
                bool justConnected = !wifiWasConnected;
                uint32_t interval = (consecutiveFailures >= 1 && consecutiveFailures < Config::EPEVER_MAX_SHORT_RETRIES)
                                     ? Config::EPEVER_RETRY_INTERVAL_MS
                                     : Config::EPEVER_FETCH_INTERVAL_MS;
                bool due = forceFetchNow || justConnected || lastFetchAttemptMs == 0 ||
                           (now - lastFetchAttemptMs >= interval);
                if (due) {
                    lastFetchAttemptMs = now;
                    forceFetchNow = false;
                    runFetchCycle();
                }
            }
            wifiWasConnected = wifiConnected;

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
    WifiStatus::init();
    // AccountBridge::init() + der erste set() MUESSEN von main.cpp VOR
    // diesem Aufruf passiert sein, siehe account_bridge.h - sonst wuerde
    // applyAccountSnapshot() im Task (unten) mangels Mutex nichts lesen.
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
