#include "reset_screen.h"
#include "display.h"
#include "touch_input.h"
#include "fetch_task.h"
#include "help_popup.h"
#include "i18n.h"
#include "config.h"
#include <SD.h>

namespace ResetScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr Rect HELP_BTN = {(int16_t)(Config::SCREEN_WIDTH - 36), 4, 32, 32};

    void showMessage(const char* msg, uint16_t color) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(color, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(msg, Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Loescht eine Datei ODER rekursiv einen kompletten Ordner mit allem
    // Inhalt (auch Unterordner/temporaere Dateien) - die SD-Bibliothek
    // bietet selbst kein rekursives Loeschen an.
    bool removeRecursive(const String& path) {
        File entryCheck = SD.open(path);
        if (!entryCheck) return true; // schon weg
        bool isDir = entryCheck.isDirectory();
        entryCheck.close();

        if (!isDir) return SD.remove(path);

        File dir = SD.open(path);
        if (!dir) return true;
        File entry = dir.openNextFile();
        while (entry) {
            String entryPath = entry.path();
            bool entryIsDir = entry.isDirectory();
            entry.close();
            bool ok = entryIsDir ? removeRecursive(entryPath) : SD.remove(entryPath);
            if (!ok) { dir.close(); return false; }
            entry = dir.openNextFile();
        }
        dir.close();
        return SD.rmdir(path);
    }
}

void run() {
    uint16_t accent = Display::systemColor();
    bool done = false;
    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::RESET_WARNING_TITLE), Config::SCREEN_WIDTH / 2, 20);
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

        // Einfacher Wortumbruch fuer den Warntext.
        String body = I18n::t(I18n::Str::RESET_WARNING_BODY);
        constexpr int16_t WRAP_W = 210;
        int16_t y = 50;
        String remaining = body;
        while (remaining.length() > 0) {
            int16_t i = 0;
            String line = "";
            int16_t lastSpace = -1;
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

        // Gross und weit auseinander (Alex' Wunsch), damit man nicht aus
        // Versehen den falschen trifft.
        Rect cancelBtn = {10, 190, (int16_t)(Config::SCREEN_WIDTH - 20), 44};
        Rect confirmBtn = {10, 254, (int16_t)(Config::SCREEN_WIDTH - 20), 44};

        tft.drawRoundRect(cancelBtn.x, cancelBtn.y, cancelBtn.w, cancelBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::CANCEL), cancelBtn.x + cancelBtn.w / 2, cancelBtn.y + cancelBtn.h / 2);
        tft.setTextDatum(TL_DATUM);

        tft.drawRoundRect(confirmBtn.x, confirmBtn.y, confirmBtn.w, confirmBtn.h, 4, TFT_RED);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::RESET_CONFIRM_BUTTON), confirmBtn.x + confirmBtn.w / 2, confirmBtn.y + confirmBtn.h / 2);
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TL_DATUM);

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            delay(15);
        }

        if (HELP_BTN.contains(tap.x, tap.y)) {
            HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_RESET));
        } else if (cancelBtn.contains(tap.x, tap.y)) {
            done = true;
        } else if (confirmBtn.contains(tap.x, tap.y)) {
            FetchTask::pause();
            showMessage(I18n::t(I18n::Str::DELETING_TEXT), accent);

            bool ok = removeRecursive(Config::SD_ROOT_DIR);
            if (ok && SD.exists(Config::SD_ROOT_DIR)) ok = false; // wirklich weg?

            if (ok) {
                ESP.restart();
                // Kehrt nicht zurueck.
            } else {
                showMessage(I18n::t(I18n::Str::RESET_FAILED_TEXT), TFT_RED);
                delay(2000);
                FetchTask::resume();
            }
        }
    }
}

}
