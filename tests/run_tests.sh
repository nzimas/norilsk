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
build/host flows
build/host patches build/sounds 12 > build/sounds/patches.txt
build/host screens build/screens > build/screens/patches.txt
$PY tools/ppm2png.py build/screens/*.ppm
build/host golden > build/golden.txt
if [ -f tests/golden/gen0.txt ]; then
    diff -q build/golden.txt tests/golden/gen0.txt >/dev/null || { echo "FAIL golden: generation 0 no longer draws its sounds again (tests/golden/gen0.txt)"; exit 1; }
    echo "golden: generation 0 as it was"
else
    cp build/golden.txt tests/golden/gen0.txt
    echo "golden: tests/golden/gen0.txt written"
fi
build/host bench
build/host render build/sounds 3 0 > build/sounds/stats-open.txt
build/host render build/sounds 2 1 > build/sounds/stats-ambient.txt
build/host render build/sounds 2 2 > build/sounds/stats-exp.txt
echo "tests: ok (sounds in build/sounds, screens in build/screens)"
