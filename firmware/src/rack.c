/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR: the rack -- twelve processor modules, always there, shared by the six slots as a pedalboard is by the
 * synthesizers plugged into it. Two things are drawn, by two buttons:
 *   - a slot's patch (HOME, slots.c) says which modules the slot's generator is wired into, and how much
 *   - the rack's patch (SAVE, here) says every module's values, and how the modules are wired to each other: an
 *     order the sound runs through, and from each module, one or two cables to modules later in that order, and/or
 *     out to the master. The order keeps it feed-forward: no loop can form between modules.
 * A module hears the sum of what is wired into it, from the slots and from the modules before it; its output goes
 * down every cable from it, as a mult would. Where a module sits on the screen is where it sounds: the three
 * columns are three places in the stereo image (the plate is stereo of its own). Every module's output passes a
 * guard (a level rider and a ceiling, as Vascular's every processor) so a resonance or a feedback never pins the bus.
 * Nothing in the rack bends a pitch: no swept delays, no wobbling shifts, delay times that change cross-fade.
 *
 * The twelve, after the survey of Vascular's processors (docs/PROCESSORS.md), as the screen has them:
 *   FLT  FILTER    a resonant state-variable filter, low-, band- or high-pass, its cutoff drifting slowly if asked
 *   FLD  FOLD      a wavefolder: the wave driven into a window and reflected back, twice
 *   MU   MULAW     the telephone: a lower rate, mu-law with fewer bits, filtered before and after
 *   SHF  SHIFT     a frequency shifter (Hilbert pair and a quadrature sine): every partial moved by the same
 *                  hertz, up or down, or both (ring modulation) at the middle; feedback stacks the shifts
 *   MDL  MODAL     three resonators tuned to the scale's root: struck metal, bars, bells
 *   STR  STRING    three Karplus strings tuned to the root, detuned: everything that passes rings
 *   VOX  VOWEL     three formants between a, e, i, o, u
 *   DLY  DELAY     Tonitor's delay: 20 ms .. 1.5 s, feedback, damping (8-bit line at half the rate)
 *   TAP  TAPE      a tape echo: saturation, age, a faint wow (8-bit line at half the rate)
 *   RVS  REVERSE   windows of the sound played backwards, at their pitch, fed back if asked
 *   SPR  SPRING    a spring: allpass chirps in a short tank, a resonance for its tone
 *   PLT  PLATE     Tonitor's Dattorro plate, stereo */

enum { RM_FLT, RM_FLD, RM_MU, RM_SHF, RM_MDL, RM_STR, RM_VOW, RM_DLY, RM_TAP, RM_RVS, RM_SPR, RM_PLT, NMOD };
static const char *const RM_NAME[NMOD] = {"FILTER", "FOLD", "MULAW", "SHIFT", "MODAL", "STRING", "VOWEL", "DELAY",
                                          "TAPE", "REVERSE", "SPRING", "PLATE"};
/* each module's character (Vascular's): a slot is never wired into two of one, nor into two destructive ones */
enum { CH_SPECTRAL, CH_DESTRUCTIVE, CH_MODULATION, CH_RESONANT, CH_TEMPORAL, CH_SPACE };
static const uint8_t RM_CHAR[NMOD] = {CH_SPECTRAL, CH_DESTRUCTIVE, CH_DESTRUCTIVE, CH_MODULATION, CH_RESONANT,
                                      CH_RESONANT, CH_SPECTRAL, CH_TEMPORAL, CH_TEMPORAL, CH_TEMPORAL, CH_SPACE, CH_SPACE};
static const uint8_t RM_RANK[6] = {0, 0, 1, 1, 2, 3};   /* where a character tends to sit in the rack's order */
static const uint8_t RM_HARSH[NMOD] = {0, 2, 2, 1, 0, 0, 0, 0, 0, 1, 0, 0};   /* what each costs of a patch's budget */
static const uint8_t RM_W[NMOD] = {12, 4, 5, 7, 11, 10, 9, 11, 10, 9, 9, 11};   /* Vascular's foundation weights, x 10 */
/* the values a module comes back to (SAVE draws around them, as Vascular draws a processor's) */
static const uint8_t RM_HOME[NMOD][4] = {
    {20, 55, 40, 30}, {45, 45, 20, 70}, {45, 60, 50, 30}, {40, 30, 70, 50}, {40, 55, 40, 50}, {35, 60, 40, 60},
    {50, 50, 50, 30}, {65, 40, 40, 60}, {50, 30, 45, 40}, {40, 60, 30, 50}, {50, 40, 50, 50}, {65, 60, 40, 55}};
#define RM_COLS 3
static const int16_t RM_PAN[RM_COLS] = {-19661, 0, 19661};   /* a column's place: left, middle, right */

/* the rack's patch (SAVE): the main loop writes the buffer the ISR is not using and publishes it */
typedef struct {
    uint8_t p[NMOD][4];          /* every module's values, 0 .. 100 */
    uint8_t order[NMOD];         /* the order the sound runs through them */
    uint16_t to[NMOD];           /* bit n: module m's output wired into module n (n later in the order) */
    uint16_t out;                /* bit m: module m's output to the master */
    uint32_t seed, serial;
} rack_patch_t;
static rack_patch_t rbuf[2];
static volatile uint8_t r_pub, r_use;
static volatile int32_t rack_root = 48;        /* the scale's root, MIDI: what MODAL and STRING are tuned to */
static int32_t mbus[NMOD][CTL];                /* each module's input, then its output, this block */

/* ------------------------------------------------------------- shared --- */
/* a constant-peak-gain resonator (Smith and Angell): zeros at DC and Nyquist, poles at f, radius r. Q28 coefficients */
typedef struct { int32_t a1, a2, g; } res_c;
typedef struct { int32_t y1, y2, x1, x2; } res_s;
/* f in Hz (Q8), Q in Q8: 1 - r = pi f / (Q FS) (to the second order), cos 2 theta = 1 - 2 sin^2 theta, theta = pi f / FS */
static void res_coef(res_c *c, int32_t hz8, int32_t q8)
{
    int64_t x, s, r, c2, x2;
    hz8 = clampi(hz8, 20 << 8, 16000 << 8);
    q8 = clampi(q8, 1 << 8, 400 << 8);
    x = ((int64_t)hz8 * 76491) >> 8;                      /* theta = pi f / FS, Q30 */
    x2 = (x * x) >> 30;
    s = x - ((((x2 * x) >> 30) * 178956971) >> 30) + ((((((x2 * x2) >> 30) * x) >> 30) * 8947849) >> 30);   /* sin theta:
                                                          * x - x^3 / 6 + x^5 / 120, Q30 */
    c2 = (1LL << 30) - 2 * ((s * s) >> 30);               /* cos 2 theta, Q30 */
    {
        uint32_t inv = 0xFFFFFFFFu / (uint32_t)q8;        /* 1 / Q, Q24 */
        int64_t om = (x * inv) >> 24;                     /* 1 - r = theta / Q, Q30 */
        r = (1LL << 30) - om + ((om * om) >> 31);
    }
    c->a1 = (int32_t)(((2 * r * c2) >> 30) >> 2);         /* 2 r cos, Q28 */
    c->a2 = (int32_t)(-((r * r) >> 30) >> 2);             /* -r^2, Q28 */
    c->g = (int32_t)(((1LL << 30) - ((r * r) >> 30)) >> 3);   /* (1 - r^2) / 2, Q28 */
}
AINL int32_t res_run(res_s *s, const res_c *c, int32_t x)
{
    int32_t y = (int32_t)(((int64_t)c->a1 * s->y1 + (int64_t)c->a2 * s->y2 + (int64_t)c->g * (x - s->x2)) >> 28);
    s->y2 = s->y1;
    s->y1 = y;
    s->x2 = s->x1;
    s->x1 = x;
    return y;
}
/* a note's frequency, Hz Q8 (MIDI, + cents) */
static int32_t note_hz8(int32_t note, int32_t cents)
{
    return (int32_t)(((uint64_t)440u * 256u * exp2_q16(((note - 69) * 100 + cents) * 65536 / 1200)) >> 16);
}
AINL uint8_t mu_enc8(int32_t x) { return gr_enc(x); }
AINL int32_t mu_dec8(uint8_t b) { return gr_dec(b); }

/* ------------------------------------------------------------- FILTER --- */
typedef struct { svf_t f; uint32_t lfo; } m_flt_t;
static void m_flt(const uint8_t *p, m_flt_t *s, int32_t *x)
{
    int32_t oct = 321600 + p[1] * (9 * 65536 / 100), hz, q, type;   /* 30 Hz up 9 octaves */
    uint32_t i;
    svf_c c;
    if (p[3] > 10) {                                    /* a drift: 0.02 .. 0.1 Hz, up to +-2/3 octave */
        s->lfo += 62000u + (uint32_t)p[3] * 2400u;
        oct += mulq15(sin_ph(s->lfo), p[3] * (2 * 65536 / 300));
    }
    hz = (int32_t)(exp2_q16(clampi(oct, 4 << 16, 14 << 16)) >> 16);
    q = 2896 + p[2] * (10 * 4096 - 2896) / 100;
    svf_coef(&c, clampi(hz, 30, 16000), q);
    type = p[0] < 34 ? 1 : p[0] < 67 ? 2 : 3;
    for (i = 0; i < CTL; i++)
        x[i] = svf_run(&s->f, &c, x[i], type);
}

/* --------------------------------------------------------------- FOLD --- */
typedef struct { int32_t lp; } m_fld_t;
AINL int32_t fold_into(int32_t x, int32_t w)      /* reflected into -w .. w */
{
    int32_t p = 4 * w;
    if (x > w || x < -w) {
        x = ((x + w) % p + p) % p;
        x = x < 2 * w ? x - w : 3 * w - x;
    }
    return x;
}
static void m_fld(const uint8_t *p, m_fld_t *s, int32_t *x)
{
    int32_t g1 = (int32_t)(exp2_q16((p[0] * (5 * 65536 / 100)) - 65536) >> 4),   /* x 0.5 .. x 16, Q12 */
            w = 2621 + p[1] * (19005 - 2621) / 100,                              /* 0.08 .. 0.58 */
            g2 = 4096 + p[2] * (4 * 4096) / 100,                                 /* x 1 .. x 5, Q12 */
            tone = 1400 + p[3] * (29430 - 1400) / 100,                           /* 300 Hz .. 16 kHz */
            back = (int32_t)((Q15 * 6000u) / (uint32_t)w);                       /* 0.18 / w, Q15 */
    uint32_t i;
    for (i = 0; i < CTL; i++) {
        int32_t y = fold_into(mulq12(x[i], g1), w);
        y = fold_into(mulq12(y, g2), w);
        y = mulq15(y, back > 4 * Q15 ? 4 * Q15 : back);
        s->lp += mulq15(y - s->lp, tone);
        x[i] = s->lp;
    }
}

/* -------------------------------------------------------------- MULAW --- */
/* p0 the rate (FS down to FS / 8, fractional), p1 the bits (8 down to 4, sign included), p2 the filter before and
 * after (0.05 .. 0.5 of the lower rate), p3 the drive into it (x 1 .. x 4: mu-law lifts the quiet) */
typedef struct { uint32_t ph; int32_t hold, a1, a2, b1, b2; } m_mu_t;
static void m_mu(const uint8_t *p, m_mu_t *s, int32_t *x)
{
    uint32_t div16 = 16u + (uint32_t)p[0] * 112u / 100u, inc = (uint32_t)(0xFFFFFFFFu / div16) << 4;   /* 1 .. 8, Q4 */
    uint32_t shift = 4u - (uint32_t)(p[1] * 4 / 101), i;      /* code bits dropped: 0 .. 3 */
    int32_t hz = (int32_t)((uint32_t)FS * 16u / div16) * (5 + p[2] * 45 / 100) / 100;
    int32_t k = clampi((hz * 4781) >> 10, 300, 32000);   /* a one-pole's step: 2 pi f / FS, Q15 */
    int32_t drive = 4096 + p[3] * (3 * 4096) / 100;
    for (i = 0; i < CTL; i++) {
        uint32_t was = s->ph;
        int32_t v = clampi(mulq12(x[i], drive), -32767, 32767);
        s->a1 += mulq15(v - s->a1, k);                  /* two poles before */
        s->a2 += mulq15(s->a1 - s->a2, k);
        s->ph += inc;
        if (s->ph < was) {                              /* a sample taken, coded, decoded */
            uint8_t b = mu_enc8(s->a2);
            b = (uint8_t)((b & 0x80u) | (((b & 0x7Fu) >> shift) << shift));
            s->hold = mu_dec8(b);
        }
        s->b1 += mulq15(s->hold - s->b1, k);            /* two poles after */
        s->b2 += mulq15(s->b1 - s->b2, k);
        x[i] = mulq12(s->b2, (4096 * 4096) / drive);
    }
}

/* -------------------------------------------------------------- SHIFT --- */
/* Niemitalo's Hilbert pair: two chains of four second-order allpass sections in z^-2, one path a sample late, 90
 * degrees apart from ~20 Hz to ~20 kHz. p0 the shift, 5 Hz .. 2 kHz (log); p1 the feedback through 25 ms, damped;
 * p2 the direction: down below 45, up above 55, both (ring modulation) between; p3 the feedback's tone, 1 .. 8 kHz.
 * The shift is fixed while it sounds: nothing sweeps */
static const int32_t HB_A[4] = {128688190, 235208111, 262153419, 267764168};   /* a^2, Q28 */
static const int32_t HB_B[4] = {43421716, 196770956, 253765378, 265911936};
#define SHF_N 1104u              /* 25 ms */
typedef struct { int32_t ax[4][2], ay[4][2], bx[4][2], by[4][2], bd; uint32_t ph, lfo, w; int32_t lp; int16_t line[SHF_N]; } m_shf_t;
AINL int32_t hb_sec(int32_t c, int32_t x, int32_t *xs, int32_t *ys)   /* y = c (x + y[n-2]) - x[n-2] */
{
    int32_t y = (int32_t)(((int64_t)c * (x + ys[1])) >> 28) - xs[1];
    xs[1] = xs[0];
    xs[0] = x;
    ys[1] = ys[0];
    ys[0] = y;
    return y;
}
static void m_shf(const uint8_t *p, m_shf_t *s, int32_t *x)
{
    int32_t oct = 152000 + p[0] * (8 * 65536 / 100 + 3932), fb = p[1] * (27853 / 100), hz8, dir;
    int32_t tone = 4600 + p[3] * (26000 - 4600) / 100;
    uint32_t inc, i, k;
    hz8 = (int32_t)(exp2_q16(clampi(oct, 2 << 16, 11 << 16)) >> 8);
    inc = hz8_inc(hz8);
    dir = p[2] < 45 ? -1 : p[2] > 55 ? 1 : 0;
    for (i = 0; i < CTL; i++) {
        int32_t in = x[i] + mulq15(s->lp, fb), a = in, b = in, y, c, sn;
        uint32_t r = s->w + SHF_N - (SHF_N - 2u);
        for (k = 0; k < 4u; k++) {
            a = hb_sec(HB_A[k], a, s->ax[k], s->ay[k]);
            b = hb_sec(HB_B[k], b, s->bx[k], s->by[k]);
        }
        { int32_t t = s->bd; s->bd = b; b = t; }        /* (the second path a sample late) */
        s->ph += inc;
        c = sin_ph(s->ph + 0x40000000u);
        sn = sin_ph(s->ph);
        y = dir > 0 ? mulq15(a, c) - mulq15(b, sn) : dir < 0 ? mulq15(a, c) + mulq15(b, sn) : mulq15(a, c);
        s->line[s->w] = (int16_t)clampi(y, -32767, 32767);
        if (++s->w >= SHF_N)
            s->w = 0;
        r = r >= SHF_N ? r - SHF_N : r;
        s->lp += mulq15(s->line[r] - s->lp, tone);      /* (the feedback's damping) */
        x[i] = y;
    }
}

/* -------------------------------------------------------------- MODAL --- */
/* three resonators: the root in an octave (p0: 1 .. 4 octaves up from the root at C1 .. B1), its modes at f s and
 * f s^2 (p2: s 1 .. 3.4); Q 4 .. 325 (p1, log); p3 the upper modes' level. Tuned once a block (cheap) */
typedef struct { res_c c[3]; res_s r[3]; int32_t key; } m_mdl_t;
static void m_mdl(const uint8_t *p, m_mdl_t *s, int32_t *x)
{
    int32_t key = p[0] | p[1] << 8 | p[2] << 16 | rack_root << 24, lvl = Q15 / 4 + p[3] * (3 * Q15 / 4 / 100), i, k;
    if (key != s->key) {
        int32_t note = 24 + (rack_root % 12) + 12 * (p[0] * 4 / 101), f = note_hz8(note, 0);
        int32_t sr = 65536 + p[2] * (int32_t)(157286 / 100);           /* s, Q16: 1 .. 3.4 */
        int32_t q8 = (int32_t)(exp2_q16(p[1] * (622 * 65536 / 10000)) >> 8) * 4;   /* 4 .. ~300 (2^6.2), Q8 */
        for (k = 0; k < 3; k++) {
            res_coef(&s->c[k], f, q8);
            f = (int32_t)(((int64_t)f * sr) >> 16);
        }
        s->key = key;
    }
    for (i = 0; i < CTL; i++) {
        int32_t in = x[i] >> 1;
        x[i] = 2 * (res_run(&s->r[0], &s->c[0], in) + mulq15(res_run(&s->r[1], &s->c[1], in) + res_run(&s->r[2], &s->c[2], in), lvl));
    }
}

/* ------------------------------------------------------------- STRING --- */
/* three Karplus strings: the root in an octave (p0), the ring time 0.08 .. 8 s (p1: the loop gain from it, by the
 * pitch), two more strings +0.9 % and -0.7 % off (p2: their level), the loop's low-pass 400 Hz .. 14 kHz (p3); the
 * input low-passed at ~4 kHz */
#define STR_N 1400u              /* C1, 32.7 Hz */
typedef struct { int16_t line[3][STR_N]; uint32_t w; int32_t lp[3], in; } m_str_t;
static void m_str(const uint8_t *p, m_str_t *s, int32_t *x)
{
    int32_t note = 24 + (rack_root % 12) + 12 * (p[0] * 4 / 101), hz8 = note_hz8(note, 0), len8[3], g, tone, lvl, k;
    int32_t t100 = (int32_t)((8u * exp2_q16(p[1] * (664 * 65536 / 10000))) >> 16);   /* 0.08 .. 8 s, x 100 (2^6.64 = 100) */
    uint32_t i;
    len8[0] = (int32_t)(((uint32_t)FS << 16) / (uint32_t)hz8);   /* samples, Q8 */
    len8[1] = (int32_t)(((int64_t)len8[0] * 64936) >> 16);   /* +0.9 % */
    len8[2] = (int32_t)(((int64_t)len8[0] * 65995) >> 16);   /* -0.7 % */
    /* the loop gain: 60 dB in T s, a trip each 1 / f: 2^(-9.966 / (T f)) */
    {   /* 9.966 / (T f), Q16 = 9.966 x 65536 x 100 x 256 / (t100 x hz8) */
        uint32_t den = ((uint32_t)t100 * (uint32_t)hz8) >> 8;
        g = (int32_t)(exp2_q16(-(int32_t)(65313178u / (den ? den : 1u))) >> 1);
    }
    g = clampi(g, 6554, 32751);
    tone = 2800 + p[3] * (30500 - 2800) / 100;
    lvl = p[2] * (Q15 / 100);
    for (i = 0; i < CTL; i++) {
        int32_t y = 0;
        s->in += mulq15(x[i] - s->in, 15700);           /* (~4 kHz) */
        for (k = 0; k < 3; k++) {
            int32_t d8 = clampi(len8[k], 4 << 8, (int32_t)(STR_N - 3u) << 8), v;
            uint32_t rp = (s->w << 8) + (STR_N << 8) - (uint32_t)d8, i0 = (rp >> 8) % STR_N, i1 = i0 + 1u >= STR_N ? 0u : i0 + 1u;
            v = s->line[k][i0] + (((s->line[k][i1] - s->line[k][i0]) * (int32_t)(rp & 255u)) >> 8);
            s->lp[k] += mulq15(v - s->lp[k], tone);
            s->line[k][s->w] = (int16_t)clampi((s->in >> 2) + mulq15(s->lp[k], g), -32767, 32767);
            y += k ? mulq15(v, lvl) : v;
        }
        if (++s->w >= STR_N)
            s->w = 0;
        x[i] = y;
    }
}

/* -------------------------------------------------------------- VOWEL --- */
/* three formants (a e i o u: F1 F2 F3) morphing along p0, the whole moved by p1 (x 2^-0.5 .. x 2^0.5), their Q by p2
 * (4 .. 20), the drive into them by p3 (x 1 .. x 8, tanh) */
static const int16_t VF[5][3] = {{600, 1040, 2250}, {400, 1620, 2400}, {250, 1750, 2600}, {400, 750, 2400}, {350, 600, 2400}};
typedef struct { res_c c[3]; res_s r[3]; int32_t key; } m_vow_t;
static void m_vow(const uint8_t *p, m_vow_t *s, int32_t *x)
{
    int32_t key = p[0] | p[1] << 8 | p[2] << 16, drive = 4096 + p[3] * (7 * 4096) / 100, i, k;
    if (key != s->key) {
        int32_t pos = p[0] * 4 * 256 / 100, seg = pos >> 8, fr = pos & 255;
        int32_t sc = (int32_t)exp2_q16((p[1] - 50) * (65536 / 100));   /* x 2^-0.5 .. 2^0.5, Q16 */
        int32_t q8 = (4 + p[2] * 16 / 100) << 8;
        seg = seg > 3 ? 3 : seg;
        for (k = 0; k < 3; k++) {
            int32_t f = VF[seg][k] + (((VF[seg + 1][k] - VF[seg][k]) * fr) >> 8);
            res_coef(&s->c[k], (int32_t)(((int64_t)f * sc) >> 8), q8);
        }
        s->key = key;
    }
    for (i = 0; i < CTL; i++) {
        int32_t in = tanh_q15(mulq12(x[i], drive)) >> 1;
        x[i] = 3 * (res_run(&s->r[0], &s->c[0], in) + mulq15(res_run(&s->r[1], &s->c[1], in), 19661) +
                    mulq15(res_run(&s->r[2], &s->c[2], in), 13107));
    }
}

/* -------------------------------------------------------------- DELAY --- */
/* an 8-bit mu-law line at FS / 2 (a sample at the half rate, read with linear interpolation) */
AINL int32_t mu_tap(const uint8_t *line, uint32_t n, uint32_t w, int32_t t8)   /* t8 samples ago, Q8 */
{
    uint32_t q = (w << 8) + (n << 8) - (uint32_t)t8, i0 = (q >> 8) % n, f = q & 255u;
    int32_t y = mu_dec8(line[i0]);
    return y + (((mu_dec8(line[i0 + 1u >= n ? 0u : i0 + 1u]) - y) * (int32_t)f) >> 8);
}
#define XF_PAIRS 512u            /* a change of time: the old head fades out, the new in, over 23 ms */
/* Tonitor's delay: TIME 20 ms .. 1.5 s (p0, log), feedback 0 .. 0.92 (p1), damping (p2: the echoes' low-pass, none
 * .. 0.12), level (p3: 0.25 .. 1). The echoes alone come out. A change of time cross-fades (no pitch slide) */
#define DLY_N 33075u             /* 1.5 s at FS / 2 */
typedef struct { uint8_t line[DLY_N]; uint32_t w; int32_t t, t0, xf, lp, prev; } m_dly_t;
static void m_dly(const uint8_t *p, m_dly_t *s, int32_t *x)
{
    int32_t ms = (int32_t)((20u * exp2_q16(p[0] * (6229 * 65536 / 100000))) >> 16);   /* 20 x 75^(p / 100) */
    int32_t want = clampi(ms * 2205 / 100 * 256, 2 * 256, (int32_t)(DLY_N - 4u) * 256);
    int32_t fb = p[1] * (LV(0.92) / 100), damp = Q15 - p[2] * (LV(0.88) / 100), lvl = 8192 + p[3] * (24576 / 100);
    uint32_t i;
    if (!s->t)
        s->t = want;
    if (want != s->t && !s->xf) {
        s->t0 = s->t;
        s->t = want;
        s->xf = XF_PAIRS;
    }
    for (i = 0; i + 1u < CTL; i += 2u) {
        int32_t in = (x[i] + x[i + 1u]) >> 1, y = mu_tap(s->line, DLY_N, s->w, s->t), o0;
        if (s->xf) {
            int32_t yo = mu_tap(s->line, DLY_N, s->w, s->t0);
            y = yo + (((y - yo) * (int32_t)(XF_PAIRS - (uint32_t)s->xf)) >> 9);
            s->xf--;
        }
        s->lp += mulq15(y - s->lp, damp);
        s->line[s->w] = mu_enc8(clampi(in + mulq15(s->lp, fb), -32767, 32767));
        if (++s->w >= DLY_N)
            s->w = 0;
        y = mulq15(s->lp, lvl);
        o0 = (s->prev + y) >> 1;
        s->prev = y;
        x[i] = o0;
        x[i + 1u] = y;
    }
}

/* --------------------------------------------------------------- TAPE --- */
/* an echo on an 8-bit mu-law line at FS / 2: time 25 .. 900 ms (p0), wow (p1: a slow wander of the time, at most
 * +-0.25 % -- the tape's breath, not a bend), feedback 0 .. 0.94 (p2) through the head's saturation, age (p3: the
 * loop's low-pass from 12 kHz down to 800 Hz). The echoes alone come out. A change of time cross-fades */
#define TAP_N 19900u             /* 0.9 s at FS / 2 */
typedef struct { uint8_t line[TAP_N]; uint32_t w; int32_t t, t0, xf, lp, prev, wn, wt; rnd_t r; } m_tap_t;
static void m_tap(const uint8_t *p, m_tap_t *s, int32_t *x)
{
    int32_t want = (25 + p[0] * 875 / 100) * 2205 / 100 * 256;   /* samples at FS / 2, Q8 */
    int32_t fb = p[2] * (30802 / 100), age = 31000 - p[3] * (31000 - 3600) / 100;
    uint32_t i;
    if (!s->t)
        s->t = want;
    if (want != s->t && !s->xf) {
        s->t0 = s->t;
        s->t = want;
        s->xf = XF_PAIRS;
    }
    if (!(s->r.s))
        rnd_seed(&s->r, 0x7A9Eu);
    if ((rnd_next(&s->r) & 255u) < 2u)                 /* the wow: a new place every ~0.2 s, glided to */
        s->wt = mulq15(rnd_bi(&s->r), p[1] * 82 / 100);  /* (Q15 of the time: at most 0.25 %) */
    s->wn += (s->wt - s->wn) >> 7;
    for (i = 0; i + 1u < CTL; i += 2u) {
        int32_t in = (x[i] + x[i + 1u]) >> 1, y, o0, tt = s->t + (int32_t)(((int64_t)s->t * s->wn) >> 15);
        y = mu_tap(s->line, TAP_N, s->w, clampi(tt, 4 << 8, (int32_t)(TAP_N - 4u) << 8));
        if (s->xf) {
            int32_t yo = mu_tap(s->line, TAP_N, s->w, s->t0);
            y = yo + (((y - yo) * (int32_t)(XF_PAIRS - (uint32_t)s->xf)) >> 9);
            s->xf--;
        }
        s->lp += mulq15(y - s->lp, age);
        s->line[s->w] = mu_enc8(tanh_q15((in + mulq15(s->lp, fb)) * 3 / 2) * 2 / 3);
        if (++s->w >= TAP_N)
            s->w = 0;
        o0 = (s->prev + s->lp) >> 1;
        s->prev = s->lp;
        x[i] = o0;
        x[i + 1u] = s->lp;
    }
}

/* ------------------------------------------------------------ REVERSE --- */
/* windows of 60 .. 400 ms (p0) read backwards at their own pitch: two heads half a window apart, each a triangle, so
 * the sum stays level; a low-pass on it, 1.4 .. 13 kHz (p1); fed back into the line (p2: 0 .. 0.7: the reversed
 * reversed again, smeared); p3 the windows' spread, how far back they reach (one to three windows). Its line: 1.2 s
 * at FS / 2, 8-bit mu-law */
#define RVS_N 26460u
typedef struct { uint8_t line[RVS_N]; uint32_t w, ph; int32_t lp, prev; } m_rvs_t;
AINL int32_t rvs_head(const m_rvs_t *s, uint32_t ph, uint32_t span, uint32_t back)   /* a head at phase ph */
{
    uint32_t d = back + (uint32_t)(((uint64_t)ph * span) >> 32) + 4u, q = s->w + RVS_N - d;
    int32_t tri = (int32_t)(ph >> 16), y;
    q = q >= RVS_N ? q - RVS_N : q;
    tri = tri < 32768 ? tri : 65535 - tri;              /* 0 .. 32767 .. 0 */
    y = mu_dec8(s->line[q]);
    return mulq15(y, tri);
}
static void m_rvs(const uint8_t *p, m_rvs_t *s, int32_t *x)
{
    uint32_t win = (60u + (uint32_t)p[0] * 340u / 100u) * 2205u / 100u;   /* samples at FS / 2 */
    uint32_t span = 2u * win, back = win * (uint32_t)p[3] / 50u, inc = 0xFFFFFFFFu / (win ? win : 1u), i;
    int32_t tone = 6400 + p[1] * (31000 - 6400) / 100, fb = p[2] * (LV(0.7) / 100);
    if (span + back > RVS_N - 8u)
        back = RVS_N - 8u - span;
    for (i = 0; i + 1u < CTL; i += 2u) {
        int32_t y;
        s->line[s->w] = mu_enc8(clampi(((x[i] + x[i + 1u]) >> 1) + mulq15(s->lp, fb), -32767, 32767));
        if (++s->w >= RVS_N)
            s->w = 0;
        s->ph += inc;
        y = rvs_head(s, s->ph, span, back) + rvs_head(s, s->ph + 0x80000000u, span, back);
        s->lp += mulq15(y - s->lp, tone);
        x[i] = (s->prev + s->lp) >> 1;
        x[i + 1u] = s->lp;
        s->prev = s->lp;
    }
}

/* ------------------------------------------------------------- SPRING --- */
/* the input through a resonance (p3: 400 Hz .. 5.9 kHz, the spring's tone), into a tank of 30 .. 120 ms (p1) whose
 * loop carries a chain of six short allpasses (the chirps; p0: their gain, 0.3 .. 0.75) and a low-pass, its gain
 * 0.2 .. 0.93 (p2). Mono, at the full rate */
#define SPR_N 5300u              /* 120 ms */
static const uint16_t SPR_AP[6] = {37, 53, 71, 89, 113, 149};
#define SPR_AP_N 520u
typedef struct { int16_t tank[SPR_N]; int16_t ap[SPR_AP_N]; uint32_t w, aw[6]; int32_t lp; res_c rc; res_s rs; int32_t key; } m_spr_t;
static void m_spr(const uint8_t *p, m_spr_t *s, int32_t *x)
{
    int32_t g = 9830 + p[0] * (24576 - 9830) / 100, fb = 6554 + p[2] * (30474 - 6554) / 100, k;
    uint32_t len = (30u + (uint32_t)p[1] * 90u / 100u) * 441u / 10u, i, base;
    int32_t key = p[3];
    if (key != s->key || !s->rc.g) {
        res_coef(&s->rc, (400 + p[3] * 55) << 8, 2 << 8);
        s->key = key;
    }
    for (i = 0; i < CTL; i++) {
        uint32_t q = s->w + SPR_N - len;
        int32_t y, v = res_run(&s->rs, &s->rc, x[i]);
        q = q >= SPR_N ? q - SPR_N : q;
        y = s->tank[q];
        base = 0;
        for (k = 0; k < 6; k++) {                       /* Schroeder allpasses: w = y + g d; out = d - g w */
            uint32_t n = SPR_AP[k], r = s->aw[k];
            int32_t d = s->ap[base + r], ww = y + mulq15(d, g);
            s->ap[base + r] = (int16_t)clampi(ww, -32767, 32767);
            y = d - mulq15(ww, g);
            s->aw[k] = r + 1u >= n ? 0u : r + 1u;
            base += n;
        }
        s->lp += mulq15(y - s->lp, 20000);
        s->tank[s->w] = (int16_t)clampi(v + mulq15(s->lp, fb), -32767, 32767);
        if (++s->w >= SPR_N)
            s->w = 0;
        x[i] = y + (y >> 1);                           /* (x 1.5: its loop loses what the allpasses spread) */
    }
}

/* -------------------------------------------------------------- PLATE --- */
#include "reverb.c"

/* -------------------------------------------------------------- the rack --- */
static m_flt_t m_flt_s POOL;
static m_fld_t m_fld_s POOL;
static m_mu_t m_mu_s POOL;
static m_shf_t m_shf_s POOL;
static m_mdl_t m_mdl_s POOL;
static m_str_t m_str_s POOL;
static m_vow_t m_vow_s POOL;
static m_dly_t m_dly_s POOL;
static m_tap_t m_tap_s POOL;
static m_rvs_t m_rvs_s POOL;
static m_spr_t m_spr_s POOL;
static int32_t rm_env[NMOD], rm_g[NMOD];       /* each module's guard: the peak it follows, its gain */
static volatile int32_t rm_meter[NMOD];        /* each module's output, its last peaks (the screen's) */
static uint16_t rm_quiet[NMOD];                /* blocks its output has been near silence */
static uint8_t rm_plate_on;                    /* (the plate's parameters set: rv_param) */
#define RM_TAIL 2756u            /* a module nothing feeds goes on until its output has been near silence 2 s */

/* the guard: a peak rider (Tonitor's limiter: instant attack, ~200 ms release) to 0.5 */
#define RM_CEIL 16384
static void rm_guard(uint32_t m, int32_t *x)
{
    int32_t pk = 0, want, g0 = rm_g[m], dg;
    uint32_t i;
    for (i = 0; i < CTL; i++) {
        int32_t a = absi(x[i]);
        if (a > pk)
            pk = a;
    }
    if (pk > rm_env[m])
        rm_env[m] = pk;
    else
        rm_env[m] -= rm_env[m] >> 8;
    want = rm_env[m] > RM_CEIL ? (int32_t)((uint32_t)RM_CEIL * 32768u / (uint32_t)rm_env[m]) : Q15;
    if (want > g0 && g0 < Q15)
        want = g0 + ((want - g0) >> 3);
    dg = (want - g0) / (int32_t)CTL;
    for (i = 0; i < CTL; i++) {
        g0 += dg;
        x[i] = mulq15(x[i], g0);
    }
    rm_g[m] = want;
    rm_meter[m] = pk > rm_meter[m] ? pk : rm_meter[m] - (rm_meter[m] >> 4);
}

/* one module over x (mono, in place); the plate, when it goes to the master: into l, r (added) */
static void rm_block(uint32_t m, const uint8_t *p, int32_t *x, int32_t *l, int32_t *r, int to_master)
{
    switch (m) {
    case RM_FLT: m_flt(p, &m_flt_s, x); break;
    case RM_FLD: m_fld(p, &m_fld_s, x); break;
    case RM_MU: m_mu(p, &m_mu_s, x); break;
    case RM_SHF: m_shf(p, &m_shf_s, x); break;
    case RM_MDL: m_mdl(p, &m_mdl_s, x); break;
    case RM_STR: m_str(p, &m_str_s, x); break;
    case RM_VOW: m_vow(p, &m_vow_s, x); break;
    case RM_DLY: m_dly(p, &m_dly_s, x); break;
    case RM_TAP: m_tap(p, &m_tap_s, x); break;
    case RM_RVS: m_rvs(p, &m_rvs_s, x); break;
    case RM_SPR: m_spr(p, &m_spr_s, x); break;
    default: {                                       /* the plate: stereo */
        int32_t pl[CTL], pr[CTL], g = 8192 + p[3] * (24576 / 100), pk = 0;   /* (its level: 0.25 .. 1) */
        uint32_t i;
        if (!rm_plate_on) {
            rv_param(p[0], p[1], p[2], 40);              /* (x 0.64: the tail sums over seconds) */
            rm_plate_on = 1;
        }
        for (i = 0; i < CTL; i++)
            pl[i] = pr[i] = 0;
        rv_block_fp(x, pl, pr, CTL);
        for (i = 0; i < CTL; i++) {
            pl[i] = mulq15(pl[i], g);
            pr[i] = mulq15(pr[i], g);
            if (to_master) {
                l[i] += pl[i];
                r[i] += pr[i];
            }
            x[i] = (pl[i] + pr[i]) >> 1;               /* (mono, for the modules after it) */
            pk = maxi(pk, absi(x[i]));
        }
        rm_meter[m] = pk > rm_meter[m] ? pk : rm_meter[m] - (rm_meter[m] >> 4);
        return;
    }
    }
    rm_guard(m, x);
}

/* the rack's patch published, taken at a block's start */
static void rack_take(void)
{
    if (r_pub != r_use) {
        r_use = r_pub;
        rm_plate_on = 0;                             /* (the plate's values taken at its next block) */
    }
}
/* one block of the rack: mbus holds what the slots send into each module, fed which of them they feed; each
 * module, in the patch's order, runs if something reaches it or its tail still sounds; its output goes down its
 * cables and, as wired, out to the master at its column's place */
static void rack_block(uint32_t fed, int32_t *l, int32_t *r)
{
    const rack_patch_t *rk = &rbuf[r_use];
    uint32_t has = fed, j, i, n;
    for (j = 0; j < NMOD; j++) {
        uint32_t m = rk->order[j], to_master = (rk->out >> m) & 1u;
        if (!((has >> m) & 1u) && rm_quiet[m] >= RM_TAIL)
            continue;                                /* (nothing reaches it, and it is silent) */
        rm_block(m, rk->p[m], mbus[m], l, r, (int)to_master);
        if (rm_meter[m] < 8) {
            if (rm_quiet[m] < 0xFFFFu)
                rm_quiet[m]++;
        } else
            rm_quiet[m] = 0;
        if (rm_quiet[m] >= RM_TAIL)
            continue;                                /* (silent: nothing to pass on) */
        if (to_master && m != RM_PLT) {
            int32_t pl, pr;
            uint32_t ph = (uint32_t)(RM_PAN[m % RM_COLS] + Q15) * 16384u;   /* (equal power) */
            pl = sin_ph(ph + 0x40000000u);
            pr = sin_ph(ph);
            for (i = 0; i < CTL; i++) {
                l[i] += mulq15(mbus[m][i], pl);
                r[i] += mulq15(mbus[m][i], pr);
            }
        }
        for (n = 0; n < NMOD; n++)
            if ((rk->to[m] >> n) & 1u) {
                for (i = 0; i < CTL; i++)
                    mbus[n][i] += mbus[m][i];
                has |= 1u << n;
            }
    }
}

/* SAVE: the rack's patch drawn anew, into the buffer the ISR is not using:
 *   - every module's values: each at its home one time in five, else moved by 15 .. 55 either way (Vascular's)
 *   - the order: by character, mostly -- colour and destruction first, then modulation and resonance, then time,
 *     then space -- but shuffled across those lines often enough that a plate may feed a fold
 *   - from each module, in that order: no cable on (0.55), one (0.38) or two (0.07) to the next four modules; with
 *     none it goes to the master, with one or more it also does, one time in three
 * Returns 0 if the ISR had not taken the patch before (the main loop then tries again) */
static uint32_t rack_serial;
static void rack_draw_into(rack_patch_t *rk, uint32_t seed)
{
    rnd_t r;
    uint32_t m, k, j;
    int32_t key[NMOD];
    static const uint8_t NTO[3] = {55, 38, 7};
    rnd_seed(&r, seed ^ 0x5241434Bu);
    rk->seed = seed;
    rk->serial = ++rack_serial;
    for (m = 0; m < NMOD; m++)
        for (k = 0; k < 4u; k++) {
            int32_t v = RM_HOME[m][k];
            if (!rnd_chance(&r, 20)) {
                int32_t d = rnd_in(&r, 15, 55);
                v += rnd_chance(&r, 50) ? d : -d;
            }
            rk->p[m][k] = (uint8_t)clampi(v, 0, 100);
        }
    for (m = 0; m < NMOD; m++) {
        rk->order[m] = (uint8_t)m;
        key[m] = RM_RANK[RM_CHAR[m]] * 40 + rnd_in(&r, 0, 70);
    }
    for (m = 0; m < NMOD; m++)
        for (k = m + 1u; k < NMOD; k++)
            if (key[rk->order[k]] < key[rk->order[m]]) {
                uint8_t t = rk->order[m];
                rk->order[m] = rk->order[k];
                rk->order[k] = t;
            }
    rk->out = 0;
    for (j = 0; j < NMOD; j++) {
        uint32_t mm = rk->order[j], left = NMOD - 1u - j, nto = left ? rnd_weighted(&r, NTO, 3) : 0u, reach = left < 4u ? left : 4u;
        rk->to[mm] = 0;
        for (k = 0; k < nto && k < reach; k++) {
            uint32_t t = rk->order[j + 1u + (uint32_t)rnd_below(&r, (int32_t)reach)];
            rk->to[mm] |= (uint16_t)(1u << t);
        }
        if (!rk->to[mm] || rnd_chance(&r, 33))
            rk->out |= (uint16_t)(1u << mm);
    }
}
static int rack_draw(uint32_t seed)
{
    uint32_t t0 = fm1_ms;
    while (r_pub != r_use) {                         /* (the ISR takes the last one at a block's start) */
        if ((uint32_t)(fm1_ms - t0) > 40u)
            return 0;
        fm1_wdt_feed();
    }
    rack_draw_into(&rbuf[r_use ^ 1u], seed);
    RING_PUBLISH();
    r_pub = (uint8_t)(r_use ^ 1u);
    return 1;
}

static void rack_init(void)
{
    uint32_t m;
    memset(&m_flt_s, 0, sizeof m_flt_s);
    memset(&m_fld_s, 0, sizeof m_fld_s);
    memset(&m_mu_s, 0, sizeof m_mu_s);
    memset(&m_shf_s, 0, sizeof m_shf_s);
    memset(&m_mdl_s, 0, sizeof m_mdl_s);
    memset(&m_str_s, 0, sizeof m_str_s);
    memset(&m_vow_s, 0, sizeof m_vow_s);
    memset(&m_dly_s, 0, sizeof m_dly_s);
    memset(m_dly_s.line, 0x80, sizeof m_dly_s.line);   /* (mu-law's silence) */
    memset(&m_tap_s, 0, sizeof m_tap_s);
    memset(m_tap_s.line, 0x80, sizeof m_tap_s.line);
    memset(&m_rvs_s, 0, sizeof m_rvs_s);
    memset(m_rvs_s.line, 0x80, sizeof m_rvs_s.line);
    memset(&m_spr_s, 0, sizeof m_spr_s);
    m_mdl_s.key = m_vow_s.key = m_spr_s.key = -1;
    memset(rm_env, 0, sizeof rm_env);
    for (m = 0; m < NMOD; m++) {
        rm_g[m] = Q15;
        rm_meter[m] = 0;
        rm_quiet[m] = 0xFFFFu;
    }
    rv_init();
    r_pub = r_use = 0;
    rack_draw_into(&rbuf[0], 0x4D4F4Cu);
}
