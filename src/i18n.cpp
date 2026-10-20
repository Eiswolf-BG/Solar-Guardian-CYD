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
        "Weiter",
        "Hole Uhrzeit...",

        // Seriennummer (Ersteinrichtung UND Konto-Screen)
        "Seriennummer",
        "Seriennummer eingeben",
        "Hier trägst du die Seriennummer deines Solar-Guardian-Geräts ein oder änderst sie. Sie sagt der App, von welcher Anlage die Daten abgefragt werden. Du findest sie im Solar-Guardian-Portal auf der Geräteseite, in der Adresszeile hinter devid=.",
        "Zum Ändern antippen",
        "Speichern",
        "Bei einer anderen Anlage bitte unter Zurücksetzen das Ertragsbuch neu beginnen.",

        // Hauptbildschirm / Seiten
        "Übersicht",
        "Leistung",
        "Ertrag",
        "Jahre",
        "Batterie",
        "folgt",
        "Kein Konto eingerichtet",

        // Hauptscreen "Uebersicht" (4 Kacheln)
        "Standort wählen",
        "Wetter wird geladen",

        // Seite "Leistung"
        "W",
        "kW",
        "jetzt",
        "Stand %02d:%02d Uhr",

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
        "Laden (schwach)",
        "Die Batterie wird gerade nicht geladen.",
        "Die Batterie ist fast voll, es wird nur wenig nachgeladen.",
        "Die Batterie wird mit voller Leistung geladen.",
        "Der Regler meldet Ausgleichsladung.",
        "Die Batterie lädt gerade nur mit geringer Stromstärke.",
        "Nächste Abfrage: %s Uhr",

        // Wochenansicht (Wetterkachel antippen)
        "Morgen",
        "h",
        "So",
        "Mo",
        "Di",
        "Mi",
        "Do",
        "Fr",
        "Sa",

        // Menue
        "Menü",
        "Netzwerke",
        "Konto",
        "Systemfarbe",
        "Helligkeit",
        "Sprache / Language",
        "Ertragswerte",
        "Zurücksetzen",

        // Netzwerke
        "Netzwerke",
        "Passwort ändern",
        "Neues Passwort für %s",
        "WebUI",

        // WebUI
        "WebUI",
        "Öffne diese Adresse im Browser eines Geräts im selben WLAN, um alle Werte groß auf Handy oder PC zu sehen.",
        "Kein WLAN",

        // Systemfarbe
        "Systemfarbe wählen",

        // Helligkeit
        "Helligkeit",

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

        // Standort (Wetter)
        "Standort",
        "Ort oder Postleitzahl eingeben",
        "Suche...",
        "Nichts gefunden",
        "Hinweis: ae, oe, ue statt ä, ö, ü eingeben",
        "Treffer",
        "Automatisch ermitteln",
        "Zum Ändern antippen",

        // Zuruecksetzen
        "Zurücksetzen",
        "Alle Einstellungen, Netzwerke, Zugangsdaten und die gezählten Ertragswerte (Monat, Jahr, Vorjahreswerte, Ertragsprotokoll) werden gelöscht. Der Ordner SolarHistory bleibt erhalten. Das Gerät startet danach neu mit der Ersteinrichtung.",
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
        "Zeigt das Wetter (antippen für die Wochenvorhersage), die aktuelle Leistung, den heutigen Ertrag und den Ladezustand der Batterie auf einen Blick. Leistung, Ertrag und Batterie antippen springt zur jeweiligen Seite.",
        "Zeigt, wie viel Strom die Solarmodule gerade liefern, dazu Spannung und Strom der Module und den Verlauf der letzten 3 Stunden. Die Werte kommen vom Solarregler über das Solar-Guardian-Portal und aktualisieren sich alle 5 Minuten. Nachts ist 0 W normal.",
        "Zeigt den erzeugten Strom heute, in den letzten 7 Tagen (heute und die 6 Tage davor), in diesem Monat und in diesem Jahr. Woche, Monat und Jahr zählt das Gerät selbst, weil die Summen des Portals Lücken haben. Die Werte aktualisieren sich alle 5 Minuten.",
        "Zeigt den Ertrag pro Kalenderjahr. Das laufende Jahr zählt das Gerät selbst, die Vorjahre sind gespeichert.",
        "Zeigt Ladezustand, Spannung, Strom, Temperatur und Ladestatus, aktualisiert alle 5 Minuten. Kein Laden: nicht geladen. Erhaltung: fast voll. Schnellladen: volle Leistung. Ausgleich: kurz höhere Spannung, gleicht die Zellen aus, normal.",
        "Das Menü mit allen Einstellungen.",
        "Bis zu 3 WLAN-Netzwerke speichern und verwalten.",
        "Netzwerk suchen und verbinden.",
        "E-Mail und Passwort für das Solar Guardian Portal eingeben.",
        "Zugangsdaten ändern oder entfernen, Geräte-Seriennummer antippen zum Ändern.",
        "Geräte-Seriennummer für die Batterie- und Leistungsabfrage eingeben oder ändern.",
        "Farbe für Schrift und Symbole auswählen.",
        "Helligkeit der Hintergrundbeleuchtung einstellen - mit dem Finger auf dem Regler ziehen. Mindestens 5%, damit das Display nie komplett dunkel wird.",
        "Zeigt die Adresse der Web-Ansicht. Diese Adresse im Browser eines Geräts im selben WLAN öffnen, um alle Werte groß auf Handy oder PC zu sehen - nur Anzeige, keine Einstellungen.",
        "Sprache der App auswählen.",
        "Alle Einstellungen löschen und neu einrichten.",
        "Hier kannst du Monat und Jahr korrigieren. Der Wert gilt einschließlich heute.",
        "Der Standort wird normalerweise automatisch über die IP-Adresse ermittelt. Hier kannst du ihn stattdessen von Hand eingeben (Ort oder Postleitzahl, aus den Treffern auswählen) oder die automatische Ermittlung erneut anstoßen.",

        // Startlogo
        "wacht über deine Anlage",
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
        "Next",
        "Getting time...",

        // Serial number (first-time setup AND account screen)
        "Serial number",
        "Enter serial number",
        "Enter or change the serial number of your Solar Guardian device here. It tells the app which solar system to read data from. You can find it in the Solar Guardian portal on the device page, in the address bar after devid=.",
        "Tap to change",
        "Save",
        "For a different installation, please restart the yield log under Factory reset.",

        // Main screen / pages
        "Overview",
        "Power",
        "Yield",
        "Years",
        "Battery",
        "coming soon",
        "No account set up",

        // Main screen "Overview" (4 tiles)
        "Set location",
        "Loading weather",

        // "Power" page
        "W",
        "kW",
        "now",
        "As of %02d:%02d",

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
        "Charging (weak)",
        "The battery is not being charged right now.",
        "The battery is almost full, only a small top-up charge is applied.",
        "The battery is being charged at full power.",
        "The controller reports equalize charging.",
        "The battery is only charging at a low current right now.",
        "Next update: %s",

        // Week view (tap the weather tile)
        "Tomorrow",
        "h",
        "Sun",
        "Mon",
        "Tue",
        "Wed",
        "Thu",
        "Fri",
        "Sat",

        // Menu
        "Menu",
        "Networks",
        "Account",
        "Theme color",
        "Brightness",
        "Language / Sprache",
        "Yield values",
        "Factory reset",

        // Networks
        "Networks",
        "Change password",
        "New password for %s",
        "WebUI",

        // WebUI
        "WebUI",
        "Open this address in a browser on a device in the same WiFi network to see all values large on your phone or PC.",
        "No WiFi",

        // Theme color
        "Choose theme color",

        // Brightness
        "Brightness",

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

        // Location (weather)
        "Location",
        "Enter place or postal code",
        "Searching...",
        "Nothing found",
        "Hint: type ae, oe, ue instead of ä, ö, ü",
        "Results",
        "Detect automatically",
        "Tap to change",

        // Factory reset
        "Factory reset",
        "All settings, networks, account details and your counted yield values (month, year, previous years, the yield log) will be deleted. The SolarHistory folder is kept. The device will then restart into first-time setup.",
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
        "Shows the weather (tap for the week forecast), the current power output, today's yield, and the battery charge level at a glance. Tap Power, Yield, or Battery to jump to that page.",
        "Shows how much power the solar panels are currently delivering, plus their voltage and current and the curve of the last 3 hours. The values come from the solar controller via the Solar Guardian portal and refresh every 5 minutes. 0 W at night is normal.",
        "Shows the power generated today, in the last 7 days (today and the 6 days before), this month, and this year. The device counts week, month, and year itself because the portal's totals have gaps. The values refresh every 5 minutes.",
        "Shows the yield per calendar year. The device counts the current year itself, previous years are stored.",
        "Shows charge level, voltage, current, temperature, and charging status, refreshed every 5 minutes. Not charging: no charge now. Float: almost full. Boost: full power. Equalize: brief higher voltage that balances the cells, normal.",
        "The menu with all settings.",
        "Save and manage up to 3 WiFi networks.",
        "Find a network and connect to it.",
        "Enter your email and password for the Solar Guardian portal.",
        "Change or remove your account details, tap the serial number to change it.",
        "Enter or change the device serial number used for the battery and power queries.",
        "Choose the color used for text and icons.",
        "Set the backlight brightness - drag your finger on the slider. At least 5% so the display never goes completely dark.",
        "Shows the address of the web view. Open this address in a browser on a device in the same WiFi network to see all values large on your phone or PC - view only, no settings.",
        "Choose the app language.",
        "Delete all settings and set up again.",
        "Here you can correct month and year. The value you enter counts including today.",
        "The location is normally detected automatically from your IP address. Here you can enter it by hand instead (place or postal code, pick one of the results) or trigger automatic detection again.",

        // Startlogo
        "watching over your system",
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
