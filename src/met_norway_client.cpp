#include "met_norway_client.h"
#include "http_get_json.h"
#include "config.h"

namespace MetNorwayClient {

namespace {
    // Endung abschneiden (_day/_night/_polartwilight, Auftrag), dann auf
    // einen Code aus den bereits vorhandenen Wettersymbol-Gruppen
    // abbilden (siehe weather_icons.cpp::draw() - dieselben Bereiche wie
    // bei Open-Meteo: 0 klar, 1/2 teils bewoelkt, 3 bedeckt, 45/48 Nebel,
    // 61 Regen, 71 Schnee, 95 Gewitter). Unbekannte/neue Namen fallen auf
    // 3 (neutrales Wolkensymbol), nie abstuerzen.
    int16_t mapSymbolToWmoCode(const char* raw) {
        if (!raw || !raw[0]) return 3;
        String base = raw;
        int us = base.lastIndexOf('_');
        if (us > 0) {
            String suffix = base.substring(us + 1);
            if (suffix == "day" || suffix == "night" || suffix == "polartwilight") {
                base = base.substring(0, us);
            }
        }
        base.toLowerCase();

        if (base.indexOf("thunder") >= 0) return 97;
        if (base.indexOf("fog") >= 0) return 45;
        if (base.indexOf("sleet") >= 0) return 71;
        if (base.indexOf("snow") >= 0) return 71;
        if (base.indexOf("rain") >= 0) return 61;
        if (base == "cloudy") return 3;
        if (base == "partlycloudy") return 2;
        if (base == "fair") return 1;
        if (base == "clearsky") return 0;
        return 3;
    }
}

bool fetchCurrent(float lat, float lon, float& outTempC, int16_t& outWmoCode, float& outCloudPercent,
                  int* outHttpCode, size_t* outResponseBytes) {
    if (outHttpCode) *outHttpCode = -1;
    if (outResponseBytes) *outResponseBytes = 0;

    char url[160];
    // 4 Nachkommastellen, wie von MET Norway vorgegeben.
    snprintf(url, sizeof(url), "%s?lat=%.4f&lon=%.4f",
             Config::MET_NORWAY_API_HOST, (double)lat, (double)lon);

    // Filter: nur Temperatur + Bewoelkung + Symbol aus next_1_hours
    // (reduziert jeden Zeitpunkt in der Antwort auf wenige kleine
    // Felder, haelt den RAM klein).
    JsonDocument filter;
    JsonObject item = filter["properties"]["timeseries"][0].to<JsonObject>();
    item["data"]["instant"]["details"]["air_temperature"] = true;
    item["data"]["instant"]["details"]["cloud_area_fraction"] = true;
    item["data"]["next_1_hours"]["summary"]["symbol_code"] = true;

    JsonDocument doc;
    HttpGetJson::Result res = HttpGetJson::fetch(url, doc, &filter, Config::WEATHER_HTTP_TIMEOUT_MS,
                                                  Config::MET_NORWAY_USER_AGENT);
    if (outHttpCode) *outHttpCode = res.httpCode;
    if (outResponseBytes) *outResponseBytes = res.responseBytes;
    if (res.httpCode != 200 || !res.jsonOk) return false;

    JsonArray series = doc["properties"]["timeseries"];
    if (series.size() == 0) return false;
    JsonObject first = series[0];
    JsonObject details = first["data"]["instant"]["details"];
    if (details.isNull()) return false;
    if (!details["air_temperature"].is<float>()) return false;

    const char* symbolRaw = first["data"]["next_1_hours"]["summary"]["symbol_code"] | "";
    if (!symbolRaw || symbolRaw[0] == '\0') return false; // next_1_hours fehlt (z.B. hohe Breitengrade) - kein Ergebnis

    outTempC = details["air_temperature"] | 0.0f;
    outCloudPercent = details["cloud_area_fraction"] | -1.0f;
    outWmoCode = mapSymbolToWmoCode(symbolRaw);
    return true;
}

}
