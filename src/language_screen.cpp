#include "language_screen.h"
#include "display.h"
#include "touch_input.h"
#include "app_config.h"
#include "fetch_task.h"
#include "help_popup.h"
#include "i18n.h"
#include "config.h"

namespace LanguageScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr Rect HELP_BTN = {(int16_t)(Config::SCREEN_WIDTH - 36), 4, 32, 32};
}

void run() {
    bool done = false;
    while (!done) {
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::LANGUAGE_SCREEN_TITLE), Config::SCREEN_WIDTH / 2, 14);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

        Rect germanBtn = {10, 60, (int16_t)(Config::SCREEN_WIDTH - 20), 44};
        Rect englishBtn = {10, 114, (int16_t)(Config::SCREEN_WIDTH - 20), 44};
        Rect backBtn = {10, 180, (int16_t)(Config::SCREEN_WIDTH - 20), 40};

        tft.drawRoundRect(germanBtn.x, germanBtn.y, germanBtn.w, germanBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::GERMAN_LABEL), germanBtn.x + germanBtn.w / 2, germanBtn.y + germanBtn.h / 2);
        if (appConfig.language == 0) {
            tft.setTextDatum(MR_DATUM);
            tft.drawString("✓", (int16_t)(germanBtn.x + germanBtn.w - 10), germanBtn.y + germanBtn.h / 2);
        }
        tft.setTextDatum(TL_DATUM);

        tft.drawRoundRect(englishBtn.x, englishBtn.y, englishBtn.w, englishBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::ENGLISH_LABEL), englishBtn.x + englishBtn.w / 2, englishBtn.y + englishBtn.h / 2);
        if (appConfig.language == 1) {
            tft.setTextDatum(MR_DATUM);
            tft.drawString("✓", (int16_t)(englishBtn.x + englishBtn.w - 10), englishBtn.y + englishBtn.h / 2);
        }
        tft.setTextDatum(TL_DATUM);

        tft.drawRoundRect(backBtn.x, backBtn.y, backBtn.w, backBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::BACK), backBtn.x + backBtn.w / 2, backBtn.y + backBtn.h / 2);
        tft.setTextDatum(TL_DATUM);

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            delay(15);
        }

        if (HELP_BTN.contains(tap.x, tap.y)) {
            HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_LANGUAGE));
        } else if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        } else if (germanBtn.contains(tap.x, tap.y) && appConfig.language != 0) {
            appConfig.language = 0;
            AppConfigStore::save();
            FetchTask::sendCommand(FetchTask::Command::LanguageChanged);
        } else if (englishBtn.contains(tap.x, tap.y) && appConfig.language != 1) {
            appConfig.language = 1;
            AppConfigStore::save();
            FetchTask::sendCommand(FetchTask::Command::LanguageChanged);
        }
    }
}

}
