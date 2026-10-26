/* SPDX-License-Identifier: GPL-3.0-only
 * NORILSK: the master delay and the master reverb -- Tonitor's (delay.c, reverb.c, the same author), at the end of
 * the chain, after the three effect slots.
 *
 * What they hear is what the mix holds: each generator's dry share (after its level) at its sends, and each effect
 * slot's output at the sends of the generators wired into it (their mean: a slot two generators share sends as much
 * as they do on average). KNOB 3 sets the delay send of the generator the keys play, KNOB 4 its reverb send
 * (Tonitor's KNOB 4 and SHIFT + KNOB 4). The delay first, mono: its echoes to both sides and half of them into the
 * reverb; then the plate, mono in, stereo out. Their returns join the mix before the master.
 *
 * The delay's line: 2 s at FS / 2 (22.05 kHz), 8-bit mu-law, 44 100 bytes in the pool. TIME 20 ms .. 2 s on a log
 * scale; FEEDBACK up to 0.95, each echo through a one-pole low-pass (DAMP); MIX its return. NORILSK: a change of
 * TIME cross-fades the old head out and the new in over 23 ms (Tonitor's glides: a bend of pitch, which this
 * firmware never makes).
 * The reverb: the plate of reverb.c as an instance of its own (34 KiB, in the pool from build 32), SIZE 0.5 .. 1.0 (Tonitor's 1.25 is
 * more memory than there is), DECAY, DAMP, MIX. Both set on the DELAY and REVERB pages (SEQ).
 *
 * Their tails are kept (the owner's, build 26: 2 s after the last voice they used to fade out over 0.5 s): whatever
 * reaches them rings out as it will -- a voice let go, a generator or an effect slot muted, the slots' own gate
 * closing -- and when they are spent, under -60 dB with nothing coming in, they stop running. */

#define MDL_N 44100u                     /* 2 s at FS / 2 */
static uint8_t mdl_line[MDL_N] POOL;
static struct {
    uint32_t w;                          /* where the next sample is written */
    int32_t t, t0;                       /* the delay (Q8 samples at FS / 2) and, while cross-fading, the old one */
    int32_t xf;                          /* pairs of the cross-fade left */
    int32_t lp;                          /* the damping's state */
    int32_t prev;                        /* the last echo, for the line drawn to the next */
} mdl;
static volatile int32_t mdl_t_want = 400 * 22 * 256, mdl_fb, mdl_damp = Q15, mdl_mix;
static rv_t mrv POOL;                    /* the master plate (the pool from build 32: RAM before) */

static uint32_t m_quiet;                                    /* blocks with nothing in and nothing out */

/* TIME as ms: 20 x 100^(v / 100) (Tonitor's) */
static int32_t mdl_ms(int32_t v) { return (int32_t)((20u * exp2_q16(clampi(v, 0, 100) * 4354 + (clampi(v, 0, 100) * 12) / 100)) >> 16); }
static void mdl_param(int32_t time, int32_t fb, int32_t damp, int32_t mix)   /* each 0..100 (main loop) */
{
    int32_t ms = mdl_ms(time);
    mdl_t_want = clampi(ms * 22 * 256 + ms * 13, 2 * 256, (int32_t)(MDL_N - 4u) * 256);   /* x 22.05 */
    mdl_fb = clampi(fb, 0, 100) * (LV(0.95) / 100);
    mdl_damp = Q15 - clampi(damp, 0, 100) * (LV(0.88) / 100);
    mdl_mix = clampi(mix, 0, 100) * (Q15 / 100);
}

/* a block: in (mono) into the line; the echoes added to l and r, half of them to rvin */
static FAST void mdl_block(const int32_t *in, int32_t *l, int32_t *r, int32_t *rvin)
{
    uint32_t i;
    int32_t fb = mdl_fb, damp = mdl_damp, mix = mdl_mix, tw = mdl_t_want;
    if (tw != mdl.t && !mdl.xf) {                    /* (a new time: cross-faded, no slide) */
        mdl.t0 = mdl.t;
        mdl.t = tw;
        mdl.xf = XF_PAIRS;
    }
    for (i = 0; i + 1u < CTL; i += 2u) {
        int32_t x = (in[i] + in[i + 1u]) >> 1, y = mu_tap(mdl_line, MDL_N, mdl.w, mdl.t), o0;
        if (mdl.xf) {
            int32_t yo = mu_tap(mdl_line, MDL_N, mdl.w, mdl.t0);
            y = yo + (((y - yo) * (int32_t)(XF_PAIRS - (uint32_t)mdl.xf)) >> 9);
            mdl.xf--;
        }
        mdl.lp += mulz15(y - mdl.lp, damp);          /* (towards zero: the echoes die out to nothing) */
        {
            int32_t wv = clampi(x + mulz15(mdl.lp, fb), -32767, 32767);
            mdl_line[mdl.w] = wv > -4 && wv < 4 ? 0x80u : mu_enc8(wv);   /* (silence as silence) */
        }
        if (++mdl.w >= MDL_N)
            mdl.w = 0;
        y = mulq15(mdl.lp, mix);
        o0 = (mdl.prev + y) >> 1;                    /* drawn out to the full rate: half-way, then the echo */
        mdl.prev = y;
        l[i] += o0;
        r[i] += o0;
        l[i + 1u] += y;
        r[i + 1u] += y;
        rvin[i] += o0 >> 1;
        rvin[i + 1u] += y >> 1;
    }
}
static __typeof__(&mdl_block) volatile mdl_block_fp = mdl_block;

/* ------------------------------------------------------------ the master's drive --- */
/* SHIFT + KNOB 1 (build 37): the whole master driven, 0 .. 100, 0 off -- after the lanes, the master delay and the
 * reverb, before the MASTER knob. Tonitor's distortion (layers.c dist_block), made rougher for this firmware: two gain
 * stages in series as a valve preamp's, the first symmetric (odd harmonics), the second clipping sooner on one side
 * (even), at twice the sample rate (linear interpolation up, a [1/4 1/2 1/4] half-band down); the highs lifted 6 dB
 * before and lowered after; a DC blocker; the level brought back to the dry signal's (the mean squares followed, the
 * wet scaled by the root of their ratio: more grit, not more loudness). Norilsk's: the drive x 0.5 .. x 64 (+42 dB;
 * Tonitor's x 32), above 50 a third stage, a hard clip, blended in, and the tone closing only to 9 kHz (6.5 before):
 * it keeps its teeth. Each side its own */
static volatile uint8_t master_drive;           /* 0 .. 100, 0 off */
typedef struct {
    int32_t amt, pre, post, dc, lp, env, ep, ymp, yp, dryp;
    int64_t ein, eout;
    int32_t g, gp;
} drv_t;
static drv_t drv[2];
#define DV_EMPH 3110                             /* 1 - e^(-2 pi 700 / FS), Q15 */
AINL int32_t dv_asym(int32_t u) { return u >= 0 ? tanh_q15(u) : (tanh_q15((u * 8) / 5) * 5) / 8; }
AINL int32_t dv_stage(int32_t e, int32_t g1, int32_t b, int32_t fb, int32_t g2, int32_t hc, int32_t hk)
{
    int32_t u = tanh_q15((int32_t)(((int64_t)e * g1) >> 12)), y = dv_asym(mulq12(u, g2) + b) - fb;
    if (hk) {                                    /* (the third stage: a hard clip at hc, blended in by hk) */
        int32_t h = clampi(y * 2, -hc, hc);
        y += mulq15(h - y, hk);
    }
    return y;
}
static int32_t dv_log2(int64_t v)
{
    int32_t e = 0;
    while (v > 0x7FFFFFFFLL) {
        v >>= 1;
        e++;
    }
    return log2_q16((uint32_t)(v > 0 ? v : 1)) + (e << 16);
}
static void drv_block(drv_t *d, int32_t *x, int32_t want)
{
    int32_t a, g1, b, fb, g2, tone, w, beta, hc, hk, g0 = d->gp, gs, i;
    int64_t sin = 0, sout = 0;
    d->amt += ((want << 8) - d->amt) >> 2;
    if (!want && d->amt < 64) {                  /* (off: from silence when it comes in again) */
        memset(d, 0, sizeof *d);
        d->g = d->gp = Q16;
        return;
    }
    a = d->amt >> 8;
    g1 = (int32_t)((2048u * (uint64_t)exp2_q16(d->amt * 7 / 100 * 256)) >> 16);   /* x 0.5 .. x 64, Q12 */
    b = 3277 + d->amt * 6554 / 25600;            /* the bias, 0.1 .. 0.3 */
    fb = dv_asym(b);
    g2 = 4096 + d->amt * 8192 / 25600;           /* x 1 .. x 3, Q12 */
    beta = 4915 + d->amt * 6554 / 25600;
    tone = 29430 - d->amt * (29430 - 23500) / 25600;   /* 16 kHz .. 9 kHz */
    hk = a > 50 ? (a - 50) * (Q15 / 50) : 0;     /* the hard clip, in above 50 */
    hc = LV(0.45);
    w = a >= 6 ? Q15 : d->amt * Q15 / (6 * 256);
    gs = (d->g - g0) / (int32_t)CTL;
    for (i = 0; i < (int32_t)CTL; i++) {
        int32_t dry = x[i], e, ym, y, o;
        d->pre += mulq15(dry - d->pre, DV_EMPH);
        e = 2 * dry - d->pre;                    /* the highs +6 dB */
        d->env = absi(e) > d->env ? absi(e) : d->env - (d->env >> 12);
        e += mulq15(d->env, beta);               /* (lopsided however hard it is driven: even harmonics) */
        ym = dv_stage((d->ep + e) >> 1, g1, b, fb, g2, hc, hk);
        y = dv_stage(e, g1, b, fb, g2, hc, hk);
        o = (d->ymp + 2 * d->yp + ym) >> 2;      /* (one sample late) */
        d->ep = e;
        d->ymp = ym;
        d->yp = y;
        d->post += mulq15(o - d->post, DV_EMPH);
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
        d->g = (int32_t)exp2_q16(clampi((dv_log2(d->ein) - dv_log2(d->eout)) / 2, -(5 << 16), 1 << 16));
}
static void drive_block(int32_t *l, int32_t *r)
{
    int32_t want = clampi(master_drive, 0, 100);
    drv_block(&drv[0], l, want);
    drv_block(&drv[1], r, want);
}

static void master_init(void)
{
    memset(mdl_line, 0x80, sizeof mdl_line);         /* (mu-law's zero is 0x80) */
    memset(&mdl, 0, sizeof mdl);
    mdl.t = mdl_t_want;
    rv_init(&mrv);
    memset(mrv.mem, 0, sizeof mrv.mem);
    memset(mst_in_d, 0, sizeof mst_in_d);
    memset(mst_in_r, 0, sizeof mst_in_r);
    memset(mst_out_l, 0, sizeof mst_out_l);
    memset(mst_out_r, 0, sizeof mst_out_r);
    m_quiet = 0;
    master_drive = 0;
    memset(drv, 0, sizeof drv);
    drv[0].g = drv[0].gp = drv[1].g = drv[1].gp = Q16;
}

/* a block, after the generators and the slots: the sends (dlin, rvin: the generators' and the slots', summed by
 * mix_block; rvin takes the echoes too); the returns into l, r, their tails whole */
static void master_block(int32_t *l, int32_t *r, const int32_t *dlin, int32_t *rvin)
{
    int32_t ol[CTL], or_[CTL];
    uint32_t i, any = 0, pk = 0;
    for (i = 0; i < CTL; i++) {
        any |= (uint32_t)(dlin[i] | rvin[i]);
        ol[i] = or_[i] = 0;
    }
    if (any || m_quiet < 64u) {                      /* (nothing sent, the tails spent: not run) */
        mdl_block_fp(dlin, ol, or_, rvin);
        rv_block_fp(&mrv, rvin, ol, or_, CTL);
        for (i = 0; i < CTL; i++) {
            uint32_t a = (uint32_t)absi(ol[i]) | (uint32_t)absi(or_[i]);
            pk = a > pk ? a : pk;
        }
        m_quiet = !any && pk < 8u ? m_quiet + 1u : 0u;
    }
    for (i = 0; i < CTL; i++) {
        l[i] += ol[i];
        r[i] += or_[i];
    }
}
/* the rack's job (either core, build 32): the sends of the block before through the delay and the plate, the returns
 * for mix_block to add */
static void master_job(void)
{
    uint32_t i;
    for (i = 0; i < CTL; i++)
        mst_out_l[i] = mst_out_r[i] = 0;
    master_block(mst_out_l, mst_out_r, mst_in_d, mst_in_r);
}

