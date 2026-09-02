#include "radar_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "geo.h"

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
void chip(Arduino_GFX* g, int x, int y, int w, int h, int size, const char* s) {
    g->fillRect(x, y, w, h, C_CHIP_BG);
    text(g, x + 3, y + (h - (size == 2 ? CH_H2 : CH_H1)) / 2, size, C_CHIP_FG, s);
}

const char* statusBadge(Status s) {
    switch (s) {
        case Status::Connecting: return "WIFI";
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

void formatDistance(float km, char* out, size_t n) {
    if (km < 100.0f) snprintf(out, n, "%.1fkm", km);
    else             snprintf(out, n, "%dkm", (int)lround(km));
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

// Aeronautical-chart style: a small ring with a runway bar through it. Drawn in
// the grid colour and before the aircraft, so it reads as geography rather than
// as traffic.
void drawAirport(Arduino_GFX* g, const Frame& f) {
    if (!f.airportCode || !f.airportCode[0]) return;
    if (f.airportDistKm > (float)f.rangeKm) return;

    double t = f.airportBearingDeg * M_PI / 180.0;
    double scale = (double)RADAR_R / (double)f.rangeKm;
    int x = RADAR_CX + (int)lround(f.airportDistKm * scale * sin(t));
    int y = RADAR_CY - (int)lround(f.airportDistKm * scale * cos(t));

    g->drawCircle(x, y, 4, C_GRID);
    g->drawLine(x - 3, y + 3, x + 3, y - 3, C_GRID);  // the runway

    // At wide ranges the airport collapses onto the "you are here" marker, and a
    // label there lands on the centre dot or the ring numbers. Draw the symbol
    // regardless, but only name it when there is genuinely room beside it.
    double fromCentre = hypot((double)(x - RADAR_CX), (double)(y - RADAR_CY));
    if (fromCentre < 16.0) return;

    int lx = x + 7, ly = y + 2;
    int w = textW(f.airportCode, 1);
    if (lx + w > RADAR_CX + RADAR_R + 4) lx = x - 7 - w;
    if (lx < 1) lx = 1;
    if (ly + CH_H1 > RADAR_CY + 2 && ly < RADAR_CY + CH_H1 + 4) ly = RADAR_CY - CH_H1 - 4;
    if (ly < HEADER_H + 1) ly = HEADER_H + 1;
    if (ly > 170 - CH_H1) ly = 170 - CH_H1;
    text(g, lx, ly, 1, C_GRID, f.airportCode);
}

void drawHeader(Arduino_GFX* g, const Frame& f) {
    text(g, 4, 1, 2, C_INK, f.cityName);
    int x = 4 + textW(f.cityName, 2) + 10;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d AC", f.inRange);
    text(g, x, 1, 1, C_INK, buf);
    snprintf(buf, sizeof(buf), "%dkm", f.rangeKm);
    text(g, x, 10, 1, C_GRID, buf);

    int right = 316;
    if (f.clock && f.clock[0]) {
        textRight(g, right, 2, 2, C_INK, f.clock);
        right -= textW(f.clock, 2) + 8;
    }
    const char* badge = statusBadge(f.status);
    if (badge) {
        int w = textW(badge, 1) + 6;
        chip(g, right - w, 4, w, 11, 1, badge);
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

        if (i == 0) g->drawCircle(x, y, 9, C_INK);  // the one the panel highlights
        drawPlane(g, x, y, a.trackDeg, C_INK);

        if (labelOnFace) {
            int lx = x + 8, ly = y - 3;
            int w = textW(a.callsign, 1);
            if (lx + w > RADAR_CX + RADAR_R + 4) lx = x - 8 - w;  // flip to the left edge
            if (lx < 1) lx = 1;
            // Keep clear of the ring-distance strip that runs below the east axis.
            if (ly + CH_H1 > RADAR_CY + 2 && ly < RADAR_CY + CH_H1 + 4) ly = RADAR_CY - CH_H1 - 4;
            if (ly < HEADER_H + 1) ly = HEADER_H + 1;
            if (ly > 170 - CH_H1) ly = 170 - CH_H1;
            text(g, lx, ly, 1, C_INK, a.callsign);
        }
    }
}

void drawPanelEmpty(Arduino_GFX* g, const Frame& f) {
    const char* msg = "NO TRAFFIC";
    if (f.status == Status::Connecting) msg = "CONNECTING";
    else if (f.status == Status::NoData) msg = "NO DATA";
    int x = PANEL_X + (PANEL_W - textW(msg, 1)) / 2;
    text(g, x, RADAR_CY - CH_H1 / 2, 1, C_GRID, msg);
}

void drawPanel(Arduino_GFX* g, const Frame& f) {
    g->drawFastVLine(PANEL_X - 4, HEADER_H, 170 - HEADER_H, C_GRID);

    if (!f.snap || f.inRange == 0) { drawPanelEmpty(g, f); return; }

    const int rowH = 34;
    int y = HEADER_H + 4;
    int shown = 0;

    for (int i = 0; i < f.snap->count && shown < PANEL_ROWS; i++) {
        const adsb::Aircraft& a = f.snap->ac[i];
        if (a.distKm > (float)f.rangeKm) continue;

        char dist[12], alt[12], line2[28];
        formatDistance(a.distKm, dist, sizeof(dist));
        formatAltitude(a, alt, sizeof(alt));
        snprintf(line2, sizeof(line2), "%s %s %c", dist, alt, verticalArrow(a));

        if (shown == 0) {
            // Nearest aircraft: inverted, so it is the first thing you read.
            chip(g, PANEL_X - 2, y - 2, PANEL_W - 2, CH_H2 + 4, 2, a.callsign);
            text(g, PANEL_X, y + CH_H2 + 5, 1, C_INK, line2);
        } else {
            text(g, PANEL_X, y, 2, C_INK, a.callsign);
            text(g, PANEL_X, y + CH_H2 + 3, 1, C_GRID, line2);
        }
        y += rowH;
        shown++;
    }

    int extra = f.inRange - shown;
    if (extra > 0) {
        char buf[20];
        snprintf(buf, sizeof(buf), "+%d more", extra);
        text(g, PANEL_X, 170 - CH_H1 - 2, 1, C_GRID, buf);
    }
}

}  // namespace

void draw(Arduino_GFX* g, const Frame& f) {
    g->fillScreen(C_GROUND);
    drawHeader(g, f);
    drawRadarGrid(g, f.rangeKm);
    drawAirport(g, f);
    drawPlanes(g, f);
    drawPanel(g, f);
}

}  // namespace radar_ui
