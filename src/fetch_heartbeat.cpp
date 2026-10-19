#include "fetch_heartbeat.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace FetchHeartbeat {

namespace {
    uint32_t lastBeatMs = 0;
    SemaphoreHandle_t mutex = nullptr;
}

void init() {
    if (!mutex) mutex = xSemaphoreCreateMutex();
    lastBeatMs = millis();
}

void beat() {
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    lastBeatMs = millis();
    xSemaphoreGive(mutex);
}

uint32_t msSinceLastBeat() {
    if (!mutex) return 0;
    uint32_t last;
    xSemaphoreTake(mutex, portMAX_DELAY);
    last = lastBeatMs;
    xSemaphoreGive(mutex);
    return (uint32_t)(millis() - last);
}

}
