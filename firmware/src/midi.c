/* SPDX-License-Identifier: GPL-3.0-only
 * NORILSK: the MIDI settings (GLOBAL SETUP's, setup.c; Tonitor's, for three generators) and what the audio ISR sends:
 * each generator's notes on its OUT channel, as they start and end (the keys', the loops', MIDI's), when SEND NOTES is
 * on. A note sent on is always sent off, on the channel it went out on, whatever the settings are by then. The main
 * loop's packets (the CCs sent, setup.c) wait in a ring of their own, which the ISR empties into usb.c's at each
 * block (one producer for usb.c's ring). No clock, transport or program changes. */

typedef struct {
    uint8_t in_ch[NGEN];         /* each generator's channel in, 0 off, 1 .. 16 */
    uint8_t auto_ch;             /* the generator the keys play's, 0 off */
    uint8_t out_ch[NGEN];        /* each generator's channel out, 0 off */
    uint8_t rcv_note, rcv_cc, send_note, send_cc;   /* 0 / 1 */
    uint8_t usb_in, trs_in;      /* the ports listened to */
    uint8_t usb_aud;             /* (GLOBAL SETUP's AUDIO, kept with these) USB audio: 0 STEREO (the mix), 1 TRACKS (three) */
} midi_cfg_t;
#define MIDI_CFG0 {{1, 2, 3}, 16, {1, 2, 3}, 1, 1, 0, 0, 1, 1, 0}
static midi_cfg_t mcfg = MIDI_CFG0;

static uint16_t midi_held[NGEN][8];              /* (each generator's notes sent on, a bit each: one off each) */
static uint8_t midi_och[NGEN];                   /* (the channel they went out on, 0 .. 15) */
static void midi_off_sent(uint32_t l, uint32_t note)
{
    uint32_t bit = 1u << (note & 15u), i = (note >> 4) & 7u;
    if (midi_held[l][i] & bit) {
        midi_out_event(0x08u | (0x80u | midi_och[l]) << 8 | (note & 0x7Fu) << 16);
        midi_held[l][i] = (uint16_t)(midi_held[l][i] & ~bit);
    }
}
static void midi_gen_off(uint32_t l)              /* (every note generator l sent on: off) */
{
    uint32_t n;
    for (n = 0; n < 128u; n++)
        midi_off_sent(l, n);
}
static void midi_note_out(uint32_t l, uint32_t note, uint32_t vel, int on)   /* (the audio ISR) */
{
    uint32_t ch = mcfg.out_ch[l];
    note &= 0x7Fu;
    midi_off_sent(l, note);                      /* (sounding: off first, or only) */
    if (!on || !mcfg.send_note || !ch || ch > 16u)
        return;
    if (ch - 1u != midi_och[l]) {                /* (OUT CHAN changed: what went out on the old one, off there) */
        midi_gen_off(l);
        midi_och[l] = (uint8_t)(ch - 1u);
    }
    midi_out_event(0x09u | (0x90u | (ch - 1u)) << 8 | note << 16 | (vel ? vel : 1u) << 24);
    midi_held[l][note >> 4] = (uint16_t)(midi_held[l][note >> 4] | 1u << (note & 15u));
}

#define MMQ 32u
static uint32_t mm_q[MMQ];
static volatile uint32_t mm_w, mm_r;
static void midi_main_send(uint32_t pkt)          /* (the main loop) */
{
    if (mm_w - mm_r < MMQ) {
        mm_q[mm_w % MMQ] = pkt;
        RING_PUBLISH();
        mm_w++;
    }
}
static void midi_main_drain(void)                 /* (the audio ISR, each block) */
{
    while (mm_r != mm_w) {
        midi_out_event(mm_q[mm_r % MMQ]);
        RING_PUBLISH();
        mm_r++;
    }
}
