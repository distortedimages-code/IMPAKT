// IMPAKT — organic one-shot bank (twigs, branches, crisps, fire, gravel, drops, claps...).
// Inspired by the multi-particle approach of the WMD Fracture: every sound exists in several
// variants that the machines pick in a pseudo-random order, so repeated hits never sound
// identical. The original Fracture samples are proprietary, so everything here is synthesised
// once at start-up with physically-informed models: modal synthesis with material damping
// (van den Doel), stochastic particle collisions (Cook's PhISEM), crackling-noise event
// streams with power-law energies (Sethna / Houle), Minnaert bubbles with rising pitch,
// bouncing-object timing and the hand-clap resonance model of Peltola et al.
#pragma once
#include "sk_math.h"

namespace sk {

enum {
    NUM_ORG  = 36,       // sounds
    ORG_VAR  = 4,        // variants per sound
    ORG_SR   = 48000,    // the bank is rendered at a fixed rate, players resample
    ORG_POOL = 2900000   // floats for every variant (~60 s of audio; about 2.66 M are used)
};

struct OrgBank
{
    int   off[NUM_ORG][ORG_VAR];
    int   len[NUM_ORG][ORG_VAR];
    int   used;
    float pool[ORG_POOL];

    void build();
    inline const float* data (int id, int v) const { return pool + off[id][v]; }
};

} // namespace sk
