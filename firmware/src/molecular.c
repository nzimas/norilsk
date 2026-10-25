/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR -- a firmware for the M-VAVE FM-1: a randomizable modular environment to explore and play.
 * One compilation unit (the HAL is header-only); the order matters.
 *
 * Built on Tonitor (Nuno Zimas), itself on Felucca (Leo Kuroshita, Hügelton Instruments) and SLOOP (isod89),
 * GPL-3.0: the HAL, the boot, the USB stack, the update entry and loader, the rescue mode, the screen. FM is msfa
 * (Google, Pascal Gauthier, Apache-2.0) in Felucca's integer port, with waves in its operators and a grain cloud
 * after them (MOLECULE, mol.c). */
#include <stdint.h>
#ifndef MOLECULAR_ID
#define MOLECULAR_ID "FM-1_700"    /* the package identity (tools/build.py sets it) */
#endif
#ifndef MOLECULAR_BUILD
#define MOLECULAR_BUILD "BUILD 13"
#endif
#define FELUCCA_ID MOLECULAR_ID
#define FELUCCA_OTA 1            /* the update entry (the installer), and the rescue mode */
#define FELUCCA_OTA_DRYRUN 0
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0            /* (USB audio out: later, from Tonitor's usb.c) */
#define FELUCCA_UAC_48K 0
#define HALF_FRAMES 256u         /* an I2S half buffer: 5.8 ms (main.c renders it) */
#define FM1_CORE1 1              /* CPU1 runs the effects and some voices (hal/fm1_multicore.h; after Tonitor, BIGORNA build 32
                                  * and Melodee 1.4.5) */

#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_power.h"
#include "fm1_irq.h"
#include "fm1_core1_bar.h"
#include "fm1_core1_stop.h"
#include "fm1_multicore.h"
#include "fm1_guard.h"
#include "fm1_input.h"
#include "fm1_timer.h"
#include "fm1_audio.h"
#include "fm1_adc.h"
#include "fm1_lcd_hw.h"
#include "molecular_tables.h"

#define NELEM(a) (sizeof(a) / sizeof((a)[0]))
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")   /* a slot's store before the index's */
static volatile uint32_t fm1_ms;                           /* milliseconds since boot (TIMER5) */
#include "bootguard.h"
bootguard_t bootguard __attribute__((section(".noinit")));

#include "libc.c"
#include "lcd.c"
#include "dsp.h"
#include "gfx.c"
#include "wt.c"
#include "fm6_core.c"
#include "mol.c"
#include "usb.c"
#include "rack.c"
#include "gens.c"
#include "loop.c"
#include "mod.c"
#include "master.c"
#include "sessions.h"
#include "ui.c"
#include "sessions.c"
#include "main.c"
