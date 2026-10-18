#include "solar_data.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace SolarData {

namespace {
    Data data;
    SemaphoreHandle_t mutex = nullptr;

    // Kleiner Helfer fuer alle Setter unten: sperren, Lambda ausfuehren,
    // changeCounter erhoehen, entsperren.
    template <typename F>
    void mutate(F fn) {
        if (!mutex) return;
        xSemaphoreTake(mutex, portMAX_DELAY);
        fn(data);
        data.changeCounter++;
        xSemaphoreGive(mutex);
    }
}

void init() {
    if (!mutex) mutex = xSemaphoreCreateMutex();
}

Data snapshot() {
    if (!mutex) return data;
    Data copy;
    xSemaphoreTake(mutex, portMAX_DELAY);
    copy = data;
    xSemaphoreGive(mutex);
    return copy;
}

void setPower(float watts, time_t measuredAt) {
    mutate([&](Data& d) {
        d.currentPowerW = watts;
        d.powerMeasuredAt = measuredAt;
        d.powerValid = true;
    });
}

void pushHistoryPoint(time_t epoch, float watts) {
    mutate([&](Data& d) {
        d.history[d.historyHead].epoch = epoch;
        d.history[d.historyHead].valueW = watts;
        d.historyHead = (uint8_t)((d.historyHead + 1) % HISTORY_MAX_POINTS);
        if (d.historyCount < HISTORY_MAX_POINTS) d.historyCount++;
        d.historyValid = true;
    });
}

void setEnergy(float todayKwh, float monthKwh, float yearKwh, float totalKwh) {
    mutate([&](Data& d) {
        d.energyTodayKwh = todayKwh;
        d.energyMonthKwh = monthKwh;
        d.energyYearKwh = yearKwh;
        d.energyTotalKwh = totalKwh;
        d.energyValid = true;
    });
}

void setBattery(float voltage, float current, float socPercent, float tempC, time_t measuredAt) {
    mutate([&](Data& d) {
        d.batteryVoltage = voltage;
        d.batteryCurrent = current;
        d.batterySocPercent = socPercent;
        d.batteryTempC = tempC;
        d.batteryValid = true;
        d.batteryMeasuredAt = measuredAt;
    });
}

void setChargeStatusCode(int16_t code) {
    mutate([&](Data& d) {
        d.chargeStatusCode = code;
        d.chargeStatusIsText = false;
        d.chargeStatusRawText[0] = 0;
    });
}

void setChargeStatusText(const char* text) {
    mutate([&](Data& d) {
        d.chargeStatusIsText = true;
        d.chargeStatusCode = -1;
        strncpy(d.chargeStatusRawText, text, sizeof(d.chargeStatusRawText) - 1);
        d.chargeStatusRawText[sizeof(d.chargeStatusRawText) - 1] = 0;
    });
}

void setDailySource(float kwh, time_t measuredAt, bool fromDatapoint) {
    mutate([&](Data& d) {
        d.dailySourceKwh = kwh;
        d.dailySourceMeasuredAt = measuredAt;
        d.dailySourceValid = true;
        d.dailySourceFromDatapoint = fromDatapoint;
        d.dayFetchSeq++;
    });
}

void setLastSuccessfulFetch(time_t epoch) {
    mutate([&](Data& d) { d.lastSuccessfulFetch = epoch; });
}

void setStatusText(const char* text) {
    mutate([&](Data& d) {
        strncpy(d.statusText, text, sizeof(d.statusText) - 1);
        d.statusText[sizeof(d.statusText) - 1] = 0;
    });
}

void setFetchStatus(FetchStatus status) {
    mutate([&](Data& d) { d.fetchStatus = status; });
}

}
