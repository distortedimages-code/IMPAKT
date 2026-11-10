// IMPAKT — master section: Pultec-style passive EQ, Distressor-style compressor, analog drive
// with seven circuit models, final soft clipper. Everything runs 4x oversampled (sk_os.h).
//
//  EQ    wave digital filter of the Pultec EQP-1A passive network (series / parallel adaptors,
//        bilinear capacitors and inductors, double precision), topology and calibrated component
//        values after A. Barrera, "Modeling the Pultec EQP-1A with wave digital filters" (SMC 2024),
//        resonance pre-warped; pot tapers fitted for even dB steps; tube make-up stage.
//  COMP  log-domain gain computer with soft knees and a smoothed branching detector (Giannoulis,
//        Massberg & Reiss, JAES 2012), ratios / opto memory / Nuke / British mode, detector HP and
//        6 kHz band emphasis, "Dist 2" (2nd harmonic) and "Dist 3" (tape-like 3rd) colour stages.
//  DRIVE TAPE    Jiles-Atherton hysteresis solved with RK4 (Chowdhury, DAFx 2019), head bump, gap loss
//        PREAMP  transformer flux saturation + class-A stage
//        TRIODE  12AX7 Koren model on a 100k load line, grid conduction, blocking bias shift
//        PENTODE push-pull pentode pair: class A..B crossover, plate knee, supply sag
//        TRANSISTOR silicon BJT with emitter degeneration (Ebers-Moll, Newton per sample)
//        DIODE   RC + diode clipper ODE (trapezoidal + Newton), asymmetric diode strings
//        BENT    starved germanium stage: dying supply, bias drift, gating sputter
#pragma once
#if defined (__clang__)
 #pragma clang diagnostic ignored "-Wsign-conversion"
#elif defined (__GNUC__)
 #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "sk_math.h"
#include "sk_os.h"

namespace sk {

enum { MASTER_CHUNK = 32, MASTER_OS = 4, TRIODE_TAB = 4096 };

enum DriveModel { DM_OFF = 0, DM_TAPE, DM_PREAMP, DM_TRIODE, DM_PENTODE, DM_TRANSISTOR, DM_DIODE, DM_BENT, NUM_DRIVE_MODELS };
enum CompRatio  { CR_OFF = 0, CR_1, CR_2, CR_3, CR_4, CR_6, CR_OPTO, CR_20, CR_NUKE, CR_BRIT, NUM_COMP_RATIOS };

// all values 0..1 as stored in the project (see the UI for the scales)
struct MasterParams
{
    float eqLf, eqLb, eqLa, eqHf, eqHb, eqBw, eqHa, eqHs;
    float cIn, cAtk, cRel, cRatio, cDist, cDet, cMix, cOut;
    float dModel, dDrive, dBias, dTone, dChar, dMix, dOut, dPos;
    float clip, vol;
    int   eqOn = 1, compOn = 1, drvOn = 1;   // the ON keys of the chain
};

struct Biquad
{
    double b0, b1, b2, a1, a2, z1, z2;
    void reset() { z1 = z2 = 0.0; }
    void setIdentity() { b0 = 1.0; b1 = b2 = a1 = a2 = 0.0; }
    void peaking (double fs, double f, double q, double db);
    void lowpass (double fs, double f, double q);
    void highpass (double fs, double f, double q);
    inline double tick (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// first-order tilt around ~800 Hz; inverse() gives the exact complementary filter
struct Tilt
{
    float b0, b1, a1, x1, y1;
    void set (float fs, float db, int inverse);
    void reset() { x1 = y1 = 0.0f; }
    inline float tick (float x) { const float y = b0 * x + b1 * x1 - a1 * y1; x1 = x; y1 = y; return y; }
};

// --------------------------------------------------------------------------- EQ
struct PultecEQ
{
    double fs;
    double zC1, zL1, zCHC, zCL1, zCL2;                          // reactive element states
    double rC1, rL1, rCHC, rCL1, rCL2;                          // their port resistances
    double rBH1, rBH2, rQ, rCH1, rCH2, rCL, rBL;               // pots
    double rSCL, rCc, rpCD, rS1, rG, rpFG, rsEFG, rH, rsHI, rpE, rS2, rJ, rS3;
    // precomputed scattering ratios (no divisions per sample)
    double kCc, kpE1, kpE2, kG, kH, kJ, kS2, kJ3, kS1, kpE, kpCD, kSCL, kC1, kL1, kpFG, kCHC, kHs, kI;
    int    lfIdx, hfIdx, hsIdx;
    float  cur[8];                                              // smoothed knob values

    void   init (double sampleRate);
    void   setKnobs (float lb, float la, float hb, float bw, float ha, int lf, int hf, int hs);
    void   update();
    double tick (double x);
};

// --------------------------------------------------------------------------- compressor
struct Compressor
{
    float fs;
    float scHpX[2], scHpY[2], scHpA;      // detector HP (100 Hz, 6 dB/oct)
    Biquad band[2];                       // detector band emphasis (6 kHz)
    float env, mem;                       // gain reduction (dB), opto / programme memory
    float dcX[2], dcY[2], dcR;            // DC blockers after the distortion stages
    float lo[2];                          // low band for Dist 3
    float grPeak;
    int   bandOn;

    void init (float sampleRate);
    void process (float* L, float* R, int n, int os, const MasterParams& p, float gIn0, float gIn1, float out0, float out1);
};

// --------------------------------------------------------------------------- drive
// Static waveshaper with first-order antiderivative anti-aliasing (Parker, Zavalishin & Le Bivic,
// DAFx 2016): y = (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1]). Tables are double precision so the
// difference quotient stays clean; outside the table the curve is held flat (F grows linearly).
enum { ADAA_N = 2048 };
struct AdaaTable
{
    double lo, hi, step, inv;
    double f[ADAA_N], F[ADAA_N];

    template <class Fn> void build (double a, double b, Fn fn)
    {
        lo = a; hi = b; step = (b - a) / (double) (ADAA_N - 1); inv = 1.0 / step;
        for (int i = 0; i < ADAA_N; ++i) f[i] = (double) fn ((float) (lo + step * (double) i));
        F[0] = 0.0;
        for (int i = 1; i < ADAA_N; ++i)                  // Simpson on each interval
        {
            const double m = (double) fn ((float) (lo + step * ((double) i - 0.5)));
            F[i] = F[i - 1] + step * (f[i - 1] + 4.0 * m + f[i]) / 6.0;
        }
    }
    static inline double cubic (const double* t, int i, double u)
    {
        const double p0 = t[i - 1], p1 = t[i], p2 = t[i + 1], p3 = t[i + 2];
        return p1 + 0.5 * u * (p2 - p0 + u * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + u * (3.0 * (p1 - p2) + p3 - p0)));
    }
    inline double val (double x) const
    {
        double t = (x - lo) * inv;
        if (t < 1.0) t = 1.0; else if (t > (double) (ADAA_N - 3)) t = (double) (ADAA_N - 3);
        const int i = (int) t;
        return cubic (f, i, t - (double) i);
    }
    inline double anti (double x) const
    {
        double t = (x - lo) * inv;
        if (t < 1.0) return F[1] + f[1] * (x - (lo + step));
        if (t > (double) (ADAA_N - 3)) { const double xe = lo + step * (double) (ADAA_N - 3); return F[ADAA_N - 3] + f[ADAA_N - 3] * (x - xe); }
        const int i = (int) t;
        return cubic (F, i, t - (double) i);
    }
    inline float process (float x, float& prev) const
    {
        const double d = (double) x - (double) prev;
        const double y = (d > 1e-5 || d < -1e-5) ? (anti ((double) x) - anti ((double) prev)) / d
                                                 : val (0.5 * ((double) x + (double) prev));
        prev = x;
        return (float) y;
    }
};

struct DriveChannel
{
    Tilt   pre, post;
    float  dcX, dcY;
    // tape
    double M, Hp, Hdp;
    Biquad bump, loss;
    Tilt   rpre, rpost;
    // transformer flux
    float  phi[2], ePrev[2];
    // triode blocking, pentode sag, bent envelope
    float  env, sag;
    // diode clipper state, previous waveshaper input (ADAA)
    float  vbe, vd, xd, xp, xp2;
    Rng    rng;
};

struct Drive
{
    float fs;
    DriveChannel ch[2];
    float triode[TRIODE_TAB];             // 12AX7 plate voltage vs grid voltage (static load line)
    float tLo, tHi, tStep;
    AdaaTable shaper;                     // static curve of the current model (triode, pentode, transistor)
    int   model, lastModel;
    float lastChar, lastBias, lastTone, lastShape;
    float norm;                           // small-signal normalisation of the current model
    float level;                          // output level compensation of the current model / drive

    void  init (float sampleRate);
    void  configure (const MasterParams& p);
    void  process (float* x, int n, int c, const MasterParams& p, float g0, float g1, float mix0, float mix1, float out0, float out1);
    // both channels in one pass (the same arithmetic per channel; two independent chains for the CPU to overlap)
    void  processStereo (float* xl, float* xr, int n, const MasterParams& p, float g0, float g1, float mix0, float mix1, float out0, float out1);
    float model1 (DriveChannel& d, float u, const MasterParams& p);
    float triodeAt (float vg) const;
};

// --------------------------------------------------------------------------- master
struct Master
{
    float sr, fs;                         // base rate, processing rate
    int   os;                         // 4, or 1 in lite mode
    Oversampler4 osL, osR;
    Oversampler4 osL2, osR2;              // the clipper's own pass when GRAIN / OTT sit in between
    PultecEQ eqL, eqR;
    float tubeX[2], tubeY[2];
    int   eqOn;
    Compressor comp;
    Drive drive;
    float sm[8];                          // smoothed gains: comp in/out, drive gain/mix/out, clip, vol
    float smFrom[8];                      // their values at the start of the chunk (for stage 2)
    float bufL[MASTER_CHUNK * MASTER_OS], bufR[MASTER_CHUNK * MASTER_OS];
    float outPeak, inPeak;

    void init (float sampleRate, int lite);
    // stage 0: the whole master; 1: EQ, compressor and drive only (back at the base rate); 2: clipper and
    // volume only (GRAIN and OTT run in between, at the base rate)
    void process (float* L, float* R, int n, const MasterParams& p, int stage = 0);
    void finish (float* L, float* R, int n, const float* from, int stage);
};

void driveCalibrate (float tapeNorm, const float* sens);   // test hook

} // namespace sk
