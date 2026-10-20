#include "menu_screen.h"
#include "display.h"
#include "touch_input.h"
#include "color_picker_screen.h"
#include "brightness_screen.h"
#include "networks_screen.h"
#include "account_screen.h"
#include "language_screen.h"
#include "reset_screen.h"
#include "yield_values_screen.h"
#include "files_screen.h"
#include "help_popup.h"
#include "layout_check.h"
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
    // CLAUDE.md-Minimum von ca. 28px fuer Menu-Zeilen. "Standort" ist kein
    // eigener Menuepunkt mehr (Auftrag "Standort automatisch, notfalls
    // einmalig per Eingabe") - der Standort-Screen ist seitdem nur noch
    // ueber die Wetterkachel/Wochenansicht erreichbar, siehe
    // home_page.cpp/week_view_screen.cpp. 7 Eintraege, Zeilenhoehe/
    // -abstand gleichmaessig auf den verfuegbaren Platz verteilt (mind.
    // 8px Abstand unten, siehe CLAUDE.md Layout-Regel).
    constexpr int16_t ROW_GAP = 8;
    constexpr int16_t TOP = 40;
    constexpr int16_t BOTTOM_MARGIN = 8;

    // "?"-Knopf darf im Stift-Menue kleiner sein (ca. 28px statt 32px,
    // siehe CLAUDE.md).
    constexpr Rect HELP_BTN = {(int16_t)(Config::SCREEN_WIDTH - 32), 4, 28, 28};

    // Um einen neuen Menuepunkt zu ergaenzen: hier eine neue Zeile
    // eintragen und unten im switch(action) einen neuen Fall ergaenzen.
    // Die Zeilenhoehe wird rechnerisch aus ITEM_COUNT ermittelt (statt
    // fest verdrahtet), damit das Menue bei einer geaenderten Zeilenzahl
    // automatisch gleichmaessig verteilt bleibt statt zu ueberlaufen.
    enum class Action { Networks, Account, ColorPicker, Brightness, Language, YieldValues, Files, Reset, Back };
    struct MenuItem {
        I18n::Str labelKey;
        Action action;
    };
    constexpr MenuItem ITEMS[] = {
        {I18n::Str::MENU_ITEM_NETWORKS, Action::Networks},
        {I18n::Str::MENU_ITEM_ACCOUNT, Action::Account},
        {I18n::Str::MENU_ITEM_COLOR, Action::ColorPicker},
        {I18n::Str::MENU_ITEM_BRIGHTNESS, Action::Brightness},
        {I18n::Str::MENU_ITEM_LANGUAGE, Action::Language},
        {I18n::Str::MENU_ITEM_YIELD_VALUES, Action::YieldValues},
        {I18n::Str::MENU_ITEM_FILES, Action::Files},
        {I18n::Str::MENU_ITEM_RESET, Action::Reset},
        {I18n::Str::BACK, Action::Back},
    };
    constexpr uint8_t ITEM_COUNT = sizeof(ITEMS) / sizeof(ITEMS[0]);

    int16_t rowHeight() {
        int16_t available = (int16_t)(Config::SCREEN_HEIGHT - TOP - BOTTOM_MARGIN);
        int16_t totalGaps = (int16_t)((ITEM_COUNT - 1) * ROW_GAP);
        return (int16_t)((available - totalGaps) / ITEM_COUNT);
    }

    Rect rowRect(uint8_t index) {
        int16_t h = rowHeight();
        return {10, (int16_t)(TOP + index * (h + ROW_GAP)), (int16_t)(Config::SCREEN_WIDTH - 20), h};
    }
}

void run() {
    bool done = false;
    while (!done) {
        LayoutCheck::begin("Menue");
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        // Immer explizit setzen, nie vom Aufrufer erben (siehe CLAUDE.md
        // "Schriftzustand") - menu_screen.cpp rief bisher nie
        // tft.setTextSize() selbst auf und verliess sich stillschweigend
        // darauf, dass Size 1 vom vorherigen Bildschirm uebrig blieb.
        tft.setTextSize(1);
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::MENU_TITLE), Config::SCREEN_WIDTH / 2, 14);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);
        LayoutCheck::rect("Titel", 0, 4, (int16_t)(Config::SCREEN_WIDTH - 36), 24);
        LayoutCheck::rect("Hilfe-Knopf", HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

        Rect rects[ITEM_COUNT];
        for (uint8_t i = 0; i < ITEM_COUNT; i++) {
            Rect r = rowRect(i);
            rects[i] = r;
            tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(I18n::t(ITEMS[i].labelKey), r.x + r.w / 2, r.y + r.h / 2);
            tft.setTextDatum(TL_DATUM);
            LayoutCheck::rect("Menuzeile", r.x, r.y, r.w, r.h);
        }
        LayoutCheck::finish();

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
                case Action::Brightness:
                    BrightnessScreen::run();
                    break;
                case Action::Language:
                    LanguageScreen::run();
                    break;
                case Action::YieldValues:
                    YieldValuesScreen::run();
                    break;
                case Action::Files:
                    FilesScreen::run();
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
