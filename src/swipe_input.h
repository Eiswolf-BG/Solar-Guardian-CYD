#pragma once
// Erkennt Wisch-Gesten (links/rechts) zusaetzlich zu reinen Taps - Basis
// fuer die Seiten-Navigation auf dem Hauptbildschirm. Prinzip uebernommen
// aus eiswolfs-toolbox (src/swipe_input.cpp), angepasst an unser
// TouchInput-Modul. Verfolgt den Beruehrungs-START-Punkt selbst, um die
// Wisch-Richtung zu bestimmen (TouchInput meldet nur den Loslass-Punkt).
#include "touch_input.h"

namespace SwipeInput {
    enum class Direction { None, Left, Right };

    // Einmal pro loop()-Durchlauf aufrufen. Liefert true genau einmal pro
    // abgeschlossener Beruehrung (Loslassen) - dir ist Left/Right bei einer
    // ueberwiegend waagerechten Bewegung ab Config::SWIPE_THRESHOLD_PX,
    // sonst None (dann war es ein normaler Tap, outPoint der Tap-Punkt).
    bool poll(Direction& dir, TouchInput::Point& outPoint);
}
