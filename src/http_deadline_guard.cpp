#include "http_deadline_guard.h"
#include <esp_timer.h>

namespace HttpDeadlineGuard {

namespace {
    esp_timer_handle_t timerHandle = nullptr;
    WiFiClientSecure* guardedClient = nullptr;
    volatile bool firedFlag = false;

    // Laeuft im esp_timer-Service-Task (normaler Task-Kontext, KEINE
    // ISR) - WiFiClientSecure::stop() von dort aufzurufen ist der
    // ueblicher Weg, einen in read()/connect() blockierten Aufruf einer
    // ANDEREN Task (hier: dem Fetch-Task) von aussen abzubrechen.
    void onTimeout(void* /*arg*/) {
        firedFlag = true;
        if (guardedClient) guardedClient->stop();
    }
}

void init() {
    if (timerHandle) return;
    esp_timer_create_args_t args = {};
    args.callback = &onTimeout;
    args.name = "httpDeadline";
    esp_timer_create(&args, &timerHandle);
}

void arm(WiFiClientSecure* client, uint32_t timeoutMs) {
    if (!timerHandle) init();
    guardedClient = client;
    firedFlag = false;
    esp_timer_stop(timerHandle); // no-op falls bereits inaktiv
    esp_timer_start_once(timerHandle, (uint64_t)timeoutMs * 1000ULL);
}

void disarm() {
    if (timerHandle) esp_timer_stop(timerHandle);
    guardedClient = nullptr;
}

bool lastCallTimedOut() { return firedFlag; }

}
