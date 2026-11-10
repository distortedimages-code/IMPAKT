// IMPAKT — CUBE (v74): a dual wavetable voice in a cube of 512 waves. Own implementation, a tribute to the dual 3D
// wavetable oscillators of the modular world (the waves set out in a cube, X, Y and Z, morphing from one to its
// neighbours), built for trigs and p-locks rather than voltages.
//
//  THE CUBE  8 banks (Z) of 64 waves, 8 x 8 (X, Y), of 256 samples: the format of the wavetable editors (WaveEdit and
//            the like): a 16-bit mono WAV of 64 waves of 256 samples is a bank. Two cubes: FACT, made here (8 banks of
//            its own, see sk_cube.cpp), and USER, the banks the page loads (a bank not loaded plays FACT's). Each wave
//            kept band-limited: 7 versions (127, 64, 32 .. 2 harmonics), 512 samples each (read with a linear
//            interpolation at twice the wave's own resolution: its images stay low), 16-bit.
//  A, B      two oscillators reading the cube where X, Y, Z say (trilinear: the 8 waves around the point, each
//            band-limited for the pitch it plays at, two versions crossfaded as the pitch moves); B at its interval
//            (BRAT, BDET) and offset in the cube (BX, BY), heard at BLVL and modulating A (FM: linear, thru zero).
//  MAIN      X Y Z, FM, SWEP (an envelope on X at each hit, bipolar: it starts there and comes back over STIM), SIDE.
//  OSC       BRAT BDET BX BY BLVL UNIS (A doubled, detuned) SET (FACT / USER).
//  MOD       SWPY (the sweep on Y), STIM, PENV (a pitch envelope, +-4 octaves) PTIM, STEP (which axes jump from a
//            wave to the next instead of morphing), WARP (the phase bent: brighter either way), BITS (OFF .. 4 bits).
#pragma once
#include "sk_math.h"

namespace sk {

enum { CUBE_SRC = 256, CUBE_LEN = 512, CUBE_MIPS = 7, CUBE_BANKS = 8, CUBE_BANK_W = 64, CUBE_WAVES = CUBE_BANKS * CUBE_BANK_W,
       CUBE_BANK_N = CUBE_BANK_W * CUBE_SRC, CUBE_INFO = 140, CUBE_STEPS = 4, CUBE_BITS = 6 };

// the band-limited versions of a cube's waves (16-bit: a value of 16384 is 1, headroom for their overshoot)
struct CubeTables { short w[CUBE_WAVES][CUBE_MIPS][CUBE_LEN]; };

// the banks the page loads (USER): as loaded (what the plugin saves), how long, whether their versions are made
// (0 none: FACT's play; 1 being made: FACT's play; 2 ready), the bank and wave being made, a count of the changes
struct CubeUser
{
    CubeTables t;
    short raw[CUBE_BANKS][CUBE_BANK_N];
    int   len[CUBE_BANKS], ready[CUBE_BANKS];
    int   busy, wave, ver;
    short stage[CUBE_BANK_N];        // (the page writes a bank here, then hands it over)
};

// a voice: its phases (A, A's unison copy, B), its sweep and pitch envelopes, where A and B were at the last sample
// (they glide from there to where the settings put them over a chunk), the hold of BITS
struct CubeState
{
    double pa, pa2, pb;
    float  swe, pe;
    float  ax, ay, az, bx, by;
    int    fresh;
};

// the factory cube (made once)
void cubeFactory();
const CubeTables& cubeFact();

} // namespace sk
