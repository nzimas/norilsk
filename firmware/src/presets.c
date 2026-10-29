/* SPDX-License-Identifier: GPL-3.0-only
 * NORILSK: PRESETS (build 47, Tonitor's): 64 slots, each a generator's patch -- its sound (what draws it again: family,
 * seed, path, generation, its measured level), its lane (the effects, their values, their outs, the return), the send
 * and dry share of its cable into the lane, its DISINTEGRATOR and its lane's FX MIX. Not kept: its level, pan, delay
 * and reverb sends, modulation and mute -- the mix's. One record of the sessions' store, number PRE_NUM (not a
 * session, never in a snapshot), written as GLOBAL SETUP's is. A preset is named by its sound's family and numbered
 * among the presets of that family in the slots' order: ANVIL 1, ANVIL 2, DRONE 1.
 *
 * SHIFT + the scale button held 0.8 s opens the page, on the generator the keys play; PRESETS chooses a slot; the
 * scale button (or SAVE) loads it into the generator and goes back to playing; held 0.8 s, saves the generator there
 * (asked first over a full one). SHIFT + the scale button again, or a slot's button: back. */

#define PRE_FORMAT 1u
typedef struct {
    uint32_t dseed, pseed;       /* the sound's seed; the patch's (HOME's) */
    int32_t gain;                /* the sound's measured level */
    int16_t send, dry;           /* its cable into the lane */
    uint8_t family, path, gen, scene;
    uint8_t used, dis, wet, nent;   /* used: 1; DISINTEGRATOR, FX MIX, 0 .. 100; the places wired */
    uint8_t ent[4];              /* .. their places in the lane (0 .. FX_N - 1: the lane is the generator's own) */
    fx_eff_t e[FX_N];            /* the lane */
    uint8_t n, recipe, out, rsv0;
    uint8_t rsv1;
    uint16_t ret, rsv2;
    uint32_t fseed;
} pre_rec_t;
typedef char pre_rec_56[sizeof(pre_rec_t) == 56u && FX_N == 3 && NENT <= 4 ? 1 : -1];
#define PRE_LEN (32u + PRE_SLOTS * sizeof(pre_rec_t))
typedef char pre_fits_a_sector[PRE_LEN <= SES_SECT ? 1 : -1];
static uint8_t pre_fam[PRE_SLOTS];               /* each slot's family (the list's names), 0xFF empty */

/* the record's slot k (0 ..), from the flash: 1 if it holds a patch */
static int pre_read(uint32_t k, pre_rec_t *r)
{
    ses_head_t h;
    int at;
    memset(r, 0, sizeof *r);
    if (!ses.ok || k >= PRE_SLOTS || (at = ses_find(PRE_NUM)) < 0)
        return 0;
    ses_flash_read(SES_LO + ses.sect[at] * SES_SECT, &h, sizeof h);
    if (h.format != PRE_FORMAT || h.len != PRE_LEN)
        return 0;
    ses_flash_read(SES_LO + ses.sect[at] * SES_SECT + 32u + k * sizeof *r, r, sizeof *r);
    return r->used == 1u && r->family < MF_N;
}
/* the list's names, from the record (the start, a save, a restore) */
static void pre_scan(void)
{
    pre_rec_t r;
    uint32_t k;
    for (k = 0; k < PRE_SLOTS; k++)
        pre_fam[k] = (uint8_t)(pre_read(k, &r) ? r.family : 0xFFu);
}
static int pre_has(uint32_t slot) { return slot >= 1u && slot <= PRE_SLOTS && pre_fam[slot - 1u] != 0xFFu; }
/* slot's name: its family and its number among the presets of that family, in the slots' order (ANVIL 2); EMPTY */
static void pre_name(uint32_t slot, char *b, uint32_t nb)
{
    uint32_t s, n = 1;
    if (!pre_has(slot)) {
        str_cpy(b, "EMPTY", nb);
        return;
    }
    for (s = 1; s < slot; s++)
        n += pre_fam[s - 1u] == pre_fam[slot - 1u];
    str_cpy(b, MF_NAME[pre_fam[slot - 1u]], nb);
    str_cpy(b + str_len(b), " ", nb - str_len(b));
    fmt_int(b + str_len(b), (int32_t)n);
}

/* generator l's patch into slot (1 .. 64): the record read whole into the sector buffer, the slot changed, written
 * anew (blank sectors, read back, the old copy retired) */
static int pre_save(uint32_t l, uint32_t slot)
{
    ses_head_t h, old;
    pre_rec_t *t = (pre_rec_t *)(void *)(ses_chunk + 32u), *r;
    gen_keep_t k;
    const fx_patch_t *f;
    uint32_t ns, i;
    int s, at;
    if (!ses.ok)
        return SES_NOFLASH;
    if (ses_locked())
        return SES_BUSY;
    if (l >= NGEN || slot < 1u || slot > PRE_SLOTS)
        return SES_FAIL;
    memset(ses_chunk, 0, PRE_LEN);
    if ((at = ses_find(PRE_NUM)) >= 0) {
        ses_flash_read(SES_LO + ses.sect[at] * SES_SECT, &old, sizeof old);
        if (old.format == PRE_FORMAT && old.len == PRE_LEN)
            ses_flash_read(SES_LO + ses.sect[at] * SES_SECT + 32u, t, PRE_SLOTS * sizeof *t);
    }
    r = &t[slot - 1u];
    memset(r, 0, sizeof *r);
    gen_keep(l, &k);
    r->dseed = k.dseed;
    r->pseed = k.pseed;
    r->gain = k.gain;
    r->send = (int16_t)clampi(k.send, 0, Q15);
    r->dry = (int16_t)clampi(k.dry, 0, Q15);
    r->family = k.family;
    r->path = k.path;
    r->gen = k.gen;
    r->scene = k.scene;
    r->used = 1;
    r->dis = (uint8_t)clampi(gen_dis[l], 0, 100);
    r->wet = (uint8_t)clampi(fx_wet[l], 0, 100);
    r->nent = (uint8_t)(k.nent <= NENT ? k.nent : 0);
    for (i = 0; i < r->nent; i++)
        r->ent[i] = (uint8_t)(k.ent[i] % FX_N);
    f = &fbuf[l][f_pub[l]];
    memcpy(r->e, f->e, sizeof r->e);
    r->n = f->n;
    r->recipe = f->recipe;
    r->out = f->out;
    r->ret = f->ret;
    r->fseed = f->seed;
    s = ses_place(PRE_LEN, &ns);
    if (s < 0)
        return -s;
    ses_head_fill(&h, PRE_NUM, PRE_LEN, ses_crc(ses_chunk + 32u, PRE_LEN - 32u, 0));
    h.format = PRE_FORMAT;
    h.head_crc = ses_crc((const uint8_t *)&h, 24u, 0);
    memcpy(ses_chunk, &h, sizeof h);
    if (ses_flash_prog(SES_LO + (uint32_t)s * SES_SECT, ses_chunk, PRE_LEN))
        return SES_FAIL;
    i = (uint32_t)ses_commit(PRE_NUM, s, ns, &h);
    pre_scan();
    return (int)i;
}
/* slot (1 .. 64) into generator l: its sound (crossfaded, as a new one), its lane, the cable into it, DISINTEGRATOR,
 * FX MIX. Its modulation by another generator stays, as HOME leaves it */
static int pre_load(uint32_t l, uint32_t slot)
{
    pre_rec_t r;
    gen_keep_t k;
    fx_patch_t f;
    uint32_t i;
    int ok;
    if (l >= NGEN || !pre_read(slot - 1u, &r))
        return SES_FAIL;
    gen_keep(l, &k);
    k.dseed = r.dseed;
    k.pseed = r.pseed;
    k.gain = r.gain;
    k.send = r.send;
    k.dry = r.dry;
    k.family = r.family;
    k.path = r.path;
    k.gen = r.gen;
    k.scene = r.scene;
    k.nent = (uint8_t)(r.nent <= NENT ? r.nent : 0);
    for (i = 0; i < NENT; i++)
        k.ent[i] = (uint8_t)(i < k.nent ? l * FX_N + r.ent[i] % FX_N : 0u);
    k.drawn = 1;
    memset(&f, 0, sizeof f);
    memcpy(f.e, r.e, sizeof f.e);
    f.n = r.n;
    f.recipe = r.recipe;
    f.out = r.out;
    f.ret = r.ret;
    f.seed = r.fseed;
    f.serial = ++fx_serial;
    if (!fx_ret_ok(&f))
        f.ret = 0;
    ok = gen_set(l, &k) && fx_set(l, &f);
    gen_dis[l] = (int8_t)clampi(r.dis, 0, 100);
    fx_wet[l] = (uint8_t)clampi(r.wet, 0, 100);
    ui.force = 1;
    return ok ? SES_OK : SES_BUSY;
}
