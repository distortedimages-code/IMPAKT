// IMPAKT — master section (see sk_master.h).
#include "sk_master.h"
#ifndef SK_TAPE_RK4
 #define SK_TAPE_RK4 1
#endif

namespace sk {

namespace {

inline float lnf_ (float x)            { return log2f_ (x) * LN2; }
inline float dbToGain (float db)       { return exp2f_ (db * 0.16609640f); }
inline float gainToDb (float g)        { return 6.0205999f * log2f_ (g); }
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }
inline float sech2 (float x)           { const float t = tanhf_ (x); return 1.0f - t * t; }
inline float softplus (float x, float k) { const float z = k * x; return z > 20.0f ? x : lnf_ (1.0f + expf_ (z)) / k; }
inline float sigmoid (float x)         { return 1.0f / (1.0f + expf_ (-x)); }
inline float coef (float sec, float fs) { return 1.0f - expf_ (-1.0f / (maxf (sec, 1e-6f) * fs)); }

// output safety stage: exactly linear up to 0.63 (-4 dBFS), then a tanh knee towards 0.9, so the
// ringing of the decimation filter after hard limiting stays below full scale
inline float safeClip (float x)
{
    const float t = 0.63f, c = 0.9f, a = absf (x);
    if (a <= t) return x;
    const float y = t + (c - t) * tanhf_ ((a - t) * (1.0f / (c - t)));
    return x < 0.0f ? -y : y;
}

// pot tapers: knob position -> wiper ratio, fitted so that boost / cut grow in even dB steps
const float TAPER_LB[17] = { 0.00000f, 0.01332f, 0.02849f, 0.04585f, 0.06581f, 0.08882f, 0.11566f, 0.14672f, 0.18300f, 0.22601f, 0.27713f, 0.33879f, 0.41432f, 0.50823f, 0.62785f, 0.78494f, 1.00000f };
const float TAPER_LC[17] = { 0.00000f, 0.01672f, 0.03596f, 0.05819f, 0.08383f, 0.11365f, 0.14785f, 0.18737f, 0.23307f, 0.28595f, 0.34726f, 0.41831f, 0.50100f, 0.59742f, 0.71027f, 0.84291f, 1.00000f };
const float TAPER_HB[17] = { 0.00000f, 0.19353f, 0.30291f, 0.39474f, 0.47528f, 0.54701f, 0.61142f, 0.66950f, 0.72207f, 0.76956f, 0.81269f, 0.85180f, 0.88733f, 0.91974f, 0.94891f, 0.97563f, 1.00000f };
const float TAPER_HC[17] = { 0.00000f, 0.21542f, 0.34580f, 0.45258f, 0.54220f, 0.61810f, 0.68277f, 0.73820f, 0.78575f, 0.82683f, 0.86251f, 0.89333f, 0.92049f, 0.94396f, 0.96474f, 0.98327f, 1.00000f };

float taper (const float* t, float k)
{
    k = clampf (k, 0.0f, 1.0f) * 16.0f;
    const int i = mini ((int) k, 15);
    return t[i] + (t[i + 1] - t[i]) * (k - (float) i);
}

// EQP-1A component values
const double LF_C2[4]  = { 2.2e-6, 1.1e-6, 560e-9, 330e-9 };          // 20, 30, 60, 100 Hz
const double HF_F[7]   = { 3000.0, 4000.0, 5000.0, 8000.0, 10000.0, 12000.0, 16000.0 };
const double HS_C[3]   = { 270e-9, 135e-9, 68e-9 };                   // 5, 10, 20 kHz
const double L1_C1     = 12e6;                                        // L1 / C1 of the boost resonator
const double EQ_MAKEUP = 15.5;                                        // insertion loss of the network

inline double par (double a, double b) { return a * b / (a + b); }

// smooth asymmetric saturator with unit slope at 0: tanh around a bias point
inline float biasedTanh (float x, float b, float s2)   // s2 = sech^2(b)
{
    return (tanhf_ (b + x) - tanhf_ (b)) / s2;
}

} // namespace

// --------------------------------------------------------------------------- filters
void Biquad::peaking (double fs, double f, double q, double db)
{
    const float w = (float) (f / fs);
    const double A = (double) exp2f_ ((float) db * (0.16609640f * 0.5f));
    const double cw = (double) cos2pi (w), al = (double) sin2pi (w) / (2.0 * q);
    const double a0 = 1.0 + al / A;
    b0 = (1.0 + al * A) / a0; b1 = -2.0 * cw / a0; b2 = (1.0 - al * A) / a0;
    a1 = -2.0 * cw / a0; a2 = (1.0 - al / A) / a0;
}
void Biquad::lowpass (double fs, double f, double q)
{
    const float w = (float) (minf ((float) f, 0.49f * (float) fs) / fs);
    const double cw = (double) cos2pi (w), al = (double) sin2pi (w) / (2.0 * q);
    const double a0 = 1.0 + al;
    b0 = (1.0 - cw) * 0.5 / a0; b1 = (1.0 - cw) / a0; b2 = b0;
    a1 = -2.0 * cw / a0; a2 = (1.0 - al) / a0;
}
void Biquad::highpass (double fs, double f, double q)
{
    const float w = (float) (f / fs);
    const double cw = (double) cos2pi (w), al = (double) sin2pi (w) / (2.0 * q);
    const double a0 = 1.0 + al;
    b0 = (1.0 + cw) * 0.5 / a0; b1 = -(1.0 + cw) / a0; b2 = b0;
    a1 = -2.0 * cw / a0; a2 = (1.0 - al) / a0;
}

void Tilt::set (float fs, float db, int inverse)
{
    // first-order high shelf, unity at LF and G at HF, corner 1.2 kHz (pre-warped bilinear)
    const float G = dbToGain (db);
    const float wc = TWO_PI * 1200.0f;
    const float K = wc / tanpi (1200.0f / fs);                      // bilinear, pre-warped at the corner
    float n0 = G * K + wc, n1 = wc - G * K, d0 = K + wc, d1 = wc - K;
    if (inverse) { float t0 = n0, t1 = n1; n0 = d0; n1 = d1; d0 = t0; d1 = t1; }
    b0 = n0 / d0; b1 = n1 / d0; a1 = d1 / d0;
}

// --------------------------------------------------------------------------- EQP-1A (WDF)
void PultecEQ::init (double sampleRate)
{
    fs = sampleRate;
    zC1 = zL1 = zCHC = zCL1 = zCL2 = 0.0;
    for (int i = 0; i < 8; ++i) cur[i] = 0.0f;
    cur[3] = 0.5f;
    lfIdx = 2; hfIdx = 3; hsIdx = 1;
    update();
}

void PultecEQ::setKnobs (float lb, float la, float hb, float bw, float ha, int lf, int hf, int hs)
{
    const float tgt[5] = { lb, la, hb, bw, ha };
    int changed = lf != lfIdx || hf != hfIdx || hs != hsIdx;
    for (int i = 0; i < 5; ++i)
    {
        const float d = tgt[i] - cur[i];
        if (absf (d) > 1e-5f) { cur[i] += absf (d) < 1e-3f ? d : d * 0.2f; changed = 1; }
    }
    lfIdx = lf; hfIdx = hf; hsIdx = hs;
    if (changed) update();
}

void PultecEQ::update()
{
    const double lim = 1e-5;
    auto pot = [lim] (float r) { return (double) clampf (r, (float) lim, (float) (1.0 - lim)); };
    const double lb = pot (taper (TAPER_LB, cur[0]));
    const double la = pot (taper (TAPER_LC, cur[1]));
    const double hb = pot (taper (TAPER_HB, cur[2]));
    const double bw = pot (cur[3]);
    const double ha = pot (taper (TAPER_HC, cur[4]));

    // high boost resonator, centre frequency pre-warped for the bilinear transform
    const double f = HF_F[mini (maxi (hfIdx, 0), 6)];
    const double fw = fs * (double) tanpi ((float) (f / fs)) / (double) PI;
    const double C1 = 1.0 / (TWO_PI * fw * __builtin_sqrt (L1_C1));
    const double L1 = L1_C1 * C1;
    const double C2 = LF_C2[mini (maxi (lfIdx, 0), 3)];
    const double CHC = HS_C[mini (maxi (hsIdx, 0), 2)];

    rC1 = 1.0 / (2.0 * fs * C1); rL1 = 2.0 * fs * L1;
    rCHC = 1.0 / (2.0 * fs * CHC);
    rCL1 = 1.0 / (2.0 * fs * (C2 / 30.0)); rCL2 = 1.0 / (2.0 * fs * C2);
    rBH1 = (1.0 - hb) * 12e3; rBH2 = hb * 12e3;
    rQ = bw * 3.9e3;
    rCH1 = (1.0 - ha) * 1e3; rCH2 = ha * 1e3;
    rCL = la * 110e3;
    rBL = lb * 8.2e3;

    rSCL = rC1 + rL1;
    rCc = rSCL + rQ + 390.0;
    rpCD = par (rCc, rBH2);
    rS1 = rBH1 + rpCD;
    rG = rCHC + 91.0;
    rpFG = par (rCH2, rG);
    rsEFG = rCH1 + rpFG;
    rH = par (rCL1, rCL);
    rsHI = rH + 11e3;
    rpE = par (rsEFG, rsHI);
    rS2 = rS1 + rpE;
    rJ = par (rBL, rCL2);
    rS3 = rS2 + rJ;

    kCc = rpCD / rCc; kG = rpFG / rG; kH = rH / rCL1;
    kpE1 = rpE / rsEFG; kpE2 = rpE / rsHI; kJ = rJ / rCL2;
    kS2 = rS2 / rS3; kJ3 = rJ / rS3; kS1 = rS1 / rS2; kpE = rpE / rS2;
    kpCD = rpCD / rS1; kSCL = rSCL / rCc; kC1 = rC1 / rSCL; kL1 = rL1 / rSCL;
    kpFG = rpFG / rsEFG; kCHC = rCHC / rG; kHs = rH / rsHI; kI = 11e3 / rsHI;
}

__attribute__ ((always_inline)) inline double PultecEQ::tick (double x)
{
    // reflected waves, leaves to root
    const double bC1 = zC1, bL1 = -zL1, bCHC = zCHC, bCL1 = zCL1, bCL2 = zCL2;
    const double bSCL = -(bC1 + bL1);
    const double bCc = -bSCL;                                   // series with the Q resistors
    const double bpCD = bCc * kCc;                              // parallel with the boost pot (lower leg)
    const double bS1 = -bpCD;                                   // series with the boost pot (upper leg)
    const double bG = -bCHC;                                    // cut capacitor + 91 ohm
    const double bpFG = bG * kG;
    const double bsEFG = -bpFG;
    const double bH = bCL1 * kH;                                // low cut capacitor || low cut pot
    const double bsHI = -bH;                                    // + 1k + 10k
    const double bpE = bsEFG * kpE1 + bsHI * kpE2;
    const double bS2 = -(bS1 + bpE);
    const double bJ = bCL2 * kJ;                                // low boost pot || capacitor
    const double bS3 = -(bS2 + bJ);

    // ideal voltage source at the root, incident waves back to the leaves
    const double aS3 = 2.0 * x - bS3;
    double t = bS2 + bJ + aS3;
    const double aS2 = bS2 - kS2 * t;
    const double aJ  = bJ - kJ3 * t;
    zCL2 = bJ + aJ - bCL2;

    t = bS1 + bpE + aS2;
    const double aS1 = bS1 - kS1 * t;
    const double apE = bpE - kpE * t;

    t = bpCD + aS1;
    const double apCD = bpCD - kpCD * t;
    const double aCc = bpCD + apCD - bCc;
    t = bSCL + aCc;
    const double aSCL = bSCL - kSCL * t;
    t = bC1 + bL1 + aSCL;
    zC1 = bC1 - kC1 * t;
    zL1 = bL1 - kL1 * t;

    const double asEFG = bpE + apE - bsEFG;
    const double asHI = bpE + apE - bsHI;
    t = bpFG + asEFG;
    const double apFG = bpFG - kpFG * t;
    const double aG = bpFG + apFG - bG;
    t = bCHC + aG;
    zCHC = bCHC - kCHC * t;

    t = bH + asHI;
    const double aH = bH - kHs * t;
    const double aI = -kI * t;
    zCL1 = bH + aH - bCL1;
    const double aR3 = -(10e3 / 11e3) * aI;                   // series 1k + 10k: incident wave on the 10k
    const double vR3 = 0.5 * aR3, vJ = 0.5 * (aJ + bJ);
    return (vR3 - vJ) * EQ_MAKEUP;
}

// --------------------------------------------------------------------------- compressor
void Compressor::init (float sampleRate)
{
    fs = sampleRate;
    for (int c = 0; c < 2; ++c) { scHpX[c] = scHpY[c] = 0.0f; band[c].peaking (fs, 6000.0, 1.0, 9.0); band[c].reset(); dcX[c] = dcY[c] = lo[c] = 0.0f; }
    scHpA = 1.0f - TWO_PI * 100.0f / fs;
    dcR = 1.0f - TWO_PI * 8.0f / fs;
    env = mem = 0.0f; grPeak = 0.0f; bandOn = 0;
}

void Compressor::process (float* L, float* R, int n, int os, const MasterParams& p, float gIn0, float gIn1, float out0, float out1)
{
    const int ratio = idxOf (p.cRatio, NUM_COMP_RATIOS);
    if (ratio == CR_OFF) return;
    // ratio, knee width, threshold (dBFS inside the compressor)
    float slope = 0.0f, W = 1.0f, T = -10.0f;
    switch (ratio)
    {
        case CR_2:    slope = 0.5f;          W = 24.0f; T = -6.0f; break;
        case CR_3:    slope = 2.0f / 3.0f;   W = 18.0f; T = -6.0f; break;
        case CR_4:    slope = 0.75f;         W = 12.0f; T = -5.0f; break;
        case CR_6:    slope = 5.0f / 6.0f;   W = 9.0f;  T = -5.0f; break;
        case CR_OPTO: slope = 0.9f;          W = 6.0f;  T = -5.0f; break;
        case CR_20:   slope = 0.95f;         W = 3.0f;  T = -3.0f; break;
        case CR_NUKE: slope = 1.0f;          W = 1.0f;  T = -2.0f; break;
        case CR_BRIT: slope = 1.25f;         W = 2.0f;  T = -4.0f; break;
        default: break;                                  // 1:1: colour only
    }
    float tAtk = 0.00005f * powf_ (600.0f, p.cAtk);     // 50 us .. 30 ms
    const float tRel = 0.05f * powf_ (70.0f, p.cRel);    // 50 ms .. 3.5 s
    if (ratio == CR_OPTO) tAtk = maxf (tAtk, 0.001f);
    if (ratio == CR_BRIT) tAtk *= 0.5f;
    // the detector and gain computer run once per group of `os` samples (true-peak of the group),
    // the gain is interpolated inside the group
    const float grpRate = fs / (float) os;
    const float aA = coef (tAtk, grpRate), aR = coef (tRel, grpRate);
    const float memA = coef (0.4f, grpRate), memR = coef (3.0f, grpRate);
    const int dist = idxOf (p.cDist, 3);
    const int det = idxOf (p.cDet, 4);
    const int hp = det & 1;
    bandOn = (det & 2) != 0;
    const int d2 = dist == 1 || ratio == CR_BRIT, d3 = dist == 2 || ratio == CR_BRIT;
    const float lpA = coef (1.0f / (TWO_PI * 500.0f), fs);
    const float inv = 1.0f / (float) n, invOs = 1.0f / (float) os;
    const float mixT = p.cMix;
    const float d2amt = 0.10f * (1.0f + env * 0.12f);

    for (int g0 = 0; g0 < n; g0 += os)
    {
        float lev = 0.0f;
        for (int i = g0; i < g0 + os; ++i)
        {
            const float gin = lerpf (gIn0, gIn1, (float) i * inv);
            for (int c = 0; c < 2; ++c)
            {
                float sc = (c == 0 ? L : R)[i] * gin;
                if (hp) { const float y = sc - scHpX[c] + scHpA * scHpY[c]; scHpX[c] = sc; scHpY[c] = y; sc = y; }
                if (bandOn) sc = (float) band[c].tick ((double) sc);
                lev = maxf (lev, absf (sc));
            }
        }
        float target = 0.0f;
        if (slope > 0.0f)
        {
            const float over = gainToDb (lev + 1e-9f) - T;
            if (2.0f * over <= -W) target = 0.0f;
            else if (2.0f * absf (over) <= W) { const float k = over + 0.5f * W; target = slope * k * k / (2.0f * W); }
            else target = slope * over;
        }
        // smoothed branching detector in the log domain, programme-dependent release
        const float envPrev = env;
        if (target > env) env += (target - env) * aA;
        else
        {
            float r;
            if (ratio == CR_OPTO) r = aR / (1.0f + 5.0f * mem);
            else if (ratio == CR_NUKE) r = env > 6.0f ? aR * 3.0f : aR;
            else if (ratio == CR_BRIT) r = aR * 1.6f;
            else r = aR / (1.0f + 0.6f * mem);
            env += (target - env) * minf (r, 1.0f);
        }
        const float m = minf (env * (1.0f / 12.0f), 1.0f);
        mem += (m - mem) * (m > mem ? memA : memR);
        if (env > grPeak) grPeak = env;
        const float gA = dbToGain (-envPrev), gB = dbToGain (-env);

        for (int i = g0; i < g0 + os; ++i)
        {
            const float fr = (float) i * inv;
            const float gin = lerpf (gIn0, gIn1, fr), gout = lerpf (out0, out1, fr);
            const float gg = lerpf (gA, gB, (float) (i - g0 + 1) * invOs);
            for (int c = 0; c < 2; ++c)
            {
                float* io = c == 0 ? L : R;
                const float dry = io[i];
                float y = dry * gin * gg;
                // input / output amplifier colour (always on), then the Dist stages
                const float sc = softclip (y);
                y += 0.015f * sc * sc;
                if (d2) y += d2amt * sc * sc;
                if (d3)
                {
                    lo[c] += (y - lo[c]) * lpA;
                    const float l = lo[c] * 1.6f;
                    y += (l / sqrtf_ (1.0f + l * l * (1.0f / 3.0f)) - l) * (1.0f / 1.6f) * (1.0f + env * 0.08f);
                    y += 0.04f * sc * sc;
                }
                const float dc = y - dcX[c] + dcR * dcY[c]; dcX[c] = y; dcY[c] = dc;
                const float wet = dc / gin;
                io[i] = (dry + (wet - dry) * mixT) * gout;
            }
        }
    }
}

// --------------------------------------------------------------------------- drive
namespace {

const double JA_MS = 3.5e5, JA_A = 2.2e4, JA_ALPHA = 1.6e-3;
// calibration (tools: build/master/calib.cpp): every model gives about 1 % THD (-40 dB) for a
// -6 dBFS sine with DRIVE at 0 dB, and unity gain for small signals
float TAPE_NORM = 17.5586f;
float SENS[NUM_DRIVE_MODELS] = { 1.0f, 0.0755f, 0.1332f, 0.2741f, 0.628f, 0.2429f, 0.803f, 1.0f };
// measured level change (dB) of a -6 dBFS sine at DRIVE 0, 5 .. 30 dB
const float DRIVE_LEVEL[NUM_DRIVE_MODELS][7] = {
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0.38f, 0.70f, 1.29f, 2.30f, 3.98f, 6.13f, 7.32f },             // TAPE
    { -0.00f, -0.01f, -0.04f, -0.18f, -0.96f, -4.84f, -15.18f },     // PREAMP
    { -0.00f, -0.01f, -0.04f, -0.13f, -0.43f, -3.79f, -8.79f },      // TRIODE
    { -0.06f, -0.27f, -1.59f, -5.04f, -9.56f, -14.43f, -19.39f },    // PENTODE
    { -0.01f, -0.02f, -0.07f, -0.22f, -1.09f, -4.38f, -8.94f },      // TRANSISTOR
    { -0.10f, -0.22f, -1.30f, -4.10f, -7.99f, -12.30f, -16.78f },    // DIODE
    { -5.49f, -7.84f, -10.42f, -13.90f, -18.25f, -22.99f, -27.86f }, // BENT
};

// Jiles-Atherton dM/dt (Chowdhury 2019, eq. 18). The constants that depend on k and c are worked out once
// per sample (JaK) and the divisions are merged: 2 per call instead of 5 (same model, same double precision)
struct JaK { double omcK, omc, cK, cAK; };
inline JaK jaConsts (double k, double c)
{
    return { (1.0 - c) * k, 1.0 - c, c * (JA_MS / JA_A), c * JA_ALPHA * (JA_MS / JA_A) };
}
inline double jaDeriv (double H, double Hd, double M, const JaK& q)
{
    const double Q = (H + JA_ALPHA * M) * (1.0 / JA_A);
    double L, Ld;
    if (Q > -0.5 && Q < 0.5)
    {
        const double q2 = Q * Q;
        L = Q * (1.0 / 3.0 - q2 * (1.0 / 45.0 - q2 * (2.0 / 945.0 - q2 * (1.0 / 4725.0))));
        Ld = 1.0 / 3.0 - q2 * (1.0 / 15.0 - q2 * (2.0 / 189.0 - q2 * (1.0 / 675.0)));
    }
    else
    {
        // coth Q and 1/Q from a single division
        const double t = (double) tanhf_ ((float) Q), r = 1.0 / (t * Q);
        const double ct = Q * r, iq = t * r;
        L = ct - iq; Ld = iq * iq - ct * ct + 1.0;
    }
    const double diff = JA_MS * L - M;
    const double den = 1.0 - q.cAK * Ld;
    // the irreversible term only counts while M moves toward the anhysteretic curve (dM = 1)
    if ((Hd >= 0.0) == (diff > 0.0) && diff != 0.0)
    {
        double den1 = (Hd >= 0.0 ? q.omcK : -q.omcK) - JA_ALPHA * diff;
        if (den1 > -1e-6 && den1 < 1e-6) den1 = den1 < 0.0 ? -1e-6 : 1e-6;
        return Hd * (q.omc * diff + q.cK * Ld * den1) / (den1 * den);
    }
    return q.cK * Hd * Ld / den;
}

// Koren 12AX7 plate current (A)
float korenIp (float vg, float vp)
{
    const float mu = 100.0f, ex = 1.4f, kg1 = 1060.0f, kp = 600.0f, kvb = 300.0f;
    const float e1 = (vp / kp) * kp * softplus (1.0f / mu + vg / sqrtf_ (kvb + vp * vp), kp);   // (Vp/kp) ln(1 + e^(kp (...)))
    if (e1 <= 0.0f) return 0.0f;
    return 2.0f * powf_ (e1, ex) / kg1;
}

} // namespace

void driveCalibrate (float tapeNorm, const float* sens) { TAPE_NORM = tapeNorm; if (sens) for (int i = 0; i < NUM_DRIVE_MODELS; ++i) SENS[i] = sens[i]; }

void Drive::init (float sampleRate)
{
    fs = sampleRate;
    for (int c = 0; c < 2; ++c)
    {
        DriveChannel& d = ch[c];
        d.pre.set (fs, 0.0f, 0); d.post.set (fs, 0.0f, 1); d.pre.reset(); d.post.reset();
        d.rpre.set (fs, 6.0f, 0); d.rpost.set (fs, 6.0f, 1); d.rpre.reset(); d.rpost.reset();
        d.dcX = d.dcY = 0.0f;
        d.M = d.Hp = d.Hdp = 0.0;
        d.bump.setIdentity(); d.loss.setIdentity(); d.bump.reset(); d.loss.reset();
        d.phi[0] = d.phi[1] = d.ePrev[0] = d.ePrev[1] = 0.0f;
        d.env = d.sag = 0.0f; d.vbe = 0.0f; d.vd = d.xd = 0.0f; d.xp = d.xp2 = 0.0f;
        d.rng.seed (0xD12u + (u32) c * 77u);
    }
    // 12AX7, 250 V supply, 100k plate load: plate voltage vs grid voltage (bisection)
    tLo = -8.0f; tHi = 2.0f; tStep = (tHi - tLo) / (float) (TRIODE_TAB - 1);
    for (int i = 0; i < TRIODE_TAB; ++i)
    {
        const float vg = tLo + tStep * (float) i;
        float lo = 0.0f, hi = 250.0f;
        for (int it = 0; it < 40; ++it)
        {
            const float vp = 0.5f * (lo + hi);
            const float f = vp - 250.0f + 100e3f * korenIp (vg, vp);
            if (f > 0.0f) hi = vp; else lo = vp;
        }
        triode[i] = 0.5f * (lo + hi);
    }
    model = lastModel = -1; lastChar = lastBias = lastTone = lastShape = -1.0f; norm = 1.0f; level = 1.0f;
}

float Drive::triodeAt (float vg) const
{
    // Catmull-Rom interpolation (smooth first derivative)
    float x = (vg - tLo) / tStep;
    x = clampf (x, 1.0f, (float) (TRIODE_TAB - 3));
    const int i = (int) x; const float t = x - (float) i;
    const float p0 = triode[i - 1], p1 = triode[i], p2 = triode[i + 1], p3 = triode[i + 2];
    return p1 + 0.5f * t * (p2 - p0 + t * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3 + t * (3.0f * (p1 - p2) + p3 - p0)));
}

void Drive::configure (const MasterParams& p)
{
    model = idxOf (p.dModel, NUM_DRIVE_MODELS);
    // output level: half of the measured level change is compensated, so DRIVE mostly changes character
    {
        const float x = clampf (p.dDrive, 0.0f, 1.0f) * 6.0f;
        const int i = mini ((int) x, 5);
        const float lv = DRIVE_LEVEL[model][i] + (DRIVE_LEVEL[model][i + 1] - DRIVE_LEVEL[model][i]) * (x - (float) i);
        level = dbToGain (-0.5f * lv);
    }
    if (model == lastModel && absf (p.dChar - lastChar) < 1e-4f && absf (p.dBias - lastBias) < 1e-4f && absf (p.dTone - lastTone) < 1e-4f) return;
    const float tdb = (p.dTone - 0.5f) * 18.0f;
    for (int c = 0; c < 2; ++c) { ch[c].pre.set (fs, tdb, 0); ch[c].post.set (fs, tdb, 1); }
    if (model == DM_TAPE)
    {
        // tape speed 3.75 .. 30 ips: head bump and gap / spacing loss move with it
        const float ips = 3.75f * exp2f_ (p.dChar * 3.0f);
        for (int c = 0; c < 2; ++c)
        {
            ch[c].bump.peaking (fs, 22.0 + 2.6 * ips, 1.1, 2.0);
            ch[c].loss.lowpass (fs, minf (1300.0f * ips, 0.45f * fs), 0.6);
        }
    }
    // static curves are rebuilt when their shape knob moved enough (they cost ~4k function evaluations)
    const float shape = model == DM_TRANSISTOR ? p.dBias * 7.0f + p.dChar : p.dBias;
    const int rebuild = model != lastModel || absf (shape - lastShape) > 0.004f;
    if (rebuild && model == DM_TRIODE)
    {
        const float vb = -0.6f - 1.6f * p.dBias;
        norm = maxf ((triodeAt (vb - 0.01f) - triodeAt (vb + 0.01f)) / 0.02f, 1e-3f);
        const float q = triodeAt (vb), nn = norm;
        // input relative to the bias point; positive grid swings are limited by grid conduction
        shaper.build (-6.5, 3.5, [this, vb, q, nn] (float x) {
            float vg = vb + x;
            if (vg > 0.0f) vg = vg / (1.0f + 3.0f * vg);
            return (q - triodeAt (vg)) / nn;
        });
        lastShape = shape;
    }
    if (rebuild && model == DM_PENTODE)
    {
        // push-pull pair with softplus cut-off (class A .. B) and a plate / transformer knee
        const float vb = -0.15f - 0.85f * p.dBias, kk = 6.0f;
        auto ip = [kk] (float v) { const float sp = softplus (v + 1.0f, kk); return sp * sqrtf_ (sp); };
        auto ipd = [kk] (float v) { const float sp = softplus (v + 1.0f, kk); return 1.5f * sqrtf_ (sp) * sigmoid (kk * (v + 1.0f)); };
        const float sl = maxf (2.0f * 0.5f * ipd (vb), 1e-3f);
        shaper.build (-14.0, 14.0, [ip, vb, sl] (float u) {
            const float s = u * 0.5f;
            const float z = (ip (vb + s) - ip (vb - s)) / sl / 1.4f;
            return 1.4f * z / sqrtf_ (sqrtf_ (1.0f + z * z * z * z));
        });
        lastShape = shape;
    }
    if (rebuild && model == DM_TRANSISTOR)
    {
        // ln(ic) + D ic = ln(icq) + D icq + v (Ebers-Moll with emitter degeneration), collector
        // saturation at 2.2 x the quiescent current; solved by Newton along the table (warm start)
        const float D = 0.2f * powf_ (100.0f, p.dChar);
        const float icq = 0.4f + 1.2f * p.dBias;
        const float sat0 = icq - softplus (icq - 2.2f, 6.0f);
        float w = lnf_ (icq);
        // the table is built from the middle outwards so each Newton solve starts close to its answer
        double tmp[ADAA_N];
        const double lo = -8.0, hi = 8.0, st = (hi - lo) / (double) (ADAA_N - 1);
        auto solve = [D, icq] (float v, float& ww) {
            const float rhs = lnf_ (icq) + D * icq + v * (1.0f / icq + D);
            for (int it = 0; it < 8; ++it)
            {
                const float ew = expf_ (ww);
                const float dw = (ww + D * ew - rhs) / (1.0f + D * ew);
                ww = clampf (ww - dw, -40.0f, 4.0f);
                if (absf (dw) < 1e-7f) break;
            }
            const float ic = expf_ (ww);
            return ic - softplus (ic - 2.2f, 6.0f);
        };
        const int mid = (int) ((0.0 - lo) / st);
        for (int i = mid; i < ADAA_N; ++i) tmp[i] = (double) (solve ((float) (lo + st * i), w) - sat0);
        w = lnf_ (icq);
        for (int i = mid - 1; i >= 0; --i) tmp[i] = (double) (solve ((float) (lo + st * i), w) - sat0);
        shaper.build (lo, hi, [&tmp, lo, st] (float v) {
            double t = ((double) v - lo) / st;
            t = t < 0.0 ? 0.0 : (t > (double) (ADAA_N - 1) ? (double) (ADAA_N - 1) : t);
            const int i = mini ((int) t, ADAA_N - 2);
            return (float) (tmp[i] + (tmp[i + 1] - tmp[i]) * (t - (double) i));
        });
        lastShape = shape;
    }
    if (model != lastModel)
        for (int c = 0; c < 2; ++c)
        {
            DriveChannel& d = ch[c];
            d.M = d.Hp = d.Hdp = 0.0; d.env = d.sag = 0.0f; d.vd = d.xd = 0.0f; d.vbe = 0.0f; d.xp = d.xp2 = 0.0f;
            d.bump.reset(); d.loss.reset();
        }
    lastModel = model; lastChar = p.dChar; lastBias = p.dBias; lastTone = p.dTone;
}

namespace {
// ln cosh(x), stable for large |x|
inline float lncosh (float x) { const float a = absf (x); return a + lnf_ (1.0f + expf_ (-2.0f * a)) - LN2; }
}

__attribute__ ((always_inline)) inline float Drive::model1 (DriveChannel& d, float u, const MasterParams& p)
{
    const float T = 1.0f / fs;
    switch (model)
    {
    case DM_TAPE:
    {
        // record emphasis -> Jiles-Atherton hysteresis (RK4) -> playback de-emphasis, head bump, gap loss
        const double k = 2.7e4 * (0.45 + 1.1 * (1.0 - (double) p.dBias));
        const double c = 0.17 * (0.55 + 0.9 * (double) p.dBias);
        const double H = (double) d.rpre.tick (u) * 2.2e4;
        const double Hd = 1.9 * (H - d.Hp) * (double) fs - 0.9 * d.Hdp;
        const double dt = (double) T;
#if SK_TAPE_RK4
        const double Hm = 0.5 * (H + d.Hp), Hdm = 0.5 * (Hd + d.Hdp);
        const JaK q = jaConsts (k, c);
        const double k1 = dt * jaDeriv (d.Hp, d.Hdp, d.M, q);
        const double k2 = dt * jaDeriv (Hm, Hdm, d.M + 0.5 * k1, q);
        const double k3 = dt * jaDeriv (Hm, Hdm, d.M + 0.5 * k2, q);
        const double k4 = dt * jaDeriv (H, Hd, d.M + k3, q);
        double M = d.M + (k1 + 2.0 * k2 + 2.0 * k3 + k4) / 6.0;
#else
        // Heun (RK2): at 4x oversampling it tracks RK4 within ~0.1 dB at half the cost
        const JaK q = jaConsts (k, c);
        const double k1 = dt * jaDeriv (d.Hp, d.Hdp, d.M, q);
        const double k2 = dt * jaDeriv (H, Hd, d.M + k1, q);
        double M = d.M + 0.5 * (k1 + k2);
#endif
        if (__builtin_isnan (M)) M = 0.0;
        M = M > JA_MS ? JA_MS : (M < -JA_MS ? -JA_MS : M);
        d.M = M; d.Hp = H; d.Hdp = Hd;
        float y = (float) (M / JA_MS) * TAPE_NORM;
        y = d.rpost.tick (y);
        y = (float) d.loss.tick (d.bump.tick ((double) y));
        return y;
    }
    case DM_PREAMP:
    {
        // input transformer (flux saturates at low frequencies), class-A stage (ADAA), output transformer
        const float w = TWO_PI * 25.0f * T, rho = 1.0f - TWO_PI * 2.0f * T;
        const float cap = 0.45f + 2.2f * (1.0f - p.dChar);
        float x = u;
        for (int s = 0; s < 2; ++s)
        {
            const float cs = s == 0 ? cap : cap * 1.8f;
            d.phi[s] = rho * d.phi[s] + w * x;
            const float e = cs * tanhf_ (d.phi[s] / cs) - d.phi[s];
            const float y = x + (e - d.ePrev[s]) / w;
            d.ePrev[s] = e;
            if (s == 0)
            {
                // (tanh(b + v) - tanh b) / sech^2 b, antiderivative (ln cosh(b + v) - v tanh b) / sech^2 b
                const float b = 0.12f + 0.55f * p.dBias, s2 = sech2 (b), tb = tanhf_ (b);
                const float v = y * 0.8f, dv = v - d.xp;
                float z;
                if (absf (dv) > 1e-4f) z = ((lncosh (b + v) - lncosh (b + d.xp)) - tb * dv) / (dv * s2);
                else z = biasedTanh (0.5f * (v + d.xp), b, s2);
                d.xp = v;
                x = z * 1.25f;
            }
            else x = y;
        }
        return x;
    }
    case DM_TRIODE:
    {
        // blocking: positive grid swings charge the coupling capacitor and push the bias down
        const float vb = -0.6f - 1.6f * p.dBias;
        const float pos = vb + u;
        if (pos > 0.0f) d.env += (pos - d.env) * coef (0.0015f, fs);
        else d.env *= 1.0f - coef (0.12f, fs);
        const float off = p.dChar * 2.2f * d.env;
        float prev = d.xp - off;                       // previous input seen with the current bias
        const float y = shaper.process (u - off, prev);
        d.xp = u;
        return y;
    }
    case DM_PENTODE:
    {
        // supply sag: the stage reaches its knee earlier after loud passages
        const float a = 1.0f + 1.6f * p.dChar * d.sag;
        float prev = d.xp * a;
        const float y = shaper.process (u * a, prev) / a;
        d.xp = u;
        const float m = absf (y);
        d.sag += (m - d.sag) * (m > d.sag ? coef (0.01f, fs) : coef (0.2f, fs));
        return y;
    }
    case DM_TRANSISTOR:
    {
        float prev = d.xp;
        const float y = shaper.process (u, prev);
        d.xp = u;
        return y;
    }
    case DM_DIODE:
    {
        // RC + diode clipper ODE, trapezoidal rule; BIAS adds diodes on one side.
        // g(v) = v - c0 - T/2 f(x, v) increases strictly with v, so Newton is kept inside a shrinking
        // bracket (bisection whenever a step leaves it): it always converges, whatever the drive, and the
        // capacitor voltage can never get pinned outside the physical range.
        const float fc = 2000.0f * powf_ (12.0f, p.dChar);
        const float wc = TWO_PI * fc;
        const float isc = 2.52e-9f * 2200.0f * wc;              // Is / C
        const float V1 = 0.0453f, V2 = 0.0453f * (1.0f + 1.2f * p.dBias);
        const float x = u * 0.55f;
        const float VMAX = 2.5f;                                  // above any reachable diode voltage
        auto ex = [] (float z) { return expf_ (clampf (z, -80.0f, 80.0f)); };
        const float fprev = wc * (d.xd - d.vd) - isc * (ex (d.vd / V1) - ex (-d.vd / V2));
        const float c0 = d.vd + 0.5f * T * fprev;
        float lo = -VMAX, hi = VMAX;
        // start from an explicit Euler guess, kept near the previous solution
        float v = clampf (clampf (d.vd + T * fprev, d.vd - 0.05f, d.vd + 0.05f), lo, hi);
        for (int it = 0; it < 40; ++it)
        {
            const float e1 = ex (v / V1), e2 = ex (-v / V2);
            const float G = v - c0 - 0.5f * T * (wc * (x - v) - isc * (e1 - e2));
            const float Gd = 1.0f + 0.5f * T * (wc + isc * (e1 / V1 + e2 / V2));
            const float dv = clampf (G / Gd, -0.05f, 0.05f);         // damped: the exponentials are stiff
            if (absf (dv) < 2e-6f) { v -= dv; break; }                // converged
            if (G > 0.0f) hi = v; else lo = v;
            float vn = v - dv;
            if (! (vn >= lo && vn <= hi)) vn = 0.5f * (lo + hi);    // left the bracket (or NaN): bisect
            v = vn;
            if (hi - lo < 4e-6f) break;
        }
        if (isBad (v)) v = 0.0f;
        d.vd = v; d.xd = x;
        return v / 0.55f;
    }
    case DM_BENT:
    {
        // starved germanium: the supply sags with the envelope, the bias drifts, quiet parts sputter
        const float a = absf (u);
        d.env += (a - d.env) * (a > d.env ? coef (0.002f, fs) : coef (0.12f, fs));
        const float vs = 1.0f - 0.82f * p.dChar;
        const float q = (p.dBias - 0.5f) * 0.6f;
        const float off = q - p.dChar * 1.1f * d.env;
        const float n = 0.2f * vs;
        auto curve = [vs, n] (float x) { return x >= 0.0f ? vs * tanhf_ (x / vs) : -n * (1.0f - expf_ (x / n)); };
        auto anti = [vs, n] (float x) { return x >= 0.0f ? vs * vs * lncosh (x / vs) : -n * x + n * n * (expf_ (x / n) - 1.0f); };
        const float s = u + off, sp = d.xp + off, ds = s - sp;
        float y = absf (ds) > 1e-4f ? (anti (s) - anti (sp)) / ds : curve (0.5f * (s + sp));
        d.xp = u;
        y -= curve (q);
        // gating with a little crackle around the threshold
        const float thr = 0.02f + 0.25f * p.dChar;
        float gate = clampf ((d.env - thr * 0.5f) / (thr * 0.5f + 1e-6f), 0.0f, 1.0f);
        if (gate > 0.0f && gate < 1.0f && d.rng.uni() > gate) gate = 0.0f;
        d.sag += (gate - d.sag) * coef (0.001f, fs);
        y *= d.sag;
        return y / maxf (vs, 0.2f);
    }
    default: return u;
    }
}

void Drive::process (float* x, int n, int c, const MasterParams& p, float g0, float g1, float mix0, float mix1, float out0, float out1)
{
    if (model <= DM_OFF) return;
    DriveChannel& d = ch[c];
    const float dcR = 1.0f - TWO_PI * 6.0f / fs;
    const float inv = 1.0f / (float) n;
    const float sens = SENS[model];
    for (int i = 0; i < n; ++i)
    {
        const float fr = (float) i * inv;
        const float g = lerpf (g0, g1, fr);
        const float dry = x[i];
        const float u = d.pre.tick (dry) * g;
        float y = model1 (d, u * sens, p) / sens;
        if (isBad (y)) { y = 0.0f; d.M = d.Hp = d.Hdp = 0.0; d.vd = d.xd = d.vbe = d.xp = 0.0f; d.env = d.sag = 0.0f; }
        y = d.post.tick (y);
        const float o = y - d.dcX + dcR * d.dcY; d.dcX = y; d.dcY = o;
        // level: drive raises saturation, not loudness (small signals keep unity gain)
        const float wet = o / g;
        x[i] = (dry + (wet - dry) * lerpf (mix0, mix1, fr)) * lerpf (out0, out1, fr);
    }
}

void Drive::processStereo (float* xl, float* xr, int n, const MasterParams& p, float g0, float g1, float mix0, float mix1, float out0, float out1)
{
    if (model <= DM_OFF) return;
    const float dcR = 1.0f - TWO_PI * 6.0f / fs;
    const float inv = 1.0f / (float) n;
    const float sens = SENS[model];
    float* io[2] = { xl, xr };
    for (int i = 0; i < n; ++i)
    {
        const float fr = (float) i * inv;
        const float g = lerpf (g0, g1, fr), mx = lerpf (mix0, mix1, fr), ot = lerpf (out0, out1, fr);
        for (int c = 0; c < 2; ++c)
        {
            DriveChannel& d = ch[c];
            float* x = io[c];
            const float dry = x[i];
            const float u = d.pre.tick (dry) * g;
            float y = model1 (d, u * sens, p) / sens;
            if (isBad (y)) { y = 0.0f; d.M = d.Hp = d.Hdp = 0.0; d.vd = d.xd = d.vbe = d.xp = 0.0f; d.env = d.sag = 0.0f; }
            y = d.post.tick (y);
            const float o = y - d.dcX + dcR * d.dcY; d.dcX = y; d.dcY = o;
            const float wet = o / g;
            x[i] = (dry + (wet - dry) * mx) * ot;
        }
    }
}

// --------------------------------------------------------------------------- master
void Master::init (float sampleRate, int lite)
{
    sr = sampleRate;
    os = lite ? 1 : MASTER_OS;
    fs = sr * (float) os;
    osL.reset(); osR.reset(); osL2.reset(); osR2.reset();
    eqL.init ((double) fs); eqR.init ((double) fs);
    tubeX[0] = tubeX[1] = tubeY[0] = tubeY[1] = 0.0f;
    eqOn = 0;
    comp.init (fs);
    drive.init (fs);
    for (int i = 0; i < 8; ++i) sm[i] = -1.0f;
    outPeak = inPeak = 0.0f;
}

void Master::process (float* L, float* R, int n, const MasterParams& p, int stage)
{
    const int N = n * os;
    if (stage == 2)
    {
        if (os == MASTER_OS) for (int i = 0; i < n; ++i) { osL2.up (L[i], bufL + i * MASTER_OS); osR2.up (R[i], bufR + i * MASTER_OS); }
        else for (int i = 0; i < n; ++i) { bufL[i] = L[i]; bufR[i] = R[i]; }
        finish (L, R, n, smFrom, 2);
        return;
    }
    drive.configure (p);
    // gains (linear), interpolated across the chunk
    const float tgt[8] = {
        dbToGain (p.cIn * 30.0f),                    // 0 compressor input (output is compensated)
        dbToGain ((p.cOut - 0.5f) * 24.0f),          // 1 compressor output
        dbToGain (p.dDrive * 30.0f),                 // 2 drive
        p.dMix,                                      // 3 drive mix
        dbToGain ((p.dOut - 0.5f) * 24.0f) * drive.level,   // 4 drive output (with level compensation)
        p.clip,                                      // 5 final clipper drive
        p.vol * p.vol * 1.5f,                        // 6 master volume
        0.0f };
    float from[8];
    for (int i = 0; i < 8; ++i) { if (sm[i] < 0.0f) sm[i] = tgt[i]; from[i] = sm[i]; sm[i] += (tgt[i] - sm[i]) * 0.25f; smFrom[i] = from[i]; }

    for (int i = 0; i < n; ++i) inPeak = maxf (inPeak, maxf (absf (L[i]), absf (R[i])));

    // up
    if (os == MASTER_OS) for (int i = 0; i < n; ++i) { osL.up (L[i], bufL + i * MASTER_OS); osR.up (R[i], bufR + i * MASTER_OS); }
    else for (int i = 0; i < n; ++i) { bufL[i] = L[i]; bufR[i] = R[i]; }

    // EQP-1A + tube make-up
    const int on = p.eqOn && (p.eqLb > 0.002f || p.eqLa > 0.002f || p.eqHb > 0.002f || p.eqHa > 0.002f);
    if (on)
    {
        const int lf = idxOf (p.eqLf, 4), hf = idxOf (p.eqHf, 7), hs = idxOf (p.eqHs, 3);
        if (! eqOn) { eqL.init ((double) fs); eqR.init ((double) fs); for (int c = 0; c < 2; ++c) tubeX[c] = tubeY[c] = 0.0f; }
        eqL.setKnobs (p.eqLb, p.eqLa, p.eqHb, p.eqBw, p.eqHa, lf, hf, hs);
        eqR.setKnobs (p.eqLb, p.eqLa, p.eqHb, p.eqBw, p.eqHa, lf, hf, hs);
        const float b = 0.2f, s2 = sech2 (b), dr = 0.3f, dcR = 1.0f - TWO_PI * 5.0f / fs;
        // both channels per sample: two independent chains for the CPU to overlap (same arithmetic)
        for (int i = 0; i < N; ++i)
            for (int c = 0; c < 2; ++c)
            {
                float* x = c == 0 ? bufL : bufR;
                PultecEQ& e = c == 0 ? eqL : eqR;
                float y = (float) e.tick ((double) x[i]);
                y = biasedTanh (y * dr, b, s2) / dr;              // 12AX7 / 12AU7 make-up, very gentle
                const float o = y - tubeX[c] + dcR * tubeY[c]; tubeX[c] = y; tubeY[c] = o;
                x[i] = o;
            }
    }
    eqOn = on;

    // drive before or after the compressor
    const int pre = idxOf (p.dPos, 2) == 0;
    if (pre && p.drvOn)
    {
        drive.processStereo (bufL, bufR, N, p, from[2], sm[2], from[3], sm[3], from[4], sm[4]);
    }
    if (p.compOn) comp.process (bufL, bufR, N, os, p, from[0], sm[0], from[1], sm[1]);
    if (! pre && p.drvOn)
    {
        drive.processStereo (bufL, bufR, N, p, from[2], sm[2], from[3], sm[3], from[4], sm[4]);
    }
    if (stage == 1)
    {
        // back to the base rate for GRAIN and OTT (no clipping here: they may need the headroom)
        if (os == MASTER_OS) for (int i = 0; i < n; ++i) { L[i] = osL.down (bufL + i * MASTER_OS); R[i] = osR.down (bufR + i * MASTER_OS); }
        else for (int i = 0; i < n; ++i) { L[i] = bufL[i]; R[i] = bufR[i]; }
        return;
    }
    finish (L, R, n, from, 0);
}

// final clipper and volume (oversampled, so the clipping does not alias), then down to the base rate
void Master::finish (float* L, float* R, int n, const float* from, int stage)
{
    const int N = n * os;
    const float inv = 1.0f / (float) N;
    for (int i = 0; i < N; ++i)
    {
        const float fr = (float) i * inv;
        const float cl = lerpf (from[5], sm[5], fr), vol = lerpf (from[6], sm[6], fr);
        float l = bufL[i], r = bufR[i];
        if (cl > 0.001f) { const float dg = 1.0f + cl * cl * 12.0f; l = softclip (l * dg) / (1.0f + cl); r = softclip (r * dg) / (1.0f + cl); }
        bufL[i] = safeClip (l * vol); bufR[i] = safeClip (r * vol);
    }

    // down
    Oversampler4& dl = stage == 2 ? osL2 : osL;
    Oversampler4& dr = stage == 2 ? osR2 : osR;
    if (os == MASTER_OS) for (int i = 0; i < n; ++i) { L[i] = clampf (dl.down (bufL + i * MASTER_OS), -1.0f, 1.0f); R[i] = clampf (dr.down (bufR + i * MASTER_OS), -1.0f, 1.0f); }
    else for (int i = 0; i < n; ++i) { L[i] = bufL[i]; R[i] = bufR[i]; }
    for (int i = 0; i < n; ++i) outPeak = maxf (outPeak, maxf (absf (L[i]), absf (R[i])));
}

} // namespace sk
