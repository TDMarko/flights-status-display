#include "airlines.h"

#include <cstring>

namespace airlines {

namespace {

struct Entry {
    const char code[4];
    const char* name;
};

// ICAO three-letter operator designators. Kept to carriers that actually show
// up over northern Europe plus the major long-haul names, and deliberately
// short: the side panel is 22 characters wide at text size 1, and a route
// prefix ("BRU>RIX ") claims eight of them. test_airlines enforces the limit.
//
// Only designators worth being confident about are listed. A wrong airline name
// is worse than none, because the fallback (registration) is still true.
const Entry TABLE[] = {
    {"AAL", "American"},        {"ABY", "Air Arabia"},      {"ACA", "Air Canada"},
    {"AEE", "Aegean"},          {"AFL", "Aeroflot"},        {"AFR", "Air France"},
    {"AHY", "Azerbaijan"},      {"AIC", "Air India"},       {"ANA", "All Nippon"},
    {"ART", "SmartLynx"},       {"AUA", "Austrian"},        {"BAW", "Brit. Airways"},
    {"BCS", "DHL"},             {"BEL", "Brussels"},        {"BRU", "Belavia"},
    {"BRX", "Braathens"},       {"BTI", "airBaltic"},       {"CCA", "Air China"},
    {"CES", "China Eastern"},
    {"CFG", "Condor"},          {"CLX", "Cargolux"},        {"CSN", "China Southern"},
    {"DAL", "Delta"},           {"DLH", "Lufthansa"},       {"EJU", "easyJet Europe"},
    {"ELY", "El Al"},           {"ENT", "Enter Air"},       {"ETD", "Etihad"},
    {"ETH", "Ethiopian"},       {"EWG", "Eurowings"},       {"EZY", "easyJet"},
    {"FDB", "flydubai"},        {"FDX", "FedEx"},           {"FIN", "Finnair"},
    {"FPY", "Play"},            {"GEC", "LH Cargo"}, {"GFA", "Gulf Air"},
    {"IBE", "Iberia"},          {"ICE", "Icelandair"},      {"ITY", "ITA Airways"},
    {"JAL", "Japan Airlines"},  {"KAC", "Kuwait"},          {"KAL", "Korean Air"},
    {"KLC", "KLM Cityhopper"},  {"KLM", "KLM"},             {"LGL", "Luxair"},
    {"LOT", "LOT Polish"},      {"MSR", "EgyptAir"},        {"NAX", "Norwegian"},
    {"NSZ", "Norwegian"},       {"NVD", "Avion Express"},   {"NWS", "Nordwind"},
    {"OMA", "Oman Air"},        {"PBD", "Pobeda"},          {"PGT", "Pegasus"},
    {"QTR", "Qatar Airways"},   {"RJA", "R. Jordanian"}, {"RYR", "Ryanair"},
    {"SAS", "SAS"},             {"SBI", "S7 Airlines"},     {"SDM", "Rossiya"},
    {"SIA", "Singapore"},       {"SVA", "Saudia"},          {"SWR", "Swiss"},
    {"SXS", "SunExpress"},      {"TAP", "TAP Portugal"},    {"THY", "Turkish"},
    {"TOM", "TUI Airways"},     {"TRA", "Transavia"},       {"TUI", "TUIfly"},
    {"UAE", "Emirates"},        {"UAL", "United"},          {"UPS", "UPS"},
    {"UTA", "UTair"},           {"UZB", "Uzbekistan"},      {"VLG", "Vueling"},
    {"WIF", "Wideroe"},         {"WZZ", "Wizz Air"},
};
const int TABLE_LEN = (int)(sizeof(TABLE) / sizeof(TABLE[0]));

bool isUpper(char c) { return c >= 'A' && c <= 'Z'; }
bool isDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

const char* fromCallsign(const char* callsign) {
    if (!callsign) return nullptr;
    // ICAO airline format: three letters then a flight number. A registration
    // ("YL-AAR", "N159L") or a private callsign ("BRIO66") fails this and is
    // left for the caller to handle.
    if (!isUpper(callsign[0]) || !isUpper(callsign[1]) || !isUpper(callsign[2])) return nullptr;
    if (!isDigit(callsign[3])) return nullptr;

    for (int i = 0; i < TABLE_LEN; i++) {
        if (strncmp(TABLE[i].code, callsign, 3) == 0) return TABLE[i].name;
    }
    return nullptr;
}

int longestNameLength() {
    int longest = 0;
    for (int i = 0; i < TABLE_LEN; i++) {
        int n = (int)strlen(TABLE[i].name);
        if (n > longest) longest = n;
    }
    return longest;
}

int count() { return TABLE_LEN; }

const char* codeAt(int i) { return (i >= 0 && i < TABLE_LEN) ? TABLE[i].code : nullptr; }
const char* nameAt(int i) { return (i >= 0 && i < TABLE_LEN) ? TABLE[i].name : nullptr; }

}  // namespace airlines
