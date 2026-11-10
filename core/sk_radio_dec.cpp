// IMPAKT — RADIO (v87): the hosts' decoder (sk_radio_dec.h). Own implementation, but the MP3 decoder itself: minimp3
// (lieff, CC0 1.0: core/ext/minimp3.h).
#include "sk_radio_dec.h"
#include "sk_radio.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#if defined(__wasm__)
#define MINIMP3_NO_SIMD
#endif
#include "ext/minimp3.h"

namespace sk {

static_assert (sizeof (mp3dec_t) <= sizeof (((RadioDec*) nullptr)->mp3st), "minimp3's state fits");

namespace {
// the IMA ADPCM tables (the standard's)
const short IMA_STEP[89] = { 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
    4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385,
    24623, 27086, 29794, 32767 };
const signed char IMA_IDX[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };
constexpr float PI_F = 3.14159265358979f;
}

void RadioDec::reset()
{
    // the kernel: a sinc under a Blackman window, NZ zero crossings each side, RES points between two
    for (int k = 0; k <= NZ * RES + 1; ++k)
    {
        const float u = (float) k / (float) RES;
        const float x = minf (u / (float) NZ, 1.0f);
        const float w = 0.42f + 0.5f * cos2pi (0.5f * x) + 0.08f * cos2pi (x);
        const float s = k == 0 ? 1.0f : sin2pi (0.5f * u) / (PI_F * u);
        proto[k] = u >= (float) NZ ? 0.0f : s * w;
    }
    gain = 0.9f;
    restart();
}

void RadioDec::restart()
{
    mp3dec_init ((mp3dec_t*) mp3st);
    inN = 0; outN = 0;
    for (float& r : ring) r = 0.0f;
    srcN = 0; T = 0.0; srcRate = 0.0f; step = 1.0; c = 0.9f; D = (float) NZ / c;
    adIdx = 0; adPrev = 0;
    rate = 0; chans = 0; kbps = 0; frames = 0; skipped = 0;
}

void RadioDec::setRate (float r)
{
    if (r == srcRate) return;
    srcRate = r;
    step = (double) r / (double) RADIO_RATE;
    c = 0.9f * minf (1.0f, (float) RADIO_RATE / r);
    D = (float) NZ / c;
    T = (double) srcN;                               // (a new rate: its first output from here)
}

// a source sample in; every output it completes out
void RadioDec::put (float x)
{
    ring[srcN & (RING - 1)] = x;
    ++srcN;
    // (both kept under 2^30, moved back together by a multiple of the ring: the plain-JS build has no 64-bit numbers)
    if (srcN >= (1 << 30)) { srcN -= 1 << 29; T -= (double) (1 << 29); }
    while (T + (double) D < (double) srcN && outN < OUT_CAP)
    {
        const int j0 = (int) __builtin_floor (T - (double) D) + 1, j1 = (int) __builtin_floor (T + (double) D);
        float y = 0.0f;
        for (int j = j0; j <= j1; ++j)
        {
            const float u = absf ((float) (T - (double) j)) * c * (float) RES;
            const int i = (int) u;
            if (i >= NZ * RES) continue;
            const float f = u - (float) i, h = proto[i] + (proto[i + 1] - proto[i]) * f;
            y += (j >= 0 ? ring[j & (RING - 1)] : 0.0f) * h;
        }
        y *= c * gain;
        out[outN++] = (short) (y > 32767.0f ? 32767 : y < -32767.0f ? -32767 : (int) (y + (y >= 0.0f ? 0.5f : -0.5f)));
        T += step;
    }
}

void RadioDec::pcmFeed (const short* x, int n, int ch, float r)
{
    if (n <= 0 || ch <= 0 || r < 4000.0f) return;
    setRate (r);
    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;
        for (int k = 0; k < ch; ++k) s += (float) x[i * ch + k];
        put (s / (float) ch);
    }
}

void RadioDec::adpcmFeed (const unsigned char* b, int n, float r)
{
    if (n <= 0 || r < 4000.0f) return;
    setRate (r);
    for (int i = 0; i < n; ++i)
        for (int h = 0; h < 2; ++h)
        {
            const int code = h ? b[i] >> 4 : b[i] & 15;
            const int stp = IMA_STEP[adIdx];
            adIdx += IMA_IDX[code]; adIdx = adIdx < 0 ? 0 : adIdx > 88 ? 88 : adIdx;
            int d = stp >> 3;
            if (code & 1) d += stp >> 2;
            if (code & 2) d += stp >> 1;
            if (code & 4) d += stp;
            if (code & 8) d = -d;
            adPrev += d; adPrev = adPrev < -32768 ? -32768 : adPrev > 32767 ? 32767 : adPrev;
            put ((float) adPrev);
        }
}

void RadioDec::mp3Feed (const unsigned char* b, int n)
{
    if (n > inRoom()) n = inRoom();
    for (int i = 0; i < n; ++i) in[inN + i] = b[i];
    inN += n;
    while (mp3Work()) {}
}

bool RadioDec::mp3Work()
{
    // (a frame makes at most 1152 samples a channel: as many as 3456 outputs from 8 kHz). The decoder wants the next
    // frame's header in hand too (else it takes the frame for noise and looks again): 4 KB at least (two of the
    // largest frames), 16 KB to find the first one
    if (inN < (frames ? 4096 : 16384) || outRoom() < 4096) return false;
    mp3dec_frame_info_t info;
    short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    const int ns = mp3dec_decode_frame ((mp3dec_t*) mp3st, in, inN, pcm, &info);
    int used = info.frame_bytes;
    if (used <= 0)
    {
        // nothing found in what is in hand: wait for more, unless it is full of something else (half of it dropped)
        if (inN < IN_CAP) return false;
        used = IN_CAP / 2; skipped += (u32) used;
    }
    if (used > inN) used = inN;
    for (int i = used; i < inN; ++i) in[i - used] = in[i];
    inN -= used;
    if (ns > 0 && info.hz >= 8000 && info.channels > 0)
    {
        rate = info.hz; chans = info.channels; kbps = info.bitrate_kbps; ++frames;
        pcmFeed (pcm, ns, info.channels, (float) info.hz);
    }
    else if (ns == 0) skipped += (u32) used;
    return true;
}

int RadioDec::take (short* dst, int max)
{
    const int n = outN < max ? outN : max;
    for (int i = 0; i < n; ++i) dst[i] = out[i];
    for (int i = n; i < outN; ++i) out[i - n] = out[i];
    outN -= n;
    return n;
}

} // namespace sk
