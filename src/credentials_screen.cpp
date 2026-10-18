#include "credentials_screen.h"
#include "display.h"
#include "touch_input.h"
#include "keypad.h"
#include "app_config.h"
#include "sd_storage.h"
#include "help_popup.h"
#include "i18n.h"
#include "config.h"
#include <WiFi.h>
#include <time.h>

namespace CredentialsScreen {

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
    appConfig.portalEmail = "";
    appConfig.portalPassword = "";

    bool done = false;
    while (!done) {
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::CREDENTIALS_TITLE), Config::SCREEN_WIDTH / 2, 14);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

        Rect emailRow = {4, 50, (int16_t)(Config::SCREEN_WIDTH - 8), 40};
        Rect passRow  = {4, 100, (int16_t)(Config::SCREEN_WIDTH - 8), 40};
        Rect goBtn    = {4, 170, (int16_t)(Config::SCREEN_WIDTH - 8), 44};

        tft.drawRoundRect(emailRow.x, emailRow.y, emailRow.w, emailRow.h, 4, accent);
        tft.setCursor((int16_t)(emailRow.x + 8), (int16_t)(emailRow.y + 16));
        tft.print(I18n::t(I18n::Str::EMAIL_LABEL));
        tft.setCursor((int16_t)(emailRow.x + 8), (int16_t)(emailRow.y + 34));
        tft.print(appConfig.portalEmail.length() ? appConfig.portalEmail : I18n::t(I18n::Str::TAP_HERE));

        tft.drawRoundRect(passRow.x, passRow.y, passRow.w, passRow.h, 4, accent);
        tft.setCursor((int16_t)(passRow.x + 8), (int16_t)(passRow.y + 16));
        tft.print(I18n::t(I18n::Str::PASSWORD_LABEL));
        tft.setCursor((int16_t)(passRow.x + 8), (int16_t)(passRow.y + 34));
        tft.print(appConfig.portalPassword.length() ? appConfig.portalPassword : I18n::t(I18n::Str::TAP_HERE));

        bool canGo = appConfig.portalEmail.length() > 0 && appConfig.portalPassword.length() > 0;
        tft.drawRoundRect(goBtn.x, goBtn.y, goBtn.w, goBtn.h, 4, canGo ? accent : TFT_DARKGREY);
        if (canGo) tft.fillRoundRect(goBtn.x, goBtn.y, goBtn.w, goBtn.h, 4, accent);
        tft.setTextColor(canGo ? TFT_BLACK : TFT_DARKGREY, canGo ? accent : TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::GO_BUTTON), goBtn.x + goBtn.w / 2, goBtn.y + goBtn.h / 2);
        tft.setTextDatum(TL_DATUM);

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            delay(15);
        }

        if (HELP_BTN.contains(tap.x, tap.y)) {
            HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_CREDENTIALS));
        } else if (emailRow.contains(tap.x, tap.y)) {
            bool confirmed = false;
            String v = Keypad::run(I18n::t(I18n::Str::EMAIL_PROMPT), appConfig.portalEmail, confirmed);
            if (confirmed) appConfig.portalEmail = v;
        } else if (passRow.contains(tap.x, tap.y)) {
            bool confirmed = false;
            String v = Keypad::run(I18n::t(I18n::Str::PASSWORD_PROMPT), appConfig.portalPassword, confirmed);
            if (confirmed) appConfig.portalPassword = v;
        } else if (canGo && goBtn.contains(tap.x, tap.y)) {
            SdStorage::ensureRootDir();
            appConfig.setupComplete = true;
            AppConfigStore::save();

            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(accent, TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(I18n::t(I18n::Str::FETCHING_TIME), Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT / 2);
            tft.setTextDatum(TL_DATUM);
            configTzTime(Config::TIMEZONE_POSIX, Config::NTP_SERVER);
            // Kurz auf eine gueltige Zeit warten, aber nicht endlos -
            // main_screen.cpp zeigt "--:--" an, falls es bis hierher nicht
            // geklappt hat, und die Zeit kommt dann spaeter im Hintergrund.
            struct tm timeinfo;
            uint32_t startMs = millis();
            while (!getLocalTime(&timeinfo, 100) && millis() - startMs < 5000) {
                delay(100);
            }

            done = true;
        }
    }
}

}
