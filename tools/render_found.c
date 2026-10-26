/* SPDX-License-Identifier: GPL-3.0-only
 * NORILSK: the industrial found sounds, rendered offline (on a computer: floating point, as heavy as it likes) --
 * the kinds no free archive gave: a transformer's hum, an air duct, a furnace, rain on a tin roof, an engine room,
 * wind in wires. Each 6 s, 44.1 kHz, 16-bit mono WAV into found/synth/, the same every time (a fixed seed). Ours,
 * under the project's licence. Build and run (from the repository):
 *   cc -O2 -o build/render_found tools/render_found.c -lm && build/render_found found/synth
 * tools/make_found.c then takes a steady second of each, as of the recordings in found/raw. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FS 44100
#define LEN (FS * 6)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static uint32_t rs = 0x4E4F524Cu;
static double rnd(void)                          /* -1 .. 1 */
{
    rs = rs * 1664525u + 1013904223u;
    return (double)(rs >> 8) / 8388608.0 - 1.0;
}
static double urnd(void) { return (rnd() + 1.0) * 0.5; }

/* a two-pole band-pass (RBJ, constant 0 dB peak) */
typedef struct { double b0, b2, a1, a2, x1, x2, y1, y2; } bp_t;
static void bp_set(bp_t *f, double hz, double q)
{
    double w = 2 * M_PI * hz / FS, al = sin(w) / (2 * q), a0 = 1 + al;
    memset(f, 0, sizeof *f);
    f->b0 = al / a0;
    f->b2 = -al / a0;
    f->a1 = -2 * cos(w) / a0;
    f->a2 = (1 - al) / a0;
}
static double bp_run(bp_t *f, double x)
{
    double y = f->b0 * x + f->b2 * f->x2 - f->a1 * f->y1 - f->a2 * f->y2;
    f->x2 = f->x1;
    f->x1 = x;
    f->y2 = f->y1;
    f->y1 = y;
    return y;
}
/* a slow random wander, 0 .. 1, moving about every `sec` seconds */
typedef struct { double a, b, t, step; } wander_t;
static void wander_set(wander_t *w, double sec) { w->a = urnd(); w->b = urnd(); w->t = 0; w->step = 1.0 / (sec * FS); }
static double wander(wander_t *w)
{
    double u;
    w->t += w->step;
    if (w->t >= 1) {
        w->t -= 1;
        w->a = w->b;
        w->b = urnd();
    }
    u = w->t * w->t * (3 - 2 * w->t);
    return w->a + (w->b - w->a) * u;
}
/* pink-ish noise (Paul Kellet's economy filter) */
typedef struct { double b0, b1, b2; } pink_t;
static double pink(pink_t *p)
{
    double w = rnd();
    p->b0 = 0.99765 * p->b0 + w * 0.0990460;
    p->b1 = 0.96300 * p->b1 + w * 0.2965164;
    p->b2 = 0.57000 * p->b2 + w * 1.0526913;
    return (p->b0 + p->b1 + p->b2 + w * 0.1848) * 0.2;
}

static double buf[LEN];

/* a transformer: magnetostriction at twice the mains (100 Hz) and its harmonics, the core buzzing (a rectified,
 * clipped 50 Hz), the casing's resonances, a little hiss; the harmonics' levels breathing slowly */
static void hum(void)
{
    static const double H[8] = {1.0, 0.55, 0.42, 0.25, 0.30, 0.12, 0.10, 0.06};
    bp_t cas[3];
    wander_t wv[8];
    int i, k;
    bp_set(&cas[0], 480, 6);
    bp_set(&cas[1], 1250, 9);
    bp_set(&cas[2], 2900, 12);
    for (k = 0; k < 8; k++)
        wander_set(&wv[k], 0.6 + 0.3 * k);
    for (i = 0; i < LEN; i++) {
        double t = (double)i / FS, x = 0, bz, c;
        for (k = 0; k < 8; k++)
            x += H[k] * (0.6 + 0.4 * wander(&wv[k])) * sin(2 * M_PI * 100 * (k + 1) * t + k * 0.7);
        bz = fabs(sin(2 * M_PI * 50 * t));
        bz = tanh(6 * (bz - 0.6)) * 0.25;            /* (the core's buzz) */
        c = bp_run(&cas[0], bz) * 3 + bp_run(&cas[1], bz) * 2 + bp_run(&cas[2], rnd() * 0.05) * 4;
        buf[i] = x * 0.25 + bz * 0.3 + c + rnd() * 0.01;
    }
}
/* an air duct: pink noise through the duct's modes (a 3 m run: ~57 Hz apart), the fan's blade pass (7 blades at
 * 23.4 turns a second, wavering a little) and its harmonics, a low rumble swelling */
static void duct(void)
{
    bp_t md[10];
    pink_t p = {0};
    wander_t rpm, sw;
    double ph = 0;
    int i, k;
    for (k = 0; k < 10; k++)
        bp_set(&md[k], 57.0 * (k + 1) * (1 + 0.013 * k), 14);
    wander_set(&rpm, 1.7);
    wander_set(&sw, 1.1);
    for (i = 0; i < LEN; i++) {
        double n = pink(&p), x = 0, bpf;
        for (k = 0; k < 10; k++)
            x += bp_run(&md[k], n) * (1.0 / (1 + k * 0.25));
        bpf = 7 * 23.4 * (1 + 0.004 * (wander(&rpm) - 0.5));   /* (a faint waver: no glide) */
        ph += 2 * M_PI * bpf / FS;
        x += 0.18 * sin(ph) + 0.08 * sin(2 * ph + 0.3) + 0.04 * sin(3 * ph + 1.1);
        buf[i] = x * 1.5 + n * (0.25 + 0.25 * wander(&sw));
    }
}
/* a furnace: a low turbulent roar (brown noise, its level and colour wandering), the burner's hiss, a dense crackle
 * of short filtered snaps */
static void furnace(void)
{
    bp_t roar, crk[3];
    wander_t lv, col;
    double brown = 0, lp = 0, env = 0;
    int i;
    bp_set(&roar, 90, 0.7);
    bp_set(&crk[0], 2200, 3);
    bp_set(&crk[1], 4100, 4);
    bp_set(&crk[2], 900, 2);
    wander_set(&lv, 0.35);
    wander_set(&col, 0.8);
    for (i = 0; i < LEN; i++) {
        double x, c, a = 0.002 + 0.02 * wander(&col);
        brown = brown * 0.995 + rnd() * 0.1;
        lp += a * (brown - lp);
        x = bp_run(&roar, brown) * (0.5 + 0.8 * wander(&lv)) + lp * 2;
        if (urnd() < 0.004)                      /* (~180 snaps a second, each its own size) */
            env = urnd() * urnd();
        env *= 0.992;
        c = env * rnd();
        x += bp_run(&crk[0], c) * 2 + bp_run(&crk[1], c) * 1.5 + bp_run(&crk[2], c);
        buf[i] = x + rnd() * 0.03;
    }
}
/* rain on a tin roof: a dense patter (thousands of drops a second) ringing the sheet's modes, a drum boom under it */
static void rain(void)
{
    static const double MODE[8] = {412, 655, 937, 1310, 1866, 2470, 3215, 4380};
    bp_t md[8], boom;
    wander_t dens;
    int i, k;
    for (k = 0; k < 8; k++)
        bp_set(&md[k], MODE[k], 18 + k * 4);
    bp_set(&boom, 95, 4);
    wander_set(&dens, 0.9);
    for (i = 0; i < LEN; i++) {
        double d = urnd() < 0.05 + 0.04 * wander(&dens) ? rnd() * urnd() : 0, x = 0;
        for (k = 0; k < 8; k++)
            x += bp_run(&md[k], d) * (1.0 / (1 + 0.15 * k));
        buf[i] = x * 2.5 + bp_run(&boom, d) * 1.5 + d * 0.15;
    }
}
/* an engine room: a diesel idling (firing at ~31 Hz, each stroke a little different) through the hull's resonances,
 * a gear's whine and its sidebands, the bearings' hiss */
static void engine(void)
{
    bp_t hull[4];
    wander_t jit;
    double ph = 0, wph = 0, stroke = 0;
    int i;
    bp_set(&hull[0], 63, 3);
    bp_set(&hull[1], 141, 5);
    bp_set(&hull[2], 380, 6);
    bp_set(&hull[3], 1150, 8);
    wander_set(&jit, 0.5);
    for (i = 0; i < LEN; i++) {
        double x, f = 31 * (1 + 0.01 * (wander(&jit) - 0.5)), pulse;
        ph += f / FS;
        if (ph >= 1) {
            ph -= 1;
            stroke = 0.7 + 0.3 * urnd();
        }
        pulse = stroke * exp(-ph * 9) * (0.8 + 0.2 * rnd());
        x = bp_run(&hull[0], pulse) * 1.6 + bp_run(&hull[1], pulse) + bp_run(&hull[2], pulse) * 0.6 +
            bp_run(&hull[3], pulse + rnd() * 0.05) * 0.5;
        wph += 2 * M_PI * 812 / FS;
        x += 0.03 * sin(wph) * (1 + 0.4 * sin(2 * M_PI * f * i / FS));   /* (the gear: sidebands at the firing) */
        buf[i] = x + rnd() * 0.02;
    }
}
/* wind in wires: aeolian tones (narrow bands on a few wires, each its own pitch, swelling and fading with the
 * wind's speed), the wind's own low rush */
static void wires(void)
{
    static const double W[5] = {318, 447, 529, 691, 873};
    bp_t aeo[5], rush;
    wander_t gust[5], wind;
    pink_t p = {0};
    int i, k;
    for (k = 0; k < 5; k++) {
        bp_set(&aeo[k], W[k], 60);
        wander_set(&gust[k], 0.7 + 0.25 * k);
    }
    bp_set(&rush, 300, 0.5);
    wander_set(&wind, 1.3);
    for (i = 0; i < LEN; i++) {
        double n = rnd(), x = 0, wv = wander(&wind);
        for (k = 0; k < 5; k++) {
            double g = wander(&gust[k]);
            x += bp_run(&aeo[k], n) * g * g * 9;
        }
        buf[i] = x + bp_run(&rush, pink(&p)) * (0.3 + 0.5 * wv);
    }
}

static void wav_write(const char *dir, const char *name)
{
    char path[512];
    FILE *f;
    double pk = 0, g;
    int i;
    uint32_t v;
    uint16_t s16;
    for (i = 0; i < LEN; i++)
        pk = fmax(pk, fabs(buf[i]));
    g = pk > 0 ? 0.7 / pk : 1;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fwrite("RIFF", 1, 4, f);
    v = 36 + LEN * 2;
    fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    v = 16;
    fwrite(&v, 4, 1, f);
    s16 = 1;
    fwrite(&s16, 2, 1, f);
    fwrite(&s16, 2, 1, f);
    v = FS;
    fwrite(&v, 4, 1, f);
    v = FS * 2;
    fwrite(&v, 4, 1, f);
    s16 = 2;
    fwrite(&s16, 2, 1, f);
    s16 = 16;
    fwrite(&s16, 2, 1, f);
    fwrite("data", 1, 4, f);
    v = LEN * 2;
    fwrite(&v, 4, 1, f);
    for (i = 0; i < LEN; i++) {
        int16_t s = (int16_t)lrint(buf[i] * g * 32767);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
    printf("%s\n", path);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "found/synth";
    hum();
    wav_write(dir, "synth-transformer");
    duct();
    wav_write(dir, "synth-duct");
    furnace();
    wav_write(dir, "synth-furnace");
    rain();
    wav_write(dir, "synth-rain-tin-roof");
    engine();
    wav_write(dir, "synth-engine-room");
    wires();
    wav_write(dir, "synth-wind-in-wires");
    return 0;
}
