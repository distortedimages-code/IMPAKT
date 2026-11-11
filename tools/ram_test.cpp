// RAM machine checks (native): takes line up with the steps, every source records what it should, PLAY trigs play
// the take (forward, reversed, from another track's buffer), the threshold, overdub, the input and its monitor, long
// takes at a lower rate, a muted track still records, long host blocks keep the input in place.
//   ram_test            -> PASS / FAIL lines, exit code 1 on a failure
#include "../core/sk_engine.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <functional>
#include <algorithm>
#include <chrono>

using namespace sk;

static int fails = 0;
static void ok (const char* name, bool c, const char* fmt = "", double a = 0, double b = 0)
{
    printf ("%s %s", c ? "PASS" : "FAIL", name);
    if (*fmt) { printf ("  ("); printf (fmt, a, b); printf (")"); }
    printf ("\n");
    if (! c) fails++;
}

static const float SR = 48000.0f;
static const int STEP = 6000;               // samples per step at 120 BPM

// a clean engine: every track silent and empty, internal transport stopped, master neutral
static void fresh (Engine& e)
{
    e.init (SR);
    for (int t = 0; t < NUM_TRACKS; ++t) { e.setParam (t, P_VOL, 0.0f); e.setParam (t, P_REV, 0.0f); e.setParam (t, P_DLY, 0.0f); }
    e.setGlobal (G_BPM, 120.0f); e.setGlobal (G_SWING, 0.0f);
    e.setGlobal (G_MXON, 0.0f);
}
// a track as a RAM machine at its UI defaults (PLAY: whole take, forward, HOLD, no slices, no DAC)
static void ramTrack (Engine& e, int t)
{
    e.setParam (t, P_MACH, machineVal (M_RAM));
    e.setParam (t, P_TUNE, 0.5f); e.setParam (t, P_DEC, 1.0f);
    e.setParam (t, P_A, 0.0f); e.setParam (t, P_B, 1.0f); e.setParam (t, P_C, 0.5f / RAM_MODES);
    e.setParam (t, P_D, 0.5f / RAM_SLICES); e.setParam (t, P_E, 0.5f / NUM_DAC_MODELS); e.setParam (t, P_F, 0.25f);
    e.setParam (t, P_FREQ, 1.0f); e.setParam (t, P_RES, 0.0f); e.setParam (t, P_VEL, 1.0f); e.setParam (t, P_PAN, 0.5f);
    e.setParam (t, P_ATK, 0.0f);
}
static float takeIdx (int i, int n) { return ((float) i + 0.5f) / (float) n; }
static void setSrc (Engine& e, int t, int src) { e.setParam (t, P_RSRC, takeIdx (src, RAM_SRCS)); }
static void setTake (Engine& e, int t, int idx) { e.setParam (t, P_RLEN, takeIdx (idx, RAM_TAKES)); }

// render n samples in host blocks of `blk`, with an optional input generator; returns the left output
static std::vector<float> run (Engine& e, int n, int blk = 128, std::function<float (long)> in = nullptr, long* clock = nullptr)
{
    std::vector<float> L (n), R (n), I (blk);
    long c0 = clock ? *clock : 0;
    for (int i = 0; i < n; i += blk)
    {
        const int k = std::min (blk, n - i);
        if (in) { if ((int) I.size() < k) I.resize (k); for (int j = 0; j < k; ++j) I[j] = in (c0 + i + j); e.setInput (I.data(), I.data()); }
        e.process (L.data() + i, R.data() + i, k, nullptr);
    }
    if (clock) *clock = c0 + n;
    return L;
}
static float peak (const std::vector<float>& x, int a = 0, int b = -1)
{
    if (b < 0) b = (int) x.size();
    float p = 0; for (int i = a; i < b; ++i) p = std::max (p, std::fabs (x[i])); return p;
}
// frequency of a (nearly pure) tone from its upward zero crossings
static double toneHz (const std::vector<float>& x, int a, int b)
{
    int first = -1, last = -1, n = 0;
    for (int i = a + 1; i < b; ++i) if (x[i - 1] < 0.0f && x[i] >= 0.0f) { if (first < 0) first = i; last = i; ++n; }
    return n > 1 ? (double) (n - 1) * 48000.0 / (double) (last - first) : 0.0;
}
static int lastLoud (const std::vector<float>& x, float thr) { for (int i = (int) x.size() - 1; i >= 0; --i) if (std::fabs (x[i]) > thr) return i; return -1; }
// a take written straight into a buffer
static void loadTake (Engine& e, int t, int n, std::function<float (int)> f)
{
    for (int i = 0; i < n; ++i) e.ramMem[t][i] = (short) std::lrint (std::max (-1.0f, std::min (1.0f, f (i))) * 32767.0f);
    e.ramLoaded (t, n, SR);
}
static void prism (Engine& e, int t, float mix, int voices)
{
    e.setParam (t, P_PMIX, mix); e.setParam (t, P_PVOI, takeIdx (voices - 1, PR_K));
    e.setParam (t, P_PBLR, 0.0f); e.setParam (t, P_PGLD, 0.0f); e.setParam (t, P_PCTR, 0.5f); e.setParam (t, P_PWID, 0.5f); e.setParam (t, P_PSPD, 0.5f);
}

// amplitude of the component at hz over [a, b) (Goertzel on a Hann window, 1 for a full-scale sine)
static double ampAt (const std::vector<float>& x, int a, int b, double hz)
{
    const int n = b - a; const double w = 2.0 * M_PI * hz / 48000.0, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0, wsum = 0;
    for (int i = 0; i < n; ++i) { const double h = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / n); wsum += h; const double s0 = x[a + i] * h + c * s1 - s2; s2 = s1; s1 = s0; }
    return 2.0 * std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / wsum;
}
static int firstLoud (const short* m, int n, int thr = 300) { for (int i = 0; i < n; ++i) if (std::abs (m[i]) > thr) return i; return -1; }

int main()
{
    auto* E = new Engine();
    Engine& e = *E;

    // 1. a take of T1 (a kick on step 1) by T2 (REC trig on step 1, 4 steps): it lines up with the kick, and nothing
    //    plays without a PLAY trig
    {
        fresh (e);
        e.setParam (0, P_MACH, machineVal (M_BD_VA)); e.setParam (0, P_VOL, 0.8f); e.setTrig (0, 0, 1);
        ramTrack (e, 1); e.setParam (1, P_VOL, 0.8f);
        setSrc (e, 1, 2); setTake (e, 1, 3);           // T1, 4 steps
        e.setRec (1, 0, 1);
        e.setPlay (1);
        auto out = run (e, 4 * STEP + 2000);
        const int len = e.ram[1].len;
        ok ("take length = 4 steps, version moved", len == 4 * STEP && e.ram[1].ver == 1, "len %.0f, ver %.0f", len, e.ram[1].ver);
        // no PLAY trig: T2 is silent (T1 alone plays): render T1 alone and compare
        Engine* ref = new Engine(); fresh (*ref);
        ref->setParam (0, P_MACH, machineVal (M_BD_VA)); ref->setParam (0, P_VOL, 0.8f); ref->setTrig (0, 0, 1); ref->setPlay (1);
        auto r = run (*ref, 4 * STEP + 2000);
        // where the kick rises above a tenth of its peak: in the take, LATENCY + the master's oversampling (86.5
        // samples) earlier than in the output
        std::vector<float> tk (len); for (int i = 0; i < len; ++i) tk[i] = e.ramMem[1][i] / 32768.0f;
        auto rise = [] (const std::vector<float>& x) { const float p = peak (x); for (size_t i = 0; i < x.size(); ++i) if (std::fabs (x[i]) > 0.1f * p) return (int) i; return -1; };
        const int at = rise (tk), atOut = rise (r);
        ok ("the take starts with the kick (latency of the source compensated)", at >= 0 && std::abs (at - (atOut - LATENCY - OS4_DELAY_X2 / 2)) <= 1, "take %.0f, output %.0f", at, atOut);
        double d = 0; for (size_t i = 0; i < r.size(); ++i) d = std::max (d, (double) std::fabs (r[i] - out[i]));
        ok ("REC trigs alone make no sound", d < 1e-6, "max difference %.2g", d);
        delete ref;
    }

    // 2. the PLAY trig plays the take: T2 plays on step 1 of the next bar what it recorded on step 1 (T1 muted then)
    {
        e.setMute (0, 1); e.setRec (1, 0, 0); e.setTrig (1, 0, 1);
        auto out = run (e, 16 * STEP);
        // the PLAY trig at step 1 of bar 2: 12 steps after where we are (we are at step 4 + 2000 samples)
        const int bar2 = 16 * STEP - (4 * STEP + 2000);
        ok ("PLAY trig plays the take", peak (out, bar2, bar2 + STEP) > 0.05f && peak (out, 0, bar2 - 200) < 1e-4f,
            "peak in the step %.3f, before %.2g", peak (out, bar2, bar2 + STEP), peak (out, 0, bar2 - 200));
    }

    // 3. IN: the input is recorded as it comes (LEVL 0 dB), in place across long host blocks (split by the engine)
    {
        fresh (e); ramTrack (e, 2); setSrc (e, 2, RAM_IN); setTake (e, 2, 0);   // 1 step
        auto sine = [] (long c) { return 0.5f * (float) std::sin (2.0 * M_PI * 440.0 * (double) c / 48000.0); };
        long clk = 0;
        run (e, 1000, 1000, sine, &clk);
        e.ramStart (2, -1);
        const long start = clk;
        run (e, STEP + 3000, 3000, sine, &clk);                                 // 3000-sample blocks: split at 1024
        double err = 0; for (int i = 0; i < STEP; ++i) err = std::max (err, std::fabs (e.ramMem[2][i] / 32768.0 - sine (start + i)));
        ok ("IN records the input sample for sample (long blocks too)", e.ram[2].len == STEP && err < 1e-4, "len %.0f, err %.2g", e.ram[2].len, err);
    }

    // 4. THRS: the take waits for the input to pass the threshold
    {
        fresh (e); ramTrack (e, 2); setSrc (e, 2, RAM_IN); setTake (e, 2, 0);
        e.setParam (2, P_RTHR, 0.5f);                                           // -33 dBFS
        auto burst = [] (long c) { return c >= 5000 ? 0.3f * (float) std::sin (2.0 * M_PI * 220.0 * c / 48000.0) : 0.001f; };
        long clk = 0;
        e.ramStart (2, -1);
        run (e, 4000, 128, burst, &clk);
        const bool waiting = e.ram[2].state == RAM_ARMED && e.ram[2].wpos == 0;
        run (e, 2 * STEP, 128, burst, &clk);
        const double first = e.ramMem[2][0] / 32768.0;
        ok ("THRS holds the take until the source passes it", waiting && e.ram[2].len == STEP && std::fabs (first) > 0.0224,
            "first sample %.4f, len %.0f", first, e.ram[2].len);
    }

    // 5. MONI: the input goes through the track; without it, no input in the output
    {
        fresh (e); ramTrack (e, 2); setSrc (e, 2, RAM_IN); e.setParam (2, P_VOL, 0.8f);
        auto sine = [] (long c) { return 0.5f * (float) std::sin (2.0 * M_PI * 440.0 * c / 48000.0); };
        auto a = run (e, 4800, 128, sine);
        e.setParam (2, P_RMON, 1.0f);
        auto b = run (e, 4800, 128, sine);
        ok ("MONI lets the input through the track", peak (a) < 1e-6f && peak (b, 2400) > 0.1f, "off %.2g, on %.3f", peak (a), peak (b, 2400));
    }

    // 6. a muted RAM track still records; MAIN records the output as heard
    {
        fresh (e);
        e.setParam (0, P_MACH, machineVal (M_SY_RAW)); e.setParam (0, P_VOL, 0.8f); e.setParam (0, P_DEC, 1.0f); e.setTrig (0, 0, 1);
        ramTrack (e, 3); setSrc (e, 3, RAM_MAIN); setTake (e, 3, 1); e.setRec (3, 0, 1); e.setMute (3, 1);
        e.setPlay (1);
        auto out = run (e, 2 * STEP + 500);
        // the take (from MAIN, mono) against the output (left; the synth is panned centre): the take starts with the
        // sound of the step, which leaves the engine LATENCY + 86.5 samples after it (BOX, master oversampling)
        const int D = LATENCY + (OS4_DELAY_X2 + 1) / 2;
        double err = 0, ref = 0;
        for (int i = 0; i < 2 * STEP - 300; ++i) { err = std::max (err, std::fabs (e.ramMem[3][i] / 32768.0 - out[i + D])); ref = std::max (ref, (double) std::fabs (out[i + D])); }
        ok ("a muted RAM track records; MAIN = the output", e.ram[3].len == 2 * STEP && ref > 0.05 && err < 2e-3, "peak %.3f, err %.2g", ref, err);
    }

    // 7. REV plays the take backwards; BUF plays another track's buffer; LOOP keeps playing
    {
        fresh (e);
        // a take: a tone, then silence
        for (int i = 0; i < STEP; ++i) e.ramMem[4][i] = i < STEP / 2 ? (short) (16000.0 * std::sin (2.0 * M_PI * 1000.0 * i / 48000.0)) : 0;
        e.ramLoaded (4, STEP, SR);
        ramTrack (e, 4); e.setParam (4, P_VOL, 1.0f);
        ramTrack (e, 5); e.setParam (5, P_VOL, 1.0f);
        e.setParam (5, P_RBUF, takeIdx (5, NUM_TRACKS + 1));                    // T5's buffer (T5 = index 4 = BUF 5)
        e.setParam (5, P_C, takeIdx (1, RAM_MODES));                            // REV
        e.audition (5, 1.0f);
        auto out = run (e, STEP + 1000);
        // backwards: the silence first, then the tone
        const float p1 = peak (out, 0, STEP / 2 - 100), p2 = peak (out, STEP / 2 + 200, STEP);
        ok ("BUF + REV: another track's buffer, backwards", p1 < 1e-3f && p2 > 0.2f, "first half %.2g, second %.3f", p1, p2);
        e.setParam (4, P_C, takeIdx (2, RAM_MODES)); e.setParam (4, P_B, 0.1f);  // LOOP over a tenth of the take
        e.audition (4, 1.0f);
        auto lp = run (e, 4 * STEP);
        ok ("LOOP keeps playing past the window", peak (lp, 3 * STEP) > 0.01f, "late peak %.3f", peak (lp, 3 * STEP));
    }

    // 8. OVDB: a second take over the first keeps it; a long take goes to a lower rate
    {
        fresh (e); ramTrack (e, 2); setSrc (e, 2, RAM_IN); setTake (e, 2, 0);
        auto dc = [] (long) { return 0.25f; };
        e.ramStart (2, -1); run (e, STEP + 128, 128, dc);
        e.setParam (2, P_ROVD, 1.0f);
        e.ramStart (2, -1); run (e, STEP + 128, 128, dc);
        const double v = e.ramMem[2][100] / 32768.0;
        ok ("OVDB at 100 % adds the new take to the old one", std::fabs (v - 0.5) < 1e-3, "%.4f", v);
        e.setGlobal (G_BPM, 20.0f);
        e.setParam (2, P_ROVD, 0.0f); setTake (e, 2, RAM_TAKES - 1);           // 64 steps at 20 BPM: 192 s
        float L[64], R[64]; e.process (L, R, 64, nullptr);
        e.ramStart (2, -1);
        ok ("a take too long for the buffer goes to a lower rate", e.ram[2].div == 8 && e.ram[2].rate == SR / 8 && e.ram[2].total <= RAM_LEN,
            "div %.0f, total %.0f", e.ram[2].div, e.ram[2].total);
    }

    // 9. the converter model changes the playback (SP-12 against none)
    {
        auto play = [&] (int dacModel) {
            fresh (e);
            for (int i = 0; i < STEP; ++i) e.ramMem[0][i] = (short) (12000.0 * std::sin (2.0 * M_PI * 3000.0 * i / 48000.0));
            e.ramLoaded (0, STEP, SR);
            ramTrack (e, 0); e.setParam (0, P_VOL, 1.0f); e.setParam (0, P_E, takeIdx (dacModel, NUM_DAC_MODELS));
            e.audition (0, 1.0f);
            return run (e, 4000);
        };
        auto a = play (DAC_OFF), b = play (DAC_SP12);
        double d = 0; for (int i = 500; i < 4000; ++i) d = std::max (d, (double) std::fabs (a[i] - b[i]));
        ok ("DAC: SP-12 changes the sound", d > 0.01 && peak (b) > 0.1f, "difference %.3f", d);
    }

    // 10. stability: random settings with takes running from every source, no NaN, no runaway level
    {
        fresh (e); Rng rng; rng.seed (7);
        bool bad = false; float worst = 0;
        for (int it = 0; it < 60; ++it)
        {
            for (int t = 0; t < 4; ++t)
            {
                ramTrack (e, t);
                for (int p = P_TUNE; p <= P_E; ++p) e.setParam (t, p, rng.uni());
                for (int p = P_RSRC; p <= P_RBUF; ++p) e.setParam (t, p, rng.uni());
                e.setParam (t, P_VOL, 0.6f); e.setRec (t, (int) (rng.uni() * 16), 1); e.setTrig (t, (int) (rng.uni() * 16), 1);
            }
            e.setPlay (1);
            auto out = run (e, 9000, 37 + it * 13, [&] (long c) { return 0.3f * (float) std::sin (c * 0.05); });
            for (float x : out) { if (! std::isfinite (x)) bad = true; worst = std::max (worst, std::fabs (x)); }
        }
        ok ("random RAM settings: no NaN, level bounded", ! bad && worst <= 1.0f, "worst %.3f", worst);
    }

    // ---------------------------------------------------------------- PRISM
    auto sine = [] (double hz, double a) { return [hz, a] (int i) { return (float) (a * std::sin (2.0 * M_PI * hz * i / 48000.0)); }; };

    // 11. a take of a 440 Hz tone, resynthesised: 440 Hz, about the level of the sample; TUNE +12 = 880 Hz for the same
    //     length (time and pitch apart); SPED x2 = half the length; SPED at 0 holds the sound
    {
        auto play = [&] (float tune, float spd, int len) {
            fresh (e); loadTake (e, 0, 48000, sine (440.0, 0.5));
            ramTrack (e, 0); e.setParam (0, P_VOL, 1.0f); prism (e, 0, 1.0f, 1);
            e.setParam (0, P_TUNE, tune); e.setParam (0, P_PSPD, spd); e.setParam (0, P_DEC, 1.0f);
            run (e, 6000);                                         // the analysis catches up (2 frames a chunk)
            const int analysed = e.ram[0].prNext;
            e.audition (0, 1.0f);
            auto out = run (e, len);
            return std::make_pair (out, analysed);
        };
        auto a = play (0.5f, 0.5f, 72000);
        const double hz = toneHz (a.first, 4800, 38400), pk = peak (a.first, 4800, 38400);
        ok ("PRISM: the take analysed in the background, 440 Hz played back", a.second >= 48000 / PR_HOP && std::fabs (hz - 440.0) < 1.5 && pk > 0.25,
            "%.1f Hz, peak %.3f", hz, pk);
        auto b = play (0.5f + 12.0f / 48.0f, 0.5f, 72000);
        const double hzb = toneHz (b.first, 4800, 38400);
        const int endA = lastLoud (a.first, 0.01f), endB = lastLoud (b.first, 0.01f);
        ok ("PRISM: TUNE +12 transposes (880 Hz) without changing the length", std::fabs (hzb - 880.0) < 3.0 && std::abs (endA - endB) < 2000,
            "%.1f Hz, length %.0f samples", hzb, endB);
        auto c2 = play (0.5f, 0.5f + 1.0f / 6.0f, 72000);
        const int endC = lastLoud (c2.first, 0.01f);
        ok ("PRISM: SPED x2 plays the take in half the time, same pitch", std::abs (endC - endA / 2) < 3000 && std::fabs (toneHz (c2.first, 4800, 19200) - 440.0) < 2.0,
            "length %.0f against %.0f", endC, endA);
        auto d = play (0.5f, 0.0f, 3 * 48000);
        ok ("PRISM: SPED at 0 freezes the sound (still there after 3 s)", peak (d.first, 2 * 48000, 3 * 48000) > 0.25, "peak %.3f", peak (d.first, 2 * 48000, 3 * 48000));
    }

    // 12. the band (CNTR / WDTH) keeps the partials inside it
    {
        auto play = [&] (double centreHz) {
            fresh (e); loadTake (e, 0, 48000, [] (int i) { return (float) (0.4 * std::sin (2.0 * M_PI * 220.0 * i / 48000.0) + 0.4 * std::sin (2.0 * M_PI * 3520.0 * i / 48000.0)); });
            ramTrack (e, 0); e.setParam (0, P_VOL, 1.0f); prism (e, 0, 1.0f, 8);
            e.setParam (0, P_PCTR, (float) (std::log2 (centreHz / 20.0) / 10.0)); e.setParam (0, P_PWID, 0.95f);
            run (e, 6000); e.audition (0, 1.0f);
            return run (e, 24000);
        };
        const double lo = toneHz (play (220.0), 4800, 24000), hi = toneHz (play (3520.0), 4800, 24000);
        ok ("PRISM: CNTR / WDTH keep one band (220 Hz, then 3520 Hz)", std::fabs (lo - 220.0) < 1.5 && std::fabs (hi - 3520.0) < 10.0, "%.1f / %.1f Hz", lo, hi);
    }

    // 13. a take recorded with PRISM on is analysed as it comes in, and plays right after
    {
        fresh (e); ramTrack (e, 2); setSrc (e, 2, RAM_IN); setTake (e, 2, 5);     // 8 steps
        prism (e, 2, 1.0f, 4); e.setParam (2, P_VOL, 1.0f);
        long clk = 0; auto in = [] (long c) { return 0.5f * (float) std::sin (2.0 * M_PI * 330.0 * c / 48000.0); };
        e.ramStart (2, -1);
        run (e, 8 * STEP + 256, 128, in, &clk);
        const int got = e.ram[2].prNext;
        e.audition (2, 1.0f);
        auto out = run (e, 24000, 128, [] (long) { return 0.0f; }, &clk);
        ok ("PRISM: the analysis follows the recording (the take plays at once)", got >= 8 * STEP / PR_HOP - 3 && std::fabs (toneHz (out, 4800, 24000) - 330.0) < 1.5,
            "%.0f frames, %.1f Hz", got, toneHz (out, 4800, 24000));
    }

    // 14. a loaded take is analysed in the background, never more than PR_BUDGET frames per chunk
    {
        fresh (e); ramTrack (e, 0); prism (e, 0, 1.0f, PR_K);
        loadTake (e, 0, RAM_LEN, [] (int i) { return 0.3f * (float) std::sin (i * 0.05); });
        run (e, 32, 32);
        const int one = e.ram[0].prNext;
        run (e, 64 * 1024, 128);
        ok ("PRISM: a loaded take is analysed a little at a time", one <= PR_BUDGET && e.ram[0].prNext == PR_FRAMES, "%.0f after one chunk, %.0f in the end", one, e.ram[0].prNext);
    }

    // ---------------------------------------------------------------- PRISM v2
    // a take of one tone (or two), played by PRISM with the settings given, from 0.1 s
    auto prismPlay = [&] (std::function<float (int)> take, std::function<void ()> set, int len) {
        fresh (e); loadTake (e, 0, 48000, take);
        ramTrack (e, 0); e.setParam (0, P_VOL, 1.0f); prism (e, 0, 1.0f, 8); e.setParam (0, P_DEC, 1.0f);
        set();
        run (e, 6000); e.audition (0, 1.0f);
        return run (e, len);
    };
    auto two = [] (double h1, double h2) { return [h1, h2] (int i) { return (float) (0.35 * std::sin (2.0 * M_PI * h1 * i / 48000.0) + 0.35 * std::sin (2.0 * M_PI * h2 * i / 48000.0)); }; };

    // 16. WAVE: SAW adds the harmonics of a saw (the 2nd at about half the 1st), the pitch stays; a 7 kHz partial read as
    //     a saw keeps its harmonics under Nyquist (nothing folds back to 1, 6, 13 or 20 kHz)
    {
        auto sn = prismPlay (sine (440.0, 0.5), [] {}, 24000);
        auto sw = prismPlay (sine (440.0, 0.5), [&] { e.setParam (0, P_PWAV, 2.0f / 3.0f); }, 24000);
        const double r1 = ampAt (sw, 4800, 24000, 880.0) / ampAt (sw, 4800, 24000, 440.0), r0 = ampAt (sn, 4800, 24000, 880.0) / ampAt (sn, 4800, 24000, 440.0);
        ok ("PRISM WAVE: SAW gives the harmonics of a saw, same pitch", r1 > 0.4 && r1 < 0.6 && r0 < 0.01 && std::fabs (toneHz (sw, 4800, 24000) - 440.0) < 2.0,
            "2nd / 1st %.3f (sine %.4f)", r1, r0);
        // (a quiet take: the track filter's own saturator would fold the products of 7 + 14 kHz back at louder levels)
        auto hi = prismPlay (sine (7000.0, 0.05), [&] { e.setParam (0, P_PWAV, 2.0f / 3.0f); }, 24000);
        const double f0 = ampAt (hi, 4800, 24000, 7000.0), h2 = ampAt (hi, 4800, 24000, 14000.0);
        double al = 0; for (double hz : { 1000.0, 6000.0, 13000.0, 20000.0 }) al = std::max (al, ampAt (hi, 4800, 24000, hz));
        ok ("PRISM WAVE: band-limited (no aliasing from a 7 kHz saw)", h2 > 0.3 * f0 && al < 1e-3 * f0, "worst alias %.1f dB", 20.0 * std::log10 (al / f0 + 1e-12));
        auto pu = prismPlay (sine (440.0, 0.5), [&] { e.setParam (0, P_PWAV, 1.0f); }, 24000);
        const double pr = ampAt (pu, 4800, 24000, 1760.0) / ampAt (pu, 4800, 24000, 440.0);
        ok ("PRISM WAVE: PULSE (25 %) has no 4th harmonic, a strong 2nd", pr < 0.02 && ampAt (pu, 4800, 24000, 880.0) > 0.5 * ampAt (pu, 4800, 24000, 440.0), "4th / 1st %.4f", pr);
    }
    // 17. OCT moves the resynthesis by octaves
    {
        auto up = prismPlay (sine (440.0, 0.5), [&] { e.setParam (0, P_POCT, takeIdx (3, PR_OCTS)); }, 24000);
        auto dn = prismPlay (sine (440.0, 0.5), [&] { e.setParam (0, P_POCT, takeIdx (0, PR_OCTS)); }, 24000);
        ok ("PRISM OCT: +1 = 880 Hz, -2 = 110 Hz", std::fabs (toneHz (up, 4800, 24000) - 880.0) < 3.0 && std::fabs (toneHz (dn, 4800, 24000) - 110.0) < 1.0,
            "%.1f / %.1f Hz", toneHz (up, 4800, 24000), toneHz (dn, 4800, 24000));
    }
    // 18. QNT: without a scale, 450 Hz goes to A (440); on C major, 460 Hz (A#) goes to A too
    {
        auto q0 = prismPlay (sine (450.0, 0.5), [&] { e.setParam (0, P_PQNT, 0.75f); }, 24000);
        auto q1 = prismPlay (sine (460.0, 0.5), [&] { e.setParam (0, P_PQNT, 0.75f); e.setScale (0xAB5u); }, 24000);
        ok ("PRISM QNT: partials snap to the scale (semitones without one)", std::fabs (toneHz (q0, 4800, 24000) - 440.0) < 1.5 && std::fabs (toneHz (q1, 4800, 24000) - 440.0) < 1.5,
            "%.1f / %.1f Hz", toneHz (q0, 4800, 24000), toneHz (q1, 4800, 24000));
    }
    // 19. WARP (full right, e = 1.62) around the band centre (640 Hz): 640 stays, 1280 goes to 1972 Hz
    {
        auto w = prismPlay (two (640.0, 1280.0), [&] { e.setParam (0, P_PCTR, 0.5f); e.setParam (0, P_PWRP, 1.0f); }, 24000);
        const double a640 = ampAt (w, 4800, 24000, 640.0), a1280 = ampAt (w, 4800, 24000, 1280.0), aw = ampAt (w, 4800, 24000, 640.0 * std::pow (2.0, std::pow (2.0, 0.7)));
        ok ("PRISM WARP: partials stretched around the centre", a640 > 0.15 && aw > 0.15 && a1280 < 0.01, "640 %.3f, 1280 %.3f, warped %.3f", a640, aw);
    }
    // 20. WDTH left of 12 o'clock: a notch on the centre (3520 Hz) leaves 220 Hz alone
    {
        auto nt = prismPlay (two (220.0, 3520.0), [&] { e.setParam (0, P_PCTR, (float) (std::log2 (3520.0 / 20.0) / 10.0)); e.setParam (0, P_PWID, 0.3f); }, 24000);
        ok ("PRISM WDTH: the left half notches the band out", std::fabs (toneHz (nt, 4800, 24000) - 220.0) < 1.5 && ampAt (nt, 4800, 24000, 3520.0) < 0.005,
            "%.1f Hz, 3520 Hz at %.4f", toneHz (nt, 4800, 24000), ampAt (nt, 4800, 24000, 3520.0));
    }
    // 21. SLIC 1/4 (4 steps): the spectrum caught at the hit is held while the take moves on, then the next slice catches up
    {
        auto take = [] (int i) { return (float) (0.5 * std::sin (2.0 * M_PI * (i < 12000 ? 300.0 : 600.0) * i / 48000.0)); };
        auto off = prismPlay (take, [] {}, 36000);
        auto on = prismPlay (take, [&] { e.setParam (0, P_PSLC, takeIdx (4, PR_SLICES)); }, 36000);
        ok ("PRISM SLIC: the spectrum is held for the slice", std::fabs (toneHz (off, 16000, 22000) - 600.0) < 3.0 && std::fabs (toneHz (on, 16000, 22000) - 300.0) < 2.0
            && std::fabs (toneHz (on, 27000, 34000) - 600.0) < 3.0, "%.1f Hz in the slice, %.1f after", toneHz (on, 16000, 22000), toneHz (on, 27000, 34000));
    }
    // 22. TILT: full right lifts 4 kHz above 250 Hz (equal in the take)
    {
        auto tl = prismPlay (two (250.0, 4000.0), [&] { e.setParam (0, P_PTLT, 1.0f); }, 24000);
        const double r = ampAt (tl, 4800, 24000, 4000.0) / ampAt (tl, 4800, 24000, 250.0);
        ok ("PRISM TILT: the highs over the lows", r > 4.0, "4 kHz / 250 Hz %.1f", r);
    }
    // 23. DYN: the loud start of a hit is read richer than its tail (sines at WAVE 0)
    {
        auto take = [] (int i) { return (float) (0.5 * std::exp (-i / 9000.0) * std::sin (2.0 * M_PI * 330.0 * i / 48000.0)); };
        auto d = prismPlay (take, [&] { e.setParam (0, P_PDYN, 1.0f); e.setParam (0, P_PBLR, 0.0f); }, 30000);
        const double h0 = ampAt (d, 0, 4000, 660.0) / ampAt (d, 0, 4000, 330.0), h1 = ampAt (d, 22000, 30000, 660.0) / ampAt (d, 22000, 30000, 330.0);
        ok ("PRISM DYN: louder partials, richer waves", h0 > 0.2 && h1 < 0.5 * h0, "2nd / 1st %.3f at the hit, %.3f later", h0, h1);
    }
    // 24. a lock-only step: OCT +1 locked on step 5 of a frozen PRISM voice moves it an octave up without a new hit, the
    //     next trig brings the track's own value back
    {
        fresh (e); loadTake (e, 0, 48000, sine (440.0, 0.5));
        ramTrack (e, 0); e.setParam (0, P_VOL, 1.0f); prism (e, 0, 1.0f, 4); e.setParam (0, P_PSPD, 0.0f); e.setParam (0, P_DEC, 1.0f);
        e.setTrig (0, 0, 1); e.setTrigless (0, 4, 1); e.setLock (0, 4, P_POCT, takeIdx (3, PR_OCTS));
        run (e, 6000);
        e.setPlay (1);
        auto out = run (e, 16 * STEP + 3 * STEP);
        const int hits = e.tr[0].hitCount;
        const double a = toneHz (out, STEP, 4 * STEP), b = toneHz (out, 5 * STEP + 1000, 15 * STEP), c = toneHz (out, 16 * STEP + 1000, 19 * STEP);
        ok ("lock-only step: the locks change the playing sound, no new hit", hits == 2 && std::fabs (a - 440.0) < 2.0 && std::fabs (b - 880.0) < 3.0 && std::fabs (c - 440.0) < 2.0,
            "%.1f > %.1f Hz (then back)", a, b);
        e.setTrigless (0, 4, 0);
        auto off = run (e, 16 * STEP);
        ok ("lock-only step: off, its locks do nothing", std::fabs (toneHz (off, 5 * STEP + 1000, 15 * STEP) - 440.0) < 2.0, "%.1f Hz", toneHz (off, 5 * STEP + 1000, 15 * STEP));
    }
    // 25. the LFO destinations reach the PRISM controls (the last one, TILT, too)
    {
        auto lfo = prismPlay (sine (440.0, 0.5), [&] { e.setParam (0, P_PMIX, 0.0f); e.setParam (0, P_LDST, takeIdx (27, MOD_DEST_SLOTS)); e.setParam (0, P_LDEP, 1.0f);
                                                        e.setParam (0, P_LSPD, takeIdx (NUM_LFO_SPEEDS - 1, NUM_LFO_SPEEDS)); e.setParam (0, P_LWAV, takeIdx (3, NUM_LFO_WAVES)); }, 24000);
        auto tl = prismPlay (two (250.0, 4000.0), [&] { e.setParam (0, P_LDST, takeIdx (34, MOD_DEST_SLOTS)); e.setParam (0, P_LDEP, 1.0f); e.setParam (0, P_LSPD, takeIdx (0, NUM_LFO_SPEEDS));
                                                        e.setParam (0, P_LWAV, takeIdx (LFO_SHAPES + LW_SQR, NUM_LFO_WAVES)); }, 24000);
        const double r = ampAt (tl, 4800, 24000, 4000.0) / ampAt (tl, 4800, 24000, 250.0);
        ok ("LFO on PRISM: MIX and TILT are destinations", peak (lfo) > 0.05 && r > 1.5, "peak %.3f, tilt %.1f", peak (lfo), r);
    }

    // 15. the worst it can get: 8 tracks PRISM at 16 oscillators (read from the wave tables), every take rich (tones + noise) and recorded over in a
    //     loop (overdub) from every source, BUF crossed, retrigs at 1/256, ratchets, both LFOs at their fastest on STRT
    //     and TUNE, no blur, no glide, random speeds and bands. No NaN, a bounded level, and the time it takes (the 99th
    //     percentile of the blocks under half the budget: the slowest few measure the machine's scheduler, not the engine)
    {
        fresh (e); Rng rng; rng.seed (99);
        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            loadTake (e, t, 2 * 48000, [&] (int i) { double s = 0; for (int k = 1; k <= 12; ++k) s += std::sin (2.0 * M_PI * (110.0 * k * (1.0 + 0.01 * t)) * i / 48000.0) / k; return (float) (0.25 * s) + 0.05f * rng.bi(); });
            ramTrack (e, t); prism (e, t, 1.0f, PR_K); e.setParam (t, P_VOL, 0.5f);
            e.setParam (t, P_PSPD, rng.uni()); e.setParam (t, P_PCTR, rng.uni()); e.setParam (t, P_PWID, rng.uni());
            // the costly reading: every oscillator from the tables, two waves morphed, DYN on; quantiser, slices, warp, tilt
            e.setParam (t, P_PWAV, 0.1f + 0.8f * rng.uni()); e.setParam (t, P_PDYN, rng.uni()); e.setParam (t, P_PQNT, t % 2 ? 0.75f : 0.25f);
            e.setParam (t, P_PSLC, takeIdx (t % PR_SLICES, PR_SLICES)); e.setParam (t, P_PWRP, rng.uni()); e.setParam (t, P_PTLT, rng.uni());
            e.setParam (t, P_POCT, takeIdx (t % PR_OCTS, PR_OCTS));
            e.setParam (t, P_RBUF, takeIdx (1 + (t + 1) % NUM_TRACKS, NUM_TRACKS + 1));
            e.setParam (t, P_C, takeIdx (t % RAM_MODES, RAM_MODES)); e.setParam (t, P_D, takeIdx (t % RAM_SLICES, RAM_SLICES));
            setSrc (e, t, t % RAM_SRCS); setTake (e, t, t % 4); e.setParam (t, P_ROVD, 0.7f);
            e.setParam (t, P_RTRG, takeIdx (NUM_RETRIG - 1, NUM_RETRIG)); e.setParam (t, P_RCNT, takeIdx (NUM_RCOUNT - 1, NUM_RCOUNT));
            e.setParam (t, P_LSPD, takeIdx (NUM_LFO_SPEEDS - 1, NUM_LFO_SPEEDS)); e.setParam (t, P_LDEP, 1.0f); e.setParam (t, P_LDST, takeIdx (3, MOD_DEST_SLOTS));
            e.setParam (t, P_L2SP, takeIdx (NUM_LFO_SPEEDS - 1, NUM_LFO_SPEEDS)); e.setParam (t, P_L2DP, 0.0f);
            e.setParam (t, P_L2DS, takeIdx (t % 2 ? 1 : 28, MOD_DEST_SLOTS));            // TUNE, or WAVE
            for (int s = 0; s < NUM_STEPS; ++s) { e.setTrig (t, s, 1); e.setRec (t, s, s % 2); e.setRatchet (t, s, 1 + s % 8); }
        }
        e.setPlay (1);
        const int N = 10 * 48000, B = 128;
        std::vector<float> L (B), R (B), I (B);
        bool bad = false; float worst = 0; double maxBlock = 0, total = 0; std::vector<double> times;
        for (int i = 0; i < N; i += B)
        {
            for (int j = 0; j < B; ++j) I[j] = 0.3f * (float) std::sin ((i + j) * 0.031);
            e.setInput (I.data(), I.data());
            const auto t0 = std::chrono::steady_clock::now();
            e.process (L.data(), R.data(), B, nullptr);
            const double dt = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            total += dt; maxBlock = std::max (maxBlock, dt); times.push_back (dt);
            for (int j = 0; j < B; ++j) { if (! std::isfinite (L[j]) || ! std::isfinite (R[j])) bad = true; worst = std::max (worst, std::max (std::fabs (L[j]), std::fabs (R[j]))); }
        }
        const double budget = (double) B / 48000.0;
        std::sort (times.begin(), times.end());
        printf ("INFO PRISM worst case (native): %.1f %% of real time, p99 block %.3f ms, worst %.2f ms (budget %.2f ms)\n", 100.0 * total / (N / 48000.0),
                times[times.size() * 99 / 100] * 1000.0, maxBlock * 1000.0, budget * 1000.0);
        ok ("PRISM worst case: 8 tracks x 16 wave oscillators, retrigs 1/256, LFOs at full speed, takes recorded over: no NaN, bounded",
            ! bad && worst <= 1.0f && times[times.size() * 99 / 100] < 0.5 * budget, "peak %.3f, p99 block %.2f ms", worst, times[times.size() * 99 / 100] * 1000.0);
    }

    printf (fails ? "%d FAILED\n" : "ALL OK\n", fails);
    delete E;
    return fails ? 1 : 0;
}
