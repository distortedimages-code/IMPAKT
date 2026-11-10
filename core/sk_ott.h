// IMPAKT — OTT: the "over the top" three-band upward + downward compressor (after the well-known
// multiband preset: bands split at 88.3 Hz and 2.5 kHz, every band squeezed hard from above and lifted
// from below, then made up). Last effect of the master chain. Own implementation.
#pragma once

#include "sk_math.h"

namespace sk {

struct OttParams
{
    float time;          // 0..1: attack and release, scaled (1 = the preset's times)
    float up, down;      // 0..1: upward / downward depth (1 = the preset's ratios)
    float lo, mid, hi;   // 0..1: band gains, 0.5 = the preset's make-up (±12 dB)
    float out;           // 0..1: output gain, 0.5 = 0 dB (±12 dB)
    float mix;           // 0..1: dry / wet
};

struct OttBiquad
{
    float b0, b1, b2, a1, a2, z1, z2;    // no initialisers: the engine is a zero-initialised static
    inline float tick (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0.0f; }
};

class Ott
{
public:
    void init (float sr);
    void reset();
    void process (float* L, float* R, int n, const OttParams& p);
    float gainDb[3];     // last gain change per band (dB, + = lifted), for the screen

private:
    float sr;
    // per channel: low = LR4 low-pass at f1 then an all-pass at f2 (phase match); mid / high from the rest
    OttBiquad lp1[2][2], hp1[2][2], lp2[2][2], hp2[2][2], ap2[2];
    OttBiquad dap[2][2]; // the dry path: the same two all-passes as the three bands summed
    float env[3];        // band mean squares, linked stereo
};

} // namespace sk
