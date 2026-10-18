#include "layout_utils.h"

namespace LayoutUtils {

namespace {
    constexpr int16_t PAD = 4; // Mindest-Innenabstand je Seite, siehe CLAUDE.md
}

FitResult fitText(TFT_eSPI& tft, const String& text, int16_t maxW, int16_t maxH,
                   const uint8_t* sizes, uint8_t sizeCount) {
    int16_t availW = maxW - 2 * PAD;
    int16_t availH = maxH - 2 * PAD;
    if (availW < 1) availW = 1;
    if (availH < 1) availH = 1;

    for (uint8_t i = 0; i < sizeCount; i++) {
        tft.setTextSize(sizes[i]);
        if (tft.textWidth(text) <= availW && tft.fontHeight() <= availH) {
            return {sizes[i], text};
        }
    }

    // Selbst die kleinste Groesse passt nicht - kuerzen und "..." anhaengen.
    uint8_t smallest = sizes[sizeCount - 1];
    tft.setTextSize(smallest);
    String truncated = text;
    while (truncated.length() > 0 && tft.textWidth(truncated + "...") > availW) {
        truncated.remove(truncated.length() - 1);
    }
    return {smallest, truncated + "..."};
}

}
