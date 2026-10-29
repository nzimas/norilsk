/* SPDX-License-Identifier: GPL-3.0-only
 * NORILSK: the modulation -- Tonitor's (mod.c, the same author): two LFOs and four slots for each of the six
 * slots, the three generators and the three effect slots. A slot takes a source (an LFO; the last note's velocity
 * or pitch; a random value drawn at each note) to a destination by an amount, -100 .. +100 of the destination's
 * whole swing. It moves values the audio reads every block: nothing is rebuilt. Worked out in the audio ISR once a
 * block (1378 Hz), before the work is dealt (gens.c mix_block).
 *
 *   a generator's destinations: its four macros (BRIGHTNESS, DETUNE or INHARMONIC, GRAIN, MOTION: mol.c), its level,
 *     its pan, its reverb and delay sends (master.c), either LFO's rate
 *   an effect slot's: the values of the effects standing in it, by place -- the first effect's four, the second's,
 *     the third's -- so a destination follows whatever effect stands at that place now: HOME draws the slot anew and
 *     the LFO moves the new effect's value at that place (none there: it rests); its level, its pan, either LFO's
 *     rate. A value is moved within the bounds its effect keeps it in (rack.c RM_LO, RM_HI)
 * The note sources: a generator's own notes; an effect slot's, the notes of the generators wired into it.
 *
 * LFO: SHAPE (SINE, TRI, SAW UP, SAW DN, SQUARE, STEPS -- a new random value each cycle --, DRIFT -- smooth random),
 * RATE (0 .. 100: 0.01 Hz .. 20 Hz), MODE (FREE; NOTE: from the start at each note; ONCE: one cycle from each note,
 * then held), FADE (0 .. 100: in over 0 .. 10 s from each note). The MOD page (SEQ's third) sets them; SHIFT + SEQ
 * throws the chosen slot's modulation anew. Nothing is modulated until a slot is set: the sound as it was. */

#define NSLOT (NGEN + NFX)       /* the six slots: three generators, three effect slots (the top row's buttons) */
enum { MS_SINE, MS_TRI, MS_SAWU, MS_SAWD, MS_SQR, MS_STEPS, MS_DRIFT, MS_N };
enum { MM_FREE, MM_NOTE, MM_ONCE, MM_N };
enum { MSRC_OFF, MSRC_L1, MSRC_L2, MSRC_VEL, MSRC_KEY, MSRC_RAND, MSRC_N };
static const char *const MS_NAME[MS_N] = {"SINE", "TRI", "SAW UP", "SAW DN", "SQUARE", "STEPS", "DRIFT"};
static const char *const MM_NAME[MM_N] = {"FREE", "NOTE", "ONCE"};
static const char *const MSRC_NAME[MSRC_N] = {"OFF", "LFO 1", "LFO 2", "VELOCITY", "KEY", "RANDOM"};
/* a generator's destinations, and an effect slot's (FMD_P0 + 4 k + j: the k-th effect's value j) */
enum { GMD_MAC1, GMD_MAC2, GMD_MAC3, GMD_MAC4, GMD_LVL, GMD_PAN, GMD_RV, GMD_DL, GMD_RATE1, GMD_RATE2, GMD_N };
enum { FMD_P0, FMD_LVL = FMD_P0 + 12, FMD_PAN, FMD_RATE1, FMD_RATE2, FMD_N };
#define MD_MAX 16                /* the most destinations a slot has */
static const int16_t GMD_SPAN[GMD_N] = {MAC_MAX, MAC_MAX, MAC_MAX, MAC_MAX, 12, PAN_MAX, 100, 100, 50, 50};
static const char *const GMD_NAME[GMD_N] = {"", "", "", "", "LEVEL", "PAN", "REVERB", "DELAY", "LFO 1 RATE", "LFO 2 RATE"};
static const char *const FMD_NAME[4] = {"LEVEL", "PAN", "LFO 1 RATE", "LFO 2 RATE"};
/* the effects' values, as the MOD page names them */
static const char *const RM_PNAME[NKIND][4] = {
    {"TYPE", "FREQ", "RES", "DRIVE"}, {"DRIVE", "WIDTH", "FOLD 2", "TONE"}, {"RATE", "BITS", "FILTER", "DRIVE"},
    {"SHIFT", "DRIVE", "DIRECTION", "TONE"}, {"PITCH", "Q", "SPREAD", "UPPER"}, {"PITCH", "RING", "DETUNE", "TONE"},
    {"VOWEL", "MOVE", "Q", "DRIVE"}, {"TIME", "FEEDBACK", "DAMP", "LEVEL"}, {"TIME", "DROPOUT", "FEEDBACK", "AGE"},
    {"WINDOW", "TONE", "FEEDBACK", "SPREAD"}, {"CHIRP", "TANK", "DECAY", "TONE"}, {"SIZE", "DECAY", "DAMP", "LEVEL"},
    {"BEND", "CLOCK", "STUTTER", "SPAN"},
    {"SIZE", "DENSITY", "FREEZE", "SPREAD"}, {"SIZE", "FEEDBACK", "TONE", "DRIVE"}, {"FREQ", "DIODE", "SELF", "TONE"},
    {"GAIN", "GATE", "BIAS", "TONE"}, {"SHAPE", "ODD", "DRIVE", "TONE"}, {"PITCH", "Q", "SET", "BRIGHT"},
    {"SPAN", "TAIL", "MIX", "TONE"}, {"SIZE", "DIFFUSE", "MIX", "TONE"}, {"RATE", "DRIVE", "MIX", "TONE"},
    {"MOMENT", "OP", "BITS", "TONE"}, {"GRAIN", "AMOUNT", "TONE", "SOUND"},
    {"MODE", "GROUP", "AMOUNT", "SHORTEST"}, {"ALGO", "AMOUNT", "DELAY", "TONE"}, {"DRIVE", "DAMP", "SCALE", "TONE"}};

typedef struct { uint8_t shape, rate, mode, fade; } mlfo_t;
typedef struct { uint8_t src, dst; int8_t amt; } mslot_t;
#define NMSLOT 8u                /* (build 18: 4 before -- sessions.c reads the older sessions' four) */
#define NMSLOT_OLD 4u
typedef struct {
    mlfo_t lfo[2];
    mslot_t s[NMSLOT];
} mod_t;                                         /* (a slot's: 32 bytes; sessions and snapshots keep it) */
static mod_t slot_mod[NSLOT];                    /* (ui.c sets it, the ISR reads it) */

static struct {
    uint32_t ph[2];
    int32_t a[2], b[2];                          /* STEPS' and DRIFT's values: the last, the next */
    int32_t fade[2];                             /* each LFO's fade in since the last note, Q16 0 .. 1 */
    int32_t val[2];                              /* each LFO now, Q15 -1 .. 1 */
    uint8_t held[2];                             /* ONCE: done */
    uint8_t vel, key;
    int16_t rnd;
    int16_t off[MD_MAX];                         /* each destination's offset, its units */
} mst[NSLOT];
static uint32_t mod_rs = 0x9E3779B9u;
static int32_t mod_rand(void)                    /* -Q15 .. Q15 */
{
    mod_rs = mod_rs * 1664525u + 1013904223u;
    return (int32_t)(mod_rs >> 16) - 32768;
}
static uint32_t mod_inc(int32_t rate)            /* 0.01 Hz x 2000^(rate / 100), a block's phase step */
{
    return (uint32_t)(((uint64_t)31166u * exp2_q16(clampi(rate, 0, 100) * 7189)) >> 16);
}
/* (build 32: each LFO's step kept with the rate it was worked out for -- worked out again only when the rate moves) */
static struct { uint32_t inc; uint8_t rate, have; } mod_ic[NSLOT][2];
static uint32_t mod_inc_at(uint32_t sl, uint32_t k, int32_t rate)
{
    uint8_t r = (uint8_t)clampi(rate, 0, 100);
    if (!mod_ic[sl][k].have || mod_ic[sl][k].rate != r) {
        mod_ic[sl][k].inc = mod_inc(r);
        mod_ic[sl][k].rate = r;
        mod_ic[sl][k].have = 1;
    }
    return mod_ic[sl][k].inc;
}
static uint32_t mod_ndst(uint32_t sl) { return sl < NGEN ? GMD_N : FMD_N; }
static int32_t mod_span(uint32_t sl, uint32_t d)
{
    if (sl < NGEN)
        return d < GMD_N ? GMD_SPAN[d] : 0;
    return d < FMD_LVL ? 100 : d == FMD_LVL ? 12 : d == FMD_PAN ? 20 : 50;
}
/* a note on slot sl: what it holds for the sources, and the LFOs started again as their modes say */
static void mod_note1(uint32_t sl, uint32_t note, uint32_t vel)
{
    uint32_t k;
    mst[sl].vel = (uint8_t)(vel ? vel : 127u);
    mst[sl].key = (uint8_t)note;
    mst[sl].rnd = (int16_t)mod_rand();
    mst[sl].fade[0] = mst[sl].fade[1] = 0;
    for (k = 0; k < 2u; k++)
        if (slot_mod[sl].lfo[k].mode != MM_FREE) {
            mst[sl].ph[k] = 0;
            mst[sl].held[k] = 0;
        }
}
/* a note on generator l (gens.c note_on): its slot, and the effect slots it is wired into */
static void mod_note(uint32_t l, uint32_t note, uint32_t vel)
{
    const gen_patch_t *p = &gbuf[l][g_use[l]];
    uint32_t e, done = 0;
    mod_note1(l, note, vel);
    for (e = 0; e < p->nent; e++) {
        uint32_t s = p->ent[e] / FX_N;
        if (s < NFX && !((done >> s) & 1u)) {
            done |= 1u << s;
            mod_note1(NGEN + s, note, vel);
        }
    }
}
static int mod_on(uint32_t sl)                   /* (any slot in use) */
{
    uint32_t i;
    for (i = 0; i < NMSLOT; i++)
        if (slot_mod[sl].s[i].src && slot_mod[sl].s[i].amt)
            return 1;
    return 0;
}
/* the block: slot sl's LFOs a step on, the offsets worked out (Tonitor's mod_block) */
static void mod_block(uint32_t sl)
{
    const mod_t *m = &slot_mod[sl];
    uint32_t k, i, r1 = sl < NGEN ? GMD_RATE1 : FMD_RATE1, nd = mod_ndst(sl);
    int32_t ro[2] = {mst[sl].off[r1], mst[sl].off[r1 + 1u]};   /* (the LFOs' rates as the last block moved them) */
    memset(mst[sl].off, 0, sizeof mst[sl].off);
    if (!mod_on(sl))
        return;
    for (k = 0; k < 2u; k++) {
        const mlfo_t *f = &m->lfo[k];
        uint32_t p0 = mst[sl].ph[k], inc = mod_inc_at(sl, k, (int32_t)f->rate + ro[k]), p1 = p0 + inc, t;
        int wrap = p1 < p0;
        int32_t v;
        if (mst[sl].held[k])
            p1 = p0;
        else if (wrap && f->mode == MM_ONCE) {
            p1 = 0xFFFFFFFFu;                    /* (one cycle: held at its end) */
            mst[sl].held[k] = 1;
        }
        if (wrap && !mst[sl].held[k]) {          /* (a new cycle: STEPS' and DRIFT's next value) */
            mst[sl].a[k] = mst[sl].b[k];
            mst[sl].b[k] = mod_rand();
        }
        mst[sl].ph[k] = p1;
        t = p1 >> 16;
        switch (f->shape) {
        case MS_TRI:
            v = t < 32768u ? (int32_t)t * 2 - 32768 : 32767 - ((int32_t)t - 32768) * 2;
            break;
        case MS_SAWU:
            v = (int32_t)t - 32768;
            break;
        case MS_SAWD:
            v = 32767 - (int32_t)t;
            break;
        case MS_SQR:
            v = t < 32768u ? 32767 : -32768;
            break;
        case MS_STEPS:
            v = mst[sl].b[k];
            break;
        case MS_DRIFT:
            v = mst[sl].a[k] + (int32_t)(((int64_t)(mst[sl].b[k] - mst[sl].a[k]) * (int32_t)t) >> 16);
            break;
        default:
            v = sin_ph(p1);
            break;
        }
        if (mst[sl].fade[k] < Q16)               /* (FADE: in over 0.1 s a step, 138 blocks) */
            mst[sl].fade[k] += f->fade ? (int32_t)(65536u / (f->fade * 138u) + 1u) : Q16;
        if (mst[sl].fade[k] > Q16)
            mst[sl].fade[k] = Q16;
        mst[sl].val[k] = (int32_t)(((int64_t)v * mst[sl].fade[k]) >> 16);
    }
    for (i = 0; i < NMSLOT; i++) {
        const mslot_t *s = &m->s[i];
        int32_t v, o;
        if (!s->src || !s->amt || s->dst >= nd)
            continue;
        if (s->src == MSRC_L1 || s->src == MSRC_L2)
            v = mst[sl].val[s->src - MSRC_L1];
        else if (s->src == MSRC_VEL)
            v = ((int32_t)mst[sl].vel - 127) * 258;   /* (0 at full, down to -1 for the softest) */
        else if (s->src == MSRC_KEY)
            v = clampi(((int32_t)mst[sl].key - 60) * 683, -Q15, Q15);   /* (+- 4 octaves around C4: +-1) */
        else
            v = mst[sl].rnd;
        o = v * s->amt * mod_span(sl, s->dst) / (Q15 * 100);   /* (32 bits: at most 32768 x 100 x 100) */
        mst[sl].off[s->dst] = (int16_t)clampi(mst[sl].off[s->dst] + o, -400, 400);
    }
}
/* what a slot's source does now (the MOD page's NOW row), Q15 */
static int32_t mod_now(uint32_t sl, const mslot_t *q)
{
    int32_t sv;
    if (!q->src || !q->amt)
        return 0;
    sv = q->src == MSRC_L1 || q->src == MSRC_L2 ? mst[sl].val[q->src - MSRC_L1]
         : q->src == MSRC_VEL ? ((int32_t)mst[sl].vel - 127) * 258
         : q->src == MSRC_KEY ? clampi(((int32_t)mst[sl].key - 60) * 683, -Q15, Q15) : mst[sl].rnd;
    return sv * q->amt / 100;
}
/* the block (mix_block, before the work is dealt): every slot's offsets, out to the generators and the rack */
static void mod_tick(void)
{
    uint32_t sl, j;
    for (sl = 0; sl < NSLOT; sl++)
        mod_block(sl);
    for (sl = 0; sl < NGEN; sl++) {
        int on = 0;
        for (j = 0; j < 4u; j++) {
            gen_mac[sl][j] = (int8_t)clampi(mst[sl].off[GMD_MAC1 + j], -MAC_MAX, MAC_MAX);
            on |= gen_mac[sl][j] != 0;
        }
        gen_mac_on[sl] = (uint8_t)on;
        gen_mod_lvl[sl] = mst[sl].off[GMD_LVL];
        gen_mod_pan[sl] = mst[sl].off[GMD_PAN];
        gen_mod_rv[sl] = mst[sl].off[GMD_RV];
        gen_mod_dl[sl] = mst[sl].off[GMD_DL];
    }
    for (sl = 0; sl < NFX; sl++) {
        int on = 0;
        for (j = 0; j < 12u; j++) {
            fx_mod_p[sl][j] = mst[NGEN + sl].off[FMD_P0 + j];
            on |= fx_mod_p[sl][j] != 0;
        }
        fx_mod_on[sl] = (uint8_t)on;
        fx_mod_lvl[sl] = mst[NGEN + sl].off[FMD_LVL];
        fx_mod_pan[sl] = mst[NGEN + sl].off[FMD_PAN];
    }
}

static void mod_check(mod_t *m, uint32_t sl)     /* (as read from a session: each value in its range, or off) */
{
    uint32_t k;
    for (k = 0; k < 2u; k++)
        if (m->lfo[k].shape >= MS_N || m->lfo[k].rate > 100u || m->lfo[k].mode >= MM_N || m->lfo[k].fade > 100u)
            memset(&m->lfo[k], 0, sizeof m->lfo[k]);
    for (k = 0; k < NMSLOT; k++)
        if (m->s[k].src >= MSRC_N || m->s[k].dst >= mod_ndst(sl) || m->s[k].amt < -100 || m->s[k].amt > 100)
            memset(&m->s[k], 0, sizeof m->s[k]);
}
static void mod_init(void)
{
    memset(slot_mod, 0, sizeof slot_mod);
    memset(mst, 0, sizeof mst);
}

/* SHIFT + SEQ: slot sl's modulation thrown anew (Tonitor's mod_random) -- both LFOs, slow (this firmware's textures:
 * 0.01 .. ~1 Hz mostly), and two to six slots from the LFOs mostly, to destinations not yet taken */
static void mod_random(uint32_t sl, uint32_t seed)
{
    static const uint8_t GW[GMD_N] = {10, 8, 6, 10, 4, 8, 7, 6, 3, 3};
    static const uint8_t FW[FMD_N] = {6, 6, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 0, 2, 2};   /* (a lane's PAN never drawn:
                                                  * build 38, a lane is panned with its generator) */
    static const uint8_t SW[MSRC_N] = {0, 40, 35, 5, 5, 15};
    rnd_t r;
    mod_t m;
    uint32_t k, n, i, nd = mod_ndst(sl);
    const uint8_t *dw = sl < NGEN ? GW : FW;
    memset(&m, 0, sizeof m);
    rnd_seed(&r, seed);
    for (k = 0; k < 2u; k++) {
        mlfo_t *f = &m.lfo[k];
        f->shape = (uint8_t)rnd_below(&r, MS_N);
        f->rate = (uint8_t)(k ? rnd_in(&r, 0, 40) : rnd_in(&r, 5, rnd_chance(&r, 20) ? 70 : 50));   /* (LFO 2 slower) */
        f->mode = (uint8_t)(rnd_chance(&r, 70) ? MM_FREE : rnd_chance(&r, 60) ? MM_NOTE : MM_ONCE);
        f->fade = (uint8_t)(rnd_chance(&r, 50) ? 0 : rnd_in(&r, 10, 60));
    }
    n = 2u + (uint32_t)rnd_below(&r, 5);        /* (two to six of the eight) */
    for (k = 0; k < n; k++) {
        mslot_t *s = &m.s[k];
        uint32_t tries, d = 0, r1 = sl < NGEN ? GMD_RATE1 : FMD_RATE1;
        int32_t mag;
        s->src = (uint8_t)rnd_weighted(&r, SW, MSRC_N);
        for (tries = 0; tries < 8u; tries++) {   /* (a destination not yet taken, and no LFO on its own rate) */
            d = rnd_weighted(&r, dw, nd);
            if ((d == r1 && s->src == MSRC_L1) || (d == r1 + 1u && s->src == MSRC_L2))
                continue;
            for (i = 0; i < k && m.s[i].dst != d; i++)
                ;
            if (i == k)
                break;
        }
        s->dst = (uint8_t)d;
        mag = rnd_in(&r, 15, 60);
        s->amt = (int8_t)(mag * (rnd_chance(&r, 50) ? 1 : -1));
    }
    slot_mod[sl] = m;
}
