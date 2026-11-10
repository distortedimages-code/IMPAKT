// IMPAKT — HOWL: an electric guitar held up to a screaming amplifier, with its own chords (machine 43). Own
// implementation.
//
// The topology follows Nathaniel Virgo's SuperCollider "Guitar feedback emulation" (sccode.org/1-U), as Forsitan
// Modulare's Ululo (GPL-3, github.com/gosub/forsitan-modulare) documents it: the amp's sound travels through the air
// (DIST, a short delay) and excites six strings; their sum goes through the amp, whose sound closes the loop. Above a
// certain gain the loop takes off on its own and howls; which harmonic wins is decided by the strings, the distance and
// the tone stack fighting each other. Written again here from those principles, with the chords built in:
//
//   CHORD   TUNE the root (E2 at 12 o'clock, on the scale when the track is quantised), DEC how long the strings ring
//           (∞ at its top: they sustain while the gate is open), CHRD the chord (5TH .. 5+♭6, KEY: the scale's own
//           seventh chord on the root, POLY: the notes played, a string each), VOIC how the hand lays it on six strings
//           (DRONE, OPEN, BARRE, SPREAD, CLUSTER, 1ST, 2ND, HIGH; thirds, sevenths and ninths never go under their low
//           limits: the bass stays clean), DROP (the whole chord an octave or two down, or the bass string alone), FDBK
//           (the loop's gain: past about half it sustains, further it howls), MRPH (a new chord: the strings glide to
//           their new notes, from at once to 30 s, the ringing carried over)
//   STRING  EXCT (the pick: none, the strings woken by the feedback alone, .. a hard strum), DAMP (dark .. bright), RESO
//           (the strings ringing each other, through the bridge), DIST (the guitar's distance from the amp: 2 .. 80 ms,
//           which harmonic howls), BLOM (how long the feedback takes to bloom after a chord), DRFT (each string out of
//           tune its own way, wandering), WAMY (the arm: the whole chord wavering)
//   AMP     DRIV (the preamp), FUZZ (from a tube's round clipping to a starved fuzz, asymmetric and sputtering), TONE,
//           BASS, MID (a passive tone stack of the classic kind, R/C values of a big bass amp: H(s) after Yeh & Smith,
//           DAFx 2006, digitised by the bilinear transform), SAG (the power stage: compression, the supply giving way),
//           LEVL
//   LOOP    (v82) the feedback's own controls: TIME (the air's length, a micro-delay gliding when it moves), COLR (where
//           the loop sings), FOCS, PKUP, NOIS, MOVE, LIMT
//   CAB     (v83) the amp further: TGHT (a high-pass before the preamp: tight, defined bass under the drive), BITE (the
//           upper mids lifted into it: crunch), PRES (the power stage's presence, a shelf over 3 kHz, cut or lifted),
//           DPTH (its depth: the cabinet's low resonance lifted), CAB (the speaker, from dark to crisp: its roll-off and
//           its own colour), SIZE (1x12 .. 4x12: where its low resonance sits), MIC (only what is recorded: close, the
//           cone's proximity and its top; back, a reflection of the room)
//   BOW     (v83) a magnetic bow held over the strings (the guitarist's drone tool): a pickup senses a string, a coil
//           drives it back, it rings on at a steady level with no feedback at all. BOW (how hard it drives), SWEL (its
//           swell: 20 ms .. 6 s), STRG (which string: ALL, 1 the highest .. 6 the lowest), MODE (FUND, or its harmonic
//           mode: OCT, 12TH, the fundamental pushed back), POS (over the neck: round; at the bridge: thin), WOBL (the
//           hand's wobble), HAND (the other strings muted by the hand)
//
// Six waveguides (in the voice's lbuf), one air line (mcomb), one amp: the strings, the loop and the amp run at the
// track's rate; the drift, the arm, the glides and the tone stack's coefficients once per chunk.
#pragma once

#include "sk_math.h"

namespace sk {

enum { HOWL_STR = 6, HOWL_LINE = 4096, HOWL_TYPES = 15, HOWL_VOICS = 8, HOWL_DROPS = 5, HOWL_SIDES = 6,
       HOWL_KEY_T = 13, HOWL_POLY_T = 14 };
// (the machine's settings in the side places: STRING S1 .. S7, AMP S8 .. S14, LOOP S15 .. S21, v83: CAB S22 .. S28,
// BOW S29 .. S35; v82: HUM where v81 had DIST, the distance gone to LOOP as TIME)
enum { HW_EXCT = 0, HW_DAMP, HW_RESO, HW_HUM, HW_BLOM, HW_DRFT, HW_WAMY,
       HW_DRIV, HW_FUZZ, HW_TONE, HW_BASS, HW_MID, HW_SAG, HW_LEVL,
       HW_TIME, HW_COLR, HW_FOCS, HW_PKUP, HW_NOIS, HW_MOVE, HW_LIMT,
       HW_TGHT, HW_BITE, HW_PRES, HW_DPTH, HW_CAB, HW_SIZE, HW_MIC,
       HW_BOW, HW_SWEL, HW_STRG, HW_BMOD, HW_BPOS, HW_WOBL, HW_HAND, HW_NSIDE };
enum { HOWL_STRGS = 7, HOWL_BMODS = 3 };           // (STRG: ALL, 1 .. 6; MODE: FUND, OCT, 12TH)
// (v81's DIST, 2 .. 80 ms, onto v82's TIME, 0.3 .. 80 ms: the same distance)
inline float howlTimeFromDist (float d) { return (float) __builtin_fmin (1.0, __builtin_fmax (0.0, (__builtin_log2 (0.002 / 0.0003) + (double) d * __builtin_log2 (40.0)) / __builtin_log2 (80.0 / 0.3))); }   // (exact: the page's)

struct HowlString
{
    int   on;                // 0 silent, 1 sounding
    float pitch;             // the note it sounds (MIDI, gliding)
    float from, to, gl;      // its glide: from, to, how far (0 .. 1)
    int   w;                 // write position in its line
    float lp;                // the loop's damping low-pass
    float dk;                // the hand's damping (per sample, 1 = none)
    float lvl;               // its level (peak follower)
    float yz;                // its last output
    int   pend;              // samples to its pick (-1: none)
    float pv;                // the pick's strength
    int   ex, exLen;         // the pick going in: samples done, its length (a period)
    float exPos, exLp, exHard, exNz;
    float drN, drFrom, drTo, drPh, drRate;   // DRFT: its wander (cents now, from, to), where it is, how fast
    float off;               // DRFT: its own tuning (-1 .. 1)
    float key;               // POLY: the note a key holds on it (-999: none)
    unsigned age;            // POLY: when it took its note
    float bowE;              // v83, BOW: the bow on it (0 .. 1: its swell), its sensing (the place's low-pass), the
    float bowLp, bh1, bh2, bf1, bf2;   // harmonic's band-pass, the fundamental's (its states)
};

// a biquad (v83, CAB): transposed direct form II, its magnitude at f (cycles a sample)
struct HowlBq
{
    float b0, b1, b2, a1, a2, z1, z2;
    void flat() { b0 = 1.0f; b1 = b2 = a1 = a2 = 0.0f; }
    inline float tick (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

struct HowlState
{
    HowlString s[HOWL_STR];
    int   fresh;
    int   held;              // the gate open (a trig's GATE, a pad, a key; until the next trig)
    float holdLeft;          // samples (-1: until the next trig)
    int   seq;               // struck by the sequencer: a stop or a mute ends it
    float gate, gateT;       // the guitar before the amp (1) or turned away (0), glided
    float bloom;             // the feedback's bloom since the last chord (0 .. 1)
    int   aw;                // the air's write position (in mcomb)
    float fbS;               // the loop's gain in the last chunk
    float z1, f1, z2, f2;    // the preamp's two stages: last input, its antiderivative
    float dcX, dcY, dc2X, dc2Y;
    float env;               // the fuzz's envelope (its sputter)
    float ts[3], tb[4], ta[3], tsK[3];      // the tone stack: its state, coefficients, the knobs they were made for
    float pwEnv, sag;        // the power stage: its envelope, the supply
    float cb1, cb2, cb3, cb4, hpX, hpY;     // the cabinet: two low-pass poles (states), the high-pass
    float ampOut;            // the amp's last sample (what the air carries)
    float wmN, wmFrom, wmTo, wmPh;          // WAMY: the arm (cents now, from, to), where it is
    float mix[HOWL_STR * HOWL_STR], mixK;  // RESO: the bridge, a lossless mix of the strings (a rotation), the RESO it was made for
    // LOOP (v82): the air's length now (samples: TIME glides, the micro-delay's speed), the loop's colour (a band-pass: its
    // two states), the limiter (its gain, its envelope), MOVE's wander (the distance, the angle: now, from, to, where),
    // the hum's phase, the next crackle
    float tmS, bp1, bp2, bp3, bp4, lim, limEnv, airEnv, mb1, mb2;
    float gSl;               // the amp's slope now against its small-signal gain (how hard what it plays drives it)
    float mvN[2], mvFrom[2], mvTo[2], mvPh;
    float humPh;
    int   crk;
    // CAB (v83): TGHT and BITE before the preamp, PRES, DPTH and the speaker's colour after the power stage, MIC's two
    // (close: the cone's proximity, its top) on what is recorded, its room's low-pass; the knobs they were made for
    HowlBq tg, bt, pr, dp, ck, mc, mh;
    float cabK[7], mrl, wbPh, wbN, wbTo;
    float outLv, loopLv;     // levels for the screen (the output, what the strings hold)
    float root;              // the chord: its root (MIDI), type, voicing, strings
    int   type, voic, n;
    float notes[HOWL_STR];
    unsigned ageN;
    Rng   nr;                // the noise floor that lets the loop start from silence
};

} // namespace sk
