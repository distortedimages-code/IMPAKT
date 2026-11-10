// IMPAKT — the FX machines (machines 30..34, and 39: FUZZ, sk_fuzz.cpp; 42: WEFT, sk_weft.cpp, Mutable Instruments'
// Warps). Own implementation (WEFT: Warps' code, MIT).
//
// A track on an FX machine plays no sound of its own: it takes other tracks' sound, in stereo as it would reach the
// mix (their pan, their sides) — SRC: every track to its left (SUM, but those an FX track before it already takes:
// two FX tracks in a row are a chain), the track just before (NBR), or one track (T1..T8) — and passes it through its
// effect, in stereo, as an insert: the source no longer reaches the mix by itself, the FX track gives back the direct
// sound and the effect together (MIX), then its own TONE, pan (a balance) and sends. The effect runs all the time;
// everything on it is sequenced like any sound: its knobs take p-locks, LFOs, followers and DICE, its trigs fire its
// gesture (ACT, on the MORE side):
//   IN       a trig opens the effect for DEC (HOLD: always open). VERB and ECHO take the source in while it is
//            open (their tails ring on: a dub throw); the others fade from the direct sound to the effect.
//   gesture  the effect stays open; a trig fires its gesture for DEC (HOLD: until the next trig): VERB freezes its
//            tail or plays its pre-delay backwards, ECHO holds its loop or plays it backwards, BEND holds its last
//            division, TAPE stops, CODEC loses its packets.
//
//  VERB   a reverb for big, resonant, unreal spaces (after the design Tom Erbe published for the Erbe-Verb, ICMC 2015,
//         grown to eight lines, in stereo): eight delay lines in a feedback matrix, an allpass in front of each, SIZE
//         from a cupboard (metal) to a stadium, every length (the early reflections too) on it, sliding (a sweep
//         bends the whole tail), DECAY up to 125 % (it sustains, then saturates), ABSRB, BASS (the lows decay longer:
//         its body), SHFT (half the lines shifted in pitch a pass: shimmer, falling spirals, a detune), TILT, the
//         lengths moved (SPED, DPTH up to their whole length) by a sine on each line or by grains of random lengths
//         (SHAPE), PRE-delay (played backwards: REV).
//  ECHO   a stereo delay in four zones (KARP: combs and plucked strings, SLAP, ECHO, LONG), its time sliding like a
//         tape's when RATE moves, REPT (above 100 % it runs away, held by a saturation), COLR (from a dark, saturated
//         tape to a thin bright line), HALO (its repeats blur into a cloud), µ (a drift: chorus, flanger, wow), SKEW
//         (the right channel's time against the left's), PING (the repeats cross from side to side), DUCK (the
//         repeats step back under the playing), SYNC (the time on the tempo's divisions).
//  BEND   a circuit-bent stereo buffer: on a grid (TIME, on the tempo or free) each division may replay the last one
//         cut into repeats (RPTS), bent in pitch (BEND), or broken (BRK: a jump back, backwards, a tape stop, a
//         dropout, at double speed); CRPT (DECIM, DROP, DSTR); SPRD (the replays smeared apart, left and right).
//  TAPE   a worn stereo tape: WOW, FLUT(ter), SAT(uration, through the record and playback emphasis), GEN (each
//         generation loses its top, gains hiss), FAIL (dropouts, crinkles, snags), HISS, four machines (CASS, VHS,
//         REEL, MICRO), WIDE (the two tracks of the tape wander apart), TONE.
//  FUZZ   (v68, sk_fuzz.cpp) three bands (Linkwitz-Riley crossovers, XLO and XHI), each through the circuit of the
//         two-transistor germanium fuzz of 1966, solved from its schematic (two PNP germanium transistors in Ebers-Moll,
//         shunt-series feedback, the 1k FUZZ pot and its 20 uF bypass, the 470 / 8.2k collector load, the 2.2 uF input
//         capacitor and the pickup's 10k in front of it): the curve it gives at audio rates, for each setting of the
//         pot and of BIAS (to the left the battery sags, 9 V down to 3.4 V: it starves, sputters, gates; to the right
//         its first stage is biased hotter: the second one saturates, woollier); the input capacitor charging under a
//         loud sound (the bias shifts: the decay sputters and gates as the real one does). FUZZ the pot, then the level
//         into it (a booster: up to +34 dB, a square), LOW / MID / HIGH each band's drive (at 0: CLEAN, past the circuit), SUST each
//         band compressed into the fuzz (a wall), GATE (keyed on the source), TONE, MIX; 4x oversampled (86 samples late,
//         1.8 ms at 48 kHz, direct sound included). Its gesture (STRV): a trig drains the battery for DEC.
//  CODEC  a low-bitrate codec in joint stereo (mid and side coded apart): LOSS (the bands below its masking threshold
//         go, the rest quantised), BAND (its bandwidth), SPED (from smeared to garbled), JITR (phase errors), PKTS
//         (packets lost: repeated or gone), MODE (what it keeps, or what it throws away), RESO, JOIN (the side gets
//         fewer bits: the image folds in), FILT. It runs 256 samples late (5.3 ms at 48 kHz).
#pragma once

#include "sk_math.h"
#include "sk_fft.h"
#include "sk_os.h"

namespace sk {

enum {
    FX_MEM = 1 << 20,                // floats of memory per track (4 MB): the lines of the machine it plays (stereo)
    FX_SRCS = 10,                    // NBR, SUM, T1..T8
    FX_INFO = 13,                    // (v71: FUZZ's SUB level, the 13th)
    FX_VL = 8,                       // VERB: its delay lines (an 8-line feedback delay network, 4 a side)
    FX_VG = 4,                       // VERB: the grains of random length on each line (SHAPE)
    FX_VSH = 4,                      // VERB: the lines SHFT shifts in pitch (half)
    FX_CFN = 256, FX_CHOP = 64, FX_CNB = FX_CFN / 2 + 1, FX_CBANDS = 21,
    FX_TAPES = 4,
    FX_CHUNK = 32,                   // (= CHUNK)
    FZ_N = 257, FZ_BIAS = 9, FZ_POT = 9,  // FUZZ: the circuit's curves (input points), BIAS and pot settings tabulated
    FZ_SUBDL = 128                   // FUZZ: SUB's CLEAN delay (a power of two over the 86 samples)
};

struct FxGrain                       // BEND: a piece of the buffer being played
{
    double src, acc, span, rate, rate1;  // where it starts (absolute sample), how far it has read, the span it loops in, its rate (and where it glides to)
    double len, ph;                  // a repeat's length (output samples), how far into it
    int    dir, on, live;
    float  gain, gain1;
};

struct FxState
{
    int    kind;                     // the machine its memory is laid out for (-1: none)
    u32    srcMask;                  // the tracks it takes this chunk (a bit each: fxPrepass)
    u32    carMask;                  // (v78, WEFT: the track its carrier is, taken too)
    float  sr;
    // the trigs: one since the last chunk, its velocity; the IN gate; the gesture
    int    trig; float trigVel, trigNote;   // (v83: the trig's NOTE, STENCIL's KARP and COMB)
    float  gate, gEnv, gPeak;
    int    gest; double gestLeft; float gestF;
    float  mixS, volS;
    float  inPk, outPk;
    Rng    rng;
    // the side (stereo) the track gives (TONE'd like the track, then spread like HALO's)
    float  side[FX_CHUNK];

    // ---- VERB
    int    vLmax, vLmask, vPre0, vPreMask, vAp0, vApMax, vApMask, vSh0, vShMask, vW, vPreW, vGq;
    float  vDSm[FX_VL], vASm[FX_VL], vLp[FX_VL], vHp[FX_VL], vLo[FX_VL], vGr[FX_VL][FX_VG];
    float  vShF[FX_VSH];                 // SHFT: the shifted paths' filter (a low-pass going up, a high-pass going down)
    float  vPh, vGPh, vShPh, vShMix, vPreSm, vFrz, vRev, vRvAge, vHpIn[2], vTlo[2], vRt, vSize;

    // ---- ECHO (two lines, left and right; four allpasses each)
    int    eLine, eMask, eAp0[8], eApLen[8], eApW[8], eW;
    double eD;
    float  eMuPh, eMuR, eMuRT, eMuRC, eHold, eFlip, eTime, eFb, eDuck;
    Svf    eLpL, eLpR; float eHpL, eHpR;
    float  eGa[2]; int eGn;            // FLIP: its two grains' ages (-1: none), the newer one

    // ---- BEND (a buffer a side)
    int    bMask; double bW, bDivIdx, bDivStart, bPrevStart, bFreeCnt, bHoldStart; float bPrevLen;
    FxGrain bCur, bOld, bOld2; float bFade, bFadeInc, bFade2, bFadeInc2;
    int    bEv, bRep, bDivN; float bHoldF, bHoldLen;   // (bDivN: divisions so far, for the screen)
    float  bDecH[2], bHCnt, bDropG, bDHold; int bDropLeft;

    // ---- TAPE (two tracks on one transport)
    int    tMask, tW;
    double tStopLag, tXLag; float tSpeed, tXf; int tStopPh;
    float  tD0, tWowPh, tWowR, tWowRT, tWowRC, tFlPh, tFlN[2], tWowR2, tWowRT2, tFlN2[2], tSnag, tSnagT, tSnagLen, tSnagAmp;
    float  tDrop, tDropT, tDropLeft, tDropLen, tCrk, tCrkT, tCrkLeft;
    float  tPeLp[2], tDeLp[2], tHpS[2], tPink[2][3];
    Svf    tLp[2], tBump[2], tLp2[2];
    float  tTlo[2], tCents; double tLagPrev;
    float  tEin, tEout, tMk;             // the saturation's level given back (both tracks together)

    // ---- CODEC (mid and side)
    RFFT<FX_CFN> cfft;
    float  cWin[FX_CFN], cIn[2][FX_CFN], cOla[2][FX_CFN], cDry[2][FX_CFN], cFx[FX_CFN];
    float  cRe[2][FX_CNB], cIm[2][FX_CNB], cPRe[2][FX_CNB], cPIm[2][FX_CNB];
    float  cKeep[2][FX_CBANDS], cOff[2][FX_CBANDS];
    int    cRing, cHop, cPktLeft, cPktMode; float cBwN, cBw, cKept;

    // ---- FUZZ (v68): per side its oversampler, its crossovers (LR4 at XLO: two low-pass sections, two high-pass; the same
    // at XHI; the low band's all-pass at XHI), per band the input capacitor's charge and SUST's envelope; the gate (keyed on
    // the source), TONE, the output's high-pass; the circuit's curves in use (blended from the tables for this BIAS and
    // FUZZ), each band's drive and make-up; the crossovers set; the mix (glided); the screen's
    Oversampler4 zOs[2];
    Svf    zX[2][9];
    float  zDelta[2][3], zSus[2][3], zSusG[2][3];
    float  zGEnv, zGOpen, zGHold, zGGain;
    float  zTone[2], zHp[2];
    float  zOut[FZ_N], zCur[FZ_N], zKey[2], zMk[3], zGb[3], zXlo, zXhi, zMix;
    float  zPk[3], zDrift, zBias;
    float  zD[2][1];                 // a sample at 4x: the round trip 86 samples late, a whole number
    // SUB (v71): the detector's low-pass (at XLO), its envelope, the comparator, the two flip-flops, the period between
    // the comparator's rising edges (samples, smoothed) and the count since the last one, the lock (edges coming); PURE's 4-pole low-passes (two
    // sections, -1 and -2 octaves), GRIT's; the last sub (FUZZ's path: its 4x steps); CLEAN's delay (86 samples, as the
    // oversampled path); the screen's peak
    Svf    zSubDet, zSubP[2][2], zSubG[2];
    float  zSubEnv, zSubCnt, zSubPer, zSubPrev, zSubPk, zSubLk;
    int    zSubCmp, zSubF1, zSubF2, zSubW;
    float  zSubDl[FZ_SUBDL];
    // v73: the other circuits (CIRC, S5; sk_fuzz): per side and band their filters' and charges' states, BENT's sticky
    // rail (samples left at 4x, the value it holds) and its draws; the circuit last run (a change: the states start over)
    float  zCs[2][3][4];
    int    zLat[2][3];
    float  zLatV[2][3];
    u32    zRng;
    int    zCirc;

    float  mem[FX_MEM];
};

} // namespace sk
