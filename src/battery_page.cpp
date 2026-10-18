#include "battery_page.h"
#include "display.h"
#include "solar_data.h"
#include "app_config.h"
#include "fetch_task.h"
#include "layout_utils.h"
#include "layout_check.h"
#include "i18n.h"
#include "config.h"
#include <time.h>

namespace BatteryPage {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr int16_t SOC_Y_OFFSET = 24;
    constexpr int16_t BAR_Y_OFFSET = 50;
    constexpr int16_t BAR_H = 18;
    constexpr int16_t GRID_TOP_OFFSET = 72;
    constexpr int16_t TILE_GAP = 4;
    constexpr int16_t TILE_H = 46;
    constexpr uint8_t VALUE_SIZES[] = {2, 1};
    constexpr uint8_t SOC_SIZES[] = {3, 2, 1};
    constexpr uint8_t EXPLAIN_MAX_LINES = 3;

    uint32_t lastSeenChangeCounter = 0xFFFFFFFFu;
    int8_t lastSeenMinute = -1;
    bool firstCheck = true;

    int16_t tileWidth() { return (int16_t)((Config::SCREEN_WIDTH - 3 * TILE_GAP) / 2); }
    int16_t gridTopFor(int16_t areaTop) { return (int16_t)(areaTop + GRID_TOP_OFFSET); }

    void formatNumber(float value, uint8_t decimals, char* out, size_t outSize) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.*f", decimals, (double)value);
        if (appConfig.language == 0) {
            for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
        }
        strncpy(out, buf, outSize - 1);
        out[outSize - 1] = 0;
    }

    // Wert 0-3 direkt, darueber (wert >> 2) & 3 - siehe Auftrag
    // (vorlaeufige EPEVER-Zuordnung, noch nicht gegen das Portal bestaetigt).
    int8_t chargeCategory(int16_t raw) {
        if (raw < 0) return -1;
        if (raw <= 3) return (int8_t)raw;
        return (int8_t)((raw >> 2) & 3);
    }

    I18n::Str chargeCategoryKey(int8_t category) {
        switch (category) {
            case 1: return I18n::Str::CHARGE_STATUS_1;
            case 2: return I18n::Str::CHARGE_STATUS_2;
            case 3: return I18n::Str::CHARGE_STATUS_3;
            default: return I18n::Str::CHARGE_STATUS_0;
        }
    }

    I18n::Str explainKeyFor(int8_t category) {
        switch (category) {
            case 1: return I18n::Str::BATTERY_EXPLAIN_1;
            case 2: return I18n::Str::BATTERY_EXPLAIN_2;
            case 3: return I18n::Str::BATTERY_EXPLAIN_3;
            default: return I18n::Str::BATTERY_EXPLAIN_0;
        }
    }

    // Wortweiser Umbruch nach echter Pixelbreite bei der AKTUELL per
    // tft.setTextSize() gesetzten Groesse (muss vom Aufrufer vorher
    // gesetzt sein) - gleiches Prinzip wie HelpPopup::wrapLines(), hier
    // bewusst lokal dupliziert statt geteilt (siehe CLAUDE.md Code-Stil:
    // jeder Screen bleibt unabhaengig lauffaehig). Faellt bei einem
    // einzelnen zu breiten Wort auf harten Pixel-Schnitt zurueck, damit
    // nie ueber den Rand gezeichnet wird.
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

    // Zahlen/Wort-Text der Status-Kachel: NIE mit "..." kuerzen (siehe
    // CLAUDE.md Layout-Regel) - zuerst Size 2 einzeilig, dann Size 1
    // einzeilig versuchen, erst wenn selbst das nicht passt (in der
    // Praxis nur bei einem sehr langen rohen API-Text, nie bei den vier
    // kurzen Status-Woertern) auf bis zu 2 Zeilen bei Size 1 umbrechen.
    struct StatusRender {
        uint8_t size = 1;
        uint8_t lineCount = 1;
        String lines[2];
    };

    StatusRender layoutStatusText(const String& text, int16_t maxW) {
        StatusRender r;
        constexpr uint8_t trySizes[] = {2, 1};
        for (uint8_t s : trySizes) {
            tft.setTextSize(s);
            if (tft.textWidth(text) <= maxW) {
                r.size = s;
                r.lineCount = 1;
                r.lines[0] = text;
                return r;
            }
        }
        tft.setTextSize(1);
        r.size = 1;
        r.lineCount = wrapWords(text, maxW, r.lines, 2);
        if (r.lineCount == 0) {
            r.lineCount = 1;
            r.lines[0] = text;
        }
        return r;
    }

    void drawValueTile(int16_t x, int16_t y, int16_t w, int16_t h, I18n::Str labelKey,
                        const String& valueStr, const char* unit, bool valid, bool stale, uint16_t accent) {
        tft.drawRoundRect(x, y, w, h, 4, accent);

        tft.setTextSize(1);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(I18n::t(labelKey), (int16_t)(x + w / 2), (int16_t)(y + 5));

        constexpr int16_t LABEL_H = 14;
        constexpr int16_t UNIT_H = 14;
        int16_t valueZoneTop = (int16_t)(y + LABEL_H);
        int16_t valueZoneH = (int16_t)(h - LABEL_H - (unit ? UNIT_H : 2));

        uint16_t valueColor = (valid && !stale) ? accent : TFT_DARKGREY;
        tft.setTextColor(valueColor, TFT_BLACK);
        LayoutUtils::FitResult fit = LayoutUtils::fitText(tft, valueStr, w, valueZoneH, VALUE_SIZES, 2);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(fit.text, (int16_t)(x + w / 2), (int16_t)(valueZoneTop + valueZoneH / 2));

        if (unit) {
            tft.setTextSize(1);
            tft.setTextColor(accent, TFT_BLACK);
            tft.setTextDatum(BC_DATUM);
            tft.drawString(unit, (int16_t)(x + w / 2), (int16_t)(y + h - 4));
        }
        tft.setTextDatum(TL_DATUM);

        if (LayoutCheck::ENABLED) LayoutCheck::rect("Kachel-Rahmen", x, y, w, h);
    }

    void drawStatusTile(int16_t x, int16_t y, int16_t w, int16_t h, const StatusRender& render,
                         bool valid, bool stale, uint16_t accent) {
        tft.drawRoundRect(x, y, w, h, 4, accent);

        tft.setTextSize(1);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(I18n::t(I18n::Str::BATTERY_TILE_STATUS), (int16_t)(x + w / 2), (int16_t)(y + 5));

        constexpr int16_t LABEL_H = 14;
        int16_t valueZoneTop = (int16_t)(y + LABEL_H);
        int16_t valueZoneH = (int16_t)(h - LABEL_H - 2);

        uint16_t valueColor = (valid && !stale) ? accent : TFT_DARKGREY;
        tft.setTextColor(valueColor, TFT_BLACK);
        tft.setTextSize(render.size);
        int16_t lineH = tft.fontHeight();
        int16_t totalH = (int16_t)(render.lineCount * lineH);
        int16_t textTop = (int16_t)(valueZoneTop + (valueZoneH - totalH) / 2);
        tft.setTextDatum(TC_DATUM);
        for (uint8_t i = 0; i < render.lineCount; i++) {
            tft.drawString(render.lines[i], (int16_t)(x + w / 2), (int16_t)(textTop + i * lineH));
        }
        tft.setTextDatum(TL_DATUM);

        if (LayoutCheck::ENABLED) LayoutCheck::rect("Kachel-Rahmen", x, y, w, h);
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
                if (snap.batteryMeasuredAt > 0) {
                    struct tm t;
                    localtime_r(&snap.batteryMeasuredAt, &t);
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

    String statusDisplayFor(const SolarData::Data& snap) {
        if (snap.chargeStatusIsText && snap.chargeStatusRawText[0]) {
            return String(snap.chargeStatusRawText);
        }
        if (snap.chargeStatusCode >= 0) {
            return String(I18n::t(chargeCategoryKey(chargeCategory(snap.chargeStatusCode))));
        }
        return String("--");
    }

    // Alle von draw()/onTap() gemeinsam benoetigten Y-Positionen an einer
    // Stelle berechnen - die Erklaerungszeile und die Zeitstempel-Zeile
    // haengen von der tatsaechlichen Zeilenzahl des Status-Texts UND der
    // Erklaerung ab (siehe CLAUDE.md Layout-Regel: Folge-Elemente duerfen
    // nie an einem festen Offset haengen, wenn vorheriger Text
    // unterschiedlich lang sein kann).
    struct PageLayout {
        int16_t y0 = 0, y1 = 0, row1H = TILE_H;
        StatusRender statusRender;
        int16_t explainTop = 0;
        String explainLines[EXPLAIN_MAX_LINES];
        uint8_t explainLineCount = 0;
        int16_t timestampY = 0;
    };

    PageLayout computeLayout(int16_t areaTop, const SolarData::Data& snap) {
        PageLayout L;
        int16_t tileW = tileWidth();

        String statusDisplay = statusDisplayFor(snap);
        int16_t statusAvailW = (int16_t)(tileW - 8);
        L.statusRender = layoutStatusText(statusDisplay, statusAvailW);

        L.y0 = gridTopFor(areaTop);
        L.row1H = TILE_H;
        if (L.statusRender.lineCount > 1) {
            tft.setTextSize(L.statusRender.size);
            int16_t extra = (int16_t)((L.statusRender.lineCount - 1) * tft.fontHeight());
            L.row1H = (int16_t)(TILE_H + extra);
        }
        L.y1 = (int16_t)(L.y0 + TILE_H + TILE_GAP);
        int16_t afterGrid = (int16_t)(L.y1 + L.row1H);

        // Erklaerungszeile nur bei einem echten, bekannten Status (kein
        // Rohtext, kein "--") - ohne Wert zeigt sie nichts (Auftrag).
        L.explainLineCount = 0;
        if (!snap.chargeStatusIsText && snap.chargeStatusCode >= 0 && snap.batteryValid) {
            I18n::Str explainKey = explainKeyFor(chargeCategory(snap.chargeStatusCode));
            tft.setTextSize(1);
            int16_t explainAvailW = (int16_t)(Config::SCREEN_WIDTH - 16);
            L.explainLineCount = wrapWords(I18n::t(explainKey), explainAvailW, L.explainLines, EXPLAIN_MAX_LINES);
        }

        int16_t afterExplain;
        if (L.explainLineCount > 0) {
            L.explainTop = (int16_t)(afterGrid + 6);
            tft.setTextSize(1);
            int16_t lineH = tft.fontHeight();
            afterExplain = (int16_t)(L.explainTop + L.explainLineCount * lineH);
        } else {
            L.explainTop = afterGrid;
            afterExplain = afterGrid;
        }
        L.timestampY = (int16_t)(afterExplain + 6);

        return L;
    }

    Rect timestampTapZone(const PageLayout& L) {
        return {0, (int16_t)(L.timestampY - 14), Config::SCREEN_WIDTH, 40};
    }
}

void draw(int16_t areaTop, int16_t areaHeight) {
    SolarData::Data snap = SolarData::snapshot();
    uint16_t accent = Display::systemColor();

    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(accent, TFT_BLACK);
    tft.drawString(I18n::t(I18n::Str::PAGE_BATTERY_TITLE), Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + 6));
    tft.setTextDatum(TL_DATUM);
    LayoutCheck::rect("Titel", 0, areaTop, Config::SCREEN_WIDTH, 16);

    time_t now = time(nullptr);
    bool stale = snap.batteryMeasuredAt > 0 &&
                 (now - snap.batteryMeasuredAt) > (time_t)(Config::EPEVER_STALE_AFTER_MS / 1000);
    bool haveSoc = snap.batteryValid;

    // Ladezustand gross und zentriert.
    char socStr[8];
    if (haveSoc) snprintf(socStr, sizeof(socStr), "%.0f %%", (double)snap.batterySocPercent);
    else strncpy(socStr, "--", sizeof(socStr));
    tft.setTextColor((haveSoc && !stale) ? accent : TFT_DARKGREY, TFT_BLACK);
    int16_t socZoneTop = (int16_t)(areaTop + SOC_Y_OFFSET);
    LayoutUtils::FitResult socFit = LayoutUtils::fitText(tft, socStr, (int16_t)(Config::SCREEN_WIDTH - 20), 28, SOC_SIZES, 3);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(socFit.text, Config::SCREEN_WIDTH / 2, socZoneTop);
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);

    // Batteriesymbol: waagerechter Balken mit kleinem Pol rechts.
    int16_t barY = (int16_t)(areaTop + BAR_Y_OFFSET);
    int16_t barW = (int16_t)(Config::SCREEN_WIDTH - 60);
    int16_t barX = (int16_t)((Config::SCREEN_WIDTH - barW) / 2 - 4);
    int16_t poleW = 5, poleH = (int16_t)(BAR_H / 2);
    tft.drawRoundRect(barX, barY, barW, BAR_H, 3, accent);
    tft.fillRect((int16_t)(barX + barW), (int16_t)(barY + (BAR_H - poleH) / 2), poleW, poleH, accent);
    if (haveSoc) {
        float pct = snap.batterySocPercent < 0 ? 0 : (snap.batterySocPercent > 100 ? 100 : snap.batterySocPercent);
        int16_t fillW = (int16_t)((barW - 4) * pct / 100.0f);
        if (fillW > 0) tft.fillRect((int16_t)(barX + 2), (int16_t)(barY + 2), fillW, (int16_t)(BAR_H - 4), (!stale) ? accent : TFT_DARKGREY);
    }
    LayoutCheck::rect("Batteriesymbol", barX, barY, (int16_t)(barW + poleW), BAR_H);

    PageLayout L = computeLayout(areaTop, snap);

    int16_t tileW = tileWidth();
    int16_t x0 = TILE_GAP;
    int16_t x1 = (int16_t)(x0 + tileW + TILE_GAP);

    char voltStr[16], currStr[16], tempStr[16];
    if (snap.batteryValid) {
        formatNumber(snap.batteryVoltage, 2, voltStr, sizeof(voltStr));
        formatNumber(snap.batteryCurrent, 2, currStr, sizeof(currStr));
        formatNumber(snap.batteryTempC, 1, tempStr, sizeof(tempStr));
    } else {
        strncpy(voltStr, "--", sizeof(voltStr));
        strncpy(currStr, "--", sizeof(currStr));
        strncpy(tempStr, "--", sizeof(tempStr));
    }

    drawValueTile(x0, L.y0, tileW, TILE_H, I18n::Str::BATTERY_TILE_VOLTAGE, voltStr, I18n::t(I18n::Str::VOLT_UNIT), snap.batteryValid, stale, accent);
    drawValueTile(x1, L.y0, tileW, TILE_H, I18n::Str::BATTERY_TILE_CURRENT, currStr, I18n::t(I18n::Str::AMP_UNIT), snap.batteryValid, stale, accent);
    drawValueTile(x0, L.y1, tileW, L.row1H, I18n::Str::BATTERY_TILE_TEMP, tempStr, I18n::t(I18n::Str::CELSIUS_UNIT), snap.batteryValid, stale, accent);
    drawStatusTile(x1, L.y1, tileW, L.row1H, L.statusRender, snap.batteryValid, stale, accent);

    if (L.explainLineCount > 0) {
        tft.setTextSize(1);
        int16_t lineH = tft.fontHeight();
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        for (uint8_t i = 0; i < L.explainLineCount; i++) {
            tft.drawString(L.explainLines[i], Config::SCREEN_WIDTH / 2, (int16_t)(L.explainTop + i * lineH));
        }
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Erklaerung", 8, L.explainTop, (int16_t)(Config::SCREEN_WIDTH - 16), (int16_t)(L.explainLineCount * lineH));
    }

    String statusLine = statusTextFor(snap, stale);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(statusLine, Config::SCREEN_WIDTH / 2, L.timestampY);
    tft.setTextDatum(TL_DATUM);
    Rect tapZone = timestampTapZone(L);
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
    SolarData::Data snap = SolarData::snapshot();
    PageLayout L = computeLayout(areaTop, snap);
    Rect tapZone = timestampTapZone(L);
    if (!tapZone.contains(x, y)) return false;
    FetchTask::sendCommand(FetchTask::Command::RefreshNow);
    return true;
}

}
