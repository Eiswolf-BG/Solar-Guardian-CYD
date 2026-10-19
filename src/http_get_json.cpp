#include "http_get_json.h"
#include "http_deadline_guard.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace HttpGetJson {

Result fetch(const char* url, JsonDocument& outDoc, JsonDocument* filterDoc, uint32_t timeoutMs,
             const char* userAgent) {
    Result result;

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(timeoutMs);

    HTTPClient http;
    http.setConnectTimeout(timeoutMs);
    http.setTimeout(timeoutMs);
    if (!http.begin(client, url)) return result;
    http.addHeader("Connection", "close");
    if (userAgent) http.addHeader("User-Agent", userAgent);

    // Gesamt-Zeitlimit (siehe http_deadline_guard.h) - deckt
    // Verbindungsaufbau, Senden UND Lesen zusammen ab, nicht nur
    // einzelne Socket-Operationen (siehe CLAUDE.md
    // "Fetch-Task-Absicherung").
    HttpDeadlineGuard::arm(&client, Config::HTTP_TOTAL_TIMEOUT_MS);
    result.httpCode = http.GET();
    if (result.httpCode == 200) {
        // getString() statt getStream(): entpackt auch "chunked"
        // Transfer-Encoding korrekt (siehe epever_client.cpp::
        // httpPostJson() fuer die gleiche, bereits bewaehrte Begruendung).
        String payload = http.getString();
        result.responseBytes = payload.length();
        DeserializationError err = filterDoc
            ? deserializeJson(outDoc, payload, DeserializationOption::Filter(*filterDoc))
            : deserializeJson(outDoc, payload);
        result.jsonOk = !err;
        result.jsonErr = err.c_str();
    }
    if (HttpDeadlineGuard::lastCallTimedOut()) {
        Serial.printf("[fetch] Gesamt-Zeitlimit (%lus) ausgeloest - Verbindung abgebrochen\n",
                      (unsigned long)(Config::HTTP_TOTAL_TIMEOUT_MS / 1000));
        result.httpCode = -2;
    }
    HttpDeadlineGuard::disarm();
    http.end();
    return result;
}

}
