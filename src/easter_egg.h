#pragma once
// Verstecktes Zusatzfeature - Ablauf bewusst nicht dokumentiert (siehe
// Auftrag). Zertifikatsdatei lebt in Config::SD_HISTORY_DIR (ueberlebt
// "Zuruecksetzen").
#include <Arduino.h>

namespace EasterEgg {

    // Frueh beim Start pruefen (vor dem Logo, SD muss dafuer schon
    // gemountet sein).
    bool certificateValid();

    // Bei jedem Tipp auf die dafuer vorgesehene Stelle aufrufen - zaehlt
    // intern mit Zeitfenster, schreibt bei Erreichen der Zielzahl das
    // Zertifikat und liefert in GENAU diesem Moment true (nur dann soll
    // der Aufrufer eine Rueckmeldung zeigen).
    bool registerTap();

    // Bei jedem Tipp ausserhalb der dafuer vorgesehenen Stelle aufrufen -
    // setzt die Zaehlung zurueck.
    void resetTapCount();

}
