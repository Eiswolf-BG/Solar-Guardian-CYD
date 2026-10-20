#include "files_screen.h"
#include "display.h"
#include "touch_input.h"
#include "sd_storage.h"
#include "help_popup.h"
#include "layout_check.h"
#include "layout_utils.h"
#include "i18n.h"
#include "config.h"
#include <SD.h>

namespace FilesScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr Rect HELP_BTN = {(int16_t)(Config::SCREEN_WIDTH - 36), 4, 32, 32};
    constexpr uint8_t MAX_ENTRIES = 100;
    constexpr uint8_t VISIBLE_ROWS = 6;
    constexpr int16_t ROW_H = 30;
    constexpr int16_t LIST_TOP = 46;
    constexpr uint8_t ONE_SIZE[] = {1};
    constexpr uint8_t TWO_SIZES[] = {2, 1};

    struct Entry {
        String name;
        bool isDir;
        uint32_t size;
    };

    // /SolarHistory ist der letzte gesicherte Stand der Anlage - HIER
    // zentral geschuetzt (nicht nur in der Oberflaeche, Auftrag), damit
    // auch spaetere Aenderungen den Schutz nicht versehentlich umgehen.
    bool isProtectedPath(const String& fullPath) {
        return fullPath == Config::SD_HISTORY_DIR ||
               fullPath.startsWith(String(Config::SD_HISTORY_DIR) + "/");
    }

    String formatSize(uint32_t bytes) {
        char buf[20];
        if (bytes < 1024UL) {
            snprintf(buf, sizeof(buf), "%u B", (unsigned)bytes);
        } else if (bytes < 1024UL * 1024UL) {
            snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0f);
        } else {
            snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0f * 1024.0f));
        }
        return String(buf);
    }

    int compareEntries(const Entry& a, const Entry& b) {
        if (a.isDir != b.isDir) return a.isDir ? -1 : 1; // Ordner zuerst
        return strcasecmp(a.name.c_str(), b.name.c_str());
    }

    // Liest bis zu MAX_ENTRIES Eintraege von dirPath (eine Ebene, keine
    // Rekursion), sortiert Ordner zuerst, dann alphabetisch. Laeuft nur
    // auf Core 1 (Menue), kein delay(), kurze Laufzeit (max. 100
    // Eintraege). true = Ordner lesbar.
    bool listDir(const String& dirPath, Entry* out, uint8_t& outCount) {
        outCount = 0;
        File dir = SD.open(dirPath);
        if (!dir || !dir.isDirectory()) {
            if (dir) dir.close();
            return false;
        }

        File entry = dir.openNextFile();
        while (entry && outCount < MAX_ENTRIES) {
            String name = entry.name(); // nur Basisname, kein Pfad
            bool isDir = entry.isDirectory();
            uint32_t size = isDir ? 0 : (uint32_t)entry.size();
            entry.close();
            if (name.length() > 0) {
                out[outCount].name = name;
                out[outCount].isDir = isDir;
                out[outCount].size = size;
                outCount++;
            }
            entry = dir.openNextFile();
        }
        dir.close();

        // Einfacher Insertion-Sort - max. 100 Eintraege, Aufruf selten
        // (beim Oeffnen/Wechseln des Ordners), kein Grund fuer mehr.
        for (uint8_t i = 1; i < outCount; i++) {
            Entry key = out[i];
            int16_t j = (int16_t)(i - 1);
            while (j >= 0 && compareEntries(out[j], key) > 0) {
                out[j + 1] = out[j];
                j--;
            }
            out[j + 1] = key;
        }
        return true;
    }

    void showToast(const char* msg, uint16_t color) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(color, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(msg, Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT / 2);
        tft.setTextDatum(TL_DATUM);
        delay(900);
    }

    String joinPath(const String& dir, const String& name) {
        return (dir == "/") ? ("/" + name) : (dir + "/" + name);
    }
}

void run() {
    String currentPath = "/";
    static Entry entries[MAX_ENTRIES];
    uint8_t entryCount = 0;
    uint8_t scrollOffset = 0;
    bool sdOk = true;

    auto reload = [&]() {
        sdOk = SdStorage::isMounted() && listDir(currentPath, entries, entryCount);
        scrollOffset = 0;
    };
    reload();

    bool done = false;
    while (!done) {
        uint16_t accent = Display::systemColor();
        LayoutCheck::begin("Dateien");
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(I18n::t(I18n::Str::FILES_TITLE), Config::SCREEN_WIDTH / 2, 14);
        tft.setTextDatum(TL_DATUM);
        HelpPopup::drawButton(HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);
        LayoutCheck::rect("Titel", 0, 4, (int16_t)(Config::SCREEN_WIDTH - 44), 24);
        LayoutCheck::rect("Hilfe-Knopf", HELP_BTN.x, HELP_BTN.y, HELP_BTN.w, HELP_BTN.h);

        // Aktueller Pfad klein darunter (zur Orientierung).
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        LayoutUtils::FitResult pathFit = LayoutUtils::fitText(tft, currentPath, (int16_t)(Config::SCREEN_WIDTH - 16), 14, ONE_SIZE, 1);
        tft.drawString(pathFit.text, Config::SCREEN_WIDTH / 2, 30);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        {
            int16_t w = tft.textWidth(pathFit.text);
            LayoutCheck::rect("Pfad", (int16_t)(Config::SCREEN_WIDTH / 2 - w / 2), (int16_t)(30 - tft.fontHeight()), w, tft.fontHeight());
        }
        tft.setTextSize(1);

        Rect rowRects[VISIBLE_ROWS];
        uint8_t visibleCount = 0;

        if (!sdOk) {
            tft.setTextDatum(MC_DATUM);
            tft.drawString(I18n::t(I18n::Str::FILES_NO_SD), Config::SCREEN_WIDTH / 2, (int16_t)(LIST_TOP + 60));
            tft.setTextDatum(TL_DATUM);
        } else if (entryCount == 0) {
            tft.setTextDatum(MC_DATUM);
            tft.drawString(I18n::t(I18n::Str::FILES_EMPTY), Config::SCREEN_WIDTH / 2, (int16_t)(LIST_TOP + 60));
            tft.setTextDatum(TL_DATUM);
        } else {
            visibleCount = (uint8_t)((entryCount - scrollOffset) < VISIBLE_ROWS ? (entryCount - scrollOffset) : VISIBLE_ROWS);
            for (uint8_t i = 0; i < visibleCount; i++) {
                uint8_t idx = (uint8_t)(scrollOffset + i);
                Rect r = {4, (int16_t)(LIST_TOP + i * ROW_H), (int16_t)(Config::SCREEN_WIDTH - 8), (int16_t)(ROW_H - 4)};
                rowRects[i] = r;

                String fullPath = joinPath(currentPath, entries[idx].name);
                bool protectedEntry = isProtectedPath(fullPath);
                uint16_t rowColor = protectedEntry ? TFT_DARKGREY : accent;
                tft.drawRoundRect(r.x, r.y, r.w, r.h, 3, rowColor);

                String sizeLabel;
                if (entries[idx].isDir) {
                    sizeLabel = "";
                } else if (protectedEntry) {
                    sizeLabel = String("[") + I18n::t(I18n::Str::FILES_PROTECTED_HINT) + "]";
                } else {
                    sizeLabel = formatSize(entries[idx].size);
                }
                tft.setTextSize(1);
                int16_t sizeW = sizeLabel.length() ? tft.textWidth(sizeLabel) : 0;

                String nameLabel = entries[idx].isDir ? (entries[idx].name + "/") : entries[idx].name;
                int16_t nameMaxW = (int16_t)(r.w - 16 - sizeW - (sizeW > 0 ? 8 : 0));
                LayoutUtils::FitResult nameFit = LayoutUtils::fitText(tft, nameLabel, nameMaxW, (int16_t)(r.h - 4), ONE_SIZE, 1);

                tft.setTextColor(rowColor, TFT_BLACK);
                tft.setTextSize(1);
                tft.setCursor((int16_t)(r.x + 6), (int16_t)(r.y + r.h / 2 + 4));
                tft.print(nameFit.text);
                if (sizeLabel.length()) {
                    tft.setCursor((int16_t)(r.x + r.w - 6 - sizeW), (int16_t)(r.y + r.h / 2 + 4));
                    tft.print(sizeLabel);
                }
                tft.setTextColor(accent, TFT_BLACK);
                LayoutCheck::rect("Dateizeile", r.x, r.y, r.w, r.h);
            }
        }

        bool canScrollUp = scrollOffset > 0;
        bool canScrollDown = (uint8_t)(scrollOffset + VISIBLE_ROWS) < entryCount;
        Rect upBtn = {(int16_t)(Config::SCREEN_WIDTH - 40), (int16_t)(LIST_TOP + VISIBLE_ROWS * ROW_H + 4), 36, 28};
        Rect downBtn = {4, upBtn.y, 36, 28};
        if (canScrollUp || canScrollDown) {
            tft.drawRoundRect(downBtn.x, downBtn.y, downBtn.w, downBtn.h, 3, canScrollUp ? accent : TFT_DARKGREY);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("<", (int16_t)(downBtn.x + downBtn.w / 2), (int16_t)(downBtn.y + downBtn.h / 2));
            tft.drawRoundRect(upBtn.x, upBtn.y, upBtn.w, upBtn.h, 3, canScrollDown ? accent : TFT_DARKGREY);
            tft.drawString(">", (int16_t)(upBtn.x + upBtn.w / 2), (int16_t)(upBtn.y + upBtn.h / 2));
            tft.setTextDatum(TL_DATUM);
            LayoutCheck::rect("Scroll-runter", downBtn.x, downBtn.y, downBtn.w, downBtn.h);
            LayoutCheck::rect("Scroll-hoch", upBtn.x, upBtn.y, upBtn.w, upBtn.h);
        }

        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 48), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        tft.drawRoundRect(backBtn.x, backBtn.y, backBtn.w, backBtn.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::BACK), (int16_t)(backBtn.x + backBtn.w / 2), (int16_t)(backBtn.y + backBtn.h / 2));
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Zurueck-Knopf", backBtn.x, backBtn.y, backBtn.w, backBtn.h);
        LayoutCheck::finish();

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            delay(15);
        }

        if (HELP_BTN.contains(tap.x, tap.y)) {
            HelpPopup::show(I18n::t(I18n::Str::HELP_BODY_FILES));
            continue;
        }
        if (backBtn.contains(tap.x, tap.y)) {
            if (currentPath != "/") {
                currentPath = "/";
                reload();
            } else {
                done = true;
            }
            continue;
        }
        if (canScrollUp && downBtn.contains(tap.x, tap.y)) {
            scrollOffset = (scrollOffset > VISIBLE_ROWS) ? (uint8_t)(scrollOffset - VISIBLE_ROWS) : 0;
            continue;
        }
        if (canScrollDown && upBtn.contains(tap.x, tap.y)) {
            scrollOffset = (uint8_t)(scrollOffset + VISIBLE_ROWS);
            continue;
        }

        int8_t tappedRow = -1;
        for (uint8_t i = 0; i < visibleCount; i++) {
            if (rowRects[i].contains(tap.x, tap.y)) { tappedRow = (int8_t)i; break; }
        }
        if (tappedRow < 0) continue;

        uint8_t idx = (uint8_t)(scrollOffset + tappedRow);
        Entry entry = entries[idx]; // Kopie - entries[] kann durch reload() unter uns veraendert werden
        String fullPath = joinPath(currentPath, entry.name);

        if (entry.isDir) {
            // Eine Ebene tiefer reicht (Auftrag) - nur vom Wurzelverzeichnis
            // aus wird geoeffnet, ein Ordner innerhalb eines Ordners wird
            // nur angezeigt, nicht weiter geoeffnet.
            if (currentPath == "/") {
                currentPath = fullPath;
                reload();
            }
            continue;
        }

        if (isProtectedPath(fullPath)) {
            showToast(I18n::t(I18n::Str::FILES_PROTECTED_HINT), accent);
            continue;
        }

        // Bestaetigungsdialog.
        char confirmMsg[96];
        snprintf(confirmMsg, sizeof(confirmMsg), I18n::t(I18n::Str::FILES_DELETE_CONFIRM_FMT), entry.name.c_str());

        LayoutCheck::begin("Dateien-Loeschen");
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(1); // nie vom Aufrufer erben, siehe CLAUDE.md "Schriftzustand"
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        LayoutUtils::FitResult confirmFit = LayoutUtils::fitText(tft, String(confirmMsg), (int16_t)(Config::SCREEN_WIDTH - 20), 44, TWO_SIZES, 2);
        tft.drawString(confirmFit.text, Config::SCREEN_WIDTH / 2, 90);
        tft.setTextDatum(TL_DATUM);
        {
            int16_t w = tft.textWidth(confirmFit.text);
            LayoutCheck::rect("Loesch-Frage", (int16_t)(Config::SCREEN_WIDTH / 2 - w / 2), (int16_t)(90 - tft.fontHeight()), w, tft.fontHeight());
        }
        tft.setTextSize(1);

        Rect cancelBtn = {10, 170, (int16_t)(Config::SCREEN_WIDTH - 20), 44};
        Rect deleteBtn = {10, 224, (int16_t)(Config::SCREEN_WIDTH - 20), 44};
        tft.drawRoundRect(cancelBtn.x, cancelBtn.y, cancelBtn.w, cancelBtn.h, 4, accent);
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(I18n::Str::CANCEL), (int16_t)(cancelBtn.x + cancelBtn.w / 2), (int16_t)(cancelBtn.y + cancelBtn.h / 2));
        tft.drawRoundRect(deleteBtn.x, deleteBtn.y, deleteBtn.w, deleteBtn.h, 4, TFT_RED);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString(I18n::t(I18n::Str::DELETE_BUTTON), (int16_t)(deleteBtn.x + deleteBtn.w / 2), (int16_t)(deleteBtn.y + deleteBtn.h / 2));
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        LayoutCheck::rect("Abbrechen-Knopf", cancelBtn.x, cancelBtn.y, cancelBtn.w, cancelBtn.h);
        LayoutCheck::rect("Loeschen-Knopf", deleteBtn.x, deleteBtn.y, deleteBtn.w, deleteBtn.h);
        LayoutCheck::finish();

        TouchInput::Point confirmTap;
        while (true) {
            if (TouchInput::wasTapped(confirmTap)) break;
            delay(15);
        }

        if (deleteBtn.contains(confirmTap.x, confirmTap.y)) {
            // Schutz nochmal hier (nicht nur weiter oben) - zentrale
            // Absicherung direkt am eigentlichen Loeschvorgang (Auftrag).
            bool ok = !isProtectedPath(fullPath) && SD.remove(fullPath);
            if (!ok) showToast(I18n::t(I18n::Str::FILES_DELETE_FAILED), TFT_RED);
            reload();
        }
        // "Abbrechen" oder sonstiger Tipp: einfach zur Liste zurueck.
    }
}

}
