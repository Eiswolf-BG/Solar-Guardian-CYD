#include "wifi_status.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace WifiStatus {

namespace {
    Status current;
    SemaphoreHandle_t mutex = nullptr;
}

void init() {
    if (!mutex) mutex = xSemaphoreCreateMutex();
}

void set(bool connected, uint8_t slot) {
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    current.connected = connected;
    current.slot = slot;
    xSemaphoreGive(mutex);
}

Status get() {
    if (!mutex) return current;
    Status copy;
    xSemaphoreTake(mutex, portMAX_DELAY);
    copy = current;
    xSemaphoreGive(mutex);
    return copy;
}

}
