#include "wifi_scan.h"
#include "config.h"
#include <WiFi.h>

namespace WifiScan {

uint8_t scan(Network* outList, uint8_t maxCount) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int found = WiFi.scanNetworks();
    if (found <= 0) return 0;

    uint8_t count = 0;
    for (int i = 0; i < found && count < maxCount; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;

        // Doppelte SSID (mehrere Access Points desselben Netzwerks, z.B.
        // Mesh/Repeater) zusammenfassen - nur den staerksten Treffer behalten.
        bool duplicate = false;
        for (uint8_t j = 0; j < count; j++) {
            if (outList[j].ssid == ssid) {
                duplicate = true;
                if (WiFi.RSSI(i) > outList[j].rssi) {
                    outList[j].rssi = WiFi.RSSI(i);
                    outList[j].open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
                }
                break;
            }
        }
        if (duplicate) continue;

        outList[count].ssid = ssid;
        outList[count].rssi = WiFi.RSSI(i);
        outList[count].open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
        count++;
    }

    // Nach Signalstaerke absteigend sortieren (einfacher Bubble-Sort, Liste
    // ist mit max. Config::MAX_WIFI_NETWORKS_SHOWN Eintraegen klein genug).
    for (uint8_t i = 0; i < count; i++) {
        for (uint8_t j = i + 1; j < count; j++) {
            if (outList[j].rssi > outList[i].rssi) {
                Network tmp = outList[i];
                outList[i] = outList[j];
                outList[j] = tmp;
            }
        }
    }

    WiFi.scanDelete();
    return count;
}

bool connect(const String& ssid, const String& password) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    if (password.length() == 0) {
        WiFi.begin(ssid.c_str());
    } else {
        WiFi.begin(ssid.c_str(), password.c_str());
    }

    uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startMs > Config::WIFI_CONNECT_TIMEOUT_MS) return false;
        delay(100);
    }
    return true;
}

uint8_t signalBars() {
    if (WiFi.status() != WL_CONNECTED) return 0;
    int32_t rssi = WiFi.RSSI();
    if (rssi >= -55) return 4;
    if (rssi >= -65) return 3;
    if (rssi >= -75) return 2;
    if (rssi >= -85) return 1;
    return 0;
}

}
