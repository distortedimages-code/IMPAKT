// IMPAKT — CHIMERA (v66): a sampler on the bank (sk_bank.h) with two ideas of its own.
//
// FAMILLE: VARI (C) swaps the sample each hit plays for one of its neighbours in the bank, the ones that sound most
// like it (sk_bank.cpp bankNear): to the right a draw at each hit, to the left a draw per step that comes back with the
// pattern (RDEC's LOOP); how far it goes, how many neighbours (1 .. 12). Its page: SENS (alike in shape, in timbre, or
// both), MEME (its category only), CIBL (the attack, the body or both), ACCD (a neighbour played at the pitch of the one
// it stands in for).
//
// GREFFE: the attack of the sample (A) on the body of another (B, its page: BCAT, BSMP). From the graft point (POIN,
// 1 .. 150 ms after the start) the sound crosses (FOND, 0.5 .. 40 ms) from A to B, B read in step with A (its own attack
// under A's, unheard), at A's pitch when both have one (the nearest note of the same name; BTUN around it), its phase
// lined up at the seam; SPLT keeps A over a frequency through the body (B under it). BLVL: B's level.
#pragma once
#include "sk_bank.h"

namespace sk {

enum { CHIM_NEAR = 12, CHIM_INFO = 24, CHIM_CHUNK = 32 };

struct ChimState
{
    int    a, b;                // the samples (attack, body: -1 none)
    double pa, a0, aEnd, aOn;   // A's read position (A frames), where the sound starts in it, where it ends, A's onset
    double kap, bOn;            // B frames per A frame, B's onset (B frames, the alignment in it)
    float  ra;                  // A frames per output sample
    float  gA, gB;              // their gains
    double gStart; float gFd;   // the graft point (A frames) and its crossfade
    float  split;               // the split (Hz; 0 none)
    Svf    la, ls, lb, lbs;     // the split's low-passes: A middle, A sides, B middle, B sides
    int    rev;                 // reversed
    float  env, envC;           // DEC's envelope and its coefficient (FULL: 1)
    float  fade, fadeInc;       // the start's fade-in (reversed, or a start inside the sample)
    float  semis;               // the trig's transposition (TUNE + NOTE) at the trigger
    float  side[CHIM_CHUNK];    // this chunk's sides (renderVoice filters them as the middle and adds them to the track)
    Svf    sf;                  // their filter (follows the voice's)
    float  sDx, sDy;            // and the voice's DC blocker (10 Hz), as on the middle
    int    hasSide, done;
};

} // namespace sk
