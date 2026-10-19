#include "tile_sprite.h"
#include "display.h"
#include "app_font.h"

namespace TileSprite {

void drawInto(int16_t x, int16_t y, int16_t w, int16_t h,
              const std::function<void(TFT_eSPI&)>& drawFn) {
    if (w <= 0 || h <= 0) return;
    TFT_eSprite sprite(&tft);
    sprite.setColorDepth(16);
    if (!sprite.createSprite(w, h)) {
        drawFn(tft);
        return;
    }
    // Ein frisch erzeugtes TFT_eSprite startet mit dem eingebauten
    // GLCD-Font, NICHT mit dem projektweiten eigenen Font (siehe
    // display.cpp::begin()) - ohne dieses setFreeFont() haette jeder
    // Text im Puffer eine komplett andere Schriftart/Metrik als der Rest
    // der App (Ursache eines schweren Anzeige-Bugs: Text erschien falsch
    // positioniert/unsichtbar, wirkte wie "keine Daten geladen", obwohl
    // die Daten laengst da waren).
    sprite.setFreeFont(&AppFont11pt);
    sprite.fillSprite(TFT_BLACK);
    drawFn(sprite);
    sprite.pushSprite(x, y);
    sprite.deleteSprite();
}

}
