#pragma once
// WebUI-Reiter "Daten" (Auftrag "WebUI-Reiter Daten") - Datei-Explorer
// fuer die SD-Karte ueber die WebUI. Laeuft komplett auf Core 1 (wie der
// Rest von web_server.cpp), benutzt dieselbe SD-Karte ohne eigenen
// Zugriffsschutz-Mechanismus (kein paralleler Zugriff auf Core 0, siehe
// CLAUDE.md "Dual-Core-Architektur" - Core 0 greift nie auf die SD-Karte
// zu). /SolarHistory ist fuer ALLE Operationen hier zentral gesperrt,
// siehe web_files.cpp::isLockedPath().
#include <WebServer.h>

namespace WebFiles {
    // Registriert die /api/files/*-Routen am uebergebenen Server-Objekt.
    void registerRoutes(WebServer& server);
}
