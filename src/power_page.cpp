#include "power_page.h"
#include "display.h"
#include "solar_data.h"
#include "app_config.h"
#include "fetch_task.h"
#include "layout_utils.h"
#include "layout_check.h"
#include "tile_sprite.h"
#include "redraw_diag.h"
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
    // Config::PAGE_FOOTER_RESERVED_H) nicht mehr versehentlich verschiebt.
    // Alle einzeiligen Textzeilen (Stand/Achsenbeschriftung/Zeitmarken)
    // sind oben verankert (TC/TL/TR_DATUM) und brauchen deshalb die
    // VOLLE Schrifthoehe (17px bei Size 1, AppFont11pt yAdvance) als
    // Zeilenhoehe, nicht nur einen schmalen Platzhalter - sonst reicht
    // der Text (real, nicht nur nominell) in die naechste Zeile hinein.
    // Ein zu knapper Wert hier war der Grund fuer den vom erweiterten
    // Layout-Pruefer gemeldeten Konflikt bei der Achsenbeschriftung.
    constexpr int16_t LINE_H_1 = 18;
    constexpr int16_t POWER_VALUE_OFFSET = 20;
    constexpr int16_t POWER_VALUE_H = 28;
    constexpr int16_t STAND_OFFSET = (int16_t)(POWER_VALUE_OFFSET + POWER_VALUE_H + 1);
    constexpr int16_t STAND_H = LINE_H_1;
    constexpr int16_t TILES_TOP_OFFSET = (int16_t)(STAND_OFFSET + STAND_H + 1);
    constexpr int16_t TILE_GAP = 6;
    // Mind. 43px, damit die Wert+Einheit-Zeile ("125,8 V") bei Size 1
    // noch 17+2x4=25px Zonenhoehe bekommt (LABEL_H=14, 4px Rand unten) -
    // siehe CLAUDE.md Layout-Regel "Zahlen werden nie gekuerzt".
    constexpr int16_t TILE_H = 43;
    // Eigene Zeile fuer die Y-Achsen-Beschriftung OBERHALB des Diagramms
    // (nicht mehr in die Gitterlinien hineingezeichnet) - das war der
    // Grund fuer die abgeschnittene/verunstaltete Beschriftung
    // (".5000 W" statt "5000 W" auf Alex' Foto): die gedimmte oberste
    // Gitterlinie lag genau auf derselben Zeile wie der Text.
    constexpr int16_t AXIS_LABEL_OFFSET = (int16_t)(TILES_TOP_OFFSET + TILE_H + 1);
    constexpr int16_t AXIS_LABEL_H = LINE_H_1;
    constexpr int16_t CHART_TOP_OFFSET = (int16_t)(AXIS_LABEL_OFFSET + AXIS_LABEL_H + 1);
    constexpr int16_t CHART_H = 37; // darf nach unten wachsen, siehe Auftrag - nutzt den Platz bis knapp vor die Fusszone
    constexpr int16_t CHART_SIDE_MARGIN = 10;
    constexpr int16_t TIMEMARKS_OFFSET = (int16_t)(CHART_TOP_OFFSET + CHART_H + 1);
    constexpr int16_t TIMEMARKS_H = LINE_H_1;
    // +4 Abstand zur Tipp-Flaeche der Statuszeile + 14px, weil diese
    // Tipp-Flaeche (siehe statusTapZone()) 14px nach OBEN ueber den Text
    // hinausreicht.
    constexpr int16_t STATUS_OFFSET = (int16_t)(TIMEMARKS_OFFSET + TIMEMARKS_H + 4 + 14);
    constexpr uint8_t POWER_SIZES[] = {3, 2, 1};
    constexpr uint8_t VALUE_SIZES[] = {2, 1};

    uint32_t lastSeenChangeCounter = 0xFFFFFFFFu;
    int8_t lastSeenMinute = -1;
    bool firstCheck = true;

    // Pro-Bereich-Inhaltssignaturen (Auftrag "Flackern, Teil 2") - jeder
    // dynamische Bereich wird nur tatsaechlich neu gezeichnet (und damit
    // ans Off-Screen-Sprite geschickt), wenn sich sein eigener
    // angezeigter Text wirklich geaendert hat.
    String lastPowerSig = "\x01";
    String lastStandSig = "\x01";
    String lastPvSig = "\x01";
    String lastChartSig = "\x01";
    String lastStatusSig = "\x01";
    bool timeMarksDrawn = false;
    uint8_t timeMarksLang = 0xFF;

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

    // Wie LayoutUtils::fitText(), aber OHNE "..."-Kuerzung - Zahlen
    // werden nie gekuerzt (siehe CLAUDE.md Layout-Regel). Gleiches
    // Prinzip wie years_page.cpp::fitNumberNoTruncate(), hier bewusst
    // lokal dupliziert.
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

    // Misst das tatsaechlich gezeichnete Rechteck eines mittig (MC_DATUM)
    // positionierten Textes bei der gegebenen Groesse - fuer die
    // Innerhalb-des-Rahmens-Pruefung (LayoutCheck::containedRect()).
    struct Measured { int16_t x, y, w, h; };
    Measured measureCentered(TFT_eSPI& t, int16_t cx, int16_t cy, const String& s, uint8_t size) {
        t.setTextSize(size);
        int16_t w = t.textWidth(s);
        int16_t h = t.fontHeight();
        return {(int16_t)(cx - w / 2), (int16_t)(cy - h / 2), w, h};
    }
    Measured measureTopCentered(TFT_eSPI& t, int16_t cx, int16_t topY, const String& s, uint8_t size) {
        t.setTextSize(size);
        int16_t w = t.textWidth(s);
        int16_t h = t.fontHeight();
        return {(int16_t)(cx - w / 2), topY, w, h};
    }

    // Kachel mit Beschriftung oben und WERT+EINHEIT ZUSAMMEN in einer
    // Zeile darunter ("125,8 V" statt getrennter Wert-/Einheitszeilen) -
    // behebt die Ueberlappung, die bei der getrennten Darstellung
    // auftrat (die Einheit sass zu dicht am unteren Kachelrand UND
    // ueberlappte teils den Wert, siehe CLAUDE.md "Seite Leistung").
    // Zeichnet in LOKALEN Koordinaten (0,0 = Kachel-Oberkante-links) auf
    // "t" - absX/absY nur fuer LayoutCheck (Auftrag "Flackerfrei
    // zeichnen", siehe home_page.cpp).
    void renderValueTile(TFT_eSPI& t, int16_t w, int16_t h, int16_t absX, int16_t absY, I18n::Str labelKey,
                          const String& valueStr, const char* unit, bool valid, bool stale, uint16_t accent) {
        t.drawRoundRect(0, 0, w, h, 4, accent);

        t.setTextSize(1);
        t.setTextDatum(TC_DATUM);
        t.setTextColor(accent, TFT_BLACK);
        const char* labelText = I18n::t(labelKey);
        int16_t labelTopY = 5;
        t.drawString(labelText, (int16_t)(w / 2), labelTopY);
        t.setTextDatum(TL_DATUM);
        {
            Measured lr = measureTopCentered(t, (int16_t)(w / 2), labelTopY, labelText, 1);
            LayoutCheck::containedRect("Kachel-Beschriftung", absX, absY, w, h,
                                        (int16_t)(absX + lr.x), (int16_t)(absY + lr.y), lr.w, lr.h);
        }

        constexpr int16_t LABEL_H = 14;
        int16_t valueZoneTop = LABEL_H;
        int16_t valueZoneH = (int16_t)(h - LABEL_H - 4);

        uint16_t valueColor = (valid && !stale) ? accent : TFT_DARKGREY;
        t.setTextColor(valueColor, TFT_BLACK);
        String combined = valueStr;
        combined += " ";
        combined += unit;
        LayoutUtils::FitResult fit = fitNumberNoTruncate(t, combined, w, valueZoneH, VALUE_SIZES, 2);
        int16_t valueCY = (int16_t)(valueZoneTop + valueZoneH / 2);
        t.setTextDatum(MC_DATUM);
        t.drawString(fit.text, (int16_t)(w / 2), valueCY);
        t.setTextDatum(TL_DATUM);
        {
            Measured vr = measureCentered(t, (int16_t)(w / 2), valueCY, fit.text, fit.textSize);
            LayoutCheck::containedRect("Kachel-Wert", absX, absY, w, h,
                                        (int16_t)(absX + vr.x), (int16_t)(absY + vr.y), vr.w, vr.h);
        }

        LayoutCheck::rect("Kachel-Rahmen", absX, absY, w, h);
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
            case SolarData::FetchStatus::Ok: {
                // "Naechste Abfrage: HH:MM" statt der vorher doppelten
                // "Messung HH:MM" (gleiche Uhrzeit wie "Stand" oben,
                // Auftrag) - Zeitpunkt kommt aus dem tatsaechlichen
                // Fetch-Task-Zeitplan (siehe SolarData::nextFetchAt).
                char hhmm[8];
                if (snap.nextFetchAt > 0) {
                    struct tm t;
                    localtime_r(&snap.nextFetchAt, &t);
                    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", t.tm_hour, t.tm_min);
                } else {
                    strncpy(hhmm, "--:--", sizeof(hhmm) - 1);
                    hhmm[sizeof(hhmm) - 1] = 0;
                }
                char buf[40];
                snprintf(buf, sizeof(buf), I18n::t(I18n::Str::NEXT_FETCH_FMT), hhmm);
                text = buf;
                break;
            }
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

void draw(int16_t areaTop, int16_t areaHeight, bool forceRedraw) {
    SolarData::Data snap = SolarData::snapshot();
    uint16_t accent = Display::systemColor();

    // Seiteneintritt - Inhalts-Diff-Caches zuruecksetzen (siehe
    // pages.h-Kommentar/home_page.cpp).
    if (forceRedraw) {
        lastPowerSig = "\x01";
        lastStandSig = "\x01";
        lastPvSig = "\x01";
        lastChartSig = "\x01";
        lastStatusSig = "\x01";
        timeMarksDrawn = false;
    }

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

    // Grosse, zentrierte aktuelle Leistung - eigener Off-Screen-Puffer
    // (Auftrag "Flackerfrei zeichnen"), nur bei echter Aenderung des
    // angezeigten Texts.
    char powerStr[24];
    if (snap.powerValid) formatPower(snap.currentPowerW, powerStr, sizeof(powerStr));
    else strncpy(powerStr, "--", sizeof(powerStr));
    String powerSig = String(powerStr) + (numColor == accent ? "1" : "0");
    int16_t powerZoneTop = (int16_t)(areaTop + POWER_VALUE_OFFSET);
    int16_t powerZoneW = (int16_t)(Config::SCREEN_WIDTH - 20);
    if (powerSig != lastPowerSig) {
        lastPowerSig = powerSig;
        RedrawDiag::log("Leistung-Wert", "Inhalt geaendert");
        TileSprite::drawInto(10, powerZoneTop, powerZoneW, POWER_VALUE_H, [&](TFT_eSPI& t) {
            t.setTextColor(numColor, TFT_BLACK);
            LayoutUtils::FitResult powerFit = LayoutUtils::fitText(t, powerStr, powerZoneW, POWER_VALUE_H, POWER_SIZES, 3);
            t.setTextDatum(TC_DATUM);
            t.drawString(powerFit.text, (int16_t)(powerZoneW / 2), 0);
            t.setTextDatum(TL_DATUM);
        });
    }
    LayoutCheck::rect("Leistung", 10, powerZoneTop, powerZoneW, POWER_VALUE_H);

    // "Stand HH:MM" direkt darunter - Zeitstempel des letzten
    // Kurvenpunkts (snap.powerMeasuredAt, von SolarData::setPower()
    // gesetzt, siehe fetch_task.cpp::runPowerCurveFetch()).
    char standStr[16] = "--:--";
    if (snap.powerValid && snap.powerMeasuredAt > 0) {
        struct tm t;
        localtime_r(&snap.powerMeasuredAt, &t);
        snprintf(standStr, sizeof(standStr), I18n::t(I18n::Str::POWER_ASOF_FMT), t.tm_hour, t.tm_min);
    }
    int16_t standY = (int16_t)(areaTop + STAND_OFFSET);
    if (String(standStr) != lastStandSig) {
        lastStandSig = String(standStr);
        RedrawDiag::log("Leistung-Stand", "Inhalt geaendert");
        // Puffer explizit auf echte Schrifthoehe+2px pruefen/dimensionieren
        // statt sich auf LINE_H_1=18 (Richtwert fuer den Offset-Plan, kein
        // garantiertes Mindestmass) zu verlassen - Auftrag "Abgeschnittener
        // Text".
        tft.setTextSize(1);
        int16_t lineH = tft.fontHeight();
        int16_t spriteH = (int16_t)(lineH + 2);
        TileSprite::drawInto(0, standY, Config::SCREEN_WIDTH, spriteH, [&](TFT_eSPI& t) {
            t.setTextColor(TFT_DARKGREY, TFT_BLACK);
            t.setTextDatum(TC_DATUM);
            t.drawString(standStr, (int16_t)(Config::SCREEN_WIDTH / 2), 0);
            t.setTextDatum(TL_DATUM);
        });
        LayoutCheck::bufferRect("Stand-Puffer", Config::SCREEN_WIDTH, spriteH, tft.textWidth(standStr), lineH);
    }
    LayoutCheck::rect("Messzeit", 0, standY, Config::SCREEN_WIDTH, STAND_H);

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
    String pvSig = String(voltStr) + "|" + currStr + (snap.pvValid ? "1" : "0") + (stale ? "s" : "");
    if (pvSig != lastPvSig) {
        lastPvSig = pvSig;
        RedrawDiag::log("Leistung-PV-Kacheln", "Inhalt geaendert");
        TileSprite::drawInto(x0, tilesY, tileW, TILE_H, [&](TFT_eSPI& t) {
            renderValueTile(t, tileW, TILE_H, x0, tilesY, I18n::Str::BATTERY_TILE_VOLTAGE, voltStr, I18n::t(I18n::Str::VOLT_UNIT), snap.pvValid, stale, accent);
        });
        TileSprite::drawInto(x1, tilesY, tileW, TILE_H, [&](TFT_eSPI& t) {
            renderValueTile(t, tileW, TILE_H, x1, tilesY, I18n::Str::BATTERY_TILE_CURRENT, currStr, I18n::t(I18n::Str::AMP_UNIT), snap.pvValid, stale, accent);
        });
    } else {
        LayoutCheck::rect("Kachel-Rahmen", x0, tilesY, tileW, TILE_H);
        LayoutCheck::rect("Kachel-Rahmen", x1, tilesY, tileW, TILE_H);
    }

    // Kurve der letzten 3 Stunden, ohne Bibliothek mit drawLine gezeichnet
    // - inkl. Y-Achsen-Beschriftung zusammen in EINEM Off-Screen-Puffer.
    int16_t chartTop = (int16_t)(areaTop + CHART_TOP_OFFSET);
    int16_t chartLeft = CHART_SIDE_MARGIN;
    int16_t chartRight = (int16_t)(Config::SCREEN_WIDTH - CHART_SIDE_MARGIN);
    int16_t chartBottom = (int16_t)(chartTop + CHART_H);
    int16_t axisLabelY = (int16_t)(areaTop + AXIS_LABEL_OFFSET);
    int16_t chartBlockY = (int16_t)(areaTop + AXIS_LABEL_OFFSET);
    int16_t chartBlockH = (int16_t)(CHART_H + AXIS_LABEL_H + 1);
    LayoutCheck::rect("Kurve", chartLeft, chartTop, (int16_t)(chartRight - chartLeft), CHART_H);

    // Signatur aus allen Werten, die die Kurve sichtbar beeinflussen -
    // Rundung auf ganze Pixelspalten reicht (Historie aendert sich nur
    // bei einer neuen Abfrage, nicht pro Tick).
    String chartSig;
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u", (unsigned)snap.historyCount);
        chartSig = buf;
        for (uint8_t i = 0; i < snap.historyCount; i++) {
            snprintf(buf, sizeof(buf), ",%ld:%.0f", (long)snap.history[i].epoch, (double)snap.history[i].valueW);
            chartSig += buf;
        }
        chartSig += stale ? "s" : "";
    }
    if (chartSig != lastChartSig) {
        lastChartSig = chartSig;
        RedrawDiag::log("Leistung-Kurve", "Inhalt geaendert");
        TileSprite::drawInto(0, chartBlockY, Config::SCREEN_WIDTH, chartBlockH, [&](TFT_eSPI& t) {
            int16_t localChartTop = (int16_t)(chartTop - chartBlockY);
            int16_t localChartBottom = (int16_t)(chartBottom - chartBlockY);
            int16_t localAxisLabelY = (int16_t)(axisLabelY - chartBlockY);
            if (snap.historyCount < 2) {
                t.setTextColor(TFT_DARKGREY, TFT_BLACK);
                t.setTextDatum(MC_DATUM);
                t.drawString("--", (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(localChartTop + CHART_H / 2));
                t.setTextDatum(TL_DATUM);
                return;
            }
            float maxVal = 0.0f;
            for (uint8_t i = 0; i < snap.historyCount; i++) {
                if (snap.history[i].valueW > maxVal) maxVal = snap.history[i].valueW;
            }
            float axisMax = niceCeil(maxVal);

            char maxLabel[16];
            snprintf(maxLabel, sizeof(maxLabel), "%.0f %s", (double)axisMax, I18n::t(I18n::Str::POWER_UNIT_W));
            t.setTextColor(dimAccent, TFT_BLACK);
            t.setTextDatum(TL_DATUM);
            t.drawString(maxLabel, chartLeft, localAxisLabelY);

            uint16_t gridColor = dimAccent;
            uint16_t curveColor = (!stale) ? accent : TFT_DARKGREY;
            for (uint8_t i = 1; i <= 4; i++) {
                int16_t gy = (int16_t)(localChartBottom - (CHART_H * i) / 4);
                t.drawFastHLine(chartLeft, gy, (int16_t)(chartRight - chartLeft), gridColor);
            }

            time_t windowStart = time(nullptr) - 3 * 3600;
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
                int16_t py = (int16_t)(localChartBottom - vFrac * CHART_H);
                if (havePrev) t.drawLine(prevX, prevY, px, py, curveColor);
                prevX = px; prevY = py; havePrev = true;
                lastX = px; lastY = py;
            }
            t.fillCircle(lastX, lastY, 3, curveColor);
        });
    }
    {
        // Achsenbeschriftung nur registrieren, wenn tatsaechlich Kurvendaten
        // vorliegen (sonst zeigt der Puffer nur "--", siehe oben).
        if (snap.historyCount >= 2) {
            float maxVal = 0.0f;
            for (uint8_t i = 0; i < snap.historyCount; i++) {
                if (snap.history[i].valueW > maxVal) maxVal = snap.history[i].valueW;
            }
            char maxLabel[16];
            snprintf(maxLabel, sizeof(maxLabel), "%.0f %s", (double)niceCeil(maxVal), I18n::t(I18n::Str::POWER_UNIT_W));
            tft.setTextSize(1);
            int16_t lw = tft.textWidth(maxLabel);
            int16_t lh = tft.fontHeight();
            LayoutCheck::rect("Achsenbeschriftung", chartLeft, axisLabelY, lw, lh);
        }
    }

    // Zeitmarken unter der Kurve - Rand-Beschriftungen (links/rechts)
    // bekommen einen Rand-Datum statt TC_DATUM, sonst ragt die Haelfte
    // des Texts ueber den Bildschirmrand hinaus (war bei "jetzt"/"now"
    // rechts der Fall, siehe CLAUDE.md "Seite Leistung"). Statisch
    // (haengt nur von der Sprache ab) - einmalig zeichnen reicht, aber
    // der Off-Screen-Puffer schadet nicht und haelt das Muster einheitlich.
    int16_t timeMarksY = (int16_t)(areaTop + TIMEMARKS_OFFSET);
    if (!timeMarksDrawn || timeMarksLang != appConfig.language) {
        timeMarksDrawn = true;
        timeMarksLang = appConfig.language;
        // Puffer explizit auf echte Schrifthoehe+2px - war vorher fest 16
        // (kleiner als die Schrifthoehe 17), Auftrag "Abgeschnittener Text".
        tft.setTextSize(1);
        int16_t tmLineH = tft.fontHeight();
        int16_t tmSpriteH = (int16_t)(tmLineH + 2);
        TileSprite::drawInto(0, timeMarksY, Config::SCREEN_WIDTH, tmSpriteH, [&](TFT_eSPI& t) {
            t.setTextColor(TFT_DARKGREY, TFT_BLACK);
            t.setTextDatum(TL_DATUM);
            t.drawString("-3h", chartLeft, 0);
            t.setTextDatum(TC_DATUM);
            t.drawString("-2h", (int16_t)(chartLeft + (chartRight - chartLeft) / 3), 0);
            t.drawString("-1h", (int16_t)(chartLeft + 2 * (chartRight - chartLeft) / 3), 0);
            t.setTextDatum(TR_DATUM);
            t.drawString(I18n::t(I18n::Str::POWER_NOW_LABEL), chartRight, 0);
            t.setTextDatum(TL_DATUM);
        });
        LayoutCheck::bufferRect("Zeitmarken-Puffer", Config::SCREEN_WIDTH, tmSpriteH, Config::SCREEN_WIDTH, tmLineH);
    }
    LayoutCheck::rect("Zeitmarken", 0, timeMarksY, Config::SCREEN_WIDTH, 16);

    // Statuszeile.
    String statusLine = statusTextFor(snap, stale);
    int16_t statusY = (int16_t)(areaTop + STATUS_OFFSET);
    if (statusLine != lastStatusSig) {
        lastStatusSig = statusLine;
        RedrawDiag::log("Leistung-Statuszeile", "Inhalt geaendert");
        // Gleicher Bugfix wie oben: Puffer auf echte Schrifthoehe+2px statt
        // fester Hoehe 18 bei Text-Y=2 (Auftrag "Abgeschnittener Text").
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
    Rect tapZone = statusTapZone(areaTop);
    LayoutCheck::rect("Statuszeile", tapZone.x, tapZone.y, tapZone.w, tapZone.h);
}

bool update() {
    SolarData::Data snap = SolarData::snapshot();
    struct tm t;
    int8_t minute = getLocalTime(&t, 5) ? (int8_t)t.tm_min : (int8_t)-1;

    bool solarChanged = snap.changeCounter != lastSeenChangeCounter;
    bool minuteTicked = minute != lastSeenMinute;
    bool changed = firstCheck || solarChanged || minuteTicked;
    if (changed) {
        if (firstCheck) RedrawDiag::log("Leistung", "erster Aufruf");
        if (solarChanged) RedrawDiag::log("Leistung", "SolarData-Zaehler geaendert");
        if (minuteTicked) RedrawDiag::log("Leistung", "Minutenwechsel");
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
