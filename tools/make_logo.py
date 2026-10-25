#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# NORILSK: the logo -- the polar night, no sun. Three thin stacks on the bank and a smear of fumes beside them; under the
# bank line their reflection in the river the nickel works turned red, broken into ripples. The name in heavy stencil
# capitals (drawn here as blocks: no font needed), their edges eaten away. Nothing above the name: the mark on the left,
# the name on the right, both the height of the capitals.
#   python3 tools/make_logo.py        writes docs/assets/mark.svg, logo-dark.svg, logo-light.svg
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "assets"

ASH, ASH_DARK = "#ddd6c1", "#24211b"            # the letters and stacks, on a dark ground / on a light one
OXIDE, SMOG = "#b5432a", "#4f4a3c"

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

BANK = 44                                       # the bank line; the mark is 72 high (the capitals), 60 wide
STACKS = [(12, 10, 6.5, 4), (24, 0, 7.5, 4.5), (36, 18, 6, 3.5)]   # centre x, top y, width at the foot, at the top
RIPPLES = [(46.0 + 2.2 * i, 1.5 - 0.06 * i, [0, 0.8, -0.6, 1.4, -1.2, 2.0, -1.8, 2.6, -2.4, 3.0, -2.8, 3.4][i], 0.95 - 0.07 * i)
           for i in range(12)]                  # y, height, shift, opacity: the strips of the reflection


def mark_body(ink, tag):
    """the mark in a 60 x 72 box: stacks, fumes, bank, and the red reflection in ripples"""
    out = []
    out.append(f'<filter id="{tag}r" x="-10%" y="-60%" width="120%" height="220%">'   # (the fumes' torn edges)
               '<feTurbulence type="fractalNoise" baseFrequency="0.05 0.2" numOctaves="3" seed="11" result="n"/>'
               '<feDisplacementMap in="SourceGraphic" in2="n" scale="7" xChannelSelector="R" yChannelSelector="G"/></filter>')
    out.append(f'<linearGradient id="{tag}g" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="{SMOG}"/>'
               f'<stop offset="1" stop-color="{SMOG}" stop-opacity="0"/></linearGradient>')
    # the fumes: one low smear leaning from the stacks' tops, down and away to the right, inside the box
    out.append(f'<path d="M20,4 C30,0 44,4 60,16 L60,22 C48,14 36,12 22,12 Z" fill="url(#{tag}g)" filter="url(#{tag}r)"/>')
    out.append(f'<path d="M33,20 C42,18 52,22 60,28 L60,31 C50,27 42,25 34,25 Z" fill="url(#{tag}g)" opacity="0.7" filter="url(#{tag}r)"/>')
    stacks = []
    for cx, top, wb, wt in STACKS:
        stacks.append(f"M{cx - wb / 2:.1f},{BANK} L{cx - wt / 2:.1f},{top} L{cx + wt / 2:.1f},{top} L{cx + wb / 2:.1f},{BANK} Z")
    out.append(f'<path d="{" ".join(stacks)}" fill="{ink}"/>')
    out.append(f'<rect x="0" y="{BANK}" width="60" height="1.6" fill="{ink}"/>')
    # the reflection: the stacks flipped and squashed under the bank, cut into strips, each strip shifted
    refl = []
    for cx, top, wb, wt in STACKS:
        rt = BANK + 1.6 + (BANK - top) * 0.58
        refl.append(f"M{cx - wb / 2:.1f},{BANK + 1.6} L{cx - wt / 2:.1f},{rt:.1f} L{cx + wt / 2:.1f},{rt:.1f} L{cx + wb / 2:.1f},{BANK + 1.6} Z")
    refl = " ".join(refl)
    for i, (y, h, dx, op) in enumerate(RIPPLES):
        out.append(f'<clipPath id="{tag}c{i}"><rect x="-6" y="{y}" width="72" height="{h}"/></clipPath>')
        out.append(f'<path d="{refl}" fill="{OXIDE}" opacity="{op}" transform="translate({dx} 0)" clip-path="url(#{tag}c{i})"/>')
    return "".join(out)


def mark():
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="-2 -2 64 76" width="64" height="76">
  <title>Norilsk</title>
  {mark_body(ASH, "m")}
</svg>
"""


def logo(ink, path):
    mask, body, w = word("NORILSK", ink)
    mx = 60 + 16                                 # the mark, then a gap
    total = mx + w
    h = 12 * U
    svg = f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="-4 -4 {total + 8:.0f} {h + 8:.0f}" width="{total + 8:.0f}" height="{h + 8:.0f}">
  <title>Norilsk</title>
  <defs>{ERODE}{mask}</defs>
  {mark_body(ink, "w")}
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
