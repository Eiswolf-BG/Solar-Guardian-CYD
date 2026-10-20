#pragma once
// Startlogo "Sonne im Schild" - erscheint bei jedem Start fuer
// Config::BOOT_LOGO_MS. Komplett im Code gezeichnet (keine Bilddatei),
// mit denselben weichen Zeichenfunktionen wie die Wettersymbole.
#include <Arduino.h>

namespace BootLogo {

    // Zeichnet Schild, Sonne (ohne Strahlen) und die Texte, startet die
    // interne Zeit-/Animationsverfolgung. "unlockedVariant" zeigt
    // zusaetzlich den oberen Schriftzug (siehe easter_egg.h) - Details
    // bewusst nicht dokumentiert.
    void begin(bool unlockedVariant);

    // Wiederholt aufrufen, bis true zurueckkommt (kein eigenes delay()
    // von aussen noetig) - zeichnet die Strahlen-Animation in den ersten
    // ~1,5s nach, verwirft dabei jeden Touch (Logo ignoriert Eingaben).
    // Liefert true, sobald Config::BOOT_LOGO_MS seit begin() vergangen
    // sind; danach keine weiteren Zeichenoperationen mehr.
    bool tick();

}
