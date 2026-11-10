// IMPAKT — CHIMERA's sample bank: its index, the lossless codec (SKC), the pool, the neighbours (sk_bank.h)
#include "sk_bank.h"

namespace sk {

Bank BANK;
short BANK_PCM[BANK_CAP];
u8 BANK_STAGE_MEM[BANK_STAGE];

static inline int   mini_ (int a, int b) { return a < b ? a : b; }
static inline u32   rd32 (const u8* p) { return (u32) p[0] | ((u32) p[1] << 8) | ((u32) p[2] << 16) | ((u32) p[3] << 24); }
static inline float rdf (const u8* p) { const u32 u = rd32 (p); float f; __builtin_memcpy (&f, &u, 4); return f; }

u32 bankCrc (const short* pcm, int count)
{
    u32 h = 2166136261u;
    for (int i = 0; i < count; ++i) { h ^= (u32) (unsigned short) pcm[i]; h *= 16777619u; }
    return h;
}

// ------------------------------------------------------------------ the index
bool bankIndex (const u8* d, int len)
{
    Bank& b = BANK;
    for (int i = 0; i < BANK_MAX; ++i) b.e[i].ok = 0;
    b.n = 0; b.ncat = 0; b.used = 0; b.done = 0; b.failed = 0; b.job = -1; b.state = -1;
    if (len < 16 || d[0] != 'S' || d[1] != 'K' || d[2] != 'B' || d[3] != '1' || rd32 (d + 4) != 1) return false;
    const int n = (int) rd32 (d + 8), nc = (int) rd32 (d + 12);
    if (n < 0 || n > BANK_MAX || nc < 1 || nc > BANK_CATS) return false;
    int p = 16;
    for (int c = 0; c < nc; ++c)
    {
        if (p >= len) return false;
        const int l = d[p++];
        if (p + l > len) return false;
        int k = 0;
        for (; k < l && k < 15; ++k) b.catName[c][k] = (char) d[p + k];
        b.catName[c][k] = 0;
        p += l;
        b.catFirst[c] = 0; b.catCount[c] = 0;
    }
    enum { FIXED = 1 + 1 + 2 + 6 * 4 + 3 * 4 + BANK_FEAT * 4 + 2 * 4 };
    int off = 0;
    for (int i = 0; i < n; ++i)
    {
        if (p + FIXED > len) return false;
        BankEntry& e = b.e[i];
        e.cat = d[p]; e.ch = d[p + 1]; const int nl = d[p + 2] | (d[p + 3] << 8);
        e.frames = (int) rd32 (d + p + 4); e.sr = (float) rd32 (d + p + 8); e.crc = rd32 (d + p + 12);
        e.coded = (int) rd32 (d + p + 16); e.file = (int) rd32 (d + p + 20); e.foff = (int) rd32 (d + p + 24);
        e.gain = rdf (d + p + 28); e.pitch = rdf (d + p + 32); e.onset = rdf (d + p + 36);
        for (int f = 0; f < BANK_FEAT; ++f) e.feat[f] = rdf (d + p + 40 + 4 * f);
        e.mx = rdf (d + p + 40 + 4 * BANK_FEAT); e.my = rdf (d + p + 44 + 4 * BANK_FEAT);
        p += FIXED;
        if (p + nl > len || e.cat >= nc || (e.ch != 1 && e.ch != 2) || e.frames < 1 || e.sr < 8000.0f) return false;
        int k = 0;
        for (; k < nl && k < BANK_NAME - 1; ++k) e.name[k] = (char) d[p + k];
        e.name[k] = 0;
        p += nl + 64;                                                  // (its outline: the page draws it)
        if (p > len) return false;
        if (i > 0 && e.cat < b.e[i - 1].cat) return false;          // (stored by category)
        if (b.catCount[e.cat]++ == 0) b.catFirst[e.cat] = i;
        // its place in the pool (none when the pool is full: it stays silent)
        const int need = e.frames * e.ch;
        if (off + need <= BANK_CAP) { e.off = off; off += need; }
        else { e.off = -1; b.failed++; }
    }
    b.n = n; b.ncat = nc; b.used = off;
    bankPublish(); b.state = 1;
    return true;
}

// ------------------------------------------------------------------ the codec
// a stream of bits, the first in the top of each byte
struct BitR
{
    const u8* d; int len; u32 pos;
    inline u32 peek32() const
    {
        const int i = (int) (pos >> 3), s = (int) (pos & 7);
        unsigned long long w = 0;
        for (int k = 0; k < 5; ++k) w = (w << 8) | (unsigned long long) (i + k < len ? d[i + k] : 0);    // (40 bits from the byte)
        return (u32) ((w >> (8 - s)) & 0xFFFFFFFFull);
    }
    inline u32 get (int n) { if (n == 0) return 0; const u32 v = peek32() >> (32 - n); pos += (u32) n; return v; }
    inline u32 unary()
    {
        u32 q = 0;
        for (;;)
        {
            const u32 w = peek32();
            if (w) { const int z = __builtin_clz (w); pos += (u32) z + 1; return q + (u32) z; }
            q += 32; pos += 32;
            if ((int) (pos >> 3) > len) return q;      // (a broken stream: the crc refuses it)
        }
    }
};
struct BitW
{
    u8* d; int cap, len; unsigned long long acc; int nb; bool over;
    void put (u32 v, int n)
    {
        for (int k = n - 1; k >= 0; --k)
        {
            acc = (acc << 1) | ((v >> k) & 1u); ++nb;
            if (nb == 8) { if (len < cap) d[len++] = (u8) acc; else over = true; acc = 0; nb = 0; }
        }
    }
    void zeros (u32 q) { while (q >= 16) { put (0, 16); q -= 16; } put (0, (int) q); }
    void flush() { if (nb) put (0, 8 - nb); for (int k = 0; k < 8; ++k) put (0, 8); }   // (and 8 bytes of zeros: the reader looks ahead)
};
static inline int predict (const int* h, int order)
{
    switch (order)
    {
        case 1:  return h[0];
        case 2:  return 2 * h[0] - h[1];
        case 3:  return 3 * h[0] - 3 * h[1] + h[2];
        case 4:  return 4 * h[0] - 6 * h[1] + 4 * h[2] - h[3];
        default: return 0;
    }
}
static inline void push (int* h, int x) { h[3] = h[2]; h[2] = h[1]; h[1] = h[0]; h[0] = x; }

static int jtmp[2][BANK_BLOCK];

// one block of the job (BANK_BLOCK frames, or what is left)
static void decodeBlock()
{
    Bank& b = BANK;
    BankEntry& e = b.e[b.job];
    BitR r { b.jd, b.jlen, b.jbit };
    const int nf = mini_ (BANK_BLOCK, e.frames - b.jframe);
    for (int c = 0; c < e.ch; ++c)
    {
        int* out = jtmp[c];
        int* h = b.jh[c];
        const int order = (int) r.get (3);
        for (int p0 = 0; p0 < nf; p0 += BANK_PART)
        {
            const int pn = mini_ (BANK_PART, nf - p0);
            const int k = (int) r.get (5);
            if (k == 31)
            {
                const int w = (int) r.get (5) + 1;
                for (int j = 0; j < pn; ++j)
                {
                    const u32 u = r.get (w);
                    const int res = (int) (u << (32 - w)) >> (32 - w);
                    const int x = res + predict (h, order);
                    push (h, x); out[p0 + j] = x;
                }
            }
            else
                for (int j = 0; j < pn; ++j)
                {
                    const u32 q = r.unary(), u = (q << k) | r.get (k);
                    const int res = (int) (u >> 1) ^ -(int) (u & 1u);
                    const int x = res + predict (h, order);
                    push (h, x); out[p0 + j] = x;
                }
        }
    }
    short* dst = BANK_PCM + e.off + b.jframe * e.ch;
    if (e.ch == 2)
        for (int f = 0; f < nf; ++f)
        {
            const int side = jtmp[1][f], mid = (jtmp[0][f] << 1) | (side & 1);
            const int L = (mid + side) >> 1, R = (mid - side) >> 1;
            dst[2 * f] = (short) L; dst[2 * f + 1] = (short) R;
        }
    else for (int f = 0; f < nf; ++f) dst[f] = (short) jtmp[0][f];
    for (int f = 0; f < nf * e.ch; ++f) { b.jcrc ^= (u32) (unsigned short) dst[f]; b.jcrc *= 16777619u; }
    b.jframe += nf; b.jbit = r.pos;
}

void bankFeed (int i, const u8* d, int len)
{
    Bank& b = BANK;
    b.job = -1;
    if (b.state < 1 || i < 0 || i >= b.n || b.e[i].off < 0) { b.failed++; return; }
    b.e[i].ok = 0;
    b.job = i; b.jd = d; b.jlen = len; b.jbit = 0; b.jframe = 0; b.jcrc = 2166136261u;
    for (int c = 0; c < 2; ++c) for (int k = 0; k < 4; ++k) b.jh[c][k] = 0;
}

int bankStep (int maxValues)
{
    Bank& b = BANK;
    if (b.job < 0) return 1;
    BankEntry& e = b.e[b.job];
    int left = maxValues;
    while (b.jframe < e.frames && left > 0) { decodeBlock(); left -= BANK_BLOCK * e.ch; }
    if (b.jframe < e.frames) return 0;
    if (b.jcrc == e.crc) { bankPublish(); e.ok = 1; b.done++; }
    else b.failed++;
    b.job = -1;
    if (b.done + b.failed >= b.n) b.state = 2;
    return 1;
}

bool bankDecodeNow (int i, const u8* d, int len)
{
    bankFeed (i, d, len);
    while (! bankStep (1 << 30)) {}
    return i >= 0 && i < BANK.n && BANK.e[i].ok;
}

// the encoder (the tools): per block and channel the predictor that costs least, per partition the Rice parameter
static int riceCost (const u32* u, int n, int& kBest)
{
    // (around the parameter the mean asks for)
    unsigned long long sum = 0;
    for (int j = 0; j < n; ++j) sum += u[j];
    int k0 = 0; while (k0 < 24 && ((unsigned long long) n << (k0 + 1)) <= sum) ++k0;
    long long best = -1; kBest = 0;
    for (int k = k0 > 1 ? k0 - 1 : 0; k <= k0 + 1 && k < 25; ++k)
    {
        long long c = 0;
        for (int j = 0; j < n; ++j) c += (long long) (u[j] >> k) + 1 + k;
        if (best < 0 || c < best) { best = c; kBest = k; }
    }
    return (int) (best < (1ll << 30) ? best : (1ll << 30));
}
int bankEncode (const short* pcm, int frames, int ch, u8* out, int cap)
{
    if (ch != 1 && ch != 2) return -1;
    static int sig[2][BANK_BLOCK], res[5][BANK_BLOCK];
    static u32 zz[BANK_BLOCK];
    BitW w { out, cap, 0, 0, 0, false };
    int h[2][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
    for (int f0 = 0; f0 < frames; f0 += BANK_BLOCK)
    {
        const int nf = mini_ (BANK_BLOCK, frames - f0);
        for (int f = 0; f < nf; ++f)
        {
            if (ch == 2) { const int L = pcm[2 * (f0 + f)], R = pcm[2 * (f0 + f) + 1]; sig[0][f] = (L + R) >> 1; sig[1][f] = L - R; }
            else sig[0][f] = pcm[f0 + f];
        }
        for (int c = 0; c < ch; ++c)
        {
            // the residuals of each order, from the history of the samples before
            int bestO = 0; long long bestC = -1;
            for (int o = 0; o <= 4; ++o)
            {
                int hh[4] = { h[c][0], h[c][1], h[c][2], h[c][3] };
                for (int f = 0; f < nf; ++f) { res[o][f] = sig[c][f] - predict (hh, o); push (hh, sig[c][f]); }
                long long cost = 3;
                for (int p0 = 0; p0 < nf; p0 += BANK_PART)
                {
                    const int pn = mini_ (BANK_PART, nf - p0);
                    for (int j = 0; j < pn; ++j) { const int r = res[o][p0 + j]; zz[j] = ((u32) r << 1) ^ (u32) (r >> 31); }
                    int k; cost += 5 + riceCost (zz, pn, k);
                }
                if (bestC < 0 || cost < bestC) { bestC = cost; bestO = o; }
            }
            w.put ((u32) bestO, 3);
            for (int p0 = 0; p0 < nf; p0 += BANK_PART)
            {
                const int pn = mini_ (BANK_PART, nf - p0);
                int mx = 0;
                for (int j = 0; j < pn; ++j) { const int r = res[bestO][p0 + j]; zz[j] = ((u32) r << 1) ^ (u32) (r >> 31); mx = r < 0 ? (-r > mx ? -r : mx) : (r > mx ? r : mx); }
                int k; const int cost = riceCost (zz, pn, k);
                int bits = 1; while (bits < 24 && (1 << (bits - 1)) <= mx) ++bits;       // (a raw value: its sign and magnitude)
                if (cost > (bits) * pn + 5)
                {
                    w.put (31, 5); w.put ((u32) (bits - 1), 5);
                    for (int j = 0; j < pn; ++j) w.put ((u32) res[bestO][p0 + j] & ((1u << bits) - 1u), bits);
                }
                else
                {
                    w.put ((u32) k, 5);
                    for (int j = 0; j < pn; ++j) { w.zeros (zz[j] >> k); w.put (1, 1); w.put (zz[j] & ((1u << k) - 1u), k); }
                }
            }
            for (int f = 0; f < nf; ++f) push (h[c], sig[c][f]);
        }
    }
    w.flush();
    return w.over ? -1 : w.len;
}

// ------------------------------------------------------------------ CAT / SMPL, the neighbours
int bankAt (float catP, float smpP)
{
    const Bank& b = BANK;
    if (b.state < 1) return -1;
    bankAcquire();
    if (b.ncat < 1) return -1;
    const int c = idxOf (catP, b.ncat), n = b.catCount[c];
    if (n < 1) return -1;
    const int i = b.catFirst[c] + idxOf (smpP, n);
    const int ok = b.e[i].ok;
    bankAcquire();
    return ok ? i : -1;
}

int bankNear (int i, float sens, bool same, int* out, int n)
{
    const Bank& b = BANK;
    if (i < 0 || i >= b.n || n < 1) return 0;
    out[0] = i;
    if (n == 1) return 1;
    // the weights: shape (length, attack) against timbre (brightness, noise, lows, highs, pitchedness); width a little
    const float sh = sens > 0.0f ? 1.0f - 0.85f * sens : 1.0f, tb = sens < 0.0f ? 1.0f + 0.85f * sens : 1.0f;
    const float W[BANK_FEAT] = { sh, sh, tb, tb, tb, tb, tb, 0.3f };
    enum { M = 16 };
    float bd[M]; int bi[M]; int k = 0;
    const int want = mini_ (n - 1, M);
    for (int j = 0; j < b.n; ++j)
    {
        if (j == i || ! b.e[j].ok || (same && b.e[j].cat != b.e[i].cat)) continue;
        float dd = 0.0f;
        for (int f = 0; f < BANK_FEAT; ++f) { const float x = b.e[j].feat[f] - b.e[i].feat[f]; dd += W[f] * x * x; }
        if (! same && b.e[j].cat != b.e[i].cat) dd += 1.0f;              // (another category: a step further)
        // kept in order, the nearest first
        if (k < want) { int q = k++; while (q > 0 && bd[q - 1] > dd) { bd[q] = bd[q - 1]; bi[q] = bi[q - 1]; --q; } bd[q] = dd; bi[q] = j; }
        else if (dd < bd[k - 1]) { int q = k - 1; while (q > 0 && bd[q - 1] > dd) { bd[q] = bd[q - 1]; bi[q] = bi[q - 1]; --q; } bd[q] = dd; bi[q] = j; }
    }
    bankAcquire();                                      // (the neighbours' sounds are read after their ok: sk_bank.h)
    for (int q = 0; q < k; ++q) out[1 + q] = bi[q];
    return 1 + k;
}

} // namespace sk
