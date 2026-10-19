#include "location_bridge.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace LocationBridge {

namespace {
    Snapshot current;
    SemaphoreHandle_t mutex = nullptr;
}

void init() {
    if (!mutex) mutex = xSemaphoreCreateMutex();
}

void set(float lat, float lon, bool valid, bool manual) {
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    current.lat = lat;
    current.lon = lon;
    current.valid = valid;
    current.manual = manual;
    xSemaphoreGive(mutex);
}

Snapshot get() {
    if (!mutex) return current;
    Snapshot copy;
    xSemaphoreTake(mutex, portMAX_DELAY);
    copy = current;
    xSemaphoreGive(mutex);
    return copy;
}

}
