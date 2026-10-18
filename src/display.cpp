#include "display.h"
#include "app_font.h"
#include "app_config.h"
#include "config.h"

TFT_eSPI tft = TFT_eSPI();

namespace Display {

void begin() {
    tft.init();
    tft.setRotation(0); // Hochformat 240x320 (native Panel-Ausrichtung, Alex' Wunsch)
    tft.setFreeFont(&AppFont11pt);
    // Dieses Board (ESP32-2432S028, ILI9341_2_DRIVER) braucht invertDisplay(true)
    // fuer korrekte Farben - uebernommen aus eiswolfs-flightradar-CYD, dort
    // bereits am echten Geraet bestaetigt.
    tft.invertDisplay(true);
    tft.fillScreen(TFT_BLACK);
}

uint16_t systemColor() {
    uint8_t idx = appConfig.systemColorIndex;
    if (idx >= Config::COLOR_COUNT) idx = 0;
    return Config::COLOR_PALETTE[idx];
}

}
