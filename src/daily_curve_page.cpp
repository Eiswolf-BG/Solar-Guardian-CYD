#include "daily_curve_page.h"
#include "display.h"
#include "solar_data.h"
#include "app_config.h"
#include "layout_utils.h"
#include "layout_check.h"
#include "tile_sprite.h"
#include "redraw_diag.h"
#include "i18n.h"
#include "config.h"
#include <time.h>
#include <math.h>

namespace DailyCurvePage {

namespace {
    // Alle Offsets rechnerisch verkettet (nicht einzeln fest verdrahtet),
    // gleiches Prinzip wie power_page.cpp - siehe dort fuer die
    // ausfuehrliche Begruendung (Konflikt bei zu knapp bemessenen
    // Zeilenhoehen).
    constexpr int16_t LINE_H_1 = 18;
    constexpr int16_t MAX_LABEL_OFFSET = 22;
    constexpr int16_t MAX_LABEL_H = 28;
    constexpr int16_t AXIS_LABEL_OFFSET = (int16_t)(MAX_LABEL_OFFSET + MAX_LABEL_H + 2);
    constexpr int16_t AXIS_LABEL_H = LINE_H_1;
    constexpr int16_t CHART_TOP_OFFSET = (int16_t)(AXIS_LABEL_OFFSET + AXIS_LABEL_H + 1);
    constexpr int16_t CHART_H = 130;
    constexpr int16_t CHART_SIDE_MARGIN = 10;
    constexpr int16_t TIMEMARKS_OFFSET = (int16_t)(CHART_TOP_OFFSET + CHART_H + 1);
    constexpr int16_t TIMEMARKS_H = LINE_H_1;
    constexpr uint8_t MAX_SIZES[] = {2, 1};

    uint32_t lastSeenChangeCounter = 0xFFFFFFFFu;
    int8_t lastSeenMinute = -1;
    bool firstCheck = true;

    String lastMaxSig = "\x01";
    String lastChartSig = "\x01";
    bool timeMarksDrawn = false;
    uint8_t timeMarksLang = 0xFF;

    // Etwa halbe Helligkeit der Systemfarbe fuer die Diagramm-Hilfslinien
    // - gleiche Kanal-Halbierung wie in power_page.cpp/years_page.cpp,
    // hier bewusst lokal dupliziert (siehe CLAUDE.md Code-Stil).
    uint16_t dimColor(uint16_t c) {
        uint8_t r = (uint8_t)((c >> 11) & 0x1F) >> 1;
        uint8_t g = (uint8_t)((c >> 5) & 0x3F) >> 1;
        uint8_t b = (uint8_t)(c & 0x1F) >> 1;
        return (uint16_t)((r << 11) | (g << 5) | b);
    }

    // Zahl auf eine "runde" Obergrenze fuer die Y-Achse aufrunden
    // (1-2-5-Schema), gleiche Logik wie power_page.cpp.
    float niceCeil(float v) {
        if (v <= 1.0f) return 100.0f;
        float mag = powf(10.0f, floorf(log10f(v)));
        float norm = v / mag;
        float niceNorm = (norm <= 1.0f) ? 1.0f : (norm <= 2.0f) ? 2.0f : (norm <= 5.0f) ? 5.0f : 10.0f;
        return niceNorm * mag;
    }

    // Wie LayoutUtils::fitText(), aber OHNE "..."-Kuerzung - Zahlen werden
    // nie gekuerzt (siehe CLAUDE.md Layout-Regel), gleiches Prinzip wie
    // power_page.cpp/years_page.cpp, hier bewusst lokal dupliziert.
    LayoutUtils::FitResult fitNumberNoTruncate(TFT_eSPI& t, const String& text, int16_t maxW, int16_t maxH,
                                                const uint8_t* sizes, uint8_t sizeCount) {
        constexpr int16_t pad = 4;
        int16_t availW = maxW - 2 * pad;
        int16_t availH = maxH - 2 * pad;
        for (uint8_t i = 0; i < sizeCount; i++) {
            t.setTextSize(sizes[i]);
            if (t.textWidth(text) <= availW && t.fontHeight() <= availH) {
                return {sizes[i], text};
            }
        }
        uint8_t smallest = sizes[sizeCount - 1];
        t.setTextSize(smallest);
        return {smallest, text};
    }
}

void draw(int16_t areaTop, int16_t /*areaHeight*/, bool forceRedraw) {
    SolarData::Data snap = SolarData::snapshot();
    uint16_t accent = Display::systemColor();
    uint16_t dimAccent = dimColor(accent);

    if (forceRedraw) {
        lastMaxSig = "\x01";
        lastChartSig = "\x01";
        timeMarksDrawn = false;
    }

    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(accent, TFT_BLACK);
    tft.drawString(I18n::t(I18n::Str::PAGE_DAILY_TITLE), Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + 6));
    tft.setTextDatum(TL_DATUM);
    LayoutCheck::rect("Titel", 0, areaTop, Config::SCREEN_WIDTH, 16);

    // Tages-Hoechstwert ueber dem Diagramm ("Max: 1234 W", Einheit in der
    // Systemfarbe wie der Wert selbst - Auftrag "Einheiten in der
    // Systemfarbe").
    float maxVal = 0.0f;
    for (uint16_t i = 0; i < snap.dailyCurveCount; i++) {
        if ((float)snap.dailyCurve[i].watts > maxVal) maxVal = (float)snap.dailyCurve[i].watts;
    }
    char maxStr[32];
    if (snap.dailyCurveCount > 0) {
        snprintf(maxStr, sizeof(maxStr), I18n::t(I18n::Str::DAILY_CURVE_MAX_LABEL_FMT), (double)maxVal, I18n::t(I18n::Str::POWER_UNIT_W));
    } else {
        strncpy(maxStr, "--", sizeof(maxStr));
    }
    int16_t maxZoneTop = (int16_t)(areaTop + MAX_LABEL_OFFSET);
    int16_t maxZoneW = (int16_t)(Config::SCREEN_WIDTH - 20);
    uint16_t maxColor = (snap.dailyCurveCount > 0) ? accent : TFT_DARKGREY;
    String maxSig = String(maxStr) + (maxColor == accent ? "1" : "0");
    if (maxSig != lastMaxSig) {
        lastMaxSig = maxSig;
        RedrawDiag::log("Tagesverlauf-Max", "Inhalt geaendert");
        TileSprite::drawInto(10, maxZoneTop, maxZoneW, MAX_LABEL_H, [&](TFT_eSPI& t) {
            t.setTextColor(maxColor, TFT_BLACK);
            LayoutUtils::FitResult maxFit = fitNumberNoTruncate(t, maxStr, maxZoneW, MAX_LABEL_H, MAX_SIZES, 2);
            t.setTextDatum(TC_DATUM);
            t.drawString(maxFit.text, (int16_t)(maxZoneW / 2), 0);
            t.setTextDatum(TL_DATUM);
        });
    }
    LayoutCheck::rect("Max", 10, maxZoneTop, maxZoneW, MAX_LABEL_H);

    // Kurve des ganzen Tages, gleiches Prinzip wie power_page.cpp's
    // 3h-Kurve - inkl. Y-Achsen-Beschriftung in EINEM Off-Screen-Puffer.
    int16_t chartTop = (int16_t)(areaTop + CHART_TOP_OFFSET);
    int16_t chartLeft = CHART_SIDE_MARGIN;
    int16_t chartRight = (int16_t)(Config::SCREEN_WIDTH - CHART_SIDE_MARGIN);
    int16_t chartBottom = (int16_t)(chartTop + CHART_H);
    int16_t axisLabelY = (int16_t)(areaTop + AXIS_LABEL_OFFSET);
    int16_t chartBlockY = (int16_t)(areaTop + AXIS_LABEL_OFFSET);
    int16_t chartBlockH = (int16_t)(CHART_H + AXIS_LABEL_H + 1);
    LayoutCheck::rect("Kurve", chartLeft, chartTop, (int16_t)(chartRight - chartLeft), CHART_H);

    struct tm nowTm;
    bool haveNow = getLocalTime(&nowTm, 5);
    constexpr uint16_t WINDOW_START_MIN = (uint16_t)(Config::DAILY_CURVE_START_HOUR * 60);
    constexpr uint16_t WINDOW_END_MIN = (uint16_t)(Config::DAILY_CURVE_END_HOUR * 60);
    constexpr uint16_t WINDOW_SPAN_MIN = (uint16_t)(WINDOW_END_MIN - WINDOW_START_MIN);

    String chartSig;
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u", (unsigned)snap.dailyCurveCount);
        chartSig = buf;
        for (uint16_t i = 0; i < snap.dailyCurveCount; i++) {
            snprintf(buf, sizeof(buf), ",%u:%u", (unsigned)snap.dailyCurve[i].minutesSinceMidnight, (unsigned)snap.dailyCurve[i].watts);
            chartSig += buf;
        }
        if (haveNow) { char nb[8]; snprintf(nb, sizeof(nb), "|%d", nowTm.tm_hour * 60 + nowTm.tm_min); chartSig += nb; }
    }
    if (chartSig != lastChartSig) {
        lastChartSig = chartSig;
        RedrawDiag::log("Tagesverlauf-Kurve", "Inhalt geaendert");
        TileSprite::drawInto(0, chartBlockY, Config::SCREEN_WIDTH, chartBlockH, [&](TFT_eSPI& t) {
            int16_t localChartTop = (int16_t)(chartTop - chartBlockY);
            int16_t localChartBottom = (int16_t)(chartBottom - chartBlockY);
            int16_t localAxisLabelY = (int16_t)(axisLabelY - chartBlockY);
            if (snap.dailyCurveCount == 0) {
                t.setTextColor(TFT_DARKGREY, TFT_BLACK);
                t.setTextDatum(MC_DATUM);
                t.drawString(I18n::t(I18n::Str::DAILY_CURVE_EMPTY), (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(localChartTop + CHART_H / 2));
                t.setTextDatum(TL_DATUM);
                return;
            }
            float axisMax = niceCeil(maxVal);

            // Reine Zahl+Einheit, kein "Max:"-Praefix (das steht schon in
            // der grossen Ueberschrift darueber) - gleiches Format wie
            // power_page.cpp's Achsenbeschriftung, bewusst nicht ueber
            // i18n (enthaelt keine uebersetzbaren Woerter).
            char axisLabel[16];
            snprintf(axisLabel, sizeof(axisLabel), "%.0f %s", (double)axisMax, I18n::t(I18n::Str::POWER_UNIT_W));
            t.setTextColor(accent, TFT_BLACK);
            t.setTextDatum(TL_DATUM);
            t.drawString(axisLabel, chartLeft, localAxisLabelY);

            uint16_t gridColor = dimAccent;
            uint16_t curveColor = accent;
            for (uint8_t i = 1; i <= 4; i++) {
                int16_t gy = (int16_t)(localChartBottom - (CHART_H * i) / 4);
                t.drawFastHLine(chartLeft, gy, (int16_t)(chartRight - chartLeft), gridColor);
            }

            int16_t prevX = 0, prevY = 0;
            bool havePrev = false;
            for (uint16_t i = 0; i < snap.dailyCurveCount; i++) {
                uint16_t minute = snap.dailyCurve[i].minutesSinceMidnight;
                float frac = (float)(minute - WINDOW_START_MIN) / (float)WINDOW_SPAN_MIN;
                if (frac < 0.0f) frac = 0.0f;
                if (frac > 1.0f) frac = 1.0f;
                int16_t px = (int16_t)(chartLeft + frac * (chartRight - chartLeft));
                float vFrac = (float)snap.dailyCurve[i].watts / axisMax;
                if (vFrac < 0.0f) vFrac = 0.0f;
                if (vFrac > 1.0f) vFrac = 1.0f;
                int16_t py = (int16_t)(localChartBottom - vFrac * CHART_H);
                if (havePrev) t.drawLine(prevX, prevY, px, py, curveColor);
                prevX = px; prevY = py; havePrev = true;
            }

            // "Jetzt"-Markierung: senkrechte Linie bei der aktuellen
            // Uhrzeit, NUR solange diese tatsaechlich im Anzeigefenster
            // liegt (vor 05:00/nach 22:00 ist die Kurve der abgeschlossene
            // Vortag und braucht keine "Jetzt"-Linie, siehe Auftrag).
            if (haveNow) {
                uint16_t nowMinute = (uint16_t)(nowTm.tm_hour * 60 + nowTm.tm_min);
                if (nowMinute >= WINDOW_START_MIN && nowMinute < WINDOW_END_MIN) {
                    float nowFrac = (float)(nowMinute - WINDOW_START_MIN) / (float)WINDOW_SPAN_MIN;
                    int16_t nowX = (int16_t)(chartLeft + nowFrac * (chartRight - chartLeft));
                    t.drawFastVLine(nowX, localChartTop, CHART_H, curveColor);
                }
            }
        });
    }
    {
        if (snap.dailyCurveCount > 0) {
            float axisMax = niceCeil(maxVal);
            char axisLabel[16];
            snprintf(axisLabel, sizeof(axisLabel), "%.0f %s", (double)axisMax, I18n::t(I18n::Str::POWER_UNIT_W));
            tft.setTextSize(1);
            int16_t lw = tft.textWidth(axisLabel);
            int16_t lh = tft.fontHeight();
            LayoutCheck::rect("Achsenbeschriftung", chartLeft, axisLabelY, lw, lh);
        }
    }

    // Zeitmarken unter der Kurve (feste Stunden 05/09/13/17/22 - reine
    // Zahlen, nicht sprachabhaengig, wie "-3h" bei power_page.cpp).
    int16_t timeMarksY = (int16_t)(areaTop + TIMEMARKS_OFFSET);
    if (!timeMarksDrawn || timeMarksLang != appConfig.language) {
        timeMarksDrawn = true;
        timeMarksLang = appConfig.language;
        tft.setTextSize(1);
        int16_t tmLineH = tft.fontHeight();
        int16_t tmSpriteH = (int16_t)(tmLineH + 2);
        TileSprite::drawInto(0, timeMarksY, Config::SCREEN_WIDTH, tmSpriteH, [&](TFT_eSPI& t) {
            t.setTextColor(TFT_DARKGREY, TFT_BLACK);
            t.setTextDatum(TL_DATUM);
            t.drawString(String(Config::DAILY_CURVE_START_HOUR), chartLeft, 0);
            t.setTextDatum(TC_DATUM);
            t.drawString("9", (int16_t)(chartLeft + (chartRight - chartLeft) * (9 - Config::DAILY_CURVE_START_HOUR) / (Config::DAILY_CURVE_END_HOUR - Config::DAILY_CURVE_START_HOUR)), 0);
            t.drawString("13", (int16_t)(chartLeft + (chartRight - chartLeft) * (13 - Config::DAILY_CURVE_START_HOUR) / (Config::DAILY_CURVE_END_HOUR - Config::DAILY_CURVE_START_HOUR)), 0);
            t.drawString("17", (int16_t)(chartLeft + (chartRight - chartLeft) * (17 - Config::DAILY_CURVE_START_HOUR) / (Config::DAILY_CURVE_END_HOUR - Config::DAILY_CURVE_START_HOUR)), 0);
            t.setTextDatum(TR_DATUM);
            t.drawString(String(Config::DAILY_CURVE_END_HOUR), chartRight, 0);
            t.setTextDatum(TL_DATUM);
        });
        LayoutCheck::bufferRect("Zeitmarken-Puffer", Config::SCREEN_WIDTH, tmSpriteH, Config::SCREEN_WIDTH, tmLineH);
    }
    LayoutCheck::rect("Zeitmarken", 0, timeMarksY, Config::SCREEN_WIDTH, 16);
}

bool update() {
    SolarData::Data snap = SolarData::snapshot();
    struct tm t;
    int8_t minute = getLocalTime(&t, 5) ? (int8_t)t.tm_min : (int8_t)-1;

    bool solarChanged = snap.changeCounter != lastSeenChangeCounter;
    bool minuteTicked = minute != lastSeenMinute;
    bool changed = firstCheck || solarChanged || minuteTicked;
    if (changed) {
        firstCheck = false;
        lastSeenChangeCounter = snap.changeCounter;
        lastSeenMinute = minute;
    }
    return changed;
}

}
