#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# MOLECULAR: fixed-point lookup tables (the target has no FPU and no float runtime).
# After Tonitor's tools/gen_tables.py (itself after Felucca's: SINE, the pitch table, the FM6 / msfa
# start-up tables) and Bigorna's (the band-limited single cycles, WT_*).
"""tools/gen_tables.py OUT.h"""
import math
import sys
from pathlib import Path

FS = 44100
CTL = 32


def arr(name, ctype, vals, per=12, attr=""):
    out = [f"static const {ctype} {name}[{len(vals)}]{attr} = {{"]
    for i in range(0, len(vals), per):
        out.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
    out.append("};")
    return out


# ---------------------------------------------------------------- the waves (wt.c) ---
# Each wave: WT_LV levels, level k keeping 128 >> k harmonics in WT_LEN[k] points (+ 1: the first again).
WT_LV = 8
WT_LEN = [256, 128, 64, 64, 64, 64, 64, 64]


def prim(k, n):
    """PRIM: sine, triangle, saw, square"""
    if k == 0:
        return 1.0 if n == 1 else 0.0
    if k == 1:
        return (8 / math.pi ** 2 / n ** 2 * (-1 if (n // 2) % 2 else 1)) if n % 2 else 0.0
    if k == 2:
        return 2 / math.pi / n * (-1 if n % 2 == 0 else 1)
    return 4 / math.pi / n if n % 2 else 0.0


def harm(k, n):
    """HARM: sixteen waves sweeping through combinations of harmonics, from a sine with its octave towards
    bright, then hollow and formant-like spectra (Bigorna's set)"""
    t = [
        {1: 1, 2: 0.5},
        {1: 1, 3: 0.45},
        {1: 1, 2: 0.6, 3: 0.4},
        {1: 1, 2: 0.5, 4: 0.35, 8: 0.25},
        {1: 1, 3: 0.5, 5: 0.35, 7: 0.25},
        {i: 1 / i for i in range(1, 9)},
        {i: 1 / i for i in range(1, 17)},
        {i: 1 / i ** 0.7 for i in range(1, 33)},
        {i: 1 / i for i in range(1, 33) if i % 2},
        {i: 1 / i for i in range(1, 33) if i % 3},
        {1: 0.6, 4: 1, 5: 0.8, 6: 0.4},
        {1: 0.5, 7: 1, 8: 0.9, 9: 0.5, 10: 0.2},
        {1: 0.5, 2: 0.2, 11: 1, 12: 0.7, 13: 0.4},
        {i: (1.0 if i in (1, 5, 9, 13, 17, 21) else 0.0) for i in range(1, 24)},
        {i: 1 / i ** 0.4 for i in range(1, 65)},
        {i: math.sin(i * 1.7) / i ** 0.5 for i in range(1, 65)},
    ][k]
    return t.get(n, 0.0)


def bell(k, n):
    """BELL: eight waves for FM operators -- partials at the places FM puts its sidebands and the bells
    their modes, so a carrier on one of these behaves like a small stack of operators (this firmware's own set)"""
    t = [
        {1: 1, 2: 0.4, 3: 0.2},                          # a sine turned by a little of itself
        {1: 1, 2: -0.5, 3: 0.3, 4: -0.15, 5: 0.08},      # alternating: the look of a low index of FM
        {1: 1, 3: 0.6, 5: 0.3, 7: 0.15, 9: 0.08},        # odd, falling: a hollow pipe
        {1: 0.8, 2: 1, 3: 0.3, 5: 0.4, 7: 0.2},          # the second partial loudest: a struck bar's hum
        {1: 1, 4: 0.5, 6: 0.3, 9: 0.25, 11: 0.15},       # inharmonic-looking: a bell's upper modes
        {1: 0.5, 2: 0.7, 3: 0.9, 4: 0.7, 5: 0.5, 6: 0.3},  # a formant round the third partial
        {1: 1, 2: 0.9, 3: 0.8, 4: 0.7, 5: 0.6, 6: 0.5, 7: 0.4, 8: 0.3, 9: 0.2, 10: 0.1},   # a flat ramp: bright
        {i: (1.0 / i if i % 2 else 0.35 / i) for i in range(1, 25)},   # saw-like with the evens held back
    ][k]
    return t.get(n, 0.0)


def wave(fn, k):
    out = []
    for lv in range(WT_LV):
        N, H = WT_LEN[lv], 128 >> lv
        d = [0.0] * N
        for n in range(1, H + 1):
            a = fn(k, n)
            if a:
                for i in range(N):
                    d[i] += a * math.sin(2 * math.pi * n * i / N)
        out.append(d)
    peak = max(max(abs(x) for x in d) for d in out) or 1.0
    vals = []
    for d in out:
        vals += [int(round(32767 * x / peak)) for x in d] + [int(round(32767 * d[0] / peak))]
    return vals


def wave_tables():
    lists = [("PRIM", prim, 4), ("HARM", harm, 16), ("BELL", bell, 8)]
    data, L, w = [], [], 0
    for name, fn, n in lists:
        L += [f"#define WV_{name}0 {w}u", f"#define WV_{name}_N {n}u"]
        for k in range(n):
            data += wave(fn, k)
        w += n
    per = sum(WT_LEN) + WT_LV
    offs, sh, o = [], [], 0
    for N in WT_LEN:
        offs.append(o)
        sh.append(32 - int(math.log2(N)))
        o += N + 1
    L = [f"#define WT_LV {WT_LV}u", f"#define WT_N {w}u", f"#define WT_WAVE {per}u", f"#define WT_TOTAL {len(data)}u"] + L
    L += arr("WT_OFF", "uint16_t", offs, 8) + arr("WT_SH", "uint8_t", sh, 8)
    L += ["/* the waves (wt.c), WT_WAVE points each, level after level */"]
    L += arr("WT_DATA", "int16_t", data, 16)
    return L


# FM6 (src/fm6_core.c, a C port of msfa, Apache-2.0): the tables msfa built with double math at start-up,
# precomputed here so the firmware needs no float. CTL-sample blocks at 44.1 kHz. (Felucca's tools/gen_tables.py.)
FM6_LFO_SOURCE = [   # msfa lfo.cc: the LFO rates 0..99 (Hz)
    0.062541, 0.125031, 0.312393, 0.437120, 0.624610, 0.750694, 0.936330, 1.125302, 1.249609, 1.436782,
    1.560915, 1.752081, 1.875117, 2.062494, 2.247191, 2.374451, 2.560492, 2.686728, 2.873976, 2.998950,
    3.188013, 3.369840, 3.500175, 3.682224, 3.812065, 4.000800, 4.186202, 4.310716, 4.501260, 4.623209,
    4.814636, 4.930480, 5.121901, 5.315191, 5.434783, 5.617346, 5.750431, 5.946717, 6.062811, 6.248438,
    6.431695, 6.564264, 6.749460, 6.868132, 7.052186, 7.250580, 7.375719, 7.556294, 7.687577, 7.877738,
    7.993605, 8.181967, 8.372405, 8.504848, 8.685079, 8.810573, 8.986341, 9.122423, 9.300595, 9.500285,
    9.607994, 9.798158, 9.950249, 10.117361, 11.251125, 11.384335, 12.562814, 13.676149, 13.904338, 15.092062,
    16.366612, 16.638935, 17.869907, 19.193858, 19.425019, 20.833333, 21.034918, 22.502250, 24.003841, 24.260068,
    25.746653, 27.173913, 27.578599, 29.052876, 30.693677, 31.191516, 32.658393, 34.317090, 34.674064, 36.416606,
    38.197097, 38.550501, 40.387722, 40.749796, 42.625746, 44.326241, 44.883303, 46.772685, 48.590865, 49.261084]


def fm6_tables():
    L = ["/* FM6 (fm6_core.c): msfa's start-up tables, precomputed */",
         f"#define FM6_LFO_UNIT {int(CTL * 25190424 / FS + 0.5)}     /* msfa Lfo::unit_ */",
         f"#define FM6_PENV_UNIT {int(CTL * (1 << 24) / (21.3 * FS) + 0.5)}    /* msfa PitchEnv::unit_ */",
         f"#define FM6_FREQ_R {int(round(2 ** 40 / FS))}u   /* 2^40 / FS (fm6_freq) */",
         f"#define FM6_AMS_C0 {int(round(12.2 * math.log2(math.e) * 2 ** 24))}   /* log2(e^12.2), Q24 */",
         f"#define FM6_AMS_K {int(round(0.07 / 262144 * math.log2(math.e) * 2 ** 24 * 1024))}      /* Q24 / 1024 */"]
    L += arr("FM6_EXP2", "uint32_t", [int(round((1 << 30) * (2 ** (i / 1024) - 1))) for i in range(1025)], 8)
    lforatio = int(4437500000.0 * CTL / FS)
    L += arr("FM6_LFO_DELTA", "uint32_t", [int(s * lforatio) for s in FM6_LFO_SOURCE], 8)
    base, step = 50857777, (1 << 24) // 12
    det = []
    for mn in range(128):
        lf = base + step * mn
        det.append(int(round(0.0209 * math.exp(-0.396 * (lf / (1 << 24))) / 7 * lf)))
    L += arr("FM6_DETUNE", "uint16_t", det, 16)
    L += arr("FM6_FINE", "int32_t", [int(math.floor(24204406.323123 * math.log(1 + 0.01 * f) + 0.5)) for f in range(100)], 10)
    return L


def main(path):
    L = ["/* generated by tools/gen_tables.py */", "#pragma once", "#include <stdint.h>",
         f"#define FS {FS}", f"#define CTL {CTL}",
         "#ifdef HOST", "#define TABLE_FAST", "#else",
         '#define TABLE_FAST __attribute__((section(".fast_data")))   /* (in the RAM beside the hot code: app.ld) */',
         "#endif", ""]
    # (1025 points: SINE[i + 1] needs no wrap)
    L += arr("SINE", "int16_t", [int(round(32767 * math.sin(2 * math.pi * i / 1024))) for i in range(1025)],
             attr=" TABLE_FAST")   # (read by every operator every sample: in the RAM beside the hot code)
    # phase increment (2^32 = one cycle a sample) of pitch p in 1/16 semitone, MIDI 0..127: the top octave
    inc = []
    for p in range(128 * 16):
        f = 440.0 * 2 ** ((p / 16 - 69) / 12)
        inc.append(min(int(f / FS * 2 ** 32), 0x7FFFFFFF))
    L += arr("PITCH_OCT", "uint32_t", inc[2048 - 192:], 8)
    L += ["/* phase increment of pitch p, 1/16 semitone 0..2047: the top octave's entry, k octaves down */",
          "static inline __attribute__((always_inline)) uint32_t pitch_inc(int32_t p)",
          "{",
          "    uint32_t k;",
          "    if (p < 0)",
          "        p = 0;",
          "    if (p > 2047)",
          "        p = 2047;",
          "    k = (2047u - (uint32_t)p) / 192u;",
          "    return PITCH_OCT[(uint32_t)p + 192u * k - (2048u - 192u)] >> k;",
          "}"]
    # 2^(i / 256), Q16, i = 0..256: the fractional part of exp2 (dsp.h exp2_q16)
    L += arr("EXP2_FRAC", "uint32_t", [int(round(65536 * 2 ** (i / 256))) for i in range(257)], 8)
    # log2(1 + i / 256), Q16, i = 0..256 (dsp.h log2_q16)
    L += arr("LOG2_FRAC", "uint32_t", [int(round(65536 * math.log2(1 + i / 256))) for i in range(257)], 8)
    # tanh(x) for x = 0 .. 4 in 512 steps, Q15 (dsp.h tanh_q15)
    L += arr("TANH_Q15", "int16_t", [int(round(32767 * math.tanh(4 * i / 512))) for i in range(513)])
    # the state-variable filter's g = tan(pi f / FS), f linear 0 .. 0.45 FS in 512 steps, Q24
    fmax = 0.45 * FS
    L += [f"#define TANG_FMAX {int(fmax)}"]
    L += arr("TAN_G", "uint32_t", [int(round((1 << 24) * math.tan(math.pi * (fmax * i / 512) / FS))) for i in range(513)], 8)
    # the grain buffers (mol.c): mu-law, 8 bits a sample. MULAW_ENC: |x| >> 4 (0 .. 2047) -> the 7-bit
    # magnitude code; MULAW_DEC: the code -> |x| (Q15). Read every sample: beside the hot code
    mu = 255.0
    enc = [min(127, int(round(127 * math.log1p(mu * (i * 16 + 8) / 32768) / math.log1p(mu)))) for i in range(2048)]
    dec = [int(round(32767 * ((1 + mu) ** (c / 127) - 1) / mu)) for c in range(128)]
    L += arr("MULAW_ENC", "uint8_t", enc, 24, attr=" TABLE_FAST")
    L += arr("MULAW_DEC", "int16_t", dec, 16, attr=" TABLE_FAST")
    # a Hann window, 257 points, Q15 (the grains' window: one read a sample instead of a sine squared)
    L += arr("HANN", "int16_t", [int(round(32767 * 0.5 * (1 - math.cos(2 * math.pi * i / 256)))) for i in range(257)],
             attr=" TABLE_FAST")
    L += fm6_tables()
    L += wave_tables()
    Path(path).write_text("\n".join(L) + "\n")
    print(f"tables: {path}")


if __name__ == "__main__":
    main(sys.argv[1])
