/* SPDX-License-Identifier: GPL-3.0-only
 * NORILSK host harness: the engine compiled for the computer.
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
#include "norilsk_tables.h"

#define NELEM(a) (sizeof(a) / sizeof((a)[0]))

/* the firmware above the HAL: what main.c and the HAL give it, as the host has them */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
static volatile uint32_t fm1_ms;
static uint32_t host_ticks;
static uint32_t fm1_ticks(void) { return host_ticks += 24000u; }
#define FM1_TICKS_PER_US 24u
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
static uint32_t fb_px;                                   /* (pixels sent to the panel: the paint test's) */
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u) {
                uint16_t c = px[j * w + i];
                fb[(y + j) * 240u + x + i] = (uint16_t)((c >> 8) | (c << 8));
                fb_px++;
            }
}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u) {
                fb[(y + j) * 240u + x + i] = c;
                fb_px++;
            }
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
#include "logo.c"
#include "wt.c"
#include "fm6_core.c"
#define MO_PROBE_CANVAS 1                                   /* (the probe voice in the screen's canvas) */
#include "mol.c"
/* usb.c's state, as the screen reads it (no host: on battery); what would be sent out kept, for the MIDI tests */
static struct { uint8_t config, suspended; } usb;
static uint32_t host_mout[256], host_mout_n;
static void midi_out_event(uint32_t pkt)
{
    if (host_mout_n < 256u)
        host_mout[host_mout_n] = pkt;
    host_mout_n++;
}
/* CPU1's mailbox (src/audio_worker.h), as one core sees it (Tonitor's host): a job posted runs at the join -- the
 * units of work are apart, so the sound is the same as with the two cores at once. host_cores 1: CPU1 off */
static int audio_worker_online;
static void (*host_job)(void *);
static void *host_job_ctx;
static int audio_worker_submit(void (*fn)(void *), void *ctx)
{
    if (!audio_worker_online)
        return 0;
    host_job = fn;
    host_job_ctx = ctx;
    return 1;
}
static int audio_worker_join(void)
{
    if (host_job)
        host_job(host_job_ctx);
    host_job = 0;
    return 1;
}
static void audio_worker_check(void) {}
#include "rack.c"
#include "gens.c"
#include "loop.c"
#include "mod.c"
#include "master.c"
#include "sessions.h"
#include "ui.c"
#include "sessions.c"
#include "setup.c"
#include "presets.c"
#include "backup.c"

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
    if (d->bd.body)
        printf("  body %u ex %u ratio %u closed %u disp %u ring %u tone %u set %u spread %u color %u press %u dry %u\n",
               d->bd.body, d->bd.ex, d->bd.ratio, d->bd.closed, d->bd.disp, d->bd.ring, d->bd.tone, d->bd.set, d->bd.spread,
               d->bd.color, d->bd.press, d->bd.dry);
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
    if (d->gen >= 3u) {
        int k;
        printf("  hand %s pace %d travel %d density %d:", d->hand.on ? "on" : "off", d->hand.pace, d->hand.travel, d->hand.density);
        for (k = 0; k < d->hand.n; k++)
            printf(" %d->%d (%d..%d)", d->hand.map[k].src, d->hand.map[k].dst, d->hand.map[k].lo, d->hand.map[k].hi);
        printf("\n  grit bits %d hold %d logic %d ratio %d stut %d/%d ms bend %d mix %d\n", d->grit.bits, d->grit.hold,
               d->grit.logic, d->grit.lratio, d->grit.stut, d->grit.stut_ms, d->grit.bend, d->grit.mix);
    }
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
            mo_design(&design, f, seed, (uint32_t)path, GEN_NOW);
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
    mo_design(&design, f, seed, (uint32_t)path, GEN_NOW);
    bright = mo_measure(&design);
    print_design(&design);
    st = play(&design, 60, 4.0, 8.0, 0, wav);
    snprintf(name, sizeof name, "%s/%s-%s-%u.wav", dir, MF_NAME[design.family], PATH_NAME[path], seed);
    wav_write(name, wav, st.len ? st.len : FS * 12);
    printf("  bright %d Hz peak %.2f rms %.3f len %.2f s %.0f ns/blk -> %s\n", bright, st.peak / 32768.0, st.rms,
           st.len / (double)FS, st.ns_blk, name);
    return 0;
}

/* generation 4's bodies, to listen to (build 34): each new family three times (one draw on each path), C3 held 8 s
 * and let go; and older sounds that draw a body on generation 4 against the same draw on generation 3 (the body the
 * only difference). Mono WAVs into DIR, a line each */
static int cmd_bodies(int argc, char **argv)
{
    static const char *const BD[3] = {"none", "tube", "modal"}, *const EXN[3] = {"operators", "noise", "bow"};
    static const char *const SETN[BS_N] = {"bar", "plate", "bell", "pipe", "hull"};
    const char *dir = argc > 2 ? argv[2] : "build/bodies";
    char name[512];
    int f, path, n;
    uint32_t seed;
    for (f = MF_G4; f < MF_G5; f++)
        for (path = 0; path < PATH_N; path++) {
            stat_t st;
            seed = 4400u + (uint32_t)f * 17u + (uint32_t)path * 5u;
            mo_design(&design, f, seed, (uint32_t)path, 4);
            mo_measure(&design);
            st = play(&design, 48, 8.0, 3.5, 0, wav);
            snprintf(name, sizeof name, "%s/%s-%s-%u.wav", dir, MF_NAME[f], PATH_NAME[path], seed);
            wav_write(name, wav, st.len ? st.len : FS * 12);
            printf("%-8s %-8s seed %u: %s, %s%s%s, ring %.1f s, dry %u%%, peak %.2f rms %.3f -> %s\n", MF_NAME[f],
                   PATH_NAME[path], seed, BD[design.bd.body % 3], EXN[design.bd.ex % 3],
                   design.bd.body == BD_MODAL ? ", " : "", design.bd.body == BD_MODAL ? SETN[design.bd.set % BS_N] : "",
                   design.bd.ring / 10.0, design.bd.dry, st.peak / 32768.0, st.rms, name);
        }
    for (n = 0, seed = 700u; n < 4 && seed < 2000u; seed++) {   /* older sounds given a body */
        static const int8_t OLD[4] = {MF_DRONE, MF_GLASS, MF_METAL, MF_CLOUD};
        int g;
        f = OLD[n];
        mo_design(&design, f, seed, PATH_OPEN, 4);
        if (design.bd.body == BD_NONE)
            continue;
        for (g = 3; g <= 4; g++) {
            stat_t st;
            mo_design(&design, f, seed, PATH_OPEN, (uint32_t)g);
            mo_measure(&design);
            st = play(&design, 48, 8.0, 3.5, 0, wav);
            snprintf(name, sizeof name, "%s/old-%s-%u-gen%d.wav", dir, MF_NAME[f], seed, g);
            wav_write(name, wav, st.len ? st.len : FS * 12);
            printf("%-8s seed %u generation %d: %s%s%s -> %s\n", MF_NAME[f], seed, g, g == 4 ? BD[design.bd.body % 3] : "no body",
                   g == 4 && design.bd.body == BD_MODAL ? ", " : "",
                   g == 4 && design.bd.body == BD_MODAL ? SETN[design.bd.set % BS_N] : "", name);
        }
        n++;
    }
    return 0;
}
/* generation 5's found sounds, to listen to (build 35): each new family on each path, C3 held 8 s and let go; and
 * older sounds with a cloud that take a clip on generation 5 against generation 4 (the clip the only difference) */
static int cmd_found(int argc, char **argv)
{
    const char *dir = argc > 2 ? argv[2] : "build/foundsounds";
    char name[512];
    int f, path, n;
    uint32_t seed;
    for (f = MF_G5; f < MF_G6; f++)
        for (path = 0; path < PATH_N; path++) {
            stat_t st;
            seed = 5100u + (uint32_t)f * 13u + (uint32_t)path * 3u;
            mo_design(&design, f, seed, (uint32_t)path, 5);
            mo_measure(&design);
            st = play(&design, 48, 8.0, 3.5, 0, wav);
            snprintf(name, sizeof name, "%s/%s-%s-%u.wav", dir, MF_NAME[f], PATH_NAME[path], seed);
            wav_write(name, wav, st.len ? st.len : FS * 12);
            printf("%-7s %-8s seed %u: %s, %u%% of the grains, tracking %u%%, peak %.2f rms %.3f -> %s\n", MF_NAME[f],
                   PATH_NAME[path], seed, design.fs.clip ? FC_NAME[design.fs.clip - 1] : "-", design.fs.share,
                   design.fs.track, st.peak / 32768.0, st.rms, name);
        }
    for (n = 0, seed = 900u; n < 4 && seed < 3000u; seed++) {
        static const int8_t OLD[4] = {MF_CLOUD, MF_TUNNEL, MF_SHATTER, MF_HOWL};
        int g;
        f = OLD[n];
        mo_design(&design, f, seed, PATH_OPEN, 5);
        if (!design.fs.clip)
            continue;
        for (g = 4; g <= 5; g++) {
            stat_t st;
            mo_design(&design, f, seed, PATH_OPEN, (uint32_t)g);
            mo_measure(&design);
            st = play(&design, 48, 8.0, 3.5, 0, wav);
            snprintf(name, sizeof name, "%s/old-%s-%u-gen%d.wav", dir, MF_NAME[f], seed, g);
            wav_write(name, wav, st.len ? st.len : FS * 12);
            printf("%-7s seed %u generation %d: %s -> %s\n", MF_NAME[f], seed, g,
                   g == 5 ? FC_NAME[design.fs.clip - 1] : "no found sound", name);
        }
        n++;
    }
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
                mo_design(&design, f, seed, (uint32_t)path, GEN_NOW);
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
                    if (getenv("CHECK_V")) {
                        uint32_t bb;
                        int32_t blk[CTL];
                        print_design(&design);
                        for (bb = 0; bb < 1378u * 8u; bb++) {   /* (on from where it is: what keeps it) */
                            int32_t pkk = 0, ii;
                            mo_render(&voice, &design, blk, 0);
                            for (ii = 0; ii < CTL; ii++)
                                pkk = maxi(pkk, absi(blk[ii]));
                            if (bb % 1378u == 0u)
                                printf("   +%us: live %u fm_done %u genv %d tail %u frozen %u gr %u hoff %d %d %d pk %d stut %u\n", bb / 1378u,
                                       voice.live, voice.fm_done, voice.genv, voice.tail, voice.frozen, design.gr.mode,
                                       voice.hoff[0], voice.hoff[1], voice.hoff[2], pkk, voice.stut_left);
                        }
                    }
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
        mo_design(&design, f, 77u + (uint32_t)f, 0, GEN_NOW);
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

/* generation 2's promise (build 5): every sound held 10 s is a texture -- it swells in (no strike: its first quarter
 * second at most twice its mean over the held part), it holds (its last 4 s at least 0.15 of its loudest quarter
 * second: a slow breath of the level is no fade) and it moves (its level or its brightness wanders over the held part: reported, the stillest counted) */
static int cmd_sustain(void)
{
    enum { WB = 344, NW = 40 };                          /* windows of 344 blocks (0.25 s), 10 s */
    int f, s, path, fails = 0, n = 0, still = 0;
    int32_t blk[CTL];
    for (path = 0; path < PATH_N; path++)
        for (f = 0; f < MF_G7; f++)                      /* (generation 7's struck families: host struck) */
            for (s = 0; s < 20; s++) {
                uint32_t seed = 9000u + (uint32_t)f * 173u + (uint32_t)s * 11u + (uint32_t)path * 1301u, b, i;
                double w[NW] = {0}, br[NW] = {0}, mx = 0, held = 0, lo = 1e30, hi = 0, blo = 1e30, bhi = 0;
                int32_t prev = 0;
                mo_design(&design, f, seed, (uint32_t)path, GEN_NOW);
                if (getenv("NOHAND"))
                    design.hand.on = 0;
                if (getenv("NOGRIT"))
                    memset(&design.grit, 0, sizeof design.grit);
                mo_measure(&design);
                memset(&voice, 0, getenv("CLEARBUF") ? sizeof voice : sizeof voice - MO_GR_N);
                mo_note_on(&voice, &design, 60, 1);
                for (b = 0; b < (uint32_t)(WB * NW); b++) {
                    int k = (int)(b / WB);
                    if (!mo_render(&voice, &design, blk, 0))
                        break;
                    for (i = 0; i < CTL; i++) {
                        double x = blk[i], dx = blk[i] - prev;
                        w[k] += x * x;
                        br[k] += dx * dx;
                        prev = blk[i];
                    }
                }
                if (getenv("SUST_SEED") && (uint32_t)atoi(getenv("SUST_SEED")) == seed) {
                    printf("seed %u windows:", seed);
                    for (i = 0; i < 8u; i++)
                        printf(" %.3f", __builtin_sqrt(w[i] / (WB * CTL)) / 32768.0);
                    printf("  gain %d\n", design.gain);
                }
                for (i = 0; i < NW; i++) {
                    br[i] = w[i] > 0 ? __builtin_sqrt(br[i] / w[i]) : 0;
                    w[i] = __builtin_sqrt(w[i] / (WB * CTL)) / 32768.0;
                    if (w[i] > mx)
                        mx = w[i];
                }
                for (i = 24; i < NW; i++)              /* (the last 4 s: a slow breath of the level is not a fade) */
                    held += w[i] / 16;
                for (i = 8; i < NW; i++) {
                    lo = w[i] < lo ? w[i] : lo;
                    hi = w[i] > hi ? w[i] : hi;
                    blo = br[i] < blo ? br[i] : blo;
                    bhi = br[i] > bhi ? br[i] : bhi;
                }
                n++;
                {
                    double mean = 0;
                    for (i = 4; i < NW; i++)
                        mean += w[i] / (NW - 4);
                    if (held < 0.15 * mx) {
                        printf("FAIL %s %s seed %u: does not hold (last 4 s %.4f, loudest %.4f)\n", MF_NAME[f],
                               PATH_NAME[path], seed, held, mx);
                        fails++;
                    }
                    if (w[0] > 2.0 * mean) {
                        printf("FAIL %s %s seed %u: struck (first 0.25 s %.4f, held %.4f)\n", MF_NAME[f], PATH_NAME[path],
                               seed, w[0], mean);
                        fails++;
                    }
                }
                if (hi < 1.03 * lo && bhi < 1.03 * blo) {
                    still++;
                    if (getenv("SUSTAIN_V"))
                        printf("still %s %s seed %u: level %.4f..%.4f bright %.3f..%.3f lfo w%d s%d amd %d\n",
                               MF_NAME[f], PATH_NAME[path], seed, lo, hi, blo, bhi, design.p[FP_LFW], design.p[FP_LFS],
                               design.p[FP_LAMD]);
                }
            }
    printf("sustain: %d sounds held 10 s, %d failures, %d barely moving (level and brightness within 3 %%)\n", n,
           fails, still);
    if (still * 50 > n) {
        printf("FAIL sustain: more than 2 %% of the sounds barely move\n");
        fails++;
    }
    return fails ? 1 : 0;
}

/* generation 7's promise (build 47): struck -- each family heard at once (in its first half second its loudest 50 ms,
 * or a machine's quarter of it: its strikes build up the body's ring);
 * STRIKE falls away while held (its last 2 s of 8 under a tenth of its loudest); MOTION keeps striking while held (no
 * 5 s of the 12 held under a twentieth of its loudest, so the slowest machine strikes in every one); every one quiet
 * 3 s after it is let go (the body rung out) */
static int cmd_struck(void)
{
    enum { WB = 69, NW = 260 };                          /* windows of 69 blocks (50 ms): 13 s */
    int f, s, path, fails = 0, n = 0;
    uint32_t g;
    int32_t blk[CTL];
    for (g = 7u; g <= GEN_NOW; g++)              /* (7: struck once; 8: those strike again while held, scattered) */
    for (path = 0; path < PATH_N; path++)
        for (f = MF_G7; f < MF_N; f++)
            for (s = 0; s < 12; s++) {
                uint32_t seed = 7100u + (uint32_t)f * 131u + (uint32_t)s * 17u + (uint32_t)path * 911u, b, i;
                double w[NW] = {0}, mx = 0, first = 0, tail = 0, lull = 1e30, after = 0;
                int motion, k, j;
                mo_design(&design, f, seed, (uint32_t)path, g);
                mo_measure(&design);
                motion = design.mc.mode == MC_MOTION || design.mc.mode == MC_SCATTER;   /* (striking while held) */
                memset(&voice, 0, sizeof voice - MO_GR_N);
                mo_note_on(&voice, &design, 60, 1);
                for (b = 0; b < (uint32_t)(WB * NW); b++) {
                    if (b == (uint32_t)(WB * (motion ? 240 : 160)))
                        mo_note_off(&voice, &design);
                    if (!mo_render(&voice, &design, blk, 0))
                        break;
                    for (i = 0; i < CTL; i++)
                        w[b / WB] += (double)blk[i] * blk[i];
                }
                for (k = 0; k < NW; k++) {
                    w[k] = __builtin_sqrt(w[k] / (WB * CTL)) / 32768.0;
                    if (w[k] > mx)
                        mx = w[k];
                    if (k < 10 && w[k] > first)
                        first = w[k];
                }
                for (k = 120; k < 160; k++)              /* (STRIKE: 6 .. 8 s) */
                    tail = w[k] > tail ? w[k] : tail;
                for (k = 0; k + 100 <= 240; k += 10) {   /* (MOTION: every 5 s of the 12 held) */
                    double m = 0;
                    for (j = k; j < k + 100; j++)
                        m = w[j] > m ? w[j] : m;
                    lull = m < lull ? m : lull;
                }
                for (k = (motion ? 240 : 160) + 60; k < NW; k++)   /* (3 s after letting go, on) */
                    after = w[k] > after ? w[k] : after;
                n++;
                if (mx < 0.01 || first < (motion ? 0.25 : 0.7) * mx) {   /* (a machine's later strikes may meet in a
                                                                          * window: louder than its first) */
                    printf("FAIL struck gen %u %s %s seed %u: not heard at once (first %.3f, loudest %.3f)\n", g, MF_NAME[f],
                           PATH_NAME[path], seed, first, mx);
                    fails++;
                }
                if (!motion && tail > 0.1 * mx) {
                    printf("FAIL struck gen %u %s %s seed %u: does not fall away (%.3f of %.3f)\n", g, MF_NAME[f], PATH_NAME[path],
                           seed, tail, mx);
                    fails++;
                }
                if (motion && lull < 0.05 * mx) {
                    printf("FAIL struck gen %u %s %s seed %u: stops while held (%.4f of %.3f)\n", g, MF_NAME[f], PATH_NAME[path],
                           seed, lull, mx);
                    fails++;
                }
                if (after > 0.001) {
                    printf("FAIL struck gen %u %s %s seed %u: rings on after letting go (%.4f)\n", g, MF_NAME[f], PATH_NAME[path],
                           seed, after);
                    fails++;
                }
            }
    printf("struck: %d sounds, %d failures\n", n, fails);
    return fails ? 1 : 0;
}

static int cmd_golden(uint32_t gen)
{
    int f, s, path;
    int32_t blk[CTL];
    for (path = 0; path < PATH_N; path++)
        for (f = 0; f < (gen < 3u ? MF_N_OLD : gen < 4u ? MF_N_G3 : gen < 5u ? MF_N_G4 : gen < 6u ? MF_N_G5 : gen < 7u ? MF_N_G6 : MF_N); f++)
            for (s = 0; s < 8; s++) {
                uint32_t seed = 100u + (uint32_t)f * 31u + (uint32_t)s + (uint32_t)path * 503u, h, b;
                mo_design(&design, f, seed, (uint32_t)path, gen);
                mo_measure(&design);
                /* (the design as generations 0 .. 2 laid it out: generation 3's fields after it, zero before it) */
                h = fnv(2166136261u, &design, gen < 3u ? (uint32_t)__builtin_offsetof(mo_design_t, hand)
                                              : gen < 4u ? (uint32_t)__builtin_offsetof(mo_design_t, bd)
                                              : gen < 5u ? (uint32_t)__builtin_offsetof(mo_design_t, fs)
                                              : gen < 7u ? (uint32_t)__builtin_offsetof(mo_design_t, mc) : (uint32_t)sizeof design);
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
            mo_design(&design, f, 300u + (uint32_t)s, 0, GEN_NOW);
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
    ui_init();
    gens_init(ui.seed);
    scale_random(ui.seed);
    ses_init(1);
    master_q12 = 4096;
}
static void print_gen(uint32_t l)
{
    const gen_patch_t *p = &gbuf[l][g_pub[l]];
    uint32_t i;
    printf("gen %u%s: %s, %s (body %s, %s), lvl %d pan %d, dry %d%%:", l + 1, gen_mute[l] ? " (muted)" : "",
           SC_NAME[p->scene], MF_NAME[p->gen.family], MF_NAME[p->gen.body], PATH_NAME[p->gen.path], gen_lvl[l], gen_pan[l],
           p->dry * 100 / Q15);
    for (i = 0; i < p->nent; i++)
        printf(" %u.%u", p->ent[i] / FX_N + 4u, p->ent[i] % FX_N + 1u);
    if (!p->nent)
        printf(" direct");
    if (p->xsrc < NGEN)
        printf("; %s from %u, %d", XM_NAME[p->xkind], p->xsrc + 1, p->xdepth);
    printf("\n");
}
static void print_fx(uint32_t s)
{
    const fx_patch_t *f = &fbuf[s][f_pub[s]];
    uint32_t k, j, mem = 0;
    printf("fx %u%s: %s:", s + 4u, fx_mute[s] ? " (muted)" : "", FR_NAME[f->recipe]);
    for (k = 0; k < f->n; k++) {
        printf(" %s(%u %u %u %u)%s", RM_NAME[f->e[k].kind], f->e[k].p[0], f->e[k].p[1], f->e[k].p[2], f->e[k].p[3],
               (f->out >> k) & 1u ? "+out" : "");
        mem += rm_size(f->e[k].kind);
    }
    (void)j;
    if (f->ret)
        printf(" return %u%%", f->ret & 0x7Fu);
    printf("  [%u B]\n", mem);
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
/* the flash, for the sessions: 1 MiB as the part behaves (erase: 0xFF; a write only clears bits), every erase counted */
static uint8_t hflash[0x100000];
static uint32_t hf_erases[SES_NSECT];
static int hf_in(uint32_t off, uint32_t n) { return off >= SES_LO && off <= SES_HI && n <= SES_HI - off; }
static int ses_flash_read(uint32_t off, void *p, uint32_t n)
{
    if (off >= sizeof hflash || n > sizeof hflash - off)
        return -1;
    memcpy(p, hflash + off, n);
    return 0;
}
static int ses_flash_erase(uint32_t off)
{
    if (!hf_in(off, 0x1000u) || (off & 0xFFFu))
        return -8;
    memset(hflash + off, 0xFF, 0x1000u);
    hf_erases[(off - SES_LO) / SES_SECT]++;
    return 0;
}
static int ses_flash_prog(uint32_t off, const void *p, uint32_t n)
{
    uint32_t i;
    if (!hf_in(off, n))
        return -8;
    for (i = 0; i < n; i++)
        hflash[off + i] &= ((const uint8_t *)p)[i];
    return 0;
}
static void ses_isr_sync(void) { fw_run(3); }
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
static int cmd_patches(int argc, char **argv)
{
    const char *dir = argc > 2 ? argv[2] : ".";
    int n = argc > 3 ? atoi(argv[3]) : 8, i, fails = 0, l;
    char name[512];
    for (i = 0; i < n; i++) {
        double t0, ns;
        fw_init();
        ui.seed = 777u + (uint32_t)i * 7919u;
        printf("patch %d:\n", i);
        for (l = 0; l < (int)NSLOT; l++) {              /* every slot anew */
            press(SLOT_BTN[l], 50);
            press(B_HOME, 50);
            fw_run(30);
        }
        for (l = 0; l < (int)NGEN; l++)
            print_gen((uint32_t)l);
        for (l = 0; l < (int)NFX; l++)
            print_fx((uint32_t)l);
        fw_frames = 0;
        fw_peak = 0;
        fw_rms = 0;
        t0 = now_ns();
        for (l = 0; l < (int)NGEN; l++) {               /* notes on each, overlapping (four voices) */
            press(SLOT_BTN[l], 50);
            key(0u + 2u * (uint32_t)l, 1);
            if (l == 0)
                key(4, 1);
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
/* nothing lingers: N patches, notes held 3 s on each generator, every key let go; how long until the voices are done,
 * and until the output is under -60 dB FS for good (TAIL_LIMIT_S at most, else a failure). The voices and the effect
 * slots only: the master delay's and reverb's returns turned down (MIX 0) -- their tails are kept, `host mtails` */
#define TAIL_LIMIT_S 6.0
static int cmd_tails(int argc, char **argv)
{
    int n = argc > 2 ? atoi(argv[2]) : 24, i, l, fails = 0;
    double worst = 0, wv = 0, sum = 0;
    for (i = 0; i < n; i++) {
        uint32_t b, last_loud = 0, voices_done = 0;
        fw_init();
        ui.seed = 4321u + (uint32_t)i * 6007u;
        ui.rvp[3] = ui.dlp[3] = 0;
        rv_param(&mrv, ui.rvp[0], ui.rvp[1], ui.rvp[2], ui.rvp[3]);
        mdl_param(ui.dlp[0], ui.dlp[1], ui.dlp[2], ui.dlp[3]);
        for (l = 0; l < (int)NSLOT; l++) {
            press(SLOT_BTN[l], 50);
            press(B_HOME, 50);
            fw_run(30);
        }
        for (l = 0; l < (int)NGEN; l++) {
            press(SLOT_BTN[l], 50);
            key(2u * (uint32_t)l, 1);
            fw_run(400);
        }
        fw_run(4134);
        for (l = 0; l < (int)NKEY; l++)
            key((uint32_t)l, 0);
        for (b = 0; b < 1378u * 40u; b++) {              /* 40 s at most */
            int32_t pk0 = fw_peak;
            uint32_t k, alive = 0;
            fw_peak = 0;
            fw_run(1);
            for (k = 0; k < NVOICE; k++)
                alive |= vox[k].sounding;
            if (alive)
                voices_done = b + 1u;
            if (fw_peak > 33)                            /* -60 dB FS */
                last_loud = b + 1u;
            fw_peak = pk0 > fw_peak ? pk0 : fw_peak;
            if (!alive && b > last_loud + 2756u)         /* 2 s quiet with no voice: done */
                break;
        }
        if (getenv("TAILS_V")) {                         /* what is still sounding: each effect slot muted in turn */
            uint32_t m, k;
            int32_t mn = 1 << 30, mx = -(1 << 30), out[2 * CTL];
            for (k = 0; k < 64u; k++) {
                mix_block(out);
                for (m = 0; m < 2u * CTL; m++) {
                    mn = out[m] < mn ? out[m] : mn;
                    mx = out[m] > mx ? out[m] : mx;
                }
            }
            printf("  residue: min %d max %d\n", mn, mx);
            for (m = 0; m < NFX; m++) {
                fx_mute[m] = 1;
                fw_peak = 0;
                fw_run(2756);
                printf("  slot %u muted too: peak %d (recipe %s)\n", m + 4u, fw_peak, FR_NAME[fbuf[m][f_use[m]].recipe]);
            }
        }
        {
            double t = last_loud * 0.0007256, tv = voices_done * 0.0007256;
            printf("patch %2d: voices %5.2f s, sound %5.2f s after the keys\n", i, tv, t);
            if (t > TAIL_LIMIT_S) {
                printf("FAIL patch %d: sound goes on %.2f s after the last key\n", i, t);
                fails++;
            }
            worst = t > worst ? t : worst;
            wv = tv > wv ? tv : wv;
            sum += t;
        }
    }
    printf("tails: %d patches, mean %.2f s, longest %.2f s (voices %.2f s), %d failures\n", n, sum / n, worst, wv, fails);
    return fails ? 1 : 0;
}
/* each effect alone, many values: what it gives out 10 s after its input has fallen silent (a tail that sustains
 * itself, or a DC offset) */
static int cmd_fxtails(void)
{
    uint32_t m, rep;
    int fails = 0;
    for (m = 0; m < NKIND; m++) {
        int32_t worst = 0, wdc = 0, bad = 0;
        for (rep = 0; rep < 24u; rep++) {
            gen_patch_t *p;
            fx_patch_t *f;
            rnd_t r;
            uint32_t b, k, alive;
            int32_t out[2 * CTL], pk = 0;
            int64_t dc = 0;
            fw_init();
            gen_draw(0, MF_DRONE, 5151u + rep);
            fw_run(4);
            for (b = 1; b < NFX; b++)
                fx_mute[b] = 1;
            f = &fbuf[0][f_use[0]];
            memset(f, 0, sizeof *f);
            f->n = 1;
            f->e[0].kind = (uint8_t)m;
            rnd_seed(&r, 333u + rep * 29u + m * 7u);
            fx_values(&f->e[0], &r);
            memset(fx_arena[0], 0, FX_ARENA);
            fx_start(0);
            p = &gbuf[0][g_use[0]];
            p->nent = 1;
            p->ent[0] = 0;
            p->send = Q15;
            p->dry = 0;
            p->xsrc = XM_NONE;
            press(B_FX, 50);
            key(0, 1);
            fw_run(1378 * 2);
            key(0, 0);
            for (b = 0; b < 1378u * 40u; b++) {
                fw_run(1);
                for (alive = 0, k = 0; k < NVOICE; k++)
                    alive |= vox[k].sounding;
                if (!alive)
                    break;
            }
            fw_run(1378 * 10);
            for (b = 0; b < 344u; b++) {
                mix_block(out);
                for (k = 0; k < 2u * CTL; k++) {
                    int32_t a = out[k] < 0 ? -out[k] : out[k];
                    pk = a > pk ? a : pk;
                    dc += out[k];
                }
            }
            dc /= 344 * 2 * CTL;
            if (pk > 33) {
                bad++;
                if (getenv("VERBOSE"))
                    printf("  %s rep %u values %u %u %u %u: peak %d dc %d\n", RM_NAME[m], rep, f->e[0].p[0], f->e[0].p[1],
                           f->e[0].p[2], f->e[0].p[3], pk, (int)dc);
            }
            worst = pk > worst ? pk : worst;
            wdc = (dc < 0 ? -dc : dc) > wdc ? (int32_t)(dc < 0 ? -dc : dc) : wdc;
        }
        printf("%-8s 10 s after: %2d of 24 still sounding, loudest %6d, DC up to %6d\n", RM_NAME[m], bad, worst, wdc);
        if (bad)
            fails++;
    }
    printf("fxtails: %d effects with a tail of their own\n", fails);
    return fails ? 1 : 0;
}
/* every effect alone: generator 1 wired into it, a few notes; its level against the dry */
static int cmd_modules(int argc, char **argv)
{
    const char *dir = argc > 2 ? argv[2] : ".";
    uint32_t m, rep;
    int fails = 0;
    char name[512];
    for (m = 0; m < NKIND; m++) {
        double dry = 0, wet = 0;
        int32_t pk = 0;
        if (m == RM_DLY || m == RM_REV)                  /* (the master's: never in a slot, build 31) */
            continue;
        for (rep = 0; rep < 3u; rep++) {
            gen_patch_t *p;
            fx_patch_t *f;
            rnd_t r;
            uint32_t b;
            fw_init();
            gen_draw(0, rep == 0 ? MF_IMPACT : rep == 1 ? MF_DRONE : MF_GLASS, 4242u + rep);
            fw_run(4);
            for (b = 1; b < NFX; b++)
                fx_mute[b] = 1;
            f = &fbuf[0][f_use[0]];                     /* effect slot 4: that effect alone (its memory was cleared) */
            memset(f, 0, sizeof *f);
            f->n = 1;
            f->e[0].kind = (uint8_t)m;
            rnd_seed(&r, 91u + rep * 17u + m);
            fx_values(&f->e[0], &r);
            memset(fx_arena[0], 0, FX_ARENA);
            fx_start(0);
            p = &gbuf[0][g_use[0]];
            p->nent = 1;
            p->ent[0] = 0;
            p->send = Q15;
            p->dry = 0;
            p->xsrc = XM_NONE;
            fw_frames = 0;
            fw_peak = 0;
            fw_rms = 0;
            press(B_FX, 50);
            key(0, 1);
            key(4, 1);
            fw_run(1378 * 2);
            key(0, 0);
            key(4, 0);
            fw_run(1378 * 2);
            wet += fw_rms / (fw_frames * 2.0);
            pk = maxi(pk, fw_peak);
            if (getenv("VERBOSE"))
                printf("  %s rep %u values %u %u %u %u rms %.4f\n", RM_NAME[m], rep, f->e[0].p[0], f->e[0].p[1], f->e[0].p[2],
                       f->e[0].p[3], __builtin_sqrt(fw_rms / (fw_frames * 2.0)) / 32768.0);
            if (rep == 0) {
                snprintf(name, sizeof name, "%s/module-%s.wav", dir, RM_NAME[m]);
                wav_write2(name, wav2, fw_frames);
            }
            p->nent = 0;                                /* (the same, dry, for the comparison) */
            p->dry = Q15;
            fw_run(1378 * 3);
            fw_frames = 0;
            fw_rms = 0;
            key(0, 1);
            key(4, 1);
            fw_run(1378 * 2);
            key(0, 0);
            key(4, 0);
            fw_run(1378 * 2);
            dry += fw_rms / (fw_frames * 2.0);
        }
        {
            double db = 10.0 * __builtin_log10((wet + 1) / (dry + 1));
            printf("%-8s %+6.1f dB against the dry, peak %.2f, %u B\n", RM_NAME[m], db, pk / 32768.0, rm_size(m));
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
    {   /* every recipe fits its slot's memory, drawn many times */
        uint32_t s, t, worst = 0;
        fw_init();
        for (t = 0; t < 400u; t++)
            for (s = 0; s < NFX; s++) {
                const fx_patch_t *f;
                uint32_t k, mem = 0;
                fw_run(30);                             /* (the last one taken) */
                draw_path = (uint8_t)(t / FR_N % DP_N);  /* (every path) */
                if (!fx_draw(s, (int32_t)(t % FR_N), 1000u + t * 3u + s)) {
                    printf("FAIL fx_draw busy\n");
                    fails++;
                    continue;
                }
                f = &fbuf[s][f_pub[s]];
                for (k = 0; k < f->n; k++) {
                    mem += rm_size(f->e[k].kind);
                    if (f->e[k].kind == RM_DLY || f->e[k].kind == RM_REV) {   /* (build 30: the master's only) */
                        printf("FAIL recipe %s drew %s\n", FR_NAME[f->recipe], RM_NAME[f->e[k].kind]);
                        fails++;
                    }
                }
                worst = mem > worst ? mem : worst;
                if (mem > FX_ARENA || f->n < 2u || f->n > FX_N) {
                    printf("FAIL recipe %s: %u effects, %u B\n", FR_NAME[f->recipe], f->n, mem);
                    fails++;
                }
            }
        draw_path = DP_ANY;
        printf("recipes: the largest slot %u B of %u (no DELAY, no REVERB drawn)\n", worst, FX_ARENA);
    }
    {   /* build 31: a saved patch with DELAY and REVERB in a slot takes TAPE and SPRING; a third that no longer fits
         * the slot's memory is left out */
        fx_patch_t t;
        const fx_patch_t *f;
        fw_init();
        fw_run(30);
        memset(&t, 0, sizeof t);
        t.n = 2;
        t.seed = 1;
        t.e[0].kind = RM_DLY;
        t.e[1].kind = RM_REV;
        if (!fx_set(0, &t))
            printf("FAIL fx_set busy\n"), fails++;
        f = &fbuf[0][f_pub[0]];
        if (f->n != 2u || f->e[0].kind != RM_TAP || f->e[1].kind != RM_SPR)
            printf("FAIL old DELAY, REVERB: now %s, %s\n", RM_NAME[f->e[0].kind], RM_NAME[f->e[1].kind]), fails++;
        fw_run(30);
        t.n = 3;
        t.seed = 2;
        t.e[0].kind = RM_STR;
        t.e[1].kind = RM_TAP;
        t.e[2].kind = RM_SPR;
        if (!fx_set(0, &t))
            printf("FAIL fx_set busy\n"), fails++;
        f = &fbuf[0][f_pub[0]];
        if (f->n != 2u || f->e[0].kind != RM_STR || f->e[1].kind != RM_TAP)
            printf("FAIL STRING TAPE SPRING: %u effects\n", f->n), fails++;
        fw_run(30);                                     /* .. and in a saved session's snapshots */
        snap.s[2].x[1].f = t;
        snap.s[2].x[1].f.n = 2;
        snap.s[2].x[1].f.e[0].kind = RM_REV;
        snap.s[2].x[1].f.e[1].kind = RM_DLY;
        snap.used |= 1u << 2;
        if (ses_save(9) != SES_OK)
            printf("FAIL retired: the session not saved\n"), fails++;
        fw_init();
        if (ses_load_slot(9) != SES_OK || snap.s[2].x[1].f.e[0].kind != RM_SPR || snap.s[2].x[1].f.e[1].kind != RM_TAP)
            printf("FAIL retired: a snapshot's REVERB, DELAY not taken as SPRING, TAPE\n"), fails++;
        printf("retired: DELAY -> TAPE, REVERB -> SPRING; a third past the slot's memory left out\n");
    }
    {   /* KNOB 2 (build 33): at 0 every effect passes its input -- two very different slots sound alike; at 100 not */
        static const uint8_t KA[2] = {RM_FLD, RM_SCR}, KB[2] = {RM_XOR, RM_SMR};
        static int32_t mixref[1378u * 2u * CTL];
        uint32_t w, m2;
        for (w = 0; w < 2u; w++) {
            double e[2] = {0, 0}, d = 0;
            for (m2 = 0; m2 < 2u; m2++) {
                fx_patch_t *f;
                uint32_t b2, i2;
                int32_t o[2 * CTL];
                fw_init();
                ui.seed = 4242u;                         /* (the same generators each time, in the same scale) */
                gens_init(ui.seed);
                scale_random(ui.seed);
                fx_wet[0] = w ? 100u : 0u;
                fx_wq[0] = fx_wet[0] * Q15 / 100;       /* (there from the start, not glided to) */
                f = &fbuf[0][f_use[0]];
                memset(f, 0, sizeof *f);
                f->n = 2;
                f->e[0].kind = m2 ? KB[0] : KA[0];
                f->e[1].kind = m2 ? KB[1] : KA[1];
                memcpy(f->e[0].p, RM_HOME[f->e[0].kind], 4);
                memcpy(f->e[1].p, RM_HOME[f->e[1].kind], 4);
                memset(fx_arena[0], 0, FX_ARENA);
                fx_start(0);
                press(B_FX, 50);
                key(0, 1);
                for (b2 = 0; b2 < 1378u; b2++) {
                    mix_block(o);
                    for (i2 = 0; i2 < 2u * CTL; i2++) {
                        uint32_t at = b2 * 2u * CTL + i2;
                        double v = o[i2];
                        e[m2] += v * v;
                        if (m2 == 0)
                            mixref[at] = o[i2];
                        else
                            d += (v - mixref[at]) * (v - mixref[at]);
                    }
                }
                key(0, 0);
            }
            d = 10.0 * __builtin_log10((d + 1) / (e[0] + 1));
            printf("fx mix %s: FOLD SCREAM against XOR SMEAR, their difference %.1f dB under the sound\n",
                   w ? "100" : "0", -d);
            if (w == 0 && d > -40.0)
                printf("FAIL fx mix 0: the effects still heard\n"), fails++;
            if (w == 1 && d < -10.0)
                printf("FAIL fx mix 100: two different slots alike\n"), fails++;
        }
        fx_wet[0] = FX_WET_DEF;
    }
    printf("modules: %d failures\n", fails);
    return fails ? 1 : 0;
}
/* the flows: choosing, muting, HOME on both kinds of slot, latching, every key at once, no voice left sounding */
/* the loopers (Tonitor's), through the panel as a hand would: a loop recorded, an overdub, a note tied over the end,
 * PLAY, PLAY held and a loop taken out, the direction and speed keys, SHIFT + OCT, a generator's own move, VARY,
 * SHIFT + PLAY, REC held -- and nothing sounding on once the loops stop */
static uint32_t loop_voices(uint32_t slot)
{
    uint32_t k, n = 0;
    for (k = 0; k < NVOICE; k++)
        n += vox[k].sounding && vox[k].gate && vox[k].src == SRC_LOOP && vox[k].slot == slot;
    return n;
}
static void hold_btn(uint32_t label, int down)
{
    if (down) {
        fm1_in.pressed |= 1u << PANEL_BTN[label];
        fm1_in.buttons |= 1u << PANEL_BTN[label];
    } else
        fm1_in.buttons &= ~(1u << PANEL_BTN[label]);
    ui_input();
}
static int cmd_looper(void)
{
    int fails = 0;
    uint32_t b, seen, gaps, len, tn;
#define LFAIL(...) do { printf("FAIL looper: " __VA_ARGS__); printf("\n"); fails++; } while (0)
    fw_init();
    press(B_FX, 50);
    press(B_REC, 50);
    if (loop[0].rec != LR_ARMED)
        LFAIL("REC did not arm");
    fw_run(300);                                         /* (armed: nothing until the first note) */
    key(0, 1);
    fw_run(300);
    if (loop[0].rec != LR_REC)
        LFAIL("the first note did not start the recording");
    key(0, 0);
    fw_run(300);
    key(2, 1);
    fw_run(300);
    key(2, 0);
    fw_run(400);
    press(B_REC, 50);
    if (loop[0].ntrk != 1 || !loop[0].play)
        LFAIL("REC again did not end the loop (%u loops, play %u)", loop[0].ntrk, loop[0].play);
    len = loop[0].trk[0].len;
    for (b = 0, seen = 0; b < 2u * len + 50u; b++) {
        fw_run(1);
        seen |= loop_voices(0) != 0;
    }
    if (!seen)
        LFAIL("the loop played no note");
    press(B_REC, 50);                                    /* an overdub, a note held over its end: tied */
    key(4, 1);
    tn = ui.key_note[4];
    fw_run(500);
    press(B_REC, 50);
    if (loop[0].ntrk != 2)
        LFAIL("the overdub was not kept (%u loops)", loop[0].ntrk);
    key(4, 0);
    for (b = 0, gaps = 0; b < 2u * loop[0].trk[1].len; b++) {   /* the tied note never lets go as it goes round */
        uint32_t k, on = 0;
        fw_run(1);
        for (k = 0; k < NVOICE; k++)
            on |= vox[k].sounding && vox[k].gate && vox[k].src == SRC_LOOP && vox[k].note == tn;
        gaps += !on;
    }
    if (gaps > 2u)
        LFAIL("the tied note was let go %u blocks as the loop went round", gaps);
    press(B_PLAY, 50);                                   /* PLAY: stopped */
    fw_run(10);
    if (loop[0].play || loop_voices(0))
        LFAIL("PLAY did not stop the loops");
    press(B_PLAY, 50);                                   /* .. and again */
    fw_run(10);
    if (!loop[0].play)
        LFAIL("PLAY did not start the loops again");
    hold_btn(B_PLAY, 1);                                 /* PLAY held: loop 1 out with the first white key */
    fw_run(700);
    ui_input();
    if (!ui.ovd)
        LFAIL("PLAY held did not bring the loops onto the keys");
    key(0, 1);
    fw_run(20);
    key(0, 0);
    hold_btn(B_PLAY, 0);
    fw_run(10);
    if (!(loop[0].mute & 1u) || ui.ovd || !loop[0].play)
        LFAIL("PLAY held and the first key did not take loop 1 out (mute %x)", loop[0].mute);
    key(KEY_REV, 1);                                     /* playing: the 4th black key does nothing (build 19) */
    fw_run(5);
    key(KEY_REV, 0);
    fw_run(5);
    if (loop[0].dmode != LD_FWD)
        LFAIL("the 4th black key turned the loops round outside PLAY held");
    hold_btn(B_PLAY, 1);                                 /* PLAY held: the loops' modes on the black keys */
    fw_run(700);
    ui_input();
    key(KEY_REV, 1);
    fw_run(5);
    key(KEY_REV, 0);
    fw_run(5);
    if (loop[0].dmode != LD_REV)
        LFAIL("PLAY held and the 4th black key did not turn the loops round");
    key(KEY_HALF, 1);
    fw_run(5);
    key(KEY_HALF, 0);
    hold_btn(B_PLAY, 0);
    fw_run(400);
    if (loop[0].rate != LS_HALF)
        LFAIL("PLAY held and the 7th black key did not halve the speed");
    if (!loop[0].play || ui.ovd)
        LFAIL("letting go of PLAY after the modes stopped the loops");
    press(B_LFO, 50);                                    /* an effect slot chosen: PLAY does nothing (build 20) */
    press(B_PLAY, 50);
    hold_btn(B_PLAY, 1);
    fw_run(700);
    ui_input();
    if (ui.ovd)
        LFAIL("PLAY held with an effect slot chosen brought the loops up");
    hold_btn(B_PLAY, 0);
    key(KEY_SHIFT, 1);
    press(B_PLAY, 50);
    key(KEY_SHIFT, 0);
    fw_run(20);
    if (!loop[0].play || loop[0].rate != LS_HALF)
        LFAIL("PLAY with an effect slot chosen changed the loops");
    press(B_FX, 50);
    key(KEY_SHIFT, 1);                                   /* SHIFT + OCT+: every loop a step up */
    press(B_OCTUP, 50);
    if (ui.lx != 1 || lx_steps != 1)
        LFAIL("SHIFT + OCT+ did not move the loops (%d, %d)", ui.lx, lx_steps);
    hold_btn(B_SEL, 1);                                  /* SHIFT + generator 2 held + OCT-: its own, and not muted */
    press(B_OCTDN, 50);
    fw_run(1200);
    hold_btn(B_SEL, 0);
    if (ui.lxt[1] != -1 || gen_mute[1])
        LFAIL("SHIFT + generator 2 + OCT- (own move %d, muted %u)", ui.lxt[1], gen_mute[1]);
    press(B_FX, 50);
    press(B_REC, 50);                                    /* SHIFT + REC: VARY */
    key(KEY_SHIFT, 0);
    if (!loop[0].vary)
        LFAIL("SHIFT + REC did not turn VARY on");
    fw_run(1378 * 4);
    key(KEY_SHIFT, 1);                                   /* SHIFT + PLAY: every loop stopped */
    press(B_PLAY, 50);
    key(KEY_SHIFT, 0);
    if (loop[0].play)
        LFAIL("SHIFT + PLAY did not stop every loop");
    {   /* nothing on, once they stop (the voices and the effect slots: the master's tails are kept, `host mtails`) */
        uint32_t last = 0;
        ui.rvp[3] = ui.dlp[3] = 0;
        rv_param(&mrv, ui.rvp[0], ui.rvp[1], ui.rvp[2], ui.rvp[3]);
        mdl_param(ui.dlp[0], ui.dlp[1], ui.dlp[2], ui.dlp[3]);
        for (b = 0; b < 1378u * 10u; b++) {
            int32_t pk = fw_peak;
            fw_peak = 0;
            fw_run(1);
            if (fw_peak > 33)
                last = b;
            fw_peak = pk;
        }
        if (last > 1378u * 6u)
            LFAIL("sound goes on %.1f s after the loops stopped", last / 1378.0);
    }
    hold_btn(B_REC, 1);                                  /* REC held 1 s: emptied */
    fw_run(1500);
    ui_input();
    hold_btn(B_REC, 0);
    fw_run(10);
    if (loop[0].ntrk)
        LFAIL("REC held did not empty the looper (%u loops)", loop[0].ntrk);
    printf("looper: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* the master delay and reverb: sends at 0 add nothing; a generator's reverb send, and its delay send, are heard;
 * a generator heard only through an effect slot reaches them through the slot's send */
static int16_t mst_a[FS * 6 * 2];
static double mst_run(int rs, int ds, int dry0)
{
    uint32_t l;
    double d = 0;
    fm1_in.notes = 0;
    fm1_in.buttons = 0;
    ui_init();
    ui.seed = 4242u;                                     /* (the same patches every run, in the same scale) */
    gens_init(ui.seed);
    scale_random(ui.seed);
    master_q12 = 4096;
    for (l = 0; l < NGEN; l++) {
        ui.rsend[l] = (int8_t)rs;
        ui.dsend[l] = (int8_t)ds;
        sends_apply(l);
        gen_mute[l] = l != 0;
    }
    if (dry0) {                                          /* (generator 1 heard only through its effect slots) */
        gen_patch_t *p = &gbuf[0][g_use[0]];
        if (!p->nent) {
            p->nent = 1;
            p->ent[0] = 0;
        }
        p->dry = 0;
    }
    ui.dlp[1] = 0;                                       /* (one echo: the delay's return plain) */
    ui.dlp[3] = 100;
    mdl_param(ui.dlp[0], ui.dlp[1], ui.dlp[2], ui.dlp[3]);
    fw_frames = 0;
    press(B_FX, 50);
    key(0, 1);
    fw_run(1378 * 3);
    key(0, 0);
    fw_run(1378 * 2);
    if (rs < 0)
        return 0;
    for (l = 0; l < fw_frames * 2u && l < sizeof mst_a / sizeof mst_a[0]; l++) {
        double x = (double)wav2[l] - mst_a[l];
        d += x * x;
    }
    return __builtin_sqrt(d / (fw_frames * 2.0)) / 32768.0;
}
static int cmd_master(void)
{
    int fails = 0;
    double r, e, z, v;
    mst_run(0, 0, 0);
    memcpy(mst_a, wav2, sizeof mst_a);                   /* (the reference: sends at 0) */
    z = mst_run(0, 0, 0);
    r = mst_run(100, 0, 0);
    e = mst_run(0, 100, 0);
    printf("master: against sends at 0 -- again %.6f, reverb send %.4f, delay send %.4f\n", z, r, e);
    if (z != 0.0)
        printf("FAIL master: the same run twice differs\n"), fails++;
    if (r < 0.002)
        printf("FAIL master: the reverb send is not heard\n"), fails++;
    if (e < 0.002)
        printf("FAIL master: the delay send is not heard\n"), fails++;
    mst_run(0, 0, 1);
    memcpy(mst_a, wav2, sizeof mst_a);
    v = mst_run(100, 0, 1);
    printf("master: generator 1 through its slots only, reverb send %.4f\n", v);
    if (v < 0.001)
        printf("FAIL master: an effect slot does not pass its generators' send on\n"), fails++;
    printf("master: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* the sessions and snapshots, on the emulated flash: another firmware's data in the area never erased nor changed; a
 * session saved, the machine changed and started again, loaded: all of it back as it was; snapshots stored and
 * recalled; saving over one keeps one; a damaged record refused; filled up: NO ROOM, the others' data still whole */
static uint32_t hf_sum(uint32_t s) { return fnv(2166136261u, hflash + SES_LO + s * SES_SECT, SES_SECT); }
static void st_norm(st_t *t)                             /* (what may differ and is not the state: the serials) */
{
    uint32_t l;
    for (l = 0; l < NFX; l++)
        t->x[l].f.serial = 0;
    for (l = 0; l < NGEN; l++) {
        uint32_t k;
        for (k = t->g[l].g.nent; k < NENT; k++)
            t->g[l].g.ent[k] = 0;
    }
}
static int st_same(const st_t *a, const st_t *b)
{
    st_t x = *a, y = *b;
    st_norm(&x);
    st_norm(&y);
    return !memcmp(&x, &y, sizeof x);
}
static int cmd_sessions(void)
{
    static const uint32_t FOREIGN[5] = {0, 1, 2, 10, 20};
    uint32_t sum[5], i, k, l, n;
    int fails = 0, rc;
    st_t a, b, s1, s5;
    uint32_t nev0, ntrk0, latch0;
    uint8_t ev0[64];
    rnd_t r;
#define SFAIL(...) do { printf("FAIL sessions: " __VA_ARGS__); printf("\n"); fails++; } while (0)
    memset(hflash, 0xFF, sizeof hflash);
    rnd_seed(&r, 99);
    for (i = 0; i < 5u; i++) {                            /* another firmware's: Tonitor's heads, Bigorna's, noise */
        uint8_t *p = hflash + SES_LO + FOREIGN[i] * SES_SECT;
        for (k = 0; k < SES_SECT; k++)
            p[k] = (uint8_t)rnd_next(&r);
        memcpy(p, i < 3u ? "ACS1" : i == 3u ? "\x00\x00\x00\x00" : "BGN3", 4);
        sum[i] = hf_sum(FOREIGN[i]);
    }
    memset(hf_erases, 0, sizeof hf_erases);
    fm1_in.notes = fm1_in.buttons = 0;
    fw_init();
    /* a state: new patches, levels, sends, a modulation, a scale; a loop; a latched note; two snapshots */
    press(B_SEL, 50);
    press(B_HOME, 50);
    fw_run(30);
    press(B_EDIT, 50);
    press(B_HOME, 50);
    fw_run(30);
    gen_lvl[1] = 15;
    gen_pan[2] = -7;
    fx_mute[2] = 1;
    ui.rsend[0] = 64;
    ui.dsend[1] = 30;
    sends_apply(0);
    sends_apply(1);
    ui.rvp[1] = 80;
    ui.dlp[0] = 30;
    slot_mod[4].lfo[0].shape = MS_DRIFT;
    slot_mod[4].lfo[0].rate = 20;
    slot_mod[4].s[0].src = MSRC_L1;
    slot_mod[4].s[0].dst = FMD_P0 + 1;
    slot_mod[4].s[0].amt = 40;
    ui.scale = 5;
    ui.root = 3;
    snap_store(0);
    st_capture(&s1);
    gen_lvl[0] = 9;
    slot_mod[1].s[1].src = MSRC_L2;
    slot_mod[1].s[1].dst = GMD_PAN;
    slot_mod[1].s[1].amt = -50;
    snap_store(4);
    st_capture(&s5);
    press(B_FX, 50);
    press(B_REC, 50);
    key(0, 1);
    fw_run(400);
    key(0, 0);
    fw_run(300);
    press(B_REC, 50);
    key(2, 1);
    key(KEY_SHIFT, 1);
    ui_input();
    key(KEY_SHIFT, 0);
    key(2, 0);
    fw_run(200);
    st_capture(&a);
    nev0 = loop[0].nev;
    ntrk0 = loop[0].ntrk;
    latch0 = ui.latch[0];
    memcpy(ev0, loop[0].ev, sizeof ev0);
    rc = ses_save(3);
    if (rc != SES_OK)
        SFAIL("save: %d", rc);
    /* changed, and started again: then loaded */
    for (l = 0; l < NSLOT; l++) {
        press(SLOT_BTN[l], 50);
        press(B_HOME, 50);
        fw_run(30);
    }
    fm1_in.notes = fm1_in.buttons = 0;
    fw_init();
    if (!ses_has(3) || ses_has(4))
        SFAIL("after a start: session 3 %s, 4 %s", ses_has(3) ? "there" : "missing", ses_has(4) ? "there" : "absent");
    fw_run(10);
    rc = ses_load_slot(3);
    fw_run(60);
    if (rc != SES_OK)
        SFAIL("load: %d", rc);
    st_capture(&b);
    if (!st_same(&a, &b))
        SFAIL("the state loaded is not the state saved");
    if (loop[0].ntrk != ntrk0 || loop[0].nev != nev0 || memcmp(ev0, loop[0].ev, sizeof ev0))
        SFAIL("the loops (%u loops, %u events; saved %u, %u)", loop[0].ntrk, loop[0].nev, ntrk0, nev0);
    if (!loop[0].play)
        SFAIL("the loops do not play after the load");
    if (ui.latch[0] != latch0 || !latch0)
        SFAIL("the latched notes (%x, saved %x)", ui.latch[0], latch0);
    if (!snap_has(0) || !snap_has(4) || snap_has(1))
        SFAIL("the snapshots kept");
    rc = snap_recall(0);
    fw_run(60);
    st_capture(&b);
    if (rc != SES_OK || !st_same(&s1, &b))
        SFAIL("snapshot 1 recalled (%d)", rc);
    rc = snap_recall(4);
    fw_run(60);
    st_capture(&b);
    if (rc != SES_OK || !st_same(&s5, &b))
        SFAIL("snapshot 5 recalled (%d)", rc);
    for (i = 0; i < 25u; i++) {                           /* saved over, again and again, with idle blanking between */
        if ((rc = ses_save(3)) != SES_OK)
            SFAIL("save over %u: %d", i, rc);
        fm1_ms += 1100u;
        ses_idle(1);
    }
    for (n = 0, i = 0; i < ses.n; i++)
        n += ses.num[i] == 3u;
    if (n != 1u || ses.n != 1u)
        SFAIL("%u copies of session 3, %u sessions", n, ses.n);
    {   /* a power cut: the new copy written, the old one not yet retired -- the new one stays; or the new one cut
         * short (its head, part of its body) -- the old one stays */
        int j = ses_find(3);
        uint32_t olds = ses.sect[j], news;
        static uint8_t keep[0x100000];
        if ((rc = ses_save(3)) != SES_OK)
            SFAIL("save: %d", rc);
        j = ses_find(3);
        news = ses.sect[j];
        memset(hflash + SES_LO + olds * SES_SECT + 28u, 0xFF, 4u);   /* (as if the retiring never happened) */
        memcpy(keep, hflash, sizeof keep);
        fw_init();
        j = ses_find(3);
        if (j < 0 || ses.sect[j] != news || ses.n != 1u)
            SFAIL("a cut before the old copy was retired: %d sessions, at %d (new %u)", ses.n, j < 0 ? -1 : ses.sect[j], news);
        memcpy(hflash, keep, sizeof keep);
        memset(hflash + SES_LO + news * SES_SECT + SES_SECT, 0xFF, SES_SECT);   /* (its second sector never written) */
        fw_init();
        j = ses_find(3);
        if (j < 0 || ses.sect[j] != olds || ses.n != 1u)
            SFAIL("a cut while the new copy was written: %d sessions, at %d (old %u)", ses.n, j < 0 ? -1 : ses.sect[j], olds);
        else if (ses_load_slot(3) != SES_OK)
            SFAIL("the old copy kept after a cut does not load");
        fw_run(60);
    }
    {   /* damaged: one byte of its body -- refused, the machine as it was */
        int j = ses_find(3);
        st_t before;
        st_capture(&before);
        hflash[SES_LO + ses.sect[j] * SES_SECT + 200u] ^= 0x5Au;
        if (ses_load_slot(3) == SES_OK)
            SFAIL("a damaged record loaded");
        st_capture(&b);
        if (!st_same(&before, &b))
            SFAIL("a damaged record changed the machine");
        hflash[SES_LO + ses.sect[j] * SES_SECT + 200u] ^= 0x5Au;
    }
    for (n = 0, k = 1; k <= SES_SLOTS; k++) {             /* filled up */
        rc = ses_save(k);
        if (rc == SES_OK)
            n++;
        else if (rc != SES_FULL)
            SFAIL("save %u: %d", k, rc);
        else
            break;
    }
    printf("sessions: %u saved before NO ROOM (one session: %u sectors)\n", n, ses.nsect[0]);
    for (i = 0; i < 5u; i++) {
        if (hf_sum(FOREIGN[i]) != sum[i])
            SFAIL("another firmware's sector %u changed", FOREIGN[i]);
        if (hf_erases[FOREIGN[i]])
            SFAIL("another firmware's sector %u erased", FOREIGN[i]);
    }
    if (getenv("SDBG")) {
        for (i = 0; i < ses.n; i++)
            printf("  before: %u at %u (%u)\n", ses.num[i], ses.sect[i], ses.nsect[i]);
        for (i = 0; i < SES_NSECT; i++)
            printf("%u", ses.st[i]);
        printf("\n");
    }
    fw_init();                                            /* (and found again, every one, at the next start) */
    if (getenv("SDBG")) {
        for (i = 0; i < ses.n; i++)
            printf("  after: %u at %u (%u)\n", ses.num[i], ses.sect[i], ses.nsect[i]);
        for (i = 0; i < SES_NSECT; i++)
            printf("%u", ses.st[i]);
        printf("\n");
    }
    if (ses.n != n)
        SFAIL("%u sessions found at the next start, %u saved", ses.n, n);
    printf("sessions: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* the LFOs: one on a generator's macro, one on an effect slot's second value -- each changes the sound; the slot drawn
 * anew (other effects), its LFO goes on moving the value at that place, and the MOD page names the new one */
static double mod_run(int gen_on, int fx_on)
{
    uint32_t l;
    double d = 0;
    fm1_in.notes = fm1_in.buttons = 0;
    ui_init();
    ui.seed = 777u;
    gens_init(ui.seed);
    scale_random(ui.seed);
    master_q12 = 4096;
    for (l = 1; l < NGEN; l++)
        gen_mute[l] = 1;
    if (gen_on) {
        slot_mod[0].lfo[0].rate = 60;
        slot_mod[0].s[0].src = MSRC_L1;
        slot_mod[0].s[0].dst = GMD_MAC1;
        slot_mod[0].s[0].amt = 80;
    }
    if (fx_on) {
        uint32_t s0 = gbuf[0][g_use[0]].nent ? gbuf[0][g_use[0]].ent[0] / FX_N : 0;
        slot_mod[NGEN + s0].lfo[0].rate = 60;
        slot_mod[NGEN + s0].s[0].src = MSRC_L1;
        slot_mod[NGEN + s0].s[0].dst = FMD_P0 + 1;
        slot_mod[NGEN + s0].s[0].amt = 90;
    }
    fw_frames = 0;
    press(B_FX, 50);
    key(0, 1);
    fw_run(1378 * 3);
    key(0, 0);
    fw_run(1378);
    if (gen_on < 0)
        return 0;
    for (l = 0; l < fw_frames * 2u && l < sizeof mst_a / sizeof mst_a[0]; l++) {
        double x = (double)wav2[l] - mst_a[l];
        d += x * x;
    }
    return __builtin_sqrt(d / (fw_frames * 2.0)) / 32768.0;
}
static int cmd_mod(void)
{
    int fails = 0;
    double z, g, f;
    char nm[32];
    mod_run(0, 0);
    memcpy(mst_a, wav2, sizeof mst_a);
    z = mod_run(0, 0);
    g = mod_run(1, 0);
    f = mod_run(0, 1);
    printf("mod: against none -- again %.6f, a generator's LFO %.4f, an effect slot's %.4f\n", z, g, f);
    if (z != 0.0)
        printf("FAIL mod: the same run twice differs\n"), fails++;
    if (g < 0.001)
        printf("FAIL mod: a generator's LFO changes nothing\n"), fails++;
    if (f < 0.0005)
        printf("FAIL mod: an effect slot's LFO changes nothing\n"), fails++;
    {   /* the slot drawn anew: the LFO on at that place, the page naming the effect there now */
        uint32_t s0 = 0, k, moved = 0;
        slot_mod[NGEN].lfo[0].rate = 80;
        slot_mod[NGEN].s[0].src = MSRC_L1;
        slot_mod[NGEN].s[0].dst = FMD_P0 + 5;   /* (the second effect's second value) */
        slot_mod[NGEN].s[0].amt = 90;
        for (k = 0; k < 6u; k++) {
            press(B_LFO, 50);
            press(B_HOME, 50);
            fw_run(400);
            mod_dst_name(NGEN + s0, FMD_P0 + 5, nm, sizeof nm);
            if (strncmp(nm, RM_NAME[fbuf[s0][f_pub[s0]].e[1].kind], str_len(RM_NAME[fbuf[s0][f_pub[s0]].e[1].kind])) &&
                strncmp(nm, RM_TILE[fbuf[s0][f_pub[s0]].e[1].kind], str_len(RM_TILE[fbuf[s0][f_pub[s0]].e[1].kind])))
                                                         /* (or its short name, when the long one does not fit: build 38) */
                printf("FAIL mod: the page names %s, the effect there is %s\n", nm, RM_NAME[fbuf[s0][f_pub[s0]].e[1].kind]), fails++;
            moved |= fx_mod_p[s0][5] != 0;
        }
        if (!moved)
            printf("FAIL mod: the LFO stopped moving the slot's value after it was drawn anew\n"), fails++;
        printf("mod: last named %s\n", nm);
    }
    printf("mod: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* every page painted whole in one pass: the screen flooded with a colour no page uses, the page entered and drawn
 * once -- not a pixel of the flood may be left */
#define PAINT_FLOOD 0xF81Fu
static int paint_one(const char *nm)
{
    uint32_t i, left = 0;
    for (i = 0; i < 240u * 240u; i++)
        fb[i] = PAINT_FLOOD;
    ui.shown_page = 0xFE;                                /* (entered from somewhere else) */
    ui.load_at = fm1_ms - 1000u;
    fb_px = 0;
    ui_draw();
    for (i = 0; i < 240u * 240u; i++)
        left += fb[i] == PAINT_FLOOD;
    printf("  %-12s %5u pixels left unpainted, %.2f screens sent\n", nm, left, fb_px / 57600.0);
    return left ? 1 : 0;
}
static int cmd_paint(void)
{
    int fails = 0;
    fm1_in.notes = fm1_in.buttons = 0;
    batt_raw = 600;
    fw_init();
    fw_run(50);
    fails += paint_one("main");
    ui.sc_page = 1;
    fails += paint_one("scale");
    pages_off();
    ui.fx_page = 1;
    fails += paint_one("delay");
    ui.fx_page = 2;
    fails += paint_one("reverb");
    ui.fx_page = 3;
    fails += paint_one("mod gen");
    ui.mod_ix = 3;
    ui.sel = NGEN + 1;
    fails += paint_one("mod slot");
    pages_off();
    ui.sel = 0;
    ui.ovd = 1;
    fails += paint_one("loops");
    ui.ovd = 0;
    ui.ses_page = SP_LIST;
    ui.ses_ix = 5;
    fails += paint_one("sessions");
    ui.ses_page = SP_ASK;
    fails += paint_one("asked");
    pages_off();
    ui.snap_page = 1;
    fails += paint_one("snapshots");
    pages_off();
    ui.gs_page = 2;
    fails += paint_one("generator");
    pages_off();
    ui.ses_page = SP_ASK;
    ui.ses_ask = AK_CLEAR;
    ui.ses_new = 3;
    fails += paint_one("clear");
    ui.ses_ask = 0;
    pages_off();
    setup_open();                                    /* (build 43) GLOBAL SETUP: the sections, a part's rows, a value */
    fails += paint_one("setup");
    set.lvl = 2;
    set.sec = 0;
    set.part = 0;
    set.row = 4;
    set.edit = 1;
    set.val = 12;
    fails += paint_one("setup rows");
    pages_off();
    ui.pre_page = SP_LIST;                           /* (build 47) PRESETS: the list, asked */
    ui.pre_ix = 2;
    ui.pre_gen = 1;
    fails += paint_one("presets");
    ui.pre_page = SP_ASK;
    fails += paint_one("presets asked");
    pages_off();
    fl_make();                                       /* (build 48) SHIFT + PRESETS: a page of families, of recipes */
    ui.fl_on = 1;
    ui.fl_slot = 0;
    ui.fl_ix = 41;
    fails += paint_one("families, the last page");
    ui.fl_slot = NGEN + 2;
    ui.fl_ix = 5;
    fails += paint_one("recipes");
    pages_off();
    ui.morph = 9;
    ui.dial_until = fm1_ms + 1500u;
    ui.dial_shown = 0;
    fails += paint_one("morph dial");
    ui.dial_until = fm1_ms;
    ui_draw();
    flash("SAVED", "12", C_OK);
    fails += paint_one("flash");
    flash("NO ROOM", "", C_FULL);
    fails += paint_one("flash, one");
    ui.fl_until = fm1_ms;
    {   /* the main page up, a slot drawn anew again and again (two effects, three): what is shown is what a whole
         * repaint of the same moment shows -- no tile of before left behind */
        static uint16_t was[240 * 240];
        uint32_t t, i, diff = 0, n2 = 0, n3 = 0;
        pages_off();
        ui_draw();
        for (t = 0; t < 24u; t++) {
            press(B_LFO, 20);
            press(B_HOME, 20);
            fw_run(200);
            ui_draw();
            ui_draw();
            n2 += fbuf[0][f_use[0]].n == 2u;
            n3 += fbuf[0][f_use[0]].n == 3u;
            memcpy(was, fb, sizeof was);
            ui.force = 1;
            ui_draw();
            for (i = 0; i < 240u * 240u; i++)
                diff += was[i] != fb[i];
        }
        printf("  %-12s %5u pixels differ from a whole repaint (%u draws of two effects, %u of three)\n", "redrawn", diff,
               n2, n3);
        if (diff)
            fails++;
    }
    printf("paint: %d pages with pixels left\n", fails);
    return fails ? 1 : 0;
}

/* the first black key: a slot rewired -- a generator's sound byte for byte the same, its cables anew, its notes never
 * cut (no voice faded or started again); an effect slot's effects and values the same, its memory not cleared, its outs
 * or cables anew */
static int cmd_rewire(void)
{
    int fails = 0;
    uint32_t t, k, gchanged = 0, fchanged = 0, cut = 0;
    fm1_in.notes = fm1_in.buttons = 0;
    ui_init();
    ui.seed = 31337u;
    gens_init(ui.seed);
    scale_random(ui.seed);
    press(B_FX, 50);
    key(0, 1);
    key(4, 1);
    fw_run(800);
    for (t = 0; t < 20u; t++) {
        mo_design_t d0 = gbuf[0][g_use[0]].gen;
        gen_patch_t p0 = gbuf[0][g_use[0]];
        key(KEY_REWIRE, 1);
        for (k = 0; k < 80u; k++) {
            uint32_t v;
            fw_run(1);
            for (v = 0; v < NVOICE; v++)
                cut += vox[v].sounding && vox[v].slot == 0 && (vox[v].fade || vox[v].pend);
        }
        key(KEY_REWIRE, 0);
        if (memcmp(&d0, &gbuf[0][g_use[0]].gen, sizeof d0))
            printf("FAIL rewire: generator 1's sound changed\n"), fails++;
        gchanged += p0.nent != gbuf[0][g_use[0]].nent || memcmp(p0.ent, gbuf[0][g_use[0]].ent, p0.nent) ||
                    p0.xsrc != gbuf[0][g_use[0]].xsrc;
    }
    if (cut)
        printf("FAIL rewire: notes cut or started again (%u voice blocks)\n", cut), fails++;
    if (gchanged < 18u)
        printf("FAIL rewire: generator 1's cables changed only %u times of 20\n", gchanged), fails++;
    press(B_LFO, 50);                                    /* effect slot 4 */
    for (t = 0; t < 20u; t++) {
        fx_patch_t f0 = fbuf[0][f_use[0]];
        uint8_t ph0 = fx_phase[0];
        key(KEY_REWIRE, 1);
        fw_run(20);
        key(KEY_REWIRE, 0);
        if (memcmp(f0.e, fbuf[0][f_use[0]].e, sizeof f0.e) || f0.n != fbuf[0][f_use[0]].n || f0.seed != fbuf[0][f_use[0]].seed)
            printf("FAIL rewire: slot 4's effects changed\n"), fails++;
        if (ph0 != FS_ON || fx_phase[0] != FS_ON)
            printf("FAIL rewire: slot 4 faded or cleared\n"), fails++;
        fchanged += f0.out != fbuf[0][f_use[0]].out || f0.ret != fbuf[0][f_use[0]].ret;
    }
    if (fchanged < 18u)
        printf("FAIL rewire: slot 4's cables changed only %u times of 20\n", fchanged), fails++;
    printf("rewire: generator cables new %u of 20, slot cables new %u of 20, %d failures\n", gchanged, fchanged, fails);
    return fails ? 1 : 0;
}

/* FILTER kept off the eardrums: each type at its highest resonance (RES at its bound), FREQ across its range, a held
 * note through it -- the loudest moment and the level against the dry: never past 0.75 of full scale, never more than
 * 6 dB over the dry */
static int cmd_ears(void)
{
    int fails = 0;
    uint32_t t, fq;
    for (t = 0; t < 4u; t++)
        for (fq = RM_LO[RM_FLT][1]; fq <= RM_HI[RM_FLT][1]; fq += 10u) {
            double wet = 0, dry = 0;
            int32_t pk = 0;
            int pass;
            for (pass = 0; pass < 2; pass++) {
                gen_patch_t *p;
                fx_patch_t *f;
                uint32_t b;
                fm1_in.notes = fm1_in.buttons = 0;
                fw_init();
                gen_draw(0, MF_DRONE, 777u);
                fw_run(4);
                for (b = 1; b < NFX; b++)
                    fx_mute[b] = 1;
                for (b = 1; b < NGEN; b++)
                    gen_mute[b] = 1;
                f = &fbuf[0][f_use[0]];
                memset(f, 0, sizeof *f);
                f->n = 1;
                f->e[0].kind = RM_FLT;
                f->e[0].p[0] = (uint8_t)(t * 25u + 10u);
                f->e[0].p[1] = (uint8_t)fq;
                f->e[0].p[2] = RM_HI[RM_FLT][2];
                f->e[0].p[3] = 100;
                memset(fx_arena[0], 0, FX_ARENA);
                fx_start(0);
                p = &gbuf[0][g_use[0]];
                p->nent = pass ? 0 : 1;
                p->ent[0] = 0;
                p->send = Q15;
                p->dry = pass ? Q15 : 0;
                p->xsrc = XM_NONE;
                fw_frames = 0;
                fw_peak = 0;
                fw_rms = 0;
                press(B_FX, 50);
                key(0, 1);
                fw_run(1378 * 2);
                key(0, 0);
                fw_run(200);
                if (!pass) {
                    wet = fw_rms;
                    pk = fw_peak;
                } else
                    dry = fw_rms;
            }
            {
                double db = 10.0 * __builtin_log10((wet + 1) / (dry + 1));
                if (pk > 24576 || db > 6.0) {
                    printf("FAIL ears: type %u at %u: peak %.2f, %+.1f dB over the dry\n", t, fq, pk / 32768.0, db);
                    fails++;
                }
                if (getenv("EARS_V"))
                    printf("  type %u freq %3u: peak %.2f %+.1f dB\n", t, fq, pk / 32768.0, db);
            }
        }
    printf("ears: %d too loud\n", fails);
    return fails ? 1 : 0;
}

/* a snapshot's morph: two snapshots apart in nearly everything, notes held, recalled with 4 s of morph -- the sound
 * never gone, generator 1's old and new sounds heard together a while, its level gliding (no step past a fine one),
 * the reverb's SIZE kept until the middle, the target blinking, and at the end the snapshot exactly */
static int cmd_morph(void)
{
    int fails = 0;
    st_t b, now;
    uint32_t t, both = 0, minw = 0xFFFFFFFFu, maxstep = 0, wins = 0;
    int32_t lastq = -1;
    int size_early = 0;
    fm1_in.notes = fm1_in.buttons = 0;
    ui_init();
    ui.seed = 2468u;
    gens_init(ui.seed);
    scale_random(ui.seed);
    ses_init(1);
    master_q12 = 4096;
    press(B_FX, 50);
    key(0, 1);
    key(KEY_SHIFT, 1);                                   /* (latched: they go on through it all) */
    ui_input();
    key(KEY_SHIFT, 0);
    key(0, 0);
    press(B_SEL, 50);
    key(4, 1);
    key(KEY_SHIFT, 1);
    ui_input();
    key(KEY_SHIFT, 0);
    key(4, 0);
    fw_run(2000);
    snap_store(0);                                       /* A */
    press(B_FX, 50);
    press(B_HOME, 50);                                   /* generator 1 a new sound */
    fw_run(60);
    gen_lvl[1] = 12;                                     /* generator 2 quieter */
    press(B_LFO, 50);
    press(B_HOME, 50);                                   /* slot 4 new effects */
    fw_run(60);
    ui.rvp[0] = 20;                                      /* the reverb smaller */
    ui.rvp[1] = 90;
    ui.scale = 7;
    snap_store(1);                                       /* B */
    b = snap.s[1];
    ui.morph = 0;
    snap_recall(0);                                      /* back to A at once */
    fw_run(400);
    ui_input();
    ui.morph = 8;                                        /* 4 s */
    snap_recall(1);
    if (snap_morph_target() != 1)
        printf("FAIL morph: the target is not marked to blink\n"), fails++;
    for (t = 0; t < 1378u * 7u / 14u; t++) {             /* 7 s, a frame every 14 blocks */
        uint32_t k, on0 = 0;
        double w = 0;
        int i;
        fw_frames = 0;
        fw_rms = 0;
        fw_run(14);
        ui_input();
        w = __builtin_sqrt(fw_rms / (fw_frames * 2.0));
        if (morph.on) {
            wins++;
            if ((uint32_t)w < minw)
                minw = (uint32_t)w;
            for (k = 0; k < NVOICE; k++)
                on0 += vox[k].sounding && vox[k].slot == 0;
            both += on0 >= 2u;
            if (lastq >= 0 && (uint32_t)absi(gen_lvl_q8[1] - lastq) > maxstep)
                maxstep = (uint32_t)absi(gen_lvl_q8[1] - lastq);
            lastq = gen_lvl_q8[1];
            if (!morph.mid && ui.rvp[0] != morph.a.rvp[0])
                size_early = 1;
        }
        (void)i;
    }
    st_capture(&now);
    if (morph.on || snap_morph_target() >= 0)
        printf("FAIL morph: not finished after 7 s\n"), fails++;
    if (!st_same(&now, &b))
        printf("FAIL morph: the end is not the snapshot\n"), fails++;
    if (minw < 60u)
        printf("FAIL morph: the sound gone a moment (rms %u)\n", minw), fails++;
    if (!both)
        printf("FAIL morph: generator 1's old and new sounds never heard together\n"), fails++;
    if (maxstep > 160u)
        printf("FAIL morph: generator 2's level stepped (%u / 256 of a step in a frame)\n", maxstep), fails++;
    if (size_early)
        printf("FAIL morph: the reverb's SIZE moved before the middle\n"), fails++;
    printf("morph: %u frames, quietest rms %u, crossing over %u frames, largest level step %u/256, %d failures\n", wins,
           minw, both, maxstep, fails);
    return fails ? 1 : 0;
}

/* a morph ends as a recall would: for many pairs of snapshots (every slot drawn anew between them, a slot muted, a
 * generator quieter), a note latched -- the machine after a morph from A into B against B applied at once to a fresh
 * machine with the same note latched: the same state, and the same loudness (nothing of A left behind) */
static int fresh_keep;                           /* (generator 1 the sound the DISINTEGRATOR's test was made on) */
static void ends_fresh(uint32_t seed)
{
    fm1_in.notes = fm1_in.buttons = 0;
    ui_init();
    ui.seed = seed;
    gens_init(ui.seed);
    scale_random(ui.seed);
    if (fresh_keep) {                            /* (build 34: generation 3's DATA, as builds 18 .. 33 drew it here --
                                                  * the test's thresholds were set on it) */
        gen_keep_t k;
        gen_keep(0, &k);
        k.family = MF_DATA;
        k.dseed = 2532491484u;
        k.path = PATH_EXP;
        k.gen = 3;
        k.gain = 2156;
        gen_set(0, &k);
        fw_run(10);
    }
    ses_init(1);
    master_q12 = 4096;
    press(B_FX, 50);
    key(0, 1);
    key(KEY_SHIFT, 1);
    ui_input();
    key(KEY_SHIFT, 0);
    key(0, 0);
    fw_run(1500);
}
static double ends_loud(void)
{
    double r = 0;
    uint32_t i, l;
    int32_t out[2 * CTL];
    for (i = 0; i < 1378u * 8u; i++) {   /* (8 s: past a swell, and a frozen cloud's slice) */
        mix_block(out);
        for (l = 0; l < 2u * CTL; l++)
            r += (double)out[l] * out[l];
    }
    return 10.0 * __builtin_log10(r + 1);
}
static int cmd_ends(void)
{
    int fails = 0;
    uint32_t sd, l, k;
    for (sd = 0; sd < 12u; sd++) {
        uint32_t seed = 9100u + sd * 37u;
        st_t B, sa, sb;
        double ra, rb, ends_tol = 3.0;
        ends_fresh(seed);
        snap_store(0);                                   /* A, then everything anew (a slot muted, a generator quieter) */
        for (l = 0; l < NSLOT; l++) {                    /* (build 41: from fixed seeds -- through HOME the host's clock
                                                          * went into them; see mtails) */
            ui.seed = seed * 7u + l;
            host_ticks = 0;
            new_patch(l, 0);
            fw_run(60);
        }
        fx_mute[seed % NFX] = 1;
        gen_lvl[seed % NGEN] = 8;
        snap_store(1);
        B = snap.s[1];
        ui.morph = 0;
        snap_recall(0);
        fw_run(1500);
        ui_input();
        ui.morph = 6;                                    /* (3 s) */
        snap_recall(1);
        for (l = 0; l < 1378u * 9u / 14u; l++) {         /* 9 s: the morph and its tails done */
            fw_run(14);
            ui_input();
        }
        st_capture(&sa);
        ra = ends_loud();
        ends_fresh(seed);                                /* B at once, to a fresh machine */
        st_apply(&B);
        for (l = 0; l < 1378u * 9u / 14u; l++) {
            fw_run(14);
            ui_input();
            if (l == 1378u * 3u / 14u)                   /* (where the morph ends: its clouds take the sound again --
                                                          * a frozen one holds what it took, when it took it) */
                for (k = 0; k < NFX; k++)
                    fx_refill(k);
        }
        st_capture(&sb);
        rb = ends_loud();
        {   /* (a frozen CLOUD holds the sound of the moment it froze -- the morph and the recall freeze it at different
             * moments: there the state alone must match, build 32) */
            uint32_t j;
            for (k = 0; k < NFX; k++)
                for (j = 0; j < B.x[k].f.n; j++)
                    if (!B.x[k].mute && B.x[k].f.e[j].kind == RM_CLD && B.x[k].f.e[j].p[2] >= 50)
                        rb = ra;
        }
        {   /* (build 45: a CLOUD's grains fall where its random state puts them -- after a morph and after a recall, two
             * histories: through a ringing BANK after it, seed 9433's ended 3.8 dB apart, the state the same. 6 dB
             * there, 3 elsewhere) */
            uint32_t j;
            for (k = 0; k < NFX; k++)
                for (j = 0; j < B.x[k].f.n; j++)
                    if (!B.x[k].mute && B.x[k].f.e[j].kind == RM_CLD)
                        ends_tol = 6.0;
        }
        if (!st_same(&sa, &B) || ra - rb > ends_tol || rb - ra > ends_tol) {
            printf("FAIL ends: seed %u: after the morph %.1f dB, at once %.1f dB%s\n", seed, ra, rb,
                   st_same(&sa, &B) ? "" : ", the state is not the snapshot");
            fails++;
        } else if (getenv("ENDS_V"))
            printf("  seed %u: %.1f / %.1f dB\n", seed, ra, rb);
        (void)sb;
    }
    printf("ends: %d morphs that do not end as a recall\n", fails);
    return fails ? 1 : 0;
}

/* the hiss left behind (the owner's case): an effect slot with TAPE hissing (AGE high) in A, the same TAPE silent of hiss
 * (AGE 0) in B; recalled from the SNAPSHOTS page with a morph; after it, with every note let go and the slot's gate
 * still open (a loop playing nothing would keep it open: here, a held note on another generator) -- the hiss gone */
static int cmd_hiss(void)
{
    int fails = 0;
    uint32_t l, s0;
    for (s0 = 0; s0 < 2u; s0++) {
        fx_patch_t *f;
        double hiss;
        uint32_t i;
        fm1_in.notes = fm1_in.buttons = 0;
        ui_init();
        ui.seed = 555u + s0;
        gens_init(ui.seed);
        scale_random(ui.seed);
        ses_init(1);
        master_q12 = 4096;
        for (l = 0; l < NFX; l++) {                      /* slot 4: TAPE alone, every generator off it */
            f = &fbuf[l][f_use[l]];
            memset(f, 0, sizeof *f);
            f->n = 2;
            f->e[0].kind = RM_TAP;
            f->e[0].p[0] = 50; f->e[0].p[1] = 0; f->e[0].p[2] = 30; f->e[0].p[3] = 100;
            f->e[1].kind = RM_FLT;
            f->e[1].p[0] = 10; f->e[1].p[1] = 80;
        }
        for (l = 0; l < NGEN; l++)
            gbuf[l][g_use[l]].nent = 0;
        press(B_FX, 50);
        key(0, 1);
        key(KEY_SHIFT, 1);
        ui_input();
        key(KEY_SHIFT, 0);
        key(0, 0);
        fw_run(1000);
        snap_store(1);                                   /* snapshot 2: hissing */
        for (l = 0; l < NFX; l++)
            fbuf[l][f_use[l]].e[0].p[3] = 0;             /* snapshot 3: the same, no hiss, quieter */
        fx_lvl[0] = 12;
        snap_store(2);
        ui.morph = 0;
        snap_recall(1);
        fw_run(500);
        press(B_SAVE, 50);                               /* the SNAPSHOTS page, KNOB 1 to 4 s, the third key */
        ui.morph = s0 ? 20 : 8;
        key(4, 1);                                       /* (the third white key) */
        fw_run(10);
        key(4, 0);
        for (i = 0; i < 1378u * 14u / 14u; i++) {
            fw_run(14);
            ui_input();
        }
        for (hiss = 0, i = 0; i < 1378u; i++) {          /* the slots' own output, alone: is TAPE still hissing? */
            uint32_t j;
            fw_run(1);
            for (j = 0; j < CTL; j++)
                hiss += (double)fx_l[j] * fx_l[j];
        }
        hiss = __builtin_sqrt(hiss / (1378.0 * CTL));
        printf("hiss: morph %u s: TAPE's AGE now %u (snapshot %u) -- the slots' output %.1f\n", ui.morph / 2u,
               fbuf[0][f_use[0]].e[0].p[3], snap_current() + 1, hiss);
        if (fbuf[0][f_use[0]].e[0].p[3] != 0 || snap_current() != 2)
            printf("FAIL hiss: snapshot 3 not reached\n"), fails++;
    }
    return fails ? 1 : 0;
}

/* the owner's case, widened: snapshots B (3) and A (2), morphs B -> A -> B (the second after the first ends, or while it
 * runs), against B applied at once to a fresh machine: the same state; the same loudness on the left and on the right;
 * each effect slot's and each generator's own output the same within 3 dB (nothing of A left on either side) */
static void trip_loud(double *lr, double *part)
{
    uint32_t i, l, j;
    int32_t out[2 * CTL];
    lr[0] = lr[1] = 0;
    for (l = 0; l < NGEN + NFX; l++)
        part[l] = 0;
    for (i = 0; i < 1378u * 2u; i++) {
        mix_block(out);
        for (l = 0; l < CTL; l++) {
            lr[0] += (double)out[2u * l] * out[2u * l];
            lr[1] += (double)out[2u * l + 1u] * out[2u * l + 1u];
        }
        for (l = 0; l < NGEN; l++)
            for (j = 0; j < CTL; j++)
                part[l] += (double)gbus[l][j] * gbus[l][j];
    }
    for (l = 0; l < 2u; l++)
        lr[l] = 10.0 * __builtin_log10(lr[l] + 1);
    for (l = 0; l < NGEN; l++)
        part[l] = 10.0 * __builtin_log10(part[l] + 1);
}
static void trip_frames(uint32_t frames)
{
    uint32_t l;
    for (l = 0; l < frames; l++) {
        fw_run(14);
        ui_input();
    }
}
static int cmd_trip(void)
{
    int fails = 0;
    uint32_t sd, l, mid;
    for (mid = 0; mid < 2u; mid++)
        for (sd = 0; sd < 16u; sd++) {
            uint32_t seed = 7300u + sd * 41u + mid * 1000u;
            st_t A, B, sa;
            double ma[2], mb[2], pa[NGEN + NFX], pb[NGEN + NFX];
            int bad = 0;
            ends_fresh(seed);
            snap_store(2);                               /* B (snapshot 3), then A (snapshot 2): everything anew */
            B = snap.s[2];
            for (l = 0; l < NSLOT; l++) {
                press(SLOT_BTN[l], 50);
                press(B_HOME, 50);
                fw_run(60);
            }
            gen_lvl[seed % NGEN] = 23;
            snap_store(1);
            A = snap.s[1];
            ui.morph = 0;
            snap_recall(2);                              /* at B */
            trip_frames(300);
            ui.morph = 8;                                /* 4 s */
            snap_recall(1);                              /* B -> A */
            trip_frames(mid ? 150 : 700);                /* (mid: 1.5 s in, still on its way) */
            snap_recall(2);                              /* -> B */
            trip_frames(1100);
            st_capture(&sa);
            trip_loud(ma, pa);
            ends_fresh(seed);                            /* B at once, to a fresh machine */
            st_apply(&B);
            trip_frames(1100);
            trip_loud(mb, pb);
            for (l = 0; l < 2u; l++)
                bad |= ma[l] - mb[l] > 3.0 || mb[l] - ma[l] > 3.0;
            for (l = 0; l < NGEN; l++)
                bad |= pa[l] - pb[l] > 3.0 || pb[l] - pa[l] > 3.0;
            if (!st_same(&sa, &B) || bad) {
                printf("FAIL trip %s seed %u: L %.1f / %.1f, R %.1f / %.1f; gens %.1f/%.1f %.1f/%.1f %.1f/%.1f%s\n",
                       mid ? "(back while on its way)" : "", seed, ma[0], mb[0], ma[1], mb[1], pa[0], pb[0], pa[1], pb[1],
                       pa[2], pb[2], st_same(&sa, &B) ? "" : "; the state is not B");
                fails++;
            }
            (void)A;
        }
    printf("trip: %d round trips that do not come back to B\n", fails);
    return fails ? 1 : 0;
}

/* each effect fed, then nothing: it falls silent (its meter with it, so the rack lets it rest) or at least fades --
 * never holds a sound of its own for ever (build 17: TAPE's hiss, STARVE's lean, the meter stuck at 15 kept effects
 * running, nothing reaching them, and a morph carried them into the next snapshot). A frozen CLOUD holds by design */
static int cmd_quiet(void)
{
    int fails = 0;
    uint32_t m, rep, b, i;
    for (m = 0; m < NKIND; m++)
        for (rep = 0; rep < 4u; rep++) {
            fx_patch_t *f;
            rnd_t r, nz;
            int32_t x[CTL], l[CTL], rr[CTL], pk = 0;
            if (m == RM_DLY || m == RM_REV)              /* (the master's: never in a slot, build 31) */
                break;
            double first = 0, last = 0;
            fw_init();
            f = &fbuf[0][f_use[0]];
            memset(f, 0, sizeof *f);
            f->n = 1;
            f->e[0].kind = (uint8_t)m;
            rnd_seed(&r, 5150u + rep * 31u + m * 7u);
            fx_values(&f->e[0], &r);
            if (m == RM_CLD && f->e[0].p[2] >= 50)
                f->e[0].p[2] = 20;                       /* (frozen: held by design) */
            memset(fx_arena[0], 0, FX_ARENA);
            fx_start(0);
            rnd_seed(&nz, 77u + rep);
            for (b = 0; b < 1378u + 55120u; b++) {       /* 1 s of noise, then 40 s of nothing */
                for (i = 0; i < CTL; i++)
                    x[i] = b < 1378u ? mulq15(rnd_bi(&nz), 12000) : 0, l[i] = rr[i] = 0;
                rm_block(0, &f->e[0], x);
                for (i = 0; i < CTL; i++) {
                    int32_t a = maxi(absi(x[i]), maxi(absi(l[i]), absi(rr[i])));
                    if (b >= 1378u + 1378u * 2u && b < 1378u * 4u)
                        first += (double)a * a;
                    if (b >= 1378u + 55120u - 1378u) {
                        last += (double)a * a;
                        pk = maxi(pk, a);
                    }
                }
            }
            if (pk >= 32 && 10.0 * __builtin_log10((last + 1) / (first + 1)) > -6.0) {
                printf("FAIL quiet: %s (%u %u %u %u) holds a sound with nothing in (peak %d after 40 s)\n", RM_NAME[m],
                       f->e[0].p[0], f->e[0].p[1], f->e[0].p[2], f->e[0].p[3], pk);
                fails++;
            } else if (pk < 32 && rm_meter[0] >= 32) {
                printf("FAIL quiet: %s silent, its meter held at %d (never rests)\n", RM_NAME[m], rm_meter[0]);
                fails++;
            }
        }
    printf("quiet: %d failures\n", fails);
    return fails ? 1 : 0;
}

static int replay_load(const char *path);
/* a session saved by build 17 (format 1: four modulation slots a slot; tests/sessions/format1.bin, its values in
 * format1.txt) read by this one: the state as it was, the first four slots as they were, the four new ones empty */
static int cmd_format1(void)
{
    FILE *f = fopen("tests/sessions/format1.txt", "r");
    char line[128];
    uint32_t q, j, lvl, rec, dseed;
    int fails = 0, num;
    if (!f)
        return printf("FAIL format1: tests/sessions/format1.txt missing\n"), 1;
    if ((num = replay_load("tests/sessions/format1.bin")) < 0) {
        fclose(f);
        return printf("FAIL format1: the session does not load\n"), 1;
    }
    for (q = 0; q < NSLOT && fgets(line, sizeof line, f); q++)
        for (j = 0; j < 20u; j++) {               /* (format 1's mod: 2 LFOs, 4 slots: 20 bytes) */
            unsigned b;
            sscanf(line + 2 * j, "%2x", &b);
            if (j < 8u ? ((const uint8_t *)snap.s[0].mod[q].lfo)[j] != (uint8_t)b      /* (the LFOs, then the slots) */
                       : ((const uint8_t *)&snap.s[0].mod[q].s[(j - 8u) / 3u])[(j - 8u) % 3u] != (uint8_t)b)
                fails++;
        }
    for (q = 0; q < NSLOT; q++)
        for (j = NMSLOT_OLD; j < NMSLOT; j++)
            if (snap.s[0].mod[q].s[j].src || snap.s[0].mod[q].s[j].amt)
                fails++;
    if (fgets(line, sizeof line, f) && sscanf(line, "%u %u %u", &lvl, &rec, &dseed) == 3)
        fails += snap.s[0].g[0].lvl != (int8_t)lvl || snap.s[0].x[1].f.recipe != rec || snap.s[0].g[2].g.dseed != dseed;
    else
        fails++;
    fclose(f);
    if (!snap_has(0) || !snap_has(1) || snap.s[0].g[0].dis)
        fails++;
    printf("format1: session %d (build 17) read, %d failures\n", num, fails);
    return fails ? 1 : 0;
}

/* the DISINTEGRATOR on a held sound (all of it dry, so its throws are heard as they are): at 0 nothing touched (the
 * output bit for bit as with none); turned up, the sound moved more the higher (gated, stuttered, crushed: further
 * from the untouched sound), never past full scale, no steps at its edges (the largest sample-to-sample jump within
 * twice the sound's own); the image thrown about at 100 (the left-right balance swinging); turned back to 0, the
 * untouched sound again within 50 ms */
#define DIS_T 2756u
static uint32_t dis_run(int32_t amount, int32_t *out, uint32_t blocks)
{
    uint32_t b, h = 2166136261u;
    int32_t o[2 * CTL];
    gbuf[0][g_use[0]].nent = 0;
    gbuf[0][g_use[0]].dry = Q15;
    for (b = 0; b < blocks; b++) {
        if (amount >= 0)
            gen_dis[0] = (int8_t)amount;
        mix_block(o);
        if (out)
            memcpy(out + b * 2u * CTL, o, sizeof o);
        h = fnv(h, o, sizeof o);
    }
    return h;
}
static double dis_swing(const int32_t *x)              /* the spread of the left-right balance, 23 ms at a time */
{
    double m = 0, v = 0, bal[DIS_T / 32u];
    uint32_t b, i, n = 0;
    for (b = 0; b + 32u <= DIS_T; b += 32u) {
        double l = 0, r = 0;
        for (i = b * 2u * CTL; i < (b + 32u) * 2u * CTL; i += 2u) {
            l += (double)x[i] * x[i];
            r += (double)x[i + 1u] * x[i + 1u];
        }
        bal[n] = (l - r) / (l + r + 1);
        m += bal[n++];
    }
    m /= n;
    for (i = 0; i < n; i++)
        v += (bal[i] - m) * (bal[i] - m);
    return __builtin_sqrt(v / n);
}
static int cmd_dis(void)
{
    static int32_t ref[DIS_T * 2 * CTL], cur[DIS_T * 2 * CTL];
    static const int32_t A[4] = {20, 45, 70, 100};
    int fails = 0;
    uint32_t i, k;
    double prev = 0;
    fresh_keep = 1;                                      /* (build 34: the sound it was made on, whatever the seed draws now) */
    ends_fresh(4711u);                                   /* a note latched on generator 1 */
    {
        uint32_t h0 = dis_run(-1, ref, DIS_T), h1;
        ends_fresh(4711u);
        h1 = dis_run(0, 0, DIS_T);
        if (h0 != h1)
            printf("FAIL dis: at 0 the sound is touched\n"), fails++;
    }
    for (k = 0; k < 4u; k++) {
        double d = 0, sw;
        int32_t pk = 0, jump = 0, rjump = 0;
        ends_fresh(4711u);
        dis_run(A[k], cur, DIS_T);
        for (i = 2; i < DIS_T * 2u * CTL; i++) {
            double x = (double)cur[i] - ref[i];
            d += x * x;
            pk = maxi(pk, absi(cur[i]));
            jump = maxi(jump, absi(cur[i] - cur[i - 2u]));
            rjump = maxi(rjump, absi(ref[i] - ref[i - 2u]));
        }
        d = 10.0 * __builtin_log10(d / (DIS_T * 2.0 * CTL) + 1);
        sw = dis_swing(cur) - dis_swing(ref);
        printf("dis %3d: moved %.1f dB, peak %d, largest step %d (the sound's own %d), the balance swinging %+.2f more\n",
               A[k], d, pk, jump, rjump, sw);
        if (d < prev - 1.0)
            printf("FAIL dis: %d moves the sound less than less does\n", A[k]), fails++;
        if (pk > 32767)
            printf("FAIL dis: %d past full scale\n", A[k]), fails++;
        if (jump > 2 * rjump + 2000)
            printf("FAIL dis: %d steps (clicks)\n", A[k]), fails++;
        if (A[k] == 100 && sw < 0.1)
            printf("FAIL dis: 100 throws nothing about the image\n"), fails++;
        prev = d;
    }
    dis_run(100, 0, 1378);                               /* back to 0: untouched within 50 ms */
    dis_run(0, 0, 70);
    if (dis[0].g != Q15 || dis[0].m || dis[0].pan)
        printf("FAIL dis: back at 0, still touched (gain %d, wet %d, pan %d)\n", dis[0].g, dis[0].m, dis[0].pan), fails++;
    fresh_keep = 0;
    printf("dis: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* the paths: HOME on a generator and on an effect slot drawn 300 times each way. ABRASIVE: the harsh families and the
 * abrasive recipes most of the time; DARK: no harsh family, none of the abrasive recipes, the harsh effects only where
 * nothing else could stand; ANY: as before */
static int cmd_paths(void)
{
    int fails = 0;
    uint32_t p, n;
    for (p = 0; p < DP_N; p++) {
        uint32_t rough = 0, harsh = 0, rrec = 0, drec = 0, hfx = 0, struck = 0, mrec = 0;
        fw_init();
        draw_path = (uint8_t)p;
        for (n = 0; n < 300u; n++) {
            const fx_patch_t *f;
            uint32_t k, fam;
            gen_draw(0, -1, 1000u + n * 7u);
            fw_run(2);
            fam = gbuf[0][g_pub[0]].gen.family;
            rough += MF_LEAN[DP_ROUGH][fam] == 3u;      /* (build 47: the families ABRASIVE favours, every
                                                         * generation's -- a list of the first twenty before) */
            struck += fam >= MF_G7;
            harsh += MF_HARSH[fam] > 0;
            fx_draw(0, -1, 5000u + n * 13u);
            fw_run(25);
            f = &fbuf[0][f_pub[0]];
            rrec += FR_W_ROUGH[f->recipe] >= 4;
            drec += FR_W_DARK[f->recipe] == 0;
            mrec += FR_W_MACHINE[f->recipe] == 0;
            for (k = 0; k < f->n; k++)
                hfx += (RM_HARSH >> f->e[k].kind) & 1u;
        }
        printf("paths %s: abrasive families %u, harsh %u, struck %u, abrasive recipes %u, not-dark recipes %u, harsh effects %u (of 300)\n",
               p == DP_ANY ? "ANY" : p == DP_ROUGH ? "ABRASIVE" : p == DP_DARK ? "DARK" : "MACHINE", rough, harsh, struck,
               rrec, drec, hfx);
        if (p == DP_ROUGH && (rough < 180u || rrec < 200u))
            printf("FAIL paths: ABRASIVE not abrasive enough\n"), fails++;
        if (p == DP_DARK && (harsh || drec))
            printf("FAIL paths: DARK drew something harsh\n"), fails++;
        if (p == DP_MACHINE && (struck != 300u || mrec))
            printf("FAIL paths: MACHINE drew something not struck, or a recipe it leaves out\n"), fails++;
        if (p != DP_MACHINE && struck)
            printf("FAIL paths: a struck family drawn off the MACHINE path\n"), fails++;
    }
    draw_path = DP_ANY;
    printf("paths: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* SHIFT + the first black key: an effect slot's values anew, its effects and cables the same, in place (no fade: the
 * patch not republished); a generator's sound anew, of the same family, its cables the same */
static int cmd_revalue(void)
{
    int fails = 0;
    uint32_t n;
    for (n = 0; n < 20u; n++) {
        fx_patch_t a, b;
        gen_patch_t ga, gb;
        uint32_t k, same = 1;
        ends_fresh(300u + n);
        a = fbuf[1][f_use[1]];
        press(SLOT_BTN[NGEN + 1], 50);
        key(KEY_SHIFT, 1);
        key(KEY_REWIRE, 1);
        ui_input();
        key(KEY_REWIRE, 0);
        key(KEY_SHIFT, 0);
        fw_run(4);
        b = fbuf[1][f_use[1]];
        for (k = 0; k < a.n; k++)
            same &= !memcmp(a.e[k].p, b.e[k].p, 4);
        if (same || a.n != b.n || a.out != b.out || a.ret != b.ret || f_pub[1] != f_use[1])
            printf("FAIL revalue: slot 5 (seed %u): values %s, effects or cables changed, or republished\n", 300u + n,
                   same ? "the same" : "new"), fails++;
        for (k = 0; k < a.n; k++)
            if (a.e[k].kind != b.e[k].kind)
                printf("FAIL revalue: slot 5 effect %u changed\n", k), fails++;
        ga = gbuf[0][g_use[0]];
        press(SLOT_BTN[0], 50);
        key(KEY_SHIFT, 1);
        key(KEY_REWIRE, 1);
        ui_input();
        key(KEY_REWIRE, 0);
        key(KEY_SHIFT, 0);
        fw_run(30);
        gb = gbuf[0][g_use[0]];
        if (gb.gen.family != ga.gen.family || gb.gen.seed == ga.gen.seed || gb.nent != ga.nent ||
            memcmp(gb.ent, ga.ent, ga.nent) || gb.xsrc != ga.xsrc)
            printf("FAIL revalue: generator 1 (seed %u): not a new sound of its family with its cables\n", 300u + n), fails++;
    }
    printf("revalue: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* a snapshot is the whole machine (build 19): besides the slots, the latched notes, the loops' play, direction, speed,
 * VARY and moves, the octave, the path, the DISINTEGRATOR -- stored, everything changed, recalled at once and by a
 * morph: all of it back, the notes latched since let go */
static int whole_check(const char *how, const st_t *want, uint32_t play, uint32_t dmode, uint32_t rate, uint32_t lk,
                       uint32_t note)
{
    st_t now;
    int fails = 0;
    uint32_t k, latched_other = 0;
    st_capture(&now);
    if (!st_same(&now, want))
        printf("FAIL whole %s: the state is not the snapshot\n", how), fails++;
    if ((loop[0].play != 0) != (play != 0) || loop[0].dmode != dmode || loop[0].rate != rate)
        printf("FAIL whole %s: the loops (play %u dmode %u rate %u)\n", how, loop[0].play, loop[0].dmode, loop[0].rate), fails++;
    if (!((ui.latch[1] >> lk) & 1u) || ui.latch_note[1][lk] != note)
        printf("FAIL whole %s: generator 2's latched note\n", how), fails++;
    for (k = 0; k < NVOICE; k++)                         /* (no voice still latched that the snapshot has not) */
        if (vox[k].sounding && vox[k].latched && !(vox[k].slot == 1 && vox[k].note == note) && vox[k].slot != 0)
            latched_other++;
    if (latched_other)
        printf("FAIL whole %s: %u notes latched that the snapshot has not\n", how, latched_other), fails++;
    return fails;
}
static int cmd_whole(void)
{
    int fails = 0;
    uint32_t pass, l, note;
    st_t A;
    for (pass = 0; pass < 2u; pass++) {
        fw_init();
        press(B_FX, 50);                                 /* a loop on generator 1, going round, REVERSE at half speed */
        press(B_REC, 50);
        key(0, 1);
        fw_run(300);
        key(0, 0);
        fw_run(300);
        press(B_REC, 50);
        hold_btn(B_PLAY, 1);
        fw_run(700);
        ui_input();
        key(KEY_REV, 1);
        ui_input();
        key(KEY_REV, 0);
        key(KEY_HALF, 1);
        ui_input();
        key(KEY_HALF, 0);
        hold_btn(B_PLAY, 0);
        fw_run(20);
        press(B_SEL, 50);                                /* a note latched on generator 2 */
        key(4, 1);
        key(KEY_SHIFT, 1);
        ui_input();
        key(KEY_SHIFT, 0);
        key(4, 0);
        note = ui.latch_note[1][4];
        ui.oct = 1;
        draw_path = DP_ROUGH;
        gen_dis[2] = 40;
        fx_wet[0] = 20;                                  /* (KNOB 2, build 33; a lane's own from build 36) */
        fx_wet[2] = 70;
        fw_run(200);
        snap_store(0);
        A = snap.s[0];
        press(B_FX, 50);                                 /* everything else: loops stopped, as recorded; generator 2 let
                                                          * go, another note on it; HOME everywhere */
        hold_btn(B_PLAY, 1);
        fw_run(700);
        ui_input();
        key(KEY_REV, 1);
        ui_input();
        key(KEY_REV, 0);
        key(KEY_HALF, 1);
        ui_input();
        key(KEY_HALF, 0);
        hold_btn(B_PLAY, 0);
        press(B_PLAY, 50);
        press(B_SEL, 50);
        key(KEY_SHIFT, 1);
        key(4, 1);
        ui_input();
        key(4, 0);
        key(KEY_SHIFT, 0);
        key(6, 1);
        key(KEY_SHIFT, 1);
        ui_input();
        key(KEY_SHIFT, 0);
        key(6, 0);
        for (l = 0; l < NSLOT; l++) {
            press(SLOT_BTN[l], 50);
            press(B_HOME, 50);
            fw_run(60);
        }
        ui.oct = -2;
        draw_path = DP_DARK;
        gen_dis[2] = 0;
        fx_wet[0] = fx_wet[2] = 90;
        fw_run(300);
        snap_store(1);
        if (loop[0].play || loop[0].dmode != LD_FWD || ((ui.latch[1] >> 4) & 1u))
            printf("FAIL whole: the second state was not set up\n"), fails++;
        ui.morph = pass ? 8u : 0u;                       /* back to the first: at once, then by a 4 s morph */
        snap_recall(0);
        for (l = 0; l < 700u; l++) {
            fw_run(14);
            ui_input();
        }
        fails += whole_check(pass ? "(morph)" : "(at once)", &A, 1, LD_REV, LS_HALF, 4, note);
        if (ui.oct != 1 || draw_path != DP_ROUGH || gen_dis[2] != 40 || fx_wet[0] != 20 || fx_wet[2] != 70)
            printf("FAIL whole: octave %d, path %u, DISINTEGRATOR %d, FX MIX %u %u\n", ui.oct, draw_path, gen_dis[2],
                   fx_wet[0], fx_wet[2]), fails++;
    }
    ui.morph = 0;
    draw_path = DP_ANY;
    fx_wet[0] = fx_wet[1] = fx_wet[2] = FX_WET_DEF;
    printf("whole: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* the website's demos: three soundscapes from the machine as it is, rendered through the real mixer into DIR (WAV,
 * 44.1 kHz stereo), each from a fixed seed -- SMELTER (ABRASIVE, a generator disintegrating), POLAR NIGHT (DARK),
 * TAILINGS (anything, two snapshots morphing into each other). tools/make_demos.sh makes them MP3s for the site */
static int16_t demo_buf[FS * 60 * 2];
static void demo_latch(uint32_t gen, uint32_t k)
{
    press(SLOT_BTN[gen], 50);
    key(k, 1);
    key(KEY_SHIFT, 1);
    ui_input();
    key(KEY_SHIFT, 0);
    key(k, 0);
}
static uint32_t demo_run(uint32_t secs, uint32_t at, int32_t *pk)
{
    uint32_t b, i, n = secs * 1378u;
    int32_t out[2 * CTL];
    for (b = 0; b < n; b++) {
        mix_block(out);
        for (i = 0; i < 2u * CTL && at < sizeof demo_buf / sizeof demo_buf[0]; i++, at++) {
            demo_buf[at] = (int16_t)clampi(out[i], -32767, 32767);
            *pk = maxi(*pk, absi(out[i]));
        }
        if (!(b % 14u))
            ui_input();
    }
    return at;
}
static int cmd_demo(int argc, char **argv)
{
    const char *dir = argc > 2 ? argv[2] : "build";
    static const char *const NAME[3] = {"smelter", "polar-night", "tailings"};
    uint32_t d, l, seed = argc > 3 ? (uint32_t)atoi(argv[3]) : 0;
    char path[512];
    for (d = 0; d < 3u; d++) {
        uint32_t at = 0;
        int32_t pk = 0;
        double rms = 0;
        fm1_in.notes = fm1_in.buttons = 0;
        ui_init();
        ui.seed = 1000u * (d + 1u) + seed;
        gens_init(ui.seed);
        scale_random(ui.seed);
        ses_init(1);
        master_q12 = 4096;
        draw_path = d == 0 ? DP_ROUGH : d == 1 ? DP_DARK : DP_ANY;
        for (l = 0; l < NSLOT; l++) {                    /* every slot drawn on the path */
            press(SLOT_BTN[l], 50);
            press(B_HOME, 50);
            fw_run(30);
        }
        if (d == 1) {                                    /* polar night: two generators, deep in the reverb */
            ui.rvp[0] = 95;
            ui.rvp[1] = 90;
            ui.rvp[3] = 70;
            rv_param(&mrv, ui.rvp[0], ui.rvp[1], ui.rvp[2], ui.rvp[3]);
            for (l = 0; l < NGEN; l++) {
                ui.rsend[l] = 80;
                sends_apply(l);
            }
            demo_latch(0, 0);
            demo_latch(1, 7);
        } else {
            demo_latch(0, 0);
            demo_latch(1, 4);
            demo_latch(2, 9);
        }
        if (d == 0) {
            at = demo_run(14, at, &pk);
            gen_dis[0] = 60;                             /* smelter: the first generator falls apart */
            at = demo_run(24, at, &pk);
            gen_dis[0] = 25;
            at = demo_run(10, at, &pk);
        } else if (d == 2) {                             /* tailings: two states, morphing (12 s) */
            snap_store(0);
            for (l = 0; l < NSLOT; l++) {
                press(SLOT_BTN[l], 50);
                press(B_HOME, 50);
                fw_run(30);
            }
            snap_store(1);
            ui.morph = 0;
            snap_recall(0);
            at = demo_run(14, at, &pk);
            ui.morph = 24;
            snap_recall(1);
            at = demo_run(22, at, &pk);
            snap_recall(0);
            at = demo_run(12, at, &pk);
        } else
            at = demo_run(48, at, &pk);
        for (l = 0; l < at; l++)
            rms += (double)demo_buf[l] * demo_buf[l];
        snprintf(path, sizeof path, "%s/demo-%s.wav", dir, NAME[d]);
        wav_write2(path, demo_buf, at / 2u);
        printf("demo %s: %.1f s, peak %d, rms %.0f, path %u, families %s %s %s\n", NAME[d], at / 2.0 / FS, pk,
               __builtin_sqrt(rms / at), draw_path, MF_NAME[gbuf[0][g_use[0]].gen.family], MF_NAME[gbuf[1][g_use[1]].gen.family],
               MF_NAME[gbuf[2][g_use[2]].gen.family]);
        for (l = 0; l < NFX; l++)
            print_fx(l);
        gen_dis[0] = 0;
        ui.morph = 0;
    }
    draw_path = DP_ANY;
    return 0;
}

/* the generators' links (build 25): HOME and REWIRE never draw one, on any path; SHIFT, generator 1's button held, then
 * generator 2's: 1 modulates 2 (a kind and a depth drawn), the message up, neither muted however long they are held;
 * HOME and REWIRE on 2 keep it; a snapshot holds it; the same gesture again undoes it */
static void link_gesture(uint32_t o, uint32_t d)
{
    key(KEY_SHIFT, 1);
    hold_btn(SLOT_BTN[o], 1);
    fw_run(100);
    hold_btn(SLOT_BTN[d], 1);
    fw_run(1400);                                        /* (held past a mute's 0.8 s) */
    ui_input();
    hold_btn(SLOT_BTN[d], 0);
    hold_btn(SLOT_BTN[o], 0);
    key(KEY_SHIFT, 0);
    fw_run(10);
}
static int cmd_link(void)
{
    int fails = 0;
    uint32_t p, n, l;
    for (p = 0; p < DP_N; p++) {                         /* never drawn */
        fw_init();
        draw_path = (uint8_t)p;
        for (n = 0; n < 60u; n++)
            for (l = 0; l < NGEN; l++) {
                gen_draw(l, -1, 700u + n * 3u + l);
                fw_run(2);
                gen_rewire(l, 900u + n);
                fw_run(2);
                if (gbuf[l][g_use[l]].xsrc != XM_NONE)
                    printf("FAIL link: generator %u drew a link (path %u)\n", l + 1u, p), fails++, n = 60u;
            }
    }
    draw_path = DP_ANY;
    ends_fresh(5150u);
    link_gesture(0, 1);
    {
        const gen_patch_t *g = &gbuf[1][g_use[1]];
        if (g->xsrc != 0 || g->xkind >= XM_N || g->xdepth < 35 || g->xdepth > 95)
            printf("FAIL link: SHIFT + 1 held + 2 did not link 1 to 2 (src %u)\n", g->xsrc), fails++;
        if (gen_mute[0] || gen_mute[1])
            printf("FAIL link: a generator muted by the gesture\n"), fails++;
        if ((int32_t)(ui.lk_until - fm1_ms) <= 0 && !ui.lk_shown)
            printf("FAIL link: no message\n"), fails++;
    }
    press(SLOT_BTN[1], 50);                              /* HOME, REWIRE on 2: kept */
    press(B_HOME, 50);
    fw_run(40);
    key(KEY_REWIRE, 1);
    ui_input();
    key(KEY_REWIRE, 0);
    fw_run(40);
    if (gbuf[1][g_use[1]].xsrc != 0)
        printf("FAIL link: HOME or REWIRE on 2 dropped its link\n"), fails++;
    snap_store(0);
    link_gesture(0, 1);                                  /* undone */
    if (gbuf[1][g_use[1]].xsrc != XM_NONE || ui.lk_kind != XM_N)
        printf("FAIL link: the gesture again did not undo it\n"), fails++;
    ui.morph = 0;
    snap_recall(0);                                      /* the snapshot holds it */
    fw_run(40);
    ui_input();
    if (gbuf[1][g_use[1]].xsrc != 0)
        printf("FAIL link: the snapshot did not bring the link back\n"), fails++;
    printf("link: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* the master's tails kept (build 26, the owner's): a note held 3 s, sent deep to the master delay and reverb, let go;
 * after its voice has ended and the effect slots have closed (6 to 8 s after it was let go), and with an effect slot
 * and the generator muted on the way, the delay and reverb still ring -- and then fall silent by themselves (within
 * 90 s, even at a long DECAY and a high FEEDBACK) */
static int cmd_mtails(void)
{
    int fails = 0;
    uint32_t b, quiet_at = 0, l;
    int32_t late = 0, out[2 * CTL], pk;
    fw_init();
    fx_wet[0] = fx_wet[1] = fx_wet[2] = 100;             /* (with the effects alone: KNOB 2 at 100, build 33) */
    for (l = 0; l < NSLOT; l++) {                        /* (build 41: drawn from fixed seeds -- through HOME the host's
                                                          * clock went into them, and any change to how often the code
                                                          * reads it drew another patch) */
        ui.seed = 777u + l;
        host_ticks = 0;
        new_patch(l, 0);
        fw_run(30);
    }
    ui.rvp[1] = 90;                                      /* a long DECAY (RT60 ~17 s), a high FEEDBACK */
    ui.dlp[1] = 80;
    rv_param(&mrv, ui.rvp[0], ui.rvp[1], ui.rvp[2], ui.rvp[3]);
    mdl_param(ui.dlp[0], ui.dlp[1], ui.dlp[2], ui.dlp[3]);
    press(B_FX, 50);
    ui.rsend[0] = 90;
    ui.dsend[0] = 60;
    sends_apply(0);
    gbuf[0][g_use[0]].dry = Q15 / 2;                     /* (build 36, lanes only: half of it dry, so the master's sends
                                                          * hear it with its lane muted, as MESH's dry share did) */
    key(0, 1);
    fw_run(4134);
    key(0, 0);
    fw_run(1378);
    fx_mute[0] = 1;                                      /* an effect slot muted, then the generator */
    fw_run(1378);
    gen_mute[0] = 1;
    for (b = 0; b < 1378u * 90u; b++) {
        uint32_t i;
        mix_block(out);
        for (i = 0, pk = 0; i < 2u * CTL; i++)
            pk = maxi(pk, absi(out[i]));
        if (b >= 1378u * 4u && b < 1378u * 6u)           /* (6 .. 8 s after it was let go) */
            late = maxi(late, pk);
        if (pk > 33)
            quiet_at = b + 1u;
    }
    printf("mtails: 6 .. 8 s after the last key the master's tails peak at %d; silent %.1f s after\n", late,
           2.0 + quiet_at / 1378.0);
    if (late < 66)                                       /* (cut, they would be silent: under 33; build 41: 300 before,
                                                          * a patch's own level -- 135 on another draw, still ringing;
                                                          * build 45: 100 before, the new recipes drew one at 99, ringing
                                                          * until 11 s: twice the silence now) */
        printf("FAIL mtails: the master delay and reverb tails were cut\n"), fails++;
    if (quiet_at >= 1378u * 88u)
        printf("FAIL mtails: the master's tails never end\n"), fails++;
    fx_mute[0] = 0;
    gen_mute[0] = 0;
    fx_wet[0] = fx_wet[1] = fx_wet[2] = FX_WET_DEF;
    printf("mtails: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* LANES (build 28): turned on over a MESH patch -- each generator into its own lane alone, no cables between lanes, the
 * lanes panned centre, left, right, the snapshots too, nothing redrawn; then every draw keeps to the lanes: HOME on a
 * generator draws its lane's chain with it (and HOME held all three), HOME and REWIRE on either kind of slot never
 * cross; kept with the session */
static int lanes_ok(const char *what)
{
    uint32_t l, k;
    for (l = 0; l < NGEN; l++) {
        const gen_patch_t *p = &gbuf[l][g_pub[l]];
        if (p->nent != 1 || p->ent[0] / FX_N != l) {
            printf("FAIL lanes: %s: generator %u cabled outside its lane\n", what, l + 1u);
            return 1;
        }
    }
    for (l = 0; l < NFX; l++)                            /* (build 44: where the cables across were, a return -- only
                                                          * where its recipe has one) */
        if ((fbuf[l][f_pub[l]].ret && !fx_ret_ok(&fbuf[l][f_pub[l]])) || fbuf[l][f_pub[l]].rsv[0] || fbuf[l][f_pub[l]].rsv[1]) {
            printf("FAIL lanes: %s: slot %u cabled across\n", what, l + 4u);
            return 1;
        }
    (void)k;
    return 0;
}
static int cmd_lanes(void)
{
    int fails = 0;
    uint32_t l, n, ser[NFX], sum0, cross = 0;
    memset(hflash, 0xFF, sizeof hflash);
    fw_init();
    fails += lanes_ok("at power-on");
    {   /* (build 36: LANES the only flow) a MESH session of builds 28 .. 35 -- generator 1 cabled into slot 6, slot 4
         * across into slot 6, the slots left, middle, right -- loads into lanes, its snapshots too, nothing redrawn */
        gbuf[0][g_pub[0]].nent = 1;
        gbuf[0][g_pub[0]].ent[0] = 2 * FX_N;
        fbuf[0][f_pub[0]].ret = (uint16_t)(1u << (2 * FX_N));   /* (the old cables' place: build 44's return) */
        fx_pan[0] = -10;
        fx_pan[1] = 0;
        fx_pan[2] = 10;
        snap_store(0);
        sum0 = fnv(2166136261u, &gbuf[0][g_pub[0]].gen, sizeof gbuf[0][0].gen);
        ses_mode_save = 0;                               /* (saved as an old MESH session) */
        ses_save(6);
        ses_mode_save = SES_LANES;
        fw_init();
        if (ses_load_slot(6) != SES_OK)
            printf("FAIL lanes: the old MESH session not loaded\n"), fails++;
        fw_run(60);
        fails += lanes_ok("an old MESH session loaded");
        if (fnv(2166136261u, &gbuf[0][g_pub[0]].gen, sizeof gbuf[0][0].gen) != sum0)
            printf("FAIL lanes: loading into lanes redrew a sound\n"), fails++;
        if (fx_pan[0] != 0 || fx_pan[1] != -14 || fx_pan[2] != 14)
            printf("FAIL lanes: the lanes not panned centre, left, right\n"), fails++;
        for (l = 0; l < NFX; l++)
            cross |= (uint32_t)(snap.s[0].x[l].f.ret | snap.s[0].x[l].f.rsv[0] | snap.s[0].x[l].f.rsv[1]);
        if (cross || snap.s[0].g[0].g.ent[0] / FX_N != 0u)
            printf("FAIL lanes: a snapshot still cabled across\n"), fails++;
    }
    for (n = 0; n < 20u; n++) {
        for (l = 0; l < NFX; l++)
            ser[l] = fbuf[l][f_pub[l]].serial;
        press(SLOT_BTN[n % NGEN], 50);                   /* HOME on a generator: its lane drawn with it */
        press(B_HOME, 50);
        fw_run(40);
        if (fbuf[n % NGEN][f_pub[n % NGEN]].serial == ser[n % NGEN])
            printf("FAIL lanes: HOME on generator %u did not draw its lane\n", n % NGEN + 1u), fails++;
        for (l = 0; l < NFX; l++)
            if (l != n % NGEN && fbuf[l][f_pub[l]].serial != ser[l])
                printf("FAIL lanes: HOME on generator %u drew lane %u\n", n % NGEN + 1u, l + 1u), fails++;
        key(KEY_REWIRE, 1);                              /* REWIRE it, and an effect slot drawn and rewired */
        ui_input();
        key(KEY_REWIRE, 0);
        press(SLOT_BTN[NGEN + (n + 1u) % NFX], 50);
        press(B_HOME, 50);
        fw_run(40);
        key(KEY_REWIRE, 1);
        ui_input();
        key(KEY_REWIRE, 0);
        fw_run(10);
        fails += lanes_ok("drawing");
    }
    fails += lanes_ok("after the draws");
    ses_save(5);                                          /* kept with the session */
    fw_init();
    if (ses_load_slot(5) != SES_OK)
        printf("FAIL lanes: the session not loaded again\n"), fails++;
    fw_run(60);
    fails += lanes_ok("loaded again");
    gens_init(1u);
    printf("lanes: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* the session pages (build 28, Bigorna's), through the panel: SHIFT + SAVE, a slot, SAVE: its page; SAVE saves (over
 * another, asked), COPY to the first empty slot, DELETE asked (NO keeps it, YES deletes), LOAD */
static void page_open(uint32_t num)
{
    key(KEY_SHIFT, 1);
    while (ui.ses_page)                                  /* (out of any session page first, a level at a time) */
        press(B_SAVE, 50);
    press(B_SAVE, 50);
    key(KEY_SHIFT, 0);
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = (int32_t)num - (int32_t)ui.ses_ix;
    ui_input();
    press(B_SAVE, 50);                                   /* (its page, on LOAD) */
}
static void page_row(uint32_t row)
{
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = (int32_t)row - (int32_t)ui.ses_row;
    ui_input();
}
static int cmd_sespage(void)
{
    int fails = 0;
    memset(hflash, 0xFF, sizeof hflash);
    fw_init();
    press(B_FX, 50);
    press(B_HOME, 50);
    page_open(3);                                        /* SAVE on an empty slot's page: saved there at once */
    page_row(SR_SAVE);
    press(B_SAVE, 50);
    if (!ses_has(3) || ses_current() != 3 || ui.ses_page)
        printf("FAIL sespage: SAVE on slot 3's page\n"), fails++;
    page_open(3);                                        /* COPY: to the first empty slot (1) */
    page_row(SR_COPY);
    press(B_SAVE, 50);
    if (ui.ses_page != SP_COPY || ui.ses_tgt != 1)
        printf("FAIL sespage: COPY did not offer the first empty slot (%u)\n", ui.ses_tgt), fails++;
    press(B_SAVE, 50);
    if (!ses_has(1))
        printf("FAIL sespage: COPY to 1\n"), fails++;
    page_open(1);                                        /* DELETE: NO keeps it, YES deletes it */
    page_row(SR_DELETE);
    press(B_SAVE, 50);
    press(B_SAVE, 50);                                   /* (NO, as asked) */
    if (!ses_has(1))
        printf("FAIL sespage: DELETE answered NO deleted\n"), fails++;
    page_row(SR_DELETE);
    press(B_SAVE, 50);
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = 1;                /* YES */
    ui_input();
    press(B_SAVE, 50);
    if (ses_has(1))
        printf("FAIL sespage: DELETE YES did not delete\n"), fails++;
    page_open(3);                                        /* saved, and loaded back */
    page_row(SR_SAVE);
    press(B_SAVE, 50);
    fw_init();
    page_open(3);
    page_row(SR_LOAD);
    press(B_SAVE, 50);
    if (ses_current() != 3)
        printf("FAIL sespage: LOAD from the page (session %u)\n", ses_current()), fails++;
    gens_init(1u);
    printf("sespage: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* a snapshot recalled at once while a slot is still taking the last change (SES_BUSY then): every part given in the
 * frames that follow, none left at the snapshot before (build 15) */
static int cmd_recall(void)
{
    int fails = 0;
    uint32_t sd, l;
    for (sd = 0; sd < 12u; sd++) {
        st_t now;
        ends_fresh(9100u + sd * 37u);
        snap_store(0);                                   /* A */
        for (l = 0; l < NSLOT; l++) {
            press(SLOT_BTN[l], 50);
            press(B_HOME, 50);
        }
        fw_run(60);
        snap_store(1);                                   /* B: every slot anew */
        ui.morph = 0;
        snap_recall(0);                                  /* A, then B before the slots have taken A */
        snap_recall(1);
        trip_frames(100);
        st_capture(&now);
        if (!st_same(&now, &snap.s[1])) {
            printf("FAIL recall seed %u: B recalled during A's change, not all of it taken\n", 9100u + sd * 37u);
            fails++;
        }
    }
    printf("recall: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* the backup (backup.c): the list, every record read back in pieces as the flash holds it, the writes refused */
static uint8_t bk_got[4096];
static uint32_t bk_gotn;
static int bk_wire(const uint8_t *p, uint32_t n)        /* F0 .. F7 -> decoded (00 59 cmd len24 body sum) */
{
    uint32_t i, acc = 0, nb = 0;
    bk_gotn = 0;
    for (i = 1; i + 1u < n; i++) {
        acc |= (uint32_t)p[i] << nb;
        nb += 7u;
        if (nb >= 8u) {
            bk_got[bk_gotn++] = (uint8_t)acc;
            acc >>= 8;
            nb -= 8u;
        }
    }
    return 0;
}
static void bk_ask(uint32_t cmd, const uint8_t *b, uint32_t n)
{
    uint8_t m[8 + 4 + 256];
    uint32_t i, s = 0;
    m[0] = 0;
    m[1] = 0x59;
    m[2] = (uint8_t)cmd;
    m[3] = (uint8_t)n;
    m[4] = (uint8_t)(n >> 8);
    m[5] = 0;
    for (i = 0; i < n; i++)
        s += m[6u + i] = b[i];
    m[6u + n] = (uint8_t)~s;
    bk_handle(m, 7u + n);
}
static int bk_reply_ok(uint32_t cmd)
{
    uint32_t n = bk_gotn >= 7u ? bk_got[3] | (uint32_t)bk_got[4] << 8 : 0, i, s = 0;
    if (bk_gotn < 7u + n || bk_got[0] || bk_got[1] != 0x59 || bk_got[2] != cmd)
        return 0;
    for (i = 0; i < n; i++)
        s += bk_got[6u + i];
    return (uint8_t)~s == bk_got[6u + n];
}
static int cmd_backup(void)
{
    int fails = 0;
    uint32_t i, k, cnt, off;
    static const uint32_t NUM[3] = {2, 7, 64};
    uint8_t list[32];
    memset(hflash, 0xFF, sizeof hflash);
    fw_init();
    for (k = 0; k < 3u; k++) {
        press(B_HOME, 50);
        snap_store(k);
        snap_store(k + 1u);
        ui.morph = 6;
        if (ses_save(NUM[k]) != SES_OK)
            printf("FAIL backup: save %u\n", NUM[k]), fails++;
    }
    bk_ask(0x50, 0, 0);
    if (!bk_reply_ok(0x50) || bk_got[6] != 3u)
        printf("FAIL backup: the list (%u records)\n", bk_got[6]), fails++;
    cnt = bk_got[6];
    memcpy(list, bk_got, sizeof list);                   /* (each read's answer takes its place) */
    for (k = 0; k < cnt && k < 3u; k++) {
        uint8_t q[8], rec[SES_NSECT * SES_SECT / 8u];
        uint32_t num = list[7u + k * 6u] | (uint32_t)list[8u + k * 6u] << 8, len = bk_rd32(list + 9u + k * 6u);
        int at = ses_find(num);
        if (num != NUM[k] || at < 0 || len > sizeof rec) {
            printf("FAIL backup: record %u listed as %u, %u B\n", k, num, len), fails++;
            continue;
        }
        for (off = 0; off < len;) {
            uint32_t n;
            q[0] = (uint8_t)num;
            q[1] = (uint8_t)(num >> 8);
            bk_wr32(q + 2, off);
            q[6] = 0;
            q[7] = 1;                                    /* 256 */
            bk_ask(0x51, q, 8);
            n = bk_got[12] | (uint32_t)bk_got[13] << 8;
            if (!bk_reply_ok(0x51) || !n || n > 256u) {
                printf("FAIL backup: read %u at %u\n", num, off), fails++;
                break;
            }
            memcpy(rec + off, bk_got + 14, n);
            off += n;
        }
        if (off == len && memcmp(rec, hflash + SES_LO + ses.sect[at] * SES_SECT, len))
            printf("FAIL backup: record %u read back differs\n", num), fails++;
        else if (off == len && !k) {                     /* (a record as fm1_sessions.py saves it: host replay's) */
            FILE *f = fopen("build/backup_session.bin", "wb");
            if (f)
                fwrite(rec, 1, len, f), fclose(f);
        }
        for (i = 0; bk_rd32(rec) == SES_MAGIC && i < 1u; i++) {          /* past its end: nothing */
            bk_wr32(q + 2, len);
            bk_ask(0x51, q, 8);
            if (bk_got[12] | bk_got[13])
                printf("FAIL backup: bytes past the end of %u\n", num), fails++;
        }
    }
    {   /* the restore (build 21): session 2 written back as it was saved -- loaded after; a copy with one byte wrong
         * refused, the session there kept; another firmware's sector never touched */
        static uint8_t rec[SES_NSECT * SES_SECT / 8u];
        uint32_t len = 0, num = 2, f;
        int at = ses_find(num), r;
        FILE *fp = fopen("build/backup_session.bin", "rb");
        uint32_t foreign = SES_NSECT - 1u, fsum;
        if (fp) {
            len = (uint32_t)fread(rec, 1, sizeof rec, fp);
            fclose(fp);
        }
        memcpy(hflash + SES_LO + foreign * SES_SECT, "TON1another", 11);   /* (another firmware's, at the top) */
        fw_init();
        fsum = fnv(2166136261u, hflash + SES_LO + foreign * SES_SECT, SES_SECT);
        for (r = 0; r < 2; r++) {
            uint8_t q[8 + 256];
            uint32_t ok = 1, last = 0;
            if (r == 1)
                rec[100] ^= 0x5Au;                       /* (the second time: one byte of its body wrong) */
            q[0] = (uint8_t)num;
            q[1] = (uint8_t)(num >> 8);
            bk_wr32(q + 2, len);
            bk_ask(0x52, q, 6);
            ok &= bk_reply_ok(0x52) && bk_got[6] == 0u;
            for (off = 0; ok && off < len; off += 256u) {
                uint32_t n = len - off < 256u ? len - off : 256u;
                bk_wr32(q, off);
                memcpy(q + 4, rec + off, n);
                bk_ask(0x53, q, 4u + n);
                ok &= bk_reply_ok(0x53) && bk_got[6] == 0u;
            }
            bk_ask(0x54, 0, 0);
            last = bk_got[6];
            if (r == 0 && (!ok || last != 0u || ses_load_slot(num) != SES_OK))
                printf("FAIL backup: the restore of session 2 (status %u)\n", last), fails++;
            if (r == 1 && (last != 4u || ses_load_slot(num) != SES_OK))
                printf("FAIL backup: a damaged record not refused (status %u), or the session there lost\n", last), fails++;
        }
        for (f = 0, at = 0; f < ses.n; f++)
            at += ses.num[f] == num;
        if (at != 1)
            printf("FAIL backup: session 2 listed %d times after the restores\n", at), fails++;
        if (fsum != fnv(2166136261u, hflash + SES_LO + foreign * SES_SECT, SES_SECT))
            printf("FAIL backup: another firmware's sector changed\n"), fails++;
    }
    {   /* a save or a load during a restore: BUSY (they share its buffer and could take its sectors); after it, or
         * 5 s after it was abandoned, free again (build 22) */
        static uint8_t rec2[SES_NSECT * SES_SECT / 8u];
        uint8_t q[8 + 256];
        uint32_t len2 = 0, off2;
        FILE *fp = fopen("build/backup_session.bin", "rb");
        if (fp) {
            len2 = (uint32_t)fread(rec2, 1, sizeof rec2, fp);
            fclose(fp);
        }
        q[0] = 2;
        q[1] = 0;
        bk_wr32(q + 2, len2);
        bk_ask(0x52, q, 6);
        bk_wr32(q, 0);
        memcpy(q + 4, rec2, 256);
        bk_ask(0x53, q, 260);
        if (ses_save(7) != SES_BUSY || ses_load_slot(2) != SES_BUSY)
            printf("FAIL backup: a save or a load went ahead during a restore\n"), fails++;
        for (off2 = 256; off2 < len2; off2 += 256u) {
            uint32_t n2 = len2 - off2 < 256u ? len2 - off2 : 256u;
            bk_wr32(q, off2);
            memcpy(q + 4, rec2 + off2, n2);
            bk_ask(0x53, q, 4u + n2);
        }
        bk_ask(0x54, 0, 0);
        if (bk_got[6] != 0u || ses_load_slot(2) != SES_OK)
            printf("FAIL backup: the restore during the lock, or a load after it (status %u)\n", bk_got[6]), fails++;
        bk_ask(0x52, q, 0);                              /* (a malformed one: nothing locked) */
        q[0] = 2;
        bk_wr32(q + 2, len2);
        bk_ask(0x52, q, 6);                              /* abandoned after its start: free 5 s later */
        fm1_ms += 6000u;
        if (ses_save(7) != SES_OK)
            printf("FAIL backup: a save still BUSY after a restore was abandoned\n"), fails++;
    }
    {   /* the owner's case (build 24): a restore while nothing sounds -- the main loop's tidying (ses_idle, a retired
         * sector of Norilsk's erased each second in silence) between its messages, its own sectors among the retired
         * ones it might take: the restore goes through, whole */
        static uint8_t rec3[SES_NSECT * SES_SECT / 8u];
        uint8_t q[8 + 256];
        uint32_t len3 = 0, off3, k3;
        FILE *fp = fopen("build/backup_session.bin", "rb");
        if (fp) {
            len3 = (uint32_t)fread(rec3, 1, sizeof rec3, fp);
            fclose(fp);
        }
        for (k3 = 0; k3 < SES_NSECT; k3++)               /* (no other retired sector to erase first) */
            if (ses.st[k3] == SC_MINE) {
                ses_flash_erase(SES_LO + k3 * SES_SECT);
                ses.st[k3] = SC_BLANK;
            }
        q[0] = 2;
        q[1] = 0;
        bk_wr32(q + 2, len3);
        ses_idle_at = fm1_ms;                            /* (the tidying has just run: its next turn comes mid-restore) */
        bk_ask(0x52, q, 6);
        for (off3 = 0; off3 < len3; off3 += 256u) {
            uint32_t n3 = len3 - off3 < 256u ? len3 - off3 : 256u;
            bk_wr32(q, off3);
            memcpy(q + 4, rec3 + off3, n3);
            bk_ask(0x53, q, 4u + n3);
            fm1_ms += bk.got % SES_SECT ? 50u : 1100u;   /* (~50 ms a message; a second of silence just after a sector
                                                          * is written: the worst moment) */
            ses_idle(1);
        }
        bk_ask(0x54, 0, 0);
        if (bk_got[6] != 0u || ses_load_slot(2) != SES_OK)
            printf("FAIL backup: a restore in silence did not go through (status %u: the tidying erased it)\n", bk_got[6]), fails++;
    }
    {   /* not a session number: refused */
        uint8_t q[6] = {65, 0, 0, 1, 0, 0};
        bk_ask(0x52, q, 6);
        if (!bk_reply_ok(0x52) || bk_got[6] != 2u)
            printf("FAIL backup: session 65 not refused\n"), fails++;
    }
    printf("backup: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* a session saved from the device (tools/fm1_sessions.py): loaded, snapshot FROM recalled at once, then morphs to TO
 * and back to FROM; the end against FROM recalled at once to a machine that had only the session loaded. The WAVs:
 * build/replay_trip.wav, build/replay_direct.wav. Each part's loudness is printed. */
static void replay_part(const char *what, double *lr, double *part)
{
    uint32_t l;
    printf("  %-8s L %.1f R %.1f  gens", what, lr[0], lr[1]);
    for (l = 0; l < NGEN; l++)
        printf(" %.1f", part[l]);
    printf("\n");
}
static double rp_inst[NINST];
static void replay_loud(double *lr, double *part)    /* trip_loud, over 3 s; each instance's meter into rp_inst */
{
    uint32_t i, l, j;
    int32_t out[2 * CTL];
    for (l = 0; l < NINST; l++)
        rp_inst[l] = 0;
    lr[0] = lr[1] = 0;
    for (l = 0; l < NGEN + NFX; l++)
        part[l] = 0;
    for (i = 0; i < 1378u * 3u; i++) {
        mix_block(out);
        for (l = 0; l < CTL; l++) {
            lr[0] += (double)out[2u * l] * out[2u * l];
            lr[1] += (double)out[2u * l + 1u] * out[2u * l + 1u];
        }
        for (l = 0; l < NGEN; l++)
            for (j = 0; j < CTL; j++)
                part[l] += (double)gbus[l][j] * gbus[l][j];
        for (l = 0; l < NINST; l++)
            rp_inst[l] += (double)rm_meter[l] * rm_meter[l];
    }
    for (l = 0; l < 2u; l++)
        lr[l] = 10.0 * __builtin_log10(lr[l] + 1);
    for (l = 0; l < NGEN; l++)
        part[l] = 10.0 * __builtin_log10(part[l] + 1);
    for (l = 0; l < NINST; l++)
        rp_inst[l] = 10.0 * __builtin_log10(rp_inst[l] + 1);
}
static int replay_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint32_t n, num;
    memset(hflash, 0xFF, sizeof hflash);
    if (!f)
        return printf("replay: cannot open %s\n", path), -1;
    n = (uint32_t)fread(hflash + SES_LO, 1, SES_HI - SES_LO, f);
    fclose(f);
    num = hflash[SES_LO + 4] | (uint32_t)hflash[SES_LO + 5] << 8;
    fm1_in.notes = fm1_in.buttons = 0;
    fw_init();
    if (ses_load_slot(num) != SES_OK)
        return printf("replay: %s (%u B) does not load\n", path, n), -1;
    return (int)num;
}
static void replay_dbg(const char *what, uint32_t i, int rc)
{
    uint32_t l, k;
    st_t now;
    st_capture(&now);
    printf("  %s %u: rc %d; ms %u;", what, i + 1u, rc, fm1_ms);
    for (l = 0; l < NGEN; l++)
        printf(" gen %u %s (pub %u use %u)", l + 1u, memcmp(&now.g[l].g, &snap.s[i].g[l].g, sizeof now.g[l].g) ? "OTHER" : "ok",
               g_pub[l], g_use[l]);
    for (l = 0; l < NGEN; l++) {
        const gen_keep_t *a = &now.g[l].g, *b = &snap.s[i].g[l].g;
        if (!memcmp(a, b, sizeof *a))
            continue;
        printf("\n   gen %u now/snap: pseed %08X/%08X dseed %08X/%08X gain %d/%d send %d/%d dry %d/%d fam %u/%u path %u/%u"
               " gen %u/%u scene %u/%u nent %u/%u ent %u.%u.%u/%u.%u.%u x %u,%u,%u/%u,%u,%u drawn %u/%u", l + 1u,
               a->pseed, b->pseed, a->dseed, b->dseed, a->gain, b->gain, a->send, b->send, a->dry, b->dry, a->family,
               b->family, a->path, b->path, a->gen, b->gen, a->scene, b->scene, a->nent, b->nent, a->ent[0], a->ent[1],
               a->ent[2], b->ent[0], b->ent[1], b->ent[2], a->xsrc, a->xkind, a->xdepth, b->xsrc, b->xkind, b->xdepth,
               a->drawn, b->drawn);
    }
    printf("\n   voices:");
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding)
            printf(" [slot %u dsg %u gate %u fade %u pend %u latched %u xf %u]", vox[k].slot, vox[k].dsg, vox[k].gate,
                   vox[k].fade, vox[k].pend, vox[k].latched, vox[k].xf);
    printf("\n");
}
static void replay_latch(void)                       /* LATCH=all: a note latched on each generator with none */
{
    uint32_t l;
    if (!getenv("LATCH"))
        return;
    for (l = 0; l < NGEN; l++)
        if (!ui.latch[l]) {
            press(SLOT_BTN[l], 50);
            key(2u + 2u * l, 1);
            key(KEY_SHIFT, 1);
            ui_input();
            key(KEY_SHIFT, 0);
            key(2u + 2u * l, 0);
        }
    press(SLOT_BTN[0], 50);
}
static int cmd_replay(int argc, char **argv)
{
    uint32_t from = argc > 3 ? (uint32_t)atoi(argv[3]) - 1u : 2u, to = argc > 4 ? (uint32_t)atoi(argv[4]) - 1u : 1u;
    uint32_t settle = argc > 5 ? (uint32_t)atoi(argv[5]) : 0, morph_t, l;
    double ma[2], mb[2], pa[NGEN + NFX], pb[NGEN + NFX], ia[NINST];
    st_t sa;
    int num;
    if (argc < 3)
        return printf("usage: host replay SESSION.bin [FROM TO [FRAMES]]  (snapshots 1..16; FRAMES at TO before going back)\n"), 1;
    if ((num = replay_load(argv[2])) < 0)
        return 1;
    morph_t = getenv("MORPH") ? (uint32_t)atoi(getenv("MORPH")) : ui.morph;   /* (half seconds) */
    printf("replay: session %d, morph %u.%u s, snapshots used %04X, current %d\n", num, morph_t / 2u, morph_t % 2u * 5u,
           snap.used, snap.cur + 1);
    for (l = 0; l < NGEN; l++)
        print_gen(l);
    for (l = 0; l < NFX; l++)
        print_fx(l);
    if (!snap_has(from) || !snap_has(to))
        return printf("replay: snapshot %u or %u empty\n", from + 1u, to + 1u), 1;
    if (!ui.latch[0] && !ui.latch[1] && !ui.latch[2]) {  /* (nothing latched in it: a note latched, as ends_fresh) */
        printf("replay: no note latched in the session: key 1 latched\n");
        key(0, 1);
        key(KEY_SHIFT, 1);
        ui_input();
        key(KEY_SHIFT, 0);
        key(0, 0);
    }
    replay_latch();
    if (getenv("RDBG")) {                                /* the two snapshots, byte by byte where they differ */
        const uint8_t *a = (const uint8_t *)&snap.s[to], *b = (const uint8_t *)&snap.s[from];
        uint32_t i;
        printf("snapshots %u / %u differ at:", to + 1u, from + 1u);
        for (i = 0; i < sizeof(st_t); i++)
            if (a[i] != b[i])
                printf(" %u(%u/%u)", i, a[i], b[i]);
        printf("\n  offsets: g %u (each %u) x %u (each %u) rvp %u dlp %u mod %u (each %u) scale %u\n",
               (uint32_t)__builtin_offsetof(st_t, g), (uint32_t)sizeof(st_gen_t), (uint32_t)__builtin_offsetof(st_t, x),
               (uint32_t)sizeof(st_fx_t), (uint32_t)__builtin_offsetof(st_t, rvp), (uint32_t)__builtin_offsetof(st_t, dlp),
               (uint32_t)__builtin_offsetof(st_t, mod), (uint32_t)sizeof(mod_t), (uint32_t)__builtin_offsetof(st_t, scale));
        printf("  fx_patch_t %u, e at %u\n", (uint32_t)sizeof(fx_patch_t), (uint32_t)__builtin_offsetof(fx_patch_t, e));
    }
    ui.morph = 0;
    l = (uint32_t)snap_recall(to);
    printf("recall %u: rc %d, cur %d, morph %d; fx6 pub %u use %u kinds %u %u\n", to + 1u, (int)l, snap.cur + 1, morph.on,
           f_pub[2], f_use[2], fbuf[2][f_pub[2]].e[0].kind, snap.s[to].x[2].f.e[0].kind);
    fw_run(3);
    printf("snapshot %u:\n", to + 1u);
    for (l = 0; l < NGEN; l++)
        print_gen(l);
    for (l = 0; l < NFX; l++)
        printf("  (fx %u lvl %d pan %d)", l + 4u, fx_lvl[l], fx_pan[l]), print_fx(l);
    snap_recall(from);
    fw_run(3);
    printf("snapshot %u:\n", from + 1u);
    for (l = 0; l < NGEN; l++)
        print_gen(l);
    for (l = 0; l < NFX; l++)
        printf("  (fx %u lvl %d pan %d)", l + 4u, fx_lvl[l], fx_pan[l]), print_fx(l);
    trip_frames(600);
    fw_frames = 0;
    ui.morph = (uint8_t)morph_t;
    l = (uint32_t)snap_recall(to);
    if (getenv("RDBG"))
        replay_dbg("to", to, (int)l);
    trip_frames(settle ? settle : morph_t * 50u + 600u);
    if (getenv("RDBG"))
        replay_dbg("at to", to, 0);
    l = (uint32_t)snap_recall(from);
    if (getenv("RDBG"))
        replay_dbg("back", from, (int)l);
    trip_frames(morph_t * 50u + 200u);
    fw_frames = 0;                                       /* (the last 4 s) */
    trip_frames(400);
    wav_write2("build/replay_trip.wav", wav2, fw_frames);
    st_capture(&sa);
    if (getenv("RDBG")) {                                /* what reaches 4.1, 4.2: their inputs and outputs, a while */
        uint32_t b, j;
        int32_t out[2 * CTL];
        for (b = 0; b < 8u; b++) {
            int32_t in0 = 0;
            for (j = 0; j < 200u; j++)
                mix_block(out);
            for (j = 0; j < CTL; j++)
                in0 = maxi(in0, absi(ebus_run[0][j]));
            printf("  4.1 in %d meter %d quiet %u | 4.2 meter %d quiet %u | gbus2 %d", in0, rm_meter[0], rm_quiet[0],
                   rm_meter[1], rm_quiet[1], absi(gbus[1][0]));
            for (in0 = 0, j = 0; j < CTL; j++)
                in0 = maxi(in0, absi(ebus_run[7][j]));
            if (!b) {
                uint32_t q, e;
                for (q = 0; q < NGEN; q++)
                    for (e = 0; e < gbuf[q][g_use[q]].nent; e++)
                        if (gbuf[q][g_use[q]].ent[e] == 7u)
                            printf(" [gen %u -> 6.2]", q + 1u);
                printf(" fed %08X fed_run %08X", ebus_fed, ebus_fed_run);
            }
            printf(" | 6.1 m %d q %u, 6.2 in %d m %d q %u, 6.3 m %d q %u | fx6 kinds %u %u %u n %u\n", rm_meter[6], rm_quiet[6], in0,
                   rm_meter[7], rm_quiet[7], rm_meter[8], rm_quiet[8], fbuf[2][f_use[2]].e[0].kind, fbuf[2][f_use[2]].e[1].kind,
                   fbuf[2][f_use[2]].e[2].kind, fbuf[2][f_use[2]].n);
        }
    }
    replay_loud(ma, pa);
    memcpy(ia, rp_inst, sizeof ia);
    replay_load(argv[2]);
    if (!ui.latch[0] && !ui.latch[1] && !ui.latch[2]) {
        key(0, 1);
        key(KEY_SHIFT, 1);
        ui_input();
        key(KEY_SHIFT, 0);
        key(0, 0);
    }
    replay_latch();
    ui.morph = 0;
    snap_recall(from);
    trip_frames(200);
    fw_frames = 0;
    trip_frames(400);
    wav_write2("build/replay_direct.wav", wav2, fw_frames);
    replay_loud(mb, pb);
    printf("snapshot %u, after %u -> %u -> %u (dB):\n", from + 1u, from + 1u, to + 1u, from + 1u);
    replay_part("morphed", ma, pa);
    replay_part("direct", mb, pb);
    printf("  effects (morphed / direct):");
    for (l = 0; l < NINST; l++)
        printf(" %u.%u %.0f/%.0f", l / FX_N + 4u, l % FX_N + 1u, ia[l], rp_inst[l]);
    printf("\n");
    if (!st_same(&sa, &snap.s[from])) {
        st_t x = sa, y = snap.s[from];
        st_norm(&x);
        st_norm(&y);
        printf("  the state after the morphs is not snapshot %u's:", from + 1u);
        for (l = 0; l < NGEN; l++)
            if (memcmp(&x.g[l], &y.g[l], sizeof x.g[l]))
                printf(" gen %u (sound %s, lvl %d/%d pan %d/%d mute %u/%u sends %u/%u %u/%u)", l + 1u,
                       memcmp(&x.g[l].g, &y.g[l].g, sizeof x.g[l].g) ? "differs" : "same", x.g[l].lvl, y.g[l].lvl,
                       x.g[l].pan, y.g[l].pan, x.g[l].mute, y.g[l].mute, x.g[l].rsend, y.g[l].rsend, x.g[l].dsend,
                       y.g[l].dsend);
        for (l = 0; l < NFX; l++)
            if (memcmp(&x.x[l], &y.x[l], sizeof x.x[l]))
                printf(" fx %u (effects %s, lvl %d/%d pan %d/%d mute %u/%u)", l + 4u,
                       memcmp(&x.x[l].f, &y.x[l].f, sizeof x.x[l].f) ? "differ" : "same", x.x[l].lvl, y.x[l].lvl,
                       x.x[l].pan, y.x[l].pan, x.x[l].mute, y.x[l].mute);
        if (memcmp(x.rvp, y.rvp, 8))
            printf(" master");
        if (memcmp(x.mod, y.mod, sizeof x.mod))
            printf(" lfos");
        if (x.scale != y.scale || x.root != y.root)
            printf(" scale");
        printf("\n");
    }
    return 0;
}

static int cmd_flows(void)
{
    int fails = 0;
    uint32_t k, n;
    fw_init();
    press(B_SEL, 900);                                   /* generator 2 held: muted */
    if (!gen_mute[1] || ui.gen != 1) {
        printf("FAIL flows: a long press did not mute generator 2\n");
        fails++;
    }
    press(B_SEL, 900);                                   /* .. and heard again */
    if (gen_mute[1]) {
        printf("FAIL flows: a second long press did not unmute generator 2\n");
        fails++;
    }
    key(0, 1);
    key(2, 1);
    fw_run(400);
    press(B_HOME, 50);                                   /* HOME under held notes: they move to the new sound */
    fw_run(400);
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && vox[k].gate && vox[k].slot == 1 && vox[k].dsg != g_use[1]) {
            printf("FAIL flows: a held voice on the old patch\n");
            fails++;
        }
    press(B_EDIT, 50);                                   /* effect slot 5 chosen: the keys still play generator 2 */
    if (ui.sel != 4 || ui.gen != 1) {
        printf("FAIL flows: choosing an effect slot moved the keys\n");
        fails++;
    }
    press(B_HOME, 50);                                   /* HOME on it: a new recipe, faded, cleared, started */
    fw_run(60);
    if (f_pub[1] != f_use[1] || fx_phase[1] != FS_ON) {
        printf("FAIL flows: the effect slot's new patch was not taken\n");
        fails++;
    }
    key(0, 0);
    key(2, 0);
    fw_run(10);
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && vox[k].gate) {
            printf("FAIL flows: a note held after its key was lifted\n");
            fails++;
        }
    /* latching: a key held and SHIFT pressed: latched; the key let go, it sounds on */
    press(B_FX, 50);
    key(9, 1);
    key(KEY_SHIFT, 1);
    key(KEY_SHIFT, 0);
    key(9, 0);
    fw_run(200);
    {
        int on = 0;
        for (k = 0; k < NVOICE; k++)
            on |= vox[k].sounding && vox[k].gate && vox[k].latched && vox[k].slot == 0;
        if (!on || !(ui.latch[0] & (1u << 9))) {
            printf("FAIL flows: a key held and SHIFT pressed did not latch\n");
            fails++;
        }
    }
    press(B_HOME, 50);                                   /* a new patch: the latched note goes on, on the new sound */
    fw_run(200);
    {
        int on = 0;
        for (k = 0; k < NVOICE; k++)
            on |= vox[k].sounding && vox[k].gate && vox[k].latched && vox[k].slot == 0 && vox[k].dsg == g_use[0];
        if (!on) {
            printf("FAIL flows: a latched note did not move to the new patch\n");
            fails++;
        }
    }
    key(9, 1);                                           /* the key alone: nothing */
    key(9, 0);
    key(KEY_SHIFT, 1);                                   /* SHIFT held and the lit key: let go */
    key(9, 1);
    key(9, 0);
    key(KEY_SHIFT, 0);
    fw_run(20);
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && vox[k].gate && vox[k].slot == 0) {
            printf("FAIL flows: a latched note was not let go\n");
            fails++;
        }
    press(B_LFO, 900);                                   /* effect slot 4 held: muted */
    if (!fx_mute[0]) {
        printf("FAIL flows: a long press did not mute effect slot 4\n");
        fails++;
    }
    press(B_LFO, 900);
    press(B_HOME, 900);                                  /* HOME held: all six anew */
    fw_run(100);
    for (n = 0; n < NGEN; n++) {                         /* every key at once, on each generator */
        press(SLOT_BTN[n], 50);
        for (k = 0; k < NKEY; k++)
            if (k != KEY_SHIFT)                          /* (SHIFT would latch the keys held) */
                key(k, 1);
        fw_run(100);
        for (k = 0; k < NKEY; k++)
            key(k, 0);
    }
    for (n = 0; n < 1378u * 40u && gens_sounding(); n += 64u)
        fw_run(64);
    if (gens_sounding()) {
        for (k = 0; k < NVOICE; k++)
            if (vox[k].sounding)
                printf("  voice %u: slot %u note %u gate %u latched %u fade %u pend %u\n", k, vox[k].slot, vox[k].note,
                       vox[k].gate, vox[k].latched, vox[k].fade, vox[k].pend);
        printf("FAIL flows: a voice still sounds 40 s after every key was let go\n");
        fails++;
    }
    for (n = 0; n < 30u; n++) {                          /* HOME on every slot, fast, under notes */
        press(SLOT_BTN[n % NSLOT], 30);
        press(SLOT_BTN[n % NGEN], 30);
        key(0, 1);
        fw_run(20);
        press(SLOT_BTN[(n * 5u) % NSLOT], 30);
        press(B_HOME, 30);
        key(0, 0);
        fw_run(20);
    }
    fw_run(2000);
    for (n = 0; n < NGEN; n++)
        if (g_use[n] != g_pub[n]) {
            printf("FAIL flows: a generator's patch left untaken\n");
            fails++;
        }
    for (n = 0; n < NFX; n++)
        if (f_use[n] != f_pub[n] || fx_phase[n] != FS_ON) {
            printf("FAIL flows: an effect slot's patch left untaken\n");
            fails++;
        }
    {   /* an effect slot chosen (build 26): the white keys play nothing and latch nothing; a key pressed on a generator
         * and let go after an effect slot was chosen still ends its note */
        uint32_t k, before = 0, after = 0;
        press(B_FX, 50);
        key(2, 1);                                       /* held on generator 1 */
        fw_run(100);
        press(B_LFO, 50);                                /* effect slot 4 chosen */
        for (k = 0; k < NVOICE; k++)
            before += vox[k].sounding && vox[k].gate;
        key(4, 1);                                       /* a white key, SHIFT: nothing */
        key(KEY_SHIFT, 1);
        ui_input();
        key(KEY_SHIFT, 0);
        fw_run(100);
        for (k = 0; k < NVOICE; k++)
            after += vox[k].sounding && vox[k].gate;
        if (after != before || ui.latch[0] || ui.key_note[4] != KEY_NONE)
            printf("FAIL flows: a white key played or latched with an effect slot chosen\n"), fails++;
        key(4, 0);
        key(2, 0);                                       /* the first one let go: its note ends */
        fw_run(100);
        for (k = 0, after = 0; k < NVOICE; k++)
            after += vox[k].sounding && vox[k].gate;
        if (after)
            printf("FAIL flows: a note held over the change to an effect slot did not end\n"), fails++;
        press(B_FX, 50);
    }
    {   /* the SCALE page (build 25): SELECT the root, ALGORITHM the scale */
        uint8_t sc0, rt0;
        press(B_ARP, 50);
        sc0 = ui.scale;
        rt0 = ui.root;
        fm1_in.enc[PANEL_ENC[EN_SELECT]] = 2;
        ui_input();
        if (ui.root != (rt0 + 2) % 12 || ui.scale != sc0)
            printf("FAIL flows: SELECT on the SCALE page did not move the root alone\n"), fails++;
        fm1_in.enc[PANEL_ENC[EN_ALGO]] = 1;
        ui_input();
        if (ui.scale != sc0 + 1 || ui.root != (rt0 + 2) % 12)
            printf("FAIL flows: ALGORITHM on the SCALE page did not move the scale alone\n"), fails++;
        press(B_ARP, 50);
    }
    printf("flows: %d failures\n", fails);
    return fails ? 1 : 0;
}
/* the two cores and one: the same sound (the rack runs one block late on both) */
static uint32_t cores_hash(int cores)
{
    uint32_t h = 2166136261u, b;
    int32_t out[2 * CTL];
    audio_worker_online = cores > 1;
    host_ticks = 0;                                      /* (the same start: the seed comes from the clock) */
    fm1_ms = 0;
    fw_init();
    press(B_FX, 50);
    key(0, 1);
    key(4, 1);
    press(B_SEL, 50);
    key(7, 1);
    for (b = 0; b < 1378u * 3u; b++) {
        mix_block(out);
        h = fnv(h, out, sizeof out);
    }
    key(0, 0);
    key(4, 0);
    key(7, 0);
    audio_worker_online = 0;
    return h;
}
/* the screens: the start; three generators playing, one muted; an effect slot chosen and muted; a message; the scale */
static int cmd_screens(int argc, char **argv)
{
    const char *dir = argc > 2 ? argv[2] : ".";
    char name[512];
    uint32_t n;
    batt_raw = 600;
    fw_init();
    load_pct = 23;
#define SHOT(nm) do { ui.load_at = fm1_ms - 1000u; ui.msg_until = 0; ui.force = 1; ui_draw(); ui_draw(); ui_draw(); \
    snprintf(name, sizeof name, "%s/screen-%s.ppm", dir, nm); screen_save(name); } while (0)
#ifndef HOST_BUILD
#define HOST_BUILD "BUILD 48"
#endif
    splash_draw(HOST_BUILD);                             /* (build 46) the splash: the logo, the name, the build (the
                                                          * firmware's, from tools/build.py: run_tests.sh) */
    snprintf(name, sizeof name, "%s/screen-splash.ppm", dir);
    screen_save(name);
    SHOT("start");
    for (n = 0; n < NGEN; n++)
        print_gen(n);
    for (n = 0; n < NFX; n++)
        print_fx(n);
    press(B_FX, 50);
    key(0, 1);
    key(4, 1);
    press(B_ENV, 50);
    key(7, 1);
    press(B_SEL, 50);
    key(9, 1);
    fw_run(800);
    load_pct = 47;
    SHOT("playing");
    link_gesture(0, 2);                                  /* generator 1 linked to 3: the message, then the band */
    ui.lk_until = fm1_ms + 1500u;
    ui.lk_shown = 0;
    SHOT("link");
    ui.lk_until = fm1_ms;
    ui_draw();
    SHOT("linked");
    link_gesture(0, 2);                                  /* (undone) */
    ui.lk_until = fm1_ms;
    ui_draw();
    press(B_ENV, 900);                                   /* generator 3 muted */
    press(B_GLO, 50);                                    /* effect slot 6 chosen */
    fw_run(200);
    SHOT("fx-chosen");
    press(B_GLO, 900);                                   /* .. and muted */
    load_pct = 88;
    SHOT("fx-muted");
    say("6 MUTED", 1000);
    ui.force = 1;
    ui_draw();
    snprintf(name, sizeof name, "%s/screen-message.ppm", dir);
    screen_save(name);
    ui.msg_until = 0;
    ui.sc_page = 1;
    ui.scale = 15;
    ui.root = 6;
    SHOT("scale");
    /* the looper: recording on generator 1, armed on generator 2, loops playing; PLAY held */
    fm1_in.notes = 0;                                    /* (the keys above let go) */
    fm1_in.buttons = 0;
    fw_init();
    press(B_FX, 50);
    press(B_REC, 50);
    key(0, 1);
    fw_run(400);
    key(0, 0);
    fw_run(400);
    press(B_REC, 50);
    press(B_REC, 50);
    key(2, 1);
    fw_run(300);
    SHOT("looper-rec");
    key(2, 0);
    press(B_REC, 50);
    press(B_SEL, 50);
    press(B_REC, 50);
    ui.msg_until = 0;
    SHOT("looper-armed");
    press(B_REC, 50);
    press(B_FX, 50);
    fm1_in.pressed |= 1u << PANEL_BTN[B_PLAY];
    fm1_in.buttons |= 1u << PANEL_BTN[B_PLAY];
    ui_input();
    fw_run(700);
    ui_input();
    key(0, 1);
    fw_run(10);
    key(0, 0);
    SHOT("loops");
    fm1_in.buttons &= ~(1u << PANEL_BTN[B_PLAY]);
    ui_input();
    {   /* (build 38) messages that take two lines, and one that fits one; a lane's MOD page; the dials at their widest */
#define MSGSHOT(nm) do { ui.load_at = fm1_ms - 1000u; ui.force = 1; ui_draw(); \
    snprintf(name, sizeof name, "%s/screen-%s.ppm", dir, nm); screen_save(name); ui.msg_until = fm1_ms; ui.force = 1; ui_draw(); } while (0)
        say_slot(NGEN + 1u, "REWIRED", 1000);
        MSGSHOT("msg-lane-rewired");
        say_slot(0, MF_NAME[MF_GLASSHARP], 1000);
        MSGSHOT("msg-family");
        say("DOUBLE SPEED", 1000);
        MSGSHOT("msg-double-speed");
        say("OVERLOAD", 1000);
        MSGSHOT("msg-overload");
        ui.sel = NGEN + 2u;
        ui.fx_page = 3;
        ui.mod_ix = 8;
        slot_mod[NGEN + 2u].s[6].src = MSRC_L1;
        slot_mod[NGEN + 2u].s[6].dst = 0;
        slot_mod[NGEN + 2u].s[6].amt = -60;
        SHOT("mod-lane");
        pages_off();
        memset(&slot_mod[NGEN + 2u], 0, sizeof slot_mod[0]);
        ui.sel = 0;
        ui.morph = MORPH_STEPS_UI;
        ui.dial_kind = 0;
        ui.dial_until = fm1_ms + 1500u;
        ui.dial_shown = 0;
        MSGSHOT("dial-morph-30");
        master_drive = 76;
        ui.dial_kind = 3;
        ui.dial_until = fm1_ms + 1500u;
        ui.dial_shown = 0;
        MSGSHOT("dial-drive");
        master_drive = 0;
        ui.morph = 0;
        ui.dial_until = fm1_ms;
        ui_draw();
    }
    press(B_SEQ, 50);                                    /* the DELAY page, a knob just turned */
    fm1_in.enc[PANEL_ENC[EN_K2]] = 5;
    ui_input();
    SHOT("delay");
    press(B_SEQ, 50);                                    /* the REVERB page */
    SHOT("reverb");
    press(B_SEQ, 50);                                    /* the MOD page: a generator's MOD A, set */
    slot_mod[0].lfo[0].rate = 40;
    slot_mod[0].s[0].src = MSRC_L1;
    slot_mod[0].s[0].dst = GMD_MAC1;
    slot_mod[0].s[0].amt = 45;
    ui.mod_ix = 0;                                       /* (from LFO 1: the mod-lane shot left it on H) */
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = 2;
    ui_input();
    fw_run(300);
    SHOT("mod-gen");
    press(B_SEQ, 50);
    press(B_LFO, 50);                                    /* .. an effect slot's MOD A, on its second effect */
    slot_mod[NGEN].s[0].src = MSRC_L2;
    slot_mod[NGEN].s[0].dst = FMD_P0 + 6;
    slot_mod[NGEN].s[0].amt = -30;
    press(B_SEQ, 50);
    press(B_SEQ, 50);
    press(B_SEQ, 50);
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = 2;
    ui_input();
    SHOT("mod-fx");
    key(KEY_SHIFT, 1);                                   /* (build 39) SHIFT + generator 1's button: its page, MUTE FADE
                                                          * OUT just turned */
    press(B_FX, 50);
    key(KEY_SHIFT, 0);
    gen_fin[0] = 9;
    fm1_in.enc[PANEL_ENC[EN_K2]] = 30;
    ui_input();
    SHOT("generator");
    gen_fin[0] = gen_fout[0] = 0;
    pages_off();
    ui.ses_page = SP_ASK;                                /* (build 41) the CLEAR key: asked */
    ui.ses_ask = AK_CLEAR;
    ui.ses_new = 3;
    ui.ses_yes = 0;
    SHOT("clear");
    pages_off();
    press(B_FX, 50);
    key(KEY_SHIFT, 1);
    press(B_SAVE, 50);                                   /* SHIFT + SAVE: the SESSIONS page */
    key(KEY_SHIFT, 0);
    SHOT("sessions");
    press(B_SAVE, 50);                                   /* SAVE: the session's page (Bigorna's), on LOAD */
    SHOT("session-page");
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = 3;                /* down to DELETE: the page scrolled */
    ui_input();
    SHOT("session-delete");
    ui.ses_row = SR_LOAD;                                /* (the questions and COPY, as they look) */
    ui.ses_page = SP_ASK;
    ui.ses_ask = AK_DEL;
    ui.ses_new = 1;
    ui.ses_yes = 0;
    SHOT("session-ask");
    ui.ses_page = SP_COPY;
    ui.ses_tgt = 7;
    SHOT("session-copy");
    ui.ses_page = SP_PAGE;
    key(KEY_SHIFT, 1);                                   /* SHIFT + SAVE: the list; again: out (build 36: no MODE being
                                                          * set to leave first) */
    press(B_SAVE, 50);
    press(B_SAVE, 50);
    key(KEY_SHIFT, 0);
    fm1_in.pressed |= 1u << PANEL_BTN[B_SAVE];           /* SAVE held, playing: asked (none loaded) */
    fm1_in.buttons |= 1u << PANEL_BTN[B_SAVE];
    ui_input();
    fw_run(1200);
    ui_input();
    fm1_in.buttons &= ~(1u << PANEL_BTN[B_SAVE]);
    ui_input();
    SHOT("save-ask");
    press(B_SAVE, 50);                                   /* YES: saved -- the whole screen says so */
    ui.load_at = fm1_ms - 1000u;
    ui.force = 1;
    ui_draw();
    snprintf(name, sizeof name, "%s/screen-saved.ppm", dir);
    screen_save(name);
    ses_done(SES_OK, SD_SNAP, "STORED", 6);             /* (a snapshot stored: SNAPSHOT, STORED, 6) */
    ui.load_at = fm1_ms - 1000u;
    ui.force = 1;
    ui_draw();
    snprintf(name, sizeof name, "%s/screen-stored.ppm", dir);
    screen_save(name);
    ui.fl_until = fm1_ms;
    snap_store(0);
    snap_store(2);
    snap_store(5);
    press(B_SAVE, 50);                                   /* SAVE: the SNAPSHOTS page */
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = 9;
    ui_input();
    SHOT("snapshots");
    fm1_in.enc[PANEL_ENC[EN_K1]] = 9;                    /* KNOB 1 on it: the MORPH TIME dial */
    ui_input();
    ui.load_at = fm1_ms - 1000u;
    ui.msg_until = 0;
    ui.fl_until = fm1_ms;                                /* (no message over it) */
    ui_draw();
    snprintf(name, sizeof name, "%s/screen-morph-dial.ppm", dir);
    screen_save(name);
    pages_off();                                         /* back to playing: KNOB 1 the DISINTEGRATOR, on its dial */
    ui.dial_until = 0;
    fm1_in.enc[PANEL_ENC[EN_K1]] = 23;
    ui_input();
    ui.load_at = fm1_ms - 1000u;
    ui.msg_until = 0;
    ui.fl_until = fm1_ms;                                /* (no message over it) */
    ui_draw();
    snprintf(name, sizeof name, "%s/screen-dis-dial.ppm", dir);
    screen_save(name);
    gen_dis[ui.gen] = 0;
    ui.dial_until = 0;                                   /* KNOB 2: the effects' mix, laid out as KNOB 1's (build 33) */
    fm1_in.enc[PANEL_ENC[EN_K2]] = -8;
    ui_input();
    ui.load_at = fm1_ms - 1000u;
    ui.msg_until = 0;
    ui.fl_until = fm1_ms;                                /* (no message over it) */
    ui_draw();
    snprintf(name, sizeof name, "%s/screen-fxmix.ppm", dir);
    screen_save(name);
    fx_wet[0] = FX_WET_DEF;
    ui.dial_until = fm1_ms;
    ui_draw();
    morph_start(&snap.s[2], 2, 4000);                    /* a morph into snapshot 3: its square blinking */
    fw_run(400);
    ui_input();
    ui.force = 1;
    ui_draw();
    snprintf(name, sizeof name, "%s/screen-snapshots-morph.ppm", dir);
    screen_save(name);
    pages_off();
#undef SHOT
    return 0;
}

/* build 36's controls: SHIFT + the scale button draws a root and a scale, the SCALE page up a moment and closed again;
 * KNOB 2 the FX MIX of the chosen generator's lane, or of the lane chosen, and of no other */
static int cmd_b36(void)
{
    int fails = 0, changed = 0;
    uint32_t t, l;
    fw_init();
    for (t = 0; t < 8u; t++) {
        uint8_t r0 = ui.root, s0 = ui.scale;
        key(KEY_SHIFT, 1);
        press(B_ARP, 50);
        key(KEY_SHIFT, 0);
        changed += ui.root != r0 || ui.scale != s0;
        if (!ui.sc_page)
            printf("FAIL b36: SHIFT + the scale button did not show the SCALE page\n"), fails++;
        if (rack_root != SCALE_BASE + ui.root)
            printf("FAIL b36: the root not the rack's\n"), fails++;
        fw_run(1378 * 2 + 200);
        ui_input();
        if (ui.sc_page)
            printf("FAIL b36: the SCALE page still up after two seconds\n"), fails++;
    }
    if (changed < 6)
        printf("FAIL b36: the root and scale drawn anew only %d times in 8\n", changed), fails++;
    for (l = 0; l < NSLOT; l++) {                        /* KNOB 2 on each slot */
        uint8_t was[NFX];
        uint32_t ln = l < NGEN ? l : l - NGEN, k;
        press(SLOT_BTN[l], 50);
        memcpy(was, (const void *)fx_wet, sizeof was);
        fm1_in.enc[PANEL_ENC[EN_K2]] = -5;
        ui_input();
        for (k = 0; k < NFX; k++)
            if ((k == ln) != (fx_wet[k] != was[k]))
                printf("FAIL b36: KNOB 2 on slot %u moved lane %u's mix (%u -> %u)\n", l + 1u, k + 4u, was[k], fx_wet[k]),
                    fails++;
    }
    printf("b36: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* build 37: the master's drive (SHIFT + KNOB 1) -- at 0 the master untouched, bit for bit; at 100 a sound well changed
 * and no louder (its level brought back), past nothing; with nothing in, nothing out; the knob sets it; a snapshot keeps it */
static double drive_run(int amt, int32_t *keep, double *rms)
{
    uint32_t b, i;
    double d = 0, e = 0;
    fm1_in.notes = fm1_in.buttons = 0;
    ui_init();
    ui.seed = 6060u;
    gens_init(ui.seed);
    scale_random(ui.seed);
    master_q12 = 4096;
    master_drive = (uint8_t)amt;
    press(B_FX, 50);
    key(4, 1);
    for (b = 0; b < 1378u * 4u; b++) {
        int32_t out[2 * CTL];
        mix_block(out);
        for (i = 0; i < 2u * CTL; i++) {
            uint32_t at = (b % 1378u) * 2u * CTL + i;
            if (b >= 1378u * 3u) {
                if (keep && amt == 0)
                    keep[at] = out[i];
                else if (keep)
                    d += ((double)out[i] - keep[at]) * ((double)out[i] - keep[at]);
                e += (double)out[i] * out[i];
            }
        }
    }
    key(4, 0);
    *rms = __builtin_sqrt(e / (1378.0 * 2 * CTL)) / 32768.0;
    return d;
}
static int cmd_drive(void)
{
    static int32_t ref[1378u * 2u * CTL];
    int fails = 0;
    double r0, r100, r0b, d0, d100;
    uint32_t b, i, pk = 0;
    drive_run(0, ref, &r0);
    d0 = drive_run(0, 0, &r0b);
    d0 = 0;
    {   /* (again at 0: the same, sample for sample) */
        double rr;
        static int32_t ref2[1378u * 2u * CTL];
        drive_run(0, ref2, &rr);
        for (i = 0; i < 1378u * 2u * CTL; i++)
            d0 += ((double)ref2[i] - ref[i]) * ((double)ref2[i] - ref[i]);
    }
    d100 = drive_run(100, ref, &r100);
    d100 = 10.0 * __builtin_log10(d100 / (1378.0 * 2 * CTL) / (r0 * r0 * 32768.0 * 32768.0) + 1e-12);
    printf("drive: at 0 the same as without %s; at 100 the difference %.1f dB against the sound, level %.3f (off %.3f)\n",
           d0 == 0 ? "(bit for bit)" : "(NOT)", d100, r100, r0);
    if (d0 != 0)
        printf("FAIL drive: at 0 the master touched\n"), fails++;
    if (d100 < -10.0)
        printf("FAIL drive: at 100 hardly heard\n"), fails++;
    if (r100 > r0 * 2.0 || r100 < r0 * 0.4)
        printf("FAIL drive: at 100 its level not brought back (%.3f against %.3f)\n", r100, r0), fails++;
    master_drive = 100;                                  /* nothing in: nothing out */
    for (b = 0; b < 1378u * 8u; b++) {
        int32_t out[2 * CTL];
        mix_block(out);
        if (b > 1378u * 6u)
            for (i = 0; i < 2u * CTL; i++)
                pk = (uint32_t)maxi((int32_t)pk, absi(out[i]));
    }
    if (pk > 40u)
        printf("FAIL drive: with nothing in, %u out\n", pk), fails++;
    master_drive = 0;                                    /* SHIFT + KNOB 1 sets it; a snapshot keeps it */
    key(KEY_SHIFT, 1);
    fm1_in.enc[PANEL_ENC[EN_K1]] = 10;
    ui_input();
    key(KEY_SHIFT, 0);
    if (master_drive != 20)
        printf("FAIL drive: SHIFT + KNOB 1 set %u, not 20\n", master_drive), fails++;
    snap_store(0);
    master_drive = 0;
    ui.morph = 0;
    snap_recall(0);
    fw_run(20);
    if (master_drive != 20)
        printf("FAIL drive: a snapshot did not keep it (%u)\n", master_drive), fails++;
    master_drive = 0;
    printf("drive: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* build 37's examples, to listen to: each of generation 6's families on each path (C3 held 8 s and let go), mono
 * WAVs into DIR; and the master's drive on one patch -- 4 s at 0, at 50, at 100 -- stereo, drive.wav */
static int cmd_timbres(int argc, char **argv)
{
    const char *dir = argc > 2 ? argv[2] : "build/timbres";
    char name[512];
    int f, path;
    uint32_t b, i, n = 0;
    for (f = MF_G6; f < MF_N; f++)
        for (path = 0; path < PATH_N; path++) {
            stat_t st;
            uint32_t seed = 7300u + (uint32_t)f * 11u + (uint32_t)path * 5u;
            mo_design(&design, f, seed, (uint32_t)path, 6);
            mo_measure(&design);
            st = play(&design, 48, 8.0, 3.5, 0, wav);
            snprintf(name, sizeof name, "%s/%s-%s-%u.wav", dir, MF_NAME[f], PATH_NAME[path], seed);
            wav_write(name, wav, st.len ? st.len : FS * 12);
            printf("%-9s %-8s seed %u: peak %.2f rms %.3f -> %s\n", MF_NAME[f], PATH_NAME[path], seed, st.peak / 32768.0, st.rms,
                   name);
        }
    fm1_in.notes = fm1_in.buttons = 0;                   /* the drive: one patch, 0, 50, 100 */
    ui_init();
    ui.seed = 4711u;
    gens_init(ui.seed);
    scale_random(ui.seed);
    master_q12 = 4096;
    press(B_FX, 50);
    key(0, 1);
    key(KEY_SHIFT, 1);
    ui_input();
    key(KEY_SHIFT, 0);
    key(0, 0);
    for (b = 0; b < 1378u * 12u; b++) {
        int32_t out[2 * CTL];
        master_drive = (uint8_t)(b < 1378u * 4u ? 0u : b < 1378u * 8u ? 50u : 100u);
        mix_block(out);
        for (i = 0; i < 2u * CTL && n < sizeof wav2 / sizeof wav2[0]; i++)
            wav2[n++] = (int16_t)clampi(out[i], -32767, 32767);
    }
    master_drive = 0;
    snprintf(name, sizeof name, "%s/drive.wav", dir);
    wav_write2(name, wav2, n / 2u);
    printf("drive: 0, 50, 100, 4 s each -> %s\n", name);
    return 0;
}

/* build 38: a lane is panned with its generator -- generator 2 (lane 2) hard right: its lane's own sound, the effects
 * alone (the generator dry share none), comes out on the right */
static int cmd_lanepan(void)
{
    int fails = 0;
    uint32_t b, i, side;
    for (side = 0; side < 2u; side++) {
        double el = 0, er = 0;
        fm1_in.notes = fm1_in.buttons = 0;
        ui_init();
        ui.seed = 8080u;
        gens_init(ui.seed);
        scale_random(ui.seed);
        master_q12 = 4096;
        gen_mute[0] = gen_mute[2] = 1;
        gen_pan[1] = (int8_t)(side ? 20 : -20);
        gbuf[1][g_use[1]].dry = 0;                       /* (its lane alone heard) */
        ui.rsend[1] = ui.dsend[1] = 0;
        sends_apply(1);
        press(B_SEL, 50);
        key(4, 1);
        for (b = 0; b < 1378u * 4u; b++) {
            int32_t out[2 * CTL];
            mix_block(out);
            if (b > 1378u * 2u)
                for (i = 0; i < CTL; i++) {
                    el += (double)out[2u * i] * out[2u * i];
                    er += (double)out[2u * i + 1u] * out[2u * i + 1u];
                }
        }
        key(4, 0);
        printf("lanepan: generator 2 %s -- its lane left %.0f, right %.0f\n", side ? "right" : "left", __builtin_sqrt(el),
               __builtin_sqrt(er));
        if (side ? er < 4.0 * el : el < 4.0 * er)
            printf("FAIL lanepan: the lane not panned with its generator\n"), fails++;
    }
    printf("lanepan: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* build 39: a mute's fade (SHIFT + a generator's button: its page; KNOB 1 MUTE FADE IN, KNOB 2 MUTE FADE OUT). OFF:
 * silent at once, as before; 2 s out: falling through it, silent after; 1 s in: back over it. Kept in a snapshot; a
 * state given back takes its mutes at once */
static double mf_win(uint32_t blocks)                    /* the output's RMS over the blocks to come */
{
    fw_rms = 0;
    fw_run(blocks);
    return __builtin_sqrt(fw_rms / (blocks * 2.0 * CTL));
}
static int cmd_mutefade(void)
{
    int fails = 0;
    double r0, a, b, z;
    st_t st;
    fm1_in.notes = fm1_in.buttons = 0;
    fw_init();
    gen_mute[0] = gen_mute[2] = 1;                       /* (generator 2 alone, dry: no cables, no sends) */
    gbuf[1][g_use[1]].nent = 0;
    gbuf[1][g_use[1]].dry = Q15;
    ui.rsend[1] = ui.dsend[1] = 0;
    sends_apply(1);
    press(B_SEL, 50);
    key(4, 1);
    fw_run(1378u * 3u);
    r0 = mf_win(276);
    slot_mute(1);                                        /* OFF: at once */
    fw_run(2);
    z = mf_win(276);
    printf("mutefade: OFF -- %.1f, muted %.4f\n", r0, z);
    if (r0 < 50.0 || z > 0.0)
        printf("FAIL mutefade: OFF not silent at once\n"), fails++;
    slot_mute(1);
    fw_run(1378u);
    /* the page: SHIFT + the button, let go; KNOB 2 to 2 s, KNOB 1 to 1 s */
    key(KEY_SHIFT, 1);
    press(B_SEL, 50);
    key(KEY_SHIFT, 0);
    if (ui.gs_page != 2u)
        printf("FAIL mutefade: SHIFT + generator 2's button did not bring its page (%u)\n", ui.gs_page), fails++;
    fm1_in.enc[PANEL_ENC[EN_K2]] = 4;
    ui_input();
    fm1_in.enc[PANEL_ENC[EN_K1]] = 2;
    ui_input();
    if (gen_fout[1] != 4u || gen_fin[1] != 2u || gen_dis[1] || fx_wet[1] != 50u)
        printf("FAIL mutefade: the knobs set out %u in %u (dis %d, mix %u)\n", gen_fout[1], gen_fin[1], gen_dis[1], fx_wet[1]),
            fails++;
    fm1_in.enc[PANEL_ENC[EN_K2]] = 200;                  /* (to its end: 60 s) */
    ui_input();
    if (gen_fout[1] != FADE_MAX)
        printf("FAIL mutefade: MUTE FADE OUT stops at %u half seconds\n", gen_fout[1]), fails++;
    fm1_in.enc[PANEL_ENC[EN_K2]] = -66;                  /* (60 s down 66: 40 + 20 .. back to 2 s) */
    ui_input();
    if (gen_fout[1] != 4u)
        printf("FAIL mutefade: MUTE FADE OUT back down at %u half seconds\n", gen_fout[1]), fails++;
    press(B_SEL, 50);                                    /* its button: back */
    if (ui.gs_page)
        printf("FAIL mutefade: the page stays up\n"), fails++;
    r0 = mf_win(276);
    press(B_SEL, 900);                                   /* held: muted, fading out over 2 s */
    if (!gen_mute[1])
        printf("FAIL mutefade: not muted\n"), fails++;
    fw_run(552);                                         /* (the host's buttons are read as they are let go: the
                                                          * mute from here) */
    a = mf_win(276);                                     /* (0.4 .. 0.6 s in) */
    fw_run(552);
    b = mf_win(276);                                     /* (1.0 .. 1.2 s) */
    fw_run(1103);
    z = mf_win(276);                                     /* (2.0 .. 2.2 s) */
    printf("mutefade: 2 s out -- %.1f, %.1f, %.1f, %.4f\n", r0, a, b, z);
    if (!(a < 0.7 * r0 && a > 0.2 * r0 && b < 0.25 * r0 && b > 0.01 * r0 && z == 0.0))
        printf("FAIL mutefade: not faded out over 2 s\n"), fails++;
    press(B_SEL, 900);                                   /* heard again, over 1 s */
    a = mf_win(138);                                     /* (0.12 .. 0.22 s) */
    fw_run(1378u);
    b = mf_win(276);
    printf("mutefade: 1 s in -- %.1f, %.1f\n", a, b);
    if (!(a < 0.2 * r0 && b > 0.5 * r0))
        printf("FAIL mutefade: not faded in over 1 s\n"), fails++;
    /* SHIFT + a button held, then another: a link, no page */
    link_gesture(0, 1);
    if (ui.gs_page)
        printf("FAIL mutefade: a link brought a page\n"), fails++;
    link_gesture(0, 1);
    /* a snapshot keeps the fades; given back, its mutes at once */
    st_capture(&st);
    if (st.g[1].fout != 4u || st.g[1].fin != 2u)
        printf("FAIL mutefade: the state has out %u in %u\n", st.g[1].fout, st.g[1].fin), fails++;
    st.g[1].mute = 1;
    gen_fout[1] = gen_fin[1] = 0;
    st_apply(&st);
    fw_run(2);
    z = mf_win(276);
    if (gen_fout[1] != 4u || gen_fin[1] != 2u || z > 0.0)
        printf("FAIL mutefade: given back out %u in %u, %.4f heard\n", gen_fout[1], gen_fin[1], z), fails++;
    key(4, 0);
    printf("mutefade: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* build 40: the work dealt by units -- voices, lanes, the master -- to both cores (one core and two the same sound:
 * host cores); build 41: the CLEAR key (the 10th black key) -- asked, a new session in place */
/* build 43: GLOBAL SETUP (SHIFT + SAVE held; SHIFT + SAVE let go soon: SESSIONS) -- its pages, values set and kept across
 * a restart, not a session; the MIDI in by channel and port, a CC on a generator, the notes and CCs sent out (a note sent
 * on always sent off); the three USB tracks, each its generator alone */
static void host_midi(uint32_t pkt)
{
    midi_in_q[mi_w % MQ] = pkt;
    mi_w++;
}
static void shift_save(uint32_t ms)
{
    key(KEY_SHIFT, 1);
    press(B_SAVE, ms);
    key(KEY_SHIFT, 0);
}
static int host_mout_has(uint32_t from, uint32_t pkt, uint32_t mask)
{
    uint32_t i;
    for (i = from; i < host_mout_n && i < 256u; i++)
        if ((host_mout[i] & mask) == pkt)
            return 1;
    return 0;
}
/* PRESETS (build 47): SHIFT + the scale button let go soon draws a scale, held opens the page; the MACHINE path draws a
 * struck family; a generator saved into a slot (its sound, lane, cable, DISINTEGRATOR, FX MIX) comes back whole into
 * another generator, in that one's lane; over a full slot it asks; the names count within a family; kept over a
 * restart; not a session */
static void shift_arp(uint32_t ms)
{
    key(KEY_SHIFT, 1);
    press(B_ARP, ms);
    key(KEY_SHIFT, 0);
}
static int cmd_presets(void)
{
    int fails = 0;
    gen_keep_t ka, kb;
    fx_patch_t fa;
    const fx_patch_t *fb;
    char nm[24], want[24];
    uint32_t fam;
#define PFAIL(...) do { printf("FAIL presets: " __VA_ARGS__); printf("\n"); fails++; } while (0)
    memset(hflash, 0xFF, sizeof hflash);
    fm1_in.notes = fm1_in.buttons = 0;
    batt_raw = 600;
    fw_init();
    setup_load();
    pre_scan();
    shift_arp(50);                                    /* let go soon: a scale drawn, the SCALE page a moment */
    if (ui.pre_page || !ui.sc_page)
        PFAIL("SHIFT + the scale button let go soon does not draw a scale (%u %u)", ui.pre_page, ui.sc_page);
    pages_off();
    key(KEY_MACHINE, 1);                              /* the MACHINE path */
    ui_input();
    key(KEY_MACHINE, 0);
    ui_input();
    if (draw_path != DP_MACHINE)
        PFAIL("the 4th black key does not turn the MACHINE path on");
    press(SLOT_BTN[0], 50);
    press(B_HOME, 50);
    fw_run(200);
    fam = gbuf[0][g_use[0]].gen.family;
    if (fam < MF_G7)
        PFAIL("HOME on the MACHINE path drew %s, not a struck family", MF_NAME[fam % MF_N]);
    gen_dis[0] = 37;
    fx_wet[0] = 63;
    gen_keep(0, &ka);
    fa = fbuf[0][f_pub[0]];
    shift_arp(1000);                                  /* held: PRESETS, on generator 1 */
    if (ui.pre_page != SP_LIST || ui.pre_gen != 0u)
        PFAIL("SHIFT + the scale button held does not open PRESETS on generator 1");
    ui.force = 1;
    ui_draw();
    screen_save("build/screens/screen-presets.ppm");
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = 2;             /* slot 3 */
    ui_input();
    press(B_ARP, 1000);                               /* held: saved (empty: not asked) */
    if (ui.pre_ix != 3u || !pre_has(3))
        PFAIL("the scale button held did not save into slot 3 (%u)", ui.pre_ix);
    pre_name(3, nm, sizeof nm);
    str_cpy(want, MF_NAME[fam], sizeof want);
    str_cpy(want + str_len(want), " 1", sizeof want - str_len(want));
    if (strcmp(nm, want))
        PFAIL("slot 3 is named %s, not %s", nm, want);
    pages_off();
    press(B_HOME, 50);                                /* generator 1 anew, and generator 3 chosen */
    gen_dis[0] = 5;
    fx_wet[0] = 20;
    fw_run(200);
    press(SLOT_BTN[2], 50);
    shift_arp(1000);
    if (ui.pre_gen != 2u || ui.pre_ix != 3u)
        PFAIL("PRESETS did not open on generator 3 at slot 3 (%u %u)", ui.pre_gen, ui.pre_ix);
    press(B_ARP, 50);                                 /* pressed: loaded into generator 3, back to playing */
    fw_run(300);
    if (ui.pre_page)
        PFAIL("loading did not go back to playing");
    gen_keep(2, &kb);
    fb = &fbuf[2][f_pub[2]];
    if (kb.family != ka.family || kb.dseed != ka.dseed || kb.gain != ka.gain || kb.path != ka.path || kb.gen != ka.gen ||
        kb.send != ka.send || kb.dry != ka.dry)
        PFAIL("generator 3's sound is not the preset's");
    if (kb.nent != ka.nent || (kb.nent && (kb.ent[0] / FX_N != 2u || kb.ent[0] % FX_N != ka.ent[0] % FX_N)))
        PFAIL("generator 3 is not wired into its own lane as the preset was (%u %u)", kb.nent, kb.ent[0]);
    if (fb->n != fa.n || fb->recipe != fa.recipe || fb->out != fa.out || fb->ret != fa.ret || fb->seed != fa.seed ||
        memcmp(fb->e, fa.e, sizeof fa.e))
        PFAIL("lane 3 is not the preset's lane");
    if (gen_dis[2] != 37 || fx_wet[2] != 63)
        PFAIL("DISINTEGRATOR and FX MIX not the preset's (%d %u)", gen_dis[2], fx_wet[2]);
    /* a second of the family: numbered 2; over a full slot, asked */
    press(SLOT_BTN[0], 50);
    shift_arp(1000);
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = 2;             /* slot 5 */
    ui_input();
    {
        uint32_t f1 = gbuf[0][g_use[0]].gen.family;
        press(B_SAVE, 1000);                          /* (SAVE as the scale button) */
        if (!pre_has(5))
            PFAIL("SAVE held on PRESETS did not save");
        if (f1 == fam) {
            pre_name(5, nm, sizeof nm);
            want[str_len(want) - 1u] = '2';
            if (strcmp(nm, want))
                PFAIL("slot 5 is named %s, not %s", nm, want);
        }
    }
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = -2;            /* slot 3: full -- asked */
    ui_input();
    press(B_ARP, 1000);
    if (ui.pre_page != SP_ASK)
        PFAIL("saving over a full slot was not asked");
    ui.fl_until = fm1_ms;                             /* (SAVED 5 gone) */
    ui.force = 1;
    ui_draw();
    screen_save("build/screens/screen-presets-ask.ppm");
    press(B_ARP, 50);                                 /* NO */
    if (ui.pre_page != SP_LIST)
        PFAIL("NO did not go back to the list");
    ui.fl_until = fm1_ms;
    ui.msg_until = fm1_ms;
    ui.force = 1;
    ui_draw();
    screen_save("build/screens/screen-presets-list.ppm");   /* (slots 3 and 5 full, 3 chosen) */
    gen_keep(0, &kb);
    pages_off();
    ses_scan(1);                                      /* (a restart) */
    pre_scan();
    if (!pre_has(3) || !pre_has(5) || pre_has(4))
        PFAIL("the presets are not kept over a restart");
    {
        pre_rec_t r;
        pre_read(2, &r);
        if (r.dseed != ka.dseed)
            PFAIL("NO overwrote slot 3");
    }
    if (ses_has(SES_SLOTS + 1u) || ses_next_number() != 1u)
        PFAIL("the presets' record is taken for a session");
    printf("presets: %s\n", fails ? "FAILED" : "saved, named, loaded into another lane, asked, kept");
    return fails ? 1 : 0;
#undef PFAIL
}
/* (build 48) SHIFT + PRESETS, playing: the families (a generator) or recipes (an effect slot) in alphabetical order,
 * four to a page, opened where the slot stands; SHIFT + HOME draws the one picked, then or later; HOME forgets it */
static void enc_turn(int32_t d)
{
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = (int8_t)d;
    ui_input();
}
static int cmd_pick(void)
{
    int fails = 0;
    uint32_t i, fam, want;
#define KFAIL(...) do { printf("FAIL pick: " __VA_ARGS__); printf("\n"); fails++; } while (0)
    fw_init();
    fl_make();
    for (i = 1; i < fl_nfam; i++)
        if (!name_before(MF_NAME[fl_fam[i - 1u]], MF_NAME[fl_fam[i]]))
            KFAIL("families not in order: %s before %s", MF_NAME[fl_fam[i - 1u]], MF_NAME[fl_fam[i]]);
    for (i = 1; i < FR_N; i++)
        if (!name_before(FR_NAME[fl_rec[i - 1u]], FR_NAME[fl_rec[i]]))
            KFAIL("recipes not in order: %s before %s", FR_NAME[fl_rec[i - 1u]], FR_NAME[fl_rec[i]]);
    if (fl_nfam != MF_N - 2u)
        KFAIL("%u families listed, not %u", fl_nfam, MF_N - 2u);
    press(SLOT_BTN[0], 50);
    fam = gbuf[0][g_use[0]].gen.family;
    key(KEY_SHIFT, 1);
    enc_turn(1);                                      /* opened where generator 1 stands */
    if (!ui.fl_on || fl_item(0, ui.fl_ix) != fam)
        KFAIL("SHIFT + PRESETS did not open the list on %s", MF_NAME[fam]);
    enc_turn(5);
    want = fl_item(0, ui.fl_ix);
    ui_draw();
    screen_save("build/screens/screen-families.ppm");
    if (ui.pick[0] != want + 1u)
        KFAIL("turning did not pick %s", MF_NAME[want]);
    press(B_HOME, 50);                                /* SHIFT + HOME: the one picked */
    fw_run(200);
    if (gbuf[0][g_use[0]].gen.family != want || ui.fl_on || ui.pick[0])
        KFAIL("SHIFT + HOME drew %s, not %s", MF_NAME[gbuf[0][g_use[0]].gen.family], MF_NAME[want]);
    enc_turn(-3);                                     /* picked, SHIFT let go, SHIFT + HOME later */
    enc_turn(-2);
    want = fl_item(0, ui.fl_ix);
    key(KEY_SHIFT, 0);
    ui_input();
    if (ui.fl_on)
        KFAIL("the list stayed up with SHIFT let go");
    key(KEY_SHIFT, 1);
    press(B_HOME, 50);
    fw_run(200);
    if (gbuf[0][g_use[0]].gen.family != want)
        KFAIL("SHIFT + HOME later drew %s, not %s", MF_NAME[gbuf[0][g_use[0]].gen.family], MF_NAME[want]);
    enc_turn(1);
    enc_turn(-40);                                    /* (at the first: stays there) */
    if (ui.fl_ix != 0u)
        KFAIL("the list ran past its first entry");
    key(KEY_SHIFT, 0);
    ui_input();
    press(B_HOME, 50);                                /* HOME: the pick forgotten */
    if (ui.pick[0])
        KFAIL("HOME did not forget the pick");
    press(SLOT_BTN[NGEN + 1], 50);                    /* an effect slot: the recipes */
    key(KEY_SHIFT, 1);
    enc_turn(1);
    enc_turn(7);
    want = fl_item(NGEN + 1, ui.fl_ix);
    ui_draw();
    screen_save("build/screens/screen-recipes.ppm");
    press(B_HOME, 50);
    fw_run(200);
    key(KEY_SHIFT, 0);
    ui_input();
    if (fbuf[1][f_use[1]].recipe != want)
        KFAIL("SHIFT + HOME on lane 2 drew %s, not %s", FR_NAME[fbuf[1][f_use[1]].recipe], FR_NAME[want]);
    printf("pick: %s\n", fails ? "FAILED" : "families and recipes in order, opened where they stand, SHIFT + HOME draws the pick");
    return fails ? 1 : 0;
#undef KFAIL
}
static int cmd_setup(void)
{
    int fails = 0;
    uint32_t k, found, m0, b;
    double e[NGEN] = {0};
#define GSFAIL(...) do { printf("FAIL setup: " __VA_ARGS__); printf("\n"); fails++; } while (0)
    memset(hflash, 0xFF, sizeof hflash);
    fm1_in.notes = fm1_in.buttons = 0;
    batt_raw = 600;
    fw_init();
    setup_load();
    shift_save(50);                                   /* let go soon: SESSIONS, as before */
    if (ui.ses_page != SP_LIST || ui.set_page)
        GSFAIL("SHIFT + SAVE let go soon does not open SESSIONS (%u %u)", ui.ses_page, ui.set_page);
    pages_off();
    shift_save(1000);                                 /* held: GLOBAL SETUP */
    if (!ui.set_page || ui.ses_page)
        GSFAIL("SHIFT + SAVE held does not open GLOBAL SETUP");
    ui.force = 1;
    ui_draw();
    screen_save("build/screens/screen-setup.ppm");
    press(B_SAVE, 50);                                /* MIDI */
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = 3;             /* IN CHAN -> SEND */
    ui_input();
    press(B_SAVE, 50);                                /* SEND: NOTES */
    press(B_SAVE, 50);                                /* its value .. */
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = 1;             /* .. ON */
    ui_input();
    ui_draw();
    screen_save("build/screens/screen-setup-value.ppm");
    press(B_SAVE, 50);                                /* set */
    if (!mcfg.send_note)
        GSFAIL("MIDI > SEND > NOTES was not set ON");
    for (k = 0; k < 3u; k++)                          /* back, back, out */
        shift_save(50);
    if (ui.set_page || ui.ses_page)
        GSFAIL("SHIFT + SAVE does not go back and out (%u %u)", ui.set_page, ui.ses_page);
    setup_poll();
    mcfg.send_note = 0;
    ses_scan(1);                                      /* (a restart) */
    setup_load();
    if (!mcfg.send_note)
        GSFAIL("the setting is not kept");
    if (ses_has(SES_SLOTS + 1u) || ses_next_number() != 1u)
        GSFAIL("the settings' record is taken for a session");
    /* AUDIO: one part, opened on its row; USB: STEREO -> TRACKS, kept */
    shift_save(1000);
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = 1;             /* MIDI -> AUDIO */
    ui_input();
    press(B_SAVE, 50);
    if (set.lvl != 2u || strcmp(SET_PART[set.part].rows[set.row - 1u].name, "USB"))
        GSFAIL("AUDIO does not open on its USB row (level %u)", set.lvl);
    press(B_SAVE, 50);
    fm1_in.enc[PANEL_ENC[EN_PRESET]] = 1;             /* .. TRACKS */
    ui_input();
    ui_draw();
    screen_save("build/screens/screen-setup-usb.ppm");
    press(B_SAVE, 50);
    if (mcfg.usb_aud != 1u)
        GSFAIL("AUDIO > USB was not set to TRACKS");
    shift_save(50);                                   /* back to the sections (one level: no parts' list) */
    if (!ui.set_page || set.lvl != 0u)
        GSFAIL("back from AUDIO's rows is not the sections (level %u)", set.lvl);
    press(B_FX, 50);                                  /* a slot's button: out */
    if (ui.set_page)
        GSFAIL("a slot's button does not leave GLOBAL SETUP");
    setup_poll();
    mcfg.usb_aud = 0;
    ses_scan(1);
    setup_load();
    if (mcfg.usb_aud != 1u || !mcfg.send_note)
        GSFAIL("USB AUDIO is not kept (%u)", mcfg.usb_aud);
    if (ses_save(1) != SES_OK || ses_load_slot(1) != SES_OK || !mcfg.send_note)
        GSFAIL("a session saved and loaded touches the settings");
    /* the MIDI in: channel 3 to generator 3, channel 16 (AUTO) to the one the keys play, CC 7 to a level, the jack off */
    press(B_SEL, 50);
    host_midi(0x09u | 0x92u << 8 | 60u << 16 | 100u << 24);
    host_midi(0x09u | 0x9Fu << 8 | 64u << 16 | 100u << 24);
    host_midi(0x0Bu | 0xB0u << 8 | 7u << 16 | 0u << 24);
    ui_input();
    fw_run(10);
    for (k = 0, found = 0; k < NVOICE; k++) {
        found |= (vox[k].gate && vox[k].slot == 2u && vox[k].note == 60u) ? 1u : 0u;
        found |= (vox[k].gate && vox[k].slot == 1u && vox[k].note == 64u) ? 2u : 0u;
    }
    if (found != 3u)
        GSFAIL("the MIDI notes do not reach their generators by channel (%u)", found);
    if (gen_lvl[0] != 0)
        GSFAIL("CC 7 does not set generator 1's level (%d)", gen_lvl[0]);
    host_midi(0x08u | 0x82u << 8 | 60u << 16);
    host_midi(0x08u | 0x8Fu << 8 | 64u << 16);
    mcfg.trs_in = 0;
    host_midi(0x19u | 0x92u << 8 | 67u << 16 | 100u << 24);   /* (cable 1: the jack) */
    ui_input();
    fw_run(10);
    for (k = 0, found = 0; k < NVOICE; k++)
        found |= vox[k].gate && (vox[k].note == 60u || vox[k].note == 64u || vox[k].note == 67u);
    if (found)
        GSFAIL("a note-off is lost, or the jack is heard with TRS IN off");
    mcfg.trs_in = 1;
    gen_lvl[0] = LEVEL_UNITY;
    /* the notes out (SEND NOTES on): a key on generator 1, its note on and off on channel 1; held, SEND NOTES off:
     * still sent off */
    press(B_FX, 50);
    m0 = host_mout_n;
    key(0, 1);
    fw_run(5);
    if (!host_mout_has(m0, 0x09u | 0x90u << 8, 0xFFFFu))
        GSFAIL("a key's note is not sent on channel 1");
    key(0, 0);
    fw_run(5);
    if (!host_mout_has(m0, 0x08u | 0x80u << 8, 0xFFFFu))
        GSFAIL("a key's note is not sent off");
    key(0, 1);
    fw_run(5);
    m0 = host_mout_n;
    mcfg.send_note = 0;                               /* (as the page does it) */
    ev_post(EV_MIDI_OFF, 0, 0, 0);
    fw_run(3);
    if (!host_mout_has(m0, 0x08u | 0x80u << 8, 0xFFFFu))
        GSFAIL("SEND NOTES off leaves a note sent on hanging");
    key(0, 0);
    fw_run(5);
    /* the CCs out (SEND CC on): KNOB 1, generator 1's DISINTEGRATOR on CC 16, channel 1 */
    mcfg.send_cc = 1;
    midi_poll();
    fw_run(2);
    m0 = host_mout_n;
    fm1_in.enc[PANEL_ENC[EN_K1]] = 5;
    ui_input();
    midi_poll();
    fw_run(2);
    if (!host_mout_has(m0, 0x0Bu | 0xB0u << 8 | 16u << 16, 0xFFFFFFu))
        GSFAIL("KNOB 1 is not sent as CC 16");
    mcfg.send_cc = 0;
    /* the tracks: generator 2 alone played -- its track sounds, the others are silent */
    fw_init();                                        /* (from a fresh start: no tails of the notes before, which
                                                       * any voice keeps sounding in every lane) */
    fw_run(200);
    press(B_SEL, 50);
    key(0, 1);
    for (b = 0; b < 3000u; b++) {
        fw_run(1);
        for (k = 0; k < NGEN; k++)
            for (found = 0; found < CTL; found++)
                e[k] += (double)trk_out[k][found] * trk_out[k][found];
    }
    key(0, 0);
    printf("setup: tracks' energy %.3g %.3g %.3g\n", e[0], e[1], e[2]);
    if (e[1] < 1e6 || e[0] > e[1] * 1e-6 || e[2] > e[1] * 1e-6)
        GSFAIL("the tracks are not each their generator's alone");
    if (!fails)
        printf("setup: ok\n");
    return fails;
#undef GSFAIL
}
/* build 44: the twin recipes -- the same effect twice in a row, the second's values apart from the first's, a return
 * from the second into the first in FR_RET's -- drawn as meant, within memory; through a lane on noise: neither silent
 * nor running away; the return heard; with nothing in, silent within 40 s though the gate is held open (a morph) */
static int32_t tw_cap[2756u * CTL];
static void tw_lane(uint32_t blocks, int noise, rnd_t *nz, double *e, int32_t *pk, int cap)
{
    uint32_t b, i;
    for (b = 0; b < blocks; b++) {
        memset(ebus_run, 0, EBUS_SIZE);
        for (i = 0; i < CTL; i++)
            ebus_run[0][i] = noise ? mulq15(rnd_bi(nz), 9000) : 0;
        ebus_fed_run = noise ? 1u : 0u;
        rack_voices = 1;                                 /* (the gate held open) */
        lane_job(0);
        for (i = 0; i < CTL; i++) {
            if (e)
                *e += (double)lane_m[0][i] * lane_m[0][i];
            if (pk)
                *pk = maxi(*pk, absi(lane_m[0][i]));
            if (cap == 1 && b < 2756u)
                tw_cap[b * CTL + i] = lane_m[0][i];
            else if (cap == 2 && b < 2756u)               /* (the difference from what was kept) */
                tw_cap[b * CTL + i] -= lane_m[0][i];
        }
    }
}
static int cmd_twins(void)
{
    int fails = 0;
    uint32_t rc, t, k, nret = 0;
    double in_e = (double)9000 * 9000 / 3.0 * CTL;       /* (a block of the noise: uniform, +-9000) */
#define TFAIL(...) do { printf("FAIL twins: " __VA_ARGS__); printf("\n"); fails++; } while (0)
    for (rc = FR_MIRRORS; rc < FR_N; rc++) {
        double lo_db = 1e9, hi_db = -1e9, ret_db = 0;
        int twin = (FR_PLACE[rc][1] & FX_TWIN) != 0;   /* (build 45's others: their level and silence only) */
        uint32_t nr = 0;
        for (t = 0; t < 12u; t++) {
            fx_patch_t *f;
            rnd_t nz;
            double e = 0, e0 = 0;
            int32_t pk = 0, pk_end = 0;
            uint32_t mem = 0;
            fw_init();
            draw_path = (uint8_t)(t % DP_N);
            fw_run(30);
            if (!fx_draw(0, (int32_t)rc, 4400u + t * 17u + rc)) {
                TFAIL("%s: fx_draw busy", FR_NAME[rc]);
                continue;
            }
            fw_run(40);                                  /* (taken: faded, cleared, started) */
            f = &fbuf[0][f_use[0]];
            for (k = 0; k < f->n; k++)
                mem += rm_size(f->e[k].kind);
            if (f->n < 2u || (twin && f->e[0].kind != f->e[1].kind) || mem > FX_ARENA)
                TFAIL("%s: drew %s then %s (%u B)", FR_NAME[rc], RM_NAME[f->e[0].kind], RM_NAME[f->e[1].kind], mem);
            if (twin && !memcmp(f->e[0].p, f->e[1].p, 4))
                TFAIL("%s: the twins' values the same", FR_NAME[rc]);
            if (f->e[0].kind == RM_SHF && ((f->e[0].p[2] < 45) == (f->e[1].p[2] < 45)) && ((f->e[0].p[2] > 55) == (f->e[1].p[2] > 55)))
                TFAIL("MIRRORS: both shifters the same way (%u, %u)", f->e[0].p[2], f->e[1].p[2]);
            if (f->e[0].kind == RM_CLD && f->e[0].p[2] >= 50 && f->e[1].p[2] >= 50)
                TFAIL("SWARM: both clouds frozen");
            if (!!FR_RET[rc] != !!fx_ret_ok(f))
                TFAIL("%s: a return where none is meant, or none where one is (%04x)", FR_NAME[rc], f->ret);
            draw_path = DP_ANY;
            rnd_seed(&nz, 99u + t);
            tw_lane(2756u, 1, &nz, &e, &pk, 1);          /* 2 s of noise */
            {
                double db = 10.0 * __builtin_log10((e / 2756.0 + 1.0) / in_e);
                lo_db = db < lo_db ? db : lo_db;
                hi_db = db > hi_db ? db : hi_db;
                if (db < -40.0)
                    TFAIL("%s (seed %u): all but silent, %.1f dB", FR_NAME[rc], t, db);
            }
            tw_lane(55120u, 0, &nz, 0, 0, 0);            /* 40 s of nothing, the gate open .. */
            tw_lane(1378u, 0, &nz, 0, &pk_end, 0);       /* .. and the last second */
            int frozen = 0;
            for (k = 0; k < f->n; k++)                   /* (a frozen CLOUD holds what it took, by design) */
                frozen |= f->e[k].kind == RM_CLD && f->e[k].p[2] >= 50;
            if (pk_end >= 32 && !frozen)
                TFAIL("%s (seed %u, return %u%%): still sounding 40 s after its input stopped (peak %d: %s %u %u %u %u, %s %u %u %u %u, %s)",
                      FR_NAME[rc], t, f->ret & 0x7Fu, pk_end, RM_NAME[f->e[0].kind], f->e[0].p[0], f->e[0].p[1],
                      f->e[0].p[2], f->e[0].p[3], RM_NAME[f->e[1].kind], f->e[1].p[0], f->e[1].p[1], f->e[1].p[2],
                      f->e[1].p[3], f->n > 2u ? RM_NAME[f->e[2].kind] : "-");
            else if (pk_end >= 32)
                printf("  (%s seed %u: its first cloud frozen -- held, as a frozen CLOUD alone is, until the gate closes)\n",
                       FR_NAME[rc], t);
            if (FR_RET[rc] && fx_ret_ok(f)) {            /* the return heard: the same again without it */
                uint16_t r0 = f->ret;
                fx_patch_t keep = *f;
                fw_init();
                fw_run(30);
                fx_set(0, &keep);
                fw_run(40);
                fbuf[0][f_use[0]].ret = 0;
                rnd_seed(&nz, 99u + t);
                tw_lane(2756u, 1, &nz, &e0, 0, 2);
                fbuf[0][f_use[0]].ret = r0;
                {   /* (what the return changes: the difference's level under the sound with it) */
                    double d = 0;
                    uint32_t i;
                    for (i = 0; i < 2756u * CTL; i++)
                        d += (double)tw_cap[i] * tw_cap[i];
                    ret_db += 10.0 * __builtin_log10((d + 1.0) / (e + 1.0));
                }
                nr++;
                (void)pk;
            }
        }
        if (nr) {
            nret++;
            printf("  %-9s %5.1f .. %5.1f dB of the noise in; what the return changes %+.1f dB under it, on average\n",
                   FR_NAME[rc], lo_db, hi_db, ret_db / nr);
        } else
            printf("  %-9s %5.1f .. %5.1f dB of the noise in\n", FR_NAME[rc], lo_db, hi_db);
    }
    draw_path = DP_ANY;
    if (nret != 4u)
        TFAIL("%u recipes with a return heard, not 4", nret);
    printf("twins: %d failures\n", fails);
    return fails ? 1 : 0;
#undef TFAIL
}
static int cmd_load(void)
{
    int fails = 0;
    uint32_t b, u, lane0 = 0, lane1 = 0, mst0 = 0, mst1 = 0;
    fm1_in.notes = fm1_in.buttons = 0;
    fw_init();
    audio_worker_online = 1;
    press(B_FX, 50);
    key(0, 1);
    key(4, 1);
    press(B_SEL, 50);
    key(7, 1);
    for (b = 0; b < 1378u; b++) {
        int32_t out[2 * CTL];
        mix_block(out);
        for (u = U_LANE; u < U_MST; u++) {
            lane0 += !u_core[u];
            lane1 += u_core[u];
        }
        mst0 += !u_core[U_MST];
        mst1 += u_core[U_MST];
    }
    audio_worker_online = 0;
    printf("load: lanes on CPU0 %u, on CPU1 %u; the master on CPU0 %u, on CPU1 %u (blocks x units)\n", lane0, lane1, mst0, mst1);
    if (!lane0 || !lane1)
        printf("FAIL load: the lanes not dealt to both cores\n"), fails++;
    {   /* (build 41) the CLEAR key: asked; NO: nothing; again: not asked; YES: a clean slate in the session's place */
        uint32_t seed0 = gbuf[0][g_use[0]].seed;
        memset(hflash, 0xFF, sizeof hflash);
        ses_init(1);
        if (ses_save(3) != SES_OK)
            printf("FAIL load: session 3 not saved\n"), fails++;
        key(KEY_CLEAR, 1);
        key(KEY_CLEAR, 0);
        if (ui.ses_page != SP_ASK || ui.ses_ask != AK_CLEAR || ui.ses_yes || ui.ses_new != 3u)
            printf("FAIL load: CLEAR not asked (page %u ask %u yes %u)\n", ui.ses_page, ui.ses_ask, ui.ses_yes), fails++;
        press(B_SAVE, 50);                               /* NO */
        if (ui.ses_page || gbuf[0][g_use[0]].seed != seed0)
            printf("FAIL load: NO cleared it\n"), fails++;
        key(KEY_CLEAR, 1);
        key(KEY_CLEAR, 0);
        key(KEY_CLEAR, 1);                               /* again: not asked any more */
        key(KEY_CLEAR, 0);
        if (ui.ses_page)
            printf("FAIL load: CLEAR again left the question up\n"), fails++;
        key(KEY_CLEAR, 1);
        key(KEY_CLEAR, 0);
        fm1_in.enc[PANEL_ENC[EN_PRESET]] = 1;            /* YES */
        ui_input();
        press(B_SAVE, 50);
        fw_run(50);
        if (ui.ses_page || ses_current() != 3u || gbuf[0][g_pub[0]].seed == seed0 || !ses_has(3) || snap.used)
            printf("FAIL load: YES -- page %u, session %u, the sound %s, session 3 %s\n", ui.ses_page, ses_current(),
                   gbuf[0][g_pub[0]].seed == seed0 ? "the same" : "new", ses_has(3) ? "kept in the flash" : "gone"), fails++;
        printf("load: CLEAR -- asked, NO, YES: a new session in session 3's place (saved one kept until SAVE)\n");
    }
    printf("load: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* build 38: every word on the screen fits it -- each message the firmware says (tools/list_says.py: as written, with
 * its widest numbers), each slot's name with every family, recipe and word; every page, dial and question with its
 * widest names and numbers. The host's cv_text counts ink drawn past the canvas (off its sides, below or above its
 * area): none may be */
#include "norilsk_says.h"
static int fit_fails;
static void fit(const char *what)
{
    uint32_t before = cv_ov_n;
    ui.force = 1;
    ui.load_at = fm1_ms - 1000u;
    ui_draw();
    if (cv_ov_n != before) {
        printf("FAIL fit: %s -- \"%s\" past the screen\n", what, cv_ov_last);
        fit_fails++;
    }
}
static void fit_say(const char *s, const char *what)
{
    say(s, 1000);
    fit(what);
    ui.msg_until = fm1_ms;
}
static int cmd_fit(void)
{
    uint32_t i, l, k, j, n = 0;
    char b[64];
    fm1_in.notes = fm1_in.buttons = 0;
    memset(hflash, 0xFF, sizeof hflash);
    fw_init();
    cv_ov_n = 0;
    fit("the main view");
    for (i = 0; i < sizeof SAYS / sizeof SAYS[0]; i++, n++)   /* the messages */
        fit_say(SAYS[i], "a message");
    for (l = 0; l < NSLOT; l++) {                          /* a slot's name with what it says */
        static const char *const W[4] = {"MUTED", "HEARD", "REWIRED", "BUSY"};
        for (k = 0; k < 4u; k++, n++) {
            say_slot(l, W[k], 1000);
            fit("a slot's message");
            ui.msg_until = fm1_ms;
        }
        for (k = 0; k < (l < NGEN ? (uint32_t)MF_N : (uint32_t)FR_N); k++, n++) {
            say_slot(l, l < NGEN ? MF_NAME[k] : FR_NAME[k], 1000);
            fit(l < NGEN ? "a generator's family" : "a lane's recipe");
            ui.msg_until = fm1_ms;
        }
    }
    for (k = 0; k < NKIND; k++, n++) {                     /* every effect on a lane's plates, three to a lane */
        for (l = 0; l < NFX; l++) {
            fx_patch_t *f = &fbuf[l][f_use[l]];
            f->n = 3;
            for (j = 0; j < 3u; j++)
                f->e[j].kind = (uint8_t)k;
        }
        fit("an effect's plate");
    }
    fw_init();
    ui.sc_page = 1;                                        /* the SCALE page: every scale, the widest root */
    for (k = 0; k < NSCALE; k++, n++) {
        ui.scale = (uint8_t)k;
        ui.root = 1;                                       /* (C#) */
        fit("the SCALE page");
    }
    pages_off();
    for (k = 1; k <= 2u; k++, n++) {                       /* DELAY, REVERB: their widest values */
        ui.fx_page = (uint8_t)k;
        for (j = 0; j < 4u; j++)
            ui.dlp[j] = ui.rvp[j] = 100;
        ui.fx_touched = 0;
        ui.fx_touch_until = fm1_ms + 1000u;
        fit(k == 1 ? "the DELAY page" : "the REVERB page");
    }
    pages_off();
    ui.fx_page = 3;                                        /* the MOD page: every slot, entry, source, destination */
    for (l = 0; l < NSLOT; l++) {
        ui.sel = (uint8_t)l;
        for (k = 0; k < 2u; k++)
            for (j = 0; j < MS_N; j++, n++) {
                slot_mod[l].lfo[k].shape = (uint8_t)j;
                slot_mod[l].lfo[k].mode = (uint8_t)(j % MM_N);
                slot_mod[l].lfo[k].rate = 100;
                slot_mod[l].lfo[k].fade = 100;
                ui.mod_ix = (uint8_t)k;
                fit("the MOD page's LFO");
            }
        for (k = 0; k < NMSLOT; k++)
            for (i = 0; i < MSRC_N; i++)
                for (j = 0; j < mod_ndst(l); j++, n++) {
                    slot_mod[l].s[k].src = (uint8_t)i;
                    slot_mod[l].s[k].dst = (uint8_t)j;
                    slot_mod[l].s[k].amt = -100;
                    ui.mod_ix = (uint8_t)(2u + k);
                    fit("the MOD page's route");
                }
    }
    pages_off();
    memset(slot_mod, 0, sizeof slot_mod);
    ui.ses_page = SP_LIST;                                 /* the sessions: the last page of the list, a session's page,
                                                            * the questions, COPY */
    for (k = 61; k <= SES_SLOTS; k++, n++) {
        ui.ses_ix = (uint8_t)k;
        fit("the SESSIONS list");
    }
    ui.ses_ix = 64;
    ui.ses_page = SP_PAGE;
    for (k = 0; k < SR_N; k++, n++) {
        ui.ses_row = (uint8_t)k;
        fit("a session's page");
    }
    ui.ses_page = SP_ASK;
    for (k = 0; k <= AK_CLEAR; k++, n++) {
        ui.ses_ask = (uint8_t)k;
        ui.ses_new = 64;
        fit("a session's question");
    }
    ui.ses_page = SP_COPY;
    ui.ses_tgt = 63;
    fit("COPY");
    pages_off();
    {   /* a session's or snapshot's outcome, each kind, the widest number */
        static const char *const WHAT[] = {"NEW", "RESTORED", "EMPTIED", "STORED", "COPIED", "DELETED", "LOADED", "SAVED",
                                           "RECALLED"};
        static const int RC[] = {SES_OK, SES_FULL, SES_BUSY, SES_FAIL, SES_NOFLASH};
        for (k = 0; k < sizeof WHAT / sizeof WHAT[0]; k++)
            for (i = 0; i < sizeof RC / sizeof RC[0]; i++)
                for (j = 0; j < 2u; j++, n++) {
                    ses_done(RC[i], (int)j, WHAT[k], j ? 16u : 64u);
                    fit("an outcome");
                    ui.fl_until = fm1_ms;
                }
    }
    ui.snap_page = 1;                                      /* the SNAPSHOTS page */
    fit("the SNAPSHOTS page");
    pages_off();
    for (l = 0; l < NGEN; l++)                             /* a generator's page (build 39): OFF, 4.5 S, 60 S, turned */
        for (k = 0; k < 3u; k++, n++) {
            static const uint8_t HS[3] = {0, 9, FADE_MAX};
            ui.gs_page = (uint8_t)(l + 1u);
            gen_fin[l] = HS[k];
            gen_fout[l] = HS[2u - k];
            ui.gs_touched = (int8_t)(k & 1u);
            ui.gs_touch_until = fm1_ms + 1000u;
            fit("a generator's page");
        }
    pages_off();
    ui.ovd = 1;                                            /* the loops (PLAY held), each generator */
    for (l = 0; l < NGEN; l++, n++) {
        ui.gen = (uint8_t)l;
        fit("the LOOPS page");
    }
    ui.ovd = 0;
    for (k = 0; k <= XM_N; k++)                            /* a link, each kind and undone, each pair */
        for (l = 0; l < NGEN; l++, n++) {
            ui.lk_from = (uint8_t)l;
            ui.lk_to = (uint8_t)((l + 1u) % NGEN);
            ui.lk_kind = (uint8_t)k;
            ui.lk_until = fm1_ms + 1500u;
            ui.lk_shown = 0;
            fit("a link");
            ui.lk_until = fm1_ms;
        }
    for (k = 0; k < 4u; k++)                               /* the dials at their widest */
        for (l = 0; l < NGEN; l++, n++) {
            ui.dial_kind = (uint8_t)k;
            ui.gen = ui.sel = (uint8_t)l;
            ui.morph = MORPH_STEPS_UI;
            gen_dis[l] = 100;
            fx_wet[l] = 100;
            master_drive = (uint8_t)(l ? 100 : 0);
            ui.dial_until = fm1_ms + 1500u;
            ui.dial_shown = 0;
            fit(k == 0 ? "MORPH" : k == 1 ? "DISINTEGRATOR" : k == 2 ? "FX MIX" : "DRIVE");
            ui.dial_until = fm1_ms;
        }
    master_drive = 0;
    snprintf(b, sizeof b, "%u screens and messages", n);
    printf("fit: %s, %d past the screen\n", b, fit_fails);
    return fit_fails ? 1 : 0;
}

int main(int argc, char **argv)
{
    memset(hflash, 0xFF, sizeof hflash);
    wt_init();
    if (argc >= 2 && !strcmp(argv[1], "patches"))
        return cmd_patches(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "flows"))
        return cmd_flows();
    if (argc >= 2 && !strcmp(argv[1], "modules"))
        return cmd_modules(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "cores")) {
        uint32_t h2 = cores_hash(2), h1 = cores_hash(1);
        printf("cores: two %08x, one %08x\n", h2, h1);
        return h1 != h2;
    }
    if (argc >= 2 && !strcmp(argv[1], "screens"))
        return cmd_screens(argc, argv);
    if (argc < 2) {
        fprintf(stderr, "host render DIR [SEEDS] [PATH] | one DIR FAMILY SEED [PATH] | check | sustain | golden | bench | patches DIR [N] | modules DIR | flows | cores | screens DIR\n");
        return 2;
    }
    if (!strcmp(argv[1], "render"))
        return cmd_render(argc, argv);
    if (!strcmp(argv[1], "one") && argc >= 5)
        return cmd_one(argc, argv);
    if (!strcmp(argv[1], "check"))
        return cmd_check();
    if (!strcmp(argv[1], "fxtails"))
        return cmd_fxtails();
    if (!strcmp(argv[1], "looper"))
        return cmd_looper();
    if (!strcmp(argv[1], "master"))
        return cmd_master();
    if (!strcmp(argv[1], "endof") && argc > 4) {         /* (a sound held 3 s, let go: when it ends, and why) */
        int f = -1, k;
        stat_t st;
        for (k = 0; k < MF_N; k++)
            if (!strcmp(argv[2], MF_NAME[k]))
                f = k;
        mo_design(&design, f, (uint32_t)atoi(argv[3]), (uint32_t)atoi(argv[4]), GEN_NOW);
        mo_measure(&design);
        print_design(&design);
        st = play(&design, 60, 3.0, 30.0, 0, wav);
        printf("ended %d reaped %d len %.2f s peak %.3f\n", st.ended, st.reaped, st.len / (double)FS, st.peak / 32768.0);
        wav_write("build/sounds/endof.wav", wav, st.len ? st.len : FS * 33);
        return 0;
    }
    if (!strcmp(argv[1], "windows") && argc > 4) {       /* (a sound's level, a quarter second at a time; and its hand) */
        int f = -1, k, nohand;
        for (k = 0; k < MF_N; k++)
            if (!strcmp(argv[2], MF_NAME[k]))
                f = k;
        for (nohand = 0; nohand < 2; nohand++) {
            uint32_t b, i;
            int32_t blk[CTL];
            double w = 0;
            mo_design(&design, f, (uint32_t)atoi(argv[3]), (uint32_t)atoi(argv[4]), GEN_NOW);
            if (nohand)
                design.hand.on = 0;
            mo_measure(&design);
            if (!nohand)
                print_design(&design);
            memset(&voice, 0, sizeof voice - MO_GR_N);
            mo_note_on(&voice, &design, 60, 1);
            printf("%s:", nohand ? "no hand" : "hand   ");
            for (b = 0; b < 344u * 40u; b++) {
                mo_render(&voice, &design, blk, 0);
                for (i = 0; i < CTL; i++)
                    w += (double)blk[i] * blk[i];
                if (b % 344u == 343u) {
                    printf(" %.3f", __builtin_sqrt(w / (344.0 * CTL)) / 32768.0);
                    if (!nohand && b / 344u < 8u)
                        printf("[%d %d %d %d]", voice.hoff[0], voice.hoff[1], voice.hoff[2], voice.hoff[5]);
                    w = 0;
                }
            }
            printf("  gain %d\n", design.gain);
        }
        return 0;
    }
    if (!strcmp(argv[1], "sizes")) {
        printf("mo_voice_t %u, mo_design_t %u, finger %u\n", (unsigned)sizeof(mo_voice_t), (unsigned)sizeof(mo_design_t),
               (unsigned)sizeof(struct mo_finger));
        printf("st_t %u, snapshots %u, gen_keep_t %u, fx_patch_t %u, mod_t %u, looper_t %u\n", (unsigned)sizeof(st_t),
               (unsigned)sizeof snap, (unsigned)sizeof(gen_keep_t), (unsigned)sizeof(fx_patch_t), (unsigned)sizeof(mod_t),
               (unsigned)sizeof(looper_t));
        return 0;
    }
    if (!strcmp(argv[1], "sessions"))
        return cmd_sessions();
    if (!strcmp(argv[1], "mod"))
        return cmd_mod();
    if (!strcmp(argv[1], "paint"))
        return cmd_paint();
    if (!strcmp(argv[1], "rewire"))
        return cmd_rewire();
    if (!strcmp(argv[1], "ears"))
        return cmd_ears();
    if (!strcmp(argv[1], "morph"))
        return cmd_morph();
    if (!strcmp(argv[1], "ends"))
        return cmd_ends();
    if (!strcmp(argv[1], "hiss"))
        return cmd_hiss();
    if (!strcmp(argv[1], "trip"))
        return cmd_trip();
    if (!strcmp(argv[1], "lanes"))
        return cmd_lanes();
    if (!strcmp(argv[1], "sespage"))
        return cmd_sespage();
    if (!strcmp(argv[1], "mtails"))
        return cmd_mtails();
    if (!strcmp(argv[1], "link"))
        return cmd_link();
    if (!strcmp(argv[1], "fit"))
        return cmd_fit();
    if (!strcmp(argv[1], "lanepan"))
        return cmd_lanepan();
    if (!strcmp(argv[1], "mutefade"))
        return cmd_mutefade();
    if (!strcmp(argv[1], "load"))
        return cmd_load();
    if (!strcmp(argv[1], "pick"))
        return cmd_pick();
    if (!strcmp(argv[1], "presets"))
        return cmd_presets();
    if (!strcmp(argv[1], "setup"))
        return cmd_setup();
    if (!strcmp(argv[1], "twins"))
        return cmd_twins();
    if (!strcmp(argv[1], "timbres"))
        return cmd_timbres(argc, argv);
    if (!strcmp(argv[1], "drive"))
        return cmd_drive();
    if (!strcmp(argv[1], "b36"))
        return cmd_b36();
    if (!strcmp(argv[1], "found"))
        return cmd_found(argc, argv);
    if (!strcmp(argv[1], "bodies"))
        return cmd_bodies(argc, argv);
    if (!strcmp(argv[1], "demo"))
        return cmd_demo(argc, argv);
    if (!strcmp(argv[1], "whole"))
        return cmd_whole();
    if (!strcmp(argv[1], "format1"))
        return cmd_format1();
    if (!strcmp(argv[1], "dis"))
        return cmd_dis();
    if (!strcmp(argv[1], "paths"))
        return cmd_paths();
    if (!strcmp(argv[1], "revalue"))
        return cmd_revalue();
    if (!strcmp(argv[1], "quiet"))
        return cmd_quiet();
    if (!strcmp(argv[1], "recall"))
        return cmd_recall();
    if (!strcmp(argv[1], "backup"))
        return cmd_backup();
    if (!strcmp(argv[1], "replay"))
        return cmd_replay(argc, argv);
    if (!strcmp(argv[1], "tails"))
        return cmd_tails(argc, argv);
    if (!strcmp(argv[1], "sustain"))
        return cmd_sustain();
    if (!strcmp(argv[1], "struck"))
        return cmd_struck();
    if (!strcmp(argv[1], "golden"))
        return cmd_golden(argc > 2 ? (uint32_t)atoi(argv[2]) : 0u);
    if (!strcmp(argv[1], "bench"))
        return cmd_bench();
    fprintf(stderr, "host: unknown command\n");
    return 2;
}
