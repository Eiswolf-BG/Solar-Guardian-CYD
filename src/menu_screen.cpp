#include "menu_screen.h"
#include "display.h"
#include "touch_input.h"
#include "color_picker_screen.h"
#include "networks_screen.h"
#include "account_screen.h"
#include "language_screen.h"
#include "reset_screen.h"
#include "yield_values_screen.h"
#include "help_popup.h"
#include "i18n.h"
#include "config.h"

namespace MenuScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    // Fuer Stift-Bedienung ausgelegt (Alex' Wunsch) - dichter und kleiner
    // als auf den fingerfreundlichen Hauptseiten, aber nicht unter dem
    // CLAUDE.md-Minimum von ca. 28px fuer Menu-Zeilen.
    constexpr int16_t ROW_H = 30;
    constexpr int16_t ROW_GAP = 6;
    constexpr int16_t TOP = 40;

    // "?"-Knopf darf im Stift-Menue kleiner sein (ca. 28px statt 32px,
    // siehe CLAUDE.md).
    constexpr Rect HELP_BTN = {(int16_t)(Config::SCREEN_WIDTH - 32), 4, 28, 28};

    // Um einen neuen Menuepunkt zu ergaenzen: hier eine neue Zeile
    // eintragen und unten im switch(action) einen neuen Fall ergaenzen.
    enum class Action { Networks, Account, ColorPicker, Language, YieldValues, Reset, Back };
    struct MenuItem {
        I18n::Str labelKey;
        Action action;
    };
    constexpr MenuItem ITEMS[] = {
        {I18n::Str::MENU_ITEM_NETWORKS, Action::Networks},
        {I18n::Str::MENU_ITEM_ACCOUNT, Action::Account},
        {I18n::Str::MENU_ITEM_COLOR, Action::ColorPicker},
        {I18n::Str::MENU_ITEM_LANGUAGE, Action::Language},
        {I18n::Str::MENU_ITEM_YIELD_VALUES, Action::YieldValues},
        {I18n::Str::MENU_ITEM_RESET, Action::Reset},
        {I18n::Str::BACK, Action::Back},
    };
    constexpr uint8_t ITEM_COUNT = sizeof(ITEMS) / sizeof(ITEMS[0]);

    Rect rowRect(uint8_t index) {
        return {10, (int16_t)(TOP + index * (ROW_H + ROW_GAP)), (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
    }
}

void run() {
    bool done = false;
    while (!done) {
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::MENU_TITLE), Config::SCREEN_WIDTH / 2, 14);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

        Rect rects[ITEM_COUNT];
        for (uint8_t i = 0; i < ITEM_COUNT; i++) {
            Rect r = rowRect(i);
            rects[i] = r;
            tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(I18n::t(ITEMS[i].labelKey), r.x + r.w / 2, r.y + r.h / 2);
            tft.setTextDatum(TL_DATUM);
        }

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            delay(15);
        }

        if (HELP_BTN.contains(tap.x, tap.y)) {
            HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_MENU));
            continue;
        }

        for (uint8_t i = 0; i < ITEM_COUNT; i++) {
            if (!rects[i].contains(tap.x, tap.y)) continue;
            switch (ITEMS[i].action) {
                case Action::Networks:
                    NetworksScreen::run();
                    break;
                case Action::Account:
                    AccountScreen::run();
                    break;
                case Action::ColorPicker:
                    ColorPickerScreen::run();
                    break;
                case Action::Language:
                    LanguageScreen::run();
                    break;
                case Action::YieldValues:
                    YieldValuesScreen::run();
                    break;
                case Action::Reset:
                    ResetScreen::run();
                    break;
                case Action::Back:
                    done = true;
                    break;
            }
            break;
        }
    }
}

}
