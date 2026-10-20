#include "web_server.h"
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
.barsChart .bar{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:flex-end;height:100%;}
.barsChart .bar .fill{width:70%;background:var(--accent);opacity:.45;border-radius:4px 4px 0 0;}
.barsChart .bar.cur .fill{opacity:1;}
.barsChart .bar .v{font-size:.72rem;color:var(--dim);margin-bottom:4px;}
.barsChart .bar .y{font-size:.78rem;color:var(--dim);margin-top:6px;}
footer{text-align:center;color:var(--dim);font-size:.72rem;padding:16px;}
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
    <div class="card" style="grid-column:1/-1;"><div class="label" id="lblBStatus">Status</div><div class="value" style="font-size:1.2rem;" id="bStatus">--</div></div>
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
</main>
<footer id="footerNote">Solar Guardian CYD</footer>
<script>
(function(){
var L={
 de:{tabs:['Übersicht','Leistung','Tagesverlauf','Ertrag','Batterie','Verlauf','Wetter'],
     power:'Leistung',today:'Heute',battery:'Batterie',week:'Woche',month:'Monat',year:'Jahr',
     voltage:'Spannung',current:'Strom',temp:'Temperatur',status:'Status',weather:'Wetter',
     stand:'Stand',next:'Nächste Abfrage',noLoc:'Standort nicht gesetzt',noConn:'Keine Verbindung zum Gerät',
     wd:['So','Mo','Di','Mi','Do','Fr','Sa'],
     mo:['Jan','Feb','Mär','Apr','Mai','Jun','Jul','Aug','Sep','Okt','Nov','Dez'],
     now:'jetzt',max:'Max',days:'Tage',months:'Monate',years:'Jahre'},
 en:{tabs:['Overview','Power','Daily curve','Yield','Battery','History','Weather'],
     power:'Power',today:'Today',battery:'Battery',week:'Week',month:'Month',year:'Year',
     voltage:'Voltage',current:'Current',temp:'Temperature',status:'Status',weather:'Weather',
     stand:'As of',next:'Next update',noLoc:'No location set',noConn:'Lost connection to device',
     wd:['Sun','Mon','Tue','Wed','Thu','Fri','Sat'],
     mo:['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'],
     now:'now',max:'Max',days:'Days',months:'Months',years:'Years'}
};
var lang='de', t=L.de;
var last={};
var tabsEl=document.getElementById('tabs');
var tabIds=['tab-overview','tab-power','tab-daily','tab-yield','tab-battery','tab-history','tab-weather'];
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
}

function setText(id,val){
  var el=document.getElementById(id);
  if(el.textContent!==val) el.textContent=val;
}
function fmt1(v){ return (typeof v==='number') ? v.toFixed(1) : '--'; }
function fmt2(v){ return (typeof v==='number') ? v.toFixed(2) : '--'; }
function fmt0(v){ return (typeof v==='number') ? Math.round(v).toString() : '--'; }

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
  var pad={l:44,r:10,t:10,b:20};
  var cw=w-pad.l-pad.r, ch=h-pad.t-pad.b;
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
  // "Jetzt"-Linie, nur innerhalb des Anzeigefensters.
  var now=new Date();
  var nowMin=now.getHours()*60+now.getMinutes();
  if(nowMin>=DAY_START_MIN && nowMin<DAY_END_MIN){
    var nfrac=(nowMin-DAY_START_MIN)/DAY_SPAN_MIN;
    var nx=pad.l+nfrac*cw;
    ctx.strokeStyle=accent; ctx.lineWidth=1; ctx.beginPath();
    ctx.moveTo(nx,pad.t); ctx.lineTo(nx,pad.t+ch); ctx.stroke();
  }
  // Zeitachse (05/09/13/17/22) in der gedaempften Sekundaerfarbe.
  ctx.fillStyle='#8a8a8a'; ctx.textAlign='center'; ctx.font='10px sans-serif';
  [5,9,13,17,22].forEach(function(hh){
    var frac=(hh*60-DAY_START_MIN)/DAY_SPAN_MIN;
    ctx.fillText(String(hh),pad.l+frac*cw,h-4);
  });
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
    var vEl=document.createElement('div'); vEl.className='v'; vEl.textContent=fmt0(e.kwh);
    var fill=document.createElement('div'); fill.className='fill';
    fill.style.height=Math.max(4,(e.kwh/maxKwh)*100)+'%';
    var yEl=document.createElement('div'); yEl.className='y'; yEl.textContent=histLabel(e);
    bar.appendChild(vEl); bar.appendChild(fill); bar.appendChild(yEl);
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
  setText('nextLine', t.next+': '+(data.next||'--:--'));

  ['lblPower','lblPvPower'].forEach(function(id){ setText(id,t.power); });
  setText('lblToday',t.today); setText('lblBattery',t.battery);
  setText('lblPvVolt',t.voltage); setText('lblPvCurr',t.current);
  setText('lblYToday',t.today); setText('lblYWeek',t.week); setText('lblYMonth',t.month); setText('lblYYear',t.year);
  setText('lblBSoc',t.battery); setText('lblBVolt',t.voltage); setText('lblBCurr',t.current);
  setText('lblBTemp',t.temp); setText('lblBStatus',t.status);
  setText('heroLabel',t.weather);

  var p=data.power||{};
  setText('ovPower', fmt0(p.w)); setText('pPower', fmt0(p.w));
  setText('pStand', p.stand ? (t.stand+': '+p.stand) : '');
  var pv=data.pv||{};
  setText('pVolt', fmt1(pv.v)); setText('pCurr', fmt2(pv.i));
  lastHist = p.hist;
  drawChart(p.hist);

  var y=data.yield||{};
  setText('ovToday', fmt2(y.today));
  setText('yToday', fmt2(y.today)); setText('yWeek', fmt2(y.week));
  setText('yMonth', fmt2(y.month)); setText('yYear', fmt2(y.year));

  var b=data.batt||{};
  setText('ovBatt', fmt0(b.soc));
  setText('bSoc', fmt0(b.soc)); setText('bVolt', fmt2(b.v)); setText('bCurr', fmt2(b.i));
  setText('bTemp', fmt1(b.temp)); setText('bStatus', b.status||'--');

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

  setText('footerNote','Solar Guardian CYD');
}

var failCount=0;
function poll(){
  fetch('/api/data').then(function(r){ return r.json(); }).then(function(data){
    failCount=0;
    render(data);
  }).catch(function(){
    failCount++;
    if(failCount>=2) setText('footerNote', t.noConn);
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
            o["kwh"] = dvals[i].kwh;
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
        server.onNotFound(handleNotFound);
        server.begin();
        started = true;
        Serial.print("[web] WebUI gestartet, Adresse: http://");
        Serial.println(WiFi.localIP());
    }
    server.handleClient();
}

}
