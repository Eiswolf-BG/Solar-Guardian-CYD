#include "display.h"
#include "app_font.h"
#include "app_config.h"
#include "config.h"

TFT_eSPI tft = TFT_eSPI();

namespace Display {

namespace {
    // Eigener LEDC-Kanal fuer die Hintergrundbeleuchtung (Auftrag
    // "Helligkeit stufenlos einstellbar") - TFT_eSPI selbst kennt nur
    // An/Aus (TFT_BACKLIGHT_ON-Build-Flag), uebernimmt den Pin danach
    // NICHT mehr aktiv, deshalb kann LEDC ihn gefahrlos uebernehmen.
    constexpr uint8_t BACKLIGHT_LEDC_CHANNEL = 0;
    constexpr uint32_t BACKLIGHT_LEDC_FREQ_HZ = 5000;
    constexpr uint8_t BACKLIGHT_LEDC_RES_BITS = 8;
}

void begin() {
    tft.init();
    tft.setRotation(0); // Hochformat 240x320 (native Panel-Ausrichtung, Alex' Wunsch)
    tft.setFreeFont(&AppFont11pt);
    // Dieses Board (ESP32-2432S028, ILI9341_2_DRIVER) braucht invertDisplay(true)
    // fuer korrekte Farben - uebernommen aus eiswolfs-flightradar-CYD, dort
    // bereits am echten Geraet bestaetigt.
    tft.invertDisplay(true);
    tft.fillScreen(TFT_BLACK);

    ledcSetup(BACKLIGHT_LEDC_CHANNEL, BACKLIGHT_LEDC_FREQ_HZ, BACKLIGHT_LEDC_RES_BITS);
    ledcAttachPin(Config::TFT_BL_PIN, BACKLIGHT_LEDC_CHANNEL);
    applyBrightness();
}

uint16_t systemColor() {
    uint8_t idx = appConfig.systemColorIndex;
    if (idx >= Config::COLOR_COUNT) idx = 0;
    return Config::COLOR_PALETTE[idx];
}

void applyBrightness() {
    uint8_t pct = appConfig.brightnessPercent;
    if (pct < Config::BRIGHTNESS_MIN_PERCENT) pct = Config::BRIGHTNESS_MIN_PERCENT;
    if (pct > 100) pct = 100;
    uint32_t maxDuty = (1u << BACKLIGHT_LEDC_RES_BITS) - 1u;
    uint32_t duty = (maxDuty * pct) / 100u;
    ledcWrite(BACKLIGHT_LEDC_CHANNEL, duty);
}

}
