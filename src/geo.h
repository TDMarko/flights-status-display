#pragma once
// Arduino-free geometry for the radar: local-tangent projection, distance,
// bearing, dead reckoning and the unit conversions ADS-B needs.
// Host-testable — do not include Arduino headers here.

namespace geo {

struct LatLon { double lat; double lon; };

// Kilometres east (x) and north (y) of the radar centre.
struct Vec2 { double x; double y; };

struct ScreenPt { int x; int y; };

constexpr double KM_PER_DEG_LAT = 110.574;
constexpr double KM_PER_DEG_LON_EQ = 111.320;

// Kilometres per degree of longitude at the given latitude.
double kmPerDegLon(double latDeg);

// Equirectangular projection about `origin`. Accurate to well under a pixel
// for the ranges this radar draws (<= 200 km).
Vec2 project(LatLon origin, LatLon p);

double distanceKm(LatLon origin, LatLon p);

// 0 = due north, increasing clockwise, always in [0, 360).
double bearingDeg(LatLon origin, LatLon p);

// Map a local offset onto the radar face. North is up, so screen y is negated.
ScreenPt toScreen(Vec2 km, double rangeKm, int cx, int cy, int radiusPx);

// Advance a position along `trackDeg` at `gsKnots` for `dtSec` seconds.
LatLon advance(LatLon p, double trackDeg, double gsKnots, double dtSec);

// Smallest absolute difference between two compass angles, in [0, 180].
double angleDiffDeg(double a, double b);

// Wraps any angle into [0, 360).
double normalizeDeg(double deg);

double knotsToKmh(double knots);
double feetToMeters(double feet);

}  // namespace geo
