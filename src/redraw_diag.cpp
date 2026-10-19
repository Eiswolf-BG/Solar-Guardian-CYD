#include "redraw_diag.h"
#include "config.h"

namespace RedrawDiag {

void log(const char* page, const char* reason) {
    if (!Config::DEBUG_REDRAW_DIAGNOSTICS) return;
    Serial.printf("[redraw-diag] %s: %s\n", page, reason);
}

}
