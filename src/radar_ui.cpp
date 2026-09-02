#include "radar_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "geo.h"
#include "airlines.h"
#include "routes.h"
#include "trails.h"

namespace radar_ui {

namespace {

constexpr int CH_W1 = 6, CH_H1 = 8;    // glyph box at text size 1
constexpr int CH_W2 = 12, CH_H2 = 16;  // ... and at size 2

int textW(const char* s, int size) {
    return (int)strlen(s) * (size == 2 ? CH_W2 : CH_W1);
}

void text(Arduino_GFX* g, int x, int y, int size, uint16_t colour, const char* s) {
    g->setTextSize(size);
    g->setTextColor(colour);
    g->setCursor(x, y);
    g->print(s);
}

void textRight(Arduino_GFX* g, int right, int y, int size, uint16_t colour, const char* s) {
    text(g, right - textW(s, size), y, size, colour, s);
}

// Black block with green lettering — used for the nearest aircraft and for the
// status badge, so they read as "picked out" rather than merely different.
void chipC(Arduino_GFX* g, int x, int y, int w, int h, int size, const char* s,
           uint16_t bg, uint16_t fg) {
    g->fillRect(x, y, w, h, bg);
    text(g, x + 3, y + (h - (size == 2 ? CH_H2 : CH_H1)) / 2, size, fg, s);
}

void chip(Arduino_GFX* g, int x, int y, int w, int h, int size, const char* s) {
    chipC(g, x, y, w, h, size, s, C_CHIP_BG, C_CHIP_FG);
}

const char* statusBadge(Status s) {
    switch (s) {
        case Status::NoWifi:     return "NO WIFI";
        case Status::Connecting: return "CONNECTING";
        case Status::Stale:      return "STALE";
        case Status::NoData:     return "NO DATA";
        default:                 return nullptr;
    }
}

// Altitude as metres, or GND for an aircraft reporting itself on the ground.
void formatAltitude(const adsb::Aircraft& a, char* out, size_t n) {
    if (a.onGround) { snprintf(out, n, "GND"); return; }
    if (!a.hasAlt)  { snprintf(out, n, "--"); return; }
    snprintf(out, n, "%dm", (int)lround(geo::feetToMeters(a.altFt)));
}

char verticalArrow(const adsb::Aircraft& a) {
    if (a.baroRateFpm > 200.0f) return '^';
    if (a.baroRateFpm < -200.0f) return 'v';
    return '-';
}

// The panel is 22 characters wide at text size 1. Compose the most useful
// identity that fits: route plus operator when both are known and fit, then
// route alone, then operator, then registration, then aircraft type. The route
// is never dropped to make room for the operator — it is the more specific
// fact, and the operator is already implied by the callsign prefix.
constexpr int PANEL_CHARS = PANEL_W / CH_W1;

void formatIdentity(const adsb::Aircraft& a, char* out, size_t n) {
    const char* route = routes::lookup(a.callsign);   // nullptr = not looked up yet
    const char* airline = airlines::fromCallsign(a.callsign);
    bool haveRoute = route && route[0];

    if (haveRoute && airline &&
        (int)(strlen(route) + 1 + strlen(airline)) <= PANEL_CHARS) {
        snprintf(out, n, "%s %s", route, airline);
    } else if (haveRoute) {
        snprintf(out, n, "%s", route);
    } else if (airline) {
        snprintf(out, n, "%s", airline);
    } else if (a.reg[0]) {
        snprintf(out, n, "%s", a.reg);
    } else {
        snprintf(out, n, "%s", a.type);
    }
}

void formatDistance(float km, char* out, size_t n) {
    if (km < 100.0f) snprintf(out, n, "%.1fkm", km);
    else             snprintf(out, n, "%dkm", (int)lround(km));
}

// Linear blend of two RGB565 colours; t=0 gives a, t=1 gives b. Each channel
// has to be unpacked because the bit widths differ (5/6/5).
uint16_t blend565(uint16_t a, uint16_t b, float t) {
    int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    int r = (int)lround(ar + (br - ar) * t);
    int gg = (int)lround(ag + (bg - ag) * t);
    int bl = (int)lround(ab + (bb - ab) * t);
    return (uint16_t)((r << 11) | (gg << 5) | bl);
}

// The rotating arm, drawn as a fan of lines fading back into the ground. It is
// painted before the grid so it passes underneath the rings and the data
// instead of scrubbing over them.
void drawSweep(Arduino_GFX* g, const Frame& f) {
    if (f.sweepDeg < 0.0f) return;

    const double step = SWEEP_TAIL_DEG / SWEEP_TAIL_STEPS;
    for (int k = SWEEP_TAIL_STEPS - 1; k >= 0; k--) {
        float t = (float)k / (float)SWEEP_TAIL_STEPS;   // 0 at the leading edge
        double a0 = f.sweepDeg - (k + 1.4) * step;      // slivers overlap slightly,
        double a1 = f.sweepDeg - k * step;              // or thin ones drop scanlines
        double r0 = a0 * M_PI / 180.0, r1 = a1 * M_PI / 180.0;
        // Filled slivers rather than a fan of lines: lines separate near the rim
        // and leave the tail visibly striped.
        g->fillTriangle(RADAR_CX, RADAR_CY,
                        RADAR_CX + (int)lround(RADAR_R * sin(r0)),
                        RADAR_CY - (int)lround(RADAR_R * cos(r0)),
                        RADAR_CX + (int)lround(RADAR_R * sin(r1)),
                        RADAR_CY - (int)lround(RADAR_R * cos(r1)),
                        blend565(C_SWEEP, C_GROUND, t));
    }
}

// Only ever paints inside the outer ring, so a trail running in from beyond the
// selected range cannot scribble across the header or the side panel.
void plotInScope(Arduino_GFX* g, int x, int y, uint16_t colour) {
    int dx = x - RADAR_CX, dy = y - RADAR_CY;
    if (dx * dx + dy * dy > RADAR_PLOT_R * RADAR_PLOT_R) return;
    if (y < HEADER_H) return;
    g->drawPixel(x, y, colour);
}

constexpr double DASH_ON = 3.0, DASH_OFF = 3.0;

// Places a size-1 label beside a point on the radar, kept inside the radar
// block so it can never bleed into the side panel. Returns false if it will not
// fit at all.
bool placeLabel(int x, int y, int w, int& lx, int& ly) {
    ly = y + 2;
    // Keep clear of the ring-distance strip below the east axis.
    if (ly + CH_H1 > RADAR_CY + 2 && ly < RADAR_CY + CH_H1 + 4) ly = RADAR_CY - CH_H1 - 4;
    if (ly < HEADER_H + 1) ly = HEADER_H + 1;
    if (ly > 170 - CH_H1) ly = 170 - CH_H1;

    const int left = 1, right = PANEL_X - 6;

    lx = x + 7;
    if (lx + w > right) lx = x - 7 - w;   // flip to the inboard side
    if (lx < left) lx = left;
    return lx >= left && lx + w <= right;
}

// Walks one segment pixel by pixel, painting the "on" part of the dash pattern.
// `phase` carries across segments so the dashes stay evenly spaced along the
// whole trail rather than restarting at every vertex.
double dashSegment(Arduino_GFX* g, double x0, double y0, double x1, double y1,
                   uint16_t colour, double phase) {
    double dx = x1 - x0, dy = y1 - y0;
    double len = hypot(dx, dy);
    if (len < 0.01) return phase;
    int steps = (int)ceil(len);
    for (int i = 0; i <= steps; i++) {
        double t = (double)i / steps;
        if (fmod(phase + t * len, DASH_ON + DASH_OFF) < DASH_ON) {
            plotInScope(g, (int)lround(x0 + dx * t), (int)lround(y0 + dy * t), colour);
        }
    }
    return phase + len;
}

// A little arrowhead pointing along the aircraft's track. Screen y grows
// downward, so north (track 0) is (0, -1).
void drawPlane(Arduino_GFX* g, int x, int y, float trackDeg, uint16_t colour) {
    double t = trackDeg * M_PI / 180.0;
    double fx = sin(t), fy = -cos(t);   // forward
    double px = cos(t), py = sin(t);    // right of forward
    int tipX = x + (int)lround(fx * 6.0), tipY = y + (int)lround(fy * 6.0);
    int aX = x + (int)lround(-fx * 4.0 + px * 3.5), aY = y + (int)lround(-fy * 4.0 + py * 3.5);
    int bX = x + (int)lround(-fx * 4.0 - px * 3.5), bY = y + (int)lround(-fy * 4.0 - py * 3.5);
    g->fillTriangle(tipX, tipY, aX, aY, bX, bY, colour);
}

// A red beacon: filled centre, outer ring, and a runway bar. Red is the one
// colour on screen that is neither ground nor ink, so the eye finds the airport
// instantly and never confuses it with traffic.
void drawAirport(Arduino_GFX* g, const Frame& f) {
    if (!f.airportCode || !f.airportCode[0]) return;
    if (f.airportDistKm > (float)f.rangeKm) return;

    double t = f.airportBearingDeg * M_PI / 180.0;
    double scale = (double)RADAR_R / (double)f.rangeKm;
    int x = RADAR_CX + (int)lround(f.airportDistKm * scale * sin(t));
    int y = RADAR_CY - (int)lround(f.airportDistKm * scale * cos(t));

    g->drawCircle(x, y, 5, C_AIRPORT);
    g->drawLine(x - 4, y + 4, x + 4, y - 4, C_AIRPORT);  // the runway
    g->fillCircle(x, y, 2, C_AIRPORT);

    // At wide ranges the airport collapses onto the "you are here" marker, and a
    // label there lands on the centre dot or the ring numbers. Draw the symbol
    // regardless, but only name it when there is genuinely room beside it.
    double fromCentre = hypot((double)(x - RADAR_CX), (double)(y - RADAR_CY));
    if (fromCentre < 18.0) return;

    int lx, ly;
    if (placeLabel(x, y, textW(f.airportCode, 1), lx, ly)) {
        text(g, lx, ly, 1, C_AIRPORT, f.airportCode);
    }
}

// Home is a blue diamond. Red marks a place aircraft go; blue marks where you
// are standing, and the two must never be confusable at a glance.
void drawHome(Arduino_GFX* g, const Frame& f) {
    if (!f.homeValid || f.homeDistKm > (float)f.rangeKm) return;

    double t = f.homeBearingDeg * M_PI / 180.0;
    double scale = (double)RADAR_R / (double)f.rangeKm;
    int x = RADAR_CX + (int)lround(f.homeDistKm * scale * sin(t));
    int y = RADAR_CY - (int)lround(f.homeDistKm * scale * cos(t));

    g->drawLine(x - 5, y, x, y - 5, C_HOME);
    g->drawLine(x, y - 5, x + 5, y, C_HOME);
    g->drawLine(x + 5, y, x, y + 5, C_HOME);
    g->drawLine(x, y + 5, x - 5, y, C_HOME);
    g->fillCircle(x, y, 1, C_HOME);

    // With an aircraft overhead, the label lands on that aircraft's own callsign
    // and the two become unreadable. The blue rings and the header chip already
    // say where it is.
    if (f.overheadCallsign && f.overheadCallsign[0]) return;

    double fromCentre = hypot((double)(x - RADAR_CX), (double)(y - RADAR_CY));
    if (fromCentre < 18.0) return;   // the label would sit on the centre marker

    int lx, ly;
    if (placeLabel(x, y, textW(HOME_LABEL, 1), lx, ly)) {
        text(g, lx, ly, 1, C_HOME, HOME_LABEL);
    }
}

void drawHeader(Arduino_GFX* g, const Frame& f) {
    // Row one: the city, large, and the clock. Row two: everything small —
    // aircraft count, range, and the airport's weather.
    text(g, 4, 0, 2, C_INK, f.cityName);
    if (f.clock && f.clock[0]) textRight(g, 316, 1, 2, C_INK, f.clock);

    // Something directly above you is the most interesting thing the radar can
    // say, so it gets the large row and the third colour.
    if (f.overheadCallsign && f.overheadCallsign[0]) {
        char over[20];
        snprintf(over, sizeof(over), "^ %s", f.overheadCallsign);
        int w = textW(over, 2) + 6;
        int x = 4 + textW(f.cityName, 2) + 10;
        int limit = 316 - (f.clock && f.clock[0] ? textW(f.clock, 2) + 8 : 0);
        if (x + w > limit) x = limit - w;
        if (x > 4 + textW(f.cityName, 2) + 4) chipC(g, x, 0, w, CH_H2, 2, over, C_HOME, C_GROUND);
    }

    char line[56];
    int used = snprintf(line, sizeof(line), "%d AC  %dkm", f.inRange, f.rangeKm);
    if (f.weather && f.weather[0]) {
        snprintf(line + used, sizeof(line) - used, "  %s", f.weather);
    }
    text(g, 4, 17, 1, C_TEXT_DIM, line);

    // Signal strength lives hard right, where it stays put instead of shifting
    // about as the weather line changes length.
    int right = 316;
    if (f.rssiDbm != 0) {
        char rssi[12];
        snprintf(rssi, sizeof(rssi), "%ddBm", f.rssiDbm);
        textRight(g, right, 17, 1, C_TEXT_DIM, rssi);
        right -= textW(rssi, 1) + 8;
    }

    const char* badge = statusBadge(f.status);
    if (badge) {
        int w = textW(badge, 1) + 6;
        chip(g, right - w, 16, w, 10, 1, badge);
    }

    g->drawFastHLine(0, HEADER_H - 1, 320, C_GRID);
}

void drawRadarGrid(Arduino_GFX* g, int rangeKm) {
    const int rings[3] = {RADAR_R / 3, (RADAR_R * 2) / 3, RADAR_R};
    for (int i = 0; i < 3; i++) g->drawCircle(RADAR_CX, RADAR_CY, rings[i], C_GRID);

    g->drawFastHLine(RADAR_CX - RADAR_R, RADAR_CY, RADAR_R * 2, C_GRID);
    g->drawFastVLine(RADAR_CX, RADAR_CY - RADAR_R, RADAR_R * 2, C_GRID);

    // Ring distances sit just *below* the east axis; the cardinal letters own the
    // strip above it, and putting both on the same side ran "200" into the "E".
    char buf[8];
    for (int i = 0; i < 3; i++) {
        int km = (int)lround((double)rangeKm * rings[i] / RADAR_R);
        snprintf(buf, sizeof(buf), "%d", km);
        text(g, RADAR_CX + rings[i] - textW(buf, 1) - 3, RADAR_CY + 3, 1, C_GRID, buf);
    }

    text(g, RADAR_CX + 4, RADAR_CY - RADAR_R + 1, 1, C_GRID, "N");
    text(g, RADAR_CX + 4, RADAR_CY + RADAR_R - CH_H1 - 1, 1, C_GRID, "S");
    text(g, RADAR_CX + RADAR_R - CH_W1 - 1, RADAR_CY - CH_H1 - 2, 1, C_GRID, "E");
    text(g, RADAR_CX - RADAR_R + 2, RADAR_CY - CH_H1 - 2, 1, C_GRID, "W");

    // You are here.
    g->drawCircle(RADAR_CX, RADAR_CY, 4, C_INK);
    g->fillCircle(RADAR_CX, RADAR_CY, 2, C_INK);
}

// Where each aircraft has been, as a dashed line running up to its current
// position. Drawn before the aircraft so the arrowheads stay on top.
void drawTrails(Arduino_GFX* g, const Frame& f) {
    if (!f.snap) return;
    double scale = (double)RADAR_R / (double)f.rangeKm;
    trails::Point pts[trails::TRAIL_POINTS];

    for (int i = 0; i < f.snap->count; i++) {
        const adsb::Aircraft& a = f.snap->ac[i];
        int n = trails::fetch(a.hex, pts, trails::TRAIL_POINTS);
        if (n < 1) continue;

        double phase = 0.0;
        double px = RADAR_CX + pts[0].x * scale;
        double py = RADAR_CY - pts[0].y * scale;
        for (int k = 1; k < n; k++) {
            double qx = RADAR_CX + pts[k].x * scale;
            double qy = RADAR_CY - pts[k].y * scale;
            phase = dashSegment(g, px, py, qx, qy, C_TRAIL, phase);
            px = qx; py = qy;
        }

        // Join the newest sample to where the aircraft is right now, so the
        // trail always ends at the arrowhead instead of trailing a gap.
        double t = a.bearingDeg * M_PI / 180.0;
        dashSegment(g, px, py, RADAR_CX + a.distKm * scale * sin(t),
                    RADAR_CY - a.distKm * scale * cos(t), C_TRAIL, phase);
    }
}

void drawPlanes(Arduino_GFX* g, const Frame& f) {
    if (!f.snap) return;
    // With only a handful up, there is room to label them on the radar face
    // itself, the way a real scope does.
    bool labelOnFace = f.inRange > 0 && f.inRange <= 4;

    for (int i = 0; i < f.snap->count; i++) {
        const adsb::Aircraft& a = f.snap->ac[i];
        if (a.distKm > (float)f.rangeKm) continue;

        double t = a.bearingDeg * M_PI / 180.0;
        double scale = (double)RADAR_R / (double)f.rangeKm;
        int x = RADAR_CX + (int)lround(a.distKm * scale * sin(t));
        int y = RADAR_CY - (int)lround(a.distKm * scale * cos(t));

        bool overhead = f.overheadHex && f.overheadHex[0] && strcmp(a.hex, f.overheadHex) == 0;
        if (overhead) {
            g->drawCircle(x, y, 9, C_HOME);
            g->drawCircle(x, y, 12, C_HOME);
        } else if (i == 0) {
            g->drawCircle(x, y, 9, C_INK);  // the one the panel highlights
        }
        drawPlane(g, x, y, a.trackDeg, C_INK);

        // The overhead aircraft is already named in the header chip; labelling it
        // again here only crowds the airport and home markers it is sitting on.
        if (labelOnFace && !overhead) {
            int lx, ly;
            if (placeLabel(x, y - 5, textW(a.callsign, 1), lx, ly)) {
                text(g, lx, ly, 1, C_INK, a.callsign);
            }
        }
    }
}

void drawPanelEmpty(Arduino_GFX* g, const Frame& f) {
    const char* msg = "NO TRAFFIC";
    if (f.status == Status::NoWifi) msg = "NO WIFI";
    else if (f.status == Status::Connecting) msg = "CONNECTING";
    else if (f.status == Status::NoData) msg = "NO DATA";
    int x = PANEL_X + (PANEL_W - textW(msg, 1)) / 2;
    text(g, x, RADAR_CY - CH_H1 / 2, 1, C_TEXT_DIM, msg);
}

void drawPanel(Arduino_GFX* g, const Frame& f) {
    g->drawFastVLine(PANEL_X - 4, HEADER_H, 170 - HEADER_H, C_GRID);

    if (!f.snap || f.inRange == 0) { drawPanelEmpty(g, f); return; }

    // Three lines per entry: callsign, then route/operator, then the numbers.
    // The header already reports how many are in range, so the panel spends its
    // last rows on aircraft rather than on a "+N more" footer.
    const int rowH = 35;
    int y = HEADER_H + 4;
    int shown = 0;

    for (int i = 0; i < f.snap->count && shown < PANEL_ROWS; i++) {
        const adsb::Aircraft& a = f.snap->ac[i];
        if (a.distKm > (float)f.rangeKm) continue;

        char ident[PANEL_CHARS + 2], dist[12], alt[12], stats[28];
        formatIdentity(a, ident, sizeof(ident));
        formatDistance(a.distKm, dist, sizeof(dist));
        formatAltitude(a, alt, sizeof(alt));
        snprintf(stats, sizeof(stats), "%s %s %c", dist, alt, verticalArrow(a));

        if (shown == 0) {
            // Nearest aircraft: inverted, so it is the first thing you read.
            chip(g, PANEL_X - 2, y - 2, PANEL_W - 2, CH_H2 + 2, 2, a.callsign);
            text(g, PANEL_X, y + 17, 1, C_INK, ident);
            text(g, PANEL_X, y + 25, 1, C_INK, stats);
        } else {
            text(g, PANEL_X, y, 2, C_INK, a.callsign);
            text(g, PANEL_X, y + 17, 1, C_TEXT_DIM, ident);
            text(g, PANEL_X, y + 25, 1, C_TEXT_DIM, stats);
        }
        y += rowH;
        shown++;
    }
}

}  // namespace

void draw(Arduino_GFX* g, const Frame& f) {
    g->fillScreen(C_GROUND);
    drawHeader(g, f);
    drawSweep(g, f);
    drawRadarGrid(g, f.rangeKm);
    drawTrails(g, f);
    drawAirport(g, f);
    drawHome(g, f);
    drawPlanes(g, f);
    drawPanel(g, f);
}

}  // namespace radar_ui
