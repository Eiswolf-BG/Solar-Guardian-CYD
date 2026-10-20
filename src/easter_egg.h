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
    // intern mit Zeitfenster. Bei Erreichen der Zielzahl wird der
    // Zustand umgeschaltet (Zertifikat schreiben bzw. loeschen) und bei
    // Erfolg GENAU in diesem Moment true geliefert (outNowUnlocked =
    // neuer Zustand, nur dann gueltig). Schlaegt das Schreiben/Loeschen
    // fehl, bleibt der Zustand unveraendert und es kommt false zurueck.
    bool registerTap(bool& outNowUnlocked);

    // Bei jedem Tipp ausserhalb der dafuer vorgesehenen Stelle aufrufen -
    // setzt die Zaehlung zurueck.
    void resetTapCount();

}
