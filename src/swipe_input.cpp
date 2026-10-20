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
        if (Config::DEBUG_TOUCH_LOG) {
            Serial.printf("[touch] %lu START x=%d y=%d\n", (unsigned long)millis(), cur.x, cur.y);
        }
    }

    TouchInput::Point released;
    if (!TouchInput::wasTapped(released)) return false;

    tracking = false;
    outPoint = released;

    int16_t dx = (int16_t)(released.x - downPoint.x);
    int16_t dy = (int16_t)(released.y - downPoint.y);
    int16_t adx = (int16_t)abs(dx);
    int16_t ady = (int16_t)abs(dy);

    // Klarer Tap: Finger ist praktisch nicht gewandert.
    if (adx <= Config::TAP_MAX_DRIFT_PX && ady <= Config::TAP_MAX_DRIFT_PX) {
        dir = Direction::None;
        if (Config::DEBUG_TOUCH_LOG) {
            Serial.printf("[touch] %lu ENDE x=%d y=%d dx=%d dy=%d -> TAP\n",
                          (unsigned long)millis(), released.x, released.y, dx, dy);
        }
        return true;
    }

    // Alles andere mit ueberwiegend waagerechter Bewegung ist ein Wisch
    // (Auftrag "alles darueber ist ein Wischen") - KEINE zusaetzliche
    // Mindestweg-Schwelle mehr zusaetzlich zum Verhaeltnis-Test: die
    // vorherige Fassung verlangte GLEICHZEITIG adx>=SWIPE_MIN_DISTANCE_PX
    // UND ein strenges 2:1-Verhaeltnis - das erzeugte eine "tote Zone"
    // zwischen Tap- und Wisch-Schwelle (z.B. 15px waagerecht mit etwas
    // Hoehenversatz), in der eine echte Wisch-Geste weder als Tap noch
    // als Wisch erkannt und komplett verworfen wurde (gar keine Reaktion -
    // genau der gemeldete "5x wischen" Bug). Jetzt reicht "ueberwiegend
    // waagerecht" (adx > ady) allein, SWIPE_MIN_DISTANCE_PX ist nur noch
    // eine zusaetzliche Mindestweg-Absicherung gegen Zittern knapp ueber
    // der Tap-Schwelle, keine zweite, unabhaengig zu erfuellende Huerde.
    if (adx > ady && adx >= Config::SWIPE_MIN_DISTANCE_PX) {
        dir = (dx < 0) ? Direction::Left : Direction::Right;
        if (Config::DEBUG_TOUCH_LOG) {
            Serial.printf("[touch] %lu ENDE x=%d y=%d dx=%d dy=%d -> WISCH %s\n",
                          (unsigned long)millis(), released.x, released.y, dx, dy,
                          dir == Direction::Left ? "links" : "rechts");
        }
        return true;
    }

    // Weder eindeutiger Tap noch eindeutiger Wisch (z.B. ueberwiegend
    // SENKRECHTE Bewegung, oder waagerecht aber unter der Mindestweg-
    // Absicherung) - verwerfen, statt versehentlich einen Knopf/eine
    // Kachel auszuloesen.
    if (Config::DEBUG_TOUCH_LOG) {
        Serial.printf("[touch] %lu ENDE x=%d y=%d dx=%d dy=%d -> VERWORFEN (weder Tap noch eindeutig waagerecht)\n",
                      (unsigned long)millis(), released.x, released.y, dx, dy);
    }
    return false;
}

}
