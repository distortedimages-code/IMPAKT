// IMPAKT — fast, dependency-free math for the shared DSP core.
// Compiles freestanding (WebAssembly, no libc) and natively (JUCE).
#pragma once

namespace sk {

typedef unsigned int u32;
typedef int i32;
typedef unsigned char u8;

static const float PI     = 3.14159265358979f;
static const float TWO_PI = 6.28318530717959f;
static const float LN2    = 0.69314718055995f;
static const float LOG2E  = 1.44269504088896f;

inline float absf (float x)                   { return __builtin_fabsf (x); }
inline float floorf_ (float x)                { return __builtin_floorf (x); }
inline float sqrtf_ (float x)                 { return __builtin_sqrtf (x); }
inline float minf (float a, float b)          { return a < b ? a : b; }
inline float maxf (float a, float b)          { return a > b ? a : b; }
inline int   mini (int a, int b)              { return a < b ? a : b; }
inline int   maxi (int a, int b)              { return a > b ? a : b; }
inline float clampf (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
// index of a stepped control: 0..1 split into n equal ranges
inline int   idxOf (float v, int n) { int i = (int) (v * (float) n); return i < 0 ? 0 : (i >= n ? n - 1 : i); }

union FI { float f; i32 i; u32 u; };

// 2^x, ~1e-7 relative error
inline float exp2f_ (float x)
{
    if (x < -126.0f) return 0.0f;
    if (x > 126.0f) x = 126.0f;
    float fl = floorf_ (x + 0.5f);
    float f  = x - fl; // [-0.5, 0.5)
    float p = 1.0f + f * (0.69314718f + f * (0.24022651f + f * (0.05550411f
              + f * (0.00961813f + f * (0.00133336f + f * 0.00015404f)))));
    FI u; u.i = ((i32) fl + 127) << 23;
    return p * u.f;
}

inline float log2f_ (float x)
{
    if (x <= 1e-30f) return -100.0f;
    FI u; u.f = x;
    float e = (float) (((u.i >> 23) & 255) - 127);
    u.i = (u.i & 0x007FFFFF) | 0x3F800000; // mantissa in [1,2)
    float m = u.f;
    float t = (m - 1.0f) / (m + 1.0f);
    float t2 = t * t;
    float ln = 2.0f * t * (1.0f + t2 * (0.33333333f + t2 * (0.2f + t2 * (0.14285714f + t2 * 0.11111111f))));
    return e + ln * LOG2E;
}

inline float expf_ (float x)            { return exp2f_ (x * LOG2E); }
inline float powf_ (float a, float b)   { return exp2f_ (b * log2f_ (a)); }
inline float semis (float st)           { return exp2f_ (st * (1.0f / 12.0f)); }

// sin(2*pi*x) with x in cycles. Max error ~ 1e-7.
inline float sin2pi (float x)
{
    x = x - floorf_ (x + 0.5f); // [-0.5, 0.5)
    if (x > 0.25f) x = 0.5f - x;
    else if (x < -0.25f) x = -0.5f - x;
    float y = x * TWO_PI;
    float y2 = y * y;
    return y * (1.0f + y2 * (-0.16666667f + y2 * (0.0083333333f + y2 * (-0.00019841270f
              + y2 * (2.7557319e-6f + y2 * (-2.5052108e-8f))))));
}
inline float cos2pi (float x) { return sin2pi (x + 0.25f); }

// tan(pi * f) for f in [0, 0.5)
inline float tanpi (float f)
{
    float h = f * 0.5f;
    return sin2pi (h) / maxf (cos2pi (h), 1e-6f);
}

inline float tanhf_ (float x)
{
    if (x > 9.0f) return 1.0f;
    if (x < -9.0f) return -1.0f;
    float e = exp2f_ (2.0f * x * LOG2E);
    return (e - 1.0f) / (e + 1.0f);
}

// cheap rational soft clip (no exp), smooth up to |x| = 3
inline float softclip (float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// v78: an envelope's curve (ECRV), after the segment shapes of Mutable Instruments' Streams (MIT: its tables LINEAR,
// EXPONENTIAL 1 - e^-4t and QUARTIC t^3.32). c at its top (EXP): the attack a straight ramp, the decay exponential, as
// they always were; toward LIN (0.5) the decay straightens into a line (done at half its time to -60 dB); toward LOG
// (0) the attack rounds off (Streams' EXPONENTIAL rise: quick, then slow) and the decay holds before it falls
// (QUARTIC, done at 0.4 of that time). x the envelope's state: stage 0 its attack's ramp (0 .. 1), else its level
// (1 at the top of the decay, falling exponentially)
inline float envCurve (float x, int stage, float c)
{
    if (c >= 0.9995f || x <= 0.0f) return x;
    if (stage == 0)
    {
        if (c >= 0.5f) return x;
        const float r = (1.0f - expf_ (-4.0f * x)) * 1.0186573f;
        return x + (r - x) * (1.0f - 2.0f * c);
    }
    if (x >= 1.0f) return 1.0f;
    const float t = -log2f_ (x) * (1.0f / 9.9657843f);           // how far to -60 dB (0 .. 1)
    const float lin = 1.0f - 2.0f * t > 0.0f ? 1.0f - 2.0f * t : 0.0f;
    if (c >= 0.5f) return lin + (x - lin) * (2.0f * c - 1.0f);
    const float u = 2.5f * t < 1.0f ? 2.5f * t : 1.0f;
    const float q = 1.0f - powf_ (u, 3.32f);
    return q + (lin - q) * (2.0f * c);
}

// coefficient for an exponential decay reaching -60 dB after `sec`
inline float decayCoef (float sec, float sr)
{
    return expf_ (-6.9077553f / (maxf (sec, 0.0001f) * sr));
}

// coefficient for a one-pole time constant `sec`
inline float tcCoef (float sec, float sr)
{
    return expf_ (-1.0f / (maxf (sec, 0.00001f) * sr));
}

inline float polyblep (float t, float dt)
{
    if (t < dt)            { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt)     { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

inline float wrap01 (float x) { return x - floorf_ (x); }

inline bool isBad (float x) { return __builtin_isnan (x) || x > 1e8f || x < -1e8f; }

struct Rng
{
    u32 s;
    void seed (u32 v) { s = v ? v : 0x1234567u; }
    u32 next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float bi()  { return (float) (i32) (next() >> 8) * (1.0f / 8388608.0f) - 1.0f; } // [-1,1)
    float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }             // [0,1)
};

// Cytomic / Andy Simper trapezoidal state-variable filter
struct Svf
{
    float ic1, ic2, a1, a2, a3, k;
    void reset() { ic1 = ic2 = 0.0f; }
    void set (float fc, float q, float sr)
    {
        float f = clampf (fc / sr, 0.00001f, 0.49f);
        float g = tanpi (f);
        k  = 1.0f / maxf (q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline void tick (float x, float& lp, float& bp, float& hp)
    {
        float v3 = x - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    void sanitize() { if (isBad (ic1) || isBad (ic2)) reset(); }
};

} // namespace sk
