// IMPAKT — spectral reverb inspired by phase-vocoder modules such as Qu-Bit Aurora.
// STFT (Hann, 75 % overlap). Per bin: magnitude-energy accumulator with frequency-dependent
// decay, instantaneous-frequency tracking, phase randomisation inside the loop for diffusion,
// spectral blur across bins, a spectral multi-tap delay ("reflect"), pitch warp at the output,
// tilt + lossy-codec artefacts ("atmosphere"), freeze and reverse.
#pragma once
#if defined (__clang__)
 #pragma clang diagnostic ignored "-Wsign-conversion"   // fixed-size arrays sized by int template parameters
#elif defined (__GNUC__)
 #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "sk_fft.h"

namespace sk {

enum { SPEC_NMAX = 4096, SPEC_BMAX = SPEC_NMAX / 2 + 1, SPEC_HIST = 128, SPEC_DMAX = 44 };

struct SpecParams { float time, blur, warp, reflect, atmo, freeze, reverse, level; };

struct SpecVerb
{
    int   N, R, B;
    float sr;
    float win[SPEC_NMAX];
    float in[SPEC_NMAX];            // last N input samples (linear, shifted every hop)
    float olaL[SPEC_NMAX], olaR[SPEC_NMAX];
    int   cnt;                      // samples since last hop
    float E[SPEC_BMAX], F[SPEC_BMAX], prevPh[SPEC_BMAX], phL[SPEC_BMAX], phR[SPEC_BMAX];
    float hist[SPEC_HIST][SPEC_BMAX]; int histW, revR, revOn;
    float dly[SPEC_DMAX][SPEC_BMAX];  int dW;
    float re[SPEC_BMAX], im[SPEC_BMAX], fr[SPEC_NMAX], tl[SPEC_BMAX];
    float outGainSmooth, energy;
    int   idle;
    Rng   rng;
    RFFT<SPEC_NMAX> fft;

    void init (float sampleRate);
    void process (const float* inM, float* oL, float* oR, int n, const SpecParams& p);
    void hop (const SpecParams& p);
};

} // namespace sk
