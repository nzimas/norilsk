/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR: the effects -- three effect slots, each a chain of two or three effects, shared by the three generator
 * slots as a pedalboard is by the synthesizers plugged into it.
 *
 * An effect slot's patch (HOME on it) is drawn from a RECIPE, a kind of chain (SPACE, ECHO, METAL, ...): which
 * effects stand in it, in what order, with what values, and its cables out to the effects of the slots after it
 * (across: slot 4 into 5 or 6, 5 into 6, so no loop can form). Within a slot the sound runs through the effects in
 * turn and out of the last to the master, and now and then out of one before it too. The same effect may stand in
 * several slots: each is an instance of its own, in its slot's memory (60 KiB a slot: a recipe is drawn so that its
 * effects fit). A generator's patch says which effects it is wired into (slots.c); an effect hears the sum of what
 * reaches it. The three slots sound at three places, left, middle, right (the plate is stereo of its own). Every
 * effect's output passes a guard (a level rider and a ceiling, as Vascular's every processor). Nothing bends a
 * pitch: no swept delays, no wobbling shifts, delay times that change cross-fade.
 *
 * A new patch on an effect slot: its sound fades out over 9 ms, its memory is cleared a piece a block (about 10 ms
 * more, silent), its new effects start. A muted slot is skipped: nothing reaches it, nothing comes out.
 *
 * The twelve effects, after the survey of Vascular's processors (docs/PROCESSORS.md), as the screen names them:
 *   FLT  FILTER    a resonant state-variable filter, low-, band- or high-pass, its cutoff drifting slowly if asked
 *   FLD  FOLD      a wavefolder: the wave driven into a window and reflected back, twice
 *   MU   MULAW     the telephone: a lower rate, mu-law with fewer bits, filtered before and after
 *   SHF  SHIFT     a frequency shifter (Hilbert pair and a quadrature sine): every partial moved by the same
 *                  hertz, up or down, or both (ring modulation) at the middle; feedback stacks the shifts
 *   MDL  MODAL     three resonators tuned to the scale's root: struck metal, bars, bells
 *   STR  STRING    three Karplus strings tuned to the root, detuned: everything that passes rings
 *   VOX  VOWEL     three formants between a, e, i, o, u
 *   DLY  DELAY     Tonitor's delay: 20 ms .. 1 s, feedback, damping (8-bit line at half the rate)
 *   TAP  TAPE      a tape echo: saturation, age, a faint wow (8-bit line at half the rate)
 *   RVS  REVERSE   windows of the sound played backwards, at their pitch, fed back if asked
 *   SPR  SPRING    a spring reverb: allpass chirps in a short tank, a resonance for its tone
 *   REV  REVERB    Tonitor's Dattorro plate, stereo */

enum { RM_FLT, RM_FLD, RM_MU, RM_SHF, RM_MDL, RM_STR, RM_VOW, RM_DLY, RM_TAP, RM_RVS, RM_SPR, RM_REV, NKIND };
static const char *const RM_NAME[NKIND] = {"FILTER", "FOLD", "MULAW", "SHIFT", "MODAL", "STRING", "VOWEL", "DELAY",
                                           "TAPE", "REVERSE", "SPRING", "REVERB"};
/* the values an effect comes back to (a draw moves them, as Vascular draws a processor's) */
static const uint8_t RM_HOME[NKIND][4] = {
    {20, 55, 40, 30}, {45, 45, 20, 70}, {45, 60, 50, 30}, {40, 30, 70, 50}, {40, 55, 40, 50}, {35, 60, 40, 60},
    {50, 50, 50, 30}, {65, 40, 40, 60}, {50, 30, 45, 40}, {40, 60, 30, 50}, {50, 40, 50, 50}, {65, 60, 40, 55}};
/* .. and the bounds a draw keeps them in, so that every effect is heard: no band-pass at 15 kHz, no spring tuned where
 * the sound has nothing */
static const uint8_t RM_LO[NKIND][4] = {
    {0, 35, 0, 0}, {10, 10, 0, 30}, {0, 30, 20, 0}, {0, 0, 0, 0}, {0, 15, 0, 20}, {0, 25, 0, 30},
    {0, 0, 10, 0}, {20, 0, 0, 40}, {10, 0, 0, 0}, {0, 30, 0, 0}, {0, 0, 35, 10}, {20, 20, 0, 40}};
static const uint8_t RM_HI[NKIND][4] = {
    {100, 85, 80, 100}, {90, 90, 80, 100}, {100, 100, 100, 80}, {100, 90, 100, 100}, {100, 85, 100, 100}, {100, 95, 100, 100},
    {100, 100, 90, 80}, {100, 85, 100, 100}, {100, 100, 90, 85}, {100, 100, 80, 100}, {100, 100, 92, 60}, {100, 95, 100, 100}};
static volatile int32_t rack_root = 48;        /* the scale's root, MIDI: what MODAL and STRING are tuned to */

/* AINL helpers the effects share; then the effects */
/* ------------------------------------------------------------- shared --- */
/* a constant-peak-gain resonator (Smith and Angell): zeros at DC and Nyquist, poles at f, radius r. Q28 coefficients */
typedef struct { int32_t a1, a2, g; } res_c;
typedef struct { int32_t y1, y2, x1, x2, e; } res_s;   /* (e: the fraction the last result dropped) */
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
    /* the fraction a result drops carried into the next one (error feedback): a truncated high-Q resonator otherwise
     * rings on by itself for ever, a small tone or a DC level, once its input has fallen silent */
    int64_t acc = (int64_t)c->a1 * s->y1 + (int64_t)c->a2 * s->y2 + (int64_t)c->g * (x - s->x2) + s->e;
    int32_t y = (int32_t)(acc >> 28);
    s->e = (int32_t)(acc - ((int64_t)y << 28));
    s->y2 = s->y1;
    s->y1 = y;
    s->x2 = s->x1;
    s->x1 = x;
    return y;
}
/* a Q15 product rounded towards zero: in a loop of gain under one it takes a little each trip, so the loop dies out
 * to nothing once its input is silent (truncating towards minus infinity leaves a level, or a tone, ringing on) */
AINL int32_t mulz15(int32_t a, int32_t b)
{
    int64_t m = (int64_t)a * b;
    return (int32_t)(m < 0 ? -((-m) >> 15) : m >> 15);
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
    type = p[0] < 34 ? 1 : p[0] < 67 ? 2 : 3;
    q = type == 2 ? 2896 + p[2] * (8192 - 2896) / 100   /* (the band-pass kept broad, Q 0.7 .. 2: else it is hardly heard) */
                  : 2896 + p[2] * (10 * 4096 - 2896) / 100;
    svf_coef(&c, clampi(hz, 30, 16000), q);
    for (i = 0; i < CTL; i++)
        x[i] = svf_run(&s->f, &c, x[i], type) * (type == 2 ? 2 : 1);
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
    int64_t m = (int64_t)c * (x + ys[1]);
    int32_t y = (int32_t)(m < 0 ? -((-m) >> 28) : m >> 28) - xs[1];   /* (towards zero: no ringing on in silence) */
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
        int32_t in = x[i] + mulz15(s->lp, fb), a = in, b = in, y, c, sn;
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
        s->lp += mulz15(s->line[r] - s->lp, tone);      /* (the feedback's damping) */
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
        s->in += mulz15(x[i] - s->in, 15700);           /* (~4 kHz; towards zero, so silence settles at 0, not -1) */
        for (k = 0; k < 3; k++) {
            int32_t d8 = clampi(len8[k], 4 << 8, (int32_t)(STR_N - 3u) << 8), v;
            uint32_t rp = (s->w << 8) + (STR_N << 8) - (uint32_t)d8, i0 = (rp >> 8) % STR_N, i1 = i0 + 1u >= STR_N ? 0u : i0 + 1u;
            v = s->line[k][i0] + (((s->line[k][i1] - s->line[k][i0]) * (int32_t)(rp & 255u)) >> 8);
            s->lp[k] += mulz15(v - s->lp[k], tone);
            s->line[k][s->w] = (int16_t)clampi(s->in / 4 + mulz15(s->lp[k], g), -32767, 32767);
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
/* Tonitor's delay: TIME 20 ms .. 1 s (p0, log), feedback 0 .. 0.92 (p1), damping (p2: the echoes' low-pass, none
 * .. 0.12), level (p3: 0.25 .. 1). The echoes alone come out. A change of time cross-fades (no pitch slide) */
#define DLY_N 22050u             /* 1 s at FS / 2 */
typedef struct { uint8_t line[DLY_N]; uint32_t w; int32_t t, t0, xf, lp, prev; } m_dly_t;
static void m_dly(const uint8_t *p, m_dly_t *s, int32_t *x)
{
    int32_t ms = (int32_t)((20u * exp2_q16(p[0] * (5644 * 65536 / 100000))) >> 16);   /* 20 x 50^(p / 100) */
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
/* windows of 60 .. 220 ms (p0) read backwards at their own pitch: two heads half a window apart, each a triangle, so
 * the sum stays level; a low-pass on it, 1.4 .. 13 kHz (p1); fed back into the line (p2: 0 .. 0.7: the reversed
 * reversed again, smeared); p3 the windows' spread, how far back they reach (one to three windows). Its line: 0.9 s
 * at FS / 2, 8-bit mu-law */
#define RVS_N 19845u
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
    uint32_t win = (60u + (uint32_t)p[0] * 160u / 100u) * 2205u / 100u;   /* samples at FS / 2 */
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
        res_coef(&s->rc, (400 + p[3] * 55) << 8, 180);   /* (Q 0.7: a broad colour) */
        s->key = key;
    }
    for (i = 0; i < CTL; i++) {
        uint32_t q = s->w + SPR_N - len;
        int32_t y, v = (x[i] >> 1) + res_run(&s->rs, &s->rc, x[i]);   /* (the sound itself and its colour, into the tank) */
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
        x[i] = 2 * y;                                  /* (x 2: its loop loses what the allpasses spread) */
    }
}

/* -------------------------------------------------------------- REVERB --- */
#include "reverb.c"

/* --------------------------------------------------------- the effect slots --- */
#define NFX 3                    /* effect slots */
#define FX_N 3                   /* effects in one: 2 or 3 */
#define NINST (NFX * FX_N)       /* effect instances, slot by slot (inst = slot x FX_N + place) */
#define FX_ARENA 51200u          /* a slot's memory (50 KiB from build 7: the master delay took the rest) */
#define FX_CLEAR 6144u           /* .. cleared this much a block when a new patch comes (10 blocks) */
#define FX_FADE 12u              /* blocks of a slot's fade out before a new patch (~9 ms) */
#define RM_TAIL 2756u            /* an effect nothing reaches goes on until its output has been near silence 2 s */

/* the memory each effect takes, rounded up to 8 bytes */
static uint32_t rm_size(uint32_t k)
{
    uint32_t n;
    switch (k) {
    case RM_FLT: n = sizeof(m_flt_t); break;
    case RM_FLD: n = sizeof(m_fld_t); break;
    case RM_MU: n = sizeof(m_mu_t); break;
    case RM_SHF: n = sizeof(m_shf_t); break;
    case RM_MDL: n = sizeof(m_mdl_t); break;
    case RM_STR: n = sizeof(m_str_t); break;
    case RM_VOW: n = sizeof(m_vow_t); break;
    case RM_DLY: n = sizeof(m_dly_t); break;
    case RM_TAP: n = sizeof(m_tap_t); break;
    case RM_RVS: n = sizeof(m_rvs_t); break;
    case RM_SPR: n = sizeof(m_spr_t); break;
    default: n = sizeof(rv_t); break;
    }
    return (n + 7u) & ~7u;
}

/* The recipes: a slot's chain, place by place, each place a choice among a few effects (a bit each); the third place
 * there one time in two when the recipe has one (FX_OPT). After Vascular's abstract chains and scenes */
#define K(x) (1u << RM_##x)
enum { FR_SPACE, FR_ECHO, FR_METAL, FR_STRINGS, FR_EROSION, FR_GHOST, FR_SPECTRAL, FR_RESONANT, FR_FOLDS, FR_TELEPHONE, FR_N };
static const char *const FR_NAME[FR_N] = {"SPACE", "ECHO", "METAL", "STRINGS", "EROSION", "GHOST", "SPECTRAL", "RESONANT",
                                          "FOLDS", "TELEPHONE"};
#define FX_OPT 0x8000u           /* (a place that may be left out) */
static const uint16_t FR_PLACE[FR_N][FX_N] = {
    {K(FLT) | K(VOW), K(DLY) | K(TAP), K(REV) | K(SPR)},                        /* SPACE: a colour, an echo, a room */
    {K(TAP) | K(DLY), K(SPR) | K(REV), FX_OPT | K(FLT)},                        /* ECHO */
    {K(MDL) | K(SHF), K(FLD) | K(FLT), FX_OPT | K(REV) | K(SPR)},               /* METAL: struck, bent, a room */
    {K(STR), K(TAP) | K(DLY) | K(FLT), FX_OPT | K(REV)},                        /* STRINGS: everything rings */
    {K(MU) | K(FLD), K(RVS) | K(TAP), FX_OPT | K(DLY) | K(SPR)},                /* EROSION */
    {K(FLT) | K(VOW), K(RVS), K(REV) | K(DLY) | K(SPR)},                        /* GHOST: the sound reversed, far off */
    {K(SHF), K(VOW) | K(FLT), FX_OPT | K(DLY) | K(SPR)},                        /* SPECTRAL: the partials moved */
    {K(VOW) | K(MDL), K(STR) | K(SPR), FX_OPT | K(DLY)},                        /* RESONANT */
    {K(FLD), K(FLT) | K(VOW), FX_OPT | K(SPR) | K(TAP)},                        /* FOLDS */
    {K(MU), K(SHF) | K(FLT), FX_OPT | K(REV) | K(DLY)},                         /* TELEPHONE */
};
static const uint8_t FR_W[FR_N] = {5, 5, 4, 3, 4, 3, 4, 3, 3, 2};
#undef K

typedef struct { uint8_t kind, p[4]; } fx_eff_t;
typedef struct {
    fx_eff_t e[FX_N];
    uint8_t n;                   /* 2 or 3 */
    uint8_t recipe;
    uint8_t out;                 /* bit k: place k's output to the master (the last always) */
    uint16_t xto[FX_N];          /* bit i: place k's output wired into instance i (of a later slot) */
    uint32_t seed, serial;
} fx_patch_t;
static fx_patch_t fbuf[NFX][2];
static volatile uint8_t f_pub[NFX], f_use[NFX];
static volatile uint8_t fx_mute[NFX];
static volatile int8_t fx_lvl[NFX], fx_pan[NFX];   /* each slot's output: level (0 .. LEVEL_MAX) and pan */
static uint8_t fx_arena[NFX][FX_ARENA] POOL __attribute__((aligned(8)));
static void *fx_st[NINST];                     /* each instance's state, in its slot's memory */
enum { FS_ON, FS_FADE, FS_CLEAR };
static uint8_t fx_phase[NFX];
static uint32_t fx_count[NFX];                 /* (the fade's blocks left; the bytes cleared so far) */
static int32_t fx_g[NFX], fx_gl[NFX], fx_gr[NFX];   /* each slot's output gain and pan gains as the last block ended */
/* the slots' gate (build 6, the owner's rule: nothing sounds once no note is held or latched): 2 s after the last
 * voice has ended, every effect slot fades out in 0.5 s, whatever its tail, and its effects are left alone; a voice
 * sounding again opens them at once. Each slot's output goes through a DC blocker (~8 Hz) as well */
#define GATE_WAIT 2756u          /* blocks with no voice (2 s) */
#define GATE_STEP (Q15 / 689)    /* closing: 0.5 s */
static volatile uint8_t rack_voices;   /* a voice sounded this block (gens.c) */
static int32_t fx_gate[NFX];     /* Q15 */
static uint32_t fx_idle[NFX];    /* blocks since a voice last sounded */
static int32_t fx_dcx[NFX][2], fx_dcy[NFX][2];   /* the DC blocker: last input, last output (Q8 above the sample) */
static int32_t rm_env[NINST], rm_g[NINST];     /* each instance's guard */
static volatile int32_t rm_meter[NINST];       /* each instance's output, its last peaks (the screen's) */
static uint16_t rm_quiet[NINST];
static uint8_t rm_plate_on[NINST];
/* the job's buffers: what the generators sent into each instance the block before (CPU0 fills ebus_next, the rack
 * runs on ebus_run: one block later, so the rack can run on CPU1 while CPU0 renders the voices); the rack's output */
static int32_t ebus_next[NINST][CTL] POOL __attribute__((aligned(32)));
static int32_t ebus_run[NINST][CTL] POOL __attribute__((aligned(32)));
static int32_t fx_l[CTL], fx_r[CTL];
static int32_t fx_mdl[CTL], fx_mrv[CTL];   /* the slots' sends to the master delay and reverb (master.c), this block */
static volatile int32_t fx_rsend[NFX], fx_dsend[NFX];   /* each slot's sends, Q15: the mean of its generators' (gens.c) */
/* the modulation's offsets (mod.c, worked out before the work is dealt): each slot's effects' values by place (4 k + j),
 * its level (steps) and pan */
static int16_t fx_mod_p[NFX][12], fx_mod_lvl[NFX], fx_mod_pan[NFX];
static uint8_t fx_mod_on[NFX];
static uint8_t rm_plate_p[NINST][3];             /* (the plate's values as last set: set again when moved) */
static uint32_t ebus_fed, ebus_fed_run;        /* (the instances something was sent into) */
static uint32_t rack_ticks;                    /* what the last block of the rack took */

/* the guard: a peak rider (Tonitor's limiter: instant attack, ~200 ms release) to 0.5 */
#define RM_CEIL 16384
static void rm_guard(uint32_t i0, int32_t *x)
{
    int32_t pk = 0, want, g0 = rm_g[i0], dg;
    uint32_t i;
    for (i = 0; i < CTL; i++) {
        int32_t a = absi(x[i]);
        if (a > pk)
            pk = a;
    }
    if (pk > rm_env[i0])
        rm_env[i0] = pk;
    else
        rm_env[i0] -= rm_env[i0] >> 8;
    want = rm_env[i0] > RM_CEIL ? (int32_t)((uint32_t)RM_CEIL * 32768u / (uint32_t)rm_env[i0]) : Q15;
    if (want > g0 && g0 < Q15)
        want = g0 + ((want - g0) >> 3);
    dg = (want - g0) / (int32_t)CTL;
    for (i = 0; i < CTL; i++) {
        g0 += dg;
        x[i] = mulq15(x[i], g0);
    }
    rm_g[i0] = want;
}

/* one instance over x (mono, in place); a plate's stereo into l, r (added) when it goes out */
static void rm_block(uint32_t inst, const fx_eff_t *e, int32_t *x, int32_t *l, int32_t *r, int to_master)
{
    void *st = fx_st[inst];
    const uint8_t *p = e->p;
    int32_t pk = 0;
    uint32_t i;
    switch (e->kind) {
    case RM_FLT: m_flt(p, (m_flt_t *)st, x); break;
    case RM_FLD: m_fld(p, (m_fld_t *)st, x); break;
    case RM_MU: m_mu(p, (m_mu_t *)st, x); break;
    case RM_SHF: m_shf(p, (m_shf_t *)st, x); break;
    case RM_MDL: m_mdl(p, (m_mdl_t *)st, x); break;
    case RM_STR: m_str(p, (m_str_t *)st, x); break;
    case RM_VOW: m_vow(p, (m_vow_t *)st, x); break;
    case RM_DLY: m_dly(p, (m_dly_t *)st, x); break;
    case RM_TAP: m_tap(p, (m_tap_t *)st, x); break;
    case RM_RVS: m_rvs(p, (m_rvs_t *)st, x); break;
    case RM_SPR: m_spr(p, (m_spr_t *)st, x); break;
    default: {                                       /* the plate: stereo */
        rv_t *R = (rv_t *)st;
        int32_t pl[CTL], pr[CTL], g = 8192 + p[3] * (24576 / 100);   /* (its level: 0.25 .. 1) */
        if (!rm_plate_on[inst] || rm_plate_p[inst][0] != p[0] || rm_plate_p[inst][1] != p[1] || rm_plate_p[inst][2] != p[2]) {
            rv_param(R, p[0], p[1], p[2], 40);       /* (x 0.64: the tail sums over seconds; again when moved) */
            rm_plate_on[inst] = 1;
            rm_plate_p[inst][0] = p[0];
            rm_plate_p[inst][1] = p[1];
            rm_plate_p[inst][2] = p[2];
        }
        for (i = 0; i < CTL; i++)
            pl[i] = pr[i] = 0;
        rv_block_fp(R, x, pl, pr, CTL);
        for (i = 0; i < CTL; i++) {
            pl[i] = mulq15(pl[i], g);
            pr[i] = mulq15(pr[i], g);
            if (to_master) {
                l[i] += pl[i];
                r[i] += pr[i];
            }
            x[i] = (pl[i] + pr[i]) >> 1;               /* (mono, for what follows it) */
        }
        break;
    }
    }
    if (e->kind != RM_REV)
        rm_guard(inst, x);
    for (i = 0; i < CTL; i++)
        pk = maxi(pk, absi(x[i]));
    rm_meter[inst] = pk > rm_meter[inst] ? pk : rm_meter[inst] - (rm_meter[inst] >> 4);
}

/* a slot's patch taken: its instances placed in its memory and started (the memory was cleared) */
static void fx_start(uint32_t s)
{
    const fx_patch_t *f = &fbuf[s][f_use[s]];
    uint32_t k, at = 0;
    for (k = 0; k < FX_N; k++) {
        uint32_t inst = s * FX_N + k;
        rm_env[inst] = 0;
        rm_g[inst] = Q15;
        rm_meter[inst] = 0;
        rm_quiet[inst] = 0xFFFFu;
        rm_plate_on[inst] = 0;
        fx_st[inst] = 0;
        if (k >= f->n)
            continue;
        fx_st[inst] = &fx_arena[s][at];
        at += rm_size(f->e[k].kind);
        if (f->e[k].kind == RM_MDL)
            ((m_mdl_t *)fx_st[inst])->key = -1;
        else if (f->e[k].kind == RM_VOW)
            ((m_vow_t *)fx_st[inst])->key = -1;
        else if (f->e[k].kind == RM_SPR)
            ((m_spr_t *)fx_st[inst])->key = -1;
        else if (f->e[k].kind == RM_REV)
            rv_init((rv_t *)fx_st[inst]);
    }
}
/* each slot's patch published: faded out, its memory cleared a piece a block, started (the rack's job, either core) */
static void fx_take(void)
{
    uint32_t s;
    for (s = 0; s < NFX; s++) {
        if (fx_phase[s] == FS_ON && f_pub[s] != f_use[s]) {
            fx_phase[s] = FS_FADE;
            fx_count[s] = FX_FADE;
        }
        if (fx_phase[s] == FS_FADE && (!fx_count[s] || fx_mute[s])) {   /* (muted: nothing to fade) */
            f_use[s] = f_pub[s];
            fx_phase[s] = FS_CLEAR;
            fx_count[s] = 0;
        }
        if (fx_phase[s] == FS_CLEAR) {
            uint32_t n = FX_ARENA - fx_count[s] < FX_CLEAR ? FX_ARENA - fx_count[s] : FX_CLEAR;
            memset(&fx_arena[s][fx_count[s]], 0, n);
            fx_count[s] += n;
            if (fx_count[s] >= FX_ARENA) {
                fx_start(s);
                fx_phase[s] = FS_ON;
            }
        }
    }
}

/* the rack's job: ebus_run holds what reached each instance from the generators; the slots in turn, each effect in
 * its chain, run if something reaches it or its tail still sounds; out down its slot's chain, across its cables, and
 * to the master as wired (fx_l, fx_r) */
static void rack_job(void)
{
    uint32_t s, k, i, j, has = ebus_fed_run;
    uint32_t t0 = fm1_ticks();
    fx_take();
    for (i = 0; i < CTL; i++)
        fx_l[i] = fx_r[i] = fx_mdl[i] = fx_mrv[i] = 0;
    for (s = 0; s < NFX; s++) {
        const fx_patch_t *f = &fbuf[s][f_use[s]];
        int32_t sl[CTL], sr[CTL], g, gl, gr, dg, dgl, dgr, gw;
        uint32_t ph;
        int fading = fx_phase[s] == FS_FADE;
        if (fx_phase[s] == FS_CLEAR || fx_mute[s]) {
            fx_g[s] = 0;
            continue;
        }
        for (i = 0; i < CTL; i++)
            sl[i] = sr[i] = 0;
        if (rack_voices) {                               /* the gate */
            fx_idle[s] = 0;
            fx_gate[s] = Q15;
        } else if (fx_idle[s] < 0xFFFFFFu)
            fx_idle[s]++;
        if (fx_idle[s] > GATE_WAIT && !fx_gate[s] && !fading)
            continue;                                    /* closed: its effects left alone, nothing out (a fade to a new
                                                          * patch still runs its course) */
        for (k = 0; k < f->n; k++) {
            uint32_t inst = s * FX_N + k, out = k + 1u == f->n || ((f->out >> k) & 1u);
            int32_t *x = ebus_run[inst];
            if (!((has >> inst) & 1u) && rm_quiet[inst] >= RM_TAIL)
                continue;
            if (fx_mod_on[s]) {                      /* (moved by the slot's modulation: within the effect's bounds) */
                fx_eff_t ee = f->e[k];
                uint32_t j;
                for (j = 0; j < 4u; j++)
                    ee.p[j] = (uint8_t)clampi((int32_t)ee.p[j] + fx_mod_p[s][k * 4u + j], RM_LO[ee.kind % NKIND][j],
                                              RM_HI[ee.kind % NKIND][j]);
                rm_block(inst, &ee, x, sl, sr, (int)out);
            } else
                rm_block(inst, &f->e[k], x, sl, sr, (int)out);
            if (rm_meter[inst] < 8) {
                if (rm_quiet[inst] < 0xFFFFu)
                    rm_quiet[inst]++;
            } else
                rm_quiet[inst] = 0;
            if (rm_quiet[inst] >= RM_TAIL)
                continue;
            if (out && f->e[k].kind != RM_REV)
                for (i = 0; i < CTL; i++) {
                    sl[i] += x[i];
                    sr[i] += x[i];
                }
            if (k + 1u < f->n) {                     /* down the chain */
                for (i = 0; i < CTL; i++)
                    ebus_run[inst + 1u][i] += x[i];
                has |= 1u << (inst + 1u);
            }
            for (j = (s + 1u) * FX_N; j < NINST; j++)   /* across, to a later slot */
                if ((f->xto[k] >> j) & 1u) {
                    for (i = 0; i < CTL; i++)
                        ebus_run[j][i] += x[i];
                    has |= 1u << j;
                }
        }
        {   /* the DC blocker, then the gate */
            int32_t *ch[2] = {sl, sr}, c;
            for (c = 0; c < 2; c++)
                for (i = 0; i < CTL; i++) {
                    int32_t xq = ch[c][i] << 8;          /* y = x - x1 + R y1, R = 1 - 2 pi 8 / FS (towards zero) */
                    int32_t y = xq - fx_dcx[s][c] + mulz15(fx_dcy[s][c], 32730);
                    fx_dcx[s][c] = xq;
                    fx_dcy[s][c] = y;
                    ch[c][i] = y >> 8;
                }
            if (fx_idle[s] > GATE_WAIT && fx_gate[s] > 0) {
                int32_t g0 = fx_gate[s], g1 = maxi(g0 - GATE_STEP, 0), dgt = (g1 - g0) / (int32_t)CTL;
                for (i = 0; i < CTL; i++) {
                    g0 += dgt;
                    sl[i] = mulq15(sl[i], g0);
                    sr[i] = mulq15(sr[i], g0);
                }
                fx_gate[s] = g1;
            } else if (fx_gate[s] < Q15) {
                for (i = 0; i < CTL; i++) {
                    sl[i] = mulq15(sl[i], fx_gate[s]);
                    sr[i] = mulq15(sr[i], fx_gate[s]);
                }
            }
        }
        /* the slot out: its level, its place (the mono parts panned; the plate's stereo as it is), its fade */
        {
            int32_t lv = fx_lvl[s] ? clampi(fx_lvl[s] + fx_mod_lvl[s], 0, 23) : 0;   /* (its modulation added) */
            gw = lv ? (int32_t)(exp2_q16((lv - 20) * 16328) >> 2) : 0;   /* Q14, 1.5 dB a step */
        }
        if (fading) {
            gw = gw * (int32_t)(fx_count[s] - 1u) / (int32_t)FX_FADE;
            fx_count[s]--;
        }
        ph = (uint32_t)(clampi(fx_pan[s] + fx_mod_pan[s], -20, 20) * (Q15 / 20) + Q15) * 16384u;
        g = gw;
        gl = sin_ph(ph + 0x40000000u);
        gr = sin_ph(ph);
        dg = (g - fx_g[s]) / (int32_t)CTL;
        dgl = (gl - fx_gl[s]) / (int32_t)CTL;
        dgr = (gr - fx_gr[s]) / (int32_t)CTL;
        for (i = 0; i < CTL; i++) {
            int32_t a, b;
            fx_g[s] += dg;
            fx_gl[s] += dgl;
            fx_gr[s] += dgr;
            a = (int32_t)(((int64_t)sl[i] * fx_g[s]) >> 14);
            b = (int32_t)(((int64_t)sr[i] * fx_g[s]) >> 14);
            /* (the mono parts went into both sides alike: panned here, the plate's two sides with them) */
            fx_l[i] += mulq15(a, fx_gl[s]);
            fx_r[i] += mulq15(b, fx_gr[s]);
            fx_mdl[i] += mulq15((a + b) >> 1, fx_dsend[s]);   /* (on to the master delay and reverb) */
            fx_mrv[i] += mulq15((a + b) >> 1, fx_rsend[s]);
        }
        fx_g[s] = g;
        fx_gl[s] = gl;
        fx_gr[s] = gr;
    }
    rack_ticks = fm1_ticks() - t0;
}
static __typeof__(&rack_job) volatile rack_job_fp = rack_job;

/* ------------------------------------------------------- the draw (main loop) --- */
/* an effect's values: each at its home one time in five, else moved by 15 .. 55 either way (Vascular's) */
static void fx_values(fx_eff_t *e, rnd_t *r)
{
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        int32_t v = RM_HOME[e->kind][k];
        if (!rnd_chance(r, 20)) {
            int32_t d = rnd_in(r, 15, 55);
            v += rnd_chance(r, 50) ? d : -d;
        }
        e->p[k] = (uint8_t)clampi(v, RM_LO[e->kind][k], RM_HI[e->kind][k]);
    }
}
/* HOME on effect slot s: a recipe (recipe >= 0: that one -- SHIFT + HOME), its effects place by place among the
 * recipe's choices, as fit the slot's memory, their values; the outs; the cables across to the slots after it (one
 * time in three a place has one, to the first or second effect of a later slot). Published to the ISR; 0 if the
 * ISR had not taken the last one yet */
static uint32_t fx_serial;
static int fx_draw(uint32_t s, int32_t recipe, uint32_t seed)
{
    uint32_t b = f_use[s] ^ 1u, k, room = FX_ARENA, t0 = fm1_ms;
    fx_patch_t *f = &fbuf[s][b];
    rnd_t rr, *r = &rr;
    while (f_pub[s] != f_use[s] || fx_phase[s] != FS_ON) {   /* (the last one not taken yet) */
        if ((uint32_t)(fm1_ms - t0) > 60u)
            return 0;
        fm1_wdt_feed();
    }
    memset(f, 0, sizeof *f);
    rnd_seed(r, seed ^ 0x46584658u);
    f->seed = seed;
    f->serial = ++fx_serial;
    f->recipe = (uint8_t)(recipe >= 0 && recipe < FR_N ? recipe : (int32_t)rnd_weighted(r, FR_W, FR_N));
    for (k = 0; k < FX_N; k++) {
        uint32_t can = FR_PLACE[f->recipe][k], m, nc = 0;
        uint8_t c[NKIND];
        if (!(can & 0x0FFFu) || ((can & FX_OPT) && !rnd_chance(r, 55)))
            break;
        for (m = 0; m < NKIND; m++)
            if (((can >> m) & 1u) && rm_size(m) <= room)
                c[nc++] = (uint8_t)m;
        if (!nc)                                     /* (none fits: the lightest of all) */
            c[nc++] = RM_FLT;
        f->e[k].kind = c[rnd_below(r, (int32_t)nc)];
        room -= rm_size(f->e[k].kind);
        fx_values(&f->e[k], r);
        f->n++;
    }
    if (f->n < 2u) {                                 /* (never one alone) */
        f->e[1].kind = RM_FLT;
        fx_values(&f->e[1], r);
        f->n = 2;
    }
    for (k = 0; k + 1u < f->n; k++)                  /* an earlier effect out to the master too, now and then */
        if (rnd_chance(r, 20))
            f->out |= (uint8_t)(1u << k);
    for (k = 0; k < f->n; k++)                       /* the cables across */
        if (s + 1u < NFX && rnd_chance(r, 33)) {
            uint32_t ts = s + 1u + (uint32_t)rnd_below(r, (int32_t)(NFX - 1u - s));
            f->xto[k] = (uint16_t)(1u << (ts * FX_N + (uint32_t)rnd_below(r, 2)));
        }
    RING_PUBLISH();
    f_pub[s] = (uint8_t)b;
    return 1;
}

/* a slot's patch as a session or a snapshot kept it, published as a draw is (each value checked; a patch too large for
 * the slot's memory as it is now loses its last effects, then FILTER stands in) */
static int fx_set(uint32_t s, const fx_patch_t *src)
{
    uint32_t b = f_use[s] ^ 1u, k, j, room = FX_ARENA, t0 = fm1_ms;
    fx_patch_t *f = &fbuf[s][b];
    const fx_patch_t *now = &fbuf[s][f_pub[s]];
    if (now->seed == src->seed && now->n == src->n && now->recipe == src->recipe && now->out == src->out &&
        !memcmp(now->e, src->e, sizeof now->e) && !memcmp(now->xto, src->xto, sizeof now->xto))
        return 1;                                    /* (the same patch: left playing, no fade) */
    while (f_pub[s] != f_use[s] || fx_phase[s] != FS_ON) {
        if ((uint32_t)(fm1_ms - t0) > 60u)
            return 0;
        fm1_wdt_feed();
    }
    *f = *src;
    f->n = (uint8_t)clampi(f->n, 2, FX_N);
    f->recipe = (uint8_t)(f->recipe < FR_N ? f->recipe : 0);
    for (k = 0; k < FX_N; k++) {
        if (k >= f->n) {
            memset(&f->e[k], 0, sizeof f->e[k]);
            f->xto[k] = 0;
            continue;
        }
        if (f->e[k].kind >= NKIND || rm_size(f->e[k].kind) > room)
            f->e[k].kind = RM_FLT;
        room -= rm_size(f->e[k].kind);
        for (j = 0; j < 4u; j++)
            f->e[k].p[j] = (uint8_t)clampi(f->e[k].p[j], RM_LO[f->e[k].kind][j], RM_HI[f->e[k].kind][j]);
        f->xto[k] &= (uint16_t)(((1u << NINST) - 1u) & ~((1u << ((s + 1u) * FX_N)) - 1u));   /* (later slots only) */
    }
    f->serial = ++fx_serial;
    RING_PUBLISH();
    f_pub[s] = (uint8_t)b;
    return 1;
}

static void rack_init(uint32_t seed)
{
    uint32_t s;
    memset(fx_arena, 0, sizeof fx_arena);
    memset(fbuf, 0, sizeof fbuf);
    memset(ebus_next, 0, sizeof ebus_next);
    memset(ebus_run, 0, sizeof ebus_run);
    for (s = 0; s < NFX; s++) {
        f_pub[s] = f_use[s] = 0;
        fx_phase[s] = FS_ON;
        fx_mute[s] = 0;
        fx_lvl[s] = 20 - 2;
        fx_pan[s] = (int8_t)((int32_t)s * 10 - 10);   /* left, middle, right */
        fx_g[s] = fx_gl[s] = fx_gr[s] = 0;
        fx_gate[s] = Q15;
        fx_idle[s] = 0;
        fx_dcx[s][0] = fx_dcx[s][1] = fx_dcy[s][0] = fx_dcy[s][1] = 0;
        fx_draw(s, -1, seed + s * 7919u);            /* (published to buffer 1) */
        f_use[s] = f_pub[s];                         /* .. and taken at once: the memory is clear */
        fx_start(s);
    }
}
