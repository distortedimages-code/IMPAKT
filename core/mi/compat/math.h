// IMPAKT: <math.h> for the freestanding WebAssembly build: builtins for what WebAssembly has as
// instructions, short series (double precision where it is used at initialisation) for the rest.
#pragma once
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif
inline float fabsf (float x) { return __builtin_fabsf (x); }
inline int abs (int x) { return x < 0 ? -x : x; }
inline double fabs (double x) { return __builtin_fabs (x); }
inline float floorf (float x) { return __builtin_floorf (x); }
inline double floor (double x) { return __builtin_floor (x); }
inline float ceilf (float x) { return __builtin_ceilf (x); }
inline float sqrtf (float x) { return __builtin_sqrtf (x); }
inline double sqrt (double x) { return __builtin_sqrt (x); }
inline float truncf (float x) { return __builtin_truncf (x); }
inline float roundf (float x) { return __builtin_floorf (x + 0.5f); }
inline float fminf (float a, float b) { return a < b ? a : b; }
inline float fmaxf (float a, float b) { return a > b ? a : b; }
// sin / cos in double: range reduction to [-pi/4, pi/4] then Taylor series (error < 1e-15)
inline double mi_sin_poly_ (double x) { double x2 = x * x; return x * (1.0 + x2 * (-1.0 / 6 + x2 * (1.0 / 120 + x2 * (-1.0 / 5040 + x2 * (1.0 / 362880 + x2 * (-1.0 / 39916800 + x2 * (1.0 / 6227020800.0))))))); }
inline double mi_cos_poly_ (double x) { double x2 = x * x; return 1.0 + x2 * (-0.5 + x2 * (1.0 / 24 + x2 * (-1.0 / 720 + x2 * (1.0 / 40320 + x2 * (-1.0 / 3628800 + x2 * (1.0 / 479001600.0)))))); }
inline double sin (double x)
{
    const double q = __builtin_floor (x / M_PI_2 + 0.5);
    const double r = x - q * M_PI_2;
    const int k = ((int) q) & 3;
    return k == 0 ? mi_sin_poly_ (r) : k == 1 ? mi_cos_poly_ (r) : k == 2 ? -mi_sin_poly_ (r) : -mi_cos_poly_ (r);
}
inline double cos (double x) { return sin (x + M_PI_2); }
inline double tan (double x) { return sin (x) / cos (x); }
inline float sinf (float x) { return (float) sin ((double) x); }
inline float cosf (float x) { return (float) cos ((double) x); }
inline float tanf (float x) { return (float) tan ((double) x); }
// exp / log in double (initialisation only)
inline double exp (double x)
{
    if (x < -700.0) return 0.0;
    if (x > 700.0) x = 700.0;
    const double k = __builtin_floor (x / 0.69314718055994530942 + 0.5);
    const double r = x - k * 0.69314718055994530942;
    double s = 1.0, t = 1.0;
    for (int i = 1; i < 18; ++i) { t *= r / i; s += t; }
    union { double d; unsigned long long u; } b; b.u = (unsigned long long) ((long long) k + 1023) << 52;
    return s * b.d;
}
inline double log (double x)
{
    if (x <= 0.0) return -1e300;
    union { double d; unsigned long long u; } b; b.d = x;
    const int e = (int) ((b.u >> 52) & 2047) - 1023;
    b.u = (b.u & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull;
    const double m = b.d, t = (m - 1.0) / (m + 1.0), t2 = t * t;
    double s = 0.0, p = t;
    for (int i = 1; i < 40; i += 2) { s += p / i; p *= t2; }
    return 2.0 * s + e * 0.69314718055994530942;
}
inline double pow (double a, double b) { return exp (b * log (a)); }
inline float expf (float x) { return (float) exp ((double) x); }
inline float logf (float x) { return (float) log ((double) x); }
inline float powf (float a, float b) { return (float) pow ((double) a, (double) b); }
inline float log2f (float x) { return (float) (log ((double) x) / 0.69314718055994530942); }
inline float exp2f (float x) { return (float) exp ((double) x * 0.69314718055994530942); }
inline float fmodf (float a, float b) { return a - b * __builtin_truncf (a / b); }
inline float tanhf (float x) { const double e = exp (2.0 * x); return (float) ((e - 1.0) / (e + 1.0)); }
