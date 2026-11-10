// IMPAKT — MANTRA (v84): eight voices that breathe and drive each other (see sk_mantra.h)
#include "sk_engine.h"

namespace sk {

namespace {
// the oscillators' thresholds: centred on half the supply, the Schmitt trigger's hysteresis either side (in supply
// units at the nominal supply: it does not follow the supply, its centre does), and what a period takes at nominal
// (two exponential segments of RC each)
constexpr float MT_HY = 0.09f;
const float MT_L = 0.72793075f;            // (2 ln (0.59 / 0.41))
constexpr int MT_LINE = 1 << 17;           // (each delay line: 2.7 s at 48 kHz, 1.36 s at 96)

inline float sideAt (const float* P, int q) { return P[sideParam (q)]; }
inline float smooth01 (float x) { x = clampf (x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }
inline float k1 (float sec, float sr) { return 1.0f - expf_ (-1.0f / (maxf (sec, 1e-5f) * sr)); }

// one relaxation oscillator's sample: the capacitor charged toward its target (the gate high: its pull-up, plus what
// is injected), the gate flipping where the capacitor crosses a threshold, between samples; the square's polyBLEP
// corrections for this sample and the one before (h: its step, x: from the edge to the sample). a1: the whole
// sample's decay, exp (-1 / tau) (what is left of a sample after an edge needs its own)
inline void oscStep (float& v, int& q, float tau, float a1, float vh, float inj, float tp, float tm, float& acc0, float& acc1)
{
    float rem = 1.0f;
    for (int it = 0; it < 4 && rem > 0.0f; ++it)
    {
        const float tgt = (q ? vh : 0.0f) + inj, thr = q ? tp : tm;
        const bool reach = q ? tgt > thr : tgt < thr;
        const float a = rem >= 1.0f ? a1 : expf_ (-rem / tau), vEnd = tgt + (v - tgt) * a;
        if (reach && (q ? vEnd >= thr : vEnd <= thr))
        {
            const float r = (v - tgt) / (thr - tgt);
            float tc = r > 1.0f ? tau * log2f_ (r) * LN2 : 0.0f;
            if (tc > rem) tc = rem;
            v = thr; rem -= tc;
            const float h = q ? -2.0f : 2.0f, x = rem;
            acc0 += 0.5f * h * x * x;
            acc1 -= 0.5f * h * (1.0f - x) * (1.0f - x);
            q ^= 1;
        }
        else { v = vEnd; rem = 0.0f; }
    }
    // (the gate's input diodes: the capacitor never goes far past the rails, however hard it is pushed)
    v = clampf (v, -0.06f, 1.06f);
}
// an LFO's relaxation oscillator (no corrections: its edges are slewed after)
inline void lfoStep (float& v, int& q, float tau, float a1)
{
    float c0 = 0.0f, c1 = 0.0f;
    oscStep (v, q, tau, a1, 1.0f, 0.0f, 0.5f + MT_HY, 0.5f - MT_HY, c0, c1);
}
inline float readLine (const float* b, int w, float d)
{
    const int di = (int) d; const float f = d - (float) di;
    const int m = MT_LINE - 1, i = w - di;
    const float x0 = b[(i + 1) & m], x1 = b[i & m], x2 = b[(i - 1) & m], x3 = b[(i - 2) & m];
    const float c1 = 0.5f * (x2 - x0), c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3, c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
    return ((c3 * f + c2) * f + c1) * f + x1;
}
} // namespace

// a trig, a pad, a drone: the sensors TCH names touched (a trig's velocity their pressure), the others lifted; held
// while its gate is (a pad, a drone: until let go), or a tap
void Engine::mantraTrigger (Voice& v, const float* P)
{
    MantraState& s = v.mt;
    if (s.fresh)
    {
        s.rng.seed (0x3A17A8u + 7919u * (u32) v.ti);
        for (int k = 0; k < MT_VOICES; ++k)
        {
            MantraVoice& o = s.vo[k];
            o.v = 0.5f - MT_HY + 0.02f * (float) k; o.q = (k & 1); o.sq1 = o.tr1 = o.acc0 = o.acc1 = 0.0f;
            o.env = 0.0f; o.seqP = o.padP = 0.0f; o.seqLeft = 0.0f;
            o.vibPh = s.rng.uni(); o.vibHz = 4.1f + 0.4f * (float) k + 0.3f * s.rng.bi();
            o.drN = o.drFrom = 0.0f; o.drTo = s.rng.bi(); o.drPh = s.rng.uni();
            // (the parts' tolerances, drawn once: TOL scales them)
            o.tolC = s.rng.bi(); o.tolT = s.rng.bi(); o.tolH = s.rng.bi();
            o.out = 0.0f; o.hz = 0.0f;
        }
        s.vddR = s.vddD = 0.0f;
        s.laV = 0.45f; s.lbV = 0.52f; s.laQ = 1; s.lbQ = 0; s.lOr = 0.0f; s.lSqA = s.lSqB = 0.0f;
        for (int p = 0; p < 4; ++p) s.pairM[p] = 0.0f;
        s.busX = s.busY = s.outX = s.outY = s.last = 0.0f; s.humPh = 0.0f;
        s.dw = 0; s.dT[0] = s.dT[1] = -1.0f;
        for (int c = 0; c < 2; ++c) { s.dIn[c][0] = s.dIn[c][1] = s.dOut[c][0] = s.dOut[c][1] = 0.0f; s.dSl[c] = s.dY[c] = 0.0f; }
        s.dCeil = 0.35f; s.dEnv = 0.0f; s.dAge = 1e9f;
        s.xHp = s.xLp = 0.0f; s.awake = 0.0f; s.outLv = s.wetLv = 0.0f;
        s.fresh = 0;
    }
    if (mtPadStart) return;                // (a finger on the screen's pads woke it: it touches nothing else)
    const unsigned mask = mantraTouchMask (idxOf (P[P_A], MT_TCHS));
    const float pr = clampf (0.3f + 0.7f * v.vel, 0.05f, 1.0f);
    const bool held = v.padHeld || v.drone || v.holdSec >= HOLD_OPEN * 0.5f;
    const float left = held ? -1.0f : (v.holdSec > 0.0f ? v.holdSec * sr : 0.12f * sr);
    for (int k = 0; k < MT_VOICES; ++k)
    {
        MantraVoice& o = s.vo[k];
        if ((mask >> k) & 1u) { o.seqP = pr; o.seqLeft = left; }
        else { o.seqP = 0.0f; o.seqLeft = 0.0f; }
    }
}

// a pad, a key let go: the sensors it held lifted
void Engine::mantraRelease (Voice& v)
{
    for (int k = 0; k < MT_VOICES; ++k) { MantraVoice& o = v.mt.vo[k]; if (o.seqLeft < 0.0f) { o.seqLeft = 0.0f; o.seqP = 0.0f; } }
}
// a stop, a mute, a drone ended: every finger off the sensors (HOLD stays: it is a knob)
void Engine::mantraLetGo (Voice& v)
{
    for (int k = 0; k < MT_VOICES; ++k) { MantraVoice& o = v.mt.vo[k]; o.seqP = 0.0f; o.seqLeft = 0.0f; }
}
// a finger on one of the screen's eight pads (its pressure; 0: lifted)
void Engine::mantraTouch (int t, int k, float pressure)
{
    if (t < 0 || t >= NUM_TRACKS || k < 0 || k >= MT_VOICES) return;
    Track& tk = tr[t];
    if (machineOf (lock (tk, -1, P_MACH)) != M_MANTRA) return;
    pressure = clampf (pressure, 0.0f, 1.0f);
    Voice* v = tk.voice[tk.cur].active && tk.voice[tk.cur].machine == M_MANTRA ? &tk.voice[tk.cur] : nullptr;
    if (! v)
    {
        if (pressure <= 0.0f) return;
        static float noLock[NUM_PARAMS]; static bool nl = false;
        if (! nl) { for (int p = 0; p < NUM_PARAMS; ++p) noLock[p] = -1.0f; nl = true; }
        mtPadStart = 1; padHit = 1;
        startVoice (tk, t, noLock, -1, lock (tk, -1, P_VEL), 0.0f, 0.0f);
        padHit = 0; mtPadStart = 0;
        tk.hitCount++;
        v = &tk.voice[tk.cur];
        if (! v->active || v->machine != M_MANTRA) return;
        v->padHeld = 0;                    // (its pads hold it, not the track's pad)
    }
    v->mt.vo[k].padP = pressure;
}
bool Engine::mantraHeld (const Voice& v) const
{
    for (int k = 0; k < MT_VOICES; ++k) { const MantraVoice& o = v.mt.vo[k]; if (o.padP > 0.0f || (o.seqP > 0.0f && o.seqLeft != 0.0f)) return true; }
    return false;
}

void Engine::mantraRender (Voice& v, const float* P, float tuneSt, int n, float* out)
{
    (void) tuneSt;
    MantraState& s = v.mt;
    FxState& f = fx[v.ti];
    // (the delay's lines live in the track's FX memory, which a sound machine does not use)
    if (f.kind != MT_KIND || f.sr != sr)
    {
        for (int i = 0; i < 2 * MT_LINE; ++i) f.mem[i] = 0.0f;
        f.kind = MT_KIND; f.sr = sr; s.dw = 0;
    }
    float* line0 = f.mem;
    float* line1 = f.mem + MT_LINE;
    const Track& t = tr[v.ti];
    const bool run = running && ! t.mute;
    if (! run && v.step >= 0) mantraLetGo (v);     // (a stop, a mute: what the sequencer touched is let go)
    const float fn = (float) n, isr = 1.0f / sr;
    // the controls
    const float transp = exp2f_ (((P[P_TUNE] - 0.5f) * 48.0f + v.note) * (1.0f / 12.0f));
    const float t60 = 0.5f * exp2f_ (6.0f * clampf (P[P_DEC], 0.0f, 1.0f));
    const float hld[2] = { P[P_B], P[P_C] };
    const float dfb = P[P_D], drv = P[P_E];
    const float pch[2] = { exp2f_ ((sideAt (P, MT_PCH1) - 0.85f) * 4.0f), exp2f_ ((sideAt (P, MT_PCH2) - 0.85f) * 4.0f) };
    const int   tunQ[MT_VOICES] = { MT_TUN1, MT_TUN2, MT_TUN3, MT_TUN4, MT_TUN5, MT_TUN6, MT_TUN7, MT_TUN8 };
    const int   shpQ[4] = { MT_SHP12, MT_SHP34, MT_SHP56, MT_SHP78 }, modQ[4] = { MT_MOD12, MT_MOD34, MT_MOD56, MT_MOD78 };
    const int   srcQ[4] = { MT_SRC12, MT_SRC34, MT_SRC56, MT_SRC78 }, fstQ[4] = { MT_FST12, MT_FST34, MT_FST56, MT_FST78 };
    const bool  oneLoop = idxOf (sideAt (P, MT_LOOP), 2) == 1, total = idxOf (sideAt (P, MT_TOTL), 2) == 1;
    const bool  andL = idxOf (sideAt (P, MT_LGIC), 2) == 1, link = idxOf (sideAt (P, MT_LINK), 2) == 1, vibOn = idxOf (sideAt (P, MT_VIB), 2) == 1;
    const float vdp = sideAt (P, MT_VDPT), sag = sideAt (P, MT_SAG), bus = sideAt (P, MT_BUS), bled = sideAt (P, MT_BLED);
    const float tol = sideAt (P, MT_TOL), drf = sideAt (P, MT_DRFT), nois = sideAt (P, MT_NOIS);
    const float dstm = sideAt (P, MT_DSTM), vol = sideAt (P, MT_VOL), dmix = sideAt (P, MT_DMIX);
    const bool  dSelf = idxOf (sideAt (P, MT_DSRC), 2) == 1, dSqr = idxOf (sideAt (P, MT_DWAV), 2) == 1;
    float sharp[4], dm[4]; int src[4]; bool fast[4];
    for (int p = 0; p < 4; ++p)
    {
        const float sh = sideAt (P, shpQ[p]); sharp[p] = sh * sh * (3.0f - 2.0f * sh);
        src[p] = idxOf (sideAt (P, srcQ[p]), MT_SRCS); fast[p] = idxOf (sideAt (P, fstQ[p]), 2) == 1;
        const float md = sideAt (P, modQ[p]);
        dm[p] = src[p] == 1 ? md * md * 1.6f : md * md * 0.9f;
    }
    // (each pair's FM source: the other pair of its group; on 1 LOOP, 12 <- 78, 34 <- 12, 56 <- 34, 78 <- 56)
    const int partner[4] = { oneLoop ? 3 : 1, 0, oneLoop ? 1 : 3, 2 };
    // the envelopes: the slow release (DEC), FAST's; the HOLD each pair gets (FAST pairs open later)
    const float kRs = k1 (t60 / 6.9f, sr), kRf = k1 (0.25f / 6.9f, sr);
    float hd[4];
    for (int p = 0; p < 4; ++p) hd[p] = mantraHoldLevel (hld[p >> 1], fast[p]);
    // the voices' frequencies this chunk (their tolerances, vibrato, drift), their time constants
    float tau[MT_VOICES], aTau[MT_VOICES], rho[MT_VOICES], leak[MT_VOICES], hcen[MT_VOICES], hhys[MT_VOICES];
    for (int k = 0; k < MT_VOICES; ++k)
    {
        MantraVoice& o = s.vo[k];
        o.vibPh += fn * o.vibHz * isr; if (o.vibPh >= 1.0f) o.vibPh -= floorf_ (o.vibPh);
        o.drPh += fn * (0.05f + 0.03f * (float) k) * isr;
        if (o.drPh >= 1.0f) { o.drPh -= floorf_ (o.drPh); o.drFrom = o.drTo; o.drTo = s.rng.bi(); }
        o.drN = o.drFrom + (o.drTo - o.drFrom) * smooth01 (o.drPh);
        const float cents = drf * drf * 12.0f * o.drN + (vibOn ? vdp * vdp * 40.0f * sin2pi (o.vibPh) : 0.0f) + tol * 18.0f * o.tolC;
        float hz = mantraTuneHz (k, sideAt (P, tunQ[k])) * pch[k >> 2] * transp * exp2f_ (cents * (1.0f / 1200.0f));
        hz = clampf (hz, 4.0f, 0.18f * sr);
        o.hz = hz;
        tau[k] = sr / (hz * MT_L); aTau[k] = expf_ (-1.0f / tau[k]);
        // (a modulation's current against its own: the higher it is tuned, the less it moves)
        rho[k] = clampf (powf_ (110.0f / hz, 0.7f), 0.05f, 2.5f);
        leak[k] = bled * 0.006f * sqrtf_ (hz * (1.0f / 440.0f)) * s.awake;
        hcen[k] = tol * 0.012f * o.tolT; hhys[k] = MT_HY * (1.0f + 0.15f * tol * o.tolH);
    }
    // the hyper LFO: A and B (0.03 .. 300 Hz), LINK (A sweeps B)
    const float fa = 0.03f * powf_ (10000.0f, sideAt (P, MT_FRQA)), fb = 0.03f * powf_ (10000.0f, sideAt (P, MT_FRQB));
    const float tauA = sr / (fa * MT_L), aA = expf_ (-1.0f / tauA), tauB = sr / (fb * MT_L), aB = expf_ (-1.0f / tauB), kOr = k1 (0.006f, sr), kSq = k1 (0.0015f, sr);
    // the delay: its times (8 ms .. 1.1 s), their converters (bandwidth, slew, hiss: the longer, the poorer), its
    // feedback (past the middle it sings), its modulation depths, its loop's ceiling
    float dTn[2], dBw[2], dSm[2], dNz[2], dMd[2];
    for (int c = 0; c < 2; ++c)
    {
        const float T = 0.008f * powf_ (137.5f, sideAt (P, c ? MT_TIM2 : MT_TIM1));
        dTn[c] = T;
        float bw = clampf (6000.0f * sqrtf_ (0.25f / T), 1200.0f, 11000.0f);
        if (T > 0.34f) bw *= sqrtf_ (0.34f / T);
        dBw[c] = 1.0f - expf_ (-TWO_PI * bw * isr);
        dSm[c] = clampf (0.08f * (0.34f / T) * (48000.0f * isr), 0.004f, 2.0f);
        // (its hiss fades when the instrument rests, unless the loop could sing from it)
        dNz[c] = 0.0008f * powf_ (T / 0.34f, 1.5f) * (dfb > 0.5f ? 1.0f : minf (1.0f, 4.0f * s.awake));
        const float md = sideAt (P, c ? MT_DMD2 : MT_DMD1); dMd[c] = md * md * 0.8f;
        if (s.dT[c] < 0.0f) s.dT[c] = T * sr;
    }
    const float dG = 2.1f * dfb, kT = k1 (0.003f, sr), kDe = k1 (1.0f, sr), kDc = k1 (1.5f, sr);
    // the mix's op-amp, the distortion, the volume
    const float gb = 1.0f + 5.0f * bus * bus, bb = 0.15f * bus, gbN = powf_ (gb, -0.5f), sb = softclip (bb);
    const float gx = 1.0f + 40.0f * drv * drv, bx = 0.3f * drv, sbx = softclip (bx), gxN = 1.0f / (1.0f + 0.55f * log2f_ (gx));
    const float kXh = k1 (1.0f / (TWO_PI * 700.0f), sr), kXl = 1.0f - expf_ (-TWO_PI * 9000.0f * isr);
    const float volG = 0.8f * vol * vol;
    const float dcR = 1.0f - TWO_PI * 5.0f * isr, kSag = k1 (0.03f, sr), kRip = 1.0f - expf_ (-TWO_PI * 3000.0f * isr);
    // (the hiss and the hum fade with the bleed when it rests: a voice left alone ends)
    const float humW = 50.0f * isr, hiss = nois * nois * 0.0015f * s.awake, humB = nois * nois * 0.0008f * s.awake;
    const float kAw = k1 (2.0f, sr);
    float outMax = 0.0f, wetMax = 0.0f;
    float pmAcc[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < n; ++i)
    {
        // the supply (from what the voices drew a sample ago)
        const float vdd = 1.0f - s.vddD - s.vddR;
        // the hyper LFO
        lfoStep (s.laV, s.laQ, tauA, aA);
        const float triA = clampf ((s.laV - 0.5f) / MT_HY, -1.0f, 1.0f);
        if (link) { const float tl = tauB * exp2f_ (-0.9f * triA); lfoStep (s.lbV, s.lbQ, tl, expf_ (-1.0f / tl)); }
        else lfoStep (s.lbV, s.lbQ, tauB, aB);
        const float triB = clampf ((s.lbV - 0.5f) / MT_HY, -1.0f, 1.0f);
        s.lSqA += kSq * ((float) s.laQ - s.lSqA); s.lSqB += kSq * ((float) s.lbQ - s.lSqB);
        s.lOr += kOr * (0.5f * ((float) s.laQ + (float) s.lbQ) - s.lOr);
        const float lfoV = andL ? s.lSqA * s.lSqB : s.lOr;                  // (0 .. 1, to the voices)
        const float lfoD = dSqr ? 2.0f * s.lSqA * s.lSqB - 1.0f : 0.5f * (triA + triB);   // (-1 .. 1, to the delay)
        // the pairs' outputs a sample ago (their FM), the crosstalk into each modulation
        float pairOut[4], all = 0.0f;
        for (int p = 0; p < 4; ++p) { pairOut[p] = 0.5f * (s.vo[2 * p].out + s.vo[2 * p + 1].out) * (1.0f / 0.6f); all += pairOut[p]; }
        float m[4];
        for (int p = 0; p < 4; ++p)
        {
            const float sv = src[p] == 0 ? 0.0f : src[p] == 1 ? pairOut[partner[p]] : (total ? s.last * 2.0f : lfoV);
            m[p] = dm[p] * sv + bled * 0.03f * all;
            pmAcc[p] += absf (m[p]);
        }
        // the voices
        float sum = 0.0f, load = 0.0f, rip = 0.0f, touch = 0.0f;
        for (int k = 0; k < MT_VOICES; ++k)
        {
            MantraVoice& o = s.vo[k];
            const int p = k >> 1;
            // its sensor: a finger's pressure (FAST: harder to wake), its attack (a light touch slower), its release
            if (o.seqLeft > 0.0f) { o.seqLeft -= 1.0f; if (o.seqLeft <= 0.0f) { o.seqLeft = 0.0f; o.seqP = 0.0f; } }
            const float pr = maxf (o.seqP, o.padP);
            touch += pr;
            float lvl = 0.0f;
            if (pr > 0.01f) lvl = fast[p] ? (pr > 0.15f ? powf_ ((pr - 0.15f) * (1.0f / 0.85f), 0.6f) : 0.0f) : powf_ (pr, 0.6f);
            if (lvl > o.env) { const float ta = 0.004f + 0.35f * (1.0f - pr) * (1.0f - pr); o.env += (lvl - o.env) * k1 (ta, sr); }
            else o.env += (lvl - o.env) * (fast[p] ? kRf : kRs);
            const float g = hd[p] + (1.0f - hd[p]) * o.env;
            // the oscillator: its pull-up through its envelope, its thresholds on the supply, what is injected
            const float vh = vdd * (1.0f - 0.05f * (1.0f - g));
            const float c = 0.5f * vdd + hcen[k], tp = c + hhys[k], tm = c - hhys[k];
            oscStep (o.v, o.q, tau[k], aTau[k], vh, rho[k] * m[p], tp, tm, o.acc0, o.acc1);
            const float sqN = o.q ? 1.0f : -1.0f, trN = clampf ((o.v - c) / hhys[k], -1.5f, 1.5f);
            const float sq = o.sq1 + o.acc0, tr = o.tr1;
            o.sq1 = sqN; o.tr1 = trN; o.acc0 = o.acc1; o.acc1 = 0.0f;
            // SHARP, its VCA (a transistor: it rounds what it lets through), its bleed
            const float sig = tr + (sq - tr) * sharp[p];
            const float y = softclip (1.5f * g * sig) * (1.0f / 1.5f) + leak[k] * sig;
            o.out = y;
            sum += y;
            load += g * (0.6f + 0.4f * (float) o.q);
            rip += ((float) o.q - 0.5f) * (0.3f + g);
        }
        // the shared supply: its load (what the open voices draw), its ripple (each gate's current)
        s.vddD += kSag * (sag * 0.025f * load * 0.125f - s.vddD);
        s.vddR += kRip * (sag * 0.015f * rip * 0.125f - s.vddR);
        // the mix: its op-amp driven (BUS), its DC, the hiss, the hum (a finger on a sensor picks more up)
        float x = (softclip (gb * 0.5f * sum + bb) - sb) * gbN;
        float yb = x - s.busX + dcR * s.busY; s.busX = x; s.busY = yb;
        x = yb;
        s.humPh += humW; if (s.humPh >= 1.0f) s.humPh -= 1.0f;
        if (nois > 0.0f && (s.awake > 1e-4f || touch > 0.0f))
        {
            const float hs = sin2pi (s.humPh), hum = humB + nois * 0.004f * minf (touch, 2.0f);
            x += hiss * s.rng.bi() + hum * (hs + 0.3f * sin2pi (3.0f * s.humPh));
        }
        // the delay: two lines on one feedback loop (its saturation held under the loop's ceiling or what comes in)
        const float xa = absf (x);
        s.dEnv += (xa > s.dEnv ? 1.0f : kDe) * (xa - s.dEnv);
        const float ct = clampf (1.2f * s.dEnv, 0.35f, 1.0f);
        s.dCeil += (ct > s.dCeil ? 1.0f : kDc) * (ct - s.dCeil);
        const float fbIn = s.dCeil * softclip (dG * 0.5f * (s.dY[0] + s.dY[1]) / s.dCeil);
        float wet = 0.0f;
        for (int c = 0; c < 2; ++c)
        {
            float* line = c ? line1 : line0;
            // its converter: the input filtered, the slew it can follow, its hiss
            float u = x + fbIn;
            s.dIn[c][0] += dBw[c] * (u - s.dIn[c][0]); s.dIn[c][1] += dBw[c] * (s.dIn[c][0] - s.dIn[c][1]);
            const float d = clampf (s.dIn[c][1] - s.dSl[c], -dSm[c], dSm[c]); s.dSl[c] += d;
            line[s.dw] = clampf (s.dSl[c] + dNz[c] * s.rng.bi(), -1.5f, 1.5f);
            if (absf (s.dSl[c]) > 1e-4f) s.dAge = 0.0f;
            // its time: modulated by its own output (SELF) or the LFO, glided as its clock moves
            const float srcM = dSelf ? s.dY[c] : lfoD;
            const float Te = clampf (dTn[c] * (1.0f + dMd[c] * srcM), 0.002f, 1.25f) * sr;
            s.dT[c] += kT * (Te - s.dT[c]);
            const float rd = readLine (line, s.dw, clampf (s.dT[c], 3.0f, (float) (MT_LINE - 4)));
            s.dOut[c][0] += dBw[c] * (rd - s.dOut[c][0]); s.dOut[c][1] += dBw[c] * (s.dOut[c][0] - s.dOut[c][1]);
            s.dY[c] = s.dOut[c][1];
            wet += 0.5f * s.dY[c];
        }
        s.dw = (s.dw + 1) & (MT_LINE - 1);
        s.dAge += 1.0f;
        wetMax = maxf (wetMax, absf (wet));
        float o = x + (wet - x) * dmix;
        // the distortion, last: its highs lifted into it, its curve (a little asymmetric), its top tamed
        s.xHp += kXh * (o - s.xHp);
        const float xe = o + drv * 1.2f * (o - s.xHp);
        float dz = (softclip (gx * xe + bx) - sbx) * gxN;
        s.xLp += kXl * (dz - s.xLp); dz = s.xLp;
        o += (dz - o) * dstm;
        o *= volG;
        float yo = o - s.outX + dcR * s.outY; s.outX = o; s.outY = yo;
        s.last = yo;
        out[i] = yo;
        outMax = maxf (outMax, absf (yo));
    }
    // (how much it plays: the bleed fades when it rests)
    float aw = 0.0f; for (int k = 0; k < MT_VOICES; ++k) aw = maxf (aw, hd[k >> 1] + (1.0f - hd[k >> 1]) * s.vo[k].env);
    s.awake += (aw > s.awake ? 1.0f : 1.0f - expf_ (-fn * kAw)) * (aw - s.awake);
    for (int p = 0; p < 4; ++p) s.pairM[p] = pmAcc[p] / fn;
    // (what keeps the track's voice: its voices' levels, the delay's tail, what comes out)
    // (and what the lines still hold, until it has come out)
    const bool pending = dmix > 0.0f && s.dAge < maxf (s.dT[0], s.dT[1]) + 0.05f * sr;
    v.level = maxf (maxf (maxf (aw, outMax), wetMax), pending ? 0.01f : 0.0f);
    s.outLv = maxf (outMax, s.outLv * 0.9f); s.wetLv = maxf (wetMax, s.wetLv * 0.9f);
    // (a numeric failure: the voices and the delay start again from silence)
    bool bad = isBad (s.vddD) || isBad (s.vddR) || isBad (s.busY) || isBad (s.outY) || isBad (s.dSl[0]) || isBad (s.dSl[1]) || isBad (s.dT[0]) || isBad (s.dT[1]) || isBad (s.xLp);
    for (int k = 0; k < MT_VOICES; ++k) if (isBad (s.vo[k].v) || isBad (s.vo[k].out) || isBad (s.vo[k].acc0)) bad = true;
    if (bad)
    {
        s.fresh = 1; mtPadStart = 1; mantraTrigger (v, P); mtPadStart = 0;
        for (int i = 0; i < 2 * MT_LINE; ++i) f.mem[i] = 0.0f;
        for (int i = 0; i < n; ++i) out[i] = 0.0f;
    }
}

// for the screen (HALO_INFO values): 5; the voices; each one's level, frequency, pressure; the LFO (its output, A, B);
// the delay's level; the output's; each pair's modulation; the supply; how awake it is
void Engine::mantraInfo (int ti, float* out) const
{
    const Track& t = tr[ti];
    const Voice& v = t.voice[t.cur];
    out[0] = 5.0f; out[1] = 0.0f;
    if (! v.active || v.machine != M_MANTRA) return;
    const MantraState& s = v.mt;
    out[1] = (float) MT_VOICES;
    float P[NUM_PARAMS];
    effParams (t, v.lk, P);
    for (int k = 0; k < MT_VOICES; ++k)
    {
        const MantraVoice& o = s.vo[k];
        const int p = k >> 1;
        const bool fast = idxOf (P[sideParam (p == 0 ? MT_FST12 : p == 1 ? MT_FST34 : p == 2 ? MT_FST56 : MT_FST78)], 2) == 1;
        const float x = P[k < 4 ? P_B : P_C];
        const float hd = mantraHoldLevel (x, fast);
        out[2 + k] = hd + (1.0f - hd) * o.env;
        out[10 + k] = o.hz;
        out[18 + k] = maxf (o.seqP, o.padP);
    }
    out[26] = idxOf (P[sideParam (MT_LGIC)], 2) == 1 ? s.lSqA * s.lSqB : s.lOr;
    out[27] = s.lSqA; out[28] = s.lSqB; out[29] = s.wetLv; out[30] = s.outLv;
    for (int p = 0; p < 4; ++p) out[31 + p] = s.pairM[p];
    out[35] = 1.0f - s.vddD - s.vddR; out[36] = s.awake;
}

} // namespace sk
