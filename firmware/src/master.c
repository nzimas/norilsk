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
 * The reverb: the plate of reverb.c as an instance of its own (34 KiB, in RAM), SIZE 0.5 .. 1.0 (Tonitor's 1.25 is
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
static rv_t mrv;                         /* the master plate (RAM) */

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

static void master_init(void)
{
    memset(mdl_line, 0x80, sizeof mdl_line);         /* (mu-law's zero is 0x80) */
    memset(&mdl, 0, sizeof mdl);
    mdl.t = mdl_t_want;
    rv_init(&mrv);
    memset(mrv.mem, 0, sizeof mrv.mem);
    m_quiet = 0;
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
