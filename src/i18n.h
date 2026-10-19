#pragma once
// Zentrale, zweisprachige Textliste (Deutsch/Englisch) - siehe CLAUDE.md
// "Sprache". JEDER sichtbare App-Text bekommt hier einen Schluessel statt
// als Literal im jeweiligen Screen zu stehen. Ausnahmen (bewusst NICHT
// hier drin): der Produktname "Solar Guardian" (Eigenname, nicht
// uebersetzt), reine Tastatur-Symbole (abc/ABC/123/OK/X), und die
// SD-Karten-Fehlt-Meldung in main.cpp (die steht fest zweizeilig DE+EN,
// weil es zu diesem Zeitpunkt noch keine geladene Spracheinstellung gibt).
#include <Arduino.h>

namespace I18n {

    enum class Str : uint16_t {
        // Allgemein, mehrfach verwendet
        BACK,
        CANCEL,
        OK_BUTTON,
        CONNECT_BUTTON,
        DELETE_BUTTON,
        CONNECTING,
        CONNECTION_FAILED,
        FREE_SLOT_LABEL,
        EMAIL_LABEL,
        PASSWORD_LABEL,
        TAP_HERE,
        SPACE_KEY_LABEL,

        // Ersteinrichtung
        CONFIG_CORRUPTED,
        CONFIG_CORRUPTED_RESTART,
        CALIBRATION_TAP_CROSS_FMT,
        CALIBRATION_DONE,
        WIFI_SELECT_TITLE,
        SCANNING_WIFI,
        RESCAN_BUTTON,
        NO_NETWORKS_FOUND,
        PASSWORD_FOR_PROMPT_FMT,
        CREDENTIALS_TITLE,
        EMAIL_PROMPT,
        PASSWORD_PROMPT,
        GO_BUTTON,
        NEXT_BUTTON,
        FETCHING_TIME,

        // Seriennummer (Ersteinrichtung UND Konto-Screen, siehe
        // serial_number_screen.h)
        SERIAL_NUMBER_TITLE,
        SERIAL_NUMBER_PROMPT,
        SERIAL_NUMBER_HINT,
        SERIAL_NUMBER_TAP_HINT,
        SERIAL_NUMBER_SAVE_BUTTON,
        SERIAL_NUMBER_CHANGE_HINT,

        // Hauptbildschirm / Seiten
        PAGE_HOME_TITLE,
        PAGE_POWER_TITLE,
        PAGE_YIELD_TITLE,
        PAGE_YEARS_TITLE,
        PAGE_BATTERY_TITLE,
        COMING_SOON,
        NO_ACCOUNT_CONFIGURED,

        // Hauptscreen "Uebersicht" (4 Kacheln)
        HOME_WEATHER_SET_LOCATION,
        HOME_WEATHER_LOADING,

        // Seite "Leistung"
        POWER_UNIT_W,
        POWER_UNIT_KW,
        POWER_NOW_LABEL,
        POWER_ASOF_FMT,

        // Seite "Ertrag"
        YIELD_TILE_TODAY,
        YIELD_TILE_WEEK,
        YIELD_TILE_MONTH,
        YIELD_TILE_YEAR,
        KWH_UNIT,
        STATUS_UPDATED_FMT,
        STATUS_STALE_SUFFIX,
        STATUS_LOGGING_IN,
        STATUS_LOGIN_FAILED,
        STATUS_NETWORK_ERROR,

        // Seite "Batterie"
        BATTERY_TILE_VOLTAGE,
        BATTERY_TILE_CURRENT,
        BATTERY_TILE_TEMP,
        BATTERY_TILE_STATUS,
        VOLT_UNIT,
        AMP_UNIT,
        CELSIUS_UNIT,
        CHARGE_STATUS_0,
        CHARGE_STATUS_1,
        CHARGE_STATUS_2,
        CHARGE_STATUS_3,
        CHARGE_STATUS_WEAK,
        BATTERY_EXPLAIN_0,
        BATTERY_EXPLAIN_1,
        BATTERY_EXPLAIN_2,
        BATTERY_EXPLAIN_3,
        BATTERY_EXPLAIN_WEAK,
        NEXT_FETCH_FMT,

        // Wochenansicht (Wetterkachel antippen)
        WEEK_DAY_TOMORROW,
        HOURS_UNIT,
        WEEKDAY_SUN,
        WEEKDAY_MON,
        WEEKDAY_TUE,
        WEEKDAY_WED,
        WEEKDAY_THU,
        WEEKDAY_FRI,
        WEEKDAY_SAT,

        // Menue
        MENU_TITLE,
        MENU_ITEM_NETWORKS,
        MENU_ITEM_ACCOUNT,
        MENU_ITEM_COLOR,
        MENU_ITEM_LANGUAGE,
        MENU_ITEM_YIELD_VALUES,
        MENU_ITEM_RESET,

        // Netzwerke
        NETWORKS_TITLE,
        CHANGE_PASSWORD_BUTTON,
        NEW_PASSWORD_FOR_PROMPT_FMT,

        // Systemfarbe
        COLOR_PICKER_TITLE,

        // Konto
        ACCOUNT_TITLE,
        CHANGE_USERNAME_BUTTON,
        CHANGE_USERNAME_PROMPT,
        DELETE_ACCOUNT_BUTTON,
        DELETE_ACCOUNT_WARNING_TITLE,
        DELETE_ACCOUNT_WARNING_BODY,

        // Sprache
        LANGUAGE_MENU_ITEM,
        LANGUAGE_SCREEN_TITLE,
        GERMAN_LABEL,
        ENGLISH_LABEL,

        // Standort (Wetter)
        LOCATION_TITLE,
        LOCATION_SEARCH_PROMPT,
        LOCATION_SEARCHING,
        LOCATION_NOTHING_FOUND,
        LOCATION_NO_UMLAUT_HINT,
        LOCATION_RESULTS_TITLE,
        LOCATION_AUTO_DETECT_BUTTON,
        LOCATION_CHANGE_HINT,

        // Zuruecksetzen
        RESET_WARNING_TITLE,
        RESET_WARNING_BODY,
        RESET_CONFIRM_BUTTON,
        DELETING_TEXT,
        RESET_FAILED_TEXT,

        // "Ertragswerte"-Menue
        SET_MONTH_BUTTON,
        SET_YEAR_BUTTON,
        VALUE_INCLUDING_TODAY_NOTE,
        SET_VALUE_PROMPT,
        INVALID_INPUT_TEXT,
        NEGATIVE_RESULT_TEXT,

        // Hilfetexte ("?"-Knopf)
        HELP_BODY_PAGE_HOME,
        HELP_BODY_PAGE_POWER,
        HELP_BODY_PAGE_YIELD,
        HELP_BODY_PAGE_YEARS,
        HELP_BODY_PAGE_BATTERY,
        HELP_BODY_MENU,
        HELP_BODY_NETWORKS,
        HELP_BODY_WIFI_SELECT,
        HELP_BODY_CREDENTIALS,
        HELP_BODY_ACCOUNT,
        HELP_BODY_SERIAL_NUMBER,
        HELP_BODY_COLOR_PICKER,
        HELP_BODY_LANGUAGE,
        HELP_BODY_RESET,
        HELP_BODY_YIELD_VALUES,
        HELP_BODY_LOCATION,

        COUNT
    };

    enum class Language : uint8_t { German = 0, English = 1 };

    // Liest den aktuellen Text in appConfig.language (siehe app_config.h).
    const char* t(Str id);
}
