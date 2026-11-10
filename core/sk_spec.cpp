// IMPAKT — spectral reverb (see sk_spec.h).
#include "sk_spec.h"

namespace sk {

// atan2 in cycles, result in (-0.5, 0.5], max error ~2e-6 cycles
static inline float atan2cyc (float y, float x)
{
    const float ax = absf (x), ay = absf (y);
    if (ax + ay < 1e-30f) return 0.0f;
    const float a = minf (ax, ay) / maxf (ax, ay), s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0.0f) r = 3.14159274f - r;
    if (y < 0.0f) r = -r;
    return r * 0.159154943f;
}

void SpecVerb::init (float sampleRate)
{
    sr = sampleRate;
    N = sr > 60000.0f ? 4096 : 2048;
    R = N / 4; B = N / 2 + 1;
    fft.init (N);
    for (int i = 0; i < N; ++i) win[i] = 0.5f - 0.5f * cos2pi ((float) i / (float) N);
    for (int i = 0; i < SPEC_NMAX; ++i) in[i] = olaL[i] = olaR[i] = 0.0f;
    for (int k = 0; k < SPEC_BMAX; ++k) E[k] = F[k] = prevPh[k] = phL[k] = phR[k] = 0.0f;
    for (int h = 0; h < SPEC_HIST; ++h) for (int k = 0; k < SPEC_BMAX; ++k) hist[h][k] = 0.0f;
    for (int d = 0; d < SPEC_DMAX; ++d) for (int k = 0; k < SPEC_BMAX; ++k) dly[d][k] = 0.0f;
    cnt = 0; histW = 0; revR = 0; revOn = 0; dW = 0; idle = 1; energy = 0.0f; outGainSmooth = 0.0f;
    rng.seed (0x5EC7u);
}

void SpecVerb::process (const float* inM, float* oL, float* oR, int n, const SpecParams& p)
{
    for (int i = 0; i < n; ++i)
    {
        in[N - R + cnt] = inM[i];
        oL[i] += olaL[cnt];
        oR[i] += olaR[cnt];
        if (++cnt == R)
        {
            for (int j = 0; j < N - R; ++j) { olaL[j] = olaL[j + R]; olaR[j] = olaR[j + R]; }
            for (int j = N - R; j < N; ++j) olaL[j] = olaR[j] = 0.0f;
            hop (p);
            for (int j = 0; j < N - R; ++j) in[j] = in[j + R];
            cnt = 0;
        }
    }
}

void SpecVerb::hop (const SpecParams& p)
{
    const bool freeze = p.freeze > 0.5f;
    double inE = 0.0;
    for (int i = 0; i < N; ++i) { fr[i] = in[i] * win[i]; inE += (double) fr[i] * fr[i]; }
    if (! freeze && inE < 1e-14 && energy < 1e-12f) { idle = 1; return; }
    idle = 0;

    fft.forward (fr, re, im);

    const float rt = 0.25f * powf_ (120.0f, p.time);
    const float lg = -3.0f * 3.3219281f * (float) R / (rt * sr);           // log2 of amplitude decay per hop
    const float gA = exp2f_ (lg);
    const float inG = freeze ? 0.0f : 1.0f;
    const int   D = 1 + (int) (p.reflect * (float) (SPEC_DMAX - 3));
    const float fbR = freeze ? 0.0f : p.reflect * 0.85f;
    const int   dRead = (dW - D + SPEC_DMAX) % SPEC_DMAX;
    const float invN = 1.0f / (float) N, invR = 1.0f / (float) R;

    double esum = 0.0;
    for (int k = 0; k < B; ++k)
    {
        const float a2 = (re[k] * re[k] + im[k] * im[k]) * inG;
        const float ph = atan2cyc (im[k], re[k]);
        float d = ph - prevPh[k] - (float) k * (float) R * invN;
        d -= floorf_ (d + 0.5f);
        const float instF = (float) k * invN + d * invR;
        prevPh[k] = ph;
        const float gk = freeze ? 1.0f : exp2f_ (2.0f * lg * (1.0f + 2.0f * (float) k / (float) B));
        // reflect: re-inject an older frame; its gain is capped so decay + feedback stays below unity
        const float dv = dly[dRead][k];
        const float rf2 = dv * dv * minf (fbR * fbR, maxf (0.0f, 0.995f - gk));
        const float eOld = E[k] * gk;
        const float eIn = a2 + rf2;
        const float eNew = eOld + eIn;
        F[k] = eNew > 1e-30f ? (eIn * (a2 > rf2 ? instF : F[k]) + eOld * F[k]) / eNew : instF;
        E[k] = minf (eNew, 1e12f);
        esum += E[k];
    }
    energy = (float) esum;

    // blur: smear energy and frequency across neighbouring bins, inside the loop
    if (p.blur > 0.001f)
    {
        const float c = p.blur * 0.85f;
        for (int k = 1; k < B; ++k) { E[k] += c * (E[k - 1] - E[k]); F[k] += c * 0.5f * (F[k - 1] - F[k]); }
        for (int k = B - 2; k >= 0; --k) { E[k] += c * (E[k + 1] - E[k]); F[k] += c * 0.5f * (F[k + 1] - F[k]); }
    }
    for (int k = 0; k < B; ++k) { dly[dW][k] = sqrtf_ (E[k]); hist[histW][k] = E[k]; }
    dW = (dW + 1) % SPEC_DMAX;
    histW = (histW + 1) % SPEC_HIST;

    // reverse: read the energy history backwards in ~0.7 s windows
    const float* Es = E;
    if (p.reverse > 0.5f)
    {
        if (! revOn) { revOn = 1; revR = 0; }
        revR += 2; if (revR >= SPEC_HIST - 1) revR = 0;
        Es = hist[((histW - 1 - revR) % SPEC_HIST + SPEC_HIST) % SPEC_HIST];
    }
    else revOn = 0;

    // output: warp, tilt, lossy artefacts, randomised phase advance
    float w = (p.warp - 0.5f) * 2.0f;
    if (absf (w) < 0.03f) w = 0.0f;
    const float ratio = exp2f_ (w * 3.0f);
    const float t = (p.atmo - 0.5f) * 2.0f;
    const float lossy = absf (t) * sqrtf_ (absf (t));
    const float V = 0.04f + p.blur * 0.45f;
    const float norm = powf_ (maxf (1.0f - gA * gA, 1e-4f), 0.35f) * (freeze ? 0.35f : 1.0f);
    const float gain = p.level * norm * (2.0f / 3.0f);
    float mmax = 0.0f;
    for (int j = 0; j < B; ++j)
    {
        const float s = (float) j / ratio;
        float m = 0.0f, f = 0.0f;
        if (s < (float) (B - 1))
        {
            const int i0 = (int) s; const float u = s - (float) i0;
            m = sqrtf_ (maxf (Es[i0] + (Es[i0 + 1] - Es[i0]) * u, 0.0f));
            f = (F[i0] + (F[i0 + 1] - F[i0]) * u) * ratio;
        }
        const float x = ((float) j + 1.0f) / ((float) B * 0.12f);
        m *= clampf (exp2f_ (t * 0.9f * log2f_ (x)), 0.03f, 6.0f);
        tl[j] = m; fr[j] = f;                       // fr reused as frequency scratch (B <= N)
        if (m > mmax) mmax = m;
    }
    const float thr = lossy * 0.08f * mmax;
    for (int j = 0; j < B; ++j)
    {
        float m = tl[j];
        if (lossy > 0.01f && (m < thr || rng.uni() < lossy * 0.3f)) m = 0.0f;
        const float adv = fr[j] * (float) R;
        const float rv = V * (0.6f + 0.8f * (float) j / (float) B);
        phL[j] += adv + rv * rng.bi() * 0.5f; phL[j] -= floorf_ (phL[j]);
        phR[j] += adv + rv * rng.bi() * 0.5f; phR[j] -= floorf_ (phR[j]);
        tl[j] = m * gain;
    }
    // left
    for (int j = 0; j < B; ++j) { re[j] = tl[j] * cos2pi (phL[j]); im[j] = tl[j] * sin2pi (phL[j]); }
    im[0] = 0.0f; im[B - 1] = 0.0f;
    fft.inverse (re, im, fr);
    for (int i = 0; i < N; ++i) olaL[i] += fr[i] * win[i];
    // right
    for (int j = 0; j < B; ++j) { re[j] = tl[j] * cos2pi (phR[j]); im[j] = tl[j] * sin2pi (phR[j]); }
    im[0] = 0.0f; im[B - 1] = 0.0f;
    fft.inverse (re, im, fr);
    for (int i = 0; i < N; ++i) olaR[i] += fr[i] * win[i];

    if (isBad (energy)) init (sr);
}

} // namespace sk
