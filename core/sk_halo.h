// IMPAKT — HALO: chords for pads and ambiences (machine 34). Own implementation, after the ideas of chord
// oscillator modules (a root, a chord quality, a voicing, a wavetable voice) and of the hazy, detuned pads of the
// Warp years.
//
//  HARMONY  the trig's NOTE (plus TUNE) is a degree of a scale: the quantiser's (MODE KEY) or a mode of HALO's own
//           on TUNE's key. CHRD stacks the chord in that scale (12 shapes, from an open fifth to clusters), so it is
//           always in key; PROG walks a progression (27, each trig a chord: in the scale — the ambient ones, then the
//           classical ones (a canon, the pop axis, cadences, a folia...) —, chromatic slips and mediants for the
//           outside ones, a pedal (the tonic held under the chords), the tintinnabuli (the tonic's chord, a melody
//           walking the scale over it and the chord's note just under the melody), or a random walk). HARM: FREE (the
//           chord as stacked), PURE (no extension a semitone or a minor ninth above another note, no diminished or
//           augmented fifth — the note moves a step —, the low register opened out, and a note of the last chord
//           that clashes with the new one let go fast), LEAD (PURE, and the upper voices take the octave nearest the
//           last chord's: the voices move by steps). The chord holds until the next trig (or for GATE), and lets go
//           when the transport stops or the track is muted.
//  VOICING  SPRD, the harmonic spread: from close (every note within the octave) the bass falls an octave and the
//           others open out over four, each crossing an octave in a fade (two copies an octave apart), never a
//           glide; INV turns the chord over (inversions, continuous). Both move live under a held chord.
//  CHANGES  a new chord keeps the notes it shares with the last; GLID makes the others glide to the nearest notes
//           of the next chord (voice leading), at 0 they fade out (DEC) while the new ones swell in (SWEL).
//  VOICE    two detuned oscillators per note, WAVE morphing SINE > SOFT > SAW > PULSE > GLASS (FM 1:2) > METAL
//           (FM 1:3.53); DRFT detunes them, makes each note drift on its own and the whole chord wow like a tape;
//           BLOOM adds the octave, twelfth and double octave of each note, breathing; AIR a breath of noise tuned
//           on each note. WIDE (in the track's output): a stereo ensemble that leaves the mono sum alone.
//  TONE     UNIS two more oscillators a note, spread wider; SUB a sine an octave under the bass; CUT a low-pass on
//           each note (following its pitch) and OPEN how far under CUT a note starts, opening as it swells in and
//           closing as it fades; VIB a vibrato (each note its own, coming in with the swell); WARM a soft, uneven
//           saturation of the chord.
//  AGE      (v60, HALO and STRUM) the notes are played by eight voice cards (STRUM: each string its own), each one a
//           little off in its own way: DTUN how far out of tune (a few cards close, one or two well out: some notes
//           sour), VARI how unequal they are (level, swell and release, the wave's place); SHEN a sheen over the top
//           (the band over 3 kHz made to sing an octave up, a shelf over 8 kHz); DROP the dropouts of a worn tape (the
//           level and the highs dipping for a moment, now and then).
#pragma once

#include "sk_math.h"

namespace sk {

// (18 slots: the chord, the last one letting go, and what is older fading out fast)
enum { HALO_SLOTS = 18, HALO_MAXN = 6, HALO_SHAPES = 12, HALO_PROGS = 28, HALO_MODES = 12, HALO_SIDES = 4, HALO_INFO = 12 + 3 * HALO_SLOTS,
       HALO_CARDS = 8 };
// HARM: FREE, PURE, LEAD; HALO_KEY: what built a chord (a change re-voices it)
enum { HARM_FREE = 0, HARM_PURE, HARM_LEAD, NUM_HARM, HALO_KEY = 7 };

struct HaloSlot
{
    int   stage;          // 0 off, 1 swelling in, 2 held, 3 letting go
    int   fast;           // letting go of an older chord: in 60 ms (2: a note that clashes with the new chord, 150 ms)
    float x;              // the swell's progress (0..1)
    float g;              // the slot's gain
    float pitch, target;  // semitones from C3 (close voicing), and where it glides
    int   tone;           // its place in the chord (0 = the root): what SPRD and INV do with it
    float oct;            // octave shift (SPRD + INV), smoothed
    int   oBase;          // the octave of the lower copy (crossing one hands the copies' phases over)
    float ph[4];          // oscillators A / B, low copy and the copy an octave up
    float mph[4];         // their FM modulators (GLASS, METAL)
    float bph[6];         // BLOOM: three partials, low and high copy
    float bAmp[3], bTgt[3];
    float drift, driftT;  // cents
    int   driftLeft;      // samples to the next drift target
    float pwPh;           // PULSE: its width's slow LFO
    float a1, a2;         // AIR: the band-pass state
    // TONE: the unison's oscillators (two a copy), the sub's phase (v71: one, the other unused), the low-pass state, the
    // vibrato; the sub's share (v71: 1 on the lowest note sounding, glided: subA its own oscillator, octaves down; v72:
    // subU, on the note itself, from the voice's phase) and whether it was the lowest when it sounded
    float uph[4], umph[4], sph[2], fz1, fz2, vph, vrate, subA, subU;
    int   subLow;
    int   card;           // AGE: the voice card that plays it
};

struct HaloState
{
    HaloSlot s[HALO_SLOTS];
    int   n;              // notes in the chord
    float chord[HALO_MAXN];
    int   key[HALO_KEY];  // what built the chord (root, shape, scale, progression degree and shift, HARM, kind and melody)
    int   progDeg, progSemi;
    int   progKind, tint;   // the progression's kind (0 degrees, 1 a pedal, 2 tintinnabuli) and the melody's degree (2)
    int   root;           // the chord's root (semitones from C3): what a chord track playing along with this one takes
    int   held;           // the chord is held (it lets go at the end of the hold, a stop or a mute)
    float holdLeft;       // samples (-1: until the next trig)
    float wowPh, wrA, wrB, wrPos;   // the shared wobble (a sine and a smooth random)
    float norm;           // level against the number of notes, smoothed
    float warmG;          // WARM: what its saturation takes back, given back
    int   fresh;          // set by startVoice: the next trigger starts from silence
    float at1, at2, al1, al2;   // AIR: the chord's breath, its top (a high-pass, softened by a low-pass)
    Rng   arng;           // AGE: which card a new note takes (draws of its own: the others' stay as they were)
};

// a voice card's (STRUM: a string's) own errors, fixed for the track: its tuning (-1..1: a card or two well out, the
// others closer: the magnitudes spread over the cards, their order and signs the track's) and k = 1.. its others
// (-1..1, even)
inline u32   cardHash (u32 x) { x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16; return x; }
inline float cardUnit (int ti, int c, int k)
{
    const u32 x = cardHash ((u32) ti * 0x9E3779B9u + (u32) c * 0x85EBCA6Bu + (u32) k * 0xC2B2AE35u + 0x5A17u);
    return (float) (x >> 8) * (1.0f / 8388608.0f) - 1.0f;
}
inline float cardErr (int ti, int c)
{
    static const float MAG[8] = { 0.04f, 0.1f, 0.17f, 0.26f, 0.37f, 0.5f, 0.7f, 1.0f };
    const u32 r = cardHash ((u32) ti * 0x632BE5ABu + 0x0D7u);
    const float m = MAG[((u32) c + r) & 7u];
    return (cardHash ((u32) ti * 0x27D4EB2Fu + (u32) c * 0x165667B1u + 0x3Cu) & 1u) ? m : -m;
}

// the AGE side's last stages (HALO, STRUM): SHEN (the band over 3 kHz, rectified: its octave over it, high-passed;
// and a shelf over 8 kHz) and DROP (a worn tape's dropouts)
struct AgeFx
{
    float b1, b2;         // SHEN: the band the sheen is made of (SVF)
    float env;            // its level
    float h1, h2;         // the sheen's high-pass
    float s1, s2;         // the shelf's high-pass
    float dLeft, dDepth, d, lp;     // DROP: samples left in the dropout, its depth, its envelope, the highs' low-pass
    Rng   rng;
};

// the track's stereo ensemble (WIDE): two modulated taps on the track's output, their difference to the sides
struct HaloWide
{
    float buf[2048];
    int   w;
    float ph1, ph2, amt;
    int   on;
};

} // namespace sk
