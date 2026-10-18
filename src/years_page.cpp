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
    constexpr int16_t UNIT_LABEL_Y_OFFSET = 24;
    constexpr int16_t CHART_TOP_OFFSET = 46; // nach Titel (y+6..+23) und "kWh"-Zeile (y+24..+41)
    // Platz fuer Wert (ueber dem Balken) bzw. Jahreszahl (unter der
    // Grundlinie) - mind. Schrifthoehe (17px bei Size 1, siehe
    // AppFont11pt yAdvance) + 2*4px Innenabstand = 25, hier mit etwas
    // Reserve. Zahlen/Jahreszahlen werden NIE gekuerzt (siehe CLAUDE.md
    // Layout-Regel) - zu knapp bemessene Zonen fuehrten frueher dazu, dass
    // selbst die kleinste Schriftgroesse die Hoehe nicht erfuellte und
    // fitText() deshalb staendig mit "..." kuerzte.
    constexpr int16_t VALUE_LABEL_H = 28;
    constexpr int16_t YEAR_LABEL_H = 28;
    constexpr int16_t SIDE_MARGIN = 6;
    constexpr int16_t BAR_GAP = 6;
    constexpr uint8_t MAX_BARS = Config::YEARS_CHART_MAX_BARS;
    constexpr uint8_t VALUE_SIZES[] = {2, 1};
    constexpr uint8_t YEAR_SIZES[] = {2, 1};

    uint32_t lastSeenRevision = 0xFFFFFFFFu;
    int8_t lastSeenMinute = -1;
    bool firstCheck = true;

    struct Bar {
        int year;
        float kwh;
        bool isCurrent;
    };

    // Liefert bis zu MAX_BARS Balken, aufsteigend nach Jahr sortiert,
    // das laufende Jahr immer als letzter (rechtester) Balken.
    uint8_t collectBars(Bar* out) {
        struct tm nowTm;
        if (!getLocalTime(&nowTm, 5)) return 0;
        int currentYear = nowTm.tm_year + 1900;

        YieldLedger::YearEntry past[Config::YEARS_CHART_MAX_BARS];
        uint8_t pastCount = YieldLedger::pastYears(past, (uint8_t)(MAX_BARS - 1));

        uint8_t n = 0;
        for (uint8_t i = 0; i < pastCount; i++) {
            out[n].year = past[i].year;
            out[n].kwh = past[i].kwh;
            out[n].isCurrent = false;
            n++;
        }
        out[n].year = currentYear;
        out[n].kwh = YieldLedger::current().yearKwh;
        out[n].isCurrent = true;
        n++;
        return n;
    }

    // Etwa halbe Helligkeit der Systemfarbe fuer fruehere Jahre (Alex'
    // Wunsch) - RGB565-Kanaele je einzeln halbieren, statt ein fest
    // verdrahtetes Grau zu nehmen.
    uint16_t dimColor(uint16_t c) {
        uint8_t r = (uint8_t)((c >> 11) & 0x1F) >> 1;
        uint8_t g = (uint8_t)((c >> 5) & 0x3F) >> 1;
        uint8_t b = (uint8_t)(c & 0x1F) >> 1;
        return (uint16_t)((r << 11) | (g << 5) | b);
    }

    // Wie LayoutUtils::fitText(), aber OHNE "..."-Kuerzung - Zahlen und
    // Jahreszahlen duerfen nie gekuerzt werden (siehe CLAUDE.md
    // Layout-Regel). Passt selbst die kleinste uebergebene Groesse nicht
    // mehr vollstaendig in die Zone, wird trotzdem die komplette Zahl bei
    // der kleinsten Groesse gezeichnet, statt Ziffern abzuschneiden -
    // bei den hier realistischen Wertebereichen (ganze kWh/Jahr,
    // 4-stellig) bleibt das ein rein theoretischer Fall.
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

void draw(int16_t areaTop, int16_t /*areaHeight*/) {
    uint16_t accent = Display::systemColor();
    uint16_t dimAccent = dimColor(accent);

    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(accent, TFT_BLACK);
    tft.drawString(I18n::t(I18n::Str::PAGE_YEARS_TITLE), Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + 6));
    LayoutCheck::rect("Titel", 0, areaTop, Config::SCREEN_WIDTH, 16);

    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(I18n::t(I18n::Str::KWH_UNIT), Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + UNIT_LABEL_Y_OFFSET));
    tft.setTextDatum(TL_DATUM);
    LayoutCheck::rect("Einheit", 0, (int16_t)(areaTop + UNIT_LABEL_Y_OFFSET), Config::SCREEN_WIDTH, 16);

    Bar bars[Config::YEARS_CHART_MAX_BARS];
    uint8_t count = collectBars(bars);

    int16_t chartTop = (int16_t)(areaTop + CHART_TOP_OFFSET);
    // Feste Fusszone (Seitenpunkte + "?"-Knopf, siehe main_screen.cpp) PLUS
    // mind. 8px Abstand (Alex' Wunsch) - das Diagramm darf diese Linie nie
    // unterschreiten. WICHTIG: ausgehend von SCREEN_HEIGHT, nicht von
    // areaTop+areaHeight (= BODY_TOP+BODY_H) - letzteres liegt bereits
    // DOTS_H (16px) oberhalb von SCREEN_HEIGHT, eine fruehere Version
    // dieser Formel subtrahierte PAGE_FOOTER_RESERVED_H dadurch effektiv
    // doppelt und war unnoetig streng (betraf nur Sicherheitsabstand,
    // keine echte Ueberlappung).
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

        char yearBuf[8];
        snprintf(yearBuf, sizeof(yearBuf), "%d", bars[i].year);
        tft.setTextColor(barColor, TFT_BLACK);
        LayoutUtils::FitResult yearFit = fitNumberNoTruncate(tft, yearBuf, barW, YEAR_LABEL_H, YEAR_SIZES, 2);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(yearFit.text, (int16_t)(x + barW / 2), (int16_t)(baselineY + 4));
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

}
