#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# MOLECULAR host tests (no toolchain, no FM-1 needed): the generated tables, the engine checked for a runtime the
# chip lacks, then the sounds rendered on the computer. Renders land in build/sounds (WAV).
set -e
cd "$(dirname "$0")/.."
PY=python3
[ -x .venv/bin/python ] && PY=.venv/bin/python
mkdir -p build/gen build/sounds
$PY tools/gen_tables.py build/gen/molecular_tables.h >/dev/null
tests/check_target_ir.sh
$PY tools/gen_font.py build/gen/molecular_font.h >/dev/null
mkdir -p build/screens
clang -O2 -std=gnu99 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function -Werror \
    -Ifirmware/src -Ibuild/gen tests/host/host.c -o build/host
build/host check
build/host sustain
build/host fxtails | tail -1
build/host tails 24 | tail -1
build/host flows
build/host looper
build/host cores
build/host modules build/sounds > build/sounds/modules.txt
build/host patches build/sounds 12 > build/sounds/patches.txt
rm -f build/screens/*.png
build/host screens build/screens > build/screens/patches.txt
$PY tools/ppm2png.py build/screens/*.ppm
for g in 0 1 2; do
    build/host golden $g > build/golden$g.txt
    if [ -f tests/golden/gen$g.txt ]; then
        diff -q build/golden$g.txt tests/golden/gen$g.txt >/dev/null || { echo "FAIL golden: generation $g no longer draws its sounds again (tests/golden/gen$g.txt)"; exit 1; }
    else
        cp build/golden$g.txt tests/golden/gen$g.txt
        echo "golden: tests/golden/gen$g.txt written"
    fi
done
echo "golden: generations 0, 1 and 2 as they were"
build/host bench
build/host render build/sounds 3 0 > build/sounds/stats-open.txt
build/host render build/sounds 2 1 > build/sounds/stats-ambient.txt
build/host render build/sounds 2 2 > build/sounds/stats-exp.txt
echo "tests: ok (sounds in build/sounds, screens in build/screens)"
