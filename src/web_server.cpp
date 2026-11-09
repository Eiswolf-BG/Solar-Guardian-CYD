#include "web_server.h"
#include "web_files.h"
#include "house_image.h"
#include "sun_times.h"
#include "easter_egg.h"
#include "solar_data.h"
#include "weather_data.h"
#include "yield_ledger.h"
#include "app_config.h"
#include "display.h"
#include "i18n.h"
#include "config.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>
#include <math.h>

namespace WebServer_ {

namespace {
    WebServer server(80);
    bool started = false;

    // -------------------------------------------------------------
    // Statische Seite (HTML+CSS+JS) komplett im Flash (PROGMEM) - KEINE
    // serverseitige Interpolation noetig: die Seite ist zu 100% statisch,
    // alle Werte holt sich das JS selbst per fetch('/api/data') (Auftrag
    // "keine grossen Strings bauen"). Gesendet wird sie in Stuecken direkt
    // aus dem Flash (siehe sendPageChunked()).
    // -------------------------------------------------------------
    const char PAGE_HTML[] PROGMEM = R"HTMLPAGE(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Solar Guardian</title>
<style>
:root{--accent:#39ff14;--bg:#000;--card:#141414;--text:#e8e8e8;--dim:#8a8a8a;}
*{box-sizing:border-box;}
body{margin:0;background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;}
header{padding:10px 14px;border-bottom:1px solid #222;display:flex;flex-wrap:wrap;gap:6px 16px;align-items:baseline;}
header h1{font-size:1.1rem;margin:0;color:var(--accent);}
header .meta{font-size:.78rem;color:var(--dim);}
nav{display:flex;overflow-x:auto;border-bottom:1px solid #222;background:#101010;}
nav button{flex:1 0 auto;padding:10px 12px;background:none;border:none;color:var(--dim);font-size:.85rem;cursor:pointer;white-space:nowrap;border-bottom:3px solid transparent;}
nav button.active{color:var(--accent);border-bottom-color:var(--accent);}
main{padding:14px;max-width:760px;margin:0 auto;}
section{display:none;}
section.active{display:block;}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px;}
.card{background:var(--card);border:1px solid #242424;border-radius:10px;padding:14px;}
.card .label{font-size:.78rem;color:var(--accent);margin-bottom:6px;text-transform:uppercase;letter-spacing:.03em;}
.card .value{font-size:1.7rem;font-weight:600;color:var(--dim);}
.card .unit{font-size:.9rem;color:var(--accent);margin-left:4px;}
.card .sub{font-size:.85rem;color:var(--dim);margin-top:6px;}
.weather-hero{display:flex;align-items:center;gap:16px;}
.weather-hero svg{width:72px;height:72px;flex:none;}
.weather-hero .temp{font-size:2.2rem;font-weight:600;color:var(--dim);}
#weekRows{display:flex;flex-direction:column;gap:6px;margin-top:14px;}
.weekRow{display:flex;align-items:center;gap:10px;background:var(--card);border:1px solid #242424;border-radius:8px;padding:8px 12px;}
.weekRow svg{width:28px;height:28px;flex:none;}
.weekRow .day{width:72px;color:var(--accent);font-size:.85rem;}
.weekRow .range{margin-left:auto;font-size:.85rem;color:var(--dim);}
.weekRow .range b{color:var(--dim);}
canvas{width:100%;height:160px;background:#101010;border:1px solid #242424;border-radius:8px;}
.maxLine{font-size:1rem;margin:6px 0 10px;}
.maxLine .lbl{color:var(--accent);}
.maxLine .v{color:var(--dim);font-weight:600;}
.maxLine .u{color:var(--accent);}
.subnav{display:flex;gap:8px;margin-bottom:10px;}
.subnav button{flex:1;padding:8px;background:none;border:1px solid #333;border-radius:6px;color:var(--accent);font-size:.85rem;cursor:pointer;}
.subnav button.active{background:var(--accent);color:#000;border-color:var(--accent);}
.barsChart{display:flex;align-items:flex-end;gap:10px;height:160px;padding-top:20px;}
.barsChart .bar{flex:1;display:flex;flex-direction:column;align-items:center;height:100%;}
.barsChart .bar .v{font-size:.72rem;color:var(--dim);margin-bottom:4px;}
/* Eigene, feste "Spur" fuer den Balken, getrennt von den Textzeilen
   (Auftrag "Balkenhoehe Monate/Jahre") - fill.style.height wird als
   Prozentwert dieser Spur gesetzt, nicht der ganzen .bar-Box. Ohne diese
   Trennung konkurrierte der Balken um Platz mit v/y (Flex-Kinder derselben
   Spalte), wodurch hohe Werte ungleichmaessig zusammengedrueckt wurden und
   unterschiedliche Werte fast gleich hoch aussahen - bei "Tage" durch die
   groessere Werte-Streuung kaum auffaellig, bei "Monate"/"Jahre" (kleinere
   relative Unterschiede) deutlich sichtbar. min-height:0 ist noetig, damit
   der Flex-Kindknoten ueberhaupt kleiner als sein natuerlicher Inhalt
   werden darf. */
.barsChart .bar .track{flex:1;min-height:0;width:70%;display:flex;flex-direction:column;justify-content:flex-end;}
.barsChart .bar .fill{width:100%;background:var(--accent);opacity:.45;border-radius:4px 4px 0 0;}
.barsChart .bar.cur .fill{opacity:1;}
.barsChart .bar .y{font-size:.78rem;color:var(--dim);margin-top:6px;}
footer{text-align:center;color:var(--dim);font-size:.72rem;padding:16px;}
footer div{margin-top:3px;}
.statusDot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:5px;background:#555;}
.statusDot.ok{background:#39ff14;}
.statusDot.bad{background:#ff3b3b;}
.filesHint{color:var(--dim);font-size:.78rem;margin-bottom:8px;}
.filesPath{color:var(--accent);font-size:.85rem;margin-bottom:6px;word-break:break-all;}
.filesRow{display:flex;align-items:center;gap:8px;padding:7px 4px;border-bottom:1px solid #222;cursor:pointer;}
.filesRow .fname{flex:1;color:var(--dim);overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
.filesRow .fname.dir{color:var(--accent);font-weight:bold;}
.filesRow .fsize{color:var(--dim);font-size:.78rem;}
.filesRow button{background:none;border:1px solid #444;color:#ff6b6b;border-radius:4px;font-size:.75rem;padding:3px 7px;}
#filesMoreBtn{margin-top:8px;width:100%;padding:8px;background:none;border:1px solid var(--accent);color:var(--accent);border-radius:6px;}
#filesViewer{margin-top:12px;border-top:1px solid #333;padding-top:10px;}
.filesViewerHead{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;}
.filesViewerHead span{color:var(--accent);word-break:break-all;}
#filesCloseBtn{background:none;border:none;color:var(--dim);font-size:1.3rem;cursor:pointer;}
#filesEditor{width:100%;background:#101010;color:var(--dim);border:1px solid #333;border-radius:6px;font-family:monospace;font-size:.8rem;padding:8px;box-sizing:border-box;}
#filesTruncHint{color:var(--dim);font-size:.78rem;margin-bottom:6px;}
.filesViewerActions{margin-top:8px;}
#filesSaveBtn{background:var(--accent);color:#000;border:none;border-radius:6px;padding:8px 16px;}
.filesBackupBtn{display:block;width:100%;background:var(--accent);color:#000;border:none;border-radius:6px;padding:10px;font-weight:600;font-size:.9rem;margin-bottom:8px;}
.filesLockBadge{color:var(--accent);font-size:.68rem;border:1px solid var(--accent);border-radius:4px;padding:1px 5px;margin-right:4px;}
.filesRow button.dl{color:var(--accent);border-color:var(--accent);}
.filesRow button.dlFolder{color:var(--accent);border-color:var(--accent);font-size:.68rem;}
#filesProgress{color:var(--dim);font-size:.78rem;margin:6px 0;}
.err{color:#ff5050;}
/* Animierte Haus-Grafik (Auftrag "Animierte Haus-Grafik in der WebUI",
   Ueberarbeitungen "Tag/Nacht nach Sonnenstand" und "zwei Bild-Varianten") -
   Aussehen/Massverhaeltnis 1:1 aus der bestaetigten Vorlage solarenergy.html
   uebernommen (viewBox 1408x768, Punktgroesse/-farbe/-glow unveraendert). */
.houseWrap{margin-top:16px;}
.houseWrap.hidden{display:none;}
.houseHero{position:relative;width:100%;aspect-ratio:1408/768;border-radius:12px;overflow:hidden;background:#000;box-shadow:inset 0 0 40px 10px #000;}
.houseHero img,.houseHero svg{position:absolute;inset:0;width:100%;height:100%;display:block;z-index:1;}
.houseHero img{opacity:0;transition:opacity 1.2s;}
#wx{position:absolute;inset:0;width:100%;height:100%;pointer-events:none;background:transparent;border:none;border-radius:0;z-index:2;}
</style>
</head>
<body>
<header>
<h1>Solar Guardian</h1>
<span class="meta" id="locLine"></span>
<span class="meta" id="standLine"></span>
<span class="meta" id="nextLine"></span>
</header>
<nav id="tabs"></nav>
<main>
<section id="tab-overview">
  <div class="grid">
    <div class="card weather-hero" style="grid-column:1/-1;">
      <div id="heroIcon"></div>
      <div><div class="temp" id="heroTemp">--</div><div class="sub" id="heroLabel">Wetter</div></div>
    </div>
    <div class="card"><div class="label" id="lblPower">Leistung</div><div class="value"><span id="ovPower">--</span><span class="unit">W</span></div></div>
    <div class="card"><div class="label" id="lblToday">Heute</div><div class="value"><span id="ovToday">--</span><span class="unit">kWh</span></div></div>
    <div class="card"><div class="label" id="lblBattery">Batterie</div><div class="value"><span id="ovBatt">--</span><span class="unit">%</span></div></div>
    <div class="card"><div class="label" id="lblOvVolt">Spannung</div><div class="value"><span id="ovVolt">--</span><span class="unit">V</span></div></div>
  </div>
</section>
<section id="tab-power">
  <div class="grid">
    <div class="card"><div class="label" id="lblPvPower">Leistung</div><div class="value"><span id="pPower">--</span><span class="unit">W</span></div><div class="sub" id="pStand"></div></div>
    <div class="card"><div class="label" id="lblPvVolt">Spannung</div><div class="value"><span id="pVolt">--</span><span class="unit">V</span></div></div>
    <div class="card"><div class="label" id="lblPvCurr">Strom</div><div class="value"><span id="pCurr">--</span><span class="unit">A</span></div></div>
    <div class="card"><div class="label" id="lblPvMax">Max heute</div><div class="value"><span id="pMax">--</span><span class="unit">W</span></div></div>
  </div>
  <canvas id="chart" style="margin-top:14px;"></canvas>
</section>
<section id="tab-daily">
  <div class="maxLine"><span class="lbl" id="lblMax">Max</span>: <span class="v" id="dMax">--</span><span class="u"> W</span></div>
  <canvas id="dailyChart"></canvas>
</section>
<section id="tab-yield">
  <div class="grid">
    <div class="card"><div class="label" id="lblYToday">Heute</div><div class="value"><span id="yToday">--</span><span class="unit">kWh</span></div></div>
    <div class="card"><div class="label" id="lblYWeek">Woche</div><div class="value"><span id="yWeek">--</span><span class="unit">kWh</span></div></div>
    <div class="card"><div class="label" id="lblYMonth">Monat</div><div class="value"><span id="yMonth">--</span><span class="unit">kWh</span></div></div>
    <div class="card"><div class="label" id="lblYYear">Jahr</div><div class="value"><span id="yYear">--</span><span class="unit">kWh</span></div></div>
  </div>
</section>
<section id="tab-battery">
  <div class="grid">
    <div class="card"><div class="label" id="lblBSoc">Ladezustand</div><div class="value"><span id="bSoc">--</span><span class="unit">%</span></div></div>
    <div class="card"><div class="label" id="lblBVolt">Spannung</div><div class="value"><span id="bVolt">--</span><span class="unit">V</span></div></div>
    <div class="card"><div class="label" id="lblBCurr">Strom</div><div class="value"><span id="bCurr">--</span><span class="unit">A</span></div></div>
    <div class="card"><div class="label" id="lblBTemp">Temperatur</div><div class="value"><span id="bTemp">--</span><span class="unit">&deg;C</span></div></div>
    <div class="card"><div class="label" id="lblBVMax">Max. Spannung</div><div class="value"><span id="bVMax">--</span><span class="unit">V</span></div></div>
    <div class="card"><div class="label" id="lblBVMin">Min. Spannung</div><div class="value"><span id="bVMin">--</span><span class="unit">V</span></div></div>
    <div class="card" style="grid-column:1/-1;"><div class="label" id="lblBStatus">Status</div><div class="value" style="font-size:1.2rem;" id="bStatus">--</div></div>
  </div>
</section>
<section id="tab-pvstatus">
  <div class="grid">
    <div class="card"><div class="label" id="lblPvsVolt">PV-Spannung</div><div class="value"><span id="pvsVolt">--</span><span class="unit">V</span></div></div>
    <div class="card"><div class="label" id="lblPvsCurr">PV-Strom</div><div class="value"><span id="pvsCurr">--</span><span class="unit">A</span></div></div>
    <div class="card"><div class="label" id="lblPvsPower">PV-Leistung</div><div class="value"><span id="pvsPower">--</span><span class="unit">W</span></div></div>
    <div class="card"><div class="label" id="lblPvsMax">Max heute</div><div class="value"><span id="pvsMax">--</span><span class="unit">W</span></div></div>
    <div class="card" style="grid-column:1/-1;"><div class="label" id="lblPvsStatus">Status</div><div class="value" style="font-size:1rem;" id="pvsStatus">--</div></div>
  </div>
</section>
<section id="tab-history">
  <div class="subnav" id="histSubnav"></div>
  <div class="maxLine"><span class="u" id="histUnit">kWh</span></div>
  <div class="barsChart" id="histChart"></div>
</section>
<section id="tab-weather">
  <div class="card weather-hero">
    <div id="wIcon"></div>
    <div><div class="temp" id="wTemp">--</div><div class="sub" id="wLoc"></div></div>
  </div>
  <div id="weekRows"></div>
</section>
<section id="tab-files">
  <button id="filesBackupBtn" class="filesBackupBtn"></button>
  <div class="filesHint" id="filesHint"></div>
  <div class="filesHint" id="filesBackupHint"></div>
  <div id="filesProgress" style="display:none;"></div>
  <div class="filesPath" id="filesPath"></div>
  <div id="filesList"></div>
  <button id="filesMoreBtn" style="display:none;"></button>
  <div id="filesViewer" style="display:none;">
    <div class="filesViewerHead">
      <span id="filesViewerName"></span>
      <button id="filesCloseBtn">×</button>
    </div>
    <div id="filesTruncHint" style="display:none;"></div>
    <textarea id="filesEditor" rows="14"></textarea>
    <div class="filesViewerActions">
      <button id="filesSaveBtn"></button>
    </div>
  </div>
</section>
<div class="houseWrap" id="houseWrap">
<div class="houseHero">
<img id="houseImgV1Day" alt="">
<img id="houseImgV1Night" alt="">
<img id="houseImgV2Day" alt="">
<img id="houseImgV2Night" alt="">
<svg viewBox="0 0 1408 768" preserveAspectRatio="none" aria-hidden="true">
<defs><filter id="hg" x="-200%" y="-200%" width="500%" height="500%"><feGaussianBlur stdDeviation="4" result="hb"/><feMerge><feMergeNode in="hb"/><feMergeNode in="SourceGraphic"/></feMerge></filter></defs>
<g id="housePaths" fill="none" stroke-linecap="round" stroke-linejoin="round" filter="url(#hg)">
<path id="hna" d="M890 163 L886 153 L874 135 L865 122 L862 118 L856 110 L842 101 L823 95 L809 95 L795 98 L780 102 L767 105 L752 109 L739 112 L724 116 L704 122 L690 125 L680 128 L669 130 L658 133 L641 138 L621 143 L609 146 L593 150 L570 156 L552 161 L542 164 L533 166 L515 170 L492 176 L473 182 L462 184 L455 190 L456 208 L465 228 L472 239 L480 249 L493 267 L502 280 L506 286 L515 299 L526 315 L538 331 L542 345 L529 352 L514 356 L509 364 L509 384 L510 406 L510 420 L510 430 L512 446 L514 465 L512 480"/>
<path id="hnb" d="M560 356 L570 354 L580 352 L597 347 L612 342 L625 340 L638 335 L652 330 L667 326 L680 322 L687 328 L686 346 L684 362 L685 374 L685 392 L685 407 L678 424 L666 436 L653 442 L637 451 L618 458 L602 461 L590 463 L574 467 L554 472 L534 475 L512 480"/>
<path id="hnc" d="M560 356 L570 354 L580 352 L597 347 L612 342 L625 340 L638 335 L652 330 L667 326 L684 321 L700 316 L710 314 L724 310 L739 305 L753 300 L770 295 L792 289 L811 283 L824 279 L838 275 L852 276 L860 288 L863 304 L862 318 L862 336 L862 354 L863 372 L863 383"/>
<path id="hxa" d="M791 111 L772 116 L759 118 L747 122 L732 129 L717 136 L698 142 L680 149 L667 154 L659 158 L650 160 L636 164 L617 171 L599 177 L588 181 L572 188 L551 196 L536 202 L527 206 L517 209 L502 216 L488 227 L486 237 L493 247 L502 260 L512 275 L521 288 L530 302 L538 314 L545 322 L552 331 L557 340 L565 351 L576 366 L587 382 L592 390 L594 404 L596 426 L596 445 L596 460 L597 472 L597 479 L597 489 L598 503 L597 522 L593 544 L583 559 L572 563"/>
<path id="hxb" d="M768 309 L774 323 L775 335 L774 343 L781 356 L792 374 L798 383 L794 386 L781 396 L772 414 L768 433 L766 447 L766 456 L766 464 L767 476 L768 492 L768 510"/>
<path id="hxc" d="M768 309 L774 323 L775 335 L774 343 L781 356 L792 374 L807 382 L825 383 L838 382 L852 376 L870 368 L882 364 L894 360 L911 354 L921 350 L933 345 L951 338 L969 330 L985 323 L995 319 L1004 316 L1020 310 L1036 307 L1042 316 L1041 336 L1040 350 L1041 359 L1042 370 L1042 383"/>
</g><g id="houseRunner" fill="none" stroke-linecap="round" filter="url(#hg)"></g></svg>
<canvas id="wx"></canvas>
</div>
</div>
</main>
<footer>
  <div><span id="footerConnDot" class="statusDot"></span><span id="footerConnText"></span></div>
  <div id="footerPortalText"></div>
</footer>
<script>
(function(){
var L={
 de:{tabs:['Übersicht','Leistung','Tagesverlauf','Ertrag','Batterie','PV-Status','Verlauf','Wetter','Daten'],
     power:'Leistung',today:'Heute',battery:'Batterie',week:'Woche',month:'Monat',year:'Jahr',
     voltage:'Spannung',current:'Strom',temp:'Temperatur',status:'Status',weather:'Wetter',
     stand:'Stand',next:'Nächste Abfrage',noLoc:'Standort nicht gesetzt',noConn:'Keine Verbindung zum Gerät',
     wd:['So','Mo','Di','Mi','Do','Fr','Sa'],
     mo:['Jan','Feb','Mär','Apr','Mai','Jun','Jul','Aug','Sep','Okt','Nov','Dez'],
     now:'jetzt',max:'Max',days:'Tage',months:'Monate',years:'Jahre',
     nightMode:'Nachtmodus',connected:'Verbunden',noConnShort:'Keine Verbindung',portalOk:'Portal OK',portalDown:'Portal nicht erreichbar',
     filesHint:'Alles auf der SD-Karte außer dem Ordner SolarHistory (Solardaten, geschützt). Änderungen an Einstellungsdateien wirken erst nach einem Neustart des Geräts.',
     filesUp:'.. (nach oben)',filesMore:'Weitere laden',filesDelete:'Löschen',filesDeleteConfirm:'Diese Datei wirklich löschen?',
     filesDeleteSettingsWarn:'Diese Datei gehört zu den Geräte-Einstellungen - nach dem Löschen muss das Gerät ggf. neu eingerichtet werden. Wirklich löschen?',
     filesSave:'Speichern',filesSaved:'Gespeichert',filesSaveFailed:'Speichern fehlgeschlagen',filesTruncated:'Datei ist größer als 16 KB - nur der Anfang wird angezeigt (nicht bearbeitbar).',
     filesBinary:'Keine Textvorschau für diesen Dateityp.',filesEmpty:'Ordner ist leer.',
     filesDownload:'Herunterladen',filesDownloadFolder:'Ordner herunterladen',
     filesProtectedBadge:'geschützt: nur ansehen und herunterladen',
     filesBackup:'Backup erstellen',
     filesBackupHint:'Das Backup enthält auch Einstellungen und WLAN-Daten - bitte nur auf dem eigenen Rechner speichern.',
     filesBigDownloadWarn:'Das ist mehr als 20 MB - das Gerät kann während des Downloads träge reagieren. Trotzdem fortfahren?',
     filesProgressFmt:'Lade Datei {i} von {n}: {name}',filesBackupDone:'Backup wurde heruntergeladen (liegt im Download-Ordner, je nach Browser wirst du nach dem Speicherort gefragt).',
     filesDownloadFailed:'Download fehlgeschlagen.',
     maxVoltage:'Max. Spannung',minVoltage:'Min. Spannung',maxToday:'Max heute',
     pvStatus:'PV-Status',portalAgeFmt:'Portaldaten {m} Min alt',
     houseAltDay:'Schematische Tagesansicht von Haus, Garage und Auto mit dem Weg der Solarenergie',
     houseAltNight:'Schematische Nachtansicht von Haus, Garage und Auto mit dem Weg der Solarenergie'},
 en:{tabs:['Overview','Power','Daily curve','Yield','Battery','PV status','History','Weather','Data'],
     power:'Power',today:'Today',battery:'Battery',week:'Week',month:'Month',year:'Year',
     voltage:'Voltage',current:'Current',temp:'Temperature',status:'Status',weather:'Weather',
     stand:'As of',next:'Next update',noLoc:'No location set',noConn:'Lost connection to device',
     wd:['Sun','Mon','Tue','Wed','Thu','Fri','Sat'],
     mo:['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'],
     now:'now',max:'Max',days:'Days',months:'Months',years:'Years',
     nightMode:'Night mode',connected:'Connected',noConnShort:'No connection',portalOk:'Portal OK',portalDown:'Portal unreachable',
     filesHint:'Everything on the SD card except the SolarHistory folder (solar data, protected). Changes to settings files only take effect after restarting the device.',
     filesUp:'.. (up)',filesMore:'Load more',filesDelete:'Delete',filesDeleteConfirm:'Really delete this file?',
     filesDeleteSettingsWarn:'This file belongs to the device settings - after deleting it, the device may need to be set up again. Really delete?',
     filesSave:'Save',filesSaved:'Saved',filesSaveFailed:'Save failed',filesTruncated:'File is larger than 16 KB - only the start is shown (not editable).',
     filesBinary:'No text preview for this file type.',filesEmpty:'Folder is empty.',
     filesDownload:'Download',filesDownloadFolder:'Download folder',
     filesProtectedBadge:'protected: view and download only',
     filesBackup:'Create backup',
     filesBackupHint:'The backup also contains settings and WiFi credentials - please only save it on your own computer.',
     filesBigDownloadWarn:'That is more than 20 MB - the device may respond slowly during the download. Continue anyway?',
     filesProgressFmt:'Loading file {i} of {n}: {name}',filesBackupDone:'Backup downloaded (check your downloads folder - depending on your browser, you may be asked where to save it).',
     filesDownloadFailed:'Download failed.',
     maxVoltage:'Max. voltage',minVoltage:'Min. voltage',maxToday:'Max today',
     pvStatus:'PV status',portalAgeFmt:'Portal data {m} min old',
     houseAltDay:'Schematic daytime illustration of house, garage and car showing the path of the solar energy',
     houseAltNight:'Schematic nighttime illustration of house, garage and car showing the path of the solar energy'}
};
var lang='de', t=L.de;
var last={};
var tabsEl=document.getElementById('tabs');
var tabIds=['tab-overview','tab-power','tab-daily','tab-yield','tab-battery','tab-pvstatus','tab-history','tab-weather','tab-files'];
// Feste, sprachunabhaengige Indizes ueber tabIds statt Zahlen-Literale
// (Bugfix "Daten-Reiter beim ersten Klick leer"): die Zahlen-Literale
// unten in showTab() waren auf den Stand VOR dem Einfuegen von
// "PV-Status" (zwischen Batterie und Verlauf) eingefroren und zeigten
// seitdem auf die FALSCHEN Tabs (Verlauf statt PV-Status, Wetter statt
// Daten) - der Daten-Reiter rief filesInit() dadurch nie beim eigenen
// Klick auf, nur zufaellig vorab beim Besuch des (falschen) Wetter-Tabs,
// dessen Inhalt dann beim naechsten Klick auf Daten schon im Hintergrund
// geladen war. ueber indexOf() bleibt das robust, auch wenn Tabs kuenftig
// erneut umsortiert werden.
var TAB_POWER=tabIds.indexOf('tab-power');
var TAB_DAILY=tabIds.indexOf('tab-daily');
var TAB_HISTORY=tabIds.indexOf('tab-history');
var TAB_WEATHER=tabIds.indexOf('tab-weather');
var TAB_FILES=tabIds.indexOf('tab-files');
var activeTab=0;
var lastHist=null;
var lastDay=null;
var histPeriod=0; // 0=Tage 1=Monate 2=Jahre
var lastHistData=null;

function buildTabs(){
  tabsEl.innerHTML='';
  t.tabs.forEach(function(name,i){
    var b=document.createElement('button');
    b.textContent=name;
    if(i===activeTab) b.className='active';
    b.onclick=function(){ activeTab=i; showTab(); };
    tabsEl.appendChild(b);
  });
}
function showTab(){
  tabIds.forEach(function(id,i){
    document.getElementById(id).className = (i===activeTab)?'active':'';
  });
  Array.prototype.forEach.call(tabsEl.children, function(b,i){
    b.className = (i===activeTab)?'active':'';
  });
  // Das Canvas hat waehrend display:none eine Breite/Hoehe von 0 - ein
  // Poll, der in dieser Zeit lief, konnte also nicht zeichnen. Beim
  // Umschalten auf den jeweiligen Diagramm-Tab deshalb mit den zuletzt
  // bekannten Werten sofort nachzeichnen, statt auf den naechsten Poll
  // zu warten. Tage/Tagesverlauf/Verlauf-Tabs holen ihre Daten erst bei
  // Aktivierung (eigene, kleine Abrufe statt Dauerlast, siehe Auftrag).
  if (activeTab===TAB_POWER) drawChart(lastHist);
  if (activeTab===TAB_DAILY) pollDay();
  // Bug behoben: der Tage/Monate/Jahre-Unterreiter (histSubnav) wurde nie
  // gebaut, nur beim Klick auf einen seiner (nie existierenden) Knoepfe -
  // Monate/Jahre waren dadurch unerreichbar, nur die Standardansicht
  // "Tage" zeigte ueberhaupt Daten.
  if (activeTab===TAB_HISTORY) { buildHistSubnav(); pollHistory(); }
  if (activeTab===TAB_FILES) filesInit();
  // Haus-Grafik (Auftrag "Animierte Haus-Grafik"): auf allen Reitern ausser
  // Wetter und Daten sichtbar, dort wuerde sie mit dem Inhalt kollidieren.
  document.getElementById('houseWrap').className =
      (activeTab===TAB_WEATHER || activeTab===TAB_FILES) ? 'houseWrap hidden' : 'houseWrap';
}

// ---- Reiter "Daten" (Datei-Explorer, Auftrag "WebUI-Reiter Daten") ----
var filesCurrentPath='/';
var filesCurrentPage=0;
var filesHintSet=false;
function filesIsSettingsPath(p){ return p.indexOf('/SolarGuardian')===0; }
function filesFmtSize(n){
  if (n<1024) return n+' B';
  if (n<1024*1024) return (n/1024).toFixed(1)+' KB';
  return (n/1024/1024).toFixed(1)+' MB';
}
function filesInit(){
  if (!filesHintSet) {
    filesHintSet=true;
    setText('filesHint', t.filesHint);
    setText('filesBackupHint', t.filesBackupHint);
    var bBtn=document.getElementById('filesBackupBtn');
    bBtn.textContent=t.filesBackup;
    bBtn.onclick=filesCreateBackup;
  }
  filesLoad('/', 0);
}
function filesLoad(path, page){
  filesCurrentPath=path; filesCurrentPage=page;
  fetch('/api/files/list?path='+encodeURIComponent(path)+'&page='+page)
    .then(function(r){ return r.json(); })
    .then(function(d){ filesRender(d, page>0); })
    .catch(function(){});
}
function filesRender(d, append){
  setText('filesPath', d.path);
  var list=document.getElementById('filesList');
  if (!append) list.innerHTML='';
  if (!append && d.path!=='/') {
    var up=document.createElement('div'); up.className='filesRow';
    var parent = d.path.substring(0, d.path.lastIndexOf('/')) || '/';
    up.innerHTML='<span class="fname dir">'+t.filesUp+'</span>';
    up.onclick=function(){ filesLoad(parent, 0); };
    list.appendChild(up);
  }
  d.entries.forEach(function(e){
    var row=document.createElement('div'); row.className='filesRow';
    var full = (d.path==='/'?'':d.path) + '/' + e.name;
    if (e.locked) { var badge=document.createElement('span'); badge.className='filesLockBadge'; badge.textContent='🔒'; badge.title=t.filesProtectedBadge; row.appendChild(badge); }
    var nameEl=document.createElement('span'); nameEl.className='fname'+(e.dir?' dir':''); nameEl.textContent=e.name;
    row.appendChild(nameEl);
    if (!e.dir) {
      var sizeEl=document.createElement('span'); sizeEl.className='fsize'; sizeEl.textContent=filesFmtSize(e.size);
      row.appendChild(sizeEl);
      var dlBtn=document.createElement('button'); dlBtn.className='dl'; dlBtn.textContent=t.filesDownload;
      dlBtn.onclick=function(ev){ ev.stopPropagation(); window.location='/api/files/download?path='+encodeURIComponent(full); };
      row.appendChild(dlBtn);
      if (!e.locked) {
        var delBtn=document.createElement('button'); delBtn.textContent=t.filesDelete;
        delBtn.onclick=function(ev){ ev.stopPropagation(); filesDelete(full); };
        row.appendChild(delBtn);
      }
      nameEl.onclick=function(){ filesOpen(full, e.locked); };
    } else {
      var dlfBtn=document.createElement('button'); dlfBtn.className='dlFolder'; dlfBtn.textContent=t.filesDownloadFolder;
      dlfBtn.onclick=function(ev){ ev.stopPropagation(); filesDownloadFolder(full, e.name); };
      row.appendChild(dlfBtn);
      nameEl.onclick=function(){ filesLoad(full, 0); };
    }
    list.appendChild(row);
  });
  var moreBtn=document.getElementById('filesMoreBtn');
  if (d.hasMore) { moreBtn.style.display='block'; moreBtn.textContent=t.filesMore; moreBtn.onclick=function(){ filesLoad(d.path, filesCurrentPage+1); }; }
  else moreBtn.style.display='none';
}
function filesOpen(path, locked){
  fetch('/api/files/read?path='+encodeURIComponent(path)).then(function(r){
    if (!r.ok) return null;
    return r.json();
  }).then(function(d){
    if (!d) return;
    document.getElementById('filesViewer').style.display='block';
    document.getElementById('filesViewerName').textContent=path+(locked?' 🔒':'');
    var editor=document.getElementById('filesEditor');
    var truncHint=document.getElementById('filesTruncHint');
    var saveBtn=document.getElementById('filesSaveBtn');
    saveBtn.textContent=t.filesSave;
    if (d.truncated || locked) {
      truncHint.style.display='block'; truncHint.textContent = locked ? t.filesProtectedBadge : t.filesTruncated;
      editor.value=d.content; editor.readOnly=true; saveBtn.style.display='none';
    } else {
      truncHint.style.display='none';
      editor.value=d.content; editor.readOnly=false; saveBtn.style.display='inline-block';
      saveBtn.onclick=function(){ filesSave(path, editor.value); };
    }
  }).catch(function(){});
}
function filesSave(path, content){
  fetch('/api/files/save', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({path:path, content:content})})
    .then(function(r){ return r.json().then(function(j){ return {ok:r.ok, j:j}; }); })
    .then(function(res){
      if (res.ok && res.j.ok) { alert(t.filesSaved); }
      else { alert(t.filesSaveFailed); }
    }).catch(function(){ alert(t.filesSaveFailed); });
}
function filesDelete(path){
  var warn = filesIsSettingsPath(path) ? t.filesDeleteSettingsWarn : t.filesDeleteConfirm;
  if (!confirm(warn)) return;
  fetch('/api/files/delete', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({path:path})})
    .then(function(){ filesLoad(filesCurrentPath, 0); document.getElementById('filesViewer').style.display='none'; })
    .catch(function(){});
}
document.getElementById('filesCloseBtn').onclick=function(){ document.getElementById('filesViewer').style.display='none'; };

// ---- ZIP ohne externe Bibliothek (Modus "gespeichert"/stored, mit
// CRC32) - Auftrag "Ordner/Backup herunterladen". ----
var crc32Table=null;
function crc32(buf){
  if(!crc32Table){ crc32Table=new Uint32Array(256); for(var n=0;n<256;n++){ var c=n; for(var k=0;k<8;k++){ c=(c&1)?(0xEDB88320^(c>>>1)):(c>>>1); } crc32Table[n]=c; } }
  var crc=0xFFFFFFFF;
  for(var i=0;i<buf.length;i++){ crc=crc32Table[(crc^buf[i])&0xFF]^(crc>>>8); }
  return (crc^0xFFFFFFFF)>>>0;
}
function dosDateTime(d){
  var dt=((d.getHours()<<11)|(d.getMinutes()<<5)|(d.getSeconds()>>1))&0xFFFF;
  var dd=(((d.getFullYear()-1980)<<9)|((d.getMonth()+1)<<5)|d.getDate())&0xFFFF;
  return {time:dt,date:dd};
}
function buildZip(files){
  var parts=[], central=[], offset=0, dt=dosDateTime(new Date());
  files.forEach(function(f){
    var nameBytes=new TextEncoder().encode(f.rel);
    var crc=crc32(f.data), size=f.data.length;
    var lh=new DataView(new ArrayBuffer(30));
    lh.setUint32(0,0x04034b50,true); lh.setUint16(4,20,true); lh.setUint16(6,0,true); lh.setUint16(8,0,true);
    lh.setUint16(10,dt.time,true); lh.setUint16(12,dt.date,true); lh.setUint32(14,crc,true);
    lh.setUint32(18,size,true); lh.setUint32(22,size,true); lh.setUint16(26,nameBytes.length,true); lh.setUint16(28,0,true);
    parts.push(new Uint8Array(lh.buffer)); parts.push(nameBytes); parts.push(f.data);
    central.push({nameBytes:nameBytes,crc:crc,size:size,offset:offset,dt:dt});
    offset+=30+nameBytes.length+size;
  });
  var centralStart=offset;
  central.forEach(function(c){
    var ch=new DataView(new ArrayBuffer(46));
    ch.setUint32(0,0x02014b50,true); ch.setUint16(4,20,true); ch.setUint16(6,20,true); ch.setUint16(8,0,true); ch.setUint16(10,0,true);
    ch.setUint16(12,c.dt.time,true); ch.setUint16(14,c.dt.date,true); ch.setUint32(16,c.crc,true);
    ch.setUint32(20,c.size,true); ch.setUint32(24,c.size,true); ch.setUint16(28,c.nameBytes.length,true);
    ch.setUint16(30,0,true); ch.setUint16(32,0,true); ch.setUint16(34,0,true); ch.setUint16(36,0,true);
    ch.setUint32(38,0,true); ch.setUint32(42,c.offset,true);
    parts.push(new Uint8Array(ch.buffer)); parts.push(c.nameBytes);
    offset+=46+c.nameBytes.length;
  });
  var centralSize=offset-centralStart;
  var eocd=new DataView(new ArrayBuffer(22));
  eocd.setUint32(0,0x06054b50,true); eocd.setUint16(8,central.length,true); eocd.setUint16(10,central.length,true);
  eocd.setUint32(12,centralSize,true); eocd.setUint32(16,centralStart,true);
  parts.push(new Uint8Array(eocd.buffer));
  return new Blob(parts,{type:'application/zip'});
}
function filesFetchBytes(path){
  return fetch('/api/files/download?path='+encodeURIComponent(path)).then(function(r){
    if(!r.ok) throw new Error('fetch failed');
    return r.arrayBuffer();
  }).then(function(buf){ return new Uint8Array(buf); });
}
function filesShowProgress(i,n,name){
  var el=document.getElementById('filesProgress');
  el.style.display='block';
  el.textContent=t.filesProgressFmt.replace('{i}',i).replace('{n}',n).replace('{name}',name);
}
function filesHideProgress(){ document.getElementById('filesProgress').style.display='none'; }
async function filesBuildZipFromList(fileEntries){
  var files=[];
  for(var i=0;i<fileEntries.length;i++){
    var e=fileEntries[i];
    filesShowProgress(i+1, fileEntries.length, e.rel+' ('+filesFmtSize(e.size)+')');
    var data=await filesFetchBytes(e.path);
    files.push({rel:e.rel, data:data});
    await new Promise(function(res){ setTimeout(res,80); }); // Geraet nicht ueberlasten, kurze Pause
  }
  return buildZip(files);
}
async function filesSaveBlob(blob, name){
  if (window.showSaveFilePicker && window.isSecureContext) {
    try {
      var handle = await window.showSaveFilePicker({suggestedName:name});
      var writable = await handle.createWritable();
      await writable.write(blob);
      await writable.close();
      return;
    } catch(e) { /* abgebrochen oder nicht unterstuetzt - normaler Download als Rueckfall */ }
  }
  var url=URL.createObjectURL(blob);
  var a=document.createElement('a'); a.href=url; a.download=name;
  document.body.appendChild(a); a.click(); document.body.removeChild(a);
  setTimeout(function(){ URL.revokeObjectURL(url); }, 2000);
}
async function filesDownloadFolder(path, name){
  try {
    var r = await fetch('/api/files/listtree?path='+encodeURIComponent(path));
    var d = await r.json();
    var files = d.files||[];
    var total = files.reduce(function(s,f){ return s+(f.size||0); },0);
    if (total > 20*1024*1024 && !confirm(t.filesBigDownloadWarn)) return;
    var zipFiles = files.map(function(f){ return {path:f.path, rel:f.rel, size:f.size}; });
    var blob = await filesBuildZipFromList(zipFiles);
    await filesSaveBlob(blob, name+'.zip');
  } catch(e) {
    alert(t.filesDownloadFailed);
  } finally {
    filesHideProgress();
  }
}
async function filesCreateBackup(){
  try {
    var rG = await fetch('/api/files/listtree?path=/SolarGuardian');
    var dG = await rG.json();
    var rH = await fetch('/api/files/listtree?path=/SolarHistory');
    var dH = await rH.json();
    var files = (dG.files||[]).map(function(f){ return {path:f.path, rel:'SolarGuardian/'+f.rel, size:f.size}; })
      .concat((dH.files||[]).map(function(f){ return {path:f.path, rel:'SolarHistory/'+f.rel, size:f.size}; }));
    var total = files.reduce(function(s,f){ return s+(f.size||0); },0);
    if (total > 20*1024*1024 && !confirm(t.filesBigDownloadWarn)) return;
    var blob = await filesBuildZipFromList(files);
    var now = new Date();
    var pad2=function(n){ return (n<10?'0':'')+n; };
    var name = 'SolarBackup_'+now.getFullYear()+'-'+pad2(now.getMonth()+1)+'-'+pad2(now.getDate())+'_'+pad2(now.getHours())+pad2(now.getMinutes())+'.zip';
    await filesSaveBlob(blob, name);
    alert(t.filesBackupDone);
  } catch(e) {
    alert(t.filesDownloadFailed);
  } finally {
    filesHideProgress();
  }
}

function setText(id,val){
  var el=document.getElementById(id);
  if(el.textContent!==val) el.textContent=val;
}
function fmt1(v){ return (typeof v==='number') ? v.toFixed(1) : '--'; }
function fmt2(v){ return (typeof v==='number') ? v.toFixed(2) : '--'; }
function fmt0(v){ return (typeof v==='number') ? Math.round(v).toString() : '--'; }
// Tagesertrag-Balken (Auftrag "WebUI erweitern"): immer zwei
// Nachkommastellen im deutschen Zahlenformat (Komma statt Punkt), z.B.
// "9,35" - unabhaengig von der aktuell angezeigten UI-Sprache (Alex'
// ausdruecklicher Wunsch). Monate/Jahre bleiben bei fmt0 (gerundet).
function fmtDay2(v){ return (typeof v==='number') ? v.toFixed(2).replace('.',',') : '--'; }

// ---- Wettersymbole als einfache, farbige SVGs (gleiche Farbwelt wie
// das Geraet: Wolken weiss, Sonne knallgelb, Regen blau, Schnee cyan,
// Gewitter dunkle Wolke+gelber Blitz, Nebel tuerkis/hellblau) ----
function cloudPath(fill){
  return '<path d="M20 40 a12 12 0 0 1 0 -24 a15 15 0 0 1 29 -3 a11 11 0 0 1 3 21 z" fill="'+fill+'" stroke="#fff" stroke-width="1.5"/>';
}
function sunSvg(){
  return '<svg viewBox="0 0 64 64"><g stroke="#FF9A00" stroke-width="3" stroke-linecap="round">'+
    '<line x1="32" y1="2" x2="32" y2="12"/><line x1="32" y1="52" x2="32" y2="62"/>'+
    '<line x1="2" y1="32" x2="12" y2="32"/><line x1="52" y1="32" x2="62" y2="32"/>'+
    '<line x1="10" y1="10" x2="17" y2="17"/><line x1="47" y1="47" x2="54" y2="54"/>'+
    '<line x1="54" y1="10" x2="47" y2="17"/><line x1="17" y1="47" x2="10" y2="54"/>'+
    '</g><circle cx="32" cy="32" r="16" fill="#FFE600" stroke="#fff" stroke-width="2"/></svg>';
}
function moonSvg(){
  return '<svg viewBox="0 0 64 64"><circle cx="30" cy="32" r="16" fill="#FFD84A" stroke="#fff" stroke-width="2"/>'+
    '<circle cx="38" cy="26" r="13" fill="#0a0a0a"/>'+
    '<circle cx="50" cy="14" r="2" fill="#fff"/><circle cx="56" cy="24" r="1.4" fill="#FFD84A"/></svg>';
}
function cloudsOnly(dark){
  var fill = dark ? '#C9D0D8' : '#FFFFFF';
  return '<svg viewBox="0 0 64 64">'+cloudPath(fill)+'</svg>';
}
function partlyCloudySvg(day){
  var sun = day ? '<circle cx="22" cy="20" r="10" fill="#FFE600" stroke="#fff" stroke-width="1.5"/>' :
                  '<circle cx="22" cy="20" r="10" fill="#FFD84A" stroke="#fff" stroke-width="1.5"/>';
  return '<svg viewBox="0 0 64 64">'+sun+cloudPath('#FFFFFF')+'</svg>';
}
function rainSvg(heavy){
  var drops = heavy ? [18,30,42] : [22,38];
  var d='';
  drops.forEach(function(x){ d+='<line x1="'+x+'" y1="40" x2="'+(x-3)+'" y2="52" stroke="#1E90FF" stroke-width="3" stroke-linecap="round"/>'; });
  return '<svg viewBox="0 0 64 64">'+cloudPath('#FFFFFF')+d+'</svg>';
}
function snowSvg(){
  var f='';
  [20,32,44].forEach(function(x,i){
    var c = i%2===0 ? '#81D4FA' : '#40C4FF';
    f+='<g stroke="'+c+'" stroke-width="2" stroke-linecap="round">'+
       '<line x1="'+(x-5)+'" y1="48" x2="'+(x+5)+'" y2="48"/>'+
       '<line x1="'+x+'" y1="43" x2="'+x+'" y2="53"/>'+
       '<line x1="'+(x-4)+'" y1="44" x2="'+(x+4)+'" y2="52"/>'+
       '<line x1="'+(x-4)+'" y1="52" x2="'+(x+4)+'" y2="44"/></g>';
  });
  return '<svg viewBox="0 0 64 64">'+cloudPath('#FFFFFF')+f+'</svg>';
}
function stormSvg(){
  return '<svg viewBox="0 0 64 64">'+cloudPath('#5B4B8A')+
    '<polygon points="30,38 38,38 28,52 34,52 24,64 28,50 22,50" fill="#FFE600" stroke="#FF9A00" stroke-width="1.5"/></svg>';
}
function fogSvg(){
  var l='';
  [24,32,40].forEach(function(y,i){
    var c = i%2===0 ? '#4DD0E1' : '#81D4FA';
    l+='<line x1="8" y1="'+y+'" x2="56" y2="'+y+'" stroke="'+c+'" stroke-width="3" stroke-linecap="round"/>';
  });
  return '<svg viewBox="0 0 64 64">'+l+'</svg>';
}
function weatherSvg(code,isDay){
  if(code===0) return isDay ? sunSvg() : moonSvg();
  if(code===1||code===2) return partlyCloudySvg(isDay);
  if(code===3) return cloudsOnly(true);
  if(code===45||code===48) return fogSvg();
  if((code>=51&&code<=57)) return rainSvg(false);
  if((code>=61&&code<=67)||(code>=80&&code<=82)) return rainSvg(true);
  if((code>=71&&code<=77)||code===85||code===86) return snowSvg();
  if(code>=95&&code<=99) return stormSvg();
  return cloudsOnly(false);
}

// ---- Leistungskurve (letzte 3 Stunden) ----
function drawChart(hist){
  var cv=document.getElementById('chart');
  var dpr=window.devicePixelRatio||1;
  var w=cv.clientWidth, h=cv.clientHeight;
  if(w<=0||h<=0) return;
  cv.width=w*dpr; cv.height=h*dpr;
  var ctx=cv.getContext('2d');
  ctx.setTransform(dpr,0,0,dpr,0,0);
  ctx.clearRect(0,0,w,h);
  var accent=getComputedStyle(document.documentElement).getPropertyValue('--accent').trim();
  if(!hist || hist.length<2){
    ctx.fillStyle='#8a8a8a'; ctx.font='13px sans-serif'; ctx.textAlign='center';
    ctx.fillText('--', w/2, h/2);
    return;
  }
  var pad={l:44,r:10,t:10,b:20};
  var cw=w-pad.l-pad.r, ch=h-pad.t-pad.b;
  var maxV=0; hist.forEach(function(p){ if(p.w>maxV) maxV=p.w; });
  var mag=Math.pow(10,Math.floor(Math.log10(Math.max(maxV,1))));
  var norm=maxV/mag;
  var niceMax=(norm<=1?1:norm<=2?2:norm<=5?5:10)*mag;
  if(niceMax<=0) niceMax=100;
  ctx.strokeStyle='#242424'; ctx.lineWidth=1;
  for(var g=1;g<=4;g++){
    var gy=pad.t+ch-(ch*g/4);
    ctx.beginPath(); ctx.moveTo(pad.l,gy); ctx.lineTo(pad.l+cw,gy); ctx.stroke();
  }
  ctx.fillStyle='#8a8a8a'; ctx.font='11px sans-serif'; ctx.textAlign='right';
  ctx.fillText(Math.round(niceMax)+' W', pad.l-4, pad.t+8);
  var now=hist[hist.length-1].t;
  var start=now-3*3600;
  ctx.strokeStyle=accent; ctx.lineWidth=2; ctx.beginPath();
  hist.forEach(function(p,i){
    var frac=(p.t-start)/(3*3600); if(frac<0)frac=0; if(frac>1)frac=1;
    var x=pad.l+frac*cw;
    var vfrac=Math.max(0,Math.min(1,p.w/niceMax));
    var y=pad.t+ch-vfrac*ch;
    if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
  });
  ctx.stroke();
  ctx.fillStyle='#8a8a8a'; ctx.textAlign='center'; ctx.font='10px sans-serif';
  ctx.fillText('-3h',pad.l,h-4);
  ctx.fillText('-1,5h',pad.l+cw/2,h-4);
  ctx.fillText(t.now,pad.l+cw,h-4);
}

// ---- Tagesverlauf (05:00-22:00 Uhr) ----
var DAY_START_MIN=5*60, DAY_END_MIN=22*60, DAY_SPAN_MIN=DAY_END_MIN-DAY_START_MIN;
function drawDailyChart(day){
  var cv=document.getElementById('dailyChart');
  var dpr=window.devicePixelRatio||1;
  var w=cv.clientWidth, h=cv.clientHeight;
  if(w<=0||h<=0) return;
  cv.width=w*dpr; cv.height=h*dpr;
  var ctx=cv.getContext('2d');
  ctx.setTransform(dpr,0,0,dpr,0,0);
  ctx.clearRect(0,0,w,h);
  var accent=getComputedStyle(document.documentElement).getPropertyValue('--accent').trim();
  var pts=(day&&day.pts)||[];
  if(pts.length<2){
    ctx.fillStyle='#8a8a8a'; ctx.font='13px sans-serif'; ctx.textAlign='center';
    ctx.fillText('--', w/2, h/2);
    return;
  }
  // Fortschrittsbalken (Auftrag "WebUI-Fortschrittsbalken Korrektur") -
  // liegt UNTER dem Diagramm UND unter der Stunden-Zeitachse, nie darueber
  // (frueherer Fehler: Balken lag zwischen Kurve und Achse, zu dick (14px)
  // und die Uhrzeit-Beschriftung ueber dem Balken ueberlappte die Kurve).
  // Reihenfolge von oben nach unten: Diagramm (ch) -> Stunden-Achse
  // (hourAxisH) -> kleiner Abstand -> Balken (pbH, jetzt 6px) -> kleiner
  // Abstand -> Uhrzeit-Beschriftung - alles fest budgetiert, damit es in
  // die gleichbleibende Canvas-Hoehe passt.
  var hourAxisH=14, pbGap1=3, pbH=6, pbGap2=2, timeLabelH=10, bottomMargin=2;
  var reserved = hourAxisH+pbGap1+pbH+pbGap2+timeLabelH+bottomMargin;
  var pad={l:44,r:10,t:10};
  var cw=w-pad.l-pad.r, ch=h-pad.t-reserved;
  var maxV=(typeof day.max==='number')?day.max:0;
  var mag=Math.pow(10,Math.floor(Math.log10(Math.max(maxV,1))));
  var norm=maxV/mag;
  var niceMax=(norm<=1?1:norm<=2?2:norm<=5?5:10)*mag;
  if(niceMax<=0) niceMax=100;
  // Raster (Hilfslinien) in der gedaempften Sekundaerfarbe, wie am Geraet.
  ctx.strokeStyle='#242424'; ctx.lineWidth=1;
  for(var g=1;g<=4;g++){
    var gy=pad.t+ch-(ch*g/4);
    ctx.beginPath(); ctx.moveTo(pad.l,gy); ctx.lineTo(pad.l+cw,gy); ctx.stroke();
  }
  ctx.fillStyle='#8a8a8a'; ctx.font='11px sans-serif'; ctx.textAlign='right';
  ctx.fillText(Math.round(niceMax)+' W', pad.l-4, pad.t+8);
  // Kurve (abgefragter Wert) in der Akzentfarbe, wie am Geraet.
  ctx.strokeStyle=accent; ctx.lineWidth=2; ctx.beginPath();
  pts.forEach(function(p,i){
    var frac=(p[0]-DAY_START_MIN)/DAY_SPAN_MIN; if(frac<0)frac=0; if(frac>1)frac=1;
    var x=pad.l+frac*cw;
    var vfrac=Math.max(0,Math.min(1,p[1]/niceMax));
    var y=pad.t+ch-vfrac*ch;
    if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
  });
  ctx.stroke();

  var chartBottom = pad.t+ch;

  // Zeitachse: jede volle Stunde 05-22 (Auftrag, vorher nur 5/9/13/17/22) -
  // bei wenig Platz (schmaler Bildschirm) kleinere Schrift, und falls auch
  // DAMIT noch nicht alle 18 Stunden ohne Ueberlappung passen, nur jede
  // zweite anzeigen (22 Uhr immer zusaetzlich, als letzter Fixpunkt).
  // Liegt direkt unter dem Diagramm, der Balken kommt erst DANACH.
  var hours=[]; for(var hh=5; hh<=22; hh++) hours.push(hh);
  var fontPx = (cw/hours.length<16) ? 8 : 10;
  ctx.font=fontPx+'px sans-serif';
  var neededPerLabel = ctx.measureText('22').width+6;
  var showEvery = (cw/hours.length<neededPerLabel) ? 2 : 1;
  var axisBaselineY = chartBottom+hourAxisH-3;
  ctx.fillStyle='#8a8a8a'; ctx.textAlign='center';
  hours.forEach(function(hh,idx){
    if(idx%showEvery!==0 && hh!==22) return;
    var frac=(hh*60-DAY_START_MIN)/DAY_SPAN_MIN;
    ctx.fillText(String(hh).padStart(2,'0'),pad.l+frac*cw,axisBaselineY);
  });

  // Fortschrittsbalken (Auftrag): faellt von links nach rechts ueber
  // denselben Zeitbereich/dieselbe Breite wie das Diagramm (05:00-22:00
  // Uhr, pad.l..pad.l+cw) - voll ab 22:00, leer vor 05:00. Liegt komplett
  // UNTER der Stunden-Zeitachse, mit kleinem Abstand.
  var barY = chartBottom+hourAxisH+pbGap1;
  var now=new Date();
  var nowMin=now.getHours()*60+now.getMinutes();
  var pfrac=(nowMin<=DAY_START_MIN)?0:(nowMin>=DAY_END_MIN?1:(nowMin-DAY_START_MIN)/DAY_SPAN_MIN);
  var dimColor='#8a8a8a';
  ctx.fillStyle=dimColor; ctx.globalAlpha=0.3; ctx.fillRect(pad.l,barY,cw,pbH); ctx.globalAlpha=1;
  ctx.fillStyle=accent; ctx.fillRect(pad.l,barY,Math.max(0,cw*pfrac),pbH);
  // Kleine Stundenstriche ohne Beschriftung (Auftrag), ueber dem Balken.
  ctx.strokeStyle=dimColor; ctx.lineWidth=1;
  hours.forEach(function(hh){
    var frac=(hh*60-DAY_START_MIN)/DAY_SPAN_MIN;
    var tx=pad.l+frac*cw;
    ctx.beginPath(); ctx.moveTo(tx,barY); ctx.lineTo(tx,barY+2); ctx.stroke();
  });

  // Aktuelle Uhrzeit als kleine Beschriftung DIREKT UNTER dem Balken, in
  // der Sekundaerfarbe, seitlich an den Raendern geklemmt, damit sie nie
  // ueber das Diagramm hinausragt oder abgeschnitten wird.
  var hhNow=String(now.getHours()).padStart(2,'0'), mmNow=String(now.getMinutes()).padStart(2,'0');
  var fillX=pad.l+cw*pfrac;
  var timeBaselineY = barY+pbH+pbGap2+timeLabelH-2;
  ctx.fillStyle=dimColor; ctx.font='9px sans-serif';
  ctx.textAlign=(pfrac>0.88?'right':(pfrac<0.12?'left':'center'));
  ctx.fillText(hhNow+':'+mmNow, Math.min(Math.max(fillX,pad.l+12),pad.l+cw-12), timeBaselineY);
  ctx.textAlign='center';
}

function pollDay(){
  fetch('/api/day').then(function(r){return r.json();}).then(function(d){
    lastDay=d;
    setText('dMax', (typeof d.max==='number')?fmt0(d.max):'--');
    drawDailyChart(d);
  }).catch(function(){});
}

// ---- Tage/Monate/Jahre (Reiter wie am Geraet) ----
function buildHistSubnav(){
  var el=document.getElementById('histSubnav');
  el.innerHTML='';
  [t.days,t.months,t.years].forEach(function(name,i){
    var b=document.createElement('button');
    b.textContent=name;
    if(i===histPeriod) b.className='active';
    b.onclick=function(){ histPeriod=i; buildHistSubnav(); renderHistory(lastHistData); };
    el.appendChild(b);
  });
}
function histLabel(entry){
  if(histPeriod===0){ // Tage: Datum -> Wochentag
    var d=new Date(entry.d+'T12:00:00');
    return isNaN(d.getTime()) ? '' : t.wd[d.getDay()];
  }
  if(histPeriod===1){ // Monate: "YYYY-MM" -> Monatskuerzel
    var mm=parseInt(entry.m.substring(5,7),10);
    return (mm>=1&&mm<=12) ? t.mo[mm-1] : '';
  }
  return String(entry.y); // Jahre
}
function renderHistory(data){
  if(!data) return;
  var arr=(histPeriod===0?data.days:histPeriod===1?data.months:data.years)||[];
  var hc=document.getElementById('histChart');
  hc.innerHTML='';
  var maxKwh=1; arr.forEach(function(e){ if(e.kwh>maxKwh) maxKwh=e.kwh; });
  arr.forEach(function(e){
    var bar=document.createElement('div'); bar.className='bar'+(e.cur?' cur':'');
    var vEl=document.createElement('div'); vEl.className='v'; vEl.textContent=(histPeriod===0?fmtDay2(e.kwh):fmt0(e.kwh));
    var track=document.createElement('div'); track.className='track';
    var fill=document.createElement('div'); fill.className='fill';
    fill.style.height=Math.max(4,(e.kwh/maxKwh)*100)+'%';
    track.appendChild(fill);
    var yEl=document.createElement('div'); yEl.className='y'; yEl.textContent=histLabel(e);
    bar.appendChild(vEl); bar.appendChild(track); bar.appendChild(yEl);
    hc.appendChild(bar);
  });
}
function pollHistory(){
  fetch('/api/history').then(function(r){return r.json();}).then(function(d){
    lastHistData=d;
    renderHistory(d);
  }).catch(function(){});
}

// ---- Animierte Haus-Grafik (Auftrag "Animierte Haus-Grafik in der WebUI",
// Ueberarbeitungen "Tag/Nacht nach Sonnenstand" und "zwei Bild-Varianten")
// ----
// Aussehen/Wege/Punktgroesse/Glow/Ueberblendung/Geschwindigkeiten 1:1 aus
// der bestaetigten Vorlage solarenergy.html
// uebernommen (viewBox 1408x768, Punktradius 5, Farbe #7df3ff, Glow-
// Filter, je EIN Punkt pro Weg, Daempfungsfaktor dt*3, Nacht-Tempo 14px/s
// konstant, Tag-Tempo-Formel wie bisher). Datenquelle fuer den Tag-Modus
// ist weiterhin der ohnehin abgerufene PV-Leistungswert, fuer den Tag/
// Nacht-Wechsel data.sunDay (echter Sonnenstand - NICHT dasselbe wie
// data.night, das ist der feste Abfrage-Ruhefenster-Takt). Welche der
// zwei neutral benannten Bild-Varianten ("v1"/"v2") angezeigt wird, kommt
// aus data.variant (1 oder 2) - rein technisch, kein Netzwerk-Mehrverkehr.
var HOUSE_MAX_W=2000; // deckelt die Tag-Tempo-Formel, siehe Vorlage
var HOUSE_STALE_MIN=30; // gleicher Schwellwert wie config.h EPEVER_STALE_AFTER_MS
var HOUSE_NIGHT_SPEED=14; // Pixel/s, siehe Vorlage
// Kabel-Schlauch (Auftrag "Rahmen/neue Bilder/Schlauch-Kabel", Werte in
// SVG-viewBox-Einheiten, 1408 breit) - am Bildschirm geprueft (siehe
// CLAUDE.md), ca. 1,5-2x so dick wie die fruehere, in den Fotos
// eingebackene Leuchtlinie, ohne klobig zu wirken.
var HOUSE_TUBE_W=5;
var HOUSE_TUBE_COLOR='#7df3ff';
var HOUSE_TUBE_ON=0.85;  // Stroemung fliesst: Schlauch voll leuchtend
var HOUSE_TUBE_DIM=0.32; // kein Stromfluss: sichtbar, aber gedaempft
// Lauflicht statt Kugel (Auftrag "Lauflicht statt Kugel") - kein
// <circle> mehr, sondern ein zweiter Stroke auf einer Pfad-Kopie mit
// stroke-dasharray/-dashoffset: ein kurzes Segment (SEG_FRAC der
// Pfadlaenge, mind. SEG_MIN_PX viewBox-Einheiten) wandert im Schlauch.
// Farbe dunkler/blauer als der helle Schlauch (Auftragsvorgabe), Breite
// knapp schmaler als der Schlauch, damit sie ihn nie ueberragt.
var HOUSE_RUNNER_COLOR='#1f6fff';
var HOUSE_RUNNER_W=HOUSE_TUBE_W-1;
var HOUSE_RUNNER_SEG_FRAC=0.05; // 5% der Pfadlaenge (Vorgabe: 4-6%)
var HOUSE_RUNNER_SEG_MIN=20;    // viewBox-Einheiten, Untergrenze
// Weiches Ausklingen nach hinten: zwei zusaetzliche, schwaechere Kopien
// kurz hinter dem Hauptsegment (einfacher, robuster "Comet-Schweif"
// statt eines Farbverlaufs entlang beliebig gekruemmter Pfade, der sich
// mit reinen SVG-Gradients nicht sauber pfadfolgend umsetzen laesst).
var HOUSE_RUNNER_TAILS=[
  {offsetFrac:0.55, opacity:0.45},
  {offsetFrac:1.10, opacity:0.18}
];
var houseReduceMotion = !!(window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches);
// Reine Vorschau zum Testen (Auftrag "Vorschau zum Testen") - erzwingt
// ueber einen URL-Zusatz Modus/Variante/Leistungswert, rein clientseitig,
// schreibt/aendert nichts auf dem Geraet, bleibt unbeworben (nicht in
// README/Release-Notes erwaehnt). Ohne Zusatz gilt die normale Logik.
var houseForce = (function(){
  var qp = new URLSearchParams(window.location.search);
  var g = qp.get('grafik');
  var w = qp.get('watt');
  var v = qp.get('variante');
  return {
    mode: (g==='tag') ? 'day' : (g==='nacht' ? 'night' : null),
    watt: (w!==null && !isNaN(+w)) ? +w : null,
    variant: (v==='1') ? 'v1' : (v==='2' ? 'v2' : null)
  };
})();
// Vier Kombinationen (Variante x Tag/Nacht) - je drei Wege, ein Punkt pro
// Weg, eigene Bild-Route/eigenes <img>-Element. Tag und Nacht teilen
// sich seit den neuen Hausbildern (Auftrag "Rahmen/neue Bilder/
// Schlauch-Kabel") dieselben Pfade - die Bilder wurden gezielt auf
// identische Geometrie zum jeweiligen Nachtbild angepasst (mit dem
// Debug-Overlay gegengeprueft, siehe CLAUDE.md), die frueheren eigenen
// Tag-Pfade hda/hdb/hdc/hya/hyb/hyc sind dadurch ueberfluessig und
// wurden entfernt (auch aus dem SVG unten).
var houseSets={
  v1_night:['hna','hnb','hnc'], v1_day:['hna','hnb','hnc'],
  v2_night:['hxa','hxb','hxc'], v2_day:['hxa','hxb','hxc']
};
var houseImgIds={
  v1_day:'houseImgV1Day', v1_night:'houseImgV1Night',
  v2_day:'houseImgV2Day', v2_night:'houseImgV2Night'
};
var houseRoutes={
  v1_day:'/house_v1_day_v4.jpg', v1_night:'/house_v1_night_v4.jpg',
  v2_day:'/house_v2_day_v4.jpg', v2_night:'/house_v2_night_v4.jpg'
};
var houseAlpha={}, houseLast=null, houseWatts=0;
Object.keys(houseSets).forEach(function(k){ houseAlpha[k]=0; });
var houseImgLoaded=false, houseCurKey=null, houseSrcLoaded={v1_day:false,v1_night:false,v2_day:false,v2_night:false};
// Eindeutige Pfad-IDs ueber alle houseSets hinweg - Tag und Nacht
// teilen sich jetzt dieselbe ID (siehe houseSets oben), deshalb EINE
// Zustandsstruktur pro tatsaechlicher <path>, nicht mehr eine pro
// (Kombination,Pfad)-Paar wie vorher bei den Punkten.
var housePathIds=Array.from(new Set([].concat.apply([],Object.keys(houseSets).map(function(k){return houseSets[k];}))));
var houseP={}; // id -> {el,len,segLen,t,runAlpha,head,tails}
function houseInit(){
  var NS='http://www.w3.org/2000/svg';
  var runnerLayer=document.getElementById('houseRunner');
  housePathIds.forEach(function(id){
    var p=document.getElementById(id);
    var len=p.getTotalLength();
    // Segmentlaenge 4-6% der Pfadlaenge (hier 5%), mind. HOUSE_RUNNER_SEG_MIN
    // viewBox-Einheiten (Auftragsvorgabe).
    var segLen=Math.max(HOUSE_RUNNER_SEG_MIN, len*HOUSE_RUNNER_SEG_FRAC);
    var dash=segLen.toFixed(1)+','+Math.max(0,len-segLen).toFixed(1);
    function makeRunnerPath(){
      var rp=document.createElementNS(NS,'path');
      rp.setAttribute('d',p.getAttribute('d'));
      rp.setAttribute('stroke',HOUSE_RUNNER_COLOR);
      rp.setAttribute('stroke-width',HOUSE_RUNNER_W);
      rp.setAttribute('stroke-dasharray',dash);
      rp.setAttribute('stroke-opacity','0');
      runnerLayer.appendChild(rp);
      return rp;
    }
    houseP[id]={
      el:p, len:len, segLen:segLen, t:0, runAlpha:0,
      head:makeRunnerPath(),
      tails:HOUSE_RUNNER_TAILS.map(makeRunnerPath)
    };
  });
  requestAnimationFrame(houseFrame);
}
function houseFrame(now){
  // Pausiert effektiv, solange der Tab nicht sichtbar ist (rAF wird vom
  // Browser dann ohnehin stark gedrosselt) - keine Bewegung, aber die
  // Schleife bleibt am Leben, damit sie beim Zurueckkehren sofort
  // weiterlaeuft, statt neu angestossen werden zu muessen.
  if (document.hidden) { houseLast=null; requestAnimationFrame(houseFrame); return; }
  if (!houseLast) houseLast=now;
  var dt=Math.min(0.1,(now-houseLast)/1000);
  houseLast=now;
  // Kabel als Schlauch (Auftrag "Rahmen/neue Bilder/Schlauch-Kabel"):
  // Tag und Nacht derselben Variante teilen sich jetzt dieselbe Pfad-ID
  // (siehe houseSets oben). Alpha/"on" werden deshalb zuerst PRO
  // KOMBINATION berechnet (houseAlpha[k], unveraendert), dann je
  // Pfad-ID auf den groessten Wert ueber alle k zusammengefuehrt, die
  // diese ID nutzen - erst danach wird einmal pro tatsaechlichem Pfad
  // gezeichnet (Schlauch UND Lauflicht), nicht mehr pro k.
  var pathState = {};
  Object.keys(houseSets).forEach(function(k){
    var isNight = k.indexOf('night')>=0;
    var on = (k===houseCurKey) && (isNight || houseWatts>=5) ? 1 : 0;
    houseAlpha[k] += (on-houseAlpha[k])*Math.min(1,dt*3);
    var speed = isNight ? HOUSE_NIGHT_SPEED : 30+170*Math.min(houseWatts,HOUSE_MAX_W)/HOUSE_MAX_W;
    houseSets[k].forEach(function(id){
      var st = pathState[id] || (pathState[id] = {alpha:0, on:0, speed:speed});
      st.alpha = Math.max(st.alpha, houseAlpha[k]);
      if (on) { st.on = 1; st.speed = speed; }
    });
  });
  Object.keys(houseP).forEach(function(id){
    var P = houseP[id];
    var st = pathState[id] || {alpha:0, on:0, speed:HOUSE_NIGHT_SPEED};

    // Schlauch (Basis-Stroke) - bleibt bei fehlendem Stromfluss
    // sichtbar, aber gedaempft (HOUSE_TUBE_DIM), bei Stromfluss voll
    // leuchtend (HOUSE_TUBE_ON).
    var tubeIntensity = st.on ? HOUSE_TUBE_ON : HOUSE_TUBE_DIM;
    P.el.setAttribute('stroke', HOUSE_TUBE_COLOR);
    P.el.setAttribute('stroke-width', HOUSE_TUBE_W);
    P.el.setAttribute('stroke-opacity', (st.alpha*tubeIntensity).toFixed(2));

    // Position nur fortschreiben, wenn Strom fliesst - steht sonst
    // still (wie zuvor der Punkt).
    P.t += dt*st.speed/P.len*(st.on?1:0);
    if (P.t>1) P.t -= 1;

    // Lauflicht sanft ein-/ausblenden (1,2s, Auftrag "Lauflicht statt
    // Kugel") - anders als zuvor beim Punkt (der immer sichtbar blieb)
    // verschwindet es jetzt bei fehlendem Stromfluss komplett, der
    // Schlauch bleibt als einziges Signal "gedaempft" sichtbar.
    P.runAlpha += ((st.on?1:0) - P.runAlpha) * Math.min(1, dt/1.2);
    var runOpacity = st.alpha * P.runAlpha;

    var headOffset = -(P.t*P.len);
    P.head.setAttribute('stroke-dashoffset', headOffset.toFixed(1));
    P.head.setAttribute('stroke-opacity', runOpacity.toFixed(2));
    HOUSE_RUNNER_TAILS.forEach(function(cfg, i){
      var tp = P.tails[i];
      // Kleinerer (weniger negativer) Offset = etwas frueherer Punkt
      // auf dem Pfad = "hinter" dem Kopf in Laufrichtung.
      tp.setAttribute('stroke-dashoffset', (headOffset + cfg.offsetFrac*P.segLen).toFixed(1));
      tp.setAttribute('stroke-opacity', (runOpacity*cfg.opacity).toFixed(2));
    });
  });
  requestAnimationFrame(houseFrame);
}
// Laedt das Bild einer Variante/Modus-Kombination genau einmal (idempotent) -
// NIE mehr als die zwei Bilder der gerade aktiven Variante.
function houseLoadImage(key){
  if (houseSrcLoaded[key]) return;
  houseSrcLoaded[key]=true;
  document.getElementById(houseImgIds[key]).src = houseRoutes[key];
}
// Blendet per CSS-Transition (1,2s, siehe .houseHero img) auf die neue
// Variante/Modus-Kombination um und stoesst bei Bedarf deren Bild-
// Ladevorgang an.
function houseSetCur(key){
  houseLoadImage(key);
  if (houseCurKey===key) return;
  houseCurKey=key;
  Object.keys(houseImgIds).forEach(function(k){
    var img=document.getElementById(houseImgIds[k]);
    img.style.opacity = k===key?1:0;
    img.setAttribute('aria-hidden', k===key?'false':'true');
  });
}
// Setzt nur den Soll-Wert - die Geschwindigkeit gleitet in houseFrame() ohne
// Sprung zum neuen Tempo, Punkte springen nie an eine andere Stelle.
function houseUpdate(data){
  var p=data.power||{};
  var valid = typeof p.w==='number';
  var stale = typeof p.ageMin==='number' && p.ageMin>=HOUSE_STALE_MIN;
  houseWatts = houseForce.watt!==null ? houseForce.watt : ((valid && !stale) ? p.w : 0);
  var isDay = houseForce.mode ? (houseForce.mode==='day') : !!data.sunDay;
  var variant = houseForce.variant ? houseForce.variant : (data.variant===1 ? 'v1' : 'v2');
  var key = variant+'_'+(isDay?'day':'night');

  if (!houseImgLoaded) {
    houseImgLoaded=true;
    houseSetCur(key); // laedt zuerst das Bild der aktuellen Variante/des aktuellen Modus
    var firstImg = document.getElementById(houseImgIds[key]);
    var siblingKey = variant+'_'+(isDay?'night':'day'); // selbe Variante, anderer Modus
    var startOther = function(){ houseLoadImage(siblingKey); };
    firstImg.addEventListener('load', startOther, {once:true});
    firstImg.addEventListener('error', startOther, {once:true});
    if (!houseReduceMotion) houseInit(); // bei "weniger Bewegung": nur Standbild, keine Punkte
  } else {
    houseSetCur(key);
  }
}

// Wetteranimation (Regen/Schnee) im Haus-Hero-Container (Auftrag
// "Wetteranimation") - Partikel-System/Aussehen 1:1 aus der bestaetigten
// Vorlage wetter.html uebernommen (WX-Tabelle, Gain-Ueberblendung wxG,
// Regen-/Schnee-Zeichnung). Die Wetter-Buttons/der Watt-Regler aus der
// Vorlage sind reine Testwerkzeuge und werden bewusst NICHT uebernommen -
// die Kategorie kommt hier ausschliesslich aus data.weather.precip
// (type+level), das der Server aus dem ohnehin abgerufenen WMO-Code
// ableitet (kein Zusatz-Netzwerkverkehr).
var wxCv = document.getElementById('wx'), wxCx = wxCv.getContext('2d');
var wxP = [], wxG = 0, wxLast = 0, wxT = 0, wxType = 'trocken';
var WX = {
  niesel:{k:'rain',n:45,sp:0.8,a:0.28}, regen:{k:'rain',n:130,sp:1,a:0.42}, starkregen:{k:'rain',n:280,sp:1.25,a:0.5},
  schnee:{k:'snow',n:70,sp:1,a:0.85}, starkschnee:{k:'snow',n:190,sp:1.3,a:0.9}
};
// Reine Vorschau zum Testen (gleiches Muster wie houseForce oben) - rein
// clientseitig, schreibt/aendert nichts auf dem Geraet, bleibt unbeworben
// (nicht in README/Release-Notes erwaehnt). Ohne Zusatz gilt data.weather.precip.
var wxForce = (function(){
  var qp = new URLSearchParams(window.location.search);
  var w = qp.get('wetter');
  return (w && (WX[w] || w === 'trocken')) ? w : null;
})();
function wxSetFromServer(precip){
  if (wxForce) { wxType = wxForce; return; }
  if (!precip || precip.type === 'none' || !precip.level) { wxType = 'trocken'; return; }
  wxType = (precip.type === 'snow')
    ? (precip.level >= 3 ? 'starkschnee' : 'schnee')
    : (precip.level >= 3 ? 'starkregen' : (precip.level >= 2 ? 'regen' : 'niesel'));
}
function wxNew(k){
  return k==='rain'
    ? {x:Math.random(),y:Math.random(),v:0.75+Math.random()*0.5,l:0.7+Math.random()*0.6}
    : {x:Math.random(),y:Math.random(),v:0.6+Math.random()*0.8,r:0.8+Math.random()*1.8,ph:Math.random()*6.28,f:0.4+Math.random()*0.8};
}
function wxFrame(now){
  // Pausiert wie houseFrame effektiv, solange der Tab nicht sichtbar ist.
  if (document.hidden) { wxLast = 0; requestAnimationFrame(wxFrame); return; }
  if (!wxLast) wxLast = now;
  var dt = Math.min(0.1, (now - wxLast) / 1000); wxLast = now; wxT += dt;
  var d = window.devicePixelRatio || 1, W = wxCv.clientWidth, H = wxCv.clientHeight;
  if (!W) { requestAnimationFrame(wxFrame); return; }
  if (wxCv.width !== Math.round(W*d)) { wxCv.width = Math.round(W*d); wxCv.height = Math.round(H*d); }
  var c = wxCx; c.setTransform(d,0,0,d,0,0); c.clearRect(0,0,W,H);
  var cfg = WX[wxType];
  wxG += ((cfg?1:0) - wxG) * Math.min(1, dt*2.5); // weiches Ein-/Ausblenden bei Kategoriewechsel, kein Sprung
  if ((cfg && wxG > 0.01) || wxG > 0.02) {
    var narrow = W < 420; // Handy-Breite: weniger Partikel, schont CPU
    var nTarget = cfg ? Math.round(cfg.n * (narrow ? 0.45 : 1)) : wxP.length;
    if (cfg) {
      if (wxP.kind !== cfg.k) { wxP = []; wxP.kind = cfg.k; }
      while (wxP.length < nTarget) wxP.push(wxNew(cfg.k));
      if (wxP.length > nTarget) wxP.length = nTarget;
    }
    var sc = W/720, a = (cfg ? cfg.a : 0.3) * wxG, sp = cfg ? cfg.sp : 1;
    if (wxP.kind === 'rain') {
      c.strokeStyle = 'rgba(170,200,255,'+a+')'; c.lineWidth = 1.1*Math.max(1,sc); c.beginPath();
      wxP.forEach(function(p){
        p.y += dt*p.v*sp*1.15; p.x -= dt*0.05*sp;
        if (p.y > 1.02) { p.y = -0.05; p.x = Math.random()*1.1; }
        if (p.x < -0.05) p.x += 1.1;
        var x = p.x*W, y = p.y*H, L = p.l*16*sc*sp;
        c.moveTo(x,y); c.lineTo(x-L*0.09, y-L);
      });
      c.stroke();
    } else if (wxP.kind === 'snow') {
      c.fillStyle = 'rgba(255,255,255,'+a+')';
      wxP.forEach(function(p){
        p.y += dt*p.v*sp*0.09;
        if (p.y > 1.02) { p.y = -0.03; p.x = Math.random(); }
        var x = p.x*W + Math.sin(wxT*p.f+p.ph)*10*sc, y = p.y*H;
        c.beginPath(); c.arc(x, y, p.r*sc*1.1, 0, 6.283); c.fill();
      });
    }
  }
  requestAnimationFrame(wxFrame);
}
if (!houseReduceMotion) requestAnimationFrame(wxFrame); // "weniger Bewegung": keine Partikel

function render(data){
  if(data.lang && data.lang!==lang){ lang=data.lang; t=L[lang]||L.de; buildTabs(); showTab(); }
  document.documentElement.lang=lang;
  var accent=data.accent||'#39ff14';
  document.documentElement.style.setProperty('--accent',accent);
  document.querySelector('meta[name=theme-color]') || (function(){
    var m=document.createElement('meta'); m.name='theme-color'; document.head.appendChild(m); return m;
  })();
  setText('standLine', t.stand+': '+(data.stand||'--:--'));
  var nextTxt = t.next+': '+(data.next||'--:--');
  if (data.night) nextTxt += '   🌙  '+t.nightMode; // Auftrag "Mondsichel-Abstand": mehr Luft links/rechts, wirkte vorher gestaucht
  setText('nextLine', nextTxt);

  ['lblPower','lblPvPower'].forEach(function(id){ setText(id,t.power); });
  setText('lblToday',t.today); setText('lblBattery',t.battery);
  setText('lblOvVolt',t.voltage);
  setText('lblPvVolt',t.voltage); setText('lblPvCurr',t.current); setText('lblPvMax',t.maxToday);
  setText('lblYToday',t.today); setText('lblYWeek',t.week); setText('lblYMonth',t.month); setText('lblYYear',t.year);
  setText('lblBSoc',t.battery); setText('lblBVolt',t.voltage); setText('lblBCurr',t.current);
  setText('lblBTemp',t.temp); setText('lblBStatus',t.status);
  setText('lblBVMax',t.maxVoltage); setText('lblBVMin',t.minVoltage);
  setText('lblPvsVolt',t.voltage); setText('lblPvsCurr',t.current);
  setText('lblPvsPower',t.power); setText('lblPvsMax',t.maxToday); setText('lblPvsStatus',t.status);
  setText('heroLabel',t.weather);
  document.getElementById('houseImgV1Day').alt=t.houseAltDay;
  document.getElementById('houseImgV1Night').alt=t.houseAltNight;
  document.getElementById('houseImgV2Day').alt=t.houseAltDay;
  document.getElementById('houseImgV2Night').alt=t.houseAltNight;
  houseUpdate(data);
  wxSetFromServer(data.weather && data.weather.precip);

  var p=data.power||{};
  setText('ovPower', fmt0(p.w)); setText('pPower', fmt0(p.w));
  setText('pStand', p.stand ? (t.stand+': '+p.stand) : '');
  setText('pMax', fmt0(p.maxToday));
  var pv=data.pv||{};
  setText('pVolt', fmt1(pv.v)); setText('pCurr', fmt2(pv.i));
  lastHist = p.hist;
  drawChart(p.hist);

  // Reiter "PV-Status" (Auftrag "jede neue CYD-Seite bekommt einen
  // WebUI-Reiter") - dieselben Werte wie oben (pv.v/pv.i, power.w/
  // maxToday), plus dieselbe Alters-/Statusangabe wie auf dem Geraet
  // (Hauptwert Leistung, NICHT der seltener aktualisierte PV-Zeitstempel
  // - siehe pv_status_page.cpp).
  setText('pvsVolt', fmt1(pv.v)); setText('pvsCurr', fmt2(pv.i));
  setText('pvsPower', fmt0(p.w)); setText('pvsMax', fmt0(p.maxToday));
  {
    var statusTxt = p.stand ? (t.next+': '+(data.next||'--:--')) : '--';
    if (typeof p.ageMin === 'number' && p.ageMin >= 2) {
      statusTxt += ' ' + t.portalAgeFmt.replace('{m}', p.ageMin);
    }
    setText('pvsStatus', statusTxt);
  }

  var y=data.yield||{};
  setText('ovToday', fmt2(y.today));
  setText('yToday', fmt2(y.today)); setText('yWeek', fmt2(y.week));
  setText('yMonth', fmt2(y.month)); setText('yYear', fmt2(y.year));

  var b=data.batt||{};
  setText('ovBatt', fmt0(b.soc));
  setText('ovVolt', fmt2(b.v));
  setText('bSoc', fmt0(b.soc)); setText('bVolt', fmt2(b.v)); setText('bCurr', fmt2(b.i));
  setText('bTemp', fmt1(b.temp)); setText('bStatus', b.status||'--');
  setText('bVMax', fmt2(b.vmax)); setText('bVMin', fmt2(b.vmin));

  var w=data.weather||{};
  var wSig = w.code+'|'+w.day;
  if (last.wSig !== wSig){
    last.wSig = wSig;
    var svg = (w.valid!==false && w.locSet!==false) ? weatherSvg(w.code,w.day) : cloudsOnly(false);
    document.getElementById('heroIcon').innerHTML = svg;
    document.getElementById('wIcon').innerHTML = svg;
  }
  setText('heroTemp', (w.valid!==false && w.locSet!==false) ? (fmt1(w.temp)+'°C') : '--');
  setText('wTemp', (w.valid!==false && w.locSet!==false) ? (fmt1(w.temp)+'°C') : '--');
  setText('wLoc', w.locSet ? (w.loc||'') : t.noLoc);

  var daysSig = JSON.stringify(w.days||[]);
  if (last.daysSig !== daysSig){
    last.daysSig = daysSig;
    var rows = document.getElementById('weekRows');
    rows.innerHTML='';
    (w.days||[]).forEach(function(d){
      var row=document.createElement('div'); row.className='weekRow';
      var icon=document.createElement('div'); icon.innerHTML=weatherSvg(d.code,true);
      var day=document.createElement('div'); day.className='day'; day.textContent=t.wd[d.wd]||'';
      var range=document.createElement('div'); range.className='range';
      range.innerHTML='<b>'+fmt0(d.max)+'°</b> / '+fmt0(d.min)+'°';
      row.appendChild(icon); row.appendChild(day); row.appendChild(range);
      rows.appendChild(row);
    });
  }

  setText('lblMax', t.max);

  // Versions-/Verbindungszeile (Auftrag "WebUI erweitern"): gruener/roter
  // Punkt + Text je nachdem, ob DIESER Abruf (also die WebUI-Verbindung
  // zum Geraet) gerade erfolgreich war - siehe updateFooterConn() unten,
  // dort nochmal fuer den Fehlerfall aufgerufen. Portal-Status kommt aus
  // dem Ergebnis der letzten Portalabfrage (portalOk vom Geraet).
  updateFooterConn(true);
  setText('footerPortalText', data.portalOk ? t.portalOk : t.portalDown);
}

function updateFooterConn(connected){
  var dot=document.getElementById('footerConnDot');
  dot.className='statusDot '+(connected?'ok':'bad');
  setText('footerConnText', 'Solar Guardian v'+APP_FWV+' · '+(connected?t.connected:t.noConnShort));
}

var APP_FWV='';
var failCount=0;
function poll(){
  fetch('/api/data').then(function(r){ return r.json(); }).then(function(data){
    failCount=0;
    if (data.fwv) APP_FWV=data.fwv;
    render(data);
  }).catch(function(){
    failCount++;
    if(failCount>=2) updateFooterConn(false);
  });
}
buildTabs(); showTab();
poll();
setInterval(poll,5000);
})();
</script>
</body>
</html>
)HTMLPAGE";

    // appConfig.locationName kann eine PLZ in Klammern enthalten (z.B.
    // "Musterstadt (<PLZ>)", siehe location_screen.cpp). Im
    // WebUI darf laut Auftrag NUR der Ortsname erscheinen, keine PLZ (zu
    // genaue Standortangabe) - deshalb hier den Klammerteil abschneiden.
    String locationNameOnly(const String& full) {
        int16_t paren = full.indexOf(" (");
        if (paren < 0) return full;
        return full.substring(0, paren);
    }

    // Liest state SICHER nur ueber Snapshot/Mutex-geschuetzte Funktionen -
    // nie direkt auf geteilte Strukturen zugreifen (siehe CLAUDE.md
    // "Dual-Core-Architektur"). Laeuft auf Core 1, genau wie die Seiten
    // selbst - darf deshalb genau wie sie z.B. YieldLedger (SD-Zugriff)
    // direkt aufrufen.

    // Gleiche Ladestatus-Logik wie battery_page.cpp - hier bewusst lokal
    // dupliziert (siehe CLAUDE.md Code-Stil: jeder Teil bleibt unabhaengig
    // lauffaehig).
    int8_t chargeCategory(int16_t raw) {
        if (raw < 0) return -1;
        if (raw <= 3) return (int8_t)raw;
        return (int8_t)((raw >> 2) & 3);
    }
    enum class EffectiveStatus : uint8_t { None, Float, Boost, Equalize, Weak };
    EffectiveStatus effectiveStatus(int8_t category, float current, bool currentValid) {
        if (currentValid) {
            if (current <= 0.0f) return EffectiveStatus::None;
            if (category == 2 || category == 3) {
                if (current > 1.0f) return (category == 2) ? EffectiveStatus::Boost : EffectiveStatus::Equalize;
                return EffectiveStatus::Weak;
            }
        }
        switch (category) {
            case 1: return EffectiveStatus::Float;
            case 2: return EffectiveStatus::Boost;
            case 3: return EffectiveStatus::Equalize;
            default: return EffectiveStatus::None;
        }
    }
    I18n::Str statusLabelKey(EffectiveStatus s) {
        switch (s) {
            case EffectiveStatus::Float: return I18n::Str::CHARGE_STATUS_1;
            case EffectiveStatus::Boost: return I18n::Str::CHARGE_STATUS_2;
            case EffectiveStatus::Equalize: return I18n::Str::CHARGE_STATUS_3;
            case EffectiveStatus::Weak: return I18n::Str::CHARGE_STATUS_WEAK;
            default: return I18n::Str::CHARGE_STATUS_0;
        }
    }
    String statusDisplayFor(const SolarData::Data& snap) {
        if (snap.chargeStatusIsText && snap.chargeStatusRawText[0]) return String(snap.chargeStatusRawText);
        if (snap.chargeStatusCode >= 0) {
            EffectiveStatus s = effectiveStatus(chargeCategory(snap.chargeStatusCode), snap.batteryCurrent, snap.batteryValid);
            return String(I18n::t(statusLabelKey(s)));
        }
        return String("--");
    }

    // Niederschlagskategorie fuer die Wetteranimation in der WebUI
    // (Auftrag "Wetteranimation") - leitet sich AUSSCHLIESSLICH aus dem
    // ohnehin schon abgerufenen WMO-Wettercode ab, kein zusaetzlicher
    // Netzwerk-Request. "none" bei unbekanntem Code, fehlenden Daten oder
    // Daten aelter als 3 Stunden (Config::WEATHER_PRECIP_STALE_SEC).
    // Zuordnung laut Auftrag:
    //   niesel (Regen Stufe 1): 51/53/55/56/57
    //   regen (Stufe 2): 61/63/80/81/66
    //   starkregen (Stufe 3): 65/82/67/95-99
    //   schnee (Stufe 2): 71/73/77/85
    //   starkschnee (Stufe 3): 75/86
    // Fuer "leichter Schnee" (Stufe 1) gibt es im WMO-Code-Satz, den diese
    // App abruft, keinen eigenen Code - 71 ("slight snow fall") ist laut
    // Auftrag bereits Stufe 2 zugeordnet, es bleibt also bewusst KEINE
    // Stufe-1-Schnee-Kategorie bestehen (an Alex zurueckgemeldet).
    void precipForCode(int16_t code, bool valid, time_t measuredAt, const char*& outType, uint8_t& outLevel) {
        outType = "none";
        outLevel = 0;
        if (!valid) return;
        time_t now = time(nullptr);
        if (measuredAt <= 0 || (now > measuredAt && (uint32_t)(now - measuredAt) > Config::WEATHER_PRECIP_STALE_SEC)) return;

        switch (code) {
            case 51: case 53: case 55: case 56: case 57:
                outType = "rain"; outLevel = 1; return;
            case 61: case 63: case 80: case 81: case 66:
                outType = "rain"; outLevel = 2; return;
            case 65: case 82: case 67:
            case 95: case 96: case 97: case 98: case 99:
                outType = "rain"; outLevel = 3; return;
            case 71: case 73: case 77: case 85:
                outType = "snow"; outLevel = 2; return;
            case 75: case 86:
                outType = "snow"; outLevel = 3; return;
            default:
                return;
        }
    }

    void formatHHMM(time_t epoch, char* out, size_t outSize) {
        if (epoch <= 0) { strncpy(out, "--:--", outSize - 1); out[outSize - 1] = 0; return; }
        struct tm t;
        localtime_r(&epoch, &t);
        snprintf(out, outSize, "%02d:%02d", t.tm_hour, t.tm_min);
    }

    // Lokaler UTC-Versatz in Sekunden (DST-sicher) - portabler Ersatz fuer
    // das auf diesem Toolchain fehlende struct tm::tm_gmtoff (ESP32-
    // Arduino/newlib stellt es hier nicht bereit, siehe Build-Fehler bei
    // einem direkten Zugriff). mktime() interpretiert die UTC-Felder als
    // waeren sie lokale Zeit und liefert dadurch genau den Versatz zurueck,
    // inklusive korrekter Sommerzeit fuer das Datum von "now" (ueber die
    // POSIX-TZ-Regel aus configTzTime()).
    long localUtcOffsetSeconds(time_t now) {
        struct tm utcTm;
        gmtime_r(&now, &utcTm);
        time_t utcAsLocalEpoch = mktime(&utcTm);
        return (long)(now - utcAsLocalEpoch);
    }

    // Echter Sonnenstand fuer die Haus-Grafik (Auftrag "Tag/Nacht nach
    // Sonnenstand", NICHT YieldLedger::isNightModeConfirmedNow() - das ist
    // der feste Abfrage-Ruhefenster-Takt). Rueckfall Nacht (false), wenn
    // kein Standort gesetzt oder die Uhrzeit (noch) nicht gueltig ist
    // (siehe sun_times.h).
    bool isGraphicDaytimeNow() {
        if (!appConfig.locationSet) return false;
        struct tm nowTm;
        if (!getLocalTime(&nowTm, 5)) return false;
        SunTimes::Result r = SunTimes::compute(appConfig.locationLat, appConfig.locationLon,
                                                nowTm.tm_yday, nowTm.tm_hour * 60 + nowTm.tm_min,
                                                localUtcOffsetSeconds(time(nullptr)));
        if (!r.valid) return false;
        return r.isDay;
    }

    // Kleine, rein lesende JSON-Antwort (Auftrag: unter ca. 3KB, keine
    // Zugangsdaten/Token/Seriennummer/Koordinaten - nur der Ortsname darf
    // erscheinen).
    void handleApiData() {
        SolarData::Data snap = SolarData::snapshot();
        WeatherData::Data weather = WeatherData::snapshot();
        YieldLedger::Values yield = YieldLedger::current();
        uint16_t accent = Display::systemColor();

        JsonDocument doc;
        doc["lang"] = (appConfig.language == 1) ? "en" : "de";
        doc["fwv"] = Config::FW_VERSION;
        doc["night"] = YieldLedger::isNightModeConfirmedNow();
        // Echter Sonnenstand fuer die Haus-Grafik (Auftrag "Tag/Nacht nach
        // Sonnenstand") - bewusst GETRENNT von "night" oben (fester Abfrage-
        // Ruhefenster-Takt). Nur ein Kennzeichen + optional die Uhrzeiten,
        // NIE Koordinaten in der Antwort.
        doc["sunDay"] = isGraphicDaytimeNow();
        // Bild-Variante der Haus-Grafik (Auftrag "zwei Bild-Varianten") -
        // haengt am vorhandenen Sondermodus-Flag (EasterEgg::
        // certificateValid(), siehe easter_egg.h), keine zweite Speicherung.
        // Rein neutrales Kennzeichen (1 oder 2), NIE ein Name/Text dazu.
        doc["variant"] = EasterEgg::certificateValid() ? 1 : 2;
        {
            struct tm sunTm;
            if (appConfig.locationSet && getLocalTime(&sunTm, 5)) {
                SunTimes::Result sr = SunTimes::compute(appConfig.locationLat, appConfig.locationLon,
                                                         sunTm.tm_yday, sunTm.tm_hour * 60 + sunTm.tm_min,
                                                         localUtcOffsetSeconds(time(nullptr)));
                if (sr.valid && !sr.polar) {
                    char riseBuf[6], setBuf[6];
                    snprintf(riseBuf, sizeof(riseBuf), "%02d:%02d", sr.sunriseMinLocal / 60, sr.sunriseMinLocal % 60);
                    snprintf(setBuf, sizeof(setBuf), "%02d:%02d", sr.sunsetMinLocal / 60, sr.sunsetMinLocal % 60);
                    JsonObject sun = doc["sun"].to<JsonObject>();
                    sun["rise"] = riseBuf;
                    sun["set"] = setBuf;
                }
            }
        }
        doc["portalOk"] = (snap.fetchStatus == SolarData::FetchStatus::Ok);
        char accentHex[8];
        snprintf(accentHex, sizeof(accentHex), "#%02x%02x%02x",
                 (int)(((accent >> 11) & 0x1F) * 255 / 31),
                 (int)(((accent >> 5) & 0x3F) * 255 / 63),
                 (int)((accent & 0x1F) * 255 / 31));
        doc["accent"] = accentHex;

        char standBuf[6], nextBuf[6];
        formatHHMM(snap.lastSuccessfulFetch, standBuf, sizeof(standBuf));
        formatHHMM(snap.nextFetchAt, nextBuf, sizeof(nextBuf));
        doc["stand"] = standBuf;
        doc["next"] = nextBuf;

        JsonObject power = doc["power"].to<JsonObject>();
        power["w"] = snap.powerValid ? snap.currentPowerW : (float)NAN;
        char powerStandBuf[6];
        formatHHMM(snap.powerMeasuredAt, powerStandBuf, sizeof(powerStandBuf));
        power["stand"] = powerStandBuf;
        // Alter des Hauptwerts in Minuten (Auftrag "veraltet nach
        // erfolgreichem Refresh") - exakt dieselbe Berechnung wie auf dem
        // Geraet (power_page.cpp/pv_status_page.cpp), -1 = noch kein Wert.
        power["ageMin"] = snap.powerMeasuredAt > 0
            ? (int)((time(nullptr) - snap.powerMeasuredAt) / 60)
            : -1;
        // Heutiges Leistungsmaximum (Auftrag "Neuer Bildschirm PV") -
        // dieselbe Quelle/Berechnung wie die "Max:"-Zeile im Tagesverlauf
        // und die neue PV-Status-Seite (snap.dailyCurve[]), keine eigene
        // Variable/Anfrage.
        {
            float maxToday = 0.0f;
            for (uint16_t i = 0; i < snap.dailyCurveCount; i++) {
                if ((float)snap.dailyCurve[i].watts > maxToday) maxToday = (float)snap.dailyCurve[i].watts;
            }
            power["maxToday"] = snap.dailyCurveCount > 0 ? maxToday : (float)NAN;
        }
        JsonArray hist = power["hist"].to<JsonArray>();
        for (uint8_t i = 0; i < snap.historyCount; i++) {
            JsonObject pt = hist.add<JsonObject>();
            pt["t"] = (uint32_t)snap.history[i].epoch;
            pt["w"] = snap.history[i].valueW;
        }

        JsonObject pv = doc["pv"].to<JsonObject>();
        pv["v"] = snap.pvValid ? snap.pvVoltage : (float)NAN;
        pv["i"] = snap.pvValid ? snap.pvCurrent : (float)NAN;

        JsonObject yieldObj = doc["yield"].to<JsonObject>();
        yieldObj["today"] = yield.valid ? yield.todayKwh : (float)NAN;
        yieldObj["week"] = yield.valid ? yield.weekKwh : (float)NAN;
        yieldObj["month"] = yield.valid ? yield.monthKwh : (float)NAN;
        yieldObj["year"] = yield.valid ? yield.yearKwh : (float)NAN;

        JsonObject batt = doc["batt"].to<JsonObject>();
        batt["v"] = snap.batteryValid ? snap.batteryVoltage : (float)NAN;
        batt["i"] = snap.batteryValid ? snap.batteryCurrent : (float)NAN;
        batt["soc"] = snap.batteryValid ? snap.batterySocPercent : (float)NAN;
        batt["temp"] = snap.batteryValid ? snap.batteryTempC : (float)NAN;
        batt["status"] = statusDisplayFor(snap);
        batt["vmax"] = snap.batteryVoltageExtremesValid ? snap.batteryVoltageMax : (float)NAN;
        batt["vmin"] = snap.batteryVoltageExtremesValid ? snap.batteryVoltageMin : (float)NAN;

        JsonObject weatherObj = doc["weather"].to<JsonObject>();
        weatherObj["locSet"] = weather.locationSet;
        weatherObj["loc"] = weather.locationSet ? locationNameOnly(appConfig.locationName) : String("");
        weatherObj["valid"] = weather.currentValid;
        weatherObj["temp"] = weather.currentValid ? weather.currentTempC : (float)NAN;
        weatherObj["code"] = weather.currentWeatherCode;
        weatherObj["day"] = weather.isDay;
        {
            const char* precipType;
            uint8_t precipLevel;
            precipForCode(weather.currentWeatherCode, weather.currentValid, weather.measuredAt, precipType, precipLevel);
            JsonObject precip = weatherObj["precip"].to<JsonObject>();
            precip["type"] = precipType;
            precip["level"] = precipLevel;
        }
        JsonArray days = weatherObj["days"].to<JsonArray>();
        for (uint8_t i = 0; i < weather.dayCount; i++) {
            const WeatherData::DayForecast& d = weather.days[i];
            JsonObject o = days.add<JsonObject>();
            struct tm t;
            localtime_r(&d.dateEpoch, &t);
            o["wd"] = (t.tm_wday >= 0 && t.tm_wday <= 6) ? t.tm_wday : 0;
            o["code"] = d.weatherCode;
            o["max"] = d.tempMaxC;
            o["min"] = d.tempMinC;
        }

        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    }

    // Tagesverlauf (05:00-22:00) - eigener, kleiner Abruf statt Teil von
    // /api/data (Auftrag "kompakte eigene Abrufe"), damit die oft recht
    // grosse Punktliste nur geladen wird, wenn der Tagesverlauf-Tab auch
    // wirklich offen ist. Punkte als [minute, watt]-Zahlenpaare statt
    // benannter Objekte - spart Bytes pro Punkt.
    void handleApiDay() {
        SolarData::Data snap = SolarData::snapshot();
        JsonDocument doc;
        float maxW = 0.0f;
        for (uint16_t i = 0; i < snap.dailyCurveCount; i++) {
            if ((float)snap.dailyCurve[i].watts > maxW) maxW = (float)snap.dailyCurve[i].watts;
        }
        doc["max"] = snap.dailyCurveCount > 0 ? maxW : (float)NAN;
        JsonArray pts = doc["pts"].to<JsonArray>();
        for (uint16_t i = 0; i < snap.dailyCurveCount; i++) {
            JsonArray p = pts.add<JsonArray>();
            p.add(snap.dailyCurve[i].minutesSinceMidnight);
            p.add(snap.dailyCurve[i].watts);
        }
        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    }

    // Tage/Monate/Jahre - ebenfalls ein eigener, kleiner Abruf (Auftrag),
    // nur bei geoeffnetem Verlauf-Tab abgerufen. Gleiche Quellen wie am
    // Geraet (siehe years_page.cpp).
    void handleApiHistory() {
        JsonDocument doc;
        YieldLedger::Values yv = YieldLedger::current();
        struct tm nowTm;
        bool haveNow = getLocalTime(&nowTm, 5);

        JsonArray days = doc["days"].to<JsonArray>();
        YieldLedger::DayValue dvals[7];
        uint8_t dcount = YieldLedger::lastDaysIncludingToday(dvals, 7);
        for (uint8_t i = 0; i < dcount; i++) {
            JsonObject o = days.add<JsonObject>();
            o["d"] = dvals[i].date;
            // Auf echte 2 Nachkommastellen runden (Auftrag "genauer
            // Tagesertrag in der WebUI") - vermeidet Rauschen wie 9.350000012
            // in der JSON-Ausgabe, die Anzeige selbst rundet ohnehin aufs
            // Hundertstel (fmtDay2() im Frontend).
            o["kwh"] = roundf(dvals[i].kwh * 100.0f) / 100.0f;
            o["cur"] = (i == (uint8_t)(dcount - 1));
        }

        JsonArray months = doc["months"].to<JsonArray>();
        YieldLedger::MonthEntry mvals[6];
        uint8_t mcount = YieldLedger::pastMonths(mvals, 6);
        for (uint8_t i = 0; i < mcount; i++) {
            JsonObject o = months.add<JsonObject>();
            o["m"] = mvals[i].key;
            o["kwh"] = mvals[i].kwh;
            o["cur"] = false;
        }
        if (haveNow) {
            char key[8];
            snprintf(key, sizeof(key), "%04d-%02d", nowTm.tm_year + 1900, nowTm.tm_mon + 1);
            JsonObject o = months.add<JsonObject>();
            o["m"] = key;
            o["kwh"] = yv.valid ? yv.monthKwh : 0.0f;
            o["cur"] = true;
        }

        JsonArray years = doc["years"].to<JsonArray>();
        YieldLedger::YearEntry yvals[Config::HISTORY_CHART_MAX_BARS];
        uint8_t ycount = YieldLedger::pastYears(yvals, (uint8_t)(Config::HISTORY_CHART_MAX_BARS - 1));
        for (uint8_t i = 0; i < ycount; i++) {
            JsonObject o = years.add<JsonObject>();
            o["y"] = yvals[i].year;
            o["kwh"] = yvals[i].kwh;
            o["cur"] = false;
        }
        if (haveNow) {
            JsonObject o = years.add<JsonObject>();
            o["y"] = nowTm.tm_year + 1900;
            o["kwh"] = yv.valid ? yv.yearKwh : 0.0f;
            o["cur"] = true;
        }

        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    }

    // Sendet die statische Seite direkt aus dem Flash in Stuecken (Auftrag
    // "chunked/sendContent", kein grosser String im RAM).
    void sendPageChunked() {
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "text/html", "");
        size_t total = sizeof(PAGE_HTML) - 1; // ohne abschliessendes NUL
        constexpr size_t CHUNK = 2048;
        size_t sentBytes = 0;
        while (sentBytes < total) {
            size_t n = (total - sentBytes < CHUNK) ? (total - sentBytes) : CHUNK;
            server.sendContent_P(PAGE_HTML + sentBytes, n);
            sentBytes += n;
            yield(); // Auftrag: keine lange Schleife ohne yield()
        }
    }

    // Haus-Grafik (Auftrag "Animierte Haus-Grafik in der WebUI",
    // Ueberarbeitungen "Tag/Nacht nach Sonnenstand" und "zwei Bild-
    // Varianten"): liefert die in house_image.h eingebetteten JPEGs direkt
    // aus dem Flash, genau wie sendPageChunked() oben - kein SD-Zugriff,
    // blockiert den WebServer nur kurz pro 2KB-Stueck (yield() dazwischen).
    // Langer Cache-Header, weil sich die Bilder nie aendern - die
    // Versionsnummer in den vier Routen (muss hier UND in den
    // houseRoutes-JS-Strings in PAGE_HTML identisch sein) wird nur bei
    // einem tatsaechlichen Bildwechsel erhoeht, damit ein Update beim
    // Nutzer automatisch die neue Version laedt statt der gecachten alten.
    // Die fruehere Einzelbild-Route "/house_v1.jpg" und die fruehere Tag/
    // Nacht-ohne-Varianten-Fassung ("/house_day_v2.jpg"/
    // "/house_night_v2.jpg") wurden entfernt (liefern jetzt 404, kein
    // Alias).
    void sendHouseImage(const uint8_t* data, size_t len) {
        server.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "image/jpeg", "");
        constexpr size_t CHUNK = 2048;
        size_t sentBytes = 0;
        while (sentBytes < len) {
            size_t n = (len - sentBytes < CHUNK) ? (len - sentBytes) : CHUNK;
            server.sendContent_P((const char*)data + sentBytes, n);
            sentBytes += n;
            yield(); // Auftrag: keine lange Schleife ohne yield()
        }
    }
    void handleHouseV1DayImage() { sendHouseImage(HOUSE_IMAGE_V1_DAY_JPG, HOUSE_IMAGE_V1_DAY_JPG_LEN); }
    void handleHouseV1NightImage() { sendHouseImage(HOUSE_IMAGE_V1_NIGHT_JPG, HOUSE_IMAGE_V1_NIGHT_JPG_LEN); }
    void handleHouseV2DayImage() { sendHouseImage(HOUSE_IMAGE_V2_DAY_JPG, HOUSE_IMAGE_V2_DAY_JPG_LEN); }
    void handleHouseV2NightImage() { sendHouseImage(HOUSE_IMAGE_V2_NIGHT_JPG, HOUSE_IMAGE_V2_NIGHT_JPG_LEN); }

    void handleNotFound() {
        server.send(404, "text/plain", "Not found");
    }
}

void tick() {
    if (!started) {
        if (WiFi.status() != WL_CONNECTED) return;
        server.on("/", sendPageChunked);
        server.on("/house_v1_day_v4.jpg", handleHouseV1DayImage);
        server.on("/house_v1_night_v4.jpg", handleHouseV1NightImage);
        server.on("/house_v2_day_v4.jpg", handleHouseV2DayImage);
        server.on("/house_v2_night_v4.jpg", handleHouseV2NightImage);
        server.on("/api/data", handleApiData);
        server.on("/api/day", handleApiDay);
        server.on("/api/history", handleApiHistory);
        WebFiles::registerRoutes(server);
        server.onNotFound(handleNotFound);
        // Der "Origin"-Header wird vom WebServer standardmaessig NICHT
        // erfasst (hasHeader() liefert ohne diese Registrierung immer
        // false) - ohne collectHeaders() hier waere der Origin-Grundschutz
        // in web_files.cpp::originAllowed() wirkungslos (Bug, per
        // curl-Test entdeckt und hier behoben).
        {
            const char* headerKeys[] = {"Origin"};
            server.collectHeaders(headerKeys, 1);
        }
        server.begin();
        started = true;
        Serial.print("[web] WebUI gestartet, Adresse: http://");
        Serial.println(WiFi.localIP());
        Serial.printf("[web] Heap nach WebUI-Start: %u Byte\n", (unsigned)ESP.getFreeHeap());
    }
    server.handleClient();
}

}
