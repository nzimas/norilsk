/* SPDX-License-Identifier: GPL-3.0-only
 * NORILSK: the single cycles the operators may read instead of the sine, band-limited by octave
 * (tools/gen_tables.py, WT_*; after Bigorna's osc.c). Each wave at WT_LV levels, level k keeping 128 >> k
 * harmonics in WT_LEN(k) points (+ 1, the first again: no wrap in the interpolation). They are copied from the
 * flash into RAM at start-up (the flash is slow to read sample by sample) and read with linear interpolation;
 * an operator chooses a level from its phase increment, so that no harmonic of the wave itself passes ~20 kHz
 * (what FM then does to it is its own business, as on any FM synthesizer with waveforms). */

static int16_t wt_ram[WT_TOTAL] POOL;

static void wt_init(void)
{
    uint32_t i;
    for (i = 0; i < WT_TOTAL; i++)
        wt_ram[i] = WT_DATA[i];
}

/* wave w's level for a phase increment in Q24 cycles a sample (the operators' unit): level k when the increment
 * reaches 2^(16 + k) -- 2^16 is ~172 Hz, where 128 harmonics reach 22 kHz */
AINL uint32_t wt_level24(uint32_t inc24)
{
    uint32_t k = 0;
    while (k + 1u < WT_LV && inc24 >= (1u << (16u + k)))
        k++;
    return k;
}
AINL const int16_t *wt_tab(uint32_t w, uint32_t lv) { return wt_ram + w * WT_WAVE + WT_OFF[lv]; }

/* a read at phase ph (2^32 = a cycle) of a level's table (sh: 32 - log2 of its length) */
AINL int32_t wt_rd(const int16_t *t, uint32_t sh, uint32_t ph)
{
    uint32_t i = ph >> sh;
    int32_t f = (int32_t)((ph << (32u - sh)) >> 17);
    return t[i] + (((t[i + 1u] - t[i]) * f) >> 15);
}
