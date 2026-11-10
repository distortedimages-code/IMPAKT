// IMPAKT — the FX machines (see sk_fx.h): the routing, the gate and the gesture, the five effects
#include "sk_engine.h"
#include "sk_weft.h"
#include "sk_stencil.h"

namespace sk {

namespace {

inline int nextPow2 (int x) { int p = 1; while (p < x) p <<= 1; return p; }
inline double floord_ (double x) { return __builtin_floor (x); }
inline double maxd_ (double a, double b) { return a > b ? a : b; }
inline double mind_ (double a, double b) { return a < b ? a : b; }

// cubic Hermite between b[i] and b[i + 1], f in [0, 1]
inline float hermR (const float* b, int mask, int i, float f)
{
    const float xm1 = b[(i - 1) & mask], x0 = b[i & mask], x1 = b[(i + 1) & mask], x2 = b[(i + 2) & mask];
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}
// d samples behind the write index w (d >= 3: the four points are all written)
inline float readBehind (const float* b, int mask, int w, float d)
{
    const int di = (int) d;
    const float fd = d - (float) di;
    return hermR (b, mask, w - di - 1, 1.0f - fd);
}
// the same, linear (for the lengths that hardly move: the cheaper read)
inline float readBehindL (const float* b, int mask, int w, float d)
{
    const int di = (int) d;
    const float fd = d - (float) di, a = b[(w - di) & mask], c = b[(w - di - 1) & mask];
    return a + (c - a) * fd;
}
inline float readBehindD (const float* b, int mask, int w, double d)
{
    const int di = (int) d;
    const float fd = (float) (d - (double) di);
    return hermR (b, mask, w - di - 1, 1.0f - fd);
}
// a buffer of length L (any), read at an absolute position (a double) with Hermite
inline float readAbs (const float* b, int L, double pos)
{
    double q = pos - floord_ (pos / (double) L) * (double) L;
    int i = (int) q; if (i >= L) i = L - 1;
    const float f = (float) (q - (double) i);
    const int im1 = i > 0 ? i - 1 : L - 1, i1 = i + 1 < L ? i + 1 : 0, i2 = i1 + 1 < L ? i1 + 1 : 0;
    const float xm1 = b[im1], x0 = b[i], x1 = b[i1], x2 = b[i2];
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}
inline float onePole (float hz, float sr) { return 1.0f - expf_ (-TWO_PI * minf (hz, 0.45f * sr) / sr); }

// Walsh-Hadamard of 8 (orthonormal: energy kept)
inline void had8 (float* a)
{
    for (int h = 1; h < 8; h <<= 1)
        for (int i = 0; i < 8; i += h << 1)
            for (int j = i; j < i + h; ++j) { const float x = a[j], y = a[j + h]; a[j] = x + y; a[j + h] = x - y; }
    for (int i = 0; i < 8; ++i) a[i] *= 0.35355339f;
}

// VERB: the lines at SIZE = 1 (ms: primes, no common resonance; the even ones left, the odd ones right), the allpass
// in front of each (under 15 ms), the early reflections (where each line is tapped, how loud), how the input goes in
const float VMS[FX_VL]   = { 151.0f, 163.0f, 181.0f, 193.0f, 211.0f, 227.0f, 241.0f, 257.0f };
const float VAPMS[FX_VL] = { 4.3f, 5.3f, 6.1f, 7.3f, 8.9f, 10.1f, 11.3f, 13.7f };
const float VTAP[2] = { 0.37f, 0.71f }, VTAPG[2] = { 0.42f, 0.28f };
const float VIN[FX_VL]  = { 1, 1, -1, 1, 1, -1, -1, -1 };
const float VOS[FX_VL]  = { 1, 1, 1, -1, -1, 1, -1, -1 };      // (the signs the lines come out with)
// ECHO: the zones (s), its halo's allpasses (ms), the tempo's divisions (16ths)
const float EZ_LO[4] = { 0.0015f, 0.03f, 0.25f, 1.0f }, EZ_HI[4] = { 0.03f, 0.35f, 1.6f, 5.0f };
const float EAPMS[8] = { 7.3f, 11.1f, 17.3f, 23.9f, 8.3f, 12.7f, 16.1f, 25.7f };   // (left, then right)
const float EDIV[15] = { 0.5f, 2.0f / 3.0f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f, 8.0f / 3.0f, 3.0f, 4.0f, 16.0f / 3.0f, 6.0f, 8.0f, 12.0f, 16.0f, 32.0f };
// BEND: its divisions on the tempo (16ths), its repeats
const float BDIV[6] = { 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };
const int   BREP[8] = { 0, 2, 3, 4, 6, 8, 12, 16 };
// TAPE: the machines (bandwidth, wow rate and depth, flutter rate and depth, hiss, head bump, its frequency)
struct TapeModel { float bw, wowHz, wowSc, flHz, flSc, hissDb, bumpDb, bumpHz; };
const TapeModel TAPES[FX_TAPES] = {
    { 11000.0f, 0.9f, 1.0f,  8.5f, 1.0f, -62.0f, 2.0f,  90.0f },   // CASS
    {  8000.0f, 0.45f, 1.3f, 11.0f, 1.6f, -57.0f, 1.0f, 120.0f },  // VHS
    { 18000.0f, 0.35f, 0.5f,  6.0f, 0.5f, -72.0f, 3.0f,  55.0f },  // REEL
    {  4800.0f, 1.6f, 1.8f,  14.0f, 2.0f, -50.0f, 0.0f, 150.0f },  // MICRO
};
// CODEC: band edges (Hz)
const float CBE[FX_CBANDS + 1] = { 0, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400,
                                   5300, 6400, 9500, 24000 };

} // namespace

bool fxMachine (int m) { return (m >= M_FXVERB && m <= M_FXCODEC) || m == M_FXFUZZ || m == M_FXWEFT || m == M_FXSTENCIL; }
int  fxActs (int m) { return m == M_FXSTENCIL ? STC_ACTS : m == M_FXECHO || m == M_FXVERB ? 3 : 2; }   // (WEFT: IN, FRZ; STENCIL: IN, HOLD, FLIP, PLUK)

// which tracks an FX track takes (a bit each): the one before, every one before (but those an FX track before it
// already takes: they are in its sound), or one (never itself)
static u32 fxSrcMask (const Track& t, int ti, u32 takenBefore)
{
    const float* lk = t.voice[t.cur].lk;
    const float v = lk[P_A] >= 0.0f ? lk[P_A] : t.p[P_A];
    // (v83: STENCIL's list starts with NONE: it plays its loop alone)
    const int m = machineOf (lk[P_MACH] >= 0.0f ? lk[P_MACH] : t.p[P_MACH]);
    const int s = m == M_FXSTENCIL ? idxOf (v, FX_SRCS + 1) - 1 : idxOf (v, FX_SRCS);
    if (s < 0) return 0u;
    if (s == 0) return ti > 0 ? 1u << (ti - 1) : 0u;
    if (s == 1) return ((1u << ti) - 1u) & ~takenBefore;
    const int k = s - 2;
    return k != ti ? 1u << k : 0u;
}

// v78, WEFT: its carrier (CARR, D: SIN TRI SAW, then T1 .. T8): the track it takes (never itself), or none
static u32 fxCarMask (const Track& t, int ti)
{
    const float* lk = t.voice[t.cur].lk;
    const int c = idxOf (lk[P_D] >= 0.0f ? lk[P_D] : t.p[P_D], 11) - 3;
    return c >= 0 && c != ti ? 1u << c : 0u;
}

int Engine::fxMachineOf (const Track& t) const
{
    const float* lk = t.voice[t.cur].lk;
    return machineOf (lk[P_MACH] >= 0.0f ? lk[P_MACH] : t.p[P_MACH]);
}

void Engine::fxPrepass()
{
    fxTaken = 0;
    for (int ti = 0; ti < NUM_TRACKS; ++ti)
        if (fxMachine (fxMachineOf (tr[ti])))
        {
            const int m = fxMachineOf (tr[ti]);
            fx[ti].carMask = m == M_FXWEFT ? fxCarMask (tr[ti], ti) : 0u;
            fx[ti].srcMask = fxSrcMask (tr[ti], ti, fxTaken) & ~fx[ti].carMask;
            fxTaken |= fx[ti].srcMask | fx[ti].carMask;
        }
}

// lays the machine's lines out in the track's memory (at this sample rate) and clears them
void Engine::fxSetup (FxState& f, int m)
{
    f.kind = m; f.sr = sr;
    int used = 0;
    if (m == M_FXVERB)
    {
        // the lines (the longest twice: DPTH moves a length up to its double), the pre-delay a side (twice its
        // longest: REV), the allpasses, the pitch shifters' buffers
        int lm = nextPow2 ((int) (0.5f * sr) + 8), pl = nextPow2 ((int) (1.1f * sr) + 8);
        const int ap = mini (4096, nextPow2 ((int) (0.03f * sr) + 8)), sh = 4096;
        while (FX_VL * lm + 2 * pl + FX_VL * ap + FX_VSH * sh > FX_MEM && lm > 4096) { lm >>= 1; if (pl > 2 * lm) pl >>= 1; }
        f.vLmax = lm; f.vLmask = lm - 1;
        f.vPre0 = FX_VL * lm; f.vPreMask = pl - 1;
        f.vApMax = ap; f.vApMask = ap - 1; f.vAp0 = f.vPre0 + 2 * pl;
        f.vSh0 = f.vAp0 + FX_VL * ap; f.vShMask = sh - 1;
        used = f.vSh0 + FX_VSH * sh;
        f.vW = f.vPreW = 0; f.vGq = 0;
        for (int j = 0; j < FX_VL; ++j)
        {
            f.vDSm[j] = -1.0f; f.vASm[j] = -1.0f; f.vLp[j] = f.vHp[j] = f.vLo[j] = 0.0f;
            for (int k = 0; k < FX_VG; ++k) f.vGr[j][k] = f.rng.bi();
        }
        for (int k = 0; k < FX_VSH; ++k) f.vShF[k] = 0.0f;
        f.vPh = 0.0f; f.vGPh = 0.0f; f.vShPh = 0.0f; f.vShMix = 0.0f; f.vPreSm = -1.0f; f.vFrz = 0.0f; f.vRev = 0.0f; f.vRvAge = 0.0f;
        f.vHpIn[0] = f.vHpIn[1] = 0.0f; f.vTlo[0] = f.vTlo[1] = 0.0f; f.vRt = 0.0f; f.vSize = 0.0f;
    }
    else if (m == M_FXECHO)
    {
        f.eLine = 1 << 18; f.eMask = f.eLine - 1;
        int o = 2 * f.eLine;
        for (int k = 0; k < 8; ++k) { f.eApLen[k] = maxi (2, (int) (EAPMS[k] * 0.001f * sr)); f.eAp0[k] = o; o += f.eApLen[k]; f.eApW[k] = 0; }
        used = o;
        f.eW = 0; f.eD = -1.0; f.eMuPh = 0.0f; f.eMuR = f.eMuRT = 0.0f; f.eHold = f.eFlip = 0.0f; f.eTime = 0.0f; f.eFb = 0.0f; f.eDuck = 0.0f;
        f.eLpL.reset(); f.eLpR.reset(); f.eHpL = f.eHpR = 0.0f;
        f.eGa[0] = f.eGa[1] = -1.0f; f.eGn = 0;
    }
    else if (m == M_FXBEND)
    {
        f.bMask = FX_MEM / 2 - 1; used = FX_MEM;
        f.bW = 0.0; f.bDivIdx = -1e18; f.bDivStart = f.bPrevStart = 0.0; f.bPrevLen = 0.0f; f.bFreeCnt = 0.0; f.bHoldStart = 0.0;
        f.bCur = FxGrain {}; f.bCur.live = 1; f.bCur.on = 1; f.bCur.gain = f.bCur.gain1 = 1.0f;
        f.bOld = f.bCur; f.bOld.on = 0; f.bFade = 1.0f; f.bFadeInc = 0.0f; f.bOld2 = f.bOld; f.bFade2 = 1.0f; f.bFadeInc2 = 0.0f;
        f.bEv = 0; f.bRep = 0; f.bDivN = 0; f.bHoldF = 0.0f; f.bHoldLen = 0.0f; f.bDecH[0] = f.bDecH[1] = 0.0f; f.bHCnt = 0.0f; f.bDropG = 1.0f; f.bDHold = 1.0f; f.bDropLeft = 0;
    }
    else if (m == M_FXTAPE)
    {
        f.tMask = (1 << 17) - 1; used = 2 << 17;
        f.tW = 0; f.tStopLag = 0.0; f.tXLag = 0.0; f.tSpeed = 1.0f; f.tXf = 0.0f; f.tStopPh = 0;
        f.tD0 = -1.0f; f.tWowPh = 0.0f; f.tWowR = f.tWowRT = 0.0f; f.tFlPh = 0.0f; f.tFlN[0] = f.tFlN[1] = 0.0f;
        f.tWowR2 = f.tWowRT2 = 0.0f; f.tFlN2[0] = f.tFlN2[1] = 0.0f;
        f.tSnag = 0.0f; f.tSnagT = 0.0f; f.tSnagLen = 1.0f; f.tSnagAmp = 0.0f;
        f.tDrop = 1.0f; f.tDropT = 0.0f; f.tDropLeft = 0.0f; f.tDropLen = 1.0f; f.tCrk = 1.0f; f.tCrkT = 0.0f; f.tCrkLeft = 0.0f;
        for (int c = 0; c < 2; ++c)
        {
            f.tPeLp[c] = f.tDeLp[c] = f.tHpS[c] = 0.0f; f.tPink[c][0] = f.tPink[c][1] = f.tPink[c][2] = 0.0f;
            f.tLp[c].reset(); f.tBump[c].reset(); f.tLp2[c].reset(); f.tTlo[c] = 0.0f;
        }
        f.tCents = 0.0f; f.tLagPrev = -1.0;
        f.tEin = f.tEout = 0.0f; f.tMk = 1.0f;
    }
    else if (m == M_FXCODEC)
    {
        f.cfft.init (FX_CFN);
        for (int i = 0; i < FX_CFN; ++i) { const float s = sin2pi (0.5f * ((float) i + 0.5f) / (float) FX_CFN); f.cWin[i] = s; }   // sqrt-Hann
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < FX_CFN; ++i) f.cIn[c][i] = f.cOla[c][i] = f.cDry[c][i] = 0.0f;
            for (int k = 0; k < FX_CNB; ++k) f.cPRe[c][k] = f.cPIm[c][k] = 0.0f;
            for (int b = 0; b < FX_CBANDS; ++b) { f.cKeep[c][b] = 1.0f; f.cOff[c][b] = 0.0f; }
        }
        f.cRing = 0; f.cHop = 0; f.cPktLeft = 0; f.cPktMode = 0; f.cBwN = 0.0f; f.cBw = 20000.0f; f.cKept = 1.0f;
    }
    else if (m == M_FXWEFT)
    {
        // (Warps' modulator, laid out in the track's memory)
        if (weftBytes() <= (unsigned) sizeof (f.mem)) weftInit (f.mem, sr);
    }
    else if (m == M_FXSTENCIL)
    {
        // (v83: its two lines and its halo, laid out in the track's memory: it clears what it uses)
        if (stencilBytes() <= (unsigned) sizeof (f.mem)) stencilInit (f.mem, sr);
        return;
    }
    else if (m == M_FXFUZZ)
    {
        for (int c = 0; c < 2; ++c)
        {
            f.zOs[c].reset();
            for (int q = 0; q < 9; ++q) f.zX[c][q].reset();
            for (int k = 0; k < 3; ++k) { f.zDelta[c][k] = 0.0f; f.zSus[c][k] = 0.0f; f.zSusG[c][k] = 1.0f; }
            f.zTone[c] = f.zHp[c] = 0.0f; f.zD[c][0] = 0.0f;
        }
        f.zGEnv = 0.0f; f.zGOpen = 1.0f; f.zGHold = 0.0f; f.zGGain = 1.0f;
        f.zKey[0] = f.zKey[1] = -1.0f; f.zXlo = f.zXhi = -1.0f; f.zMix = 0.0f;
        for (int k = 0; k < 3; ++k) { f.zMk[k] = 1.0f; f.zGb[k] = 1.0f; f.zPk[k] = 0.0f; }
        f.zDrift = 0.0f; f.zBias = 0.5f;
        f.zSubDet.reset(); f.zSubG[0].reset(); f.zSubG[1].reset();
        for (int k = 0; k < 2; ++k) { f.zSubP[k][0].reset(); f.zSubP[k][1].reset(); }
        f.zSubEnv = f.zSubCnt = f.zSubPer = f.zSubPrev = f.zSubPk = f.zSubLk = 0.0f;
        f.zSubCmp = -1; f.zSubF1 = f.zSubF2 = -1; f.zSubW = 0;
        for (int i = 0; i < FZ_SUBDL; ++i) f.zSubDl[i] = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 3; ++k) { for (int q = 0; q < 4; ++q) f.zCs[c][k][q] = 0.0f; f.zLat[c][k] = 0; f.zLatV[c][k] = 0.0f; }
        f.zRng = 0x9E3779B9u; f.zCirc = 0;
    }
    for (int i = 0; i < used; ++i) f.mem[i] = 0.0f;
}

void Engine::fxReset (int ti)
{
    FxState& f = fx[ti];
    unsigned char* b = reinterpret_cast<unsigned char*> (&f);
    const __SIZE_TYPE__ small = (__SIZE_TYPE__) (reinterpret_cast<unsigned char*> (f.mem) - b);
    for (__SIZE_TYPE__ i = 0; i < small; ++i) b[i] = 0;
    f.kind = -1; f.sr = 0.0f; f.mixS = 0.0f; f.volS = 0.0f;
    f.rng.seed (0x9E3779B9u ^ (u32) (ti * 0x85EBCA6Bu + 1u));
    for (int k = 0; k < 5; ++k) { fxTone[ti].s[k][0] = fxTone[ti].s[k][1] = 0.0f; }
    fxTone[ti].on = 0;
}

// ----------------------------------------------------------------- VERB
// DECAY: the lines' gain a pass, up to 1 at 80 % (no loss: it sustains), 1.25 at the top (the saturation holds it)
static float fxVerbGain (float v)
{
    if (v >= 0.8f) return 1.0f + 0.25f * (v - 0.8f) / 0.2f;
    const float u = 1.0f - v / 0.8f;
    return 1.0f - 0.92f * u * sqrtf_ (u);
}
// the saturation before the matrix: a cubic, unity at small levels, flat at th (2/3 th out)
static inline float vSat (float x, float th, float ith)
{
    float u = x * ith; u = u > 1.0f ? 1.0f : (u < -1.0f ? -1.0f : u);
    return th * (u - u * u * u * (1.0f / 3.0f));
}
// SHFT: what half the lines are shifted by a pass (semitones): none at the left, then from an octave down to one up
// (10 cents either side of none: a detune)
const float VSHIFT[13] = { 0.0f, -12.0f, -7.0f, -5.0f, -3.0f, -1.0f, -0.1f, 0.1f, 1.0f, 3.0f, 5.0f, 7.0f, 12.0f };

void Engine::fxVerb (FxState& f, const float* P, const float* xL, const float* xR, const float* g, float* wL, float* wR, int n)
{
    float* mem = f.mem;
    const float size = P[P_B], absorb = P[P_D], tilt = (P[P_TUNE] - 0.5f) * 2.0f;
    const float speed = P[P_XA], depth = P[P_XB], pre = P[P_XC], shape = P[P_XD], bass = P[P_XF];
    const int act = idxOf (P[P_XE], 3), shi = idxOf (P[P_XG], 13);
    const float frzT = act == 1 ? f.gestF : 0.0f, revT = act == 2 ? f.gestF : 0.0f;
    // SIZE: from a cupboard (lines of a millisecond: metal, combs) to a stadium; the allpasses shrink with it more slowly
    const float scale = 0.004f * powf_ (250.0f, size), aScale = powf_ (scale, 0.7f);
    const float lim = (float) (f.vLmax - 8), alim = (float) (f.vApMax - 4);
    float dT[FX_VL], aT[FX_VL];
    for (int j = 0; j < FX_VL; ++j)
    {
        dT[j] = clampf (VMS[j] * 0.001f * sr * scale, 3.0f, lim * 0.48f); if (f.vDSm[j] < 0.0f) f.vDSm[j] = dT[j];
        aT[j] = clampf (VAPMS[j] * 0.001f * sr * aScale, 1.5f, alim); if (f.vASm[j] < 0.0f) f.vASm[j] = aT[j];
    }
    const float g0 = fxVerbGain (P[P_C]);
    {
        float L = 0.0f; for (int j = 0; j < FX_VL; ++j) L += f.vDSm[j] + f.vASm[j];
        L *= 1.0f / (float) FX_VL;
        f.vRt = g0 >= 0.9999f ? -1.0f : 3.0f * L / (-0.30103f * log2f_ (maxf (g0, 1e-4f)) * sr);
        f.vSize = f.vDSm[FX_VL - 1] / sr * 1000.0f;
    }
    // ABSRB: the branches' low-pass, 20 kHz down to 250 Hz; BASS: under 220 Hz the tail lasts up to 4 times longer
    const float cD0 = onePole (20000.0f * powf_ (0.0125f, absorb), sr), cLo = onePole (220.0f, sr);
    const float mult = powf_ (4.0f, bass), gLo0 = g0 >= 1.0f ? g0 : powf_ (g0, 1.0f / mult);
    const float cH = onePole (4.0f, sr), cIn = onePole (25.0f, sr);
    const float apG = 0.55f, th = 0.85f, ith = 1.0f / th;
    // the modulation: 0.1 .. 30 Hz; its depth up to the whole length of each line; SHAPE from the sine (each line an
    // eighth of a turn from the next) to grains of random lengths (four on each line, raised cosines, one starting
    // every quarter of the period)
    const float hz = 0.1f * powf_ (300.0f, speed), inc = hz / sr, dep = depth * depth * 0.97f;
    const float shp = clampf (shape, 0.0f, 1.0f);
    // SHFT: two heads over 50 ms windows on each of the last four lines, half their sound shifted (the shift's share
    // fading in and out), going up through a low-pass (7 kHz: it shimmers, it does not screech), going down through a
    // high-pass (90 Hz: it falls, it does not rumble)
    const float ratio = exp2f_ (VSHIFT[shi] * (1.0f / 12.0f)), shW = minf (0.05f * sr, (float) f.vShMask - 8.0f);
    const float shInc = (1.0f - ratio) / shW, shT = shi == 0 ? 0.0f : 0.5f;
    const bool shUp = ratio > 1.0f;
    const float cSh = shUp ? onePole (7000.0f, sr) : onePole (60.0f, sr);
    // (the size slides: a sweep bends the tail, a jump knocks it; the pre-delay too)
    const float kS = 1.0f - expf_ (-1.0f / (0.03f * sr)), kP = 1.0f - expf_ (-1.0f / (0.05f * sr)), kF = 1.0f - expf_ (-1.0f / (0.03f * sr));
    const float preT = pre * pre * 0.5f * sr;
    if (f.vPreSm < 0.0f) f.vPreSm = preT;
    const float gt = exp2f_ (tilt), glo = 1.0f / gt, cT = onePole (650.0f, sr);
    float* preL = mem + f.vPre0;
    float* preR = preL + (f.vPreMask + 1);
    for (int i = 0; i < n; ++i)
    {
        // the input, its low end cleaned, the pre-delay (REV: read backwards, two heads crossfading)
        float xl = xL[i] * g[i], xr = xR[i] * g[i];
        f.vHpIn[0] += cIn * (xl - f.vHpIn[0]); xl -= f.vHpIn[0];
        f.vHpIn[1] += cIn * (xr - f.vHpIn[1]); xr -= f.vHpIn[1];
        preL[f.vPreW] = xl; preR[f.vPreW] = xr;
        f.vPreSm += kP * (preT - f.vPreSm);
        f.vRev += kF * (revT - f.vRev);
        float pl = xl, pr = xr;
        if (f.vPreSm >= 1.0f) { pl = readBehind (preL, f.vPreMask, f.vPreW + 1, f.vPreSm + 1.0f); pr = readBehind (preR, f.vPreMask, f.vPreW + 1, f.vPreSm + 1.0f); }
        if (f.vRev > 0.001f)
        {
            const float Ls = clampf (maxf (f.vPreSm, 0.15f * sr), 64.0f, 0.45f * (float) (f.vPreMask + 1));
            f.vRvAge += 1.0f / Ls; if (f.vRvAge >= 1.0f) f.vRvAge -= 1.0f;
            float rl = 0.0f, rr = 0.0f;
            for (int h = 0; h < 2; ++h)
            {
                float a = f.vRvAge + 0.5f * (float) h; if (a >= 1.0f) a -= 1.0f;
                const float w = sin2pi (0.5f * a), w2 = w * w, d = 2.0f * a * Ls + 3.0f;
                rl += w2 * readBehind (preL, f.vPreMask, f.vPreW + 1, d); rr += w2 * readBehind (preR, f.vPreMask, f.vPreW + 1, d);
            }
            pl += (rl - pl) * f.vRev; pr += (rr - pr) * f.vRev;
        }
        f.vPreW = (f.vPreW + 1) & f.vPreMask;
        // the modulation's clocks
        f.vPh += inc; if (f.vPh >= 1.0f) f.vPh -= 1.0f;
        f.vGPh += inc; if (f.vGPh >= 1.0f) f.vGPh -= 1.0f;
        const int q = (int) (f.vGPh * (float) FX_VG);
        if (q != f.vGq) { f.vGq = q; for (int j = 0; j < FX_VL; ++j) f.vGr[j][q] = f.rng.bi(); }   // a new grain on layer q
        float gw[FX_VG];
        for (int k = 0; k < FX_VG; ++k) { float a = f.vGPh - (float) k / (float) FX_VG; if (a < 0.0f) a += 1.0f; gw[k] = 0.25f - 0.25f * cos2pi (a); }   // (they sum to 1)
        // FREEZE: no loss, no input, the walls open
        f.vFrz += kF * (frzT - f.vFrz);
        const float frz = f.vFrz, cD = cD0 + (1.0f - cD0) * frz, gl = g0 + (1.0f - g0) * frz, gLo = gLo0 + (1.0f - gLo0) * frz;
        float y[FX_VL], z[FX_VL], er[FX_VL];
        for (int j = 0; j < FX_VL; ++j)
        {
            f.vDSm[j] += kS * (dT[j] - f.vDSm[j]);
            f.vASm[j] += kS * (aT[j] - f.vASm[j]);
            const float D = f.vDSm[j];
            const float* lj = mem + j * f.vLmax;
            float ys = 0.0f, yg = 0.0f;
            if (shp < 0.999f) ys = readBehind (lj, f.vLmask, f.vW, clampf (D * (1.0f + dep * sin2pi (f.vPh + 0.125f * (float) j)), 3.0f, lim));
            if (shp > 0.001f) for (int k = 0; k < FX_VG; ++k) yg += gw[k] * readBehindL (lj, f.vLmask, f.vW, clampf (D * (1.0f + dep * f.vGr[j][k]), 3.0f, lim));
            const float yj = ys + (yg - ys) * shp;
            y[j] = yj;
            // the early reflections: the line tapped on its way (they follow SIZE too)
            er[j] = VTAPG[0] * readBehindL (lj, f.vLmask, f.vW, maxf (3.0f, D * VTAP[0])) + VTAPG[1] * readBehindL (lj, f.vLmask, f.vW, maxf (3.0f, D * VTAP[1]));
            // the branch: its absorption, its DC out, its gain (the lows their own), the saturation
            f.vLp[j] += cD * (yj - f.vLp[j]);
            float zj = f.vLp[j];
            f.vHp[j] += cH * (zj - f.vHp[j]); zj -= f.vHp[j];
            f.vLo[j] += cLo * (zj - f.vLo[j]);
            z[j] = vSat (f.vLo[j] * gLo + (zj - f.vLo[j]) * gl, th, ith);
        }
        // the matrix (Hadamard, lossless)
        had8 (z);
        // SHFT: the last four lines shifted in pitch, a pass at a time (the others not: they mix)
        f.vShMix += kF * (shT - f.vShMix);
        if (f.vShMix > 0.0005f)
        {
            f.vShPh += shInc; if (f.vShPh >= 1.0f) f.vShPh -= 1.0f; if (f.vShPh < 0.0f) f.vShPh += 1.0f;
            for (int s2 = 0; s2 < FX_VSH; ++s2)
            {
                float* sb = mem + f.vSh0 + s2 * (f.vShMask + 1);
                const int j = FX_VL - FX_VSH + s2;
                sb[f.vW & f.vShMask] = z[j];
                float ysh = 0.0f;
                for (int h = 0; h < 2; ++h)
                {
                    float a = f.vShPh + 0.5f * (float) h + 0.23f * (float) s2; a -= floorf_ (a);
                    const float w = sin2pi (0.5f * a);
                    ysh += w * w * readBehindL (sb, f.vShMask, f.vW + 1, 2.0f + a * shW);
                }
                f.vShF[s2] += cSh * (ysh - f.vShF[s2]);
                ysh = shUp ? f.vShF[s2] : ysh - f.vShF[s2];
                z[j] += (ysh - z[j]) * f.vShMix;
            }
        }
        // the input (the left into the even lines, the right into the odd ones), the allpass in front of each line
        const float il = pl * 0.5f * (1.0f - frz), ir = pr * 0.5f * (1.0f - frz), den = (i & 1) ? 1e-18f : -1e-18f;
        for (int j = 0; j < FX_VL; ++j)
        {
            float* ap = mem + f.vAp0 + j * f.vApMax;
            const float a = z[j] + VIN[j] * ((j & 1) ? ir : il);
            const float v = readBehindL (ap, f.vApMask, f.vW, f.vASm[j]), u = a + apG * v;
            ap[f.vW & f.vApMask] = u;
            mem[j * f.vLmax + (f.vW & f.vLmask)] = v - apG * u + den;
        }
        f.vW = (f.vW + 1) & f.vLmask;          // (the allpasses' and shifters' buffers divide the lines': one index)
        // out: the even lines left, the odd ones right, each with its reflections
        float L = 0.0f, R = 0.0f;
        for (int j = 0; j < FX_VL; j += 2) L += VOS[j] * (1.0f * y[j] + 0.78f * er[j]);
        for (int j = 1; j < FX_VL; j += 2) R += VOS[j] * (1.0f * y[j] + 0.78f * er[j]);
        f.vTlo[0] += cT * (L - f.vTlo[0]); L = f.vTlo[0] * glo + (L - f.vTlo[0]) * gt;
        f.vTlo[1] += cT * (R - f.vTlo[1]); R = f.vTlo[1] * glo + (R - f.vTlo[1]) * gt;
        wL[i] = L; wR[i] = R;
    }
    for (int j = 0; j < FX_VL; ++j) if (isBad (f.vLp[j]) || isBad (f.vHp[j]) || isBad (f.vLo[j])) { fxSetup (f, M_FXVERB); return; }
}

// ----------------------------------------------------------------- ECHO
void Engine::fxEcho (FxState& f, const float* P, const float* xL, const float* xR, const float* g, float* wL, float* wR, int n, double spt)
{
    float* lineL = f.mem;
    float* lineR = f.mem + f.eLine;
    const int zone = idxOf (P[P_B], 4);
    const float rate = P[P_C], rep = P[P_D], color = (P[P_TUNE] - 0.5f) * 2.0f;
    const float mu = P[P_XA], halo = P[P_XB], skew = P[P_XC];
    const bool sync = idxOf (P[P_XD], 2) == 1;
    const float maxT = (float) (f.eLine - 64) / sr;
    float T = EZ_LO[zone] * powf_ (EZ_HI[zone] / EZ_LO[zone], rate);
    if (sync && zone >= 1 && spt > 0.0)
    {
        // the division nearest (in ratio) to where RATE is
        const float t16 = T / (float) spt;
        int best = 0; float bd = 1e9f;
        for (int k = 0; k < 15; ++k) { const float d = absf (log2f_ (EDIV[k] / t16)); if (d < bd) { bd = d; best = k; } }
        T = EDIV[best] * (float) spt;
    }
    T = minf (T, maxT);
    const double dT = (double) maxf (T * sr, 4.0f);
    if (f.eD < 0.0) f.eD = dT;
    const double kG = 1.0 - (double) expf_ (-1.0f / ((zone == 0 ? 0.04f : 0.22f) * sr));
    const float muRate = 0.12f * powf_ (50.0f, mu), muFrac = mu * mu * (zone == 0 ? 0.6f : 0.018f);
    const float muInc = muRate / sr, kMu = onePole (muRate * 1.3f, sr);
    const float fb = rep * 1.1f;
    const float lpHz = color < 0.0f ? 14000.0f * powf_ (0.05f, -color) : 14000.0f + 4500.0f * color;
    const float hpHz = color > 0.0f ? 30.0f * powf_ (60.0f, color) : 30.0f;
    const float drive = 1.0f + maxf (0.0f, -color) * 2.5f, idrive = 1.0f / drive;
    f.eLpL.set (lpHz, 0.7f, sr); f.eLpR.set (lpHz, 0.7f, sr);
    const float cHp = onePole (hpHz, sr);
    const float apG = 0.6f * halo;
    const int act = idxOf (P[P_XE], 3);
    const float holdT = act == 1 ? f.gestF : 0.0f, flipT = act == 2 ? f.gestF : 0.0f;
    const float kF = 1.0f - expf_ (-1.0f / (0.015f * sr));
    // PING: the repeats cross sides (all of it: the input goes in on the left, then bounces); DUCK: the repeats step
    // back while the source plays (5 ms down, 200 ms back up)
    const float ping = P[P_XF], duck = P[P_XG];
    const float kDa = 1.0f - expf_ (-1.0f / (0.005f * sr)), kDr = 1.0f - expf_ (-1.0f / (0.2f * sr));
    f.eTime = (float) (f.eD / sr * 1000.0); f.eFb = fb;
    const double lim = (double) (f.eLine - 8);
    for (int i = 0; i < n; ++i)
    {
        f.eD += kG * (dT - f.eD);
        f.eMuPh += muInc;
        if (f.eMuPh >= 1.0f) { f.eMuPh -= 1.0f; f.eMuRT = f.rng.bi(); }
        f.eMuR += kMu * (f.eMuRT - f.eMuR);
        const float m = 0.65f * sin2pi (f.eMuPh) + 0.35f * f.eMuR;
        double d = f.eD * (1.0 + (double) (muFrac * m)); if (d < 3.0) d = 3.0; if (d > lim) d = lim;
        double dR = d * (1.0 + 0.35 * (double) skew); if (dR > lim) dR = lim;
        float yL = readBehindD (lineL, f.eMask, f.eW, d), yR = readBehindD (lineR, f.eMask, f.eW, dR);
        f.eHold += kF * (holdT - f.eHold);
        f.eFlip += kF * (flipT - f.eFlip);
        // FLIP: two grains overlapping by half, each one reading the recent past backwards (at its age a it reads the
        // sample 2a old: from where it started, back), on both lines
        if (f.eFlip > 0.0005f || flipT > 0.0f)
        {
            const float G = (float) maxd_ (mind_ (f.eD, 1.5 * (double) sr), 0.03 * (double) sr);
            const int c = f.eGn;
            if (f.eGa[c] < 0.0f) f.eGa[c] = 0.0f;
            else if (f.eGa[c] >= 0.5f * G && f.eGa[c ^ 1] < 0.0f) { f.eGn = c ^ 1; f.eGa[f.eGn] = 0.0f; }
            float rl = 0.0f, rr = 0.0f;
            for (int k = 0; k < 2; ++k)
            {
                if (f.eGa[k] < 0.0f) continue;
                if (f.eGa[k] >= G) { f.eGa[k] = -1.0f; continue; }
                const float w = sin2pi (0.5f * f.eGa[k] / G), w2 = w * w;
                const double dd = mind_ (2.0 * (double) f.eGa[k] + 4.0, lim);
                rl += w2 * readBehindD (lineL, f.eMask, f.eW, dd); rr += w2 * readBehindD (lineR, f.eMask, f.eW, dd);
                f.eGa[k] += 1.0f;
            }
            yL = yL + (rl - yL) * f.eFlip; yR = yR + (rr - yR) * f.eFlip;
        }
        // the colour of the repeats (out of the way while it holds)
        float lp, bp, hp;
        f.eLpL.tick (yL, lp, bp, hp); f.eHpL += cHp * (lp - f.eHpL); float cL = lp - f.eHpL;
        f.eLpR.tick (yR, lp, bp, hp); f.eHpR += cHp * (lp - f.eHpR); float cR = lp - f.eHpR;
        const float open = 1.0f - f.eHold;
        cL = yL + (cL - yL) * open; cR = yR + (cR - yR) * open;
        // HALO: the repeats through four allpasses a side, each pass blurring them a little more
        float hL = cL, hR = cR;
        if (apG > 0.0005f)
        {
            for (int k = 0; k < 8; ++k)
            {
                float* ap = f.mem + f.eAp0[k];
                float& h = k < 4 ? hL : hR;
                const float v = ap[f.eApW[k]], u = h + apG * v;
                ap[f.eApW[k]] = u; h = v - apG * u;
                if (++f.eApW[k] >= f.eApLen[k]) f.eApW[k] = 0;
            }
            hL = cL + (hL - cL) * halo * open; hR = cR + (hR - cR) * halo * open;
        }
        // the loops, each fed the other's with PING: saturated (a run-away stays a sound), linear while it holds
        const float fbG = fb + (1.0f - fb) * f.eHold;
        const float aL = hL + (hR - hL) * ping, aR = hR + (hL - hR) * ping;
        const float sL = tanhf_ (aL * fbG * drive) * idrive, sR = tanhf_ (aR * fbG * drive) * idrive;
        const float loopL = sL + (aL * fbG - sL) * f.eHold, loopR = sR + (aR * fbG - sR) * f.eHold;
        const float xl = xL[i] * g[i] * open, xr = xR[i] * g[i] * open, mid = 0.5f * (xl + xr);
        const float den = (i & 1) ? 1e-18f : -1e-18f;
        lineL[f.eW & f.eMask] = xl + (mid - xl) * ping + loopL + den;
        lineR[f.eW & f.eMask] = xr * (1.0f - ping) + loopR + den;
        f.eW = (f.eW + 1) & f.eMask;
        // DUCK: under the source, the repeats step back
        float dg = 1.0f;
        if (duck > 0.002f)
        {
            const float e = maxf (absf (xL[i]), absf (xR[i]));
            f.eDuck += (e > f.eDuck ? kDa : kDr) * (e - f.eDuck);
            dg = 1.0f - 0.92f * duck * minf (1.0f, f.eDuck * 8.0f);
        }
        wL[i] = hL * dg; wR[i] = hR * dg;
    }
    if (isBad (f.eLpL.ic1) || isBad (f.eLpR.ic1) || isBad (f.eHpL) || isBad (f.eHpR)) fxSetup (f, M_FXECHO);
}

// ----------------------------------------------------------------- BEND
// (a grain reads straight on: a repeat starts again before it runs out of its span, and a grain fading out reads on
// past it, into what was recorded next: no jump anywhere)
// (each side has its buffer, half the memory; both read on the same grains)
static inline float grainRead (const float* buf, const FxGrain& gr, const float* x, int i, double rateMul, double off)
{
    if (gr.live) return x[i];
    const double a = gr.acc * rateMul + off;
    const double pos = gr.dir > 0 ? gr.src + a : gr.src - 1.0 - a;
    return readAbs (buf, FX_MEM / 2, pos) * gr.gain;
}

// a seam: the grain playing fades out while the new one fades in (an older one still fading carries on alone)
static inline void bendSeam (FxState& f, const FxGrain& old, float len)
{
    if (f.bFade < 1.0f) { f.bOld2 = f.bOld; f.bFade2 = f.bFade; f.bFadeInc2 = f.bFadeInc; }
    f.bOld = old; f.bFade = 0.0f; f.bFadeInc = 1.0f / maxf (len, 1.0f);
}
// equal power, flat at both ends (no kink where it starts or stops)
static inline float seamIn (float u) { return sin2pi (0.25f * u * u * (3.0f - 2.0f * u)); }
static inline float seamOut (float u) { return sin2pi (0.25f * (1.0f + u * u * (3.0f - 2.0f * u))); }

void Engine::fxBendDivision (FxState& f, const float* P, double D)
{
    f.bPrevStart = f.bDivStart; f.bPrevLen = (float) (f.bW - f.bDivStart);
    f.bDivN = (f.bDivN + 1) & 0xFFFFF;
    f.bDivStart = f.bW;
    const bool held = f.bHoldF > 0.5f;
    const double src = held ? f.bHoldStart : f.bPrevStart;
    const double span = (double) maxf (held ? f.bHoldLen : f.bPrevLen, 64.0f);
    const float bend = (P[P_TUNE] - 0.5f) * 2.0f;
    const double rate = (double) exp2f_ (bend * 2.0f);
    const int rp = idxOf (P[P_C], 8), nrep = BREP[rp];
    const float brk = P[P_D];
    FxGrain g {};
    g.on = 1; g.dir = 1; g.gain = g.gain1 = 1.0f; g.src = src; g.span = span; g.rate = g.rate1 = rate; g.len = 0.0; g.ph = 0.0; g.acc = 0.0;
    f.bEv = 0;
    // a break: a jump back, backwards, a tape stop, a dropout, at double speed
    if (f.bPrevLen > 64.0f && f.rng.uni() < brk * 0.9f)
    {
        const float r = f.rng.uni();
        if (r < 0.3f)
        {
            const int k = 1 + mini ((int) (f.rng.uni() * 4.0f), 3);
            const double s2 = src - (double) k * span;
            if (s2 > f.bW - (double) (FX_MEM / 2) + 4.0 * span) g.src = s2;
            f.bEv = 2;
        }
        else if (r < 0.55f) { g.dir = -1; g.src = src + span; f.bEv = 3; }
        else if (r < 0.7f)  { g.rate1 = 0.0; g.gain1 = 0.0f; f.bEv = 4; }
        else if (r < 0.8f)  { g.gain = g.gain1 = 0.0f; f.bEv = 5; }
        else                { g.rate = g.rate1 = rate * 2.0; f.bEv = 6; }
        if (nrep > 1 && f.bEv != 5) g.len = D / (double) nrep;
    }
    else if (nrep > 1) { g.len = D / (double) nrep; f.bEv = 1; }
    else if (absf (bend) > 0.02f || held) f.bEv = 7;
    else g.live = 1;
    if (held && f.bEv == 0) f.bEv = 7;
    // a repeat never reads past its span: faster than the recording, it starts again sooner
    if (! g.live && g.rate > 1.0)
    {
        const double lmax = span / g.rate;
        if (g.len <= 0.0 || g.len > lmax) g.len = lmax;
    }
    else if (! g.live && g.len <= 0.0) g.len = span;
    f.bRep = nrep;
    const FxGrain old = f.bCur;
    f.bCur = g;
    // 3 ms, never more than a quarter of the repeat it starts
    bendSeam (f, old, (float) mind_ (0.003 * (double) sr, 0.25 * (g.len > 0.0 ? g.len : D)));
}

void Engine::fxBend (FxState& f, const float* P, const float* xL, const float* xR, float* wL, float* wR, int n, double pos16, double inc, double spt)
{
    float* bufL = f.mem;
    float* bufR = f.mem + FX_MEM / 2;
    const double BL = (double) (FX_MEM / 2);
    const bool sync = idxOf (P[P_XD], 2) == 1;
    double D16 = 0.0, D = 0.0;
    if (sync) { D16 = (double) BDIV[idxOf (P[P_B], 6)]; D = D16 * spt * (double) sr; }
    else D = (double) (0.04f * powf_ (60.0f, P[P_B]) * sr);
    if (D > BL * 0.2) D = BL * 0.2;
    const float crp = P[P_XA], sprd = P[P_XC];
    const int cmd = idxOf (P[P_XB], 3);
    const int act = idxOf (P[P_XE], 2);
    const float holdT = act == 1 ? f.gestF : 0.0f;
    if (holdT > 0.5f && f.bHoldF <= 0.5f) { f.bHoldStart = f.bPrevStart; f.bHoldLen = f.bPrevLen; }
    f.bHoldF = holdT;
    const int hold = (int) (1.0f + crp * crp * 40.0f);
    const float lv = exp2f_ (12.0f - crp * 9.0f);
    const float drv = 1.0f + crp * 20.0f, dComp = 1.0f / (1.0f + crp * 3.0f);
    for (int i = 0; i < n; ++i)
    {
        const int wi = (int) (f.bW - floord_ (f.bW / BL) * BL);
        bufL[wi] = xL[i]; bufR[wi] = xR[i];
        // a new division?
        if (sync && inc > 0.0)
        {
            const double k = floord_ ((pos16 + (double) i * inc) / D16 + 1e-9);
            if (k != f.bDivIdx) { const bool first = f.bDivIdx < -1e17; f.bDivIdx = k; if (! first) fxBendDivision (f, P, D); else f.bDivStart = f.bW; }
        }
        else
        {
            f.bFreeCnt += 1.0;
            if (f.bFreeCnt >= D) { f.bFreeCnt -= D; fxBendDivision (f, P, D); }
        }
        // a repeat that ends starts again (from its slice), faded like a new division
        FxGrain& c = f.bCur;
        if (! c.live && c.len > 0.0 && c.ph >= c.len)
        {
            const FxGrain old = c;
            c.ph -= c.len; c.acc = 0.0;
            bendSeam (f, old, (float) mind_ (0.002 * (double) sr, 0.25 * c.len));
        }
        // each side, and (SPRD) the same a little faster on the left, slower on the right: smeared apart
        float yl = grainRead (bufL, c, xL, i, 1.0, 0.0), yr = grainRead (bufR, c, xR, i, 1.0, 0.0);
        float al = c.live ? yl : grainRead (bufL, c, xL, i, 1.012, 0.003 * sr), ar = c.live ? yr : grainRead (bufR, c, xR, i, 0.988, 0.005 * sr);
        if (f.bFade < 1.0f && c.live && f.bOld.live) f.bFade = 1.0f;
        if (f.bFade < 1.0f)
        {
            const float a = seamIn (f.bFade), b = seamOut (f.bFade);
            const float ol = grainRead (bufL, f.bOld, xL, i, 1.0, 0.0), orr = grainRead (bufR, f.bOld, xR, i, 1.0, 0.0);
            yl = yl * a + ol * b; al = al * a + ol * b; yr = yr * a + orr * b; ar = ar * a + orr * b;
            f.bFade = minf (1.0f, f.bFade + f.bFadeInc);
            if (! f.bOld.live) { f.bOld.acc += f.bOld.rate; f.bOld.ph += 1.0; }
        }
        if (f.bFade2 < 1.0f)
        {
            const float w2 = seamOut (f.bFade2);
            const float ol = grainRead (bufL, f.bOld2, xL, i, 1.0, 0.0) * w2, orr = grainRead (bufR, f.bOld2, xR, i, 1.0, 0.0) * w2;
            yl += ol; al += ol; yr += orr; ar += orr;
            f.bFade2 = minf (1.0f, f.bFade2 + f.bFadeInc2);
            if (! f.bOld2.live) { f.bOld2.acc += f.bOld2.rate; f.bOld2.ph += 1.0; }
        }
        if (! c.live)
        {
            c.acc += c.rate; c.ph += 1.0;
            if (c.rate1 != c.rate) { c.rate += (c.rate1 - c.rate) * 4.0 / maxd_ (D, 1.0); if (c.rate < 0.0) c.rate = 0.0; }
            if (c.gain1 != c.gain) c.gain += (c.gain1 - c.gain) * (float) (3.0 / maxd_ (D, 1.0));
        }
        yl += sprd * 0.5f * (al - yl); yr += sprd * 0.5f * (ar - yr);
        // corrupt: DECIM (sample rate and bits), DROP (dropouts, both sides), DSTR (a softer then harder clip)
        if (crp > 0.002f)
        {
            if (cmd == 0)
            {
                if (f.bHCnt <= 0.0f) { f.bDecH[0] = yl; f.bDecH[1] = yr; f.bHCnt = (float) hold; }
                f.bHCnt -= 1.0f;
                yl = floorf_ (f.bDecH[0] * lv + 0.5f) / lv; yr = floorf_ (f.bDecH[1] * lv + 0.5f) / lv;
            }
            else if (cmd == 1)
            {
                if (f.bDropLeft <= 0) { f.bDropLeft = (int) (0.005f * sr); f.bDropG = f.rng.uni() < crp * 0.6f ? 0.0f : 1.0f; }
                --f.bDropLeft;
                f.bDHold += (f.bDropG - f.bDHold) * 0.02f;
                yl *= f.bDHold; yr *= f.bDHold;
            }
            else
            {
                const float k1 = (1.0f + crp * 0.4f), k2 = dComp * (1.0f + crp * 2.0f) * 0.5f, k3 = (1.0f - minf (1.0f, crp * 3.0f)) * 0.5f;
                yl = clampf (tanhf_ (yl * drv) * k1, -0.95f, 0.95f) * k2 + yl * k3;
                yr = clampf (tanhf_ (yr * drv) * k1, -0.95f, 0.95f) * k2 + yr * k3;
            }
        }
        wL[i] = yl; wR[i] = yr;
        f.bW += 1.0;
    }
}

// ----------------------------------------------------------------- TAPE
void Engine::fxTape (FxState& f, const float* P, const float* xL, const float* xR, float* wL, float* wR, int n)
{
    float* tape[2] = { f.mem, f.mem + (f.tMask + 1) };
    const TapeModel& M = TAPES[idxOf (P[P_XD], FX_TAPES)];
    const float wow = P[P_B], flut = P[P_C], satA = P[P_D], gen = P[P_XA], fail = P[P_XB], hissA = P[P_XC], wide = P[P_XF];
    const float tone = (P[P_TUNE] - 0.5f) * 2.0f;
    const float Aw = wow * wow * 0.0065f * sr * M.wowSc * (1.0f + 0.6f * gen);
    const float Af = flut * flut * 0.00035f * sr * M.flSc;
    const float D0 = 0.0015f * sr + 1.15f * (Aw + Af) + 0.004f * sr * fail;
    if (f.tD0 < 0.0f) f.tD0 = D0;
    const float kD = 1.0f - expf_ (-1.0f / (0.2f * sr));
    const float wowInc = M.wowHz * (1.0f + 0.3f * gen) / sr, kWr = onePole (M.wowHz * 0.7f, sr);
    const float flInc = M.flHz / sr, kFl = onePole (M.flHz * 1.5f, sr);
    const float cPe = onePole (2500.0f, sr), kE = 1.0f + satA;                 // record emphasis
    const float d = 0.25f + satA * 5.75f, bias = 0.15f * satA, tb = tanhf_ (bias);
    const float sech2 = 1.0f - tb * tb, norm = 1.0f / (d * maxf (sech2, 0.05f));
    // what the saturation takes in level, given back (slowly, like a tape machine's output trimmed by ear): a hot
    // track keeps its loudness, its peaks rounded
    const float kR = 1.0f - expf_ (-1.0f / (0.25f * sr)), kM = 1.0f - expf_ (-1.0f / (0.4f * sr));
    const float bw = M.bw * powf_ (0.18f, gen);
    for (int c = 0; c < 2; ++c) { f.tLp[c].set (bw, 0.75f, sr); f.tBump[c].set (M.bumpHz, 1.2f, sr); }
    const float bumpG = exp2f_ (M.bumpDb * (1.0f / 6.0206f)) - 1.0f;
    const float cHp = onePole (35.0f, sr);
    const float hissG = exp2f_ ((M.hissDb + 26.0f * hissA + 10.0f * gen) * (1.0f / 6.0206f));
    const float cT = onePole (900.0f, sr), gt = exp2f_ (tone), glo = 1.0f / gt;
    const float lam = fail * fail * fail * 5.0f / sr;                               // failures a sample
    const int act = idxOf (P[P_XE], 2);
    const bool stopping = act == 1 && f.gest;
    const float stopT = maxf (0.15f, minf (1.2f, 0.6f * 0.02f * powf_ (300.0f, P[P_DEC]))) * sr;
    // WIDE: the right track's wow and flutter their own, and its head a little out of azimuth
    const float azi = wide * 0.0003f * sr;
    for (int i = 0; i < n; ++i)
    {
        // the record side: emphasis, saturation, de-emphasis (the level given back on both tracks together)
        const float xin[2] = { xL[i], xR[i] };
        float u[2], sv[2];
        for (int c = 0; c < 2; ++c)
        {
            f.tPeLp[c] += cPe * (xin[c] - f.tPeLp[c]);
            u[c] = f.tPeLp[c] + (xin[c] - f.tPeLp[c]) * kE;
            sv[c] = (tanhf_ (d * u[c] + bias) - tb) * norm;
        }
        f.tEin += kR * (0.5f * (u[0] * u[0] + u[1] * u[1]) - f.tEin); f.tEout += kR * (0.5f * (sv[0] * sv[0] + sv[1] * sv[1]) - f.tEout);
        const float mk = f.tEout > 1e-9f ? minf (4.0f, maxf (1.0f, sqrtf_ ((f.tEin + 1e-12f) / f.tEout))) : 1.0f;
        f.tMk += kM * (mk - f.tMk);
        for (int c = 0; c < 2; ++c)
        {
            const float sc = sv[c] * f.tMk;
            f.tDeLp[c] += cPe * (sc - f.tDeLp[c]);
            tape[c][f.tW & f.tMask] = f.tDeLp[c] + (sc - f.tDeLp[c]) / kE;
        }
        // the transport: wow, flutter, snags, a stop (one capstan: both tracks; WIDE: the right one wanders apart)
        f.tD0 += kD * (D0 - f.tD0);
        f.tWowPh += wowInc; if (f.tWowPh >= 1.0f) { f.tWowPh -= 1.0f; f.tWowRT = f.rng.bi(); f.tWowRT2 = f.rng.bi(); }
        f.tWowR += kWr * (f.tWowRT - f.tWowR); f.tWowR2 += kWr * (f.tWowRT2 - f.tWowR2);
        f.tFlPh += flInc; if (f.tFlPh >= 1.0f) f.tFlPh -= 1.0f;
        f.tFlN[0] += kFl * (f.rng.bi() - f.tFlN[0]); f.tFlN[1] += kFl * (f.tFlN[0] - f.tFlN[1]);
        if (wide > 0.002f) { f.tFlN2[0] += kFl * (f.rng.bi() - f.tFlN2[0]); f.tFlN2[1] += kFl * (f.tFlN2[0] - f.tFlN2[1]); }
        const float wv = 0.6f * sin2pi (f.tWowPh) + 0.4f * f.tWowR;
        const float fv = 0.5f * sin2pi (f.tFlPh) + 3.0f * f.tFlN[1];
        const float wv2 = wv + wide * (0.6f * sin2pi (f.tWowPh + 0.21f) + 0.4f * f.tWowR2 - wv);
        const float fv2 = fv + wide * (0.5f * sin2pi (f.tFlPh + 0.37f) + 3.0f * f.tFlN2[1] - fv);
        if (f.rng.uni() < lam)
        {
            const float r = f.rng.uni();
            if (r < 0.5f) { f.tDropLen = (0.03f + 0.22f * f.rng.uni()) * sr; f.tDropLeft = f.tDropLen; f.tDropT = exp2f_ (-(1.0f + 4.0f * fail * f.rng.uni())); }
            else if (r < 0.75f) { f.tCrkLeft = (0.02f + 0.07f * f.rng.uni()) * sr; }
            else { f.tSnagLen = (0.05f + 0.1f * f.rng.uni()) * sr; f.tSnagT = 0.0f; f.tSnagAmp = (0.002f + 0.004f * f.rng.uni()) * sr; }
        }
        float snag = 0.0f;
        if (f.tSnagT < f.tSnagLen) { const float v = f.tSnagT / f.tSnagLen; snag = f.tSnagAmp * 0.5f * (1.0f - cos2pi (v)); f.tSnagT += 1.0f; }
        // STOP: the tape slows down to nothing, then spins back up and finds its place again
        if (stopping) { f.tStopPh = 1; f.tSpeed = maxf (0.0f, f.tSpeed - 1.0f / stopT); }
        else if (f.tStopPh == 1) { f.tSpeed = minf (1.0f, f.tSpeed + 1.0f / (0.12f * sr)); if (f.tSpeed >= 1.0f) { f.tStopPh = 2; f.tXLag = f.tStopLag; f.tXf = 1.0f; f.tStopLag = 0.0; } }
        if (f.tStopPh == 1) f.tStopLag += (double) (1.0f - f.tSpeed);
        const double lim = (double) (f.tMask - 8);
        double lagC[2];
        lagC[0] = (double) (f.tD0 + Aw * wv + Af * fv + snag);
        lagC[1] = (double) (f.tD0 + Aw * wv2 + Af * fv2 + snag + azi);
        float y[2];
        for (int c = 0; c < 2; ++c)
        {
            double lag = lagC[c]; if (lag < 3.0) lag = 3.0;
            double l1 = lag + f.tStopLag; if (l1 > lim) l1 = lim;
            float yc = readBehindD (tape[c], f.tMask, f.tW + 1, l1);
            if (f.tXf > 0.0f)
            {
                double l2 = lag + f.tXLag; if (l2 > lim) l2 = lim;
                const float yo = readBehindD (tape[c], f.tMask, f.tW + 1, l2);
                yc = yc + (yo - yc) * f.tXf;
            }
            y[c] = yc * (f.tStopPh == 1 ? sqrtf_ (f.tSpeed) : 1.0f);
        }
        if (f.tXf > 0.0f) { f.tXf = maxf (0.0f, f.tXf - 1.0f / (0.025f * sr)); if (f.tXf <= 0.0f) f.tStopPh = 0; }
        f.tW = (f.tW + 1) & f.tMask;
        // the playback: hiss under each head, the tape's bandwidth (narrowed by a crinkle), its head bump, its low
        // cut, a dropout (the level dips, then comes back), TONE
        if (f.tCrkLeft > 0.0f) { f.tCrkLeft -= 1.0f; f.tCrk += (0.12f - f.tCrk) * 0.01f; } else f.tCrk += (1.0f - f.tCrk) * 0.003f;
        if ((i & 7) == 0) for (int c = 0; c < 2; ++c) f.tLp2[c].set (minf (bw, 1200.0f + (bw - 1200.0f) * f.tCrk), 0.6f, sr);
        if (f.tDropLeft > 0.0f)
        {
            f.tDropLeft -= 1.0f;
            const float e = minf (1.0f, minf (f.tDropLen - f.tDropLeft, f.tDropLeft) / (0.004f * sr));
            f.tDrop = 1.0f + (f.tDropT - 1.0f) * e;
        }
        else f.tDrop = 1.0f;
        for (int c = 0; c < 2; ++c)
        {
            const float wn = f.rng.bi();
            float* pk = f.tPink[c];
            pk[0] = 0.997f * pk[0] + 0.029f * wn; pk[1] = 0.985f * pk[1] + 0.032f * wn; pk[2] = 0.95f * pk[2] + 0.048f * wn;
            float yc = y[c] + (pk[0] + pk[1] + pk[2] + 0.03f * wn) * hissG * 3.0f;
            float lp, bp, hp;
            f.tLp[c].tick (yc, lp, bp, hp); yc = lp;
            f.tLp2[c].tick (yc, lp, bp, hp); yc = f.tCrk > 0.999f ? yc : lp;
            f.tBump[c].tick (yc, lp, bp, hp); yc += bumpG * bp;
            f.tHpS[c] += cHp * (yc - f.tHpS[c]); yc -= f.tHpS[c];
            yc *= f.tDrop;
            f.tTlo[c] += cT * (yc - f.tTlo[c]); yc = f.tTlo[c] * glo + (yc - f.tTlo[c]) * gt;
            (c ? wR : wL)[i] = yc;
        }
        const double lagNow = lagC[0] + f.tStopLag;
        if (i == n - 1 && f.tLagPrev >= 0.0) f.tCents = 1200.0f * log2f_ (maxf (0.01f, (float) (1.0 - (lagNow - f.tLagPrev))));
        f.tLagPrev = lagNow;
    }
    for (int c = 0; c < 2; ++c) if (isBad (f.tLp[c].ic1) || isBad (f.tBump[c].ic1) || isBad (f.tLp2[c].ic1)) { fxSetup (f, M_FXTAPE); return; }
}

// ----------------------------------------------------------------- CODEC
void Engine::fxCodecFrame (FxState& f, const float* P)
{
    const float loss = P[P_B], band = P[P_C], spd = P[P_D], jit = P[P_XA], pkt = P[P_XB], reso = P[P_XD], join = P[P_XF];
    const bool inv = idxOf (P[P_XC], 2) == 1;
    const float filt = (P[P_TUNE] - 0.5f) * 2.0f;
    const bool burst = idxOf (P[P_XE], 2) == 1 && f.gest;
    const float binHz = sr / (float) FX_CFN;
    // the frames (mid, side), windowed, in order
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int k = 0; k < FX_CFN; ++k) f.cFx[k] = f.cIn[ch][(f.cRing + k) & (FX_CFN - 1)] * f.cWin[k];
        f.cfft.forward (f.cFx, f.cRe[ch], f.cIm[ch]);
    }
    // packets lost (both channels): the frame repeats the last one, or is gone (they come in runs)
    const float pLoss = burst ? 0.9f : pkt * pkt * 0.6f;
    if (f.cPktLeft <= 0 && f.rng.uni() < pLoss) { f.cPktLeft = 1 + (int) (f.rng.uni() * 6.0f); f.cPktMode = f.rng.uni() < 0.65f ? 1 : 2; }
    if (f.cPktLeft > 0)
    {
        --f.cPktLeft;
        for (int ch = 0; ch < 2; ++ch)
            if (f.cPktMode == 1) for (int k = 0; k < FX_CNB; ++k) { f.cRe[ch][k] = f.cPRe[ch][k]; f.cIm[ch][k] = f.cPIm[ch][k]; }
            else for (int k = 0; k < FX_CNB; ++k) f.cRe[ch][k] = f.cIm[ch][k] = 0.0f;
    }
    else
    {
        // the bandwidth, breathing with LOSS (one for both); FILT: a low-pass (left) or a high-pass (right), steep
        f.cBwN += (f.rng.bi() - f.cBwN) * (0.1f + 0.8f * spd);
        const float bw = band >= 0.98f ? 0.5f * sr : minf (0.5f * sr, 1000.0f * powf_ (20.0f, band) * (1.0f + loss * 0.25f * f.cBwN));
        f.cBw = bw;
        const float kc = bw / binHz;
        const float lpF = filt < 0.0f ? 20000.0f * powf_ (0.015f, -filt) : 1e9f, hpF = filt > 0.0f ? 20.0f * powf_ (200.0f, filt) : 0.0f;
        unsigned char bandOf[FX_CNB];
        for (int k = 0, b = 0; k < FX_CNB; ++k) { const float hz = (float) k * binHz; while (b < FX_CBANDS - 1 && hz >= CBE[b + 1]) ++b; bandOf[k] = (unsigned char) b; }
        // JOIN: above a frequency that comes down with it (20 kHz .. 500 Hz) the side is not sent, only how far each
        // band leans left or right (intensity stereo: the bands keep their place, lose their width)
        float lean[FX_CBANDS];
        const float kj = join > 0.002f ? 20000.0f * powf_ (0.025f, join) / binHz : 1e9f;
        if (join > 0.002f)
        {
            float eL[FX_CBANDS], eR[FX_CBANDS];
            for (int b = 0; b < FX_CBANDS; ++b) eL[b] = eR[b] = 0.0f;
            for (int k = 0; k < FX_CNB; ++k)
            {
                const float lr = f.cRe[0][k] + f.cRe[1][k], li = f.cIm[0][k] + f.cIm[1][k], rr = f.cRe[0][k] - f.cRe[1][k], ri = f.cIm[0][k] - f.cIm[1][k];
                eL[bandOf[k]] += lr * lr + li * li; eR[bandOf[k]] += rr * rr + ri * ri;
            }
            for (int b = 0; b < FX_CBANDS; ++b) { const float a = sqrtf_ (eL[b]), c = sqrtf_ (eR[b]); lean[b] = (a - c) / (a + c + 1e-12f); }
        }
        // joint stereo: the mid, then the side, each coded on its own; the side's bands measured against the mid's
        // loudest (a side quieter than the mid goes first), with JOIN more of its bits gone too
        float emaxM = 1e-20f;
        for (int ch = 0; ch < 2; ++ch)
        {
            float* Re = f.cRe[ch];
            float* Im = f.cIm[ch];
            const float lc = ch == 0 ? loss : minf (1.0f, loss + join * (1.0f - loss));
            // the bands: under the masking threshold (from the loudest band, closer with LOSS) they go; near it, they
            // flicker (SPED: slowly, smeared, or every frame, garbled)
            float E[FX_CBANDS];
            for (int b = 0; b < FX_CBANDS; ++b) E[b] = 0.0f;
            for (int k = 0; k < FX_CNB; ++k) E[bandOf[k]] += Re[k] * Re[k] + Im[k] * Im[k];
            if (ch == 0) for (int b = 0; b < FX_CBANDS; ++b) emaxM = maxf (emaxM, E[b]);
            const float range = 66.0f - 58.0f * lc;
            int kept = 0;
            float* keep = f.cKeep[ch];
            float* off = f.cOff[ch];
            for (int b = 0; b < FX_CBANDS; ++b)
            {
                off[b] += (f.rng.bi() - off[b]) * (0.05f + 0.9f * spd);
                const float db = 10.0f * 0.30103f * log2f_ (E[b] / emaxM + 1e-20f);
                const float tgt = lc < 0.01f || db + lc * 8.0f * off[b] > -range ? 1.0f : 0.0f;
                keep[b] += (tgt - keep[b]) * (0.25f + 0.75f * spd);
                if (keep[b] > 0.5f) ++kept;
            }
            if (ch == 0) f.cKept = (float) kept / (float) FX_CBANDS;
            const float step = 1.0f + lc * 9.0f;           // the magnitudes on a coarse grid (dB)
            const float jitA = jit * 3.14159f;
            double eIn = 0.0, eOut = 0.0;
            for (int k = 0; k < FX_CNB; ++k)
            {
                float re = Re[k], im = Im[k];
                const float hz = (float) k * binHz;
                // what the codec keeps: its bands over the threshold, under its bandwidth (a 2-bin edge); INV: the rest
                float gs = keep[bandOf[k]];
                if ((float) k > kc) gs *= maxf (0.0f, 1.0f - ((float) k - kc) * 0.5f);
                float gn = inv ? 1.0f - gs : gs;
                if (hz > lpF) { const float r = lpF / hz; gn *= r * r * r * r; }
                if (hz < hpF && hz > 0.0f) { const float r = hz / hpF; gn *= r * r * r * r; }
                float mg = sqrtf_ (re * re + im * im);
                if (mg > 1e-9f && gn > 0.0f)
                {
                    if (lc > 0.02f) { float mdb = 20.0f * 0.30103f * log2f_ (mg); mdb = floorf_ (mdb / step + 0.5f) * step; mg = exp2f_ (mdb * 0.16609640f); }
                    eIn += (double) (mg * mg * gn * gn);
                    if (reso > 0.002f) mg = powf_ (mg * 64.0f, 1.0f + reso * 0.8f) * (1.0f / 64.0f);
                    float c = re, sn = im;
                    if (jitA > 0.0005f)
                    {
                        const float a = jitA * f.rng.bi(), ca = cos2pi (a * 0.15915494f), sa = sin2pi (a * 0.15915494f);
                        c = re * ca - im * sa; sn = re * sa + im * ca;
                    }
                    const float nrm = mg / maxf (sqrtf_ (c * c + sn * sn), 1e-12f) * gn;
                    re = c * nrm; im = sn * nrm;
                }
                else re = im = 0.0f;
                eOut += (double) (re * re + im * im);
                Re[k] = re; Im[k] = im;
            }
            // RESO sharpens the spectrum (its peaks out of the noise): the energy it moved is given back
            if (reso > 0.002f && eOut > 1e-20)
            {
                const float gK = minf (4.0f, sqrtf_ ((float) (eIn / eOut)));
                for (int k = 0; k < FX_CNB; ++k) { Re[k] *= gK; Im[k] *= gK; }
            }
        }
        // the side over JOIN's frequency: the mid as it was coded, leaning as the band did
        if (join > 0.002f)
            for (int k = 0; k < FX_CNB; ++k)
                if ((float) k > kj) { const float l = lean[bandOf[k]]; f.cRe[1][k] = f.cRe[0][k] * l; f.cIm[1][k] = f.cIm[0][k] * l; }
        for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < FX_CNB; ++k) { f.cPRe[ch][k] = f.cRe[ch][k]; f.cPIm[ch][k] = f.cIm[ch][k]; }
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        f.cfft.inverse (f.cRe[ch], f.cIm[ch], f.cFx);
        // overlap-add (sqrt-Hann both ways, a quarter hop: the windows sum to 2)
        float* ola = f.cOla[ch];
        for (int k = 0; k < FX_CFN; ++k) ola[(f.cRing + k) & (FX_CFN - 1)] += f.cFx[k] * f.cWin[k] * 0.5f;
    }
}

void Engine::fxCodec (FxState& f, const float* P, const float* xL, const float* xR, float* wL, float* wR, float* dL, float* dR, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const int r = f.cRing;
        const float m = f.cOla[0][r], sd = f.cOla[1][r];
        wL[i] = m + sd; wR[i] = m - sd; f.cOla[0][r] = f.cOla[1][r] = 0.0f;
        dL[i] = f.cDry[0][r]; dR[i] = f.cDry[1][r]; f.cDry[0][r] = xL[i]; f.cDry[1][r] = xR[i];
        f.cIn[0][r] = 0.5f * (xL[i] + xR[i]); f.cIn[1][r] = 0.5f * (xL[i] - xR[i]);
        f.cRing = (r + 1) & (FX_CFN - 1);
        if (++f.cHop >= FX_CHOP) { f.cHop = 0; fxCodecFrame (f, P); }
    }
}

// ----------------------------------------------------------------- the track
void Engine::fxRun (Track& t, int ti, const float* P, int n, double pos16, double inc)
{
    FxState& f = fx[ti];
    const int m = machineOf (P[P_MACH]);
    if (f.kind != m || f.sr != sr) fxSetup (f, m);
    // the source, in stereo as it would have reached the mix (its tracks' pans and sides)
    float xL[CHUNK], xR[CHUNK];
    for (int i = 0; i < n; ++i) xL[i] = xR[i] = 0.0f;
    const u32 mask = f.srcMask;
    for (int k = 0; k < NUM_TRACKS; ++k) if (mask & (1u << k)) for (int i = 0; i < n; ++i) { xL[i] += trkL[k][i]; xR[i] += trkR[k][i]; }
    // a trig: IN opens the effect (its velocity sets how far); a gesture starts (HOLD: a trig starts it, the next one stops it)
    const int act = idxOf (P[P_XE], fxActs (m));
    const bool hold = P[P_DEC] >= 0.99f;
    const float decS = 0.02f * powf_ (300.0f, P[P_DEC]);
    const bool trigNow = f.trig != 0;
    if (f.trig)
    {
        f.trig = 0;
        if (act == 0) { f.gPeak = maxf (0.05f, f.trigVel); f.gEnv = 1.0f; }
        else if (hold) { f.gest = ! f.gest; f.gestLeft = -1.0; }
        else { f.gest = 1; f.gestLeft = (double) (decS * sr); }
    }
    if (act == 0) f.gest = 0;
    if (f.gest && f.gestLeft >= 0.0) { f.gestLeft -= (double) n; if (f.gestLeft <= 0.0) f.gest = 0; }
    f.gestF += ((f.gest ? 1.0f : 0.0f) - f.gestF) * (1.0f - expf_ (-(float) n / (0.008f * sr)));
    float g[CHUNK];
    {
        const float atk = 1.0f / (0.003f * sr), dc = decayCoef (decS, sr), gk = 1.0f - expf_ (-1.0f / (0.0015f * sr));
        for (int i = 0; i < n; ++i)
        {
            float tg;
            if (act != 0 || hold) tg = 1.0f;
            else
            {
                // AD: up in 3 ms to the trig's level, down over DEC
                if (f.gEnv > 0.5f) { f.gate += atk; if (f.gate >= f.gPeak) { f.gate = f.gPeak; f.gEnv = 0.0f; } tg = f.gate; }
                else { f.gate *= dc; tg = f.gate; }
                g[i] = tg; continue;
            }
            f.gate += (tg - f.gate) * gk;
            g[i] = f.gate;
        }
    }
    // the machine (left and right)
    float wl[CHUNK], wr[CHUNK], dl[CHUNK], dr[CHUNK];
    for (int i = 0; i < n; ++i) { dl[i] = xL[i]; dr[i] = xR[i]; }
    const double spt = 60.0 / (double) bpm / 4.0;          // seconds a 16th
    bool sendType = false;
    switch (m)
    {
        case M_FXVERB:  fxVerb (f, P, xL, xR, g, wl, wr, n); sendType = true; break;
        case M_FXECHO:  fxEcho (f, P, xL, xR, g, wl, wr, n, spt); sendType = true; break;
        case M_FXBEND:  fxBend (f, P, xL, xR, wl, wr, n, pos16, inc, spt); break;
        case M_FXTAPE:  fxTape (f, P, xL, xR, wl, wr, n); break;
        case M_FXFUZZ:  fxFuzz (f, P, xL, xR, g, wl, wr, n); for (int i = 0; i < n; ++i) { dl[i] = wl[i]; dr[i] = wr[i]; } break;
        case M_FXWEFT:
        {
            // the carrier: a track (mono), or the internal oscillator; the modulator: the source (mono); the direct
            // sound as late as the effect
            const int cs = idxOf (P[P_D], 11);
            float car[CHUNK], md[CHUNK];
            for (int i = 0; i < n; ++i) { car[i] = 0.0f; md[i] = 0.5f * (xL[i] + xR[i]); }
            for (int k = 0; k < NUM_TRACKS; ++k) if (f.carMask & (1u << k)) for (int i = 0; i < n; ++i) car[i] += 0.5f * (trkL[k][i] + trkR[k][i]);
            WeftParams wp;
            wp.algo = P[P_B]; wp.timb = P[P_C]; wp.lvl1 = P[P_XA]; wp.lvl2 = P[P_XB];
            wp.note = 48.0f + (P[P_TUNE] - 0.5f) * 48.0f;
            wp.carrier = cs < 3 ? cs + 1 : 0;
            wp.shift = idxOf (P[P_XC], 2);
            wp.freeze = act == 1 && f.gestF > 0.5f;
            weftProcess (f.mem, wp, car, md, xL, xR, wl, wr, dl, dr, n);
            break;
        }
        case M_FXSTENCIL:
        {
            // v83: ZONE RATE REPT on MAIN, COLR on TUNE; µ HALO SKEW SYNC ACT PING NOIS on MORE; SPACE on the side places
            StencilParams sp;
            sp.zone = idxOf (P[P_B], STC_ZONES); sp.rate = P[P_C]; sp.rept = P[P_D]; sp.color = (P[P_TUNE] - 0.5f) * 2.0f;
            sp.mu = P[P_XA]; sp.halo = P[P_XB]; sp.skew = (P[P_XC] - 0.5f) * 2.0f; sp.sync = idxOf (P[P_XD], 2);
            sp.ping = P[P_XF]; sp.nois = P[P_XG];
            sp.hSize = P[P_S1]; sp.hDcy = P[P_S2]; sp.hDamp = P[P_S3]; sp.hDrift = P[P_S4]; sp.muSpd = P[P_S5]; sp.duck = P[P_S6]; sp.wide = P[P_S7];
            sp.hold = act == 1 ? f.gestF : 0.0f; sp.flip = act == 2 ? f.gestF : 0.0f;
            sp.pluck = act == 3 && trigNow; sp.pluckVel = f.trigVel; sp.note = f.trigNote;
            sp.spt = spt;
            if (f.kind == M_FXSTENCIL) stencilProcess (f.mem, sp, xL, xR, g, wl, wr, n);
            else for (int i = 0; i < n; ++i) wl[i] = wr[i] = 0.0f;
            sendType = true;
            break;
        }
        default:        fxCodec (f, P, xL, xR, wl, wr, dl, dr, n); break;
    }
    // the mix: VERB and ECHO take the source in through the gate (above) and give back direct + effect; the others
    // fade from the direct sound to direct + effect with the gate
    const float mixT = P[P_E], volT = t.mute ? 0.0f : P[P_VOL] * 1.25f;
    const float km = 1.0f - expf_ (-1.0f / (0.004f * sr));
    float* bus = t.bus;
    float pin = 0.0f, pout = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        f.mixS += km * (mixT - f.mixS); f.volS += km * (volT - f.volS);
        const float mx = f.mixS;
        float yl, yr;
        if (sendType)
        {
            // equal power (the effect's tail is another sound than the direct one: their powers add)
            const float dg = mx <= 1e-6f ? 1.0f : mx >= 1.0f - 1e-6f ? 0.0f : sin2pi (0.25f * (1.0f - mx));
            const float wg = mx <= 1e-6f ? 0.0f : mx >= 1.0f - 1e-6f ? 1.0f : sin2pi (0.25f * mx);
            yl = dl[i] * dg + wl[i] * wg; yr = dr[i] * dg + wr[i] * wg;
        }
        else
        {
            yl = dl[i] + g[i] * mx * (wl[i] - dl[i]); yr = dr[i] + g[i] * mx * (wr[i] - dr[i]);
        }
        // out as the track's mid (its bus: TONE, pan) and side
        float y = 0.5f * (yl + yr) * f.volS, s = 0.5f * (yl - yr) * f.volS;
        if (isBad (y)) y = 0.0f;
        if (isBad (s)) s = 0.0f;
        bus[i] += y;
        pin = maxf (pin, maxf (absf (xL[i]), absf (xR[i]))); pout = maxf (pout, maxf (absf (yl), absf (yr)) * f.volS);
        f.side[i] = s;
    }
    f.inPk = maxf (f.inPk, pin); f.outPk = maxf (f.outPk, pout);
}

// the screen: the machine, what it takes, the gate, the gesture, the levels, its readouts
void Engine::fxInfo (int ti, float* out)
{
    for (int k = 0; k < FX_INFO; ++k) out[k] = 0.0f;
    if (ti < 0 || ti >= NUM_TRACKS) return;
    const Track& t = tr[ti];
    FxState& f = fx[ti];
    const int m = fxMachineOf (t);
    out[0] = fxMachine (m) ? (float) m : -1.0f;
    if (! fxMachine (m)) return;
    out[1] = (float) f.srcMask;
    out[2] = f.gate; out[3] = f.gestF;
    out[4] = f.inPk; out[5] = f.outPk; f.inPk = f.outPk = 0.0f;
    if (f.kind != m) return;
    switch (m)
    {
        case M_FXVERB:  out[6] = f.vRt; out[7] = f.vSize; out[8] = f.vFrz; out[9] = f.vPreSm / sr * 1000.0f; break;
        case M_FXECHO:  out[6] = f.eTime; out[7] = f.eFb; out[8] = f.eHold; out[9] = f.eFlip; break;
        case M_FXBEND:  out[6] = (float) f.bEv; out[7] = (float) f.bRep; out[8] = f.bHoldF; out[9] = f.bCur.live ? 1.0f : (float) f.bCur.rate; out[10] = (float) f.bDivN; break;
        case M_FXTAPE:  out[6] = f.tCents; out[7] = f.tDrop; out[8] = f.tSpeed; out[9] = f.tCrk; break;
        case M_FXWEFT:  weftInfo (f.mem, out + 6); break;
        case M_FXSTENCIL: stencilInfo (f.mem, out + 6); break;
        case M_FXFUZZ:  out[6] = f.zPk[0]; out[7] = f.zPk[1]; out[8] = f.zPk[2]; out[9] = f.zGGain; out[10] = f.zDrift; out[11] = f.zBias; out[12] = f.zSubPk;
                        f.zPk[0] = f.zPk[1] = f.zPk[2] = 0.0f; f.zSubPk = 0.0f; break;
        default:        out[6] = f.cBw; out[7] = f.cKept; out[8] = (float) f.cPktLeft; break;
    }
}

} // namespace sk
