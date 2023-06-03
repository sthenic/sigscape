#!/usr/bin/env bash

set -xe

drawio --export --format png --transparent --crop --output "sigscape.png" "sigscape.drawio"

generate() {
    convert "sigscape.png" -resize "$1x$1" "sigscape$1.png"
    xxd -i "sigscape$1.png" > "sigscape$1.h"
}

# Windows
generate 16
generate 32
generate 48
convert sigscape.png -alpha on -define icon:auto-resize="256,128,96,64,48,32,16" -compress jpeg sigscape.ico

# Linux
generate 128
