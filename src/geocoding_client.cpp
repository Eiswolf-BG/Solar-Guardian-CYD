#include "geocoding_client.h"
#include "http_get_json.h"
#include "config.h"

namespace GeocodingClient {

namespace {
    // Eindeutiger User-Agent, wie von Nominatims Nutzungsbedingungen
    // verlangt - nur einmalige, nutzerausgeloeste Abfragen (siehe
    // geocoding_client.h), keine Dauerabfrage.
    constexpr const char* USER_AGENT = "SolarGuardianCYD/1.0 (privates Geraet, kein Dauerabruf)";

    // Minimaler URL-Encoder - nur das noetigste (Buchstaben/Ziffern
    // unveraendert, alles andere als %XX, Leerzeichen als "+"), reicht
    // fuer Ortsnamen/Postleitzahlen von der hiesigen Tastatur (siehe
    // keypad.cpp, kein eigener URL-Encoder im Projekt vorhanden).
    String urlEncode(const String& s) {
        String out;
        out.reserve(s.length() * 3);
        for (uint16_t i = 0; i < s.length(); i++) {
            char c = s[i];
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~') {
                out += c;
            } else if (c == ' ') {
                out += '+';
            } else {
                char buf[4];
                snprintf(buf, sizeof(buf), "%%%02X", (uint8_t)c);
                out += buf;
            }
        }
        return out;
    }

    bool isAllDigits(const String& s) {
        if (s.length() == 0) return false;
        for (uint16_t i = 0; i < s.length(); i++) {
            if (s[i] < '0' || s[i] > '9') return false;
        }
        return true;
    }

    // Baut "Stadtname (PLZ)" aus den addressdetails=1-Feldern statt des
    // langen display_name (Auftrag) - Stadt/Gemeinde zuerst (city/town/
    // municipality/village), ein Stadtteil (suburb/city_district) NUR als
    // letzter Rueckfall, falls nichts Groesseres vorhanden ist. Bei einer
    // PLZ-Suche traf frueher oft nur der Stadtteil zu, in dem die PLZ
    // beginnt (z.B. "Türkheim" statt der eigentlichen Stadt) - das wird
    // durch die Prioritaet city>town>...>suburb korrigiert.
    String buildName(JsonObject r) {
        JsonObject addr = r["address"];
        String place;
        constexpr const char* PRIMARY_KEYS[] = {"city", "town", "municipality", "village"};
        for (const char* k : PRIMARY_KEYS) {
            String v = String((const char*)(addr[k] | ""));
            if (v.length()) { place = v; break; }
        }
        if (place.length() == 0) {
            constexpr const char* FALLBACK_KEYS[] = {"suburb", "city_district"};
            for (const char* k : FALLBACK_KEYS) {
                String v = String((const char*)(addr[k] | ""));
                if (v.length()) { place = v; break; }
            }
        }
        if (place.length() == 0) {
            // Letzter Rueckfall, falls addressdetails gar nichts Brauchbares
            // liefert (sollte praktisch nie vorkommen) - lieber ein nicht
            // ganz passender Name als ein leerer Treffer.
            place = String((const char*)(r["display_name"] | ""));
        }
        String postcode = String((const char*)(addr["postcode"] | ""));
        if (postcode.length()) {
            place += " (";
            place += postcode;
            place += ")";
        }
        return place;
    }

    // Ein einzelner Nominatim-Abruf - gibt true bei HTTP 200 + gueltigem
    // JSON zurueck (auch wenn 0 Treffer), false bei Netzwerk-/JSON-Fehler.
    // Entfernt doppelte Namen (gleicher gebauter Anzeigename) direkt hier.
    bool fetchOnce(const String& url, Result* out, uint8_t& outCount) {
        outCount = 0;
        JsonDocument filter;
        JsonObject item = filter.add<JsonObject>();
        item["display_name"] = true;
        item["lat"] = true;
        item["lon"] = true;
        JsonObject addr = item["address"].to<JsonObject>();
        addr["city"] = true;
        addr["town"] = true;
        addr["municipality"] = true;
        addr["village"] = true;
        addr["suburb"] = true;
        addr["city_district"] = true;
        addr["postcode"] = true;

        JsonDocument doc;
        HttpGetJson::Result res = HttpGetJson::fetch(url.c_str(), doc, &filter, Config::WEATHER_HTTP_TIMEOUT_MS, USER_AGENT);
        if (res.httpCode != 200 || !res.jsonOk) return false;

        JsonArray results = doc.as<JsonArray>();
        for (JsonObject r : results) {
            if (outCount >= MAX_RESULTS) break;
            String name = buildName(r);
            bool duplicate = false;
            for (uint8_t i = 0; i < outCount; i++) {
                if (out[i].name == name) { duplicate = true; break; }
            }
            if (duplicate) continue;

            // lat/lon kommen bei Nominatim als TEXT, nicht als Zahl -
            // String+toFloat() statt des "|"-Operators (der bei Zahlen-
            // als-Text sonst leise auf den Default zurueckfaellt, siehe
            // CLAUDE.md "ArduinoJson |-Operator"-Falle).
            String latStr = String((const char*)(r["lat"] | ""));
            String lonStr = String((const char*)(r["lon"] | ""));
            out[outCount].name = name;
            out[outCount].lat = latStr.toFloat();
            out[outCount].lon = lonStr.toFloat();
            outCount++;
        }
        return true;
    }
}

SearchStatus search(const String& query, Result* out, uint8_t& outCount) {
    outCount = 0;
    String trimmed = query;
    trimmed.trim();

    if (isAllDigits(trimmed) && trimmed.length() >= 4 && trimmed.length() <= 5) {
        // Postleitzahl - Nominatims "postalcode="-Parameter ist dafuer
        // zuverlaessig (Open-Meteo war das NICHT, siehe geocoding_client.h-
        // Kommentar). Reihenfolge DE -> AT -> CH, erster Treffer gewinnt.
        constexpr const char* COUNTRIES[] = {"de", "at", "ch"};
        bool anyRequestSucceeded = false;
        for (const char* cc : COUNTRIES) {
            String url = "https://nominatim.openstreetmap.org/search?postalcode=" + trimmed +
                         "&country=" + String(cc) + "&format=json&addressdetails=1&limit=" + String(MAX_RESULTS);
            uint8_t n = 0;
            bool ok = fetchOnce(url, out, n);
            if (ok) {
                anyRequestSucceeded = true;
                if (n > 0) { outCount = n; return SearchStatus::Ok; }
            }
        }
        return anyRequestSucceeded ? SearchStatus::Ok : SearchStatus::NetworkError;
    }

    String url = "https://nominatim.openstreetmap.org/search?q=" + urlEncode(trimmed) +
                 "&format=json&addressdetails=1&limit=" + String(MAX_RESULTS);
    uint8_t n = 0;
    bool ok = fetchOnce(url, out, n);
    if (!ok) return SearchStatus::NetworkError;
    outCount = n;
    return SearchStatus::Ok;
}

}
