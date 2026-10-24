/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR (from Tonitor): the plate, as an effect's instance (rv_t: its lines inside it, so a plate may stand in
 * more than one effect slot, each in its slot's memory).
 *
 * A plate after Jon Dattorro, "Effect Design, Part 1: Reverberator and Other Filters" (J. Audio Eng. Soc.
 * 45(9), 1997): a predelay and a bandwidth filter, four allpasses that diffuse the input, then a tank of two
 * branches in a figure of eight -- each a modulated allpass, a long delay, a damping filter, the decay, a
 * second allpass and another delay, feeding the other branch -- read at fourteen taps for the two sides.
 * The delays are Dattorro's (at 29761 Hz) scaled to 22.05 kHz and by SIZE: the plate runs at half the rate
 * (each pair of samples in, averaged; out, drawn back up between the two) -- half the work and half the memory,
 * its top an octave lower than at 44.1 kHz (11 kHz, near Dattorro's own 14.9), its times and decay the same, the
 * filters set for the slower rate. The two modulated allpasses sweep slowly, out of step with each other, so
 * the tail never settles into a ring: that is its lushness.
 *
 * Mono in, stereo out. Fixed point: Q15 samples, the delay lines in 16 bits (34 KiB an instance), the coefficients Q15.
 *
 *   SIZE   the room: every delay 0.5 .. 1.0 times Dattorro's (MOLECULAR: 1.25 no more, for the memory)
 *   DECAY  how long it rings: 0.4 s .. 25 s to fall 60 dB (about 4 s in the middle), at any SIZE
 *   DAMP   how fast the highs die in the tank
 *   MIX    the reverb's return to the mix: x 0 .. 4, x 1 in the middle */

#define RV_F 24275               /* 22050 / 29761, Q15: the plate runs at FS / 2 */
#define RV_SMAX 4096             /* SIZE's largest scale, Q12 (1.0) */
#define RV_EXC 12                /* the modulated allpasses' excursion, samples (at FS / 2) */
#define RV_PRE 441               /* predelay, 20 ms */

/* Dattorro's lengths at 29761 Hz: the four input allpasses, then the tank (left: modulated allpass,
 * delay, allpass, delay; right: the same) */
static const uint16_t RV_DIF[4] = {142, 107, 379, 277};
static const uint16_t RV_TANK[8] = {672, 4453, 1800, 3720, 908, 4217, 2656, 3163};
static const int16_t RV_DIFG[4] = {24576, 24576, 20480, 20480};   /* 0.75, 0.75, 0.625, 0.625 */
/* the output taps: line (0..7 of the tank), offset at 29761 Hz, sign */
static const uint8_t RV_TAPL_LINE[7] = {5, 5, 6, 7, 1, 2, 3};
static const uint16_t RV_TAPL_OFF[7] = {266, 2974, 1913, 1996, 1990, 187, 1066};
static const uint8_t RV_TAPR_LINE[7] = {1, 1, 2, 3, 5, 6, 7};
static const uint16_t RV_TAPR_OFF[7] = {353, 3627, 1228, 2673, 2111, 335, 121};
static const int8_t RV_TAP_SIGN[7] = {1, 1, -1, 1, -1, -1, -1};

typedef struct {
    int16_t *buf;
    uint32_t len, w;             /* allocated length, write index */
} rv_line;

#define RV_MEM 17240u
typedef struct {
    rv_line pre, dif[4], tank[8];
    uint32_t tank_base[8];       /* each tank line's delay at SIZE 1.0, samples (22.05 kHz) */
    uint32_t tank_total;         /* their sum: a sound's trip round the tank at SIZE 1.0 */
    int32_t bw, damp_l, damp_r;  /* the filters' states */
    int32_t fb_l, fb_r;          /* each branch's output, into the other */
    int32_t pl, pr;              /* (the last output pair, the next drawn up from it) */
    uint32_t lfo, lfo2;          /* the modulation's phases, one a side */
    int32_t scale;               /* SIZE as it is now (Q12), gliding to scale_t */
    /* the parameters, from the main loop */
    volatile int32_t scale_t, decay, damp, mix;   /* Q12; Q15; Q15; Q15 */
    int ok;
    int16_t mem[RV_MEM];
} rv_t;

/* rounded: a floor's bias, round the tank, settles into a DC that never dies */
AINL int32_t rvm(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b + (1 << 14)) >> 15); }
static inline int16_t rv_sat(int32_t x) { return (int16_t)clampi(x, -32768, 32767); }
AINL void rv_write(rv_line *l, int32_t x)
{
    l->buf[l->w] = rv_sat(x);
    if (++l->w >= l->len)
        l->w = 0;
}
/* the sample written d samples ago (d < len) */
AINL int32_t rv_tap(const rv_line *l, uint32_t d)
{
    uint32_t i = l->w + l->len - 1u - d;
    if (i >= l->len)
        i -= l->len;
    return l->buf[i];
}
/* .. d + frac / 256 samples ago, linear */
AINL int32_t rv_tapf(const rv_line *l, uint32_t d, int32_t frac)
{
    int32_t a = rv_tap(l, d), b = rv_tap(l, d + 1u);
    return a + (((b - a) * frac) >> 8);
}
/* an allpass whose delay is the whole line (Dattorro's form: out = d - g w, w = x + g d) */
AINL int32_t rv_allpass(rv_line *l, int32_t x, int32_t g, uint32_t d)
{
    int32_t dl = rv_tap(l, d - 1u), w = x + rvm(g, dl);
    rv_write(l, w);
    return dl - rvm(g, w);
}

static void rv_param(rv_t *R, int32_t size, int32_t decay, int32_t damp, int32_t mix)   /* each 0..100 (main loop) */
{
    int32_t m = clampi(mix, 0, 100), e;
    uint32_t loop_q16;
    R->scale_t = 2048 + clampi(size, 0, 100) * 2048 / 100;           /* 0.5 .. 1.0 */
    /* RT60 = 0.4 s x 2^(6 decay / 100); a trip round the tank takes T = total x scale / FS, and loses
     * 60 dB x T / RT60 over two decay stages: each is 2^(-60 / 6.02 / 2 x T / RT60) = 2^(-12.46 x T x 2^(-6d) ) */
    loop_q16 = (uint32_t)(((uint64_t)R->tank_total * (uint32_t)R->scale_t * 194782u) >> 28);   /* T, Q16 s (FS / 2) */
    e = (int32_t)(((int64_t)loop_q16 * 816600) >> 16);                /* 12.46 T, Q16 */
    e = (int32_t)(((int64_t)e * exp2_q16(-(clampi(decay, 0, 100) * 65536 * 6 / 100))) >> 16);
    R->decay = clampi((int32_t)(exp2_q16(-e) >> 1), 1000, 31785);    /* Q15, at most 0.97 */
    R->damp = 1638 + clampi(damp, 0, 100) * 27853 / 100;             /* 0.05 .. 0.90 */
    R->mix = m * m * 131072 / 10000;                                  /* x m^2 x 4 */
}

/* (its lines are not cleared here: the effect slot's memory is, a piece a block, before it starts) */
static void rv_init(rv_t *R)
{
    uint32_t k, at = 0;
    rv_line *l;
    memset(R, 0, (uint32_t)((char *)R->mem - (char *)R));
#define RV_ALLOC(ln, n) do { (ln)->buf = R->mem + at; (ln)->len = (n); (ln)->w = 0; at += (n); } while (0)
    RV_ALLOC(&R->pre, RV_PRE + 2u);
    for (k = 0; k < 4u; k++)
        RV_ALLOC(&R->dif[k], (uint32_t)RV_DIF[k] * RV_F / 32768u + 2u);
    for (k = 0; k < 8u; k++) {
        R->tank_base[k] = (uint32_t)RV_TANK[k] * RV_F / 32768u;
        R->tank_total += R->tank_base[k];
        l = &R->tank[k];
        RV_ALLOC(l, R->tank_base[k] * RV_SMAX / 4096u + RV_EXC + 4u);
    }
#undef RV_ALLOC
    R->ok = at <= RV_MEM;
    R->scale = 4096;
    rv_param(R, 70, 65, 40, 50);
    R->scale = R->scale_t;
}

/* one block: in (mono, Q15) -> added to l, r; run at FS / 2 (n even) */
static FAST void rv_block(rv_t *R, const int32_t *in, int32_t *ol, int32_t *or_, uint32_t n)
{
    uint32_t i, k, d[8], dl[7], dr[7];
    int32_t dec = R->decay, mix = R->mix, s, kd;
    /* (the one-poles at half the rate: a step of 1 - (1 - k)^2 loses per second what k did at FS) */
    kd = Q15 - R->damp;
    kd = 2 * kd - (int32_t)(((int64_t)kd * kd) >> 15);
    if (!R->ok || !mix)
        return;
    R->scale += (R->scale_t - R->scale) >> 6;     /* SIZE glides: ~45 ms */
    s = R->scale;
    for (k = 0; k < 8u; k++) {
        d[k] = R->tank_base[k] * (uint32_t)s >> 12;
        if (d[k] + RV_EXC + 2u >= R->tank[k].len)
            d[k] = R->tank[k].len - RV_EXC - 3u;
    }
    for (k = 0; k < 7u; k++) {                    /* the taps, scaled as the lines are */
        dl[k] = (uint32_t)RV_TAPL_OFF[k] * RV_F / 32768u * (uint32_t)s >> 12;
        dr[k] = (uint32_t)RV_TAPR_OFF[k] * RV_F / 32768u * (uint32_t)s >> 12;
        if (dl[k] >= d[RV_TAPL_LINE[k]])
            dl[k] = d[RV_TAPL_LINE[k]] - 1u;
        if (dr[k] >= d[RV_TAPR_LINE[k]])
            dr[k] = d[RV_TAPR_LINE[k]] - 1u;
    }
    for (i = 0; i + 1u < n; i += 2u) {
        int32_t x, a, b, ml, mr, yl = 0, yr = 0;
        uint32_t e1, e2;
        /* the slow sweep: two sines a quarter apart, ~0.5 Hz and a little faster for the other side */
        R->lfo += 97400u;                        /* 0.50 Hz (at FS / 2) */
        R->lfo2 += 123800u;                      /* 0.64 Hz */
        ml = sin_ph(R->lfo);
        mr = sin_ph(R->lfo2 + 0x40000000u);
        e1 = (uint32_t)((ml + Q15) * RV_EXC);    /* 0 .. 2 EXC, Q15 */
        e2 = (uint32_t)((mr + Q15) * RV_EXC);
        /* the input: the predelay, the bandwidth filter, the four diffusers */
        rv_write(&R->pre, (in[i] + in[i + 1u]) >> 2);   /* (the pair averaged, and halved as before) */
        x = rv_tap(&R->pre, RV_PRE - 1u);
        R->bw += rvm(x - R->bw, 29818);       /* 0.7 at FS: 0.91 at FS / 2 */
        x = R->bw;
        for (k = 0; k < 4u; k++)
            x = rv_allpass(&R->dif[k], x, RV_DIFG[k], R->dif[k].len - 2u);
        /* the left branch: in with the right branch's output */
        a = x + rvm(R->fb_r, dec);
        {
            rv_line *l = &R->tank[0];
            uint32_t dd = d[0] - RV_EXC + (e1 >> 16);
            int32_t dly = rv_tapf(l, dd, (int32_t)((e1 >> 8) & 255u)), w = a + rvm(-22938, dly);
            rv_write(l, w);
            a = dly - rvm(-22938, w);         /* decay diffusion 1, the sign turned: -0.7 */
        }
        rv_write(&R->tank[1], a);
        a = rv_tap(&R->tank[1], d[1]);
        R->damp_l += rvm(a - R->damp_l, kd);
        a = rvm(R->damp_l, dec);
        a = rv_allpass(&R->tank[2], a, 16384, d[2]);   /* decay diffusion 2: 0.5 */
        rv_write(&R->tank[3], a);
        b = rv_tap(&R->tank[3], d[3]);
        /* the right branch: in with the left one's */
        a = x + rvm(R->fb_l, dec);
        R->fb_l = b;
        {
            rv_line *l = &R->tank[4];
            uint32_t dd = d[4] - RV_EXC + (e2 >> 16);
            int32_t dly = rv_tapf(l, dd, (int32_t)((e2 >> 8) & 255u)), w = a + rvm(-22938, dly);
            rv_write(l, w);
            a = dly - rvm(-22938, w);
        }
        rv_write(&R->tank[5], a);
        a = rv_tap(&R->tank[5], d[5]);
        R->damp_r += rvm(a - R->damp_r, kd);
        a = rvm(R->damp_r, dec);
        a = rv_allpass(&R->tank[6], a, 16384, d[6]);
        rv_write(&R->tank[7], a);
        R->fb_r = rv_tap(&R->tank[7], d[7]);
        /* the fourteen taps */
        for (k = 0; k < 7u; k++) {
            int32_t tl = rv_tap(&R->tank[RV_TAPL_LINE[k]], dl[k]), tr = rv_tap(&R->tank[RV_TAPR_LINE[k]], dr[k]);
            yl += RV_TAP_SIGN[k] > 0 ? tl : -tl;
            yr += RV_TAP_SIGN[k] > 0 ? tr : -tr;
        }
        yl = rvm(yl, mix);
        yr = rvm(yr, mix);
        ol[i] += (R->pl + yl) >> 1;              /* (drawn back up: halfway, then there) */
        or_[i] += (R->pr + yr) >> 1;
        ol[i + 1u] += yl;
        or_[i + 1u] += yr;
        R->pl = yl;
        R->pr = yr;
    }
}
static __typeof__(&rv_block) volatile rv_block_fp = rv_block;   /* (the flash's way into the RAM code) */
