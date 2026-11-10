// IMPAKT — FUZZ, the multiband germanium fuzz (an FX machine, v68; see sk_fx.h). Own implementation.
//
// The circuit (the two-transistor germanium fuzz of 1966, as its schematic is published): Q1's base takes the input
// through C1 (2.2 uF) and, through R4 (100k), the feedback from Q2's emitter; Q1's emitter is grounded, its collector
// goes to the supply through R1 (33k) and drives Q2's base directly; Q2's collector goes to the supply through R3 (8.2k)
// then R2 (470), the output taken between the two through C3 (10 nF) into the 500k volume; Q2's emitter goes to ground
// through the 1k FUZZ pot, whose wiper is bypassed to ground by C2 (20 uF): at full FUZZ no audio feedback is left, at
// the other end all of it. Two PNP germanium transistors on -9 V (solved here mirrored, NPN on +9 V, the signs turned
// back at the end): Ebers-Moll, Is 0.3 uA, n Vt 31 mV (their low knee, their leakage), beta 75 and 120, reverse 4. In
// front, a pickup's 10k (the circuit's input is shaped by what drives it).
//
// At audio rates C1, C2 and C3 hold their charge: for each BIAS and pot setting the circuit's operating point is solved
// (Newton on its four nodes), then its output and the current it draws from the input for every input voltage, from
// that point out (continuation): two curves. Running, a band's signal (its level into the circuit set by FUZZ and the
// band's drive) reads them, less the charge C1 has taken (the current it draws, integrated: under a loud sound the
// base-emitter junction rectifies, C1 charges, the bias shifts — the sputter and the gating of the real one; it settles
// back with the input's time constant). The curves are built once (about 20 ms), shared.
#include "sk_engine.h"
#include "sk_send.h"          // (setLp4 / lp4: SUB's pure octaves)

namespace sk {

namespace {

constexpr float FZ_A = 0.05f, FZ_UM = 0.995f;          // the input axis: x = A u / (1 - |u|), u on a grid up to UM (±9.95 V)
constexpr float FZ_C1 = 2.2e-6f;
// a band at this level (of full scale) comes out at it, whatever its drive: the level of a band of a track at its usual
// level (the presets bring a hit to about -21 LUFS), -28 dBFS; so a saturated wall comes out a few LU over its source,
// not 25 (at 0.4 it did: every FX track's MIX, then the master, clipped)
constexpr float FZ_AREF = 0.04f;
// SUB (v71): its level at a knob's middle (-23 dBFS; at the top four times it, -11 dBFS)
constexpr float FZ_AREF_SUB = 0.07f;

struct FzTables
{
    bool  ready;
    float out[FZ_BIAS][FZ_POT][FZ_N], cur[FZ_BIAS][FZ_POT][FZ_N];
    float g0[FZ_BIAS][FZ_POT], vp[FZ_BIAS][FZ_POT];   // the small-signal gain at rest, the swing
};
FzTables FT;

// exp in double, the same everywhere (the page, the plugin, the tests build the same curves): range reduction, then the
// series
inline double fzExp (double x)
{
    if (x < -700.0) return 0.0;
    if (x > 700.0) x = 700.0;
    const double kd = __builtin_floor (x / 0.69314718055994530942 + 0.5);
    const double r = x - kd * 0.69314718055994530942;
    double s = 1.0, t = 1.0;
    for (int i = 1; i < 18; ++i) { t *= r / i; s += t; }
    // 2^k by squaring (exact: powers of two; no 64-bit integers, which the plain-JS build cannot lower)
    int k = (int) kd; double p = 1.0, b = k < 0 ? 0.5 : 2.0;
    if (k < 0) k = -k;
    while (k) { if (k & 1) p *= b; b *= b; k >>= 1; }
    return s * p;
}
inline double fzMin (double a, double b) { return a < b ? a : b; }
inline double fzMax (double a, double b) { return a > b ? a : b; }
inline double fzAbs (double a) { return __builtin_fabs (a); }

struct Em { double ic, ib, ic_be, ic_bc, ib_be, ib_bc; };
inline Em ebersMoll (double vbe, double vbc, double bF)
{
    constexpr double Is = 3e-7, nVt = 0.031, bR = 4.0;
    const double ef = fzExp (fzMin (40.0, fzMax (-80.0, vbe / nVt))), er = fzExp (fzMin (40.0, fzMax (-80.0, vbc / nVt)));
    const double gf = Is * ef / nVt, gr = Is * er / nVt;
    return { Is * (ef - er) - Is / bR * (er - 1.0), Is / bF * (ef - 1.0) + Is / bR * (er - 1.0), gf, -gr - gr / bR, gf / bF, gr / bR };
}

struct Ckt
{
    double Vs = 9.0, R1 = 33e3, R2 = 470.0, R3 = 8.2e3, R4 = 100e3, Rs = 10e3, Rv = 500e3, Rp = 1e3, Rt = 500.0;
    double b1 = 75.0, b2 = 120.0;
};

// the nodes b (Q1's base), c1 (Q1's collector, Q2's base), e2, c2. ac: the input (veff, from the operating point) and the
// capacitors' held voltages (bq: the base at rest; wq: the wiper; oq: the output node)
bool solve (const Ckt& k, bool ac, double veff, double bq, double wq, double oq, double* X)
{
    for (int it = 0; it < 300; ++it)
    {
        const double b = X[0], c1 = X[1], e2 = X[2], c2 = X[3];
        const Em q1 = ebersMoll (b, b - c1, k.b1), q2 = ebersMoll (c1 - e2, c1 - c2, k.b2);
        const double S = 1.0 / k.R2 + 1.0 / k.R3 + (ac ? 1.0 / k.Rv : 0.0);
        const double o = (k.Vs / k.R2 + c2 / k.R3 + (ac ? oq / k.Rv : 0.0)) / S, dO = (1.0 / k.R3) / S;
        const double Ic1b = q1.ic_be + q1.ic_bc, Ic1c = -q1.ic_bc, Ib1b = q1.ib_be + q1.ib_bc, Ib1c = -q1.ib_bc;
        const double Ic2c1 = q2.ic_be + q2.ic_bc, Ic2e = -q2.ic_be, Ic2c2 = -q2.ic_bc;
        const double Ib2c1 = q2.ib_be + q2.ib_bc, Ib2e = -q2.ib_be, Ib2c2 = -q2.ib_bc;
        const double Re = ac ? fzMax (k.Rt, 0.5) : k.Rp, ve = ac ? wq : 0.0;
        double A[4][5];
        A[0][4] = -((ac ? (veff - (b - bq)) / k.Rs : 0.0) - q1.ib - (b - e2) / k.R4);
        A[0][0] = (ac ? -1.0 / k.Rs : 0.0) - Ib1b - 1.0 / k.R4; A[0][1] = -Ib1c; A[0][2] = 1.0 / k.R4; A[0][3] = 0.0;
        A[1][4] = -((k.Vs - c1) / k.R1 - q1.ic - q2.ib);
        A[1][0] = -Ic1b; A[1][1] = -1.0 / k.R1 - Ic1c - Ib2c1; A[1][2] = -Ib2e; A[1][3] = -Ib2c2;
        A[2][4] = -((q2.ic + q2.ib) - (e2 - b) / k.R4 - (e2 - ve) / Re);
        A[2][0] = 1.0 / k.R4; A[2][1] = Ic2c1 + Ib2c1; A[2][2] = Ic2e + Ib2e - 1.0 / k.R4 - 1.0 / Re; A[2][3] = Ic2c2 + Ib2c2;
        A[3][4] = -((o - c2) / k.R3 - q2.ic);
        A[3][0] = 0.0; A[3][1] = -Ic2c1; A[3][2] = -Ic2e; A[3][3] = (dO - 1.0) / k.R3 - Ic2c2;
        for (int c = 0; c < 4; ++c)
        {
            int p = c;
            for (int r = c + 1; r < 4; ++r) if (fzAbs (A[r][c]) > fzAbs (A[p][c])) p = r;
            if (p != c) for (int j = 0; j < 5; ++j) { const double t = A[c][j]; A[c][j] = A[p][j]; A[p][j] = t; }
            if (A[c][c] == 0.0) return false;
            for (int r = c + 1; r < 4; ++r) { const double f = A[r][c] / A[c][c]; for (int j = c; j < 5; ++j) A[r][j] -= f * A[c][j]; }
        }
        double d[4], mx = 0.0;
        for (int i = 3; i >= 0; --i) { double s = A[i][4]; for (int j = i + 1; j < 4; ++j) s -= A[i][j] * d[j]; d[i] = s / A[i][i]; mx = fzMax (mx, fzAbs (d[i])); }
        const double sc = mx > 0.2 ? 0.2 / mx : 1.0;          // (steps of 0.2 V at most: the junctions stay in reach)
        for (int i = 0; i < 4; ++i) X[i] += d[i] * sc;
        if (mx < 1e-10) return true;
    }
    return false;
}

inline float axisX (int i) { const float u = (float) (i - FZ_N / 2) / (float) (FZ_N / 2) * FZ_UM; return FZ_A * u / (1.0f - absf (u)); }

void buildFuzz()
{
    if (FT.ready) return;
    Ckt k;
    for (int j = 0; j < FZ_BIAS; ++j)
    {
        // BIAS: to the left the battery sags (9 V down to 3.4 V), to the right R1 shrinks (33k down to 17.5k: Q1 hotter)
        k.Vs = j < 4 ? 3.4 + (9.0 - 3.4) * j / 4.0 : 9.0;
        k.R1 = j > 4 ? 33e3 * fzExp (-0.6343066805370119 * (j - 4) / 4.0) : 33e3;     // (33k x (17.5 / 33)^t)
        double X[4] = { 0.15, 0.6, 0.5, k.Vs * 0.5 };
        solve (k, false, 0.0, 0.0, 0.0, 0.0, X);
        const double bq = X[0], e2q = X[2];
        const double oq = (k.Vs / k.R2 + X[3] / k.R3) / (1.0 / k.R2 + 1.0 / k.R3);
        for (int f = 0; f < FZ_POT; ++f)
        {
            // the pot: its unbypassed part (log-ish steps towards full FUZZ), the wiper's voltage held by C2
            k.Rt = 1000.0 * (1.0 - f / 8.0) * (1.0 - f / 8.0);
            const double wq = e2q * (1.0 - k.Rt / 1000.0);
            float* out = FT.out[j][f]; float* cur = FT.cur[j][f];
            for (int dir = 1; dir >= -1; dir -= 2)
            {
                double Y[4] = { X[0], X[1], X[2], X[3] };
                for (int i = FZ_N / 2; i >= 0 && i < FZ_N; i += dir)
                {
                    const double x = (double) axisX (i), veff = -x;      // (mirrored: the real input is -veff)
                    solve (k, true, veff, bq, wq, oq, Y);
                    const double o = (k.Vs / k.R2 + Y[3] / k.R3 + oq / k.Rv) / (1.0 / k.R2 + 1.0 / k.R3 + 1.0 / k.Rv);
                    out[i] = (float) -(o - oq);
                    cur[i] = (float) -((veff - (Y[0] - bq)) / k.Rs);
                }
            }
            const int c = FZ_N / 2;
            FT.g0[j][f] = (out[c + 1] - out[c - 1]) / (axisX (c + 1) - axisX (c - 1));
            FT.vp[j][f] = 0.5f * (absf (out[0]) + absf (out[FZ_N - 1]));
        }
    }
    FT.ready = true;
}

inline float dbToLin (float db) { return exp2f_ (db * (1.0f / 6.0206f)); }

// v73, CIRC (S5): the circuit each band goes through. FUZZ (the one above; the default, so a project made before keeps
// it), then from the gentlest: PRE, a preamp stage pushed (soft, a little asymmetric: even harmonics); SAT, a
// saturation (a softer knee, symmetric: odd harmonics, glue); XCIT, an exciter (only what is over 3 kHz is driven, what
// it makes added to the band as it is); MID, a mid drive (the mids pushed into soft diodes, lows and highs less: a mid
// hump, tight lows, then smoothed); XSTR, one silicon transistor (its exponential: the 2nd harmonic from the start, then
// a hard clip on one side, its coupling charge under overload: the sputter, the gating); GAIN, three stages in a row,
// their DC and rumble taken out between them (compressed, high gain); BENT, a bent circuit (a dead zone around zero:
// crossover; a slow output: slew; a rail that sticks for a random while when it is hit: spit, stutter). All in the
// same units (the knee at 1), each band's drive, SUST and BIAS as for FUZZ
enum { FZC_FUZZ, FZC_PRE, FZC_SAT, FZC_XCIT, FZC_MID, FZC_XSTR, FZC_GAIN, FZC_BENT, FZ_CIRCS };
// each one's slope at rest (per band, where its pre-filter makes them differ) and its swing: the make-up
const float FZC_G0[FZ_CIRCS][3] = { { 1, 1, 1 }, { 0.92f, 0.92f, 0.92f }, { 1, 1, 1 }, { 1, 1, 1 }, { 0.6f, 1.5f, 0.8f },
                                    { 0.3f, 0.3f, 0.3f }, { 6, 6, 6 }, { 1, 1, 1 } };
const float FZC_VP[FZ_CIRCS] = { 1, 1.0f, 1.0f, 1.0f, 1.1f, 1.0f, 1.0f, 1.0f };
// (the make-up of the other circuits: unity while they are not saturated, a saturated band brought to about +7 dB over
// AREF, not to AREF as FUZZ does: in a loud mix (bands near -20 dBFS), a circuit pushed hard comes out about as loud as
// it went in, a quiet band (AREF) at their top up to 11 dB louder; its corner sharper than FUZZ's: a / sqrt (1 +
// (a / swing)^2))
constexpr float FZC_OUT = 0.55f;
// DRIV on FUZZ (v73): from 60 % on as it was (v68); under it the circuit stays where it breaks up (as DRIV 35 % did,
// up to 60 %: the fuzz is a threshold, clean under it, all of it over), blended with the band as it is, more and
// more of it (6 % at 0, all of it at 60 %: the harmonics grow from a few per cent)
constexpr float FZ_D0 = 0.35f, FZ_W0 = 0.06f;
// each circuit's drive against the others (dB: how close to the knee DRIV 0 is)
const float FZC_OFF[FZ_CIRCS] = { 0.0f, 0.0f, -2.0f, 0.0f, -8.0f, 0.0f, -4.0f, 0.0f };

// the per-block coefficients of the other circuits
struct FzCk
{
    float hb, tb, sb, mb, xb, rail, dz;                 // BIAS: the offset (hot) and each shaper's value at it; the rail, the dead zone (starved)
    float aXh, aXe, xa;                                 // XCIT: its high-pass (3 kHz), the second one (2 kHz), how much of what it makes
    float aMh, aMl, aMp;                                // MID: the band's edges (2 kHz, 300 Hz), the smoothing (5 kHz)
    float aG1, aG2, aG3, g2, g3;                        // GAIN: the DC and rumble out (40, 30 Hz), the smoothing (6 kHz), the 2nd and 3rd stages' gains
    float kCh, kDis;                                    // XSTR: the coupling charge under overload, its discharge
    float bDz, slew, latP, latL;                        // BENT: the crossover's dead zone, the slew, the chance a rail sticks, how long (at 4x)
};
inline float deadZone (float x, float dz) { return x > dz ? x - dz : x < -dz ? x + dz : 0.0f; }
inline float satS (float u) { return u / sqrtf_ (1.0f + u * u); }
inline float midS (float u) { return u > 0.0f ? u / (1.0f + u) : u / (1.0f - 0.6f * u); }
inline u32 fzRand (u32& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }

// one band (v: into the circuit, the knee at 1; x: the band as it is) through a circuit other than FUZZ: its output,
// before the make-up (XCIT: after it, the band and what it adds)
inline float fzCircuit (int circ, FxState& f, int c, int k, float x, float sc, const FzCk& q)
{
    const float v = x * sc;
    float* s = f.zCs[c][k];
    switch (circ)
    {
        case FZC_PRE:
            return q.rail * (tanhf_ (deadZone (v, q.dz) / q.rail + 0.12f + q.hb) - q.tb);
        case FZC_SAT:
            return q.rail * (satS (deadZone (v, q.dz) / q.rail + q.hb) - q.sb);
        case FZC_XCIT:
        {
            // (two one-pole high-passes at 3 kHz: the lows never reach the shaper)
            s[0] += (x - s[0]) * q.aXh; const float h1 = x - s[0];
            s[1] += (h1 - s[1]) * q.aXh;
            // (rectified: its even harmonics, none of its fundamental, which stays as it was; BIAS hot: some of it,
            // saturated, more presence)
            const float hn = deadZone ((h1 - s[1]) * sc, q.dz) / q.rail;
            const float e = tanhf_ (absf (hn)) + q.hb * tanhf_ (hn);
            s[2] += (e - s[2]) * q.aXe;
            return x + (e - s[2]) * q.rail * q.xa * FZ_AREF;
        }
        case FZC_MID:
        {
            s[0] += (v - s[0]) * q.aMh; s[1] += (v - s[1]) * q.aMl;
            const float u = 0.35f * v + 1.8f * (s[0] - s[1]);
            const float d = q.rail * (midS (deadZone (u, q.dz) / q.rail + q.hb) - q.mb);
            s[2] += (d - s[2]) * q.aMp;
            return s[2];
        }
        case FZC_XSTR:
        {
            const float u = deadZone (0.3f * v - s[0], q.dz) / q.rail + q.hb;
            const float e = expf_ (minf (u, 4.0f)) - 1.0f;
            if (e > 1.0f) s[0] = minf (s[0] + minf (e - 1.0f, 4.0f) * q.kCh, 3.0f);
            s[0] *= q.kDis;
            return q.rail * (minf (e, 1.0f) - q.xb);
        }
        case FZC_GAIN:
        {
            s[0] += (v - s[0]) * q.aG1;
            const float s1 = tanhf_ (deadZone (v - s[0], q.dz) / q.rail + q.hb) - q.tb;
            s[1] += (s1 - s[1]) * q.aG2;
            const float s3 = tanhf_ (q.g3 * tanhf_ (q.g2 * (s1 - s[1])));
            s[2] += (s3 - s[2]) * q.aG3;
            return q.rail * s[2];
        }
        default:   // FZC_BENT
        {
            // (its rail: a hard knee, but a knee: u / (1 + u^4)^(1/4))
            const float u = deadZone (v + q.hb, q.bDz + q.dz) / q.rail, u2 = u * u;
            float o = q.rail * u / sqrtf_ (sqrtf_ (1.0f + u2 * u2));
            if (f.zLat[c][k] > 0) { --f.zLat[c][k]; o = f.zLatV[c][k]; }
            else if (absf (u) > 1.5f && (float) (fzRand (f.zRng) >> 8) * (1.0f / 16777216.0f) < q.latP)
            {
                f.zLat[c][k] = (int) (q.latL * (0.2f + 0.8f * (float) (fzRand (f.zRng) >> 8) * (1.0f / 16777216.0f)));
                f.zLatV[c][k] = o;
            }
            s[0] += clampf (o - s[0], -q.slew, q.slew);
            return s[0];
        }
    }
}

} // namespace

void fuzzTables() { buildFuzz(); }

void Engine::fxFuzz (FxState& f, const float* P, const float* xL, const float* xR, const float* g, float* wL, float* wR, int n)
{
    buildFuzz();
    const float sr4 = 4.0f * sr;
    // the curves for this BIAS (STRV drains the battery) and FUZZ (the pot over its first 70 %), blended from the tables
    const int act = idxOf (P[P_XE], 2);
    const float bias = clampf (P[P_C] * (act == 1 ? 1.0f - f.gestF : 1.0f), 0.0f, 1.0f), fuzz = clampf (P[P_B], 0.0f, 1.0f);
    // v73: the circuit (a change: its states start over); DRIV under 60 %: more of it on FUZZ (FZ_LOWDB, FZ_LOWPOT)
    const int circ = idxOf (P[P_S5], FZ_CIRCS);
    if (circ != f.zCirc)
    {
        f.zCirc = circ;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 3; ++k) { for (int q = 0; q < 4; ++q) f.zCs[c][k][q] = 0.0f; f.zLat[c][k] = 0; }
    }
    const float dc = fuzz < 0.6f ? FZ_D0 + fuzz * ((0.6f - FZ_D0) / 0.6f) : fuzz, fw = fuzz < 0.6f ? FZ_W0 + (1.0f - FZ_W0) * (fuzz / 0.6f) * (fuzz / 0.6f) : 1.0f;
    const float bj = bias * (float) (FZ_BIAS - 1), kf = minf (1.0f, dc / 0.7f) * (float) (FZ_POT - 1);
    float g0, vp;
    {
        const int j0 = mini ((int) bj, FZ_BIAS - 2), k0 = mini ((int) kf, FZ_POT - 2);
        const float fj = bj - (float) j0, fk = kf - (float) k0;
        const float w00 = (1.0f - fj) * (1.0f - fk), w01 = (1.0f - fj) * fk, w10 = fj * (1.0f - fk), w11 = fj * fk;
        if (absf (bj - f.zKey[0]) > 1e-4f || absf (kf - f.zKey[1]) > 1e-4f)
        {
            f.zKey[0] = bj; f.zKey[1] = kf;
            const float *a = FT.out[j0][k0], *b = FT.out[j0][k0 + 1], *c = FT.out[j0 + 1][k0], *d = FT.out[j0 + 1][k0 + 1];
            const float *ca = FT.cur[j0][k0], *cb = FT.cur[j0][k0 + 1], *cc = FT.cur[j0 + 1][k0], *cd = FT.cur[j0 + 1][k0 + 1];
            for (int i = 0; i < FZ_N; ++i)
            {
                f.zOut[i] = w00 * a[i] + w01 * b[i] + w10 * c[i] + w11 * d[i];
                f.zCur[i] = w00 * ca[i] + w01 * cb[i] + w10 * cc[i] + w11 * cd[i];
            }
        }
        g0 = w00 * FT.g0[j0][k0] + w01 * FT.g0[j0][k0 + 1] + w10 * FT.g0[j0 + 1][k0] + w11 * FT.g0[j0 + 1][k0 + 1];
        vp = w00 * FT.vp[j0][k0] + w01 * FT.vp[j0][k0 + 1] + w10 * FT.vp[j0 + 1][k0] + w11 * FT.vp[j0 + 1][k0 + 1];
    }
    f.zBias = bias;
    // the level into the circuit (volts a full-scale signal makes): FUZZ, -26 .. +34 dB; each band's drive -48 .. +18 dB
    // from it (at 0: CLEAN, the band does not go through the circuit); the make-up that brings a band at AREF back to AREF (clean: its gain; saturated: its swing)
    const float gin = dbToLin (-26.0f + 60.0f * dc);
    const float drv[3] = { P[P_XA], P[P_XB], P[P_XC] };
    // (v73, the other circuits: a band at AREF reaches -6 dB of the knee at DRIV 0, +42 dB at its top, faster and
    // faster: -6 + 16 d + 32 d^2 dB; their make-up from their slope and swing)
    const float gD = dbToLin (-6.0f + 16.0f * fuzz + 32.0f * fuzz * fuzz + FZC_OFF[circ]);
    const float gGain = (1.0f + 2.0f * fuzz) * (1.0f + fuzz);                   // (GAIN's slope at rest: its stages')
    for (int k = 0; k < 3; ++k)
    {
        if (circ == FZC_FUZZ)
        {
            f.zGb[k] = drv[k] < 0.02f ? 0.0f : gin * dbToLin (-48.0f + 66.0f * drv[k]);        // (0: CLEAN, the band past the circuit)
            const float a = absf (g0) * f.zGb[k] * FZ_AREF, m = a * vp / (a + vp + 1e-9f);
            f.zMk[k] = FZ_AREF / maxf (m, 1e-6f);
        }
        else
        {
            const float rel = dbToLin (-48.0f + 66.0f * drv[k]);
            f.zGb[k] = drv[k] < 0.02f ? 0.0f : gD * rel / FZ_AREF;
            const float a = (circ == FZC_GAIN ? gGain : FZC_G0[circ][k]) * gD * rel, vpE = FZC_VP[circ] * FZC_OUT, m = a / sqrtf_ (1.0f + (a / vpE) * (a / vpE));
            f.zMk[k] = FZ_AREF / maxf (m, 1e-6f);
        }
    }
    // their BIAS: right of the middle an offset (hotter: even harmonics), left the rail sags and a dead zone opens (starved:
    // it clips sooner, the quiet parts sputter); their filters (one-pole, at 4x)
    FzCk ck;
    {
        const float hot = bias > 0.5f ? 1.2f * (bias - 0.5f) : 0.0f, starve = bias < 0.5f ? (0.5f - bias) * 2.0f : 0.0f;
        auto op = [&] (float hz) { return 1.0f - expf_ (-TWO_PI * hz / sr4); };
        ck.hb = hot; ck.rail = 1.0f - 0.6f * starve; ck.dz = 0.12f * starve;
        ck.tb = tanhf_ ((circ == FZC_PRE ? 0.12f : 0.0f) + hot); ck.sb = satS (hot); ck.mb = midS (hot);
        ck.xb = circ == FZC_XSTR ? minf (expf_ (hot) - 1.0f, 1.0f) : tanhf_ (hot);
        ck.aXh = op (3000.0f); ck.aXe = op (2000.0f); ck.xa = 0.15f + 1.5f * fuzz;
        ck.aMh = op (2000.0f); ck.aMl = op (300.0f); ck.aMp = op (5000.0f);
        ck.aG1 = op (40.0f); ck.aG2 = op (30.0f); ck.aG3 = op (6000.0f); ck.g2 = 1.0f + 2.0f * fuzz; ck.g3 = 1.0f + fuzz;
        ck.kCh = 0.004f; ck.kDis = expf_ (-1.0f / (0.04f * sr4));
        ck.bDz = 0.01f + 0.3f * fuzz * fuzz; ck.slew = 0.06f + 2.0f * (1.0f - fuzz) * (1.0f - fuzz) * (1.0f - fuzz);
        ck.latP = 0.002f * fuzz * fuzz; ck.latL = (0.0003f + 0.012f * fuzz) * sr4;
    }
    // the crossovers: XLO 60 .. 600 Hz, XHI 0.9 .. 8.1 kHz (at least 1.5 x XLO), Linkwitz-Riley 24 dB/oct
    const float xlo = 60.0f * powf_ (10.0f, P[P_XD]);
    const float xhi = maxf (900.0f * powf_ (9.0f, P[P_XF]), 1.5f * xlo);
    if (absf (xlo - f.zXlo) > 0.01f || absf (xhi - f.zXhi) > 0.01f)
    {
        f.zXlo = xlo; f.zXhi = xhi;
        for (int c = 0; c < 2; ++c)
        {
            for (int q = 0; q < 4; ++q) f.zX[c][q].set (xlo, 0.70710678f, sr4);
            for (int q = 4; q < 9; ++q) f.zX[c][q].set (xhi, 0.70710678f, sr4);
        }
    }
    // SUST: each band's envelope (2 ms up, 150 ms down) lifts it into the circuit, up to +34 dB
    const float sust = clampf (P[P_D], 0.0f, 1.0f);
    const float kSa = 1.0f - expf_ (-1.0f / (0.002f * sr4)), kSr = 1.0f - expf_ (-1.0f / (0.15f * sr4));
    // GATE: keyed on the source (its peak, 1 ms up, 12 ms down): opens above the threshold, closes 6 dB under it after
    // 30 ms, in 0.3 ms / 40 ms
    const float gk = P[P_XG];
    const bool gateOn = gk >= 0.02f;
    const float thr = dbToLin (-72.0f + 50.0f * gk);
    const float kGa = 1.0f - expf_ (-1.0f / (0.001f * sr)), kGr = 1.0f - expf_ (-1.0f / (0.012f * sr));
    const float kOa = 1.0f - expf_ (-1.0f / (0.0003f * sr)), kOr = 1.0f - expf_ (-1.0f / (0.04f * sr));
    // TONE: a tilt of +-9 dB about 1 kHz; the output's coupling (C3 into 500k: 31 Hz)
    const float tl = (P[P_TUNE] - 0.5f) * 2.0f, gh = dbToLin (9.0f * tl), gl = 1.0f / gh;
    const float kT = 1.0f - expf_ (-TWO_PI * 1000.0f / sr4), kH = 1.0f - expf_ (-TWO_PI * 31.0f / sr4);
    const float dtC = 1.0f / (sr4 * FZ_C1);
    const float km = 1.0f - expf_ (-1.0f / (0.004f * sr));
    // SUB (v71, the SUB page: S1 .. S4): an octave and two under what the LOW band holds, found as the analog octavers
    // find them: the band's fundamental (the source in mono, low-passed at XLO) squared by a comparator with hysteresis,
    // halved by a flip-flop, halved again by a second one. PURE (GRIT at 0): each flip-flop through a 4-pole low-pass at
    // its own frequency (the comparator's period, smoothed), what is left the square's fundamental, a sine, on the band's
    // envelope; GRIT: the band times the flip-flop (the octave its own waveform makes), through a 2-pole low-pass. Like
    // the fuzz, its level hardly follows the band's (SUB at its top: 4 x FZ_AREF_SUB, -11 dBFS;
    // SUB1, SUB2: the square of the knob), down to where the band fades (-50 dBFS); without edges for 2.5 of its
    // periods (the band gone quiet, or too high to follow) it lets go in 8 ms. PATH: CLEAN, under the fuzz, past the
    // circuit, 86 samples late like it; FUZZ, into the LOW band's circuit. Off (both at 0) it costs nothing and changes
    // nothing
    const float sb1 = clampf (P[P_S1], 0.0f, 1.0f), sb2 = clampf (P[P_S2], 0.0f, 1.0f), grit = clampf (P[P_S3], 0.0f, 1.0f);
    const bool subOn = sb1 > 0.0005f || sb2 > 0.0005f, subPre = idxOf (P[P_S4], 2) == 1;
    const float sg1 = 4.0f * sb1 * sb1, sg2 = 4.0f * sb2 * sb2;
    float kEa = 0.0f, kEr = 0.0f, kLk = 0.0f;
    if (subOn)
    {
        kEa = 1.0f - expf_ (-1.0f / (0.002f * sr)); kEr = 1.0f - expf_ (-1.0f / (0.12f * sr)); kLk = 1.0f - expf_ (-1.0f / (0.008f * sr));
        f.zSubDet.set (xlo, 0.7071f, sr);
        const float fin = f.zSubPer > 0.0f ? clampf (sr / f.zSubPer, 12.0f, 1200.0f) : xlo;
        setLp4 (f.zSubP[0], 0.5f * fin, sr); setLp4 (f.zSubP[1], 0.25f * fin, sr);
        f.zSubG[0].set (1.5f * fin, 0.7071f, sr); f.zSubG[1].set (0.75f * fin, 0.7071f, sr);
    }
    const float xmax = axisX (FZ_N - 1);
    float pk[3] = { 0.0f, 0.0f, 0.0f }, drift = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        // the gate
        const float key = maxf (absf (xL[i]), absf (xR[i]));
        f.zGEnv += (key - f.zGEnv) * (key > f.zGEnv ? kGa : kGr);
        if (! gateOn || f.zGEnv > thr) { f.zGOpen = 1.0f; f.zGHold = 0.03f * sr; }
        else if (f.zGEnv < 0.5f * thr) { if (f.zGHold > 0.0f) f.zGHold -= 1.0f; else f.zGOpen = 0.0f; }
        f.zGGain += (f.zGOpen - f.zGGain) * (f.zGOpen > f.zGGain ? kOa : kOr);
        f.zMix += (P[P_E] - f.zMix) * km;
        const float wet = g[i] * f.zMix;
        float sub = 0.0f;
        if (subOn)
        {
            float lp, bp, hp;
            f.zSubDet.tick (0.5f * (xL[i] + xR[i]), lp, bp, hp);
            const float d = lp, ad = absf (d);
            f.zSubEnv += (ad - f.zSubEnv) * (ad > f.zSubEnv ? kEa : kEr);
            f.zSubCnt += 1.0f;
            const float hy = 0.1f * f.zSubEnv;
            if (f.zSubCmp < 0 ? (d > hy && f.zSubEnv > 1e-5f) : d < -hy)
            {
                f.zSubCmp = -f.zSubCmp;
                if (f.zSubCmp > 0)
                {
                    // a rising edge: a period of the band, the flip-flops
                    const float T = f.zSubCnt; f.zSubCnt = 0.0f;
                    if (T > sr / 1200.0f && T < sr / 12.0f) f.zSubPer = f.zSubPer > 0.0f ? f.zSubPer + (T - f.zSubPer) * 0.3f : T;
                    f.zSubF1 = -f.zSubF1;
                    if (f.zSubF1 > 0) f.zSubF2 = -f.zSubF2;
                }
            }
            const float q1 = (float) f.zSubF1, q2 = (float) f.zSubF2;
            const float p1 = lp4 (f.zSubP[0], q1), p2 = lp4 (f.zSubP[1], q2);
            float g1, g2;
            f.zSubG[0].tick (d * q1, g1, bp, hp); f.zSubG[1].tick (d * q2, g2, bp, hp);
            // locked while the edges come (2.5 periods at most between two)
            const bool lk = f.zSubPer > 0.0f && f.zSubCnt < 2.5f * f.zSubPer && f.zSubEnv > 1e-5f;
            f.zSubLk += ((lk ? 1.0f : 0.0f) - f.zSubLk) * kLk;
            // (PURE: the square's fundamental through its low-pass, 4 / pi x 0.707, brought to 1; GRIT: the band's own
            // octave over its envelope, to about the same)
            const float e = f.zSubEnv, nrm = FZ_AREF_SUB * f.zSubLk * e / (e + 0.003f), gi = 1.4f / (e + 1e-6f);
            sub = nrm * (sg1 * (p1 * 1.11f + (g1 * gi - p1 * 1.11f) * grit) + sg2 * (p2 * 1.11f + (g2 * gi - p2 * 1.11f) * grit));
            f.zSubPk = maxf (f.zSubPk, absf (sub) * wet);
        }
        for (int c = 0; c < 2; ++c)
        {
            float x4[4], y4[4];
            f.zOs[c].up ((c ? xR : xL)[i], x4);
            // SUST's lift, once a host sample (from the envelope as it is)
            for (int k = 0; k < 3; ++k)
                f.zSusG[c][k] = sust > 0.0005f ? clampf (powf_ (maxf (f.zSus[c][k], 1e-4f) * (1.0f / 0.3f), -0.85f * sust), 0.5f, 50.0f) : 1.0f;
            Svf* X = f.zX[c];
            for (int j = 0; j < 4; ++j)
            {
                const float s = x4[j];
                float lp, bp, hp, lo, rest, mid, hi, h2;
                X[0].tick (s, lp, bp, hp); X[1].tick (lp, lo, bp, hp);              // LR4 low-pass at XLO
                X[2].tick (s, lp, bp, hp); X[3].tick (hp, lp, bp, rest);            // LR4 high-pass at XLO
                X[4].tick (rest, lp, bp, hp); X[5].tick (lp, mid, bp, hp);          // at XHI
                X[6].tick (rest, lp, bp, hp); X[7].tick (hp, lp, bp, hi);
                X[8].tick (lo, lp, bp, h2); lo = lo - 2.0f * 1.41421356f * bp;      // (the low band's all-pass at XHI: x - 2 k bp)
                if (subOn && subPre) lo += f.zSubPrev + (sub - f.zSubPrev) * (float) (j + 1) * 0.25f;
                const float band[3] = { lo, mid, hi };
                float y = 0.0f;
                for (int k = 0; k < 3; ++k)
                {
                    const float a = absf (band[k]);
                    f.zSus[c][k] += (a - f.zSus[c][k]) * (a > f.zSus[c][k] ? kSa : kSr);
                    if (f.zGb[k] == 0.0f) { y += band[k]; pk[k] = maxf (pk[k], absf (band[k])); continue; }
                    if (circ != FZC_FUZZ)
                    {
                        // (v73: the other circuits; XCIT gives the band back with what it adds)
                        float yk = fzCircuit (circ, f, c, k, band[k], f.zSusG[c][k] * f.zGb[k], ck);
                        if (circ != FZC_XCIT) yk *= f.zMk[k];
                        pk[k] = maxf (pk[k], absf (yk));
                        y += yk;
                        continue;
                    }
                    // into the circuit, less what C1 holds; its curves on the companded axis
                    float v = clampf (band[k] * f.zSusG[c][k] * f.zGb[k] - f.zDelta[c][k], -xmax, xmax);
                    const float u = v / (absf (v) + FZ_A);
                    float fi = (u * (1.0f / FZ_UM) + 1.0f) * (float) (FZ_N / 2);
                    fi = clampf (fi, 0.0f, (float) (FZ_N - 1) - 0.001f);
                    const int i0 = (int) fi; const float fr = fi - (float) i0;
                    const float o = f.zOut[i0] + (f.zOut[i0 + 1] - f.zOut[i0]) * fr;
                    const float ic = f.zCur[i0] + (f.zCur[i0 + 1] - f.zCur[i0]) * fr;
                    f.zDelta[c][k] = clampf (f.zDelta[c][k] + ic * dtC, -10.0f, 10.0f);
                    float yk = o * f.zMk[k];
                    if (fw < 1.0f) yk = band[k] + (yk - band[k]) * fw;          // (v73: DRIV under 60 %)
                    pk[k] = maxf (pk[k], absf (yk));
                    y += yk;
                }
                // TONE, the coupling, the gate; then the mix with the direct sound (both at 4x: they stay aligned)
                f.zTone[c] += (y - f.zTone[c]) * kT;
                y = f.zTone[c] * gl + (y - f.zTone[c]) * gh;
                f.zHp[c] += (y - f.zHp[c]) * kH;
                y = (y - f.zHp[c]) * f.zGGain;
                y4[j] = s + wet * (y - s);
            }
            // (a quarter of a sample more: the round trip, 85.75 samples, a whole number, 86)
            const float d4[4] = { f.zD[c][0], y4[0], y4[1], y4[2] };
            f.zD[c][0] = y4[3];
            (c ? wR : wL)[i] = f.zOs[c].down (d4);
            for (int k = 0; k < 3; ++k) drift = maxf (drift, absf (circ == FZC_XSTR ? f.zCs[c][k][0] : f.zDelta[c][k]));   // (XSTR: its own coupling charge)
        }
        if (subOn)
        {
            // CLEAN: under the fuzz, as late as the oversampled path, its mix and its gate
            f.zSubDl[f.zSubW & (FZ_SUBDL - 1)] = subPre ? 0.0f : sub * wet * f.zGGain;
            const float late = f.zSubDl[(f.zSubW - 86) & (FZ_SUBDL - 1)];
            ++f.zSubW;
            wL[i] += late; wR[i] += late;
            f.zSubPrev = sub;
        }
    }
    for (int k = 0; k < 3; ++k) f.zPk[k] = maxf (f.zPk[k] * 0.9f, pk[k]);
    f.zDrift = drift;
    for (int c = 0; c < 2; ++c)
    {
        bool bad = isBad (f.zTone[c]) || isBad (f.zHp[c]);
        for (int q = 0; q < 9; ++q) bad = bad || isBad (f.zX[c][q].ic1) || isBad (f.zX[c][q].ic2);
        bad = bad || isBad (f.zSubP[c][0].ic1) || isBad (f.zSubP[c][1].ic1) || isBad (f.zSubG[c].ic1) || isBad (f.zSubEnv) || isBad (f.zSubPer);
        for (int k = 0; k < 3; ++k) for (int q = 0; q < 4; ++q) bad = bad || isBad (f.zCs[c][k][q]);
        if (bad) { fxSetup (f, M_FXFUZZ); return; }
    }
}

} // namespace sk
