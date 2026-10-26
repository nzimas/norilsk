/* SPDX-License-Identifier: GPL-3.0-only
 * NORILSK: sessions and their snapshots -- Tonitor's sessions (sessions.c, the same author), and Bigorna's patterns
 * as snapshots (projects.c).
 *
 * A SESSION is the whole machine at a moment: every generator (its sound, its cables, its modulation by another,
 * level, pan, mute and its fades, sends), every effect slot (its effects and their values, its cables, level, pan,
 * mute), the master delay and reverb, the six slots' LFOs, the scale and root, the octave, the slot chosen, the
 * loops' move along the scale; every looper's loops; the latched notes; and its 16 SNAPSHOTS. 64 slots (Tonitor's).
 * A SNAPSHOT is the machine's state inside a session: all of the above but the loops, the latched notes, the octave
 * and the slot chosen (they go on as they are). Stored and recalled in RAM (SHIFT + SAVE: the SNAPSHOTS page); kept in
 * the flash with the session when it is saved.
 *
 * A sound is kept as what draws it again bit for bit (its family, seed, path and generation: mol.c's contract) and
 * the level it was measured at; an effect slot's patch whole (its effects and values: a later build that draws
 * recipes otherwise does not change it).
 *
 * In flash, in the area Tonitor and Bigorna keep theirs in (0x97000 .. 0xE0000, 73 sectors of 4 KiB). A session is
 * one record, from the start of a sector over as many as it needs (Tonitor's): a head (32 B: magic "MOL1", its number,
 * format, length, a sequence number -- the newest copy wins --, a CRC of the body, one of the head; and a word that
 * stays 0xFFFFFFFF while the record lives), then the body. Saving over a session writes the new copy into free
 * sectors first and only then retires the old one, by programming its last word to zero (no erase): a power cut in
 * between leaves the old or both, never neither.
 * NORILSK, unlike Tonitor: the area is shared with the other firmwares this FM-1 runs, so a sector is written only
 * if it is blank or holds a record of Norilsk's own that has been retired; anything else in the area (Tonitor's
 * sessions, Bigorna's, SLOOP's projects) is never erased. Erasing a sector silences the audio for a moment: the
 * main loop blanks Norilsk's retired sectors one at a time while nothing sounds (ses_idle). */

#define SES_LO 0x00097000u
#define SES_HI 0x000E0000u
#define SES_SECT 4096u
#define SES_NSECT ((SES_HI - SES_LO) / SES_SECT)
#define SES_MAGIC 0x314C4F4Du    /* "MOL1" */
#define SES_LIVE 0xFFFFFFFFu     /* the head's last word while the record lives */
#define SES_FORMAT 3u            /* (3 from build 19: the snapshots whole, their play too; 2 from build 18: eight
                                  * modulation slots, the DISINTEGRATOR; 1 and 2 still read) */
#define SES_MAX 73u              /* records in the list (one a sector at most) */

/* the flash, for the target (main.c) or the host test (host.c); offsets from the flash's start */
static int ses_flash_read(uint32_t off, void *p, uint32_t n);
static int ses_flash_erase(uint32_t off);   /* a 4 KiB sector */
static int ses_flash_prog(uint32_t off, const void *p, uint32_t n);
static void ses_isr_sync(void);             /* (the ISR has taken every event posted: main.c, host.c) */

typedef struct {
    uint32_t magic;
    uint16_t num, format;
    uint32_t len;                /* head and body, bytes */
    uint32_t seq;
    uint16_t body_crc, rsv0;
    uint32_t rsv1;
    uint16_t head_crc, rsv2;     /* head_crc: of the 24 bytes before it */
    uint32_t live;               /* SES_LIVE; 0: retired */
} ses_head_t;
typedef char ses_head_is_32[sizeof(ses_head_t) == 32u ? 1 : -1];

/* what a sector holds, as the scan found it */
enum { SC_UNKNOWN, SC_BLANK, SC_USED, SC_MINE, SC_OTHER };   /* (MINE: Norilsk's, retired: may be erased) */
static struct {
    uint8_t n;                   /* sessions found */
    uint16_t num[SES_MAX];       /* sorted by number */
    uint8_t sect[SES_MAX];       /* the first sector of each */
    uint8_t nsect[SES_MAX];
    uint32_t seq;                /* the highest sequence number seen */
    uint8_t st[SES_NSECT];       /* SC_* */
    uint16_t current;            /* the session loaded, saved or chosen, 0 = none */
    uint8_t ok;                  /* the flash is the expected part */
} ses;

/* ------------------------------------------------------- the machine's state --- */
typedef struct {
    gen_keep_t g;
    int8_t lvl, pan;
    uint8_t mute, rsend, dsend, dis;   /* dis: KNOB 1's DISINTEGRATOR, 0 .. 100 (build 18; 0 before) */
    uint8_t fin, fout;           /* MUTE FADE IN, MUTE FADE OUT: half seconds, 0 OFF (build 39; 0 before) */
} st_gen_t;
typedef struct {
    fx_patch_t f;
    int8_t lvl, pan;
    uint8_t mute, rsv;           /* rsv: KNOB 2's FX MIX of this lane + 1 (build 36; 0 before) */
} st_fx_t;
/* how a generator is played (build 19): its latched notes, and how its loops go -- whether they play, which are held
 * out, their direction, speed, VARY, their own move along the scale (the loops' recordings are the session's) */
#define ST_LATCH 8u              /* latched notes kept (the four voices sound at most four) */
typedef struct {
    uint32_t latch;              /* the keys latched (no more than ST_LATCH) */
    uint8_t note[ST_LATCH];      /* their notes, in the keys' order */
    uint8_t play, dmode, rate, vary;
    uint16_t mute;
    int8_t lxt;
    uint8_t rsv;
} st_play_t;
typedef struct {
    st_gen_t g[NGEN];
    st_fx_t x[NFX];
    int8_t rvp[4], dlp[4];
    mod_t mod[NSLOT];
    uint8_t scale, root, fxmix, drive;   /* fxmix: KNOB 2's effects' mix + 1 (build 33; 0 before: kept at 100, as it
                                          * was); drive: the master's DRIVE (build 37; 0 before: off) */
    /* (build 19: the rest of the machine; a state of an older session has kept 0 and leaves it as it is) */
    st_play_t p[NGEN];
    int8_t oct, lx;              /* the octave; every loop's move along the scale */
    uint8_t path, kept;          /* the draw's path; 1: this part was kept */
} st_t;
#define ST2_SIZE ((uint32_t)__builtin_offsetof(st_t, p))   /* (a state of format 1 converted, or 2: the part before) */
/* (a session of format 1, before build 18: four modulation slots a slot) */
typedef struct {
    mlfo_t lfo[2];
    mslot_t s[NMSLOT_OLD];
} mod1_t;
typedef struct {
    st_gen_t g[NGEN];
    st_fx_t x[NFX];
    int8_t rvp[4], dlp[4];
    mod1_t mod[NSLOT];
    uint8_t scale, root, rsv[2];
} st1_t;
typedef char st1_is_360[sizeof(st1_t) == 360u ? 1 : -1];
static void st_from1(st_t *t, const st1_t *o)
{
    uint32_t l, k;
    memset(t, 0, sizeof *t);
    memcpy(t->g, o->g, sizeof t->g);
    memcpy(t->x, o->x, sizeof t->x);
    memcpy(t->rvp, o->rvp, sizeof t->rvp);
    memcpy(t->dlp, o->dlp, sizeof t->dlp);
    for (l = 0; l < NSLOT; l++) {
        memcpy(t->mod[l].lfo, o->mod[l].lfo, sizeof t->mod[l].lfo);
        for (k = 0; k < NMSLOT_OLD; k++)
            t->mod[l].s[k] = o->mod[l].s[k];
    }
    t->scale = o->scale;
    t->root = o->root;
}
static struct {
    st_t s[NSNAP];
    uint16_t used;               /* a bit a snapshot that holds a state */
    int8_t cur;                  /* the snapshot last stored or recalled, -1 none */
} snap;

static void st_capture(st_t *t)
{
    uint32_t l;
    memset(t, 0, sizeof *t);
    for (l = 0; l < NGEN; l++) {
        gen_keep(l, &t->g[l].g);
        t->g[l].lvl = gen_lvl[l];
        t->g[l].pan = gen_pan[l];
        t->g[l].mute = gen_mute[l];
        t->g[l].rsend = (uint8_t)ui.rsend[l];
        t->g[l].dsend = (uint8_t)ui.dsend[l];
        t->g[l].dis = (uint8_t)gen_dis[l];
        t->g[l].fin = gen_fin[l];
        t->g[l].fout = gen_fout[l];
    }
    for (l = 0; l < NFX; l++) {
        t->x[l].f = fbuf[l][f_pub[l]];
        t->x[l].lvl = fx_lvl[l];
        t->x[l].pan = fx_pan[l];
        t->x[l].mute = fx_mute[l];
        t->x[l].rsv = (uint8_t)(fx_wet[l] + 1u);   /* (build 36: each lane's FX MIX, + 1) */
    }
    memcpy(t->rvp, ui.rvp, sizeof t->rvp);
    memcpy(t->dlp, ui.dlp, sizeof t->dlp);
    memcpy(t->mod, slot_mod, sizeof t->mod);
    t->scale = ui.scale;
    t->root = ui.root;
    t->fxmix = (uint8_t)(fx_wet[0] + 1u);
    t->drive = master_drive;
    for (l = 0; l < NGEN; l++) {
        st_play_t *q = &t->p[l];
        const looper_t *lp = &loop[l];
        uint32_t k, n = 0;
        for (k = 0; k < NKEY && n < ST_LATCH; k++)
            if ((ui.latch[l] >> k) & 1u) {
                q->latch |= 1u << k;
                q->note[n++] = ui.latch_note[l][k];
            }
        q->play = (uint8_t)(lp->play && lp->ntrk);
        q->mute = lp->mute;
        q->dmode = lp->dmode;
        q->rate = lp->rate;
        q->vary = lp->vary;
        q->lxt = ui.lxt[l];
    }
    t->oct = ui.oct;
    t->lx = ui.lx;
    t->path = draw_path;
    t->kept = 1;
}
/* the rest of a state given back: the latched notes (those that change let go, the new ones held), the loops' play,
 * mutes, direction, speed, VARY and moves, the octave, the path. A state of an older session (kept 0): left as it is */
static void st_apply_play(const st_t *t)
{
    uint32_t l, k, m;
    if (!t->kept)
        return;
    ui.oct = (int8_t)clampi(t->oct, -3, 3);
    draw_path = (uint8_t)(t->path < DP_N ? t->path : DP_ANY);
    ui.lx = (int8_t)clampi(t->lx, -24, 24);
    for (l = 0; l < NGEN; l++)
        ui.lxt[l] = (int8_t)clampi(t->p[l].lxt, -24, 24);
    lx_post();
    for (l = 0; l < NGEN; l++) {
        const st_play_t *q = &t->p[l];
        const looper_t *lp = &loop[l];
        uint32_t n = 0, want = q->latch & ((1u << NKEY) - 1u);
        for (k = 0; k < NKEY; k++) {
            uint32_t on = (ui.latch[l] >> k) & 1u, w = (want >> k) & 1u, note = w ? q->note[n++ % ST_LATCH] : 0u;
            if (w && (white_ix(k) < 0 || note > 127u))
                w = 0;
            if (on && (!w || ui.latch_note[l][k] != note)) {   /* (let go: its own release) */
                ev_post(EV_OFF, l, ui.latch_note[l][k], 0);
                ui.latch[l] &= ~(1u << k);
                on = 0;
            }
            if (w && !on) {
                ev_post(EV_ON, l, note, 0);
                ev_post(EV_LATCH, l, note, 0);
                ui.latch[l] |= 1u << k;
                ui.latch_note[l][k] = (uint8_t)note;
            }
        }
        if (lp->ntrk) {
            if (lp->dmode != q->dmode && q->dmode <= LD_RAND)
                ev_post(EV_LOOP_MODE, l, 0, q->dmode);
            if (lp->rate != q->rate && q->rate <= LS_DOUBLE)
                ev_post(EV_LOOP_MODE, l, 1, q->rate);
            if (lp->vary != (q->vary != 0))
                ev_post(EV_LOOP_MODE, l, 2, q->vary != 0);
            for (k = 0; k < lp->ntrk; k++)
                if (((lp->mute ^ q->mute) >> k) & 1u)
                    ev_post(EV_LOOP_TRK, l, k, !((q->mute >> k) & 1u));
            m = lp->play != 0;
            if (m != (q->play != 0))
                ev_post(EV_LOOP_PLAY, l, 0, 0);   /* (a toggle: started from their starts, or stopped) */
        }
        ses_isr_sync();                          /* (the events taken: the queue never overflows) */
    }
    ui.force = 1;
}
/* a state's effects' mix (KNOB 2): as kept, or 100 for a state from before build 33 (its effects alone, as it sounded) */
static uint8_t st_fxmix(const st_t *t, uint32_t l)   /* (lane l's: its own from build 36; builds 33 .. 35 one for all) */
{
    return (uint8_t)(t->x[l].rsv ? clampi(t->x[l].rsv - 1, 0, 100) : t->fxmix ? clampi(t->fxmix - 1, 0, 100) : 100);
}
/* the state given back: each part checked; returns SES_OK, or SES_BUSY if a slot could not take its patch in time */
static int st_apply(const st_t *t)
{
    uint32_t l, k;
    int ok = 1;
    for (l = 0; l < NGEN; l++) {
        ok &= gen_set(l, &t->g[l].g);
        gen_lvl[l] = (int8_t)clampi(t->g[l].lvl, 0, LEVEL_MAX);
        gen_pan[l] = (int8_t)clampi(t->g[l].pan, -PAN_MAX, PAN_MAX);
        gen_mute[l] = (uint8_t)(t->g[l].mute != 0);
        ui.rsend[l] = (int8_t)clampi(t->g[l].rsend, 0, 100);
        ui.dsend[l] = (int8_t)clampi(t->g[l].dsend, 0, 100);
        sends_apply(l);
        gen_dis[l] = (int8_t)clampi(t->g[l].dis, 0, 100);
        gen_fin[l] = (uint8_t)(t->g[l].fin <= FADE_MAX ? t->g[l].fin : FADE_MAX);
        gen_fout[l] = (uint8_t)(t->g[l].fout <= FADE_MAX ? t->g[l].fout : FADE_MAX);
    }
    RING_PUBLISH();
    gen_fsnap = (uint8_t)((1u << NGEN) - 1u);    /* (a state given back: its mutes at once, no fade) */
    for (l = 0; l < NFX; l++) {
        ok &= fx_set(l, &t->x[l].f);
        fx_lvl[l] = (int8_t)clampi(t->x[l].lvl, 0, 23);
        fx_pan[l] = (int8_t)clampi(t->x[l].pan, -20, 20);
        fx_mute[l] = (uint8_t)(t->x[l].mute != 0);
    }
    for (k = 0; k < 4u; k++) {
        ui.rvp[k] = (int8_t)clampi(t->rvp[k], 0, 100);
        ui.dlp[k] = (int8_t)clampi(t->dlp[k], 0, 100);
    }
    rv_param(&mrv, ui.rvp[0], ui.rvp[1], ui.rvp[2], ui.rvp[3]);
    mdl_param(ui.dlp[0], ui.dlp[1], ui.dlp[2], ui.dlp[3]);
    for (l = 0; l < NSLOT; l++) {
        mod_t m = t->mod[l];
        mod_check(&m, l);
        slot_mod[l] = m;
    }
    ui.scale = (uint8_t)(t->scale < NSCALE ? t->scale : 1u);
    ui.root = (uint8_t)(t->root % 12u);
    rack_root = SCALE_BASE + ui.root;
    for (l = 0; l < NFX; l++)
        fx_wet[l] = st_fxmix(t, l);
    master_drive = (uint8_t)clampi(t->drive, 0, 100);
    lx_post();
    ui.force = 1;
    return ok ? SES_OK : SES_BUSY;
}

/* ------------------------------------------------------------- the morph --- */
/* A snapshot recalled with a morph time (KNOB 1: 0 .. 30 s) flows into it rather than jumping (build 13):
 *   glided all the way      every level (finer than its steps: the mixer's fine level, the mutes faded through it),
 *                           pan and send; the master reverb's DECAY DAMP MIX and the delay's FEEDBACK DAMP MIX; the
 *                           LFOs' rates, fades and amounts (an amount whose source or destination changes goes out
 *                           in the first half and the new one in in the second); an effect slot's values where the
 *                           same effects stand in the same places
 *   swapped, one at a time  a generator whose sound changes: its held and latched notes cross over, the old sound fading
 *                           out on its voice as the new one comes in on a voice of its own (one at a time: the voices
 *                           are few); an effect slot whose effects change: faded out, changed, faded back in. Each its
 *                           own stretch of the morph, generators and slots taking turns
 *   switched at the middle  what has no between: the scale and root, the LFOs' shapes, sources and destinations, the
 *                           cables (a generator's, a slot's) where nothing else changes, the reverb's SIZE and the
 *                           delay's TIME (moved, they would bend the pitch)
 * At its end the snapshot is given exactly as stored. Recalled again on the way: a new morph from where it is. The
 * snapshot it flows into blinks on the SNAPSHOTS page and on its key until it is there */
#define MORPH_STEPS 60u          /* KNOB 1: 0 .. 60 half seconds */
typedef struct { uint8_t gen, idx, done, swapped; uint32_t at, len; } morph_w_t;   /* a swap, in turn (ms from t0) */
static struct {
    uint8_t on, mid, nw, to;
    uint32_t t0, dur;            /* ms (at most 30 s: el << 15 fits 32 bits) */
    st_t a, b;                   /* from, to */
    int16_t ga[NGEN];            /* each generator's fine level at the start (its mute's fade in it) */
    morph_w_t w[NGEN + NFX];
} morph;
/* a slot whose values may glide: the same effects in the same places -- but not a CLOUD whose values change: glided, it
 * would go on holding the snapshot before's sound (frozen, for ever); it is swapped instead, filled anew with the sound
 * the morph flows into */
static int fx_same_effects(const fx_patch_t *a, const fx_patch_t *b)
{
    uint32_t k;
    if (a->n != b->n)
        return 0;
    for (k = 0; k < a->n && k < FX_N; k++) {
        if (a->e[k].kind != b->e[k].kind)
            return 0;
        if (a->e[k].kind == RM_CLD && memcmp(a->e[k].p, b->e[k].p, 4))
            return 0;
    }
    return 1;
}
AINL int32_t mlerp(int32_t a, int32_t b, int32_t u) { return a + (int32_t)(((int64_t)(b - a) * u + (u >= 0 ? Q15 / 2 : 0)) >> 15); }
static int snap_morph_target(void) { return morph.on ? morph.to : -1; }
static void morph_start(const st_t *to, uint32_t snap_i, uint32_t ms)
{
    uint32_t l, n = 0, slice;
    st_capture(&morph.a);
    morph.b = *to;
    morph.on = 1;
    morph.mid = 0;
    morph.to = (uint8_t)snap_i;
    morph.t0 = fm1_ms;
    morph.dur = ms ? ms : 1u;
    for (l = 0; l < NGEN + NFX; l++) {           /* the swaps: generators and slots taking turns */
        uint32_t g = l & 1u ? NGEN + l / 2u : l / 2u;   /* (g0 f0 g1 f1 g2 f2) */
        int change = g < NGEN ? !gen_same_sound(&morph.a.g[g].g, &morph.b.g[g].g)
                              : !fx_same_effects(&morph.a.x[g - NGEN].f, &morph.b.x[g - NGEN].f);
        if (change) {
            morph.w[n].gen = (uint8_t)(g < NGEN);
            morph.w[n].idx = (uint8_t)(g < NGEN ? g : g - NGEN);
            morph.w[n].done = 0;
            morph.w[n].swapped = 0;
            n++;
        }
    }
    morph.nw = (uint8_t)n;
    slice = n ? morph.dur / n : morph.dur;
    for (l = 0; l < n; l++) {
        morph.w[l].at = l * slice;
        morph.w[l].len = slice ? slice : 1u;
    }
    for (l = 0; l < NGEN; l++) {                 /* (the fine levels from where they are: a mute's fade where it
                                                  * stands, build 39) */
        morph.ga[l] = (int16_t)(gen_fq[l] >= Q15 ? morph.a.g[l].lvl * 256 : mulq15(morph.a.g[l].lvl * 256, gen_fq[l]));
        gen_lvl_q8[l] = morph.ga[l];
    }
    for (l = 0; l < NFX; l++)
        fx_lvl_q8[l] = (int16_t)(morph.a.x[l].mute ? 0 : morph.a.x[l].lvl * 256);
    RING_PUBLISH();
    gen_fine = (uint8_t)((1u << NGEN) - 1u);
    fx_fine = (uint8_t)((1u << NFX) - 1u);
}
/* A snapshot given at once that a slot could not take yet (its last change still under way, a voice still on the
 * buffer to be written: SES_BUSY), or a morph's end that could not: given again every frame until every part has
 * taken it (build 15: it was left half done, the slots not taken keeping the snapshot before) */
static struct { int8_t i; uint32_t t0; } snap_pend = {-1, 0};
#define SNAP_PEND_MS 10000u
static void snap_pend_poll(void)
{
    if (snap_pend.i < 0)
        return;
    if (st_apply(&snap.s[snap_pend.i]) == SES_OK || (uint32_t)(fm1_ms - snap_pend.t0) > SNAP_PEND_MS)
        snap_pend.i = -1;
}
/* the main loop, every frame: the morph a step on */
static void morph_poll(void)
{
    uint32_t el, l, k;
    int32_t u;
    snap_pend_poll();
    if (!morph.on)
        return;
    el = fm1_ms - morph.t0;
    u = el >= morph.dur ? Q15 : (int32_t)((el << 15) / morph.dur);
    /* the swaps due */
    for (k = 0; k < morph.nw; k++) {
        morph_w_t *w = &morph.w[k];
        if (w->done || el < w->at)
            continue;
        if (w->gen) {                            /* a generator: its notes cross over through its stretch */
            if (gen_set_x(w->idx, &morph.b.g[w->idx].g, (w->len * 1378u) / 1000u + 1u))
                w->done = 1;
        } else if (!w->swapped && el >= w->at + w->len * 2u / 5u) {   /* a slot: changed at its quietest */
            if (fx_set(w->idx, &morph.b.x[w->idx].f))
                w->swapped = 1;
        } else if (w->swapped && el >= w->at + w->len)
            w->done = 1;
    }
    /* the levels, pans, sends: glided (an effect slot's level dipped through its swap) */
    for (l = 0; l < NGEN; l++) {
        int32_t qa = morph.ga[l], qb = morph.b.g[l].mute ? 0 : morph.b.g[l].lvl * 256;
        gen_lvl_q8[l] = (int16_t)mlerp(qa, qb, u);
        gen_pan[l] = (int8_t)mlerp(morph.a.g[l].pan, morph.b.g[l].pan, u);
        ui.rsend[l] = (int8_t)mlerp(morph.a.g[l].rsend, morph.b.g[l].rsend, u);
        ui.dsend[l] = (int8_t)mlerp(morph.a.g[l].dsend, morph.b.g[l].dsend, u);
        sends_apply(l);
        gen_dis[l] = (int8_t)mlerp(morph.a.g[l].dis, morph.b.g[l].dis, u);
    }
    for (l = 0; l < NFX; l++) {
        int32_t qa = morph.a.x[l].mute ? 0 : morph.a.x[l].lvl * 256, qb = morph.b.x[l].mute ? 0 : morph.b.x[l].lvl * 256,
                q = mlerp(qa, qb, u), dip = Q15;
        for (k = 0; k < morph.nw; k++) {         /* (its swap: down over the first 2/5, up over the last 2/5) */
            morph_w_t *w = &morph.w[k];
            if (!w->gen && w->idx == l && el >= w->at && el < w->at + w->len) {
                uint32_t e = el - w->at, f = w->len * 2u / 5u ? w->len * 2u / 5u : 1u;
                dip = e < f ? Q15 - (int32_t)((e << 15) / f)
                    : e < w->len - f ? 0 : (int32_t)(((e - (w->len - f)) << 15) / f);
            }
        }
        fx_lvl_q8[l] = (int16_t)mulq15(q, clampi(dip, 0, Q15));
        fx_pan[l] = (int8_t)mlerp(morph.a.x[l].pan, morph.b.x[l].pan, u);
        if (fx_same_effects(&morph.a.x[l].f, &morph.b.x[l].f) && f_pub[l] == f_use[l] && fx_phase[l] == FS_ON) {
            fx_patch_t *f = &fbuf[l][f_use[l]];  /* (the same effects: their values glided, in place) */
            uint32_t j;
            for (k = 0; k < f->n && k < FX_N; k++)
                for (j = 0; j < 4u; j++)
                    f->e[k].p[j] = (uint8_t)mlerp(morph.a.x[l].f.e[k].p[j], morph.b.x[l].f.e[k].p[j], u);
        }
    }
    for (l = 0; l < NFX; l++)                           /* (KNOB 2's mixes, glided) */
        fx_wet[l] = (uint8_t)mlerp(st_fxmix(&morph.a, l), st_fxmix(&morph.b, l), u);
    master_drive = (uint8_t)mlerp(clampi(morph.a.drive, 0, 100), clampi(morph.b.drive, 0, 100), u);   /* (the drive) */
    /* the master: all but SIZE and TIME glided */
    for (k = 1; k < 4u; k++) {
        ui.rvp[k] = (int8_t)mlerp(morph.a.rvp[k], morph.b.rvp[k], u);
        ui.dlp[k] = (int8_t)mlerp(morph.a.dlp[k], morph.b.dlp[k], u);
    }
    ui.rvp[0] = morph.mid ? morph.b.rvp[0] : morph.a.rvp[0];
    ui.dlp[0] = morph.mid ? morph.b.dlp[0] : morph.a.dlp[0];
    rv_param(&mrv, ui.rvp[0], ui.rvp[1], ui.rvp[2], ui.rvp[3]);
    mdl_param(ui.dlp[0], ui.dlp[1], ui.dlp[2], ui.dlp[3]);
    /* the LFOs: rates and fades glided; an amount glided where it stays on the same course, else out and in */
    for (l = 0; l < NSLOT; l++) {
        mod_t m = morph.mid ? morph.b.mod[l] : morph.a.mod[l];
        const mod_t *ma = &morph.a.mod[l], *mb = &morph.b.mod[l];
        for (k = 0; k < 2u; k++) {
            m.lfo[k].rate = (uint8_t)mlerp(ma->lfo[k].rate, mb->lfo[k].rate, u);
            m.lfo[k].fade = (uint8_t)mlerp(ma->lfo[k].fade, mb->lfo[k].fade, u);
        }
        for (k = 0; k < NMSLOT; k++) {
            if (ma->s[k].src == mb->s[k].src && ma->s[k].dst == mb->s[k].dst)
                m.s[k].amt = (int8_t)mlerp(ma->s[k].amt, mb->s[k].amt, u);
            else
                m.s[k].amt = (int8_t)(morph.mid ? mlerp(0, mb->s[k].amt, clampi(2 * u - Q15, 0, Q15))
                                                : mlerp(ma->s[k].amt, 0, clampi(2 * u, 0, Q15)));
        }
        slot_mod[l] = m;
    }
    /* the middle: what has no between */
    if (!morph.mid && u >= Q15 / 2) {
        morph.mid = 1;
        st_apply_play(&morph.b);                 /* (the latched notes and the loops' play: at the middle) */
        ui.scale = morph.b.scale < NSCALE ? morph.b.scale : 1u;
        ui.root = (uint8_t)(morph.b.root % 12u);
        rack_root = SCALE_BASE + ui.root;
        lx_post();
        for (l = 0; l < NGEN; l++)               /* (a generator whose sound stays: its new cables, no fade) */
            if (gen_same_sound(&morph.a.g[l].g, &morph.b.g[l].g))
                gen_set(l, &morph.b.g[l].g);
        for (l = 0; l < NFX; l++)                /* (a slot whose effects stay: its new outs and cables, in place) */
            if (fx_same_effects(&morph.a.x[l].f, &morph.b.x[l].f) && f_pub[l] == f_use[l]) {
                fx_patch_t *f = &fbuf[l][f_use[l]];
                f->out = morph.b.x[l].f.out;
                for (k = 0; k < FX_N; k++)
                    f->xto[k] = (uint16_t)(morph.b.x[l].f.xto[k] & ~((1u << ((l + 1u) * FX_N)) - 1u));
            }
        ui.force = 1;
    }
    /* the end: the snapshot exactly */
    if (u >= Q15) {
        for (k = 0; k < morph.nw && morph.w[k].done; k++)
            ;
        if (k < morph.nw && el < morph.dur + 3000u)
            return;                              /* (a swap still on its way in: a moment more) */
        if (st_apply(&morph.b) != SES_OK) {
            if (el < morph.dur + 3000u)
                return;
            snap_pend.i = (int8_t)morph.to;      /* (still not taken: given again until it is) */
            snap_pend.t0 = fm1_ms;
        }
        for (k = 0; k < morph.nw; k++)           /* (the clouds swapped in on the way: the sound now, again) */
            if (!morph.w[k].gen && morph.w[k].swapped)
                fx_refill(morph.w[k].idx);
        RING_PUBLISH();
        gen_fine = 0;
        fx_fine = 0;
        morph.on = 0;
        snap.cur = (int8_t)morph.to;
        ui.force = 1;
    }
}

/* ------------------------------------------------------------ the signal flow --- */
/* a state of a session of builds 28 .. 35 saved in MESH (rack.c SES_LANES), rewired into lanes as it loads: each
 * generator into its own effect slot alone, no cables between slots, the slots centre, left, right -- nothing redrawn */
static void st_lanes(st_t *t)
{
    uint32_t l, k;
    for (l = 0; l < NGEN; l++) {
        gen_keep_t *g = &t->g[l].g;
        uint8_t e = (uint8_t)(g->nent && g->ent[0] / FX_N == l ? g->ent[0] : l * FX_N);
        memset(g->ent, 0, sizeof g->ent);
        g->ent[0] = e;
        g->nent = 1;
        if (g->dry >= Q15)
            g->dry = LV(0.2);
    }
    for (l = 0; l < NFX; l++) {
        for (k = 0; k < FX_N; k++)
            t->x[l].f.xto[k] = 0;
        t->x[l].pan = LANE_PAN[l];
    }
}

/* the snapshots (SHIFT + SAVE): in RAM, with the session */
static void snap_store(uint32_t i)
{
    if (i >= NSNAP)
        return;
    snap_pend.i = -1;
    st_capture(&snap.s[i]);
    snap.used |= (uint16_t)(1u << i);
    snap.cur = (int8_t)i;
}
static int snap_recall(uint32_t i)
{
    if (i >= NSNAP || !((snap.used >> i) & 1u))
        return SES_FAIL;
    snap_pend.i = -1;
    if (ui.morph) {                              /* (a morph time: it flows there) */
        morph_start(&snap.s[i], i, ui.morph * 500u);
        return SES_OK;
    }
    if (morph.on) {                              /* (a morph on its way: given up, the snapshot at once) */
        morph.on = 0;
        gen_fine = fx_fine = 0;
    }
    snap.cur = (int8_t)i;
    if (st_apply(&snap.s[i]) != SES_OK) {        /* (a slot not ready: the rest given in the frames to come) */
        snap_pend.i = (int8_t)i;
        snap_pend.t0 = fm1_ms;
    }
    st_apply_play(&snap.s[i]);
    return SES_OK;
}
static void snap_clear(uint32_t i)
{
    if (i >= NSNAP)
        return;
    snap.used &= (uint16_t)~(1u << i);
    if (snap_pend.i == (int8_t)i)
        snap_pend.i = -1;
    if (snap.cur == (int8_t)i)
        snap.cur = -1;
}
static int snap_has(uint32_t i) { return i < NSNAP && ((snap.used >> i) & 1u); }
static int snap_current(void) { return snap.cur; }

/* ------------------------------------------------------------ the stream --- */
/* A record goes to and from the flash through one sector's buffer, a sector at a time (Tonitor's): counted first (its
 * length and CRC, for its head), then written; read back through the same buffer */
static uint8_t ses_chunk[SES_SECT];
static uint16_t ses_crc(const uint8_t *p, uint32_t n, uint32_t c)   /* CRC16, poly 0x1021 */
{
    uint32_t i;
    while (n--) {
        c ^= (uint32_t)*p++ << 8;
        for (i = 0; i < 8u; i++)
            c = c & 0x8000u ? (c << 1) ^ 0x1021u : c << 1;
        c &= 0xFFFFu;
    }
    return (uint16_t)c;
}
enum { SS_COUNT, SS_WRITE, SS_READ };
static struct {
    uint8_t mode;
    int err;
    uint32_t base, pos;          /* the record's place in the flash; bytes so far */
    uint16_t crc;                /* of its body (after the head) so far */
    uint32_t window;             /* SS_READ: the part of the record in ses_chunk (its offset), ~0: none */
} ss;
static void ss_begin(uint32_t mode, uint32_t base)
{
    ss.mode = (uint8_t)mode;
    ss.err = 0;
    ss.base = base;
    ss.pos = 0;
    ss.crc = 0;
    ss.window = 0xFFFFFFFFu;
}
static void ss_crc(const uint8_t *b, uint32_t n)       /* (the head is not the body's) */
{
    if (ss.pos + n <= sizeof(ses_head_t))
        return;
    if (ss.pos < sizeof(ses_head_t)) {
        b += sizeof(ses_head_t) - ss.pos;
        n -= sizeof(ses_head_t) - ss.pos;
    }
    ss.crc = ses_crc(b, n, ss.crc);
}
static void ses_out(const void *src, uint32_t n)       /* n bytes out: counted, or into the buffer, a full one written */
{
    const uint8_t *b = src;
    while (n) {
        uint32_t at = ss.pos % SES_SECT, k = SES_SECT - at < n ? SES_SECT - at : n;
        ss_crc(b, k);
        if (ss.mode == SS_WRITE) {
            memcpy(ses_chunk + at, b, k);
            if (at + k == SES_SECT && ses_flash_prog(ss.base + ss.pos + k - SES_SECT, ses_chunk, SES_SECT))
                ss.err = 1;
        }
        ss.pos += k;
        b += k;
        n -= k;
    }
}
static void ss_end(void)                               /* (the last part of a sector) */
{
    uint32_t at = ss.pos % SES_SECT;
    if (ss.mode == SS_WRITE && at && ses_flash_prog(ss.base + ss.pos - at, ses_chunk, at))
        ss.err = 1;
}
static void ses_in(void *dst, uint32_t n)              /* n bytes in, through the sector's buffer */
{
    uint8_t *d = dst;
    while (n) {
        uint32_t w = ss.pos / SES_SECT * SES_SECT, at = ss.pos - w, k = SES_SECT - at < n ? SES_SECT - at : n;
        if (ss.window != w) {
            if (ses_flash_read(ss.base + w, ses_chunk, SES_SECT))
                ss.err = 1;
            ss.window = w;
        }
        ss_crc(ses_chunk + at, k);
        if (d) {
            memcpy(d, ses_chunk + at, k);
            d += k;
        }
        ss.pos += k;
        n -= k;
    }
}
static void ses_io(void *p, uint32_t n)                /* (the body is written and read by the same code) */
{
    if (ss.mode == SS_READ)
        ses_in(p, n);
    else
        ses_out(p, n);
}

/* ------------------------------------------------------------- the list --- */
static int ses_head_ok(const ses_head_t *h)
{
    return h->magic == SES_MAGIC && h->len >= sizeof(ses_head_t) && h->len <= SES_NSECT * SES_SECT &&
           h->head_crc == ses_crc((const uint8_t *)h, 24u, 0);
}
static void ses_list_add(uint32_t num, uint32_t sect, uint32_t nsect)
{
    uint32_t i = ses.n, k;
    if (ses.n >= SES_MAX)
        return;
    while (i > 0 && ses.num[i - 1u] > num)
        i--;
    for (k = ses.n; k > i; k--) {
        ses.num[k] = ses.num[k - 1u];
        ses.sect[k] = ses.sect[k - 1u];
        ses.nsect[k] = ses.nsect[k - 1u];
    }
    ses.num[i] = (uint16_t)num;
    ses.sect[i] = (uint8_t)sect;
    ses.nsect[i] = (uint8_t)nsect;
    ses.n++;
}
static int ses_find(uint32_t num)
{
    uint32_t i;
    for (i = 0; i < ses.n; i++)
        if (ses.num[i] == num)
            return (int)i;
    return -1;
}
static void ses_list_drop(uint32_t i)
{
    for (; i + 1u < ses.n; i++) {
        ses.num[i] = ses.num[i + 1u];
        ses.sect[i] = ses.sect[i + 1u];
        ses.nsect[i] = ses.nsect[i + 1u];
    }
    ses.n--;
}
/* retire a record in place: its last word programmed to zero (bits only go 1 -> 0: no erase); its sectors Norilsk's
 * to blank */
static void ses_kill(uint32_t sect, uint32_t nsect)
{
    uint32_t zero = 0, k;                              /* (in RAM: the flash is programmed from RAM only) */
    ses_flash_prog(SES_LO + sect * SES_SECT + 28u, &zero, 4u);
    for (k = 0; k < nsect && sect + k < SES_NSECT; k++)
        ses.st[sect + k] = SC_MINE;
}
/* a record's body as its head says (its CRC): whole */
static int ses_body_ok(uint32_t sect, const ses_head_t *h)
{
    ss_begin(SS_READ, SES_LO + sect * SES_SECT);
    ses_in(0, h->len);
    return !ss.err && ss.crc == h->body_crc;
}
/* boot: the records in the area. Two copies of a number (a save cut short): the newer one stays -- if it is whole
 * (a power cut while its sectors were written leaves its head and part of its body: the older one stays then) */
static void ses_scan(int flash_ok)
{
    uint32_t s = 0;
    memset(&ses, 0, sizeof ses);
    ses.ok = (uint8_t)flash_ok;
    if (!flash_ok)
        return;
    while (s < SES_NSECT) {
        ses_head_t h;
        uint32_t ns;
        ses_flash_read(SES_LO + s * SES_SECT, &h, sizeof h);
        if (h.magic == 0xFFFFFFFFu) {                   /* blank, or so its start says: checked when wanted */
            ses.st[s++] = SC_UNKNOWN;
            continue;
        }
        if (h.magic != SES_MAGIC) {                     /* another firmware's: never touched */
            ses.st[s++] = SC_OTHER;
            continue;
        }
        if (!ses_head_ok(&h)) {                         /* Norilsk's, its head cut short: blanked in time */
            ses.st[s++] = SC_MINE;
            continue;
        }
        ns = (h.len + SES_SECT - 1u) / SES_SECT;
        if (s + ns > SES_NSECT)
            ns = SES_NSECT - s;
        if (h.live != SES_LIVE) {                       /* retired: its sectors Norilsk's to blank -- but a sector of
                                                         * it may have been blanked and begin another record since:
                                                         * each one after the first looked at on its own */
            ses.st[s++] = SC_MINE;
            for (; ns > 1u && s < SES_NSECT; ns--, s++) {
                uint32_t w;
                ses_flash_read(SES_LO + s * SES_SECT, &w, sizeof w);
                if (w == SES_MAGIC || w == 0xFFFFFFFFu)
                    break;                              /* (a record's start, or blank: the scan's own) */
                ses.st[s] = SC_MINE;
            }
            continue;
        }
        {
            int dup = ses_find(h.num);
            if (h.seq > ses.seq)
                ses.seq = h.seq;
            if (dup >= 0) {                             /* keep the newer, if whole */
                ses_head_t o;
                ses_flash_read(SES_LO + ses.sect[dup] * SES_SECT, &o, sizeof o);
                if (o.seq > h.seq ? ses_body_ok(ses.sect[dup], &o) || !ses_body_ok(s, &h)
                                  : !ses_body_ok(s, &h) && ses_body_ok(ses.sect[dup], &o)) {
                    ses_kill(s, ns);
                    s += ns;
                    continue;
                }
                ses_kill(ses.sect[dup], ses.nsect[dup]);
                ses_list_drop((uint32_t)dup);
            }
            if (h.num >= 1u && h.num <= SES_SLOTS)
                ses_list_add(h.num, s, ns);
            for (; ns; ns--, s++)
                ses.st[s] = SC_USED;
        }
    }
}
/* a sector is blank (all 0xFF) */
static int ses_blank(uint32_t s)
{
    uint32_t buf[64], o, i;
    for (o = 0; o < SES_SECT; o += sizeof buf) {
        if (ses_flash_read(SES_LO + s * SES_SECT + o, buf, sizeof buf))
            return 0;
        for (i = 0; i < 64u; i++)
            if (buf[i] != 0xFFFFFFFFu)
                return 0;
    }
    return 1;
}
/* a sector Norilsk may write: blank, or its own retired (erased first) */
static int ses_free(uint32_t s)
{
    if (ses.st[s] == SC_UNKNOWN)
        ses.st[s] = (uint8_t)(ses_blank(s) ? SC_BLANK : SC_OTHER);
    return ses.st[s] == SC_BLANK || ses.st[s] == SC_MINE;
}
/* where n sectors fit: a run of free ones, blank ones if there are (no erase, no gap in the sound) */
static int ses_alloc(uint32_t n)
{
    uint32_t s, k, best = SES_NSECT, best_dirty = 0xFFFFu;
    for (s = 0; s + n <= SES_NSECT; s++) {
        uint32_t d = 0;
        for (k = 0; k < n && ses_free(s + k); k++)
            d += ses.st[s + k] == SC_MINE;
        if (k < n)
            continue;
        if (d < best_dirty) {
            best = s;
            best_dirty = d;
            if (!d)
                break;
        }
    }
    return best < SES_NSECT ? (int)best : -1;
}
/* room for a record of len bytes: blank now (Norilsk's own erased if need be); the first, or -SES_FULL / -SES_FAIL */
/* (build 23) what the last placing and commit saw, for backup.c's 0x55: the sectors taken and their states before;
 * a commit's head as meant and as read back */
static struct {
    int16_t s;
    uint8_t ns, before[4], erased, rsv;
    ses_head_t want, got;
} ses_diag = {-1, 0, {0}, 0, 0, {0}, {0}};
static int ses_place(uint32_t len, uint32_t *nsect)
{
    uint32_t ns = (len + SES_SECT - 1u) / SES_SECT, k;
    int s = ses_alloc(ns);
    *nsect = ns;
    ses_diag.s = (int16_t)s;
    ses_diag.ns = (uint8_t)ns;
    ses_diag.erased = 0;
    for (k = 0; k < 4u; k++)
        ses_diag.before[k] = s >= 0 && k < ns && (uint32_t)s + k < SES_NSECT ? ses.st[(uint32_t)s + k] : 0xFFu;
    if (s < 0)
        return -SES_FULL;
    for (k = 0; k < ns; k++) {
        uint32_t q = (uint32_t)s + k;
        if (ses.st[q] == SC_MINE) {
            if (ses_flash_erase(SES_LO + q * SES_SECT))
                return -SES_FAIL;
            ses.st[q] = SC_BLANK;
            ses_diag.erased |= (uint8_t)(1u << k);
        }
        if (ses.st[q] != SC_BLANK)                     /* (never anything but a blank sector written) */
            return -SES_FAIL;
    }
    for (k = 0; k < ns; k++)                           /* (a write that fails leaves them Norilsk's, to blank) */
        ses.st[(uint32_t)s + k] = SC_MINE;
    return s;
}
/* the record written at sector s is number num's now: its head read back, and only then the old copy retired */
static int ses_commit(uint32_t num, int s, uint32_t ns, const ses_head_t *head)
{
    uint32_t k;
    int old;
    ses_head_t h;
    ses_flash_read(SES_LO + (uint32_t)s * SES_SECT, &h, sizeof h);
    ses_diag.want = *head;
    ses_diag.got = h;
    if (memcmp(&h, head, sizeof h))
        return SES_FAIL;
    for (k = 0; k < ns; k++)
        ses.st[(uint32_t)s + k] = SC_USED;
    old = ses_find(num);
    if (old >= 0) {
        ses_kill(ses.sect[old], ses.nsect[old]);
        ses.sect[old] = (uint8_t)s;
        ses.nsect[old] = (uint8_t)ns;
    } else
        ses_list_add(num, (uint32_t)s, ns);
    return SES_OK;
}

/* ------------------------------------------------------------- the body --- */
typedef struct {
    uint8_t scale, root, gen, sel;   /* (scale, root: the live state's too; kept here for a quick look) */
    int8_t oct, lx, lxt[NGEN];
    int8_t snap_cur;
    uint16_t snap_used;
    uint8_t morph;               /* KNOB 1's morph time, half seconds (0 in sessions saved before build 13) */
    uint8_t path;                /* the draw's path (rack.c DP_*: 0, anything, before build 18) */
    uint8_t mode;                /* the signal flow: SES_LANES (1); 0 MESH (builds 28 .. 35, and before build 28) */
    uint8_t rsv;
} ses_glob_t;
typedef struct {
    uint8_t ntrk, dmode, rate, vary, play, rsv[3];
    uint16_t mute, nev;
} ses_loop_t;
typedef struct { uint32_t len; uint16_t first, n; int8_t xb, rsv[3]; } ses_trk_t;
static st_t ses_live;                                  /* (the state saved: captured once, counted and written alike) */
static uint8_t ses_lplay[NGEN];                        /* (a session read: whether each generator's loops were playing) */

/* the body, out (SS_COUNT, SS_WRITE: from the machine) or in (SS_READ: into ses_live, the snapshots, the globals of
 * gl, the latches and loops left in the flash for ses_take_loops) */
static uint32_t ses_fmt = SES_FORMAT;                  /* (the format of the record being read) */
static uint8_t ses_mode_save = SES_LANES;             /* (what a save writes as its flow: the host's test writes 0) */
static void ses_body(ses_glob_t *gl)
{
    uint32_t l, k;
    ses_io(gl, sizeof *gl);
    if (ss.mode == SS_READ && ses_fmt == 2u) {          /* (format 2: the states without how they are played) */
        memset(&ses_live, 0, sizeof ses_live);
        ses_io(&ses_live, ST2_SIZE);
        for (l = 0; l < NSNAP; l++) {
            memset(&snap.s[l], 0, sizeof snap.s[l]);
            ses_io(&snap.s[l], ST2_SIZE);
        }
    } else if (ss.mode == SS_READ && ses_fmt == 1u) {   /* (format 1: its states converted, four modulation slots) */
        st1_t o;
        ses_io(&o, sizeof o);
        st_from1(&ses_live, &o);
        for (l = 0; l < NSNAP; l++) {
            ses_io(&o, sizeof o);
            st_from1(&snap.s[l], &o);
        }
    } else {
        ses_io(&ses_live, sizeof ses_live);
        ses_io(snap.s, sizeof snap.s);
    }
    for (l = 0; l < NGEN; l++) {                       /* the latched notes */
        ses_io(&ui.latch[l], sizeof ui.latch[l]);
        ses_io(ui.latch_note[l], NKEY);
    }
    for (l = 0; l < NGEN; l++) {                       /* the loops */
        looper_t *p = &loop[l];
        ses_loop_t h;
        memset(&h, 0, sizeof h);
        if (ss.mode != SS_READ) {
            h.ntrk = p->ntrk;
            h.dmode = p->dmode;
            h.rate = p->rate;
            h.vary = p->vary;
            h.play = p->play;
            h.mute = p->mute;
            h.nev = (uint16_t)p->nev;
        }
        ses_io(&h, sizeof h);
        if (ss.mode == SS_READ) {                      /* (into the looper, which the ISR has let go: ses_load) */
            h.ntrk = (uint8_t)(h.ntrk <= LOOP_TRK ? h.ntrk : 0);
            h.nev = (uint16_t)(h.nev <= LOOP_N ? h.nev : 0);
            p->dmode = (uint8_t)(h.dmode <= LD_RAND ? h.dmode : LD_FWD);
            p->rate = (uint8_t)(h.rate <= LS_DOUBLE ? h.rate : LS_NORMAL);
            p->vary = (uint8_t)(h.vary != 0);
            p->mute = h.mute;
            p->nev = h.nev;
        }
        for (k = 0; k < h.ntrk; k++) {
            ses_trk_t t;
            memset(&t, 0, sizeof t);
            if (ss.mode != SS_READ) {
                t.len = p->trk[k].len;
                t.first = p->trk[k].first;
                t.n = p->trk[k].n;
                t.xb = p->trk[k].xb;
            }
            ses_io(&t, sizeof t);
            if (ss.mode == SS_READ) {
                if (t.first + (uint32_t)t.n > h.nev || t.len < 2u || t.len > 0xFFFFFFu) {
                    h.ntrk = (uint8_t)k;                 /* (a loop that does not check: it and those after dropped) */
                    break;
                }
                p->trk[k].len = t.len;
                p->trk[k].first = t.first;
                p->trk[k].n = t.n;
                p->trk[k].xb = t.xb;
            }
        }
        ses_io(p->ev, h.nev * 4u);
        if (ss.mode == SS_READ) {
            p->ntrk = h.ntrk;                          /* (stopped: ses_load hands them to the ISR, EV_LOOP_SET) */
            ses_lplay[l] = (uint8_t)(h.play && h.ntrk);
        }
    }
}
static void ses_head_fill(ses_head_t *h, uint32_t num, uint32_t len, uint16_t crc)
{
    memset(h, 0, sizeof *h);
    h->magic = SES_MAGIC;
    h->num = (uint16_t)num;
    h->format = SES_FORMAT;
    h->len = len;
    h->seq = ++ses.seq;
    h->body_crc = crc;
    h->head_crc = ses_crc((const uint8_t *)h, 24u, 0);
    h->live = SES_LIVE;
}
static void ses_glob_now(ses_glob_t *g)
{
    uint32_t l;
    memset(g, 0, sizeof *g);
    g->scale = ui.scale;
    g->root = ui.root;
    g->gen = ui.gen;
    g->sel = ui.sel;
    g->oct = ui.oct;
    g->lx = ui.lx;
    for (l = 0; l < NGEN; l++)
        g->lxt[l] = ui.lxt[l];
    g->snap_cur = snap.cur;
    g->snap_used = snap.used;
    g->morph = ui.morph;
    g->path = draw_path;
    g->mode = (uint8_t)ses_mode_save;
}

/* save the machine as session num (a new one, or over the one of that number). Counted, then written a sector at a
 * time; should a loop change in between (a recording ending), again */
/* a session coming in over USB (backup.c, build 22): its sectors reserved and the sector buffer half full between its
 * messages -- a save or a load meanwhile would share both. Until it is committed, abandoned, or silent for 5 s */
static uint8_t ses_ext;
static uint32_t ses_ext_t;
static int ses_locked(void)
{
    if (ses_ext && (uint32_t)(fm1_ms - ses_ext_t) > 5000u)
        ses_ext = 0;                                    /* (abandoned: backup.c sees it and starts again) */
    return ses_ext;
}
static int ses_save(uint32_t num)
{
    uint32_t tries;
    if (!ses.ok)
        return SES_NOFLASH;
    if (ses_locked())
        return SES_BUSY;
    if (num < 1u || num > SES_SLOTS)
        return SES_FAIL;
    st_capture(&ses_live);
    for (tries = 0; tries < 2u; tries++) {
        ses_head_t h;
        ses_glob_t g;
        uint32_t len, ns, l, nev[NGEN];
        uint16_t crc;
        int s;
        ses_glob_now(&g);
        for (l = 0; l < NGEN; l++)
            nev[l] = loop[l].nev | (uint32_t)loop[l].ntrk << 16;
        ss_begin(SS_COUNT, 0);
        ss.pos = sizeof h;
        ses_body(&g);
        len = ss.pos;
        crc = ss.crc;
        s = ses_place(len, &ns);
        if (s < 0)
            return -s;
        ses_head_fill(&h, num, len, crc);
        ss_begin(SS_WRITE, SES_LO + (uint32_t)s * SES_SECT);
        ses_out(&h, sizeof h);
        ses_body(&g);
        ss_end();
        for (l = 0; l < NGEN; l++)
            if (nev[l] != (loop[l].nev | (uint32_t)loop[l].ntrk << 16))
                ss.err = 2;                             /* (the loops changed under it: again) */
        if (ss.err || ss.pos != len || ss.crc != crc)
            continue;                                   /* (its sectors are Norilsk's to blank: ses_place) */
        if (ses_commit(num, s, ns, &h) != SES_OK)
            return SES_FAIL;
        ses.current = (uint16_t)num;
        return SES_OK;
    }
    return SES_FAIL;
}

/* every latched note let go, every loop emptied (the ISR's: through events, waited for) */
static void ses_quiet_all(void)
{
    uint32_t l, k;
    for (l = 0; l < NGEN; l++) {
        for (k = 0; k < NKEY; k++)
            if ((ui.latch[l] >> k) & 1u)
                ev_post(EV_OFF, l, ui.latch_note[l][k], 0);
        ui.latch[l] = 0;
        ev_post(EV_LOOP_CLEAR, l, 0, 0);
    }
    ses_isr_sync();
}
/* the latched notes held again (as ui.latch has them) */
static void ses_latch_all(void)
{
    uint32_t l, k;
    for (l = 0; l < NGEN; l++)
        for (k = 0; k < NKEY; k++)
            if ((ui.latch[l] >> k) & 1u) {
                ev_post(EV_ON, l, ui.latch_note[l][k], 0);
                ev_post(EV_LATCH, l, ui.latch_note[l][k], 0);
            }
}

/* session i of the list loaded: checked whole first (its CRC), then given to the machine */
static int ses_load(uint32_t i)
{
    ses_head_t h;
    ses_glob_t g;
    uint32_t base, l, k;
    if (!ses.ok)
        return SES_NOFLASH;
    if (i >= ses.n)
        return SES_FAIL;
    base = SES_LO + ses.sect[i] * SES_SECT;
    ses_flash_read(base, &h, sizeof h);
    if (!ses_head_ok(&h) || h.format < 1u || h.format > SES_FORMAT)
        return SES_FAIL;
    ss_begin(SS_READ, base);                            /* (the CRC first: nothing taken from a record that fails) */
    ses_in(0, h.len);
    if (ss.err || ss.crc != h.body_crc)
        return SES_FAIL;
    ses_quiet_all();                                    /* (the loopers empty and still: theirs to be written) */
    ss_begin(SS_READ, base);
    ses_in(0, sizeof h);
    ses_fmt = h.format;
    ses_body(&g);
    ses_fmt = SES_FORMAT;
    if (ss.err)
        return SES_FAIL;
    for (l = 0; l < NFX; l++) {                         /* (build 31: DELAY and REVERB out of the slots, in every state) */
        fx_retire(&ses_live.x[l].f);
        for (k = 0; k < NSNAP; k++)
            fx_retire(&snap.s[k].x[l].f);
    }
    morph.on = 0;
    snap_pend.i = -1;
    gen_fine = fx_fine = 0;
    ui.morph = (uint8_t)(g.morph <= MORPH_STEPS ? g.morph : 0);
    draw_path = (uint8_t)(g.path < DP_N ? g.path : DP_ANY);
    snap.used = g.snap_used;
    snap.cur = (int8_t)(g.snap_cur >= 0 && g.snap_cur < (int8_t)NSNAP ? g.snap_cur : -1);
    ui.gen = (uint8_t)(g.gen < NGEN ? g.gen : 0);
    ui.sel = (uint8_t)(g.sel < NSLOT ? g.sel : ui.gen);
    ui.oct = (int8_t)clampi(g.oct, -3, 3);
    ui.lx = (int8_t)clampi(g.lx, -24, 24);
    for (l = 0; l < NGEN; l++) {
        ui.lxt[l] = (int8_t)clampi(g.lxt[l], -24, 24);
        for (k = 0; k < NKEY; k++)
            if (((ui.latch[l] >> k) & 1u) && (ui.latch_note[l][k] > 127u || white_ix(k) < 0))
                ui.latch[l] &= ~(1u << k);
        ui.latch[l] &= (1u << NKEY) - 1u;
    }
    if (g.mode != SES_LANES) {                          /* (a MESH session, builds 28 .. 35: into lanes, every state) */
        st_lanes(&ses_live);
        for (k = 0; k < NSNAP; k++)
            st_lanes(&snap.s[k]);
    }
    st_apply(&ses_live);
    for (l = 0; l < NGEN; l++)                          /* the loops, to the ISR: from their starts */
        ev_post(EV_LOOP_SET, l, 0, ses_lplay[l]);
    ses_latch_all();
    ses.current = ses.num[i];
    return SES_OK;
}
static int ses_has(uint32_t num) { return ses_find(num) >= 0; }
/* the session pages (build 28, Bigorna's): DELETE -- the record retired (its sectors Norilsk's to blank, in time); the
 * session playing, if it was that one, chosen no more */
static int ses_delete(uint32_t num)
{
    int i = ses_find(num);
    if (!ses.ok)
        return SES_NOFLASH;
    if (i < 0)
        return SES_FAIL;
    if (ses_locked())
        return SES_BUSY;
    ses_kill(ses.sect[i], ses.nsect[i]);
    ses_list_drop((uint32_t)i);
    if (ses.current == num)
        ses.current = 0;
    return SES_OK;
}
/* COPY -- session from, as it is in the flash, into slot to (replacing what is there only once the copy is whole and
 * read back): its sectors one at a time through the sector buffer, the head given its new number, sequence and CRC */
static int ses_copy(uint32_t from, uint32_t to)
{
    int i = ses_find(from), s;
    uint32_t ns, k, len, src;
    ses_head_t h;
    if (!ses.ok)
        return SES_NOFLASH;
    if (i < 0 || to < 1u || to > SES_SLOTS || to == from)
        return SES_FAIL;
    if (ses_locked())
        return SES_BUSY;
    src = SES_LO + ses.sect[i] * SES_SECT;
    ses_flash_read(src, &h, sizeof h);
    if (!ses_head_ok(&h))
        return SES_FAIL;
    len = h.len;
    s = ses_place(len, &ns);
    if (s < 0)
        return -s;
    for (k = 0; k < ns; k++) {
        uint32_t n = len - k * SES_SECT < SES_SECT ? len - k * SES_SECT : SES_SECT;
        if (ses_flash_read(src + k * SES_SECT, ses_chunk, n))
            return SES_FAIL;
        if (!k) {
            memcpy(&h, ses_chunk, sizeof h);
            h.num = (uint16_t)to;
            h.seq = ++ses.seq;
            h.live = SES_LIVE;
            h.head_crc = ses_crc((const uint8_t *)&h, 24u, 0);
            memcpy(ses_chunk, &h, sizeof h);
        }
        if (ses_flash_prog(SES_LO + ((uint32_t)s + k) * SES_SECT, ses_chunk, n))
            return SES_FAIL;
    }
    return ses_commit(to, s, ns, &h);
}
static uint32_t ses_current(void) { return ses.current; }
static int ses_load_slot(uint32_t num)
{
    int i = ses_find(num);
    if (ses_locked())
        return SES_BUSY;
    return i < 0 ? SES_FAIL : ses_load((uint32_t)i);
}
/* the first empty slot, 0 = none */
static uint32_t ses_next_number(void)
{
    uint32_t n;
    for (n = 1; n <= SES_SLOTS; n++)
        if (!ses_has(n))
            return n;
    return 0;
}
/* a clean slate in slot num (not saved until SAVE is held): as at power-on -- every slot drawn anew, the loops and
 * latches gone, the snapshots empty, every setting at its default */
static void ses_fresh(uint32_t num)
{
    uint32_t l;
    ses_quiet_all();
    for (l = 0; l < NGEN; l++) {
        ui.lxt[l] = 0;
        gen_lvl[l] = LEVEL_UNITY - 2;
        gen_pan[l] = GEN_PAN0[l];
        gen_mute[l] = 0;
        ui.rsend[l] = RSEND_DEF;
        ui.dsend[l] = 0;
        sends_apply(l);
        gen_dis[l] = 0;
        gen_fin[l] = gen_fout[l] = 0;
    }
    RING_PUBLISH();
    gen_fsnap = (uint8_t)((1u << NGEN) - 1u);
    for (l = 0; l < NFX; l++) {
        fx_lvl[l] = 20 - 2;
        fx_pan[l] = LANE_PAN[l];                         /* (a clean slate is LANES) */
        fx_mute[l] = 0;
    }
    for (l = 0; l < 4u; l++) {
        ui.rvp[l] = RV_DEF[l];
        ui.dlp[l] = DL_DEF[l];
    }
    rv_param(&mrv, ui.rvp[0], ui.rvp[1], ui.rvp[2], ui.rvp[3]);
    mdl_param(ui.dlp[0], ui.dlp[1], ui.dlp[2], ui.dlp[3]);
    memset(slot_mod, 0, sizeof slot_mod);
    scale_random(next_seed());                           /* (build 36: a root and a scale drawn) */
    master_drive = 0;
    for (l = 0; l < NFX; l++)
        fx_wet[l] = FX_WET_DEF;
    ui.lx = 0;
    ui.oct = 0;
    lx_post();
    for (l = 0; l < NSLOT; l++)
        new_patch(l, 0);
    memset(&snap, 0, sizeof snap);
    snap.cur = -1;
    snap_pend.i = -1;
    morph.on = 0;
    gen_fine = fx_fine = 0;
    ses.current = (uint16_t)num;
    ui.force = 1;
}
static void ses_init(int flash_ok)
{
    ses_scan(flash_ok);
    memset(&snap, 0, sizeof snap);
    snap.cur = -1;
}

/* the main loop, now and then: one of Norilsk's retired sectors blanked, while nothing sounds (the erase silences
 * the audio for some tens of ms). Nothing else in the area is ever erased */
static uint32_t ses_idle_at;
static void ses_idle(int quiet)
{
    uint32_t s;
    if (!ses.ok || !quiet || (uint32_t)(fm1_ms - ses_idle_at) < 1000u || ses_locked())
        return;                                         /* (build 24: not while a session comes in over USB -- its
                                                         * sectors look retired until it is committed, and the first,
                                                         * written, was erased under it in silence) */
    ses_idle_at = fm1_ms;
    for (s = 0; s < SES_NSECT; s++)
        if (ses.st[s] == SC_MINE) {
            if (!ses_flash_erase(SES_LO + s * SES_SECT))
                ses.st[s] = SC_BLANK;
            return;
        }
}
/* sectors Norilsk can still write (blank, or its own retired): the SESSIONS page's room */
static uint32_t ses_room(void)
{
    uint32_t s, n = 0;
    for (s = 0; s < SES_NSECT; s++)
        n += ses.st[s] == SC_BLANK || ses.st[s] == SC_MINE || ses.st[s] == SC_UNKNOWN;
    return n;
}
