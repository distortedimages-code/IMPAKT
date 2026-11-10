// IMPAKT — the sample bank of CHIMERA (v66): the kit's samples, decoded once into one pool that every engine of the
// process reads (the page's, the plugin's instances), with what the bank's tool measured on each one (tools/make_bank.py).
//
// The bank is two kinds of files (bank/): chimera.idx, its index (the header below), and chimera.N.bin, the samples
// coded one after another (SKC, lossless: mid / side, fixed predictors of order 0..4, Rice codes; bankEncode,
// bankDecode). The page fetches them and feeds them in (a sample at a time, over the audio blocks: bankStep); the
// plugin reads them from its bundle and decodes them at once.
//
// Index (little endian): "SKB1", version, n, ncat; per category: a byte of length, its name; per sample: category (u8),
// channels (u8), name length (u16), frames, rate, crc, coded bytes, file, offset in it (u32 each), gain, pitch (Hz, 0:
// no pitch), onset (s), the 8 features (z-scores: length, attack, brightness, noisiness, lows, highs, pitchedness,
// width), its place on the map (x, y in 0..1), then its name and its outline (64 peaks, for the page). Samples are
// stored by category.
#pragma once
#include "sk_math.h"
#ifndef __wasm__
#include <atomic>
#endif

namespace sk {

// The plugin decodes the bank on a thread of its own while the audio thread plays from it: what the decoder wrote
// before it raised a flag (the index before state, a sample before its ok) is seen by a reader that saw the flag
// (bankPublish before the flag, bankAcquire after reading it; nothing in the single-threaded WebAssembly build)
#ifdef __wasm__
inline void bankPublish() {}
inline void bankAcquire() {}
#else
inline void bankPublish() { std::atomic_thread_fence (std::memory_order_release); }
inline void bankAcquire() { std::atomic_thread_fence (std::memory_order_acquire); }
#endif

enum { BANK_MAX = 256, BANK_CATS = 16, BANK_FEAT = 8, BANK_NAME = 40, BANK_BLOCK = 4096, BANK_PART = 256 };
// the pool: 16-bit samples, stereo interleaved (L R) or mono; SK_BANK_LITE (the plain-JS build): none
#ifdef SK_BANK_LITE
constexpr int BANK_CAP = 1 << 16;
#else
constexpr int BANK_CAP = 42 << 20;
#endif
constexpr int BANK_STAGE = 4 << 20;          // the page writes a sample's coded bytes here (sk_bank_stage)

// the features (z-scores over the bank)
enum { BF_LEN = 0, BF_ATK, BF_BRIGHT, BF_NOISE, BF_LOW, BF_HIGH, BF_PITCHED, BF_WIDTH };

struct BankEntry
{
    int   off, frames, ch, cat, file, foff, coded;
    u32   crc;
    float sr, gain, pitch, onset, mx, my;
    float feat[BANK_FEAT];
    char  name[BANK_NAME];
    volatile int ok;                          // decoded and checked: the engines may play it
};

struct Bank
{
    int   n, ncat, used, state;               // state: 0 none, 1 index read (decoding), 2 all decoded, -1 error
    int   done, failed;                       // samples decoded / refused (crc, size)
    int   catFirst[BANK_CATS], catCount[BANK_CATS];
    char  catName[BANK_CATS][16];
    BankEntry e[BANK_MAX];
    // the decoder at work (bankFeed .. bankStep): the sample, its bytes, where it is
    int   job; const u8* jd; int jlen; u32 jbit; int jframe; int jh[2][4]; u32 jcrc;
};
extern Bank BANK;
extern short BANK_PCM[BANK_CAP];
extern u8 BANK_STAGE_MEM[BANK_STAGE];

// the index; false (state -1) when it is not one
bool bankIndex (const u8* d, int len);
// sample i's coded bytes (kept by the caller until bankStep says it is done); then bankStep decodes up to maxValues
// 16-bit values of it (returns 1 when it is done or none is going, 0 while it works)
void bankFeed (int i, const u8* d, int len);
int  bankStep (int maxValues);
// the whole sample at once (the plugin, the tools)
bool bankDecodeNow (int i, const u8* d, int len);
// the sample a CAT / SMPL pair points at (-1: none, or not decoded yet)
int  bankAt (float catP, float smpP);
// the n samples nearest to i (i itself first): what FAMILLE draws from. sens: -1 shape (length, attack) .. 0 both ..
// +1 timbre (brightness, noise, lows, highs, pitch); same: from its category only. Returns how many (1 .. n)
int  bankNear (int i, float sens, bool same, int* out, int n);
// a 16-bit value of sample i at frame f, channel c (0 outside it)
inline float bankVal (const BankEntry& e, int f, int c)
{
    if (f < 0 || f >= e.frames) return 0.0f;
    return (float) BANK_PCM[e.off + f * e.ch + (c < e.ch ? c : 0)];
}

// the tools: a sample (16-bit, interleaved, ch 1 or 2) coded into out (cap bytes); its length, or -1
int  bankEncode (const short* pcm, int frames, int ch, u8* out, int cap);
u32  bankCrc (const short* pcm, int count);

} // namespace sk
