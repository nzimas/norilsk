# Changelog

From build 5 the releases carry build numbers, not versions: build 5 follows 0.04, the next is build 6.

## Build 10 (2026-10-25): invisible hands, grit, new families and recipes

Engine generation 3 (dsp.h `GEN_NOW` 3). Generations 0 .. 2 draw and sound as before, bit for bit (tests/golden).
- **The invisible hand** (Anima's, Tonitor's anima.c after Vascular's AnimaCore.h), a hand on each voice: one finger
  on the sound dragging figures (orbits, figures of eight, scribbles, slow glides, now and then a flick), pressing in
  slow swells, landing elsewhere every ~18 s; a ghost finger on a clock that will not keep time. Each sound's design maps
  the hand's controllers (where it is, how hard it presses, how fast it moves, its angle, distance, age; the whole
  hand; how many fingers) to two to four of: the modulators' brightness (never the carriers' level), the cloud, the
  level (a harder press never quieter), the grit, the glitches, the cloud's darkness. Never the pitch. 85 % of the
  sounds of generation 3 have one; AMBIENT's slower, EXPERIMENTAL's quicker.
- **Grit** (Russolo's, Tonitor's russolo.c after Robert Heel's Noisferatu), after the operators: a small DAC (bits
  kept, the sample held), bitwise logic (the sound XOR'd with a triangle at a ratio to the note, as loud as the sound),
  stutters (the moment just before looped a few rounds, from the voice's own buffer), and bent grains (the cloud read
  with an address bit flipped). The older families on EXPERIMENTAL get a little of it now and then.
- **Eight new families**: FURNACE (a low roaring feedback body, crushed, under a dark cloud), HUM (carriers nailed near
  50 and 100 Hz, beating, XOR'd: a machine room), STATIC (noise from feedback, crushed hard, stuttering, shattered and
  bent), HOWL (formants an octave down under long dark grains), SCRAPE (high inharmonic metal, bent grains, held
  samples), DATA (glass through bitwise logic, crushed, stuttering pulsars), ASH (a dark drone under a sparse dusty
  crackle), TUNNEL (one or two octaves down, a huge dark fed-back cloud, sometimes frozen). All held and moving
  (generation 2's rules after them; `host sustain`: 1200 sounds, none struck, none fading).
- **A new effect, GLITCH** (Russolo's bent buffers): the sound through a 0.74 s line read at its own speed (never
  another pitch), its address bent, jumped, or stuttering on a clock; changes cross-faded. Values BEND, CLOCK,
  STUTTER, SPAN. Tile GLT.
- **Six new recipes**: HELL (torn, rung, a vast room), FACTORY (crushed, metal resonance, echoes), GLITCH (bent
  buffers, reversed or crushed), BROADCAST (a broken transmission), ABYSS (moved partials ringing, deep), RUST (a spring,
  torn, glitched).
- Fixed for generation 3: a grain read backwards no longer reaches into an earlier note's leftovers in its buffer (an
  old flaw, kept as it was for generations 0 .. 2); a voice let go and silent a quarter second ends.
- Memory: each effect slot 47 KiB (the largest recipe drawn needs 46.5); the build prints what is truly free in the
  pool after CPU1's stacks (9.2 KiB).

## Build 9 (2026-10-25): pages paint in one pass

- Every page used to be painted twice on the way in: the whole screen filled with black first (Tonitor never does),
  then the page -- two sweeps down the screen at 12 MHz, ~77 ms each, seen as the page unfurling. Now each page paints
  every one of its pixels itself and nothing else: one pass, as Tonitor's. Back on the playing page only the gaps
  between its parts are painted, an effect row blanks only between its tiles, and a whole redraw (or a row that
  changed) draws its tiles in the same frame.
- A new test, `host paint`: every page entered over a flood of a colour no page uses -- none left, and exactly one
  screen of pixels sent; a slot drawn anew 24 times under the playing page, the screen the same as a whole repaint.

## Build 8 (2026-10-25): sessions, snapshots, LFOs

- **Sessions** (Tonitor's, `sessions.c`): SAVE opens the SESSIONS page -- PRESETS through 64 slots (the number very
  large: red holding a session, grey empty), SAVE loads one, or starts a new session in an empty one; SAVE held 0.8 s
  saves into the session loaded or chosen (on the page: into the slot shown, asked SAVE NEW / OVERWRITE?, NO / YES on
  PRESETS). A session keeps every generator (its sound as what draws it again, bit for bit, and its measured level;
  its cables, modulation by another, level, pan, mute, sends), every effect slot (its patch whole), the master delay
  and reverb, the six slots' LFOs, the scale, root, octave, slot chosen, the loops' moves, every looper's loops, the
  latched notes, and its snapshots. The whole screen says SAVED / LOADED / NEW and the number.
- **Snapshots** (Bigorna's patterns, renamed): SHIFT + SAVE opens the SNAPSHOTS page, 16 a session in four rows. A
  white key let go soon recalls its snapshot; held 0.8 s stores the state now in it; SHIFT and held empties it.
  PRESETS moves the frame, SAVE recalls the one framed, SAVE held stores into it. A snapshot is the machine's state
  but the loops, the latched notes, the octave and the slot chosen. Kept with the session when it is saved.
- **The flash**: Tonitor's record format and its order of writing (the new copy first, then the old one retired), in
  the area Tonitor and Bigorna use (0x97000 .. 0xE0000). Unlike them, Molecular writes only into blank sectors and
  its own retired records, and never erases another firmware's data. Two fixes on Tonitor's scheme: a retired
  record's later sectors are looked at one by one (a sector blanked and reused since is not lost), and of two copies
  of a session the newer stays only if its body checks (a power cut while it was written keeps the older).
- **LFOs** (Tonitor's modulation, `mod.c`): SEQ's third page, MOD, for the slot chosen -- two LFOs (SHAPE, RATE, MODE,
  FADE) and four slots (FROM, TO, AMOUNT; NOW shows what it does). A generator's destinations: its four macros, level,
  pan, reverb and delay sends, the LFOs' rates. An effect slot's: the values of the effects standing in it, by place
  (named on the page by the effect there now: a slot drawn anew, the LFO moves the new effect's value at that place),
  its level and pan, the rates. SHIFT + SEQ throws the chosen slot's modulation anew. Nothing is modulated until set.
- Tests: `host sessions` (another firmware's sectors untouched through saves, overwrites, blanking and filling up; a
  full round trip after a restart; snapshots; power cuts; a damaged record refused), `host mod`.
- Memory: RAM 75 of 96 KiB (the snapshots 5.8 KiB, a sector's buffer 4 KiB); pool unchanged.

## Build 7 (2026-10-24): the master delay and reverb

- **Tonitor's master delay and reverb** (`master.c`), at the end of the chain, after the three effect slots. They hear
  what the mix holds: each generator's dry share, after its level, at its sends; and each effect slot's output at
  the mean send of the generators wired into it. The delay first (mono; its echoes to both sides, half of them into
  the reverb), then the plate (mono in, stereo out). Their returns follow the owner's rule: 2 s after the last voice
  (no loop going round) they fade out in 0.5 s.
- **KNOB 3**: the delay send of the generator the keys play; **KNOB 4**: its reverb send (0 .. 100; Tonitor's KNOB 4
  and SHIFT + KNOB 4). At power-on every reverb send 40, every delay send 0.
- **SEQ**: the DELAY page (TIME 20 ms .. 2 s, FEEDBACK, DAMP, MIX), again the REVERB page (SIZE, DECAY, DAMP, MIX),
  again back; KNOB 1 .. 4 set the four rows. Tonitor's defaults and pages.
- Differences from Tonitor: a change of the delay's TIME cross-fades instead of gliding (no bend of pitch); the
  plate's SIZE goes to 1.0, not 1.25 (memory).
- Memory: the delay's 2 s line (44 100 B) in the pool, paid by each effect slot's memory, 60 KiB to 50 KiB (the
  largest recipe drawn needs 43 KiB; a slot no longer holds its own reverb with a delay or tape beside it); the
  master plate (34 KiB) in RAM (RAM 63 of 96 KiB).
- A new test, `host master`: sends at 0 add nothing (bit for bit); each send is heard; a generator heard only
  through an effect slot reaches the master through the slot.

## Build 6 (2026-10-24): the looper; nothing lingers

- **Nothing sounds on once no note is held or latched** (the owner's rule). A voice let go sounds as it will for
  1 s, then fades 60 dB in 2 s and ends (it could ring up to 30 s). The effect slots close, over 0.5 s, 2 s after
  the last voice has ended and while no loop goes round; a note opens them at once. Measured through the mixer on 24
  patches: silence (under -60 dB FS) at most 5.5 s after the last key. A new test, `host tails`.
- **Effects that rang on by themselves, fixed**: MODAL, VOWEL, SPRING (the resonator now carries the fraction each
  result drops into the next: a truncated high-Q resonator rang on as a tone or a DC level -- MODAL up to -10 dB FS
  of DC); STRING and SHIFT (their loops round towards zero, so they die out). Every effect slot's output goes
  through a DC blocker (~8 Hz). A new test, `host fxtails`: every effect, 24 draws each, silent 10 s after its
  input.
- **The looper, Tonitor's** (`loop.c`), one a generator, on the generator the keys play: up to ten free loops each
  (no tempo, no grid; each overdub its own length), about 500 notes a generator. REC arms, the first note starts,
  REC ends (a note still held is tied over the end: a drone stays one note), REC again overdubs; REC held 1 s
  empties; SHIFT + REC: VARY. PLAY stops or starts; PLAY held: the loops on the white keys (and the screen), each
  in or out; SHIFT + PLAY: every generator's loops. The 4th to 8th black keys: REVERSE, PENDULUM, RANDOM, HALF and
  DOUBLE speed. SHIFT + OCT: every loop a step of the scale (a generator's button held too: its loops only; held with
  SHIFT, a button no longer mutes). The keys' and the loops' notes never end each other's. A test, `host looper`.
- **The screen**: in a generator's block, at the left, a white disc while its looper records, a ring blinking while
  armed, a ring while its loops play. PLAY held: the LOOPS page (Tonitor's), a numbered box a loop, in its
  generator's colour when in, a grey frame when out. REC and PLAY light as on Tonitor.

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
