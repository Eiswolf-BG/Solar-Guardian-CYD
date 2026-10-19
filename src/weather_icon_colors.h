#pragma once
// Zentrale Farbdefinitionen fuer die Wettersymbole (Auftrag "knallig,
// bunt und froehlich" - das Geraet steht als Hingucker auf dem
// Schreibtisch). Alle Farben als RGB888-Tripel an EINER Stelle, damit
// sie sich spaeter leicht anpassen lassen - color565() wandelt sie beim
// Zeichnen um (siehe weather_icons.cpp).
#include <stdint.h>

namespace WeatherIconColors {
    struct RGB { uint8_t r, g, b; };

    constexpr RGB SUN_CORE            = {255, 230, 0};   // #FFE600, knalliges Gelb
    constexpr RGB SUN_RAYS            = {255, 154, 0};   // #FF9A00, kraeftig Orange
    constexpr RGB MOON_CORE           = {255, 216, 74};  // #FFD84A, kein Weiss
    constexpr RGB MOON_STAR_WARM      = {255, 216, 74};
    constexpr RGB MOON_STAR_COOL      = {255, 255, 255};
    constexpr RGB CLOUD_LIGHT_TOP     = {255, 255, 255}; // #FFFFFF, reines Weiss
    constexpr RGB CLOUD_LIGHT_BOTTOM  = {227, 232, 238}; // #E3E8EE, neutraler Schatten, kein Blauton
    constexpr RGB CLOUD_OVERCAST      = {201, 208, 216}; // #C9D0D8, helles Neutralgrau
    constexpr RGB FOG_A                = {77, 208, 225};  // #4DD0E1, kraeftiges Tuerkis
    constexpr RGB FOG_B                = {129, 212, 250}; // #81D4FA, kraeftiges Hellblau
    constexpr RGB RAIN_CLOUD          = {255, 255, 255}; // #FFFFFF, gleiche weisse Wolke
    constexpr RGB RAIN_DROP           = {30, 144, 255};  // #1E90FF, kraeftiges, sattes Blau
    constexpr RGB RAIN_DROP_HIGHLIGHT = {255, 255, 255}; // weisser Glanzpunkt
    constexpr RGB SNOW_CLOUD          = {255, 255, 255}; // #FFFFFF, weisse Wolke
    // Beide Flockentoene bewusst Cyan/Hellblau statt Weiss - eine rein
    // weisse Flocke waere auf der jetzt ebenfalls weissen Wolke
    // unsichtbar (Auftrag "damit sie auf der weissen Wolke zu sehen
    // sind").
    constexpr RGB SNOW_FLAKE_WHITE    = {129, 212, 250}; // #81D4FA, helleres Cyan
    constexpr RGB SNOW_FLAKE_CYAN     = {64, 196, 255};  // #40C4FF, kraeftiges Hellblau/Cyan
    constexpr RGB STORM_CLOUD         = {91, 75, 138};   // #5B4B8A
    constexpr RGB BOLT_CORE           = {255, 230, 0};   // #FFE600, knallgelb
    constexpr RGB BOLT_EDGE           = {255, 154, 0};   // #FF9A00, Orange

    // Heller Rand/Kontur, damit sich jedes Symbol vom SCHWARZEN
    // Bildschirmhintergrund abhebt (Auftrag) - bei einem rein schwarzen
    // Hintergrund ist ein dunkler Rand wirkungslos, deshalb hier bewusst
    // nur die helle Variante.
    constexpr RGB OUTLINE = {255, 255, 255};
}
