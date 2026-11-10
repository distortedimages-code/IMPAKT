// IMPAKT — the CHORUS and the PHASER, two sends (see sk_send.h). Own implementation.
#include "sk_send.h"

namespace sk {

namespace {

// CHORUS: the modes' rates (Hz) and how far they sweep the lines' clock (± share); the lines' delay at rest (4.5 ms);
// the compander's reference level, its rectifiers (10 ms), the emphasis (+8 dB above 2 kHz); the hiss under it; the
// cross-mix's high-pass and the direct way's bass
const float CHO_RATE[CHO_MODES]  = { 0.25f, 0.25f, 0.25f, 0.5f };
const float CHO_DEPTH[CHO_MODES] = { 0.067f, 0.122f, 0.189f, 0.244f };
constexpr float CHO_D0 = 0.0045f, CHO_REF = 0.1f, CHO_EMPH = 2.5f, CHO_NOISE = 1e-4f, CHO_XHP = 300.0f, CHO_BASS = 0.15f;
constexpr float CHO_SLEEP = 0.3f;              // seconds of silence in and out before it stops running
// PHASER: the photocells (the stages' corner at full light, the darkest light, the law), their response
constexpr float PH_FMAX = 4800.0f, PH_LMIN = 0.008f, PH_GAMMA = 0.8f;

inline float onePoleK (float hz, float sr) { return 1.0f - expf_ (-TWO_PI * minf (hz, 0.45f * sr) / sr); }

// 4-point Hermite, d samples behind the last one written (w - 1)
inline float hread (const float* b, int w, float d)
{
    const float rp = (float) (w - 1) - d;
    const float fl = floorf_ (rp);
    const int i = (int) fl; const float f = rp - fl;
    const int m = CHO_LEN - 1;
    const float xm1 = b[(i - 1) & m], x0 = b[i & m], x1 = b[(i + 1) & m], x2 = b[(i + 2) & m];
    const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

// tan(x) for the stages' corners (x < 0.7): a Padé form
inline float tanPade (float x) { const float x2 = x * x; return x * (15.0f - x2) / (15.0f - 6.0f * x2); }

inline float phChain (float* s, float x, float G)
{
    for (int k = 0; k < PHS_STAGES; ++k)
    {
        const float v = (x - s[k]) * G, lp = v + s[k];
        s[k] = lp + v;
        x = 2.0f * lp - x;
    }
    return x;
}

} // namespace

// ----------------------------------------------------------------- CHORUS
void DimChorus::init (float sampleRate)
{
    sr = sampleRate;
    rng.seed (0x51DD320u);
    clear();
    ph = 0.0f; depth = CHO_DEPTH[1]; rate = CHO_RATE[1];
    quiet = (int) (CHO_SLEEP * sr) + 1;
}

void DimChorus::clear()
{
    for (int i = 0; i < CHO_LEN; ++i) bufL[i] = bufR[i] = 0.0f;
    w = 0;
    cut = minf (10000.0f, 0.42f * sr);
    for (int k = 0; k < 2; ++k)
    {
        setLp4 (aa[k], cut, sr); setLp4 (rc[k], cut, sr);
        aa[k][0].reset(); aa[k][1].reset(); rc[k][0].reset(); rc[k][1].reset();
        pe[k] = de[k] = ec[k] = ee[k] = hp[k] = 0.0f;
    }
    bass = 0.0f;
}

void DimChorus::process (const float* inL, const float* inR, float* outL, float* outR, int n, const ChorusParams& p, float gain)
{
    float e = 0.0f;
    for (int i = 0; i < n; ++i) e += absf (inL[i]) + absf (inR[i]);
    const int sleep = (int) (CHO_SLEEP * sr);
    if (e > 1e-9f) quiet = 0;
    else if (quiet > sleep) return;
    const int mode = p.mode < 0 ? 0 : (p.mode >= CHO_MODES ? CHO_MODES - 1 : p.mode);
    const float kG = 1.0f - expf_ (-(float) n / (0.2f * sr));          // a mode change glides over 0.2 s
    depth += (CHO_DEPTH[mode] - depth) * kG; rate += (CHO_RATE[mode] - rate) * kG;
    const float d0 = CHO_D0 * sr, inc = rate / sr;
    const float cE = onePoleK (2000.0f, sr), cR = 1.0f - expf_ (-1.0f / (0.01f * sr));
    const float cH = onePoleK (CHO_XHP, sr), cB = onePoleK (200.0f, sr);
    float outE = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        ph += inc; if (ph >= 1.0f) ph -= 1.0f;
        const float tri = 4.0f * absf (ph - 0.5f) - 1.0f;
        // the two clocks swept in antiphase: each line's delay is its stages over its clock
        const float D1 = d0 / (1.0f + depth * tri), D2 = d0 / (1.0f - depth * tri);
        const float m = 0.5f * (inL[i] + inR[i]);
        const float x[2] = { p.stereoIn ? inL[i] : m, p.stereoIn ? inR[i] : m };
        // into the lines: pre-emphasis, the 2:1 compressor, the anti-alias filter, the bucket brigade's soft limit
        for (int k = 0; k < 2; ++k)
        {
            pe[k] += cE * (x[k] - pe[k]);
            const float u = pe[k] + (x[k] - pe[k]) * CHO_EMPH;
            ec[k] += cR * (absf (u) - ec[k]);
            const float gc = minf (30.0f, sqrtf_ (CHO_REF / (ec[k] + 1e-7f)));
            float y = lp4 (aa[k], u * gc);
            y = clampf (y, -1.5f, 1.5f); y = y * (1.0f - 0.06f * y * y);
            (k ? bufR : bufL)[w] = y;
        }
        w = (w + 1) & (CHO_LEN - 1);
        const float r[2] = { hread (bufL, w, D1), hread (bufR, w, D2) };
        // out of them: the hiss, the reconstruction filter, the 1:2 expander, the de-emphasis
        float d[2];
        for (int k = 0; k < 2; ++k)
        {
            float y = lp4 (rc[k], r[k] + CHO_NOISE * rng.bi());
            ee[k] += cR * (absf (y) - ee[k]);
            const float z = y * minf (6.0f, ee[k] * (1.0f / CHO_REF));
            de[k] += cE * (z - de[k]);
            d[k] = de[k] + (z - de[k]) * (1.0f / CHO_EMPH);
        }
        // each line on its side, the other one against it above the cross-over; a little more bass
        hp[0] += cH * (d[0] - hp[0]); hp[1] += cH * (d[1] - hp[1]);
        bass += cB * (m - bass);
        const float l = d[0] - (d[1] - hp[1]) + CHO_BASS * bass, rr = d[1] - (d[0] - hp[0]) + CHO_BASS * bass;
        outL[i] += l * gain; outR[i] += rr * gain;
        outE += absf (l) + absf (rr);
    }
    if (e <= 1e-9f && outE < 1e-6f * (float) n) quiet += n;
    if (quiet > sleep) clear();
    for (int k = 0; k < 2; ++k) { aa[k][0].sanitize(); aa[k][1].sanitize(); rc[k][0].sanitize(); rc[k][1].sanitize(); if (isBad (ee[k]) || isBad (ec[k])) clear(); }
}

// ----------------------------------------------------------------- PHASER
void BiPhaser::init (float sampleRate)
{
    sr = sampleRate;
    clear();
    ph1 = 0.0f; ph2 = 0.25f;
    kOn = 1.0f - expf_ (-1.0f / (0.004f * sr));
    kOff = 1.0f - expf_ (-1.0f / (0.015f * sr));
    kOffDark = 1.0f - expf_ (-1.0f / (0.08f * sr));
    quiet = (int) (CHO_SLEEP * sr) + 1;
}

void BiPhaser::clear()
{
    for (int k = 0; k < PHS_STAGES; ++k) sA[k] = sB[k] = 0.0f;
    yA = yB = 0.0f;
    lA = lB = PH_LMIN;
}

void BiPhaser::process (const float* inL, const float* inR, float* outL, float* outR, int n, const PhaserParams& p, float gain)
{
    float e = 0.0f;
    for (int i = 0; i < n; ++i) e += absf (inL[i]) + absf (inR[i]);
    const int sleep = (int) (CHO_SLEEP * sr);
    if (e > 1e-9f) quiet = 0;
    else if (quiet > sleep) { ph1 = wrap01 (ph1 + p.rate1 * (float) n / sr); ph2 = wrap01 (ph2 + p.rate2 * (float) n / sr); return; }
    const float i1 = p.rate1 / sr, i2 = p.rate2 / sr;
    const float fA = clampf (p.fbA, 0.0f, 1.0f) * 0.9f, fB = clampf (p.fbB, 0.0f, 1.0f) * 0.9f;
    const float xs = PI / sr, fmax = minf (PH_FMAX, 0.2f * sr);
    float outE = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        ph1 += i1; if (ph1 >= 1.0f) ph1 -= 1.0f;
        ph2 += i2; if (ph2 >= 1.0f) ph2 -= 1.0f;
        const float g1 = sweepShape (sin2pi (ph1), p.shape1), g2 = sweepShape (sin2pi (ph2), p.shape2);
        const float gb = (p.bGen2 ? g2 : g1) * (p.rev ? -1.0f : 1.0f);
        // the lamps: each photocell brightens fast and darkens slowly (the more slowly, the darker it is)
        const float tA = PH_LMIN + p.depthA * (0.5f + 0.5f * g1) * (1.0f - PH_LMIN);
        const float tB = PH_LMIN + p.depthB * (0.5f + 0.5f * gb) * (1.0f - PH_LMIN);
        lA += (tA - lA) * (tA > lA ? kOn : kOff + (kOffDark - kOff) * (1.0f - lA));
        lB += (tB - lB) * (tB > lB ? kOn : kOff + (kOffDark - kOff) * (1.0f - lB));
        // its conductance follows the light (a power law): the stages' corner
        const float cA = fmax * powf_ (maxf (lA, PH_LMIN), PH_GAMMA), cB = fmax * powf_ (maxf (lB, PH_LMIN), PH_GAMMA);
        const float gA = tanPade (cA * xs), gB = tanPade (cB * xs);
        const float GA = gA / (1.0f + gA), GB = gB / (1.0f + gB);
        const float m = 0.5f * (inL[i] + inR[i]);
        float a, b;
        if (p.route == 0)      { a = phChain (sA, softclip (inL[i] + fA * yA), GA); b = phChain (sB, softclip (inR[i] + fB * yB), GB); }
        else if (p.route == 1) { a = phChain (sA, softclip (m + fA * yA), GA); b = phChain (sB, softclip (m + fB * yB), GB); }
        else                   { a = phChain (sA, softclip (m + fA * yA), GA); b = phChain (sB, softclip (a + fB * yB), GB); }
        yA = a; yB = b;
        outL[i] += a * gain; outR[i] += b * gain;
        outE += absf (a) + absf (b);
    }
    if (e <= 1e-9f && outE < 1e-6f * (float) n) quiet += n;
    if (quiet > sleep) clear();
    if (isBad (yA) || isBad (yB)) clear();
}

void Echo82::clear()
{
    for (int k = 0; k < 2; ++k)
    {
        aa[k][0].reset(); aa[k][1].reset(); rc[k][0].reset(); rc[k][1].reset();
        xPrev[k] = hold[k] = 0.0f;
    }
    limEnv = 0.0f; limG = 1.0f; cut = 0.0f; ph = 0.0; w = 0; N = 0; c = 1.0f; fs0 = 0.0f; vph = 0.0f; env = 0.0f; vco = 0.0f;
}

} // namespace sk
