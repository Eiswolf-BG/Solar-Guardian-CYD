#pragma once
// Display-Grundlagen: das globale TFT-Objekt, Initialisierung und die
// aktuelle Systemfarbe (zentral an einer Stelle, damit sie sich spaeter
// per Menue aendern laesst, siehe color_picker_screen.h).
#include <TFT_eSPI.h>

extern TFT_eSPI tft;

namespace Display {
    // tft.init(), Rotation, Font, Hintergrund schwarz - einmal beim Start.
    void begin();

    // Aktuelle Systemfarbe (aus appConfig.systemColorIndex, siehe
    // app_config.h). Ueberall verwenden statt einer fest verdrahteten
    // Farbe, damit der Farbwechsel im Menue wirklich ueberall greift.
    uint16_t systemColor();

    // Wendet appConfig.brightnessPercent (geklemmt auf mind.
    // Config::BRIGHTNESS_MIN_PERCENT) per PWM auf die Hintergrund-
    // beleuchtung an - einmal bei begin() und erneut nach jeder Aenderung
    // im Helligkeit-Menue (brightness_screen.cpp).
    void applyBrightness();

    // Gemeinsame Farblogik fuer einen Wert UND seine Einheit (z.B. "28"
    // und "W") - beide IMMER dieselbe Farbe (Auftrag "Einheiten in der
    // Systemfarbe"): Systemfarbe bei einem gueltigen/aktuellen Wert,
    // sonst gedaempftes Grau. Ueberall verwenden, wo ein Wert und seine
    // Einheit getrennt gezeichnet werden, statt die Einheit fest verdrahtet
    // einzufaerben.
    uint16_t valueColor(bool valid, uint16_t accent);
}
