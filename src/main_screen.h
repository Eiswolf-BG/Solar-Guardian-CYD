#pragma once
// Hauptbildschirm: Uhrzeit, WLAN-Empfang, "Solar Guardian"-Knopf zum
// Menue. Nutzt das enter()/tick()-Muster (nicht ein eigenes
// Endlos-while()) - so bleibt main.cpp's loop() frei fuer spaeter
// hinzukommende Hintergrundaufgaben.
#include <Arduino.h>

namespace MainScreen {
    // Einmal aufrufen, sobald der Hauptbildschirm zum ersten Mal gezeigt
    // wird (auch nach jedem Verlassen des Menues erneut, damit der
    // Bildschirm komplett neu aufgebaut wird).
    void enter();

    // In jeder loop()-Iteration aufrufen. Aktualisiert Uhrzeit/WLAN-Balken
    // bei Bedarf, prueft auf Taps (oeffnet das Menue) und versucht im
    // Hintergrund erneut, sich mit dem gespeicherten WLAN zu verbinden,
    // falls die Verbindung (noch) fehlt.
    void tick();
}
