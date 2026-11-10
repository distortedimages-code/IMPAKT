// IMPAKT — RADIO (v87): what the hosts decode a receiver's sound with, before they hand it to the engine (radioPush):
// an MP3 stream (minimp3, CC0: core/ext), plain 16-bit sound, or the IMA ADPCM a shortwave receiver sends; downmixed
// to mono and resampled to RADIO_RATE (a windowed sinc: 12 zero crossings, cut at 45 % of the lower rate). The host's
// network thread runs it (the plugin), or the page's audio thread (the browser); never the engine's own.
#pragma once

#include "sk_math.h"

namespace sk {

struct RadioDec
{
    enum { IN_CAP = 32768, OUT_CAP = 16384, RING = 1024, NZ = 12, RES = 256 };
    // MP3: the decoder's state (minimp3's, opaque here), the bytes not decoded yet
    alignas (16) unsigned char mp3st[8192];
    unsigned char in[IN_CAP];
    int   inN;
    // the resampler: the source's last samples, how many came, where the next output falls (in source samples), the
    // step (source samples an output), the cut (zero crossings a source sample), the kernel's half length
    float ring[RING];
    int    srcN;
    double T, step;
    float  c, D, srcRate;
    float  proto[NZ * RES + 2];
    // the output (RADIO_RATE, mono): what the host takes (take)
    short out[OUT_CAP];
    int   outN;
    // IMA ADPCM: its index and last value
    int   adIdx, adPrev;
    // what came: its rate, channels, kbit/s; frames decoded, bytes skipped
    int   rate, chans, kbps;
    u32   frames, skipped;
    float gain;

    void reset();                                   // (the kernel's table too: once, before the first use)
    void restart();                                 // a new stream: the decoder and the resampler from silence
    int  inRoom() const { return IN_CAP - inN; }
    int  outRoom() const { return OUT_CAP - outN; }
    // the bytes of an MP3 stream (as many as inRoom); then decodes what it can (as long as the output has room)
    void mp3Feed (const unsigned char* b, int n);
    bool mp3Work();                                 // (more decoded, from what is in hand: false when it needs more)
    // n frames of 16-bit sound, ch channels interleaved, at rate
    void pcmFeed (const short* x, int n, int ch, float rate);
    // a shortwave receiver's IMA ADPCM (4 bits a sample, the low nibble first), at rate
    void adpcmFeed (const unsigned char* b, int n, float rate);
    // what is decoded: up to max samples into dst (how many)
    int  take (short* dst, int max);

private:
    void setRate (float r);
    void put (float x);
};

} // namespace sk
