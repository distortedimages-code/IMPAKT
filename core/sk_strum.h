// IMPAKT — STRUM: strummed chords on plucked strings (machine 41). Own implementation.
//
// HALO's harmony (its shapes, progressions, modes and HARM) spread over up to six strings the way a hand finds them
// on a neck: the root in the bass, then the chord's notes going up with their doublings, no two strings closer than
// SPRD allows, every note of the chord in it; LEAD keeps the top string near the last chord's. The strings are struck
// one after another (STRM: from all together to a slow roll; DIR: down, up, down and up in turn, at random); a retrig
// or a ratchet strums the same chord again the other way (an upstroke skips the bass strings), it never moves the
// progression on. Each string is a digital waveguide (Karplus-Strong as Jaffe and Smith extended it): a delay line
// closed on a damping low-pass, two dispersion allpasses and a fractional allpass, tuned on its fundamental with
// the filters' delays taken out; plucked by a triangle at the pick's place (its harmonics fall like a real pluck's)
// and a little noise, a few cents sharp at first (the string's tension); four body resonances after them.
//
//   CHORD   TUNE (a degree, as on HALO), DEC (how long the strings ring; HOLD at the top: they sustain while the
//           gate is open), CHRD, PROG, SPRD, STRM, DIR
//   PLAY    MODE, INV, HARM, HUMN (timing, velocity, a few cents), ACCN (the strum's dynamics: the first strings
//           louder on the left, the last ones on the right), RING (on a change of chord the hand damps the strings
//           it leaves; to the right they ring on, like a harp's), WIDE
//   STRING  MATL (NYLON, STEEL, ELEC, HARP, KOTO, SYNTH), PICK (soft, at the middle .. hard, at the bridge), DAMP (dark
//           .. bright), BODY, MUTE (the palm), DBL (doubled courses: an octave over the low strings, a unison over
//           the high ones), STIF (stiffness: from a string to a bell)
//   AGE     (v60) AIR (the pick's breath on each string struck, and the strings' air: noise shaped by their own
//           vibration, over 2.5 kHz), SHEN, DTUN (each string out of tune its own way: a guitar not quite in tune),
//           VARI (each string its own level, brightness, ring), DROP — as on HALO
#pragma once

#include "sk_math.h"

namespace sk {

enum { STRUM_VOICES = 16, STRUM_LINE = 2048, STRUM_MAXS = 6, STRUM_MATS = 6, STRUM_DIRS = 4, STRUM_SIDES = 4 };

struct StrumVoice
{
    int   on;                // 0 silent, 1 waiting for its first pluck, 2 sounding
    int   pend;              // samples to a pluck (-1: none)
    int   w;                 // write position in its line
    float pitch;             // semitones from C3
    float vel;               // the pluck's
    float apX, apY;          // the fractional allpass: last input, last output
    float lp;                // the loop low-pass
    float d1, d2;            // the dispersion allpasses
    float dk;                // the hand's damping (per sample, 1 = none)
    int   ex, exLen;         // samples of the pluck still going in, its length (a period)
    float exLp, exPos, exAmp, exNz, exHard;   // the pluck: its low-pass, the pick's place, level, noise, hardness
    float bend;              // the pluck's overshoot (cents), dying away
    float re;                // a re-pluck: what of the vibration is left when the pick strikes again
    float lvl;               // its level (peak follower)
    float gain;              // its share in the mix (a doubled course's second string: DBL)
    int   unit;              // AGE: the string it is (0..5, a course's second 6..11): its errors
    float air;               // AIR: the pick's breath, dying away
};

struct StrumState
{
    StrumVoice s[STRUM_VOICES];
    int   n;                         // strings in the last chord
    float str[STRUM_MAXS];           // their pitches
    int   cur[2 * STRUM_MAXS];       // the voice each string (and its double) plays (-1: none)
    int   dir;                       // the last strum's direction (1 down, -1 up)
    float body[8];                   // the body: four resonances, two states each
    int   held;                      // DEC on HOLD: the strings sustain while the gate is open
    float holdLeft;                  // samples (-1: until the next trig)
    int   seq;                       // struck by the sequencer: a stop or a mute damps it
    int   fresh;
    float top;                       // the last chord's top string (LEAD)
    float aH1, aH2, aLp, aL2;        // AIR: its high-pass, its low-pass
    Rng   arng;                      // AIR: its noise (draws of its own)
};

} // namespace sk
