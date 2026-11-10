// IMPAKT — RADIO (v87): a machine that plays the radio. Own implementation.
//
//  RECEIVERS  four, shared by the project's RADIO tracks (RX 1 .. 4). Each one is tuned by the host to an internet
//             station (WEB: an MP3 stream) or to a frequency on a shortwave receiver on the network (SW); the host
//             fetches, decodes and resamples it (sk_radio_dec) and hands the engine its sound, mono, at RADIO_RATE,
//             from its own thread (radioPush: a ring the engine reads, the network's jitter taken up in it). The
//             engine reads it a little faster or slower to keep about a second in hand (RADIO_SW_FILL for the
//             receivers: half a second), so the stream's clock never drifts from the audio's; it waits for that much
//             before it plays, and again after the network ran dry. Read, it goes into the receiver's history (its
//             last 21 s, on the engine's own time): what the tracks play.
//  PLAY       TUNE (LIVE: shifts the signal up or down, as a receiver tuned beside it; LOOP, DIG: transposes, as a
//             tape), DEC (HOLD at its top: it plays on until the next trig or the key let go), RX (the receiver), MODE
//             (LIVE: what the receiver plays now, a trig opening it; LOOP: a trig takes the last LEN of it and plays
//             that, in time; DIG: a trig takes LEN from further back, DPTH, somewhere), LEN (LIVE: how long a trig
//             opens it, FREE: as long as DEC; LOOP, DIG: the loop, FREE: a bar), D (LIVE: SQL, the squelch: only what
//             is loud enough comes through; LOOP: GRAB, the chance a trig takes a new loop: at 0 the first one stays;
//             DIG: DPTH, how far back it digs: 1 .. 20 s), STAT (the static: the hiss and the crackle of the air, what
//             is heard alone when nothing comes in), SIDE
//  AIR        BAND (the receiver's bandwidth: FULL .. 250 Hz), CNTR (where the band sits: 300 Hz .. 3 kHz), FADE (the
//             signal fading in and out, a notch wandering through it), RATE (how fast), HET (a carrier's whistle
//             beating against it), CRKL (crackle, the crashes of a storm far away), DRIV (the detector and the small
//             speaker pushed)
#pragma once

#include "sk_math.h"
#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace sk {

enum {
    RADIO_RX = 4,
    RADIO_RATE = 24000,                       // the receivers' rate (what the host hands over, what the history holds)
    RADIO_FIFO = 1 << 17,                     // the feed: 5.4 s at most in hand
    RADIO_HIST = 1 << 19,                     // the history: 21.8 s
    RADIO_LOOP = 1 << 17,                     // a track's loop (two of them: the voice playing and the next): 5.4 s
    RADIO_NPEAK = 128, RADIO_PKLEN = 512,     // the screen: the last 2.7 s, in 128 peaks of 21 ms
    RADIO_LENS = 11, RADIO_MODES = 3,
    RADIO_HEAD = 16, RADIO_INFO = RADIO_HEAD + RADIO_NPEAK,
    RADIO_RX_INFO = 8                         // per receiver, for the hosts' tuner
};
// a receiver's state (the host sets it): nothing tuned, off, connecting, waiting for its sound (and the engine waiting
// for enough of it), playing, retuning (a new frequency asked, the old sound flushed), the receiver busy (no channel
// free), failed, on standby (no track plays it: the host let the connection go)
enum { RS_NONE = 0, RS_OFF, RS_CONNECT, RS_BUFFER, RS_LIVE, RS_TUNING, RS_BUSY, RS_FAIL, RS_STANDBY };
enum { RK_WEB = 0, RK_SW = 1 };
enum { RM_LIVE = 0, RM_LOOP, RM_DIG };
// the side places (S1 ..): AIR
enum { RA_BAND = 0, RA_CNTR, RA_FADE, RA_RATE, RA_HET, RA_CRKL, RA_DRIV, RA_NSIDE };

// LEN: sixteenths, FREE last (0)
inline float radioLen16 (int i)
{
    static const float L[RADIO_LENS] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 0 };
    return L[i < 0 ? 0 : (i >= RADIO_LENS ? RADIO_LENS - 1 : i)];
}
// TUNE on LIVE: the shift, Hz (25 Hz a semitone of the knob: +- 600 Hz)
inline float radioShiftHz (float st) { return st * 25.0f; }
// DPTH (DIG): how far back, seconds
inline float radioDepth (float v) { return 1.0f * powf_ (20.0f, clampf (v, 0.0f, 1.0f)); }
// SQL: the threshold, dB (OFF at 0)
inline float radioSqlDb (float v) { return -66.0f + 56.0f * v; }
// BAND: the bandwidth (Hz; FULL at 0), CNTR its centre (Hz)
inline float radioBandHz (float v) { return 12000.0f * powf_ (250.0f / 12000.0f, clampf (v, 0.0f, 1.0f)); }
inline float radioCntrHz (float v) { return 300.0f * powf_ (10.0f, clampf (v, 0.0f, 1.0f)); }
inline float radioRateHz (float v) { return 0.04f * powf_ (100.0f, clampf (v, 0.0f, 1.0f)); }

// (a word shared with the host's thread: read and written whole, in order)
#if defined(_MSC_VER)
inline int  rdLoad (const volatile int* p)  { return _InterlockedOr ((volatile long*) p, 0); }
inline void rdStore (volatile int* p, int v) { _InterlockedExchange ((volatile long*) p, v); }
#else
inline int  rdLoad (const volatile int* p)  { return __atomic_load_n (p, __ATOMIC_ACQUIRE); }
inline void rdStore (volatile int* p, int v) { __atomic_store_n (p, v, __ATOMIC_RELEASE); }
#endif

struct RadioRx
{
    // the feed: the host writes (wr), the engine reads (rd); both count on (masked into the ring)
    short fifo[RADIO_FIFO];
    volatile int wr, rd;
    volatile int flushReq;                    // (the host's: its sound from now on is new: the engine drops the rest)
    volatile int state, kind;                 // (RS_ .., RK_ ..)
    volatile int meterC;                      // the receiver's S-meter (centi-dBm; -32768: none)
    // the engine's side: where it reads in the feed, how fast, waiting to fill, how often it ran dry
    int    flushSeen, priming, underruns;
    float  fpos, ratio;
    float  gain;                              // (what it writes into the history fades in and out with the signal)
    // the history (the engine's time): hw samples written; the samples still due this chunk
    short  hist[RADIO_HIST];
    long long hw;
    double hacc;
    float  sig;                               // the signal there (0 .. 1: the voices fade between it and the air)
    float  level, fill;                       // (the screens: its peak, seconds in hand)
    unsigned char peak[RADIO_NPEAK]; int pkW, pkN; float pkAcc;
};

struct RadioBank
{
    RadioRx rx[RADIO_RX];
    int     ver;
    void    reset();
};

// a track's voice on RADIO
struct RadioHead { float pos, g, dg; int on; };
struct RadioState
{
    int    mode, rx, buf, grabbed, fresh;
    double p;                                 // LIVE: where it reads in the receiver's history
    int    L;                                 // LOOP, DIG: the loop (radio samples)
    RadioHead h[2];                           // its two read heads (one fading out at the seam, one in)
    double period, el;                        // the loop's period (output samples), how far into it
    int    gateLeft; float gate;              // LIVE: LEN's gate (output samples left), its level
    // TUNE on LIVE: the shifter (two allpass chains a quarter turn apart), its oscillator
    float  ax[8][2], ay[8][2], adl; float shPh;
    // AIR
    float  bh1[2], bh2[2], bl1[2], bl2[2];     // the band: high-pass, low-pass (two SVFs each: their states)
    float  fdA, fdB, fdPh, fdN[2], fdF;        // the fading: from, to, where between, its notch, its frequency
    float  htPh, htF, htT;                     // the whistle: phase, frequency, where it wanders to
    float  ckE, ckLp; int ckLeft;              // the crackle: a burst's envelope, its colour, its length left
    float  pk[3];                              // the hiss's colour
    float  sqE, sqG;                           // the squelch: the signal's envelope, the gate
    float  dLp, dPh, swPh;                     // the speaker; the slow wobble; the dial's sweep while it tunes
    float  live;                               // (the screen: where the loop is, 0 .. 1)
};

} // namespace sk
