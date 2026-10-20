#include "device_no_resolution.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace DeviceNoResolution {

namespace {
    Data data;
    SemaphoreHandle_t mutex = nullptr;
}

void init() {
    if (!mutex) mutex = xSemaphoreCreateMutex();
}

Data snapshot() {
    if (!mutex) return data;
    Data copy;
    xSemaphoreTake(mutex, portMAX_DELAY);
    copy = data;
    xSemaphoreGive(mutex);
    return copy;
}

void setResult(const String& gatewayId) {
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    data.gatewayId = gatewayId;
    data.shouldApply = true;
    data.changeCounter++;
    xSemaphoreGive(mutex);
}

}
