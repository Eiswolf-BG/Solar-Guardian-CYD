#pragma once
// Zusaetzliche, noch nicht vollstaendig bekannte HTTP-Header fuer die
// Port-7002-Abfrage (siehe epever_client.cpp::fetchBatteryData() und
// CLAUDE.md "Solar Guardian Portal API"). Laut Browser-Netzwerk-Tab
// schickt der Preflight (OPTIONS) genau diese fuenf Header an: Content-
// Type, languagetype, sdk-version, token, usr-source. "token" (roher
// JWT, kein "Bearer") wird direkt im Code gesetzt - die drei anderen
// sind hier als Konstanten vorbereitet, Standardwert leer. Ein leerer
// Wert wird NICHT als Header gesendet (siehe httpPostJson()).
namespace EpeverAuthHeaders {
    constexpr const char* LANGUAGETYPE = "";
    constexpr const char* SDK_VERSION  = "";
    constexpr const char* USR_SOURCE   = "";
}
