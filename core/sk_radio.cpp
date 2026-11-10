// IMPAKT — RADIO (v87): the receivers' feed and history, the machine's voice (sk_radio.h). Own implementation.
#include "sk_engine.h"

namespace sk {

namespace {

constexpr int FMASK = RADIO_FIFO - 1, HMASK = RADIO_HIST - 1, GUARD = 16;
constexpr float WEB_FILL = 1.0f, SW_FILL = 0.5f;            // seconds in hand

inline float hermite (float xm1, float x0, float x1, float x2, float t)
{
    const float c = 0.5f * (x1 - xm1), v = x0 - x1, w = c + v, a = w + v + 0.5f * (x2 - x0), b = w + a;
    return ((a * t - b) * t + c) * t + x0;
}
// a position in the history (samples, from its start: a double) as a place in its ring (no 64-bit conversion: the
// plain-JS build of the engine has none)
inline int histIdx (double fl) { return (int) (fl - __builtin_floor (fl * (1.0 / RADIO_HIST)) * (double) RADIO_HIST); }
inline float histAt (const RadioRx& r, double p)
{
    const double fl = __builtin_floor (p);
    const int i = histIdx (fl);
    const float t = (float) (p - fl);
    const short* h = r.hist;
    return hermite (h[(i - 1) & HMASK], h[i & HMASK], h[(i + 1) & HMASK], h[(i + 2) & HMASK], t) * (1.0f / 32768.0f);
}
inline float loopAt (const short* b, int L, float p)
{
    int i = (int) p; const float t = p - (float) i;
    auto at = [&] (int k) { k %= L; if (k < 0) k += L; return (float) b[k]; };
    return hermite (at (i - 1), at (i), at (i + 1), at (i + 2), t) * (1.0f / 32768.0f);
}
// the shifter's allpass chains: their outputs a quarter turn apart across the band (a known pair of 4-stage chains)
const float HA[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
const float HB[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };

// the common amplitude envelope (as sk_engine.cpp's)
inline float ampTickR (Voice& v, float attInc, float coef)
{
    if (v.astage == 0)      { v.aenv += attInc; if (v.aenv >= 1.0f) { v.aenv = 1.0f; v.astage = 1; } }
    else if (v.astage == 1) { if (v.aHoldLeft > 0.0f) v.aHoldLeft -= 1.0f; else v.astage = 2; }
    else                    v.aenv *= v.hrel > 0.0f ? v.hrel : coef;
    return v.ecrv < 0.9995f ? envCurve (v.aenv, v.astage, v.ecrv) : v.aenv;
}

} // namespace

void RadioBank::reset()
{
    for (int k = 0; k < RADIO_RX; ++k)
    {
        RadioRx& r = rx[k];
        for (int i = 0; i < RADIO_FIFO; ++i) r.fifo[i] = 0;
        for (int i = 0; i < RADIO_HIST; ++i) r.hist[i] = 0;
        r.wr = r.rd = 0; r.flushReq = 0; r.flushSeen = 0;
        r.state = RS_NONE; r.kind = RK_WEB; r.meterC = -32768;
        r.priming = 1; r.underruns = 0; r.fpos = 0.0f; r.ratio = 1.0f;
        r.gain = 0.0f;
        r.hw = 0; r.hacc = 0.0; r.sig = 0.0f; r.level = 0.0f; r.fill = 0.0f;
        for (int i = 0; i < RADIO_NPEAK; ++i) r.peak[i] = 0;
        r.pkW = 0; r.pkN = 0; r.pkAcc = 0.0f;
    }
    ver = 0;
}

// ---- the host's side (its own thread): its sound in, a flush, its state
int Engine::radioPush (int k, const short* x, int n)
{
    if (k < 0 || k >= RADIO_RX || n <= 0) return 0;
    RadioRx& r = radio.rx[k];
    const unsigned wr = (unsigned) r.wr, rd = (unsigned) rdLoad (&r.rd);
    const int space = RADIO_FIFO - (int) (wr - rd) - GUARD;
    if (n > space) n = space;
    if (n <= 0) return 0;
    for (int i = 0; i < n; ++i) r.fifo[(wr + (unsigned) i) & FMASK] = x[i];
    rdStore (&r.wr, (int) (wr + (unsigned) n));
    return n;
}
int  Engine::radioSpace (int k) const
{
    if (k < 0 || k >= RADIO_RX) return 0;
    const RadioRx& r = radio.rx[k];
    return RADIO_FIFO - (int) ((unsigned) r.wr - (unsigned) rdLoad (&r.rd)) - GUARD;
}
void Engine::radioFlush (int k)            { if (k >= 0 && k < RADIO_RX) rdStore (&radio.rx[k].flushReq, radio.rx[k].flushReq + 1); }
void Engine::radioSetState (int k, int st, int kind)
{
    if (k < 0 || k >= RADIO_RX) return;
    rdStore (&radio.rx[k].kind, kind);
    rdStore (&radio.rx[k].state, st);
}
void Engine::radioMeter (int k, float dbm)  { if (k >= 0 && k < RADIO_RX) rdStore (&radio.rx[k].meterC, dbm < -300.0f ? -32768 : (int) (dbm * 100.0f)); }

// which receivers the tracks play (bits): the host keeps those connected
int Engine::radioUse() const
{
    int m = 0;
    for (int t = 0; t < NUM_TRACKS; ++t)
        if (machineOf (tr[t].p[P_MACH]) == M_RADIO) m |= 1 << idxOf (tr[t].p[P_A], RADIO_RX);
    return m;
}

// ---- once a chunk: each receiver's feed read into its history (n output samples' worth)
void Engine::radioAdvance (int n)
{
    const double due = (double) n * (double) RADIO_RATE / (double) sr;
    const float sgA = 1.0f - expf_ (-(float) n / (0.03f * sr));
    for (int k = 0; k < RADIO_RX; ++k)
    {
        RadioRx& r = radio.rx[k];
        const int st = rdLoad (&r.state), kind = rdLoad (&r.kind);
        const int fr = rdLoad (&r.flushReq);
        unsigned rd = (unsigned) r.rd;
        const unsigned wr = (unsigned) rdLoad (&r.wr);
        if (fr != r.flushSeen) { r.flushSeen = fr; rd = wr; r.priming = 1; r.fpos = 0.0f; }
        r.hacc += due;
        const int m = (int) r.hacc;
        r.hacc -= (double) m;
        int avail = (int) (wr - rd);
        const int target = (int) ((kind == RK_SW ? SW_FILL : WEB_FILL) * (float) RADIO_RATE);
        if (r.priming && avail >= target && (st == RS_LIVE || st == RS_BUFFER))
        {
            r.priming = 0;
            if (avail > target + target / 2) { rd = wr - (unsigned) target; avail = target; }    // (a burst: keep only what is needed)
        }
        if (! r.priming)
        {
            // (far too much in hand: a burst after the connection or a stall: on to the newest, faded in again)
            if (avail > 2 * target) { rd = wr - (unsigned) target; avail = target; r.fpos = 0.0f; r.gain = 0.0f; }
            // a little faster when it has too much in hand, slower when too little (+-0.6 %: a few cents at most)
            const float e = clampf (3.0f * (float) (avail - target) / (float) target, -1.0f, 1.0f);
            r.ratio += (1.0f + 0.006f * e - r.ratio) * minf (1.0f, (float) m / (2.0f * (float) RADIO_RATE));
        }
        else r.ratio = 1.0f;
        float lv = 0.0f;
        for (int i = 0; i < m; ++i)
        {
            float s = 0.0f;
            if (! r.priming)
            {
                if (avail < 3) { r.priming = 1; ++r.underruns; }
                else
                {
                    const short* f = r.fifo;
                    s = hermite (f[(rd - 1u) & FMASK], f[rd & FMASK], f[(rd + 1u) & FMASK], f[(rd + 2u) & FMASK], r.fpos);
                    r.fpos += r.ratio;
                    while (r.fpos >= 1.0f && avail > 0) { r.fpos -= 1.0f; ++rd; --avail; }
                }
            }
            // (the sound fades in and out with the signal, 10 ms: no click when the network runs dry)
            const float gT = r.priming || st != RS_LIVE ? 0.0f : 1.0f;
            r.gain += (gT - r.gain) * (1.0f / 240.0f);
            s *= r.gain;
            const int si = (int) (s > 32767.0f ? 32767.0f : s < -32767.0f ? -32767.0f : s);
            r.hist[(r.hw + i) & HMASK] = (short) si;
            const float a = absf (s) * (1.0f / 32768.0f);
            if (a > lv) lv = a;
            if (a > r.pkAcc) r.pkAcc = a;
            if (++r.pkN >= RADIO_PKLEN)
            {
                r.peak[r.pkW] = (unsigned char) (minf (1.0f, sqrtf_ (r.pkAcc)) * 255.0f);
                r.pkW = (r.pkW + 1) % RADIO_NPEAK; r.pkN = 0; r.pkAcc = 0.0f;
            }
        }
        r.hw += m;
        rdStore (&r.rd, (int) rd);
        r.sig += ((r.priming || st != RS_LIVE ? 0.0f : 1.0f) - r.sig) * sgA;
        r.level = maxf (lv, r.level * 0.995f);
        r.fill = (float) avail / (float) RADIO_RATE;
    }
}

// ---- the voice
void Engine::radioTrigger (Voice& v, const float* P)
{
    RadioState& s = v.rd;
    const int ti = v.ti;
    if (s.fresh)
    {
        // a new voice: its air from silence (the whistle and the fading somewhere of their own)
        for (int j = 0; j < 8; ++j) { s.ax[j][0] = s.ax[j][1] = s.ay[j][0] = s.ay[j][1] = 0.0f; }
        s.adl = 0.0f; s.shPh = 0.0f;
        for (int j = 0; j < 2; ++j) { s.bh1[j] = s.bh2[j] = s.bl1[j] = s.bl2[j] = 0.0f; s.fdN[j] = 0.0f; }
        s.fdA = s.fdB = 1.0f; s.fdPh = 1.0f; s.fdF = 900.0f;
        s.htPh = 0.0f; s.htF = 600.0f + 1400.0f * v.rng.uni(); s.htT = s.htF;
        s.ckE = 0.0f; s.ckLp = 0.0f; s.ckLeft = 0;
        s.pk[0] = s.pk[1] = s.pk[2] = 0.0f;
        s.sqE = 0.0f; s.sqG = P[P_D] > 0.005f ? 0.0f : 1.0f;
        s.dLp = 0.0f; s.dPh = 0.0f; s.swPh = 0.0f;
        s.fresh = 0;
    }
    s.mode = idxOf (P[P_B], RADIO_MODES);
    s.rx = idxOf (P[P_A], RADIO_RX);
    const RadioRx& r = radio.rx[s.rx];
    const double inc = (double) RADIO_RATE / (double) sr;
    const double behind = (double) CHUNK * inc + 8.0;              // (just behind what the chunk wrote)
    const float l16 = radioLen16 (idxOf (P[P_C], RADIO_LENS));
    const double sps = samplesPerStep (tr[ti]);
    s.live = -1.0f;
    if (s.mode == RM_LIVE)
    {
        s.p = (double) r.hw - behind;
        s.gateLeft = l16 > 0.0f ? (int) (l16 * sps) : -1;
        s.gate = 1.0f;
        return;
    }
    s.gateLeft = -1; s.gate = 1.0f;
    const double per = (l16 > 0.0f ? l16 : 16.0f) * sps;              // the loop's period (output samples)
    const int L = (int) clampf ((float) (per * inc), 64.0f, (float) RADIO_LOOP);
    const bool take = s.mode == RM_DIG || radLast[ti] < 0 || (P[P_D] > 0.0f && v.rng.uni() < P[P_D]);
    if (take)
    {
        const int b = radLast[ti] < 0 ? 0 : 1 - radLast[ti];
        double end = (double) r.hw - behind;
        if (s.mode == RM_DIG) end -= (double) (v.rng.uni() * radioDepth (P[P_D]) * (float) RADIO_RATE);
        double st = __builtin_floor (end) - (double) L;
        const double oldest = (double) r.hw - (double) (RADIO_HIST - 2 * RADIO_RATE);
        if (st < oldest) st = oldest;
        const int s0 = histIdx (st);
        short* dst = radLoop[ti][b];
        for (int i = 0; i < L; ++i) dst[i] = r.hist[(s0 + i) & HMASK];
        radLen[ti][b] = L; radLast[ti] = b;
        s.grabbed = 1;
    }
    else s.grabbed = 0;
    s.buf = radLast[ti];
    s.L = mini (L, radLen[ti][s.buf]);
    s.h[0].on = 0; s.h[1].on = 0;                                    // (the first sample starts a head)
    s.period = per; s.el = -1.0;
}

void Engine::radioRender (Voice& v, const float* P, float tuneSt, int n, float* out)
{
    RadioState& s = v.rd;
    const RadioRx& r = radio.rx[s.rx];
    const float isr = 1.0f / sr;
    const double inc = (double) RADIO_RATE / (double) sr;
    const float att = 1.0f / (0.002f * sr);
    const float dc = decayCoef (0.02f * powf_ (400.0f, P[P_DEC]), sr);
    const float* S = P + P_S1;                                         // (AIR: the first side places)
    const int st = rdLoad (&r.state);
    // TUNE: LIVE a shift (Hz), LOOP and DIG a transposition
    const float shHz = s.mode == RM_LIVE ? radioShiftHz (tuneSt) : 0.0f;
    const bool shift = absf (shHz) > 0.5f;
    const float rate = s.mode == RM_LIVE ? 1.0f : semis (tuneSt);
    // the air
    const float stat = P[P_E], hiss = 0.22f * stat * stat;
    const bool tuning = s.mode == RM_LIVE && (st == RS_CONNECT || st == RS_BUFFER || st == RS_TUNING);
    const float dead = tuning ? (0.06f + 0.25f * stat) * (1.0f - r.sig) : 0.0f;
    const float bandV = S[RA_BAND], fade = S[RA_FADE], het = S[RA_HET], crk = S[RA_CRKL], drv = S[RA_DRIV];
    const bool band = bandV > 0.01f;
    Svf h1, h2, l1, l2, nt;
    if (band)
    {
        const float bw = radioBandHz (bandV), c = radioCntrHz (S[RA_CNTR]);
        const float lo = maxf (40.0f, c - 0.5f * bw), hi = minf (c + 0.5f * bw, 0.45f * sr);
        h1.set (lo, 0.5412f, sr); h2.set (lo, 1.3066f, sr);
        l1.set (maxf (hi, lo * 1.2f), 0.5412f, sr); l2.set (maxf (hi, lo * 1.2f), 1.3066f, sr);
        h1.ic1 = s.bh1[0]; h1.ic2 = s.bh1[1]; h2.ic1 = s.bh2[0]; h2.ic2 = s.bh2[1];
        l1.ic1 = s.bl1[0]; l1.ic2 = s.bl1[1]; l2.ic1 = s.bl2[0]; l2.ic2 = s.bl2[1];
    }
    // the fading: a new depth to fade to every 1 / RATE seconds (deep ones rarer), a notch wandering with it
    const float fdInc = radioRateHz (S[RA_RATE]) * isr;
    if (fade > 0.002f) { nt.set (s.fdF, 1.2f, sr); nt.ic1 = s.fdN[0]; nt.ic2 = s.fdN[1]; }
    // the whistle wanders, slowly
    if (het > 0.002f)
    {
        if (v.rng.uni() < (float) n * isr * 0.3f) s.htT = 300.0f + 2200.0f * v.rng.uni();
        s.htF += (s.htT - s.htF) * minf (1.0f, (float) n * isr * 0.8f);
    }
    const float htInc = s.htF * isr, htLvl = 0.12f * het * het;
    const float ckRate = 60.0f * crk * crk * isr;
    const float dg = 1.0f + 10.0f * drv * drv, dmix = minf (1.0f, drv * 8.0f), dnorm = 1.0f / tanhf_ (dg);
    const float dlpA = 1.0f - expf_ (-TWO_PI * (5200.0f - 3000.0f * drv) * isr);
    // the squelch (LIVE): its threshold, the signal's envelope (2 ms up, 25 ms down: it shuts between words)
    const bool sql = s.mode == RM_LIVE && P[P_D] > 0.005f;
    const float sqThr = exp2f_ (radioSqlDb (P[P_D]) * (1.0f / 6.0206f));
    const float sqUp = 1.0f - expf_ (-isr / 0.002f), sqDn = 1.0f - expf_ (-isr / 0.025f);
    const float sgOpen = 1.0f / (0.002f * sr), sgClose = 1.0f / (0.03f * sr);
    const float gtDn = 1.0f / (0.006f * sr);
    const float shInc = shHz * isr;
    const short* lb = s.mode != RM_LIVE && s.buf >= 0 ? radLoop[v.ti][s.buf] : nullptr;
    const float pinc = (float) inc * rate;
    // (a seam's crossfade: 4 ms, a quarter of the loop at most)
    const float xf = s.mode == RM_LIVE ? 1.0f : maxf (8.0f, minf (0.004f * sr, minf ((float) s.period * 0.25f, (float) s.L / pinc * 0.25f)));
    const float xfInc = 1.0f / xf;
    float lvl = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float x = 0.0f;
        if (s.mode == RM_LIVE)
        {
            // (kept just behind what is written, inside the history)
            const double hi = (double) r.hw - 4.0, lo = (double) r.hw - (double) (RADIO_HIST - 4096);
            if (s.p > hi) s.p = hi;
            if (s.p < lo) s.p = lo;
            x = histAt (r, s.p);
            s.p += inc;
        }
        else if (lb && s.L > 0)
        {
            // the loop: from its start every period; its seams (its end, a new period) crossfaded between two heads
            const float Lf = (float) s.L;
            s.el += 1.0;
            const int mh = s.h[0].on && s.h[0].dg >= 0.0f ? 0 : (s.h[1].on && s.h[1].dg >= 0.0f ? 1 : -1);
            bool seam = mh < 0;
            if (s.el >= s.period) { s.el -= s.period; seam = true; }
            if (mh >= 0 && s.h[mh].pos + pinc * xf >= Lf) seam = true;
            if (seam)
            {
                const int nw = mh >= 0 ? 1 - mh : 0;
                if (mh >= 0) s.h[mh].dg = -xfInc;
                s.h[nw].pos = 0.0f; s.h[nw].g = 0.0f; s.h[nw].dg = xfInc; s.h[nw].on = 1;
            }
            for (int k = 0; k < 2; ++k)
            {
                RadioHead& h = s.h[k];
                if (! h.on) continue;
                const float g = h.g * h.g * (3.0f - 2.0f * h.g);
                x += loopAt (lb, s.L, h.pos) * g;
                h.pos += pinc;
                if (h.pos >= Lf) h.pos -= Lf;
                h.g += h.dg;
                if (h.g >= 1.0f) { h.g = 1.0f; h.dg = 0.0f; }
                if (h.g <= 0.0f) { h.g = 0.0f; h.on = 0; }
            }
            const int m2 = s.h[0].on && s.h[0].dg >= 0.0f ? 0 : 1;
            s.live = s.h[m2].pos / Lf;
        }
        // TUNE on LIVE: the signal moved up or down (a quarter turn apart: one chain a sample later)
        if (shift)
        {
            float a = x, b = s.adl; s.adl = x;
            for (int j = 0; j < 4; ++j)
            {
                const float ca = HA[j] * HA[j], cb = HB[j] * HB[j];
                const float ya = ca * (a + s.ay[j][1]) - s.ax[j][1];
                s.ax[j][1] = s.ax[j][0]; s.ax[j][0] = a; s.ay[j][1] = s.ay[j][0]; s.ay[j][0] = ya; a = ya;
                const float yb = cb * (b + s.ay[4 + j][1]) - s.ax[4 + j][1];
                s.ax[4 + j][1] = s.ax[4 + j][0]; s.ax[4 + j][0] = b; s.ay[4 + j][1] = s.ay[4 + j][0]; s.ay[4 + j][0] = yb; b = yb;
            }
            s.shPh += shInc; s.shPh -= floorf_ (s.shPh);
            x = a * cos2pi (s.shPh) + b * sin2pi (s.shPh);
        }
        // the squelch listens to the signal itself
        if (sql)
        {
            const float e = absf (x);
            s.sqE += (e - s.sqE) * (e > s.sqE ? sqUp : sqDn);
            const float gT = s.sqE > sqThr ? 1.0f : 0.0f;
            s.sqG += gT > s.sqG ? minf (sgOpen, gT - s.sqG) : -minf (sgClose, s.sqG - gT);
        }
        else s.sqG = 1.0f;
        // the fading
        if (fade > 0.002f)
        {
            s.fdPh += fdInc;
            if (s.fdPh >= 1.0f)
            {
                s.fdPh -= 1.0f; s.fdA = s.fdB;
                const float u = v.rng.uni();
                s.fdB = 1.0f - u * u;
                s.fdF = 300.0f * powf_ (10.0f, v.rng.uni());
                nt.set (s.fdF, 1.2f, sr);
            }
            const float w = s.fdPh * s.fdPh * (3.0f - 2.0f * s.fdPh);
            const float g = s.fdA + (s.fdB - s.fdA) * w;
            float lp, bp, hp; nt.tick (x, lp, bp, hp);
            x = (x - fade * nt.k * bp) * (1.0f - fade + fade * g);
        }
        // the whistle, the hiss (and the dial's sweep while it tunes), the crackle
        s.dPh += isr * 0.37f; s.dPh -= floorf_ (s.dPh);
        if (htLvl > 0.0f)
        {
            s.htPh += htInc * (1.0f + 0.003f * sin2pi (s.dPh)); s.htPh -= floorf_ (s.htPh);
            x += htLvl * sin2pi (s.htPh) * (0.4f + 0.6f * r.sig);
        }
        if (hiss > 0.0f || dead > 0.0f)
        {
            const float w = v.rng.bi();
            s.pk[0] = 0.997f * s.pk[0] + 0.03f * w; s.pk[1] = 0.96f * s.pk[1] + 0.12f * w; s.pk[2] = 0.6f * s.pk[2] + 0.3f * w;
            const float pn = (s.pk[0] + s.pk[1] + s.pk[2] + 0.2f * w) * 1.3f;
            x += pn * (hiss + dead);
            if (dead > 0.0f)
            {
                // the dial's sweep: a whistle falling through the band, again and again
                s.swPh += isr * 0.7f; s.swPh -= floorf_ (s.swPh);
                const float f = 3200.0f - 3000.0f * s.swPh;
                s.htPh += f * isr; s.htPh -= floorf_ (s.htPh);
                x += dead * 0.25f * sin2pi (s.htPh) * (1.0f - s.swPh);
            }
        }
        if (ckRate > 0.0f)
        {
            if (s.ckLeft <= 0 && v.rng.uni() < ckRate)
            {
                const float u = v.rng.uni();
                const bool crash = crk > 0.7f && u < 0.05f;
                s.ckLeft = (int) ((crash ? 0.25f + 0.3f * v.rng.uni() : 0.001f + 0.015f * u * u) * sr);
                s.ckE = (crash ? 0.5f : 0.15f + 0.7f * u * u * u) * (0.3f + 0.7f * crk);
            }
            if (s.ckLeft > 0)
            {
                --s.ckLeft;
                s.ckLp += (v.rng.bi() - s.ckLp) * 0.5f;
                x += s.ckLp * s.ckE;
                s.ckE *= 0.9993f;
            }
        }
        // the receiver's band
        if (band)
        {
            float lp, bp, hp;
            h1.tick (x, lp, bp, hp); h2.tick (hp, lp, bp, hp);
            l1.tick (hp, lp, bp, hp); l2.tick (lp, lp, bp, hp);
            x = lp;
        }
        // the detector and the speaker pushed
        if (dmix > 0.0f)
        {
            const float y = tanhf_ (dg * (x + 0.25f * drv * x * x)) * dnorm;
            s.dLp += (y - s.dLp) * dlpA;
            x += (s.dLp - x) * dmix;
        }
        // LEN (LIVE): the gate, shut after its length
        if (s.gateLeft >= 0)
        {
            if (s.gateLeft > 0) --s.gateLeft;
            else if (s.gate > 0.0f) s.gate = maxf (0.0f, s.gate - gtDn);
        }
        const float e = ampTickR (v, att, dc);
        const float o = x * 0.8f * e * s.gate * s.sqG;
        out[i] = o;
        lvl = maxf (lvl, e * s.gate);
    }
    if (band) { s.bh1[0] = h1.ic1; s.bh1[1] = h1.ic2; s.bh2[0] = h2.ic1; s.bh2[1] = h2.ic2; s.bl1[0] = l1.ic1; s.bl1[1] = l1.ic2; s.bl2[0] = l2.ic1; s.bl2[1] = l2.ic2; }
    if (fade > 0.002f) { s.fdN[0] = nt.ic1; s.fdN[1] = nt.ic2; }
    for (int j = 0; j < 8; ++j) if (isBad (s.ay[j][0])) { for (int q = 0; q < 8; ++q) s.ay[q][0] = s.ay[q][1] = s.ax[q][0] = s.ax[q][1] = 0.0f; break; }
    if (isBad (s.bh1[0]) || isBad (s.bl2[1]) || isBad (s.fdN[0])) { for (int j = 0; j < 2; ++j) s.bh1[j] = s.bh2[j] = s.bl1[j] = s.bl2[j] = s.fdN[j] = 0.0f; }
    v.level = lvl;
    v.ns += (u32) n;
}

// ---- the screens
// a track (RADIO_INFO): its receiver, mode, state, signal, level, seconds in hand, speed, dropouts, the loop (seconds,
// where it plays), the voice's level, the S-meter (dBm, -999 none), the squelch, the history's peaks (oldest first)
void Engine::radioInfo (int ti, float* out) const
{
    for (int i = 0; i < RADIO_INFO; ++i) out[i] = 0.0f;
    if (ti < 0 || ti >= NUM_TRACKS) return;
    const Track& t = tr[ti];
    const int k = idxOf (t.p[P_A], RADIO_RX);
    const RadioRx& r = radio.rx[k];
    out[0] = (float) k; out[1] = (float) idxOf (t.p[P_B], RADIO_MODES);
    out[2] = (float) rdLoad (&r.state); out[3] = r.sig; out[4] = r.level; out[5] = r.fill; out[6] = r.ratio;
    out[7] = (float) r.underruns; out[9] = -1.0f;
    const int mc = rdLoad (&r.meterC);
    out[11] = mc == -32768 ? -999.0f : (float) mc * 0.01f;
    const Voice& v = t.voice[t.cur];
    if (v.active && v.machine == M_RADIO)
    {
        out[8] = v.rd.mode != RM_LIVE ? (float) v.rd.L / (float) RADIO_RATE : 0.0f;
        out[9] = v.rd.live; out[10] = v.level; out[12] = v.rd.sqG;
    }
    out[13] = (float) RADIO_NPEAK;
    for (int i = 0; i < RADIO_NPEAK; ++i) out[RADIO_HEAD + i] = (float) r.peak[(r.pkW + i) % RADIO_NPEAK] * (1.0f / 255.0f);
}
// the four receivers (RADIO_RX_INFO each): state, signal, level, seconds in hand, speed, dropouts, S-meter, waiting
void Engine::radioRxInfo (float* out) const
{
    for (int k = 0; k < RADIO_RX; ++k)
    {
        const RadioRx& r = radio.rx[k];
        float* o = out + k * RADIO_RX_INFO;
        const int mc = rdLoad (&r.meterC);
        o[0] = (float) rdLoad (&r.state); o[1] = r.sig; o[2] = r.level; o[3] = r.fill; o[4] = r.ratio;
        o[5] = (float) r.underruns; o[6] = mc == -32768 ? -999.0f : (float) mc * 0.01f; o[7] = (float) r.priming;
    }
}

} // namespace sk
