# Solar Guardian CYD — Projektkontext für Claude Code

Lies diese Datei zuerst, bevor du am Code etwas änderst.

## Was ist das Projekt?

Ein privates Solar-Dashboard für das Portal "Solar Guardian" (EPEVER-Cloud)
auf einem ESP32 "Cheap Yellow Display" (CYD, ESP32-2432S028), 240x320
Touch-TFT, Hochformat. Rein privates Gerät, **nicht für GitHub oder sonst
irgendeine Veröffentlichung gedacht**.

## Wer nutzt das?

Alex — Anfänger bei Arduino/ESP32. Bitte verständlich und knapp erklären,
was gebaut wurde, nicht von Fachbegriffen ausgehen, die als bekannt
vorausgesetzt werden.

## Arbeitsweise

- Wir bauen ein Feature nach dem anderen und warten nach jedem Feature auf
  Alex' OK, bevor das nächste angefangen wird.
- Bei Unklarheiten nachfragen statt zu raten.
- Flashen auf dem Testgerät ist nach erfolgreichem Build und bestandenen
  nativen Tests dauerhaft erlaubt, ohne vorher zu fragen - nur melden, was
  und wann geflasht wurde. Eine zeitliche Flash-Sperre gilt ausschließlich,
  wenn Alex sie in der jeweiligen Nachricht ausdrücklich für einen
  genannten Zeitraum verlangt, und endet automatisch mit diesem Zeitraum -
  nicht als Dauerregel merken. Commit, Tag und Push weiterhin nur auf
  ausdrücklichen Auftrag (gilt nicht für Releases).

## Pflichtregeln

- **Nur bauen und flashen.** Niemals committen, taggen oder pushen, außer
  Alex fordert das in ihrer aktuellen Nachricht ausdrücklich an. Seit
  V1.0 liegt das Projekt öffentlich auf GitHub
  (`Eiswolf-BG/Solar-Guardian-CYD`, MIT-Lizenz, Web-Flasher via GitHub
  Pages) — das ändert nichts an dieser Regel, jeder Push/Tag/Release
  bleibt an eine ausdrückliche Aufforderung in der jeweiligen Nachricht
  gebunden.
- **Release-Notes sind IMMER zweisprachig**, erst Deutsch, dann
  Englisch (Alex' fester Wunsch für dieses Repo: ein Nutzer des Geräts
  spricht nur wenig Englisch) — gilt für jedes GitHub-Release, ohne
  Ausnahme, auch bei kleinen Patch-Versionen.
- **README, Release-Notes und Commit-Nachrichten nennen ausschließlich
  Änderungen, die ein Nutzer am Gerät sieht oder merkt.** Interne
  Änderungen, Aufräumarbeiten und alles, was mit Zugangs- oder
  Gerätedaten, Sicherheit oder Datenschutz zu tun hat, werden dort NIE
  erwähnt. Für solche Änderungen bleibt die Commit-Nachricht neutral
  (z.B. "Interne Aufräumarbeiten" oder "Verbesserungen").
- **Keine Geheimnisse im Log.** E-Mail-Adresse, Passwort, WLAN-Passwort,
  MD5-Hash des Passworts, Portal-Token und Meldungstexte aus
  Login-Antworten (die enthalten die E-Mail) werden NIE über den
  seriellen Monitor ausgegeben, auch nicht gekürzt — nicht einmal
  teilweise. Erlaubt sind Statuscodes (HTTP-Code, `status`-Nummer der
  API), Erfolg/Fehlschlag-Meldungen ohne Klartext-Inhalt, die
  Energie-/Messwerte selbst, Tages-/Monats-/Jahreswechsel-Meldungen,
  Heap/Stack-Diagnosewerte, Migrations-Meldungen sowie History-Backup-
  geschrieben/-wiederhergestellt-Meldungen (jeweils nur Datum + kWh).
  Fehlertexte eines Endpunkts,
  die nachweislich keine Kontodaten enthalten (z.B. das `info`-Feld der
  Port-7002-Antwort, eine reine API-Fehlermeldung zur Anfrage selbst),
  dürfen ausnahmsweise mitgeloggt werden, wenn es zur Fehlersuche nötig
  ist — im Zweifel immer die vorsichtigere Variante (nur Statuscode)
  wählen.
- **Layout-Regel: keine Überschneidungen, nie über den Bildschirmrand.**
  Gilt für JEDEN Screen, in BEIDEN Sprachen, auch für jeden künftig neu
  hinzukommenden Screen:
  - `layout_utils.h/.cpp` (`LayoutUtils::fitText()`): wählt die größte
    Schriftgröße aus einer übergebenen Liste, die in eine verfügbare
    Breite/Höhe passt (mind. 4px Innenabstand). Passt selbst die
    kleinste Größe nicht, wird mit "…" gekürzt. Für JEDEN Wert/Text in
    Kacheln, Buttons und Titeln verwenden, der theoretisch lang werden
    kann (SSIDs, E-Mail-Adressen, übersetzte Texte) — nie ein fester
    `drawString()`/`print()` ohne Breitenprüfung.
  - **Ausnahme von der "…"-Kürzung: Zahlen und Jahreszahlen werden NIE
    gekürzt** (Alex' Wunsch, nach einem Bug auf der Seite "Jahre", wo
    Werte wie "4452" zu "4452…" wurden, weil die Höhen-Zone für die
    Schrift zu knapp bemessen war — bei dieser Schriftart
    (`AppFont11pt`, `yAdvance`=17) braucht selbst Size 1 mind.
    17+2×4=25px Zonenhöhe, sonst scheitert `fitText()` an der Höhe,
    nicht an der Breite, und kürzt trotz ausreichender Breite). Für
    Zahlenwerte/Jahreszahlen statt `fitText()` eine eigene,
    kürzungsfreie Variante verwenden, die bei fehlendem Platz nur die
    Schrift bis zur kleinsten Größe verkleinert, aber nie Ziffern
    abschneidet (siehe `years_page.cpp::fitNumberNoTruncate()` als
    Vorbild für künftige numerische Anzeigen).
  - `layout_check.h/.cpp` (`LayoutCheck`, nur Debug, über die Konstante
    `ENABLED` abschaltbar): Screens melden ihre gezeichneten Rechtecke
    (`LayoutCheck::begin()`/`rect()`/`finish()`), der Prüfer testet
    paarweise auf Überlappung und Bildschirmrand-Überschreitung und
    loggt Konflikte über den seriellen Monitor. **Begin/Finish laufen
    zentral in `main_screen.cpp::drawBody()`**, nicht mehr einzeln in
    jeder Seite — nur so melden sich die von `main_screen.cpp` selbst
    gezeichneten, festen Elemente (Seitenpunkte, "?"-Knopf) beim Prüfer
    an. Das war der Grund, warum ein tatsächlicher Konflikt (Seite
    "Jahre": Balkendiagramm reichte bis in die Fusszone, "?"-Knopf
    überlappte das Jahreszahl-Label) nicht erkannt wurde — jede Seite
    rief `begin()`/`finish()` selbst auf, bevor die Fusszone überhaupt
    gezeichnet war. Jede Seite registriert über `LayoutCheck::rect()`
    nur noch ihre eigenen Elemente, NICHT mehr `begin()`/`finish()`
    selbst. Für JEDE Wisch-Seite gilt zusätzlich: eigener Inhalt muss
    mind. 8px oberhalb der festen Fusszone enden
    (`Config::PAGE_FOOTER_RESERVED_H`, muss synchron mit `main_screen.
    cpp`s `HELP_BTN`/`DOTS_Y` bleiben).
  - Vor jedem Flash wird jeder neue/geänderte Screen rechnerisch mit
    Worst-Case-Texten geprüft (längster deutscher/englischer Text,
    Zahlen bis 99999,99, längste Statustexte) — Schrift-/Button-Größe
    wird dafür selbstständig angepasst, nie etwas überlappen gelassen.

## Hardware-Eckdaten

- Board: ESP32-2432S028 ("Cheap Yellow Display", klassische Variante ohne
  Kapazitiv-Touch), 240x320 Touch-TFT (Hochformat), SD-Kartenslot, 4MB
  Flash (per `esptool flash_id` bestätigt).
- TFT: ILI9341-Treiber, Pins/Timing siehe `platformio.ini` `build_flags`
  (übernommen aus eiswolfs-flightradar-CYD, dort am echten Gerät
  bestätigt). `tft.setRotation(0)` für Hochformat (native Panel-
  Ausrichtung, Alex' Wunsch), `tft.invertDisplay(true)` nötig für korrekte
  Farben auf diesem Board.
- Touch: XPT2046 über eigenen SPI-Bus (VSPI), Pins in `config.h`
  (`TOUCH_*_PIN`). Der Touch-Chip ist an die PHYSISCHE Panel-Ausrichtung
  gebunden, nicht an `tft.setRotation()` — ob roher X- oder Y-Wert der
  Bildschirm-X-Achse entspricht, wird deshalb bei der Kalibrierung selbst
  ermittelt (4 Eck-Kreuze, TL/TR/BR/BL, je ca. 20px Rand), nicht fest
  angenommen. Siehe `calibration_screen.cpp` und `touch_input.cpp`.
- SD-Karte: eigener SPI-Bus (HSPI), Pins in `config.h` (`SD_SPI_*_PIN`).
- Schrift: `app_font.h` (`AppFont11pt`), ein aus eiswolfs-flightradar-CYD
  übernommener, selbst generierter Font (GFXFF/TFT_eSPI), der Umlaute
  (ä/ö/ü/ß) korrekt darstellt — bestätigt und im Code genutzt. Wird global
  per `tft.setFreeFont(&AppFont11pt)` aktiviert (siehe
  `display.cpp::begin()`), gilt danach für jeden Text.
- **Partitionierung: Single-App, kein OTA** (Alex' bewusste Entscheidung,
  OTA ist für dieses private Projekt nicht geplant). `platformio.ini`:
  `board_build.partitions = huge_app.csv` — ein großer App-Slot (3MB)
  statt der zwei kleineren OTA-Slots im Standard-Schema. NVS bleibt
  erhalten (falls die WiFi-Bibliothek es braucht), SPIFFS/LittleFS werden
  nicht genutzt — die gesamte Konfiguration liegt auf der SD-Karte.

## Dual-Core-Architektur

Feste Grundregel für das ganze Projekt, gilt für jedes neue Feature:

- **Core 1** (`loop()`, `main.cpp`/`main_screen.cpp`/alle Screens):
  TFT-Ausgabe, Touch, Wischen, Menüs, Tastatur, Seitenanzeiger, Uhr,
  WLAN-Balken, **jeder** Zugriff auf die SD-Karte (`config.json` lesen und
  schreiben).
- **Core 0** (eigener FreeRTOS-Task `"fetch"`, siehe `fetch_task.cpp`,
  `xTaskCreatePinnedToCore`, Stack 12288 Byte, Priorität 1): WLAN-
  Verbindung/Wiederverbindung mit den gespeicherten Netzwerken, NTP-Zeit,
  alle HTTPS-API-Abfragen (Login, Ertrag, später weitere). **Greift NIE
  auf TFT oder SD-Karte zu.** Läuft nie busy-wait (`vTaskDelay` zwischen
  jedem Durchlauf), schaltet den Watchdog nie ab.
- **Gemeinsame Daten**: `SolarData` (siehe `solar_data.h/.cpp`) — vom
  Fetch-Task geschrieben, von Core 1 gelesen, geschützt durch einen
  Mutex. Core 1 holt sich mit `SolarData::snapshot()` eine Momentaufnahme
  und zeichnet danach OHNE Mutex weiter. Ein `changeCounter` steigt bei
  jeder Aktualisierung, damit Core 1 nur bei echter Änderung neu
  zeichnet. **Falle (bereits aufgetreten, siehe Git-Historie "Flackern"):**
  der Setter hinter `changeCounter` erhöht ihn IMMER, auch wenn der neu
  geschriebene Wert mit dem alten identisch ist - ein Aufrufer, der einen
  Wert unbedingt bei JEDEM Schleifendurchlauf setzt (z.B.
  `fetch_task.cpp`s nächster Abfragezeitpunkt, alle 200ms), erzeugt damit
  eine SPURIOUS Änderungsmeldung an alle lesenden Seiten, auch wenn sich
  visuell nichts geändert hat - sichtbar als Flackern aller Kacheln im
  Takt dieser Schleife. Fix: der Aufrufer selbst merkt sich den zuletzt
  tatsächlich gesendeten Wert und ruft den Setter nur auf, wenn er sich
  wirklich geändert hat (siehe `fetch_task.cpp`s `lastSentNextFetchAt`) -
  NICHT den Setter selbst ändern (der muss für andere, tatsächlich immer
  wertändernde Aufrufer unbedingt bleiben). Zusätzlich zeichnen die Seiten
  (`home_page.cpp`/`power_page.cpp`/`yield_page.cpp`/`battery_page.cpp`)
  dynamische Kachelinhalte nicht mehr per Vollbild-Löschen, sondern
  überzeichnen nur die betroffene Teilzone mit `fillRect(..., TFT_BLACK)`
  direkt vor dem neuen Text - verhindert zusätzliches Flackern selbst bei
  einer echten, aber für das Auge irrelevanten Änderung. `main_screen.cpp`
  zählt zur Diagnose Neuzeichnungen pro Seite und loggt sie einmal pro
  Minute (`[redraw] Neuzeichnungen/Minute: ...`).
- **Zugangsdaten an den Fetch-Task**: `AccountBridge` (siehe
  `account_bridge.h/.cpp`) — eigener kleiner Mutex, Core 1 schreibt dort
  die E-Mail/Passwort-Kopie (beim Start und nach jeder Kontoänderung über
  `account_screen.cpp`), der Fetch-Task liest sie beim eigenen Start und
  wenn `Command::AccountChanged` ankommt. **Core 0 greift dafür nie
  direkt auf `appConfig` zu.** Das Portal-Token lebt ausschließlich im
  RAM des Fetch-Tasks (task-lokale `String`), wird NIE auf die SD-Karte
  geschrieben, und wird bei jeder Kontoänderung verworfen (neu einloggen).
- **Wetter-Standort an den Fetch-Task**: `LocationBridge` (siehe
  `location_bridge.h/.cpp`) — gleiches Prinzip wie `AccountBridge`, Core 1
  schreibt dort Breite/Länge (beim Start und nach jeder Standortänderung
  über `location_screen.cpp`), der Fetch-Task liest beim eigenen Start
  und wenn `Command::LocationChanged` ankommt. `WeatherData` (siehe
  `weather_data.h/.cpp`) läuft umgekehrt wie `SolarData` (Fetch-Task
  schreibt, Core 1 liest per `snapshot()`) und ist unabhängig vom
  Solar-Guardian-Konto.
- **WLAN-Verbindungsstatus zurück an Core 1**: `WifiStatus` (siehe
  `wifi_status.h/.cpp`) — der Fetch-Task meldet dort, welcher gespeicherte
  Platz aktuell verbunden ist. Core 1 übernimmt daraus
  `appConfig.lastActiveWifiSlot` und speichert (SD-Zugriff bleibt
  exklusiv auf Core 1).
- **Befehle von Core 1 an den Fetch-Task**: eine FreeRTOS-Queue
  (`FetchTask::sendCommand()`, siehe `fetch_task.h`), nicht per direktem
  Funktionsaufruf — z.B. `ReconnectNow`, `DisconnectWifi`,
  `ConnectSlot0/1/2`, `AccountChanged`, `LanguageChanged`, `RefreshNow`
  ("jetzt aktualisieren"), `LocationChanged`.
- **WLAN-Scan/Verbindungstest in den Einrichtungs-Dialogen** (`wifi_select_
  screen.cpp`, `networks_screen.cpp`) laufen weiterhin blockierend auf
  Core 1 — der Fetch-Task pausiert währenddessen
  (`FetchTask::pause()`/`resume()`), damit nie zwei Seiten gleichzeitig
  aufs WLAN-Modul zugreifen.
- Der Hauptbildschirm wartet NIE auf das Netzwerk — er erscheint sofort,
  die Verbindung und alle Abfragen laufen im Hintergrund.

## Solar Guardian Portal (EPEVER-Cloud) — API

Alles POST mit JSON-Body über HTTPS (`WiFiClientSecure::setInsecure()` —
privates Gerät, keine Zertifikatsprüfung), neue Verbindung pro Abfrage,
`Connection: close`, Timeout 10s. Siehe `epever_client.h/.cpp`.

Gemeinsame Header: `Content-Type: application/json`,
`Appid: <Config::EPEVER_APPID>` (feste App-Kennung, kein Geheimnis).

- **Login** — `POST https://hncloudapi.epsolarpv.com/usrCloud/user/login`
  Body: `{"account":"<E-Mail>","password":"<MD5 des Klartext-Passworts,
  32 Zeichen Hex klein>","captchaVerification":"","isPhone":0}` (MD5 über
  `MD5Builder`). Antwort: `data.token` bei `status==0`. Token ~30 Tage
  gültig, nur im RAM.
- **Ertrag** — `POST https://hncloudapi.epsolarpv.com/usrCloud/vn/
  userView/getGeneratedEnergy`, Body: `{"loraFlag":0,"token":"<token>"}`.
  Antwort (kWh): `data.toDayGeneratingCapacity`, `data.monthGeneratedEnergy`,
  `data.yearGeneratedEnergy` (`data.generatingCapacitySum`/"Gesamt" wird
  NICHT mehr genutzt, siehe "Ertragsbuch" unten). Werte hinken dem
  Geräteportal einige Minuten hinterher (normal). Bei Fehler: einmal neu
  einloggen und wiederholen, erst danach Fehlerstatus setzen. Dient nur
  noch als einmaliger Startwert für Monat/Jahr bei der Erstinbetriebnahme
  — die laufenden Anzeigewerte kommen aus dem Ertragsbuch.
- **Batterie/Verlauf** — `POST https://hncloudapi.epsolarpv.com:7002/
  history/getSampleDataPoint` (eigener Port!), Body: `{"timeSort":"desc",
  "devDatapoints":[{"deviceNo":"<deviceNo>","dataPointId":<ID>}, ...],
  "start":<jetzt-3h in ms>,"end":<jetzt in ms>,"sampleLimit":5}`.
  Datenpunkt-IDs in `epever_datapoints.h`: Batteriespannung (101596357),
  -strom (101596376), Ladestatus (101596385), -temperatur (101596387),
  SOC (101596397), Tagesertrag "Daily Generation" (101596358, primäre
  Quelle fürs Ertragsbuch, aktueller als `getGeneratedEnergy`). Antwort:
  `data.list[]`, ein Eintrag pro `dataPointId`, darin `list[]` mit
  `time`(ms)/`value` — `list[0]` ist wegen `"desc"` der neueste Wert,
  `value` kann Zahl oder Text sein. `deviceNo` steht in `config.json`
  (Feld `device_no`, Version 4) und wird seit `serial_number_screen.h/
  .cpp` DIREKT AM GERÄT eingegeben — eigener Screen in der
  Ersteinrichtung (zwischen Konto und "Los geht's") und jederzeit im
  Menü unter "Konto", dort auf das Feld "Seriennummer" tippen (siehe
  "Bisher fertige Features" unten). Kein eingebauter Standardwert im
  Code (Auftrag "Seriennummer aus dem Projekt entfernen", entfernt
  `Config::DEFAULT_DEVICE_NO`) — `AppConfig::deviceNo` ist standardmäßig
  leer, `AppConfigStore::load()` übernimmt nur, was bereits in
  `config.json` steht. Ist ein Konto gespeichert, aber keine
  Seriennummer, öffnet `main.cpp` nach dem Startlogo einmalig
  `SerialNumberScreen::runFromAccount()`; ohne Seriennummer macht der
  Fetch-Task keine Solardaten-Anfragen (`FetchTask::hasDeviceNo`,
  `SolarData::FetchStatus::NoDeviceNo`, Text `NO_DEVICE_NO_CONFIGURED`)
  und die entsprechenden Seiten zeigen einen Hinweis statt Werten.
  Validierung (beide Screens,
  `serial_number_screen.cpp::isValidSerial()`): mind. 8 Zeichen, nur
  A-Z/0-9/"-" (Eingabe wird nach dem Tastatur-Schließen automatisch auf
  Großbuchstaben normalisiert). Ändern im Menü löst `Command::
  AccountChanged` aus; `fetch_task.cpp::applyAccountSnapshot()` erkennt
  eine GEÄNDERTE (nicht die erste) Seriennummer und ruft dafür
  `SolarData::resetForDeviceChange()` auf (verwirft Leistung/Batterie/
  PV/Verlauf/Ertrag, damit nie veraltete Zahlen der alten Anlage
  angezeigt werden) — das Ertragsbuch selbst bleibt dabei bewusst
  unangetastet (nur ein Hinweis vor dem Speichern, kein Zwang). Weder
  `main.cpp` noch die neuen Screens loggen den vollen Wert — nur
  `[core] device_no gesetzt: ja/nein`.
  **Authentifizierung (per Browser-Netzwerk-Tab bestätigt, löste den
  vorher dauerhaften `status 5004 "Insufficient permissions"`)**: KEIN
  `Appid`-Header, KEIN `"token"` im Body — der rohe JWT (ohne "Bearer")
  steht stattdessen im HTTP-Header `token` (siehe
  `epever_client.cpp::fetchBatteryData()`). Drei weitere vom Preflight
  gemeldete Header (`languagetype`, `sdk-version`, `usr-source`) sind als
  Konstanten in `epever_auth_headers.h` vorbereitet (Standard leer, ein
  leerer Wert wird nicht gesendet) — ihre tatsächlichen Werte sind noch
  offen und werden dort ergänzt, falls die Abfrage sie doch braucht. Bei
  401 oder jedem sonstigen Fehler wird einmal neu eingeloggt und die
  Abfrage wiederholt (`fetch_task.cpp::runBatteryFetch()`, gleiches
  Muster wie bei der Ertragsabfrage). Ladestatus-Zuordnung (vorläufig,
  noch nicht am Portal bestätigt): Rohwert 0-3 direkt (0 Kein Laden, 1
  Erhaltung, 2 Schnellladen, 3 Ausgleich — kurze Bezeichnungen seit der
  Status-Erklärung, siehe "Bisher fertige Features" unten), bei Rohwert
  >3 gilt `(wert >> 2) & 3` mit derselben Zuordnung. In derselben
  Anfrage laufen seit der Seite "Leistung" auch PV-Spannung (101596362)
  und PV-Strom (101596380) mit (kleines `sampleLimit`). Läuft im selben
  5-Minuten-Zyklus direkt nach der
  Ertragsabfrage, gleiche Wiederholungs-/Fehlerregeln.
- **Leistungskurve** — dieselbe URL/Authentifizierung wie Batterie/
  Verlauf, aber eigene Anfrage NUR für den Datenpunkt "PV Power"
  (101596388, W) mit großem `sampleLimit` (`EpeverClient::
  POWER_CURVE_MAX_POINTS` = 50, deckt die letzten 3 Stunden ab, siehe
  `epever_client.cpp::fetchPowerCurve()`). Antwort mit ArduinoJson-Filter
  geparst (nur `status`/`info`/`data.list[].list[].time`+`value`), da bei
  50 Punkten deutlich größer als die Batterieabfrage. `list[0]` (wegen
  `"desc"` der neueste Punkt) ist zugleich die aktuelle Leistung samt
  Messzeit; `fetch_task.cpp::runPowerCurveFetch()` sortiert beim
  Übernehmen nach `SolarData::setHistory()` auf ältester-zuerst um (die
  API liefert neuester-zuerst). 0 W (nachts, bei Bewölkung) ist ein
  gültiger Wert, kein Fehler. Läuft im selben 5-Minuten-Zyklus direkt
  nach der Batterieabfrage, gleiche Login-/Wiederholungsregeln,
  gleiches Token.

Zeitschema (siehe `Config::EPEVER_*` in `config.h` und `fetch_task.cpp`):
direkt nach erfolgreicher WLAN-Verbindung die erste Abfrage, danach alle
5 Minuten (`EPEVER_FETCH_INTERVAL_MS`). Bei Fehler Wiederholung nach 60s
(`EPEVER_RETRY_INTERVAL_MS`), nach 3 Fehlschlägen in Folge
(`EPEVER_MAX_SHORT_RETRIES`) zurück auf den 5-Minuten-Takt. "Jetzt
aktualisieren" (`Command::RefreshNow`) höchstens alle 30s
(`EPEVER_MANUAL_REFRESH_MIN_INTERVAL_MS`). Werte älter als 30 Minuten
(`EPEVER_STALE_AFTER_MS`) gelten als veraltet (Anzeige gedimmt + Hinweis),
werden aber NIE durch einen Fehler gelöscht — der letzte gültige Wert
bleibt stehen.

## Ertragsbuch (Woche/Monat/Jahr selbst zählen)

Die Portal-Summen für Monat/Jahr/Gesamt sind bekanntermaßen unzuverlässig
(Lücken, "Gesamt" ohne echten Bezug zum tatsächlichen Anlagenstart) — das
Gerät zählt Woche/Monat/Jahr deshalb selbst, siehe `yield_ledger.h/.cpp`.
**"Gesamt" gibt es bewusst nicht mehr** (Alex' Wunsch, nicht gebraucht) —
die Seite "Ertrag" zeigt Heute/Woche/Monat/Jahr.

- Läuft auf **Core 1** (benutzt die SD-Karte), wird aus `main.cpp::loop()`
  jede Iteration getickt — unabhängig davon, ob die Seite "Ertrag" gerade
  angezeigt wird. Liest den Tagesertrag-Rohwert aus `SolarData`
  (`dailySourceKwh`, vom Fetch-Task auf Core 0 befüllt: primär der
  Datenpunkt "Daily Generation", sonst `getGeneratedEnergy`s Tageswert).
- **Woche = gleitendes 7-Tage-Fenster** (heute + die 6 vorangehenden Tage),
  KEINE Kalenderwoche, kein Wochenschlüssel mehr (ersetzt die frühere
  ISO-Wochen-Logik). Fehlende Tage zählen als 0. Gespeichert in
  `letzteTage`/`last_days` (bis zu 6 Einträge `{datum, kwh}` für die
  zuletzt abgeschlossenen Tage), bei jedem Tageswechsel auf Einträge
  älter als 6 Tage geprüft/aussortiert.
- Monat/Jahr bleiben Kalendermonat/-jahr: `monat_basis`+`monat_key`
  ("YYYY-MM"), `jahr_basis`+`jahr_key` ("YYYY") — jeweils Summe der
  ABGESCHLOSSENEN Tage, ohne heute. Anzeige = Basis + `today_max`.
- Neuer Tageswert: `today_max = max(today_max, Wert)`. Werte <0 oder
  >200 kWh werden ignoriert, ebenso jeder Wert zwischen 00:00 und 04:00
  Uhr lokal (das Portal setzt den Tageswert um Mitternacht zurück und
  liefert kurz Altwerte) — "Heute" bleibt in diesem Fenster bei 0.
- Tageswechsel: zuerst `today_max` des alten Tages auf `monat_basis` UND
  `jahr_basis` addieren, als Zeile in `days.csv` protokollieren und in
  `letzteTage` aufnehmen (ältere als 6 Tage werden dabei entfernt);
  bei Jahreswechsel wird VORHER der abgeschlossene Jahreswert in
  `years.json` geschrieben (siehe unten), danach `jahr_basis` auf 0; bei
  Monatswechsel `monat_basis` auf 0. Mehrere Tage Lücke (Gerät war aus) →
  nur der gespeicherte Tag wird abgeschlossen, der Rest wird als "Lücke
  von N Tagen" geloggt, nicht rekonstruiert. Ohne gültige NTP-Zeit wird
  nichts gezählt.
- **Speicherung**: `yield.json` (atomar, temporäre Datei + Umbenennen,
  Schreiben bei Änderung höchstens alle 60s, sofort bei Tageswechsel/
  manueller Korrektur) und `days.csv` (nur Anhängen, eine Zeile
  "YYYY-MM-DD;kWh" pro abgeschlossenem Tag).
- **Format-Version der `yield.json`** (`yield_format_version` im JSON,
  getrennt von `config.json`s eigener Version): Version 1 hatte
  Monat/Jahr/**Gesamt**, keine Woche. Version 2: Gesamt entfernt, Woche als
  ISO-Kalenderwoche dazu. **Version 3 (aktuell)**: ISO-Wochenschlüssel
  wieder entfernt, Woche ist jetzt das gleitende 7-Tage-Fenster
  (`letzteTage`), Monat/Jahr unverändert. Migration v2→v3: `wocheBasis`/
  `wocheKey`/`gesamtBasis` werden verworfen, `letzteTage` startet leer
  (wird beim nächsten Start aus `days.csv` nachgefüllt, falls unvollständig).
  Fehlende/defekte `yield.json` startet die Zählung neu wie bei der
  Erstinbetriebnahme (Monat/Jahr aus der ersten `getGeneratedEnergy`-
  Antwort als Startwert, Woche ergibt sich automatisch aus den
  Tageswerten).
- **History-Backup-Ordner** (`/SolarHistory/tagesertrag.json`, siehe
  `Config::SD_HISTORY_*` in `config.h`): bei JEDER erfolgreichen
  Tagesertrag-Abfrage (alle 5 Minuten, über `SolarData::dayFetchSeq` als
  Änderungszähler erkannt) überschreibt Core 1 genau eine Datei mit
  `{"datum","zeit","kwh"}` — gespeichert wird `today_max` (der Tages-
  Höchstwert), NICHT der rohe Abfragewert, weil das Portal den Tageswert
  kurz vor Mitternacht auf 0 zurücksetzt; die Datei bekommt nie einen
  kleineren Wert fürs selbe Datum. Dient als Wiederherstellungs-Quelle:
  beim Start wird, falls das Datum der Backup-Datei dem heutigen Tag
  entspricht und ihr Wert größer als `today_max` ist, dieser Wert
  übernommen (geloggt als Datum+kWh); fehlt/ist defekt `yield.json`, wird
  `today_max` direkt aus der Backup-Datei gespeist. **Liegt bewusst
  AUSSERHALB von `/SolarGuardian`** (eigener Wurzelordner, früher
  `/SolarGuardian/History`) — "Zurücksetzen" löscht nur `/SolarGuardian`
  rekursiv (siehe `reset_screen.cpp`), `/SolarHistory` bleibt dadurch
  nach einem Reset erhalten und der heutige Tagesertrag stellt sich
  automatisch wieder her. Einmalige Migration beim Start
  (`sd_storage.cpp::migrateHistoryFolder()`): existiert noch der alte
  Ordner und der neue nicht, wird die eine Backup-Datei kopiert und der
  alte Ordner gelöscht.
- **Einmaliger historischer Ertragsimport** gab es früher über eine
  eigene, site-spezifische Datei mit fest eingebauten Tages-/
  Monatswerten für die Erstbefüllung eines bestimmten Geräts - seit dem
  Auftrag "Startwerte aus dem öffentlichen Code entfernen" vollständig
  entfernt (lief ohnehin nur einmal pro Gerät, bereits durchgeführte
  Importe stecken längst in den SD-Dateien). Eine lokale Kopie der
  entfernten Werte liegt ausserhalb des Repos, siehe Alex' eigene
  Notizen dazu.
- **Jahresübersicht** (`/SolarGuardian/years.json`, flache Map
  `{"YYYY": kwh}`): bei Jahreswechsel wird der abgeschlossene Jahreswert
  hier eingetragen. Fehlt die Datei, wird sie LEER angelegt (seit dem
  Auftrag "Startwerte aus dem öffentlichen Code entfernen" - vorher
  vorbefüllt mit fest eingebauten Vorjahreswerten). Seite "Jahre"
  (`years_page.h/.cpp`) zeigt eine
  Balkengrafik mit dem laufenden Jahr (live aus dem Ertragsbuch,
  Systemfarbe) plus bis zu 5 Vorjahren aus `years.json` (gedimmt) — bis
  zu 6 Balken insgesamt, Wert in ganzen kWh über, Jahreszahl unter jedem
  Balken (beides per `fitText()`).
- Manuelle Korrektur über Menü → "Ertragswerte" (`yield_values_screen.cpp`):
  nur noch "Monat setzen"/"Jahr setzen" ("Woche setzen" entfällt, die
  Woche ergibt sich automatisch), Eingabe gilt INKLUSIVE heute (neue
  Basis = Eingabe − `today_max`, nicht negativ).

## i18n (Deutsch/Englisch)

**Feste Regel: JEDER sichtbare App-Text bekommt einen Schlüssel in der
zentralen Textliste** (`i18n.h` enum `Str`, `i18n.cpp` zwei Tabellen DE/EN
in exakt derselben Reihenfolge wie das Enum, mit `static_assert` gegen
`Str::COUNT`) — niemals ein Text-Literal verstreut im jeweiligen Screen.
Gilt für Ersteinrichtung, Menüs, Fehlermeldungen, Warnungen, Statustexte,
Seitenüberschriften und Hilfetexte. Ausnahmen (bewusst NICHT in der
Textliste): der Produktname "Solar Guardian" (Eigenname), reine
Tastatur-Symbole (abc/ABC/123/OK/X), und die SD-Karten-Fehlt-Meldung in
`main.cpp` (bleibt fest zweizeilig DE+EN, weil es dort noch keine
geladene Spracheinstellung gibt).

`appConfig.language` (0=Deutsch, 1=Englisch, Standard Deutsch) — Menü →
"Sprache / Language" (dieser Menüpunkt-Name selbst bleibt immer
zweisprachig, damit er unabhängig von der aktuellen Einstellung
auffindbar ist). Auswahl gilt sofort, wird gespeichert, meldet
`Command::LanguageChanged` an den Fetch-Task (aktuell ohne Wirkung dort,
vorbereitet für spätere Features).

## Hilfe-Knöpfe ("?")

**Jeder Bildschirm bekommt einen "?"-Knopf** (siehe `help_popup.h/.cpp`),
außer Touchkalibrierung und Bildschirmtastatur. Tipp-Fläche mind. 32x32px
auf den Hauptseiten/Einrichtungsbildschirmen, im Stift-Menü darf er
kleiner sein (ca. 28px). Öffnet ein zentriertes Erklär-Fenster mit
echtem Pixel-Umbruch (2-4 kurze Zeilen) und OK-Knopf — funktioniert auch,
während der Fetch-Task beschäftigt ist (reine UI, keine Netzwerk-
Abhängigkeit). **Gilt auch für jeden neuen Screen, der künftig
dazukommt**, nicht nur für die aktuell existierenden.

## SD-Kartenstruktur

```
/SolarGuardian/
  config.json        - komplette App-Konfiguration (siehe app_config.h)
  config.json.tmp     - temporaere Datei beim Speichern (erst schreiben,
                        dann umbenennen - schuetzt vor Datenverlust bei
                        Stromausfall mitten im Schreiben)
  yield.json          - Ertragsbuch-Zustand (siehe "Ertragsbuch" oben),
                        ebenfalls atomar geschrieben
  yield.json.tmp       - temporaere Datei beim Speichern von yield.json
  days.csv            - Ertragsprotokoll, eine Zeile pro abgeschlossenem
                        Tag ("YYYY-MM-DD;kWh"), nur Anhaengen
  years.json          - Jahresuebersicht ("YYYY": kWh), siehe
                        "Ertragsbuch" oben, leer angelegt falls fehlend
  years.json.tmp       - temporaere Datei beim Speichern von years.json
  History/
    tagesertrag.json  - Backup des heutigen Tages-Hoechstwerts
                        (ueberschrieben bei jeder erfolgreichen Abfrage,
                        siehe "Ertragsbuch" oben)
    tagesertrag.json.tmp - temporaere Datei dafuer
```

`config.json` (Format-Version 4) enthält: Touch-Kalibrierung, bis zu 3
WLAN-Plätze (Name+Passwort je Platz) plus den zuletzt aktiven Platz,
Portal-E-Mail/-Passwort (Klartext — bewusst, privates Gerät; das Portal-
**Token** steht NIE hier, nur im RAM des Fetch-Tasks), Geräte-
Seriennummer `device_no` (für die Batterie/Verlauf-Abfrage, kein
Geheimnis), gewählte Systemfarbe, Sprache (`"de"`/`"en"`), Format-Version,
`setup_complete`-Flag. Ältere Versionen werden beim Laden automatisch
migriert (Version 1 → 2: einzelnes WLAN wird Platz 1; Version 2 → 3:
neues Feld `language`, Standard `"de"`; Version 3 → 4: neues Feld
`device_no` mit Standardwert), ohne dass die Ersteinrichtung neu laufen
muss — siehe `AppConfigStore::load()`. `yield.json` hat eine eigene,
unabhängige Format-Version (siehe "Ertragsbuch" oben).

## Design-Grundsätze

- Hintergrund immer schwarz.
- Systemfarbe für Schrift/Symbole zentral in `appConfig.systemColorIndex`
  gespeichert (`Display::systemColor()` benutzen, nie eine Farbe fest
  verdrahten) — Standard Weiß, änderbar über Menü → Systemfarbe.
- **Feste Farbregel (Auftrag "Farbregeln vereinheitlichen"/"Farbregel
  nachziehen"), gilt für JEDEN Screen (Gerät UND WebUI), auch künftige
  neue:** Die Systemfarbe (`Display::systemColor()`) ist für
  Überschriften, Beschriftungen, Einheiten (W/kWh/V/A/%/°C ...), Rahmen,
  Symbole und Reiter reserviert, ebenso für die Kurve bzw. die Balken
  eines Graphen. Die Sekundärfarbe (`Display::secondaryColor()`, aktuell
  `TFT_DARKGREY`, in der WebUI `--dim`) ist für ALLE vom Portal/
  Wetterdienst ABGEFRAGTEN WERTE - Zahlen UND Texte gleichermaßen
  (Leistung, Spannung, Strom, Ertrag, Batteriewerte, Temperatur,
  Tages-Höchstwert, aber auch ein textueller Ladestatus wie "Erhaltung"
  oder ein roher Status-Text vom Portal) -, für Raster-Hilfslinien in
  Graphen sowie für Achsen-/Zeitachsenbeschriftungen und für Wochentags-/
  Monats-/Jahreszahlen-Beschriftungen unter Balken. `Display::
  valueColor(valid, accent)` liefert für einen abgefragten Wert immer die
  Sekundärfarbe (auch ein "--"-Platzhalter bei fehlenden Daten). NICHT
  betroffen: reine Beschriftungen, Menü-/Hilfetexte und App-eigene
  Hinweissätze (z.B. "Nächste Abfrage: HH:MM", "Keine Verbindung") - die
  beschreiben einen Zustand der App selbst, keinen abgefragten Wert, und
  bleiben systemfarben bzw. wie bisher.
- Touch-Ziele auf den fingerfreundlichen Hauptseiten (Hauptbildschirm,
  Netzwerke-Liste, Farbauswahl, Konto) mindestens ca. 40 Pixel groß. Das
  Menü ist bewusst für Stift-Bedienung ausgelegt und darf dichter sein
  (mind. ca. 28px, siehe `menu_screen.cpp`). Einzelne Tasten der
  Bildschirmtastatur sind aus Platzgründen ebenfalls kleiner (siehe
  `keypad.cpp`).
- Überschriften auf Menü-/Einstellungsbildschirmen werden zentriert
  dargestellt (`TC_DATUM`), nicht linksbündig.
- Kopfzeile: Uhr, "Solar Guardian" und WLAN-Symbol teilen sich eine
  gemeinsame Grundlinie (`HEADER_BASELINE_Y` in `main_screen.cpp`) — der
  Font ist grundlinien-verankert, Text wächst von dieser Y-Position aus
  nach oben. "Solar Guardian" sitzt 3px darunter (Alex' Wunsch nach
  Sichtkontrolle am Gerät).

## Schriftzustand (`tft.setTextSize()`/`setTextDatum()`) nie vom Aufrufer erben

**Echter, auf Fotos beobachteter Bug** (behoben, siehe Git-Historie): Nach
dem Hinzufügen der Übersicht (`home_page.cpp`, Kacheln nutzen Size 2 für
große Zahlen) blieb die Textgröße am Ende von `HomePage::draw()` auf 2
stehen. Die NÄCHSTE Zeichenfunktion — `main_screen.cpp::drawHeader()`,
die nie selbst `tft.setTextSize()` aufrief — übernahm diesen Zustand
ungeprüft, wodurch Uhr/Titel/WLAN-Symbol riesig wurden und sich
überlappten; dasselbe pflanzte sich ins Menü fort (`menu_screen.cpp` rief
ebenfalls nie `setTextSize()` auf). Das TFT_eSPI-Objekt ist global und
behält Size/Datum/Font über Funktions- und Screen-Grenzen hinweg bei -
anders als man es von anderen Grafik-APIs erwarten würde.

**Feste Regel, gilt für JEDE Zeichenfunktion (Kopfzeile, Menü, Seiten,
Popups, neue wie alte):**
- Am ANFANG explizit `tft.setTextSize(1)` (oder die tatsächlich benötigte
  Größe) setzen, nie auf den Zustand des Aufrufers verlassen — auch wenn
  es "bisher immer zufällig gepasst hat". `setTextDatum()` wird bereits
  überall unmittelbar vor jedem `drawString()` gesetzt und nach Gebrauch
  auf `TL_DATUM` zurückgesetzt; das reicht, SOLANGE die Größe stimmt.
- Ein Screen, der zwischendurch eine andere Größe braucht (Size 2 für
  große Zahlen etc.), setzt sie am ENDE der eigenen Zeichenfunktion
  wieder auf 1 zurück (siehe `home_page.cpp::draw()`, letzte Zeilen) -
  zusätzlich zur obigen Regel, nicht als Ersatz dafür.
- `help_popup.cpp::drawButton()` (das "?") setzt seit diesem Bugfix
  ebenfalls selbst `setTextSize(1)` - ein Screen, der vorher mit Size 2
  endet, ließ das "?" sonst überdimensioniert/ausgefranst wirken.
- Die Kopfzeile prüft seitdem selbst per `LayoutCheck` (eigenes
  `begin("Kopfzeile")`/`finish()`, unabhängig von der Seiten-Prüfung in
  `drawBody()`, da `drawHeader()` auch separat beim Uhr-Tick läuft), dass
  Uhr/Titel/WLAN-Symbol mit ihrer ECHTEN gemessenen Textbreite in ihre
  Bereiche passen und sich nicht überlappen.

## WebUI (`web_server.cpp`)

- **Feste Regel (Auftrag "WebUI erweitern"):** Die WebUI wird IMMER um
  alles erweitert, was auch das Gerät bekommt und solartechnisch relevant
  ist (neue Wert-Screens, Diagramme, Übersichten) - Standard, nicht
  optional, gilt auch für künftige neue Seiten/Features. Jede neue
  CYD-Wisch-Seite bekommt dafür IMMER einen eigenen Reiter in der WebUI,
  OHNE Ausnahme - auch wenn ihre Werte bereits in einem anderen Reiter
  stehen (z. B. "PV-Status": eigener Reiter direkt nach "Batterie", obwohl
  Leistung/Spannung/Strom auch im Reiter "Leistung" zu sehen sind). NICHT
  gespiegelt
  werden Einstellungen und Menüs (Konto, Netzwerke, Farbe, Sprache,
  Helligkeit, Zurücksetzen ...) - die WebUI bleibt rein lesend. **Eine
  Ausnahme (Auftrag "WebUI-Reiter Daten"):** der Reiter "Daten"
  (`web_files.h/.cpp`) ist ein Datei-Explorer für die SD-Karte und darf
  dort lesen, bearbeiten und löschen - außer in `/SolarHistory`: dort
  ist NUR Ansehen/Herunterladen erlaubt, Speichern/Löschen bleibt für
  JEDE Datei dort (auch temporäre) zentral gesperrt (`web_files.cpp::
  isWriteLocked()`, pfadnormalisiert, case-insensitiv - `handleList()`/
  `handleRead()`/`handleDownload()` rufen diese Prüfung bewusst NICHT
  auf). Diese eine Ausnahme gilt NUR für diesen Reiter - alle anderen
  Endpunkte/Seiten bleiben rein lesend wie bisher.
  **Download/Backup (Auftrag "Daten: Download/Backup"):** jede Datei
  hat einen Download-Button (`/api/files/download`, gestreamt, auch für
  `/SolarHistory`); Ordner lassen sich als ZIP herunterladen
  (`/api/files/listtree` liefert die rekursive Dateiliste, der BROWSER
  baut die ZIP-Datei selbst zusammen - Modus "gespeichert"/stored mit
  CRC32, keine externe Bibliothek, siehe `buildZip()`/`crc32()` im
  WebUI-JS). Der Button "Backup erstellen" oben im Reiter zippt
  `/SolarGuardian` UND `/SolarHistory` zusammen
  (`SolarBackup_JJJJ-MM-TT_HHMM.zip`, Zeit vom Browser) - enthält
  Zugangsdaten, deshalb der Hinweistext im Reiter. Dateien werden dabei
  strikt nacheinander geholt (kurze Pause dazwischen), nie parallel.
- Jede blockierende Vollbild-Schleife (Menü, Dateien, Standort,
  Netzwerke, Tastatur ...) muss `WebServer_::tick()` in ihrer
  Warteschleife aufrufen - sonst ist die WebUI nicht erreichbar, solange
  diese Seite offen ist (einmal als echter Fehler aufgetreten und
  systemweit behoben). Beim Anlegen einer neuen Vollbild-Schleife daran
  denken.
- Eigene, kompakte Endpunkte für neue Datenarten (`/api/day`,
  `/api/history` ...) statt alles in `/api/data` zu packen - hält die
  alle 5s abgerufene Haupt-Antwort klein, neue Endpunkte werden nur
  abgerufen, wenn der jeweilige Tab offen ist.
- Farbregel in der WebUI sinngemäß wie am Gerät (siehe
  "Design-Grundsätze"): Kurve/Balken in der Akzentfarbe, Raster/
  Achsentexte gedämpft (`--dim`), ALLE abgefragten Werte (Zahlen UND
  Texte, z.B. auch der Batterie-Ladestatus) in `--dim`, Einheiten/
  Beschriftungen in der Akzentfarbe.
- **Versionsanzeige (Auftrag "WebUI erweitern", Versions-/
  Verbindungszeile im Footer):** `Config::FW_VERSION` (`config.h`) ist die
  EINZIGE Stelle im Firmware-Code für die Versionsnummer, die WebUI zeigt
  sie direkt (`/api/data`-Feld `fwv`) - KEINE zweite, separat zu
  pflegende Versionsnummer im WebUI-Code selbst anlegen. **Muss bei jedem
  Release händisch mit der Versionsnummer im Web-Flasher-Manifest
  (`manifest.json` im Wurzelverzeichnis) übereinstimmen** - beide Stellen
  zusammen pflegen, nicht nur eine davon vergessen.

## Swipe-Struktur (Hauptbildschirm)

- Die Kopfzeile (Uhr links, WLAN-Balken rechts, "Solar Guardian"-Text
  mittig als Button zum Menü, ohne sichtbaren Rahmen) ist fest und wird
  nie durch einen Seitenwechsel neu gezeichnet.
- Darunter liegt der Seitenbereich - Wischen (links/rechts, siehe
  `swipe_input.cpp`, Prinzip übernommen aus eiswolfs-toolbox) wechselt
  zwischen den Seiten, im Rundlauf (nach der letzten kommt wieder die
  erste und umgekehrt). **Empfindlichkeit** (Auftrag "Wischgeste
  empfindlicher" - vorher musste man "gewaltig drücken"): drei
  unabhängige Ursachen gefunden und behoben.
  1. **Druckschwelle des Touch-Chips** (XPT2046): `touched()`/die X/Y-
     Rohmessung selbst waren an eine feste `Z_THRESHOLD=300` in der
     Bibliothek gekoppelt, ohne öffentliche Setter-API. Gelöst über
     einen lokal gepatchten Fork in `lib/XPT2046_Touchscreen/` (Z auf
     180 gesenkt, siehe dessen `README_PATCH.md`) - bewusst NICHT mehr
     über `lib_deps` in `platformio.ini` bezogen, damit der Patch bei
     einem `pio run` nicht durch das Original-Paket überschrieben wird.
  2. **Druck-Aussetzer beim Gleiten**: `TouchInput::wasTapped()` wertete
     bisher JEDE einzelne Abfrage ohne Touch sofort als Loslassen - ein
     kurzer Abfall unter die Druckschwelle mitten in einer Wisch-
     Bewegung (bei leichtem Druck häufig) brach die Geste ab und ließ
     die nächste Berührung wie einen komplett neuen, kurzen Tap
     aussehen. Jetzt toleriert, bis zu `Config::TOUCH_DROPOUT_TOLERANCE_MS`
     (80ms) ohne erneuten Touch, bevor wirklich "loslassen" gilt -
     merkt sich dafür die letzte bekannte Position. Wirkt projektweit
     (ALLE Screens nutzen `wasTapped()`), nicht nur beim Wischen.
  3. **Schwellenwerte selbst**: `swipe_input.cpp` unterschied bisher nur
     "ab 50px überwiegend waagerecht = Wisch, sonst Tap" - dazwischen
     (z.B. 30px schräg) wurde trotzdem ein Tap ausgelöst. Jetzt drei
     benannte Konstanten in `config.h`: `SWIPE_MIN_DISTANCE_PX` (22,
     vorher 50), `SWIPE_HORIZONTAL_RATIO` (2 - waagerecht muss
     mindestens doppelt so groß wie senkrecht sein), `TAP_MAX_DRIFT_PX`
     (8 - nur darunter gilt sicher "Tap"). Bewegungen DAZWISCHEN (zu
     weit für einen Tap, aber zu kurz/schräg für einen Wisch) werden
     jetzt verworfen (`SwipeInput::poll()` liefert `false`), statt
     versehentlich einen Knopf/eine Kachel auszulösen.
- Ein Punkte-Indikator unten im Seitenbereich zeigt die aktuelle Seite
  (Systemfarbe) unter den anderen (gedimmt).
- Seiten sind in `pages.cpp` als Liste mit `draw()`, optionalem
  `update()` (true = neu zeichnen, für Seiten mit echten Daten) und
  optionalem `onTap()` (eigene Tipp-Flächen im Seitenbereich, z.B. "jetzt
  aktualisieren") angelegt - eine neue Seite ergänzt man dort, siehe
  Kommentar in `pages.h`. Größere Seiten (wie "Ertrag") bekommen ihr
  eigenes `*_page.h/.cpp`-Modul.
- Kein Vollbild-Sprite - nur der Seitenbereich wird bei einer echten
  Änderung neu gezeichnet.

## Bisher fertige Features

(Diese Liste nach jedem fertigen Feature aktualisieren.)

1. **Ersteinrichtung** (`main.cpp`, `calibration_screen.cpp`,
   `wifi_select_screen.cpp`, `credentials_screen.cpp`): SD-Karten-Check,
   4-Punkt-Touch-Kalibrierung, WLAN-Scan+Auswahl+Verbindungstest,
   Solar-Guardian-Zugangsdaten, speichert alles gesammelt am Ende
   (`config.json`), holt NTP-Zeit (Europe/Berlin).
2. **Bildschirmtastatur** (`keypad.cpp`): deutsches QWERTZ-Layout,
   Buchstaben-/Zahlen-Symbol-Ebenen, @ und Punkt immer auf der festen
   Aktionsreihe erreichbar, Einmal-Shift (kein Capslock), Klartext.
3. **Hauptbildschirm mit Wisch-Seiten** (`main_screen.cpp`, `pages.cpp`,
   `swipe_input.cpp`): feste Kopfzeile, darunter Wisch-Seiten im Rundlauf
   ("Übersicht"/"Leistung"/"Tagesverlauf"/"Ertrag"/"Batterie"/"PV-Status"/
   "Verlauf", Reihenfolge zentral in `pages.cpp::PAGES[]`, "Übersicht" ist
   seit Punkt 21 unten die erste Seite nach dem Booten, "Verlauf"
   (Tage/Monate/Jahre, vormals "Jahre") ist seit Auftrag
   "Seitenreihenfolge am CYD ändern" bewusst die letzte Seite) mit
   Punkte-Indikator.
4. **Menü** (`menu_screen.cpp`, für Stift-Bedienung ausgelegt):
   "Netzwerke", "Konto", "Systemfarbe", "Sprache / Language",
   "Ertragswerte", "Zurücksetzen", "Zurück". Die Seriennummer ist seit
   dem Umzug ins Konto (siehe Punkt 20 unten) kein eigener Menüpunkt
   mehr, sondern ein antippbares Feld im Konto-Screen; der Standort
   (Punkt 21 unten) hat nie einen eigenen Menüpunkt bekommen - er wird
   automatisch ermittelt und nur über die Wetterkachel geändert.
5. **Netzwerke-Verwaltung** (`networks_screen.cpp`,
   `wifi_connect_manager.cpp`): bis zu 3 gespeicherte WLANs, freier Platz
   → Scan+Passwort+Verbindungstest vor dem Speichern, belegter Platz →
   Untermenü (Verbinden/Passwort ändern/Löschen/Zurück), aktuell
   verbundenes Netz mit Haken markiert. Verbindungsaufbau läuft komplett
   im Hintergrund (Fetch-Task), der Hauptbildschirm erscheint sofort.
6. **Konto** (`account_screen.cpp`): zeigt E-Mail + maskiertes Passwort,
   darunter das antippbare Seriennummer-Feld (siehe Punkt 20 unten),
   Benutzername/Passwort ändern, Konto löschen (nur Zugangsdaten, WLANs
   bleiben) → führt direkt in die Zugangsdaten-Ersteinrichtung.
7. **Zurücksetzen** (`reset_screen.cpp`): Werksreset mit Warn-Bildschirm,
   löscht `/SolarGuardian` rekursiv von der SD-Karte, startet danach neu
   in die Ersteinrichtung.
8. **Sprache** (`language_screen.cpp`, `i18n.h/.cpp`): Deutsch/Englisch,
   zentrale zweisprachige Textliste für die gesamte App.
9. **Hilfe-Knöpfe** (`help_popup.h/.cpp`): "?"-Knopf mit kurzem
   Erklär-Fenster auf jedem Bildschirm außer Kalibrierung/Tastatur.
10. **Dual-Core-Architektur** (`fetch_task.h/.cpp`, `solar_data.h/.cpp`,
    `wifi_status.h/.cpp`, `account_bridge.h/.cpp`): Fetch-Task auf Core 0
    für WLAN/NTP/API-Abfragen, Core 1 bleibt für Anzeige/Touch/SD.
11. **Single-App-Partitionierung** (`platformio.ini`): `huge_app.csv`,
    kein OTA, 3MB App-Slot statt zwei kleineren.
12. **Ertrag-Abfrage** (`epever_client.h/.cpp`, `yield_page.h/.cpp`,
    `yield_ledger.h/.cpp`): Login + Ertragsabfrage beim Solar Guardian
    Portal, 2x2-Kacheln (Heute/Woche/Monat/Jahr, selbst gezählt übers
    Ertragsbuch — "Gesamt" gibt es nicht) auf der Seite "Ertrag",
    Statuszeile mit "jetzt aktualisieren"-Tippfläche, veraltete Werte
    werden gedimmt statt gelöscht.
13. **Batterie-Abfrage** (`epever_client.h/.cpp`, `epever_auth_headers.h`,
    `battery_page.h/.cpp`, `epever_datapoints.h`): Port-7002-Abfrage
    (Spannung/Strom/SOC/Temperatur/Ladestatus + Tagesertrag-Datenpunkt)
    über den Header `token` (siehe "Solar Guardian Portal API" oben),
    Seite "Batterie" mit großer SOC-Anzeige und 2x2-Kacheln
    (Spannung/Strom/Temperatur/Status) — die ursprüngliche Balkengrafik
    unter der SOC-Zahl wurde beim Hinzufügen der Status-Erklärung (siehe
    Punkt 17) wieder entfernt, da der Platz für die neue Erklärungszeile
    gebraucht wurde.
14. **Menü "Ertragswerte"** (`yield_values_screen.h/.cpp`): zeigt
    Heute/Woche/Monat/Jahr UNTEREINANDER (4 Zeilen, z.B. "Heute 1,49
    kWh"), erlaubt manuelle Korrektur von Monat/Jahr (inklusive heute;
    Woche entfällt, ergibt sich automatisch aus dem gleitenden
    7-Tage-Fenster). War ursprünglich eine Zeile mit "/" getrennt und
    per `fitText()` gekürzt — verletzte die Nie-Kürzen-Regel bei großen
    Werten, deshalb auf vier volle Bildschirmbreiten-Zeilen umgestellt.
15. **Layout-Regel** (`layout_utils.h/.cpp`, `layout_check.h/.cpp`):
    `fitText()` für automatische Schriftgrößen-Anpassung/Kürzung,
    Debug-Layout-Prüfer für Überlappungen/Randüberschreitungen.
16. **Gleitendes 7-Tage-Fenster + History-Backup + Seite "Jahre"**
    (`yield_ledger.h/.cpp`, `years_page.h/.cpp`): Woche
    ist jetzt heute + die 6 Vortage (keine Kalenderwoche mehr),
    `/SolarGuardian/History/tagesertrag.json` sichert täglich den
    Tages-Höchstwert als Wiederherstellungs-Quelle, `/SolarGuardian/
    years.json` speichert abgeschlossene Jahreswerte und wird auf der
    neuen Seite "Jahre" als Balkengrafik angezeigt (laufendes Jahr in
    voller Systemfarbe, frühere Jahre auf etwa halbe Helligkeit gedimmt;
    Zahlen/Jahreszahlen werden nie gekürzt, siehe Layout-Regel oben).
17. **Status-Erklärung auf der Seite "Batterie"** (`battery_page.cpp`):
    kürzere Ladestatus-Bezeichnungen (Kein Laden/Erhaltung/Schnellladen/
    Ausgleich, Rohwert 0-3 unverändert), Status-Text wird nie mit "…"
    gekürzt (Schrift schrumpft, bei Bedarf bis zu 2 Zeilen, Kachel wird
    dafür höher) und eine dezente, bis zu 3-zeilige Erklärungszeile unter
    den vier Kacheln (nur bei bekanntem Status, sonst leer). Alle
    Y-Positionen danach (Erklärung, Zeitstempel) werden dynamisch aus der
    tatsächlichen Zeilenzahl berechnet, nicht aus festen Offsets (siehe
    `battery_page.cpp::computeLayout()`). `HelpPopup::show()` erlaubt
    seitdem bis zu 10 statt 6 Zeilen und setzt die Textgröße jetzt immer
    explizit auf 1 (vorher abhängig von der zuletzt vom Aufrufer
    gesetzten Größe).
18. **Seite "Leistung"** (`power_page.h/.cpp`, `epever_client.cpp::
    fetchPowerCurve()`): löst den früheren Platzhalter ab. Große,
    zentrierte aktuelle PV-Leistung (ganze Watt, ab 1000 W als "X,XX kW"
    mit Dezimalkomma/-punkt je Sprache) mit Messzeit darunter, zwei
    kleine Kacheln (PV-Spannung/-Strom, wiederverwendet die Batterie-
    Textschlüssel "Spannung"/"Strom"), darunter eine selbst mit
    `drawLine()` gezeichnete 3-Stunden-Kurve (Y-Achse ab 0, Maximum auf
    ein 1-2-5-Schema aufgerundet und beschriftet, Hilfslinien in
    gedimmter Systemfarbe, neuester Punkt als kleiner Kreis, Zeitmarken
    "-3h"/"-2h"/"-1h"/"jetzt" darunter) und eine Statuszeile wie auf der
    Seite "Ertrag" (Tipp darauf löst "jetzt aktualisieren" aus) - zeigt
    im Erfolgsfall "Nächste Abfrage: HH:MM Uhr" statt einer (gegenüber
    dem "Stand HH:MM" oben darüber redundanten) zweiten Messzeit; die
    Uhrzeit kommt aus `SolarData::nextFetchAt` (vom Fetch-Task nach jedem
    Zyklusversuch neu berechnet, siehe `fetch_task.cpp`), "--:--" falls
    kein Zeitplan bekannt ist (kein Konto/keine Verbindung). Gleiche
    Änderung auf der Seite "Batterie". Alle
    Offsets sind rechnerisch verkettet (nicht einzeln fest verdrahtet),
    siehe `power_page.cpp` Kommentar am Konstanten-Block — eine erste
    Fassung reichte mit der Statuszeilen-Tippfläche bis in die
    Seitenpunkte-/"?"-Zone hinein, was der (seit der Seite "Jahre")
    zentral in `main_screen.cpp` verankerte Layout-Prüfer direkt beim
    ersten Testlauf gemeldet hat.
19. **Ladestrom-Schwelle für "Schnellladen"/"Ausgleich"** (`battery_page.
    cpp::effectiveStatus()`): ein EPEVER Tracer-AN meldete diese beiden
    Zustände schon bei wenigen mA Ladestrom, optisch irreführend. Beide
    werden deshalb nur noch angezeigt, wenn `batteryCurrent > 1,0 A` ist;
    darunter (Rohwert meldet trotzdem Laden) erscheint neutral "Laden
    (schwach)" (`CHARGE_STATUS_WEAK`/`BATTERY_EXPLAIN_WEAK`). Bei 0 A
    immer "Kein Laden", unabhängig vom Rohwert. "Erhaltung" (Kategorie 1)
    ist von der Schwelle nicht betroffen. Der Rohwert-Ladestatus wird
    jetzt bei JEDER Abfrage geloggt (zuvor nur bei Änderung), zusammen
    mit dem Batteriestrom, siehe `fetch_task.cpp`. Die Erklärungszeile
    für "Ausgleich" wurde dabei bewusst abgeschwächt (kein Hinweis mehr
    auf "höhere Spannung", nur noch "Der Regler meldet
    Ausgleichsladung.").
    **Beim Verketten der Offsets in `computeLayout()` die 14px
    Aufwärts-Reichweite der Statuszeilen-Tippfläche (`timestampTapZone()`)
    nicht vergessen** — ein fehlendes `+14` im Abstand zwischen
    Erklärungszeile und Zeitstempel führte hier zu einem echten, vom
    Layout-Prüfer gemeldeten Übereinanderliegen (siehe Git-Historie/
    Bugfix), analog zum `SCREEN_HEIGHT`-vs-`areaTop+areaHeight`-Fehler bei
    der Fusszonen-Formel (siehe `Config::PAGE_FOOTER_RESERVED_H` oben).
20. **Seriennummer direkt am Gerät** (`serial_number_screen.h/.cpp`):
    eigener Ersteinrichtung-Screen zwischen Konto und "Los geht's" (mit
    "Zurück" zu Konto, ohne Datenverlust — `CredentialsScreen::run()`
    löscht die Eingabefelder seitdem nicht mehr selbst, das übernimmt nur
    noch `appConfig = AppConfig()` ganz am Anfang der Ersteinrichtung;
    `CredentialsScreen::run(false)` zeigt dabei "Weiter" statt "Los
    geht's" und speichert/holt noch keine NTP-Zeit — das erledigt erst
    `CredentialsScreen::finalizeSetup()`, nachdem auch die Seriennummer
    eingegeben wurde) sowie derselbe Screen zum späteren Ändern
    (`SerialNumberScreen::runFromAccount()`, seit dem Umzug "Seriennummer
    zu Konto" kein eigener Hauptmenüpunkt mehr: `account_screen.cpp`
    zeigt ein eigenes, antippbares Feld — Beschriftung oben, aktueller
    Wert darunter, mit kleinem "Zum Ändern antippen"-Hinweis direkt
    darüber — das diesen Screen öffnet und nach "Zurück"/Speichern wieder
    zum Konto-Screen zurückkehrt), mit Hinweis vor dem Speichern, dass
    bei einer anderen Anlage das Ertragsbuch über "Zurücksetzen" neu
    begonnen werden sollte (reine Info, kein Zwang — das Ertragsbuch wird
    beim Ändern NICHT automatisch gelöscht). Die ausführliche Erklärung
    unter dem Eingabefeld (`I18n::Str::SERIAL_NUMBER_HINT`) ist EIN
    gemeinsamer i18n-Text für Ersteinrichtung UND Konto-Screen (nicht
    doppelt formuliert) — in der Ersteinrichtung war sie schon vorhanden,
    im Konto-Screen stand darunter vorher nichts
    (`SerialNumberScreen::runFromAccount()` ruft seitdem denselben
    `drawHint()`-Helper auf wie `runFirstTimeSetup()`). Siehe oben
    ("Solar Guardian Portal API") für Validierung und das
    automatische Verwerfen alter Batterie-/Leistungs-/Ertragswerte beim
    Ändern.
21. **Hauptscreen "Übersicht" + Wetter + Standort** (`home_page.h/.cpp`,
    `weather_data.h/.cpp`, `weather_client.h/.cpp`, `weather_icons.h/.cpp`,
    `geocoding_client.h/.cpp`, `location_bridge.h/.cpp`,
    `location_screen.h/.cpp`, `week_view_screen.h/.cpp`, `page_nav.h/.cpp`):
    neue erste Wisch-Seite (`pages.cpp::PAGES[0]`, alle anderen Seiten um
    eins nach hinten verschoben) mit 2x2 gleich großen Kacheln (Wetter/
    Leistung/Heute/Batterie), gleichmäßiger Abstand, siehe
    `home_page.cpp::computeLayout()` — **wichtig**: wie jede andere Seite
    von `Config::SCREEN_HEIGHT - PAGE_FOOTER_RESERVED_H - 8` ausgehen,
    nicht von `areaTop+areaHeight` (gleicher Fehler wie bei
    `years_page.cpp`/`battery_page.cpp`, siehe `Config::
    PAGE_FOOTER_RESERVED_H` oben) — die untere Kachelreihe ragte sonst
    hinter den "?"-Knopf (vom Layout-Prüfer gemeldeter echter Konflikt,
    schon beim ersten Testlauf aufgetreten und behoben).
    - **Wetter** (Open-Meteo.com, kostenlos, ohne Zugangsschlüssel):
      Abruf läuft im Fetch-Task (Core 0, `fetch_task.cpp::
      runWeatherFetch()`), eigener 30-Minuten-Takt
      (`Config::WEATHER_FETCH_INTERVAL_MS`), sofort nach dem WLAN-Verbinden
      und nach einer Standortänderung, bei Fehler alle 2 Minuten erneut
      (`Config::WEATHER_RETRY_INTERVAL_MS`) bis zum nächsten Erfolg.
      Unabhängig vom Solar-Guardian-Konto (`hasLocation && wifiConnected`,
      nicht `hasAccount`). `WeatherData` (Fetch-Task → Core 1) und
      `LocationBridge` (Core 1 → Fetch-Task, Standort-Koordinaten) folgen
      demselben Mutex/Momentaufnahme-Muster wie `SolarData`/
      `AccountBridge`. Wettersymbole werden rein mit TFT_eSPI-Grundformen
      gezeichnet (`weather_icons.cpp`, keine Bilddateien) — Code-Gruppen
      laut Auftrag (0 klar, 1/2 leicht bewölkt, 3 bewölkt, 45/48 Nebel,
      51-57/61-67/80-82 Regen, 71-77/85/86 Schnee, 95-99 Gewitter), Tag/
      Nacht aus `is_day`. Bei Abruffehler bleiben die letzten Werte
      stehen (nur bei Erfolg schreibt `WeatherClient::fetch()` in
      `WeatherData`). Ortsname/Koordinaten werden NIE geloggt, nur
      HTTP-Code und Antwortgröße (`[fetch] Wetter: HTTP-Code=... Antwort
      größe=... Byte`). Symbole sind knallig-bunt mit hellem Rand (Sonne
      knallgelb/orange mit abwechselnd langen/kurzen Strahlen, Mond
      warmgelb mit 2-3 Sternen, Wolken reines Weiß mit nur ganz leichtem
      neutralem Schatten an der Unterseite - KEIN Blauton mehr, bewusst
      so geändert, nachdem die ursprüngliche hellblaue Wolkenfarbe sich
      laut Alex' Fotos nicht von den blauen Regentropfen/Schneeflocken
      abhob; bedeckt helles Neutralgrau, klar dunkler als die weiße
      Wolke; Regentropfen kräftiges Blau mit weißem Glanzpunkt;
      Schneeflocken kräftiges Hellblau/Cyan (bewusst NICHT mehr auch
      weiß, da eine weiße Flocke auf der jetzt weißen Wolke unsichtbar
      wäre); Gewitterblitz knallgelb mit orangem Rand auf dunkler Wolke;
      Nebel kräftiges Türkis/Hellblau - alle Farben zentral in
      `weather_icon_colors.h`). Sonne/Mond bekommen bei ausreichender
      Größe (`r >= 14px`, siehe `bigEnoughForFace` in
      `weather_icons.cpp::draw()`) ein kleines Gesicht (zwei Augen +
      lächelnder Mund) - in der kleinen Wochenansicht bewusst ohne
      Gesicht. Gezeichnet mit TFT_eSPI's geglätteten Funktionen
      (`fillSmoothCircle`/`fillSmoothRoundRect`/`drawWedgeLine`) statt
      treppiger Grundformen - **Falle**: der jeweilige `bg_color`-
      Parameter muss die Farbe sein, auf der die Form TATSÄCHLICH
      sitzt (z.B. die Outline-Farbe bei der inneren Füllung über dem
      Rand, der Sonnen-Körper bei den Augen des Gesichts), sonst
      entsteht ein sichtbarer dunkler Spalt an der Kante (bereits
      einmal so aufgetreten und behoben). **Kein Demo-Modus** (wurde
      kurz eingeführt, dann auf Alex' ausdrücklichen Wunsch wieder
      komplett entfernt, siehe Git-Historie - war nur zum einmaligen
      Durchsehen aller Symbole gedacht, kein dauerhaftes Feature): kein
      langer Druck auf die Wetterkachel, kein Menüpunkt, kein
      Namenstext unter dem Symbol - ein Tap auf die Wetterkachel öffnet
      IMMER direkt die Wochenansicht (bzw. den Standort-Screen ohne
      Standort), nichts anderes.
    - **Standort - automatisch, notfalls einmalig per Eingabe** (Alex'
      Entscheidung nach dem ersten Test: KEIN eigener Menüpunkt und KEIN
      Schritt in der Ersteinrichtung mehr, siehe Git-Historie):
      - **Automatische Ermittlung über die öffentliche IP-Adresse**
        (`ip_geolocation_client.h/.cpp`, `ip_location_data.h/.cpp`):
        läuft im Fetch-Task (Core 0), Hauptdienst `ipwho.is` (liefert
        Breite/Länge als echte JSON-Zahlen), bei Fehler Rückfall auf den
        unabhängigen zweiten Anbieter `get.geojs.io` (liefert sie als
        Text, deshalb `String::toFloat()` statt des `|`-Operators, siehe
        unten). Takt: sofort nach dem WLAN-Verbinden, danach einmal pro
        Tag (`Config::IP_LOCATION_DAILY_INTERVAL_MS`), sofort bei einem
        WLAN-Netzwechsel, bei Fehler alle 2 Minuten erneut - NIE, wenn
        `appConfig.locationManual` gesetzt ist. Das Ergebnis geht über
        `IpLocationData` (Fetch-Task → Core 1, gleiches Mutex/
        Momentaufnahme-Prinzip wie `WeatherData`) an `main_screen.cpp::
        tick()`, das es NUR bei `!appConfig.locationManual` in
        `appConfig` übernimmt, speichert und `FetchTask::Command::
        LocationChanged` sendet (SD-Zugriff bleibt exklusiv auf Core 1).
      - **Einmalige Eingabe per Antippen** (`location_screen.cpp`):
        Suche über Nominatim (OpenStreetMap, siehe unten) ODER Knopf
        "Automatisch ermitteln" (startet die IP-Ermittlung sofort,
        blockierend mit `FetchTask::pause()/resume()`, setzt
        `locationManual=false`). Eine Auswahl aus der Trefferliste setzt
        `locationManual=true` - danach überschreibt die automatische
        Ermittlung nichts mehr. Erreichbar NUR über die Wetterkachel
        (ohne Standort) bzw. die Wochenansicht ("Zum Ändern antippen"
        über dem Ortsnamen, mit Standort) - siehe `home_page.cpp::
        onTap()`/`week_view_screen.cpp`. Der "ae/oe/ue statt ä/ö/ü"-
        Hinweis erscheint einmalig beim ersten Öffnen der Tastatur pro
        Bildschirmbesuch (die Tastatur hat keine Umlaute, siehe
        `keypad.cpp`).
      - **Geocoding-Dienst: Nominatim statt Open-Meteo** (wichtiger
        Bugfix - "Keine Adresse gefunden" bei Postleitzahlen-Eingabe):
        ein Live-Test (`curl` gegen beide APIs mit einer echten PLZ)
        zeigte, dass Open-Meteos Geocoding-API den `name=`-Parameter rein
        als TEXT-Suche behandelt und bei einer deutschen PLZ (z.B.
        "10115") eine komplett falsche US-Stadt lieferte (deren
        `postcodes`-Liste die Ziffernfolge nur zufällig enthält) - keine
        echte Postleitzahlen-Unterstützung, unabhängig vom Client-Code.
        `geocoding_client.cpp` nutzt deshalb jetzt Nominatim
        (OpenStreetMap, kostenlos, verlangt aber einen eindeutigen
        `User-Agent`-Header, siehe `http_get_json.h`s neuer optionaler
        Parameter): bei 4-5-stelliger Zifferneingabe `postalcode=` mit
        Rückfallkette DE→AT→CH, sonst `q=<Suchtext>` - beides im Test
        zuverlässig für PLZ UND Orts-/Stadtnamen. Nominatim liefert
        `lat`/`lon` als TEXT, nicht als Zahl - `String::toFloat()` statt
        des `|`-Operators (der bei einer Zahl-als-Text sonst leise auf
        den Default zurückfällt). Fehler unterschieden: HTTP-/JSON-Fehler
        → "Keine Verbindung" (wiederverwendet `STATUS_NETWORK_ERROR`),
        leere Trefferliste → "Nichts gefunden". **Name-Aufbau** (Bugfix:
        eine PLZ mit nur einem Nominatim-Treffer lieferte oft einen
        Nebenort statt der eigentlichen Stadt, plus den langen,
        unübersichtlichen `display_name`): Anfrage mit
        `addressdetails=1`, Anzeigename wird selbst aus den
        strukturierten Adressfeldern gebaut, Priorität
        city > town > municipality > village > suburb > city_district,
        Postleitzahl in Klammern dahinter, z.B. aus (erfundenen) Rohdaten
        `{village: "Kleindorf", town: "Großstadt", postcode: "<PLZ>"}`
        wird `"Großstadt (<PLZ>)"` (nicht `"Kleindorf"` und nicht der
        lange `display_name`). Bei genau einem Treffer wird er automatisch
        übernommen (kein Listen-Umweg), bei mehreren Treffern zeigt
        `location_screen.cpp::showResultList()` weiterhin eine Liste
        (doppelte Namen zusammengefasst, Namen umbrechen statt mit "..."
        gekürzt zu werden).
      - `app_config.h`: zusätzliches Feld `locationManual` (Version 5,
        optional, fehlend = `false`).
    - **Wochenansicht** (`week_view_screen.cpp`): kein eigener
      Hauptmenüpunkt, nur über Antippen der Wetterkachel erreichbar (nur
      wenn ein Standort bekannt ist). Ortsname oben mit "Zum Ändern
      antippen"-Hinweis darunter (öffnet `location_screen.cpp`). 7 Zeilen
      (Heute/Morgen/Wochentagskürzel), Wochentag wird aus dem
      Kalenderdatum der Antwort berechnet (`weather_client.cpp::
      parseDateNoon()`, lokales Mittags-Epoch gegen DST-Randfälle), NICHT
      aus der Uhr. Sonnenscheindauer nur, wenn sie tatsächlich in den
      verbleibenden Platz der Zeile passt (echte Pixelbreite geprüft),
      sonst weggelassen.
    - **Seitensprung aus einer Kachel** (`page_nav.h/.cpp`): schmaler
      Umweg, weil `currentPage` privat in `main_screen.cpp` lebt —
      `HomePage::onTap()` ruft `PageNav::requestPage(index)`,
      `main_screen.cpp::tick()` prüft das nach jedem `onTap()` und setzt
      `currentPage` VOR dem Neuzeichnen (sonst würde noch mit der alten
      Seite gezeichnet).
    - `app_config.h` Version 5: neue Felder `locationName`/`locationLat`/
      `locationLon`/`locationSet`/`locationManual`, alle optional gelesen
      (fehlend = leer/`false`, kein formatVersion-Zweig nötig, siehe
      `AppConfigStore::load()`).
22. **Fetch-Task-Absicherung** (unbeaufsichtigter Betrieb, Alex' Wunsch
    nach einem beobachteten mehrminütigen Haenger bei der
    Leistungskurve): zwei unabhängige Schutzschichten.
    - **Gesamt-Zeitlimit pro HTTPS-Abfrage** (`http_deadline_guard.h/.cpp`,
      `Config::HTTP_TOTAL_TIMEOUT_MS = 20000`): ein `esp_timer`
      (Software-Timer, läuft im eigenen Task-Kontext) ruft bei Ablauf
      `WiFiClientSecure::stop()` auf dem gerade aktiven Client auf - das
      lässt JEDEN blockierenden Aufruf der HTTPClient-Bibliothek
      (`connect()`/`getString()`) sofort mit einem Fehler zurückkehren.
      Deckt die GESAMTE Dauer ab (Verbindungsaufbau+Senden+Lesen), nicht
      nur einzelne Socket-Lesevorgänge - genau das unterschied sich vom
      vorher schon gesetzten `http.setConnectTimeout()`/`setTimeout()`
      (bleiben als 10s-Verbindungsaufbau-Teillimit aktiv), die ein
      Server, der immer wieder (aber zu selten) ein paar Bytes sendet,
      einzeln unterschreiten kann. Eingebaut in
      `epever_client.cpp::httpPostJson()` UND `http_get_json.cpp::fetch()`
      (Wetter/Geocoding) - deckt damit automatisch jede Abfrage ab, die
      diese beiden gemeinsamen Helfer benutzt. Bei Auslösung:
      `httpCode=-2` (eigener Sentinel-Wert) + Log-Zeile, der bestehende
      Wiederholungs-Ablauf greift danach unverändert. **Live getestet**
      (Auftrag verlangte das ausdrücklich): `EPEVER_BATTERY_URL`
      testweise auf `https://httpbin.org/delay/8` (antwortet nach 8s)
      gesetzt, `HTTP_TOTAL_TIMEOUT_MS` testweise auf 3000 - jede Abfrage
      brach zuverlässig nach 3s ab (`Gesamt-Zeitlimit (3s) ausgeloest`),
      kein Hängenbleiben, Gerät blieb durchgehend ansprechbar. Danach
      beide Werte zurückgesetzt.
    - **Watchdog fürs komplette Haengenbleiben** (`fetch_heartbeat.h/.cpp`,
      `fetch_watchdog.h/.cpp`): der Fetch-Task meldet nach JEDEM
      abgeschlossenen Schleifendurchlauf (auch waehrend `pause()`, z.B.
      WLAN-Scan - das ist kein Haenger) einen Herzschlag
      (`FetchHeartbeat::beat()`, Mutex-geschuetzter Zeitstempel, gleiches
      Prinzip wie `WifiStatus`). `FetchWatchdog::tick()` (Core 1,
      `main.cpp::loop()`) prueft den Abstand: ab 10 Minuten nur Log-
      Meldung (die "Stand"-Zeiten auf den Seiten frieren dadurch ohnehin
      schon von selbst ein, weil `SolarData`/`WeatherData` nur bei
      Erfolg aktualisiert werden - keine extra Logik dafuer noetig), ab
      15 Minuten `YieldLedger::flush()` (erzwingt das Speichern trotz
      des sonst geltenden Mindestintervalls) + `esp_restart()`. Bewusst
      EIN eigener Mechanismus statt `esp_task_wdt`: nur so laesst sich
      zuerst eine lange Karenzzeit geben und VOR dem Neustart noch
      geordnet die SD-Karte sichern - ein ausgeloester `esp_task_wdt`
      wuerde sofort (ueblicherweise Sekunden, nicht Minuten) einen
      Panic/Reset ohne Gelegenheit zum Aufraeumen ausloesen, und seine
      globale Zeitschranke (sdkconfig) wuerde sich mit der des
      Arduino-`loop()`-Tasks ueberschneiden. **Hoechstens 3 Neustarts
      HINTEREINANDER** (ohne dazwischen einen echten Herzschlag), sonst
      bleibt das Geraet liegen (Anzeige bleibt bedienbar) statt sich in
      einer Schleife totzulaufen - der Zaehler steht in einer winzigen
      Datei (`/SolarGuardian/watchdog.json`), NICHT in `RTC_DATA_ATTR`
      oder per `time(nullptr)`-Vergleich: beide wurden in einem Live-
      Test ausprobiert und griffen dort nie (RTC-Speicher ueberlebte
      den `esp_restart()` auf diesem Board in der Praxis nicht
      zuverlaessig; die Systemzeit ist direkt nach einem Haenger-
      Neustart oft noch nicht per NTP synchronisiert). **Zweite
      gefundene Falle beim Umsetzen**: `SD.open(pfad, FILE_WRITE)`
      HAENGT beim ESP32-SD-Treiber an, kuerzt die Datei NICHT - ohne
      vorheriges `SD.remove()` (gleiches Muster wie
      `app_config.cpp::save()`) sammelten sich mehrere JSON-Objekte
      hintereinander in der Datei, `deserializeJson()` las beim
      naechsten Laden dadurch immer nur das ERSTE (also den
      allerersten je geschriebenen Wert) zurueck - der Zaehler schien
      dadurch nie hochzuzaehlen, bis das erkannt und behoben wurde.
      **Live getestet** (verkuerzte Schwellwerte 8s/16s, Herzschlag
      testweise auskommentiert): Warnung erschien nach 8s, Neustart nach
      16s (`Reset-Grund: Software (esp_restart)` im Log des naechsten
      Boots bestaetigt), Zaehler zaehlte korrekt 1→2→3 ueber drei echte
      Neustarts hoch, beim vierten Versuch blieb das Geraet wie
      vorgesehen liegen (kein weiterer Neustart). Danach alle
      Testaenderungen zurueckgesetzt. **Nachtrag** (Auftrag "Neustart-
      Zaehler", zweite Runde): der Zaehler wurde vorher NUR zurueckgesetzt,
      wenn WAEHREND DESSELBEN Boots schon gewarnt worden war und sich der
      Herzschlag seitdem erholt hatte - ein Geraet, das nach einem
      Watchdog-Neustart einfach wieder normal lief (kein erneuter Stall in
      diesem Boot), behielt den alten Zaehlerstand fuer immer, bis
      irgendwann unabhaengige, zeitlich weit auseinanderliegende Vorfaelle
      sich zu `FETCH_WATCHDOG_MAX_CONSECUTIVE_REBOOTS` aufsummierten. Fix:
      `SolarData::successfulFetchCount` (steigt bei JEDEM erfolgreichen
      Ertrags-/Batterie-Zyklus unbedingt, anders als `changeCounter`) wird
      bei Boot als Basislinie genommen - nach
      `Config::FETCH_WATCHDOG_RESET_AFTER_SUCCESSFUL_CYCLES` (2) neuen
      erfolgreichen Zyklen seit dem Boot wird der Zaehler erst zurueckgesetzt.
      Zusaetzlich gibt das Geraet nach Erreichen der maximalen Neustarts
      NICHT mehr fuer immer auf: hoechstens ein weiterer Neustart-Versuch
      pro Stunde (`Config::FETCH_WATCHDOG_HOURLY_RETRY_MS`, millis()-
      basiert - in diesem Zustand finden ja keine Neustarts im bisherigen
      Takt mehr statt, millis() laeuft also ungestoert durch), bedienbar
      bleibt es in der Zwischenzeit durchgehend.
    - **SD-Schreibfehler-Audit** (Auftrag, nach obigem Fund): JEDE
      `SD.open(pfad, FILE_WRITE)`-Stelle im Projekt geprueft. Fast alle
      folgten bereits dem korrekten Tmp-Datei+Umbenennen-Muster
      (`app_config.cpp`, `yield_ledger.cpp`s `save()`/`ensureYearsFileExists()`/
      `writeYearToYearsFile()`/`writeHistoryBackup()`, `fetch_watchdog.cpp`).
      Zwei Stellen OHNE Risiko fuer falsche Werte, aber defensiv ergaenzt:
      `sd_storage.cpp::migrateHistoryFolder()` (schreibt praktisch nie auf
      eine bereits existierende Datei, da vorher explizit geprueft) und
      `yield_ledger.cpp`s Import-Marker-Datei (nur per `SD.exists()`
      abgefragt, Inhalt wird nie gelesen) - beide bekamen trotzdem ein
      `SD.remove()` vor dem `FILE_WRITE`, rein zur Konsistenz mit der
      Projekt-Regel unten. `days.csv` nutzt bewusst `FILE_APPEND` (jede
      Zeile ist ein neuer Tag, Anhaengen ist hier das gewuenschte
      Verhalten). **Regel fuer neue SD-Schreibstellen**: wer eine Datei
      komplett ersetzen will, MUSS sie vorher per `SD.remove()` entfernen
      (oder ueber eine Tmp-Datei + `SD.rename()` schreiben) - `FILE_WRITE`
      haengt beim ESP32-SD-Treiber IMMER an, kuerzt nie.
    - **Reset-Grund-Log** (`main.cpp::logResetReason()`, `esp_reset_reason()`):
      bei JEDEM Start, nicht nur nach einem Watchdog-Neustart - damit
      sich im Serial-Log nachvollziehen laesst, ob ein Neustart vom
      Watchdog, einem Stromausfall, einem Absturz o.ae. kam.
    - Core 1 (Anzeige/Touch) wartet nie auf den Fetch-Task - beide
      Schutzschichten leben vollstaendig auf/fuer Core 0, Core 1 liest
      nur einen Mutex-geschuetzten Zeitstempel (`FetchHeartbeat::
      msSinceLastBeat()`, nicht-blockierend) und schreibt im Ernstfall
      selbst (wie ohnehin schon ueblich) auf die SD-Karte.

23. **Seite "PV-Status"** (`pv_status_page.h/.cpp`, Auftrag "Neuer
    Bildschirm PV"): direkt nach "Batterie" (seit Auftrag
    "Seitenreihenfolge am CYD ändern" die 6. von 7 Seiten, davor war sie
    die letzte - "Verlauf" ist jetzt die letzte Seite).
    2x2-Kacheln PV-Spannung/PV-Strom/PV-Leistung/heutiges
    Leistungsmaximum, TILE_H 50 wie die anderen 2x2-Kachelseiten.
    Quellen: PV-Spannung/PV-Strom laufen bereits in der bestehenden
    Batterie-Abfrage mit (`EpeverDatapoints::PV_VOLTAGE/PV_CURRENT`,
    `SolarData::pvVoltage/pvCurrent`), PV-Leistung ist exakt
    `SolarData::currentPowerW` - DIESELBE Quelle wie die Leistungs-
    anzeige auf der Uebersicht und der Seite "Leistung" (keine eigene
    Abfrage, keine Abweichung zwischen den Anzeigen moeglich). Das
    heutige Leistungsmaximum ist KEIN Portalwert, sondern wird genau
    wie die "Max:"-Zeile im Tagesverlauf aus `SolarData::dailyCurve[]`
    berechnet (gleiche Quelle, in `pv_status_page.cpp` und
    `web_server.cpp` je lokal dupliziert statt geteilt, siehe
    Code-Stil) - setzt sich damit automatisch zum Tagesfenster-Beginn
    (04:30) zurueck und wird nach einem Neustart ueber
    `fetch_task.cpp::runDailyCurveBackfillIfNeeded()` aus dem Portal
    neu aufgebaut (keine eigene Persistenz noetig). Refresh-
    Rueckmeldung wie bei den anderen Kachelseiten (`RefreshFeedbackText::
    FeedbackState`), aber bewusst NUR in der tatsaechlich angetippten
    Kachel sichtbar, nicht in allen vier gleichzeitig. Eigener WebUI-
    Reiter "PV-Status" direkt nach "Batterie" (vor "Verlauf") - zeigt
    dieselben 4 Werte wie auf dem Geraet, auch wenn PV-Spannung/-Strom/
    -Leistung zusaetzlich im Reiter "Leistung" zu sehen sind (keine
    Ausnahme von der "jede Seite bekommt einen Reiter"-Regel).

24. **Aktiv-Auslesen beim Antippen** (`epever_client.cpp::
    activeReadBattery()`, `active_read_limiter.h`, `epever_active_read.h`,
    `fetch_task.h::RefreshRequest`): ein manueller Tipp auf eine Kachel
    loest eine frische Messung beim Regler selbst aus (ein Befehl PRO
    angezeigtem Datenpunkt, siehe Tabelle unten), statt wie der geplante
    Takt nur den letzten im Portal gespeicherten Wert zu lesen -
    ausschliesslich lesend (fest auf einen einzigen, nie
    konfigurierbaren Lese-Modus begrenzt), nur im Tagesfenster, nur auf
    Antippen. Wartet bis zu 50s gemeinsam auf alle neuen Zeitstempel,
    die Rueckmeldung selbst zeigt "Lade..." bis zu 55s. Die Obergrenzen
    (Mindestabstand zwischen zwei Tipps, max. Befehle/Stunde, max.
    Befehle/Tag) zaehlen BEFEHLE, nicht Tipps - ein Tipp kann je nach
    Bereich mehrere Befehle ausloesen:

    | Bereich | gelesene Werte |
    |---|---|
    | Uebersicht "Leistung" | PV-Leistung |
    | Uebersicht "Ertrag" | keine (nicht einzeln lesbar) |
    | Uebersicht "Batterie" | Ladestand, Spannung |
    | Seite "Leistung" (ganze Seite) | PV-Leistung |
    | Seite "Ertrag" (ganze Seite) | keine (nicht einzeln lesbar) |
    | Seite "Batterie" (ganze Seite) | Spannung, Strom, Ladestand, Temperatur |
    | PV-Status: PV-Spannung/-Strom/-Leistung | jeweils der eigene Wert |
    | PV-Status: "Max heute" | keine (nicht einzeln lesbar) |

25. **Naechtlicher Batterie-Abruf** (`fetch_task.cpp::taskFn()`, Auftrag
    "Batterie auch nachts abfragen"): spaetestens ab 22:55 Uhr bis 04:30
    Uhr wird alle 10 Minuten NUR die Batterie passiv abgefragt (derselbe
    Abruf wie tagsueber, kein aktives Auslesen) - UNABHAENGIG davon, ob
    die Abschlussabfrage/Sicherung bestaetigt wurde (die Bestaetigung
    lebt nur im RAM und geht bei jedem Neustart verloren; ein Neustart
    mitten in der Nacht darf den Abruf nicht bis zum naechsten Tag
    blockieren). Vor 22:55 (im Abschlussfenster 22:30-22:55) gilt
    weiterhin: nur nach bestaetigter Abschlussabfrage/Sicherung. Schreibt
    ausdruecklich nichts in Ertragsbuch, Tageskurve oder auf die SD-
    Karte, aktualisiert ausschliesslich Batteriefelder im Speicher (kein
    PV-Spannung/-Strom/-Leistung).

26. **"Neuester Wert"-Abruf nimmt immer den aktuellsten Stand**
    (`epever_client.cpp::fetchBatteryData()`, Auftrag "Fehler im Abruf
    beheben"): Endzeit = jetzt (plus kleiner Puffer gegen Uhrabweichung),
    Start schmal genug davor, damit bei der Portal-Antwort nie aeltere
    Werte statt der echten letzten Minutenwerte zurueckkommen (frueher
    ein zu breites 3h-Fenster mit zu wenigen erlaubten Werten - siehe
    config.h `EPEVER_BATTERY_WINDOW_MS`/`EPEVER_BATTERY_SAMPLE_LIMIT`).
    Gilt fuer Batterie, Leistung, PV-Spannung/-Strom, Max/Min und Status,
    sowohl im Tages- als auch im naechtlichen Abruf (dieselbe Funktion).
    Tagesertrag und die 3h-Leistungskurve sind davon ausdruecklich
    ausgenommen - bleiben bei ihrem eigenen, bewusst anderen Fenster.

27. **Geraete-Seriennummer fuer das Aktiv-Auslesen** (Auftrag
    "Seriennummer-Quelle fuer das Aktiv-Auslesen"): kommt aus derselben
    Quelle wie ueberall sonst im Projekt - dem einmal bei der
    Ersteinrichtung (oder automatisch nach dem Login) ermittelten und in
    der Konfiguration abgelegten Wert, NICHT aus einer eigenen, erneuten
    Geraeteliste-Abfrage beim Antippen. Fehlt sie, wird das als eigene,
    klare Rueckmeldung gezeigt - es wird dabei NIE ein Befehl gesendet.
    Erscheint wie jede Geraete-Seriennummer im Projekt nie in Logs,
    Kommentaren oder der WebUI.

## Code-Struktur

- `config.h` — alle Konstanten (Pins, Größen, Zeiten, Farbpalette, API-URLs)
- `app_config.h/.cpp` — Konfigurationsstruktur (3 WLAN-Plätze, Sprache,
  Geräte-Seriennummer, Wetter-Standort) + SD-Laden/Speichern +
  Versionsmigration (1→2→3→4→5)
- `display.h/.cpp` — TFT-Objekt, Init, Systemfarbe
- `touch_input.h/.cpp` — Touch-Rohwerte, Kalibrierung, Tap-Erkennung
- `swipe_input.h/.cpp` — Wisch-Gesten-Erkennung (links/rechts)
- `sd_storage.h/.cpp` — SD-Karte mounten, Ordner anlegen
- `keypad.h/.cpp` — Bildschirmtastatur
- `help_popup.h/.cpp` — "?"-Knopf + Erklär-Fenster
- `layout_utils.h/.cpp` — `fitText()` fürs automatische Schriftgrößen/
  Kürzen
- `layout_check.h/.cpp` — Debug-Layout-Prüfer (Überlappung/Bildschirmrand)
- `i18n.h/.cpp` — zentrale zweisprachige Textliste
- `wifi_scan.h/.cpp` — WLAN-Scan/Verbindung (einzelner Versuch)/Signalstärke
- `wifi_connect_manager.h/.cpp` — nicht-blockierender Verbindungs-Zyklus
  über die 3 gespeicherten Plätze, läuft NUR im Fetch-Task (Core 0)
- `wifi_status.h/.cpp` — Fetch-Task → Core 1: aktuell verbundener Platz
- `account_bridge.h/.cpp` — Core 1 → Fetch-Task: Zugangsdaten-Kopie
- `location_bridge.h/.cpp` — Core 1 → Fetch-Task: Wetter-Standort-Kopie
  (Breite/Länge), gleiches Prinzip wie `account_bridge.h`
- `solar_data.h/.cpp` — Fetch-Task → Core 1: Solaranlagen-Daten + Status
- `weather_data.h/.cpp` — Fetch-Task → Core 1: Wetterdaten (aktuell + 7
  Tage Vorhersage), gleiches Mutex/Momentaufnahme-Prinzip wie
  `solar_data.h`
- `fetch_task.h/.cpp` — der Fetch-Task selbst (Core 0), Befehls-Queue,
  Pause/Resume, Ertrags-/Batterie-/Wetter-Zeitplan
- `epever_client.h/.cpp` — HTTPS-Zugriff auf das Solar Guardian Portal
  (Login, Ertrag, Batterie/Verlauf)
- `http_get_json.h/.cpp` — gemeinsamer HTTPS-GET-Helfer fürs Wetter/
  Geocoding (gleiches Grundprinzip wie `epever_client.cpp`s POST-Helfer,
  aber GET und ohne Appid/Zugangsdaten-Header)
- `weather_client.h/.cpp` — Open-Meteo-Wetterabfrage (aktuell + 7-Tage-
  Vorhersage), schreibt in `weather_data.h`
- `weather_icons.h/.cpp` — zeichnet Wettersymbole rein mit TFT_eSPI-
  Grundformen (keine Bilddateien), Größe als Parameter
- `geocoding_client.h/.cpp` — Orts-/Postleitzahlensuche ueber Nominatim
  (bis zu 5 Treffer) - NICHT Open-Meteo, siehe "Fetch-Task-Absicherung"/
  Punkt 21 oben
- `ip_geolocation_client.h/.cpp` — automatische Standort-Ermittlung ueber
  die oeffentliche IP-Adresse (ipwho.is, Rueckfall geojs.io)
- `ip_location_data.h/.cpp` — Fetch-Task → Core 1: Ergebnis der
  automatischen IP-Standort-Ermittlung
- `http_deadline_guard.h/.cpp` — Gesamt-Zeitlimit (20s) fuer jede HTTPS-
  Abfrage im Fetch-Task, unabhaengig von Pro-Socket-Timeouts
- `fetch_heartbeat.h/.cpp` — Lebenszeichen des Fetch-Tasks fuer Core 1
  (Mutex-geschuetzter Zeitstempel)
- `fetch_watchdog.h/.cpp` — Core 1: ueberwacht den Herzschlag, loggt/
  startet notfalls kontrolliert neu (siehe "Fetch-Task-Absicherung" oben)
- `epever_datapoints.h` — Datenpunkt-IDs der Port-7002-Abfrage
- `epever_auth_headers.h` — noch unbekannte, standardmäßig leere
  Zusatz-Header für Port 7002 (`languagetype`/`sdk-version`/
  `usr-source`, siehe "Solar Guardian Portal API" oben)
- `yield_ledger.h/.cpp` — das Ertragsbuch (Core 1, SD-gestützt), schreibt
  auch das History-Backup und `years.json`
- `calibration_screen.h/.cpp`, `wifi_select_screen.h/.cpp`,
  `credentials_screen.h/.cpp`, `serial_number_screen.h/.cpp`,
  `location_screen.h/.cpp` — Ersteinrichtung-Bildschirme
  (`wifi_select_screen`/`credentials_screen`/`serial_number_screen`/
  `location_screen` werden auch von Netzwerke/Konto/Menü wiederverwendet)
- `pages.h/.cpp`, `home_page.h/.cpp`, `power_page.h/.cpp`,
  `daily_curve_page.h/.cpp`, `yield_page.h/.cpp`, `battery_page.h/.cpp`,
  `pv_status_page.h/.cpp`, `years_page.h/.cpp` — die per Wisch-Geste
  erreichbaren Hauptseiten (`home_page` ist Seite 1; seit Auftrag
  "Seitenreihenfolge am CYD ändern" ist `years_page` ("Verlauf") die
  letzte/7. Seite, direkt nach `pv_status_page`)
- `page_nav.h/.cpp` — schmaler Umweg, damit eine Seite (z.B. `home_page`)
  zu einer anderen Wisch-Seite springen kann
- `week_view_screen.h/.cpp` — Wochenansicht (kein Hauptmenüpunkt, nur
  über die Wetterkachel erreichbar)
- `main_screen.h/.cpp`, `menu_screen.h/.cpp`, `networks_screen.h/.cpp`,
  `account_screen.h/.cpp`, `color_picker_screen.h/.cpp`,
  `language_screen.h/.cpp`, `reset_screen.h/.cpp`,
  `yield_values_screen.h/.cpp` — laufender Betrieb
- `boot_logo.h/.cpp` — Startlogo "Sonne im Schild", komplett im Code
  gezeichnet, läuft während der übrigen Startsequenz
- `easter_egg.h/.cpp` — verstecktes Zusatzfeature, Details bewusst nicht
  dokumentiert
- `main.cpp` — orchestriert den Startablauf
