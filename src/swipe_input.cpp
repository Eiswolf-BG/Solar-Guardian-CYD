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

    // Klarer Tap: Finger ist praktisch nicht gewandert (Auftrag
    // "Wischgeste empfindlicher").
    if (adx <= Config::TAP_MAX_DRIFT_PX && ady <= Config::TAP_MAX_DRIFT_PX) {
        dir = Direction::None;
        return true;
    }

    // Klarer Wisch: genug Weg UND ueberwiegend waagerecht (waagerecht
    // mindestens SWIPE_HORIZONTAL_RATIO-mal so gross wie senkrecht -
    // leicht schraege Wischer bleiben damit erlaubt, ohne komplett
    // waagerecht sein zu muessen).
    if (adx >= Config::SWIPE_MIN_DISTANCE_PX && adx >= ady * Config::SWIPE_HORIZONTAL_RATIO) {
        dir = (dx < 0) ? Direction::Left : Direction::Right;
        return true;
    }

    // Weder eindeutiger Tap noch eindeutiger Wisch (z.B. zu weit fuer
    // einen Tap, aber zu schraeg/zu kurz fuer einen Wisch) - verwerfen,
    // statt versehentlich einen Knopf/eine Kachel auszuloesen.
    return false;
}

}
