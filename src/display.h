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

    // Feste, systemweite Gestaltungsregel (Auftrag "Farbregeln
    // vereinheitlichen"): Systemfarbe (accent) ist fuer Ueberschriften/
    // Beschriftungen/Einheiten/Rahmen/Symbole/Reiter sowie fuer die Kurve
    // bzw. Balken eines Graphen reserviert. Die Sekundaerfarbe ist fuer
    // vom Portal/Wetterdienst ABGEFRAGTE ZAHLENWERTE (Leistung, Spannung,
    // Strom, Ertrag, Batteriewerte, Temperatur, Tages-Hoechstwert ...),
    // fuer Raster-Hilfslinien in Graphen und fuer Achsenbeschriftungen/
    // Zeitachsen sowie Wochentags-/Monats-/Jahreszahlen-Beschriftungen
    // unter Balken. Aktuell TFT_DARKGREY, zentral hier definiert, damit
    // sich das spaeter an einer einzigen Stelle aendern laesst.
    uint16_t secondaryColor();

    // Gemeinsame Farblogik fuer einen abgefragten Zahlenwert: IMMER die
    // Sekundaerfarbe (der "valid"-Parameter bleibt aus Kompatibilitaets-
    // gruenden in der Signatur, beeinflusst das Ergebnis aber nicht mehr -
    // auch ein "--"-Platzhalter bei fehlenden Daten wird in der
    // Sekundaerfarbe gezeichnet, der Text selbst zeigt bereits die
    // Ungueltigkeit an). Ueberall verwenden, wo ein abgefragter Zahlenwert
    // gezeichnet wird. Die zugehoerige Einheit bleibt davon UNABHAENGIG
    // immer in der Systemfarbe (siehe CLAUDE.md "Farbregeln").
    uint16_t valueColor(bool valid, uint16_t accent);
}
