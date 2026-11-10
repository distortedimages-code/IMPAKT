// IMPAKT — CHIMERA (v66), the sampler on the bank: FAMILLE and GREFFE (sk_chimera.h)
#include "sk_engine.h"

namespace sk {

// the knobs (A .. F: CAT, SMPL, VARI, STRT, PLAY, SIDE) and the side places (sk_engine.h ChimParam)
static inline float chimPoint (float v) { return 0.001f * powf_ (150.0f, v); }                  // POIN: 1 .. 150 ms
static inline float chimFade (float v)  { return 0.0005f * powf_ (80.0f, v); }                  // FOND: 0.5 .. 40 ms
static inline float chimSplit (float v) { return v < 0.02f ? 0.0f : 60.0f * powf_ (133.3333f, (v - 0.02f) / 0.98f); }   // SPLT: OFF, 60 Hz .. 8 kHz
static inline float chimDec (float v)   { return 0.02f * powf_ (300.0f, v); }                   // DEC: 20 ms .. 6 s, FULL at its top
constexpr float CHIM_FULL = 0.99f;

// the sample at a category and SMPL (-1: none)
static int sampleIn (int cat, float smpP)
{
    const Bank& b = BANK;
    if (b.state < 1) return -1;
    bankAcquire();                                      // (the index, then the sample: sk_bank.h)
    if (cat < 0 || cat >= b.ncat || b.catCount[cat] < 1) return -1;
    const int i = b.catFirst[cat] + idxOf (smpP, b.catCount[cat]);
    const int ok = b.e[i].ok;
    bankAcquire();
    return ok ? i : -1;
}
// the interval (semitones) that brings pitch q to the name of pitch p, the nearest (within a tritone); 0 when either
// has none
static float matchSemis (float p, float q)
{
    if (p <= 0.0f || q <= 0.0f) return 0.0f;
    float d = 12.0f * log2f_ (p / q);
    d -= 12.0f * floorf_ (d / 12.0f + 0.5f);
    return d;
}

// a frame of a sample (cubic Hermite between its frames): its middle and its sides, in -1 .. 1
static inline void readAt (const BankEntry& e, double pos, float& m, float& s)
{
    const double fl = __builtin_floor (pos);
    const int i = (int) fl;
    const float f = (float) (pos - fl);
    float l[4], r[4];
    if (i >= 1 && i + 2 < e.frames)
    {
        const short* p = BANK_PCM + e.off + (i - 1) * e.ch;
        if (e.ch == 2) for (int k = 0; k < 4; ++k) { l[k] = (float) p[2 * k]; r[k] = (float) p[2 * k + 1]; }
        else for (int k = 0; k < 4; ++k) l[k] = r[k] = (float) p[k];
    }
    else for (int k = 0; k < 4; ++k) { l[k] = bankVal (e, i - 1 + k, 0); r[k] = e.ch == 2 ? bankVal (e, i - 1 + k, 1) : l[k]; }
    auto herm = [f] (const float* y) {
        const float c1 = 0.5f * (y[2] - y[0]), c2 = y[0] - 2.5f * y[1] + 2.0f * y[2] - 0.5f * y[3], c3 = 0.5f * (y[3] - y[0]) + 1.5f * (y[1] - y[2]);
        return ((c3 * f + c2) * f + c1) * f + y[1];
    };
    const float L = herm (l), R = herm (r);
    m = (L + R) * (0.5f / 32768.0f); s = (L - R) * (0.5f / 32768.0f);
}
// the middle only, linearly (the seam's alignment)
static inline float readLin (const BankEntry& e, double pos)
{
    const int i = (int) pos; const float f = (float) (pos - (double) i);
    float a, b;
    if (e.ch == 2) { a = bankVal (e, i, 0) + bankVal (e, i, 1); b = bankVal (e, i + 1, 0) + bankVal (e, i + 1, 1); }
    else { a = 2.0f * bankVal (e, i, 0); b = 2.0f * bankVal (e, i + 1, 0); }
    return a + (b - a) * f;
}

void Engine::chimTrigger (Voice& v, const float* P)
{
    ChimState& c = v.chm;
    Track& t = tr[v.ti];
    c.done = 1; c.hasSide = 0; c.a = c.b = -1; c.env = 1.0f; c.envC = 1.0f; c.fade = 1.0f; c.fadeInc = 0.0f;
    c.la.reset(); c.ls.reset(); c.lb.reset(); c.lbs.reset(); c.sf.reset(); c.sDx = c.sDy = 0.0f;
    for (int i = 0; i < CHIM_CHUNK; ++i) c.side[i] = 0.0f;
    const Bank& b = BANK;
    int a = sampleIn (b.ncat > 0 ? idxOf (P[P_A], b.ncat) : -1, P[P_B]);
    const int bc = b.ncat > 0 ? idxOf (P[PC_BCAT], b.ncat + 1) : 0;
    int bb = bc > 0 ? sampleIn (bc - 1, P[PC_BSMP]) : -1;
    t.chNc = 0;
    if (a < 0) { t.chA = -1; t.chB = -1; t.chHits++; return; }
    // FAMILLE: a neighbour instead, drawn at each hit (right) or per step (left, round again with the pattern)
    const float vr = P[P_C] - 0.5f;
    float semA = 0.0f;
    if (absf (vr) > 0.005f)
    {
        const int N = 1 + (int) (absf (vr) * 2.0f * (float) (CHIM_NEAR - 1) + 0.5f);
        const int tgt = idxOf (P[PC_CIBL], 3);
        const bool same = idxOf (P[PC_MEME], 2) == 1, acc = idxOf (P[PC_ACCD], 2) == 1;
        const float sens = (P[PC_SENS] - 0.5f) * 2.0f;
        const unsigned li = v.step >= 0 ? (unsigned) v.step * 64u + (unsigned) mini (t.rdSub, 63) : 4096u + (unsigned) t.rdSub;
        auto draw = [&] (int i, unsigned salt, bool keep) {
            int cand[CHIM_NEAR];
            const int k = bankNear (i, sens, same, cand, N);
            if (keep) { t.chNc = k; for (int q = 0; q < k; ++q) t.chCand[q] = cand[q]; }
            const float r = vr > 0.0f ? t.rng.uni() : 0.5f * (loopDraw (P[PC_SEED], salt, li) + 1.0f);
            return cand[mini (k - 1, (int) (r * (float) k))];
        };
        if (tgt != 1) { const int na = draw (a, 5u, true); if (acc) semA = matchSemis (b.e[a].pitch, b.e[na].pitch); a = na; }
        if (tgt != 0 && bb >= 0) bb = draw (bb, 6u, tgt == 1);
    }
    const BankEntry& ea = b.e[a];
    c.a = a; c.b = bb;
    c.semis = semA;
    c.gA = ea.gain;
    c.rev = idxOf (P[P_E], 2) == 1;
    c.aOn = (double) (ea.onset * ea.sr);
    c.a0 = (double) (P[P_D] * 0.9f * (float) ea.frames);
    c.aEnd = (double) ea.frames;
    c.hasSide = ea.ch == 2;
    // GREFFE: B read in step with A from their onsets, at A's pitch (the nearest of its name) and BTUN, lined up at the seam
    if (bb >= 0)
    {
        const BankEntry& eb = b.e[bb];
        const float st = matchSemis (ea.pitch, eb.pitch) + (P[PC_BTUN] - 0.5f) * 24.0f;
        c.kap = (double) (eb.sr / ea.sr) * (double) semis (st);
        c.bOn = (double) (eb.onset * eb.sr);
        c.gB = eb.gain * exp2f_ ((36.0f * P[PC_BLVL] - 24.0f) * 0.16609640f);   // (BLVL: -24 .. +12 dB)
        c.gStart = c.a0 + (double) (chimPoint (P[PC_POIN]) * ea.sr);
        c.gFd = maxf (1.0f, chimFade (P[PC_FOND]) * ea.sr);
        c.split = chimSplit (P[PC_SPLT]);
        // the alignment: B shifted by up to 2.5 ms where it best matches A over 5 ms around the seam (coarse, then fine)
        const int D = (int) (0.0025f * eb.sr), W = (int) (0.005f * ea.sr);
        const double x0 = c.gStart - 0.5 * (double) W;
        float ea2 = 0.0f;
        for (int k = 0; k < W; k += 2) { const float x = readLin (ea, x0 + k); ea2 += x * x; }
        int best = 0;
        if (ea2 > 1e3f)
        {
            float bestS = -1e30f;
            auto score = [&] (int d) {
                float ab = 0.0f, b2 = 1e-6f;
                for (int k = 0; k < W; k += 2)
                {
                    const float x = readLin (ea, x0 + k), y = readLin (eb, c.bOn + (x0 + k - c.aOn) * c.kap + d);
                    ab += x * y; b2 += y * y;
                }
                return ab / sqrtf_ (b2);
            };
            for (int d = -D; d <= D; d += 3) { const float s = score (d); if (s > bestS) { bestS = s; best = d; } }
            const int c0 = best;
            for (int d = c0 - 2; d <= c0 + 2; ++d) { if (d == c0) continue; const float s = score (d); if (s > bestS) { bestS = s; best = d; } }
        }
        c.bOn += (double) best;
        t.chAlign = 1000.0f * (float) best / eb.sr;
        // the sound lasts as long as the longer of the two
        const double bEndInA = c.aOn + ((double) eb.frames - c.bOn) / c.kap;
        if (bEndInA > c.aEnd) c.aEnd = bEndInA;
        if (eb.ch == 2) c.hasSide = 1;
        const float fs = clampf (c.split, 20.0f, 0.45f * ea.sr);
        if (c.split > 0.0f) { c.la.set (fs, 0.7071f, ea.sr); c.ls = c.la; c.lb = c.la; c.lbs = c.la; c.la.reset(); c.ls.reset(); c.lb.reset(); c.lbs.reset(); }
    }
    else t.chAlign = 0.0f;
    // DEC: an envelope over the sound (FULL at the top: the sample as it is); a start inside it or reversed fades in
    c.envC = P[P_DEC] >= CHIM_FULL ? 1.0f : decayCoef (chimDec (P[P_DEC]), sr);
    if (c.rev || c.a0 > 1.0) { c.fade = 0.0f; c.fadeInc = 1.0f / maxf (1.0f, 0.002f * sr); }
    c.pa = c.rev ? c.aEnd - 1.0 : c.a0;
    c.done = 0;
    t.chA = a; t.chB = bb; t.chSemis = semA; t.chHits++;
}

void Engine::chimRender (Voice& v, const float* P, float tuneSt, int n, float* out)
{
    ChimState& c = v.chm;
    if (c.done || c.a < 0 || ! BANK.e[c.a].ok || (c.b >= 0 && ! BANK.e[c.b].ok))
    {
        for (int i = 0; i < n; ++i) { out[i] = 0.0f; c.side[i] = 0.0f; }
        c.done = 1; v.level = 0.0f; return;
    }
    const BankEntry& ea = BANK.e[c.a];
    const BankEntry* eb = c.b >= 0 ? &BANK.e[c.b] : nullptr;
    // its rate (A frames a sample): TUNE + NOTE now (an LFO on TUNE moves it), ACCD's interval of the hit
    c.ra = (ea.sr / sr) * semis (tuneSt + c.semis);
    const double step = c.rev ? -(double) c.ra : (double) c.ra;
    const bool split = eb && c.split > 0.0f;
    float lvl = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        if (c.done) { out[i] = 0.0f; c.side[i] = 0.0f; continue; }
        float am, as; readAt (ea, c.pa, am, as);
        am *= c.gA; as *= c.gA;
        float m = am, s = as;
        if (eb)
        {
            const float u = (float) ((c.pa - c.gStart) / (double) c.gFd);
            const float x = clampf (u, 0.0f, 1.0f), w = x * x * (3.0f - 2.0f * x);
            float bm = 0.0f, bs = 0.0f;
            if (w > 0.0f || split) { readAt (*eb, c.bOn + (c.pa - c.aOn) * c.kap, bm, bs); bm *= c.gB; bs *= c.gB; }
            if (split)
            {
                // A's lows give way to B's, A's highs go on: A - w LP(A) + w LP(B)
                float lo, bp, hp;
                c.la.tick (am, lo, bp, hp); m = am - w * lo;
                c.lb.tick (bm, lo, bp, hp); m += w * lo;
                c.ls.tick (as, lo, bp, hp); s = as - w * lo;
                c.lbs.tick (bs, lo, bp, hp); s += w * lo;
            }
            else { m = am + (bm - am) * w; s = as + (bs - as) * w; }
        }
        const float g = c.env * c.fade;
        out[i] = m * g; c.side[i] = s * g;
        lvl = maxf (lvl, absf (out[i]));
        c.env *= c.envC;
        if (c.fade < 1.0f) { c.fade += c.fadeInc; if (c.fade > 1.0f) c.fade = 1.0f; }
        c.pa += step;
        if ((! c.rev && c.pa >= c.aEnd) || (c.rev && c.pa < c.a0) || c.env < 1e-5f) c.done = 1;
    }
    if (split) { c.la.sanitize(); c.lb.sanitize(); c.ls.sanitize(); c.lbs.sanitize(); }
    v.level = c.done ? 0.0f : maxf (lvl, 1e-3f);
    v.ns += (u32) n;
}

// the sides, through a filter like the voice's (its type, where it is; not its resonance nor its drive): what
// renderVoice adds to the track's sides with the voice's level
void Engine::chimSides (Voice& v, int ft, float fnorm, int n)
{
    ChimState& c = v.chm;
    if (! c.hasSide) return;
    const float R = 1.0f - TWO_PI * 10.0f / sr;
    for (int i = 0; i < n; ++i) { const float y = c.side[i] - c.sDx + R * c.sDy; c.sDx = c.side[i]; c.sDy = y; c.side[i] = y; }
    const float fc = 20.0f * exp2f_ (fnorm * 9.9658f);
    const bool open = (ft == FT_LP && fnorm > 0.97f) || (ft == FT_HP && fnorm < 0.03f) || ft == FT_COMB;
    if (open) return;
    c.sf.set (fc, 0.7071f, sr);
    for (int i = 0; i < n; ++i)
    {
        float lo, bp, hp; c.sf.tick (c.side[i], lo, bp, hp);
        c.side[i] = ft == FT_LP ? lo : (ft == FT_HP ? hp : bp * 1.414f);
    }
    c.sf.sanitize();
}

// for the screen: the bank (state, decoded, samples, refused), the track's last hit (A, B, hits, the alignment in ms,
// ACCD's interval), how many neighbours it drew from and which
void Engine::chimInfo (int t, float* out) const
{
    for (int i = 0; i < CHIM_INFO; ++i) out[i] = -1.0f;
    out[0] = (float) BANK.state; out[1] = (float) BANK.done; out[2] = (float) BANK.n; out[3] = (float) BANK.failed;
    if (t < 0 || t >= NUM_TRACKS) return;
    const Track& k = tr[t];
    out[4] = (float) k.chA; out[5] = (float) k.chB; out[6] = (float) k.chHits; out[7] = k.chAlign; out[8] = k.chSemis;
    out[9] = (float) k.chNc;
    for (int q = 0; q < k.chNc && q < CHIM_NEAR; ++q) out[10 + q] = (float) k.chCand[q];
}

} // namespace sk
