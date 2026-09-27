#!/bin/sh
# Regenerates macos/CSBwin.icns from sword.txt (the sword in CSBwin.ico).
set -e
cd "$(dirname "$0")"
iconset=$(mktemp -d)/CSBwin.iconset
mkdir -p "$iconset"
for size in 16 32 128 256 512; do
   swift make_icon.swift sword.txt "$iconset/icon_${size}x${size}.png" $size
   swift make_icon.swift sword.txt "$iconset/icon_${size}x${size}@2x.png" $((size * 2))
done
iconutil -c icns "$iconset" -o ../CSBwin.icns
