/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR: the six slots -- a generator (MOLECULE, mol.c) each -- the voices they share, their wiring into the
 * rack (rack.c) and to each other, and the mix.
 *
 * A slot's patch is drawn whole by HOME (slot_draw, the main loop): the generator's sound; its chain -- one, two or
 * three of the rack's modules, in rack order, the first fed by the slot's send, each feeding the next, the last
 * out to the master -- and the share of the slot heard dry; where it sits (level, pan); and, now and then, a
 * modulation by another active slot (ring, amplitude, or a filter opened by the other's level), as Vascular's
 * generators modulate each other. The draw follows Vascular's abstract path: a scene leans the family and the
 * modules, a harshness budget pays for what is rough, no two modules of one character, never two destructive.
 * The modules' own values are not the patch's: they belong to the rack (SEQ draws them).
 *
 * Six voices for the six slots: a note takes a free voice, whatever its slot; all six busy, the quietest release
 * goes, else the oldest note. The keys play the slot chosen; a key lifted ends its note on the slot it was played
 * on. A slot switched off fades its voices and stops feeding the rack; what the rack holds rings out.
 *
 * The audio ISR sees each slot's patch through two buffers, as Tonitor's layers: the main loop writes the one the
 * ISR is not using and publishes it; the ISR takes it at a block's start, fading that slot's voices (they finish
 * on the old buffer). */

#define NSLOT 6
#define NVOICE 6
#define NCHAIN 3
#define FADE 12u                 /* blocks of a fade (~9 ms) */
#define LEVEL_MAX 23
#define LEVEL_UNITY 20
#define PAN_MAX 20
enum { XM_RING, XM_AM, XM_FILTER, XM_N };
static const char *const XM_NAME[XM_N] = {"RING", "AM", "FILTER"};
#define XM_NONE 0xFFu

typedef struct {
    mo_design_t gen;
    uint8_t chain[NCHAIN], nch;  /* the modules, in rack order */
    int32_t send, dry;           /* into the chain's first module; the slot heard directly (Q15) */
    uint8_t xsrc, xkind, xdepth; /* modulated by slot xsrc (XM_NONE: none), how, how deep (0 .. 100) */
    uint8_t scene;
    uint32_t seed, serial;
} slot_patch_t;
static slot_patch_t sbuf[NSLOT][2];
static volatile uint8_t s_pub[NSLOT], s_use[NSLOT];
static volatile uint8_t slot_on[NSLOT];       /* the slot is active (the main loop sets it) */
static uint8_t slot_drawn[NSLOT];             /* (a patch has been drawn for it) */
static volatile int8_t slot_lvl[NSLOT], slot_pan[NSLOT];   /* the level (0 .. LEVEL_MAX) and pan (-PAN_MAX .. PAN_MAX) */
static volatile int32_t slot_meter[NSLOT];    /* each slot's sound, its last peaks (the screen's) */

typedef struct {
    uint8_t note, gate, sounding, slot;
    uint8_t fade;                /* blocks of a fade left (a steal, a patch change, a slot off), 0 = none */
    uint8_t pend, pend_note, pend_slot;
    uint8_t dsg;                 /* the buffer of its slot it plays */
    uint32_t age, pk, rel_at;
    int32_t lvl;
    uint16_t quiet;
} voice_t;
static voice_t vox[NVOICE];
static mo_voice_t vmem[NVOICE] POOL;
static uint32_t blk_clock, age_clock;
static volatile int32_t master_q12 = 4096;
static volatile uint8_t load_pct, shed_req;
static int32_t sl_env[NSLOT], sl_g[NSLOT];    /* each slot's limiter */
static int32_t sl_gg[NSLOT], sl_pl[NSLOT], sl_pr[NSLOT];   /* its level and pan as the last block ended (they glide) */
static int32_t xm_env[NSLOT];                 /* each slot's level as a modulator follows it */
static svf_t xm_f[NSLOT];
static int32_t sbus[NSLOT][CTL], sraw[NSLOT][CTL], mbus[NMOD][CTL];
static uint16_t rm_edge_last[NMOD];           /* where each module's output went, the last block it was fed */
static uint8_t rm_end_last[NMOD];

#define TAIL_FLOOR 11u           /* let go, a voice 66 dB (2^11) under its loudest .. */
#define TAIL_QUIET 276u          /* .. for 0.2 s ends; */
#define TAIL_MAX 41340u          /* .. and none rings on more than 30 s after it was let go */
#define RM_TAIL 2756u            /* a module no slot feeds goes on until its output has been near silence 2 s */

/* ------------------------------------------------------------ the events --- */
enum { EV_ON, EV_OFF, EV_SLOT_OFF, EV_STOP };
typedef struct { uint8_t type, slot, note; } ev_t;
#define EVQ 64u
static ev_t evq[EVQ];
static volatile uint32_t ev_w, ev_r;
static void ev_post(uint32_t type, uint32_t slot, uint32_t note)
{
    if (ev_w - ev_r >= EVQ)
        return;
    evq[ev_w % EVQ].type = (uint8_t)type;
    evq[ev_w % EVQ].slot = (uint8_t)slot;
    evq[ev_w % EVQ].note = (uint8_t)note;
    RING_PUBLISH();
    ev_w++;
}

/* ------------------------------------------------------------ the voices --- */
static void voice_start(uint32_t k, uint32_t slot, uint32_t note)
{
    voice_t *v = &vox[k];
    mo_note_on(&vmem[k], &sbuf[slot][s_use[slot]].gen, (int32_t)note, 1);
    v->note = (uint8_t)note;
    v->slot = (uint8_t)slot;
    v->gate = 1;
    v->sounding = 1;
    v->fade = 0;
    v->pend = 0;
    v->dsg = s_use[slot];
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
static void note_on(uint32_t slot, uint32_t note)
{
    uint32_t k;
    if (!slot_on[slot])
        return;
    for (k = 0; k < NVOICE; k++)                          /* the same note again on the slot: its voice retriggers */
        if (vox[k].sounding && vox[k].gate && vox[k].slot == slot && vox[k].note == note && !vox[k].fade) {
            mo_note_on(&vmem[k], &sbuf[slot][vox[k].dsg].gen, (int32_t)note, 0);
            return;
        }
    k = voice_pick();
    if (!vox[k].sounding) {
        voice_start(k, slot, note);
        return;
    }
    vox[k].gate = 0;                                      /* stolen: faded, then the new note */
    vox[k].fade = FADE;
    vox[k].pend = 1;
    vox[k].pend_note = (uint8_t)note;
    vox[k].pend_slot = (uint8_t)slot;
}
static void note_off(uint32_t slot, uint32_t note)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && vox[k].gate && vox[k].slot == slot && vox[k].note == note) {
            vox[k].gate = 0;
            vox[k].rel_at = blk_clock;
            mo_note_off(&vmem[k], &sbuf[slot][vox[k].dsg].gen);
        }
}
static void slot_fade(uint32_t slot)                     /* every voice of a slot (NSLOT: of all), faded */
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && (slot == NSLOT || vox[k].slot == slot)) {
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
            note_on(e.slot, e.note);
        else if (e.type == EV_OFF)
            note_off(e.slot, e.note);
        else if (e.type == EV_SLOT_OFF)
            slot_fade(e.slot);
        else
            slot_fade(NSLOT);
    }
}
/* each slot's patch published, taken: its voices fade */
static void slots_take(void)
{
    uint32_t l;
    for (l = 0; l < NSLOT; l++)
        if (s_pub[l] != s_use[l]) {
            slot_fade(l);
            s_use[l] = s_pub[l];
        }
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

/* a voice's block, added into its slot's bus; its bookkeeping */
static void voice_block(uint32_t k)
{
    voice_t *v = &vox[k];
    int32_t vb[CTL], pk = 0, *x = sbus[v->slot];
    uint32_t i;
    int alive = mo_render_fp(&vmem[k], &sbuf[v->slot][v->dsg].gen, vb, 0);
    if (v->fade) {                                       /* a fade: linear over FADE blocks */
        int32_t g0 = (int32_t)v->fade * (Q15 / (int32_t)FADE), dg = -(Q15 / (int32_t)FADE) / (int32_t)CTL;
        for (i = 0; i < CTL; i++) {
            g0 += dg;
            vb[i] = mulq15(vb[i], g0);
        }
        if (!--v->fade) {
            alive = 0;
            if (v->pend) {
                uint32_t n = v->pend_note, s = v->pend_slot;
                v->sounding = 0;
                for (i = 0; i < CTL; i++)
                    x[i] += vb[i];
                voice_start(k, s, n);
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

/* a slot's limiter, after its voices: never past LIM. The peak of a block sets the gain of the next (instant attack,
 * released over ~200 ms) */
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
        want = g0 + ((want - g0) >> 3);
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
#define SLOT_GAIN 7373           /* Q12, x 1.8: one voice at 0.1 RMS reads -15 dBFS */

/* the modulation of slot t by its source's sound (raw), as Vascular's: the source normalised by its level (so the
 * depth means the same loud or soft); RING: t (1 - d) + t u 1.8 d; AM: t (1 + u d); FILTER: a low-pass on t from 340
 * Hz, opened up to 4 octaves by the source's level */
static void xmod_block(uint32_t t, const slot_patch_t *p)
{
    const int32_t *u = sraw[p->xsrc];
    int32_t *x = sbus[t], d = p->xdepth * (Q15 / 100), pk = 0, nrm;
    uint32_t i;
    for (i = 0; i < CTL; i++)
        pk = maxi(pk, absi(u[i]));
    xm_env[t] = pk > xm_env[t] ? pk : xm_env[t] - (xm_env[t] >> 6);
    nrm = (int32_t)((uint32_t)Q15 * 8192u / (uint32_t)maxi(xm_env[t], 1024));   /* Q13: u at full scale */
    if (p->xkind == XM_FILTER) {
        svf_c c;
        int32_t oct = 551000 + mulq15(clampi(mulq15(pk, nrm << 2), 0, Q15), d) * 8;   /* log2 340 Hz, up to 4 octaves: the
                                                          * source's level now against its loudest of late */
        svf_coef(&c, (int32_t)(exp2_q16(oct) >> 16), 4096);
        for (i = 0; i < CTL; i++)
            x[i] = svf_run(&xm_f[t], &c, x[i], 1);
        return;
    }
    for (i = 0; i < CTL; i++) {
        int32_t uu = clampi((u[i] * (nrm >> 2)) >> 11, -Q15, Q15);   /* the source, normalised, Q15 */
        if (p->xkind == XM_RING)
            x[i] = mulq15(x[i], Q15 - d) + mulq15(mulq15(mulq15(x[i], uu), 58982), d);
        else
            x[i] += mulq15(mulq15(x[i], uu), d);
    }
}

/* a pan as gains (equal power) */
static void pan_gains(int32_t pan_q15, int32_t *pl, int32_t *pr)   /* -Q15 .. Q15 */
{
    uint32_t ph = (uint32_t)(pan_q15 + Q15) * 16384u;   /* 0 .. a quarter turn */
    *pl = sin_ph(ph + 0x40000000u);
    *pr = sin_ph(ph);
}

/* one block: the events, the patches, the voices, the slots, the rack, the master (Q15 stereo, interleaved) */
static void mix_block(int32_t *out)
{
    int32_t l_[CTL], r_[CTL];
    uint32_t i, l, k, m;
    uint16_t edge[NMOD];
    uint8_t end[NMOD], fed[NMOD];
    events_take();
    slots_take();
    if (shed_req) {
        shed(shed_req > 1u);
        shed_req = 0;
    }
    memset(sbus, 0, sizeof sbus);
    memset(mbus, 0, sizeof mbus);
    memset(edge, 0, sizeof edge);
    memset(end, 0, sizeof end);
    memset(fed, 0, sizeof fed);
    for (i = 0; i < CTL; i++)
        l_[i] = r_[i] = 0;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding)
            voice_block(k);
    for (l = 0; l < NSLOT; l++) {                        /* each slot: its gain and limiter, its raw sound kept */
        int32_t pk = 0;
        for (i = 0; i < CTL; i++)
            sbus[l][i] = mulq12(sbus[l][i], SLOT_GAIN);
        lim_block(&sl_env[l], &sl_g[l], sbus[l], CTL);
        for (i = 0; i < CTL; i++)
            pk = maxi(pk, absi(sbus[l][i]));
        slot_meter[l] = pk > slot_meter[l] ? pk : slot_meter[l] - (slot_meter[l] >> 4);
    }
    memcpy(sraw, sbus, sizeof sraw);
    for (l = 0; l < NSLOT; l++) {                        /* the slots' modulations of each other */
        const slot_patch_t *p = &sbuf[l][s_use[l]];
        if (slot_on[l] && p->xsrc < NSLOT && p->xsrc != l && slot_on[p->xsrc])
            xmod_block(l, p);
    }
    for (l = 0; l < NSLOT; l++) {                        /* each slot: dry to the master, its send into its chain */
        const slot_patch_t *p = &sbuf[l][s_use[l]];
        int32_t g, pl, pr, dg, dpl, dpr;
        if (!slot_on[l] && !slot_meter[l])
            continue;
        g = slot_lvl[l] ? (int32_t)(exp2_q16((slot_lvl[l] - LEVEL_UNITY) * 16328) >> 2) : 0;   /* Q14, 1.5 dB a step */
        pan_gains(slot_pan[l] * (Q15 / PAN_MAX), &pl, &pr);
        dg = (g - sl_gg[l]) / (int32_t)CTL;
        dpl = (pl - sl_pl[l]) / (int32_t)CTL;
        dpr = (pr - sl_pr[l]) / (int32_t)CTL;
        for (i = 0; i < CTL; i++) {
            int32_t s;
            sl_gg[l] += dg;
            sl_pl[l] += dpl;
            sl_pr[l] += dpr;
            s = (int32_t)(((int64_t)sbus[l][i] * sl_gg[l]) >> 14);
            sbus[l][i] = s;                              /* (after the level: what the chain hears too) */
            s = mulq15(s, p->dry);
            l_[i] += mulq15(s, sl_pl[l]);
            r_[i] += mulq15(s, sl_pr[l]);
        }
        sl_gg[l] = g;
        sl_pl[l] = pl;
        sl_pr[l] = pr;
        if (p->nch && slot_on[l]) {
            uint32_t m0 = p->chain[0];
            for (i = 0; i < CTL; i++)
                mbus[m0][i] += mulq15(sbus[l][i], p->send);
            fed[m0] = 1;
            for (k = 0; k + 1u < p->nch; k++) {
                edge[p->chain[k]] |= (uint16_t)(1u << p->chain[k + 1u]);
                fed[p->chain[k + 1u]] = 1;
            }
            end[p->chain[p->nch - 1u]] = 1;
        }
    }
    for (m = 0; m < NMOD; m++) {                         /* the rack, in its order */
        int32_t pl, pr;
        if (fed[m]) {
            rm_edge_last[m] = edge[m];
            rm_end_last[m] = end[m];
            rm_quiet[m] = 0;
        } else if (rm_quiet[m] >= RM_TAIL)
            continue;                                    /* (silent, and nothing feeds it) */
        else {
            edge[m] = rm_edge_last[m];                   /* (ringing out where it went) */
            end[m] = rm_end_last[m];
        }
        rm_block(m, mbus[m], l_, r_);
        if (rm_meter[m] < 8)
            rm_quiet[m] = (uint16_t)(rm_quiet[m] < 0xFFFEu ? rm_quiet[m] + 1u : rm_quiet[m]);
        else if (!fed[m])
            rm_quiet[m] = 0;
        if (m != RM_PLT && end[m]) {                     /* out, at its column's place (the plate is stereo already) */
            pan_gains(RM_PAN[m % RM_COLS], &pl, &pr);
            for (i = 0; i < CTL; i++) {
                l_[i] += mulq15(mbus[m][i], pl);
                r_[i] += mulq15(mbus[m][i], pr);
            }
        }
        for (k = m + 1u; k < NMOD; k++)
            if ((edge[m] >> k) & 1u)
                for (i = 0; i < CTL; i++)
                    mbus[k][i] += mbus[m][i];
    }
    for (i = 0; i < CTL; i++) {
        out[2u * i] = ceiling(mulq12(l_[i], master_q12));
        out[2u * i + 1u] = ceiling(mulq12(r_[i], master_q12));
    }
    blk_clock++;
}

static int slots_sounding(void)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding)
            return 1;
    return 0;
}
/* the modules a slot's chain passes through, as a mask (the screen's) */
static uint32_t slot_mods(uint32_t l)
{
    const slot_patch_t *p = &sbuf[l][s_use[l]];
    uint32_t k, m = 0;
    for (k = 0; k < p->nch; k++)
        m |= 1u << p->chain[k];
    return m;
}

/* ------------------------------------------------------- the draw (main loop) --- */
/* after Vascular's abstract path:
 *   - a SCENE leans the family (x 3) and the modules (x 2.8), sets the chance of a shared tail (the plate at the end)
 *     and of a modulation by another slot:
 *       FIELD      glass, formants, clouds; the filter, the vowel, the flanger, the plate
 *       MATERIAL   metals, glass, reeds, impacts, pulsars; the modal, the string, the spring, the plate
 *       FEEDBACK   bruits, drones, pulses, reeds; the shift, the tape, the flanger, the fold
 *       EROSION    clouds, freezes, shatters, bruits; the mulaw, the fold, the reverse, the tape
 *   - a HARSHNESS budget of 0, 1 or 2 (0.4, 0.4, 0.2) pays for the rough families and modules
 *   - the chain: none one time in twenty, else one module, a second 0.45, a third 0.25; never two of a character,
 *     never two destructive; the plate added at the end by the scene's chance; in rack order
 *   - the dry share 0.15 .. 0.6 with a chain (Vascular's processors are 0.62 wet), all of it without one
 *   - the level a little under unity, the pan anywhere */
enum { SC_FIELD, SC_MATERIAL, SC_FEEDBACK, SC_EROSION, SC_N };
static const char *const SC_NAME[SC_N] = {"FIELD", "MATERIAL", "FEEDBACK", "EROSION"};
static const uint8_t SC_PATH[SC_N] = {PATH_AMBIENT, PATH_OPEN, PATH_EXP, PATH_EXP};
static const uint8_t SC_TAIL[SC_N] = {35, 30, 18, 35};     /* the chance of the shared tail, percent */
static const uint8_t SC_CROSS[SC_N] = {12, 18, 30, 12};    /* .. of a modulation by another slot */
static const uint8_t SC_FAM[SC_N][MF_N] = {
    {0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0},
    {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0},
    {0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0},
    {0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1}};
static const uint8_t MF_HARSH[MF_N] = {0, 0, 2, 1, 1, 0, 0, 0, 0, 1, 1, 0};
static const uint8_t SC_MOD[SC_N][NMOD] = {
    {1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1},
    {0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1},
    {0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0},
    {0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0}};

static void chain_draw(slot_patch_t *p, rnd_t *r, int32_t *budget)
{
    uint32_t n = rnd_chance(r, 5) ? 0u : 1u + (rnd_chance(r, 45) ? 1u : 0u) + (rnd_chance(r, 25) ? 1u : 0u);
    uint32_t i, j, chars = 0, destr = 0, used = 0;
    p->nch = 0;
    for (i = 0; i < n; i++) {
        uint8_t w[NMOD];
        uint32_t k, any = 0;
        for (k = 0; k < NMOD; k++) {
            int ok = !((used >> k) & 1u) && !((chars >> RM_CHAR[k]) & 1u) && (int32_t)RM_HARSH[k] <= *budget &&
                     !(destr && RM_CHAR[k] == CH_DESTRUCTIVE);
            w[k] = (uint8_t)(ok ? RM_W[k] * (SC_MOD[p->scene][k] ? 28u : 10u) / 10u : 0u);
            any |= w[k];
        }
        if (!any)
            break;
        k = rnd_weighted(r, w, NMOD);
        if (!w[k])
            break;
        used |= 1u << k;
        chars |= 1u << RM_CHAR[k];
        if (RM_CHAR[k] == CH_DESTRUCTIVE)
            destr = 1;
        *budget -= RM_HARSH[k];
        p->chain[p->nch++] = (uint8_t)k;
    }
    if (p->nch && p->nch < NCHAIN && !((used >> RM_PLT) & 1u) && !((chars >> CH_SPACE) & 1u) && rnd_chance(r, SC_TAIL[p->scene]))
        p->chain[p->nch++] = RM_PLT;                     /* the shared tail */
    for (i = 0; i < p->nch; i++)                         /* in rack order */
        for (j = i + 1u; j < p->nch; j++)
            if (p->chain[j] < p->chain[i]) {
                uint8_t t = p->chain[i];
                p->chain[i] = p->chain[j];
                p->chain[j] = t;
            }
}

/* the main loop waits (up to ~40 ms) until no voice plays the buffer of slot l it is about to write */
static int slot_buf_free(uint32_t l, uint32_t b)
{
    uint32_t t0 = fm1_ms, k;
    for (;;) {
        int busy = 0;
        for (k = 0; k < NVOICE; k++)
            if (vox[k].sounding && vox[k].slot == l && vox[k].dsg == b)
                busy = 1;
        if (!busy && s_pub[l] == s_use[l])
            return 1;
        if ((uint32_t)(fm1_ms - t0) > 40u)
            return 0;
        fm1_wdt_feed();
    }
}
/* HOME: a new patch for slot l from a seed (family -1: any; else that family, on the scene it had: SHIFT + HOME).
 * Published to the ISR. Returns 0 if the buffer was not free in time */
static uint32_t slot_serial;
static int slot_draw(uint32_t l, int32_t family, uint32_t seed)
{
    uint32_t b = s_use[l] ^ 1u, k, path;
    slot_patch_t *p = &sbuf[l][b];
    const slot_patch_t *was = &sbuf[l][s_use[l]];
    rnd_t rr, *r = &rr;
    int32_t budget, f;
    uint8_t w[MF_N];
    if (!slot_buf_free(l, b))
        return 0;
    {
        uint8_t scene0 = was->scene;
        memset(p, 0, sizeof *p);
        rnd_seed(r, seed ^ 0x534C4F54u);
        p->scene = family >= 0 && slot_drawn[l] ? scene0 : (uint8_t)rnd_below(r, SC_N);
        if (family >= 0 && slot_drawn[l])
            (void)rnd_next(r);
    }
    p->seed = seed;
    p->serial = ++slot_serial;
    budget = rnd_chance(r, 40) ? 0 : rnd_chance(r, 67) ? 1 : 2;
    path = SC_PATH[p->scene];
    if (family >= 0 && family < MF_N)
        f = family;
    else {
        for (k = 0; k < MF_N; k++)
            w[k] = (uint8_t)((int32_t)MF_HARSH[k] <= budget ? MF_W[path][k] * (SC_FAM[p->scene][k] ? 3u : 1u) : 0u);
        f = (int32_t)rnd_weighted(r, w, MF_N);
        if (!w[f])
            f = MF_GLASS;
    }
    budget -= MF_HARSH[f];
    mo_design(&p->gen, f, rnd_next(r), path, GEN_NOW);
    mo_measure(&p->gen);
    chain_draw(p, r, &budget);
    p->send = rnd_in(r, LV(0.6), LV(1.0));
    p->dry = p->nch ? rnd_in(r, LV(0.15), LV(0.6)) : Q15;
    p->xsrc = XM_NONE;
    {   /* a modulation by another active slot, now and then */
        uint32_t cand[NSLOT], nc = 0;
        for (k = 0; k < NSLOT; k++)
            if (k != l && slot_on[k] && slot_drawn[k])
                cand[nc++] = k;
        if (nc && rnd_chance(r, SC_CROSS[p->scene])) {
            static const uint8_t XW[XM_N] = {22, 26, 26};
            p->xsrc = (uint8_t)cand[rnd_below(r, (int32_t)nc)];
            p->xkind = (uint8_t)rnd_weighted(r, XW, XM_N);
            p->xdepth = (uint8_t)rnd_in(r, 35, 95);
        }
    }
    slot_lvl[l] = (int8_t)rnd_in(r, LEVEL_UNITY - 5, LEVEL_UNITY - 1);
    slot_pan[l] = (int8_t)rnd_in(r, -16, 16);
    RING_PUBLISH();
    s_pub[l] = (uint8_t)b;
    slot_drawn[l] = 1;
    return 1;
}

static void slots_init(void)
{
    uint32_t l;
    memset(vox, 0, sizeof vox);
    memset(vmem, 0, sizeof vmem);
    memset(sbuf, 0, sizeof sbuf);
    for (l = 0; l < NSLOT; l++) {
        s_pub[l] = s_use[l] = 0;
        slot_on[l] = 0;
        slot_drawn[l] = 0;
        slot_lvl[l] = LEVEL_UNITY - 2;
        slot_pan[l] = 0;
        sl_g[l] = Q15;
        sl_env[l] = 0;
        sl_gg[l] = sl_pl[l] = sl_pr[l] = 0;
        sbuf[l][0].xsrc = sbuf[l][1].xsrc = XM_NONE;
    }
    wt_init();
    rack_init();
}
