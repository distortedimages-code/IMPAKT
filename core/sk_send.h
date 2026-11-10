// IMPAKT — the send effects of v68, beside the DELAY and the REVERB (the FX page: SEND, then each one's module). Own code,
// modelled on how the studio units of their kind are built, as their published descriptions and owner's manuals give it
// (no manufacturer's code, nothing measured from a unit: the numbers they do not publish are chosen, and said so below).
//
//  CHORUS   the "dimension" studio chorus of 1979: two bucket-brigade lines, one per side, their clocks swept in
//           antiphase by one slow triangle; each line heard on its own side and, opposite in polarity and high-passed,
//           on the other one (in mono the two cancel above the cross-over: the sound widens without going out of tune);
//           a compander around the lines (a 2:1 compressor with pre-emphasis before them, its 1:2 expander and the
//           de-emphasis after), their anti-alias and reconstruction filters, the hiss under the expander, a touch more
//           bass on the direct way. Four modes, the softest to the strongest (the manual gives no rates nor depths:
//           0.25 Hz for 1 to 3 and 0.5 Hz for 4, a clock swept by ±7 % to ±24 % around 4.5 ms are this model's).
//           Its input MONO (both lines take the sum) or STEREO (each its side).
//  PHASER   the dual opto phaser of 1971: two phasors of six first-order all-pass stages, each stage's resistor a
//           photocell lit by its sweep (it brightens fast and darkens slowly, more slowly the darker it gets: the sweep
//           leans, a square one rounds off), feedback around each; two sweep generators (0.1 .. 18 Hz, sine to square);
//           phasor B on generator 1 or 2, its sweep with A's or against it (REV); STEREO (A on the left send, B on the
//           right), MONO (both on the sum) or SERIES (B after A: twelve stages on the right). The send gives back the
//           phase-shifted sound only: added to the track's own, it digs the notches (all the way at SEND 100 %, LEVEL
//           80 %), as the unit's mix does.
//  DELAY 1982 (sk_engine.cpp): the rack digital delay of 1982 (its memory clocked by a VCO: TIME sets the clock, the
//           pitch of the repeats glides when it moves; x1 = 16 kHz of bandwidth, x2 = 6 kHz, for twice the memory;
//           a 12-bit converter that ranges its gain; a limiter at its input; the repeats go round through the converters
//           every pass, HOLD loops the memory as it is) — Echo82 below.
#pragma once

#include "sk_math.h"

namespace sk {

enum { CHO_LEN = 4096, CHO_MODES = 4, PHS_STAGES = 6, PHS_ROUTES = 3 };

struct ChorusParams { int mode, stereoIn; float level; };

struct DimChorus
{
    float sr;
    float bufL[CHO_LEN], bufR[CHO_LEN]; int w;
    float ph, depth, rate;                 // the triangle's phase; the mode's sweep and rate (glided)
    Svf   aa[2][2], rc[2][2];              // per side: anti-alias, reconstruction (4th-order Butterworth each)
    float pe[2], de[2], ec[2], ee[2];      // emphasis, de-emphasis, the compressor's and the expander's rectifiers
    float hp[2], bass;                     // the cross-mix's high-pass (per line), the direct way's low-pass (mono)
    float cut;                             // the filters' cutoff they were set for
    Rng   rng;
    int   quiet;                           // samples of silence in and out (asleep past CHO_SLEEP: nothing runs)
    void init (float sampleRate);
    // adds the chorus of inL / inR (the send) into outL / outR, scaled by gain (its level and its ON key's fade)
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const ChorusParams& p, float gain);
    void clear();
};

struct PhaserParams { float rate1, shape1, rate2, shape2, depthA, fbA, depthB, fbB; int bGen2, rev, route; };

struct BiPhaser
{
    float sr;
    float sA[PHS_STAGES], sB[PHS_STAGES];  // the stages' states
    float yA, yB;                          // the last outputs (the feedback)
    float ph1, ph2;                        // the generators' phases
    float lA, lB;                          // the photocells' light
    float kOn, kOff, kOffDark;             // their response (per sample: brightening, darkening when lit, when dark)
    int   quiet;
    void init (float sampleRate);
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const PhaserParams& p, float gain);
    void clear();
};

// the 1982 delay's machine side, per channel: the input's limiter, the converters and their filters; the memory is the
// engine's (dl / dr)
struct Echo82
{
    Svf   aa[2][2], rc[2][2];              // anti-alias, reconstruction (per side, two sections each)
    float xPrev[2], hold[2];               // the input a host sample ago (where a tick falls between), the DAC's held value
    float limEnv, limG;                    // the optical limiter (both sides)
    float cut;                             // the filters' cutoff now
    double ph;                             // the machine clock against the host's (ticks to come)
    int   w, N;                            // the write index (machine samples), the memory's length in use
    float c, fs0;                          // the clock now (x the nominal), glided; the nominal (x1 40 kHz, x2 20 kHz)
    float vph, env, vco;                   // the VCO's LFO phase, the input's envelope, the VCO at the end of the last chunk
    void  clear();
};

// a 4th-order Butterworth low-pass as two sections
inline void setLp4 (Svf* s, float fc, float sr) { s[0].set (fc, 0.5412f, sr); s[1].set (fc, 1.3066f, sr); }
inline float lp4 (Svf* s, float x)
{
    float lp, bp, hp;
    s[0].tick (x, lp, bp, hp); s[1].tick (lp, lp, bp, hp);
    return lp;
}

// the shape a sweep generator gives (sine at 0, square at 1): -1 .. 1
inline float sweepShape (float s, float shape)
{
    const float k = 40.0f * shape * shape;
    return s * (1.0f + k) / (1.0f + k * absf (s));
}

// the 1982's converter: 12 bits whose gain ranges over three 6 dB steps (the quieter, the finer), clipping at full scale
inline float conv12 (float x)
{
    x = clampf (x, -1.0f, 1.0f);
    const float a = absf (x);
    const float step = a < 0.125f ? (1.0f / 16384.0f) : a < 0.25f ? (1.0f / 8192.0f) : a < 0.5f ? (1.0f / 4096.0f) : (1.0f / 2048.0f);
    return floorf_ (x / step + 0.5f) * step;
}

} // namespace sk
