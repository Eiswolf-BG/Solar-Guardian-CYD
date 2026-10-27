#include "web_server.h"
#include "web_files.h"
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
:root{--accent:#39ff14;--bg:#0a0a0a;--card:#141414;--text:#e8e8e8;--dim:#8a8a8a;}
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
     pvStatus:'PV-Status',portalAgeFmt:'Portaldaten {m} Min alt'},
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
     pvStatus:'PV status',portalAgeFmt:'Portal data {m} min old'}
};
var lang='de', t=L.de;
var last={};
var tabsEl=document.getElementById('tabs');
var tabIds=['tab-overview','tab-power','tab-daily','tab-yield','tab-battery','tab-pvstatus','tab-history','tab-weather','tab-files'];
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
  if (activeTab===1) drawChart(lastHist);
  if (activeTab===2) pollDay();
  // Bug behoben: der Tage/Monate/Jahre-Unterreiter (histSubnav) wurde nie
  // gebaut, nur beim Klick auf einen seiner (nie existierenden) Knoepfe -
  // Monate/Jahre waren dadurch unerreichbar, nur die Standardansicht
  // "Tage" zeigte ueberhaupt Daten.
  if (activeTab===5) { buildHistSubnav(); pollHistory(); }
  if (activeTab===7) filesInit();
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
  setText('lblPvVolt',t.voltage); setText('lblPvCurr',t.current); setText('lblPvMax',t.maxToday);
  setText('lblYToday',t.today); setText('lblYWeek',t.week); setText('lblYMonth',t.month); setText('lblYYear',t.year);
  setText('lblBSoc',t.battery); setText('lblBVolt',t.voltage); setText('lblBCurr',t.current);
  setText('lblBTemp',t.temp); setText('lblBStatus',t.status);
  setText('lblBVMax',t.maxVoltage); setText('lblBVMin',t.minVoltage);
  setText('lblPvsVolt',t.voltage); setText('lblPvsCurr',t.current);
  setText('lblPvsPower',t.power); setText('lblPvsMax',t.maxToday); setText('lblPvsStatus',t.status);
  setText('heroLabel',t.weather);

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

    void formatHHMM(time_t epoch, char* out, size_t outSize) {
        if (epoch <= 0) { strncpy(out, "--:--", outSize - 1); out[outSize - 1] = 0; return; }
        struct tm t;
        localtime_r(&epoch, &t);
        snprintf(out, outSize, "%02d:%02d", t.tm_hour, t.tm_min);
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

    void handleNotFound() {
        server.send(404, "text/plain", "Not found");
    }
}

void tick() {
    if (!started) {
        if (WiFi.status() != WL_CONNECTED) return;
        server.on("/", sendPageChunked);
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
