// IMPAKT — organic one-shot bank, synthesised at start-up (see sk_org.h).
#include "sk_org.h"

namespace sk {

namespace {

const float SRF = (float) ORG_SR;

inline float lnf (float x) { return log2f_ (x) * LN2; }

enum { BP = 0, LP = 1, HP = 2 };
enum { K_MODE = 0, K_GRAIN, K_CLICK, K_POP, K_DROP };

// A stream of random events (crackling noise): Poisson arrivals whose rate follows an
// attack / decay envelope, event sizes drawn from a power law (many tiny ones, a few big ones).
struct Crk
{
    float t0, dur, att, dec;   // start, length, rate envelope attack and decay (s)
    float rate;                // events per second at the envelope peak
    float alpha, amin, ascale; // size = amin * u^(-1/alpha), clipped at 1, then scaled
    int   kind;
    float f1, f2;              // frequency range of an event (log-uniform)
    float tanphi;              // material internal friction: mode decay = 1 / (pi f tanphi)
    int   nm;                  // modes per modal event
    float g1, g2;              // noise grain decay range (ms)
    float q;                   // noise grain filter Q
    float clk;                 // click on modal events
};

Crk crk (float t0, float dur, float att, float dec, float rate, int kind, float f1, float f2)
{
    Crk c;
    c.t0 = t0; c.dur = dur; c.att = att; c.dec = dec; c.rate = rate; c.kind = kind; c.f1 = f1; c.f2 = f2;
    c.alpha = 1.5f; c.amin = 0.05f; c.ascale = 1.0f; c.tanphi = 0.03f; c.nm = 2;
    c.g1 = 0.2f; c.g2 = 1.0f; c.q = 1.0f; c.clk = 0.3f;
    return c;
}

// (each random draw in a statement of its own, left to right: the arguments of a call, the operands of an operator,
// are evaluated in an order C++ leaves to the compiler, and every build has to make the same sounds)
struct Gen
{
    float* h; int L; Rng r;

    float u()  { return r.uni(); }
    float b()  { return r.bi(); }
    float sgn() { return r.next() & 1u ? 1.0f : -1.0f; }
    float rnd (float a, float c)  { return a + (c - a) * r.uni(); }
    float lrnd (float a, float c) { return a * exp2f_ (log2f_ (c / a) * r.uni()); }
    float vr (float x) { return 1.0f + x * r.bi(); }
    int   ri (int n) { int k = (int) (r.uni() * (float) n); return k < n ? k : n - 1; }
    int   at (float t) { return (int) (t * SRF + 0.5f); }

    // one resonant mode: amp * e^(-t/tau) * sin(2 pi f t)
    void mode (float t0, float amp, float f, float tau)
    {
        if (f < 20.0f || f > 0.45f * SRF || absf (amp) < 1e-12f) return;
        const int s = at (t0); if (s < 0 || s >= L) return;
        tau = maxf (tau, 0.00015f);
        const float lim = lnf (absf (amp) / 2e-4f);
        if (lim <= 0.0f) return;
        int n = (int) (tau * SRF * lim); if (n > L - s) n = L - s;
        const float rr = expf_ (-1.0f / (tau * SRF));
        const float c = rr * cos2pi (f / SRF), sn = rr * sin2pi (f / SRF);
        float zr = amp, zi = 0.0f;
        float* o = h + s;
        for (int i = 0; i < n; ++i) { o[i] += zi; const float nr = zr * c - zi * sn; zi = zr * sn + zi * c; zr = nr; }
    }
    // one mode excited by a list of impulses (times in samples): a single pass over the buffer
    void modeImp (const int* at_, const float* amp, int n, float f, float tau)
    {
        if (n <= 0 || f < 20.0f || f > 0.45f * SRF) return;
        const float rr = expf_ (-1.0f / (maxf (tau, 0.00015f) * SRF));
        const float c = rr * cos2pi (f / SRF), sn = rr * sin2pi (f / SRF);
        float zr = 0.0f, zi = 0.0f;
        int k = 0;
        for (int i = at_[0]; i < L; ++i)
        {
            while (k < n && at_[k] == i) zr += amp[k++];
            h[i] += zi;
            const float nr = zr * c - zi * sn; zi = zr * sn + zi * c; zr = nr;
            if (k >= n && absf (zr) + absf (zi) < 1e-6f) break;
        }
    }
    // mode with a material damping (van den Doel: decay rate proportional to frequency)
    void modeM (float t0, float amp, float f, float tanphi) { mode (t0, amp, f, 1.0f / (PI * f * tanphi)); }

    // derivative-of-Gaussian click; sigma in samples sets the brightness (1 -> ~7 kHz, 3 -> ~2.5 kHz)
    void click (float t0, float amp, float sigma)
    {
        sigma = maxf (sigma, 0.35f);
        const int half = (int) (sigma * 4.0f) + 1, c = at (t0);
        const float k = amp / 0.60653066f, is = 1.0f / sigma;
        for (int i = -half; i <= half; ++i)
        {
            const int j = c + i; if (j < 0 || j >= L) continue;
            const float x = (float) i * is;
            h[j] -= k * x * expf_ (-0.5f * x * x);
        }
    }

    // filtered noise burst with linear attack, exponential decay and an optional exponential sweep
    void noise (float t0, float amp, float att, float dec, float fc, float q, int type, float fc2 = 0.0f)
    {
        const int s = at (t0); if (s < 0 || s >= L) return;
        const int na = maxi (1, at (att));
        dec = maxf (dec, 0.00005f);
        int n = na + (int) (dec * SRF * 7.0f); if (n > L - s) n = L - s;
        Svf f; f.reset(); f.set (fc, q, SRF);
        const int   sweeping = fc2 > 0.0f;
        const float sweep = sweeping ? log2f_ (fc2 / fc) : 0.0f;
        const float sweepLen = 1.0f / (dec * SRF * 3.0f + 1.0f);
        const float dcf = expf_ (-1.0f / (dec * SRF));
        float env = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            if (sweeping && (i & 15) == 0) f.set (fc * exp2f_ (sweep * minf (1.0f, (float) i * sweepLen)), q, SRF);
            if (i < na) env = (float) (i + 1) / (float) na; else env *= dcf;
            float lp, bp, hp; f.tick (b(), lp, bp, hp);
            const float y = type == BP ? bp * f.k : (type == LP ? lp : hp);
            h[s + i] += amp * env * y;
        }
    }

    // decaying sine whose frequency moves linearly: f(t) = f0 (1 + rise t)
    void chirp (float t0, float amp, float f0, float rise, float tau)
    {
        const int s = at (t0); if (s < 0 || s >= L) return;
        int n = (int) (tau * SRF * 8.0f); if (n > L - s) n = L - s;
        const float dcf = expf_ (-1.0f / (tau * SRF));
        float env = amp, ph = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float f = clampf (f0 * (1.0f + rise * (float) i / SRF), 20.0f, 0.45f * SRF);
            ph += f / SRF; ph -= floorf_ (ph);
            h[s + i] += env * sin2pi (ph);
            env *= dcf;
        }
    }

    // Minnaert bubble with the damping and rising pitch of van den Doel's liquid model
    void bubble (float t0, float amp, float f0, float xi)
    {
        const float d = 0.043f * f0 + 0.0014f * f0 * sqrtf_ (f0);
        chirp (t0, amp, f0, xi * d, 1.5f / d);
    }

    void shell (float t0, float amp, float f1, float f2, int n, float tanphi)
    {
        for (int k = 0; k < n; ++k) { const float ra = rnd (0.35f, 1.0f), rb = sgn(), rc = lrnd (f1, f2); modeM (t0, amp * ra * rb, rc, tanphi); }
    }

    // free-free bar modes (twigs, branches, wood blocks)
    void bar (float t0, float amp, float f1, float tanphi, int n)
    {
        static const float R[6] = { 1.0f, 2.756f, 5.404f, 8.933f, 13.34f, 18.64f };
        static const float A[6] = { 1.0f, 0.62f, 0.42f, 0.27f, 0.17f, 0.1f };
        for (int k = 0; k < n && k < 6; ++k) { const float ra = vr (0.3f), rb = vr (0.03f); modeM (t0, amp * A[k] * ra, f1 * R[k] * rb, tanphi); }
    }

    void event (const Crk& c, float t, float a)
    {
        switch (c.kind)
        {
            case K_MODE:
                for (int m = 0; m < c.nm; ++m) { const float ra = m == 0 ? 1.0f : rnd (0.3f, 0.8f), rb = lrnd (c.f1, c.f2); modeM (t, a * ra, rb, c.tanphi); }
                if (c.clk > 0.0f) click (t, a * c.clk, rnd (0.4f, 1.0f));
                break;
            case K_GRAIN:
                { const float ra = rnd (c.g1, c.g2), rb = lrnd (c.f1, c.f2); noise (t, a, 0.00004f, ra * 0.001f, rb, c.q, BP); }
                break;
            case K_CLICK:
                click (t, a, rnd (0.35f, 0.9f));
                modeM (t, a * 0.5f, lrnd (c.f1, c.f2), c.tanphi);
                break;
            case K_POP:
                click (t, a, rnd (0.6f, 1.6f));
                modeM (t, a * 0.7f, lrnd (c.f1, c.f2), c.tanphi);
                if (absf (a) > 0.35f) noise (t, a * 0.8f, 0.0003f, 0.006f, rnd (180.0f, 320.0f), 0.7f, LP);
                break;
            default: // K_DROP: impact, sometimes a bubble
                click (t, a * 0.8f, rnd (0.5f, 1.5f));
                if (u() < 0.3f) { const float ra = lrnd (c.f1, c.f2), rb = rnd (0.03f, 0.1f); bubble (t + 0.0008f, a * 0.8f, ra, rb); }
                break;
        }
    }

    void crackle (const Crk& c)
    {
        const int s = at (c.t0); if (s < 0 || s >= L) return;
        int n = at (c.dur); if (n > L - s) n = L - s;
        const int na = maxi (1, at (c.att));
        const float dcf = expf_ (-1.0f / (maxf (c.dec, 0.0001f) * SRF));
        const float p0 = c.rate / SRF, ia = -1.0f / c.alpha;
        float env = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            if (i < na) env = (float) (i + 1) / (float) na; else env *= dcf;
            if (u() >= p0 * env) continue;
            float a = c.amin * powf_ (maxf (u(), 1e-6f), ia);
            if (a > 1.0f) a = 1.0f;
            event (c, c.t0 + (float) i / SRF, a * c.ascale * sgn());
        }
    }

    // Cook's PhISEM: a decaying "shake energy" drives random collisions, each collision adds
    // energy to a fast-decaying noise excitation that rings a few resonances (re-tuned per collision)
    void phisem (float t0, float dur, float att, float sysTau, float prob, float sndTau,
                 int nres, const float* fr, float q, float fjit, float amp)
    {
        const int s = at (t0); if (s < 0 || s >= L) return;
        int n = at (dur); if (n > L - s) n = L - s;
        Svf rs[3];
        for (int k = 0; k < nres; ++k) { rs[k].reset(); rs[k].set (fr[k], q, SRF); }
        const int na = maxi (1, at (att));
        const float sysC = expf_ (-1.0f / (sysTau * SRF)), sndC = expf_ (-1.0f / (sndTau * SRF));
        float E = 0.0f, lev = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            if (i < na) E = (float) (i + 1) / (float) na; else E *= sysC;
            if (u() < prob)
            {
                lev += E * rnd (0.3f, 1.0f);
                if (fjit > 0.0f) for (int k = 0; k < nres; ++k) rs[k].set (fr[k] * vr (fjit), q, SRF);
            }
            const float x = lev * b();
            lev *= sndC;
            float y = 0.0f;
            for (int k = 0; k < nres; ++k) { float lp, bp, hp; rs[k].tick (x, lp, bp, hp); y += bp * rs[k].k; }
            h[s + i] += amp * y;
        }
    }

    // DC block, loudness match (peak and loudest 50 ms), trim the silent tail, fade out
    int finish()
    {
        float x1 = 0.0f, y1 = 0.0f;
        const float R = 1.0f - TWO_PI * 20.0f / SRF;
        float pk = 0.0f;
        for (int i = 0; i < L; ++i)
        {
            const float x = h[i], y = x - x1 + R * y1; x1 = x; y1 = y; h[i] = y;
            if (absf (y) > pk) pk = absf (y);
        }
        if (pk < 1e-9f) return 64;
        const int W = 2400;
        double acc = 0.0, best = 0.0;
        for (int i = 0; i < L; ++i)
        {
            acc += (double) h[i] * (double) h[i];
            if (i >= W) acc -= (double) h[i - W] * (double) h[i - W];
            if (acc > best) best = acc;
        }
        const float rms = sqrtf_ ((float) (best / (double) mini (W, L)));
        float gn = 0.8f / pk;
        if (rms * gn > 0.2f) gn = 0.2f / rms;
        for (int i = 0; i < L; ++i) h[i] *= gn;
        int end = L;
        while (end > 64 && absf (h[end - 1]) < 2e-4f) --end;
        end = mini (L, end + 96);
        const int fl = mini (192, end / 4);
        for (int i = 0; i < fl; ++i) h[end - 1 - i] *= (float) i / (float) fl;
        return end;
    }
};

// seconds per sound (upper bound: silent tails are trimmed)
const float DUR[NUM_ORG] = {
    0.22f, 0.75f, 0.25f, 0.7f, 0.5f, 0.3f,      // RAMEAU BRANCHE BOIS BAMBOU FEUILLES GRAINES
    0.4f, 0.35f, 0.28f, 0.25f, 0.35f,           // CHIPS BISCOTTE CELERI COQUILLE NOIX
    0.7f, 0.65f,                                // FEU ALLUMETTE
    0.45f, 0.35f, 0.4f, 0.5f, 0.5f, 0.8f,       // GRAVIER CAILLOUX SABLE NEIGE GLACON VERRE
    0.25f, 0.6f, 0.7f, 0.5f, 0.3f,              // GOUTTE BULLES PLUIE PSCHITT BOUCHON
    0.6f, 0.5f, 0.15f, 0.45f, 0.45f,            // PAPIER SACHET P.BULLE ZIP VELCRO
    0.2f, 0.18f, 0.12f, 0.15f,                  // CLAP DOIGTS LANGUE PHALANGE
    0.9f, 0.7f, 0.4f };                         // PINGPONG PIECES GRILLON

void recipe (int id, Gen& g)
{
    switch (id)
    {
    case 0: // RAMEAU — twig snap: a few fibre clicks, the break, wood bar modes, torn fibres
    {
        const float tm = g.rnd (0.02f, 0.045f);
        const int npre = g.ri (4);
        for (int k = 0; k < npre; ++k)
        {
            const float t = g.rnd (0.002f, tm - 0.004f), a = g.rnd (0.08f, 0.25f);
            g.click (t, a, g.rnd (0.5f, 1.0f)); g.shell (t, a * 0.6f, 2500.0f, 7000.0f, 2, 0.03f);
        }
        g.click (tm, 1.0f, g.rnd (0.8f, 1.6f));
        g.bar (tm, 0.9f, g.lrnd (800.0f, 1700.0f), 0.035f, 5);
        g.noise (tm, 0.5f, 0.0002f, 0.004f, g.rnd (4000.0f, 8000.0f), 0.7f, BP);
        const int nf = 1 + g.ri (3);
        for (int k = 0; k < nf; ++k)
        {
            const float t = tm + g.rnd (0.003f, 0.025f), a = g.rnd (0.15f, 0.45f);
            g.click (t, a, g.rnd (0.6f, 1.2f)); g.shell (t, a * 0.7f, 1500.0f, 5000.0f, 2, 0.04f);
        }
        break;
    }
    case 1: // BRANCHE — creaking that accelerates, the big crack, aftershocks, leaves
    {
        const float tb = g.rnd (0.28f, 0.34f);
        Crk c = crk (0.0f, tb, tb, 0.05f, g.rnd (180.0f, 300.0f), K_MODE, 500.0f, 3000.0f);
        c.tanphi = 0.04f; c.alpha = 1.6f; c.amin = 0.03f; c.ascale = 0.5f; g.crackle (c);
        g.click (tb, 1.0f, g.rnd (1.2f, 2.0f));
        const float f1 = g.lrnd (240.0f, 420.0f);
        g.bar (tb, 1.0f, f1, 0.03f, 5);
        g.bar (tb + 0.002f, 0.5f, f1 * g.rnd (2.0f, 2.6f), 0.035f, 4);
        g.noise (tb, 0.7f, 0.0005f, 0.02f, 2500.0f, 0.5f, BP);
        g.noise (tb, 0.5f, 0.001f, 0.05f, 350.0f, 0.7f, LP);
        Crk a = crk (tb + 0.004f, 0.3f, 0.001f, 0.06f, 300.0f, K_MODE, 800.0f, 4000.0f);
        a.tanphi = 0.04f; a.amin = 0.04f; a.ascale = 0.7f; g.crackle (a);
        Crk l = crk (tb + 0.02f, 0.4f, 0.03f, 0.12f, 700.0f, K_GRAIN, 3000.0f, 9000.0f);
        l.amin = 0.02f; l.ascale = 0.25f; l.g1 = 0.1f; l.g2 = 0.6f; g.crackle (l);
        break;
    }
    case 2: // BOIS — wood block knock, sometimes a second bounce
    {
        const float f1 = g.lrnd (450.0f, 950.0f);
        g.click (0.002f, 0.6f, g.rnd (1.0f, 2.0f));
        g.bar (0.002f, 1.0f, f1, 0.012f, 4);
        g.mode (0.002f, 0.4f, f1 * 0.52f, 0.012f);
        if (g.u() < 0.35f)
        {
            const float t2 = g.rnd (0.05f, 0.1f);
            g.click (t2, 0.3f, 1.5f); g.bar (t2, 0.45f, f1 * g.vr (0.02f), 0.012f, 4);
        }
        break;
    }
    case 3: // BAMBOU — bamboo chimes: collisions crowd at the start, three tube resonances
    {
        const float base[3] = { 2800.0f * g.vr (0.15f), 2240.0f * g.vr (0.15f), 3360.0f * g.vr (0.15f) };
        const int n = 6 + g.ri (6);
        for (int k = 0; k < n; ++k)
        {
            const float t = k == 0 ? 0.002f : 0.002f + 0.45f * g.u() * g.u();
            const float a = expf_ (-t / 0.18f) * g.rnd (0.4f, 1.0f);
            const int bi_ = g.ri (3); const float f = base[bi_] * g.vr (0.04f);
            g.click (t, a * 0.3f, 0.8f);
            g.modeM (t, a, f, 0.0022f);
            g.modeM (t, a * 0.25f, f * 2.76f, 0.003f);
        }
        break;
    }
    case 4: // FEUILLES — dry leaves crushed: dense paper-like grains, a few crisp snaps
    {
        const float ra = g.rnd (0.02f, 0.06f), rb = g.rnd (0.1f, 0.18f);
        Crk c = crk (0.0f, 0.48f, ra, rb, 2500.0f, K_GRAIN, 1500.0f, 10000.0f);
        c.amin = 0.03f; c.g1 = 0.2f; c.g2 = 1.2f; c.q = 1.5f; g.crackle (c);
        Crk s = crk (0.0f, 0.4f, 0.02f, 0.12f, 120.0f, K_MODE, 3000.0f, 9000.0f);
        s.tanphi = 0.05f; s.nm = 1; s.amin = 0.1f; s.ascale = 0.6f; g.crackle (s);
        g.noise (0.0f, 0.08f, 0.03f, 0.12f, 5000.0f, 0.6f, BP);
        break;
    }
    case 5: // GRAINES — seed shaker (PhISEM maraca), sometimes a back-shake
    {
        const float fr[2] = { 3200.0f * g.vr (0.12f), 5200.0f * g.vr (0.1f) };
        const float ra = g.rnd (0.008f, 0.02f), rb = g.rnd (0.04f, 0.07f);
        g.phisem (0.0f, 0.3f, ra, rb, 0.012f, 0.0009f, 2, fr, 3.0f, 0.0f, 1.0f);
        if (g.u() < 0.5f) g.phisem (g.rnd (0.11f, 0.15f), 0.17f, 0.01f, 0.04f, 0.01f, 0.0009f, 2, fr, 3.0f, 0.0f, 0.6f);
        break;
    }
    case 6: // CHIPS — crisps crunched: brittle thin-shell fracture cascades in two bites
    {
        const float t0 = 0.003f;
        g.click (t0, 0.9f, 0.8f); g.shell (t0, 0.7f, 2000.0f, 8000.0f, 3, 0.02f);
        Crk c = crk (t0, 0.35f, 0.008f, g.rnd (0.05f, 0.09f), 1500.0f, K_MODE, 1800.0f, 9000.0f);
        c.tanphi = 0.025f; c.alpha = 1.4f; c.amin = 0.05f; c.clk = 0.4f; g.crackle (c);
        Crk d = c; d.t0 = g.rnd (0.1f, 0.18f); d.dur = 0.2f; d.rate = 900.0f; d.dec = 0.05f; d.ascale = 0.6f; g.crackle (d);
        Crk gr = crk (t0, 0.3f, 0.005f, 0.06f, 1500.0f, K_GRAIN, 3000.0f, 11000.0f);
        gr.ascale = 0.3f; gr.g1 = 0.1f; gr.g2 = 0.5f; g.crackle (gr);
        break;
    }
    case 7: // BISCOTTE — toasted cracker: drier, lower, with a body thump
    {
        g.click (0.002f, 0.8f, 1.2f);
        g.noise (0.002f, 0.6f, 0.0005f, 0.02f, 900.0f, 0.8f, BP);
        g.noise (0.002f, 0.35f, 0.001f, 0.03f, 250.0f, 0.7f, LP);
        Crk c = crk (0.002f, 0.3f, 0.004f, 0.08f, 700.0f, K_MODE, 900.0f, 5000.0f);
        c.tanphi = 0.04f; c.alpha = 1.3f; c.amin = 0.08f; c.clk = 0.5f; g.crackle (c);
        Crk gr = crk (0.002f, 0.3f, 0.004f, 0.07f, 1000.0f, K_GRAIN, 1500.0f, 7000.0f);
        gr.ascale = 0.4f; gr.g1 = 0.5f; gr.g2 = 2.0f; gr.q = 0.8f; g.crackle (gr);
        break;
    }
    case 8: // CELERI — wet snap: a squeak, the snap, damped watery fibres
    {
        g.chirp (0.0f, 0.06f, g.rnd (1100.0f, 1600.0f), 8.0f, 0.015f);
        const float ts = g.rnd (0.02f, 0.035f);
        g.click (ts, 1.0f, g.rnd (1.2f, 2.0f));
        g.noise (ts, 0.9f, 0.0003f, 0.012f, g.rnd (1400.0f, 2200.0f), 1.2f, BP);
        g.shell (ts, 0.4f, 700.0f, 2500.0f, 3, 0.07f);
        Crk f = crk (ts + 0.003f, 0.12f, 0.002f, 0.03f, 600.0f, K_GRAIN, 1000.0f, 4000.0f);
        f.g1 = 0.3f; f.g2 = 1.5f; f.q = 2.0f; f.ascale = 0.6f; g.crackle (f);
        break;
    }
    case 9: // COQUILLE — eggshell: a few brittle cracks, the cavity, crumbs
    {
        const int n = 2 + g.ri (3);
        float t = 0.002f;
        for (int k = 0; k < n; ++k)
        {
            const float a = k == 0 ? 1.0f : g.rnd (0.3f, 0.9f);
            g.click (t, a * 0.7f, g.rnd (0.4f, 0.8f)); g.shell (t, a, 3000.0f, 11000.0f, 3, 0.012f);
            t += g.rnd (0.006f, 0.02f);
        }
        const float fc = g.lrnd (800.0f, 1300.0f);
        g.mode (0.002f, 0.3f, fc, 0.012f); g.mode (0.002f, 0.15f, fc * 1.7f, 0.008f);
        Crk c = crk (0.004f, 0.15f, 0.002f, 0.04f, 200.0f, K_MODE, 4000.0f, 12000.0f);
        c.tanphi = 0.02f; c.amin = 0.04f; c.ascale = 0.4f; c.nm = 1; g.crackle (c);
        break;
    }
    case 10: // NOIX — walnut: stress creaks, hard-shell crack, falling fragments
    {
        const float tb = g.rnd (0.09f, 0.13f);
        Crk c = crk (0.0f, tb, tb, 0.05f, 150.0f, K_MODE, 1500.0f, 5000.0f);
        c.tanphi = 0.015f; c.amin = 0.05f; c.ascale = 0.4f; g.crackle (c);
        g.click (tb, 1.0f, g.rnd (0.8f, 1.4f));
        g.shell (tb, 0.9f, 1200.0f, 4500.0f, 4, 0.008f);
        g.mode (tb, 0.5f, g.lrnd (150.0f, 230.0f), 0.02f);
        g.noise (tb, 0.4f, 0.0003f, 0.008f, 3000.0f, 0.7f, BP);
        const int nf = 2 + g.ri (4);
        for (int k = 0; k < nf; ++k)
        {
            const float t = tb + g.rnd (0.01f, 0.15f), a = g.rnd (0.08f, 0.3f);
            g.click (t, a, 0.6f); g.shell (t, a, 2000.0f, 7000.0f, 2, 0.01f);
        }
        break;
    }
    case 11: // FEU — fire: power-law pops with resin bursts, sizzle, low roar
    {
        g.click (0.003f, 0.9f, 1.0f); g.modeM (0.003f, 0.6f, g.lrnd (1500.0f, 4000.0f), 0.08f);
        g.noise (0.003f, 0.7f, 0.0003f, 0.006f, 250.0f, 0.7f, LP);
        Crk c = crk (0.0f, 0.7f, 0.001f, 10.0f, g.rnd (25.0f, 45.0f), K_POP, 1500.0f, 6000.0f);
        c.alpha = 1.2f; c.amin = 0.08f; c.tanphi = 0.08f; g.crackle (c);
        Crk s = crk (0.0f, 0.7f, 0.001f, 10.0f, 400.0f, K_GRAIN, 4000.0f, 12000.0f);
        s.amin = 0.01f; s.ascale = 0.15f; s.g1 = 0.1f; s.g2 = 0.4f; g.crackle (s);
        g.noise (0.0f, 0.05f, 0.05f, 1.0f, 900.0f, 0.5f, LP);
        break;
    }
    case 12: // ALLUMETTE — match: scratch, ignition crackle, whoosh, flame sizzle
    {
        Crk sc = crk (0.0f, 0.12f, 0.04f, 0.04f, 4000.0f, K_GRAIN, 2000.0f, 8000.0f);
        sc.alpha = 1.8f; sc.amin = 0.05f; sc.g1 = 0.05f; sc.g2 = 0.4f; sc.q = 0.8f; g.crackle (sc);
        g.noise (0.0f, 0.25f, 0.05f, 0.03f, 4000.0f, 0.7f, BP);
        const float ti = g.rnd (0.08f, 0.12f);
        Crk ig = crk (ti, 0.15f, 0.002f, 0.03f, 600.0f, K_MODE, 2000.0f, 8000.0f);
        ig.tanphi = 0.05f; ig.amin = 0.05f; ig.ascale = 0.8f; g.crackle (ig);
        g.noise (ti, 0.6f, 0.02f, 0.15f, 500.0f, 0.6f, BP, 1400.0f);
        g.noise (ti, 0.3f, 0.03f, 0.2f, 200.0f, 0.7f, LP);
        Crk fl = crk (ti + 0.05f, 0.45f, 0.01f, 0.2f, 300.0f, K_GRAIN, 3000.0f, 10000.0f);
        fl.ascale = 0.15f; fl.amin = 0.02f; g.crackle (fl);
        break;
    }
    case 13: // GRAVIER — footstep in gravel (PhISEM little rocks) with a heel thud
    {
        const float fr[3] = { g.lrnd (2000.0f, 3000.0f), g.lrnd (3000.0f, 4500.0f), g.lrnd (4500.0f, 6500.0f) };
        const float ra = g.rnd (0.02f, 0.04f), rb = g.rnd (0.09f, 0.15f);
        g.phisem (0.0f, 0.42f, ra, rb, 0.05f, 0.0015f, 3, fr, 2.0f, 0.25f, 1.0f);
        g.noise (0.0f, 0.5f, 0.008f, 0.04f, 120.0f, 0.7f, LP);
        Crk p = crk (0.0f, 0.3f, 0.02f, 0.1f, 60.0f, K_MODE, 1500.0f, 4000.0f);
        p.tanphi = 0.01f; p.amin = 0.1f; p.ascale = 0.6f; g.crackle (p);
        break;
    }
    case 14: // CAILLOUX — two pebbles knocking and bouncing
    {
        float fa[4], fb[4];
        for (int k = 0; k < 4; ++k) { fa[k] = g.lrnd (2200.0f, 7000.0f); fb[k] = g.lrnd (1800.0f, 6000.0f); }
        float t = 0.002f, T = g.rnd (0.05f, 0.09f), a = 1.0f;
        const int n = 2 + g.ri (4);
        for (int k = 0; k < n && t < 0.3f; ++k)
        {
            const float* f = g.u() < 0.5f ? fa : fb;
            g.click (t, a * 0.7f, g.rnd (0.4f, 0.7f));
            for (int m = 0; m < 4; ++m) { const float ra = g.rnd (0.3f, 1.0f), rb = g.vr (0.01f); g.modeM (t, a * ra, f[m] * rb, 0.004f); }
            t += T * g.rnd (0.7f, 1.2f); T *= 0.6f; a *= g.rnd (0.4f, 0.75f);
        }
        break;
    }
    case 15: // SABLE — sandpaper / sand scrape: stick-slip micro grains under a gesture
    {
        const float ra = g.rnd (0.08f, 0.14f), rb = g.rnd (0.06f, 0.12f);
        Crk c = crk (0.0f, 0.38f, ra, rb, 6000.0f, K_GRAIN, 2000.0f, 9000.0f);
        c.alpha = 2.0f; c.amin = 0.05f; c.g1 = 0.05f; c.g2 = 0.3f; c.q = 0.8f; g.crackle (c);
        g.noise (0.0f, 0.15f, 0.1f, 0.1f, 3500.0f, 0.8f, BP, 5000.0f);
        break;
    }
    case 16: // NEIGE — snow crunch under a boot: low damped crackles, compression body
    {
        const float ra = g.rnd (0.04f, 0.08f), rb = g.rnd (0.09f, 0.14f);
        Crk c = crk (0.0f, 0.45f, ra, rb, 1600.0f, K_MODE, 450.0f, 1800.0f);
        c.tanphi = 0.1f; c.nm = 1; c.amin = 0.03f; c.alpha = 1.6f; c.clk = 0.2f; g.crackle (c);
        Crk d = c; d.kind = K_GRAIN; d.rate = 1500.0f; d.f1 = 800.0f; d.f2 = 4000.0f; d.g1 = 0.3f; d.g2 = 1.0f; d.ascale = 0.4f; g.crackle (d);
        g.noise (0.0f, 0.25f, 0.04f, 0.08f, 250.0f, 0.7f, LP);
        break;
    }
    case 17: // GLACON — ice cube in a glass: clink, rebounds, cracking fizz
    {
        static const float RR[5] = { 1.0f, 1.47f, 2.09f, 2.56f, 3.3f };
        float fm[5];
        const float f1 = g.lrnd (2000.0f, 3400.0f);
        for (int k = 0; k < 5; ++k) fm[k] = f1 * RR[k] * g.vr (0.05f);
        float t = 0.002f, a = 1.0f;
        const int nr = 2 + g.ri (2);
        for (int hit = 0; hit < nr; ++hit)
        {
            g.click (t, a * 0.6f, 0.6f);
            for (int k = 0; k < 5; ++k) g.modeM (t, a * g.rnd (0.3f, 1.0f), fm[k], 0.0012f);
            t += g.rnd (0.03f, 0.08f); a = g.rnd (0.15f, 0.4f);
        }
        Crk c = crk (0.01f, 0.35f, 0.01f, 0.08f, 400.0f, K_CLICK, 5000.0f, 12000.0f);
        c.tanphi = 0.01f; c.amin = 0.02f; c.ascale = 0.25f; g.crackle (c);
        break;
    }
    case 18: // VERRE — glass clink: bowl modes as beating doublets
    {
        static const float RR[4] = { 1.0f, 2.65f, 4.9f, 7.9f };
        static const float AA[4] = { 1.0f, 0.5f, 0.3f, 0.15f };
        const float f1 = g.lrnd (1100.0f, 2300.0f);
        g.click (0.001f, 0.3f, 0.5f);
        for (int k = 0; k < 4; ++k)
        {
            const float f = f1 * RR[k] * g.vr (0.02f), a = AA[k] * g.vr (0.2f);
            g.modeM (0.001f, a, f, 0.0004f);
            g.modeM (0.001f, a * 0.5f, f * 1.0025f, 0.0004f);
        }
        break;
    }
    case 19: // GOUTTE — water drop: impact, rising bubble, sometimes a droplet
    {
        g.click (0.002f, 0.15f, 1.0f);
        { const float ra = g.lrnd (700.0f, 2000.0f), rb = g.rnd (0.04f, 0.12f); g.bubble (0.0028f, 1.0f, ra, rb); }
        if (g.u() < 0.5f) { const float ra = g.rnd (0.06f, 0.12f), rb = g.rnd (0.15f, 0.35f), rc = g.lrnd (1800.0f, 4000.0f); g.bubble (ra, rb, rc, 0.1f); }
        break;
    }
    case 20: // BULLES — bubbling: many bubbles, small ones more frequent
    {
        const int n = 8 + g.ri (9);
        for (int k = 0; k < n; ++k)
        {
            const float t = k == 0 ? 0.002f : 0.002f + 0.5f * g.u() * g.u();
            const float f0 = g.lrnd (500.0f, 4500.0f);
            const float ra = g.rnd (0.3f, 1.0f), rb = g.rnd (0.03f, 0.12f);
            g.bubble (t, ra * (1.2f - f0 / 6000.0f), f0, rb);
        }
        break;
    }
    case 21: // PLUIE — rain spatter: impacts, some with bubbles, soft hiss
    {
        g.click (0.002f, 0.7f, 0.8f);
        Crk c = crk (0.0f, 0.68f, 0.001f, 10.0f, g.rnd (70.0f, 110.0f), K_DROP, 1500.0f, 5000.0f);
        c.alpha = 1.3f; c.amin = 0.1f; g.crackle (c);
        g.noise (0.0f, 0.05f, 0.02f, 2.0f, 6000.0f, 0.5f, BP);
        break;
    }
    case 22: // PSCHITT — can opening: tab click, gas release, fizz
    {
        g.click (0.003f, 0.5f, 0.7f); g.shell (0.003f, 0.5f, 2000.0f, 6000.0f, 4, 0.002f);
        const float tp = g.rnd (0.012f, 0.025f);
        g.click (tp, 1.0f, 1.2f);
        { const float ra = g.rnd (0.09f, 0.15f), rb = g.rnd (4500.0f, 6500.0f); g.noise (tp, 0.9f, 0.0005f, ra, rb, 0.8f, BP, 3000.0f); }
        Crk f = crk (tp + 0.005f, 0.45f, 0.02f, 0.18f, 900.0f, K_CLICK, 4000.0f, 12000.0f);
        f.tanphi = 0.02f; f.amin = 0.02f; f.ascale = 0.35f; g.crackle (f);
        break;
    }
    case 23: // BOUCHON — cork: twist squeak, pop with a falling neck resonance, gas
    {
        g.chirp (0.0f, 0.05f, g.rnd (600.0f, 900.0f), 4.0f, 0.015f);
        const float tp = g.rnd (0.02f, 0.03f);
        g.click (tp, 1.0f, g.rnd (1.6f, 2.6f));
        { const float ra = g.lrnd (380.0f, 650.0f), rb = g.rnd (0.01f, 0.016f); g.chirp (tp, 0.9f, ra, -6.0f, rb); }
        g.noise (tp, 0.3f, 0.0005f, 0.04f, 2500.0f, 0.7f, BP);
        break;
    }
    case 24: // PAPIER — paper crumpled in two or three gestures (power-law crumpling events)
    {
        const int ng = 2 + g.ri (2);
        float t = 0.0f;
        for (int k = 0; k < ng; ++k)
        {
            const float ra = g.rnd (0.01f, 0.03f), rb = g.rnd (0.04f, 0.08f);
            Crk c = crk (t, 0.18f, ra, rb, 1800.0f, K_GRAIN, 800.0f, 7000.0f);
            c.alpha = 1.3f; c.amin = 0.03f; c.g1 = 0.3f; c.g2 = 3.0f; c.q = 0.7f; c.ascale = k == 0 ? 1.0f : g.rnd (0.5f, 0.9f);
            g.crackle (c);
            t += g.rnd (0.12f, 0.2f);
        }
        Crk p = crk (0.0f, 0.55f, 0.01f, 0.25f, 150.0f, K_MODE, 1000.0f, 4000.0f);
        p.tanphi = 0.05f; p.amin = 0.1f; p.ascale = 0.6f; p.nm = 1; g.crackle (p);
        break;
    }
    case 25: // SACHET — plastic bag crinkle: ringing thin-film pings
    {
        const float ra = g.rnd (0.02f, 0.05f), rb = g.rnd (0.1f, 0.2f);
        Crk c = crk (0.0f, 0.48f, ra, rb, 900.0f, K_MODE, 2500.0f, 11000.0f);
        c.tanphi = 0.012f; c.alpha = 1.4f; c.amin = 0.04f; c.clk = 0.2f; g.crackle (c);
        Crk d = crk (0.0f, 0.45f, 0.03f, 0.15f, 600.0f, K_GRAIN, 3000.0f, 10000.0f);
        d.ascale = 0.4f; d.g1 = 0.2f; d.g2 = 1.0f; g.crackle (d);
        break;
    }
    case 26: // P.BULLE — bubble wrap: squeeze creak, then the pop
    {
        Crk cr = crk (0.0f, 0.02f, 0.02f, 0.01f, 200.0f, K_MODE, 2000.0f, 6000.0f);
        cr.amin = 0.02f; cr.ascale = 0.15f; cr.nm = 1; g.crackle (cr);
        const float tp = 0.02f;
        g.click (tp, 1.0f, g.rnd (0.8f, 1.3f));
        { const float ra = g.rnd (0.004f, 0.008f), rb = g.lrnd (2000.0f, 4000.0f); g.noise (tp, 0.6f, 0.0001f, ra, rb, 1.5f, BP); }
        g.mode (tp, 0.4f, g.lrnd (1200.0f, 2500.0f), 0.004f);
        g.shell (tp, 0.2f, 4000.0f, 9000.0f, 2, 0.01f);
        break;
    }
    case 27: // ZIP — zipper: teeth clicks whose rate follows the pull gesture
    {
        const float D = g.rnd (0.3f, 0.42f), r0 = g.rnd (120.0f, 200.0f), r1 = g.rnd (450.0f, 750.0f);
        const float fa = g.lrnd (3000.0f, 5000.0f), fb = g.lrnd (1100.0f, 1700.0f);
        float ph = 0.0f;
        const int n = g.at (D);
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / SRF, s = sin2pi (0.5f * t / D);
            ph += (r0 + (r1 - r0) * s) / SRF;
            if (ph < 1.0f) continue;
            ph = -0.25f * g.u();   // teeth are not perfectly regular
            const float a = (0.5f + 0.5f * s) * g.rnd (0.6f, 1.0f);
            g.click (t, a, g.rnd (0.4f, 0.8f));
            g.mode (t, a * 0.6f, fa * g.vr (0.1f), 0.0015f);
            g.mode (t, a * 0.4f, fb * g.vr (0.1f), 0.002f);
        }
        break;
    }
    case 28: // VELCRO — hooks tearing off: very dense micro snaps and a rasp
    {
        const float ra = g.rnd (0.04f, 0.08f), rb = g.rnd (0.15f, 0.25f);
        Crk c = crk (0.0f, 0.43f, ra, rb, 5000.0f, K_CLICK, 2000.0f, 6000.0f);
        c.alpha = 1.8f; c.amin = 0.05f; c.tanphi = 0.15f; g.crackle (c);
        g.noise (0.0f, 0.15f, 0.06f, 0.2f, 1800.0f, 1.0f, BP);
        break;
    }
    case 29: // CLAP — one hand clap: the air cavity between the palms as a resonant noise burst
    {
        const float fc = g.lrnd (800.0f, 2200.0f), q = g.rnd (2.0f, 6.0f), dec = g.rnd (0.006f, 0.012f);
        g.click (0.001f, 0.4f, 1.0f);
        g.noise (0.001f, 1.0f, 0.0004f, dec, fc, q, BP);
        g.noise (0.001f, 0.25f, 0.0006f, 0.012f, 400.0f, 0.7f, LP);
        break;
    }
    case 30: // DOIGTS — finger snap: sharp release click, finger-on-palm resonance, thump
    {
        g.click (0.001f, 1.0f, 0.5f);
        g.noise (0.001f, 0.7f, 0.0002f, 0.004f, g.lrnd (2200.0f, 3200.0f), 1.5f, BP);
        g.mode (0.0015f, 0.5f, g.lrnd (1500.0f, 2200.0f), 0.006f);
        g.noise (0.002f, 0.3f, 0.0005f, 0.012f, 300.0f, 0.7f, LP);
        break;
    }
    case 31: // LANGUE — tongue click: oral cavity resonances
    {
        const float f1 = g.lrnd (600.0f, 1300.0f);
        g.click (0.001f, 0.3f, 1.5f);
        g.mode (0.001f, 1.0f, f1, g.rnd (0.006f, 0.01f));
        g.mode (0.001f, 0.4f, f1 * g.rnd (2.2f, 2.7f), 0.004f);
        g.noise (0.001f, 0.2f, 0.0002f, 0.003f, f1 * 1.5f, 2.0f, BP);
        break;
    }
    case 32: // PHALANGE — knuckle crack: one to three cavitation pops
    {
        const int n = 1 + g.ri (3);
        float t = 0.002f;
        for (int k = 0; k < n; ++k)
        {
            const float a = g.rnd (0.6f, 1.0f);
            g.click (t, a, g.rnd (0.6f, 1.0f));
            g.mode (t, a * 0.8f, g.lrnd (1200.0f, 2800.0f), 0.003f);
            g.mode (t, a * 0.6f, g.lrnd (600.0f, 1100.0f), 0.004f);
            g.mode (t, a * 0.22f, g.lrnd (140.0f, 260.0f), 0.008f);
            t += g.rnd (0.004f, 0.03f);
        }
        break;
    }
    case 33: // PINGPONG — ball bouncing on a table: geometric intervals, celluloid + board modes
    {
        const float f1 = g.lrnd (2100.0f, 2800.0f), ft = g.lrnd (320.0f, 480.0f), e = g.rnd (0.72f, 0.82f);
        float t = 0.002f, T = g.rnd (0.16f, 0.26f), a = 1.0f;
        while (t < 0.86f && T > 0.012f)
        {
            g.click (t, a * 0.5f, 0.5f);
            g.modeM (t, a, f1, 0.008f); g.modeM (t, a * 0.5f, f1 * 1.62f, 0.009f); g.modeM (t, a * 0.3f, f1 * 2.71f, 0.01f);
            g.mode (t, a * 0.4f, ft, 0.02f); g.mode (t, a * 0.2f, ft * 2.3f, 0.012f);
            t += T; T *= e; a *= e;
        }
        break;
    }
    case 34: // PIECES — coin dropped on a table, rattling faster as it settles (Euler's disk)
    {
        static const float RR[5] = { 1.0f, 1.42f, 2.13f, 2.83f, 3.55f };
        const float f1 = g.lrnd (3500.0f, 6000.0f);
        int at_[24]; float am[5][24];
        int n = 0;
        float t = 0.002f, T = g.rnd (0.08f, 0.12f), a = 1.0f;
        for (; n < 24 && t < 0.6f; ++n)
        {
            at_[n] = g.at (t);
            for (int m = 0; m < 5; ++m) am[m][n] = a * g.rnd (0.2f, 1.0f) * (m == 0 ? 1.0f : 0.6f);
            g.click (t, a * 0.4f, 0.5f);
            g.mode (t, a * 0.3f, 320.0f * g.vr (0.1f), 0.008f);
            t += T; T = maxf (T * 0.78f, 0.004f); a *= 0.86f;
        }
        for (int m = 0; m < 5; ++m)
        {
            const float f = f1 * RR[m] * g.vr (0.02f);
            g.modeImp (at_, am[m], n, f, 1.0f / (PI * f * 0.0003f));
        }
        break;
    }
    default: // 35 GRILLON — cricket chirp: a few sine pulses near 4-5 kHz
    {
        const float fc = g.lrnd (3800.0f, 5000.0f);
        const int np = 3 + g.ri (3);
        const float per = g.rnd (0.026f, 0.036f), plen = g.rnd (0.012f, 0.018f);
        for (int p = 0; p < np; ++p)
        {
            const int s = g.at (0.003f + (float) p * per), n = g.at (plen);
            float ph = g.u();
            const float amp = 1.0f - 0.1f * (float) p * g.u();
            for (int i = 0; i < n && s + i < g.L; ++i)
            {
                const float x = (float) i / (float) n, w = sin2pi (0.5f * x);
                ph += fc * (1.0f - 0.03f * x) / SRF; ph -= floorf_ (ph);
                g.h[s + i] += amp * w * w * (sin2pi (ph) + 0.15f * sin2pi (2.0f * ph));
            }
        }
        break;
    }
    }
}

} // namespace

void OrgBank::build()
{
    used = 0;
    for (int id = 0; id < NUM_ORG; ++id)
        for (int v = 0; v < ORG_VAR; ++v)
        {
            Gen g;
            g.h = pool + used;
            g.L = mini ((int) (DUR[id] * SRF), ORG_POOL - used);
            off[id][v] = used; len[id][v] = 0;
            if (g.L < 256) continue;
            for (int i = 0; i < g.L; ++i) g.h[i] = 0.0f;
            g.r.seed (0x51EDu + (u32) id * 7919u + (u32) v * 104729u);
            for (int k = 0; k < 4; ++k) g.r.next();
            recipe (id, g);
            const int L = g.finish();
            len[id][v] = L;
            used += L;
        }
}

} // namespace sk
