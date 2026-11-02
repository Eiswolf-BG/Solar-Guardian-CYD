#include "week_page.h"
#include "display.h"
#include "weather_data.h"
#include "weather_icons.h"
#include "app_config.h"
#include "layout_check.h"
#include "redraw_diag.h"
#include "i18n.h"
#include "config.h"
#include <time.h>

namespace WeekPage {

namespace {
    constexpr uint8_t ROW_COUNT = WeatherData::FORECAST_DAYS;
    constexpr int16_t ICON_COL_W = 26;
    constexpr int16_t DAY_GAP = 4;
    // Daten gelten ab diesem Alter als veraltet - deutlich grosszuegiger
    // als der normale 30-Minuten-Takt (Config::WEATHER_FETCH_INTERVAL_MS),
    // damit ein einzelner verzoegerter Abruf nicht sofort den Hinweis
    // zeigt.
    constexpr uint32_t WEEK_STALE_AFTER_MS = 3UL * 60UL * 60UL * 1000UL;

    constexpr I18n::Str WEEKDAY_STRS[7] = {
        I18n::Str::WEEKDAY_SUN, I18n::Str::WEEKDAY_MON, I18n::Str::WEEKDAY_TUE, I18n::Str::WEEKDAY_WED,
        I18n::Str::WEEKDAY_THU, I18n::Str::WEEKDAY_FRI, I18n::Str::WEEKDAY_SAT,
    };

    const char* dayLabel(uint8_t index, time_t dateEpoch) {
        if (index == 0) return I18n::t(I18n::Str::YIELD_TILE_TODAY);
        if (index == 1) return I18n::t(I18n::Str::WEEK_DAY_TOMORROW);
        struct tm t;
        localtime_r(&dateEpoch, &t);
        uint8_t wday = (t.tm_wday >= 0 && t.tm_wday <= 6) ? (uint8_t)t.tm_wday : 0;
        return I18n::t(WEEKDAY_STRS[wday]);
    }

    uint32_t lastChangeCounter = 0xFFFFFFFFu;
    bool lastHadData = false;
}

void draw(int16_t areaTop, int16_t /*areaHeight*/, bool forceRedraw) {
    WeatherData::Data w = WeatherData::snapshot();
    uint16_t accent = Display::systemColor();

    bool stale = w.currentValid && w.measuredAt > 0 &&
                 (time(nullptr) - w.measuredAt) > (time_t)(WEEK_STALE_AFTER_MS / 1000);
    bool haveData = w.locationSet && w.currentValid && w.dayCount > 0 && !stale;

    bool changed = forceRedraw || w.changeCounter != lastChangeCounter || haveData != lastHadData;
    if (!changed) return;
    lastChangeCounter = w.changeCounter;
    lastHadData = haveData;
    RedrawDiag::log("Wetter-Woche", "Inhalt geaendert");

    // Eigenen Bereich (unter der Kopfzeile, ueber der Fusszone) komplett
    // schwarz loeschen - Seite wird nur bei echter Aenderung neu
    // gezeichnet (kein Dauer-Flackern), siehe update().
    int16_t contentBottom = (int16_t)(Config::SCREEN_HEIGHT - Config::PAGE_FOOTER_RESERVED_H - 8);
    tft.fillRect(0, areaTop, Config::SCREEN_WIDTH, (int16_t)(contentBottom - areaTop), TFT_BLACK);

    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(accent, TFT_BLACK);
    tft.drawString(I18n::t(I18n::Str::PAGE_WEEKWEATHER_TITLE), Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + 6));
    tft.setTextDatum(TL_DATUM);
    LayoutCheck::rect("Titel", 0, areaTop, Config::SCREEN_WIDTH, 16);

    if (!haveData) {
        // Klarer Hinweis statt leerer Flaeche (Auftrag) - gilt sowohl bei
        // fehlendem Standort/noch keinen Daten als auch bei veralteten
        // Daten.
        tft.setTextColor(Display::secondaryColor(), TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        int16_t midY = (int16_t)((areaTop + 24 + contentBottom) / 2);
        tft.drawString(I18n::t(I18n::Str::WEEKWEATHER_NO_DATA), Config::SCREEN_WIDTH / 2, midY);
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Hinweis", 0, (int16_t)(areaTop + 24), Config::SCREEN_WIDTH, (int16_t)(contentBottom - areaTop - 24));
        return;
    }

    int16_t rowsTop = (int16_t)(areaTop + 26);
    int16_t availH = (int16_t)(contentBottom - rowsTop);
    uint8_t n = w.dayCount < ROW_COUNT ? w.dayCount : ROW_COUNT;
    int16_t rowH = (int16_t)(availH / (n > 0 ? n : 1));
    if (rowH > 36) rowH = 36; // nicht unnoetig weit auseinanderziehen

    for (uint8_t i = 0; i < n; i++) {
        int16_t rowY = (int16_t)(rowsTop + i * rowH);
        int16_t iconCx = (int16_t)(8 + ICON_COL_W / 2);
        int16_t iconCy = (int16_t)(rowY + rowH / 2);
        const WeatherData::DayForecast& d = w.days[i];

        WeatherIcons::draw(tft, iconCx, iconCy, (int16_t)(ICON_COL_W - 4), d.weatherCode, true);
        LayoutCheck::rect("Symbol", (int16_t)(iconCx - (ICON_COL_W - 4) / 2), (int16_t)(iconCy - (ICON_COL_W - 4) / 2),
                           (int16_t)(ICON_COL_W - 4), (int16_t)(ICON_COL_W - 4));

        int16_t dayX = (int16_t)(8 + ICON_COL_W);
        tft.setTextColor(accent, TFT_BLACK);
        const char* label = dayLabel(i, d.dateEpoch);
        int16_t labelW = tft.textWidth(label);
        tft.setTextDatum(ML_DATUM);
        tft.drawString(label, dayX, iconCy);
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Wochentag", dayX, (int16_t)(iconCy - 8), labelW, 16);

        int16_t tempX = (int16_t)(dayX + labelW + DAY_GAP);
        char maxBuf[8], minBuf[8];
        snprintf(maxBuf, sizeof(maxBuf), "%.0f", (double)d.tempMaxC);
        snprintf(minBuf, sizeof(minBuf), "%.0f", (double)d.tempMinC);
        // Abgefragter Temperaturwert in Sekundaerfarbe (Auftrag
        // "Farbregeln vereinheitlichen") - "°" in Systemfarbe als
        // Trennzeichen zwischen Max und Min.
        String tempLine = String(maxBuf) + "° / " + String(minBuf) + "°";
        tft.setTextColor(Display::secondaryColor(), TFT_BLACK);
        tft.setTextDatum(ML_DATUM);
        tft.drawString(tempLine, tempX, iconCy);
        tft.setTextDatum(TL_DATUM);
        int16_t tempAvailW = (int16_t)(Config::SCREEN_WIDTH - 8 - tempX);
        LayoutCheck::rect("Temperatur", tempX, (int16_t)(iconCy - 8), tempAvailW, 16);
    }
}

bool update() {
    WeatherData::Data w = WeatherData::snapshot();
    bool stale = w.currentValid && w.measuredAt > 0 &&
                 (time(nullptr) - w.measuredAt) > (time_t)(WEEK_STALE_AFTER_MS / 1000);
    bool haveData = w.locationSet && w.currentValid && w.dayCount > 0 && !stale;
    return w.changeCounter != lastChangeCounter || haveData != lastHadData;
}

}
