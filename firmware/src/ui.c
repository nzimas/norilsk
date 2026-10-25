/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR: the panel and the screen.
 *
 *   FX SEL ENV                the generator slots 1 to 3. Pressed: the slot chosen (the keys play it). Held 0.8 s:
 *                             muted, or heard again
 *   LFO EDIT GLO              the effect slots 4 to 6. Pressed: the slot chosen. Held 0.8 s: muted, or heard again
 *   HOME                      a new patch for the slot chosen. A generator: its sound, the effects it is wired into,
 *                             its modulation by another generator (its level and pan stay). An effect slot: a recipe -- its
 *                             two or three effects, their values, its cables across to the slots after it.
 *                             SHIFT + HOME: a new one of the same family, or recipe. HOME held 0.8 s: all six anew
 *   the white keys            play the generator chosen last, in the scale chosen: the first key is the root, every
 *                             white key the next note of the scale. Four voices for the three generators. A key held
 *                             and SHIFT pressed: its note latched (the key lit while its generator is chosen); SHIFT
 *                             held and the lit key pressed: let go
 *   SELECT, ALGORITHM         the level and pan of the generator chosen (or chosen last, while an effect slot is):
 *                             as on Tonitor. At power-on generator 1 is in the middle, 2 left, 3 right. (The SCALE
 *                             page aside)
 *   ARP                       the SCALE page (its light: on it): SELECT the scale, ALGORITHM the root
 *   OCT- / OCT+               the octave, -3 .. +3
 *   REC                       the looper of the generator the keys play (Tonitor's, loop.c): pressed, a new loop
 *                             armed (the first note starts it); again, ended (it plays at once); again while they
 *                             play, an overdub. Held 1 s: its loops emptied. SHIFT + REC: VARY on or off
 *   PLAY                      pressed: its loops stopped, or played again from their starts. Held: its loops on the
 *                             white keys (and the screen), each brought in or taken out. SHIFT + PLAY: every
 *                             generator's loops stopped, or all started together
 *   the 1st black key         the chosen slot REWIRED: a generator's cables into the effect slots and its modulation by
 *                             another generator drawn anew; an effect slot's outs and cables across to the slots
 *                             after it. Nothing else changes, and nothing is cut off: its sound and values stay
 *   the 4th .. 8th black keys its loops REVERSE, PENDULUM, RANDOM; HALF, DOUBLE speed (pressed again: as recorded)
 *   SHIFT + OCT- / OCT+       every loop a step of the scale down or up; a generator's button held too: its loops
 *                             only. The white keys and the latched notes move with them
 *   KNOB 3, KNOB 4            the delay send and the reverb send of the generator the keys play, 0 .. 100 (Tonitor's
 *                             KNOB 4 and SHIFT + KNOB 4): to the master delay and reverb, at the end of the chain
 *   SEQ                       the DELAY page, again the REVERB page, again MOD, again back (Tonitor's): KNOB 1 .. 4
 *                             (TIME FEEDBACK DAMP MIX; SIZE DECAY DAMP MIX), each a row, the one turned black on white
 *   SAVE                      the SESSIONS page (Tonitor's): PRESETS through the 64 slots (the number very large: red
 *                             when it holds a session, grey when empty); SAVE on a saved one loads it, on an empty
 *                             one starts a new session there (a clean slate); a slot button goes back. SAVE held
 *                             0.8 s: saved into the session loaded or chosen, at once; on the page, into the slot
 *                             shown (asked: SAVE NEW, or OVERWRITE? in red on NO until PRESETS turns it to YES)
 *   SHIFT + SAVE              the SNAPSHOTS page (Bigorna's patterns): the session's 16 states, 1 .. 16 in four rows.
 *                             A white key let go soon: that snapshot recalled; held 0.8 s: the state now stored in it;
 *                             SHIFT held too: emptied. PRESETS moves the frame; SAVE recalls the one framed, SAVE held
 *                             stores into it. SHIFT + SAVE again, or a slot button: back. Kept in the flash with the
 *                             session (SAVE held)
 *   KNOB 1                    (playing, and on the SNAPSHOTS page) the morph time: a snapshot recalled flows into place
 *                             over 0 .. 30 s (sessions.c); its square blinks, and its key, until it is there
 *   SEQ (third press)         the MOD page of the slot chosen (Tonitor's): PRESETS through LFO 1, LFO 2, MOD A .. D,
 *                             KNOB 1 .. 4 their values. SHIFT + SEQ: the slot's modulation thrown anew
 *   USB MIDI notes            as the keys, on the generator chosen last
 *   MASTER (knob)             the level
 *
 * The screen is for low vision: a few words, large, high contrast. At the top, the audio's load at the left and the
 * battery at the right (or a message, black on white, across). Under it the three generators, a block each in its
 * own colour: filled when heard, an outline when muted, brighter as it sounds, framed in white when chosen (in grey
 * when an effect slot is chosen and the keys still play it); a small square in another generator's colour in its
 * corner when that one modulates it. Under them the three effect slots, a row each: its effects as tiles, named;
 * round each tile a border in the colours of the generators wired into it, shared out equally; the tiles the
 * generator the keys play is wired into filled with its colour; a tile brightens as sound passes through it; a
 * muted slot's tiles dark; the chosen slot's row framed in white. No cables are drawn. */

/* the panel: which matrix button carries which printed label (as SLOOP's panel.c, default table) */
enum { B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC, B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };
static const uint8_t PANEL_BTN[NB] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1};
static const uint8_t PANEL_ENC[NE] = {0, 1, 6, 2, 3, 4, 5};
static const uint8_t SLOT_BTN[NSLOT] = {B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO};
#define NKEY 27u
#define KEY_SHIFT 25u            /* the rightmost black key */
#define KEY_NONE 0xFFu
#define HOLD_MS 800u             /* a long press */
#define KEY_REWIRE 1u            /* the 1st black key: the chosen slot rewired (its cables anew, its sound as it is) */
#define KEY_REV 8u               /* the 4th black key: the loops backwards (again: forwards) */
#define KEY_PEND 10u             /* the 5th: PENDULUM, forwards and backwards in turn */
#define KEY_RAND 13u             /* the 6th: RANDOM, forwards from a note drawn at random each time the next is due */
#define KEY_HALF 15u             /* the 7th: half speed (again: normal) */
#define KEY_DOUBLE 17u           /* the 8th: double speed */

/* the scales: the white keys play their degrees, from the root, up (Tonitor's) */
#define NSCALE 23
#define SCALE_MAXN 8
static const char *const SCALE_L1[NSCALE] = {"MAJOR", "MINOR", "MINOR", "MINOR", "DORIAN", "MINOR", "WHOLE", "ENIGMATIC", "JAPANESE",
    "PHRYGIAN", "LYDIAN", "MIXO-", "LOCRIAN", "MAJOR", "BLUES", "HUNGAR-", "DOUBLE", "PHRYGIAN", "JAPANESE",
    "JAPANESE", "PELOG", "OCTA-", "PROME-"};
static const char *const SCALE_L2[NSCALE] = {"", "NATURAL", "HARMONIC", "MELODIC", "", "PENTA", "TONE", "", "IN",
    "", "", "LYDIAN", "", "PENTA", "", "IAN MINOR", "HARMONIC", "DOMINANT", "HIRAJOSHI", "IWATO", "", "TONIC", "THEUS"};
static const uint8_t SCALE_N[NSCALE] = {7, 7, 7, 7, 7, 5, 6, 7, 5, 7, 7, 7, 7, 5, 6, 7, 7, 7, 5, 5, 5, 8, 6};
static const uint8_t SCALE_STEP[NSCALE][SCALE_MAXN] = {
    {0, 2, 4, 5, 7, 9, 11},      /* major */
    {0, 2, 3, 5, 7, 8, 10},      /* natural minor */
    {0, 2, 3, 5, 7, 8, 11},      /* harmonic minor */
    {0, 2, 3, 5, 7, 9, 11},      /* melodic minor (rising) */
    {0, 2, 3, 5, 7, 9, 10},      /* dorian */
    {0, 3, 5, 7, 10},            /* minor pentatonic */
    {0, 2, 4, 6, 8, 10},         /* whole tone */
    {0, 1, 4, 6, 8, 10, 11},     /* enigmatic */
    {0, 1, 5, 7, 8},             /* in (miyako-bushi) */
    {0, 1, 3, 5, 7, 8, 10},      /* phrygian */
    {0, 2, 4, 6, 7, 9, 11},      /* lydian */
    {0, 2, 4, 5, 7, 9, 10},      /* mixolydian */
    {0, 1, 3, 5, 6, 8, 10},      /* locrian */
    {0, 2, 4, 7, 9},             /* major pentatonic */
    {0, 3, 5, 6, 7, 10},         /* blues */
    {0, 2, 3, 6, 7, 8, 11},      /* hungarian minor */
    {0, 1, 4, 5, 7, 8, 11},      /* double harmonic */
    {0, 1, 4, 5, 7, 8, 10},      /* phrygian dominant */
    {0, 2, 3, 7, 8},             /* hirajoshi */
    {0, 1, 5, 6, 10},            /* iwato */
    {0, 1, 3, 7, 8},             /* pelog (selisir) */
    {0, 1, 3, 4, 6, 7, 9, 10},   /* octatonic, half step first */
    {0, 2, 4, 6, 9, 10},         /* prometheus */
};
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
#define SCALE_BASE 48            /* the root's octave: C3 .. B3 on the first key (the octave moves it) */

/* the generators' colours (not Tonitor's: a set of their own) */
static const uint16_t GEN_COL[NGEN] = {RGB(255, 72, 72), RGB(72, 150, 255), RGB(170, 255, 60)};
#define C_SCALE RGB(255, 196, 90)
#define C_OK RGB(60, 220, 90)
static const char *const RM_TILE[NKIND] = {"FLT", "FLD", "MU", "SHF", "MDL", "STR", "VOX", "DLY", "TAP", "RVS", "SPR", "REV",
                                           "GLT", "CLD", "SCR", "RNG", "STV", "CHB", "BNK"};

/* the screen's layout */
#define TOP_H 40
#define GEN_Y 42
#define GEN_H 48
#define GEN_P 80                 /* a generator's block's pitch across */
#define FXR_Y 92                 /* the first effect slot's row */
#define FXR_H 49                 /* a row's height */

static struct {
    uint8_t sel;                 /* the slot chosen: 0 .. 2 a generator, 3 .. 5 an effect slot */
    uint8_t gen;                 /* the generator the keys play: the last chosen */
    int8_t oct;
    uint8_t key_note[NKEY], key_slot[NKEY];   /* what a key down holds (KEY_NONE: nothing) */
    uint32_t keys;               /* the keys down last time */
    char msg[16];
    uint32_t msg_until;
    uint32_t home_until;         /* HOME's light after a new patch */
    uint32_t seed;
    uint8_t sc_page;             /* the SCALE page is up */
    uint8_t scale, root;         /* the keyboard's scale and root (0 = C .. 11 = B) */
    int8_t sc_touched;           /* 0 the root just turned, 1 the scale, -1 none (until sc_touch_until) */
    uint32_t sc_touch_until;
    uint8_t btn_down[NSLOT], btn_done[NSLOT];   /* a slot's button down since btn_t0; held long enough: done */
    uint32_t btn_t0[NSLOT];
    uint8_t home_down, home_done, home_shift;   /* HOME down since home_t0 (with SHIFT); held: done */
    uint32_t home_t0;
    uint8_t force;               /* the screen drawn whole */
    /* what the screen shows (a part is drawn again when what it shows changed) */
    uint8_t shown_page;
    int32_t shown_top;
    int32_t shown_gen[NGEN], shown_row[NFX], shown_tile[NINST];
    int8_t shown_sc_touch;
    uint8_t shown_scale, shown_root;
    uint32_t load_at;
    uint8_t load_shown;
    uint8_t midi_note[128], midi_slot[128];
    uint32_t latch[NGEN];        /* each generator's keys latched */
    uint8_t latch_note[NGEN][NKEY];
    uint8_t was_on;              /* (CPU1 running, last time: its loss said once) */
    /* the loopers (Tonitor's gestures) */
    uint8_t rec_down, rec_done, play_down, ovd;   /* REC, PLAY down; held: done; PLAY held: the loops on the keys */
    uint32_t rec_t0, play_t0;
    int8_t lx, lxt[NGEN];        /* the loops moved along the scale: all of them, and each generator's own */
    int32_t shown_ovd;
    /* the master delay and reverb (master.c) */
    int8_t rsend[NGEN], dsend[NGEN];   /* each generator's sends, 0 .. 100 (KNOB 4, KNOB 3) */
    int8_t rvp[4], dlp[4];       /* the reverb's SIZE DECAY DAMP MIX, the delay's TIME FEEDBACK DAMP MIX, 0 .. 100 */
    uint8_t fx_page;             /* 0 none, 1 the DELAY page, 2 the REVERB page (SEQ) */
    int8_t fx_touched;           /* the row just turned (-1 none), until fx_touch_until */
    uint32_t fx_touch_until;
    int8_t shown_fxp[4], shown_fx_touch;
    /* sessions and snapshots (sessions.c), the MOD page (mod.c), the whole-screen message */
    uint8_t ses_page, ses_ix, ses_yes, ses_over;   /* ses_page: 0, SP_LIST, SP_ASK */
    uint16_t ses_new;            /* the slot a save asked about goes to */
    uint8_t save_down, save_long;
    uint32_t save_t0;
    uint8_t snap_page, snap_ix;  /* the SNAPSHOTS page; its frame */
    int8_t snap_key;             /* the white key down on it (-1 none), since snap_t0; held: done */
    uint8_t snap_done, snap_sh;
    uint32_t snap_t0;
    uint8_t mod_ix;              /* the MOD page's entry: LFO 1, LFO 2, MOD A .. D */
    int8_t mod_touched;
    uint32_t mod_touch_until, mod_drawn_at;
    char fl1[14], fl2[8];        /* the whole screen's message, until fl_until */
    uint16_t fl_col;
    uint32_t fl_until;
    uint8_t fl_shown;
    int32_t shown_ses, shown_snap;
    uint8_t morph;               /* KNOB 1: a snapshot's morph time, half seconds (0: at once) */
} ui;
enum { SP_LIST = 1, SP_ASK = 2 };
#define C_SES RGB(255, 200, 60)
#define C_FULL RGB(255, 60, 60)
#define C_SNAP RGB(120, 220, 255)
#define MORPH_STEPS_UI 60        /* (sessions.c MORPH_STEPS: 30 s in half seconds) */
static const int8_t RV_DEF[4] = {70, 65, 40, 50};      /* Tonitor's: lush, present, not overdone */
static const char *const RV_NAME[4] = {"SIZE", "DECAY", "DAMP", "MIX"};
static const int8_t DL_DEF[4] = {65, 40, 40, 50};      /* Tonitor's: 400 ms, a few repeats, a little darker each */
static const char *const DL_NAME[4] = {"TIME", "FEEDBACK", "DAMP", "MIX"};
#define RSEND_DEF 40             /* (Tonitor's send) */
#define C_SEND RGB(140, 190, 255)
#define C_DELAY RGB(255, 170, 90)
static void sends_apply(uint32_t g)
{
    gen_rsend[g] = ui.rsend[g] * 32767 / 100;
    gen_dsend[g] = ui.dsend[g] * 32767 / 100;
}
static int32_t batt_raw;                         /* the battery's divider, smoothed (main.c), 0 .. 1023; 0 none yet */

static void say(const char *s, uint32_t ms)
{
    str_cpy(ui.msg, s, sizeof ui.msg);
    ui.msg_until = fm1_ms + ms;
}
static void say_n(const char *s, int32_t n, const char *t, uint32_t ms)   /* "s" n "t" */
{
    char b[16], d[8];
    uint32_t k = 0, j;
    for (j = 0; s[j] && k < 10u; j++)
        b[k++] = s[j];
    fmt_int(d, n);
    for (j = 0; d[j] && k < 13u; j++)
        b[k++] = d[j];
    for (j = 0; t[j] && k < 15u; j++)
        b[k++] = t[j];
    b[k] = 0;
    say(b, ms);
}
/* the whole screen, for 1.5 s: l1 over l2, in colour c (SAVED 3, LOADED 3, NEW 3) */
static void flash(const char *l1, const char *l2, uint16_t c)
{
    str_cpy(ui.fl1, l1, sizeof ui.fl1);
    str_cpy(ui.fl2, l2, sizeof ui.fl2);
    ui.fl_col = c;
    ui.fl_until = fm1_ms + 1500u;
    ui.fl_shown = 0;
}
static int btn_bit(uint32_t mask, uint32_t label) { return (int)((mask >> PANEL_BTN[label]) & 1u); }
static int shift_held(void) { return (int)((fm1_in.notes >> KEY_SHIFT) & 1u); }

/* white key k's place among the white keys (0 .. 15), -1 for a black key (keys from F3, chromatic) */
static int32_t white_ix(uint32_t k)
{
    static const int8_t W[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};   /* C .. B: whites in an octave */
    uint32_t n = 53u + k, o = n / 12u;
    int32_t w = W[n % 12u];
    if (w < 0)
        return -1;
    return (int32_t)o * 7 + w - (4 * 7 + 3);                                  /* F3 = 0 */
}
/* the note white key w plays, in the scale, at octave oct */
static uint32_t scale_note(int32_t w, int32_t oct)
{
    int32_t n = SCALE_N[ui.scale], q = w >= 0 ? w / n : -((n - 1 - w) / n);
    return (uint32_t)clampi(SCALE_BASE + ui.root + 12 * oct + 12 * q + SCALE_STEP[ui.scale][w - q * n], 12, 120);
}

/* the scale's pitch classes, a mask from C */
static uint32_t scale_mask(void)
{
    uint32_t k, m = 0;
    for (k = 0; k < SCALE_N[ui.scale]; k++)
        m |= 1u << ((ui.root + SCALE_STEP[ui.scale][k]) % 12u);
    return m;
}
/* the loops' transposition and the scale it walks, to the ISR (which moves the notes sounding) */
static void lx_post(void)
{
    uint32_t k;
    lx_want_mask = (uint16_t)scale_mask();
    lx_want_steps = ui.lx;
    for (k = 0; k < NGEN; k++)
        lx_want_tr[k] = ui.lxt[k];
    RING_PUBLISH();
    ev_post(EV_LOOP_XP, 0, 0, 0);
}
/* generator g's latched notes moved d steps of the scale, with the loops: each let go and struck at its new note,
 * and its lit key moved as the hand would, d white keys along (unless that is off the keyboard or taken) */
static void latch_move(uint32_t g, int32_t d)
{
    uint32_t k, m = scale_mask(), latch = 0;
    uint8_t note[NKEY];
    if (!d)
        return;
    for (k = 0; k < NKEY; k++)
        if ((ui.latch[g] >> k) & 1u) {
            uint32_t was = ui.latch_note[g][k], now = lx_map(was, d, m), k2;
            ev_post(EV_OFF, g, was, 0);
            ev_post(EV_ON, g, now, 0);
            ev_post(EV_LATCH, g, now, 0);
            for (k2 = 0; k2 < NKEY && white_ix(k2) != white_ix(k) + d; k2++)
                ;
            if (k2 == NKEY || ((latch >> k2) & 1u))
                k2 = k;
            if ((latch >> k2) & 1u)
                for (k2 = 0; k2 < NKEY && (white_ix(k2) < 0 || ((latch >> k2) & 1u)); k2++)
                    ;                            /* (a free white key, should two meet at an edge) */
            if (k2 < NKEY) {
                latch |= 1u << k2;
                note[k2] = (uint8_t)now;
            }
        }
    for (k = 0; k < NKEY; k++)
        if ((latch >> k) & 1u)
            ui.latch_note[g][k] = note[k];
    ui.latch[g] = latch;
}

static uint32_t next_seed(void)
{
    ui.seed = ui.seed * 1664525u + 1013904223u + fm1_ticks();
    return ui.seed;
}
/* HOME on slot l (same: of the same family, or recipe) */
static int new_patch(uint32_t l, int same)
{
    int ok;
    if (l < NGEN)
        ok = gen_draw(l, same && gen_drawn[l] ? (int32_t)gbuf[l][g_use[l]].gen.family : -1, next_seed());
    else
        ok = fx_draw(l - NGEN, same ? (int32_t)fbuf[l - NGEN][f_use[l - NGEN]].recipe : -1, next_seed());
    if (ok)
        ui.home_until = fm1_ms + 400u;
    return ok;
}
/* a slot muted or heard again (a long press on its button) */
static void slot_mute(uint32_t l)
{
    volatile uint8_t *m = l < NGEN ? &gen_mute[l] : &fx_mute[l - NGEN];
    *m ^= 1u;
    say_n("", (int32_t)l + 1, *m ? " MUTED" : " HEARD", 1200);
}

/* every page down: back to playing */
static void pages_off(void)
{
    ui.sc_page = ui.fx_page = ui.ses_page = ui.snap_page = 0;
    ui.force = 1;
}
/* a session's or a snapshot's outcome, on the whole screen */
static void ses_done(int rc, const char *what, uint32_t num)
{
    char n[8];
    fmt_int(n, (int32_t)num);
    if (rc == SES_OK)
        flash(what, num ? n : "", C_OK);
    else
        flash(rc == SES_FULL ? "NO ROOM" : rc == SES_NOFLASH ? "NO FLASH" : rc == SES_BUSY ? "BUSY" : "FAILED", "", C_FULL);
}
/* SAVE held (Tonitor's save_long); on the SNAPSHOTS page, the state now stored into the snapshot framed */
static void save_long(void)
{
    if (ui.snap_page) {
        snap_store(ui.snap_ix);
        ses_done(SES_OK, "STORED", ui.snap_ix + 1u);
        ui.force = 1;
        return;
    }
    if (ui.ses_page == SP_ASK)
        return;
    if (ui.ses_page == SP_LIST && ui.ses_ix != ses_current()) {   /* in the list: into the slot shown, asked */
        ui.ses_new = ui.ses_ix;
        ui.ses_over = (uint8_t)ses_has(ui.ses_ix);
        ui.ses_page = SP_ASK;
        ui.ses_yes = (uint8_t)!ui.ses_over;      /* (over a session: NO until turned to YES) */
        ui.force = 1;
        return;
    }
    if (ses_current()) {                         /* a session loaded or chosen: saved over it, at once */
        ui.ses_page = 0;
        ses_done(ses_save(ses_current()), "SAVED", ses_current());
        ui.force = 1;
        return;
    }
    ui.ses_new = (uint16_t)ses_next_number();   /* none chosen: the first empty slot, asked first */
    if (!ui.ses_new) {
        ses_done(SES_FULL, "", 0);
        return;
    }
    ui.ses_over = 0;
    ui.ses_page = SP_ASK;
    ui.ses_yes = 1;
    ui.force = 1;
}
/* SAVE pressed (let go before it was held): the SESSIONS page, or what it asks; on the SNAPSHOTS page, the snapshot
 * framed recalled */
static void save_short(void)
{
    if (ui.snap_page) {
        if (snap_has(ui.snap_ix))
            ses_done(snap_recall(ui.snap_ix), "RECALLED", ui.snap_ix + 1u);
        else
            say("EMPTY", 900);
        ui.force = 1;
        return;
    }
    if (ui.ses_page == SP_ASK) {
        uint32_t num = ui.ses_new;
        ui.ses_page = 0;
        ui.force = 1;
        if (!ui.ses_yes) {
            say("NOT SAVED", 1500);
            return;
        }
        ses_done(ses_save(num), "SAVED", num);
        return;
    }
    if (ui.ses_page == SP_LIST) {                /* the slot shown: loaded (or, empty, a new session), and back */
        uint32_t num = ui.ses_ix;
        ui.ses_page = 0;
        ui.force = 1;
        if (ses_has(num))
            ses_done(ses_load_slot(num), "LOADED", num);
        else {
            ses_fresh(num);                      /* a clean slate; SAVE held writes it there */
            ses_done(SES_OK, "NEW", num);
        }
        return;
    }
    pages_off();                                 /* the SESSIONS page, on the session chosen if any */
    ui.ses_page = SP_LIST;
    ui.ses_ix = (uint8_t)(ses_current() ? ses_current() : 1u);
}
/* KNOB j on the MOD page: entry ix of slot sl (Tonitor's mod_knob) */
static void mod_knob(uint32_t sl, uint32_t ix, uint32_t j, int32_t s)
{
    mod_t *m = &slot_mod[sl];
    if (ix < 2u) {
        mlfo_t *f = &m->lfo[ix];
        if (j == 0u)
            f->shape = (uint8_t)clampi(f->shape + s, 0, MS_N - 1);
        else if (j == 1u)
            f->rate = (uint8_t)clampi(f->rate + s, 0, 100);
        else if (j == 2u)
            f->mode = (uint8_t)clampi(f->mode + s, 0, MM_N - 1);
        else
            f->fade = (uint8_t)clampi(f->fade + 2 * s, 0, 100);
    } else {
        mslot_t *q = &m->s[ix - 2u];
        if (j == 0u)
            q->src = (uint8_t)clampi(q->src + s, 0, MSRC_N - 1);
        else if (j == 1u)
            q->dst = (uint8_t)clampi(q->dst + s, 0, (int32_t)mod_ndst(sl) - 1);
        else if (j == 2u)
            q->amt = (int8_t)clampi(q->amt + 2 * s, -100, 100);
        else
            return;
    }
    ui.mod_touched = (int8_t)j;
    ui.mod_touch_until = fm1_ms + 1500u;
    ui.force = 1;
}

/* USB MIDI in: notes as the keys, on the generator chosen last (any channel) */
static void midi_take(void)
{
    while (mi_r != mi_w) {
        uint32_t pkt = midi_in_q[mi_r % MQ], cin = pkt & 0xFu, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        RING_PUBLISH();
        mi_r++;
        if (cin == 9u && d2) {
            ev_post(EV_ON, ui.gen, d1, 0);
            ui.midi_note[d1] = 1;
            ui.midi_slot[d1] = ui.gen;
        } else if (cin == 8u || (cin == 9u && !d2)) {
            if (ui.midi_note[d1]) {
                ev_post(EV_OFF, ui.midi_slot[d1], d1, 0);
                ui.midi_note[d1] = 0;
            }
        }
    }
}

static void ui_input(void)
{
    uint32_t rel, press = fm1_input_edges(&rel), keys, ch, k, l;
    int32_t s;
    morph_poll();                                /* (a snapshot's morph, a step on) */
    if (ui.was_on && !audio_worker_online)
        say("1 CORE", 4000);                     /* (CPU1 retired as it plays: said, once) */
    ui.was_on = (uint8_t)audio_worker_online;
    /* the slots' buttons: pressed, chosen; held, muted or heard again */
    for (l = 0; l < NSLOT; l++) {
        if (btn_bit(press, SLOT_BTN[l])) {
            ui.sel = (uint8_t)l;
            if (l < NGEN)
                ui.gen = (uint8_t)l;
            ui.btn_down[l] = 1;
            ui.btn_done[l] = (uint8_t)shift_held();   /* (with SHIFT: no mute -- SHIFT + its button + OCT moves its loops) */
            ui.btn_t0[l] = fm1_ms;
            if (ui.sc_page || ui.fx_page || ui.ses_page || ui.snap_page)
                pages_off();
        }
        if (ui.btn_down[l] && !btn_bit(fm1_in.buttons, SLOT_BTN[l]))
            ui.btn_down[l] = 0;
        if (ui.btn_down[l] && !ui.btn_done[l] && (uint32_t)(fm1_ms - ui.btn_t0[l]) >= HOLD_MS) {
            ui.btn_done[l] = 1;
            slot_mute(l);
        }
    }
    /* HOME: let go soon, a new patch for the slot chosen (SHIFT: of the same family / recipe); held, all six anew */
    if (btn_bit(press, B_HOME)) {
        ui.home_down = 1;
        ui.home_done = 0;
        ui.home_t0 = fm1_ms;
        ui.home_shift = (uint8_t)shift_held();
    }
    if (ui.home_down && !ui.home_done && (uint32_t)(fm1_ms - ui.home_t0) >= HOLD_MS) {
        int ok = 1;
        ui.home_done = 1;
        for (l = 0; l < NSLOT; l++)
            ok &= new_patch(l, 0);
        say(ok ? "ALL NEW" : "BUSY", 1000);
    }
    if (ui.home_down && !btn_bit(fm1_in.buttons, B_HOME)) {
        ui.home_down = 0;
        if (!ui.home_done && !new_patch(ui.sel, ui.home_shift))
            say("BUSY", 600);
    }
    if (btn_bit(press, B_ARP)) {
        uint8_t was = ui.sc_page;
        pages_off();
        ui.sc_page = (uint8_t)!was;
    }
    if (btn_bit(press, B_SEQ) && shift_held()) { /* SHIFT and SEQ: the chosen slot's modulation thrown anew */
        mod_random(ui.sel, next_seed());
        say("MOD RANDOM", 900);
        ui.mod_touched = -1;
        ui.force = 1;
    } else if (btn_bit(press, B_SEQ)) {          /* SEQ, again and again: DELAY, REVERB, MOD, back to playing */
        uint8_t f = (uint8_t)(ui.fx_page >= 3u ? 0u : ui.fx_page + 1u);
        pages_off();
        ui.fx_page = f;
        ui.fx_touched = -1;
        ui.mod_touched = -1;
    }
    if (btn_bit(press, B_SAVE) && shift_held()) {   /* SHIFT and SAVE: the SNAPSHOTS page, or back */
        uint8_t was = ui.snap_page;
        pages_off();
        ui.snap_page = (uint8_t)!was;
        ui.snap_key = -1;
        if (ui.snap_page && snap_current() >= 0)
            ui.snap_ix = (uint8_t)snap_current();
    } else if (btn_bit(press, B_SAVE)) {
        ui.save_t0 = fm1_ms;
        ui.save_down = 1;
        ui.save_long = 0;
    }
    if (ui.save_down) {
        if (!btn_bit(fm1_in.buttons, B_SAVE)) {
            ui.save_down = 0;
            if (!ui.save_long)
                save_short();
        } else if (!ui.save_long && (uint32_t)(fm1_ms - ui.save_t0) >= HOLD_MS) {
            ui.save_long = 1;
            save_long();
        }
    }
    s = fm1_enc_take(PANEL_ENC[EN_PRESET]);      /* PRESETS: through the page's list */
    if (s && ui.ses_page == SP_LIST) {
        ui.ses_ix = (uint8_t)clampi((int32_t)ui.ses_ix + s, 1, SES_SLOTS);
    } else if (s && ui.ses_page == SP_ASK) {
        ui.ses_yes = (uint8_t)(s > 0);
    } else if (s && ui.snap_page) {
        ui.snap_ix = (uint8_t)clampi((int32_t)ui.snap_ix + s, 0, NSNAP - 1);
    } else if (s && ui.fx_page == 3u) {
        ui.mod_ix = (uint8_t)clampi((int32_t)ui.mod_ix + s, 0, 2 + (int32_t)NMSLOT - 1);
        ui.mod_touched = -1;
        ui.force = 1;
    }
    if ((btn_bit(press, B_OCTDN) || btn_bit(press, B_OCTUP)) && shift_held()) {
        int32_t lim = 2 * SCALE_N[ui.scale], t = -1, x;   /* SHIFT and OCT: every loop a step of the scale down or */
        for (l = 0; l < NGEN; l++)                         /* up; a generator's button held too: its loops only */
            if (btn_bit(fm1_in.buttons, SLOT_BTN[l]))
                t = (int32_t)l;
        if (t >= 0) {
            x = clampi(ui.lxt[t] + (btn_bit(press, B_OCTUP) ? 1 : -1), -lim, lim);
            latch_move((uint32_t)t, x - ui.lxt[t]);
            ui.lxt[t] = (int8_t)x;
            ui.btn_done[t] = 1;                  /* (its button held for this: not muted) */
            {   /* GEN 2 +3 */
                char g[16] = "GEN 1 ";
                g[4] = (char)('1' + t);
                if (x > 0)
                    g[6] = '+';
                fmt_int(g + str_len(g), x);
                say(g, 1200);
            }
        } else {
            x = clampi(ui.lx + (btn_bit(press, B_OCTUP) ? 1 : -1), -lim, lim);
            for (l = 0; l < NGEN; l++)
                latch_move(l, x - ui.lx);
            ui.lx = (int8_t)x;
            say_n(x > 0 ? "MOVE +" : "MOVE ", x, "", 1200);
        }
        lx_post();
    } else {
        if (btn_bit(press, B_OCTDN) && ui.oct > -3)
            ui.oct--;
        if (btn_bit(press, B_OCTUP) && ui.oct < 3)
            ui.oct++;
    }
    /* the looper of the generator the keys play (Tonitor's) */
    if (btn_bit(press, B_REC) && shift_held()) {  /* SHIFT and REC: its loops VARY (or as played) */
        uint32_t v = !loop[ui.gen].vary;
        ev_post(EV_LOOP_MODE, ui.gen, 2, v);
        say(v ? "VARY" : "VARY OFF", 1200);
    } else if (btn_bit(press, B_REC)) {          /* at once: the loop ends exactly here */
        const looper_t *p = &loop[ui.gen];
        ev_post(EV_LOOP_REC, ui.gen, 0, 0);
        ui.rec_t0 = fm1_ms;
        ui.rec_down = 1;
        ui.rec_done = 0;
        if (p->rec == LR_REC)
            say_n("LOOP ", (int32_t)p->ntrk + 1, "", 1200);
        else if (p->rec == LR_ARMED)
            say("REC OFF", 900);
        else if (p->ntrk >= LOOP_TRK)
            say("10 LOOPS", 1500);           /* full: not armed */
        else
            say("ARMED", 1200);
    }
    if (ui.rec_down && !btn_bit(fm1_in.buttons, B_REC))
        ui.rec_down = 0;
    if (ui.rec_down && !ui.rec_done && (uint32_t)(fm1_ms - ui.rec_t0) >= 1000u) {   /* held: emptied */
        ev_post(EV_LOOP_CLEAR, ui.gen, 0, 0);
        ui.lxt[ui.gen] = 0;                      /* (emptied: its own move gone too) */
        lx_post();
        ui.rec_done = 1;
        say_n("LOOPS ", (int32_t)ui.gen + 1, " EMPTY", 1500);
    }
    if (btn_bit(press, B_PLAY)) {
        if (shift_held()) {                      /* SHIFT and PLAY: every loop stops if one plays, else all start */
            uint32_t playing = 0, kept = 0;
            for (l = 0; l < NGEN; l++) {
                playing |= loop[l].play && loop[l].ntrk;
                kept |= loop[l].ntrk != 0u;
            }
            if (playing || kept)
                ev_post(EV_LOOPS, 0, 0, (uint32_t)!playing);
            say(playing ? "ALL STOPPED" : kept ? "ALL PLAYING" : "NO LOOPS", 1200);
        } else {                                 /* PLAY: let go soon, the loops; held, the overdubs */
            ui.play_down = 1;
            ui.play_t0 = fm1_ms;
        }
    }
    if (ui.play_down) {
        if (!btn_bit(fm1_in.buttons, B_PLAY)) {
            ui.play_down = 0;
            if (ui.ovd) {
                ui.ovd = 0;
                ui.force = 1;
            } else {
                const looper_t *p = &loop[ui.gen];
                ev_post(EV_LOOP_PLAY, ui.gen, 0, 0);
                say(!p->ntrk ? "NO LOOPS" : p->play ? "STOP" : "PLAY", 900);
            }
        } else if (!ui.ovd && (uint32_t)(fm1_ms - ui.play_t0) >= 450u) {
            ui.ovd = 1;                          /* held: the loops on the white keys */
            ui.force = 1;
        }
    }
    if (ui.sc_page) {                            /* the SCALE page: SELECT the scale, ALGORITHM the root */
        s = fm1_enc_take(PANEL_ENC[EN_SELECT]);
        if (s) {
            ui.scale = (uint8_t)clampi((int32_t)ui.scale + s, 0, NSCALE - 1);
            lx_post();                           /* (the loops moved walk the new scale) */
            ui.sc_touched = 1;
            ui.sc_touch_until = fm1_ms + 700u;
        }
        s = fm1_enc_take(PANEL_ENC[EN_ALGO]);
        if (s) {
            ui.root = (uint8_t)(((int32_t)ui.root + s + 1200) % 12);
            rack_root = SCALE_BASE + ui.root;
            lx_post();
            ui.sc_touched = 0;
            ui.sc_touch_until = fm1_ms + 700u;
        }
    } else {                                     /* SELECT the generator's level, ALGORITHM its pan (Tonitor's) */
        volatile int8_t *lv = &gen_lvl[ui.gen];   /* (the generator the keys play: chosen, or chosen last) */
        volatile int8_t *pn = &gen_pan[ui.gen];
        s = fm1_enc_take(PANEL_ENC[EN_SELECT]);
        if (s) {
            *lv = (int8_t)clampi(*lv + s, 0, LEVEL_MAX);
            if (*lv)
                say_n("LEVEL ", *lv, "", 800);
            else
                say("LEVEL OFF", 800);
        }
        s = fm1_enc_take(PANEL_ENC[EN_ALGO]);
        if (s) {
            int32_t p = clampi(*pn + s, -PAN_MAX, PAN_MAX);
            *pn = (int8_t)p;
            if (!p)
                say("PAN C", 800);
            else
                say_n(p < 0 ? "PAN L" : "PAN R", p < 0 ? -p : p, "", 800);
        }
    }
    if (ui.fx_page == 3u) {                      /* KNOB 1 .. 4: the MOD entry's */
        static const uint8_t K[4] = {EN_K1, EN_K2, EN_K3, EN_K4};
        for (k = 0; k < 4u; k++)
            if ((s = fm1_enc_take(PANEL_ENC[K[k]])) != 0)
                mod_knob(ui.sel, ui.mod_ix, k, s);
    } else if (ui.fx_page) {                     /* KNOB 1 .. 4: the delay, or the reverb */
        static const uint8_t K[4] = {EN_K1, EN_K2, EN_K3, EN_K4};
        int8_t *pv = ui.fx_page == 1u ? ui.dlp : ui.rvp;
        for (k = 0; k < 4u; k++) {
            s = fm1_enc_take(PANEL_ENC[K[k]]);
            if (s) {
                pv[k] = (int8_t)clampi(pv[k] + 2 * s, 0, 100);
                if (ui.fx_page == 1u)
                    mdl_param(ui.dlp[0], ui.dlp[1], ui.dlp[2], ui.dlp[3]);
                else
                    rv_param(&mrv, ui.rvp[0], ui.rvp[1], ui.rvp[2], ui.rvp[3]);
                ui.fx_touched = (int8_t)k;
                ui.fx_touch_until = fm1_ms + 1500u;
            }
        }
    } else {                                     /* KNOB 1: the morph time; KNOB 3, KNOB 4: the generator's sends */
        s = fm1_enc_take(PANEL_ENC[EN_K1]);
        if (s) {
            char m[16] = "MORPH ";
            ui.morph = (uint8_t)clampi((int32_t)ui.morph + s, 0, MORPH_STEPS_UI);
            if (ui.morph) {
                fmt_fix(m + 6, ui.morph * 5, 1);
                str_cpy(m + str_len(m), " S", 3);
            } else
                str_cpy(m + 6, "OFF", 4);
            say(m, 1200);
        }
        s = fm1_enc_take(PANEL_ENC[EN_K3]);
        if (s) {
            ui.dsend[ui.gen] = (int8_t)clampi(ui.dsend[ui.gen] + 2 * s, 0, 100);
            sends_apply(ui.gen);
            say_n("DELAY ", ui.dsend[ui.gen], "", 1000);
        }
        s = fm1_enc_take(PANEL_ENC[EN_K4]);
        if (s) {
            ui.rsend[ui.gen] = (int8_t)clampi(ui.rsend[ui.gen] + 2 * s, 0, 100);
            sends_apply(ui.gen);
            say_n("REVERB ", ui.rsend[ui.gen], "", 1000);
        }
    }
    for (k = 0; k < NE; k++)                     /* (the other knobs: nothing yet; their steps dropped) */
        fm1_enc_take(PANEL_ENC[k]);
    /* the keys: the generator chosen last; a key lifted ends its note on the slot it was played on */
    keys = fm1_in.notes;
    (void)fm1_input_note_edges();
    ch = keys ^ ui.keys;
    if (ui.snap_page) {                          /* the SNAPSHOTS page: the white keys are the 16 snapshots, no notes */
        for (k = 0; k < NKEY; k++)
            if (((ch & keys) >> k & 1u) && white_ix(k) >= 0 && white_ix(k) < (int32_t)NSNAP) {
                ui.snap_key = (int8_t)white_ix(k);
                ui.snap_t0 = fm1_ms;
                ui.snap_done = 0;
                ui.snap_sh = (uint8_t)shift_held();
                ui.snap_ix = (uint8_t)ui.snap_key;
                ui.force = 1;
            }
        if (ui.snap_key >= 0) {
            uint32_t kk;
            int down = 0;
            for (kk = 0; kk < NKEY; kk++)
                if (white_ix(kk) == ui.snap_key && ((keys >> kk) & 1u))
                    down = 1;
            if (down && !ui.snap_done && (uint32_t)(fm1_ms - ui.snap_t0) >= HOLD_MS) {
                ui.snap_done = 1;                /* held: stored, or (SHIFT) emptied */
                if (ui.snap_sh || shift_held()) {
                    snap_clear((uint32_t)ui.snap_key);
                    say("EMPTIED", 1000);
                } else {
                    snap_store((uint32_t)ui.snap_key);
                    ses_done(SES_OK, "STORED", (uint32_t)ui.snap_key + 1u);
                }
                ui.force = 1;
            } else if (!down) {                  /* let go soon: recalled */
                if (!ui.snap_done) {
                    if (snap_has((uint32_t)ui.snap_key))
                        ses_done(snap_recall((uint32_t)ui.snap_key), "RECALLED", (uint32_t)ui.snap_key + 1u);
                    else
                        say("EMPTY", 900);
                }
                ui.snap_key = -1;
                ui.force = 1;
            }
        }
        ui.keys = keys;
        midi_take();
        return;
    }
    if ((ch & keys) >> KEY_REWIRE & 1u) {        /* the first black key: the chosen slot rewired */
        int ok = ui.sel < NGEN ? gen_rewire(ui.sel, next_seed()) : fx_rewire(ui.sel - NGEN, next_seed());
        say_n(ok ? "REWIRED " : "BUSY ", (int32_t)ui.sel + 1, "", 1000);
    }
    {   /* the loops' direction (REVERSE, PENDULUM, RANDOM) and speed (HALF, DOUBLE); pressed again, as recorded */
        const looper_t *p = &loop[ui.gen];
        uint32_t dn = ch & keys;
        if ((dn >> KEY_REV & 1u) || (dn >> KEY_PEND & 1u) || (dn >> KEY_RAND & 1u)) {
            uint32_t want = (dn >> KEY_REV & 1u) ? LD_REV : (dn >> KEY_PEND & 1u) ? LD_PEND : LD_RAND;
            want = p->dmode == want ? LD_FWD : want;
            ev_post(EV_LOOP_MODE, ui.gen, 0, want);
            say(want == LD_REV ? "REVERSE" : want == LD_PEND ? "PENDULUM" : want == LD_RAND ? "RANDOM" : "FORWARD", 1200);
        }
        if ((dn >> KEY_HALF & 1u) || (dn >> KEY_DOUBLE & 1u)) {
            uint32_t want = (dn >> KEY_HALF & 1u) ? LS_HALF : LS_DOUBLE;
            want = p->rate == want ? LS_NORMAL : want;
            ev_post(EV_LOOP_MODE, ui.gen, 1, want);
            say(want == LS_HALF ? "HALF SPEED" : want == LS_DOUBLE ? "DOUBLE SPEED" : "NORMAL SPEED", 1200);
        }
    }
    if ((ch & keys) >> KEY_SHIFT & 1u)           /* SHIFT pressed: the keys held latched */
        for (k = 0; k < NKEY; k++)
            if (((keys >> k) & 1u) && k != KEY_SHIFT && ui.key_note[k] != KEY_NONE) {
                uint32_t g = ui.key_slot[k];
                ui.latch[g] |= 1u << k;
                ui.latch_note[g][k] = ui.key_note[k];
                ev_post(EV_LATCH, g, ui.key_note[k], 0);
                ui.key_note[k] = KEY_NONE;       /* (letting go of the key no longer ends it) */
            }
    for (k = 0; ch && k < NKEY; k++) {
        if (!((ch >> k) & 1u) || white_ix(k) < 0)
            continue;                            /* (the black keys: no notes) */
        if (((keys >> k) & 1u) && ui.ovd) {      /* PLAY held: white key w, loop w in or out */
            uint32_t w = (uint32_t)white_ix(k);
            const looper_t *p = &loop[ui.gen];
            ui.key_note[k] = KEY_NONE;
            if (w < p->ntrk)
                ev_post(EV_LOOP_TRK, ui.gen, w, (p->mute >> w) & 1u);
            ui.force = 1;
        } else if ((keys >> k) & 1u) {
            /* (moved as the generator's loops are -- all of them, and its own -- so what is added fits them) */
            uint32_t note = scale_note(white_ix(k) + ui.lx + ui.lxt[ui.gen], ui.oct), bit = 1u << k;
            ui.key_note[k] = KEY_NONE;
            if ((ui.latch[ui.gen] & bit) && ((keys >> KEY_SHIFT) & 1u)) {   /* SHIFT held, a lit key: let go */
                ui.latch[ui.gen] &= ~bit;
                ev_post(EV_OFF, ui.gen, ui.latch_note[ui.gen][k], 0);
                continue;
            }
            if (ui.latch[ui.gen] & bit)
                continue;                        /* (a latched key alone: nothing) */
            ui.key_note[k] = (uint8_t)note;
            ui.key_slot[k] = ui.gen;
            ev_post(EV_ON, ui.gen, note, 0);
        } else if (ui.key_note[k] != KEY_NONE) {
            ev_post(EV_OFF, ui.key_slot[k], ui.key_note[k], 0);
            ui.key_note[k] = KEY_NONE;
        }
    }
    ui.keys = keys;
    midi_take();
}

/* ---------------------------------------------------------------- LEDs --- */
static void ui_leds(void)
{
    uint32_t k;
    for (k = 0; k < NSLOT; k++) {                /* the chosen slot lit; a muted one blinking */
        int muted = k < NGEN ? gen_mute[k] : fx_mute[k - NGEN];
        fm1_led_key(PANEL_BTN[SLOT_BTN[k]], k == ui.sel ? !muted || (fm1_ms / 300u) & 1u : muted && (fm1_ms / 300u) % 4u == 0u);
    }
    fm1_led_key(PANEL_BTN[B_HOME], (int32_t)(ui.home_until - fm1_ms) > 0);
    fm1_led_key(PANEL_BTN[B_ARP], ui.sc_page);
    fm1_led_key(PANEL_BTN[B_SEQ], ui.fx_page != 0u);
    fm1_led_key(PANEL_BTN[B_SAVE], ui.ses_page || ui.snap_page || ui.save_down);
    {
        const looper_t *p = &loop[ui.gen];
        fm1_led_key(PANEL_BTN[B_REC], p->rec == LR_REC || (p->rec == LR_ARMED && (fm1_ms / 250u) & 1u));
        fm1_led_key(PANEL_BTN[B_PLAY], p->play && p->ntrk);
    }
    fm1_led_key(PANEL_BTN[B_OCTDN], ui.oct < 0);
    fm1_led_key(PANEL_BTN[B_OCTUP], ui.oct > 0);
    for (k = 0; k < NKEY; k++) {                 /* a key held, or latched on the generator the keys play */
        int32_t w = white_ix(k);
        const looper_t *p = &loop[ui.gen];
        if (ui.snap_page)                        /* the SNAPSHOTS page: the snapshots that hold a state, lit (the one a
                                                  * morph flows into blinking) */
            fm1_led_key(14u + k, w >= 0 && w < (int32_t)NSNAP && snap_has((uint32_t)w) &&
                                 (w != snap_morph_target() || (fm1_ms / 250u) & 1u));
        else if (ui.ovd)                         /* PLAY held: the loops that play, lit */
            fm1_led_key(14u + k, w >= 0 && (uint32_t)w < p->ntrk && !((p->mute >> w) & 1u));
        else if (w >= 0)
            fm1_led_key(14u + k, ((ui.keys >> k) & 1u) || ((ui.latch[ui.gen] >> k) & 1u));
        else                                     /* the loops' direction and speed keys, lit while on */
            fm1_led_key(14u + k, p->ntrk && ((k == KEY_REV && p->dmode == LD_REV) || (k == KEY_PEND && p->dmode == LD_PEND) ||
                                             (k == KEY_RAND && p->dmode == LD_RAND) || (k == KEY_HALF && p->rate == LS_HALF) ||
                                             (k == KEY_DOUBLE && p->rate == LS_DOUBLE)));
    }
    fm1_led_key(14u + KEY_SHIFT, shift_held());
}

/* -------------------------------------------------------------- the screen --- */
/* a colour at a share of its brightness (Q8: 256 full) */
static uint16_t col_scale(uint16_t c, int32_t q8)
{
    int32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return (uint16_t)(((r * q8 >> 8) << 11) | ((g * q8 >> 8) << 5) | (b * q8 >> 8));
}
static int32_t level_step(int32_t pk)            /* a peak as one of four steps */
{
    return pk < 120 ? 0 : pk < 1500 ? 1 : pk < 6000 ? 2 : 3;
}

/* the SCALE page (Tonitor's): the title; the root, very large; the scale's name. What was just turned is drawn black
 * on white for a moment */
static void draw_sc_page(void)
{
    int32_t t = (int32_t)(ui.sc_touch_until - fm1_ms) > 0 ? ui.sc_touched : -1;
    uint16_t bg, fg;
    if (!ui.force && ui.shown_scale == ui.scale && ui.shown_root == ui.root && ui.shown_sc_touch == t)
        return;
    cv_begin(240, 50, C_BLACK);
    cv_text_c(6, &FONT_M, "SCALE", C_SCALE);
    cv_blit(0, 0);
    bg = t == 0 ? C_WHITE : C_BLACK;
    fg = t == 0 ? C_BLACK : C_WHITE;
    CV_AREA(0, 50, 240, 100, bg)
        cv_text_c(4, &FONT_XL, NOTE_NAME[ui.root], fg);
    bg = t == 1 ? C_WHITE : C_BLACK;
    fg = t == 1 ? C_BLACK : C_SCALE;
    CV_AREA(0, 150, 240, 90, bg) {
        if (SCALE_L2[ui.scale][0]) {
            cv_text_fit(4, &FONT_M, SCALE_L1[ui.scale], fg);
            cv_text_fit(46, &FONT_M, SCALE_L2[ui.scale], fg);
        } else
            cv_text_fit(26, &FONT_M, SCALE_L1[ui.scale], fg);
    }
    ui.shown_scale = ui.scale;
    ui.shown_root = ui.root;
    ui.shown_sc_touch = (int8_t)t;
}

/* the battery at the top right, drawn large: a body and a cap; three bars green, two green, one amber, one red when
 * low; a bolt on a USB host's power. MELODEE's thresholds (Tonitor's); -1 none yet, 4 USB */
static int32_t batt_key(void)
{
    if (!batt_raw)
        return -1;
    if (usb.config && !usb.suspended)
        return 4;
    return batt_raw >= 591 ? 3 : batt_raw >= 561 ? 2 : batt_raw >= 531 ? 1 : 0;
}
static void draw_batt(int32_t xr, int32_t y)
{
    int32_t k = batt_key(), w = 34, h = 20, x0 = xr - w - 5, i;
    uint16_t c = k >= 2 ? C_OK : k == 1 ? RGB(255, 170, 0) : RGB(255, 40, 40);
    if (k < 0)
        return;
    cv_frame(x0, y, w, h, 3, C_WHITE);
    cv_rect(x0 + w, y + 6, 5, h - 12, C_WHITE);
    if (k == 4) {                                /* (the bolt) */
        cv_rect(x0 + 15, y + 4, 5, 5, C_WHITE);
        cv_rect(x0 + 11, y + 8, 12, 4, C_WHITE);
        cv_rect(x0 + 14, y + 11, 5, 5, C_WHITE);
    } else
        for (i = 0; i < (k ? k : 1); i++)
            cv_rect(x0 + 6 + i * 8, y + 6, 6, 8, c);
}
/* the top band: the load and the battery; or the message across */
static void draw_top(void)
{
    int msg = (int32_t)(ui.msg_until - fm1_ms) > 0;
    int32_t key;
    if ((uint32_t)(fm1_ms - ui.load_at) >= 500u) {   /* (the load, twice a second, in steps of 5 %) */
        ui.load_at = fm1_ms;
        ui.load_shown = (uint8_t)((load_pct + 2u) / 5u * 5u);
    }
    key = msg ? -1 - (int32_t)(ui.msg_until & 0xFFFFFu) : ui.load_shown * 10 + batt_key() + 1;
    if (!ui.force && key == ui.shown_top)
        return;
    if (msg) {
        cv_begin(240, TOP_H, C_WHITE);
        cv_text_fit(2, &FONT_M, ui.msg, C_BLACK);
    } else {
        char b[8];
        uint32_t n;
        cv_begin(240, TOP_H, C_BLACK);
        fmt_int(b, ui.load_shown);
        n = str_len(b);
        b[n] = '%';
        b[n + 1] = 0;
        cv_text(6, 7, &FONT_S, b, ui.load_shown >= 85 ? RGB(255, 60, 60) : C_GREY);
        draw_batt(236, 10);
    }
    cv_blit(0, 0);
    ui.shown_top = key;
}
/* a generator's block: filled in its colour when heard (brighter as it sounds), an outline when muted; framed in
 * white when chosen, in grey when an effect slot is chosen and the keys play it; its number; the colour of the
 * generator that modulates it, in a corner */
static int32_t gen_key(uint32_t l)
{
    const gen_patch_t *p = &gbuf[l][g_use[l]];
    int32_t xm = p->xsrc < NGEN ? p->xsrc + 1 : 0, fr = ui.sel == l ? 2 : ui.gen == l ? 1 : 0;
    const looper_t *q = &loop[l];
    int32_t lr = q->rec == LR_REC ? 3 : q->rec == LR_ARMED ? 1 + (int32_t)((fm1_ms / 250u) & 1u) : 0;
    return level_step(gen_mute[l] ? 0 : gen_meter[l]) | gen_mute[l] << 2 | fr << 3 | xm << 5 | lr << 7 |
           (int32_t)(q->play && q->ntrk) << 9;
}
static void draw_gen(uint32_t l, int32_t key)
{
    uint16_t c = GEN_COL[l], fg, bg = 0;
    int32_t st = key & 3, fr = (key >> 3) & 3, xm = (key >> 5) & 3, w = GEN_P - 2;
    char d[2] = {(char)('1' + l), 0};
    cv_begin((uint32_t)w, GEN_H, C_BLACK);
    if (!gen_mute[l]) {
        bg = col_scale(c, 184 + st * 24);
        cv_rect(4, 4, w - 8, GEN_H - 8, bg);
        fg = C_BLACK;
        cv_bg_set(bg);
    } else {
        cv_frame(6, 6, w - 12, GEN_H - 12, 4, c);
        fg = c;
    }
    if (fr == 2)
        cv_frame(0, 0, w, GEN_H, 4, C_WHITE);
    else if (fr == 1)
        cv_frame(0, 0, w, GEN_H, 2, C_GREY);
    cv_text((w - text_w(&FONT_M, d)) / 2 + ((key >> 7) & 7 ? 8 : 0), (GEN_H - FONT_M.h) / 2, &FONT_M, d, fg);   /* (moved
                                                                                * right by the looper's mark) */
    {   /* its looper, at the left: recording, a red disc; armed, a red ring blinking; its loops playing, a ring */
        int32_t lr = (key >> 7) & 3, cy = GEN_H / 2;   /* (white and black: seen on every generator's colour) */
        if (lr == 3) {
            cv_disc(15, cy, 11, C_BLACK);
            cv_disc(15, cy, 8, C_WHITE);
        } else if (lr == 1) {
            cv_disc(15, cy, 11, C_BLACK);
            cv_ring(15, cy, 8, 3, C_WHITE);
        } else if ((key >> 9) & 1)
            cv_ring(15, cy, 10, 4, fg);
    }
    if (xm) {                                    /* modulated by another generator */
        cv_rect(w - 20, 4, 16, 16, C_BLACK);
        cv_rect(w - 18, 6, 12, 12, GEN_COL[xm - 1]);
    }
    cv_blit(l * GEN_P + 1u, GEN_Y);
}
/* a border t pixels thick round a w x h box, shared out between n colours: the perimeter walked clockwise from the top
 * left, each colour its equal stretch of it */
static void cv_border_shared(int32_t w, int32_t h, int32_t t, const uint16_t *col, int32_t n)
{
    int32_t per = 2 * (w + h), k;
    for (k = 0; k < n; k++) {
        int32_t a = per * k / n, b = per * (k + 1) / n, lo, hi;
        uint16_t c = col[k];
        lo = clampi(a, 0, w), hi = clampi(b, 0, w);                  /* the top, left to right */
        if (hi > lo)
            cv_rect(lo, 0, hi - lo, t, c);
        lo = clampi(a - w, 0, h), hi = clampi(b - w, 0, h);          /* the right side, down */
        if (hi > lo)
            cv_rect(w - t, lo, t, hi - lo, c);
        lo = clampi(a - w - h, 0, w), hi = clampi(b - w - h, 0, w);  /* the bottom, right to left */
        if (hi > lo)
            cv_rect(w - hi, h - t, hi - lo, t, c);
        lo = clampi(a - 2 * w - h, 0, h), hi = clampi(b - 2 * w - h, 0, h);   /* the left side, up */
        if (hi > lo)
            cv_rect(0, h - hi, t, hi - lo, c);
    }
}
/* an effect slot's row: framed in white when chosen; its tiles inside */
static int32_t row_key(uint32_t s)
{
    const fx_patch_t *f = &fbuf[s][f_use[s]];
    return (int32_t)(ui.sel == NGEN + s) | fx_mute[s] << 1 | f->n << 2 | (int32_t)(f->serial & 0xFFFFu) << 4 |
           (fx_phase[s] == FS_CLEAR) << 20;
}
static void draw_row(uint32_t s, int32_t key)
{
    uint16_t c = (key & 1) ? C_WHITE : C_BLACK;
    uint32_t y = FXR_Y + s * FXR_H;
    lcd_fill(0, y, 240, 3, c);
    lcd_fill(0, y + FXR_H - 3, 240, 3, c);
    lcd_fill(0, y, 3, FXR_H, c);
    lcd_fill(237, y, 3, FXR_H, c);
}
/* a row's inside blanked where its tiles do not cover it (the edge, the gaps between them, the line under them): the
 * tiles paint the rest, so each pixel goes to the panel once. A slot being cleared (no tiles): all of it */
static void row_clear(uint32_t s)
{
    const fx_patch_t *f = &fbuf[s][f_use[s]];
    uint32_t y = FXR_Y + s * FXR_H, n = f->n, k, pitch;
    if (fx_phase[s] == FS_CLEAR || n < 1u || n > FX_N) {
        lcd_fill(3, y + 3, 234, FXR_H - 6, C_BLACK);
        return;
    }
    pitch = 234u / n;
    lcd_fill(3, y + 3, 1, FXR_H - 7, C_BLACK);             /* (draw_tile: a tile k at 3 + k pitch + 1, pitch - 2 wide, */
    for (k = 0; k < n; k++) {                               /* FXR_H - 7 high, from y + 3) */
        uint32_t x1 = 3u + k * pitch + 1u + pitch - 2u;
        lcd_fill(x1, y + 3, k + 1u < n ? 2u : 237u - x1, FXR_H - 7, C_BLACK);
    }
    lcd_fill(3, y + FXR_H - 4, 234, 1, C_BLACK);
}
/* an effect's tile: its name; round it, a border in the colours of the generators wired into it (shared out equally);
 * filled with the colour of the generator the keys play when that one is wired into it; brighter as sound passes */
static int32_t tile_key(uint32_t inst, const uint32_t *mods)
{
    uint32_t s = inst / FX_N, k = inst % FX_N;
    const fx_patch_t *f = &fbuf[s][f_use[s]];
    int32_t feed = 0, g;
    if (k >= f->n || fx_phase[s] == FS_CLEAR)
        return -2;                               /* (no effect there) */
    for (g = 0; g < NGEN; g++)
        if (!gen_mute[g] && ((mods[g] >> inst) & 1u))
            feed |= 1 << g;
    return level_step(fx_mute[s] ? 0 : rm_meter[inst]) | feed << 2 | (int32_t)(!gen_mute[ui.gen] && ((mods[ui.gen] >> inst) & 1u)) << 5 |
           (int32_t)ui.gen << 6 | fx_mute[s] << 8 | f->e[k].kind << 9 | f->n << 14 | (int32_t)(f->serial & 0xFFu) << 16;
}
static void draw_tile(uint32_t inst, int32_t key)
{
    uint32_t s = inst / FX_N, k = inst % FX_N, n = (uint32_t)(key >> 14) & 3u, kind = (uint32_t)(key >> 9) & 31u;
    int32_t st = key & 3, feed = (key >> 2) & 7, hi = (key >> 5) & 1, muted = (key >> 8) & 1, g, nc = 0;
    int32_t pitch = 234 / (int32_t)(n ? n : 1u), w = pitch - 2, h = FXR_H - 7;
    static const uint8_t GREY[4] = {18, 40, 66, 96};
    uint16_t col[NGEN], bg, fg;
    if (key == -2)
        return;
    if (muted) {
        bg = RGB(10, 10, 10);
        fg = RGB(70, 70, 70);
    } else if (hi) {
        bg = col_scale(GEN_COL[ui.gen], 60 + st * 24);
        fg = C_WHITE;
    } else {
        bg = RGB(GREY[st], GREY[st], GREY[st]);
        fg = feed ? RGB(160, 160, 160) : RGB(90, 90, 90);
    }
    cv_begin((uint32_t)w, (uint32_t)h, bg);
    for (g = 0; g < NGEN; g++)
        if ((feed >> g) & 1)
            col[nc++] = muted ? col_scale(GEN_COL[g], 90) : GEN_COL[g];
    if (nc)
        cv_border_shared(w, h, 4, col, nc);
    cv_bg_set(bg);
    cv_text_fit((h - FONT_S.h) / 2 + 1, &FONT_S, RM_TILE[kind], fg);
    cv_blit(3u + k * (uint32_t)pitch + 1u, FXR_Y + s * FXR_H + 3u);
}
/* PLAY held: the loops of the generator the keys play, 1 .. 10 as the white keys have them -- in, its colour; out, a
 * grey frame (Tonitor's) */
static void draw_ovd_page(void)
{
    const looper_t *p = &loop[ui.gen];
    int32_t key = p->ntrk | p->mute << 4 | (int32_t)ui.gen << 16;
    uint32_t r, c;
    uint16_t col = GEN_COL[ui.gen];
    char t[12] = "LOOPS ";
    if (!ui.force && ui.shown_ovd == key)
        return;
    ui.shown_ovd = key;
    t[6] = (char)('1' + ui.gen);
    t[7] = 0;
    cv_begin(240, 52, C_BLACK);
    cv_text_c((52 - FONT_M.h) / 2, &FONT_M, p->ntrk ? t : "NO LOOPS", col);
    cv_blit(0, 0);
    for (r = 0; r < 2u; r++)
        CV_AREA(0, 52 + 94 * r, 240, 94, C_BLACK)
        for (c = 0; c < 5u; c++) {
            uint32_t k = r * 5u + c;
            int32_t x = 4 + (int32_t)c * 47;
            char n[4];
            const font_t *f = k < 9u ? &FONT_M : &FONT_S;
            if (k >= p->ntrk)
                continue;
            fmt_int(n, (int32_t)k + 1);
            if (!((p->mute >> k) & 1u)) {
                cv_rect(x, 6, 43, 82, col);
                cv_bg_set(col);
                cv_text(x + (43 - text_w(f, n)) / 2, 6 + (82 - f->h) / 2, f, n, C_BLACK);
                cv_bg_set(C_BLACK);
            } else {
                cv_frame(x, 6, 43, 82, 3, C_GREY);
                cv_text(x + (43 - text_w(f, n)) / 2, 6 + (82 - f->h) / 2, f, n, C_GREY);
            }
        }
}

/* the DELAY and REVERB pages (Tonitor's): the title, large; a row a parameter -- its name, its value (TIME in ms),
 * a bar; the row just turned black on white */
static void draw_fx_title(void)
{
    cv_begin(240, 52, C_BLACK);
    cv_text_c(3, &FONT_L, ui.fx_page == 1u ? "DELAY" : "REVERB", ui.fx_page == 1u ? C_DELAY : C_SEND);
    cv_blit(0, 0);
}
static void draw_fx_row(uint32_t j, int touched)
{
    char v[12];
    const int8_t *pv = ui.fx_page == 1u ? ui.dlp : ui.rvp;
    uint16_t bg = touched ? C_WHITE : C_BLACK, fg = touched ? C_BLACK : C_WHITE, c = ui.fx_page == 1u ? C_DELAY : C_SEND;
    const char *name = ui.fx_page == 1u ? DL_NAME[j] : RV_NAME[j];
    const font_t *f = &FONT_M;
    int32_t y = 1;
    cv_begin(240, 47, bg);
    if (ui.fx_page == 1u && j == 0u) {           /* (the delay's TIME in ms) */
        fmt_int(v, mdl_ms(pv[0]));
        str_cpy(v + str_len(v), " MS", 4);
    } else
        fmt_int(v, pv[j]);
    if (text_w(f, name) + text_w(f, v) > 220) {   /* (FEEDBACK 60, TIME 400 MS: a size smaller) */
        f = &FONT_S;
        y = 1 + (FONT_M.h - FONT_S.h) / 2;
    }
    cv_text(6, y, f, name, fg);
    cv_text(234 - text_w(f, v), y, f, v, fg);
    cv_rect(6, 38, 228, 6, touched ? C_GREY : RGB(60, 60, 60));
    cv_rect(6, 38, pv[j] * 228 / 100, 6, touched ? C_BLACK : c);
    cv_blit(0, 52 + 47 * j);
    ui.shown_fxp[j] = pv[j];
}
static void draw_fx_page(void)
{
    uint32_t j;
    int32_t t = (int32_t)(ui.fx_touch_until - fm1_ms) > 0 ? ui.fx_touched : -1;
    const int8_t *pv = ui.fx_page == 1u ? ui.dlp : ui.rvp;
    if (ui.force) {
        draw_fx_title();
        for (j = 0; j < 4u; j++)
            draw_fx_row(j, (int32_t)j == t);
        ui.shown_fx_touch = (int8_t)t;
        return;
    }
    for (j = 0; j < 4u; j++)
        if (ui.shown_fxp[j] != pv[j] || (((int32_t)j == t) != ((int32_t)j == ui.shown_fx_touch)))
            draw_fx_row(j, (int32_t)j == t);
    ui.shown_fx_touch = (int8_t)t;
}

/* the whole screen's message (flash): l1 very large, l2 under it */
static int draw_flash(void)
{
    if ((int32_t)(ui.fl_until - fm1_ms) <= 0) {
        if (ui.fl_shown) {
            ui.fl_shown = 0;
            ui.force = 1;
            ui.shown_page = 0xFF;
        }
        return 0;
    }
    if (!ui.fl_shown) {                          /* (every pixel painted once, top to bottom: no clear first) */
        CV_AREA(0, 0, 240, 50, C_BLACK) {
        }
        CV_AREA(0, 50, 240, 70, C_BLACK)
            cv_text_fit(10, &FONT_M, ui.fl1, ui.fl_col);
        CV_AREA(0, 120, 240, 120, C_BLACK)
            if (ui.fl2[0])
                cv_text_c(4, &FONT_XL, ui.fl2, C_WHITE);
        ui.fl_shown = 1;
    }
    return 1;
}
/* NO / YES, the answer shown black on white (Tonitor's) */
static void draw_noyes(int yes)
{
    cv_rect(10, 10, 100, 56, yes ? C_BLACK : C_WHITE);
    cv_rect(130, 10, 100, 56, yes ? C_WHITE : C_BLACK);
    cv_frame(10, 10, 100, 56, 3, C_WHITE);
    cv_frame(130, 10, 100, 56, 3, C_WHITE);
    cv_bg_set(yes ? C_BLACK : C_WHITE);
    cv_text(60 - text_w(&FONT_M, "NO") / 2, 10 + (56 - FONT_M.h) / 2, &FONT_M, "NO", yes ? C_WHITE : C_BLACK);
    cv_bg_set(yes ? C_WHITE : C_BLACK);
    cv_text(180 - text_w(&FONT_M, "YES") / 2, 10 + (56 - FONT_M.h) / 2, &FONT_M, "YES", yes ? C_BLACK : C_WHITE);
    cv_bg_set(C_BLACK);
}
/* the SESSIONS page (Tonitor's): the title; the slot's number very large (red: it holds a session; grey: empty);
 * LOADED, CHOSEN or EMPTY under it -- or, asked, NO / YES */
static void draw_ses_page(void)
{
    char n[8];
    int has = ses_has(ui.ses_ix);
    int32_t key = ui.ses_page | (ui.ses_page == SP_ASK ? ui.ses_yes : ui.ses_ix) << 4;
    if (!ui.force && ui.shown_ses == key)
        return;
    ui.shown_ses = key;
    {
        const char *t = ui.ses_page != SP_ASK ? "SESSIONS" : ui.ses_over ? "OVERWRITE?" : "SAVE NEW";
        cv_begin(240, 52, C_BLACK);
        cv_text_fit((52 - FONT_M.h) / 2, &FONT_M, t, ui.ses_page == SP_ASK && ui.ses_over ? C_FULL : C_SES);
        cv_blit(0, 0);
    }
    fmt_int(n, ui.ses_page == SP_ASK ? ui.ses_new : ui.ses_ix);
    CV_AREA(0, 52, 240, 112, C_BLACK)
        cv_text_c(8, &FONT_XL, n, ui.ses_page == SP_ASK ? (ui.ses_over ? C_FULL : C_WHITE) : has ? C_FULL : C_GREY);
    CV_AREA(0, 164, 240, 76, C_BLACK) {
        if (ui.ses_page == SP_ASK)
            draw_noyes(ui.ses_yes);
        else if (ui.ses_ix == ses_current())
            cv_text_c(20, &FONT_M, has ? "LOADED" : "CHOSEN", C_OK);
        else if (!has)
            cv_text_c(20, &FONT_M, ses_room() ? "EMPTY" : "NO ROOM", C_GREY);
    }
}
/* the SNAPSHOTS page: 1 .. 16 in four rows of four (as the white keys have them) -- holding a state, filled (the one
 * last stored or recalled in white); empty, a grey frame; the one framed in white (PRESETS, a key) */
static void draw_snap_page(void)
{
    uint32_t r, c, used = 0;
    int32_t key, cur = snap_current();
    for (r = 0; r < NSNAP; r++)
        used |= (uint32_t)snap_has(r) << r;
    {   /* (a morph: the square it flows into blinks) */
        int32_t tg = snap_morph_target();
        key = (int32_t)(used ^ (uint32_t)ui.snap_ix << 16 ^ (uint32_t)(cur + 1) << 21 ^
                        (uint32_t)(tg >= 0 ? (tg + 1) * 2 + ((fm1_ms / 250u) & 1u) : 0) << 26);
    }
    if (!ui.force && ui.shown_snap == key)
        return;
    ui.shown_snap = key;
    {   /* (the title a size smaller where the large one does not fit: never squeezed) */
        const font_t *tf = text_w(&FONT_M, "SNAPSHOTS") <= 232 ? &FONT_M : &FONT_S;
        cv_begin(240, 52, C_BLACK);
        cv_text_c((52 - tf->h) / 2, tf, "SNAPSHOTS", C_SNAP);
        cv_blit(0, 0);
    }
    for (r = 0; r < 4u; r++)
        CV_AREA(0, 52 + 47 * r, 240, 47, C_BLACK)
        for (c = 0; c < 4u; c++) {
            uint32_t k = r * 4u + c;
            int32_t x = 2 + (int32_t)c * 60;
            char n[4];
            uint16_t fill = (int32_t)k == cur ? C_WHITE : C_SNAP;
            if ((int32_t)k == snap_morph_target())   /* (flowing into it: white and black by turns) */
                fill = (fm1_ms / 250u) & 1u ? C_WHITE : C_GREY;
            const font_t *nf;
            fmt_int(n, (int32_t)k + 1);
            nf = k < 9u ? &FONT_M : &FONT_S;   /* (10 .. 16 a size smaller, alike: inside the box) */
            if ((used >> k) & 1u) {
                cv_rect(x + 4, 4, 48, 39, fill);
                cv_bg_set(fill);
                cv_text(x + 4 + (48 - text_w(nf, n)) / 2, 4 + (39 - nf->h) / 2, nf, n, C_BLACK);
                cv_bg_set(C_BLACK);
            } else {
                cv_frame(x + 4, 4, 48, 39, 2, C_GREY);
                cv_text(x + 4 + (48 - text_w(nf, n)) / 2, 4 + (39 - nf->h) / 2, nf, n, C_GREY);
            }
            if (k == ui.snap_ix)
                cv_frame(x, 0, 56, 47, 4, C_WHITE);
        }
}
/* the MOD page (Tonitor's): the title (the slot and the entry); a row a value on two lines, the name small with its
 * bar, the value under it; the row just turned black on white; MOD A .. D: NOW, what it does at this moment */
static void mod_dst_name(uint32_t sl, uint32_t d, char *b, uint32_t n)
{
    if (sl < NGEN) {
        str_cpy(b, d < 4u ? mo_mac_name(&gbuf[sl][g_use[sl]].gen, d) : d < GMD_N ? GMD_NAME[d] : "", n);
        return;
    }
    if (d < FMD_LVL) {                           /* (the effect standing at that place now, and its value) */
        const fx_patch_t *f = &fbuf[sl - NGEN][f_pub[sl - NGEN]];
        uint32_t k = d / 4u, j = d % 4u;
        if (k < f->n) {
            str_cpy(b, RM_NAME[f->e[k].kind % NKIND], n);
            str_cpy(b + str_len(b), " ", n - str_len(b));
            str_cpy(b + str_len(b), RM_PNAME[f->e[k].kind % NKIND][j], n - str_len(b));
        } else {
            str_cpy(b, "PLACE 3: NONE", n);
            b[6] = (char)('1' + k);
        }
        return;
    }
    str_cpy(b, d - FMD_LVL < 4u ? FMD_NAME[d - FMD_LVL] : "", n);
}
static void draw_mod_row(uint32_t j, const char *name, const char *val, int bar, int32_t v, int touched, uint16_t col)
{
    uint16_t bg = touched ? C_WHITE : C_BLACK, fg = touched ? C_BLACK : C_WHITE;
    cv_begin(240, 47, bg);
    cv_text(6, 0, &FONT_XS, name, touched ? C_BLACK : RGB(175, 175, 175));
    if (bar) {
        int32_t x0 = 6 + text_w(&FONT_XS, name) + 12, w0 = 234 - x0, y0 = 6;
        cv_rect(x0, y0, w0, 7, touched ? C_GREY : RGB(60, 60, 60));
        if (bar == 1)
            cv_rect(x0, y0, clampi(v, 0, 100) * w0 / 100, 7, touched ? C_BLACK : col);
        else {
            int32_t c = x0 + w0 / 2, w = clampi(v, -100, 100) * (w0 / 2) / 100;
            cv_rect(w >= 0 ? c : c + w, y0, w >= 0 ? w : -w, 7, touched ? C_BLACK : col);
            cv_rect(c - 1, y0 - 2, 2, 11, touched ? C_BLACK : C_WHITE);
        }
    }
    if (val[0]) {
        const font_t *f = text_w(&FONT_S, val) <= 228 ? &FONT_S : &FONT_XS;
        cv_text(6, 47 - f->h - 1, f, val, fg);
    }
    cv_rect(0, 46, 240, 1, RGB(50, 50, 50));
    cv_blit(0, 52 + 47 * j);
}
static void draw_mod_page(void)
{
    uint32_t sl = ui.sel, ix = ui.mod_ix;
    int32_t t = (int32_t)(ui.mod_touch_until - fm1_ms) > 0 ? ui.mod_touched : -1;
    const mod_t *m = &slot_mod[sl];
    uint16_t col = sl < NGEN ? GEN_COL[sl] : C_WHITE;
    char v[24];
    ui.mod_drawn_at = fm1_ms;
    if (ui.force) {
        char ti[12] = "1 LFO 1";
        ti[0] = (char)('1' + sl);
        if (ix == 1u)
            ti[6] = '2';
        else if (ix >= 2u) {
            str_cpy(ti + 2, "MOD A", sizeof ti - 2);
            ti[6] = (char)('A' + ix - 2u);
        }
        cv_begin(240, 52, C_BLACK);
        cv_text_fit(3, &FONT_L, ti, col);        /* (in the slot's colour: the chosen slot's modulation) */
        cv_blit(0, 0);
    }
    if (ix < 2u) {
        const mlfo_t *f = &m->lfo[ix];
        uint32_t ch = (uint32_t)(((uint64_t)100u * exp2_q16(f->rate * 7189)) >> 16);   /* (0.01 Hz units, x 100) */
        if (!ui.force)
            return;
        draw_mod_row(0, "SHAPE", MS_NAME[f->shape % MS_N], 0, 0, t == 0, col);
        fmt_fix(v, (int32_t)(ch < 100000u ? ch / 100u : ch / 1000u), ch < 100000u ? 2 : 1);
        str_cpy(v + str_len(v), " HZ", 4);
        draw_mod_row(1, "RATE", v, 1, f->rate, t == 1, col);
        draw_mod_row(2, "MODE", MM_NAME[f->mode % MM_N], 0, 0, t == 2, col);
        if (f->fade) {
            fmt_fix(v, f->fade, 1);
            str_cpy(v + str_len(v), " S", 3);
        } else
            str_cpy(v, "OFF", 4);
        draw_mod_row(3, "FADE", v, 1, f->fade, t == 3, col);
        return;
    }
    {
        const mslot_t *q = &m->s[ix - 2u];
        if (ui.force) {
            mod_dst_name(sl, q->dst, v, sizeof v);
            draw_mod_row(0, "FROM", MSRC_NAME[q->src % MSRC_N], 0, 0, t == 0, col);
            draw_mod_row(1, "TO", v, 0, 0, t == 1, col);
            if (q->amt > 0) {
                v[0] = '+';
                fmt_int(v + 1, q->amt);
            } else
                fmt_int(v, q->amt);
            draw_mod_row(2, "AMOUNT", v, 2, q->amt, t == 2, col);
        }
        draw_mod_row(3, "NOW", "", 2, mod_now(sl, q) * 100 / Q15, 0, col);
    }
}

static void ui_draw(void)
{
    uint32_t l, n = 0, mods[NGEN], all;
    if (draw_flash())
        return;
    if (ui.ses_page || ui.snap_page) {
        uint8_t id = ui.ses_page ? 8u : 9u;
        if (ui.shown_page != id) {
            ui.force = 1;
        }
        ui.shown_page = id;
        if (ui.ses_page)
            draw_ses_page();
        else
            draw_snap_page();
        ui.force = 0;
        return;
    }
    if (ui.fx_page == 3u) {                      /* MOD (SEQ's third) */
        if (ui.shown_page != 7u) {
            ui.force = 1;
        }
        ui.shown_page = 7;
        if (ui.force || (uint32_t)(fm1_ms - ui.mod_drawn_at) >= 40u)
            draw_mod_page();
        ui.force = 0;
        return;
    }
    if (ui.ovd) {
        if (ui.shown_page != 2u) {
            ui.force = 1;
        }
        ui.shown_page = 2;
        draw_ovd_page();
        ui.force = 0;
        return;
    }
    if (ui.fx_page) {
        if (ui.shown_page != 2u + ui.fx_page) {
            ui.force = 1;
        }
        ui.shown_page = (uint8_t)(2u + ui.fx_page);
        draw_fx_page();
        ui.force = 0;
        return;
    }
    if (ui.sc_page) {
        if (ui.shown_page != 1u)
            ui.force = 1;
        ui.shown_page = 1;
        draw_sc_page();
        ui.force = 0;
        return;
    }
    if (ui.shown_page != 0u) {                   /* back to playing: only the gaps between the parts painted (each
                                                  * part paints itself whole: no clear first) */
        ui.force = 1;
        lcd_fill(0, TOP_H, 240, GEN_Y - TOP_H, C_BLACK);
        lcd_fill(0, GEN_Y + GEN_H, 240, FXR_Y - GEN_Y - GEN_H, C_BLACK);
        lcd_fill(0, FXR_Y + NFX * FXR_H, 240, 240 - FXR_Y - NFX * FXR_H, C_BLACK);
        for (l = 0; l <= NGEN; l++)              /* (between the generators' blocks, and at the edges) */
            lcd_fill(l * GEN_P >= 1u ? l * GEN_P - 1u : 0u, GEN_Y, l * GEN_P >= 1u && l < NGEN ? 2u : 1u, GEN_H, C_BLACK);
    }
    ui.shown_page = 0;
    all = ui.force;                              /* (forced: every part owed, the tiles all in this frame; else the
                                                  * tiles that changed, six a frame) */
    if (ui.force)
        for (l = 0; l < NINST; l++)
            ui.shown_tile[l] = -1;
    draw_top();
    for (l = 0; l < NGEN; l++) {
        int32_t k = gen_key(l);
        mods[l] = gen_mods(l);
        if (ui.force || k != ui.shown_gen[l]) {
            draw_gen(l, k);
            ui.shown_gen[l] = k;
        }
    }
    for (l = 0; l < NFX; l++) {
        int32_t k = row_key(l);
        if (ui.force || k != ui.shown_row[l]) {   /* (a row changed: its frame, its gaps, its tiles at once) */
            uint32_t j;
            row_clear(l);
            draw_row(l, k);
            all = 1;
            ui.shown_row[l] = k;
            for (j = 0; j < FX_N; j++)
                ui.shown_tile[l * FX_N + j] = -1;
        }
    }
    for (l = 0; l < NINST; l++) {
        int32_t k = tile_key(l, mods);
        if (k != ui.shown_tile[l] && (all || n < 6u)) {   /* (a whole redraw: every tile at once) */
            draw_tile(l, k);
            ui.shown_tile[l] = k;
            n++;
        }
    }
    ui.force = 0;
}

static void ui_init(void)
{
    uint32_t k;
    memset(&ui, 0, sizeof ui);
    for (k = 0; k < NKEY; k++)
        ui.key_note[k] = KEY_NONE;
    ui.scale = 1;                                /* natural minor, C */
    rack_root = SCALE_BASE;
    ui.seed = 0x4D4F4Cu ^ fm1_ticks();
    ui.sc_touched = -1;
    ui.shown_top = -0x7FFFFFFF;
    ui.force = 1;
    ui.was_on = (uint8_t)audio_worker_online;
    for (k = 0; k < NGEN; k++) {                 /* the master's sends and settings (Tonitor's defaults) */
        ui.rsend[k] = RSEND_DEF;
        ui.dsend[k] = 0;
        sends_apply(k);
    }
    for (k = 0; k < 4u; k++) {
        ui.rvp[k] = RV_DEF[k];
        ui.dlp[k] = DL_DEF[k];
    }
    rv_param(&mrv, ui.rvp[0], ui.rvp[1], ui.rvp[2], ui.rvp[3]);
    mdl_param(ui.dlp[0], ui.dlp[1], ui.dlp[2], ui.dlp[3]);
    ui.fx_touched = -1;
}
