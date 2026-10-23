/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR: the panel and the screen.
 *
 *   FX SEL ENV LFO EDIT GLO   the top row: slots 1 to 6. Pressed: the slot chosen. Held 0.8 s: the slot on, or
 *                             off again (only slot 1 is on at the start)
 *   HOME                      a new patch for the slot chosen: its sound, the modules it is wired into, its
 *                             modulation by another slot, its level and pan. SHIFT + HOME: a new one of the same
 *                             family. HOME held 0.8 s: a new patch for every slot that is on
 *   SAVE                      a new patch for the rack: every module's values, and how the modules are wired to
 *                             each other (the slots' wiring into the rack is kept)
 *   the white keys            play the slot chosen, in the scale chosen: the first key is the root, every white key
 *                             the next note of the scale. Six voices for the six slots. SHIFT + a key: its note
 *                             latched on the slot (the key lit while the slot is chosen); SHIFT + it again: let go
 *   ARP                       the SCALE page (its light: on it): SELECT the scale, ALGORITHM the root
 *   SELECT, ALGORITHM         the chosen slot's level and pan (the SCALE page aside)
 *   OCT- / OCT+               the octave, -3 .. +3
 *   SEQ, PLAY, REC            nothing yet
 *   USB MIDI notes            as the keys, on the slot chosen
 *   MASTER (knob)             the level
 *
 * The screen is for low vision: a few words, large, high contrast. At the top, the audio's load at the left and the
 * battery at the right (or a message, black on white, across). Under it the six slots, each a block in its own
 * colour: filled when on, an outline when off, framed in white when chosen, brighter as it sounds; a small square in
 * another slot's colour in its corner when that slot modulates it. Under them the rack: twelve tiles, three across,
 * four down, each with its name. No cables: round each tile, a border in the colours of the slots wired into it, the
 * border shared out equally between them; the tiles the chosen slot is wired into filled with its colour, their names
 * white, the others dark, their names grey; a tile brightens as sound passes through it. The left column sounds at
 * the left, the right column at the right. How the modules are wired to each other is not drawn. */

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

/* the slots' colours (not Tonitor's: a set of their own) */
static const uint16_t SLOT_COL[NSLOT] = {RGB(255, 72, 72), RGB(72, 150, 255), RGB(170, 255, 60), RGB(190, 110, 255),
                                         RGB(255, 150, 200), RGB(235, 185, 60)};
#define C_SCALE RGB(255, 196, 90)
#define C_OK RGB(60, 220, 90)
static const char *const RM_TILE[NMOD] = {"FLT", "FLD", "MU", "SHF", "MDL", "STR", "VOX", "DLY", "TAP", "RVS", "SPR", "PLT"};

/* the screen's layout */
#define TOP_H 40
#define SLOT_Y 42
#define SLOT_H 46
#define SLOT_P 40                /* a slot's pitch across */
#define RACK_Y 90
#define RACK_COLS 3
#define TILE_P 80                /* a tile's pitch across */
#define TILE_H 37                /* .. and down */

static struct {
    uint8_t sel;
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
    int32_t shown_slot[NSLOT], shown_tile[NMOD];
    int8_t shown_sc_touch;
    uint8_t shown_scale, shown_root;
    uint32_t load_at;
    uint8_t load_shown;
    uint8_t midi_note[128], midi_slot[128];
    uint32_t latch[NSLOT];       /* each slot's keys latched (lit while it is chosen) */
    uint8_t latch_note[NSLOT][NKEY];
} ui;
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

static uint32_t next_seed(void)
{
    ui.seed = ui.seed * 1664525u + 1013904223u + fm1_ticks();
    return ui.seed;
}
/* HOME on slot l (same: of the same family) */
static void new_patch(uint32_t l, int same)
{
    int32_t f = same && slot_drawn[l] ? (int32_t)sbuf[l][s_use[l]].gen.family : -1;
    if (slot_draw(l, f, next_seed())) {
        ui.home_until = fm1_ms + 400u;
        if (!slot_on[l])
            say_n("SLOT ", (int32_t)l + 1, " OFF", 1200);
    } else
        say("BUSY", 600);
}
/* a slot on or off (a long press on its button) */
static void slot_toggle(uint32_t l)
{
    if (slot_on[l]) {
        slot_on[l] = 0;
        ui.latch[l] = 0;                         /* (its latched notes go with it) */
        ev_post(EV_SLOT_OFF, l, 0);
        say_n("SLOT ", (int32_t)l + 1, " OFF", 1200);
    } else {
        if (!slot_drawn[l])
            slot_draw(l, -1, next_seed());
        slot_on[l] = 1;
        say_n("SLOT ", (int32_t)l + 1, " ON", 1200);
    }
}

/* USB MIDI in: notes as the keys, on the slot chosen (any channel) */
static void midi_take(void)
{
    while (mi_r != mi_w) {
        uint32_t pkt = midi_in_q[mi_r % MQ], cin = pkt & 0xFu, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        RING_PUBLISH();
        mi_r++;
        if (cin == 9u && d2) {
            ev_post(EV_ON, ui.sel, d1);
            ui.midi_note[d1] = 1;
            ui.midi_slot[d1] = ui.sel;
        } else if (cin == 8u || (cin == 9u && !d2)) {
            if (ui.midi_note[d1]) {
                ev_post(EV_OFF, ui.midi_slot[d1], d1);
                ui.midi_note[d1] = 0;
            }
        }
    }
}

static void ui_input(void)
{
    uint32_t rel, press = fm1_input_edges(&rel), keys, ch, k, l;
    int32_t s;
    /* the slots' buttons: pressed, chosen; held, on or off */
    for (l = 0; l < NSLOT; l++) {
        if (btn_bit(press, SLOT_BTN[l])) {
            ui.sel = (uint8_t)l;
            ui.btn_down[l] = 1;
            ui.btn_done[l] = 0;
            ui.btn_t0[l] = fm1_ms;
            if (ui.sc_page) {
                ui.sc_page = 0;
                ui.force = 1;
            }
        }
        if (ui.btn_down[l] && !btn_bit(fm1_in.buttons, SLOT_BTN[l]))
            ui.btn_down[l] = 0;
        if (ui.btn_down[l] && !ui.btn_done[l] && (uint32_t)(fm1_ms - ui.btn_t0[l]) >= HOLD_MS) {
            ui.btn_done[l] = 1;
            slot_toggle(l);
        }
    }
    /* HOME: let go soon, a new patch for the slot chosen (SHIFT: of the same family); held, for every slot that is on */
    if (btn_bit(press, B_HOME)) {
        ui.home_down = 1;
        ui.home_done = 0;
        ui.home_t0 = fm1_ms;
        ui.home_shift = (uint8_t)shift_held();
    }
    if (ui.home_down && !ui.home_done && (uint32_t)(fm1_ms - ui.home_t0) >= HOLD_MS) {
        ui.home_done = 1;
        for (l = 0; l < NSLOT; l++)
            if (slot_on[l])
                new_patch(l, 0);
        say("ALL NEW", 1000);
    }
    if (ui.home_down && !btn_bit(fm1_in.buttons, B_HOME)) {
        ui.home_down = 0;
        if (!ui.home_done)
            new_patch(ui.sel, ui.home_shift);
    }
    if (btn_bit(press, B_SAVE)) {
        if (rack_draw(next_seed()))
            say("NEW RACK", 800);
        else
            say("BUSY", 600);
    }
    if (btn_bit(press, B_ARP)) {
        ui.sc_page ^= 1u;
        ui.force = 1;
    }
    if (btn_bit(press, B_OCTDN) && ui.oct > -3)
        ui.oct--;
    if (btn_bit(press, B_OCTUP) && ui.oct < 3)
        ui.oct++;
    if (ui.sc_page) {                            /* the SCALE page: SELECT the scale, ALGORITHM the root */
        s = fm1_enc_take(PANEL_ENC[EN_SELECT]);
        if (s) {
            ui.scale = (uint8_t)clampi((int32_t)ui.scale + s, 0, NSCALE - 1);
            ui.sc_touched = 1;
            ui.sc_touch_until = fm1_ms + 700u;
        }
        s = fm1_enc_take(PANEL_ENC[EN_ALGO]);
        if (s) {
            ui.root = (uint8_t)(((int32_t)ui.root + s + 1200) % 12);
            rack_root = SCALE_BASE + ui.root;
            ui.sc_touched = 0;
            ui.sc_touch_until = fm1_ms + 700u;
        }
    } else {                                     /* SELECT the chosen slot's level, ALGORITHM its pan */
        s = fm1_enc_take(PANEL_ENC[EN_SELECT]);
        if (s) {
            slot_lvl[ui.sel] = (int8_t)clampi(slot_lvl[ui.sel] + s, 0, LEVEL_MAX);
            if (slot_lvl[ui.sel])
                say_n("LEVEL ", slot_lvl[ui.sel], "", 800);
            else
                say("LEVEL OFF", 800);
        }
        s = fm1_enc_take(PANEL_ENC[EN_ALGO]);
        if (s) {
            int32_t p = clampi(slot_pan[ui.sel] + s, -PAN_MAX, PAN_MAX);
            slot_pan[ui.sel] = (int8_t)p;
            if (!p)
                say("PAN C", 800);
            else
                say_n(p < 0 ? "PAN L" : "PAN R", p < 0 ? -p : p, "", 800);
        }
    }
    for (k = 0; k < NE; k++)                     /* (the other knobs: nothing yet; their steps dropped) */
        fm1_enc_take(PANEL_ENC[k]);
    /* the keys: the slot chosen; a key lifted ends its note on the slot it was played on */
    keys = fm1_in.notes;
    (void)fm1_input_note_edges();
    ch = keys ^ ui.keys;
    for (k = 0; ch && k < NKEY; k++) {
        if (!((ch >> k) & 1u))
            continue;
        if (white_ix(k) < 0)
            continue;                            /* the black keys: no notes */
        if ((keys >> k) & 1u) {
            uint32_t note = scale_note(white_ix(k), ui.oct), bit = 1u << k;
            ui.key_note[k] = KEY_NONE;
            if (!slot_on[ui.sel]) {
                say_n("SLOT ", (int32_t)ui.sel + 1, " OFF", 800);
                continue;
            }
            if (ui.latch[ui.sel] & bit) {        /* a latched key: SHIFT lets it go; alone, nothing */
                if (shift_held()) {
                    ui.latch[ui.sel] &= ~bit;
                    ev_post(EV_OFF, ui.sel, ui.latch_note[ui.sel][k]);
                }
                continue;
            }
            if (shift_held()) {                  /* SHIFT and a key: latched */
                ui.latch[ui.sel] |= bit;
                ui.latch_note[ui.sel][k] = (uint8_t)note;
                ev_post(EV_LATCH, ui.sel, note);
                continue;
            }
            ui.key_note[k] = (uint8_t)note;
            ui.key_slot[k] = ui.sel;
            ev_post(EV_ON, ui.sel, note);
        } else if (ui.key_note[k] != KEY_NONE) {
            ev_post(EV_OFF, ui.key_slot[k], ui.key_note[k]);
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
    for (k = 0; k < NSLOT; k++)                  /* the chosen slot lit; one that is on but not chosen, blinking slowly */
        fm1_led_key(PANEL_BTN[SLOT_BTN[k]], k == ui.sel || (slot_on[k] && (fm1_ms / 600u) % 4u == 0u));
    fm1_led_key(PANEL_BTN[B_HOME], (int32_t)(ui.home_until - fm1_ms) > 0);
    fm1_led_key(PANEL_BTN[B_ARP], ui.sc_page);
    fm1_led_key(PANEL_BTN[B_OCTDN], ui.oct < 0);
    fm1_led_key(PANEL_BTN[B_OCTUP], ui.oct > 0);
    for (k = 0; k < NKEY; k++)                   /* a key held, or latched on the slot chosen */
        fm1_led_key(14u + k, white_ix(k) >= 0 && (((ui.keys >> k) & 1u) || ((ui.latch[ui.sel] >> k) & 1u)));
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
/* a slot's block: filled in its colour when on (brighter as it sounds), an outline when off, framed in white when
 * chosen; its number; the colour of the slot that modulates it, in a corner */
static void draw_slot(uint32_t l, int32_t key)
{
    const slot_patch_t *p = &sbuf[l][s_use[l]];
    uint16_t c = SLOT_COL[l], fg;
    int32_t st = key & 3, w = SLOT_P - 2;
    char d[2] = {(char)('1' + l), 0};
    cv_begin((uint32_t)w, SLOT_H, C_BLACK);
    if (slot_on[l]) {
        cv_rect(3, 3, w - 6, SLOT_H - 6, col_scale(c, 184 + st * 24));
        fg = C_BLACK;
        cv_bg_set(col_scale(c, 184 + st * 24));
    } else {
        cv_frame(5, 5, w - 10, SLOT_H - 10, 3, c);
        fg = c;
    }
    if (l == ui.sel)
        cv_frame(0, 0, w, SLOT_H, 3, C_WHITE);
    cv_text((w - text_w(&FONT_S, d)) / 2, (SLOT_H - FONT_S.h) / 2, &FONT_S, d, fg);
    if (slot_on[l] && p->xsrc < NSLOT && slot_on[p->xsrc]) {   /* modulated by another slot */
        cv_rect(w - 15, 3, 12, 12, C_BLACK);
        cv_rect(w - 13, 5, 8, 8, SLOT_COL[p->xsrc]);
    }
    cv_blit((uint32_t)(l * SLOT_P + 1u), SLOT_Y);
}
static int32_t slot_key(uint32_t l)
{
    const slot_patch_t *p = &sbuf[l][s_use[l]];
    int32_t xm = slot_on[l] && p->xsrc < NSLOT && slot_on[p->xsrc] ? p->xsrc + 1 : 0;
    return level_step(slot_on[l] ? slot_meter[l] : 0) | slot_on[l] << 2 | (l == ui.sel) << 3 | xm << 4 |
           (int32_t)(p->serial & 0xFFFFu) << 8;
}
/* a border t pixels thick round a w x h box, shared out between n colours: the perimeter walked clockwise from the
 * top left, each colour its equal stretch of it */
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
/* a module's tile: round it, a border in the colours of the slots wired into it (shared out equally); filled with the
 * chosen slot's colour, its name white, when the chosen slot is wired into it, else dark, its name grey; brighter as
 * sound passes through it */
static void draw_tile(uint32_t m, int32_t key)
{
    int32_t st = key & 3, feed = (key >> 2) & 63, hi = (key >> 8) & 1, w = TILE_P - 2, h = TILE_H - 2, k, n = 0;
    static const uint8_t GREY[4] = {18, 40, 66, 96};
    uint16_t col[NSLOT], bg, fg;
    if (hi) {
        bg = col_scale(SLOT_COL[ui.sel], 60 + st * 24);
        fg = C_WHITE;
    } else {
        bg = RGB(GREY[st], GREY[st], GREY[st]);
        fg = feed ? RGB(150, 150, 150) : RGB(80, 80, 80);
    }
    cv_begin((uint32_t)w, (uint32_t)h, bg);
    for (k = 0; k < NSLOT; k++)
        if ((feed >> k) & 1)
            col[n++] = SLOT_COL[k];
    if (n)
        cv_border_shared(w, h, 4, col, n);
    cv_bg_set(bg);
    cv_text((w - text_w(&FONT_S, RM_TILE[m])) / 2, (h - FONT_S.h) / 2 + 1, &FONT_S, RM_TILE[m], fg);
    cv_blit((m % RACK_COLS) * TILE_P + 1u, RACK_Y + (m / RACK_COLS) * TILE_H);
}
static int32_t tile_key(uint32_t m, const uint32_t *mods)
{
    int32_t feed = 0, k;
    for (k = 0; k < NSLOT; k++)
        if (slot_on[k] && ((mods[k] >> m) & 1u))
            feed |= 1 << k;
    return level_step(rm_meter[m]) | feed << 2 | (int32_t)(slot_on[ui.sel] && ((mods[ui.sel] >> m) & 1u)) << 8 |
           (int32_t)ui.sel << 9;
}
static void ui_draw(void)
{
    uint32_t l, m, n = 0, mods[NSLOT];
    if (ui.sc_page) {
        if (ui.shown_page != 1u)
            ui.force = 1;
        ui.shown_page = 1;
        draw_sc_page();
        ui.force = 0;
        return;
    }
    if (ui.shown_page != 0u) {
        ui.force = 1;
        lcd_fill(0, 0, 240, 240, C_BLACK);
    }
    ui.shown_page = 0;
    if (ui.force)                                /* (forced: every tile owed, drawn six a frame) */
        for (m = 0; m < NMOD; m++)
            ui.shown_tile[m] = -1;
    draw_top();
    for (l = 0; l < NSLOT; l++) {
        int32_t k = slot_key(l);
        mods[l] = slot_mods(l);
        if (ui.force || k != ui.shown_slot[l]) {
            draw_slot(l, k);
            ui.shown_slot[l] = k;
        }
    }
    for (m = 0; m < NMOD; m++) {                 /* (at most six tiles a frame: the rest the next) */
        int32_t k = tile_key(m, mods);
        if (k != ui.shown_tile[m] && n < 6u) {
            draw_tile(m, k);
            ui.shown_tile[m] = k;
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
}
