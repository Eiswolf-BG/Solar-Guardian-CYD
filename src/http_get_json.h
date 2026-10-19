#pragma once
// Gemeinsamer Helfer fuer einen einzelnen HTTPS-GET mit JSON-Antwort
// (Open-Meteo Wetter- und Geocoding-API, beide ohne Zugangsschluessel) -
// gleiches Grundprinzip wie EpeverClient::httpPostJson() (eigene frische
// Verbindung je Aufruf, setInsecure(), optionaler ArduinoJson-Filter),
// hier als GET statt POST und ohne Appid/Zugangsdaten-Header, da beide
// Open-Meteo-Endpunkte oeffentlich und ohne Schluessel erreichbar sind.
#include <Arduino.h>
#include <ArduinoJson.h>

namespace HttpGetJson {

    struct Result {
        int httpCode = -1;
        bool jsonOk = false;
        const char* jsonErr = "";
        // Groesse der rohen Antwort in Byte (Alex' Wunsch: im Abschlussbericht
        // mitteilen, wie gross eine Wetterantwort ist) - kein Datenschutz-
        // Risiko, da reine Bytezahl, kein Inhalt.
        size_t responseBytes = 0;
    };

    // filterDoc == nullptr -> keine Filterung (kompletter Parse).
    // userAgent == nullptr -> kein eigener "User-Agent"-Header (Standard
    // von HTTPClient). Nominatim (OpenStreetMap) verlangt laut
    // Nutzungsbedingungen einen eindeutigen User-Agent, siehe
    // geocoding_client.cpp.
    Result fetch(const char* url, JsonDocument& outDoc, JsonDocument* filterDoc, uint32_t timeoutMs,
                 const char* userAgent = nullptr);
}
