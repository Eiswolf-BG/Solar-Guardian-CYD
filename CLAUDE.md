# Solar Guardian CYD — Projektkontext für Claude Code

Lies diese Datei zuerst, bevor du am Code etwas änderst.

## Was ist das Projekt?

Ein privates Solar-Dashboard für das Portal "Solar Guardian" (EPEVER-Cloud)
auf einem ESP32 "Cheap Yellow Display" (CYD, ESP32-2432S028), 240x320
Touch-TFT, Hochformat. Rein privates Gerät, **nicht für GitHub oder sonst
irgendeine Veröffentlichung gedacht**.

## Wer nutzt das?

Die Nutzerin — Anfängerin bei Arduino/ESP32. Bitte verständlich und knapp erklären,
was gebaut wurde, nicht von Fachbegriffen ausgehen, die als bekannt
vorausgesetzt werden.

## Arbeitsweise

- Wir bauen ein Feature nach dem anderen und warten nach jedem Feature auf
  ihr OK, bevor das nächste angefangen wird.
- Bei Unklarheiten nachfragen statt zu raten.

## Pflichtregeln

- **Nur bauen und flashen.** Niemals committen, taggen oder pushen, außer
  die Nutzerin fordert das in ihrer aktuellen Nachricht ausdrücklich an. Seit
  V1.0 liegt das Projekt öffentlich auf GitHub
  (`Eiswolf-BG/Solar-Guardian-CYD`, MIT-Lizenz, Web-Flasher via GitHub
  Pages) — das ändert nichts an dieser Regel, jeder Push/Tag/Release
  bleibt an eine ausdrückliche Aufforderung in der jeweiligen Nachricht
  gebunden.
- **Release-Notes sind IMMER zweisprachig**, erst Deutsch, dann
  Englisch (fester Wunsch der Projektinhaberin für dieses Repo: ein Nutzer des Geräts
  spricht nur wenig Englisch) — gilt für jedes GitHub-Release, ohne
  Ausnahme, auch bei kleinen Patch-Versionen.
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
    gekürzt** (ihr Wunsch, nach einem Bug auf der Seite "Jahre", wo
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
  Ausrichtung, ihr Wunsch), `tft.invertDisplay(true)` nötig für korrekte
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
- **Partitionierung: Single-App, kein OTA** (ihre bewusste Entscheidung,
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
  zeichnet.
- **Zugangsdaten an den Fetch-Task**: `AccountBridge` (siehe
  `account_bridge.h/.cpp`) — eigener kleiner Mutex, Core 1 schreibt dort
  die E-Mail/Passwort-Kopie (beim Start und nach jeder Kontoänderung über
  `account_screen.cpp`), der Fetch-Task liest sie beim eigenen Start und
  wenn `Command::AccountChanged` ankommt. **Core 0 greift dafür nie
  direkt auf `appConfig` zu.** Das Portal-Token lebt ausschließlich im
  RAM des Fetch-Tasks (task-lokale `String`), wird NIE auf die SD-Karte
  geschrieben, und wird bei jeder Kontoänderung verworfen (neu einloggen).
- **WLAN-Verbindungsstatus zurück an Core 1**: `WifiStatus` (siehe
  `wifi_status.h/.cpp`) — der Fetch-Task meldet dort, welcher gespeicherte
  Platz aktuell verbunden ist. Core 1 übernimmt daraus
  `appConfig.lastActiveWifiSlot` und speichert (SD-Zugriff bleibt
  exklusiv auf Core 1).
- **Befehle von Core 1 an den Fetch-Task**: eine FreeRTOS-Queue
  (`FetchTask::sendCommand()`, siehe `fetch_task.h`), nicht per direktem
  Funktionsaufruf — z.B. `ReconnectNow`, `DisconnectWifi`,
  `ConnectSlot0/1/2`, `AccountChanged`, `LanguageChanged`, `RefreshNow`
  ("jetzt aktualisieren").
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
  (Feld `device_no`, Version 4). `Config::DEFAULT_DEVICE_NO` in
  `config.h` ist die echte Seriennummer des eigenen Reglers
  (`04510146093XRJTQ-00918`), kein Platzhalter — `AppConfigStore::
  load()` übernimmt diesen Standardwert beim Laden, wenn `device_no` in
  der `config.json` leer ist. `main.cpp` loggt beim Start nur
  `[core] device_no gesetzt: ja/nein` (nie den Wert selbst im Klartext).
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
**"Gesamt" gibt es bewusst nicht mehr** (ihr Wunsch, nicht gebraucht) —
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
- **History-Backup-Ordner** (`/SolarGuardian/History/tagesertrag.json`,
  siehe `Config::SD_HISTORY_*` in `config.h`): bei JEDER erfolgreichen
  Tagesertrag-Abfrage (alle 5 Minuten, über `SolarData::dayFetchSeq` als
  Änderungszähler erkannt) überschreibt Core 1 genau eine Datei mit
  `{"datum","zeit","kwh"}` — gespeichert wird `today_max` (der Tages-
  Höchstwert), NICHT der rohe Abfragewert, weil das Portal den Tageswert
  kurz vor Mitternacht auf 0 zurücksetzt; die Datei bekommt nie einen
  kleineren Wert fürs selbe Datum. Dient als Wiederherstellungs-Quelle:
  beim Start wird, falls das Datum der Backup-Datei dem heutigen Tag
  entspricht und ihr Wert größer als `today_max` ist, dieser Wert
  übernommen (geloggt als Datum+kWh); fehlt/ist defekt `yield.json`, wird
  `today_max` direkt aus der Backup-Datei gespeist.
- **Jahresübersicht** (`/SolarGuardian/years.json`, flache Map
  `{"YYYY": kwh}`): bei Jahreswechsel wird der abgeschlossene Jahreswert
  hier eingetragen. Fehlt die Datei, wird sie mit den Startwerten aus
  `seed_years.h` (`SeedYears::SEEDS`, eigene Konstanten, leicht
  anpassbar) angelegt. Seite "Jahre" (`years_page.h/.cpp`) zeigt eine
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
                        "Ertragsbuch" oben, Startwerte in seed_years.h
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
  nach oben. "Solar Guardian" sitzt 3px darunter (ihr Wunsch nach
  Sichtkontrolle am Gerät).

## Swipe-Struktur (Hauptbildschirm)

- Die Kopfzeile (Uhr links, WLAN-Balken rechts, "Solar Guardian"-Text
  mittig als Button zum Menü, ohne sichtbaren Rahmen) ist fest und wird
  nie durch einen Seitenwechsel neu gezeichnet.
- Darunter liegt der Seitenbereich - Wischen (links/rechts, ab ca. 50px
  überwiegend waagerechter Bewegung, siehe `swipe_input.cpp`, Prinzip
  übernommen aus eiswolfs-toolbox) wechselt zwischen den Seiten, im
  Rundlauf (nach der letzten kommt wieder die erste und umgekehrt).
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
   ("Leistung"/"Ertrag"/"Jahre"/"Batterie", Reihenfolge zentral in
   `pages.cpp::PAGES[]`) mit Punkte-Indikator.
4. **Menü** (`menu_screen.cpp`, für Stift-Bedienung ausgelegt):
   "Netzwerke", "Konto", "Systemfarbe", "Sprache / Language",
   "Ertragswerte", "Zurücksetzen", "Zurück".
5. **Netzwerke-Verwaltung** (`networks_screen.cpp`,
   `wifi_connect_manager.cpp`): bis zu 3 gespeicherte WLANs, freier Platz
   → Scan+Passwort+Verbindungstest vor dem Speichern, belegter Platz →
   Untermenü (Verbinden/Passwort ändern/Löschen/Zurück), aktuell
   verbundenes Netz mit Haken markiert. Verbindungsaufbau läuft komplett
   im Hintergrund (Fetch-Task), der Hauptbildschirm erscheint sofort.
6. **Konto** (`account_screen.cpp`): zeigt E-Mail + maskiertes Passwort,
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
    (`yield_ledger.h/.cpp`, `years_page.h/.cpp`, `seed_years.h`): Woche
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
    Seite "Ertrag" (Tipp darauf löst "jetzt aktualisieren" aus). Alle
    Offsets sind rechnerisch verkettet (nicht einzeln fest verdrahtet),
    siehe `power_page.cpp` Kommentar am Konstanten-Block — eine erste
    Fassung reichte mit der Statuszeilen-Tippfläche bis in die
    Seitenpunkte-/"?"-Zone hinein, was der (seit der Seite "Jahre")
    zentral in `main_screen.cpp` verankerte Layout-Prüfer direkt beim
    ersten Testlauf gemeldet hat.

## Code-Struktur

- `config.h` — alle Konstanten (Pins, Größen, Zeiten, Farbpalette, API-URLs)
- `app_config.h/.cpp` — Konfigurationsstruktur (3 WLAN-Plätze, Sprache,
  Geräte-Seriennummer) + SD-Laden/Speichern + Versionsmigration (1→2→3→4)
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
- `solar_data.h/.cpp` — Fetch-Task → Core 1: Solaranlagen-Daten + Status
- `fetch_task.h/.cpp` — der Fetch-Task selbst (Core 0), Befehls-Queue,
  Pause/Resume, Ertrags-/Batterie-Zeitplan
- `epever_client.h/.cpp` — HTTPS-Zugriff auf das Solar Guardian Portal
  (Login, Ertrag, Batterie/Verlauf)
- `epever_datapoints.h` — Datenpunkt-IDs der Port-7002-Abfrage
- `epever_auth_headers.h` — noch unbekannte, standardmäßig leere
  Zusatz-Header für Port 7002 (`languagetype`/`sdk-version`/
  `usr-source`, siehe "Solar Guardian Portal API" oben)
- `yield_ledger.h/.cpp` — das Ertragsbuch (Core 1, SD-gestützt), schreibt
  auch das History-Backup und `years.json`
- `seed_years.h` — Startwerte für `years.json`, falls die Datei fehlt
- `calibration_screen.h/.cpp`, `wifi_select_screen.h/.cpp`,
  `credentials_screen.h/.cpp` — Ersteinrichtung-Bildschirme
  (`wifi_select_screen`/`credentials_screen` werden auch von
  Netzwerke/Konto wiederverwendet)
- `pages.h/.cpp`, `power_page.h/.cpp`, `yield_page.h/.cpp`,
  `years_page.h/.cpp`, `battery_page.h/.cpp` — die per Wisch-Geste
  erreichbaren Hauptseiten
- `main_screen.h/.cpp`, `menu_screen.h/.cpp`, `networks_screen.h/.cpp`,
  `account_screen.h/.cpp`, `color_picker_screen.h/.cpp`,
  `language_screen.h/.cpp`, `reset_screen.h/.cpp`,
  `yield_values_screen.h/.cpp` — laufender Betrieb
- `main.cpp` — orchestriert den Startablauf
