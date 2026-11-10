// IMPAKT — WEST (v76; v78: the whole panel of EaseLFO): a west coast voice, a tribute to the portable west coast
// instruments of the early seventies. Since v78 its patch is EaseLFO's (Laboratoire Formes·Ondes, 2023, BSD 3-Clause:
// core/LICENSE-EaseLFO.txt): its modules, their switches and the patch points between them, ported from its Max patch
// and its gen~ code, with the pulser of v76 (CNT, BNCE, ACNT, SWNG, PROB, JITR on the tempo) in its pulser's place and
// ChowDSP's BYOD spring (sk_spring, GPL-3.0) in its reverb's:
//
//  COMPLEX OSC  a sine through the TIMBRE folder (EaseLFO's: the wave pushed 1 .. 25 times round a sine; or CELL, the
//               five-cell circuit of Esqueda, Pöntynen, Välimäki and Parker, DAFx 2017), crossfaded (XFAD) to its
//               other wave unfolded (WAVE: TRI SQR SPKE); TIMB (MAIN), SYMM (its offset); CKEY (on the note, or not);
//               MDST, what the modulation oscillator does to it: FM (its pitch), AM (its level), BAL (gate 2's input
//               balanced by it); its pitch CV (PSRC, PAMT: the polarity its sign), its timbre CV (TSRC, TAMT)
//  MOD OSC      MWAV (SAW SQR TRI), MKEY (on the note at RATI, or FREE at MFRQ), MOD (MAIN: the index; ISRC, IAMT its
//               CV), its frequency CV (FSRC, FAMT); TMOD (it on TIMB too)
//  ENVELOPE     EMOD (TRAN: attack, sustain, decay; SUST: held while the trig's gate is; CYCL; OFF), ETRG (KEY: the
//               trigs; PULS: the pulses, the trigs when the pulser is off; SEQ: the sequencer's stage pulses), ATK, SUST,
//               DEC (the track's), ECRV (its curve, after Streams: LOG .. LIN .. EXP)
//  PULSER       PULS (MAIN: OFF, or a division of the tempo), CNT, BNCE, ACNT, SWNG, PROB, JITR; PTRG (SELF: running on
//               its own; KEY: CNT pulses from each trig; SEQ: from each stage pulse); its period's CV (PCSR, PCAM)
//  SEQUENCER    five steps (V1 .. V5, bipolar), LEN, STRG (AUTO: the pulses, a trig starting it over, or the trigs when
//               the pulser is off; KEY; PULS; OFF), SPLS (the steps that fire a stage pulse), DEST (a direct route:
//               PTCH +-12 semitones, TIMB, MOD, LVL)
//  RANDOM       a new value (0 .. 1) at each trig or pulse (RTRG); RAND (draws on TIMB and MOD at each pulse)
//  INVERTER     1 - a source (IVSR)
//  ENV DETECT   the voice's own output over a threshold, followed (20 ms up and down: EaseLFO's envFollow)
//  GATE 1       the complex oscillator into a low-pass gate: G1MD (BOTH: the vactrol and the passive network of
//               Parker and D'Angelo's model, gen~ lpg / control / non-linear; RESO: with its capacitor C3 and the
//               feedback through a tanh, RESO; VCA: the level through the other vactrol, go.vactrol; LPF: a state-
//               variable low-pass to 8 kHz on it), its level G1LV + G1AM x a source (G1SR)
//  GATE 2       G2IN (the modulation oscillator, PRE: the voice's own output, or gate 1's) into G2MD (BOTH: the
//               lpg vactrol on the level; VCA; LPF), its level G2LV + G2AM x a source (G2SR)
//  OUTPUT       CHA, CHB (the two gates' levels to the output), the spring (SMIX, SSIZ, SDCY, SREF, SSPN, SDMP, SCHA;
//               SSHK: each trig knocks it)
//  sources      SVS (the sequencer), EG, MO, RAND, PULS (the pulser's falling ramp), ED, PRES (the velocity), INV
#pragma once
#include "sk_math.h"

namespace sk {

enum { WS_INFO = 24, WS_STEPS = 5, WS_RATIOS = 16, WS_DIVS = 11, WS_CNTS = 17, WS_KIND = 1001, WS_SRCS = 8 };

// the gate's network for one resistance of the photocell (the coefficients of its trapezoidal integrators)
struct WsGateK { double f, a1, a2, b1, b2, b3, b4, d1, Dx, Do; };
// the control circuit (EaseLFO's control.gendsp): the control voltage (volts) to the photoresistor's resistance (ohms)
double westRf (double vb);

struct WestState
{
    u32    tag;                 // (set once the state is laid out for WEST)
    double xp;                  // the folder's last input (its antiderivative's difference)
    float  pp, mp;              // the complex oscillator's phase, the modulation oscillator's (turns)
    float  pit;                 // the pitch now (semitones over TUNE's), gliding to its target
    int    pitOn;               // (the glide has a start)
    float  e, eHold, pk;        // the envelope (its state), the sustain left (samples), this ping's peak
    int    est;                 // its stage: 0 attack, 1 sustain, 2 decay, 3 done
    float  y, vs;               // gate 1: the lpg vactrol's light (0 .. 1: its control, 0 .. 10 V), its state
    double sx, so, sd, xo;      // gate 1's network: its integrators' states (C2, C1, C3), its last output (the tanh)
    WsGateK gk;                 // ... its coefficients for the cell's resistance now
    float  gn, fc;              // its gain to the level heard wide open, its pole (the screen)
    int    cnt;                 // samples to the next update of the gate's coefficients
    double next;                // samples to the next pulse
    float  iv, ivNow;           // the interval now (samples); the one running (the pulser's ramp)
    int    left, np, sw;        // pulses left (-1: endless), pulses so far, swing's long / short
    float  pl, amp, ampT;       // the next pulse's level; the level heard (glided to its target)
    float  r1, r2;              // RAND's draws (TIMB, MOD)
    int    step;                // the sequence's step playing
    float  flash;               // seconds since the last ping (the screen)
    float  dr, drT, drC;        // DRFT's walk, its target, samples to the next
    float  wl, hx, hy, hx2, hy2, dc; // WARM's low-pass, the two high-passes before the gate (SYMM's DC out), the DC out
    float  gv, gl;              // gate 1: VCA's vactrol (go.vactrol), LPF's (go.vactrol on the cutoff)
    Svf    f1, f2;              // gate 1, gate 2: LPF's filters
    float  y2, vs2, gv2, gl2;   // gate 2: its lpg vactrol (light, state), VCA's, LPF's
    float  zOut;                // the voice's last output (PRE)
    float  ed;                  // the envelope detector
    float  rnd;                 // RANDOM's value
    float  mo;                  // the modulation oscillator now
    float  pT, pC;              // TIMB and MOD at the last chunk (ramped across the next)
    float  lvl, Anow, cnow;     // the screen: the level, the folder's drive and the modulation now
    float  c1, c2, egv, pul, sv;// the screen: the gates' controls, the envelope out, the pulser's ramp, the sequencer's value
    Rng    rng;
};

} // namespace sk
