#include "i18n.h"
#include "app_config.h"

namespace I18n {

namespace {
    // Reihenfolge MUSS exakt zum enum Str in i18n.h passen - der
    // static_assert unten prueft nur die Anzahl, nicht die Zuordnung.
    constexpr const char* DE[] = {
        // Allgemein
        "Zurück",
        "Abbrechen",
        "OK",
        "Verbinden",
        "Löschen",
        "Verbinde...",
        "Verbindung fehlgeschlagen",
        "frei",
        "E-Mail:",
        "Passwort:",
        "(antippen)",
        "Leer",

        // Ersteinrichtung
        "Konfiguration beschädigt",
        "Ersteinrichtung startet neu",
        "Kreuz antippen (%d/%d)",
        "Kalibrierung fertig",
        "WLAN auswählen",
        "Suche WLAN-Netzwerke...",
        "Neu scannen",
        "Keine Netzwerke gefunden.",
        "Passwort für %s",
        "Solar Guardian Zugangsdaten",
        "E-Mail eingeben",
        "Passwort eingeben",
        "Los geht's",
        "Hole Uhrzeit...",

        // Hauptbildschirm / Seiten
        "Leistung",
        "Ertrag",
        "Jahre",
        "Batterie",
        "folgt",
        "Kein Konto eingerichtet",

        // Seite "Ertrag"
        "Heute",
        "Woche",
        "Monat",
        "Jahr",
        "kWh",
        "Aktualisiert %02d:%02d",
        "veraltet",
        "Anmeldung läuft...",
        "Anmeldung fehlgeschlagen",
        "Keine Verbindung",

        // Seite "Batterie"
        "Spannung",
        "Strom",
        "Temperatur",
        "Status",
        "V",
        "A",
        "°C",
        "Kein Laden",
        "Erhaltung",
        "Schnellladen",
        "Ausgleich",
        "Die Batterie wird gerade nicht geladen.",
        "Die Batterie ist fast voll, es wird nur wenig nachgeladen.",
        "Die Batterie wird mit voller Leistung geladen.",
        "Kurzes Laden mit etwas höherer Spannung. Das gleicht die Zellen aus und ist normal.",
        "Messung %02d:%02d",

        // Menue
        "Menü",
        "Netzwerke",
        "Konto",
        "Systemfarbe",
        "Sprache / Language",
        "Ertragswerte",
        "Zurücksetzen",

        // Netzwerke
        "Netzwerke",
        "Passwort ändern",
        "Neues Passwort für %s",

        // Systemfarbe
        "Systemfarbe wählen",

        // Konto
        "Konto",
        "Benutzername ändern",
        "Neuen Benutzernamen eingeben",
        "Konto löschen",
        "Konto löschen?",
        "Die Zugangsdaten werden vom Gerät entfernt. WLAN-Netzwerke und die gezählten Ertragswerte bleiben erhalten.",

        // Sprache
        "Sprache / Language",
        "Sprache wählen",
        "Deutsch",
        "English",

        // Zuruecksetzen
        "Zurücksetzen",
        "Alle Einstellungen, Netzwerke, Zugangsdaten und die gezählten Ertragswerte (Monat, Jahr, Vorjahreswerte, Ertragsprotokoll und der Ordner History) werden gelöscht. Das Gerät startet danach neu mit der Ersteinrichtung.",
        "OK, löschen",
        "Lösche...",
        "Löschen fehlgeschlagen",

        // "Ertragswerte"-Menue
        "Monat setzen",
        "Jahr setzen",
        "Wert inkl. heute",
        "Neuer Wert (kWh)",
        "Ungültige Eingabe",
        "Wert zu niedrig",

        // Hilfetexte
        "Hier erscheint bald die aktuelle PV-Leistung.",
        "Zeigt den erzeugten Strom heute, in den letzten 7 Tagen (heute und die 6 Tage davor), in diesem Monat und in diesem Jahr. Woche, Monat und Jahr zählt das Gerät selbst, weil die Summen des Portals Lücken haben. Die Werte aktualisieren sich alle 5 Minuten.",
        "Zeigt den Ertrag pro Kalenderjahr. Das laufende Jahr zählt das Gerät selbst, die Vorjahre sind gespeichert.",
        "Zeigt Ladezustand, Spannung, Strom, Temperatur und Ladestatus, aktualisiert alle 5 Minuten. Kein Laden: nicht geladen. Erhaltung: fast voll. Schnellladen: volle Leistung. Ausgleich: kurz höhere Spannung, gleicht die Zellen aus, normal.",
        "Das Menü mit allen Einstellungen.",
        "Bis zu 3 WLAN-Netzwerke speichern und verwalten.",
        "Netzwerk suchen und verbinden.",
        "E-Mail und Passwort für das Solar Guardian Portal eingeben.",
        "Zugangsdaten ändern oder entfernen.",
        "Farbe für Schrift und Symbole auswählen.",
        "Sprache der App auswählen.",
        "Alle Einstellungen löschen und neu einrichten.",
        "Hier kannst du Monat und Jahr korrigieren. Der Wert gilt einschließlich heute.",
    };

    constexpr const char* EN[] = {
        // General
        "Back",
        "Cancel",
        "OK",
        "Connect",
        "Delete",
        "Connecting...",
        "Connection failed",
        "free",
        "Email:",
        "Password:",
        "(tap here)",
        "Space",

        // Setup
        "Configuration damaged",
        "Restarting setup",
        "Tap the cross (%d/%d)",
        "Calibration done",
        "Select WiFi",
        "Scanning for WiFi networks...",
        "Scan again",
        "No networks found.",
        "Password for %s",
        "Solar Guardian account",
        "Enter email",
        "Enter password",
        "Let's go",
        "Getting time...",

        // Main screen / pages
        "Power",
        "Yield",
        "Years",
        "Battery",
        "coming soon",
        "No account set up",

        // "Yield" page
        "Today",
        "Week",
        "Month",
        "Year",
        "kWh",
        "Updated %02d:%02d",
        "stale",
        "Signing in...",
        "Sign-in failed",
        "No connection",

        // "Battery" page
        "Voltage",
        "Current",
        "Temperature",
        "Status",
        "V",
        "A",
        "°C",
        "Not charging",
        "Float",
        "Boost",
        "Equalize",
        "The battery is not being charged right now.",
        "The battery is almost full, only a small top-up charge is applied.",
        "The battery is being charged at full power.",
        "A brief charge at a slightly higher voltage. This balances the cells and is normal.",
        "Reading %02d:%02d",

        // Menu
        "Menu",
        "Networks",
        "Account",
        "Theme color",
        "Language / Sprache",
        "Yield values",
        "Factory reset",

        // Networks
        "Networks",
        "Change password",
        "New password for %s",

        // Theme color
        "Choose theme color",

        // Account
        "Account",
        "Change username",
        "Enter new username",
        "Delete account",
        "Delete account?",
        "The account details will be removed from this device. Saved WiFi networks and your counted yield values are kept.",

        // Language
        "Language / Sprache",
        "Choose language",
        "Deutsch",
        "English",

        // Factory reset
        "Factory reset",
        "All settings, networks, account details and your counted yield values (month, year, previous years, the yield log, and the History folder) will be deleted. The device will then restart into first-time setup.",
        "OK, delete",
        "Deleting...",
        "Deletion failed",

        // "Yield values" menu
        "Set month",
        "Set year",
        "Value incl. today",
        "New value (kWh)",
        "Invalid input",
        "Value too low",

        // Help texts
        "The current PV power will appear here soon.",
        "Shows the power generated today, in the last 7 days (today and the 6 days before), this month, and this year. The device counts week, month, and year itself because the portal's totals have gaps. The values refresh every 5 minutes.",
        "Shows the yield per calendar year. The device counts the current year itself, previous years are stored.",
        "Shows charge level, voltage, current, temperature, and charging status, refreshed every 5 minutes. Not charging: no charge now. Float: almost full. Boost: full power. Equalize: brief higher voltage that balances the cells, normal.",
        "The menu with all settings.",
        "Save and manage up to 3 WiFi networks.",
        "Find a network and connect to it.",
        "Enter your email and password for the Solar Guardian portal.",
        "Change or remove your account details.",
        "Choose the color used for text and icons.",
        "Choose the app language.",
        "Delete all settings and set up again.",
        "Here you can correct month and year. The value you enter counts including today.",
    };

    static_assert(sizeof(DE) / sizeof(DE[0]) == (size_t)Str::COUNT, "DE table size mismatch - add/remove an entry to match enum Str");
    static_assert(sizeof(EN) / sizeof(EN[0]) == (size_t)Str::COUNT, "EN table size mismatch - add/remove an entry to match enum Str");
}

const char* t(Str id) {
    size_t idx = (size_t)id;
    if (idx >= (size_t)Str::COUNT) idx = 0;
    return (appConfig.language == 1) ? EN[idx] : DE[idx];
}

}
