// IMPAKT — the spring of WEST (v78): ChowDSP's BYOD spring reverb, ported. Its DSP is Jatin Chowdhury's (BYOD,
// src/processors/other/spring_reverb: SpringReverb, SchroederAllpass, ReflectionNetwork; the delay lines'
// interpolations and the first-order shelf from chowdsp_utils), GPL-3.0: core/LICENSE-BYOD.txt. As there:
//
//   rate      half the track's (the original downsamples by 2 too; here through the engine's halfbands)
//   input     tanh (x - feedback x the long delay's way back), a 40 Hz high-pass, the shake's burst
//   tank      16 nested (order 2) Schroeder allpasses, all of one length (0.35 + 3 SIZE ms) and gain (0.5 - 0.4 SPIN),
//             on four lanes: the way forward (+g) and the way back (-g, its last output) of each side
//   delay     the long one (1000 samples + SIZE x 99 ms, moved by CHAOS^3 x a smoothed random share of 7 %), fed by
//             the way back; its feedback for a T60 of 0.5 x 9^(0.95 DECAY - 0.7 (1 - SIZE^2)) seconds
//   early     four delays (0.07 0.17 0.23 0.29 x SIZE seconds) through a Householder matrix, their feedback on T60
//             and REFL, under a shelf (800 Hz, -3 .. -12 dB with DAMP), taken from the way forward
//   out       a low-pass (4 kHz .. 18 kHz as DAMP goes down), what comes out the next way back
//   shake     a half-sine burst (0.5 .. 1 ms x (1 + SIZE / 2), gain 2) into the input: the tank knocked
// Ours: the right side's lengths a few percent longer (a mono voice in, two sides out); the chaos's random target
// drawn every 512 samples (the original: each host block); the shake on WEST's trigs, scaled by SSHK.
#pragma once
#include "sk_math.h"
#include "sk_os.h"

namespace sk {

enum { SB_STAGES = 16, SB_APF = 1024, SB_REFL = 32768, SB_DLY = 16384, SB_LANES = 4 };

struct ByodSpring
{
    // the allpasses: per lane, stage, nesting level (0 the outer, 1 the inner) a line (Thiran), its read state
    float apf[SB_LANES][SB_STAGES][2][SB_APF];
    float apfSt[SB_LANES][SB_STAGES][2];
    int   apfW;
    // the long delay a side (Lagrange 3rd), the reflections' four a side
    float dly[2][SB_DLY];
    float refl[2][4][SB_REFL];
    int   dlyW, reflW;
    // their lengths now (samples at fs): the allpasses' per lane, the delay's and the reflections' per side
    float apfD[SB_LANES], apfA[SB_LANES]; int apfK[SB_LANES];
    float dlyD[2], reflD[2][4];
    float g, fbS[2], reflFb[2][4];
    // the filters: the input's high-pass, the output's low-pass, the reflections' shelf (b0 b1 a1, its state) per side
    Svf   dc[2], lpf[2];
    float shB0, shB1, shA1, shZ[2];
    // the lanes' way back (each side's last output), the chaos's smoothed value and its next target, the shake
    float z[2];
    float chaos, chaosT; int chaosCnt;
    int   shakeLeft, shakeLen; float shakeGain;
    // rates: the track's, the tank's (half); the halfbands down (mono in) and up (each side)
    float sr, fs;
    HalfbandDown<80> down;
    HalfbandUp<80>   up[2];
    u32   rng;
    int   ready;

    void init (float sampleRate);
    // SIZE DECAY REFL SPIN DAMP CHAOS (0 .. 1), every chunk
    void configure (float size, float decay, float refl, float spin, float damp, float chaosAmt);
    // knock the tank (amount 0 .. 1: the burst's gain, 2 at 1)
    void shake (float amount, float size);
    // n samples (even) of mono in, the two sides out (wet only)
    void process (const float* in, float* outL, float* outR, int n);
};

} // namespace sk
