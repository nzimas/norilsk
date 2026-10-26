#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# NORILSK host tests (no toolchain, no FM-1 needed): the generated tables, the engine checked for a runtime the
# chip lacks, then the sounds rendered on the computer. Renders land in build/sounds (WAV).
set -e
cd "$(dirname "$0")/.."
PY=python3
[ -x .venv/bin/python ] && PY=.venv/bin/python
mkdir -p build/gen build/sounds
$PY tools/gen_tables.py build/gen/norilsk_tables.h >/dev/null
tests/check_target_ir.sh
$PY tools/gen_font.py build/gen/norilsk_font.h >/dev/null
$PY tools/list_says.py build/gen/norilsk_says.h >/dev/null
mkdir -p build/screens
clang -O2 -std=gnu99 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function -Werror \
    -Ifirmware/src -Ibuild/gen tests/host/host.c -o build/host
build/host check
build/host sustain
build/host fxtails | tail -1
build/host tails 24 | tail -1
build/host mtails > build/mtails.txt || { cat build/mtails.txt; exit 1; }; tail -1 build/mtails.txt
build/host flows
build/host looper
build/host rewire
build/host ears
build/host master
build/host mod
build/host sessions
build/host backup
build/host morph
build/host ends
build/host hiss
build/host recall
build/host quiet
build/host format1
build/host whole
build/host link
build/host lanes
build/host sespage
build/host b36 | tail -1
build/host drive | tail -2
build/host lanepan | tail -1
build/host mutefade | tail -1
build/host load
build/host dis
build/host paths | tail -1
build/host revalue
build/host paint | tail -1
build/host fit | tail -1
build/host cores
build/host modules build/sounds > build/sounds/modules.txt
build/host patches build/sounds 12 > build/sounds/patches.txt
rm -f build/screens/*.png
build/host screens build/screens > build/screens/patches.txt
$PY tools/ppm2png.py build/screens/*.ppm
for g in 0 1 2 3 4 5 6; do
    build/host golden $g > build/golden$g.txt
    if [ -f tests/golden/gen$g.txt ]; then
        diff -q build/golden$g.txt tests/golden/gen$g.txt >/dev/null || { echo "FAIL golden: generation $g no longer draws its sounds again (tests/golden/gen$g.txt)"; exit 1; }
    else
        cp build/golden$g.txt tests/golden/gen$g.txt
        echo "golden: tests/golden/gen$g.txt written"
    fi
done
echo "golden: generations 0 .. 6 as they were"
build/host bench
build/host render build/sounds 3 0 > build/sounds/stats-open.txt
build/host render build/sounds 2 1 > build/sounds/stats-ambient.txt
build/host render build/sounds 2 2 > build/sounds/stats-exp.txt
echo "tests: ok (sounds in build/sounds, screens in build/screens)"
