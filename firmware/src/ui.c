/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR: the panel and the screen, the first working version.
 *
 *   HOME                      a new patch: a generator's sound, its place, a chain of processors (patch.c)
 *   the white keys            play the patch, in the scale chosen: the first key is the root, every white key
 *                             the next note of the scale, up across the keyboard. Six voices
 *   ARP                       the SCALE page (its light: on it): SELECT the scale, ALGORITHM the root
 *   OCT- / OCT+               the octave, -3 .. +3 (their lights: below or above the middle)
 *   SAVE, SEQ                 kept for the sessions and the patterns (nothing yet)
 *   PLAY, REC, the top row    nothing yet
 *   USB MIDI notes            as the keys
 *   MASTER (knob)             the level
 *
 * The screen is for low vision: a few words, very large, high contrast. The patch view: the sound's family at
 * the top, large, in the family's colour; the network under it -- the generator a disc in that colour where
 * the patch put it (across: its place in the stereo image; down: its level, the top loudest), the processors
 * along the way to the output at the bottom right, joined by lines; along the bottom, the chain's names. A
 * short message in black on white when something changes. The octave at the top right. */

/* the panel: which matrix button carries which printed label (as SLOOP's panel.c, default table) */
enum { B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC, B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };
static const uint8_t PANEL_BTN[NB] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1};
static const uint8_t PANEL_ENC[NE] = {0, 1, 6, 2, 3, 4, 5};
#define NKEY 27u
#define KEY_SHIFT 25u            /* the rightmost black key */
#define KEY_NONE 0xFFu

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

/* the families' colours (MF_*: the bodies warm to cold, the clouds pale) */
static const uint16_t MF_COL[MF_N] = {RGB(255, 214, 0), RGB(0, 214, 255), RGB(255, 92, 60), RGB(255, 140, 0),
                                      RGB(255, 92, 210), RGB(96, 255, 96), RGB(190, 160, 255), RGB(255, 170, 120),
                                      RGB(170, 220, 255), RGB(255, 120, 160), RGB(200, 255, 170), RGB(230, 230, 230)};
#define C_NODE RGB(200, 200, 200)
#define C_LINE RGB(90, 90, 90)
#define C_SCALE RGB(255, 196, 90)

static struct {
    int8_t oct;
    uint8_t key_note[NKEY];      /* what a key down holds (KEY_NONE: nothing) */
    uint32_t keys;               /* the keys down last time */
    char msg[16];
    uint32_t msg_until;
    uint32_t home_until;         /* HOME's light after a new patch */
    uint32_t seed;
    uint8_t sc_page;             /* the SCALE page is up */
    uint8_t scale, root;         /* the keyboard's scale and root (0 = C .. 11 = B) */
    int8_t sc_touched;           /* 0 the root just turned, 1 the scale, -1 none (until sc_touch_until) */
    uint32_t sc_touch_until;
    uint8_t force;               /* the screen drawn whole */
    /* what the screen shows (a band is drawn again when what it shows changed) */
    uint32_t shown_serial;
    int8_t shown_oct, shown_sc_touch;
    uint8_t shown_scale, shown_root, shown_msg, shown_page, shown_load;
    uint8_t midi_note[128];      /* each MIDI note held */
} ui;
static int32_t batt_raw;                         /* the battery's divider, smoothed (main.c), 0 .. 1023; 0 none yet */

static void say(const char *s, uint32_t ms)
{
    str_cpy(ui.msg, s, sizeof ui.msg);
    ui.msg_until = fm1_ms + ms;
}
static int btn_bit(uint32_t mask, uint32_t label) { return (int)((mask >> PANEL_BTN[label]) & 1u); }

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

/* HOME: a new patch */
static void new_patch(void)
{
    ui.seed = ui.seed * 1664525u + 1013904223u + fm1_ticks();
    if (patch_new(ui.seed)) {
        ui.home_until = fm1_ms + 400u;
        ui.force = 1;
    } else
        say("BUSY", 600);
}

/* USB MIDI in: notes as the keys (any channel) */
static void midi_take(void)
{
    while (mi_r != mi_w) {
        uint32_t pkt = midi_in_q[mi_r % MQ], cin = pkt & 0xFu, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        RING_PUBLISH();
        mi_r++;
        if (cin == 9u && d2) {
            ev_post(EV_ON, d1);
            ui.midi_note[d1] = 1;
        } else if (cin == 8u || (cin == 9u && !d2)) {
            if (ui.midi_note[d1]) {
                ev_post(EV_OFF, d1);
                ui.midi_note[d1] = 0;
            }
        }
    }
}

static void ui_input(void)
{
    uint32_t rel, press = fm1_input_edges(&rel), keys, ch, k;
    int32_t s;
    (void)rel;
    if (btn_bit(press, B_HOME))
        new_patch();
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
            ui.sc_touched = 0;
            ui.sc_touch_until = fm1_ms + 700u;
        }
    }
    for (k = 0; k < NE; k++)                     /* (the other knobs: nothing yet; their steps dropped) */
        fm1_enc_take(PANEL_ENC[k]);
    /* the keys */
    keys = fm1_in.notes;
    (void)fm1_input_note_edges();
    ch = keys ^ ui.keys;
    for (k = 0; ch && k < NKEY; k++) {
        if (!((ch >> k) & 1u))
            continue;
        if (white_ix(k) < 0)
            continue;                            /* the black keys: no notes */
        if ((keys >> k) & 1u) {
            uint32_t note = scale_note(white_ix(k), ui.oct);
            ui.key_note[k] = (uint8_t)note;
            ev_post(EV_ON, note);
        } else if (ui.key_note[k] != KEY_NONE) {
            ev_post(EV_OFF, ui.key_note[k]);
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
    fm1_led_key(PANEL_BTN[B_HOME], (int32_t)(ui.home_until - fm1_ms) > 0);
    fm1_led_key(PANEL_BTN[B_ARP], ui.sc_page);
    fm1_led_key(PANEL_BTN[B_OCTDN], ui.oct < 0);
    fm1_led_key(PANEL_BTN[B_OCTUP], ui.oct > 0);
    for (k = 0; k < NKEY; k++)
        fm1_led_key(14u + k, white_ix(k) >= 0 && ((ui.keys >> k) & 1u));
}

/* -------------------------------------------------------------- the screen --- */
/* the SCALE page: the title; the root, very large; the scale's name, one or two lines. What was just turned
 * is drawn black on white for a moment (Tonitor's) */
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

/* the patch view's geometry: the field the network is drawn in, and the output's corner */
#define NET_X 4
#define NET_Y 46
#define NET_W 232
#define NET_H 148
#define OUT_X 220
#define OUT_Y 184
#define GEN_R 17
#define NODE_R 12
static void net_gen_at(const patch_t *p, int32_t *x, int32_t *y)
{
    *x = NET_X + GEN_R + 2 + (int32_t)p->gx * (NET_W - 2 * GEN_R - 4) / 255;
    *y = NET_Y + GEN_R + 2 + (int32_t)p->gy * (NET_H - 2 * GEN_R - 4) / 255;
}
/* node i of n on the way from the generator to the output */
static void net_node_at(const patch_t *p, uint32_t i, int32_t *x, int32_t *y)
{
    int32_t gx, gy;
    net_gen_at(p, &gx, &gy);
    *x = gx + (OUT_X - gx) * (int32_t)(i + 1u) / (int32_t)(p->nnd + 1u);
    *y = gy + (OUT_Y - gy) * (int32_t)(i + 1u) / (int32_t)(p->nnd + 1u);
}
/* the top band: the family, the octave; or the message */
static void draw_top(const patch_t *p)
{
    int msg = (int32_t)(ui.msg_until - fm1_ms) > 0;
    if (!ui.force && ui.shown_msg == msg && ui.shown_oct == ui.oct && ui.shown_serial == p->serial)
        return;
    if (msg) {
        cv_begin(240, 46, C_WHITE);
        cv_text_fit(5, &FONT_M, ui.msg, C_BLACK);
    } else {
        char o[4];
        cv_begin(240, 46, C_BLACK);
        cv_text(6, 5, &FONT_M, MF_NAME[p->gen.family], MF_COL[p->gen.family]);
        if (ui.oct) {
            o[0] = ui.oct < 0 ? '-' : '+';
            o[1] = (char)('0' + (ui.oct < 0 ? -ui.oct : ui.oct));
            o[2] = 0;
            cv_text(240 - 6 - text_w(&FONT_S, o), 10, &FONT_S, o, C_GREY);
        }
    }
    cv_blit(0, 0);
    ui.shown_msg = (uint8_t)msg;
    ui.shown_oct = ui.oct;
}
/* the network */
static void draw_net(const patch_t *p)
{
    int32_t gx, gy, x0, y0, x1, y1;
    uint32_t i;
    if (!ui.force && ui.shown_serial == p->serial)
        return;
    CV_AREA(0, NET_Y, 240, NET_H, C_BLACK) {
        cv_frame(NET_X, NET_Y, NET_W, NET_H, 1, C_LINE);
        net_gen_at(p, &gx, &gy);
        x0 = gx;
        y0 = gy;
        for (i = 0; i < p->nnd; i++) {                   /* the lines first, the discs over them */
            net_node_at(p, i, &x1, &y1);
            cv_line(x0, y0, x1, y1, 3, C_LINE);
            x0 = x1;
            y0 = y1;
        }
        cv_line(x0, y0, OUT_X, OUT_Y, 3, C_LINE);
        cv_rect(OUT_X - 6, OUT_Y - 6, 12, 12, C_WHITE);  /* the output */
        for (i = 0; i < p->nnd; i++) {
            net_node_at(p, i, &x1, &y1);
            cv_disc(x1, y1, NODE_R, C_BLACK);
            cv_ring(x1, y1, NODE_R, 3, C_NODE);
        }
        cv_disc(gx, gy, GEN_R, MF_COL[p->gen.family]);
    }
}
/* the bottom band: the chain's names (and, for now, the audio's load at the right) */
static void draw_chain(const patch_t *p)
{
    char line[24];
    uint32_t i, n = 0;
    uint8_t load = (uint8_t)(load_pct / 10u);
    if (!ui.force && ui.shown_serial == p->serial && ui.shown_load == load)
        return;
    for (i = 0; i < p->nnd; i++) {
        const char *s = PK_SHORT[p->nd[i].kind];
        if (n)
            line[n++] = ' ';
        while (*s)
            line[n++] = *s++;
    }
    line[n] = 0;
    cv_begin(240, 46, C_BLACK);
    if (!p->nnd)
        cv_text(6, 10, &FONT_S, "DIRECT", C_GREY);
    else
        cv_text(6, 10, &FONT_S, line, C_WHITE);
    {
        char b[8];
        fmt_int(b, (int32_t)load_pct);
        n = str_len(b);
        b[n] = '%';
        b[n + 1] = 0;
        cv_text(240 - 4 - text_w(&FONT_XS, b), 24, &FONT_XS, b, C_GREY);
    }
    cv_blit(0, 194);
    ui.shown_load = load;
}
static void ui_draw(void)
{
    const patch_t *p = &pbuf[p_use];
    if (ui.sc_page) {
        if (ui.shown_page != 1u)
            ui.force = 1;
        ui.shown_page = 1;
        draw_sc_page();
        ui.force = 0;
        return;
    }
    if (ui.shown_page != 0u)
        ui.force = 1;
    ui.shown_page = 0;
    draw_top(p);
    draw_net(p);
    draw_chain(p);
    ui.shown_serial = p->serial;
    ui.force = 0;
}

static void ui_init(void)
{
    uint32_t k;
    memset(&ui, 0, sizeof ui);
    for (k = 0; k < NKEY; k++)
        ui.key_note[k] = KEY_NONE;
    ui.scale = 1;                                /* natural minor, C */
    ui.seed = 0x4D4F4Cu ^ fm1_ticks();
    ui.sc_touched = -1;
    ui.shown_oct = 99;
    ui.force = 1;
}
