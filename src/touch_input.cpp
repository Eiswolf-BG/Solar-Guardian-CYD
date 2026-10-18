#include "touch_input.h"
#include "config.h"
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

namespace TouchInput {

namespace {
    SPIClass touchSpi(VSPI);
    XPT2046_Touchscreen touch(Config::TOUCH_CS_PIN, Config::TOUCH_IRQ_PIN);

    bool swapXY = false;
    int16_t xRawMin = 0, xRawMax = 4095;
    int16_t yRawMin = 0, yRawMax = 4095;

    // Die Kalibrierungs-Kreuze (siehe calibration_screen.cpp) stehen nicht
    // exakt in der Bildschirmecke, sondern mit etwas Abstand vom Rand -
    // resistive Touch-Chips sind ganz am Rand oft ungenau. xRawMin/xRawMax
    // entsprechen also dem Rohwert BEI DIESEM Abstand, nicht bei Screen 0/
    // Breite-1. map() rechnet trotzdem auch ausserhalb dieses Bereichs
    // linear weiter, das abschliessende clampi() faengt das Ergebnis dann
    // auf 0..Breite-1 ein - exakt dasselbe Prinzip wie im Flightradar-
    // Projekt (dort bereits am echten Geraet bestaetigt).
    constexpr int16_t MARGIN_PX = 20;

    bool lastTouched = false;
    Point lastMapped;

    int16_t clampi(int16_t v, int16_t lo, int16_t hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }
}

void begin() {
    touchSpi.begin(Config::TOUCH_CLK_PIN, Config::TOUCH_MISO_PIN,
                    Config::TOUCH_MOSI_PIN, Config::TOUCH_CS_PIN);
    touch.begin(touchSpi);
    touch.setRotation(0);
}

Point rawPoint() {
    Point p;
    p.touched = touch.touched();
    if (p.touched) {
        TS_Point raw = touch.getPoint();
        p.x = raw.x;
        p.y = raw.y;
    }
    return p;
}

void setCalibration(bool swap, int16_t xmin, int16_t xmax, int16_t ymin, int16_t ymax) {
    swapXY = swap;
    xRawMin = xmin;
    xRawMax = xmax;
    yRawMin = ymin;
    yRawMax = ymax;
}

Point mappedPoint() {
    Point raw = rawPoint();
    Point out;
    out.touched = raw.touched;
    if (!raw.touched) return out;

    // Der Touch-Chip ist an die physische Panel-Ausrichtung gebunden, nicht
    // an tft.setRotation() - swapXY (aus der Kalibrierung, siehe
    // calibration_screen.cpp) legt fest, welcher Rohwert welcher
    // Bildschirmachse entspricht. map() funktioniert auch mit vertauschten
    // Min/Max-Werten korrekt (invertierte Achse).
    int16_t rawForX = swapXY ? raw.y : raw.x;
    int16_t rawForY = swapXY ? raw.x : raw.y;

    long mx = map((long)rawForX, xRawMin, xRawMax, MARGIN_PX, Config::SCREEN_WIDTH - 1 - MARGIN_PX);
    long my = map((long)rawForY, yRawMin, yRawMax, MARGIN_PX, Config::SCREEN_HEIGHT - 1 - MARGIN_PX);

    out.x = clampi((int16_t)mx, 0, Config::SCREEN_WIDTH - 1);
    out.y = clampi((int16_t)my, 0, Config::SCREEN_HEIGHT - 1);
    return out;
}

bool wasTapped(Point& outPoint) {
    Point cur = mappedPoint();

    bool fired = false;
    if (!cur.touched && lastTouched) {
        outPoint = lastMapped;
        fired = true;
    }

    lastTouched = cur.touched;
    if (cur.touched) lastMapped = cur;

    return fired;
}

}
