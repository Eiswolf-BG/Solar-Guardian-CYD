#pragma once
// Ergebnis der automatischen Seriennummer-Ermittlung (Geraeteliste, siehe
// epever_client.h::fetchEquipmentList()) vom Fetch-Task (Core 0) zu
// Core 1 - gleiches Mutex/Momentaufnahme-Prinzip wie ip_location_data.h.
// Core 1 (main_screen.cpp::tick()) uebernimmt ein NEUES Ergebnis in
// appConfig.deviceNo und speichert (SD-Zugriff bleibt exklusiv auf
// Core 1). Enthaelt NIE die Seriennummer im Log - nur hier im RAM.
#include <Arduino.h>

namespace DeviceNoResolution {

    struct Data {
        String gatewayId;      // vom Portal ermittelte Seriennummer
        bool shouldApply = false; // true = Core 1 soll sie uebernehmen (nur bei genau 1 Geraet)
        uint32_t changeCounter = 0;
    };

    void init();
    Data snapshot();

    // NUR vom Fetch-Task (Core 0) aufrufen, NUR wenn genau ein Geraet in
    // der Liste steht.
    void setResult(const String& gatewayId);
}
