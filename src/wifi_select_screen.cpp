#include "wifi_select_screen.h"
#include "display.h"
#include "touch_input.h"
#include "keypad.h"
#include "wifi_scan.h"
#include "help_popup.h"
#include "i18n.h"
#include "config.h"
#include "web_server.h"

namespace WifiSelectScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr Rect HELP_BTN = {4, 2, 32, 32};
    constexpr int16_t BUTTON_ROW_Y = 40;
    constexpr uint8_t VISIBLE_ROWS = 6;
    constexpr int16_t ROW_H = 30;
    constexpr int16_t LIST_TOP = 72;

    Rect rowRect(uint8_t visibleIndex) {
        return {4, (int16_t)(LIST_TOP + visibleIndex * ROW_H), (int16_t)(Config::SCREEN_WIDTH - 8), (ROW_H - 2)};
    }

    void drawSignalBars(int16_t rightX, int16_t rowTop, int32_t rssi) {
        uint8_t level;
        if (rssi >= -55) level = 4;
        else if (rssi >= -65) level = 3;
        else if (rssi >= -75) level = 2;
        else if (rssi >= -85) level = 1;
        else level = 0;

        constexpr uint8_t BAR_W = 3, BAR_GAP = 2, BAR_COUNT = 4;
        int16_t baseline = rowTop + ROW_H - 6;
        int16_t x = rightX - BAR_COUNT * (BAR_W + BAR_GAP);
        uint16_t accent = Display::systemColor();
        for (uint8_t i = 0; i < BAR_COUNT; i++) {
            int16_t barH = 3 + i * 3;
            uint16_t color = (i < level) ? accent : Display::secondaryColor();
            tft.fillRect(x, baseline - barH, BAR_W, barH, color);
            x += BAR_W + BAR_GAP;
        }
    }

    void showMessage(const char* msg) {
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(msg, Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT / 2);
        tft.setTextDatum(TL_DATUM);
    }
}

bool run(bool allowCancel, String& outSsid, String& outPassword) {
    WifiScan::Network networks[Config::MAX_WIFI_NETWORKS_SHOWN];
    uint8_t count = 0;
    uint8_t scrollOffset = 0;

    auto doScan = [&]() {
        showMessage(I18n::t(I18n::Str::SCANNING_WIFI));
        count = WifiScan::scan(networks, Config::MAX_WIFI_NETWORKS_SHOWN);
        scrollOffset = 0;
    };

    doScan();

    while (true) {
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::WIFI_SELECT_TITLE), Config::SCREEN_WIDTH / 2, 14);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

        Rect cancelBtn = {4, BUTTON_ROW_Y, 70, 26};
        Rect rescanBtn = {(int16_t)(Config::SCREEN_WIDTH - 110), BUTTON_ROW_Y, 106, 26};
        if (allowCancel) {
            tft.drawRoundRect(cancelBtn.x, cancelBtn.y, cancelBtn.w, cancelBtn.h, 3, TFT_RED);
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(I18n::t(I18n::Str::CANCEL), cancelBtn.x + cancelBtn.w / 2, cancelBtn.y + cancelBtn.h / 2);
            tft.setTextDatum(TL_DATUM);
            tft.setTextColor(accent, TFT_BLACK);
        }
        tft.drawRoundRect(rescanBtn.x, rescanBtn.y, rescanBtn.w, rescanBtn.h, 3, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::RESCAN_BUTTON), rescanBtn.x + rescanBtn.w / 2, rescanBtn.y + rescanBtn.h / 2);
        tft.setTextDatum(TL_DATUM);

        if (count == 0) {
            tft.setCursor(4, LIST_TOP + 20);
            tft.print(I18n::t(I18n::Str::NO_NETWORKS_FOUND));
        }

        uint8_t visibleCount = (count - scrollOffset) < VISIBLE_ROWS ? (count - scrollOffset) : VISIBLE_ROWS;
        Rect rowRects[VISIBLE_ROWS];
        for (uint8_t i = 0; i < visibleCount; i++) {
            uint8_t netIdx = scrollOffset + i;
            Rect r = rowRect(i);
            rowRects[i] = r;
            tft.drawRoundRect(r.x, r.y, r.w, r.h, 3, Display::secondaryColor());
            tft.setCursor((int16_t)(r.x + 8), (int16_t)(r.y + r.h / 2 + 4));
            tft.print(networks[netIdx].ssid);
            drawSignalBars((int16_t)(r.x + r.w - 6), r.y, networks[netIdx].rssi);
        }

        bool canScrollUp = scrollOffset > 0;
        bool canScrollDown = (scrollOffset + VISIBLE_ROWS) < count;
        Rect upBtn = {(int16_t)(Config::SCREEN_WIDTH - 40), (int16_t)(LIST_TOP + VISIBLE_ROWS * ROW_H + 4), 36, 30};
        Rect downBtn = {4, upBtn.y, 36, 30};
        if (canScrollUp || canScrollDown) {
            tft.drawRoundRect(downBtn.x, downBtn.y, downBtn.w, downBtn.h, 3, canScrollUp ? accent : Display::secondaryColor());
            tft.setTextDatum(MC_DATUM);
            tft.drawString("<", downBtn.x + downBtn.w / 2, downBtn.y + downBtn.h / 2);
            tft.drawRoundRect(upBtn.x, upBtn.y, upBtn.w, upBtn.h, 3, canScrollDown ? accent : Display::secondaryColor());
            tft.drawString(">", upBtn.x + upBtn.w / 2, upBtn.y + upBtn.h / 2);
            tft.setTextDatum(TL_DATUM);
        }

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            WebServer_::tick();
            delay(15);
        }

        if (HELP_BTN.contains(tap.x, tap.y)) {
            HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_WIFI_SELECT));
            continue;
        }
        if (allowCancel && cancelBtn.contains(tap.x, tap.y)) {
            return false;
        }
        if (rescanBtn.contains(tap.x, tap.y)) {
            doScan();
            continue;
        }
        if (canScrollUp && downBtn.contains(tap.x, tap.y)) {
            scrollOffset -= VISIBLE_ROWS;
            continue;
        }
        if (canScrollDown && upBtn.contains(tap.x, tap.y)) {
            scrollOffset += VISIBLE_ROWS;
            continue;
        }

        int8_t tappedRow = -1;
        for (uint8_t i = 0; i < visibleCount; i++) {
            if (rowRects[i].contains(tap.x, tap.y)) { tappedRow = (int8_t)i; break; }
        }
        if (tappedRow < 0) continue;

        WifiScan::Network& net = networks[scrollOffset + tappedRow];
        String password = "";
        if (!net.open) {
            bool confirmed = false;
            char promptBuf[48];
            snprintf(promptBuf, sizeof(promptBuf), I18n::t(I18n::Str::PASSWORD_FOR_PROMPT_FMT), net.ssid.c_str());
            password = Keypad::run(promptBuf, "", confirmed);
            if (!confirmed) continue;
        }

        showMessage(I18n::t(I18n::Str::CONNECTING));
        if (WifiScan::connect(net.ssid, password)) {
            outSsid = net.ssid;
            outPassword = password;
            return true;
        } else {
            showMessage(I18n::t(I18n::Str::CONNECTION_FAILED));
            delay(1500);
        }
    }
}

}
