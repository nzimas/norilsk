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

State: **build 38**. Each build is described in `CHANGELOG.md`. Norilsk was called Molecular until build 17.

# Manual

## First steps

1. Power on. Generator 1 is chosen; each generator already has a sound and a lane of effects.
2. Play white keys. Press **FX**, **SEL** or **ENV** to hear the other generators.
3. Press **HOME** until a generator sounds right to you. **SHIFT + HOME** keeps its family and tries again.
4. Hold a key and press **SHIFT** to latch it. Latch notes on two or three generators to build a texture.
5. Turn **KNOB 2** for more or less effect, **KNOB 1** to disintegrate, **KNOB 3** and **KNOB 4** for delay and reverb.
6. Press **SAVE**, hold a white key 0.8 s: the whole machine is stored in that snapshot. Make a few, set a morph time
   with **KNOB 1** and move between them.
7. Hold **SAVE** (outside the SNAPSHOTS page) to keep everything in the FM-1's flash as a session.

## Cheat sheet

SHIFT is the rightmost black key. Black keys are contextual: a key with no job in the current view does nothing and
stays dark. "Hold" means 0.8 s unless stated otherwise.

### Slots

| To | Do this |
|---|---|
| Choose generator 1, 2 or 3 (keys play it) | Press **FX**, **SEL** or **ENV** |
| Choose effect slot (lane) 1, 2 or 3 | Press **LFO**, **EDIT** or **GLO** |
| Mute or unmute a slot | Hold its button |
| Mute fade in / fade out of a generator (OFF to 60 s) | **SHIFT** + its button, let go: its page. **KNOB 1** MUTE FADE IN, **KNOB 2** MUTE FADE OUT. Its button to leave |
| Level / pan of chosen generator (pan moves its lane too) | Turn **SELECT** / **ALGORITHM** |
| Link two generators (first modulates second) | Hold **SHIFT**, hold first generator's button, then hold second's. Repeat to unlink |

### Playing

| To | Do this |
|---|---|
| Play | White keys: chosen generator, in current scale. Same with USB MIDI notes |
| Change octave (−3 to +3) | **OCT−** / **OCT+** |
| Latch a note | Hold white key, press **SHIFT** (key stays lit) |
| Release a latched note | Hold **SHIFT**, press lit key |
| Set root and scale | **ARP** opens SCALE page: **SELECT** root, **ALGORITHM** scale (23). **ARP** again to leave |
| Random root and scale | **SHIFT + ARP** |
| DISINTEGRATOR of generator being played (0 to 100) | **KNOB 1** |
| Master DRIVE (0 off to 100) | **SHIFT + KNOB 1** |
| FX MIX of chosen lane, dry to wet | **KNOB 2** |
| Delay send / reverb send of generator being played | **KNOB 3** / **KNOB 4** |
| Output level | **MASTER** |

### Drawing new material

| To | Do this |
|---|---|
| New sound for chosen generator (with its lane's effects) | **HOME** |
| New effect chain for chosen lane | Choose the effect slot, press **HOME** |
| New, but same family (or same recipe) | **SHIFT + HOME** |
| Everything new, all six slots | Hold **HOME** |
| New cables only | 1st black key (REWIRE) |
| New values only (effects), or new sound of same family (generator) | **SHIFT** + 1st black key (RAND PARAMS) |
| Bias draws towards harsh, torn material | 2nd black key: ABRASIVE path on/off |
| Bias draws towards dark ambient and drones | 3rd black key: DARK path on/off |

### Pages

| To | Do this |
|---|---|
| Delay settings (TIME, FEEDBACK, DAMP, MIX) | **SEQ**, then **KNOB 1** to **4** |
| Reverb settings (SIZE, DECAY, DAMP, MIX) | **SEQ** twice, then **KNOB 1** to **4** |
| Modulation of chosen slot | **SEQ** three times. **PRESETS** picks LFO 1, LFO 2 or route A to H; **KNOB 1** to **4** set its values |
| Back to playing | **SEQ** a fourth time |
| A generator's page (MUTE FADE IN, MUTE FADE OUT) | **SHIFT** + its button, then **KNOB 1** / **KNOB 2**. Its button to leave |
| LOAD page: how busy the FM-1 is | 10th black key (beside SHIFT), lit while on. Again to leave |
| Random modulation for chosen slot | **SHIFT + SEQ** |

### Looper (generator being played)

| To | Do this |
|---|---|
| Record a loop | **REC** to arm (first note starts it), **REC** again to end (plays at once) |
| Overdub | **REC** while loops play |
| Clear loops | Hold **REC** 1 s |
| VARY on/off | **SHIFT + REC** |
| Stop or restart loops | **PLAY** |
| Stop or start all generators' loops | **SHIFT + PLAY** |
| Take loops out or bring them back | Hold **PLAY**, press white keys |
| REVERSE, PENDULUM, RANDOM, HALF SPEED, DOUBLE SPEED | Hold **PLAY**, press 4th to 8th black key (again: off) |
| Move loops one scale step down / up | **SHIFT + OCT−** / **OCT+** (hold a generator's button too for its loops only) |

### Snapshots

| To | Do this |
|---|---|
| Open SNAPSHOTS page | **SAVE** (again, or a slot button, to leave) |
| Recall snapshot | Press and release its white key |
| Store current state | Hold its white key 0.8 s, or move frame with **PRESETS** and hold **SAVE** |
| Empty snapshot | Hold **SHIFT**, hold its white key |
| Morph time (0 to 30 s) | **KNOB 1** on SNAPSHOTS page |

### Sessions

| To | Do this |
|---|---|
| Save while playing | Hold **SAVE** (saves into loaded session; with none loaded, offers first empty slot) |
| Open SESSIONS list | **SHIFT + SAVE** |
| Pick a session | **PRESETS** |
| Open its page (BACK, LOAD or NEW, SAVE, COPY, DELETE) | **SAVE** |
| Pick and run a row | **PRESETS**, then **SAVE** |
| Go back a level | **SHIFT + SAVE** |

### Start-up

| To | Do this |
|---|---|
| Rescue mode (install again) | Hold **OCT−** while powering on |
| Chip's own updater | Hold **OCT−** and **OCT+** 5 s |

## How it is put together

Norilsk has three generators, three effect slots, a master delay and a plate reverb. I think of the generators as
furnaces: each burns a sound. Effect slots are the pipes the sound runs through; delay and reverb are the air the
fumes end up in. In practice:

- Each generator plays through its own **lane** of effects: generator 1 through lane 1, 2 through 2, 3 through 3. A
  small dry part of each generator also goes straight to the master.
- Each lane is a chain of two or three effects, drawn from a recipe. An effect before the last can also send to the
  master on its own.
- White keys play the generator chosen last, in the current scale. Four voices are shared by the three generators.
- Each generator has level, pan, mute, delay and reverb sends, a looper and a DISINTEGRATOR. Each lane has level
  and mute. A generator's pan also pans its lane and every effect in it: panning is set on the generator only.
- One generator can modulate another (ring, amplitude or filter), but only if you link them by hand: hold SHIFT, hold
  one generator's button, then the other's.

CPU core 1 renders the voices. Core 2 runs the effects, delay and reverb, one block (0.73 ms) behind.

Every sound swells in and keeps moving. However, **nothing sounds on once no note is held, latched or looped**:
released voices fade out within three seconds, and effect slots close two seconds after the last voice. Delay and
reverb are the exception: their tails always ring out in full, whatever you let go or mute.

## What is random and what isn't

Almost nothing is set by hand at the start. It is drawn at random; your job is to choose, keep, throw again and play.
Randomness works at every scale:

1. **Sound.** FURNACE, the engine, draws a sound from family, seed, path and engine generation. The same four values
   always give exactly the same sound, so a session only needs to store the seed.
2. **Note while it sounds.** An invisible hand (after Anima) keeps moving each voice's four macros. Russolo's grit
   drops bits, crosses the sound with itself and makes it stutter. Grains are read from places that wander. In other
   words, the same key held twice never sounds the same twice.
3. **Generator** (HOME on a generator): scene, family (chosen by how harsh it may be), which effects it is cabled
   into, and how much of it is heard dry.
4. **Effect slot** (HOME on an effect slot): one of 24 recipes. Each place in the recipe gets one of its allowed
   effects that fits the slot's memory; every value is drawn around its default, and the outs are drawn too. HOME on a
   generator also draws its lane's chain.
5. **Paths.** ABRASIVE and DARK bias every draw above: scenes, families, recipes and effect values.
6. **Partial draws.** REWIRE draws only cables. RAND PARAMS draws only an effect slot's values, or a new sound of the
   same family for a generator. SHIFT + HOME keeps family or recipe. Holding HOME draws all six slots.
7. **Modulation.** SHIFT + SEQ draws a slot's two LFOs and two to six of its eight routes. STEPS and DRIFT shapes and
   the RANDOM source keep drawing new values as they play.
8. **Playing.** DISINTEGRATOR gates, stutters, crushes and pans each generator in pulses of random length. Looper's
   VARY changes every note it plays; RANDOM jumps between loops.
9. **What isn't random.** A snapshot stores the whole machine and recalls it exactly, or morphs into it. Sessions keep
   everything in flash.

## Screen

Designed for low vision: few words, very large type, high contrast.

Main view is drawn as a place. Smog drifts slowly across the top, with audio load on the left and battery on the
right. Below ground, the three effect slots are rusty pipes, one above the other. Each effect is a section between
bolted flanges, its name on a dark plate: FLT filter, FLD fold, MU mulaw, SHF shift, MDL modal, STR string, VOX vowel,
TAP tape, RVS reverse, SPR spring, GLT glitch, CLD cloud, SCR scream, RNG ring, STV starve, CHB cheb, BNK bank, SMR
smear, DIF diffuse, SLW slew, XOR xor, DST dust.

Under each name, a band shows the colours of the generators feeding it. Plates fed by the chosen generator are filled
with its colour. A section glows hotter while sound passes through it; a muted pipe is cold grey.

At the bottom, the generators are brick furnaces, fire in their mouths in their colours (red, blue, green): brighter
while they sound, dark when muted. Chosen slot has a white frame. Cables are not drawn. KNOB 1 shows its value on a
full-screen dial.

On the settings pages (DELAY, REVERB, MOD, a generator's page) each setting's name is amber and its value white. The
setting just turned is black on white, its name dark amber.

The LOAD page shows each part of the work, its peak over the last second, in % of the time a block has (0.73 ms):
CPU 0 (the whole block), CPU 1 (its share), VOICES, LANES, MASTER (delay, reverb, drive), MIX (what CPU 0 does
alone). A value at 90 % or more turns red. Near 100 % on CPU 0, the FM-1 lets a held note go and says OVERLOAD.

Messages (LANE 2 REWIRED, GEN 1 WIRE, OVERLOAD) cover the top of the screen for a moment. One too wide for a line
takes two; a line still too wide drops one size. No word runs off the screen.

## Generators

Each generator runs an instance of **FURNACE**, the engine, playing one sound drawn from thirty-six families. Four
voices are shared by the three generators, and each voice is a complete instance of the engine. Because of this, one
sound on two keys never sounds quite the same. Details: [FURNACE engine](#furnace-engine) below.

### Mute fades

SHIFT + a generator's button, let go, opens its page: GEN 1, GEN 2 or GEN 3 at the top, two settings below.

- **MUTE FADE IN** (KNOB 1): how long the generator takes to come back when its button is held to unmute it.
- **MUTE FADE OUT** (KNOB 2): how long it takes to fade away when its button is held to mute it.

Each OFF (at once, as before) to 60 s: half-second steps to 10 s, whole seconds above. Both OFF at power-on and in a
new session. The fade falls evenly in loudness; the generator's sends to its lane and to the master delay and reverb
fade with it. Muted or unmuted again halfway, it turns back from where it is. Kept in snapshots and sessions. A
snapshot recalled at once, or a session loaded, sets the mutes at once; a morph glides them over its own time.

The generator's button, or SHIFT + it again, leaves the page. Holding SHIFT with one generator's button and then
pressing another still links them (no page).

### DISINTEGRATOR

KNOB 1 while playing sets the DISINTEGRATOR of the generator the keys play, 0 to 100. It is an unstable gate after
the sound is made and before its cables, so effects hear it too.

It cuts the sound into pulses of uneven length. Each pulse lets the sound through, gates it down (to silence from 60
upwards), repeats a 10 to 90 ms slice of what was just played, or crushes it (bits and sample rate). A pulse may also
throw the dry part across the stereo image.

Higher values give shorter, more irregular pulses, more of them acting, deeper and wider. Every edge is smoothed over
about 1.5 ms, so no clicks. At 0 nothing is touched. Each generator keeps its own value in snapshots and sessions.

Ideas from Destroy FX's Skidder and from buffer repeaters; none of their code.

## FURNACE engine

FURNACE started as Tonitor's FM engine and was rebuilt around one goal: sound that is never struck, never still and
never exactly the same twice.

It works on one voice, one note and one block of 32 samples (0.73 ms at 44.1 kHz) at a time. Everything is integer
maths, because the chip has no floating point; per-sample code runs from RAM. Code: `firmware/src/mol.c` (voice and
draw), `fm6_core.c` (operators), `wt.c` (waves), `found_clips.h` (found sounds).

### Voice signal path

Every block goes through these stages, in this order:

1. **Hand** (generation 3). An invisible hand rests on each voice (after Anima: Tonitor's version, from Vascular).
   One finger drags figures across a surface: orbits, figures of eight, scribbles, slow glides, now and then a flick.
   It presses in slow swells and sometimes lifts to land elsewhere; a second "ghost" finger comes and goes. Finger
   position, pressure, speed, angle, distance and age are read once a block. Two to four of them, chosen when the
   sound is drawn, are mapped to brightness, cloud, level, grit, glitches and cloud darkness. Never to pitch.
2. **Operators.** Six-operator FM: msfa (Dexed's core) in Felucca's integer port, as Tonitor shaped it. DX7's 32
   algorithms, four-rate / four-level envelopes, keyboard scaling, ratio or fixed frequencies, feedback, LFO. Each
   operator reads a sine or one of 28 band-limited single cycles (four primitives, Bigorna's sixteen harmonic sets,
   eight made for bells). A wave can morph towards the next in its bank, and its phase can be bent as on the Casio CZ:
   a sine opens like a filter, a square's width follows its envelope. Each wave has eight levels of detail; one is
   picked every block from the operator's frequency, so the wave itself never aliases. Macros act here: BRIGHTNESS on
   modulator index, DETUNE (or INHARMONIC) on ratios, MOTION on LFO and tremolo. Finally a 7 Hz high-pass removes the
   DC that FM produces, and the sound's measured gain is applied.
3. **Exciter and body** (generation 4). Operator output, noise or a bow drives a body tuned to the note. **TUBE** is a
   waveguide: a delay line one period long, damped and low-passed inside its loop, with an allpass adding a wire's
   stiffness (partials pulled apart). Open, or closed at one end for odd partials only. **MODAL** is six resonators on
   the ratios of a bar, plate, bell, closed pipe or hull, slightly detuned. The bow is Smith's bowed-string model:
   friction that grips while string and bow move together and slips when they don't. A body rings 2.5 s at most and
   never bends pitch.
4. **Grit** (generation 3). Russolo's, after Robert Heel's Noisferatu. Three parts: a small DAC (bits dropped, each
   sample held several times); XOR of the sample with a triangle at a ratio of the note (logic oscillators); and
   stutters, looping the last 5 to 120 ms 3 to 12 times before carrying on. Mixed with the clean sound; the hand moves
   both mix and stutter chance.
5. **Grains.** Everything above is written continuously into the voice's own buffer: 12288 samples at 22.05 kHz,
   8-bit mu-law, about 0.56 s. Up to four grains at once read it back, in one of two ways. **CLOUD**: grains of 2 to
   400 ms at their own rate (1 to 400 per second), starting at a drifting point behind the write head; jittered in
   time and level, pitched by the sound's intervals, sometimes reversed, darkened by a low-pass, fed back into the
   buffer so the cloud smears on itself. When the sound asks for it, the cloud is **FROZEN**: the buffer stops
   recording a moment after note-on and the cloud keeps turning over what it holds. **PULSARS** (after Curtis Roads):
   grains locked to the note's period, half a grain to four per period. One grain per period reads the sound back as a
   formant; a burst mask drops some grains, adding a sub-harmonic. From generation 5, a share of grains reads a found
   sound instead (see below). GRAIN macro changes the cloud's share and rate, and gives a sound without a cloud the
   cloud drawn with it.
6. **Hand pressure** on level, last.

A voice ends when its note is released and it has been silent for a quarter of a second, or when operators, body and
cloud have nothing left. Separately, generators fade a released voice out within three seconds.

### Found sounds (generation 5)

Sixteen 0.6 s clips in flash, 8-bit like the grain buffers, each looping without a seam:

- Ice and sea, from NOAA hydrophones: iceberg calving, its tremor, icequake, grounding, ship above the Challenger Deep.
- Aurora radio emissions, from NASA's Polar spacecraft: auroral hiss (twice), kilometric radiation, chorus,
  "saucers".
- Plant, rendered here: transformer, air duct, furnace, rain on tin, engine room, wind in wires.

A grain can start anywhere in its clip. Playback rate follows the keys only by a drawn share, so clips are barely
transposed. DEPTHS, AURORA and PLANT take 70 to 100 % of their grains from a clip. Older sounds with a cloud get a
clip one time in three, for 30 to 60 % of their grains. Sources and credits: `found/CREDITS.md`.

### How sounds are drawn

A sound is drawn from four values: family, seed, path and generation. Same values, same sound, bit for bit. This is
why sessions store seeds, not sounds. Order of the draw:

1. **Family.** From the generator's scene and a harshness budget (HOME), or kept (SHIFT + HOME). Scene also sets the
   path: FIELD ambient, MATERIAL open, FEEDBACK and EROSION experimental.
2. **Body.** One of eight FM bodies. Newer families each borrow one and build on it.
3. **Family features.** Cloud, shattered grains, pulsars, freeze, grit and logic, exciter and body, or found sound.
4. **Path.** AMBIENT: slower and darker (longer attacks and releases, modulators down, longer cloud, an octave down
   one time in three). EXPERIMENTAL: harsher (modulators and feedback up, mutated more often than not). Open path:
   one sound in three mutated. A mutation fixes a partial to a frequency, turns an envelope upside down, pushes
   feedback, moves an octave, replaces a sine with another wave, or bends one.
5. **Care.** Modulators on high ratios turned down, index scaled down the keyboard, rate scaling on carriers.
6. **Generation 2 rule.** Every sound swells in, holds and keeps moving: slow LFO (0.1 to 1 Hz) on modulator levels,
   no pitch envelope, cloud fading in.
7. **Later generations.** Hand on most sounds (85 %). Grit on some older families on the experimental path. Body for
   one older sound in five; found sound for one older cloud in three.
8. **Sanitiser.** Every value forced into range.
9. **Measurement.** Sound rendered for 2.5 s at middle C, outside the audio path; its loudest 46 ms brought to a fixed
   level. Boost limited to +15.6 dB, so a slow swell stays a swell.

### Engine generations

A sound keeps the generation it was drawn under, and a generation never changes what it draws. Tests check this with
a hash of hundreds of sounds per generation. In practice, a session from an older build sounds exactly as it did.
New sounds are always drawn on the newest generation.

| Generation | Added |
|---|---|
| 0 | FM with waves in the operators, grains, twelve families |
| 1 | No pitch bends in bodies (no zaps, no bleeps), tonal ones roughened; no freeze before the body has sounded |
| 2 | Textures and drones only: IMPACT and PULSE no longer drawn; every sound swells in, holds and keeps moving |
| 3 | Hand, grit, eight families |
| 4 | Exciter and body, five families |
| 5 | Found sounds, three families |
| 6 | Eight families; one older sound in four gets other waves (harmonic sets, bells, bent) |

### Families

| Family | Gen. | What it is |
|---|---|---|
| METAL | 0 | Risset's bells and gongs: many carriers on inharmonic ratios, high partials dying first |
| DRONE | 0 | Slow pairs, near-harmonic, detuned to beat, wandering index |
| BRUIT | 0 | Noise from feedback and high ratios, scattered by a sample-and-hold LFO |
| IMPACT | 0 | Struck body, modulators dying first (not drawn from generation 2) |
| PULSE | 0 | Sound pulsed by its own LFO (not drawn from generation 2) |
| GLASS | 0 | Carriers on bell waves, morphed, barely modulated |
| FORMANT | 0 | CZ approach: carriers bent by their envelopes under a modulator at the fundamental |
| REED | 0 | Saw or square whose width follows its envelope, under a feedback operator |
| CLOUD | 0 | Body under a slow cloud: long grains, pitched, dark, smeared |
| SHATTER | 0 | Body broken into short grains: fast, jittered, reversed, scattered |
| PULSAR | 0 | Sustained body read back one grain per period: formants, sub-harmonics |
| FREEZE | 0 | Body frozen into a cloud a moment after it starts |
| FURNACE | 3 | Low and roaring, crushed, dark cloud over it |
| HUM | 3 | Machine room: carriers fixed near 50 and 100 Hz, beating, XOR'd |
| STATIC | 3 | Feedback noise, crushed hard, stuttering, shattered |
| HOWL | 3 | Formants an octave down under long, dark grains |
| SCRAPE | 3 | Scraping metal: high inharmonic partials, bent grains, held samples |
| DATA | 3 | Glass through bitwise logic, crushed, stuttering, bent pulsars |
| ASH | 3 | Dark drone under sparse, dusty crackle |
| TUNNEL | 3 | Huge dark cloud, fed back, sometimes frozen |
| WIRE | 4 | Stiff wire bowed by friction, operators as the bow arm |
| STEAM | 4 | Noise in a pipe closed at one end, hiss beside it |
| CHIMNEY | 4 | Wind in a long open tube, an octave down |
| GIRDER | 4 | Noise in a bar's or plate's modes |
| HULL | 4 | Operators droning in a bell's or hull's modes |
| DEPTHS | 5 | Ice breaking and grinding, ship far above: slow, dark |
| AURORA | 5 | Hiss and radio from above the ice: airy, drifting |
| PLANT | 5 | Hum, ducts, furnace, rain on tin, engines, wires |
| ORGAN | 6 | Organ in a ruin: six carriers on drawbar ratios, held; sometimes sounding in a pipe |
| CHOIR | 6 | Wordless voices: formant-like waves bent by their envelopes, widely detuned, soft cloud |
| FOGHORN | 6 | Saws and squares an octave down, fed back hard, in a closed tube |
| SWARM | 6 | Many high carriers slightly apart, beating: buzzing mass |
| CORONA | 6 | Bell waves high up, cloud pitched an octave and a twelfth above |
| SUBSTRATE | 6 | Squares and saws an octave down, bent; sometimes a pipe's modes |
| GLASSHARP | 6 | Noise drawn around a bell's modes an octave up, long |
| OXIDE | 6 | Corroded metal: bent waves, crushed bits, plant grains |

### Macros

Four macros, each -64 to 64. At 0 the sound is exactly as drawn. There is no page to set them by hand: the hand
moves them, and so do modulation routes (MOD page) aimed at them. Response follows the square root of
the knob's travel, so the first part of the turn does the most.

- **BRIGHTNESS**: modulator index about 27 dB either way, higher carriers tilted up, feedback with it.
- **DETUNE**: carriers pulled apart (up to 40 cents), modulators moved off their carriers. On metallic and noisy
  bodies it becomes **INHARMONIC**: every operator except the lowest carrier moved off its ratio.
- **GRAIN**: cloud wetter and faster, or drier and slower. On a sound without a cloud, fades in the cloud drawn with
  it.
- **MOTION**: LFO depths, a tremolo (1.5 to 6 Hz), grain drift.

Hand and modulation routes can move the macros at the same time.

## Effect slots

Twenty-two effects, two or three per slot. The last five came in build 32; their names are provisional. Delay and
plate reverb belong to the master only (since build 30). A session saved with them in a slot loads them as TAPE and
SPRING.

| Effect | What it is |
|---|---|
| FILTER | Driven state-variable filter: low-pass, band-pass, high-pass or notch. Resonant to the edge of screaming, never piercing |
| FOLD | Wavefolder, with a second fold and a tone control |
| MULAW | Rate and bit reduction, filter open enough to alias, drive into it |
| SHIFT | Frequency shifter: every partial moved by the same 40 to 900 Hz, tearing the harmony apart |
| MODAL | Three tuned resonators, spread inharmonically, ringing up to Q 325 |
| STRING | Three Karplus-Strong strings on the scale's root, ringing up to 8 s |
| VOWEL | Three formants morphing through vowels, driven |
| TAPE | Echo through a worn head: dropouts, saturation, age, hiss under the sound |
| REVERSE | Windows of the sound played backwards, fed back |
| SPRING | Spring tank, with its chirps |
| GLITCH | Russolo's bent buffers: jumps and stutters through the last 0.74 s, never at another pitch |
| CLOUD | Granular cloud that can freeze what it holds |
| SCREAM | Shaped feedback howl |
| RING | Diode ring modulator modulating itself |
| STARVE | Starved, gated fuzz (after the Fuzz Factory) |
| CHEB | Harmonic re-voicer using Chebyshev polynomials (after Airwindows' Pafnuty) |
| BANK | Inharmonic resonator bank: bar, plate, bell, membrane (after Mutable Instruments' Rings) |
| SMEAR | Sound through twenty random taps, each + or −, over 12 to 46 ms: grainy haze rather than a room (velvet-noise decorrelator) |
| DIFFUSE | Four allpasses in series (lengths from Mutable Instruments' Clouds): blurred edges, no tail of its own |
| SLEW | Limits how far the sound can move in one sample: highs turned into dark, grinding ramps (after Airwindows' Slew) |
| XOR | Sound combined bit by bit (XOR, AND, OR) with itself a moment earlier: broken and digital, but following its shape |
| DUST | Hiss and crackle where the sound moves, none where it is still (after Airwindows' TapeDust) |

Twenty-four **recipes** decide which effects may stand at each place: SPACE, ECHO, METAL, STRINGS, EROSION, GHOST,
SPECTRAL, RESONANT, FOLDS, TELEPHONE, HELL, FACTORY, GLITCH, BROADCAST, ABYSS, RUST, FREEZER, FEEDBACK, FOUNDRY, GONG,
SPECTRE, SPUTTER, SMOG, STATIC.

Every effect falls silent with nothing coming in. None sounds on by itself.

## Lanes

Generator 1 plays through lane 1 only, generator 2 through lane 2, generator 3 through lane 3. Each generator is
cabled into the head of its lane, with a small dry share. No cables between lanes. A lane has no pan of its own: it and every effect in it follow its generator's pan
(centre, left and right at power-on).

HOME on a generator also draws its lane's chain, so the chain is part of the generator's patch (SHIFT + HOME keeps
family and recipe). HOME on an effect slot redraws that lane and keeps its generator. REWIRE inside a lane redraws
where the generator enters and which effects also send to the master. Holding HOME draws all three generators and
their lanes.

From build 28 to 35 a session could also be MESH: any generator cabled into any effect, effects cabled across slots.
In practice it mostly produced smear, so from build 36 there are only lanes. A MESH session loads rewired into lanes,
snapshots included, nothing redrawn.

## Paths

Two black keys bias everything HOME draws, and RAND PARAMS too:

- **ABRASIVE** (2nd black key): gnarly, textural, torn. Favours experimental scenes; harsh families (BRUIT, SHATTER,
  FURNACE, STATIC, SCRAPE, DATA, METAL); more cables and cross-modulation; torn and crushed recipes; more drive,
  breakage and crushing in effects.
- **DARK** (3rd black key): dark ambient, drones, introspection, along the lines of Lustmord and the Cold Meat
  Industry catalogue. Favours the FIELD scene; drones, hums, tunnels, ash and freezes, nothing harsh; spacious and
  ringing recipes (SPACE, GHOST, ABYSS, FREEZER, RESONANT, GONG and others); darker, less driven values, larger
  spaces. Also 3 dB quieter.

With neither on, anything goes. Path is saved with the session and with each snapshot.

## Modulation

Every slot, generator or effect slot, has two LFOs and eight routes, A to H.

- LFO shapes: SINE, TRI, SAW UP, SAW DOWN, SQUARE, STEPS, DRIFT. Each LFO runs FREE, restarts on each NOTE, or runs
  ONCE, and has a fade-in.
- A route goes from a source (LFO 1, LFO 2, VELOCITY, KEY, RANDOM) to a destination, by an amount.
- Generator destinations: four macros, level, pan, sends, LFO rates.
- Effect slot destinations: effect values by place, level, LFO rates (a lane's pan is its generator's). Routes point at a place, not at an effect,
  so after HOME draws new effects the LFO moves the new effect's value at that place.

SEQ's third page sets them; SHIFT + SEQ draws them at random.

## Snapshots and sessions

**A snapshot is the whole machine.** Every slot's sound, effects, values and cables; levels, pans, mutes, mute fades and sends;
DISINTEGRATORs; FX MIX; master delay and reverb; modulation; scale and root; octave; path. It also stores how the
machine is being played: latched notes, whether each generator's loops are playing, which loops are taken out, their
direction and speed, VARY, and moves along the scale. Loop recordings themselves belong to the session. Sixteen
snapshots per session.

**SAVE** opens the SNAPSHOTS page. Release a white key to recall its snapshot. Hold it 0.8 s to store the current
state in it. Hold SHIFT as well to empty it.

KNOB 1 on this page sets **morph time**, 0 to 30 s. A recalled snapshot moves into place over that time. Levels, pans,
sends, values and LFOs glide. Sounds cross-fade on voices of their own; effect chains fade out and in, one after the
other. Anything without an in-between value switches halfway. The target snapshot's square and key blink until the
morph is done.

**SHIFT + SAVE** opens the SESSIONS list, as on Bigorna: four per page, PRESETS moves through 64 slots. Saved session
white, empty slot grey, loaded one marked on its right. SAVE opens the slot's page:

- **BACK**
- **LOAD**, or **NEW** on an empty slot (clean slate)
- **SAVE** (asks first before overwriting another session)
- **COPY** to a slot chosen with PRESETS. First empty slot offered; copying over a session asks first
- **DELETE** (asks first)

PRESETS moves through the rows, SAVE runs the selected one, SHIFT + SAVE goes back a level. While playing, holding
SAVE saves straight into the loaded session. A session holds the machine, its sixteen snapshots, latched notes and
every loop, in the FM-1's flash.

Norilsk only writes to blank flash or to its own retired sessions, never over another firmware's data. Other firmware
does not return the favour, however: back up your sessions before installing something else.

## Looper

Each generator has Tonitor's looper: up to ten free-running loops, each on its own clock. No tempo, no grid.

Press REC to arm a loop; the first note starts it. Press REC again to end it; it plays at once. Press REC again while
loops play to overdub. A note held across the end of a loop is tied over.

Hold PLAY to show loops on the white keys and take them out or bring them back. This also lights the 4th to 8th black
keys: REVERSE, PENDULUM, RANDOM, HALF SPEED, DOUBLE SPEED. SHIFT + OCT moves all loops along the scale; SHIFT + REC
turns on VARY.

## Backing up sessions

The website's [backup page](https://nzimas.github.io/norilsk/backup.html) saves every session on the FM-1 to a file
and writes them back, from desktop Chrome, Edge or Opera.

From a terminal: `tools/fm1_sessions.py` lists sessions, `tools/fm1_sessions.py save N` saves one,
`tools/fm1_sessions.py restore FILE` writes one back. Page and script only talk to an FM-1 whose USB name is FM-1
Norilsk.

## USB audio

Connected to a computer, the FM-1 is also a class-compliant USB audio interface: no driver needed. It sends the
stereo master at 16 bits, 48 kHz (or 44.1 kHz), taken before the MASTER knob. Recordings therefore don't follow the
headphone level. Code from Tonitor and Bigorna.

# Building from source

`./setup.sh` once: a Python environment, JieLi's toolchain and three files of the AC79 SDK (Tonitor's procedure). On
macOS, Docker Desktop must be running to build. Then:

```bash
./build.sh
```

It must print `package ... build N` with no FAIL. `./install.sh` builds and installs over USB; `./install.sh --no-build`
installs `build/norilsk.fwsc` as it is; `./install.sh FILE.fwsc` installs another package; `./install.sh --info` says
what the connected FM-1 runs.

## Running tests

```bash
tests/run_tests.sh
```

Needs `clang` and `python3` with Pillow. It checks the firmware unit for anything the chip's runtime lacks (no float,
no 64-bit division), renders thousands of sounds (none silent, none clipping, none running away), plays the panel
through the real firmware code, checks every effect falls silent, every recipe fits its slot's memory, snapshots and
morphs land exactly, every message fits the screen, sessions survive power cuts and never touch another firmware's flash, and that the engine's
generations still draw what they drew. Renders land in `build/sounds` (WAV), the screens in `build/screens` (PNG). The
website: `tools/make_logo.py`, `tools/make_demos.sh`, then `tools/make_site.py` (into `docs/`, served by GitHub Pages).

## Layout

| Path | What |
|---|---|
| `firmware/src/norilsk.c` | the single compilation unit: the sources in order |
| `firmware/hal/`, `crt0.S`, `app.ld`, `loader/` | the board layer, boot, memory map and update loader (Tonitor's, from Felucca and SLOOP) |
| `firmware/src/main.c`, `usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c` | boot and interrupts, USB, the installer's handshake, the rescue mode, the screen |
| `firmware/src/mol.c`, `fm6_core.c`, `wt.c` | FURNACE: the voice, the grains, the hand, the grit, the families; the FM core; the waves |
| `firmware/src/gens.c` | the three generators: the draw, the voices, the two cores, the DISINTEGRATOR, the mix |
| `firmware/src/rack.c`, `reverb.c` | the effect slots: the twenty-two effects, the recipes, the paths, the memory, the cables |
| `firmware/src/mod.c`, `loop.c`, `master.c` | the modulation; the loopers; the master delay and reverb |
| `firmware/src/sessions.c`, `backup.c` | sessions, snapshots and the morph; the sessions over USB |
| `firmware/src/ui.c` | the panel and the screen |
| `tools/` | the build, the installer, the sessions tool, the tables, fonts, logo, demos and website |
| `tests/host/host.c` | the host harness: every test named above, `render`, `one`, `replay` |
| `docs/` | the website (GitHub Pages); `docs/ENGINE.md` the engine's first design (0.01) |
| `site/` | the website's pages, before `tools/make_site.py` frames them |

# Licensing

The firmware is GPL-3.0-only (`LICENSE`); `fm6_core.c` is msfa under Apache-2.0 (`LICENSES/`); the website and logo
are MIT. See `LICENSING.md`.

# Disclaimer

Norilsk is unofficial. It is not made, endorsed or supported by M-VAVE. Installing it replaces the FM-1's firmware
and may void any warranty that applies to your device; an interrupted update can leave the FM-1 unable to start until
it is recovered. The software comes with no warranty of any kind; you install it entirely at your own risk. Norilsk is
named after the city; it has nothing to do with the company that runs its smelters.
