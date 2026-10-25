#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# NORILSK: the logo -- the polar night, no sun. A smelter hall with its sawtooth roof and three tapered stacks, each
# with one oxide band; their fumes bend over and run flat to the right as a low ceiling over the name, fading out.
# The name in condensed stencil capitals (drawn here as blocks: no font needed), corners cut, edges eaten away.
# Ash, soot, sulphur fumes and the oxide red of the river the nickel works turned red.
#   python3 tools/make_logo.py        writes docs/assets/mark.svg, logo-dark.svg, logo-light.svg
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "assets"

ASH, ASH_DARK = "#ddd6c1", "#24211b"            # the letters and stacks, on a dark ground / on a light one
OXIDE = "#a8402a"
FUMES = ["#4b4739", "#5c5644", "#6a5d2c"]       # the three plumes: soot, smog, sulphur dioxide

U = 6.0                                         # the letters' grid unit
H, T = 13.0, 2.4                                # capitals' height and stroke, units
C = 1.3                                         # the corners cut off (top left, bottom right), units
# each letter: filled polygons (grid units, y down), the stencil's cuts (rectangles taken out), its width
LETTERS = {
    "N": ([[(0, 0), (T, 0), (T, H), (0, H)], [(7 - T, 0), (7, 0), (7, H), (7 - T, H)],
           [(0, 0), (T, 0), (7, H), (7 - T, H)]],
          [(7 - T - 0.6, 9.0, 0.6, 4.0)], 7),
    "O": ([[(0, 0), (7, 0), (7, H), (0, H), (0, 0), (T, T), (T, H - T), (7 - T, H - T), (7 - T, T), (T, T)]],
          [(3.1, 0, 0.8, T), (3.1, H - T, 0.8, T)], 7),
    "R": ([[(0, 0), (T, 0), (T, H), (0, H)],
           [(T, 0), (7.2, 0), (7.2, 7.4), (T, 7.4), (T, 0), (T, T), (4.8, T), (4.8, 5.0), (T, 5.0), (T, T)],
           [(3.4, 7.4), (6.0, 7.4), (7.8, H), (5.2, H)]],
          [(T, 0, 0.6, 7.4)], 7.8),
    "I": ([[(0, 0), (T, 0), (T, H), (0, H)]], [(0, 6.2, T, 0.6)], T),
    "L": ([[(0, 0), (T, 0), (T, H), (0, H)], [(T, H - T), (6.2, H - T), (6.2, H), (T, H)]], [(T, H - T, 0.6, T)], 6.2),
    "S": ([[(0, 0), (7, 0), (7, T), (0, T)], [(0, 0), (T, 0), (T, 7.7), (0, 7.7)], [(0, 5.3), (7, 5.3), (7, 7.7), (0, 7.7)],
           [(7 - T, 5.3), (7, 5.3), (7, H), (7 - T, H)], [(0, H - T), (7, H - T), (7, H), (0, H)]],
          [(4.4, 0, 0.6, T), (2.0, H - T, 0.6, T)], 7),
    "K": ([[(0, 0), (T, 0), (T, H), (0, H)], [(T + 0.6, 5.6), (4.9, 0), (7.6, 0), (5.4, 6.9), (T + 0.6, 6.9)],
           [(T + 0.6, 6.1), (5.4, 6.1), (7.8, H), (5.1, H), (T + 0.6, 7.7)]],
          [(T, 0, 0.6, H)], 7.8),
}
GAP = 1.5                                       # between letters, units


def word(text, ink):
    """the word as paths and its stencil mask (eroded edges by a filter); its width in px"""
    x, polys, cuts = 0.0, [], []
    for ch in text:
        ps, cs, w = LETTERS[ch]
        for p in ps:                             # (each part its own path: overlaps add, counters cut)
            polys.append(f'<path fill-rule="evenodd" d="M' + " L".join(f"{(x + px) * U:.1f},{py * U:.1f}" for px, py in p) + ' Z"/>')
        for cx, cy, cw, chh in cs:
            cuts.append(f'<rect x="{(x + cx) * U:.1f}" y="{cy * U:.1f}" width="{cw * U:.1f}" height="{chh * U:.1f}" fill="#000"/>')
        for tri in ([(x, 0), (x + C, 0), (x, C)], [(x + w, H), (x + w - C, H), (x + w, H - C)]):
            cuts.append('<path fill="#000" d="M' + " L".join(f"{px * U:.1f},{py * U:.1f}" for px, py in tri) + ' Z"/>')
        x += w + GAP
    width = (x - GAP) * U
    mask = (f'<mask id="cut" maskUnits="userSpaceOnUse" x="-10" y="-10" width="{width + 20:.0f}" height="{H * U + 20:.0f}">'
            f'<rect x="-10" y="-10" width="{width + 20:.0f}" height="{H * U + 20:.0f}" fill="#fff"/>{"".join(cuts)}</mask>')
    body = f'<g fill="{ink}" mask="url(#cut)" filter="url(#erode)">{"".join(polys)}</g>'
    return mask, body, width


ERODE = ('<filter id="erode" x="-5%" y="-10%" width="110%" height="120%">'
         '<feTurbulence type="fractalNoise" baseFrequency="0.9" numOctaves="2" seed="7" result="n"/>'
         '<feDisplacementMap in="SourceGraphic" in2="n" scale="2.0" xChannelSelector="R" yChannelSelector="G"/></filter>')

BASE = H * U                                    # the ground line: the letters' baseline
# the stacks: centre x, top y, width at the foot, width at the top (px, before scaling); and each one's plume:
# the height it levels out at, its half thickness there, where it fades out
STACKS = [(22, -24, 11, 7, -40, 5.0, 300), (42, -40, 13, 8, -55, 6.5, 470), (62, -12, 10, 6, -27, 4.0, 200)]


def smelter(ink, x0=0.0, s=1.0, tag="m", reach=1.0):
    """the mark: hall, stacks and their plumes, ground at BASE; reach scales how far the plumes run"""
    def X(v): return f"{x0 + v * s:.1f}"
    def Y(v): return f"{v * s:.1f}"
    defs, back, front = [], [], []
    for i, (cx, top, wb, wt, py, ht, xe) in enumerate(STACKS):
        xe = cx + (xe - cx) * reach
        g = f"{tag}f{i}"
        defs.append(f'<linearGradient id="{g}" gradientUnits="userSpaceOnUse" x1="{X(cx)}" y1="0" x2="{X(xe)}" y2="0">'
                    f'<stop offset="0" stop-color="{FUMES[i]}"/><stop offset="0.45" stop-color="{FUMES[i]}" stop-opacity="0.8"/>'
                    f'<stop offset="1" stop-color="{FUMES[i]}" stop-opacity="0"/></linearGradient>')
        l, r, bend = cx - wt / 2, cx + wt / 2, cx + 26
        droop = 3.0                              # (the far end sinks a little: the fumes are heavy)
        mid = bend + (xe - bend) * 0.45           # (the fumes swell as they go, then thin out)
        d = (f"M{X(l)},{Y(top)} C{X(l)},{Y(py - ht - 3)} {X(cx + 6)},{Y(py - ht)} {X(bend)},{Y(py - ht)} "
             f"Q{X(mid)},{Y(py - ht * 2.2)} {X(xe)},{Y(py + droop - 0.5)} "
             f"Q{X(mid)},{Y(py + ht * 1.9 + droop)} {X(bend)},{Y(py + ht)} "
             f"C{X(cx + 12)},{Y(py + ht)} {X(r)},{Y(py + ht + 3)} {X(r)},{Y(top)} Z")
        back.append(f'<path d="{d}" fill="url(#{g})" filter="url(#{tag}r)"/>')
        front.append(f'<path d="M{X(cx - wb / 2)},{Y(BASE)} L{X(l)},{Y(top)} L{X(r)},{Y(top)} L{X(cx + wb / 2)},{Y(BASE)} Z" fill="{ink}"/>')
        b0, b1 = top + 5, top + 10               # the oxide band, following the taper
        def half(y): return (wt + (wb - wt) * (y - top) / (BASE - top)) / 2
        front.append(f'<path d="M{X(cx - half(b0))},{Y(b0)} L{X(cx + half(b0))},{Y(b0)} L{X(cx + half(b1))},{Y(b1)} '
                     f'L{X(cx - half(b1))},{Y(b1)} Z" fill="{OXIDE}"/>')
    roof = [(4, BASE), (4, 58)]                  # the hall: a sawtooth roof, five teeth
    for k in range(5):
        x = 4 + 15 * k
        roof += [(x + 15, 50), (x + 15, 58)]
    roof.append((79, BASE))
    front.append('<path d="M' + " L".join(f"{X(px)},{Y(py)}" for px, py in roof) + f' Z" fill="{ink}"/>')
    front.append(f'<rect x="{X(-2)}" y="{Y(BASE)}" width="{86 * s:.1f}" height="{2.4 * s:.1f}" fill="{ink}"/>')
    haze = (f'<filter id="{tag}b" x="-20%" y="-80%" width="140%" height="260%"><feGaussianBlur stdDeviation="{5 * s:.1f}"/></filter>')
    defs.append(haze)
    defs.append(f'<filter id="{tag}r" x="-5%" y="-60%" width="110%" height="220%">'   # (ragged, torn edges)
                f'<feTurbulence type="fractalNoise" baseFrequency="{0.035 / s:.3f} {0.12 / s:.3f}" numOctaves="3" seed="11" result="n"/>'
                f'<feDisplacementMap in="SourceGraphic" in2="n" scale="{9 * s:.1f}" xChannelSelector="R" yChannelSelector="G"/></filter>')
    murk = (f'<ellipse cx="{X(150 * reach + 40)}" cy="{Y(-42)}" rx="{160 * reach * s:.1f}" '
            f'ry="{14 * s:.1f}" fill="{FUMES[0]}" opacity="0.35" filter="url(#{tag}b)"/>')
    return "".join(defs), murk + "".join(back) + "".join(front)


def mark():
    defs, body = smelter(ASH, 0, 1.0, "m", 0.12)
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="-4 -74 104 158" width="104" height="158">
  <title>Norilsk</title>
  <defs>{defs}</defs>
  {body}
</svg>
"""


def logo(ink, path):
    mask, body, w = word("NORILSK", ink)
    defs, sm = smelter(ink, 0, 1.0, "w")
    mx = 96                                      # the word after the hall
    total = mx + w
    top = -74
    h = BASE + 3 - top
    svg = f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="-4 {top} {total + 8:.0f} {h:.0f}" width="{total + 8:.0f}" height="{h:.0f}">
  <title>Norilsk</title>
  <defs>{ERODE}{mask}{defs}</defs>
  {sm}
  <g transform="translate({mx:.1f} 0)">{body}</g>
</svg>
"""
    path.write_text(svg)
    return total + 8, h


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "mark.svg").write_text(mark())
    w, h = logo(ASH, OUT / "logo-dark.svg")
    logo(ASH_DARK, OUT / "logo-light.svg")
    print(f"logo: docs/assets/mark.svg, logo-dark.svg, logo-light.svg ({w:.0f} x {h:.0f})")
