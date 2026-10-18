#include "account_bridge.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace AccountBridge {

namespace {
    Snapshot current;
    SemaphoreHandle_t mutex = nullptr;
}

void init() {
    if (!mutex) mutex = xSemaphoreCreateMutex();
}

void set(const String& email, const String& password, const String& deviceNo) {
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    current.email = email;
    current.password = password;
    current.deviceNo = deviceNo;
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
