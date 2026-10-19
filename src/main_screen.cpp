#include "main_screen.h"
#include "display.h"
#include "touch_input.h"
#include "swipe_input.h"
#include "wifi_scan.h"
#include "wifi_status.h"
#include "app_config.h"
#include "ip_location_data.h"
#include "location_bridge.h"
#include "fetch_task.h"
#include "menu_screen.h"
#include "pages.h"
#include "page_nav.h"
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
    uint32_t lastSeenIpLocationCounter = 0xFFFFFFFFu;

    // Diagnose: Neuzeichnungen je Seite zaehlen und einmal pro Minute
    // loggen (Auftrag "Flackern" - vorher/nachher-Vergleich).
    constexpr uint8_t MAX_TRACKED_PAGES = 8;
    uint32_t redrawCounts[MAX_TRACKED_PAGES] = {0};
    uint32_t lastRedrawLogMs = 0;

    char lastClockText[6] = "--:--";
    uint8_t currentPage = 0;

    void drawHeader() {
        // IMMER explizit setzen, nie vom Aufrufer/einer vorherigen Seite
        // erben (siehe CLAUDE.md "Schriftzustand") - eine Seite, die
        // zuletzt mit Size 2 gezeichnet hat (Bugfix: war bei home_page.cpp
        // der Fall), liess Uhr/Titel/WLAN-Symbol hier sonst riesig und
        // ineinanderlaufend erscheinen, da drawHeader() bislang NIE selbst
        // tft.setTextSize() aufrief.
        tft.setTextSize(1);
        uint16_t accent = Display::systemColor();
        tft.fillRect(0, 0, Config::SCREEN_WIDTH, HEADER_H, TFT_BLACK);
        // Eigene begin()/finish()-Pruefung nur fuer die drei Kopfzeilen-
        // Elemente untereinander (Auftrag) - unabhaengig von der Seiten-
        // Pruefung in drawBody(), da drawHeader() auch separat (Uhr-Tick)
        // aufgerufen wird.
        LayoutCheck::begin("Kopfzeile");

        tft.setTextColor(accent, TFT_BLACK);
        tft.setCursor(6, HEADER_BASELINE_Y);
        tft.print(lastClockText);
        {
            int16_t clockW = tft.textWidth(lastClockText);
            LayoutCheck::rect("Uhr", 6, (int16_t)(HEADER_BASELINE_Y - tft.fontHeight()), clockW, tft.fontHeight());
        }

        // Bewusst OHNE Rahmen (Alex' Wunsch) - nur der Text ist sichtbar,
        // die Tipp-Flaeche (MENU_BTN) bleibt trotzdem gleich gross und
        // reicht bis zur Oberkante des Displays. 3px unterhalb der
        // gemeinsamen Grundlinie (Alex' Wunsch nach Sichtkontrolle) - nur
        // dieser eine Text, Uhr/WLAN-Symbol bleiben auf HEADER_BASELINE_Y.
        constexpr const char* TITLE_TEXT = "Solar Guardian";
        tft.setTextDatum(BC_DATUM);
        tft.drawString(TITLE_TEXT, MENU_BTN.x + MENU_BTN.w / 2, (int16_t)(HEADER_BASELINE_Y + 3));
        tft.setTextDatum(TL_DATUM);
        {
            int16_t titleW = tft.textWidth(TITLE_TEXT);
            int16_t titleX = (int16_t)(MENU_BTN.x + MENU_BTN.w / 2 - titleW / 2);
            LayoutCheck::rect("Titel", titleX, (int16_t)(HEADER_BASELINE_Y + 3 - tft.fontHeight()), titleW, tft.fontHeight());
        }

        // Etwas kleiner als vorher und auf dieselbe Grundlinie gehoben
        // (Alex' Wunsch).
        uint8_t bars = WifiScan::signalBars();
        constexpr uint8_t BAR_W = 3, BAR_GAP = 2, BAR_COUNT = 4;
        int16_t rightX = Config::SCREEN_WIDTH - 6;
        int16_t x = rightX - BAR_COUNT * (BAR_W + BAR_GAP);
        int16_t barsStartX = x;
        for (uint8_t i = 0; i < BAR_COUNT; i++) {
            int16_t barH = 3 + i * 3;
            uint16_t color = (i < bars) ? accent : TFT_DARKGREY;
            tft.fillRect(x, HEADER_BASELINE_Y - barH, BAR_W, barH, color);
            x += BAR_W + BAR_GAP;
        }
        constexpr int16_t TALLEST_BAR_H = 3 + (BAR_COUNT - 1) * 3;
        LayoutCheck::rect("WLAN-Symbol", barsStartX, (int16_t)(HEADER_BASELINE_Y - TALLEST_BAR_H),
                           (int16_t)(BAR_COUNT * (BAR_W + BAR_GAP) - BAR_GAP), TALLEST_BAR_H);
        LayoutCheck::finish();
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

    // fullClear=true: kompletter schwarzer Wisch vor dem Zeichnen - fuer
    // einen SEITENWECHSEL (Wisch/Kachel-Sprung) oder nach einem Popup
    // (Menue/Hilfe), die den Bereich ueberdeckt haben. fullClear=false:
    // nur fuer ein- und dieselbe, bereits sichtbare Seite neu zeichnen,
    // ausgeloest durch update()==true (ein Datenwert hat sich geaendert) -
    // OHNE den Wisch, der sonst bei jeder Aktualisierung kurz alles
    // schwarz aufblitzen liess (Auftrag "Flackern"/CLAUDE.md "Nur bei
    // echter Aenderung neu zeichnen"). Die einzelnen Seiten ueberzeichnen
    // ihre Kachel-Werte dafuer selbst mit ihrem eigenen Hintergrund
    // (siehe z.B. home_page.cpp/power_page.cpp - jede Wert-/Einheitszone
    // wird vor dem Text mit einem fillRect auf die Kachelfarbe
    // zurueckgesetzt, nicht die ganze Seite).
    void drawBody(bool fullClear = true) {
        if (fullClear) tft.fillRect(0, BODY_TOP, Config::SCREEN_WIDTH, BODY_H, TFT_BLACK);
        // Begin/Rect/Finish zentral HIER statt in jeder einzelnen Seite -
        // nur so werden Seitenpunkte und "?"-Knopf (unten von main_screen
        // selbst gezeichnet) beim Layout-Pruefer mit angemeldet, siehe
        // CLAUDE.md Layout-Regel.
        LayoutCheck::begin(I18n::t(Pages::get(currentPage).titleKey));
        Pages::get(currentPage).draw(BODY_TOP, BODY_H, fullClear);
        drawDots();
        // Als letztes zeichnen - der Knopf reicht in die Punkte-Zeile
        // hinein (siehe HELP_BTN-Kommentar), darf also nicht von
        // drawDots() wieder uebermalt werden.
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);
        LayoutCheck::rect("Hilfe-Knopf", HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);
        LayoutCheck::finish();

        // Diagnose-Zaehler (Auftrag "Flackern" - Anzahl Neuzeichnungen je
        // Seite und Minute mitloggen) - dauerhaft drin gelassen wie der
        // Heap-Log im Fetch-Task, kostet praktisch nichts.
        if (currentPage < MAX_TRACKED_PAGES) redrawCounts[currentPage]++;
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

    // Automatische IP-Standort-Ermittlung (Fetch-Task, Core 0) in
    // appConfig uebernehmen - NUR wenn noch nicht von Hand gesetzt
    // (Auftrag "Standort automatisch, notfalls einmalig per Eingabe").
    // SD-Zugriff bleibt exklusiv auf Core 1, deshalb hier statt im
    // Fetch-Task selbst.
    IpLocationData::Data ipLoc = IpLocationData::snapshot();
    if (ipLoc.valid && ipLoc.changeCounter != lastSeenIpLocationCounter) {
        lastSeenIpLocationCounter = ipLoc.changeCounter;
        if (!appConfig.locationManual) {
            appConfig.locationLat = ipLoc.lat;
            appConfig.locationLon = ipLoc.lon;
            appConfig.locationName = ipLoc.name;
            appConfig.locationSet = true;
            appConfig.locationManual = false; // automatisch ermittelt, siehe Bedingung oben
            AppConfigStore::save();
            LocationBridge::set(appConfig.locationLat, appConfig.locationLon, true, false);
            FetchTask::sendCommand(FetchTask::Command::LocationChanged);
        }
    }

    // Seiten mit eigener Aktualisierung (z.B. "Ertrag") zeichnen nur neu,
    // wenn sich wirklich etwas geaendert hat - kein Flackern, siehe
    // CLAUDE.md.
    bool (*pageUpdate)() = Pages::get(currentPage).update;
    if (pageUpdate && pageUpdate()) drawBody(false); // gleiche Seite, kein Schwarz-Wisch (Auftrag "Flackern")

    uint32_t redrawLogNow = millis();
    if (redrawLogNow - lastRedrawLogMs >= 60000) {
        lastRedrawLogMs = redrawLogNow;
        Serial.print("[redraw] Neuzeichnungen/Minute:");
        for (uint8_t i = 0; i < Pages::count() && i < MAX_TRACKED_PAGES; i++) {
            Serial.printf(" %s=%u", I18n::t(Pages::get(i).titleKey), (unsigned)redrawCounts[i]);
            redrawCounts[i] = 0;
        }
        Serial.println();
    }

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
        if (pageOnTap && pageOnTap(point.x, point.y, BODY_TOP, BODY_H)) {
            // Eine Kachel (siehe home_page.cpp) kann ueber PageNav zu einer
            // ANDEREN Seite springen wollen - vor dem Neuzeichnen pruefen,
            // sonst wuerde drawBody() noch mit der alten currentPage
            // zeichnen.
            uint8_t requestedPage;
            if (PageNav::consume(requestedPage) && requestedPage < Pages::count()) {
                currentPage = requestedPage;
            }
            drawBody();
        }
    }
}

}
