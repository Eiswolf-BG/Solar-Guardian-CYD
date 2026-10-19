#include "page_nav.h"

namespace PageNav {

namespace {
    bool pending = false;
    uint8_t pendingIndex = 0;
}

void requestPage(uint8_t index) {
    pending = true;
    pendingIndex = index;
}

bool consume(uint8_t& outIndex) {
    if (!pending) return false;
    pending = false;
    outIndex = pendingIndex;
    return true;
}

}
