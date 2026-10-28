#pragma once
// Gemeinsamer Text-Helfer fuer die Refresh-Rueckmeldung (Auftrag
// "Refresh-Regeln und Rueckmeldung") - von allen antippbaren Seiten
// genutzt (Uebersicht, Leistung, Ertrag, Batterie), damit die Zuordnung
// Feedback-Wert -> Text nur an einer Stelle steht. Reine Textfunktion,
// keine SD-/Netzwerk-Zugriffe - unbedenklich von Core 1 aus jeder Seite
// heraus aufrufbar.
#include "solar_data.h"
#include "i18n.h"
#include "config.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace RefreshFeedbackText {
    // true = Text wird nur kurz (ca. 2s) angezeigt, danach normale
    // Anzeige wiederherstellen. false (nur bei "Working") = bleibt
    // stehen, bis das naechste Ereignis (Erfolg/Fehlschlag) eintrifft -
    // ABER siehe FeedbackState::update(): auch "Working" hat inzwischen
    // eine harte Obergrenze (Auftrag "Fehler in der Refresh-
    // Rueckmeldung"), damit die Anzeige nie fuer immer stehen bleibt.
    inline bool isTimed(SolarData::RefreshFeedback fb) {
        return fb != SolarData::RefreshFeedback::Working &&
               fb != SolarData::RefreshFeedback::WorkingActiveRead;
    }

    inline String textFor(SolarData::RefreshFeedback fb) {
        switch (fb) {
            case SolarData::RefreshFeedback::Working:
            // WorkingActiveRead zeigt bewusst DENSELBEN Text ("Lade...")
            // wie Working - nur die harte Zeitgrenze unterscheidet sich
            // (siehe FeedbackState::update() unten), Korrektur B2.
            case SolarData::RefreshFeedback::WorkingActiveRead:
                return I18n::t(I18n::Str::REFRESH_WORKING);
            case SolarData::RefreshFeedback::Done:      return I18n::t(I18n::Str::REFRESH_DONE);
            case SolarData::RefreshFeedback::NoChange:  return I18n::t(I18n::Str::REFRESH_NO_CHANGE);
            case SolarData::RefreshFeedback::NoWifi:    return I18n::t(I18n::Str::REFRESH_NO_WIFI);
            case SolarData::RefreshFeedback::Busy:      return I18n::t(I18n::Str::REFRESH_BUSY);
            case SolarData::RefreshFeedback::Cooldown:  return I18n::t(I18n::Str::REFRESH_COOLDOWN);
            case SolarData::RefreshFeedback::LockedNight: {
                char buf[48];
                snprintf(buf, sizeof(buf), I18n::t(I18n::Str::REFRESH_LOCKED_NIGHT_FMT),
                         Config::DAY_START_HOUR, Config::DAY_START_MINUTE);
                return String(buf);
            }
            // Auftrag "Seriennummer-Quelle fuer das Aktiv-Auslesen" -
            // dieselbe, bereits vorhandene Meldung wie auf der normalen
            // Statuszeile ohne Seriennummer (NO_DEVICE_NO_CONFIGURED),
            // damit es nur EINEN Text dafuer gibt.
            case SolarData::RefreshFeedback::NoDeviceNo:
                return I18n::t(I18n::Str::NO_DEVICE_NO_CONFIGURED);
        }
        return String();
    }

    // Kurzfassungen fuer schmale Kachel-Beschriftungszeilen (Auftrag
    // "Fehler in der Refresh-Rueckmeldung", Punkt 2) - dieselbe
    // Bedeutung, aber garantiert kuerzer als die Vollversion oben.
    inline String shortTextFor(SolarData::RefreshFeedback fb) {
        switch (fb) {
            case SolarData::RefreshFeedback::Working:
            case SolarData::RefreshFeedback::WorkingActiveRead:
                return I18n::t(I18n::Str::REFRESH_WORKING_SHORT);
            case SolarData::RefreshFeedback::Done:      return I18n::t(I18n::Str::REFRESH_DONE_SHORT);
            case SolarData::RefreshFeedback::NoChange:  return I18n::t(I18n::Str::REFRESH_NO_CHANGE_SHORT);
            case SolarData::RefreshFeedback::NoWifi:    return I18n::t(I18n::Str::REFRESH_NO_WIFI_SHORT);
            case SolarData::RefreshFeedback::Busy:      return I18n::t(I18n::Str::REFRESH_BUSY_SHORT);
            case SolarData::RefreshFeedback::Cooldown:  return I18n::t(I18n::Str::REFRESH_COOLDOWN_SHORT);
            case SolarData::RefreshFeedback::LockedNight: {
                char buf[24];
                snprintf(buf, sizeof(buf), I18n::t(I18n::Str::REFRESH_LOCKED_NIGHT_SHORT_FMT),
                         Config::DAY_START_HOUR, Config::DAY_START_MINUTE);
                return String(buf);
            }
            case SolarData::RefreshFeedback::NoDeviceNo:
                return I18n::t(I18n::Str::REFRESH_NO_DEVICE_SHORT);
        }
        return String();
    }

    // Misst die Vollversion bei der AKTUELL gesetzten Schriftgroesse
    // gegen maxW (z.B. die Kachel-Innenbreite) und liefert die
    // Kurzfassung, falls die Vollversion nicht einzeilig passt - Auftrag
    // "Textbreite gegen Kachelbreite messen". Ruft KEIN setTextSize()
    // selbst auf - der Aufrufer setzt die Groesse vorher explizit (siehe
    // CLAUDE.md "Schriftzustand"), genau wie bei den uebrigen
    // fitText()-aehnlichen Helfern im Projekt. Nimmt die bereits
    // AUFGELOESTEN Texte (FeedbackState::text/shortText) entgegen, NICHT
    // mehr den rohen SolarData::RefreshFeedback-Wert - der kann vom
    // tatsaechlich gerade angezeigten Text abweichen (z.B. nach der
    // 15s-Obergrenze in FeedbackState::update(), die lokal auf "Keine
    // neuen Daten" umschaltet, OHNE dass sich SolarData::refreshFeedback
    // selbst aendert).
    inline String fitToWidth(TFT_eSPI& t, const String& full, const String& shortVariant, int16_t maxW) {
        if (t.textWidth(full) <= maxW) return full;
        return shortVariant;
    }

    // Gebuendelter Zustand fuer EINE Kachel (je eigene Instanz pro
    // Kachel als lokales static-Array in der jeweiligen *_page.cpp,
    // NICHT mehr eine einzige geteilte Instanz fuer die ganze Seite -
    // Auftrag "Rueckmeldung pro Kachel, nicht pro Seite": vorher gab es
    // auf Seiten mit mehreren unabhaengig antippbaren Kacheln (z.B.
    // home_page.cpp) nur EINE FeedbackState-Instanz fuer alle Kacheln
    // zusammen, wodurch ein Tipp auf eine einzelne Kachel faelschlich
    // ALLE gleichzeitig betroffenen Kacheln umgeschaltet hat, obwohl
    // RefreshNow technisch nur EIN globales Ereignis ist - die Kachel-
    // Zuordnung "wer zeigt gerade die Rueckmeldung" gehoert deshalb zur
    // einzelnen Kachel, nicht zum globalen Ereignis selbst). Erkennt
    // neue Ereignisse (ueber refreshFeedbackSeq, siehe solar_data.h) und
    // haelt die Anzeige ca. 2s sichtbar. "Working" ist KEIN unbegrenzter
    // Wartezustand mehr: laeuft das naechste Ereignis nicht innerhalb von
    // Config::REFRESH_WORKING_TIMEOUT_MS (15s) ein, wird das selbst wie
    // "Keine neuen Daten" behandelt - verhindert, dass die Anzeige fuer
    // immer haengen bleibt, z.B. wenn das WLAN direkt nach dem Tipp kurz
    // wegfaellt, bevor der geplante Abruf ueberhaupt starten konnte
    // (beobachteter Fehler, siehe Bericht).
    struct FeedbackState {
        uint32_t lastSeq = 0;
        uint32_t showUntilMs = 0; // 0 = nicht aktiv
        bool indefinite = false;
        uint32_t workingStartMs = 0; // nur waehrend indefinite gueltig
        // Welche harte Obergrenze fuer DIESES "Working" gilt (Korrektur
        // B2) - Working (passiver Abruf) 15s, WorkingActiveRead (Auftrag
        // "Aktiv-Auslesen") 55s, da der Regler laut Messung bis zu ~27s
        // fuer eine frische Messung braucht. Beim Eintritt in indefinite
        // gesetzt, siehe update() unten.
        uint32_t workingTimeoutMs = Config::REFRESH_WORKING_TIMEOUT_MS;
        String text;
        String shortText; // siehe fitToWidth() - immer zusammen mit text gesetzt
        // Ob GERADE JETZT (nach diesem update()-Aufruf) ein Feedback-Text
        // anstelle der normalen Beschriftung gezeigt werden soll - das
        // ist die fuer die ANZEIGE massgebliche Information. Der
        // Rueckgabewert von update() ist etwas ANDERES (siehe dort):
        // "soll JETZT neu gezeichnet werden" - bei einem einzelnen,
        // uebergangsweisen Aufruf koennen sich beide unterscheiden
        // (genau EIN Aufruf, bei dem active bereits false ist, aber
        // trotzdem true zurueckgegeben wird, um die Normalanzeige
        // tatsaechlich auf den Bildschirm zu bringen).
        bool active = false;

        // Rueckgabewert = "soll JETZT (neu) gezeichnet werden" - fuer die
        // ANZEIGE-Entscheidung ("Feedback-Text statt normaler
        // Beschriftung zeigen?") das Feld `active` nach diesem Aufruf
        // lesen, NICHT den Rueckgabewert (siehe unten, Breitenpruefung
        // dafuer ueber RefreshFeedbackText::fitToWidth()).
        //
        // Auftrag "Rueckmeldung bleibt dauerhaft stehen" (Rueckmeldung
        // blieb z.B. permanent bei "Aktualisiert" statt zur normalen
        // Beschriftung zurueckzukehren): Ursache war ein fehlender
        // "Revert-Tick". Die Seiten rufen update() typischerweise aus
        // ZWEI Stellen auf: einmal in der Seiten-eigenen update()-Funktion
        // (entscheidet, OB neu gezeichnet wird) und einmal in draw()
        // (liest den aktuellen Text). Lief das 2s-Fenster GENAU zwischen
        // zwei Aufrufen ab, lieferte die FRUEHERE Version hier `false`
        // zurueck (korrekt fuer "ab jetzt nicht mehr aktiv"), aber KEIN
        // Aufrufer bekam dadurch das Signal "jetzt noch einmal neu
        // zeichnen, um zur Normalanzeige zurueckzukehren" - die zuletzt
        // gezeichneten Pixel ("Aktualisiert") blieben einfach so lange
        // stehen, bis zufaellig ein voellig anderer Grund (z.B. der
        // naechste echte Datenabruf) ohnehin einen Redraw ausloeste.
        // Fix: wasPending merkt sich den Zustand VOR dieser Auswertung -
        // der Uebergang von aktiv auf inaktiv liefert dadurch selbst
        // noch genau EIN "true" (der Aufrufer zeichnet einmal neu und
        // zeigt danach wieder korrekt die normale Beschriftung).
        // Auftrag "Flackern waehrend der Rueckmeldung": vorher lieferte
        // update() auf JEDEM Aufruf "true" zurueck, solange "Working"
        // (indefinite) aktiv war - nicht nur beim Beginn/Ende, sondern
        // auch waehrend der GESAMTEN ~15s, in denen sich am angezeigten
        // Text gar nichts aenderte. Jede Seite ORt diesen Rueckgabewert in
        // ihre eigene update()-Funktion, die bei "true" main_screen.cpp
        // zu einem kompletten drawBody(false) veranlasst - und DAS
        // zeichnete bei jedem einzelnen Schleifendurchlauf (viele Male
        // pro Sekunde) ungebremst die Fusszone (Seitenpunkte,
        // "?"-Knopf-Ueberlappung) neu, was als Flackern sichtbar war
        // (siehe main_screen.cpp::drawDots()/drawBody()). Fix: "true" wird
        // jetzt NUR noch genau dann zurueckgegeben, wenn sich der
        // angezeigte TEXT tatsaechlich aendert (neues Ereignis oder die
        // 15s-Zwangsumschaltung) oder beim Uebergang aktiv->inaktiv
        // (Revert-Tick, siehe `wasPending` oben) - nicht mehr bei jedem
        // Tick, in dem "Working" lediglich unveraendert weiterlaeuft.
        bool update(const SolarData::Data& snap) {
            bool wasPending = indefinite || (showUntilMs != 0);
            bool textChanged = false;
            if (snap.refreshFeedbackSeq != 0 && snap.refreshFeedbackSeq != lastSeq) {
                lastSeq = snap.refreshFeedbackSeq;
                text = textFor(snap.refreshFeedback);
                shortText = shortTextFor(snap.refreshFeedback);
                if (isTimed(snap.refreshFeedback)) {
                    indefinite = false;
                    // LockedNight (Auftrag) bewusst laenger sichtbar als
                    // die uebrigen, selbsterklaerenden Rueckmeldungen -
                    // sonst wird der Hinweis auf 04:30 Uhr leicht
                    // uebersehen.
                    uint32_t showMs = (snap.refreshFeedback == SolarData::RefreshFeedback::LockedNight)
                        ? Config::REFRESH_LOCKED_NIGHT_SHOW_MS : 2000;
                    showUntilMs = millis() + showMs;
                } else {
                    indefinite = true;
                    workingStartMs = millis();
                    showUntilMs = 0;
                    workingTimeoutMs = (snap.refreshFeedback == SolarData::RefreshFeedback::WorkingActiveRead)
                        ? Config::EPEVER_ACTIVE_READ_FEEDBACK_TIMEOUT_MS
                        : Config::REFRESH_WORKING_TIMEOUT_MS;
                }
                textChanged = true;
            }
            bool activeNow;
            if (indefinite) {
                if (millis() - workingStartMs >= workingTimeoutMs) {
                    // Harte Obergrenze erreicht - kein Folge-Ereignis
                    // eingetroffen. Wie "Keine neuen Daten" behandeln,
                    // dann ganz normal nach ca. 2s wieder verschwinden.
                    indefinite = false;
                    text = textFor(SolarData::RefreshFeedback::NoChange);
                    shortText = shortTextFor(SolarData::RefreshFeedback::NoChange);
                    showUntilMs = millis() + 2000;
                    activeNow = true;
                    textChanged = true;
                } else {
                    activeNow = true;
                }
            } else if (showUntilMs != 0 && (int32_t)(millis() - showUntilMs) < 0) {
                activeNow = true;
            } else {
                showUntilMs = 0;
                activeNow = false;
            }
            active = activeNow;
            bool justDeactivated = wasPending && !activeNow;
            return textChanged || justDeactivated;
        }
    };
}
