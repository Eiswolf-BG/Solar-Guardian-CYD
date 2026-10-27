#include "home_page.h"
#include "display.h"
#include "solar_data.h"
#include "weather_data.h"
#include "weather_icons.h"
#include "yield_ledger.h"
#include "app_config.h"
#include "layout_utils.h"
#include "layout_check.h"
#include "tile_sprite.h"
#include "redraw_diag.h"
#include "refresh_feedback_text.h"
#include "page_nav.h"
#include "fetch_task.h"
#include "week_view_screen.h"
#include "location_screen.h"
#include "i18n.h"
#include "config.h"
#include <math.h>

namespace HomePage {

namespace {
    // 2x2 Kacheln, gleich gross, gleichmaessiger Abstand zueinander und
    // zum Rand (Auftrag) - keine eigene Titelzeile, der komplette Bereich
    // zwischen Kopf- und Fusszeile gehoert den Kacheln.
    constexpr int16_t MARGIN = 8;
    constexpr int16_t GAP = 8;

    uint32_t lastSeenSolarCounter = 0xFFFFFFFFu;
    uint32_t lastSeenYieldRevision = 0xFFFFFFFFu;
    uint32_t lastSeenWeatherCounter = 0xFFFFFFFFu;
    bool firstCheck = true;

    // Pro-Kachel-Inhaltssignatur (Auftrag "Flackern, Teil 2" - Seiten
    // sollen den ANGEZEIGTEN Text mit dem letzten Zeichnen vergleichen,
    // nicht nur den globalen changeCounter) - jede Kachel wird NUR
    // tatsaechlich neu gezeichnet (und damit ueberhaupt erst ans
    // Off-Screen-Sprite geschickt), wenn sich ihr eigener Inhalt wirklich
    // geaendert hat, selbst wenn update() wegen einer ANDEREN Kachel
    // true zurueckgab.
    String lastWeatherSig = "\x01";
    String lastPowerSig = "\x01";
    String lastTodaySig = "\x01";
    String lastBatterySig = "\x01";

    // Rueckmeldung zum manuellen Refresh (Auftrag "Refresh-Regeln",
    // korrigiert durch Auftrag "Fehler in der Refresh-Rueckmeldung") -
    // die Uebersicht hat keine eigene Statuszeile wie die anderen Seiten,
    // die Rueckmeldung ersetzt deshalb kurzzeitig NUR DIE BESCHRIFTUNGS-
    // ZEILE der tatsaechlich angetippten Kachel (Leistung/Ertrag/
    // Batterie), siehe battery_page.cpp fuer die genaue Funktionsweise
    // von FeedbackState. lastTappedTile (0=Leistung,1=Ertrag,2=Batterie,
    // -1=keine) legt fest, WELCHE Kachel die Rueckmeldung zeigt - EINE
    // geteilte FeedbackState-Instanz treibt nur Text/Zeitsteuerung,
    // welche Kachel sie anzeigt ist eine reine Zeichen-Entscheidung
    // (gleiches Prinzip wie pv_status_page.cpp). Vorher hatten alle drei
    // Kacheln DENSELBEN Zustand ausgewertet, wodurch ein Tipp auf nur
    // eine Kachel faelschlich alle drei umschaltete (Auftrag, Punkt 1).
    RefreshFeedbackText::FeedbackState feedbackState;
    int8_t lastTappedTile = -1;

    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    struct Layout {
        Rect weather, power, today, battery;
    };

    Layout computeLayout(int16_t areaTop, int16_t /*areaHeight*/) {
        // WICHTIG: wie alle anderen Seiten von SCREEN_HEIGHT ausgehen,
        // NICHT von areaTop+areaHeight - der "?"-Knopf (main_screen.cpp)
        // reicht bewusst etwas in den Seitenbereich hinein (bis knapp vor
        // die Fusszone), siehe Config::PAGE_FOOTER_RESERVED_H. Ohne diese
        // Grenze ragte die untere Kachelreihe zuvor hinter den Knopf
        // (vom Layout-Pruefer gemeldeter echter Konflikt).
        int16_t contentBottom = (int16_t)(Config::SCREEN_HEIGHT - Config::PAGE_FOOTER_RESERVED_H - 8);
        int16_t tileW = (int16_t)((Config::SCREEN_WIDTH - 2 * MARGIN - GAP) / 2);
        int16_t topY = (int16_t)(areaTop + MARGIN);
        int16_t tileH = (int16_t)((contentBottom - topY - GAP) / 2);
        int16_t leftX = MARGIN;
        int16_t rightX = (int16_t)(leftX + tileW + GAP);
        int16_t bottomY = (int16_t)(topY + tileH + GAP);
        Layout L;
        L.weather = {leftX, topY, tileW, tileH};
        L.power = {rightX, topY, tileW, tileH};
        L.today = {leftX, bottomY, tileW, tileH};
        L.battery = {rightX, bottomY, tileW, tileH};
        return L;
    }

    // Misst das tatsaechlich gezeichnete Rechteck eines Textes bei der
    // gegebenen Groesse - fuer LayoutCheck::containedRect(), gleiches
    // Prinzip wie power_page.cpp/battery_page.cpp (hier bewusst lokal
    // dupliziert, siehe CLAUDE.md Code-Stil).
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

    // Wie LayoutUtils::fitText(), aber OHNE "..."-Kuerzung - Zahlen werden
    // nie gekuerzt (Auftrag, siehe CLAUDE.md Layout-Regel), gleiches
    // Prinzip wie in den anderen Seiten.
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

    void formatDecimal(float value, uint8_t decimals, char* out, size_t outSize) {
        char buf[20];
        snprintf(buf, sizeof(buf), "%.*f", decimals, (double)value);
        if (appConfig.language == 0) {
            for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
        }
        strncpy(out, buf, outSize - 1);
        out[outSize - 1] = 0;
    }

    // Unter 1000 W in W, ab 1000 W in kW mit 2 Nachkommastellen - gleiche
    // Regel/Quelle wie power_page.cpp (Auftrag). Gibt NUR die Zahl zurueck
    // (Einheit getrennt, siehe drawLabelValueTile()-Kommentar), outUnit
    // zeigt danach auf den passenden i18n-Text.
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

    const uint8_t VALUE_SIZES[] = {2, 1};

    // Alle folgenden render*()-Funktionen zeichnen in LOKALEN Koordinaten
    // (0,0 = Kachel-Oberkante-links) auf die uebergebene Zeichenflaeche
    // "t" - das ist entweder der echte Bildschirm ODER ein temporaerer
    // Off-Screen-Puffer (siehe TileSprite::drawInto(), Auftrag
    // "Flackerfrei zeichnen"/CLAUDE.md). "absX"/"absY" sind NUR fuer die
    // LayoutCheck-Registrierung (die arbeitet mit absoluten
    // Bildschirmkoordinaten) noetig, beeinflussen das Zeichnen selbst
    // nicht.

    // Beschriftung oben, darunter die ZAHL ALLEIN gross+zentriert, dann
    // die Einheit klein darunter - Zahl und Einheit bewusst GETRENNT
    // (nicht wie vorher ein gemeinsamer String), weil eine laengere
    // Einheit (z.B. "kWh" bei "Heute") den fitNumberNoTruncate()-
    // Automaten sonst auf eine kleinere Schriftgroesse zwang als bei
    // kurzen Einheiten ("W"/"kW") - die Kacheln wirkten dadurch
    // uneinheitlich gross (Auftrag "Kacheln einheitlich machen").
    // labelOverride: leer = normale Beschriftung (label) zeichnen: nicht
    // leer = diesen Text STATT der Beschriftung zeigen (Refresh-
    // Rueckmeldung, Auftrag "Fehler in der Refresh-Rueckmeldung") - der
    // Wert darunter wird in JEDEM Fall normal weitergezeichnet, damit er
    // nie verschwindet (Auftrag, Punkt 2).
    void renderLabelValueTile(TFT_eSPI& t, int16_t w, int16_t h, int16_t absX, int16_t absY,
                               const char* label, const String& numberStr, const char* unit,
                               bool valid, uint16_t accent, const String& labelOverride = String()) {
        t.drawRoundRect(0, 0, w, h, 4, accent);
        LayoutCheck::rect("Kachel-Rahmen", absX, absY, w, h);

        t.setTextSize(1);
        t.setTextDatum(TC_DATUM);
        t.setTextColor(accent, TFT_BLACK);
        int16_t labelTopY = 8;
        String labelText = labelOverride.length() > 0 ? labelOverride : String(label);
        t.drawString(labelText, (int16_t)(w / 2), labelTopY);
        t.setTextDatum(TL_DATUM);
        {
            Measured lr = measureTopCentered(t, (int16_t)(w / 2), labelTopY, labelText, 1);
            LayoutCheck::containedRect("Kachel-Beschriftung", absX, absY, w, h,
                                        (int16_t)(absX + lr.x), (int16_t)(absY + lr.y), lr.w, lr.h);
        }

        constexpr int16_t LABEL_H = 22;
        constexpr int16_t UNIT_H = 20;
        int16_t valueZoneTop = LABEL_H;
        int16_t valueZoneH = (int16_t)(h - LABEL_H - UNIT_H - 4);
        uint16_t valColor = Display::valueColor(valid, accent);
        t.setTextColor(valColor, TFT_BLACK);
        LayoutUtils::FitResult fit = fitNumberNoTruncate(t, numberStr, w, valueZoneH, VALUE_SIZES, 2);
        int16_t valueCY = (int16_t)(valueZoneTop + valueZoneH / 2);
        t.setTextDatum(MC_DATUM);
        t.drawString(fit.text, (int16_t)(w / 2), valueCY);
        t.setTextDatum(TL_DATUM);
        {
            Measured vr = measureCentered(t, (int16_t)(w / 2), valueCY, fit.text, fit.textSize);
            LayoutCheck::containedRect("Kachel-Wert", absX, absY, w, h,
                                        (int16_t)(absX + vr.x), (int16_t)(absY + vr.y), vr.w, vr.h);
        }

        t.setTextSize(1);
        // Einheit IMMER in der Systemfarbe, unabhaengig vom Wert (Auftrag
        // "Farbregeln vereinheitlichen" - Einheiten bleiben systemfarben,
        // nur der abgefragte Zahlenwert selbst ist sekundaer).
        t.setTextColor(accent, TFT_BLACK);
        int16_t unitLineH = t.fontHeight();
        int16_t unitCY = (int16_t)(h - 6 - unitLineH / 2);
        t.setTextDatum(MC_DATUM);
        t.drawString(unit, (int16_t)(w / 2), unitCY);
        t.setTextDatum(TL_DATUM);
        t.setTextColor(accent, TFT_BLACK);
        {
            Measured ur = measureCentered(t, (int16_t)(w / 2), unitCY, unit, 1);
            LayoutCheck::containedRect("Kachel-Einheit", absX, absY, w, h,
                                        (int16_t)(absX + ur.x), (int16_t)(absY + ur.y), ur.w, ur.h);
        }
    }

    // Kleines Batteriesymbol mit Fuellstand - eigene, einfache Zeichnung
    // (keine Bilddatei), Farbe nach Auftrag: ab 50% gruen, 20-49% gelb,
    // unter 20% rot. cx/cy lokal (relativ zur Zeichenflaeche "t").
    uint16_t batteryColorFor(float pct) {
        if (pct >= 50.0f) return TFT_GREEN;
        if (pct >= 20.0f) return TFT_ORANGE;
        return TFT_RED;
    }

    void drawBatteryIcon(TFT_eSPI& t, int16_t cx, int16_t cy, int16_t w, int16_t h, float pct, bool valid) {
        int16_t bodyW = (int16_t)(w - 3);
        int16_t capW = 3, capH = (int16_t)(h / 3);
        int16_t x = (int16_t)(cx - w / 2);
        int16_t y = (int16_t)(cy - h / 2);
        uint16_t color = valid ? batteryColorFor(pct) : Display::secondaryColor();
        t.drawRoundRect(x, y, bodyW, h, 2, color);
        t.fillRect((int16_t)(x + bodyW), (int16_t)(cy - capH / 2), capW, capH, color);
        if (valid) {
            float frac = pct / 100.0f;
            if (frac < 0.0f) frac = 0.0f;
            if (frac > 1.0f) frac = 1.0f;
            int16_t innerPad = 2;
            int16_t fillW = (int16_t)((bodyW - 2 * innerPad) * frac);
            if (fillW > 0) {
                t.fillRect((int16_t)(x + innerPad), (int16_t)(y + innerPad), fillW, (int16_t)(h - 2 * innerPad), color);
            }
        }
    }

    // labelOverride: siehe renderLabelValueTile() - Wert/Symbol bleiben
    // in JEDEM Fall sichtbar, nur die Beschriftung wird ersetzt.
    void renderBatteryTile(TFT_eSPI& t, int16_t w, int16_t h, int16_t absX, int16_t absY,
                            const SolarData::Data& snap, uint16_t accent, const String& labelOverride = String()) {
        t.drawRoundRect(0, 0, w, h, 4, accent);
        LayoutCheck::rect("Kachel-Rahmen", absX, absY, w, h);

        t.setTextSize(1);
        t.setTextDatum(TC_DATUM);
        t.setTextColor(accent, TFT_BLACK);
        int16_t labelTopY = 8;
        String label = labelOverride.length() > 0 ? labelOverride : String(I18n::t(I18n::Str::PAGE_BATTERY_TITLE));
        t.drawString(label, (int16_t)(w / 2), labelTopY);
        t.setTextDatum(TL_DATUM);
        {
            Measured lr = measureTopCentered(t, (int16_t)(w / 2), labelTopY, label, 1);
            LayoutCheck::containedRect("Kachel-Beschriftung", absX, absY, w, h,
                                        (int16_t)(absX + lr.x), (int16_t)(absY + lr.y), lr.w, lr.h);
        }

        bool valid = snap.batteryValid;
        char pctStr[8];   // nur fuer die Groessenberechnung (inkl. "%")
        char pctNum[8];   // Zahlenteil allein, Sekundaerfarbe
        if (valid) {
            snprintf(pctStr, sizeof(pctStr), "%.0f %%", (double)snap.batterySocPercent);
            snprintf(pctNum, sizeof(pctNum), "%.0f", (double)snap.batterySocPercent);
        } else {
            strncpy(pctStr, "--", sizeof(pctStr));
            strncpy(pctNum, "--", sizeof(pctNum));
        }

        constexpr int16_t LABEL_H = 22;
        int16_t valueZoneTop = LABEL_H;
        int16_t valueZoneH = (int16_t)(h - LABEL_H - 6);
        int16_t valueCY = (int16_t)(valueZoneTop + valueZoneH / 2);

        LayoutUtils::FitResult fit = fitNumberNoTruncate(t, pctStr, (int16_t)(w - 26), valueZoneH, VALUE_SIZES, 2);
        t.setTextSize(fit.textSize);
        int16_t textW = t.textWidth(pctStr);
        int16_t iconW = 20, iconH = 11;
        int16_t totalW = (int16_t)(iconW + 6 + textW);
        int16_t startX = (int16_t)(w / 2 - totalW / 2);

        drawBatteryIcon(t, (int16_t)(startX + iconW / 2), valueCY, iconW, iconH, snap.batterySocPercent, valid);
        // Zahlenwert in Sekundaerfarbe, Einheit "%" in Systemfarbe (Auftrag
        // "Farbregeln vereinheitlichen").
        t.setTextDatum(ML_DATUM);
        t.setTextColor(Display::valueColor(valid, accent), TFT_BLACK);
        t.drawString(pctNum, (int16_t)(startX + iconW + 6), valueCY);
        int16_t numW = t.textWidth(pctNum);
        t.setTextColor(accent, TFT_BLACK);
        t.drawString(" %", (int16_t)(startX + iconW + 6 + numW), valueCY);
        t.setTextDatum(TL_DATUM);
        {
            int16_t th = t.fontHeight();
            LayoutCheck::containedRect("Kachel-Wert", absX, absY, w, h,
                                        (int16_t)(absX + startX), (int16_t)(absY + valueCY - th / 2), totalW, th);
        }
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

    // Zentrierte Hinweis-Nachricht (bis zu 2 Zeilen), fuer "Standort
    // waehlen"/"Wetter wird geladen" - nichts wird abgeschnitten (Auftrag):
    // groesste Groesse probieren, die in <=2 Zeilen UND die Kachelhoehe
    // passt, sonst die naechstkleinere.
    void renderWrappedMessage(TFT_eSPI& t, int16_t w, int16_t h, int16_t absX, int16_t absY,
                               const char* msg, uint16_t accent) {
        constexpr uint8_t sizes[] = {2, 1};
        constexpr uint8_t MAX_LINES = 2;
        String lines[MAX_LINES];
        uint8_t n = 0;
        uint8_t chosenSize = 1;
        int16_t lineH = 0;
        for (uint8_t s : sizes) {
            t.setTextSize(s);
            lineH = t.fontHeight();
            String tmp[MAX_LINES];
            uint8_t tn = wrapWords(t, msg, (int16_t)(w - 16), tmp, MAX_LINES);
            String rebuilt = tmp[0];
            for (uint8_t i = 1; i < tn; i++) { rebuilt += " "; rebuilt += tmp[i]; }
            bool completelyWrapped = (rebuilt == String(msg)) || tn == 0;
            if (completelyWrapped && tn * lineH <= (int16_t)(h - 16)) {
                for (uint8_t i = 0; i < tn; i++) lines[i] = tmp[i];
                n = tn;
                chosenSize = s;
                break;
            }
        }
        if (n == 0) {
            t.setTextSize(1);
            lineH = t.fontHeight();
            n = wrapWords(t, msg, (int16_t)(w - 16), lines, MAX_LINES);
            chosenSize = 1;
        }
        t.setTextSize(chosenSize);
        t.setTextColor(accent, TFT_BLACK);
        t.setTextDatum(MC_DATUM);
        int16_t totalH = (int16_t)(n * lineH);
        int16_t top = (int16_t)(h / 2 - totalH / 2 + lineH / 2);
        int16_t maxLineW = 0;
        for (uint8_t i = 0; i < n; i++) {
            t.drawString(lines[i], (int16_t)(w / 2), (int16_t)(top + i * lineH));
            int16_t lw = t.textWidth(lines[i]);
            if (lw > maxLineW) maxLineW = lw;
        }
        t.setTextDatum(TL_DATUM);
        LayoutCheck::containedRect("Kachel-Hinweistext", absX, absY, w, h,
                                    (int16_t)(absX + w / 2 - maxLineW / 2), (int16_t)(absY + top - lineH / 2), maxLineW, totalH);
    }

    void renderWeatherTile(TFT_eSPI& t, int16_t w, int16_t h, int16_t absX, int16_t absY,
                            const WeatherData::Data& weather, uint16_t accent) {
        t.drawRoundRect(0, 0, w, h, 4, accent);
        LayoutCheck::rect("Kachel-Rahmen", absX, absY, w, h);

        if (!weather.locationSet) {
            renderWrappedMessage(t, w, h, absX, absY, I18n::t(I18n::Str::HOME_WEATHER_SET_LOCATION), accent);
            return;
        }
        if (!weather.currentValid) {
            renderWrappedMessage(t, w, h, absX, absY, I18n::t(I18n::Str::HOME_WEATHER_LOADING), accent);
            return;
        }

        // Icon darf groesser werden, solange es Rahmen/Text nicht beruehrt
        // (Auftrag) - 0.62 der kleineren Kachelseite, rechnerisch gegen
        // Beschriftungs-/Temperaturzone geprueft.
        int16_t iconSize = (int16_t)((w < h ? w : h) * 0.62f);
        int16_t iconCy = (int16_t)(h * 0.4f);
        int16_t iconCx = (int16_t)(w / 2);
        WeatherIcons::draw(t, iconCx, iconCy, iconSize, weather.currentWeatherCode, weather.isDay);
        LayoutCheck::containedRect("Kachel-Symbol", absX, absY, w, h,
                                    (int16_t)(absX + iconCx - iconSize / 2), (int16_t)(absY + iconCy - iconSize / 2),
                                    iconSize, iconSize);

        // Temperatur wieder anzeigen (Auftrag) - jetzt ueber MET Norway
        // verfeinert, siehe fetch_task.cpp::runWeatherFetch().
        char tempStr[16];
        formatDecimal(weather.currentTempC, 1, tempStr, sizeof(tempStr));
        String unitStr = String(" ") + I18n::t(I18n::Str::CELSIUS_UNIT);
        String tempLine = String(tempStr) + unitStr;

        constexpr int16_t TEMP_ZONE_H = 22;
        int16_t tempZoneTop = (int16_t)(h - TEMP_ZONE_H - 4);
        // Groesse anhand der Gesamtlaenge (Zahl+Einheit) bestimmen, dann
        // als zwei Farbsegmente nebeneinander zeichnen: Zahlenwert
        // sekundaer, Einheit "°C" systemfarben (Auftrag "Farbregeln
        // vereinheitlichen").
        LayoutUtils::FitResult fit = fitNumberNoTruncate(t, tempLine, w, TEMP_ZONE_H, VALUE_SIZES, 2);
        t.setTextSize(fit.textSize);
        int16_t numW = t.textWidth(tempStr);
        int16_t totalW = t.textWidth(tempLine);
        int16_t tempCY = (int16_t)(tempZoneTop + TEMP_ZONE_H / 2);
        int16_t startX = (int16_t)(w / 2 - totalW / 2);
        t.setTextDatum(ML_DATUM);
        t.setTextColor(Display::valueColor(weather.currentValid, accent), TFT_BLACK);
        t.drawString(tempStr, startX, tempCY);
        t.setTextColor(accent, TFT_BLACK);
        t.drawString(unitStr, (int16_t)(startX + numW), tempCY);
        t.setTextDatum(TL_DATUM);
        {
            int16_t th = t.fontHeight();
            LayoutCheck::containedRect("Kachel-Temperatur", absX, absY, w, h,
                                        (int16_t)(absX + startX), (int16_t)(absY + tempCY - th / 2), totalW, th);
        }
    }

    // Baut die Inhaltssignatur der Wetterkachel - alles, was renderWeatherTile()
    // tatsaechlich sichtbar beeinflusst (Auftrag "nur bei Aenderung neu
    // zeichnen", pro Kachel statt nur global).
    String weatherSignature(const WeatherData::Data& w) {
        if (!w.locationSet) return "L0";
        if (!w.currentValid) return "L1";
        char temp[16];
        formatDecimal(w.currentTempC, 1, temp, sizeof(temp));
        char buf[40];
        snprintf(buf, sizeof(buf), "W%d,%d,", w.currentWeatherCode, w.isDay ? 1 : 0);
        return String(buf) + temp;
    }
}

void draw(int16_t areaTop, int16_t areaHeight, bool forceRedraw) {
    SolarData::Data snap = SolarData::snapshot();
    WeatherData::Data weather = WeatherData::snapshot();
    YieldLedger::Values yield = YieldLedger::current();
    uint16_t accent = Display::systemColor();

    // Seiteneintritt (main_screen.cpp hat den Bereich schwarz geloescht) -
    // Inhalts-Diff-Caches zuruecksetzen, sonst wuerden unveraenderte
    // Kacheln nach einem Seitenwechsel faelschlich leer bleiben (siehe
    // pages.h-Kommentar).
    if (forceRedraw) {
        lastWeatherSig = "\x01";
        lastPowerSig = "\x01";
        lastTodaySig = "\x01";
        lastBatterySig = "\x01";
    }

    Layout L = computeLayout(areaTop, areaHeight);

    // Jede Kachel bekommt ihre eigene Inhaltssignatur - nur bei
    // tatsaechlicher Aenderung wird ueberhaupt gezeichnet (Auftrag
    // "die Seiten vergleichen die angezeigten Werte als Text mit denen
    // vom letzten Zeichnen"). Das Zeichnen selbst laeuft ueber
    // TileSprite::drawInto() in einen temporaeren Off-Screen-Puffer,
    // der per EINEM pushSprite() auf den Bildschirm geschoben wird -
    // nie eine sichtbare Luecke, egal wie oft (Auftrag "Flackerfrei
    // zeichnen").
    String weatherSig = weatherSignature(weather);
    if (weatherSig != lastWeatherSig) {
        lastWeatherSig = weatherSig;
        RedrawDiag::log("Uebersicht-Wetter", "Inhalt geaendert");
        TileSprite::drawInto(L.weather.x, L.weather.y, L.weather.w, L.weather.h, [&](TFT_eSPI& t) {
            renderWeatherTile(t, L.weather.w, L.weather.h, L.weather.x, L.weather.y, weather, accent);
        });
    }

    // Refresh-Rueckmeldung (Auftrag "Fehler in der Refresh-Rueckmeldung")
    // - erscheint NUR in der tatsaechlich angetippten Kachel
    // (lastTappedTile), ersetzt dort NUR die Beschriftungszeile (Wert
    // bleibt immer sichtbar, Punkt 2) und wird vorher gegen die
    // Kachelbreite gemessen (Punkt 2, Kurzfassung bei Bedarf).
    // Rueckgabewert von update() nur fuer "jetzt neu zeichnen" (hier
    // bereits durch draw() implizit erfuellt) - die ANZEIGE-Entscheidung
    // kommt aus feedbackState.active (siehe refresh_feedback_text.h,
    // Auftrag "Rueckmeldung bleibt dauerhaft stehen").
    feedbackState.update(snap);
    tft.setTextSize(1); // fuer die Breitenmessung unten (Schrift explizit setzen)
    bool showOnPower = feedbackState.active && lastTappedTile == 0;
    bool showOnToday = feedbackState.active && lastTappedTile == 1;
    bool showOnBattery = feedbackState.active && lastTappedTile == 2;
    String powerLabelOverride = showOnPower ? RefreshFeedbackText::fitToWidth(tft, feedbackState.text, feedbackState.shortText, (int16_t)(L.power.w - 8)) : String();
    String todayLabelOverride = showOnToday ? RefreshFeedbackText::fitToWidth(tft, feedbackState.text, feedbackState.shortText, (int16_t)(L.today.w - 8)) : String();
    String batteryLabelOverride = showOnBattery ? RefreshFeedbackText::fitToWidth(tft, feedbackState.text, feedbackState.shortText, (int16_t)(L.battery.w - 8)) : String();

    char powerStr[16];
    const char* powerUnit = I18n::t(I18n::Str::POWER_UNIT_W);
    if (snap.powerValid) formatPower(snap.currentPowerW, powerStr, sizeof(powerStr), &powerUnit);
    else strncpy(powerStr, "--", sizeof(powerStr));
    String powerSig = (showOnPower ? (String("F") + powerLabelOverride) : String()) + "|" + powerStr + "|" + powerUnit + (snap.powerValid ? "1" : "0");
    if (powerSig != lastPowerSig) {
        lastPowerSig = powerSig;
        RedrawDiag::log("Uebersicht-Leistung", "Inhalt geaendert");
        TileSprite::drawInto(L.power.x, L.power.y, L.power.w, L.power.h, [&](TFT_eSPI& t) {
            renderLabelValueTile(t, L.power.w, L.power.h, L.power.x, L.power.y,
                                  I18n::t(I18n::Str::PAGE_POWER_TITLE), String(powerStr), powerUnit, snap.powerValid, accent, powerLabelOverride);
        });
    }

    char todayStr[16];
    if (yield.valid) formatDecimal(yield.todayKwh, 2, todayStr, sizeof(todayStr));
    else strncpy(todayStr, "--", sizeof(todayStr));
    String todaySig = (showOnToday ? (String("F") + todayLabelOverride) : String()) + "|" + todayStr + (yield.valid ? "1" : "0");
    if (todaySig != lastTodaySig) {
        lastTodaySig = todaySig;
        RedrawDiag::log("Uebersicht-Ertrag", "Inhalt geaendert");
        TileSprite::drawInto(L.today.x, L.today.y, L.today.w, L.today.h, [&](TFT_eSPI& t) {
            renderLabelValueTile(t, L.today.w, L.today.h, L.today.x, L.today.y,
                                  I18n::t(I18n::Str::YIELD_TILE_TODAY), String(todayStr), I18n::t(I18n::Str::KWH_UNIT), yield.valid, accent, todayLabelOverride);
        });
    }

    char batterySigBuf[8];
    if (snap.batteryValid) snprintf(batterySigBuf, sizeof(batterySigBuf), "%.0f", (double)snap.batterySocPercent);
    else strncpy(batterySigBuf, "--", sizeof(batterySigBuf));
    String batterySig = (showOnBattery ? (String("F") + batteryLabelOverride) : String()) + "|" + batterySigBuf + (snap.batteryValid ? "1" : "0");
    if (batterySig != lastBatterySig) {
        lastBatterySig = batterySig;
        RedrawDiag::log("Uebersicht-Batterie", "Inhalt geaendert");
        TileSprite::drawInto(L.battery.x, L.battery.y, L.battery.w, L.battery.h, [&](TFT_eSPI& t) {
            renderBatteryTile(t, L.battery.w, L.battery.h, L.battery.x, L.battery.y, snap, accent, batteryLabelOverride);
        });
    }

    // Hauptscreen setzt seinen Zustand am Ende zurueck (Auftrag) - sonst
    // erbt die naechste Zeichenfunktion (Kopfzeile/Menue) die hier zuletzt
    // benutzte Groesse/Ausrichtung (war die Ursache des grossen/
    // ineinanderlaufenden Kopfzeilen-/Menue-Bugs, siehe CLAUDE.md
    // "Schriftzustand").
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(accent, TFT_BLACK);
}

bool update() {
    SolarData::Data snap = SolarData::snapshot();
    uint32_t yieldRev = YieldLedger::revision();
    uint32_t weatherCounter = WeatherData::snapshot().changeCounter;

    bool solarChanged = snap.changeCounter != lastSeenSolarCounter;
    bool yieldChanged = yieldRev != lastSeenYieldRevision;
    bool weatherChanged = weatherCounter != lastSeenWeatherCounter;
    bool feedbackActive = feedbackState.update(snap);
    bool changed = firstCheck || solarChanged || yieldChanged || weatherChanged || feedbackActive;
    if (changed) {
        if (firstCheck) RedrawDiag::log("Uebersicht", "erster Aufruf");
        if (solarChanged) RedrawDiag::log("Uebersicht", "SolarData-Zaehler geaendert");
        if (yieldChanged) RedrawDiag::log("Uebersicht", "YieldLedger-Zaehler geaendert");
        if (weatherChanged) RedrawDiag::log("Uebersicht", "WeatherData-Zaehler geaendert");
        firstCheck = false;
        lastSeenSolarCounter = snap.changeCounter;
        lastSeenYieldRevision = yieldRev;
        lastSeenWeatherCounter = weatherCounter;
    }
    return changed;
}

bool onTap(int16_t x, int16_t y, int16_t areaTop, int16_t areaHeight) {
    Layout L = computeLayout(areaTop, areaHeight);
    if (L.weather.contains(x, y)) {
        // Ohne Standort oeffnet die Kachel direkt den Standort-Screen,
        // mit Standort die Wochenansicht (die selbst "Zum Aendern
        // antippen" zum Standort-Screen anbietet) - Auftrag "Standort
        // automatisch, notfalls einmalig per Eingabe".
        if (WeatherData::snapshot().locationSet) WeekViewScreen::run();
        else LocationScreen::run();
        return true;
    }
    // Tippen auf Leistung/Ertrag/Batterie loest den manuellen Refresh aus
    // (Auftrag "manueller Refresh") statt wie urspruenglich auf die
    // jeweilige Wisch-Seite zu springen. Navigation zu diesen Seiten
    // laeuft weiterhin ganz normal per Wischen. Wetter-Kachel bleibt
    // davon unberuehrt (siehe oben). lastTappedTile wird VOR dem Senden
    // gesetzt, damit die Rueckmeldung (naechster draw()-Aufruf) garantiert
    // nur in DIESER Kachel erscheint (Auftrag "Fehler in der Refresh-
    // Rueckmeldung", Punkt 1).
    if (L.power.contains(x, y)) {
        lastTappedTile = 0;
        FetchTask::sendCommand(FetchTask::Command::RefreshNow);
        return true;
    }
    if (L.today.contains(x, y)) {
        lastTappedTile = 1;
        FetchTask::sendCommand(FetchTask::Command::RefreshNow);
        return true;
    }
    if (L.battery.contains(x, y)) {
        lastTappedTile = 2;
        FetchTask::sendCommand(FetchTask::Command::RefreshNow);
        return true;
    }
    return false;
}

bool onLongPress(int16_t x, int16_t y, int16_t areaTop, int16_t areaHeight) {
    Layout L = computeLayout(areaTop, areaHeight);
    if (L.weather.contains(x, y)) {
        // Immer den Standort-Screen oeffnen, unabhaengig davon, ob schon
        // ein Standort bekannt ist (Auftrag "Standort jederzeit neu
        // einstellen koennen") - anders als der kurze Tipp (onTap()),
        // der bei bekanntem Standort stattdessen die Wochenansicht zeigt.
        LocationScreen::run();
        return true;
    }
    return false;
}

}
