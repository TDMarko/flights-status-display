# Flydar — realtime flight radar on a LilyGO T-Display-S3

Date: 2026-09-02

## Purpose

A desk device that shows, at a glance, which aircraft are overhead right now.
A radar sweep centred on your city, planes drawn as heading-oriented icons on
labelled distance rings, and a panel listing the nearest few with callsign,
distance, altitude and vertical trend. It runs standalone: plug it into any USB
charger and it works. No computer, no cloud service, no companion app.

## Hardware

LilyGO T-Display-S3 — ESP32-S3, 16 MB flash, 8 MB PSRAM, ST7789 1.9" IPS panel
at 320x170, driven over an 8-bit parallel bus. Rotation 3 (landscape). Two
buttons to ground with internal pull-ups: GPIO0 (BOOT) and GPIO14.

Reused from `lily/T-Display-S3`: the PlatformIO environment (espressif32@6.5.0,
Arduino_GFX 1.4.2, ArduinoJson 7), `boards/lilygo-t-displays3.json`,
`src/display_config.h`, and the `Arduino_Canvas` off-screen framebuffer pattern
— every frame is composed in RAM and blitted in one pass, so nothing flickers.

## Colour scheme

Inverted relative to a classic radar: **green is the ground, black is the ink.**

| Token      | Role                                                        |
|------------|-------------------------------------------------------------|
| `C_GROUND` | Phosphor green. The entire background.                      |
| `C_INK`    | Black. Planes, callsigns, every piece of data.              |
| `C_GRID`   | Dark green. Rings, crosshair, ring labels — grid sits under the data. |
| `C_CHIP`   | Black fill with green text, for the nearest aircraft.       |

`config.h` defines `CLASSIC_SCHEME`. Defining it swaps ground and ink to the
traditional black-background/green-foreground look. No other code changes.

## Data source

`GET https://api.adsb.lol/v2/lat/<lat>/lon/<lon>/dist/<nmi>`

Free, no API key, no credit budget, and it accepts a radius query directly.
Measured against Riga: 0.8 KB for 1 aircraft, 3.6 KB for 5 — small enough that
buffering the whole body is fine. Polled every 10 s. TLS with the ISRG Root X1
certificate pinned (adsb.lol serves a Let's Encrypt chain; that root is valid
until 2035).

ArduinoJson parses with a **filter** so only these fields are retained:
`hex, flight, r, t, lat, lon, alt_baro, gs, track, baro_rate, seen_pos`.
Aircraft without a position are discarded. Capacity is capped at 32.

`alt_baro` is feet and may be the string `"ground"`; `gs` is knots; `track` is
degrees true. Altitude is converted to metres for display, groundspeed to km/h.

## Geometry

Equirectangular projection, accurate to well under a pixel at these ranges:

```
dx_km = (lon - lon0) * cos(lat0) * 111.32     // east positive
dy_km = (lat - lat0) * 110.574                // north positive
distance = hypot(dx, dy)
bearing  = atan2(dx, dy)                      // 0 = north, clockwise
screen_x = cx + (dx / range_km) * R           // north up
screen_y = cy - (dy / range_km) * R
```

**Dead reckoning.** Fetches are 10 s apart but the screen redraws at ~10 fps.
Between fetches each aircraft is advanced along its own track at its own
groundspeed:

```
v_kms = gs_kt * 1.852 / 3600
lat += v_kms * cos(track) * dt / 110.574
lon += v_kms * sin(track) * dt / (111.32 * cos(lat0))
```

so planes glide continuously and are corrected to truth on each fetch. An
aircraft that turns hard mid-interval drifts slightly and snaps back; at these
speeds and screen scales the error is a pixel or two.

## Controls

| Button        | Short press                                              |
|---------------|----------------------------------------------------------|
| GPIO0 (BOOT)  | Next city preset. Triggers an immediate refetch.          |
| GPIO14        | Next range: 20 / 50 / 100 / 200 km. Redraws immediately.  |

Both are debounced at 40 ms. City index and range index persist to NVS
(`Preferences`, namespace `flydar`) and are restored on boot.

City presets live in `config.h`: Riga (56.9496, 24.1052) first and default,
then Vilnius, Tallinn, Kaunas, Helsinki, Stockholm, Warsaw. Each entry also
carries its main airport (IATA code and position): RIX, VNO, TLL, KUN, HEL,
ARN, WAW.

The airport is drawn as a red beacon — ring, runway bar and filled centre —
beneath the aircraft layer so traffic stays dominant. Red is deliberately
outside the two-colour scheme: it is the only mark on screen that is neither
ground nor ink, so the eye never mistakes a fixed place for traffic. Its IATA code is drawn beside it only
when the symbol is at least 16 px from the radar centre; at wide ranges the
airport collapses onto the centre marker and a label there would sit on the
ring numbers.

## Layout — 320 x 170

```
 0                        176 180                  316
+-----------------------------+---------------------+ 0
| RIGA    5 AC    50km              12:04      [!]  |   header, 18 px
+-----------------------------+---------------------+ 18
|          ring 50 (dashed)   |  BTI1PA             |
|      ring 33      ring 17   |  4.2km  1250m  ^    |
|  -------- (you) --------    |  RYR9JC             |
|          planes as          |  11.8km 3400m  v    |
|      heading-rotated        |  BTI9KV             |
|      black triangles        |  22.0km 5500m  ^    |
|                             |  +2 more            |
+-----------------------------+---------------------+ 170
```

- Radar centre `cx = 88`, `cy = 95`, outer radius `R = 72`.
- Three rings at R/3, 2R/3, R, labelled with their distance in km.
- Crosshair with N/E/S/W ticks, north up. Centre marked with a small filled dot.
- Info panel `x = 180..316`. Up to four aircraft, nearest first. Per entry:
  callsign at text size 2, then `distance / altitude / vertical arrow` at
  size 1. The nearest is drawn as an inverted chip. A `+N more` footer when
  more are in range.
- Header: city name, aircraft count, current range, NTP clock (Europe/Riga),
  and a status badge.

## Modules

Pure logic is kept free of Arduino headers so it compiles and is tested on the
host.

| File                 | Responsibility                                                    |
|----------------------|-------------------------------------------------------------------|
| `src/geo.h/.cpp`     | Projection, distance, bearing, dead reckoning, unit conversion.    |
| `src/adsb.h/.cpp`    | JSON body to `Aircraft[]`, filtering, distance sort, staleness.    |
| `src/trails.h/.cpp`  | Per-aircraft position history, keyed by ICAO hex.                  |
| `src/radar_ui.h/.cpp`| All drawing against an `Arduino_GFX*`.                            |
| `src/settings.h/.cpp`| NVS load/save of city and range index.                            |
| `src/buttons.h/.cpp` | Debounce and edge detection for the two buttons.                  |
| `src/config.h`       | Cities, ranges, timings, colours, layout constants.               |
| `src/display_config.h`| Bus, driver, pins, panel geometry. From the reference project.   |
| `src/main.ino`       | Wiring: WiFi, NTP, poll loop, render loop.                        |

`geo` and `adsb` depend only on the C++ standard library and ArduinoJson, so
`pio test -e native` runs their unit tests on the development machine, using
real captured adsb.lol responses as fixtures (`test/fixtures/`). Display and
network code is thin and verified on hardware.

## History trails

Each aircraft leaves a dashed trail of where it has been. History cannot live
in the aircraft snapshot, because a snapshot is replaced wholesale on every
fetch; `trails` keeps a separate fixed table keyed by ICAO hex address.

Points are stored as kilometres east/north of the radar centre, which stays
valid as the range changes, and are cleared when the city changes. The table
holds 32 aircraft of 32 points sampled every 5 s — about two and a half
minutes, or roughly 30 km behind an airliner. A full table evicts the least
recently seen aircraft. An aircraft unseen for two minutes is forgotten.

Drawing walks the polyline pixel by pixel with a dash phase carried across
segments, so dashes stay evenly spaced rather than restarting at each vertex,
and paints only inside the outer ring — a trail running in from beyond the
selected range must not scribble across the header or the side panel. A final
segment joins the newest sample to the aircraft's current position so the trail
ends at the arrowhead.

## Operator and route

Neither is in the aircraft feed. Both are derived from the callsign.

`airlines` maps the three-letter ICAO operator designator to a name. A callsign
qualifies only when its first three characters are letters and the fourth is a
digit, which excludes registrations and private callsigns. An unknown
designator returns nothing rather than a guess; the panel then falls back to
the registration, then the aircraft type. Names are capped at 14 characters so
a route still fits beside them on a 22-character line, and the table is kept
sorted with tests enforcing sorting, uniqueness, ASCII and length.

`routes` fetches `https://vrs-standing-data.adsb.lol/routes/<AB>/<CALLSIGN>.json`,
where `<AB>` is the first two characters of the callsign, and keeps the answer.
Routes do not change mid-flight, so this is one request per newly seen
aircraft. At most one lookup runs per fetch cycle. Misses are cached too, or
every refresh would ask again. The parser must reject an HTML body, because
the host answers an unknown callsign with a landing page rather than a 404.

That host uses Google Trust Services while the aircraft feed uses Let's
Encrypt, so `cert_roots.h` pins both roots as one concatenated PEM bundle;
mbedTLS parses a chain of PEM blocks from a single buffer.

## Failure behaviour

| Condition                | Behaviour                                                          |
|--------------------------|--------------------------------------------------------------------|
| WiFi not connected       | Header badge `WIFI`. Reconnect attempts with backoff. Last known aircraft keep gliding. |
| Fetch or TLS failure     | Keep the previous aircraft list. Header badge `STALE` once data is older than 30 s. |
| Data older than 120 s    | Clear the aircraft list. Panel reads `NO DATA`.                    |
| Fetch succeeds, empty    | Rings drawn empty. Panel reads `NO TRAFFIC`.                       |
| More than 32 aircraft    | Keep the 32 nearest.                                               |
| Malformed JSON           | Treated as a fetch failure.                                        |

## Out of scope

No web configuration page, no map underlay, no flight history or trails, no
audio, no touch. Cities are compiled in and cycled by button, which is the
agreed interaction.
