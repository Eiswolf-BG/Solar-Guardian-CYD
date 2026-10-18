#pragma once
// Der "?"-Hilfe-Knopf, den jeder Bildschirm (ausser Touchkalibrierung und
// Tastatur) bekommt - siehe CLAUDE.md. Prinzip uebernommen aus
// eiswolfs-flightradar-CYD/eiswolfs-toolbox (kleiner "?"-Knopf, der ein
// Erklaer-Fenster mit OK-Knopf oeffnet), mit eigenem, einfacherem Design
// (kein Scroll-Text noetig, da alle Hilfetexte hier kurz sind).
//
// Nimmt bewusst nur x/y/w/h statt eines eigenen Rect-Typs - jeder Screen
// hat ohnehin schon seine eigene lokale Rect-Struct fuer die Tap-Erkennung
// (Copy-Paste-Muster wie im Flightradar-Projekt), die fuer den "?"-Knopf
// einfach mit denselben Werten befuellt wird.
#include <Arduino.h>

namespace HelpPopup {
    // Zeichnet den "?"-Knopf an der angegebenen Stelle.
    void drawButton(int16_t x, int16_t y, int16_t w, int16_t h);

    // Blockiert, bis OK angetippt wird - zeigt body zentriert mit
    // Wortumbruch (nach echter Pixelbreite, nicht nach fester
    // Zeichenzahl). Der Aufrufer zeichnet seinen Bildschirm danach selbst
    // neu (alle Screens hier tun das ohnehin bei jedem Schleifendurchlauf).
    void show(const char* body);
}
