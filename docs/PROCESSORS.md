# Vascular's processors on the FM-1: what ports, what is heard

A survey of Vascular's 71 processors (`vascular-ios`, `Core/Sources/VascularCore/Generated/ProcessorKind.swift`,
`Resources/vascular.csd`, `Sources/CVascularOpcodes`), judged for this chip: no FPU, int32 Q15 at 44.1 kHz in
32-sample blocks, about 160 MHz, mono nodes, roughly 130 KiB of pool for effect lines once the voices and the
waves have theirs. The twelve in Molecular's rack (0.02): FILTER, FOLD, MULAW, SHIFT (sideband), MODAL, STRING,
VOWEL, FLANGE, TAPE, REVERSE, SPRING, PLATE. The **[node]** marks below are 0.01's chain processors.

Two of Vascular's own rules carry over whole: every processor exits through a guard (an RMS gain rider with a
ceiling, a NaN kill, a clip), which is what keeps feedback paths from pinning the bus; and a loop gain is clamped
to 0.2 .. 0.93, the reverb time derived from it, not the other way round. Vascular also vets every processor by
rendering it (its `vet-processors.py`): the ones that measured inaudible were rebuilt (the phaser was silent, the
comb was a flanger, three macros out of four on several kinds did nothing) or withdrawn (a dynamics region).

## Port first: cheap, strong, little memory

| Kind | Character | What it does | Memory | Note |
|---|---|---|---|---|
| fold **[node FOLD]** | destructive | the wave driven into a window and reflected back, twice | none | strong |
| wrap | destructive | the wave wrapped modulo a window: torn | none | very harsh |
| clip | destructive | soft and hard clipping with bias and tone | none | strong |
| powershape | destructive | an exponent on the wave | none | medium |
| chebyshev | destructive | Chebyshev polynomials T1 .. T6: harmonics added | none | strong on tones |
| decimate | destructive | sample hold, bit depth, a ring on a resonator | none | strong |
| glitter, vacuum (Airwindows) | destructive | quantisers blended; multistage tube saturation | none | strong |
| mulaw, gsm | destructive | the G.711 and GSM 06.10 codecs behind a downsampler | ~1 KiB | lo-fi; GSM is integer C already |
| Tonitor's distortion **[node DRIVE]**, crush **[node CRUSH]** | destructive | two valve stages at 2x; bits and rate | none | |
| rezzy, moog, lpf18, diode, squelcher | spectral | resonant filters: an SVF with tanh, a 4-pole ladder, a 3-pole, a diode ladder, an acid low-pass | none | strong; the ladder and the diode need care in Q15 |
| state-variable **[node FILTER]** | spectral | LP / BP / HP, resonant, a slow wobble on the cutoff | none | |
| mode (modal) | resonant | three 2-pole resonators tuned to the note, Q 4 .. 325 | none | turns anything into struck metal; high Q needs 32-bit state |
| streson (string) | resonant | three Karplus-style string resonators | ~8 KiB | strong ring |
| waveguide | resonant | two delay lines with a low-pass in the loop | ~11 KiB | |
| comb **[node COMB]** | resonant | a tuned feedback delay, 0.4 .. 50 ms, damped, wobbling | 4.5 KiB | pitched ring |
| vowel | resonant | three formant band-passes morphing a, e, i, o, u | none | cheap, clearly heard |
| resonX (formant) | resonant | two banks of four band-passes, one swept by an LFO | none | its raw output is quiet: gain before it |
| flanger **[node FLANGE]** | modulation | a swept short delay with feedback | 2 KiB | strong |
| phaser | modulation | first-order allpasses swept, mixed with the dry | none | was silent in Vascular until fixed |
| sideband / ring **[node RING]** | modulation | a Hilbert pair and a quadrature oscillator: single-sideband shifting; at its middle, ring modulation | 2 KiB | excellent: inharmonic, barber-pole; the node has the ring alone so far |
| doppler | pitch | a moving source: a delay read at a changing distance | 10 KiB | pitch swoops |
| exciter | pitch | a high band distorted and added back | none | subtle on dark voices |
| shadow | pitch | a PLL tracks the pitch, an oscillator follows it | none | wanders on noise |
| breadButter (AUDelay), tape **[node DELAY is Tonitor's]** | temporal | a plain delay with one pole in the loop; a tape with wow, saturation and age | 26 .. 44 KiB | strong |
| spring | space | three allpasses into a 30 .. 120 ms tank, a resonator for tone | 25 KiB | the spring sound; a good second tail |
| freeverb, hall (nreverb), chamber **[node REVERB is Tonitor's plate]** | space | combs and allpasses; a plate; early taps into an FDN | 25 .. 50 KiB | one shared tail at a time on this chip |

## Portable cut down

cloud (Clouds' grains: a 0.5 s line instead of 1.6 s), warp (a modulated delay, 100 ms instead of 900), spiral
(echoes that climb: 250 ms plus a 60 ms shifter), reverse (250 ms windows), chew and skip (a 0.5 s ring, bent,
stopped, skipped), taps (four heads on 250 ms), diffuse (nested allpasses, 40 ms), destroyer (20 short layers,
8 KiB), disperse (four bands delayed apart, 150 ms each), shifter (a splicing shifter on 8k samples), lpc (order
10 .. 12, not 32), madDelay (two lines of 300 ms, not 2 s).

## Not portable (no FFT, no thread, megabytes)

The fourteen `pvs` kinds (freeze, blur, scale, shift, warp2, morph, cross, vocode, arp, stencil, skeleton, prism),
convolve, churn (a worker thread over 42 offline processes), galactic (a megabyte of lines), spool and fickle (24 s
and 20 s of tape), harmony (an FFT pitch tracker). Time-domain stand-ins of the same character: sideband for
shift, a grain loop for freeze and blur, disperse for prism, a stepped resonant band-pass for arp, vowel or resonX
for warp2 and vocode.

## What a chain obeys (Vascular's abstract path, as patch.c has it)

A scene leans the kinds (x 2.8), a harshness budget of 0, 1 or 2 pays for the destructive and the resonant kinds,
a chain never has two kinds of one character nor two destructive ones, the reverb comes last, a shared tail is
added by the scene's chance. Vascular's processor mix is 0.62 wet for every node; here each node's values carry
their own mix where the kind has one.
