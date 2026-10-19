#include "calibration_screen.h"
#include "display.h"
#include "touch_input.h"
#include "app_config.h"
#include "i18n.h"
#include "config.h"

namespace CalibrationScreen {

namespace {
    // Gleicher Rand-Abstand wie MARGIN_PX in touch_input.cpp - die Kreuze
    // muessen genau dort stehen, wo touch_input.cpp spaeter den
    // kalibrierten Rohwert erwartet. Jedes Kreuz ist 2*CROSS_ARM+1 Pixel
    // gross - bei MARGIN=20 bleibt es damit komplett innerhalb des
    // sichtbaren Bereichs, auch in der Ecke.
    constexpr int16_t MARGIN = 20;
    constexpr int16_t CROSS_ARM = 12;

    void drawCross(int16_t x, int16_t y, uint8_t stepNum, uint8_t stepCount) {
        uint16_t accent = Display::systemColor();
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        char buf[48];
        snprintf(buf, sizeof(buf), I18n::t(I18n::Str::CALIBRATION_TAP_CROSS_FMT), stepNum, stepCount);
        tft.drawString(buf, Config::SCREEN_WIDTH / 2, 40);
        tft.setTextDatum(TL_DATUM);
        tft.drawLine((int16_t)(x - CROSS_ARM), y, (int16_t)(x + CROSS_ARM), y, accent);
        tft.drawLine(x, (int16_t)(y - CROSS_ARM), x, (int16_t)(y + CROSS_ARM), accent);
        tft.drawCircle(x, y, 6, accent);
    }

    // Wartet, bis beruehrt wird, mittelt ein paar Rohwerte zur Stabilitaet,
    // und wartet danach auf das Loslassen, bevor die Funktion zurueckkehrt -
    // verhindert, dass derselbe Tap versehentlich fuer das naechste Kreuz
    // mitgezaehlt wird.
    TouchInput::Point captureRawAtCross() {
        TouchInput::Point p;
        while (!p.touched) {
            p = TouchInput::rawPoint();
            delay(10);
        }

        long sumX = 0, sumY = 0;
        uint8_t samples = 0;
        uint32_t startMs = millis();
        while (millis() - startMs < 150) {
            TouchInput::Point s = TouchInput::rawPoint();
            if (s.touched) { sumX += s.x; sumY += s.y; samples++; }
            delay(10);
        }
        if (samples > 0) {
            p.x = (int16_t)(sumX / samples);
            p.y = (int16_t)(sumY / samples);
        }

        while (TouchInput::rawPoint().touched) delay(10);

        return p;
    }
}

void run() {
    int16_t tlX = MARGIN, tlY = MARGIN;
    int16_t trX = (int16_t)(Config::SCREEN_WIDTH - MARGIN), trY = MARGIN;
    int16_t brX = (int16_t)(Config::SCREEN_WIDTH - MARGIN), brY = (int16_t)(Config::SCREEN_HEIGHT - MARGIN);
    int16_t blX = MARGIN, blY = (int16_t)(Config::SCREEN_HEIGHT - MARGIN);

    drawCross(tlX, tlY, 1, 4);
    TouchInput::Point rawTL = captureRawAtCross();

    drawCross(trX, trY, 2, 4);
    TouchInput::Point rawTR = captureRawAtCross();

    drawCross(brX, brY, 3, 4);
    TouchInput::Point rawBR = captureRawAtCross();

    drawCross(blX, blY, 4, 4);
    TouchInput::Point rawBL = captureRawAtCross();

    // Von TL nach TR aendert sich NUR die Bildschirm-X-Position - welcher
    // Rohwert (x oder y) sich dabei staerker aendert, ist der Rohwert, der
    // der Bildschirm-X-Achse entspricht. Analog TL->BL fuer die Y-Achse.
    // Das macht die Kalibrierung unabhaengig davon, ob der Touch-Chip
    // gegenueber der aktuellen Bildschirm-Rotation vertauschte Achsen hat.
    long dTRx = (long)rawTR.x - rawTL.x;
    long dTRy = (long)rawTR.y - rawTL.y;
    bool swapXY = labs(dTRy) > labs(dTRx);

    int16_t xRawMin, xRawMax, yRawMin, yRawMax;
    if (!swapXY) {
        // X-Achse: Mittelwert aus TL->TR (oben) und BL->BR (unten) fuer
        // etwas mehr Genauigkeit als nur ein einzelnes Punktpaar.
        xRawMin = (int16_t)((rawTL.x + rawBL.x) / 2);
        xRawMax = (int16_t)((rawTR.x + rawBR.x) / 2);
        yRawMin = (int16_t)((rawTL.y + rawTR.y) / 2);
        yRawMax = (int16_t)((rawBL.y + rawBR.y) / 2);
    } else {
        xRawMin = (int16_t)((rawTL.y + rawBL.y) / 2);
        xRawMax = (int16_t)((rawTR.y + rawBR.y) / 2);
        yRawMin = (int16_t)((rawTL.x + rawTR.x) / 2);
        yRawMax = (int16_t)((rawBL.x + rawBR.x) / 2);
    }

    appConfig.touchSwapXY = swapXY;
    appConfig.touchScreenXRawMin = xRawMin;
    appConfig.touchScreenXRawMax = xRawMax;
    appConfig.touchScreenYRawMin = yRawMin;
    appConfig.touchScreenYRawMax = yRawMax;
    appConfig.touchCalibrated = true;

    TouchInput::setCalibration(swapXY, xRawMin, xRawMax, yRawMin, yRawMax);

    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
    tft.setTextColor(Display::systemColor(), TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(I18n::t(I18n::Str::CALIBRATION_DONE), Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT / 2);
    tft.setTextDatum(TL_DATUM);
    delay(800);
}

}
