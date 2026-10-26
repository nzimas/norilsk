# Changelog

From build 5 the releases carry build numbers, not versions: build 5 follows 0.04, the next is build 6.

## Build 36 (2026-10-26): lanes only; FX MIX a lane's own; the scale drawn

- **Lanes only**: MESH is gone, and with it the session page's MODE row. Each generator runs through its own lane, as
  LANES did; HOME held draws the three generators and their lanes. A session saved in MESH (builds 28 .. 35) loads
  rewired into lanes -- the playing state and every snapshot, nothing redrawn. Sessions keep their format.
- **KNOB 2, FX MIX, a lane's own**: the chosen generator's lane, or the lane chosen (one for all lanes from build 33).
  Shown as before, with GEN n LANE n under the value in the generator's colour. Kept in snapshots per lane (a snapshot
  of builds 33 .. 35 gives its one mix to every lane), glided by a morph.
- **SHIFT + ARP (the scale button): a root and a scale drawn**, the SCALE page up for two seconds to show them (turned
  there, it stays). **At power-on and in a new session the root and scale are drawn too** (natural minor in C before).
- **STRING lets go**: at some roots its long, nearly lossless loop held a whisper (about -49 dB) for ever after its
  input stopped -- the tests had only ever tuned it to C, the random root found it. Its read now rounds towards zero,
  and with nothing in and under -48 dB it is cleared at once.
- `tools/fm1_sessions.py restore FILE N`: a saved session back onto the FM-1 as session N.
- Tests: `host b36` (the scale drawn, the page shown and closed; KNOB 2 on each slot moves its lane's mix alone); `host
  lanes` (an old MESH session loaded into lanes).

## Build 35 (2026-10-26): engine generation 5 -- found sounds in the grains

- **Grains of found sound**: sixteen clips of 0.6 s in the flash (211 KB, `firmware/src/found_clips.h`), each the
  steadiest stretch of its source, looping without a seam, coded 8-bit as the grain buffers. A grain may read a clip
  instead of the operators' sound -- anywhere in it, pitched by the cloud's intervals, following the keys only as far
  as the sound's tracking (0 .. 50 %): no glide.
  - Ice and the sea (NOAA PMEL hydrophones): an iceberg calving, an iceberg's harmonic tremor, an icequake (the
    "Bloop"), an iceberg grounding, a ship heard from the Challenger Deep.
  - The aurora's radio (NASA, the Polar spacecraft's plasma wave instrument): auroral hiss (two), auroral kilometric
    radiation, chorus, "saucer" emissions.
  - The plant, rendered offline here (`tools/render_found.c`): a transformer, an air duct, a furnace, rain on a tin
    roof, an engine room, wind in wires.
  Sources and credits: `found/CREDITS.md`; the clips cut by `tools/make_found.c`.
- **Three families of them**: DEPTHS (ice and the sea, slow and dark, an octave or two down), AURORA (the radio,
  airy), PLANT (the plant, now and then crushed). One older sound with a cloud in three takes a clip for a third to
  two thirds of its grains (AMBIENT: the depths and the aurora; EXPERIMENTAL: the plant).
- Every sound of generations 0 .. 4 draws and sounds bit for bit as before (`tests/golden`, `gen5.txt` new). The cost
  is a cloud's (the clips are read from the flash, never during a flash write: those are the main loop's, interrupts
  off). `host found DIR` renders the examples.

## Build 34 (2026-10-26): engine generation 4 -- an exciter and a body

- **An exciter and a body after the operators** (new sounds are drawn on generation 4; every sound of generations 0 .. 3
  draws and sounds bit for bit as before, `tests/golden`). The exciter: the operators' sound, or noise (coloured, at
  the operators' level, followed slowly: their swell, not their beating), or a bow. The body, tuned to the note:
  - TUBE: a waveguide a period long -- its loop low-passed and damped to ring 0.3 .. 2.5 s, an allpass for a wire's
    stiffness, no DC round it; closed at one end, its partials odd.
  - MODAL: six resonators on the ratios of a bar, a plate, a bell, a pipe or a hull, pulled off them a little; noise
    into them raised by the square root of each mode's sharpness; the operators into them with a breath of noise.
  - The bow: Smith's bowed string (as the STK's) -- neck and bridge, both ends inverting, the bow table
    (|3 dv| + 0.75)^-4, rosin in the bow; its stroke its own, in over the sound's attack, lifted in a quarter second,
    the arm wandering +- 20 %.
  A spent body is let go below -56 dB. Nothing in it moves the pitch.
- **Five families of it**: WIRE (a bowed stiff wire), STEAM (noise in a pipe closed at one end, hissing beside it),
  CHIMNEY (dark noise in a long open tube, an octave down), GIRDER (noise in a bar's or a plate's modes), HULL (the
  operators droning in a bell's or a hull's modes). One sound in five of the older families is given a body too, most of
  it still heard as it was. The generator's scenes and paths lean on the new families as on the others.
- Cost: a voice with a body 590 .. 800 ns a block on the host against ~500 without (about what the grain cloud adds);
  2 KiB more a voice (8 KiB of the pool).
- Tests: `host check`, `host sustain` and `host tails` over the new families; `tests/golden/gen4.txt`; `host bodies DIR`
  renders the examples (each new family on each path, older sounds without and with a body).

## Build 33 (2026-10-25): KNOB 2, the effects' mix

- **KNOB 2, playing: FX MIX**, 0 .. 100, every effect of every lane at once -- each effect's input and output crossed:
  0 every effect passes what reaches it (dry), 100 its own output alone (as every build before). 50 at power-on and in
  a new session. Shown over the screen as KNOB 1's DISINTEGRATOR is: FX MIX, the value very large, ALL LANES (DRY at 0,
  WET at 100), a bar. Glided over a block when it moves. Kept in snapshots and sessions, glided by a morph; a session
  saved before build 33 loads at 100, as it sounded. Test: `host modules` (at 0 two very different slots sound alike,
  bit for bit; at 100 not), `host whole`.
- An effect keeps running as long as what comes out of it is heard (with the mix, its own output alone could be quiet
  while its input passes).
- The screens test's DISINTEGRATOR shot showed the SNAPSHOTS page; it shows KNOB 1 again, and KNOB 2 beside it.

## Build 32 (2026-10-25): lighter effects and master; five new effects

- **Five new effects** (names provisional): SMEAR (twenty random taps over 12 .. 46 ms, each + or -: a grainy haze, a
  velvet-noise decorrelator), DIFFUSE (four allpasses, Clouds' lengths: the edges blurred, no tail), SLEW (a slew
  limiter: dark, grinding ramps), XOR (the sound combined bit by bit with itself a moment before), DUST (hiss and
  crackle where the sound moves). Each costs under ~40 operations a sample and at most 4.2 KiB; none sounds with
  nothing in, none bends a pitch. They stand among the choices of fourteen recipes, and in two new ones: SMOG (a haze,
  dark, a grain in it) and STATIC (a broken signal, blurred). The screen names them SMR DIF SLW XOR DST.
- **The master delay and reverb run in the effects' job, a block late** (on the second core when it is up): off the
  first core's way; their returns come 0.7 ms later.
- **The master plate is in the pool, not RAM** (RAM 87 -> 52 KiB); the engine's measuring voice shares the screen's
  canvas (13 KiB of the pool).
- **The same sound for less work**: FOLD, STRING and the delay lines' reads without a division a sample; FILTER, FOLD,
  MULAW and STRING work out their values only when their knobs move; the LFOs' steps likewise; the effect slots run
  mono (no plate in them since build 31); each effect's meter is taken in its guard; the effects' input buffers are
  swapped, not copied. These leave every render bit for bit as before (checked against build 31's).
- CLOUD and GLITCH draw their randomness from their place in the slot's memory, not its address: the same on the
  device and on a computer, run after run (a different random stream from before, the same character).

## Build 31 (2026-10-25): the effect slots' memory 47 KiB -> 36 KiB

- **A session or snapshot saved with DELAY or REVERB in a slot loads them as TAPE and SPRING**, at their home values
  (the slots no longer hold either). Nothing else in a saved patch changes.
- **Each effect slot's memory is 36 KiB, not 47** (34 KiB of the pool freed): the largest chain still drawn, GLITCH +
  VOWEL + TAPE, is 36.5 KiB. Two chains no longer fit with their third effect -- STRINGS as STRING + TAPE + SPRING,
  RUST as SPRING + GLITCH + TAPE -- and are drawn, or load, as two (the third, which may be left out, is left out).
- A new patch clears its slot in 6 blocks, not 8. Test: `host modules`.

## Build 30 (2026-10-25): DELAY and REVERB left to the master

- **DELAY and REVERB are no longer drawn into the effect slots**: they are the master's (KNOB 3 / KNOB 4, SEQ), and a
  plate in a slot cost as much as several other effects. The recipes that had them take SPRING, TAPE or CLOUD in their
  place, or leave the place out. A session or snapshot saved with them still plays them as saved. Test: `host modules`
  (thousands of draws on every path, none with DELAY or REVERB).

## Build 29 (2026-10-25): LANES by default; the session pages redrawn; the main view as a place

- **LANES is the default**: at power-on and in a new session. Sessions saved as MESH load as MESH.
- **The session pages, redrawn as Bigorna's**: the list four to a page, SESSION n on each row, the chosen row white,
  the loaded one marked at its right, the saved ones white, the empty grey. A session's page has a title and rules
  between its parts: BACK; LOAD (NEW), SAVE, COPY, DELETE (in red); then the settings -- MODE, its value (LANES or
  MESH) at the right of the row, in a white field while it is being set.
- **The main view as a place**: smog across the top, drifting a few pixels every two seconds, the load and the battery
  on black over it; the three effect slots as rusty pipes underground, each effect a section between bolted flanges,
  its name on a dark plate, a band under it in the colours of the generators feeding it, the pipe glowing hotter as
  sound passes, grey and cold when the slot is muted; the three generators as brick furnaces at the foot, the fire in
  the mouth in the generator's colour, brighter as it sounds, dark with a rim when muted. The chosen slot framed in
  white; the looper's marks and a link's band as before. The textures come from a hash of each pixel's place: a
  section is redrawn alone, as before, and matches a whole repaint pixel for pixel (`host paint`).

## Build 28 (2026-10-25): MESH and LANES; Bigorna's session pages

- **A session's MODE: MESH or LANES.** MESH is the signal flow there was: any generator into any effect, effects across
  slots. In LANES each generator runs through its own effect slot (1 through 4, 2 through 5, 3 through 6), cabled into
  its head with a small dry share, no cables between lanes, the lanes centre, left, right; HOME on a generator draws
  its lane's chain with it -- the chain is part of the patch -- and HOME on an effect slot or REWIRE stays within the
  lane. Turning a session to LANES rewires it and its snapshots into lanes, nothing redrawn. Kept with the session;
  older sessions, and the FM-1 at power-on, are MESH (build 29: LANES). Test: `host lanes`.
- **The sessions are handled as Bigorna's**: SHIFT + SAVE, the list; SAVE, a session's page -- BACK, LOAD (NEW on an
  empty slot), SAVE, COPY, DELETE, MODE -- PRESETS through the rows, SAVE runs one, SHIFT + SAVE a level back; saving
  over another session, copying over one and deleting ask first. Test: `host sespage`.

## Build 27 (2026-10-25): the messages say what was saved

- **A session's or a snapshot's message names it**: SESSION (in the sessions' yellow) or SNAPSHOT (in the snapshots'
  blue) across the top, then what happened -- SAVED, STORED, LOADED, NEW, RECALLED, EMPTIED, RESTORED, or NO ROOM,
  BUSY, FAILED -- then its number, very large. (A snapshot emptied said only EMPTIED before.)

## Build 26 (2026-10-25): tails kept; USB audio; the keys only on a generator

- **The master delay's and reverb's tails ring out whole**: whatever reaches them -- a voice let go, a generator or an
  effect slot muted, the effect slots closing -- they decay as they will, then stop running once spent (before, they
  were faded out 2 s after the last voice). Nothing else lingers: the voices and the effect slots end as before. Test:
  `host mtails` (fails on build 25); `host tails` and the looper's check measure the voices and slots alone.
- **USB audio out**, Tonitor's and Bigorna's (their usb.c, the same here): connected to a computer the FM-1 is a class
  compliant audio interface -- the master before the MASTER knob, stereo, 16-bit, 48 kHz offered first (resampled by
  SLOOP's polyphase filter) or 44.1 kHz.
- **With an effect slot chosen, the white keys play nothing and latch nothing, and nothing is lit on them**: only a
  generator shows its notes. A key held over the change still ends its note when let go. No generator is framed in grey
  then, and no tiles are filled for it. Test: `host flows`.

## Build 25 (2026-10-25): generators linked by hand; the SCALE page's knobs swapped

- **One generator modulates another only when you link them**: SHIFT held, one generator's button held (the one that
  modulates), then another's (the one modulated). A kind is drawn -- RING, AM or FILTER -- and a depth; the screen
  shows it for 1.5 s in two lines: the pair, very large, each number in its generator's colour with an arrow between,
  and the kind (OFF when undone). The same gesture again undoes it. On the main view, a linked generator's block has a
  band along its foot in the colour of the one that modulates it (a small corner square before). HOME and REWIRE no
  longer draw links (before, HOME drew one now and then: a new sound on one generator could change another), and keep
  the one there is. Links in sessions and snapshots stay as they were saved. Test: `host link`.
- **On the SCALE page (ARP), SELECT sets the root and ALGORITHM the scale** (the other way round before). Test: `host
  flows`.

## Build 24 (2026-10-25): restores that always go through

- **Found with build 23's diagnosis**: a failed restore's new copy had its first sector blank -- written, then erased.
  While nothing sounds, the main loop blanks one of Norilsk's retired sectors a second; a restore's sectors look
  retired until it is committed, so when the FM-1 was silent during a restore and no older retired sector lay below
  them, the first was erased under it. The tidying now waits while a session comes in over USB. (A save from the
  panel is written in one go and never met it.) Test: `host backup`, a restore in silence with the tidying's turn
  falling just after its first sector is written -- it failed before.

## Build 23 (2026-10-25): a diagnosis of the flash, over USB

- A restore over USB sometimes stops at its last check ("the flash failed" while checking: the new copy's head does not
  read back as written) -- safely, the session already there kept, and the same restore goes through when tried
  again. To find why, the backup protocol answers a diagnosis (0x55, reading only): each sector's state as the
  firmware sees it, the last write's sectors and their states before, and the last commit's head as meant and as read
  back. `tools/fm1_sessions.py diag` prints it.

## Build 22 (2026-10-25): a restore is never disturbed

- **While a session is being restored over USB, saving or loading a session on the FM-1 shows BUSY.** A restore keeps
  its sectors reserved and half a sector waiting between its messages; a save or a load in the middle of it shares
  the same buffer and could take the same sectors. Found by the first restore on the device, which stopped with "the
  flash failed" (the session there kept, as it should be); six restores after it went through. A restore abandoned
  half-way frees them 5 s after its last message. Test: `host backup`.
- `tools/fm1_sessions.py restore` says which step failed, if one does.

## Build 21 (2026-10-25): sessions restored over USB

- **Sessions can be written back to the FM-1** over USB (Tonitor's backup protocol, now with its writes): the website's
  backup page and `tools/fm1_sessions.py restore FILE` put a saved session back in its slot, replacing the one there
  only once the new copy is whole and checked. Only blank sectors or Norilsk's own retired ones are written -- never
  another firmware's. A damaged record is refused and the session already there kept. Test: `host backup`.
- `tools/fm1_sessions.py` talks only to an FM-1 whose USB name is FM-1 Norilsk (Tonitor and Bigorna answer the same
  identity).

## Build 20 (2026-10-25): PLAY is the loops'

- **PLAY is for the loops only.** With an effect slot chosen it does nothing -- pressed, held, with SHIFT -- and stays
  dark (before, it worked the loops of the generator chosen last). With a generator chosen, its light glows dimly when
  the generator has loops, lit while they play, dark when it has none. Muting stays where it was: a slot button held
  0.8 s. Test: `host looper`.
- Both kinds of mute are in a snapshot: each slot's (build 18) and each loop's held out or in (build 19, `host whole`).

## Build 19 (2026-10-25): a snapshot is the whole machine

- **A snapshot holds the whole machine now**: besides every slot (sounds, effects, values, cables, levels, pans,
  mutes, sends), the master delay and reverb, the modulation, the scale and the DISINTEGRATOR, also how it is played --
  each generator's latched notes, whether its loops play, which are held out, their direction, speed, VARY and moves
  along the scale -- and the octave and the draw's path. Recalled, the notes latched since are let go and the
  snapshot's are held again, the loops start or stop as stored; by a morph, that happens at its middle. The loops'
  recordings themselves are the session's (each looper is 4.5 KiB: sixteen copies would not fit the RAM): a snapshot
  says how they play, not what they hold. The morph time and the slot chosen are not in it (how you move between
  snapshots, where you look). Sessions are format 3; formats 1 and 2 are read (their snapshots leave how it is played
  as it is). Test: `host whole`.
- **A morph's end refreshes its clouds**: a CLOUD swapped in during a morph, frozen, held what the generators made
  while still crossing over. At the end it takes the sound again, its grains going on over it.
- **The black keys belong to the view**: the loops' REVERSE, PENDULUM, RANDOM, HALF, DOUBLE (4th .. 8th) work only while
  PLAY is held, where they glow dimly and the ones on are lit (Tonitor's). Playing, those keys do nothing and stay
  dark, kept for what comes later.
- **DISINTEGRATOR on one line**: a font of its own (28 pt, its letters only), as large as the word fits the screen.
- **SHIFT + the 1st black key** says RAND PARAMS.

## Build 18 (2026-10-25): Norilsk

- **The firmware is now Norilsk** (Molecular until build 17). The screen at power-on, the USB name ("FM-1 Norilsk",
  a product number of its own, 0x0008, so a computer shows the new name), the package (`build/norilsk.fwsc`), the
  sources. Its sessions stay: the same flash area, the same records.
- **SAVE and SHIFT + SAVE swapped**: SAVE brings up the SNAPSHOTS page (SAVE again: back); SHIFT + SAVE the SESSIONS
  page. SAVE held while playing still saves the session; on the SNAPSHOTS page it stores into the snapshot framed. A
  white key held 0.8 s on the SNAPSHOTS page stores into its snapshot (as before).
- **KNOB 1: the DISINTEGRATOR** of the generator the keys play, 0 .. 100, on a large dial in its colour (KNOB 1 is
  the morph time only on the SNAPSHOTS page now). An unstable gate on the generator's sound, before its cables, so the
  effects hear it too: the sound goes as a chain of pulses of uneven length; a pulse lets it through, gates it down
  (to nothing from 60 up), stutters a slice of what it just made (10 .. 90 ms, repeated), or crushes it (bits and
  rate), and may throw its dry part across the stereo image. The higher, the shorter and less even the pulses, the
  more of them act, the deeper and wider. Every edge glides (~1.5 ms). Ideas from Destroy FX's Skidder and from
  buffer repeaters, none of their code. Kept with snapshots and sessions, glided by a morph. Test: `host dis`.
- **Two draw paths** (toggles, lit while on, one at a time, kept with the session): the 2nd black key ABRASIVE --
  the harsh scenes and families (BRUIT, SHATTER, FURNACE, STATIC, SCRAPE, DATA, METAL), more cables and
  cross-modulation, the torn and crushed recipes, the effects' values driven; the 3rd black key DARK -- the FIELD
  scene, drones, hums, tunnels, ash, freezes, nothing harsh, 3 dB quieter, the spaces and ringing recipes (SPACE,
  GHOST, ABYSS, FREEZER, RESONANT ..), the values darker and less driven. Neither: anything goes, every draw as before.
  The engine draws each sound as ever (the golden hashes unchanged). Test: `host paths`.
- **SHIFT + the 1st black key**: the chosen effect slot's every value drawn anew (the path's way), in place -- no
  break, its effects and cables stay; on a generator, a new sound of its family, its cables kept. Test: `host
  revalue`.
- **Eight modulation slots** a slot (four before): MOD A .. H on the MOD page; SHIFT + SEQ throws two to six. Sessions
  are now format 2; a session saved before (format 1) is read, its four slots as A .. D. Test: `host format1` (a
  session saved by build 17, `tests/sessions/format1.bin`).

## Build 17 (2026-10-25): the hiss that a morph carried over

- **Found with session 2**: snapshots 2 and 3 hold the same effects in slots 4 and 5, other cables. Recalled at once,
  a slot whose cables change is cleared and started again; morphed, its cables change in place and its effects go on.
  In snapshot 2 generator 1 feeds slot 4 (on the left): its TAPE ran. In snapshot 3 nothing reaches it -- but TAPE's
  hiss was made whatever came in, and an effect nothing reaches runs on while it still sounds: its own hiss kept it
  sounding, for ever. Hence the hiss on the left in snapshot 3, after a morph only.
- **TAPE**: the hiss is under what passes (the input and the echoes), ~0.3 s out after them, and gone with them.
- **Every effect falls silent with nothing in** (or fades, as a long echo does; a frozen CLOUD holds, as it should).
  Others held a sound of their own for ever: BANK and MODAL a tone from their resonators' last digit raised x 600 (the
  resonator now works 256 times finer), STARVE a level (its lean filter stopped short of the lean), FILTER at a high Q
  a small remainder (let go when nothing comes in). The level meter of each effect stopped at 15 instead of 0, and an
  effect under 32 (-60 dB, was 8) for 2 s with nothing in now rests. Test: `host quiet` (each effect, four ways: fed
  1 s, then 40 s of nothing).

## Build 16 (2026-10-25): a recall is never left half done

- **A snapshot recalled while a slot is still taking the last change** (its fade and clearing, ~15 ms; or a voice
  still on the sound about to be replaced) used to show BUSY and stop there: the slots that had not taken it kept the
  snapshot before, sounding inside the new one. Now the rest is given in the frames that follow, until every part has
  it (10 s at most). The same at a morph's end: after its 3 s of grace it went on as if done; now it keeps going until
  it is. Test: `host recall` (twelve cases; all twelve failed before).
- **A snapshot stored by build 14 held stale cable places** past a generator's last, so every recall took the
  generator for changed and started its notes again (a break in the sound for nothing). They are ignored now.
- Found with session 2 copied from the FM-1 (build 15): `host replay` of its 3 -> 2 -> 3 at morph times 0 .. 30 s,
  back at once or while on its way, lands on snapshot 3 within 3 dB, each side and each band. Session 2 was saved with
  MORPH OFF and a note latched only on generator 1.

## Build 15 (2026-10-25): the sessions read over USB

- **The sessions can be copied to the computer**, to find what the morph leaves behind in session 2: Tonitor's backup
  protocol, reading only (backup.c: 0x50 the list, 0x51 a piece of a record; the writes are refused, nothing is
  written to the flash). `tools/fm1_sessions.py` lists the sessions and saves one (`save 2`: `build/session_2.bin`);
  `build/host replay build/session_2.bin 3 2` plays it on the computer: snapshot 3, a morph to 2 and back, against 3
  recalled at once (each side's and each generator's loudness; WAVs in build/). Test: `host backup`.
- Nothing else changes.

## Build 14 (2026-10-25): the morph fixed; its dial; the snapshots' keys

- **A morph no longer carries the snapshot before into the one after.** An effect slot whose effects stayed the same
  had its values glided in place, its memory kept: a frozen CLOUD went on holding the earlier snapshot's sound for ever
  (recalled at once, the slot was cleared -- and a frozen CLOUD then stayed empty and silent). Now a slot with a CLOUD
  whose values change is swapped (faded out, cleared, faded in) like a slot whose effects change, and a fresh CLOUD
  fills its 0.74 s with the sound playing before it freezes. Tests: `host ends` (twelve morphs, each against the same
  snapshot applied at once to a fresh machine: the same state, the same loudness), `host hiss` (a hissing TAPE morphed
  into a quiet one).
- **KNOB 1's dial**: turned, on the playing page or the SNAPSHOTS page, it covers the screen for 1.5 s -- MORPH, the
  time very large (4.5, or OFF), SECONDS, a bar of its reach (0 .. 30 s). (The large font gained . O S.)
- **The SNAPSHOTS page's keys**: every stored snapshot's key glows dimly, the one chosen lit, the one a morph flows into
  blinking.

## Build 13 (2026-10-25): snapshots morph

- **KNOB 1** (playing, and on the SNAPSHOTS page) sets the morph time, 0 .. 30 s in half seconds (MORPH 4.5 S; 0:
  OFF, at once as before); kept with the session.
- **A snapshot recalled with a morph time flows into place**: every level (finer than its 1.5 dB steps while it
  glides; mutes faded through it), pan and send, the master reverb's DECAY DAMP MIX and the delay's FEEDBACK DAMP MIX,
  the LFOs' rates, fades and amounts, and an effect slot's values where the same effects stand in the same places,
  glided all the way. A generator whose sound changes: its held and latched notes cross over, the old sound fading out
  on its voice while the new one comes in on a voice of its own; an effect slot whose effects change: faded out,
  changed, faded back in -- each its own stretch of the morph, generators and slots taking turns (the voices and the
  memory allow one at a time). At the middle what has no between: the scale and root, the LFOs' shapes and courses,
  cables where nothing else changes, the reverb's SIZE and the delay's TIME (glided, they would bend the pitch). At the
  end the snapshot exactly as stored. Recalled again on the way: a new morph from where it is.
- The snapshot it flows into **blinks** on the SNAPSHOTS page, and its key, until it is there.
- A test, `host morph`: notes held through a 4 s morph between snapshots apart in nearly everything -- never silent,
  old and new sounds heard together, levels gliding in steps of a few 256ths, SIZE kept to the middle, the snapshot
  exactly at the end.

## Build 12 (2026-10-25): the dull effects made drastic

Saved sessions and snapshots that use these effects sound different from build 12 on (their values are kept; what
they do with them changed).
- **FILTER**: a notch beside low-, band- and high-pass; DRIVE (x 1 .. x 12, tanh) into it in place of the slow drift
  (with more resonance it would have swept like a siren); RES up to Q 12, squelchy and screechy but kept off the
  eardrums -- its level comes down as the resonance goes up, the resonance is pulled back above 3 kHz, a soft ceiling
  under full scale. A test, `host ears`: every type at its highest resonance, across its range: at most 0.44 of full
  scale and +2.7 dB over the dry.
- **MULAW**: down to 2 bits (was 4), rates down to FS / 32 (1.4 kHz; was FS / 8), the filters open far enough to
  alias, drive up to x 8.
- **SHIFT**: shifts of 40 .. 900 Hz (every partial moved off the harmonics; small shifts sounded like a chorus); the
  feedback gone (fed back, a shifter spirals: a barber pole) for DRIVE into it (denser sidebands); TONE after it.
- **MODAL**: its modes further apart (up to 4.2 times), the sound driven into them, their ring brought up with the Q
  under a soft ceiling: from -14 dB against the dry to +5.
- **VOWEL**: formants three times sharper (Q 10 .. 60), driven harder (x 2 .. x 16), brought up with the Q.
- **TAPE**: DROPOUTS in place of the wow (the echoes gone 20 .. 200 ms, up to twice a second; the time's own wander a
  breath of 0.1 %); AGE now saturates the head harder, darkens to 3 kHz and adds hiss.

## Build 11 (2026-10-25): rewiring; six drastic effects

- **The first black key rewires the chosen slot**: a generator's cables into the effect slots and its modulation by
  another generator drawn anew (HOME's rules), its sound, send and levels as they are -- taken without a fade, the
  notes going on; an effect slot's outs to the master and cables across to the slots after it drawn anew, its effects
  and values as they are, changed in place (no fade, nothing cleared). The top band says REWIRED and the slot.
- **Six new effects**, more drastic, after open sources (ideas, not code):
  CLOUD (a granular freezer, after Mutable Instruments' Clouds and Beads: SIZE, DENSITY, FREEZE, SPREAD; frozen above
  the middle of FREEZE), SCREAM (a saturated feedback network, a no-input mixer's howl, ducked by the input so it dies
  with it: SIZE, FEEDBACK, TONE, DRIVE), RING (a diode ring modulator after Parker's model, a fixed carrier or the
  sound's own past: FREQ, DIODE, SELF, TONE), STARVE (a starved, gated fuzz after the Fuzz Factory: it sputters and
  tears: GAIN, GATE, BIAS, TONE), CHEB (a Chebyshev re-voicer after Airwindows' Pafnuty: SHAPE, ODD, DRIVE, TONE),
  BANK (an inharmonic resonator bank after Rings and Elements -- a bar, a plate, a bell, a membrane: PITCH, Q, SET,
  BRIGHT). None bends a pitch; none rings on by itself (`host fxtails`).
- **Six new recipes**: FREEZER, FEEDBACK, FOUNDRY, GONG, SPECTRE, SPUTTER; HELL, FACTORY and ABYSS may draw the new
  effects too.
- A test, `host rewire`: the sound byte for byte the same, no note cut, the cables new every time.

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

- **Six slots**, a generator each, on the top row's six buttons: pressed, the slot chosen; held 0.8 s, the
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

- **The patch** (`patch.c`): one generator (the engine, `mol.c`) and a chain of up to four processors, drawn whole by
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

The engine (FURNACE from build 37): six-operator FM (msfa in Felucca's integer port as Tonitor shaped it) with waves in its operators (28
band-limited single cycles, morph, phase distortion, the bend following the envelope) and a grain cloud after
them (CLOUD, PULSAR, FREEZE). Twelve families on three paths, four macros, every sound measured to one level;
the host harness and the golden hashes of generation 0.
