#include "boot_logo.h"
#include "display.h"
#include "touch_input.h"
#include "layout_check.h"
#include "i18n.h"
#include "config.h"
#include <math.h>

namespace BootLogo {

namespace {
    // Dasselbe Gelb wie die Sonne der Wettersymbole (siehe
    // weather_icon_colors.h::SUN_CORE).
    constexpr uint8_t SUN_R8 = 255, SUN_G8 = 230, SUN_B8 = 0;

    // Nochmals kleiner (Auftrag "80% reichen", wirkte vorher unscharf/
    // großgezogen) - feste Groesse/Form, nur die Y-Position haengt von
    // der Variante ab (siehe shieldTopYFor()).
    constexpr int16_t SHIELD_CX = Config::SCREEN_WIDTH / 2; // 120
    constexpr int16_t SHIELD_W = 90;
    constexpr int16_t SHIELD_H = 108;
    constexpr int16_t SHIELD_LEFT_X = (int16_t)(SHIELD_CX - SHIELD_W / 2);
    constexpr int16_t SHIELD_RIGHT_X = (int16_t)(SHIELD_CX + SHIELD_W / 2);
    constexpr int16_t SHIELD_SHOULDER_OFFSET = 10;
    constexpr int16_t SHIELD_TAPER_OFFSET = 23;
    constexpr float SHIELD_STROKE = 2.0f; // halbe Breite -> ~4px gesamt

    // Ohne Zusatzschriftzug (Variante A) darf der Schild 20px hoeher
    // sitzen als mit ihm (Variante B, dort bereits bestaetigt) - der
    // Platz oben wird sonst nicht gebraucht.
    constexpr int16_t SHIELD_TOP_Y_B = 80;
    constexpr int16_t SHIELD_TOP_Y_A = (int16_t)(SHIELD_TOP_Y_B - 20);

    constexpr int16_t SUN_CX = SHIELD_CX;
    constexpr int16_t SUN_CY_OFFSET = 52; // ab Schild-Oberkante
    constexpr int16_t SUN_R = 14;
    constexpr int16_t RAY_GAP = 6;
    constexpr int16_t RAY_LEN = 7;
    constexpr float RAY_STROKE = 2.0f; // halbe Breite -> ~4px gesamt
    constexpr uint8_t RAY_COUNT = 8;

    // Abstand von der Schild-Unterkante bis zur OBERKANTE der Titel-
    // Schrift (Auftrag) - die tatsaechliche Baseline wird daraus zur
    // Laufzeit berechnet (haengt von der Schriftgroesse/-hoehe ab, siehe
    // begin()), nicht als feste Zahl hinterlegt.
    constexpr int16_t TITLE_GAP_BELOW_SHIELD = 10;
    // Bisheriger Abstand zwischen den beiden Baselines (Titel/Untertitel)
    // bleibt gleich (Auftrag "gleicher Abstand wie bisher").
    constexpr int16_t TITLE_TO_SUBTITLE_GAP = 24;
    constexpr int16_t UNLOCK_TEXT_BASELINE_Y = 40;

    uint32_t startMs = 0;
    uint8_t raysRevealed = 0;
    uint16_t sunColor565 = 0;
    int16_t shieldTopY = SHIELD_TOP_Y_B;
    int16_t sunCy = (int16_t)(SHIELD_TOP_Y_B + SUN_CY_OFFSET);

    void drawShieldOutline(TFT_eSPI& t, uint16_t color, int16_t topY) {
        int16_t shoulderY = (int16_t)(topY + SHIELD_SHOULDER_OFFSET);
        int16_t bottomY = (int16_t)(topY + SHIELD_H);
        int16_t vertBottomY = (int16_t)(bottomY - SHIELD_TAPER_OFFSET);

        auto seg = [&](float x1, float y1, float x2, float y2) {
            t.drawWedgeLine(x1, y1, x2, y2, SHIELD_STROKE, SHIELD_STROKE, color, TFT_BLACK);
        };

        // Obere Schultern (Oberkante laeuft in der Mitte spitz zu).
        seg(SHIELD_CX, topY, SHIELD_LEFT_X, shoulderY);
        seg(SHIELD_CX, topY, SHIELD_RIGHT_X, shoulderY);
        // Senkrechte Seiten.
        seg(SHIELD_LEFT_X, shoulderY, SHIELD_LEFT_X, vertBottomY);
        seg(SHIELD_RIGHT_X, shoulderY, SHIELD_RIGHT_X, vertBottomY);

        // Abgerundete Spitze unten: mehrere kurze, zunehmend nach innen
        // gezogene Segmente statt einer einzelnen scharfen Kante - ergibt
        // eine runde statt eckige Spitze.
        constexpr uint8_t TAPER_STEPS = 8;
        float prevLX = SHIELD_LEFT_X, prevLY = vertBottomY;
        float prevRX = SHIELD_RIGHT_X, prevRY = vertBottomY;
        for (uint8_t i = 1; i <= TAPER_STEPS; i++) {
            float tt = (float)i / TAPER_STEPS;
            float eased = tt * tt;
            float y = vertBottomY + (bottomY - vertBottomY) * tt;
            float lx = SHIELD_LEFT_X + (SHIELD_CX - SHIELD_LEFT_X) * eased;
            float rx = SHIELD_RIGHT_X + (SHIELD_CX - SHIELD_RIGHT_X) * eased;
            seg(prevLX, prevLY, lx, y);
            seg(prevRX, prevRY, rx, y);
            prevLX = lx; prevLY = y;
            prevRX = rx; prevRY = y;
        }
    }

    // Winkel 0 = rechts, steigender Index dreht auf dem (y-unten-)
    // Bildschirm im Uhrzeigersinn.
    void drawRay(TFT_eSPI& t, uint8_t index, uint16_t color) {
        float ang = index * 0.7853981f; // 45 Grad in Radiant
        float x1 = SUN_CX + cosf(ang) * (SUN_R + RAY_GAP);
        float y1 = sunCy + sinf(ang) * (SUN_R + RAY_GAP);
        float x2 = SUN_CX + cosf(ang) * (SUN_R + RAY_GAP + RAY_LEN);
        float y2 = sunCy + sinf(ang) * (SUN_R + RAY_GAP + RAY_LEN);
        t.drawWedgeLine(x1, y1, x2, y2, RAY_STROKE, RAY_STROKE, color, TFT_BLACK);
    }
}

void begin(bool unlockedVariant) {
    tft.fillScreen(TFT_BLACK);
    sunColor565 = tft.color565(SUN_R8, SUN_G8, SUN_B8);

    shieldTopY = unlockedVariant ? SHIELD_TOP_Y_B : SHIELD_TOP_Y_A;
    sunCy = (int16_t)(shieldTopY + SUN_CY_OFFSET);

    LayoutCheck::begin("Startlogo");

    drawShieldOutline(tft, sunColor565, shieldTopY);
    tft.fillSmoothCircle(SUN_CX, sunCy, SUN_R, sunColor565, TFT_BLACK);
    LayoutCheck::rect("Schild", SHIELD_LEFT_X, shieldTopY,
                       (int16_t)(SHIELD_RIGHT_X - SHIELD_LEFT_X), SHIELD_H);

    // Schild-Unterkante aus der Geometrie (haengt von der Variante ab,
    // siehe shieldTopY oben) - Titel/Untertitel ruecken relativ dazu
    // nach oben (Auftrag), keine festen Bildschirm-Y-Werte mehr.
    int16_t shieldBottomY = (int16_t)(shieldTopY + SHIELD_H);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    constexpr const char* TITLE_TEXT = "Solar Guardian";
    tft.setTextSize(2);
    int16_t titleBaselineY = (int16_t)(shieldBottomY + TITLE_GAP_BELOW_SHIELD + tft.fontHeight());
    {
        int16_t titleW = tft.textWidth(TITLE_TEXT);
        tft.setTextDatum(BC_DATUM);
        tft.drawString(TITLE_TEXT, SHIELD_CX, titleBaselineY);
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Titel", (int16_t)(SHIELD_CX - titleW / 2), (int16_t)(titleBaselineY - tft.fontHeight()),
                           titleW, tft.fontHeight());
    }

    int16_t subtitleBaselineY = (int16_t)(titleBaselineY + TITLE_TO_SUBTITLE_GAP);
    Serial.printf("[boot] Logo shieldBottom=%d titleBaseline=%d subtitleBaseline=%d\n", shieldBottomY, titleBaselineY, subtitleBaselineY); // TEMP-TEST, wieder entfernen
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextSize(1);
    {
        const char* subtitle = I18n::t(I18n::Str::BOOT_LOGO_SUBTITLE);
        int16_t subW = tft.textWidth(subtitle);
        tft.setTextDatum(BC_DATUM);
        tft.drawString(subtitle, SHIELD_CX, subtitleBaselineY);
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Untertitel", (int16_t)(SHIELD_CX - subW / 2), (int16_t)(subtitleBaselineY - tft.fontHeight()),
                           subW, tft.fontHeight());
    }

    if (unlockedVariant) {
        // Zusatzschriftzug bewusst NICHT als lesbarer String im Quelltext
        // oder in firmware.bin (Auftrag) - einzelne Bytes XOR-
        // verschluesselt, erst hier zur Laufzeit in einen lokalen Puffer
        // entschluesselt, nie als zusammenhaengendes String-Literal
        // irgendwo abgelegt.
        constexpr uint8_t KEY = 0x5A;
        constexpr uint8_t ENC[] = {9, 46, 63, 60, 60, 63, 52, 41};
        char unlockText[sizeof(ENC) + 1];
        for (uint8_t i = 0; i < sizeof(ENC); i++) unlockText[i] = (char)(ENC[i] ^ KEY);
        unlockText[sizeof(ENC)] = 0;

        // Gleiche Groesse wie der Titel "Solar Guardian" (Auftrag).
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(2);
        int16_t w = tft.textWidth(unlockText);
        tft.setTextDatum(BC_DATUM);
        tft.drawString(unlockText, SHIELD_CX, UNLOCK_TEXT_BASELINE_Y);
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Zusatzschriftzug", (int16_t)(SHIELD_CX - w / 2), (int16_t)(UNLOCK_TEXT_BASELINE_Y - tft.fontHeight()),
                           w, tft.fontHeight());
        tft.setTextSize(1);
    }

    LayoutCheck::finish();
    tft.setTextSize(1);

    startMs = millis();
    raysRevealed = 0;
}

bool tick() {
    uint32_t elapsed = millis() - startMs;

    // Strahlen nacheinander aufdecken (nur das NEU Hinzukommende
    // zeichnen, kein erneutes Loeschen - kein Flackern).
    constexpr uint32_t ANIM_MS = 1500;
    if (raysRevealed < RAY_COUNT) {
        uint32_t animElapsed = elapsed < ANIM_MS ? elapsed : ANIM_MS;
        uint8_t targetRays = (uint8_t)((animElapsed * RAY_COUNT) / ANIM_MS);
        if (targetRays > RAY_COUNT) targetRays = RAY_COUNT;
        while (raysRevealed < targetRays) {
            drawRay(tft, raysRevealed, sunColor565);
            raysRevealed++;
        }
    }

    // Antippen waehrend des Logos ignorieren, aber einmal konsumieren,
    // damit kein Phantom-Tipp in den naechsten Screen weiterwandert.
    TouchInput::Point ignored;
    TouchInput::wasTapped(ignored);

    return elapsed >= Config::BOOT_LOGO_MS;
}

}
