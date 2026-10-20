#pragma once
// Menuepunkt "Helligkeit": stufenloser Schieberegler fuer die
// Hintergrundbeleuchtung (Config::BRIGHTNESS_MIN_PERCENT..100%,
// Auftrag "Helligkeit stufenlos einstellbar"). Aenderung gilt sofort
// (Display::applyBrightness()) und wird beim Loslassen gespeichert.
#include <Arduino.h>

namespace BrightnessScreen {
    void run();
}
