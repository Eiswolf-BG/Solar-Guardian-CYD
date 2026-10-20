#include "years_page.h"
#include "display.h"
#include "yield_ledger.h"
#include "layout_utils.h"
#include "layout_check.h"
#include "i18n.h"
#include "config.h"
#include <time.h>

namespace YearsPage {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    enum class Tab : uint8_t { Days = 0, Months = 1, Years = 2 };

    // Zuletzt gewaehlter Reiter - bleibt bis zum naechsten Neustart
    // erhalten (Auftrag "beim Wiederoeffnen der Seite beibehalten"),
    // startet mit "Jahre" (bisheriges alleiniges Verhalten dieser Seite).
    Tab activeTab = Tab::Years;

    constexpr int16_t SIDE_MARGIN = 6;
    constexpr int16_t TAB_GAP = 4;
    constexpr int16_t TAB_BAR_TOP_OFFSET = 2;
    constexpr int16_t TAB_BAR_H = 32;
    constexpr int16_t UNIT_LABEL_Y_OFFSET = 46; // unter dem Reiterbalken
    constexpr int16_t CHART_TOP_OFFSET = 62;    // nach Reiterbalken + "kWh"-Zeile
    constexpr int16_t VALUE_LABEL_H = 28;
    constexpr int16_t YEAR_LABEL_H = 28;
    constexpr int16_t BAR_GAP = 6;
    constexpr uint8_t MAX_BARS = Config::HISTORY_CHART_MAX_BARS;
    constexpr uint8_t VALUE_SIZES[] = {2, 1};
    constexpr uint8_t LABEL_SIZES[] = {2, 1};

    constexpr I18n::Str WEEKDAY_STRS[7] = {
        I18n::Str::WEEKDAY_SUN, I18n::Str::WEEKDAY_MON, I18n::Str::WEEKDAY_TUE, I18n::Str::WEEKDAY_WED,
        I18n::Str::WEEKDAY_THU, I18n::Str::WEEKDAY_FRI, I18n::Str::WEEKDAY_SAT,
    };
    constexpr I18n::Str MONTH_STRS[12] = {
        I18n::Str::MONTH_JAN, I18n::Str::MONTH_FEB, I18n::Str::MONTH_MAR, I18n::Str::MONTH_APR,
        I18n::Str::MONTH_MAY, I18n::Str::MONTH_JUN, I18n::Str::MONTH_JUL, I18n::Str::MONTH_AUG,
        I18n::Str::MONTH_SEP, I18n::Str::MONTH_OCT, I18n::Str::MONTH_NOV, I18n::Str::MONTH_DEC,
    };

    uint32_t lastSeenRevision = 0xFFFFFFFFu;
    int8_t lastSeenMinute = -1;
    bool firstCheck = true;

    struct Bar {
        String label; // Wochentag/Monatskuerzel/Jahreszahl, je nach Reiter
        float kwh;
        bool isCurrent;
    };

    // Reiterbalken-Rechtecke - 3 gleich breite Felder. areaTop muss mit
    // dem areaTop aus draw()/onTap() uebereinstimmen (beide kommen von
    // main_screen.cpp's BODY_TOP).
    Rect tabRect(int16_t areaTop, uint8_t index) {
        int16_t totalW = (int16_t)(Config::SCREEN_WIDTH - 2 * SIDE_MARGIN - 2 * TAB_GAP);
        int16_t w = (int16_t)(totalW / 3);
        int16_t x = (int16_t)(SIDE_MARGIN + index * (w + TAB_GAP));
        int16_t y = (int16_t)(areaTop + TAB_BAR_TOP_OFFSET);
        return {x, y, w, TAB_BAR_H};
    }

    // Tage: abgeschlossene Vortage + der heutige, noch laufende Tag
    // (letzter/rechtester Balken) - gleiches Fenster wie current().weekKwh,
    // hier als Einzelwerte (siehe yield_ledger.h).
    uint8_t collectDayBars(Bar* out) {
        YieldLedger::DayValue days[Config::HISTORY_CHART_MAX_BARS];
        uint8_t n = YieldLedger::lastDaysIncludingToday(days, Config::HISTORY_CHART_MAX_BARS);
        for (uint8_t i = 0; i < n; i++) {
            out[i].kwh = days[i].kwh;
            out[i].isCurrent = (i == (uint8_t)(n - 1));
            int y, m, d;
            if (sscanf(days[i].date, "%d-%d-%d", &y, &m, &d) == 3) {
                struct tm dt = {};
                dt.tm_year = y - 1900; dt.tm_mon = m - 1; dt.tm_mday = d; dt.tm_hour = 12;
                mktime(&dt); // normalisiert tm_wday
                uint8_t wday = (dt.tm_wday >= 0 && dt.tm_wday <= 6) ? (uint8_t)dt.tm_wday : 0;
                out[i].label = I18n::t(WEEKDAY_STRS[wday]);
            } else {
                out[i].label = "?";
            }
        }
        return n;
    }

    // Monate: gespeicherte Vormonate (months.json) + der laufende Monat
    // live aus current() - gleiches Prinzip wie die bisherige Jahre-Logik.
    uint8_t collectMonthBars(Bar* out) {
        struct tm nowTm;
        if (!getLocalTime(&nowTm, 5)) return 0;

        YieldLedger::MonthEntry past[Config::HISTORY_CHART_MAX_BARS];
        uint8_t pastCount = YieldLedger::pastMonths(past, (uint8_t)(MAX_BARS - 1));

        uint8_t n = 0;
        for (uint8_t i = 0; i < pastCount; i++) {
            int y, m;
            out[n].kwh = past[i].kwh;
            out[n].isCurrent = false;
            if (sscanf(past[i].key, "%d-%d", &y, &m) == 2 && m >= 1 && m <= 12) {
                out[n].label = I18n::t(MONTH_STRS[m - 1]);
            } else {
                out[n].label = "?";
            }
            n++;
        }
        out[n].kwh = YieldLedger::current().monthKwh;
        out[n].isCurrent = true;
        out[n].label = I18n::t(MONTH_STRS[nowTm.tm_mon]);
        n++;
        return n;
    }

    // Jahre: unveraendertes bisheriges Verhalten (years.json + laufendes
    // Jahr live), jetzt auf MAX_BARS=7 statt vormals 6 Balken erweitert.
    uint8_t collectYearBars(Bar* out) {
        struct tm nowTm;
        if (!getLocalTime(&nowTm, 5)) return 0;
        int currentYear = nowTm.tm_year + 1900;

        YieldLedger::YearEntry past[Config::HISTORY_CHART_MAX_BARS];
        uint8_t pastCount = YieldLedger::pastYears(past, (uint8_t)(MAX_BARS - 1));

        uint8_t n = 0;
        for (uint8_t i = 0; i < pastCount; i++) {
            out[n].kwh = past[i].kwh;
            out[n].isCurrent = false;
            out[n].label = String(past[i].year);
            n++;
        }
        out[n].kwh = YieldLedger::current().yearKwh;
        out[n].isCurrent = true;
        out[n].label = String(currentYear);
        n++;
        return n;
    }

    uint8_t collectBars(Bar* out) {
        switch (activeTab) {
            case Tab::Days: return collectDayBars(out);
            case Tab::Months: return collectMonthBars(out);
            case Tab::Years: default: return collectYearBars(out);
        }
    }

    // Etwa halbe Helligkeit der Systemfarbe fuer fruehere Perioden (Alex'
    // Wunsch) - RGB565-Kanaele je einzeln halbieren, statt ein fest
    // verdrahtetes Grau zu nehmen.
    uint16_t dimColor(uint16_t c) {
        uint8_t r = (uint8_t)((c >> 11) & 0x1F) >> 1;
        uint8_t g = (uint8_t)((c >> 5) & 0x3F) >> 1;
        uint8_t b = (uint8_t)(c & 0x1F) >> 1;
        return (uint16_t)((r << 11) | (g << 5) | b);
    }

    // Wie LayoutUtils::fitText(), aber OHNE "..."-Kuerzung - Zahlen duerfen
    // nie gekuerzt werden (siehe CLAUDE.md Layout-Regel). Passt selbst die
    // kleinste uebergebene Groesse nicht mehr vollstaendig in die Zone,
    // wird trotzdem die komplette Zahl bei der kleinsten Groesse
    // gezeichnet, statt Ziffern abzuschneiden.
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

    void drawTabBar(int16_t areaTop, uint16_t accent) {
        constexpr I18n::Str TAB_LABELS[3] = {I18n::Str::YEARS_TAB_DAYS, I18n::Str::YEARS_TAB_MONTHS, I18n::Str::YEARS_TAB_YEARS};
        for (uint8_t i = 0; i < 3; i++) {
            Rect r = tabRect(areaTop, i);
            bool active = ((uint8_t)activeTab == i);
            if (active) {
                tft.fillRect(r.x, r.y, r.w, r.h, accent);
                tft.setTextColor(TFT_BLACK, accent);
            } else {
                tft.fillRect(r.x, r.y, r.w, r.h, TFT_BLACK);
                tft.drawRect(r.x, r.y, r.w, r.h, accent);
                tft.setTextColor(accent, TFT_BLACK);
            }
            LayoutUtils::FitResult fit = LayoutUtils::fitText(tft, I18n::t(TAB_LABELS[i]), (int16_t)(r.w - 4), (int16_t)(r.h - 4), LABEL_SIZES, 2);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(fit.text, (int16_t)(r.x + r.w / 2), (int16_t)(r.y + r.h / 2));
            tft.setTextDatum(TL_DATUM);
            LayoutCheck::rect("Reiter", r.x, r.y, r.w, r.h);
        }
    }
}

void draw(int16_t areaTop, int16_t /*areaHeight*/, bool /*forceRedraw*/) {
    uint16_t accent = Display::systemColor();
    uint16_t dimAccent = dimColor(accent);

    tft.setTextSize(1);
    drawTabBar(areaTop, accent);

    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(accent, TFT_BLACK);
    tft.drawString(I18n::t(I18n::Str::KWH_UNIT), Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + UNIT_LABEL_Y_OFFSET));
    tft.setTextDatum(TL_DATUM);
    LayoutCheck::rect("Einheit", 0, (int16_t)(areaTop + UNIT_LABEL_Y_OFFSET), Config::SCREEN_WIDTH, 16);

    Bar bars[Config::HISTORY_CHART_MAX_BARS];
    uint8_t count = collectBars(bars);

    int16_t chartTop = (int16_t)(areaTop + CHART_TOP_OFFSET);
    // Feste Fusszone (Seitenpunkte + "?"-Knopf, siehe main_screen.cpp) PLUS
    // mind. 8px Abstand - das Diagramm darf diese Linie nie unterschreiten.
    // WICHTIG: ausgehend von SCREEN_HEIGHT, nicht von areaTop+areaHeight
    // (= BODY_TOP+BODY_H) - siehe Config::PAGE_FOOTER_RESERVED_H.
    int16_t contentBottom = (int16_t)(Config::SCREEN_HEIGHT - Config::PAGE_FOOTER_RESERVED_H - 8);
    int16_t baselineY = (int16_t)(contentBottom - YEAR_LABEL_H);
    int16_t barMaxH = (int16_t)(baselineY - chartTop - VALUE_LABEL_H);
    if (barMaxH < 10) barMaxH = 10;

    if (count == 0) {
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("--", Config::SCREEN_WIDTH / 2, (int16_t)(chartTop + barMaxH / 2));
        tft.setTextDatum(TL_DATUM);
        return;
    }

    float maxVal = 0.0f;
    for (uint8_t i = 0; i < count; i++) {
        if (bars[i].kwh > maxVal) maxVal = bars[i].kwh;
    }
    if (maxVal < 1.0f) maxVal = 1.0f;

    int16_t barW = (int16_t)((Config::SCREEN_WIDTH - 2 * SIDE_MARGIN - (count - 1) * BAR_GAP) / count);
    if (barW < 1) barW = 1;

    int16_t x = SIDE_MARGIN;
    for (uint8_t i = 0; i < count; i++) {
        uint16_t barColor = bars[i].isCurrent ? accent : dimAccent;

        int16_t barH = (int16_t)((bars[i].kwh / maxVal) * barMaxH);
        if (bars[i].kwh > 0 && barH < 2) barH = 2;
        int16_t barY = (int16_t)(baselineY - barH);
        if (barH > 0) tft.fillRect(x, barY, barW, barH, barColor);

        char valBuf[12];
        snprintf(valBuf, sizeof(valBuf), "%.0f", (double)bars[i].kwh);
        tft.setTextColor(barColor, TFT_BLACK);
        LayoutUtils::FitResult valFit = fitNumberNoTruncate(tft, valBuf, barW, VALUE_LABEL_H, VALUE_SIZES, 2);
        tft.setTextDatum(BC_DATUM);
        tft.drawString(valFit.text, (int16_t)(x + barW / 2), (int16_t)(barY - 2));

        tft.setTextColor(barColor, TFT_BLACK);
        LayoutUtils::FitResult labelFit = fitNumberNoTruncate(tft, bars[i].label, barW, YEAR_LABEL_H, LABEL_SIZES, 2);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(labelFit.text, (int16_t)(x + barW / 2), (int16_t)(baselineY + 4));
        tft.setTextDatum(TL_DATUM);

        LayoutCheck::rect("Balken", x, chartTop, barW, (int16_t)(contentBottom - chartTop));
        x += (int16_t)(barW + BAR_GAP);
    }
}

bool update() {
    uint32_t revision = YieldLedger::revision();
    struct tm t;
    int8_t minute = getLocalTime(&t, 5) ? (int8_t)t.tm_min : (int8_t)-1;

    bool changed = firstCheck || revision != lastSeenRevision || minute != lastSeenMinute;
    if (changed) {
        firstCheck = false;
        lastSeenRevision = revision;
        lastSeenMinute = minute;
    }
    return changed;
}

bool onTap(int16_t x, int16_t y, int16_t areaTop, int16_t /*areaHeight*/) {
    for (uint8_t i = 0; i < 3; i++) {
        Rect r = tabRect(areaTop, i);
        if (r.contains(x, y)) {
            Tab tapped = (Tab)i;
            if (tapped == activeTab) return false;
            activeTab = tapped;
            return true;
        }
    }
    return false;
}

}
