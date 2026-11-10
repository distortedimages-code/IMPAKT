// IMPAKT — reverb algorithms besides the plate and the spectral reverb:
//  FdnReverb    ROOM / HALL / SPACE: 8-line feedback delay network (Jot & Chaigne 1991) with a
//               Hadamard matrix, per-line absorption filters set from the RT60 and its high-frequency
//               ratio, input diffusion, early reflections (room), modulated lines (hall / space) and
//               an octave-up shimmer in the loop (space).
//  SpringReverb parametric spring tank after Välimäki, Parker & Abel (JAES 2010): a cascade of
//               stretched all-pass filters makes the dispersive "chirp", inside a lossy feedback loop;
//               two tanks of different lengths give the stereo image, DRIP excites the boing.
#pragma once
#if defined (__clang__)
 #pragma clang diagnostic ignored "-Wsign-conversion"
#elif defined (__GNUC__)
 #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "sk_math.h"

namespace sk {

enum { FDN_N = 8, FDN_LEN = 32768, FDN_MASK = FDN_LEN - 1, ER_LEN = 8192, SHIM_LEN = 8192 };
enum { REV_ROOM = 0, REV_PLATE, REV_SPRING, REV_SPECTRAL, REV_HALL, REV_SPACE, NUM_REV_ALGOS };

struct FdnReverb
{
    float sr;
    float line[FDN_N][FDN_LEN]; int w;
    float len[FDN_N], gain[FDN_N], bco[FDN_N], lp[FDN_N], ph[FDN_N], rate[FDN_N];
    float ap[4][2048]; int apLen[4], apW[4];
    float er[ER_LEN]; int erW;
    float sh[SHIM_LEN]; int shW; float shPh, shOut, shLp;
    int   mode;
    float modDepth, diff, early, shimmer, scale;

    void init (float sampleRate);
    void configure (int algo, float time, float size, float damp, float ch1, float ch2);
    void process (const float* in, float* oL, float* oR, int n, float level);
};

enum { SPR_STAGES = 72, SPR_KMAX = 12, SPR_LEN = 16384 };
struct SpringTank
{
    float xh[SPR_STAGES][SPR_KMAX], yh[SPR_STAGES][SPR_KMAX];
    float dl[SPR_LEN]; int dw;
    float fbLp, outLp1, outLp2, hpX, hpY, wander;
};

struct SpringReverb
{
    float sr;
    SpringTank tank[2];
    int   K, hp;
    float a, len[2], g[2], lpC, outC, hpR, drip, emph, rng01;
    Rng   rng;

    void init (float sampleRate);
    void configure (float time, float size, float damp, float tension, float dripAmt);
    void process (const float* in, float* oL, float* oR, int n, float level);
};

} // namespace sk
