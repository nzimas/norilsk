/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR: the processors -- the nodes a patch puts after its generator, in a chain. Each is a kind and four
 * values 0 .. 100 whose meaning is the kind's, drawn when the patch is made (patch.c) and fixed while it plays.
 * Mono, in place, a block at a time, in the order the chain has them; the reverb alone is stereo and comes last
 * (patch.c pans the chain's sound and adds the reverb's). The delay and the reverb keep one line each (44 KiB
 * and 43 KiB in the pool), so a chain has at most one of each.
 *
 * The kinds, after the processors of Vascular that port to this chip and are heard (the survey in docs/):
 *   FILTER   a resonant state-variable filter, low-, band- or high-pass; a slow sine on its cutoff if asked
 *   CRUSH    fewer bits (16 down to 2) and a lower sample rate (44.1 kHz down to ~300 Hz), as Tonitor's crush
 *   DRIVE    Tonitor's distortion: two valve-like stages at twice the rate, the level brought back
 *   FOLD     a wavefolder (Vascular's fold): the wave driven into a window and reflected back, twice
 *   RING     ring modulation by a sine, 5 Hz .. 2 kHz, wobbling (Vascular's sideband at its middle)
 *   COMB     a tuned feedback delay, 0.4 .. 50 ms, damped: a pitched ring on anything (Vascular's comb)
 *   FLANGE   a swept short delay with feedback (Vascular's flanger)
 *   DELAY    Tonitor's delay: 20 ms .. 2 s, feedback, damping, mu-law line at FS / 2
 *   REVERB   Tonitor's Dattorro plate at FS / 2
 * Each has a character (Vascular's): a chain never has two of one character, nor two destructive kinds, and the
 * reverb comes last (patch.c). */

enum { PK_NONE, PK_FILTER, PK_CRUSH, PK_DRIVE, PK_FOLD, PK_RING, PK_COMB, PK_FLANGE, PK_DELAY, PK_REVERB, PK_N };
enum { CH_NONE, CH_SPECTRAL, CH_DESTRUCTIVE, CH_MODULATION, CH_RESONANT, CH_TEMPORAL, CH_SPACE };
static const char *const PK_NAME[PK_N] = {"", "FILTER", "CRUSH", "DRIVE", "FOLD", "RING", "COMB", "FLANGE", "DELAY", "REVERB"};
static const char *const PK_SHORT[PK_N] = {"", "FLT", "CRU", "DRV", "FLD", "RNG", "CMB", "FLG", "DLY", "REV"};
static const uint8_t PK_CHAR[PK_N] = {CH_NONE, CH_SPECTRAL, CH_DESTRUCTIVE, CH_DESTRUCTIVE, CH_DESTRUCTIVE, CH_MODULATION,
                                      CH_RESONANT, CH_MODULATION, CH_TEMPORAL, CH_SPACE};
static const uint8_t PK_HARSH[PK_N] = {0, 0, 2, 2, 2, 1, 1, 0, 0, 0};   /* what each costs of the patch's harshness */
#define NODE_P 4
typedef struct {
    uint8_t kind;
    uint8_t p[NODE_P];           /* FILTER: type (0 .. 33 LP, 34 .. 66 BP, 67 .. 100 HP), cutoff, resonance, wobble
                                  * CRUSH: bits, rate; DRIVE: amount; FOLD: drive, window, second fold, tone;
                                  * RING: frequency, mix, wobble; COMB: pitch, decay, damp, wobble;
                                  * FLANGE: delay, rate, feedback, depth; DELAY: time, feedback, damp, mix;
                                  * REVERB: size, decay, damp, mix */
} node_t;

/* ---------------------------------------------------------------- FILTER --- */
typedef struct { svf_t f; uint32_t lfo; } st_filter_t;
static void flt_block(const node_t *nd, st_filter_t *s, int32_t *x)
{
    int32_t oct = 321600 + nd->p[1] * (9 * 65536 / 100), hz, q, type;   /* 30 Hz up 9 octaves (log2 30 = 4.907) */
    uint32_t i;
    svf_c c;
    if (nd->p[3]) {                                  /* the wobble: a sine of 0.1 .. 0.5 Hz, up to +-2 octaves */
        s->lfo += 310000u + (uint32_t)nd->p[3] * 12000u;
        oct += mulq15(sin_ph(s->lfo), nd->p[3] * (2 * 65536 / 100));
    }
    hz = (int32_t)(exp2_q16(clampi(oct, 4 << 16, 14 << 16)) >> 16);
    q = 2896 + nd->p[2] * (10 * 4096 - 2896) / 100;
    svf_coef(&c, clampi(hz, 30, 16000), q);
    type = nd->p[0] < 34 ? 1 : nd->p[0] < 67 ? 2 : 3;
    for (i = 0; i < CTL; i++)
        x[i] = svf_run(&s->f, &c, x[i], type);
}

/* ----------------------------------------------------------------- CRUSH --- */
typedef struct { uint32_t ph; int32_t hold; } st_crush_t;
static void crush_block(const node_t *nd, st_crush_t *s, int32_t *x)
{
    uint32_t i;
    if (nd->p[0]) {                                  /* fewer bits: the step, 2^(16 - bits), 16 .. 2 bits */
        uint32_t step = exp2_q16(nd->p[0] * (14 * 65536 / 100)) >> 16, inv;
        if (step >= 2u) {
            inv = 0xFFFFFFFFu / step;
            for (i = 0; i < CTL; i++) {
                int32_t q = (int32_t)(((int64_t)x[i] * inv + 0x80000000LL) >> 32);
                x[i] = q * (int32_t)step;
            }
        }
    }
    if (nd->p[1]) {                                  /* a lower rate: each sample held (2^(-7.2 p / 100): to ~300 Hz) */
        uint32_t r = exp2_q16(-(nd->p[1] * 7 * 65536 / 100 + nd->p[1] * 13107 / 100)), inc;
        inc = r >= 65535u ? 0xFFFFFFFFu : r << 16;
        for (i = 0; i < CTL; i++) {
            uint32_t was = s->ph;
            s->ph += inc;
            if (s->ph < was || inc == 0xFFFFFFFFu)
                s->hold = x[i];
            x[i] = s->hold;
        }
    }
}

/* ----------------------------------------------------------------- DRIVE --- */
/* Tonitor's distortion: two gain stages in series, as in a valve preamp: the first symmetric (odd harmonics), the
 * second asymmetric (it clips sooner on one side: even harmonics), both at twice the sample rate (linear
 * interpolation up, a [1/4 1/2 1/4] half-band down: fewer aliases). The highs are lifted 6 dB before the stages
 * and lowered as much after; then a DC blocker and a low-pass that comes down from 16 kHz to 6.5 kHz as the
 * amount goes up. The level is brought back to the dry signal's: the mean squares of both followed (~0.2 s), the
 * wet scaled by the square root of their ratio */
typedef struct {
    int32_t amt;                 /* the amount as it glides, x 256 */
    int32_t pre, post;           /* the emphasis's low-passes (700 Hz), before and after */
    int32_t dc;                  /* the DC blocker's low-pass, x 256 */
    int32_t lp;                  /* the tone's low-pass */
    int32_t env;                 /* the emphasised input's peak, falling over ~0.1 s (the bias follows it) */
    int32_t ep, ymp, yp, dryp;
    int64_t ein, eout;           /* the mean squares, dry and wet, as they follow (x 256) */
    int32_t g, gp;               /* the level brought back, Q16: wanted, and as the last block ended (it glides) */
} dist_t;
#define DS_EMPH 3110             /* 1 - e^(-2 pi 700 / FS), Q15 */
AINL int32_t ds_asym(int32_t u)  /* the first stage's curve: tanh, clipping at 5/8 below */
{
    return u >= 0 ? tanh_q15(u) : (tanh_q15((u * 8) / 5) * 5) / 8;
}
AINL int32_t ds_stage(int32_t e, int32_t g1, int32_t b, int32_t fb, int32_t g2)   /* the two stages (e biased) */
{
    int32_t u = tanh_q15((int32_t)(((int64_t)e * g1) >> 12));
    return ds_asym(mulq12(u, g2) + b) - fb;
}
static int32_t ds_log2(int64_t v)            /* log2 of a mean square, Q16 */
{
    int32_t e = 0;
    while (v > 0x7FFFFFFFLL) {
        v >>= 1;
        e++;
    }
    return log2_q16((uint32_t)(v > 0 ? v : 1)) + (e << 16);
}
static void dist_block(dist_t *d, int32_t *x, uint32_t n, int32_t want)
{
    int32_t a, g1, b, fb, g2, tone, w, beta, g0 = d->gp, gs, i;
    int64_t sin = 0, sout = 0;
    d->amt += ((want << 8) - d->amt) >> 2;
    if (!want && d->amt < 64) {                  /* (off: from silence when it comes in again) */
        memset(d, 0, sizeof *d);
        d->g = d->gp = Q16;
        return;
    }
    a = d->amt >> 8;
    g1 = (int32_t)((2048u * (uint64_t)exp2_q16(d->amt * 6 / 100 * 256)) >> 16);   /* x 0.5 .. x 32, Q12 */
    b = 3277 + d->amt * 6554 / 25600;            /* the bias, 0.1 .. 0.3 */
    fb = ds_asym(b);
    g2 = 4096 + d->amt * 6144 / 25600;           /* x 1 .. x 2.5, Q12 */
    beta = 4915 + d->amt * 6554 / 25600;         /* the first stage's bias: 0.15 .. 0.35 of the input's peak */
    tone = 29430 - d->amt * (29430 - 19800) / 25600;   /* 16 kHz .. 6.5 kHz */
    w = a >= 6 ? Q15 : d->amt * Q15 / (6 * 256);
    gs = (d->g - g0) / (int32_t)n;
    for (i = 0; i < (int32_t)n; i++) {
        int32_t dry = x[i], e, ym, y, o;
        d->pre += mulq15(dry - d->pre, DS_EMPH);
        e = 2 * dry - d->pre;                    /* the highs +6 dB */
        d->env = absi(e) > d->env ? absi(e) : d->env - (d->env >> 12);
        e += mulq15(d->env, beta);
        ym = ds_stage((d->ep + e) >> 1, g1, b, fb, g2);
        y = ds_stage(e, g1, b, fb, g2);
        o = (d->ymp + 2 * d->yp + ym) >> 2;      /* (one sample late) */
        d->ep = e;
        d->ymp = ym;
        d->yp = y;
        d->post += mulq15(o - d->post, DS_EMPH);
        o = (o + d->post) >> 1;                  /* the highs -6 dB again */
        d->dc += (((o << 8) - d->dc) * 37) >> 15;   /* (~8 Hz) */
        o -= d->dc >> 8;
        d->lp += mulq15(o - d->lp, tone);
        o = d->lp;
        sin += (int64_t)d->dryp * d->dryp;
        sout += (int64_t)o * o;
        o = (int32_t)(((int64_t)o * (g0 + gs * (i + 1))) >> 16);
        x[i] = d->dryp + mulq15(o - d->dryp, w);
        d->dryp = dry;
    }
    d->gp = d->g;
    d->ein += (sin - d->ein) >> 8;
    d->eout += (sout - d->eout) >> 8;
    if (d->ein > 32 * 1000 && d->eout > 32 * 100)   /* (sounding: else the level as it was) */
        d->g = (int32_t)exp2_q16(clampi((ds_log2(d->ein) - ds_log2(d->eout)) / 2, -(5 << 16), 1 << 16));   /* x 1/32 .. x 2 */
}

/* ------------------------------------------------------------------ FOLD --- */
/* the wave driven (x 0.5 .. x 18) into a window (+-0.08 .. +-0.58 of full scale) and reflected back into it, then a
 * second fold at a gain of its own (x 1 .. x 5), then a low-pass (300 Hz .. 16 kHz); the level brought back by the
 * window's size */
typedef struct { int32_t lp; } st_fold_t;
AINL int32_t fold_into(int32_t x, int32_t w)      /* reflected into -w .. w */
{
    int32_t p = 4 * w;
    if (x > w || x < -w) {
        x = ((x + w) % p + p) % p;                     /* 0 .. 4w */
        x = x < 2 * w ? x - w : 3 * w - x;
    }
    return x;
}
static void fold_block(const node_t *nd, st_fold_t *s, int32_t *x)
{
    int32_t g1 = (int32_t)(exp2_q16((nd->p[0] * (5 * 65536 / 100)) - 65536) >> 4),   /* x 0.5 .. x 16, Q12 */
            w = 2621 + nd->p[1] * (19005 - 2621) / 100,                                 /* 0.08 .. 0.58 */
            g2 = 4096 + nd->p[2] * (4 * 4096) / 100,                                    /* x 1 .. x 5, Q12 */
            tone = 1400 + nd->p[3] * (29430 - 1400) / 100,                              /* 300 Hz .. 16 kHz */
            back = (int32_t)((Q15 * 20000u) / (uint32_t)w);                             /* 0.6 / w, Q15 */
    uint32_t i;
    for (i = 0; i < CTL; i++) {
        int32_t y = fold_into(mulq12(x[i], g1), w);
        y = fold_into(mulq12(y, g2), w);
        y = mulq15(y, back > 4 * Q15 ? 4 * Q15 : back);
        s->lp += mulq15(y - s->lp, tone);
        x[i] = s->lp;
    }
}

/* ------------------------------------------------------------------ RING --- */
/* the sound times a sine of 5 Hz .. 2 kHz (log), the sine's frequency wobbling by a slow sine when asked; mixed */
typedef struct { uint32_t ph, lfo; } st_ring_t;
static void ring_block(const node_t *nd, st_ring_t *s, int32_t *x)
{
    int32_t oct = 152000 + nd->p[0] * (8 * 65536 / 100 + 3932);                        /* log2 5 = 2.32, up 8.6 octaves */
    int32_t mix = nd->p[1] * (Q15 / 100), hz8;
    uint32_t inc, i;
    if (nd->p[2]) {
        s->lfo += 200000u + (uint32_t)nd->p[2] * 6000u;
        oct += mulq15(sin_ph(s->lfo), nd->p[2] * (65536 / 100));                         /* +-1 octave */
    }
    hz8 = (int32_t)(exp2_q16(clampi(oct, 2 << 16, 11 << 16)) >> 8);                     /* Hz Q8 */
    inc = hz8_inc(hz8);
    for (i = 0; i < CTL; i++) {
        int32_t m;
        s->ph += inc;
        m = mulq15(x[i], sin_ph(s->ph));
        x[i] += mulq15(m - x[i], mix);
    }
}

/* ------------------------------------------------------------------ COMB --- */
/* a feedback delay of 0.4 .. 50 ms (log), its loop gain 0.2 .. 0.93, a one-pole low-pass in the loop (300 Hz .. 20
 * kHz); its length wobbles by a slow sine, up to half a semitone */
#define CMB_N 2304u              /* 52 ms */
typedef struct { int16_t line[CMB_N]; uint32_t w, lfo; int32_t lp; } st_comb_t;
static void comb_block(const node_t *nd, st_comb_t *s, int32_t *x)
{
    int32_t ms16 = (int32_t)((exp2_q16(nd->p[0] * (7 * 65536 / 100)) * 4u) >> 10) * 11;   /* 0.4 .. 51 ms, x 1024 .. (Q10 of ms x 44.1 -> samples Q8) */
    int32_t g = 6554 + nd->p[1] * (30474 - 6554) / 100, damp = 1400 + nd->p[2] * (31000 - 1400) / 100;
    int32_t len8;
    uint32_t i;
    s->lfo += 60000u + (uint32_t)nd->p[3] * 2000u;
    len8 = ms16 + (nd->p[3] ? mulq15(sin_ph(s->lfo), ms16 * nd->p[3] / 3500) : 0);   /* (up to +-3 %) */
    len8 = clampi(len8, 8 * 256, (int32_t)(CMB_N - 4u) * 256);
    for (i = 0; i < CTL; i++) {
        uint32_t p = (s->w << 8) + (CMB_N << 8) - (uint32_t)len8, i0 = (p >> 8) % CMB_N, i1 = i0 + 1u >= CMB_N ? 0u : i0 + 1u;
        int32_t f = (int32_t)(p & 255u), y = s->line[i0];
        y += ((s->line[i1] - y) * f) >> 8;
        s->lp += mulq15(y - s->lp, damp);
        y = x[i] + mulq15(s->lp, g);
        s->line[s->w] = (int16_t)clampi(y, -32767, 32767);
        if (++s->w >= CMB_N)
            s->w = 0;
        x[i] = y;
    }
}

/* ---------------------------------------------------------------- FLANGE --- */
/* a short delay (0.1 .. 19 ms about its centre) swept by a sine of 0.02 .. 10 Hz, with feedback up to 0.88; the
 * delayed sound added to the dry */
#define FLG_N 1024u              /* 23 ms */
typedef struct { int16_t line[FLG_N]; uint32_t w, lfo; } st_flange_t;
static void flange_block(const node_t *nd, st_flange_t *s, int32_t *x)
{
    int32_t c8 = 32 + nd->p[0] * (838 * 256 - 32) / 100;                             /* 0.12 .. 19 ms, samples Q8 */
    int32_t depth = nd->p[3] * (Q15 / 100), fb = nd->p[2] * (28836 / 100);
    uint32_t rate = (uint32_t)(exp2_q16(nd->p[1] * (9 * 65536 / 100)) * 1948u >> 16); /* 0.02 .. 10 Hz, a sample's step */
    uint32_t i;
    for (i = 0; i < CTL; i++) {
        int32_t d8, y, f;
        uint32_t p, i0, i1;
        s->lfo += rate;
        d8 = c8 + mulq15(mulq15(sin_ph(s->lfo), depth), c8 - 24);
        d8 = clampi(d8, 24, (int32_t)(FLG_N - 4u) * 256);
        p = (s->w << 8) + (FLG_N << 8) - (uint32_t)d8;
        i0 = (p >> 8) % FLG_N;
        i1 = i0 + 1u >= FLG_N ? 0u : i0 + 1u;
        f = (int32_t)(p & 255u);
        y = s->line[i0];
        y += ((s->line[i1] - y) * f) >> 8;
        s->line[s->w] = (int16_t)clampi(x[i] + mulq15(y, fb), -32767, 32767);
        if (++s->w >= FLG_N)
            s->w = 0;
        x[i] = (x[i] + y) >> 1;
    }
}

/* ----------------------------------------------------------------- DELAY --- */
/* Tonitor's: a line of 2 s at FS / 2 (22.05 kHz), 8-bit mu-law, 44 100 bytes in the pool. Each echo through a
 * one-pole low-pass (DAMP) and the feedback (at most 0.95). TIME 20 ms .. 2 s on a log scale; a change of it glides
 * (~0.1 s), so the echoes bend in pitch rather than click */
#define DL_N 44100u                     /* 2 s at FS / 2 */
static uint8_t dl_a[DL_N] POOL;
static struct {
    uint32_t w;                         /* where the next sample is written */
    int32_t t;                          /* the delay now, Q8 samples at FS / 2 (it glides to dl_t_want) */
    int32_t lp;                         /* the damping's state */
    int32_t prev;                       /* the last echo, for the line drawn to the next */
} dl;
static volatile int32_t dl_t_want = 400 * 22 * 256, dl_fb, dl_damp = Q15, dl_mix;

AINL int32_t dl_get(uint32_t i) { return gr_dec(dl_a[i]); }
AINL void dl_put(uint32_t i, int32_t x)
{
    dl_a[i] = x > -4 && x < 4 ? 0u : gr_enc(x);   /* (silence as silence: the code's first step is 6, not 0) */
}
/* TIME as ms: 20 x 100^(v / 100) */
static int32_t dl_ms(int32_t v) { return (int32_t)((20u * exp2_q16(clampi(v, 0, 100) * 4354 + (clampi(v, 0, 100) * 12) / 100)) >> 16); }
static void dl_param(int32_t time, int32_t fb, int32_t damp, int32_t mix)   /* each 0..100 */
{
    int32_t ms = dl_ms(time);
    dl_t_want = clampi(ms * 22 * 256 + ms * 13, 2 * 256, (int32_t)(DL_N - 4u) * 256);   /* x 22.05 */
    dl_fb = clampi(fb, 0, 100) * (LV(0.95) / 100);
    dl_damp = Q15 - clampi(damp, 0, 100) * (LV(0.88) / 100);   /* (the low-pass's coefficient: 1 none .. 0.12) */
    dl_mix = clampi(mix, 0, 100) * (Q15 / 100);
}
static void dl_init(void)
{
    memset(dl_a, 0x80, sizeof dl_a);    /* (mu-law's zero is not 0x00: 0x80 is -0, silence either way) */
    memset(&dl, 0, sizeof dl);
    dl.t = dl_t_want;
}
/* a block, mono, in place: the echoes added */
static FAST void dl_block(int32_t *x)
{
    uint32_t i;
    int32_t fb = dl_fb, damp = dl_damp, mix = dl_mix, tw = dl_t_want;
    for (i = 0; i + 1u < CTL; i += 2u) {
        int32_t in = (x[i] + x[i + 1u]) >> 1, y, o0, wv;
        uint32_t p, i0, f;
        dl.t += (tw - dl.t) >> 11;                       /* (the glide) */
        p = (dl.w << 8) + (DL_N << 8) - (uint32_t)dl.t;
        i0 = (p >> 8) % DL_N;
        f = p & 255u;
        y = dl_get(i0);
        y += ((dl_get(i0 + 1u >= DL_N ? 0u : i0 + 1u) - y) * (int32_t)f) >> 8;
        dl.lp += mulq15(y - dl.lp, damp);
        wv = clampi(in + mulq15(dl.lp, fb), -32767, 32767);
        dl_put(dl.w, wv);
        if (++dl.w >= DL_N)
            dl.w = 0;
        y = mulq15(dl.lp, mix);
        o0 = (dl.prev + y) >> 1;                         /* drawn out to the full rate: half-way, then the echo */
        dl.prev = y;
        x[i] += o0;
        x[i + 1u] += y;
    }
}
static __typeof__(&dl_block) volatile dl_block_fp = dl_block;   /* (the flash's way into the RAM code) */

/* ---------------------------------------------------------------- REVERB --- */
#include "reverb.c"

/* ------------------------------------------------------------- the chain --- */
typedef union { st_filter_t flt; st_crush_t cr; dist_t ds; st_fold_t fo; st_ring_t rg; st_comb_t cb; st_flange_t fl; } nstate_t;

/* a node's parameters into the singletons (at the patch's start) */
static void node_cook(const node_t *nd)
{
    if (nd->kind == PK_DELAY)
        dl_param(nd->p[0], nd->p[1], nd->p[2], nd->p[3]);
    else if (nd->kind == PK_REVERB)
        rv_param(nd->p[0], nd->p[1], nd->p[2], nd->p[3]);
}
/* one block of a mono node over x (the reverb: nothing here, patch.c adds it after the pan) */
static void node_block(const node_t *nd, nstate_t *st, int32_t *x)
{
    switch (nd->kind) {
    case PK_FILTER: flt_block(nd, &st->flt, x); break;
    case PK_CRUSH: crush_block(nd, &st->cr, x); break;
    case PK_DRIVE: dist_block(&st->ds, x, CTL, nd->p[0]); break;
    case PK_FOLD: fold_block(nd, &st->fo, x); break;
    case PK_RING: ring_block(nd, &st->rg, x); break;
    case PK_COMB: comb_block(nd, &st->cb, x); break;
    case PK_FLANGE: flange_block(nd, &st->fl, x); break;
    case PK_DELAY: dl_block_fp(x); break;
    default: break;
    }
}
static void nodes_init(void)
{
    dl_init();
    rv_init();
}
