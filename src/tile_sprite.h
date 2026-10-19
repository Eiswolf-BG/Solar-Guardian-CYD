#pragma once
// Flackerfreies Zeichnen dynamischer Bereiche (Auftrag "Flackern, Teil 2"
// - echtes Off-Screen-Puffern statt Loeschen+Neuzeichnen). Siehe
// CLAUDE.md "Dynamische Bereiche immer ueber Off-Screen-Puffer zeichnen".
#include <TFT_eSPI.h>
#include <functional>

namespace TileSprite {
    // Zeichnet in einen TEMPORAEREN Off-Screen-Puffer (TFT_eSprite,
    // 16-Bit, exakt w x h gross - klein, nur fuer die Dauer dieses einen
    // Aufrufs alloziert und direkt danach wieder freigegeben, KEIN
    // dauerhaft gehaltener Speicherblock), ruft drawFn(canvas) auf -
    // canvas-LOKALE Koordinaten ab (0,0), NICHT die absoluten
    // Bildschirmkoordinaten des Aufrufers! - und schiebt das Ergebnis
    // dann mit GENAU EINEM pushSprite() an die Bildschirmposition (x,y).
    // Dadurch entsteht nie eine sichtbare Luecke (kein Schwarz-Aufblitzen
    // zwischen Loeschen und Neuzeichnen), egal wie oft aufgerufen wird.
    // Schlaegt die Zuteilung fehl (sehr knapper Heap-Moment), zeichnet
    // drawFn stattdessen direkt auf den echten Bildschirm (kann dann
    // kurz flackern, aber nie unsichtbar bleiben).
    void drawInto(int16_t x, int16_t y, int16_t w, int16_t h,
                  const std::function<void(TFT_eSPI&)>& drawFn);
}
