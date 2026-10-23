/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR: the patch -- a network of one generator and a chain of processors -- its voices, and the mix.
 *
 * A patch is drawn whole by HOME (patch_new, the main loop): a sound of the generator (MOLECULE, mol.c), where
 * it sits on the screen (x: its place in the stereo image; y: its level, the top loudest), and a chain of up to
 * NNODE processors (nodes.c) with their values. The keys play it: six voices, all of the one generator, drawn
 * from a pool as the notes need them (a note never cuts the release of the one before while a voice is free;
 * all six busy, the quietest release goes, else the oldest note).
 *
 * The audio ISR sees a patch through two buffers: the main loop writes the one the ISR is not using and
 * publishes it (p_pub); the ISR takes it at a block's start, fading the voices of the one before over FADE
 * blocks (they go on rendering from their old buffer until they are gone: patch_slot_free waits for that
 * before the main loop writes it again). The processors' states start afresh; the delay's and the reverb's
 * lines keep what they hold (a tail of the patch before, into the next). */

#define NVOICE 6
#define NNODE 4
#define FADE 12u                 /* blocks of a fade (~9 ms) */

typedef struct {
    mo_design_t gen;
    uint8_t gx, gy;              /* the generator's place: 0 .. 255 across (the pan), 0 .. 255 down (the level) */
    node_t nd[NNODE];
    uint8_t nnd;
    uint8_t scene;               /* what the draw leaned to (SC_*) */
    uint32_t seed;
    uint32_t serial;             /* (the screen: each patch a number of its own) */
} patch_t;
static patch_t pbuf[2];
static volatile uint8_t p_pub, p_use;

typedef struct {
    uint8_t note, gate, sounding;
    uint8_t fade;                /* blocks of a fade left (a steal, a patch change), 0 = none */
    uint8_t pend, pend_note;     /* the fade ends in a note-on */
    uint8_t dsg;                 /* the patch buffer it plays (pbuf) */
    uint32_t age;                /* note-on order */
    uint32_t pk;                 /* its loudest block */
    int32_t lvl;                 /* its last block's peak */
    uint16_t quiet;              /* let go: blocks far below its loudest */
    uint32_t rel_at;             /* the block it was let go at */
} voice_t;
static voice_t vox[NVOICE];
static mo_voice_t vmem[NVOICE] POOL;
static nstate_t nst[NNODE];
static uint32_t blk_clock, age_clock;
static volatile int32_t master_q12 = 4096;
static volatile uint8_t load_pct, shed_req;
static int32_t lim_env, lim_g = Q15;
static int32_t mix_g, mix_pl, mix_pr;   /* the level and the pan as the last block ended (they glide) */

#define TAIL_FLOOR 11u           /* let go, a voice 66 dB (2^11) under its loudest .. */
#define TAIL_QUIET 276u          /* .. for 0.2 s ends; */
#define TAIL_MAX 41340u          /* .. and none rings on more than 30 s after it was let go */

/* ------------------------------------------------------------ the events --- */
enum { EV_ON, EV_OFF, EV_STOP };
typedef struct { uint8_t type, note; } ev_t;
#define EVQ 64u
static ev_t evq[EVQ];
static volatile uint32_t ev_w, ev_r;
static void ev_post(uint32_t type, uint32_t note)
{
    if (ev_w - ev_r >= EVQ)
        return;
    evq[ev_w % EVQ].type = (uint8_t)type;
    evq[ev_w % EVQ].note = (uint8_t)note;
    RING_PUBLISH();
    ev_w++;
}

/* ------------------------------------------------------------ the voices --- */
static void voice_start(uint32_t k, uint32_t note)
{
    voice_t *v = &vox[k];
    mo_note_on(&vmem[k], &pbuf[p_use].gen, (int32_t)note, 1);
    v->note = (uint8_t)note;
    v->gate = 1;
    v->sounding = 1;
    v->fade = 0;
    v->pend = 0;
    v->dsg = p_use;
    v->age = ++age_clock;
    v->pk = 0;
    v->lvl = 0;
    v->quiet = 0;
}
static uint32_t voice_pick(void)
{
    uint32_t k, best = NVOICE;
    for (k = 0; k < NVOICE; k++)
        if (!vox[k].sounding)
            return k;
    for (k = 0; k < NVOICE; k++)                          /* the quietest release */
        if (!vox[k].gate && !vox[k].pend && (best == NVOICE || vox[k].lvl < vox[best].lvl))
            best = k;
    if (best != NVOICE)
        return best;
    for (k = 0; k < NVOICE; k++)                          /* the oldest note */
        if (!vox[k].pend && (best == NVOICE || vox[k].age < vox[best].age))
            best = k;
    return best == NVOICE ? 0 : best;
}
static void note_on(uint32_t note)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)                          /* the same note again: its voice retriggers */
        if (vox[k].sounding && vox[k].gate && vox[k].note == note && !vox[k].fade) {
            mo_note_on(&vmem[k], &pbuf[vox[k].dsg].gen, (int32_t)note, 0);
            return;
        }
    k = voice_pick();
    if (!vox[k].sounding) {
        voice_start(k, note);
        return;
    }
    vox[k].gate = 0;                                      /* stolen: faded, then the new note */
    vox[k].fade = FADE;
    vox[k].pend = 1;
    vox[k].pend_note = (uint8_t)note;
}
static void note_off(uint32_t note)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && vox[k].gate && vox[k].note == note) {
            vox[k].gate = 0;
            vox[k].rel_at = blk_clock;
            mo_note_off(&vmem[k], &pbuf[vox[k].dsg].gen);
        }
}
static void all_off(void)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding) {
            vox[k].gate = 0;
            vox[k].pend = 0;
            if (!vox[k].fade)
                vox[k].fade = FADE;
        }
}
static void events_take(void)
{
    while (ev_r != ev_w) {
        ev_t e = evq[ev_r % EVQ];
        RING_PUBLISH();
        ev_r++;
        if (e.type == EV_ON)
            note_on(e.note);
        else if (e.type == EV_OFF)
            note_off(e.note);
        else
            all_off();
    }
}
/* the patch published, taken: the voices of the one before fade, the processors start afresh */
static void patch_take(void)
{
    uint32_t k;
    const patch_t *p;
    if (p_pub == p_use)
        return;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && !vox[k].fade) {
            vox[k].gate = 0;
            vox[k].pend = 0;
            vox[k].fade = FADE;
        }
    p_use = p_pub;
    p = &pbuf[p_use];
    memset(nst, 0, sizeof nst);
    for (k = 0; k < NNODE; k++)
        nst[k].ds.g = nst[k].ds.gp = Q16;
    for (k = 0; k < p->nnd; k++)
        node_cook(&p->nd[k]);
}
/* overload (main.c: a half late, or two near the edge): the quietest release goes; late, the oldest held note too */
static void shed(int held)
{
    uint32_t k, ok = NVOICE;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && !vox[k].fade && !vox[k].gate && (ok == NVOICE || vox[k].lvl < vox[ok].lvl))
            ok = k;
    if (ok == NVOICE && held)
        for (k = 0; k < NVOICE; k++)
            if (vox[k].sounding && !vox[k].fade && (ok == NVOICE || vox[k].age < vox[ok].age))
                ok = k;
    if (ok != NVOICE) {
        vox[ok].gate = 0;
        vox[ok].pend = 0;
        vox[ok].fade = FADE;
    }
}

/* a voice's block into x (added), Q15; its bookkeeping */
static void voice_block(uint32_t k, int32_t *x)
{
    voice_t *v = &vox[k];
    int32_t vb[CTL], pk = 0;
    uint32_t i;
    int alive = mo_render_fp(&vmem[k], &pbuf[v->dsg].gen, vb, 0);
    if (v->fade) {                                       /* a fade: linear over FADE blocks */
        int32_t g0 = (int32_t)v->fade * (Q15 / (int32_t)FADE), dg = -(Q15 / (int32_t)FADE) / (int32_t)CTL;
        for (i = 0; i < CTL; i++) {
            g0 += dg;
            vb[i] = mulq15(vb[i], g0);
        }
        if (!--v->fade) {
            alive = 0;
            if (v->pend) {
                uint32_t n = v->pend_note;
                v->sounding = 0;
                voice_start(k, n);
                return;                                  /* (its first block next time) */
            }
        }
    }
    for (i = 0; i < CTL; i++) {
        int32_t a = absi(vb[i]);
        if (a > pk)
            pk = a;
        x[i] += vb[i];
    }
    v->lvl = pk;
    if ((uint32_t)pk > v->pk)
        v->pk = (uint32_t)pk;
    if (!v->gate && !v->fade) {                          /* let go: the tail reaper */
        if ((uint32_t)pk < (v->pk >> TAIL_FLOOR))
            v->quiet++;
        else
            v->quiet = 0;
        if (v->quiet >= TAIL_QUIET || blk_clock - v->rel_at > TAIL_MAX)
            alive = 0;
    }
    if (!alive)
        v->sounding = 0;
}

/* a layer's limiter, after the chain: never past LIM. The peak of a block sets the gain of the next (instant
 * attack, the block's overshoot left to the master's soft ceiling), released over ~200 ms */
#define LIM 19661                /* 0.6, Q15 */
static void lim_block(int32_t *env, int32_t *g, int32_t *x, uint32_t n)
{
    int32_t pk = 0, want, g0 = *g, dg;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t a = absi(x[i]);
        if (a > pk)
            pk = a;
    }
    if (pk > *env)
        *env = pk;
    else
        *env -= *env >> 8;
    want = *env > LIM ? (int32_t)((uint32_t)LIM * 32768u / (uint32_t)*env) : Q15;
    if (want > g0 && g0 < Q15)
        want = g0 + ((want - g0) >> 3);          /* back up gently */
    dg = (want - g0) / (int32_t)n;
    for (i = 0; i < n; i++) {
        g0 += dg;
        x[i] = mulq15(x[i], g0);
    }
    *g = want;
}
/* above half scale, a soft knee into a ceiling at full scale */
static inline int32_t ceiling(int32_t x)
{
    int32_t a = absi(x);
    if (a <= Q15 / 2)
        return x;
    a = Q15 / 2 + (tanh_q15((a - Q15 / 2) * 2) >> 1);
    return x < 0 ? -a : a;
}
#define PATCH_GAIN 7373          /* Q12, x 1.8: one voice at 0.1 RMS reads -15 dBFS */

/* one block: the events, the patch, the voices, the chain, the level and the pan, the reverb, the master (Q15
 * stereo, interleaved) */
static void mix_block(int32_t *out)
{
    int32_t x[CTL], l_[CTL], r_[CTL], g, pl, pr, dg, dpl, dpr;
    uint32_t i, k, rev = 0;
    const patch_t *p;
    events_take();
    patch_take();
    if (shed_req) {
        shed(shed_req > 1u);
        shed_req = 0;
    }
    p = &pbuf[p_use];
    for (i = 0; i < CTL; i++)
        x[i] = 0;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding)
            voice_block(k, x);
    for (i = 0; i < CTL; i++)
        x[i] = mulq12(x[i], PATCH_GAIN);
    for (k = 0; k < p->nnd; k++) {
        if (p->nd[k].kind == PK_REVERB)
            rev = 1;
        else
            node_block(&p->nd[k], &nst[k], x);
    }
    lim_block(&lim_env, &lim_g, x, CTL);
    /* the level (the top loudest: 0 .. -30 dB down the screen) and the pan (equal power), gliding */
    g = (int32_t)(exp2_q16(-(int32_t)p->gy * (5 * 65536 / 255)) >> 2);       /* Q14 */
    {
        uint32_t ph = (uint32_t)p->gx * (0x40000000u / 255u);
        pl = sin_ph(ph + 0x40000000u);
        pr = sin_ph(ph);
    }
    dg = (g - mix_g) / (int32_t)CTL;
    dpl = (pl - mix_pl) / (int32_t)CTL;
    dpr = (pr - mix_pr) / (int32_t)CTL;
    for (i = 0; i < CTL; i++) {
        int32_t s;
        mix_g += dg;
        mix_pl += dpl;
        mix_pr += dpr;
        s = (int32_t)(((int64_t)x[i] * mix_g) >> 14);
        x[i] = s;
        l_[i] = mulq15(s, mix_pl);
        r_[i] = mulq15(s, mix_pr);
    }
    mix_g = g;
    mix_pl = pl;
    mix_pr = pr;
    if (rev)
        rv_block_fp(x, l_, r_, CTL);
    for (i = 0; i < CTL; i++) {
        out[2u * i] = ceiling(mulq12(l_[i], master_q12));
        out[2u * i + 1u] = ceiling(mulq12(r_[i], master_q12));
    }
    blk_clock++;
}

static int patch_sounding(void)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding)
            return 1;
    return 0;
}

/* ------------------------------------------------------- the draw (main loop) --- */
/* A patch is drawn after Vascular's abstract path, cut to one generator:
 *   - a SCENE, drawn alike: what the draw leans to -- the generator's families and the processors' kinds it
 *     favours (x 3 and x 2.8, as Vascular's), and the chance of a shared tail (a reverb at the end):
 *       FIELD      spectral fields: glass, formants, clouds; filters, flangers, combs; tails often
 *       MATERIAL   resonant bodies: metals, glass, reeds, impacts, pulsars; combs, reverbs, filters
 *       FEEDBACK   feedback ecologies: bruits, drones, pulses, reeds; delays, rings, drives; tails seldom
 *       EROSION    erosion: clouds, freezes, shatters, bruits; crushes, folds, reverbs, filters
 *   - a HARSHNESS budget of 0, 1 or 2 (0.4, 0.4, 0.2): the rough families and the destructive or resonant
 *     processors each cost some of it, and nothing the budget cannot pay for is drawn
 *   - the generator: its family by the scene's weights on the engine's own path weights, on the scene's path
 *   - the chain: one, two or three processors (0.45 a second, 0.25 a third), each kind weighted by the scene,
 *     never two of a character, at most one destructive; then, by the scene's chance, a reverb as the tail;
 *     the reverb always last
 *   - the generator's place: across at random (its pan), its level 0.5 .. 0.85 of the way up (Vascular's
 *     foundation sits at 0.78) */
enum { SC_FIELD, SC_MATERIAL, SC_FEEDBACK, SC_EROSION, SC_N };
static const char *const SC_NAME[SC_N] = {"FIELD", "MATERIAL", "FEEDBACK", "EROSION"};
static const uint8_t SC_PATH[SC_N] = {PATH_AMBIENT, PATH_OPEN, PATH_EXP, PATH_EXP};
static const uint8_t SC_TAIL[SC_N] = {35, 30, 18, 35};                /* the chance of a shared tail, percent */
/* the families each scene favours (x 3), and what each costs of the harshness budget */
static const uint8_t SC_FAM[SC_N][MF_N] = {
    {0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0},
    {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0},
    {0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0},
    {0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1}};
static const uint8_t MF_HARSH[MF_N] = {0, 0, 2, 1, 1, 0, 0, 0, 0, 1, 1, 0};
/* the kinds each scene favours (x 2.8) */
static const uint8_t SC_KIND[SC_N][PK_N] = {
    {0, 1, 0, 0, 0, 0, 1, 1, 0, 0},
    {0, 1, 0, 0, 0, 0, 1, 0, 0, 1},
    {0, 0, 0, 1, 0, 1, 0, 0, 1, 0},
    {0, 1, 1, 0, 1, 0, 0, 0, 0, 1}};
/* each kind's own weight (Vascular's foundation weights, x 10) */
static const uint8_t PK_W[PK_N] = {0, 12, 4, 5, 4, 7, 10, 10, 11, 11};

/* a node of a kind, its values drawn (Vascular: each value at its home with a chance, else moved) */
static void node_draw(node_t *nd, uint32_t kind, rnd_t *r)
{
    nd->kind = (uint8_t)kind;
    nd->p[0] = nd->p[1] = nd->p[2] = nd->p[3] = 0;
    switch (kind) {
    case PK_FILTER:
        nd->p[0] = (uint8_t)(rnd_chance(r, 60) ? rnd_in(r, 0, 33) : rnd_chance(r, 50) ? rnd_in(r, 34, 66) : rnd_in(r, 67, 100));
        nd->p[1] = (uint8_t)rnd_in(r, 30, 85);
        nd->p[2] = (uint8_t)rnd_in(r, 10, 70);
        nd->p[3] = (uint8_t)(rnd_chance(r, 50) ? rnd_in(r, 10, 60) : 0);
        break;
    case PK_CRUSH:
        nd->p[0] = (uint8_t)(rnd_chance(r, 70) ? rnd_in(r, 20, 70) : 0);
        nd->p[1] = (uint8_t)(rnd_chance(r, 60) ? rnd_in(r, 10, 60) : 0);
        if (!nd->p[0] && !nd->p[1])
            nd->p[0] = 40;
        break;
    case PK_DRIVE:
        nd->p[0] = (uint8_t)rnd_in(r, 15, 70);
        break;
    case PK_FOLD:
        nd->p[0] = (uint8_t)rnd_in(r, 20, 70);
        nd->p[1] = (uint8_t)rnd_in(r, 20, 80);
        nd->p[2] = (uint8_t)rnd_in(r, 0, 60);
        nd->p[3] = (uint8_t)rnd_in(r, 40, 100);
        break;
    case PK_RING:
        nd->p[0] = (uint8_t)rnd_in(r, 10, 90);
        nd->p[1] = (uint8_t)rnd_in(r, 40, 100);
        nd->p[2] = (uint8_t)(rnd_chance(r, 50) ? rnd_in(r, 5, 50) : 0);
        break;
    case PK_COMB:
        nd->p[0] = (uint8_t)rnd_in(r, 20, 90);
        nd->p[1] = (uint8_t)rnd_in(r, 30, 95);
        nd->p[2] = (uint8_t)rnd_in(r, 20, 80);
        nd->p[3] = (uint8_t)(rnd_chance(r, 50) ? rnd_in(r, 10, 60) : 0);
        break;
    case PK_FLANGE:
        nd->p[0] = (uint8_t)rnd_in(r, 5, 60);
        nd->p[1] = (uint8_t)rnd_in(r, 10, 70);
        nd->p[2] = (uint8_t)rnd_in(r, 20, 80);
        nd->p[3] = (uint8_t)rnd_in(r, 30, 95);
        break;
    case PK_DELAY:
        nd->p[0] = (uint8_t)rnd_in(r, 40, 85);
        nd->p[1] = (uint8_t)rnd_in(r, 20, 70);
        nd->p[2] = (uint8_t)rnd_in(r, 20, 70);
        nd->p[3] = (uint8_t)rnd_in(r, 30, 70);
        break;
    case PK_REVERB:
        nd->p[0] = (uint8_t)rnd_in(r, 40, 90);
        nd->p[1] = (uint8_t)rnd_in(r, 40, 80);
        nd->p[2] = (uint8_t)rnd_in(r, 20, 60);
        nd->p[3] = (uint8_t)rnd_in(r, 30, 70);
        break;
    default:
        break;
    }
}
/* the chain, drawn (above) */
static void chain_draw(patch_t *p, rnd_t *r, int32_t *budget)
{
    uint32_t n = 1u + (rnd_chance(r, 45) ? 1u : 0u) + (rnd_chance(r, 25) ? 1u : 0u), i, j, chars = 0, destructive = 0;
    int rev = 0;
    p->nnd = 0;
    for (i = 0; i < n; i++) {
        uint8_t w[PK_N];
        uint32_t k, any = 0;
        for (k = 0; k < PK_N; k++) {
            int ok = k != PK_NONE && !((chars >> PK_CHAR[k]) & 1u) && (int32_t)PK_HARSH[k] <= *budget &&
                     !(destructive && PK_CHAR[k] == CH_DESTRUCTIVE);
            w[k] = (uint8_t)(ok ? PK_W[k] * (SC_KIND[p->scene][k] ? 28u : 10u) / 10u : 0u);
            any |= w[k];
        }
        if (!any)
            break;
        k = rnd_weighted(r, w, PK_N);
        if (!w[k])
            break;
        chars |= 1u << PK_CHAR[k];
        if (PK_CHAR[k] == CH_DESTRUCTIVE)
            destructive = 1;
        *budget -= PK_HARSH[k];
        if (k == PK_REVERB)
            rev = 1;
        node_draw(&p->nd[p->nnd++], k, r);
    }
    if (!rev && p->nnd < NNODE && rnd_chance(r, SC_TAIL[p->scene]))   /* the shared tail: a reverb at the end */
        node_draw(&p->nd[p->nnd++], PK_REVERB, r);
    for (i = 0; i < p->nnd; i++)                         /* the reverb last */
        for (j = i + 1; j < p->nnd; j++)
            if (p->nd[i].kind == PK_REVERB) {
                node_t t = p->nd[i];
                p->nd[i] = p->nd[j];
                p->nd[j] = t;
            }
}

/* the main loop waits (up to ~40 ms) until no voice plays the buffer it is about to write */
static int patch_slot_free(uint32_t slot)
{
    uint32_t t0 = fm1_ms, k;
    for (;;) {
        int busy = 0;
        for (k = 0; k < NVOICE; k++)
            if (vox[k].sounding && vox[k].dsg == slot)
                busy = 1;
        if (!busy || p_pub != p_use)
            break;
        if ((uint32_t)(fm1_ms - t0) > 40u)
            return 0;
        fm1_wdt_feed();
    }
    return !(p_pub != p_use);
}
/* HOME: a new patch, from a seed; published to the ISR. Returns 0 if the slot was not free in time */
static uint32_t patch_serial;
static int patch_new(uint32_t seed)
{
    uint32_t slot = p_use ^ 1u;
    patch_t *p = &pbuf[slot];
    rnd_t rr, *r = &rr;
    int32_t budget, f;
    uint8_t w[MF_N];
    uint32_t k, path;
    if (!patch_slot_free(slot))
        return 0;
    memset(p, 0, sizeof *p);
    rnd_seed(r, seed ^ 0x50415443u);
    p->seed = seed;
    p->serial = ++patch_serial;
    p->scene = (uint8_t)rnd_below(r, SC_N);
    budget = rnd_chance(r, 40) ? 0 : rnd_chance(r, 67) ? 1 : 2;
    p->gx = (uint8_t)rnd_in(r, 10, 245);
    p->gy = (uint8_t)(255 - rnd_in(r, 128, 217));           /* 0.5 .. 0.85 of the way up */
    path = SC_PATH[p->scene];
    for (k = 0; k < MF_N; k++)                               /* the family: the engine's path weights, the scene's lean */
        w[k] = (uint8_t)((int32_t)MF_HARSH[k] <= budget ? MF_W[path][k] * (SC_FAM[p->scene][k] ? 3u : 1u) : 0u);
    f = (int32_t)rnd_weighted(r, w, MF_N);
    if (!w[f])
        f = MF_GLASS;
    budget -= MF_HARSH[f];
    mo_design(&p->gen, f, rnd_next(r), path, GEN_NOW);
    mo_measure(&p->gen);
    chain_draw(p, r, &budget);
    RING_PUBLISH();
    p_pub = (uint8_t)slot;
    return 1;
}

static void patch_init(void)
{
    memset(vox, 0, sizeof vox);
    memset(vmem, 0, sizeof vmem);
    memset(pbuf, 0, sizeof pbuf);
    memset(nst, 0, sizeof nst);
    wt_init();
    nodes_init();
    mix_g = 0;
    mix_pl = mix_pr = 0;
}
