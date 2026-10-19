#include "ip_geolocation_client.h"
#include "http_get_json.h"
#include "config.h"

namespace IpGeolocationClient {

namespace {
    // ipwho.is: flache Antwort, Breite/Laenge als echte JSON-Zahlen
    // (sicherer zu parsen als Text-Koordinaten, siehe CLAUDE.md
    // "ArduinoJson |-Operator"-Falle).
    bool tryIpWhoIs(Result& out, int* outHttpCode) {
        JsonDocument filter;
        filter["success"] = true;
        filter["city"] = true;
        filter["country"] = true;
        filter["latitude"] = true;
        filter["longitude"] = true;

        JsonDocument doc;
        HttpGetJson::Result res = HttpGetJson::fetch("https://ipwho.is/", doc, &filter, Config::WEATHER_HTTP_TIMEOUT_MS);
        if (outHttpCode) *outHttpCode = res.httpCode;
        if (res.httpCode != 200 || !res.jsonOk) return false;
        if (!(doc["success"] | false)) return false;

        out.lat = doc["latitude"] | 0.0f;
        out.lon = doc["longitude"] | 0.0f;
        String city = String((const char*)(doc["city"] | ""));
        String country = String((const char*)(doc["country"] | ""));
        out.name = city;
        if (country.length()) { if (out.name.length()) out.name += ", "; out.name += country; }
        out.valid = (out.lat != 0.0f || out.lon != 0.0f) && out.name.length() > 0;
        return out.valid;
    }

    // geojs.io als unabhaengiger zweiter Dienst (Rueckfall) - liefert
    // Breite/Laenge als TEXT, deshalb String+toFloat() statt des
    // "|"-Operators (der bei Zahlen-als-Text sonst leise auf den Default
    // zurueckfaellt).
    bool tryGeoJs(Result& out, int* outHttpCode) {
        JsonDocument filter;
        filter["city"] = true;
        filter["country"] = true;
        filter["latitude"] = true;
        filter["longitude"] = true;

        JsonDocument doc;
        HttpGetJson::Result res = HttpGetJson::fetch("https://get.geojs.io/v1/ip/geo.json", doc, &filter, Config::WEATHER_HTTP_TIMEOUT_MS);
        if (outHttpCode) *outHttpCode = res.httpCode;
        if (res.httpCode != 200 || !res.jsonOk) return false;

        String latStr = String((const char*)(doc["latitude"] | ""));
        String lonStr = String((const char*)(doc["longitude"] | ""));
        if (latStr.length() == 0 || lonStr.length() == 0) return false;
        out.lat = latStr.toFloat();
        out.lon = lonStr.toFloat();
        String city = String((const char*)(doc["city"] | ""));
        String country = String((const char*)(doc["country"] | ""));
        out.name = city;
        if (country.length()) { if (out.name.length()) out.name += ", "; out.name += country; }
        out.valid = (out.lat != 0.0f || out.lon != 0.0f) && out.name.length() > 0;
        return out.valid;
    }
}

bool detect(Result& out, int* outHttpCode) {
    out = Result();
    if (tryIpWhoIs(out, outHttpCode)) return true;
    out = Result();
    return tryGeoJs(out, outHttpCode);
}

}
