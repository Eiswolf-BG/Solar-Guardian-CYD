#pragma once
// Automatische Standort-Ermittlung ueber die oeffentliche IP-Adresse
// (kostenlos, ohne Zugangsschluessel) - Hauptdienst ipwho.is (liefert
// Breite/Laenge als echte JSON-Zahlen, einfacher/sicherer zu parsen),
// bei Fehler Rueckfall auf geojs.io (unabhaengiger zweiter Anbieter,
// liefert die Koordinaten als Text, siehe .cpp). Genauigkeit liegt nur
// auf Stadt-Ebene (Auftrag).
#include <Arduino.h>

namespace IpGeolocationClient {

    struct Result {
        float lat = 0.0f;
        float lon = 0.0f;
        String name; // Ortsname, fuer die Anzeige - NIE loggen (Auftrag)
        bool valid = false;
    };

    // Versucht ipwho.is, bei Fehler geojs.io. Gibt true bei Erfolg
    // zurueck. outHttpCode = Code des ZULETZT versuchten Diensts (fuer
    // Diagnose, keine Zugangsdaten/Koordinaten enthalten).
    bool detect(Result& out, int* outHttpCode);
}
