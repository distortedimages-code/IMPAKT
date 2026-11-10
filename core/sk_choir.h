// IMPAKT — CHOIR: a sung voice (machine 42), a female alto by default. Own implementation (a source-filter singer
// after Klatt's synthesiser and the studies of singing it led to — Sundberg's formant tuning among them —, a
// gliding monophonic voice in the spirit of the ondes Martenot, its own big reverb).
//
//  SOURCE    the glottis: each cycle a pulse of air (the derivative of a KLGLOTT88-like flow: open for OQ of the
//            cycle, closing abruptly — band-limited where it closes —), its open quotient and its slope (a soft,
//            breathy voice: open longer, darker; an effort: shorter, brighter) from EFRT, BRTH, the velocity and the
//            ribbon's height; each cycle a little longer or shorter (jitter) and louder or softer (shimmer) than the
//            last (a little: a clean, trained voice), the pitch drifting slowly (DRFT); the breath: noise, only while
//            the glottis is open, in a band, coming in just before the voice. Vibrato (VIBR, VRAT) comes in after the note has started, each singer its own.
//  TRACT     five formant resonators in cascade (their relative levels come out as a voice's do), on the vowel
//            VOWL walks through: MM (the mouth closed) OO OH AH EH EE, the formants of a female alto; FORM scales the
//            tract (darker, a contralto .. brighter, a soprano); the first formant follows the pitch when it rises
//            past it (as a singer tunes it).
//  NOTES     a note glides from the last (GLID, legato: a singer's portamento), starts a little under itself
//            (SCOP) and swells in (SWEL); it holds until the next trig (or for GATE, or while a pad, a key or the
//            ribbon holds it) and fades in DEC. A stop or a mute lets it go.
//  CHOIR     SING: one singer to four, each a little apart (pitch, tract, timing, vibrato); PART: the same line
//            sung a third or a fifth above, both, a third or an octave below (the scale's when the track is
//            quantised).
//  SPACE     the machine's own reverb on the track's way out (after its TONE), in stereo: an eight-line feedback
//            network (the VERB machine's), set for big, smooth, modulated spaces: MIX, SIZE, DECAY, DAMP, MOD,
//            SHIM (half the lines shifted an octave or a fifth: a shimmer), PRE.
//  RIBBON    on the page, under the knobs: a strip the voice follows as long as it is touched (pitch along it,
//            expression up it), over two octaves; SNAP holds it to the quantiser's notes (its scale, or the chord of
//            the HALO or STRUM track it follows), gliding from one to the next in GLID.
#pragma once

#include "sk_math.h"

namespace sk {

enum { CHOIR_SING = 4, CHOIR_PARTS = 3, CHOIR_VOX = CHOIR_SING * CHOIR_PARTS, CHOIR_FORM = 5, CHOIR_VOWELS = 6,
       CHOIR_PARTN = 6, CHOIR_SHIMS = 4, CHOIR_SIDES = 3, CHOIR_KIND = 1000 };

struct ChoirSinger
{
    float ph, dph0;                         // the glottal cycle (0..1), its last increment
    float oq, amp, per;                     // this cycle's open quotient, amplitude (shimmer), length (jitter)
    float tilt, tilt2;                      // the source's slope (two one-poles)
    float r1[CHOIR_FORM], r2[CHOIR_FORM];   // the formants' states
    float ca[CHOIR_FORM], cb[CHOIR_FORM], cc[CHOIR_FORM];   // their coefficients (this chunk)
    float hum;                              // MM: the closed mouth's low-pass
    float vibPh, vibRate;
    float drift, driftT; int driftLeft;     // cents
    float size;                             // its tract against the alto's
    float det;                              // cents
    float wait;                             // samples before it starts (an ensemble never starts as one)
    float nz1, nz2;                         // the breath's band-pass
    float g;                                // its share (singers fade in and out with SING)
};

struct ChoirState
{
    ChoirSinger s[CHOIR_VOX];
    float pitch;                 // semitones from the track's pitch (TUNE + NOTE 0), sounding
    float from, to, gx, gInc;    // a glide: from, to, its progress, its rate (per sample)
    float env;                   // the voice's level (0..1)
    int   stage;                 // 0 off, 1 swelling in, 2 holding, 3 fading
    int   held;                  // its gate is open
    float holdLeft;              // samples (-1: until the next trig)
    float scoop;                 // semitones under (the onset's), going
    float age;                   // seconds since the note began (the vibrato's onset)
    float vowel;                 // VOWL, smoothed
    float expr;                  // the ribbon's height (1 when it does not play)
    float exS;                   // its level, glided
    int   rib;                   // the ribbon plays this voice
    float ribT;                  // where the ribbon is (semitones)
    int   ribSnap;               // SNAP: held to the quantiser's notes
    float ribQ;                  // the note it holds to (semitones from the track's pitch; -999: none yet)
    float breath;                // the breath before the voice
    float hp;                    // the output's DC
    float lvl;
    float poff[CHOIR_PARTS];     // the parts' intervals (semitones), glided
    int   poffOk;
    float fShow[CHOIR_FORM];     // the formants of the first singer (Hz), for the screen
    int   fresh;
};

} // namespace sk
