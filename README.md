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

State: **build 37**. Each build is described in `CHANGELOG.md`. Norilsk was called Molecular until build 17.

# Manual

## How it is put together

Norilsk has three generators, three effect slots and a master delay and plate reverb. I think of the generators as
furnaces: each one burns a sound. The effect slots are the pipes the sound goes through, and the delay and reverb are
the air the fumes end up in. In practice:

- Each generator plays through its own **lane** of effects: generator 1 through lane 1, 2 through 2, 3 through 3. A
  small part of each generator also goes straight to the master, dry.
- Each lane is a chain of two or three effects, drawn from a recipe. An effect before the last one can also send to
  the master on its own.
- The white keys play the generator you chose last, in the current scale. There are four voices, shared by the three
  generators.
- Each generator has a level, pan, mute, delay and reverb sends, a looper and a DISINTEGRATOR. Each effect slot has a
  level, pan and mute.
- One generator can modulate another (ring, amplitude or filter), but only if you link them by hand: hold SHIFT, hold
  one generator's button, then the other's.

The first processor core renders the voices. The second runs the effects and the master delay and reverb, one block
(0.73 ms) behind.

Every sound swells in and keeps moving. However, **nothing sounds on once no note is held, latched or looped**:
released voices fade out within three seconds, and the effect slots close two seconds after the last voice. The master
delay and reverb are the exception: their tails always ring out in full, whatever you let go or mute.

## What is random and what isn't

Almost nothing in Norilsk is set by hand at the start. It is drawn at random, and your job is to choose, keep, throw
again and play. Randomness works at every scale:

1. **The sound.** FURNACE, the engine, draws a sound from a family, a seed, a path and an engine generation. The same
   four values always give exactly the same sound, so a session only needs to store the seed.
2. **The note while it sounds.** An invisible hand (after Anima) keeps moving the four macros of every voice. Russolo's
   grit drops bits, crosses the sound with itself and makes it stutter. The grains are read from places that wander.
   In other words, the same key held twice never sounds the same twice.
3. **A generator** (HOME on a generator): a scene, a family chosen by how harsh it is allowed to be, which effects it
   is cabled into, and how much of it is heard dry.
4. **An effect slot** (HOME on an effect slot): one of 24 recipes. Each place in the recipe is filled with one of its
   allowed effects that fits the slot's memory, every value is drawn around its default, and the outs are drawn too.
   HOME on a generator also draws its lane's chain.
5. **The paths.** ABRASIVE and DARK bias every draw above: scenes, families, recipes and effect values.
6. **Partial draws.** REWIRE draws only the cables. RAND PARAMS draws only an effect slot's values, or a new sound of
   the same family for a generator. SHIFT + HOME keeps the family or the recipe. Holding HOME draws all six slots.
7. **Modulation.** SHIFT + SEQ draws a slot's two LFOs and two to six of its eight routes. The STEPS and DRIFT shapes
   and the RANDOM source keep drawing new values as they play.
8. **Playing.** The DISINTEGRATOR gates, stutters, crushes and pans each generator in pulses of random length. The
   looper's VARY changes every note it plays, and RANDOM jumps between loops.
9. **What isn't random.** A snapshot stores the whole machine and recalls it exactly, or morphs into it. Sessions keep
   everything in flash.

## The panel

SHIFT is the rightmost black key.

| Control | What it does |
|---|---|
| **FX SEL ENV** | Generators 1 to 3. Press to choose one (the keys play it). Hold 0.8 s to mute it or unmute it |
| **LFO EDIT GLO** | Lanes 1 to 3 (the effect slots). Press to choose one. Hold 0.8 s to mute it or unmute it |
| **HOME** | Draws a new patch for the chosen slot. On a generator: its sound and its cables into the effects. On an effect slot: its recipe, effects, values and cables |
| **SHIFT + HOME** | A new patch of the same family, or with the same recipe |
| **HOME** held 0.8 s | All six slots drawn again |
| White keys | Play the chosen generator in the current scale. With an effect slot chosen they do nothing and show nothing |
| Hold a white key, press **SHIFT** | Latches the note (the key stays lit). To release it, hold **SHIFT** and press the lit key |
| **SELECT / ALGORITHM** | Level / pan of the chosen generator (or the one chosen last). At power-on: 1 centre, 2 left, 3 right |
| **OCT− / OCT+** | Octave, −3 to +3 |
| **ARP** | Opens the SCALE page: SELECT sets the root, ALGORITHM the scale (23 of them). **SHIFT + ARP** draws a random root and scale and shows them briefly. A root and scale are also drawn at power-on and in a new session |
| **KNOB 1** | While playing: the DISINTEGRATOR of the generator the keys play, 0 to 100. On the SNAPSHOTS page: morph time, 0 to 30 s |
| **SHIFT + KNOB 1** | While playing: the master DRIVE, 0 (off) to 100. A rough, valve-like overdrive over everything, with its level compensated |
| **KNOB 2** | While playing: FX MIX of the chosen generator's lane (or of the chosen lane), 0 (dry) to 100 (wet). 50 at power-on |
| **KNOB 3 / KNOB 4** | Delay send / reverb send of the generator the keys play |
| **SEQ** | Steps through the DELAY, REVERB and MOD pages and back. KNOB 1 to 4 set the page's four rows |
| **SHIFT + SEQ** | Draws new modulation for the chosen slot |
| **SHIFT**, hold a generator's button, then hold another's | LINK: the first generator modulates the second (RING, AM or FILTER, drawn at random). The screen shows it in two lines, and the second generator's block gets a band at its foot in the first one's colour. Do it again to unlink (OFF) |
| 1st black key | REWIRE the chosen slot: new cables for a generator, or new outs and cables for an effect slot. Sound and values stay |
| **SHIFT** + 1st black key | RAND PARAMS: every value of an effect slot drawn again, without a break (effects and cables stay); or, on a generator, a new sound of the same family (cables stay) |
| 2nd black key | ABRASIVE path on or off (lit while on) |
| 3rd black key | DARK path on or off (lit while on). Only one path can be on; with neither, anything goes |
| **SAVE** | Opens the SNAPSHOTS page (press again to go back) |
| **SAVE** held (while playing) | Saves into the session that is loaded or chosen |
| **SHIFT + SAVE** | Opens the SESSIONS list. SAVE opens a session's page: BACK, LOAD, SAVE, COPY, DELETE |
| **REC** | Looper of the generator the keys play. Press to arm a loop (the first note starts it), press again to end it (it plays at once), press again to overdub. Up to 10 loops |
| **REC** held 1 s | Clears that generator's loops |
| **SHIFT + REC** | VARY: every note the loops play is changed a little (level, length, sometimes swapped or joined) |
| **PLAY** | Loops only. Stops the chosen generator's loops, or restarts them from the beginning. Dim when it has loops, lit while they play, dark when there are none. With an effect slot chosen it is dark and does nothing |
| **PLAY** held | Shows its loops on the white keys: press one to take it out or bring it back |
| **PLAY** held + 4th to 8th black keys | Loop modes: REVERSE, PENDULUM, RANDOM, HALF SPEED, DOUBLE SPEED (press again for normal). The keys glow dimly while PLAY is held and are lit while on |
| **SHIFT + PLAY** | Stops every generator's loops, or starts them all together |
| **SHIFT + OCT− / OCT+** | Moves every loop one step of the scale down / up (hold a generator's button too for only its loops) |
| **MASTER** | Output level |
| USB MIDI notes | Same as the keys, on the generator chosen last |
| **OCT−** held at power-on | Rescue mode (install again). **OCT− and OCT+** held 5 s: the chip's own updater |

The black keys are contextual. A key that has no job in the current view does nothing there and stays dark.

## The screen

The screen is designed for low vision: few words, very large type, high contrast.

The main view is drawn as a place. At the top there is slowly drifting smog, with the audio load on the left and the
battery on the right. Below ground, the three effect slots are rusty pipes, one above the other. Each effect is a
section between bolted flanges with its name on a dark plate: FLT filter, FLD fold, MU mulaw, SHF shift, MDL modal,
STR string, VOX vowel, TAP tape, RVS reverse, SPR spring, GLT glitch, CLD cloud, SCR scream, RNG ring, STV starve, CHB
cheb, BNK bank, SMR smear, DIF diffuse, SLW slew, XOR xor, DST dust.

Under each name there is a band in the colours of the generators feeding it. Plates fed by the chosen generator are
filled with its colour. A section glows hotter while sound passes through it, and a muted pipe is cold grey.

At the bottom, the three generators are brick furnaces with fire in their mouths in their colours (red, blue, green).
The fire is brighter while they sound and dark when they are muted. The chosen slot has a white frame. Cables are not
drawn. KNOB 1 shows its value on a dial that covers the whole screen.

## The generators

Each generator runs an instance of **FURNACE**, the engine, playing one sound drawn from thirty-six families. The
four voices are shared by the three generators, and each voice is a complete instance of the engine. Because of this,
the same sound on two keys never sounds quite the same. The engine is described in detail in
[FURNACE: the engine](#furnace-the-engine) below.

### The DISINTEGRATOR

KNOB 1 while playing sets the DISINTEGRATOR of the generator the keys play, 0 to 100. It is an unstable gate placed
after the sound is made and before its cables, so the effects hear it too.

It cuts the sound into pulses of uneven length. Each pulse either lets the sound through, gates it down (to silence
from 60 upwards), repeats a 10 to 90 ms slice of what was just played, or crushes it (bits and sample rate). A pulse
may also throw the dry part across the stereo image.

The higher the value, the shorter and more irregular the pulses, and the more of them act, more deeply and more
widely. Every edge is smoothed over about 1.5 ms, so there are no clicks. At 0 nothing is touched. Each generator
keeps its own value in snapshots and sessions.

The ideas come from Destroy FX's Skidder and from buffer repeaters. None of their code is used.

## FURNACE: the engine

FURNACE is the engine in each generator. It started as Tonitor's FM engine and was rebuilt around one goal: a sound
that is never struck, never still and never exactly the same twice.

It works on one voice, one note and one block of 32 samples (0.73 ms at 44.1 kHz) at a time. Everything is integer
maths, because the chip has no floating point, and the per-sample code runs from RAM. The code is in
`firmware/src/mol.c` (the voice and the draw), `fm6_core.c` (the operators), `wt.c` (the waves) and `found_clips.h`
(the found sounds).

### A voice, stage by stage

Every block goes through these stages in this order:

1. **The hand** (generation 3). An invisible hand rests on each voice (after Anima: Tonitor's version, from
   Vascular). One finger drags figures across a surface: orbits, figures of eight, scribbles, slow glides and the
   occasional flick. It presses in slow swells and sometimes lifts to land somewhere else, and a second "ghost" finger
   comes and goes. The fingers' position, pressure, speed, angle, distance and age are read once a block. Two to four
   of them, chosen when the sound is drawn, are mapped onto the sound's brightness, cloud, level, grit, glitches and
   cloud darkness. The hand never touches pitch.
2. **The operators.** Six-operator FM: msfa (Dexed's core) in Felucca's integer port, as Tonitor shaped it. That means
   the DX7's 32 algorithms, four-rate / four-level envelopes, keyboard scaling, ratio or fixed frequencies, feedback
   and an LFO. Each operator reads a sine or one of 28 band-limited single cycles (four primitives, Bigorna's sixteen
   harmonic sets and eight made for bells). The wave can be morphed towards the next one in its bank, and its phase can
   be bent as on the Casio CZ, so a sine opens like a filter and a square's width follows its envelope. Each wave has
   eight levels of detail, and one is picked every block from the operator's frequency, so the wave itself never
   aliases. The macros act here: BRIGHTNESS on the modulators' index, DETUNE (or INHARMONIC) on the ratios, MOTION on
   the LFO and a tremolo. Finally a 7 Hz high-pass removes the DC that FM produces, and the sound's measured gain is
   applied.
3. **The exciter and the body** (generation 4). The operators' sound, or noise, or a bow, drives a body tuned to the
   note. A **TUBE** is a waveguide: a delay line one period long, damped and low-passed inside its loop, with an
   allpass that adds a wire's stiffness (the partials are pulled apart). It can be open, or closed at one end, which
   leaves only odd partials. A **MODAL** body is six resonators tuned to the ratios of a bar, a plate, a bell, a closed
   pipe or a hull, slightly detuned. The bow is Smith's bowed string model: friction that grips while the string moves
   with the bow and slips when it doesn't. A body rings for 2.5 s at most and never bends the pitch.
4. **The grit** (generation 3). Russolo's grit, after Robert Heel's Noisferatu. It has three parts: a small DAC (bits
   dropped, each sample held several times), XOR of the sample with a triangle at a ratio of the note (logic
   oscillators), and stutters, which loop the last 5 to 120 ms of the sound 3 to 12 times and then carry on. The grit is
   mixed with the clean sound, and the hand moves both the mix and the chance of a stutter.
5. **The grains.** All of the above is written continuously into the voice's own buffer: 12288 samples at 22.05 kHz in
   8-bit mu-law, about 0.56 s. Up to four grains at once read it back, in one of two ways. A **CLOUD** plays grains of 2
   to 400 ms at its own rate (1 to 400 per second), starting at a drifting point behind the write head. The grains are
   jittered in time and level, pitched by the sound's intervals, sometimes reversed, darkened by a low-pass and fed back
   into the buffer, so the cloud smears on itself. When the sound asks for it, the cloud is **FROZEN**: the buffer
   stops recording a moment after the note starts, and the cloud keeps turning over what it holds. **PULSARS** (after
   Curtis Roads) are grains locked to the note's period, from half a grain to four grains per period. One grain per
   period reads the sound back as a formant, and a burst mask drops some grains, which adds a sub-harmonic. From
   generation 5, a share of the grains reads a found sound instead (see below). The GRAIN macro changes the cloud's
   share and rate, and gives a sound without a cloud the cloud drawn with it.
6. **The hand's pressure** on the level, last.

A voice ends when its note has been released and it has been silent for a quarter of a second, or when its operators,
body and cloud have nothing left to play. Separately, the generators fade a released voice out within three seconds.

### The found sounds (generation 5)

There are sixteen clips of 0.6 s in flash, stored in 8 bits like the grain buffers, each looping without a seam:

- Ice and the sea, from NOAA's hydrophones: an iceberg calving, its tremor, an icequake, a grounding, and a ship
  above the Challenger Deep.
- The aurora's radio emissions, from NASA's Polar spacecraft: auroral hiss (twice), kilometric radiation, chorus and
  "saucers".
- A plant, rendered here: a transformer, an air duct, a furnace, rain on tin, an engine room and wind in wires.

A grain can start anywhere in its clip. Its playback rate follows the keys only by a drawn share, so the clip is
barely transposed. DEPTHS, AURORA and PLANT take 70 to 100 % of their grains from a clip. Older sounds with a cloud
get a clip one time in three, for 30 to 60 % of their grains. Sources and credits are in `found/CREDITS.md`.

### How a sound is drawn

A sound is drawn from four values: a family, a seed, a path and a generation. The same four values always give the
same sound, bit for bit. This is why a session stores the seed and not the sound itself. The draw goes like this:

1. **Family.** Chosen from the generator's scene and a harshness budget (HOME), or kept (SHIFT + HOME). The scene also
   sets the draw's path: FIELD uses the ambient path, MATERIAL the open one, FEEDBACK and EROSION the experimental
   one.
2. **Body.** One of eight FM bodies. The newer families each borrow one and build on it.
3. **The family's own features**: a cloud, shattered grains, pulsars, a freeze, grit and logic, an exciter and body,
   or a found sound.
4. **The path.** AMBIENT makes the sound slower and darker: longer attacks and releases, lower modulators, a longer
   cloud, and one time in three an octave down. EXPERIMENTAL makes it harsher: modulators and feedback up, mutated
   more often than not. On the open path one sound in three is mutated. A mutation fixes a partial to a frequency,
   turns an envelope upside down, pushes the feedback, moves an octave, replaces a sine with another wave, or bends
   one.
5. **Care.** Modulators on high ratios are turned down, the index is scaled down the keyboard, and the carriers get
   rate scaling.
6. **Generation 2's rule.** Every sound swells in, holds and keeps moving: a slow LFO (0.1 to 1 Hz) on the modulators'
   levels, no pitch envelope, and a cloud that fades in.
7. **Later generations.** Most sounds (85 %) get the hand. Some older families get grit on the experimental path. One
   older sound in five gets a body, and one older cloud in three gets a found sound.
8. **Sanitiser.** Every value is forced into its range.
9. **Measurement.** The sound is rendered for 2.5 s at middle C, outside the audio path, and its loudest 46 ms are
   brought to a fixed level. The boost is limited to +15.6 dB, so a slow swell stays a swell.

### The generations

A sound keeps the generation it was drawn under, and a generation never changes what it draws. The tests check this
with a hash of hundreds of sounds for each generation. In practical terms, a session from an older build sounds
exactly as it did. New sounds are always drawn on the newest generation.

| Generation | What it added |
|---|---|
| 0 | FM with waves in the operators, the grains, twelve families |
| 1 | No pitch bends in the bodies (no zaps, no bleeps), the tonal ones roughened; no freeze before the body has sounded |
| 2 | Textures and drones only: IMPACT and PULSE are no longer drawn; every sound swells in, holds and keeps moving |
| 3 | The hand, the grit, eight families |
| 4 | The exciter and the body, five families |
| 5 | The found sounds, three families |
| 6 | Eight families; one older sound in four gets other waves (harmonic sets, bells, bent) |

### The families

| Family | Gen. | What it is |
|---|---|---|
| METAL | 0 | Risset's bells and gongs: many carriers on inharmonic ratios, the high partials dying first |
| DRONE | 0 | Slow pairs, near-harmonic, detuned to beat, with a wandering index |
| BRUIT | 0 | Noise from feedback and high ratios, scattered by a sample-and-hold LFO |
| IMPACT | 0 | A struck body, its modulators dying first (not drawn from generation 2) |
| PULSE | 0 | A sound pulsed by its own LFO (not drawn from generation 2) |
| GLASS | 0 | Carriers on the bell waves, morphed, barely modulated |
| FORMANT | 0 | The CZ approach: carriers bent by their envelopes under a modulator at the fundamental |
| REED | 0 | A saw or square whose width follows its envelope, under a feedback operator |
| CLOUD | 0 | A body under a slow cloud: long grains, pitched, dark, smeared |
| SHATTER | 0 | A body broken into short grains: fast, jittered, reversed, scattered |
| PULSAR | 0 | A sustained body read back one grain per period: formants, sub-harmonics |
| FREEZE | 0 | A body frozen into a cloud a moment after it starts |
| FURNACE | 3 | Low and roaring, crushed, with a dark cloud over it |
| HUM | 3 | A machine room: carriers fixed near 50 and 100 Hz, beating, XOR'd |
| STATIC | 3 | Noise from feedback, crushed hard, stuttering, shattered |
| HOWL | 3 | Formants an octave down under long, dark grains |
| SCRAPE | 3 | Scraping metal: high inharmonic partials, bent grains, held samples |
| DATA | 3 | Glass through bitwise logic, crushed, stuttering, with bent pulsars |
| ASH | 3 | A dark drone under a sparse, dusty crackle |
| TUNNEL | 3 | A huge dark cloud, fed back, sometimes frozen |
| WIRE | 4 | A stiff wire bowed by friction, the operators acting as the bow arm |
| STEAM | 4 | Noise in a pipe closed at one end, with hiss beside it |
| CHIMNEY | 4 | Wind in a long open tube, an octave down |
| GIRDER | 4 | Noise in a bar's or a plate's modes |
| HULL | 4 | The operators droning in a bell's or a hull's modes |
| DEPTHS | 5 | Ice breaking and grinding, a ship far above: slow and dark |
| AURORA | 5 | Hiss and radio from above the ice: airy, drifting |
| PLANT | 5 | Hum, ducts, a furnace, rain on tin, engines, wires |
| ORGAN | 6 | An organ in a ruin: six carriers on drawbar ratios, held; sometimes sounding in a pipe |
| CHOIR | 6 | Voices without words: formant-like waves bent by their envelopes, widely detuned, a soft cloud |
| FOGHORN | 6 | Saws and squares an octave down, fed back hard, in a closed tube |
| SWARM | 6 | Many high carriers slightly apart, beating: a buzzing mass |
| CORONA | 6 | Bell waves high up, with a cloud pitched an octave and a twelfth above |
| SUBSTRATE | 6 | Squares and saws an octave down, bent; sometimes a pipe's modes |
| GLASSHARP | 6 | Noise drawn around a bell's modes an octave up, long |
| OXIDE | 6 | Corroded metal: bent waves, crushed bits, grains of the plant |

### The macros

There are four macros on the EDIT page, each from -64 to 64. At 0 you hear the sound exactly as drawn. The response
is the square root of the knob's travel, so the first part of the turn has the most effect.

- **BRIGHTNESS** moves the modulators' index about 27 dB either way, tilts the higher carriers up and raises the
  feedback with it.
- **DETUNE** pulls the carriers apart (up to 40 cents) and moves the modulators off their carriers. On metallic and
  noisy bodies it becomes **INHARMONIC**, which moves every operator except the lowest carrier off its ratio.
- **GRAIN** makes the cloud wetter and faster, or drier and slower. On a sound without a cloud, it fades in the cloud
  drawn with that sound.
- **MOTION** sets the LFO's depths, adds a tremolo (1.5 to 6 Hz) and moves the grains' drift.

The hand, the LFOs (SEQ) and you can all move the macros at the same time.

## The effect slots

There are twenty-two effects, two or three to a slot. The last five were added in build 32, and their names are
provisional. The delay and plate reverb belong to the master only (since build 30). If a session was saved with them
in a slot, they load as TAPE and SPRING.

| Effect | What it is |
|---|---|
| FILTER | A driven state-variable filter: low-pass, band-pass, high-pass or notch. Resonant up to the edge of screaming, never piercing |
| FOLD | A wavefolder, with a second fold and a tone control |
| MULAW | Rate and bit reduction, with the filter open enough to alias and a drive into it |
| SHIFT | A frequency shifter: every partial moved by the same 40 to 900 Hz, which tears the harmony apart |
| MODAL | Three tuned resonators, spread inharmonically, ringing up to Q 325 |
| STRING | Three Karplus-Strong strings on the scale's root, ringing up to 8 s |
| VOWEL | Three formants morphing through the vowels, driven |
| TAPE | An echo through a worn head: dropouts, saturation, age, hiss under the sound |
| REVERSE | Windows of the sound played backwards, fed back |
| SPRING | A spring tank, with its chirps |
| GLITCH | Russolo's bent buffers: jumps and stutters through the last 0.74 s, never at another pitch |
| CLOUD | A granular cloud that can freeze what it holds |
| SCREAM | A shaped feedback howl |
| RING | A diode ring modulator that modulates itself |
| STARVE | A starved, gated fuzz (after the Fuzz Factory) |
| CHEB | A harmonic re-voicer using Chebyshev polynomials (after Airwindows' Pafnuty) |
| BANK | An inharmonic resonator bank: a bar, a plate, a bell, a membrane (after Mutable Instruments' Rings) |
| SMEAR | The sound through twenty random taps, each + or −, over 12 to 46 ms: a grainy haze rather than a room (a velvet-noise decorrelator) |
| DIFFUSE | Four allpasses in series (with the lengths from Mutable Instruments' Clouds): blurred edges, no tail of its own |
| SLEW | Limits how far the sound can move in one sample, turning the highs into dark, grinding ramps (after Airwindows' Slew) |
| XOR | The sound combined bit by bit (XOR, AND, OR) with itself a moment earlier: broken and digital, but following its shape |
| DUST | Hiss and crackle where the sound moves, none where it is still (after Airwindows' TapeDust) |

Twenty-four **recipes** decide which effects may stand at each place: SPACE, ECHO, METAL, STRINGS, EROSION, GHOST,
SPECTRAL, RESONANT, FOLDS, TELEPHONE, HELL, FACTORY, GLITCH, BROADCAST, ABYSS, RUST, FREEZER, FEEDBACK, FOUNDRY, GONG,
SPECTRE, SPUTTER, SMOG, STATIC.

Every effect falls silent when nothing comes in. None of them sounds on by itself.

## The lanes

Generator 1 plays through lane 1 only, generator 2 through lane 2, generator 3 through lane 3. Each generator is
cabled into the head of its lane, with a small dry share. There are no cables between lanes. The lanes sit centre,
left and right.

HOME on a generator also draws its lane's chain, so the chain is part of the generator's patch (SHIFT + HOME keeps the
family and the recipe). HOME on an effect slot draws that lane again and keeps its generator. REWIRE inside a lane
redraws where the generator enters and which effects also send to the master. Holding HOME draws all three
generators and their lanes.

From build 28 to 35 a session could also be MESH: any generator cabled into any effect, and effects cabled across
slots. In practice it mostly produced smear, so from build 36 there are only lanes. A MESH session loads rewired into
lanes, snapshots included, with nothing redrawn.

## The paths

Two black keys bias everything HOME draws, and RAND PARAMS too:

- **ABRASIVE** (2nd black key): gnarly, textural, torn. It favours the experimental scenes; the harsh families
  (BRUIT, SHATTER, FURNACE, STATIC, SCRAPE, DATA, METAL); more cables and cross-modulation; the torn and crushed
  recipes; and higher drive, breakage and crushing in the effects.
- **DARK** (3rd black key): dark ambient, drones, introspection, along the lines of Lustmord and the Cold Meat
  Industry catalogue. It favours the FIELD scene; drones, hums, tunnels, ash and freezes, nothing harsh; the spacious
  and ringing recipes (SPACE, GHOST, ABYSS, FREEZER, RESONANT, GONG and others); and darker, less driven values with
  larger spaces. It is also 3 dB quieter.

With neither on, anything goes. The path is saved with the session and with each snapshot.

## Modulation

Every slot, generator or effect slot, has two LFOs and eight routes, A to H.

- The LFO shapes are SINE, TRI, SAW UP, SAW DOWN, SQUARE, STEPS and DRIFT. Each LFO runs FREE, restarts on each NOTE,
  or runs ONCE, and has a fade-in.
- A route goes from a source (LFO 1, LFO 2, VELOCITY, KEY, RANDOM) to a destination, by an amount.
- A generator's destinations are its four macros, level, pan, sends and the LFOs' rates.
- An effect slot's destinations are the values of its effects by place, its level, pan and the LFOs' rates. Because
  routes point at a place and not at an effect, after HOME draws new effects the LFO moves the new effect's value at
  that place.

SEQ's third page sets them, and SHIFT + SEQ draws them at random.

## Snapshots and sessions

**A snapshot is the whole machine.** It stores every slot's sound, effects, values and cables; levels, pans, mutes and
sends; the DISINTEGRATORs; the FX MIX; the master delay and reverb; the modulation; the scale and root; the octave;
and the path. It also stores how the machine is being played: the latched notes, whether each generator's loops are
playing, which loops are taken out, their direction and speed, VARY, and moves along the scale. The loop recordings
themselves belong to the session. There are sixteen snapshots in a session.

**SAVE** opens the SNAPSHOTS page. Release a white key to recall its snapshot. Hold it 0.8 s to store the current
state in it. Hold SHIFT as well to empty it.

KNOB 1 on this page sets the **morph time**, 0 to 30 s. A recalled snapshot moves into place over that time. Levels,
pans, sends, values and LFOs glide. Sounds cross-fade on voices of their own, and effect chains are faded out and in,
one after the other. Anything that has no in-between value switches halfway through. The square and key of the target
snapshot blink until the morph is finished.

**SHIFT + SAVE** opens the SESSIONS list, as on Bigorna: four to a page, and PRESETS moves through the 64 slots. A
saved session is white, an empty slot grey, and the loaded one is marked on its right. SAVE opens that slot's page:

- **BACK**
- **LOAD**, or **NEW** on an empty slot (a clean slate)
- **SAVE** (asks first if it would overwrite another session)
- **COPY** to a slot chosen with PRESETS. The first empty slot is offered; copying over a session asks first
- **DELETE** (asks first)

PRESETS moves through the rows, SAVE runs the selected one, and SHIFT + SAVE goes back a level. While playing, holding
SAVE saves straight into the loaded session. A session holds the machine, its sixteen snapshots, the latched notes and
every loop, in the FM-1's flash.

Norilsk only writes to blank flash or to its own retired sessions, never over another firmware's data. However, other
firmware does not return the favour, so back up your sessions before installing something else.

## The looper

Each generator has Tonitor's looper: up to ten free-running loops, each on its own clock. There is no tempo and no
grid.

Press REC to arm a loop; the first note starts it. Press REC again to end it, and it plays at once. Press REC again
while the loops play to overdub. A note held across the end of a loop is tied over.

Holding PLAY shows the loops on the white keys, so you can take them out or bring them back. It also lights the 4th
to 8th black keys: REVERSE, PENDULUM, RANDOM, HALF SPEED, DOUBLE SPEED. SHIFT + OCT moves every loop along the scale,
and SHIFT + REC turns on VARY.

## Backing up sessions

The website's [backup page](https://nzimas.github.io/norilsk/backup.html) saves every session on the FM-1 to a file
and writes them back, from desktop Chrome, Edge or Opera.

From a terminal, `tools/fm1_sessions.py` lists the sessions, `tools/fm1_sessions.py save N` saves one and
`tools/fm1_sessions.py restore FILE` writes one back. Both the page and the script only talk to an FM-1 whose USB name
is FM-1 Norilsk.

## USB audio

Connected to a computer, the FM-1 is also a class-compliant USB audio interface, so no driver is needed. It sends the
stereo master at 16 bits, 48 kHz (or 44.1 kHz), taken before the MASTER knob. This means a recording does not follow
the headphone level. The code comes from Tonitor and Bigorna.

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
