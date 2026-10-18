# Solar Guardian CYD

A solar dashboard for the Solar Guardian (EPEVER) cloud, running on an
ESP32 "Cheap Yellow Display" (CYD, ESP32-2432S028, 2.8" touch TFT).

![platform](https://img.shields.io/badge/platform-ESP32--2432S028-yellow)
![framework](https://img.shields.io/badge/framework-Arduino%20%2F%20PlatformIO-blue)
![license](https://img.shields.io/badge/license-MIT-green)
![version](https://img.shields.io/badge/version-1.1-blue)

![Solar Guardian CYD - Yield screen](docs/images/main.jpg)

[![Photos](https://img.shields.io/badge/📷_Photos-View_Gallery-blue?style=for-the-badge)](docs/gallery.md)

[🇩🇪 Deutsch](#deutsch) | [🇬🇧 English](#english)

---

## Deutsch

Ein Solar-Dashboard für die Solar-Guardian-Cloud (EPEVER) auf einem ESP32
"Cheap Yellow Display" (CYD, ESP32-2432S028, 2,8" Touch-TFT).

### Funktionen

- **Wisch-Seiten**: Leistung (aktuelle PV-Leistung groß, PV-Spannung/
  -Strom als Kacheln, Kurve der letzten 3 Stunden), Ertrag (Heute, Woche
  als gleitendes 7-Tage-Fenster, Monat, Jahr), Jahre (Balkengrafik der
  letzten Kalenderjahre) und Batterie (Ladezustand, Spannung, Strom,
  Temperatur, Ladestatus mit kurzer Erklärung der vier Zustände).
- **Ertragsbuch**: zählt Woche/Monat/Jahr selbst (die Portal-Summen
  haben bekanntermaßen Lücken), mit täglichem Backup und manueller
  Korrekturmöglichkeit.
- **Menü**: Konto, Netzwerke (bis zu 3 WLANs), Systemfarbe, Sprache
  (Deutsch/Englisch), Ertragswerte (Monat/Jahr manuell korrigieren),
  Zurücksetzen.
- **Ersteinrichtung** direkt am Gerät: Touch-Kalibrierung, WLAN-Auswahl,
  Solar-Guardian-Zugangsdaten.

### Installation

1. SD-Karte (FAT32-formatiert) ins Gerät einlegen - wird für die
   Konfiguration und das Ertragsbuch benötigt.
2. Firmware über den [Web-Flasher](https://eiswolf-bg.github.io/Solar-Guardian-CYD/)
   installieren (Chrome, Edge oder Opera, USB-Kabel).
3. Beim ersten Start: Touch kalibrieren, WLAN auswählen, Solar-Guardian-
   E-Mail und Passwort eingeben.

**Treiber-Hinweis:** Erkennt der Computer das Gerät nicht (kein USB/COM-
Port sichtbar), wird meist ein Treiber für den CH340-USB-Chip benötigt:
[Windows](https://www.wch-ic.com/downloads/CH341SER_EXE.html) ·
[macOS](https://www.wch-ic.com/downloads/CH341SER_MAC_ZIP.html) · unter
Linux meist bereits im Kernel enthalten.

### Hinweis

Dies ist eine inoffizielle Schnittstelle zur Solar-Guardian-Cloud (EPEVER)
- es besteht keine Verbindung zu oder Zusammenarbeit mit EPEVER, und die
Schnittstelle kann sich jederzeit ändern. Die Zugangsdaten liegen im
Klartext auf der SD-Karte (`/SolarGuardian/config.json`), wie es für ein
privates Gerät ohne weitere Absicherung üblich ist.

### Anpassen

Die Geräte-Seriennummer (`device_no`) steht in
`/SolarGuardian/config.json` auf der SD-Karte. Der in der Firmware
eingebaute Standardwert gehört zu einer bestimmten Anlage und muss für
jede andere Anlage auf die eigene Seriennummer geändert werden - sonst
meldet die Batterie-Abfrage einen Berechtigungsfehler. Die Seriennummer
findet man im Solar-Guardian-Portal auf der Geräteseite; die Adresszeile
dort enthält `devid=` gefolgt von der Seriennummer. Die Startwerte für
die Seite "Jahre" (Vorjahresertrag) stehen in `years.json` auf der SD-
Karte und können dort direkt angepasst werden.

### Galerie

Weitere Fotos: [docs/gallery.md](docs/gallery.md)

### Lizenz

MIT, siehe [LICENSE](LICENSE).

### Selbst bauen

Projekt mit [PlatformIO](https://platformio.org/) öffnen
(`platform = espressif32`, `board = esp32dev`, `framework = arduino`) und
bauen/flashen.

---

## English

A solar dashboard for the Solar Guardian (EPEVER) cloud, running on an
ESP32 "Cheap Yellow Display" (CYD, ESP32-2432S028, 2.8" touch TFT).

### Features

- **Swipe pages**: Power (current PV power front and center, PV voltage/
  current as tiles, a chart of the last 3 hours), Yield (Today, Week as a
  rolling 7-day window, Month, Year), Years (bar chart of the last
  calendar years), and Battery (charge level, voltage, current,
  temperature, charging status with a short explanation of all four
  states).
- **Yield log**: counts week/month/year itself (the portal's own totals
  are known to have gaps), with a daily backup and manual correction.
- **Menu**: Account, Networks (up to 3 WiFi networks), Theme color,
  Language (German/English), Yield values (manually correct month/year),
  Factory reset.
- **First-time setup** directly on the device: touch calibration, WiFi
  selection, Solar Guardian account details.

### Installation

1. Insert a FAT32-formatted SD card into the device - needed for
   configuration and the yield log.
2. Install the firmware via the [Web Flasher](https://eiswolf-bg.github.io/Solar-Guardian-CYD/)
   (Chrome, Edge, or Opera, USB cable).
3. On first boot: calibrate touch, select WiFi, enter your Solar Guardian
   email and password.

**Driver note:** If your computer doesn't detect the device (no USB/COM
port shows up), you likely need a driver for the board's CH340 USB chip:
[Windows](https://www.wch-ic.com/downloads/CH341SER_EXE.html) ·
[macOS](https://www.wch-ic.com/downloads/CH341SER_MAC_ZIP.html) · usually
built into the kernel on Linux already.

### Note

This is an unofficial interface to the Solar Guardian (EPEVER) cloud -
there is no affiliation with or endorsement by EPEVER, and the interface
may change at any time. Account details are stored in plain text on the
SD card (`/SolarGuardian/config.json`), as is typical for a private
device without further hardening.

### Customizing

The device serial number (`device_no`) lives in
`/SolarGuardian/config.json` on the SD card. The default baked into the
firmware belongs to one specific installation and must be changed to
your own serial number for any other installation - otherwise the
battery query fails with a permission error. You can find the serial
number on the device page in the Solar Guardian portal; its address bar
contains `devid=` followed by the serial number. The seed values for the
"Years" page (previous years' yield) live in `years.json` on the SD card
and can be edited there directly.

### Gallery

More photos: [docs/gallery.md](docs/gallery.md)

### License

MIT, see [LICENSE](LICENSE).

### Build it yourself

Open the project with [PlatformIO](https://platformio.org/)
(`platform = espressif32`, `board = esp32dev`, `framework = arduino`) and
build/flash.
