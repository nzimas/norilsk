/* SPDX-License-Identifier: GPL-3.0-only
 * NORILSK: the three generator slots -- a MOLECULE generator each (mol.c) -- the voices they share, their wiring
 * into the effect slots (rack.c) and to each other, the two cores, and the mix.
 *
 * A generator slot's patch is drawn whole by HOME (gen_draw, the main loop): the generator's sound; the effects it
 * is wired into -- one, two or three, in any effect slot, mostly at the head of its chain, often shared with the
 * other generators, as synthesizers share a pedalboard -- and how much it sends them; the share of it heard dry;
 * and, made by hand (SHIFT and two generators' buttons, from build 25; drawn now and then before), a modulation by
 * another generator (ring, amplitude, or a filter the other's level opens), as Vascular's generators modulate each other. The draw follows Vascular's abstract path: a scene
 * leans the family, a harshness budget pays for what is rough; it leans to the inharmonic and the non-melodic.
 *
 * Four voices for the three generators (the engine is dear: a voice is an instance of it): a note takes a free
 * voice, whatever its slot; all four busy, the quietest release goes, else the oldest note not latched. The keys
 * play the generator chosen last; a key lifted ends its note on the slot it was played on. A key held and SHIFT
 * pressed: its note latched (it sounds on with the key let go); SHIFT held and the latched key pressed: let go. A
 * new patch on a generator moves the notes held and latched on it over to the new sound. A muted generator is not
 * heard and sends nothing; its notes go on, silent, and are heard again when it is unmuted.
 *
 * The audio ISR sees each generator's patch through two buffers, as Tonitor's layers: the main loop writes the one
 * the ISR is not using and publishes it; the ISR takes it at a block's start, its voices fading (they finish on the
 * old buffer) and starting again on the new.
 *
 * The two cores (Tonitor's, after BIGORNA build 32 and Melodee): each block CPU1 runs the effects (on what the
 * generators sent the block before: 0.73 ms later, so one core and two give the same sound) and the voices dealt to
 * it, the dearest first, each to the core with less so far; CPU0 the rest, then everything after. A CPU1 fault, stall
 * or misread retires it until power-off; CPU0 renders alone from then on ("1 CORE" on the screen). */

#define NGEN 3
#define NVOICE 4
#define NENT 3
#define FADE 12u                 /* blocks of a fade (~9 ms) */
#define LEVEL_MAX 23
#define LEVEL_UNITY 20
#define PAN_MAX 20
enum { XM_RING, XM_AM, XM_FILTER, XM_N };
static const char *const XM_NAME[XM_N] = {"RING", "AM", "FILTER"};
#define XM_NONE 0xFFu

typedef struct {
    mo_design_t gen;
    uint8_t ent[NENT], nent;     /* the effects it is wired into: slot x FX_N + place (a place past the slot's last: its
                                  * last) */
    int32_t send, dry;           /* into them; heard directly (Q15) */
    uint8_t xsrc, xkind, xdepth; /* modulated by generator xsrc (XM_NONE: none), how, how deep (0 .. 100) */
    uint8_t scene;
    uint8_t rewired;             /* the same sound, new cables (the first black key): taken without a fade */
    uint16_t xfade;              /* a snapshot's morph: blocks the notes held cross over in, on a voice each (0: as ever) */
    uint32_t seed, serial;
} gen_patch_t;
static gen_patch_t gbuf[NGEN][2];
static volatile uint8_t g_pub[NGEN], g_use[NGEN];
static volatile uint8_t gen_mute[NGEN];
static uint8_t gen_drawn[NGEN];
static volatile int8_t gen_lvl[NGEN], gen_pan[NGEN];   /* the level (0 .. LEVEL_MAX) and pan (-PAN_MAX .. PAN_MAX) */
static volatile int32_t gen_meter[NGEN];      /* each generator's sound, its last peaks (the screen's) */
/* a snapshot's morph (sessions.c): each generator's level finer than its steps (Q8 steps, 0: silent) while it glides --
 * the mute folded in -- a bit a generator when in force */
static volatile int16_t gen_lvl_q8[NGEN];
static volatile uint8_t gen_fine;
/* the modulation (mod.c, after this file): each generator's macros, level (steps), pan, sends (0 .. 100) moved */
static int8_t gen_mac[NGEN][4];
static uint8_t gen_mac_on[NGEN];
static int16_t gen_mod_lvl[NGEN], gen_mod_pan[NGEN], gen_mod_rv[NGEN], gen_mod_dl[NGEN];
static void mod_note(uint32_t l, uint32_t note, uint32_t vel);
static void mod_tick(void);
static void mod_init(void);

enum { SRC_KEY, SRC_LOOP };      /* who plays a note: the keys (and MIDI), or the looper (loop.c) */
typedef struct {
    uint8_t note, gate, sounding, slot, latched;
    uint8_t src, vel;            /* SRC_*; its level, 1 .. 127 (127 full: the keys; less: the looper's VARY) */
    uint8_t fade;                /* blocks of a fade left (a steal, a patch change), 0 = none */
    uint8_t pend, pend_note, pend_slot, pend_latched, pend_src, pend_vel;   /* the fade ends in a note-on */
    uint8_t dsg;                 /* the buffer of its slot it plays */
    uint32_t age, pk, rel_at;
    int32_t lvl;
    uint16_t quiet;
    uint32_t cost;               /* what a block of it took, of late (ticks) */
    uint16_t xf, xflen;          /* a morph's crossfade: blocks left of its long fade out, of how many */
} voice_t;
static voice_t vox[NVOICE];
static mo_voice_t vmem[NVOICE] POOL;
static int32_t vr_buf[NVOICE][CTL] POOL __attribute__((aligned(32)));   /* each voice's sound, this block */
static uint8_t vr_core[NVOICE], vr_alive[NVOICE], rack_core;
static uint32_t vr_ticks[NVOICE];
static uint32_t blk_clock, age_clock;
static volatile int32_t master_q12 = 4096;
static volatile uint8_t load_pct, shed_req;
static int32_t gl_env[NGEN], gl_g[NGEN];      /* each generator's limiter */
static int32_t gl_gg[NGEN], gl_pl[NGEN], gl_pr[NGEN];   /* its level and pan as the last block ended (they glide) */
static int32_t xm_env[NGEN];
static svf_t xm_f[NGEN];
static int32_t gbus[NGEN][CTL], graw[NGEN][CTL];

#define TAIL_FLOOR 11u           /* let go, a voice 66 dB (2^11) under its loudest .. */
#define TAIL_QUIET 276u          /* .. for 0.2 s ends; */
#define TAIL_MAX 41340u          /* .. and none rings on more than 30 s after it was let go; */
#define REL_HOLD 1378u           /* .. but from build 6 a voice let go sounds as it will for 1 s, then is faded out, */
#define REL_FADE 2756u           /* .. 60 dB in 2 s, and ends: nothing lingers once no key is held or latched */

/* ------------------------------------------------------------ the events --- */
enum { EV_ON, EV_OFF, EV_LATCH, EV_STOP,
       EV_LOOP_REC, EV_LOOP_PLAY, EV_LOOP_CLEAR, EV_LOOPS, EV_LOOP_TRK, EV_LOOP_XP, EV_LOOP_MODE, EV_LOOP_SET };   /* (loop.c's) */
typedef struct { uint8_t type, slot, note, val; } ev_t;
#define EVQ 64u
static ev_t evq[EVQ];
static volatile uint32_t ev_w, ev_r;
static void ev_post(uint32_t type, uint32_t slot, uint32_t note, uint32_t val)
{
    if (ev_w - ev_r >= EVQ)
        return;
    evq[ev_w % EVQ].type = (uint8_t)type;
    evq[ev_w % EVQ].slot = (uint8_t)slot;
    evq[ev_w % EVQ].note = (uint8_t)note;
    evq[ev_w % EVQ].val = (uint8_t)val;
    RING_PUBLISH();
    ev_w++;
}

/* ------------------------------------------------------------ the voices --- */
static uint8_t note_vel = 127;   /* the level of the note about to start (the looper's VARY sets it) */
static void voice_start(uint32_t k, uint32_t slot, uint32_t note, uint32_t latched, uint32_t src, uint32_t vel)
{
    voice_t *v = &vox[k];
    mo_note_on(&vmem[k], &gbuf[slot][g_use[slot]].gen, (int32_t)note, 1);
    v->note = (uint8_t)note;
    v->slot = (uint8_t)slot;
    v->gate = 1;
    v->latched = (uint8_t)latched;
    v->src = (uint8_t)src;
    v->vel = (uint8_t)vel;
    v->sounding = 1;
    v->fade = 0;
    v->pend = 0;
    v->dsg = g_use[slot];
    v->age = ++age_clock;
    v->xf = v->xflen = 0;
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
    for (k = 0; k < NVOICE; k++)                          /* the oldest note not latched */
        if (!vox[k].pend && !vox[k].latched && (best == NVOICE || vox[k].age < vox[best].age))
            best = k;
    if (best != NVOICE)
        return best;
    for (k = 0; k < NVOICE; k++)                          /* (all latched: the oldest) */
        if (!vox[k].pend && (best == NVOICE || vox[k].age < vox[best].age))
            best = k;
    return best == NVOICE ? 0 : best;
}
/* a note on slot slot, from the keys or the looper: theirs are told apart, so neither ends the other's */
static void note_on(uint32_t slot, uint32_t note, uint32_t src)
{
    uint32_t k;
    mod_note(slot, note, note_vel);                       /* (the modulation's note sources) */
    for (k = 0; k < NVOICE; k++)                          /* the same note again on the slot: its voice retriggers */
        if (vox[k].sounding && vox[k].gate && vox[k].slot == slot && vox[k].note == note && vox[k].src == src &&
            !vox[k].fade) {
            mo_note_on(&vmem[k], &gbuf[slot][vox[k].dsg].gen, (int32_t)note, 0);
            vox[k].vel = note_vel;
            return;
        }
    k = voice_pick();
    if (!vox[k].sounding) {
        voice_start(k, slot, note, 0, src, note_vel);
        return;
    }
    vox[k].gate = 0;                                      /* stolen: faded, then the new note */
    vox[k].latched = 0;
    vox[k].fade = FADE;
    vox[k].pend = 1;
    vox[k].pend_note = (uint8_t)note;
    vox[k].pend_slot = (uint8_t)slot;
    vox[k].pend_latched = 0;
    vox[k].pend_src = (uint8_t)src;
    vox[k].pend_vel = note_vel;
}
static void note_off(uint32_t slot, uint32_t note, uint32_t src)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++) {
        voice_t *v = &vox[k];
        if (v->sounding && v->gate && v->slot == slot && v->note == note && v->src == src) {
            v->gate = 0;
            v->latched = 0;
            v->rel_at = blk_clock;
            mo_note_off(&vmem[k], &gbuf[slot][v->dsg].gen);
        } else if (v->sounding && v->pend && v->pend_slot == slot && v->pend_note == note && v->pend_src == src)
            v->pend = 0;                                  /* (a note let go while its voice still fades for it) */
    }
}
static void note_latch(uint32_t slot, uint32_t note)      /* a note held, latched */
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++) {
        voice_t *v = &vox[k];
        if (v->sounding && v->gate && v->slot == slot && v->note == note && v->src == SRC_KEY)
            v->latched = 1;
        else if (v->sounding && v->pend && v->pend_slot == slot && v->pend_note == note && v->pend_src == SRC_KEY)
            v->pend_latched = 1;
    }
}
static void all_fade(void)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding) {
            vox[k].gate = 0;
            vox[k].pend = 0;
            vox[k].latched = 0;
            if (!vox[k].fade)
                vox[k].fade = FADE;
        }
}
/* the looper (loop.c, after this file) */
static void loop_hear(uint32_t l, uint32_t on, uint32_t note);
static void loop_event(const ev_t *e);
static void loops_run(void);
static uint32_t loops_playing(void);
static void loops_init(void);
/* the master delay and reverb (master.c, after this file) */
static void master_init(void);
static void master_block(int32_t *l, int32_t *r, const int32_t *dlin, int32_t *rvin, int open);
static volatile int32_t gen_rsend[NGEN], gen_dsend[NGEN];   /* each generator's sends to them, Q15 (KNOB 4, KNOB 3) */
static void events_take(void)
{
    while (ev_r != ev_w) {
        ev_t e = evq[ev_r % EVQ];
        RING_PUBLISH();
        ev_r++;
        if (e.type == EV_ON) {
            loop_hear(e.slot, 1, e.note);                /* (a looper recording keeps the keys' notes) */
            note_on(e.slot, e.note, SRC_KEY);
        } else if (e.type == EV_OFF) {
            loop_hear(e.slot, 0, e.note);
            note_off(e.slot, e.note, SRC_KEY);
        } else if (e.type == EV_LATCH)
            note_latch(e.slot, e.note);
        else if (e.type == EV_STOP)
            all_fade();
        else
            loop_event(&e);
    }
}
/* each generator's patch published, taken: its voices fade; the notes held or latched on it start again on the new
 * sound */
static void gens_take(void)
{
    uint32_t l, k;
    for (l = 0; l < NGEN; l++)
        if (g_pub[l] != g_use[l] && gbuf[l][g_pub[l]].rewired) {   /* rewired: the voices go on, on the new cables (the
                                                                     * sound is the same, byte for byte) */
            for (k = 0; k < NVOICE; k++)
                if (vox[k].sounding && vox[k].slot == l && vox[k].dsg == g_use[l])
                    vox[k].dsg = g_pub[l];
            g_use[l] = g_pub[l];
        } else if (g_pub[l] != g_use[l] && gbuf[l][g_pub[l]].xfade) {   /* a morph: the notes cross over */
            uint32_t old = g_use[l], xl = gbuf[l][g_pub[l]].xfade, j;
            g_use[l] = g_pub[l];
            for (k = 0; k < NVOICE; k++) {
                voice_t *v = &vox[k];
                if (!v->sounding || v->slot != l || v->dsg != old || v->xf)
                    continue;
                if (!v->gate || v->fade || v->pend)
                    continue;                            /* (a tail, or already on its way out: as it is) */
                for (j = 0; j < NVOICE && vox[j].sounding; j++)
                    ;                                    /* a free voice for the new sound */
                if (j == NVOICE) {                       /* (none: the quick change, as ever) */
                    v->pend = 1;
                    v->pend_note = v->note;
                    v->pend_slot = (uint8_t)l;
                    v->pend_latched = v->latched;
                    v->pend_src = v->src;
                    v->pend_vel = v->vel;
                    v->gate = 0;
                    v->latched = 0;
                    v->fade = FADE;
                    continue;
                }
                voice_start(j, l, v->note, v->latched, v->src, v->vel ? v->vel : 127u);
                v->gate = 0;                             /* the old one: out slowly, on the old sound */
                v->latched = 0;
                v->xf = v->xflen = (uint16_t)xl;
            }
        } else if (g_pub[l] != g_use[l]) {
            for (k = 0; k < NVOICE; k++) {
                voice_t *v = &vox[k];
                if (!v->sounding || v->slot != l)
                    continue;
                if (v->gate && !v->fade) {
                    v->pend = 1;
                    v->pend_note = v->note;
                    v->pend_slot = (uint8_t)l;
                    v->pend_latched = v->latched;
                    v->pend_src = v->src;
                    v->pend_vel = v->vel;
                }
                v->gate = 0;
                v->latched = 0;
                if (!v->fade)
                    v->fade = FADE;
            }
            g_use[l] = g_pub[l];
        }
}
/* overload (main.c: a half late, or two near the edge): the quietest release goes; late, the oldest held note not
 * latched too */
static void shed(int held)
{
    uint32_t k, ok = NVOICE;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && !vox[k].fade && !vox[k].gate && (ok == NVOICE || vox[k].lvl < vox[ok].lvl))
            ok = k;
    if (ok == NVOICE && held)
        for (k = 0; k < NVOICE; k++)
            if (vox[k].sounding && !vox[k].fade && !vox[k].latched && (ok == NVOICE || vox[k].age < vox[ok].age))
                ok = k;
    if (ok != NVOICE) {
        vox[ok].gate = 0;
        vox[ok].pend = 0;
        vox[ok].fade = FADE;
    }
}

/* a voice's block, into vr_buf (either core: its own memory only) */
static void voice_render(uint32_t k)
{
    uint32_t t0 = fm1_ticks();
    vr_alive[k] = (uint8_t)mo_render_fp(&vmem[k], &gbuf[vox[k].slot][vox[k].dsg].gen, vr_buf[k],
                                        gen_mac_on[vox[k].slot] ? gen_mac[vox[k].slot] : 0);   /* (its LFOs' macros) */
    vr_ticks[k] = fm1_ticks() - t0;
}
/* .. then its fade, its bookkeeping, into its generator's bus (CPU0) */
static void voice_post(uint32_t k)
{
    voice_t *v = &vox[k];
    int32_t *vb = vr_buf[k], pk = 0, *x = gbus[v->slot];
    uint32_t i;
    int alive = vr_alive[k];
    v->cost = (v->cost * 7u + vr_ticks[k]) / 8u;
    if (v->fade) {                                       /* a fade: linear over FADE blocks */
        int32_t g0 = (int32_t)v->fade * (Q15 / (int32_t)FADE), dg = -(Q15 / (int32_t)FADE) / (int32_t)CTL;
        for (i = 0; i < CTL; i++) {
            g0 += dg;
            vb[i] = mulq15(vb[i], g0);
        }
        if (!--v->fade) {
            alive = 0;
            if (v->pend) {
                uint32_t n = v->pend_note, s = v->pend_slot, lt = v->pend_latched, sr = v->pend_src, vl = v->pend_vel;
                v->sounding = 0;
                for (i = 0; i < CTL; i++)
                    x[i] += vb[i];
                voice_start(k, s, n, lt, sr, vl ? vl : 127u);
                return;                                  /* (its first block next time) */
            }
        }
    }
    if (v->xf) {                                         /* a morph's crossfade: out over xflen blocks, linear */
        int32_t g0 = (int32_t)(((uint32_t)v->xf << 15) / v->xflen), g1 = (int32_t)(((uint32_t)(v->xf - 1u) << 15) / v->xflen),
                dg = (g1 - g0) / (int32_t)CTL;
        for (i = 0; i < CTL; i++) {
            g0 += dg;
            vb[i] = mulq15(vb[i], g0);
        }
        if (!--v->xf)
            alive = 0;
    }
    if (v->vel && v->vel < 127u) {                       /* (a looper's VARY note: its level) */
        int32_t g = (int32_t)v->vel * 258;
        for (i = 0; i < CTL; i++)
            vb[i] = mulq15(vb[i], g);
    }
    if (!v->gate && !v->fade && blk_clock - v->rel_at > REL_HOLD) {   /* let go a while: faded out */
        uint32_t t = blk_clock - v->rel_at - REL_HOLD;
        int32_t g0 = (int32_t)(exp2_q16(-(int32_t)((t - 1u) * (10u * 65536u / REL_FADE))) >> 1);   /* Q15 */
        int32_t g1 = (int32_t)(exp2_q16(-(int32_t)(t * (10u * 65536u / REL_FADE))) >> 1), dg = (g1 - g0) / (int32_t)CTL;
        for (i = 0; i < CTL; i++) {
            g0 += dg;
            vb[i] = mulq15(vb[i], g0);
        }
        if (t >= REL_FADE)
            alive = 0;
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

/* CPU1's share of a block */
static void blk_job(void *ctx)
{
    uint32_t k;
    (void)ctx;
    if (rack_core)
        rack_job_fp();
    for (k = 0; k < NVOICE; k++)
        if (vr_core[k])
            voice_render(k);
}
/* the work dealt: the effects to CPU1, the voices by their cost, the dearest first, each to the core with less so
 * far; rendered on both, CPU1's joined. One core: all here */
static void blk_render(void)
{
    uint32_t k, j, n = 0, s0 = 0, s1;
    uint8_t ord[NVOICE];
    int any1 = 0;
    audio_worker_check();
    for (k = 0; k < NVOICE; k++) {
        vr_core[k] = 0;
        if (!vox[k].sounding)
            continue;
        for (j = n; j > 0u && vox[ord[j - 1u]].cost < vox[k].cost; j--)
            ord[j] = ord[j - 1u];
        ord[j] = (uint8_t)k;
        n++;
    }
    rack_core = 0;
    if (audio_worker_online) {
        rack_core = 1;
        s1 = rack_ticks;
        for (j = 0; j < n; j++) {
            k = ord[j];
            if (s1 <= s0) {
                vr_core[k] = 1;
                s1 += vox[k].cost;
            } else
                s0 += vox[k].cost;
        }
        any1 = audio_worker_submit(blk_job, 0);
    }
    if (!any1) {
        memset(vr_core, 0, sizeof vr_core);
        rack_core = 0;
        rack_job_fp();
    }
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && !vr_core[k])
            voice_render(k);
    if (any1 && !audio_worker_join()) {          /* (CPU1 lost: retired; its share silent this block) */
        for (k = 0; k < NVOICE; k++)
            if (vr_core[k]) {
                memset(vr_buf[k], 0, sizeof vr_buf[k]);
                vr_alive[k] = 1;
                vr_ticks[k] = 0;
            }
        if (rack_core) {
            memset(fx_l, 0, sizeof fx_l);
            memset(fx_r, 0, sizeof fx_r);
        }
    }
}

/* a generator's limiter, after its voices: never past LIM. The peak of a block sets the gain of the next (instant
 * attack, released over ~200 ms) */
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
#define GEN_GAIN 7373            /* Q12, x 1.8: one voice at 0.1 RMS reads -15 dBFS */

/* the modulation of generator t by its source's sound (raw), as Vascular's: the source normalised by its level (so
 * the depth means the same loud or soft); RING: t (1 - d) + t u 1.8 d; AM: t (1 + u d); FILTER: a low-pass on t from
 * 340 Hz, opened up to 4 octaves by the source's level */
static void xmod_block(uint32_t t, const gen_patch_t *p)
{
    const int32_t *u = graw[p->xsrc];
    int32_t *x = gbus[t], d = p->xdepth * (Q15 / 100), pk = 0, nrm;
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
/* the effect a generator's cable reaches: its place, or its slot's last when the slot has fewer */
static uint32_t ent_inst(uint32_t e)
{
    uint32_t s = e / FX_N, k = e % FX_N, n = fbuf[s][f_use[s]].n;
    return s * FX_N + (k < n ? k : n - 1u);
}

/* ------------------------------------------------------ the DISINTEGRATOR --- */
/* KNOB 1, playing (build 18): each generator's own, 0 .. 100 -- an unstable gate on what it makes, after its limiter and
 * before its level, so its cables carry it into the effects too. Its sound goes as a chain of pulses of uneven
 * lengths; a pulse lets it through, or gates it down towards nothing, or stutters a slice of what it just made, or
 * crushes it (its bits, its rate), and may throw its dry part elsewhere in the stereo image, criss-crossing. The more
 * DISINTEGRATOR, the shorter and the less even the pulses, the more of them do something, the deeper the gates and the
 * wider the throws. Ideas from Destroy FX's Skidder (random rate and width, a slope at each edge, a floor, a pan a
 * pulse) and from buffer repeaters (Buffer Override, Rerun); none of their code. Every edge is a short glide (~1.5 ms):
 * no clicks. 0: nothing touched, as before */
#define DIS_N 2048u              /* the slices' line: 93 ms at FS / 2, 8-bit mu-law (RAM: 6 KiB for three) */
enum { DK_OPEN, DK_GATE, DK_STUT, DK_CRUSH };
typedef struct {
    uint8_t ring[DIS_N];
    uint32_t w;                  /* the next pair written */
    int32_t left;                /* samples left in this pulse */
    uint8_t kind, half;          /* DK_*; the pair's first sample seen */
    int32_t g, gt, m;            /* the gate's gain and its target; the wet part's share (Q15) */
    uint32_t s0, sl, sp;         /* a stutter: its slice's first pair, its length in pairs, where it plays (samples) */
    int32_t hold, sh;            /* a crush: the sample held, the bits dropped */
    uint32_t hc, hp;             /* .. held for hp samples */
    int32_t pan, pant, prev;     /* the throw (Q15 of the image) and its target; the pair's first sample */
    rnd_t r;
} dis_t;
static dis_t dis[NGEN];
static volatile int8_t gen_dis[NGEN];          /* each generator's DISINTEGRATOR, 0 .. 100 (ui.c) */
static void dis_pulse(dis_t *d, int32_t a)     /* the next pulse, at amount a (1 .. 100) */
{
    int32_t ms, lo = 25 + (100 - a) * 4, hi = 120 + (100 - a) * 14, ev = a * 85 / 100;
    int wet = d->kind == DK_STUT || d->kind == DK_CRUSH;
    ms = rnd_chance(&d->r, a / 4) ? rnd_in(&d->r, 8, 30) : rnd_in(&d->r, lo, hi);   /* (now and then a splinter) */
    d->left = ms * 44 + ms / 10;
    d->kind = DK_OPEN;
    d->gt = Q15;
    if (rnd_chance(&d->r, ev)) {
        int32_t c = rnd_below(&d->r, 100);
        if (wet || c < 50 || a < 10) {           /* (after a wet pulse: a gate or nothing, so wet never meets wet) */
            d->kind = (uint8_t)(wet && c >= 50 ? DK_OPEN : DK_GATE);
            if (d->kind == DK_GATE)               /* the floor: -6 dB at the least, nothing from 60 up */
                d->gt = mulq15(Q15 * maxi(0, 60 - a) / 120, rnd_in(&d->r, Q15 / 2, Q15));
        } else if (c < 80) {
            d->kind = DK_STUT;
            d->sl = (uint32_t)(rnd_in(&d->r, 10, 90) * 22);   /* 10 .. 90 ms, pairs */
            d->s0 = (d->w + DIS_N - d->sl - 2u) % DIS_N;      /* (the slice just made) */
            d->sp = 0;
        } else {
            d->kind = DK_CRUSH;
            d->hp = 1u + (uint32_t)rnd_below(&d->r, 1 + a / 9);
            d->sh = 4 + a / 14;
            d->hc = 0;
        }
    }
    if (rnd_chance(&d->r, a)) {                  /* a throw: across from where it is, as far as a lets it */
        int32_t t = mulq15(rnd_in(&d->r, Q15 / 4, Q15), a * (Q15 / 100));
        d->pant = d->pant > 0 ? -t : t;
    } else if (rnd_chance(&d->r, 30))
        d->pant = 0;
}
static void dis_block(uint32_t l, int32_t *x)
{
    dis_t *d = &dis[l];
    int32_t a = clampi(gen_dis[l], 0, 100);
    uint32_t i;
    if (!a && d->g == Q15 && !d->m && !d->pan)
        return;                                  /* (none: nothing touched) */
    if (!a) {                                    /* (turned down to 0: back to the sound as it is, gliding) */
        d->kind = DK_OPEN;
        d->gt = Q15;
        d->pant = 0;
        d->left = 1 << 30;
    }
    for (i = 0; i < CTL; i++) {
        int32_t in = x[i], y = in, m1;
        if (d->half) {                           /* (the line, a pair at a time) */
            d->ring[d->w] = mu_enc8((d->prev + in) >> 1);
            d->w = d->w + 1u >= DIS_N ? 0u : d->w + 1u;
        }
        d->prev = in;
        d->half ^= 1u;
        if (a && --d->left <= 0)
            dis_pulse(d, a);
        if (d->kind == DK_STUT) {
            uint32_t pos = d->sp >> 1, e = pos < d->sl - pos ? pos : d->sl - pos;
            y = mu_dec8(d->ring[(d->s0 + pos) % DIS_N]);
            if (e < 33u)                         /* (each repeat's edges: 1.5 ms) */
                y = y * (int32_t)e / 33;
            if (++d->sp >= d->sl * 2u)
                d->sp = 0;
        } else if (d->kind == DK_CRUSH) {
            if (!d->hc)
                d->hold = in < 0 ? -((-in >> d->sh) << d->sh) : (in >> d->sh) << d->sh;
            d->hc = d->hc + 1u >= d->hp ? 0u : d->hc + 1u;
            y = d->hold;
        }
        m1 = d->kind == DK_STUT || d->kind == DK_CRUSH ? Q15 : 0;   /* (into the wet part and out: 2 ms) */
        d->m += d->m < m1 ? mini(372, m1 - d->m) : -mini(372, d->m - m1);
        d->g += d->g < d->gt ? mini(500, d->gt - d->g) : -mini(500, d->g - d->gt);   /* (the gate's slope: 1.5 ms) */
        y = in + (int32_t)(((int64_t)(y - in) * d->m) >> 15);
        x[i] = (int32_t)(((int64_t)y * d->g) >> 15);
    }
    d->pan += (d->pant - d->pan) / 6;            /* (a throw: ~5 ms) */
    if (!a && d->pan > -64 && d->pan < 64)
        d->pan = 0;
}

/* one block: the events, the patches, the effects (on what the generators sent the block before), the voices, the
 * generators, their sends for the next block, the master (Q15 stereo, interleaved) */
static void mix_block(int32_t *out)
{
    int32_t l_[CTL], r_[CTL], pk[NGEN], dlin[CTL], rvin[CTL], g_rs[NGEN], g_ds[NGEN];
    uint32_t i, l, k;
    events_take();
    loops_run();                                        /* (the loops' notes for this block) */
    gens_take();
    mod_tick();                                         /* (the LFOs: this block's offsets) */
    if (shed_req) {
        shed(shed_req > 1u);
        shed_req = 0;
    }
    memcpy(ebus_run, ebus_next, sizeof ebus_run);       /* (what the generators sent the block before: the effects' input) */
    ebus_fed_run = ebus_fed;
    memset(gbus, 0, sizeof gbus);
    {   /* (the effect slots' gate: open while a voice sounds; rack.c) */
        uint32_t any = loops_playing();                 /* (loops going round keep it open between their notes) */
        for (k = 0; k < NVOICE; k++)
            any |= vox[k].sounding;
        rack_voices = (uint8_t)(any != 0);
    }
    {   /* each generator's sends this block, its modulation added */
        for (l = 0; l < NGEN; l++) {
            g_rs[l] = clampi(gen_rsend[l] + gen_mod_rv[l] * (Q15 / 100), 0, Q15);
            g_ds[l] = clampi(gen_dsend[l] + gen_mod_dl[l] * (Q15 / 100), 0, Q15);
        }
    }
    for (k = 0; k < NFX; k++) {                         /* each slot's sends: the mean of its generators' */
        int32_t rs = 0, ds = 0, n = 0;
        for (l = 0; l < NGEN; l++) {
            const gen_patch_t *p = &gbuf[l][g_use[l]];
            uint32_t e;
            if (gen_mute[l])
                continue;
            for (e = 0; e < p->nent; e++)
                if (p->ent[e] / FX_N == k)
                    break;
            if (e < p->nent) {
                rs += g_rs[l];
                ds += g_ds[l];
                n++;
            }
        }
        fx_rsend[k] = n ? rs / n : 0;
        fx_dsend[k] = n ? ds / n : 0;
    }
    blk_render();                                       /* the effects and the voices, on the two cores */
    memset(ebus_next, 0, sizeof ebus_next);
    ebus_fed = 0;
    for (i = 0; i < CTL; i++) {
        l_[i] = fx_l[i];                                /* (the effects' output) */
        r_[i] = fx_r[i];
        dlin[i] = fx_mdl[i];                            /* (.. and their sends to the master delay and reverb) */
        rvin[i] = fx_mrv[i];
    }
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding)
            voice_post(k);
    for (l = 0; l < NGEN; l++) {                        /* each generator: its gain and limiter, its raw sound kept */
        pk[l] = 0;
        for (i = 0; i < CTL; i++)
            gbus[l][i] = mulq12(gbus[l][i], GEN_GAIN);
        lim_block(&gl_env[l], &gl_g[l], gbus[l], CTL);
        for (i = 0; i < CTL; i++)
            pk[l] = maxi(pk[l], absi(gbus[l][i]));
        gen_meter[l] = pk[l] > gen_meter[l] ? pk[l] : gen_meter[l] - (gen_meter[l] >> 4);
    }
    memcpy(graw, gbus, sizeof graw);
    for (l = 0; l < NGEN; l++) {                        /* the generators' modulations of each other */
        const gen_patch_t *p = &gbuf[l][g_use[l]];
        if (p->xsrc < NGEN && p->xsrc != l)
            xmod_block(l, p);
    }
    for (l = 0; l < NGEN; l++)                          /* each one's DISINTEGRATOR */
        dis_block(l, gbus[l]);
    for (l = 0; l < NGEN; l++) {                        /* each: dry to the master, its sends to the effects */
        const gen_patch_t *p = &gbuf[l][g_use[l]];
        int32_t g, pl, pr, dg, dpl, dpr;
        if (!pk[l] && !gl_gg[l])
            continue;
        if ((gen_fine >> l) & 1u) {                      /* (a morph: the level between its steps, the mute in it) */
            int32_t q = gen_lvl_q8[l] > 0 ? clampi(gen_lvl_q8[l] + gen_mod_lvl[l] * 256, 0, LEVEL_MAX * 256) : 0;
            g = q >= 256 ? (int32_t)(exp2_q16(((q - LEVEL_UNITY * 256) * 16328) >> 8) >> 2)
                         : (int32_t)((exp2_q16((1 - LEVEL_UNITY) * 16328) >> 2) * (uint32_t)q >> 8);   /* (to silence) */
        } else
        g = gen_lvl[l] && !gen_mute[l] ? (int32_t)(exp2_q16((clampi(gen_lvl[l] + gen_mod_lvl[l], 0, LEVEL_MAX) - LEVEL_UNITY) * 16328) >> 2) : 0;   /* Q14, 1.5 dB a
                                                         * step; muted: 0, gliding */
        pan_gains(clampi(clampi(gen_pan[l] + gen_mod_pan[l], -PAN_MAX, PAN_MAX) * (Q15 / PAN_MAX) + dis[l].pan, -Q15, Q15),
                  &pl, &pr);                        /* (the DISINTEGRATOR's throw added) */
        dg = (g - gl_gg[l]) / (int32_t)CTL;
        dpl = (pl - gl_pl[l]) / (int32_t)CTL;
        dpr = (pr - gl_pr[l]) / (int32_t)CTL;
        for (i = 0; i < CTL; i++) {
            int32_t s;
            gl_gg[l] += dg;
            gl_pl[l] += dpl;
            gl_pr[l] += dpr;
            s = (int32_t)(((int64_t)gbus[l][i] * gl_gg[l]) >> 14);
            gbus[l][i] = s;                             /* (after the level: what the effects hear too) */
            s = mulq15(s, p->dry);
            l_[i] += mulq15(s, gl_pl[l]);
            r_[i] += mulq15(s, gl_pr[l]);
            dlin[i] += mulq15(s, g_ds[l]);               /* (its dry share on to the master delay and reverb) */
            rvin[i] += mulq15(s, g_rs[l]);
        }
        gl_gg[l] = g;
        gl_pl[l] = pl;
        gl_pr[l] = pr;
        if (pk[l] && gl_gg[l])                          /* (sounding, not muted: its cables carry it) */
            for (k = 0; k < p->nent; k++) {
                uint32_t e = ent_inst(p->ent[k]);
                for (i = 0; i < CTL; i++)
                    ebus_next[e][i] += mulq15(gbus[l][i], p->send);
                ebus_fed |= 1u << e;
            }
    }
    master_block(l_, r_, dlin, rvin, rack_voices);      /* the master delay and reverb, last */
    for (i = 0; i < CTL; i++) {
        out[2u * i] = ceiling(mulq12(l_[i], master_q12));
        out[2u * i + 1u] = ceiling(mulq12(r_[i], master_q12));
    }
    blk_clock++;
}

static int gens_sounding(void)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding)
            return 1;
    return 0;
}
/* the effects a generator is wired into, as a mask of instances (the screen's) */
static uint32_t gen_mods(uint32_t l)
{
    const gen_patch_t *p = &gbuf[l][g_use[l]];
    uint32_t k, m = 0;
    for (k = 0; k < p->nent; k++)
        m |= 1u << ent_inst(p->ent[k]);
    return m;
}

/* ------------------------------------------------------- the draw (main loop) --- */
/* after Vascular's abstract path:
 *   - a SCENE leans the family (x 3) (and, before build 25, the chance of a modulation by another generator):
 *       FIELD      glass, formants, clouds          MATERIAL   metals, glass, reeds, impacts, pulsars
 *       FEEDBACK   bruits, drones, pulses, reeds    EROSION    clouds, freezes, shatters, bruits
 *   - a HARSHNESS budget of 0, 1 or 2 (0.4, 0.4, 0.2) pays for the rough families
 *   - the lean to the inharmonic and the non-melodic (the owner's): metals, bruits, impacts, glass, clouds, shatters,
 *     freezes up; formants and reeds down
 *   - the effects it is wired into: none one time in twenty, else one (0.55), two (0.35) or three (0.1), each in an
 *     effect slot of its own if it can, at the head of its chain mostly (0.7), else the second (0.2) or third place
 *   - the dry share 0.15 .. 0.6 when wired (Vascular's processors are 0.62 wet), all of it when not
 *   - not the level and pan: those are the player's (SELECT, ALGORITHM) */
enum { SC_FIELD, SC_MATERIAL, SC_FEEDBACK, SC_EROSION, SC_N };
static const char *const SC_NAME[SC_N] = {"FIELD", "MATERIAL", "FEEDBACK", "EROSION"};
static const uint8_t SC_PATH[SC_N] = {PATH_AMBIENT, PATH_OPEN, PATH_EXP, PATH_EXP};
static const uint8_t SC_FAM[SC_N][MF_N] = {   /* (generation 3's families after the twelve) */
    {0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1},
    {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0},
    {0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0},
    {0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0}};
static const uint8_t MF_HARSH[MF_N] = {0, 0, 2, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 2, 0, 1, 1, 0, 0};
/* the paths (rack.c draw_path): the scenes' weights, and each family's lean -- x 3 favoured, 0 left out, 1 as ever
 * (METAL DRONE BRUIT IMPACT PULSE GLASS FORMANT REED CLOUD SHATTER PULSAR FREEZE FURNACE HUM STATIC HOWL SCRAPE DATA
 * ASH TUNNEL) */
static const uint8_t SC_W_ROUGH[SC_N] = {0, 1, 3, 3}, SC_W_DARK[SC_N] = {5, 1, 0, 0};
static const uint8_t MF_LEAN[DP_N][MF_N] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {3, 1, 3, 1, 0, 1, 1, 1, 1, 3, 0, 1, 3, 1, 3, 1, 3, 3, 1, 1},    /* ABRASIVE: torn, scraped, burnt, broken */
    {1, 3, 0, 0, 0, 1, 1, 1, 3, 0, 0, 3, 0, 3, 0, 3, 0, 0, 3, 3}};   /* DARK: drones, hums, tunnels, ash */
static const uint8_t MF_INH[MF_N] = {6, 2, 4, 4, 2, 4, 1, 1, 4, 4, 2, 4, 5, 4, 4, 3, 5, 4, 4, 4};

/* generator l's cables, drawn into p (HOME's rules, after Vascular's abstract path): the effects it is wired into --
 * none one time in twenty, else one (0.55), two (0.35) or three (0.1), each in an effect slot of its own if it can,
 * at the head of its chain mostly; the dry share (its modulation by another generator left as it is). fresh: HOME's
 * draw (the send drawn too); else a rewiring (the send kept; the dry share kept unless cables appear where there were
 * none, or all go) */
static void gen_wire(uint32_t l, gen_patch_t *p, rnd_t *r, int fresh)
{
    static const uint8_t NE[3] = {55, 35, 10};
    static const uint8_t PL[3] = {70, 20, 10};
    static const uint8_t NE_ROUGH[3] = {25, 45, 30};
    uint32_t used = 0, k, n = rnd_chance(r, 5) ? 0u : 1u + rnd_weighted(r, draw_path == DP_ROUGH ? NE_ROUGH : NE, 3),
             had = p->nent;
    p->nent = 0;
    for (k = 0; k < n; k++) {
        uint32_t s = (uint32_t)rnd_below(r, NFX), tries;
        for (tries = 0; tries < 6u && ((used >> s) & 1u); tries++)
            s = (uint32_t)rnd_below(r, NFX);
        used |= 1u << s;
        p->ent[p->nent++] = (uint8_t)(s * FX_N + rnd_weighted(r, PL, 3));
    }
    if (fresh) {
        p->send = rnd_in(r, LV(0.6), LV(1.0));
        p->dry = p->nent ? rnd_in(r, LV(0.15), LV(0.6)) : Q15;
    } else if (!p->nent)
        p->dry = Q15;                            /* (no cable left: all of it heard dry) */
    else if (!had)
        p->dry = rnd_in(r, LV(0.15), LV(0.6));   /* (cables where there were none: a share for them) */
    p->rewired = 0;                              /* (its modulation by another generator, if any, as it was: never
                                                  * drawn from build 25 -- made and unmade by hand, gen_link) */
}

/* HOME on generator l: a new patch from a seed (family -1: any; else that family, on the scene it had: SHIFT + HOME).
 * Published to the ISR. Returns 0 if the buffer was not free in time */
static uint32_t gen_serial;
static int gen_draw(uint32_t l, int32_t family, uint32_t seed)
{
    uint32_t b = g_use[l] ^ 1u, k, path, t0 = fm1_ms;
    gen_patch_t *p = &gbuf[l][b];
    rnd_t rr, *r = &rr;
    int32_t budget, f;
    uint8_t w[MF_N], scene0 = gbuf[l][g_use[l]].scene;
    for (;;) {                                           /* (no voice may still play the buffer about to be written) */
        int busy = g_pub[l] != g_use[l];
        for (k = 0; k < NVOICE; k++)
            if (vox[k].sounding && vox[k].slot == l && vox[k].dsg == b)
                busy = 1;
        if (!busy)
            break;
        if ((uint32_t)(fm1_ms - t0) > 40u)
            return 0;
        fm1_wdt_feed();
    }
    {   /* (its modulation by another generator, if it has one, kept: gen_wire) */
        const gen_patch_t *now = &gbuf[l][g_use[l]];
        uint8_t xs = gen_drawn[l] ? now->xsrc : XM_NONE, xk = now->xkind, xd = now->xdepth;
        memset(p, 0, sizeof *p);
        p->xsrc = xs;
        p->xkind = xk;
        p->xdepth = xd;
    }
    rnd_seed(r, seed ^ 0x534C4F54u);
    p->scene = family >= 0 && gen_drawn[l] ? scene0
             : draw_path == DP_ANY ? (uint8_t)rnd_below(r, SC_N)
                                   : (uint8_t)rnd_weighted(r, draw_path == DP_ROUGH ? SC_W_ROUGH : SC_W_DARK, SC_N);
    p->seed = seed;
    p->serial = ++gen_serial;
    budget = rnd_chance(r, 40) ? 0 : rnd_chance(r, 67) ? 1 : 2;
    if (draw_path != DP_ANY)
        budget = draw_path == DP_ROUGH ? 2 : 0;          /* (abrasive: the harshest allowed; dark: none harsh) */
    path = SC_PATH[p->scene];
    if (family >= 0 && family < MF_N)
        f = family;
    else {
        for (k = 0; k < MF_N; k++)
            w[k] = (uint8_t)((int32_t)MF_HARSH[k] <= budget ? mo_fam_w(path, GEN_NOW)[k] * (SC_FAM[p->scene][k] ? 3u : 1u) * MF_INH[k] / 2u : 0u);
        for (k = 0; draw_path != DP_ANY && k < MF_N; k++)
            w[k] = (uint8_t)clampi((int32_t)w[k] * MF_LEAN[draw_path][k], 0, 255);
        f = (int32_t)rnd_weighted(r, w, MF_N);
        if (!w[f])
            f = draw_path == DP_ROUGH ? MF_BRUIT : draw_path == DP_DARK ? MF_DRONE : MF_GLASS;
    }
    mo_design(&p->gen, f, rnd_next(r), path, GEN_NOW);
    mo_measure(&p->gen);
    if (draw_path == DP_DARK)
        p->gen.gain = p->gen.gain * 23170 / 32768;      /* (dark: 3 dB quieter) */
    gen_wire(l, p, r, 1);
    RING_PUBLISH();
    g_pub[l] = (uint8_t)b;
    gen_drawn[l] = 1;
    return 1;
}

/* SHIFT held, generator o's button held, then generator d's (build 25): o modulates d -- RING, AM or FILTER, as deep
 * as drawn (35 .. 100) -- or, if it did already, no more. Published as a rewiring is (no fade). Returns the kind made,
 * XM_N for undone, -1 if busy */
static int gen_link(uint32_t o, uint32_t d, uint32_t seed)
{
    static const uint8_t XW[XM_N] = {22, 26, 26};
    uint32_t b = g_use[d] ^ 1u, k, t0 = fm1_ms;
    gen_patch_t *p = &gbuf[d][b];
    rnd_t rr;
    if (o >= NGEN || d >= NGEN || o == d)
        return -1;
    for (;;) {
        int busy = g_pub[d] != g_use[d];
        for (k = 0; k < NVOICE; k++)
            if (vox[k].sounding && vox[k].slot == d && vox[k].dsg == b)
                busy = 1;
        if (!busy)
            break;
        if ((uint32_t)(fm1_ms - t0) > 40u)
            return -1;
        fm1_wdt_feed();
    }
    *p = gbuf[d][g_use[d]];
    rnd_seed(&rr, seed ^ 0x4C494E4Bu);
    if (p->xsrc == o)
        p->xsrc = XM_NONE;
    else {
        p->xsrc = (uint8_t)o;
        p->xkind = (uint8_t)rnd_weighted(&rr, XW, XM_N);
        p->xdepth = (uint8_t)rnd_in(&rr, 35, 95);
    }
    p->rewired = 1;
    p->serial = ++gen_serial;
    RING_PUBLISH();
    g_pub[d] = (uint8_t)b;
    return p->xsrc == XM_NONE ? XM_N : p->xkind;
}

/* SHIFT + the first black key on generator l: a new sound of the family it has (the path's way), its cables, send, dry
 * share and modulation by another generator as they are. Published as a draw is (its notes start again on the new
 * sound). 0 if busy */
static int gen_resound(uint32_t l, uint32_t seed)
{
    uint32_t b = g_use[l] ^ 1u, k, t0 = fm1_ms;
    gen_patch_t *p = &gbuf[l][b];
    const gen_patch_t *now = &gbuf[l][g_use[l]];
    rnd_t rr;
    for (;;) {
        int busy = g_pub[l] != g_use[l];
        for (k = 0; k < NVOICE; k++)
            if (vox[k].sounding && vox[k].slot == l && vox[k].dsg == b)
                busy = 1;
        if (!busy)
            break;
        if ((uint32_t)(fm1_ms - t0) > 40u)
            return 0;
        fm1_wdt_feed();
    }
    rnd_seed(&rr, seed ^ 0x52534E44u);
    *p = *now;
    mo_design(&p->gen, now->gen.family < MF_N ? now->gen.family : MF_DRONE, rnd_next(&rr), now->gen.path, GEN_NOW);
    mo_measure(&p->gen);
    if (draw_path == DP_DARK)
        p->gen.gain = p->gen.gain * 23170 / 32768;
    p->seed = seed;
    p->rewired = 0;
    p->xfade = 0;
    p->serial = ++gen_serial;
    RING_PUBLISH();
    g_pub[l] = (uint8_t)b;
    gen_drawn[l] = 1;
    return 1;
}

/* the first black key on generator l: its cables drawn anew -- the effects it is wired into, and whether another
 * generator modulates it and how -- its sound, its send and dry share, levels and sends as they are (the dry share
 * only where cables appear or all go). Published as a draw is, taken without a fade (gens_take). 0 if busy */
static int gen_rewire(uint32_t l, uint32_t seed)
{
    uint32_t b = g_use[l] ^ 1u, k, tries, t0 = fm1_ms;
    gen_patch_t *p = &gbuf[l][b];
    const gen_patch_t *now;
    rnd_t rr;
    for (;;) {
        int busy = g_pub[l] != g_use[l];
        for (k = 0; k < NVOICE; k++)
            if (vox[k].sounding && vox[k].slot == l && vox[k].dsg == b)
                busy = 1;
        if (!busy)
            break;
        if ((uint32_t)(fm1_ms - t0) > 40u)
            return 0;
        fm1_wdt_feed();
    }
    now = &gbuf[l][g_use[l]];
    rnd_seed(&rr, seed ^ 0x52455749u);
    for (tries = 0; tries < 8u; tries++) {       /* (anew: not the cables it had) */
        *p = *now;
        gen_wire(l, p, &rr, 0);
        if (p->nent != now->nent || memcmp(p->ent, now->ent, p->nent) || p->xsrc != now->xsrc ||
            (p->xsrc != XM_NONE && p->xkind != now->xkind))
            break;
    }
    p->rewired = 1;
    p->serial = ++gen_serial;
    RING_PUBLISH();
    g_pub[l] = (uint8_t)b;
    return 1;
}

/* the pans at power-on: generator 1 in the middle, 2 left, 3 right (SELECT and ALGORITHM move them; HOME does not) */
static const int8_t GEN_PAN0[NGEN] = {0, -14, 14};
/* a generator's patch as a session or a snapshot kept it: its sound drawn again from (family, seed, path, generation)
 * with the level it was measured at (no measuring), its cables and modulation as they were; published as a draw is.
 * The same patch already playing: left as it is (no fade) */
typedef struct {
    uint32_t pseed, dseed;       /* the patch's seed (HOME's); its sound's */
    int32_t gain, send, dry;
    uint8_t family, path, gen, scene;
    uint8_t ent[NENT], nent, xsrc, xkind, xdepth, drawn;
} gen_keep_t;
static void gen_keep(uint32_t l, gen_keep_t *k)
{
    const gen_patch_t *p = &gbuf[l][g_pub[l]];
    memset(k, 0, sizeof *k);
    k->pseed = p->seed;
    k->dseed = p->gen.seed;
    k->gain = p->gen.gain;
    k->send = p->send;
    k->dry = p->dry;
    k->family = p->gen.family;
    k->path = p->gen.path;
    k->gen = p->gen.gen;
    k->scene = p->scene;
    k->nent = p->nent;
    memcpy(k->ent, p->ent, p->nent <= NENT ? p->nent : NENT);   /* (the places past nent: 0, whatever the patch held) */
    k->xsrc = p->xsrc;
    k->xkind = p->xkind;
    k->xdepth = p->xdepth;
    k->drawn = gen_drawn[l];
}
/* same sound (what draws it, its level): only cables to change */
static int gen_same_sound(const gen_keep_t *a, const gen_keep_t *b)
{
    return a->dseed == b->dseed && a->gain == b->gain && a->family == b->family && a->path == b->path && a->gen == b->gen;
}
/* xfade: blocks a changed sound's held notes cross over in, on a voice each (a morph); 0: the quick change, as ever */
static int gen_set_x(uint32_t l, const gen_keep_t *k, uint32_t xfade)
{
    uint32_t b = g_use[l] ^ 1u, i, t0 = fm1_ms;
    gen_patch_t *p = &gbuf[l][b];
    gen_keep_t now, want = *k;
    int same;
    for (i = want.nent; i < NENT; i++)                   /* (a snapshot of build 14 may hold old places past nent) */
        want.ent[i] = 0;
    gen_keep(l, &now);
    if (!memcmp(&now, &want, sizeof now))
        return 1;
    same = gen_same_sound(&now, k);
    for (;;) {                                           /* (no voice may still play the buffer about to be written) */
        int busy = g_pub[l] != g_use[l];
        for (i = 0; i < NVOICE; i++)
            if (vox[i].sounding && vox[i].slot == l && vox[i].dsg == b)
                busy = 1;
        if (!busy)
            break;
        if ((uint32_t)(fm1_ms - t0) > 40u)
            return 0;
        fm1_wdt_feed();
    }
    memset(p, 0, sizeof *p);
    mo_design(&p->gen, k->family < MF_N ? k->family : MF_DRONE, k->dseed, k->path, k->gen < GEN_N ? k->gen : GEN_NOW);
    p->gen.gain = clampi(k->gain, 300, 4096 * 6);
    p->seed = k->pseed;
    p->serial = ++gen_serial;
    p->scene = (uint8_t)(k->scene < SC_N ? k->scene : 0);
    p->nent = (uint8_t)(k->nent <= NENT ? k->nent : 0);
    for (i = 0; i < p->nent; i++)
        p->ent[i] = (uint8_t)(k->ent[i] < NINST ? k->ent[i] : 0);
    p->send = clampi(k->send, 0, Q15);
    p->dry = clampi(k->dry, 0, Q15);
    p->xsrc = k->xsrc < NGEN && k->xsrc != l ? k->xsrc : XM_NONE;
    p->xkind = (uint8_t)(k->xkind < XM_N ? k->xkind : 0);
    p->xdepth = (uint8_t)clampi(k->xdepth, 0, 100);
    p->rewired = (uint8_t)same;                          /* (the same sound, other cables: no fade at all) */
    p->xfade = (uint16_t)(same ? 0u : xfade > 65535u ? 65535u : xfade);
    RING_PUBLISH();
    g_pub[l] = (uint8_t)b;
    gen_drawn[l] = k->drawn;
    return 1;
}
static int gen_set(uint32_t l, const gen_keep_t *k) { return gen_set_x(l, k, 0); }

static void gens_init(uint32_t seed)
{
    uint32_t l;
    memset(vox, 0, sizeof vox);
    memset(vmem, 0, sizeof vmem);
    memset(gbuf, 0, sizeof gbuf);
    memset(xm_env, 0, sizeof xm_env);
    memset(xm_f, 0, sizeof xm_f);
    memset(gbus, 0, sizeof gbus);
    memset(graw, 0, sizeof graw);
    memset(dis, 0, sizeof dis);
    for (l = 0; l < NGEN; l++) {
        dis[l].g = dis[l].gt = Q15;
        rnd_seed(&dis[l].r, 0x44495300u + l);
        gen_dis[l] = 0;
    }
    memset((void *)gen_meter, 0, sizeof gen_meter);
    memset(vr_buf, 0, sizeof vr_buf);
    memset(vr_core, 0, sizeof vr_core);
    blk_clock = age_clock = 0;
    ebus_fed = ebus_fed_run = 0;
    rack_ticks = 0;
    wt_init();
    rack_init(seed);
    loops_init();
    master_init();
    mod_init();
    for (l = 0; l < NGEN; l++) {
        g_pub[l] = g_use[l] = 0;
        gen_mute[l] = 0;
        gen_drawn[l] = 0;
        gen_lvl[l] = LEVEL_UNITY - 2;
        gen_pan[l] = (int8_t)GEN_PAN0[l];
        gl_g[l] = Q15;
        gl_env[l] = 0;
        gl_gg[l] = gl_pl[l] = gl_pr[l] = 0;
        gbuf[l][0].xsrc = gbuf[l][1].xsrc = XM_NONE;
    }
    for (l = 0; l < NGEN; l++) {
        gen_draw(l, -1, seed + 101u + l * 7907u);
        g_use[l] = g_pub[l];                            /* (taken at once: nothing sounds yet) */
    }
}
