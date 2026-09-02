# Flydar

A live flight radar for your desk. It shows the aircraft actually overhead
right now — where they are, where they came from, and where they're going.

Runs standalone on a LilyGO T-Display-S3. Plug it into any USB charger and it
works. No computer, no cloud service, no companion app.

![Flydar showing traffic over Riga](docs/preview.png)

## What's on screen

- **Radar**, north up, on labelled distance rings, with a rotating sweep.
- **Aircraft** as arrowheads pointing along their real track, each trailing a
  dashed line of where it has been for the last couple of minutes.
- **Side panel** with the four nearest: callsign, route and airline
  (`RIX>ARN airBaltic`), distance, altitude, and climbing / descending.
- **Airport** in red, with the local weather in the header
  (`RIX SSW 4kt 19C BKN`).
- **Home** in blue, and when something passes within 3 km of it, a blue chip
  across the top telling you what's above you.

## You need

| | |
|---|---|
| LilyGO T-Display-S3 | ESP32-S3, 1.9" 320×170. Non-touch is fine. |
| USB-C cable | Flashing, then power. |
| 2.4 GHz WiFi | The ESP32-S3 radio can't see 5 GHz. |
| [PlatformIO](https://platformio.org/) | The CLI or the VS Code extension. |

## Quick start

```bash
git clone <this-repo> flydar && cd flydar
cp src/secrets.h.example src/secrets.h
```

Put your WiFi details in `src/secrets.h` — it's git-ignored, so it never gets
committed:

```c
#define WIFI_SSID "your-network"
#define WIFI_PASS "your-password"

// Optional: your own coordinates, for the blue home marker and the
// "something is overhead" alert.
#define HOME_LAT 56.9496
#define HOME_LON 24.1052
```

Flash it:

```bash
pio run -t upload
pio device monitor        # optional, shows what it's fetching
```

That's it. It picks up WiFi, syncs the clock and starts drawing.

> Upload fails with a serial sync error? Hold **BOOT**, tap **RST**, release
> **BOOT**, then try again — and close any serial monitor first.

## Buttons

| Button | Does |
|---|---|
| **BOOT** | Next city — Riga, Vilnius, Tallinn, Kaunas, Helsinki, Stockholm, Warsaw. |
| **GPIO14** | Next range — 20 / 50 / 100 / 200 km. |

Both are saved to flash, so it comes back where you left it.

## Making it yours

Everything is in `src/config.h`.

**Your city.** Edit the `CITIES` table — the first entry is the default:

```c
{"RIGA", 56.9496, 24.1052, "RIX", "EVRA", 56.9236, 23.9711},
//  name   city centre      IATA   ICAO    airport position
```

Keep names to about twelve characters. `pio test -e native` checks every entry,
so a mistyped coordinate fails rather than drawing an airport in the wrong
country.

**Colours.** Green ground with black data by default. Uncomment
`CLASSIC_SCHEME` for the traditional black-on-green.

**The rest.** Ranges, sweep speed, trail length, poll intervals, overhead
radius, timezone — all named constants near the top of the file.

## Where the data comes from

- Aircraft: [adsb.lol](https://adsb.lol) — free, no key, community-fed. Coverage
  depends on volunteer receivers, so a quiet screen may just mean no nearby
  feeder.
- Routes and operators: adsb.lol's route files, plus a built-in table of ICAO
  airline codes.
- Weather: [aviationweather.gov](https://aviationweather.gov) METARs (NOAA).

## Developing

```bash
pio test -e native                  # 80 unit tests, run on your machine
sh tools/preview/build.sh           # render the UI without a board
python3 tools/preview/montage.py .preview
```

The preview compiles the real drawing code against a stand-in for the display,
using the same font the panel does, and writes `.preview/montage.png`. It's how
the layout gets checked before anything is flashed.

Porting to another ESP32 with an ST7789 or ILI9341 means editing
`src/display_config.h`; the layout constants in `config.h` assume 320×170.

## Licence

MIT — see [LICENSE](LICENSE).
