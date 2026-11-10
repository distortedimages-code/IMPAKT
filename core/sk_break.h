// IMPAKT — BREAK: a break slicer (machine 35). Own implementation.
//
//  SLOTS    ten loops the user loads (shared by every BREAK track of the project). The host hands each one over mono, at
//           32 kHz, levelled and cut to its bars (breakSet: the page decodes the file, the plugin keeps the slots in its
//           state); the engine then reads it, a little every block (breakWork): 16 slices a bar on the grid, each one
//           moved onto the attack nearest to it, what it starts with (kick, snare, anything else), its waveform.
//  SLICES   a trig plays its slot from a slice: its own step's (STEP: trigs on every step play the loop as recorded) or
//           the one SLCE (or NOTE) names, for LEN 16ths or on to the next trig. SYNC keeps the loop on the project's
//           tempo without changing its pitch: each 16th of it starts on the grid (the project's swing too) and plays to
//           its end (a slower tempo leaves a breath after each, a faster one cuts them). VARI plays it like a sample:
//           speed and pitch follow the project's tempo against the loop's own. TUNE transposes.
//  CHOP     what each trig may do instead, by chance: the same 16th of another loaded slot (SWAP: the bar goes on, in
//           another break), another slice (RAND, preferring one that starts like its own), backwards (REV), its start
//           repeated (STUT), an octave, a fifth or a fourth away (PJMP), silence (SKIP); SEED fixes the draws to the step
//           (the same chops on every pass). DUST: the record and the sampler (crackle, hiss, a darker top, a softer,
//           crunchier stage).
#pragma once

#include "sk_math.h"

namespace sk {

enum {
    BRK_SLOTS = 10, BRK_MAXBARS = 8, BRK_MAXLEN = 16 * BRK_MAXBARS, BRK_NPEAK = 128, BRK_LENS = 8, BRK_SEEDS = 33,
    BRK_RATE = 32000,
    BRK_CAP = 20 * BRK_RATE,                 // samples a slot holds (20 s)
    BRK_HOP = 64, BRK_HOPS = BRK_CAP / BRK_HOP + 2,
    BRK_SEGS = 4,                            // pieces of a loop sounding at once (the one playing, the ones fading)
    BRK_HEAD = 16,
    BRK_INFO = BRK_HEAD + BRK_MAXLEN + BRK_NPEAK,
    BRK_SLOT_INFO = 4                        // per slot, for the hosts: state, tempo, bars, seconds
};

struct BrkInfo
{
    float bpm;
    int   bars, len, L16;                    // bars, samples, 16ths
    int   onset[BRK_MAXLEN];                 // each slice's first sample (on its attack)
    unsigned char cls[BRK_MAXLEN];           // what it starts with: 1 kick, 2 snare, 0 anything else
    unsigned char str[BRK_MAXLEN];           // how hard (0..255)
    unsigned char peak[BRK_NPEAK];           // the waveform, for the screens
};

struct BreakBank
{
    int     ver;                             // moves on every change (the hosts show the slots again)
    int     state[BRK_SLOTS];                // 0 empty, 1 waiting to be read, 2 ready
    int     len[BRK_SLOTS], bars[BRK_SLOTS]; // as handed over
    BrkInfo b[BRK_SLOTS];
    // the slot being read: where, the band filters (low, mid, high: two biquads each), each band's energy per hop
    int     job, jpos, jhop, jacc;
    float   z[6][2], c[6][5];
    float   acc[3];
    float   env[3][BRK_HOPS];
    float   onv[BRK_HOPS], tmp[BRK_HOPS];
    short   pool[BRK_SLOTS * BRK_CAP];

    short*       mem (int s)       { return pool + s * BRK_CAP; }
    const short* mem (int s) const { return pool + s * BRK_CAP; }
    void  reset();
    int   ready (int s) const      { return s >= 0 && s < BRK_SLOTS && state[s] == 2; }
    int   resolve (int s) const;                // the slot played for a knob on s: s, else the next one ready (-1: none)
    int   work (int budget);                    // reads the waiting slots, budget samples at most; 1 when one is done
    void  finish (int s);
};

// a piece of a loop that sounds: from a slice's attack to the next one (SYNC), or on (VARI), forward or backwards
struct BrkSeg
{
    int    on, brk, dir, loop;
    double pos, stop;                        // where it reads (samples, from the loop's start), where it ends
    float  rate;                             // (unused)
    float  g, gInc;                          // its gain, and where it goes (in at the start, out when cut)
};

struct BreakState
{
    BrkSeg s[BRK_SEGS];
    // the cursor: the trig's slot and slice, how far it has gone (16ths of the loop), how it runs
    int    on, brk, dir, stutN, pj, skip;
    double b0, tau;
    float  rlen, stutLen;
    int    ph, pi;                           // what it plays next: 0 STUT's repeats, 1 REV's window, 2 the pieces on, 3 VARI's run; how many
    double next, f0;                         // when it starts the next piece (16ths since the trig), where the pieces on begin
    int    base, k0;                         // the slice they begin from; the trig's step (the swing's parity)
    float  env, envT, gate, rel;             // DEC's fade (env glides to envT), the LEN gate (samples), the release
    int    flags;                            // the last trig: 1 RAND, 2 REV, 4 STUT, 8 PJMP, 16 SKIP, 32 SWAP
    int    slice;
    float  lp1, lp2, crk, hp;                // DUST: tone, crackle, its high-pass
    float  pos;                              // the cursor (16ths of the loop, wrapped), for the screen
};

} // namespace sk
