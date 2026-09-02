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
- **Side panel** listing the four nearest, sorted by distance: callsign,
  distance, altitude in metres, and `^` / `v` / `-` for climbing, descending,
  or level. The nearest is inverted so you read it first.
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
    {"RIGA",    56.9496, 24.1052},
    {"VILNIUS", 54.6872, 25.2797},
    // {"YOUR TOWN", latitude, longitude},
};
```

Names render at text size 2, so keep them to about twelve characters.

**Ranges.** `RANGES_KM` is the list the second button cycles.

**Colours.** The default is inverted radar — a green ground with black data on
top. Uncomment `CLASSIC_SCHEME` for the traditional black ground with green
data. Nothing else changes.

**Clock.** `TZ_STRING` is a POSIX timezone string, set to Latvia by default.

## Data source

[adsb.lol](https://adsb.lol) — a free, community-fed ADS-B aggregator. No API
key, no account, no credit budget. The board asks for a radius matching the
selected range and polls every 10 seconds; a typical response is a few
kilobytes. Coverage comes from volunteer receivers, so an aircraft with no
nearby feeder will not appear.

TLS is verified against a pinned ISRG Root X1 certificate (`src/cert_isrg.h`),
valid to 2035.

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

27 unit tests covering the geometry and the ADS-B parser, run on your machine
against real captured adsb.lol responses in `test/fixtures/`.

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
