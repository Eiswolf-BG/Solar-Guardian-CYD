#include "help_popup.h"
#include "display.h"
#include "touch_input.h"
#include "i18n.h"
#include "config.h"
#include "web_server.h"

namespace HelpPopup {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr int16_t WRAP_WIDTH_PX = 210; // bei 20px Rand links/rechts auf 240px Breite

    // Wortweiser Umbruch nach echter Pixelbreite (nicht nach fester
    // Zeichenzahl) - gleiches Prinzip wie layoutWrapped() im
    // Flightradar-Projekt, hier bewusst einfacher, da die Hilfetexte immer
    // kurz sind (2-4 Zeilen).
    uint8_t wrapLines(const char* text, String* outLines, uint8_t maxLines) {
        String remaining = text;
        uint8_t lineCount = 0;
        while (remaining.length() > 0 && lineCount < maxLines) {
            int16_t lastGoodSpace = -1;
            int16_t i = 0;
            String line = "";
            while (i < (int16_t)remaining.length()) {
                char c = remaining[i];
                String candidate = line + c;
                if (tft.textWidth(candidate) > WRAP_WIDTH_PX) break;
                line = candidate;
                if (c == ' ') lastGoodSpace = i;
                i++;
            }
            if (i >= (int16_t)remaining.length()) {
                // Rest passt komplett in diese Zeile.
                outLines[lineCount++] = line;
                remaining = "";
                break;
            }
            if (lastGoodSpace >= 0) {
                outLines[lineCount++] = remaining.substring(0, lastGoodSpace);
                remaining = remaining.substring(lastGoodSpace + 1);
            } else {
                // Kein Leerzeichen in Reichweite (sehr langes Wort) - hart
                // an der Pixelbreite trennen, damit nichts abgeschnitten
                // vom Bildschirm laeuft.
                outLines[lineCount++] = line;
                remaining = remaining.substring(line.length());
            }
        }
        return lineCount;
    }
}

void drawButton(int16_t x, int16_t y, int16_t w, int16_t h) {
    uint16_t accent = Display::systemColor();
    tft.drawRoundRect(x, y, w, h, 4, accent);
    tft.setTextColor(accent, TFT_BLACK);
    // Immer explizit Size 1 setzen statt sich auf den Zustand des
    // Aufrufers zu verlassen - eine Seite, die vorher mit Size 2
    // gezeichnet hat (siehe home_page.cpp-Bugfix), liess das "?" sonst
    // ueberdimensioniert/ausgefranst wirken (CLAUDE.md "Schriftzustand
    // nie vom Aufrufer erben").
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("?", (int16_t)(x + w / 2), (int16_t)(y + h / 2));
    tft.setTextDatum(TL_DATUM);
}

void show(const char* body) {
    // Explizit Size 1 setzen statt sich auf die zuletzt vom Aufrufer
    // gesetzte Groesse zu verlassen - WRAP_WIDTH_PX ist auf Size 1
    // ausgelegt, ein Aufrufer mit zuletzt gesetzter Size 2/3 (z.B. nach
    // einer Zahlen-Kachel) wuerde sonst falsch/zu frueh umbrechen.
    tft.setTextSize(1);

    // MAX_LINES bewusst grosszuegig (langere Hilfetexte, z.B. Seite
    // "Batterie" mit allen vier Ladestatus-Erklaerungen, siehe CLAUDE.md
    // "Hilfe-Knoepfe") - bei bis zu 10 Zeilen bleibt trotz zentrierter
    // Anzeige genug Rand zum Bildschirmrand (siehe Rechnung im Auftrag).
    constexpr uint8_t MAX_LINES = 10;
    String lines[MAX_LINES];
    uint8_t lineCount = wrapLines(body, lines, MAX_LINES);

    uint16_t accent = Display::systemColor();
    tft.fillScreen(TFT_BLACK);

    constexpr int16_t LINE_H = 22;
    int16_t totalTextH = (int16_t)(lineCount * LINE_H);
    int16_t startY = (int16_t)(Config::SCREEN_HEIGHT / 2 - totalTextH / 2 - 20);

    tft.setTextColor(accent, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    for (uint8_t i = 0; i < lineCount; i++) {
        tft.drawString(lines[i], Config::SCREEN_WIDTH / 2, (int16_t)(startY + i * LINE_H));
    }
    tft.setTextDatum(TL_DATUM);

    Rect okBtn = {(int16_t)(Config::SCREEN_WIDTH / 2 - 50), (int16_t)(startY + totalTextH + 20), 100, 40};
    tft.fillRoundRect(okBtn.x, okBtn.y, okBtn.w, okBtn.h, 4, accent);
    tft.setTextColor(TFT_BLACK, accent);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(I18n::t(I18n::Str::OK_BUTTON), okBtn.x + okBtn.w / 2, okBtn.y + okBtn.h / 2);
    tft.setTextDatum(TL_DATUM);

    while (true) {
        TouchInput::Point tap;
        if (TouchInput::wasTapped(tap)) {
            if (okBtn.contains(tap.x, tap.y)) break;
        }
        WebServer_::tick();
        delay(15);
    }
}

}
