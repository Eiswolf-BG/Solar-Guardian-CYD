#include "swipe_input.h"
#include "config.h"

namespace SwipeInput {

namespace {
    bool tracking = false;
    TouchInput::Point downPoint;
}

bool poll(Direction& dir, TouchInput::Point& outPoint) {
    TouchInput::Point cur = TouchInput::mappedPoint();
    if (cur.touched && !tracking) {
        tracking = true;
        downPoint = cur;
    }

    TouchInput::Point released;
    if (!TouchInput::wasTapped(released)) return false;

    tracking = false;
    outPoint = released;

    int16_t dx = (int16_t)(released.x - downPoint.x);
    int16_t dy = (int16_t)(released.y - downPoint.y);
    int16_t adx = (int16_t)abs(dx);
    int16_t ady = (int16_t)abs(dy);

    // Nur eine ueberwiegend waagerechte Bewegung ab der Schwelle gilt als
    // Wisch - ein normaler (auch leicht zittriger) Tap darf nie
    // versehentlich die Seite wechseln.
    if (adx >= Config::SWIPE_THRESHOLD_PX && adx > ady) {
        dir = (dx < 0) ? Direction::Left : Direction::Right;
    } else {
        dir = Direction::None;
    }
    return true;
}

}
