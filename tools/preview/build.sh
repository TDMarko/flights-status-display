#!/bin/sh
# Renders the radar UI to PPM previews on the host. Run from the project root:
#   sh tools/preview/build.sh
set -e
OUT=${1:-.preview}
AJ=".pio/libdeps/flydar/ArduinoJson/src"
mkdir -p "$OUT"
c++ -std=gnu++17 -O1 -Wall \
    -Itools/preview/fakeinc -Itools/preview -Isrc -I"$AJ" \
    tools/preview/preview.cpp src/radar_ui.cpp src/geo.cpp src/adsb.cpp src/trails.cpp \
    -o "$OUT/preview"
"$OUT/preview" "$OUT" test/fixtures
