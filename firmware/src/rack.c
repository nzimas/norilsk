/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR: the rack -- twelve processor modules, always there, in a fixed order that is also the signal's: a
 * slot's patch wires its generator into one, two or three of them, in rack order (slots.c), and the wiring of all
 * the slots together is the rack's patch. A module fed by several slots hears their sum; a module's output goes on
 * to the next module of every chain that passes through it, and out to the master where a chain ends there -- as
 * a cable and a mult would on a modular. The order keeps it feed-forward: no loop can form between modules.
 *
 * Where a module sits in the rack is where it sounds: the four columns are four places in the stereo image, left
 * to right (the plate is stereo of its own). Each module has four values 0 .. 100, the module's own (SEQ draws them
 * anew for the whole rack), and its output passes a guard (a level rider and a ceiling, as Vascular's every
 * processor) so a resonance or a feedback never pins the bus.
 *
 * The twelve, after the survey of Vascular's processors (docs/PROCESSORS.md):
 *   FLT  FILTER    a resonant state-variable filter, low-, band- or high-pass, its cutoff wobbling if asked
 *   FLD  FOLD      a wavefolder: the wave driven into a window and reflected back, twice
 *   MU   MULAW     the telephone: a lower rate, mu-law with fewer bits, filtered before and after
 *   SHF  SHIFT     a frequency shifter (Hilbert pair and a quadrature sine): every partial moved by the same
 *                  hertz, up or down, or both (ring modulation) at the middle; feedback barber-poles it
 *   MDL  MODAL     three resonators tuned to the scale's root: struck metal, bars, bells
 *   STR  STRING    three Karplus strings tuned to the root, detuned: everything that passes rings
 *   VOW  VOWEL     three formants morphing a, e, i, o, u
 *   FLG  FLANGE    a swept short delay with feedback
 *   TAP  TAPE      a tape echo: wow, saturation, age (8-bit line at half the rate)
 *   RVS  REVERSE   windows of the sound played backwards, an octave up beside them if asked
 *   SPR  SPRING    a spring: allpass chirps in a short tank, a resonance for its tone
 *   PLT  PLATE     Tonitor's Dattorro plate, stereo */

enum { RM_FLT, RM_FLD, RM_MU, RM_SHF, RM_MDL, RM_STR, RM_VOW, RM_FLG, RM_TAP, RM_RVS, RM_SPR, RM_PLT, NMOD };
static const char *const RM_SHORT[NMOD] = {"FLT", "FLD", "MU", "SHF", "MDL", "STR", "VOW", "FLG", "TAP", "RVS", "SPR", "PLT"};
static const char *const RM_NAME[NMOD] = {"FILTER", "FOLD", "MULAW", "SHIFT", "MODAL", "STRING", "VOWEL", "FLANGE",
                                          "TAPE", "REVERSE", "SPRING", "PLATE"};
/* each module's character (Vascular's): a chain never has two of one, nor two destructive ones */
enum { CH_SPECTRAL, CH_DESTRUCTIVE, CH_MODULATION, CH_RESONANT, CH_TEMPORAL, CH_SPACE };
static const uint8_t RM_CHAR[NMOD] = {CH_SPECTRAL, CH_DESTRUCTIVE, CH_DESTRUCTIVE, CH_MODULATION, CH_RESONANT,
                                      CH_RESONANT, CH_SPECTRAL, CH_MODULATION, CH_TEMPORAL, CH_TEMPORAL, CH_SPACE, CH_SPACE};
static const uint8_t RM_HARSH[NMOD] = {0, 2, 2, 1, 0, 0, 0, 0, 0, 1, 0, 0};   /* what each costs of a patch's budget */
static const uint8_t RM_W[NMOD] = {12, 4, 5, 7, 11, 10, 9, 10, 11, 9, 9, 11};   /* Vascular's foundation weights, x 10 */
/* the values a module comes back to (SEQ draws around them, as Vascular draws a processor's) */
static const uint8_t RM_HOME[NMOD][4] = {
    {20, 55, 40, 30}, {45, 45, 20, 70}, {45, 60, 50, 30}, {40, 30, 70, 20}, {40, 55, 40, 50}, {35, 60, 40, 60},
    {50, 50, 50, 30}, {25, 30, 50, 60}, {50, 30, 45, 40}, {40, 60, 30, 50}, {50, 40, 50, 50}, {65, 60, 40, 55}};
#define RM_COLS 4
static const int16_t RM_PAN[RM_COLS] = {-19661, -6554, 6554, 19661};   /* a column's place: -0.6 .. 0.6 */

static volatile uint8_t rack_p[NMOD][4];       /* the modules' values (main loop) */
static volatile int32_t rack_root = 48;        /* the scale's root, MIDI: what MODAL and STRING are tuned to */

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
    if (p[3] > 10) {                                    /* the wobble: 0.1 .. 0.5 Hz, up to +-2 octaves */
        s->lfo += 310000u + (uint32_t)p[3] * 12000u;
        oct += mulq15(sin_ph(s->lfo), p[3] * (2 * 65536 / 100));
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
 * p2 the direction: down below 45, up above 55, both (ring modulation) between; p3 a slow wobble of the shift */
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
    uint32_t inc, i, k;
    if (p[3] > 10) {
        s->lfo += 200000u + (uint32_t)p[3] * 6000u;
        oct += mulq15(sin_ph(s->lfo), p[3] * (65536 / 100));
    }
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
        s->lp += mulq15(s->line[r] - s->lp, 16000);     /* (the feedback's damping, ~4 kHz) */
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

/* ------------------------------------------------------------- FLANGE --- */
#define FLG_N 1024u              /* 23 ms */
typedef struct { int16_t line[FLG_N]; uint32_t w, lfo; } m_flg_t;
static void m_flg(const uint8_t *p, m_flg_t *s, int32_t *x)
{
    int32_t c8 = 32 + p[0] * (838 * 256 - 32) / 100, depth = p[3] * (Q15 / 100), fb = p[2] * (28836 / 100);
    uint32_t rate = (uint32_t)(exp2_q16(p[1] * (9 * 65536 / 100)) * 1948u >> 16), i;   /* 0.02 .. 10 Hz */
    for (i = 0; i < CTL; i++) {
        int32_t d8, y, f;
        uint32_t q, i0, i1;
        s->lfo += rate;
        d8 = c8 + mulq15(mulq15(sin_ph(s->lfo), depth), c8 - 24);
        d8 = clampi(d8, 24, (int32_t)(FLG_N - 4u) * 256);
        q = (s->w << 8) + (FLG_N << 8) - (uint32_t)d8;
        i0 = (q >> 8) % FLG_N;
        i1 = i0 + 1u >= FLG_N ? 0u : i0 + 1u;
        f = (int32_t)(q & 255u);
        y = s->line[i0] + (((s->line[i1] - s->line[i0]) * f) >> 8);
        s->line[s->w] = (int16_t)clampi(x[i] + mulq15(y, fb), -32767, 32767);
        if (++s->w >= FLG_N)
            s->w = 0;
        x[i] = (x[i] + y) >> 1;
    }
}

/* --------------------------------------------------------------- TAPE --- */
/* an echo on an 8-bit mu-law line at FS / 2: time 25 .. 900 ms (p0), wow (p1: a slow wander of the time, up to
 * +-3 %, and a flutter), feedback 0 .. 0.94 (p2) through the head's saturation, age (p3: the loop's low-pass from 12
 * kHz down to 800 Hz). The echoes alone come out */
#define TAP_N 19900u             /* 0.9 s at FS / 2 */
typedef struct { uint8_t line[TAP_N]; uint32_t w, wph, fph; int32_t t, lp, prev, wn, wt; rnd_t r; } m_tap_t;
static void m_tap(const uint8_t *p, m_tap_t *s, int32_t *x)
{
    int32_t want = (25 + p[0] * 875 / 100) * 2205 / 100 * 256;   /* samples at FS / 2, Q8 */
    int32_t fb = p[2] * (30802 / 100), age = 31000 - p[3] * (31000 - 3600) / 100, wow = p[1] * 10, mod;
    uint32_t i;
    if (!s->t)
        s->t = want;
    if (!(s->r.s))
        rnd_seed(&s->r, 0x7A9Eu);
    if ((rnd_next(&s->r) & 255u) < 3u)                 /* the wow: a new place every ~0.1 s, glided to */
        s->wt = mulq15(rnd_bi(&s->r), wow);
    s->wn += (s->wt - s->wn) >> 6;
    s->fph += 380000u + (uint32_t)p[3] * 6000u;          /* the flutter, ~9 .. 26 Hz a block */
    mod = s->wn + mulq15(sin_ph(s->fph), p[3] * 2);      /* (Q15 of the time: the wow up to +-3 %, the flutter 0.6 %) */
    for (i = 0; i + 1u < CTL; i += 2u) {
        int32_t in = (x[i] + x[i + 1u]) >> 1, y, o0, tt;
        uint32_t q, i0, f;
        s->t += (want - s->t) >> 10;
        tt = s->t + (int32_t)(((int64_t)s->t * mod) >> 15);
        tt = clampi(tt, 4 << 8, (int32_t)(TAP_N - 4u) << 8);
        q = (s->w << 8) + (TAP_N << 8) - (uint32_t)tt;
        i0 = (q >> 8) % TAP_N;
        f = q & 255u;
        y = mu_dec8(s->line[i0]);
        y += ((mu_dec8(s->line[i0 + 1u >= TAP_N ? 0u : i0 + 1u]) - y) * (int32_t)f) >> 8;
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
/* windows of 60 .. 400 ms (p0) read backwards: two heads half a window apart, each a triangle, so the sum stays
 * level; the read runs away from the write at a rate (p3: 1.5 .. 3; 2 = backwards at pitch); a third head at twice
 * the rate (an octave up, backwards) beside them (p2: its level); a low-pass on it all, 1.4 .. 13 kHz (p1). Its line:
 * 1.2 s at FS / 2, 8-bit mu-law */
#define RVS_N 26460u
typedef struct { uint8_t line[RVS_N]; uint32_t w, ph[3]; int32_t lp, prev; } m_rvs_t;
AINL int32_t rvs_head(const m_rvs_t *s, uint32_t ph, uint32_t span)   /* a head at phase ph (2^32 a window) */
{
    uint32_t d = (uint32_t)(((uint64_t)ph * span) >> 32) + 4u, q = s->w + RVS_N - d;
    int32_t tri = (int32_t)(ph >> 16), y;
    q = q >= RVS_N ? q - RVS_N : q;
    tri = tri < 32768 ? tri : 65535 - tri;              /* 0 .. 32767 .. 0 */
    y = mu_dec8(s->line[q]);
    return mulq15(y, tri);
}
static void m_rvs(const uint8_t *p, m_rvs_t *s, int32_t *x)
{
    uint32_t win = (60u + (uint32_t)p[0] * 340u / 100u) * 2205u / 100u;   /* samples at FS / 2 */
    uint32_t run16 = 98304u + (uint32_t)p[3] * 98304u / 100u;             /* 1.5 .. 3, Q16 */
    uint32_t span = (uint32_t)(((uint64_t)win * run16) >> 16), span2, inc = 0xFFFFFFFFu / (win ? win : 1u), i;
    int32_t oct = p[2] * (Q15 / 100), tone = 6400 + p[1] * (31000 - 6400) / 100;
    if (span > RVS_N - 8u)
        span = RVS_N - 8u;
    span2 = span * 3u / 4u;                              /* (read twice as fast: the delay grows 1.5 x the run -- an octave up) */
    for (i = 0; i + 1u < CTL; i += 2u) {
        int32_t y;
        s->line[s->w] = mu_enc8((x[i] + x[i + 1u]) >> 1);
        if (++s->w >= RVS_N)
            s->w = 0;
        s->ph[0] += inc;
        s->ph[1] = s->ph[0] + 0x80000000u;
        s->ph[2] += inc * 2u;
        y = rvs_head(s, s->ph[0], span) + rvs_head(s, s->ph[1], span);
        if (oct)
            y += mulq15(rvs_head(s, s->ph[2], span2), oct);
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
static m_flg_t m_flg_s POOL;
static m_tap_t m_tap_s POOL;
static m_rvs_t m_rvs_s POOL;
static m_spr_t m_spr_s POOL;
static int32_t rm_env[NMOD], rm_g[NMOD];       /* each module's guard: the peak it follows, its gain */
static volatile int32_t rm_meter[NMOD];        /* each module's output, its last peaks (the screen's) */
static uint16_t rm_quiet[NMOD];                /* blocks its output has been near silence (a tail let go) */
static uint8_t rm_plate_on;                    /* (the plate's parameters set: rv_param) */

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

/* one module over x (mono, in place); the plate: into l, r (added) */
static void rm_block(uint32_t m, int32_t *x, int32_t *l, int32_t *r)
{
    const uint8_t *p = (const uint8_t *)rack_p[m];
    switch (m) {
    case RM_FLT: m_flt(p, &m_flt_s, x); break;
    case RM_FLD: m_fld(p, &m_fld_s, x); break;
    case RM_MU: m_mu(p, &m_mu_s, x); break;
    case RM_SHF: m_shf(p, &m_shf_s, x); break;
    case RM_MDL: m_mdl(p, &m_mdl_s, x); break;
    case RM_STR: m_str(p, &m_str_s, x); break;
    case RM_VOW: m_vow(p, &m_vow_s, x); break;
    case RM_FLG: m_flg(p, &m_flg_s, x); break;
    case RM_TAP: m_tap(p, &m_tap_s, x); break;
    case RM_RVS: m_rvs(p, &m_rvs_s, x); break;
    case RM_SPR: m_spr(p, &m_spr_s, x); break;
    default: {                                       /* the plate: stereo, added to the master */
        int32_t pl[CTL], pr[CTL];
        uint32_t i;
        if (!rm_plate_on) {
            rv_param(p[0], p[1], p[2], 40);              /* (x 0.64: the tail sums over seconds) */
            rm_plate_on = 1;
        }
        for (i = 0; i < CTL; i++)
            pl[i] = pr[i] = 0;
        rv_block_fp(x, pl, pr, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t g = 8192 + p[3] * (24576 / 100);  /* (its level: 0.25 .. 1) */
            l[i] += mulq15(pl[i], g);
            r[i] += mulq15(pr[i], g);
            x[i] = (pl[i] + pr[i]) >> 1;               /* (mono, for what follows it: nothing does, it is last) */
        }
        {
            int32_t pk = 0;
            for (i = 0; i < CTL; i++)
                pk = maxi(pk, absi(x[i]));
            rm_meter[m] = pk > rm_meter[m] ? pk : rm_meter[m] - (rm_meter[m] >> 4);
        }
        return;
    }
    }
    rm_guard(m, x);
}

/* the modules' values drawn anew: each at its home one time in five, else moved by 15 .. 55 either way */
static void rack_draw(uint32_t seed)
{
    rnd_t r;
    uint32_t m, k;
    rnd_seed(&r, seed ^ 0x5241434Bu);
    for (m = 0; m < NMOD; m++)
        for (k = 0; k < 4u; k++) {
            int32_t v = RM_HOME[m][k];
            if (!rnd_chance(&r, 20)) {
                int32_t d = rnd_in(&r, 15, 55);
                v += rnd_chance(&r, 50) ? d : -d;
            }
            rack_p[m][k] = (uint8_t)clampi(v, 0, 100);
        }
    rm_plate_on = 0;                                 /* (the plate's values taken at its next block) */
}

static void rack_init(void)
{
    memset(&m_flt_s, 0, sizeof m_flt_s);
    memset(&m_fld_s, 0, sizeof m_fld_s);
    memset(&m_mu_s, 0, sizeof m_mu_s);
    memset(&m_shf_s, 0, sizeof m_shf_s);
    memset(&m_mdl_s, 0, sizeof m_mdl_s);
    memset(&m_str_s, 0, sizeof m_str_s);
    memset(&m_vow_s, 0, sizeof m_vow_s);
    memset(&m_flg_s, 0, sizeof m_flg_s);
    memset(&m_tap_s, 0, sizeof m_tap_s);
    memset(m_tap_s.line, 0x80, sizeof m_tap_s.line);   /* (mu-law's silence) */
    memset(&m_rvs_s, 0, sizeof m_rvs_s);
    memset(m_rvs_s.line, 0x80, sizeof m_rvs_s.line);
    memset(&m_spr_s, 0, sizeof m_spr_s);
    m_mdl_s.key = m_vow_s.key = m_spr_s.key = -1;
    memset(rm_env, 0, sizeof rm_env);
    {
        uint32_t m;
        for (m = 0; m < NMOD; m++) {
            rm_g[m] = Q15;
            rm_meter[m] = 0;
            rm_quiet[m] = 0xFFFFu;
        }
    }
    rv_init();
    rack_draw(0x4D4F4Cu);
}
