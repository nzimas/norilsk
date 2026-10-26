/* SPDX-License-Identifier: GPL-3.0-only
 * NORILSK: the effects -- three effect slots, each a chain of two or three effects, shared by the three generator
 * slots as a pedalboard is by the synthesizers plugged into it.
 *
 * An effect slot's patch (HOME on it) is drawn from a RECIPE, a kind of chain (SPACE, ECHO, METAL, ...): which
 * effects stand in it, in what order, with what values, and its cables out to the effects of the slots after it
 * (across: slot 4 into 5 or 6, 5 into 6, so no loop can form). Within a slot the sound runs through the effects in
 * turn and out of the last to the master, and now and then out of one before it too. The same effect may stand in
 * several slots: each is an instance of its own, in its slot's memory (36 KiB a slot from build 31: a recipe is drawn so that its
 * effects fit). A generator's patch says which effects it is wired into (slots.c); an effect hears the sum of what
 * reaches it. The three slots sound at three places, left, middle, right (mono each from build 32). Every
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
 *   DLY  DELAY     (the master's from build 30; a saved one loads as TAPE, build 31)
 *   TAP  TAPE      a tape echo: saturation, age, a faint wow (8-bit line at half the rate)
 *   RVS  REVERSE   windows of the sound played backwards, at their pitch, fed back if asked
 *   SPR  SPRING    a spring reverb: allpass chirps in a short tank, a resonance for its tone
 *   REV  REVERB    (the master's from build 30; a saved one loads as SPRING, build 31)
 * and build 32's: SMR SMEAR, DIF DIFFUSE, SLW SLEW, XOR XOR, DST DUST (further down) */

enum { RM_FLT, RM_FLD, RM_MU, RM_SHF, RM_MDL, RM_STR, RM_VOW, RM_DLY, RM_TAP, RM_RVS, RM_SPR, RM_REV, RM_GLT,
       RM_CLD, RM_SCR, RM_RNG, RM_STV, RM_CHB, RM_BNK, RM_SMR, RM_DIF, RM_SLW, RM_XOR, RM_DST, NKIND };
static const char *const RM_NAME[NKIND] = {"FILTER", "FOLD", "MULAW", "SHIFT", "MODAL", "STRING", "VOWEL", "DELAY",
                                           "TAPE", "REVERSE", "SPRING", "REVERB", "GLITCH",
                                           "CLOUD", "SCREAM", "RING", "STARVE", "CHEB", "BANK",
                                           "SMEAR", "DIFFUSE", "SLEW", "XOR", "DUST"};
/* the values an effect comes back to (a draw moves them, as Vascular draws a processor's) */
static const uint8_t RM_HOME[NKIND][4] = {
    {20, 55, 40, 30}, {45, 45, 20, 70}, {45, 60, 50, 30}, {40, 30, 70, 50}, {40, 55, 40, 50}, {35, 60, 40, 60},
    {50, 50, 50, 30}, {65, 40, 40, 60}, {50, 30, 45, 40}, {40, 60, 30, 50}, {50, 40, 50, 50}, {65, 60, 40, 55},
    {50, 40, 35, 45},
    {55, 45, 20, 50}, {50, 60, 45, 50}, {45, 50, 20, 60}, {60, 30, 35, 55}, {40, 50, 60, 60}, {40, 60, 30, 50},
    {60, 40, 70, 55}, {60, 55, 70, 60}, {35, 40, 70, 60}, {40, 15, 45, 45}, {40, 40, 45, 70}};
/* .. and the bounds a draw keeps them in, so that every effect is heard: no band-pass at 15 kHz, no spring tuned where
 * the sound has nothing */
static const uint8_t RM_LO[NKIND][4] = {
    {0, 35, 0, 0}, {10, 10, 0, 30}, {0, 30, 20, 0}, {0, 0, 0, 0}, {0, 15, 0, 20}, {0, 25, 0, 30},
    {0, 0, 10, 0}, {20, 0, 0, 40}, {10, 0, 0, 0}, {0, 30, 0, 0}, {0, 0, 35, 10}, {20, 20, 0, 40}, {15, 0, 0, 0},
    {10, 10, 0, 10}, {0, 25, 10, 10}, {0, 0, 0, 20}, {20, 0, 0, 20}, {0, 0, 20, 20}, {0, 25, 0, 10},
    {0, 0, 30, 20}, {0, 0, 30, 20}, {0, 0, 30, 15}, {0, 0, 0, 20}, {0, 0, 15, 30}};
static const uint8_t RM_HI[NKIND][4] = {
    {100, 85, 80, 100}, {90, 90, 80, 100}, {100, 100, 100, 80}, {100, 90, 100, 100}, {100, 85, 100, 100}, {100, 95, 100, 100},
    {100, 100, 90, 80}, {100, 85, 100, 100}, {100, 100, 90, 85}, {100, 100, 80, 100}, {100, 100, 92, 60}, {100, 95, 100, 100},
    {100, 90, 85, 100},
    {100, 100, 100, 100}, {100, 92, 90, 100}, {100, 100, 90, 100}, {100, 70, 100, 100}, {100, 100, 100, 100}, {100, 95, 100, 100},
    {100, 100, 100, 100}, {100, 100, 100, 100}, {85, 100, 100, 100}, {100, 100, 90, 85}, {100, 85, 90, 100}};
static volatile int32_t rack_root = 48;        /* the scale's root, MIDI: what MODAL and STRING are tuned to */

/* AINL helpers the effects share; then the effects */
/* an instance's place in the effect slots' memory (build 32: its random seed -- the same on the device and on a
 * computer, run after run; the address itself before) */
static uint32_t fx_place(const void *st);
/* ------------------------------------------------------------- shared --- */
/* a constant-peak-gain resonator (Smith and Angell): zeros at DC and Nyquist, poles at f, radius r. Q28 coefficients */
typedef struct { int32_t a1, a2, g; } res_c;
typedef struct { int32_t y1, y2, x1, x2, e; } res_s;   /* (y: Q8 above the sample; e: the fraction the last dropped) */
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
     * rings on by itself for ever, a small tone or a DC level, once its input has fallen silent. (build 17) Its state
     * 256 times finer than the sample, the result rounded towards zero: the last digit it may still hold is under the
     * sample's -- MODAL and BANK raise it x 600, and it sang on alone, nothing in */
    int64_t acc = (int64_t)c->a1 * s->y1 + (int64_t)c->a2 * s->y2 + (int64_t)c->g * ((int64_t)(x - s->x2) << 8) + s->e;
    int32_t y = (int32_t)(acc >> 28);
    s->e = (int32_t)(acc - ((int64_t)y << 28));
    s->y2 = s->y1;
    s->y1 = y;
    s->x2 = s->x1;
    s->x1 = x;
    return y < 0 ? -((-y) >> 8) : y >> 8;
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
/* (build 32: an effect's values from its knobs are worked out when the knobs move, not every block -- the same
 * numbers; key: the knobs packed, have: worked out once) */
AINL uint32_t p_key(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
typedef struct { svf_t f; uint32_t key, have; svf_c c; int32_t type, drive, comp; } m_flt_t;
static void m_flt(const uint8_t *p, m_flt_t *s, int32_t *x)
{
    int32_t type, drive, comp;
    uint32_t i;
    if (!s->have || s->key != p_key(p)) {
        int32_t oct = 321600 + p[1] * (9 * 65536 / 100), hz, q;
        hz = clampi((int32_t)(exp2_q16(clampi(oct, 4 << 16, 14 << 16)) >> 16), 30, 16000);
        q = 2896 + p[2] * (12 * 4096 - 2896) / 100;         /* Q 0.7 .. 12, Q12 */
        if (hz > 3000)                                      /* (high up, less of it: no whistle at 8 kHz) */
            q = 2896 + (q - 2896) * 3000 / hz;          /* (32 bits: < 1.4e8) */
        s->type = p[0] < 25 ? 1 : p[0] < 50 ? 2 : p[0] < 75 ? 3 : 4;
        s->drive = 4096 + p[3] * (11 * 4096) / 100;
        s->comp = (int32_t)(((uint32_t)Q15 * 4096u) / (uint32_t)(4096 + (q - 2896) / 3));   /* (x 1 .. ~0.3 with the Q) */
        svf_coef(&s->c, hz, q);
        s->key = p_key(p);
        s->have = 1;
    }
    type = s->type, drive = s->drive, comp = s->comp;
    {   /* (build 17: nothing in and only a remainder left -- a high Q's rounding held one for ever: let it go) */
        uint32_t any = 0;
        for (i = 0; i < CTL; i++)
            any |= (uint32_t)x[i];
        if (!any && absi(s->f.ic1) < 128 && absi(s->f.ic2) < 128)
            s->f.ic1 = s->f.ic2 = 0;
    }
    for (i = 0; i < CTL; i++) {
        int32_t v = tanh_q15(mulq12(x[i], drive)) >> 1, y;
        if (type == 4) {                                    /* notch: the band taken out (low + high) */
            int32_t bp = svf_run(&s->f, &s->c, v, 2);
            y = v - mulr24(s->c.k, bp);
        } else
            y = svf_run(&s->f, &s->c, v, type) * (type == 2 ? 2 : 1);
        x[i] = mulq15(tanh_q15(mulq15(y, comp) * 2), LV(0.7));   /* (a soft ceiling under full scale) */
    }
}

/* --------------------------------------------------------------- FOLD --- */
typedef struct { int32_t lp; uint32_t key, have, rp; int32_t g1, w, g2, tone, back; } m_fld_t;
/* reflected into -w .. w: (x + w) mod 4w, by a reciprocal of 4w (rp = (2^32 - 1) / 4w, worked out with the values)
 * and a step or two of correction -- the remainder exactly as % gives it, without a division a sample */
AINL int32_t fold_into(int32_t x, int32_t w, uint32_t rp)
{
    uint32_t p = 4u * (uint32_t)w;
    if (x > w || x < -w) {
        int32_t u = x + w;
        uint32_t a = (uint32_t)(u < 0 ? -u : u), m = a - (uint32_t)(((uint64_t)a * rp) >> 32) * p;
        while (m >= p)
            m -= p;
        if (u < 0 && m)
            m = p - m;
        x = (int32_t)m < 2 * w ? (int32_t)m - w : 3 * w - (int32_t)m;
    }
    return x;
}
static void m_fld(const uint8_t *p, m_fld_t *s, int32_t *x)
{
    int32_t g1, w, g2, tone, back;
    uint32_t i, rp;
    if (!s->have || s->key != p_key(p)) {
        s->g1 = (int32_t)(exp2_q16((p[0] * (5 * 65536 / 100)) - 65536) >> 4);   /* x 0.5 .. x 16, Q12 */
        s->w = 2621 + p[1] * (19005 - 2621) / 100;                             /* 0.08 .. 0.58 */
        s->g2 = 4096 + p[2] * (4 * 4096) / 100;                                /* x 1 .. x 5, Q12 */
        s->tone = 1400 + p[3] * (29430 - 1400) / 100;                          /* 300 Hz .. 16 kHz */
        s->back = (int32_t)((Q15 * 6000u) / (uint32_t)s->w);                   /* 0.18 / w, Q15 */
        s->rp = 0xFFFFFFFFu / (4u * (uint32_t)s->w);
        s->key = p_key(p);
        s->have = 1;
    }
    g1 = s->g1, w = s->w, g2 = s->g2, tone = s->tone, back = s->back, rp = s->rp;
    for (i = 0; i < CTL; i++) {
        int32_t y = fold_into(mulq12(x[i], g1), w, rp);
        y = fold_into(mulq12(y, g2), w, rp);
        y = mulq15(y, back > 4 * Q15 ? 4 * Q15 : back);
        s->lp += mulq15(y - s->lp, tone);
        x[i] = s->lp;
    }
}

/* -------------------------------------------------------------- MULAW --- */
/* (build 12: further) p0 the rate (FS down to FS / 32, fractional: to 1.4 kHz), p1 the bits (8 down to 2, sign
 * included), p2 the filter before and after (0.05 .. 1.0 of the lower rate: open, it aliases), p3 the drive into it
 * (x 1 .. x 8: mu-law lifts the quiet) */
typedef struct { uint32_t ph; int32_t hold, a1, a2, b1, b2; uint32_t key, have, inc, shift; int32_t k, drive, undo; } m_mu_t;
static void m_mu(const uint8_t *p, m_mu_t *s, int32_t *x)
{
    uint32_t inc, shift, i;
    int32_t k, drive, undo;
    if (!s->have || s->key != p_key(p)) {
        uint32_t div16 = 16u + (uint32_t)(((uint32_t)p[0] * p[0]) * 496u / 10000u);   /* 1 .. 32, Q4 (squared: the
                                                          * low rates at the top of the knob) */
        int32_t hz = (int32_t)((uint32_t)FS * 16u / div16) * (5 + p[2] * 95 / 100) / 100;
        s->inc = (uint32_t)(0xFFFFFFFFu / div16) << 4;
        s->shift = 6u - (uint32_t)(p[1] * 7 / 101);     /* code bits dropped: 0 .. 6 (2 bits left) */
        s->k = clampi((hz * 4781) >> 10, 150, 32000);   /* a one-pole's step: 2 pi f / FS, Q15 */
        s->drive = 4096 + p[3] * (7 * 4096) / 100;
        s->undo = (4096 * 4096) / s->drive;
        s->key = p_key(p);
        s->have = 1;
    }
    inc = s->inc, shift = s->shift, k = s->k, drive = s->drive, undo = s->undo;
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
        x[i] = mulq12(s->b2, undo);
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
typedef struct { int16_t line[3][STR_N]; uint32_t w; int32_t lp[3], in; uint32_t key, have; int32_t d8[3], g, tone, lvl, root; } m_str_t;
static void m_str(const uint8_t *p, m_str_t *s, int32_t *x)
{
    int32_t g, tone, lvl, k, root = rack_root, pk = 0;
    uint32_t i, any = 0;
    if (!s->have || s->key != p_key(p) || s->root != root) {
        int32_t note = 24 + (root % 12) + 12 * (p[0] * 4 / 101), hz8 = note_hz8(note, 0), len8[3];
        int32_t t100 = (int32_t)((8u * exp2_q16(p[1] * (664 * 65536 / 10000))) >> 16);   /* 0.08 .. 8 s, x 100 (2^6.64 = 100) */
        len8[0] = (int32_t)(((uint32_t)FS << 16) / (uint32_t)hz8);   /* samples, Q8 */
        len8[1] = (int32_t)(((int64_t)len8[0] * 64936) >> 16);   /* +0.9 % */
        len8[2] = (int32_t)(((int64_t)len8[0] * 65995) >> 16);   /* -0.7 % */
        for (k = 0; k < 3; k++)
            s->d8[k] = clampi(len8[k], 4 << 8, (int32_t)(STR_N - 3u) << 8);
        /* the loop gain: 60 dB in T s, a trip each 1 / f: 2^(-9.966 / (T f)) */
        {   /* 9.966 / (T f), Q16 = 9.966 x 65536 x 100 x 256 / (t100 x hz8) */
            uint32_t den = ((uint32_t)t100 * (uint32_t)hz8) >> 8;
            g = (int32_t)(exp2_q16(-(int32_t)(65313178u / (den ? den : 1u))) >> 1);
        }
        s->g = clampi(g, 6554, 32751);
        s->tone = 2800 + p[3] * (30500 - 2800) / 100;
        s->lvl = p[2] * (Q15 / 100);
        s->key = p_key(p);
        s->root = root;
        s->have = 1;
    }
    g = s->g, tone = s->tone, lvl = s->lvl;
    for (i = 0; i < CTL; i++)
        any |= (uint32_t)x[i];
    for (i = 0; i < CTL; i++) {
        int32_t y = 0;
        s->in += mulz15(x[i] - s->in, 15700);           /* (~4 kHz; towards zero, so silence settles at 0, not -1) */
        for (k = 0; k < 3; k++) {
            int32_t v;
            uint32_t rp = (s->w << 8) + (STR_N << 8) - (uint32_t)s->d8[k], i0 = rp >> 8, i1;
            if (i0 >= STR_N)                            /* (w + N - d, d within 4 .. N - 3: under 2N, one step) */
                i0 -= STR_N;
            i1 = i0 + 1u >= STR_N ? 0u : i0 + 1u;
            v = s->line[k][i0] + (s->line[k][i1] - s->line[k][i0]) * (int32_t)(rp & 255u) / 256;   /* (towards zero:
                                                  * a shift leaned every trip one way, and a string held a whisper at
                                                  * some roots for ever -- build 36) */
            s->lp[k] += mulz15(v - s->lp[k], tone);
            s->line[k][s->w] = (int16_t)clampi(s->in / 4 + mulz15(s->lp[k], g), -32767, 32767);
            y += k ? mulq15(v, lvl) : v;
        }
        if (++s->w >= STR_N)
            s->w = 0;
        x[i] = y;
        pk = maxi(pk, absi(y));
    }
    if (!any && absi(s->in) < 8 && pk && pk < 128) {          /* nothing in and under -48 dB: let go at once (build 36: the
                                                      * roundings of a long, nearly lossless loop could keep a whisper
                                                      * going round for ever at some roots) */
        memset(s->line, 0, sizeof s->line);
        s->lp[0] = s->lp[1] = s->lp[2] = 0;
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
    uint32_t q = (w << 8) + (n << 8) - (uint32_t)t8, i0 = q >> 8, f = q & 255u;
    int32_t y;
    if (i0 >= n)                                 /* (build 32: the remainder by a step -- w + n - t is under 2n) */
        i0 -= n;
    if (i0 >= n)
        i0 %= n;
    y = mu_dec8(line[i0]);
    return y + (((mu_dec8(line[i0 + 1u >= n ? 0u : i0 + 1u]) - y) * (int32_t)f) >> 8);
}
#define XF_PAIRS 512u            /* a change of time: the old head fades out, the new in, over 23 ms */
/* --------------------------------------------------------------- TAPE --- */
/* an echo on an 8-bit mu-law line at FS / 2: time 25 .. 900 ms (p0), (build 12) DROPOUTS (p1: the tape failing --
 * the echoes gone for 20 .. 200 ms, now and then, up to twice a second), feedback 0 .. 0.94 (p2) through the head's
 * saturation, AGE (p3: the loop's low-pass from 12 kHz down to 3 kHz, the head driven harder, hiss under it). The
 * time's own wander is a breath (at most 0.1 %), never a bend. The echoes alone come out. A change of time cross-fades */
#define TAP_N 19900u             /* 0.9 s at FS / 2 */
typedef struct { uint8_t line[TAP_N]; uint32_t w; int32_t t, t0, xf, lp, prev, wn, wt, dg, drop, env; rnd_t r; } m_tap_t;
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
        {   /* (build 17: the hiss under what passes, as on a tape: it follows the input and the echoes, ~0.3 s out,
             * and is gone with them -- before, it hissed on alone and kept the effect running for ever) */
            int32_t a = maxi(absi(in), absi(s->lp));
            s->env += a > s->env ? (a - s->env) >> 3 : -((s->env >> 12) + (s->env > 0));
        }
        y = mulq15(s->lp, s->dg) + (hiss ? mulq15(rnd_bi(&s->r), mulq15(hiss, clampi(s->env * 16, 0, Q15))) : 0);
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
        rnd_seed(&s->r, 0x474C54u ^ fx_place(s));
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
 * holds (and the slot's gate still closes it once no note sounds); a fresh one (a new patch, a recall) fills its 0.74 s
 * with the sound first, then holds it. The grains alone come out */
#define CLD_N 16384u
#define CLD_G 6u
typedef struct {
    uint8_t line[CLD_N];
    uint32_t w, filled, gp[CLD_G], gw[CLD_G], gwinc[CLD_G];
    volatile uint32_t fresh;     /* (build 19) pairs still to be written anew, frozen or not: a morph's end (fx_refill) */
    int32_t next, prev;
    uint8_t gon[CLD_G];
    rnd_t r;
} m_cld_t;
static void m_cld(const uint8_t *p, m_cld_t *s, int32_t *x)
{
    uint32_t len = ms_samples2(30u + p[0] * 220u / 100u), rate = 3u + p[1] * 57u / 100u, iv = 22050u / rate;
    uint32_t spread = ms_samples2(50u + p[3] * 650u / 100u), i, k, k2;
    int frozen = p[2] >= 50 && s->filled >= CLD_N && !s->fresh;   /* (frozen only once it holds something: a fresh cloud
                                                     * fills first, with the sound now, then holds it -- never an empty,
                                                     * silent one; told to, it takes the sound now again, its grains
                                                     * going on over it) */
    int32_t norm, o0;
    if (!s->r.s)
        rnd_seed(&s->r, 0x434C44u ^ fx_place(s));
    k2 = (iv << 15) / (3u * len);                        /* (as the voices' cloud: each grain at sqrt(8 / 3 O)) */
    norm = clampi((int32_t)isqrt32((k2 > 65535u ? 65535u : k2) << 16) << 1, 4096, 2 * Q15);
    for (i = 0; i + 1u < CTL; i += 2u) {
        int32_t in = (x[i] + x[i + 1u]) >> 1, y = 0;
        if (!frozen) {
            s->line[s->w] = mu_enc8(clampi(in, -32767, 32767));
            s->w = (s->w + 1u) & (CLD_N - 1u);
            if (s->filled < CLD_N)
                s->filled++;
            if (s->fresh)
                s->fresh--;
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
typedef struct { int32_t env, lp, open, dc; } m_stv_t;   /* (dc: Q10 above the sample) */
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
        s->dc += y - (s->dc >> 10);                      /* (the clipper's lean taken off: Q10, so it settles on the
                                                          * lean itself -- build 17: it stopped up to 1023 short and a
                                                          * GATE of 0 held that much out for ever, nothing in) */
        s->lp += mulz15(y - (s->dc >> 10) - s->lp, tone);
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
#define FX_ARENA 36864u          /* a slot's memory (47 KiB from build 10; 36 KiB from build 31, DELAY and REVERB the
                                  * master's: the largest chain drawn, GLITCH + VOWEL + TAPE, is 36.5 KiB) */
#define FX_CLEAR 6144u           /* .. cleared this much a block when a new patch comes (6 blocks) */
#define FX_FADE 12u              /* blocks of a slot's fade out before a new patch (~9 ms) */
#define RM_TAIL 2756u            /* an effect nothing reaches goes on until its output has been near silence 2 s */

/* ============================================================== build 32 ===
 * Five more, cheap -- each under ~40 operations a sample and a few KiB --, none sounding with nothing in, none bending
 * a pitch. (Their names are provisional.) */

/* -------------------------------------------------------------- SMEAR --- */
/* a velvet-noise smear (after Alary, Politis and Valimaki's velvet-noise decorrelator, DAFx 2017): the sound through
 * SMR_T taps at random places within SPAN (p0: 12 .. 46 ms), each + or -, their weights falling to TAIL (p1: the last
 * at 1.0 .. 0.1 of the first) -- smoke, not a room; p2 the mix (0.4 .. 1 of it), p3 the tone after (300 Hz .. 16 kHz).
 * No feedback: nothing in, nothing out 46 ms later. The taps are drawn from the instance's place: the same each time */
#define SMR_N 2048u              /* 46 ms */
#define SMR_T 20u
typedef struct { int16_t line[SMR_N]; uint16_t at[SMR_T]; int16_t g[SMR_T]; uint32_t w, key, have; int32_t lp, norm, mix, tone; } m_smr_t;
static void m_smr(const uint8_t *p, m_smr_t *s, int32_t *x)
{
    uint32_t i, k;
    int32_t norm, mix, tone;
    if (!s->have || s->key != p_key(p)) {
        rnd_t r;
        uint32_t span = 530u + (uint32_t)p[0] * (SMR_N - 8u - 530u) / 100u, seg = span / SMR_T, e2 = 0;
        int32_t gl = Q15 - p[1] * (Q15 * 9 / 10) / 100;   /* (the last tap's weight) */
        rnd_seed(&r, 0x534D52u ^ fx_place(s));
        for (k = 0; k < SMR_T; k++) {
            int32_t g = (Q15 + (gl - Q15) * (int32_t)k / (int32_t)(SMR_T - 1u)) >> 4;   /* Q11 */
            s->at[k] = (uint16_t)(1u + k * seg + (uint32_t)rnd_below(&r, (int32_t)seg));
            s->g[k] = (int16_t)(rnd_chance(&r, 50) ? g : -g);
            e2 += (uint32_t)(g * g);
        }
        s->norm = (int32_t)((2048u * 32768u) / (isqrt32(e2) + 1u));   /* (the sum's level as the sound's, Q15) */
        s->mix = LV(0.4) + p[2] * (Q15 - LV(0.4)) / 100;
        s->tone = 1400 + p[3] * (30000 - 1400) / 100;
        s->key = p_key(p);
        s->have = 1;
    }
    norm = s->norm, mix = s->mix, tone = s->tone;
    for (i = 0; i < CTL; i++) {
        int32_t acc = 0, y;
        s->line[s->w] = (int16_t)clampi(x[i], -32767, 32767);
        for (k = 0; k < SMR_T; k++)                     /* (Q15 x Q11, twenty: under 2^31) */
            acc += s->line[(s->w - s->at[k]) & (SMR_N - 1u)] * s->g[k];
        s->w = (s->w + 1u) & (SMR_N - 1u);
        y = mulq15(acc >> 11, norm);
        y = mulq15(x[i], Q15 - mix) + mulq15(y, mix);
        s->lp += mulz15(y - s->lp, tone);
        x[i] = s->lp;
    }
}

/* ------------------------------------------------------------ DIFFUSE --- */
/* four allpasses in a row (Schroeder's; the lengths of Mutable Instruments' Clouds diffuser, at 44.1 kHz): the edges
 * of the sound blurred, no tail of its own. p0 the size (0.4 .. 1 of the lengths), p1 the diffusion (0.4 .. 0.75), p2
 * the mix (0.5 .. 1), p3 the tone after. The lengths never slide: a new size starts each line's reading afresh */
#define DIF_N 1405u
static const uint16_t DIF_L[4] = {174, 248, 371, 612}, DIF_O[4] = {0, 174, 422, 793};
typedef struct { int16_t d[DIF_N]; uint16_t len[4], w[4]; uint32_t key, have; int32_t lp, kap, mix, tone; } m_dif_t;
static void m_dif(const uint8_t *p, m_dif_t *s, int32_t *x)
{
    uint32_t i, k;
    int32_t kap, mix, tone;
    if (!s->have || s->key != p_key(p)) {
        for (k = 0; k < 4u; k++) {
            s->len[k] = (uint16_t)maxi(8, DIF_L[k] * (40 + p[0] * 60 / 100) / 100);
            if (s->w[k] >= s->len[k])
                s->w[k] = 0;
        }
        s->kap = LV(0.4) + p[1] * (LV(0.75) - LV(0.4)) / 100;
        s->mix = LV(0.5) + p[2] * (Q15 - LV(0.5)) / 100;
        s->tone = 1400 + p[3] * (30000 - 1400) / 100;
        s->key = p_key(p);
        s->have = 1;
    }
    kap = s->kap, mix = s->mix, tone = s->tone;
    for (i = 0; i < CTL; i++) {
        int32_t v = x[i], y;
        for (k = 0; k < 4u; k++) {
            int16_t *line = &s->d[DIF_O[k]];
            int32_t d = line[s->w[k]], a = v + mulz15(d, kap);   /* (towards zero: it dies out to nothing) */
            line[s->w[k]] = (int16_t)clampi(a, -32767, 32767);
            v = d - mulz15(a, kap);
            if (++s->w[k] >= s->len[k])
                s->w[k] = 0;
        }
        y = mulq15(x[i], Q15 - mix) + mulq15(v, mix);
        s->lp += mulz15(y - s->lp, tone);
        x[i] = s->lp;
    }
}

/* --------------------------------------------------------------- SLEW --- */
/* a slew limiter (after Airwindows' Slew): the sound may move only so far a sample -- the highs turned to dark,
 * grinding ramps. p0 the rate (40 .. 4000 a sample: from treacle to a trace), p1 the drive into it (x 1 .. x 8),
 * p2 the mix (0.3 .. 1), p3 the tone after. With nothing in it walks to 0 and stays */
typedef struct { int32_t y, lp; uint32_t key, have; int32_t step, drive, mix, tone; } m_slw_t;
static void m_slw(const uint8_t *p, m_slw_t *s, int32_t *x)
{
    uint32_t i;
    int32_t step, drive, mix, tone;
    if (!s->have || s->key != p_key(p)) {
        s->step = (int32_t)((40u * exp2_q16(p[0] * (664 * 65536 / 10000))) >> 16);   /* 40 .. 4000 */
        s->drive = 4096 + p[1] * (7 * 4096) / 100;
        s->mix = LV(0.3) + p[2] * (Q15 - LV(0.3)) / 100;
        s->tone = 1400 + p[3] * (30000 - 1400) / 100;
        s->key = p_key(p);
        s->have = 1;
    }
    step = s->step, drive = s->drive, mix = s->mix, tone = s->tone;
    for (i = 0; i < CTL; i++) {
        int32_t v = clampi(mulq12(x[i], drive), -2 * Q15, 2 * Q15), y;
        s->y += clampi(v - s->y, -step, step);
        y = mulq15(x[i], Q15 - mix) + mulq15(tanh_q15(s->y), mix);
        s->lp += mulz15(y - s->lp, tone);
        x[i] = s->lp;
    }
}

/* ---------------------------------------------------------------- XOR --- */
/* the sound combined bit by bit with itself a moment before (after the Teensy Audio library's effect_combine):
 * broken, digital, following the sound's shape. p0 the moment (0.1 .. 23 ms), p1 the operation (XOR, AND, OR), p2 the
 * bits kept (3 .. 10: fewer, harsher), p3 the tone after. Both taken towards zero first, so near-silence is silence;
 * the lean the bits leave taken off */
#define XOR_N 1024u
typedef struct { int16_t line[XOR_N]; uint32_t w, key, have; int32_t dx, dy, lp, back, op, sh, tone; } m_xor_t;
AINL int32_t xor_q(int32_t v, int32_t sh) { return v < 0 ? -((-v) >> sh) : v >> sh; }
static void m_xor(const uint8_t *p, m_xor_t *s, int32_t *x)
{
    uint32_t i;
    int32_t back, op, sh, tone;
    if (!s->have || s->key != p_key(p)) {
        s->back = 4 + p[0] * (int32_t)(XOR_N - 8u) / 100;
        s->op = p[1] < 34 ? 0 : p[1] < 67 ? 1 : 2;
        s->sh = 13 - p[2] * 7 / 100;                    /* (16 bits: 3 .. 10 kept) */
        s->tone = 1400 + p[3] * (30000 - 1400) / 100;
        s->key = p_key(p);
        s->have = 1;
    }
    back = s->back, op = s->op, sh = s->sh, tone = s->tone;
    for (i = 0; i < CTL; i++) {
        int32_t a = clampi(x[i] * 2, -32767, 32767), b = s->line[(s->w - (uint32_t)back) & (XOR_N - 1u)], qa, qb, r, y;
        s->line[s->w] = (int16_t)a;
        s->w = (s->w + 1u) & (XOR_N - 1u);
        qa = xor_q(a, sh);
        qb = xor_q(b, sh);
        r = op == 0 ? qa ^ qb : op == 1 ? qa & qb : qa | qb;
        y = r * (1 << (sh - 1));                        /* (back to the sample, halved) */
        {   /* (the lean off: y = x - x1 + R y1, towards zero) */
            int32_t yq = y - s->dx + mulz15(s->dy, 32500);
            s->dx = y;
            s->dy = yq;
            y = yq;
        }
        s->lp += mulz15(y - s->lp, tone);
        x[i] = s->lp;
    }
}

/* --------------------------------------------------------------- DUST --- */
/* hiss and crackle that live where the sound moves (after Airwindows' TapeDust): noise scaled by how fast the sound is
 * changing, followed a little. p0 the grain (hiss .. crackle, the crackle dense: ~600 grains a second, never a
 * rhythm), p1 the amount (x 1 .. x 8), p2 the dust's tone (300 Hz .. 16 kHz), p3 the sound under it (0 .. 1).
 * Nothing moving, nothing in it */
typedef struct { int32_t prev, env, nlp, seeded; rnd_t r; uint32_t key, have; int32_t grain, amt, tone, dry; } m_dst_t;
static void m_dst(const uint8_t *p, m_dst_t *s, int32_t *x)
{
    uint32_t i;
    int32_t grain, amt, tone, dry;
    if (!s->seeded) {
        rnd_seed(&s->r, 0x445354u ^ fx_place(s));
        s->seeded = 1;
    }
    if (!s->have || s->key != p_key(p)) {
        s->grain = p[0] * (Q15 / 100);
        s->amt = 4096 + p[1] * (7 * 4096) / 100;
        s->tone = 1400 + p[2] * (30000 - 1400) / 100;
        s->dry = p[3] * (Q15 / 100);
        s->key = p_key(p);
        s->have = 1;
    }
    grain = s->grain, amt = s->amt, tone = s->tone, dry = s->dry;
    for (i = 0; i < CTL; i++) {
        int32_t sl = absi(x[i] - s->prev) * 4, n = rnd_bi(&s->r), hiss, crack = 0;
        s->prev = x[i];
        if (sl > s->env)                                /* (the slope followed: fast up, ~20 ms down, to 0) */
            s->env += (sl - s->env) >> 3;
        else
            s->env -= (s->env >> 8) + (s->env > 0);
        hiss = mulq15(n, mini(s->env, 2 * Q15));
        if (rnd_q15(&s->r) < 446)                       /* (~600 a second) */
            crack = hiss * 4;
        hiss = mulq15(hiss, Q15 - grain) + mulq15(crack, grain);
        s->nlp += mulz15(clampi(mulq12(hiss, amt), -2 * Q15, 2 * Q15) - s->nlp, tone);
        x[i] = mulq15(x[i], dry) + s->nlp;
    }
}

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
    case RM_SMR: n = sizeof(m_smr_t); break;
    case RM_DIF: n = sizeof(m_dif_t); break;
    case RM_SLW: n = sizeof(m_slw_t); break;
    case RM_XOR: n = sizeof(m_xor_t); break;
    case RM_DST: n = sizeof(m_dst_t); break;
    default: n = 8u; break;                     /* (DELAY, REVERB: the master's from build 30, nothing here) */
    }
    return (n + 7u) & ~7u;
}

/* The recipes: a slot's chain, place by place, each place a choice among a few effects (a bit each); the third place
 * there one time in two when the recipe has one (FX_OPT). After Vascular's abstract chains and scenes */
#define K(x) (1u << RM_##x)
enum { FR_SPACE, FR_ECHO, FR_METAL, FR_STRINGS, FR_EROSION, FR_GHOST, FR_SPECTRAL, FR_RESONANT, FR_FOLDS, FR_TELEPHONE,
       FR_HELL, FR_FACTORY, FR_GLITCH, FR_BROADCAST, FR_ABYSS, FR_RUST,
       FR_FREEZER, FR_FEEDBACK, FR_FOUNDRY, FR_GONG, FR_SPECTRE, FR_SPUTTER, FR_SMOG, FR_STATIC, FR_N };
static const char *const FR_NAME[FR_N] = {"SPACE", "ECHO", "METAL", "STRINGS", "EROSION", "GHOST", "SPECTRAL", "RESONANT",
                                          "FOLDS", "TELEPHONE", "HELL", "FACTORY", "GLITCH", "BROADCAST", "ABYSS", "RUST",
                                          "FREEZER", "FEEDBACK", "FOUNDRY", "GONG", "SPECTRE", "SPUTTER", "SMOG", "STATIC"};
#define FX_OPT 0x80000000u       /* (a place that may be left out) */
static const uint32_t FR_PLACE[FR_N][FX_N] = {   /* (build 30: DELAY and REVERB no longer drawn -- they are the
                                                 * master's; a session saved with them still plays them. Build 32: SMEAR,
                                                 * DIFFUSE, SLEW, XOR, DUST among the choices; SMOG, STATIC) */
    {K(FLT) | K(VOW), K(TAP), K(SPR) | K(CLD) | K(DIF)},                        /* SPACE: a colour, an echo, a room */
    {K(TAP), K(SPR), FX_OPT | K(FLT) | K(DIF)},                                 /* ECHO */
    {K(MDL) | K(SHF), K(FLD) | K(FLT), FX_OPT | K(SPR)},                        /* METAL: struck, bent, a room */
    {K(STR), K(TAP) | K(FLT), FX_OPT | K(SPR) | K(DIF)},                        /* STRINGS: everything rings */
    {K(MU) | K(FLD) | K(SLW), K(RVS) | K(TAP), FX_OPT | K(SPR) | K(DST)},       /* EROSION */
    {K(FLT) | K(VOW) | K(SMR), K(RVS), K(SPR) | K(CLD)},                        /* GHOST: the sound reversed, far off */
    {K(SHF), K(VOW) | K(FLT), FX_OPT | K(TAP) | K(SPR) | K(SMR)},               /* SPECTRAL: the partials moved */
    {K(VOW) | K(MDL), K(STR) | K(SPR), FX_OPT | K(TAP) | K(DIF)},               /* RESONANT */
    {K(FLD), K(FLT) | K(VOW) | K(SLW), FX_OPT | K(SPR) | K(TAP)},               /* FOLDS */
    {K(MU), K(SHF) | K(FLT), FX_OPT | K(SPR) | K(TAP) | K(DST)},                /* TELEPHONE */
    /* build 10: hellscapes, atmospheric industrial, abstract glitch */
    {K(FLD) | K(MU) | K(STV) | K(XOR), K(SPR) | K(MDL) | K(SCR), K(SPR) | K(CLD)},   /* HELL: torn, rung, a vast room */
    {K(MU) | K(STV) | K(XOR), K(MDL) | K(STR) | K(BNK) | K(RNG), FX_OPT | K(TAP)},   /* FACTORY: crushed, metal resonance */
    {K(GLT), K(RVS) | K(MU) | K(XOR), FX_OPT | K(TAP)},                         /* GLITCH: bent buffers */
    {K(GLT) | K(MU) | K(DST), K(VOW) | K(FLT), FX_OPT | K(TAP)},                /* BROADCAST: a broken transmission */
    {K(SHF) | K(RNG), K(STR) | K(MDL) | K(BNK), K(SPR) | K(CLD) | K(DIF)},      /* ABYSS: moved partials ringing, deep */
    {K(SPR), K(FLD) | K(GLT) | K(SLW), FX_OPT | K(TAP)},                        /* RUST: a spring, torn, glitched */
    /* build 11: the drastic ones */
    {K(CLD), K(BNK) | K(VOW) | K(SHF) | K(SMR), FX_OPT | K(SPR) | K(TAP)},      /* FREEZER: a cloud held, coloured */
    {K(SCR), K(FLT) | K(BNK) | K(STV), FX_OPT | K(GLT) | K(DST)},               /* FEEDBACK: a howl, shaped, torn */
    {K(RNG), K(STV) | K(FLD) | K(CHB), FX_OPT | K(SPR)},                        /* FOUNDRY: clangour through a fuzz */
    {K(BNK), K(RNG) | K(SHF) | K(CLD), FX_OPT | K(SPR) | K(DIF)},               /* GONG: a struck body, ringing */
    {K(CHB), K(CLD) | K(RVS) | K(SMR), FX_OPT | K(SCR)},                        /* SPECTRE: re-voiced, smeared */
    {K(STV), K(GLT) | K(MU) | K(RNG) | K(XOR), FX_OPT | K(SPR) | K(TAP)},       /* SPUTTER: torn and broken */
    /* build 32 */
    {K(SMR) | K(DIF), K(FLT) | K(VOW) | K(SLW), FX_OPT | K(DST) | K(TAP)},      /* SMOG: a haze, dark, a grain in it */
    {K(XOR) | K(DST), K(SLW) | K(MU) | K(FLT), FX_OPT | K(DIF) | K(SPR)},       /* STATIC: a broken signal, blurred */
};
static const uint8_t FR_W[FR_N] = {4, 4, 4, 3, 4, 3, 3, 3, 3, 2, 4, 4, 4, 3, 4, 3, 5, 4, 5, 4, 4, 4, 4, 4};
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
static uint32_t fx_place(const void *st) { return (uint32_t)((const uint8_t *)st - &fx_arena[0][0]); }
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
static int32_t fx_dcx[NFX], fx_dcy[NFX];   /* the DC blocker: last input, last output (Q8 above the sample) */
static int32_t rm_env[NINST], rm_g[NINST];     /* each instance's guard */
static volatile int32_t rm_meter[NINST];       /* each instance's output, its last peaks (the screen's) */
static uint16_t rm_quiet[NINST];
/* the job's buffers: what the generators sent into each instance the block before (CPU0 fills ebus_next, the rack
 * runs on ebus_run: one block later, so the rack can run on CPU1 while CPU0 renders the voices); the rack's output */
static int32_t ebus_mem[2][NINST][CTL] POOL __attribute__((aligned(32)));
static int32_t (*ebus_next)[CTL] = ebus_mem[0], (*ebus_run)[CTL] = ebus_mem[1];   /* (build 32: swapped, not copied) */
#define EBUS_SIZE (sizeof ebus_mem[0])
static int32_t fx_l[CTL], fx_r[CTL];
/* (build 32: the master delay and reverb run in the rack's job too, on what was sent to them the block before --
 * mix_block keeps the sends in mst_in_*, adds the returns from mst_out_*: off CPU0's way, 0.7 ms later) */
static int32_t mst_in_d[CTL], mst_in_r[CTL], mst_out_l[CTL], mst_out_r[CTL];
static void master_job(void);
static int32_t fx_mdl[CTL], fx_mrv[CTL];   /* the slots' sends to the master delay and reverb (master.c), this block */
static volatile int32_t fx_rsend[NFX], fx_dsend[NFX];   /* each slot's sends, Q15: the mean of its generators' (gens.c) */
/* the modulation's offsets (mod.c, worked out before the work is dealt): each slot's effects' values by place (4 k + j),
 * its level (steps) and pan */
static int16_t fx_mod_p[NFX][12], fx_mod_lvl[NFX], fx_mod_pan[NFX];
static uint8_t fx_mod_on[NFX];
static volatile int16_t fx_lvl_q8[NFX];           /* a snapshot's morph: each slot's level between its steps (Q8, 0 silent) */
static volatile uint8_t fx_fine;                  /* .. a bit a slot when in force (the mute folded in) */
static uint32_t ebus_fed, ebus_fed_run;        /* (the instances something was sent into) */
static uint32_t rack_ticks;                    /* what the last block of the rack took */
/* KNOB 2 (build 33): the effects' mix, 0 (each effect passes its input) .. 100 (its own output alone, as before):
 * every effect of every slot at once, each its own input and output crossed by it; 50 at power-on. Glided over a
 * block when it moves */
#define FX_WET_DEF 50
static volatile uint8_t fx_wet[NFX] = {FX_WET_DEF, FX_WET_DEF, FX_WET_DEF};   /* (build 36: a lane's own -- the chosen
                                                  * generator's lane, or the lane chosen; one for all before) */
static int32_t fx_wq[NFX] = {FX_WET_DEF * Q15 / 100, FX_WET_DEF * Q15 / 100, FX_WET_DEF * Q15 / 100};   /* (where each
                                                  * ended the last block, Q15) */

/* the guard: a peak rider (Tonitor's limiter: instant attack, ~200 ms release) to 0.5 */
#define RM_CEIL 16384
static int32_t rm_guard(uint32_t i0, int32_t *x)   /* (returns its output's peak: the meter's, build 32) */
{
    int32_t pk = 0, po = 0, want, g0 = rm_g[i0], dg;
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
        po = maxi(po, absi(x[i]));
    }
    rm_g[i0] = want;
    return po;
}

/* one instance over x (mono, in place) */
static void rm_block(uint32_t inst, const fx_eff_t *e, int32_t *x)
{
    void *st = fx_st[inst];
    const uint8_t *p = e->p;
    int32_t pk;
    switch (e->kind) {
    case RM_FLT: m_flt(p, (m_flt_t *)st, x); break;
    case RM_FLD: m_fld(p, (m_fld_t *)st, x); break;
    case RM_MU: m_mu(p, (m_mu_t *)st, x); break;
    case RM_SHF: m_shf(p, (m_shf_t *)st, x); break;
    case RM_MDL: m_mdl(p, (m_mdl_t *)st, x); break;
    case RM_STR: m_str(p, (m_str_t *)st, x); break;
    case RM_VOW: m_vow(p, (m_vow_t *)st, x); break;
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
    case RM_SMR: m_smr(p, (m_smr_t *)st, x); break;
    case RM_DIF: m_dif(p, (m_dif_t *)st, x); break;
    case RM_SLW: m_slw(p, (m_slw_t *)st, x); break;
    case RM_XOR: m_xor(p, (m_xor_t *)st, x); break;
    case RM_DST: m_dst(p, (m_dst_t *)st, x); break;
    default: break;                                  /* (DELAY, REVERB: never in a slot from build 31 -- fx_retire) */
    }
    pk = rm_guard(inst, x);
    rm_meter[inst] = pk > rm_meter[inst] ? pk : rm_meter[inst] - (rm_meter[inst] >> 4) - (rm_meter[inst] > 0);
    /* (build 17: down to 0 -- a sixteenth of 15 is nothing, so it stopped at 15 and the effect was never quiet: it ran
     * for ever, nothing reaching it, a TAPE hissing on alone) */
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
        if (fx_phase[s] == FS_FADE && (!fx_count[s] || (fx_mute[s] && !((fx_fine >> s) & 1u)))) {   /* (muted: nothing to
                                                                                                * fade) */
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
        int32_t sl[CTL], g, gl, gr, dg, dgl, dgr, gw;   /* (mono: build 32 -- no plate in a slot) */
        int32_t wq1 = clampi(fx_wet[s], 0, 100) * Q15 / 100, wr[CTL], mixing = wq1 < Q15 || fx_wq[s] < Q15;
        uint32_t ph;
        int fading = fx_phase[s] == FS_FADE;
        if (mixing) {                                /* (this lane's mix this block, sample by sample, from where it was) */
            int32_t dw = (wq1 - fx_wq[s]) / (int32_t)CTL, w = fx_wq[s];
            for (i = 0; i < CTL; i++)
                wr[i] = w += dw;
            wr[CTL - 1u] = wq1;
        }
        fx_wq[s] = wq1;
        if (fx_phase[s] == FS_CLEAR || (fx_mute[s] && !((fx_fine >> s) & 1u))) {   /* (a morph: the mute is its level) */
            fx_g[s] = 0;
            continue;
        }
        for (i = 0; i < CTL; i++)
            sl[i] = 0;
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
            int32_t dry[CTL], lvl;
            if (mixing)
                memcpy(dry, x, sizeof dry);
            if (fx_mod_on[s]) {                      /* (moved by the slot's modulation: within the effect's bounds) */
                fx_eff_t ee = f->e[k];
                uint32_t j;
                for (j = 0; j < 4u; j++)
                    ee.p[j] = (uint8_t)clampi((int32_t)ee.p[j] + fx_mod_p[s][k * 4u + j], RM_LO[ee.kind % NKIND][j],
                                              RM_HI[ee.kind % NKIND][j]);
                rm_block(inst, &ee, x);
            } else
                rm_block(inst, &f->e[k], x);
            lvl = rm_meter[inst];
            if (mixing)                              /* (KNOB 2: its input and its output crossed; what comes out
                                                      * keeps it running, not its own output alone) */
                for (i = 0; i < CTL; i++) {
                    x[i] = dry[i] + mulq15(x[i] - dry[i], wr[i]);
                    lvl = maxi(lvl, absi(x[i]));
                }
            if (lvl < 32) {               /* (-60 dB: build 17, 8 before -- small values some effects
                                                      * hold for ever kept them running, nothing reaching them) */
                if (rm_quiet[inst] < 0xFFFFu)
                    rm_quiet[inst]++;
            } else
                rm_quiet[inst] = 0;
            if (rm_quiet[inst] >= RM_TAIL)
                continue;
            if (out)
                for (i = 0; i < CTL; i++)
                    sl[i] += x[i];
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
            for (i = 0; i < CTL; i++) {
                int32_t xq = sl[i] << 8;                 /* y = x - x1 + R y1, R = 1 - 2 pi 8 / FS (towards zero) */
                int32_t y = xq - fx_dcx[s] + mulz15(fx_dcy[s], 32730);
                fx_dcx[s] = xq;
                fx_dcy[s] = y;
                sl[i] = y >> 8;
            }
            if (fx_idle[s] > GATE_WAIT && fx_gate[s] > 0) {
                int32_t g0 = fx_gate[s], g1 = maxi(g0 - GATE_STEP, 0), dgt = (g1 - g0) / (int32_t)CTL;
                for (i = 0; i < CTL; i++) {
                    g0 += dgt;
                    sl[i] = mulq15(sl[i], g0);
                }
                fx_gate[s] = g1;
            } else if (fx_gate[s] < Q15) {
                for (i = 0; i < CTL; i++)
                    sl[i] = mulq15(sl[i], fx_gate[s]);
            }
        }
        /* the slot out: its level, its place (the mono parts panned; the plate's stereo as it is), its fade */
        if ((fx_fine >> s) & 1u) {                   /* (a morph: between its steps, the mute in it) */
            int32_t q = fx_lvl_q8[s] > 0 ? clampi(fx_lvl_q8[s] + fx_mod_lvl[s] * 256, 0, 23 * 256) : 0;
            gw = q >= 256 ? (int32_t)(exp2_q16(((q - 20 * 256) * 16328) >> 8) >> 2)
                          : (int32_t)((exp2_q16((1 - 20) * 16328) >> 2) * (uint32_t)q >> 8);
        } else {
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
            int32_t a;
            fx_g[s] += dg;
            fx_gl[s] += dgl;
            fx_gr[s] += dgr;
            a = (int32_t)(((int64_t)sl[i] * fx_g[s]) >> 14);
            fx_l[i] += mulq15(a, fx_gl[s]);             /* (panned) */
            fx_r[i] += mulq15(a, fx_gr[s]);
            fx_mdl[i] += mulq15(a, fx_dsend[s]);        /* (on to the master delay and reverb) */
            fx_mrv[i] += mulq15(a, fx_rsend[s]);
        }
        fx_g[s] = g;
        fx_gl[s] = gl;
        fx_gr[s] = gr;
    }
    master_job();                                    /* the master delay and reverb (master.c) */
    rack_ticks = fm1_ticks() - t0;
}
static __typeof__(&rack_job) volatile rack_job_fp = rack_job;

/* ------------------------------------------------------- the draw (main loop) --- */
/* The draw's path (build 18; the 2nd and 3rd black keys, each a toggle, one at a time): what HOME draws leans its way.
 * DP_ANY: anything goes, as before (every draw the same as before). DP_ROUGH (ABRASIVE): gnarly, textural, abrasive --
 * the harsh scenes and families, more cables, the torn and crushed recipes, the values driven. DP_DARK (DARK): dark
 * ambient, drones -- the quiet, wide scenes and families, nothing harsh, a little quieter, the spaces and the ringing
 * recipes, the values darker and less driven. The sounds themselves are drawn by the engine as ever (mol.c): the path
 * chooses among them */
enum { DP_ANY, DP_ROUGH, DP_DARK, DP_N };
static uint8_t draw_path;
/* The signal flow: LANES, the only one from build 36 (MESH, from build 28 to 35, a session's other MODE -- a generator
 * cabled into any effect, effects across slots -- is gone): generator l runs through effect slot l alone, its lane:
 * cabled into its head, no cables between lanes, the lanes centre, left, right; HOME on a generator draws its lane's
 * chain with it, so the chain is part of the patch. Within a lane the outs to the master are still drawn and rewired.
 * A session keeps the flow it was saved in: SES_LANES; a MESH session of builds 28 .. 35 is rewired into lanes as it
 * loads (sessions.c ses_lanes) */
#define SES_LANES 1u
static const int8_t LANE_PAN[3] = {0, -14, 14};   /* (the lanes: centre, left, right) */
/* what each effect's values do, for the paths' leaning (RM_PNAME, mod.c) */
enum { VR_NONE, VR_DRIVE, VR_TONE, VR_SPACE, VR_DAMP, VR_BREAK, VR_BITS };
static const uint8_t RM_ROLE[NKIND][4] = {
    {0, 0, VR_DRIVE, VR_DRIVE}, {VR_DRIVE, 0, VR_DRIVE, VR_TONE}, {0, VR_BITS, VR_TONE, VR_DRIVE}, {0, VR_DRIVE, 0, VR_TONE},
    {0, 0, 0, VR_TONE}, {0, VR_SPACE, 0, VR_TONE}, {0, 0, 0, VR_DRIVE}, {0, 0, VR_DAMP, 0}, {0, VR_BREAK, 0, 0},
    {0, VR_TONE, 0, 0}, {0, 0, VR_SPACE, VR_TONE}, {VR_SPACE, VR_SPACE, VR_DAMP, 0}, {0, VR_BREAK, VR_BREAK, 0},
    {VR_SPACE, 0, 0, 0}, {0, 0, VR_TONE, VR_DRIVE}, {0, VR_DRIVE, VR_DRIVE, VR_TONE}, {VR_DRIVE, VR_BREAK, 0, VR_TONE},
    {0, 0, VR_DRIVE, VR_TONE}, {0, 0, 0, VR_TONE},
    {VR_SPACE, 0, 0, VR_TONE}, {VR_SPACE, 0, 0, VR_TONE}, {0, VR_DRIVE, 0, VR_TONE}, {0, 0, VR_BITS, VR_TONE}, {0, VR_DRIVE, VR_TONE, 0}};
#define RM_HARSH ((1u << RM_FLD) | (1u << RM_MU) | (1u << RM_STV) | (1u << RM_SCR) | (1u << RM_GLT) | (1u << RM_CHB) | (1u << RM_RNG) | \
                  (1u << RM_XOR) | (1u << RM_SLW))
/* the recipes' weights on each path (ANY: FR_W) */
static const uint8_t FR_W_ROUGH[FR_N] = {0, 1, 3, 1, 6, 1, 2, 1, 6, 4, 6, 6, 6, 5, 2, 6, 1, 6, 6, 1, 3, 6, 1, 6};
static const uint8_t FR_W_DARK[FR_N] = {6, 5, 1, 4, 1, 6, 4, 5, 0, 0, 2, 0, 0, 0, 6, 0, 6, 0, 0, 4, 4, 0, 6, 0};
/* an effect's value v (0 .. 100 within lo .. hi) leant the path's way */
static int32_t fx_lean(uint32_t role, int32_t v, int32_t lo, int32_t hi)
{
    if (draw_path == DP_DARK)
        switch (role) {
        case VR_DRIVE: return lo + (v - lo) * 2 / 5;
        case VR_TONE: return lo + (v - lo) * 3 / 5;
        case VR_SPACE: return v + (hi - v) / 3;
        case VR_DAMP: return v + (hi - v) / 2;
        case VR_BREAK: return lo + (v - lo) / 3;
        case VR_BITS: return v + (hi - v) / 2;
        default: return v;
        }
    if (draw_path == DP_ROUGH)
        switch (role) {
        case VR_DRIVE: return v + (hi - v) / 2;
        case VR_TONE: return v + (hi - v) / 4;
        case VR_SPACE: return lo + (v - lo) * 3 / 4;
        case VR_BREAK: return v + (hi - v) / 3;
        case VR_BITS: return lo + (v - lo) / 2;
        default: return v;
        }
    return v;
}
/* an effect's values: each at its home one time in five, else moved by 15 .. 55 either way (Vascular's); leant the
 * path's way */
static void fx_values(fx_eff_t *e, rnd_t *r)
{
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        int32_t v = RM_HOME[e->kind][k];
        if (!rnd_chance(r, 20)) {
            int32_t d = rnd_in(r, 15, 55);
            v += rnd_chance(r, 50) ? d : -d;
        }
        v = clampi(v, RM_LO[e->kind][k], RM_HI[e->kind][k]);
        e->p[k] = (uint8_t)clampi(fx_lean(RM_ROLE[e->kind][k], v, RM_LO[e->kind][k], RM_HI[e->kind][k]),
                                  RM_LO[e->kind][k], RM_HI[e->kind][k]);
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
    f->recipe = (uint8_t)(recipe >= 0 && recipe < FR_N ? recipe
                          : (int32_t)rnd_weighted(r, draw_path == DP_ROUGH ? FR_W_ROUGH : draw_path == DP_DARK ? FR_W_DARK : FR_W,
                                                  FR_N));
    for (k = 0; k < FX_N; k++) {
        uint32_t can = FR_PLACE[f->recipe][k], m, nc = 0;   /* (a bit an effect, FX_OPT the top one) */
        uint8_t c[NKIND];
        if (!(can & ~FX_OPT) || ((can & FX_OPT) && !rnd_chance(r, 55)))
            break;
        if (draw_path == DP_DARK && (can & ~FX_OPT & ~RM_HARSH))
            can &= ~RM_HARSH;                        /* (dark: the harsh ones left out where there is another) */
        else if (draw_path == DP_ROUGH && (can & RM_HARSH) && rnd_chance(r, 67))
            can = (can & FX_OPT) | (can & RM_HARSH);  /* (abrasive: the harsh ones mostly) */
        for (m = 0; m < NKIND; m++)
            if (((can >> m) & 1u) && rm_size(m) <= room)
                c[nc++] = (uint8_t)m;
        if (!nc && (can & FX_OPT))                   /* (none fits a place that may be left out: left out) */
            break;
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
    (void)s;                                     /* (lanes: no cable across to another lane) */
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
/* a snapshot's morph at its end: each CLOUD of slot s takes the sound now anew (frozen, a cloud swapped in during the
 * morph held what the generators made while they were still crossing over -- the snapshot before in it, for ever) */
static void fx_refill(uint32_t s)
{
    const fx_patch_t *f = &fbuf[s][f_use[s]];
    uint32_t k;
    if (f_pub[s] != f_use[s] || fx_phase[s] != FS_ON)
        return;
    for (k = 0; k < f->n && k < FX_N; k++)
        if (f->e[k].kind == RM_CLD && fx_st[s * FX_N + k])
            ((m_cld_t *)fx_st[s * FX_N + k])->fresh = CLD_N;
}
/* SHIFT + the first black key on effect slot s: every effect's values drawn anew (the path's way), its effects, outs
 * and cables as they are -- changed in place, as a rewiring (no fade, nothing cleared: the sound goes on, moved). 0 if
 * a new patch is still on its way in */
static int fx_revalue(uint32_t s, uint32_t seed)
{
    fx_patch_t *f;
    fx_eff_t e;
    rnd_t r;
    uint32_t k;
    if (f_pub[s] != f_use[s] || fx_phase[s] != FS_ON)
        return 0;
    f = &fbuf[s][f_use[s]];
    rnd_seed(&r, seed ^ 0x46585641u);
    for (k = 0; k < f->n && k < FX_N; k++) {
        e = f->e[k];
        fx_values(&e, &r);
        memcpy(f->e[k].p, e.p, sizeof e.p);
    }
    RING_PUBLISH();
    return 1;
}
/* a slot's patch as a session or a snapshot kept it, published as a draw is (each value checked; a patch too large for
 * the slot's memory as it is now loses its last effects, then FILTER stands in) */
/* build 31: DELAY and REVERB are the master's; a patch saved with them in a slot (a session, a snapshot) takes TAPE
 * for the delay, SPRING for the reverb, at their home values */
static void fx_retire(fx_patch_t *f)
{
    uint32_t k, j;
    for (k = 0; k < FX_N; k++)
        if (f->e[k].kind == RM_DLY || f->e[k].kind == RM_REV) {
            f->e[k].kind = f->e[k].kind == RM_DLY ? RM_TAP : RM_SPR;
            for (j = 0; j < 4u; j++)
                f->e[k].p[j] = RM_HOME[f->e[k].kind][j];
        }
}
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
    fx_retire(f);
    f->n = (uint8_t)clampi(f->n, 2, FX_N);
    f->recipe = (uint8_t)(f->recipe < FR_N ? f->recipe : 0);
    for (k = 0; k < FX_N; k++) {
        if (k >= 2u && k < f->n && f->e[k].kind < NKIND && rm_size(f->e[k].kind) > room)
            f->n = (uint8_t)k;                       /* (a third that no longer fits: left out, build 31) */
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
        f->xto[k] = 0;                           /* (lanes: no cable across, build 36) */
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
    memset(ebus_mem, 0, sizeof ebus_mem);
    for (s = 0; s < NFX; s++) {                      /* (KNOB 2's mixes, at power-on) */
        fx_wet[s] = FX_WET_DEF;
        fx_wq[s] = FX_WET_DEF * Q15 / 100;
    }
    for (s = 0; s < NFX; s++) {
        f_pub[s] = f_use[s] = 0;
        fx_phase[s] = FS_ON;
        fx_mute[s] = 0;
        fx_lvl[s] = 20 - 2;
        fx_pan[s] = LANE_PAN[s];
        fx_g[s] = fx_gl[s] = fx_gr[s] = 0;
        fx_gate[s] = Q15;
        fx_idle[s] = 0;
        fx_dcx[s] = fx_dcy[s] = 0;
        fx_draw(s, -1, seed + s * 7919u);            /* (published to buffer 1) */
        f_use[s] = f_pub[s];                         /* .. and taken at once: the memory is clear */
        fx_start(s);
    }
}
