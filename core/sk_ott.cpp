// IMPAKT — OTT (see sk_ott.h). Three Linkwitz-Riley bands (4th order, 88.3 Hz and 2.5 kHz; the low band
// goes through the 2.5 kHz all-pass so the three sum flat), an RMS detector per band linked across the two
// channels, then the static curve of the preset: above the upper threshold a 66.7:1 squeeze, below the lower
// one a 4.17:1 lift (at most +36 dB), and the preset's make-up gain per band. The dry signal goes through the
// same all-passes, so MIX blends in phase.
#include "sk_ott.h"

namespace sk {

namespace {

const float F1 = 88.3f, F2 = 2500.0f;
// low, mid, high (the preset's values)
const float T_DOWN[3] = { -33.8f, -30.2f, -35.5f };       // dB, above: downward
const float T_UP[3]   = { -40.8f, -41.8f, -40.8f };       // dB, below: upward
const float R_DOWN = 66.7f, R_UP = 4.17f;
const float ATK_MS[3] = { 47.8f, 22.4f, 13.5f };
const float REL_MS[3] = { 282.0f, 282.0f, 132.0f };
const float MAKEUP[3] = { 10.3f, 5.7f, 10.3f };           // dB
const float MAX_LIFT = 36.0f;                              // dB
// after the preset's curve a loud mix comes out ~9 dB lower: OUT at 0 dB gives it back, so MIX blends
// two signals of about the same loudness
const float OUT_TRIM = 9.0f;                               // dB

void design (OttBiquad& f, int type, float fc, float sr)  // 0 LP, 1 HP, 2 AP (Butterworth Q)
{
    const float k = tanpi (fc / sr), q = 0.70710678f;
    const float n = 1.0f / (1.0f + k / q + k * k);
    const float a1 = 2.0f * (k * k - 1.0f) * n, a2 = (1.0f - k / q + k * k) * n;
    if (type == 0) { f.b0 = k * k * n; f.b1 = 2.0f * f.b0; f.b2 = f.b0; }
    else if (type == 1) { f.b0 = n; f.b1 = -2.0f * n; f.b2 = n; }
    else { f.b0 = a2; f.b1 = a1; f.b2 = 1.0f; }
    f.a1 = a1; f.a2 = a2;
    f.reset();
}

inline float dbToLin (float db) { return exp2f_ (db * 0.16609640f); }      // 10^(db/20)

} // namespace

void Ott::init (float rate)
{
    sr = rate > 1000.0f ? rate : 48000.0f;
    for (int c = 0; c < 2; ++c)
    {
        for (int s = 0; s < 2; ++s)
        {
            design (lp1[c][s], 0, F1, sr); design (hp1[c][s], 1, F1, sr);
            design (lp2[c][s], 0, F2, sr); design (hp2[c][s], 1, F2, sr);
        }
        design (ap2[c], 2, F2, sr);
        design (dap[c][0], 2, F1, sr); design (dap[c][1], 2, F2, sr);
    }
    reset();
}

void Ott::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        for (int s = 0; s < 2; ++s) { lp1[c][s].reset(); hp1[c][s].reset(); lp2[c][s].reset(); hp2[c][s].reset(); }
        ap2[c].reset(); dap[c][0].reset(); dap[c][1].reset();
    }
    for (int b = 0; b < 3; ++b) { env[b] = 1e-9f; gainDb[b] = 0.0f; }
}

void Ott::process (float* L, float* R, int n, const OttParams& p)
{
    const float tf = maxf (0.01f, p.time);
    float aAtk[3], aRel[3], makeup[3];
    const float bandKnob[3] = { p.lo, p.mid, p.hi };
    const float rDown = 1.0f + (R_DOWN - 1.0f) * clampf (p.down, 0.0f, 1.0f);
    const float rUp = 1.0f + (R_UP - 1.0f) * clampf (p.up, 0.0f, 1.0f);
    const float kDown = 1.0f - 1.0f / rDown, kUp = 1.0f - 1.0f / rUp;
    for (int b = 0; b < 3; ++b)
    {
        aAtk[b] = 1.0f - tcCoef (ATK_MS[b] * 0.001f * tf, sr);
        aRel[b] = 1.0f - tcCoef (REL_MS[b] * 0.001f * tf, sr);
        makeup[b] = MAKEUP[b] + (bandKnob[b] - 0.5f) * 24.0f;
    }
    const float outG = dbToLin ((p.out - 0.5f) * 24.0f);
    const float wetG = dbToLin (OUT_TRIM);
    const float mix = clampf (p.mix, 0.0f, 1.0f);
    float gLast[3] = { 0.0f, 0.0f, 0.0f };

    for (int i = 0; i < n; ++i)
    {
        float band[3][2], dry[2];
        const float in[2] = { L[i], R[i] };
        for (int c = 0; c < 2; ++c)
        {
            const float x = in[c];
            const float lo = ap2[c].tick (lp1[c][1].tick (lp1[c][0].tick (x)));
            const float rest = hp1[c][1].tick (hp1[c][0].tick (x));
            band[0][c] = lo;
            band[1][c] = lp2[c][1].tick (lp2[c][0].tick (rest));
            band[2][c] = hp2[c][1].tick (hp2[c][0].tick (rest));
            dry[c] = dap[c][1].tick (dap[c][0].tick (x));
        }
        float wet[2] = { 0.0f, 0.0f };
        for (int b = 0; b < 3; ++b)
        {
            // RMS, both channels (linked)
            const float ms = 0.5f * (band[b][0] * band[b][0] + band[b][1] * band[b][1]);
            env[b] += (ms - env[b]) * (ms > env[b] ? aAtk[b] : aRel[b]);
            const float lvl = 3.0103f * log2f_ (env[b] + 1e-12f);           // 10 log10
            float g = 0.0f;
            if (lvl > T_DOWN[b]) g = (T_DOWN[b] - lvl) * kDown;
            else if (lvl < T_UP[b]) g = minf ((T_UP[b] - lvl) * kUp, MAX_LIFT);
            g += makeup[b];
            gLast[b] = g - makeup[b];
            const float lin = dbToLin (g);
            wet[0] += band[b][0] * lin * wetG; wet[1] += band[b][1] * lin * wetG;
        }
        L[i] = (dry[0] + (wet[0] - dry[0]) * mix) * outG;
        R[i] = (dry[1] + (wet[1] - dry[1]) * mix) * outG;
    }
    for (int b = 0; b < 3; ++b) gainDb[b] = gLast[b];
}

} // namespace sk
