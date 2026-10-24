/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR: the loopers -- Tonitor's (layers.c, the same author), one a generator, ported as it is.
 *
 * Every generator has a looper of up to LOOP_TRK loops, all free and each on its own clock: no tempo, no grid,
 * no two loops in step, not even a generator's own -- each goes round in its own length, so they drift apart.
 * A loop keeps the notes of the keys (and MIDI), not the sound: it plays them again on its generator's sound as
 * it is now, a new patch included.
 *   REC        arms a new loop (the others go on playing); the first note played on the generator starts it;
 *   REC again  ends it: the loop is as long as from that first note to now, and it plays at once, for ever
 *              (with the generator's loops stopped, they all start again with it). A note still held is tied
 *              over the loop's end: a drone stays one note
 *   PLAY       stops the generator's loops, or plays them again, each from its start; held, its loops on the
 *              white keys, each brought in or taken out; SHIFT + PLAY every generator's loops at once
 *   REC held   empties the looper: every loop of the generator
 *   SHIFT+REC  VARY: each note the loops play drawn anew (its level, its length, now and then changed or joined)
 *   the 4th .. 8th black keys: REVERSE, PENDULUM, RANDOM; HALF and DOUBLE speed
 *   SHIFT+OCT  every loop a step of the scale down or up (a generator's button held too: its loops only)
 * A loop's notes play on the generator's voices as the keys' do, and stay the loop's: a key lifted never ends a
 * note the loop is playing, nor the loop a note a key holds. The ISR's (gens.c events_take, mix_block); the main
 * loop reads the state for the screen and the LEDs. */
/* ------------------------------------------------------------ looper --- */
#define LOOP_N 1024u             /* note events a track's loops hold between them (~500 notes) */
#define LOOP_TRK 10u             /* loops a layer */
enum { LR_IDLE, LR_ARMED, LR_REC };
/* an event: t (24 bits, blocks from the loop's start), the note (7 bits), on (bit 31) */
#define LE_T(e) ((e) & 0xFFFFFFu)
#define LE_NOTE(e) (((e) >> 24) & 0x7Fu)
#define LE_ON(e) ((e) >> 31)
#define LE(t, note, on) (((t) & 0xFFFFFFu) | ((uint32_t)((note) & 0x7Fu) << 24) | ((uint32_t)((on) != 0) << 31))
typedef struct {
    uint32_t len;                /* blocks of loop time */
    int32_t pos;                 /* where it is: the block of loop time it plays next */
    uint16_t first, n;           /* its events in the looper's ev[] */
    int16_t ix;                  /* the next event to play: forwards the first after those played, backwards the last
                                  * before them (-1: none) */
    int8_t xb;                   /* the loops' transposition when it was recorded (it plays at lx_steps - xb) */
    uint8_t wrap;                /* it has just gone round (its notes tied over the end are still sounding) */
    uint8_t rgo;                 /* RANDOM: a note has been struck since the last jump */
    int8_t dir;                  /* 1 forwards, -1 backwards (the looper's direction; PENDULUM turns it at the ends) */
    uint32_t snd[4];             /* the notes it sounds now (as recorded, before the transposition) */
} loop_trk_t;
enum { LD_FWD, LD_REV, LD_PEND, LD_RAND };   /* the direction a looper plays its loops in (RANDOM: forwards, jumping
                                              * to a note drawn at random each time the next is due) */
enum { LS_NORMAL, LS_HALF, LS_DOUBLE };   /* .. and its speed */
typedef struct {
    volatile uint8_t rec;        /* LR_*: the screen's and the LEDs' */
    volatile uint8_t play;       /* the loops go round */
    volatile uint8_t ntrk;       /* loops kept */
    volatile uint16_t mute;      /* the loops held out (PLAY held, a white key each): a bit each */
    uint8_t nheld;
    uint8_t held[16];            /* notes on in the recording, not yet off */
    uint32_t start;              /* the loop clock when the recording began */
    uint32_t lclk;               /* the loop clock: half blocks of loop time, 1 a block at half speed, 2, 4 at double */
    uint8_t dmode, rate;         /* LD_*, LS_* (the 4th .. 8th black keys) */
    uint8_t vary;                /* SHIFT + REC: each note it plays at a level and for a length drawn at random */
    uint32_t rnd;                /* RANDOM's draws */
    uint32_t nev;                /* events used, every loop's */
    loop_trk_t trk[LOOP_TRK];
    uint32_t ev[LOOP_N];
} looper_t;
static looper_t loop[NGEN];      /* (in RAM: 4.5 KiB a generator) */
#define VQ_N 32u                 /* VARY: a looper's notes waiting to be struck, and struck waiting to be let go */
typedef struct { uint16_t wait, len; uint8_t note, vel; } vq_t;   /* (len 0: a free place) */
static vq_t vq[NGEN][VQ_N];
static struct { uint8_t note; uint16_t left; } vo[NGEN][VQ_N];
static uint32_t vs_blk[NGEN];          /* VARY's strum: the block a loop note was last struck in, the strum's */
static uint8_t vs_idx[NGEN], vs_on[NGEN];
static uint16_t vs_gap[NGEN];
/* a note tied over the loop's end: held from its first block to its last (held when the recording ended: ended
 * with the last block, REC's; a key let go is never later than the one before it). Going round, it is neither let
 * go at the end nor struck again at the start: a drone stays one note */
static int trk_tied(const looper_t *p, const loop_trk_t *t, uint32_t e)
{
    uint32_t i;
    if (LE_ON(e) ? LE_T(e) != 0u : LE_T(e) != t->len - 1u)
        return 0;
    if (LE_ON(e))
        for (i = t->n; i-- > 0u;) {               /* (its end: among the last events) */
            uint32_t f = p->ev[t->first + i];
            if (LE_T(f) != t->len - 1u)
                return 0;
            if (!LE_ON(f) && LE_NOTE(f) == LE_NOTE(e))
                return 1;
        }
    else
        for (i = 0; i < t->n; i++) {              /* (its start: among the first) */
            uint32_t f = p->ev[t->first + i];
            if (LE_T(f) != 0u)
                return 0;
            if (LE_ON(f) && LE_NOTE(f) == LE_NOTE(e))
                return 1;
        }
    return 0;
}
/* a loop from its start in its looper's direction (at its end, backwards) */
static void trk_start(const looper_t *p, loop_trk_t *t)
{
    t->dir = (int8_t)(p->dmode == LD_REV ? -1 : 1);
    t->pos = t->dir > 0 ? 0 : (int32_t)t->len - 1;
    t->ix = (int16_t)(t->dir > 0 ? 0 : (int32_t)t->n - 1);
    t->wrap = 0;
    t->rgo = 0;
    memset(t->snd, 0, sizeof t->snd);
}
/* the loops' transposition (SHIFT + OCT): every loop moved by steps of the scale, its notes walked up or down the
 * scale's pitch classes (a mask from C); a note off the scale keeps its distance from the scale note below it.
 * The ISR's; the main loop asks for a change with lx_want_* and EV_LOOP_XP */
static int8_t lx_steps;
static int8_t lx_tr[NGEN];                     /* each track's own, on top (SHIFT + its button held + OCT) */
static uint16_t lx_mask = 0xFFFu;
static volatile int8_t lx_want_steps;
static volatile int8_t lx_want_tr[NGEN];
static volatile uint16_t lx_want_mask = 0xFFFu;
static uint32_t lx_map(uint32_t n, int32_t k, uint32_t mask)
{
    int32_t b = (int32_t)n;
    mask &= 0xFFFu;
    if (!k || !mask)
        return n;
    while (b > 0 && !((mask >> (b % 12)) & 1u))   /* the scale note at or below it */
        b--;
    n -= (uint32_t)b;                             /* (how far it is above it) */
    for (; k > 0; k--)
        do
            b++;
        while (!((mask >> (b % 12)) & 1u) && b < 140);
    for (; k < 0; k++)
        do
            b--;
        while (b > 0 && !((mask >> (b % 12)) & 1u));
    return (uint32_t)clampi(b + (int32_t)n, 0, 127);
}

/* every note the loop plays on generator l, let go */
static void loop_notes_off(uint32_t l)
{
    uint32_t k;
    for (k = 0; k < LOOP_TRK; k++)
        memset(loop[l].trk[k].snd, 0, sizeof loop[l].trk[k].snd);
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && vox[k].gate && vox[k].slot == l && vox[k].src == SRC_LOOP)
            note_off(l, vox[k].note, SRC_LOOP);
}
/* ------------------------------------------------------ the loopers --- */
static void loop_put(looper_t *p, uint32_t on, uint32_t note)
{
    uint32_t t = (p->lclk - p->start) >> 1, k;   /* (in loop time: played back at this speed, as played) */
    if (on) {
        if (p->nev >= LOOP_N - 16u || p->nheld >= 16u)
            return;                               /* full: no more notes (room is kept for their ends) */
        p->held[p->nheld++] = (uint8_t)note;
    } else {
        for (k = 0; k < p->nheld && p->held[k] != note; k++)
            ;
        if (k == p->nheld)
            return;                               /* its start was not recorded */
        p->held[k] = p->held[--p->nheld];
    }
    p->ev[p->nev++] = LE(t, note, on);
}
static void loop_rewind(looper_t *p)               /* every loop from its start */
{
    uint32_t k;
    for (k = 0; k < p->ntrk; k++)
        trk_start(p, &p->trk[k]);
}
static void loop_cmd(uint32_t l, uint32_t type)
{
    looper_t *p = &loop[l];
    switch (type) {
    case EV_LOOP_REC:
        if (p->rec == LR_REC) {                   /* the end: a loop as long as this */
            loop_trk_t *t = &p->trk[p->ntrk];
            uint32_t len = (p->lclk - p->start) >> 1, i;
            if (len < 2u)
                len = 2u;
            for (i = t->first; i < p->nev; i++)
                if (LE_T(p->ev[i]) > len - 2u)
                    p->ev[i] = LE(len - 2u, LE_NOTE(p->ev[i]), LE_ON(p->ev[i]));
            while (p->nheld)                      /* notes still down: they end with the loop's last block */
                p->ev[p->nev++] = LE(len - 1u, p->held[--p->nheld], 0);
            t->len = len;
            t->n = (uint16_t)(p->nev - t->first);
            if (p->dmode == LD_REV) {             /* recorded while the loops play backwards: kept turned round, so
                                                   * that played backwards it is as it was played */
                uint32_t a = t->first, b = p->nev - 1u;
                for (i = t->first; i < p->nev; i++)
                    p->ev[i] = LE(len - 1u - LE_T(p->ev[i]), LE_NOTE(p->ev[i]), !LE_ON(p->ev[i]));
                for (; a < b; a++, b--) {
                    uint32_t x = p->ev[a];
                    p->ev[a] = p->ev[b];
                    p->ev[b] = x;
                }
            }
            trk_start(p, t);
            if (!p->play)
                loop_rewind(p);                   /* stopped: all of them again, with the new one */
            p->mute &= (uint16_t)~(1u << p->ntrk);   /* (a new one plays) */
            p->ntrk++;
            p->play = 1;
            p->rec = LR_IDLE;
        } else if (p->rec == LR_ARMED) {          /* disarmed */
            p->rec = LR_IDLE;
        } else if (p->ntrk < LOOP_TRK && p->nev < LOOP_N - 32u) {
            p->rec = LR_ARMED;
        }
        break;
    case EV_LOOP_PLAY:
        if (!p->ntrk)
            break;
        if (p->play) {
            loop_notes_off(l);
            p->play = 0;
        } else {
            loop_rewind(p);
            p->play = 1;
        }
        break;
    case EV_LOOP_CLEAR:
        loop_notes_off(l);
        p->dmode = LD_FWD;                        /* (emptied: forwards, at its speed, as played, again) */
        p->rate = LS_NORMAL;
        p->vary = 0;
        memset(vq[l], 0, sizeof vq[l]);
        memset(vo[l], 0, sizeof vo[l]);
        p->play = 0;
        p->mute = 0;
        p->ntrk = 0;
        p->nev = 0;
        p->nheld = 0;
        p->rec = LR_IDLE;
        break;
    }
}
/* a note from the keys on layer l: kept by its looper when it records (and the first one starts it) */
static void loop_hear(uint32_t l, uint32_t on, uint32_t note)
{
    looper_t *p = &loop[l];
    if (p->rec == LR_ARMED && on) {
        p->rec = LR_REC;
        p->start = p->lclk;
        p->trk[p->ntrk].first = (uint16_t)p->nev;
        p->trk[p->ntrk].xb = (int8_t)(lx_steps + lx_tr[l]);   /* (it plays back as played, whatever the others are
                                                              * moved by) */
        p->nheld = 0;
    }
    if (p->rec == LR_REC)
        loop_put(p, on, note);
}
/* loop k of layer l in or out (PLAY held, its white key). In while they play: from its start, now. Out: the notes
 * it holds now let go (found again from its events up to where it is), the others' left be */
static void loop_trk_set(uint32_t l, uint32_t k, uint32_t on)
{
    looper_t *p = &loop[l];
    loop_trk_t *t = &p->trk[k];
    uint32_t bit = 1u << k;
    if (k >= p->ntrk)
        return;
    if (on && (p->mute & bit)) {
        if (p->play)
            trk_start(p, t);
        p->mute &= (uint16_t)~bit;
    } else if (!on && !(p->mute & bit)) {
        if (p->play) {
            uint32_t n;
            for (n = 0; n < 128u; n++)
                if ((t->snd[n >> 5] >> (n & 31u)) & 1u)
                    note_off(l, lx_map(n, lx_steps + lx_tr[l] - t->xb, lx_mask), SRC_LOOP);
            memset(t->snd, 0, sizeof t->snd);
        }
        p->mute |= (uint16_t)bit;
    }
}
/* VARY (SHIFT + REC): each loop note struck at a random level (-10 dB .. full), for a random length (50 ms ..
 * 1.5 s, the recorded end not waited for) -- and now and then, at odds of its own, changed or joined:
 *   dropped (5 %), late by 5 .. 40 ms (25 %), an octave up or down (8 %), a neighbouring step of the scale, a
 *   fifth instead or as well (8 %), a grace note a step away just before it (4 %), struck 2 .. 4 times quickly
 *   (4 %), a third or a fifth above with it (10 %), echoed 1 .. 4 times 120 .. 400 ms apart, each quieter, some
 *   climbing or falling a step each (15 %), and the notes of a chord spread out 20 .. 60 ms apart (12 %).
 * Each looper's notes to come wait in vq[], those struck in vo[] until their time is up */
static uint32_t lv_rnd(looper_t *p)
{
    if (!p->rnd)
        p->rnd = 0x9E3779B9u ^ (blk_clock * 2654435761u);
    p->rnd ^= p->rnd << 13;
    p->rnd ^= p->rnd >> 17;
    p->rnd ^= p->rnd << 5;
    return p->rnd;
}
static int lv_odds(looper_t *p, uint32_t pct) { return (lv_rnd(p) >> 8) % 1000u < pct * 10u; }
static uint32_t lv_in(looper_t *p, uint32_t lo, uint32_t hi) { return lo + (lv_rnd(p) >> 8) % (hi - lo + 1u); }
static uint32_t lv_blocks(uint32_t ms) { return (ms * 1378u) >> 10; }
/* a note of track l struck now (the same note still sounding let go first), let go after len blocks */
static void vq_strike(uint32_t l, uint32_t note, uint32_t vel, uint32_t len)
{
    uint32_t i, slot = VQ_N;
    for (i = 0; i < VQ_N; i++) {
        if (vo[l][i].left && vo[l][i].note == note) {
            note_off(l, note, SRC_LOOP);
            vo[l][i].left = 0;
        }
        if (!vo[l][i].left && slot == VQ_N)
            slot = i;
    }
    if (slot == VQ_N) {                           /* (all sounding: the soonest let go now) */
        slot = 0;
        for (i = 1; i < VQ_N; i++)
            if (vo[l][i].left < vo[l][slot].left)
                slot = i;
        note_off(l, vo[l][slot].note, SRC_LOOP);
    }
    note_vel = (uint8_t)clampi((int32_t)vel, 1, 126);
    note_on(l, note, SRC_LOOP);
    note_vel = 127;
    vo[l][slot].note = (uint8_t)note;
    vo[l][slot].left = (uint16_t)clampi((int32_t)len, 1, 0xFFFF);
}
/* .. or in wait blocks */
static void vq_add(uint32_t l, uint32_t wait, uint32_t note, uint32_t vel, uint32_t len)
{
    uint32_t i;
    if (!wait) {
        vq_strike(l, note, vel, len);
        return;
    }
    for (i = 0; i < VQ_N; i++)
        if (!vq[l][i].len) {
            vq[l][i].wait = (uint16_t)clampi((int32_t)wait, 1, 0xFFFF);
            vq[l][i].len = (uint16_t)clampi((int32_t)len, 1, 0xFFFF);
            vq[l][i].note = (uint8_t)note;
            vq[l][i].vel = (uint8_t)vel;
            return;
        }
}
/* a loop note under VARY */
static void vary_strike(uint32_t l, looper_t *p, loop_trk_t *t, uint32_t raw)
{
    uint32_t nt = lx_map(raw, lx_steps + lx_tr[l] - t->xb, lx_mask), wait = 0, vel, len, k;
    if (lv_odds(p, 5))
        return;                                   /* dropped, this time round */
    vel = lv_in(p, 40, 126);
    len = lv_blocks((50u * exp2_q16((int32_t)((lv_rnd(p) >> 8) % 322000u))) >> 16);   /* 50 ms .. 1.5 s */
    if (vs_blk[l] == blk_clock) {                 /* (with another struck at the same moment: a chord) */
        if (vs_on[l])
            wait += (uint32_t)++vs_idx[l] * vs_gap[l];   /* strummed */
    } else {
        vs_blk[l] = blk_clock;
        vs_idx[l] = 0;
        vs_on[l] = (uint8_t)lv_odds(p, 12);
        vs_gap[l] = (uint16_t)lv_blocks(lv_in(p, 20, 60));
    }
    if (lv_odds(p, 25))                           /* late */
        wait += lv_blocks(lv_in(p, 5, 40));
    if (lv_odds(p, 8)) {                          /* an octave up or down */
        int32_t o = lv_odds(p, 50) ? 12 : -12;
        nt = (uint32_t)((int32_t)nt + o >= 12 && (int32_t)nt + o <= 120 ? (int32_t)nt + o : (int32_t)nt - o);
    }
    if (lv_odds(p, 8)) {                          /* a note not recorded: a step or two away, or a fifth */
        static const int8_t ST[5] = {-2, -1, 1, 2, 4};
        uint32_t f = lx_map(nt, ST[lv_in(p, 0, 4)], lx_mask);
        if (lv_odds(p, 60))
            nt = f;
        else
            vq_add(l, wait, f, vel * 4u / 5u, len);
    }
    if (lv_odds(p, 4)) {                          /* a grace note just before */
        vq_add(l, wait, lx_map(nt, lv_odds(p, 50) ? 1 : -1, lx_mask), vel * 7u / 10u, lv_blocks(lv_in(p, 40, 80)));
        wait += lv_blocks(lv_in(p, 50, 90));
    }
    if (lv_odds(p, 4)) {                          /* ratchet: struck 2 .. 4 times, quickly */
        uint32_t n = lv_in(p, 2, 4), sp = lv_blocks(lv_in(p, 50, 110));
        for (k = 0; k < n; k++)
            vq_add(l, wait + k * sp, nt, vel, sp * 4u / 5u);
    } else
        vq_add(l, wait, nt, vel, len);
    if (lv_odds(p, 10))                           /* a third or a fifth above, with it */
        vq_add(l, wait, lx_map(nt, lv_odds(p, 50) ? 2 : 4, lx_mask), vel * 4u / 5u, len);
    if (lv_odds(p, 15)) {                         /* echoes, quieter each, some climbing or falling */
        uint32_t n = lv_in(p, 1, 4), d = lv_blocks(lv_in(p, 120, 400)), v = vel;
        int32_t dir = lv_odds(p, 40) ? (lv_odds(p, 50) ? 1 : -1) : 0;
        for (k = 1; k <= n; k++) {
            v = v * 3u / 5u;
            vq_add(l, wait + k * d, lx_map(nt, dir * (int32_t)k, lx_mask), v > 8u ? v : 8u, len < d ? len : d);
        }
    }
}
/* a loop note struck: as recorded, or under VARY */
static void loop_strike(uint32_t l, looper_t *p, loop_trk_t *t, uint32_t raw)
{
    if (p->vary) {
        vary_strike(l, p, t, raw);
        return;
    }
    note_on(l, lx_map(raw, lx_steps + lx_tr[l] - t->xb, lx_mask), SRC_LOOP);
    t->snd[raw >> 5] |= 1u << (raw & 31u);
}
/* a block: VARY's notes due struck, those whose time is up let go (the loops stopped: nothing more struck) */
static void lv_tick(uint32_t l)
{
    uint32_t i;
    for (i = 0; i < VQ_N; i++) {
        vq_t *q = &vq[l][i];
        if (q->len && (!loop[l].play || !--q->wait)) {
            if (loop[l].play)
                vq_strike(l, q->note, q->vel, q->len);
            q->len = 0;
        }
        if (vo[l][i].left && !--vo[l][i].left)
            note_off(l, vo[l][i].note, SRC_LOOP);
    }
}
/* an event of loop t played: as recorded, or (backwards) turned round -- its start an end, its end a start. A note
 * tied over the loop's end is never let go there, nor struck again just after (forwards: its start is the loop's
 * first block, its end the last; backwards the other way round) */
static void trk_play(uint32_t l, loop_trk_t *t, looper_t *p, uint32_t e, int as_on)
{
    uint32_t raw = LE_NOTE(e), nt = lx_map(raw, lx_steps + lx_tr[l] - t->xb, lx_mask);
    int tied = trk_tied(p, t, e);
    if (as_on) {
        if (t->wrap && tied && !p->vary)
            return;                               /* (still sounding from the round before) */
        (void)nt;
        loop_strike(l, p, t, raw);
    } else if (p->vary ? (t->snd[raw >> 5] >> (raw & 31u)) & 1u : !tied) {
        /* (VARY: its notes have their own lengths -- but one struck as recorded before VARY came on, tied over the
         * loop's end or not, is let go here, at its recorded end: else it would sound on for ever) */
        note_off(l, nt, SRC_LOOP);
        t->snd[raw >> 5] &= ~(1u << (raw & 31u));
    }
}
/* RANDOM: the notes sounding let go, and on from a note of the loop drawn at random (with any struck at its time) */
static void trk_jump(uint32_t l, looper_t *p, loop_trk_t *t)
{
    const uint32_t *ev = p->ev + t->first;
    uint32_t n, j, k;
    for (n = 0; n < 128u; n++)
        if ((t->snd[n >> 5] >> (n & 31u)) & 1u)
            note_off(l, lx_map(n, lx_steps + lx_tr[l] - t->xb, lx_mask), SRC_LOOP);
    memset(t->snd, 0, sizeof t->snd);
    t->rgo = 0;
    t->wrap = 0;
    if (!p->rnd)                                  /* (a session loaded in RANDOM: seeded at its first draw) */
        p->rnd = 0x9E3779B9u ^ (blk_clock * 2654435761u) ^ (uint32_t)l;
    p->rnd ^= p->rnd << 13;
    p->rnd ^= p->rnd >> 17;
    p->rnd ^= p->rnd << 5;
    j = t->n ? (uint32_t)(((uint64_t)p->rnd * t->n) >> 32) : 0;
    for (k = 0; k < t->n && !LE_ON(ev[(j + k) % t->n]); k++)
        ;                                         /* (the next note's start from there) */
    if (k == t->n) {                              /* (no notes: from the start) */
        t->pos = 0;
        t->ix = 0;
        return;
    }
    j = (j + k) % t->n;
    while (j > 0 && LE_T(ev[j - 1]) == LE_T(ev[j]))
        j--;                                      /* (the first event at its time) */
    t->pos = (int32_t)LE_T(ev[j]);
    t->ix = (int16_t)j;
}
/* one block of loop time of loop t, in its direction; at an end, round again -- or, PENDULUM, back the other way;
 * RANDOM: forwards, but when the next note is due (or the loop ends), a jump to a note drawn at random */
static void trk_step(uint32_t l, looper_t *p, loop_trk_t *t)
{
    const uint32_t *ev = p->ev + t->first;
    if (p->dmode == LD_RAND && t->dir > 0) {
        if (t->pos >= (int32_t)t->len || (t->rgo && t->ix < (int32_t)t->n && LE_ON(ev[t->ix]) && (int32_t)LE_T(ev[t->ix]) <= t->pos))
            trk_jump(l, p, t);
        while (t->ix < (int32_t)t->n && (int32_t)LE_T(ev[t->ix]) <= t->pos) {
            uint32_t e = ev[t->ix++];
            if (LE_ON(e)) {
                loop_strike(l, p, t, LE_NOTE(e));
                t->rgo = 1;
            } else if ((t->snd[LE_NOTE(e) >> 5] >> (LE_NOTE(e) & 31u)) & 1u) {   /* (only what it struck as
                                                                                    * recorded -- VARY's, never) */
                note_off(l, lx_map(LE_NOTE(e), lx_steps + lx_tr[l] - t->xb, lx_mask), SRC_LOOP);
                t->snd[LE_NOTE(e) >> 5] &= ~(1u << (LE_NOTE(e) & 31u));
            }
        }
        t->pos++;
        return;
    }
    if (t->dir > 0) {
        while (t->ix < (int32_t)t->n && (int32_t)LE_T(ev[t->ix]) <= t->pos) {
            uint32_t e = ev[t->ix++];
            trk_play(l, t, p, e, (int)LE_ON(e));
        }
        t->wrap = 0;
        if (++t->pos >= (int32_t)t->len) {
            if (p->dmode == LD_PEND) {
                t->dir = -1;
                t->pos = (int32_t)t->len - 1;
                t->ix = (int16_t)(t->n - 1u);
            } else {
                t->pos = 0;
                t->ix = 0;
            }
            t->wrap = 1;
        }
    } else {
        while (t->ix >= 0 && (int32_t)LE_T(ev[t->ix]) >= t->pos) {
            uint32_t e = ev[t->ix--];
            trk_play(l, t, p, e, !LE_ON(e));
        }
        t->wrap = 0;
        if (--t->pos < 0) {
            if (p->dmode == LD_PEND) {
                t->dir = 1;
                t->pos = 0;
                t->ix = 0;
            } else {
                t->pos = (int32_t)t->len - 1;
                t->ix = (int16_t)(t->n - 1u);
            }
            t->wrap = 1;
        }
    }
}
/* loop t turned round where it is: what is next forwards is next backwards, and the other way (the events of the
 * block it is at played again, turned: what had just started ends, what had just ended starts) */
static void trk_turn(loop_trk_t *t, int dir)
{
    if (dir == t->dir)
        return;
    t->dir = (int8_t)dir;
    if (dir < 0) {
        t->pos--;
        t->ix--;
        if (t->pos < 0) {                         /* (at its very start: from its end) */
            t->pos = (int32_t)t->len - 1;
            t->ix = (int16_t)(t->n - 1u);
            t->wrap = 1;
        }
    } else {
        t->pos++;
        t->ix++;
        if (t->pos >= (int32_t)t->len) {
            t->pos = 0;
            t->ix = 0;
            t->wrap = 1;
        }
    }
}
/* the direction or the speed of layer l's loops (the black keys): forwards, backwards, PENDULUM; half, double */
static void loop_mode(uint32_t l, uint32_t what, uint32_t val)
{
    looper_t *p = &loop[l];
    uint32_t k;
    if (what == 2u) {                             /* VARY on or off */
        p->vary = (uint8_t)(val != 0);
        return;
    }
    if (what) {
        p->rate = (uint8_t)(val <= LS_DOUBLE ? val : LS_NORMAL);
        return;
    }
    if (p->dmode == LD_RAND && val != LD_RAND)    /* (out of RANDOM: on from where it is, in range) */
        for (k = 0; k < p->ntrk; k++)
            if (p->trk[k].pos >= (int32_t)p->trk[k].len) {
                p->trk[k].pos = 0;
                p->trk[k].ix = 0;
            }
    p->dmode = (uint8_t)(val <= LD_RAND ? val : LD_FWD);
    if (p->dmode != LD_PEND)
        for (k = 0; k < p->ntrk; k++)
            trk_turn(&p->trk[k], p->dmode == LD_REV ? -1 : 1);
    if (!p->rnd)
        p->rnd = 0x9E3779B9u ^ (blk_clock * 2654435761u) ^ l;
}
/* VARY's time: the loops' pace no longer steady -- a slow undulation (a sine, its rate and depth redrawn now and
 * then), and every ~6 s on average one of: a halt (stopped dead 0.15 .. 1 s, then on at once), a burst (2 .. 3
 * times as fast for 0.2 .. 0.8 s), a grind (slowing to a stop over 0.4 .. 1.5 s, held up to 0.4 s, then back at
 * once), a swoop (rising to 2 .. 2.5 times over 0.8 .. 2 s, then dropping to 0.4 for 0.3 s). Not while REC records
 * (its pace steady, to play in time to) */
enum { VT_RUN, VT_HALT, VT_BURST, VT_GRIND, VT_SWOOP };
static struct {
    uint8_t ev, phase;
    uint32_t left, len;          /* blocks of the event (its phase) left, and its length */
    int32_t top;                 /* the event's speed, Q16 */
    uint32_t lph, lrate;         /* the undulation: phase, step a block */
    int32_t depth;               /* .. its depth, Q16 */
    uint32_t acc;                /* blocks of loop time to come, Q16 */
} vt[NGEN];
static int32_t vt_speed(uint32_t l, looper_t *p)   /* a block on: the pace, Q16 (1.0 = as recorded) */
{
    int32_t m = Q16;
    if (!vt[l].lrate || (lv_rnd(p) >> 8) % 4000u == 0u) {   /* (the undulation redrawn, every ~3 s) */
        vt[l].lrate = lv_in(p, 70, 350) * 3117u;            /* 0.07 .. 0.35 Hz: x 2^32 / 1378 / 1000 */
        vt[l].depth = (int32_t)lv_in(p, 9830, 26214);       /* 0.15 .. 0.4 */
    }
    vt[l].lph += vt[l].lrate;
    m += (int32_t)(((int64_t)sin_ph(vt[l].lph) * vt[l].depth) >> 15);
    if (vt[l].ev == VT_RUN && (lv_rnd(p) >> 8) % 8268u == 0u) {   /* (one a block in 6 x 1378: every ~6 s) */
        uint32_t w = lv_in(p, 0, 99);
        vt[l].ev = (uint8_t)(w < 30u ? VT_HALT : w < 55u ? VT_BURST : w < 80u ? VT_GRIND : VT_SWOOP);
        vt[l].phase = 0;
        if (vt[l].ev == VT_HALT)
            vt[l].left = lv_blocks(lv_in(p, 150, 1000));
        else if (vt[l].ev == VT_BURST) {
            vt[l].left = lv_blocks(lv_in(p, 200, 800));
            vt[l].top = (int32_t)lv_in(p, 131072, 196608);  /* x 2 .. 3 */
        } else if (vt[l].ev == VT_GRIND)
            vt[l].left = lv_blocks(lv_in(p, 400, 1500));
        else {
            vt[l].left = lv_blocks(lv_in(p, 800, 2000));
            vt[l].top = (int32_t)lv_in(p, 131072, 163840);  /* x 2 .. 2.5 */
        }
        vt[l].len = vt[l].left ? vt[l].left : 1u;
    }
    if (vt[l].ev != VT_RUN) {
        uint32_t done = vt[l].len - vt[l].left;             /* (blocks of this phase gone) */
        int32_t f = (int32_t)((done << 16) / vt[l].len);   /* 0 .. 1, Q16 (done < 2^15: 32 bits) */
        switch (vt[l].ev) {
        case VT_HALT:
            m = 0;
            break;
        case VT_BURST:
            m = vt[l].top;
            break;
        case VT_GRIND:                                     /* down to a stop, then held */
            m = vt[l].phase ? 0 : (int32_t)(((int64_t)m * (Q16 - f)) >> 16);
            break;
        default:                                           /* up, then the drop */
            m = vt[l].phase ? 26214 : m + (int32_t)(((int64_t)(vt[l].top - m) * f) >> 16);
            break;
        }
        if (!vt[l].left || !--vt[l].left) {                /* the phase over: the next, or back to running */
            if (vt[l].phase == 0 && (vt[l].ev == VT_GRIND || vt[l].ev == VT_SWOOP)) {
                vt[l].phase = 1;
                vt[l].left = vt[l].ev == VT_GRIND ? lv_blocks(lv_in(p, 0, 400)) : lv_blocks(300);
                vt[l].len = vt[l].left ? vt[l].left : 1u;
                if (!vt[l].left)
                    vt[l].ev = VT_RUN;
            } else
                vt[l].ev = VT_RUN;
        }
    }
    return m < 0 ? 0 : m;
}

/* one block of every loop that plays: as many blocks of loop time as the speed has passed (at half speed every
 * other block, at double two; VARY's time on top) */
static void loops_run(void)
{
    uint32_t l, k;
    for (l = 0; l < NGEN; l++) {
        looper_t *p = &loop[l];
        uint32_t was = p->lclk, steps;
        p->lclk += p->rate == LS_HALF ? 1u : p->rate == LS_DOUBLE ? 4u : 2u;
        steps = (p->lclk >> 1) - (was >> 1);
        lv_tick(l);
        if (!p->play)
            continue;
        if (p->vary && p->rec != LR_REC) {                 /* VARY's time: the steps through its pace */
            uint32_t base = p->rate == LS_HALF ? 32768u : p->rate == LS_DOUBLE ? 131072u : 65536u;
            vt[l].acc += (uint32_t)(((uint64_t)base * (uint32_t)vt_speed(l, p)) >> 16);
            steps = vt[l].acc >> 16;
            vt[l].acc &= 0xFFFFu;
        }
        for (k = 0; k < p->ntrk; k++) {
            loop_trk_t *t = &p->trk[k];
            uint32_t s;
            if ((p->mute >> k) & 1u)
                continue;                         /* (held out: it waits, silent, where it is) */
            for (s = 0; s < steps; s++)
                trk_step(l, p, t);
        }
    }
}

/* the loops' transposition changed (its steps, or the scale it walks): the loop notes sounding move to their new
 * notes at once -- let go at the old, struck at the new */
static void loop_retune(void)
{
    int32_t s = lx_want_steps, tr[NGEN];
    uint32_t m = lx_want_mask, l, k, pass, same = s == lx_steps, still = !s;
    for (l = 0; l < NGEN; l++) {
        tr[l] = lx_want_tr[l];
        same &= tr[l] == lx_tr[l];
        still &= !tr[l] && !lx_tr[l];
    }
    if (same && (m == lx_mask || still))
        return;
    for (pass = 0; pass < 2u; pass++) {           /* every old note off first, then every new one on */
        for (l = 0; l < NGEN; l++) {
            looper_t *p = &loop[l];
            if (!p->play)
                continue;
            for (k = 0; k < p->ntrk; k++) {
                loop_trk_t *t = &p->trk[k];
                uint32_t n;
                if ((p->mute >> k) & 1u)
                    continue;
                for (n = 0; n < 128u; n++)
                    if ((t->snd[n >> 5] >> (n & 31u)) & 1u) {
                        uint32_t was = lx_map(n, lx_steps + lx_tr[l] - t->xb, lx_mask), now = lx_map(n, s + tr[l] - t->xb, m);
                        if (was == now)
                            continue;
                        if (!pass)
                            note_off(l, was, SRC_LOOP);
                        else
                            note_on(l, now, SRC_LOOP);
                    }
            }
        }
    }
    lx_steps = (int8_t)s;
    for (l = 0; l < NGEN; l++)
        lx_tr[l] = (int8_t)tr[l];
    lx_mask = (uint16_t)m;
}

/* the events the main loop sends the loopers (gens.c events_take) */
static void loop_event(const ev_t *e)
{
    uint32_t k;
    switch (e->type) {
    case EV_LOOP_MODE:                        /* (note: 0 the direction, 1 the speed, 2 VARY; val: which) */
        loop_mode(e->slot, e->note, e->val);
        break;
    case EV_LOOP_XP:                          /* SHIFT and OCT: the loops moved along the scale */
        loop_retune();
        break;
    case EV_LOOP_TRK:                         /* (note: the loop, val: in or out) */
        loop_trk_set(e->slot, e->note, e->val);
        break;
    case EV_LOOPS:                            /* SHIFT and PLAY: every loop at once, in this block */
        for (k = 0; k < NGEN; k++) {
            looper_t *p = &loop[k];
            if (e->val && !p->play && p->ntrk) {
                loop_rewind(p);
                p->play = 1;
            } else if (!e->val && p->play) {
                loop_notes_off(k);
                p->play = 0;
            }
        }
        break;
    default:
        if (e->slot < NGEN)
            loop_cmd(e->slot, e->type);
        break;
    }
}
/* a loop goes round, somewhere (the effect slots' gate stays open between its notes) */
static uint32_t loops_playing(void)
{
    uint32_t l;
    for (l = 0; l < NGEN; l++)
        if (loop[l].play && loop[l].ntrk && (loop[l].mute & ((1u << loop[l].ntrk) - 1u)) != ((1u << loop[l].ntrk) - 1u))
            return 1;
    return 0;
}
static void loops_init(void)
{
    memset(loop, 0, sizeof loop);
    memset(vq, 0, sizeof vq);
    memset(vo, 0, sizeof vo);
    memset(vt, 0, sizeof vt);
    memset(vs_blk, 0, sizeof vs_blk);
    lx_steps = 0;
    memset(lx_tr, 0, sizeof lx_tr);
    lx_mask = 0xFFFu;
    lx_want_steps = 0;
    memset((void *)lx_want_tr, 0, sizeof lx_want_tr);
    lx_want_mask = 0xFFFu;
}
