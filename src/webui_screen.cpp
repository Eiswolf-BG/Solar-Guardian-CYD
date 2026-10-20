#include "webui_screen.h"
#include "display.h"
#include "touch_input.h"
#include "help_popup.h"
#include "layout_utils.h"
#include "layout_check.h"
#include "i18n.h"
#include "config.h"
#include "web_server.h"
#include <WiFi.h>

namespace WebUiScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr Rect HELP_BTN = {(int16_t)(Config::SCREEN_WIDTH - 36), 4, 32, 32};

    // Wortweiser Umbruch nach echter Pixelbreite - gleiches Prinzip wie in
    // anderen Screens (hier bewusst lokal dupliziert, siehe CLAUDE.md
    // Code-Stil).
    uint8_t wrapWords(const String& text, int16_t maxW, String* outLines, uint8_t maxLines) {
        String remaining = text;
        uint8_t lineCount = 0;
        while (remaining.length() > 0 && lineCount < maxLines) {
            int16_t lastGoodSpace = -1;
            int16_t i = 0;
            String line = "";
            while (i < (int16_t)remaining.length()) {
                char c = remaining[i];
                String candidate = line + c;
                if (tft.textWidth(candidate) > maxW && line.length() > 0) break;
                line = candidate;
                if (c == ' ') lastGoodSpace = i;
                i++;
            }
            if (i >= (int16_t)remaining.length()) {
                outLines[lineCount++] = line;
                remaining = "";
                break;
            }
            if (lastGoodSpace >= 0) {
                outLines[lineCount++] = remaining.substring(0, lastGoodSpace);
                remaining = remaining.substring(lastGoodSpace + 1);
            } else {
                outLines[lineCount++] = line;
                remaining = remaining.substring(line.length());
            }
        }
        return lineCount;
    }
}

void run() {
    bool done = false;
    while (!done) {
        LayoutCheck::begin("WebUI");
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::WEBUI_TITLE), Config::SCREEN_WIDTH / 2, 14);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);
        LayoutCheck::rect("Titel", 0, 4, (int16_t)(Config::SCREEN_WIDTH - 44), 24);
        LayoutCheck::rect("Hilfe-Knopf", HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

        // Beschreibung (2-3 Zeilen, wortweise umgebrochen - Auftrag).
        constexpr int16_t INFO_TOP = 46;
        constexpr int16_t INFO_LINE_H = 18;
        constexpr uint8_t INFO_MAX_LINES = 3;
        String infoLines[INFO_MAX_LINES];
        uint8_t infoLineCount = wrapWords(I18n::t(I18n::Str::WEBUI_INFO_TEXT), (int16_t)(Config::SCREEN_WIDTH - 24), infoLines, INFO_MAX_LINES);
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        for (uint8_t i = 0; i < infoLineCount; i++) {
            tft.drawString(infoLines[i], Config::SCREEN_WIDTH / 2, (int16_t)(INFO_TOP + i * INFO_LINE_H));
        }
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Beschreibung", 4, INFO_TOP, (int16_t)(Config::SCREEN_WIDTH - 8), (int16_t)(infoLineCount * INFO_LINE_H));

        int16_t addrTop = (int16_t)(INFO_TOP + infoLineCount * INFO_LINE_H + 16);
        bool wifiConnected = (WiFi.status() == WL_CONNECTED);
        if (wifiConnected) {
            String urlLine = "http://" + WiFi.localIP().toString();
            // Grosse, nie abgeschnittene Adresse - Schrift verkleinert sich
            // automatisch, bis sie passt (Auftrag).
            constexpr uint8_t ADDR_SIZES[] = {3, 2, 1};
            constexpr int16_t ADDR_ZONE_H = 60;
            LayoutUtils::FitResult addrFit = LayoutUtils::fitText(tft, urlLine, (int16_t)(Config::SCREEN_WIDTH - 16), ADDR_ZONE_H, ADDR_SIZES, 3);
            tft.setTextColor(accent, TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            int16_t addrCy = (int16_t)(addrTop + ADDR_ZONE_H / 2);
            tft.drawString(addrFit.text, (int16_t)(Config::SCREEN_WIDTH / 2), addrCy);
            tft.setTextDatum(TL_DATUM);
            {
                tft.setTextSize(addrFit.textSize);
                int16_t w = tft.textWidth(addrFit.text);
                int16_t h = tft.fontHeight();
                LayoutCheck::rect("Adresse", (int16_t)(Config::SCREEN_WIDTH / 2 - w / 2), (int16_t)(addrCy - h / 2), w, h);
                tft.setTextSize(1);
            }
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(I18n::t(I18n::Str::WEBUI_NO_WIFI), (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(addrTop + 24));
            tft.setTextDatum(TL_DATUM);
            tft.setTextColor(accent, TFT_BLACK);
            {
                tft.setTextSize(1);
                int16_t w = tft.textWidth(I18n::t(I18n::Str::WEBUI_NO_WIFI));
                int16_t h = tft.fontHeight();
                LayoutCheck::rect("Kein-WLAN-Hinweis", (int16_t)(Config::SCREEN_WIDTH / 2 - w / 2), (int16_t)(addrTop + 24 - h / 2), w, h);
            }
        }

        Rect backBtn = {16, (int16_t)(Config::SCREEN_HEIGHT - 60), (int16_t)(Config::SCREEN_WIDTH - 32), 44};
        tft.drawRoundRect(backBtn.x, backBtn.y, backBtn.w, backBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::BACK), (int16_t)(backBtn.x + backBtn.w / 2), (int16_t)(backBtn.y + backBtn.h / 2));
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Zurueck-Knopf", backBtn.x, backBtn.y, backBtn.w, backBtn.h);
        LayoutCheck::finish();

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            WebServer_::tick();
            delay(15);
        }

        if (HELP_BTN.contains(tap.x, tap.y)) {
            HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_WEBUI));
            continue;
        }
        if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
