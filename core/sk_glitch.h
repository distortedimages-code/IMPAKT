// IMPAKT — GLITCH: the last effect of the master chain (page GLCH). Own implementation, after the ideas of
// circuit-bent buffer modules (repeats, skips, varispeed, corruption) and of low-bitrate codec emulation.
//
//  BENDER   a 2.7 s stereo buffer of the master. On a grid locked to the tempo (TIME, 1/64 .. 1 bar), each
//           division may become an event: REPEAT (its first 1/n played n times: stutters, micro-glitches),
//           BREAK (a jump back into the buffer, or a dropout), BEND (the previous division at another speed,
//           backwards, or slowing to a tape stop). HOLD loops the last division until it is released. Every
//           seam is faded (0.7 ms): it glitches, it does not click.
//  CORRUPT  DECIM (bits and sample rate), DROP (random dropouts: few and long, then many and short), DSTR
//           (soft saturation, then hard clipping).
//  LOSS     a codec in the spectrum: 512-point frames, hop 128, square-root Hann (the frames sum back to the
//           input when nothing is lost). Per frame: the bandwidth narrows, the bands below a dynamic range
//           are dropped, the rest is quantised (steps that grow with LOSS), SPEED from smeared (the band
//           decisions and magnitudes move slowly) to garbled (every frame decides again). Modes: STD (what the
//           codec keeps), INV (what it throws away), JIT (phase and timing errors), PKT·L (packets lost: the
//           codec with gaps), PKT·R (packets repeated). RESO brings the codec's peaks out of its noise (the
//           spectrum sharpened, its energy kept) and puts a resonance at the edge of its band.
//           ANA swaps the codec for a tape: record emphasis into a saturation, wow and flutter (SPD from the one to
//           the other), failures (level drops, snags of the transport, crinkles), hiss, then the playback heads
//           (low cut, head bump, a resonant high cut: RESO). Five machines: CASS, VHS, MICRO, TOY, WORN.
//           While LOSS is up the output runs 512 samples late (10.7 ms), digital or analog; it fades in and out
//           of that path, and from the one to the other.
#pragma once

#include "sk_math.h"
#include "sk_fft.h"

namespace sk {

struct GlitchParams
{
    int   on;
    float tim;           // grid: 1/64, 1/32, 1/16, 1/8, 1/4, 1/2, 1 bar
    float rpt, brk, bnd; // how often each event comes (and how far it goes)
    float crp; int cmd;  // corrupt amount, mode (0 DECIM, 1 DROP, 2 DSTR)
    float los; int lmd;  // loss amount, mode (0 STD, 1 INV, 2 JIT, 3 PKT·L, 4 PKT·R)
    float spd;           // loss speed: 0 smeared .. 1 garbled
    float mix;           // dry / wet of the whole effect
    int   hld;           // HOLD: loop the last division
    float res;           // LOSS's resonance (clarity)
    int   ana;           // LOSS: 0 the codec, 1 the tape
    int   amd;           // tape machine (0 CASS, 1 VHS, 2 MICRO, 3 TOY, 4 WORN)
};

enum { GL_PASS = 0, GL_REPEAT, GL_JUMP, GL_DROP, GL_BEND, GL_HOLD, GL_TYPES };
enum { GL_CDECIM = 0, GL_CDROP, GL_CDSTR, GL_CMODES };
enum { GL_LSTD = 0, GL_LINV, GL_LJIT, GL_LPKTL, GL_LPKTR, GL_LMODES };
enum { GL_ACASS = 0, GL_AVHS, GL_AMICRO, GL_ATOY, GL_AWORN, GL_AMODES };
enum { GL_GRIDS = 7 };

class Glitch
{
public:
    enum { BUF = 1 << 17, MASK = BUF - 1, FN = 512, HOP = 128, NB = FN / 2 + 1, HIST = 16 };
    void init (float sr);
    void reset();
    // pos16: the song position of the chunk's first sample (16ths), inc: 16ths per sample
    void process (float* L, float* R, int n, const GlitchParams& p, double pos16, double inc);
    bool busy() const { return onMix > 0.0f; }
    // for the screen: the last divisions (event types, oldest first, -1 = none yet), the current one's
    // progress (0..1) and its detail (repeats, rate x 100, jump back in divisions), the codec's bandwidth (Hz)
    // (the tape's high cut on ANA) and the tape's speed error (cents)
    int   hist[HIST];
    float curProg, curDetail, lossBw, lossCents;

private:
    float sr;
    float bufL[BUF], bufR[BUF];
    double w;                    // samples written so far (a double: no 64-bit integers in the JS build)
    Rng   rng;
    // the division being played
    int    evType, evR, evBack;
    double evStart, evLen;       // absolute sample index where it starts, length (samples)
    double evRate, evPh;
    double holdStart;
    double divIdx;               // which grid division (floor (pos / grid))
    double gridPrev;
    float  gFade;                // 0 = direct, 1 = from the buffer (crossfade at division edges)
    float  onMix;                // ON / OFF fade
    // corrupt
    float  hL, hR, hCnt;         // DECIM: held sample
    float  dropG; int dropLeft;  // DROP: gain, samples left in the current dropout
    float  cmix;                 // the corrupt stage faded in
    // loss
    RFFT<FN> fft;
    float  win[FN];
    float  inBuf[2][FN];         // the last FN input samples (ring)
    float  olaBuf[2][FN];        // overlap-add accumulator (ring)
    float  dryD[2][FN];          // the dry signal, delayed like the codec
    int    ring, hopCnt;
    float  lossMix;              // fade into / out of the codec path
    float  bandG[2][24], prevMag[2][NB], prevRe[2][NB], prevIm[2][NB];
    int    pktLeft, pktDrop;     // PKT modes: frames left in the current packet, dropped / repeated
    float  fx[FN], re[NB], im[NB];
    float  rm[NB], rt[NB], rc[NB + 1];   // RESO: the magnitudes, sharpened, their running sum
    // tape
    enum { TBUF = 2048, TMASK = TBUF - 1 };
    float  tapeBuf[2][TBUF];
    int    tw;
    float  anaMix;               // codec <-> tape
    float  eLp[2], dLp[2];       // record emphasis, its exact inverse after the saturation
    float  hpS[2][2], bmS[2][2], lpS[2][2], hsLp[2];
    double wowPh, flPh;
    float  wr0, wr1, wrT, wrPos, flN, flN2;   // the wow's smooth random, the flutter's noise
    float  snagA, snagT, snagLen; float dropT, dropEnv; int dropLeft2; float crk[2];
    float  dPrev;

    void   tapeReset();
    void   newDivision (const GlitchParams& p, double now);
    float  readBuf (const float* b, double pos) const;
    void   codecFrame (int ch, const GlitchParams& p);
};

} // namespace sk
