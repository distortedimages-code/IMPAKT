// IMPAKT — GRAIN, master effect: the texture processor of Mutable Instruments Beads (MIT, © Emilie Gillet;
// the sources are in mi/beads) on the stereo master bus, after the compressor. The module runs at its own
// codec rate (48 kHz, 32 kHz in VINT) and is resampled to the host rate; its SEED input can take IMPAKT's
// transport. Like MACRO and DICE, the unit lives in a raw block.
#pragma once

namespace sk {

enum { GRAIN_BYTES = 920 * 1024, GRAIN_MEDIA = 4 };

struct GrainControls
{
    float time, size, shape, pitch;   // 0..1 (pitch: the module's PITCH knob, ±24 st with its detents)
    float density;                    // 0..1: CCW regular, 12 o'clock none, CW random (with SYNC: divider / probability)
    float feedback, reverb;           // 0..1
    float rnd;                        // 0..1: randomisation of TIME / SIZE / SHAPE / PITCH from grain to grain
    float mix;                        // 0..1: DRY / WET
    float pitchMod;                   // semitones added to PITCH (the LFO)
    int   media;                      // 0 DIGI (16-bit 48 kHz), 1 VINT (12-bit 32 kHz), 2 TAPE, 3 DIRT (8-bit µ-law, wow)
    int   sync;                       // clock on SEED from the transport: 0 none, else its period in steps (1, 4, 16)
    int   freeze;
};

void grainInit (void* mem, float hostSr);        // mem: GRAIN_BYTES, 16-byte aligned
void grainSetRate (void* mem, float hostSr);
void grainClear (void* mem);                     // empties the buffers
// n host samples of the bus, processed in place (DRY / WET applied). pos: transport position in steps (16ths)
// at the first sample, inc: steps per sample, running: transport on.
void grainProcess (void* mem, const GrainControls& c, float* L, float* R, int n, double pos, double inc, int running);

} // namespace sk
