#pragma once
// Die Swipe-Seite "Tagesverlauf": Leistungskurve des GANZEN Tages von
// Config::DAILY_CURVE_START_HOUR bis DAILY_CURVE_END_HOUR Uhr, plus
// Tages-Hoechstwert. Direkt hinter "Leistung" in der Wischreihenfolge,
// siehe pages.cpp. Daten kommen aus SolarData::Data::dailyCurve (siehe
// solar_data.h, fetch_task.cpp).
#include <Arduino.h>

namespace DailyCurvePage {
    void draw(int16_t areaTop, int16_t areaHeight, bool forceRedraw);
    bool update();
}
