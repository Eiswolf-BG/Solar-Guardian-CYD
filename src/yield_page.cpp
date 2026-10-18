#include "yield_page.h"
#include "display.h"
#include "solar_data.h"
#include "yield_ledger.h"
#include "app_config.h"
#include "fetch_task.h"
#include "layout_utils.h"
#include "layout_check.h"
#include "i18n.h"
#include "config.h"
#include <time.h>

namespace YieldPage {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr int16_t GRID_TOP_OFFSET = 26;
    constexpr int16_t TILE_GAP = 10;
    constexpr int16_t TILE_H = 76; // etwas groesser, seit "Gesamt" entfiel (Alex' Wunsch)
    constexpr uint8_t VALUE_SIZES[] = {2, 1};

    uint32_t lastSeenChangeCounter = 0xFFFFFFFFu;
    int8_t lastSeenMinute = -1;
    bool firstCheck = true;

    int16_t gridTopFor(int16_t areaTop) { return (int16_t)(areaTop + GRID_TOP_OFFSET); }
    int16_t tileWidth() { return (int16_t)((Config::SCREEN_WIDTH - 3 * TILE_GAP) / 2); }
    int16_t statusYFor(int16_t areaTop) {
        int16_t y0 = gridTopFor(areaTop);
        int16_t y1 = (int16_t)(y0 + TILE_H + TILE_GAP);
        return (int16_t)(y1 + TILE_H + 18);
    }

    // Zwei Nachkommastellen, kein Tausendertrennzeichen, Dezimalzeichen
    // je Sprache (Komma Deutsch, Punkt Englisch) - siehe Auftrag.
    void formatKwh(float value, char* out, size_t outSize) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", (double)value);
        if (appConfig.language == 0) {
            for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
        }
        strncpy(out, buf, outSize - 1);
        out[outSize - 1] = 0;
    }

    // Kachel: Name klein oben, Wert per fitText in der Mitte, Einheit
    // "kWh" in einer EIGENEN Zeile unter dem Wert - nie nebeneinander,
    // nie ueberlappend (siehe CLAUDE.md Layout-Regel).
    void drawTile(int16_t x, int16_t y, int16_t w, int16_t h, I18n::Str labelKey,
                  float value, bool valid, bool stale, uint16_t accent) {
        tft.drawRoundRect(x, y, w, h, 4, accent);

        tft.setTextSize(1);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(I18n::t(labelKey), (int16_t)(x + w / 2), (int16_t)(y + 6));

        char valStr[16];
        if (valid) {
            formatKwh(value, valStr, sizeof(valStr));
        } else {
            strncpy(valStr, "--", sizeof(valStr));
        }

        // Reservierter Bereich fuer den Wert: unter dem Namen, oberhalb
        // der eigenen Einheiten-Zeile - fitText waehlt die groesste
        // Schrift, die dort (inkl. 4px Innenabstand) noch passt.
        constexpr int16_t LABEL_H = 16;
        constexpr int16_t UNIT_H = 16;
        int16_t valueZoneTop = (int16_t)(y + LABEL_H);
        int16_t valueZoneH = (int16_t)(h - LABEL_H - UNIT_H);

        uint16_t valueColor = (valid && !stale) ? accent : TFT_DARKGREY;
        tft.setTextColor(valueColor, TFT_BLACK);
        LayoutUtils::FitResult fit = LayoutUtils::fitText(tft, valStr, w, valueZoneH, VALUE_SIZES, 2);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(fit.text, (int16_t)(x + w / 2), (int16_t)(valueZoneTop + valueZoneH / 2));

        tft.setTextSize(1);
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(BC_DATUM);
        tft.drawString(I18n::t(I18n::Str::KWH_UNIT), (int16_t)(x + w / 2), (int16_t)(y + h - 6));
        tft.setTextDatum(TL_DATUM);

        if (LayoutCheck::ENABLED) {
            LayoutCheck::rect("Kachel-Rahmen", x, y, w, h);
        }
    }

    String statusTextFor(const SolarData::Data& snap, bool stale) {
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
                if (snap.lastSuccessfulFetch > 0) {
                    struct tm t;
                    localtime_r(&snap.lastSuccessfulFetch, &t);
                    char buf[32];
                    snprintf(buf, sizeof(buf), I18n::t(I18n::Str::STATUS_UPDATED_FMT), t.tm_hour, t.tm_min);
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
}

void draw(int16_t areaTop, int16_t areaHeight) {
    SolarData::Data snap = SolarData::snapshot();
    uint16_t accent = Display::systemColor();

    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(accent, TFT_BLACK);
    tft.drawString(I18n::t(I18n::Str::PAGE_YIELD_TITLE), Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + 6));
    tft.setTextDatum(TL_DATUM);
    LayoutCheck::rect("Titel", 0, areaTop, Config::SCREEN_WIDTH, 16);

    time_t now = time(nullptr);
    bool stale = snap.lastSuccessfulFetch > 0 &&
                 (now - snap.lastSuccessfulFetch) > (time_t)(Config::EPEVER_STALE_AFTER_MS / 1000);

    // Anzeige kommt aus dem Ertragsbuch (selbst gezaehlt, siehe
    // yield_ledger.h) - die rohen Portal-Werte in SolarData dienen nur
    // noch als einmaliger Startwert.
    YieldLedger::Values yv = YieldLedger::current();

    int16_t tileW = tileWidth();
    int16_t x0 = TILE_GAP;
    int16_t x1 = (int16_t)(x0 + tileW + TILE_GAP);
    int16_t y0 = gridTopFor(areaTop);
    int16_t y1 = (int16_t)(y0 + TILE_H + TILE_GAP);

    drawTile(x0, y0, tileW, TILE_H, I18n::Str::YIELD_TILE_TODAY, yv.todayKwh, yv.valid, stale, accent);
    drawTile(x1, y0, tileW, TILE_H, I18n::Str::YIELD_TILE_WEEK, yv.weekKwh, yv.valid, stale, accent);
    drawTile(x0, y1, tileW, TILE_H, I18n::Str::YIELD_TILE_MONTH, yv.monthKwh, yv.valid, stale, accent);
    drawTile(x1, y1, tileW, TILE_H, I18n::Str::YIELD_TILE_YEAR, yv.yearKwh, yv.valid, stale, accent);

    String statusLine = statusTextFor(snap, stale);
    int16_t statusY = statusYFor(areaTop);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(statusLine, Config::SCREEN_WIDTH / 2, statusY);
    tft.setTextDatum(TL_DATUM);
    LayoutCheck::rect("Statuszeile", 0, (int16_t)(statusY - 14), Config::SCREEN_WIDTH, 40);
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
    int16_t statusY = statusYFor(areaTop);
    // Mind. 40px hohe Tipp-Flaeche (CLAUDE.md), ueber die volle Breite.
    Rect statusRow = {0, (int16_t)(statusY - 14), Config::SCREEN_WIDTH, 40};
    if (!statusRow.contains(x, y)) return false;
    FetchTask::sendCommand(FetchTask::Command::RefreshNow);
    return true;
}

}
