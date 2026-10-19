#pragma once
// Standort fuer die Wetterabfrage - KEIN eigener Menuepunkt und KEIN
// Schritt in der Ersteinrichtung mehr (Auftrag "Standort automatisch,
// notfalls einmalig per Eingabe"): der Standort wird normalerweise
// automatisch ueber die oeffentliche IP-Adresse ermittelt (siehe
// ip_geolocation_client.h, laeuft im Fetch-Task). Dieser Screen ist nur
// noch ueber die Wetterkachel (ohne Standort) bzw. die Wochenansicht
// ("Zum Aendern antippen", mit Standort) erreichbar - siehe
// home_page.cpp/week_view_screen.cpp.
#include <Arduino.h>

namespace LocationScreen {
    // Zeigt Eingabefeld (Suche) + Trefferliste + "Automatisch ermitteln"
    // + "Zurueck". Speichert bei Auswahl/Automatisch-Ermitteln sofort
    // (AppConfigStore::save()) und benachrichtigt den Fetch-Task
    // (Command::LocationChanged).
    void run();
}
