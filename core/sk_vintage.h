// IMPAKT — vintage character
//  Ms20Filter  Korg35 (the MS-20 mk1 filter chip) as a zero-delay-feedback model after Pirkle
//              ("Virtual Analog Korg35 Lowpass / Highpass Filter", AN-5 / AN-7): three TPT one-poles,
//              the feedback loop solved without unit delay, a tanh saturator inside the loop.
//              LP  H(s) = 1 / (s^2 + (2 - K) s + 1)          (passband stays at 0 dB as K rises)
//              HP  H(s) = s (1 + s) / (s^2 + (2 - K) s + 1)
//              BP  the HP into a one-pole low-pass at the same cutoff: s / (s^2 + (2 - K) s + 1)
//              K = 2 is the self-oscillation point.
//  sp12        SP-1200 style sample playback: a 26.04 kHz clock, drop-sample (no interpolation),
//              12-bit words, zero-order hold, then a gentle fixed output filter.
//  Dac         output converter models: multibit R-2R ladder with resistor tolerances (bit-weight
//              errors, i.e. the zero-crossing distortion of early converters), mu-law companding for
//              the 8-bit model, sample-and-hold at the converter rate, reconstruction filter and an
//              analog output stage (even-order curvature, hiss, coupling capacitor).
//  qPitch      scale quantiser on a 12-bit pitch-class mask.
#pragma once
#include "sk_math.h"

namespace sk {

// --------------------------------------------------------------------------- MS-20 / Korg35
struct Tpt1
{
    float z;
    inline float lp (float x, float G) { const float v = (x - z) * G; const float y = v + z; z = y + v; return y; }
};

enum { MS_LP = 0, MS_BP, MS_HP };

struct Ms20Filter
{
    Tpt1 a, b, c, d;           // LP: a = input LP, b = LP in the loop, c = HP in the feedback
                               // HP: a = input HP, b = HP in the feedback, c = LP in the feedback, d = BP output pole
    float G, g1, K, alpha0;    // g1 = 1 / (1 + g)
    void reset() { a.z = b.z = c.z = d.z = 0.0f; }
    void set (float fc, float k, float sr)
    {
        const float g = tanpi (clampf (fc / sr, 1e-5f, 0.45f));
        G = g / (1.0f + g); g1 = 1.0f / (1.0f + g);
        K = k;
        alpha0 = 1.0f / (1.0f - K * G + K * G * G);
    }
    // the loop saturator: unity gain for small signals, soft limit around +-1.2
    static inline float sat (float x) { return 1.2f * tanhf_ (x * (1.0f / 1.2f)); }
    inline float tick (float x, int mode)
    {
        if (mode == MS_LP)
        {
            const float y1 = a.lp (x, G);
            const float s = K * b.z * g1 * g1 - c.z * g1;              // feedback taps (Pirkle's S35)
            const float u = sat (alpha0 * (y1 + s));
            const float y = b.lp (u, G);
            const float fb = K * y;
            c.lp (fb, G);                                                // the feedback high-pass (state only)
            return y;
        }
        // high-pass / band-pass
        const float y1 = x - a.lp (x, G);
        const float s = c.z * g1 - G * b.z * g1;
        const float u = sat (alpha0 * (y1 + s));
        const float yk = K * u;
        const float h = yk - b.lp (yk, G);
        c.lp (h, G);
        if (mode == MS_HP) return u;
        return d.lp (u, G) * (2.0f - 0.85f * K);
    }
    void sanitize()
    {
        if (isBad (a.z) || isBad (b.z) || isBad (c.z) || isBad (d.z)) reset();
    }
};

// --------------------------------------------------------------------------- SP-1200
enum { SP_RATE = 26040 };
inline float q12 (float x)
{
    x = clampf (x, -1.0f, 0.99951172f);
    return floorf_ (x * 2048.0f + 0.5f) * (1.0f / 2048.0f);
}

// --------------------------------------------------------------------------- DAC
enum { DAC_OFF = 0, DAC_CD80, DAC_PCM14, DAC_SP12, DAC_LO8, NUM_DAC_MODELS };

struct Dac
{
    float sr;
    int   model; float age;
    float w[2][16];            // bit weights with tolerance errors (per channel: two converters)
    float lut[2][2][256];      // ladder output after the sign bit and the low byte, summed in the same order
    int   bits, nLow;
    float rate, ph, hold[2];
    float lp1[2], lp2[2], lpC, lp2C;
    float dcx[2], dcy[2], dcR;
    float noise, curve;
    Rng   rng;

    void init (float sampleRate);
    void configure (int m, float a);
    void process (float* L, float* R, int n);
    void processMono (float* x, int n);      // the first converter only (the RAM machine's voices)
    void reset();                            // held word, filters and coupling back to rest (the model stays)
};

// pitch quantiser: st = semitone offset from base frequency baseHz; the result snaps the absolute
// pitch to the nearest pitch class allowed by mask (bit 0 = C ... bit 11 = B). mask 0 = no change.
float qPitch (float st, float baseHz, u32 mask);
float qInterval (float st, int semi, float baseHz, u32 mask);   // an interval in steps of the scale over a note on it

} // namespace sk
