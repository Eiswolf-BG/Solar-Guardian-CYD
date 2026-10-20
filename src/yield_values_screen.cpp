#include "yield_values_screen.h"
#include "display.h"
#include "touch_input.h"
#include "keypad.h"
#include "yield_ledger.h"
#include "app_config.h"
#include "help_popup.h"
#include "layout_utils.h"
#include "layout_check.h"
#include "i18n.h"
#include "config.h"
#include "web_server.h"

namespace YieldValuesScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr Rect HELP_BTN = {(int16_t)(Config::SCREEN_WIDTH - 36), 4, 32, 32};

    void showMessage(const char* msg) {
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(msg, Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Ziffern, hoechstens ein Komma/Punkt, hoechstens 2 Nachkommastellen.
    bool parseKwhInput(const String& input, float& outValue) {
        if (input.length() == 0) return false;
        String s = input;
        s.replace(',', '.');
        int dotCount = 0;
        for (uint16_t i = 0; i < s.length(); i++) {
            char c = s[i];
            if (c == '.') { dotCount++; if (dotCount > 1) return false; }
            else if (c < '0' || c > '9') return false;
        }
        int dotPos = s.indexOf('.');
        if (dotPos >= 0 && (int)(s.length() - dotPos - 1) > 2) return false;
        if (s == ".") return false;
        outValue = s.toFloat();
        return true;
    }

    void formatKwh(float value, char* out, size_t outSize) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", (double)value);
        if (appConfig.language == 0) {
            for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
        }
        strncpy(out, buf, outSize - 1);
        out[outSize - 1] = 0;
    }

    void runSetValue(YieldLedger::SetTargetResult (*setter)(float)) {
        bool confirmed = false;
        String input = Keypad::run(I18n::t(I18n::Str::SET_VALUE_PROMPT), "", confirmed, true);
        if (!confirmed) return;

        float value;
        if (!parseKwhInput(input, value)) {
            showMessage(I18n::t(I18n::Str::INVALID_INPUT_TEXT));
            delay(1500);
            return;
        }

        YieldLedger::SetTargetResult result = setter(value);
        if (result == YieldLedger::SetTargetResult::Negative) {
            showMessage(I18n::t(I18n::Str::NEGATIVE_RESULT_TEXT));
            delay(1500);
        } else if (result == YieldLedger::SetTargetResult::NoDataYet) {
            showMessage(I18n::t(I18n::Str::NO_ACCOUNT_CONFIGURED));
            delay(1500);
        }
    }
}

void run() {
    bool done = false;
    while (!done) {
        LayoutCheck::begin("Ertragswerte");
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::MENU_ITEM_YIELD_VALUES), Config::SCREEN_WIDTH / 2, 14);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

        YieldLedger::Values v = YieldLedger::current();
        char todayStr[16], weekStr[16], monthStr[16], yearStr[16];
        if (v.valid) {
            formatKwh(v.todayKwh, todayStr, sizeof(todayStr));
            formatKwh(v.weekKwh, weekStr, sizeof(weekStr));
            formatKwh(v.monthKwh, monthStr, sizeof(monthStr));
            formatKwh(v.yearKwh, yearStr, sizeof(yearStr));
        } else {
            strcpy(todayStr, "--"); strcpy(weekStr, "--"); strcpy(monthStr, "--"); strcpy(yearStr, "--");
        }

        // Vier Zeilen UNTEREINANDER statt einer Zeile mit "/" getrennt -
        // bei einer Zeile muessten alle vier Werte in eine einzige
        // Bildschirmbreite passen, was bei grossen Werten (Worst Case
        // "Jahr 99999,99 kWh") selbst bei kleinster Schrift nicht
        // garantiert ist. Zahlen werden NIE gekuerzt (siehe CLAUDE.md
        // Layout-Regel) - untereinander hat jede Zeile die volle
        // Bildschirmbreite zur Verfuegung, fitText() kuerzt dadurch nie.
        constexpr int16_t SUMMARY_TOP = 36;
        constexpr int16_t SUMMARY_LINE_H = 20;
        constexpr uint8_t SUMMARY_SIZES[] = {1};
        struct SummaryRow { I18n::Str labelKey; const char* value; };
        SummaryRow summaryRows[] = {
            {I18n::Str::YIELD_TILE_TODAY, todayStr},
            {I18n::Str::YIELD_TILE_WEEK, weekStr},
            {I18n::Str::YIELD_TILE_MONTH, monthStr},
            {I18n::Str::YIELD_TILE_YEAR, yearStr},
        };
        tft.setTextDatum(TC_DATUM);
        for (uint8_t i = 0; i < 4; i++) {
            char line[40];
            snprintf(line, sizeof(line), "%s %s %s", I18n::t(summaryRows[i].labelKey),
                     summaryRows[i].value, I18n::t(I18n::Str::KWH_UNIT));
            LayoutUtils::FitResult fit = LayoutUtils::fitText(tft, line, (int16_t)(Config::SCREEN_WIDTH - 16),
                                                               SUMMARY_LINE_H, SUMMARY_SIZES, 1);
            tft.drawString(fit.text, Config::SCREEN_WIDTH / 2, (int16_t)(SUMMARY_TOP + i * SUMMARY_LINE_H));
        }
        tft.setTextDatum(TL_DATUM);

        constexpr int16_t TOP = (int16_t)(SUMMARY_TOP + 4 * SUMMARY_LINE_H + 8);
        constexpr int16_t ROW_H = 40;
        constexpr int16_t ROW_GAP = 8;
        constexpr int16_t NOTE_H = 18;
        // Nur noch zwei Eintraege - "Woche setzen" entfaellt, die Woche
        // ergibt sich automatisch aus den Tageswerten (gleitendes
        // 7-Tage-Fenster, siehe CLAUDE.md).
        Rect monthBtn = {10, TOP, (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
        Rect yearBtn  = {10, (int16_t)(TOP + (ROW_H + ROW_GAP)), (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
        // Eigene Zeile fuer den Hinweistext, damit er den Zurueck-Knopf
        // nicht beruehrt (Layout-Regel, siehe CLAUDE.md).
        int16_t noteY = (int16_t)(yearBtn.y + yearBtn.h + ROW_GAP);
        Rect backBtn  = {10, (int16_t)(noteY + NOTE_H + ROW_GAP), (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};

        // Fuer Button-Beschriftungen immer explizit Size 1 setzen statt
        // sich auf den von der Zusammenfassungs-Schleife (fitText) zuletzt
        // gesetzten Wert zu verlassen - sonst koennte containedRect() hier
        // eine falsche (zu grosse) Textbreite/-hoehe pruefen.
        tft.setTextSize(1);
        tft.drawRoundRect(monthBtn.x, monthBtn.y, monthBtn.w, monthBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        const char* monthLabel = I18n::t(I18n::Str::SET_MONTH_BUTTON);
        tft.drawString(monthLabel, monthBtn.x + monthBtn.w / 2, monthBtn.y + monthBtn.h / 2);
        tft.setTextDatum(TL_DATUM);
        {
            int16_t tw = tft.textWidth(monthLabel);
            int16_t th = tft.fontHeight();
            LayoutCheck::containedRect("Monat-setzen-Beschriftung", monthBtn.x, monthBtn.y, monthBtn.w, monthBtn.h,
                                        (int16_t)(monthBtn.x + monthBtn.w / 2 - tw / 2),
                                        (int16_t)(monthBtn.y + monthBtn.h / 2 - th / 2), tw, th);
        }

        tft.drawRoundRect(yearBtn.x, yearBtn.y, yearBtn.w, yearBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        const char* yearLabel = I18n::t(I18n::Str::SET_YEAR_BUTTON);
        tft.drawString(yearLabel, yearBtn.x + yearBtn.w / 2, yearBtn.y + yearBtn.h / 2);
        tft.setTextDatum(TL_DATUM);
        {
            int16_t tw = tft.textWidth(yearLabel);
            int16_t th = tft.fontHeight();
            LayoutCheck::containedRect("Jahr-setzen-Beschriftung", yearBtn.x, yearBtn.y, yearBtn.w, yearBtn.h,
                                        (int16_t)(yearBtn.x + yearBtn.w / 2 - tw / 2),
                                        (int16_t)(yearBtn.y + yearBtn.h / 2 - th / 2), tw, th);
        }

        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::VALUE_INCLUDING_TODAY_NOTE), Config::SCREEN_WIDTH / 2, noteY);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(accent, TFT_BLACK);

        tft.drawRoundRect(backBtn.x, backBtn.y, backBtn.w, backBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        const char* backLabel = I18n::t(I18n::Str::BACK);
        tft.drawString(backLabel, backBtn.x + backBtn.w / 2, backBtn.y + backBtn.h / 2);
        tft.setTextDatum(TL_DATUM);
        {
            int16_t tw = tft.textWidth(backLabel);
            int16_t th = tft.fontHeight();
            LayoutCheck::containedRect("Zurueck-Beschriftung", backBtn.x, backBtn.y, backBtn.w, backBtn.h,
                                        (int16_t)(backBtn.x + backBtn.w / 2 - tw / 2),
                                        (int16_t)(backBtn.y + backBtn.h / 2 - th / 2), tw, th);
        }

        LayoutCheck::finish();

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            WebServer_::tick();
            delay(15);
        }

        if (HELP_BTN.contains(tap.x, tap.y)) {
            HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_YIELD_VALUES));
        } else if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        } else if (monthBtn.contains(tap.x, tap.y)) {
            runSetValue(YieldLedger::setMonthTarget);
        } else if (yearBtn.contains(tap.x, tap.y)) {
            runSetValue(YieldLedger::setYearTarget);
        }
    }
}

}
