#include "main_screen.h"
#include "display.h"
#include "touch_input.h"
#include "swipe_input.h"
#include "wifi_scan.h"
#include "wifi_status.h"
#include "app_config.h"
#include "menu_screen.h"
#include "pages.h"
#include "help_popup.h"
#include "layout_check.h"
#include "i18n.h"
#include "config.h"
#include <WiFi.h>
#include <time.h>

namespace MainScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    // Gemeinsame Grundlinie fuer Uhr, "Solar Guardian" und WLAN-Symbol
    // (Alex' Wunsch, nach ihrem Foto) - eine einzige Konstante, damit sich
    // die Kopfzeile spaeter leicht anpassen laesst. Unser Font ist
    // grundlinien-verankert (siehe CLAUDE.md), text waechst von dieser
    // Y-Position aus nach oben.
    constexpr int16_t HEADER_BASELINE_Y = 18;

    // Tipp-Flaeche bleibt mind. 40px hoch und reicht von der Oberkante des
    // Displays nach unten (CLAUDE.md) - der sichtbare Text sitzt dank der
    // Grundlinie oben in dieser Flaeche, nicht mittig.
    constexpr Rect MENU_BTN = {(int16_t)(Config::SCREEN_WIDTH / 2 - 70), 0, 140, 40};
    constexpr int16_t HEADER_H = 40;
    constexpr int16_t DOTS_H = 16;
    constexpr int16_t BODY_TOP = HEADER_H;
    constexpr int16_t BODY_H = Config::SCREEN_HEIGHT - HEADER_H - DOTS_H;
    constexpr int16_t DOTS_Y = Config::SCREEN_HEIGHT - DOTS_H;

    // Unten rechts auf Hoehe der Seitenpunkte, 6px Abstand zum Rand
    // (Alex' Wunsch, Teil 1b) - 32px Knopf passt nicht komplett in die nur
    // 16px hohe Punkte-Zeile, reicht deshalb etwas in den Seitenbereich
    // hinein. Liegt weit genug rechts, um die mittig stehenden Punkte
    // nicht zu beruehren (siehe drawBody()-Zeichenreihenfolge: Punkte
    // zuerst, Knopf zuletzt, damit er nicht uebermalt wird).
    constexpr Rect HELP_BTN = {(int16_t)(Config::SCREEN_WIDTH - 32 - 6), (int16_t)(Config::SCREEN_HEIGHT - 32 - 6), 32, 32};

    uint32_t lastClockRedrawMs = 0;
    char lastClockText[6] = "--:--";
    uint8_t currentPage = 0;

    void drawHeader() {
        uint16_t accent = Display::systemColor();
        tft.fillRect(0, 0, Config::SCREEN_WIDTH, HEADER_H, TFT_BLACK);

        tft.setTextColor(accent, TFT_BLACK);
        tft.setCursor(6, HEADER_BASELINE_Y);
        tft.print(lastClockText);

        // Bewusst OHNE Rahmen (Alex' Wunsch) - nur der Text ist sichtbar,
        // die Tipp-Flaeche (MENU_BTN) bleibt trotzdem gleich gross und
        // reicht bis zur Oberkante des Displays. 3px unterhalb der
        // gemeinsamen Grundlinie (Alex' Wunsch nach Sichtkontrolle) - nur
        // dieser eine Text, Uhr/WLAN-Symbol bleiben auf HEADER_BASELINE_Y.
        tft.setTextDatum(BC_DATUM);
        tft.drawString("Solar Guardian", MENU_BTN.x + MENU_BTN.w / 2, (int16_t)(HEADER_BASELINE_Y + 3));
        tft.setTextDatum(TL_DATUM);

        // Etwas kleiner als vorher und auf dieselbe Grundlinie gehoben
        // (Alex' Wunsch).
        uint8_t bars = WifiScan::signalBars();
        constexpr uint8_t BAR_W = 3, BAR_GAP = 2, BAR_COUNT = 4;
        int16_t rightX = Config::SCREEN_WIDTH - 6;
        int16_t x = rightX - BAR_COUNT * (BAR_W + BAR_GAP);
        for (uint8_t i = 0; i < BAR_COUNT; i++) {
            int16_t barH = 3 + i * 3;
            uint16_t color = (i < bars) ? accent : TFT_DARKGREY;
            tft.fillRect(x, HEADER_BASELINE_Y - barH, BAR_W, barH, color);
            x += BAR_W + BAR_GAP;
        }
    }

    void drawDots() {
        uint16_t accent = Display::systemColor();
        tft.fillRect(0, DOTS_Y, Config::SCREEN_WIDTH, DOTS_H, TFT_BLACK);
        uint8_t n = Pages::count();
        constexpr int16_t DOT_R = 3;
        constexpr int16_t DOT_GAP = 10;
        int16_t totalW = (int16_t)((n - 1) * DOT_GAP);
        int16_t x = (int16_t)(Config::SCREEN_WIDTH / 2 - totalW / 2);
        int16_t y = (int16_t)(DOTS_Y + DOTS_H / 2);
        int16_t firstX = x;
        for (uint8_t i = 0; i < n; i++) {
            if (i == currentPage) {
                tft.fillCircle(x, y, DOT_R, accent);
            } else {
                tft.drawCircle(x, y, DOT_R, TFT_DARKGREY);
            }
            x += DOT_GAP;
        }
        // Tatsaechliche Ausdehnung der Punktreihe melden (nicht die volle
        // Bildschirmbreite) - sie liegt mittig und beruehrt den "?"-Knopf
        // am rechten Rand dadurch nie (siehe CLAUDE.md Layout-Regel).
        int16_t lastX = (int16_t)(x - DOT_GAP);
        LayoutCheck::rect("Seitenpunkte", (int16_t)(firstX - DOT_R), (int16_t)(y - DOT_R),
                           (int16_t)(lastX - firstX + 2 * DOT_R), (int16_t)(2 * DOT_R));
    }

    void drawBody() {
        tft.fillRect(0, BODY_TOP, Config::SCREEN_WIDTH, BODY_H, TFT_BLACK);
        // Begin/Rect/Finish zentral HIER statt in jeder einzelnen Seite -
        // nur so werden Seitenpunkte und "?"-Knopf (unten von main_screen
        // selbst gezeichnet) beim Layout-Pruefer mit angemeldet, siehe
        // CLAUDE.md Layout-Regel.
        LayoutCheck::begin(I18n::t(Pages::get(currentPage).titleKey));
        Pages::get(currentPage).draw(BODY_TOP, BODY_H);
        drawDots();
        // Als letztes zeichnen - der Knopf reicht in die Punkte-Zeile
        // hinein (siehe HELP_BTN-Kommentar), darf also nicht von
        // drawDots() wieder uebermalt werden.
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);
        LayoutCheck::rect("Hilfe-Knopf", HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);
        LayoutCheck::finish();
    }

    void updateClockText() {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 10)) {
            snprintf(lastClockText, sizeof(lastClockText), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        } else {
            strncpy(lastClockText, "--:--", sizeof(lastClockText));
        }
    }
}

void enter() {
    tft.fillScreen(TFT_BLACK);
    updateClockText();
    drawHeader();
    drawBody();
    lastClockRedrawMs = millis();
}

void tick() {
    uint32_t now = millis();

    if (now - lastClockRedrawMs >= 1000) {
        lastClockRedrawMs = now;
        updateClockText();
        drawHeader();
    }

    // Der Fetch-Task (Core 0) darf die SD-Karte nie beruehren - er meldet
    // einen Verbindungswechsel nur ueber WifiStatus. Core 1 uebernimmt die
    // eigentliche Speicherung hier, sobald sich etwas geaendert hat.
    WifiStatus::Status wifiStatus = WifiStatus::get();
    if (wifiStatus.connected && wifiStatus.slot != 255 && wifiStatus.slot != appConfig.lastActiveWifiSlot) {
        appConfig.lastActiveWifiSlot = wifiStatus.slot;
        AppConfigStore::save();
    }

    // Seiten mit eigener Aktualisierung (z.B. "Ertrag") zeichnen nur neu,
    // wenn sich wirklich etwas geaendert hat - kein Flackern, siehe
    // CLAUDE.md.
    bool (*pageUpdate)() = Pages::get(currentPage).update;
    if (pageUpdate && pageUpdate()) drawBody();

    SwipeInput::Direction dir;
    TouchInput::Point point;
    if (!SwipeInput::poll(dir, point)) return;

    if (dir == SwipeInput::Direction::Left) {
        currentPage = (uint8_t)((currentPage + 1) % Pages::count());
        drawBody();
    } else if (dir == SwipeInput::Direction::Right) {
        currentPage = (uint8_t)((currentPage + Pages::count() - 1) % Pages::count());
        drawBody();
    } else if (HELP_BTN.contains(point.x, point.y)) {
        HelpPopup::show(I18n::t(Pages::get(currentPage).helpKey));
        drawBody();
    } else if (MENU_BTN.contains(point.x, point.y)) {
        MenuScreen::run();
        enter(); // Bildschirm nach Rueckkehr aus dem Menue neu aufbauen
    } else {
        bool (*pageOnTap)(int16_t, int16_t, int16_t, int16_t) = Pages::get(currentPage).onTap;
        if (pageOnTap && pageOnTap(point.x, point.y, BODY_TOP, BODY_H)) drawBody();
    }
}

}
