#pragma once
// Bildschirmtastatur (deutsches QWERTZ-Layout) fuer WLAN- und
// Portal-Passwoerter/Benutzernamen. Zeigt den eingegebenen Text immer im
// Klartext an (kein Sternchen) - bewusst, siehe CLAUDE.md.
#include <Arduino.h>

namespace Keypad {
    // Zeigt die Tastatur vollbildschirm, mit title oben und initialText
    // bereits im Eingabefeld. Blockiert, bis OK oder Abbrechen angetippt
    // wird. confirmed wird auf true gesetzt, wenn mit OK bestaetigt wurde -
    // bei Abbruch false, Rueckgabewert dann == initialText (unveraendert).
    // startOnNumbersPage = true startet auf der Zahlen-/Sonderzeichen-
    // Ebene statt auf Buchstaben (z.B. fuer Zahleneingaben).
    String run(const char* title, const String& initialText, bool& confirmed, bool startOnNumbersPage = false);
}
