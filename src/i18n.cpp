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
        "Die Seriennummer wird nach dem Login automatisch vom Portal geholt und muss normalerweise nicht eingetragen werden. Nur bei Problemen oder mehreren Geräten hier eintragen - du findest sie im Solar-Guardian-Portal auf der Geräteseite, in der Adresszeile hinter devid=.",
        "Zum Ändern antippen",
        "Speichern",
        "Bei einer anderen Anlage bitte unter Zurücksetzen das Ertragsbuch neu beginnen.",
        "Verbinde mit dem Portal …",

        // Hauptbildschirm / Seiten
        "Übersicht",
        "Leistung",
        "Ertrag",
        "Jahre",
        "Batterie",
        "folgt",
        "Kein Konto eingerichtet",
        "Seriennummer fehlt. Bitte im Menü unter Konto eintragen.",

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
        "Portaldaten %d Min alt",
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
        "Nicht geladen.",
        "Fast voll.",
        "Lädt voll.",
        "Ausgleichsladung.",
        "Lädt schwach.",
        "Nächste Abfrage: %s Uhr",
        "Max",
        "Min",
        "Aktualisiere...",
        "Aktualisiert",
        "Keine neuen Daten",
        "Aktualisierung ab %02d:%02d Uhr wieder möglich",
        "Kein WLAN",
        "Abruf läuft",
        "Bitte kurz warten",

        // Refresh-Rueckmeldung, Kurzfassungen fuer schmale Kacheln
        "Lade...",
        "Fertig",
        "Gleich",
        "WLAN?",
        "Läuft...",
        "Warten...",
        "Ab %02d:%02d",
        "Nr. fehlt",

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
        "Dateien",

        // Dateien
        "Dateien",
        "Keine SD-Karte",
        "Keine Dateien",
        "%s löschen?",
        "Löschen fehlgeschlagen",
        "Geschützt",

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
        "Alle Einstellungen, Netzwerke und Zugangsdaten werden gelöscht. Die gezählten Ertragswerte (Monat, Jahr, Vorjahreswerte, Ertragsprotokoll) im Ordner SolarHistory bleiben erhalten. Das Gerät startet danach neu mit der Ersteinrichtung.",
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
        "Zeigt den Ertrag der letzten 7 Tage, Monate oder Jahre - oben die Reiter antippen, um umzuschalten. Die laufende Periode zählt das Gerät selbst, frühere Perioden sind gespeichert.",
        "Zeigt Ladezustand, Spannung, Strom, Temperatur und Ladestatus, aktualisiert alle 5 Minuten. Kein Laden: nicht geladen. Erhaltung: fast voll. Schnellladen: volle Leistung. Ausgleich: kurz höhere Spannung, gleicht die Zellen aus, normal.",
        "Das Menü mit allen Einstellungen.",
        "Bis zu 3 WLAN-Netzwerke speichern und verwalten.",
        "Netzwerk suchen und verbinden.",
        "E-Mail und Passwort für das Solar Guardian Portal eingeben.",
        "Zugangsdaten ändern oder entfernen, Geräte-Seriennummer antippen zum Ändern.",
        "Wird normalerweise automatisch vom Portal geholt. Hier nur bei Problemen oder mehreren Geräten von Hand eingeben oder ändern.",
        "Farbe für Schrift und Symbole auswählen.",
        "Helligkeit der Hintergrundbeleuchtung einstellen - mit dem Finger auf dem Regler ziehen. Mindestens 5%, damit das Display nie komplett dunkel wird.",
        "Zeigt die Adresse der Web-Ansicht. Diese Adresse im Browser eines Geräts im selben WLAN öffnen, um alle Werte groß auf Handy oder PC zu sehen - nur Anzeige, keine Einstellungen.",
        "Sprache der App auswählen.",
        "Alle Einstellungen löschen und neu einrichten.",
        "Hier kannst du Monat und Jahr korrigieren. Der Wert gilt einschließlich heute.",
        "Der Standort wird normalerweise automatisch über die IP-Adresse ermittelt. Hier kannst du ihn stattdessen von Hand eingeben (Ort oder Postleitzahl, aus den Treffern auswählen) oder die automatische Ermittlung erneut anstoßen.",
        "Zeigt die Dateien auf der SD-Karte. Ordner antippen öffnet ihn, Datei antippen fragt vor dem Löschen nach. Der Ordner SolarHistory ist geschützt und kann hier nicht geändert werden.",

        // Startlogo
        "wacht über deine Anlage",

        // Seite "Jahre": Reiter
        "Tage",
        "Monate",
        "Jahre",

        // Monatskuerzel
        "Jan", "Feb", "Mär", "Apr", "Mai", "Jun",
        "Jul", "Aug", "Sep", "Okt", "Nov", "Dez",

        // Seite "Tagesverlauf"
        "Tagesverlauf",
        "Zeigt die Leistungskurve des ganzen Tages von 05:00 bis 22:00 Uhr sowie den heutigen Höchstwert. Die Kurve bleibt abends stehen, bis um 05:00 Uhr der nächste Tag beginnt.",
        "Max:",
        "Noch keine Werte für heute",

        // Seite "PV-Status"
        "PV-Status",
        "PV-Spannung",
        "PV-Strom",
        "PV-Leistung",
        "Max heute",
        "Zeigt Spannung, Strom und Leistung der Solarmodule sowie den heutigen Leistungs-Höchstwert (derselbe Wert wie im Tagesverlauf). Die Werte kommen vom Solarregler über das Solar-Guardian-Portal und aktualisieren sich alle 5 Minuten. Kacheln antippen aktualisiert sofort. Nachts ist 0 W normal.",
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
        "The serial number is fetched from the portal automatically after login and normally doesn't need to be entered. Only enter it here if there's a problem or more than one device - you can find it in the Solar Guardian portal on the device page, in the address bar after devid=.",
        "Tap to change",
        "Save",
        "For a different installation, please restart the yield log under Factory reset.",
        "Connecting to the portal …",

        // Main screen / pages
        "Overview",
        "Power",
        "Yield",
        "Years",
        "Battery",
        "coming soon",
        "No account set up",
        "Serial number missing. Please enter it under Menu, Account.",

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
        "Portal data %d min old",
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
        "Not charging.",
        "Nearly full.",
        "Charging fully.",
        "Equalizing.",
        "Charging weakly.",
        "Next update: %s",
        "Max",
        "Min",
        "Refreshing...",
        "Updated",
        "No new data",
        "Refresh available again from %02d:%02d",
        "No WiFi",
        "Fetch in progress",
        "Please wait a moment",

        // Refresh feedback, short versions for narrow tiles
        "Loading...",
        "Done",
        "Same",
        "No WiFi",
        "Busy...",
        "Wait...",
        "From %02d:%02d",
        "No S/N",

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
        "Files",

        // Files
        "Files",
        "No SD card",
        "No files",
        "Delete %s?",
        "Delete failed",
        "Protected",

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
        "All settings, networks and account details will be deleted. Your counted yield values (month, year, previous years, the yield log) in the SolarHistory folder are kept. The device will then restart into first-time setup.",
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
        "Shows the yield of the last 7 days, months, or years - tap the tabs at the top to switch. The device counts the current period itself, earlier periods are stored.",
        "Shows charge level, voltage, current, temperature, and charging status, refreshed every 5 minutes. Not charging: no charge now. Float: almost full. Boost: full power. Equalize: brief higher voltage that balances the cells, normal.",
        "The menu with all settings.",
        "Save and manage up to 3 WiFi networks.",
        "Find a network and connect to it.",
        "Enter your email and password for the Solar Guardian portal.",
        "Change or remove your account details, tap the serial number to change it.",
        "Normally fetched from the portal automatically. Only enter or change it here if there's a problem or more than one device.",
        "Choose the color used for text and icons.",
        "Set the backlight brightness - drag your finger on the slider. At least 5% so the display never goes completely dark.",
        "Shows the address of the web view. Open this address in a browser on a device in the same WiFi network to see all values large on your phone or PC - view only, no settings.",
        "Choose the app language.",
        "Delete all settings and set up again.",
        "Here you can correct month and year. The value you enter counts including today.",
        "The location is normally detected automatically from your IP address. Here you can enter it by hand instead (place or postal code, pick one of the results) or trigger automatic detection again.",
        "Shows the files on the SD card. Tap a folder to open it, tap a file to be asked before deleting it. The SolarHistory folder is protected and can't be changed here.",

        // Startlogo
        "watching over your system",

        // "Years" page: tabs
        "Days",
        "Months",
        "Years",

        // Month abbreviations
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",

        // "Daily curve" page
        "Daily curve",
        "Shows the power curve of the whole day from 05:00 to 22:00, plus today's peak value. The curve stays visible in the evening until the next day starts at 05:00.",
        "Max:",
        "No data yet for today",

        // "PV status" page
        "PV status",
        "PV voltage",
        "PV current",
        "PV power",
        "Max today",
        "Shows voltage, current, and power of the solar panels, plus today's power peak (the same value as on the Daily curve page). The values come from the solar controller via the Solar Guardian portal and refresh every 5 minutes. Tap a tile to refresh immediately. 0 W at night is normal.",
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
