#pragma once
// Schreibgeschuetzte Web-Ansicht (Auftrag "WebUI") - zeigt dieselben
// Werte wie das Display, groesser, im Browser eines Geraets im selben
// WLAN. KEIN eigener Task (bewusst, siehe CLAUDE.md/Auftrag - ein
// eigener Task fuehrte in einem anderen Projekt zu Darstellungsfehlern):
// server.handleClient() laeuft im normalen Core-1-Ablauf
// (main.cpp::loop()), genau wie alle anderen Core-1-Aufgaben. Liest nur
// aus bestehenden Mutex-/Snapshot-geschuetzten Datenquellen
// (SolarData::snapshot(), WeatherData::snapshot(), YieldLedger), nie
// direkt. Keine Schreib-/Steuer-Endpunkte, keine Zugangsdaten/Token/
// Seriennummer/Koordinaten in der Antwort - nur der Ortsname darf
// erscheinen.
#include <Arduino.h>

namespace WebServer_ {
    // In main.cpp::loop() aufrufen (Core 1, jede Iteration) - startet den
    // Server beim ersten Mal, sobald WLAN verbunden ist, und bedient
    // danach server.handleClient().
    void tick();
}
