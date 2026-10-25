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
#include "wt.c"
#include "fm6_core.c"
#include "mol.c"
/* usb.c's state, as the screen reads it (no host: on battery) */
static struct { uint8_t config, suspended; } usb;
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
        for (f = 0; f < MF_N; f++)
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

static int cmd_golden(uint32_t gen)
{
    int f, s, path;
    int32_t blk[CTL];
    for (path = 0; path < PATH_N; path++)
        for (f = 0; f < (gen < 3u ? MF_N_OLD : MF_N); f++)   /* (each generation over the families it knows) */
            for (s = 0; s < 8; s++) {
                uint32_t seed = 100u + (uint32_t)f * 31u + (uint32_t)s + (uint32_t)path * 503u, h, b;
                mo_design(&design, f, seed, (uint32_t)path, gen);
                mo_measure(&design);
                /* (the design as generations 0 .. 2 laid it out: generation 3's fields after it, zero before it) */
                h = fnv(2166136261u, &design, gen < 3u ? (uint32_t)__builtin_offsetof(mo_design_t, hand) : (uint32_t)sizeof design);
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
        for (j = 0; j < NINST; j++)
            if ((f->xto[k] >> j) & 1u)
                printf("->%u.%u", j / FX_N + 4u, j % FX_N + 1u);
    }
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
 * and until the output is under -60 dB FS for good (TAIL_LIMIT_S at most, else a failure) */
#define TAIL_LIMIT_S 6.0
static int cmd_tails(int argc, char **argv)
{
    int n = argc > 2 ? atoi(argv[2]) : 24, i, l, fails = 0;
    double worst = 0, wv = 0, sum = 0;
    for (i = 0; i < n; i++) {
        uint32_t b, last_loud = 0, voices_done = 0;
        fw_init();
        ui.seed = 4321u + (uint32_t)i * 6007u;
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
                if (!fx_draw(s, (int32_t)(t % FR_N), 1000u + t * 3u + s)) {
                    printf("FAIL fx_draw busy\n");
                    fails++;
                    continue;
                }
                f = &fbuf[s][f_pub[s]];
                for (k = 0; k < f->n; k++)
                    mem += rm_size(f->e[k].kind);
                worst = mem > worst ? mem : worst;
                if (mem > FX_ARENA || f->n < 2u || f->n > FX_N) {
                    printf("FAIL recipe %s: %u effects, %u B\n", FR_NAME[f->recipe], f->n, mem);
                    fails++;
                }
            }
        printf("recipes: the largest slot %u B of %u\n", worst, FX_ARENA);
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
    {   /* nothing on, once they stop */
        uint32_t last = 0;
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
    ui.seed = 4242u;                                     /* (the same patches every run) */
    gens_init(ui.seed);
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
            if (strncmp(nm, RM_NAME[fbuf[s0][f_pub[s0]].e[1].kind], str_len(RM_NAME[fbuf[s0][f_pub[s0]].e[1].kind])))
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
        fchanged += f0.out != fbuf[0][f_use[0]].out || memcmp(f0.xto, fbuf[0][f_use[0]].xto, sizeof f0.xto);
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
static void ends_fresh(uint32_t seed)
{
    fm1_in.notes = fm1_in.buttons = 0;
    ui_init();
    ui.seed = seed;
    gens_init(ui.seed);
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
        double ra, rb;
        ends_fresh(seed);
        snap_store(0);                                   /* A, then everything anew (a slot muted, a generator quieter) */
        for (l = 0; l < NSLOT; l++) {
            press(SLOT_BTN[l], 50);
            press(B_HOME, 50);
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
        if (!st_same(&sa, &B) || ra - rb > 3.0 || rb - ra > 3.0) {
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
                rm_block(0, &f->e[0], x, l, rr, 1);
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
    printf("dis: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* the paths: HOME on a generator and on an effect slot drawn 300 times each way. ABRASIVE: the harsh families and the
 * abrasive recipes most of the time; DARK: no harsh family, none of the abrasive recipes, the harsh effects only where
 * nothing else could stand; ANY: as before */
static int cmd_paths(void)
{
    static const uint8_t ROUGH_FAM[MF_N] = {1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 0};
    int fails = 0;
    uint32_t p, n;
    for (p = 0; p < DP_N; p++) {
        uint32_t rough = 0, harsh = 0, rrec = 0, drec = 0, hfx = 0;
        fw_init();
        draw_path = (uint8_t)p;
        for (n = 0; n < 300u; n++) {
            const fx_patch_t *f;
            uint32_t k, fam;
            gen_draw(0, -1, 1000u + n * 7u);
            fw_run(2);
            fam = gbuf[0][g_pub[0]].gen.family;
            rough += ROUGH_FAM[fam];
            harsh += MF_HARSH[fam] > 0;
            fx_draw(0, -1, 5000u + n * 13u);
            fw_run(25);
            f = &fbuf[0][f_pub[0]];
            rrec += FR_W_ROUGH[f->recipe] >= 4;
            drec += FR_W_DARK[f->recipe] == 0;
            for (k = 0; k < f->n; k++)
                hfx += (RM_HARSH >> f->e[k].kind) & 1u;
        }
        printf("paths %s: abrasive families %u, harsh %u, abrasive recipes %u, not-dark recipes %u, harsh effects %u (of 300)\n",
               p == DP_ANY ? "ANY" : p == DP_ROUGH ? "ABRASIVE" : "DARK", rough, harsh, rrec, drec, hfx);
        if (p == DP_ROUGH && (rough < 180u || rrec < 200u))
            printf("FAIL paths: ABRASIVE not abrasive enough\n"), fails++;
        if (p == DP_DARK && (harsh || drec))
            printf("FAIL paths: DARK drew something harsh\n"), fails++;
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
        if (same || a.n != b.n || a.out != b.out || memcmp(a.xto, b.xto, sizeof a.xto) || f_pub[1] != f_use[1])
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
        if (ui.oct != 1 || draw_path != DP_ROUGH || gen_dis[2] != 40)
            printf("FAIL whole: octave %d, path %u, DISINTEGRATOR %d\n", ui.oct, draw_path, gen_dis[2]), fails++;
    }
    ui.morph = 0;
    draw_path = DP_ANY;
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
                for (q = 0; q < NFX; q++)
                    for (e = 0; e < FX_N; e++)
                        if ((fbuf[q][f_use[q]].xto[e] >> 7) & 1u)
                            printf(" [%u.%u -> 6.2, meter %d quiet %u]", q + 4u, e + 1u, rm_meter[q * FX_N + e], rm_quiet[q * FX_N + e]);
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
    press(B_FX, 50);
    key(KEY_SHIFT, 1);
    press(B_SAVE, 50);                                   /* SHIFT + SAVE: the SESSIONS page */
    key(KEY_SHIFT, 0);
    SHOT("sessions");
    fm1_in.pressed |= 1u << PANEL_BTN[B_SAVE];           /* SAVE held: asked (none loaded) */
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
    ui_draw();
    snprintf(name, sizeof name, "%s/screen-morph-dial.ppm", dir);
    screen_save(name);
    press(B_SAVE, 50);                                   /* back to playing: KNOB 1 the DISINTEGRATOR, on its dial */
    ui.dial_until = 0;
    fm1_in.enc[PANEL_ENC[EN_K1]] = 23;
    ui_input();
    ui.load_at = fm1_ms - 1000u;
    ui.msg_until = 0;
    ui_draw();
    snprintf(name, sizeof name, "%s/screen-dis-dial.ppm", dir);
    screen_save(name);
    gen_dis[ui.gen] = 0;
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
    if (!strcmp(argv[1], "golden"))
        return cmd_golden(argc > 2 ? (uint32_t)atoi(argv[2]) : 0u);
    if (!strcmp(argv[1], "bench"))
        return cmd_bench();
    fprintf(stderr, "host: unknown command\n");
    return 2;
}
