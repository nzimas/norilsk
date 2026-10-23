# Changelog

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
