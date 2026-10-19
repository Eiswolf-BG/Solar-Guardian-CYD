#include "layout_check.h"
#include "config.h"

namespace LayoutCheck {

namespace {
    struct Entry {
        const char* label;
        int16_t x, y, w, h;
    };
    constexpr uint8_t MAX_ENTRIES = 32;
    Entry entries[MAX_ENTRIES];
    uint8_t count = 0;
    const char* currentScreen = "";

    bool overlap(const Entry& a, const Entry& b) {
        return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
    }
}

void begin(const char* screenName) {
    if (!ENABLED) return;
    currentScreen = screenName;
    count = 0;
}

void rect(const char* label, int16_t x, int16_t y, int16_t w, int16_t h) {
    if (!ENABLED) return;
    if (count >= MAX_ENTRIES) return;
    entries[count++] = {label, x, y, w, h};
}

void finish() {
    if (!ENABLED) return;
    for (uint8_t i = 0; i < count; i++) {
        const Entry& e = entries[i];
        if (e.x < 0 || e.y < 0 || e.x + e.w > Config::SCREEN_WIDTH || e.y + e.h > Config::SCREEN_HEIGHT) {
            Serial.printf("[layout] Konflikt in %s: %s ragt ueber den Bildschirmrand (x=%d y=%d w=%d h=%d)\n",
                          currentScreen, e.label, e.x, e.y, e.w, e.h);
        }
    }
    for (uint8_t i = 0; i < count; i++) {
        for (uint8_t j = i + 1; j < count; j++) {
            if (overlap(entries[i], entries[j])) {
                Serial.printf("[layout] Konflikt in %s: %s ueberlappt %s\n",
                              currentScreen, entries[i].label, entries[j].label);
            }
        }
    }
}

void containedRect(const char* label, int16_t frameX, int16_t frameY, int16_t frameW, int16_t frameH,
                    int16_t childX, int16_t childY, int16_t childW, int16_t childH) {
    if (!ENABLED) return;
    constexpr int16_t MARGIN = 4;
    int16_t innerX = (int16_t)(frameX + MARGIN);
    int16_t innerY = (int16_t)(frameY + MARGIN);
    int16_t innerRight = (int16_t)(frameX + frameW - MARGIN);
    int16_t innerBottom = (int16_t)(frameY + frameH - MARGIN);
    bool fits = childX >= innerX && childY >= innerY &&
                (childX + childW) <= innerRight && (childY + childH) <= innerBottom;
    if (!fits) {
        Serial.printf("[layout] Konflikt in %s: %s beruehrt/ueberragt den eigenen Rahmen "
                      "(Rahmen x=%d y=%d w=%d h=%d, Text x=%d y=%d w=%d h=%d)\n",
                      currentScreen, label, frameX, frameY, frameW, frameH, childX, childY, childW, childH);
    }
}

void bufferRect(const char* label, int16_t bufferW, int16_t bufferH, int16_t textW, int16_t textH) {
    if (!ENABLED) return;
    if (textW > bufferW || textH > bufferH) {
        Serial.printf("[layout] Konflikt in %s: %s ist groesser als sein Off-Screen-Puffer "
                      "(Puffer w=%d h=%d, Text w=%d h=%d) - wird abgeschnitten\n",
                      currentScreen, label, bufferW, bufferH, textW, textH);
    }
}

}
