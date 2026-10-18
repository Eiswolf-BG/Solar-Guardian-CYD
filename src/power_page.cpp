#include "power_page.h"
#include "display.h"
#include "solar_data.h"
#include "app_config.h"
#include "fetch_task.h"
#include "layout_utils.h"
#include "layout_check.h"
#include "i18n.h"
#include "config.h"
#include <time.h>
#include <math.h>

namespace PowerPage {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    // Alle Offsets rechnerisch verkettet (nicht einzeln fest verdrahtet),
    // damit sich der verfuegbare Platz bis zur Fusszone (siehe
    // Config::PAGE_FOOTER_RESERVED_H) nicht mehr versehentlich verschiebt
    // - eine fruehere Version dieser Seite reichte mit der Statuszeilen-
    // Tippflaeche bis zur Seitenpunkte-/"?"-Zeile hinein, was der
    // zentrale Layout-Pruefer in main_screen.cpp aufgedeckt hat.
    constexpr int16_t POWER_VALUE_OFFSET = 20;
    constexpr int16_t POWER_VALUE_H = 38;
    constexpr int16_t TIME_OFFSET = (int16_t)(POWER_VALUE_OFFSET + POWER_VALUE_H + 1);
    constexpr int16_t TIME_H = 14;
    constexpr int16_t TILES_TOP_OFFSET = (int16_t)(TIME_OFFSET + TIME_H + 2);
    constexpr int16_t TILE_GAP = 6;
    constexpr int16_t TILE_H = 46;
    constexpr int16_t CHART_TOP_OFFSET = (int16_t)(TILES_TOP_OFFSET + TILE_H + 2);
    constexpr int16_t CHART_H = 45;
    constexpr int16_t CHART_SIDE_MARGIN = 10;
    constexpr int16_t TIMEMARKS_OFFSET = (int16_t)(CHART_TOP_OFFSET + CHART_H + 2);
    constexpr int16_t TIMEMARKS_H = 16;
    // +6 Abstand zur Tipp-Flaeche der Statuszeile (reicht 14px nach OBEN
    // ueber den Text hinaus, siehe statusTapZone()) + diese 14px selbst.
    constexpr int16_t STATUS_OFFSET = (int16_t)(TIMEMARKS_OFFSET + TIMEMARKS_H + 6 + 14);
    constexpr uint8_t POWER_SIZES[] = {3, 2, 1};
    constexpr uint8_t VALUE_SIZES[] = {2, 1};

    uint32_t lastSeenChangeCounter = 0xFFFFFFFFu;
    int8_t lastSeenMinute = -1;
    bool firstCheck = true;

    int16_t tileWidth() { return (int16_t)((Config::SCREEN_WIDTH - 3 * TILE_GAP) / 2); }

    // Etwa halbe Helligkeit der Systemfarbe fuer die Diagramm-Hilfslinien
    // (Alex' Wunsch "gedimmte Systemfarbe") - gleiche Kanal-Halbierung wie
    // in years_page.cpp, hier bewusst lokal dupliziert (siehe CLAUDE.md
    // Code-Stil: jeder Screen bleibt unabhaengig).
    uint16_t dimColor(uint16_t c) {
        uint8_t r = (uint8_t)((c >> 11) & 0x1F) >> 1;
        uint8_t g = (uint8_t)((c >> 5) & 0x3F) >> 1;
        uint8_t b = (uint8_t)(c & 0x1F) >> 1;
        return (uint16_t)((r << 11) | (g << 5) | b);
    }

    // Zahl auf eine "runde" Obergrenze fuer die Y-Achse aufrunden
    // (1-2-5-Schema) - 0/negative Werte (z.B. nachts) bekommen eine feste
    // Mindestskala, damit die Achse nicht kollabiert.
    float niceCeil(float v) {
        if (v <= 1.0f) return 100.0f;
        float mag = powf(10.0f, floorf(log10f(v)));
        float norm = v / mag;
        float niceNorm = (norm <= 1.0f) ? 1.0f : (norm <= 2.0f) ? 2.0f : (norm <= 5.0f) ? 5.0f : 10.0f;
        return niceNorm * mag;
    }

    void formatPower(float watts, char* out, size_t outSize) {
        char buf[24];
        if (fabsf(watts) >= 1000.0f) {
            snprintf(buf, sizeof(buf), "%.2f %s", watts / 1000.0f, I18n::t(I18n::Str::POWER_UNIT_KW));
        } else {
            snprintf(buf, sizeof(buf), "%.0f %s", watts, I18n::t(I18n::Str::POWER_UNIT_W));
        }
        if (appConfig.language == 0) {
            for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
        }
        strncpy(out, buf, outSize - 1);
        out[outSize - 1] = 0;
    }

    void formatNumber(float value, uint8_t decimals, char* out, size_t outSize) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.*f", decimals, (double)value);
        if (appConfig.language == 0) {
            for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
        }
        strncpy(out, buf, outSize - 1);
        out[outSize - 1] = 0;
    }

    void drawValueTile(int16_t x, int16_t y, int16_t w, int16_t h, I18n::Str labelKey,
                        const String& valueStr, const char* unit, bool valid, bool stale, uint16_t accent) {
        tft.drawRoundRect(x, y, w, h, 4, accent);

        tft.setTextSize(1);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(I18n::t(labelKey), (int16_t)(x + w / 2), (int16_t)(y + 5));

        constexpr int16_t LABEL_H = 10;
        constexpr int16_t UNIT_H = 10;
        int16_t valueZoneTop = (int16_t)(y + LABEL_H);
        int16_t valueZoneH = (int16_t)(h - LABEL_H - UNIT_H);

        uint16_t valueColor = (valid && !stale) ? accent : TFT_DARKGREY;
        tft.setTextColor(valueColor, TFT_BLACK);
        LayoutUtils::FitResult fit = LayoutUtils::fitText(tft, valueStr, w, valueZoneH, VALUE_SIZES, 2);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(fit.text, (int16_t)(x + w / 2), (int16_t)(valueZoneTop + valueZoneH / 2));

        tft.setTextSize(1);
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(BC_DATUM);
        tft.drawString(unit, (int16_t)(x + w / 2), (int16_t)(y + h - 4));
        tft.setTextDatum(TL_DATUM);

        if (LayoutCheck::ENABLED) LayoutCheck::rect("Kachel-Rahmen", x, y, w, h);
    }

    String statusTextFor(const SolarData::Data& snap, bool stale) {
        if (!snap.powerValid) return String("--");
        String text;
        switch (snap.fetchStatus) {
            case SolarData::FetchStatus::NoAccount:
                text = I18n::t(I18n::Str::NO_ACCOUNT_CONFIGURED);
                break;
            case SolarData::FetchStatus::LoggingIn:
                text = I18n::t(I18n::Str::STATUS_LOGGING_IN);
                break;
            case SolarData::FetchStatus::LoginFailed:
                text = I18n::t(I18n::Str::STATUS_LOGIN_FAILED);
                break;
            case SolarData::FetchStatus::NetworkError:
                text = I18n::t(I18n::Str::STATUS_NETWORK_ERROR);
                break;
            case SolarData::FetchStatus::Ok:
                if (snap.powerMeasuredAt > 0) {
                    struct tm t;
                    localtime_r(&snap.powerMeasuredAt, &t);
                    char buf[32];
                    snprintf(buf, sizeof(buf), I18n::t(I18n::Str::STATUS_MEASURED_FMT), t.tm_hour, t.tm_min);
                    text = buf;
                }
                break;
        }
        if (stale) {
            text += " ";
            text += I18n::t(I18n::Str::STATUS_STALE_SUFFIX);
        }
        return text;
    }

    Rect statusTapZone(int16_t areaTop) {
        return {0, (int16_t)(areaTop + STATUS_OFFSET - 14), Config::SCREEN_WIDTH, 40};
    }
}

void draw(int16_t areaTop, int16_t areaHeight) {
    SolarData::Data snap = SolarData::snapshot();
    uint16_t accent = Display::systemColor();

    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(accent, TFT_BLACK);
    tft.drawString(I18n::t(I18n::Str::PAGE_POWER_TITLE), Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + 6));
    tft.setTextDatum(TL_DATUM);
    LayoutCheck::rect("Titel", 0, areaTop, Config::SCREEN_WIDTH, 16);

    time_t now = time(nullptr);
    bool stale = snap.powerMeasuredAt > 0 &&
                 (now - snap.powerMeasuredAt) > (time_t)(Config::EPEVER_STALE_AFTER_MS / 1000);
    uint16_t dimAccent = dimColor(accent);
    uint16_t numColor = (snap.powerValid && !stale) ? accent : TFT_DARKGREY;

    // Grosse, zentrierte aktuelle Leistung.
    char powerStr[24];
    if (snap.powerValid) formatPower(snap.currentPowerW, powerStr, sizeof(powerStr));
    else strncpy(powerStr, "--", sizeof(powerStr));
    tft.setTextColor(numColor, TFT_BLACK);
    int16_t powerZoneTop = (int16_t)(areaTop + POWER_VALUE_OFFSET);
    LayoutUtils::FitResult powerFit = LayoutUtils::fitText(tft, powerStr, (int16_t)(Config::SCREEN_WIDTH - 20),
                                                             POWER_VALUE_H, POWER_SIZES, 3);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(powerFit.text, Config::SCREEN_WIDTH / 2, powerZoneTop);
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    LayoutCheck::rect("Leistung", 10, powerZoneTop, (int16_t)(Config::SCREEN_WIDTH - 20), POWER_VALUE_H);

    // Kleine Messzeit direkt darunter.
    char timeStr[8] = "--:--";
    if (snap.powerValid && snap.powerMeasuredAt > 0) {
        struct tm t;
        localtime_r(&snap.powerMeasuredAt, &t);
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", t.tm_hour, t.tm_min);
    }
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(timeStr, Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + TIME_OFFSET));
    tft.setTextDatum(TL_DATUM);
    LayoutCheck::rect("Messzeit", 0, (int16_t)(areaTop + TIME_OFFSET), Config::SCREEN_WIDTH, 16);

    // PV-Spannung/-Strom als kleine Kacheln.
    int16_t tileW = tileWidth();
    int16_t x0 = TILE_GAP;
    int16_t x1 = (int16_t)(x0 + tileW + TILE_GAP);
    int16_t tilesY = (int16_t)(areaTop + TILES_TOP_OFFSET);

    char voltStr[16], currStr[16];
    if (snap.pvValid) {
        formatNumber(snap.pvVoltage, 1, voltStr, sizeof(voltStr));
        formatNumber(snap.pvCurrent, 2, currStr, sizeof(currStr));
    } else {
        strncpy(voltStr, "--", sizeof(voltStr));
        strncpy(currStr, "--", sizeof(currStr));
    }
    drawValueTile(x0, tilesY, tileW, TILE_H, I18n::Str::BATTERY_TILE_VOLTAGE, voltStr, I18n::t(I18n::Str::VOLT_UNIT), snap.pvValid, stale, accent);
    drawValueTile(x1, tilesY, tileW, TILE_H, I18n::Str::BATTERY_TILE_CURRENT, currStr, I18n::t(I18n::Str::AMP_UNIT), snap.pvValid, stale, accent);

    // Kurve der letzten 3 Stunden, ohne Bibliothek mit drawLine gezeichnet.
    int16_t chartTop = (int16_t)(areaTop + CHART_TOP_OFFSET);
    int16_t chartLeft = CHART_SIDE_MARGIN;
    int16_t chartRight = (int16_t)(Config::SCREEN_WIDTH - CHART_SIDE_MARGIN);
    int16_t chartBottom = (int16_t)(chartTop + CHART_H);
    LayoutCheck::rect("Kurve", chartLeft, chartTop, (int16_t)(chartRight - chartLeft), CHART_H);

    if (snap.historyCount < 2) {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("--", Config::SCREEN_WIDTH / 2, (int16_t)(chartTop + CHART_H / 2));
        tft.setTextDatum(TL_DATUM);
    } else {
        float maxVal = 0.0f;
        for (uint8_t i = 0; i < snap.historyCount; i++) {
            if (snap.history[i].valueW > maxVal) maxVal = snap.history[i].valueW;
        }
        float axisMax = niceCeil(maxVal);

        uint16_t gridColor = dimAccent;
        uint16_t curveColor = (!stale) ? accent : TFT_DARKGREY;
        for (uint8_t i = 1; i <= 4; i++) {
            int16_t gy = (int16_t)(chartBottom - (CHART_H * i) / 4);
            tft.drawFastHLine(chartLeft, gy, (int16_t)(chartRight - chartLeft), gridColor);
        }

        char maxLabel[16];
        snprintf(maxLabel, sizeof(maxLabel), "%.0f %s", (double)axisMax, I18n::t(I18n::Str::POWER_UNIT_W));
        tft.setTextColor(gridColor, TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        tft.drawString(maxLabel, (int16_t)(chartLeft + 2), (int16_t)(chartTop + 1));

        time_t windowStart = now - 3 * 3600;
        time_t windowSpan = 3 * 3600;
        int16_t prevX = 0, prevY = 0;
        bool havePrev = false;
        int16_t lastX = 0, lastY = 0;
        for (uint8_t i = 0; i < snap.historyCount; i++) {
            time_t pe = snap.history[i].epoch;
            float frac = (float)(pe - windowStart) / (float)windowSpan;
            if (frac < 0.0f) frac = 0.0f;
            if (frac > 1.0f) frac = 1.0f;
            int16_t px = (int16_t)(chartLeft + frac * (chartRight - chartLeft));
            float vFrac = snap.history[i].valueW / axisMax;
            if (vFrac < 0.0f) vFrac = 0.0f;
            if (vFrac > 1.0f) vFrac = 1.0f;
            int16_t py = (int16_t)(chartBottom - vFrac * CHART_H);
            if (havePrev) tft.drawLine(prevX, prevY, px, py, curveColor);
            prevX = px; prevY = py; havePrev = true;
            lastX = px; lastY = py;
        }
        tft.fillCircle(lastX, lastY, 3, curveColor);
    }

    // Zeitmarken unter der Kurve.
    int16_t timeMarksY = (int16_t)(areaTop + TIMEMARKS_OFFSET);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("-3h", chartLeft, timeMarksY);
    tft.drawString("-2h", (int16_t)(chartLeft + (chartRight - chartLeft) / 3), timeMarksY);
    tft.drawString("-1h", (int16_t)(chartLeft + 2 * (chartRight - chartLeft) / 3), timeMarksY);
    tft.drawString(I18n::t(I18n::Str::POWER_NOW_LABEL), chartRight, timeMarksY);
    tft.setTextDatum(TL_DATUM);
    LayoutCheck::rect("Zeitmarken", 0, timeMarksY, Config::SCREEN_WIDTH, 16);

    // Statuszeile.
    String statusLine = statusTextFor(snap, stale);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(statusLine, Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + STATUS_OFFSET));
    tft.setTextDatum(TL_DATUM);
    Rect tapZone = statusTapZone(areaTop);
    LayoutCheck::rect("Statuszeile", tapZone.x, tapZone.y, tapZone.w, tapZone.h);
}

bool update() {
    SolarData::Data snap = SolarData::snapshot();
    struct tm t;
    int8_t minute = getLocalTime(&t, 5) ? (int8_t)t.tm_min : (int8_t)-1;

    bool changed = firstCheck || snap.changeCounter != lastSeenChangeCounter || minute != lastSeenMinute;
    if (changed) {
        firstCheck = false;
        lastSeenChangeCounter = snap.changeCounter;
        lastSeenMinute = minute;
    }
    return changed;
}

bool onTap(int16_t x, int16_t y, int16_t areaTop, int16_t /*areaHeight*/) {
    Rect tapZone = statusTapZone(areaTop);
    if (!tapZone.contains(x, y)) return false;
    FetchTask::sendCommand(FetchTask::Command::RefreshNow);
    return true;
}

}
