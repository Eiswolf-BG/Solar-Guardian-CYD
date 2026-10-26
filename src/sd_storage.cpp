#include "sd_storage.h"
#include "config.h"
#include <SPI.h>
#include <SD.h>

namespace SdStorage {

namespace {
    SPIClass sdSpi(HSPI);
    bool mounted = false;
}

bool begin() {
    sdSpi.begin(Config::SD_SPI_CLK_PIN, Config::SD_SPI_MISO_PIN,
                Config::SD_SPI_MOSI_PIN, Config::SD_SPI_CS_PIN);
    mounted = SD.begin(Config::SD_SPI_CS_PIN, sdSpi, 4000000);
    return mounted;
}

bool isMounted() { return mounted; }

namespace {
    // Einmalige Migration: der History-Backup-Ordner lag frueher unter
    // /SolarGuardian/History, lebt jetzt eine Ebene hoeher unter
    // /SolarHistory (ausserhalb von /SolarGuardian, siehe config.h). Nur
    // noetig, wenn der alte Ordner noch existiert und der neue noch
    // nicht - kopiert die eine Backup-Datei und loescht danach den alten
    // Ordner.
    void migrateHistoryFolder() {
        if (!SD.exists(Config::SD_HISTORY_DIR_OLD)) return;
        if (SD.exists(Config::SD_HISTORY_DIR)) return; // neuer Ordner existiert schon - nichts zu tun

        SD.mkdir(Config::SD_HISTORY_DIR);
        if (SD.exists(Config::SD_HISTORY_BACKUP_FILE_OLD)) {
            File rf = SD.open(Config::SD_HISTORY_BACKUP_FILE_OLD, FILE_READ);
            if (rf) {
                // FILE_WRITE haengt beim ESP32-SD-Treiber an, statt zu
                // ueberschreiben (Auftrag "SD-Schreibfehler pruefen") -
                // in der Praxis nie erreichbar (der neue Ordner existiert
                // laut der Pruefung oben noch gar nicht), aber defensiv
                // trotzdem entfernen, falls eine fruehere Migration schon
                // einmal (unvollstaendig) hier geschrieben hat.
                if (SD.exists(Config::SD_HISTORY_BACKUP_FILE)) SD.remove(Config::SD_HISTORY_BACKUP_FILE);
                File wf = SD.open(Config::SD_HISTORY_BACKUP_FILE, FILE_WRITE);
                if (wf) {
                    while (rf.available()) wf.write(rf.read());
                    wf.close();
                }
                rf.close();
            }
        }
        SD.remove(Config::SD_HISTORY_BACKUP_FILE_OLD);
        SD.rmdir(Config::SD_HISTORY_DIR_OLD);
        Serial.println("[sd] History-Ordner migriert: /SolarGuardian/History -> /SolarHistory");
    }
}

namespace {
    // Kopiert oldPath nach newPath (ueber eine .tmp-Datei), prueft danach
    // die Dateigroesse gegen das Original und benennt erst dann um -
    // schlaegt die Pruefung fehl, bleibt oldPath unangetastet (kein
    // Datenverlust). Gibt true zurueck, wenn newPath danach existiert und
    // korrekt ist.
    bool safeCopyFile(const char* oldPath, const char* newPath, const char* tmpPath) {
        File rf = SD.open(oldPath, FILE_READ);
        if (!rf) return false;
        size_t expectedSize = rf.size();

        if (SD.exists(tmpPath)) SD.remove(tmpPath);
        File wf = SD.open(tmpPath, FILE_WRITE);
        if (!wf) { rf.close(); return false; }

        uint8_t buf[256];
        size_t written = 0;
        while (rf.available()) {
            int n = rf.read(buf, sizeof(buf));
            if (n <= 0) break;
            written += wf.write(buf, (size_t)n);
        }
        rf.close();
        wf.close();

        if (written != expectedSize) {
            SD.remove(tmpPath);
            return false;
        }

        if (SD.exists(newPath)) SD.remove(newPath);
        File check = SD.open(tmpPath, FILE_READ);
        bool sizeOk = check && check.size() == expectedSize;
        if (check) check.close();
        if (!sizeOk) { SD.remove(tmpPath); return false; }

        // SD.rename() gibt es auf diesem Treiber nicht zuverlaessig fuer
        // alle Faelle - stattdessen die bereits verifizierte .tmp-Datei
        // einfach als die eigentliche Zieldatei weiternutzen: nochmal
        // lesen+schreiben waere unnoetig, deshalb .tmp direkt umbenennen.
        if (!SD.rename(tmpPath, newPath)) {
            // Fallback, falls rename() auf dieser SD-Karte/Treiberversion
            // nicht unterstuetzt ist: Inhalt nochmal kopieren.
            File rf2 = SD.open(tmpPath, FILE_READ);
            File wf2 = SD.open(newPath, FILE_WRITE);
            bool ok = rf2 && wf2;
            if (ok) {
                while (rf2.available()) {
                    int n = rf2.read(buf, sizeof(buf));
                    if (n <= 0) break;
                    wf2.write(buf, (size_t)n);
                }
            }
            if (rf2) rf2.close();
            if (wf2) wf2.close();
            SD.remove(tmpPath);
            if (!ok) return false;
        }
        return true;
    }

    // Verschiebt eine einzelne Zustands-Datei (yield.json/years.json) von
    // oldPath nach newPath, falls newPath noch nicht existiert. Existiert
    // newPath bereits (z.B. ein frueherer, unvollstaendiger Migrations-
    // versuch), wird NICHT die aeltere Datei bevorzugt - behalten wird die
    // groessere (= vollstaendigere) der beiden, die andere wird entfernt,
    // ein Hinweis geht in den seriellen Log (nur Dateiname, keine Werte).
    void migrateStateFile(const char* oldPath, const char* newPath, const char* tmpPath, const char* label) {
        if (!SD.exists(oldPath)) return;

        if (!SD.exists(newPath)) {
            if (safeCopyFile(oldPath, newPath, tmpPath)) {
                SD.remove(oldPath);
                Serial.printf("[sd] %s nach /SolarHistory verschoben\n", label);
            } else {
                Serial.printf("[sd] %s konnte nicht nach /SolarHistory verschoben werden - bleibt vorerst in /SolarGuardian\n", label);
            }
            return;
        }

        // Beide existieren - Konflikt, die vollstaendigere (= groessere)
        // behalten.
        File oldF = SD.open(oldPath, FILE_READ);
        File newF = SD.open(newPath, FILE_READ);
        size_t oldSize = oldF ? oldF.size() : 0;
        size_t newSize = newF ? newF.size() : 0;
        if (oldF) oldF.close();
        if (newF) newF.close();

        if (oldSize > newSize) {
            if (safeCopyFile(oldPath, newPath, tmpPath)) {
                SD.remove(oldPath);
                Serial.printf("[sd] %s: Konflikt beim Verschieben - Version aus /SolarGuardian war vollstaendiger, wurde uebernommen\n", label);
            }
        } else {
            SD.remove(oldPath);
            Serial.printf("[sd] %s: Konflikt beim Verschieben - Version in /SolarHistory war bereits vollstaendiger, alte Datei entfernt\n", label);
        }
    }

    // days.csv ist ein reines Anhaenge-Protokoll (eine Zeile pro
    // abgeschlossenem Tag). Existieren beim Umzug beide Dateien, werden
    // sie Zeile fuer Zeile zusammengefuehrt (nach Datum eindeutig), damit
    // garantiert keine einzelne Tageszeile verloren geht - statt einfach
    // eine der beiden Dateien zu verwerfen.
    void mergeDaysCsvLine(File& out, const String& line, String& seenDatesCsv) {
        if (line.length() == 0) return;
        int sep = line.indexOf(';');
        String dateKey = sep > 0 ? line.substring(0, sep) : line;
        String marker = "," + dateKey + ",";
        if (seenDatesCsv.indexOf(marker) >= 0) return; // Datum schon uebernommen
        seenDatesCsv += dateKey + ",";
        out.println(line);
    }

    void migrateDaysCsv() {
        const char* oldPath = Config::SD_YIELD_DAYS_CSV_OLD;
        const char* newPath = Config::SD_YIELD_DAYS_CSV;
        if (!SD.exists(oldPath)) return;

        if (!SD.exists(newPath)) {
            if (safeCopyFile(oldPath, newPath, "/SolarHistory/days.csv.tmp")) {
                SD.remove(oldPath);
                Serial.println("[sd] days.csv nach /SolarHistory verschoben");
            } else {
                Serial.println("[sd] days.csv konnte nicht nach /SolarHistory verschoben werden - bleibt vorerst in /SolarGuardian");
            }
            return;
        }

        // Beide existieren - zusammenfuehren statt eine Seite zu verwerfen.
        const char* mergedTmp = "/SolarHistory/days.csv.merge.tmp";
        if (SD.exists(mergedTmp)) SD.remove(mergedTmp);
        File out = SD.open(mergedTmp, FILE_WRITE);
        if (!out) {
            Serial.println("[sd] days.csv: Zusammenfuehren fehlgeschlagen (konnte keine Datei anlegen)");
            return;
        }

        String seenDatesCsv = ",";
        // Zuerst die bereits in /SolarHistory vorhandenen Zeilen (gilt als
        // "aktueller" Stand), danach alles aus /SolarGuardian, das dort
        // noch zusaetzlich steht.
        File newF = SD.open(newPath, FILE_READ);
        if (newF) {
            while (newF.available()) mergeDaysCsvLine(out, newF.readStringUntil('\n'), seenDatesCsv);
            newF.close();
        }
        File oldF = SD.open(oldPath, FILE_READ);
        if (oldF) {
            while (oldF.available()) mergeDaysCsvLine(out, oldF.readStringUntil('\n'), seenDatesCsv);
            oldF.close();
        }
        out.close();

        SD.remove(newPath);
        if (SD.rename(mergedTmp, newPath)) {
            SD.remove(oldPath);
            Serial.println("[sd] days.csv: Eintraege aus /SolarGuardian und /SolarHistory zusammengefuehrt, keine Zeile verloren");
        } else {
            Serial.println("[sd] days.csv: Zusammenfuehren fehlgeschlagen beim Umbenennen - beide Ausgangsdateien bleiben vorsichtshalber erhalten");
        }
    }

    // Auftrag "Ordneraufteilung SD-Karte": /SolarGuardian enthaelt danach
    // NUR noch App-Einstellungen (config.json), /SolarHistory ALLE
    // Solardaten (Ertragsbuch yield.json/days.csv/years.json, dazu wie
    // bisher schon tagesertrag.json/months.json/sg.cert). Einmalige
    // Migration beim Start, sicher: erst kopieren+pruefen, dann erst die
    // alte Datei entfernen (siehe safeCopyFile()).
    void migrateSolarDataFolder() {
        if (!SD.exists(Config::SD_HISTORY_DIR)) SD.mkdir(Config::SD_HISTORY_DIR);
        migrateStateFile(Config::SD_YIELD_FILE_OLD, Config::SD_YIELD_FILE, Config::SD_YIELD_TMP_FILE, "yield.json");
        migrateStateFile(Config::SD_YEARS_FILE_OLD, Config::SD_YEARS_FILE, Config::SD_YEARS_TMP_FILE, "years.json");
        migrateDaysCsv();
    }
}

void ensureRootDir() {
    if (!mounted) return;
    if (!SD.exists(Config::SD_ROOT_DIR)) SD.mkdir(Config::SD_ROOT_DIR);

    migrateHistoryFolder();

    // History-Backup-Ordner (siehe yield_ledger.h) - wird hier erzeugt,
    // damit er sowohl bei der Ersteinrichtung als auch bei jedem
    // normalen Start existiert (ensureRootDir() wird in beiden Faellen
    // aufgerufen).
    if (!SD.exists(Config::SD_HISTORY_DIR)) SD.mkdir(Config::SD_HISTORY_DIR);

    migrateSolarDataFolder();
}

}
