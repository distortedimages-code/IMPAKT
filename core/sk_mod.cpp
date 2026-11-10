// IMPAKT — TONE (the track's high-pass, low-pass and three-band EQ) and the two envelope followers of each track.
//
// TONE sits at the end of the track, after its BOX: a 12 dB/oct high-pass and low-pass (Butterworth, no resonance: they
// make room in the mix, they do not colour), a low shelf, a mid bell whose bandwidth narrows as its gain grows (like an
// analog console) and a high shelf. All five are trapezoidal state-variable filters (A. Simper), so their settings can
// move under p-locks, LFOs and the followers without zipper or blow-up. A filter at its neutral setting is skipped.
//
// The envelope followers (after the Analog Heat's): a source (the track itself, another track, the host input or the
// master, as heard in the previous 32-sample chunk), an input gain, a band (FULL, LOW, MID, HIGH), then either a follower
// (attack / decay) or, with TRG above zero, a level crossing that fires an attack / decay envelope.
#include "sk_engine.h"

namespace sk {

// ----------------------------------------------------------------- TONE

namespace {
struct SvfC { float a1, a2, a3, m0, m1, m2, g, k; };
inline void svfSet (SvfC& c, float g, float k)
{
    c.g = g; c.k = k;
    c.a1 = 1.0f / (1.0f + g * (g + k)); c.a2 = g * c.a1; c.a3 = g * c.a2;
}
inline float svfTick (const SvfC& c, float* z, float x)
{
    const float v3 = x - z[1];
    const float v1 = c.a1 * z[0] + c.a2 * v3;
    const float v2 = z[1] + c.a2 * z[0] + c.a3 * v3;
    z[0] = 2.0f * v1 - z[0]; z[1] = 2.0f * v2 - z[1];
    return c.m0 * x + c.m1 * v1 + c.m2 * v2;
}
inline float gOf (float hz, float sr) { return tanpi (clampf (hz / sr, 1e-5f, 0.45f)); }
}

void Engine::toneProcess (ToneEq& q, const float* P, float* x, int n)
{
    const float lowDb = (P[P_LOW] - 0.5f) * 30.0f, midDb = (P[P_MID] - 0.5f) * 30.0f, hiDb = (P[P_HIGH] - 0.5f) * 30.0f;
    int on = 0;
    if (P[P_HPF] > 0.005f) on |= 1;
    if (P[P_LPF] < 0.995f) on |= 2;
    if (absf (lowDb) > 0.05f) on |= 4;
    if (absf (midDb) > 0.05f) on |= 8;
    if (absf (hiDb) > 0.05f) on |= 16;
    // a filter that starts again starts from rest; the low-pass from where its input is (no step)
    for (int f = 0; f < 5; ++f)
        if ((on & (1 << f)) && ! (q.on & (1 << f))) { q.s[f][0] = 0.0f; q.s[f][1] = f == 1 ? x[0] : 0.0f; }
    const int was = q.on, run = on | was;    // a filter that stops runs one more chunk, fading out to the dry signal
    q.on = on;
    if (! run) return;
    SvfC c[5];
    const float K2 = 1.41421356f;
    if (on & 1) { svfSet (c[0], gOf (20.0f * powf_ (400.0f, P[P_HPF]), sr), K2); c[0].m0 = 1.0f; c[0].m1 = -K2; c[0].m2 = -1.0f; }
    if (on & 2) { svfSet (c[1], gOf (100.0f * powf_ (200.0f, P[P_LPF]), sr), K2); c[1].m0 = 0.0f; c[1].m1 = 0.0f; c[1].m2 = 1.0f; }
    if (on & 4)
    {
        const float A = exp2f_ (lowDb * (1.0f / 12.0412f)), sA = sqrtf_ (A);      // A = 10^(dB / 40)
        svfSet (c[2], gOf (40.0f * powf_ (10.0f, P[P_LOWF]), sr) / sA, K2);
        c[2].m0 = 1.0f; c[2].m1 = K2 * (A - 1.0f); c[2].m2 = A * A - 1.0f;
    }
    if (on & 8)
    {
        const float A = exp2f_ (midDb * (1.0f / 12.0412f)), Qb = 0.6f + 0.04f * absf (midDb), k = 1.0f / (Qb * A);
        svfSet (c[3], gOf (150.0f * powf_ (53.333f, P[P_MIDF]), sr), k);
        c[3].m0 = 1.0f; c[3].m1 = k * (A * A - 1.0f); c[3].m2 = 0.0f;
    }
    if (on & 16)
    {
        const float A = exp2f_ (hiDb * (1.0f / 12.0412f)), sA = sqrtf_ (A);
        svfSet (c[4], gOf (1500.0f * powf_ (10.0f, P[P_HIGF]), sr) * sA, K2);
        c[4].m0 = A * A; c[4].m1 = K2 * (1.0f - A) * A; c[4].m2 = 1.0f - A * A;
    }
    // silence in and the filters at rest (most of the time between two hits, a muted track): nothing to compute,
    // the filters stay at these settings
    {
        float pk = 0.0f;
        for (int i = 0; i < n; ++i) pk = maxf (pk, absf (x[i]));
        if (pk < 1e-9f)
        {
            float zs = 0.0f;
            for (int f = 0; f < 5; ++f) if (run & (1 << f)) zs = maxf (zs, maxf (absf (q.s[f][0]), absf (q.s[f][1])));
            if (zs < 1e-9f)
            {
                for (int f = 0; f < 5; ++f)
                {
                    q.s[f][0] = q.s[f][1] = 0.0f;
                    if (on & (1 << f)) { const float cv[5] = { c[f].g, c[f].k, c[f].m0, c[f].m1, c[f].m2 }; for (int j = 0; j < 5; ++j) q.pc[f][j] = q.ps[f][j] = cv[j]; }
                }
                for (int i = 0; i < n; ++i) x[i] = 0.0f;
                return;
            }
        }
    }
    // each filter glides from the coefficients of the last chunk to these ones (a sweep, a follower on a band:
    // no step at the chunk edges). One that starts fades in from the dry signal over the chunk, one that stops
    // (HP down to OFF, LP up to OFF, a band back to 0 dB, often under an LFO) fades out to it
    const float inN = 1.0f / (float) n;
    for (int f = 0; f < 5; ++f)
    {
        const int bit = 1 << f;
        if (! (run & bit)) continue;
        float* z = q.s[f];
        float* pc = q.pc[f];
        if (! (on & bit))
        {
            SvfC cl; svfSet (cl, pc[0], pc[1]); cl.m0 = pc[2]; cl.m1 = pc[3]; cl.m2 = pc[4];
            for (int i = 0; i < n; ++i) { const float y = svfTick (cl, z, x[i]); x[i] = y + (x[i] - y) * ((float) (i + 1) * inN); }
            if (isBad (z[0]) || isBad (z[1])) z[0] = z[1] = 0.0f;
            continue;
        }
        const SvfC& cf = c[f];
        const float cv[5] = { cf.g, cf.k, cf.m0, cf.m1, cf.m2 };
        float* ps = q.ps[f];
        const bool starts = ! (was & bit);
        if (starts) for (int j = 0; j < 5; ++j) pc[j] = ps[j] = cv[j];      // just started: these settings at once
        bool same = true;
        for (int j = 0; j < 5; ++j) if (pc[j] != cv[j] || ps[j] != cv[j]) same = false;
        if (starts) for (int i = 0; i < n; ++i) { const float y = svfTick (cf, z, x[i]); x[i] += (y - x[i]) * ((float) (i + 1) * inN); }
        else if (same) for (int i = 0; i < n; ++i) x[i] = svfTick (cf, z, x[i]);
        else
        {
            // the cutoff and the damping glide (the integrator gains follow from them), the output mix too: the
            // chunk's ramp, rounded by a one-pole (0.5 ms) so that two ramps meet without a corner
            const float in = 1.0f / (float) n, c = smoothC;
            SvfC g;
            for (int i = 0; i < n; ++i)
            {
                const float u = (float) (i + 1) * in;
                for (int j = 0; j < 5; ++j) ps[j] += (pc[j] + (cv[j] - pc[j]) * u - ps[j]) * c;
                svfSet (g, ps[0], ps[1]);
                g.m0 = ps[2]; g.m1 = ps[3]; g.m2 = ps[4];
                x[i] = svfTick (g, z, x[i]);
            }
            bool near = true;
            for (int j = 0; j < 5; ++j) if (absf (ps[j] - cv[j]) > 1e-6f * (1.0f + absf (cv[j]))) near = false;
            if (near) for (int j = 0; j < 5; ++j) ps[j] = cv[j];
        }
        for (int j = 0; j < 5; ++j) pc[j] = cv[j];
        if (isBad (z[0]) || isBad (z[1])) { z[0] = z[1] = 0.0f; for (int i = 0; i < n; ++i) x[i] = 0.0f; }
    }
}

// ----------------------------------------------------------------- envelope followers

void Engine::efAdvance (int n)
{
    static const float BAND_HZ[EF_BANDS] = { 0.0f, 150.0f, 1000.0f, 3000.0f };
    const float fA = 1.0f - expf_ (-1.0f / (0.0005f * sr)), fD = 1.0f - expf_ (-1.0f / (0.03f * sr));
    for (int ti = 0; ti < NUM_TRACKS; ++ti)
    {
        Track& t = tr[ti];
        // like the LFOs: the settings of the step the current voice plays
        const float* lk = t.voice[t.cur].lk;
        auto Q = [&] (int p) { return lk[p] >= 0.0f ? lk[p] : t.p[p]; };
        for (int e = 0; e < 2; ++e)
        {
            const int b = e ? P_E2SR : P_E1SR;
            EnvFol& f = t.ef[e];
            const int dest = idxOf (Q (b + 7), NUM_LFO_DEST);           // 0: OFF
            // nothing to drive and nobody looking: at rest
            if (dest == 0 && ti != specTrack) { f.env = f.fast = f.ad = f.val = f.in = f.pkVal = f.pkIn = 0.0f; f.stage = f.held = 0; f.ic1 = f.ic2 = 0.0f; continue; }
            const int src = idxOf (Q (b), EF_SRCS);
            const float* x; int m;
            if (src == EF_IN)        { x = inM; m = n; }
            else if (src == EF_MAIN) { x = efMain; m = efPrevN; }
            else                     { x = trkOut[src == EF_SELF ? ti : src - 1]; m = efPrevN; }
            const float gain = exp2f_ ((-12.0f + 48.0f * Q (b + 1)) * (1.0f / 6.0206f));
            const int band = idxOf (Q (b + 2), EF_BANDS);
            float g = 0.0f, k = 1.41421356f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
            if (band != EF_FULL)
            {
                g = tanpi (BAND_HZ[band] / sr); k = band == EF_MIDB ? 1.0f / 0.7f : 1.41421356f;
                a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
            }
            const float tA = 0.0001f * powf_ (10000.0f, Q (b + 4)), tD = 0.005f * powf_ (800.0f, Q (b + 5));
            const float cA = 1.0f - expf_ (-1.0f / (tA * sr)), cD = 1.0f - expf_ (-1.0f / (tD * sr));
            const float tg = Q (b + 3), thr = tg > 0.01f ? exp2f_ ((-48.0f + 48.0f * tg) * (1.0f / 6.0206f)) : -1.0f;
            float pk = 0.0f;
            for (int i = 0; i < m; ++i)
            {
                float y = x[i];
                if (band != EF_FULL)
                {
                    const float v3 = y - f.ic2, v1 = a1 * f.ic1 + a2 * v3, v2 = f.ic2 + a2 * f.ic1 + a3 * v3;
                    f.ic1 = 2.0f * v1 - f.ic1; f.ic2 = 2.0f * v2 - f.ic2;
                    y = band == EF_LOW ? v2 : (band == EF_MIDB ? k * v1 : y - k * v1 - v2);
                }
                const float a = absf (y) * gain;
                pk = maxf (pk, a);
                f.env += (a > f.env ? cA : cD) * (a - f.env);
                f.fast += (a > f.fast ? fA : fD) * (a - f.fast);
                if (thr > 0.0f)
                {
                    if (! f.held && f.fast > thr) { f.held = 1; f.stage = 1; }
                    else if (f.held && f.fast < 0.5f * thr) f.held = 0;
                }
            }
            if (isBad (f.ic1) || isBad (f.ic2) || isBad (f.env)) { f.ic1 = f.ic2 = f.env = f.fast = 0.0f; }
            f.in = pk;
            if (thr > 0.0f)
            {
                // the triggered envelope runs at the rate of this chunk: a linear attack, an exponential decay
                const float atkInc = (float) n / maxf (tA * sr, 1.0f), dc = powf_ (decayCoef (tD, sr), (float) n);
                if (f.stage == 1) { f.ad += atkInc; if (f.ad >= 1.0f) { f.ad = 1.0f; f.stage = 2; } }
                else if (f.stage == 2) { f.ad *= dc; if (f.ad < 1e-4f) { f.ad = 0.0f; f.stage = 0; } }
                f.val = f.ad;
            }
            else { f.stage = 0; f.ad = 0.0f; f.val = minf (f.env, 1.0f); }
            f.pkVal = maxf (f.pkVal, f.val); f.pkIn = maxf (f.pkIn, f.in);
        }
    }
}

// GLITCH for its screen: the last 16 divisions (event types, -1 none), the current one's progress and detail
// (repeats / rate / divisions back), the codec's bandwidth (Hz), whether the effect is in
void Engine::glitchInfo (float* out) const
{
    for (int h = 0; h < Glitch::HIST; ++h) out[h] = (float) glitch.hist[h];
    out[16] = glitch.curProg; out[17] = glitch.curDetail; out[18] = g[G_XLOS] > 0.002f ? glitch.lossBw : 0.0f;
    out[19] = (g[G_XON] > 0.5f || glitch.busy()) ? 1.0f : 0.0f;
    out[20] = g[G_XANA] > 0.5f ? 1.0f : 0.0f; out[21] = glitch.lossCents;
}

void Engine::modInfo (float* out)
{
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        Track& k = tr[t];
        float* o = out + 8 * t;
        // (LOOP: where it is in its loop, the page draws the whole loop)
        o[0] = k.lfo[0].lp >= 0.0f ? k.lfo[0].lp : minf (k.lfo[0].ph, 1.0f); o[1] = k.lfo[0].val;
        o[2] = k.lfo[1].lp >= 0.0f ? k.lfo[1].lp : minf (k.lfo[1].ph, 1.0f); o[3] = k.lfo[1].val;
        // the followers: the highest value and input since the last call, so that no hit falls between two frames
        o[4] = k.ef[0].pkVal; o[5] = k.ef[0].pkIn; o[6] = k.ef[1].pkVal; o[7] = k.ef[1].pkIn;
        for (int e = 0; e < 2; ++e) k.ef[e].pkVal = k.ef[e].pkIn = 0.0f;
    }
}

} // namespace sk
