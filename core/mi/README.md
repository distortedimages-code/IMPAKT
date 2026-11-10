# Mutable Instruments code (MIT)

These folders are copies of Mutable Instruments' open-source code, MIT licence, © Emilie Gillet
(https://github.com/pichenettes/eurorack, https://github.com/pichenettes/stmlib). See LICENSE.txt.
Mutable Instruments is a registered trademark: IMPAKT's UI uses its own names (MACRO, GRAIN, WEFT).

- `plaits/` — the DSP of Plaits 1.2 (all 24 models), used by the MACRO machine (`../sk_macro.cpp`).
- `beads/` — the DSP of Beads (granular processor, delay, reverb, released in October 2026), used by GRAIN, the master effect (`../sk_grain.cpp`).
- `warps/` — the DSP of Warps (the meta-modulator: its six cross-modulation algorithms, vocoder and frequency shifter), used by WEFT, an FX machine (`../sk_weft.cpp`, v78).
- `stmlib/` — the helpers they rely on.
(Marbles, behind the DICE module until v66, left with it in v67.)

Changes from the originals:
- `plaits/dsp/dsp.h`: `kCorrectedSampleRate` equals `kSampleRate` (the engines run at exactly 48 kHz here).
- `plaits/dsp/voice.*` removed: `../sk_macro.cpp` drives the engines (float output, no trigger delay).
- `stmlib/stmlib.h`: defines `TEST` off ARM, so the portable paths are used (no ARM assembly).
- `stmlib/utils/dsp.h`: `__attribute__((always_inline))` behind a macro that is empty under MSVC.
- `beads/`: only `dsp/` (without the synth mode) and `resources.*`; the variable-length arrays of `processor.cc`,
  `granular_processor.h` and `trigger_generator.cc` are fixed-size (`kMaxBlockSize` in `dsp/frame.h`, 32 frames)
  and `ENABLE_HARDWARE_24K` is off in `dsp/processor.h` (the codec runs at 48 kHz, the tape modes decimate inside);
  the two empty tables of `resources.cc` (`lookup_table_i16_table`, `lookup_table_i8_table`), like `lookup_table_i32_table` in `plaits/resources.cc`, hold one null entry, so
  that GCC and MSVC compile them (Beads never reads them).
- `warps/`: only `dsp/` and `resources.*`; `Modulator::Process` and `ProcessEasterEgg` take and give `FloatFrame`
  (full scale 1, clipped there as the codec's 16 bits were; the aux output scaled by 0.5 as before), the saturating
  amplifiers read floats, the debug pin is gone; `vocoder.h`: `kFollowerGain` a literal (sqrt of the 20 bands) rather
  than a static initialiser. Envelope shapes in `../sk_math.h` (`envCurve`, v78) follow Streams' segment tables
  (LINEAR, EXPONENTIAL, QUARTIC) as formulas; no Streams file is copied.
- `compat/`: minimal `<algorithm>` / `<cmath>` / `<cassert>` / ... for the freestanding WebAssembly build only.

The default banks of the three 6-op FM models (`fm_patches_table` in `plaits/resources.cc`) are DX7
patches, some from Yamaha's factory cartridges: they come with the Plaits sources but their rights are not
covered by the MIT licence.
