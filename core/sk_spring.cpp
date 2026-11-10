// IMPAKT — the spring of WEST (v78, sk_spring.h): ChowDSP's BYOD spring reverb, ported (GPL-3.0: core/LICENSE-BYOD.txt;
// © Jatin Chowdhury, BYOD and chowdsp_utils). The structure, the formulas and the constants are the original's.
#include "sk_spring.h"

namespace sk {

namespace {

inline float uni (u32& s) { s = s * 1664525u + 1013904223u; return (float) (s >> 8) * (1.0f / 16777216.0f); }

// chowdsp's DelayLine (pop before push: an integer delay D is D samples): a line with its write head at w (the next
// sample's place); what was written j samples ago (j >= 1) is at w - j
template <int N> inline float tap (const float* b, int w, int j) { return b[(w - j) & (N - 1)]; }

// Lagrange 3rd-order (DelayLineInterpolationTypes::Lagrange3rd): its integer part one sample back (the fraction in
// [1, 2) once the delay is past a sample), four points
template <int N> inline float popLagrange (const float* b, int w, float D)
{
    int k = (int) D; float d = D - (float) k;
    if (k >= 1) { d += 1.0f; --k; }
    const float v1 = tap<N> (b, w, k), v2 = tap<N> (b, w, k + 1), v3 = tap<N> (b, w, k + 2), v4 = tap<N> (b, w, k + 3);
    const float d1 = d - 1.0f, d2 = d - 2.0f, d3 = d - 3.0f;
    const float c1 = -d1 * d2 * d3 * (1.0f / 6.0f), c2 = d2 * d3 * 0.5f, c3 = -d1 * d3 * 0.5f, c4 = d1 * d2 * (1.0f / 6.0f);
    return v1 * c1 + d * (v2 * c2 + v3 * c3 + v4 * c4);
}

} // namespace

void ByodSpring::init (float sampleRate)
{
    // (the whole tank at rest: its lines, their states)
    unsigned char* p = reinterpret_cast<unsigned char*> (this);
    for (unsigned k = 0; k < (unsigned) sizeof (ByodSpring); ++k) p[k] = 0;
    sr = sampleRate; fs = 0.5f * sampleRate;
    down.reset (HB1); up[0].reset (HB1); up[1].reset (HB1);
    for (int c = 0; c < 2; ++c) { dc[c].reset(); lpf[c].reset(); dc[c].set (40.0f, 0.70710678f, fs); }
    rng = 0x5EB1A5u;
    shakeLeft = 0;
    chaosCnt = 0;
    configure (0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f);
    ready = 1;
}

void ByodSpring::configure (float size, float decay, float reflections, float spin, float damping, float chaosAmt)
{
    size = clampf (size, 0.0f, 1.0f); decay = clampf (decay, 0.0f, 1.0f); reflections = clampf (reflections, 0.0f, 1.0f);
    spin = clampf (spin, 0.0f, 1.0f); damping = clampf (damping, 0.0f, 1.0f); chaosAmt = clampf (chaosAmt, 0.0f, 1.0f);
    // T60: 0.5 .. 4.5 s on DECAY, shorter for a small tank
    const float lowT60 = 0.5f, highT60 = 4.5f;
    const float decayCorr = 0.7f * (1.0f - size * size);
    const float t60 = lowT60 * powf_ (highT60 / lowT60, 0.95f * decay - decayCorr);
    // the long delay, its chaos (a random share of 7 % of it, smoothed, to the cube of CHAOS)
    float D = 1000.0f + (size * 0.099f) * fs;
    chaosT = D * 0.07f;      // (the target's scale; its draw: process)
    D += chaosAmt * chaosAmt * chaosAmt * chaos;
    const float dmax = (float) (SB_DLY - 8);
    dlyD[0] = minf (D, dmax / 1.05f); dlyD[1] = minf (D * 1.05f, dmax);
    for (int c = 0; c < 2; ++c) fbS[c] = powf_ (0.001f, dlyD[c] / (t60 * fs));
    // the allpasses: all of one length and gain; the way back with the gain's opposite
    g = 0.5f - 0.4f * spin;
    const float ad = (0.35f + 3.0f * size) * 0.001f * fs;
    for (int l = 0; l < SB_LANES; ++l)
    {
        const float d = minf (l < 2 ? ad : ad * 1.07f, (float) (SB_APF - 4));
        // (Thiran: the fraction kept in [0.618, 1.618) once past a sample)
        int k = (int) d; float fr = d - (float) k;
        if (fr < 0.618f && k >= 1) { fr += 1.0f; --k; }
        apfD[l] = d; apfK[l] = k; apfA[l] = (1.0f - fr) / (1.0f + fr);
    }
    // the output's low-pass: 4 kHz (DAMP at the top) .. 18 kHz (or under the tank's Nyquist)
    const float dampLow = 4000.0f, dampHigh = minf (18000.0f, fs * 0.48f);
    const float dampF = dampLow * powf_ (dampHigh / dampLow, 1.0f - damping);
    lpf[0].set (dampF, 0.70710678f, fs); lpf[1].set (dampF, 0.70710678f, fs);
    // the reflections: four lengths on SIZE, their feedback on T60 and REFL (0.95 REFL^2: "reflSkew"), the shelf
    const float mix = 0.95f * reflections * reflections;
    static const float base[4] = { 0.07f, 0.17f, 0.23f, 0.29f };
    const float fbBase = 0.001f * powf_ (10.0f, mix), fbScale = 0.23f * mix * (0.735f + 0.235f * size);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 4; ++i)
        {
            const float d = minf (base[i] * size * fs * (c ? 1.09f : 1.0f), (float) (SB_REFL - 8));
            reflD[c][i] = maxf (d, 1.0f);
            reflFb[c][i] = (mix > 0.0f ? powf_ (fbBase, reflD[c][i] / (t60 * fs)) : 0.0f) * fbScale;
        }
    // the first-order shelf (chowdsp calcFirstOrderShelf, the bilinear transform): 1 under 800 Hz, -3 .. -12 dB over
    const float lowG = 1.0f, highG = exp2f_ ((-3.0f - 9.0f * damping) * 0.16609640f);
    const float rr = 1.0f / sqrtf_ (highG / lowG), K = 1.0f / tanpi (800.0f / fs);
    const float b0 = highG * rr * K + lowG, b1 = lowG - highG * rr * K, a0 = rr * K + 1.0f, a1 = 1.0f - rr * K;
    shB0 = b0 / a0; shB1 = b1 / a0; shA1 = a1 / a0;
}

void ByodSpring::shake (float amount, float size)
{
    if (amount <= 0.0f) return;
    const float secs = (0.0005f + 0.0005f * uni (rng)) * (1.0f + 0.5f * clampf (size, 0.0f, 1.0f));
    shakeLen = (int) maxf (1.0f, fs * secs);
    shakeLeft = shakeLen;
    shakeGain = 2.0f * clampf (amount, 0.0f, 1.0f);
}

void ByodSpring::process (const float* in, float* outL, float* outR, int n)
{
    for (int i = 0; i + 1 < n; i += 2)
    {
        // the chaos's target, every 512 samples (the original sets it each host block; its smoothing, 50 ms long,
        // moved on by a 2 ms block each time: 4 % of the way)
        if (--chaosCnt <= 0) { chaosCnt = (int) (sr * (512.0f / 48000.0f) * 0.5f); chaos += (uni (rng) * chaosT - chaos) * (48.0f / 1200.0f); }
        const float x = down.process (in[i], in[i + 1]);
        float sh = 0.0f;
        if (shakeLeft > 0) { sh = shakeGain * sin2pi ((float) (shakeLen - shakeLeft) / (2.0f * (float) shakeLen)); --shakeLeft; }
        // the input a side: tanh (x - feedback x the long delay), its DC out (40 Hz), the shake
        float lane[SB_LANES];
        for (int c = 0; c < 2; ++c)
        {
            const float back = popLagrange<SB_DLY> (dly[c], dlyW, dlyD[c]);
            float lp, bp, hp;
            dc[c].tick (tanhf_ (x - fbS[c] * back), lp, bp, hp);
            lane[2 * c] = hp + sh;
            lane[2 * c + 1] = z[c];
        }
        // the tank: 16 nested allpasses (SchroederAllpass<Vec, 2>), lane by lane
        for (int l = 0; l < SB_LANES; ++l)
        {
            const float gl = (l & 1) ? -g : g, al = apfA[l];
            const int k = apfK[l];
            float y = lane[l];
            for (int s = 0; s < SB_STAGES; ++s)
            {
                float* o = apf[l][s][0];
                float* q = apf[l][s][1];
                float& so = apfSt[l][s][0];
                float& sq = apfSt[l][s][1];
                // the outer line's output, through the inner allpass
                const float vo = so = tap<SB_APF> (o, apfW, k + 1) + al * (tap<SB_APF> (o, apfW, k) - so);
                const float vi = sq = tap<SB_APF> (q, apfW, k + 1) + al * (tap<SB_APF> (q, apfW, k) - sq);
                const float u = vo + gl * vi;
                q[apfW & (SB_APF - 1)] = u;
                const float dOut = vi - gl * u;
                y += gl * dOut;
                o[apfW & (SB_APF - 1)] = y;
                y = dOut - gl * y;
            }
            lane[l] = y;
        }
        ++apfW;
        // out a side: the way back into the long delay; the reflections taken off the way forward; the low-pass
        float yo[2];
        for (int c = 0; c < 2; ++c)
        {
            dly[c][dlyW & (SB_DLY - 1)] = lane[2 * c + 1];
            float r[4], sum = 0.0f;
            for (int k = 0; k < 4; ++k) { r[k] = popLagrange<SB_REFL> (refl[c][k], reflW, reflD[c][k]); sum += r[k]; }
            float acc = 0.0f;
            for (int k = 0; k < 4; ++k) acc += (r[k] - 0.5f * sum) * reflFb[c][k];
            acc *= 0.25f;
            const float e = shB0 * acc + shZ[c];
            shZ[c] = shB1 * acc - shA1 * e;
            float y = lane[2 * c] - e;
            for (int k = 0; k < 4; ++k) refl[c][k][reflW & (SB_REFL - 1)] = y;
            float lp, bp, hp;
            lpf[c].tick (y, lp, bp, hp);
            yo[c] = lp;
            z[c] = lp;
        }
        ++dlyW; ++reflW;
        // back to the track's rate
        up[0].process (yo[0], outL[i], outL[i + 1]);
        up[1].process (yo[1], outR[i], outR[i + 1]);
    }
    // (a tank gone wrong starts over)
    if (isBad (z[0]) || isBad (z[1]) || isBad (shZ[0]) || isBad (shZ[1]))
    {
        const float s = sr;
        init (s);
    }
}

} // namespace sk
