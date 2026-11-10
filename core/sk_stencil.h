// IMPAKT — STENCIL (v83): a stereo delay of zones, an homage to the Mimeophon (Make Noise), our VERB in its repeats.
// Own implementation, from what the instrument publicly does (its manual), not from its code.
//
// Two lines (left and right) of up to 5.4 s at 48 kHz. ZONE picks the range RATE moves in, from strings plucked in a
// comb (KARP: 0.3 .. 12 ms, RATE as a pitch, a trig's NOTE transposing it) to a long wash (HUGE: up to 5.4 s); RATE
// is a speed (to the right faster), glided like a tape's (its pitch bends when it moves). REPT up to 125 %: past
// 100 % the loop runs away on its own, held by its saturation; with NOIS (the hiss of the loop) it starts from
// nothing, with no source at all. COLR, in the loop: to the left a dark, saturated tape with its head bump, to the
// right a thin, clean, bright line. HALO: the repeats through an eight-line network (VERB's design, after Tom Erbe's
// Erbe-Verb): a little, a smear; all of it, each repeat a reverb, the next one more. SKEW the right line's time
// against the left's (on SYNC: in musical ratios), PING the repeats crossing sides. µ a drift (in the short zones a
// flanger, in the long ones a tape's wow). SYNC: on the tempo's divisions (in KARP and COMB: on semitones). The
// gestures (ACT): IN (a trig opens the input), HOLD (the loop held at unity, untouched), FLIP (played backwards),
// PLUK (a trig plucks the loop: a burst as long as a period, a string in KARP).
#pragma once

#include "sk_math.h"

namespace sk {

enum { STC_ZONES = 8, STC_ACTS = 4, STC_NDIV = 18, STC_NSKEW = 7, STC_HL = 8, STC_INFO = 7 };

struct StencilParams
{
    int    zone;                 // 0 .. 7
    float  rate;                 // 0 .. 1 (to the right faster: shorter)
    float  rept;                 // 0 .. 1 (0 .. 125 %)
    float  color;                // -1 .. 1 (dark .. bright)
    float  mu, muSpd;            // the drift's depth, its speed
    float  halo;                 // the reverb in the repeats
    float  skew;                 // -1 .. 1
    float  ping, nois;
    int    sync;
    float  hSize, hDcy, hDamp, hDrift;
    float  duck, wide;
    float  hold, flip;           // the gestures, glided (0 .. 1)
    int    pluck; float pluckVel; // a trig on PLUK
    float  note;                 // the last trig's NOTE (semitones: KARP and COMB)
    double spt;                  // seconds a 16th
};

unsigned stencilBytes();
void  stencilInit (float* mem, float sr);
void  stencilProcess (float* mem, const StencilParams& p, const float* xL, const float* xR, const float* g, float* wL, float* wR, int n);
void  stencilInfo (const float* mem, float* out);          // STC_INFO values
// the left line's time (s) for these settings (the page shows the same)
float stencilTime (int zone, float rate, int sync, double spt, float note);
// the right line's against the left's
float stencilSkew (float skew, int sync);

} // namespace sk
