#include "ip_location_data.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace IpLocationData {

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

void setResult(float lat, float lon, const String& name, bool valid) {
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    data.lat = lat;
    data.lon = lon;
    data.name = name;
    data.valid = valid;
    data.changeCounter++;
    xSemaphoreGive(mutex);
}

}
