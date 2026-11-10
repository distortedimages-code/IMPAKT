// IMPAKT — real FFT (radix-2, split real/imag arrays), no allocation.
#pragma once
#if defined (__clang__)
 #pragma clang diagnostic ignored "-Wsign-conversion"   // fixed-size arrays sized by int template parameters
#elif defined (__GNUC__)
 #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "sk_math.h"

namespace sk {

// Real FFT of size n (power of two, n <= MAXN). Forward is unnormalised;
// inverse includes the 1/n factor so inverse(forward(x)) == x.
template <int MAXN> struct RFFT
{
    int n, h;
    float ct[MAXN / 4 + 1], st[MAXN / 4 + 1];   // complex FFT twiddles (size h)
    float pc[MAXN / 2 + 1], ps[MAXN / 2 + 1];   // real post-processing twiddles (size n)
    int   rev[MAXN / 2];
    float wr[MAXN / 2], wi[MAXN / 2];           // work buffers
    // per-stage twiddles laid out contiguously (stage with half-length `half` starts at half - 1):
    // the same values as the ct / st lookup, so the butterflies give the same results, in an order
    // that runs through memory linearly (and vectorises)
    float tc[MAXN / 2], tsF[MAXN / 2], tsI[MAXN / 2];

    void init (int size)
    {
        n = size; h = size / 2;
        for (int k = 0; k < h / 2 + 1 && k <= MAXN / 4; ++k) { ct[k] = cos2pi ((float) k / (float) h); st[k] = sin2pi ((float) k / (float) h); }
        for (int k = 0; k <= h; ++k) { pc[k] = cos2pi ((float) k / (float) n); ps[k] = sin2pi ((float) k / (float) n); }
        int bits = 0; while ((1 << bits) < h) ++bits;
        for (int i = 0; i < h; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[i] = r;
        }
        const int q = h / 4;
        for (int len = 2; len <= h; len <<= 1)
        {
            const int half = len >> 1, step = h / len;
            for (int j = 0; j < half; ++j)
            {
                // twiddle e^{sign * 2pi i j / len} = cos(2pi m/h) + i sign sin(2pi m/h), m = j*step < h/2
                const int m = j * step;
                float c, s;
                if (m <= q) { c = ct[m]; s = st[m]; }
                else { c = -ct[h / 2 - m]; s = st[h / 2 - m]; }
                tc[half - 1 + j] = c; tsF[half - 1 + j] = -s; tsI[half - 1 + j] = s;
            }
        }
    }

    // in-place complex FFT of size h on (wr, wi); sign = -1 forward, +1 inverse (unnormalised)
    void cfft (float sign)
    {
        for (int i = 0; i < h; ++i)
        {
            int j = rev[i];
            if (j > i) { float t = wr[i]; wr[i] = wr[j]; wr[j] = t; t = wi[i]; wi[i] = wi[j]; wi[j] = t; }
        }
        const float* S0 = sign < 0.0f ? tsF : tsI;
        // first stage: one twiddle (cos2pi (0), sin2pi (0) from the tables, kept as such)
        {
            const float c = tc[0], s = S0[0];
            for (int i = 0; i + 1 < h; i += 2)
            {
                const float tr = wr[i + 1] * c - wi[i + 1] * s;
                const float ti = wr[i + 1] * s + wi[i + 1] * c;
                wr[i + 1] = wr[i] - tr; wi[i + 1] = wi[i] - ti;
                wr[i] += tr;            wi[i] += ti;
            }
        }
        for (int len = 4; len <= h; len <<= 1)
        {
            const int half = len >> 1;
            const float* __restrict C = tc + half - 1;
            const float* __restrict S = S0 + half - 1;
            for (int i0 = 0; i0 < h; i0 += len)
            {
                float* __restrict ar = wr + i0;  float* __restrict ai = wi + i0;
                float* __restrict br = ar + half; float* __restrict bi = ai + half;
                for (int j = 0; j < half; ++j)
                {
                    const float c = C[j], s = S[j];
                    const float tr = br[j] * c - bi[j] * s;
                    const float ti = br[j] * s + bi[j] * c;
                    br[j] = ar[j] - tr; bi[j] = ai[j] - ti;
                    ar[j] += tr;        ai[j] += ti;
                }
            }
        }
    }

    // x[n] -> re[h+1], im[h+1]
    void forward (const float* x, float* re, float* im)
    {
        for (int m = 0; m < h; ++m) { wr[m] = x[2 * m]; wi[m] = x[2 * m + 1]; }
        cfft (-1.0f);
        for (int k = 0; k <= h; ++k)
        {
            const int a = k == h ? 0 : k, b = k == 0 ? 0 : h - k;
            const float zr = wr[a], zi = wi[a], cr = wr[b], ci = -wi[b];    // Z[k], conj(Z[h-k])
            const float er = 0.5f * (zr + cr), ei = 0.5f * (zi + ci);       // even part
            const float orr = 0.5f * (zi - ci), oi = -0.5f * (zr - cr);     // odd part = (Z - conjZ)/(2i)
            const float c = pc[k], s = -ps[k];                              // W^k = e^{-2pi i k/n}
            re[k] = er + orr * c - oi * s;
            im[k] = ei + orr * s + oi * c;
        }
    }

    // re[h+1], im[h+1] -> x[n]
    void inverse (const float* re, const float* im, float* x)
    {
        for (int k = 0; k < h; ++k)
        {
            const int b = h - k;
            const float xr = re[k], xi = im[k], cr = re[b], ci = -im[b];    // X[k], conj(X[h-k])
            const float er = 0.5f * (xr + cr), ei = 0.5f * (xi + ci);
            const float dr = 0.5f * (xr - cr), di = 0.5f * (xi - ci);
            const float c = pc[k], s = ps[k];                               // W^-k = e^{+2pi i k/n}
            const float orr = dr * c - di * s, oi = dr * s + di * c;       // Fo = D / W^k
            wr[k] = er - oi; wi[k] = ei + orr;                              // Z = Fe + i Fo
        }
        cfft (1.0f);
        const float g = 1.0f / (float) h;
        for (int m = 0; m < h; ++m) { x[2 * m] = wr[m] * g; x[2 * m + 1] = wi[m] * g; }
    }
};

} // namespace sk
