// IMPAKT — GLITCH (see sk_glitch.h)
#include "sk_glitch.h"

namespace sk {

namespace {
const double GRID16[GL_GRIDS] = { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };      // in 16ths
const int    REPEATS[7] = { 2, 3, 4, 6, 8, 12, 16 };
const float  RATES[6] = { 0.5f, 0.75f, 1.5f, 2.0f, -1.0f, 0.0f };           // 0: tape stop
// the codec's bands, in bins of 93.75 Hz at 48 kHz (closer together in the lows, like the ear's)
const int    BAND_EDGE[25] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 17, 20, 24, 29, 35, 42, 51, 62, 76, 93, 114, 140, 257 };
inline float fadeIn (float amt) { const float o = clampf (amt * (1.0f / 0.06f), 0.0f, 1.0f); return o * o * (3.0f - 2.0f * o); }
// distance to the nearest edge of [0, len), as a gain: the seams of every slice fade over 0.7 ms
inline float edge (double t, double len)
{
    const double F = len * 0.25 < 32.0 ? len * 0.25 : 32.0;
    if (F < 1.0) return 1.0f;
    const double a = t / F, b = (len - t) / F;
    const double g = a < b ? a : b;
    return g >= 1.0 ? 1.0f : (g <= 0.0 ? 0.0f : (float) g);
}
inline double floord (double x) { return __builtin_floor (x); }
inline float smooth01 (float x) { const float o = clampf (x, 0.0f, 1.0f); return o * o * (3.0f - 2.0f * o); }

// the tape machines (ANA): playback high cut and low cut at full LOSS (Hz), head bump (Hz, dB), wow (samples at
// 48 kHz, rate in Hz), flutter (samples, Hz), saturation drive, hiss level, failures per second at full LOSS
// (level drops, snags, crinkles x 25), azimuth error between the two heads (samples), mono (a one-head machine),
// make-up gain at full LOSS (dB: the narrow machines lose the lows and the highs)
struct TapeModel { float lp, hp, fb, bump, wow, wowHz, fl, flHz, sat, hiss, drop, snag, crk, az, mono, mk; };
const TapeModel TAPE[GL_AMODES] = {
    //  lp      hp     fb     bump  wow    wowHz  fl    flHz   sat   hiss     drop  snag  crk   az    mono
    { 8000.f,  35.f,  65.f, 3.0f,  70.f, 0.55f, 1.2f,  9.0f, 1.8f, 0.0035f, 0.25f, 0.15f, 0.3f, 1.2f, 0.0f, 0.0f },   // CASS
    { 7500.f,  50.f,  90.f, 2.0f,  60.f, 0.40f, 0.8f,  7.5f, 1.4f, 0.0028f, 0.35f, 0.10f, 0.15f, 0.6f, 0.0f, 1.0f },  // VHS
    { 3800.f, 280.f, 180.f, 4.0f,  90.f, 0.80f, 2.6f, 13.0f, 2.8f, 0.0055f, 0.40f, 0.30f, 0.5f, 0.0f, 1.0f, 7.0f },   // MICRO
    { 5000.f, 140.f, 150.f, 5.0f, 220.f, 0.45f, 2.0f,  5.5f, 3.5f, 0.0070f, 0.50f, 0.50f, 0.6f, 0.0f, 1.0f, 4.0f },   // TOY
    { 6500.f,  60.f,  60.f, 3.0f, 300.f, 0.65f, 3.0f, 10.0f, 2.2f, 0.0045f, 1.00f, 1.00f, 1.0f, 2.5f, 0.0f, 2.0f },   // WORN
};
// a zero-delay state-variable filter (its coefficients for a cutoff and a damping 1/Q)
struct SvfK { float a1, a2, a3, k; };
inline SvfK svfK (float fc, float sr, float q)
{
    const float g = tanpi (clampf (fc / sr, 1e-5f, 0.45f)), k = 1.0f / q;
    SvfK c; c.a1 = 1.0f / (1.0f + g * (g + k)); c.a2 = g * c.a1; c.a3 = g * c.a2; c.k = k;
    return c;
}
inline void svf (float* st, float x, const SvfK& c, float& lp, float& bp)
{
    const float v3 = x - st[1], v1 = c.a1 * st[0] + c.a2 * v3, v2 = st[1] + c.a2 * st[0] + c.a3 * v3;
    st[0] = 2.0f * v1 - st[0]; st[1] = 2.0f * v2 - st[1];
    lp = v2; bp = v1;
}
// a 4-point Hermite read of the tape (the playback head between two samples)
inline float hermite (const float* b, float pos)
{
    const float fp = floorf_ (pos); const int i = (int) fp; const float t = pos - fp;
    const int M = 2048 - 1;
    const float y0 = b[(i - 1) & M], y1 = b[i & M], y2 = b[(i + 1) & M], y3 = b[(i + 2) & M];
    const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}
// a sample position (a whole number held in a double) to its place in the ring buffer
inline int ringIdx (double p) { const double q = p - floord (p * (1.0 / (double) Glitch::BUF)) * (double) Glitch::BUF; return ((int) q) & Glitch::MASK; }
}

void Glitch::init (float sampleRate)
{
    sr = sampleRate;
    fft.init (FN);
    for (int j = 0; j < FN; ++j) win[j] = sqrtf_ (0.5f - 0.5f * cos2pi ((float) j / (float) FN));
    rng.seed (0x6E17C4u);
    reset();
}

void Glitch::reset()
{
    for (int i = 0; i < BUF; ++i) bufL[i] = bufR[i] = 0.0f;
    w = 0.0;
    evType = GL_PASS; evR = 2; evBack = 1; evStart = 0.0; evLen = 1.0; evRate = 1.0; evPh = 0.0; holdStart = 0.0;
    divIdx = -1.0; gridPrev = 0.0;
    gFade = 0.0f; onMix = 0.0f;
    hL = hR = 0.0f; hCnt = 0.0f; dropG = 1.0f; dropLeft = 0; cmix = 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        for (int j = 0; j < FN; ++j) inBuf[c][j] = olaBuf[c][j] = dryD[c][j] = 0.0f;
        for (int b = 0; b < 24; ++b) bandG[c][b] = 1.0f;
        for (int k = 0; k < NB; ++k) prevMag[c][k] = prevRe[c][k] = prevIm[c][k] = 0.0f;
    }
    ring = 0; hopCnt = 0; lossMix = 0.0f; pktLeft = 0; pktDrop = 0;
    for (int h = 0; h < HIST; ++h) hist[h] = -1;
    curProg = 0.0f; curDetail = 0.0f; lossBw = 20000.0f; lossCents = 0.0f;
    for (int c = 0; c < 2; ++c) for (int j = 0; j < TBUF; ++j) tapeBuf[c][j] = 0.0f;
    tw = 0; anaMix = 0.0f;
    tapeReset();
}

void Glitch::tapeReset()
{
    for (int c = 0; c < 2; ++c)
    {
        eLp[c] = dLp[c] = hsLp[c] = crk[c] = 0.0f;
        hpS[c][0] = hpS[c][1] = bmS[c][0] = bmS[c][1] = lpS[c][0] = lpS[c][1] = 0.0f;
    }
    wowPh = 0.0; flPh = 0.0; wr0 = wr1 = 0.0f; wrPos = 0.0f; flN = flN2 = 0.0f;
    snagA = 0.0f; snagT = snagLen = 1.0f; dropT = 1.0f; dropEnv = 1.0f; dropLeft2 = 0;
    dPrev = (float) FN;
}

float Glitch::readBuf (const float* b, double pos) const
{
    const double i0 = floord (pos);
    const float t = (float) (pos - i0);
    const int k = ringIdx (i0);
    const float a = b[k], c = b[(k + 1) & MASK];
    return a + (c - a) * t;
}

void Glitch::newDivision (const GlitchParams& p, double now)
{
    for (int h = 0; h < HIST - 1; ++h) hist[h] = hist[h + 1];
    evStart = now;
    // what the buffer holds before this division (and it never reaches back further than its length)
    const double avail = now < (double) (BUF - 256) ? now : (double) (BUF - 256);
    if (p.hld)
    {
        if (evType != GL_HOLD) holdStart = avail >= evLen ? now - evLen : now;   // the last division
        evType = GL_HOLD;
        hist[HIST - 1] = evType;
        return;
    }
    const float pr = p.rpt * 0.85f, pb = p.brk * 0.7f, pn = p.bnd * 0.7f;
    const float tot = pr + pb + pn, sc = tot > 0.95f ? 0.95f / tot : 1.0f;
    const float u = rng.uni();
    evType = GL_PASS;
    if (u < pr * sc)
    {
        const int top = mini ((int) (p.rpt * 6.99f), 6);
        evR = REPEATS[mini ((int) (rng.uni () * (float) (top + 1)), top)];
        evType = GL_REPEAT;
    }
    else if (u < (pr + pb) * sc)
    {
        const int maxBack = mini (4, (int) (avail / evLen) - 1);
        if (maxBack >= 1 && rng.uni() < 0.65f) { evBack = 1 + mini ((int) (rng.uni() * (float) maxBack), maxBack - 1); evType = GL_JUMP; }
        else evType = GL_DROP;
    }
    else if (u < tot * sc)
    {
        if (avail >= evLen + 64.0)
        {
            const int top = mini (5, 1 + (int) (p.bnd * 5.99f));
            evRate = RATES[mini ((int) (rng.uni() * (float) (top + 1)), top)];
            evPh = evRate < 0.0f ? evLen - 1.0 : 0.0;
            evType = GL_BEND;
        }
        else { evR = 2; evType = GL_REPEAT; }
    }
    hist[HIST - 1] = evType;
}

void Glitch::codecFrame (int ch, const GlitchParams& p)
{
    const float L = p.los;
    for (int j = 0; j < FN; ++j) fx[j] = inBuf[ch][(ring + j) & (FN - 1)] * win[j];
    fft.forward (fx, re, im);
    const int mode = p.lmd;
    if (L > 0.002f || mode != GL_LSTD)
    {
        // the bandwidth: all of it at zero, down to about 4 kHz at the top
        const float nyq = 0.5f * sr, bwHz = nyq - (nyq - 4000.0f) * powf_ (L, 0.7f);
        const int kc = mini (NB - 1, (int) (bwHz * (float) FN / sr));
        lossBw = (float) kc * sr / (float) FN;
        float E[24], Emax = 1e-20f;
        for (int b = 0; b < 24; ++b)
        {
            float e = 0.0f; int cnt = 0;
            for (int k = BAND_EDGE[b]; k < BAND_EDGE[b + 1] && k < kc; ++k) { e += re[k] * re[k] + im[k] * im[k]; ++cnt; }
            E[b] = cnt ? e / (float) cnt : 0.0f;
            Emax = maxf (Emax, E[b]);
        }
        // the bands the codec can afford: those within its dynamic range (90 dB at 0, 12 dB at 1)
        const float thr = Emax * exp2f_ (-(90.0f - 78.0f * L) * (0.33219281f));
        const float aB = 0.06f + 0.94f * p.spd, aM = 0.25f + 0.75f * p.spd;
        const float qf = 0.01f + 1.8f * L * L;
        // PKT modes: packets of 2..8 frames, a share of them lost or repeated (decided once for both channels)
        if (ch == 0 && (mode == GL_LPKTL || mode == GL_LPKTR))
        {
            if (pktLeft <= 0) { pktLeft = 2 + (int) (rng.uni() * 7.0f); pktDrop = rng.uni() < 0.6f * L ? 1 : 0; }
            --pktLeft;
        }
        const bool lost = (mode == GL_LPKTL || mode == GL_LPKTR) && pktDrop;
        for (int b = 0; b < 24; ++b)
        {
            float& g = bandG[ch][b];
            g += ((E[b] > thr ? 1.0f : 0.0f) - g) * aB;
            const float step = sqrtf_ (E[b]) * qf;
            for (int k = BAND_EDGE[b]; k < BAND_EDGE[b + 1]; ++k)
            {
                const float xr = re[k], xi = im[k], mag = sqrtf_ (xr * xr + xi * xi);
                float mq = step > 0.0f ? floorf_ (mag / step + 0.5f) * step : mag;
                if (k >= kc) mq *= clampf (1.0f - (float) (k - kc) * 0.25f, 0.0f, 1.0f);
                float& pm = prevMag[ch][k];
                pm += (mq * g - pm) * aM;
                rm[k] = pm; rt[k] = 1.0f;
            }
        }
        // RESO: the peaks out of the codec's mush (each bin against its neighbourhood, the frame's energy kept),
        // then a resonance just under the edge of the band. It grows with LOSS (full from 30 %).
        const float rs = p.res * minf (1.0f, L * (1.0f / 0.3f));
        if (rs > 1e-3f)
        {
            const int W = 4, top = mini (NB - 1, kc + 4);
            rc[0] = 0.0f;
            for (int k = 0; k < NB; ++k) rc[k + 1] = rc[k] + rm[k];
            const float gam = 1.5f * rs;
            float e0 = 0.0f, e1 = 0.0f;
            for (int k = 0; k <= top; ++k)
            {
                const int lo = k - W < 0 ? 0 : k - W, hi = mini (NB - 1, k + W);
                const float env = (rc[hi + 1] - rc[lo]) / (float) (hi - lo + 1);
                const float r = (rm[k] + 1e-12f) / (env + 1e-12f);
                const float gs = minf (6.0f, powf_ (r, gam));
                const float m2 = rm[k] * gs;
                e0 += rm[k] * rm[k]; e1 += m2 * m2;
                rt[k] = gs;
            }
            const float nrm = e1 > 1e-24f ? sqrtf_ (e0 / e1) : 1.0f;
            for (int k = 0; k < NB; ++k) rt[k] *= nrm;
            // the bump: a gaussian 3 widths each side (nothing left beyond)
            const float kr = 0.8f * (float) kc, wd = maxf (1.5f, 0.05f * (float) kc), A = 3.5f * rs, iw = 1.0f / wd;
            const int b0 = (int) maxf (0.0f, kr - 3.0f * wd), b1 = mini (top, (int) (kr + 3.0f * wd) + 1);
            for (int k = b0; k <= b1; ++k)
            {
                const float u = ((float) k - kr) * iw;
                rt[k] *= 1.0f + A * expf_ (-u * u);
            }
        }
        for (int k = 0; k < NB; ++k)
        {
            const float xr = re[k], xi = im[k], mag = sqrtf_ (xr * xr + xi * xi);
            const float s = mag > 1e-12f ? rm[k] * rt[k] / mag : 0.0f;
            float yr = xr * s, yi = xi * s;
            switch (mode)
            {
                case GL_LINV: yr = xr - yr; yi = xi - yi; break;
                case GL_LJIT:
                {
                    // phase errors, larger in the highs; the magnitudes stay the source's (RESO still shapes them)
                    const float ph = rng.bi() * 0.5f * L * (0.2f + 0.8f * (float) k / (float) NB);
                    const float c = cos2pi (ph), sn = sin2pi (ph);
                    yr = (xr * c - xi * sn) * rt[k]; yi = (xr * sn + xi * c) * rt[k];
                    if (k >= kc) { yr *= 0.0f; yi *= 0.0f; }
                    break;
                }
                case GL_LPKTL: if (lost) { yr = yi = 0.0f; } break;
                case GL_LPKTR: if (lost) { yr = prevRe[ch][k]; yi = prevIm[ch][k]; } break;
                default: break;
            }
            if (! lost) { prevRe[ch][k] = yr; prevIm[ch][k] = yi; }
            re[k] = yr; im[k] = yi;
        }
    }
    else lossBw = 20000.0f;
    fft.inverse (re, im, fx);
    for (int j = 0; j < FN; ++j) olaBuf[ch][(ring + j) & (FN - 1)] += fx[j] * win[j] * 0.5f;
}

void Glitch::process (float* L, float* R, int n, const GlitchParams& p, double pos16, double inc)
{
    const float onT = p.on ? 1.0f : 0.0f;
    if (onT <= 0.0f && onMix <= 0.0f) return;
    const float onStep = 1.0f / (0.01f * sr), xStep = 1.0f / 32.0f;
    const double grid = GRID16[idxOf (p.tim, GL_GRIDS)];
    if (grid != gridPrev) { gridPrev = grid; divIdx = -1.0; }
    const double lenS = grid / (inc > 1e-9 ? inc : 1e-9);
    // corrupt
    const float cT = fadeIn (p.crp), c = p.crp;
    const float div = 1.0f + c * c * 47.0f, levels = exp2f_ (16.0f - 13.0f * c);
    const float dropRate = (1.0f + 30.0f * c * c) / sr, dropLen = (0.12f - 0.105f * c) * sr;
    const float dk = 1.0f + 24.0f * minf (c, 0.5f), dkN = 1.0f / tanhf_ (dk), hg = 1.0f + 30.0f * maxf (c - 0.5f, 0.0f);
    // loss
    const float lmT = p.on && p.los > 0.002f ? 1.0f : 0.0f, lStep = 1.0f / 1024.0f;
    const float anaT = p.ana ? 1.0f : 0.0f;
    if (lmT > 0.0f && lossMix <= 0.0f)
    {
        // entering the loss path: it starts empty (its first frames fill up as the fade comes in)
        for (int ch = 0; ch < 2; ++ch) for (int j = 0; j < FN; ++j) { inBuf[ch][j] = olaBuf[ch][j] = 0.0f; dryD[ch][j] = 0.0f; }
        for (int ch = 0; ch < 2; ++ch) for (int j = 0; j < TBUF; ++j) tapeBuf[ch][j] = 0.0f;
        hopCnt = 0; anaMix = anaT; tapeReset();
    }
    if (lmT <= 0.0f && lossMix <= 0.0f) anaMix = anaT;
    if (anaT > 0.0f && anaMix <= 0.0f)
    {
        // the tape starts from rest (the fade covers it); its emphasis filters already follow the signal
        const float e0 = eLp[0], e1 = eLp[1]; tapeReset(); eLp[0] = dLp[0] = e0; eLp[1] = dLp[1] = e1;
    }
    // the tape, for this chunk
    const bool tapeOn = anaT > 0.0f || anaMix > 0.0f;
    const TapeModel& M = TAPE[p.amd < 0 ? 0 : (p.amd >= GL_AMODES ? GL_AMODES - 1 : p.amd)];
    const float La = p.los, s48 = sr * (1.0f / 48000.0f);
    const float tA = smooth01 (La * 10.0f);                                     // transparent at 0, all of it from 10 %
    const float wB = minf (1.0f, 2.0f * (1.0f - p.spd)), fB = minf (1.0f, 2.0f * p.spd);
    const float wowAmp = M.wow * powf_ (maxf (La, 1e-6f), 1.3f) * wB * s48, flAmp = M.fl * La * fB * s48;
    const float sg = 1.0f + M.sat * La, sgN = powf_ (sg, -0.75f), bias = 0.06f * La, tb = tanhf_ (sg * bias);
    const float kE = 2.0f, aE = 1.0f - expf_ (-6.2831853f * 2500.0f / sr), dE = 1.0f / (1.0f + kE * (1.0f - aE));
    const float hiss = M.hiss * La * sqrtf_ (La), aH = 1.0f - expf_ (-6.2831853f * 1500.0f / sr);
    const float tq = minf (1.0f, La * (1.0f / 0.3f));
    const float lpBase = 20000.0f * powf_ (M.lp * (1.0f / 20000.0f), powf_ (maxf (La, 1e-6f), 0.7f));
    const float lpF = lpBase * (0.3f + 0.7f * dropEnv);
    const SvfK kHp = svfK (12.0f * powf_ (M.hp * (1.0f / 12.0f), powf_ (maxf (La, 1e-6f), 0.8f)), sr, 0.7071f);
    const SvfK kBm = svfK (M.fb, sr, 1.2f);
    const SvfK kLp = svfK (lpF, sr, 0.7071f + 2.3f * p.res * tq);
    const float bmG = (exp2f_ ((M.bump + 4.0f * p.res) * La * (0.16609640f)) - 1.0f) * kBm.k;   // (10^(dB/20) - 1) / Q
    const float az = M.az * La * s48, mono = M.mono * La;
    const float tDropRate = M.drop * 2.0f * La * La / sr, snagRate = M.snag * 1.2f * La * La / sr, crkRate = M.crk * 25.0f * La * La / sr;
    const float aAtt = 1.0f / (0.004f * sr), aRel = 1.0f / (0.015f * sr), crkDec = expf_ (-1.0f / (0.0008f * sr));
    const float aF = 1.0f - expf_ (-6.2831853f * 2.0f * M.flHz / sr), nF = sqrtf_ (3.0f * (2.0f - aF) / aF) * 1.4f;
    const float Dmax = (float) FN - 8.0f, mkG = exp2f_ (M.mk * La * 0.16609640f);
    float d0 = dPrev;
    if (tapeOn) lossBw = lpF;
    const float mix = p.mix;
    float* X[2] = { L, R };
    for (int i = 0; i < n; ++i)
    {
        const float xl = L[i], xr = R[i];
        const double now = w;
        const int wi = ringIdx (now);
        bufL[wi] = xl; bufR[wi] = xr;
        w += 1.0;
        // a new division of the grid
        const double d = floord ((pos16 + (double) i * inc) / grid);
        if (d != divIdx) { divIdx = d; evLen = lenS; newDivision (p, now); }
        // the bender
        const double t = now - evStart;
        bool fromBuf = true; double src = (double) now; float env = 1.0f, rate = 1.0f;
        switch (evType)
        {
            case GL_REPEAT: { const double sl = evLen / (double) evR, tm = t - floord (t / sl) * sl; src = evStart + tm; env = edge (tm, sl); break; }
            case GL_JUMP:   src = evStart - (double) evBack * evLen + t; env = edge (t, evLen); break;
            case GL_DROP:   env = 0.0f; break;
            case GL_BEND:
            {
                if (evRate == 0.0)      rate = maxf (0.0f, 1.0f - (float) (t / evLen));                                        // tape stop
                else if (evRate < 0.0)  rate = -1.0f;                                                                            // backwards
                else                    rate = 1.0f + ((float) evRate - 1.0f) * minf (1.0f, (float) (t / (0.25 * evLen)));      // a varispeed glide
                src = evStart - evLen + evPh;
                env = edge (evPh, evLen);
                evPh += rate;
                if (evPh >= evLen) evPh -= evLen;
                if (evPh < 0.0) evPh += evLen;
                break;
            }
            case GL_HOLD:   { const double tm = now - holdStart, tl = tm - floord (tm / evLen) * evLen; src = holdStart + tl; env = edge (tl, evLen); break; }
            default:        fromBuf = false; break;
        }
        gFade += fromBuf ? (gFade < 1.0f ? xStep : 0.0f) : (gFade > 0.0f ? -xStep : 0.0f);
        gFade = clampf (gFade, 0.0f, 1.0f);
        float yl = xl, yr = xr;
        if (gFade > 0.0f)
        {
            const float bl = fromBuf ? readBuf (bufL, src) * env : 0.0f, br = fromBuf ? readBuf (bufR, src) * env : 0.0f;
            yl = xl + (bl - xl) * gFade; yr = xr + (br - xr) * gFade;
        }
        curProg = (float) clampf ((float) (t / evLen), 0.0f, 1.0f);
        // corrupt
        cmix += (cT - cmix) * (1.0f / 480.0f);
        if (absf (cT - cmix) < 1e-4f) cmix = cT;
        if (cmix > 1e-5f)
        {
            float zl = yl, zr = yr;
            if (p.cmd == GL_CDECIM)
            {
                hCnt += 1.0f;
                if (hCnt >= div) { hCnt -= div; if (hCnt > div) hCnt = 0.0f; hL = yl; hR = yr; }
                zl = floorf_ (hL * levels + 0.5f) / levels; zr = floorf_ (hR * levels + 0.5f) / levels;
            }
            else if (p.cmd == GL_CDROP)
            {
                if (dropLeft > 0) --dropLeft;
                else if (rng.uni() < dropRate) dropLeft = (int) (dropLen * (0.5f + rng.uni()));
                dropG += ((dropLeft > 0 ? 0.0f : 1.0f) - dropG) * (1.0f / 24.0f);
                zl = yl * dropG; zr = yr * dropG;
            }
            else
            {
                if (c <= 0.5f) { zl = tanhf_ (yl * dk) * dkN; zr = tanhf_ (yr * dk) * dkN; }
                else { zl = clampf (tanhf_ (yl * dk) * dkN * hg, -1.0f, 1.0f) * 0.8f; zr = clampf (tanhf_ (yr * dk) * dkN * hg, -1.0f, 1.0f) * 0.8f; }
            }
            yl += (zl - yl) * cmix; yr += (zr - yr) * cmix;
        }
        // the codec path (10.7 ms late) and the direct one, faded
        lossMix += lmT > lossMix ? lStep : (lmT < lossMix ? -lStep : 0.0f);
        lossMix = clampf (lossMix, 0.0f, 1.0f);
        float outL = xl + (yl - xl) * mix, outR = xr + (yr - xr) * mix;
        if (lossMix > 0.0f || lmT > 0.0f)
        {
            float zL = olaBuf[0][ring], zR = olaBuf[1][ring];
            const float dL = dryD[0][ring], dR = dryD[1][ring];
            olaBuf[0][ring] = olaBuf[1][ring] = 0.0f;
            inBuf[0][ring] = yl; inBuf[1][ring] = yr;
            dryD[0][ring] = xl; dryD[1][ring] = xr;
            ring = (ring + 1) & (FN - 1);
            if (++hopCnt >= HOP) { hopCnt = 0; if (anaMix < 1.0f || anaT < 1.0f) { codecFrame (0, p); codecFrame (1, p); } }
            // the tape (ANA): recorded, then played back FN samples later, give or take the transport
            anaMix += anaT > anaMix ? lStep : (anaT < anaMix ? -lStep : 0.0f);
            anaMix = clampf (anaMix, 0.0f, 1.0f);
            if (anaMix > 0.0f)
            {
                // the transport: wow (a sine and a smooth random), flutter (a sine and a band of noise), snags
                wowPh += (double) (M.wowHz / sr); if (wowPh >= 1.0) wowPh -= 1.0;
                wrPos += M.wowHz * 0.7f / sr;
                if (wrPos >= 1.0f) { wrPos -= 1.0f; wr0 = wr1; wr1 = rng.bi(); }
                const float wrv = wr0 + (wr1 - wr0) * (0.5f - 0.5f * cos2pi (0.5f * wrPos));
                const float wow = 0.55f * sin2pi ((float) wowPh) + 0.45f * wrv;
                flPh += (double) (M.flHz / sr); if (flPh >= 1.0) flPh -= 1.0;
                flN += (rng.bi() - flN) * aF; flN2 += (flN - flN2) * aF;
                const float fl = 0.6f * sin2pi ((float) flPh) + 0.4f * clampf (flN2 * nF, -2.0f, 2.0f);
                // the failures, one draw for the three (they are rare: at most a few per second)
                const float uf = rng.uni();
                const int fail = uf < tDropRate ? 1 : (uf < tDropRate + snagRate ? 2 : (uf < tDropRate + snagRate + crkRate ? 3 : 0));
                float snag = 0.0f;
                if (snagT >= snagLen) { if (fail == 2) { snagT = 0.0f; snagLen = (0.06f + 0.14f * rng.uni()) * sr; snagA = (8.0f + 25.0f * rng.uni()) * s48 * La; } }
                else { snag = snagA * (0.5f - 0.5f * cos2pi (snagT / snagLen)); snagT += 1.0f; }
                const float D = (float) FN + clampf (wowAmp * wow + flAmp * fl + snag, -Dmax, Dmax);
                // failures: level drops (the highs go first), crinkles
                if (dropLeft2 > 0) --dropLeft2;
                else if (fail == 1) { dropLeft2 = (int) ((0.02f + 0.2f * rng.uni()) * sr); dropT = 0.08f + 0.5f * rng.uni(); }
                dropEnv += ((dropLeft2 > 0 ? dropT : 1.0f) - dropEnv) * (dropLeft2 > 0 ? aAtt : aRel);
                if (fail == 3) { const float a = (0.01f + 0.05f * rng.uni()) * La; crk[0] = a; crk[1] = a * (0.4f + 0.6f * rng.uni()); }
                const float amp = dropEnv * mkG * (1.0f + 0.03f * fB * La * fl);
                float in2[2] = { yl, yr }, out2[2];
                if (mono > 0.0f) { const float m = 0.5f * (yl + yr); in2[0] += (m - yl) * mono; in2[1] += (m - yr) * mono; }
                for (int c = 0; c < 2; ++c)
                {
                    // record: emphasis, the saturation (a touch of asymmetry), the emphasis undone
                    const float x = in2[c];
                    eLp[c] += aE * (x - eLp[c]);
                    const float e = (1.0f + kE) * x - kE * eLp[c];
                    const float sat = (tanhf_ (sg * (e + bias)) - tb) * sgN;
                    const float sm = e + (sat - e) * La;
                    const float xo = (sm + kE * (1.0f - aE) * dLp[c]) * dE;
                    dLp[c] += aE * (xo - dLp[c]);
                    tapeBuf[c][tw] = xo;
                    // playback: the head (the right one a little off), hiss, crinkles, low cut, head bump, high cut
                    const float rd = hermite (tapeBuf[c], (float) tw - D - (c ? az : 0.0f));
                    const float nz = rng.bi();
                    hsLp[c] += aH * (nz - hsLp[c]);
                    float y = rd + hiss * (nz - hsLp[c]);
                    if (crk[c] > 1e-6f) { crk[c] *= crkDec; y += crk[c] * rng.bi(); }
                    float lp, bp;
                    svf (hpS[c], y, kHp, lp, bp); y = y - kHp.k * bp - lp;
                    svf (bmS[c], y, kBm, lp, bp); y += bmG * bp;
                    svf (lpS[c], y, kLp, lp, bp); y = lp * amp;
                    out2[c] = rd + (y - rd) * tA;
                }
                tw = (tw + 1) & TMASK;
                d0 = D;
                zL += (out2[0] - zL) * anaMix; zR += (out2[1] - zR) * anaMix;
            }
            else
            {
                // the codec's turn: the tape still records (plainly), its emphasis filters follow, ready to take over
                tapeBuf[0][tw] = yl; tapeBuf[1][tw] = yr; tw = (tw + 1) & TMASK;
                eLp[0] += aE * (yl - eLp[0]); eLp[1] += aE * (yr - eLp[1]); dLp[0] = eLp[0]; dLp[1] = eLp[1];
            }
            const float cL = dL + (zL - dL) * mix, cR = dR + (zR - dR) * mix;
            outL += (cL - outL) * lossMix; outR += (cR - outR) * lossMix;
        }
        // ON / OFF
        onMix += onT > onMix ? onStep : (onT < onMix ? -onStep : 0.0f);
        onMix = clampf (onMix, 0.0f, 1.0f);
        X[0][i] = xl + (outL - xl) * onMix; X[1][i] = xr + (outR - xr) * onMix;
    }
    curDetail = evType == GL_REPEAT ? (float) evR : (evType == GL_BEND ? (float) evRate : (evType == GL_JUMP ? (float) evBack : 0.0f));
    for (int c2 = 0; c2 < 2; ++c2) for (int b = 0; b < 24; ++b) if (isBad (bandG[c2][b])) bandG[c2][b] = 1.0f;
    // the tape's speed error over this chunk (a delay that grows plays slower)
    if (anaMix > 0.0f && n > 0) { const float r = 1.0f - (d0 - dPrev) / (float) n; lossCents = 1200.0f * log2f_ (maxf (r, 0.25f)); }
    else lossCents = 0.0f;
    dPrev = d0;
    for (int c2 = 0; c2 < 2; ++c2)
        if (isBad (eLp[c2]) || isBad (dLp[c2]) || isBad (hpS[c2][0]) || isBad (hpS[c2][1]) || isBad (bmS[c2][0]) || isBad (bmS[c2][1])
            || isBad (lpS[c2][0]) || isBad (lpS[c2][1])) tapeReset();
}

} // namespace sk

