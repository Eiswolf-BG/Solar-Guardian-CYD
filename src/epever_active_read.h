#pragma once
// Reine Whitelist-Pruefung fuer das Aktiv-Auslesen (Auftrag "Aktualisieren-
// Knopf soll echt etwas bewirken") - bewusst OHNE Arduino-Abhaengigkeit
// (nur <stdint.h>), damit ein nativer Test (test/) diese Funktion direkt
// einbinden und pruefen kann, statt die Logik nur nachzubilden. Der
// einzige erlaubte opType ist Config::EPEVER_ACTIVE_READ_OP_TYPE (0 =
// NUR Lesen) - JEDER andere Wert (auch versehentlich/zukuenftig) wird
// abgelehnt, bevor epever_client.cpp::activeReadBattery() ueberhaupt
// einen HTTP-Request baut. Kein schreibender opType wird je gesendet.
#include <stdint.h>

namespace EpeverActiveRead {
    constexpr int32_t ALLOWED_OP_TYPE = 0;

    constexpr bool isOpTypeAllowed(int32_t opType) {
        return opType == ALLOWED_OP_TYPE;
    }
}
