/* SPDX-License-Identifier: GPL-3.0-only
 * MOLECULAR -- a firmware for the M-VAVE FM-1: a randomizable modular environment to explore and play.
 * One compilation unit (the HAL will be header-only, as Tonitor's); the order matters.
 *
 * This is the engine stage of the project: no board layer, no boot, no USB yet. Those come over from Tonitor
 * (itself built on Felucca and SLOOP, GPL-3.0) when the first installable build is made. FM is msfa (Google,
 * Pascal Gauthier, Apache-2.0) in Felucca's integer port, with waves in its operators and a grain cloud after
 * them (MOLECULE, mol.c). */
#include <stdint.h>
#ifndef MOLECULAR_VERSION
#define MOLECULAR_VERSION "0.01"
#endif
#include "molecular_tables.h"
#ifndef HOST
static void fm1_wdt_feed(void) {}   /* (the HAL's, once there is one: the watchdog fed while a sound is measured) */
#endif

#define NELEM(a) (sizeof(a) / sizeof((a)[0]))

#include "libc.c"
#include "dsp.h"
#include "wt.c"
#include "fm6_core.c"
#include "mol.c"
