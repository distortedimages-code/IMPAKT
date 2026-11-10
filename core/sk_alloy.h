// IMPAKT — ALLOY: metallic sounds (machine 38). Own implementation, after the acoustics of struck metal and the ways
// SOPHIE built her metals and drums: an impact rings an object made of modes (physical modelling), and the ways to the
// metal of synthesisers lie over it and after it (FM on inharmonic ratios with its envelopes, pure sines stacked at
// the top, a resonant comb, a noise as it is, the sound fed back, a drive, a cold 12-bit converter), into its own
// metallic space (the VERB machine's network, from a box of millisecond lines to a hall) and a gate. v63: six pages.
//
//  OBJECT  (A..E) SHAP its shape: WIRE (a stiff wire: its harmonics, stretched by STIF), BAR (a free bar: 1, 2.76, 5.40 ..),
//          TUBE (a tubular chime: the same modes, the note heard an octave under its fourth), PLATE (a rectangular
//          plate), DISC (a free disc: Chladni's (m + 2n)^2, its near pairs beating), BELL (a minor-third bell), GONG (a
//          tam-tam: denser, and struck hard its energy climbs into the high modes after the hit) or BOWL (a bowl: its
//          modes in doublets); STIF its stiffness (the series squeezed towards the note or stretched away: soft
//          .. as built .. glassy), MASS its density (light: the high modes die at once, wood or plastic; dense: they
//          ring as long as the low ones, steel, brass), SIZE how many modes ring (6 .. 48: a small chime .. a sheet),
//          HIT where it is struck (the edge .. the middle); DEC how long it rings (HOLD: as long as the gate is open).
//  IMPACT  MALT the mallet (felt .. steel: its contact 8 ms .. 80 µs), CLIK the transient (the first milliseconds
//          pushed up), PENV a pitch envelope at the hit (down to the note from up to four octaves over it, or up to
//          it from three under) that falls in PTIM (1 .. 200 ms: the metal's click .. a slide), PRIS how long it takes to
//          get there (0: at once; else it rises first: an arc, a squeak, a rubber band), BNCE the mallet again (to the
//          right it bounces, sooner and softer each time; to the left a flam: up to four strikes close together, a
//          clap), ORGN the life in it (each hit a little different: its pitch, its place, the mallet, every mode's
//          tuning and level; the pitch pushed up by a hard hit, settling as it rings; a slow drift).
//  NOISE   NOIS a noise in the impact (it rings the object), HISS a noise heard as it is (a snare's wires, a hat, a
//          clap's hands; its level the same whatever its colour), NCOL their colour (dark .. thin: a low-pass 120 Hz ..
//          open, then a high-pass up to 10 kHz), NLEN how long (a burst of 1 ms .. 2 s; HELD: a bow, a
//          scrape, as long as the gate is open), FDBK the sound fed back into the object (it rings longer, then sings
//          by itself, held under a ceiling that falls with DEC), DRIV a drive (warm, uneven: the fat of a kick; its
//          peak kept), DAC the converter: OFF, 16 bits, 12 bits, then
//          12 bits at 32 kHz and 24 kHz, 10 bits at 16 kHz, 8 at 11 kHz, 6 at 8 kHz.
//  FM      OBJ the object's level; FM a carrier on the note and its modulator on RATI (0.5 .. 16, sitting on the
//          whole numbers and on the inharmonic ones: √2, φ, √3, e, π ..), INDX how deep, IATK how long it takes to
//          get there (0: at once .. 2 s, a swell), IDEC how fast it falls (HOLD at the top), MENV the modulator swept
//          to its ratio over PTIM (from three octaves under .. none .. three over: the metal's zap).
//  COLOR   ADD a stack of pure sines over the top (one to 24 as the object grows: the first on BAND itself; spread
//          around BAND: twice .. 48 times the note), each its own decay; COMB a resonant comb on the sum (CTUN its pitch from the note, CFB its
//          feedback: past 0 all the harmonics, under it the odd ones, a tube), FLNG its delay swept (a flanger in the
//          loop) and PHAS a phaser after it, both on one LFO (RATE, on the SPACE page, restarted at each hit).
//  SPACE   VERB the machine's own reverb (the VERB machine's network), ROOM its size (a metal box, lines of a
//          millisecond: a ringing, combed space .. a tin room .. a hall), DCAY how long (80 ms .. 4 s at any size), DAMP
//          how dark, SHFT its tail
//          shifted in pitch a pass at a time (OFF, an octave down .. a detune .. an octave up: it climbs, it falls),
//          CUT a gate on all of it (OFF, or shut 20 ms .. 1.5 s after the hit: a gated snare), RATE the LFO (FLNG,
//          PHAS, and the space's lines with it).
#pragma once

#include "sk_math.h"

namespace sk {

enum { AL_M = 48, AL_ADD = 24, AL_SHAPES = 8, AL_SIDES = 6, AL_DACS = 8, AL_COMB = 4096, AL_PH = 6, AL_HN = 65 };

struct AlloyState
{
    float y1[AL_M], y2[AL_M];          // the object's modes (two states each)
    float aRe[AL_ADD], aIm[AL_ADD];    // ADD: its sines (each a decaying phasor)
    float mPos, mLen, mLenN, mAmp;     // the mallet: its contact so far, its length (samples; at the hardest hit), force
    float bNext, bInt, bAmp;           // BNCE: the next bounce (seconds), the gap, its force
    float t;                           // seconds since the hit
    float cE, tE;                      // CLIK: the transient's envelope, the tick's
    float nEnv; int nStage;            // NOIS: its envelope, its stage (0 starting, 1 sounding, 2 spent)
    float nLp, nBp, nHl;               // its colour (state-variable low-pass, the high-pass's state)
    float pSn;                         // PENV: the share left (1 at the hit, again at a bounce)
    float fPh[2];                      // FM: the carrier's and the modulator's phases (at twice the rate)
    float fEnv, fIdx;                  // its level and its depth (1 at the hit)
    float env, peak;                   // the object's level (a follower) and its widest (GONG's bloom)
    float bHp;                         // GONG: its swell (0 at the hit .. 1)
    int   cw; float cLp;               // COMB: its line's write (in the voice's lbuf), its loop's damping
    float apx[AL_PH], apy[AL_PH], apFb; // PHAS: the all-pass stages, the feedback
    float lfo;                         // FLNG / PHAS: the LFO's phase
    float dPh, dHold, dL1, dL2;        // DAC: its clock, the word held, its output filter
    float sObj, sFm, sAdd, sCmb, sPha; // the layers' levels, glided
    float lim;                         // the output's safety: its peak follower
    float rel;                         // the damper (0 off .. 1 down)
    int   gated;                       // the note has a gate (GATE, a pad, HOLD): its end brings the damper down
    float dcx, dcy;                    // the output's DC blocker
    float lvl;                         // peak follower (the voice's end)
    float gNorm;                       // the output level against the modes' gains (glided)
    float simPk;                       // the first peak against the modes' sum (a dry run, once a note)
    float addAtt;                      // ADD: its fade in
    // v63
    float pR, pNorm, pRk;              // PRIS: the rise (0 .. 1), the arc's top brought to 1, its rate
    float mSw;                         // MENV: the share of the sweep left (1 at the hit)
    float fAtk;                        // IATK: the depth's rise (0 .. 1)
    int   bMode, bLeft, bK;            // BNCE: 0 none, 1 bounces, 2 flams (how many left, which)
    float fGap, nFast;                 // the flams' gap (seconds); the burst's decay cut short (a flam before the last)
    float hNorm, hCol;                 // HISS: its level against its colour's (measured at the hit), for which colour
    float fbD, fbLvl, fbE;             // FDBK: what goes back in (the modes' speeds), the loop's level,
                                       //  its ceiling (1 at the hit, falling with DEC)
    float dvX, dvE;                    // DRIV: the last input (its antiderivative's step), the peak going in
    float cutG;                        // CUT: the gate (1 open .. 0 shut)
    float oPit, oHit, oLvl, oTen, oDr, oDrT; // ORGN: this hit's pitch (semitones), place, level; the tension (1 at the
                                       //  hit), the drift and where it goes
    u32   oSeed;                       // its draw for the modes (tuning, level)
};

} // namespace sk
