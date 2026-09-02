# Flydar

A realtime flight radar for your desk. It shows the aircraft actually overhead
right now — north-up, on labelled distance rings, with the nearest few listed
beside them. Green ground, black ink.

It runs on a LilyGO T-Display-S3 and is standalone: plug it into any USB
charger and it works. No computer, no cloud service, no companion app.

![Six frames of the radar rendered from real Riga traffic](docs/preview.png)

```
   api.adsb.lol  ──HTTPS every 10s──▶  ESP32-S3
                                        ├── dead-reckons each aircraft at ~10fps
                                        └── draws the scope on a 320x170 panel
```

## What is on screen

- **Radar face**, north up, centred on your city. Three labelled rings at a
  third, two thirds, and the full selected range.
- **Aircraft** as arrowheads pointing along their real track. When four or
  fewer are in range, each is labelled with its callsign on the face itself.
  The nearest one is ringed.
- **Airport beacon** for the selected city's main airport, in red — the one
  colour on screen that is neither ground nor ink, so it never reads as
  traffic. Labelled with its IATA code when there is room beside it. Aircraft
  reporting themselves on the ground sit on top of it.
- **History trails**: a dashed line behind each aircraft showing roughly the
  last two and a half minutes of its flight, clipped to the outer ring.
- **Side panel** listing the four nearest, sorted by distance. Three lines
  each: callsign, then the route and operator in tiny text
  (`RIX>ARN airBaltic`), then distance, altitude in metres and `^` / `v` / `-`
  for climbing, descending or level. The nearest is inverted so you read it
  first. The header already carries the in-range count, so the panel spends its
  space on aircraft rather than a "+N more" footer.
- **Header** with the city, how many aircraft are in range, the range, a clock,
  and a status badge when something is wrong.

Between fetches every aircraft is advanced along its own track at its own
groundspeed and redrawn ten times a second, so the picture glides instead of
stepping. Each fetch eases it back onto truth.

## Hardware

| You need | Notes |
|---|---|
| LilyGO T-Display-S3 | ESP32-S3, 1.9" ST7789 IPS, 320x170. The non-touch version is fine. |
| USB-C cable | For flashing, and for power afterwards. |
| 2.4 GHz WiFi | The ESP32-S3 radio is 2.4 GHz only. |

## Setup

```bash
cp src/secrets.h.example src/secrets.h
```

Put your 2.4 GHz network name and password in `src/secrets.h`. That file is
git-ignored, so it is never committed.

```c
#define WIFI_SSID "your-network"
#define WIFI_PASS "your-password"
```

Flash it:

```bash
pio run -e flydar -t upload
pio device monitor -b 115200
```

If upload fails with `Invalid head of packet` or a serial sync error, put the
board in bootloader mode: hold BOOT, tap RST, release BOOT, then upload again.
Close any serial monitor holding the port first.

## Controls

| Button | Does |
|---|---|
| **BOOT** (GPIO0) | Next city. Riga, Vilnius, Tallinn, Kaunas, Helsinki, Stockholm, Warsaw. |
| **GPIO14** | Next range: 20 / 50 / 100 / 200 km. |

Both choices are saved to flash, so the radar comes back up where you left it.

## Configuring

Everything you would want to change lives in `src/config.h`.

**Your city.** Edit the `CITIES` table. Riga is index 0 and therefore the
default:

```c
static const City CITIES[] = {
    //  name       city centre            main airport
    {"RIGA",    56.9496, 24.1052, "RIX", 56.9236, 23.9711},
    {"VILNIUS", 54.6872, 25.2797, "VNO", 54.6341, 25.2858},
    // {"YOUR TOWN", lat, lon, "XXX", airportLat, airportLon},
};
```

Names render at text size 2, so keep them to about twelve characters. The
`test_cities` suite checks every entry — each airport must be 2-60 km from its
city centre, so a swapped lat/lon or a mistyped digit fails the build rather
than drawing an airport in the wrong country.

**Ranges.** `RANGES_KM` is the list the second button cycles.

**Colours.** The default is inverted radar — a green ground with black data on
top. Uncomment `CLASSIC_SCHEME` for the traditional black ground with green
data. Nothing else changes.

**Clock.** `TZ_STRING` is a POSIX timezone string, set to Latvia by default.

**Trail length.** `TRAIL_SAMPLE_MS` in `config.h` and `TRAIL_POINTS` in
`trails.h` multiply out to how far back a trail reaches — 32 points at 5 s is
about 2.5 minutes, roughly 30 km behind an airliner. Each point costs 8 bytes
per aircraft.

## Where the route and operator come from

The aircraft feed carries neither, so both are derived from the callsign.

**Operator** comes from a table in `src/airlines.cpp` mapping the three-letter
ICAO designator to a name — `BTI9UG` starts with `BTI`, which is airBaltic. The
table covers the carriers that actually appear over northern Europe plus the
major long-haul names. An unknown designator resolves to nothing rather than a
guess, and the panel falls back to the registration, then the aircraft type. A
wrong airline is worse than none, because the registration is still true.

**Route** comes from `vrs-standing-data.adsb.lol`, which serves a static JSON
file per callsign giving the airport pair (`BRU-RIX`, drawn as `BRU>RIX`).
A route does not change during a flight, so each callsign is looked up once and
cached — one extra request per newly seen aircraft, not one per refresh, and at
most one lookup per ten-second cycle so the panel fills in over a few refreshes
instead of firing a burst. Multi-leg services show all three legs
(`CAN>CKG>AMS`). Aircraft with no published route keep the operator line.

Note this host uses a different certificate authority from the aircraft feed,
so `src/cert_roots.h` pins both ISRG Root X1 (Let's Encrypt) and GTS Root R4
(Google Trust Services) as one concatenated PEM bundle.

## Data source

[adsb.lol](https://adsb.lol) — a free, community-fed ADS-B aggregator. No API
key, no account, no credit budget. The board asks for a radius matching the
selected range and polls every 10 seconds; a typical response is a few
kilobytes. Coverage comes from volunteer receivers, so an aircraft with no
nearby feeder will not appear.

TLS is verified against the pinned roots in `src/cert_roots.h`.

## When something is wrong

| Badge | Meaning |
|---|---|
| `WIFI` | Not associated, or no successful fetch yet. Reconnects on its own. |
| `STALE` | The last good fetch is over 30 s old. Aircraft keep gliding on dead reckoning. |
| `NO DATA` | Nothing good for over two minutes. The list is cleared rather than shown as fact. |
| `NO TRAFFIC` | The fetch worked; the sky is genuinely empty inside your range. |

## Layout of the code

Pure logic is kept free of Arduino headers so it can be tested on your machine.

| File | Does |
|---|---|
| `src/geo.cpp` | Projection, distance, bearing, dead reckoning, unit conversion. |
| `src/adsb.cpp` | JSON to aircraft, filtering, distance sort, staleness. |
| `src/trails.cpp` | Position history per aircraft, keyed by ICAO hex. |
| `src/airlines.cpp` | ICAO operator designator to airline name. |
| `src/routes.cpp` | Origin/destination lookup and its cache. |
| `src/radar_ui.cpp` | All drawing. |
| `src/settings.cpp` | Saves city and range to flash. |
| `src/buttons.cpp` | Debounce. |
| `src/config.h` | Everything configurable. |
| `src/display_config.h` | Bus, driver, pins, panel geometry. |
| `src/main.ino` | WiFi, the fetch task, the render loop. |

Fetching runs as its own FreeRTOS task pinned to core 0, so a slow or failing
request never stalls the animation.

## Tests

```bash
pio test -e native
```

67 unit tests covering the geometry, the ADS-B parser, the history trails, the
airline table, the route cache, and the city/airport table, run on your machine against real captured adsb.lol responses in
`test/fixtures/`.

## Previewing the UI without a board

```bash
sh tools/preview/build.sh
python3 tools/preview/montage.py .preview
```

This compiles `radar_ui.cpp` against a host stand-in for Arduino_GFX that uses
the same 5x7 font the panel does, renders six scenarios — real traffic, a busy
sky, and each failure state — and stitches them into `.preview/montage.png`.
It is how the layout was checked before any hardware was involved.

## Porting to another board

`src/display_config.h` holds the bus, driver, pins and panel geometry, and is
the only file that needs to change for another ESP32 with an ST7789 or ILI9341
panel. The layout constants at the bottom of `src/config.h` assume 320x170 and
would need adjusting for a different size.
