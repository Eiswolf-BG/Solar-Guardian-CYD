#include "location_screen.h"
#include "display.h"
#include "touch_input.h"
#include "keypad.h"
#include "app_config.h"
#include "fetch_task.h"
#include "location_bridge.h"
#include "geocoding_client.h"
#include "ip_geolocation_client.h"
#include "help_popup.h"
#include "layout_utils.h"
#include "layout_check.h"
#include "i18n.h"
#include "config.h"

namespace LocationScreen {

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

    // Zentrierte Meldung, bis zu 2 Zeilen, nichts wird abgeschnitten
    // (Auftrag) - fuer "Keine Verbindung"/"Nichts gefunden"/"Suche...".
    void showWrappedMessage(uint16_t accent, const char* msg) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(accent, TFT_BLACK);
        String lines[2];
        uint8_t n = wrapWords(msg, (int16_t)(Config::SCREEN_WIDTH - 32), lines, 2);
        int16_t lineH = tft.fontHeight();
        int16_t top = (int16_t)(Config::SCREEN_HEIGHT / 2 - (n * lineH) / 2);
        tft.setTextDatum(TC_DATUM);
        for (uint8_t i = 0; i < n; i++) {
            tft.drawString(lines[i], Config::SCREEN_WIDTH / 2, (int16_t)(top + i * lineH));
        }
        tft.setTextDatum(TL_DATUM);
    }

    // Zeigt bis zu MAX_RESULTS Treffer als antippbare Liste. Namen duerfen
    // umbrechen (bis zu 2 Zeilen), werden NIE mit "..." abgeschnitten
    // (Auftrag) - Zeilenanzahl/Zeilen pro Treffer werden einmal vorab
    // berechnet, die Zeilenhoehe jeder Zeile ist dynamisch statt pauschal
    // auf den schlimmsten Fall ausgelegt. Rueckgabe: Index des angetippten
    // Treffers, oder -1 bei "Zurueck" (zurueck zur Tastatur fuer eine
    // neue Suche).
    int8_t showResultList(uint16_t accent, const GeocodingClient::Result* results, uint8_t count) {
        constexpr int16_t TOP = 40;
        constexpr int16_t ROW_GAP = 6;
        constexpr int16_t LINE_H = 18;
        constexpr int16_t ROW_PAD = 8; // 4px oben + unten
        constexpr uint8_t MAX_LINES_PER_ROW = 2;

        tft.setTextSize(1);
        String rowLines[GeocodingClient::MAX_RESULTS][MAX_LINES_PER_ROW];
        uint8_t rowLineCount[GeocodingClient::MAX_RESULTS];
        int16_t rowH[GeocodingClient::MAX_RESULTS];
        for (uint8_t i = 0; i < count; i++) {
            uint8_t n = wrapWords(results[i].name, (int16_t)(Config::SCREEN_WIDTH - 20 - 16), rowLines[i], MAX_LINES_PER_ROW);
            rowLineCount[i] = n > 0 ? n : 1;
            rowH[i] = (int16_t)(rowLineCount[i] * LINE_H + ROW_PAD);
        }

        while (true) {
            LayoutCheck::begin("Standort-Treffer");
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(accent, TFT_BLACK);
            tft.setTextSize(1);
            tft.setTextDatum(TC_DATUM);
            tft.drawString(I18n::t(I18n::Str::LOCATION_RESULTS_TITLE), Config::SCREEN_WIDTH / 2, 14);
            tft.setTextDatum(TL_DATUM);
            HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);
            LayoutCheck::rect("Titel", 0, 4, (int16_t)(Config::SCREEN_WIDTH - 44), 24);
            LayoutCheck::rect("Hilfe-Knopf", HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

            Rect rects[GeocodingClient::MAX_RESULTS];
            int16_t y = TOP;
            for (uint8_t i = 0; i < count; i++) {
                Rect r = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20), rowH[i]};
                rects[i] = r;
                tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
                for (uint8_t ln = 0; ln < rowLineCount[i]; ln++) {
                    tft.setTextDatum(ML_DATUM);
                    tft.drawString(rowLines[i][ln], (int16_t)(r.x + 8),
                                    (int16_t)(r.y + ROW_PAD / 2 + LINE_H / 2 + ln * LINE_H));
                }
                tft.setTextDatum(TL_DATUM);
                LayoutCheck::rect("Treffer-Zeile", r.x, r.y, r.w, r.h);
                y = (int16_t)(y + rowH[i] + ROW_GAP);
            }

            Rect backBtn = {10, (int16_t)(y + 4), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
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
                HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_LOCATION));
                continue;
            }
            if (backBtn.contains(tap.x, tap.y)) return -1;
            for (uint8_t i = 0; i < count; i++) {
                if (rects[i].contains(tap.x, tap.y)) return (int8_t)i;
            }
        }
    }

    // Tastatur -> Geocoding-Abruf (Fetch-Task pausiert waehrenddessen,
    // gleiches Prinzip wie der blockierende WLAN-Scan, siehe CLAUDE.md
    // "Dual-Core-Architektur") -> Trefferliste. Bei Abbruch/Fehler/kein
    // Treffer ueberall zurueck zur Tastatur. Setzt appConfig.location*
    // bei Auswahl (locationManual=true) und gibt true zurueck.
    bool runSearchFlow(uint16_t accent, bool showUmlautHintFirst) {
        if (showUmlautHintFirst) {
            HelpPopup::show(I18n::t(I18n::Str::LOCATION_NO_UMLAUT_HINT));
        }
        while (true) {
            bool confirmed = false;
            String query = Keypad::run(I18n::t(I18n::Str::LOCATION_SEARCH_PROMPT), "", confirmed);
            if (!confirmed) return false;
            query.trim();
            if (query.length() == 0) continue;

            FetchTask::pause();
            showWrappedMessage(accent, I18n::t(I18n::Str::LOCATION_SEARCHING));
            GeocodingClient::Result results[GeocodingClient::MAX_RESULTS];
            uint8_t count = 0;
            GeocodingClient::SearchStatus status = GeocodingClient::search(query, results, count);
            FetchTask::resume();

            if (status == GeocodingClient::SearchStatus::NetworkError) {
                showWrappedMessage(accent, I18n::t(I18n::Str::STATUS_NETWORK_ERROR));
                delay(1800);
                continue;
            }
            if (count == 0) {
                showWrappedMessage(accent, I18n::t(I18n::Str::LOCATION_NOTHING_FOUND));
                delay(1800);
                continue;
            }

            // Genau ein Treffer: automatisch uebernehmen, ohne die Liste
            // zu zeigen (Auftrag) - bei mehreren Treffern weiterhin die
            // Liste zur Auswahl anzeigen.
            int8_t picked = 0;
            if (count > 1) {
                picked = showResultList(accent, results, count);
                if (picked < 0) continue; // zurueck zur Tastatur, neue Suche
            }

            appConfig.locationName = results[picked].name;
            appConfig.locationLat = results[picked].lat;
            appConfig.locationLon = results[picked].lon;
            appConfig.locationSet = true;
            appConfig.locationManual = true;
            return true;
        }
    }

    // "Automatisch ermitteln"-Knopf - startet D1 sofort (blockierend,
    // Fetch-Task pausiert), setzt locationManual=false. Zeigt "Keine
    // Verbindung", falls auch der Rueckfall-Dienst fehlschlug.
    bool runAutoDetect(uint16_t accent) {
        FetchTask::pause();
        showWrappedMessage(accent, I18n::t(I18n::Str::LOCATION_SEARCHING));
        IpGeolocationClient::Result result;
        int httpCode = -1;
        bool ok = IpGeolocationClient::detect(result, &httpCode);
        FetchTask::resume();

        if (!ok) {
            showWrappedMessage(accent, I18n::t(I18n::Str::STATUS_NETWORK_ERROR));
            delay(1800);
            return false;
        }
        appConfig.locationName = result.name;
        appConfig.locationLat = result.lat;
        appConfig.locationLon = result.lon;
        appConfig.locationSet = true;
        appConfig.locationManual = false;
        return true;
    }
}

void run() {
    bool done = false;
    bool firstSearchThisVisit = true;

    while (!done) {
        LayoutCheck::begin("Standort");
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::LOCATION_TITLE), Config::SCREEN_WIDTH / 2, 14);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);
        LayoutCheck::rect("Titel", 0, 4, (int16_t)(Config::SCREEN_WIDTH - 44), 24);
        LayoutCheck::rect("Hilfe-Knopf", HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

        Rect fieldRow = {4, 46, (int16_t)(Config::SCREEN_WIDTH - 8), 40};
        tft.drawRoundRect(fieldRow.x, fieldRow.y, fieldRow.w, fieldRow.h, 4, accent);
        constexpr uint8_t FIELD_SIZES[] = {1};
        String fieldValue = appConfig.locationSet ? appConfig.locationName : String(I18n::t(I18n::Str::TAP_HERE));
        LayoutUtils::FitResult fieldFit = LayoutUtils::fitText(tft, fieldValue, (int16_t)(fieldRow.w - 16), 18, FIELD_SIZES, 1);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(fieldFit.text, (int16_t)(fieldRow.x + fieldRow.w / 2), (int16_t)(fieldRow.y + fieldRow.h / 2));
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Standort-Feld", fieldRow.x, fieldRow.y, fieldRow.w, fieldRow.h);

        constexpr int16_t BTN_TOP = 102;
        constexpr int16_t BTN_H = 44;
        constexpr int16_t BTN_GAP = 10;
        Rect autoBtn = {10, BTN_TOP, (int16_t)(Config::SCREEN_WIDTH - 20), BTN_H};
        Rect backBtn = {10, (int16_t)(BTN_TOP + BTN_H + BTN_GAP), (int16_t)(Config::SCREEN_WIDTH - 20), BTN_H};

        tft.drawRoundRect(autoBtn.x, autoBtn.y, autoBtn.w, autoBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::LOCATION_AUTO_DETECT_BUTTON), autoBtn.x + autoBtn.w / 2, autoBtn.y + autoBtn.h / 2);
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Automatisch-ermitteln-Knopf", autoBtn.x, autoBtn.y, autoBtn.w, autoBtn.h);

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

        bool changed = false;
        if (HELP_BTN.contains(tap.x, tap.y)) {
            HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_LOCATION));
        } else if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        } else if (fieldRow.contains(tap.x, tap.y)) {
            changed = runSearchFlow(accent, firstSearchThisVisit);
            firstSearchThisVisit = false;
        } else if (autoBtn.contains(tap.x, tap.y)) {
            changed = runAutoDetect(accent);
        }

        if (changed) {
            AppConfigStore::save();
            LocationBridge::set(appConfig.locationLat, appConfig.locationLon, true, appConfig.locationManual);
            FetchTask::sendCommand(FetchTask::Command::LocationChanged);
        }
    }
}

}
