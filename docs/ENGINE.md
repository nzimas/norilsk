# MOLECULE: the engine

The engine of Norilsk, as built at the engine stage (version 0.01). Everything below is in
`firmware/src/mol.c`, `fm6_core.c` and `wt.c`.

## What it is

A six-operator FM voice (the DX7's 32 algorithms, four-rate / four-level envelopes, keyboard scaling, ratio or
fixed frequencies, feedback, LFO and pitch envelope, in the 155-byte single-voice layout: msfa, in Felucca's
integer port as Tonitor shaped it), with:

1. **waves in the operators**, from wavetable synthesis;
2. **a grain cloud after the operators**, from granular synthesis;
3. **a generator** of sounds, in families, on paths, measured as they are made, as Tonitor's engines have.

It keeps Tonitor's engine contract (design / voice / note on, off, hold / render / measure / env_adsr /
mac_name), so Tonitor's host (the six layers, the shared pool of twelve voices, the two-core block, the
loopers, the sessions) can run it with the changes a new engine always needs there.

## The operators' waves

Each operator carries a `fm6_wsel_t`: `wave` (0 the sine, else one of 28 band-limited single cycles),
`morph` (0..255 towards the next wave of the bank), `pd` (the phase-distortion breakpoint, 128 straight),
`flags` (`FW_PDENV`: the bend follows the operator's envelope) and `lvup` (set by the generator for a modulated
operator: its wave one level higher, since FM spreads what the wave has).

- **The waves** (`wt.c`, `tools/gen_tables.py`): PRIM (sine, triangle, saw, square), HARM (Bigorna's sixteen
  harmonic sets) and BELL (eight made here: partials at the places FM puts its sidebands and bells their modes).
  Each wave has eight levels, level k keeping 128 >> k harmonics in 256, 128 or 64 points; all levels of a wave
  are normalised by one peak. 28 waves x 776 points x 2 bytes = 43 KiB, copied to POOL at start-up (the flash is
  too slow to read a sample at a time).
- **The level** is chosen once a block from the operator's phase increment (Q24 cycles a sample): level k when
  the increment reaches 2^(16 + k), so no harmonic of the wave itself passes ~22 kHz. What FM then does to the
  wave aliases as it does on any FM synthesizer with waveforms (the Opsix, the Digitone): the generator keeps
  modulators on high ratios turned down, as Tonitor's does.
- **Phase distortion** (after the Casio CZ, as Bigorna's WAVY does it): the phase is bent so that the first half
  of the wave takes `bp` of the cycle; two reciprocals once a block, one multiply a sample. On a sine it is a
  resonant knee that opens the harmonics like a filter; on a square it is the pulse width. With `FW_PDENV` the
  bend is scaled by the operator's linear gain, so the wave opens with the level and closes as it dies (the CZ's
  DCW envelope). The bend is then driven by the same envelope as the level, which is the compromise: the DX7's
  envelope is the only one an operator has.
- **The kernels** (`fm6_op_run`, `fm6_op_fb`): eight loops, four for the plain wave (the sine with no bend is
  the same work as msfa's) and four for a bent or morphed one (one bend, two table reads, a crossfade).

## The grains

Each voice holds a buffer of 12288 mu-law samples at FS / 2 (0.557 s). The operators' sound is always written
into it (pairs averaged, mu-law encoded); what the grains may read is what the note has written so far
(`filled`), so a voice reused for a new note never reads the old one. The buffer is not cleared at a note-on:
that would cost a tenth of a block in the ISR.

A sound's cloud is a `mo_gr_t`:

| Field | What |
|---|---|
| `mode` | OFF, CLOUD, PULSAR |
| `size_ms` | a grain, 2 .. 400 ms |
| `rate` | CLOUD: grains a second, 1 .. 400 (at most 660); PULSAR: 4 x grains a period, 1 .. 16 (a half to four a period) |
| `back_ms`, `spread_ms` | where a grain starts: behind the write head, and how far it may scatter |
| `iv[4]`, `pch` | the intervals a grain may take (semitones) and the chance it takes one |
| `jit` | CLOUD: the timing's jitter (and the level's); PULSAR: the chance a grain is dropped |
| `cents`, `rev`, `shape` | the pitch's jitter; the chance a grain plays backwards; Hann, percussive or swell |
| `fb` | the grains fed back into the buffer: the cloud smears on itself |
| `mix` | the grains' share of the output |
| `burst` | PULSAR: of every eight grains, how many sound (a sub-harmonic) |
| `freeze_ms` | the buffer stops listening this long after the note begins (0: never) |
| `atk_ms`, `rel_ms` | the cloud's own level: in after the note begins, out after it ends |
| `dark` | a low-pass on the grains, 16 kHz down 5.5 octaves |
| `drift` | a slow wander (0.025 .. 0.1 Hz) of where the grains start, as a share of the spread |

- **CLOUD**: a grain every `FS / rate` samples, jittered; at most four at once (`MO_GR_MAXG`). Their level is
  normalised by the overlap (a Hann grain's mean square being 3/8: each at sqrt(8 / (3 O)), at most x 2). A
  grain starts between `need` (so its read head never meets the write head) and `dmax` (so the write head never
  comes round to it, and never past what the note wrote), as Tonitor's granulator does.
- **PULSAR** (Roads): the interval is the note's period (from the note's log frequency) times 4 / `rate`; a
  burst mask of eight drops the grains past `burst`; `jit` drops grains at random. A grain a period reads the
  sound back as a formant whose bandwidth is the grain's length; fewer grains a period make a sub-harmonic.
- **Frozen**: once frozen, the voice stops writing; when the cloud is fully wet, the operators are not even
  computed (a frozen cloud costs only its grains). The voice then lives until its own level has fallen after the
  note ends (or until the host ends it, in hold mode).
- **Alive**: a voice without a cloud ends when its carriers have fallen silent for good (msfa's rule). With a
  cloud, it goes on for the span the grains can still read (back + spread + size, plus two seconds if fed back),
  or until its level has fallen after the note ends.

## The macros

Four, -64 .. 64 each, 0 the sound as drawn, bit for bit, each felt early (the square root of the knob's turn):

- **BRIGHTNESS**: the modulators' level (the index) about 27 dB either way, the carriers tilted (the higher ones
  up), the feedback up with it (Tonitor's).
- **DETUNE**: the carriers but the lowest pulled apart (+-40 cents, one way and the other), every modulator
  +-25 cents off its carrier; **INHARMONIC** on METAL, BRUIT, IMPACT, PULSE and SHATTER bodies: every operator but
  the lowest carrier off its ratio (Tonitor's).
- **GRAIN**: on a sound with a cloud, up: wetter, the rate up to four times, more of the intervals; down: drier,
  slower. On a sound with none, up: the cloud it would have (`gr_def`, drawn with every sound), from dry to wet.
- **MOTION**: the LFO's depths, a tremolo of its own above the middle (1.5 .. 6 Hz), and the grains' drift.

## The families

Eight bodies and four clouds that borrow a body (CLOUD and SHATTER any body, PULSAR a sustained one, FREEZE a
struck one), weighted on each path:

| Family | What | OPEN | AMBIENT | EXP |
|---|---|---|---|---|
| METAL | many carriers on inharmonic ratios, the high partials dying first; some on bell waves | 2 | 3 | 2 |
| DRONE | three pairs, near-harmonic, detuned to beat, slow, an index that wanders | 2 | 5 | 1 |
| BRUIT | feedback 7, modulators on high or fixed frequencies (saws and squares among them), a sample-and-hold LFO on the pitch | 2 | 0 | 5 |
| IMPACT | a pitch envelope that falls, the modulators dying before the carriers | 2 | 1 | 4 |
| PULSE | a square or random LFO at 2 .. 12 Hz on every operator's level | 2 | 0 | 4 |
| GLASS | two or three carriers on bell waves, morphed, half of them bent by their envelopes, barely modulated | 2 | 5 | 1 |
| FORMANT | one or two carriers bent by their envelopes (open or closed knees), a modulator at the fundamental or an octave under | 2 | 3 | 2 |
| REED | a saw or a square whose width is its envelope, modulated by a feedback operator | 2 | 2 | 3 |
| CLOUD | long grains (80 .. 300 ms, 8 .. 40 a second), pitched by the dark intervals, dark, smeared, a freeze one time in five | 3 | 5 | 2 |
| SHATTER | short grains (4 .. 40 ms, 60 .. 400 a second), jittered, reversed, scattered, bright intervals | 3 | 0 | 5 |
| PULSAR | a sustained body read back a half to four grains a period, 2 .. 15 ms, bursts of 2 .. 8 | 3 | 2 | 4 |
| FREEZE | a struck body frozen 150 .. 800 ms after the strike, fully wet, fed back, fading over 1 .. 6 s | 3 | 4 | 3 |

Then the path's leaning (AMBIENT: the carriers' attacks and releases slower, the modulators down, the LFO
slower, the cloud darker and longer, an octave down one time in three; EXPERIMENTAL: the modulators up, the
feedback up, mutated more often than not, sometimes twice), one mutation in three on the open path (a partial
nailed to a frequency, an envelope turned over, the feedback pushed, an octave away, a wave in place of the
sine, a bend), the care pass (modulators on high ratios turned down, the index scaled down the keyboard, rate
scaling on the carriers) and the sanitiser (every byte in its range; `lvup` for the modulated operators).

The draw is a function of (seed, path, generation). `tests/golden/gen0.txt` holds a hash of the design and
the first two seconds of every one of 288 sounds; a change to any of them fails the tests. New behaviour of
the generators goes under a new generation, as Tonitor's do.

## Measurement

`mo_measure` renders the sound for 2.5 s at middle C (the probe voice is in POOL) and sets `gain` (Q12) so
that the loudest 2048-sample window reaches 0.1 RMS (at most +15.6 dB, so a slow swell stays a swell). It
returns a measure of brightness in Hz, which a host may use to choose among candidates (Tonitor's `new_fm`
draws several and keeps the one whose brightness suits the path).

## Cost and memory

Host bench (relative; the chip is measured on the chip): a plain FM body 250 .. 400 ns a block, a cloud or
pulsar 480 .. 530 ns, the GRAIN macro on a body about +250 ns. Tonitor measured a plain FM voice at 5 .. 6 %
of a 726 us block on the device; expect a cloud voice at about twice that, so six to eight cloud voices across
the two cores. If that proves tight, the grain loop can run at FS / 2 like the buffer, or `MO_GR_MAXG` can drop
to 3.

Memory: a voice is 12.6 KiB (its buffer 12 KiB), so twelve voices are 151 KiB of POOL, plus 43 KiB of waves
and 12.6 KiB for the probe; 207 KiB of the 336 KiB pool, leaving room for a delay line (44 KiB in Tonitor) and
a reverb (43 KiB). The waves could be halved (level 0 at 128 points) if needed.

## What is not here yet, on purpose

- The host: layers, the voice pool, the two-core block, the loopers, sessions, MIDI, the screen, the panel.
- The module system and the UI: the owner's decisions.
- Effects as modules (delay, reverb, distortion, bit crush, sample-rate reduction: Tonitor has them all).
- A choice among candidates by brightness at HOME (Tonitor's `new_fm`).
- A second LFO or envelope for the bend: the CZ had one; here the bend follows the operator's own envelope.
