#include "geo.h"

#include <cmath>

namespace geo {

namespace {
constexpr double DEG2RAD = 3.14159265358979323846 / 180.0;
constexpr double RAD2DEG = 180.0 / 3.14159265358979323846;
constexpr double KM_PER_KNOT_SEC = 1.852 / 3600.0;  // km travelled per second at 1 kt
}  // namespace

double kmPerDegLon(double latDeg) {
    return KM_PER_DEG_LON_EQ * std::cos(latDeg * DEG2RAD);
}

Vec2 project(LatLon origin, LatLon p) {
    return {(p.lon - origin.lon) * kmPerDegLon(origin.lat),
            (p.lat - origin.lat) * KM_PER_DEG_LAT};
}

double distanceKm(LatLon origin, LatLon p) {
    Vec2 v = project(origin, p);
    return std::hypot(v.x, v.y);
}

double normalizeDeg(double deg) {
    deg = std::fmod(deg, 360.0);
    if (deg < 0.0) deg += 360.0;
    return deg;
}

double angleDiffDeg(double a, double b) {
    double d = std::fabs(normalizeDeg(a) - normalizeDeg(b));
    return d > 180.0 ? 360.0 - d : d;
}

double bearingDeg(LatLon origin, LatLon p) {
    Vec2 v = project(origin, p);
    // x (east) first, so 0 is due north and the angle grows clockwise.
    return normalizeDeg(std::atan2(v.x, v.y) * RAD2DEG);
}

ScreenPt toScreen(Vec2 km, double rangeKm, int cx, int cy, int radiusPx) {
    if (rangeKm <= 0.0) return {cx, cy};
    double scale = radiusPx / rangeKm;
    return {cx + (int)std::lround(km.x * scale),
            cy - (int)std::lround(km.y * scale)};
}

LatLon advance(LatLon p, double trackDeg, double gsKnots, double dtSec) {
    if (gsKnots <= 0.0 || dtSec == 0.0) return p;
    double km = gsKnots * KM_PER_KNOT_SEC * dtSec;
    double t = trackDeg * DEG2RAD;
    double north = km * std::cos(t);
    double east = km * std::sin(t);
    double lonScale = kmPerDegLon(p.lat);
    if (lonScale < 1e-6) lonScale = 1e-6;  // guard at the poles
    return {p.lat + north / KM_PER_DEG_LAT, p.lon + east / lonScale};
}

double knotsToKmh(double knots) { return knots * 1.852; }
double feetToMeters(double feet) { return feet * 0.3048; }

}  // namespace geo
