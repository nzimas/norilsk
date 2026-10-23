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

#include "dsp.h"
#include "wt.c"
#include "fm6_core.c"
#include "mol.c"

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

int main(int argc, char **argv)
{
    wt_init();
    if (argc < 2) {
        fprintf(stderr, "host render DIR [SEEDS] [PATH] | one DIR FAMILY SEED [PATH] | check | golden | bench\n");
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
