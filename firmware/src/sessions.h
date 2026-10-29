/* SPDX-License-Identifier: GPL-3.0-only
 * NORILSK: what the panel (ui.c) sees of the sessions and snapshots (sessions.c, included after it) */

#define SES_SLOTS 64u            /* the slots: sessions 1 .. 64 (Tonitor's) */
#define NSNAP 16u                /* a session's snapshots */
enum { SES_OK, SES_FAIL, SES_FULL, SES_NOFLASH, SES_BUSY };
static int ses_save(uint32_t num);
static int ses_load_slot(uint32_t num);
static int ses_has(uint32_t num);
static uint32_t ses_current(void);
static uint32_t ses_next_number(void);
static void ses_fresh(uint32_t num);
static uint32_t ses_room(void);
static int ses_delete(uint32_t num);           /* (build 28: the session pages) */
static int ses_copy(uint32_t from, uint32_t to);
static uint32_t ses_mode_of(uint32_t num);
static int sig_set(uint32_t m);
static void snap_store(uint32_t i);
static int snap_recall(uint32_t i);
static void snap_clear(uint32_t i);
static int snap_has(uint32_t i);
static int snap_current(void);
static int snap_morph_target(void);
static void morph_poll(void);
/* GLOBAL SETUP (setup.c, build 43): its record's number in the sessions' store, its page, the MIDI in */
#define SET_NUM 1000u
static void setup_open(void);
static void setup_press(int sh);
static void setup_preset(int32_t s);
static void setup_draw(void);
static void midi_take(void);
