#include "solar_data.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>
#include <string.h>

namespace SolarData {

namespace {
    Data data;
    SemaphoreHandle_t mutex = nullptr;

    // Kleiner Helfer fuer alle Setter unten: sperren, Lambda ausfuehren,
    // changeCounter NUR erhoehen, wenn das Lambda eine echte Aenderung
    // meldet (true zurueckgibt), entsperren. Root-Fix gegen Flackern
    // (Auftrag "Ursachen suchen, nicht raten"): vorher erhoehte JEDER
    // Setter den Zaehler bedingungslos, auch wenn der neu geschriebene
    // Wert mit dem alten identisch war - ein Aufrufer, der einen Wert
    // bei jedem Schleifendurchlauf setzt, erzeugte dadurch eine
    // Endlosschleife aus Scheinaenderungen. Siehe CLAUDE.md
    // "Dual-Core-Architektur".
    template <typename F>
    void mutate(F fn) {
        if (!mutex) return;
        xSemaphoreTake(mutex, portMAX_DELAY);
        bool changed = fn(data);
        if (changed) data.changeCounter++;
        xSemaphoreGive(mutex);
    }

    // Toleranz an der tatsaechlichen Anzeige-Genauigkeit ausgerichtet
    // (siehe jeweilige *_page.cpp-Formatierung) - eine Aenderung UNTER
    // dieser Schwelle wuerde auf dem Display ohnehin identisch aussehen,
    // ist also keine "echte" Aenderung im Sinne des Auftrags.
    bool floatChanged(float a, float b, float eps) { return fabsf(a - b) > eps; }

    // Zeiten werden nur auf die ANGEZEIGTE MINUTE verglichen (Auftrag),
    // nicht auf die Sekunde - alle Uhrzeiten auf den Seiten zeigen nur
    // HH:MM.
    bool minuteChanged(time_t a, time_t b) { return (a / 60) != (b / 60); }
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
        bool changed = !d.powerValid || floatChanged(d.currentPowerW, watts, 0.5f) ||
                       minuteChanged(d.powerMeasuredAt, measuredAt);
        d.currentPowerW = watts;
        d.powerMeasuredAt = measuredAt;
        d.powerValid = true;
        return changed;
    });
}

void pushHistoryPoint(time_t epoch, float watts) {
    // Fuegt immer einen neuen, echten Messpunkt mit eigenem Zeitstempel
    // hinzu (kein Wiederholungsfall wie die skalaren Setter) - die Kurve
    // AENDERT sich dadurch per Definition, Zaehler bleibt hier bewusst
    // unbedingt.
    mutate([&](Data& d) {
        d.history[d.historyHead].epoch = epoch;
        d.history[d.historyHead].valueW = watts;
        d.historyHead = (uint8_t)((d.historyHead + 1) % HISTORY_MAX_POINTS);
        if (d.historyCount < HISTORY_MAX_POINTS) d.historyCount++;
        d.historyValid = true;
        return true;
    });
}

void setHistory(const HistoryPoint* points, uint8_t count) {
    mutate([&](Data& d) {
        uint8_t n = count > HISTORY_MAX_POINTS ? HISTORY_MAX_POINTS : count;
        bool changed = !d.historyValid || d.historyCount != n;
        if (!changed) {
            for (uint8_t i = 0; i < n; i++) {
                if (floatChanged(d.history[i].valueW, points[i].valueW, 0.5f) ||
                    minuteChanged(d.history[i].epoch, points[i].epoch)) {
                    changed = true;
                    break;
                }
            }
        }
        for (uint8_t i = 0; i < n; i++) d.history[i] = points[i];
        d.historyCount = n;
        d.historyHead = (uint8_t)(n % HISTORY_MAX_POINTS);
        d.historyValid = true;
        return changed;
    });
}

void setDailyCurve(const DailyCurvePoint* points, uint16_t count, const char* dayKey) {
    mutate([&](Data& d) {
        uint16_t n = count > DAILY_CURVE_MAX_POINTS ? DAILY_CURVE_MAX_POINTS : count;
        bool dayChanged = strncmp(d.dailyCurveDate, dayKey, sizeof(d.dailyCurveDate) - 1) != 0;
        bool changed = dayChanged || !d.dailyCurveValid || d.dailyCurveCount != n;
        if (!changed) {
            for (uint16_t i = 0; i < n; i++) {
                if (d.dailyCurve[i].minutesSinceMidnight != points[i].minutesSinceMidnight ||
                    d.dailyCurve[i].watts != points[i].watts) {
                    changed = true;
                    break;
                }
            }
        }
        for (uint16_t i = 0; i < n; i++) d.dailyCurve[i] = points[i];
        d.dailyCurveCount = n;
        d.dailyCurveValid = true;
        strncpy(d.dailyCurveDate, dayKey, sizeof(d.dailyCurveDate) - 1);
        d.dailyCurveDate[sizeof(d.dailyCurveDate) - 1] = 0;
        return changed;
    });
}

void appendDailyCurvePoint(uint16_t minutesSinceMidnight, uint16_t watts, const char* dayKey) {
    mutate([&](Data& d) {
        if (!d.dailyCurveValid) return false;
        if (strncmp(d.dailyCurveDate, dayKey, sizeof(d.dailyCurveDate) - 1) != 0) return false;
        if (d.dailyCurveCount >= DAILY_CURVE_MAX_POINTS) return false;
        if (d.dailyCurveCount > 0 && minutesSinceMidnight <= d.dailyCurve[d.dailyCurveCount - 1].minutesSinceMidnight) return false;
        d.dailyCurve[d.dailyCurveCount].minutesSinceMidnight = minutesSinceMidnight;
        d.dailyCurve[d.dailyCurveCount].watts = watts;
        d.dailyCurveCount++;
        return true;
    });
}

void setEnergy(float todayKwh, float monthKwh, float yearKwh, float totalKwh) {
    mutate([&](Data& d) {
        bool changed = !d.energyValid || floatChanged(d.energyTodayKwh, todayKwh, 0.005f) ||
                       floatChanged(d.energyMonthKwh, monthKwh, 0.005f) ||
                       floatChanged(d.energyYearKwh, yearKwh, 0.005f) ||
                       floatChanged(d.energyTotalKwh, totalKwh, 0.005f);
        d.energyTodayKwh = todayKwh;
        d.energyMonthKwh = monthKwh;
        d.energyYearKwh = yearKwh;
        d.energyTotalKwh = totalKwh;
        d.energyValid = true;
        return changed;
    });
}

void setBattery(float voltage, float current, float socPercent, float tempC, time_t measuredAt) {
    mutate([&](Data& d) {
        bool changed = !d.batteryValid || floatChanged(d.batteryVoltage, voltage, 0.005f) ||
                       floatChanged(d.batteryCurrent, current, 0.005f) ||
                       floatChanged(d.batterySocPercent, socPercent, 0.5f) ||
                       floatChanged(d.batteryTempC, tempC, 0.05f) ||
                       minuteChanged(d.batteryMeasuredAt, measuredAt);
        d.batteryVoltage = voltage;
        d.batteryCurrent = current;
        d.batterySocPercent = socPercent;
        d.batteryTempC = tempC;
        d.batteryValid = true;
        d.batteryMeasuredAt = measuredAt;
        return changed;
    });
}

void setBatteryVoltageExtremes(float maxV, float minV) {
    mutate([&](Data& d) {
        bool changed = !d.batteryVoltageExtremesValid ||
                       floatChanged(d.batteryVoltageMax, maxV, 0.005f) ||
                       floatChanged(d.batteryVoltageMin, minV, 0.005f);
        d.batteryVoltageMax = maxV;
        d.batteryVoltageMin = minV;
        d.batteryVoltageExtremesValid = true;
        return changed;
    });
}

void setPv(float voltage, float current, time_t measuredAt) {
    mutate([&](Data& d) {
        bool changed = !d.pvValid || floatChanged(d.pvVoltage, voltage, 0.05f) ||
                       floatChanged(d.pvCurrent, current, 0.005f) ||
                       minuteChanged(d.pvMeasuredAt, measuredAt);
        d.pvVoltage = voltage;
        d.pvCurrent = current;
        d.pvValid = true;
        d.pvMeasuredAt = measuredAt;
        return changed;
    });
}

void resetForDeviceChange() {
    // Seltener, einschneidender Vorgang (Geraete-Seriennummer geaendert)
    // - bleibt bewusst unbedingt, Vergleich waere hier unnoetiger
    // Aufwand fuer einen Fall, der ohnehin fast immer eine echte
    // Aenderung ist.
    mutate([&](Data& d) {
        d.currentPowerW = 0.0f; d.powerMeasuredAt = 0; d.powerValid = false;
        d.historyCount = 0; d.historyHead = 0; d.historyValid = false;
        d.dailyCurveCount = 0; d.dailyCurveValid = false; d.dailyCurveDate[0] = 0;
        d.energyTodayKwh = d.energyMonthKwh = d.energyYearKwh = d.energyTotalKwh = 0.0f;
        d.energyValid = false;
        d.batteryVoltage = d.batteryCurrent = d.batterySocPercent = d.batteryTempC = 0.0f;
        d.batteryValid = false; d.batteryMeasuredAt = 0;
        d.chargeStatusCode = -1; d.chargeStatusIsText = false; d.chargeStatusRawText[0] = 0;
        d.pvVoltage = d.pvCurrent = 0.0f; d.pvValid = false; d.pvMeasuredAt = 0;
        d.dailySourceKwh = 0.0f; d.dailySourceMeasuredAt = 0; d.dailySourceValid = false;
        d.dailySourceFromDatapoint = false;
        d.lastSuccessfulFetch = 0;
        return true;
    });
}

void setChargeStatusCode(int16_t code) {
    mutate([&](Data& d) {
        bool changed = d.chargeStatusIsText || d.chargeStatusCode != code;
        d.chargeStatusCode = code;
        d.chargeStatusIsText = false;
        d.chargeStatusRawText[0] = 0;
        return changed;
    });
}

void setChargeStatusText(const char* text) {
    mutate([&](Data& d) {
        bool changed = !d.chargeStatusIsText || strncmp(d.chargeStatusRawText, text, sizeof(d.chargeStatusRawText) - 1) != 0;
        d.chargeStatusIsText = true;
        d.chargeStatusCode = -1;
        strncpy(d.chargeStatusRawText, text, sizeof(d.chargeStatusRawText) - 1);
        d.chargeStatusRawText[sizeof(d.chargeStatusRawText) - 1] = 0;
        return changed;
    });
}

void setDailySource(float kwh, time_t measuredAt, bool fromDatapoint) {
    mutate([&](Data& d) {
        // dayFetchSeq steigt bewusst IMMER (YieldLedger braucht jeden
        // Abfrageversuch, siehe Header-Kommentar) - der Anzeige-
        // Aenderungszaehler aber nur bei echter Wertaenderung.
        bool changed = !d.dailySourceValid || floatChanged(d.dailySourceKwh, kwh, 0.005f) ||
                       minuteChanged(d.dailySourceMeasuredAt, measuredAt) ||
                       d.dailySourceFromDatapoint != fromDatapoint;
        d.dailySourceKwh = kwh;
        d.dailySourceMeasuredAt = measuredAt;
        d.dailySourceValid = true;
        d.dailySourceFromDatapoint = fromDatapoint;
        d.dayFetchSeq++;
        return changed;
    });
}

void setLastSuccessfulFetch(time_t epoch) {
    mutate([&](Data& d) {
        bool changed = minuteChanged(d.lastSuccessfulFetch, epoch);
        d.lastSuccessfulFetch = epoch;
        // Unbedingt, unabhaengig von "changed" oben (siehe solar_data.h-
        // Kommentar) - der Fetch-Watchdog muss jeden einzelnen
        // erfolgreichen Zyklus zaehlen, nicht nur die, die auch eine
        // neue Anzeige-Minute auslösen.
        d.successfulFetchCount++;
        return changed;
    });
}

void setNextFetchAt(time_t epoch) {
    mutate([&](Data& d) {
        bool changed = minuteChanged(d.nextFetchAt, epoch);
        d.nextFetchAt = epoch;
        return changed;
    });
}

void setNightBatteryNextFetchAt(time_t epoch) {
    mutate([&](Data& d) {
        bool changed = minuteChanged(d.nightBatteryNextFetchAt, epoch);
        d.nightBatteryNextFetchAt = epoch;
        return changed;
    });
}

void setFinalQueryDoneDate(const char* dateStr) {
    mutate([&](Data& d) {
        bool changed = strncmp(d.finalQueryDoneDate, dateStr, sizeof(d.finalQueryDoneDate) - 1) != 0;
        strncpy(d.finalQueryDoneDate, dateStr, sizeof(d.finalQueryDoneDate) - 1);
        return changed;
    });
}

void setStatusText(const char* text) {
    mutate([&](Data& d) {
        bool changed = strncmp(d.statusText, text, sizeof(d.statusText) - 1) != 0;
        strncpy(d.statusText, text, sizeof(d.statusText) - 1);
        d.statusText[sizeof(d.statusText) - 1] = 0;
        return changed;
    });
}

void setFetchStatus(FetchStatus status) {
    mutate([&](Data& d) {
        bool changed = d.fetchStatus != status;
        d.fetchStatus = status;
        return changed;
    });
}

void setRefreshFeedback(RefreshFeedback fb) {
    mutate([&](Data& d) {
        d.refreshFeedback = fb;
        d.refreshFeedbackSeq++;
        return true; // immer ein eigenes Ereignis, siehe Header-Kommentar
    });
}

}
