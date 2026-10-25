#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# NORILSK: the website's demos -- build/host demo renders three soundscapes from the machine (tests/host/host.c
# cmd_demo); here each is faded in and out, brought to about -18 LUFS (ambient, not loudness-war) and made an MP3 in
# docs/assets/demo/. Needs ffmpeg with libmp3lame. Run tests/run_tests.sh (or build the host) first.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/demo docs/assets/demo
build/host demo build/demo > build/demo/demo.txt
for n in smelter polar-night tailings; do
    ffmpeg -loglevel error -y -i build/demo/demo-$n.wav \
        -af "afade=t=in:d=2.5,afade=t=out:st=44:d=4,loudnorm=I=-18:TP=-1.5:LRA=14" \
        -ar 44100 -c:a libmp3lame -q:a 2 docs/assets/demo/$n.mp3
done
ls -la docs/assets/demo
