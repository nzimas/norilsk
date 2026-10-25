#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# NORILSK: the logo -- the smelter's banded stacks under their plume, a sulphur sun drowned in it at the horizon, and the
# name in heavy stencil capitals (drawn here as blocks: no font needed), their edges eaten away. Ash, sulphur and the
# oxide red of the river the nickel works turned red.
#   python3 tools/make_logo.py        writes docs/assets/mark.svg, logo-dark.svg, logo-light.svg
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "assets"

ASH, ASH_DARK = "#ddd6c1", "#24211b"            # the letters and stacks, on a dark ground / on a light one
SULPHUR, OXIDE, SMOG, BILE = "#bea43a", "#b5432a", "#5f5a4a", "#8a7630"

U = 6.0                                         # the letters' grid unit; capitals 12 units high
# each letter: filled polygons (grid units, y down) and the stencil's cuts (rectangles taken out)
LETTERS = {
    "N": ([[(0, 0), (3, 0), (3, 12), (0, 12)], [(5, 0), (8, 0), (8, 12), (5, 12)],
           [(0, 0), (3, 0), (8, 12), (5, 12)]],
          [(4.3, 8.4, 0.7, 3.6)], 8),
    "O": ([[(0, 0), (8, 0), (8, 12), (0, 12), (0, 0), (3, 3), (3, 9), (5, 9), (5, 3), (3, 3)]],
          [(3.6, 0, 0.8, 3), (3.6, 9, 0.8, 3)], 8),
    "R": ([[(0, 0), (3, 0), (3, 12), (0, 12)],
           [(3, 0), (8, 0), (8, 7), (3, 7), (3, 0), (3, 2.6), (5.2, 2.6), (5.2, 4.4), (3, 4.4), (3, 2.6)],
           [(3.4, 7), (6.4, 7), (8.4, 12), (5.4, 12)]],
          [(3.0, 0, 0.7, 7)], 8.4),
    "I": ([[(0, 0), (3, 0), (3, 12), (0, 12)]], [(0, 5.6, 3, 0.7)], 3),
    "L": ([[(0, 0), (3, 0), (3, 12), (0, 12)], [(3, 9), (7, 9), (7, 12), (3, 12)]], [(3.0, 9, 0.7, 3)], 7),
    "S": ([[(0, 0), (8, 0), (8, 3), (0, 3)], [(0, 0), (3, 0), (3, 7.5), (0, 7.5)], [(0, 4.5), (8, 4.5), (8, 7.5), (0, 7.5)],
           [(5, 4.5), (8, 4.5), (8, 12), (5, 12)], [(0, 9), (8, 9), (8, 12), (0, 12)]],
          [(5.2, 0, 0.7, 3), (2.1, 9, 0.7, 3)], 8),
    "K": ([[(0, 0), (3, 0), (3, 12), (0, 12)], [(3.7, 5.2), (5.6, 0), (8.6, 0), (6.4, 6.4), (3.7, 6.4)],
           [(3.7, 5.6), (6.4, 5.6), (8.6, 12), (5.6, 12), (3.7, 7.0)]],
          [(3.0, 0, 0.7, 12)], 8.6),
}
GAP = 1.6                                       # between letters, units


def word(text, ink):
    """the word as paths and its stencil mask (eroded edges by a filter); its width in px"""
    x, polys, cuts = 0.0, [], []
    for ch in text:
        ps, cs, w = LETTERS[ch]
        for p in ps:                             # (each part its own path: overlaps add, counters cut)
            polys.append(f'<path fill-rule="evenodd" d="M' + " L".join(f"{(x + px) * U:.1f},{py * U:.1f}" for px, py in p) + ' Z"/>')
        for cx, cy, cw, chh in cs:
            cuts.append(f'<rect x="{(x + cx) * U:.1f}" y="{cy * U:.1f}" width="{cw * U:.1f}" height="{chh * U:.1f}" fill="#000"/>')
        x += w + GAP
    width = (x - GAP) * U
    mask = (f'<mask id="cut" maskUnits="userSpaceOnUse" x="-10" y="-10" width="{width + 20:.0f}" height="{12 * U + 20:.0f}">'
            f'<rect x="-10" y="-10" width="{width + 20:.0f}" height="{12 * U + 20:.0f}" fill="#fff"/>{"".join(cuts)}</mask>')
    body = f'<g fill="{ink}" mask="url(#cut)" filter="url(#erode)">{"".join(polys)}</g>'
    return mask, body, width


ERODE = ('<filter id="erode" x="-5%" y="-10%" width="110%" height="120%">'
         '<feTurbulence type="fractalNoise" baseFrequency="0.9" numOctaves="2" seed="7" result="n"/>'
         '<feDisplacementMap in="SourceGraphic" in2="n" scale="2.6" xChannelSelector="R" yChannelSelector="G"/></filter>')


def stacks(ink, x0=0.0, scale=1.0, tag="m"):
    """the mark: three stacks, banded, their plume smeared right and down across a sun sunk in the ground; 96 x 76"""
    s = scale
    def r(x, y, w, h, fill, extra=""):
        return f'<rect x="{x0 + x * s:.1f}" y="{y * s:.1f}" width="{w * s:.1f}" height="{h * s:.1f}" fill="{fill}"{extra}/>'
    out = [f'<clipPath id="{tag}g"><rect x="{x0 - 4 * s:.1f}" y="{-20 * s:.1f}" width="{110 * s:.1f}" height="{94 * s:.1f}"/></clipPath>',
           f'<filter id="{tag}b" x="-30%" y="-60%" width="160%" height="220%"><feGaussianBlur stdDeviation="{2.4 * s:.2f}"/></filter>',
           f'<circle cx="{x0 + 74 * s:.1f}" cy="{74 * s:.1f}" r="{15 * s:.1f}" fill="{SULPHUR}" opacity="0.9" clip-path="url(#{tag}g)"/>']
    plume = []                                  # (smeared ellipses, heavier near the stacks)
    for cx, cy, rx, ry, op in [(22, 11, 10, 8, 1), (35, 12, 14, 9, .95), (50, 17, 17, 10, .9), (66, 25, 18, 10, .85),
                               (81, 35, 17, 10, .75), (93, 47, 14, 9, .6), (102, 58, 11, 7, .45), (58, 30, 15, 6, .6)]:
        plume.append(f'<ellipse cx="{x0 + cx * s:.1f}" cy="{cy * s:.1f}" rx="{rx * s:.1f}" ry="{ry * s:.1f}" '
                     f'transform="rotate(24 {x0 + cx * s:.1f} {cy * s:.1f})" opacity="{op}"/>')
    sick = []                                   # (under it, a sulphurous brown seeping out)
    for cx, cy, rx, ry in [(46, 26, 16, 5), (70, 38, 18, 6), (90, 55, 14, 5)]:
        sick.append(f'<ellipse cx="{x0 + cx * s:.1f}" cy="{cy * s:.1f}" rx="{rx * s:.1f}" ry="{ry * s:.1f}" '
                    f'transform="rotate(24 {x0 + cx * s:.1f} {cy * s:.1f})"/>')
    out.append(f'<g fill="{BILE}" opacity="0.55" filter="url(#{tag}b)">{"".join(sick)}</g>')
    out.append(f'<g fill="{SMOG}" filter="url(#{tag}b)">{"".join(plume)}</g>')
    for x, top, w in [(8, 22, 8), (22, 12, 9), (37, 30, 7)]:          # the stacks, each with oxide bands
        out.append(r(x, top, w, 74 - top, ink))
        for b in range(2):
            out.append(r(x, top + 4 + b * 10, w, 3.2, OXIDE))
    out.append(r(-2, 74, 104, 2.2, ink))                              # the ground
    return "".join(out)


def mark():
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="-4 -2 112 80" width="112" height="80">
  <title>Norilsk</title>
  {stacks(ASH)}
</svg>
"""


def logo(ink, path):
    mask, body, w = word("NORILSK", ink)
    mx = 106 * (12 * U / 76) + 18                # the mark as tall as the capitals, then a gap
    total = mx + w
    h = 12 * U
    sc = h / 76
    svg = f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="-4 -4 {total + 8:.0f} {h + 8:.0f}" width="{total + 8:.0f}" height="{h + 8:.0f}">
  <title>Norilsk</title>
  <defs>{ERODE}{mask}</defs>
  {stacks(ink, 0, sc, "w")}
  <g transform="translate({mx:.1f} 0)">{body}</g>
</svg>
"""
    path.write_text(svg)
    return total + 8, h + 8


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "mark.svg").write_text(mark())
    w, h = logo(ASH, OUT / "logo-dark.svg")
    logo(ASH_DARK, OUT / "logo-light.svg")
    print(f"logo: docs/assets/mark.svg, logo-dark.svg, logo-light.svg ({w:.0f} x {h:.0f})")
