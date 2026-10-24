# Changelog

From build 5 the releases carry build numbers, not versions: build 5 follows 0.04, the next is build 6.

## Build 5 (2026-10-24): textures and drones only

The owner's direction: Molecular is for evolving soundscapes. Nothing struck, plucked or pulsed; nothing that needs a
sequence to make sense.

- **Generation 2 of the generators** (dsp.h `GEN_NOW` 2; `mol.c` `mo_gen2`), applied over every family's body, the
  path and the mutations: the carriers swell in (no strike: they rise to the level they hold, never fall back to it)
  and hold high; the modulators get slow envelopes of their own, so the timbre opens and closes over many seconds;
  the LFO breathes (roughly 0.1 to 1 Hz) on the modulators' depth and a little on the carriers' level, never more
  than a trace on the pitch; no pitch envelope; the carriers detuned apart to beat; a cloud fades in. IMPACT and
  PULSE are no longer drawn; FREEZE and the other clouds borrow a body that holds. Generations 0 and 1 draw as
  before (tests/golden).
- **A new test** (`host sustain`): 720 sounds held 10 s; each must swell in (its first quarter second at most twice
  what it holds), hold (its last 2 s at least a fifth of its loudest moment), and no more than 2 % may barely move.
  At build 5: no failures, 6 barely moving.
- **SELECT and ALGORITHM**: the level and pan of the generator chosen, or chosen last while an effect slot is chosen,
  as on Tonitor. The effect slots' level and pan are no longer on the knobs (they stay as drawn at power-on).
- **Pans at power-on**: generator 1 in the middle, 2 left, 3 right (70 % of the way). HOME no longer draws a
  generator's level and pan: they are the player's.
- The startup screen says BUILD 5; the build prints `build 5` in its package line.

## 0.04 (2026-10-24): three generators, three effect slots, two cores

The owner's design after trying 0.03: the engine is dear, so three generators instead of six.

- **The top row**: FX SEL ENV are the generator slots 1 to 3, LFO EDIT GLO the effect slots 4 to 6. A press chooses
  the slot; a 0.8 s hold mutes it, or lets it be heard again. Nothing is switched off: a muted generator's notes go on,
  silent; a muted effect slot is skipped.
- **Effect slots**: each a chain of two or three effects drawn from a recipe (SPACE, ECHO, METAL, STRINGS, EROSION,
  GHOST, SPECTRAL, RESONANT, FOLDS, TELEPHONE). HOME on an effect slot draws its recipe, its effects, their values and
  its cables across to the slots after it; SHIFT + HOME keeps the recipe. The same effect may stand in several slots:
  each is an instance in its slot's own memory (60 KiB a slot; a recipe is drawn to fit). A new patch fades the slot
  out, clears its memory a piece a block, and starts the new effects. REV (the plate reverb, capped at SIZE 1.0 for the
  memory) and DLY (the delay, now 1 s) are effects like any other.
- **Generators**: HOME draws the chosen generator's sound, the effects it is wired into (one to three, in any slot,
  mostly at the head of a chain), its modulation by another generator, its level and pan. SHIFT + HOME keeps the
  family. HOME held 0.8 s draws all six slots anew.
- **Four voices** for the three generators; the keys play the generator chosen last, also while an effect slot is
  chosen. SELECT and ALGORITHM set the chosen slot's level and pan, generator or effect slot.
- **Latching, the other way round**: hold a white key and press SHIFT to latch it; hold SHIFT and press the lit key to
  let it go.
- **The second core** (Tonitor's): CPU1 runs the effects, on what the generators sent the block before (0.73 ms
  later), and the voices dealt to it by their cost; CPU0 the rest. One core and two give the same sound, bit for bit
  (a host test). A CPU1 fault retires it until power-off, said on the screen.
- **The screen**: the three generators as large numbered blocks (filled when heard, an outline when muted, framed in
  white when chosen, in grey when the keys play it while an effect slot is chosen); the three effect slots as rows of
  named tiles, each bordered in the colours of the generators wired into it, the keyboard generator's tiles filled with
  its colour, a muted row dark, the chosen row framed in white. No cables.
- Effects' values are drawn within bounds that keep each one heard; SPRING takes the sound itself as well as its
  colour; FILTER's band-pass is kept broad.

## 0.03 (2026-10-23): two dice, generators and pedalboard

The owner's model: synthesizers sharing a pedalboard.

- **HOME** rolls the chosen slot: its sound, the modules it is wired into (one to three, often shared with other
  slots), how much it sends them, its dry share, its level and pan, and now and then its modulation by another slot.
- **SAVE** rolls the rack: every module's values, and how the modules are wired to each other -- an order the sound
  runs through, from each module one or two cables to modules later in that order and/or out to the master. The
  slots' wiring into the rack is kept. (SEQ does nothing now.)
- **DELAY** (Tonitor's) is a module again, in FLANGE's place; delay and reverb are modules like any other, no sends.
- **Nothing in the rack bends a pitch**: SHIFT's shift is fixed while it sounds (its fourth value is now its
  feedback's tone), REVERSE plays at the sound's own pitch (no octave head; its third value is now feedback, its
  fourth how far back it reaches), TAPE's wow is faint (at most 0.25 %), no flutter; a change of delay time
  cross-fades instead of sliding; FILTER's drift is slow and narrow.
- **The engine's generation 1** (new sounds): IMPACT's pitch fall a thud of a few semitones, not a zap; BRUIT's
  sample-and-hold on the level more than the pitch; PULSE's pitch still; DRONE's vibrato faint; FORMANT and REED given
  an inharmonic modulator one time in two; a freeze never before the body's attack is over. Generation 0 is kept as it
  was (tests/golden).
- **The draw leans to the inharmonic**: metals, bruits, impacts, glass, clouds, shatters, freezes up; formants and
  reeds down.
- **Latching**: SHIFT + a white key latches its note on the chosen slot (the key stays lit while that slot is
  chosen); SHIFT + the key again lets it go. A latched note is never stolen while another can go. A new patch on a
  slot moves its held and latched notes over to the new sound instead of cutting them.
- **The screen**: round each module, a border in the colours of the slots wired into it, shared out equally
  between them; the modules the chosen slot is wired into filled with its colour, their names white. How the modules
  are wired to each other is not drawn.

## 0.02 (2026-10-23): six slots and a rack

The owner's design after trying 0.01.

- **Six slots**, a MOLECULE generator each, on the top row's six buttons: pressed, the slot chosen; held 0.8 s, the
  slot on, or off again. Only slot 1 is on at the start. The keys play the slot chosen; a key lifted ends its note
  on the slot it was played on. Six voices shared by the slots.
- **A rack of twelve modules**, always there, in the signal's order: FILTER, FOLD, MULAW, SHIFT (a frequency
  shifter), MODAL, STRING, VOWEL, FLANGE, TAPE, REVERSE, SPRING, PLATE (`rack.c`, after `docs/PROCESSORS.md`).
  Where a module sits is where it sounds: the left column at the left, the right at the right. Each passes a guard
  (a level rider and a ceiling). MODAL and STRING are tuned to the scale's root.
- **A patch is a slot's**: HOME draws the chosen slot's sound, its chain through one to three modules (in rack
  order: each feeds the next, the last goes out), its dry share, its level and pan, and now and then a modulation by
  another slot that is on (ring, amplitude, or a filter its level opens). SHIFT + HOME: a new one of the same
  family. HOME held 0.8 s: every slot that is on, anew. The draw follows Vascular's abstract path as 0.01's did.
  Modules fed by several slots hear their sum; a module's output goes to the next module of every chain through
  it, as a mult would. A module no slot feeds any more rings out where it went.
- **SEQ** draws new values for the twelve modules (each at its home one time in five, else moved), keeping the
  wiring.
- **SELECT and ALGORITHM**: the chosen slot's level and pan (said on the screen).
- **The screen**: the load at the top left (red from 85 %), the battery at the top right, large; the six slots, a
  block each in its own colour (filled when on, an outline when off, white frame when chosen, brighter as it sounds,
  a corner square in the colour of the slot that modulates it); the rack as twelve named tiles, three across, lit as
  sound passes, a bar along each tile's foot in the colour of every slot wired into it, the chosen slot's chain
  framed in its colour. No cables. No family names.
- Tests: `host modules` (every module alone, its level against the dry), the flows of the slots' buttons.

## 0.01 (2026-10-23): the first working version

The first installable build: a patch played from the keys.

- **The patch** (`patch.c`): one generator (MOLECULE, `mol.c`) and a chain of up to four processors, drawn whole by
  HOME after Vascular's abstract path: a scene (FIELD, MATERIAL, FEEDBACK, EROSION) leans the generator's family
  and the chain's kinds; a harshness budget of 0, 1 or 2 pays for the rough families and the destructive or
  resonant kinds; a chain never has two kinds of one character nor two destructive ones; the reverb comes last,
  added as a shared tail by the scene's chance. The generator's place on the screen is its pan (across) and its
  level (down, the top loudest).
- **The processors** (`nodes.c`): FILTER, CRUSH, DRIVE, FOLD, RING, COMB, FLANGE, DELAY, REVERB -- Tonitor's
  filter, crush, distortion, delay and plate, and four after Vascular's (`docs/PROCESSORS.md`).
- **Six voices** of the one generator, drawn from a pool as the notes need them; a stolen voice fades, a patch
  change fades them all; a released voice ends 66 dB under its loudest, or at 30 s.
- **The panel**: HOME a new patch; the white keys play it in the scale chosen; ARP the SCALE page (Tonitor's 23
  scales: SELECT the scale, ALGORITHM the root); OCT- and OCT+ the octave; MASTER the level; USB MIDI notes as the
  keys. SAVE and SEQ are kept for the sessions and the patterns; PLAY, REC and the top row do nothing yet.
- **The screen**: the family's name, large, in its colour; the network -- the generator a disc where the patch put
  it, the processors as rings on the way to the output, joined by lines; the chain's names along the bottom; the
  octave at the top right; the audio's load, small, at the bottom right (a diagnostic, for now).
- **The board layer**: Tonitor's HAL, boot, USB (identity "FM-1 Molecular", PID 0x0007), the installer's handshake,
  the update loader and the rescue mode. One core renders; USB audio out, the TRS MIDI jack, sessions, the second
  core and the backup protocol are not in this build.
- **The host harness**: the engine's checks, and now the firmware's -- patches drawn and played through the real
  mixer, the flows (HOME under held notes, every key at once, forty patches in a row), the screens as PNG.

## 0.00 (2026-10-23): the engine stage

MOLECULE: six-operator FM (msfa in Felucca's integer port as Tonitor shaped it) with waves in its operators (28
band-limited single cycles, morph, phase distortion, the bend following the envelope) and a grain cloud after
them (CLOUD, PULSAR, FREEZE). Twelve families on three paths, four macros, every sound measured to one level;
the host harness and the golden hashes of generation 0.
