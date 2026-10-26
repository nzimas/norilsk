/* SPDX-License-Identifier: GPL-3.0-only
 * NORILSK: the found sounds made clips for the engine (generation 5's grains read them). From each WAV in found/ --
 * the recordings (found/raw, NOAA's and NASA's: found/CREDITS.md) and our own renders (found/synth, render_found.c)
 * -- the steadiest 0.6 s (no quiet stretch, no sudden change), resampled to 22.05 kHz (the grain buffers' rate), the
 * DC taken off, its end crossed into its start (it loops without a seam), brought to one level, coded 8-bit as the
 * grain buffers are (mol.c gr_enc). Out: firmware/src/found_clips.h, and each clip decoded to build/found/ to listen.
 *   cc -O2 -Ibuild/gen -o build/make_found tools/make_found.c -lm && build/make_found
 * Released clips never change: a sound drawn from one must draw and sound again the same (tests/golden). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "norilsk_tables.h"

#define OUT_FS 22050
#define CLIP_N 13230u            /* 0.6 s */
#define XF 1102u                 /* the loop's crossing, 50 ms */
#define FRAME 441u               /* 20 ms */
#define TARGET_RMS 5190.0        /* -16 dBFS */

/* the clips, in their order for ever (the engine names them by index) */
static const struct { const char *file, *name; } CLIP[] = {
    {"found/raw/noaa-iceberg-calving.wav", "ICEBERG CALVING"},
    {"found/raw/noaa-iceberg-tremor.wav", "ICEBERG TREMOR"},
    {"found/raw/noaa-bloop.wav", "ICEQUAKE"},
    {"found/raw/noaa-iceberg-grounding.wav", "ICEBERG GROUNDING"},
    {"found/raw/noaa-challenger-deep-ship.wav", "SHIP ABOVE THE DEEP"},
    {"found/raw/nasa-polar-auroral-hiss-1.wav", "AURORAL HISS"},
    {"found/raw/nasa-polar-auroral-hiss-2.wav", "AURORAL HISS 2"},
    {"found/raw/nasa-polar-akr.wav", "AURORAL RADIO"},
    {"found/raw/nasa-polar-chorus.wav", "CHORUS"},
    {"found/raw/nasa-polar-saucer.wav", "SAUCER"},
    {"found/synth/synth-transformer.wav", "TRANSFORMER"},
    {"found/synth/synth-duct.wav", "DUCT"},
    {"found/synth/synth-furnace.wav", "FURNACE"},
    {"found/synth/synth-rain-tin-roof.wav", "RAIN ON TIN"},
    {"found/synth/synth-engine-room.wav", "ENGINE ROOM"},
    {"found/synth/synth-wind-in-wires.wav", "WIND IN WIRES"},
};
#define NCLIP (sizeof CLIP / sizeof CLIP[0])

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* a WAV's samples (8-bit unsigned or 16-bit, mono or the first channel), -1 .. 1 */
static double *wav_read(const char *path, uint32_t *n, uint32_t *fs)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b, *p, *end;
    long len;
    double *x = 0;
    uint32_t ch = 1, bits = 16, i;
    if (!f) {
        perror(path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc((size_t)len);
    if (fread(b, 1, (size_t)len, f) != (size_t)len)
        exit(1);
    fclose(f);
    if (memcmp(b, "RIFF", 4) || memcmp(b + 8, "WAVE", 4)) {
        fprintf(stderr, "%s: not a WAV\n", path);
        exit(1);
    }
    for (p = b + 12, end = b + len; p + 8 <= end; p += 8 + ((rd32(p + 4) + 1u) & ~1u)) {
        uint32_t sz = rd32(p + 4);
        if (!memcmp(p, "fmt ", 4)) {
            ch = rd16(p + 10);
            *fs = rd32(p + 12);
            bits = rd16(p + 22);
        } else if (!memcmp(p, "data", 4)) {
            uint32_t bps = bits / 8u * ch;
            if (sz > (uint32_t)(end - p - 8))
                sz = (uint32_t)(end - p - 8);
            *n = sz / bps;
            x = malloc(*n * sizeof *x);
            for (i = 0; i < *n; i++) {
                const uint8_t *s = p + 8 + i * bps;
                x[i] = bits == 8 ? (s[0] - 128) / 128.0 : (int16_t)rd16(s) / 32768.0;
            }
            break;
        }
    }
    free(b);
    if (!x) {
        fprintf(stderr, "%s: no data\n", path);
        exit(1);
    }
    return x;
}
/* resampled to OUT_FS: a windowed sinc (Blackman, 32 taps a side), low-passed below the lower Nyquist */
static double *resample(const double *x, uint32_t n, uint32_t fs, uint32_t *m)
{
    double ratio = (double)OUT_FS / fs, cut = ratio < 1 ? ratio : 1, *y;
    uint32_t i;
    *m = (uint32_t)(n * ratio);
    y = malloc(*m * sizeof *y);
    for (i = 0; i < *m; i++) {
        double t = i / ratio, acc = 0;
        long c = (long)floor(t), k;
        for (k = c - 31; k <= c + 32; k++) {
            double d = t - k, w, s;
            if (k < 0 || k >= (long)n)
                continue;
            if (fabs(d) >= 32)
                continue;
            w = 0.42 + 0.5 * cos(M_PI * d / 32) + 0.08 * cos(2 * M_PI * d / 32);
            s = d == 0 ? 1 : sin(M_PI * d * cut) / (M_PI * d * cut);
            acc += x[k] * s * w * cut;
        }
        y[i] = acc;
    }
    return y;
}

int main(void)
{
    FILE *h = fopen("firmware/src/found_clips.h", "w");
    uint32_t c, i;
    if (!h) {
        perror("firmware/src/found_clips.h");
        return 1;
    }
    fprintf(h, "/* SPDX-License-Identifier: GPL-3.0-only\n * NORILSK: the found sounds' clips -- written by tools/make_found.c from found/ (the recordings NOAA's and\n"
               " * NASA's: found/CREDITS.md; the rest rendered by tools/render_found.c). Never edit; never change once released: a\n"
               " * sound drawn from a clip draws and sounds again the same (tests/golden). %u clips of %u samples at 22.05 kHz, coded\n"
               " * 8-bit as the grain buffers (mol.c gr_dec), each looping without a seam. In the flash: read by the grains */\n",
            (unsigned)NCLIP, CLIP_N);
    fprintf(h, "#define FC_N %uu\n#define FC_LEN %uu\n", (unsigned)NCLIP, CLIP_N);
    fprintf(h, "static const char *const FC_NAME[FC_N] = {");
    for (c = 0; c < NCLIP; c++)
        fprintf(h, "%s\"%s\"", c ? ", " : "", CLIP[c].name);
    fprintf(h, "};\nstatic const uint8_t FC_DATA[FC_N][FC_LEN] = {\n");
    for (c = 0; c < NCLIP; c++) {
        uint32_t n, fs = 44100, m, best = 0, nf, s, k;
        double *x = wav_read(CLIP[c].file, &n, &fs), *y = resample(x, n, fs, &m), *fr, maxm = 0, bests = 1e30, dc = 0,
               dcy = 0, rms = 0, g, clip[CLIP_N];
        uint8_t code[CLIP_N];
        char path[256];
        FILE *w;
        for (i = 0; i < m; i++) {                /* (the DC off: a high-pass at ~20 Hz) */
            double v = y[i];
            dcy = v - dc + 0.9943 * dcy;
            dc = v;
            y[i] = dcy;
        }
        nf = m / FRAME;
        fr = calloc(nf, sizeof *fr);
        for (k = 0; k < nf; k++) {
            double a = 0;
            for (i = 0; i < FRAME; i++)
                a += y[k * FRAME + i] * y[k * FRAME + i];
            fr[k] = sqrt(a / FRAME);
        }
        for (s = 25; s + (CLIP_N + XF) / FRAME + 2 < nf; s++) {   /* (0.5 s in: past a fade) */
            uint32_t nw = (CLIP_N + XF) / FRAME + 1;
            double mean = 0, var = 0, jump = 0;
            for (k = 0; k < nw; k++)
                mean += fr[s + k] / nw;
            if (mean > maxm)
                maxm = mean;
        }
        for (s = 25; s + (CLIP_N + XF) / FRAME + 2 < nf; s++) {   /* the steadiest window, loud enough */
            uint32_t nw = (CLIP_N + XF) / FRAME + 1;
            double mean = 0, var = 0, jump = 0, score;
            for (k = 0; k < nw; k++)
                mean += fr[s + k] / nw;
            if (mean < 0.4 * maxm)
                continue;
            for (k = 0; k < nw; k++) {
                var += (fr[s + k] - mean) * (fr[s + k] - mean) / nw;
                if (k)
                    jump = fmax(jump, fabs(fr[s + k] - fr[s + k - 1]));
            }
            score = sqrt(var) / mean + 2 * jump / mean;
            if (score < bests) {
                bests = score;
                best = s;
            }
        }
        for (i = 0; i < CLIP_N; i++)             /* its end crossed into its start: a seamless loop */
            clip[i] = y[best * FRAME + i];
        for (i = 0; i < XF; i++) {
            double u = (double)i / XF;
            clip[i] = clip[i] * sin(u * M_PI / 2) + y[best * FRAME + CLIP_N + i] * cos(u * M_PI / 2);
        }
        for (i = 0; i < CLIP_N; i++)
            rms += clip[i] * clip[i] / CLIP_N;
        g = TARGET_RMS / (sqrt(rms) * 32768 + 1e-9);
        snprintf(path, sizeof path, "build/found/%02u-%s.wav", c, CLIP[c].name);
        for (i = 0; path[i]; i++)
            if (path[i] == ' ')
                path[i] = '-';
        w = fopen(path, "wb");
        fprintf(h, "    {");
        for (i = 0; i < CLIP_N; i++) {
            double v = clip[i] * 32768 * g;
            int32_t q, a;
            v = 32000 * tanh(v / 32000);         /* (a soft ceiling) */
            q = (int32_t)lrint(v);
            a = q < 0 ? -q : q;
            if (a > 32767)
                a = 32767;
            code[i] = (uint8_t)((q < 0 ? 0x80u : 0u) | MULAW_ENC[a >> 4]);
            fprintf(h, "%u%s", code[i], i + 1 < CLIP_N ? (i % 32u == 31u ? ",\n     " : ",") : "");
        }
        fprintf(h, "}%s   /* %s: %s, from %.2f s, steadiness %.3f */\n", c + 1 < NCLIP ? "," : "", CLIP[c].name,
                CLIP[c].file, best * (double)FRAME / OUT_FS, bests);
        if (w) {                                 /* (decoded, to listen to) */
            uint32_t v32;
            uint16_t v16;
            fwrite("RIFF", 1, 4, w);
            v32 = 36 + CLIP_N * 2;
            fwrite(&v32, 4, 1, w);
            fwrite("WAVEfmt ", 1, 8, w);
            v32 = 16;
            fwrite(&v32, 4, 1, w);
            v16 = 1;
            fwrite(&v16, 2, 1, w);
            fwrite(&v16, 2, 1, w);
            v32 = OUT_FS;
            fwrite(&v32, 4, 1, w);
            v32 = OUT_FS * 2;
            fwrite(&v32, 4, 1, w);
            v16 = 2;
            fwrite(&v16, 2, 1, w);
            v16 = 16;
            fwrite(&v16, 2, 1, w);
            fwrite("data", 1, 4, w);
            v32 = CLIP_N * 2;
            fwrite(&v32, 4, 1, w);
            for (i = 0; i < CLIP_N; i++) {
                int32_t mm = MULAW_DEC[code[i] & 0x7Fu];
                int16_t sv = (int16_t)(code[i] & 0x80u ? -mm : mm);
                fwrite(&sv, 2, 1, w);
            }
            fclose(w);
        }
        printf("%-20s from %5.2f s of %5.2f s (%u Hz), steadiness %.3f -> %s\n", CLIP[c].name,
               best * (double)FRAME / OUT_FS, (double)n / fs, fs, bests, path);
        free(x);
        free(y);
        free(fr);
    }
    fprintf(h, "};\n");
    fclose(h);
    return 0;
}
