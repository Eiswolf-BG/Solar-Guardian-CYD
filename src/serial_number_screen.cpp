#include "serial_number_screen.h"
#include "display.h"
#include "touch_input.h"
#include "keypad.h"
#include "app_config.h"
#include "fetch_task.h"
#include "account_bridge.h"
#include "epever_client.h"
#include "help_popup.h"
#include "layout_check.h"
#include "layout_utils.h"
#include "i18n.h"
#include "config.h"
#include "web_server.h"

namespace SerialNumberScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr Rect HELP_BTN = {(int16_t)(Config::SCREEN_WIDTH - 36), 4, 32, 32};
    // Grosszuegig bemessen (statt vorher 5) - die neue, ausfuehrlichere
    // Erklaerung (Auftrag "Seriennummer zu Konto verschieben") ist
    // deutlich laenger als der alte Text und darf nie mit wegfallenden
    // Zeilen abgeschnitten werden (wrapLines() bricht sonst bei
    // maxLines ab, ohne Warnung).
    constexpr uint8_t HINT_MAX_LINES = 10;

    // Mindestens 8 Zeichen, nur Grossbuchstaben, Ziffern und Bindestrich
    // (Auftrag) - der Keypad-Eingabetext wird vor der Pruefung getrimmt
    // und auf Grossbuchstaben normalisiert (verzeihlicher, falls Shift
    // mal vergessen wurde - Shift bleibt trotzdem fuer jeden Buchstaben
    // einzeln bedienbar, siehe keypad.cpp).
    bool isValidSerial(const String& s) {
        if (s.length() < 8) return false;
        for (uint16_t i = 0; i < s.length(); i++) {
            char c = s[i];
            if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')) return false;
        }
        return true;
    }

    // Wortweiser Umbruch nach echter Pixelbreite bei Size 1 - gleiches
    // Prinzip wie in battery_page.cpp/reset_screen.cpp, hier bewusst
    // lokal dupliziert (siehe CLAUDE.md Code-Stil).
    uint8_t wrapLines(const String& text, int16_t maxW, String* outLines, uint8_t maxLines) {
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

    void drawHeader(uint16_t accent) {
        LayoutCheck::begin("Seriennummer");
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::SERIAL_NUMBER_TITLE), Config::SCREEN_WIDTH / 2, 14);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);
        LayoutCheck::rect("Titel", 0, 4, (int16_t)(Config::SCREEN_WIDTH - 44), 24);
        LayoutCheck::rect("Hilfe-Knopf", HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);
    }

    Rect drawInputRow(uint16_t accent, const String& value) {
        Rect row = {4, 50, (int16_t)(Config::SCREEN_WIDTH - 8), 40};
        tft.drawRoundRect(row.x, row.y, row.w, row.h, 4, accent);
        tft.setTextSize(1);
        tft.setCursor((int16_t)(row.x + 8), (int16_t)(row.y + 16));
        tft.print(I18n::t(I18n::Str::SERIAL_NUMBER_TITLE));

        // Wert nie ungeprueft drucken (Auftrag "letzte Stelle abgeschnitten")
        // - Schrift verkleinert sich automatisch, bis die ganze Nummer in
        // die Zeile passt, statt ungeprueft ueber den Rahmen hinauszulaufen.
        String displayValue = value.length() ? value : String(I18n::t(I18n::Str::TAP_HERE));
        constexpr uint8_t SIZES[] = {1};
        int16_t maxW = (int16_t)(row.w - 16);
        LayoutUtils::FitResult fit = LayoutUtils::fitText(tft, displayValue, maxW, 16, SIZES, 1);
        tft.setCursor((int16_t)(row.x + 8), (int16_t)(row.y + 34));
        tft.print(fit.text);
        tft.setTextSize(1);

        LayoutCheck::rect("Eingabefeld", row.x, row.y, row.w, row.h);
        LayoutCheck::containedRect("Eingabefeld-Wert", row.x, row.y, row.w, row.h,
                                    (int16_t)(row.x + 8), (int16_t)(row.y + 34 - tft.fontHeight()),
                                    tft.textWidth(fit.text), tft.fontHeight());
        return row;
    }

    int16_t drawHint(uint16_t accent) {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        String lines[HINT_MAX_LINES];
        uint8_t n = wrapLines(I18n::t(I18n::Str::SERIAL_NUMBER_HINT), (int16_t)(Config::SCREEN_WIDTH - 16), lines, HINT_MAX_LINES);
        constexpr int16_t HINT_TOP = 100;
        constexpr int16_t LINE_H = 16;
        tft.setTextDatum(TC_DATUM);
        for (uint8_t i = 0; i < n; i++) {
            tft.drawString(lines[i], Config::SCREEN_WIDTH / 2, (int16_t)(HINT_TOP + i * LINE_H));
        }
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        LayoutCheck::rect("Hinweistext", 8, HINT_TOP, (int16_t)(Config::SCREEN_WIDTH - 16), (int16_t)(n * LINE_H));
        return (int16_t)(HINT_TOP + n * LINE_H);
    }
}

bool tryAutoResolve(const String& email, const String& password, String& outDeviceNo) {
    uint16_t accent = Display::systemColor();
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
    tft.setTextColor(accent, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    const char* msg = I18n::t(I18n::Str::CONNECTING_TO_PORTAL);
    tft.drawString(msg, Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT / 2);
    tft.setTextDatum(TL_DATUM);
    LayoutCheck::begin("Verbinde mit dem Portal");
    {
        int16_t w = tft.textWidth(msg);
        LayoutCheck::rect("Text", (int16_t)(Config::SCREEN_WIDTH / 2 - w / 2),
                           (int16_t)(Config::SCREEN_HEIGHT / 2 - tft.fontHeight()), w, tft.fontHeight());
    }
    LayoutCheck::finish();

    // Blockierend (Ersteinrichtung laeuft noch vor FetchTask::start(),
    // siehe main.cpp) - die HTTP-Hilfsfunktionen bringen ihr eigenes
    // Zeitlimit mit (Config::EPEVER_HTTP_TIMEOUT_MS/HTTP_TOTAL_TIMEOUT_MS).
    String token;
    if (!EpeverClient::login(email, password, token)) return false;

    EpeverClient::EquipmentListResult result;
    int httpCode = -1, apiStatus = -1;
    if (!EpeverClient::fetchEquipmentList(token, result, &httpCode, &apiStatus)) return false;

    if (result.total == 1 && result.count >= 1) {
        outDeviceNo = result.gatewayIds[0];
        return true;
    }
    // 0 oder mehrere Geraete - kein Raten, Aufrufer faellt auf den
    // manuellen Screen zurueck.
    return false;
}

bool runFirstTimeSetup(bool& outBack) {
    outBack = false;
    bool done = false;
    bool result = false;

    while (!done) {
        uint16_t accent = Display::systemColor();
        drawHeader(accent);
        Rect inputRow = drawInputRow(accent, appConfig.deviceNo);
        int16_t afterHint = drawHint(accent);

        bool valid = isValidSerial(appConfig.deviceNo);
        int16_t btnTop = (int16_t)(afterHint + 10);
        Rect backBtn = {10, btnTop, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        Rect goBtn = {10, (int16_t)(btnTop + 48), (int16_t)(Config::SCREEN_WIDTH - 20), 44};

        tft.drawRoundRect(backBtn.x, backBtn.y, backBtn.w, backBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::BACK), backBtn.x + backBtn.w / 2, backBtn.y + backBtn.h / 2);
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Zurueck-Knopf", backBtn.x, backBtn.y, backBtn.w, backBtn.h);

        tft.drawRoundRect(goBtn.x, goBtn.y, goBtn.w, goBtn.h, 4, valid ? accent : TFT_DARKGREY);
        if (valid) tft.fillRoundRect(goBtn.x, goBtn.y, goBtn.w, goBtn.h, 4, accent);
        tft.setTextColor(valid ? TFT_BLACK : TFT_DARKGREY, valid ? accent : TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::GO_BUTTON), goBtn.x + goBtn.w / 2, goBtn.y + goBtn.h / 2);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        LayoutCheck::rect("Los-gehts-Knopf", goBtn.x, goBtn.y, goBtn.w, goBtn.h);
        LayoutCheck::finish();

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            WebServer_::tick();
            delay(15);
        }

        if (HELP_BTN.contains(tap.x, tap.y)) {
            HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_SERIAL_NUMBER));
        } else if (inputRow.contains(tap.x, tap.y)) {
            bool confirmed = false;
            String v = Keypad::run(I18n::t(I18n::Str::SERIAL_NUMBER_PROMPT), appConfig.deviceNo, confirmed);
            if (confirmed) {
                v.trim();
                v.toUpperCase();
                appConfig.deviceNo = v;
            }
        } else if (backBtn.contains(tap.x, tap.y)) {
            outBack = true;
            done = true;
            result = false;
        } else if (valid && goBtn.contains(tap.x, tap.y)) {
            done = true;
            result = true;
        }
    }
    return result;
}

void runFromAccount() {
    bool done = false;
    while (!done) {
        uint16_t accent = Display::systemColor();
        drawHeader(accent);

        // Kleiner Hinweis direkt ueber dem Feld (Auftrag "Seriennummer
        // zu Konto verschieben") - sagt, dass das Feld antippbar ist,
        // auch wenn schon ein Wert eingetragen ist.
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::SERIAL_NUMBER_TAP_HINT), Config::SCREEN_WIDTH / 2, 32);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        LayoutCheck::rect("Antipp-Hinweis", 0, 32, Config::SCREEN_WIDTH, 16);

        Rect inputRow = drawInputRow(accent, appConfig.deviceNo);
        // Erklaerung unter dem Feld - vorher war dieser Platz leer
        // (Auftrag Teil B), nutzt dieselbe i18n-Erklaerung wie die
        // Ersteinrichtung (siehe runFirstTimeSetup()/drawHint()).
        int16_t afterHint = drawHint(accent);

        int16_t btnTop = (int16_t)(afterHint + 10);
        Rect backBtn = {10, btnTop, (int16_t)(Config::SCREEN_WIDTH - 20), 44};
        tft.drawRoundRect(backBtn.x, backBtn.y, backBtn.w, backBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::BACK), backBtn.x + backBtn.w / 2, backBtn.y + backBtn.h / 2);
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
            HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_SERIAL_NUMBER));
        } else if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        } else if (inputRow.contains(tap.x, tap.y)) {
            bool confirmed = false;
            String v = Keypad::run(I18n::t(I18n::Str::SERIAL_NUMBER_PROMPT), appConfig.deviceNo, confirmed);
            if (!confirmed) continue;
            v.trim();
            v.toUpperCase();
            if (!isValidSerial(v)) {
                uint16_t errAccent = Display::systemColor();
                tft.fillScreen(TFT_BLACK);
                tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
                tft.setTextColor(errAccent, TFT_BLACK);
                tft.setTextDatum(MC_DATUM);
                tft.drawString(I18n::t(I18n::Str::INVALID_INPUT_TEXT), Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT / 2);
                tft.setTextDatum(TL_DATUM);
                delay(1500);
                continue;
            }
            // Nur Info, kein Zwang (Auftrag) - HelpPopup::show() hat
            // bereits genau dieses Verhalten (ein OK-Knopf, blockierend).
            HelpPopup::show(I18n::t(I18n::Str::SERIAL_NUMBER_CHANGE_HINT));

            appConfig.deviceNo = v;
            AppConfigStore::save();
            AccountBridge::set(appConfig.portalEmail, appConfig.portalPassword, appConfig.deviceNo);
            FetchTask::sendCommand(FetchTask::Command::AccountChanged);
        }
    }
}

}
