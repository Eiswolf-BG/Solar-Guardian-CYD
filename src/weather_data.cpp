#include "weather_data.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>

namespace WeatherData {

namespace {
    Data data;
    SemaphoreHandle_t mutex = nullptr;

    // Gleiches Root-Fix-Prinzip wie SolarData::mutate() (siehe CLAUDE.md
    // "Dual-Core-Architektur") - der Zaehler steigt nur bei echter
    // Aenderung, nicht bei jedem Setter-Aufruf.
    template <typename F>
    void mutate(F fn) {
        if (!mutex) return;
        xSemaphoreTake(mutex, portMAX_DELAY);
        bool changed = fn(data);
        if (changed) data.changeCounter++;
        xSemaphoreGive(mutex);
    }

    bool floatChanged(float a, float b, float eps) { return fabsf(a - b) > eps; }
    bool minuteChanged(time_t a, time_t b) { return (a / 60) != (b / 60); }

    bool dayForecastChanged(const DayForecast& a, const DayForecast& b) {
        return a.weatherCode != b.weatherCode ||
               floatChanged(a.tempMaxC, b.tempMaxC, 0.05f) ||
               floatChanged(a.tempMinC, b.tempMinC, 0.05f) ||
               a.sunshineValid != b.sunshineValid ||
               (a.sunshineValid && floatChanged(a.sunshineHours, b.sunshineHours, 0.05f)) ||
               a.dateEpoch != b.dateEpoch; // Kalendertag, kein "Minute"-Vergleich sinnvoll
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

void setCurrent(float tempC, int16_t weatherCode, bool isDay, time_t measuredAt) {
    mutate([&](Data& d) {
        bool changed = !d.currentValid || floatChanged(d.currentTempC, tempC, 0.05f) ||
                       d.currentWeatherCode != weatherCode || d.isDay != isDay ||
                       minuteChanged(d.measuredAt, measuredAt);
        d.currentTempC = tempC;
        d.currentWeatherCode = weatherCode;
        d.isDay = isDay;
        d.currentValid = true;
        d.measuredAt = measuredAt;
        return changed;
    });
}

void setCurrentRefined(float tempC, int16_t weatherCode) {
    mutate([&](Data& d) {
        if (!d.currentValid) return false;
        bool changed = floatChanged(d.currentTempC, tempC, 0.05f) || d.currentWeatherCode != weatherCode;
        d.currentTempC = tempC;
        d.currentWeatherCode = weatherCode;
        return changed;
    });
}

void setDaily(const DayForecast* days, uint8_t count) {
    mutate([&](Data& d) {
        uint8_t n = count > FORECAST_DAYS ? FORECAST_DAYS : count;
        bool changed = d.dayCount != n;
        if (!changed) {
            for (uint8_t i = 0; i < n; i++) {
                if (dayForecastChanged(d.days[i], days[i])) { changed = true; break; }
            }
        }
        for (uint8_t i = 0; i < n; i++) d.days[i] = days[i];
        d.dayCount = n;
        return changed;
    });
}

void setLocationSet(bool set) {
    mutate([&](Data& d) {
        bool changed = d.locationSet != set;
        d.locationSet = set;
        return changed;
    });
}

}
