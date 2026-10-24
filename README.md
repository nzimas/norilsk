# Molecular

A firmware for the M-VAVE FM-1: a randomizable modular environment to explore and play, bringing the
philosophy of Vascular (and Wild Rider) to the FM-1 with the compromises the device asks for.

**State: 0.04.** Three generator slots, each a MOLECULE generator, share three effect slots the way synthesizers
share a pedalboard. HOME on a generator draws its sound and its cables into the effects; HOME on an effect slot draws
its chain of two or three effects from a recipe, their values and its cables across to the other effect slots. The
white keys play the generator chosen last, in the scale chosen. The second core runs the effects. No sessions, no
patterns, no internal clocks yet. `CHANGELOG.md` says what each version does.

## Playing it

| Control | What it does |
|---|---|
| **FX SEL ENV** | generators 1 to 3: pressed, chosen (the keys play it); held 0.8 s, muted, or heard again |
| **LFO EDIT GLO** | effect slots 4 to 6: pressed, chosen; held 0.8 s, muted, or heard again |
| **HOME** | a new patch for the slot chosen: a generator's sound and its cables into the effects; an effect slot's recipe, effects, values and cables across |
| **SHIFT + HOME** | a new patch of the same family, or the same recipe (SHIFT: the rightmost black key) |
| **HOME** held 0.8 s | all six slots anew |
| the white keys | play the generator chosen last, in the scale chosen. Four voices for the three generators |
| a white key held, then **SHIFT** | its note latched (the key stays lit); **SHIFT** held, then the lit key: let go |
| **SELECT / ALGORITHM** | the chosen slot's level / pan |
| **ARP** | the SCALE page: SELECT the scale (23), ALGORITHM the root |
| **OCT- / OCT+** | the octave, -3 .. +3 |
| **MASTER** | the level |
| USB MIDI notes | as the keys, on the generator chosen last |
| **OCT-** held at power-on | the rescue mode (install again); **OCT- and OCT+** held 5 s: the chip's own updater |

The screen: the audio's load at the top left, the battery at the top right; the three generators as large numbered
blocks; under them the three effect slots as rows of named tiles -- FLT filter, FLD fold, MU mulaw, SHF shift, MDL
modal, STR string, VOX vowel, DLY delay, TAP tape, RVS reverse, SPR spring, REV reverb -- each bordered in the
colours of the generators wired into it, the keyboard generator's tiles filled with its colour, a muted row dark, the
chosen row framed in white. No cables are drawn.

## Installing

`./setup.sh` once (a Python environment, JieLi's toolchain, three files of the AC79 SDK; on macOS Docker Desktop
must be running to build), then `./install.sh` (builds, then installs over USB), or `./install.sh FILE.fwsc`.
`./install.sh --info` says what the connected FM-1 runs. Installing replaces the firmware the FM-1 runs; see the
disclaimer.

## MOLECULE, the engine

Six-operator FM (msfa, Dexed's core, in Felucca's integer port as Tonitor shaped it) with two things added to
the operators and one after them:

- **Waves.** An operator reads the sine, or one of 28 band-limited single cycles (the four primitives, Bigorna's
  sixteen harmonic sets, eight made for bells and FM), morphed with the next of its bank, and its phase may be
  bent (phase distortion, after the Casio CZ), as drawn or following its envelope.
- **Grains.** The operators' sound is written into a buffer of about half a second a voice and read back as
  grains: a CLOUD at a rate of its own (pitched by the sound's intervals, reversed, darkened, fed back, and, when
  the sound asks for it, FROZEN a moment after the note begins), or a PULSAR at a ratio of the note's frequency
  (a grain a period: formants, sub-harmonics by a burst mask).

Twelve families of sound objects: METAL, DRONE, BRUIT, IMPACT, PULSE, GLASS, FORMANT, REED (bodies), CLOUD,
SHATTER, PULSAR, FREEZE (clouds that borrow a body). Three paths lean the draw: OPEN, AMBIENT, EXPERIMENTAL.
Four macros: BRIGHTNESS, DETUNE (INHARMONIC on the metallic and noisy bodies), GRAIN, MOTION. Every sound is
measured as it is made and brought to one level. `docs/ENGINE.md` has the design in full.

## Layout

| Path | What |
|---|---|
| `firmware/src/molecular.c` | the single compilation unit: the HAL, then the sources in order |
| `firmware/hal/`, `crt0.S`, `app.ld`, `loader/` | the board layer, boot, memory map and update loader (Tonitor's, from Felucca and SLOOP) |
| `firmware/src/main.c`, `usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c` | boot and ISRs, USB, the installer's handshake, the rescue mode, the screen |
| `firmware/src/gens.c` | the three generators: the draw, the four voices, the two cores, the mix |
| `firmware/src/rack.c`, `reverb.c` | the three effect slots: the twelve effects, the recipes, the memory, the wiring |
| `firmware/src/ui.c` | the panel and the screen |
| `firmware/src/dsp.h` | fixed-point helpers (Q15 samples, 2^32 phases; no float, no 64-bit division) |
| `firmware/src/wt.c` | the band-limited single cycles, copied to RAM at start-up |
| `firmware/src/fm6_core.c` | the FM core (msfa, Apache-2.0), with waves in its operators |
| `firmware/src/mol.c` | the engine: the voice, the grains, the macros, the families, the measurement |
| `tools/gen_tables.py` | the lookup tables (`build/gen/molecular_tables.h`) |
| `tests/host/host.c` | the host harness: the engine's `render`, `one`, `check`, `golden`, `bench`; the firmware's `patches`, `modules`, `flows`, `screens` |
| `docs/ENGINE.md`, `docs/PROCESSORS.md` | the engine's design; Vascular's processors judged for this chip |
| `tests/run_tests.sh` | the tests; `tests/check_target_ir.sh` fails on anything the chip's runtime lacks |
| `tests/golden/gen0.txt` | the sounds of generation 0, as hashes: they must never change |

## Running the tests

```bash
tests/run_tests.sh
```

Needs `clang` and `python3` with Pillow (the fonts). Renders land in `build/sounds` (a WAV a sound and a WAV a
patch, with `stats-*.txt` and `patches.txt`), the screens in `build/screens` (PNG). One sound, with its voice
printed:

```bash
build/host one build/sounds FREEZE 2112
```

## Licensing

GPL-3.0-only (`LICENSE`); `fm6_core.c` is msfa under Apache-2.0 (`LICENSES/`). See `LICENSING.md`.

## Disclaimer

Molecular is unofficial. It is not made, endorsed or supported by M-VAVE, and installing it, once it can be
installed, may void any warranty that applies to your FM-1.
