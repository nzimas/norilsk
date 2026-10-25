/* SPDX-License-Identifier: GPL-3.0-only
 * NORILSK: MOLECULE, the engine -- six-operator FM with waves in its operators and a grain cloud on its
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
       MF_CLOUD = MF_BODY_N, MF_SHATTER, MF_PULSAR, MF_FREEZE,
       MF_G3, MF_FURNACE = MF_G3, MF_HUM, MF_STATIC, MF_HOWL, MF_SCRAPE, MF_DATA, MF_ASH, MF_TUNNEL, MF_N };
#define MF_N_OLD MF_G3           /* the families generations 0 .. 2 know */
static const char *const MF_NAME[MF_N] = {"METAL", "DRONE", "BRUIT", "IMPACT", "PULSE", "GLASS", "FORMANT", "REED",
                                          "CLOUD", "SHATTER", "PULSAR", "FREEZE",
                                          "FURNACE", "HUM", "STATIC", "HOWL", "SCRAPE", "DATA", "ASH", "TUNNEL"};
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
/* generation 3: the new families (FURNACE .. TUNNEL) as likely as the old ones together, leaning by path */
static const uint8_t MF_W3[PATH_N][MF_N] = {
    {2, 3, 2, 0, 0, 2, 1, 1, 3, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    {2, 4, 1, 0, 0, 4, 2, 1, 4, 1, 2, 3, 2, 3, 1, 4, 1, 1, 5, 6},
    {2, 1, 4, 0, 0, 1, 2, 1, 2, 3, 3, 2, 5, 3, 6, 3, 5, 6, 2, 1},
};
static const uint8_t *mo_fam_w(uint32_t path, uint32_t gen)
{
    path = path >= PATH_N ? PATH_OPEN : path;
    return gen >= 3u ? MF_W3[path] : gen >= 2u ? MF_W2[path] : MF_W[path];
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
    /* generation 3 (zero before it, and nothing below runs then) */
    struct {                     /* the invisible hand (Anima's): what moves what */
        uint8_t on, n;           /* on; maps in use */
        uint8_t travel, density; /* the finger's speed, the ghost's frequency: 0 .. 100 */
        uint16_t pace;           /* the hand's time, Q8 (256: as Anima's) */
        uint8_t rsv[2];
        struct { uint8_t src, dst; int8_t lo, hi; } map[5];   /* a controller to a destination, lo .. hi % of its swing */
    } hand;
    struct {                     /* the grit (Russolo's): after the operators, before the grains */
        uint8_t bits;            /* 0 off; else the bits kept, 3 .. 10 */
        uint8_t hold;            /* samples held more: 0 .. 7 (a lower rate, a small DAC's) */
        uint8_t logic;           /* XOR with a triangle: its depth 0 .. 100 */
        uint8_t lratio;          /* .. the triangle's ratio to the note, x 8 */
        uint8_t stut;            /* stutters: chance a second x 10, 0 none */
        uint8_t stut_ms;         /* .. a loop's length, 5 .. 120 ms */
        uint8_t bend;            /* grains read with an address bit bent, % */
        uint8_t mix;             /* the crushed and logical share, 0 .. 100 */
    } grit;
} mo_design_t;
/* the hand's controllers (Anima's) and its destinations here */
enum { HS_X, HS_Y, HS_SIZE, HS_SPEED, HS_ANGLE, HS_DIST, HS_AGE, HS_GX, HS_GY, HS_COUNT, HS_N };
enum { HD_BRIGHT, HD_GRAIN, HD_LEVEL, HD_GRIT, HD_GLITCH, HD_DARK, HD_N };

#define MO_GR_N 12288u           /* the grain buffer: samples at FS / 2 (0.557 s), mu-law */
#define MO_GR_MAXG 4u            /* grains at once */
#define MO_VEL 100
#define MO_TARGET_RMS 3277       /* 0.1 Q15: what a sound's loudest window is brought to */

typedef struct {
    int32_t pos, inc;            /* the read head in the buffer (Q16 samples) and its step a sample */
    uint32_t wph, winc;          /* the window's phase (a grain = 2^32) and step */
    int32_t gain;                /* Q15 */
    uint8_t on, shape;
    uint16_t bmask;              /* (generation 3) an address bit bent as it reads: Russolo's BEND */
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
    /* generation 3: the hand (two fingers: the one on the sound, a ghost), its offsets this block; the grit */
    struct mo_finger {
        int16_t x, y, px, py, size, speed, pressure;   /* Q15 */
        int16_t ax, ay, tx, ty, r, dx1, dy1, glide, gx, gy;
        int32_t nseed, t, dur, figureAt, figureFor, invFor, w, w2, phase, phase2;   /* Q16 */
        uint8_t down, path, flick, rsv;
    } f, ghost;
    int32_t ghostClock, handover, countSmooth;   /* Q16 s; Q15 */
    int16_t hoff[HD_N];          /* the hand's offsets: macro units (BRIGHT, GRAIN, DARK), Q15 (LEVEL), % (GRIT, GLITCH) */
    rnd_t motion;
    int32_t gheld, gstep;        /* the grit's held sample, samples to the next */
    uint32_t lph;                /* the logic triangle's phase */
    uint32_t stut_left, stut_len, stut_pos, stut_ph;   /* a stutter: samples to go, its loop (buffer samples), where it
                                                          * starts in the buffer, how far round (at the full rate) */
    uint8_t gbend;               /* this block's share of bent grains, % */
    uint16_t hquiet;             /* blocks let go and silent (generation 3: the hand may keep the operators from settling) */
    uint8_t g3;                  /* the sound is of generation 3 or later (the grains' fix) */
    int32_t lenv;                /* the grit's follower of the sound's level */
    uint8_t buf[MO_GR_N];
} mo_voice_t;

/* ------------------------------------------------------------ the voice --- */
static void mo_hand_start(mo_voice_t *v, const mo_design_t *d);   /* (generation 3: below) */
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
    v->g3 = (uint8_t)(d->gen >= 3u);
    if (d->gen >= 3u) {                          /* (generation 3: the hand put down, the grit from silence) */
        if (fresh && d->hand.on)
            mo_hand_start(v, d);
        v->hquiet = 0;
        if (fresh) {
            v->lenv = 0;
            v->gheld = 0;
            v->gstep = 0;
            v->lph = 0;
            v->stut_left = 0;
        }
    } else {
        v->gbend = 0;
        memset(v->hoff, 0, sizeof v->hoff);
    }
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
    if (v->g3 && rev && v->filled < MO_GR_N) {         /* (generation 3: a grain read backwards stays inside what this
                                                        * note has written -- before it, an earlier note's leftovers) */
        uint32_t lim = v->filled > bk + 24u ? v->filled - bk - 8u : 0u;
        if (dmax > lim)
            dmax = lim;
    }
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
    g->bmask = 0;
    if (v->gbend && rnd_below(&v->r, 100) < v->gbend)   /* (generation 3 only: no draw otherwise) */
        g->bmask = (uint16_t)(1u << rnd_in(&v->r, 5, 12));
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
            if (g->bmask) {                                /* (bent: the address with a bit flipped) */
                i0 ^= g->bmask;
                if (i0 >= MO_GR_N)
                    i0 -= MO_GR_N;
            }
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

/* -------------------------------------------------------------- the hand --- */
/* Generation 3: the invisible hand of Anima (Tonitor's anima.c, after Vascular's AnimaCore.h, the same author), a
 * hand on each voice: one finger on the sound while the note lasts, dragging one figure after another -- orbits,
 * figures of eight, scribbles, slow glides, now and then a flick -- pressing in slow swells, and now and then lifting
 * to land somewhere else; a ghost finger that comes and goes on a clock that will not keep time. Its controllers
 * (where the finger is, how hard it presses, how fast it moves, its angle and distance from the middle, its age;
 * where the whole hand is, how many fingers are down) move what the sound's design maps them to: its brightness, its
 * cloud, its level, its grit, its glitches, the cloud's darkness. Never its pitch. Worked out once a block. */
#define HR16(x) ((int32_t)((x) * 65536.0))
#define HLV(x) ((int32_t)((x) * 32768.0))
AINL int32_t ha_sinr(int32_t a) { return sin_ph((uint32_t)((int64_t)a * 10430)); }   /* a: Q16 radians */
AINL int32_t ha_cosr(int32_t a) { return sin_ph((uint32_t)((int64_t)a * 10430) + 0x40000000u); }
AINL int32_t ha_smooth(int32_t u) { return mulq15(mulq15(u, u), 3 * Q15 - 2 * u); }   /* smoothstep, Q15 */
AINL int32_t ha_noise1(int32_t t, int32_t seed)
{
    return mulq15(ha_sinr(mulq16(t, HR16(1.3)) + seed), HLV(0.6)) +
           mulq15(ha_sinr(mulq16(t, HR16(2.9)) + mulq16(seed, HR16(1.7))), HLV(0.3)) +
           mulq15(ha_sinr(mulq16(t, HR16(6.1)) + mulq16(seed, HR16(0.3))), HLV(0.1));
}
static FAST int32_t ha_angle(int32_t dx, int32_t dy)   /* (atan2 + pi) / 2 pi, Q15 (Anima's) */
{
    int32_t ax = absi(dx), ay = absi(dy), z, a;
    if (!ax && !ay)
        return Q15 / 2;
    z = ax > ay ? (int32_t)(((uint32_t)ay << 15) / (uint32_t)ax) : (int32_t)(((uint32_t)ax << 15) / (uint32_t)ay);
    a = mulq15(z, HLV(0.125) - mulq15(z - Q15, HLV(0.0389) + mulq15(z, HLV(0.0105))));
    if (ay > ax)
        a = Q15 / 4 - a;
    if (dx < 0)
        a = Q15 / 2 - a;
    if (dy < 0)
        a = -a;
    return clampi(a + Q15 / 2, 0, Q15);
}
/* the next figure a finger drags, from where it is (it never jumps) */
static FAST void ha_figure(mo_voice_t *v, const mo_design_t *d, struct mo_finger *f)
{
    rnd_t *r = &v->motion;
    int32_t speed, a, tr = HR16(0.05) + d->hand.travel * (HR16(0.35) / 100);
    f->ax = f->x;
    f->ay = f->y;
    f->figureAt = f->t;
    f->path = (uint8_t)rnd_below(r, 4);    /* orbit, figure, scribble, glide */
    f->flick = (uint8_t)rnd_chance(r, 20);
    f->figureFor = f->flick ? rnd_in(r, HR16(1.5), HR16(4.0)) : rnd_in(r, HR16(6.0), HR16(18.0));
    f->invFor = (int32_t)(0xFFFFFFFFu / (uint32_t)f->figureFor);
    f->r = (int16_t)(f->flick ? rnd_in(r, HLV(0.08), HLV(0.18)) : rnd_in(r, HLV(0.16), HLV(0.4)));
    speed = mulq16(tr, f->flick ? rnd_in(r, HR16(5.0), HR16(9.0)) : rnd_in(r, HR16(1.6), HR16(3.2)));
    f->w = (int32_t)(((int64_t)speed * (0xFFFFFFFFu / (uint32_t)(f->r > 64 ? f->r : 64))) >> 17);   /* speed / r */
    if (rnd_chance(r, 50))
        f->w = -f->w;
    f->w2 = rnd_chance(r, 50) ? f->w * 2 : f->w + (f->w >> 1);
    f->phase = rnd_in(r, 0, HR16(6.2831853));
    f->phase2 = rnd_in(r, 0, HR16(6.2831853));
    a = rnd_in(r, 0, HR16(6.2831853));
    f->dx1 = (int16_t)clampi(ha_cosr(a), -32767, 32767);
    f->dy1 = (int16_t)clampi(ha_sinr(a), -32767, 32767);
    f->tx = (int16_t)(f->flick ? 0 : rnd_in(r, HLV(0.15), HLV(0.85)) - f->ax);   /* (the figure carries its anchor) */
    f->ty = (int16_t)(f->flick ? 0 : rnd_in(r, HLV(0.15), HLV(0.85)) - f->ay);
}
static FAST void ha_land(mo_voice_t *v, const mo_design_t *d, struct mo_finger *f, int ghost)
{
    rnd_t *r = &v->motion;
    int was = f->down;
    int16_t ox = f->x, oy = f->y, os = f->size;
    f->down = 1;
    f->t = 0;
    f->dur = ghost ? rnd_in(r, HR16(3.0), HR16(15.0)) : 0x7FFFFFFF;
    f->x = f->px = (int16_t)rnd_in(r, HLV(0.1), HLV(0.9));
    f->y = f->py = (int16_t)rnd_in(r, HLV(0.1), HLV(0.9));
    f->pressure = (int16_t)rnd_in(r, HLV(0.55), HLV(0.9));
    f->nseed = rnd_in(r, 0, HR16(100.0));
    f->speed = 0;
    f->glide = 0;
    f->size = 0;
    if (!ghost && was) {                    /* a handover: the hand slides over from where it was (~90 ms) */
        f->glide = 32767;
        f->gx = ox;
        f->gy = oy;
        f->size = os;
    }
    ha_figure(v, d, f);
}
static FAST void ha_move(mo_voice_t *v, const mo_design_t *d, struct mo_finger *f, int ghost, int32_t dt)
{
    int32_t u, ox = 0, oy = 0, ease, carry, fade, x, y, dx, dy, sp;
    f->t += dt;
    u = f->t - f->figureAt;
    switch (f->path) {
    case 0:                                 /* an orbit */
        ox = mulq15(f->r, ha_cosr(f->phase + mulq16(f->w, u)) - ha_cosr(f->phase));
        oy = mulq15(f->r, ha_sinr(f->phase + mulq16(f->w, u)) - ha_sinr(f->phase));
        break;
    case 1:                                 /* a figure of eight */
        ox = mulq15(f->r, ha_sinr(mulq16(f->w, u) + f->phase) - ha_sinr(f->phase));
        oy = mulq15(mulq15(f->r, HLV(0.7)), ha_sinr(mulq16(f->w2, u) + f->phase2) - ha_sinr(f->phase2));
        break;
    case 2: {                               /* a scribble */
        int32_t k = (HR16(0.05) + d->hand.travel * (HR16(0.35) / 100)) * 5;
        ox = mulq15(ha_noise1(mulq16(u, k), f->nseed) - ha_noise1(0, f->nseed), HLV(0.25));
        oy = mulq15(ha_noise1(mulq16(u, k), f->nseed + HR16(40.0)) - ha_noise1(0, f->nseed + HR16(40.0)), HLV(0.25));
        break;
    }
    default: {                              /* a slow slide back and forth */
        int32_t a = mulq15(f->r + (f->r >> 1), ha_sinr(mulq16(f->w, u) >> 1));
        ox = mulq15(f->dx1, a);
        oy = mulq15(f->dy1, a);
        break;
    }
    }
    ease = ha_smooth(clampi(u >> 1, 0, Q15));             /* eased in over a second */
    carry = ha_smooth(clampi(mulq16(u, f->invFor) >> 1, 0, Q15));
    x = clampi(f->ax + mulq15(f->tx, carry) + mulq15(ox, ease), HLV(0.03), HLV(0.97));
    y = clampi(f->ay + mulq15(f->ty, carry) + mulq15(oy, ease), HLV(0.03), HLV(0.97));
    if (f->glide > 0) {
        x = f->gx + mulq15(x - f->gx, Q15 - f->glide);
        y = f->gy + mulq15(y - f->gy, Q15 - f->glide);
        f->glide = (int16_t)(f->glide > 260 ? f->glide - 260 : 0);
    }
    f->x = (int16_t)x;
    f->y = (int16_t)y;
    fade = ghost ? clampi((f->dur - f->t) >> 1, 0, Q15) : Q15;
    /* pressing harder and easing off in slow swells */
    f->size = (int16_t)mulq15(mulq15(mulq15(f->pressure, HLV(0.6) + mulq15(HLV(0.4), ha_sinr(mulq16(f->t, HR16(0.45)) + f->nseed))),
                                     clampi(f->t, 0, HR16(0.5))), fade);
    dx = f->x - f->px;
    dy = f->y - f->py;
    sp = clampi((int32_t)isqrt32((uint32_t)(dx * dx + dy * dy)) * 4594, 0, Q15);   /* (0.3 surface a second: full) */
    f->speed = (int16_t)(f->speed + (sp - f->speed) / 10);
    f->px = f->x;
    f->py = f->y;
    if (u >= f->figureFor)
        ha_figure(v, d, f);
}
/* each destination's swing at 100 % (its units: macro steps, Q15 level, %) */
static const int16_t HD_SPAN[HD_N] = {MAC_MAX, MAC_MAX / 2, Q15 / 2, 100, 100, MAC_MAX};   /* (GRAIN: half -- the cloud wetter and faster moves a sound's level too) */
/* .. and how far each may go down (BRIGHT pulled far down takes a metal's level with it; LEVEL: never under ~0.6) */
static const int16_t HD_FLOOR[HD_N] = {-MAC_MAX / 2, -MAC_MAX / 4, -Q15 * 2 / 5, -100, -100, -MAC_MAX};
/* a block of the hand: the fingers moved, the controllers read, the offsets worked out (v->hoff) */
static FAST void mo_hand(mo_voice_t *v, const mo_design_t *d)
{
    struct mo_finger *f = &v->f, *g = &v->ghost;
    int32_t c[HS_N], dt = (int32_t)(((int64_t)HR16(0.00072562) * d->hand.pace) >> 8), gx, gy, k;
    if (f->down) {
        v->handover -= dt;                  /* now and then the finger lands somewhere else (an exponential wait ~18 s) */
        if (v->handover <= 0) {
            ha_land(v, d, f, 0);
            v->handover = mulq16(HR16(18.0), mulq16(rnd_in(&v->motion, HR16(0.4), HR16(2.0)),
                                                    (int32_t)(((int64_t)(HR16(15.0) - log2_q16((uint32_t)rnd_in(&v->motion, 64, Q15))) * 45426) >> 16)));
        }
        ha_move(v, d, f, 0, dt);
    }
    v->ghostClock -= dt;                    /* the ghost: down a while, lifted again, on a clock that will not keep time */
    if (v->ghostClock <= 0) {
        int32_t dens = HR16(0.2) + d->hand.density * (HR16(0.8) / 100), mean;
        if (!g->down && f->down && d->hand.density > 45u)
            ha_land(v, d, g, 1);
        mean = (int32_t)((uint32_t)HR16(10.0) / (uint32_t)((mulq16(dens, dens) >> 8) + 1) << 8);
        mean = mulq16(mean, rnd_in(&v->motion, HR16(0.35), HR16(2.5)));
        v->ghostClock = mulq16(mean, (int32_t)(((int64_t)(HR16(15.0) - log2_q16((uint32_t)rnd_in(&v->motion, 4, Q15))) * 45426) >> 16));
    }
    if (g->down) {
        ha_move(v, d, g, 1, dt);
        if (g->t >= g->dur)
            g->down = 0;
    }
    gx = g->down ? (f->x + g->x) >> 1 : f->x;
    gy = g->down ? (f->y + g->y) >> 1 : f->y;
    v->countSmooth += ((g->down ? Q15 : 0) - v->countSmooth) / 16;
    c[HS_X] = f->x;
    c[HS_Y] = Q15 - f->y;
    c[HS_SIZE] = maxi(f->size, g->down ? g->size : 0);
    c[HS_SPEED] = maxi(f->speed, g->down ? g->speed : 0);
    c[HS_ANGLE] = ha_angle(f->x - Q15 / 2, f->y - Q15 / 2);
    c[HS_DIST] = clampi(((int32_t)isqrt32((uint32_t)((f->x - Q15 / 2) * (f->x - Q15 / 2) + (f->y - Q15 / 2) * (f->y - Q15 / 2))) * 46341) >> 15, 0, Q15);
    c[HS_AGE] = Q15 / 2 - (ha_cosr(mulq16(f->t, HR16(0.27318))) >> 1);   /* (a cycle in 23 s) */
    c[HS_GX] = gx;
    c[HS_GY] = Q15 - gy;
    c[HS_COUNT] = v->countSmooth;
    for (k = 0; k < HD_N; k++)
        v->hoff[k] = 0;
    for (k = 0; k < (int32_t)d->hand.n && k < 5; k++) {
        uint32_t src = d->hand.map[k].src, dst = d->hand.map[k].dst;
        int32_t lo, hi;
        if (src >= HS_N || dst >= HD_N)
            continue;
        lo = d->hand.map[k].lo * HD_SPAN[dst] / 100;
        hi = d->hand.map[k].hi * HD_SPAN[dst] / 100;
        v->hoff[dst] = (int16_t)clampi(v->hoff[dst] + lo + mulq15(hi - lo, c[src]), -32767, 32767);
    }
    for (k = 0; k < HD_N; k++)
        if (v->hoff[k] < HD_FLOOR[k])
            v->hoff[k] = HD_FLOOR[k];
}
static __typeof__(&ha_land) volatile ha_land_fp = ha_land;   /* (the flash's way into the RAM code) */
/* the hand put down at a fresh note (the main loop's or the ISR's note-on: flash code) */
static void mo_hand_start(mo_voice_t *v, const mo_design_t *d)
{
    memset(&v->f, 0, sizeof v->f);
    memset(&v->ghost, 0, sizeof v->ghost);
    memset(v->hoff, 0, sizeof v->hoff);
    v->countSmooth = 0;
    rnd_seed(&v->motion, d->seed ^ 0x48414E44u ^ v->w * 2654435761u);
    ha_land_fp(v, d, &v->f, 0);
    v->handover = HR16(6.0) + rnd_in(&v->motion, 0, HR16(20.0));
    v->ghostClock = rnd_in(&v->motion, HR16(1.0), HR16(8.0));
}

/* -------------------------------------------------------------- the grit --- */
/* Generation 3: Russolo's (Tonitor's russolo.c, after Robert Heel's Noisferatu), on the operators' sound before the
 * cloud: a small DAC's grit (the bits kept, the sample held a few times over), bitwise logic (the sample XOR'd with a
 * triangle at a ratio to the note: Noisferatu's logic oscillators), and stutters -- by chance, the last moment of the
 * sound (from the cloud's buffer, which always listens) looped a while, then on as it was. The crushed share is
 * mixed with the clean; the hand may move it (GRIT) and the stutters' and bent grains' chance (GLITCH). */
static FAST void mo_grit(mo_voice_t *v, const mo_design_t *d, int32_t *y)
{
    uint32_t i, sh = d->grit.bits ? 15u - clampi(d->grit.bits, 3, 10) + 1u : 0u, hold = d->grit.hold;
    int32_t mix = clampi(d->grit.mix + v->hoff[HD_GRIT], 0, 100) * (Q15 / 100), lg = d->grit.logic * (Q15 / 100);
    uint32_t linc = 0;
    int32_t stut = clampi(d->grit.stut + v->hoff[HD_GLITCH], 0, 200);
    if (lg) {                                /* the logic triangle: the note's step x ratio / 8 */
        int32_t inc24 = fm6_freq(fm6_note_logfreq(v->note * 16, 0));
        linc = (uint32_t)(((uint64_t)(uint32_t)inc24 * (d->grit.lratio ? d->grit.lratio : 8u)) << 5);   /* Q24 x 2^8 / 8 */
    }
    if (!v->stut_left && stut && rnd_below(&v->r, 13780) < stut && v->filled > 4096u) {   /* (x 10 a second) */
        uint32_t ms = clampi(d->grit.stut_ms, 5, 120);
        v->stut_len = ms_samples2(ms);
        v->stut_left = (uint32_t)rnd_in(&v->r, 3, 12) * v->stut_len * 2u;   /* (3 .. 12 rounds, at the full rate) */
        if (v->stut_left > (MO_GR_N - v->stut_len) * 2u - 64u)   /* (done before the write head comes round to it) */
            v->stut_left = (MO_GR_N - v->stut_len) * 2u - 64u;
        v->stut_pos = v->w + MO_GR_N - v->stut_len;               /* (the moment just before, fixed where it is) */
        if (v->stut_pos >= MO_GR_N)
            v->stut_pos -= MO_GR_N;
        v->stut_ph = 0;
    }
    for (i = 0; i < CTL; i++) {
        int32_t x = y[i], c = x;
        v->lenv += (absi(x) - v->lenv) >> 7;     /* (the sound's level, followed: ~2 ms) */
        if (sh || hold || lg) {
            if (v->gstep <= 0) {             /* a new sample for the small DAC */
                int32_t q = x;
                if (lg) {                    /* XOR, ten bits against ten -- its result as loud as the sound (a follower
                                              * of its level): XOR'd with near silence, a triangle would sound out */
                    int32_t t = (int32_t)(v->lph >> 22), tri = t < 512 ? t * 2 - 512 : 1535 - t * 2;
                    int32_t a = clampi(q >> 6, -512, 511) + 512, xv = ((((a ^ (tri + 512)) & 0x3FF) - 512) * 64);
                    xv = mulq15(xv, clampi(v->lenv * 2, 0, Q15));
                    q = q + mulq15(xv - q, lg);
                }
                if (sh)                      /* (towards zero: silence kept at 0, never a step below it) */
                    q = q < 0 ? -(((-q) >> sh) << sh) : (q >> sh) << sh;
                v->gheld = q;
                v->gstep = (int32_t)hold + 1;
            }
            v->gstep--;
            c = v->gheld;
            v->lph += linc;
        }
        if (mix != Q15)
            c = x + mulq15(c - x, mix);
        if (v->stut_left) {                  /* a stutter: the moment just before, looped (half-rate buffer) */
            uint32_t at = v->stut_pos + (v->stut_ph >> 1);
            if (at >= MO_GR_N)
                at -= MO_GR_N;
            c = gr_dec(v->buf[at]);
            if (++v->stut_ph >= v->stut_len * 2u)
                v->stut_ph = 0;
            v->stut_left--;
        }
        y[i] = c;
    }
}

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
    int8_t hmac[NMAC];
    int32_t hlvl = Q15;
    int mac_on = 0;
    mo_gr_t gdark;
    if (!v->live)
        return 0;
    v->blocks++;
    if (d->gen >= 3u) {                          /* generation 3: the hand's offsets on the macros, the level, the cloud */
        if (d->hand.on) {
            mo_hand(v, d);
            for (i = 0; i < NMAC; i++)           /* (BRIGHT below: the modulators alone, not the macro's carrier tilt) */
                hmac[i] = (int8_t)clampi((mac ? mac[i] : 0) + (i == 2u ? v->hoff[HD_GRAIN] : 0), -MAC_MAX, MAC_MAX);
            mac = hmac;
            hlvl = clampi(Q15 + v->hoff[HD_LEVEL], Q15 / 8, Q15);
            if (gr && v->hoff[HD_DARK]) {
                gdark = *gr;
                gdark.dark = (uint8_t)clampi(gr->dark + v->hoff[HD_DARK] * 100 / MAC_MAX, 0, 100);
                gr = &gdark;
            }
        }
        v->gbend = (uint8_t)clampi(d->grit.bend + v->hoff[HD_GLITCH], 0, 100);
    }
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
        mac_on = 1;
        if (!gr && gmac > 0)
            gr = &d->gr_def;                             /* a cloud for a sound that has none */
    }
    if (d->gen >= 3u && d->hand.on && v->hoff[HD_BRIGHT]) {   /* the hand's brightness: the modulators' index alone, as
                                                              * the macro moves them (the timbre, not the level) */
        uint32_t car = mo_carriers_of(p), k;
        int32_t hb = v->hoff[HD_BRIGHT], a = hb < 0 ? -hb : hb, q = (int32_t)isqrt32((uint32_t)(a * MAC_MAX));
        hb = hb < 0 ? -q : q;
        for (k = 0; k < 6u; k++)
            if (!((car >> k) & 1u))
                lvl[k] += hb * 1179648;
        (void)mac_on;
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
    if (d->gen >= 3u && (d->grit.bits | d->grit.hold | d->grit.logic | d->grit.stut | (uint32_t)v->hoff[HD_GLITCH]))
        mo_grit(v, d, y);                        /* (generation 3: Russolo's grit) */
    mo_grains(v, gr, y, out, gmac, motion);
    if (hlvl != Q15)                             /* (the hand's press on the level) */
        for (i = 0; i < CTL; i++)
            out[i] = mulq15(out[i], hlvl);
    if (d->gen >= 3u) {                          /* (generation 3: let go and silent a quarter second -- done) */
        int32_t pk = 0;
        for (i = 0; i < CTL; i++)
            pk = maxi(pk, absi(out[i]));
        v->hquiet = (uint16_t)(!v->n.down && !v->hold && pk < 4 ? (v->hquiet < 0xFFFFu ? v->hquiet + 1u : v->hquiet) : 0u);
        if (v->hquiet > 345u)
            return (v->live = 0);
    }
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

/* ------------------------------------------------------- generation 3 --- */
/* the new families -- ilbient, atmospheric industrial, hellscapes, abstract glitch; every one held and moving
 * (generation 2's rules apply after them): each borrows a body, then is made what it is */
static int mo_fam3_body(int32_t f, rnd_t *r)
{
    switch (f) {
    case MF_FURNACE: case MF_STATIC: return MF_BRUIT;
    case MF_HUM: case MF_SCRAPE: return MF_METAL;
    case MF_HOWL: return MF_FORMANT;
    case MF_DATA: return MF_GLASS;
    case MF_ASH: return MF_DRONE;
    default: return rnd_chance(r, 50) ? MF_GLASS : MF_DRONE;   /* TUNNEL */
    }
}
static void mo_fam3(mo_design_t *d, int32_t f, rnd_t *r)
{
    static const uint8_t LR[6] = {12, 18, 25, 43, 57, 26};   /* logic ratios x 8: 1.5, 2.25, 3.125, 5.375, 7.125, 3.25 */
    uint8_t *p = d->p;
    mo_gr_t *g = &d->gr;
    int n;
    switch (f) {
    case MF_FURNACE:                             /* a furnace: low, roaring, crushed, a dark cloud over it */
        p[FP_TRNSP] = 12;
        p[FP_FB] = 7;
        for (n = 1; n <= 6; n++)
            if (mo_is_carrier(p, n))
                mo_ratio(mo_op(p, n), rnd_chance(r, 50) ? 50 : 100);
        mo_cloud(g, r);
        g->dark = (uint8_t)rnd_in(r, 60, 90);
        g->mix = (uint8_t)rnd_in(r, 40, 70);
        d->grit.bits = (uint8_t)rnd_in(r, 7, 9);
        d->grit.hold = (uint8_t)rnd_in(r, 0, 2);
        d->grit.logic = (uint8_t)(rnd_chance(r, 30) ? rnd_in(r, 20, 40) : 0);
        d->grit.lratio = LR[rnd_below(r, 6)];
        d->grit.stut = (uint8_t)rnd_in(r, 0, 3);
        d->grit.stut_ms = (uint8_t)rnd_in(r, 20, 90);
        d->grit.mix = (uint8_t)rnd_in(r, 50, 90);
        break;
    case MF_HUM: {                               /* a machine room: carriers nailed to the mains' region, beating, XOR'd */
        int nc = 0;
        for (n = 1; n <= 6; n++)
            if (mo_is_carrier(p, n) && nc++ < 2)
                mo_fixed(mo_op(p, n), nc == 1 ? 1 : 2, nc == 1 ? rnd_in(r, 66, 80) : rnd_in(r, 0, 20));   /* ~50 Hz, ~100 Hz */
        if (rnd_chance(r, 60))
            mo_pulsar(g, r);
        if (g->mode)
            g->mix = (uint8_t)rnd_in(r, 30, 60);
        d->grit.bits = (uint8_t)rnd_in(r, 8, 10);
        d->grit.logic = (uint8_t)rnd_in(r, 30, 70);
        d->grit.lratio = LR[rnd_below(r, 6)];
        d->grit.mix = (uint8_t)rnd_in(r, 40, 80);
        break;
    }
    case MF_STATIC:                              /* static: noise from feedback, crushed hard, stuttering, shattered */
        p[FP_FB] = 7;
        for (n = 1; n <= 6; n++)
            if (!mo_is_carrier(p, n) && !mo_op(p, n)[FP_MODE])
                mo_ratio(mo_op(p, n), rnd_in(r, 1100, 1900));
        mo_shatter(g, r);
        g->mix = (uint8_t)rnd_in(r, 30, 60);
        d->grit.bits = (uint8_t)rnd_in(r, 4, 6);
        d->grit.hold = (uint8_t)rnd_in(r, 2, 6);
        d->grit.stut = (uint8_t)rnd_in(r, 5, 20);
        d->grit.stut_ms = (uint8_t)rnd_in(r, 10, 60);
        d->grit.bend = (uint8_t)rnd_in(r, 20, 50);
        d->grit.mix = (uint8_t)rnd_in(r, 80, 100);
        break;
    case MF_HOWL:                                /* a howl: formants an octave down under long dark grains, pitched down */
        p[FP_TRNSP] = 12;
        mo_cloud(g, r);
        g->size_ms = (uint16_t)rnd_in(r, 200, 400);
        g->rev = (uint8_t)rnd_in(r, 20, 40);
        g->fb = (uint8_t)rnd_in(r, 30, 50);
        g->mix = (uint8_t)rnd_in(r, 60, 90);
        g->dark = (uint8_t)rnd_in(r, 40, 80);
        if (rnd_chance(r, 40)) {
            d->grit.bits = (uint8_t)rnd_in(r, 9, 10);
            d->grit.mix = (uint8_t)rnd_in(r, 30, 60);
        }
        break;
    case MF_SCRAPE:                              /* scraping metal: high inharmonic partials, bent grains, held samples */
        for (n = 1; n <= 6; n++)
            if (!mo_op(p, n)[FP_MODE])
                mo_ratio(mo_op(p, n), INHARM[rnd_in(r, 4, 11)] * (rnd_chance(r, 30) ? 2 : 1));
        mo_shatter(g, r);
        g->size_ms = (uint16_t)rnd_in(r, 20, 80);
        g->rate = (uint16_t)rnd_in(r, 20, 90);
        g->mix = (uint8_t)rnd_in(r, 40, 80);
        d->grit.hold = (uint8_t)rnd_in(r, 1, 3);
        d->grit.stut = (uint8_t)rnd_in(r, 2, 6);
        d->grit.stut_ms = (uint8_t)rnd_in(r, 15, 70);
        d->grit.bend = (uint8_t)rnd_in(r, 40, 80);
        d->grit.mix = (uint8_t)rnd_in(r, 40, 70);
        break;
    case MF_DATA:                                /* data: glass through bitwise logic, crushed, stuttering, bent pulsars */
        mo_pulsar(g, r);
        g->mix = (uint8_t)rnd_in(r, 50, 90);
        d->grit.bits = (uint8_t)rnd_in(r, 5, 7);
        d->grit.logic = (uint8_t)rnd_in(r, 60, 100);
        d->grit.lratio = LR[rnd_below(r, 6)];
        d->grit.stut = (uint8_t)rnd_in(r, 5, 15);
        d->grit.stut_ms = (uint8_t)rnd_in(r, 5, 25);
        d->grit.bend = (uint8_t)rnd_in(r, 30, 60);
        d->grit.mix = (uint8_t)rnd_in(r, 70, 100);
        break;
    case MF_ASH:                                 /* ash: a dark drone under a sparse dusty crackle */
        p[FP_TRNSP] = 12;
        mo_cloud(g, r);
        g->size_ms = (uint16_t)rnd_in(r, 4, 15);
        g->rate = (uint16_t)rnd_in(r, 3, 12);
        g->shape = 1;
        g->spread_ms = (uint16_t)rnd_in(r, 200, 400);
        g->pch = 0;
        g->mix = (uint8_t)rnd_in(r, 40, 70);
        g->dark = (uint8_t)rnd_in(r, 20, 50);
        g->freeze_ms = 0;
        d->grit.bits = (uint8_t)rnd_in(r, 6, 8);
        d->grit.hold = (uint8_t)rnd_in(r, 3, 6);
        d->grit.mix = (uint8_t)rnd_in(r, 30, 60);
        break;
    default:                                     /* TUNNEL: deep below, a huge dark cloud fed back, sometimes frozen */
        p[FP_TRNSP] = (uint8_t)(rnd_chance(r, 50) ? 12 : 0);
        mo_cloud(g, r);
        g->size_ms = (uint16_t)rnd_in(r, 250, 400);
        g->rate = (uint16_t)rnd_in(r, 6, 15);
        g->back_ms = (uint16_t)rnd_in(r, 150, 250);
        g->fb = (uint8_t)rnd_in(r, 40, 60);
        g->dark = (uint8_t)rnd_in(r, 60, 90);
        g->mix = (uint8_t)rnd_in(r, 70, 100);
        g->freeze_ms = (uint16_t)(rnd_chance(r, 30) ? rnd_in(r, 1500, 4000) : 0);
        break;
    }
}
/* the hand drawn: most sounds of generation 3 have one; what it moves leans on the family (the glitchy ones' speed
 * on their glitches, the dark ones' age on the cloud's darkness); paced by the path (AMBIENT slower) */
static void mo_hand_draw(mo_design_t *d, int32_t f, uint32_t path, rnd_t *r)
{
    static const struct { uint8_t src, dst; int8_t lo, hi; } POOLH[10] = {
        {HS_X, HD_BRIGHT, -30, 40}, {HS_SIZE, HD_LEVEL, -70, 0}, {HS_Y, HD_GRAIN, -20, 50}, {HS_AGE, HD_DARK, -30, 30},
        {HS_ANGLE, HD_BRIGHT, -30, 30}, {HS_COUNT, HD_GRAIN, 0, 40}, {HS_SPEED, HD_GLITCH, 0, 60},
        {HS_DIST, HD_GRIT, -40, 40}, {HS_GX, HD_DARK, -40, 20}, {HS_GY, HD_GRIT, -30, 50}};
    int gritty = d->grit.mix || d->grit.bend, k, n;
    uint8_t w[10] = {6, 5, 5, 4, 3, 3, 0, 0, 3, 0};
    if (gritty)
        w[6] = 6, w[7] = 5, w[9] = 4;
    if (f == MF_ASH || f == MF_TUNNEL || f == MF_HOWL)
        w[3] = 8, w[8] = 6;
    if (!rnd_chance(r, 85))
        return;
    d->hand.on = 1;
    d->hand.pace = (uint16_t)(path == PATH_AMBIENT ? rnd_in(r, 110, 230) : path == PATH_EXP ? rnd_in(r, 230, 480) : rnd_in(r, 170, 320));
    d->hand.travel = (uint8_t)rnd_in(r, 15, 80);
    d->hand.density = (uint8_t)rnd_in(r, 20, 95);
    n = 2 + rnd_below(r, 3);
    for (k = 0; k < n; k++) {
        uint32_t i = rnd_weighted(r, w, 10);
        int32_t amt = rnd_in(r, 60, 130);        /* (each its own reach: 0.6 .. 1.3 of the archetype's) */
        d->hand.map[k].src = POOLH[i].src;
        d->hand.map[k].dst = POOLH[i].dst;
        d->hand.map[k].lo = (int8_t)clampi(POOLH[i].lo * amt / 100, -100, 100);
        d->hand.map[k].hi = (int8_t)clampi(POOLH[i].hi * amt / 100, -100, 100);
        if (POOLH[i].dst != HD_LEVEL && rnd_chance(r, 30)) {   /* (now and then the other way round -- but a harder
                                                                 * press never quieter: the sound would start loud) */
            int8_t t = d->hand.map[k].lo;
            d->hand.map[k].lo = d->hand.map[k].hi;
            d->hand.map[k].hi = t;
        }
        w[i] = 0;                                /* (each archetype once) */
    }
    d->hand.n = (uint8_t)n;
}
/* the older families on EXPERIMENTAL now and then roughened too: a little of the small DAC */
static void mo_grit_draw(mo_design_t *d, uint32_t path, rnd_t *r)
{
    if (path != PATH_EXP || !rnd_chance(r, 30))
        return;
    d->grit.bits = (uint8_t)rnd_in(r, 7, 10);
    d->grit.hold = (uint8_t)rnd_in(r, 0, 2);
    d->grit.mix = (uint8_t)rnd_in(r, 30, 60);
    d->grit.bend = (uint8_t)(rnd_chance(r, 40) ? rnd_in(r, 10, 40) : 0);
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
    if (f >= MF_G3)                                      /* (generation 3's families: a body borrowed) */
        body = mo_fam3_body(f, r);
    else if (f < MF_BODY_N)
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
    default:
        if (f >= MF_G3)
            mo_fam3(d, f, r);
        break;
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
    if (gen >= 3u) {                                     /* generation 3: the hand; the older families' grit */
        if (f < MF_G3)
            mo_grit_draw(d, path, r);
        mo_hand_draw(d, f, path, r);
    }
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
