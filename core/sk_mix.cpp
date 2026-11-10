// IMPAKT — background mix helpers (see sk_mix.h).
#include "sk_mix.h"

namespace sk {

namespace {
inline float dbOf (float p) { return log2f_ (p + 1e-20f) * 3.0103f; }   // 10 log10
const float UM_FC0 = 50.0f, UM_RATIO = 1.78f;
}

// --------------------------------------------------------------------------- unmasker
void Unmasker::init (float sampleRate)
{
    sr = sampleRate;
    fft.init (UM_N);
    for (int i = 0; i < UM_N; ++i) win[i] = 0.5f - 0.5f * cos2pi ((float) i / (float) UM_N);
    const float df = sr / (float) UM_N;
    for (int b = 0; b < UM_BANDS; ++b)
    {
        const float fc = UM_FC0 * powf_ (UM_RATIO, (float) b);
        cw[b] = cos2pi (minf (fc / sr, 0.45f)); sw[b] = sin2pi (minf (fc / sr, 0.45f));
        const float lo = fc / sqrtf_ (UM_RATIO), hi = fc * sqrtf_ (UM_RATIO);
        k0[b] = maxi (1, (int) (lo / df + 0.5f));
        k1[b] = mini (UM_N / 2, maxi (k0[b], (int) (hi / df + 0.5f) - 1));
        top[b] = -1;
    }
    for (int t = 0; t < UM_TRACKS; ++t)
    {
        for (int i = 0; i < UM_N; ++i) buf[t][i] = 0.0f;
        for (int b = 0; b < UM_BANDS; ++b) { E[t][b] = 0.0f; gT[t][b] = gC[t][b] = gA[t][b] = 0.0f; bell[t][b].reset(); bell[t][b].set (cw[b], sw[b], 1.2f, 0.0f); }
        act[t] = 0.0f;
    }
    w = 0; hop = 0;
}

void Unmasker::push (int t, const float* x, int n)
{
    int k = w;
    float e = 0.0f;
    for (int i = 0; i < n; ++i) { buf[t][k] = x[i]; k = (k + 1) & UM_MASK; e += x[i] * x[i]; }
    act[t] += e;
}

void Unmasker::advance (int n, const int* role, float depth, float kickPrio)
{
    w = (w + n) & UM_MASK;
    hop += n;
    const float dtc = (float) n / sr;
    // gain smoothing (dB per chunk): cuts arrive in ~15 ms, leave in ~150 ms
    const float att = 1.0f - expf_ (-dtc / 0.015f), rel = 1.0f - expf_ (-dtc / 0.15f);
    for (int t = 0; t < UM_TRACKS; ++t)
        for (int b = 0; b < UM_BANDS; ++b)
        {
            const float tg = depth > 0.001f ? gT[t][b] : 0.0f;
            gC[t][b] += (tg - gC[t][b]) * (tg < gC[t][b] ? att : rel);
        }
    if (hop < UM_HOP) return;
    hop = 0;

    // band energies of every track that sounded during the last hop
    for (int t = 0; t < UM_TRACKS; ++t)
    {
        if (act[t] < 1e-9f) { for (int b = 0; b < UM_BANDS; ++b) E[t][b] *= 0.5f; act[t] = 0.0f; continue; }
        act[t] = 0.0f;
        for (int i = 0; i < UM_N; ++i) fr[i] = buf[t][(w + i) & UM_MASK] * win[i];
        fft.forward (fr, re, im);
        for (int b = 0; b < UM_BANDS; ++b)
        {
            float e = 0.0f;
            for (int k = k0[b]; k <= k1[b]; ++k) e += re[k] * re[k] + im[k] * im[k];
            e /= (float) (k1[b] - k0[b] + 1);
            E[t][b] = maxf (e, E[t][b] * 0.5f);            // instant rise, quick fall between frames
        }
    }

    // who owns each band; everybody else who collides gets a little out of the way
    const float maxCut = 12.0f * depth;
    for (int b = 0; b < UM_BANDS; ++b)
    {
        float W[UM_TRACKS]; int best = -1; float bw = 0.0f;
        for (int t = 0; t < UM_TRACKS; ++t)
        {
            float pr = 1.0f;
            if (role[t] == ROLE_KICK) pr = b <= 1 ? 1.0f + 7.0f * kickPrio : (b == 2 ? 1.0f + 2.0f * kickPrio : 1.0f);
            else if (role[t] == ROLE_SNARE) pr = (b == 3 || b == 4) ? 2.0f : (b == 7 ? 1.5f : 1.0f);
            else if (role[t] == ROLE_HAT) pr = b >= 8 ? 2.0f : 1.0f;
            W[t] = E[t][b] * pr;
            if (W[t] > bw) { bw = W[t]; best = t; }
        }
        // hysteresis: the previous owner keeps the band unless clearly beaten (1.5 dB)
        if (top[b] >= 0 && best != top[b] && W[top[b]] * 1.41f > bw) best = top[b];
        top[b] = bw > 1e-10f ? best : -1;
        for (int t = 0; t < UM_TRACKS; ++t)
        {
            float tg = 0.0f;
            if (top[b] >= 0 && t != top[b] && W[t] > 1e-12f)
            {
                const float r = W[t] / maxf (W[top[b]], 1e-20f);
                // the closer the collision, the deeper the cut (smoothstep from r = 0.03 to 0.6)
                const float x = clampf ((r - 0.03f) / 0.57f, 0.0f, 1.0f);
                tg = -maxCut * x * x * (3.0f - 2.0f * x);
            }
            gT[t][b] = tg;
        }
    }
}

void Unmasker::apply (int t, float* x, int n)
{
    for (int b = 0; b < UM_BANDS; ++b)
    {
        Bell& bl = bell[t][b];
        const float g = gC[t][b];
        if (g > -0.2f && gA[t][b] > -0.2f)
        {
            // at 0 dB the bell is an identity and its state is exactly zero: skip it
            if (gA[t][b] < -1e-6f) { gA[t][b] = 0.0f; bl.set (cw[b], sw[b], 1.2f, 0.0f); bl.reset(); }
            continue;
        }
        if (absf (g - gA[t][b]) > 0.1f) { gA[t][b] = g; bl.set (cw[b], sw[b], 1.2f, g); }
        for (int i = 0; i < n; ++i) x[i] = bl.tick (x[i]);
        if (isBad (bl.z1) || isBad (bl.z2)) bl.reset();
    }
}

// --------------------------------------------------------------------------- calm (resonance suppressor)
void Calm::init (float sampleRate)
{
    sr = sampleRate;
    fft.init (CALM_N);
    for (int i = 0; i < CALM_N; ++i) { win[i] = 0.5f - 0.5f * cos2pi ((float) i / (float) CALM_N); buf[i] = 0.0f; }
    for (int k = 0; k < CALM_B; ++k) psm[k] = 0.0f;
    w = 0; hop = 0;
    for (int k = 0; k < CALM_K; ++k)
    {
        Slot& z = s[k];
        z.f = z.ft = 1000.0f * (float) (k + 1); z.g = z.gt = 0.0f; z.q = z.qt = 4.0f; z.ap = 0.0f;
        for (int c = 0; c < 2; ++c) { z.b[c].reset(); z.b[c].set (1.0f, 0.0f, 4.0f, 0.0f); }
    }
}

void Calm::analyse (const CalmParams& p)
{
    for (int i = 0; i < CALM_N; ++i) fr[i] = buf[(w + i) & CALM_MASK] * win[i];
    fft.forward (fr, re, im);
    const int B = CALM_B;
    const float df = sr / (float) CALM_N;
    float pmax = -200.0f;
    // power smoothed over ~4 frames: noise flickers average out, real resonances stay
    const float ts = 0.75f - 0.45f * p.speed;
    for (int k = 0; k < B; ++k)
    {
        const float pw = re[k] * re[k] + im[k] * im[k];
        psm[k] = pw > psm[k] ? psm[k] + (pw - psm[k]) * (1.0f - ts * 0.6f) : psm[k] * ts + pw * (1.0f - ts);
        pdb[k] = dbOf (psm[k]); pmax = maxf (pmax, pdb[k]);
    }
    for (int k = 0; k < CALM_K; ++k) s[k].gt = 0.0f;
    if (pmax < -40.0f) return;                                   // silence (in this FFT's scale)

    // smoothed envelope: mean of the dB spectrum over +-1/3 octave (at least +-3 bins)
    cum[0] = 0.0;
    for (int k = 0; k < B; ++k) cum[k + 1] = cum[k] + (double) pdb[k];
    for (int k = 1; k < B; ++k)
    {
        const int half = maxi (3, (int) ((float) k * 0.26f));
        const int a = maxi (1, k - half), b = mini (B - 1, k + half);
        env[k] = (float) ((cum[b + 1] - cum[a]) / (double) (b - a + 1));
    }
    env[0] = pdb[0];

    // excess over the envelope, weighted by the focus range
    const float thr = 1.5f + 7.5f * p.select;
    const float fcF = 250.0f * powf_ (40.0f, p.focus);
    const float flo = 70.0f, fhi = minf (18000.0f, 0.45f * sr);
    for (int k = 0; k < B; ++k)
    {
        ex[k] = 0.0f;
        const float f = (float) k * df;
        if (k < 2 || f < flo || f > fhi || pdb[k] < pmax - 70.0f) continue;
        const float oct = absf (log2f_ (f / fcF)) / 2.6f;
        if (oct >= 1.0f) continue;
        const float c = cos2pi (0.25f * oct);
        ex[k] = maxf (0.0f, pdb[k] - env[k] - thr) * c * c;
    }

    // the strongest local maxima, at least 1/6 octave apart
    int pk[CALM_K]; float pv[CALM_K]; int np = 0;
    for (int k = 2; k < B - 1; ++k)
    {
        const float v = ex[k];
        if (v < 0.5f || v < ex[k - 1] || v < ex[k + 1]) continue;
        // insert in descending order, keeping the spacing rule
        bool close = false;
        for (int j = 0; j < np; ++j)
            if (absf (log2f_ ((float) k / (float) pk[j])) < 0.1667f) { if (v > pv[j]) { pv[j] = v; pk[j] = k; } close = true; break; }
        if (close) continue;
        if (np < CALM_K) { pk[np] = k; pv[np] = v; ++np; }
        else
        {
            int mi = 0; for (int j = 1; j < np; ++j) if (pv[j] < pv[mi]) mi = j;
            if (v > pv[mi]) { pk[mi] = k; pv[mi] = v; }
        }
    }

    bool used[CALM_K] = {};
    const float qMul = exp2f_ (-1.0f + 2.3f * p.sharp);
    for (int j = 0; j < np; ++j)
    {
        const int k = pk[j];
        // parabolic interpolation of the peak frequency
        const float a = pdb[k - 1], b = pdb[k], c = pdb[k + 1];
        const float den = a - 2.0f * b + c;
        const float d = absf (den) > 1e-6f ? clampf (0.5f * (a - c) / den, -0.5f, 0.5f) : 0.0f;
        const float f = ((float) k + d) * df;
        // width: where the excess falls to half
        int kl = k, kr = k;
        while (kl > 1 && ex[kl - 1] > pv[j] * 0.5f) --kl;
        while (kr < B - 2 && ex[kr + 1] > pv[j] * 0.5f) ++kr;
        const float bw = (float) (kr - kl + 1) * df;
        const float q = clampf (f / bw * 0.7f * qMul, 0.7f, 16.0f);
        const float g = -p.depth * minf (pv[j] * 1.5f, 24.0f);
        // the closest slot within 1/3 octave keeps tracking it, else a free slot
        int best = -1; float bd = 0.34f;
        for (int i = 0; i < CALM_K; ++i)
            if (! used[i] && s[i].g < -0.3f) { const float dd = absf (log2f_ (s[i].f / f)); if (dd < bd) { bd = dd; best = i; } }
        if (best < 0)
        {
            float lg = 1e9f;
            for (int i = 0; i < CALM_K; ++i) if (! used[i] && absf (s[i].g) < lg) { lg = absf (s[i].g); best = i; }
            if (best >= 0 && absf (s[best].g) < 0.3f) { s[best].f = f; s[best].q = q; }   // a silent slot jumps there
        }
        if (best < 0) continue;
        used[best] = true;
        s[best].ft = f; s[best].qt = q; s[best].gt = g;
    }
}

void Calm::process (float* L, float* R, int n, const CalmParams& p)
{
    // analysis on the mid signal (no latency on the audio path: the filters follow the last frame)
    for (int i = 0; i < n; ++i) { buf[w] = 0.5f * (L[i] + R[i]); w = (w + 1) & CALM_MASK; }
    hop += n;
    if (hop >= CALM_HOP) { hop = 0; if (p.depth > 0.001f) analyse (p); else for (int k = 0; k < CALM_K; ++k) s[k].gt = 0.0f; }

    const float dtc = (float) n / sr;
    const float ta = 0.03f * exp2f_ (-3.0f * p.speed), tr = 0.35f * exp2f_ (-3.0f * p.speed);
    const float att = 1.0f - expf_ (-dtc / ta), rel = 1.0f - expf_ (-dtc / tr), glide = 1.0f - expf_ (-dtc / 0.03f);
    const bool delta = p.delta > 0.5f;
    if (delta) for (int i = 0; i < n && i < 64; ++i) { dry[0][i] = L[i]; dry[1][i] = R[i]; }
    for (int k = 0; k < CALM_K; ++k)
    {
        Slot& z = s[k];
        z.g += (z.gt - z.g) * (z.gt < z.g ? att : rel);
        z.f *= exp2f_ (log2f_ (z.ft / z.f) * glide);
        z.q += (z.qt - z.q) * glide;
        if (z.g > -0.05f && z.ap > -0.05f)
        {
            if (z.ap < -1e-6f) { z.ap = 0.0f; z.b[0].reset(); z.b[1].reset(); }
            continue;
        }
        z.ap = z.g;
        const float fn = clampf (z.f / sr, 0.001f, 0.45f);
        const float cw = cos2pi (fn), sw = sin2pi (fn);
        z.b[0].set (cw, sw, z.q, z.g);                   // coefficients only: the states carry on
        z.b[1].set (cw, sw, z.q, z.g);
        for (int i = 0; i < n; ++i) { L[i] = z.b[0].tick (L[i]); R[i] = z.b[1].tick (R[i]); }
        if (isBad (z.b[0].z1) || isBad (z.b[1].z1)) { z.b[0].reset(); z.b[1].reset(); }
    }
    if (delta) for (int i = 0; i < n && i < 64; ++i) { L[i] = dry[0][i] - L[i]; R[i] = dry[1][i] - R[i]; }
}

} // namespace sk
