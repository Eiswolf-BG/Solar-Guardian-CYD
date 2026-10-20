#include "week_view_screen.h"
#include "display.h"
#include "touch_input.h"
#include "weather_data.h"
#include "weather_icons.h"
#include "app_config.h"
#include "layout_check.h"
#include "location_screen.h"
#include "i18n.h"
#include "config.h"
#include "web_server.h"
#include <time.h>

namespace WeekViewScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr uint8_t ROW_COUNT = WeatherData::FORECAST_DAYS;
    // 29 statt 30 - gibt im Extremfall (zweizeiliger Ortsname) noch genug
    // Reserve bis zum "Zurueck"-Knopf (Auftrag "Ortsname darf umbrechen").
    constexpr int16_t ROW_H = 29;
    constexpr int16_t ICON_COL_W = 26;
    constexpr int16_t DAY_GAP = 4; // Mindestabstand Wochentag -> Temperatur (Auftrag "Morgen8°")
    constexpr int16_t TEMP_COL_W = 72;

    constexpr I18n::Str WEEKDAY_STRS[7] = {
        I18n::Str::WEEKDAY_SUN, I18n::Str::WEEKDAY_MON, I18n::Str::WEEKDAY_TUE, I18n::Str::WEEKDAY_WED,
        I18n::Str::WEEKDAY_THU, I18n::Str::WEEKDAY_FRI, I18n::Str::WEEKDAY_SAT,
    };

    void formatDecimal1(float value, char* out, size_t outSize) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", (double)value);
        if (appConfig.language == 0) {
            for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
        }
        strncpy(out, buf, outSize - 1);
        out[outSize - 1] = 0;
    }

    const char* dayLabel(uint8_t index, time_t dateEpoch) {
        if (index == 0) return I18n::t(I18n::Str::YIELD_TILE_TODAY);
        if (index == 1) return I18n::t(I18n::Str::WEEK_DAY_TOMORROW);
        struct tm t;
        localtime_r(&dateEpoch, &t);
        uint8_t wday = (t.tm_wday >= 0 && t.tm_wday <= 6) ? (uint8_t)t.tm_wday : 0;
        return I18n::t(WEEKDAY_STRS[wday]);
    }

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
    WeatherData::Data w = WeatherData::snapshot();

    bool done = false;
    while (!done) {
        LayoutCheck::begin("Wochenansicht");
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(accent, TFT_BLACK);

        // Kein "?"-Knopf mehr auf diesem Screen (Auftrag - braucht keine
        // Erklaerung). Der Ortsname bekommt dadurch die volle Breite -
        // mit dem jetzt kuerzeren "Stadtname (PLZ)"-Namen (siehe
        // geocoding_client.cpp) passt er meist auf eine Zeile; ist er
        // trotzdem zu lang, erst umbrechen (2 Zeilen), erst dann eine
        // Groesse kleiner - NIE mit "..." abschneiden (Auftrag).
        String titleText = appConfig.locationName.length() ? appConfig.locationName : String(I18n::t(I18n::Str::LOCATION_TITLE));
        constexpr int16_t TITLE_AVAIL_W = (int16_t)(Config::SCREEN_WIDTH - 16);
        constexpr int16_t LINE_H = 18;
        String titleLines[2];
        uint8_t titleLineCount;
        if (tft.textWidth(titleText) <= TITLE_AVAIL_W) {
            titleLines[0] = titleText;
            titleLineCount = 1;
        } else {
            titleLineCount = wrapWords(titleText, TITLE_AVAIL_W, titleLines, 2);
            if (titleLineCount == 0) { titleLines[0] = titleText; titleLineCount = 1; }
        }
        tft.setTextDatum(TC_DATUM);
        for (uint8_t i = 0; i < titleLineCount; i++) {
            tft.drawString(titleLines[i], Config::SCREEN_WIDTH / 2, (int16_t)(6 + i * LINE_H));
        }
        int16_t titleBottom = (int16_t)(6 + titleLineCount * LINE_H);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        int16_t hintY = titleBottom;
        tft.drawString(I18n::t(I18n::Str::LOCATION_CHANGE_HINT), Config::SCREEN_WIDTH / 2, hintY);
        tft.setTextDatum(TL_DATUM);
        // Titel + Hinweis zusammen als EINE Tipp-Flaeche (Auftrag: oeffnet
        // den Standort-Screen).
        int16_t titleTapBottom = (int16_t)(hintY + LINE_H);
        Rect titleTapZone = {0, 0, Config::SCREEN_WIDTH, titleTapBottom};
        LayoutCheck::rect("Titel", titleTapZone.x, titleTapZone.y, titleTapZone.w, titleTapZone.h);

        int16_t rowsTop = (int16_t)(titleTapBottom + 4);

        uint8_t n = w.dayCount < ROW_COUNT ? w.dayCount : ROW_COUNT;
        Rect rowRects[ROW_COUNT];
        for (uint8_t i = 0; i < n; i++) {
            Rect r = {8, (int16_t)(rowsTop + i * ROW_H), (int16_t)(Config::SCREEN_WIDTH - 16), (int16_t)(ROW_H - 2)};
            rowRects[i] = r;
            const WeatherData::DayForecast& d = w.days[i];

            int16_t iconCx = (int16_t)(r.x + ICON_COL_W / 2);
            int16_t iconCy = (int16_t)(r.y + r.h / 2);
            WeatherIcons::draw(tft, iconCx, iconCy, (int16_t)(ICON_COL_W - 4), d.weatherCode, true);
            LayoutCheck::rect("Symbol", (int16_t)(iconCx - (ICON_COL_W - 4) / 2), (int16_t)(iconCy - (ICON_COL_W - 4) / 2),
                               (int16_t)(ICON_COL_W - 4), (int16_t)(ICON_COL_W - 4));

            int16_t dayX = (int16_t)(r.x + ICON_COL_W);
            tft.setTextColor(accent, TFT_BLACK);
            tft.setTextSize(1);
            const char* label = dayLabel(i, d.dateEpoch);
            int16_t labelW = tft.textWidth(label);
            tft.setTextDatum(ML_DATUM);
            tft.drawString(label, dayX, iconCy);
            tft.setTextDatum(TL_DATUM);
            LayoutCheck::rect("Wochentag", dayX, (int16_t)(iconCy - 8), labelW, 16);

            // Mind. DAY_GAP Abstand zur TATSAECHLICHEN Label-Breite, nicht
            // zu einer festen Spaltenbreite - "Morgen"/"Heute" sind
            // breiter als die 2-Buchstaben-Wochentagskuerzel und klebten
            // sonst an der Temperatur (Auftrag "Morgen8°").
            int16_t tempX = (int16_t)(dayX + labelW + DAY_GAP);
            char maxBuf[8], minBuf[8];
            snprintf(maxBuf, sizeof(maxBuf), "%.0f", (double)d.tempMaxC);
            snprintf(minBuf, sizeof(minBuf), "%.0f", (double)d.tempMinC);
            String tempLine = String(maxBuf) + "° / " + String(minBuf) + "°";
            tft.setTextDatum(ML_DATUM);
            tft.drawString(tempLine, tempX, iconCy);
            tft.setTextDatum(TL_DATUM);
            LayoutCheck::rect("Temperatur", tempX, (int16_t)(iconCy - 8), TEMP_COL_W, 16);

            if (d.sunshineValid) {
                int16_t sunX = (int16_t)(tempX + TEMP_COL_W);
                int16_t sunAvailW = (int16_t)(r.x + r.w - sunX - 4);
                char sunBuf[8];
                formatDecimal1(d.sunshineHours, sunBuf, sizeof(sunBuf));
                String sunLine = String(sunBuf) + " " + I18n::t(I18n::Str::HOURS_UNIT);
                tft.setTextSize(1);
                // "sonst weglassen" (Auftrag) - nur zeichnen, wenn die
                // tatsaechliche Pixelbreite in den verbleibenden Platz
                // passt, sonst bleibt die Zeile ohne Sonnenstunden.
                if (sunAvailW > 0 && tft.textWidth(sunLine) <= sunAvailW) {
                    tft.setTextDatum(ML_DATUM);
                    tft.drawString(sunLine, sunX, iconCy);
                    tft.setTextDatum(TL_DATUM);
                    LayoutCheck::rect("Sonnenstunden", sunX, (int16_t)(iconCy - 8), sunAvailW, 16);
                }
            }
        }

        constexpr int16_t BACK_H = 40;
        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 8 - BACK_H), (int16_t)(Config::SCREEN_WIDTH - 20), BACK_H};
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

        if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        } else if (titleTapZone.contains(tap.x, tap.y)) {
            LocationScreen::run();
            w = WeatherData::snapshot(); // evtl. neuer Standort/neues Wetter nach dem Standort-Screen
        }
    }
}

}
