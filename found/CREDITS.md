# Found sounds: sources and credits

Norilsk's engine (generation 5, from build 35) reads grains from sixteen short clips of found sound, cut from the
recordings below by `tools/make_found.c` into `firmware/src/found_clips.h`. The clips are 0.6 s each, at 22.05 kHz,
8-bit. Once released they never change.

## Recordings (`found/raw/`)

**NOAA Pacific Marine Environmental Laboratory (PMEL), Acoustics Program.** Hydrophone recordings, published as public
information that may be distributed freely; credit: NOAA PMEL Acoustics Program.

| File | What it is | Source page |
|---|---|---|
| `noaa-iceberg-calving.wav` | an iceberg calving while adrift (sped up 3×) | <https://pmel.noaa.gov/acoustics/specs_cryogenic.html> |
| `noaa-iceberg-tremor.wav` | iceberg harmonic tremor, 40 Hz and overtones (sped up 3×) | <https://pmel.noaa.gov/acoustics/specs_cryogenic.html> |
| `noaa-bloop.wav` | the "Bloop": an icequake, 1997 (sped up 16×) | <https://pmel.noaa.gov/acoustics/specs_cryogenic.html> |
| `noaa-iceberg-grounding.wav` | an iceberg grounding on the seafloor (sped up 16×) | <https://pmel.noaa.gov/acoustics/specs_cryogenic.html> |
| `noaa-challenger-deep-ship.wav` | ship traffic heard by the Challenger Deep hydrophone, June 2020 (sped up 10×) | <https://pmel.noaa.gov/acoustics/multimedia.html> |

**NASA Goddard Space Flight Center: the Polar spacecraft's Plasma Wave Instrument (University of Iowa), 1996.** Radio
and plasma waves in Earth's magnetosphere, made audible; NASA media is generally not subject to copyright in the
United States; credit: NASA / Polar PWI. Their use here implies no endorsement by NASA.

| File | What it is | Source page |
|---|---|---|
| `nasa-polar-auroral-hiss-1.wav`, `nasa-polar-auroral-hiss-2.wav` | auroral hiss | <https://pwg.gsfc.nasa.gov/istp/polar/polar_pwi_sounds.html> |
| `nasa-polar-akr.wav` | auroral kilometric radiation | <https://pwg.gsfc.nasa.gov/istp/polar/polar_pwi_sounds.html> |
| `nasa-polar-chorus.wav` | chorus | <https://pwg.gsfc.nasa.gov/istp/polar/polar_pwi_sounds.html> |
| `nasa-polar-saucer.wav` | "saucer" emissions | <https://pwg.gsfc.nasa.gov/istp/polar/polar_pwi_sounds.html> |

## Rendered (`found/synth/`)

The industrial kinds no free archive gave, rendered offline by `tools/render_found.c` (part of this project, under its
licence): a transformer's hum, an air duct, a furnace, rain on a tin roof, an engine room, wind in wires.
