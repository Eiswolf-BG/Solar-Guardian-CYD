#pragma once
// Debug-Layout-Pruefer (siehe CLAUDE.md "Layout-Regel") - nur aktiv, wenn
// ENABLED true ist (eine Konstante zum Abschalten, kein Laufzeit-Schalter
// noetig). Screens melden ihre gezeichneten Rechtecke (Text/Button/
// Symbol) an, finish() prueft paarweise auf Ueberlappung und ob ein
// Rechteck ueber den Bildschirmrand ragt, und loggt bei einem Konflikt
// eine Warnung ueber den seriellen Monitor.
#include <Arduino.h>

namespace LayoutCheck {

    constexpr bool ENABLED = true;

    // Vor dem ersten rect()-Aufruf eines Bildschirm-Durchlaufs.
    void begin(const char* screenName);

    // Jedes sichtbare Element melden (Label frei waehlbar, z.B. "Titel",
    // "Kachel Heute: Wert").
    void rect(const char* label, int16_t x, int16_t y, int16_t w, int16_t h);

    // Prueft alle seit begin() gemeldeten Rechtecke und loggt Konflikte.
    void finish();

    // Prueft SOFORT (nicht erst bei finish()), ob ein Textblock (childX/Y/
    // W/H) innerhalb seines eigenen Rahmens (frameX/Y/W/H) bleibt - mit
    // mind. 4px Abstand auf allen Seiten. Fuer Kacheln/Boxen mit
    // Rahmenlinie UND Text darin gedacht (siehe CLAUDE.md Layout-Regel) -
    // die normale begin()/rect()/finish()-Pruefung sieht nur das
    // AEUSSERE Rahmen-Rechteck, nicht was darin gezeichnet wird, und
    // haette z.B. einen Wert, der seine eigene Kachel-Einheit beruehrt,
    // nie gemeldet.
    void containedRect(const char* label, int16_t frameX, int16_t frameY, int16_t frameW, int16_t frameH,
                        int16_t childX, int16_t childY, int16_t childW, int16_t childH);

    // Prueft SOFORT, ob Text mit textW/textH (die tatsaechlich gemessene
    // Breite/Hoehe bei der verwendeten Schriftgroesse, siehe
    // tft.textWidth()/tft.fontHeight()) in einen Off-Screen-Puffer
    // (TileSprite, siehe tile_sprite.h) der Groesse bufferW/bufferH
    // passt - ohne das waere Text am unteren/rechten Rand des Puffers
    // abgeschnitten (Auftrag "Abgeschnittener Text", Bugfix: mehrere
    // Statuszeilen-Puffer waren niedriger als die Schrift). Anders als
    // containedRect() geht es hier NICHT um einen sichtbaren Rahmen mit
    // Innenabstand, sondern um die rohe Puffergroesse selbst - schon 1px
    // Ueberschreitung wird abgeschnitten, deshalb ohne Toleranz-Rand.
    void bufferRect(const char* label, int16_t bufferW, int16_t bufferH, int16_t textW, int16_t textH);
}
