#include "pages.h"
#include "home_page.h"
#include "power_page.h"
#include "daily_curve_page.h"
#include "yield_page.h"
#include "years_page.h"
#include "battery_page.h"
#include "pv_status_page.h"
#include "week_page.h"
#include "i18n.h"

namespace Pages {

namespace {
    // Reihenfolge der Seiten - zentral an dieser einen Stelle, siehe
    // CLAUDE.md. Die Uebersicht (4 Kacheln) ist seit dem Auftrag
    // "Hauptscreen/Wetter" die erste Seite nach dem Booten. Seit Auftrag
    // "Seitenreihenfolge am CYD aendern, Teil 2": PV-Status und Batterie
    // ruecken direkt nach der Uebersicht (vorher nach Ertrag), "Verlauf"
    // bleibt die letzte Zahlen-/Diagrammseite, NEU dahinter die
    // Wetter-Wochenansicht als allerletzte Seite (Auftrag "Neuer
    // CYD-Screen: Wetter-Wochenansicht"). Inhalt/Farben/
    // Aktualisierungslogik der einzelnen Seiten unveraendert, nur die
    // Position in diesem Array hat sich geaendert.
    constexpr Page PAGES[] = {
        {I18n::Str::PAGE_HOME_TITLE, HomePage::draw, HomePage::update, HomePage::onTap, HomePage::onLongPress, I18n::Str::HELP_BODY_PAGE_HOME},
        {I18n::Str::PAGE_PV_STATUS_TITLE, PvStatusPage::draw, PvStatusPage::update, PvStatusPage::onTap, nullptr, I18n::Str::HELP_BODY_PAGE_PV_STATUS},
        {I18n::Str::PAGE_BATTERY_TITLE, BatteryPage::draw, BatteryPage::update, BatteryPage::onTap, nullptr, I18n::Str::HELP_BODY_PAGE_BATTERY},
        {I18n::Str::PAGE_POWER_TITLE, PowerPage::draw, PowerPage::update, PowerPage::onTap, nullptr, I18n::Str::HELP_BODY_PAGE_POWER},
        {I18n::Str::PAGE_DAILY_TITLE, DailyCurvePage::draw, DailyCurvePage::update, nullptr, nullptr, I18n::Str::HELP_BODY_PAGE_DAILY},
        {I18n::Str::PAGE_YIELD_TITLE, YieldPage::draw, YieldPage::update, YieldPage::onTap, nullptr, I18n::Str::HELP_BODY_PAGE_YIELD},
        {I18n::Str::PAGE_YEARS_TITLE, YearsPage::draw, YearsPage::update, YearsPage::onTap, nullptr, I18n::Str::HELP_BODY_PAGE_YEARS},
        {I18n::Str::PAGE_WEEKWEATHER_TITLE, WeekPage::draw, WeekPage::update, nullptr, nullptr, I18n::Str::HELP_BODY_PAGE_WEEKWEATHER},
    };
    constexpr uint8_t PAGE_COUNT = sizeof(PAGES) / sizeof(PAGES[0]);
}

uint8_t count() { return PAGE_COUNT; }
const Page& get(uint8_t index) { return PAGES[index]; }

}
