// IMPAKT — ALLOY, metallic sounds (see sk_alloy.h)
#include "sk_engine.h"

namespace sk {

namespace {

// ---- the shapes' modes: their frequency ratios against the note (ascending), the indices a hit's place reads, a
// gain each (from the physics of each shape; the free disc and the gong from Chladni's law, the bell from its
// measured series)
enum { SH_WIRE = 0, SH_BAR, SH_TUBE, SH_PLATE, SH_DISC, SH_BELL, SH_GONG, SH_BOWL };

struct Tables
{
    float r[AL_SHAPES][AL_M];            // ratios
    float l2[AL_SHAPES][AL_M];           // their log2 against the pivot (STIF stretches around it)
    int   m1[AL_SHAPES][AL_M], m2[AL_SHAPES][AL_M];
    float g[AL_SHAPES][AL_M];
    float pivot[AL_SHAPES];              // the ratio that stays where it is as STIF stretches the others (the note)
    float addU[AL_ADD], addA[AL_ADD];    // ADD: each sine's place in its band (0 .. 1) and its level
    int   ready;
};
Tables T;

// (sorts n modes by ratio, their indices and gains with them)
void sortModes (float* f, int* a, int* b, float* g, int n)
{
    for (int i = 1; i < n; ++i)
        for (int j = i; j > 0 && f[j] < f[j - 1]; --j)
        {
            float t = f[j]; f[j] = f[j - 1]; f[j - 1] = t;
            int u = a[j]; a[j] = a[j - 1]; a[j - 1] = u; u = b[j]; b[j] = b[j - 1]; b[j - 1] = u;
            t = g[j]; g[j] = g[j - 1]; g[j - 1] = t;
        }
}

// a dense shape's modes thinned out over the top (a plate's or a disc's crowd past a few times the note, where the ear
// hears a shimmer, not each one): all of them near the note, then one for each band, its gain the cluster's energy
// (the root of how many it stands for, at most twice: never one lone tone over the rest). From n sorted modes into AL_M slots of shape sh
void thin (int sh, const float* f, const int* a, const int* b, int n, float slope)
{
    int k = 0, i = 0;
    while (k < AL_M && i < n)
    {
        const float r = f[i] / f[0];
        const float gap = exp2f_ (0.02f + slope * log2f_ (maxf (r, 1.0f)));       // (1.4 % at the note, more and more apart over it)
        int j = i + 1, c = 1;
        while (j < n && f[j] / f[0] < r * gap) { ++j; ++c; }
        T.r[sh][k] = r; T.m1[sh][k] = a[i]; T.m2[sh][k] = b[i]; T.g[sh][k] = minf (sqrtf_ ((float) c), 2.0f);
        ++k; i = j;
    }
    for (; k < AL_M; ++k) { T.r[sh][k] = T.r[sh][k - 1] * 1.08f; T.m1[sh][k] = T.m1[sh][k - 1] + 1; T.m2[sh][k] = T.m2[sh][k - 1]; T.g[sh][k] = 1.0f; }
}

// the circular shapes (DISC, GONG): modes (m nodal diameters, n nodal circles), Chladni's f ~ (m + 2n)^p (a flat disc
// p = 2; the gong's curved, hammered plate lower: denser), the pairs his law puts together split a little (a real
// disc is never exact: they beat), the rigid ones left out
void circular (int sh, float p, float split, float slope)
{
    enum { NC = 1400 };
    static float f[NC]; static int a[NC], b[NC]; static float g[NC]; int c = 0;
    for (int m = 0; m <= 60; ++m)
        for (int n = 0; n <= 22; ++n)
        {
            if ((m < 2 && n == 0) || c >= NC) continue;
            const float base = (float) (m + 2 * n);
            // (the lowest pair left together: the note stays clean, the beating starts over it)
            const float sp = split * clampf ((base - 2.0f) / 6.0f, 0.0f, 1.0f);
            f[c] = powf_ (base, p) * (1.0f + sp * (0.011f * (float) (m % 9) - 0.008f * (float) (n % 7) + 0.004f * (float) ((m * 7 + n * 3) % 5)));
            a[c] = m; b[c] = n; g[c] = 1.0f; ++c;
        }
    sortModes (f, a, b, g, c);
    thin (sh, f, a, b, c, slope);
}

void buildTables()
{
    if (T.ready) return;
    for (int s = 0; s < AL_SHAPES; ++s) { T.pivot[s] = 1.0f; for (int k = 0; k < AL_M; ++k) { T.g[s][k] = 1.0f; T.m2[s][k] = 0; } }
    // WIRE: a wire, its harmonics (STIF stretches them: a spring, a piano's bass wire)
    for (int k = 0; k < AL_M; ++k) { T.r[SH_WIRE][k] = (float) (k + 1); T.m1[SH_WIRE][k] = k + 1; }
    // BAR: a free bar (beta L = 4.730, 7.853, 10.996, 14.137, then (2n + 1) pi / 2), squared against the first
    {
        const double bt[4] = { 4.73004, 7.85320, 10.99561, 14.13717 };
        for (int k = 0; k < AL_M; ++k)
        {
            const double bk = k < 4 ? bt[k] : (2.0 * k + 3.0) * 1.5707963;
            T.r[SH_BAR][k] = (float) ((bk / bt[0]) * (bk / bt[0])); T.m1[SH_BAR][k] = k + 1;
        }
    }
    // TUBE: a tubular chime: a free bar's modes, the note heard an octave under its fourth (8.93 : 13.34 : 18.64, about
    // 2 : 3 : 4 of it); the two lowest far under the note, faint
    for (int k = 0; k < AL_M; ++k)
    {
        T.r[SH_TUBE][k] = T.r[SH_BAR][k] / (0.5f * T.r[SH_BAR][3]); T.m1[SH_TUBE][k] = k + 1;
        T.g[SH_TUBE][k] = k < 2 ? 0.2f : k == 2 ? 0.55f : 1.0f;
    }
    T.pivot[SH_TUBE] = 2.0f;
    // PLATE: a simply supported plate, its sides 1 : 1.618: (m^2 + (n / 1.618)^2) against (1, 1)
    {
        enum { NP = 1600 };
        static float f[NP]; static int a[NP], b[NP]; static float g[NP]; int c = 0;
        for (int m = 1; m <= 40; ++m) for (int n = 1; n <= 40; ++n) { f[c] = (float) (m * m) + (float) (n * n) / (1.618034f * 1.618034f); a[c] = m; b[c] = n; g[c] = 1.0f; ++c; }
        sortModes (f, a, b, g, c);
        thin (SH_PLATE, f, a, b, c, 0.035f);
    }
    // DISC, GONG
    circular (SH_DISC, 2.0f, 1.0f, 0.035f);
    circular (SH_GONG, 1.38f, 2.2f, 0.085f);
    // BELL: a minor-third church bell against its prime (hum, prime, tierce, quint, nominal, then the upper partials,
    // further apart as they go)
    {
        static const float B[22] = { 0.5f, 1.0f, 1.188f, 1.5f, 2.0f, 2.5f, 2.66f, 3.0f, 3.36f, 4.0f, 4.32f, 4.75f, 5.34f,
            5.62f, 6.0f, 6.66f, 7.12f, 7.5f, 8.0f, 8.7f, 9.33f, 10.0f };
        float r = 10.0f;
        for (int k = 0; k < AL_M; ++k)
        {
            if (k >= 22) r *= 1.06f + 0.002f * (float) (k - 22);
            T.r[SH_BELL][k] = k < 22 ? B[k] : r; T.m1[SH_BELL][k] = k + 1;
        }
    }
    // BOWL: a bowl, its (m, 0) modes for m = 2.. (m (m^2 - 1) / sqrt (m^2 + 1) against m = 2), each a doublet (the
    // rim never quite round: two modes 0.2 % apart, beating)
    for (int k = 0; k < AL_M; ++k)
    {
        const int m = 2 + k / 2;
        const double fm = m * (m * m - 1.0) / __builtin_sqrt (m * m + 1.0), f2 = 6.0 / __builtin_sqrt (5.0);
        T.r[SH_BOWL][k] = (float) (fm / f2 * (k & 1 ? 1.002 : 1.0)); T.m1[SH_BOWL][k] = m;
    }
    for (int s = 0; s < AL_SHAPES; ++s) for (int k = 0; k < AL_M; ++k) T.l2[s][k] = log2f_ (T.r[s][k] / T.pivot[s]);
    // ADD: its sines spread over the band by the golden ratio (never two together, never a harmonic row), their
    // levels uneven the same way
    for (int k = 0; k < AL_ADD; ++k)
    {
        const float u = 0.5f + 0.6180340f * (float) k;
        T.addU[k] = u - floorf_ (u);
        const float w = 0.7548777f * (float) (k + 1);
        T.addA[k] = 0.55f + 0.45f * (w - floorf_ (w));
    }
    T.ready = 1;
}

// a mode's shape at a place p (0 the edge .. 0.5 the middle), signed: a hit there rings it by as much, a point heard
// there hears it by as much (q: the other coordinate, across a plate or round a disc)
float shapeAt (int sh, int k, float p, float q)
{
    const int m = T.m1[sh][k], n = T.m2[sh][k];
    switch (sh)
    {
        case SH_WIRE: return sin2pi (0.5f * (float) m * p);
        case SH_BAR: case SH_TUBE: return sin2pi (0.5f * ((float) m + 0.5f) * p + 0.06f);
        case SH_PLATE: return sin2pi (0.5f * (float) m * p) * sin2pi (0.5f * (float) n * q);
        case SH_DISC: case SH_GONG:
        {
            // (from the rim inwards: the circles' nodes across the radius; the diameters' modes fade towards the
            // middle, only the round ones ring there; q the angle round it)
            const float rho = clampf (1.0f - 2.0f * p, 0.0f, 1.0f);
            const float ang = m == 0 ? 1.0f : minf (1.0f, powf_ (maxf (rho * 1.25f, 1e-3f), 0.6f * (float) m));
            return ang * cos2pi ((float) m * q) * (0.3f + 0.7f * cos2pi (0.5f * ((float) n + 0.35f) * rho));
        }
        default: return sin2pi (0.5f * (float) m * p + 0.08f);     // BELL, BOWL: from the rim, every mode sounds
    }
}
// how much a hit at p rings mode k as heard from a fixed point (its sign with it: the modes neither all add up at the
// hit nor all cancel, as on a real object)
inline float shapeOf (int sh, int k, float p)
{
    const float h = shapeAt (sh, k, p, 0.41f), l = shapeAt (sh, k, 0.37f, 0.13f);
    return h * (l < 0.0f ? -1.0f : 1.0f) * (0.4f + 0.6f * absf (l));
}

// the mallet's spectrum: a raised cosine of T seconds (its area 1), at f
inline float malletAt (float f, float Tm)
{
    const float x = f * Tm;
    if (x < 1e-4f) return 1.0f;
    const float d = 1.0f - x * x;
    if (absf (d) < 1e-3f) return 0.5f;
    return absf (sin2pi (0.5f * x) / (PI * x) / d);
}

// a draw from a number (-1 .. 1): the same for the same number (ORGN: a mode's place and level this hit)
inline float hashBi (u32 x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return (float) (x >> 8) * (2.0f / 16777216.0f) - 1.0f;
}

struct Mode { float a1, a2, b, bb, fw; };

// the object's modes this chunk; g2, gs: their gains as the mallet rings them (squared and summed, over the first 0.3
// s: the level heard; their first peak), gp their gains squared (a noise rings them apart)
// (pm: the pitch at the hit, a ratio; pushed up, a mode near the top only goes as far as it can, its distance to it
// shrinking smoothly: none crosses it, none is left out, none lands on another)
int setModes (Mode* md, int sh, float nAct, float f0, float pm, float sr, float T60, float matE, float stE, float hit,
              float relF, float mT, float orgn, u32 seed, float& g2, float& gs, float& gp, float& fbQ, float& fbT)
{
    int n = 0; g2 = 0.0f; gs = 0.0f; gp = 0.0f; fbQ = 0.0f; fbT = 1.0f;
    const float nyq = 0.45f * sr, pv = T.pivot[sh], cap = 0.43f * sr;
    const int top = mini ((int) nAct + 1, AL_M);
    for (int k = 0; k < top; ++k)
    {
        // (ORGN: each mode a little out of its place this hit, the lowest kept: the note stays)
        const float r = pv * exp2f_ (stE * T.l2[sh][k]) * (orgn > 0.0f && k > 0 ? 1.0f + orgn * 0.007f * hashBi (seed + 97u * (u32) k) : 1.0f);
        const float fn = f0 * r;
        if (fn > cap) break;
        float f = fn * pm;
        if (pm > 1.0f) { const float room = cap - fn; f = fn + room * (1.0f - expf_ (-(f - fn) / maxf (room, 1.0f))); }
        float t60 = T60 * exp2f_ (-matE * log2f_ (maxf (r, 0.05f)));
        t60 = maxf (t60 * relF, 0.004f);
        const float rr = f < 15.0f ? 0.0f : expf_ (-6.9077553f / (t60 * sr));
        const float th = f / sr, sn = sin2pi (th);
        float g = shapeOf (sh, k, hit) * T.g[sh][k];
        if (orgn > 0.0f) g *= 1.0f + 0.4f * orgn * hashBi (seed + 31u * (u32) k + 7u);
        if ((float) k >= nAct) g *= nAct - (float) (int) nAct;          // (SIZE: the last one fades in)
        if (f > 0.35f * sr) g *= (nyq - f) / (0.1f * sr);
        if (f < 15.0f) g = 0.0f;
        md[n].a1 = 2.0f * rr * cos2pi (th); md[n].a2 = rr * rr;
        md[n].b = g * sn;                                                // (an impulse rings at g, whatever the pitch)
        md[n].bb = k < 6 ? 0.0f : T.g[sh][k] * sn * (f > 0.35f * sr ? (nyq - f) / (0.1f * sr) : 1.0f);   // (GONG's swell: every high mode alike)
        const float ge = g * malletAt (f, mT);
        const float q = 4.14f / t60;
        g2 += ge * ge * (1.0f - expf_ (-q)) / q;
        gs += absf (g) * minf (1.0f, 1.0f / (PI * f * mT));
        gp += g * g;
        // FDBK: each mode's speed taken back, over its pitch's (a mode's loop the same at any pitch, in phase); the
        // loop's gain set against the mode that sings first (the loudest for the longest)
        md[n].fw = (g < 0.0f ? -0.5f : 0.5f) / maxf (sin2pi (0.5f * th), 1e-4f);
        if (absf (g) * t60 > fbQ) { fbQ = absf (g) * t60; fbT = t60; }
        ++n;
    }
    return n;
}

// the engine's amplitude envelope (its gate: attack, hold while the gate is open, then the release)
inline void gateTick (Voice& v, float attInc)
{
    if (v.astage == 0)      { v.aenv += attInc; if (v.aenv >= 1.0f) { v.aenv = 1.0f; v.astage = 1; } }
    else if (v.astage == 1) { if (v.aHoldLeft > 0.0f) v.aHoldLeft -= 1.0f; else v.astage = 2; }
}

inline float wrapP (float p) { return p - floorf_ (p); }

// DRIV: log cosh, its part over |u| (log cosh u = |u| - ln 2 + this): the drive's antiderivative, the steps between
// two samples taken without losing the small ones
inline float lcTail (float a) { return 0.6931472f * log2f_ (1.0f + exp2f_ (-2.8853901f * a)); }

// RATI: 0.5 .. 16 on a log scale; within 1.2 % of a whole number, a half, or one of the inharmonic ratios metal is made
// of (√2, φ, √3, e, π, 3.5), it sits on it
inline float ratioOf (float v)
{
    const float r = 0.5f * exp2f_ (clampf (v, 0.0f, 1.0f) * 5.0f);
    static const float NICE[24] = { 0.5f, 1.0f, 1.4142136f, 1.5f, 1.6180340f, 1.7320508f, 2.0f, 2.5f, 2.7182818f, 3.0f,
        3.1415927f, 3.5f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 16.0f };
    for (int i = 0; i < 24; ++i) if (absf (r - NICE[i]) < 0.012f * NICE[i]) return NICE[i];
    return r;
}

// DAC: bits and clock (0: the engine's own)
struct DacStep { int bits; float rate; };
const DacStep DACS[AL_DACS] = { { 0, 0.0f }, { 16, 0.0f }, { 12, 0.0f }, { 12, 32000.0f }, { 12, 24000.0f },
                                { 10, 16000.0f }, { 8, 11025.0f }, { 6, 8000.0f } };

// BNCE to the left, the flams: how the strikes come (their gaps against the first, their force; the last full)
const float FLAM_G[4] = { 1.0f, 0.78f, 1.22f, 0.9f };
const float FLAM_A[4] = { 0.72f, 0.88f, 0.8f, 0.95f };

// NCOL: the noise's colour, a low-pass (12 dB/oct: 120 Hz .. open past 0.84) and over 0.55 a high-pass (6 dB/oct,
// rising to 10 kHz): dark .. as it is .. thin
struct NoiseCol { float f, c1, c2, h; };
inline NoiseCol noiseCol (float col, float sr)
{
    NoiseCol c;
    c.f = tanpi (minf (120.0f * exp2f_ (col * 8.6f), 0.45f * sr) / sr);
    c.c1 = 1.0f / (1.0f + c.f * (c.f + 1.4142136f)); c.c2 = c.f * c.c1;
    c.h = col > 0.55f ? 1.0f - expf_ (-TWO_PI * 20.0f * exp2f_ ((col - 0.55f) * 20.0f) / sr) : 0.0f;
    return c;
}
// HISS: a noise of each colour, its level measured (a long run of its filters on a fixed noise; AL_HN colours, built
// when the engine starts, at its rate): one over its root mean square
float hissMeasure (const NoiseCol& c)
{
    float lp = 0.0f, bp = 0.0f, hl = 0.0f; double e = 0.0;
    u32 x = 0x2545F491u;
    for (int i = 0; i < 20480; ++i)
    {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        const float s = (float) (i32) (x >> 8) * (1.0f / 8388608.0f) - 1.0f;
        const float v3 = s - lp, v1 = c.c1 * bp + c.c2 * v3, v2 = lp + c.f * v1;
        bp = 2.0f * v1 - bp; lp = 2.0f * v2 - lp;
        hl += c.h * (v2 - hl);
        const float y = v2 - hl;
        if (i >= 4096) e += (double) (y * y);
    }
    return 1.0f / sqrtf_ (maxf ((float) (e / 16384.0), 1e-10f));
}
inline float hissNorm (const float* tab, float col)
{
    const float u = clampf (col, 0.0f, 1.0f) * (float) (AL_HN - 1);
    const int k = mini ((int) u, AL_HN - 2);
    const float t = u - (float) k;
    return tab[k] * powf_ (tab[k + 1] / tab[k], t);
}

}  // namespace

// (the tables are built once, as the engine starts: not on the audio thread at the first hit)
void alloyTables (float sr, float* hiss)
{
    buildTables();
    for (int k = 0; k < AL_HN; ++k) hiss[k] = hissMeasure (noiseCol ((float) k / (float) (AL_HN - 1), sr));
}

void Engine::alloyTrigger (Voice& v, const float* P)
{
    buildTables();
    AlloyState& a = v.al;
    for (int k = 0; k < AL_M; ++k) a.y1[k] = a.y2[k] = 0.0f;
    const float vel = clampf (v.vel, 0.0f, 1.0f);
    // ORGN: this hit's own (its pitch, its place, the mallet, its level; the modes' draw), the tension, the drift
    const float orgn = P[PA_ORGN];
    a.oPit = orgn * 0.3f * v.rng.bi();
    a.oHit = orgn * 0.1f * v.rng.bi();
    const float oMal = exp2f_ (orgn * 0.7f * v.rng.bi());
    a.oLvl = exp2f_ (orgn * 0.35f * v.rng.bi());
    a.oSeed = v.rng.next();
    a.oTen = 1.0f; a.oDr = 0.0f; a.oDrT = v.rng.bi();
    // the mallet: felt (8 ms) .. steel (80 µs), harder with the velocity, shorter on a stiffer object
    const float malt = P[PA_MALT];
    const float mL = sr * (0.00008f + 0.008f * (1.0f - malt) * (1.0f - malt)) * exp2f_ (-(P[P_B] - 0.5f)) * oMal;
    a.mLen = maxf (2.0f, mL * (1.6f - 1.1f * vel));
    a.mLenN = maxf (2.0f, mL * 0.5f);         // (the level is set as for the hardest hit: softer, darker and quieter)
    a.mPos = 0.0f; a.mAmp = 1.0f;
    a.cE = 1.0f; a.tE = 1.0f;
    // BNCE: to the right the first bounce, its gap and force; to the left the flams (a grace note 30 ms before .. four
    // strikes 10 ms apart, a clap; the first of them softer)
    const float bn = P[PA_BNCE];
    a.bMode = 0; a.bLeft = 0; a.bK = 0; a.bAmp = 0.0f; a.nFast = 0.0f; a.fGap = 0.0f;
    if (bn > 0.502f)
    {
        const float b = (bn - 0.5f) * 2.0f;
        a.bMode = 1; a.bInt = 0.05f + 0.25f * b; a.bNext = a.bInt; a.bAmp = 0.45f + 0.4f * b;
    }
    else if (bn < 0.498f)
    {
        const float b = (0.5f - bn) * 2.0f;
        a.bMode = 2; a.bLeft = 1 + (int) (b * 3.0f + 0.5f);
        a.fGap = 0.03f - 0.02f * b;
        a.bNext = a.fGap * FLAM_G[0] * (1.0f + 0.3f * orgn * v.rng.bi());
        a.mAmp = FLAM_A[0]; a.nFast = 1.0f;
    }
    a.t = 0.0f;
    a.nLp = a.nBp = a.nHl = 0.0f;
    a.nStage = 1; a.nEnv = P[PA_NLEN] >= 0.99f ? 0.0f : a.mAmp;
    a.hCol = -1.0f; a.hNorm = 1.0f;
    a.pSn = 1.0f; a.pR = P[PA_PRIS] > 0.002f ? 0.0f : 1.0f; a.mSw = 1.0f;
    a.fPh[0] = a.fPh[1] = 0.0f; a.fEnv = 1.0f; a.fIdx = 1.0f; a.fAtk = P[PA_IATK] > 0.002f ? 0.0f : 1.0f;
    // ADD: each sine starts on its level, at the zero of its phase (no click)
    // (each on its own phase, the golden angle apart: they never start as one lump; faded in over 1 ms)
    for (int k = 0; k < AL_ADD; ++k) { const float ph = 0.3819660f * (float) k; a.aRe[k] = T.addA[k] * cos2pi (ph); a.aIm[k] = T.addA[k] * sin2pi (ph); }
    a.addAtt = 0.0f; a.simPk = 0.0f;
    a.env = 0.0f; a.peak = 1e-4f; a.bHp = 0.0f;
    a.cw = 0; a.cLp = 0.0f;
    for (int i = 0; i < AL_COMB; ++i) v.lbuf[i] = 0.0f;
    for (int s = 0; s < AL_PH; ++s) a.apx[s] = a.apy[s] = 0.0f;
    a.apFb = 0.0f;
    a.lfo = 0.25f;                            // (the LFO from its top at each hit: every hit swept the same)
    a.dPh = 1.0f; a.dHold = 0.0f; a.dL1 = a.dL2 = 0.0f;
    a.sObj = P[PA_OBJ]; a.sFm = P[PA_FM]; a.sAdd = P[PA_ADD]; a.sCmb = P[PA_COMB]; a.sPha = P[PA_PHAS];
    a.lim = 0.0f; a.rel = 0.0f; a.dcx = a.dcy = 0.0f; a.lvl = 1.0f; a.gNorm = 0.0f;
    a.fbD = a.fbLvl = 0.0f; a.fbE = 1.0f; a.dvX = a.dvE = 0.0f; a.cutG = 1.0f;
    a.gated = v.holdSec > 0.0f || v.padHeld || v.hold;
    v.os2.reset (HB1);
}

void Engine::alloyRender (Voice& v, const float* P, float tuneSt, int n, float* out)
{
    AlloyState& a = v.al;
    const float fn = (float) n, isr = 1.0f / sr;
    const float vel = clampf (v.vel, 0.0f, 1.0f);
    // the gate (the engine's: open while the amplitude envelope holds) and the damper when it shuts
    const float att = 1.0f / (0.0002f * sr);
    const bool hold = P[P_DEC] >= DEC_HOLD, open = v.astage < 2;
    const float T60 = (hold && open) ? 60.0f : 0.04f * exp2f_ (minf (P[P_DEC], DEC_HOLD) * 9.0f);
    if (a.gated && ! open) a.rel += (1.0f - a.rel) * (1.0f - expf_ (-fn / (0.008f * sr)));
    const float relMul = 1.0f - 0.97f * a.rel;
    // OBJECT
    const int sh = idxOf (P[P_A], AL_SHAPES);
    const float stE = exp2f_ ((P[P_B] - 0.5f) * 1.3f);
    const float mass = P[P_C], omm = 1.0f - mass;
    const float matE = 0.08f + 1.9f * omm * sqrtf_ (omm) * omm * 0.85f;
    const float size = P[P_D], nAct = 6.0f + 42.0f * size * size;
    const float hit = clampf (0.02f + 0.48f * P[P_E] + a.oHit, 0.02f, 0.5f);
    // ORGN: this hit's pitch, the tension (a hard hit pushes it up; it settles as the metal rings), a slow drift
    const float orgn = P[PA_ORGN];
    const float tenSt = orgn * (0.25f + 0.85f * vel) * a.oTen;
    a.oTen *= expf_ (-fn / (clampf (0.2f * T60, 0.03f, 0.5f) * sr));
    if (orgn > 0.0f)
    {
        a.oDr += (a.oDrT - a.oDr) * (1.0f - expf_ (-fn / (0.12f * sr)));
        if (v.rng.uni() < fn / (0.25f * sr)) a.oDrT = v.rng.bi();
    }
    const float orgSt = a.oPit + tenSt + 0.12f * orgn * a.oDr;
    // IMPACT: the pitch at the hit (up to four octaves down to the note, or up to it from three under), how fast; PRIS:
    // how long it takes to get there (the arc's top brought to the full PENV)
    const float pv = P[PA_PENV];
    const float penvSt = absf (pv - 0.5f) < 0.01f ? 0.0f : pv > 0.5f ? (pv - 0.5f) * 96.0f : (pv - 0.5f) * 72.0f;
    const float pTau = 0.001f * exp2f_ (P[PA_PTIM] * 7.64f), pDec = expf_ (-1.0f / (pTau * sr));
    const float rTau = P[PA_PRIS] > 0.002f ? 0.001f * exp2f_ (P[PA_PRIS] * 8.6f) : 0.0f;
    float pNorm = 1.0f;
    if (rTau > 0.0f) { const float q = rTau / (rTau + pTau); pNorm = 1.0f / maxf ((1.0f - q) * powf_ (q, rTau / pTau), 1e-3f); }
    else a.pR = 1.0f;
    const float rK = rTau > 0.0f ? 1.0f - expf_ (-1.0f / (rTau * sr)) : 1.0f;
    const float pRm = rTau > 0.0f ? 1.0f - (1.0f - a.pR) * expf_ (-0.5f * fn / (rTau * sr)) : 1.0f;
    const float pMid = pRm * a.pSn * expf_ (-0.5f * fn / (pTau * sr)) * pNorm;      // (the chunk's middle)
    const float fNote = 130.8128f * exp2f_ ((tuneSt + orgSt) * (1.0f / 12.0f)), pMul = exp2f_ (penvSt * pMid * (1.0f / 12.0f));
    const float f0 = fNote * pMul;
    // the object's modes
    Mode M[AL_M];
    float g2 = 0.0f, gs = 0.0f, gp = 0.0f, fbQ = 0.0f, fbT = 1.0f;
    const float mT = a.mLenN / sr;
    const int nM = setModes (M, sh, nAct, fNote, pMul, sr, T60, matE, stE, hit, relMul, mT, orgn, a.oSeed, g2, gs, gp, fbQ, fbT);
    // the output's level against the modes' gains as the mallet rings them (together, about the root of their
    // squares; their first peak, a share of their sum), glided; the noise's against the root of their squares
    // (the first peak: the hardest hit's first milliseconds run once a note through these modes, its peak against their
    // sum; later chunks scale it with the sum, the settings moving)
    if (a.simPk <= 0.0f && nM > 0)
    {
        float s1[AL_M], s2[AL_M];
        for (int k = 0; k < nM; ++k) s1[k] = s2[k] = 0.0f;
        const int L = maxi (2, (int) a.mLenN), N = mini (L + (int) (0.012f * sr + 1.5f * sr / maxf (f0, 40.0f)), (int) (0.06f * sr));
        const float iL = 1.0f / (float) L, ar = 2.0f / (float) L;
        float pk = 1e-6f;
        for (int i = 0; i < N; ++i)
        {
            const float xm = i < L ? (0.5f - 0.5f * cos2pi ((float) i * iL)) * ar : 0.0f;
            float sm = 0.0f;
            for (int k = 0; k < nM; ++k) { const float y = M[k].a1 * s1[k] - M[k].a2 * s2[k] + M[k].b * xm; s2[k] = s1[k]; s1[k] = y; sm += y; }
            pk = maxf (pk, absf (sm));
        }
        a.simPk = pk / maxf (gs, 1e-6f);
    }
    const float gM = 0.5f / maxf (maxf (sqrtf_ (g2) * 1.2f, gs * maxf (a.simPk, 1e-3f)), 1e-2f);
    const float gS = 0.45f / sqrtf_ (maxf (gp, 1e-4f));
    if (a.gNorm <= 0.0f) a.gNorm = gM; else a.gNorm += (gM - a.gNorm) * minf (1.0f, fn / (0.02f * sr));
    // NOISE: NOIS its share in the impact (past the middle the mallet gives way to it), HISS heard as it is, NCOL their
    // colour, NLEN their envelope (a burst of 1 ms .. 2 s, HELD at the top: a bow, a scrape, while the gate is open; a
    // flam's burst cut short by the next)
    const float nois = P[PA_NOIS], hiss = P[PA_HISS], nlen = P[PA_NLEN];
    const float mG = 1.0f - 0.85f * clampf ((nois - 0.5f) * 2.0f, 0.0f, 1.0f), xG = minf (1.0f, nois * 2.0f);
    const bool nOn = xG > 0.0f || hiss > 0.0005f;
    const bool held = nlen >= 0.99f;
    const float bTau = 0.001f * exp2f_ (nlen * 11.0f);
    const float col = P[PA_NCOL];
    const NoiseCol nc = noiseCol (col, sr);
    if (hiss > 0.0005f && absf (col - a.hCol) > 0.002f) { a.hNorm = hissNorm (alHiss, col); a.hCol = col; }
    const float hG = 0.22f * hiss * sqrtf_ (hiss) * a.hNorm;
    const float susLeft = a.gated ? (open ? 1.0f : 0.0f) : (v.ns < (u32) (0.5f * T60 * sr) ? 1.0f : 0.0f);
    const float nDec = expf_ (-1.0f / ((held ? (susLeft > 0.0f ? 1e9f : 0.06f) : bTau) * sr));
    const float nDecF = a.bMode == 2 ? expf_ (-1.0f / (minf (bTau, 0.4f * a.fGap) * sr)) : nDec;
    const float nAtt = held ? 1.0f - expf_ (-1.0f / (0.02f * sr)) : 1.0f;
    const float rRef = expf_ (-6.9077553f / (maxf (T60 * relMul, 0.004f) * sr));
    // (its level, as heard against the mallet's: a burst against an impulse's energy, a held one against the decay)
    const float nGain = (held ? 6.6f * sqrtf_ (1.0f - rRef) : 1.7f / sqrtf_ (bTau * sr)) * (gS / maxf (a.gNorm, 1e-6f));
    // CLIK: the transient (its first milliseconds pushed up, a tick of the contact over it)
    const float clik = P[PA_CLIK];
    const float cDec = expf_ (-1.0f / (0.003f * sr)), tDec = expf_ (-1.0f / (0.0007f * sr)), cKn = 1.0f / (1.0f + 1.5f * clik);
    // FDBK: the modes' speeds back into the object, the loop's gain set against the mode that sings first (the loudest
    // for the longest): up to 3/4 it rings longer (the loop just under 1), past it it sings by itself, its swell from
    // endless to 30 ms whatever DEC; its level held under a ceiling that falls with the decay (it sings on as long as
    // DEC lets it, it never runs away)
    const float fdbk = P[PA_FDBK];
    const bool fbOn = fdbk > 0.002f && fbQ > 0.0f;
    float fbL = 0.97f * minf (fdbk, 0.75f) / 0.75f;
    if (fdbk > 0.75f) { const float u = (fdbk - 0.75f) * 4.0f; fbL = 1.0f + fbT * (1.0f / 6.9077553f) * 33.0f * u * u; }
    const float kFb = fbOn ? fbL * 13.815511f / (sr * fbQ) : 0.0f;
    const float fbA = 1.0f - expf_ (-1.0f / (0.001f * sr)), fbR = 1.0f - expf_ (-1.0f / (0.08f * sr));
    const float fbCeil = 0.3f * a.fbE;
    a.fbE = (hold && open) ? a.fbE : a.fbE * expf_ (-6.9077553f * fn / (maxf (1.5f * T60 * relMul, 0.004f) * sr));
    // FM
    const float objL = P[PA_OBJ], fmL = P[PA_FM], addL = P[PA_ADD];
    const float ratio = ratioOf (P[PA_RATI]);
    const float iMax = P[PA_INDX] * P[PA_INDX] * 12.0f * (0.5f + 0.5f * vel);
    const float iaTau = P[PA_IATK] > 0.002f ? 0.001f * exp2f_ (P[PA_IATK] * 11.0f) : 0.0f;
    if (iaTau <= 0.0f) a.fAtk = 1.0f;
    const float iaK = iaTau > 0.0f ? 1.0f - expf_ (-1.0f / (iaTau * sr)) : 1.0f;
    const bool iHold = P[PA_IDEC] >= 0.99f;
    const float iDec = iHold ? 1.0f : expf_ (-1.0f / (0.002f * exp2f_ (P[PA_IDEC] * 10.0f) * sr));
    const float mo = P[PA_MENV] - 0.5f;
    const float menvOct = absf (mo) < 0.01f ? 0.0f : mo * 6.0f;
    const float fDec = expf_ (-6.9077553f / (maxf (0.8f * T60 * relMul, 0.004f) * sr));
    const float aaMax = 1.4f * sr;
    // ADD: one to 24 sines (as the object grows: the first on BAND itself) around BAND (2 .. 48 times the note), a
    // band 1.6 octaves wide
    const int nAdd = addL > 0.002f || a.sAdd > 0.002f ? mini (AL_ADD, 1 + (int) (23.0f * size + 0.5f)) : 0;
    const float band = f0 * 2.0f * exp2f_ (P[PA_BAND] * 4.585f);
    float aC[AL_ADD], aS[AL_ADD], aOn[AL_ADD];
    const float aNorm = 0.36f / sqrtf_ (maxf ((float) nAdd, 1.0f)), aAtt = 1.0f / (0.001f * sr);
    for (int k = 0; k < nAdd; ++k)
    {
        const float fk = band * exp2f_ (1.6f * (T.addU[k] - 0.5f));
        // (one over the top of the band, the pitch at the hit pushing it there: not heard till it comes back under)
        aOn[k] = fk > 0.42f * sr ? 0.0f : 1.0f;
        const float fu = minf (fk, 0.42f * sr);
        const float t60 = maxf (0.45f * T60 * relMul * exp2f_ (-0.5f * log2f_ (fk / band)), 0.01f);
        const float rr = expf_ (-6.9077553f / (t60 * sr));
        aC[k] = rr * cos2pi (fu * isr); aS[k] = rr * sin2pi (fu * isr);
    }
    // COLOR: the comb (its pitch from the note, its feedback; under 0 the odd harmonics, a tube, its line twice as
    // short to stay on the note), its loop's damping (a denser metal, brighter), the LFO for FLNG and PHAS
    const float cmb = P[PA_COMB];
    const float cSt = (float) (idxOf (P[PA_CTUN], 49) - 24);
    const float fb = clampf ((P[PA_CFB] - 0.5f) * 2.0f, -1.0f, 1.0f) * 0.995f;
    const float oddK = clampf (-fb * 10.0f, 0.0f, 1.0f), odd = oddK * oddK * (3.0f - 2.0f * oddK);
    // (its level: between what it gives a noise, the root of 1 - fb^2, and what it gives a tone on its teeth, 1 - fb)
    const float cNorm = 1.3f * powf_ (maxf (1.0f - absf (fb), 1e-3f), 0.6f);
    const float cLc = 1.0f - expf_ (-TWO_PI * minf (2500.0f * exp2f_ (mass * 2.7f), 0.45f * sr) / sr);
    const float flng = P[PA_FLNG], pha = P[PA_PHAS];
    const float lfoInc = 0.05f * exp2f_ (P[PA_RATE] * 8.6f) * isr;
    const bool combOn = cmb > 0.002f || a.sCmb > 0.002f;
    float* cb = v.lbuf;
    // PHAS: six all-pass stages, their corner swept 250 Hz .. 6 kHz by the LFO (set once a chunk)
    float apA = 0.0f;
    if (pha > 0.002f || a.sPha > 0.002f)
    {
        const float fc = minf (250.0f * exp2f_ (4.6f * (0.5f + 0.5f * sin2pi (a.lfo))), 0.4f * sr);
        const float tg = tanpi (fc / sr);
        apA = (tg - 1.0f) / (tg + 1.0f);
    }
    // DRIV: a drive, a little uneven (its even harmonics), its antiderivative's steps (no aliasing whistle), its peak
    // brought back to the peak going in (a follower: the tail squares up, it does not swell); faded in over the first
    // 6 % of the knob
    const float driv = P[PA_DRIV];
    const bool drvOn = driv > 0.0005f;
    const float dG = 1.0f + 24.0f * driv * driv, dBi = 0.25f * driv, dTb = tanhf_ (dBi), dW = wetOf (driv);
    const float dvR = expf_ (-1.0f / (0.03f * sr));
    // DAC
    const DacStep& D = DACS[idxOf (P[PA_DAC], AL_DACS)];
    const float dQ = D.bits > 0 ? (float) (1 << (D.bits - 1)) : 0.0f, dIq = dQ > 0.0f ? 1.0f / dQ : 0.0f;
    const float dInc = D.rate > 0.0f && D.rate < sr ? D.rate / sr : 0.0f;
    const float dLc = dInc > 0.0f ? 1.0f - expf_ (-TWO_PI * 0.45f * D.rate / sr) : 1.0f;
    // CUT: all of it shut 20 ms .. 1.5 s after the hit (the later, the softer: 3 .. 80 ms)
    const float cutV = P[PA_CUT];
    const float cutT = cutV > 0.002f ? 0.02f * exp2f_ (cutV * 6.2f) : 1e9f;
    const float cutK = 1.0f - expf_ (-1.0f / ((0.002f + 0.05f * minf (cutT, 1.5f)) * sr));
    // GONG: after the hit the energy climbs from its low modes into the high ones (the swell of a tam-tam): the modes
    // above the sixth rung by a hiss that rises over half a second and follows the gong's own level (harder hits more),
    // as loud as a scrape at its top
    const bool bloom = sh == SH_GONG;
    const float bloomG = bloom ? (0.3f + 0.7f * vel) * 0.05f * 6.6f * sqrtf_ (1.0f - rRef) * (gS / maxf (a.gNorm, 1e-6f)) : 0.0f;
    const float bloomR = 1.0f - expf_ (-1.0f / (0.45f * sr));
    // the glides of the layers' levels (20 ms), the safety, the DC blocker, the followers
    const float sK = 1.0f - expf_ (-1.0f / (0.02f * sr));
    const float limR = expf_ (-1.0f / (0.12f * sr)), dcR = 1.0f - TWO_PI * 12.0f / sr;
    const float envK = 1.0f - expf_ (-1.0f / (0.01f * sr));
    const float mPi = 1.0f / a.mLen, mArea = 2.0f / a.mLen;
    float lvl = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        gateTick (v, att);
        // BNCE: the mallet strikes again (a bounce: sooner and softer each time; a flam: close, the last one full),
        // the pitch, the FM's depth and sweep, the tension and the sines with it
        if (a.bMode != 0 && a.t >= a.bNext)
        {
            float amp;
            if (a.bMode == 1)
            {
                amp = a.bAmp;
                a.bInt *= 0.7f; a.bNext += a.bInt; a.bAmp *= 0.7f;
                if (a.bInt < 0.01f || a.bAmp <= 0.03f) a.bMode = 0;
            }
            else
            {
                ++a.bK; --a.bLeft;
                amp = a.bLeft <= 0 ? 1.0f : FLAM_A[a.bK & 3];
                a.bNext += a.fGap * FLAM_G[a.bK & 3] * (1.0f + 0.3f * orgn * v.rng.bi());
                a.nFast = a.bLeft > 0 ? 1.0f : 0.0f;
                if (a.bLeft <= 0) a.bMode = 0;
            }
            a.mPos = 0.0f; a.mAmp = amp; a.cE = maxf (a.cE, amp); a.tE = maxf (a.tE, amp * 0.7f);
            a.pSn = maxf (a.pSn, amp); a.mSw = maxf (a.mSw, amp); a.oTen = maxf (a.oTen, amp);
            a.fIdx = maxf (a.fIdx, amp);
            if (! held) a.nEnv = maxf (a.nEnv, amp);
            for (int k = 0; k < nAdd; ++k)
            {
                const float want = T.addA[k] * amp * 0.7f, mag = sqrtf_ (a.aRe[k] * a.aRe[k] + a.aIm[k] * a.aIm[k]);
                if (mag < want) { if (mag > 1e-9f) { const float s = want / mag; a.aRe[k] *= s; a.aIm[k] *= s; } else { a.aRe[k] = want; a.aIm[k] = 0.0f; } }
            }
        }
        // the impact: the mallet (a raised cosine of its length, its area fixed: softer, fewer highs, not less hit)
        float x = 0.0f;
        if (a.mPos < a.mLen)
        {
            x = (0.5f - 0.5f * cos2pi (a.mPos * mPi)) * mArea * a.mAmp * mG;
            a.mPos += 1.0f;
        }
        // ... its noise (into the object), the hiss (heard as it is)
        float hs = 0.0f;
        if (nOn && a.nStage < 2)
        {
            if (held && susLeft > 0.0f) a.nEnv += (1.0f - a.nEnv) * nAtt;
            else { a.nEnv *= a.nFast > 0.5f ? nDecF : nDec; if (a.nEnv < 1e-4f && (held || a.bMode == 0)) { a.nEnv = 0.0f; a.nStage = 2; } }
            if (a.nEnv > 0.0f)
            {
                const float s = v.rng.bi();
                const float v3 = s - a.nLp, v1 = nc.c1 * a.nBp + nc.c2 * v3, v2 = a.nLp + nc.f * v1;
                a.nBp = 2.0f * v1 - a.nBp; a.nLp = 2.0f * v2 - a.nLp;
                a.nHl += nc.h * (v2 - a.nHl);
                const float nz = (v2 - a.nHl) * a.nEnv;
                x += nz * nGain * xG;
                hs = nz * hG;
            }
        }
        // FDBK: the sound back into the object
        if (fbOn)
        {
            // (over the ceiling the loop comes down steeply, its eighth power: it settles just over it, however hot)
            float agc = 1.0f;
            if (a.fbLvl > fbCeil) { agc = fbCeil / a.fbLvl; agc *= agc; agc *= agc; agc *= agc; }
            x += kFb * a.fbD * agc * relMul;
        }
        // the object (GONG: its high modes rung by the swell)
        float xu = 0.0f;
        if (bloom)
        {
            a.bHp += (1.0f - a.bHp) * bloomR;
            xu = v.rng.bi() * bloomG * a.bHp * minf (1.0f, a.env / maxf (a.peak, 1e-6f));
        }
        float so = 0.0f, sl = 0.0f;
        if (fbOn)
        {
            float fv = 0.0f;
            for (int k = 0; k < nM; ++k)
            {
                const float y = M[k].a1 * a.y1[k] - M[k].a2 * a.y2[k] + M[k].b * x + M[k].bb * xu;
                fv += M[k].fw * (y - a.y1[k]);
                a.y2[k] = a.y1[k]; a.y1[k] = y; so += y;
                if (k == 5) sl = so;
            }
            a.fbD = fv;
        }
        else
            for (int k = 0; k < nM; ++k)
            {
                const float y = M[k].a1 * a.y1[k] - M[k].a2 * a.y2[k] + M[k].b * x + M[k].bb * xu;
                a.y2[k] = a.y1[k]; a.y1[k] = y; so += y;
                if (k == 5) sl = so;
            }
        so *= a.gNorm;
        if (fbOn) { const float l = absf (so); a.fbLvl += (l - a.fbLvl) * (l > a.fbLvl ? fbA : fbR); }
        // (the swell follows the low modes alone: they give the energy, it never feeds itself)
        a.env += (absf (sl * a.gNorm) - a.env) * envK;
        a.peak = maxf (a.peak, a.env);
        // FM: a carrier on the note (with the pitch at the hit), its modulator on RATI (swept by MENV), at twice the
        // rate; its depth rising over IATK, falling over IDEC
        a.sFm += (fmL - a.sFm) * sK;
        float yf = 0.0f;
        if (a.sFm > 1e-4f)
        {
            const float f = 130.8128f * exp2f_ ((tuneSt + orgSt + penvSt * a.pR * a.pSn * pNorm) * (1.0f / 12.0f));
            const float rt = menvOct != 0.0f ? ratio * exp2f_ (menvOct * a.mSw) : ratio;
            float I = iMax * a.fIdx * a.fAtk;
            const float sw = f * (1.0f + I * rt);
            if (sw > aaMax) I *= maxf (0.0f, (aaMax - f) / maxf (sw - f, 1e-3f));
            const float dC = minf (f * 0.5f * isr, 0.45f), dM = minf (f * rt * 0.5f * isr, 0.45f);
            float xs[2];
            for (int h = 0; h < 2; ++h)
            {
                xs[h] = sin2pi (a.fPh[0] + 0.159155f * I * sin2pi (a.fPh[1]));
                a.fPh[0] = wrapP (a.fPh[0] + dC); a.fPh[1] = wrapP (a.fPh[1] + dM);
            }
            yf = v.os2.process (xs[0], xs[1]) * a.fEnv * 0.42f;
        }
        // ADD: the sines over the top
        a.sAdd += (addL - a.sAdd) * sK;
        float ya = 0.0f;
        for (int k = 0; k < nAdd; ++k)
        {
            const float re = a.aRe[k] * aC[k] - a.aIm[k] * aS[k], im = a.aRe[k] * aS[k] + a.aIm[k] * aC[k];
            a.aRe[k] = re; a.aIm[k] = im; ya += im * aOn[k];
        }
        ya *= aNorm * a.addAtt;
        a.addAtt = minf (1.0f, a.addAtt + aAtt);
        // the sum, its transient (CLIK: the first milliseconds pushed up, the contact's tick)
        a.sObj += (objL - a.sObj) * sK;
        float s = so * a.sObj + yf * a.sFm + ya * a.sAdd + hs;
        // (the first milliseconds pushed up, the rest brought down by half as much: the peak kept near where it was)
        if (clik > 0.002f)
        {
            s *= (1.0f + 3.0f * clik * a.cE) * cKn;
            if (a.tE > 1e-4f) s += v.rng.bi() * a.tE * clik * 0.2f;
        }
        a.cE *= cDec; a.tE *= tDec;
        // COMB: a resonant comb on the sum, its delay swept by FLNG (a flanger in its loop)
        a.sCmb += (cmb - a.sCmb) * sK;
        if (combOn)
        {
            const float sweep = flng > 0.002f ? exp2f_ (0.5f * flng * sin2pi (a.lfo)) : 1.0f;
            const float fc = f0 * exp2f_ (cSt * (1.0f / 12.0f)) * sweep;
            const float dl = clampf (sr / maxf (fc, 10.0f) * (1.0f - 0.5f * odd), 2.0f, (float) (AL_COMB - 4));
            const float rp = (float) a.cw - dl + (float) AL_COMB, fl = floorf_ (rp), fr = rp - fl;
            const int j1 = ((int) fl) & (AL_COMB - 1), j0 = (j1 - 1) & (AL_COMB - 1), j2 = (j1 + 1) & (AL_COMB - 1), j3 = (j1 + 2) & (AL_COMB - 1);
            // (four points, Hermite: the comb's highs kept at any pitch)
            const float p0 = cb[j0], p1 = cb[j1], p2 = cb[j2], p3 = cb[j3];
            const float c1 = 0.5f * (p2 - p0), c2 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3, c3 = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);
            const float d = ((c3 * fr + c2) * fr + c1) * fr + p1;
            a.cLp += cLc * (d - a.cLp);
            const float w = s + fb * a.cLp;
            cb[a.cw] = (isBad (w) || absf (w) < 1e-15f) ? 0.0f : w; a.cw = (a.cw + 1) & (AL_COMB - 1);
            s = s * (1.0f - a.sCmb) + w * cNorm * a.sCmb;
        }
        // PHAS: six all-pass stages and their feedback, mixed half and half at the top (the notches)
        a.sPha += (pha - a.sPha) * sK;
        if (a.sPha > 1e-4f)
        {
            float u = s + 0.6f * a.sPha * a.apFb;
            for (int st = 0; st < AL_PH; ++st)
            {
                const float y = apA * u + a.apx[st] - apA * a.apy[st];
                a.apx[st] = u; a.apy[st] = y; u = y;
            }
            a.apFb = u;
            s = s * (1.0f - 0.5f * a.sPha) + u * 0.5f * a.sPha;
        }
        a.lfo = wrapP (a.lfo + lfoInc);
        // (ORGN: this hit's level, before the drive: a harder one drives harder)
        s *= a.oLvl;
        // DRIV: tanh, by the steps of log cosh between two samples (the small ones as tanh of their middle)
        if (drvOn)
        {
            const float u = dG * s + dBi, u0 = a.dvX, du = u - u0;
            float y;
            if (absf (du) > 1e-3f) y = ((absf (u) - absf (u0)) + (lcTail (absf (u)) - lcTail (absf (u0)))) / du;
            else y = tanhf_ (0.5f * (u + u0));
            a.dvX = u;
            a.dvE = maxf (absf (s), a.dvE * dvR);
            const float gN = a.dvE > 1e-6f ? a.dvE / maxf (tanhf_ (dG * a.dvE + dBi) - dTb, 1e-9f) : 1.0f / dG;
            s += ((y - dTb) * gN - s) * dW;
        }
        else { a.dvX = s; a.dvE = maxf (absf (s), a.dvE * dvR); }     // (ready: no step when it is turned up)
        // DAC: the word held at its clock, its bits, its filter
        if (dQ > 0.0f)
        {
            if (dInc > 0.0f)
            {
                a.dPh += dInc;
                if (a.dPh >= 1.0f) { a.dPh -= 1.0f; a.dHold = floorf_ (clampf (s, -1.0f, 1.0f) * dQ + 0.5f) * dIq; }
                a.dL1 += dLc * (a.dHold - a.dL1); a.dL2 += dLc * (a.dL1 - a.dL2);
                s = a.dL2;
            }
            else s = floorf_ (clampf (s, -1.0f, 1.0f) * dQ + 0.5f) * dIq;
        }
        // the DC blocker; a safety: past 0.8 the level comes down (at once, back in 120 ms), whatever the settings stack
        // up to
        float o = s - a.dcx + dcR * a.dcy; a.dcx = s; a.dcy = o;
        {
            const float ay = absf (o);
            a.lim = maxf (ay, a.lim * limR);
            if (a.lim > 0.8f) o *= 0.8f / a.lim;
        }
        // CUT
        a.cutG += ((a.t >= cutT ? 0.0f : 1.0f) - a.cutG) * cutK;
        out[i] = o * a.cutG;
        lvl = maxf (lvl, absf (out[i]));
        // the envelopes
        a.pR += (1.0f - a.pR) * rK; a.pSn *= pDec; a.mSw *= pDec;
        a.fIdx *= iDec; a.fAtk += (1.0f - a.fAtk) * iaK;
        a.fEnv = (hold && open) ? a.fEnv : a.fEnv * fDec;
        a.t += isr;
        v.ns++;
    }
    // (a mode died away rests at zero: no denormals in the long tails; a bad value, the object starts again quiet)
    for (int k = 0; k < nM; ++k) if (isBad (a.y1[k]) || isBad (a.y2[k])) { for (int q = 0; q < AL_M; ++q) a.y1[q] = a.y2[q] = 0.0f; a.fbD = 0.0f; break; }
    for (int k = 0; k < AL_M; ++k) if (absf (a.y1[k]) + absf (a.y2[k]) < 1e-14f) a.y1[k] = a.y2[k] = 0.0f;
    for (int k = 0; k < AL_ADD; ++k) if (absf (a.aRe[k]) + absf (a.aIm[k]) < 1e-12f) a.aRe[k] = a.aIm[k] = 0.0f;
    if (absf (a.nLp) + absf (a.nBp) + absf (a.nHl) < 1e-14f) a.nLp = a.nBp = a.nHl = 0.0f;
    if (absf (a.dcy) < 1e-14f) a.dcy = 0.0f;
    if (absf (a.cLp) < 1e-14f) a.cLp = 0.0f;
    if (absf (a.bHp) < 1e-14f) a.bHp = 0.0f;
    if (absf (a.fbD) < 1e-14f) a.fbD = 0.0f;
    for (int st = 0; st < AL_PH; ++st) { if (absf (a.apx[st]) + absf (a.apy[st]) < 1e-14f) a.apx[st] = a.apy[st] = 0.0f; }
    if (absf (a.apFb) < 1e-14f || isBad (a.apFb)) a.apFb = 0.0f;
    if (isBad (a.dcy) || isBad (a.nBp) || isBad (a.nLp) || isBad (a.nHl) || isBad (a.cLp) || isBad (a.dL2) || isBad (a.dvX))
    { a.dcx = a.dcy = a.nBp = a.nLp = a.nHl = a.cLp = a.dL1 = a.dL2 = a.dHold = a.dvX = 0.0f; }
    a.lvl = maxf (lvl, a.lvl * 0.97f);
    const bool shut = a.t >= cutT && a.cutG < 1e-4f;
    const bool pending = ! shut && ((nOn && a.nStage < 2) || a.bMode != 0);
    v.level = shut ? 0.0f : pending ? maxf (a.lvl, 0.01f) : a.lvl;
}

// SPACE: the machine's own reverb on the track's way out (after its TONE, as CHOIR's): the VERB machine's network, set
// for metal (from a box whose lines last a millisecond, a ringing, combed space, to a hall), the middle back into the
// track, the sides with WIDE's; CUT shuts it with the voice. It sleeps when it has nothing left to ring (its memory
// kept: no clearing when it wakes)
void Engine::alloySpace (Track& t, int ti, const float* P, float* out, float* side, int n)
{
    FxState& f = fx[ti];
    const float target = P[PA_VERB];
    if (! t.cvOn)
    {
        float pk = 0.0f;
        for (int i = 0; i < n; ++i) pk = maxf (pk, absf (out[i]));
        if (target <= 0.0005f || pk < 1e-7f) return;
        if (f.kind != CHOIR_KIND || f.sr != sr) { fxSetup (f, M_FXVERB); f.kind = CHOIR_KIND; }
        t.cvOn = 1; t.cvQuiet = 0.0f; t.cvGate = 1.0f;
        if (t.cvMix <= 0.0f) t.cvMix = target;
    }
    if (f.kind != CHOIR_KIND) { fxSetup (f, M_FXVERB); f.kind = CHOIR_KIND; }
    float Pv[NUM_PARAMS];
    for (int k = 0; k < NUM_PARAMS; ++k) Pv[k] = 0.0f;
    const float dmp = P[PA_DAMP];
    Pv[P_TUNE] = 0.5f + 0.2f * (0.5f - dmp);              // (the tail's tilt: brighter undamped)
    Pv[P_B] = 0.75f * P[PA_ROOM];                         // SIZE: lines of 0.6 .. 1 ms (a metal box) .. a hall
    // DECAY: DCAY a time (0.08 .. 4 s), the lines' gain a pass for it at this size (a box's lines pass a thousand
    // times a second: they need almost none of a loss to ring), back onto the VERB machine's knob
    {
        const float scale = 0.004f * powf_ (250.0f, Pv[P_B]);
        const float L = (203.0f * scale + 8.375f * powf_ (scale, 0.7f)) * 0.001f * sr;
        const float rt = 0.08f * powf_ (50.0f, P[PA_DCAY]);
        const float g = exp2f_ (-3.0f * L / (rt * sr) * 3.3219281f);
        const float u = powf_ (maxf (1.0f - g, 1e-6f) / 0.92f, 0.6666667f);
        Pv[P_C] = clampf (0.8f * (1.0f - u), 0.0f, 0.79f);
    }
    Pv[P_D] = 0.05f + 0.7f * dmp;                         // ABSRB
    Pv[P_XA] = 0.1f + 0.6f * P[PA_RATE];                  // the lines' movement: RATE's
    Pv[P_XB] = 0.15f;
    Pv[P_XC] = 0.0f;                                      // (no pre-delay)
    Pv[P_XD] = 0.0f;                                      // (sines, each line an eighth of a turn from the next)
    Pv[P_XE] = 0.5f / 3.0f;                               // (ACT: IN, no gesture)
    Pv[P_XF] = 0.2f;                                      // BASS
    Pv[P_XG] = P[PA_SHFT];                                // SHFT (the VERB machine's thirteen)
    float g1[CHUNK];
    for (int i = 0; i < n; ++i) g1[i] = 1.0f;
    fxVerb (f, Pv, out, out, g1, wa, wb, n);
    // CUT: the space shut when the newest hit's voice is (its gate's time past, or the voice gone)
    float gT = 1.0f;
    const float cutV = P[PA_CUT], cutT = 0.02f * exp2f_ (cutV * 6.2f);
    if (cutV > 0.002f)
    {
        const Voice& vc = t.voice[t.cur];
        gT = vc.active && vc.machine == M_ALLOY && vc.al.t < cutT ? 1.0f : 0.0f;
    }
    const float k = 1.0f - expf_ (-1.0f / (0.03f * sr)), kG = 1.0f - expf_ (-1.0f / ((0.002f + 0.05f * minf (cutT, 1.5f)) * sr));
    float pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        t.cvMix += (target - t.cvMix) * k;
        t.cvGate += (gT - t.cvGate) * kG;
        const float m = t.cvMix, wet = 0.6f * m * t.cvGate;
        const float mid = 0.5f * (wa[i] + wb[i]), sd = 0.5f * (wa[i] - wb[i]);
        out[i] = out[i] * (1.0f - 0.3f * m * m) + wet * mid;
        side[i] = wet * sd;
        pk = maxf (pk, absf (wa[i]) + absf (wb[i]));
    }
    // asleep when its tail has died away (or its mix has)
    if (pk < 3e-6f || (target <= 0.0f && t.cvMix < 1e-4f)) t.cvQuiet += (float) n; else t.cvQuiet = 0.0f;
    if (t.cvQuiet > 1.5f * sr || (target <= 0.0f && t.cvMix < 1e-4f)) { t.cvOn = 0; t.cvQuiet = 0.0f; if (target <= 0.0f) t.cvMix = 0.0f; }
}

}  // namespace sk
