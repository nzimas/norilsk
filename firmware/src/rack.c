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

enum { RM_FLT, RM_FLD, RM_MU, RM_SHF, RM_MDL, RM_STR, RM_VOW, RM_DLY, RM_TAP, RM_RVS, RM_SPR, RM_REV, RM_GLT,
       RM_CLD, RM_SCR, RM_RNG, RM_STV, RM_CHB, RM_BNK, NKIND };
static const char *const RM_NAME[NKIND] = {"FILTER", "FOLD", "MULAW", "SHIFT", "MODAL", "STRING", "VOWEL", "DELAY",
                                           "TAPE", "REVERSE", "SPRING", "REVERB", "GLITCH",
                                           "CLOUD", "SCREAM", "RING", "STARVE", "CHEB", "BANK"};
/* the values an effect comes back to (a draw moves them, as Vascular draws a processor's) */
static const uint8_t RM_HOME[NKIND][4] = {
    {20, 55, 40, 30}, {45, 45, 20, 70}, {45, 60, 50, 30}, {40, 30, 70, 50}, {40, 55, 40, 50}, {35, 60, 40, 60},
    {50, 50, 50, 30}, {65, 40, 40, 60}, {50, 30, 45, 40}, {40, 60, 30, 50}, {50, 40, 50, 50}, {65, 60, 40, 55},
    {50, 40, 35, 45},
    {55, 45, 20, 50}, {50, 60, 45, 50}, {45, 50, 20, 60}, {60, 30, 35, 55}, {40, 50, 60, 60}, {40, 60, 30, 50}};
/* .. and the bounds a draw keeps them in, so that every effect is heard: no band-pass at 15 kHz, no spring tuned where
 * the sound has nothing */
static const uint8_t RM_LO[NKIND][4] = {
    {0, 35, 0, 0}, {10, 10, 0, 30}, {0, 30, 20, 0}, {0, 0, 0, 0}, {0, 15, 0, 20}, {0, 25, 0, 30},
    {0, 0, 10, 0}, {20, 0, 0, 40}, {10, 0, 0, 0}, {0, 30, 0, 0}, {0, 0, 35, 10}, {20, 20, 0, 40}, {15, 0, 0, 0},
    {10, 10, 0, 10}, {0, 25, 10, 10}, {0, 0, 0, 20}, {20, 0, 0, 20}, {0, 0, 20, 20}, {0, 25, 0, 10}};
static const uint8_t RM_HI[NKIND][4] = {
    {100, 85, 80, 100}, {90, 90, 80, 100}, {100, 100, 100, 80}, {100, 90, 100, 100}, {100, 85, 100, 100}, {100, 95, 100, 100},
    {100, 100, 90, 80}, {100, 85, 100, 100}, {100, 100, 90, 85}, {100, 100, 80, 100}, {100, 100, 92, 60}, {100, 95, 100, 100},
    {100, 90, 85, 100},
    {100, 100, 100, 100}, {100, 92, 90, 100}, {100, 100, 90, 100}, {100, 70, 100, 100}, {100, 100, 100, 100}, {100, 95, 100, 100}};
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
/* (build 12: more drastic, kept off the eardrums) the sound driven (DRIVE, p3: x 1 .. x 12, tanh) into a state-variable
 * filter -- TYPE (p0): low-pass, band-pass, high-pass, notch; FREQ (p1: 30 Hz up 9 octaves); RES (p2: Q 0.7 .. 12,
 * squelchy, short of ringing on its own) -- and out through a soft ceiling. Its level comes down as the resonance goes
 * up, and above 3 kHz the resonance is pulled back with the frequency: it may squelch and screech, never pierce. Fixed
 * where it is set: nothing sweeps by itself */
typedef struct { svf_t f; uint32_t rsv; } m_flt_t;
static void m_flt(const uint8_t *p, m_flt_t *s, int32_t *x)
{
    int32_t oct = 321600 + p[1] * (9 * 65536 / 100), hz, q, type, drive, comp;
    uint32_t i;
    svf_c c;
    hz = clampi((int32_t)(exp2_q16(clampi(oct, 4 << 16, 14 << 16)) >> 16), 30, 16000);
    type = p[0] < 25 ? 1 : p[0] < 50 ? 2 : p[0] < 75 ? 3 : 4;
    q = 2896 + p[2] * (12 * 4096 - 2896) / 100;             /* Q 0.7 .. 12, Q12 */
    if (hz > 3000)                                          /* (high up, less of it: no whistle at 8 kHz) */
        q = 2896 + (q - 2896) * 3000 / hz;              /* (32 bits: < 1.4e8) */
    drive = 4096 + p[3] * (11 * 4096) / 100;
    comp = (int32_t)(((uint32_t)Q15 * 4096u) / (uint32_t)(4096 + (q - 2896) / 3));   /* (x 1 .. ~0.3 with the Q) */
    svf_coef(&c, hz, q);
    for (i = 0; i < CTL; i++) {
        int32_t v = tanh_q15(mulq12(x[i], drive)) >> 1, y;
        if (type == 4) {                                    /* notch: the band taken out (low + high) */
            int32_t bp = svf_run(&s->f, &c, v, 2);
            y = v - mulr24(c.k, bp);
        } else
            y = svf_run(&s->f, &c, v, type) * (type == 2 ? 2 : 1);
        x[i] = mulq15(tanh_q15(mulq15(y, comp) * 2), LV(0.7));   /* (a soft ceiling under full scale) */
    }
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
/* (build 12: further) p0 the rate (FS down to FS / 32, fractional: to 1.4 kHz), p1 the bits (8 down to 2, sign
 * included), p2 the filter before and after (0.05 .. 1.0 of the lower rate: open, it aliases), p3 the drive into it
 * (x 1 .. x 8: mu-law lifts the quiet) */
typedef struct { uint32_t ph; int32_t hold, a1, a2, b1, b2; } m_mu_t;
static void m_mu(const uint8_t *p, m_mu_t *s, int32_t *x)
{
    uint32_t div16 = 16u + (uint32_t)(((uint32_t)p[0] * p[0]) * 496u / 10000u), inc = (uint32_t)(0xFFFFFFFFu / div16) << 4;   /* 1 .. 32, Q4
                                                          * (squared: the low rates at the top of the knob) */
    uint32_t shift = 6u - (uint32_t)(p[1] * 7 / 101), i;      /* code bits dropped: 0 .. 6 (2 bits left) */
    int32_t hz = (int32_t)((uint32_t)FS * 16u / div16) * (5 + p[2] * 95 / 100) / 100;
    int32_t k = clampi((hz * 4781) >> 10, 150, 32000);   /* a one-pole's step: 2 pi f / FS, Q15 */
    int32_t drive = 4096 + p[3] * (7 * 4096) / 100;
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
 * degrees apart from ~20 Hz to ~20 kHz. (build 12: more drastic) p0 the shift, 40 .. 900 Hz (log: every partial moved
 * off the harmonics, not a chorus); p1 the drive into it (x 1 .. x 6, tanh: denser sidebands); p2 the direction: down
 * below 45, up above 55, both (ring modulation) between; p3 the tone after, 1 .. 16 kHz. No feedback (a shifter fed
 * back spirals up or down: a barber pole). The shift is fixed while it sounds: nothing sweeps */
static const int32_t HB_A[4] = {128688190, 235208111, 262153419, 267764168};   /* a^2, Q28 */
static const int32_t HB_B[4] = {43421716, 196770956, 253765378, 265911936};
typedef struct { int32_t ax[4][2], ay[4][2], bx[4][2], by[4][2], bd; uint32_t ph; int32_t lp; } m_shf_t;
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
    int32_t oct = 348800 + p[0] * (294000 / 100), drive = 4096 + p[1] * (5 * 4096) / 100, hz8, dir;   /* log2 40 .. 900 Hz */
    int32_t tone = 2800 + p[3] * (31000 - 2800) / 100;
    uint32_t inc, i, k;
    hz8 = (int32_t)(exp2_q16(oct) >> 8);
    inc = hz8_inc(hz8);
    dir = p[2] < 45 ? -1 : p[2] > 55 ? 1 : 0;
    for (i = 0; i < CTL; i++) {
        int32_t in = tanh_q15(mulq12(x[i], drive)) >> 1, a = in, b = in, y, c, sn;
        for (k = 0; k < 4u; k++) {
            a = hb_sec(HB_A[k], a, s->ax[k], s->ay[k]);
            b = hb_sec(HB_B[k], b, s->bx[k], s->by[k]);
        }
        { int32_t t = s->bd; s->bd = b; b = t; }        /* (the second path a sample late) */
        s->ph += inc;
        c = sin_ph(s->ph + 0x40000000u);
        sn = sin_ph(s->ph);
        y = dir > 0 ? mulq15(a, c) - mulq15(b, sn) : dir < 0 ? mulq15(a, c) + mulq15(b, sn) : mulq15(a, c) * 2;
        s->lp += mulz15(y - s->lp, tone);
        x[i] = s->lp * 2;
    }
}

/* -------------------------------------------------------------- MODAL --- */
/* three resonators: the root in an octave (p0: 1 .. 4 octaves up from the root at C1 .. B1), its modes at f s and
 * f s^2 (p2: s 1 .. 4.2: further apart, inharmonic); Q 4 .. 325 (p1, log); p3 the upper modes' level. (build 12) the
 * sound driven into them, their ring brought up with the Q and held under a soft ceiling: a body that sings, not a
 * murmur. Tuned once a block (cheap) */
typedef struct { res_c c[3]; res_s r[3]; int32_t key, mk; } m_mdl_t;
static void m_mdl(const uint8_t *p, m_mdl_t *s, int32_t *x)
{
    int32_t key = p[0] | p[1] << 8 | p[2] << 16 | rack_root << 24, lvl = Q15 / 4 + p[3] * (3 * Q15 / 4 / 100), i, k;
    if (key != s->key) {
        int32_t note = 24 + (rack_root % 12) + 12 * (p[0] * 4 / 101), f = note_hz8(note, 0);
        int32_t sr = 65536 + p[2] * (int32_t)(209715 / 100);           /* s, Q16: 1 .. 4.2 */
        int32_t q8 = (int32_t)(exp2_q16(p[1] * (622 * 65536 / 10000)) >> 8) * 4;   /* 4 .. ~300 (2^6.2), Q8 */
        s->mk = 512 + q8 / 2;                                          /* (make-up, Q8: x 6 .. x 75 with the Q) */
        for (k = 0; k < 3; k++) {
            res_coef(&s->c[k], f, q8);
            f = (int32_t)(((int64_t)f * sr) >> 16);
        }
        s->key = key;
    }
    for (i = 0; i < CTL; i++) {
        int32_t in = tanh_q15(x[i] * 2) >> 1,
                y = res_run(&s->r[0], &s->c[0], in) + mulq15(res_run(&s->r[1], &s->c[1], in) + res_run(&s->r[2], &s->c[2], in), lvl);
        x[i] = mulq15(tanh_q15(clampi((int32_t)(((int64_t)y * s->mk) >> 8), -8 * Q15, 8 * Q15)), LV(0.6));
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
 * (build 12: 10 .. 60 -- a voice, not a tint), the drive into them by p3 (x 2 .. x 16, tanh); their ring brought up
 * with the Q, under a soft ceiling */
static const int16_t VF[5][3] = {{600, 1040, 2250}, {400, 1620, 2400}, {250, 1750, 2600}, {400, 750, 2400}, {350, 600, 2400}};
typedef struct { res_c c[3]; res_s r[3]; int32_t key, mk; } m_vow_t;
static void m_vow(const uint8_t *p, m_vow_t *s, int32_t *x)
{
    int32_t key = p[0] | p[1] << 8 | p[2] << 16, drive = 8192 + p[3] * (14 * 4096) / 100, i, k;
    if (key != s->key) {
        int32_t pos = p[0] * 4 * 256 / 100, seg = pos >> 8, fr = pos & 255;
        int32_t sc = (int32_t)exp2_q16((p[1] - 50) * (65536 / 100));   /* x 2^-0.5 .. 2^0.5, Q16 */
        int32_t q8 = (10 + p[2] * 50 / 100) << 8;
        s->mk = 256 + q8 / 4;                                          /* (make-up, Q8: x 3.5 .. x 16) */
        seg = seg > 3 ? 3 : seg;
        for (k = 0; k < 3; k++) {
            int32_t f = VF[seg][k] + (((VF[seg + 1][k] - VF[seg][k]) * fr) >> 8);
            res_coef(&s->c[k], (int32_t)(((int64_t)f * sc) >> 8), q8);
        }
        s->key = key;
    }
    for (i = 0; i < CTL; i++) {
        int32_t in = tanh_q15(mulq12(x[i], drive)) >> 1,
                y = res_run(&s->r[0], &s->c[0], in) + mulq15(res_run(&s->r[1], &s->c[1], in), 19661) +
                    mulq15(res_run(&s->r[2], &s->c[2], in), 13107);
        x[i] = mulq15(tanh_q15(clampi((int32_t)(((int64_t)y * s->mk) >> 8), -8 * Q15, 8 * Q15)), LV(0.6));
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
/* an echo on an 8-bit mu-law line at FS / 2: time 25 .. 900 ms (p0), (build 12) DROPOUTS (p1: the tape failing --
 * the echoes gone for 20 .. 200 ms, now and then, up to twice a second), feedback 0 .. 0.94 (p2) through the head's
 * saturation, AGE (p3: the loop's low-pass from 12 kHz down to 3 kHz, the head driven harder, hiss under it). The
 * time's own wander is a breath (at most 0.1 %), never a bend. The echoes alone come out. A change of time cross-fades */
#define TAP_N 19900u             /* 0.9 s at FS / 2 */
typedef struct { uint8_t line[TAP_N]; uint32_t w; int32_t t, t0, xf, lp, prev, wn, wt, dg, drop; rnd_t r; } m_tap_t;
static void m_tap(const uint8_t *p, m_tap_t *s, int32_t *x)
{
    int32_t want = (25 + p[0] * 875 / 100) * 2205 / 100 * 256;   /* samples at FS / 2, Q8 */
    int32_t fb = p[2] * (30802 / 100), age = 31000 - p[3] * (31000 - 12000) / 100, sat = 6144 + p[3] * (10 * 1024) / 100,
            hiss = p[3] * 6;                              /* (the head x 1.5 .. 4; hiss up to ~-40 dB) */
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
    if ((rnd_next(&s->r) & 255u) < 2u)                 /* the breath: a new place every ~0.2 s, glided to */
        s->wt = mulq15(rnd_bi(&s->r), 33);               /* (Q15 of the time: at most 0.1 %) */
    s->wn += (s->wt - s->wn) >> 7;
    if (!s->drop && p[1] && rnd_below(&s->r, 68900) < p[1])   /* a dropout: up to twice a second (1378 blocks / 100 %) */
        s->drop = (int32_t)ms_blocks((uint32_t)rnd_in(&s->r, 20, 200));
    if (s->drop)
        s->drop--;
    for (i = 0; i + 1u < CTL; i += 2u) {
        int32_t in = (x[i] + x[i + 1u]) >> 1, y, o0, tt = s->t + (int32_t)(((int64_t)s->t * s->wn) >> 15);
        y = mu_tap(s->line, TAP_N, s->w, clampi(tt, 4 << 8, (int32_t)(TAP_N - 4u) << 8));
        if (s->xf) {
            int32_t yo = mu_tap(s->line, TAP_N, s->w, s->t0);
            y = yo + (((y - yo) * (int32_t)(XF_PAIRS - (uint32_t)s->xf)) >> 9);
            s->xf--;
        }
        s->lp += mulz15(y - s->lp, age);
        s->line[s->w] = mu_enc8(mulq12(tanh_q15(mulq12(in + mulz15(s->lp, fb), sat)), 4096 * 4096 / sat));
        if (++s->w >= TAP_N)
            s->w = 0;
        s->dg += ((s->drop ? 0 : Q15) - s->dg) >> 6;     /* (a dropout's edges: ~3 ms, no click) */
        y = mulq15(s->lp, s->dg) + (hiss ? mulq15(rnd_bi(&s->r), hiss) : 0);
        o0 = (s->prev + y) >> 1;
        s->prev = y;
        x[i] = o0;
        x[i + 1u] = y;
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

/* ------------------------------------------------------------- GLITCH --- */
/* Russolo's bent buffers (Tonitor's russolo.c BEND, JUMP, after Noisferatu) as an effect: the sound written into a
 * line of 0.74 s (8-bit mu-law at FS / 2) and read back at its own speed -- never another pitch -- from SPAN behind
 * (p3: 20 .. 700 ms), its address now and then bent (p0: how often and how far -- a bit of it flipped, a higher one the
 * further), on a clock (p1: 0.5 .. 16 Hz) that at each tick bends, straightens, jumps the read elsewhere in the span,
 * or (p2: the chance) stutters: a few rounds of a short loop where it is (10 .. 120 ms). Each change cross-faded over
 * 2 ms, so it glitches without clicking. The read alone comes out */
#define GLT_N 16384u             /* 0.74 s at FS / 2 */
typedef struct {
    uint8_t line[GLT_N];
    uint32_t w, clk, ph;
    uint32_t mask, omask;        /* the address's bent bits now, before the change */
    int32_t back, oback;         /* how far behind the read is (samples at FS / 2), now and before */
    uint32_t loop, loop_at, loop_left;   /* a stutter: its length, its start, samples to go */
    int32_t xf, prev;
    rnd_t r;
} m_glt_t;
AINL int32_t glt_read(const m_glt_t *s, int32_t back, uint32_t mask)
{
    uint32_t i = (s->w + GLT_N - (uint32_t)back) & (GLT_N - 1u);
    return mu_dec8(s->line[(i ^ mask) & (GLT_N - 1u)]);
}
static void m_glt(const uint8_t *p, m_glt_t *s, int32_t *x)
{
    int32_t span = ms_samples2(20u + p[3] * 680u / 100u), o0;
    uint32_t i, tick = 97391u * 2u * (50u + p[1] * 1550u / 100u) / 100u;   /* 0.5 .. 16 Hz at FS / 2 (2^32 / 22050 a Hz:
                                                                             * in 32 bits, at most 3.1e8 before / 100) */
    if (!s->r.s)
        rnd_seed(&s->r, 0x474C54u ^ (uint32_t)(uintptr_t)s);
    if (!s->back)
        s->back = s->oback = span;
    for (i = 0; i + 1u < CTL; i += 2u) {
        int32_t in = (x[i] + x[i + 1u]) >> 1, y;
        s->line[s->w] = mu_enc8(clampi(in, -32767, 32767));
        s->w = (s->w + 1u) & (GLT_N - 1u);
        s->ph += tick;
        if (s->ph < tick) {                      /* the clock: a change */
            s->omask = s->mask;
            s->oback = s->back;
            s->xf = 44;                          /* (2 ms at FS / 2) */
            if (rnd_below(&s->r, 100) < p[2] && !s->loop_left) {   /* a stutter where it is */
                s->loop = ms_samples2((uint32_t)rnd_in(&s->r, 10, 120));
                s->loop_at = (s->w + GLT_N - (uint32_t)s->back) & (GLT_N - 1u);
                s->loop_left = s->loop * (uint32_t)rnd_in(&s->r, 2, 6);
            } else if (rnd_below(&s->r, 100) < p[0]) {   /* bent: a bit flipped, the higher the further */
                s->mask = 1u << rnd_in(&s->r, 4, 4 + p[0] * 9 / 100);
            } else if (rnd_chance(&s->r, 35)) {  /* a jump in the span */
                s->mask = 0;
                s->back = rnd_in(&s->r, 64, span);
            } else
                s->mask = 0;
        }
        if (s->loop_left) {                      /* stuttering: round the loop */
            uint32_t k = s->loop - s->loop_left % s->loop;
            y = mu_dec8(s->line[(s->loop_at + k) & (GLT_N - 1u)]);
            s->loop_left--;
        } else
            y = glt_read(s, s->back, s->mask);
        if (s->xf) {                             /* (the change cross-faded) */
            int32_t yo = glt_read(s, s->oback, s->omask);
            y = yo + (((y - yo) * (44 - s->xf) * 745) >> 15);   /* (/ 44 as x 745 / 2^15: no divide a sample) */
            s->xf--;
        }
        o0 = (s->prev + y) >> 1;
        s->prev = y;
        x[i] = o0;
        x[i + 1u] = y;
    }
}

/* ---------------------------------------------- build 11: drastic ones --- */
/* CLOUD -- a granular freezer (after Mutable Instruments' Clouds and Beads, MIT: the idea, not the code): the sound
 * written into 0.74 s (8-bit mu-law at FS / 2) and read back as up to six Hann grains of SIZE (p0: 30 .. 250 ms),
 * DENSITY a second (p1: 3 .. 60), each from a place drawn at random up to SPREAD back (p3: 0.05 .. 0.7 s), at the
 * sound's own speed (never another pitch); FREEZE (p2 above its middle) stops the writing: the cloud turns over what it
 * holds (and the slot's gate still closes it once no note sounds). The grains alone come out */
#define CLD_N 16384u
#define CLD_G 6u
typedef struct {
    uint8_t line[CLD_N];
    uint32_t w, filled, gp[CLD_G], gw[CLD_G], gwinc[CLD_G];
    int32_t next, prev;
    uint8_t gon[CLD_G];
    rnd_t r;
} m_cld_t;
static void m_cld(const uint8_t *p, m_cld_t *s, int32_t *x)
{
    uint32_t len = ms_samples2(30u + p[0] * 220u / 100u), rate = 3u + p[1] * 57u / 100u, iv = 22050u / rate;
    uint32_t spread = ms_samples2(50u + p[3] * 650u / 100u), i, k, k2;
    int frozen = p[2] >= 50;
    int32_t norm, o0;
    if (!s->r.s)
        rnd_seed(&s->r, 0x434C44u ^ (uint32_t)(uintptr_t)s);
    k2 = (iv << 15) / (3u * len);                        /* (as the voices' cloud: each grain at sqrt(8 / 3 O)) */
    norm = clampi((int32_t)isqrt32((k2 > 65535u ? 65535u : k2) << 16) << 1, 4096, 2 * Q15);
    for (i = 0; i + 1u < CTL; i += 2u) {
        int32_t in = (x[i] + x[i + 1u]) >> 1, y = 0;
        if (!frozen) {
            s->line[s->w] = mu_enc8(clampi(in, -32767, 32767));
            s->w = (s->w + 1u) & (CLD_N - 1u);
            if (s->filled < CLD_N)
                s->filled++;
        }
        if (--s->next <= 0 && s->filled > len + 64u) {   /* a grain: a free place, from somewhere behind */
            for (k = 0; k < CLD_G && s->gon[k]; k++)
                ;
            if (k < CLD_G) {
                uint32_t room = s->filled - len - 32u, back = len + 16u + (uint32_t)rnd_below(&s->r, (int32_t)(spread < room ? spread : room));
                s->gp[k] = (s->w + CLD_N - back) & (CLD_N - 1u);
                s->gw[k] = 0;
                s->gwinc[k] = 0xFFFFFFFFu / len;
                s->gon[k] = 1;
            }
            s->next = (int32_t)iv / 2 + rnd_below(&s->r, (int32_t)iv);
        }
        for (k = 0; k < CLD_G; k++)
            if (s->gon[k]) {
                uint32_t hi = s->gw[k] >> 24, was = s->gw[k];
                int32_t hf = (int32_t)((s->gw[k] >> 9) & 0x7FFFu), w = HANN[hi] + (((HANN[hi + 1u] - HANN[hi]) * hf) >> 15);
                y += mulq15(mu_dec8(s->line[s->gp[k]]), w);
                s->gp[k] = (s->gp[k] + 1u) & (CLD_N - 1u);
                s->gw[k] += s->gwinc[k];
                if (s->gw[k] < was)
                    s->gon[k] = 0;
            }
        y = mulq15(y, norm);
        o0 = (s->prev + y) >> 1;
        s->prev = y;
        x[i] = o0;
        x[i + 1u] = y;
    }
}

/* SCREAM -- a saturated feedback network (a no-input mixer's howl, a feedback delay network after Jot and Stautner):
 * four lines of prime lengths (SIZE, p0: 1 .. 23 ms, never gliding: a new size is a new length at once), mixed by a
 * Hadamard matrix, each through a one-pole (TONE, p2) and a tanh, fed back by FEEDBACK (p1: up to 1.05 -- the tanh holds
 * it) times the input's level followed (g x env / (env + k): silence lets it die). DRIVE (p3) into it. The lines out */
#define SCR_N 1024u
typedef struct { int16_t d[4][SCR_N]; uint32_t w; int32_t lp[4], env; } m_scr_t;
static void m_scr(const uint8_t *p, m_scr_t *s, int32_t *x)
{
    static const uint16_t BASE[4] = {1013, 761, 557, 409};   /* (primes: no two lines in step) */
    uint32_t i, k, len[4];
    int32_t gmax = LV(0.6) + p[1] * (LV(0.45) / 100), tone = 2000 + p[2] * (30000 - 2000) / 100,
            drive = 2048 + p[3] * (6 * 4096) / 100, g;
    for (k = 0; k < 4u; k++)
        len[k] = clampi((int32_t)(BASE[k] * (uint32_t)(4 + p[0] * 96 / 100) / 100u), 17, SCR_N - 1);
    g = (int32_t)((uint32_t)gmax * (uint32_t)s->env / (uint32_t)(s->env + 600));   /* (once a block, 32 bits: the loop's
                                                                                   * gain by the input's level) */
    for (i = 0; i < CTL; i++) {
        int32_t in = mulq12(x[i], drive), y[4], h[4], o;
        s->env += (absi(x[i]) - s->env) >> 7;
        for (k = 0; k < 4u; k++)
            y[k] = s->d[k][(s->w + SCR_N - len[k]) & (SCR_N - 1u)];
        h[0] = (y[0] + y[1] + y[2] + y[3]) >> 1;         /* (a 4 x 4 Hadamard, / 2: it keeps the energy) */
        h[1] = (y[0] - y[1] + y[2] - y[3]) >> 1;
        h[2] = (y[0] + y[1] - y[2] - y[3]) >> 1;
        h[3] = (y[0] - y[1] - y[2] + y[3]) >> 1;
        o = (y[0] + y[1] + y[2] + y[3]) >> 1;
        for (k = 0; k < 4u; k++) {
            s->lp[k] += mulz15(h[k] - s->lp[k], tone);
            s->d[k][s->w] = (int16_t)clampi(tanh_q15(((k & 1u) ? -in : in) + mulz15(s->lp[k], g)), -32767, 32767);
        }
        s->w = (s->w + 1u) & (SCR_N - 1u);
        x[i] = o;
    }
}

/* RING -- a diode ring modulator (after Julian Parker's model, DAFx-11, as Mutable Instruments' Warps has it): a carrier
 * fixed at FREQ (p0: 30 .. 1450 Hz), or (SELF, p2) the sound's own past (23 ms back) as its carrier; DIODE (p1) from the
 * ideal product (clean sidebands) to the diodes' half-wave square law (leakage, grit); TONE (p3) a low-pass after */
#define RNG_N 1024u
typedef struct { uint8_t line[RNG_N]; uint32_t w, ph; int32_t lp; uint8_t odd; } m_rng_t;
AINL int32_t rng_d(int32_t v) { return v > 0 ? (v * v) >> 15 : 0; }   /* a diode: square law above its knee, Q15 */
static void m_rng(const uint8_t *p, m_rng_t *s, int32_t *x)
{
    int32_t hz8 = (int32_t)((30u * 256u * exp2_q16(p[0] * (558 * 65536 / 10000))) >> 16), dio = p[1] * (Q15 / 100),
            self = p[2] * (Q15 / 100), tone = 3000 + p[3] * (31000 - 3000) / 100;
    uint32_t inc = hz8_inc(hz8), i;
    for (i = 0; i < CTL; i++) {
        int32_t xv = clampi(x[i], -32767, 32767), c = sin_ph(s->ph), a, b, ideal, dd, y;
        s->ph += inc;
        if (self) {                                      /* (its own past as the carrier, 23 ms back, lifted) */
            int32_t past = mu_dec8(s->line[(s->w + RNG_N - 507u) & (RNG_N - 1u)]);
            c += mulq15(clampi(past * 4, -32767, 32767) - c, self);
        }
        if (s->odd ^= 1u) {
            s->line[s->w] = mu_enc8(xv);
            s->w = (s->w + 1u) & (RNG_N - 1u);
        }
        ideal = mulq15(xv, c) * 2;
        a = (c + xv) >> 1;
        b = (c - xv) >> 1;
        dd = (rng_d(a) - rng_d(b) - rng_d(-b) + rng_d(-a)) * 4;
        y = ideal + mulq15(dd - ideal, dio);
        s->lp += mulz15(y - s->lp, tone);
        x[i] = s->lp;
    }
}

/* STARVE -- a starved, gated fuzz (after the Fuzz Factory's circuit): GAIN (p0: x 10 .. x 1000, 20 .. 60 dB) into an
 * asymmetric clipper (soft above, hard below), its BIAS (p2) pulled down as the sound gets louder (a starved supply);
 * a GATE (p1) on the sound's level with hysteresis, chopping it to nothing below -- the velcro sputter, the tearing;
 * TONE (p3) a low-pass after. Its level brought back to the sound's */
typedef struct { int32_t env, lp, open, dc; } m_stv_t;
static void m_stv(const uint8_t *p, m_stv_t *s, int32_t *x)
{
    int32_t g = (int32_t)((10u * exp2_q16(p[0] * (664 * 65536 / 10000))) >> 6), thr = p[1] * 33,   /* (gain Q10; to ~-20 dB) */
            bias = p[2] * (LV(0.5) / 100), tone = 1400 + p[3] * (30000 - 1400) / 100;
    uint32_t i;
    for (i = 0; i < CTL; i++) {
        int32_t xv = x[i], v, y;
        s->env += (absi(xv) - s->env) >> (absi(xv) > s->env ? 4 : 9);
        if (s->open ? s->env < thr : s->env > thr + (thr >> 1) + 8)
            s->open ^= 1;                                /* (hysteresis: it sputters at the threshold, not chatters) */
        v = (int32_t)clampi(((int64_t)xv * g) >> 10, -4 * Q15, 4 * Q15) + bias - mulq15(s->env, LV(0.6));
        y = v > 0 ? tanh_q15(v) : clampi(v, -LV(0.6), 0);
        y = s->open ? mulq15(y, LV(0.22)) : 0;          /* (its level near the sound's) */
        s->dc += (y - s->dc) >> 10;                      /* (the clipper's lean taken off) */
        s->lp += mulz15(y - s->dc - s->lp, tone);
        x[i] = s->lp;
    }
}

/* CHEB -- a harmonic re-voicer (after Airwindows' Pafnuty, MIT: the idea): the sound brought to a level (DRIVE, p2:
 * 0.3 .. 0.98 of full), through a sum of Chebyshev polynomials T1 .. T8 -- each makes its harmonic of a sine, and of a
 * texture a dense, fixed intermodulation -- weighted by SHAPE (p0: four sets, crossed) and ODD (p1: the even ones down,
 * or the odd), then brought back to the sound's level; TONE (p3) a low-pass after */
typedef struct { int32_t env, lp; } m_chb_t;
static const int16_t CHB_W[4][8] = {   /* Q15 weights of T1 .. T8 */
    {16384, 9830, 6554, 4915, 3277, 3277, 1638, 1638},   /* rich: everything, falling */
    {19661, 0, 11469, 0, 8192, 0, 4915, 0},              /* hollow: the odd ones */
    {6554, 0, 0, 9830, 0, 9830, 0, 9830},                /* glass: high ones only */
    {9830, 13107, 0, 9830, 0, 6554, 6554, 0}};           /* growl: the low even ones */
static void m_chb(const uint8_t *p, m_chb_t *s, int32_t *x)
{
    int32_t w[8], a = p[0] * 3, set = a / 100, fr = (a % 100) * (Q15 / 100), odd = p[1] * (Q15 / 100), tgt, gain, inv;
    int32_t tone = 3000 + p[3] * (31000 - 3000) / 100;
    uint32_t i, k;
    for (k = 0; k < 8u; k++) {
        int32_t v = set >= 3 ? CHB_W[3][k] : CHB_W[set][k] + mulq15(CHB_W[set + 1][k] - CHB_W[set][k], fr);
        w[k] = (k & 1u) ? mulq15(v, Q15 - odd) : mulq15(v, Q15 / 2 + odd / 2);   /* (k odd: T2, T4 ..: the even harmonics) */
    }
    tgt = LV(0.3) + p[2] * (LV(0.68) / 100);
    gain = (int32_t)(((uint32_t)tgt << 12) / (uint32_t)(s->env > 64 ? s->env : 64));   /* (once a block: to the level, Q12) */
    gain = clampi(gain, 4096 / 4, 4096 * 64);
    inv = (int32_t)(((uint32_t)clampi(s->env, 64, 65535) << 15) / (uint32_t)tgt);   /* (and back) */
    for (i = 0; i < CTL; i++) {
        int32_t xn = clampi(mulq12(x[i], gain), -32767, 32767), t0 = Q15, t1 = xn, y = mulq15(w[0], t1), t2;
        s->env += (absi(x[i]) - s->env) >> 8;
        for (k = 1; k < 8u; k++) {                       /* T(k+1) = 2 x T(k) - T(k-1) */
            t2 = clampi(2 * mulq15(xn, t1) - t0, -32767, 32767);
            y += mulq15(w[k], t2);
            t0 = t1;
            t1 = t2;
        }
        y = mulq15(clampi(y, -2 * Q15, 2 * Q15), inv) * 3;   /* (x 3: the sum of weights under one is brought back up) */
        s->lp += mulz15(y - s->lp, tone);
        x[i] = s->lp;
    }
}

/* BANK -- an inharmonic resonator bank (after Mutable Instruments' Rings and Elements, MIT: the idea): eight modes of a
 * bar, a plate, a bell or a membrane (SET, p2), the lowest in an octave of the scale's root (PITCH, p0), ringing at Q
 * (p1: 30 .. 400), the upper ones as bright as BRIGHT (p3) lets them. A texture into it comes out a struck, a bowed, a
 * ringing body. Tuned once a block when a value moves */
static const uint16_t BNK_R[4][8] = {   /* the modes' ratios to the lowest, x 100 */
    {100, 276, 540, 893, 1334, 1863, 2481, 3188},       /* a free bar */
    {100, 159, 214, 230, 292, 316, 365, 409},           /* a plate */
    {100, 250, 300, 433, 500, 580, 667, 800},           /* a bell */
    {100, 159, 214, 230, 265, 292, 316, 352}};          /* a membrane */
typedef struct { res_c c[8]; res_s r[8]; int32_t key, lv[8], mk; uint8_t nm; } m_bnk_t;
static void m_bnk(const uint8_t *p, m_bnk_t *s, int32_t *x)
{
    int32_t key = p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24 | (rack_root & 127) << 25;
    uint32_t i, k;
    if (key != s->key || !s->nm) {
        int32_t note = 36 + (rack_root % 12) + 12 * (p[0] * 3 / 101), hz8 = note_hz8(note, 0), set = p[2] * 3 / 100;
        int32_t q8 = (int32_t)((30u * 256u * exp2_q16(p[1] * (374 * 65536 / 10000))) >> 16);   /* 30 .. 400 */
        s->nm = 0;
        s->mk = 256 + q8 * 3 / 2;                        /* (make-up, Q8, with the Q: a narrower band passes less of a
                                                          * texture -- x 46 at Q 30 .. x 600 at Q 400, then a tanh) */
        for (k = 0; k < 8u; k++) {
            int32_t f = (int32_t)((uint32_t)hz8 * BNK_R[set][k] / 100u);   /* (32 bits: < 9e8) */
            if (f > (15000 << 8))
                break;
            res_coef(&s->c[k], f, q8);
            s->lv[k] = k ? Q15 * 6 / 10 - mulq15(Q15 * 6 / 10, Q15 - p[3] * (Q15 / 100)) * (int32_t)k / 8 : Q15 * 7 / 10;
            s->nm = (uint8_t)(k + 1u);
        }
        s->key = key;
    }
    for (i = 0; i < CTL; i++) {
        int32_t in = x[i] >> 1, y = 0;
        for (k = 0; k < s->nm; k++)
            y += mulq15(res_run(&s->r[k], &s->c[k], in), s->lv[k]);
        x[i] = mulq15(tanh_q15(clampi((int32_t)(((int64_t)y * s->mk) >> 8), -8 * Q15, 8 * Q15)), LV(0.6));   /* (a ringing
                                                                                                              * peak held) */
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
#define FX_ARENA 48128u          /* a slot's memory (47 KiB from build 10: the master delay and the voices' hands took the rest) */
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
    case RM_GLT: n = sizeof(m_glt_t); break;
    case RM_CLD: n = sizeof(m_cld_t); break;
    case RM_SCR: n = sizeof(m_scr_t); break;
    case RM_RNG: n = sizeof(m_rng_t); break;
    case RM_STV: n = sizeof(m_stv_t); break;
    case RM_CHB: n = sizeof(m_chb_t); break;
    case RM_BNK: n = sizeof(m_bnk_t); break;
    default: n = sizeof(rv_t); break;
    }
    return (n + 7u) & ~7u;
}

/* The recipes: a slot's chain, place by place, each place a choice among a few effects (a bit each); the third place
 * there one time in two when the recipe has one (FX_OPT). After Vascular's abstract chains and scenes */
#define K(x) (1u << RM_##x)
enum { FR_SPACE, FR_ECHO, FR_METAL, FR_STRINGS, FR_EROSION, FR_GHOST, FR_SPECTRAL, FR_RESONANT, FR_FOLDS, FR_TELEPHONE,
       FR_HELL, FR_FACTORY, FR_GLITCH, FR_BROADCAST, FR_ABYSS, FR_RUST,
       FR_FREEZER, FR_FEEDBACK, FR_FOUNDRY, FR_GONG, FR_SPECTRE, FR_SPUTTER, FR_N };
static const char *const FR_NAME[FR_N] = {"SPACE", "ECHO", "METAL", "STRINGS", "EROSION", "GHOST", "SPECTRAL", "RESONANT",
                                          "FOLDS", "TELEPHONE", "HELL", "FACTORY", "GLITCH", "BROADCAST", "ABYSS", "RUST",
                                          "FREEZER", "FEEDBACK", "FOUNDRY", "GONG", "SPECTRE", "SPUTTER"};
#define FX_OPT 0x80000000u       /* (a place that may be left out) */
static const uint32_t FR_PLACE[FR_N][FX_N] = {
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
    /* build 10: hellscapes, atmospheric industrial, abstract glitch */
    {K(FLD) | K(MU) | K(STV), K(SPR) | K(MDL) | K(SCR), K(REV) | K(CLD)},       /* HELL: torn, rung, a vast room */
    {K(MU) | K(STV), K(MDL) | K(STR) | K(BNK) | K(RNG), FX_OPT | K(DLY) | K(TAP)},   /* FACTORY: crushed, metal resonance */
    {K(GLT), K(RVS) | K(MU), FX_OPT | K(DLY)},                                  /* GLITCH: bent buffers */
    {K(GLT) | K(MU), K(VOW) | K(FLT), FX_OPT | K(TAP)},                         /* BROADCAST: a broken transmission */
    {K(SHF) | K(RNG), K(STR) | K(MDL) | K(BNK), K(REV) | K(CLD)},               /* ABYSS: moved partials ringing, deep */
    {K(SPR), K(FLD) | K(GLT), FX_OPT | K(REV)},                                 /* RUST: a spring, torn, glitched */
    /* build 11: the drastic ones */
    {K(CLD), K(BNK) | K(VOW) | K(SHF), FX_OPT | K(REV) | K(DLY)},               /* FREEZER: a cloud held, coloured */
    {K(SCR), K(FLT) | K(BNK) | K(STV), FX_OPT | K(GLT) | K(REV)},               /* FEEDBACK: a howl, shaped, torn */
    {K(RNG), K(STV) | K(FLD) | K(CHB), FX_OPT | K(SPR) | K(DLY)},               /* FOUNDRY: clangour through a fuzz */
    {K(BNK), K(RNG) | K(SHF) | K(CLD), FX_OPT | K(REV)},                        /* GONG: a struck body, ringing */
    {K(CHB), K(CLD) | K(RVS), FX_OPT | K(REV) | K(SCR)},                        /* SPECTRE: re-voiced, smeared */
    {K(STV), K(GLT) | K(MU) | K(RNG), FX_OPT | K(SPR) | K(TAP)},                /* SPUTTER: torn and broken */
};
static const uint8_t FR_W[FR_N] = {4, 4, 4, 3, 4, 3, 3, 3, 3, 2, 4, 4, 4, 3, 4, 3, 5, 4, 5, 4, 4, 4};
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
    case RM_GLT: m_glt(p, (m_glt_t *)st, x); break;
    case RM_CLD: m_cld(p, (m_cld_t *)st, x); break;
    case RM_SCR: m_scr(p, (m_scr_t *)st, x); break;
    case RM_RNG: m_rng(p, (m_rng_t *)st, x); break;
    case RM_STV: m_stv(p, (m_stv_t *)st, x); break;
    case RM_CHB: m_chb(p, (m_chb_t *)st, x); break;
    case RM_BNK: m_bnk(p, (m_bnk_t *)st, x); break;
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
static void fx_wire(uint32_t s, fx_patch_t *f, rnd_t *r);
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
        uint32_t can = FR_PLACE[f->recipe][k], m, nc = 0;   /* (a bit an effect, FX_OPT the top one) */
        uint8_t c[NKIND];
        if (!(can & ~FX_OPT) || ((can & FX_OPT) && !rnd_chance(r, 55)))
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
    fx_wire(s, f, r);                                /* the outs, the cables across */
    RING_PUBLISH();
    f_pub[s] = (uint8_t)b;
    return 1;
}

/* the outs and the cables across of slot s's patch p, drawn (fx_draw's rules): an earlier effect out to the master too,
 * now and then; a place's cable to the first or second effect of a later slot, one time in three */
static void fx_wire(uint32_t s, fx_patch_t *f, rnd_t *r)
{
    uint32_t k;
    f->out = 0;
    for (k = 0; k < FX_N; k++)
        f->xto[k] = 0;
    for (k = 0; k + 1u < f->n; k++)
        if (rnd_chance(r, 20))
            f->out |= (uint8_t)(1u << k);
    for (k = 0; k < f->n; k++)
        if (s + 1u < NFX && rnd_chance(r, 33)) {
            uint32_t ts = s + 1u + (uint32_t)rnd_below(r, (int32_t)(NFX - 1u - s));
            f->xto[k] = (uint16_t)(1u << (ts * FX_N + (uint32_t)rnd_below(r, 2)));
        }
}
/* the first black key on effect slot s: its outs and cables across drawn anew, its effects and their values as they
 * are -- changed in place (each a byte or two, taken by the next block: no fade, no clearing). 0 if a new patch is
 * still on its way in */
static int fx_rewire(uint32_t s, uint32_t seed)
{
    fx_patch_t *f, t;
    rnd_t r;
    uint32_t tries, k;
    if (f_pub[s] != f_use[s] || fx_phase[s] != FS_ON)
        return 0;
    f = &fbuf[s][f_use[s]];
    rnd_seed(&r, seed ^ 0x46585257u);
    t = *f;
    for (tries = 0; tries < 8u; tries++) {       /* (anew, where anything can change: the last slot only its outs) */
        fx_wire(s, &t, &r);
        if (t.out != f->out || memcmp(t.xto, f->xto, sizeof t.xto))
            break;
    }
    f->out = t.out;
    for (k = 0; k < FX_N; k++)
        f->xto[k] = t.xto[k];
    RING_PUBLISH();
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
