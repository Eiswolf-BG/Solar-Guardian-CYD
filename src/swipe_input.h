#pragma once
// Erkennt Wisch-Gesten (links/rechts) zusaetzlich zu reinen Taps - Basis
// fuer die Seiten-Navigation auf dem Hauptbildschirm. Prinzip uebernommen
// aus eiswolfs-toolbox (src/swipe_input.cpp), angepasst an unser
// TouchInput-Modul. Verfolgt den Beruehrungs-START-Punkt selbst, um die
// Wisch-Richtung zu bestimmen (TouchInput meldet nur den Loslass-Punkt).
#include "touch_input.h"

namespace SwipeInput {
    enum class Direction { None, Left, Right };

    // Einmal pro loop()-Durchlauf aufrufen. Liefert true bei einer
    // EINDEUTIG klassifizierbaren abgeschlossenen Beruehrung: dir ist
    // Left/Right bei einer ueberwiegend waagerechten Bewegung ab
    // Config::SWIPE_MIN_DISTANCE_PX, None bei einem klaren Tap (Finger
    // bewegte sich hoechstens Config::TAP_MAX_DRIFT_PX). Liefert false
    // sowohl wenn (noch) keine Beruehrung abgeschlossen wurde ALS AUCH
    // wenn die Bewegung ambig war (zu weit fuer einen Tap, aber zu kurz/
    // zu schraeg fuer einen Wisch) - wird dann bewusst verworfen statt
    // versehentlich einen Knopf/eine Kachel auszuloesen.
    bool poll(Direction& dir, TouchInput::Point& outPoint);
}
