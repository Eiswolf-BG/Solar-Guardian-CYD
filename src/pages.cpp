#include "pages.h"
#include "display.h"
#include "app_config.h"
#include "yield_page.h"
#include "years_page.h"
#include "battery_page.h"
#include "i18n.h"
#include "config.h"

namespace Pages {

namespace {
    // Platzhalter-Zeichenfunktion fuer die noch unveraenderten Seiten
    // "Leistung"/"Batterie" - zeigt nur die Ueberschrift zentriert oben im
    // Seitenbereich und darunter dezent "folgt". Ohne eingerichtetes Konto
    // zusaetzlich ein dezenter Hinweis darauf, weil ohne Zugangsdaten nie
    // Daten ankommen koennen.
    void drawPlaceholder(I18n::Str titleKey, int16_t areaTop, int16_t areaHeight) {
        uint16_t accent = Display::systemColor();
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(I18n::t(titleKey), Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + 20));
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString(I18n::t(I18n::Str::COMING_SOON), Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + 50));

        bool hasAccount = appConfig.portalEmail.length() > 0 && appConfig.portalPassword.length() > 0;
        if (!hasAccount) {
            tft.drawString(I18n::t(I18n::Str::NO_ACCOUNT_CONFIGURED), Config::SCREEN_WIDTH / 2, (int16_t)(areaTop + 80));
        }
        tft.setTextDatum(TL_DATUM);
    }

    void drawLeistung(int16_t areaTop, int16_t areaHeight) { drawPlaceholder(I18n::Str::PAGE_POWER_TITLE, areaTop, areaHeight); }

    // Reihenfolge der Seiten: Leistung, Ertrag, Jahre, Batterie - zentral
    // an dieser einen Stelle, siehe CLAUDE.md.
    constexpr Page PAGES[] = {
        {I18n::Str::PAGE_POWER_TITLE, drawLeistung, nullptr, nullptr, I18n::Str::HELP_BODY_PAGE_POWER},
        {I18n::Str::PAGE_YIELD_TITLE, YieldPage::draw, YieldPage::update, YieldPage::onTap, I18n::Str::HELP_BODY_PAGE_YIELD},
        {I18n::Str::PAGE_YEARS_TITLE, YearsPage::draw, YearsPage::update, nullptr, I18n::Str::HELP_BODY_PAGE_YEARS},
        {I18n::Str::PAGE_BATTERY_TITLE, BatteryPage::draw, BatteryPage::update, BatteryPage::onTap, I18n::Str::HELP_BODY_PAGE_BATTERY},
    };
    constexpr uint8_t PAGE_COUNT = sizeof(PAGES) / sizeof(PAGES[0]);
}

uint8_t count() { return PAGE_COUNT; }
const Page& get(uint8_t index) { return PAGES[index]; }

}
