#include "brightness_screen.h"
#include "display.h"
#include "touch_input.h"
#include "app_config.h"
#include "tile_sprite.h"
#include "help_popup.h"
#include "layout_check.h"
#include "i18n.h"
#include "config.h"

namespace BrightnessScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr Rect HELP_BTN = {(int16_t)(Config::SCREEN_WIDTH - 36), 4, 32, 32};
    constexpr int16_t PCT_ZONE_TOP = 40;
    constexpr int16_t PCT_ZONE_H = 36;
    constexpr int16_t SLIDER_TOP = 92;
    constexpr int16_t SLIDER_H = 40;
    constexpr int16_t SLIDER_SIDE_MARGIN = 16;

    Rect sliderRect() {
        return {SLIDER_SIDE_MARGIN, SLIDER_TOP, (int16_t)(Config::SCREEN_WIDTH - 2 * SLIDER_SIDE_MARGIN), SLIDER_H};
    }

    // Wandelt eine X-Bildschirmposition innerhalb des Reglers in einen
    // Prozentwert um, geklemmt auf Config::BRIGHTNESS_MIN_PERCENT..100
    // (Auftrag "nie komplett dunkel, Mindestwert 5%").
    uint8_t percentFromX(int16_t x, const Rect& r) {
        int32_t rel = x - r.x;
        if (rel < 0) rel = 0;
        if (rel > r.w) rel = r.w;
        int32_t pct = (rel * 100) / r.w;
        if (pct < Config::BRIGHTNESS_MIN_PERCENT) pct = Config::BRIGHTNESS_MIN_PERCENT;
        if (pct > 100) pct = 100;
        return (uint8_t)pct;
    }

    // Zeichnet NUR die Fuellung in den Puffer - der Rahmen bleibt stehen
    // (einmal auf dem echten Bildschirm gezeichnet), nur der innere
    // Fuellstand aendert sich beim Ziehen (Auftrag "darf beim Ziehen
    // nicht flackern").
    void renderSliderFill(TFT_eSPI& t, int16_t innerW, int16_t innerH, uint8_t pct, uint16_t accent) {
        int16_t fillW = (int16_t)(((int32_t)innerW * pct) / 100);
        if (fillW > 0) t.fillRoundRect(0, 0, fillW, innerH, 4, accent);
    }
}

void run() {
    bool done = false;
    bool dragging = false;
    String lastFillSig = "\x01";
    String lastPctSig = "\x01";

    while (!done) {
        LayoutCheck::begin("Helligkeit");
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::BRIGHTNESS_TITLE), Config::SCREEN_WIDTH / 2, 14);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);
        LayoutCheck::rect("Titel", 0, 4, (int16_t)(Config::SCREEN_WIDTH - 44), 24);
        LayoutCheck::rect("Hilfe-Knopf", HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

        Rect sr = sliderRect();
        tft.drawRoundRect(sr.x, sr.y, sr.w, sr.h, 6, accent);
        LayoutCheck::rect("Schieberegler", sr.x, sr.y, sr.w, sr.h);

        Rect backBtn = {16, (int16_t)(SLIDER_TOP + SLIDER_H + 24), (int16_t)(Config::SCREEN_WIDTH - 32), 44};
        tft.drawRoundRect(backBtn.x, backBtn.y, backBtn.w, backBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::BACK), (int16_t)(backBtn.x + backBtn.w / 2), (int16_t)(backBtn.y + backBtn.h / 2));
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Zurueck-Knopf", backBtn.x, backBtn.y, backBtn.w, backBtn.h);
        LayoutCheck::finish();

        // Prozentzahl + Reglerfuellung werden beim Betreten einmal ueber
        // den Puffer gezeichnet, danach nur noch bei echter Aenderung
        // (Auftrag "Flackerfrei").
        lastFillSig = "\x01";
        lastPctSig = "\x01";

        bool pageDone = false;
        while (!pageDone) {
            uint8_t pct = appConfig.brightnessPercent;

            String pctSig = String(pct);
            if (pctSig != lastPctSig) {
                lastPctSig = pctSig;
                char buf[8];
                snprintf(buf, sizeof(buf), "%u %%", (unsigned)pct);
                int16_t zoneW = (int16_t)(Config::SCREEN_WIDTH - 32);
                TileSprite::drawInto(16, PCT_ZONE_TOP, zoneW, PCT_ZONE_H, [&](TFT_eSPI& t) {
                    t.setTextSize(2);
                    t.setTextColor(accent, TFT_BLACK);
                    t.setTextDatum(TC_DATUM);
                    t.drawString(buf, (int16_t)(zoneW / 2), 0);
                    t.setTextDatum(TL_DATUM);
                    t.setTextSize(1);
                });
            }

            String fillSig = String(pct);
            if (fillSig != lastFillSig) {
                lastFillSig = fillSig;
                constexpr int16_t PAD = 3;
                int16_t innerW = (int16_t)(sr.w - 2 * PAD);
                int16_t innerH = (int16_t)(sr.h - 2 * PAD);
                TileSprite::drawInto((int16_t)(sr.x + PAD), (int16_t)(sr.y + PAD), innerW, innerH, [&](TFT_eSPI& t) {
                    renderSliderFill(t, innerW, innerH, pct, accent);
                });
            }

            TouchInput::Point cur = TouchInput::mappedPoint();
            if (cur.touched) {
                if (!dragging && sr.contains(cur.x, cur.y)) dragging = true;
                if (dragging) {
                    uint8_t newPct = percentFromX(cur.x, sr);
                    if (newPct != appConfig.brightnessPercent) {
                        appConfig.brightnessPercent = newPct;
                        Display::applyBrightness();
                    }
                }
            } else if (dragging) {
                dragging = false;
                AppConfigStore::save();
            }

            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) {
                // Ein Loslassen, das zum Ziehen gehoerte (Finger startete
                // auf dem Regler), zaehlt NICHT zusaetzlich als Tap auf
                // Hilfe/Zurueck - verhindert, dass ein Ziehen bis an den
                // Bildschirmrand versehentlich einen Knopf ausloest.
                if (!sr.contains(tap.x, tap.y)) {
                    if (HELP_BTN.contains(tap.x, tap.y)) {
                        HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_BRIGHTNESS));
                        pageDone = true;
                    } else if (backBtn.contains(tap.x, tap.y)) {
                        done = true;
                        pageDone = true;
                    }
                }
            }

            delay(15);
        }
    }
}

}
