// IMPAKT — the HEX analog kit.
//
// HEX·BD / HEX·SD / HEX·TM: modelled on the hexagonal-pad analog drum brains of the early eighties. Each voice
// is a triangle VCO (softened, as on the original boards) and a transistor noise source through a 4-pole low-pass,
// mixed by BAL, with a CLICK (a short, loud burst of the filtered noise plus a tick), a BEND that the decay also
// times, a fast pitch blip on the attack and an OTA-style VCA. The velocity does what it did on the pads: louder
// (in the track strip), brighter (filter), a harder attack, a deeper bend. The noise is the point: it stays raw.
//
// The three voices render at twice the rate and come back through the halfband decimator.
#include "sk_engine.h"

namespace sk {

// ----------------------------------------------------------------- helpers

static inline float wtTri (float p)  { p += 0.25f; p -= floorf_ (p); return 1.0f - 4.0f * absf (p - 0.5f); }   // 0 at p = 0, rising

static inline float wrapPh (float p) { return p - floorf_ (p); }

// the amp envelope of every machine (the same as the engine's): attack ramp, gate hold, exponential decay
static inline float ampTick (Voice& v, float attInc, float coef)
{
    if (v.astage == 0)      { v.aenv += attInc; if (v.aenv >= 1.0f) { v.aenv = 1.0f; v.astage = 1; } }
    else if (v.astage == 1) { if (v.aHoldLeft > 0.0f) v.aHoldLeft -= 1.0f; else v.astage = 2; }
    else                    v.aenv *= v.hrel > 0.0f ? v.hrel : coef;      // (DEC on HOLD: the gate shut, its release)
    return v.ecrv < 0.9995f ? envCurve (v.aenv, v.astage, v.ecrv) : v.aenv;
}

// ----------------------------------------------------------------- HEX·BD / SD / TM

void Engine::hexTrigger (Voice& v, const float* P)
{
    (void) P;
    v.os2.reset (HB1);
    for (int k = 0; k < 12; ++k) v.hx[k] = 0.0f;
    v.ph[0] = v.ph[1] = 0.0f;               // the VCO starts at its zero crossing, as the boards did on the trigger
    v.penv = v.menv = v.cenv = v.ienv = 1.0f;
}

void Engine::hexRender (Voice& v, const float* P, float tuneRaw, u32 qm, int n, float* out)
{
    const int m = v.machine;
    const float sr2 = 2.0f * sr, isr2 = 0.5f / sr;
    const float vel = clampf (v.vel, 0.0f, 1.0f);
    float base, T, soft, hpHz, blipOct, k, swp;
    if (m == M_HEXBD)      { base = 58.0f;  T = 0.05f * powf_ (40.0f, P[P_DEC]); soft = 2.2f; hpHz = 90.0f;  blipOct = 2.0f * P[P_F]; k = 0.0f; swp = P[P_E]; }
    else if (m == M_HEXSD) { base = 196.0f; T = 0.04f * powf_ (40.0f, P[P_DEC]); soft = 7.0f; hpHz = 320.0f; blipOct = 0.6f; k = 3.4f * P[P_E]; swp = 0.0f; }
    else
    {
        static const float SIZE_HZ[3] = { 233.0f, 155.0f, 98.0f };            // HI / MID / LOW
        base = SIZE_HZ[idxOf (P[P_F], 3)]; T = 0.06f * powf_ (50.0f, P[P_DEC]); soft = 7.0f; hpHz = 90.0f; blipOct = 0.4f; k = 0.8f; swp = P[P_E];
    }
    const float st = qm ? qPitch (tuneRaw, base, qm) : tuneRaw;
    const float f0 = base * semis (st);
    // BEND: right of 12 o'clock the hit starts high and drops (the classic), left it rises; harder hits bend further.
    // DEC times it, as on the original (a third of the decay)
    const float bend = (P[P_B] - 0.5f) * 4.0f * (0.7f + 0.3f * vel);
    const float bc = tcCoef (0.33f * T, sr2);
    const float blip = blipOct * (0.4f + 0.6f * vel), mc = tcCoef (0.004f, sr2);
    const float dc = decayCoef (T, sr);
    const float att = 1.0f / ((0.0016f - 0.0014f * vel) * sr);              // harder hits, faster attack (1.6 -> 0.2 ms)
    // the triangle, softened by a one-pole (the boards' "filtered triangle"; the kick's is nearly a sine)
    const float sc = 1.0f - expf_ (-TWO_PI * minf (soft * f0, 0.4f * sr2) / sr2), sGain = m == M_HEXBD ? 1.1f : 1.0f;
    // noise: high-pass, then the 4-pole ladder (FLTR, brighter with the velocity; SWP closes it with the envelope)
    const float hpc = 1.0f - expf_ (-TWO_PI * hpHz / sr2);
    const float fc0 = 150.0f * powf_ (93.0f, P[P_A]) * (0.55f + 0.45f * vel);
    const float fc = minf (fc0 * (swp > 0.0f ? exp2f_ (-5.0f * swp * (1.0f - v.aenv)) : 1.0f), 0.42f * sr2);
    const float g = tanpi (fc / sr2), G = g / (1.0f + g), G2 = G * G, G4 = G2 * G2, B = 1.0f / (1.0f + g);
    const float kComp = 1.0f + 0.35f * k;                                    // some of the bass a resonant ladder loses
    // BAL: tone <-> noise (both full at 12 o'clock)
    const float bal = P[P_C];
    const float tg = minf (1.0f, 1.41421356f * cos2pi (0.25f * bal)), ng = minf (1.0f, 1.41421356f * sin2pi (0.25f * bal));
    // the noise at about the level of the tone at a middle cutoff (a darker filter is still quieter, as it was)
    const float nrm = 6.5f * powf_ (1500.0f / maxf (fc, 60.0f), 0.3f);
    // CLICK: the noise, filtered and raw, loud and short (1.5 .. 4.5 ms, shorter on hard hits), and a tick on the trigger
    const float clk = P[P_D] * P[P_D] * 1.6f, cc = tcCoef (0.0015f + 0.003f * (1.0f - vel), sr2);
    const float hnG = 2.2f * vel * vel;                                      // the raw part: only the hard hits snap
    // RIM (snare): on hard hits, an FM crack on the VCO (2.37x, a fast index envelope)
    const float rim = m == M_HEXSD && vel > 0.45f ? P[P_F] * (vel - 0.45f) * (1.0f / 0.55f) * 4.0f * 0.15915494f : 0.0f;
    const float ic = tcCoef (0.012f, sr2);
    const float fcap = 0.4f * sr2;
    const float outG = m == M_HEXBD ? 2.0f : (m == M_HEXSD ? 0.9f : 0.72f);
    float* L = v.hx;   // 0..3 ladder, 4 noise high-pass state, 5 softened triangle
    for (int i = 0; i < n; ++i)
    {
        const float a = ampTick (v, att, dc);
        float xs[2];
        for (int s = 0; s < 2; ++s)
        {
            const float f = minf (f0 * exp2f_ (bend * v.penv + blip * v.menv), fcap);
            v.penv *= bc; v.menv *= mc;
            v.ph[0] += f * isr2; v.ph[0] -= floorf_ (v.ph[0]);
            float ph = v.ph[0];
            if (rim > 0.0f)
            {
                v.ph[1] += 2.37f * f * isr2; v.ph[1] -= floorf_ (v.ph[1]);
                ph = wrapPh (ph + rim * v.ienv * sin2pi (v.ph[1]));
                v.ienv *= ic;
            }
            L[5] += (wtTri (ph) - L[5]) * sc;
            const float tone = L[5] * sGain;
            // transistor noise -> high-pass -> ladder (tanh at its input)
            const float wn = v.rng.bi();
            L[4] += (wn - L[4]) * hpc;
            const float hn = wn - L[4];
            const float S = G2 * G * B * L[0] + G2 * B * L[1] + G * B * L[2] + B * L[3];
            const float y4 = (G4 * hn + S) / (1.0f + k * G4);
            float x = softclip (hn - k * y4);                               // (a rational tanh: no exp per sample)
            for (int j = 0; j < 4; ++j) { const float vv = (x - L[j]) * G; const float y = vv + L[j]; L[j] = y + vv; x = y; }
            const float nz = x * kComp * nrm;
            float sum = (tone * tg + nz * ng) * a;
            // the click: the filtered noise, and the transistor noise itself so that it still snaps when FLTR is dark
            sum += (0.6f * nz + hnG * hn) * clk * v.cenv;
            v.cenv *= cc;
            if (v.ns == 0 && s == 0) sum += clk * 0.35f * (0.5f + 0.5f * vel);
            xs[s] = softclip (1.25f * sum) * 0.8f;                           // the VCA: an OTA, it rounds the loud hits
        }
        out[i] = v.os2.process (xs[0], xs[1]) * outG;
        v.level = maxf (a, v.cenv * clk);
        v.ns++;
    }
    for (int j = 0; j < 6; ++j) if (isBad (L[j])) L[j] = 0.0f;
}

} // namespace sk
