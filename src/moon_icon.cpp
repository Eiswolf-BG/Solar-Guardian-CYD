#include "moon_icon.h"

namespace MoonIcon {

void draw(TFT_eSPI& t, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    t.fillCircle(cx, cy, r, color);
    // Versatz 60% des Radius nach rechts - "beisst" einen Teil des Kreises
    // schwarz weg und laesst eine nach links geoeffnete Sichel stehen.
    t.fillCircle((int16_t)(cx + (r * 6) / 10), cy, r, TFT_BLACK);
}

}
