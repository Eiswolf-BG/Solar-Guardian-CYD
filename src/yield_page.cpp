#include "yield_page.h"
#include "display.h"
#include "solar_data.h"
#include "yield_ledger.h"
#include "app_config.h"
#include "fetch_task.h"
#include "layout_utils.h"
#include "layout_check.h"
#include "tile_sprite.h"
#include "redraw_diag.h"
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

    // Pro-Kachel-Inhaltssignaturen (Auftrag "Flackern, Teil 2").
    String lastTileSig[4] = {"\x01", "\x01", "\x01", "\x01"};
    String lastStatusSig = "\x01";

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
    // nie ueberlappend (siehe CLAUDE.md Layout-Regel). Zeichnet in
    // LOKALEN Koordinaten (0,0 = Kachel-Oberkante-links) - absX/absY nur
    // fuer LayoutCheck (Auftrag "Flackerfrei zeichnen").
    void renderTile(TFT_eSPI& t, int16_t w, int16_t h, int16_t absX, int16_t absY, I18n::Str labelKey,
                     const char* valStr, bool valid, bool stale, uint16_t accent) {
        t.drawRoundRect(0, 0, w, h, 4, accent);

        t.setTextSize(1);
        t.setTextDatum(TC_DATUM);
        t.setTextColor(accent, TFT_BLACK);
        t.drawString(I18n::t(labelKey), (int16_t)(w / 2), 6);

        constexpr int16_t LABEL_H = 16;
        constexpr int16_t UNIT_H = 16;
        int16_t valueZoneTop = LABEL_H;
        int16_t valueZoneH = (int16_t)(h - LABEL_H - UNIT_H);

        uint16_t valueColor = (valid && !stale) ? accent : TFT_DARKGREY;
        t.setTextColor(valueColor, TFT_BLACK);
        LayoutUtils::FitResult fit = LayoutUtils::fitText(t, valStr, w, valueZoneH, VALUE_SIZES, 2);
        t.setTextDatum(MC_DATUM);
        t.drawString(fit.text, (int16_t)(w / 2), (int16_t)(valueZoneTop + valueZoneH / 2));

        t.setTextSize(1);
        t.setTextColor(accent, TFT_BLACK);
        t.setTextDatum(BC_DATUM);
        t.drawString(I18n::t(I18n::Str::KWH_UNIT), (int16_t)(w / 2), (int16_t)(h - 6));
        t.setTextDatum(TL_DATUM);

        LayoutCheck::rect("Kachel-Rahmen", absX, absY, w, h);
    }

    String statusTextFor(const SolarData::Data& snap, bool stale) {
        String text;
        switch (snap.fetchStatus) {
            case SolarData::FetchStatus::NoAccount:
                text = I18n::t(I18n::Str::NO_ACCOUNT_CONFIGURED);
                break;
            case SolarData::FetchStatus::NoDeviceNo:
                text = I18n::t(I18n::Str::NO_DEVICE_NO_CONFIGURED);
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

void draw(int16_t areaTop, int16_t areaHeight, bool forceRedraw) {
    SolarData::Data snap = SolarData::snapshot();
    uint16_t accent = Display::systemColor();

    if (forceRedraw) {
        for (auto& s : lastTileSig) s = "\x01";
        lastStatusSig = "\x01";
    }

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

    struct TileSpec { int16_t x, y; I18n::Str labelKey; float value; };
    TileSpec specs[4] = {
        {x0, y0, I18n::Str::YIELD_TILE_TODAY, yv.todayKwh},
        {x1, y0, I18n::Str::YIELD_TILE_WEEK, yv.weekKwh},
        {x0, y1, I18n::Str::YIELD_TILE_MONTH, yv.monthKwh},
        {x1, y1, I18n::Str::YIELD_TILE_YEAR, yv.yearKwh},
    };
    for (uint8_t i = 0; i < 4; i++) {
        char valStr[16];
        if (yv.valid) formatKwh(specs[i].value, valStr, sizeof(valStr));
        else strncpy(valStr, "--", sizeof(valStr));
        String sig = String(valStr) + (yv.valid ? "1" : "0") + (stale ? "s" : "");
        if (sig != lastTileSig[i]) {
            lastTileSig[i] = sig;
            RedrawDiag::log("Ertrag-Kachel", "Inhalt geaendert");
            int16_t tx = specs[i].x, ty = specs[i].y;
            I18n::Str labelKey = specs[i].labelKey;
            TileSprite::drawInto(tx, ty, tileW, TILE_H, [&, tx, ty, labelKey](TFT_eSPI& t) {
                renderTile(t, tileW, TILE_H, tx, ty, labelKey, valStr, yv.valid, stale, accent);
            });
        } else {
            LayoutCheck::rect("Kachel-Rahmen", specs[i].x, specs[i].y, tileW, TILE_H);
        }
    }

    String statusLine = statusTextFor(snap, stale);
    int16_t statusY = statusYFor(areaTop);
    if (statusLine != lastStatusSig) {
        lastStatusSig = statusLine;
        RedrawDiag::log("Ertrag-Statuszeile", "Inhalt geaendert");
        // Puffer muss mindestens die echte Schrifthoehe (+2px Reserve)
        // fassen - gleicher Bugfix wie battery_page.cpp (Auftrag
        // "Abgeschnittener Text"): vorher feste Hoehe 18 bei Text-Y=2
        // (braucht 2+Schrifthoehe), dadurch unten abgeschnitten.
        tft.setTextSize(1);
        int16_t lineH = tft.fontHeight();
        int16_t spriteH = (int16_t)(lineH + 2);
        TileSprite::drawInto(0, statusY, Config::SCREEN_WIDTH, spriteH, [&](TFT_eSPI& t) {
            t.setTextDatum(TC_DATUM);
            t.setTextColor(TFT_DARKGREY, TFT_BLACK);
            t.drawString(statusLine, (int16_t)(Config::SCREEN_WIDTH / 2), 0);
            t.setTextDatum(TL_DATUM);
        });
        LayoutCheck::bufferRect("Statuszeile-Puffer", Config::SCREEN_WIDTH, spriteH, tft.textWidth(statusLine), lineH);
    }
    LayoutCheck::rect("Statuszeile", 0, (int16_t)(statusY - 14), Config::SCREEN_WIDTH, 40);
}

bool update() {
    SolarData::Data snap = SolarData::snapshot();
    struct tm t;
    int8_t minute = getLocalTime(&t, 5) ? (int8_t)t.tm_min : (int8_t)-1;

    bool solarChanged = snap.changeCounter != lastSeenChangeCounter;
    bool minuteTicked = minute != lastSeenMinute;
    bool changed = firstCheck || solarChanged || minuteTicked;
    if (changed) {
        if (firstCheck) RedrawDiag::log("Ertrag", "erster Aufruf");
        if (solarChanged) RedrawDiag::log("Ertrag", "SolarData-Zaehler geaendert");
        if (minuteTicked) RedrawDiag::log("Ertrag", "Minutenwechsel");
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
