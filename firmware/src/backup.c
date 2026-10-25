/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR: the sessions read over USB MIDI -- Tonitor's backup protocol (backup.c, the same author), reading only.
 * The installer's framing (ota.c: F0 pack7(00 59 cmd len24 body ~sum) F7):
 *   0x50  list                       -> 0x50  count, then a record each: number (u16), length (u32)
 *   0x51  read  number off(u32) n(u16, <= 256)   -> 0x51  number off n, then n bytes of the record
 *   0x52 .. 0x54 (Tonitor's writes)  -> the same command, status 2 (not as asked): nothing is written here.
 * tools/fm1_sessions.py lists and saves them. All in the main loop. */

static int bk_wire(const uint8_t *p, uint32_t n);   /* one F0 .. F7 message to the host (main.c; host.c) */

#define BK_DATA 256u
typedef char bk_read_fits[7u + 1u + SES_MAX * 6u >= 7u + 8u + BK_DATA ? 1 : -1];
static uint8_t bk_msg[7u + 1u + SES_MAX * 6u], bk_out[2u + (sizeof bk_msg * 8u + 6u) / 7u];

static uint32_t bk_rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void bk_wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static void bk_send(uint32_t cmd, uint32_t n)        /* the body already in bk_msg + 6 */
{
    uint32_t i, s = 0, o = 1, acc = 0, nb = 0;
    bk_msg[0] = 0;
    bk_msg[1] = 0x59;
    bk_msg[2] = (uint8_t)cmd;
    bk_msg[3] = (uint8_t)n;
    bk_msg[4] = (uint8_t)(n >> 8);
    bk_msg[5] = (uint8_t)(n >> 16);
    for (i = 0; i < n; i++)
        s += bk_msg[6u + i];
    bk_msg[6u + n] = (uint8_t)~s;
    bk_out[0] = 0xF0;
    for (i = 0; i < 7u + n; i++) {                   /* pack7, as ota.c's */
        acc |= (uint32_t)bk_msg[i] << nb;
        nb += 8u;
        while (nb >= 7u) {
            bk_out[o++] = (uint8_t)(acc & 0x7Fu);
            acc >>= 7;
            nb -= 7u;
        }
    }
    if (nb)
        bk_out[o++] = (uint8_t)(acc & 0x7Fu);
    bk_out[o++] = 0xF7;
    bk_wire(bk_out, o);
}
/* a message from the host (decoded: 00 59 cmd len24 body sum) */
static void bk_handle(const uint8_t *m, uint32_t d)
{
    uint32_t cmd = m[2], n = d - 7u, i;
    const uint8_t *b = m + 6;
    if (cmd == 0x50u) {                              /* the list */
        for (i = 0; i < ses.n; i++) {
            ses_head_t h;
            ses_flash_read(SES_LO + ses.sect[i] * SES_SECT, &h, sizeof h);
            bk_msg[7u + i * 6u] = (uint8_t)ses.num[i];
            bk_msg[8u + i * 6u] = (uint8_t)(ses.num[i] >> 8);
            bk_wr32(bk_msg + 9u + i * 6u, h.len);
        }
        bk_msg[6] = (uint8_t)ses.n;
        bk_send(0x50u, 1u + ses.n * 6u);
    } else if (cmd == 0x51u && n == 8u) {            /* a piece of a record */
        uint32_t num = b[0] | (uint32_t)b[1] << 8, off = bk_rd32(b + 2), cnt = b[6] | (uint32_t)b[7] << 8, len = 0;
        int at = ses_find(num);
        if (at >= 0) {
            ses_head_t h;
            ses_flash_read(SES_LO + ses.sect[at] * SES_SECT, &h, sizeof h);
            len = h.len;
        }
        cnt = at < 0 || off >= len ? 0u : cnt > BK_DATA ? BK_DATA : cnt;
        cnt = off + cnt > len ? len - off : cnt;
        memcpy(bk_msg + 6, b, 6);
        bk_msg[12] = (uint8_t)cnt;
        bk_msg[13] = (uint8_t)(cnt >> 8);
        if (cnt)
            ses_flash_read(SES_LO + ses.sect[at] * SES_SECT + off, bk_msg + 14, cnt);
        bk_send(0x51u, 8u + cnt);
    } else {                                         /* the writes: not here */
        bk_msg[6] = 2u;
        bk_send(cmd, 1u);
    }
}
