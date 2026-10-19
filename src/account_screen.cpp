#include "account_screen.h"
#include "display.h"
#include "touch_input.h"
#include "keypad.h"
#include "app_config.h"
#include "fetch_task.h"
#include "account_bridge.h"
#include "credentials_screen.h"
#include "serial_number_screen.h"
#include "help_popup.h"
#include "layout_utils.h"
#include "layout_check.h"
#include "i18n.h"
#include "config.h"

namespace AccountScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr Rect HELP_BTN = {(int16_t)(Config::SCREEN_WIDTH - 36), 4, 32, 32};

    // Warn-Bildschirm vor dem Loeschen - eigene kleine Schleife, damit der
    // Aufrufer (run()) einfach weiss, ob wirklich geloescht werden soll.
    bool confirmDeleteAccount() {
        uint16_t accent = Display::systemColor();
        bool choice = false;
        bool answered = false;
        while (!answered) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setTextDatum(TC_DATUM);
            tft.drawString(I18n::t(I18n::Str::DELETE_ACCOUNT_WARNING_TITLE), Config::SCREEN_WIDTH / 2, 20);
            tft.setTextColor(accent, TFT_BLACK);
            tft.setTextDatum(TL_DATUM);

            // Einfacher Wortumbruch fuer den Warntext (kurz genug fuer 2-3
            // Zeilen auf 240px Breite).
            String body = I18n::t(I18n::Str::DELETE_ACCOUNT_WARNING_BODY);
            constexpr int16_t WRAP_W = 210;
            int16_t y = 50;
            String line = "";
            int16_t lastSpace = -1;
            String remaining = body;
            while (remaining.length() > 0) {
                int16_t i = 0;
                line = "";
                lastSpace = -1;
                while (i < (int16_t)remaining.length()) {
                    String candidate = line + remaining[i];
                    if (tft.textWidth(candidate) > WRAP_W) break;
                    line = candidate;
                    if (remaining[i] == ' ') lastSpace = i;
                    i++;
                }
                if (i >= (int16_t)remaining.length()) {
                    tft.setTextDatum(TC_DATUM);
                    tft.drawString(line, Config::SCREEN_WIDTH / 2, y);
                    tft.setTextDatum(TL_DATUM);
                    break;
                }
                String thisLine = (lastSpace >= 0) ? remaining.substring(0, lastSpace) : line;
                tft.setTextDatum(TC_DATUM);
                tft.drawString(thisLine, Config::SCREEN_WIDTH / 2, y);
                tft.setTextDatum(TL_DATUM);
                remaining = (lastSpace >= 0) ? remaining.substring(lastSpace + 1) : remaining.substring(line.length());
                y += 20;
            }

            Rect cancelBtn = {10, 180, (int16_t)(Config::SCREEN_WIDTH - 20), 44};
            Rect deleteBtn = {10, 236, (int16_t)(Config::SCREEN_WIDTH - 20), 44};
            tft.drawRoundRect(cancelBtn.x, cancelBtn.y, cancelBtn.w, cancelBtn.h, 4, accent);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(I18n::t(I18n::Str::CANCEL), cancelBtn.x + cancelBtn.w / 2, cancelBtn.y + cancelBtn.h / 2);
            tft.setTextDatum(TL_DATUM);

            tft.drawRoundRect(deleteBtn.x, deleteBtn.y, deleteBtn.w, deleteBtn.h, 4, TFT_RED);
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(I18n::t(I18n::Str::DELETE_BUTTON), deleteBtn.x + deleteBtn.w / 2, deleteBtn.y + deleteBtn.h / 2);
            tft.setTextDatum(TL_DATUM);

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                delay(15);
            }
            if (cancelBtn.contains(tap.x, tap.y)) { choice = false; answered = true; }
            else if (deleteBtn.contains(tap.x, tap.y)) { choice = true; answered = true; }
        }
        return choice;
    }
}

void run() {
    bool done = false;
    while (!done) {
        LayoutCheck::begin("Konto");
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::ACCOUNT_TITLE), Config::SCREEN_WIDTH / 2, 14);
        // fitText() statt festem drawString() - eine lange E-Mail darf
        // den Bildschirmrand nie ueberschreiten (Layout-Regel).
        tft.setTextSize(1);
        constexpr uint8_t EMAIL_SIZES[] = {1};
        String emailLabel = appConfig.portalEmail.length() ? appConfig.portalEmail : String(I18n::t(I18n::Str::TAP_HERE));
        LayoutUtils::FitResult emailFit = LayoutUtils::fitText(tft, emailLabel, (int16_t)(Config::SCREEN_WIDTH - 16), 18, EMAIL_SIZES, 1);
        tft.drawString(emailFit.text, Config::SCREEN_WIDTH / 2, 38);
        LayoutCheck::rect("E-Mail-Zeile", 8, 38, (int16_t)(Config::SCREEN_WIDTH - 16), 17);
        tft.drawString(String(I18n::t(I18n::Str::PASSWORD_LABEL)) + " ********", Config::SCREEN_WIDTH / 2, 56);
        LayoutCheck::rect("Passwort-Zeile", 8, 56, (int16_t)(Config::SCREEN_WIDTH - 16), 17);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);
        LayoutCheck::rect("Titel", 0, 4, (int16_t)(Config::SCREEN_WIDTH - 44), 24);
        LayoutCheck::rect("Hilfe-Knopf", HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

        // Seriennummer-Feld (Auftrag "Seriennummer zu Konto verschieben") -
        // Beschriftung oben, aktueller Wert darunter, antippbar wie die
        // anderen Felder, oeffnet SerialNumberScreen::runFromAccount().
        constexpr int16_t SERIAL_LABEL_Y = 80;
        constexpr int16_t SERIAL_VALUE_Y = 98;
        Rect serialRow = {10, 76, (int16_t)(Config::SCREEN_WIDTH - 20), 42};
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(I18n::t(I18n::Str::SERIAL_NUMBER_TITLE), Config::SCREEN_WIDTH / 2, SERIAL_LABEL_Y);
        String serialValue = appConfig.deviceNo.length() ? appConfig.deviceNo : String(I18n::t(I18n::Str::TAP_HERE));
        tft.drawString(serialValue, Config::SCREEN_WIDTH / 2, SERIAL_VALUE_Y);
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Seriennummer-Feld", serialRow.x, serialRow.y, serialRow.w, serialRow.h);

        constexpr int16_t TOP = 126;
        constexpr int16_t ROW_H = 40;
        constexpr int16_t ROW_GAP = 8;
        Rect changeUserBtn = {10, TOP, (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
        Rect changePassBtn = {10, (int16_t)(TOP + (ROW_H + ROW_GAP)), (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
        Rect deleteBtn      = {10, (int16_t)(TOP + 2 * (ROW_H + ROW_GAP)), (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
        Rect backBtn        = {10, (int16_t)(TOP + 3 * (ROW_H + ROW_GAP)), (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};

        tft.drawRoundRect(changeUserBtn.x, changeUserBtn.y, changeUserBtn.w, changeUserBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::CHANGE_USERNAME_BUTTON), changeUserBtn.x + changeUserBtn.w / 2, changeUserBtn.y + changeUserBtn.h / 2);
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Benutzername-aendern-Knopf", changeUserBtn.x, changeUserBtn.y, changeUserBtn.w, changeUserBtn.h);

        tft.drawRoundRect(changePassBtn.x, changePassBtn.y, changePassBtn.w, changePassBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::CHANGE_PASSWORD_BUTTON), changePassBtn.x + changePassBtn.w / 2, changePassBtn.y + changePassBtn.h / 2);
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Passwort-aendern-Knopf", changePassBtn.x, changePassBtn.y, changePassBtn.w, changePassBtn.h);

        tft.drawRoundRect(deleteBtn.x, deleteBtn.y, deleteBtn.w, deleteBtn.h, 4, TFT_RED);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::DELETE_ACCOUNT_BUTTON), deleteBtn.x + deleteBtn.w / 2, deleteBtn.y + deleteBtn.h / 2);
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Konto-loeschen-Knopf", deleteBtn.x, deleteBtn.y, deleteBtn.w, deleteBtn.h);

        tft.drawRoundRect(backBtn.x, backBtn.y, backBtn.w, backBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::BACK), backBtn.x + backBtn.w / 2, backBtn.y + backBtn.h / 2);
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Zurueck-Knopf", backBtn.x, backBtn.y, backBtn.w, backBtn.h);
        LayoutCheck::finish();

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            delay(15);
        }

        if (HELP_BTN.contains(tap.x, tap.y)) {
            HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_ACCOUNT));
        } else if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        } else if (serialRow.contains(tap.x, tap.y)) {
            // Oeffnet den vorhandenen Seriennummer-Screen, vorbelegt mit
            // dem gespeicherten Wert - speichert/verwirft selbststaendig
            // (siehe serial_number_screen.cpp::runFromAccount()), landet
            // danach wieder hier im Konto-Screen.
            SerialNumberScreen::runFromAccount();
        } else if (changeUserBtn.contains(tap.x, tap.y)) {
            bool confirmed = false;
            String v = Keypad::run(I18n::t(I18n::Str::CHANGE_USERNAME_PROMPT), appConfig.portalEmail, confirmed);
            if (confirmed) {
                appConfig.portalEmail = v;
                AppConfigStore::save();
                AccountBridge::set(appConfig.portalEmail, appConfig.portalPassword, appConfig.deviceNo);
                FetchTask::sendCommand(FetchTask::Command::AccountChanged);
            }
        } else if (changePassBtn.contains(tap.x, tap.y)) {
            bool confirmed = false;
            String v = Keypad::run(I18n::t(I18n::Str::PASSWORD_PROMPT), "", confirmed);
            if (confirmed) {
                appConfig.portalPassword = v;
                AppConfigStore::save();
                AccountBridge::set(appConfig.portalEmail, appConfig.portalPassword, appConfig.deviceNo);
                FetchTask::sendCommand(FetchTask::Command::AccountChanged);
            }
        } else if (deleteBtn.contains(tap.x, tap.y)) {
            if (confirmDeleteAccount()) {
                appConfig.portalEmail = "";
                appConfig.portalPassword = "";
                AppConfigStore::save();
                AccountBridge::set(appConfig.portalEmail, appConfig.portalPassword, appConfig.deviceNo);
                FetchTask::sendCommand(FetchTask::Command::AccountChanged);
                // Direkt weiter zum Zugangsdaten-Bildschirm der
                // Ersteinrichtung, damit gleich neue eingegeben werden
                // koennen - blockiert, bis "Los geht's" gedrueckt wurde.
                CredentialsScreen::run();
                AccountBridge::set(appConfig.portalEmail, appConfig.portalPassword, appConfig.deviceNo);
                FetchTask::sendCommand(FetchTask::Command::AccountChanged);
                done = true;
            }
        }
    }
}

}
