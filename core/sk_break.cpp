// IMPAKT — BREAK, the break slicer (see sk_break.h): the slots, the player
#include "sk_engine.h"

namespace sk {

namespace {

inline u32 hash32 (u32 x) { x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16; return x; }
inline double floord_ (double x) { return __builtin_floor (x); }
inline double absd_ (double x) { return x < 0.0 ? -x : x; }
inline double minf_d (double a, double b) { return a < b ? a : b; }

const float BRK_LEN16[BRK_LENS] = { 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 8.0f, 16.0f, 0.0f };   // 1/32 .. 1 bar, FREE
const int   BRK_PJ[6] = { -12, -7, -5, 5, 7, 12 };

// a slice's class (what it starts with): 1 a kick, 2 a snare, 0 anything else
inline int sliceClass (const BrkInfo& b, int i) { return b.cls[i]; }

// a Butterworth section on the bilinear transform: low-pass or high-pass at fc, quality q
void biquad (float* c, float fc, float q, bool hp)
{
    const float K = tanpi (fc / (float) BRK_RATE), K2 = K * K, nrm = 1.0f / (1.0f + K / q + K2);
    if (hp) { c[0] = nrm; c[1] = -2.0f * nrm; c[2] = nrm; }
    else    { c[0] = K2 * nrm; c[1] = 2.0f * c[0]; c[2] = c[0]; }
    c[3] = 2.0f * (K2 - 1.0f) * nrm; c[4] = (1.0f - K / q + K2) * nrm;
}
inline float biquadRun (const float* c, float* z, float x)
{
    const float y = c[0] * x + z[0];
    z[0] = c[1] * x - c[3] * y + z[1];
    z[1] = c[2] * x - c[4] * y;
    return y;
}
// the k-th smallest of a[0..n) (a is reordered)
float kth (float* a, int n, int k)
{
    int lo = 0, hi = n - 1;
    while (lo < hi)
    {
        const float pv = a[(lo + hi) >> 1];
        int i = lo, j = hi;
        while (i <= j)
        {
            while (a[i] < pv) ++i;
            while (a[j] > pv) --j;
            if (i <= j) { const float t = a[i]; a[i] = a[j]; a[j] = t; ++i; --j; }
        }
        if (k <= j) hi = j; else if (k >= i) lo = i; else break;
    }
    return a[k];
}

} // namespace

void BreakBank::reset()
{
    ver = 1; job = -1; jpos = jhop = jacc = 0;
    for (int s = 0; s < BRK_SLOTS; ++s) { state[s] = 0; len[s] = 0; bars[s] = 0; b[s].len = 0; b[s].L16 = 16; b[s].bars = 1; b[s].bpm = 120.0f; }
}

int BreakBank::resolve (int s) const
{
    for (int k = 0; k < BRK_SLOTS; ++k) { const int q = (s + k) % BRK_SLOTS; if (state[q] == 2) return q; }
    return -1;
}

// the slots waiting are read in turn: the three bands' energy every 64 samples, then (finish) the slices
int BreakBank::work (int budget)
{
    int done = 0;
    while (budget > 0)
    {
        if (job < 0)
        {
            for (int s = 0; s < BRK_SLOTS && job < 0; ++s) if (state[s] == 1) job = s;
            if (job < 0) break;
            jpos = jhop = jacc = 0;
            for (int i = 0; i < 6; ++i) z[i][0] = z[i][1] = 0.0f;
            for (int i = 0; i < 3; ++i) acc[i] = 0.0f;
            biquad (c[0], 150.0f, 0.5412f, false);  biquad (c[1], 150.0f, 1.3066f, false);    // the kick's band
            biquad (c[2], 200.0f, 0.7071f, true);   biquad (c[3], 2500.0f, 0.7071f, false);   // the snare's
            biquad (c[4], 5000.0f, 0.5412f, true);  biquad (c[5], 5000.0f, 1.3066f, true);    // the cymbals'
        }
        const short* x = mem (job);
        const int n = len[job], end = mini (n, jpos + budget);
        budget -= end - jpos;
        for (int i = jpos; i < end; ++i)
        {
            const float v = (float) x[i] * (1.0f / 32768.0f);
            for (int k = 0; k < 3; ++k)
            {
                const float y = biquadRun (c[2 * k + 1], z[2 * k + 1], biquadRun (c[2 * k], z[2 * k], v));
                acc[k] += y * y;
            }
            if (++jacc == BRK_HOP)
            {
                for (int k = 0; k < 3; ++k) { env[k][jhop] = acc[k] * (1.0f / (float) BRK_HOP); acc[k] = 0.0f; }
                jacc = 0; ++jhop;
            }
        }
        jpos = end;
        if (jpos >= n)
        {
            if (jacc > 0) { for (int k = 0; k < 3; ++k) env[k][jhop] = acc[k] / (float) jacc; ++jhop; }
            finish (job);
            state[job] = 2; ++ver; ++done;
            job = -1;
        }
    }
    return done;
}

void BreakBank::finish (int s)
{
    BrkInfo& B = b[s];
    const short* x = mem (s);
    const int n = len[s], H = jhop;
    B.len = n; B.bars = bars[s];
    B.L16 = 16 * B.bars;
    B.bpm = (float) ((double) B.bars * 240.0 * (double) BRK_RATE / (double) n);
    // each band's mean energy over the loop: a hit counts when it brings a fair share of it (a hat's mids are no snare)
    float mean[3];
    for (int k = 0; k < 3; ++k) { double m = 0.0; for (int h = 0; h < H; ++h) m += (double) env[k][h]; mean[k] = (float) (m / (double) maxi (H, 1)); }
    // where something starts: the rise of the log energy in the three bands, summed (a hit in any band shows); a band
    // is not heard below a fiftieth of its mean (its noise does not rise)
    const float L10 = 0.30103f;
    for (int h = 0; h < H; ++h)
    {
        float o = 0.0f;
        for (int k = 0; k < 3; ++k)
        {
            // (against the hops before: a tail's flutter does not rise; the kick's band over longer windows, its
            // cycles are longer than a hop)
            const float fl = 0.02f * mean[k] + 1e-9f;
            const int nA = k ? 1 : 3, nB = k ? 3 : 5;
            float now = 0.0f, pr = 0.0f; int na = 0, np = 0;
            for (int j = h; j < mini (h + nA, H); ++j) { now += env[k][j]; ++na; }
            for (int j = maxi (h - nB, 0); j < h; ++j) { pr += env[k][j]; ++np; }
            const float e1 = log2f_ (now / (float) maxi (na, 1) + fl), e0 = np ? log2f_ (pr / (float) np + fl) : e1;
            o += maxf (e1 - e0, 0.0f) * L10;
        }
        onv[h] = o; tmp[h] = o;
    }
    // (a hit: a rise above most of the loop's, and of 2 dB at least)
    const float thr = maxf (H > 4 ? kth (tmp, H, (H * 3) / 4) : 0.0f, 0.2f);
    // the slices: on the grid, each moved onto the attack nearest to it (within a third of a 16th, a breath before it)
    const int L = B.L16;
    const double step = (double) n / (double) L;
    const int pre = (int) (0.0015 * BRK_RATE), W = (int) (0.03 * BRK_RATE / BRK_HOP + 0.5);
    float smax = 1e-9f, strength[BRK_MAXLEN];
    for (int k = 0; k < L; ++k)
    {
        const double g0 = (double) k * step;
        int a = (int) g0;
        if (k > 0)
        {
            int lo = (int) ((g0 - step / 3.0) / BRK_HOP), hi = (int) ((g0 + step / 3.0) / BRK_HOP);
            lo = maxi (lo, 1); hi = mini (hi, H - 1);
            // (the nearer the grid, the likelier: a ghost note on its 16th beats a tail's flutter further away)
            int am = -1; float mx = -1.0f;
            const double win = step / 3.0;
            for (int h = lo; h < hi; ++h)
            {
                const float near = 1.0f - 0.3f * (float) (absd_ ((double) (h * BRK_HOP) - g0) / win);
                if (onv[h] > thr && onv[h] * near > mx) { mx = onv[h] * near; am = h; }
            }
            if (am >= 0) a = maxi (am * BRK_HOP - pre, 0);
            if (a <= B.onset[k - 1] + (int) (step / 4.0)) a = (int) g0;
        }
        a = mini (maxi (a, k ? B.onset[k - 1] + 1 : 0), n - 1);
        B.onset[k] = a;
        // what it starts with: the band whose energy rises the most at its attack (30 ms after against 30 ms before):
        // a kick (below 150 Hz), a snare (the mids and the highs together), anything else
        const int ha = a / BRK_HOP;
        float rise[3], after[3];
        for (int q = 0; q < 3; ++q)
        {
            float af = 0.0f, bf = 0.0f; int na = 0, nb = 0;
            for (int h = ha; h < mini (ha + W, H); ++h) { af += env[q][h]; ++na; }
            for (int h = maxi (ha - W, 0); h < ha; ++h) { bf += env[q][h]; ++nb; }
            af = na ? af / (float) na : 0.0f;
            bf = nb >= 4 ? bf / (float) nb : 1e-9f;
            rise[q] = af / (bf + 1e-9f); after[q] = af;
        }
        const bool kick = rise[0] > 3.0f && rise[0] >= rise[1] && after[0] > 0.7f * mean[0];
        const bool snare = rise[1] > 2.5f && rise[2] > 2.0f && after[1] > 0.7f * mean[1];
        B.cls[k] = kick ? 1 : (snare ? 2 : 0);
        double e = 0.0; int ne = 0;
        for (int i = a; i < mini (a + 960, n); ++i) { const double v = (double) x[i]; e += v * v; ++ne; }
        strength[k] = ne ? (float) __builtin_sqrt (e / (double) ne) : 0.0f;
        smax = maxf (smax, strength[k]);
    }
    for (int k = 0; k < L; ++k) B.str[k] = (unsigned char) (255.0f * strength[k] / smax + 0.5f);
    for (int k = L; k < BRK_MAXLEN; ++k) { B.onset[k] = n - 1; B.cls[k] = 0; B.str[k] = 0; }
    // the waveform
    for (int i = 0; i < BRK_NPEAK; ++i)
    {
        int m = 0;
        for (int j = (int) ((long long) i * n / BRK_NPEAK); j < (int) ((long long) (i + 1) * n / BRK_NPEAK); ++j) { const int v = x[j] < 0 ? -x[j] : x[j]; if (v > m) m = v; }
        B.peak[i] = (unsigned char) (255.0f * minf (1.0f, (float) m / (0.95f * 32767.0f)) + 0.5f);
    }
}

// the voices that play slot s stop (it changes, or it goes)
static void stopSlot (Engine& e, int s)
{
    for (int t = 0; t < NUM_TRACKS; ++t)
        for (int k = 0; k < 2; ++k)
        {
            BreakState& b = e.tr[t].voice[k].bs;
            bool hit = b.on && b.brk == s;
            for (int j = 0; j < BRK_SEGS; ++j) if (b.s[j].on && b.s[j].brk == s) hit = true;
            if (hit) { b.on = 0; for (int j = 0; j < BRK_SEGS; ++j) b.s[j].on = 0; }
        }
}

void Engine::breakSet (int s, int n, int bars)
{
    if (s < 0 || s >= BRK_SLOTS) return;
    stopSlot (*this, s);
    if (brk.job == s) brk.job = -1;
    if (n < BRK_RATE / 10 || n > BRK_CAP || bars < 1 || bars > BRK_MAXBARS) { brk.state[s] = 0; brk.len[s] = 0; ++brk.ver; return; }
    brk.len[s] = n; brk.bars[s] = bars; brk.state[s] = 1; ++brk.ver;
}

void Engine::breakClear (int s)
{
    if (s < 0 || s >= BRK_SLOTS) return;
    stopSlot (*this, s);
    if (brk.job == s) brk.job = -1;
    brk.state[s] = 0; brk.len[s] = 0; ++brk.ver;
}

void Engine::breakBars (int s, int bars)
{
    if (s < 0 || s >= BRK_SLOTS || brk.state[s] == 0) return;
    breakSet (s, brk.len[s], bars);
}

void Engine::breakWork (int budget) { brk.work (budget); }

void Engine::breakSlots (float* out) const
{
    out[0] = (float) brk.ver;
    for (int s = 0; s < BRK_SLOTS; ++s)
    {
        float* o = out + 1 + s * BRK_SLOT_INFO;
        o[0] = (float) brk.state[s];
        o[1] = brk.state[s] == 2 ? brk.b[s].bpm : (brk.len[s] > 0 ? (float) ((double) brk.bars[s] * 240.0 * BRK_RATE / brk.len[s]) : 0.0f);
        o[2] = (float) brk.bars[s];
        o[3] = (float) brk.len[s] / (float) BRK_RATE;
    }
}

void Engine::breakPeaks (int s, float* out) const
{
    for (int i = 0; i < BRK_NPEAK; ++i) out[i] = brk.ready (s) ? (float) brk.b[s].peak[i] * (1.0f / 255.0f) : 0.0f;
}

// where 16th q of the break begins, counted on and on through the loop (pool samples from its start)
static inline double brkU (const BrkInfo& B, int q)
{
    const int L = B.L16, m = q >= 0 ? q / L : -((-q + L - 1) / L), a = q - m * L;
    return (double) B.onset[a] + (double) m * (double) B.len;
}

// a piece of the break starts: what sounds is cut in 3 ms, unless it already reads there (the break goes on)
static void breakPiece (BreakState& s, const BreakBank& bank, int bi, double start, double stop, int dir, int loop, float fadeSec, float sr)
{
    const BrkInfo& B = bank.b[bi];
    const double tol = 0.002 * (double) BRK_RATE;
    const double len = (double) B.len;
    for (int j = 0; j < BRK_SEGS; ++j)
    {
        BrkSeg& g = s.s[j];
        if (! g.on || g.gInc < 0.0f || g.brk != bi || g.dir != dir || dir < 0 || loop || g.loop) continue;
        // (forward pieces read round the loop: compare where they are in it)
        double d = start - (g.pos - floord_ (g.pos / len) * len);
        if (d > 0.5 * len) d -= len;
        if (d < -0.5 * len) d += len;
        if (absd_ (d) < tol) { g.stop = g.pos + d + (stop - start); return; }
    }
    for (int j = 0; j < BRK_SEGS; ++j) if (s.s[j].on && s.s[j].gInc >= 0.0f) s.s[j].gInc = -1.0f / (0.003f * sr);
    int k = 0;
    for (int j = 0; j < BRK_SEGS; ++j) { if (! s.s[j].on) { k = j; break; } if (s.s[j].g < s.s[k].g) k = j; }
    BrkSeg& g = s.s[k];
    g.on = 1; g.brk = bi; g.dir = dir; g.loop = loop; g.pos = start; g.stop = stop;
    g.g = fadeSec > 0.0f ? 0.0f : 1.0f; g.gInc = fadeSec > 0.0f ? 1.0f / (fadeSec * sr) : 0.0f;
}

void Engine::breakTrigger (Voice& v, const float* P)
{
    BreakState& s = v.bs;
    const Track& t = tr[v.ti];
    const int own = brk.resolve (idxOf (P[P_A], BRK_SLOTS));
    if (own < 0) { s.on = 0; return; }
    const int sc = idxOf (P[P_C], BRK_MAXLEN + 1), li = idxOf (P[P_D], BRK_LENS);
    const bool stepped = v.step >= 0 && running;
    const int k = stepped ? t.bAbs : 0;
    // the draws, always the same number in the same order (a knob only decides what they do); SEED: from the step in
    // the loop (the same chops on every pass of it)
    const int sd = idxOf (P[P_BSED], BRK_SEEDS), Lo = brk.b[own].L16;
    Rng r;
    if (sd == 0) r.seed (v.rng.next() | 1u);
    else r.seed (hash32 ((u32) sd * 0x9E3779B1u ^ (u32) (((k % Lo) + Lo) % Lo) * 0x85EBCA6Bu ^ (u32) own * 0xC2B2AE35u) | 1u);
    float u[12]; for (int i = 0; i < 12; ++i) u[i] = r.uni();
    int flags = 0, bi = own;
    if (u[10] < P[P_BSWP])
    {
        // SWAP: the same 16th of another slot (the bar goes on, in another loop)
        int cand[BRK_SLOTS], nc = 0;
        for (int q = 0; q < BRK_SLOTS; ++q) if (q != own && brk.ready (q)) cand[nc++] = q;
        if (nc > 0) { bi = cand[mini ((int) (u[11] * (float) nc), nc - 1)]; flags |= 32; }
    }
    const BrkInfo& B = brk.b[bi];
    const int L = B.L16;
    int slice = (sc == 0 ? k : sc - 1) + (int) v.note;
    slice = ((slice % L) + L) % L;
    if (u[0] < P[P_BRND])
    {
        // another slice, three times in four one that starts like its own (a kick for a kick, a snare for a snare)
        const int cls = sliceClass (B, slice);
        int cand[BRK_MAXLEN], nc = 0;
        if (u[1] < 0.75f) for (int i = 0; i < L; ++i) if (sliceClass (B, i) == cls) cand[nc++] = i;
        slice = nc > 0 ? cand[mini ((int) (u[2] * (float) nc), nc - 1)] : mini ((int) (u[2] * (float) L), L - 1);
        flags |= 1;
    }
    const float len16 = BRK_LEN16[li];
    const int skip = u[9] < P[P_BSKP] ? 1 : 0;
    int dir = 1; float rlen = 0.0f;
    if (! skip && u[3] < P[P_BREV]) { dir = -1; rlen = len16 > 0.0f ? len16 : 2.0f; flags |= 2; }
    int stN = 1; float stL = 0.0f;
    if (! skip && dir > 0 && u[4] < P[P_BSTU]) { stN = 2 + mini ((int) (u[5] * 3.0f), 2); stL = u[6] < 0.6f ? 0.5f : 0.25f; flags |= 4; }
    int pj = 0;
    if (u[7] < P[P_BPJM]) { pj = BRK_PJ[mini ((int) (u[8] * 6.0f), 5)]; flags |= 8; }
    if (skip) flags |= 16;
    s.flags = flags; s.slice = slice;

    const double Tstep = samplesPerStep (t);
    const float gate = len16 > 0.0f ? (float) ((double) len16 * Tstep) + 0.002f * sr
                     : (stepped ? 1e12f : (float) ((double) L * Tstep));       // FREE: on and on (one pass when stopped)
    // the same loop, going on from here (the step's own slice, nothing changed): the cursor carries on
    if (s.on && s.rel >= 1.0f && s.brk == bi && s.dir > 0 && s.stutN == 1 && s.pj == pj && ! s.skip && dir > 0 && stN == 1 && ! skip)
    {
        const double bc = s.b0 + s.tau;
        double d = bc - floord_ (bc / (double) L) * (double) L - (double) slice;
        if (d > 0.5 * (double) L) d -= (double) L;
        if (d < -0.5 * (double) L) d += (double) L;
        if (d >= -0.12 && d <= 0.5) { s.envT = 1.0f; s.gate = gate; return; }
    }
    // a new start: what sounds is cut (the next piece does it), the cursor on the slice
    for (int j = 0; j < BRK_SEGS; ++j) if (s.s[j].on && s.s[j].gInc >= 0.0f) s.s[j].gInc = -1.0f / (0.003f * sr);
    s.on = 1; s.brk = bi; s.dir = dir; s.stutN = stN; s.stutLen = stL; s.rlen = rlen; s.pj = pj; s.skip = skip;
    s.b0 = (double) slice; s.tau = 0.0; s.k0 = k;
    s.ph = stN > 1 ? 0 : (dir < 0 ? 1 : 2); s.pi = 0; s.next = 0.0;
    s.f0 = (stN > 1 ? (double) stL * (double) (stN - 1) : 0.0) + (dir < 0 ? (double) rlen : 0.0);
    s.base = slice + (dir < 0 ? maxi (1, (int) (rlen + 0.999f)) : 0);
    s.env = s.envT = 1.0f; s.gate = gate; s.rel = 1.0f;
}

void Engine::breakRender (Voice& v, const float* P, int n, float* out)
{
    BreakState& s = v.bs;
    const Track& t = tr[v.ti];
    for (int i = 0; i < n; ++i) out[i] = 0.0f;
    if (s.on && ! brk.ready (s.brk)) s.on = 0;
    for (int j = 0; j < BRK_SEGS; ++j) if (s.s[j].on && ! brk.ready (s.s[j].brk)) s.s[j].on = 0;
    const double Tstep = samplesPerStep (t);
    const float dTau = (float) (1.0 / Tstep);
    const float tune = (P[P_TUNE] - 0.5f) * 48.0f;
    const bool vari = idxOf (P[P_B], 2) == 1;
    // the project's swing: the off 16ths later (their place on the grid: the trig's step, on)
    const double swing = (double) g[G_SWING] * 0.5;
    const float envC = P[P_DEC] >= 0.99f ? 1.0f : decayCoef (0.02f * powf_ (300.0f, P[P_DEC]), sr);
    const float envG = 1.0f - expf_ (-1.0f / (0.001f * sr));
    const float relInc = 1.0f / (0.004f * sr);
    const bool letGo = v.step >= 0 && (! running || t.mute);    // a step's loop stops with the transport or a mute
    if (letGo) s.gate = minf (s.gate, 0.0f);
    const BrkInfo& B = brk.b[s.brk >= 0 && s.brk < BRK_SLOTS ? s.brk : 0];
    const double L = (double) maxi (B.L16, 1), stepS = (double) maxi (B.len, 1) / L;   // samples per 16th of the loop
    const float semi = semis (tune + (float) s.pj);
    // VARI: the loop runs at the pitch's speed (its 16ths against the grid's); SYNC: the grid's, each piece at the pitch
    const float spd = vari ? semi : 1.0f;
    const float rate = vari ? (float) (stepS * (double) semi / Tstep) : semi * (float) BRK_RATE / sr;
    const float K = 1.0f / 32768.0f;
    for (int i = 0; i < n; ++i)
    {
        // the cursor: the pieces it starts
        if (s.on && s.rel > 0.0f)
        {
            while (s.ph < 3 && s.tau >= s.next)
            {
                const int bi = s.brk;
                if (s.ph == 0)
                {
                    // STUT: the slice's start, again
                    const double a = (double) B.onset[(int) s.b0];
                    breakPiece (s, brk, bi, a, minf_d (a + (double) s.stutLen * stepS, (double) B.len), 1, 0, s.pi ? 0.0007f : 0.0003f, sr);
                    if (++s.pi >= s.stutN - 1) { s.ph = 2; s.pi = 0; s.next = s.f0; s.base = (int) s.b0; }
                    else s.next = (double) s.pi * (double) s.stutLen;
                }
                else if (s.ph == 1)
                {
                    // REV: the window, backwards from its end
                    const double a = (double) B.onset[(int) s.b0];
                    const double e = minf_d (a + (double) s.rlen * stepS, (double) B.len - 1.0);
                    breakPiece (s, brk, bi, e, a, -1, 0, 0.002f, sr);
                    s.ph = 2; s.pi = 0; s.next = s.f0;
                }
                else if (vari)
                {
                    // VARI: one run, on and on through the loop
                    if (! (s.skip && s.pi == 0)) breakPiece (s, brk, bi, (double) B.onset[((s.base % B.L16) + B.L16) % B.L16], 1e18, 1, 1, 0.0003f, sr);
                    s.ph = 3;
                }
                else
                {
                    // SYNC: the next 16th, from its attack to the next one; it starts where its attack falls in the
                    // loop, the tempo's way (so the drummer's feel stays), the off 16ths of the grid later with the swing
                    const int q = s.base + s.pi, a = ((q % B.L16) + B.L16) % B.L16;
                    if (! (s.skip && s.pi == 0))
                        breakPiece (s, brk, bi, (double) B.onset[a], brkU (B, q + 1) - brkU (B, q) + (double) B.onset[a], 1, 0, 0.0003f, sr);
                    ++s.pi;
                    const int q1 = s.base + s.pi, g0 = s.k0 + (int) (s.f0 + 0.5);
                    const double sw = swing * (double) ((((g0 + s.pi) % 2) + 2) % 2 - ((s.k0 % 2) + 2) % 2);
                    s.next = s.f0 + (brkU (B, q1) - brkU (B, s.base)) / stepS + sw;
                }
            }
            s.tau += (double) (dTau * spd);
            s.envT *= envC; s.env += (s.envT - s.env) * envG;
            if (s.gate > 0.0f) s.gate -= 1.0f; else { s.rel = maxf (s.rel - relInc, 0.0f); if (s.rel <= 0.0f) s.on = 0; }
        }
        // the pieces sounding
        float y = 0.0f;
        for (int j = 0; j < BRK_SEGS; ++j)
        {
            BrkSeg& g = s.s[j];
            if (! g.on) continue;
            const BrkInfo& G = brk.b[g.brk];
            const short* mem = brk.mem (g.brk);
            const double len = (double) G.len;
            double p = g.pos - floord_ (g.pos / len) * len;
            const int i0 = mini ((int) p, G.len - 1);
            const int i1 = i0 + 1 < G.len ? i0 + 1 : (g.dir > 0 ? 0 : i0);
            const float fr = (float) (p - (double) i0);
            const float a0 = (float) mem[i0], a1 = (float) mem[i1];
            float w = 1.0f;
            if (! g.loop)
            {
                // the next piece goes on from here and starts before this one would fade: it carries on (no dip)
                if (g.dir > 0 && g.gInc >= 0.0f && s.on && s.ph == 2 && ! vari && s.brk == g.brk && (g.stop - g.pos) < (double) rate * 0.004 * (double) sr)
                {
                    const int q = s.base + s.pi, a = ((q % G.L16) + G.L16) % G.L16;
                    double d = (double) G.onset[a] - (g.stop - floord_ (g.stop / len) * len);
                    if (d > 0.5 * len) d -= len;
                    if (d < -0.5 * len) d += len;
                    const double tNext = (s.next - s.tau) / (double) (dTau * spd), tStop = (g.stop - g.pos) / (double) rate;
                    if (absd_ (d) < 0.002 * (double) BRK_RATE && tNext <= tStop + 0.0015 * (double) sr)
                        g.stop += d + (brkU (G, q + 1) - brkU (G, q));
                }
                // out before its end (3 ms): never into the next attack
                const double dist = (g.stop - g.pos) * (double) g.dir;
                if (dist <= 0.0) { g.on = 0; continue; }
                w = (float) minf_d (1.0, dist / ((double) rate * 0.003 * (double) sr));
            }
            g.g = clampf (g.g + g.gInc, 0.0f, 1.0f);
            if (g.gInc > 0.0f && g.g >= 1.0f) g.gInc = 0.0f;
            if (g.gInc < 0.0f && g.g <= 0.0f) { g.on = 0; continue; }
            y += (a0 + (a1 - a0) * fr) * w * g.g;
            g.pos += (double) rate * (double) g.dir;
        }
        out[i] = y * K * s.env * s.rel;
    }
    // the screen: where the cursor is
    if (s.on) { const double bp = s.b0 + (s.dir < 0 && s.tau < (double) s.rlen ? (double) s.rlen - s.tau : s.tau); s.pos = (float) (bp - floord_ (bp / L) * L); }
    float lvl = 0.0f;
    for (int j = 0; j < BRK_SEGS; ++j) if (s.s[j].on) lvl = 1.0f;
    lvl *= s.env * maxf (s.rel, s.on ? 1.0f : 0.0f);
    // DUST: the record (crackle, hiss) and the sampler's stage (the top rolled off, a softer, crunchier drive)
    const float dust = P[P_E];
    if (dust > 0.001f)
    {
        const float fc = 16000.0f * (1.0f - 0.68f * dust), lc = 1.0f - expf_ (-TWO_PI * minf (fc, 0.45f * sr) / sr);
        const float drv = 1.0f + 2.5f * dust, dn = 1.0f / tanhf_ (drv);
        const float crate = (3.0f + 40.0f * dust * dust) / sr, hiss = 0.006f * dust * dust;
        for (int i = 0; i < n; ++i)
        {
            float x = out[i];
            if (v.rng.uni() < crate) { const float uu = v.rng.uni() + 0.02f; s.crk += (v.rng.next() & 1u ? 1.0f : -1.0f) * minf (0.004f / (uu * uu), 0.3f) * dust; }
            const float c = s.crk - s.hp; s.hp += 0.25f * c; s.crk *= 0.4f;    // a click, high-passed
            x = tanhf_ (drv * x) * dn * (1.0f / (1.0f + 0.4f * dust)) + (c + hiss * v.rng.bi()) * (lvl > 0.0f ? 1.0f : 0.0f);
            s.lp1 += (x - s.lp1) * lc; s.lp2 += (s.lp1 - s.lp2) * lc;
            out[i] = s.lp2;
        }
    }
    v.ns += (u32) n;
    v.level = lvl;
}

// the screen: the slot under the knob (its slices, what each starts with, its waveform); the cursor, the last trig
//   0 on, 1 the slot shown (-1: none ready), 2 its 16ths, 3 the cursor (16ths, -1: elsewhere), 4 the slice, 5 LEN (16ths,
//   0 FREE), 6 what the trig did (1 RAND, 2 REV, 4 STUT, 8 PJMP, 16 SKIP, 32 SWAP), 7 the pitch jump, 8 slots ready,
//   9 the tempo, 10 the direction, 11 the level, 12 the slot playing (-1: none), 13 its 16ths, 14 the knob's slot,
//   15 the bars; then per slice its class + 4 x its strength (0..15), then the waveform (0..1)
void Engine::breakInfo (int ti, float* out) const
{
    for (int k = 0; k < BRK_INFO; ++k) out[k] = 0.0f;
    if (ti < 0 || ti >= NUM_TRACKS) return;
    const Track& t = tr[ti];
    const int knob = idxOf (t.p[P_A], BRK_SLOTS), bi = brk.resolve (knob);
    int nr = 0; for (int q = 0; q < BRK_SLOTS; ++q) if (brk.ready (q)) ++nr;
    out[1] = (float) bi; out[8] = (float) nr; out[14] = (float) knob; out[12] = -1.0f; out[3] = -1.0f;
    if (bi < 0) return;
    const BrkInfo& B = brk.b[bi];
    const Voice& v = t.voice[t.cur];
    const BreakState& s = v.bs;
    bool on = false;
    if (v.active && v.machine == M_BREAK) for (int j = 0; j < BRK_SEGS; ++j) if (s.s[j].on) on = true;
    on = on && brk.ready (s.brk);
    out[0] = on ? 1.0f : 0.0f;
    out[2] = (float) B.L16;
    out[3] = on ? s.pos : -1.0f;
    out[4] = (float) s.slice;
    out[5] = BRK_LEN16[idxOf (t.p[P_D], BRK_LENS)];
    out[6] = (float) s.flags; out[7] = (float) s.pj;
    out[9] = B.bpm;
    out[10] = on ? (float) s.dir : 0.0f; out[11] = on ? s.env * s.rel : 0.0f;
    out[12] = on ? (float) s.brk : -1.0f; out[13] = on ? (float) brk.b[s.brk].L16 : 0.0f;
    out[15] = (float) B.bars;
    for (int i = 0; i < B.L16; ++i) out[BRK_HEAD + i] = (float) B.cls[i] + 4.0f * floorf_ ((float) B.str[i] * (15.0f / 255.0f) + 0.5f);
    for (int i = 0; i < BRK_NPEAK; ++i) out[BRK_HEAD + BRK_MAXLEN + i] = (float) B.peak[i] * (1.0f / 255.0f);
}

} // namespace sk
