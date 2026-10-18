#pragma once
// Die per Wisch-Geste erreichbaren Hauptseiten (siehe main_screen.cpp).
// Um eine neue Seite zu ergaenzen: hier im PAGES-Array einen neuen
// Eintrag mit eigener draw()-Funktion (und Hilfetext-Schluessel)
// ergaenzen, optional update() fuer wiederkehrende Aktualisierungen
// (z.B. API-Abfragen) und onTap() fuer eigene Tipp-Flaechen im
// Seitenbereich.
#include <Arduino.h>
#include "i18n.h"

namespace Pages {

    struct Page {
        I18n::Str titleKey;
        // areaTop/areaHeight = der Bereich UNTER der festen Kopfzeile, in
        // den die Seite zeichnen darf (siehe main_screen.cpp).
        void (*draw)(int16_t areaTop, int16_t areaHeight);
        // nullptr = keine wiederkehrende Aktualisierung noetig (reine
        // Platzhalterseiten). Sonst: wird jede loop()-Iteration
        // aufgerufen, true = seit dem letzten Aufruf hat sich etwas
        // geaendert - Core 1 zeichnet die Seite dann neu (kein Flackern,
        // kein unnoetiges Neuzeichnen ohne Aenderung).
        bool (*update)();
        // nullptr = die Seite hat keine eigene Tipp-Flaeche im
        // Seitenbereich. Sonst: bei einem Tap (kein Wisch), der nicht auf
        // Kopfzeile/"?"-Knopf fiel, aufgerufen - true = behandelt, Core 1
        // zeichnet danach neu.
        bool (*onTap)(int16_t x, int16_t y, int16_t areaTop, int16_t areaHeight);
        // Hilfetext fuer den "?"-Knopf (siehe main_screen.cpp).
        I18n::Str helpKey;
    };

    uint8_t count();
    const Page& get(uint8_t index);
}
