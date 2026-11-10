// IMPAKT — MANTRA (v84): eight voices that breathe and drive each other, an homage to the organismic synthesizers of
// Soma Laboratory (the Lyra-8): own implementation, from what the instrument does and how such circuits behave (its
// manual, its published parts: CMOS Schmitt-trigger oscillators, transistor VCAs, two PT2399 delay lines, op-amps),
// not from its code or its schematic.
//
// The voices are circuits, solved sample by sample: each a Schmitt-trigger relaxation oscillator (a capacitor charged
// through its TUNE resistance toward the gate's output, which flips at two thresholds: a square at the gate, a curved
// triangle on the capacitor), its threshold crossings placed between samples (the square band-limited, polyBLEP), its
// frequency set by its resistance (TUNE: neither linear nor exponential, as a pot in series with a resistor), its
// modulation a current into its capacitor (the higher it is tuned, the less a modulation moves it; pushed hard, it
// stalls and chirps), its envelope the pull-up of its gate (opening, it lifts its pitch a little and reshapes it), its
// VCA a transistor (it rounds what it lets through). All eight share one supply: what each draws moves the others'
// thresholds (they pull each other, lock, beat), they bleed into each other and into the mix, and their sum goes into
// an op-amp that saturates (their intermodulation: the harmonics between them).
//
//   PLAY    TUNE (all eight transposed: a trig's NOTE too), DEC (the slow release: 0.5 .. 32 s), TCH (the sensors a
//           trig touches: one, a pair, a group, all), HLD1 HLD2 (each group's HOLD: the level its voices never fall
//           under; on FAST pairs it opens later), DFB (the delay's feedback: past the middle it sings alone), DRIV (the
//           distortion), PAGE
//   V1-4    TUN1 .. TUN4 (1 2 bass, 3 4 middle), SHP12 SHP34 (SHARP: triangle .. square), PCH1 (the group's PITCH;
//           near its top, as it is)
//   V5-8    TUN5 .. TUN8 (5 6 middle, 7 8 high), SHP56 SHP78, PCH2
//   FM1-4   MOD12 SRC12 FST12 MOD34 SRC34 FST34 (each pair's modulation depth, its source: OFF, FM (the other pair:
//           12 <- 34, 34 <- 12; on 1 LOOP 12 <- 78), LFO (the hyper LFO; TOTL on: the instrument's own output), FAST:
//           a short release, the sensors harder to wake), LOOP (2 LOOP: 12 34 and 56 78 each a loop of their own;
//           1 LOOP: 12 -> 34 -> 56 -> 78 -> 12)
//   FM5-8   MOD56 SRC56 FST56 MOD78 SRC78 FST78, TOTL
//   LFO     FRQA FRQB (two square LFOs, 0.03 .. 300 Hz), LGIC (OR: their analogue sum, a gradient; AND: their
//           product), LINK (A sweeps B: a soft FM), VIB (each voice's own vibrato), VDPT (its depth), SAG (the shared
//           supply: how much the voices pull each other)
//   DELAY   TIM1 TIM2 (two lo-fi lines, 8 ms .. 1.1 s: past about 340 ms beyond what their converters are made for,
//           darker, noisier, breaking up), DMD1 DMD2 (their times modulated), DSRC (LFO, or SELF: each by its own
//           output), DWAV (TRI: two triangles at FRQA and FRQB; SQR: the AND), DMIX
//   OUT     DSTM (the distortion's mix), VOL, BUS (the mix's op-amp driven: the voices saturate together), BLED (the
//           crosstalk: silent voices heard, neighbours in each other's modulation), TOL (the parts' tolerances: no two
//           voices alike), DRFT (their slow thermal drift), NOIS (the hiss, the hum a finger picks up)
#pragma once

#include "sk_math.h"

namespace sk {

enum { MT_VOICES = 8, MT_TCHS = 15, MT_PAGES = 8, MT_SRCS = 3, MT_INFO = 48, MT_KIND = 1002 };
// the side places (S1 ..): seven a page
enum { MT_TUN1 = 0, MT_TUN2, MT_TUN3, MT_TUN4, MT_SHP12, MT_SHP34, MT_PCH1,
       MT_TUN5, MT_TUN6, MT_TUN7, MT_TUN8, MT_SHP56, MT_SHP78, MT_PCH2,
       MT_MOD12, MT_SRC12, MT_FST12, MT_MOD34, MT_SRC34, MT_FST34, MT_LOOP,
       MT_MOD56, MT_SRC56, MT_FST56, MT_MOD78, MT_SRC78, MT_FST78, MT_TOTL,
       MT_FRQA, MT_FRQB, MT_LGIC, MT_LINK, MT_VIB, MT_VDPT, MT_SAG,
       MT_TIM1, MT_TIM2, MT_DMD1, MT_DMD2, MT_DSRC, MT_DWAV, MT_DMIX,
       MT_DSTM, MT_VOL, MT_BUS, MT_BLED, MT_TOL, MT_DRFT, MT_NOIS, MT_NSIDE };

// a voice's frequency for its TUNE (Hz, before PITCH, the transposition and the circuit's own deviations): 1 2 bass
// (30 .. 600 Hz), 3 .. 6 middle (60 .. 1200), 7 8 high (120 .. 2400)
inline float mantraTuneHz (int k, float x)
{
    const float hi = k < 2 ? 600.0f : k < 6 ? 1200.0f : 2400.0f;
    const float u = 1.0f - (x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x);
    return hi / (1.0f + 19.0f * u * u);
}
// a pair's HOLD: the level its voices never fall under (FAST pairs open later)
inline float mantraHoldLevel (float x, bool fast)
{
    const float u = fast ? (x - 0.45f) / 0.55f : (x - 0.08f) / 0.8f;
    const float c = u < 0.0f ? 0.0f : u > 1.0f ? 1.0f : u;
    return c * c * (3.0f - 2.0f * c);
}
// the sensors a trig touches (TCH): a bit a voice
inline unsigned mantraTouchMask (int i)
{
    static const unsigned M[MT_TCHS] = { 1, 2, 4, 8, 16, 32, 64, 128, 3, 12, 48, 192, 15, 240, 255 };
    return M[i < 0 ? 0 : i >= MT_TCHS ? MT_TCHS - 1 : i];
}

struct MantraVoice
{
    float v;                 // the timing capacitor's voltage (in supply units)
    int   q;                 // the gate's output (1 high)
    float sq1, tr1, acc0, acc1;   // the square and the triangle a sample late, their polyBLEP corrections
    float env;               // its envelope (its VCA's control)
    float seqP, padP;        // the sensor's pressure: from a trig, from a finger on the screen (0: untouched)
    float seqLeft;           // a trig's touch: samples left (-1: until let go)
    float vibPh, vibHz;      // its vibrato
    float drN, drFrom, drTo, drPh;   // DRFT: its wander
    float tolC, tolT, tolH;  // TOL: its capacitor, its thresholds' centre, their hysteresis
    float out;               // its output (after its VCA), for the FM it sends
    float hz;                // its frequency now (for the screen)
};

struct MantraState
{
    MantraVoice vo[MT_VOICES];
    int   fresh;
    float vddR, vddD;        // the shared supply: its ripple, its load
    // the hyper LFO: two relaxation oscillators (their capacitors, their gates), the OR's gradient
    float laV, lbV; int laQ, lbQ; float lOr, lSqA, lSqB;
    float pairM[4];          // each pair's modulation now (for the screen)
    // the mix: its op-amp's DC, the output's DC blocker, the last output (TOTAL FB), the hum's phase
    float busX, busY, outX, outY, last, humPh;
    // the delay: where the lines are written, their times now, their input and output filters, the slew of their
    // converters, their outputs, the loop's ceiling and the level it follows
    int   dw;
    float dT[2], dIn[2][2], dOut[2][2], dSl[2], dY[2], dCeil, dEnv;
    float dAge;              // samples since the lines were last given something (what they hold has to come out)
    // the distortion: its pre-emphasis, its output filter
    float xHp, xLp;
    float awake;             // how much the instrument is playing (the bleed fades when it rests)
    float outLv, wetLv;      // levels for the screen
    Rng   rng;
};

} // namespace sk
