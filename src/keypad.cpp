#include "keypad.h"
#include "display.h"
#include "touch_input.h"
#include "i18n.h"
#include "config.h"
#include <cctype>

namespace Keypad {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    enum class KeyType { Char, Backspace };

    struct KeyDef {
        KeyType type;
        char ch; // nur fuer KeyType::Char relevant
    };

    // Deutsches QWERTZ-Layout (Z/Y vertauscht gegenueber englischem QWERTY).
    constexpr KeyDef LETTERS_ROW1[] = {
        {KeyType::Char,'q'},{KeyType::Char,'w'},{KeyType::Char,'e'},{KeyType::Char,'r'},
        {KeyType::Char,'t'},{KeyType::Char,'z'},{KeyType::Char,'u'},{KeyType::Char,'i'},
        {KeyType::Char,'o'},{KeyType::Char,'p'},
    };
    constexpr KeyDef LETTERS_ROW2[] = {
        {KeyType::Char,'a'},{KeyType::Char,'s'},{KeyType::Char,'d'},{KeyType::Char,'f'},
        {KeyType::Char,'g'},{KeyType::Char,'h'},{KeyType::Char,'j'},{KeyType::Char,'k'},
        {KeyType::Char,'l'},
    };
    // @ und . sitzen bewusst auf der festen Aktionsreihe (siehe ACTION ROW
    // unten), nicht hier - sie muessen fuer E-Mail-Adressen auf JEDER Ebene
    // sofort erreichbar sein, nicht erst nach einem Ebenenwechsel.
    constexpr KeyDef LETTERS_ROW3[] = {
        {KeyType::Char,'y'},{KeyType::Char,'x'},{KeyType::Char,'c'},{KeyType::Char,'v'},
        {KeyType::Char,'b'},{KeyType::Char,'n'},{KeyType::Char,'m'},{KeyType::Backspace,0},
    };

    constexpr KeyDef SYMBOLS_ROW1[] = {
        {KeyType::Char,'1'},{KeyType::Char,'2'},{KeyType::Char,'3'},{KeyType::Char,'4'},
        {KeyType::Char,'5'},{KeyType::Char,'6'},{KeyType::Char,'7'},{KeyType::Char,'8'},
        {KeyType::Char,'9'},{KeyType::Char,'0'},
    };
    constexpr KeyDef SYMBOLS_ROW2[] = {
        {KeyType::Char,'-'},{KeyType::Char,'_'},{KeyType::Char,'='},{KeyType::Char,'+'},
        {KeyType::Char,':'},{KeyType::Char,';'},{KeyType::Char,'\''},{KeyType::Char,'"'},
        {KeyType::Char,'?'},{KeyType::Char,'!'},
    };
    // @ und . fehlen hier bewusst (stehen bereits auf der Aktionsreihe) -
    // die uebrigen Sonderzeichen bleiben wie gewohnt auf dieser Ebene.
    constexpr KeyDef SYMBOLS_ROW3[] = {
        {KeyType::Char,'#'},{KeyType::Char,'$'},{KeyType::Char,'%'},{KeyType::Char,'&'},
        {KeyType::Char,'*'},{KeyType::Char,'('},{KeyType::Char,')'},{KeyType::Backspace,0},
    };

    constexpr int16_t KB_TOP   = 64;
    constexpr int16_t ROW_H    = 38;
    constexpr int16_t ROW_GAP  = 6;
    constexpr int16_t KEY_GAP  = 3;
    constexpr int16_t SIDE_MARGIN = 4;
    constexpr int16_t FIELD_Y = 30;
    constexpr int16_t FIELD_H = 28;

    void layoutRow(Rect* out, uint8_t count, int16_t y) {
        int16_t usableW = Config::SCREEN_WIDTH - 2 * SIDE_MARGIN;
        int16_t keyW = (usableW - (count - 1) * KEY_GAP) / count;
        int16_t x = SIDE_MARGIN;
        for (uint8_t i = 0; i < count; i++) {
            out[i] = {x, y, keyW, ROW_H};
            x += keyW + KEY_GAP;
        }
    }

    void drawKeyBox(const Rect& r, const String& label, bool active = false) {
        uint16_t accent = Display::systemColor();
        if (active) {
            tft.fillRoundRect(r.x, r.y, r.w, r.h, 3, accent);
            tft.setTextColor(TFT_BLACK, accent);
        } else {
            tft.fillRoundRect(r.x, r.y, r.w, r.h, 3, TFT_BLACK);
            tft.drawRoundRect(r.x, r.y, r.w, r.h, 3, accent);
            tft.setTextColor(accent, TFT_BLACK);
        }
        tft.setTextDatum(MC_DATUM);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    String labelFor(const KeyDef& k, bool shiftOn, bool lettersPage) {
        if (k.type == KeyType::Backspace) return "<-";
        char c = k.ch;
        if (lettersPage && shiftOn) c = (char)toupper((unsigned char)c);
        return String(c);
    }
}

String run(const char* title, const String& initialText, bool& confirmed, bool startOnNumbersPage) {
    char buf[96] = {0};
    strncpy(buf, initialText.c_str(), sizeof(buf) - 1);
    uint8_t len = (uint8_t)strlen(buf);

    bool lettersPage = !startOnNumbersPage;
    bool shiftOn = false; // Einmal-Shift (One-Shot) - startet immer klein, siehe CLAUDE.md
    bool done = false;
    confirmed = false;

    int16_t row1Y = KB_TOP;
    int16_t row2Y = (int16_t)(row1Y + ROW_H + ROW_GAP);
    int16_t row3Y = (int16_t)(row2Y + ROW_H + ROW_GAP);
    int16_t row4Y = (int16_t)(row3Y + ROW_H + ROW_GAP);

    Rect row1Rects[10], row2Rects[10], row3Rects[8];

    // Oben rechts: kleiner "X"-Knopf zum Abbrechen - so bleibt die
    // Aktionsreihe unten frei fuer die 6 wichtigen, immer erreichbaren
    // Tasten (Alex' Wunsch: Shift, Ebenen-Umschalter, @, Leertaste, Punkt,
    // OK).
    Rect closeBtn = {(int16_t)(Config::SCREEN_WIDTH - 28), 2, 24, 24};

    // Aktionsreihe: feste Breiten, die zusammen auf die 240px-
    // Bildschirmbreite (Hochformat) passen.
    Rect shiftBtn  = {SIDE_MARGIN, row4Y, 32, 40};
    Rect toggleBtn = {(int16_t)(shiftBtn.x + shiftBtn.w + KEY_GAP), row4Y, 32, 40};
    Rect atBtn     = {(int16_t)(toggleBtn.x + toggleBtn.w + KEY_GAP), row4Y, 32, 40};
    Rect spaceBtn  = {(int16_t)(atBtn.x + atBtn.w + KEY_GAP), row4Y, 45, 40};
    Rect dotBtn    = {(int16_t)(spaceBtn.x + spaceBtn.w + KEY_GAP), row4Y, 32, 40};
    Rect confirmBtn = {(int16_t)(dotBtn.x + dotBtn.w + KEY_GAP), row4Y, 44, 40};

    auto redraw = [&]() {
        tft.fillScreen(TFT_BLACK);
        uint16_t accent = Display::systemColor();
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextSize(1);
        tft.setCursor(SIDE_MARGIN, 12);
        tft.print(title);

        tft.drawRoundRect(closeBtn.x, closeBtn.y, closeBtn.w, closeBtn.h, 3, TFT_RED);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("X", closeBtn.x + closeBtn.w / 2, closeBtn.y + closeBtn.h / 2);
        tft.setTextDatum(TL_DATUM);

        tft.drawRoundRect(SIDE_MARGIN, FIELD_Y, Config::SCREEN_WIDTH - 2 * SIDE_MARGIN, FIELD_H, 3, accent);
        tft.setTextColor(accent, TFT_BLACK);
        tft.setCursor((int16_t)(SIDE_MARGIN + 6), (int16_t)(FIELD_Y + 20));
        tft.print(buf);

        const KeyDef* r1 = lettersPage ? LETTERS_ROW1 : SYMBOLS_ROW1;
        const KeyDef* r2 = lettersPage ? LETTERS_ROW2 : SYMBOLS_ROW2;
        const KeyDef* r3 = lettersPage ? LETTERS_ROW3 : SYMBOLS_ROW3;
        uint8_t n1 = 10, n2 = lettersPage ? 9 : 10, n3 = 8;

        layoutRow(row1Rects, n1, row1Y);
        layoutRow(row2Rects, n2, row2Y);
        layoutRow(row3Rects, n3, row3Y);

        for (uint8_t i = 0; i < n1; i++) drawKeyBox(row1Rects[i], labelFor(r1[i], shiftOn, lettersPage));
        for (uint8_t i = 0; i < n2; i++) drawKeyBox(row2Rects[i], labelFor(r2[i], shiftOn, lettersPage));
        for (uint8_t i = 0; i < n3; i++) drawKeyBox(row3Rects[i], labelFor(r3[i], shiftOn, lettersPage));

        // Shift wirkt nur auf Buchstaben - auf der Zahlen-/Sonderzeichen-
        // Ebene bleibt der Knopf sichtbar, aber ohne Wirkung (keine
        // Gross-/Kleinform fuer Ziffern/Symbole).
        drawKeyBox(shiftBtn, shiftOn ? "ABC" : "abc", lettersPage && shiftOn);
        drawKeyBox(toggleBtn, lettersPage ? "123" : "ABC");
        drawKeyBox(atBtn, "@");
        drawKeyBox(spaceBtn, I18n::t(I18n::Str::SPACE_KEY_LABEL));
        drawKeyBox(dotBtn, ".");
        drawKeyBox(confirmBtn, "OK", true);
    };

    redraw();

    while (!done) {
        TouchInput::Point tap;
        if (!TouchInput::wasTapped(tap)) { delay(15); continue; }

        bool handled = false;
        bool needsRedraw = false;

        const KeyDef* r1 = lettersPage ? LETTERS_ROW1 : SYMBOLS_ROW1;
        const KeyDef* r2 = lettersPage ? LETTERS_ROW2 : SYMBOLS_ROW2;
        const KeyDef* r3 = lettersPage ? LETTERS_ROW3 : SYMBOLS_ROW3;
        uint8_t n1 = 10, n2 = lettersPage ? 9 : 10, n3 = 8;

        auto handleKey = [&](const KeyDef& k) {
            switch (k.type) {
                case KeyType::Char: {
                    if (len < sizeof(buf) - 1) {
                        char c = k.ch;
                        if (lettersPage && shiftOn) c = (char)toupper((unsigned char)c);
                        buf[len++] = c;
                        buf[len] = 0;
                    }
                    if (lettersPage && shiftOn) shiftOn = false; // Einmal-Shift verbraucht
                    break;
                }
                case KeyType::Backspace:
                    if (len > 0) { len--; buf[len] = 0; }
                    break;
            }
        };

        for (uint8_t i = 0; i < n1 && !handled; i++) {
            if (row1Rects[i].contains(tap.x, tap.y)) { handleKey(r1[i]); handled = true; needsRedraw = true; }
        }
        for (uint8_t i = 0; i < n2 && !handled; i++) {
            if (row2Rects[i].contains(tap.x, tap.y)) { handleKey(r2[i]); handled = true; needsRedraw = true; }
        }
        for (uint8_t i = 0; i < n3 && !handled; i++) {
            if (row3Rects[i].contains(tap.x, tap.y)) { handleKey(r3[i]); handled = true; needsRedraw = true; }
        }
        if (!handled && shiftBtn.contains(tap.x, tap.y)) {
            if (lettersPage) shiftOn = !shiftOn;
            handled = true; needsRedraw = true;
        }
        if (!handled && toggleBtn.contains(tap.x, tap.y)) {
            lettersPage = !lettersPage;
            shiftOn = false;
            handled = true; needsRedraw = true;
        }
        if (!handled && atBtn.contains(tap.x, tap.y)) {
            if (len < sizeof(buf) - 1) { buf[len++] = '@'; buf[len] = 0; }
            handled = true; needsRedraw = true;
        }
        if (!handled && spaceBtn.contains(tap.x, tap.y)) {
            if (len < sizeof(buf) - 1) { buf[len++] = ' '; buf[len] = 0; }
            handled = true; needsRedraw = true;
        }
        if (!handled && dotBtn.contains(tap.x, tap.y)) {
            if (len < sizeof(buf) - 1) { buf[len++] = '.'; buf[len] = 0; }
            handled = true; needsRedraw = true;
        }
        if (!handled && closeBtn.contains(tap.x, tap.y)) {
            confirmed = false;
            done = true;
            handled = true;
        }
        if (!handled && confirmBtn.contains(tap.x, tap.y)) {
            confirmed = true;
            done = true;
            handled = true;
        }

        if (needsRedraw) redraw();
    }

    return confirmed ? String(buf) : initialText;
}

}
