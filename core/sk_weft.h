// IMPAKT — WEFT (v78): an FX machine on Mutable Instruments' Warps, the meta-modulator (core/mi/warps, MIT, © Emilie
// Gillet): its modulator (Modulator: the saturating input amplifiers, the internal carrier oscillators, the six
// cross-modulation algorithms morphing into each other at six times its rate, the 20-band vocoder) and its "easter
// egg" (the frequency shifter), as released, run at its own rate (96 kHz: twice the track's at 48 kHz, through the
// engine's halfbands; in blocks of 48, its vocoder's bands decimating by 12). The source (SRC) is its modulator (input
// 2); its carrier (input 1) is one of its internal oscillators (SIN TRI SAW; the vocoder's SAW PULSE NOISE) or a track
// (T1 .. T8, taken as an insert as SRC is).
//
//   ALGO   the algorithm, morphing: XFADE, FOLD, ANALOG RING, DIGITAL RING, XOR, COMPARE, then the vocoder (its release
//          shortening, frozen at the top); SHFT: the frequency shift (left down, right up, the middle none)
//   TIMB   the algorithm's parameter; SHFT: the sidebands (down .. up)
//   CARR   the carrier; NOTE (TUNE) the internal one's pitch
//   LVL1   the carrier's drive into its amplifier; SHFT: the feedback
//   LVL2   the modulator's; SHFT: dry .. wet (inside the shifter)
//   MODE   WARP (the meta-modulator) or SHFT (the frequency shifter, in stereo: one sideband a side)
// The whole path (up, the block, its cross-modulation's own resampling, down) is 106 samples late at 48 kHz; the
// direct sound is delayed as much.
#pragma once
#include "sk_math.h"

namespace sk {

enum { WEFT_LAT = 106, WEFT_DRY = 128 };

struct WeftParams
{
    float algo, timb, lvl1, lvl2, note;    // note: MIDI (the internal carrier's)
    int   carrier;                         // 0: a track (external), 1 .. 3: the internal oscillator
    int   shift;                           // the frequency shifter
    int   freeze;                          // (ACT FRZ: the vocoder held)
};

// the unit lives in the FX track's memory (bytes: weftBytes); laid out (and cleared) for this rate
unsigned weftBytes();
void weftInit (void* mem, float sr);
// n samples: the carrier (a track's, mono; unused with the internal one), the modulator (mono), the direct sound (left,
// right); out the effect (left, right) and the direct sound as late as it (left, right)
void weftProcess (void* mem, const WeftParams& p, const float* car, const float* mod, const float* xL, const float* xR,
                  float* wL, float* wR, float* dL, float* dR, int n);
// the screen: the vocoder's level, the algorithm's two neighbours and the blend between them
void weftInfo (void* mem, float* out3);

} // namespace sk
