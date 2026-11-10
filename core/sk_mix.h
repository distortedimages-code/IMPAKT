// IMPAKT — mix helpers that run in the background so the kit always sits well.
//  Unmasker  per-track dynamic EQ (10 bands, ~0.8 octave apart). Every 10 ms each track's spectrum is
//            measured (FFT 1024); in each band the dominant track (with a priority for the kick in
//            the lows, snares in the body and hats in the air) keeps its level and the tracks it
//            collides with are pulled down a little, more when the collision is closer. Works like a
//            spectral sidechain: the bass only dips under the kick in the sub band, etc. Zero latency
//            (minimum-phase bells driven by the previous analysis frame).
//  Calm      dynamic resonance suppressor on the master input, in the spirit of oeksound's soothe:
//            the spectrum (FFT 2048) is compared with its own smoothed envelope; bins that stick out
//            (resonances, harsh peaks) are tracked by up to 8 dynamic notch / bell filters whose
//            frequency, width and depth follow them. DEPTH, SHARP (width), SELECT (how much a peak
//            must stick out), SPEED (attack / release), FOCUS (frequency range), DELTA (listen to
//            what is removed). Zero latency as well.
#pragma once
#if defined (__clang__)
 #pragma clang diagnostic ignored "-Wsign-conversion"
#elif defined (__GNUC__)
 #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "sk_fft.h"

namespace sk {

// RBJ peaking filter, transposed direct form II
struct Bell
{
    float b0, b1, b2, a1, a2, z1, z2;
    void reset() { z1 = z2 = 0.0f; }
    void set (float cw, float sw, float q, float db)
    {
        const float A = exp2f_ (db * 0.08304820f);                 // 10^(db/40)
        const float al = sw / (2.0f * q);
        const float ia0 = 1.0f / (1.0f + al / A);
        b0 = (1.0f + al * A) * ia0; b1 = -2.0f * cw * ia0; b2 = (1.0f - al * A) * ia0;
        a1 = b1; a2 = (1.0f - al / A) * ia0;
    }
    inline float tick (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

enum { UM_TRACKS = 8, UM_BANDS = 10, UM_N = 1024, UM_MASK = UM_N - 1, UM_HOP = 768 };
enum { ROLE_OTHER = 0, ROLE_KICK, ROLE_SNARE, ROLE_HAT };

struct Unmasker
{
    float sr;
    float buf[UM_TRACKS][UM_N]; int w, hop;
    float win[UM_N], fr[UM_N], re[UM_N / 2 + 1], im[UM_N / 2 + 1];
    float E[UM_TRACKS][UM_BANDS];
    float gT[UM_TRACKS][UM_BANDS], gC[UM_TRACKS][UM_BANDS], gA[UM_TRACKS][UM_BANDS];
    float cw[UM_BANDS], sw[UM_BANDS];
    int   k0[UM_BANDS], k1[UM_BANDS];
    int   top[UM_BANDS];
    float act[UM_TRACKS];          // energy of the last hop (skip silent tracks)
    Bell  bell[UM_TRACKS][UM_BANDS];
    RFFT<UM_N> fft;

    void init (float sampleRate);
    void push (int t, const float* x, int n);
    void advance (int n, const int* role, float depth, float kickPrio);
    void apply (int t, float* x, int n);
};

enum { CALM_N = 2048, CALM_MASK = CALM_N - 1, CALM_HOP = 1024, CALM_K = 8, CALM_B = CALM_N / 2 + 1 };

struct CalmParams { float depth, sharp, select, speed, focus, delta; };

struct Calm
{
    struct Slot { float f, g, q, ft, gt, qt, ap; Bell b[2]; };
    float sr;
    float buf[CALM_N]; int w, hop;
    float win[CALM_N], fr[CALM_N], re[CALM_B], im[CALM_B];
    float pdb[CALM_B], env[CALM_B], ex[CALM_B], psm[CALM_B];
    double cum[CALM_B + 1];
    Slot  s[CALM_K];
    float dry[2][64];
    RFFT<CALM_N> fft;

    void init (float sampleRate);
    void analyse (const CalmParams& p);
    void process (float* L, float* R, int n, const CalmParams& p);
};

} // namespace sk
