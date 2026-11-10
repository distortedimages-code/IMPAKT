// IMPAKT — STENCIL (v83): see sk_stencil.h
#include "sk_stencil.h"
#include "sk_fx.h"

namespace sk {

namespace {

// the zones (seconds: RATE goes from the longest, at its left, to the shortest)
const float SZ_LO[STC_ZONES] = { 0.0003f, 0.004f, 0.03f, 0.12f, 0.35f, 0.8f, 1.5f, 2.5f };
const float SZ_HI[STC_ZONES] = { 0.012f, 0.045f, 0.22f, 0.6f, 1.2f, 2.0f, 3.5f, 5.4f };
// SYNC: the tempo's divisions (16ths: 1/64 .. 4 bars, triplets and dotted), SKEW's ratios
const float SDIV[STC_NDIV] = { 0.25f, 0.5f, 2.0f / 3.0f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f, 8.0f / 3.0f, 3.0f, 4.0f, 16.0f / 3.0f, 6.0f, 8.0f, 12.0f, 16.0f, 24.0f, 32.0f, 64.0f };
const float SSKEW[STC_NSKEW] = { 0.5f, 2.0f / 3.0f, 0.75f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f };
// HALO: the network's lines and allpasses at SIZE 1 (ms; VERB's: primes, the even ones left, the odd ones right)
const float HMS[STC_HL]  = { 151.0f, 163.0f, 181.0f, 193.0f, 211.0f, 227.0f, 241.0f, 257.0f };
const float HAMS[STC_HL] = { 4.3f, 5.3f, 6.1f, 7.3f, 8.9f, 10.1f, 11.3f, 13.7f };
const float HIN[STC_HL]  = { 1, 1, -1, 1, 1, -1, -1, -1 };
const float HOS[STC_HL]  = { 1, 1, 1, -1, -1, 1, -1, -1 };
constexpr int SLINE = 1 << 18;
constexpr float STC_CEIL = 0.38f;              // (v84: the loop's ceiling, where its saturation sits for a quiet source or none)

inline int nextPow2 (int x) { int p = 1; while (p < x) p <<= 1; return p; }
inline float hermR (const float* b, int mask, int i, float f)
{
    const float xm1 = b[(i - 1) & mask], x0 = b[i & mask], x1 = b[(i + 1) & mask], x2 = b[(i + 2) & mask];
    const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}
// d samples behind the write index w (d >= 3)
inline float readBehind (const float* b, int mask, int w, float d)
{
    const int di = (int) d; const float fd = d - (float) di;
    return hermR (b, mask, w - di - 1, 1.0f - fd);
}
inline float readBehindL (const float* b, int mask, int w, float d)
{
    const int di = (int) d; const float fd = d - (float) di, a = b[(w - di) & mask], c = b[(w - di - 1) & mask];
    return a + (c - a) * fd;
}
// (v84) the repeats going out held under 1.3 times the loop's ceiling: exactly as they are under it, a soft knee
// over it (HALO's network can give back more than the loop holds)
inline float knee (float x, float c)
{
    const float a = absf (x);
    if (a <= c) return x;
    const float k = 0.3f * c, y = c + k * tanhf_ ((a - c) / k);
    return x < 0.0f ? -y : y;
}
inline float onePole (float hz, float sr) { return 1.0f - expf_ (-TWO_PI * minf (hz, 0.45f * sr) / sr); }
inline void had8 (float* a)
{
    for (int h = 1; h < 8; h <<= 1)
        for (int i = 0; i < 8; i += h << 1)
            for (int j = i; j < i + h; ++j) { const float x = a[j], y = a[j + h]; a[j] = x + y; a[j + h] = x - y; }
    for (int i = 0; i < 8; ++i) a[i] *= 0.35355339f;
}

// the state, at the start of the track's memory (floats and ints only: the memory is a float array), then the lines
struct StState
{
    float sr;
    int   ln, mask, w;                     // the two lines (2^18 each), where they are written
    float dL, dR;                          // the lengths now (samples, glided)
    float muPh, muR, muRT;                 // µ: its sine, its random walk
    Svf   lpL, lpR, bpL, bpR;              // COLR: the low-pass, the head bump
    float hpL, hpR;                        // COLR: the high-pass
    float lpHz, bumpHz;                    // (the filters' settings in use)
    // HALO: its lines (8 x hl), allpasses (8 x ha), where they are written, their lengths now, the branches' filters
    int   h0, hl, hmask, a0, ha, amask, hw;
    float hD[STC_HL], hA[STC_HL], hLp[STC_HL], hHp[STC_HL], hPh;
    float hPk;                             // (its level, for the screen)
    // FLIP: two grains' ages (-1: none), the newer one
    float ga[2]; int gn;
    float duckE;
    // v84, the loop's regulator: the loop's level (what goes out and round again), the source's, the gain it leaves
    float rgIn, rgC;
    // PLUK: the burst going in (samples left, its length), its colour
    int   bLeft, bLen; float bAmp, bLpL, bLpR;
    float nzL[2], nzR[2];                  // NOIS: pink-ish (two poles a side)
    Rng   rng;
    float tL, tR, fb, loopPk, holdS, flipS;// (the screen's)
};

inline StState& st (float* mem) { return *reinterpret_cast<StState*> (mem); }
inline const StState& st (const float* mem) { return *reinterpret_cast<const StState*> (mem); }
constexpr int HDR = ((int) sizeof (StState) + 63) / 64 * 16;          // (floats, the lines start 64-byte aligned)

} // namespace

// atan2 (~1e-5 rad), and COLR's phase lag (rad) at f (cycles a sample) through the loop's filters: the low-pass and
// the head bump, and the high-pass as far as the fundamental is what is heard (dark, or few harmonics under the
// low-pass: hpW; with many, their period is the pitch and the high-pass, which only moves the lowest, is left out).
// Taken out of KARP's and COMB's lines, the loop plays in tune whatever its colour (within about 10 cents)
inline float stcAtan (float x)
{
    const float x2 = x * x;
    return x * (0.99997726f + x2 * (-0.33262347f + x2 * (0.19354346f + x2 * (-0.11643287f + x2 * (0.05265332f + x2 * -0.01172120f)))));
}
inline float stcAtan2 (float y, float x)
{
    const float ax = absf (x), ay = absf (y);
    if (ax < 1e-30f && ay < 1e-30f) return 0.0f;
    float a = ax >= ay ? stcAtan (ay / ax) : 1.5707963f - stcAtan (ax / ay);
    if (x < 0.0f) a = PI - a;
    return y < 0.0f ? -a : a;
}
static float colourLag (float f, float lpHz, float bump, float aHp, float hpW, float sr)
{
    f = clampf (f, 1e-6f, 0.49f);
    const float W = tanpi (f), g = tanpi (clampf (lpHz / sr, 0.00001f, 0.49f)), k = 1.0f / 0.6f;
    float arg = -stcAtan2 (k * g * W, g * g - W * W);                                   // (the SVF's low-pass)
    if (bump > 0.0f)
    {
        const float gb = tanpi (110.0f / sr), kb = 1.0f / 1.2f, dr = gb * gb - W * W, di = kb * gb * W, m = dr * dr + di * di + 1e-30f;
        arg += stcAtan2 (bump * gb * W * dr / m, 1.0f + bump * gb * W * di / m);     // (1 + bump x its band-pass)
    }
    const float sn = sin2pi (f), cs = cos2pi (f), q = 1.0f - aHp;
    if (hpW > 0.0f) arg += hpW * (stcAtan2 (sn, 1.0f - cs) - stcAtan2 (q * sn, 1.0f - q * cs));   // (x minus its one-pole)
    return -arg;
}

unsigned stencilBytes() { return (unsigned) (HDR + 2 * SLINE) * 4u; }

float stencilTime (int zone, float rate, int sync, double spt, float note)
{
    zone = zone < 0 ? 0 : (zone >= STC_ZONES ? STC_ZONES - 1 : zone);
    float T = SZ_HI[zone] * powf_ (SZ_LO[zone] / SZ_HI[zone], clampf (rate, 0.0f, 1.0f));
    if (zone <= 1)
    {
        // KARP, COMB: a pitch; SYNC on its semitone; a trig's NOTE transposes it
        if (sync)
        {
            const float m = 69.0f + 12.0f * log2f_ (1.0f / (T * 440.0f));
            const float mr = floorf_ (m + 0.5f);
            T = 1.0f / (440.0f * exp2f_ ((mr - 69.0f) * (1.0f / 12.0f)));
        }
        T *= exp2f_ (-note * (1.0f / 12.0f));
        return clampf (T, 0.0001f, 0.2f);
    }
    if (sync && spt > 0.0)
    {
        const float t16 = T / (float) spt;
        int best = 0; float bd = 1e9f;
        for (int k = 0; k < STC_NDIV; ++k)
        {
            if (SDIV[k] * (float) spt > 5.4f) break;
            const float d = absf (log2f_ (SDIV[k] / t16)); if (d < bd) { bd = d; best = k; }
        }
        T = SDIV[best] * (float) spt;
    }
    return T;
}

float stencilSkew (float skew, int sync)
{
    skew = clampf (skew, -1.0f, 1.0f);
    if (sync) { const int k = (int) floorf_ ((skew + 1.0f) * 0.5f * (float) STC_NSKEW); return SSKEW[k < 0 ? 0 : (k >= STC_NSKEW ? STC_NSKEW - 1 : k)]; }
    return exp2f_ (0.585f * skew);                // (x 0.67 .. x 1.5)
}

void stencilInit (float* mem, float sr)
{
    StState& s = st (mem);
    for (int i = 0; i < HDR; ++i) mem[i] = 0.0f;
    s.sr = sr;
    s.ln = SLINE; s.mask = SLINE - 1; s.w = 0;
    s.dL = s.dR = -1.0f;
    s.muPh = 0.0f; s.muR = s.muRT = 0.0f;
    s.lpL.reset(); s.lpR.reset(); s.bpL.reset(); s.bpR.reset(); s.hpL = s.hpR = 0.0f; s.lpHz = s.bumpHz = -1.0f;
    // HALO: its lines (half a second at SIZE 1, with their drift) and allpasses after the two lines, all in the memory
    int hl = nextPow2 ((int) (0.5f * sr) + 8);
    const int ha = nextPow2 ((int) (0.03f * sr) + 8);
    while (HDR + 2 * SLINE + STC_HL * (hl + ha) > (int) FX_MEM && hl > 2048) hl >>= 1;
    s.h0 = HDR + 2 * SLINE; s.hl = hl; s.hmask = hl - 1;
    s.a0 = s.h0 + STC_HL * hl; s.ha = ha; s.amask = ha - 1; s.hw = 0;
    for (int j = 0; j < STC_HL; ++j) { s.hD[j] = s.hA[j] = -1.0f; s.hLp[j] = s.hHp[j] = 0.0f; }
    s.hPh = 0.0f; s.hPk = 0.0f;
    s.ga[0] = s.ga[1] = -1.0f; s.gn = 0; s.duckE = 0.0f;
    s.rgIn = 0.0f; s.rgC = STC_CEIL;
    s.bLeft = 0; s.bLen = 1; s.bAmp = 0.0f; s.bLpL = s.bLpR = 0.0f;
    s.nzL[0] = s.nzL[1] = s.nzR[0] = s.nzR[1] = 0.0f;
    s.rng.seed (0x5713C11u);
    s.tL = s.tR = s.fb = s.loopPk = s.holdS = s.flipS = 0.0f;
    const int used = s.a0 + STC_HL * ha;
    for (int i = HDR; i < used; ++i) mem[i] = 0.0f;
}

void stencilProcess (float* mem, const StencilParams& p, const float* xL, const float* xR, const float* g, float* wL, float* wR, int n)
{
    StState& s = st (mem);
    const float sr = s.sr;
    float* lineL = mem + HDR;
    float* lineR = lineL + s.ln;
    const int zone = p.zone < 0 ? 0 : (p.zone >= STC_ZONES ? STC_ZONES - 1 : p.zone);
    const bool shortZ = zone <= 1;
    // the times: the left line's, the right's against it (SKEW), glided like a tape (its pitch bends)
    const float maxD = (float) (s.ln - 16);
    const float T = stencilTime (zone, p.rate, p.sync, p.spt, p.note);
    const float tL0 = clampf (T * sr, 3.0f, maxD), tR0 = clampf (T * stencilSkew (p.skew, p.sync) * sr, 3.0f, maxD);
    const float kG = 1.0f - expf_ (-1.0f / ((shortZ ? 0.03f : zone <= 3 ? 0.18f : 0.3f) * sr));
    // µ: a sine and a random walk (in KARP and COMB a sweeping flanger, then a tape's wow and chorus)
    const float muHz = 0.08f * powf_ (80.0f, p.muSpd), muInc = muHz / sr, kMu = onePole (muHz * 1.3f, sr);
    const float muFrac = p.mu * p.mu * (shortZ ? 0.35f : 0.02f);
    // COLR: dark (a low-pass down to 840 Hz, a tape's saturation, its head bump) .. bright (thin: the low end out, clean)
    const float c = clampf (p.color, -1.0f, 1.0f), dk = maxf (0.0f, -c), br = maxf (0.0f, c);
    const float lpHz = c < 0.0f ? 14000.0f * powf_ (0.06f, dk) : 14000.0f + 6000.0f * br;
    const float hpHz = 25.0f * powf_ (24.0f, br);
    if (absf (lpHz - s.lpHz) > 0.5f) { s.lpHz = lpHz; s.lpL.set (lpHz, 0.6f, sr); s.lpR.set (lpHz, 0.6f, sr); }
    if (s.bumpHz < 0.0f) { s.bumpHz = 110.0f; s.bpL.set (110.0f, 1.2f, sr); s.bpR.set (110.0f, 1.2f, sr); }
    const float cHp = onePole (hpHz, sr), bump = 0.5f * dk;
    const float drv = 1.0f + 3.0f * dk, idrv = 1.0f / drv, bias = 0.12f * dk, tb = tanhf_ (bias);
    // HALO: the network (SIZE: 3 .. 257 ms lines; DCAY: its gain a pass; DAMP: its absorption; DRFT: its lines'
    // wander); its output kept at the level that goes in (a smear, not a boost)
    const float hScale = 0.02f * powf_ (50.0f, p.hSize), haScale = powf_ (hScale, 0.7f);
    const float hLim = (float) (s.hl - 8), aLim = (float) (s.ha - 4);
    float hT[STC_HL], aT[STC_HL];
    for (int j = 0; j < STC_HL; ++j)
    {
        hT[j] = clampf (HMS[j] * 0.001f * sr * hScale, 3.0f, hLim * 0.45f); if (s.hD[j] < 0.0f) s.hD[j] = hT[j];
        aT[j] = clampf (HAMS[j] * 0.001f * sr * haScale, 1.5f, aLim); if (s.hA[j] < 0.0f) s.hA[j] = aT[j];
    }
    const bool haloOn = p.halo > 0.0005f;
    // (a pass through it about 1 dB under what went in, whatever SIZE and DCAY: measured on a click, fitted; HALO in
    // the loop leaves REPT what it says)
    const float gH = 0.3f + 0.68f * p.hDcy;
    const float hNorm = 0.5f * sqrtf_ (1.0f - gH * gH) * powf_ (10.0f, (2.5f + 3.5f * p.hSize + (2.4f - 2.0f * p.hSize) * p.hDcy * p.hDcy) * 0.05f);
    const float cD = onePole (18000.0f * powf_ (0.03f, p.hDamp), sr), cH = onePole (4.0f, sr);
    const float hInc = 0.31f / sr, hDep = p.hDrift * p.hDrift * 0.02f, kS = 1.0f - expf_ (-1.0f / (0.05f * sr));
    const float apG = 0.55f;
    // the loop: REPT (0 .. 125 %), HOLD (unity, untouched: no input, no colour, no halo), PING, NOIS (the loop's hiss)
    const float fb = p.rept * 1.25f, hold = clampf (p.hold, 0.0f, 1.0f), open = 1.0f - hold;
    const float fbG = fb + (1.0f - fb) * hold;
    // KARP, COMB: the lines shortened by what COLR's filters delay their pitch (in tune whatever the colour)
    float tL = tL0, tR = tR0;
    if (shortZ)
    {
        const float wL = clampf (dk + (12.0f - lpHz * tL0 / sr) * 0.125f, 0.0f, 1.0f), wR = clampf (dk + (12.0f - lpHz * tR0 / sr) * 0.125f, 0.0f, 1.0f);
        tL = maxf (3.0f, tL0 - open * colourLag (1.0f / tL0, lpHz, bump, cHp, wL, sr) * tL0 * (1.0f / TWO_PI));
        tR = maxf (3.0f, tR0 - open * colourLag (1.0f / tR0, lpHz, bump, cHp, wR, sr) * tR0 * (1.0f / TWO_PI));
    }
    if (s.dL < 0.0f) { s.dL = tL; s.dR = tR; }
    const float nzA = 0.02f * p.nois * p.nois * open;
    const float ping = p.ping;
    // DUCK (5 ms down, 200 ms back up), WIDE (the repeats' side: none .. twice)
    const float kDa = 1.0f - expf_ (-1.0f / (0.005f * sr)), kDr = 1.0f - expf_ (-1.0f / (0.2f * sr));
    const float wide = 2.0f * p.wide;
    // (the ceiling's times: the source's peaks caught at once, let go in a second; the ceiling down again in 1.5 s)
    const float rgA = 1.0f - expf_ (-1.0f / (0.001f * sr)), rgIr = 1.0f - expf_ (-1.0f / (1.0f * sr)), rgCr = 1.0f - expf_ (-1.0f / (1.5f * sr));
    // PLUK: a trig plucks the loop, a burst a period long (in KARP a string; longer zones, a tap: 4 ms)
    if (p.pluck)
    {
        s.bLen = maxi (8, (int) (shortZ ? minf (tL, 0.02f * sr) : 0.004f * sr));
        s.bLeft = s.bLen; s.bAmp = 0.7f * clampf (p.pluckVel, 0.05f, 1.0f);
    }
    const float bC = onePole (2500.0f * powf_ (6.0f, c + 0.3f), sr);
    s.tL = s.dL * (tL0 / tL) / sr * 1000.0f; s.tR = s.dR * (tR0 / tR) / sr * 1000.0f; s.fb = fbG; s.holdS = hold; s.flipS = p.flip;
    float loopPk = 0.0f, hPk = 0.0f;
    const float* hMemL = mem + s.h0;
    for (int i = 0; i < n; ++i)
    {
        s.dL += kG * (tL - s.dL); s.dR += kG * (tR - s.dR);
        s.muPh += muInc; if (s.muPh >= 1.0f) { s.muPh -= 1.0f; s.muRT = s.rng.bi(); }
        s.muR += kMu * (s.muRT - s.muR);
        const float m1 = 0.65f * sin2pi (s.muPh) + 0.35f * s.muR, m2 = 0.65f * sin2pi (s.muPh + 0.25f) + 0.35f * s.muR;
        const float dl = clampf (s.dL * (1.0f + muFrac * m1), 3.0f, maxD), dr = clampf (s.dR * (1.0f + muFrac * m2), 3.0f, maxD);
        float yL = readBehind (lineL, s.mask, s.w, dl), yR = readBehind (lineR, s.mask, s.w, dr);
        // FLIP: two grains overlapping by half, each reading the recent past backwards
        if (p.flip > 0.0005f)
        {
            const float G = clampf (s.dL, 0.03f * sr, 1.5f * sr);
            const int cg = s.gn;
            if (s.ga[cg] < 0.0f) s.ga[cg] = 0.0f;
            else if (s.ga[cg] >= 0.5f * G && s.ga[cg ^ 1] < 0.0f) { s.gn = cg ^ 1; s.ga[s.gn] = 0.0f; }
            float rl = 0.0f, rr = 0.0f;
            for (int k = 0; k < 2; ++k)
            {
                if (s.ga[k] < 0.0f) continue;
                if (s.ga[k] >= G) { s.ga[k] = -1.0f; continue; }
                const float wv = sin2pi (0.5f * s.ga[k] / G), w2 = wv * wv, dd = minf (2.0f * s.ga[k] + 4.0f, maxD);
                rl += w2 * readBehind (lineL, s.mask, s.w, dd); rr += w2 * readBehind (lineR, s.mask, s.w, dd);
                s.ga[k] += 1.0f;
            }
            yL += (rl - yL) * p.flip; yR += (rr - yR) * p.flip;
        }
        // COLR: the low-pass, the head bump, the high-pass (out of the way while it holds)
        float lp, bp, hp;
        s.lpL.tick (yL, lp, bp, hp); float cl = lp; s.bpL.tick (cl, lp, bp, hp); cl += bump * bp;
        s.hpL += cHp * (cl - s.hpL); cl -= s.hpL;
        s.lpR.tick (yR, lp, bp, hp); float cr = lp; s.bpR.tick (cr, lp, bp, hp); cr += bump * bp;
        s.hpR += cHp * (cr - s.hpR); cr -= s.hpR;
        cl = yL + (cl - yL) * open; cr = yR + (cr - yR) * open;
        // HALO: the repeats into the network (left into the even lines, right into the odd), back out as a smear
        float hl = cl, hr = cr;
        if (haloOn)
        {
            s.hPh += hInc; if (s.hPh >= 1.0f) s.hPh -= 1.0f;
            float y[STC_HL], z[STC_HL];
            for (int j = 0; j < STC_HL; ++j)
            {
                s.hD[j] += kS * (hT[j] - s.hD[j]); s.hA[j] += kS * (aT[j] - s.hA[j]);
                const float* lj = hMemL + j * s.hl;
                const float D = clampf (s.hD[j] * (1.0f + hDep * sin2pi (s.hPh + 0.125f * (float) j)), 3.0f, hLim);
                const float yj = readBehindL (lj, s.hmask, s.hw, D);
                y[j] = yj;
                s.hLp[j] += cD * (yj - s.hLp[j]);
                float zj = s.hLp[j];
                s.hHp[j] += cH * (zj - s.hHp[j]); zj -= s.hHp[j];
                z[j] = zj * gH;
            }
            had8 (z);
            const float il = cl * 0.5f, ir = cr * 0.5f, den = (i & 1) ? 1e-18f : -1e-18f;
            for (int j = 0; j < STC_HL; ++j)
            {
                float* ap = mem + s.a0 + j * s.ha;
                const float a = z[j] + HIN[j] * ((j & 1) ? ir : il);
                const float v = readBehindL (ap, s.amask, s.hw, s.hA[j]), u = a + apG * v;
                ap[s.hw & s.amask] = u;
                mem[s.h0 + j * s.hl + (s.hw & s.hmask)] = v - apG * u + den;
            }
            s.hw = (s.hw + 1) & s.hmask;
            float L = 0.0f, R = 0.0f;
            for (int j = 0; j < STC_HL; j += 2) L += HOS[j] * y[j];
            for (int j = 1; j < STC_HL; j += 2) R += HOS[j] * y[j];
            L *= hNorm; R *= hNorm;
            hPk = maxf (hPk, maxf (absf (L), absf (R)));
            const float hm = p.halo * open;
            hl = cl + (L - cl) * hm; hr = cr + (R - cr) * hm;
        }
        // the loops (PING: each fed the other's), saturated as COLR says (a run-away stays a sound); linear while it holds
        const float aL = hl + (hr - hl) * ping, aR = hr + (hl - hr) * ping;
        float nl = 0.0f, nr = 0.0f;
        if (nzA > 0.0f)
        {
            const float wl0 = s.rng.bi(), wr0 = s.rng.bi();
            s.nzL[0] = 0.97f * s.nzL[0] + 0.03f * wl0; s.nzL[1] = 0.6f * s.nzL[1] + 0.4f * wl0;
            s.nzR[0] = 0.97f * s.nzR[0] + 0.03f * wr0; s.nzR[1] = 0.6f * s.nzR[1] + 0.4f * wr0;
            nl = nzA * (3.0f * s.nzL[0] + s.nzL[1]); nr = nzA * (3.0f * s.nzR[0] + s.nzR[1]);
        }
        // the loop's ceiling (v84): its saturation is set where the loop may go, its ceiling (-8.4 dBFS at its peaks) or
        // what comes in, whichever is higher: a delay plays its source as it did (up to its own level, the saturation
        // where it always was for a loud one); a run-away, with or without HALO, from a source or from nothing, sings
        // there, at about a track's level, held sample by sample (no pumping); HOLD left untouched
        {
            const float xp = maxf (absf (xL[i]), absf (xR[i])) * g[i];
            s.rgIn += (xp > s.rgIn ? rgA : rgIr) * (xp - s.rgIn);
            const float ct = clampf (1.2f * s.rgIn, STC_CEIL, 1.0f);
            s.rgC += (ct > s.rgC ? rgA : rgCr) * (ct - s.rgC);
        }
        const float ul = aL * fbG + nl, ur = aR * fbG + nr, cI = 1.0f / s.rgC;
        float sl, sr2;
        if (dk > 0.0f) { sl = (tanhf_ (ul * cI * drv + bias) - tb) * idrv * s.rgC; sr2 = (tanhf_ (ur * cI * drv + bias) - tb) * idrv * s.rgC; }
        else { sl = softclip (ul * cI) * s.rgC; sr2 = softclip (ur * cI) * s.rgC; }
        const float loopL = sl + (aL * fbG - sl) * hold, loopR = sr2 + (aR * fbG - sr2) * hold;
        loopPk = maxf (loopPk, maxf (absf (loopL), absf (loopR)));
        // the input through the gate (PING: the source goes in on the left), a pluck
        const float xl = xL[i] * g[i] * open, xr = xR[i] * g[i] * open, mid = 0.5f * (xl + xr);
        float pl = 0.0f, pr = 0.0f;
        if (s.bLeft > 0)
        {
            const float u = 1.0f - (float) s.bLeft / (float) s.bLen, e = s.bAmp * (1.0f - u) * (1.0f - u);
            s.bLpL += bC * (s.rng.bi() - s.bLpL); s.bLpR += bC * (s.rng.bi() - s.bLpR);
            pl = e * s.bLpL * 2.0f; pr = e * s.bLpR * 2.0f;
            --s.bLeft;
        }
        const float den = (i & 1) ? 1e-18f : -1e-18f;
        lineL[s.w & s.mask] = xl + (mid - xl) * ping + loopL + pl + den;
        lineR[s.w & s.mask] = xr * (1.0f - ping) + loopR + pr + den;
        s.w = (s.w + 1) & s.mask;
        // DUCK, WIDE
        float dg = 1.0f;
        if (p.duck > 0.002f)
        {
            const float e = maxf (absf (xL[i]), absf (xR[i]));
            s.duckE += (e > s.duckE ? kDa : kDr) * (e - s.duckE);
            dg = 1.0f - 0.92f * p.duck * minf (1.0f, s.duckE * 8.0f);
        }
        const float ol = knee (hl, s.rgC), orr = knee (hr, s.rgC);
        const float om = 0.5f * (ol + orr), os = 0.5f * (ol - orr) * wide;
        wL[i] = (om + os) * dg; wR[i] = (om - os) * dg;
    }
    s.loopPk = maxf (loopPk, s.loopPk * 0.9f); s.hPk = maxf (hPk, s.hPk * 0.9f);
    // (a numeric failure: everything again from silence)
    bool bad = isBad (s.rgC) || isBad (s.lpL.ic1) || isBad (s.lpR.ic1) || isBad (s.hpL) || isBad (s.hpR) || isBad (s.dL) || isBad (s.bpL.ic1);
    for (int j = 0; j < STC_HL; ++j) if (isBad (s.hLp[j]) || isBad (s.hHp[j])) bad = true;
    if (bad) { stencilInit (mem, sr); for (int i = 0; i < n; ++i) wL[i] = wR[i] = 0.0f; }
}

void stencilInfo (const float* mem, float* out)
{
    const StState& s = st (mem);
    out[0] = s.tL; out[1] = s.tR; out[2] = s.fb; out[3] = s.loopPk; out[4] = s.holdS; out[5] = s.flipS; out[6] = s.hPk;
}

} // namespace sk
