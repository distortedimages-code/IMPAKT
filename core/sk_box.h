// IMPAKT — BOX section: a bank of synthesised impulse responses (toys, speakers, objects,
// cabinets, rooms — sorted from the smallest space to the largest) and a low-latency
// two-stage uniformly-partitioned convolver used as an insert on every track.
#pragma once
#if defined (__clang__)
 #pragma clang diagnostic ignored "-Wsign-conversion"   // fixed-size arrays sized by int template parameters
#elif defined (__GNUC__)
 #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "sk_fft.h"

namespace sk {

enum {
    NUM_IRS   = 24,          // bank entries 1..24 (0 = OFF)
    IR_MAX    = 49152,       // longest IR in samples (~1 s at 48 kHz)
    BOX_NA    = 64,          // stage A block (latency of the wet path)
    BOX_PA    = 8,           // stage A partitions -> first 512 samples
    BOX_NB    = 512,         // stage B block
    BOX_PBMAX = 100,         // stage B partitions
    IR_POOL   = 3700000,     // floats for all IR spectra (forward + reversed)
    PRE_MAX   = 24576        // predelay buffer (250 ms at 96 kHz)
};

struct IrEntry
{
    int   len;               // samples
    int   pa, pb;            // partitions used by stage A / B
    int   offA, offB;        // spectra offsets in the pool (forward)
    int   offAr, offBr;      // reversed IR
    float peak;              // max |H(f)|, used to keep box feedback below unity loop gain
};

struct IrBank
{
    float   sr;
    IrEntry e[NUM_IRS + 1];
    float   pool[IR_POOL];
    int     used;
    float   h[IR_MAX], tmp[IR_MAX], tmp2[IR_MAX], pad[2 * BOX_NB];
    float   sre[8193], sim[8193];
    Rng     rng;
    RFFT<16384> fpk;

    void build (float sampleRate, RFFT<2 * BOX_NA>& fa, RFFT<2 * BOX_NB>& fb);
    int  generate (int id);   // fills h[], returns length
    void store (int id, int L, RFFT<2 * BOX_NA>& fa, RFFT<2 * BOX_NB>& fb);
};

// Uniformly partitioned overlap-save convolver. Output lags input by N samples.
// The multiply-accumulate of older partitions is spread over the block so the
// per-call cost stays flat.
template <int N, int PMAX> struct UConv
{
    float in2[2 * N];
    float out[N];
    float fdlRe[PMAX][N + 1], fdlIm[PMAX][N + 1];
    float accRe[N + 1], accIm[N + 1];
    float yRe[N + 1], yIm[N + 1], time2[2 * N];
    int   pos, last, macDone;
    const float* H; int P, Puse; const float* g;

    void reset (int phase)
    {
        for (int i = 0; i < 2 * N; ++i) in2[i] = 0.0f;
        for (int i = 0; i < N; ++i) out[i] = 0.0f;
        for (int p = 0; p < PMAX; ++p) for (int k = 0; k <= N; ++k) fdlRe[p][k] = fdlIm[p][k] = 0.0f;
        for (int k = 0; k <= N; ++k) accRe[k] = accIm[k] = 0.0f;
        pos = phase % N; last = 0; macDone = 0;
    }

    void setIR (const float* h, int parts, int use, const float* gains)
    {
        H = h; P = parts; Puse = use < parts ? use : parts; g = gains;
        if (Puse < 0) Puse = 0;
        if (macDone > (Puse > 0 ? Puse - 1 : 0)) macDone = Puse > 0 ? Puse - 1 : 0;
    }

    inline void peek (float* o, int n) const { for (int i = 0; i < n; ++i) o[i] = out[pos + i]; }

    inline void mac (int p)
    {
        const int idx = ((last - (p - 1)) % PMAX + PMAX) % PMAX;
        const float* xr = fdlRe[idx]; const float* xi = fdlIm[idx];
        const float* hr = H + p * 2 * (N + 1); const float* hi = hr + (N + 1);
        const float gg = g[p];
        for (int k = 0; k <= N; ++k)
        {
            accRe[k] += gg * (xr[k] * hr[k] - xi[k] * hi[k]);
            accIm[k] += gg * (xr[k] * hi[k] + xi[k] * hr[k]);
        }
    }

    template <class F> void push (const float* x, int n, F& fft)
    {
        for (int i = 0; i < n; ++i) in2[N + pos + i] = x[i];
        pos += n;
        if (Puse > 1)
        {
            int target = ((Puse - 1) * pos) / N;
            while (macDone < target) mac (++macDone);
        }
        if (pos >= N) block (fft);
    }

    template <class F> void block (F& fft)
    {
        while (macDone < Puse - 1) mac (++macDone);
        const int nx = (last + 1) % PMAX;
        fft.forward (in2, fdlRe[nx], fdlIm[nx]);
        last = nx;
        if (Puse > 0)
        {
            const float* hr = H; const float* hi = H + (N + 1);
            const float g0 = g[0];
            for (int k = 0; k <= N; ++k)
            {
                yRe[k] = accRe[k] + g0 * (fdlRe[nx][k] * hr[k] - fdlIm[nx][k] * hi[k]);
                yIm[k] = accIm[k] + g0 * (fdlRe[nx][k] * hi[k] + fdlIm[nx][k] * hr[k]);
            }
            fft.inverse (yRe, yIm, time2);
            for (int i = 0; i < N; ++i) out[i] = time2[N + i];
        }
        else for (int i = 0; i < N; ++i) out[i] = 0.0f;
        for (int i = 0; i < N; ++i) in2[i] = in2[N + i];
        for (int k = 0; k <= N; ++k) accRe[k] = accIm[k] = 0.0f;
        pos = 0; macDone = 0;
    }
};

struct TrackBox
{
    UConv<BOX_NA, BOX_PA>    a;
    UConv<BOX_NB, BOX_PBMAX> b;
    float gA[BOX_PA], gB[BOX_PBMAX];
    float pre[PRE_MAX]; int prew;
    float dry[BOX_NA]; int dryw;       // dry path delayed by the wet latency
    float wetPrev, toneLp, toneHp, fbEnv;
    int   irCur, revCur, phase;
    float dcyCur, gateCur;
    int   quiet, idle, active;
};

} // namespace sk
