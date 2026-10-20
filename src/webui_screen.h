#pragma once
// Menuepunkt "WebUI" (unter Netzwerke): zeigt die Adresse der
// schreibgeschuetzten Web-Ansicht (siehe web_server.h) zum Aufrufen im
// Browser eines Geraets im selben WLAN.
#include <Arduino.h>

namespace WebUiScreen {
    void run();
}
