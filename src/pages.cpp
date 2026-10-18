#include "pages.h"
#include "power_page.h"
#include "yield_page.h"
#include "years_page.h"
#include "battery_page.h"
#include "i18n.h"

namespace Pages {

namespace {
    // Reihenfolge der Seiten: Leistung, Ertrag, Jahre, Batterie - zentral
    // an dieser einen Stelle, siehe CLAUDE.md.
    constexpr Page PAGES[] = {
        {I18n::Str::PAGE_POWER_TITLE, PowerPage::draw, PowerPage::update, PowerPage::onTap, I18n::Str::HELP_BODY_PAGE_POWER},
        {I18n::Str::PAGE_YIELD_TITLE, YieldPage::draw, YieldPage::update, YieldPage::onTap, I18n::Str::HELP_BODY_PAGE_YIELD},
        {I18n::Str::PAGE_YEARS_TITLE, YearsPage::draw, YearsPage::update, nullptr, I18n::Str::HELP_BODY_PAGE_YEARS},
        {I18n::Str::PAGE_BATTERY_TITLE, BatteryPage::draw, BatteryPage::update, BatteryPage::onTap, I18n::Str::HELP_BODY_PAGE_BATTERY},
    };
    constexpr uint8_t PAGE_COUNT = sizeof(PAGES) / sizeof(PAGES[0]);
}

uint8_t count() { return PAGE_COUNT; }
const Page& get(uint8_t index) { return PAGES[index]; }

}
