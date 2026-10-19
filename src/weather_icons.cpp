#include "weather_icons.h"
#include "weather_icon_colors.h"
#include <math.h>

namespace WeatherIcons {

namespace {
    using WeatherIconColors::RGB;

    uint16_t toColor(TFT_eSPI& t, RGB c) { return t.color565(c.r, c.g, c.b); }

    // Gefuellter Kreis MIT hellem Rand (Auftrag "knallig, bunt, mit
    // hellem Rand/dunkler Kontur, damit sie sich vom Hintergrund
    // abheben") - der Hintergrund ist bei uns immer schwarz (siehe
    // weather_icon_colors.h), deshalb genuegt ein heller Rand. "bg" ist
    // die Farbe, auf der der Kreis tatsaechlich sitzt (fuer die
    // Kantenglaettung von fillSmoothCircle) - meist TFT_BLACK, bei
    // Details AUF einem anderen Symbolteil (z.B. Augen auf der Sonne)
    // dessen Fuellfarbe.
    void outlinedCircle(TFT_eSPI& t, int16_t cx, int16_t cy, int16_t r, uint16_t fill, uint16_t bg) {
        uint16_t outline = toColor(t, WeatherIconColors::OUTLINE);
        // Aeusserer Ring liegt auf "bg" (echter Hintergrund dahinter),
        // der innere Fuell-Kreis liegt auf DIESEM Ring - sein eigener
        // Kantenglaettungs-Hintergrund muss deshalb "outline" sein, nicht
        // "bg", sonst entsteht ein duenner schwarzer Spalt zwischen
        // Fuellung und Rand.
        t.fillSmoothCircle(cx, cy, (int16_t)(r + 1), outline, bg);
        t.fillSmoothCircle(cx, cy, r, fill, outline);
    }

    // Kleines, freundliches Gesicht (zwei Augen + laechelnder Mund) -
    // nur wenn die Groesse es zulaesst (Auftrag), sonst wirkt es bei der
    // kleinen Wochenansicht nur wie Pixelmatsch. "bg" = die Fuellfarbe
    // des Koerpers (Sonne/Mond), auf dem das Gesicht sitzt.
    void drawFace(TFT_eSPI& t, int16_t cx, int16_t cy, int16_t r, uint16_t bg) {
        if (r < 9) return;
        int16_t eyeR = (int16_t)(r * 0.11f);
        if (eyeR < 1) eyeR = 1;
        int16_t eyeOffX = (int16_t)(r * 0.38f);
        int16_t eyeY = (int16_t)(cy - r * 0.12f);
        t.fillSmoothCircle((int16_t)(cx - eyeOffX), eyeY, eyeR, TFT_BLACK, bg);
        t.fillSmoothCircle((int16_t)(cx + eyeOffX), eyeY, eyeR, TFT_BLACK, bg);

        // Laechelnder Mund als kleines "V" (zwei kurze Liniensegmente,
        // bei dieser winzigen Groesse optisch wie eine sanfte Kurve) -
        // Mitte tiefer als die Enden.
        int16_t mouthY = (int16_t)(cy + r * 0.32f);
        int16_t mouthHalfW = (int16_t)(r * 0.3f);
        int16_t mouthDip = (int16_t)(r * 0.14f);
        if (mouthHalfW >= 2) {
            t.drawLine((int16_t)(cx - mouthHalfW), mouthY, cx, (int16_t)(mouthY + mouthDip), TFT_BLACK);
            t.drawLine(cx, (int16_t)(mouthY + mouthDip), (int16_t)(cx + mouthHalfW), mouthY, TFT_BLACK);
        }
    }

    // Sonne: kraeftig gelber Koerper, abwechselnd lange/kurze orange
    // Strahlen (Auftrag), weiche Enden ueber drawWedgeLine statt
    // treppiger drawLine.
    void drawSun(TFT_eSPI& t, int16_t cx, int16_t cy, int16_t r, bool withFace) {
        int16_t core = (int16_t)(r * 0.5f);
        if (core < 2) core = 2;
        uint16_t coreColor = toColor(t, WeatherIconColors::SUN_CORE);
        uint16_t rayColor = toColor(t, WeatherIconColors::SUN_RAYS);
        for (uint8_t i = 0; i < 8; i++) {
            float ang = i * 0.7853981f; // 45 Grad in Radiant
            // Abwechselnd lange/kurze Strahlen (Auftrag).
            float rayLen = (i % 2 == 0) ? r * 1.0f : r * 0.78f;
            float x1 = cx + cosf(ang) * (core + 1);
            float y1 = cy + sinf(ang) * (core + 1);
            float x2 = cx + cosf(ang) * rayLen;
            float y2 = cy + sinf(ang) * rayLen;
            t.drawWedgeLine(x1, y1, x2, y2, 1.6f, 0.6f, rayColor, TFT_BLACK);
        }
        outlinedCircle(t, cx, cy, core, coreColor, TFT_BLACK);
        if (withFace) drawFace(t, cx, cy, core, coreColor);
    }

    // Mond: warmer Gelbton (KEIN Weiss, Auftrag), Sichel durch einen
    // versetzten schwarzen "Biss", dazu 2-3 kleine Sterne.
    void drawMoon(TFT_eSPI& t, int16_t cx, int16_t cy, int16_t r, bool withFace) {
        int16_t core = (int16_t)(r * 0.58f);
        if (core < 2) core = 2;
        uint16_t color = toColor(t, WeatherIconColors::MOON_CORE);
        outlinedCircle(t, cx, cy, core, color, TFT_BLACK);
        int16_t biteR = (int16_t)(core * 0.82f);
        t.fillSmoothCircle((int16_t)(cx + core * 0.48f), (int16_t)(cy - core * 0.32f), biteR, TFT_BLACK, color);
        if (withFace) drawFace(t, (int16_t)(cx - core * 0.18f), cy, core, color);

        // Kleine Sterne (gelb/weiss gemischt, Auftrag) - nur wenn genug
        // Platz um den Mond herum bleibt.
        if (r > 8) {
            uint16_t starWarm = toColor(t, WeatherIconColors::MOON_STAR_WARM);
            uint16_t starCool = toColor(t, WeatherIconColors::MOON_STAR_COOL);
            struct { float dx, dy; uint16_t color; } stars[3] = {
                {-0.75f, -0.55f, starCool},
                {0.7f, 0.65f, starWarm},
                {-0.6f, 0.75f, starCool},
            };
            uint8_t starCount = (r > 12) ? 3 : 2;
            for (uint8_t i = 0; i < starCount; i++) {
                int16_t sx = (int16_t)(cx + stars[i].dx * r);
                int16_t sy = (int16_t)(cy + stars[i].dy * r);
                int16_t sr = (int16_t)(r * 0.08f);
                if (sr < 1) sr = 1;
                t.fillSmoothCircle(sx, sy, sr, stars[i].color, TFT_BLACK);
            }
        }
    }

    // Weiche Wolke aus ueberlappenden Kreisen + abgerundeter Basis -
    // optional zweifarbig (heller oben, etwas dunkler an der Unterseite,
    // Auftrag "leicht bewoelkt").
    void drawCloud(TFT_eSPI& t, int16_t cx, int16_t cy, int16_t r, uint16_t topColor, uint16_t bottomColor) {
        int16_t baseW = (int16_t)(r * 1.6f);
        int16_t baseH = (int16_t)(r * 0.55f);
        if (baseW < 4) baseW = 4;
        if (baseH < 2) baseH = 2;
        uint16_t outline = toColor(t, WeatherIconColors::OUTLINE);

        // Rand: dieselbe Form 1px groesser in der Kontur-Farbe, dann die
        // eigentliche Wolke obendrauf (Auftrag "heller Rand").
        t.fillSmoothRoundRect((int16_t)(cx - baseW / 2 - 1), (int16_t)(cy - 1), (int16_t)(baseW + 2), (int16_t)(baseH + 2),
                               (int16_t)(baseH / 2 + 1), outline, TFT_BLACK);
        t.fillSmoothCircle((int16_t)(cx - r * 0.35f), cy, (int16_t)(r * 0.45f + 1), outline, TFT_BLACK);
        t.fillSmoothCircle(cx, (int16_t)(cy - r * 0.2f), (int16_t)(r * 0.55f + 1), outline, TFT_BLACK);
        t.fillSmoothCircle((int16_t)(cx + r * 0.4f), (int16_t)(cy + r * 0.05f), (int16_t)(r * 0.4f + 1), outline, TFT_BLACK);

        // Die eigentliche Fuellung sitzt auf dem gerade gezeichneten
        // Rand - Kantenglaettungs-Hintergrund ist deshalb "outline",
        // nicht TFT_BLACK (gleiche Begruendung wie outlinedCircle()).
        t.fillSmoothRoundRect((int16_t)(cx - baseW / 2), cy, baseW, baseH, (int16_t)(baseH / 2), bottomColor, outline);
        t.fillSmoothCircle((int16_t)(cx - r * 0.35f), cy, (int16_t)(r * 0.45f), bottomColor, outline);
        t.fillSmoothCircle(cx, (int16_t)(cy - r * 0.2f), (int16_t)(r * 0.55f), topColor, outline);
        t.fillSmoothCircle((int16_t)(cx + r * 0.4f), (int16_t)(cy + r * 0.05f), (int16_t)(r * 0.4f), topColor, outline);
    }

    void drawCloud(TFT_eSPI& t, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
        drawCloud(t, cx, cy, r, color, color);
    }

    // Pastellfarbene, runde Nebelbalken (Auftrag) statt scharfkantiger
    // Doppel-Linien - drawWedgeLine mit gleicher Start-/Endbreite ergibt
    // einen weich gerundeten Balken.
    void drawFogLines(TFT_eSPI& t, int16_t cx, int16_t cy, int16_t r) {
        uint16_t colorA = toColor(t, WeatherIconColors::FOG_A);
        uint16_t colorB = toColor(t, WeatherIconColors::FOG_B);
        float w = r * 1.7f;
        float barH = (r > 10) ? 2.2f : 1.6f;
        for (uint8_t i = 0; i < 3; i++) {
            float lw = w * (1.0f - i * 0.18f);
            float y = cy - r * 0.35f + i * (r * 0.5f);
            uint16_t color = (i % 2 == 0) ? colorA : colorB;
            t.drawWedgeLine(cx - lw / 2, y, cx + lw / 2, y, barH, barH, color, TFT_BLACK);
        }
    }

    // Regentropfen als weich zulaufende Linien (drawWedgeLine, oben
    // dicker/unten spitz) mit kleinem hellen Glanzpunkt - Regen bekommt
    // mehr Tropfen als Nieselregen (Auftrag).
    void drawRainDrops(TFT_eSPI& t, int16_t cx, int16_t cy, int16_t r, uint8_t dropCount) {
        uint16_t color = toColor(t, WeatherIconColors::RAIN_DROP);
        uint16_t highlight = toColor(t, WeatherIconColors::RAIN_DROP_HIGHLIGHT);
        float y = cy + r * 0.5f;
        float spacing = r * 0.42f;
        float startOffset = -((dropCount - 1) * 0.5f) * spacing;
        for (uint8_t i = 0; i < dropCount; i++) {
            float x = cx + startOffset + i * spacing;
            float x2 = x - r * 0.1f;
            float y2 = y + r * 0.42f;
            t.drawWedgeLine(x, y, x2, y2, 1.8f, 0.4f, color, TFT_BLACK);
            int16_t hr = (int16_t)(r * 0.06f);
            if (hr >= 1) t.fillSmoothCircle((int16_t)(x - r * 0.03f), (int16_t)(y + r * 0.1f), hr, highlight, color);
        }
    }

    // Schneeflocken als 6-strahlige Sternchen (3 kreuzende Linien),
    // abwechselnd weiss/cyan (Auftrag).
    void drawSnowFlakes(TFT_eSPI& t, int16_t cx, int16_t cy, int16_t r, uint8_t flakeCount) {
        uint16_t white = toColor(t, WeatherIconColors::SNOW_FLAKE_WHITE);
        uint16_t cyan = toColor(t, WeatherIconColors::SNOW_FLAKE_CYAN);
        float spacing = r * 0.55f;
        float startOffset = -((flakeCount - 1) * 0.5f) * spacing;
        float y = cy + r * 0.55f;
        for (uint8_t i = 0; i < flakeCount; i++) {
            float x = cx + startOffset + i * spacing;
            int16_t s = (int16_t)(r * 0.22f);
            if (s < 2) s = 2;
            uint16_t color = (i % 2 == 0) ? white : cyan;
            t.drawWedgeLine(x - s, y, x + s, y, 1.2f, 1.2f, color, TFT_BLACK);
            t.drawWedgeLine(x, y - s, x, y + s, 1.2f, 1.2f, color, TFT_BLACK);
            t.drawWedgeLine(x - s * 0.7f, y - s * 0.7f, x + s * 0.7f, y + s * 0.7f, 1.0f, 1.0f, color, TFT_BLACK);
            t.drawWedgeLine(x - s * 0.7f, y + s * 0.7f, x + s * 0.7f, y - s * 0.7f, 1.0f, 1.0f, color, TFT_BLACK);
        }
    }

    // Blitz: grosser gelber Kern mit orangem Rand (Auftrag) - drawTriangle
    // zeichnet den Rand an EXAKT derselben Kontur wie fillTriangle, kein
    // Versatz noetig.
    void drawBolt(TFT_eSPI& t, int16_t cx, int16_t cy, int16_t r) {
        uint16_t core = toColor(t, WeatherIconColors::BOLT_CORE);
        uint16_t edge = toColor(t, WeatherIconColors::BOLT_EDGE);
        int16_t y0 = (int16_t)(cy + r * 0.45f);
        int16_t yMid = (int16_t)(cy + r * 0.75f);
        int16_t y2 = (int16_t)(cy + r * 1.05f);
        int16_t xMid = (int16_t)(cx - r * 0.12f);

        int16_t ax1 = cx, ay1 = y0, bx1 = (int16_t)(cx + r * 0.25f), by1 = y0, cx1 = xMid, cy1 = yMid;
        t.drawTriangle(ax1, ay1, bx1, by1, cx1, cy1, edge);
        t.fillTriangle(ax1, ay1, bx1, by1, cx1, cy1, core);

        int16_t ax2 = xMid, ay2 = yMid, bx2 = (int16_t)(cx + r * 0.1f), by2 = yMid, cx2 = (int16_t)(cx - r * 0.3f), cy2 = y2;
        t.drawTriangle(ax2, ay2, bx2, by2, cx2, cy2, edge);
        t.fillTriangle(ax2, ay2, bx2, by2, cx2, cy2, core);
    }
}

void draw(TFT_eSPI& t, int16_t cx, int16_t cy, int16_t size, int16_t weatherCode, bool isDay) {
    t.fillRect((int16_t)(cx - size / 2), (int16_t)(cy - size / 2), size, size, TFT_BLACK);
    int16_t r = (int16_t)(size / 2);
    if (r < 3) r = 3;
    bool bigEnoughForFace = r >= 14; // winzige Wochenansicht-Symbole bleiben ohne Gesicht

    if (weatherCode == 0) {
        if (isDay) drawSun(t, cx, cy, r, bigEnoughForFace);
        else drawMoon(t, cx, cy, r, bigEnoughForFace);
    } else if (weatherCode == 1 || weatherCode == 2) {
        int16_t smallR = (int16_t)(r * 0.62f);
        uint16_t top = toColor(t, WeatherIconColors::CLOUD_LIGHT_TOP);
        uint16_t bottom = toColor(t, WeatherIconColors::CLOUD_LIGHT_BOTTOM);
        if (isDay) drawSun(t, (int16_t)(cx - r * 0.3f), (int16_t)(cy - r * 0.28f), smallR, false);
        else drawMoon(t, (int16_t)(cx - r * 0.3f), (int16_t)(cy - r * 0.28f), smallR, false);
        drawCloud(t, (int16_t)(cx + r * 0.22f), (int16_t)(cy + r * 0.28f), (int16_t)(r * 0.8f), top, bottom);
    } else if (weatherCode == 3) {
        // Bedeckt: deutlich dunkleres Blaugrau als "leicht bewoelkt",
        // damit beide Lagen auf den ersten Blick unterscheidbar bleiben
        // (Auftrag).
        drawCloud(t, cx, cy, r, toColor(t, WeatherIconColors::CLOUD_OVERCAST));
    } else if (weatherCode == 45 || weatherCode == 48) {
        drawFogLines(t, cx, cy, r);
    } else if (weatherCode >= 51 && weatherCode <= 57) {
        // Nieselregen: weniger Tropfen als "Regen" (Auftrag).
        drawCloud(t, cx, (int16_t)(cy - r * 0.3f), (int16_t)(r * 0.85f), toColor(t, WeatherIconColors::RAIN_CLOUD));
        drawRainDrops(t, cx, cy, r, 2);
    } else if ((weatherCode >= 61 && weatherCode <= 67) || (weatherCode >= 80 && weatherCode <= 82)) {
        drawCloud(t, cx, (int16_t)(cy - r * 0.3f), (int16_t)(r * 0.85f), toColor(t, WeatherIconColors::RAIN_CLOUD));
        drawRainDrops(t, cx, cy, r, 4);
    } else if ((weatherCode >= 71 && weatherCode <= 77) || weatherCode == 85 || weatherCode == 86) {
        drawCloud(t, cx, (int16_t)(cy - r * 0.3f), (int16_t)(r * 0.85f), toColor(t, WeatherIconColors::SNOW_CLOUD));
        drawSnowFlakes(t, cx, cy, r, (r > 16) ? 3 : 2);
    } else if (weatherCode >= 95 && weatherCode <= 99) {
        drawCloud(t, cx, (int16_t)(cy - r * 0.3f), (int16_t)(r * 0.85f), toColor(t, WeatherIconColors::STORM_CLOUD));
        drawBolt(t, cx, cy, r);
    } else {
        // Unbekannter/nicht abgedeckter Code - neutrale helle Wolke als
        // Rueckfall, nie ein leeres Feld.
        drawCloud(t, cx, cy, r, toColor(t, WeatherIconColors::CLOUD_LIGHT_TOP), toColor(t, WeatherIconColors::CLOUD_LIGHT_BOTTOM));
    }
}

}
