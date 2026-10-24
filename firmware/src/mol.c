/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR: MOLECULE, the engine -- six-operator FM with waves in its operators and a grain cloud on its
 * output, one voice a note.
 *
 * The synthesis is msfa (Dexed's core, Apache-2.0) as Felucca ported it to integer C and Tonitor shaped it
 * (fm6_core.c): 32 algorithms, six operators with four-rate / four-level envelopes, keyboard scaling, ratio
 * or fixed frequencies, feedback, the LFO and the pitch envelope, in the 155-byte single-voice layout. Two
 * things are added to the operators and one after them:
 *
 *   WAVES     an operator reads the sine, or one of 28 band-limited single cycles (wt.c: the four primitives,
 *             sixteen harmonic sets, eight made for bells and FM), morphed with the next of its bank; and its
 *             phase may be bent (phase distortion, after the Casio CZ), as drawn or following its envelope:
 *             a sine that opens like a filter, a square whose width is its envelope
 *   GRAINS    the operators' sound is written, always, into a buffer of about half a second (at half the
 *             rate, in 8-bit mu-law: 12 KiB a voice) and read back as grains, two ways:
 *               CLOUD   grains at a rate of their own (a few to hundreds a second), of a length of their own,
 *                       from a place behind the write head, jittered in time and level, pitched by the sound's
 *                       intervals, reversed now and then, darkened, fed back, and, when the sound asks for it,
 *                       FROZEN: the buffer stops listening a moment after the note begins and the cloud turns
 *                       over what it holds -- a strike that becomes a texture
 *               PULSAR  grains at a ratio of the note's frequency (Roads's pulsars): one grain a period reads
 *                       the sound back as a formant, a burst mask drops some, so the note has a sub-harmonic
 *             The cloud has a level of its own after the note begins and after it ends, so a frozen cloud
 *             fades in its own time.
 *
 * The generator does not make instruments. It draws from twelve families of sound objects, eight bodies and
 * four clouds that borrow a body:
 *   METAL     Risset's bells and gongs: many carriers on inharmonic ratios, the high partials dying first
 *   DRONE     a spectral drone: slow pairs, near-harmonic, detuned to beat, an index that wanders
 *   BRUIT     noise from feedback and high ratios, scattered by a sample-and-hold LFO
 *   IMPACT    a struck body, its pitch falling, the modulators dying before the carriers
 *   PULSE     Schaeffer's iterative objects: a sound pulsed by its own LFO, square or random
 *   GLASS     carriers on the bell waves, morphed, barely modulated: glass, bowls, a dark celesta
 *   FORMANT   the CZ: carriers bent by their envelopes under a modulator at the fundamental
 *   REED      a saw or a square whose width is its envelope, modulated by a feedback operator: reeds, brass
 *   CLOUD     a body under a slow cloud: long grains, pitched down and up, dark, smeared
 *   SHATTER   a body broken into short grains: fast, jittered, reversed, scattered
 *   PULSAR    a sustained body read back a grain a period: formants, sub-harmonics
 *   FREEZE    a struck body frozen into a cloud a moment after the strike, which fades in its own time
 * A path leans the draw (PATH_OPEN: every family alike): AMBIENT towards the drones, the glass, the metals and
 * the clouds, slower and darker; EXPERIMENTAL towards the bruits, impacts, pulses and shatters, the modulators
 * up. Generation 2 (build 5: the owner's, textures and drones only) draws no IMPACT and no PULSE, lends the clouds
 * only bodies that hold, and makes every sound swell in, hold and keep moving (mo_gen2). About one sound in three is
 * mutated once more. Every sound is measured as it is made (mo_measure) and
 * given the gain that brings its loudest moment to one level.
 *
 * EDIT's four macros on a sound, live under a held note: BRIGHTNESS, DETUNE (or INHARMONIC), GRAIN (the
 * cloud's share and rate -- and a cloud for the sounds that have none), MOTION. 0 everywhere: the sound as
 * drawn, bit for bit. */

enum { MF_METAL, MF_DRONE, MF_BRUIT, MF_IMPACT, MF_PULSE, MF_GLASS, MF_FORMANT, MF_REED,
       MF_BODY_N,
       MF_CLOUD = MF_BODY_N, MF_SHATTER, MF_PULSAR, MF_FREEZE, MF_N };
static const char *const MF_NAME[MF_N] = {"METAL", "DRONE", "BRUIT", "IMPACT", "PULSE", "GLASS", "FORMANT", "REED",
                                          "CLOUD", "SHATTER", "PULSAR", "FREEZE"};
/* the families' weights on each path (generation 0) */
static const uint8_t MF_W[PATH_N][MF_N] = {
    {2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3},
    {3, 5, 0, 1, 0, 5, 3, 2, 5, 0, 2, 4},
    {2, 1, 5, 4, 4, 1, 2, 3, 2, 5, 4, 3},
};
/* .. and the bodies the clouds borrow: CLOUD and SHATTER any, PULSAR a sustained one, FREEZE a struck one */
static const uint8_t MF_BODY_ANY[PATH_N][MF_BODY_N] = {
    {2, 2, 2, 2, 2, 2, 2, 2}, {3, 5, 0, 2, 0, 5, 3, 2}, {2, 1, 5, 4, 3, 1, 2, 3}};
static const uint8_t MF_BODY_SUST[PATH_N][MF_BODY_N] = {
    {0, 3, 1, 0, 1, 3, 3, 3}, {0, 5, 0, 0, 0, 5, 3, 2}, {0, 2, 3, 0, 2, 1, 3, 4}};
static const uint8_t MF_BODY_STRUCK[PATH_N][MF_BODY_N] = {
    {3, 0, 1, 3, 2, 2, 0, 0}, {4, 0, 0, 2, 0, 4, 0, 0}, {2, 0, 3, 4, 3, 1, 0, 0}};
/* generation 2: only what sustains and evolves -- no IMPACT, no PULSE drawn; a cloud borrows a body that holds */
static const uint8_t MF_W2[PATH_N][MF_N] = {
    {2, 3, 2, 0, 0, 2, 1, 1, 3, 2, 2, 3},
    {3, 5, 1, 0, 0, 5, 2, 1, 5, 1, 2, 4},
    {3, 2, 5, 0, 0, 2, 2, 2, 3, 4, 3, 3},
};
static const uint8_t MF_BODY_G2[PATH_N][MF_BODY_N] = {
    {3, 3, 2, 0, 0, 3, 2, 1}, {3, 5, 1, 0, 0, 5, 2, 1}, {3, 2, 4, 0, 0, 2, 2, 2}};
static const uint8_t *mo_fam_w(uint32_t path, uint32_t gen)
{
    path = path >= PATH_N ? PATH_OPEN : path;
    return gen >= 2u ? MF_W2[path] : MF_W[path];
}

/* the grain recipe */
enum { GM_OFF, GM_CLOUD, GM_PULSAR };
typedef struct {
    uint8_t mode;
    uint16_t size_ms;            /* a grain, 2 .. 400 ms */
    uint16_t rate;               /* CLOUD: grains a second (1 .. 400); PULSAR: 4 x grains a period (1 .. 16) */
    uint16_t back_ms, spread_ms; /* where a grain starts: behind the write head, and how far it may scatter */
    int8_t iv[4];                /* the intervals a grain may take, semitones */
    uint8_t pch;                 /* .. the chance it takes one, percent */
    uint8_t jit;                 /* CLOUD: the timing's jitter, percent; PULSAR: the chance a grain is dropped */
    uint8_t cents;               /* the pitch's jitter */
    uint8_t rev;                 /* the chance a grain is reversed, percent */
    uint8_t shape;               /* the window: 0 Hann, 1 percussive, 2 swell */
    uint8_t fb;                  /* the grains fed back into the buffer, percent */
    uint8_t mix;                 /* the grains' share of the output, percent */
    uint8_t burst;               /* PULSAR: of every eight grains, how many sound (1 .. 8) */
    uint16_t freeze_ms;          /* 0: never; else the buffer stops listening this long after the note begins */
    uint16_t atk_ms, rel_ms;     /* the cloud's own level: in after the note begins, out after it ends */
    uint8_t dark;                /* a low-pass on the grains, 0 (open) .. 100 (350 Hz) */
    uint8_t drift;               /* a slow wander of where the grains start, percent of the spread */
} mo_gr_t;

typedef struct {
    uint8_t p[FP_SIZE + 1u];     /* the DX7 voice */
    fm6_wsel_t ws[6];            /* each operator's wave (the sixth first, as the voice lays them) */
    mo_gr_t gr;                  /* the cloud, if the sound has one */
    mo_gr_t gr_def;              /* .. and the one GRAIN turned up gives a sound that has none */
    uint8_t family, body;        /* the family; the body a cloud borrowed (the family itself for a body) */
    uint8_t path, gen;           /* what it was drawn on: PATH_*, the generators' generation */
    int32_t gain;                /* Q12: the measured level brought to the target */
    uint32_t seed;               /* what made it: mo_design(-1, seed, path, gen) + mo_measure gives it again */
} mo_design_t;

#define MO_GR_N 12288u           /* the grain buffer: samples at FS / 2 (0.557 s), mu-law */
#define MO_GR_MAXG 4u            /* grains at once */
#define MO_VEL 100
#define MO_TARGET_RMS 3277       /* 0.1 Q15: what a sound's loudest window is brought to */

typedef struct {
    int32_t pos, inc;            /* the read head in the buffer (Q16 samples) and its step a sample */
    uint32_t wph, winc;          /* the window's phase (a grain = 2^32) and step */
    int32_t gain;                /* Q15 */
    uint8_t on, shape;
} mo_grain_t;

typedef struct {
    fm6_note_t n;
    fm6_lfo_t lfo;
    int32_t note;                /* MIDI note, transposed */
    int32_t lp;                  /* (SATURATION below the middle -- unused by MOLECULE's macros; kept for the host) */
    uint32_t tph;                /* MOTION above the middle: its tremolo's phase */
    uint8_t live, hold;
    uint8_t fm_done;             /* the operators have fallen silent for good (or are frozen out) */
    /* the cloud */
    mo_grain_t g[MO_GR_MAXG];
    uint32_t w;                  /* the write index */
    uint32_t filled;             /* samples written since the note began (what the grains may read) */
    int32_t half;                /* the sample waiting for its pair (the buffer is at FS / 2) */
    uint8_t odd, frozen, bursti, gr_on;
    int32_t next;                /* samples to the next grain */
    int32_t last;                /* the grains' last output (the feedback) */
    int32_t genv;                /* the cloud's own level, Q15 */
    int32_t amt;                 /* the wet share as it glides, Q15 */
    uint32_t blocks;             /* blocks since the note began */
    uint32_t tail;               /* blocks the cloud goes on after the operators fall silent */
    uint32_t drift;              /* the drift's phase */
    svf_t dark;
    rnd_t r;
    uint8_t buf[MO_GR_N];
} mo_voice_t;

/* ------------------------------------------------------------ the voice --- */
static void mo_note_on(mo_voice_t *v, const mo_design_t *d, int32_t note, int fresh)
{
    const uint8_t *p = d->p;
    uint32_t k;
    if (!v->live)
        fresh = 1;
    v->note = note + p[FP_TRNSP] - 24;
    if (fresh) {
        fm6_lfo_reset(&v->lfo, p);
        for (k = 0; k < MO_GR_MAXG; k++)
            v->g[k].on = 0;
        v->filled = 0;                           /* (the buffer is not cleared: what the grains may read starts here) */
        v->half = 0;
        v->odd = 0;
        v->last = 0;
        v->genv = 0;
        v->amt = 0;
        v->next = 0;
        v->bursti = 0;
        v->dark.ic1 = v->dark.ic2 = 0;
        v->tph = 0;
        rnd_seed(&v->r, d->seed ^ (uint32_t)note * 7919u ^ v->w);
    }
    v->frozen = 0;
    v->blocks = 0;
    v->tail = 0;
    v->fm_done = 0;
    v->hold = 0;
    fm6_note_init(&v->n, p, v->note, MO_VEL, fresh);
    fm6_lfo_key(&v->lfo);
    v->live = 1;
}
/* the carriers to full and held there: the track's own envelope shapes the voice instead */
static void mo_hold(mo_voice_t *v, const mo_design_t *d)
{
    uint32_t car = fm6_carriers(d->p[FP_ALG] & 31u), k;
    for (k = 0; k < 6u; k++)
        if ((car >> k) & 1u) {
            v->n.env[k].hold = 1;
            fm6_env_advance_fp(&v->n.env[k], d->p + k * FP_OP, v->n.env[k].ix);
        }
    v->hold = 1;
}
static void mo_note_off(mo_voice_t *v, const mo_design_t *d)
{
    if (v->live && v->n.down)
        fm6_note_key(&v->n, d->p, 0);
}

/* ------------------------------------------------------------ the macros --- */
AINL uint32_t mo_carriers_of(const uint8_t *p) { return fm6_carriers(p[FP_ALG] & 31u); }
AINL int mo_skews(uint32_t f)
{
    return f == MF_METAL || f == MF_BRUIT || f == MF_IMPACT || f == MF_PULSE || f == MF_SHATTER;
}
static const char *mo_mac_name(const mo_design_t *d, uint32_t j)
{
    static const char *const N[4] = {"BRIGHTNESS", "DETUNE", "GRAIN", "MOTION"};
    return j == 1u && mo_skews(d->body) ? "INHARMONIC" : N[j & 3u];
}
/* an operator's frequency against the note's, x 100 (fixed ones: against middle C) */
AINL int32_t mo_op_ratio100(const uint8_t *op)
{
    if (!op[FP_MODE])
        return op[FP_FC] ? op[FP_FC] * (100 + op[FP_FF]) : (100 + op[FP_FF]) / 2;
    return (int32_t)((uint32_t)(exp2_q16((op[FP_FC] & 3) * 217706 + (int32_t)(op[FP_FF] % 100u) * 2177) >> 16) *
                     100u / 262u);                       /* 10^x = 2^(3.3219 x), Q16 */
}
/* BRIGHTNESS (the modulators' level -- the index -- about 27 dB either way, the feedback up with it, the carriers
 * tilted, the higher ones up), DETUNE (the carriers but the lowest pulled apart, +-40 cents one way and the other,
 * every modulator +-25 cents off) or INHARMONIC for the metallic and noisy bodies (every operator but the lowest
 * carrier off its ratio), as the core takes them: a pitch and a level offset an operator, the feedback, the LFO's
 * depths (MOTION). (Tonitor's fm_macros.) */
static FAST void mo_macros(const mo_design_t *d, const int8_t *mac, int32_t *dt, int32_t *lvl, int32_t *fb, uint32_t *lfoq8)
{
    const uint8_t *p = d->p;
    uint32_t car = mo_carriers_of(p), k, lo = 6, ci = 0, mi = 0;
    int32_t r[6], rmin = 0x7FFFFFFF, rmax = 0;
    for (k = 0; k < 6u; k++) {
        r[k] = mo_op_ratio100(p + k * FP_OP);
        if ((car >> k) & 1u) {
            if (r[k] < rmin)
                rmin = r[k], lo = k;
            if (r[k] > rmax)
                rmax = r[k];
        }
    }
    for (k = 0; k < 6u; k++) {
        int carrier = (int)((car >> k) & 1u);
        if (!carrier)
            lvl[k] = mac[0] * 1179648;
        else if (rmax > rmin)
            lvl[k] = mac[0] * ((r[k] - rmin) * 128 / (rmax - rmin) - 64) * 8192;
        if (mo_skews(d->body)) {
            if (k != lo)
                dt[k] = mac[1] * 21845 * (int32_t)(1u + (k & 1u));
        } else if (carrier && k != lo)
            dt[k] = mac[1] * 8738 * ((ci++ & 1u) ? -1 : 1);
        else if (!carrier)
            dt[k] = mac[1] * 5461 * ((mi++ & 1u) ? -1 : 1);
    }
    *fb = clampi(*fb + (mac[0] > 0 ? mac[0] / 32 : 0), 0, 7);
    *lfoq8 = (uint32_t)clampi(256 + mac[3] * 4, 0, 512);
}
static __typeof__(&mo_macros) volatile mo_macros_fp = mo_macros;   /* (the flash's way into the RAM code) */

/* ------------------------------------------------------------- the grains --- */
AINL int32_t gr_dec(uint8_t b)
{
    int32_t m = MULAW_DEC[b & 0x7Fu];
    return b & 0x80u ? -m : m;
}
AINL uint8_t gr_enc(int32_t x)
{
    uint32_t a = (uint32_t)(x < 0 ? -x : x);
    if (a > 32767u)
        a = 32767u;
    return (uint8_t)((x < 0 ? 0x80u : 0u) | MULAW_ENC[a >> 4]);
}
AINL uint32_t ms_samples2(uint32_t ms) { return ms * 2205u / 100u; }   /* ms -> samples at FS / 2 */
AINL uint32_t ms_blocks(uint32_t ms) { return (ms * 1411u) >> 10; }    /* ms -> blocks (1378 a second) */

/* a grain, now: into a free slot */
static FAST void mo_spawn(mo_voice_t *v, const mo_gr_t *gr, uint32_t len, int32_t pch, int32_t cents_max, int32_t norm,
                          uint32_t back, uint32_t spread, int32_t rate16)
{
    mo_grain_t *g = 0;
    uint32_t k, bk, need, dmax, delay, start;
    int32_t semis = 0, cents = 0, r_q16, inc;
    int rev;
    for (k = 0; k < MO_GR_MAXG; k++)
        if (!v->g[k].on) {
            g = &v->g[k];
            break;
        }
    if (!g)
        return;
    if (gr->mode == GM_PULSAR) {                           /* the burst mask, and the drops */
        uint32_t i = v->bursti++ & 7u;
        if (i >= gr->burst || rnd_below(&v->r, 100) < gr->jit)
            return;
    }
    if (rnd_below(&v->r, 100) < pch)
        semis = gr->iv[rnd_below(&v->r, 4)];
    if (cents_max)
        cents = mulq15(rnd_bi(&v->r), cents_max * (Q15 / 100)) * 100 / Q15;
    r_q16 = (int32_t)exp2_q16((semis * 100 + cents) * 65536 / 1200);
    r_q16 = (int32_t)(((int64_t)r_q16 * rate16) >> 16);
    inc = r_q16 >> 1;                                      /* the buffer is at half the rate */
    rev = rnd_below(&v->r, 100) < gr->rev;
    /* where it starts: far enough back that the read head never meets the write head, and not so far back that
     * the write head comes round to it (or past what the note has written) */
    bk = (uint32_t)(((uint64_t)len * (uint32_t)inc) >> 16);   /* buffer samples the grain reads */
    need = rev ? 4u : (inc > Q16 / 2 ? (uint32_t)(((uint64_t)len * (uint32_t)(inc - Q16 / 2)) >> 16) + 8u : 8u);
    dmax = MO_GR_N - 64u - (rev ? bk + len / 2u : inc < Q16 / 2 ? (uint32_t)(((uint64_t)len * (uint32_t)(Q16 / 2 - inc)) >> 16) : 0u);
    if (v->filled < MO_GR_N && dmax > v->filled)
        dmax = v->filled;
    if (need + 16u > dmax)
        return;
    delay = back + (spread ? (uint32_t)rnd_below(&v->r, (int32_t)spread + 1) : 0u);
    if (delay < need)
        delay = need;
    if (delay > dmax)
        delay = dmax;
    start = v->w + MO_GR_N - delay;
    while (start >= MO_GR_N)
        start -= MO_GR_N;
    g->pos = (int32_t)(start << 16);
    g->inc = rev ? -inc : inc;
    g->wph = 0;
    g->winc = 0xFFFFFFFFu / (len ? len : 1u);
    g->shape = gr->shape;
    g->gain = gr->mode == GM_CLOUD && gr->jit ? mulq15(norm, Q15 - mulq15(rnd_q15(&v->r), gr->jit * (Q15 / 200))) : norm;
    g->on = 1;
}
static __typeof__(&mo_spawn) volatile mo_spawn_fp = mo_spawn;   /* (the flash's way into the RAM code) */

/* one block of the cloud over y (the operators' sound, Q15) into out; gr: the recipe in force (0: none -- the
 * buffer still listens), gmac: the GRAIN macro as felt (-64 .. 64), drift_pct: how far the start wanders */
static FAST void mo_grains(mo_voice_t *v, const mo_gr_t *gr, const int32_t *y, int32_t *out, int32_t gmac, int32_t motion)
{
    int32_t mix1 = 0, da, norm = 0, fb = 0, pch = 0, cents = 0, rate16 = Q16, lpk = 0;
    uint32_t i, k, len = 0, interval = 0, back = 0, spread = 0, listen = !v->frozen;
    svf_c dc;
    if (gr) {
        int32_t m = gmac;
        pch = gr->pch;
        cents = gr->cents;
        mix1 = gr->mix * (Q15 / 100);
        if (m > 0) {                                       /* GRAIN up: wetter, faster, more of the intervals */
            mix1 += ((Q15 - mix1) * m) >> 6;
            pch += m >> 1;
        } else if (m < 0)                                  /* .. down: drier, slower */
            mix1 += (mix1 * m) >> 6;
        rate16 = (int32_t)exp2_q16(m * 2048);              /* 2^(m / 32): a quarter to four times */
        if (gr->mode == GM_PULSAR) {
            int32_t inc24 = fm6_freq(fm6_note_logfreq(v->note * 16, 0));   /* the note's step (Q24 cycles a sample) */
            uint32_t per;
            per = inc24 > 0 ? (uint32_t)((1u << 24) / (uint32_t)inc24) : 4096u;   /* samples a period */
            {   /* per x 4 / (rate / 4 x rate16 / 2^16): in 32 bits (per < 2^22, the divisor < 2^15) */
                uint32_t den = ((uint32_t)(gr->rate ? gr->rate : 4u) * (uint32_t)rate16) >> 8;
                interval = (per * 1024u) / (den ? den : 1u);
            }
            if (interval < 24u)
                interval = 24u;
            back = ms_samples2(gr->back_ms);
            spread = ms_samples2(gr->spread_ms);
        } else {
            uint32_t rate = (uint32_t)(((uint64_t)(gr->rate ? gr->rate : 1u) * (uint32_t)rate16) >> 16);
            rate = rate < 1u ? 1u : rate > 660u ? 660u : rate;
            interval = (uint32_t)FS / rate;
            back = ms_samples2(gr->back_ms);
            spread = ms_samples2(gr->spread_ms);
            fb = gr->fb * (Q15 / 100);
        }
        len = (uint32_t)gr->size_ms * 441u / 10u;
        if (len < 32u)
            len = 32u;
        /* the drift: a slow wander of where the grains start (MOTION adds to it) */
        {
            int32_t dr = gr->drift + (motion > 0 ? motion : 0), s;
            v->drift += 80000u + (uint32_t)dr * 3000u;    /* 0.025 .. 0.1 Hz */
            s = mulq15(sin_ph(v->drift), (int32_t)spread / 2);
            s = mulq15(s, clampi(dr, 0, 100) * (Q15 / 100));
            back = (uint32_t)clampi((int32_t)back + s, 0, (int32_t)MO_GR_N / 2);
        }
        /* the grains' level: their power adds up with the overlap O = len / interval, a Hann grain's mean square
         * being 3/8: each at sqrt(8 / (3 O)) -- above 1 for sparse ones (at most x 2), less for a dense cloud */
        {
            uint32_t k2 = (interval << 15) / (3u * len);
            if (k2 > 65535u)
                k2 = 65535u;
            norm = (int32_t)isqrt32(k2 << 16) << 1;
            norm = clampi(norm, 4096, 2 * Q15);
        }
        if (gr->dark) {                                    /* the darkening: 16 kHz down 5.5 octaves */
            lpk = (int32_t)(exp2_q16(915000 - gr->dark * 3604) >> 16);
            svf_coef(&dc, clampi(lpk, 200, 18000), 2896);
        }
        /* the cloud's own level */
        if (v->n.down || v->hold) {
            int32_t step = Q15 / (int32_t)(ms_blocks(gr->atk_ms) + 1u);
            v->genv = v->genv + step > Q15 ? Q15 : v->genv + step;
        } else {
            int32_t step = Q15 / (int32_t)(ms_blocks(gr->rel_ms) + 1u);
            v->genv = v->genv - step < 0 ? 0 : v->genv - step;
        }
        mix1 = mulq15(mix1, v->genv);
        /* the freeze */
        if (gr->freeze_ms && !v->frozen && v->blocks >= ms_blocks(gr->freeze_ms)) {
            v->frozen = 1;
            listen = 0;
        }
    } else {
        v->genv = 0;
    }
    da = (mix1 - v->amt) / (int32_t)CTL;
    for (i = 0; i < CTL; i++) {
        int32_t in = y[i], sum = 0, wet;
        if (listen) {
            int32_t s = in + mulq15(v->last, fb);
            if (v->odd) {
                v->buf[v->w] = gr_enc((v->half + s) >> 1);
                if (++v->w >= MO_GR_N)
                    v->w = 0;
                if (v->filled < MO_GR_N)
                    v->filled++;
            } else
                v->half = s;
            v->odd ^= 1u;
        }
        v->amt += da;
        if (!gr || (!v->amt && !mix1)) {
            v->last = 0;
            out[i] = in;
            continue;
        }
        if (--v->next <= 0) {
            int32_t j = gr->mode == GM_CLOUD ? mulq15(rnd_bi(&v->r), gr->jit * (Q15 / 100)) : 0;
            mo_spawn(v, gr, len, pch, cents, norm, back, spread, rate16);
            v->next = (int32_t)interval + mulq15((int32_t)interval, j);
            if (v->next < 8)
                v->next = 8;
        }
        for (k = 0; k < MO_GR_MAXG; k++) {
            mo_grain_t *g = &v->g[k];
            int32_t p, f, s0, s1, w, t;
            uint32_t i0, i1, was;
            if (!g->on)
                continue;
            p = g->pos;
            i0 = (uint32_t)p >> 16;
            i1 = i0 + 1u >= MO_GR_N ? 0u : i0 + 1u;
            f = (p & 0xFFFF) >> 1;
            s0 = gr_dec(v->buf[i0]);
            s1 = gr_dec(v->buf[i1]);
            t = (int32_t)(g->wph >> 17);                   /* 0 .. Q15 through the grain */
            if (g->shape == 1)                             /* percussive: a quick rise, a fall */
                w = t < 2048 ? t * 16 : mulq15(Q15 - t, Q15 - t);
            else if (g->shape == 2)                        /* swell: a slow rise, a quick fall */
                w = t > 30720 ? (Q15 - t) * 16 : mulq15(t, t);
            else {                                         /* Hann, from the table */
                uint32_t hi = g->wph >> 24;
                int32_t hf = (int32_t)((g->wph >> 9) & 0x7FFFu);
                w = HANN[hi] + (((HANN[hi + 1u] - HANN[hi]) * hf) >> 15);
            }
            sum += mulq15(mulq15(s0 + (((s1 - s0) * f) >> 15), w), g->gain);
            p += g->inc;
            if (p < 0)
                p += (int32_t)(MO_GR_N << 16);
            else if (p >= (int32_t)(MO_GR_N << 16))
                p -= (int32_t)(MO_GR_N << 16);
            g->pos = p;
            was = g->wph;
            g->wph += g->winc;
            if (g->wph < was)
                g->on = 0;
        }
        wet = lpk ? svf_run(&v->dark, &dc, sum, 1) : sum;
        v->last = wet;
        out[i] = in + mulq15(wet - in, v->amt);
    }
    v->amt = mix1;
}
static __typeof__(&mo_grains) volatile mo_grains_fp = mo_grains;   /* (the flash's way into the RAM code) */

/* ------------------------------------------------------------- the block --- */
/* one block (CTL samples), Q15 into out; 0 when the voice is silent for good. mac: EDIT's macros (0: none) */
static FAST int mo_render(mo_voice_t *v, const mo_design_t *d, int32_t *out, const int8_t *mac)
{
    const uint8_t *p = d->p;
    int32_t bus[FM6_N], y[FM6_N], lv, ld, lf, dt[6] = {0, 0, 0, 0, 0, 0}, lvl[6] = {0, 0, 0, 0, 0, 0}, fb = p[FP_FB] & 7u;
    int32_t trem = Q15, gmac = 0, motion = 0;
    uint32_t i, lfoq8 = 256;
    const mo_gr_t *gr = d->gr.mode ? &d->gr : 0;
    int fm_quiet;
    if (!v->live)
        return 0;
    v->blocks++;
    if (mac && (mac[0] | mac[1] | mac[2] | mac[3])) {
        int8_t felt[NMAC];
        for (i = 0; i < NMAC; i++) {                     /* (the knob's turn, felt early: its square root) */
            int32_t m = mac[i], a = m < 0 ? -m : m, q = (int32_t)isqrt32((uint32_t)(a * MAC_MAX));
            felt[i] = (int8_t)(m < 0 ? -q : q);
        }
        mo_macros(d, felt, dt, lvl, &fb, &lfoq8);
        if (felt[3] > 0) {                               /* a tremolo of its own, 1.5 .. 6 Hz, deeper as it turns */
            v->tph += 4380000u + (uint32_t)felt[3] * 205000u;
            trem = Q15 - mulq15(felt[3] * (Q15 * 7 / 10 / MAC_MAX), (sin_ph(v->tph) + Q15) >> 1);
        }
        motion = felt[3];
        gmac = felt[2];
        if (!gr && gmac > 0)
            gr = &d->gr_def;                             /* a cloud for a sound that has none */
    }
    /* the operators: skipped once the cloud is frozen and nothing dry is heard */
    if (v->frozen && gr && gr->mix >= 100 && gmac >= 0) {
        for (i = 0; i < FM6_N; i++)
            y[i] = 0;
        v->fm_done = 1;
    } else {
        if (!v->n.down && !v->fm_done && fm6_note_done(&v->n, p, p[FP_ALG] & 31u))
            v->fm_done = 1;
        lv = fm6_lfo_sample(&v->lfo);
        ld = fm6_lfo_delay(&v->lfo);
        lf = fm6_note_logfreq(v->note * 16, 0);
        fm_quiet = v->fm_done || !fm6_note_compute(&v->n, p, bus, lv, ld, lf, p[FP_ALG] & 31u, fb, dt, lvl, lfoq8, d->ws);
        if (fm_quiet) {
            for (i = 0; i < FM6_N; i++)
                y[i] = 0;
        } else {
            for (i = 0; i < FM6_N; i++) {
                int32_t x = bus[i] - v->n.dc;            /* Q24 */
                v->n.dc += x >> 10;                      /* a one-pole high-pass, ~7 Hz: FM puts sidebands at 0 Hz */
                y[i] = (int32_t)(((int64_t)x * d->gain) >> 21);   /* Q24 x Q12 -> Q15 */
                if (trem != Q15)
                    y[i] = mulq15(y[i], trem);
            }
        }
    }
    mo_grains(v, gr, y, out, gmac, motion);
    /* alive? the operators sounding; else the cloud, until it has let go */
    if (!v->fm_done)
        return 1;
    if (!gr)
        return (v->live = 0);
    if (v->frozen) {
        if (!v->n.down && !v->hold && v->genv <= 0)
            v->live = 0;
        return v->live;
    }
    if (!v->tail) {
        v->tail = ms_blocks((uint32_t)gr->back_ms + gr->spread_ms + gr->size_ms) + 64u;
        if (gr->mode == GM_CLOUD && gr->fb)
            v->tail += ms_blocks(2000u);                 /* (fed back: the smear goes on) */
    } else if (!--v->tail || (!v->n.down && !v->hold && v->genv <= 0))
        v->live = 0;
    return v->live;
}
static __typeof__(&mo_render) volatile mo_render_fp = mo_render;   /* (the flash's way into the RAM code) */

/* the sound's own envelope as A D S R (the ENVELOPE page, as drawn): its loudest carrier at middle C, timed --
 * a[0] the first stage, a[1] the second and third, a[2] the third's level (0 .. 100 % of the highest), a[3] the
 * release to 60 dB below it; ms. (Tonitor's fm_env_adsr; a cloud's attack and release are added to it) */
static void mo_env_adsr(const mo_design_t *d, int32_t *a)
{
    const uint8_t *p = d->p, *op;
    uint32_t car = fm6_carriers(p[FP_ALG] & 31u), k, best = 0, n;
    int32_t bl = -1, note = 60 + p[FP_TRNSP] - 24, ol, peak = 0, lv;
    fm6_env_t e;
    for (k = 0; k < 6u; k++)
        if ((car >> k) & 1u && (int32_t)p[k * FP_OP + FP_OL] > bl) {
            bl = p[k * FP_OP + FP_OL];
            best = k;
        }
    op = p + best * FP_OP;
    ol = fm6_scaleout(op[FP_OL]) + fm6_scale_level(note, op);
    ol = ((ol > 127 ? 127 : ol) << 5) + fm6_scale_vel(MO_VEL, op[FP_KVS]);
    fm6_env_init(&e, op, ol < 0 ? 0 : ol, fm6_scale_rate(note, op[FP_RS]));
    for (n = 0; e.ix == 0u && n < 14000u; n++)   /* (blocks: 0.7256 ms each) */
        peak = maxi(peak, fm6_env_get_fp(&e, op));
    a[0] = (int32_t)(n * 7256u / 10000u);
    for (n = 0; e.ix < 3u && n < 14000u; n++)
        peak = maxi(peak, fm6_env_get_fp(&e, op));
    a[1] = (int32_t)(n * 7256u / 10000u);
    peak = maxi(peak, e.level);
    a[2] = (int32_t)((100u * exp2_q16(-((peak - e.level) >> 8))) >> 16);
    fm6_env_key(&e, op, 0);
    for (n = 0; e.ix < 4u && n < 28000u; n++) {
        lv = fm6_env_get_fp(&e, op);
        if (lv <= peak - (10 << 24))
            break;
    }
    a[3] = (int32_t)(n * 7256u / 10000u);
    if (d->gr.mode) {
        a[0] = maxi(a[0], d->gr.atk_ms);
        a[3] = maxi(a[3], d->gr.rel_ms);
    }
}

/* ---------------------------------------------------------- the generator --- */
/* operator n (1..6, as the DX7 numbers them) of voice p, and its wave */
static uint8_t *mo_op(uint8_t *p, int n) { return p + (6 - n) * FP_OP; }
static fm6_wsel_t *mo_wsel(mo_design_t *d, int n) { return &d->ws[6 - n]; }

static void mo_env(uint8_t *op, int r1, int r2, int r3, int r4, int l1, int l2, int l3, int l4)
{
    op[FP_R1] = (uint8_t)clampi(r1, 0, 99);
    op[FP_R1 + 1] = (uint8_t)clampi(r2, 0, 99);
    op[FP_R1 + 2] = (uint8_t)clampi(r3, 0, 99);
    op[FP_R1 + 3] = (uint8_t)clampi(r4, 0, 99);
    op[FP_L1] = (uint8_t)clampi(l1, 0, 99);
    op[FP_L1 + 1] = (uint8_t)clampi(l2, 0, 99);
    op[FP_L1 + 2] = (uint8_t)clampi(l3, 0, 99);
    op[FP_L1 + 3] = (uint8_t)clampi(l4, 0, 99);
}
/* a ratio as the DX7 has it: coarse (0 = 0.5, 1..31) x (1 + fine / 100); x in hundredths */
static void mo_ratio(uint8_t *op, int32_t x100)
{
    int32_t c, f;
    op[FP_MODE] = 0;
    if (x100 < 100) {
        op[FP_FC] = 0;
        op[FP_FF] = (uint8_t)clampi((x100 - 50) * 2, 0, 99);
        return;
    }
    c = clampi(x100 / 100, 1, 31);
    f = (x100 / c) - 100;
    if (f > 99) {
        c = clampi(c + 1, 1, 31);
        f = (x100 / c) - 100;
    }
    op[FP_FC] = (uint8_t)c;
    op[FP_FF] = (uint8_t)clampi(f, 0, 99);
}
/* a fixed frequency: 10^decade x 10^(fine / 100) Hz (decade 0..3: 1, 10, 100, 1000 Hz) */
static void mo_fixed(uint8_t *op, int decade, int fine)
{
    op[FP_MODE] = 1;
    op[FP_FC] = (uint8_t)clampi(decade, 0, 3);
    op[FP_FF] = (uint8_t)clampi(fine, 0, 99);
}
static int mo_is_carrier(const uint8_t *p, int n) { return (mo_carriers_of(p) >> (6 - n)) & 1u; }
static int mo_ncarriers(const uint8_t *p)
{
    int n, c = 0;
    for (n = 1; n <= 6; n++)
        c += mo_is_carrier(p, n);
    return c;
}
/* an algorithm with lo .. hi carriers, drawn */
static uint8_t mo_alg(rnd_t *r, int lo, int hi)
{
    uint8_t cand[32];
    uint32_t a, n = 0, k;
    for (a = 0; a < 32u; a++) {
        int c = 0;
        for (k = 0; k < 6u; k++)
            c += (fm6_carriers(a) >> k) & 1u;
        if (c >= lo && c <= hi)
            cand[n++] = (uint8_t)a;
    }
    return n ? cand[rnd_below(r, (int32_t)n)] : 0;
}
/* the voice's defaults: no pitch envelope, a quiet LFO, no transpose, every operator the sine, straight */
static void mo_defaults(mo_design_t *d)
{
    uint8_t *p = d->p;
    int i;
    memset(p, 0, FP_SIZE + 1u);
    for (i = 0; i < 4; i++) {
        p[FP_PR1 + i] = 99;
        p[FP_PL1 + i] = 50;
    }
    p[FP_OKS] = 1;
    p[FP_LFS] = 35;
    p[FP_LFW] = 4;
    p[FP_LPMS] = 3;
    p[FP_TRNSP] = 24;
    for (i = 1; i <= 6; i++) {
        uint8_t *op = mo_op(p, i);
        mo_env(op, 99, 99, 99, 99, 99, 99, 99, 0);
        op[FP_BP] = 39;
        op[FP_DET] = 7;
        op[FP_FC] = 1;
        op[FP_OL] = 0;
    }
    for (i = 0; i < 6; i++) {
        d->ws[i].wave = 0;
        d->ws[i].morph = 0;
        d->ws[i].pd = 128;
        d->ws[i].lvup = 0;
        d->ws[i].flags = 0;
    }
}
/* the inharmonic ratios of bells (x 100) */
static const uint16_t INHARM[12] = {100, 141, 173, 224, 283, 337, 397, 449, 527, 561, 679, 741};
/* a bell wave for a carrier: one of the BELL bank, morphed a little towards the next */
static void mo_bellwave(fm6_wsel_t *w, rnd_t *r)
{
    w->wave = (uint8_t)(WV_BELL0 + 1u + rnd_below(r, (int32_t)WV_BELL_N));
    w->morph = (uint8_t)(rnd_chance(r, 50) ? rnd_in(r, 0, 160) : 0);
}

static void mo_body_metal(mo_design_t *d, rnd_t *r)
{
    uint8_t *p = d->p;
    int n, base = rnd_below(r, 3), ol = rnd_in(r, 85, 99);
    p[FP_ALG] = mo_alg(r, 4, 6);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = mo_op(p, n);
        if (mo_is_carrier(p, n)) {
            int32_t ratio = INHARM[clampi(base + rnd_below(r, 6), 0, 11)] * (rnd_chance(r, 30) ? 2 : 1);
            int high = ratio > 300;
            mo_ratio(op, ratio);
            op[FP_OL] = (uint8_t)clampi(ol - (ratio - 100) / 25 - rnd_below(r, 8), 40, 99);
            mo_env(op, 99, rnd_in(r, 25, 45) + high * 15, rnd_in(r, 15, 35) + high * 10, rnd_in(r, 35, 60), 99,
                   rnd_in(r, 60, 85), rnd_in(r, 0, 30), 0);
            if (rnd_chance(r, 35))
                mo_bellwave(mo_wsel(d, n), r);
            op[FP_DET] = (uint8_t)rnd_in(r, 3, 11);
        } else {                                         /* a modulator: a low index, dying with the strike */
            mo_ratio(op, rnd_chance(r, 50) ? 141 : rnd_in(r, 300, 700));
            op[FP_OL] = (uint8_t)rnd_in(r, 45, 70);
            mo_env(op, 99, rnd_in(r, 50, 80), 30, 60, 99, rnd_in(r, 0, 40), 0, 0);
        }
    }
    p[FP_FB] = (uint8_t)rnd_in(r, 0, 3);
}
static void mo_body_drone(mo_design_t *d, rnd_t *r)
{
    uint8_t *p = d->p;
    static const uint16_t CR[6] = {50, 100, 100, 200, 300, 150};
    int pair;
    p[FP_ALG] = (uint8_t)(rnd_chance(r, 50) ? 4 : 5);  /* three pairs */
    for (pair = 0; pair < 3; pair++) {
        int c = 1 + pair * 2, m = c + 1;
        uint8_t *oc = mo_op(p, c), *om = mo_op(p, m);
        int32_t cr = CR[rnd_below(r, 6)];
        mo_ratio(oc, cr);
        oc[FP_DET] = (uint8_t)rnd_in(r, 0, 14);
        oc[FP_OL] = (uint8_t)rnd_in(r, 80, 99);
        mo_env(oc, rnd_in(r, 20, 50), rnd_in(r, 10, 30), 0, rnd_in(r, 24, 40), 99, rnd_in(r, 85, 99), rnd_in(r, 85, 99), 0);
        mo_ratio(om, cr * (1 + rnd_below(r, 3)) + (rnd_chance(r, 40) ? rnd_in(r, 1, 3) : 0));
        om[FP_OL] = (uint8_t)rnd_in(r, 35, 70);
        om[FP_AMS] = (uint8_t)rnd_in(r, 1, 3);
        mo_env(om, rnd_in(r, 15, 45), rnd_in(r, 10, 30), 0, 40, 99, rnd_in(r, 60, 99), rnd_in(r, 60, 99), 0);
        if (rnd_chance(r, 30))
            mo_bellwave(mo_wsel(d, c), r);
    }
    p[FP_LFS] = (uint8_t)rnd_in(r, 5, 30);
    p[FP_LFW] = (uint8_t)(rnd_chance(r, 70) ? 4 : 0);
    p[FP_LAMD] = (uint8_t)rnd_in(r, 20, 60);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 0, 12);
    p[FP_LPMS] = 2;
    p[FP_LFD] = (uint8_t)rnd_in(r, 0, 60);
    p[FP_FB] = (uint8_t)rnd_in(r, 0, 4);
}
static void mo_body_bruit(mo_design_t *d, rnd_t *r)
{
    uint8_t *p = d->p;
    static const uint8_t ALG[6] = {0, 1, 2, 3, 6, 15};
    int n;
    p[FP_ALG] = ALG[rnd_below(r, 6)];
    p[FP_FB] = 7;
    for (n = 1; n <= 6; n++) {
        uint8_t *op = mo_op(p, n);
        if (mo_is_carrier(p, n)) {
            mo_ratio(op, rnd_chance(r, 50) ? 100 : INHARM[rnd_below(r, 6)]);
            op[FP_OL] = (uint8_t)rnd_in(r, 85, 99);
            mo_env(op, rnd_in(r, 60, 99), rnd_in(r, 20, 60), 0, rnd_in(r, 40, 70), 99, rnd_in(r, 70, 99), rnd_in(r, 50, 99), 0);
        } else {
            if (rnd_chance(r, 30))
                mo_fixed(op, rnd_in(r, 2, 3), rnd_in(r, 0, 99));
            else
                mo_ratio(op, rnd_in(r, 700, 1900) + rnd_in(r, 0, 99));
            op[FP_OL] = (uint8_t)rnd_in(r, 80, 99);
            mo_env(op, 99, rnd_in(r, 30, 80), 0, 60, 99, rnd_in(r, 60, 99), rnd_in(r, 40, 99), 0);
            if (rnd_chance(r, 30))
                mo_wsel(d, n)->wave = (uint8_t)(WV_PRIM0 + 3u + rnd_below(r, 2));   /* a saw or a square as a modulator */
        }
    }
    p[FP_LFW] = 5;                                       /* sample and hold on the pitch */
    p[FP_LFS] = (uint8_t)rnd_in(r, 60, 99);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 40, 99);
    p[FP_LPMS] = (uint8_t)rnd_in(r, 4, 7);
}
static void mo_body_impact(mo_design_t *d, rnd_t *r)
{
    uint8_t *p = d->p;
    int n, fall = rnd_in(r, 60, 99);
    p[FP_ALG] = mo_alg(r, 1, 2);
    p[FP_PL1] = (uint8_t)rnd_in(r, 65, 99);
    p[FP_PR1] = 99;
    p[FP_PL1 + 1] = 50;
    p[FP_PR1 + 1] = (uint8_t)rnd_in(r, 55, 90);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = mo_op(p, n);
        if (mo_is_carrier(p, n)) {
            mo_ratio(op, rnd_chance(r, 60) ? 100 : INHARM[rnd_below(r, 4)]);
            op[FP_OL] = (uint8_t)rnd_in(r, 90, 99);
            mo_env(op, 99, rnd_in(r, 30, 60), rnd_in(r, 20, 50), rnd_in(r, 50, 80), 99, rnd_in(r, 40, 80), 0, 0);
            if (rnd_chance(r, 30))
                mo_bellwave(mo_wsel(d, n), r);
        } else {
            mo_ratio(op, rnd_chance(r, 50) ? rnd_in(r, 100, 400) : INHARM[rnd_below(r, 12)]);
            op[FP_OL] = (uint8_t)rnd_in(r, 75, 99);
            mo_env(op, 99, fall + rnd_in(r, -10, 0), 40, 70, 99, rnd_in(r, 0, 30), 0, 0);
        }
    }
    p[FP_FB] = (uint8_t)rnd_in(r, 2, 7);
}
static void mo_body_pulse(mo_design_t *d, rnd_t *r)
{
    uint8_t *p = d->p;
    int n;
    p[FP_ALG] = mo_alg(r, 1, 3);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = mo_op(p, n);
        if (mo_is_carrier(p, n)) {
            mo_ratio(op, rnd_chance(r, 70) ? 100 * (1 + rnd_below(r, 3)) : INHARM[rnd_below(r, 6)]);
            op[FP_OL] = (uint8_t)rnd_in(r, 85, 99);
            mo_env(op, rnd_in(r, 50, 99), 30, 0, rnd_in(r, 40, 70), 99, 99, rnd_in(r, 70, 99), 0);
        } else {
            mo_ratio(op, rnd_in(r, 100, 600));
            op[FP_OL] = (uint8_t)rnd_in(r, 40, 80);
            mo_env(op, 99, rnd_in(r, 20, 60), 0, 50, 99, rnd_in(r, 50, 99), rnd_in(r, 30, 90), 0);
        }
        op[FP_AMS] = 3;
    }
    p[FP_LFW] = (uint8_t)(rnd_chance(r, 60) ? 3 : 5);   /* square or sample-and-hold */
    p[FP_LFS] = (uint8_t)rnd_in(r, 40, 78);             /* ~2 .. 12 Hz */
    p[FP_LAMD] = 99;
    p[FP_LPMD] = (uint8_t)(rnd_chance(r, 30) ? rnd_in(r, 5, 30) : 0);
    p[FP_LFD] = 0;
    p[FP_FB] = (uint8_t)rnd_in(r, 0, 5);
}
static void mo_body_glass(mo_design_t *d, rnd_t *r)
{
    uint8_t *p = d->p;
    static const uint16_t CR[6] = {100, 200, 301, 499, 150, 400};
    int n;
    p[FP_ALG] = mo_alg(r, 2, 3);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = mo_op(p, n);
        if (mo_is_carrier(p, n)) {
            fm6_wsel_t *w = mo_wsel(d, n);
            mo_ratio(op, CR[rnd_below(r, 6)]);
            op[FP_OL] = (uint8_t)rnd_in(r, 80, 99);
            op[FP_DET] = (uint8_t)rnd_in(r, 4, 10);
            mo_env(op, rnd_in(r, 55, 90), rnd_in(r, 15, 35), rnd_in(r, 5, 20), rnd_in(r, 30, 55), 99, rnd_in(r, 70, 95), rnd_in(r, 20, 60), 0);
            mo_bellwave(w, r);
            w->morph = (uint8_t)rnd_in(r, 0, 255);
            if (rnd_chance(r, 50)) {
                w->pd = (uint8_t)rnd_in(r, 100, 156);
                w->flags = FW_PDENV;
            }
        } else {
            mo_ratio(op, rnd_in(r, 100, 400));
            op[FP_OL] = (uint8_t)rnd_in(r, 25, 55);
            mo_env(op, 99, rnd_in(r, 30, 60), 20, 50, 99, rnd_in(r, 30, 70), rnd_in(r, 0, 40), 0);
        }
    }
    p[FP_FB] = (uint8_t)rnd_in(r, 0, 2);
}
static void mo_body_formant(mo_design_t *d, rnd_t *r)
{
    uint8_t *p = d->p;
    int n, open = rnd_chance(r, 50);
    p[FP_ALG] = mo_alg(r, 1, 2);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = mo_op(p, n);
        if (mo_is_carrier(p, n)) {
            fm6_wsel_t *w = mo_wsel(d, n);
            mo_ratio(op, rnd_chance(r, 70) ? 100 : 200);
            op[FP_OL] = 99;
            op[FP_DET] = (uint8_t)rnd_in(r, 5, 9);
            mo_env(op, rnd_in(r, 55, 99), rnd_in(r, 20, 50), 0, rnd_in(r, 40, 65), 99, rnd_in(r, 60, 99), rnd_in(r, 50, 99), 0);
            w->wave = (uint8_t)(rnd_chance(r, 60) ? 0 : WV_PRIM0 + 3u + rnd_below(r, 2));
            w->pd = (uint8_t)(open ? rnd_in(r, 170, 250) : rnd_in(r, 8, 80));
            w->flags = FW_PDENV;
        } else {
            mo_ratio(op, rnd_chance(r, 70) ? 100 : 50);
            op[FP_OL] = (uint8_t)rnd_in(r, 30, 60);
            mo_env(op, 99, rnd_in(r, 20, 50), 0, 50, 99, rnd_in(r, 50, 99), rnd_in(r, 50, 99), 0);
        }
    }
    p[FP_LFS] = (uint8_t)rnd_in(r, 20, 45);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 2, 10);
    p[FP_LFD] = (uint8_t)rnd_in(r, 40, 80);
    p[FP_FB] = (uint8_t)rnd_in(r, 0, 3);
}
static void mo_body_reed(mo_design_t *d, rnd_t *r)
{
    uint8_t *p = d->p;
    int n;
    p[FP_ALG] = mo_alg(r, 1, 2);
    p[FP_FB] = (uint8_t)rnd_in(r, 4, 7);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = mo_op(p, n);
        if (mo_is_carrier(p, n)) {
            fm6_wsel_t *w = mo_wsel(d, n);
            mo_ratio(op, rnd_chance(r, 70) ? 100 : 50);
            op[FP_OL] = (uint8_t)rnd_in(r, 85, 99);
            op[FP_DET] = (uint8_t)rnd_in(r, 2, 12);
            mo_env(op, rnd_in(r, 55, 80), rnd_in(r, 20, 40), 0, rnd_in(r, 45, 65), 99, rnd_in(r, 75, 99), rnd_in(r, 70, 99), 0);
            w->wave = (uint8_t)(WV_PRIM0 + 3u + rnd_below(r, 2));   /* a saw or a square */
            w->pd = (uint8_t)rnd_in(r, 40, 110);
            w->flags = FW_PDENV;
        } else {
            mo_ratio(op, rnd_chance(r, 60) ? 100 : 200);
            op[FP_OL] = (uint8_t)rnd_in(r, 55, 80);
            mo_env(op, rnd_in(r, 60, 90), rnd_in(r, 20, 50), 0, 60, 99, rnd_in(r, 60, 99), rnd_in(r, 50, 99), 0);
        }
    }
    p[FP_LFS] = (uint8_t)rnd_in(r, 30, 50);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 3, 12);
    p[FP_LFD] = (uint8_t)rnd_in(r, 50, 90);
}
/* generation 1: the bodies kept from bending pitch, the tonal ones roughened (dsp.h GEN_NOW) */
static void mo_gen1(mo_design_t *d, int body, rnd_t *r)
{
    uint8_t *p = d->p;
    int n;
    switch (body) {
    case MF_IMPACT:                                      /* the fall a thud: a few semitones, quick */
        p[FP_PL1] = (uint8_t)rnd_in(r, 52, 58);
        p[FP_PR1 + 1] = (uint8_t)rnd_in(r, 75, 95);
        break;
    case MF_BRUIT:                                       /* the sample-and-hold on the level more than the pitch */
        p[FP_LPMD] = (uint8_t)rnd_in(r, 5, 25);
        p[FP_LPMS] = (uint8_t)rnd_in(r, 2, 4);
        p[FP_LAMD] = (uint8_t)rnd_in(r, 40, 90);
        for (n = 1; n <= 6; n++)
            if (mo_is_carrier(p, n))
                mo_op(p, n)[FP_AMS] = 2;
        break;
    case MF_PULSE:
        p[FP_LPMD] = 0;
        break;
    case MF_DRONE:
        p[FP_LPMD] = (uint8_t)rnd_in(r, 0, 4);
        break;
    case MF_FORMANT:
    case MF_REED:
        if (p[FP_LPMD] > 6)
            p[FP_LPMD] = 6;
        if (rnd_chance(r, 50))                           /* a modulator off the harmonics: clangour */
            for (n = 1; n <= 6; n++)
                if (!mo_is_carrier(p, n)) {
                    mo_ratio(mo_op(p, n), INHARM[1 + rnd_below(r, 6)]);
                    break;
                }
        break;
    default:
        break;
    }
}
/* generation 2: every sound a texture that is held and keeps moving, nothing struck, plucked or pulsed. Applied
 * last, over the body, the path and the mutations: the carriers swell in (never a strike) and hold high; the
 * modulators get slow envelopes of their own, so the timbre opens and closes over many seconds; the LFO breathes
 * (0.1 .. 1 Hz roughly) on the modulators' levels, never more than a trace on the pitch; no pitch envelope; a
 * cloud fades in */
static void mo_gen2(mo_design_t *d, rnd_t *r)
{
    static const uint8_t LW[3] = {3, 4, 2};              /* the LFO: triangle, sine, sample-and-hold */
    static const uint8_t LWAVE[3] = {0, 4, 5};
    uint8_t *p = d->p;
    int n, i;
    for (i = 0; i < 4; i++) {
        p[FP_PR1 + i] = 99;
        p[FP_PL1 + i] = 50;
    }
    for (n = 1; n <= 6; n++) {
        uint8_t *op = mo_op(p, n);
        if (mo_is_carrier(p, n)) {
            int r1 = op[FP_R1] < 50 ? op[FP_R1] : rnd_in(r, 20, 50);
            mo_env(op, r1, rnd_in(r, 8, 35), rnd_in(r, 4, 25), rnd_in(r, 24, 42), rnd_in(r, 86, 95),
                   rnd_in(r, 90, 99), rnd_in(r, 92, 99), 0);  /* (a level a step 0.75 dB: up to the held one, not down) */
            op[FP_AMS] = 1;                              /* the level breathing a little, slowly (no tremolo) */
            op[FP_DET] = (uint8_t)(7 + ((n & 1) ? 1 : -1) * rnd_in(r, 2, 5));   /* the carriers apart: a slow beating */
        } else {
            mo_env(op, rnd_in(r, 6, 35), rnd_in(r, 4, 22), rnd_in(r, 3, 18), rnd_in(r, 25, 50), rnd_in(r, 70, 99),
                   rnd_in(r, 35, 99), rnd_in(r, 45, 95), 0);
            op[FP_AMS] = (uint8_t)rnd_in(r, 2, 3);       /* the LFO always on the modulators: the timbre moves */
            if (!op[FP_MODE] && mo_op_ratio100(op) < 800) {   /* .. deep enough to be heard (care's high ratios kept) */
                int floor = rnd_in(r, 52, 68);
                if (op[FP_OL] < floor)
                    op[FP_OL] = (uint8_t)floor;
            }
        }
    }
    p[FP_LFW] = LWAVE[rnd_weighted(r, LW, 3)];
    p[FP_LFS] = (uint8_t)rnd_in(r, 4, 26);
    p[FP_LAMD] = (uint8_t)rnd_in(r, 30, 70);
    p[FP_LFD] = (uint8_t)rnd_in(r, 0, 70);
    if (p[FP_LPMD] > 2)
        p[FP_LPMD] = 2;
    if (p[FP_LPMS] > 2)
        p[FP_LPMS] = 2;
    if (d->gr.mode && d->gr.atk_ms < 150)
        d->gr.atk_ms = (uint16_t)rnd_in(r, 150, 1200);
}
static void mo_body(mo_design_t *d, int body, rnd_t *r)
{
    switch (body) {
    case MF_METAL: mo_body_metal(d, r); break;
    case MF_DRONE: mo_body_drone(d, r); break;
    case MF_BRUIT: mo_body_bruit(d, r); break;
    case MF_IMPACT: mo_body_impact(d, r); break;
    case MF_PULSE: mo_body_pulse(d, r); break;
    case MF_GLASS: mo_body_glass(d, r); break;
    case MF_FORMANT: mo_body_formant(d, r); break;
    default: mo_body_reed(d, r); break;
    }
}

/* the clouds */
static const int8_t IV_DARK[8] = {0, 0, -12, -7, -5, -24, 7, -12};
static const int8_t IV_BRIGHT[8] = {0, 12, 7, 19, -12, 5, 24, 6};
static void mo_iv(mo_gr_t *g, const int8_t *set, rnd_t *r)
{
    int k;
    g->iv[0] = 0;
    for (k = 1; k < 4; k++)
        g->iv[k] = set[rnd_below(r, 8)];
}
static void mo_cloud(mo_gr_t *g, rnd_t *r)
{
    g->mode = GM_CLOUD;
    g->size_ms = (uint16_t)rnd_in(r, 80, 300);
    g->rate = (uint16_t)rnd_in(r, 8, 40);
    g->back_ms = (uint16_t)rnd_in(r, 50, 200);
    g->spread_ms = (uint16_t)rnd_in(r, 100, 400);
    mo_iv(g, IV_DARK, r);
    g->pch = (uint8_t)rnd_in(r, 20, 60);
    g->jit = (uint8_t)rnd_in(r, 20, 40);
    g->cents = (uint8_t)rnd_in(r, 0, 20);
    g->rev = (uint8_t)rnd_in(r, 10, 40);
    g->shape = 0;
    g->fb = (uint8_t)rnd_in(r, 0, 30);
    g->mix = (uint8_t)rnd_in(r, 50, 90);
    g->freeze_ms = (uint16_t)(rnd_chance(r, 20) ? rnd_in(r, 400, 1500) : 0);
    g->atk_ms = (uint16_t)rnd_in(r, 100, 800);
    g->rel_ms = (uint16_t)rnd_in(r, 500, 3000);
    g->dark = (uint8_t)rnd_in(r, 30, 70);
    g->drift = (uint8_t)rnd_in(r, 20, 60);
    g->burst = 8;
}
static void mo_shatter(mo_gr_t *g, rnd_t *r)
{
    g->mode = GM_CLOUD;
    g->size_ms = (uint16_t)rnd_in(r, 4, 40);
    g->rate = (uint16_t)rnd_in(r, 60, 400);
    g->back_ms = (uint16_t)rnd_in(r, 10, 50);
    g->spread_ms = (uint16_t)rnd_in(r, 50, 300);
    mo_iv(g, IV_BRIGHT, r);
    g->pch = (uint8_t)rnd_in(r, 30, 70);
    g->jit = (uint8_t)rnd_in(r, 60, 90);
    g->cents = (uint8_t)rnd_in(r, 20, 45);
    g->rev = (uint8_t)rnd_in(r, 30, 50);
    g->shape = (uint8_t)rnd_in(r, 0, 2);
    g->fb = (uint8_t)rnd_in(r, 0, 10);
    g->mix = (uint8_t)rnd_in(r, 70, 100);
    g->freeze_ms = (uint16_t)(rnd_chance(r, 25) ? rnd_in(r, 150, 600) : 0);
    g->atk_ms = (uint16_t)rnd_in(r, 0, 50);
    g->rel_ms = (uint16_t)rnd_in(r, 100, 500);
    g->dark = (uint8_t)rnd_in(r, 0, 20);
    g->drift = (uint8_t)rnd_in(r, 0, 30);
    g->burst = 8;
}
static void mo_pulsar(mo_gr_t *g, rnd_t *r)
{
    static const uint8_t RATE[8] = {2, 4, 4, 4, 8, 8, 12, 16};   /* a half, one, two, three, four grains a period */
    g->mode = GM_PULSAR;
    g->size_ms = (uint16_t)rnd_in(r, 2, 15);
    g->rate = RATE[rnd_below(r, 8)];
    g->back_ms = (uint16_t)rnd_in(r, 5, 30);
    g->spread_ms = (uint16_t)rnd_in(r, 0, 5);
    mo_iv(g, IV_BRIGHT, r);
    g->pch = (uint8_t)(rnd_chance(r, 30) ? rnd_in(r, 5, 25) : 0);
    g->jit = (uint8_t)rnd_in(r, 0, 30);                  /* the drops */
    g->cents = (uint8_t)rnd_in(r, 0, 10);
    g->rev = (uint8_t)rnd_in(r, 0, 10);
    g->shape = (uint8_t)(rnd_chance(r, 30) ? 1 : 0);
    g->fb = 0;
    g->mix = (uint8_t)rnd_in(r, 80, 100);
    g->freeze_ms = 0;
    g->atk_ms = (uint16_t)rnd_in(r, 0, 100);
    g->rel_ms = (uint16_t)rnd_in(r, 200, 800);
    g->dark = (uint8_t)rnd_in(r, 0, 30);
    g->drift = 0;
    g->burst = (uint8_t)rnd_in(r, 2, 8);
}
static void mo_freeze(mo_gr_t *g, rnd_t *r)
{
    g->mode = GM_CLOUD;
    g->size_ms = (uint16_t)rnd_in(r, 100, 300);
    g->rate = (uint16_t)rnd_in(r, 10, 30);
    g->back_ms = (uint16_t)rnd_in(r, 0, 100);
    g->spread_ms = (uint16_t)rnd_in(r, 200, 500);
    mo_iv(g, IV_DARK, r);
    g->pch = (uint8_t)rnd_in(r, 10, 50);
    g->jit = (uint8_t)rnd_in(r, 20, 50);
    g->cents = (uint8_t)rnd_in(r, 0, 15);
    g->rev = (uint8_t)rnd_in(r, 20, 50);
    g->shape = 0;
    g->fb = (uint8_t)rnd_in(r, 20, 50);
    g->mix = 100;
    g->freeze_ms = (uint16_t)rnd_in(r, 150, 800);
    g->atk_ms = 0;
    g->rel_ms = (uint16_t)rnd_in(r, 1000, 6000);
    g->dark = (uint8_t)rnd_in(r, 20, 60);
    g->drift = (uint8_t)rnd_in(r, 20, 60);
    g->burst = 8;
}

/* care: modulators on high ratios turned down (their sidebands alias and scream), the brightness scaled down the
 * keyboard, rate scaling for the carriers */
static void mo_care(mo_design_t *d, rnd_t *r)
{
    uint8_t *p = d->p;
    int n;
    for (n = 1; n <= 6; n++) {
        uint8_t *op = mo_op(p, n);
        int32_t ratio = mo_op_ratio100(op);
        op[FP_BP] = 39;
        if (mo_is_carrier(p, n)) {
            op[FP_RS] = (uint8_t)rnd_in(r, 1, 3);
        } else {
            if (!op[FP_MODE] && ratio >= 800)
                op[FP_OL] = (uint8_t)clampi(op[FP_OL] - (ratio - 800) / 50, 0, 99);
            op[FP_RC] = 0;                               /* -LIN above the breakpoint: the index down with the pitch */
            op[FP_RD] = (uint8_t)rnd_in(r, 15, 35);
        }
    }
}
/* the paths' leanings */
static void mo_ambient(mo_design_t *d, rnd_t *r)
{
    uint8_t *p = d->p;
    int n, slow = rnd_in(r, 10, 30);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = mo_op(p, n);
        if (mo_is_carrier(p, n)) {
            op[FP_R1] = (uint8_t)clampi(op[FP_R1] - slow, 15, 99);
            op[FP_R1 + 3] = (uint8_t)clampi(op[FP_R1 + 3] - slow / 2, 22, 99);   /* (22: a release of ~20 s at most) */
        } else
            op[FP_OL] = (uint8_t)clampi(op[FP_OL] - rnd_in(r, 4, 12), 0, 99);
    }
    p[FP_LFS] = (uint8_t)clampi(p[FP_LFS] - 15, 0, 99);
    if (p[FP_LFW] == 5)
        p[FP_LPMD] = (uint8_t)(p[FP_LPMD] / 2);
    if (rnd_chance(r, 30))
        p[FP_TRNSP] = 12;
    if (d->gr.mode) {
        d->gr.dark = (uint8_t)clampi(d->gr.dark + 15, 0, 100);
        d->gr.rel_ms = (uint16_t)clampi(d->gr.rel_ms * 3 / 2, 0, 8000);
    }
}
static void mo_experimental(mo_design_t *d, rnd_t *r)
{
    uint8_t *p = d->p;
    int n;
    for (n = 1; n <= 6; n++) {
        uint8_t *op = mo_op(p, n);
        if (!mo_is_carrier(p, n))
            op[FP_OL] = (uint8_t)clampi(op[FP_OL] + rnd_in(r, 3, 10), 0, 99);
    }
    p[FP_FB] = (uint8_t)clampi(p[FP_FB] + rnd_in(r, 1, 3), 0, 7);
}
/* a mutation: something bent without breaking the sound */
static void mo_mutate(mo_design_t *d, rnd_t *r)
{
    uint8_t *p = d->p;
    int n = 1 + rnd_below(r, 6);
    uint8_t *op = mo_op(p, n);
    switch (rnd_below(r, 6)) {
    case 0:                                              /* a partial nailed to a frequency */
        if (!mo_is_carrier(p, n) || mo_ncarriers(p) > 1)
            mo_fixed(op, rnd_in(r, 1, 3), rnd_in(r, 0, 99));
        break;
    case 1: {                                            /* an envelope turned over */
        uint8_t t = op[FP_R1];
        op[FP_R1] = op[FP_R1 + 1];
        op[FP_R1 + 1] = t;
        break;
    }
    case 2:                                              /* the feedback pushed */
        p[FP_FB] = (uint8_t)clampi(p[FP_FB] + 3, 0, 7);
        break;
    case 3:                                              /* an octave away */
        if (op[FP_FC] >= 2 && !op[FP_MODE])
            op[FP_FC] = (uint8_t)(op[FP_FC] / 2);
        else if (!op[FP_MODE])
            op[FP_FC] = (uint8_t)clampi(op[FP_FC] * 2, 1, 31);
        break;
    case 4: {                                            /* a wave in place of the sine */
        fm6_wsel_t *w = mo_wsel(d, n);
        w->wave = (uint8_t)(1u + rnd_below(r, (int32_t)WT_N));
        w->morph = (uint8_t)rnd_in(r, 0, 200);
        break;
    }
    default:                                             /* a bend */
        mo_wsel(d, n)->pd = (uint8_t)(rnd_chance(r, 50) ? rnd_in(r, 20, 90) : rnd_in(r, 170, 240));
        mo_wsel(d, n)->flags = (uint8_t)(rnd_chance(r, 50) ? FW_PDENV : 0);
        break;
    }
}
/* every byte of the 155-byte voice inside its range (from SLOOP's eng_fm6.c, Leo Kuroshita, GPL-3.0) */
static uint32_t fm6_max(uint32_t i)
{
    static const uint8_t OPMAX[21] = {99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 3, 3, 7, 3, 7, 99, 1, 31, 99, 14};
    static const uint8_t VMAX[19] = {99, 99, 99, 99, 99, 99, 99, 99, 31, 7, 1, 99, 99, 99, 99, 1, 5, 7, 48};
    if (i < 126u)
        return OPMAX[i % 21u];
    if (i < FP_NAME)
        return VMAX[i - 126u];
    return 126u;
}
static void mo_sanitize(mo_design_t *d)
{
    uint8_t *p = d->p;
    const uint8_t *a = FM6_ALG[p[FP_ALG] & 31u];
    uint32_t i;
    for (i = 0; i < FP_SIZE; i++) {
        if (i >= FP_NAME)
            p[i] = p[i] < 32u || p[i] > 126u ? ' ' : p[i];
        else if (p[i] > fm6_max(i))
            p[i] = (uint8_t)fm6_max(i);
    }
    p[FP_SIZE] = 0;
    for (i = 0; i < 6u; i++) {                           /* a modulated operator: its wave a level higher */
        d->ws[i].lvup = (uint8_t)((a[i] & 0x30u) ? 1u : 0u);
        if (d->ws[i].wave > WT_N)
            d->ws[i].wave = (uint8_t)WT_N;
    }
}

/* a sound of family f (-1: any, by the path's weights), from seed, on a path; then mo_measure */
static void mo_design(mo_design_t *d, int32_t f, uint32_t seed, uint32_t path, uint32_t gen)
{
    rnd_t rr, *r = &rr;
    int body;
    memset(d, 0, sizeof *d);
    rnd_seed(r, seed ^ 0x4D4F4C45u);
    path = path >= PATH_N ? PATH_OPEN : path;
    if (f < 0 || f >= MF_N)
        f = (int32_t)rnd_weighted(r, mo_fam_w(path, gen), MF_N);
    d->family = (uint8_t)f;
    d->seed = seed;
    d->path = (uint8_t)path;
    d->gen = (uint8_t)gen;
    d->gain = 4096;
    mo_defaults(d);
    if (f < MF_BODY_N)
        body = f;
    else if (gen >= 2u)
        body = (int)rnd_weighted(r, MF_BODY_G2[path], MF_BODY_N);
    else if (f == MF_PULSAR)
        body = (int)rnd_weighted(r, MF_BODY_SUST[path], MF_BODY_N);
    else if (f == MF_FREEZE)
        body = (int)rnd_weighted(r, MF_BODY_STRUCK[path], MF_BODY_N);
    else
        body = (int)rnd_weighted(r, MF_BODY_ANY[path], MF_BODY_N);
    d->body = (uint8_t)body;
    mo_body(d, body, r);
    if (gen >= 1u)
        mo_gen1(d, body, r);
    switch (f) {
    case MF_CLOUD: mo_cloud(&d->gr, r); break;
    case MF_SHATTER: mo_shatter(&d->gr, r); break;
    case MF_PULSAR: mo_pulsar(&d->gr, r); break;
    case MF_FREEZE: mo_freeze(&d->gr, r); break;
    default: break;
    }
    mo_cloud(&d->gr_def, r);                             /* (the cloud GRAIN gives a sound that has none) */
    d->gr_def.freeze_ms = 0;
    d->gr_def.mix = 100;
    d->gr_def.atk_ms = 20;
    if (path == PATH_AMBIENT) {
        mo_ambient(d, r);
        if (rnd_chance(r, 15))
            mo_mutate(d, r);
    } else if (path == PATH_EXP) {
        mo_experimental(d, r);
        if (rnd_chance(r, 60))
            mo_mutate(d, r);
        if (rnd_chance(r, 30))
            mo_mutate(d, r);
    } else if (rnd_chance(r, 33))
        mo_mutate(d, r);
    mo_care(d, r);
    if (gen >= 2u)
        mo_gen2(d, r);
    mo_sanitize(d);
    if (gen >= 1u && d->gr.freeze_ms) {                  /* (generation 1: never frozen before the body has sounded) */
        int32_t a[4], fz;
        mo_env_adsr(d, a);
        fz = clampi(a[0] + 150, 0, 3000);
        if (d->gr.freeze_ms < fz)
            d->gr.freeze_ms = (uint16_t)fz;
    }
}

/* the sound rendered offline (the main loop) for 2.5 s at middle C, its loudest 2048-sample window brought to
 * MO_TARGET_RMS (d->gain); returns a measure of its brightness, Hz (the spectral centroid of its difference, roughly).
 * (Tonitor's fm_measure.) The probe voice is in the pool: 12 KiB */
static mo_voice_t mo_probe POOL;
static int32_t mo_measure(mo_design_t *d)
{
    mo_voice_t *v = &mo_probe;
    int32_t out[FM6_N], prev = 0;
    uint32_t b, i;
    int64_t acc = 0, e = 0, dd = 0;
    int64_t best = 0;
    d->gain = 4096;
    memset(v, 0, sizeof *v - MO_GR_N);                 /* (the buffer need not be cleared: filled starts at 0) */
    mo_note_on(v, d, 60, 1);
    for (b = 0; b < 3445u; b++) {          /* 2.5 s of blocks */
        if (!mo_render_fp(v, d, out, 0))
            break;
        for (i = 0; i < FM6_N; i++) {
            int32_t dx = out[i] - prev;
            acc += ((int64_t)out[i] * out[i]) >> 8;
            dd += ((int64_t)dx * dx) >> 8;
            prev = out[i];
        }
        if ((b & 63u) == 63u) {           /* windows of 64 blocks (2048 samples) */
            e += acc;
            if (acc > best)
                best = acc;
            acc = 0;
        }
#ifndef HOST
        if ((b & 255u) == 255u)
            fm1_wdt_feed();
#endif
    }
    if (acc > best)
        best = acc;
    {
        uint32_t ms = (uint32_t)(best >> 11);              /* the window's mean square (2048 samples), Q22 */
        uint32_t rms = isqrt32(ms) << 4;                   /* Q11 -> Q15 */
        int32_t g = rms ? (int32_t)((uint32_t)MO_TARGET_RMS * 4096u / rms) : 4096 * 8;
        d->gain = clampi(g, 300, 4096 * 6);               /* at most +15.6 dB: a slow swell stays a swell */
    }
    e += acc;
    while (e > 0xFFFF) {                                  /* (no 64-bit division: both brought into 32 bits) */
        e >>= 1;
        dd >>= 1;
    }
    if (e < 16)
        return 0;                                         /* silent */
    {
        uint32_t q12 = (uint32_t)(dd * 4096) / (uint32_t)e;
        return (int32_t)(7019u * isqrt32(q12 << 8) >> 10);   /* 44100 / 2 pi x sqrt(q12 / 4096): Hz */
    }
}
