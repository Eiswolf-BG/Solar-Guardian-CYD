#include "weather_client.h"
#include "weather_data.h"
#include "http_get_json.h"
#include "config.h"
#include <time.h>

namespace WeatherClient {

namespace {
    // "YYYY-MM-DD" -> lokales Mittags-Epoch (12:00, nicht Mitternacht, um
    // DST-Randfaelle beim Umrechnen zu vermeiden - fuer die Anzeige zaehlt
    // nur das Kalenderdatum/den Wochentag daraus, siehe Auftrag "Tagesnamen
    // aus den Datumsangaben der Antwort berechnen, nicht aus der Uhr").
    time_t parseDateNoon(const char* s) {
        if (!s) return 0;
        int y = 0, m = 0, d = 0;
        if (sscanf(s, "%d-%d-%d", &y, &m, &d) != 3) return 0;
        struct tm t = {};
        t.tm_year = y - 1900;
        t.tm_mon = m - 1;
        t.tm_mday = d;
        t.tm_hour = 12;
        t.tm_isdst = -1;
        return mktime(&t);
    }
}

bool fetch(float lat, float lon, int* outHttpCode, size_t* outResponseBytes) {
    if (outHttpCode) *outHttpCode = -1;
    if (outResponseBytes) *outResponseBytes = 0;

    char url[256];
    // Koordinaten mit 6 Nachkommastellen (cm-genau) - URL enthaelt bewusst
    // KEINE Zugangsdaten, darf also normal geloggt werden; der eigentliche
    // Standort (Name/Koordinaten) wird trotzdem nirgends geloggt (Auftrag).
    snprintf(url, sizeof(url),
             "%s?latitude=%.6f&longitude=%.6f&current=temperature_2m,weather_code,is_day"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunshine_duration"
             "&timezone=auto&forecast_days=%u",
             Config::WEATHER_API_HOST, (double)lat, (double)lon, (unsigned)Config::WEATHER_FORECAST_DAYS);

    JsonDocument filter;
    JsonObject current = filter["current"].to<JsonObject>();
    current["temperature_2m"] = true;
    current["weather_code"] = true;
    current["is_day"] = true;
    JsonObject daily = filter["daily"].to<JsonObject>();
    daily["time"] = true;
    daily["weather_code"] = true;
    daily["temperature_2m_max"] = true;
    daily["temperature_2m_min"] = true;
    daily["sunshine_duration"] = true;

    JsonDocument doc;
    HttpGetJson::Result res = HttpGetJson::fetch(url, doc, &filter, Config::WEATHER_HTTP_TIMEOUT_MS);
    if (outHttpCode) *outHttpCode = res.httpCode;
    if (outResponseBytes) *outResponseBytes = res.responseBytes;
    if (res.httpCode != 200 || !res.jsonOk) return false;

    JsonObject cur = doc["current"];
    if (!cur.isNull()) {
        float tempC = cur["temperature_2m"] | 0.0f;
        int16_t code = cur["weather_code"] | -1;
        bool isDay = (cur["is_day"] | 1) != 0;
        WeatherData::setCurrent(tempC, code, isDay, time(nullptr));
    }

    JsonObject dailyResp = doc["daily"];
    if (!dailyResp.isNull()) {
        JsonArray times = dailyResp["time"];
        JsonArray codes = dailyResp["weather_code"];
        JsonArray maxT = dailyResp["temperature_2m_max"];
        JsonArray minT = dailyResp["temperature_2m_min"];
        JsonArray sun = dailyResp["sunshine_duration"];

        WeatherData::DayForecast days[WeatherData::FORECAST_DAYS];
        uint8_t n = times.size() > WeatherData::FORECAST_DAYS ? WeatherData::FORECAST_DAYS : (uint8_t)times.size();
        for (uint8_t i = 0; i < n; i++) {
            days[i].dateEpoch = parseDateNoon((const char*)(times[i] | ""));
            days[i].weatherCode = (i < codes.size()) ? (int16_t)(codes[i] | -1) : -1;
            days[i].tempMaxC = (i < maxT.size()) ? (float)(maxT[i] | 0.0f) : 0.0f;
            days[i].tempMinC = (i < minT.size()) ? (float)(minT[i] | 0.0f) : 0.0f;
            if (i < sun.size() && !sun[i].isNull()) {
                days[i].sunshineHours = (float)(sun[i] | 0.0f) / 3600.0f;
                days[i].sunshineValid = true;
            } else {
                days[i].sunshineValid = false;
            }
        }
        WeatherData::setDaily(days, n);
    }

    return true;
}

}
