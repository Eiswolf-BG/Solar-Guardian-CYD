#pragma once
// Die komplette Konfiguration, die auf der SD-Karte unter
// Config::SD_CONFIG_FILE gespeichert wird. Ein einziges globales Objekt
// (siehe appConfig unten) wird beim Start geladen bzw. waehrend der
// Ersteinrichtung befuellt und am Ende einmal gespeichert.
#include <Arduino.h>
#include "config.h"

struct AppConfig {
    // Touch-Kalibrierung (siehe touch_input.h/calibration_screen.cpp).
    // Der XPT2046-Touch-Chip ist fest an die PHYSISCHE Panel-Ausrichtung
    // gebunden, nicht an tft.setRotation() - ob der rohe X- oder Y-Wert des
    // Chips der Bildschirm-X-Achse entspricht (oder vertauscht ist), haengt
    // vom Rotationswinkel ab. touchSwapXY wird deshalb waehrend der
    // Kalibrierung selbst ermittelt (aus 3 angetippten Ecken), nicht fest
    // angenommen - robust gegen diese Boardbesonderheit, ohne sie vorher
    // erraten zu muessen.
    bool touchSwapXY = false;
    int16_t touchScreenXRawMin = 0;
    int16_t touchScreenXRawMax = 4095;
    int16_t touchScreenYRawMin = 0;
    int16_t touchScreenYRawMax = 4095;
    bool touchCalibrated = false;

    // WLAN - bis zu 3 gespeicherte Netzwerke (siehe networks_screen.h).
    // Ein leerer ssid-String bedeutet "freier Platz". lastActiveWifiSlot
    // merkt sich, welches Netzwerk beim letzten Mal verbunden war (255 =
    // keins) - wird beim Start zuerst probiert (siehe wifi_connect_manager.h).
    struct WifiSlot {
        String ssid;
        String password;
    };
    WifiSlot wifiSlots[3];
    uint8_t lastActiveWifiSlot = 255;

    // Zugangsdaten Solar Guardian Portal (EPEVER-Cloud)
    String portalEmail;
    String portalPassword;

    // Geraete-Seriennummer fuer die Port-7002-Batterieabfrage - kein
    // Geheimnis. Standardwert ist Steffens Regler, siehe config.h.
    String deviceNo = Config::DEFAULT_DEVICE_NO;

    // Standort fuer die Wetterabfrage - leer/0 = noch kein Standort
    // bekannt, die Wetterkachel zeigt dann "Standort waehlen" statt eines
    // Symbols. locationSet unterscheidet das explizit von echten
    // Koordinaten 0.0/0.0 (z.B. Null-Insel). Wird normalerweise
    // automatisch ueber die oeffentliche IP-Adresse ermittelt (siehe
    // ip_geolocation_client.h) - locationManual=true (von Hand ueber die
    // Wetterkachel/location_screen.cpp gesetzt) schaltet das ab, die
    // automatische Ermittlung ueberschreibt dann nichts mehr.
    String locationName;
    float locationLat = 0.0f;
    float locationLon = 0.0f;
    bool locationSet = false;
    bool locationManual = false;

    // Darstellung
    uint8_t systemColorIndex = 0; // Index in Config::COLOR_PALETTE, siehe config.h
    // Helligkeit der Hintergrundbeleuchtung in Prozent (5-100, siehe
    // Config::BRIGHTNESS_MIN_PERCENT) - angewendet per PWM in
    // Display::applyBrightness().
    uint8_t brightnessPercent = 100;

    // Sprache der App (0 = Deutsch, 1 = Englisch, siehe i18n.h). Standard
    // Deutsch.
    uint8_t language = 0;

    // Version 1: einzelnes WLAN (wifiSsid/wifiPassword). Version 2: drei
    // WLAN-Plaetze (wifiSlots). Version 3: Sprache (language). Version 4:
    // Geraete-Seriennummer (deviceNo). Version 5: Wetter-Standort
    // (locationName/locationLat/locationLon/locationSet) - siehe
    // AppConfigStore::load() fuer die automatische Migration alter
    // Versionen.
    uint8_t formatVersion = 5;

    // true, sobald die Ersteinrichtung (Touch, WLAN, Zugangsdaten) einmal
    // komplett durchlaufen und gespeichert wurde.
    bool setupComplete = false;
};

// Einziges globales Konfigurationsobjekt der App - waehrend der
// Ersteinrichtung direkt befuellt, danach per AppConfigStore::save()
// einmalig auf die SD-Karte geschrieben.
extern AppConfig appConfig;

namespace AppConfigStore {
    // Liest config.json von der SD-Karte in appConfig. Gibt false zurueck,
    // wenn die Datei fehlt oder nicht lesbar/beschaedigt ist (appConfig
    // bleibt dann auf den Default-Werten).
    bool load();

    // Schreibt appConfig als config.json auf die SD-Karte. Schreibt zuerst
    // in eine temporaere Datei und benennt sie erst danach um, damit bei
    // einem Stromausfall mitten im Schreiben die alte Datei nicht
    // beschaedigt wird.
    bool save();
}
