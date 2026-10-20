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
    // None bei einem klaren Tap (Finger bewegte sich hoechstens
    // Config::TAP_MAX_DRIFT_PX), sonst Left/Right bei JEDER ueberwiegend
    // waagerechten Bewegung (waagerechter Weg groesser als senkrechter)
    // ab Config::SWIPE_MIN_DISTANCE_PX - "alles darueber ist ein
    // Wischen" (Auftrag). Liefert false sowohl wenn (noch) keine
    // Beruehrung abgeschlossen wurde ALS AUCH wenn die Bewegung
    // ueberwiegend SENKRECHT war - wird dann bewusst verworfen statt
    // versehentlich einen Knopf/eine Kachel auszuloesen.
    bool poll(Direction& dir, TouchInput::Point& outPoint);
}
