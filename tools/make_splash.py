#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# NORILSK: the splash screen's logo, rendered from the website's (tools/make_logo.py: the same shapes, the same eroded
# edges and torn fumes) at the screen's size, over black: the mark MARK_H px high, the lettering WORD_W px wide. Written
# to assets/splash/ and kept in the repository, so the build needs no SVG renderer (tools/gen_splash.py turns the two
# into the firmware's header). Needs rsvg-convert (librsvg: it draws the SVG filters). Run it again when the logo changes.
#   python3 tools/make_splash.py
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from make_logo import ASH, ERODE, U, mark_body, word   # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "splash"
MARK_H = 136                     # (Tonitor's mark: 140 px)
WORD_W = 224                     # (Tonitor's lettering: 224 px across)


def render(svg, out, w=None, h=None):
    with tempfile.NamedTemporaryFile("w", suffix=".svg", delete=False) as f:
        f.write(svg)
    size = ["-w", str(w)] if w else ["-h", str(h)]
    subprocess.run(["rsvg-convert", "-b", "black", *size, "-o", str(out), f.name], check=True)
    Path(f.name).unlink()


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    render(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="-2 -2 64 76" width="64" height="76">{mark_body(ASH, "m")}</svg>',
           OUT / "mark.png", h=MARK_H)
    mask, body, w = word("NORILSK", ASH)
    render(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="-4 -4 {w + 8:.0f} {12 * U + 8:.0f}" width="{w + 8:.0f}" '
           f'height="{12 * U + 8:.0f}"><defs>{ERODE}{mask}</defs>{body}</svg>', OUT / "word.png", w=WORD_W)
    print("splash: assets/splash/mark.png, word.png")
