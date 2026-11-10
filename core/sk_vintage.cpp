// IMPAKT — vintage character (see sk_vintage.h).
#include "sk_vintage.h"

namespace sk {

namespace {
struct DacModel { int bits; float rate, fc, fcAge, poles, tol, noise, curve; };
// bits, converter rate (0 = host rate), reconstruction cutoff (Hz) and its drop with AGE, number of poles,
// resistor tolerance at AGE 1, hiss at AGE 1, even-order curvature at AGE 1
const DacModel MODELS[NUM_DAC_MODELS] = {
    { 24, 0.0f,     0.0f,     0.0f,    0, 0.0f,    0.0f,    0.0f  },   // OFF
    { 16, 0.0f,     20000.0f, 3500.0f, 1, 0.0025f, 1.2e-5f, 0.03f },   // CD-80: early multibit CD player
    { 14, 0.0f,     16000.0f, 3000.0f, 1, 0.004f,  3.5e-5f, 0.04f },   // PCM-14: 14-bit video-tape recorder adapter
    { 12, 26040.0f, 11500.0f, 2500.0f, 2, 0.006f,  9.0e-5f, 0.06f },   // SP-12: 12 bits at 26.04 kHz
    { 8,  31250.0f, 9000.0f,  2500.0f, 2, 0.01f,   2.5e-4f, 0.09f },   // LO-8: 8-bit companded (early drum machines)
};
// (the draws one after the other: the operands of + are evaluated in an order C++ leaves to the compiler, and the
// sum's rounding follows it)
inline float gauss (Rng& r) { float s = r.uni(); s += r.uni(); s += r.uni(); s += r.uni(); return (s - 2.0f) * 1.7320508f; }
}

void Dac::init (float sampleRate)
{
    sr = sampleRate;
    model = -1; age = -1.0f;
    ph = 0.0f;
    for (int c = 0; c < 2; ++c) { hold[c] = lp1[c] = lp2[c] = dcx[c] = dcy[c] = 0.0f; }
    dcR = 1.0f - TWO_PI * 6.0f / sr;
    rng.seed (0xDAC0DAC1u);
    configure (DAC_OFF, 0.0f);
}

void Dac::configure (int m, float a)
{
    m = m < 0 ? 0 : (m >= NUM_DAC_MODELS ? NUM_DAC_MODELS - 1 : m);
    if (m == model && absf (a - age) < 1e-4f) return;
    model = m; age = a;
    const DacModel& M = MODELS[m];
    bits = M.bits;
    rate = M.rate > 0.0f && M.rate < sr ? M.rate : 0.0f;
    const float fc = minf (M.fc - M.fcAge * a, 0.45f * sr);
    lpC = M.poles > 0 ? expf_ (-TWO_PI * fc / sr) : 0.0f;
    lp2C = M.poles > 1 ? lpC : 0.0f;
    noise = M.noise * a;
    curve = M.curve * a;
    // the same "unit" every time: the resistor errors come from a fixed seed, scaled by AGE
    Rng r; r.seed (0x5EEDDACu);
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 16; ++k)
        {
            const float e = gauss (r) * M.tol * a;
            w[c][k] = (float) (1 << k) * (1.0f + e);
        }
    // the sign bit and the low bits as one lookup: the same additions in the same order as the bit loop
    nLow = mini (8, bits - 1);
    for (int c = 0; c < 2; ++c)
        for (int sgn = 0; sgn < 2; ++sgn)
            for (int b = 0; b < (1 << nLow); ++b)
            {
                float v = sgn ? -w[c][bits - 1] : 0.0f;
                for (int k = 0; k < nLow; ++k) if ((b >> k) & 1) v += w[c][k];
                lut[c][sgn][b] = v;
            }
}

void Dac::process (float* L, float* R, int n)
{
    if (model == DAC_OFF) return;
    float* io[2] = { L, R };
    const int N = bits;
    const float full = (float) (1 << (N - 1)), inv = 1.0f / full;
    const int lo = -(1 << (N - 1)), hi = (1 << (N - 1)) - 1;
    const unsigned mask = (1u << N) - 1u;
    const bool companded = model == DAC_LO8;
    const float step = rate > 0.0f ? rate / sr : 0.0f;
    const float LN256 = 5.5451774f;
    for (int i = 0; i < n; ++i)
    {
        bool tick = true;
        if (step > 0.0f) { ph += step; tick = ph >= 1.0f; if (tick) ph -= 1.0f; }
        for (int c = 0; c < 2; ++c)
        {
            float y;
            if (tick)
            {
                float x = clampf (io[c][i], -1.0f, 1.0f);
                if (companded)
                {
                    // mu-law (mu = 255) companding around an 8-bit word
                    const float ax = absf (x);
                    x = (x < 0.0f ? -1.0f : 1.0f) * log2f_ (1.0f + 255.0f * ax) * LN2 / LN256;
                }
                int code = (int) floorf_ (x * full + 0.5f);
                code = code < lo ? lo : (code > hi ? hi : code);
                const unsigned u = (unsigned) code & mask;
                // R-2R ladder: every bit has its own (slightly wrong) weight; the sign bit counts negative
                float v = lut[c][(u >> (N - 1)) & 1u][u & ((1u << nLow) - 1u)];
                for (int k = nLow; k < N - 1; ++k) if ((u >> k) & 1u) v += w[c][k];
                y = v * inv;
                if (companded)
                {
                    const float ay = minf (absf (y), 1.0f);
                    y = (y < 0.0f ? -1.0f : 1.0f) * (exp2f_ (ay * 8.0f) - 1.0f) * (1.0f / 255.0f);
                }
                hold[c] = y;
            }
            y = hold[c];
            // reconstruction filter
            if (lpC > 0.0f) { lp1[c] = y + (lp1[c] - y) * lpC; y = lp1[c]; }
            if (lp2C > 0.0f) { lp2[c] = y + (lp2[c] - y) * lp2C; y = lp2[c]; }
            // analog output stage: even-order curvature, hiss, coupling capacitor
            y += curve * y * y;
            if (noise > 0.0f) y += noise * rng.bi();
            const float o = y - dcx[c] + dcR * dcy[c]; dcx[c] = y; dcy[c] = o;
            io[c][i] = clampf (o, -1.0f, 1.0f);      // the output stage rails
        }
    }
    for (int c = 0; c < 2; ++c)
    {
        if (isBad (dcy[c]) || isBad (lp1[c]) || isBad (lp2[c])) { dcx[c] = dcy[c] = lp1[c] = lp2[c] = hold[c] = 0.0f; }
        if (absf (dcy[c]) < 1e-25f) dcy[c] = 0.0f;
    }
}

void Dac::reset()
{
    ph = 0.0f;
    for (int c = 0; c < 2; ++c) { hold[c] = lp1[c] = lp2[c] = dcx[c] = dcy[c] = 0.0f; }
}

// the same converter as process(), on one channel (its first ladder)
void Dac::processMono (float* x, int n)
{
    if (model == DAC_OFF) return;
    const int N = bits;
    const float full = (float) (1 << (N - 1)), inv = 1.0f / full;
    const int lo = -(1 << (N - 1)), hi = (1 << (N - 1)) - 1;
    const unsigned mask = (1u << N) - 1u;
    const bool companded = model == DAC_LO8;
    const float step = rate > 0.0f ? rate / sr : 0.0f;
    const float LN256 = 5.5451774f;
    for (int i = 0; i < n; ++i)
    {
        bool tick = true;
        if (step > 0.0f) { ph += step; tick = ph >= 1.0f; if (tick) ph -= 1.0f; }
        if (tick)
        {
            float v = clampf (x[i], -1.0f, 1.0f);
            if (companded) { const float av = absf (v); v = (v < 0.0f ? -1.0f : 1.0f) * log2f_ (1.0f + 255.0f * av) * LN2 / LN256; }
            int code = (int) floorf_ (v * full + 0.5f);
            code = code < lo ? lo : (code > hi ? hi : code);
            const unsigned u = (unsigned) code & mask;
            float y = lut[0][(u >> (N - 1)) & 1u][u & ((1u << nLow) - 1u)];
            for (int k = nLow; k < N - 1; ++k) if ((u >> k) & 1u) y += w[0][k];
            y *= inv;
            if (companded) { const float ay = minf (absf (y), 1.0f); y = (y < 0.0f ? -1.0f : 1.0f) * (exp2f_ (ay * 8.0f) - 1.0f) * (1.0f / 255.0f); }
            hold[0] = y;
        }
        float y = hold[0];
        if (lpC > 0.0f) { lp1[0] = y + (lp1[0] - y) * lpC; y = lp1[0]; }
        if (lp2C > 0.0f) { lp2[0] = y + (lp2[0] - y) * lp2C; y = lp2[0]; }
        y += curve * y * y;
        if (noise > 0.0f) y += noise * rng.bi();
        const float o = y - dcx[0] + dcR * dcy[0]; dcx[0] = y; dcy[0] = o;
        x[i] = clampf (o, -1.0f, 1.0f);
    }
    if (isBad (dcy[0]) || isBad (lp1[0]) || isBad (lp2[0])) reset();
    if (absf (dcy[0]) < 1e-25f) dcy[0] = 0.0f;
}

float qPitch (float st, float baseHz, u32 mask)
{
    mask &= 0xFFFu;
    if (! mask) return st;
    const float m = 69.0f + 12.0f * log2f_ (baseHz * (1.0f / 440.0f)) + st;
    const int n0 = (int) floorf_ (m + 0.5f);
    int best = n0; float bd = 1e9f;
    for (int d = -6; d <= 6; ++d)
    {
        const int nn = n0 + d;
        const int pc = ((nn % 12) + 12) % 12;
        if (! ((mask >> pc) & 1u)) continue;
        const float dist = absf ((float) nn - m);
        if (dist < bd - 1e-4f) { bd = dist; best = nn; }
    }
    return st + ((float) best - m);
}

// an interval over a note on the scale (st, semitones from baseHz), counted in steps of the scale: a third stays the
// scale's third (major or minor as the degree has it), a fifth its fifth, an octave an octave. Where that lands on a
// tritone, a minor second or a major seventh over the note (and that is not what was asked), a step either way
// instead: the one nearer the interval asked, up on a tie (a scale with fewer notes, a chord's: its steps the same way)
float qInterval (float st, int semi, float baseHz, u32 mask)
{
    mask &= 0xFFFu;
    if (! mask) return st + (float) semi;
    int L = 0; for (int c = 0; c < 12; ++c) L += (mask >> c) & 1u;
    if (L < 2 || semi == 0) return semi == 0 ? st : qPitch (st + (float) semi, baseHz, mask);
    const float m = 69.0f + 12.0f * log2f_ (baseHz * (1.0f / 440.0f)) + st;
    const int n0 = (int) floorf_ (m + 0.5f);
    auto in = [mask] (int n) { return ((mask >> (((n % 12) + 12) % 12)) & 1u) != 0u; };
    auto walk = [&] (int k) { int n = n0; const int d = k > 0 ? 1 : -1; for (int i = 0; i != k; i += d) { int g = 0; do n += d; while (! in (n) && ++g < 12); } return n; };
    auto bad = [] (int iv) { const int a = ((iv % 12) + 12) % 12; return a == 1 || a == 6 || a == 11; };
    const float sf = (float) semi * (float) L / 12.0f;
    const int steps = sf >= 0.0f ? (int) floorf_ (sf + 0.5f) : -(int) floorf_ (-sf + 0.5f);
    int n = walk (steps);
    if (bad (n - n0) && ! bad (semi))
    {
        const int up = walk (steps + 1), dn = walk (steps - 1);
        const bool ub = bad (up - n0), db = bad (dn - n0);
        const int du = up - n0 - semi < 0 ? semi - (up - n0) : up - n0 - semi, dd = dn - n0 - semi < 0 ? semi - (dn - n0) : dn - n0 - semi;
        if (! ub && (db || du <= dd)) n = up;
        else if (! db) n = dn;
    }
    return st + (float) (n - n0);
}

} // namespace sk
