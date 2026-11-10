// IMPAKT — WEST (v76; v78: the whole panel of EaseLFO): a west coast voice (sk_west.h). Its patch, its low-pass gates,
// its timbre folder, its envelope detector and its modules' switches are EaseLFO's, ported from its Max patch and its
// gen~ code:
//
//   Copyright (c) 2023, Laboratoire Formes·Ondes. BSD 3-Clause License: the notice, its conditions and its disclaimer
//   in core/LICENSE-EaseLFO.txt (its control circuit, vactrol and network adapted there from the reference source of
//   Parker and D'Angelo, "A Digital Model of the Buchla Lowpass-Gate", DAFx 2013).
//
// The pulser (on the tempo: CNT, BNCE, ACNT, SWNG, PROB, JITR), the CELL folder, DRFT, WARM and the envelope's curve are
// IMPAKT's; the spring is BYOD's (sk_spring).
#include "sk_engine.h"
#include "sk_spring.h"

namespace sk {

namespace {

constexpr u32 WS_TAG = 0x57535432u;   // "WST2"
// the side places (sk_west.h): S1 .. S35 as v77 had most of them, the rest in S36 .. (sideParam)
enum { Q_XFAD = P_S1, Q_FOLD, Q_GLID, Q_DRFT, Q_TAMT, Q_VELO, Q_WARM,
       Q_MWAV, Q_MDST, Q_MKEY, Q_MFRQ, Q_IAMT, Q_TMOD, Q_CHB,
       Q_CNT, Q_BNCE, Q_ACNT, Q_SWNG, Q_PROB, Q_JITR, Q_RAND,
       Q_V1, Q_V2, Q_V3, Q_V4, Q_V5, Q_LEN, Q_DEST,
       Q_G1MD, Q_RESO, Q_ATK, Q_SUST, Q_EMOD, Q_SMIX, Q_SDCY };
static_assert ((int) Q_SDCY == (int) P_S35, "WEST's first 35 places end on S35");
enum { Q_WAVE = P_S36, Q_CKEY, Q_PSRC, Q_PAMT, Q_TSRC, Q_ISRC, Q_FSRC, Q_FAMT, Q_RTRG, Q_IVSR, Q_ETRG, Q_ECRV, Q_PTRG,
       Q_STRG, Q_SPLS, Q_G1LV, Q_G1SR, Q_G1AM, Q_G2MD, Q_G2IN, Q_G2LV, Q_G2SR, Q_G2AM, Q_CHA, Q_PCSR, Q_PCAM,
       Q_SSIZ, Q_SREF, Q_SSPN, Q_SDMP, Q_SCHA, Q_SSHK };
static_assert ((int) Q_SSHK <= (int) P_S70, "WEST's places fit");
enum { D_PTCH = 0, D_TIMB, D_MOD, D_LVL };
enum { E_TRAN = 0, E_SUST, E_CYCL, E_OFF };
enum { G1_BOTH = 0, G1_RESO, G1_VCA, G1_LPF };
enum { G2_BOTH = 0, G2_VCA, G2_LPF };
enum { M_FM = 0, M_AM, M_BAL };
enum { S_SVS = 0, S_EG, S_MO, S_RAND, S_PULS, S_ED, S_PRES, S_INV };
enum { T_KEY = 0, T_PULS, T_SEQ };          // ETRG
enum { PT_SELF = 0, PT_KEY, PT_SEQ };       // PTRG
enum { ST_AUTO = 0, ST_KEY, ST_PULS, ST_OFF };
enum { R_OFF = 0, R_KEY, R_PULS };
enum { I2_MO = 0, I2_PRE, I2_G1 };

inline float bip (float v) { return (v - 0.5f) * 2.0f; }

// RATI: the modulator against the note: under it, on the harmonics, and between them (a free bar's modes: metal)
const float RAT[WS_RATIOS] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.41421356f, 1.5f, 2.0f, 2.41421356f, 2.7565f, 3.0f,
                               3.5f, 4.0f, 5.4039f, 7.0f, 8.933f };
// PULS: OFF, then the interval in steps (16ths): 1/64 1/48 1/32 1/24 1/16 1/12 1/8 3/16 1/4 1/2
const float DIV[WS_DIVS] = { 0.0f, 0.25f, 1.0f / 3.0f, 0.5f, 2.0f / 3.0f, 1.0f, 4.0f / 3.0f, 2.0f, 3.0f, 4.0f, 8.0f };

// the folder (Esqueda, Pöntynen, Välimäki & Parker, DAFx 2017): five cells, each g (|x| - T) sgn x past its threshold
// T, summed with weights w against 5 x (here c = w g); foldf is the circuit, foldF its antiderivative (ADAA)
const double FT[5] = { 0.6, 2.994, 5.46, 1.8, 4.08 };
const double FC[5] = { -12.000 * 0.8333, -27.777 * 0.3768, -21.428 * 0.2829, 17.647 * 0.5743, 36.363 * 0.2673 };
inline double foldF (double x)
{
    const double a = x < 0.0 ? -x : x;
    double y = 2.5 * x * x;
    for (int k = 0; k < 5; ++k) if (a > FT[k]) { const double r = a - FT[k]; y += 0.5 * FC[k] * r * r; }
    return y;
}
inline double foldf (double x)
{
    const double a = x < 0.0 ? -x : x, s = x < 0.0 ? -1.0 : 1.0;
    double y = 5.0 * x;
    for (int k = 0; k < 5; ++k) if (a > FT[k]) y += FC[k] * (a - FT[k]) * s;
    return y;
}

// ---- the low-pass gates: EaseLFO's gen~ code (lpg, control, non-linear, go.vactrol), ported
// the vactrol's time behaviour (lpg.gendsp): a one-pole on the control (trapezoidal), its rate following its own state:
// up in about a millisecond, down in 6 ms (lit) .. 60 ms (dark)
inline float wsVactrol (float& s1, float in, float sr)
{
    const float aBase = 400.0f * 3.14159265f / sr;
    const float tDown = 1.0f + 150.0f * (1.0f - 0.9f * s1), tUp = 1.0f + 2.0f * (1.0f - 0.999f * s1);
    const float a = in - s1 < 0.0f ? aBase / tDown : aBase / tUp;
    const float x = (in - s1) * a / (1.0f + a), y = x + s1;
    s1 = y + x;
    return y;
}
// go.vactrol.gendsp: a one-pole slew on the control, its time following the control itself: 10 ms lit, 200 ms dark
// (to -60 dB): VCA's level, LPF's cutoff
inline float wsVactrolVca (float& g, float in, float sr)
{
    const float n = maxf (1.0f, (0.2f - 0.19f * in) * sr);
    g = in + (g - in) * expf_ (-6.9077553f / n);
    return g;
}
// the network (non-linear.gendsp) for a resistance rf of the cell: C1 1 nF, C2 220 pF, C3 4.7 nF (RESO) or none, the
// resistor r3 (5 M), the feedback through C3 (RESO: a share of where it would ring on its own)
inline void wsGateSet (WsGateK& k, double rf, double r3, double c3, double aNorm, double sr2)
{
    const double c1 = 1e-9, c2 = 2.2e-10;
    k.f = 0.5 / sr2;
    k.a1 = 1.0 / (c1 * rf); k.a2 = -(1.0 / rf + 1.0 / r3) / c1;
    k.b1 = 1.0 / (rf * c2); k.b2 = -2.0 / (rf * c2); k.b3 = 1.0 / (rf * c2); k.b4 = c3 / c2;
    k.d1 = c3 > 0.0 ? aNorm * (2.0 * c1 * r3 + (c2 + c3) * (r3 + rf)) / (c3 * r3) : 0.0;
    k.Dx = 1.0 / (1.0 - k.b2 * k.f); k.Do = 1.0 / (1.0 - k.a2 * k.f);
}
// a sample through it: the feedback's tanh linearised around the last output (its value and its slope there)
inline double wsGateTick (const WsGateK& k, double& sx, double& so, double& sd, double& xo, double x)
{
    const double f = k.f, t = (double) tanhf_ ((float) xo), dt = 1.0 - t * t, d2 = -1.0;
    const double off = k.d1 * (t - xo * dt);
    const double Dmas = 1.0 / (1.0 - k.Dx * (f * f * k.b3 * k.Do * k.a1 + k.b4 * f * k.d1 * dt * k.Do * k.a1 + k.b4 * d2));
    const double yx = (sx + f * k.b1 * x + f * k.b3 * k.Do * so + f * k.b4 * (sd + off / f) + k.b4 * k.d1 * dt * k.Do * so) * k.Dx * Dmas;
    const double yo = (so + f * k.a1 * yx) * k.Do;
    sx += 2.0 * f * (k.b1 * x + k.b2 * yx + k.b3 * yo + k.b4 * ((sd + off / f) + (1.0 / f) * (k.d1 * dt * yo + d2 * yx)));
    so += 2.0 * f * (k.a1 * yx + k.a2 * yo);
    // (C3's differentiator: no C3, no state: a switch to RESO starts it at rest)
    sd = k.b4 > 0.0 ? -(sd + (2.0 / f) * off) - (2.0 / f) * (k.d1 * dt * yo + d2 * yx) : 0.0;
    xo = yo;
    return yo;
}

inline float blep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
// a band-limited pulse (polyBLEP) of width w
inline float pulseW (float ph, float dt, float w)
{
    float h = ph + 1.0f - w; h -= floorf_ (h);
    return (ph < w ? 1.0f : -1.0f) + blep (ph, dt) - blep (h, dt);
}
// the triangle, in the sine's phase (0 at 0, rising)
inline float triS (float ph)
{
    float h = ph + 0.25f; h -= floorf_ (h);
    return 1.0f - 2.0f * absf (2.0f * h - 1.0f);
}
// the complex oscillator's other wave (EaseLFO: tri~, rect~, rect~ 1 0.1)
inline float coWave (int w, float ph, float dt)
{
    if (w == 0) return triS (ph);
    return pulseW (ph, dt, w == 1 ? 0.5f : 0.1f);
}
// the modulation oscillator (EaseLFO: saw~, rect~, tri~)
inline float moWave (int w, float ph, float dt)
{
    if (w == 0) return 2.0f * ph - 1.0f - blep (ph, dt);
    if (w == 1) return pulseW (ph, dt, 0.5f);
    return 1.0f - 2.0f * absf (2.0f * ph - 1.0f);
}

// the time a pulse waits for the next: the interval, swung (long, short) and shaken (JITR), its period's CV
inline double wsWait (WestState& s, const float* P, float pcv)
{
    const float swg = P[Q_SWNG] * 0.5f;
    s.sw ^= 1;
    float d = s.iv * (s.sw ? 1.0f + swg : 1.0f - swg);
    if (P[Q_JITR] > 0.002f) d *= 1.0f + 0.3f * P[Q_JITR] * s.rng.bi();
    if (pcv != 0.0f) d *= exp2f_ (-3.0f * pcv);
    d = maxf (d, 8.0f);
    s.ivNow = d;
    return (double) d;
}

// a source (sk_west.h): its value now, bipolar where it is (SVS, MO), else 0 .. 1; uni: as a level (EaseLFO's gates
// take the modulation oscillator's |x|, the sequencer's 0 .. 1)
struct Src { float v[WS_SRCS]; };
inline float srcB (const Src& s, int k) { return s.v[k < 0 ? 0 : (k >= WS_SRCS ? WS_SRCS - 1 : k)]; }
inline float srcU (const Src& s, int k)
{
    const float x = srcB (s, k);
    return k == S_SVS ? 0.5f + 0.5f * x : (k == S_MO ? absf (x) : x);
}

} // namespace

// the control circuit (v77, EaseLFO's control.gendsp): the control voltage through the op-amp (its diode's knee
// approximated by a cubic) to the LED's current (10.1 uA .. 40 mA, its zener's bend past 9 V), then the photoresistor
double westRf (double vb)
{
    const double Vb = vb < -10.0 ? -10.0 : (vb > 50.0 ? 50.0 : vb);
    const double offset = 0.0001, scale = 0.48;                         // (no offset; the input's range as tuned there)
    const double A = 3.4645912, B = 1136.2129956, Ifmin = 10.1e-6, Ifmax = 40e-3, R2max = 10e3, R6max = 20e3, R7 = 33e3;
    const double R3 = 150e3, R5 = 100e3, R8 = 4.7e3, R9 = 470.0, VB = 3.9, VT = 26e-3, n = 3.9696, kl = 6.3862, G = 2e5;
    const double Vs = 15.0, k0 = 1.468e2, k1 = 4.9202e-1, k2 = 4.1667e-4, k3 = 7.3915e-9, gamma = 0.0001;
    const double R6 = scale * R6max, R1 = (1.0 - offset) * R2max, R2 = offset * R2max, R67 = R6 + R7;
    const double alpha = 1.0 + R67 * (1.0 / R3 + 1.0 / R5);
    const double beta = ((1.0 / alpha) - 1.0) / R67 - 1.0 / R8;
    const double bound1 = 600.0 * alpha * n * VT / (G * (R67 - 1.0 / (alpha * beta)));
    const double Ia = Vb / R5 + Vs / (R3 * (1.0 + R1 / R2));
    double V3;
    if (Ia <= -bound1) V3 = -Ia / (alpha * beta);
    else if (Ia < bound1)
    {
        const double x = G * Ia * (R67 - 1.0 / (alpha * beta)) / (alpha * n * VT);
        const double w = k0 + k1 * x + k2 * x * x + k3 * x * x * x;
        V3 = -(alpha / G) * n * VT * w - Ia / (alpha * beta);
    }
    else V3 = kl * alpha / G * n * VT - Ia * R67;
    const double lim1 = alpha * (Ifmin - beta * V3), lim2 = VB / R67;
    const double lim3 = (gamma * G * VB + alpha * R9 * (VB * beta + Ifmax)) / (gamma * G * R67 + R9);
    double If;
    if (Ia <= lim1) If = Ifmin;
    else if (Ia <= lim2) If = beta * V3 + Ia / alpha;
    else if (Ia <= lim3) If = gamma * G * (Ia * R67 - VB) / (alpha * R9) - beta * VB + Ia / alpha;
    else If = Ifmax;
    If = If < Ifmin ? Ifmin : (If > Ifmax ? Ifmax : If);
    return B + A / (double) powf_ ((float) If, 1.4f);
}

// a ping: the envelope from where it is to this pulse's peak (its level, the sequence's LVL, the velocity), RAND's draws
static void wsPing (WestState& s, const float* P, float vel, float holdSamples)
{
    const int len = 1 + idxOf (P[Q_LEN], WS_STEPS), dest = idxOf (P[Q_DEST], 4);
    float lv = s.pl;
    if (dest == D_LVL)
    {
        const float V = bip (P[Q_V1 + s.step % len]);
        lv *= V >= 0.0f ? 1.0f + 0.25f * V : 1.0f + 0.95f * V;
    }
    s.pk = clampf (0.85f * lv * (0.75f + 0.25f * clampf (vel, 0.0f, 1.0f)), 0.0f, 1.0f);
    s.ampT = clampf (lv, 0.0f, 1.25f);
    if (idxOf (P[Q_EMOD], 4) != E_OFF) { s.est = 0; s.eHold = holdSamples; }
    s.r1 = s.rng.bi(); s.r2 = s.rng.bi();
    s.flash = 0.0f;
}

// the sustain a ping holds for: SUST, or (SUST mode, or DEC on HOLD) as long as the trig's gate is open; 5 ms at the
// least (the pulse that lights the LED: at its shortest DEC the vactrol still lights)
static float wsHold (const Voice& v, const float* P, float sr)
{
    const float sus = maxf (2.0f * P[Q_SUST] * P[Q_SUST], 0.005f) * sr;
    if (v.hold || idxOf (P[Q_EMOD], 4) == E_SUST)
    {
        const float g = v.gateShut ? 0.0f : maxf (0.0f, v.holdSec * sr - (float) v.ns);
        return maxf (sus, g);
    }
    return sus;
}

// the sequencer moves on a step (or to step); a stage pulse if that step's is set (SPLS): what it fires
static int wsStep (WestState& s, Track& t, const float* P, int to)
{
    const int len = 1 + idxOf (P[Q_LEN], WS_STEPS);
    s.step = to < 0 ? (s.step + 1) % len : to % len;
    t.wsNext = (s.step + 1) % len;
    const int mask = idxOf (P[Q_SPLS], 32);
    return (mask >> s.step) & 1;
}

// the pulser starts: CNT pulses (or endless) at the tempo's division, the next one an interval on (from a trig: the trig
// is the first; from a stage pulse: a pulse can't fire the pulser that fires it in the same sample)
static void wsPulser (WestState& s, const float* P, float spStep, float pcv)
{
    const float div = DIV[idxOf (P[P_E], WS_DIVS)];
    const int ci = idxOf (P[Q_CNT], WS_CNTS);
    s.left = div > 0.0f ? (ci >= WS_CNTS - 1 ? -1 : ci + 1) : 0;
    s.np = 0; s.sw = 0;
    s.iv = div * spStep;
    s.next = s.left != 0 ? wsWait (s, P, pcv) : 0.0;
    // ACNT: to the left the first is the quietest, the pulses climb back
    const float ac = bip (P[Q_ACNT]);
    s.pl = ac < 0.0f ? 1.0f + 0.8f * ac : 1.0f;
}

void Engine::westTrigger (Voice& v, const float* P)
{
    WestState& s = v.ws;
    Track& t = tr[v.ti];
    // (a voice that rang out, or another machine's: a fresh start; one still ringing is pinged again where it is,
    // as the instrument would be: the vactrols' light, the phases, the glide go on)
    const bool fresh = s.tag != WS_TAG || v.aenv < 0.5f;
    if (fresh)
    {
        const u32 seed = s.tag == WS_TAG ? s.rng.next() : 0x9E3779B9u ^ ((u32) v.ti * 2654435761u + 0x5157u);
        for (unsigned k = 0; k < (unsigned) sizeof (WestState); ++k) reinterpret_cast<unsigned char*> (&s)[k] = 0;
        s.tag = WS_TAG;
        s.rng.seed (seed ^ 0x3C6EF372u);
        s.est = 3;
        s.pp = 0.0f; s.mp = s.rng.uni();
        s.f1.reset(); s.f2.reset();
        s.step = t.wsNext;
        v.os2.reset (HB1);
    }
    v.aenv = 1.0f;
    const float div = DIV[idxOf (P[P_E], WS_DIVS)];
    const int ptrg = idxOf (P[Q_PTRG], 3), etrg = idxOf (P[Q_ETRG], 3), strg = idxOf (P[Q_STRG], 4), rtrg = idxOf (P[Q_RTRG], 3);
    const bool pulsOn = div > 0.0f && ptrg != PT_SEQ;
    const float spStep = (float) samplesPerStep (t);
    // the sequencer: AUTO (the pulser on: from its first step, the trig the first; off: a step a trig), KEY (a step a
    // trig), PULS (from its first step: the pulses move it), OFF
    int stage = 0;
    if (strg == ST_KEY || (strg == ST_AUTO && ! pulsOn)) stage = wsStep (s, t, P, t.wsNext);
    else if (strg == ST_PULS || strg == ST_AUTO) stage = wsStep (s, t, P, 0);
    // RANDOM: a draw at each trig (RTRG KEY)
    if (rtrg == R_KEY) s.rnd = s.rng.uni();
    // the pulser: KEY, CNT pulses from the trig (the trig the first); SELF, running on its own (started by the first
    // trig, then left to run); SEQ, from the stage pulses
    const bool keyStart = ptrg == PT_KEY || (ptrg == PT_SELF && (fresh || s.left == 0));
    if (div > 0.0f && keyStart)
    {
        wsPulser (s, P, spStep, 0.0f);
        if (ptrg == PT_SELF) s.left = -1;
    }
    else if (div <= 0.0f) s.left = 0;
    if (s.left == 0) s.pl = 1.0f;       // (no pulser running: the trig at its full level)
    // the pulser's first pulse is this trig: the sequence and RANDOM moved on by it too (PULS)
    const bool asPulse = div > 0.0f && keyStart;
    if (asPulse && rtrg == R_PULS) s.rnd = s.rng.uni();
    // the envelope: KEY, or PULS (the trig the pulser's first pulse; the pulser off, the trig), or SEQ (a stage pulse;
    // no stage pulse set, the trig)
    const int mask = idxOf (P[Q_SPLS], 32);
    bool ping = etrg == T_KEY || (etrg == T_PULS && (asPulse || ! pulsOn)) || (etrg == T_SEQ && (mask == 0 || stage));
    if (stage && ptrg == PT_SEQ && div > 0.0f) wsPulser (s, P, spStep, 0.0f);
    if (ping) wsPing (s, P, v.vel, wsHold (v, P, sr));
    else if (fresh) { s.pk = 0.85f * (0.75f + 0.25f * clampf (v.vel, 0.0f, 1.0f)); s.ampT = 1.0f; }
    if (fresh) s.amp = s.ampT;
    // the spring knocked (SSHK, by the trig's velocity)
    t.wsShake = maxf (t.wsShake, P[Q_SSHK] * clampf (v.vel, 0.0f, 1.0f));
    // held until it is let go (a pad: as long as it is pressed; a trig: until the next one, a stop or a mute): an
    // endless pulser, a cycling envelope, a gate left open (G1LV, G2LV: a drone)
    const bool drone = (P[Q_G1LV] > 0.002f && P[Q_CHA] > 0.002f) || (P[Q_G2LV] > 0.002f && P[Q_CHB] > 0.002f);
    if (s.left < 0 || idxOf (P[Q_EMOD], 4) == E_CYCL || drone) { if (padHit) v.padHeld = 1; else v.sus = 1; }
}

void Engine::westRender (Voice& v, const float* P, float tuneSt, int n, float* out)
{
    WestState& s = v.ws;
    Track& t = tr[v.ti];
    const float fn = (float) n, isr = 1.0f / sr, sr2 = 2.0f * sr, isr2 = 1.0f / sr2;
    const int len = 1 + idxOf (P[Q_LEN], WS_STEPS), dest = idxOf (P[Q_DEST], 4);
    const float vel = clampf (v.vel, 0.0f, 1.0f);

    // ---- the envelope: ATK, SUST (wsHold), DEC, its curve (ECRV); the vactrol's fade shortened with a short DEC
    const float Td = v.hold ? HOLD_REL : 0.005f * powf_ (1400.0f, minf (P[P_DEC], 0.98f));
    const float dco = decayCoef (Td, sr);
    const float aT = P[Q_ATK] < 0.005f ? 0.0f : 0.001f * powf_ (2000.0f, P[Q_ATK]);
    const float ainc = aT > 0.0f ? isr / aT : 2.0f;
    const int em = idxOf (P[Q_EMOD], 4);
    const float ecrv = P[Q_ECRV];
    // ---- the routing: the triggers, the sources and their amounts
    const int etrg = idxOf (P[Q_ETRG], 3), ptrg = idxOf (P[Q_PTRG], 3), strg = idxOf (P[Q_STRG], 4), rtrg = idxOf (P[Q_RTRG], 3);
    const int psrc = idxOf (P[Q_PSRC], WS_SRCS), tsrc = idxOf (P[Q_TSRC], WS_SRCS), isrc = idxOf (P[Q_ISRC], WS_SRCS);
    const int fsrc = idxOf (P[Q_FSRC], WS_SRCS), g1src = idxOf (P[Q_G1SR], WS_SRCS), g2src = idxOf (P[Q_G2SR], WS_SRCS);
    const int pcsrc = idxOf (P[Q_PCSR], WS_SRCS), ivsrc = idxOf (P[Q_IVSR], 6);
    const float pamt = bip (P[Q_PAMT]), tamt = bip (P[Q_TAMT]), iamt = bip (P[Q_IAMT]), famt = bip (P[Q_FAMT]);
    const float g1am = bip (P[Q_G1AM]), g2am = bip (P[Q_G2AM]), pcam = bip (P[Q_PCAM]);
    const bool pQuant = psrc == S_SVS || psrc == S_RAND;
    const float div = DIV[idxOf (P[P_E], WS_DIVS)];
    const bool pulsOn = div > 0.0f;
    // ---- the gates: gate 1's mode (the network's C3), its resonance (RESO: a share of where it would ring on its
    // own; LPF: its Q), its gain to the level heard wide open (10 V on its control); gate 2's
    const int g1m = idxOf (P[Q_G1MD], 4), g2m = idxOf (P[Q_G2MD], 3), g2in = idxOf (P[Q_G2IN], 3);
    const double r3 = 5.01e6, c3 = g1m == G1_RESO ? 4.7e-9 : 0.0, an = g1m == G1_RESO ? 0.98 * (double) P[Q_RESO] : 0.0;
    const double rOpen = westRf (10.0);
    const float gOpen = (float) ((r3 + 2.0 * rOpen) / r3);
    const bool net = g1m == G1_BOTH || g1m == G1_RESO;
    const float lq = 0.55f + 8.0f * P[Q_RESO] * P[Q_RESO];
    const bool sineF = idxOf (P[Q_FOLD], 2) == 0;
    const float cha = 2.0f * P[Q_CHA] * P[Q_CHA], chb = 2.0f * P[Q_CHB] * P[Q_CHB];
    const float g1lv = v.gateShut ? 0.0f : P[Q_G1LV], g2lv = v.gateShut ? 0.0f : P[Q_G2LV];

    // ---- the pitch: TUNE (CKEY on: with the note), the sequence's PTCH, gliding (GLID); DRFT's walk
    const float tune = idxOf (P[Q_CKEY], 2) == 0 ? tuneSt : (P[P_TUNE] - 0.5f) * 48.0f;
    const float pitT = dest == D_PTCH ? floorf_ (bip (P[Q_V1 + s.step % len]) * 12.0f + 0.5f) : 0.0f;
    if (! s.pitOn) { s.pit = pitT; s.pitOn = 1; }
    const float gl = P[Q_GLID];
    const float glK = gl < 0.005f ? 1.0f : 1.0f - expf_ (-isr / (0.003f * powf_ (300.0f, gl)));
    s.drC -= fn;
    if (s.drC <= 0.0f) { s.drT = s.rng.bi(); s.drC = sr * (0.3f + 0.7f * s.rng.uni()); }
    s.dr += (s.drT - s.dr) * minf (1.0f, fn * isr * 2.0f);
    const float drift = P[Q_DRFT] * 0.35f * s.dr;

    // ---- TIMB and MOD at the end of this chunk (their bases: the knobs, the sequence, the velocity, RAND), ramped
    const float rnd = P[Q_RAND];
    float Tb = P[P_A] + (P[Q_VELO]) * (vel - 0.6f) * 0.8f + rnd * 0.5f * s.r1;
    float Cb = P[P_C] + rnd * 0.4f * s.r2;
    if (dest == D_TIMB) Tb += 0.6f * bip (P[Q_V1 + s.step % len]);
    if (dest == D_MOD) Cb += 0.6f * bip (P[Q_V1 + s.step % len]);
    if (v.ns == 0) { s.pT = Tb; s.pC = Cb; }
    const float sy = bip (P[P_B]) * 0.6f, xf = P[Q_XFAD], tmod = P[Q_TMOD];
    const int cw = idxOf (P[Q_WAVE], 3), mw = idxOf (P[Q_MWAV], 3), md = idxOf (P[Q_MDST], 3), freeM = idxOf (P[Q_MKEY], 2);
    const float rat = RAT[idxOf (P[P_D], WS_RATIOS)] * semis (bip (P[Q_MFRQ]));
    const float ffree = 0.1f * powf_ (20000.0f, P[Q_MFRQ]);
    const float wk = 1.0f - expf_ (-TWO_PI * 20000.0f * powf_ (0.0665f, P[Q_WARM]) * isr2);
    // (the high-passes before the gates: 8 Hz, up to 58 Hz as SYMM leaves its centre: the offset's DC, moving with
    // TIMB, would thump through the gate)
    const float hpR = 1.0f - TWO_PI * (8.0f + 50.0f * absf (sy) * (1.0f / 0.6f)) * isr2;
    const float ampK = 1.0f - expf_ (-isr / 0.002f);
    const float edK = 1.0f / (0.02f * sr);             // (envFollow: 20 ms up, 20 ms down)
    const float dT = (Tb - s.pT) / fn, dC = (Cb - s.pC) / fn;
    float T0 = s.pT, C0 = s.pC;
    const float spStep = (float) samplesPerStep (t);

    float pk = 0.0f, ymax = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        // ---- the sources now
        Src S;
        S.v[S_SVS] = bip (P[Q_V1 + s.step % len]);
        const float eg = em == E_OFF ? 0.0f : minf (1.0f, 1.5f * envCurve (s.e, s.est == 0 ? 0 : 2, ecrv) * s.pk);
        S.v[S_EG] = eg;
        S.v[S_MO] = s.mo;
        S.v[S_RAND] = s.rnd;
        s.pul = s.left != 0 && s.ivNow > 0.0f ? s.pl * clampf ((float) s.next / s.ivNow, 0.0f, 1.0f) : 0.0f;
        S.v[S_PULS] = s.pul;
        S.v[S_ED] = s.ed;
        S.v[S_PRES] = vel;
        S.v[S_INV] = 1.0f - srcU (S, ivsrc);
        s.egv = eg; s.sv = S.v[S_SVS];

        // ---- the pulser: a pulse (PROB: or not) moves the sequence on (STRG AUTO, PULS), draws (RTRG PULS), pings
        // the envelope (ETRG PULS)
        if (s.left != 0 && ! v.gateShut)
        {
            s.next -= 1.0;
            if (s.next <= 0.0)
            {
                ++s.np;
                int stage = 0;
                if (strg == ST_AUTO || strg == ST_PULS) stage = wsStep (s, t, P, -1);
                const float ac = bip (P[Q_ACNT]);
                if (ac > 0.0f) s.pl *= 1.0f - 0.5f * ac; else s.pl += (1.0f - s.pl) * (-ac) * 0.5f;
                const bool fire = P[Q_PROB] >= 0.999f || s.rng.uni() < P[Q_PROB];
                if (fire)
                {
                    if (rtrg == R_PULS) s.rnd = s.rng.uni();
                    if (etrg == T_PULS) wsPing (s, P, vel, wsHold (v, P, sr));
                }
                if (stage && etrg == T_SEQ) wsPing (s, P, vel, wsHold (v, P, sr));
                if (s.left > 0) --s.left;
                const float b = bip (P[Q_BNCE]);
                s.iv *= b > 0.0f ? 1.0f - 0.45f * b : 1.0f - 0.6f * b;
                if (s.iv < 0.004f * sr || s.iv > 8.0f * sr || s.pl < 0.02f) s.left = 0;   // (the ball at rest)
                if (s.left != 0) s.next += wsWait (s, P, pcam * srcB (S, pcsrc));
                // (a stage pulse starting the pulser again, PTRG SEQ: CNT more, after an interval)
                if (stage && ptrg == PT_SEQ && s.left == 0 && pulsOn) wsPulser (s, P, spStep, pcam * srcB (S, pcsrc));
            }
        }
        s.flash += isr;
        // ---- the envelope
        if (s.est == 0) { s.e += ainc; if (s.e >= 1.0f) { s.e = 1.0f; s.est = 1; } }
        else if (s.est == 1) { if (s.eHold > 0.0f && ! (v.gateShut && (v.hold || em == E_SUST))) s.eHold -= 1.0f; else s.est = 2; }
        else if (s.est == 2)
        {
            s.e *= dco;
            if (s.e < (em == E_CYCL ? 0.01f : 1e-5f))
            {
                if (em == E_CYCL && ! v.gateShut) { s.est = 0; s.eHold = maxf (2.0f * P[Q_SUST] * P[Q_SUST], 0.005f) * sr; }
                else { s.est = 3; s.e = 0.0f; }
            }
        }
        // ---- gate 1: its level (G1LV + G1AM x its source) through its vactrols: the lpg's (BOTH, RESO; driven past the
        // light's top, as the circuit is: a ping lights it fully), go.vactrol (VCA's level, LPF's cutoff)
        const float c1 = clampf (g1lv + g1am * srcU (S, g1src), 0.0f, 1.0f);
        const float c2 = clampf (g2lv + g2am * srcU (S, g2src), 0.0f, 1.0f);
        s.c1 = c1; s.c2 = c2;
        s.y = wsVactrol (s.vs, c1, sr);
        wsVactrolVca (s.gv, c1, sr);
        wsVactrolVca (s.gl, c1, sr);
        s.y2 = wsVactrol (s.vs2, c2, sr);
        wsVactrolVca (s.gv2, c2, sr);
        wsVactrolVca (s.gl2, c2, sr);
        ymax = maxf (ymax, maxf (cha > 0.0f ? (net ? s.y : (g1m == G1_VCA ? s.gv : s.gl)) : 0.0f,
                                 chb > 0.0f ? (g2m == G2_BOTH ? s.y2 : (g2m == G2_VCA ? s.gv2 : s.gl2)) : 0.0f));
        // the network for that light (every 4 samples: the cell's resistance through the control circuit, 0 .. 10 V)
        if (net && --s.cnt <= 0)
        {
            s.cnt = 4;
            const double rf = westRf (10.0 * (double) clampf (s.y, 0.0f, 1.0f));
            wsGateSet (s.gk, rf, r3, c3, an, (double) sr2);
            s.gn = gOpen;
            // (the screen: the network's lower pole, a1 / a2)
            const double a1 = 1.0 + 2.0 * rf / r3, a2 = rf * (2e-9 + 2.2e-10 + c3 + (2.2e-10 + c3) * rf / r3);
            s.fc = (float) (a1 / a2 * (1.0 / 6.283185307));
        }
        // the LPFs' cutoffs (EaseLFO: 0 .. 8 kHz, ^1.06, on go.vactrol)
        if (g1m == G1_LPF && (i & 3) == 0) { s.f1.set (maxf (20.0f, 8000.0f * powf_ (s.gl, 1.06f)), lq, sr2); s.fc = 8000.0f * powf_ (s.gl, 1.06f); }
        if (g2m == G2_LPF && (i & 3) == 0) s.f2.set (maxf (20.0f, 8000.0f * powf_ (s.gl2, 1.06f)), lq, sr2);
        // ---- TIMB, MOD for this sample: their ramps, their CV (TSRC, ISRC)
        T0 += dT; C0 += dC;
        const float Tn = clampf (T0 + tamt * srcB (S, tsrc), 0.0f, 2.0f), Cn = clampf (C0 + iamt * srcB (S, isrc), 0.0f, 1.0f);
        const float A = 0.55f + 7.5f * minf (Tn, 1.0f) * minf (Tn, 1.0f) * (Tn > 1.0f ? Tn : 1.0f);
        const float Ak = 8.0f * (3.0f * Tn + 0.125f), beta = 10.0f * Cn * Cn;
        // ---- the pitch (PSRC: x PAMT, +-24 semitones; on the semitones from the sequencer and RANDOM), the
        // modulation oscillator's (KEYD on the note at RATI, FREE; FSRC: x FAMT, +-48 semitones)
        s.pit += (pitT - s.pit) * glK;
        float pcv = pamt * 24.0f * srcB (S, psrc);
        if (pQuant) pcv = floorf_ (pcv + 0.5f);
        const float f0 = 130.81f * semis (tune + s.pit + drift + pcv);
        const float fmul = famt != 0.0f ? semis (famt * 48.0f * srcB (S, fsrc)) : 1.0f;
        const float fm = (freeM ? ffree : f0 * rat) * fmul, dtm = minf (fm * isr2, 0.45f);
        // FM's deviation: the index on the note (KEYD), or a share of the note's own frequency (FREE: a slow modulator
        // is a vibrato, a fast one FM all the same)
        const float dev = freeM ? 2.0f * Cn * Cn * f0 : beta * fm;
        float h2[2];
        for (int j = 0; j < 2; ++j)
        {
            const float m = moWave (mw, s.mp, dtm);
            s.mp += dtm; s.mp -= floorf_ (s.mp);
            s.mo = m;
            const float inc = md == M_FM ? clampf ((f0 + dev * m) * isr2, -0.45f, 0.45f) : minf (f0 * isr2, 0.45f);
            const float ph = s.pp;
            s.pp += inc; s.pp -= floorf_ (s.pp);
            float w = sin2pi (ph);
            float wv = xf > 0.002f ? coWave (cw, ph, absf (inc)) : 0.0f;
            // AM (EaseLFO: 1 - index x (0.5 + 0.5 x the modulator)), on both
            if (md == M_AM) { const float am = 1.0f - Cn * (0.5f + 0.5f * m); w *= am; wv *= am; }
            // the sine into the folder, through its antiderivative: SINE (EaseLFO's timbre: pushed 1 .. 25 times round a
            // sine, 49 with its CV), CELL (beyond 7 V its input rounds off toward the rails; a third of its volts out)
            double yf;
            if (sineF)
            {
                const double x = (double) (Ak * (1.0f + tmod * m) * (w + sy)), dx = x - s.xp;
                yf = (dx > 1e-3 || dx < -1e-3) ? (double) (cos2pi ((float) (s.xp * 0.15915494309)) - cos2pi ((float) (x * 0.15915494309))) / dx
                                               : (double) sin2pi ((float) (0.5 * (x + s.xp) * 0.15915494309));
                s.xp = x;
            }
            else
            {
                double x = (double) (A * (1.0f + tmod * m) * (w + sy));
                const double ax = x < 0.0 ? -x : x;
                if (ax > 7.0) x = (x < 0.0 ? -1.0 : 1.0) * (7.0 + 3.0 * (double) tanhf_ ((float) ((ax - 7.0) * (1.0 / 3.0))));
                const double dx = x - s.xp;
                yf = ((dx > 1e-7 || dx < -1e-7) ? (foldF (x) - foldF (s.xp)) / dx : foldf (0.5 * (x + s.xp))) * (1.0 / 3.0);
                s.xp = x;
            }
            // XFAD: the folded sine to the other wave; WARM (the output stage); the DC out before the gates
            const float co = (float) yf + ((wv - (float) yf) * xf);
            s.wl += (co - s.wl) * wk;
            const float hp1 = s.wl - s.hx + hpR * s.hy; s.hx = s.wl; s.hy = hp1;
            const float hp = hp1 - s.hx2 + hpR * s.hy2; s.hx2 = hp1; s.hy2 = hp;
            // gate 1
            float g1 = 0.0f;
            if (cha > 0.0f || g2in == I2_G1)
            {
                if (net) g1 = (float) wsGateTick (s.gk, s.sx, s.so, s.sd, s.xo, (double) hp) * s.gn;
                else if (g1m == G1_VCA) g1 = hp * s.gv;
                else { float lp, bp, hpo; s.f1.tick (hp, lp, bp, hpo); g1 = lp; }
                if (net) { const float u = minf (1.0f, s.y * 83.0f); g1 *= u * u * (3.0f - 2.0f * u); }   // (the cell at its darkest: its leak let go)
            }
            // gate 2: the modulation oscillator, PRE (the voice's own output), gate 1's; BAL: balanced by the modulator
            float g2 = 0.0f;
            if (chb > 0.0f)
            {
                float x2 = g2in == I2_MO ? 0.6f * m : (g2in == I2_PRE ? softclip (s.zOut) : g1);
                if (md == M_BAL && g2in != I2_MO) x2 += x2 * Cn * m;
                if (g2m == G2_BOTH) g2 = x2 * s.y2;
                else if (g2m == G2_VCA) g2 = x2 * s.gv2;
                else { float lp, bp, hpo; s.f2.tick (x2, lp, bp, hpo); g2 = lp; }
            }
            h2[j] = cha * g1 + chb * g2;
            s.zOut = h2[j];
        }
        // back to the track's rate; the level heard (ACNT, LVL) glided
        float y = v.os2.process (h2[0], h2[1]);
        s.amp += (s.ampT - s.amp) * ampK;
        y *= 0.5f * s.amp;
        s.dc += (y - s.dc) * 0.0004f;
        y -= s.dc;
        out[i] = y;
        pk = maxf (pk, absf (y));
        // the envelope detector (envFollow: over 0.1, followed)
        s.ed += ((absf (y) > 0.1f ? 1.0f : 0.0f) - s.ed) * edK;
        s.Anow = sineF ? Ak : A; s.cnow = Cn;
    }
    s.pT = Tb; s.pC = Cb;
    auto badD = [] (double x) { return ! (x > -1e12 && x < 1e12); };
    if (badD (s.sx) || badD (s.so) || badD (s.sd) || badD (s.xo) || isBad (s.wl) || isBad (s.hy) || isBad (s.dc) || badD (s.xp)
        || isBad (s.vs) || isBad (s.vs2) || isBad (s.zOut) || isBad (s.f1.ic1) || isBad (s.f2.ic1))
    {
        s.sx = s.so = s.sd = s.xo = 0.0; s.wl = s.hx = s.hy = s.hx2 = s.hy2 = s.dc = s.vs = s.y = s.gv = s.gl = 0.0f; s.xp = 0.0;
        s.vs2 = s.y2 = s.gv2 = s.gl2 = s.zOut = 0.0f; s.f1.reset(); s.f2.reset();
    }
    s.lvl = maxf (pk, s.lvl * 0.9f);
    // (the voice lives while a gate is lit, and while pulses or cycles are still to come, or a gate is held open)
    const float u = minf (1.0f, ymax * 83.0f);
    const bool more = ((s.left != 0 || (em == E_CYCL && s.est != 3) || g1lv > 0.002f || g2lv > 0.002f) && ! v.gateShut) || s.est < 2;
    v.level = maxf (ymax * u * u * (3.0f - 2.0f * u), more ? 1e-2f : 0.0f);
    v.aenv = 1.0f;
    v.ns += (u32) n;
}

// the spring (v78: BYOD's, sk_spring) on the track's way out, its own; the middle back into the track, the sides with
// WIDE's. It sleeps when it has nothing left to ring
void Engine::westSpace (Track& t, int ti, const float* P, float* out, float* side, int n)
{
    FxState& f = fx[ti];
    static_assert (sizeof (ByodSpring) <= sizeof (f.mem), "WEST's spring lives in the track's FX memory");
    ByodSpring& sp = *reinterpret_cast<ByodSpring*> (f.mem);
    const bool mine = machineOf (P[P_MACH]) == M_WEST;
    const float target = mine ? P[Q_SMIX] : 0.0f;
    if (! t.cvOn)
    {
        float pk = 0.0f;
        for (int i = 0; i < n; ++i) pk = maxf (pk, absf (out[i]));
        if (target <= 0.0005f || pk < 1e-7f) { t.wsShake = 0.0f; return; }
        if (f.kind != WS_KIND || f.sr != sr) { sp.init (sr); f.kind = WS_KIND; f.sr = sr; }
        t.cvOn = 1; t.cvQuiet = 0.0f;
        if (t.cvMix <= 0.0f) t.cvMix = target;
    }
    if (f.kind != WS_KIND || f.sr != sr) { sp.init (sr); f.kind = WS_KIND; f.sr = sr; }
    if (mine) sp.configure (P[Q_SSIZ], P[Q_SDCY], P[Q_SREF], P[Q_SSPN], P[Q_SDMP], P[Q_SCHA]);
    if (t.wsShake > 0.0f) { sp.shake (t.wsShake, P[Q_SSIZ]); t.wsShake = 0.0f; }
    float wl[CHUNK], wr[CHUNK];
    for (int i = 0; i < n; ++i) wl[i] = wr[i] = 0.0f;
    sp.process (out, wl, wr, n);
    const float k = 1.0f - expf_ (-1.0f / (0.03f * sr));
    float pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        t.cvMix += (target - t.cvMix) * k;
        // (BYOD's mix: the wet added to the dry, at its gain)
        const float wet = 1.6f * t.cvMix;
        const float mid = 0.5f * (wl[i] + wr[i]), sd = 0.5f * (wl[i] - wr[i]);
        out[i] += wet * mid;
        side[i] = wet * sd;
        pk = maxf (pk, absf (wl[i]) + absf (wr[i]));
    }
    if (pk < 3e-6f || (target <= 0.0f && t.cvMix < 1e-4f)) t.cvQuiet += (float) n; else t.cvQuiet = 0.0f;
    if (t.cvQuiet > 1.5f * sr || (! mine && t.cvMix < 1e-4f)) { t.cvOn = 0; t.cvQuiet = 0.0f; if (! mine) t.cvMix = 0.0f; }
}

// the screen: playing, gate 1's light, the envelope, the step, the time since the last ping, the pulses so far, the
// folder's drive, the modulation, the gate's pole (Hz), the level, the pitch over TUNE's, the spring's mix, the pulses
// left (-1 endless), the interval (seconds), the level heard, the envelope's stage; gate 2's light, RANDOM, the detector,
// the pulser's ramp, the sequencer's value, the gates' controls, the envelope out
void Engine::westInfo (int ti, float* out)
{
    for (int k = 0; k < WS_INFO; ++k) out[k] = 0.0f;
    if (ti < 0 || ti >= NUM_TRACKS) return;
    const Track& t = tr[ti];
    const Voice& v = t.voice[t.cur];
    const bool on = v.active && v.machine == M_WEST && v.ws.tag == WS_TAG;
    out[3] = (float) t.wsNext;
    if (! on) return;
    const WestState& s = v.ws;
    out[0] = 1.0f;
    out[1] = s.y; out[2] = s.e; out[3] = (float) s.step; out[4] = s.flash; out[5] = (float) s.np;
    out[6] = s.Anow; out[7] = s.cnow; out[8] = s.fc; out[9] = s.lvl; out[10] = s.pit;
    out[11] = t.cvOn ? t.cvMix : 0.0f;
    out[12] = (float) s.left; out[13] = s.iv / sr;
    out[14] = s.amp; out[15] = (float) s.est;
    out[16] = s.y2; out[17] = s.rnd; out[18] = s.ed; out[19] = s.pul; out[20] = s.sv;
    out[21] = s.c1; out[22] = s.c2; out[23] = s.egv;
}

} // namespace sk
