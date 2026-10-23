#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# MOLECULAR: build, then install on the FM-1 connected over USB (any firmware it runs: M-VAVE's, SLOOP,
# Felucca, TONITOR, MOLECULAR, or the rescue mode).
#   ./install.sh            build and install
#   ./install.sh --no-build install build/molecular.fwsc as it is
#   ./install.sh FILE.fwsc  install that package (another firmware, or SLOOP to go back)
#   ./install.sh --info     what the connected FM-1 runs
set -e
cd "$(dirname "$0")"
PY=.venv/bin/python
[ -x "$PY" ] || { echo "install.sh: run ./setup.sh first"; exit 1; }
case "$1" in
--info) exec "$PY" tools/fm1_install.py --info ;;
--no-build) PKG=build/molecular.fwsc ;;
"") ./build.sh; PKG=build/molecular.fwsc ;;
*) PKG="$1" ;;
esac
exec "$PY" tools/fm1_install.py "$PKG"
