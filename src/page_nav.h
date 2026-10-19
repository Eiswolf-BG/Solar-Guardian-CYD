#pragma once
// Kleiner Helfer, mit dem eine Seite (siehe pages.h, z.B. home_page.cpp)
// beim Antippen einer eigenen Kachel zu einer ANDEREN Wisch-Seite
// springen kann - der Seitenindex (currentPage) lebt privat in
// main_screen.cpp, deshalb dieser schmale Umweg statt direktem Zugriff.
#include <Arduino.h>

namespace PageNav {
    // Von einer Seite's onTap() aufrufen, um nach dem aktuellen Tick zur
    // Seite mit diesem Index zu springen.
    void requestPage(uint8_t index);

    // Von main_screen.cpp nach jedem onTap()-Aufruf geprueft - true =
    // ein Sprung wurde angefordert (outIndex gesetzt, Anforderung danach
    // geloescht).
    bool consume(uint8_t& outIndex);
}
