// IMPAKT — shared drum-machine engine (used by the web build and the JUCE plugin).
// 8 tracks x 64 steps, 39 machines, every track parameter can be p-locked per step.
// Per track: machine -> SRR -> bits -> filter -> drive -> [BOX impulse response] -> pan/sends.
// Global: ping-pong delay, reverb (room / plate / spring / spectral / hall / space), master section.
// RAM: a live sampler per track (REC trigs record a take in the background, PLAY trigs play it).
// No libc, no allocation: the whole engine is one plain struct, initialised with init().
#pragma once
#if defined (__clang__)
 #pragma clang diagnostic ignored "-Wsign-conversion"   // fixed-size arrays sized by int template parameters
#elif defined (__GNUC__)
 #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "sk_math.h"
#include "sk_box.h"
#include "sk_spec.h"
#include "sk_org.h"
#include "sk_master.h"
#include "sk_rev.h"
#include "sk_vintage.h"
#include "sk_mix.h"
#include "sk_macro.h"
#include "sk_grain.h"
#include "sk_halo.h"
#include "sk_strum.h"
#include "sk_choir.h"
#include "sk_alloy.h"
#include "sk_cube.h"
#include "sk_west.h"
#include "sk_howl.h"
#include "sk_mantra.h"
#include "sk_radio.h"
#include "sk_chimera.h"
#include "sk_break.h"
#include "sk_fx.h"
#include "sk_send.h"
#include "sk_ott.h"
#include "sk_glitch.h"
#include "sk_lfo_tables.h"
#include "sk_fft.h"

namespace sk {

enum { NUM_TRACKS = 8, NUM_STEPS = 64, NUM_MACHINES = 47, CHUNK = 32, LATENCY = BOX_NA };
static_assert ((int) CHUNK == (int) CHIM_CHUNK, "CHIMERA's sides run on the engine's chunks");
// patterns: 8 banks of 16 (A01 .. H16); a slot keeps its sounds, steps, lengths, speeds and p-locks (sparse); a chain
enum { NUM_PATS = 128, PAT_LOCKS = 8192, CHAIN_MAX = 64, PAT_INFO = 8 };
enum { NUM_JUMPS = 25, NUM_JRANGE = 24 };     // JUMP: -12 .. +12; its range: 1 .. 24 semitones
// DEC at its top: HOLD (the amplitude holds while the gate is open: a pad or a key held, the trig's GATE, or until the
// next trig); the gate shut, the note ends in HOLD_REL seconds
constexpr float DEC_HOLD = 0.99f, HOLD_REL = 0.25f, HOLD_OPEN = 3600.0f;   // (HOLD_OPEN: a gate held open, seconds)
static_assert ((int) CHUNK == (int) FX_CHUNK, "the FX side runs on the engine's chunks");
// P_MACH is stored on a fixed grid of slots ((m + 0.5) / MACH_SLOTS); 32 slots until v39, 64 since (projects v9). v61
// (projects v21) took five machines out (MODAL, STRING and the three wavetable ones) and numbered the others again:
// a project saved before has its machines moved onto the new numbers as it loads (oldMachine21); v62 (projects v22)
// put MATTER and LATEX together into ALLOY, the last (oldMachine22)
enum { MACH_SLOTS = 64 };
inline int   machineOf (float v)  { const int m = idxOf (v, MACH_SLOTS); return m < NUM_MACHINES ? m : NUM_MACHINES - 1; }
// how much of a machine's own drive / fold / grit stage is heard: faded in over the first 6 % of its knob
inline float wetOf (float amt)     { const float o = clampf (amt * (1.0f / 0.06f), 0.0f, 1.0f); return o * o * (3.0f - 2.0f * o); }
inline float machineVal (int m)   { return ((float) m + 0.5f) / (float) MACH_SLOTS; }
enum { NUM_ULOOP_SYNTH = 4, NUM_ULOOP_SRC = NUM_ULOOP_SYNTH + NUM_ORG, MAX_PART = 24, MAX_BURST = 32 };

enum Machine {
    M_BD_VA = 0, M_BD_FM, M_SD_VA, M_SD_FM, M_HH_MT, M_CP_NZ,
    M_FM_X, M_NOISE, M_IMPLS, M_SY_RAW, M_BYTE,
    M_ULOOP, M_BUG, M_CHAOS, M_VOX, M_CZPD, M_FRACT,
    M_ITERA, M_TRIO,
    M_SINUS, M_DATA, M_FMZ, M_HARD,         // pure sine (Pan Sonic), binary data (Ikeda), wild FM, industrial kick
    M_HEXBD, M_HEXSD, M_HEXTM,              // the hexagonal-pad analog kit (bass, snare, toms): tone + noise + click
    M_MACRO,                                // the 24 models of Plaits (sk_macro.h, Mutable Instruments, MIT)
    M_RAM,                                  // live sampler: REC trigs record the track's buffer, PLAY trigs play it
    M_HALO,                                 // chords for pads (sk_halo): TUNE + NOTE a degree, CHRD, PROG, SPRD, WAVE, BLOOM
    M_BREAK,                                // the break slicer (sk_break): SLOT, MODE (SYNC / VARI), SLCE, LEN, DUST, SIDE (PLAY / CHOP)
    // the FX machines (sk_fx): the track takes another one's output (SRC, A) through its effect, MIX (E), SIDE (F:
    // MAIN / MORE); on the MORE side, P_XF, P_XG, P_XA .. P_XE (ACT: what a trig does, P_XE)
    M_FXVERB, M_FXECHO, M_FXBEND, M_FXTAPE, M_FXCODEC,
    M_STRUM,                                // strummed chords on plucked strings (sk_strum): HALO's harmony, a string a note
    M_CHOIR,                                // a sung voice (sk_choir): glottal source, formants, glides, a ribbon, its own space
    M_ALLOY,                                // metallic sounds (sk_alloy): an impact, an object of modes, FM, sines, a comb, a DAC
    M_CHIMERA,                              // the sampler on the bank (sk_chimera, sk_bank): FAMILLE (VARI) and GREFFE
    M_FXFUZZ,                               // v68: an FX machine (sk_fuzz): three bands, a germanium fuzz circuit on each
    M_CUBE,                                 // v74: a dual wavetable voice in a cube of 512 waves (sk_cube)
    M_WEST,                                 // v76: a west coast voice: folder, low-pass gate, pulser, five steps, spring (sk_west)
    M_FXWEFT,                               // v78: an FX machine (sk_weft): Mutable Instruments' Warps, the meta-modulator (MIT)
    M_HOWL,                                 // v81: a guitar held up to a screaming amp, its own chords (sk_howl)
    M_FXSTENCIL,                            // v83: an FX machine (sk_stencil): a stereo delay of zones, our VERB in its repeats
    M_MANTRA,                               // v84: eight voices that breathe and drive each other (sk_mantra), the Lyra-8's homage
    M_RADIO                                 // v87: the radio (sk_radio): four receivers the host tunes, LIVE, LOOP, DIG, the air
};
// v61: the machine of a project saved before v21 (its number then) on v21's numbers: the five taken out went to the
// nearest (MODAL, STRING and WT·DRM to MATTER, 37 then, WT·GRL and WT·ZAP to LATEX, 38), their A..F to its defaults
inline int oldMachine21 (int m)
{
    static const signed char MAP[42] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, -1, 9, 10, 11, 12, -1, 13, 14, 15, 16, 17, 18, 19, 20,
        21, 22, 23, 24, 25, -1, 26, 27, -2, -2, 28, 29, 30, 31, 32, 33, 34, 35, 36 };
    if (m < 0 || m >= 42) return 0;
    return MAP[m] == -1 ? 37 : MAP[m] == -2 ? 38 : MAP[m];
}
inline bool removedMachine21 (int m) { return m == 9 || m == 14 || m == 28 || m == 31 || m == 32; }
// v62: a v21 machine (its number then) on today's: MATTER (37) and LATEX (38) both into ALLOY (37), their A..F and
// their sides to ALLOY's defaults (replacedMachine22: they mean something else on it)
inline int  oldMachine22 (int m)      { return m >= 37 ? (int) M_ALLOY : (m < 0 ? 0 : m); }
inline bool replacedMachine22 (int m) { return m == 37 || m == 38; }
// ALLOY's side pages as a fresh track has them (the page's MACH defaults, sd): how many, into out
int machineSideDefaults (int m, float* out);
// ... and its A .. F (d[2..7]): 6 for ALLOY, CHIMERA and WEST (a track moved onto it as a project loads), else 0
int machineAFDefaults (int m, float* out);
// ALLOY's mode tables (built once), its noise's levels at the engine's rate (into hiss; Engine::init calls it)
void alloyTables (float sr, float* hiss);
// the machines that play sound (not the FX machines)
inline bool soundMachine (int m) { return m >= 0 && m < NUM_MACHINES && (m < M_FXVERB || m > M_FXCODEC) && m != M_FXFUZZ && m != M_FXWEFT && m != M_FXSTENCIL; }
bool fxMachine (int m);
int  fxActs (int m);
// FUZZ's circuit curves (sk_fuzz.cpp; built once, about 20 ms: Engine::init)
void fuzzTables();

// Every track parameter is normalised 0..1 and p-lockable.
// Indices 0..32 are the original layout; new parameters are appended so saved projects stay valid.
enum Param {
    P_MACH = 0,
    P_TUNE, P_DEC, P_A, P_B, P_C, P_D, P_E, P_F,                     // SYN
    P_FTYP, P_FREQ, P_RES, P_FENV, P_FDEC, P_DRV, P_BITS, P_SRR,     // FLT
    P_VOL, P_PAN, P_DLY, P_REV, P_LSPD, P_LDEP, P_LDST, P_LWAV,      // AMP / LFO 1
    P_NOTE, P_VEL, P_PROB, P_MICR, P_RTRG, P_RCNT, P_RVEL, P_GATE,   // TRIG
    P_SPC, P_ATK, P_CHOK, P_DUCK,                                    // AMP (RND depth — was the spectral send —, attack, choke group, ducking)
    P_L2SP, P_L2DP, P_L2DS, P_L2WV,                                  // LFO 2
    P_IR, P_IMIX, P_IPRE, P_IDCY, P_IGAT, P_ITON, P_IFB, P_IREV,     // BOX
    P_PNCH,                                                          // AMP: punch (transient at the onset)
    // RAM, the REC side of the machine: source, take length, level, threshold, overdub, input monitor, and the
    // buffer its PLAY trigs read (its own or another track's). Its PLAY side uses TUNE DEC A..E (F: the switch)
    P_RSRC, P_RLEN, P_RLEV, P_RTHR, P_ROVD, P_RMON, P_RBUF,
    // RAM, its PRISM side: spectral resynthesis of the take (dry / resynthesis, oscillators, blur, glide, band centre and
    // width (pass on the right of 12 o'clock, notch on the left), speed of the take under it)
    P_PMIX, P_PVOI, P_PBLR, P_PGLD, P_PCTR, P_PWID, P_PSPD,
    // PRISM v2 (v39): oscillator wave (SINE > TRI > SAW > PULSE), DYN (loud partials richer), octave, spectral warp
    // around the band centre, tempo slices, spectral tilt, scale quantiser on / off
    P_PWAV, P_PDYN, P_POCT, P_PWRP, P_PSLC, P_PTLT, P_PQNT,
    // TONE (v42, projects v10): high-pass and low-pass (12 dB/oct), low shelf (gain, frequency), mid bell (gain,
    // frequency), high shelf (gain, frequency), at the end of the track, after its BOX
    P_HPF, P_LPF, P_LOW, P_LOWF, P_MID, P_MIDF, P_HIGH, P_HIGF,
    // two envelope followers (v42): source, input gain, band, trigger threshold (0 = follower; above, the level
    // crossing it fires an attack / decay envelope), attack, decay, depth (bipolar), destination (the LFO list)
    P_E1SR, P_E1GN, P_E1BD, P_E1TG, P_E1AT, P_E1DC, P_E1DP, P_E1DS,
    P_E2SR, P_E2GN, P_E2BD, P_E2TG, P_E2AT, P_E2DC, P_E2DP, P_E2DS,
    // HALO, its VOICE side (v47, projects v13; F is its switch, CHORD / VOICE): swell, glide (voice leading), mode,
    // inversion, drift, air, width
    P_HSWL, P_HGLD, P_HMOD, P_HINV, P_HDRF, P_HAIR, P_HWID,
    // BREAK, its CHOP side (v50, projects v14; F is its switch, PLAY / CHOP): the chances of another slice, of a reversed
    // one, of a stutter, of a pitch jump, of a silent slice (v51, projects v15: was the ghost notes' weight); the seed of
    // the draws (0 = free); the chance of the same 16th in another slot (v51: was SYNC / VARI, now B)
    P_BRND, P_BREV, P_BSTU, P_BPJM, P_BSKP, P_BSED, P_BSWP,
    // the FX machines, their MORE side (v52, projects v16; F is its switch, MAIN / MORE): five settings whose meaning
    // is the machine's (see sk_fx.h), the last one ACT (what a trig does)
    P_XA, P_XB, P_XC, P_XD, P_XE,
    P_XF, P_XG,                             // v17: two more on the MORE side (VERB: BASS, SHFT; ECHO: PING, DUCK; TAPE: WIDE; CODEC: JOIN)
    // TRIG, its MORE page (v58): the retrig's curve (centre: even; right: the hits close in, faster and faster; left:
    // they spread out), the transposition jump (-12 .. +12, scale degrees when the track is quantised: each trig
    // that has one adds it, the track's notes climb or fall) and its range (1 .. 24 semitones, then back to the start)
    P_RCRV, P_JUMP, P_JRNG,
    // v59 (projects v19): the harmony of HALO and STRUM (FREE: as built; PURE: no clashing extension, no diminished or
    // augmented fifth, an open low register, a note of the last chord that clashes with the new one let go fast;
    // LEAD: PURE, and the upper voices led to the nearest notes of the next chord)
    P_HARM,
    // HALO, its TONE side (with AIR): unison, sub-octave, the voices' low-pass (CUT) and how far each one opens with
    // its swell (OPEN), vibrato, warmth
    P_HUNI, P_HSUB, P_HCUT, P_HOPN, P_HVIB, P_HWRM,
    // STRUM, its PLAY side (with MODE, INV, HARM and WIDE): humanise, accent, ring (the last chord's strings ring on);
    // its STRING side: material, pick, damping, body, palm mute, doubled courses, stiffness
    P_SHUM, P_SACC, P_SRNG, P_SMAT, P_SPCK, P_SDMP, P_SBDY, P_SMUT, P_SDBL, P_SSTF,
    // CHOIR, its VOICE side: swell, formant size, effort, vibrato rate, parts, scoop, drift; its SPACE side (the
    // machine's own reverb): mix, size, decay, damping, modulation, shimmer, pre-delay
    P_CSWL, P_CFRM, P_CEFF, P_CVRT, P_CPRT, P_CSCP, P_CDRF,
    P_CSPC, P_CSIZ, P_CDCY, P_CDMP, P_CMOD, P_CSHM, P_CPRE,
    // v60 (projects v20): HALO and STRUM, their AGE side (a fourth): voice cards out of tune (DTUN), unequal (VARI),
    // a sheen over the top (SHEN), tape dropouts (DROP); STRUM's AIR (the pick's breath, the strings' air)
    P_GDTN, P_GVAR, P_GSHN, P_GDRP, P_SAIR,
    // v61 (projects v21): a machine's side pages, settings whose meaning is the machine's (21; 35 since v63, projects
    // v23); ALLOY's (sk_alloy.h, the names below): IMPACT S1..S7, NOISE S8..S14, FM S15..S21, COLOR S22..S28, SPACE
    // S29..S35
    P_S1, P_S2, P_S3, P_S4, P_S5, P_S6, P_S7, P_S8, P_S9, P_S10, P_S11, P_S12, P_S13, P_S14,
    P_S15, P_S16, P_S17, P_S18, P_S19, P_S20, P_S21,
    P_S22, P_S23, P_S24, P_S25, P_S26, P_S27, P_S28, P_S29, P_S30, P_S31, P_S32, P_S33, P_S34, P_S35,
    // v64 (projects v24): the trig's pan drawn at each hit (RPAN, the TRIG row's MORE page: centre none, to the right
    // a draw on either side, to the left left and right in turn), the track's spread (SPRD, TONE's MORE page) and the
    // frequency under which it stays centred (MONO)
    P_RPAN, P_SPRD, P_SMON,
    // v65 (projects v25): the trig's decay drawn at each hit (RDEC, the TRIG row's MORE page: centre none, to the right a
    // draw at each hit, to the left LOOP, a draw per step of the pattern that comes back with it) and the seed of LOOP's
    // draws (RDSD: a new one each time RDEC leaves its centre to the left); per LFO, SLW (the time the SLEW wave takes to
    // reach each new draw, a share of its cycle), LEN (how many draws the LOOP wave plays before it comes back to the
    // first) and the seed of those draws (a new one when its depth leaves 0, or the LOOP wave is chosen)
    P_RDEC, P_RDSD, P_L1SL, P_L1LN, P_L1SD, P_L2SL, P_L2LN, P_L2SD,
    // v68 (projects v26): the sends to the CHORUS and the PHASER (the FX page's SEND, with DLY and REV)
    P_CHO, P_PHS,
    // v78 (projects v27): 35 more side places (S36 .. S70, after the others: a project saved before keeps its numbers;
    // sideParam) for WEST's whole panel, and the curve of the amplitude envelope (ECRV, the AMP page: LOG .. LIN .. EXP,
    // after Mutable Instruments' Streams; EXP, at its top, as it always was)
    P_S36, P_S37, P_S38, P_S39, P_S40, P_S41, P_S42, P_S43, P_S44, P_S45, P_S46, P_S47, P_S48, P_S49,
    P_S50, P_S51, P_S52, P_S53, P_S54, P_S55, P_S56, P_S57, P_S58, P_S59, P_S60, P_S61, P_S62, P_S63,
    P_S64, P_S65, P_S66, P_S67, P_S68, P_S69, P_S70,
    P_ECRV,
    // v81: DRONE (AMP): the track plays on its own while the transport runs, no trig needed; its trigs change it (a new
    // note, a new chord, their locks) instead of striking it again
    P_DRON,
    // v88: the trig's pitch drawn at each hit (RTUN, the TRIG row's MORE page, beside RDEC: centre none, to the right a
    // draw at each hit, to the left LOOP, a draw per step of the pattern that comes back with it) and the seed of LOOP's
    // draws (RTSD: a new one each time RTUN leaves its centre to the left)
    P_RTUN, P_RTSD,
    NUM_PARAMS
};
// the side places: S1 .. S35 (v63), then S36 .. S70 (v78) after the params that came between
enum { NUM_SIDES_A = P_S35 - P_S1 + 1, NUM_SIDES = NUM_SIDES_A + (P_S70 - P_S36 + 1) };
inline int  sideParam (int q) { return q < NUM_SIDES_A ? P_S1 + q : P_S36 + (q - NUM_SIDES_A); }
inline int  sideIndex (int p) { return p >= P_S1 && p <= P_S35 ? p - P_S1 : (p >= P_S36 && p <= P_S70 ? NUM_SIDES_A + (p - P_S36) : -1); }
inline bool sideP (int p)     { return sideIndex (p) >= 0; }

// ALLOY's side pages (v63, projects v23)
enum AlloyParam {
    PA_MALT = P_S1, PA_CLIK, PA_PENV, PA_PRIS, PA_PTIM, PA_BNCE, PA_ORGN,        // IMPACT
    PA_NOIS, PA_HISS, PA_NCOL, PA_NLEN, PA_FDBK, PA_DRIV, PA_DAC,                // NOISE
    PA_OBJ, PA_FM, PA_RATI, PA_INDX, PA_IATK, PA_IDEC, PA_MENV,                  // FM
    PA_ADD, PA_BAND, PA_COMB, PA_CTUN, PA_CFB, PA_FLNG, PA_PHAS,                 // COLOR
    PA_VERB, PA_ROOM, PA_DCAY, PA_DAMP, PA_SHFT, PA_CUT, PA_RATE                 // SPACE
};
// CHIMERA's side places (v66): GREFFE (the body: its category, 0 = none, and sample; the graft point, the crossfade, the
// split, B's tuning around A's pitch, B's level), FAMILLE (SENS, MEME, CIBL, ACCD) and the seed of VARI's LOOP
enum ChimParam {
    PC_BCAT = P_S1, PC_BSMP, PC_POIN, PC_FOND, PC_SPLT, PC_BTUN, PC_BLVL,
    PC_SENS, PC_MEME, PC_CIBL, PC_ACCD, PC_SEED = P_S15
};
static_assert ((int) PA_RATE == (int) P_S35, "ALLOY's sides end on S35");
// v63: an ALLOY track's 21 sides of v22 (EXCITE, LAYERS, COLOR) on today's 35 (IMPACT, NOISE, FM, COLOR, SPACE): where
// each went (alloySide23To, from 0), what it holds there (BNCE bipolar now: its old range on the right half; PENV
// down to three octaves under now: the same pitch), the new
// ones at their defaults (NCOL the colour MALT gave the noise)
inline int alloySide23To (int q)
{
    static const signed char TO[21] = { 0, 7, 10, 1, 2, 4, 5, 14, 15, 16, 17, 19, 21, 22, 23, 24, 25, 26, 27, 34, 13 };
    return q >= 0 && q < 21 ? TO[q] : -1;
}
inline float alloySide23Val (int q, float v) { return q == 6 ? 0.5f + 0.5f * v : (q == 4 && v < 0.5f) ? 0.5f - (0.5f - v) * (2.0f / 3.0f) : v; }
inline void alloySide23 (const float* o, float* s)
{
    static const float D[NUM_SIDES] = { 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0.5f,
        0, 0, 0, 0, 0, 0, 0,  0, 0.3f, 0.4f, 0.4f, 0.5f / 13.0f, 0, 0 };
    for (int i = 0; i < NUM_SIDES; ++i) s[i] = D[i];
    for (int q = 0; q < 21; ++q) s[alloySide23To (q)] = alloySide23Val (q, o[q]);
    s[PA_NCOL - P_S1] = 0.2f + 0.75f * o[0];
}

enum Global {
    G_BPM = 0,  // raw BPM value (not normalised)
    G_SWING,    // 0..1 -> 50..75 %
    G_DTIM, G_DFB, G_DTON, G_DPNG,
    G_RSIZ, G_RDMP, G_MDRV, G_MVOL,                                  // reverb size / damping
    G_STIM, G_SBLR, G_SWRP, G_SREF, G_SATM, G_SFRZ, G_SREV, G_SLVL,   // unused since v4 (old spectral send, kept for the layout)
    G_EQLF, G_EQLB, G_EQLA, G_EQHF, G_EQHB, G_EQBW, G_EQHA, G_EQHS,   // master: Pultec EQ
    G_CINP, G_CATK, G_CREL, G_CRAT, G_CDST, G_CDET, G_CMIX, G_COUT,   // master: compressor
    G_DMOD, G_DDRV, G_DBIA, G_DTNE, G_DCHR, G_DMIX, G_DOUT, G_DPOS,   // master: drive
    G_RALG, G_RTIM, G_RPRE, G_RCH1, G_RCH2, G_RLVL,                   // reverb: algorithm, time, pre-delay, 2 algorithm controls, level
    G_DACM, G_DACA,                                                  // unused since v37 (the DAC moved into the RAM machine)
    G_UMSK, G_UKCK, G_CALM, G_CSHP, G_CSEL, G_CSPD, G_CFOC, G_CDLT,   // MIX: unmasking (depth, kick priority), calm (resonances)
    G_LOOP, G_LLEN, G_LPOS,                                          // sequence looper: on, length, position offset
    // DICE (Marbles, until v66): on, t rate / range / model / bias / jitter, deja vu amount / length / targets, x spread /
    // bias / steps / mode / range / scale / clock, y divider; then 7 routes (t1 t2 t3 x1 x2 x3 y) of 3 values each.
    // v67: gone; the places stay (at 0) so the projects keep their layout
    G_DON, G_DRAT, G_DTRN, G_DTMD, G_DTBI, G_DJIT, G_DDV, G_DLEN, G_DDVM,
    G_DSPR, G_DXBI, G_DSTP, G_DXMD, G_DXRN, G_DSCL, G_DXCK, G_DYDV,
    G_DR0,
    G_DR_END = G_DR0 + 3 * 7 - 1,
    G_DPW,                                                           // DICE: t pulse width (v6 .. v66)
    // GRAIN (Beads), master effect after the compressor: time, size, shape, pitch, density, feedback, its
    // reverb, randomness, media (DIGI VINT TAPE DIRT), transport clock on SEED, freeze (v6; a reverb then)
    G_GTIM, G_GSIZ, G_GSHP, G_GPIT, G_GDEN, G_GFDB, G_GVRB, G_GRND, G_GMED, G_GSYN, G_GFRZ,
    // v7: GRAIN on, dry / wet; its LFO (rate, wave, phase) and the 8 attenuverters (TIME SIZE SHAP PTCH DENS
    // FDBK VERB MIX); the ON keys of EQ, COMP, DRV; OTT: on, time, upward, downward, low / mid / high, out, mix
    G_GON, G_GMIX, G_GLRT, G_GLWV, G_GLPH, G_GA0, G_GA_END = G_GA0 + 7,
    G_EQON, G_COON, G_DRON,
    G_OON, G_OTIM, G_OUP, G_ODN, G_OLO, G_OMD, G_OHI, G_OOUT, G_OMIX,
    // the remaining ON keys: reverb, delay, DAC (unused since v37), MIX (unmasking + calm); on by default
    G_RVON, G_DLON, G_DCON, G_MXON,
    // v11: GLITCH (page GLCH, end of the master chain): on, RAND (the tracks marked r: a new random offset of
    // their synthesis settings on every hit, scaled by the knob, gone at zero), TIME (grid), RPT, BEND, BRK,
    // CRPT, LOSS, MIX, the corrupt and loss modes, the loss speed, HOLD, and the r tracks (a mask / 255)
    G_XON, G_XRND, G_XTIM, G_XRPT, G_XBND, G_XBRK, G_XCRP, G_XLOS, G_XMIX, G_XCMD, G_XLMD, G_XSPD, G_XHLD, G_XMSK,
    // v12: LOSS's resonance, its analog (tape) path and the tape model
    G_XRES, G_XANA, G_XAMD,
    // v59.1: what the quantiser holds to: its scale, or the chord a HALO or STRUM track plays now (0: the scale,
    // 1..8: T1..T8, on a grid of 9; the track's own harmony keeps the scale)
    G_QSRC,
    // v68 (projects v26): the CHORUS (a send: its mode 1..4, its input MONO / STEREO, its level, its ON key), the PHASER
    // (a send: generator 1's rate and shape, generator 2's, phasor A's depth and feedback, B's, what sweeps B (GEN 1 /
    // GEN 2), the sync (NORM / REV), the route (STEREO / MONO / SERIES), its level, its ON key), the DELAY's model (STD /
    // 1982), the 1982's bandwidth (x1 / x2), the clock's VCO (depth, rate, shape, the input's envelope on it: bipolar),
    // HOLD (the repeats loop as they are)
    G_CHMD, G_CHIN, G_CHLV, G_CHON,
    G_PRT1, G_PSH1, G_PRT2, G_PSH2, G_PDPA, G_PFBA, G_PDPB, G_PFBB, G_PBSW, G_PSYN, G_PRTE, G_PLVL, G_PHON,
    G_DMDL, G_DBW, G_DVDP, G_DVRT, G_DVSH, G_DVEN, G_DHLD,
    NUM_GLOBALS
};

enum { FT_LP = 0, FT_BP, FT_HP, FT_COMB, NUM_FTYPES };
enum { DICE_OUTS = 7, NUM_DICE_DEST = 44 };     // (DICE's routes and destinations, until v66: the projects' migrations read them)
enum { NUM_GLFO_RATES = 15 };    // GRAIN's LFO: cycle lengths from 1/16 to 16 bars
// the LFO waves: eight shapes free-running, then the same eight restarting with each trig (v65: SLEW, a random line, and
// LOOP, a sample and hold whose LEN draws come round again; 6 shapes and 12 waves before, projects v24)
enum { LW_SIN = 0, LW_TRI, LW_SAW, LW_SQR, LW_SH, LW_EXP, LW_SLEW, LW_LOOP, LFO_SHAPES, NUM_LFO_LENS = 10 };
enum { NUM_SPEEDS = 7, NUM_LFO_SPEEDS = 16, NUM_LFO_DEST = 38, NUM_LFO_WAVES = 2 * LFO_SHAPES,
       NUM_RETRIG = 12, NUM_RCOUNT = 16, NUM_RATIOS = 24, NUM_DLY_TIMES = 11, NUM_CHOKE = 5 };


enum { COMB_SIZE = 8192, COMB_MASK = COMB_SIZE - 1, LBUF_SIZE = 32768, LBUF_MASK = LBUF_SIZE - 1 };

// RAM machine. One mono 16-bit buffer per track (2^19 samples: 10.9 s at 48 kHz; longer takes are recorded at
// a half, a quarter or an eighth of the rate). Sources: IN (the host input), MAIN (the master output), T1..T8 (a
// track after its BOX, before its pan). Takes are counted in steps of the recording track.
enum { RAM_LEN = 1 << 19, RAM_SRCS = 10, RAM_TAKES = 12, RAM_MODES = 4, RAM_SLICES = 5, RAM_PEAKS = 128 };
enum { RAM_IN = 0, RAM_MAIN = 1 };
// PRISM: each take is analysed once, as it is recorded (or after it is loaded): a 2048-point FFT every 512 samples,
// the 32 strongest peaks of each frame kept (frequency, amplitude). Playing it is a bank of up to 16 oscillators per
// voice: sines (rotating phasors) or band-limited waves read from tables (one per octave, 4 shapes morphed).
enum { PR_WIN = 2048, PR_HOP = 512, PR_FRAMES = RAM_LEN / PR_HOP, PR_PK = 32, PR_K = 16, PR_BUDGET = 1 };
enum { PRT_N = 2048, PRT_LEV = 9, PRT_SHAPES = 4, PR_SLICES = 7, PR_OCTS = 5 };
enum { RAM_IDLE = 0, RAM_WAIT, RAM_ARMED, RAM_REC };
struct RamBuf
{
    int   len;               // samples that play
    float rate;              // their sample rate
    int   ver;               // +1 when a take ends or the host loads one (the hosts save it then)
    // the take being recorded
    int   state;             // RAM_IDLE, RAM_WAIT (the latency of the source), RAM_ARMED (under the threshold), RAM_REC
    int   src, total, wpos, wait, oldLen, div, dcnt;
    float gain, thr, ovd, acc;
    int   prNext;            // PRISM: frames analysed from the start of the buffer
    // display: peak of each 128th of the take, and a counter that moves when they change
    float pk[RAM_PEAKS];
    int   pkVer;
};

// one grain of an organic sample (FRACT machine)
struct Particle
{
    const float* src;
    float pos, rate, gain, start, end, fin, fout;
    int   on;
};

struct Voice
{
    int   active;
    int   machine;
    int   step;          // pattern step that triggered (-1 = audition)
    float lk[NUM_PARAMS]; // lock snapshot (<0 = not locked)
    float vel;
    float note;          // semitone offset from NOTE
    float holdSec;       // gate hold
    float choke;         // 1 = playing, fades to 0 when choked
    int   choking;
    u32   ns;            // samples since trigger
    float level;         // envelope level (for end-of-voice detection)
    int   tail;
    float atk;           // attack fade-in (0..1)

    // amp envelope (v78: ecrv, ECRV this chunk: its curve)
    float aenv; int astage; float aHoldLeft; float ecrv;
    // generic envelopes
    float penv, menv, nenv, cenv, senv, ienv, i3env;
    // oscillators / state
    float ph[8];
    float y1[8], y2[8];
    float fb1, fb2, lastOut;
    Rng   rng;
    Svf   s1, s2, s3;
    float lp1, lp2;
    // clap
    float burstAt[8]; int nBursts, burstIdx; float benv, tenv;
    // impulse train
    int impLeft; float nextImp, curInt, impAmp;
    // bytebeat
    u32 bt; float btc, btOut;
    // granular gate (noise)
    float grainLeft, gate, gateS;
    // noise hold
    float shCnt, shVal; u32 lfsr;
    // micro-loop / buffer-error / string
    float lstart, llen, lrate, lphase, lmax; int ldir, lsrcLen;
    const float* lsrc;   // looped source: lbuf (synth) or the organic bank
    int   ovar;          // last bank variant played on this track (never repeated twice in a row)
    // multi-particle player (FRACT)
    Particle part[MAX_PART];
    float pAt[MAX_BURST]; int pNum, pIdx, pId; float tailEnv, pgain;
    int   bw, bmode, bleft, bread, bflip; float bhold;
    float exLen, delLen;
    double cx, cy, cz; float ccnt, cprev, ccur;
    // machine comb / delay line (impulse, string)
    float mcomb[COMB_SIZE]; int mcw;
    float lbuf[LBUF_SIZE];

    // 2x oversampled machines (FM·Z, HARD): rendered at twice the rate, brought back with a halfband filter
    HalfbandDown<80> os2;
    // DATA: the bit pattern, the current cell and the time spent in it
    u32 dbits; int didx; float dcnt;
    // TRIO filter: CEM3320-style 4-pole ladder states; punch envelope and its high-frequency split
    float lad[4];
    float pe1, pe2, pLp;
    // analog warm drive (BD·VA, HARD): low split for the pre-emphasis, envelope follower for the bias, post low-pass
    float wLo, wEnv, wPost;
    // TRIO: the VCA comes after the filter, as on the original; the machine hands its envelope over here
    float vca[CHUNK]; float wPost2;
    // HEX voices: the noise filter (4-pole ladder), the noise high-pass, the softened triangle, click and bend envelopes
    float hx[12];
    HaloState hs;         // HALO: its chord, its twelve note slots (STRUM: its chord)
    StrumState ss;        // STRUM: its strings (their lines in lbuf)
    ChoirState cs;        // CHOIR: its singers
    AgeFx ag;             // HALO, STRUM: their AGE side's sheen and dropouts
    AlloyState al;        // ALLOY: its impact, its object's modes, its layers (its comb's line in lbuf)
    ChimState chm;        // CHIMERA: its two samples, where it reads them, the graft, its sides
    CubeState cb;         // CUBE: its phases, its sweep and pitch envelopes, where A and B are in the cube
    WestState ws;         // WEST: its oscillators, its folder, its envelope, the vactrol and its gate, the pulser
    HowlState hw;         // HOWL: its strings (their lines in lbuf), the air (mcomb), the amp
    MantraState mt;       // MANTRA (v84): its eight voices, the supply they share, its hyper LFO, its delay (in the track's FX memory)
    RadioState rd;        // RADIO (v87): where it reads (the receiver, its loop), its air
    BreakState bs;        // BREAK: the slice playing and the one it cut
    void* mac;            // the track's MACRO unit (a MACRO voice drives it)

    // RAM: the voice's track, the buffer it reads, play mode and direction, done; position and window (buffer
    // samples); the converter model on its output
    int   ti, rbuf, rmode, rdir, rdone;
    double rpos;
    float rbeg, rend;
    Dac   dac;
    // PRISM: the oscillator bank (phasors), their amplitudes (now, target) and phase increments (now, target), the
    // analysed frequency each one follows (Hz, to match the next frame), how many slots are in use, a fresh voice
    alignas (16) float pRe[PR_K], pIm[PR_K], pAmp[PR_K], pAmpT[PR_K], pInc[PR_K], pIncT[PR_K], pHz[PR_K], pCs[PR_K], pSn[PR_K];
    float pPh[PR_K];      // the oscillators' phase (cycles) while the bank reads the wave tables
    int   pAct, pFresh, pTab;
    float pPk;            // DYN: the voice's loudest partial, slowly released
    float pSlL; int pSlF; // SLIC: samples left in the slice, the frame it holds
    float pKey[10];       // what the last match was made from (frame, voices, band, pitch, warp, tilt, quantiser, version)

    // SP-1200 reader (µLOOP, FRACT): 26.04 kHz clock phase, held 12-bit word, output filter
    float spPh, spHold, spLp1, spLp2;
    int   quant;            // pitch quantiser on for this voice's track

    // post chain
    float drvPrev, drvLc;   // drive anti-aliasing (previous input and its ln cosh)
    float srrCnt, srrHold;
    Svf   flt;              // (kept for the layout; the track filter is the MS-20 model below)
    Ms20Filter ms;
    float comb[COMB_SIZE]; int cw;
    float fenv;
    float dcx, dcy;
    float acI1, acI2;       // AC coupling of the machine's source (Engine::acCouple)
    // the voice strip's settings in the last chunk (the next one glides from them): filter position and resonance,
    // drive, level; smOk = 0 on a new note (its first chunk starts where it is)
    int   smOk;
    float rnd[12]; int rndOn;   // RAND: this hit's offsets (-1..1) of the synthesis settings (Engine::RND_PS)
    float rdec;                 // RDEC: this hit's offset of DEC (0: none)
    float rtun;                 // RTUN (v88): this hit's offset of TUNE (0: none)
    float mWet;             // the machine's own drive / fold / grit stage in the last chunk (it fades, never switches)
    float smFn, smRs, smDrv, smAmp, smD1, smA1, smF1, smR1;   // *1: after their per-sample (filter: per 4) smoothing
    // HOLD (DEC at its top): the voice holds (hold), held by a pad or a key until it is let go (padHeld), the decay once
    // the gate shuts (hrel: 0 = the machine's own). sus: held until the track's next trig (GATE off, a RAM loop on
    // HOLD, ITERA's free run): a stop, a mute or a pattern change lets it go (gateShut: it has been let go)
    int   hold, padHeld, sus, gateShut;
    float hrel;
    // v81: DRONE: the voice is the track's drone (started by the transport, changed by its trigs, ended by a stop, a mute,
    // the switch); the note it glides to (a drone's change: its new note, legato)
    int   drone;
    float noteT;
};

// an LFO: its phase in the cycle, this cycle's draw, its value; SLEW: where its glide started; LOOP: the cycles counted
// since it started (which draw of the loop plays) and where it is in its loop (for the page; -1: another wave)
struct Lfo { float ph, sh, val, from, lp; int k; };
// RDEC's and RTUN's LOOP and the LOOP wave: draw i of a loop (-1 .. 1), from its seed (a parameter, 0..1: 2^20 seeds) and
// a salt (the LFO, RDEC, RTUN); the same draws on the page (app.js loopDraw)
inline float loopDraw (float seedP, unsigned salt, unsigned i)
{
    unsigned h = (unsigned) (seedP * 1048576.0f) * 0x9E3779B1u + salt * 0x632BE5ABu + i * 0x85EBCA77u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return (float) (h >> 8) * (2.0f / 16777216.0f) - 1.0f;
}

// envelope follower sources: the track itself, a track (T1..T8), the host input, the master; bands
enum { EF_SELF = 0, EF_IN = 9, EF_MAIN = 10, EF_SRCS = 11, EF_FULL = 0, EF_LOW, EF_MIDB, EF_HIGHB, EF_BANDS };
// one envelope follower: its band filter, the follower, the fast follower that fires the triggered envelope and that
// envelope (stage 0 idle, 1 attack, 2 decay; held: the level is still above the threshold), the output (0..1) and,
// for the screen, the band's input level
struct EnvFol { float ic1, ic2, env, fast, ad; int stage, held; float val, in, pkVal, pkIn; };   // pk*: the highest since modInfo
// TONE: the state of its five filters (high-pass, low-pass, low shelf, mid bell, high shelf), two integrators each,
// and which of them ran in the last chunk (bit per filter)
struct ToneEq { float s[5][2]; int on; float pc[5][6]; float ps[5][6]; };   // pc: the last chunk's settings, ps: where the glide is

// SPRD (v64): its high-pass (over MONO), four all-passes (their lines, one write index), the amount (glided)
enum { SPD_AP = 4, SPD_LEN = 1024 };
struct TrackSpread
{
    float buf[SPD_AP][SPD_LEN];
    int   w, on;
    Svf   hp;
    float amt;
};

struct Track
{
    float p[NUM_PARAMS];
    u8    trig[NUM_STEPS];
    u8    rat[NUM_STEPS];      // ratchet: the step is split into n even hits (0/1 = single hit)
    u8    rec[NUM_STEPS];      // RAM: REC trigs (a take starts on the step); PLAY trigs are trig[]
    u8    tlk[NUM_STEPS];      // a step without a trig whose p-locks change the playing sound (no new hit)
    float lock[NUM_STEPS][NUM_PARAMS];
    int   len, spd, mute;
    int   q;             // follows the scale quantiser

    int   curStep;       // for UI playhead
    int   hitCount;      // increments on each trigger (UI flash)
    Voice voice[2];
    int   cur;

    // retrig
    int    rtLeft, rtTotal, rtStep;
    double rtNext, rtInterval;
    double rtBase, rtScale, rtRatio;   // RCRV: the rate's gap (RTRG), the gap now against it, how it changes hit to hit
    int    jAcc;                       // JUMP: where the jumps have got to (semitones or scale degrees), from 0
    float  rtVel, rtCurve;
    int    rtRatchet;      // the running roll is a ratchet (its rate stays fixed)

    Lfo   lfo[2];
    EnvFol ef[2];
    ToneEq tone;
    Rng   rng;
    TrackBox box;
    float bus[CHUNK];
    // MACRO: one Plaits unit per track, like the module (a new hit retriggers it, nothing doubles); the
    // units live in Engine::macMem so the tracks stay compact
    void* mac;
    // a knob turned while the track sounds: p glides to pt (12 ms) instead of jumping, one bit per entry of SLEW_PS
    float pt[NUM_PARAMS];
    unsigned slew;
    // the track's pan, sends, BOX mix and ducking in the last chunk (the next one glides from them)
    int   smOk;
    float smGl, smGr, smSd, smRv, smMix, smDuck;
    float smGl1, smGr1, smSd1, smRv1;      // pan and sends after their per-sample smoothing
    float smCh, smPh;                      // v68: the CHORUS and PHASER sends in the last chunk (a ramp to the next)
    int   hProg, hWalk;  // HALO: where its progression is (each trig a chord; back to the start when the transport starts), WALK's degree
    HaloWide wide;       // HALO: the stereo ensemble on the track's output (WIDE)
    int   bAbs;          // BREAK: the step of the last trig, counted from the start of the song (its STEP slice)
    // the ribbon (CHOIR's): touched, where (semitones from the track's pitch), how high (expression)
    int   ribOn;
    float ribSemis, ribExpr;
    // HALO, STRUM: the pitch classes of the chord it plays (bit 0 = C; 0 = none yet), the quantiser's when it follows it;
    // its root's (0..11)
    u32   chordPc;
    int   chordRoot;
    // CHOIR's SPACE (its reverb, in fx[]'s memory): running, its mix (glided), the side it makes
    int   cvOn;
    float cvMix, cvQuiet;
    float cvGate;                   // ALLOY's SPACE: its CUT (1 open .. 0 shut)
    int   wsNext;                   // WEST: the step its sequence plays next (a step a trig, the pulser off)
    float wsShake;                  // WEST (v78): its spring knocked by a trig (SSHK x its velocity), for westSpace
    // v64: RPAN, this hit's pan against the track's (drawn at the hit, applied, the side of the last ping-pong hit);
    // SPRD, its all-passes and crossover
    float rpanT, rpan; int rpanS;
    int   rdStep, rdSub;     // RDEC's and RTUN's LOOP: the step of the last hit and how many hits it has had (a roll's hits each draw)
    // CHIMERA (v66): its voices' sides (sbus), through the BOX's latency (sdly) and their own TONE; how many chunks they
    // still sound (sAct); the last hit for the screen: its samples, how many hits, the neighbours it drew from
    float sbus[CHUNK], sdly[LATENCY]; int sdw, sAct; ToneEq sTone;
    int   chA, chB, chHits, chNc, chCand[CHIM_NEAR]; float chAlign, chSemis;
    TrackSpread spr;
};

template <int N> struct DLine
{
    float b[(__SIZE_TYPE__) N]; int w; int len;
    void clear() { for (int i = 0; i < N; ++i) b[i] = 0.0f; w = 0; }
    inline void push (float x) { b[w] = x; if (++w >= len) w = 0; }
    inline float tap (int d) const { int i = w - 1 - d; while (i < 0) i += len; return b[i]; } // d = 0: last written
    inline float tapF (float d) const
    {
        int di = (int) d; float fr = d - (float) di;
        return tap (di) + (tap (di + 1) - tap (di)) * fr;
    }
    inline float oldest() const { return b[w]; } // delay of len samples
};

// allpass using a delay line: y = -g*x + d; push(x + g*y)
template <int N> struct Allpass
{
    DLine<N> d; float g;
    inline float tick (float x) { float del = d.oldest(); float v = x + g * del; d.push (v); return del - g * v; }
};

enum { DLY_SIZE = 524288, DLY_MASK = DLY_SIZE - 1 };

struct Reverb
{
    float sr, scale, size;
    DLine<4096>  pre;
    Allpass<1024> ap1; Allpass<1024> ap2; Allpass<4096> ap3; Allpass<2048> ap4;
    DLine<8192>  mapL, mapR;  float mapLenL, mapLenR;
    DLine<32768> d1L, d2L, d1R, d2R;
    Allpass<16384> apL; Allpass<32768> apR;
    float bwState, dampL, dampR, fbL, fbR;
    float lfoPh;
    float decay, damp;
};

// a stored pattern: per track its sounds, steps and settings; its p-locks as a list (track, step, parameter, value)
struct PatTrack { float p[NUM_PARAMS]; u8 trig[NUM_STEPS], rat[NUM_STEPS], rec[NUM_STEPS], tlk[NUM_STEPS]; u8 len, spd, q, pad; };
struct PatLock  { u8 t, s, p, pad; float v; };
struct PatSlot  { int used, chg, nlk; PatTrack trk[NUM_TRACKS]; PatLock lk[PAT_LOCKS]; };

struct HostTime
{
    int    valid;    // host provides transport info
    int    playing;
    double ppq;      // quarter notes at block start
    double bpm;
};

struct Engine
{
    float sr;
    float acA1, acA2, acA3;  // the sources' AC coupling: 2-pole high-pass at 20 Hz (TPT state-variable)
    float smoothC, smoothC4; // the rounding of the glides (one-pole, 0.5 ms; per sample, per 4 samples)
    Track tr[NUM_TRACKS];
    float g[NUM_GLOBALS];

    int    internalPlay;
    int    lite;           // 1 = skip BOX convolution and spectral reverb (slow plain-JS fallback)
    int    running, wasRunning;
    float  hwSide[CHUNK];           // HALO's ensemble: the side signal of the track being mixed
    float  cvSide[CHUNK];           // CHOIR's SPACE: its side
    float  chSide[CHUNK];           // CHIMERA: its sides (the track's, after the BOX's latency)
    float  alHiss[AL_HN];           // ALLOY: its HISS's level for each colour (at this rate)
    double pos16;          // song position in 16ths
    double posAnchor, clockAnchor; float anchorBpm;   // pos16 = posAnchor + (sampleClock - clockAnchor) * rate:
                                                      // event times do not depend on the host block size
    double sampleClock;    // absolute samples since init
    double now;            // absolute sample time of the current render position
    float  bpm;
    Rng    rng;

    // delay
    float dl[DLY_SIZE], dr[DLY_SIZE];
    int   dw; float dTime, dLpL, dLpR, dHpL, dHpR;
    float dlyOnF; int dlyOff, dlyClr;   // ON key: return fade (0..1); off and idle; samples of the buffer wiped so far
    int   dlyModel;                     // v68: the model running (0 STD, 1 1982; a change fades out, wipes, starts the other)
    Echo82 e82;                         // the 1982's converters, clock, VCO
    // v68: the CHORUS and the PHASER (sk_send), their ON keys' fades, their sends (stereo, after the pan)
    DimChorus cho; BiPhaser phs;
    float choOnF, phsOnF;
    float sChL[CHUNK], sChR[CHUNK], sPhL[CHUNK], sPhR[CHUNK];
    Reverb rv;             // PLATE (Dattorro)
    SpecVerb spec;         // SPECTRAL
    FdnReverb fdn;         // ROOM / HALL / SPACE
    SpringReverb spring;   // SPRING
    enum { PRE_LEN = 65536 };
    float preBuf[PRE_LEN]; int preW; float preLen;   // shared pre-delay (0..250 ms)
    int   revCur, revIdle; float revFade;
    float revOnF; int revOff;           // ON key: return fade (0..1); off and idle (starts from silence when back on)
    float rvL[CHUNK], rvR[CHUNK], rvIn[CHUNK];
    Master   master;       // EQ / compressor / drive / clipper, 4x oversampled
    Unmasker um;           // spectral room between the tracks
    Calm     calm;         // resonance suppressor on the master input
    int      role[NUM_TRACKS];
    u32      scaleMask;    // quantiser: allowed pitch classes (bit 0 = C), 0 = off

    // sequence looper (EP-133 style): the song position is folded into a window of loopLen 16ths.
    // effective = loopBase + offset + ((real - loopAnchor) mod loopLen); the real clock keeps running (slip).
    int    loopOn;
    double loopBase, loopAnchor, loopLen;
    float  loopInfo[5];    // for the UI: on, window start (16ths, with offset), length, effective position, real position

    // patterns (Elektron style): the one playing lives in tr[]; the others in pats[]. A change waits for the end of the
    // pattern (its CHG length, from where it started: patStart, in effective 16ths), or comes at once (START: from its
    // first step, JUMP: where the playhead is); a chain plays its patterns in turn. Edits go to editPat (the playing
    // one: tr[]; another: its slot), so an edit sent while the engine changes pattern still lands where it was meant.
    PatSlot pats[NUM_PATS];
    int    curPat, editPat, queued, qMode, curChg, patSerial, chainLen, chainPos, chain[CHAIN_MAX];
    double patStart;
    float  patInfo[PAT_INFO];  // for the UI: playing, queued, chain length, chain position, serial, CHG (16ths), position in its round, 16ths to the change

    alignas (16) unsigned char macMem[NUM_TRACKS][MACRO_BYTES];

    enum { PROC_MAXB = 1024 };   // process() splits longer host blocks
    alignas (16) unsigned char grainMem[GRAIN_BYTES];    // GRAIN (sk_grain), master effect
    Ott    ott;                                        // OTT
    Glitch glitch;                                     // GLITCH, last of the chain (sk_glitch)
    Rng    xrng;                                       // RAND's draws (apart: the machines' noise stays the same)
    float  xrndS;                                      // RAND, glided (and 0 when GLITCH is off)
    double glPos0, glNow0, glInc, glFree; int glRun;   // the song position at the block start, for the grid
    double glfoPh; float glfoVal;                      // GRAIN's LFO
    int    postWas;                                    // GRAIN / OTT were in the chain last chunk
    void   grainRun (float* L, float* R, int n);

    // organic sample bank (µLOOP sources, FRACT)
    OrgBank org;
    // BREAK: its ten slots (the host hands each loop over: breakSet)
    BreakBank brk;
    // RADIO (v87): the four receivers (their feeds, their histories); each track's two loops (the voice playing and
    // the next), the one taken last, how long each is
    RadioBank radio;
    alignas (16) short radLoop[NUM_TRACKS][2][RADIO_LOOP];
    int    radLast[NUM_TRACKS], radLen[NUM_TRACKS][2];
    // the FX machines: each track's state and memory, the tracks they take this chunk (bits), TONE for their side
    FxState fx[NUM_TRACKS];
    u32    fxTaken;
    ToneEq fxTone[NUM_TRACKS];

    // BOX
    IrBank bank;
    RFFT<2 * BOX_NA> fftA;
    RFFT<2 * BOX_NB> fftB;

    // scratch
    float mL[CHUNK], mR[CHUNK], sDL[CHUNK], sDR[CHUNK], sRv[CHUNK], vb[CHUNK];
    float duckAcc[CHUNK], duckState; u32 duckSrc;     // (DUCK: what track 1 played this chunk, its follower, its source)
    float wa[CHUNK], wb[CHUNK], cin[CHUNK];

    // track spectrum (UI display)
    enum { SPEC_BANDS = 24 };
    int   specTrack;
    float specB0[SPEC_BANDS], specA1[SPEC_BANDS], specA2[SPEC_BANDS], specS1[SPEC_BANDS], specS2[SPEC_BANDS], specEnv[SPEC_BANDS], specRel;
    void  specInit();
    void  specProcess (const float* x, int n);

    // RAM machine: the buffers, the takes; each track's output for the takes that record it; the host input
    alignas (16) short ramMem[NUM_TRACKS][RAM_LEN];
    RamBuf ram[NUM_TRACKS];
    float  trkOut[NUM_TRACKS][CHUNK];
    float  trkL[NUM_TRACKS][CHUNK], trkR[NUM_TRACKS][CHUNK];   // a track taken by an FX track: its stereo, as it would have reached the mix
    const float* inL; const float* inR;   // input of the current process() call (null: silence)
    double inStart;                       // absolute sample of its first frame
    float  inM[CHUNK], mainM[CHUNK];
    // envelope followers: the master as heard in the last chunk (MAIN source), its length
    float  efMain[CHUNK]; int efPrevN;
    // PRISM: the analysed frames of each buffer (strongest partial first: frequency in cycles per buffer sample,
    // amplitude), how many partials each frame holds; the analysis FFT and its work buffers; the output of a bank
    alignas (16) float prF[NUM_TRACKS][PR_FRAMES][PR_PK];
    alignas (16) float prA[NUM_TRACKS][PR_FRAMES][PR_PK];
    unsigned char prN[NUM_TRACKS][PR_FRAMES];
    RFFT<PR_WIN> prFft;
    float  prWin[PR_WIN], prX[PR_WIN], prRe[PR_WIN / 2 + 1], prIm[PR_WIN / 2 + 1], prM[PR_WIN / 2 + 1];
    float  prOut[CHUNK];
    // the wave tables: SINE TRI SAW PULSE, octave k holds 2^k harmonics (one guard sample at the end)
    float  prTab[PRT_SHAPES][PRT_LEV][PRT_N + 1];
    void   prismTables();
    // the new machines (sk_kit.cpp): the HEX analog kit and the three wavetable machines
    void   hexTrigger (Voice& v, const float* P);
    void   hexRender (Voice& v, const float* P, float tuneRaw, u32 qm, int n, float* out);
    void   alloyTrigger (Voice& v, const float* P);
    void   alloyRender (Voice& v, const float* P, float tuneSt, int n, float* out);
    void   chimTrigger (Voice& v, const float* P);
    void   chimRender (Voice& v, const float* P, float tuneSt, int n, float* out);
    void   cubeTrigger (Voice& v, const float* P);
    void   cubeRender (Voice& v, const float* P, float tuneSt, int n, float* out);
    // CUBE (v74): the banks the page loads (USER), their versions made as the engine runs; the screen
    CubeUser cube;
    void   cubeSetBank (int bank, const short* data, int n);
    void   cubeClear (int bank);
    void   cubeStep (int waves);
    void   cubeInfo (int ti, float* out);
    // HOWL (v81, sk_howl.cpp): its chord, its strings, the loop and the amp; a key let go (POLY); the hand on the strings
    void   howlTrigger (Voice& v, const float* P);
    void   howlRender (Voice& v, const float* P, float tuneSt, int n, float* out);
    void   howlRelease (Voice& v, float note);
    void   howlLetGo (Voice& v, float tau);
    void   howlInfo (int ti, float* out) const;
    // v84, MANTRA: a trig touches its sensors (TCH), a pad or a key let go, a stop lets go of them all; held (a finger on
    // a sensor, a held touch); its screen
    int    mtPadStart;
    void   mantraTrigger (Voice& v, const float* P);
    void   mantraRender (Voice& v, const float* P, float tuneSt, int n, float* out);
    void   mantraRelease (Voice& v);
    void   mantraLetGo (Voice& v);
    bool   mantraHeld (const Voice& v) const;
    void   mantraInfo (int ti, float* out) const;
    // v81: DRONE: does the track want one now (the switch on, a machine that can hold one), it started (the transport),
    // changed (a trig, a pad: its locks, its note), ended (a stop, a mute, the switch, another machine)
    int    droneHit;
    bool   droneWanted (const Track& t) const;
    void   droneStart (Track& t, int ti);
    void   droneChange (Track& t, int ti, Voice& v, const float* lk, int step, float vel, float note);
    void   droneEnd (Voice& v);
    // WEST (v76): its voice, its spring on the track, the screen
    void   westTrigger (Voice& v, const float* P);
    void   westRender (Voice& v, const float* P, float tuneSt, int n, float* out);
    void   westSpace (Track& t, int ti, const float* P, float* out, float* side, int n);
    void   westInfo (int ti, float* out);
    void   chimSides (Voice& v, int ft, float fnorm, int n);
    // HALO (sk_halo.cpp)
    void   haloTrigger (Voice& v, const float* P);
    void   haloRender (Voice& v, const float* P, float tuneRaw, u32 qm, int n, float* out);
    int    haloChord (Voice& v, const float* P, float tuneRaw, u32 qm, float* notes, int* key, const int* cmp);
    void   haloProg (Voice& v, const float* P);
    void   haloAssign (Voice& v, const float* P, const float* notes, int m, bool trig);
    void   haloWide (Track& t, const float* P, const float* x, float* side, int n);
    void   ageFx (Voice& v, const float* P, int n, float* out);
    void   ageReset (Voice& v);
    // STRUM (sk_strum.cpp), CHOIR (sk_choir.cpp)
    void   strumTrigger (Voice& v, const float* P);
    void   strumRender (Voice& v, const float* P, float tuneRaw, u32 qm, int n, float* out);
    void   strumInfo (int t, float* out) const;
    void   choirTrigger (Voice& v, const float* P);
    void   choirRender (Voice& v, const float* P, float tuneRaw, float tuneSt, u32 qm, int n, float* out);
    void   choirSpace (Track& t, int ti, const float* P, float* out, float* side, int n);
    void   alloySpace (Track& t, int ti, const float* P, float* out, float* side, int n);
    void   trackSpread (Track& t, const float* P, float* out, float* side, int n);
    void   choirInfo (int t, float* out) const;
    int    retrigHit;                                   // (the voice starting is a retrig's or a ratchet's)
    // BREAK (sk_break.cpp)
    // the slots: s's loop is in brk.mem (s) (n samples, mono, 32 kHz, cut to its bars): it is read over the next blocks
    void   breakSet (int s, int n, int bars);
    void   breakClear (int s);
    void   breakBars (int s, int bars);                          // its bars again (the tempo doubled or halved)
    void   breakWork (int budget);                               // reads the slots waiting (budget samples at most)
    void   breakSlots (float* out) const;                        // the version, then per slot BRK_SLOT_INFO values
    void   breakPeaks (int s, float* out) const;                 // its waveform (BRK_NPEAK values)
    void   breakTrigger (Voice& v, const float* P);
    void   breakRender (Voice& v, const float* P, int n, float* out);
    // RADIO (sk_radio.cpp). The host, from its own thread: a receiver's sound (mono, RADIO_RATE, 16-bit; what fits: how
    // much was taken), how much fits, its sound from now on new (the rest dropped), its state (RS_ ..) and kind (RK_ ..),
    // its S-meter (dBm; under -300: none). The engine: the receivers the tracks play (bits), each chunk's reading, the
    // voice, the screens
    int    radioPush (int rx, const short* x, int n);
    int    radioSpace (int rx) const;
    void   radioFlush (int rx);
    void   radioSetState (int rx, int state, int kind);
    void   radioMeter (int rx, float dbm);
    int    radioUse() const;
    void   radioAdvance (int n);
    void   radioTrigger (Voice& v, const float* P);
    void   radioRender (Voice& v, const float* P, float tuneSt, int n, float* out);
    void   radioInfo (int t, float* out) const;                 // RADIO for its screen (RADIO_INFO values)
    void   radioRxInfo (float* out) const;                      // the receivers (RADIO_RX x RADIO_RX_INFO values)
    int    prRR;
    float  prInfo[2 * PR_K];

    // io buffers (used by the wasm wrapper)
    float outL[4096], outR[4096];
    float inBufL[4096], inBufR[4096];

    // -------- API --------
    void init (float sampleRate, int liteMode = 0);   // lite: no BOX / spectral reverb (the IR bank is not built)
    void setSampleRate (float sampleRate);
    void resetPattern();
    void setParam (int t, int p, float v);
    void setTrig (int t, int s, int on);
    void setTrigless (int t, int s, int on);              // a lock-only step: its p-locks change the playing sound
    void setLock (int t, int s, int p, float v);
    void clearLocks (int t, int s);
    void setRatchet (int t, int s, int n);
    void setLen (int t, int l);
    void setSpeed (int t, int i);
    void setMute (int t, int m);
    void setGlobal (int gi, float v);
    void setScale (u32 mask)        { scaleMask = mask & 0xFFFu; }
    // the notes the quantiser holds to now: its scale, or the chord of the track G_QSRC names (a HALO or STRUM that has
    // played one; otherwise the scale)
    u32  quantMask() const;
    // the notes a HALO or STRUM track builds its chords in (MODE KEY): the scale for the track the quantiser follows
    // (it never follows itself), the quantiser's notes for the others (the followed chord: they play along with it)
    u32  harmMask (int ti) const;
    // a chord track playing along with the one the quantiser follows: that one's root (0..11), else -1
    int  harmRoot (int ti) const;
    void setQuant (int t, int on);
    void setParamLive (int t, int p, float v);           // (the tracks themselves, whatever the pattern edited)
    static int slewBit (int p);                          // the glide bit of a parameter (-1: none)
    void setPlay (int on);
    void audition (int t, float vel, float note = 0.0f);   // note: semitones (the KEY mode's keys)
    void release (int t, float note = 0.0f);   // a pad or a key let go: a note it holds (DEC HOLD) ends
    // the ribbon (CHOIR): touched (gate 1; 2: SNAP, held to the quantiser's notes, gliding from one to the next in
    // GLID) at semis (from the track's pitch), expr its height (0..1); moved; let go (gate 0)
    void ribbon (int t, float semis, int gate, float expr);
    // v84, MANTRA: a finger on one of the screen's eight pads (k 0 .. 7), its pressure (0 .. 1; 0: lifted)
    void mantraTouch (int t, int k, float pressure);
    // patterns: the one playing (a project loaded: no change), all slots emptied, where the edits go, a slot emptied,
    // a change queued (mode 0 at the end of the pattern, 1 at once from its start, 2 at once where the playhead is), the
    // chain (its patterns, then its length: 0 = none), the CHG length of the pattern edited (16ths, 0 = AUTO)
    void patCur (int i);
    void patAll();
    void patEdit (int i);
    void patClear (int i);
    void patQueue (int i, int mode);
    void patChain (int k, int i);
    void patChainLen (int n);
    void patChg (int n);
    const float* getPatInfo();
    void panic();
    void process (float* L, float* R, int n, const HostTime* ht);
    // RAM machine
    void setRec (int t, int s, int on);                  // a REC trig
    void setInput (const float* l, const float* r);      // the host input for the next process() call (r may be l)
    void ramStart (int t, int step);                     // start a take now (step: its p-locks; -1 = none)
    void ramLoaded (int t, int len, float rate);         // the host wrote len samples into ramMem[t]
    void ramCopy (int from, int to);                     // v80: a track's take onto another (Paste track)
    void ramSwap (int a, int b);                         // v80: two tracks' takes exchanged (Move track)
    void ramClear (int t)                  { ramLoaded (t, 0, sr); }
    // per track: samples, sample rate, version, state (0 idle, 1 waiting for its source / the threshold, 2 recording),
    // progress of the take (0..1), and the counter of its display peaks (ram[t].pk)
    enum { RAM_INFO = 6 };
    void ramInfo (float* out) const;
    // PRISM of a track for the display: its 16 oscillators (frequency Hz, amplitude), 0 when silent
    const float* prismInfo (int t);
    int  getStep (int t) const { return tr[t].curStep; }
    // master meters since the last reset: peak gain reduction (dB) and output peak (linear)
    float meterGr() const  { return master.comp.grPeak; }
    float meterOut() const { return master.outPeak; }
    void  meterReset()     { master.comp.grPeak = 0.0f; master.outPeak = master.inPeak = 0.0f; }
    // MIX page display: 8 calm slots (frequency Hz, gain dB), then the unmasking gains (8 tracks x 10 bands, dB)
    void  mixInfo (float* out) const;
    int  getHits (int t) const { return tr[t].hitCount; }
    const float* getLoopInfo() const { return loopInfo; }
    // spectrum of one track for the UI (its signal after the track's filter, drive, BOX and ducking):
    // SPEC_BANDS band-pass filters spread from 40 Hz to 16 kHz, peak envelopes (linear). -1 = off.
    void  setSpecTrack (int t);
    const float* specInfo() const { return specEnv; }

    // -------- internals --------
    void   triggerTrack (int t, int step, float velMul, int isRetrig);
    void   triglessStep (int t, int step);                 // a lock-only step reached: its locks join the playing voice
    void   startVoice (Track& t, int ti, const float* lk, int step, float vel, float note, float holdSec);
    void   holdSetup (Voice& v, const float* P, int m, float& holdSec);
    void   letGo (Voice& v);
    float  jumpNote (Track& t, int step, float note);                     // JUMP: this trig's note, the jump moved on                                              // a note held until its next trig ends now (HOLD_REL)   // DEC on HOLD (and a pad held): the voice's gate
    int    padHit;                                                       // (audition: the voice starting is a pad's or a key's)
    void   renderSegment (int n, float* L, float* R);
    void   renderChunk (int n, float* L, float* R);
    void   renderVoice (Track& t, Voice& v, int n);
    void   ramRecord (int n, const float* L, const float* R);
    void   ramWindow (Voice& v, const float* P) const;
    void   prismAnalyze();                              // a few frames per chunk, for the buffers PRISM plays
    void   prismFrame (int b, int f, int lim);
    void   prismBank (Voice& v, const float* P, float tuneSt, int n);   // match the frame, glide, render into prOut
    void   trackPost (Track& t, int ti, int n);
    void   effParams (const Track& t, const float* lk, float* P) const;
    void   efAdvance (int n);                                   // the 16 envelope followers, once per chunk (sk_mod.cpp)
    void   toneProcess (ToneEq& q, const float* P, float* x, int n);  // TONE on a track's output (sk_mod.cpp)
    // the FX machines (sk_fx.cpp)
    int    fxMachineOf (const Track& t) const;
    void   fxPrepass();
    void   fxSetup (FxState& f, int m);
    void   fxReset (int ti);
    void   fxRun (Track& t, int ti, const float* P, int n, double pos16, double inc);
    // (each takes the source left and right, xL / xR, and gives its effect, wL / wR)
    void   fxVerb (FxState& f, const float* P, const float* xL, const float* xR, const float* g, float* wL, float* wR, int n);
    void   fxEcho (FxState& f, const float* P, const float* xL, const float* xR, const float* g, float* wL, float* wR, int n, double spt);
    void   fxBendDivision (FxState& f, const float* P, double D);
    void   fxBend (FxState& f, const float* P, const float* xL, const float* xR, float* wL, float* wR, int n, double pos16, double inc, double spt);
    void   fxTape (FxState& f, const float* P, const float* xL, const float* xR, float* wL, float* wR, int n);
    void   fxCodecFrame (FxState& f, const float* P);
    void   fxCodec (FxState& f, const float* P, const float* xL, const float* xR, float* wL, float* wR, float* dL, float* dR, int n);
    // (FUZZ mixes the direct sound in itself, at 4x: wL / wR are the track's sound, MIX and the gate done)
    void   fxFuzz (FxState& f, const float* P, const float* xL, const float* xR, const float* g, float* wL, float* wR, int n);
    void   fxInfo (int t, float* out);
    void   glitchInfo (float* out) const;                       // GLITCH for its screen (22 values, see sk_wasm)
    void   haloInfo (int t, float* out) const;                  // HALO for its screen (HALO_INFO values, see sk_halo.cpp)
    void   breakInfo (int t, float* out) const;                 // BREAK for its screen (BRK_INFO values, see sk_break.cpp)
    void   chimInfo (int t, float* out) const;                  // CHIMERA for its screen (CHIM_INFO values, see sk_chimera.cpp)
    void   modInfo (float* out);                                // the screen: per track LFO phase / value x 2, EF value / input x 2 (peaks since the last call)
    void   machineRender (Voice& v, const float* P, int n, float* out);
    void   machineTrigger (Voice& v, const float* P);
    int    pickVar (Voice& v);
    void   spawnParticle (Voice& v, float rate, float vari, int full, float gsec, float gain);
    float  lock (const Track& t, int step, int p) const;
    float  trigParam (const Track& t, int step, int p) const;   // lock value + the LFOs that target p (trig-time reads)
    int    lfoTargets (const Track& t, int step, int p) const;
    double samplesPerStep (const Track& t) const;
    // patterns (internals): the playing pattern into its slot, a slot into the tracks, a change, the CHG length now
    void   patSave (int i);
    void   patLoad (int i);
    void   patSwitch (int i, double start);
    double patChangeLen() const;
    int    patNext() const;                       // the pattern the next change goes to (-1: none)
    // the edits that land in a slot (the pattern edited is not the one playing)
    PatSlot* patTarget (int t);
    void   slotLock (PatSlot& sl, int t, int s, int p, float v);
    void   fxInit();
    void   boxInit();
    void   delayProcess (int n);
    void   delay82 (int n, int on, float fb, float la, float ha, float p, float vA, float vB, int hold);   // the 1982 model (v68)
    float  delayVco (int n, float& envIn);                            // the VCO (octaves) for this chunk, both models
    void   sendFx (int n);                                            // the CHORUS and the PHASER (v68)
    void   reverbProcess (int n, float* inM, float* oL, float* oR);   // dispatches to the selected algorithm
    void   revStart (int a);                                          // an algorithm from silence
    void   plateProcess (int n, const float* in, float* oL, float* oR);
    void   plateSize (float size);
    int    revAlgo() const;
    double loopOffset() const;                    // POS offset in 16ths
    double loopMapRaw (double p) const { double q = p - loopAnchor; q -= __builtin_floor (q / loopLen) * loopLen; return loopBase + q; }
};

} // namespace sk
