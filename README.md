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

State: **build 36**. Each build is described in `CHANGELOG.md`. Norilsk was called Molecular until build 17.

# Manual

## How it is put together

Think of the city it is named after. Each of the three generators is a **furnace**: it burns a sound, and the sound
never stops changing as it burns. The furnaces feed the **pipes** -- the effect slots, where the sound is crushed,
corroded and smeared on its way through -- and out of the pipes the **fumes** pour over the city: the master delay and
plate reverb, the air where everything hangs before it thins out. In plain terms:

Three **generators** make sound; three **effect slots** wreck it; a master **delay** and **plate reverb** close the
chain. The white keys play the generator chosen last, in the scale chosen; four voices are shared between the three
generators.

- Each generator runs through its own **lane**: generator 1 through effect slot 4, 2 through 5, 3 through 6, with a
  small dry share of its own straight to the master. One generator may modulate another (ring, amplitude, filter) --
  only when you link them: SHIFT, hold one generator's button, then the other's.
- Each effect slot is a chain of two or three effects, drawn from a recipe; an effect before the last may also go out
  to the master on its own. **KNOB 2** sets how much of a lane's effects is heard: FX MIX, dry to wet, 50 at
  power-on -- the chosen generator's lane, or the lane chosen.
- Each generator has its level, pan, mute, delay and reverb sends, a looper, and a DISINTEGRATOR; each effect slot its
  level, pan and mute.
- The first processor core renders the voices; the second runs the effects and the master delay and reverb, a block
  behind.

Every sound swells in and keeps moving, and **nothing sounds on once no note is held, latched or looped**: released
voices fade out within three seconds and the effect slots close two seconds after the last voice -- while the master
delay's and reverb's tails ring out whole, whatever is let go or muted.

## Randomness, level by level

Almost nothing in Norilsk is set by hand at the start: it is drawn. What you do is choose, keep, throw again, and play.

1. **The sound.** FURNACE, the engine, draws a sound from a family, a seed, a path and an engine generation, bit for
   bit the same every time: a sound is kept as its seed, and a session brings it back exactly.
2. **The note in motion.** In every voice an invisible hand (after Anima) keeps drifting the four macros; Russolo's grit
   cuts bits, crosses the sound with itself and stutters it from points of its own; the grains are taken from places
   that wander. The same key held twice never sounds the same twice.
3. **The generator** (HOME on a generator): a scene, a family weighed by how harsh it may be, how many effects it is
   cabled into and where, its dry share.
4. **The effect slot** (HOME on an effect slot): one of 24 recipes, each place filled from the recipe's choices that
   fit the slot's memory, every value drawn around its home, the outs. HOME on a generator draws its lane's chain with
   it.
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
| **ARP** | the SCALE page: SELECT the root, ALGORITHM the scale (23). **SHIFT + ARP**: a root and a scale drawn at random, shown a moment. At power-on and in a new session the root and scale are drawn too |
| **KNOB 1** | playing: the DISINTEGRATOR of the generator the keys play, 0 to 100. On the SNAPSHOTS page: the morph time, 0 to 30 s |
| **KNOB 2** | playing: FX MIX of the chosen generator's lane (or of the lane chosen), 0 (dry) to 100 (wet); 50 at power-on |
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
| **SHIFT + SAVE** | the SESSIONS list; SAVE opens a session's page: BACK, LOAD, SAVE, COPY, DELETE |
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

Made for low vision: few words, very large, high contrast. The main view is a place. At the top, smog, drifting
slowly, the audio's load at the left and the battery at the right on black. Under the ground, the three effect slots
as rusty pipes, one above another, each effect a section between bolted flanges, its name on a dark plate: FLT filter,
FLD fold, MU mulaw, SHF shift, MDL modal, STR string, VOX vowel, TAP tape, RVS reverse, SPR spring, GLT glitch, CLD
cloud, SCR scream, RNG ring, STV starve, CHB cheb, BNK bank, SMR smear, DIF diffuse, SLW slew, XOR xor, DST dust. Under
each name a band in the
colours of the generators feeding it; the plates the chosen generator feeds are filled with its colour; a section
glows hotter as sound passes through it; a muted pipe is cold grey. At the foot, the three generators as brick
furnaces, the fire in the mouth in their colours (red, blue, green), brighter as they sound, dark when muted. The chosen
slot is framed in white. No cables are drawn. KNOB 1 shows its value on a dial over the whole screen.

## The generators

Each generator is a furnace: an instance of **FURNACE**, the engine, burning one sound drawn from twenty-eight
families. The four voices are shared between the three generators, and each voice is a whole furnace of its own: the
same sound on two keys burns twice, never quite alike. How the engine is built: [FURNACE: the engine](#furnace-the-engine)
below.

### The DISINTEGRATOR

KNOB 1 while playing: an unstable gate on the generator the keys play, 0 to 100, after its sound is made and before its
cables, so the effects hear it too. The sound goes as a chain of pulses of uneven length. A pulse lets it through,
gates it down (to nothing from 60 up), stutters a slice of what it has just made (10 to 90 ms, repeated), or crushes
it (bits and rate), and may throw its dry part across the stereo image. The higher, the shorter and less even the
pulses, the more of them act, the deeper and wider. Every edge glides over about 1.5 ms: no clicks. At 0 nothing is
touched. Each generator keeps its own, in snapshots and sessions. (Ideas from Destroy FX's Skidder and from buffer
repeaters; none of their code.)

## FURNACE: the engine

FURNACE is what burns in each generator: Tonitor's FM engine taken apart and rebuilt around one idea, a sound that is
never struck, never still and never quite the same twice. It works one voice, one note and one block of 32 samples
(0.73 ms at 44.1 kHz) at a time, in integers only (the chip has no floating point), its per-sample code run from RAM.
The code: `firmware/src/mol.c` (the voice and the draw), `fm6_core.c` (the operators), `wt.c` (the waves),
`found_clips.h` (the found sounds).

### A voice, stage by stage

Every block, in this order:

1. **The hand** (generation 3). An invisible hand rests on the voice (after Anima: Tonitor's, from Vascular): one
   finger dragging figures across a surface -- orbits, figures of eight, scribbles, slow glides, now and then a flick
   -- pressing in slow swells, lifting to land somewhere else; a second, ghost finger comes and goes. Where the
   fingers are, how hard and fast they move, their angle, distance and age are read once a block and mapped, two to
   four ways drawn with the sound, onto its brightness, its cloud, its level, its grit, its glitches and the cloud's
   darkness. Never its pitch.
2. **The operators.** Six-operator FM: msfa (Dexed's core) in Felucca's integer port as Tonitor shaped it -- the
   DX7's 32 algorithms, four-rate / four-level envelopes, keyboard scaling, ratio or fixed frequencies, feedback, an
   LFO. Each operator reads the sine or one of 28 band-limited single cycles (four primitives, Bigorna's sixteen
   harmonic sets, eight made for bells), morphed towards the next of its bank, its phase bent as the Casio CZ bent it
   (a sine that opens like a filter, a square whose width follows its envelope). Each wave has eight levels of detail;
   one is chosen every block from the operator's frequency, so the wave itself never aliases. The macros act here:
   BRIGHTNESS on the modulators' index, DETUNE (or INHARMONIC) on the ratios, MOTION on the LFO and a tremolo. A
   7 Hz high-pass takes out what FM leaves at 0 Hz, and the sound's measured gain is applied.
3. **The exciter and the body** (generation 4). The operators' sound -- or noise, or a bow -- drives a body tuned to
   the note. A **TUBE** is a waveguide: a delay line one period long, damped and low-passed in its loop, an allpass in
   it for a wire's stiffness (its partials pulled apart), open, or closed at one end (odd partials only). A **MODAL**
   body is six resonators on the ratios of a bar, a plate, a bell, a closed pipe or a hull, pulled a little off them.
   The bow is Smith's bowed string: a friction that grips while the string moves with the bow and slips when it does
   not. A body rings 2.5 s at most and never bends the pitch.
4. **The grit** (generation 3). Russolo's, after Robert Heel's Noisferatu: a small DAC (bits dropped, each sample held
   a few times over), the sample XOR'd with a triangle at a ratio of the note (logic oscillators), and stutters -- the
   last 5 to 120 ms of the sound looped 3 to 12 times, then on as before. Mixed with the clean sound; the hand moves
   the mix and the stutters' chance.
5. **The grains.** Everything above is written, all the time, into the voice's own buffer: 12288 samples at 22.05 kHz
   in 8-bit mu-law, 0.56 s. Up to four grains at once read it back, two ways. As a **CLOUD**: grains of 2 to 400 ms at a rate of their own (1 to 400 a second), started behind the write head at a
   place that drifts, jittered in time and level, pitched by the sound's intervals, now and then reversed, darkened
   by a low-pass, fed back into the buffer so the cloud smears on itself; and, when the sound asks for it,
   **FROZEN**: the buffer stops listening a moment after the note begins and the cloud keeps turning over what it
   holds. As **PULSARS** (after Curtis Roads): grains at a ratio of the note's period, a half to four a period: one grain a
   period reads the sound back as a formant; a burst mask drops some of them, so the note grows a sub-harmonic.
   From generation 5 a share of the grains reads a found sound instead (below). The GRAIN macro moves the cloud's
   share and rate, and gives a sound without a cloud the cloud drawn with it.
6. **The hand's press** on the level, last.

A voice is done when its note is let go and it has been silent for a quarter of a second, or when its operators, its
body and its cloud have nothing left. On top of that, the host fades a released voice out within three seconds.

### The found sounds (generation 5)

Sixteen clips of 0.6 s in the flash, 8-bit like the grain buffers, each looping without a seam: ice and the sea from
NOAA's hydrophones (an iceberg calving, its tremor, an icequake, a grounding, a ship above the Challenger Deep), the
aurora's radio from NASA's Polar spacecraft (auroral hiss twice, kilometric radiation, chorus, "saucers"), and a plant
rendered here (a transformer, an air duct, a furnace, rain on tin, an engine room, wind in wires). A grain reads its
clip from anywhere in it, at a rate that follows the keys only by a drawn share, so the clip is barely transposed.
DEPTHS, AURORA and PLANT take 70 to 100 % of their grains from a clip; an older sound with a cloud takes one, one time
in three, for 30 to 60 %. Sources and credits: `found/CREDITS.md`.

### How a sound is drawn

A sound is a design drawn from four things -- a family, a seed, a path and a generation -- bit for bit the same every
time: a session keeps the seed, not the sound, and brings it back exactly. The draw:

1. **The family**, from the generator's scene and a budget of harshness (HOME), or kept (SHIFT + HOME). The scene
   leans the draw's path: FIELD the ambient one, MATERIAL the open one, FEEDBACK and EROSION the experimental one.
2. **A body**: one of eight FM bodies. The newer families each borrow one and are made what they are on top of it.
3. **The family's own**: a cloud, shattered grains, pulsars, a freeze; grit and logic; an exciter and a body; a
   found sound.
4. **The path's leaning**: AMBIENT slower and darker (attacks and releases longer, the modulators down, the cloud
   longer, an octave down one time in three); EXPERIMENTAL harsher (modulators and feedback up, mutated more often than
   not); the open path mutated one time in three. A mutation nails a partial to a frequency, turns an envelope over,
   pushes the feedback, moves an octave, puts a wave in place of a sine, or bends one.
5. **Care**: modulators on high ratios turned down, the index scaled down the keyboard, rate scaling on the carriers.
6. **Generation 2's rule**: every sound swells in, holds and keeps moving -- a slow LFO (0.1 to 1 Hz) on the
   modulators' levels, no pitch envelope, a cloud that fades in.
7. **The later generations' additions**: the hand on most sounds (85 %), grit on some older families on the
   experimental path; a body for one older sound in five; a found sound for one older cloud in three.
8. **The sanitiser**: every value within its range.
9. **The measurement**: the sound is rendered for 2.5 s at middle C, off the audio path, and its loudest 46 ms brought
   to one level (by at most +15.6 dB, so a slow swell stays a swell).

### The generations

A sound keeps the generation it was drawn under, and a generation never changes what it draws (the tests hold a hash
of hundreds of sounds of each): a session from an older build sounds as it did. New sounds are drawn on the newest.

| Generation | What it added |
|---|---|
| 0 | FM with waves in the operators, the grains, twelve families |
| 1 | the bodies kept from bending pitch (no zaps, no bleeps), the tonal ones roughened; no freeze before the body has sounded |
| 2 | textures and drones only: no IMPACT or PULSE drawn; every sound swells in, holds and keeps moving |
| 3 | the hand, the grit, eight families |
| 4 | the exciter and the body, five families |
| 5 | the found sounds, three families |

### The families

| Family | Gen. | What burns |
|---|---|---|
| METAL | 0 | Risset's bells and gongs: many carriers on inharmonic ratios, the high partials dying first |
| DRONE | 0 | slow pairs, near-harmonic, detuned to beat, an index that wanders |
| BRUIT | 0 | noise from feedback and high ratios, scattered by a sample-and-hold LFO |
| IMPACT | 0 | a struck body, its modulators dying first (not drawn from generation 2) |
| PULSE | 0 | a sound pulsed by its own LFO (not drawn from generation 2) |
| GLASS | 0 | carriers on the bell waves, morphed, barely modulated |
| FORMANT | 0 | the CZ: carriers bent by their envelopes under a modulator at the fundamental |
| REED | 0 | a saw or a square whose width is its envelope, under a feedback operator |
| CLOUD | 0 | a body under a slow cloud: long grains, pitched, dark, smeared |
| SHATTER | 0 | a body broken into short grains: fast, jittered, reversed, scattered |
| PULSAR | 0 | a sustained body read back a grain a period: formants, sub-harmonics |
| FREEZE | 0 | a body frozen into a cloud a moment after it begins |
| FURNACE | 3 | low and roaring, crushed, a dark cloud over it |
| HUM | 3 | a machine room: carriers nailed near 50 and 100 Hz, beating, XOR'd |
| STATIC | 3 | noise from feedback, crushed hard, stuttering, shattered |
| HOWL | 3 | formants an octave down under long dark grains |
| SCRAPE | 3 | scraping metal: high inharmonic partials, bent grains, held samples |
| DATA | 3 | glass through bitwise logic, crushed, stuttering, bent pulsars |
| ASH | 3 | a dark drone under a sparse, dusty crackle |
| TUNNEL | 3 | deep below: a huge dark cloud fed back, sometimes frozen |
| WIRE | 4 | a stiff wire bowed by friction, the operators its arm |
| STEAM | 4 | noise in a pipe closed at one end, hissing beside it |
| CHIMNEY | 4 | wind in a long open tube, an octave down |
| GIRDER | 4 | a girder in the wind: noise in a bar's or a plate's modes |
| HULL | 4 | the operators droning in a bell's or a hull's modes |
| DEPTHS | 5 | ice breaking and grinding, a ship far above: slow, dark |
| AURORA | 5 | hiss and radio from above the ice: airy, drifting |
| PLANT | 5 | hum, ducts, a furnace, rain on tin, engines, wires |

### The macros

Four, on the EDIT page, -64 to 64, 0 the sound as drawn, bit for bit; each felt early (the square root of the turn).
**BRIGHTNESS**: the modulators' index about 27 dB either way, the higher carriers tilted up, the feedback with it.
**DETUNE**: the carriers pulled apart (up to 40 cents), the modulators off their carriers; on the metallic and noisy
bodies **INHARMONIC**, every operator but the lowest carrier off its ratio. **GRAIN**: the cloud wetter and faster, or
drier and slower; on a sound with none, its drawn cloud from dry to wet. **MOTION**: the LFO's depths, a tremolo of its
own (1.5 to 6 Hz) and the grains' drift. The hand, the LFOs (SEQ) and you all move them at once.

## The effect slots

Twenty-two effects, two or three to a slot (the last five from build 32, their names provisional). The delay and the
plate reverb are the master's alone (build 30); a session saved with them in a slot loads them as TAPE and SPRING:

| Effect | What it is |
|---|---|
| FILTER | a driven state-variable filter: low-, band-, high-pass or notch; resonant to the edge of screaming, never piercing |
| FOLD | a wavefolder, with a second fold and a tone |
| MULAW | rate and bit reduction, the filter open enough to alias, a drive into it |
| SHIFT | a frequency shifter: every partial moved by the same 40 to 900 Hz, the harmony torn |
| MODAL | three tuned resonators, spread inharmonically, ringing to Q 325 |
| STRING | three Karplus strings on the scale's root, ringing up to 8 s |
| VOWEL | three formants morphing through the vowels, driven |
| TAPE | an echo through a worn head: dropouts, saturation, age, hiss under the sound |
| REVERSE | windows of the sound read backwards, fed back |
| SPRING | a spring tank, its chirps |
| GLITCH | Russolo's bent buffers: jumps and stutters through the last 0.74 s, never another pitch |
| CLOUD | a granular cloud; it can freeze what it holds |
| SCREAM | a feedback howl, shaped |
| RING | a diode ring modulator, modulating itself |
| STARVE | a starved, gated fuzz (after the Fuzz Factory) |
| CHEB | a harmonic re-voicer by Chebyshev polynomials (after Airwindows' Pafnuty) |
| BANK | an inharmonic resonator bank: a bar, a plate, a bell, a membrane (after Mutable Instruments' Rings) |
| SMEAR | the sound through twenty random taps, each + or −, over 12 to 46 ms: a grainy haze, smoke rather than a room (a velvet-noise decorrelator) |
| DIFFUSE | four allpasses in a row (the lengths of Mutable Instruments' Clouds): the edges blurred, no tail of its own |
| SLEW | how far the sound may move a sample, limited: the highs turned to dark, grinding ramps (after Airwindows' Slew) |
| XOR | the sound combined bit by bit (XOR, AND, OR) with itself a moment before: broken, digital, following its shape |
| DUST | hiss and crackle where the sound moves, none where it is still (after Airwindows' TapeDust) |

Twenty-four **recipes** decide what may stand at each place: SPACE, ECHO, METAL, STRINGS, EROSION, GHOST, SPECTRAL,
RESONANT, FOLDS, TELEPHONE, HELL, FACTORY, GLITCH, BROADCAST, ABYSS, RUST, FREEZER, FEEDBACK, FOUNDRY, GONG, SPECTRE,
SPUTTER, SMOG, STATIC. Every effect, given nothing, falls silent: none sounds on by itself.

## The lanes

Generator 1 runs through effect slot 4 alone, generator 2 through slot 5, generator 3 through slot 6 -- cabled into its
lane's head, with a small dry share; no cables between lanes; the lanes centre, left, right. HOME on a generator draws
its lane's chain with it, so the chain is part of its patch (SHIFT + HOME: the same family and the same recipe); HOME on
an effect slot draws that lane anew, its generator kept; REWIRE inside a lane redraws where its generator enters and
which effects also go out to the master. HOME held draws the three generators and their lanes.

(From build 28 to 35 a session could also be MESH, any generator cabled into any effect and effects across slots; it
mostly smeared. From build 36 there are only lanes: a MESH session loads rewired into them, its snapshots too, nothing
redrawn.)

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
DISINTEGRATORs, the FX MIX, the master delay and reverb, the modulation, the scale and root, the octave, the path — and how it is
played: the latched notes, whether each generator's loops play, which loops are held out, their direction, speed, VARY
and moves along the scale. (The loops' recordings themselves belong to the session.) Sixteen to a session.

**SAVE** brings up the SNAPSHOTS page: a white key let go recalls its snapshot; held 0.8 s, it stores the machine as it
is into it; SHIFT held too, it empties it. KNOB 1 there sets the **morph time**, 0 to 30 s: a snapshot recalled flows
into place over it — levels, pans, sends, values and LFOs glide; sounds cross over on voices of their own and effect
chains are faded out and in, in turn; what has no between switches at the middle. The square and key of the snapshot
it flows into blink until it is there.

**SHIFT + SAVE** brings up the SESSIONS list, as Bigorna's: four to a page, PRESETS through 64 slots (a saved session
white, an empty slot grey, the one loaded marked at its right). SAVE opens that slot's page, as Bigorna's: **BACK**; **LOAD** (on an empty slot **NEW**,
a clean slate there); **SAVE** (over another session, asked first); **COPY** (to a slot PRESETS chooses, the first
empty one offered; over a session, asked); **DELETE** (asked). PRESETS moves through the rows, SAVE runs one, SHIFT + SAVE goes back a
level. Playing, SAVE held saves at once into the session loaded. A session keeps the machine, its sixteen snapshots,
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
