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
    String lastProgressSig = "\x01";
    bool timeMarksDrawn = false;
    uint8_t timeMarksLang = 0xFF;

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

    if (forceRedraw) {
        lastMaxSig = "\x01";
        lastChartSig = "\x01";
        lastProgressSig = "\x01";
        timeMarksDrawn = false;
    }

    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(accent, TFT_BLACK);
    tft.drawString(I18n::t(I18n::Str::PAGE_DAILY_TITLE), Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + 6));
    tft.setTextDatum(TL_DATUM);
    LayoutCheck::rect("Titel", 0, areaTop, Config::SCREEN_WIDTH, 16);

    // Tages-Hoechstwert ueber dem Diagramm ("Max: 1234 W") - Auftrag
    // "Farbregeln vereinheitlichen": Label "Max:" und Einheit "W" in der
    // Systemfarbe, der abgefragte Zahlenwert selbst in der Sekundaerfarbe.
    float maxVal = 0.0f;
    for (uint16_t i = 0; i < snap.dailyCurveCount; i++) {
        if ((float)snap.dailyCurve[i].watts > maxVal) maxVal = (float)snap.dailyCurve[i].watts;
    }
    const char* maxLabel = I18n::t(I18n::Str::DAILY_CURVE_MAX_LABEL);
    char maxNum[16];
    char maxUnit[8] = {0};
    if (snap.dailyCurveCount > 0) {
        snprintf(maxNum, sizeof(maxNum), "%.0f", (double)maxVal);
        strncpy(maxUnit, I18n::t(I18n::Str::POWER_UNIT_W), sizeof(maxUnit) - 1);
    } else {
        strncpy(maxNum, "--", sizeof(maxNum));
    }
    char maxFull[40];
    snprintf(maxFull, sizeof(maxFull), "%s %s %s", maxLabel, maxNum, maxUnit);
    int16_t maxZoneTop = (int16_t)(areaTop + MAX_LABEL_OFFSET);
    int16_t maxZoneW = (int16_t)(Config::SCREEN_WIDTH - 20);
    String maxSig = String(maxFull);
    if (maxSig != lastMaxSig) {
        lastMaxSig = maxSig;
        RedrawDiag::log("Tagesverlauf-Max", "Inhalt geaendert");
        TileSprite::drawInto(10, maxZoneTop, maxZoneW, MAX_LABEL_H, [&](TFT_eSPI& t) {
            LayoutUtils::FitResult maxFit = fitNumberNoTruncate(t, maxFull, maxZoneW, MAX_LABEL_H, MAX_SIZES, 2);
            t.setTextSize(maxFit.textSize);
            String labelPart = String(maxLabel) + " ";
            int16_t labelW = t.textWidth(labelPart);
            int16_t numW = t.textWidth(maxNum);
            int16_t totalW = t.textWidth(maxFull);
            int16_t startX = (int16_t)(maxZoneW / 2 - totalW / 2);
            t.setTextDatum(TL_DATUM);
            t.setTextColor(accent, TFT_BLACK);
            t.drawString(labelPart, startX, 0);
            t.setTextColor(Display::valueColor(snap.dailyCurveCount > 0, accent), TFT_BLACK);
            t.drawString(maxNum, (int16_t)(startX + labelW), 0);
            t.setTextColor(accent, TFT_BLACK);
            t.drawString(String(" ") + maxUnit, (int16_t)(startX + labelW + numW), 0);
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
    }
    if (chartSig != lastChartSig) {
        lastChartSig = chartSig;
        RedrawDiag::log("Tagesverlauf-Kurve", "Inhalt geaendert");
        TileSprite::drawInto(0, chartBlockY, Config::SCREEN_WIDTH, chartBlockH, [&](TFT_eSPI& t) {
            int16_t localChartTop = (int16_t)(chartTop - chartBlockY);
            int16_t localChartBottom = (int16_t)(chartBottom - chartBlockY);
            int16_t localAxisLabelY = (int16_t)(axisLabelY - chartBlockY);
            if (snap.dailyCurveCount == 0) {
                t.setTextColor(Display::secondaryColor(), TFT_BLACK);
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

            uint16_t gridColor = Display::secondaryColor();
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
            t.setTextColor(Display::secondaryColor(), TFT_BLACK);
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

    // Fortschrittsbalken (Auftrag "Fortschrittsbalken im Tagesverlauf-
    // Screen am Geraet") - ersetzt die vorherige senkrechte "Jetzt"-Linie
    // im Diagramm (wurde faelschlich fuer einen Grafikfehler gehalten).
    // Gleicher Zeitbereich/gleiche Breite wie das Diagramm (05:00-22:00
    // Uhr), direkt unter der Zeitachse. Vor 05:00 leer, ab 22:00 voll -
    // das deckt den Nachtmodus automatisch mit ab (22:00-04:30 liegt
    // komplett in der "danach"/"davor"-Klemmung, kein Sonderfall noetig).
    constexpr int16_t PROGRESS_GAP = 2;
    constexpr int16_t PROGRESS_H = 9;
    tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
    int16_t progressY = (int16_t)(timeMarksY + (tft.fontHeight() + 2) + PROGRESS_GAP);
    float nowFracClamped = 0.0f;
    if (haveNow) {
        uint16_t nowMinute = (uint16_t)(nowTm.tm_hour * 60 + nowTm.tm_min);
        if (nowMinute <= WINDOW_START_MIN) nowFracClamped = 0.0f;
        else if (nowMinute >= WINDOW_END_MIN) nowFracClamped = 1.0f;
        else nowFracClamped = (float)(nowMinute - WINDOW_START_MIN) / (float)WINDOW_SPAN_MIN;
    }
    String progressSig = String(haveNow ? (nowTm.tm_hour * 60 + nowTm.tm_min) : -1);
    if (progressSig != lastProgressSig) {
        lastProgressSig = progressSig;
        TileSprite::drawInto(0, progressY, Config::SCREEN_WIDTH, PROGRESS_H, [&](TFT_eSPI& t) {
            uint16_t dim = Display::secondaryColor();
            t.drawRect(chartLeft, 0, (int16_t)(chartRight - chartLeft), PROGRESS_H, dim);
            int16_t fillW = (int16_t)((chartRight - chartLeft - 2) * nowFracClamped);
            if (fillW > 0) t.fillRect((int16_t)(chartLeft + 1), 1, fillW, (int16_t)(PROGRESS_H - 2), accent);
            // Kleine Stundenstriche ohne Beschriftung, fuer jede volle
            // Stunde zwischen 05 und 22 Uhr (Auftrag).
            for (uint8_t hh = Config::DAILY_CURVE_START_HOUR; hh <= Config::DAILY_CURVE_END_HOUR; hh++) {
                int16_t tx = (int16_t)(chartLeft + (chartRight - chartLeft) * (hh - Config::DAILY_CURVE_START_HOUR) / (Config::DAILY_CURVE_END_HOUR - Config::DAILY_CURVE_START_HOUR));
                t.drawFastVLine(tx, 0, 2, dim);
            }
        });
        LayoutCheck::bufferRect("Fortschrittsbalken-Puffer", Config::SCREEN_WIDTH, PROGRESS_H, Config::SCREEN_WIDTH, PROGRESS_H);
    }
    LayoutCheck::rect("Fortschrittsbalken", 0, progressY, Config::SCREEN_WIDTH, PROGRESS_H);
    // Kein Platz mehr fuer eine zusaetzliche Uhrzeit-Beschriftung am
    // Fuellende (nur ca. 2px Luft bis zur Fusszone bei dieser
    // Bildschirmgroesse) - bewusst weggelassen (Auftrag erlaubt das
    // ausdruecklich), siehe Bericht.
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
