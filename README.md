# Molecular

A firmware for the M-VAVE FM-1: a randomizable modular environment to explore and play, bringing the
philosophy of Vascular (and Wild Rider) to the FM-1 with the compromises the device asks for.

**State: the engine stage.** There is no installable build yet. What exists is the sound engine, MOLECULE,
with a host harness that renders, checks and benches it on the computer. The board layer, boot, USB, screen,
sessions and the module system come next, from Tonitor.

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
| `firmware/src/molecular.c` | the single compilation unit (the HAL comes later) |
| `firmware/src/dsp.h` | fixed-point helpers (Q15 samples, 2^32 phases; no float, no 64-bit division) |
| `firmware/src/wt.c` | the band-limited single cycles, copied to RAM at start-up |
| `firmware/src/fm6_core.c` | the FM core (msfa, Apache-2.0), with waves in its operators |
| `firmware/src/mol.c` | the engine: the voice, the grains, the macros, the families, the measurement |
| `tools/gen_tables.py` | the lookup tables (`build/gen/molecular_tables.h`) |
| `tests/host/host.c` | the host harness: `render`, `one`, `check`, `golden`, `bench` |
| `tests/run_tests.sh` | the tests; `tests/check_target_ir.sh` fails on anything the chip's runtime lacks |
| `tests/golden/gen0.txt` | the sounds of generation 0, as hashes: they must never change |

## Running the tests

```bash
tests/run_tests.sh
```

Needs `clang` and `python3`. Renders land in `build/sounds` (a WAV a sound, with `stats-*.txt`). One sound,
with its voice printed:

```bash
build/host one build/sounds FREEZE 2112
```

## Licensing

GPL-3.0-only (`LICENSE`); `fm6_core.c` is msfa under Apache-2.0 (`LICENSES/`). See `LICENSING.md`.

## Disclaimer

Molecular is unofficial. It is not made, endorsed or supported by M-VAVE, and installing it, once it can be
installed, may void any warranty that applies to your FM-1.
