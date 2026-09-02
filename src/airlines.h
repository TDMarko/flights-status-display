#pragma once
// Operator names for ICAO callsigns.
//
// adsb.lol returns no operator field, only the callsign. An airline callsign is
// a three-letter ICAO operator designator followed by a flight number, so the
// prefix is enough to name the carrier. Arduino-free, host-testable.

namespace airlines {

// "BTI9UG" -> "airBaltic". Returns nullptr when the callsign is not in ICAO
// airline format (a registration or a private callsign) or the designator is
// not in the table. Never guesses: an unknown prefix returns nullptr so the
// caller can fall back to something factual rather than show a wrong airline.
const char* fromCallsign(const char* callsign);

// Longest name in the table, for layout checks in tests.
int longestNameLength();

// Number of entries, for tests.
int count();

// Table access by index, for tests. Null when out of range.
const char* codeAt(int i);
const char* nameAt(int i);

}  // namespace airlines
