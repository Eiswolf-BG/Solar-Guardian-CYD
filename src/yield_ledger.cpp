#include "yield_ledger.h"
#include "solar_data.h"
#include "day_window.h"
#include "config.h"
#include <SD.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>

namespace YieldLedger {

namespace {
    constexpr uint8_t YIELD_FORMAT_VERSION = 3; // 1: Monat/Jahr/Gesamt. 2: +Kalenderwoche. 3: Gesamt/Woche weg, gleitendes 7-Tage-Fenster via letzteTage.

    struct DayEntry {
        char date[11] = {0};
        float kwh = 0.0f;
    };

    struct State {
        char currentDate[11] = {0}; // "YYYY-MM-DD"
        float todayMax = 0.0f;
        char monthKey[8] = {0};     // "YYYY-MM"
        float monthBase = 0.0f;
        char yearKey[5] = {0};      // "YYYY"
        float yearBase = 0.0f;
        DayEntry lastDays[6];       // Config::YIELD_ROLLING_WEEK_DAYS abgeschlossene Vortage, aeltester zuerst
        uint8_t lastDaysCount = 0;
        bool initialized = false; // true, sobald ein Datum/Basiswerte gueltig sind
        bool seeded = false;      // true, sobald die Erstinbetriebnahme-Startwerte gesetzt wurden
    };

    State state;
    bool dirty = false;
    uint32_t lastSaveMs = 0;
    uint32_t revisionCounter = 0;
    time_t lastProcessedMeasuredAt = 0;
    uint32_t lastSeenDayFetchSeq = 0;
    bool restoreAttempted = false;

    // Tage zwischen zwei "YYYY-MM-DD"-Daten (Mittag lokal, um DST-Kanten
    // zu vermeiden): Ergebnis = epochDay(b) - epochDay(a).
    long daysBetweenDateStrings(const char* a, const char* b) {
        struct tm ta = {}, tb = {};
        int ay, am, ad, by, bm, bd;
        if (sscanf(a, "%d-%d-%d", &ay, &am, &ad) != 3) return 0;
        if (sscanf(b, "%d-%d-%d", &by, &bm, &bd) != 3) return 0;
        ta.tm_year = ay - 1900; ta.tm_mon = am - 1; ta.tm_mday = ad; ta.tm_hour = 12;
        tb.tm_year = by - 1900; tb.tm_mon = bm - 1; tb.tm_mday = bd; tb.tm_hour = 12;
        time_t ea = mktime(&ta), eb = mktime(&tb);
        return (long)llround((double)(eb - ea) / 86400.0);
    }

    void appendDaysCsv(const char* dateStr, float kwh) {
        File f = SD.open(Config::SD_YIELD_DAYS_CSV, FILE_APPEND);
        if (!f) return;
        f.printf("%s;%.2f\n", dateStr, (double)kwh);
        f.close();
    }

    void pruneLastDays(const char* newDateStr) {
        DayEntry kept[6];
        uint8_t keptCount = 0;
        for (uint8_t i = 0; i < state.lastDaysCount; i++) {
            long diff = daysBetweenDateStrings(state.lastDays[i].date, newDateStr);
            if (diff >= 0 && diff <= Config::YIELD_ROLLING_WEEK_DAYS) {
                kept[keptCount++] = state.lastDays[i];
            }
        }
        for (uint8_t i = 0; i < keptCount; i++) state.lastDays[i] = kept[i];
        state.lastDaysCount = keptCount;
    }

    // Fehlerursache "Tage in falscher Reihenfolge" (Auftrag): frueher
    // haengte diese Funktion jeden Tag blind ans ENDE des Arrays an, auch
    // wenn sein Datum chronologisch VOR bereits vorhandenen Eintraegen lag
    // (z.B. beim Nachladen aus days.csv nach einem Neustart, wenn ein
    // aelterer Tag in yield.json fehlte) - dadurch landete ein
    // "ueberholter" Tag mitten in der eigentlich aufsteigend sortierten
    // Liste (siehe Screenshot/Git-Historie: "Mi Do Fr Sa Di So Mo" statt
    // "Di Mi Do Fr Sa So Mo"). Die SUMME (current().weekKwh) war davon
    // nicht betroffen (Reihenfolge ist fuer eine Summe egal), nur die
    // Anzeige auf der Geraete-Seite "Jahre"/Reiter "Tage" und in der
    // WebUI (beide lesen lastDays in Array-Reihenfolge). "YYYY-MM-DD"
    // sortiert als reiner String bereits korrekt chronologisch, deshalb
    // reicht ein einfacher strcmp()-Sortierdurchlauf. pushLastDay() haelt
    // das Array jetzt nach JEDEM Aufruf sortiert UND dupliziert kein
    // Datum mehr (Auftrag "keine Duplikate pro Datum") - unabhaengig
    // davon, in welcher Reihenfolge die Tage eintreffen.
    void sortLastDaysByDate() {
        for (uint8_t i = 1; i < state.lastDaysCount; i++) {
            DayEntry key = state.lastDays[i];
            int8_t j = (int8_t)(i - 1);
            while (j >= 0 && strcmp(state.lastDays[j].date, key.date) > 0) {
                state.lastDays[j + 1] = state.lastDays[j];
                j--;
            }
            state.lastDays[j + 1] = key;
        }
    }

    void pushLastDay(const char* dateStr, float kwh) {
        for (uint8_t i = 0; i < state.lastDaysCount; i++) {
            if (strcmp(state.lastDays[i].date, dateStr) == 0) {
                state.lastDays[i].kwh = kwh; // Datum schon vorhanden - Wert aktualisieren statt duplizieren
                sortLastDaysByDate();
                return;
            }
        }
        if (state.lastDaysCount >= Config::YIELD_ROLLING_WEEK_DAYS) {
            // Sortiert entfernt das Array garantiert den CHRONOLOGISCH
            // aeltesten Eintrag (Index 0), nicht einfach den zuletzt
            // physisch angehaengten.
            sortLastDaysByDate();
            for (uint8_t i = 1; i < state.lastDaysCount; i++) state.lastDays[i - 1] = state.lastDays[i];
            state.lastDaysCount = Config::YIELD_ROLLING_WEEK_DAYS - 1;
        }
        strncpy(state.lastDays[state.lastDaysCount].date, dateStr, 10);
        state.lastDays[state.lastDaysCount].date[10] = 0;
        state.lastDays[state.lastDaysCount].kwh = kwh;
        state.lastDaysCount++;
        sortLastDaysByDate();
    }

    bool save() {
        revisionCounter++;
        JsonDocument doc;
        doc["yield_format_version"] = YIELD_FORMAT_VERSION;
        doc["current_date"] = state.currentDate;
        doc["today_max"] = state.todayMax;
        doc["month_key"] = state.monthKey;
        doc["month_base"] = state.monthBase;
        doc["year_key"] = state.yearKey;
        doc["year_base"] = state.yearBase;
        JsonArray arr = doc["last_days"].to<JsonArray>();
        for (uint8_t i = 0; i < state.lastDaysCount; i++) {
            JsonObject o = arr.add<JsonObject>();
            o["date"] = state.lastDays[i].date;
            o["kwh"] = state.lastDays[i].kwh;
        }
        doc["initialized"] = state.initialized;
        doc["seeded"] = state.seeded;

        if (SD.exists(Config::SD_YIELD_TMP_FILE)) SD.remove(Config::SD_YIELD_TMP_FILE);
        File f = SD.open(Config::SD_YIELD_TMP_FILE, FILE_WRITE);
        if (!f) return false;
        bool ok = serializeJson(doc, f) > 0;
        f.close();
        if (!ok) { SD.remove(Config::SD_YIELD_TMP_FILE); return false; }

        if (SD.exists(Config::SD_YIELD_FILE)) SD.remove(Config::SD_YIELD_FILE);
        return SD.rename(Config::SD_YIELD_TMP_FILE, Config::SD_YIELD_FILE);
    }

    // Legt years.json leer an, falls sie noch nicht existiert - OHNE
    // vorbefuellte Vorjahreswerte (frueher mit fest eingebauten privaten
    // Anlagendaten vorbefuellt, inzwischen entfernt). Die Seite "Jahre"
    // zeigt dann einfach nur das laufende Jahr, bis echte
    // Jahresabschluesse dazukommen.
    void ensureYearsFileExists() {
        if (SD.exists(Config::SD_YEARS_FILE)) return;
        JsonDocument doc;
        doc.to<JsonObject>();
        if (SD.exists(Config::SD_YEARS_TMP_FILE)) SD.remove(Config::SD_YEARS_TMP_FILE);
        File f = SD.open(Config::SD_YEARS_TMP_FILE, FILE_WRITE);
        if (!f) return;
        bool ok = serializeJson(doc, f) > 0;
        f.close();
        if (!ok) { SD.remove(Config::SD_YEARS_TMP_FILE); return; }
        SD.rename(Config::SD_YEARS_TMP_FILE, Config::SD_YEARS_FILE);
    }

    void writeYearToYearsFile(int year, float kwh) {
        ensureYearsFileExists();
        JsonDocument doc;
        if (SD.exists(Config::SD_YEARS_FILE)) {
            File f = SD.open(Config::SD_YEARS_FILE, FILE_READ);
            if (f) { deserializeJson(doc, f); f.close(); }
        }
        char key[8];
        snprintf(key, sizeof(key), "%d", year);
        doc[key] = kwh;

        if (SD.exists(Config::SD_YEARS_TMP_FILE)) SD.remove(Config::SD_YEARS_TMP_FILE);
        File f = SD.open(Config::SD_YEARS_TMP_FILE, FILE_WRITE);
        if (!f) return;
        bool ok = serializeJson(doc, f) > 0;
        f.close();
        if (!ok) { SD.remove(Config::SD_YEARS_TMP_FILE); return; }
        if (SD.exists(Config::SD_YEARS_FILE)) SD.remove(Config::SD_YEARS_FILE);
        SD.rename(Config::SD_YEARS_TMP_FILE, Config::SD_YEARS_FILE);
        revisionCounter++;
    }

    // Analog zu ensureYearsFileExists() - legt months.json leer an, falls
    // sie noch nicht existiert (frueher mit fest eingebauten privaten
    // Anlagendaten vorbefuellt, inzwischen entfernt).
    void ensureMonthsFileExists() {
        if (SD.exists(Config::SD_MONTHS_FILE)) return;
        JsonDocument doc;
        doc.to<JsonObject>();
        if (SD.exists(Config::SD_MONTHS_TMP_FILE)) SD.remove(Config::SD_MONTHS_TMP_FILE);
        File f = SD.open(Config::SD_MONTHS_TMP_FILE, FILE_WRITE);
        if (!f) return;
        bool ok = serializeJson(doc, f) > 0;
        f.close();
        if (!ok) { SD.remove(Config::SD_MONTHS_TMP_FILE); return; }
        SD.rename(Config::SD_MONTHS_TMP_FILE, Config::SD_MONTHS_FILE);
    }

    void writeMonthToMonthsFile(const char* monthKey, float kwh) {
        ensureMonthsFileExists();
        JsonDocument doc;
        if (SD.exists(Config::SD_MONTHS_FILE)) {
            File f = SD.open(Config::SD_MONTHS_FILE, FILE_READ);
            if (f) { deserializeJson(doc, f); f.close(); }
        }
        doc[monthKey] = kwh;

        if (SD.exists(Config::SD_MONTHS_TMP_FILE)) SD.remove(Config::SD_MONTHS_TMP_FILE);
        File f = SD.open(Config::SD_MONTHS_TMP_FILE, FILE_WRITE);
        if (!f) return;
        bool ok = serializeJson(doc, f) > 0;
        f.close();
        if (!ok) { SD.remove(Config::SD_MONTHS_TMP_FILE); return; }
        if (SD.exists(Config::SD_MONTHS_FILE)) SD.remove(Config::SD_MONTHS_FILE);
        SD.rename(Config::SD_MONTHS_TMP_FILE, Config::SD_MONTHS_FILE);
        revisionCounter++;
    }

    void handleDayChange(const char* newDateStr, const struct tm& /*nowTm*/) {
        if (!state.initialized) {
            strncpy(state.currentDate, newDateStr, sizeof(state.currentDate) - 1);
            return;
        }

        // Knappe Logzeile fuer jeden Tageswechsel (Auftrag "Logzeilen fuer
        // heute Abend") - bisher gab es dafuer nur bei einer Luecke >1 Tag
        // ueberhaupt eine Meldung.
#ifdef NIGHT_MODE_DIAG_TEST
        Serial.printf("[yield] Tageswechsel: %s -> %s (%.2f kWh)\n", state.currentDate, newDateStr, (double)state.todayMax);
#endif

        long gapDays = daysBetweenDateStrings(state.currentDate, newDateStr);

        if (state.todayMax > 0) {
            state.monthBase += state.todayMax;
            state.yearBase += state.todayMax;
            appendDaysCsv(state.currentDate, state.todayMax);
            pushLastDay(state.currentDate, state.todayMax);
        }

        char newMonthKey[8];
        snprintf(newMonthKey, sizeof(newMonthKey), "%.7s", newDateStr);
        char newYearKey[5];
        snprintf(newYearKey, sizeof(newYearKey), "%.4s", newDateStr);

        if (strcmp(state.yearKey, newYearKey) != 0) {
            int oldYearNum = atoi(state.yearKey);
            if (oldYearNum > 0) {
                writeYearToYearsFile(oldYearNum, state.yearBase);
                Serial.printf("[yield] Jahreswechsel: %d abgeschlossen mit %.2f kWh\n", oldYearNum, (double)state.yearBase);
            }
            state.yearBase = 0.0f;
            strncpy(state.yearKey, newYearKey, sizeof(state.yearKey) - 1);
        }
        if (strcmp(state.monthKey, newMonthKey) != 0) {
            // state.monthBase enthaelt an dieser Stelle bereits den
            // soeben abgeschlossenen Tag (siehe Addition weiter oben) -
            // das ist exakt die Endsumme des alten Monats.
            if (state.monthKey[0]) {
                writeMonthToMonthsFile(state.monthKey, state.monthBase);
                Serial.printf("[yield] Monatswechsel: %s abgeschlossen mit %.2f kWh\n", state.monthKey, (double)state.monthBase);
            }
            state.monthBase = 0.0f;
            strncpy(state.monthKey, newMonthKey, sizeof(state.monthKey) - 1);
            Serial.println("[yield] Neuer Monat");
        }

        pruneLastDays(newDateStr);

        if (gapDays > 1) {
            Serial.printf("[yield] Luecke von %ld Tagen\n", gapDays - 1);
        }

        strncpy(state.currentDate, newDateStr, sizeof(state.currentDate) - 1);
        state.todayMax = 0.0f;
        save();
        lastSaveMs = millis();
    }

    void applyDailyValue(float value, const struct tm& nowTm) {
        if (value < Config::YIELD_DAILY_MIN_KWH || value > Config::YIELD_DAILY_MAX_KWH) return;
        if (nowTm.tm_hour < Config::YIELD_IGNORE_WINDOW_END_HOUR) return; // 00:00-04:00: ignorieren
        if (value > state.todayMax) {
            state.todayMax = value;
            dirty = true;
        }
    }

    void trySeedFromPortal(const struct tm& /*nowTm*/, const char* todayStr) {
        SolarData::Data snap = SolarData::snapshot();
        if (!snap.energyValid) return;

        state.monthBase = max(0.0f, snap.energyMonthKwh - snap.energyTodayKwh);
        state.yearBase = max(0.0f, snap.energyYearKwh - snap.energyTodayKwh);
        // todayMax kann durch eine Backup-Wiederherstellung schon gesetzt
        // sein (siehe tryRestoreFromBackup()) - nie verkleinern.
        state.todayMax = max(state.todayMax, snap.energyTodayKwh);
        strncpy(state.currentDate, todayStr, sizeof(state.currentDate) - 1);
        strncpy(state.monthKey, todayStr, sizeof(state.monthKey) - 1);
        strncpy(state.yearKey, todayStr, sizeof(state.yearKey) - 1);
        state.initialized = true;
        state.seeded = true;
        Serial.println("[yield] Erstinbetriebnahme: Monat/Jahr-Startwerte aus dem Portal gesetzt");
        save();
    }

    void tryRestoreFromBackup(const char* todayStr) {
        if (!SD.exists(Config::SD_HISTORY_BACKUP_FILE)) return;
        File f = SD.open(Config::SD_HISTORY_BACKUP_FILE, FILE_READ);
        if (!f) return;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, f);
        f.close();
        if (err) return;

        String backupDate = String((const char*)(doc["datum"] | ""));
        float backupKwh = doc["kwh"] | 0.0f;
        if (backupDate.length() != 10 || backupDate != todayStr) return;

        if (!state.initialized) {
            strncpy(state.currentDate, todayStr, sizeof(state.currentDate) - 1);
            strncpy(state.monthKey, todayStr, sizeof(state.monthKey) - 1);
            strncpy(state.yearKey, todayStr, sizeof(state.yearKey) - 1);
            state.todayMax = backupKwh;
            Serial.printf("[yield] Wiederherstellung aus Backup: %s, %.2f kWh\n", todayStr, (double)backupKwh);
        } else if (backupKwh > state.todayMax) {
            state.todayMax = backupKwh;
            dirty = true;
            Serial.printf("[yield] Wiederherstellung aus Backup: %s, %.2f kWh\n", todayStr, (double)backupKwh);
        }
    }

    // Schreibt die History-Backup-Datei - ueberschreibt immer dieselbe
    // eine Datei, nie einen kleineren Wert fuer dasselbe Datum (das
    // Portal setzt den Tageswert kurz vor Mitternacht auf 0, siehe
    // CLAUDE.md).
    void writeHistoryBackup(const char* dateStr, float kwh) {
        if (SD.exists(Config::SD_HISTORY_BACKUP_FILE)) {
            File rf = SD.open(Config::SD_HISTORY_BACKUP_FILE, FILE_READ);
            if (rf) {
                JsonDocument old;
                DeserializationError e = deserializeJson(old, rf);
                rf.close();
                if (!e) {
                    String oldDate = String((const char*)(old["datum"] | ""));
                    float oldKwh = old["kwh"] | 0.0f;
                    if (oldDate == dateStr && oldKwh > kwh) return;
                }
            }
        }

        char timeStr[6] = "--:--";
        struct tm t;
        if (getLocalTime(&t, 5)) snprintf(timeStr, sizeof(timeStr), "%02d:%02d", t.tm_hour, t.tm_min);

        JsonDocument doc;
        doc["datum"] = dateStr;
        doc["zeit"] = timeStr;
        doc["kwh"] = roundf(kwh * 100.0f) / 100.0f;
        // Nachtmodus-Bestaetigung (Auftrag "Tagesfenster mit Nachtmodus")
        // faengt bei jedem normalen Tagesschreiben bewusst wieder bei false
        // an - erst die Abschlussabfrage-Pruefung am Abend setzt sie
        // gezielt auf true (siehe refreshNightModeConfirmation() unten).
        doc["bestaetigt"] = false;

        if (SD.exists(Config::SD_HISTORY_BACKUP_TMP)) SD.remove(Config::SD_HISTORY_BACKUP_TMP);
        File wf = SD.open(Config::SD_HISTORY_BACKUP_TMP, FILE_WRITE);
        if (!wf) return;
        bool ok = serializeJson(doc, wf) > 0;
        wf.close();
        if (!ok) { SD.remove(Config::SD_HISTORY_BACKUP_TMP); return; }

        if (SD.exists(Config::SD_HISTORY_BACKUP_FILE)) SD.remove(Config::SD_HISTORY_BACKUP_FILE);
        SD.rename(Config::SD_HISTORY_BACKUP_TMP, Config::SD_HISTORY_BACKUP_FILE);

        Serial.printf("[yield] History-Backup geschrieben: %s, %.2f kWh\n", dateStr, (double)kwh);
    }

    // Patcht NUR das "bestaetigt"-Feld der bestehenden Backup-Datei (Datum/
    // Zeit/kWh bleiben unangetastet) - no-op, falls die Datei fehlt oder zu
    // einem anderen Datum gehoert (z.B. noch nicht geschrieben, weil die
    // Abschlussabfrage komplett fehlgeschlagen ist).
    void setHistoryBackupConfirmed(const char* dateStr, bool confirmed) {
        if (!SD.exists(Config::SD_HISTORY_BACKUP_FILE)) return;
        File rf = SD.open(Config::SD_HISTORY_BACKUP_FILE, FILE_READ);
        if (!rf) return;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, rf);
        rf.close();
        if (err) return;
        String backupDate = String((const char*)(doc["datum"] | ""));
        if (backupDate != dateStr) return;
        doc["bestaetigt"] = confirmed;

        if (SD.exists(Config::SD_HISTORY_BACKUP_TMP)) SD.remove(Config::SD_HISTORY_BACKUP_TMP);
        File wf = SD.open(Config::SD_HISTORY_BACKUP_TMP, FILE_WRITE);
        if (!wf) return;
        bool ok = serializeJson(doc, wf) > 0;
        wf.close();
        if (!ok) { SD.remove(Config::SD_HISTORY_BACKUP_TMP); return; }
        SD.remove(Config::SD_HISTORY_BACKUP_FILE);
        SD.rename(Config::SD_HISTORY_BACKUP_TMP, Config::SD_HISTORY_BACKUP_FILE);
    }

    // Nachtmodus-Zustand (Mondsichel) - Cache, damit nicht bei jedem Tick
    // erneut die SD-Karte gelesen werden muss, aber IMMER aus Uhrzeit +
    // gespeicherten Daten ableitbar (Auftrag 3g), nie nur aus dem RAM:
    // nightConfirmedCacheDate ist der Abend ("YYYY-MM-DD"), fuer den
    // nightConfirmedCache zuletzt ermittelt wurde.
    char nightConfirmedCacheDate[11] = {0};
    bool nightConfirmedCache = false;

    // Einmal pro Abend/Nacht ausgefuehrt (idempotent ueber
    // nightConfirmedCacheDate): entweder eine FRISCHE Pruefung (Auftrag
    // 3d, wenn die Abschlussabfrage in DIESER Sitzung lief) oder, falls das
    // Geraet erst waehrend der Nacht neu gestartet ist, die zuletzt
    // gespeicherte Bestaetigung direkt aus der Backup-Datei uebernehmen -
    // in beiden Faellen OHNE Abhaengigkeit von RAM-Zustand aus der Zeit vor
    // einem moeglichen Neustart.
    void refreshNightModeConfirmation(time_t nowEpoch, const struct tm& nowTm) {
        char eveningDate[11];
        DayWindow::currentNightEveningDate(nowEpoch, nowTm, eveningDate, sizeof(eveningDate));
        if (nightConfirmedCacheDate[0] && strcmp(nightConfirmedCacheDate, eveningDate) == 0) return;

        SolarData::Data snap = SolarData::snapshot();
        bool freshSignalThisSession = snap.finalQueryDoneDate[0] &&
                                       strcmp(snap.finalQueryDoneDate, eveningDate) == 0;

        bool confirmed = false;
        if (SD.exists(Config::SD_HISTORY_BACKUP_FILE)) {
            File rf = SD.open(Config::SD_HISTORY_BACKUP_FILE, FILE_READ);
            if (rf) {
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, rf);
                rf.close();
                if (!err) {
                    String backupDate = String((const char*)(doc["datum"] | ""));
                    bool backupMatchesTonight = (backupDate == eveningDate);
                    if (freshSignalThisSession) {
                        float backupKwh = doc["kwh"] | -1.0f;
                        confirmed = backupMatchesTonight && snap.dailySourceValid &&
                                    fabsf(backupKwh - snap.dailySourceKwh) < 0.01f;
                    } else {
                        confirmed = backupMatchesTonight && (doc["bestaetigt"] | false);
                    }
                }
            }
        }

        if (freshSignalThisSession) {
            setHistoryBackupConfirmed(eveningDate, confirmed);
#ifdef NIGHT_MODE_DIAG_TEST
            if (confirmed) {
                Serial.printf("[yield] Nachtmodus bestaetigt fuer %s\n", eveningDate);
            } else {
                Serial.printf("[yield] Nachtmodus NICHT bestaetigt fuer %s (Abschlussabfrage fehlgeschlagen oder Backup weicht ab)\n", eveningDate);
            }
#endif
        }

        strncpy(nightConfirmedCacheDate, eveningDate, sizeof(nightConfirmedCacheDate) - 1);
        nightConfirmedCache = confirmed;
    }

    void backfillLastDaysFromCsv(const char* todayStr) {
        File f = SD.open(Config::SD_YIELD_DAYS_CSV, FILE_READ);
        if (!f) return;
        while (f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            int sep = line.indexOf(';');
            if (sep != 10) continue; // "YYYY-MM-DD" ist genau 10 Zeichen lang
            String dateStr = line.substring(0, sep);
            float kwh = line.substring(sep + 1).toFloat();

            long diff = daysBetweenDateStrings(dateStr.c_str(), todayStr);
            if (diff < 1 || diff > Config::YIELD_ROLLING_WEEK_DAYS) continue;

            bool exists = false;
            for (uint8_t i = 0; i < state.lastDaysCount; i++) {
                if (strcmp(state.lastDays[i].date, dateStr.c_str()) == 0) { exists = true; break; }
            }
            if (!exists) pushLastDay(dateStr.c_str(), kwh);
        }
        f.close();
        pruneLastDays(todayStr);
        sortLastDaysByDate();
    }

}

void init() {
    if (!SD.exists(Config::SD_YIELD_FILE)) {
        Serial.println("[yield] yield.json fehlt - starte Zaehlung neu (Erstinbetriebnahme)");
        return;
    }

    File f = SD.open(Config::SD_YIELD_FILE, FILE_READ);
    if (!f) {
        Serial.println("[yield] yield.json konnte nicht geoeffnet werden - starte Zaehlung neu");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.println("[yield] yield.json ist beschaedigt - starte Zaehlung neu");
        state = State();
        return;
    }

    uint8_t loadedVersion = doc["yield_format_version"] | 1;

    strncpy(state.currentDate, (const char*)(doc["current_date"] | ""), sizeof(state.currentDate) - 1);
    state.todayMax = doc["today_max"] | 0.0f;
    state.monthBase = doc["month_base"] | 0.0f;
    state.yearBase = doc["year_base"] | 0.0f;
    state.initialized = doc["initialized"] | false;
    state.seeded = doc["seeded"] | false;

    if (loadedVersion >= 3) {
        strncpy(state.monthKey, (const char*)(doc["month_key"] | ""), sizeof(state.monthKey) - 1);
        strncpy(state.yearKey, (const char*)(doc["year_key"] | ""), sizeof(state.yearKey) - 1);
        JsonArray arr = doc["last_days"];
        state.lastDaysCount = 0;
        for (JsonObject o : arr) {
            if (state.lastDaysCount >= Config::YIELD_ROLLING_WEEK_DAYS) break;
            strncpy(state.lastDays[state.lastDaysCount].date, (const char*)(o["date"] | ""), 10);
            state.lastDays[state.lastDaysCount].kwh = o["kwh"] | 0.0f;
            state.lastDaysCount++;
        }
        // Auftrag "Tage in falscher Reihenfolge": unabhaengig davon, in
        // welcher Reihenfolge yield.json die Tage gespeichert hatte (auch
        // aeltere, vor diesem Fix geschriebene Dateien) - beim Laden
        // IMMER neu sortieren, bevor irgendeine Seite/die WebUI sie liest.
        sortLastDaysByDate();
    } else {
        // Migration auf Version 3: "Gesamt" (und eine evtl. vorhandene
        // Kalenderwoche aus Version 2) entfallen restlos - letzteTage
        // startet leer und wird in tick() aus days.csv ergaenzt, sobald
        // NTP-Zeit verfuegbar ist. Monat/Jahr bleiben unveraendert.
        state.lastDaysCount = 0;
        if (state.currentDate[0]) {
            strncpy(state.monthKey, state.currentDate, sizeof(state.monthKey) - 1);
            strncpy(state.yearKey, state.currentDate, sizeof(state.yearKey) - 1);
        }
        Serial.println("[yield] Migration auf Format-Version 3: Gesamt entfernt, Woche ist jetzt ein gleitendes 7-Tage-Fenster");
        save();
    }
}

void tick() {
    struct tm nowTm;
    if (!getLocalTime(&nowTm, 5)) return; // ohne gueltige NTP-Zeit wird nichts gezaehlt

    // Datumsschutz (Auftrag, freigegeben): eine Uhr ohne plausibles Jahr
    // gilt als nicht synchronisiert - das Ertragsbuch verarbeitet dann
    // GAR NICHTS (genau wie ganz ohne NTP-Zeit oben), bis sich die Uhr
    // wieder in einem plausiblen Bereich befindet. Kein pauschaler
    // Sprung-Schutz - ein echter mehrtaegiger Ausfall hat weiterhin ein
    // plausibles Jahr und wird dadurch nicht blockiert.
    int currentYear = nowTm.tm_year + 1900;
    if (currentYear < 2024 || currentYear > 2099) {
#ifdef NIGHT_MODE_DIAG_TEST
        static uint32_t lastImplausibleYearWarnMs = 0;
        uint32_t warnNow = millis();
        if (lastImplausibleYearWarnMs == 0 || warnNow - lastImplausibleYearWarnMs >= 60000) {
            lastImplausibleYearWarnMs = warnNow;
            Serial.printf("[yield] Unplausibles Datum abgelehnt: Uhr nicht synchronisiert (Jahr=%d) - Ertragsbuch verarbeitet nichts\n", currentYear);
        }
#endif
        return;
    }

    char todayStr[11];
    snprintf(todayStr, sizeof(todayStr), "%04d-%02d-%02d",
             nowTm.tm_year + 1900, nowTm.tm_mon + 1, nowTm.tm_mday);

    if (!restoreAttempted) {
        restoreAttempted = true;
        tryRestoreFromBackup(todayStr);
        if (state.lastDaysCount < Config::YIELD_ROLLING_WEEK_DAYS) backfillLastDaysFromCsv(todayStr);
#ifdef NIGHT_MODE_DIAG_TEST
        // Logzeile "Neustart im Nachtmodus" (Auftrag) - nur HIER beim
        // allerersten tick() nach dem Start sinnvoll zu pruefen.
        if (DayWindow::isNightWindow(nowTm)) {
            Serial.println("[yield] Neustart erfolgte im Nachtmodus - keine Abfrage bis 04:30");
        }
#endif
    }

    if (!state.seeded) {
        trySeedFromPortal(nowTm, todayStr);
    } else if (strncmp(todayStr, state.currentDate, sizeof(todayStr)) != 0) {
        // Datumsschutz: ein Datum VOR dem gespeicherten wird ignoriert
        // (z.B. ein kurzer Rueckwaertssprung der Uhr nach einem NTP-
        // Ausrutscher) - kein pauschaler Sprung-Schutz, ein echter
        // mehrtaegiger VORWAERTS-Ausfall bleibt dadurch erlaubt.
        if (state.currentDate[0] && strcmp(todayStr, state.currentDate) < 0) {
#ifdef NIGHT_MODE_DIAG_TEST
            static uint32_t lastBackwardWarnMs = 0;
            uint32_t warnNow = millis();
            if (lastBackwardWarnMs == 0 || warnNow - lastBackwardWarnMs >= 60000) {
                lastBackwardWarnMs = warnNow;
                Serial.printf("[yield] Unplausibles Datum abgelehnt: Rueckwaertssprung (gespeichert=%s, gemeldet=%s)\n",
                              state.currentDate, todayStr);
            }
#endif
        } else {
            handleDayChange(todayStr, nowTm);
        }
    }

    // Tagesfenster/Nachtmodus (Auftrag): waehrend der Nacht (22:55-04:30)
    // einmal pro Abend die Mondsicht-Bestaetigung ableiten/aktualisieren -
    // siehe refreshNightModeConfirmation() oben, komplett unabhaengig vom
    // sonstigen Ertragsbuch-Ablauf.
    if (DayWindow::isNightWindow(nowTm)) {
        refreshNightModeConfirmation(time(nullptr), nowTm);
    }

    SolarData::Data snap = SolarData::snapshot();

    if (state.seeded) {
        if (snap.dailySourceValid && snap.dailySourceMeasuredAt != lastProcessedMeasuredAt) {
            lastProcessedMeasuredAt = snap.dailySourceMeasuredAt;
            applyDailyValue(snap.dailySourceKwh, nowTm);
        }
    }

    // History-Backup (Teil 2): bei jeder erfolgreichen Tagesertrag-
    // Abfrage den aktuellen Tagesmaximalwert sichern - nicht den rohen
    // Abfragewert, siehe CLAUDE.md.
    if (snap.dayFetchSeq != lastSeenDayFetchSeq) {
        lastSeenDayFetchSeq = snap.dayFetchSeq;
        if (nowTm.tm_hour >= Config::YIELD_IGNORE_WINDOW_END_HOUR) {
            writeHistoryBackup(state.currentDate, state.todayMax);
        }
    }

    uint32_t now = millis();
    if (dirty && now - lastSaveMs >= Config::YIELD_SAVE_MIN_INTERVAL_MS) {
        save();
        dirty = false;
        lastSaveMs = now;
    }
}

void flush() {
    if (dirty) {
        save();
        dirty = false;
        lastSaveMs = millis();
    }
}

Values current() {
    Values v;
    v.valid = state.initialized;
    v.todayKwh = state.todayMax;
    float weekSum = state.todayMax;
    for (uint8_t i = 0; i < state.lastDaysCount; i++) weekSum += state.lastDays[i].kwh;
    v.weekKwh = weekSum;
    v.monthKwh = state.monthBase + state.todayMax;
    v.yearKwh = state.yearBase + state.todayMax;
    return v;
}

namespace {
    SetTargetResult setTargetCommon(float& base, float desiredIncludingToday) {
        if (!state.initialized) return SetTargetResult::NoDataYet;
        float newBase = desiredIncludingToday - state.todayMax;
        if (newBase < 0) return SetTargetResult::Negative;
        base = newBase;
        save();
        return SetTargetResult::Ok;
    }
}

SetTargetResult setMonthTarget(float desiredIncludingToday) { return setTargetCommon(state.monthBase, desiredIncludingToday); }
SetTargetResult setYearTarget(float desiredIncludingToday) { return setTargetCommon(state.yearBase, desiredIncludingToday); }

uint8_t pastYears(YearEntry* out, uint8_t maxCount) {
    ensureYearsFileExists();
    File f = SD.open(Config::SD_YEARS_FILE, FILE_READ);
    if (!f) return 0;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return 0;

    constexpr uint8_t MAX_READ = 32;
    YearEntry all[MAX_READ];
    uint8_t n = 0;
    for (JsonPair kv : doc.as<JsonObject>()) {
        if (n >= MAX_READ) break;
        all[n].year = atoi(kv.key().c_str());
        all[n].kwh = kv.value().as<float>();
        n++;
    }
    // Absteigend nach Jahr sortieren (einfacher Bubble-Sort, n ist klein).
    for (uint8_t i = 0; i < n; i++) {
        for (uint8_t j = (uint8_t)(i + 1); j < n; j++) {
            if (all[j].year > all[i].year) { YearEntry tmp = all[i]; all[i] = all[j]; all[j] = tmp; }
        }
    }

    uint8_t take = (uint8_t)(n < maxCount ? n : maxCount);
    for (uint8_t i = 0; i < take; i++) out[i] = all[take - 1 - i];
    return take;
}

uint8_t pastMonths(MonthEntry* out, uint8_t maxCount) {
    ensureMonthsFileExists();
    File f = SD.open(Config::SD_MONTHS_FILE, FILE_READ);
    if (!f) return 0;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return 0;

    constexpr uint8_t MAX_READ = 64;
    MonthEntry all[MAX_READ];
    uint8_t n = 0;
    for (JsonPair kv : doc.as<JsonObject>()) {
        if (n >= MAX_READ) break;
        strncpy(all[n].key, kv.key().c_str(), sizeof(all[n].key) - 1);
        all[n].kwh = kv.value().as<float>();
        n++;
    }
    // Absteigend nach Monatsschluessel sortieren ("YYYY-MM" sortiert als
    // Text bereits korrekt chronologisch, einfacher Bubble-Sort, n ist
    // klein).
    for (uint8_t i = 0; i < n; i++) {
        for (uint8_t j = (uint8_t)(i + 1); j < n; j++) {
            if (strcmp(all[j].key, all[i].key) > 0) { MonthEntry tmp = all[i]; all[i] = all[j]; all[j] = tmp; }
        }
    }

    uint8_t take = (uint8_t)(n < maxCount ? n : maxCount);
    for (uint8_t i = 0; i < take; i++) out[i] = all[take - 1 - i];
    return take;
}

uint8_t lastDaysIncludingToday(DayValue* out, uint8_t maxCount) {
    if (!state.initialized) return 0;
    uint8_t n = 0;
    for (uint8_t i = 0; i < state.lastDaysCount && n < maxCount; i++) {
        strncpy(out[n].date, state.lastDays[i].date, sizeof(out[n].date) - 1);
        out[n].kwh = state.lastDays[i].kwh;
        n++;
    }
    if (n < maxCount) {
        strncpy(out[n].date, state.currentDate, sizeof(out[n].date) - 1);
        out[n].kwh = state.todayMax;
        n++;
    }
    return n;
}

uint32_t revision() { return revisionCounter; }

bool isNightModeConfirmedNow() {
    time_t now = time(nullptr);
    struct tm nowTm;
    if (!getLocalTime(&nowTm, 5)) return false;
    if (!DayWindow::isNightWindow(nowTm)) return false;

    char eveningDate[11];
    DayWindow::currentNightEveningDate(now, nowTm, eveningDate, sizeof(eveningDate));
    if (!nightConfirmedCacheDate[0] || strcmp(nightConfirmedCacheDate, eveningDate) != 0) {
        // Cache noch nicht fuer diesen Abend gefuellt (z.B. diese Funktion
        // wird vor dem naechsten tick() aufgerufen) - sofort einmalig
        // nachholen, rein lesend/ableitend.
        refreshNightModeConfirmation(now, nowTm);
    }
    return nightConfirmedCache;
}

}
