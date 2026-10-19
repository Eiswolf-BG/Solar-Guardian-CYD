#include "networks_screen.h"
#include "display.h"
#include "touch_input.h"
#include "keypad.h"
#include "wifi_scan.h"
#include "wifi_select_screen.h"
#include "wifi_status.h"
#include "fetch_task.h"
#include "app_config.h"
#include "help_popup.h"
#include "layout_utils.h"
#include "i18n.h"
#include "config.h"

namespace NetworksScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr int16_t ROW_H = 48;
    constexpr int16_t ROW_GAP = 10;
    constexpr int16_t TOP = 40;

    constexpr Rect HELP_BTN = {(int16_t)(Config::SCREEN_WIDTH - 36), 4, 32, 32};

    Rect slotRect(uint8_t index) {
        return {10, (int16_t)(TOP + index * (ROW_H + ROW_GAP)), (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
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

    // Kleines Untermenue fuer einen belegten Platz: Verbinden/Passwort
    // aendern/Loeschen/Zurueck. Gibt bei "Loeschen" true zurueck (Aufrufer
    // zeichnet die Liste danach neu), sonst immer false.
    void runSlotSubmenu(uint8_t slotIndex) {
        AppConfig::WifiSlot& slot = appConfig.wifiSlots[slotIndex];
        bool done = false;
        while (!done) {
            uint16_t accent = Display::systemColor();
            tft.fillScreen(TFT_BLACK);
            tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
            tft.setTextColor(accent, TFT_BLACK);
            tft.setTextSize(1);
            // fitText() statt festem drawString() - eine lange SSID darf
            // den Bildschirmrand nie ueberschreiten (Layout-Regel).
            constexpr uint8_t TITLE_SIZES[] = {1};
            LayoutUtils::FitResult titleFit = LayoutUtils::fitText(tft, slot.ssid, (int16_t)(Config::SCREEN_WIDTH - 16), 18, TITLE_SIZES, 1);
            tft.setTextDatum(TC_DATUM);
            tft.drawString(titleFit.text, Config::SCREEN_WIDTH / 2, 14);
            tft.setTextDatum(TL_DATUM);

            const char* labels[] = {
                I18n::t(I18n::Str::CONNECT_BUTTON),
                I18n::t(I18n::Str::CHANGE_PASSWORD_BUTTON),
                I18n::t(I18n::Str::DELETE_BUTTON),
                I18n::t(I18n::Str::BACK),
            };
            Rect rects[4];
            for (uint8_t i = 0; i < 4; i++) {
                Rect r = {10, (int16_t)(50 + i * 44), (int16_t)(Config::SCREEN_WIDTH - 20), 36};
                rects[i] = r;
                uint16_t color = (i == 2) ? TFT_RED : accent; // Loeschen in Rot
                tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, color);
                tft.setTextColor(color, TFT_BLACK);
                tft.setTextDatum(MC_DATUM);
                tft.drawString(labels[i], r.x + r.w / 2, r.y + r.h / 2);
                tft.setTextDatum(TL_DATUM);
            }

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                delay(15);
            }

            if (rects[3].contains(tap.x, tap.y)) {
                done = true;
            } else if (rects[0].contains(tap.x, tap.y)) {
                // Fetch-Task pausieren, solange hier selbst (auf Core 1)
                // eine blockierende Verbindung getestet wird - sonst
                // greifen zwei Seiten gleichzeitig aufs WLAN-Modul zu.
                FetchTask::pause();
                showMessage(I18n::t(I18n::Str::CONNECTING));
                bool ok = WifiScan::connect(slot.ssid, slot.password);
                FetchTask::resume();
                if (ok) {
                    // Verbindung steht bereits (WifiScan::connect() hat
                    // gerade erfolgreich verbunden) - der Fetch-Task
                    // erkennt das selbst anhand der aktiven SSID, sobald
                    // er wieder laeuft, ein ReconnectNow waere hier nur
                    // ein unnoetiger kurzer Verbindungsabbruch.
                    appConfig.lastActiveWifiSlot = slotIndex;
                    AppConfigStore::save();
                } else {
                    showMessage(I18n::t(I18n::Str::CONNECTION_FAILED));
                    delay(1500);
                }
            } else if (rects[1].contains(tap.x, tap.y)) {
                FetchTask::pause();
                bool confirmed = false;
                char promptBuf[48];
                snprintf(promptBuf, sizeof(promptBuf), I18n::t(I18n::Str::NEW_PASSWORD_FOR_PROMPT_FMT), slot.ssid.c_str());
                String newPass = Keypad::run(promptBuf, slot.password, confirmed);
                if (confirmed) {
                    slot.password = newPass;
                    AppConfigStore::save();
                }
                FetchTask::resume();
            } else if (rects[2].contains(tap.x, tap.y)) {
                FetchTask::pause();
                bool wasActive = (WifiStatus::get().slot == slotIndex);
                slot.ssid = "";
                slot.password = "";
                if (appConfig.lastActiveWifiSlot == slotIndex) appConfig.lastActiveWifiSlot = 255;
                AppConfigStore::save();
                FetchTask::resume();
                if (wasActive) FetchTask::sendCommand(FetchTask::Command::ReconnectNow);
                done = true;
            }
        }
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
        tft.drawString(I18n::t(I18n::Str::NETWORKS_TITLE), Config::SCREEN_WIDTH / 2, 14);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

        Rect rects[Config::MAX_WIFI_SLOTS];
        uint8_t connected = WifiStatus::get().slot;
        for (uint8_t i = 0; i < Config::MAX_WIFI_SLOTS; i++) {
            Rect r = slotRect(i);
            rects[i] = r;
            bool occupied = appConfig.wifiSlots[i].ssid.length() > 0;
            tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
            // fitText() statt festem print() - eine lange SSID (bis 32
            // Zeichen moeglich) darf die Zeile nie ueberlaufen (Layout-
            // Regel, siehe CLAUDE.md). 20px Platz rechts fuer den Haken.
            tft.setTextColor(accent, TFT_BLACK);
            constexpr uint8_t ROW_SIZES[] = {1};
            String ssidLabel = occupied ? appConfig.wifiSlots[i].ssid : String(I18n::t(I18n::Str::FREE_SLOT_LABEL));
            LayoutUtils::FitResult ssidFit = LayoutUtils::fitText(tft, ssidLabel, (int16_t)(r.w - 20), 18, ROW_SIZES, 1);
            tft.setCursor((int16_t)(r.x + 10), (int16_t)(r.y + r.h / 2 + 4));
            tft.print(ssidFit.text);
            if (occupied && i == connected) {
                tft.setTextDatum(MR_DATUM);
                tft.drawString("✓", (int16_t)(r.x + r.w - 10), (int16_t)(r.y + r.h / 2));
                tft.setTextDatum(TL_DATUM);
            }
        }

        Rect backBtn = {10, (int16_t)(TOP + Config::MAX_WIFI_SLOTS * (ROW_H + ROW_GAP) + 4), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
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
            HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_NETWORKS));
            continue;
        }
        if (backBtn.contains(tap.x, tap.y)) {
            done = true;
            continue;
        }
        for (uint8_t i = 0; i < Config::MAX_WIFI_SLOTS; i++) {
            if (!rects[i].contains(tap.x, tap.y)) continue;
            if (appConfig.wifiSlots[i].ssid.length() > 0) {
                runSlotSubmenu(i);
            } else {
                FetchTask::pause();
                String ssid, password;
                bool ok = WifiSelectScreen::run(true, ssid, password);
                if (ok) {
                    // Verbindung steht bereits, siehe Kommentar oben bei
                    // "Verbinden" im Untermenue - kein ReconnectNow nötig.
                    appConfig.wifiSlots[i].ssid = ssid;
                    appConfig.wifiSlots[i].password = password;
                    appConfig.lastActiveWifiSlot = i;
                    AppConfigStore::save();
                }
                FetchTask::resume();
            }
            break;
        }
    }
}

}
