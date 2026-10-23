/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR host harness: the engine compiled for the computer.
 *
 *   host render DIR [SEEDS] [PATH]   every family, SEEDS sounds each (default 3), on a path (0 open, 1 ambient,
 *                                    2 experimental; default 0): a note held 4 s and released, a WAV each, and a
 *                                    line of figures (gain, peak, level, brightness, length, cost)
 *   host one DIR FAMILY SEED [PATH]  one sound, with its voice printed
 *   host check                       every sound of 40 seeds per family and path: none silent, none clipping
 *                                    hard, none running away, the macros at their ends too; exits 1 on a failure
 *   host golden                      a hash of every sound of 8 seeds per family and path (tests/golden)
 *   host bench                       the cost of a block, ns, per family (the host's; the chip's is measured there)
 *
 * Build: tests/run_tests.sh (or see the clang line there). */
#define HOST 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "molecular_tables.h"

#define NELEM(a) (sizeof(a) / sizeof((a)[0]))

/* the firmware above the HAL: what main.c and the HAL give it, as the host has them */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
static volatile uint32_t fm1_ms;
static uint32_t host_ticks;
static uint32_t fm1_ticks(void) { return host_ticks += 24000u; }
static void fm1_wdt_feed(void) { fm1_ms++; }   /* (the host: time passes while the main loop waits) */
static unsigned str_len(const char *s) { return (unsigned)strlen(s); }
static void str_cpy(char *d, const char *s, unsigned max)
{
    unsigned i = 0;
    while (s[i] && i + 1 < max) {
        d[i] = s[i];
        i++;
    }
    d[i] = 0;
}
static void fmt_fix(char *b, int32_t v, int decimals)
{
    char t[12];
    int n = 0, neg = v < 0, i;
    uint32_t u = (uint32_t)(neg ? -v : v);
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
        if (n == decimals)
            t[n++] = '.';
    } while (u || n <= decimals);
    if (neg)
        t[n++] = '-';
    for (i = 0; i < n; i++)
        b[i] = t[n - 1 - i];
    b[n] = 0;
}
static void fmt_int(char *b, int32_t v) { fmt_fix(b, v, 0); }
/* the screen: a framebuffer instead of the panel */
static uint16_t fb[240 * 240];
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u) {
                uint16_t c = px[j * w + i];
                fb[(y + j) * 240u + x + i] = (uint16_t)((c >> 8) | (c << 8));
            }
}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u)
                fb[(y + j) * 240u + x + i] = c;
}
static void screen_save(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint32_t i;
    if (!f)
        return;
    fprintf(f, "P6 240 240 255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = fb[i];
        uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
/* the panel, as the tests press it */
static struct { uint32_t notes, buttons, pressed; int32_t enc[7]; } fm1_in;
static uint32_t fm1_input_edges(uint32_t *rel)
{
    uint32_t p = fm1_in.pressed;
    fm1_in.pressed = 0;
    if (rel)
        *rel = 0;
    return p;
}
static uint32_t fm1_input_note_edges(void) { return 0; }
static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s = fm1_in.enc[e];
    fm1_in.enc[e] = 0;
    return s;
}
static void fm1_led_key(uint32_t id, int on) { (void)id; (void)on; }
/* usb.c's MIDI in ring, as the tests fill it */
#define MQ 64u
static uint32_t midi_in_q[MQ];
static volatile uint32_t mi_w, mi_r;

#include "dsp.h"
#include "gfx.c"
#include "wt.c"
#include "fm6_core.c"
#include "mol.c"
/* usb.c's state, as the screen reads it (no host: on battery) */
static struct { uint8_t config, suspended; } usb;
#include "rack.c"
#include "slots.c"
#include "ui.c"

static const char *const PATH_NAME[PATH_N] = {"OPEN", "AMBIENT", "EXP"};

/* a 16-bit mono WAV */
static void wav_write(const char *path, const int16_t *x, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t v;
    uint16_t w;
    if (!f)
        return;
    fwrite("RIFF", 1, 4, f);
    v = 36 + n * 2;
    fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    v = 16;
    fwrite(&v, 4, 1, f);
    w = 1;
    fwrite(&w, 2, 1, f);
    w = 1;
    fwrite(&w, 2, 1, f);
    v = FS;
    fwrite(&v, 4, 1, f);
    v = FS * 2;
    fwrite(&v, 4, 1, f);
    w = 2;
    fwrite(&w, 2, 1, f);
    w = 16;
    fwrite(&w, 2, 1, f);
    fwrite("data", 1, 4, f);
    v = n * 2;
    fwrite(&v, 4, 1, f);
    fwrite(x, 2, n, f);
    fclose(f);
}

static double now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e9 + t.tv_nsec;
}

static mo_voice_t voice;
static mo_design_t design;
static int16_t wav[FS * 12];

typedef struct {
    int32_t peak;                /* |x| max, Q15 */
    uint32_t over;               /* samples past 1.0 */
    uint32_t len;                /* samples until the voice ended (0: never ended within the time) */
    uint32_t blocks;             /* blocks rendered */
    double rms;                  /* over the held part */
    double ns_blk;               /* ns a block */
    int ended;                   /* the engine said so */
    int reaped;                  /* .. or the host's tail reaper would have: 66 dB under its loudest for 0.2 s after release */
} stat_t;

/* a note held `hold` s, released, followed `tail` s more or until the voice ends; the macros mac (or 0) */
static stat_t play(mo_design_t *d, int note, double hold, double tail, const int8_t *mac, int16_t *out)
{
    stat_t s;
    int32_t blk[CTL];
    uint32_t b, i, hb = (uint32_t)(hold * FS / CTL), tb = hb + (uint32_t)(tail * FS / CTL), n = 0;
    double acc = 0, t0;
    int32_t pk = 0, bpk;
    uint32_t quiet = 0;
    memset(&s, 0, sizeof s);
    memset(&voice, 0, sizeof voice - MO_GR_N);
    mo_note_on(&voice, d, note, 1);
    t0 = now_ns();
    for (b = 0; b < tb; b++) {
        int alive;
        if (b == hb)
            mo_note_off(&voice, d);
        alive = mo_render(&voice, d, blk, mac);
        s.blocks++;
        bpk = 0;
        for (i = 0; i < CTL; i++) {
            int32_t x = blk[i], a = x < 0 ? -x : x;
            if (a > s.peak)
                s.peak = a;
            if (a > bpk)
                bpk = a;
            if (a > 32767)
                s.over++;
            if (b < hb)
                acc += (double)x * x;
            if (out && n < FS * 12)
                out[n] = (int16_t)clampi(x, -32767, 32767);
            n++;
        }
        if (!alive) {
            s.ended = 1;
            s.len = n;
            break;
        }
        pk = bpk > pk ? bpk : pk;
        if (b >= hb && bpk < (pk >> 11))
            quiet++;
        else
            quiet = 0;
        if (quiet >= 276u) {                             /* (layers.c TAIL_FLOOR, TAIL_QUIET) */
            s.reaped = 1;
            s.len = n;
            break;
        }
    }
    s.ns_blk = (now_ns() - t0) / s.blocks;
    s.rms = hb ? acc / ((double)hb * CTL) : 0;
    s.rms = s.rms > 0 ? __builtin_sqrt(s.rms) / 32768.0 : 0;
    s.len = s.ended ? s.len : n;
    return s;
}

static void print_design(const mo_design_t *d)
{
    int n;
    printf("  %s (body %s) path %s seed %u alg %d fb %d gain %d\n", MF_NAME[d->family], MF_NAME[d->body],
           PATH_NAME[d->path], d->seed, d->p[FP_ALG] + 1, d->p[FP_FB], d->gain);
    for (n = 1; n <= 6; n++) {
        const uint8_t *op = d->p + (6 - n) * FP_OP;
        const fm6_wsel_t *w = &d->ws[6 - n];
        printf("  op%d %s %s ratio %d.%02d ol %2d env R %2d %2d %2d %2d L %2d %2d %2d %2d det %2d wave %2d morph %3d pd %3d%s\n", n,
               mo_is_carrier(d->p, n) ? "car" : "mod", op[FP_MODE] ? "fixed" : "ratio", mo_op_ratio100(op) / 100,
               mo_op_ratio100(op) % 100, op[FP_OL], op[FP_R1], op[FP_R1 + 1], op[FP_R1 + 2], op[FP_R1 + 3], op[FP_L1],
               op[FP_L1 + 1], op[FP_L1 + 2], op[FP_L1 + 3], op[FP_DET], w->wave, w->morph, w->pd,
               w->flags & FW_PDENV ? " (env)" : "");
    }
    printf("  lfo wave %d speed %d delay %d pmd %d amd %d pms %d; penv L %d %d %d %d R %d %d %d %d\n", d->p[FP_LFW],
           d->p[FP_LFS], d->p[FP_LFD], d->p[FP_LPMD], d->p[FP_LAMD], d->p[FP_LPMS], d->p[FP_PL1], d->p[FP_PL1 + 1],
           d->p[FP_PL1 + 2], d->p[FP_PL1 + 3], d->p[FP_PR1], d->p[FP_PR1 + 1], d->p[FP_PR1 + 2], d->p[FP_PR1 + 3]);
    if (d->gr.mode) {
        const mo_gr_t *g = &d->gr;
        printf("  grains %s size %d ms rate %d back %d spread %d iv %d %d %d %d pch %d jit %d cents %d rev %d shape %d fb %d mix %d burst %d freeze %d atk %d rel %d dark %d drift %d\n",
               g->mode == GM_PULSAR ? "PULSAR" : "CLOUD", g->size_ms, g->rate, g->back_ms, g->spread_ms, g->iv[0],
               g->iv[1], g->iv[2], g->iv[3], g->pch, g->jit, g->cents, g->rev, g->shape, g->fb, g->mix, g->burst,
               g->freeze_ms, g->atk_ms, g->rel_ms, g->dark, g->drift);
    }
}

static uint32_t fnv(uint32_t h, const void *p, uint32_t n)
{
    const uint8_t *b = p;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}

static int cmd_render(int argc, char **argv)
{
    const char *dir = argc > 2 ? argv[2] : ".";
    int seeds = argc > 3 ? atoi(argv[3]) : 3, path = argc > 4 ? atoi(argv[4]) : 0, f, s;
    char name[512];
    printf("%-8s %-8s seed  gain  bright  peak   rms   len(s)  ns/blk  grains\n", "family", "body");
    for (f = 0; f < MF_N; f++)
        for (s = 0; s < seeds; s++) {
            uint32_t seed = 1000u + (uint32_t)f * 101u + (uint32_t)s;
            int32_t bright;
            stat_t st;
            mo_design(&design, f, seed, (uint32_t)path, 0);
            bright = mo_measure(&design);
            st = play(&design, 60, 4.0, 8.0, 0, wav);
            snprintf(name, sizeof name, "%s/%s-%s-%u.wav", dir, MF_NAME[f], PATH_NAME[path], seed);
            wav_write(name, wav, st.len ? st.len : FS * 12);
            printf("%-8s %-8s %4u %5d %6d %6.2f %6.3f %6.2f %7.0f  %s\n", MF_NAME[f], MF_NAME[design.body], seed,
                   design.gain, bright, st.peak / 32768.0, st.rms, st.len / (double)FS, st.ns_blk,
                   design.gr.mode == GM_PULSAR ? "pulsar" : design.gr.mode ? "cloud" : "-");
        }
    return 0;
}

static int cmd_one(int argc, char **argv)
{
    const char *dir = argv[2];
    int f = -1, path = argc > 5 ? atoi(argv[5]) : 0, k;
    uint32_t seed = (uint32_t)atoi(argv[4]);
    char name[512];
    stat_t st;
    int32_t bright;
    for (k = 0; k < MF_N; k++)
        if (!strcmp(argv[3], MF_NAME[k]))
            f = k;
    mo_design(&design, f, seed, (uint32_t)path, 0);
    bright = mo_measure(&design);
    print_design(&design);
    st = play(&design, 60, 4.0, 8.0, 0, wav);
    snprintf(name, sizeof name, "%s/%s-%s-%u.wav", dir, MF_NAME[design.family], PATH_NAME[path], seed);
    wav_write(name, wav, st.len ? st.len : FS * 12);
    printf("  bright %d Hz peak %.2f rms %.3f len %.2f s %.0f ns/blk -> %s\n", bright, st.peak / 32768.0, st.rms,
           st.len / (double)FS, st.ns_blk, name);
    return 0;
}

static int cmd_check(void)
{
    static const int8_t MACS[9][4] = {{0, 0, 0, 0}, {64, 0, 0, 0}, {-64, 0, 0, 0}, {0, 64, 0, 0}, {0, -64, 0, 0},
                                      {0, 0, 64, 0}, {0, 0, -64, 0}, {0, 0, 0, 64}, {64, 64, 64, 64}};
    int f, s, path, fails = 0, n = 0, m;
    for (path = 0; path < PATH_N; path++)
        for (f = 0; f < MF_N; f++)
            for (s = 0; s < 40; s++) {
                uint32_t seed = 5000u + (uint32_t)f * 211u + (uint32_t)s * 7u + (uint32_t)path * 977u;
                stat_t st;
                mo_design(&design, f, seed, (uint32_t)path, 0);
                mo_measure(&design);
                st = play(&design, 60, 3.0, 30.0, 0, 0);
                n++;
                if (st.rms < 0.01) {
                    printf("FAIL %s %s seed %u: silent (rms %.4f)\n", MF_NAME[f], PATH_NAME[path], seed, st.rms);
                    fails++;
                }
                if (st.peak > 2 * 32768 || st.over > st.blocks * CTL / 50) {
                    printf("FAIL %s %s seed %u: clipping hard (peak %.2f, %u over)\n", MF_NAME[f], PATH_NAME[path], seed,
                           st.peak / 32768.0, st.over);
                    fails++;
                }
                if (!st.ended && !st.reaped && !design.gr.freeze_ms) {
                    printf("FAIL %s %s seed %u: runs away (still alive 30 s after release)\n", MF_NAME[f], PATH_NAME[path], seed);
                    fails++;
                }
                if (s < 4)                               /* the macros at their ends, on a few */
                    for (m = 1; m < 9; m++) {
                        st = play(&design, 60, 2.0, 12.0, MACS[m], 0);
                        if (st.peak > 3 * 32768) {
                            printf("FAIL %s %s seed %u macros %d %d %d %d: peak %.2f\n", MF_NAME[f], PATH_NAME[path], seed,
                                   MACS[m][0], MACS[m][1], MACS[m][2], MACS[m][3], st.peak / 32768.0);
                            fails++;
                        }
                    }
            }
    /* a note far up and far down: nothing breaks */
    for (f = 0; f < MF_N; f++) {
        stat_t st;
        mo_design(&design, f, 77u + (uint32_t)f, 0, 0);
        mo_measure(&design);
        st = play(&design, 108, 1.0, 6.0, 0, 0);
        if (st.peak > 3 * 32768)
            printf("FAIL %s at note 108: peak %.2f\n", MF_NAME[f], st.peak / 32768.0), fails++;
        st = play(&design, 24, 1.0, 6.0, 0, 0);
        if (st.peak > 3 * 32768)
            printf("FAIL %s at note 24: peak %.2f\n", MF_NAME[f], st.peak / 32768.0), fails++;
        n += 2;
    }
    printf("check: %d sounds, %d failures\n", n, fails);
    return fails ? 1 : 0;
}

static int cmd_golden(void)
{
    int f, s, path;
    int32_t blk[CTL];
    for (path = 0; path < PATH_N; path++)
        for (f = 0; f < MF_N; f++)
            for (s = 0; s < 8; s++) {
                uint32_t seed = 100u + (uint32_t)f * 31u + (uint32_t)s + (uint32_t)path * 503u, h, b;
                mo_design(&design, f, seed, (uint32_t)path, 0);
                mo_measure(&design);
                h = fnv(2166136261u, &design, sizeof design);
                memset(&voice, 0, sizeof voice - MO_GR_N);
                mo_note_on(&voice, &design, 60, 1);
                for (b = 0; b < 2756u; b++) {            /* 2 s */
                    if (b == 1378u)
                        mo_note_off(&voice, &design);
                    if (!mo_render(&voice, &design, blk, 0))
                        break;
                    h = fnv(h, blk, sizeof blk);
                }
                printf("%s %s %u %08x\n", MF_NAME[f], PATH_NAME[path], seed, h);
            }
    return 0;
}

static int cmd_bench(void)
{
    int f, s;
    printf("%-8s  ns/blk (held, 6 seeds)   no-grain-macro  grain-macro\n", "family");
    for (f = 0; f < MF_N; f++) {
        double t = 0, tg = 0;
        static const int8_t G[4] = {0, 0, 64, 0};
        for (s = 0; s < 6; s++) {
            stat_t st;
            mo_design(&design, f, 300u + (uint32_t)s, 0, 0);
            mo_measure(&design);
            st = play(&design, 60, 2.0, 0.0, 0, 0);
            t += st.ns_blk;
            st = play(&design, 60, 2.0, 0.0, G, 0);
            tg += st.ns_blk;
        }
        printf("%-8s  %8.0f  %8.0f\n", MF_NAME[f], t / 6, tg / 6);
    }
    return 0;
}

/* ---------------------------------------------------------- the firmware's --- */
/* a stereo WAV */
static void wav_write2(const char *path, const int16_t *x, uint32_t frames)
{
    FILE *f = fopen(path, "wb");
    uint32_t v;
    uint16_t w;
    if (!f)
        return;
    fwrite("RIFF", 1, 4, f);
    v = 36 + frames * 4;
    fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    v = 16;
    fwrite(&v, 4, 1, f);
    w = 1;
    fwrite(&w, 2, 1, f);
    w = 2;
    fwrite(&w, 2, 1, f);
    v = FS;
    fwrite(&v, 4, 1, f);
    v = FS * 4;
    fwrite(&v, 4, 1, f);
    w = 4;
    fwrite(&w, 2, 1, f);
    w = 16;
    fwrite(&w, 2, 1, f);
    fwrite("data", 1, 4, f);
    v = frames * 4;
    fwrite(&v, 4, 1, f);
    fwrite(x, 4, frames, f);
    fclose(f);
}
static int16_t wav2[FS * 12 * 2];
static void fw_init(void)
{
    slots_init();
    ui_init();
    slot_draw(0, -1, ui.seed);
    slot_on[0] = 1;
    master_q12 = 4096;
}
static void print_slot(uint32_t l)
{
    const slot_patch_t *p = &sbuf[l][s_pub[l]];
    uint32_t i;
    printf("slot %u%s: %s, %s (body %s, %s), lvl %d pan %d, dry %d%%:", l + 1, slot_on[l] ? "" : " (off)",
           SC_NAME[p->scene], MF_NAME[p->gen.family], MF_NAME[p->gen.body], PATH_NAME[p->gen.path], slot_lvl[l], slot_pan[l],
           p->dry * 100 / Q15);
    for (i = 0; i < p->nch; i++)
        printf(" %s", RM_NAME[p->chain[i]]);
    if (!p->nch)
        printf(" direct");
    if (p->xsrc < NSLOT)
        printf("; %s from slot %u, %d", XM_NAME[p->xkind], p->xsrc + 1, p->xdepth);
    printf("\n");
}
/* the keys: a few notes, held, let go; blocks run between; the output into wav2 */
static uint32_t fw_frames;
static int32_t fw_peak;
static double fw_rms;
static void fw_run(uint32_t blocks)
{
    int32_t out[2 * CTL];
    uint32_t b, i;
    for (b = 0; b < blocks; b++) {
        mix_block(out);
        for (i = 0; i < 2u * CTL; i++) {
            int32_t a = out[i] < 0 ? -out[i] : out[i];
            if (a > fw_peak)
                fw_peak = a;
            fw_rms += (double)out[i] * out[i];
            if (fw_frames * 2u + i < sizeof wav2 / sizeof wav2[0])
                wav2[fw_frames * 2u + i] = (int16_t)clampi(out[i], -32767, 32767);
        }
        fw_frames += CTL;
        {   /* (a block is 0.7256 ms) */
            static uint32_t frac;
            frac += 7256u;
            fm1_ms += frac / 10000u;
            frac %= 10000u;
        }
    }
}
static void key(uint32_t k, int down)
{
    if (down)
        fm1_in.notes |= 1u << k;
    else
        fm1_in.notes &= ~(1u << k);
    ui_input();
}
static void press(uint32_t label, uint32_t ms)       /* a button pressed, held ms, let go (the blocks run meanwhile) */
{
    fm1_in.pressed |= 1u << PANEL_BTN[label];
    fm1_in.buttons |= 1u << PANEL_BTN[label];
    ui_input();
    fw_run(ms * 1378u / 1000u + 1u);
    ui_input();
    fm1_in.buttons &= ~(1u << PANEL_BTN[label]);
    ui_input();
}
static void play_notes(uint32_t slot, double secs)    /* slot chosen; a chord held secs, let go, as long again */
{
    press(SLOT_BTN[slot], 50);
    key(0, 1);
    fw_run(400);
    key(4, 1);
    key(7, 1);
    fw_run((uint32_t)(secs * 1378) - 400);
    key(0, 0);
    key(4, 0);
    key(7, 0);
}
static int cmd_patches(int argc, char **argv)
{
    const char *dir = argc > 2 ? argv[2] : ".";
    int n = argc > 3 ? atoi(argv[3]) : 8, i, fails = 0, l;
    char name[512];
    for (i = 0; i < n; i++) {
        uint32_t seed = 777u + (uint32_t)i * 7919u, nsl = 1u + (uint32_t)i % 4u;
        double t0, ns;
        fw_init();
        ui.seed = seed;
        printf("patch %d:\n", i);
        for (l = 0; l < (int)nsl; l++) {                /* slots on, a patch each */
            if (l)
                press(SLOT_BTN[l], 900);
            press(SLOT_BTN[l], 50);
            press(B_HOME, 50);
            fw_run(4);
            print_slot((uint32_t)l);
        }
        press(B_SEQ, 50);
        fw_frames = 0;
        fw_peak = 0;
        fw_rms = 0;
        t0 = now_ns();
        for (l = 0; l < (int)nsl; l++) {                /* a chord on each, overlapping */
            press(SLOT_BTN[l], 50);
            key(0 + 2u * (uint32_t)l, 1);
            key(4 + 2u * (uint32_t)l, 1);
            fw_run(700);
        }
        fw_run(1378);
        for (l = 0; l < (int)NKEY; l++)
            key((uint32_t)l, 0);
        fw_run(1378 * 3);
        ns = (now_ns() - t0) / ((double)fw_frames / CTL);
        snprintf(name, sizeof name, "%s/patch-%02d.wav", dir, i);
        wav_write2(name, wav2, fw_frames);
        printf("  peak %.2f rms %.3f, %.0f ns a block\n", fw_peak / 32768.0, __builtin_sqrt(fw_rms / (fw_frames * 2.0)) / 32768.0, ns);
        if (fw_rms / (fw_frames * 2.0) < 100.0 * 100.0) {
            printf("FAIL patch %d: silent\n", i);
            fails++;
        }
        if (fw_peak > 32767) {
            printf("FAIL patch %d: past full scale (%d)\n", i, fw_peak);
            fails++;
        }
    }
    printf("patches: %d, %d failures\n", n, fails);
    return fails ? 1 : 0;
}
/* every module alone: a slot wired into it, a few notes; its level against the dry */
static int cmd_modules(int argc, char **argv)
{
    const char *dir = argc > 2 ? argv[2] : ".";
    uint32_t m, rep;
    int fails = 0;
    char name[512];
    for (m = 0; m < NMOD; m++) {
        double dry = 0, wet = 0;
        int32_t pk = 0;
        for (rep = 0; rep < 3u; rep++) {
            slot_patch_t *p;
            uint32_t b;
            fw_init();
            rack_draw(91u + rep * 17u + m);
            slot_draw(0, rep == 0 ? MF_IMPACT : rep == 1 ? MF_DRONE : MF_GLASS, 4242u + rep);
            fw_run(4);
            p = &sbuf[0][s_use[0]];
            p->nch = 1;
            p->chain[0] = (uint8_t)m;
            p->send = Q15;
            p->dry = 0;
            fw_frames = 0;
            fw_peak = 0;
            fw_rms = 0;
            key(0, 1);
            key(4, 1);
            fw_run(1378 * 2);
            key(0, 0);
            key(4, 0);
            fw_run(1378 * 2);
            wet += fw_rms / (fw_frames * 2.0);
            pk = maxi(pk, fw_peak);
            if (rep == 0) {
                snprintf(name, sizeof name, "%s/module-%s.wav", dir, RM_NAME[m]);
                wav_write2(name, wav2, fw_frames);
            }
            p->nch = 0;                                 /* (the same, dry, for the comparison) */
            p->dry = Q15;
            fw_frames = 0;
            fw_rms = 0;
            key(0, 1);
            key(4, 1);
            fw_run(1378 * 2);
            key(0, 0);
            key(4, 0);
            for (b = 0; b < 1378u * 2u; b += 64u)
                fw_run(64);
            dry += fw_rms / (fw_frames * 2.0);
        }
        {
            double db = 10.0 * __builtin_log10((wet + 1) / (dry + 1));
            printf("%-8s %+6.1f dB against the dry, peak %.2f\n", RM_NAME[m], db, pk / 32768.0);
            if (db < -18.0) {
                printf("FAIL %s: hardly heard\n", RM_NAME[m]);
                fails++;
            }
            if (pk > 32767) {
                printf("FAIL %s: past full scale\n", RM_NAME[m]);
                fails++;
            }
        }
    }
    printf("modules: %d failures\n", fails);
    return fails ? 1 : 0;
}
/* the flows: slots on and off, HOME under held notes, every key at once, no voice left sounding */
static int cmd_flows(void)
{
    int fails = 0;
    uint32_t k, n;
    fw_init();
    if (!slot_on[0] || slot_on[1]) {
        printf("FAIL flows: slot 1 alone is not on at the start\n");
        fails++;
    }
    press(B_SEL, 900);                                   /* slot 2 held: on */
    if (!slot_on[1] || ui.sel != 1) {
        printf("FAIL flows: a long press did not turn slot 2 on\n");
        fails++;
    }
    press(B_SEL, 100);                                   /* a short press: chosen, still on */
    if (!slot_on[1]) {
        printf("FAIL flows: a short press turned slot 2 off\n");
        fails++;
    }
    key(0, 1);
    key(2, 1);
    fw_run(400);
    press(B_HOME, 50);                                   /* HOME under held notes */
    fw_run(400);
    if (s_use[1] != s_pub[1]) {
        printf("FAIL flows: the new patch was not taken\n");
        fails++;
    }
    key(4, 1);
    fw_run(400);
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && vox[k].gate && vox[k].slot == 1 && vox[k].dsg != s_use[1]) {
            printf("FAIL flows: a held voice on the old patch\n");
            fails++;
        }
    press(B_FX, 50);                                     /* slot 1 chosen: the keys held on slot 2 still end there */
    key(0, 0);
    key(2, 0);
    key(4, 0);
    fw_run(10);
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && vox[k].gate) {
            printf("FAIL flows: a note held after its key was lifted\n");
            fails++;
        }
    press(B_SEL, 900);                                   /* slot 2 held again: off */
    if (slot_on[1]) {
        printf("FAIL flows: a long press did not turn slot 2 off\n");
        fails++;
    }
    press(B_SEL, 50);
    key(0, 1);                                           /* a key on a slot that is off: nothing */
    fw_run(50);
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && vox[k].gate && vox[k].slot == 1) {
            printf("FAIL flows: a slot that is off played\n");
            fails++;
        }
    key(0, 0);
    for (n = 1; n < NSLOT; n++)                          /* every slot on, every key at once, on each */
        press(SLOT_BTN[n], 900);
    press(B_HOME, 900);                                  /* HOME held: every slot anew */
    for (n = 0; n < NSLOT; n++) {
        press(SLOT_BTN[n], 50);
        for (k = 0; k < NKEY; k++)
            key(k, 1);
        fw_run(100);
        for (k = 0; k < NKEY; k++)
            key(k, 0);
    }
    for (n = 0; n < 1378u * 40u && slots_sounding(); n += 64u)
        fw_run(64);
    if (slots_sounding()) {
        printf("FAIL flows: a voice still sounds 40 s after every key was let go\n");
        fails++;
    }
    for (n = 0; n < 30u; n++) {                          /* HOME and SEQ thirty times, fast, under notes */
        press(SLOT_BTN[n % NSLOT], 30);
        key(0, 1);
        fw_run(20);
        press(n & 1u ? B_HOME : B_SEQ, 30);
        key(0, 0);
        fw_run(20);
    }
    fw_run(2000);
    for (n = 0; n < NSLOT; n++)
        if (s_use[n] != s_pub[n]) {
            printf("FAIL flows: a patch left untaken\n");
            fails++;
        }
    printf("flows: %d failures\n", fails);
    return fails ? 1 : 0;
}
/* the screens: one slot; three slots playing through the rack; all six with one chosen and off; a message; the scale
 * page */
static int cmd_screens(int argc, char **argv)
{
    const char *dir = argc > 2 ? argv[2] : ".";
    char name[512];
    uint32_t n;
    batt_raw = 600;
    fw_init();
    load_pct = 23;
#define SHOT(nm) do { ui.load_at = fm1_ms - 1000u; ui.force = 1; ui_draw(); ui_draw(); ui_draw(); \
    snprintf(name, sizeof name, "%s/screen-%s.ppm", dir, nm); screen_save(name); } while (0)
    SHOT("one-slot");
    print_slot(0);
    press(B_SEL, 900);
    press(B_SEL, 50);
    press(B_HOME, 50);
    press(B_LFO, 900);
    press(B_LFO, 50);
    press(B_HOME, 50);
    fw_run(4);
    for (n = 0; n < 3; n++)
        print_slot(n == 2 ? 3 : n);
    press(B_SEL, 50);
    key(0, 1);
    key(4, 1);
    press(B_LFO, 50);
    key(7, 1);
    fw_run(800);
    load_pct = 47;
    SHOT("three-slots");
    for (n = 0; n < 6; n++)
        if (!slot_on[n])
            press(SLOT_BTN[n], 900);
    press(B_EDIT, 900);                                  /* slot 5 off again, chosen */
    fw_run(4);
    load_pct = 88;
    ui.msg_until = 0;
    SHOT("five-slots");
    say("SLOT 5 OFF", 1000);
    SHOT("message");
    ui.msg_until = 0;
    ui.sc_page = 1;
    ui.scale = 15;
    ui.root = 6;
    SHOT("scale");
#undef SHOT
    return 0;
}

int main(int argc, char **argv)
{
    wt_init();
    if (argc >= 2 && !strcmp(argv[1], "patches"))
        return cmd_patches(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "flows"))
        return cmd_flows();
    if (argc >= 2 && !strcmp(argv[1], "modules"))
        return cmd_modules(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "screens"))
        return cmd_screens(argc, argv);
    if (argc < 2) {
        fprintf(stderr, "host render DIR [SEEDS] [PATH] | one DIR FAMILY SEED [PATH] | check | golden | bench | patches DIR [N] | modules DIR | flows | screens DIR\n");
        return 2;
    }
    if (!strcmp(argv[1], "render"))
        return cmd_render(argc, argv);
    if (!strcmp(argv[1], "one") && argc >= 5)
        return cmd_one(argc, argv);
    if (!strcmp(argv[1], "check"))
        return cmd_check();
    if (!strcmp(argv[1], "golden"))
        return cmd_golden();
    if (!strcmp(argv[1], "bench"))
        return cmd_bench();
    fprintf(stderr, "host: unknown command\n");
    return 2;
}
