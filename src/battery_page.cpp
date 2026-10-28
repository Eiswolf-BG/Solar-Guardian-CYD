#include "battery_page.h"
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

namespace BatteryPage {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    // Kein Balkensymbol mehr unter der SOC-Zahl (entfernt, um Platz fuer
    // die neue Erklaerungszeile zu schaffen, siehe computeLayout()-
    // Kommentar unten) - die grosse Prozentzahl bleibt der primaere
    // Ladezustands-Indikator.
    constexpr int16_t SOC_Y_OFFSET = 20;
    constexpr int16_t SOC_ZONE_H = 26; // 17 (fontHeight Size1) + 2*4px Pad, siehe fitText()
    constexpr int16_t GRID_TOP_OFFSET = 48;
    constexpr int16_t TILE_GAP = 2;
    // Mind. 50px, damit selbst bei Size 1 noch 17+2*4=25px Zonenhoehe fuer
    // Zahlen bleibt (LABEL_H+UNIT_H=20) - siehe CLAUDE.md Layout-Regel
    // "Zahlen werden nie gekuerzt". Bei TILE_H=46 (fruehere Version)
    // scheiterte das knapp und fitText() kuerzte Volt/Strom/Temperatur
    // trotz kurzer Werte mit "...", weil schon die Hoehe nicht reichte.
    constexpr int16_t TILE_H = 50;
    constexpr uint8_t VALUE_SIZES[] = {2, 1};
    constexpr uint8_t SOC_SIZES[] = {3, 2, 1};
    // Auf 1 Zeile reduziert (Auftrag "Max/Min-Spannungskacheln" - Platz
    // fuer die dritte Kachelreihe, Erklaertexte entsprechend gekuerzt,
    // siehe i18n BATTERY_EXPLAIN_*).
    constexpr uint8_t EXPLAIN_MAX_LINES = 1;
    constexpr int16_t EXPLAIN_LINE_H = 12; // etwas enger als fontHeight(17)
    // Dritte, flache Kachelreihe (Max./Min. Spannung) - eigene, kleinere
    // Hoehe als die vier grossen Kacheln (TILE_H=50 bleibt unveraendert,
    // siehe Auftrag Punkt 3).
    constexpr int16_t FLAT_TILE_H = 28;

    uint32_t lastSeenChangeCounter = 0xFFFFFFFFu;
    int8_t lastSeenMinute = -1;
    bool firstCheck = true;

    // Inhaltssignaturen pro dynamischem Bereich (Auftrag "Flackern, Teil 2").
    String lastSocSig = "\x01";
    String lastVoltSig = "\x01";
    String lastCurrSig = "\x01";
    String lastTempSig = "\x01";
    String lastStatusTileSig = "\x01";
    String lastExplainSig = "\x01";
    String lastTimestampSig = "\x01";
    String lastMaxMinSig = "\x01"; // Max/Min-Spannungskacheln

    // Rueckmeldung zum manuellen Refresh (Auftrag "Refresh-Regeln") -
    // ersetzt kurzzeitig die normale Statuszeile, siehe
    // refresh_feedback_text.h. Dieselbe Instanz wird von update() (erkennt
    // neue Ereignisse, erzwingt einen draw()-Aufruf) UND draw() (liest den
    // aktuellen Text) verwendet.
    RefreshFeedbackText::FeedbackState feedbackState;

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

    // "Schnellladen"/"Ausgleich" meldete ein Regler (EPEVER Tracer-AN,
    // siehe CLAUDE.md) auch schon bei wenigen mA Ladestrom - optisch
    // irrefuehrend. Diese beiden Zustaende werden deshalb nur noch bei
    // mehr als 1,0A Batteriestrom angezeigt; darunter (aber noch
    // ladend laut Rohwert) neutral "Laden (schwach)". Bei 0A immer
    // "Kein Laden", unabhaengig vom Rohwert. "Erhaltung" (category 1)
    // ist von der Schwelle nicht betroffen - Erhaltungsladung laeuft
    // schon by Design mit geringem Strom.
    enum class EffectiveStatus : uint8_t { None, Float, Boost, Equalize, Weak };

    EffectiveStatus effectiveStatus(int8_t category, float current, bool currentValid) {
        if (currentValid) {
            if (current <= 0.0f) return EffectiveStatus::None;
            if (category == 2 || category == 3) {
                if (current > 1.0f) return (category == 2) ? EffectiveStatus::Boost : EffectiveStatus::Equalize;
                return EffectiveStatus::Weak;
            }
        }
        switch (category) {
            case 1: return EffectiveStatus::Float;
            case 2: return EffectiveStatus::Boost;
            case 3: return EffectiveStatus::Equalize;
            default: return EffectiveStatus::None;
        }
    }

    I18n::Str statusLabelKey(EffectiveStatus s) {
        switch (s) {
            case EffectiveStatus::Float: return I18n::Str::CHARGE_STATUS_1;
            case EffectiveStatus::Boost: return I18n::Str::CHARGE_STATUS_2;
            case EffectiveStatus::Equalize: return I18n::Str::CHARGE_STATUS_3;
            case EffectiveStatus::Weak: return I18n::Str::CHARGE_STATUS_WEAK;
            default: return I18n::Str::CHARGE_STATUS_0;
        }
    }

    I18n::Str statusExplainKey(EffectiveStatus s) {
        switch (s) {
            case EffectiveStatus::Float: return I18n::Str::BATTERY_EXPLAIN_1;
            case EffectiveStatus::Boost: return I18n::Str::BATTERY_EXPLAIN_2;
            case EffectiveStatus::Equalize: return I18n::Str::BATTERY_EXPLAIN_3;
            case EffectiveStatus::Weak: return I18n::Str::BATTERY_EXPLAIN_WEAK;
            default: return I18n::Str::BATTERY_EXPLAIN_0;
        }
    }

    // Wortweiser Umbruch nach echter Pixelbreite bei der AKTUELL per
    // t.setTextSize() gesetzten Groesse (muss vom Aufrufer vorher
    // gesetzt sein) - gleiches Prinzip wie HelpPopup::wrapLines(), hier
    // bewusst lokal dupliziert statt geteilt (siehe CLAUDE.md Code-Stil:
    // jeder Screen bleibt unabhaengig lauffaehig). Faellt bei einem
    // einzelnen zu breiten Wort auf harten Pixel-Schnitt zurueck, damit
    // nie ueber den Rand gezeichnet wird.
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

    StatusRender layoutStatusText(TFT_eSPI& t, const String& text, int16_t maxW) {
        StatusRender r;
        constexpr uint8_t trySizes[] = {2, 1};
        for (uint8_t s : trySizes) {
            t.setTextSize(s);
            if (t.textWidth(text) <= maxW) {
                r.size = s;
                r.lineCount = 1;
                r.lines[0] = text;
                return r;
            }
        }
        t.setTextSize(1);
        r.size = 1;
        r.lineCount = wrapWords(t, text, maxW, r.lines, 2);
        if (r.lineCount == 0) {
            r.lineCount = 1;
            r.lines[0] = text;
        }
        return r;
    }

    // Misst das tatsaechlich gezeichnete Rechteck eines Textes bei der
    // gegebenen Groesse - fuer die Innerhalb-des-Rahmens-Pruefung
    // (LayoutCheck::containedRect()), gleiches Prinzip wie power_page.cpp
    // (hier bewusst lokal dupliziert, siehe CLAUDE.md Code-Stil).
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

    // Wie LayoutUtils::fitText(), aber OHNE "..."-Kuerzung - fuer die
    // kombinierte Wert+Einheit-Zeile ("49,47 V" statt getrennter Wert-/
    // Einheitszeilen, die sich vorher ueberlappen konnten), gleiches
    // Prinzip wie years_page.cpp/power_page.cpp.
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

    // Beschriftung oben, WERT+EINHEIT ZUSAMMEN in einer Zeile darunter
    // ("49,47 V" statt getrennter Wert-/Einheitszeilen) - behebt eine
    // Ueberlappung zwischen Wert und Einheit, die bei der getrennten
    // Darstellung auftreten konnte (siehe CLAUDE.md "Seite Leistung").
    // Zeichnet in LOKALEN Koordinaten - absX/absY nur fuer LayoutCheck.
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

        // Zahlenwert in Sekundaerfarbe, Einheit in Systemfarbe (Auftrag
        // "Farbregeln vereinheitlichen") - Groesse weiter anhand der
        // Gesamtlaenge (Zahl+Einheit) bestimmt, dann als zwei
        // Farbsegmente nebeneinander gezeichnet.
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

    // Flache Kachel fuer Max./Min. Batteriespannung (Auftrag "zwei neue
    // Kacheln") - Beschriftung+Wert+Einheit auf EINER Zeile ("Max
    // 53,41 V"), anders als renderValueTile() (Beschriftung oben,
    // Wert/Einheit darunter) - bei nur ca. 28px Hoehe ist dafuer kein
    // Platz. Nur Groesse 1 (immer kurz genug, Zahl wird nie gekuerzt).
    void renderFlatValueTile(TFT_eSPI& t, int16_t w, int16_t h, int16_t absX, int16_t absY,
                              I18n::Str labelKey, const String& valueStr, const char* unit,
                              bool valid, bool stale, uint16_t accent) {
        t.drawRoundRect(0, 0, w, h, 4, accent);
        t.setTextSize(1);
        t.setTextDatum(ML_DATUM);
        uint16_t numColor = Display::valueColor(valid && !stale, accent);
        // Auftrag "fehlendes Leerzeichen Max/Min": t.textWidth() auf einen
        // String, der mit einem LEERZEICHEN endet ("Max "), lieferte beim
        // eigenen Font denselben Wert wie ohne das Leerzeichen (dessen
        // Vorschubbreite wird am Stringende offenbar nicht mitgezaehlt) -
        // der Wert wurde dadurch direkt an die Beschriftung angrenzend
        // gezeichnet ("Max53,48"), obwohl das Leerzeichen selbst
        // technisch mitgezeichnet wurde. Fester Pixel-Abstand (GAP) statt
        // eines gemessenen Leerzeichens behebt das zuverlaessig.
        constexpr int16_t GAP = 4;
        String labelOnly = String(I18n::t(labelKey));
        String unitPart = unit ? (String(" ") + unit) : String();
        int16_t labelW = t.textWidth(labelOnly);
        int16_t numW = t.textWidth(valueStr);
        int16_t unitW = unit ? t.textWidth(unitPart) : 0;
        int16_t totalW = (int16_t)(labelW + GAP + numW + unitW);
        int16_t cy = (int16_t)(h / 2);
        int16_t startX = (int16_t)(w / 2 - totalW / 2);
        t.setTextColor(accent, TFT_BLACK);
        t.drawString(labelOnly, startX, cy);
        t.setTextColor(numColor, TFT_BLACK);
        t.drawString(valueStr, (int16_t)(startX + labelW + GAP), cy);
        if (unit) {
            t.setTextColor(accent, TFT_BLACK);
            t.drawString(unitPart, (int16_t)(startX + labelW + GAP + numW), cy);
        }
        t.setTextDatum(TL_DATUM);
        {
            int16_t th = t.fontHeight();
            LayoutCheck::containedRect("Kachel-Wert", absX, absY, w, h,
                                        (int16_t)(absX + startX), (int16_t)(absY + cy - th / 2), totalW, th);
        }
        LayoutCheck::rect("Kachel-Rahmen", absX, absY, w, h);
    }

    void renderStatusTile(TFT_eSPI& t, int16_t w, int16_t h, int16_t absX, int16_t absY, const StatusRender& render,
                           bool valid, bool stale, uint16_t accent) {
        t.drawRoundRect(0, 0, w, h, 4, accent);

        t.setTextSize(1);
        t.setTextDatum(TC_DATUM);
        t.setTextColor(accent, TFT_BLACK);
        const char* labelText = I18n::t(I18n::Str::BATTERY_TILE_STATUS);
        int16_t labelTopY = 5;
        t.drawString(labelText, (int16_t)(w / 2), labelTopY);
        t.setTextDatum(TL_DATUM);
        {
            Measured lr = measureTopCentered(t, (int16_t)(w / 2), labelTopY, labelText, 1);
            LayoutCheck::containedRect("Kachel-Beschriftung", absX, absY, w, h,
                                        (int16_t)(absX + lr.x), (int16_t)(absY + lr.y), lr.w, lr.h);
        }

        constexpr int16_t LABEL_H = 10;
        int16_t valueZoneTop = LABEL_H;
        int16_t valueZoneH = (int16_t)(h - LABEL_H - 2);

        // Klarstellung Auftrag "Farbregel nachziehen": der Ladestatus ist
        // ein vom Portal abgefragter WERT (auch wenn Text statt Zahl) -
        // Sekundaerfarbe wie jeder andere abgefragte Wert.
        t.setTextColor(Display::valueColor(valid && !stale, accent), TFT_BLACK);
        t.setTextSize(render.size);
        int16_t lineH = t.fontHeight();
        int16_t totalH = (int16_t)(render.lineCount * lineH);
        int16_t textTop = (int16_t)(valueZoneTop + (valueZoneH - totalH) / 2);
        t.setTextDatum(TC_DATUM);
        int16_t widestLineW = 0;
        for (uint8_t i = 0; i < render.lineCount; i++) {
            t.drawString(render.lines[i], (int16_t)(w / 2), (int16_t)(textTop + i * lineH));
            int16_t lw = t.textWidth(render.lines[i]);
            if (lw > widestLineW) widestLineW = lw;
        }
        t.setTextDatum(TL_DATUM);
        LayoutCheck::containedRect("Kachel-Wert", absX, absY, w, h,
                                    (int16_t)(absX + w / 2 - widestLineW / 2), (int16_t)(absY + textTop), widestLineW, totalH);

        LayoutCheck::rect("Kachel-Rahmen", absX, absY, w, h);
    }

    // ageMinutes: Alter des Hauptwerts (batteryMeasuredAt) in Minuten
    // (-1 = noch kein Wert) - zeigt ab 2 Minuten das tatsaechliche Alter
    // statt des blossen Worts "veraltet" (Auftrag "veraltet nach
    // erfolgreichem Refresh").
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
                // "Naechste Abfrage: HH:MM" statt "Messung HH:MM"
                // (Auftrag) - gleiches Prinzip wie power_page.cpp, nutzt
                // denselben Fetch-Task-Zeitplan (Batterie laeuft im
                // selben Zyklus wie Ertrag/Leistung). AUSNAHME nur hier
                // auf der Batterieseite (Auftrag "Batterie auch nachts
                // abfragen"): nachts, NACH bestaetigter Abschlussabfrage/
                // Sicherung, zeigt diese Zeile die Zeit des naechsten
                // naechtlichen Batterie-Abrufs (alle 10 Minuten) statt
                // "04:30 Uhr" - nightBatteryNextFetchAt ist nur in genau
                // diesem Fall > 0 (siehe fetch_task.cpp), alle anderen
                // Seiten lesen weiterhin nur nextFetchAt und bleiben
                // dadurch unveraendert.
                char hhmm[8];
                time_t nextAt = snap.nightBatteryNextFetchAt > 0 ? snap.nightBatteryNextFetchAt : snap.nextFetchAt;
                if (nextAt > 0) {
                    struct tm t;
                    localtime_r(&nextAt, &t);
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

    String statusDisplayFor(const SolarData::Data& snap) {
        if (snap.chargeStatusIsText && snap.chargeStatusRawText[0]) {
            return String(snap.chargeStatusRawText);
        }
        if (snap.chargeStatusCode >= 0) {
            EffectiveStatus s = effectiveStatus(chargeCategory(snap.chargeStatusCode), snap.batteryCurrent, snap.batteryValid);
            return String(I18n::t(statusLabelKey(s)));
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
        int16_t y0 = 0, y1 = 0, y2 = 0, row1H = TILE_H;
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
        L.statusRender = layoutStatusText(tft, statusDisplay, statusAvailW);

        L.y0 = gridTopFor(areaTop);
        L.row1H = TILE_H;
        if (L.statusRender.lineCount > 1) {
            tft.setTextSize(L.statusRender.size);
            int16_t extra = (int16_t)((L.statusRender.lineCount - 1) * tft.fontHeight());
            L.row1H = (int16_t)(TILE_H + extra);
        }
        L.y1 = (int16_t)(L.y0 + TILE_H + TILE_GAP);
        // Dritte, flache Reihe (Max./Min. Spannung, Auftrag) - direkt
        // unter der bisherigen 2-reihigen Kachelgruppe.
        L.y2 = (int16_t)(L.y1 + L.row1H + TILE_GAP);
        int16_t afterGrid = (int16_t)(L.y2 + FLAT_TILE_H);

        // Erklaerungszeile nur bei einem echten, bekannten Status (kein
        // Rohtext, kein "--") - ohne Wert zeigt sie nichts (Auftrag).
        L.explainLineCount = 0;
        if (!snap.chargeStatusIsText && snap.chargeStatusCode >= 0 && snap.batteryValid) {
            EffectiveStatus s = effectiveStatus(chargeCategory(snap.chargeStatusCode), snap.batteryCurrent, snap.batteryValid);
            I18n::Str explainKey = statusExplainKey(s);
            tft.setTextSize(1);
            int16_t explainAvailW = (int16_t)(Config::SCREEN_WIDTH - 16);
            L.explainLineCount = wrapWords(tft, I18n::t(explainKey), explainAvailW, L.explainLines, EXPLAIN_MAX_LINES);
        }

        int16_t afterExplain;
        if (L.explainLineCount > 0) {
            L.explainTop = (int16_t)(afterGrid + 2);
            afterExplain = (int16_t)(L.explainTop + L.explainLineCount * EXPLAIN_LINE_H);
        } else {
            L.explainTop = afterGrid;
            afterExplain = afterGrid;
        }
        // +4 gewollter Abstand + 14, weil die Tipp-Flaeche der Statuszeile
        // (siehe timestampTapZone()) 14px UEBER den Text hinausreicht -
        // ohne dieses +14 ragt die Tipp-Flaeche in die Erklaerungszeile
        // hinein (vom Layout-Pruefer als echter Konflikt gemeldet).
        L.timestampY = (int16_t)(afterExplain + 4 + 14);

        return L;
    }

    Rect timestampTapZone(const PageLayout& L) {
        return {0, (int16_t)(L.timestampY - 14), Config::SCREEN_WIDTH, 40};
    }
}

void draw(int16_t areaTop, int16_t areaHeight, bool forceRedraw) {
    SolarData::Data snap = SolarData::snapshot();
    uint16_t accent = Display::systemColor();

    if (forceRedraw) {
        lastSocSig = "\x01";
        lastVoltSig = "\x01";
        lastCurrSig = "\x01";
        lastTempSig = "\x01";
        lastStatusTileSig = "\x01";
        lastExplainSig = "\x01";
        lastTimestampSig = "\x01";
        lastMaxMinSig = "\x01";
    }

    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(accent, TFT_BLACK);
    tft.drawString(I18n::t(I18n::Str::PAGE_BATTERY_TITLE), Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + 6));
    tft.setTextDatum(TL_DATUM);
    LayoutCheck::rect("Titel", 0, areaTop, Config::SCREEN_WIDTH, 16);

    time_t now = time(nullptr);
    long ageMinutes = snap.batteryMeasuredAt > 0 ? (long)((now - snap.batteryMeasuredAt) / 60) : -1;
    bool stale = snap.batteryMeasuredAt > 0 &&
                 (now - snap.batteryMeasuredAt) > (time_t)(Config::EPEVER_STALE_AFTER_MS / 1000);
    bool haveSoc = snap.batteryValid;

    // Ladezustand gross und zentriert - eigener Off-Screen-Puffer.
    // Zahlenwert in Sekundaerfarbe, Einheit "%" in Systemfarbe (Auftrag
    // "Farbregeln vereinheitlichen").
    char socNum[8];
    if (haveSoc) snprintf(socNum, sizeof(socNum), "%.0f", (double)snap.batterySocPercent);
    else strncpy(socNum, "--", sizeof(socNum));
    char socStr[12];
    snprintf(socStr, sizeof(socStr), "%s %%", socNum);
    int16_t socZoneTop = (int16_t)(areaTop + SOC_Y_OFFSET);
    int16_t socZoneW = (int16_t)(Config::SCREEN_WIDTH - 20);
    uint16_t socNumColor = Display::valueColor(haveSoc && !stale, accent);
    String socSig = String(socStr);
    if (socSig != lastSocSig) {
        lastSocSig = socSig;
        RedrawDiag::log("Batterie-SOC", "Inhalt geaendert");
        TileSprite::drawInto(10, socZoneTop, socZoneW, SOC_ZONE_H, [&](TFT_eSPI& t) {
            LayoutUtils::FitResult socFit = LayoutUtils::fitText(t, socStr, socZoneW, SOC_ZONE_H, SOC_SIZES, 3);
            t.setTextSize(socFit.textSize);
            if (socFit.text == socStr) {
                int16_t numW = t.textWidth(socNum);
                int16_t totalW = t.textWidth(socStr);
                int16_t startX = (int16_t)(socZoneW / 2 - totalW / 2);
                t.setTextDatum(TL_DATUM);
                t.setTextColor(socNumColor, TFT_BLACK);
                t.drawString(socNum, startX, 0);
                t.setTextColor(accent, TFT_BLACK);
                t.drawString(" %", (int16_t)(startX + numW), 0);
            } else {
                t.setTextColor(socNumColor, TFT_BLACK);
                t.setTextDatum(TC_DATUM);
                t.drawString(socFit.text, (int16_t)(socZoneW / 2), 0);
                t.setTextDatum(TL_DATUM);
            }
        });
    }
    LayoutCheck::rect("Ladezustand", 10, socZoneTop, socZoneW, SOC_ZONE_H);

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
    String voltSig = String(voltStr) + (snap.batteryValid ? "1" : "0") + (stale ? "s" : "");
    String currSig = String(currStr) + (snap.batteryValid ? "1" : "0") + (stale ? "s" : "");
    String tempSig = String(tempStr) + (snap.batteryValid ? "1" : "0") + (stale ? "s" : "");

    if (voltSig != lastVoltSig) {
        lastVoltSig = voltSig;
        RedrawDiag::log("Batterie-Spannung", "Inhalt geaendert");
        TileSprite::drawInto(x0, L.y0, tileW, TILE_H, [&](TFT_eSPI& t) {
            renderValueTile(t, tileW, TILE_H, x0, L.y0, I18n::Str::BATTERY_TILE_VOLTAGE, voltStr, I18n::t(I18n::Str::VOLT_UNIT), snap.batteryValid, stale, accent);
        });
    } else {
        LayoutCheck::rect("Kachel-Rahmen", x0, L.y0, tileW, TILE_H);
    }
    if (currSig != lastCurrSig) {
        lastCurrSig = currSig;
        RedrawDiag::log("Batterie-Strom", "Inhalt geaendert");
        TileSprite::drawInto(x1, L.y0, tileW, TILE_H, [&](TFT_eSPI& t) {
            renderValueTile(t, tileW, TILE_H, x1, L.y0, I18n::Str::BATTERY_TILE_CURRENT, currStr, I18n::t(I18n::Str::AMP_UNIT), snap.batteryValid, stale, accent);
        });
    } else {
        LayoutCheck::rect("Kachel-Rahmen", x1, L.y0, tileW, TILE_H);
    }
    if (tempSig != lastTempSig) {
        lastTempSig = tempSig;
        RedrawDiag::log("Batterie-Temperatur", "Inhalt geaendert");
        TileSprite::drawInto(x0, L.y1, tileW, L.row1H, [&](TFT_eSPI& t) {
            renderValueTile(t, tileW, L.row1H, x0, L.y1, I18n::Str::BATTERY_TILE_TEMP, tempStr, I18n::t(I18n::Str::CELSIUS_UNIT), snap.batteryValid, stale, accent);
        });
    } else {
        LayoutCheck::rect("Kachel-Rahmen", x0, L.y1, tileW, L.row1H);
    }

    // Max./Min. Batteriespannung (Auftrag "Max/Min-Spannungskacheln") -
    // echte Tageswerte, laufen in der bestehenden Abfrage mit. "--" nur,
    // wenn noch nie ein gueltiger Wert empfangen wurde (direkt nach dem
    // Start) - danach bleibt bei einem Ausfall/im Nachtmodus der zuletzt
    // gueltige Wert stehen (snap.batteryVoltageExtremesValid wird dafuer
    // NIE wieder auf false gesetzt, siehe SolarData::
    // setBatteryVoltageExtremes()).
    char maxVStr[16], minVStr[16];
    if (snap.batteryVoltageExtremesValid) {
        formatNumber(snap.batteryVoltageMax, 2, maxVStr, sizeof(maxVStr));
        formatNumber(snap.batteryVoltageMin, 2, minVStr, sizeof(minVStr));
    } else {
        strncpy(maxVStr, "--", sizeof(maxVStr));
        strncpy(minVStr, "--", sizeof(minVStr));
    }
    String maxMinSig = String(maxVStr) + "|" + minVStr + (snap.batteryVoltageExtremesValid ? "1" : "0") + (stale ? "s" : "");
    if (maxMinSig != lastMaxMinSig) {
        lastMaxMinSig = maxMinSig;
        RedrawDiag::log("Batterie-Max-Min-Spannung", "Inhalt geaendert");
        TileSprite::drawInto(x0, L.y2, tileW, FLAT_TILE_H, [&](TFT_eSPI& t) {
            renderFlatValueTile(t, tileW, FLAT_TILE_H, x0, L.y2, I18n::Str::BATTERY_MAX_VOLTAGE_LABEL, maxVStr, I18n::t(I18n::Str::VOLT_UNIT), snap.batteryVoltageExtremesValid, stale, accent);
        });
        TileSprite::drawInto(x1, L.y2, tileW, FLAT_TILE_H, [&](TFT_eSPI& t) {
            renderFlatValueTile(t, tileW, FLAT_TILE_H, x1, L.y2, I18n::Str::BATTERY_MIN_VOLTAGE_LABEL, minVStr, I18n::t(I18n::Str::VOLT_UNIT), snap.batteryVoltageExtremesValid, stale, accent);
        });
    } else {
        LayoutCheck::rect("Kachel-Rahmen", x0, L.y2, tileW, FLAT_TILE_H);
        LayoutCheck::rect("Kachel-Rahmen", x1, L.y2, tileW, FLAT_TILE_H);
    }

    String statusTileSig;
    for (uint8_t i = 0; i < L.statusRender.lineCount; i++) statusTileSig += L.statusRender.lines[i];
    statusTileSig += (snap.batteryValid ? "1" : "0");
    statusTileSig += (stale ? "s" : "");
    if (statusTileSig != lastStatusTileSig) {
        lastStatusTileSig = statusTileSig;
        RedrawDiag::log("Batterie-Status-Kachel", "Inhalt geaendert");
        TileSprite::drawInto(x1, L.y1, tileW, L.row1H, [&](TFT_eSPI& t) {
            renderStatusTile(t, tileW, L.row1H, x1, L.y1, L.statusRender, snap.batteryValid, stale, accent);
        });
    } else {
        LayoutCheck::rect("Kachel-Rahmen", x1, L.y1, tileW, L.row1H);
    }

    // Erklaerungszeile - eigener Off-Screen-Puffer in maximaler
    // Zeilenzahl-Groesse (der Ladestatus kann zwischen Abfragen
    // wechseln, siehe Auftrag).
    String explainSig;
    for (uint8_t i = 0; i < L.explainLineCount; i++) explainSig += L.explainLines[i];
    if (explainSig != lastExplainSig) {
        lastExplainSig = explainSig;
        RedrawDiag::log("Batterie-Erklaerung", "Inhalt geaendert");
        // Puffer muss die unterste Zeile VOLLSTAENDIG fassen, nicht nur
        // EXPLAIN_LINE_H*Zeilenzahl (der zeilenweise Abstand ist bewusst
        // enger als die Schrifthoehe, siehe EXPLAIN_LINE_H-Kommentar oben -
        // der PUFFER selbst muss trotzdem bis zum unteren Rand der letzten
        // Zeile reichen, sonst wird sie abgeschnitten, Auftrag
        // "Abgeschnittener Text").
        tft.setTextSize(1);
        int16_t explainFontH = tft.fontHeight();
        int16_t explainSpriteH = (int16_t)((EXPLAIN_MAX_LINES - 1) * EXPLAIN_LINE_H + explainFontH + 2);
        TileSprite::drawInto(8, L.explainTop, (int16_t)(Config::SCREEN_WIDTH - 16), explainSpriteH, [&](TFT_eSPI& t) {
            if (L.explainLineCount > 0) {
                t.setTextSize(1);
                t.setTextColor(Display::secondaryColor(), TFT_BLACK);
                t.setTextDatum(TC_DATUM);
                for (uint8_t i = 0; i < L.explainLineCount; i++) {
                    t.drawString(L.explainLines[i], (int16_t)((Config::SCREEN_WIDTH - 16) / 2), (int16_t)(i * EXPLAIN_LINE_H));
                }
                t.setTextDatum(TL_DATUM);
            }
        });
        if (L.explainLineCount > 0) {
            int16_t usedH = (int16_t)((L.explainLineCount - 1) * EXPLAIN_LINE_H + explainFontH);
            LayoutCheck::bufferRect("Erklaerung-Puffer", (int16_t)(Config::SCREEN_WIDTH - 16), explainSpriteH,
                                     (int16_t)(Config::SCREEN_WIDTH - 16), usedH);
        }
    }
    if (L.explainLineCount > 0) {
        LayoutCheck::rect("Erklaerung", 8, L.explainTop, (int16_t)(Config::SCREEN_WIDTH - 16), (int16_t)(L.explainLineCount * EXPLAIN_LINE_H));
    }

    // Refresh-Rueckmeldung (Auftrag "Refresh-Regeln") hat Vorrang vor der
    // normalen Statuszeile, solange sie aktiv ist (ca. 2s, bei "Working"
    // bis zum naechsten Ereignis).
    // Anzeige-Entscheidung aus feedbackState.active (NICHT aus dem
    // Rueckgabewert) - siehe refresh_feedback_text.h, Auftrag
    // "Rueckmeldung bleibt dauerhaft stehen".
    feedbackState.update(snap);
    String statusLine = feedbackState.active ? feedbackState.text : statusTextFor(snap, ageMinutes);

    // Auftrag "Statuszeile wird abgeschnitten": "Naechste Abfrage: HH:MM
    // Uhr" + " veraltet" (oder eine lange Refresh-Rueckmeldung wie die
    // Nachtsperre-Meldung) passt nicht immer auf eine Zeile - LayoutCheck
    // meldete das zuvor nur ("wird abgeschnitten"), ohne dass etwas
    // dagegen unternommen wurde. Jetzt: erst volle Breite probieren,
    // sonst wortweise auf bis zu 2 Zeilen umbrechen (nie abgeschnitten,
    // CLAUDE.md Layout-Regel) - 8px Sicherheitsrand (4px je Seite), damit
    // "knapp passt" nicht wieder "knapp nicht passt" wird. Diese
    // Berechnung laeuft JEDEN draw()-Aufruf (nicht nur bei Aenderung),
    // damit die LayoutCheck-Registrierung unten immer aktuell ist.
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
    // Bei 2 Zeilen engerer Zeilenabstand (14px statt der vollen
    // Schrifthoehe ~17px) - die Flaeche bis zur Fusszone ist auf dieser
    // Seite knapp (3 Kachelreihen + Erklaerzeile), gleiches Prinzip wie
    // EXPLAIN_LINE_H oben ("etwas enger als fontHeight(17)", dort bereits
    // bewaehrt).
    int16_t pitch = (lineCount > 1) ? 14 : lineH;
    int16_t spriteH = (int16_t)(pitch * lineCount + 2);

    if (statusLine != lastTimestampSig) {
        lastTimestampSig = statusLine;
        RedrawDiag::log("Batterie-Statuszeile", "Inhalt geaendert");
        TileSprite::drawInto(0, L.timestampY, Config::SCREEN_WIDTH, spriteH, [&](TFT_eSPI& t) {
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
    // EIN Rechteck fuer Tipp-Flaeche UND Text zusammen registrieren (statt
    // zwei separaten, die sich zwangslaeufig ueberlappen wuerden - eine
    // Tipp-Flaeche UM den Text herum ist gewollt, kein Konflikt. Bei 2
    // Zeilen reicht der Text ggf. weiter nach unten als die feste 40px-
    // Tipp-Flaeche - das deckt dieses eine, vergroesserte Rechteck mit ab,
    // damit der Layout-Pruefer das trotzdem gegen die Fusszone pruefen
    // kann). Unconditioned, damit jeder draw()-Aufruf konsistent prueft.
    Rect tapZone = timestampTapZone(L);
    int16_t textBottom = (int16_t)(L.timestampY + pitch * lineCount);
    int16_t combinedBottom = textBottom > (tapZone.y + tapZone.h) ? textBottom : (int16_t)(tapZone.y + tapZone.h);
    LayoutCheck::rect("Statuszeile", tapZone.x, tapZone.y, tapZone.w, (int16_t)(combinedBottom - tapZone.y));
}

bool update() {
    SolarData::Data snap = SolarData::snapshot();
    struct tm t;
    int8_t minute = getLocalTime(&t, 5) ? (int8_t)t.tm_min : (int8_t)-1;

    bool solarChanged = snap.changeCounter != lastSeenChangeCounter;
    bool minuteTicked = minute != lastSeenMinute;
    // feedbackActive haelt update() auf "true", solange eine Refresh-
    // Rueckmeldung angezeigt wird - sonst wuerde draw() nie erneut
    // aufgerufen, um sie nach Ablauf wieder durch die normale Statuszeile
    // zu ersetzen.
    bool feedbackActive = feedbackState.update(snap);
    bool changed = firstCheck || solarChanged || minuteTicked || feedbackActive;
    if (changed) {
        if (firstCheck) RedrawDiag::log("Batterie", "erster Aufruf");
        if (solarChanged) RedrawDiag::log("Batterie", "SolarData-Zaehler geaendert");
        if (minuteTicked) RedrawDiag::log("Batterie", "Minutenwechsel");
        firstCheck = false;
        lastSeenChangeCounter = snap.changeCounter;
        lastSeenMinute = minute;
    }
    return changed;
}

bool onTap(int16_t x, int16_t y, int16_t areaTop, int16_t /*areaHeight*/) {
    SolarData::Data snap = SolarData::snapshot();
    PageLayout L = computeLayout(areaTop, snap);
    // Auftrag "manueller Refresh": nicht mehr nur die Statuszeile - die
    // GESAMTE Kachelreihe (alle sechs Kacheln: Spannung/Strom/Temperatur/
    // Status/Max/Min) ist jetzt ebenfalls antippbar, gleiche Aktion wie
    // die Statuszeile. Ein einziges, die ganze Kachelreihe umfassendes
    // Rechteck reicht dafuer (einzelne Kachel-Grenzen muessen hier nicht
    // unterschieden werden, da alle dieselbe Aktion ausloesen).
    Rect gridZone = {0, L.y0, Config::SCREEN_WIDTH, (int16_t)(L.y2 + FLAT_TILE_H - L.y0)};
    Rect tapZone = timestampTapZone(L);
    if (!gridZone.contains(x, y) && !tapZone.contains(x, y)) return false;
    // Aktiv ausgelesene Datenpunkte (Auftrag "Aktiv-Auslesen", Korrektur
    // B3): genau die vier im Auftrag genannten Werte dieser Seite
    // (Spannung/Strom/Ladestand/Temperatur) - Max/Min NICHT separat
    // angestossen (laufen aber ohnehin in derselben Nachfrage mit, siehe
    // epever_client.cpp::fetchBatteryData()).
    FetchTask::RefreshRequest req;
    req.dataPointCount = 4;
    req.dataPointIds[0] = EpeverDatapoints::BATTERY_VOLTAGE;
    req.dataPointIds[1] = EpeverDatapoints::BATTERY_CURRENT;
    req.dataPointIds[2] = EpeverDatapoints::BATTERY_SOC;
    req.dataPointIds[3] = EpeverDatapoints::BATTERY_TEMP;
    FetchTask::sendRefreshNow(req);
    return true;
}

}
