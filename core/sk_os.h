// IMPAKT — 4x oversampling for the master section: two cascaded linear-phase halfband FIR
// stages (Kaiser windowed sinc, ~110 dB image / alias rejection, flat to 20 kHz at 44.1 kHz).
// Polyphase form: one branch is a pure delay, the other a symmetric FIR on the odd taps.
#pragma once
#if defined (__clang__)
 #pragma clang diagnostic ignored "-Wsign-conversion"
#elif defined (__GNUC__)
 #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "sk_math.h"
#if defined (__wasm_simd128__)
 #include <wasm_simd128.h>
#endif

namespace sk {

static const float HB1[80] = { -3.4302662808e-07f, 1.2230258064e-06f, -2.9325272476e-06f, 5.9231358573e-06f, -1.0801151426e-05f, 1.8358646654e-05f, -2.9606499893e-05f, 4.5808810835e-05f, -6.8518074852e-05f, 9.9610489302e-05f, -1.4132082835e-04f, 1.9627646902e-04f, -2.6753039997e-04f, 3.5859342010e-04f, -4.7346626624e-04f, 6.1667313949e-04f, -7.9329908469e-04f, 1.0090350032e-03f, -1.2702358745e-03f, 1.5840002260e-03f, -1.9582823485e-03f, 2.4020537087e-03f, -2.9255373260e-03f, 3.5405499596e-03f, -4.2610042520e-03f, 5.1036507966e-03f, -6.0891862680e-03f, 7.2439330146e-03f, -8.6024366917e-03f, 1.0211590673e-02f, -1.2137406553e-02f, 1.4476601407e-02f, -1.7377485796e-02f, 2.1080153294e-02f, -2.6000517414e-02f, 3.2926377862e-02f, -4.3549542685e-02f, 6.2274919780e-02f, -1.0526570291e-01f, 3.1802980744e-01f, 3.1802980744e-01f, -1.0526570291e-01f, 6.2274919780e-02f, -4.3549542685e-02f, 3.2926377862e-02f, -2.6000517414e-02f, 2.1080153294e-02f, -1.7377485796e-02f, 1.4476601407e-02f, -1.2137406553e-02f, 1.0211590673e-02f, -8.6024366917e-03f, 7.2439330146e-03f, -6.0891862680e-03f, 5.1036507966e-03f, -4.2610042520e-03f, 3.5405499596e-03f, -2.9255373260e-03f, 2.4020537087e-03f, -1.9582823485e-03f, 1.5840002260e-03f, -1.2702358745e-03f, 1.0090350032e-03f, -7.9329908469e-04f, 6.1667313949e-04f, -4.7346626624e-04f, 3.5859342010e-04f, -2.6753039997e-04f, 1.9627646902e-04f, -1.4132082835e-04f, 9.9610489302e-05f, -6.8518074852e-05f, 4.5808810835e-05f, -2.9606499893e-05f, 1.8358646654e-05f, -1.0801151426e-05f, 5.9231358573e-06f, -2.9325272476e-06f, 1.2230258064e-06f, -3.4302662808e-07f };   // h[2i], N = 159, Kaiser beta 11.5, center tap 0.5
static const float HB2[16] = { -1.8066070661e-06f, 1.1033596938e-04f, -8.8900458062e-04f, 3.9765251269e-03f, -1.2820918463e-02f, 3.3989991494e-02f, -8.4988738077e-02f, 3.1062357757e-01f, 3.1062357757e-01f, -8.4988738077e-02f, 3.3989991494e-02f, -1.2820918463e-02f, 3.9765251269e-03f, -8.8900458062e-04f, 1.1033596938e-04f, -1.8066070661e-06f };   // h[2i], N = 31, Kaiser beta 11.5, center tap 0.5

// sum_i h[i] p[-i] for a symmetric h (h[i] == h[NH-1-i]): half the multiplies, four accumulators
#if defined (__wasm_simd128__)
// WebAssembly SIMD: the four accumulators are the four lanes, so the sum is bit-identical to the scalar one
template <int NH> inline float symDotT (const float* h, const float* p)
{
    static_assert ((NH / 2) % 4 == 0, "four taps per vector");
    const float* q = p - (NH - 1);
    v128_t acc = wasm_f32x4_splat (0.0f);
    for (int i = 0; i < NH / 2; i += 4)
    {
        v128_t pr = wasm_v128_load (p - i - 3);                      // p[-i-3] .. p[-i]
        pr = wasm_i32x4_shuffle (pr, pr, 3, 2, 1, 0);                 // p[-i] .. p[-i-3]
        acc = wasm_f32x4_add (acc, wasm_f32x4_mul (wasm_v128_load (h + i), wasm_f32x4_add (pr, wasm_v128_load (q + i))));
    }
    return (wasm_f32x4_extract_lane (acc, 0) + wasm_f32x4_extract_lane (acc, 1))
         + (wasm_f32x4_extract_lane (acc, 2) + wasm_f32x4_extract_lane (acc, 3));
}
#else
template <int NH> inline float symDotT (const float* h, const float* p)
{
    float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    const float* q = p - (NH - 1);
    int i = 0;
    for (; i + 4 <= NH / 2; i += 4)
    {
        a0 += h[i]     * (p[-i]     + q[i]);
        a1 += h[i + 1] * (p[-i - 1] + q[i + 1]);
        a2 += h[i + 2] * (p[-i - 2] + q[i + 2]);
        a3 += h[i + 3] * (p[-i - 3] + q[i + 3]);
    }
    if constexpr ((NH / 2) % 4 != 0)
        for (; i < NH / 2; ++i) a0 += h[i] * (p[-i] + q[i]);
    return (a0 + a1) + (a2 + a3);
}
#endif

// NH = number of taps in the FIR branch (h[0], h[2], ... h[N-1]); the centre tap 0.5 is the delay branch.
template <int NH> struct HalfbandUp
{
    const float* h;
    static inline float symDot (const float* hh, const float* p) { return symDotT<NH> (hh, p); }
    float buf[2 * NH]; int w;

    void reset (const float* taps) { h = taps; for (int i = 0; i < 2 * NH; ++i) buf[i] = 0.0f; w = 0; }
    // one input sample -> two output samples
    inline void process (float x, float& y0, float& y1)
    {
        buf[w] = x; buf[w + NH] = x;
        const float* p = buf + w + NH;         // p[0] = x[n], p[-i] = x[n - i]
        y0 = 2.0f * symDot (h, p);
        y1 = p[-(NH / 2 - 1)];
        if (++w >= NH) w = 0;
    }
};

template <int NH> struct HalfbandDown
{
    const float* h;
    static inline float symDot (const float* hh, const float* p) { return symDotT<NH> (hh, p); }
    float odd[2 * NH]; int w;
    float even[NH]; int we;

    void reset (const float* taps) { h = taps; for (int i = 0; i < 2 * NH; ++i) odd[i] = 0.0f; for (int i = 0; i < NH; ++i) even[i] = 0.0f; w = we = 0; }
    // two input samples (v[2n], v[2n+1]) -> one output sample
    inline float process (float v0, float v1)
    {
        odd[w] = v1; odd[w + NH] = v1;
        const float* p = odd + w + NH;
        const float acc = symDot (h, p);
        even[we] = v0;
        int r = we - (NH / 2 - 1); if (r < 0) r += NH;
        const float d = even[r];
        if (++we >= NH) we = 0;
        if (++w >= NH) w = 0;
        return acc + 0.5f * d;
    }
};

// one channel, 4x: base rate <-> 4 samples. Up then down delays the signal by 2 x (79 / 2 + 15 / 4) = 86.5 samples
// (linear phase: each halfband adds NH - 1 samples of its higher rate)
enum { OS4_DELAY_X2 = 173 };          // twice the delay of one round trip, in base samples
struct Oversampler4
{
    HalfbandUp<80>   u1; HalfbandUp<16>   u2;
    HalfbandDown<16> d2; HalfbandDown<80> d1;

    void reset() { u1.reset (HB1); u2.reset (HB2); d2.reset (HB2); d1.reset (HB1); }
    inline void up (float x, float* o4)
    {
        float a, b;
        u1.process (x, a, b);
        u2.process (a, o4[0], o4[1]);
        u2.process (b, o4[2], o4[3]);
    }
    inline float down (const float* i4)
    {
        const float a = d2.process (i4[0], i4[1]);
        const float b = d2.process (i4[2], i4[3]);
        return d1.process (a, b);
    }
};

} // namespace sk
