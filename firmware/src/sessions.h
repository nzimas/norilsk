/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR: what the panel (ui.c) sees of the sessions and snapshots (sessions.c, included after it) */

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
static void snap_store(uint32_t i);
static int snap_recall(uint32_t i);
static void snap_clear(uint32_t i);
static int snap_has(uint32_t i);
static int snap_current(void);
