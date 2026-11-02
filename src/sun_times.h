#pragma once
// Sonnenauf-/-untergang fuer die Tag/Nacht-Auswahl der animierten Haus-
// Grafik in der WebUI (Auftrag "Animierte Haus-Grafik" / Ueberarbeitung
// "Tag/Nacht nach Sonnenstand"). NICHT zu verwechseln mit YieldLedger::
// isNightModeConfirmedNow() (day_window.h) - das ist ein fester Abfrage-
// Ruhefenster-Takt [22:55, 04:30), unabhaengig vom echten Sonnenstand.
//
// NOAA-Naeherungsformel (Sonnenauf-/-untergangsgleichung, ~1 Minute
// Genauigkeit - fuer eine Tag/Nacht-Grafik voellig ausreichend). Reine
// Berechnung ohne IO, genau wie day_window.h nativ testbar (siehe
// test/test_sun_times) - KEINE Koordinaten/Zeit werden hier selbst
// geholt, der Aufrufer uebergibt sie.
//
// gmtoffSeconds kommt vom Aufrufer aus tm_gmtoff (von localtime_r/
// getLocalTime) - DST-sicher, da die POSIX-TZ-Regel (configTzTime()) den
// Sommerzeit-Versatz automatisch korrekt einrechnet; dieses Modul selbst
// weiss nichts von Sommerzeit, es verschiebt nur additiv um den
// uebergebenen Versatz.
#include <Arduino.h>
#include <time.h>
#include <math.h>

namespace SunTimes {

struct Result {
    bool valid = false;           // false => ungueltige Eingabe, Aufrufer soll Fallback (Nacht) nehmen
    bool isDay = false;
    bool polar = false;           // true = Polartag/-nacht (kein Sonnenauf-/-untergang an diesem Tag)
    uint16_t sunriseMinLocal = 0; // Minuten seit lokaler Mitternacht, nur sinnvoll wenn valid && !polar
    uint16_t sunsetMinLocal = 0;
};

inline double wrapMinutes(double m) {
    while (m < 0.0) m += 1440.0;
    while (m >= 1440.0) m -= 1440.0;
    return m;
}

// latDeg/lonDeg: WGS84-Grad (Ost/Nord positiv). dayOfYear: 0-basiert wie
// tm_yday. nowMinutesLocal: lokale Minuten seit Mitternacht
// (tm_hour*60+tm_min). gmtoffSeconds: lokaler UTC-Versatz (tm_gmtoff).
inline Result compute(float latDeg, float lonDeg, int dayOfYear, int nowMinutesLocal, long gmtoffSeconds) {
    Result r;
    if (dayOfYear < 0 || dayOfYear > 365) return r;
    if (nowMinutesLocal < 0 || nowMinutesLocal >= 1440) return r;
    if (latDeg < -90.0f || latDeg > 90.0f || lonDeg < -180.0f || lonDeg > 180.0f) return r;

    double n = (double)(dayOfYear + 1);
    double gamma = 2.0 * M_PI / 365.0 * (n - 1.0);

    double eqtime = 229.18 * (0.000075 + 0.001868 * cos(gamma) - 0.032077 * sin(gamma)
                               - 0.014615 * cos(2 * gamma) - 0.040849 * sin(2 * gamma));
    double decl = 0.006918 - 0.399912 * cos(gamma) + 0.070257 * sin(gamma)
                  - 0.006758 * cos(2 * gamma) + 0.000907 * sin(2 * gamma)
                  - 0.002697 * cos(3 * gamma) + 0.00148 * sin(3 * gamma);

    double latRad = (double)latDeg * M_PI / 180.0;
    constexpr double ZENITH = 90.833 * M_PI / 180.0; // Standard-Zenit inkl. Refraktion/Sonnenradius

    double cosHa = (cos(ZENITH) - sin(latRad) * sin(decl)) / (cos(latRad) * cos(decl));

    r.valid = true;
    if (cosHa >= 1.0) { r.polar = true; r.isDay = false; return r; } // Polarnacht
    if (cosHa <= -1.0) { r.polar = true; r.isDay = true; return r; } // Polartag

    double haDeg = acos(cosHa) * 180.0 / M_PI;
    double solarNoonUtcMin = 720.0 - 4.0 * (double)lonDeg - eqtime;
    double sunriseLocalMin = wrapMinutes(solarNoonUtcMin - 4.0 * haDeg + (double)gmtoffSeconds / 60.0);
    double sunsetLocalMin = wrapMinutes(solarNoonUtcMin + 4.0 * haDeg + (double)gmtoffSeconds / 60.0);

    r.sunriseMinLocal = (uint16_t)(sunriseLocalMin + 0.5);
    r.sunsetMinLocal = (uint16_t)(sunsetLocalMin + 0.5);
    if (r.sunriseMinLocal >= 1440) r.sunriseMinLocal = 1439;
    if (r.sunsetMinLocal >= 1440) r.sunsetMinLocal = 1439;

    r.isDay = (nowMinutesLocal >= r.sunriseMinLocal) && (nowMinutesLocal < r.sunsetMinLocal);
    return r;
}

}
