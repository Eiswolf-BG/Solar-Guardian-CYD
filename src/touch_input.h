#pragma once
// Touch-Eingabe (XPT2046). Liefert sowohl rohe als auch - nach der
// Kalibrierung - auf Bildschirmkoordinaten (320x240) umgerechnete
// Tap-Positionen.
#include <Arduino.h>

namespace TouchInput {

    struct Point {
        int16_t x = 0;
        int16_t y = 0;
        bool touched = false;
    };

    // SPI/Chip initialisieren - einmal beim Start aufrufen.
    void begin();

    // Unverarbeiteter Rohwert direkt vom Touch-Chip (0-4095 je Achse),
    // OHNE jede Kalibrierung/Achsenzuordnung - wird von
    // calibration_screen.cpp benutzt, um die drei Eck-Punkte einzusammeln.
    Point rawPoint();

    // Setzt die aus der Kalibrierung ermittelte Achsenzuordnung (siehe
    // app_config.h::touchSwapXY) und die Roh-Wertebereiche je Bildschirm-
    // achse. Wirkt sofort auf mappedPoint()/wasTapped().
    void setCalibration(bool swapXY, int16_t xRawMin, int16_t xRawMax,
                         int16_t yRawMin, int16_t yRawMax);

    // In Bildschirmkoordinaten (0..319 / 0..239) umgerechneter aktueller
    // Touch-Punkt, unter Beruecksichtigung der per setCalibration() gesetzten
    // Achsenzuordnung.
    Point mappedPoint();

    // true, wenn seit dem letzten Aufruf ein Tap (Antippen + Loslassen)
    // abgeschlossen wurde - liefert die Position des Loslassens. Nach dem
    // in allen Bildschirmen gleichen Muster:
    //   TouchInput::Point tap;
    //   if (TouchInput::wasTapped(tap)) { ... }
    bool wasTapped(Point& outPoint);

}
