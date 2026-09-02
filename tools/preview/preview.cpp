// Renders radar_ui frames to PPM files so the layout can be checked without
// flashing the board. Build with tools/preview/build.sh.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "adsb.h"
#include "config.h"
#include "radar_ui.h"

static const geo::LatLon RIGA{56.9496, 24.1052};

static std::string readFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path.c_str()); return {}; }
    std::string out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    fclose(f);
    return out;
}

static void writePPM(const Arduino_GFX& g, const std::string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    fprintf(f, "P6\n%d %d\n255\n", g.width(), g.height());
    for (uint16_t p : g.pixels()) {
        unsigned char rgb[3] = {(unsigned char)(((p >> 11) & 0x1F) * 255 / 31),
                                (unsigned char)(((p >> 5) & 0x3F) * 255 / 63),
                                (unsigned char)((p & 0x1F) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void render(const std::string& out, const adsb::Snapshot& snapIn, int rangeKm,
                   const char* city, radar_ui::Status status, const char* clock) {
    adsb::Snapshot snap = snapIn;
    adsb::computeRelative(snap, RIGA);
    adsb::sortByDistance(snap);

    Arduino_GFX gfx(320, 170);
    radar_ui::Frame f{city, rangeKm, &snap, adsb::countWithin(snap, rangeKm), status, clock};
    radar_ui::draw(&gfx, f);
    writePPM(gfx, out);
    printf("%-28s range=%3dkm inRange=%d/%d\n", out.c_str(), rangeKm, f.inRange, snap.count);
}

// A busier sky than Riga happened to have when the fixtures were captured.
static adsb::Snapshot syntheticBusy() {
    std::string body = "{\"ac\":[";
    struct Row { const char* cs; const char* type; double brg, km, alt, track, rate; };
    const Row rows[] = {
        {"BTI1PA", "A220", 15,  8.0, 4100,  200, 1800},
        {"RYR9JC", "B738", 95, 18.0, 11000, 260, -1600},
        {"SAS742",  "A20N", 160, 27.0, 22000, 340, 0},
        {"AFL2311", "B77W", 220, 41.0, 34000, 45,  0},
        {"DLH88X",  "A343", 275, 55.0, 29000, 120, -400},
        {"BTI711",  "BCS3", 320, 66.0, 18050, 190, 900},
        {"UZB211",  "B763", 40,  78.0, 20175, 250, 0},
        {"FIN1122", "E190", 200, 88.0, 9000,  10,  1200},
        {"YLEVI",   "C172", 350, 95.0, 300,   80,  0},
    };
    bool first = true;
    for (const Row& r : rows) {
        double t = r.brg * M_PI / 180.0;
        double north = r.km * cos(t), east = r.km * sin(t);
        double lat = RIGA.lat + north / geo::KM_PER_DEG_LAT;
        double lon = RIGA.lon + east / geo::kmPerDegLon(RIGA.lat);
        char one[256];
        snprintf(one, sizeof(one),
                 "%s{\"hex\":\"4%05x\",\"flight\":\"%s\",\"t\":\"%s\",\"lat\":%.6f,"
                 "\"lon\":%.6f,\"alt_baro\":%.0f,\"gs\":420,\"track\":%.1f,\"baro_rate\":%.0f}",
                 first ? "" : ",", (unsigned)(r.km * 100), r.cs, r.type, lat, lon, r.alt,
                 r.track, r.rate);
        body += one;
        first = false;
    }
    body += "]}";
    adsb::Snapshot s;
    adsb::parse(body.c_str(), body.size(), s);
    return s;
}

int main(int argc, char** argv) {
    std::string outDir = argc > 1 ? argv[1] : ".";
    std::string fixtures = argc > 2 ? argv[2] : "test/fixtures";

    adsb::Snapshot real;
    std::string body = readFile(fixtures + "/riga_5ac.json");
    if (!body.empty()) adsb::parse(body.c_str(), body.size(), real);

    adsb::Snapshot busy = syntheticBusy();
    adsb::Snapshot empty;

    render(outDir + "/01_real_200km.ppm", real, 200, "RIGA", radar_ui::Status::Ok, "12:04");
    render(outDir + "/02_real_50km.ppm", real, 50, "RIGA", radar_ui::Status::Ok, "12:04");
    render(outDir + "/03_busy_100km.ppm", busy, 100, "RIGA", radar_ui::Status::Ok, "12:04");
    render(outDir + "/04_busy_20km.ppm", busy, 20, "RIGA", radar_ui::Status::Ok, "12:04");
    render(outDir + "/05_connecting.ppm", empty, 50, "RIGA", radar_ui::Status::Connecting, "");
    render(outDir + "/06_stale.ppm", busy, 100, "STOCKHOLM", radar_ui::Status::Stale, "23:59");
    return 0;
}
