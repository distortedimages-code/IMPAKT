// IMPAKT — FDN (room / hall / space) and spring reverbs (see sk_rev.h).
#include "sk_rev.h"

namespace sk {

namespace {
inline float log10f_ (float x) { return log2f_ (x) * 0.30103f; }
inline float pow10f_ (float x) { return exp2f_ (x * 3.3219281f); }
// line lengths in ms at size 1 (mutually incommensurate)
const float ROOM_MS[FDN_N]  = { 11.3f, 13.7f, 17.1f, 19.9f, 23.3f, 27.1f, 31.7f, 37.1f };
const float HALL_MS[FDN_N]  = { 29.7f, 37.1f, 41.1f, 47.3f, 53.9f, 61.1f, 67.7f, 79.3f };
const float SPACE_MS[FDN_N] = { 47.9f, 59.3f, 71.1f, 83.9f, 97.7f, 109.3f, 127.1f, 149.9f };
const float ER_MS[8]        = { 4.3f, 7.9f, 11.7f, 15.1f, 19.9f, 23.7f, 29.3f, 37.1f };
const float SGN_IN[FDN_N]   = { 1, -1, 1, -1, 1, -1, 1, -1 };
const float SGN_L[FDN_N]    = { 1, 1, -1, -1, 1, 1, -1, -1 };     // rows of a Hadamard matrix:
const float SGN_R[FDN_N]    = { 1, -1, -1, 1, 1, -1, -1, 1 };     // L, R and the input are orthogonal
}

// --------------------------------------------------------------------------- FDN
void FdnReverb::init (float sampleRate)
{
    sr = sampleRate;
    for (int j = 0; j < FDN_N; ++j)
    {
        for (int i = 0; i < FDN_LEN; ++i) line[j][i] = 0.0f;
        len[j] = 100.0f; gain[j] = 0.0f; bco[j] = 0.0f; lp[j] = 0.0f; ph[j] = (float) j / (float) FDN_N;
        rate[j] = (0.07f + 0.11f * (float) j) / sr;
    }
    w = 0;
    const int base[4] = { 142, 107, 379, 277 };
    for (int k = 0; k < 4; ++k)
    {
        apLen[k] = mini ((int) ((float) base[k] * sr / 29761.0f) + 1, 2047);
        apW[k] = 0;
        for (int i = 0; i < 2048; ++i) ap[k][i] = 0.0f;
    }
    for (int i = 0; i < ER_LEN; ++i) er[i] = 0.0f;
    for (int i = 0; i < SHIM_LEN; ++i) sh[i] = 0.0f;
    erW = shW = 0; shPh = 0.0f; shOut = shLp = 0.0f;
    mode = -1; modDepth = diff = early = shimmer = 0.0f; scale = 1.0f;
}

void FdnReverb::configure (int algo, float time, float size, float damp, float ch1, float ch2)
{
    const float* ms = ROOM_MS;
    float sc = 1.0f, t60 = 1.0f;
    if (algo == REV_ROOM)
    {
        ms = ROOM_MS; sc = 0.35f + 0.65f * size; t60 = 0.2f * powf_ (15.0f, time);
        early = ch1; diff = ch2; modDepth = 0.15f; shimmer = 0.0f;
    }
    else if (algo == REV_HALL)
    {
        ms = HALL_MS; sc = 0.5f + 0.9f * size; t60 = 0.8f * powf_ (12.5f, time);
        early = 0.3f; diff = ch1; modDepth = ch2 * 1.2f; shimmer = 0.0f;
    }
    else
    {
        ms = SPACE_MS; sc = 0.6f + 1.1f * size; t60 = 2.0f * powf_ (15.0f, time);
        early = 0.0f; diff = 0.85f; modDepth = 0.4f + ch2 * 2.5f; shimmer = ch1;
    }
    scale = sc;
    if (mode != algo)
    {
        // a different algorithm starts from silence
        for (int j = 0; j < FDN_N; ++j) { for (int i = 0; i < FDN_LEN; ++i) line[j][i] = 0.0f; lp[j] = 0.0f; }
        for (int i = 0; i < SHIM_LEN; ++i) sh[i] = 0.0f;
        shOut = shLp = 0.0f;
        mode = algo;
    }
    const float alpha = maxf (1.0f - 0.92f * damp, 0.08f);      // RT60(high) / RT60(low)
    const float maxLen = (float) (FDN_LEN - 8) - modDepth * 0.001f * sr;
    for (int j = 0; j < FDN_N; ++j)
    {
        len[j] = minf (ms[j] * sc * 0.001f * sr, maxLen);
        const float lg = -3.0f * len[j] / (t60 * sr);               // log10 of the line gain
        // shimmer re-injects energy one octave up: the network itself decays a little faster to compensate
        gain[j] = pow10f_ (lg) * (1.0f - 0.08f * shimmer);
        bco[j] = clampf (0.5756463f * lg * (1.0f - 1.0f / (alpha * alpha)), 0.0f, 0.97f);   // Jot: ln10/4 log10(g) (1 - 1/a^2)
    }
}

void FdnReverb::process (const float* in, float* oL, float* oR, int n, float level)
{
    const float apg = 0.72f * diff;
    const float md = modDepth * 0.001f * sr;
    const float erScale = scale * 0.001f * sr;
    const float shInc = 1.0f / 4096.0f;
    const float inScale = 0.5f;
    for (int i = 0; i < n; ++i)
    {
        float x = in[i];
        er[erW] = x;
        // input diffusion: four Schroeder all-passes
        float d = x;
        if (apg > 0.001f)
            for (int k = 0; k < 4; ++k)
            {
                int r = apW[k] - apLen[k]; if (r < 0) r += 2048;
                const float del = ap[k][r];
                const float v = d + apg * del;
                ap[k][apW[k]] = v;
                d = del - apg * v;
                apW[k] = (apW[k] + 1) & 2047;
            }
        // read the lines (modulated, linear interpolation), absorption filters
        float y[FDN_N];
        float outL = 0.0f, outR = 0.0f;
        for (int j = 0; j < FDN_N; ++j)
        {
            float dl = len[j];
            if (md > 0.0f) { ph[j] += rate[j]; if (ph[j] >= 1.0f) ph[j] -= 1.0f; dl += md * (0.5f + 0.5f * sin2pi (ph[j])); }
            float rp = (float) w - dl;
            while (rp < 0.0f) rp += (float) FDN_LEN;
            const int i0 = (int) rp; const float fr = rp - (float) i0;
            const float a0 = line[j][i0 & FDN_MASK], a1 = line[j][(i0 + 1) & FDN_MASK];
            const float o = a0 + (a1 - a0) * fr;
            lp[j] = gain[j] * (1.0f - bco[j]) * o + bco[j] * lp[j];
            y[j] = lp[j];
            outL += SGN_L[j] * y[j]; outR += SGN_R[j] * y[j];
        }
        // Hadamard feedback matrix (orthogonal, lossless)
        for (int h = 1; h < FDN_N; h <<= 1)
            for (int a = 0; a < FDN_N; a += h << 1)
                for (int b = a; b < a + h; ++b) { const float p = y[b], q = y[b + h]; y[b] = p + q; y[b + h] = p - q; }
        // octave-up shimmer fed back into the network (space)
        float shim = 0.0f;
        if (shimmer > 0.001f)
        {
            sh[shW] = (outL + outR) * 0.0625f;
            shPh -= shInc; if (shPh < 0.0f) shPh += 1.0f;
            float s = 0.0f;
            for (int t = 0; t < 2; ++t)
            {
                float p = shPh + 0.5f * (float) t; if (p >= 1.0f) p -= 1.0f;
                float rp = (float) shW - p * 4096.0f - 2.0f;
                while (rp < 0.0f) rp += (float) SHIM_LEN;
                const int i0 = (int) rp; const float fr = rp - (float) i0;
                const float a0 = sh[i0 & (SHIM_LEN - 1)], a1 = sh[(i0 + 1) & (SHIM_LEN - 1)];
                const float win = sin2pi (0.5f * p);
                s += (a0 + (a1 - a0) * fr) * win * win;
            }
            shW = (shW + 1) & (SHIM_LEN - 1);
            shLp += (s - shLp) * 0.35f;
            shim = tanhf_ (shLp * shimmer * 2.2f) * 0.45f;
        }
        const float inj = (d + shim) * inScale;
        for (int j = 0; j < FDN_N; ++j) line[j][w] = y[j] * 0.35355339f + inj * SGN_IN[j];
        w = (w + 1) & FDN_MASK;
        // early reflections (room, a little in the hall)
        if (early > 0.001f)
        {
            for (int k = 0; k < 8; ++k)
            {
                int r = erW - (int) (ER_MS[k] * erScale); if (r < 0) r += ER_LEN;
                const float v = er[r & (ER_LEN - 1)] * early * 0.5f * (1.0f - 0.09f * (float) k);
                if (k & 1) outR += v * 1.4f, outL += v * 0.4f; else outL += v * 1.4f, outR += v * 0.4f;
            }
        }
        erW = (erW + 1) & (ER_LEN - 1);
        oL[i] += outL * 0.42f * level;
        oR[i] += outR * 0.42f * level;
    }
    for (int j = 0; j < FDN_N; ++j) if (isBad (lp[j]) || absf (lp[j]) < 1e-25f) lp[j] = 0.0f;
    if (isBad (shLp)) shLp = 0.0f;
}

// --------------------------------------------------------------------------- spring
void SpringReverb::init (float sampleRate)
{
    sr = sampleRate;
    K = mini (maxi ((int) (sr / 9600.0f + 0.5f), 2), SPR_KMAX);
    hp = 0;
    for (int t = 0; t < 2; ++t)
    {
        SpringTank& k = tank[t];
        for (int s = 0; s < SPR_STAGES; ++s) for (int i = 0; i < SPR_KMAX; ++i) k.xh[s][i] = k.yh[s][i] = 0.0f;
        for (int i = 0; i < SPR_LEN; ++i) k.dl[i] = 0.0f;
        k.dw = 0; k.fbLp = k.outLp1 = k.outLp2 = k.hpX = k.hpY = k.wander = 0.0f;
    }
    a = -0.6f; len[0] = len[1] = 1000.0f; g[0] = g[1] = 0.0f; lpC = 0.5f; outC = 0.5f;
    hpR = 1.0f - TWO_PI * 90.0f / sr; drip = emph = 0.0f; rng01 = 0.0f;
    rng.seed (0x5B1u);
}

void SpringReverb::configure (float time, float size, float damp, float tension, float dripAmt)
{
    a = -(0.42f + 0.33f * tension);
    const float t60 = 0.6f * powf_ (10.0f, time);
    for (int t = 0; t < 2; ++t)
    {
        const float at = t ? a * 0.88f : a;            // the right spring is a little stiffer: decorrelated chirps
        const float cascade = (float) (SPR_STAGES * K) * (1.0f - at) / (1.0f + at);
        len[t] = minf ((25.0f + 45.0f * size) * (t == 0 ? 1.0f : 1.13f) * 0.001f * sr, (float) (SPR_LEN - 64));
        g[t] = exp2f_ (-3.0f * 3.3219281f * (len[t] + cascade) / (t60 * sr));
    }
    const float fc = 1500.0f + 6500.0f * (1.0f - damp);
    lpC = expf_ (-TWO_PI * fc / sr);
    outC = expf_ (-TWO_PI * 5200.0f / sr);
    drip = dripAmt;
    emph = dripAmt;
}

void SpringReverb::process (const float* in, float* oL, float* oR, int n, float level)
{
    float* outs[2] = { oL, oR };
    const float wanderAmt = drip * 0.0006f * sr;
    for (int i = 0; i < n; ++i)
    {
        const float x = in[i];
        for (int t = 0; t < 2; ++t)
        {
            SpringTank& k = tank[t];
            const float at = t ? a * 0.88f : a;
            // input: transients pushed harder into the tank give the "drip"
            const float hpv = x - k.hpX + 0.95f * k.hpY; k.hpX = x; k.hpY = hpv;
            float u = x * 0.5f + emph * hpv * 1.6f;
            // lossy feedback loop (slowly wandering length: the springs never sit still)
            if ((i & 31) == 0) k.wander += (rng.bi() * wanderAmt - k.wander) * 0.05f;
            float rp = (float) k.dw - len[t] - k.wander - 2.0f;
            while (rp < 0.0f) rp += (float) SPR_LEN;
            const int i0 = (int) rp; const float fr = rp - (float) i0;
            const float o = k.dl[i0 & (SPR_LEN - 1)] + (k.dl[(i0 + 1) & (SPR_LEN - 1)] - k.dl[i0 & (SPR_LEN - 1)]) * fr;
            k.fbLp = o * (1.0f - lpC) + k.fbLp * lpC;
            u += g[t] * k.fbLp;
            // dispersion: cascade of stretched all-passes (a + z^-K) / (1 + a z^-K)
            for (int s = 0; s < SPR_STAGES; ++s)
            {
                const float xo = k.xh[s][hp], yo = k.yh[s][hp];
                const float y = at * (u - yo) + xo;
                k.xh[s][hp] = u; k.yh[s][hp] = y;
                u = y;
            }
            k.dl[k.dw] = u;
            k.dw = (k.dw + 1) & (SPR_LEN - 1);
            // the tank output is band-limited (springs carry little above ~5 kHz)
            k.outLp1 = u * (1.0f - outC) + k.outLp1 * outC;
            k.outLp2 = k.outLp1 * (1.0f - outC) + k.outLp2 * outC;
            outs[t][i] += k.outLp2 * 0.9f * level;
        }
        if (++hp >= K) hp = 0;
    }
    for (int t = 0; t < 2; ++t)
    {
        SpringTank& k = tank[t];
        if (isBad (k.fbLp) || isBad (k.outLp2)) { const float sv = sr; init (sv); return; }
        if (absf (k.fbLp) < 1e-25f) k.fbLp = 0.0f;
    }
}

} // namespace sk
