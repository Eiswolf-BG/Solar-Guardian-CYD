#include "color_picker_screen.h"
#include "display.h"
#include "touch_input.h"
#include "app_config.h"
#include "help_popup.h"
#include "i18n.h"
#include "config.h"
#include "web_server.h"

namespace ColorPickerScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    // 2 Spalten x 4 Reihen statt 4x2 - passt besser ins schmalere
    // Hochformat (240px Breite) als eine breite 4-Spalten-Reihe.
    constexpr uint8_t COLS = 2;
    constexpr int16_t SWATCH = 50;
    constexpr int16_t GAP = 6;
    constexpr int16_t GRID_LEFT = (Config::SCREEN_WIDTH - (COLS * SWATCH + (COLS - 1) * GAP)) / 2;
    constexpr int16_t GRID_TOP = 36;

    constexpr Rect HELP_BTN = {(int16_t)(Config::SCREEN_WIDTH - 36), 4, 32, 32};

    Rect swatchRect(uint8_t index) {
        uint8_t col = index % COLS;
        uint8_t row = index / COLS;
        return {(int16_t)(GRID_LEFT + col * (SWATCH + GAP)), (int16_t)(GRID_TOP + row * (SWATCH + GAP)), SWATCH, SWATCH};
    }
}

void run() {
    bool done = false;
    while (!done) {
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::COLOR_PICKER_TITLE), Config::SCREEN_WIDTH / 2, 14);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

        Rect rects[Config::COLOR_COUNT];
        for (uint8_t i = 0; i < Config::COLOR_COUNT; i++) {
            Rect r = swatchRect(i);
            rects[i] = r;
            tft.fillRoundRect(r.x, r.y, r.w, r.h, 6, Config::COLOR_PALETTE[i]);
            if (i == appConfig.systemColorIndex) {
                tft.drawRoundRect(r.x - 3, r.y - 3, r.w + 6, r.h + 6, 8, accent);
            }
        }

        constexpr uint8_t ROWS = Config::COLOR_COUNT / COLS;
        Rect backBtn = {4, (int16_t)(GRID_TOP + ROWS * (SWATCH + GAP) - GAP + 10), (int16_t)(Config::SCREEN_WIDTH - 8), 36};
        tft.drawRoundRect(backBtn.x, backBtn.y, backBtn.w, backBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::BACK), backBtn.x + backBtn.w / 2, backBtn.y + backBtn.h / 2);
        tft.setTextDatum(TL_DATUM);

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            WebServer_::tick();
            delay(15);
        }

        if (HELP_BTN.contains(tap.x, tap.y)) {
            HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_COLOR_PICKER));
            continue;
        }
        if (backBtn.contains(tap.x, tap.y)) {
            done = true;
            continue;
        }
        for (uint8_t i = 0; i < Config::COLOR_COUNT; i++) {
            if (rects[i].contains(tap.x, tap.y)) {
                appConfig.systemColorIndex = i;
                AppConfigStore::save();
                break;
            }
        }
    }
}

}
