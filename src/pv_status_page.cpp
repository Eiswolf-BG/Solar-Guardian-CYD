#include "pv_status_page.h"
#include "display.h"
#include "solar_data.h"
#include "app_config.h"
#include "fetch_task.h"
#include "epever_datapoints.h"
#include "layout_utils.h"
#include "layout_check.h"
#include "tile_sprite.h"
#include "redraw_diag.h"
#include "refresh_feedback_text.h"
#include "i18n.h"
#include "config.h"
#include <time.h>
#include <math.h>

namespace PvStatusPage {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    // Gleiche Kachelmasse wie die bestehenden 2x2-Kachelseiten (Auftrag
    // "TILE_H 50", siehe battery_page.cpp).
    constexpr int16_t GRID_TOP_OFFSET = 48;
    constexpr int16_t TILE_GAP = 2;
    constexpr int16_t TILE_H = 50;
    constexpr uint8_t VALUE_SIZES[] = {2, 1};
    // +4 gewollter Abstand + 14, weil die Tipp-Flaeche der Statuszeile
    // (siehe statusTapZone()) 14px UEBER den Text hinausreicht - gleiches
    // Prinzip wie battery_page.cpp/power_page.cpp.
    constexpr int16_t STATUS_GAP = 4 + 14;

    uint32_t lastSeenChangeCounter = 0xFFFFFFFFu;
    int8_t lastSeenMinute = -1;
    bool firstCheck = true;

    // Pro-Kachel-Inhaltssignaturen (Auftrag "Flackern, Teil 2").
    String lastTileSig[4] = {"\x01", "\x01", "\x01", "\x01"};
    String lastStatusSig = "\x01";

    // Rueckmeldung zum manuellen Refresh (Auftrag "Refresh-Regeln"), siehe
    // battery_page.cpp fuer die genaue Funktionsweise von FeedbackState.
    // Anders als bei den anderen Seiten erscheint die Rueckmeldung hier
    // NUR in der tatsaechlich angetippten Kachel (Auftrag "Nur diese
    // Kachel neu zeichnen"), nicht in einer gemeinsamen Statuszeile -
    // lastTappedTile haelt fest, welche der 4 Kacheln das war (-1 = keine/
    // noch nie, z.B. direkt nach Seiteneintritt).
    RefreshFeedbackText::FeedbackState feedbackState;
    int8_t lastTappedTile = -1;

    int16_t tileWidth() { return (int16_t)((Config::SCREEN_WIDTH - 3 * TILE_GAP) / 2); }
    int16_t gridTopFor(int16_t areaTop) { return (int16_t)(areaTop + GRID_TOP_OFFSET); }
    int16_t statusYFor(int16_t areaTop) {
        int16_t y0 = gridTopFor(areaTop);
        int16_t y1 = (int16_t)(y0 + TILE_H + TILE_GAP);
        return (int16_t)(y1 + TILE_H + STATUS_GAP);
    }

    // Zwei Nachkommastellen, Dezimalzeichen je Sprache - gleiches Prinzip
    // wie battery_page.cpp/power_page.cpp.
    void formatNumber(float value, uint8_t decimals, char* out, size_t outSize) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.*f", decimals, (double)value);
        if (appConfig.language == 0) {
            for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
        }
        strncpy(out, buf, outSize - 1);
        out[outSize - 1] = 0;
    }

    // Unter 1000 W in W (ganze Zahl), ab 1000 W in kW mit 2
    // Nachkommastellen - EXAKT dieselbe Regel/Quelle wie home_page.cpp/
    // power_page.cpp (Auftrag "muss zur Hauptansicht passen").
    void formatPower(float watts, char* out, size_t outSize, const char** outUnit) {
        char buf[16];
        bool kw = fabsf(watts) >= 1000.0f;
        if (kw) {
            snprintf(buf, sizeof(buf), "%.2f", watts / 1000.0f);
            *outUnit = I18n::t(I18n::Str::POWER_UNIT_KW);
        } else {
            snprintf(buf, sizeof(buf), "%.0f", watts);
            *outUnit = I18n::t(I18n::Str::POWER_UNIT_W);
        }
        if (appConfig.language == 0) {
            for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
        }
        strncpy(out, buf, outSize - 1);
        out[outSize - 1] = 0;
    }

    // Heutiges Leistungsmaximum - EXAKT dieselbe Berechnung wie die
    // "Max:"-Zeile im Tagesverlauf (daily_curve_page.cpp), auf denselben
    // snap.dailyCurve[]-Daten (keine eigene/zusaetzliche Quelle). Liefert
    // 0, solange noch keine Punkte fuer heute vorliegen (z.B. kurz nach
    // 04:30, bevor die erste Abfrage des Tages durch ist).
    float computeMaxToday(const SolarData::Data& snap) {
        float maxVal = 0.0f;
        for (uint16_t i = 0; i < snap.dailyCurveCount; i++) {
            if ((float)snap.dailyCurve[i].watts > maxVal) maxVal = (float)snap.dailyCurve[i].watts;
        }
        return maxVal;
    }

    // Misst das tatsaechlich gezeichnete Rechteck eines Textes - fuer
    // LayoutCheck::containedRect(), gleiches Prinzip wie in den anderen
    // Seiten (hier bewusst lokal dupliziert, siehe CLAUDE.md Code-Stil).
    struct Measured { int16_t x, y, w, h; };
    Measured measureTopCentered(TFT_eSPI& t, int16_t cx, int16_t topY, const String& s, uint8_t size) {
        t.setTextSize(size);
        int16_t w = t.textWidth(s);
        int16_t h = t.fontHeight();
        return {(int16_t)(cx - w / 2), topY, w, h};
    }

    // Wie LayoutUtils::fitText(), aber OHNE "..."-Kuerzung - Zahlen werden
    // nie gekuerzt (CLAUDE.md Layout-Regel), gleiches Prinzip wie in den
    // anderen Seiten.
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

    // Beschriftung oben (Systemfarbe), Wert+Einheit zusammen in einer
    // Zeile darunter (Zahl in Sekundaerfarbe, Einheit in Systemfarbe) -
    // Farbregeln exakt wie battery_page.cpp/power_page.cpp. Zeichnet in
    // LOKALEN Koordinaten - absX/absY nur fuer LayoutCheck. labelOverride:
    // leer = normale Beschriftung, sonst Refresh-Rueckmeldung STATT der
    // Beschriftung (Auftrag "Fehler in der Refresh-Rueckmeldung") - der
    // Wert bleibt in JEDEM Fall sichtbar.
    void renderValueTile(TFT_eSPI& t, int16_t w, int16_t h, int16_t absX, int16_t absY, I18n::Str labelKey,
                          const String& valueStr, const char* unit, bool valid, bool stale, uint16_t accent,
                          const String& labelOverride = String()) {
        t.drawRoundRect(0, 0, w, h, 4, accent);

        t.setTextSize(1);
        t.setTextDatum(TC_DATUM);
        t.setTextColor(accent, TFT_BLACK);
        String labelText = labelOverride.length() > 0 ? labelOverride : String(I18n::t(labelKey));
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

        uint16_t numColor = Display::valueColor(valid && !stale, accent);
        String combined = valueStr;
        if (unit) { combined += " "; combined += unit; }
        LayoutUtils::FitResult fit = fitNumberNoTruncate(t, combined, w, valueZoneH, VALUE_SIZES, 2);
        t.setTextSize(fit.textSize);
        int16_t numW = t.textWidth(valueStr);
        int16_t totalW = t.textWidth(combined);
        int16_t valueCY = (int16_t)(valueZoneTop + valueZoneH / 2);
        int16_t startX = (int16_t)(w / 2 - totalW / 2);
        t.setTextDatum(ML_DATUM);
        t.setTextColor(numColor, TFT_BLACK);
        t.drawString(valueStr, startX, valueCY);
        if (unit) {
            t.setTextColor(accent, TFT_BLACK);
            t.drawString(String(" ") + unit, (int16_t)(startX + numW), valueCY);
        }
        t.setTextDatum(TL_DATUM);
        {
            int16_t th = t.fontHeight();
            LayoutCheck::containedRect("Kachel-Wert", absX, absY, w, h,
                                        (int16_t)(absX + startX), (int16_t)(absY + valueCY - th / 2), totalW, th);
        }

        LayoutCheck::rect("Kachel-Rahmen", absX, absY, w, h);
    }

    // Wortweiser Umbruch nach echter Pixelbreite - gleiches Prinzip wie in
    // anderen Screens (hier bewusst lokal dupliziert, siehe CLAUDE.md
    // Code-Stil).
    uint8_t wrapWords(TFT_eSPI& t, const String& text, int16_t maxW, String* outLines, uint8_t maxLines) {
        String remaining = text;
        uint8_t lineCount = 0;
        while (remaining.length() > 0 && lineCount < maxLines) {
            int16_t lastGoodSpace = -1;
            int16_t i = 0;
            String line = "";
            while (i < (int16_t)remaining.length()) {
                char c = remaining[i];
                String candidate = line + c;
                if (t.textWidth(candidate) > maxW && line.length() > 0) break;
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

    // "Naechste Abfrage: HH:MM" - exakt dieselbe Quelle/Formatierung wie
    // battery_page.cpp/power_page.cpp (snap.nextFetchAt, gemeinsamer
    // Fetch-Task-Zeitplan - zeigt im Nachtmodus automatisch "04:30").
    // ageMinutes: Alter des massgeblichen HAUPTWERTS in Minuten (-1 = noch
    // kein Wert) - zeigt ab 2 Minuten das tatsaechliche Alter statt des
    // blossen Worts "veraltet" (Auftrag "veraltet nach erfolgreichem
    // Refresh"). Die reine Wort-Warnung fuehrte zu der berechtigten Frage
    // "Ich habe gerade aktualisiert, wie kann es veraltet sein?" - der
    // echte Grund war, dass ein selten aktualisierter PV-Einzelwert die
    // Anzeige bestimmte, nicht die frischen Hauptwerte.
    String statusTextFor(const SolarData::Data& snap, long ageMinutes) {
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
            case SolarData::FetchStatus::Ok: {
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
        if (ageMinutes >= 2) {
            char ageBuf[32];
            snprintf(ageBuf, sizeof(ageBuf), I18n::t(I18n::Str::PORTAL_DATA_AGE_FMT), (int)ageMinutes);
            text += " ";
            text += ageBuf;
        }
        return text;
    }

    Rect statusTapZoneFor(int16_t areaTop) {
        int16_t y = statusYFor(areaTop);
        return {0, (int16_t)(y - 14), Config::SCREEN_WIDTH, 40};
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
    tft.drawString(I18n::t(I18n::Str::PAGE_PV_STATUS_TITLE), Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + 6));
    tft.setTextDatum(TL_DATUM);
    LayoutCheck::rect("Titel", 0, areaTop, Config::SCREEN_WIDTH, 16);

    time_t now = time(nullptr);
    // Frische/Alter wird am HAUPTWERT Leistung gemessen (snap.
    // powerMeasuredAt), NICHT am PV-spezifischen Zeitstempel
    // (snap.pvMeasuredAt) - der aktualisiert sich laut Portal deutlich
    // seltener als die Leistung selbst und wuerde die ganze Seite sonst
    // faelschlich als veraltet zeigen, obwohl die Hauptwerte frisch sind
    // (Auftrag "veraltet nach erfolgreichem Refresh", Punkt 3). Gilt
    // einheitlich fuer die Statuszeile UND das Abdunkeln der Werte in
    // allen 4 Kacheln.
    long ageMinutes = snap.powerMeasuredAt > 0 ? (long)((now - snap.powerMeasuredAt) / 60) : -1;
    bool stale = snap.powerMeasuredAt > 0 &&
                 (now - snap.powerMeasuredAt) > (time_t)(Config::EPEVER_STALE_AFTER_MS / 1000);

    int16_t tileW = tileWidth();
    int16_t x0 = TILE_GAP;
    int16_t x1 = (int16_t)(x0 + tileW + TILE_GAP);
    int16_t y0 = gridTopFor(areaTop);
    int16_t y1 = (int16_t)(y0 + TILE_H + TILE_GAP);

    char voltStr[16], currStr[16], powerStr[16], maxStr[16];
    const char* powerUnit = I18n::t(I18n::Str::POWER_UNIT_W);
    const char* maxUnit = I18n::t(I18n::Str::POWER_UNIT_W);
    if (snap.pvValid) {
        formatNumber(snap.pvVoltage, 2, voltStr, sizeof(voltStr));
        formatNumber(snap.pvCurrent, 2, currStr, sizeof(currStr));
    } else {
        strncpy(voltStr, "--", sizeof(voltStr));
        strncpy(currStr, "--", sizeof(currStr));
    }
    if (snap.powerValid) formatPower(snap.currentPowerW, powerStr, sizeof(powerStr), &powerUnit);
    else strncpy(powerStr, "--", sizeof(powerStr));
    float maxToday = computeMaxToday(snap);
    bool maxValid = snap.dailyCurveCount > 0;
    if (maxValid) formatPower(maxToday, maxStr, sizeof(maxStr), &maxUnit);
    else strncpy(maxStr, "--", sizeof(maxStr));

    struct TileSpec { int16_t x, y; I18n::Str labelKey; const char* valueStr; const char* unit; bool valid; };
    TileSpec specs[4] = {
        {x0, y0, I18n::Str::PV_VOLTAGE_LABEL, voltStr, I18n::t(I18n::Str::VOLT_UNIT), snap.pvValid},
        {x1, y0, I18n::Str::PV_CURRENT_LABEL, currStr, I18n::t(I18n::Str::AMP_UNIT), snap.pvValid},
        {x0, y1, I18n::Str::PV_POWER_LABEL, powerStr, powerUnit, snap.powerValid},
        {x1, y1, I18n::Str::PV_MAX_TODAY_LABEL, maxStr, maxUnit, maxValid},
    };

    // Refresh-Rueckmeldung erscheint NUR in der angetippten Kachel
    // (Auftrag "Fehler in der Refresh-Rueckmeldung") - ersetzt dort NUR
    // die Beschriftungszeile, der Wert bleibt immer sichtbar.
    // feedbackState.update() treibt die Zeitsteuerung (2s-Fenster/
    // "Working" mit harter 15s-Obergrenze, siehe refresh_feedback_text.h).
    // Die ANZEIGE-Entscheidung kommt aus feedbackState.active, NICHT aus
    // dem Rueckgabewert (siehe dortiger Kommentar, Auftrag "Rueckmeldung
    // bleibt dauerhaft stehen").
    feedbackState.update(snap);
    tft.setTextSize(1); // fuer die Breitenmessung unten (Schrift explizit setzen)

    for (uint8_t i = 0; i < 4; i++) {
        bool showFeedbackHere = feedbackState.active && (i == lastTappedTile);
        String labelOverride = showFeedbackHere
            ? RefreshFeedbackText::fitToWidth(tft, feedbackState.text, feedbackState.shortText, (int16_t)(tileW - 8))
            : String();
        String sig = (showFeedbackHere ? (String("F") + labelOverride) : String())
            + "|" + specs[i].valueStr + "|" + specs[i].unit + (specs[i].valid ? "1" : "0") + (stale ? "s" : "");
        if (sig != lastTileSig[i]) {
            lastTileSig[i] = sig;
            RedrawDiag::log("PV-Status-Kachel", "Inhalt geaendert");
            int16_t tx = specs[i].x, ty = specs[i].y;
            I18n::Str labelKey = specs[i].labelKey;
            String valueStr = specs[i].valueStr;
            const char* unit = specs[i].unit;
            bool valid = specs[i].valid;
            TileSprite::drawInto(tx, ty, tileW, TILE_H, [&, tx, ty, labelKey, valueStr, unit, valid, labelOverride](TFT_eSPI& t) {
                renderValueTile(t, tileW, TILE_H, tx, ty, labelKey, valueStr, unit, valid, stale, accent, labelOverride);
            });
        } else {
            LayoutCheck::rect("Kachel-Rahmen", specs[i].x, specs[i].y, tileW, TILE_H);
        }
    }

    String statusLine = statusTextFor(snap, ageMinutes);
    int16_t statusY = statusYFor(areaTop);

    // Auftrag "Statuszeile wird abgeschnitten" - LayoutCheck meldete hier
    // tatsaechlich einen Konflikt (Puffer 240px, Text 243px) - siehe
    // battery_page.cpp fuer die ausfuehrliche Begruendung. Berechnung
    // laeuft JEDEN draw()-Aufruf, damit die Registrierung unten immer
    // aktuell ist.
    tft.setTextSize(1);
    int16_t lineH = tft.fontHeight();
    int16_t maxW = (int16_t)(Config::SCREEN_WIDTH - 8);
    String lines[2];
    uint8_t lineCount = 1;
    if (tft.textWidth(statusLine) <= maxW) {
        lines[0] = statusLine;
    } else {
        lineCount = wrapWords(tft, statusLine, maxW, lines, 2);
        if (lineCount == 0) { lineCount = 1; lines[0] = statusLine; }
    }
    int16_t pitch = (lineCount > 1) ? 15 : lineH;
    int16_t spriteH = (int16_t)(pitch * lineCount + 2);

    if (statusLine != lastStatusSig) {
        lastStatusSig = statusLine;
        RedrawDiag::log("PV-Status-Statuszeile", "Inhalt geaendert");
        TileSprite::drawInto(0, statusY, Config::SCREEN_WIDTH, spriteH, [&](TFT_eSPI& t) {
            t.setTextDatum(TC_DATUM);
            t.setTextColor(Display::secondaryColor(), TFT_BLACK);
            for (uint8_t i = 0; i < lineCount; i++) {
                t.drawString(lines[i], (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(i * pitch));
            }
            t.setTextDatum(TL_DATUM);
        });
        int16_t widestLineW = 0;
        for (uint8_t i = 0; i < lineCount; i++) {
            int16_t lw = tft.textWidth(lines[i]);
            if (lw > widestLineW) widestLineW = lw;
        }
        LayoutCheck::bufferRect("Statuszeile-Puffer", Config::SCREEN_WIDTH, spriteH, widestLineW, (int16_t)(pitch * lineCount));
    }
    // EIN Rechteck fuer Tipp-Flaeche UND Text (siehe battery_page.cpp -
    // zwei separate, sich zwangslaeufig ueberlappende Rechtecke waeren ein
    // Falsch-Konflikt).
    Rect tapZone = statusTapZoneFor(areaTop);
    int16_t textBottom = (int16_t)(statusY + pitch * lineCount);
    int16_t combinedBottom = textBottom > (tapZone.y + tapZone.h) ? textBottom : (int16_t)(tapZone.y + tapZone.h);
    LayoutCheck::rect("Statuszeile", tapZone.x, tapZone.y, tapZone.w, (int16_t)(combinedBottom - tapZone.y));
}

bool update() {
    SolarData::Data snap = SolarData::snapshot();
    struct tm t;
    int8_t minute = getLocalTime(&t, 5) ? (int8_t)t.tm_min : (int8_t)-1;

    bool solarChanged = snap.changeCounter != lastSeenChangeCounter;
    bool minuteTicked = minute != lastSeenMinute;
    bool feedbackActive = feedbackState.update(snap);
    bool changed = firstCheck || solarChanged || minuteTicked || feedbackActive;
    if (changed) {
        if (firstCheck) RedrawDiag::log("PV-Status", "erster Aufruf");
        if (solarChanged) RedrawDiag::log("PV-Status", "SolarData-Zaehler geaendert");
        if (minuteTicked) RedrawDiag::log("PV-Status", "Minutenwechsel");
        firstCheck = false;
        lastSeenChangeCounter = snap.changeCounter;
        lastSeenMinute = minute;
    }
    return changed;
}

bool onTap(int16_t x, int16_t y, int16_t areaTop, int16_t /*areaHeight*/) {
    int16_t tileW = tileWidth();
    int16_t x0 = TILE_GAP;
    int16_t x1 = (int16_t)(x0 + tileW + TILE_GAP);
    int16_t y0 = gridTopFor(areaTop);
    int16_t y1 = (int16_t)(y0 + TILE_H + TILE_GAP);
    Rect tiles[4] = {
        {x0, y0, tileW, TILE_H}, {x1, y0, tileW, TILE_H},
        {x0, y1, tileW, TILE_H}, {x1, y1, tileW, TILE_H},
    };
    // Welcher Datenpunkt aktiv ausgelesen wird, je angetippter Kachel
    // (Auftrag "Aktiv-Auslesen", Korrektur B3) - Index 3 ("Max heute")
    // ist ein lokal aus der Tageskurve berechneter Wert, am Portal NICHT
    // einzeln lesbar (wie "Tagesertrag", siehe Auftrag) - bleibt deshalb
    // bei 0 Datenpunkten, faellt auf den normalen passiven Refresh zurueck.
    constexpr int32_t kTileDataPointId[4] = {
        EpeverDatapoints::PV_VOLTAGE, EpeverDatapoints::PV_CURRENT,
        EpeverDatapoints::PV_POWER, 0,
    };
    for (uint8_t i = 0; i < 4; i++) {
        if (tiles[i].contains(x, y)) {
            lastTappedTile = (int8_t)i;
            FetchTask::RefreshRequest req;
            if (kTileDataPointId[i] != 0) {
                req.dataPointCount = 1;
                req.dataPointIds[0] = kTileDataPointId[i];
            }
            FetchTask::sendRefreshNow(req);
            return true;
        }
    }
    return false;
}

}
