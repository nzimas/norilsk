<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/assets/logo-dark.svg">
  <img src="docs/assets/logo-light.svg" alt="Norilsk" width="490">
</picture>

# Norilsk

**A firmware for the M-VAVE FM-1 for inhospitable soundscapes.**

Norilsk is not made for musical niceties. There are no pretty chords here, no polite arpeggios, no presets that
flatter. It is made for ambient hellscapes and barren, grim atmospheres: drones that never resolve, textures that
corrode as they hold, noise that does not apologise. Sound for places nobody should have to live in.

It is named after Norilsk, the nickel-smelting city above the Arctic Circle: built by prisoners of the Gulag, closed
to outsiders, sunk in polar night for weeks each winter, its air thick with sulphur dioxide, its rivers turned red by
the works upstream. Every sound here is drawn at random, wired at random into effects drawn at random, and torn apart
at will: as fit for noise experiments as for slow, dark ambient.

**Website, browser installer and session backup: <https://nzimas.github.io/norilsk/>**

State: **build 27**. Each build is described in `CHANGELOG.md`. Norilsk was called Molecular until build 17.

# Manual

## How it is put together

Three **generators** make sound; three **effect slots** wreck it; a master **delay** and **plate reverb** close the
chain. The white keys play the generator chosen last, in the scale chosen; four voices are shared between the three
generators.

- Each generator is wired by cables into one, two or three effects anywhere in the effect slots (none, one time in
  twenty), and keeps a dry share of its own that goes straight to the master. One generator may modulate another (ring,
  amplitude, filter) -- only when you link them: SHIFT, hold one generator's button, then the other's.
- Each effect slot is a chain of two or three effects, drawn from a recipe. Any effect may also be wired across into an
  effect of a later slot, or sent out to the master on its own.
- Each generator has its level, pan, mute, delay and reverb sends, a looper, and a DISINTEGRATOR; each effect slot its
  level, pan and mute.
- The first processor core renders the voices; the second runs the effects, a block behind.

Every sound swells in and keeps moving, and **nothing sounds on once no note is held, latched or looped**: released
voices fade out within three seconds and the effect slots close two seconds after the last voice -- while the master
delay's and reverb's tails ring out whole, whatever is let go or muted.

## Randomness, level by level

Almost nothing in Norilsk is set by hand at the start: it is drawn. What you do is choose, keep, throw again, and play.

1. **The sound.** MOLECULE, the engine, draws a sound from a family, a seed, a path and an engine generation, bit for
   bit the same every time: a sound is kept as its seed, and a session brings it back exactly.
2. **The note in motion.** In every voice an invisible hand (after Anima) keeps drifting the four macros; Russolo's grit
   cuts bits, crosses the sound with itself and stutters it from points of its own; the grains are taken from places
   that wander. The same key held twice never sounds the same twice.
3. **The generator** (HOME on a generator): a scene, a family weighed by how harsh it may be, how many effects it is
   cabled into and where, its dry share.
4. **The effect slot** (HOME on an effect slot): one of 22 recipes, each place filled from the recipe's choices that
   fit the slot's memory, every value drawn around its home, the outs, the cables across to the slots after it.
5. **The paths.** ABRASIVE and DARK lean every draw above: the scenes, the families, the recipes, the effects' values.
6. **Partial throws.** REWIRE draws only the cables; RAND PARAMS only an effect slot's values, or a generator's sound
   within its family; SHIFT + HOME keeps the family or the recipe; HOME held throws all six slots at once.
7. **Modulation.** SHIFT + SEQ throws a slot's two LFOs and two to six of its eight routes; the STEPS and DRIFT shapes
   and the RANDOM source keep drawing as it plays.
8. **Playing.** The DISINTEGRATOR gates, stutters, crushes and throws each generator about in pulses of random length;
   the looper's VARY redraws every note it plays, and RANDOM jumps through the loops.
9. **What chance does not touch.** A snapshot is the whole machine, recalled exactly or morphed into; sessions keep it
   all in the flash.

## The panel

SHIFT is the rightmost black key.

| Control | What it does |
|---|---|
| **FX SEL ENV** | generators 1 to 3: pressed, chosen (the keys play it); held 0.8 s, muted, or heard again |
| **LFO EDIT GLO** | effect slots 4 to 6: pressed, chosen; held 0.8 s, muted, or heard again |
| **HOME** | a new patch for the slot chosen: a generator's sound and its cables into the effects; an effect slot's recipe, effects, values and cables across |
| **SHIFT + HOME** | a new patch of the same family, or the same recipe |
| **HOME** held 0.8 s | all six slots anew |
| the white keys | play the generator chosen, in the scale chosen. With an effect slot chosen they play nothing and show nothing |
| a white key held, then **SHIFT** | its note latched (the key stays lit); **SHIFT** held, then the lit key: let go |
| **SELECT / ALGORITHM** | the level / pan of the generator chosen (or chosen last). At power-on: 1 middle, 2 left, 3 right |
| **OCT− / OCT+** | the octave, −3 to +3 |
| **ARP** | the SCALE page: SELECT the root, ALGORITHM the scale (23) |
| **KNOB 1** | playing: the DISINTEGRATOR of the generator the keys play, 0 to 100. On the SNAPSHOTS page: the morph time, 0 to 30 s |
| **KNOB 3 / KNOB 4** | the delay send / reverb send of the generator the keys play |
| **SEQ** | the DELAY page, again the REVERB page, again the MOD page, again back: KNOB 1 to 4 set the four rows |
| **SHIFT + SEQ** | the chosen slot's modulation thrown anew |
| **SHIFT** + hold a generator's button, then hold another's | LINK: the first modulates the second (RING, AM or FILTER, drawn), shown in two lines over the screen and as a band at the foot of the second's block in the first's colour; the same again unlinks (OFF) |
| 1st black key | REWIRE the chosen slot: a generator's cables, or an effect slot's outs and cables, drawn anew; its sound and values stay |
| **SHIFT** + 1st black key | RAND PARAMS: an effect slot's every value anew (in place, no break: its effects and cables stay), or a generator's sound of the same family (its cables stay) |
| 2nd black key | the ABRASIVE path on or off (lit while on) |
| 3rd black key | the DARK path on or off (lit while on). One path at a time; neither: anything goes |
| **SAVE** | the SNAPSHOTS page (again: back) |
| **SAVE** held (playing) | save into the session loaded or chosen |
| **SHIFT + SAVE** | the SESSIONS page |
| **REC** | the looper of the generator the keys play: arm a loop (the first note starts it), end it (it plays at once), again to overdub; up to 10 loops |
| **REC** held 1 s | empty that generator's loops |
| **SHIFT + REC** | VARY: each note the loops play drawn anew (level, length, now and then changed or joined) |
| **PLAY** | the loops only: stop the chosen generator's loops, or start them again from their starts. Dim when it has loops, lit while they play, dark with none, and with an effect slot chosen (then it does nothing) |
| **PLAY** held | its loops on the white keys: press one to take it out or bring it back |
| **PLAY** held + 4th to 8th black keys | the loops REVERSE, PENDULUM, RANDOM, HALF SPEED, DOUBLE SPEED (again: as recorded). They glow dimly while PLAY is held, lit while on |
| **SHIFT + PLAY** | every generator's loops stopped, or all started together |
| **SHIFT + OCT− / OCT+** | every loop a step of the scale down / up (a generator's button held too: its loops only) |
| **MASTER** | the level |
| USB MIDI notes | as the keys, on the generator chosen last |
| **OCT−** held at power-on | the rescue mode (install again); **OCT− and OCT+** held 5 s: the chip's own updater |

The black keys do what the view they are in gives them: a key with nothing to do in a view does nothing there and stays
dark.

## The screen

Made for low vision: few words, very large, high contrast. At the top, the audio's load at the left and the battery at
the right. Under it the three generators as large numbered blocks in their colours (red, blue, green), the chosen one
framed in white. Under them the three effect slots, a row each, their effects as named tiles: FLT filter, FLD fold, MU
mulaw, SHF shift, MDL modal, STR string, VOX vowel, DLY delay, TAP tape, RVS reverse, SPR spring, REV reverb, GLT
glitch, CLD cloud, SCR scream, RNG ring, STV starve, CHB cheb, BNK bank. Each tile is bordered in the colours of the
generators cabled into it; the tiles the chosen generator feeds are filled with its colour; a tile brightens as sound
passes through it; a muted row is dark. No cables are drawn. KNOB 1 shows its value on a dial over the whole screen.

## The generators

Each generator is a **MOLECULE**: six-operator FM (msfa, Dexed's core, in Felucca's integer port as Tonitor shaped it),
its operators reading the sine or one of 28 band-limited single cycles, morphed and bent by phase distortion; its sound
written into a buffer of about half a second a voice and read back as grains — a CLOUD at a rate of its own, pitched by
the sound's intervals, reversed, darkened, fed back, FROZEN — or a PULSAR at a ratio of the note.

Twenty **families**: METAL, DRONE, BRUIT, IMPACT, PULSE, GLASS, FORMANT, REED, CLOUD, SHATTER, PULSAR, FREEZE, FURNACE,
HUM, STATIC, HOWL, SCRAPE, DATA, ASH, TUNNEL. Four **scenes** choose among them: FIELD (ambient), MATERIAL (open),
FEEDBACK and EROSION (experimental), each with its own families.
Four macros — BRIGHTNESS, DETUNE (INHARMONIC on the metallic and noisy bodies), GRAIN, MOTION — moved by the invisible
hand, by the LFOs, and by you. Every sound is measured as it is made and brought to one level. `docs/ENGINE.md` has the
design in full.

### The DISINTEGRATOR

KNOB 1 while playing: an unstable gate on the generator the keys play, 0 to 100, after its sound is made and before its
cables, so the effects hear it too. The sound goes as a chain of pulses of uneven length. A pulse lets it through,
gates it down (to nothing from 60 up), stutters a slice of what it has just made (10 to 90 ms, repeated), or crushes
it (bits and rate), and may throw its dry part across the stereo image. The higher, the shorter and less even the
pulses, the more of them act, the deeper and wider. Every edge glides over about 1.5 ms: no clicks. At 0 nothing is
touched. Each generator keeps its own, in snapshots and sessions. (Ideas from Destroy FX's Skidder and from buffer
repeaters; none of their code.)

## The effect slots

Nineteen effects, two or three to a slot:

| Effect | What it is |
|---|---|
| FILTER | a driven state-variable filter: low-, band-, high-pass or notch; resonant to the edge of screaming, never piercing |
| FOLD | a wavefolder, with a second fold and a tone |
| MULAW | rate and bit reduction, the filter open enough to alias, a drive into it |
| SHIFT | a frequency shifter: every partial moved by the same 40 to 900 Hz, the harmony torn |
| MODAL | three tuned resonators, spread inharmonically, ringing to Q 325 |
| STRING | three Karplus strings on the scale's root, ringing up to 8 s |
| VOWEL | three formants morphing through the vowels, driven |
| DELAY | an echo on a mu-law line; a change of time cross-fades, never bends |
| TAPE | an echo through a worn head: dropouts, saturation, age, hiss under the sound |
| REVERSE | windows of the sound read backwards, fed back |
| SPRING | a spring tank, its chirps |
| REVERB | a plate |
| GLITCH | Russolo's bent buffers: jumps and stutters through the last 0.74 s, never another pitch |
| CLOUD | a granular cloud; it can freeze what it holds |
| SCREAM | a feedback howl, shaped |
| RING | a diode ring modulator, modulating itself |
| STARVE | a starved, gated fuzz (after the Fuzz Factory) |
| CHEB | a harmonic re-voicer by Chebyshev polynomials (after Airwindows' Pafnuty) |
| BANK | an inharmonic resonator bank: a bar, a plate, a bell, a membrane (after Mutable Instruments' Rings) |

Twenty-two **recipes** decide what may stand at each place: SPACE, ECHO, METAL, STRINGS, EROSION, GHOST, SPECTRAL,
RESONANT, FOLDS, TELEPHONE, HELL, FACTORY, GLITCH, BROADCAST, ABYSS, RUST, FREEZER, FEEDBACK, FOUNDRY, GONG, SPECTRE,
SPUTTER. Every effect, given nothing, falls silent: none sounds on by itself.

## The paths

Two black keys lean everything HOME draws (and RAND PARAMS):

- **ABRASIVE** (2nd black key): gnarly, textural, abrasive. The experimental scenes; the harsh families (BRUIT,
  SHATTER, FURNACE, STATIC, SCRAPE, DATA, METAL); more cables and cross-modulation; the torn and crushed recipes; the
  effects' drives, breakage and crushing pushed up.
- **DARK** (3rd black key): dark ambient, drones, introspection — along the lines of Lustmord and the Cold Meat
  Industry catalogue. Mostly the FIELD scene; drones, hums, tunnels, ash, freezes, nothing harsh; 3 dB quieter; the
  spaces and ringing recipes (SPACE, GHOST, ABYSS, FREEZER, RESONANT, GONG ..); the values darker, less driven, the
  spaces larger.

Neither on: anything goes. The path is kept with the session and with each snapshot.

## Modulation

Every slot, generator or effect slot, has two LFOs (SINE, TRI, SAW UP, SAW DOWN, SQUARE, STEPS, DRIFT; FREE, NOTE or
ONCE; a fade-in) and eight routes, A to H: each from a source (LFO 1, LFO 2, VELOCITY, KEY, RANDOM) to a destination
by an amount. A generator's destinations: its four macros, level, pan, sends, the LFOs' rates. An effect slot's: the
values of the effects standing in it, by place (so HOME draws new effects and the LFO moves the new one's value at
that place), its level, pan, the LFOs' rates. SEQ's third page sets them; SHIFT + SEQ throws them.

## Snapshots and sessions

**A snapshot is the whole machine**: every slot's sound, effects, values and cables, levels, pans, mutes and sends, the
DISINTEGRATORs, the master delay and reverb, the modulation, the scale and root, the octave, the path — and how it is
played: the latched notes, whether each generator's loops play, which loops are held out, their direction, speed, VARY
and moves along the scale. (The loops' recordings themselves belong to the session.) Sixteen to a session.

**SAVE** brings up the SNAPSHOTS page: a white key let go recalls its snapshot; held 0.8 s, it stores the machine as it
is into it; SHIFT held too, it empties it. KNOB 1 there sets the **morph time**, 0 to 30 s: a snapshot recalled flows
into place over it — levels, pans, sends, values and LFOs glide; sounds cross over on voices of their own and effect
chains are faded out and in, in turn; what has no between switches at the middle. The square and key of the snapshot
it flows into blink until it is there.

**SHIFT + SAVE** brings up the SESSIONS page: PRESETS through 64 slots, the number very large (red when it holds a
session, grey when empty); SAVE loads it, or starts a clean slate in an empty one; SAVE held saves into it (asked
first). Playing, SAVE held saves at once into the session loaded. A session keeps the machine, its sixteen snapshots,
the latched notes and every loop, in the FM-1's flash.

Norilsk writes only to blank flash or to its own retired sessions, never over another firmware's data. Other firmware
does not return the favour: back your sessions up before installing another.

## The looper

Tonitor's looper, on each generator: up to ten free-running loops, each on its own clock — no tempo, no grid. REC arms
a loop (the first note starts it), REC again ends it and it plays at once; again, while they play, an overdub. A note
held across the end of a loop is tied over. PLAY held shows the loops on the white keys to take out or bring back, and
lights the 4th to 8th black keys: REVERSE, PENDULUM, RANDOM, HALF SPEED, DOUBLE SPEED. SHIFT + OCT moves every loop
along the scale. SHIFT + REC: VARY.

## Backing up sessions

The website's [backup page](https://nzimas.github.io/norilsk/backup.html) saves every session on the FM-1 to a file
and puts them back, from a desktop Chrome, Edge or Opera. From a terminal: `tools/fm1_sessions.py` lists them,
`tools/fm1_sessions.py save N` saves one, `tools/fm1_sessions.py restore FILE` writes one back. Both talk only to an
FM-1 whose USB name is FM-1 Norilsk.

## USB audio

Connected to a computer, the FM-1 is also a USB audio interface (class compliant, no driver): the master, stereo,
16-bit, at 48 kHz (or 44.1 kHz), before the MASTER knob -- a recording does not follow the headphones' level.
Tonitor's and Bigorna's.

# Building from source

`./setup.sh` once: a Python environment, JieLi's toolchain and three files of the AC79 SDK (Tonitor's procedure). On
macOS, Docker Desktop must be running to build. Then:

```bash
./build.sh
```

It must print `package ... build N` with no FAIL. `./install.sh` builds and installs over USB; `./install.sh --no-build`
installs `build/norilsk.fwsc` as it is; `./install.sh FILE.fwsc` installs another package; `./install.sh --info` says
what the connected FM-1 runs.

## Running the tests

```bash
tests/run_tests.sh
```

Needs `clang` and `python3` with Pillow. It checks the firmware unit for anything the chip's runtime lacks (no float,
no 64-bit division), renders thousands of sounds (none silent, none clipping, none running away), plays the panel
through the real firmware code, checks every effect falls silent, every recipe fits its slot's memory, snapshots and
morphs land exactly, sessions survive power cuts and never touch another firmware's flash, and that the engine's
generations still draw what they drew. Renders land in `build/sounds` (WAV), the screens in `build/screens` (PNG). The
website: `tools/make_logo.py`, `tools/make_demos.sh`, then `tools/make_site.py` (into `docs/`, served by GitHub Pages).

## Layout

| Path | What |
|---|---|
| `firmware/src/norilsk.c` | the single compilation unit: the sources in order |
| `firmware/hal/`, `crt0.S`, `app.ld`, `loader/` | the board layer, boot, memory map and update loader (Tonitor's, from Felucca and SLOOP) |
| `firmware/src/main.c`, `usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c` | boot and interrupts, USB, the installer's handshake, the rescue mode, the screen |
| `firmware/src/mol.c`, `fm6_core.c`, `wt.c` | MOLECULE: the voice, the grains, the hand, the grit, the families; the FM core; the waves |
| `firmware/src/gens.c` | the three generators: the draw, the voices, the two cores, the DISINTEGRATOR, the mix |
| `firmware/src/rack.c`, `reverb.c` | the effect slots: the nineteen effects, the recipes, the paths, the memory, the cables |
| `firmware/src/mod.c`, `loop.c`, `master.c` | the modulation; the loopers; the master delay and reverb |
| `firmware/src/sessions.c`, `backup.c` | sessions, snapshots and the morph; the sessions over USB |
| `firmware/src/ui.c` | the panel and the screen |
| `tools/` | the build, the installer, the sessions tool, the tables, fonts, logo, demos and website |
| `tests/host/host.c` | the host harness: every test named above, `render`, `one`, `replay` |
| `docs/` | the website (GitHub Pages); `docs/ENGINE.md` the engine's design |
| `site/` | the website's pages, before `tools/make_site.py` frames them |

# Licensing

The firmware is GPL-3.0-only (`LICENSE`); `fm6_core.c` is msfa under Apache-2.0 (`LICENSES/`); the website and logo
are MIT. See `LICENSING.md`.

# Disclaimer

Norilsk is unofficial. It is not made, endorsed or supported by M-VAVE. Installing it replaces the FM-1's firmware
and may void any warranty that applies to your device; an interrupted update can leave the FM-1 unable to start until
it is recovered. The software comes with no warranty of any kind; you install it entirely at your own risk. Norilsk is
named after the city; it has nothing to do with the company that runs its smelters.
