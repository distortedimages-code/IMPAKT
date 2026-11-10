// Native test harness for the IMPAKT core.
//   render_test stress              -> per-machine robustness / level report
//   render_test ops <file> <wav> <seconds> [machineSolo]
//                                   -> apply ops (same format as the web worklet) and render;
//                                      "at <seconds>" schedules the following ops during the render
#include "../core/sk_engine.h"
#include "../core/sk_weft.h"
#include "../core/sk_stencil.h"
#include "../core/sk_mantra.h"
#include "../core/sk_radio_dec.h"
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <functional>
#include <vector>
#include <complex>
#include <string>
#include <chrono>
#include <array>
#include <initializer_list>

using namespace sk;

static const char* MNAMES[NUM_MACHINES] = { "BD-VA", "BD-FM", "SD-VA", "SD-FM", "HH-MT", "CP-NZ",
                                            "FM-X", "NOISE", "IMPLS", "SY-RAW", "BYTE",
                                            "uLOOP", "BUG", "CHAOS", "VOX", "CZ-PD", "FRACT", "ITERA", "TRIO",
                                            "SINE", "DATA", "FM-Z", "HARD", "HEX-BD", "HEX-SD", "HEX-TM",
                                            "MACRO", "RAM", "HALO", "BREAK", "VERB", "ECHO", "BEND", "TAPE",
                                            "CODEC", "STRUM", "CHOIR", "ALLOY", "CHIMERA", "FUZZ", "CUBE", "WEST", "WEFT", "HOWL",
                                            "STENCIL", "MANTRA", "RADIO" };

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = fopen (path, "wb");
    int n = (int) L.size();
    int dataBytes = n * 2 * 2;
    auto w32 = [&] (unsigned v) { fwrite (&v, 4, 1, f); };
    auto w16 = [&] (unsigned short v) { fwrite (&v, 2, 1, f); };
    fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); fwrite ("WAVE", 1, 4, f);
    fwrite ("fmt ", 1, 4, f); w32 (16); w16 (1); w16 (2); w32 (sr); w32 (sr * 4); w16 (4); w16 (16);
    fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (int i = 0; i < n; ++i)
    {
        float a = L[i], b = R[i];
        short sa = (short) (fmaxf (-1.f, fminf (1.f, a)) * 32767.f);
        short sb = (short) (fmaxf (-1.f, fminf (1.f, b)) * 32767.f);
        fwrite (&sa, 2, 1, f); fwrite (&sb, 2, 1, f);
    }
    fclose (f);
}

struct Stats { float peak = 0, rms = 0; bool bad = false; float lastLoud = 0; float dc = 0; };

static Stats renderStats (Engine& e, int samples, int block = 128)
{
    Stats s; double sum = 0, dcs = 0;
    std::vector<float> L (block), R (block);
    for (int i = 0; i < samples; i += block)
    {
        int n = std::min (block, samples - i);
        e.process (L.data(), R.data(), n, nullptr);
        for (int k = 0; k < n; ++k)
        {
            float a = L[k], b = R[k];
            if (! std::isfinite (a) || ! std::isfinite (b)) s.bad = true;
            float m = std::max (fabsf (a), fabsf (b));
            s.peak = std::max (s.peak, m);
            sum += (double) a * a + (double) b * b;
            dcs += a + b;
            if (m > 0.001f) s.lastLoud = (float) (i + k) / e.sr;
        }
    }
    s.rms = (float) std::sqrt (sum / (2.0 * samples));
    s.dc = (float) (dcs / (2.0 * samples));
    return s;
}

// RAM: a test take (one second of a decaying 220 Hz tone) in a track's buffer
static void ramTestTake (Engine& e, int t)
{
    const int n = 48000;
    for (int i = 0; i < n; ++i) e.ramMem[t][i] = (short) (20000.0 * std::sin (6.283185307 * 220.0 * i / 48000.0) * std::exp (-3.0 * i / n));
    e.ramLoaded (t, n, 48000.0f);
}

static Engine* REF = nullptr;
static void softReset (Engine* e, float sr)
{
    if (absf (e->sr - sr) > 0.5f) { e->init (sr); return; }
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        for (int p = 0; p < NUM_PARAMS; ++p) e->tr[t].p[p] = REF->tr[t].p[p];
        for (int s = 0; s < NUM_STEPS; ++s) { e->tr[t].trig[s] = 0; e->tr[t].rat[s] = 1; for (int p = 0; p < NUM_PARAMS; ++p) e->tr[t].lock[s][p] = -1.0f; }
        e->tr[t].len = 16; e->tr[t].spd = 4; e->tr[t].mute = 0;
    }
    for (int i = 0; i < NUM_GLOBALS; ++i) e->g[i] = REF->g[i];
    e->setPlay (0); e->panic(); e->fxInit(); e->boxInit(); e->spec.init (e->sr);
    e->pos16 = 0; e->running = e->wasRunning = 0;
}


// CHIMERA's bank (bank/ next to tools/, or $IMPAKT_BANK): read and decoded at once (the plugin does the same)
static bool loadBank()
{
    if (sk::BANK.state == 2) return true;
    const char* env = getenv ("IMPAKT_BANK");
    std::string dir = env ? env : "bank";
    auto readAll = [] (const std::string& f) {
        std::vector<unsigned char> v; FILE* fp = fopen (f.c_str(), "rb"); if (! fp) return v;
        fseek (fp, 0, SEEK_END); const long n = ftell (fp); fseek (fp, 0, SEEK_SET); v.resize ((size_t) n);
        if (n > 0 && fread (v.data(), 1, (size_t) n, fp) != (size_t) n) v.clear();
        fclose (fp); return v; };
    auto idx = readAll (dir + "/chimera.idx");
    if (idx.empty()) { dir = "../bank"; idx = readAll (dir + "/chimera.idx"); }
    if (idx.empty() || ! sk::bankIndex (idx.data(), (int) idx.size())) return false;
    static std::vector<std::vector<unsigned char>> files;
    files.clear();
    for (int k = 0;; ++k) { auto f = readAll (dir + "/chimera." + std::to_string (k) + ".bin"); if (f.empty()) break; files.push_back (std::move (f)); }
    for (int i = 0; i < sk::BANK.n; ++i)
    {
        const sk::BankEntry& e = sk::BANK.e[i];
        if (e.file < (int) files.size()) sk::bankDecodeNow (i, files[(size_t) e.file].data() + e.foff, e.coded);
    }
    return sk::BANK.state == 2 && sk::BANK.failed == 0;
}

static int info()
{
    Engine* e = new Engine();
    for (float sr : { 44100.0f, 48000.0f, 96000.0f })
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        e->init (sr);
        auto t1 = std::chrono::high_resolution_clock::now();
        printf ("sr %.0f: init %.1f ms, IR pool %d / %d floats\n", sr, std::chrono::duration<double, std::milli> (t1 - t0).count(), e->bank.used, (int) IR_POOL);
        if (sr == 48000.0f)
            for (int i = 1; i <= NUM_IRS; ++i) printf ("  IR %2d: %6d samples, A %d, B %d partitions\n", i, e->bank.e[i].len, e->bank.e[i].pa, e->bank.e[i].pb);
    }
    printf ("sizeof(Engine) = %.1f MB\n", sizeof (Engine) / 1048576.0);
    delete e;
    return 0;
}

static void windows (Engine* e, float secs, const char* label)
{
    const float marks[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
    std::vector<float> L (128), R (128);
    int total = (int) (secs * e->sr), done = 0, mi = 0; double acc = 0; int cnt = 0; float peak = 0; bool bad = false;
    printf ("%-34s", label);
    while (done < total)
    {
        e->process (L.data(), R.data(), 128, nullptr);
        for (int k = 0; k < 128; ++k) { acc += L[k] * L[k] + R[k] * R[k]; peak = std::max (peak, std::fabs (L[k])); if (! std::isfinite (L[k])) bad = true; }
        cnt += 128; done += 128;
        if (mi < 6 && done >= (int) (marks[mi] * e->sr))
        {
            printf (" %6.1f", 10 * std::log10 (acc / (2.0 * cnt) + 1e-12)); acc = 0; cnt = 0; ++mi;
            if (marks[mi - 1] >= secs) break;
        }
    }
    printf ("  dB   peak %.2f%s\n", peak, bad ? "  NaN!" : "");
}

static int fxtest()
{
    Engine* e = new Engine(); e->init (48000.f);
    REF = new Engine(); REF->init (48000.f);
    printf ("%-34s %6s %6s %6s %6s %6s %6s\n", "window ends (s) ->", "0.25", "0.5", "1", "2", "4", "8");
    // the six reverb algorithms on a kick (full send)
    const char* RNAMES[NUM_REV_ALGOS] = { "ROOM", "PLATE", "SPRING", "SPECTRAL", "HALL", "SPACE" };
    for (int a = 0; a < NUM_REV_ALGOS; ++a)
        for (float tm : { 0.2f, 0.5f, 0.8f })
        {
            softReset (e, 48000.f);
            for (int t = 1; t < 8; ++t) e->setParam (t, P_VOL, 0);
            e->setParam (0, P_REV, 1.0f);
            e->setGlobal (G_RALG, (a + 0.5f) / NUM_REV_ALGOS); e->setGlobal (G_RTIM, tm); e->setGlobal (G_RLVL, 1.0f);
            e->audition (0, 1.0f);
            char lab[64]; snprintf (lab, 64, "REV %-8s time %.1f", RNAMES[a], tm); windows (e, 8.0f, lab);
        }
    for (int mode = 0; mode < 6; ++mode)
    {
        softReset (e, 48000.f);
        for (int t = 1; t < 8; ++t) e->setParam (t, P_VOL, 0);
        e->setParam (0, P_MACH, machineVal (M_CHAOS)); e->setParam (0, P_DEC, 0.6f);
        e->setParam (0, P_REV, 1.0f); e->setGlobal (G_RALG, (REV_SPECTRAL + 0.5f) / NUM_REV_ALGOS);
        e->setGlobal (G_RTIM, 0.5f); e->setGlobal (G_RLVL, 1.0f);
        const char* names[] = { "SPEC chaos default", "SPEC blur 1 atmo 1", "SPEC warp +3oct reflect .8", "SPEC freeze after 0.4 s", "SPEC reverse", "SPACE shimmer 1 mod 1" };
        if (mode == 1) { e->setGlobal (G_RSIZ, 1.0f); e->setGlobal (G_RDMP, 1.0f); }
        if (mode == 2) { e->setGlobal (G_RCH1, 1.0f); e->setGlobal (G_RCH2, 0.8f); }
        if (mode == 4) e->setGlobal (G_RPRE, 2.5f / 4.0f);
        if (mode == 5) { e->setGlobal (G_RALG, (REV_SPACE + 0.5f) / NUM_REV_ALGOS); e->setGlobal (G_RCH1, 1.0f); e->setGlobal (G_RCH2, 1.0f); e->setGlobal (G_RTIM, 1.0f); }
        e->audition (0, 1.0f);
        if (mode == 3)
        {
            std::vector<float> L (128), R (128);
            for (int i = 0; i < (int) (0.4f * 48000) / 128; ++i) e->process (L.data(), R.data(), 128, nullptr);
            e->setGlobal (G_RPRE, 1.5f / 4.0f);
        }
        windows (e, 8.0f, names[mode]);
    }
    // BOX: every IR on a kick, mix 1
    for (int ir = 1; ir <= NUM_IRS; ++ir)
    {
        softReset (e, 48000.f);
        for (int t = 1; t < 8; ++t) e->setParam (t, P_VOL, 0);
        e->setParam (0, P_IR, (ir + 0.5f) / (NUM_IRS + 1)); e->setParam (0, P_IMIX, 1.0f);
        e->audition (0, 1.0f);
        char lab[64]; snprintf (lab, 64, "BOX IR %2d (kick, wet)", ir); windows (e, 2.0f, lab);
    }
    softReset (e, 48000.f);
    for (int t = 1; t < 8; ++t) e->setParam (t, P_VOL, 0);
    e->setParam (0, P_IR, (9 + 0.5f) / 25); e->setParam (0, P_IMIX, 1.0f); e->setParam (0, P_IFB, 0.95f);
    e->audition (0, 1.0f); windows (e, 8.0f, "BOX bidon feedback .95");
    softReset (e, 48000.f);
    for (int t = 1; t < 8; ++t) e->setParam (t, P_VOL, 0);
    e->setParam (0, P_IR, (24 + 0.5f) / 25); e->setParam (0, P_IMIX, 1.0f); e->setParam (0, P_IREV, 0.9f);
    e->audition (0, 1.0f); windows (e, 2.0f, "BOX eglise reversed");
    return 0;
}

// HEX machines: one hit of track 1 (the others silent) -> mono samples
static std::vector<float> kitHit (Engine* e, int m, float vel, float secs, std::initializer_list<std::pair<int, float>> ps, float note = 0.0f)
{
    softReset (e, 48000.f);
    for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, t == 0 ? 0.8f : 0.0f);
    e->setParam (0, P_MACH, machineVal (m));
    for (auto& p : ps) e->setParam (0, p.first, p.second);
    e->audition (0, vel, note);
    std::vector<float> L (128), R (128), out;
    for (int i = 0; i < (int) (secs * 48000); i += 128) { e->process (L.data(), R.data(), 128, nullptr); for (int k = 0; k < 128; ++k) out.push_back (0.5f * (L[k] + R[k])); }
    return out;
}
// a machine's own output (no track strip, no master): one voice, triggered and rendered by the engine's machine code
static std::vector<float> machineOnly (Engine* e, int m, float note, std::initializer_list<std::pair<int, float>> ps, float secs)
{
    static Voice v; memset ((void*) &v, 0, sizeof v);
    float P[NUM_PARAMS]; for (int p = 0; p < NUM_PARAMS; ++p) P[p] = e->tr[0].p[p];
    P[P_MACH] = machineVal (m); for (auto& q : ps) P[q.first] = q.second;
    v.active = 1; v.machine = m; v.vel = 0.9f; v.note = note; v.quant = 0;
    v.penv = v.menv = v.nenv = v.cenv = v.senv = v.ienv = v.i3env = 1.0f; v.rng.seed (12345u);
    e->machineTrigger (v, P);
    std::vector<float> out; float buf[CHUNK];
    for (int i = 0; i < (int) (secs * 48000); i += CHUNK) { e->machineRender (v, P, CHUNK, buf); out.insert (out.end(), buf, buf + CHUNK); }
    return out;
}
// spectral centroid (Hz) of x[a, a + 4096)
static double centroid (const std::vector<float>& x, int a)
{
    double num = 0, den = 0;
    for (int b = 1; b < 1024; ++b)
    {
        double re = 0, im = 0; const double w = 2 * M_PI * (2.0 * b) / 4096.0;
        for (int i = 0; i < 4096; ++i) { const double h = 0.5 - 0.5 * cos (2 * M_PI * i / 4096.0); re += x[a + i] * h * cos (w * i); im -= x[a + i] * h * sin (w * i); }
        const double mg = sqrt (re * re + im * im); num += mg * 2.0 * b * 48000.0 / 4096.0; den += mg;
    }
    return den > 0 ? num / den : 0;
}
// energy off the harmonics of f0 below 20 kHz (dB below the total) in x[a, a + 4800), Blackman-Harris window: 10 Hz
// bins, f0 a multiple of 10 Hz
static double offHarmonic (const std::vector<float>& x, int a, double f0)
{
    const int N = 4800, step = (int) (f0 / 10.0 + 0.5);
    double tot = 0, off = 0;
    for (int b = 1; b < N / 2; ++b)
    {
        double re = 0, im = 0; const double w = 2 * M_PI * b / N;
        for (int i = 0; i < N; ++i)
        {
            const double c = 2 * M_PI * i / N, h = 0.35875 - 0.48829 * cos (c) + 0.14128 * cos (2 * c) - 0.01168 * cos (3 * c);
            re += x[a + i] * h * cos (w * i); im -= x[a + i] * h * sin (w * i);
        }
        const double e2 = re * re + im * im; tot += e2;
        const int r = b % step; if (r > 4 && r < step - 4 && b < 2000) off += e2;   // below 20 kHz (the halfband's transition
                                                                                // band, 20 .. 24 kHz, folds what is just above)
    }
    return 10 * log10 ((off + 1e-30) / (tot + 1e-30));
}
// power (dB) of x[a, a + 8192) between f1 and f2 (Hann window, 5.9 Hz bins)
static double bandDb (const std::vector<float>& x, int a, double f1, double f2)
{
    const int N = 8192; double e = 0;
    for (int b = (int) (f1 * N / 48000.0); b <= (int) (f2 * N / 48000.0); ++b)
    {
        double re = 0, im = 0; const double w = 2 * M_PI * b / N;
        for (int i = 0; i < N; i += 1) { const double h = 0.5 - 0.5 * cos (2 * M_PI * i / N); re += x[a + i] * h * cos (w * i); im -= x[a + i] * h * sin (w * i); }
        e += re * re + im * im;
    }
    return 10 * log10 (e + 1e-30);
}
// TONE and the envelope followers
static int modtest()
{
    Engine* e = new Engine(); e->init (48000.f);
    REF = new Engine(); REF->init (48000.f);
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-66s %.2f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    // white noise (a RAM take of it, played once), the track flat, then each TONE filter
    auto noiseWith = [&] (std::initializer_list<std::pair<int, float>> tone) {
        softReset (e, 48000.f);
        Rng r; r.seed (4242u);
        for (int i = 0; i < 96000; ++i) e->ramMem[0][i] = (short) (r.bi() * 8000.0f);
        e->ramLoaded (0, 96000, 48000.0f);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, t == 0 ? 0.3f : 0.0f);
        e->setParam (0, P_MACH, machineVal (M_RAM));
        for (auto& p : std::initializer_list<std::pair<int, float>> { { P_A, 0.0f }, { P_B, 1.0f }, { P_C, 0.5f / RAM_MODES }, { P_D, 0.5f / RAM_SLICES },
                                                                     { P_E, 0.5f / NUM_DAC_MODELS }, { P_F, 0.0f }, { P_DEC, 1.0f }, { P_TUNE, 0.5f } })
            e->setParam (0, p.first, p.second);
        for (auto& p : tone) e->setParam (0, p.first, p.second);
        e->audition (0, 0.9f);
        std::vector<float> L (128), R (128), out;
        for (int i = 0; i < 48000; i += 128) { e->process (L.data(), R.data(), 128, nullptr); for (int k = 0; k < 128; ++k) out.push_back (0.5f * (L[k] + R[k])); }
        return out;
    };
    const auto flat = noiseWith ({});
    // HP at 1 kHz: 20 * 400^v = 1000 -> v = 0.653; LP at 500 Hz: 100 * 200^v = 500 -> v = 0.304
    const auto hp = noiseWith ({ { P_HPF, 0.653f } }), lp = noiseWith ({ { P_LPF, 0.304f } });
    const int a = 9600;
    const double f100 = bandDb (flat, a, 80, 120), f5k = bandDb (flat, a, 4500, 5500), f1k = bandDb (flat, a, 900, 1100);
    check (f100 - bandDb (hp, a, 80, 120) > 30, "TONE HP at 1 kHz: 100 Hz down by more than 30 dB (dB)", f100 - bandDb (hp, a, 80, 120));
    check (absf ((float) (f5k - bandDb (hp, a, 4500, 5500))) < 1.0, "TONE HP at 1 kHz: 5 kHz untouched (dB)", f5k - bandDb (hp, a, 4500, 5500));
    check (f5k - bandDb (lp, a, 4500, 5500) > 30, "TONE LP at 500 Hz: 5 kHz down by more than 30 dB (dB)", f5k - bandDb (lp, a, 4500, 5500));
    check (absf ((float) (f100 - bandDb (lp, a, 80, 120))) < 1.0, "TONE LP at 500 Hz: 100 Hz untouched (dB)", f100 - bandDb (lp, a, 80, 120));
    // the bands: +12 dB (v = 0.9) at their frequency; LOWF 0.5 = 126 Hz, MIDF 0.5 = 1.1 kHz, HIGF 0.5 = 4.7 kHz
    const auto lo = noiseWith ({ { P_LOW, 0.9f } }), mi = noiseWith ({ { P_MID, 0.9f } }), hi = noiseWith ({ { P_HIGH, 0.9f } });
    check (absf ((float) (bandDb (lo, a, 25, 45) - bandDb (flat, a, 25, 45) - 12.0)) < 1.5, "TONE LOW +12 dB: the shelf below 45 Hz (dB)", bandDb (lo, a, 25, 45) - bandDb (flat, a, 25, 45));
    check (absf ((float) (bandDb (mi, a, 1050, 1150) - bandDb (flat, a, 1050, 1150) - 12.0)) < 1.5, "TONE MID +12 dB: the bell at 1.1 kHz (dB)", bandDb (mi, a, 1050, 1150) - bandDb (flat, a, 1050, 1150));
    check (absf ((float) (bandDb (hi, a, 12000, 16000) - bandDb (flat, a, 12000, 16000) - 12.0)) < 1.5, "TONE HIGH +12 dB: the shelf above 12 kHz (dB)", bandDb (hi, a, 12000, 16000) - bandDb (flat, a, 12000, 16000));
    check (absf ((float) (bandDb (mi, a, 80, 120) - f100)) < 0.5, "TONE MID: 100 Hz untouched (dB)", bandDb (mi, a, 80, 120) - f100);
    // envelope follower as a sidechain: T1 a kick on every beat, T2 a held tone; EF1 of T2 on T1 pulls T2's VOL down
    {
        auto run = [&] (bool duck) {
            softReset (e, 48000.f);
            for (int t = 2; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
            e->setParam (0, P_MACH, machineVal (M_BD_VA)); e->setParam (0, P_DEC, 0.3f); e->setParam (0, P_VOL, 0.8f); e->setParam (0, P_PAN, 0.0f);
            for (int s = 0; s < 16; s += 4) e->setTrig (0, s, 1);
            e->setParam (1, P_PAN, 1.0f);                                // T1 hard left, T2 hard right: R is T2 alone
            e->setParam (1, P_MACH, machineVal (M_SINUS)); e->setParam (1, P_DEC, 1.0f); e->setParam (1, P_GATE, 1.0f); e->setParam (1, P_VOL, 0.5f);
            e->setParam (1, P_TUNE, 0.75f);
            e->setTrig (1, 0, 1);
            if (duck)
            {
                e->setParam (1, P_E1SR, 1.5f / EF_SRCS);                 // T1
                e->setParam (1, P_E1GN, 0.5f);                           // +12 dB
                e->setParam (1, P_E1AT, 0.15f); e->setParam (1, P_E1DC, 0.45f);
                e->setParam (1, P_E1DP, 0.0f);                           // -100 %
                e->setParam (1, P_E1DS, 14.5f / NUM_LFO_DEST);          // VOL
            }
            e->setPlay (1);
            std::vector<float> L (128), R (128), out;
            for (int i = 0; i < 48000 * 2; i += 128) { e->process (L.data(), R.data(), 128, nullptr); for (int k = 0; k < 128; ++k) out.push_back (R[k]); }
            return out;
        };
        const auto dry = run (false), sc = run (true);
        // the hit at beat 2 (0.5 s at 120 BPM): T2's level 30 ms after it vs 30 ms before it, at T2's pitch
        auto lvl = [&] (const std::vector<float>& x, double at) { double s2 = 0; const int i0 = (int) (at * 48000); for (int i = i0; i < i0 + 960; ++i) s2 += (double) x[i] * x[i]; return 10 * log10 (s2 / 960 + 1e-30); };
        const double dDry = lvl (dry, 0.53) - lvl (dry, 0.45), dSc = lvl (sc, 0.53) - lvl (sc, 0.45);
        check (dSc < dDry - 6.0, "EF1 sidechain (T1 -> VOL of T2): T2 dips under the kick by > 6 dB (dB)", dSc - dDry);
        check (lvl (sc, 0.95) - lvl (sc, 0.45) > -1.5, "EF1 sidechain: T2 back up before the next kick (dB)", lvl (sc, 0.95) - lvl (sc, 0.45));
    }
    // TRG mode: a threshold crossing fires the envelope once per hit
    {
        softReset (e, 48000.f);
        for (int t = 1; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setParam (0, P_MACH, machineVal (M_BD_VA)); e->setParam (0, P_DEC, 0.3f);
        for (int s = 0; s < 16; s += 4) e->setTrig (0, s, 1);
        e->setParam (0, P_E1TG, 0.6f); e->setParam (0, P_E1AT, 0.2f); e->setParam (0, P_E1DC, 0.5f);
        e->setParam (0, P_E1DP, 1.0f); e->setParam (0, P_E1DS, 9.5f / NUM_LFO_DEST);   // FREQ
        e->setSpecTrack (0);
        e->setPlay (1);
        std::vector<float> L (128), R (128); float mi[64]; int fires = 0; float prev = 0;
        for (int i = 0; i < 48000 * 2; i += 128)
        {
            e->process (L.data(), R.data(), 128, nullptr);
            e->modInfo (mi);
            if (mi[4] > 0.3f && prev <= 0.1f) ++fires;
            prev = mi[4];
        }
        check (fires == 4, "EF1 TRG: one envelope per kick over 2 s at 120 BPM (fires)", fires);
    }
    // no zipper: a held sine with a fast LFO (32 Hz) on drive, level, pan, the filter and TONE's LP, then a knob
    // turned in UI-message steps. The roughness at the chunk edges (|second difference| on the samples where a
    // 32-sample chunk starts) over the roughness elsewhere: 1 = no trace of the chunks
    {
        auto zipRun = [&] (std::initializer_list<std::pair<int, float>> ps, int dest, bool knob, int ch) {
            softReset (e, 48000.f);
            for (int t = 1; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
            e->setParam (0, P_MACH, machineVal (M_SINUS)); e->setParam (0, P_DEC, 1.0f); e->setParam (0, P_GATE, 1.0f);
            e->setParam (0, P_VOL, 0.6f); e->setParam (0, P_TUNE, 0.6f);
            // a clean held sine: no sub, no drop, no ring, square gate, no noise
            e->setParam (0, P_A, 0.0f); e->setParam (0, P_B, 0.0f); e->setParam (0, P_C, 0.5f / 16.0f); e->setParam (0, P_E, 1.0f); e->setParam (0, P_F, 0.0f);
            for (auto& p : ps) e->setParam (0, p.first, p.second);
            if (dest > 0) { e->setParam (0, P_LDST, (dest + 0.5f) / NUM_LFO_DEST); e->setParam (0, P_LDEP, 0.9f); e->setParam (0, P_LSPD, 15.5f / 16.0f); e->setParam (0, P_LWAV, 0.5f / NUM_LFO_WAVES); }
            e->setTrig (0, 0, 1);
            e->setPlay (1);
            std::vector<float> L (128), R (128), out;
            for (int i = 0; i < 48000 * 3 / 2; i += 128)
            {
                if (knob && i >= 9600 && i < 9600 + 24000 && (i % 768) == 0) e->setParam (0, P_DRV, 0.1f + 0.5f * (float) (i - 9600) / 24000.0f);
                e->process (L.data(), R.data(), 128, nullptr);
                for (int k = 0; k < 128; ++k) out.push_back (ch ? R[k] : L[k]);
            }
            // folded on the chunk: where a chunk starts (wherever the output's latency puts it) stands out
            double fold[CHUNK] = {};
            for (int i = 9600; i < (int) out.size(); ++i) fold[i % CHUNK] += fabs ((double) out[i] - 2.0 * out[i - 1] + out[i - 2]);
            double mx = 0, srt[CHUNK]; for (int k = 0; k < CHUNK; ++k) { mx = fmax (mx, fold[k]); srt[k] = fold[k]; }
            std::sort (srt, srt + CHUNK);
            return mx / (srt[CHUNK / 2] + 1e-30);
        };
        const double zD = zipRun ({ { P_DRV, 0.35f } }, 11, false, 0), zV = zipRun ({}, 14, false, 0), zP = zipRun ({}, 15, false, 0);
        const double zF = zipRun ({ { P_FTYP, 0.5f / NUM_FTYPES }, { P_FREQ, 0.42f }, { P_RES, 0.5f } }, 9, false, 0);
        const double zT = zipRun ({ { P_LPF, 0.35f } }, 36, false, 0), zK = zipRun ({}, 0, true, 0);
        check (zD < 1.3, "no zipper: LFO on DRIV (roughness at the chunk edge / median)", zD);
        check (zV < 1.3, "no zipper: LFO on VOL", zV);
        check (zP < 1.3, "no zipper: LFO on PAN", zP);
        check (zF < 1.3, "no zipper: LFO on FREQ (LP, RESO 50 %)", zF);
        check (zT < 1.3, "no zipper: LFO on TONE LP", zT);
        check (zK < 1.3, "no zipper: DRIV turned in UI steps (every 16 ms)", zK);
    }
    // no click when a machine's own drive / fold / grit comes in: a held note, the knob turned up from zero in UI
    // steps. Above 5 kHz (a click is broadband, the stage's harmonics are not that high), the loudest moment of the
    // turn over the loudest moment once it has settled: 1 = the turn adds nothing of its own
    {
        auto turnRun = [&] (int m, std::initializer_list<std::pair<int, float>> ps, int knob, float to) {
            softReset (e, 48000.f);
            for (int t = 1; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
            e->setParam (0, P_MACH, machineVal (m)); e->setParam (0, P_DEC, 1.0f); e->setParam (0, P_GATE, 1.0f); e->setParam (0, P_VOL, 0.6f);
            // (a machine with side pages, ALLOY: at its defaults, as the page gives it)
            float sd[NUM_SIDES]; const int ns = machineSideDefaults (m, sd); for (int q = 0; q < ns; ++q) e->setParam (0, sideParam (q), sd[q]);
            const int na = machineAFDefaults (m, sd); for (int q = 0; q < na; ++q) e->setParam (0, P_A + q, sd[q]);
            if (m == M_ALLOY) e->setParam (0, PA_VERB, 0.0f);
            for (auto& p : ps) e->setParam (0, p.first, p.second);
            e->setParam (0, knob, 0.0f);
            e->setTrig (0, 0, 1);
            e->setPlay (1);
            std::vector<float> L (128), R (128), out;
            for (int i = 0; i < 48000 * 3 / 2; i += 128)
            {
                if (i >= 24000 && i < 24000 + 7680 && (i % 768) == 0) e->setParam (0, knob, to * (float) (i - 24000 + 768) / 7680.0f);
                e->process (L.data(), R.data(), 128, nullptr);
                for (int k = 0; k < 128; ++k) out.push_back (L[k]);
            }
            // 2-pole high-pass at 5 kHz (RBJ, Q 0.707)
            const double w = 2 * M_PI * 5000.0 / 48000.0, al = sin (w) / (2 * 0.7071), cw = cos (w), a0 = 1 + al;
            const double b0 = (1 + cw) / 2 / a0, b1 = -(1 + cw) / a0, b2 = b0, a1 = -2 * cw / a0, a2 = (1 - al) / a0;
            double x1 = 0, x2 = 0, y1 = 0, y2 = 0, during = 0, after = 0;
            for (int i = 0; i < (int) out.size(); ++i)
            {
                const double y = b0 * out[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = out[i]; y2 = y1; y1 = y;
                if (i >= 23000 && i < 24000 + 7680 + 2400) during = fmax (during, fabs (y));
                else if (i >= 48000 && i < 70000) after = fmax (after, fabs (y));
            }
            return during / (after + 1e-9);
        };
        const double cBd = turnRun (M_BD_VA, { { P_A, 0.0f }, { P_B, 0.0f }, { P_D, 0.0f }, { P_F, 1.0f }, { P_TUNE, 0.6f } }, P_E, 0.4f);
        // (ALLOY: a scrape held on a plate, its COMB and its PHAS turned up)
        const double cSt = turnRun (M_ALLOY, { { P_TUNE, 0.5f }, { P_A, 3.5f / 8 }, { PA_NOIS, 0.7f }, { PA_NLEN, 1.0f } }, PA_COMB, 0.8f);
        const double cWg = turnRun (M_ALLOY, { { P_TUNE, 0.5f }, { P_A, 3.5f / 8 }, { PA_NOIS, 0.7f }, { PA_NLEN, 1.0f } }, PA_PHAS, 0.8f);
        const double cDv = turnRun (M_ALLOY, { { P_TUNE, 0.5f }, { P_A, 3.5f / 8 }, { PA_NOIS, 0.7f }, { PA_NLEN, 1.0f } }, PA_DRIV, 0.6f);
        const double cTd = turnRun (M_SINUS, { { P_A, 0.0f }, { P_B, 0.0f }, { P_C, 0.5f / 16.0f }, { P_E, 1.0f }, { P_F, 0.0f }, { P_TUNE, 0.6f } }, P_DRV, 0.4f);
        check (cTd < 1.5, "no click: the track's DRIV (FLT page) turned up from 0 on a held sine", cTd);
        check (cBd < 1.5, "no click: BD·VA DRIV turned up from 0 on a held note (turn / settled, > 5 kHz)", cBd);
        check (cSt < 1.5, "no click: ALLOY COMB turned up from 0 (a scrape held)", cSt);
        check (cWg < 1.5, "no click: ALLOY PHAS turned up from 0 (a scrape held)", cWg);
        check (cDv < 1.5, "no click: ALLOY DRIV turned up from 0 (a scrape held)", cDv);
    }
    delete e;
    printf (fails ? "%d FAILED\n" : "MOD OK\n", fails);
    return fails ? 1 : 0;
}

// GLITCH: transparent at rest, RAND back to the sound at zero, RAM left alone, each event and mode doing its job
static int glitchtest()
{
    Engine* e = new Engine(); e->init (48000.f);
    REF = new Engine(); REF->init (48000.f);
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-70s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    // a pattern: T1 BD·VA on every step (r), T2 a RAM take of white noise on beats (r too: RAND must not touch it)
    auto setup = [&] (bool ram) {
        e->init (48000.f);                    // from scratch: the machines' noise starts from the same seeds
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setParam (0, P_MACH, machineVal (M_BD_VA)); e->setParam (0, P_VOL, 0.6f); e->setParam (0, P_DEC, 0.2f); e->setParam (0, P_D, 0.4f);
        for (int s = 0; s < 16; ++s) e->setTrig (0, s, 1);
        if (ram)
        {
            Rng r; r.seed (777u);
            for (int i = 0; i < 48000; ++i) e->ramMem[1][i] = (short) (r.bi() * 8000.0f);
            e->ramLoaded (1, 48000, 48000.0f);
            e->setParam (1, P_MACH, machineVal (M_RAM)); e->setParam (1, P_VOL, 0.5f);
            for (auto& p : std::initializer_list<std::pair<int, float>> { { P_A, 0.0f }, { P_B, 0.25f }, { P_C, 0.5f / RAM_MODES }, { P_D, 0.5f / RAM_SLICES },
                                                                         { P_E, 0.5f / NUM_DAC_MODELS }, { P_F, 0.0f }, { P_DEC, 1.0f }, { P_TUNE, 0.5f } })
                e->setParam (1, p.first, p.second);
            for (int s = 0; s < 16; s += 4) e->setTrig (1, s, 1);
        }
        e->g[G_XMSK] = 1.0f;
        e->g[G_MXON] = 0.0f;                  // (the MIX helpers adapt to what they heard: comparisons need them off)
    };
    auto render = [&] (double secs, std::vector<float>& L, std::vector<float>& R, std::function<void (int)> at) {
        std::vector<float> l (128), r (128);
        for (int i = 0; i < (int) (secs * 48000); i += 128) { if (at) at (i); e->process (l.data(), r.data(), 128, nullptr); L.insert (L.end(), l.begin(), l.end()); R.insert (R.end(), r.begin(), r.end()); }
    };
    auto maxDiff = [] (const std::vector<float>& a, const std::vector<float>& b, int from, int to) { double m = 0; for (int i = from; i < to; ++i) m = fmax (m, fabs (a[i] - b[i])); return m; };
    // 1. the effect alone (the engine adds its master stages around it): ON with every knob at zero gives the
    //    input back bit for bit; LOSS just above zero gives it back 512 samples late
    Glitch* gl = new Glitch(); gl->init (48000.0f);
    float noiseAmt = 0.25f;
    auto glRun = [&] (GlitchParams gp, std::vector<float>& inL, std::vector<float>& outL, int secs) {
        gl->reset(); Rng r; r.seed (31337u);
        std::vector<float> l (128), rr (128);
        for (int i = 0; i < secs * 48000; i += 128)
        {
            for (int k = 0; k < 128; ++k) { const float x = 0.5f * sinf ((float) (i + k) * 0.0573f) + noiseAmt * r.bi(); l[k] = x; rr[k] = -x; inL.push_back (x); }
            gl->process (l.data(), rr.data(), 128, gp, (double) i * 120.0 / 60.0 * 4.0 / 48000.0, 120.0 / 60.0 * 4.0 / 48000.0);
            outL.insert (outL.end(), l.begin(), l.end());
        }
    };
    GlitchParams z0 { 1, 2.5f / GL_GRIDS, 0, 0, 0, 0, 0, 0, 0, 0.5f, 1.0f, 0 };
    {
        std::vector<float> in, out; glRun (z0, in, out, 2);
        check (maxDiff (in, out, 0, (int) in.size()) == 0.0, "GLITCH on with every knob at zero: the input, bit for bit (max diff)", maxDiff (in, out, 0, (int) in.size()));
        GlitchParams zl = z0; zl.los = 0.0021f;
        noiseAmt = 0.0f;                      // (a tone: the barely narrower band at the very top does not count)
        std::vector<float> in2, out2; glRun (zl, in2, out2, 2);
        noiseAmt = 0.25f;
        double m = 0; for (int i = 48000; i < 90000; ++i) m = fmax (m, fabs (out2[i] - in2[i - Glitch::FN]));
        check (m < 0.02, "LOSS just above 0: the input, 512 samples late (max diff)", m);
        // CRPT, each mode at 100 %: DECIM holds samples (few changes), DROP leaves silences, DSTR clips under 0.85
        auto crp = [&] (int md) { GlitchParams c = z0; c.crp = 1.0f; c.cmd = md; std::vector<float> a, b; glRun (c, a, b, 2); return b; };
        const auto D = crp (GL_CDECIM), Pd = crp (GL_CDROP), S = crp (GL_CDSTR);
        int changes = 0; for (int i = 48001; i < 96000; ++i) if (fabs (D[i] - D[i - 1]) > 1e-4f) ++changes;
        int quiet = 0; for (int i = 48000; i < 96000; ++i) if (fabs (Pd[i]) < 1e-4f) ++quiet;
        double pk = 0; for (int i = 48000; i < 96000; ++i) pk = fmax (pk, fabs (S[i]));
        check (changes < 48000 / 20, "CRPT DECIM 100 %: the sample rate falls (value changes per second)", changes);
        check (quiet > 2400, "CRPT DROP 100 %: dropouts (silent samples per second)", quiet);
        check (pk < 0.85, "CRPT DSTR 100 %: clipped below 0.85 (peak)", pk);
    }
    // 1b. LOSS's RESO and its tape (ANA), on the effect alone
    {
        // any input (a function of the sample index), both channels out, the tape's speed error per block
        auto run2 = [&] (GlitchParams gp, std::function<void (int, float&, float&)> gen, double secs, std::vector<float>& oL, std::vector<float>& oR, std::vector<float>* cents) {
            gl->reset();
            std::vector<float> l (128), rr (128);
            for (int i = 0; i < (int) (secs * 48000); i += 128)
            {
                for (int k = 0; k < 128; ++k) gen (i + k, l[k], rr[k]);
                gl->process (l.data(), rr.data(), 128, gp, (double) i * 8.0 / 48000.0, 8.0 / 48000.0);
                oL.insert (oL.end(), l.begin(), l.end()); oR.insert (oR.end(), rr.begin(), rr.end());
                if (cents) cents->push_back (gl->lossCents);
            }
        };
        Rng nr; nr.seed (4242u);
        auto noise = [&] (int, float& a, float& b) { a = 0.3f * nr.bi(); b = 0.3f * nr.bi(); };
        // a chord of four tones in noise: RESO pulls the tones out of the codec's noise
        auto chord = [&] (int i, float& a, float& b) {
            const float t = (float) i / 48000.0f;
            a = 0.12f * (sinf (6.2831853f * 330.0f * t) + sinf (6.2831853f * 495.0f * t) + sinf (6.2831853f * 1320.0f * t) + sinf (6.2831853f * 2475.0f * t)) + 0.15f * nr.bi();
            b = a; };
        auto rms = [] (const std::vector<float>& x, int a, int b) { double e = 0; for (int i = a; i < b; ++i) e += x[i] * x[i]; return 10 * log10 (e / (b - a) + 1e-30); };
        GlitchParams d = z0; d.los = 0.6f; d.lmd = GL_LSTD;
        std::vector<float> c0L, c0R, c1L, c1R;
        nr.seed (5u); run2 (d, chord, 3.0, c0L, c0R, nullptr);
        GlitchParams dr = d; dr.res = 1.0f;
        nr.seed (5u); run2 (dr, chord, 3.0, c1L, c1R, nullptr);
        auto tonal = [&] (const std::vector<float>& x) {
            double t = 0; for (double f : { 330.0, 495.0, 1320.0, 2475.0 }) t += pow (10.0, bandDb (x, 96000, f - 12, f + 12) / 10);
            const double all = pow (10.0, bandDb (x, 96000, 200, 3000) / 10);
            return 10 * log10 (t / fmax (all - t, 1e-30)); };
        const double tn0 = tonal (c0L), tn1 = tonal (c1L), lv = rms (c1L, 96000, 140000) - rms (c0L, 96000, 140000);
        check (tn1 - tn0 > 3.0, "RESO 100 % (LOSS 60 % STD): the tones over the codec's noise (dB more)", tn1 - tn0);
        check (fabs (lv) < 3.0, "RESO 100 %: the level stays (dB)", lv);
        // and a resonance under the edge of the band (LOSS 100 %: the band ends near 4 kHz)
        std::vector<float> e0L, e0R, e1L, e1R;
        GlitchParams d2 = z0; d2.los = 1.0f;
        nr.seed (6u); run2 (d2, noise, 3.0, e0L, e0R, nullptr);
        d2.res = 1.0f; nr.seed (6u); run2 (d2, noise, 3.0, e1L, e1R, nullptr);
        const double bump = (bandDb (e1L, 96000, 2900, 3500) - bandDb (e1L, 96000, 600, 1200)) - (bandDb (e0L, 96000, 2900, 3500) - bandDb (e0L, 96000, 600, 1200));
        check (bump > 4.0, "RESO 100 % at LOSS 100 %: a resonance under the band's edge (dB)", bump);
        // ANA just above 0: the input, 512 samples late
        auto tone = [&] (int i, float& a, float& b) { a = 0.5f * sinf ((float) i * 0.0573f); b = a; };
        GlitchParams t0 = z0; t0.ana = 1; t0.los = 0.0021f; t0.res = 1.0f;
        std::vector<float> tL, tR; std::vector<float> in; for (int i = 0; i < 96000; ++i) { float a, b; tone (i, a, b); in.push_back (a); }
        run2 (t0, tone, 2.0, tL, tR, nullptr);
        double m = 0; for (int i = 48000; i < 90000; ++i) m = fmax (m, fabs (tL[i] - in[i - Glitch::FN]));
        check (m < 0.02, "ANA just above 0: the input, 512 samples late (max diff)", m);
        // the five machines at 100 %: their band, their speed error, their hiss; bounded
        const char* nm[GL_AMODES] = { "CASS", "VHS", "MICRO", "TOY", "WORN" };
        double cWow[GL_AMODES];
        for (int md = 0; md < GL_AMODES; ++md)
        {
            GlitchParams a = z0; a.ana = 1; a.amd = md; a.los = 1.0f; a.res = 0.35f;
            std::vector<float> nL, nR, sL, sR, wL, wR, w2L, w2R, cw, cf;
            nr.seed (7u); run2 (a, noise, 3.0, nL, nR, nullptr);
            run2 (a, [] (int, float& x, float& y) { x = y = 0.0f; }, 3.0, sL, sR, nullptr);
            a.spd = 0.0f; run2 (a, tone, 6.0, wL, wR, &cw);
            a.spd = 1.0f; run2 (a, tone, 3.0, w2L, w2R, &cf);
            double cmin = 1e9, cmax = -1e9, fmn = 1e9, fmx = -1e9;
            // the wow: the speed error over half a second (flutter and the snags average out: a snag comes back)
            {
                const int W = 48000 / 2 / 128;
                for (size_t i = 200 + W; i < cw.size(); ++i) { double m = 0; for (int j = 0; j < W; ++j) m += cw[i - j]; m /= W; cmin = fmin (cmin, m); cmax = fmax (cmax, m); }
            }
            for (size_t i = 200; i < cf.size(); ++i) { fmn = fmin (fmn, cf[i]); fmx = fmax (fmx, cf[i]); }
            cWow[md] = cmax - cmin;
            const double hi = bandDb (nL, 96000, 13000, 16000) - bandDb (nL, 96000, 500, 3000) + 10 * log10 (2500.0 / 3000.0);
            const double hissDb = rms (sL, 48000, 140000);
            double pk = 0; bool bad = false;
            for (auto* v : { &nL, &nR, &wL, &w2L }) for (float x : *v) { if (! std::isfinite (x)) bad = true; pk = fmax (pk, fabs (x)); }
            char what[120];
            snprintf (what, sizeof what, "ANA %s 100 %%: 13..16 kHz under the mids (dB per Hz)", nm[md]);
            check (hi < -12.0, what, hi);
            snprintf (what, sizeof what, "ANA %s 100 %%: wow (speed error over 0.5 s, range in cents)", nm[md]);
            check (cWow[md] > 3.0 && cWow[md] < 150.0, what, cWow[md]);
            snprintf (what, sizeof what, "ANA %s 100 %%: flutter alone (cents range)", nm[md]);
            check (fmx - fmn > 0.5 && fmx - fmn < 150.0, what, fmx - fmn);
            snprintf (what, sizeof what, "ANA %s 100 %%: hiss on silence (dBFS)", nm[md]);
            check (hissDb > -80.0 && hissDb < -38.0, what, hissDb);
            snprintf (what, sizeof what, "ANA %s 100 %%: finite, bounded (peak)", nm[md]);
            check (! bad && pk < 1.5, what, pk);
        }
        check (cWow[GL_AWORN] > cWow[GL_ACASS] * 2.5, "ANA: WORN wobbles more than CASS (ratio)", cWow[GL_AWORN] / cWow[GL_ACASS]);
        // MICRO: a one-head machine with no lows
        {
            GlitchParams a = z0; a.ana = 1; a.amd = GL_AMICRO; a.los = 1.0f;
            std::vector<float> L, R; nr.seed (8u); run2 (a, noise, 3.0, L, R, nullptr);
            double side = 0, all = 0; for (int i = 96000; i < 140000; ++i) { side += (L[i] - R[i]) * (L[i] - R[i]); all += L[i] * L[i] + R[i] * R[i]; }
            const double low = bandDb (L, 96000, 50, 120) - bandDb (L, 96000, 800, 870) + 10 * log10 (70.0 / 70.0);
            check (side / all < 0.01, "ANA MICRO 100 %: mono (side / all)", side / all);
            check (low < -10.0, "ANA MICRO 100 %: 50..120 Hz under 800 Hz (dB)", low);
        }
        // from the codec to the tape and back: no click (a tone, the second difference around the switches)
        {
            GlitchParams a = z0; a.los = 0.05f; a.res = 0.0f;
            std::vector<float> L, R; std::vector<float> l (128), rr (128);
            gl->reset(); double base = 0, sw = 0;
            for (int i = 0; i < 4 * 48000; i += 128)
            {
                a.ana = (i >= 48000 && i < 120000) ? 1 : 0;
                for (int k = 0; k < 128; ++k) tone (i + k, l[k], rr[k]);
                gl->process (l.data(), rr.data(), 128, a, (double) i * 8.0 / 48000.0, 8.0 / 48000.0);
                L.insert (L.end(), l.begin(), l.end());
            }
            for (int i = 30000; i < 46000; ++i) base = fmax (base, fabs (L[i] - 2 * L[i - 1] + L[i - 2]));
            for (int i = 46000; i < 180000; ++i) sw = fmax (sw, fabs (L[i] - 2 * L[i - 1] + L[i - 2]));
            check (sw < base * 1.5, "DIG <-> ANA: no click (second difference, x the steady tone)", sw / base);
        }
    }
    delete gl;
    // the engine with GLITCH on, all at zero: the reference for what follows
    std::vector<float> onL, onR;
    setup (false); e->g[G_XON] = 1.0f; e->setPlay (1); render (2.0, onL, onR, nullptr);
    // 2. RAND: it changes the hits, and back at zero the hits are the original ones again
    std::vector<float> rL, rR;
    setup (false); e->g[G_XON] = 1.0f; e->setPlay (1);
    render (2.0, rL, rR, [&] (int i) { e->g[G_XRND] = i < 48000 ? 0.8f : 0.0f; });
    const double dIn = maxDiff (onL, rL, 4800, 43000), dBack = maxDiff (onL, rL, 62400, 96000);   // (from 1.3 s: the hits that were sounding have died)
    if (getenv ("GLDBG")) for (int w0 = 48000; w0 < 96000; w0 += 4800) printf ("  rand back %.2f s: %.6f\n", w0 / 48000.0, maxDiff (onL, rL, w0, w0 + 4800));
    check (dIn > 0.05, "RAND 80 %: the hits move (max diff)", dIn);
    check (dBack < 1e-6, "RAND back to 0: the original hits again, bit for bit (max diff)", dBack);
    // 2b. LIVE (RAND on the SEQ page) does not need GLITCH on
    {
        std::vector<float> o0L, o0R, o1L, o1R;
        setup (false); e->setPlay (1); render (2.0, o0L, o0R, nullptr);
        setup (false); e->setPlay (1); render (2.0, o1L, o1R, [&] (int i) { e->g[G_XRND] = i < 48000 ? 0.8f : 0.0f; });
        const double a = maxDiff (o0L, o1L, 4800, 43000), b = maxDiff (o0L, o1L, 62400, 96000);
        check (a > 0.05 && b < 1e-6, "LIVE with GLITCH off: the hits move, then come back bit for bit (max diff)", a);
    }
    // 3. RAM is never moved by RAND (T1 muted: T2 alone)
    std::vector<float> a0L, a0R, a1L, a1R;
    setup (true); e->setMute (0, 1); e->g[G_XON] = 1.0f; e->setPlay (1); render (2.0, a0L, a0R, nullptr);
    setup (true); e->setMute (0, 1); e->g[G_XON] = 1.0f; e->g[G_XRND] = 1.0f; e->setPlay (1); render (2.0, a1L, a1R, nullptr);
    check (maxDiff (a0L, a1L, 0, (int) a0L.size()) == 0.0, "RAND 100 % leaves a RAM track as it is (max diff)", maxDiff (a0L, a1L, 0, (int) a0L.size()));
    // 4. the events: RPT, BRK, BEND each change the output, stay bounded, and leave a trace in the history
    auto evRun = [&] (int knob, float v, int& count) {
        std::vector<float> L, R;
        setup (true); e->g[G_XON] = 1.0f; e->g[knob] = v; e->g[G_XTIM] = 2.5f / GL_GRIDS; e->setPlay (1);
        render (3.0, L, R, nullptr);
        count = 0; for (int h = 0; h < Glitch::HIST; ++h) if (e->glitch.hist[h] > 0) ++count;
        double pk = 0; bool bad = false; for (float x : L) { pk = fmax (pk, fabs (x)); if (! std::isfinite (x)) bad = true; }
        return bad ? 99.0 : pk;
    };
    std::vector<float> refL, refR; setup (true); e->setPlay (1); render (3.0, refL, refR, nullptr);
    int cR = 0, cB = 0, cN = 0;
    const double pR = evRun (G_XRPT, 1.0f, cR), pB = evRun (G_XBRK, 1.0f, cB), pN = evRun (G_XBND, 1.0f, cN);
    check (cR >= 9 && pR < 2.0, "RPT 100 %: repeats on most divisions, bounded (events in the last 16)", cR);
    check (cB >= 6 && pB < 2.0, "BRK 100 %: jumps and dropouts, bounded (events in the last 16)", cB);
    check (cN >= 6 && pN < 2.0, "BEND 100 %: speed, reverse, tape stops, bounded (events in the last 16)", cN);
    // 5. HOLD: the last division loops (1/16 at 120 BPM = 6000 samples: two divisions apart, the same sound)
    {
        std::vector<float> L, R;
        setup (true); e->g[G_XON] = 1.0f; e->g[G_XTIM] = 2.5f / GL_GRIDS; e->setPlay (1);
        render (2.0, L, R, [&] (int i) { e->g[G_XHLD] = i >= 48000 ? 1.0f : 0.0f; });
        double num = 0, den = 0;
        for (int i = 60000; i < 66000; ++i) { num += (L[i] - L[i + 6000]) * (L[i] - L[i + 6000]); den += L[i] * L[i]; }
        check (num / (den + 1e-12) < 0.01, "HOLD: the held division repeats (residual between two loops)", num / (den + 1e-12));
    }
    // 6. LOSS at 100 %: the top is gone; every mode stays bounded
    {
        std::vector<float> L1, R1, in, inR;
        setup (true); e->setMute (0, 1); e->g[G_XON] = 1.0f; e->setPlay (1); render (2.0, in, inR, nullptr);
        setup (true); e->setMute (0, 1); e->g[G_XON] = 1.0f; e->g[G_XLOS] = 1.0f; e->setPlay (1); render (2.0, L1, R1, nullptr);
        const double hiIn = bandDb (in, 50000, 7000, 12000), hiOut = bandDb (L1, 50000, 7000, 12000);
        check (hiIn - hiOut > 20.0, "LOSS 100 % STD: 7..12 kHz down by more than 20 dB (dB)", hiIn - hiOut);
        bool bad = false;
        for (int md = 0; md < GL_LMODES; ++md)
        {
            std::vector<float> L, R;
            setup (true); e->g[G_XON] = 1.0f; e->g[G_XLOS] = 0.8f; e->g[G_XLMD] = (md + 0.5f) / GL_LMODES; e->g[G_XSPD] = md * 0.25f; e->setPlay (1);
            render (1.5, L, R, nullptr);
            for (float x : L) if (! std::isfinite (x) || fabs (x) > 4.0f) bad = true;
        }
        check (! bad, "LOSS: the five modes stay finite and bounded", bad ? 1 : 0);
        // the tape on the drums: the level stays near the input's (RMS, from 0.5 s), peaks bounded
        auto lvl = [] (const std::vector<float>& x) { double e = 0; for (size_t i = 24000; i < x.size(); ++i) e += x[i] * x[i]; return 10 * log10 (e / (x.size() - 24000) + 1e-30); };
        const double l0 = lvl (in);
        double worst = 0, worstN = 0, pk = 0;
        for (int md = 0; md < GL_AMODES; ++md)
            for (float los : { 0.5f, 1.0f })
            {
                std::vector<float> L, R;
                setup (true); e->g[G_XON] = 1.0f; e->g[G_XLOS] = los; e->g[G_XANA] = 1.0f; e->g[G_XAMD] = (md + 0.5f) / GL_AMODES; e->setPlay (1);
                render (2.0, L, R, nullptr);
                std::vector<float> ref, rR; setup (true); e->g[G_XON] = 1.0f; e->setPlay (1); render (2.0, ref, rR, nullptr);
                const double d = lvl (L) - lvl (ref);
                if (getenv ("GLDBG")) printf ("  ANA %d LOSS %.1f: %+.2f dB\n", md, los, d);
                double& w = (md == GL_AMICRO || md == GL_ATOY) ? worstN : worst;
                if (fabs (d) > fabs (w)) w = d;
                for (float x : L) pk = fmax (pk, fabs (x));
            }
        (void) l0;
        check (fabs (worst) < 3.0 && pk < 1.5, "ANA on the drums: CASS, VHS, WORN keep the level (worst dB, LOSS 50 / 100 %)", worst);
        check (fabs (worstN) < 6.0, "ANA on the drums: MICRO and TOY (no lows, no highs) near it (worst dB)", worstN);
    }
    // 8. everything at once, at random, for 10 s: no NaN, bounded
    {
        setup (true); e->g[G_XON] = 1.0f; e->setPlay (1);
        Rng r; r.seed (99u); bool bad = false; double pk = 0;
        std::vector<float> l (128), rr (128);
        for (int i = 0; i < 480000; i += 128)
        {
            if (i % 9600 == 0)
            {
                for (int gi = G_XRND; gi <= G_XHLD; ++gi) e->g[gi] = gi == G_XMIX ? 0.5f + 0.5f * r.uni() : (gi == G_XHLD ? (r.uni() < 0.1f ? 1.0f : 0.0f) : r.uni());
                e->g[G_XRES] = r.uni(); e->g[G_XANA] = r.uni() < 0.5f ? 1.0f : 0.0f; e->g[G_XAMD] = r.uni();
            }
            e->process (l.data(), rr.data(), 128, nullptr);
            for (int k = 0; k < 128; ++k) { if (! std::isfinite (l[k]) || ! std::isfinite (rr[k])) bad = true; pk = fmax (pk, fabs (l[k])); }
        }
        check (! bad && pk < 4.0, "everything at random for 10 s: finite, bounded (peak)", pk);
    }
    delete e;
    printf (fails ? "%d FAILED\n" : "GLITCH OK\n", fails);
    return fails ? 1 : 0;
}

// HALO: chords in the scale, held until the next trig, voice leading, the spread, the voice, the ensemble
static int halotest()
{
    Engine* e = new Engine(); e->init (48000.f);
    REF = new Engine(); REF->init (48000.f);
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-74s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    const float SH = 1.0f / HALO_SHAPES, PR = 1.0f / HALO_PROGS, MD = 1.0f / HALO_MODES;
    // a clean voice: sines, no drift, no air, no bloom, no width, no reverb; C minor on the quantiser
    auto setup = [&] (int shape, int prog) {
        e->init (48000.f);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setParam (0, P_MACH, machineVal (M_HALO)); e->setParam (0, P_VOL, 0.7f);
        e->setParam (0, P_A, (shape + 0.5f) * SH); e->setParam (0, P_B, (prog + 0.5f) * PR);
        e->setParam (0, P_C, 0.0f); e->setParam (0, P_D, 0.0f); e->setParam (0, P_E, 0.0f);
        e->setParam (0, P_HSWL, 0.2f); e->setParam (0, P_HDRF, 0.0f); e->setParam (0, P_HAIR, 0.0f); e->setParam (0, P_HWID, 0.0f);
        e->setParam (0, P_DEC, 0.3f);
        e->setScale (1453u);
        e->g[G_MXON] = 0.0f;
    };
    auto voice = [&] () -> Voice& { Track& t = e->tr[0]; return t.voice[t.cur]; };
    auto render = [&] (double secs, std::vector<float>& L, std::vector<float>& R, std::function<void (int)> at) {
        std::vector<float> l (128), r (128);
        for (int i = 0; i < (int) (secs * 48000); i += 128) { if (at) at (i); e->process (l.data(), r.data(), 128, nullptr); L.insert (L.end(), l.begin(), l.end()); R.insert (R.end(), r.begin(), r.end()); }
    };
    auto rmsDb = [] (const std::vector<float>& x, int a, int b) { double s2 = 0; for (int i = a; i < b; ++i) s2 += x[i] * x[i]; return 10 * log10 (s2 / (b - a) + 1e-30); };
    auto inC = [] (float st) { const int pc = (((int) floorf (st + 0.5f)) % 12 + 12) % 12; return ((1453u >> pc) & 1u) != 0; };
    // 1. the chords: C minor, CHRD 9TH on NOTE 0 is C Eb G Bb D; NOTE +2 (D) is D F Ab C Eb; every shape, on every
    //    degree, stays in the scale
    {
        setup (6, 0); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> L, R; render (0.2, L, R, nullptr);
        const HaloState& h = voice().hs;
        char got[64] = ""; for (int k = 0; k < h.n; ++k) snprintf (got + strlen (got), sizeof got - strlen (got), "%d ", (int) h.chord[k]);
        check (h.n == 5 && h.chord[0] == 0 && h.chord[1] == 3 && h.chord[2] == 7 && h.chord[3] == 10 && h.chord[4] == 14, "C minor, 9TH on the root: C Eb G Bb D (notes)", h.n);
        setup (6, 0); e->setLock (0, 0, P_NOTE, 0.5f + 2.0f / 48.0f); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> L2, R2; render (0.2, L2, R2, nullptr);
        const HaloState& h2 = voice().hs;
        check (h2.n == 5 && h2.chord[0] == 2 && h2.chord[1] == 5 && h2.chord[2] == 8 && h2.chord[3] == 12 && h2.chord[4] == 15, "NOTE +2: the chord on the 2nd degree, D F Ab C Eb (notes)", h2.n);
        int out = 0, total = 0;
        for (int sh = 0; sh < HALO_SHAPES; ++sh)
            for (int nt = -12; nt <= 12; ++nt)
            {
                setup (sh, 0); e->setLock (0, 0, P_NOTE, 0.5f + (float) nt / 48.0f); e->setTrig (0, 0, 1); e->setPlay (1);
                std::vector<float> a, b; render (0.02, a, b, nullptr);
                const HaloState& hh = voice().hs;
                for (int k = 0; k < hh.n; ++k) { ++total; if (! inC (hh.chord[k])) ++out; }
            }
        check (out == 0 && total > 900, "12 shapes x 25 notes: every chord note in C minor (out of key)", out);
        // without the quantiser: MODE picks the scale on TUNE's key (DOR on D: D F A C E for 9TH)
        setup (6, 0); e->setScale (0u); e->setParam (0, P_HMOD, (2 + 0.5f) * MD); e->setParam (0, P_TUNE, 0.5f + 2.0f / 48.0f);
        e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> L3, R3; render (0.2, L3, R3, nullptr);
        const HaloState& h3 = voice().hs;
        check (h3.n == 5 && h3.chord[0] == 2 && h3.chord[1] == 5 && h3.chord[2] == 9 && h3.chord[3] == 12 && h3.chord[4] == 16, "no quantiser, MODE DOR, TUNE +2: D dorian, D F A C E (notes)", h3.n);
    }
    // 2. the progressions: AEON walks i VI iv VII (C, Ab, F, Bb), one chord per trig; the diatonic ones stay in key
    {
        setup (3, 1); for (int s = 0; s < 16; s += 4) e->setTrig (0, s, 1); e->setPlay (1);
        int roots[4] = { -99, -99, -99, -99 };
        std::vector<float> L, R;
        render (2.0, L, R, [&] (int i) { const int k = i / 24000; if (k < 4 && i % 24000 >= 12000 && i % 24000 < 12128) roots[k] = (int) voice().hs.chord[0]; });
        check (roots[0] == 0 && roots[1] == -4 && roots[2] == 5 && roots[3] == -2, "PROG AEON: C, Ab, F, Bb (roots in semitones)", roots[1]);
        int out = 0;
        for (int pr = 1; pr < HALO_PROGS; ++pr)
        {
            if (pr == 10 || pr == 11) continue;                // SLIP, MEDI: outside on purpose
            setup (5, pr); for (int s = 0; s < 16; s += 2) e->setTrig (0, s, 1); e->setPlay (1);
            std::vector<float> a, b;
            render (2.0, a, b, [&] (int i) { if (i % 6016 == 3008) { const HaloState& hh = voice().hs; for (int k = 0; k < hh.n; ++k) if (! inC (hh.chord[k])) ++out; } });
        }
        check (out == 0, "the 10 diatonic progressions (and WALK) stay in C minor (notes out)", out);
    }
    // 3. the hold: one trig a bar, the chord holds the whole bar; GATE 2 steps lets it go; a stop lets it go
    {
        setup (5, 0); e->setParam (0, P_DEC, 0.6f); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> L, R; render (1.9, L, R, nullptr);
        const double a = rmsDb (L, 24000, 36000), b = rmsDb (L, 84000, 91000);
        check (fabs (a - b) < 1.0, "one trig a bar: the chord holds to the end of the bar (dB, start vs end)", a - b);
        setup (5, 0); e->setParam (0, P_DEC, 0.3f); e->setLock (0, 0, P_GATE, 2.0f / 8.0f); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> L2, R2; render (1.9, L2, R2, nullptr);
        check (rmsDb (L2, 72000, 91000) < -70.0, "GATE 2 steps: it lets go (dBFS late in the bar)", rmsDb (L2, 72000, 91000));
        setup (5, 0); e->setParam (0, P_DEC, 0.3f); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> L3, R3; render (2.5, L3, R3, [&] (int i) { if (i == 48000) e->setPlay (0); });
        check (rmsDb (L3, 30000, 46000) > -40.0 && rmsDb (L3, 96000, 120000) < -70.0, "the transport stops: it lets go (dBFS a second later)", rmsDb (L3, 96000, 120000));
        // muted: it lets go too
        setup (5, 0); e->setParam (0, P_DEC, 0.3f); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> L4, R4; render (2.0, L4, R4, [&] (int i) { if (i == 24064) e->setMute (0, 1); });
        check (rmsDb (L4, 72000, 90000) < -70.0, "the track muted: it lets go (dBFS)", rmsDb (L4, 72000, 90000));
    }
    // 4. chord changes without a click (a crossfade, then a glide), and the glide goes through the notes in between
    auto hfBurst = [&] (const std::vector<float>& x, const std::vector<int>& at) {
        // the high band (a one-pole high-pass at ~6 kHz) around each change, against its level elsewhere
        std::vector<float> hp (x.size()); float s = 0.0f, prev = 0.0f;
        for (size_t i = 0; i < x.size(); ++i) { s += 0.45f * (x[i] - s); hp[i] = x[i] - s - 0.0f * prev; prev = x[i]; }
        double base = 0, worst = 0;
        for (int i = 24000; i < (int) x.size() - 480; i += 480)
        {
            double e2 = 0; for (int k = 0; k < 480; ++k) e2 += hp[i + k] * hp[i + k];
            bool near = false; for (int a : at) if (abs (i - a) < 2400) near = true;
            if (near) worst = fmax (worst, e2); else base = fmax (base, e2);
        }
        return 10 * log10 ((worst + 1e-20) / (base + 1e-20));
    };
    {
        setup (5, 1); e->setParam (0, P_D, 0.4f); for (int s = 0; s < 16; s += 4) e->setTrig (0, s, 1); e->setPlay (1);
        std::vector<float> L, R; render (4.0, L, R, nullptr);
        std::vector<int> at; for (int k = 1; k < 8; ++k) at.push_back (k * 24000);
        const double b0 = hfBurst (L, at);
        check (b0 < 3.0, "chord changes, crossfaded (SAW): no click (high band at the changes vs elsewhere, dB)", b0);
        setup (5, 1); e->setParam (0, P_D, 0.4f); e->setParam (0, P_HGLD, 0.6f); for (int s = 0; s < 16; s += 4) e->setTrig (0, s, 1); e->setPlay (1);
        std::vector<float> L2, R2; float mid = 0.0f;
        render (4.0, L2, R2, [&] (int i) { if (i == 24064 + 4864) { const HaloState& hh = voice().hs; for (int s = 0; s < HALO_SLOTS; ++s) { const float f = hh.s[s].pitch - floorf (hh.s[s].pitch); if (hh.s[s].stage && f > 0.05f && f < 0.95f) mid = 1.0f; } } });
        const double b1 = hfBurst (L2, at);
        check (b1 < 3.0, "chord changes, gliding (GLID 60 %): no click (dB)", b1);
        check (mid > 0.5f, "GLID: 0.1 s into a change, a voice is between two notes", mid);
    }
    // 5. SPRD and INV: swept under a held chord of sines, no click (a click is a lone spike of the second difference
    //    over its neighbourhood); at the top of SPRD the bass is an octave down
    auto spike = [] (const std::vector<float>& x, int a, int b) {
        const int W = 96; std::vector<double> e;
        for (int i = a; i + W < b; i += W) { double s2 = 0; for (int k = 0; k < W; ++k) { const double d = x[i + k] - 2.0 * x[i + k - 1] + x[i + k - 2]; s2 += d * d; } e.push_back (s2); }
        double worst = 0;
        for (int i = 10; i + 10 < (int) e.size(); ++i)
        {
            std::vector<double> nb; for (int j = i - 10; j <= i + 10; ++j) if (j != i) nb.push_back (e[j]);
            std::nth_element (nb.begin(), nb.begin() + nb.size() / 2, nb.end());
            worst = fmax (worst, 10 * log10 ((e[i] + 1e-24) / (nb[nb.size() / 2] + 1e-24)));
        }
        return worst;
    };
    {
        setup (6, 0); e->setTrig (0, 0, 1); e->setSpeed (0, 0); e->setPlay (1);
        std::vector<float> L, R; render (6.0, L, R, [&] (int i) { e->setParam (0, P_C, clampf ((float) (i - 48000) / (4.0f * 48000.0f), 0.0f, 1.0f)); });
        std::vector<float> L0, R0; setup (6, 0); e->setTrig (0, 0, 1); e->setSpeed (0, 0); e->setPlay (1); render (6.0, L0, R0, nullptr);
        const double sp = spike (L, 48000, 5 * 48000), sp0 = spike (L0, 48000, 5 * 48000);
        check (sp < sp0 + 3.0, "SPRD swept 0 -> 100 % in 4 s under the chord: no click (worst spike, dB over a held chord's)", sp - sp0);
        const double bass = bandDb (L, 5 * 48000 + 4000, 62, 69) - bandDb (L0, 5 * 48000 + 4000, 62, 69);
        check (bass > 20.0, "SPRD 100 %: the root an octave down appears (65 Hz, dB against close)", bass);
        setup (6, 0); e->setTrig (0, 0, 1); e->setSpeed (0, 0); e->setPlay (1);
        std::vector<float> L2, R2; render (6.0, L2, R2, [&] (int i) { e->setParam (0, P_HINV, clampf ((float) (i - 48000) / (4.0f * 48000.0f), 0.0f, 1.0f)); });
        const double si = spike (L2, 48000, 5 * 48000);
        check (si < sp0 + 3.0, "INV swept 0 -> 100 % in 4 s: no click (dB over a held chord's)", si - sp0);
    }
    // 6. WAVE: SINE .. METAL at about the same loudness; BLOOM and AIR add without blowing up the level
    {
        double lo = 1e9, hi = -1e9;
        for (int w = 0; w <= 10; ++w)
        {
            setup (5, 0); e->setParam (0, P_D, w / 10.0f); e->setTrig (0, 0, 1); e->setPlay (1);
            std::vector<float> L, R; render (1.0, L, R, nullptr);
            const double r = rmsDb (L, 24000, 46000);
            if (getenv ("HDBG")) printf ("  WAVE %.1f: %.2f dB\n", w / 10.0, r);
            lo = fmin (lo, r); hi = fmax (hi, r);
        }
        check (hi - lo < 3.0, "WAVE 0 .. 100 %: the level stays (range, dB)", hi - lo);
        double worst = 0, pk = 0; bool bad = false;
        for (float bl : { 0.0f, 0.5f, 1.0f }) for (float ai : { 0.0f, 0.5f, 1.0f })
        {
            setup (7, 0); e->setParam (0, P_D, 0.5f); e->setParam (0, P_E, bl); e->setParam (0, P_HAIR, ai); e->setParam (0, P_HDRF, 1.0f); e->setParam (0, P_C, 1.0f);
            e->setTrig (0, 0, 1); e->setPlay (1);
            std::vector<float> L, R; render (1.0, L, R, nullptr);
            const double r = rmsDb (L, 24000, 46000);
            if (getenv ("HDBG")) printf ("  BLOOM %.1f AIR %.1f: %.2f dB\n", bl, ai, r);
            worst = fmax (worst, fabs (r - rmsDb (L, 24000, 46000)));
            for (float x : L) { pk = fmax (pk, fabs (x)); if (! std::isfinite (x)) bad = true; }
        }
        check (! bad && pk < 1.0, "BLOOM, AIR, DRFT and SPRD at the top, 11TH: finite, bounded (peak)", pk);
    }
    // 7. WIDE: the sides open, the mono sum stays the track's
    {
        setup (6, 0); e->setParam (0, P_D, 0.45f); e->setParam (0, P_HWID, 1.0f); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> L, R; render (2.0, L, R, nullptr);
        double sl = 0, sr2 = 0, slr = 0, side = 0, mid = 0;
        for (int i = 48000; i < 90000; ++i) { sl += L[i] * L[i]; sr2 += R[i] * R[i]; slr += L[i] * R[i]; side += (L[i] - R[i]) * (L[i] - R[i]); mid += (L[i] + R[i]) * (L[i] + R[i]); }
        const double corr = slr / sqrt (sl * sr2 + 1e-30), sideDb = 10 * log10 (side / (mid + 1e-30));
        check (corr < 0.9 && sideDb > -16.0, "WIDE 100 %: L / R decorrelated (correlation)", corr);
        setup (6, 0); e->setParam (0, P_D, 0.45f); e->setParam (0, P_HWID, 0.0f); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> L0, R0; render (2.0, L0, R0, nullptr);
        double dz = 0; for (int i = 0; i < (int) L0.size(); ++i) dz = fmax (dz, fabs (L0[i] - R0[i]));
        check (dz < 1e-6, "WIDE 0: mono, as the other machines (L - R)", dz);
        const double mono = rmsDb (L, 48000, 90000) + 0.0, m0 = rmsDb (L0, 48000, 90000);
        check (fabs (mono - m0) < 2.0, "WIDE 100 %: the level stays (dB against WIDE 0)", mono - m0);
    }
    // 8. what it costs: a six-note chord, SAW > PULSE, BLOOM, AIR, DRFT, SPRD at the top, WIDE (native, % of real time)
    {
        setup (7, 1); e->setParam (0, P_D, 0.5f); e->setParam (0, P_E, 1.0f); e->setParam (0, P_HAIR, 1.0f); e->setParam (0, P_HDRF, 1.0f);
        e->setParam (0, P_C, 0.5f); e->setParam (0, P_HWID, 1.0f); e->setParam (0, P_HGLD, 0.5f);
        for (int s = 0; s < 16; s += 4) e->setTrig (0, s, 1); e->setPlay (1);
        std::vector<float> l (128), r (128);
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 10 * 48000; i += 128) e->process (l.data(), r.data(), 128, nullptr);
        const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        printf ("INFO the whole engine with one HALO track at its heaviest: %.1f %% of real time\n", el / 10.0 * 100.0);
    }
    delete e;
    printf (fails ? "%d FAILED\n" : "HALO OK\n", fails);
    return fails ? 1 : 0;
}

static int kittest()
{
    Engine* e = new Engine(); e->init (48000.f);
    REF = new Engine(); REF->init (48000.f);
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-62s %.2f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    // HEX: a hard hit is brighter than a soft one (the velocity opens the noise filter); a hit always ends
    {
        const auto soft = kitHit (e, M_HEXSD, 0.3f, 1.0f, {}), hard = kitHit (e, M_HEXSD, 1.0f, 1.0f, {});
        const double cs = centroid (soft, 0), ch = centroid (hard, 0);
        check (ch > cs * 1.1, "HEX·SD: hard hit brighter than soft (centroid ratio)", ch / cs);
        int last = 0; for (int i = 0; i < (int) hard.size(); ++i) if (fabsf (hard[i]) > 0.001f) last = i;
        check (last < 48000 * 0.9, "HEX·SD: the hit has ended within 0.9 s (s)", last / 48000.0);
    }
    // HEX·TM: SIZE LOW sounds lower than HI (zero crossings over 100 ms, tone only, no bend)
    {
        auto zc = [] (const std::vector<float>& x) { int n = 0; for (int i = 2400; i < 7200; ++i) if ((x[i - 1] < 0) != (x[i] < 0)) ++n; return n; };
        const auto hi = kitHit (e, M_HEXTM, 0.8f, 0.3f, { { P_B, 0.5f }, { P_C, 0.0f }, { P_D, 0.0f }, { P_F, 0.1f } });
        const auto lo = kitHit (e, M_HEXTM, 0.8f, 0.3f, { { P_B, 0.5f }, { P_C, 0.0f }, { P_D, 0.0f }, { P_F, 0.9f } });
        check (zc (hi) > zc (lo) * 2, "HEX·TM: HI more than twice the crossings of LOW", (double) zc (hi) / zc (lo));
    }
    delete e;
    printf (fails ? "%d FAILED\n" : "KIT OK\n", fails);
    return fails ? 1 : 0;
}

// v59: HALO's harmony (HARM, the new progressions), STRUM, CHOIR
static int harmtest()
{
    Engine* e = new Engine(); e->init (48000.f);
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-82s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    const float SH = 1.0f / HALO_SHAPES, PR = 1.0f / HALO_PROGS;
    auto setup = [&] (int shape, int prog, int harm, u32 scale) {
        e->init (48000.f, 1);      // (lite: no BOX bank, no PRISM tables to build each time)
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setParam (0, P_MACH, machineVal (M_HALO)); e->setParam (0, P_VOL, 0.7f);
        e->setParam (0, P_A, (shape + 0.5f) * SH); e->setParam (0, P_B, (prog + 0.5f) * PR);
        e->setParam (0, P_C, 0.0f); e->setParam (0, P_D, 0.0f); e->setParam (0, P_E, 0.0f);
        e->setParam (0, P_HSWL, 0.1f); e->setParam (0, P_HDRF, 0.0f); e->setParam (0, P_HAIR, 0.0f); e->setParam (0, P_HWID, 0.0f);
        e->setParam (0, P_HARM, (harm + 0.5f) / 3.0f);
        e->setScale (scale);
        e->g[G_MXON] = 0.0f;
    };
    auto voice = [&] () -> Voice& { Track& t = e->tr[0]; return t.voice[t.cur]; };
    auto run = [&] (double secs, std::function<void (int)> at) {
        std::vector<float> l (128), r (128);
        for (int i = 0; i < (int) (secs * 48000); i += 128) { if (at) at (i); e->process (l.data(), r.data(), 128, nullptr); }
    };
    auto chordOf = [&] () { const HaloState& h = voice().hs; std::vector<int> c; for (int k = 0; k < h.n; ++k) c.push_back ((int) h.chord[k]); return c; };
    const u32 MAJ = 2741u, MIN = 1453u, HMIN = 2477u + 0u;
    // 1. PURE: every shape on every degree of C major, C minor and A harmonic minor: no note a semitone, a minor ninth (or
    //    two octaves and a semitone) over another, no diminished or augmented fifth, under E3 no third or second over the root
    {
        int bad9 = 0, badF = 0, badLow = 0, total = 0, out = 0;
        const u32 scales[3] = { MAJ, MIN, 2869u };      // (2869: A harmonic minor: A B C D E F G#)
        for (int sc = 0; sc < 3; ++sc)
            for (int sh = 0; sh < HALO_SHAPES; ++sh)
                for (int nt = -7; nt <= 12; ++nt)
                {
                    setup (sh, 0, HARM_PURE, scales[sc]);
                    e->setLock (0, 0, P_NOTE, 0.5f + (float) nt / 48.0f); e->setTrig (0, 0, 1); e->setPlay (1);
                    run (0.02, nullptr);
                    const std::vector<int> c = chordOf ();
                    ++total;
                    for (size_t a = 0; a < c.size(); ++a)
                    {
                        const int pc = ((c[a] % 12) + 12) % 12;
                        if (! ((scales[sc] >> pc) & 1u)) ++out;
                        for (size_t b = 0; b < c.size(); ++b) { const int d = c[b] - c[a]; if (d == 1 || d == 13 || d == 25) { if (bad9 < 6) { printf ("      b9: scale %d shape %d note %d:", sc, sh, nt); for (int x : c) printf (" %d", x); printf ("\n"); } ++bad9; } }
                        if (a > 0 && c[a] < 4 && c[a] - c[0] < 7) ++badLow;
                    }
                    // the fifth over the root, when the shape has one (TRI, 7TH, ...)
                    // (a tritone over the root, or an augmented triad; a sixth over the root is a first inversion: G/B)
                    if (sh == 3 || sh == 5) { bool has[12] = {}; for (int x : c) has[(((x - c[0]) % 12) + 12) % 12] = true; if (has[6] || (has[4] && has[8])) { if (badF < 4) { printf ("      fifth: scale %d shape %d note %d:", sc, sh, nt); for (int x : c) printf (" %d", x); printf ("\n"); } ++badF; } }
                }
        check (bad9 == 0, "PURE: 12 shapes x 20 notes x 3 scales, no minor second / ninth over a note (pairs)", bad9);
        check (badF == 0, "PURE: TRI and 7TH never with a diminished or augmented fifth (chords)", badF);
        check (badLow == 0, "PURE: under E3, no third or second over the root (notes)", badLow);
        check (out == 0 && total == 720, "PURE: every note still in the scale (notes out)", out);
    }
    // 2. FREE is the chord as stacked (v58): C major 9TH on the 3rd degree: E G B D F (the F a minor ninth over E); PURE: E G B D
    {
        setup (6, 0, HARM_FREE, MAJ); e->setLock (0, 0, P_NOTE, 0.5f + 4.0f / 48.0f); e->setTrig (0, 0, 1); e->setPlay (1); run (0.02, nullptr);
        std::vector<int> c = chordOf ();
        check (c.size() == 5 && c[0] == 4 && c[4] == 17, "FREE: C major, 9TH on E: E G B D F (as v58 stacked it)", c.size());
        setup (6, 0, HARM_PURE, MAJ); e->setLock (0, 0, P_NOTE, 0.5f + 4.0f / 48.0f); e->setTrig (0, 0, 1); e->setPlay (1); run (0.02, nullptr);
        c = chordOf ();
        check (c.size() == 4 && c[0] == 4 && c[3] == 14, "PURE: the same, the F gone: E G B D", c.size());
        // B diminished (TRI on B in C major): its F moves up a step, G/B
        setup (3, 0, HARM_PURE, MAJ); e->setLock (0, 0, P_NOTE, 0.5f - 1.0f / 48.0f); e->setTrig (0, 0, 1); e->setPlay (1); run (0.02, nullptr);
        c = chordOf ();
        check (c.size() == 3 && c[0] == -1 && c[1] == 7 && c[2] == 14, "PURE: TRI on B2 (diminished): B2 G3 D4 (the fifth a step up, the low third opened)", c.size() == 3 ? c[1] : -1);
    }
    // 3. the classical progressions: CANON in C major walks I V vi iii IV I IV V (roots C G A E F C F G)
    {
        setup (3, 12, HARM_PURE, MAJ); for (int s = 0; s < 16; s += 2) e->setTrig (0, s, 1); e->setPlay (1);
        int roots[8]; for (int k = 0; k < 8; ++k) roots[k] = -99;
        run (2.0, [&] (int i) { const int k = i / 12000; if (k < 8 && i % 12000 >= 6000 && i % 12000 < 6128) roots[k] = (((int) voice().hs.chord[0] % 12) + 12) % 12; });
        const int want[8] = { 0, 7, 9, 4, 5, 0, 5, 7 };
        int ok = 0; for (int k = 0; k < 8; ++k) if (roots[k] == want[k]) ++ok;
        check (ok == 8, "PROG CANON (C major): C G A E F C F G (roots right, of 8)", ok);
        // PEDL: the tonic an octave down under every chord
        setup (3, 23, HARM_PURE, MIN); for (int s = 0; s < 16; s += 4) e->setTrig (0, s, 1); e->setPlay (1);
        int ped = 0;
        run (2.0, [&] (int i) { if (i % 24000 >= 12000 && i % 24000 < 12128) { const std::vector<int> c = chordOf (); if (! c.empty() && c[0] == -12) ++ped; } });
        check (ped == 4, "PROG PEDL: C2 under each of the 4 chords", ped);
        // TIN↓: the triad, the melody a step lower each trig (C5 B4 A4 ...), the triad's note just under it
        setup (3, 24, HARM_PURE, MAJ); for (int s = 0; s < 16; s += 2) e->setTrig (0, s, 1); e->setPlay (1);
        int mel[8], tv[8]; for (int k = 0; k < 8; ++k) mel[k] = tv[k] = -99;
        run (2.0, [&] (int i) { const int k = i / 12000; if (k < 8 && i % 12000 >= 6000 && i % 12000 < 6128) { const std::vector<int> c = chordOf (); mel[k] = c.back(); tv[k] = c.size() > 1 ? c[c.size() - 2] : -99; } });
        const int wm[8] = { 24, 23, 21, 19, 17, 16, 14, 12 };
        int okm = 0, okt = 0;
        for (int k = 0; k < 8; ++k)
        {
            if (mel[k] == wm[k]) ++okm;
            const int pc = ((tv[k] % 12) + 12) % 12;
            if ((pc == 0 || pc == 4 || pc == 7) && tv[k] < mel[k] && mel[k] - tv[k] <= 5) ++okt;
        }
        check (okm == 8, "PROG TIN↓ (C major): the melody C5 B4 A4 G4 F4 E4 D4 C4 (right, of 8)", okm);
        check (okt >= 7, "TIN↓: the T voice, the triad's note just under the melody (right, of 8)", okt);
    }
    // 4. LEAD: AXIS with 7THs, the upper voices move little (mean semitones a voice per change), and stay round home
    {
        double moveL = 0, moveF = 0; int nL = 0, nF = 0; int far = 0;
        for (int pass = 0; pass < 2; ++pass)
        {
            setup (5, 13, pass ? HARM_LEAD : HARM_PURE, MAJ); for (int s = 0; s < 16; s += 2) e->setTrig (0, s, 1); e->setPlay (1);
            std::vector<int> last;
            run (4.0, [&] (int i) {
                if (i % 12000 < 6000 || i % 12000 >= 6128) return;
                const std::vector<int> c = chordOf ();
                // (the upper voices in order against the last chord's: how far they move together)
                if (c.size() > 1 && last.size() > 1)
                {
                    const size_t m = std::min (c.size(), last.size());
                    for (size_t k = 1; k < m; ++k)
                    {
                        const int d = abs (c[c.size() - m + k] - last[last.size() - m + k]);
                        if (pass) { moveL += d; ++nL; if (c[k] < 0 || c[k] > 20) ++far; } else { moveF += d; ++nF; }
                    }
                }
                last = c;
            });
        }
        printf ("      voice motion: LEAD %.2f, PURE %.2f semitones a voice\n", moveL / std::max (1, nL), moveF / std::max (1, nF));
        check (nL > 0 && moveL / nL < 2.5 && moveL / nL < moveF / std::max (1, nF), "LEAD (AXIS, 7THs): the upper voices move less than PURE's (semitones a voice)", moveL / std::max (1, nL));
        check (far == 0, "LEAD: they stay between C3 and G#4 (notes out)", far);
    }
    // 5. a clashing note of the last chord goes fast (PURE): i -> VI in C minor (G against Ab)
    {
        setup (3, 3, HARM_PURE, MIN); e->setParam (0, P_DEC, 0.9f); e->setTrig (0, 0, 1); e->setTrig (0, 8, 1); e->setPlay (1);
        int fast2 = 0;
        run (1.2, [&] (int i) { if (i == 64 * 128 * 4) { } if (i > 49000 && i < 49300) { const HaloState& h = voice().hs; for (int s = 0; s < HALO_SLOTS; ++s) if (h.s[s].stage == 3 && h.s[s].fast == 2) ++fast2; } });
        check (fast2 > 0, "PURE: TIDE (i VI), the G against the Ab let go in 150 ms (slots)", fast2);
    }
    // 6. HALO's TONE at rest changes nothing; CUT darkens, SUB adds the octave under the bass, WARM keeps the level
    auto renderHalo = [&] (std::function<void ()> set) {
        setup (3, 0, HARM_FREE, MIN); e->setParam (0, P_D, 0.4f); set (); e->setTrig (0, 0, 1); e->setSpeed (0, 0); e->setPlay (1);
        std::vector<float> L; std::vector<float> l (128), r (128);
        for (int i = 0; i < 96000; i += 128) { e->process (l.data(), r.data(), 128, nullptr); L.insert (L.end(), l.begin(), l.end()); }
        return L;
    };
    {
        const std::vector<float> a = renderHalo ([] () {}), b = renderHalo ([&] () { e->setParam (0, P_HUNI, 0.0f); e->setParam (0, P_HCUT, 1.0f); });
        double d = 0; for (size_t i = 0; i < a.size(); ++i) d = std::max (d, (double) fabsf (a[i] - b[i]));
        check (d == 0.0, "TONE at rest: the same samples (max difference)", d);
        const std::vector<float> c = renderHalo ([&] () { e->setParam (0, P_HCUT, 0.15f); });
        check (bandDb (a, 48000, 2000, 6000) - bandDb (c, 48000, 2000, 6000) > 15.0, "CUT 15 %: 2..6 kHz down (dB)", bandDb (a, 48000, 2000, 6000) - bandDb (c, 48000, 2000, 6000));
        const std::vector<float> sb = renderHalo ([&] () { e->setParam (0, P_HSUB, 1.0f); });
        check (bandDb (sb, 48000, 60, 70) - bandDb (a, 48000, 60, 70) > 15.0, "SUB (v71): the sine an octave under the lowest note, C2 (65 Hz), appears (dB)", bandDb (sb, 48000, 60, 70) - bandDb (a, 48000, 60, 70));
        const std::vector<float> wm = renderHalo ([&] () { e->setParam (0, P_HWRM, 1.0f); });
        double ea = 0, ew = 0; for (int i = 48000; i < 96000; ++i) { ea += a[i] * a[i]; ew += wm[i] * wm[i]; }
        check (fabs (10 * log10 (ew / ea)) < 2.0, "WARM 100 %: the level stays (dB)", 10 * log10 (ew / ea));
        const std::vector<float> un = renderHalo ([&] () { e->setParam (0, P_HUNI, 1.0f); e->setParam (0, P_HVIB, 1.0f); e->setParam (0, P_HOPN, 1.0f); });
        double eu = 0, pk = 0; for (int i = 48000; i < 96000; ++i) { eu += un[i] * un[i]; pk = std::max (pk, (double) fabsf (un[i])); }
        check (fabs (10 * log10 (eu / ea)) < 3.0 && pk < 1.0, "UNIS, VIB, OPEN at the top: bounded, the level within 3 dB (dB)", 10 * log10 (eu / ea));
    }
    delete e;
    printf (fails ? "%d FAILED\n" : "HARM OK\n", fails);
    return fails ? 1 : 0;
}

static int strumtest()
{
    Engine* e = new Engine(); e->init (48000.f);
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-82s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    auto setup = [&] (int shape, int prog, float strm, int dir) {
        e->init (48000.f);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setParam (0, P_MACH, machineVal (M_STRUM)); e->setParam (0, P_VOL, 0.7f);
        e->setParam (0, P_A, (shape + 0.5f) / HALO_SHAPES); e->setParam (0, P_B, (prog + 0.5f) / HALO_PROGS);
        e->setParam (0, P_C, 0.35f); e->setParam (0, P_D, strm); e->setParam (0, P_E, (dir + 0.5f) / STRUM_DIRS);
        e->setParam (0, P_HARM, 1.5f / 3.0f); e->setParam (0, P_SHUM, 0.0f); e->setParam (0, P_HWID, 0.0f);
        e->setScale (2741u);
        e->g[G_MXON] = 0.0f;
    };
    auto voice = [&] () -> Voice& { Track& t = e->tr[0]; return t.voice[t.cur]; };
    auto render = [&] (double secs, std::vector<float>& L, std::vector<float>& R, std::function<void (int)> at) {
        std::vector<float> l (128), r (128);
        for (int i = 0; i < (int) (secs * 48000); i += 128) { if (at) at (i); e->process (l.data(), r.data(), 128, nullptr); L.insert (L.end(), l.begin(), l.end()); R.insert (R.end(), r.begin(), r.end()); }
    };
    auto rmsDb = [] (const std::vector<float>& x, int a, int b) { double s2 = 0; for (int i = a; i < b; ++i) s2 += x[i] * x[i]; return 10 * log10 (s2 / (b - a) + 1e-30); };
    // 1. the strings of C major: from C3 up, C E G in it, no two closer than SPRD's, six at most
    {
        setup (3, 0, 0.3f, 0); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> L, R; render (0.05, L, R, nullptr);
        const StrumState& ss = voice().ss;
        bool hasC = false, hasE = false, hasG = false, close = false;
        for (int j = 0; j < ss.n; ++j) { const int pc = (((int) ss.str[j]) % 12 + 12) % 12; hasC |= pc == 0; hasE |= pc == 4; hasG |= pc == 7; if (j && ss.str[j] - ss.str[j - 1] < 3) close = true; }
        check (ss.n >= 4 && ss.n <= 6 && ss.str[0] == 0.0f && hasC && hasE && hasG && ! close, "TRI on C: the bass C3, C E G on the strings, none closer than 3 (strings)", ss.n);
    }
    // 2. the strum: DOWN strikes the bass first, gaps even; UP the top first; D/U turns; a retrig strums the same chord back
    {
        setup (3, 0, 0.5f, 0); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> L, R; render (0.003, L, R, nullptr);
        const StrumState& ss = voice().ss;
        int waits[6]; for (int j = 0; j < 6; ++j) waits[j] = -1;
        for (int k = 0; k < STRUM_VOICES; ++k) for (int j = 0; j < 6; ++j) if (ss.cur[j] == k && ss.s[k].pend >= 0) waits[j] = ss.s[k].pend;
        // (after 128 samples the first has struck: the others wait their turn)
        const double T = 0.003 * pow (500.0, 0.5) * 48000.0, gap = T / (ss.n - 1);
        bool order = true; double err = 0;
        for (int j = 2; j < ss.n; ++j) { if (waits[j] <= waits[j - 1]) order = false; err = std::max (err, fabs ((waits[j] - waits[j - 1]) - gap)); }
        check (order && err < 2.0, "DOWN, STRM 50 %: the bass first, the strings an even gap apart (worst gap error, samples)", err);
        setup (3, 0, 0.5f, 1); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> L2, R2; render (0.003, L2, R2, nullptr);
        const StrumState& s2 = voice().ss;
        int wb = -1, wt = -1; for (int k = 0; k < STRUM_VOICES; ++k) { if (s2.cur[0] == k) wb = s2.s[k].pend; if (s2.cur[s2.n - 1] == k) wt = s2.s[k].pend; }
        check (wb > 0 && wt < 0, "UP: the top string first, the bass last (bass still waiting, samples)", wb);
        // D/U and the retrigs: the direction turns each trig; a retrig does not move the progression on
        setup (3, 13, 0.4f, 2); for (int s = 0; s < 16; s += 4) e->setTrig (0, s, 1); e->setParam (0, P_RTRG, 3.5f / NUM_RETRIG); e->setParam (0, P_RCNT, 2.5f / NUM_RCOUNT); e->setPlay (1);
        int dirs[8], progs[8]; for (int k = 0; k < 8; ++k) dirs[k] = progs[k] = 0;
        std::vector<float> L3, R3;
        render (2.0, L3, R3, [&] (int i) { const int k = i / 6000; if (k < 8 && i % 6000 >= 4000 && i % 6000 < 4128) { dirs[k] = voice().ss.dir; progs[k] = e->tr[0].hProg; } });
        bool alt = true; for (int k = 1; k < 8; ++k) if (dirs[k] == dirs[k - 1]) alt = false;
        check (alt, "D/U with a retrig on each trig: down, up, down... every strum the other way", dirs[1]);
        check (progs[1] == progs[0] && progs[3] == progs[0] && progs[4] == progs[0] + 1, "a retrig strums the same chord (the progression moves on at the next trig only)", progs[4] - progs[0]);
    }
    // 3. in tune: every string of an open chord, each material, within 2 cents of its note
    {
        double worst = 0;
        for (int mat = 0; mat < STRUM_MATS; ++mat)
        {
            setup (0, 0, 0.0f, 0); e->setParam (0, P_C, 0.0f); e->setParam (0, P_DEC, 0.8f); e->setParam (0, P_SBDY, 0.0f); e->setParam (0, P_SMAT, (mat + 0.5f) / STRUM_MATS);
            e->setTrig (0, 0, 1); e->setSpeed (0, 0); e->setPlay (1);
            std::vector<float> L, R; render (1.2, L, R, nullptr);
            // C3 G3 C4 G4 C5: G3 and G4 are alone at their pitch
            for (int st : { 7, 19 })
            {
                const double f = 130.8128 * pow (2.0, st / 12.0);
                double best = f, bm = 0;
                for (double fr = f * 0.99; fr <= f * 1.01; fr += f * 0.0002)
                {
                    double re = 0, im = 0; const int N = 32768, a = 14000;
                    for (int i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * cos (2 * M_PI * i / N); re += L[a + i] * w * cos (2 * M_PI * fr * i / 48000.0); im += L[a + i] * w * sin (2 * M_PI * fr * i / 48000.0); }
                    const double m = re * re + im * im; if (m > bm) { bm = m; best = fr; }
                }
                worst = std::max (worst, fabs (1200 * log2 (best / f)));
            }
        }
        check (worst < 2.0, "6 materials: G3 and G4 within 2 cents of the scale (worst, cents)", worst);
    }
    // 4. DEC: longer rings longer; HOLD sustains while the gate is open; a stop damps the strings
    {
        double dec[2];
        for (int k = 0; k < 2; ++k)
        {
            setup (3, 0, 0.0f, 0); e->setParam (0, P_DEC, k ? 0.8f : 0.3f); e->setTrig (0, 0, 1); e->setSpeed (0, 0); e->setPlay (1);
            std::vector<float> L, R; render (2.0, L, R, nullptr);
            dec[k] = rmsDb (L, 4800, 9600) - rmsDb (L, 72000, 81600);
        }
        check (dec[1] < dec[0] - 10.0, "DEC 80 % rings longer than 30 % (dB lost from 0.1 s to 1.5 s, 80 %)", dec[1]);
        setup (3, 0, 0.0f, 0); e->setParam (0, P_DEC, 1.0f); e->setTrig (0, 0, 1); e->setSpeed (0, 0); e->setPlay (1);
        std::vector<float> L, R; render (4.0, L, R, [&] (int i) { if (i == 3 * 48000) e->setPlay (0); });
        const double held = rmsDb (L, 4800, 9600) - rmsDb (L, 120000, 134400), after = rmsDb (L, 3 * 48000 + 24000, 4 * 48000 - 128);
        check (held < 8.0, "HOLD: the strings sustain while the gate is open (dB lost in 2.6 s)", held);
        check (after < -70.0, "HOLD, the transport stops: the strings damped (dBFS 0.5 s later)", after);
        setup (3, 0, 0.0f, 0); e->setParam (0, P_DEC, 0.9f); e->setTrig (0, 0, 1); e->setSpeed (0, 0); e->setPlay (1);
        std::vector<float> L2, R2; render (2.0, L2, R2, [&] (int i) { if (i == 48000) e->setPlay (0); });
        check (rmsDb (L2, 48000 + 30000, 96000 - 128) < -60.0, "DEC 90 %, the transport stops: damped (dBFS 0.6 s later)", rmsDb (L2, 48000 + 30000, 96000 - 128));
    }
    // 5. RING: at 0 the hand damps the last chord's strings; at the top they ring on under the next
    {
        double r[2];
        for (int k = 0; k < 2; ++k)
        {
            setup (3, 4, 0.0f, 0); e->setParam (0, P_DEC, 0.9f); e->setParam (0, P_SRNG, k ? 1.0f : 0.0f); e->setTrig (0, 0, 1); e->setTrig (0, 8, 1); e->setPlay (1);
            int on = 0;
            std::vector<float> L, R; render (1.5, L, R, [&] (int i) { if (i == 469 * 128) { const StrumState& ss = voice().ss; for (int s = 0; s < STRUM_VOICES; ++s) if (ss.s[s].on == 2 && ss.s[s].lvl > 1e-3f) ++on; } });
            r[k] = on;
        }
        check (r[1] > r[0], "RING 100 % keeps more strings sounding after a change of chord than RING 0 (voices)", r[1] - r[0]);
    }
    // 6. no NaN, bounded: every material, the extremes of every STRUM knob, fast ratchets, D/U and RAND
    {
        bool bad = false; double pk = 0;
        Rng rng; rng.seed (77);
        for (int it = 0; it < 60; ++it)
        {
            setup ((int) (rng.uni() * 12), (int) (rng.uni() * HALO_PROGS), rng.uni(), (int) (rng.uni() * 4));
            for (int p : { P_DEC, P_C, P_HINV, P_HARM, P_SHUM, P_SACC, P_SRNG, P_SMAT, P_SPCK, P_SDMP, P_SBDY, P_SMUT, P_SDBL, P_SSTF, P_TUNE, P_HMOD })
                e->setParam (0, p, rng.uni() < 0.3f ? (rng.uni() < 0.5f ? 0.0f : 1.0f) : rng.uni());
            for (int s = 0; s < 16; ++s) if (rng.uni() < 0.5f) { e->setTrig (0, s, 1); if (rng.uni() < 0.3f) e->setRatchet (0, s, 1 + (int) (rng.uni() * 6)); }
            e->setPlay (1);
            std::vector<float> L, R; render (1.0, L, R, nullptr);
            for (float x : L) { if (! std::isfinite (x)) bad = true; pk = std::max (pk, (double) fabsf (x)); }
        }
        check (! bad && pk < 1.0, "60 random settings, ratchets: finite, bounded (peak)", pk);
    }
    // 7. CPU: the whole engine with one STRUM track at its heaviest (12-string, ratchets, a roll)
    {
        setup (7, 13, 0.5f, 3); e->setParam (0, P_SDBL, 1.0f); e->setParam (0, P_DEC, 0.95f); e->setParam (0, P_SRNG, 1.0f); e->setParam (0, P_HWID, 1.0f);
        for (int s = 0; s < 16; ++s) { e->setTrig (0, s, 1); e->setRatchet (0, s, 2); }
        e->setPlay (1);
        std::vector<float> L, R;
        const auto t0 = std::chrono::steady_clock::now(); render (8.0, L, R, nullptr); const auto t1 = std::chrono::steady_clock::now();
        printf ("INFO the whole engine with one STRUM track at its heaviest: %.1f %% of real time\n", std::chrono::duration<double> (t1 - t0).count() / 8.0 * 100.0);
    }
    delete e;
    printf (fails ? "%d FAILED\n" : "STRUM OK\n", fails);
    return fails ? 1 : 0;
}

static int choirtest()
{
    Engine* e = new Engine(); e->init (48000.f);
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-82s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    auto setup = [&] () {
        e->init (48000.f);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setParam (0, P_MACH, machineVal (M_CHOIR)); e->setParam (0, P_VOL, 0.7f);
        e->setParam (0, P_A, 0.6f); e->setParam (0, P_B, 0.0f); e->setParam (0, P_C, 0.1f); e->setParam (0, P_D, 0.0f); e->setParam (0, P_E, 0.0f);
        e->setParam (0, P_CDRF, 0.0f); e->setParam (0, P_CSCP, 0.0f); e->setParam (0, P_CSPC, 0.0f); e->setParam (0, P_DEC, 0.3f);
        e->g[G_MXON] = 0.0f;
    };
    auto voice = [&] () -> Voice& { Track& t = e->tr[0]; return t.voice[t.cur]; };
    auto render = [&] (double secs, std::vector<float>& L, std::vector<float>& R, std::function<void (int)> at) {
        std::vector<float> l (128), r (128);
        for (int i = 0; i < (int) (secs * 48000); i += 128) { if (at) at (i); e->process (l.data(), r.data(), 128, nullptr); L.insert (L.end(), l.begin(), l.end()); R.insert (R.end(), r.begin(), r.end()); }
    };
    auto rmsDb = [] (const std::vector<float>& x, int a, int b) { double s2 = 0; for (int i = a; i < b; ++i) s2 += x[i] * x[i]; return 10 * log10 (s2 / (b - a) + 1e-30); };
    auto pitchOf = [] (const std::vector<float>& x, int a, double f) {
        double best = f, bm = 0;
        for (double fr = f * 0.97; fr <= f * 1.03; fr += f * 0.0003)
        {
            double re = 0, im = 0; const int N = 16384;
            for (int i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * cos (2 * M_PI * i / N); re += x[a + i] * w * cos (2 * M_PI * fr * i / 48000.0); im += x[a + i] * w * sin (2 * M_PI * fr * i / 48000.0); }
            const double m = re * re + im * im; if (m > bm) { bm = m; best = fr; }
        }
        return best;
    };
    // 1. in tune: C4, E4, G4 sung (solo, no vibrato, no drift), each within 5 cents
    {
        setup (); const int notes[3] = { 0, 4, 7 };
        for (int k = 0; k < 3; ++k) { e->setTrig (0, k * 4, 1); e->setLock (0, k * 4, P_NOTE, 0.5f + notes[k] / 48.0f); }
        e->setSpeed (0, 2); e->setPlay (1);
        std::vector<float> L, R; render (3.2, L, R, nullptr);
        double worst = 0;
        for (int k = 0; k < 3; ++k) { const double f = 261.63 * pow (2.0, notes[k] / 12.0), p = pitchOf (L, k * 48000 + 24000, f); worst = std::max (worst, fabs (1200 * log2 (p / f))); }
        check (worst < 5.0, "C4 E4 G4: in tune (worst, cents)", worst);
    }
    // 2. legato: it glides (GLID) from note to note, never falls silent between them; it holds to the next trig
    {
        setup (); e->setParam (0, P_B, 0.6f); e->setTrig (0, 0, 1); e->setTrig (0, 8, 1); e->setLock (0, 8, P_NOTE, 0.5f + 7.0f / 48.0f); e->setPlay (1);
        float mid = 0; double dip = 0;
        std::vector<float> L, R; render (2.0, L, R, [&] (int i) { if (i == 48000 + 3840) mid = voice().cs.pitch; });
        for (int i = 48000; i < 54000; i += 480) dip = std::min (dip, rmsDb (L, i, i + 480) - rmsDb (L, 20000, 23000));
        check (mid > 1.0f && mid < 6.0f, "GLID 60 %: 80 ms after the second trig, between C and G (semitones)", mid);
        check (dip > -4.0, "legato: no dip between the notes (worst 10 ms against the first note, dB)", dip);
        check (fabs (rmsDb (L, 88000, 95000) - rmsDb (L, 62000, 69000)) < 2.0, "it holds to the end of the bar (dB against the start of the note)", rmsDb (L, 88000, 95000) - rmsDb (L, 62000, 69000));
    }
    // 3. a stop, a mute, GATE let it go
    {
        setup (); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> L, R; render (2.0, L, R, [&] (int i) { if (i == 48000) e->setPlay (0); });
        check (rmsDb (L, 48000 + 24000, 96000 - 128) < -60.0, "the transport stops: it fades (dBFS 0.5 s later)", rmsDb (L, 48000 + 24000, 96000 - 128));
        setup (); e->setTrig (0, 0, 1); e->setLock (0, 0, P_GATE, 1.0f / 8.0f); e->setPlay (1);
        std::vector<float> L2, R2; render (1.5, L2, R2, nullptr);
        check (rmsDb (L2, 48000, 70000) < -60.0, "GATE 1 step: it fades (dBFS late in the bar)", rmsDb (L2, 48000, 70000));
    }
    // 4. the vowels: about as loud as each other; EE brighter than OO (2..3.5 kHz against 250..500 Hz)
    {
        double lv[6], tilt[6];
        for (int vw = 0; vw < 6; ++vw)
        {
            setup (); e->setParam (0, P_A, vw / 5.0f); e->setParam (0, P_NOTE, 0.5f - 5.0f / 48.0f); e->setTrig (0, 0, 1); e->setPlay (1);
            std::vector<float> L, R; render (1.0, L, R, nullptr);
            lv[vw] = rmsDb (L, 24000, 48000);
            tilt[vw] = bandDb (L, 24000, 2000, 3500) - bandDb (L, 24000, 250, 500);
        }
        double lo = 1e9, hi = -1e9; for (int vw = 0; vw < 6; ++vw) { lo = std::min (lo, lv[vw]); hi = std::max (hi, lv[vw]); }
        printf ("      vowels (dB): %.1f %.1f %.1f %.1f %.1f %.1f\n", lv[0], lv[1], lv[2], lv[3], lv[4], lv[5]);
        check (hi - lo < 3.0, "6 vowels within 3 dB of each other (spread, dB)", hi - lo);
        check (tilt[5] > tilt[1] + 15.0, "EE brighter than OO (2..3.5 kHz against 250..500 Hz, dB more)", tilt[5] - tilt[1]);
        check (tilt[3] > tilt[0] + 10.0, "AH brighter than MM (dB more)", tilt[3] - tilt[0]);
    }
    // 5. the ensemble and the parts keep the level; PART +3RD on C major sings E over C, F over D (the scale's thirds)
    {
        double base = 0, ens = 0, tri = 0;
        for (int k = 0; k < 3; ++k)
        {
            setup (); if (k == 1) e->setParam (0, P_E, 1.0f); if (k == 2) e->setParam (0, P_CPRT, 3.5f / CHOIR_PARTN);
            e->setTrig (0, 0, 1); e->setPlay (1);
            std::vector<float> L, R; render (1.0, L, R, nullptr);
            (k == 0 ? base : k == 1 ? ens : tri) = rmsDb (L, 24000, 48000);
        }
        check (fabs (ens - base) < 3.0, "SING 4 singers: the level within 3 dB of one (dB)", ens - base);
        check (fabs (tri - base) < 3.0, "PART TRIAD: the level within 3 dB (dB)", tri - base);
        setup (); e->setScale (2741u); e->setParam (0, P_CPRT, 1.5f / CHOIR_PARTN); e->setTrig (0, 0, 1); e->setTrig (0, 8, 1); e->setLock (0, 8, P_NOTE, 0.5f + 2.0f / 48.0f); e->setPlay (1);
        float o1 = 0, o2 = 0;
        std::vector<float> L, R; render (1.6, L, R, [&] (int i) { if (i == 20096) o1 = voice().cs.poff[1]; if (i == 60032) o2 = voice().cs.poff[1]; });
        check (fabsf (o1 - 4.0f) < 0.05f && fabsf (o2 - 3.0f) < 0.05f, "PART +3RD in C major: a major third over C, a minor third over D (semitones)", o2);
    }
    // 6. the ribbon: touched it sings where it is, follows it, lets go when it is let go
    {
        setup ();
        std::vector<float> L, R;
        float p1 = 0, p2 = 0;
        render (1.5, L, R, [&] (int i) {
            if (i == 0) e->ribbon (0, 5.0f, 1, 0.8f);
            if (i == 24064) p1 = voice().cs.pitch;
            if (i == 24064) e->ribbon (0, -3.0f, 1, 0.5f);
            if (i == 36096) p2 = voice().cs.pitch;
            if (i == 48000) e->ribbon (0, -3.0f, 0, 0.5f);
        });
        check (fabsf (p1 - 5.0f) < 0.05f && fabsf (p2 + 3.0f) < 0.05f, "the ribbon: it sings at +5, then follows it to -3 (semitones, last)", p2);
        check (rmsDb (L, 20000, 23000) > -40.0 && rmsDb (L, 48000 + 30000, 72000 - 128) < -60.0, "the ribbon let go: it fades (dBFS 0.6 s later)", rmsDb (L, 48000 + 30000, 72000 - 128));
    }
    // 7. SPACE: off, nothing after the note and mono; on, a stereo tail after it, the voice's level kept; it sleeps after
    {
        double tail[2], side[2], dry[2];
        for (int k = 0; k < 2; ++k)
        {
            setup (); e->setParam (0, P_CSPC, k ? 0.5f : 0.0f); e->setTrig (0, 0, 1); e->setLock (0, 0, P_GATE, 2.0f / 8.0f); e->setPlay (1);
            std::vector<float> L, R; render (3.0, L, R, nullptr);
            dry[k] = rmsDb (L, 12000, 20000);
            tail[k] = rmsDb (L, 40000, 52000);
            double d = 0; for (int i = 20000; i < 52000; ++i) d += (L[i] - R[i]) * (L[i] - R[i]);
            side[k] = 10 * log10 (d / 32000 + 1e-30);
        }
        check (tail[0] < -70.0 && side[0] < -120.0, "SPACE off: nothing after the note, mono (dBFS of the tail)", tail[0]);
        check (tail[1] > -50.0 && side[1] > -60.0, "SPACE 50 %: a tail after the note, in stereo (dBFS of the tail)", tail[1]);
        // (v61: the cleaner voice, its space a little plainer over it)
        check (dry[1] - dry[0] > -1.0 && dry[1] - dry[0] < 3.5, "SPACE 50 %: the voice's level kept, its space added (dB)", dry[1] - dry[0]);
        setup (); e->setParam (0, P_CSPC, 0.5f); e->setParam (0, P_CDCY, 0.2f); e->setTrig (0, 0, 1); e->setLock (0, 0, P_GATE, 1.0f / 8.0f); e->setPlay (1);
        std::vector<float> L, R; render (12.0, L, R, [&] (int i) { if (i == 48000) e->setPlay (0); });
        check (e->tr[0].cvOn == 0, "SPACE sleeps once its tail has died away", e->tr[0].cvOn);
    }
    // 8. finite and bounded: the extremes of every CHOIR knob, PARTS and SING at the top, the ribbon thrown about
    {
        bool bad = false; double pk = 0;
        Rng rng; rng.seed (99);
        for (int it = 0; it < 40; ++it)
        {
            setup ();
            for (int p : { P_A, P_B, P_C, P_D, P_E, P_DEC, P_TUNE, P_CSWL, P_CFRM, P_CEFF, P_CVRT, P_CPRT, P_CSCP, P_CDRF, P_CSPC, P_CSIZ, P_CDCY, P_CDMP, P_CMOD, P_CSHM, P_CPRE })
                e->setParam (0, p, rng.uni() < 0.3f ? (rng.uni() < 0.5f ? 0.0f : 1.0f) : rng.uni());
            for (int s = 0; s < 16; ++s) if (rng.uni() < 0.4f) { e->setTrig (0, s, 1); e->setLock (0, s, P_NOTE, rng.uni()); }
            e->setPlay (1);
            std::vector<float> L, R; render (1.0, L, R, [&] (int i) { if (it % 3 == 0 && i % 4096 == 0) e->ribbon (0, rng.bi() * 30.0f, i < 40000, rng.uni()); });
            for (size_t i = 0; i < L.size(); ++i) { if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) bad = true; pk = std::max (pk, (double) std::max (fabsf (L[i]), fabsf (R[i]))); }
        }
        check (! bad && pk < 1.2, "40 random settings and ribbon moves: finite, bounded (peak)", pk);
    }
    {
        setup (); e->setParam (0, P_E, 1.0f); e->setParam (0, P_CPRT, 3.5f / CHOIR_PARTN); e->setParam (0, P_CSPC, 0.6f); e->setParam (0, P_C, 0.6f);
        for (int s = 0; s < 16; s += 4) e->setTrig (0, s, 1);
        e->setPlay (1);
        std::vector<float> L, R;
        const auto t0 = std::chrono::steady_clock::now(); render (8.0, L, R, nullptr); const auto t1 = std::chrono::steady_clock::now();
        printf ("INFO the whole engine with one CHOIR track at its heaviest (12 singers, SPACE): %.1f %% of real time\n", std::chrono::duration<double> (t1 - t0).count() / 8.0 * 100.0);
    }
    delete e;
    printf (fails ? "%d FAILED\n" : "CHOIR OK\n", fails);
    return fails ? 1 : 0;
}

// v59.1: what a hit reads follows the LFOs (STRUM's chord); the quantiser follows the chord of a HALO or STRUM track
// (G_QSRC), the notes struck with that track's chord hearing the new one; CHOIR's ribbon on SNAP holds to the
// quantiser's notes (a scale's, a chord's) and glides from one to the next in GLID
static int followtest()
{
    Engine* e = new Engine(); e->init (48000.f, 1);
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-82s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    auto run = [&] (double secs, std::function<void (int)> at) {
        std::vector<float> l (128), r (128);
        for (int i = 0; i < (int) (secs * 48000); i += 128) { if (at) at (i); e->process (l.data(), r.data(), 128, nullptr); }
    };
    auto voiceOf = [&] (int t) -> Voice& { Track& k = e->tr[t]; return k.voice[k.cur]; };
    const u32 MAJ = 2741u;
    const float SH = 1.0f / HALO_SHAPES, PR = 1.0f / HALO_PROGS;
    auto base = [&] () {
        e->init (48000.f, 1);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setScale (MAJ); e->g[G_MXON] = 0.0f;
    };
    auto strum = [&] (int t, int shape, int prog) {
        e->setParam (t, P_MACH, machineVal (M_STRUM)); e->setParam (t, P_VOL, 0.6f);
        e->setParam (t, P_A, (shape + 0.5f) * SH); e->setParam (t, P_B, (prog + 0.5f) * PR); e->setParam (t, P_C, 0.35f); e->setParam (t, P_D, 0.2f);
        e->setParam (t, P_HARM, 0.5f / 3.0f);
    };
    auto halo = [&] (int t, int prog) {
        e->setParam (t, P_MACH, machineVal (M_HALO)); e->setParam (t, P_VOL, 0.5f);
        e->setParam (t, P_A, 3.5f * SH); e->setParam (t, P_B, (prog + 0.5f) * PR); e->setParam (t, P_C, 0.0f); e->setParam (t, P_HARM, 0.5f / 3.0f);
    };
    auto choir = [&] (int t, float glide) {
        e->setParam (t, P_MACH, machineVal (M_CHOIR)); e->setParam (t, P_VOL, 0.6f);
        e->setParam (t, P_A, 0.6f); e->setParam (t, P_B, glide); e->setParam (t, P_C, 0.1f); e->setParam (t, P_D, 0.0f); e->setParam (t, P_E, 0.0f);
        e->setParam (t, P_CDRF, 0.0f); e->setParam (t, P_CSCP, 0.0f); e->setParam (t, P_CSPC, 0.0f); e->setParam (t, P_DEC, 0.2f);
    };
    const float QSRC = 1.0f / (NUM_TRACKS + 1);
    // 1. STRUM on TRI, a trig-mode LFO (its decay) on CHRD lifting it to 7TH: each strum plays the 7TH (4 notes; the
    //    hit read the knob alone, 3); without it, the triad
    for (int lfo = 0; lfo < 2; ++lfo)
    {
        base (); strum (0, 3, 0);
        if (lfo) { e->setParam (0, P_LDST, 3.5f / NUM_LFO_DEST); e->setParam (0, P_LDEP, 0.5f + 1.0f / 12.0f); e->setParam (0, P_LWAV, (LFO_SHAPES + LW_EXP + 0.5f) / NUM_LFO_WAVES); e->setParam (0, P_LSPD, 0.5f / NUM_LFO_SPEEDS); }
        for (int s = 0; s < 16; s += 4) e->setTrig (0, s, 1);
        e->setPlay (1);
        int lo = 99, hi = 0;
        run (2.0, [&] (int i) { if (i % 24000 >= 1536 && i % 24000 < 1536 + 128) { const int n = voiceOf (0).hs.n; lo = std::min (lo, n); hi = std::max (hi, n); } });
        if (lfo) check (lo == 4 && hi == 4, "STRUM: an LFO (trig mode) on CHRD reaches each strum: the 7TH (notes, fewest)", lo);
        else check (lo == 3 && hi == 3, "STRUM: no LFO, CHRD on TRI: the triad (notes)", lo);
    }
    // 2. a free LFO (a sine, a bar a cycle) on CHRD: the strums one after the other play different chords
    {
        base (); strum (0, 5, 0);
        e->setParam (0, P_LDST, 3.5f / NUM_LFO_DEST); e->setParam (0, P_LDEP, 0.5f + 0.2f); e->setParam (0, P_LWAV, 0.5f / NUM_LFO_WAVES); e->setParam (0, P_LSPD, 3.5f / NUM_LFO_SPEEDS);
        for (int s = 0; s < 16; s += 2) e->setTrig (0, s, 1);
        e->setPlay (1);
        std::vector<int> seen;
        run (2.0, [&] (int i) { if (i % 12000 >= 1536 && i % 12000 < 1536 + 128) { const HaloState& h = voiceOf (0).hs; int key = h.n; for (int k = 0; k < h.n; ++k) key = key * 31 + (int) h.chord[k]; if (std::find (seen.begin(), seen.end(), key) == seen.end()) seen.push_back (key); } });
        check (seen.size() >= 3, "STRUM: a free LFO on CHRD, a strum every 2 steps: different chords (of 8 strums)", (double) seen.size());
    }
    // 3. the quantiser follows HALO T1 (CANON: C major, then G major): C E G, then G B D; its scale before T1 has played,
    //    with the source off, or once T1 is no longer a HALO
    {
        base (); halo (0, 12); e->setTrig (0, 0, 1); e->setTrig (0, 8, 1);
        e->g[G_QSRC] = 1.5f * QSRC;
        const u32 before = e->quantMask();
        e->setPlay (1);
        u32 m1 = 0, m2 = 0;
        run (1.2, [&] (int i) { if (i == 24064) m1 = e->quantMask(); if (i == 54016) m2 = e->quantMask(); });
        check (before == MAJ && m1 == 145u && m2 == 2180u, "quantiser on HALO T1: the scale until it plays, then C E G, then G B D (mask)", (double) m2);
        e->g[G_QSRC] = 0.0f;
        const u32 off = e->quantMask();
        e->g[G_QSRC] = 1.5f * QSRC; e->setParam (0, P_MACH, machineVal (M_SD_FM));
        check (off == MAJ && e->quantMask() == MAJ, "the source off, or T1 no longer a HALO: the scale again (mask)", (double) e->quantMask());
    }
    // 4. a CHOIR on T2 (quantised, D sung) under it: on the scale D; following T1's C major, C or E; on G major, D again
    {
        float sc = -99, c1 = -99, c2 = -99;
        for (int k = 0; k < 2; ++k)
        {
            base (); halo (0, 12); choir (1, 0.0f);
            e->setTrig (0, 0, 1); e->setTrig (0, 8, 1); e->setTrig (1, 0, 1); e->setTrig (1, 8, 1);
            e->setLock (1, 0, P_NOTE, 0.5f + 2.0f / 48.0f); e->setLock (1, 8, P_NOTE, 0.5f + 2.0f / 48.0f);
            e->g[G_QSRC] = k ? 1.5f * QSRC : 0.0f;
            e->setPlay (1);
            run (1.4, [&] (int i) { if (i == 36096) (k ? c1 : sc) = voiceOf (1).cs.to; if (i == 60032 && k) c2 = voiceOf (1).cs.to; });
        }
        check (fabsf (sc - 2.0f) < 0.01f && (fabsf (c1) < 0.01f || fabsf (c1 - 4.0f) < 0.01f) && fabsf (c2 - 2.0f) < 0.01f,
               "CHOIR T2 sings D: D on the scale, C or E on T1's C major, D on its G major (semitones, last)", c2);
    }
    // 5. struck with the chord it follows: the source (T3, a STRUM) changes chord on step 9, a CHOIR on T1 starts a note
    //    there from silence: it starts on the new chord's note (before: on the old one's, then a glide)
    {
        base (); strum (2, 3, 12); choir (0, 0.0f);
        e->setTrig (2, 0, 1); e->setTrig (2, 8, 1);
        e->setTrig (0, 8, 1); e->setLock (0, 8, P_NOTE, 0.5f + 2.0f / 48.0f); e->setLock (0, 8, P_GATE, 2.0f / 8.0f);
        e->g[G_QSRC] = 3.5f * QSRC;
        e->setPlay (1);
        float from = -99, to = -99;
        run (1.2, [&] (int i) { if (i == 48128) { from = voiceOf (0).cs.from; to = voiceOf (0).cs.to; } });
        check (fabsf (from - 2.0f) < 0.01f && fabsf (to - 2.0f) < 0.01f, "the source first at a shared step: the CHOIR's new note starts on G major's D (semitones)", from);
    }
    // 6. the ribbon on SNAP (C major), GLID 60 %: from C, the finger to 3.6: the voice glides to E (80 ms on, between;
    //    0.6 s on, on E); FREE: it follows the finger; a finger near the middle of two notes keeps the one it holds
    {
        base (); choir (0, 0.6f);
        float mid = -99, end = -99, h1 = -99, h2 = -99;
        run (2.0, [&] (int i) {
            if (i == 0) e->ribbon (0, 0.3f, 2, 0.8f);
            if (i == 24064) e->ribbon (0, 3.6f, 2, 0.8f);
            if (i == 24064 + 3840) mid = voiceOf (0).cs.pitch;
            if (i == 24064 + 28800) end = voiceOf (0).cs.pitch;
            if (i == 60032) e->ribbon (0, 2.95f, 2, 0.8f);            // (E held: 2.95 is a little nearer D)
            if (i == 64000) h1 = voiceOf (0).cs.ribQ;
            if (i == 64000) e->ribbon (0, 2.6f, 2, 0.8f);             // (now clearly nearer D)
            if (i == 68096) h2 = voiceOf (0).cs.ribQ;
        });
        check (mid > 0.5f && mid < 3.5f && fabsf (end - 4.0f) < 0.02f, "ribbon SNAP, GLID 60 %: C to E in a glide (80 ms on, semitones)", mid);
        check (h1 == 4.0f && h2 == 2.0f, "ribbon SNAP: E kept with the finger at 2.95, D once at 2.6 (the note held, last)", h2);
        base (); choir (0, 0.6f);
        float fr = -99;
        run (1.0, [&] (int i) { if (i == 0) e->ribbon (0, 0.3f, 1, 0.8f); if (i == 24064) e->ribbon (0, 3.6f, 1, 0.8f); if (i == 24064 + 4864) fr = voiceOf (0).cs.pitch; });
        check (fabsf (fr - 3.6f) < 0.1f, "ribbon FREE: it follows the finger (100 ms on, semitones)", fr);
    }
    // 7. the ribbon on SNAP following HALO T2's chord: at 2.4, E of C major (the scale alone: D); the chord turns to G
    //    major under the finger: D
    {
        float onScale = -99, onC = -99, onG = -99;
        for (int k = 0; k < 2; ++k)
        {
            base (); choir (0, 0.3f); halo (1, 12); e->setTrig (1, 0, 1); e->setTrig (1, 8, 1);
            e->g[G_QSRC] = k ? 2.5f * QSRC : 0.0f;
            e->setPlay (1);
            run (1.4, [&] (int i) {
                if (i == 6016) e->ribbon (0, 2.4f, 2, 0.8f);
                if (i == 36096) (k ? onC : onScale) = voiceOf (0).cs.ribQ;
                if (i == 60032 && k) onG = voiceOf (0).cs.ribQ;
            });
        }
        check (onScale == 2.0f && onC == 4.0f && onG == 2.0f, "ribbon SNAP on HALO T2: E on its C major (D on the scale), D on its G major (last)", onG);
    }
    // 7b. TRIO's OSC2 / OSC3 on a quantised track: in steps of the scale (C minor; the chord C E G): a third is the
    //     scale's (C: Eb; D: F), a fifth too, the diminished fifth on D a step away (Bb, a minor sixth), the octave an
    //     octave; on the chord, its next notes
    {
        const u32 CMIN = 1453u, CEG = 145u;
        const float a = qInterval (0.0f, 4, 65.41f, CMIN), b2 = qInterval (2.0f, 3, 65.41f, CMIN), c = qInterval (0.0f, 7, 65.41f, CMIN);
        const float d = qInterval (2.0f, 7, 65.41f, CMIN), o = qInterval (2.0f, 12, 65.41f, CMIN), dn = qInterval (7.0f, -5, 65.41f, CMIN);
        const float h1 = qInterval (0.0f, 4, 65.41f, CEG), h2 = qInterval (0.0f, 7, 65.41f, CEG), h3 = qInterval (4.0f, 4, 65.41f, CEG);
        printf ("      TRIO intervals: C+4 %g, D+3 %g, C+7 %g, D+7 %g, D+12 %g, G-5 %g; on C E G: C+4 %g C+7 %g E+4 %g\n", a, b2, c, d, o, dn, h1, h2, h3);
        check (a == 3.0f && b2 == 5.0f && c == 7.0f && d == 10.0f && o == 14.0f && dn == 2.0f && h1 == 4.0f && h2 == 7.0f && h3 == 7.0f,
               "TRIO OSC2 / OSC3 quantised: the scale's thirds and fifths, no tritone over D, the chord's notes (D+7, semitones)", d);
    }
    // 8. the other chord tracks play along: the quantiser on STRUM T2 (CANON: C major, then G major); a HALO on T1 (TRI,
    //    no progression) and a STRUM on T3 build their chords in its notes, the HALO re-voiced as T2 changes; T2 itself
    //    goes on in the scale (C major, then G major: it does not loop on its own chord)
    {
        base (); halo (0, 0); strum (1, 3, 12); strum (2, 3, 0); halo (3, 10);
        e->setTrig (0, 0, 1); e->setTrig (1, 0, 1); e->setTrig (1, 8, 1); e->setTrig (2, 0, 1); e->setTrig (2, 8, 1);
        e->setTrig (3, 0, 1); e->setTrig (3, 4, 1); e->setTrig (3, 8, 1); e->setTrig (3, 12, 1);
        e->g[G_QSRC] = 2.5f * QSRC;
        e->setPlay (1);
        auto pcsOf = [&] (int t) { const HaloState& h = voiceOf (t).hs; u32 m = 0; for (int k = 0; k < h.n; ++k) m |= 1u << ((((int) floorf_ (h.chord[k] + 0.5f)) % 12 + 12) % 12); return m; };
        u32 h1 = 0, h2 = 0, s1 = 0, s2 = 0, src1 = 0, src2 = 0, sl[4] = {}; int r1 = -1, r2 = -1;
        run (1.2, [&] (int i) {
            if (i == 24064) { h1 = pcsOf (0); s1 = pcsOf (2); src1 = e->tr[1].chordPc; r1 = voiceOf (0).hs.key[0]; sl[0] = pcsOf (3); }
            if (i == 36096) sl[1] = pcsOf (3);
            if (i == 54016) { h2 = pcsOf (0); s2 = pcsOf (2); src2 = e->tr[1].chordPc; r2 = voiceOf (0).hs.key[0]; sl[2] = pcsOf (3); }
            if (i == 57088) sl[3] = pcsOf (3);
        });
        check (h1 == 145u && h2 == 2180u, "HALO T1 under the quantiser on STRUM T2: C E G, then (held) G B D (mask)", (double) h2);
        check (s1 == 145u && s2 == 2180u, "STRUM T3 under it: C E G, then G B D at its next strum (mask)", (double) s2);
        check (src1 == 145u && src2 == 2180u, "STRUM T2, followed: its own chords in the scale, C major then G major (mask)", (double) src2);
        check (r1 == 0 && r2 == -5, "HALO T1 plays along on the followed roots: C3, then G2 (its own register; semitones from C3)", r2);
        check (sl[0] == 145u && sl[1] == 145u && sl[2] == 2180u && sl[3] == 2180u, "a HALO on SLIP playing along: its progression set aside, on T2's chords (mask)", (double) sl[3]);
    }
    delete e;
    printf (fails ? "%d FAILED\n" : "FOLLOW OK\n", fails);
    return fails ? 1 : 0;
}

static int stress()
{
    loadBank();                                    // (CHIMERA plays the bank's first kick)
    REF = new Engine(); REF->init (48000.f);
    Engine* e = new Engine();
    e->init (48000.f);
    int fails = 0;
    printf ("%-7s %8s %8s %8s %6s\n", "machine", "peak", "rms", "tail(s)", "ok");
    for (int m = 0; m < NUM_MACHINES; ++m)
    {
        softReset (e, 48000.f);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setGlobal (G_RSIZ, 0.0f); e->setGlobal (G_DFB, 0.0f);
        e->setParam (0, P_MACH, machineVal (m));
        e->setParam (0, P_VOL, 0.7f);
        if (m == M_RAM) ramTestTake (*e, 0);       // RAM plays its buffer: a decaying tone for the test
        if (m == M_CHIMERA) e->setParam (0, P_E, 0.0f);   // (forward: reversed, at DEC 0.4 it would only play the tail of the 808)
        if (m == M_WEST || m == M_HOWL || m == M_MANTRA) { float sd[NUM_SIDES]; const int k = machineSideDefaults (m, sd); for (int q = 0; q < k; ++q) e->setParam (0, sideParam (q), sd[q]); }   // (its sequence, its pulser; HOWL's pick and level, MANTRA's whole panel: as a fresh track has them)
        e->audition (0, 1.0f);
        Stats s = renderStats (*e, 48000 * 3);
        // (an FX machine has no sound of its own: it takes another track's, silent here, and only has to stay finite)
        const bool fxm = fxMachine (m);
        bool ok = ! s.bad && (fxm ? s.peak <= 1.0f : s.peak > 0.02f && s.peak <= 1.0f);
        if (! ok) fails++;
        printf ("%-7s %8.3f %8.4f %8.2f %6s\n", MNAMES[m], s.peak, s.rms, s.lastLoud, ok ? (fxm ? "ok fx" : "ok") : "FAIL");
    }

    // extreme parameter sweep: every machine x every param at 0 and 1, plus random combos
    Rng rng; rng.seed (42);
    int combos = 0, bad = 0; float worst = 0;
    for (int m = 0; m < NUM_MACHINES; ++m)
        for (int p = 1; p < NUM_PARAMS; ++p)
            for (int ex = 0; ex < 2; ++ex)
            {
                softReset (e, 48000.f);
                e->setParam (0, P_MACH, machineVal (m));
                e->setParam (0, p, (float) ex);
                if (p == P_VOL) e->setParam (0, P_VOL, 1.0f);
                e->audition (0, 1.0f);
                Stats s = renderStats (*e, 24000);
                combos++;
                worst = std::max (worst, s.peak);
                if (s.bad) { bad++; printf ("NaN: machine %s param %d = %d\n", MNAMES[m], p, ex); }
            }
    for (int it = 0; it < 400; ++it)
    {
        softReset (e, it < 200 ? 44100.f : 48000.f);
        int m = (int) (rng.uni() * NUM_MACHINES);
        e->setParam (0, P_MACH, machineVal (m));
        for (int p = 1; p < NUM_PARAMS; ++p) e->setParam (0, p, rng.uni());
        e->setParam (0, P_PROB, 1.0f);
        e->setTrig (0, 0, 1); e->setTrig (0, 5, 1);
        e->setRec (0, 2, 1);                        // RAM: a take on step 3 (from a random source)
        e->setLock (0, 5, P_MACH, rng.uni());
        // a lock-only step with random locks over the playing voice (its machine stays)
        e->setTrigless (0, 3, 1);
        for (int p = 1; p < NUM_PARAMS; ++p) if (rng.uni() < 0.3f) e->setLock (0, 3, p, rng.uni());
        e->setGlobal (G_DFB, rng.uni()); e->setGlobal (G_RSIZ, rng.uni()); e->setGlobal (G_MDRV, rng.uni());
        for (int gi : { G_RALG, G_RTIM, G_RPRE, G_RCH1, G_RCH2, G_RDMP, G_DACM, G_DACA, G_UMSK, G_UKCK, G_CALM, G_CSHP, G_CSEL, G_CSPD, G_CFOC, G_CDLT }) e->setGlobal (gi, rng.uni());
        e->setPlay (1);
        Stats s = renderStats (*e, 44100);
        combos++;
        if (s.bad) { bad++; printf ("NaN in random combo %d (machine %s)\n", it, MNAMES[m]); }
    }
    printf ("param sweep: %d combos, %d with NaN/inf, worst peak %.3f\n", combos, bad, worst);

    // CPU benchmark: all 8 tracks with heavy retrigs at 48k
    softReset (e, 48000.f);
    const int kit[8] = { M_FM_X, M_ALLOY, M_HH_MT, M_SY_RAW, M_IMPLS, M_NOISE, M_CP_NZ, M_BYTE };
    for (int t = 0; t < 8; ++t)
    {
        e->setParam (t, P_MACH, machineVal (kit[t]));
        e->setParam (t, P_DEC, 0.8f);
        e->setParam (t, P_FTYP, 0.4f);
        e->setParam (t, P_DLY, 0.3f); e->setParam (t, P_REV, 0.3f);
        e->setParam (t, P_D, 1.0f);
        for (int s = 0; s < 16; ++s) e->setTrig (t, s, 1);
        e->setParam (t, P_RTRG, 0.7f); e->setParam (t, P_RCNT, 1.0f);
    }
    for (int t = 0; t < 8; ++t)
    {
        e->setParam (t, P_IR, (float) (24 - t) / 25.0f + 0.01f);   // the eight longest IRs
        e->setParam (t, P_IMIX, 0.6f);
        e->setParam (t, P_REV, 0.4f);
    }
    e->setGlobal (G_RALG, (REV_SPECTRAL + 0.5f) / NUM_REV_ALGOS); e->setGlobal (G_RLVL, 0.8f);
    e->setPlay (1);
    auto t0 = std::chrono::high_resolution_clock::now();
    Stats s = renderStats (*e, 48000 * 10);
    auto t1 = std::chrono::high_resolution_clock::now();
    double secs = std::chrono::duration<double> (t1 - t0).count();
    printf ("heavy load (8 IR + spectral): 10 s audio rendered in %.3f s (%.1fx realtime), peak %.3f, bad=%d\n",
            secs, 10.0 / secs, s.peak, (int) s.bad);
    delete e;
    return (fails || bad) ? 1 : 0;
}

// one op of the worklet format; "at <seconds>" schedules the ops that follow (automation, looper moves)
struct TimedOp { double at; std::string o; double a, b, c, d; };
static void applyOp (Engine* e, const TimedOp& x)
{
    const std::string& o = x.o;
    if (o == "p")          e->setParam ((int) x.a, (int) x.b, (float) x.c);
    else if (o == "trig")  e->setTrig ((int) x.a, (int) x.b, (int) x.c);
    else if (o == "tlk")   e->setTrigless ((int) x.a, (int) x.b, (int) x.c);
    else if (o == "lock")  e->setLock ((int) x.a, (int) x.b, (int) x.c, (float) x.d);
    else if (o == "rat")   e->setRatchet ((int) x.a, (int) x.b, (int) x.c);
    else if (o == "clrl")  e->clearLocks ((int) x.a, (int) x.b);
    else if (o == "len")   e->setLen ((int) x.a, (int) x.b);
    else if (o == "spd")   e->setSpeed ((int) x.a, (int) x.b);
    else if (o == "mute")  e->setMute ((int) x.a, (int) x.b);
    else if (o == "g")     e->setGlobal ((int) x.a, (float) x.b);
    else if (o == "reset") e->resetPattern();
    else if (o == "scale") e->setScale ((unsigned) x.a);
    else if (o == "qtrk")  e->setQuant ((int) x.a, (int) x.b);
    // patterns (v58): the one playing, all emptied, the one edited, one emptied, the chain, CHG (as the worklet does)
    else if (o == "pcur")  e->patCur ((int) x.a);
    else if (o == "pall")  e->patAll();
    else if (o == "epat")  e->patEdit ((int) x.a);
    else if (o == "pclr")  e->patClear ((int) x.a);
    else if (o == "pch")   e->patChain ((int) x.a, (int) x.b);
    else if (o == "pchn")  e->patChainLen ((int) x.a);
    else if (o == "pchg")  e->patChg ((int) x.a);
}
static int opsRender (const char* opsPath, const char* wavPath, float seconds, int solo)
{
    loadBank();
    Engine* e = new Engine();
    int sr = 48000;
    e->init ((float) sr);
    FILE* f = fopen (opsPath, "r");
    if (! f) { printf ("cannot open %s\n", opsPath); return 1; }
    char op[32]; int nops = 0;
    std::vector<TimedOp> ops;
    double at = 0.0;
    while (fscanf (f, "%31s", op) == 1)
    {
        TimedOp x { at, op, 0, 0, 0, 0 };
        const std::string& o = x.o;
        int na = 0;
        if (o == "at") { fscanf (f, "%lf", &at); continue; }
        if (o == "p" || o == "trig" || o == "rat" || o == "tlk") na = 3;
        else if (o == "lock") na = 4;
        else if (o == "clrl" || o == "len" || o == "spd" || o == "mute" || o == "g" || o == "qtrk") na = 2;
        else if (o == "scale" || o == "pcur" || o == "epat" || o == "pclr" || o == "pchn" || o == "pchg") na = 1;
        else if (o == "pch") na = 2;
        else if (o == "reset" || o == "pall") na = 0;
        else { printf ("unknown op %s\n", op); return 1; }
        double v[4] = { 0, 0, 0, 0 };
        for (int k = 0; k < na; ++k) fscanf (f, "%lf", &v[k]);
        x.a = v[0]; x.b = v[1]; x.c = v[2]; x.d = v[3];
        ops.push_back (x);
        nops++;
    }
    fclose (f);
    size_t oi = 0;
    while (oi < ops.size() && ops[oi].at <= 0.0) applyOp (e, ops[oi++]);
    if (solo >= 0) for (int t = 0; t < NUM_TRACKS; ++t) e->setMute (t, t != solo);
    // (v74: $CUBE_DIR, a folder of CUBE banks 1.wav .. 8.wav, 16-bit mono WAV files of 64 waves of 256 samples, as the
    // page's LOAD panel reads them: into the USER cube's places 1 .. 8, made at once)
    if (const char* cd = getenv ("CUBE_DIR"))
    {
        for (int b = 0; b < CUBE_BANKS; ++b)
        {
            char fn[1024]; snprintf (fn, sizeof fn, "%s/%d.wav", cd, b + 1);
            FILE* wf = fopen (fn, "rb"); if (! wf) continue;
            std::vector<unsigned char> buf; unsigned char tmp[65536]; size_t got;
            while ((got = fread (tmp, 1, sizeof tmp, wf)) > 0) buf.insert (buf.end(), tmp, tmp + got);
            fclose (wf);
            size_t q = 12; int n = 0; const short* d = nullptr;
            while (q + 8 <= buf.size())
            {
                const unsigned len = (unsigned) buf[q + 4] | ((unsigned) buf[q + 5] << 8) | ((unsigned) buf[q + 6] << 16) | ((unsigned) buf[q + 7] << 24);
                if (! memcmp (&buf[q], "data", 4)) { d = reinterpret_cast<const short*> (&buf[q + 8]); n = (int) (std::min ((size_t) len, buf.size() - q - 8) / 2); break; }
                q += 8 + len + (len & 1);
            }
            if (d && n > 0) { std::vector<short> v (d, d + std::min (n, (int) CUBE_BANK_N)); e->cubeSetBank (b, v.data(), (int) v.size()); }
        }
        while (e->cube.busy >= 0) e->cubeStep (64);
    }
    e->setPlay (1);
    int total = (int) (seconds * sr);
    std::vector<float> L (total), R (total);
    double sum = 0; float peak = 0; bool bad = false;
    const int bs = getenv ("BS") ? atoi (getenv ("BS")) : 128;
    for (int i = 0; i < total; i += bs)
    {
        while (oi < ops.size() && ops[oi].at * sr <= (double) i) applyOp (e, ops[oi++]);
        int n = std::min (bs, total - i);
        e->process (L.data() + i, R.data() + i, n, nullptr);
    }
    for (int i = 0; i < total; ++i)
    {
        if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) bad = true;
        peak = std::max (peak, std::max (fabsf (L[i]), fabsf (R[i])));
        sum += L[i] * L[i] + R[i] * R[i];
    }
    printf ("%d ops, %.1f s, peak %.3f (%.1f dBFS), rms %.1f dBFS, bad=%d\n", nops, seconds, peak,
            20 * log10 (peak + 1e-9), 10 * log10 (sum / (2.0 * total) + 1e-12), (int) bad);
    writeWav (wavPath, L, R, sr);
    delete e;
    return bad ? 1 : 0;
}

// organic bank, µLOOP sources, FRACT particles, ratchets
static int orgtest()
{
    REF = new Engine(); REF->init (48000.f);
    Engine* e = new Engine();
    auto t0 = std::chrono::high_resolution_clock::now();
    e->init (48000.f);
    printf ("init %.1f ms (bank %d floats)\n", std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - t0).count(), e->org.used);
    int fails = 0;
    float minPk = 9, maxPk = 0;
    for (int src = 0; src < NUM_ULOOP_SRC; ++src)
        for (int full = 0; full < 2; ++full)
        {
            softReset (e, 48000.f);
            e->setParam (0, P_MACH, machineVal (M_ULOOP));
            e->setParam (0, P_A, (src + 0.5f) / NUM_ULOOP_SRC);
            e->setParam (0, P_B, full ? 1.0f : 0.45f);
            e->setParam (0, P_E, full ? 0.0f : 0.3f);
            e->setParam (0, P_DEC, full ? 1.0f : 0.5f);
            e->audition (0, 1.0f);
            Stats s = renderStats (*e, 48000);
            minPk = std::min (minPk, s.peak); maxPk = std::max (maxPk, s.peak);
            if (s.bad || s.peak < 0.0005f) { fails++; printf ("uLOOP src %d full %d: peak %.4f bad %d\n", src, full, s.peak, (int) s.bad); }
        }
    printf ("uLOOP: %d sources x 2, peak %.3f..%.3f\n", NUM_ULOOP_SRC, minPk, maxPk);
    minPk = 9; maxPk = 0;
    for (int src = 0; src < NUM_ORG; ++src)
        for (int mode = 0; mode < 3; ++mode)
        {
            softReset (e, mode == 2 ? 44100.f : 48000.f);
            e->setParam (0, P_MACH, machineVal (M_FRACT));
            e->setParam (0, P_A, (src + 0.5f) / NUM_ORG);
            e->setParam (0, P_B, mode == 0 ? 0.0f : 1.0f);
            e->setParam (0, P_C, mode == 1 ? 0.0f : 0.5f);
            e->setParam (0, P_D, mode == 2 ? 1.0f : 0.0f);
            e->setParam (0, P_E, mode == 2 ? 1.0f : 0.2f);
            e->setParam (0, P_F, mode == 1 ? 0.2f : 1.0f);
            e->audition (0, 1.0f);
            Stats s = renderStats (*e, 48000 * 2);
            minPk = std::min (minPk, s.peak); maxPk = std::max (maxPk, s.peak);
            if (s.bad || s.peak < 0.0005f) { fails++; printf ("FRACT src %d mode %d: peak %.4f bad %d\n", src, mode, s.peak, (int) s.bad); }
        }
    printf ("FRACT: %d sources x 3 modes, peak %.3f..%.3f\n", NUM_ORG, minPk, maxPk);

    // ratchets: one bar, step 0 split in 4, step 8 split in 3 -> 7 hits on track 0
    softReset (e, 48000.f);
    e->setTrig (0, 0, 1); e->setRatchet (0, 0, 4);
    e->setTrig (0, 8, 1); e->setRatchet (0, 8, 3);
    e->setParam (0, P_VOL, 0.0f);
    const int h0 = e->getHits (0);
    e->setPlay (1);
    renderStats (*e, 96000 - 2000);   // one bar at 120 BPM is 2 s, stop before the next downbeat
    const int hits = e->getHits (0) - h0;
    printf ("ratchets: %d hits in one bar (expected 7)\n", hits);
    if (hits != 7) fails++;

    // CPU: 8 FRACT tracks with full crowds and dense tails on every step
    softReset (e, 48000.f);
    for (int t = 0; t < 8; ++t)
    {
        e->setParam (t, P_MACH, machineVal (M_FRACT));
        e->setParam (t, P_A, (t * 4 + 0.5f) / NUM_ORG);
        e->setParam (t, P_B, 1.0f); e->setParam (t, P_C, 0.6f); e->setParam (t, P_D, 1.0f); e->setParam (t, P_DEC, 1.0f);
        e->setParam (t, P_VOL, 0.3f);
        for (int st = 0; st < 16; ++st) e->setTrig (t, st, 1);
    }
    e->setPlay (1);
    auto c0 = std::chrono::high_resolution_clock::now();
    Stats s = renderStats (*e, 48000 * 10);
    double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - c0).count();
    printf ("8 x FRACT max: %.1fx realtime, peak %.3f bad %d\n", 10.0 / secs, s.peak, (int) s.bad);
    if (s.bad) fails++;
    delete e;
    printf (fails ? "FAILED (%d)\n" : "all ok\n", fails);
    return fails ? 1 : 0;
}

// master section: random settings of EQ / compressor / drive on a busy pattern, NaN and level checks
static int mastertest()
{
    REF = new Engine(); REF->init (48000.f);
    Engine* e = new Engine(); e->init (48000.f);
    Rng rng; rng.seed (7);
    int bad = 0; float worst = 0;
    for (int it = 0; it < 240; ++it)
    {
        softReset (e, it % 3 == 0 ? 44100.f : 48000.f);
        for (int t = 0; t < 8; ++t) { for (int st = 0; st < 16; st += 1 + t % 3) e->setTrig (t, st, 1); e->setParam (t, P_VOL, 0.9f); }
        for (int gi = G_EQLF; gi < NUM_GLOBALS; ++gi) e->setGlobal (gi, rng.uni());
        e->setGlobal (G_DMOD, ((float) (it % NUM_DRIVE_MODELS) + 0.5f) / (float) NUM_DRIVE_MODELS);
        e->setGlobal (G_CRAT, ((float) ((it / 8) % NUM_COMP_RATIOS) + 0.5f) / (float) NUM_COMP_RATIOS);
        e->setGlobal (G_MDRV, rng.uni()); e->setGlobal (G_MVOL, 0.5f + 0.5f * rng.uni());
        e->setPlay (1);
        Stats s = renderStats (*e, 24000);
        worst = std::max (worst, s.peak);
        if (s.bad || s.peak > 1.0f) { bad++; printf ("bad master combo %d (drive %d ratio %d) peak %.3f\n", it, it % NUM_DRIVE_MODELS, (it / 8) % NUM_COMP_RATIOS, s.peak); }
    }
    printf ("master sweep: 240 random settings, %d bad, worst peak %.3f\n", bad, worst);
    delete e;
    return bad ? 1 : 0;
}

// BREAK: loops to slice, made here (a kick, a snare, hats on every 16th, the off 16ths a little late, as played), the
// hits' places kept to check what the slots read; levelled as the page levels a loop it loads (-16 dBFS RMS)
struct TestLoop { std::vector<short> x; std::vector<int> at, cls; int bars; float bpm; };
static TestLoop synthLoop (float bpm, int bars, int variant)
{
    TestLoop T; T.bars = bars; T.bpm = bpm;
    const int R = BRK_RATE, n = (int) lround (bars * 240.0 / bpm * R), L = 16 * bars;
    std::vector<float> x ((size_t) n, 0.0f);
    Rng rng; rng.seed (1234u + (u32) variant * 77u);
    static const char* PAT[3] = { "K..H.S..K.K.S..H", "K..K.S.HK.K..S.S", "K.H.S..KH.K.S.H." };
    const char* pat = PAT[variant % 3];
    const double st = (double) n / L;
    for (int k = 0; k < L; ++k)
    {
        const char c = pat[k % 16];
        const double late = (k % 2 ? 0.006 : 0.0) + 0.002 * rng.bi();
        const int t0 = std::max (0, std::min (n - 1, (int) lround (k * st + (k ? late * R : 0.0))));
        T.at.push_back (t0); T.cls.push_back (c == 'K' ? 1 : (c == 'S' ? 2 : 0));
        double ph = 0.0, lastN = 0.0;
        const int len = std::min (n - t0, (int) (0.4 * R));
        for (int i = 0; i < len; ++i)
        {
            const double t = (double) i / R;
            double y = 0.0;
            const double nz = rng.bi(), hpN = nz - lastN; lastN = nz;
            y += (c == 'H' ? 0.22 : 0.12) * hpN * exp (-t / 0.03);                                  // the hat
            if (c == 'K') { ph += 2 * M_PI * (48.0 + 130.0 * exp (-t / 0.03)) / R; y += 0.9 * sin (ph) * exp (-t / 0.22); }
            if (c == 'S') y += 0.45 * nz * exp (-t / 0.11) + 0.35 * sin (2 * M_PI * 190.0 * t) * exp (-t / 0.07);
            const double fi = std::min (1.0, i / 8.0);
            x[(size_t) (t0 + i)] += (float) (y * fi);
        }
    }
    double s2 = 0, pk = 0; for (float v : x) { s2 += v * v; pk = std::max (pk, (double) fabsf (v)); }
    double g = pow (10.0, -16.0 / 20.0) / (sqrt (s2 / n) + 1e-12);
    g = std::min (g, pow (10.0, -0.5 / 20.0) / (pk + 1e-12));
    const int fd = (int) (0.002 * R); for (int i = 0; i < fd; ++i) x[(size_t) (n - 1 - i)] *= (float) i / fd;
    T.x.resize ((size_t) n);
    for (int i = 0; i < n; ++i) T.x[(size_t) i] = (short) std::max (-32767.0, std::min (32767.0, std::round (x[(size_t) i] * g * 32767.0)));
    return T;
}
static void loadSlot (Engine* e, int s, const TestLoop& T, bool now = true)
{
    memcpy (e->brk.mem (s), T.x.data(), T.x.size() * sizeof (short));
    e->breakSet (s, (int) T.x.size(), T.bars);
    if (now) e->breakWork (1 << 30);
}

// BREAK: what a slot reads, the slices following on seamlessly, the tempo (SYNC keeps the pitch, VARI follows), the
// chops, SWAP, the slots coming and going
static int breaktest()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-78s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    Engine* e = new Engine();
    e->init (48000.f);
    const TestLoop A0 = synthLoop (120.0f, 2, 0), A1 = synthLoop (96.0f, 1, 1), A2 = synthLoop (137.0f, 4, 2);
    // 1. what the slot reads: the tempo, a slice on each hit (a breath before it), what each starts with
    {
        loadSlot (e, 0, A0); loadSlot (e, 1, A1); loadSlot (e, 2, A2);
        check (e->brk.ready (0) && e->brk.ready (1) && e->brk.ready (2), "three slots read", 3);
        check (fabsf (e->brk.b[2].bpm - 137.0f) < 0.05f && e->brk.b[2].L16 == 64, "the tempo (137 BPM, 4 bars)", e->brk.b[2].bpm);
        int near = 0, tot = 0, clsOk = 0, loud = 0, loudOk = 0; double worst = 0;
        for (const auto* T : { &A0, &A1, &A2 })
        {
            const int s = T == &A0 ? 0 : (T == &A1 ? 1 : 2);
            const BrkInfo& B = e->brk.b[s];
            for (int k = 0; k < B.L16; ++k)
            {
                const int d = B.onset[k] - T->at[(size_t) k];
                ++tot; if (d >= -130 && d <= 30) ++near;
                if (T->cls[(size_t) k]) { ++loud; if (d >= -130 && d <= 30) ++loudOk; }
                if (getenv ("BRKDBG") && (d < -130 || d > 30)) printf ("  slot %d slice %d: %d (hit at %d, grid %.0f) cls %d/%d\n", s, k, d, T->at[(size_t) k], k * (double) B.len / B.L16, B.cls[k], T->cls[(size_t) k]);
                worst = std::max (worst, (double) abs (d));
                if (B.cls[k] == T->cls[(size_t) k]) ++clsOk;
            }
        }
        check (loudOk == loud, "every kick and snare: its slice on it (3.5 ms before .. 1 ms after)", loudOk);
        check (near >= tot * 9 / 10, "the hats too, but a few quiet ones in a snare's tail (share on their hit)", (double) near / tot);
        check (worst < 700, "none further than 22 ms (worst, samples)", worst);
        check (clsOk >= tot * 9 / 10, "what each slice starts with: kick, snare, hat (share right)", (double) clsOk / tot);
    }
    const float LN = 1.0f / BRK_LENS;
    auto setup = [&] (int slot, float bpm) {
        e->init (48000.f); loadSlot (e, 0, A0); loadSlot (e, 1, A1); loadSlot (e, 2, A2);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setParam (0, P_MACH, machineVal (M_BREAK)); e->setParam (0, P_VOL, 0.8f);
        e->setParam (0, P_A, (slot + 0.5f) / (float) BRK_SLOTS); e->setParam (0, P_B, 0.25f);
        e->setParam (0, P_C, 0.0f); e->setParam (0, P_D, (7 + 0.5f) * LN); e->setParam (0, P_E, 0.0f); e->setParam (0, P_DEC, 1.0f);
        e->setGlobal (G_BPM, bpm); e->g[G_MXON] = 0.0f;
    };
    auto render = [&] (double secs, std::vector<float>& L) {
        std::vector<float> l (128), r (128);
        for (int i = 0; i < (int) (secs * 48000); i += 128) { e->process (l.data(), r.data(), 128, nullptr); L.insert (L.end(), l.begin(), l.end()); }
    };
    auto rmsDb = [] (const std::vector<float>& x, int a, int b) { double s2 = 0; for (int i = a; i < b; ++i) s2 += x[i] * x[i]; return 10 * log10 (s2 / (b - a) + 1e-30); };
    auto diffDb = [] (const std::vector<float>& A, const std::vector<float>& B) {
        double d2 = 0, s2 = 0; for (size_t i = 0; i < std::min (A.size(), B.size()); ++i) { d2 += (A[i] - B[i]) * (A[i] - B[i]); s2 += A[i] * A[i]; }
        return 10 * log10 (d2 / (s2 + 1e-30) + 1e-30); };
    const float bpm0 = 120.0f;
    // 2. a trig on every step (STEP) plays the loop as one trig on the first step does (FREE): no seam between slices;
    //    at the loop's own tempo, SYNC gives back the loop itself
    {
        setup (0, bpm0); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> A; render (6.0, A);
        setup (0, bpm0); for (int s = 0; s < 16; ++s) e->setTrig (0, s, 1); e->setPlay (1);
        std::vector<float> B; render (6.0, B);
        check (diffDb (A, B) < -40.0, "16 trigs a bar against one (FREE) (difference, dB)", diffDb (A, B));
        check (rmsDb (A, 0, (int) A.size()) > -30.0, "it plays (level, dB)", rmsDb (A, 0, (int) A.size()));
        const double ratio = (double) BRK_RATE / 48000.0;
        const std::vector<short>& X = A0.x;
        double corr = -1.0; int best = 0;
        for (int lag = 0; lag <= 600; ++lag)
        {
            double xy = 0, xx = 0, yy = 0;
            const int n = std::min ((int) A.size() - lag, (int) (X.size() / ratio) - 64);
            for (int i = 2000; i < n; i += (lag % 1 ? 1 : 1))
            {
                const double p = i * ratio; const int i0 = (int) p; const double fr = p - i0;
                const double y = X[(size_t) i0] + (X[(size_t) i0 + 1] - X[(size_t) i0]) * fr;
                xy += A[i + lag] * y; xx += A[i + lag] * A[i + lag]; yy += y * y;
            }
            const double c = xy / sqrt (xx * yy + 1e-30);
            if (c > corr) { corr = c; best = lag; }
        }
        printf ("     (best lag %d samples)\n", best);
        if (getenv ("BRKDBG"))
            for (int q = 0; q < 48; ++q)
            {
                double dd = 0, ss = 0;
                for (int i = q * 6000; i < (q + 1) * 6000 && i + best < (int) A.size(); ++i)
                {
                    const double p = i * ratio; const int i0 = (int) p % (int) X.size(), i1 = (i0 + 1) % (int) X.size(); const double fr = p - (int) p;
                    const double y = (X[(size_t) i0] + (X[(size_t) i1] - X[(size_t) i0]) * fr) / 32768.0 * 0.8;
                    dd += (A[i + best] - y) * (A[i + best] - y); ss += y * y;
                }
                printf ("  16th %2d: err %.1f dB\n", q, 10 * log10 (dd / (ss + 1e-30) + 1e-30));
            }
        check (corr > 0.98, "SYNC at the loop's tempo: the loop as it is, through the track (correlation)", corr);
        std::vector<float> R (A); writeWav ("build/break_loop.wav", A, R, 48000);
    }
    // 3. the tempo: SYNC keeps the pitch at another tempo, VARI follows the tempo ratio (zero crossings of the body of the
    //    first slice, the kick, 30 .. 100 ms, below 150 Hz)
    {
        auto zc = [&] (float bpm, int vari) {
            setup (0, bpm); e->setParam (0, P_B, vari == 1 ? 0.75f : 0.25f);
            e->setTrig (0, 0, 1); e->setPlay (1);
            std::vector<float> A; render (0.2, A);
            double l1 = 0, l2 = 0; const double c = 1.0 - exp (-2 * M_PI * 150.0 / 48000.0);
            for (float& x : A) { l1 += (x - l1) * c; l2 += (l1 - l2) * c; x = (float) l2; }
            int z = 0; for (int i = 1440; i < (vari > 1 ? 4320 : 9600); ++i) if ((A[i - 1] < 0.0f) != (A[i] < 0.0f)) ++z;
            return z;
        };
        // (SYNC cuts the kick at the next 16th, 100 ms at x1.25: 30 .. 90 ms there; VARI: 30 .. 200 ms)
        const int s0 = zc (bpm0, 0), s0s = zc (bpm0, 2) , s1 = zc (bpm0 * 1.25f, 2), v1 = zc (bpm0 * 1.25f, 1);
        check (abs (s1 - s0s) <= 1, "SYNC: the pitch does not move with the tempo (zero crossings, x1.25 - x1)", s1 - s0s);
        const double ratio = (double) v1 / (double) std::max (s0, 1);
        check (ratio > 1.1 && ratio < 1.4, "VARI: it rises with the tempo, about x1.25 (ratio of zero crossings)", ratio);
        setup (0, bpm0 * 1.25f); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> F; render (8.0, F);
        const double pass = e->brk.b[0].L16 * 15.0 / (bpm0 * 1.25);
        check (rmsDb (F, (int) (0.1 * pass * 48000), (int) (0.95 * pass * 48000)) > -30.0, "SYNC x1.25: one pass in its shorter time (level, dB)", pass);
    }
    // 4. LEN: a 16th per trig on the beats leaves the off-beats silent
    {
        setup (0, bpm0); e->setParam (0, P_D, (1 + 0.5f) * LN);
        for (int s = 0; s < 16; s += 4) e->setTrig (0, s, 1); e->setPlay (1);
        std::vector<float> A; render (1.8, A);
        const double T = 48000.0 * 60.0 / bpm0 / 4.0;
        const double on = rmsDb (A, (int) (0.1 * T), (int) (0.9 * T)), off = rmsDb (A, (int) (2.2 * T), (int) (3.8 * T));
        check (on - off > 40.0, "LEN 1/16 on the beats: the gap after the slice (on - off, dB)", on - off);
    }
    // 5. the chops: SWAP, RAND, REV, STUT, PJMP and SKIP happen; SEED repeats the same chops on every pass, free draws
    //    do not; no click sharper than the loop's own hits
    // one pass of every loop together (2, 1 and 4 bars: SWAP plays the others where they are in their own bars)
    const int P4 = (int) (48000.0 * 60.0 / bpm0 * 4.0 * 4);
    {
        auto chopRun = [&] (float swp, float rnd, float rev, float stu, float pj, float skp, int seed, double secs, std::vector<float>& A, int& nflags) {
            setup (0, bpm0);
            e->setParam (0, P_BSWP, swp);
            e->setParam (0, P_BRND, rnd); e->setParam (0, P_BREV, rev); e->setParam (0, P_BSTU, stu); e->setParam (0, P_BPJM, pj); e->setParam (0, P_BSKP, skp);
            e->setParam (0, P_BSED, (seed + 0.5f) / (float) BRK_SEEDS);
            for (int s = 0; s < 16; ++s) e->setTrig (0, s, 1); e->setPlay (1);
            nflags = 0; std::vector<float> l (128), r (128); int last = -1;
            for (int i = 0; i < (int) (secs * 48000); i += 128)
            {
                e->process (l.data(), r.data(), 128, nullptr); A.insert (A.end(), l.begin(), l.end());
                const Voice& v = e->tr[0].voice[e->tr[0].cur];
                if (e->tr[0].hitCount != last) { last = e->tr[0].hitCount; nflags |= v.bs.flags; }
            }
        };
        std::vector<float> A, B, Cl; int f1, f2, f3;
        chopRun (0.3f, 0.5f, 0.3f, 0.3f, 0.3f, 0.15f, 7, 2.2 * P4 / 48000.0, A, f1);
        // (the clicks are measured without PJMP: an octave up doubles the hits' own sharpness)
        chopRun (0.3f, 0.5f, 0.3f, 0.3f, 0.0f, 0.15f, 7, 2.2 * P4 / 48000.0, Cl, f3);
        check (f1 == 63, "SWAP RAND REV STUT PJMP SKIP: all of them happen (flags)", f1);
        double d2 = 0, s2 = 0; for (int i = 0; i < P4 && i + P4 < (int) A.size(); ++i) { d2 += (A[i + P4] - A[i]) * (A[i + P4] - A[i]); s2 += A[i] * A[i]; }
        const double rep = 10 * log10 (d2 / (s2 + 1e-30) + 1e-30);
        check (rep < -30.0, "SEED 7: the second pass (4 bars) chops like the first (difference, dB)", rep);
        chopRun (0.3f, 0.5f, 0.3f, 0.3f, 0.3f, 0.15f, 0, 2.2 * P4 / 48000.0, B, f2);
        d2 = 0; s2 = 0; for (int i = 0; i < P4 && i + P4 < (int) B.size(); ++i) { d2 += (B[i + P4] - B[i]) * (B[i + P4] - B[i]); s2 += B[i] * B[i]; }
        const double fr = 10 * log10 (d2 / (s2 + 1e-30) + 1e-30);
        check (fr > -10.0, "SEED FREE: the second pass chops differently (difference, dB)", fr);
        auto spike = [] (const std::vector<float>& x) { double m = 0; for (size_t i = 2; i < x.size(); ++i) m = std::max (m, (double) fabsf (x[i] - 2 * x[i - 1] + x[i - 2])); return m; };
        std::vector<float> Pl; setup (0, bpm0); e->setTrig (0, 0, 1); e->setPlay (1); render (2.2 * P4 / 48000.0, Pl);
        const double sc = spike (Cl) / spike (Pl);
        check (sc < 1.6, "chopped against plain: no click sharper than the hits (spike ratio)", sc);
        for (float k : { 0.8f, 1.3f })
        {
            std::vector<float> C; setup (0, bpm0 * k); for (int s = 0; s < 16; ++s) e->setTrig (0, s, 1); e->setPlay (1); render (6.0, C);
            char what[96]; snprintf (what, sizeof what, "SYNC at x%.1f, a trig a step: no click sharper than the hits (spike ratio)", k);
            check (spike (C) / spike (Pl) < 1.6, what, spike (C) / spike (Pl));
        }
    }
    // 6. SWAP: always, with one other slot ready, plays that slot as itself; half the time, each trig keeps its place in
    //    the bar (the slice of its step in the loop it plays)
    {
        e->init (48000.f); setup (0, bpm0); e->breakClear (2);
        e->setParam (0, P_BSWP, 1.0f);
        for (int s = 0; s < 16; ++s) e->setTrig (0, s, 1); e->setPlay (1);
        std::vector<float> A; render (4.0, A);
        setup (1, bpm0); e->breakClear (2);
        for (int s = 0; s < 16; ++s) e->setTrig (0, s, 1); e->setPlay (1);
        std::vector<float> B; render (4.0, B);
        check (diffDb (B, A) < -40.0, "SWAP 100 %, one other slot: that slot, as it plays (difference, dB)", diffDb (B, A));
        setup (2, bpm0); e->setParam (0, P_BSWP, 0.5f); e->setParam (0, P_BSED, 3.5f / BRK_SEEDS);
        for (int s = 0; s < 16; ++s) e->setTrig (0, s, 1); e->setPlay (1);
        std::vector<float> l (128), r (128); int last = -1, swaps = 0, kept = 0, trigs = 0, other[3] = { 0, 0, 0 };
        for (int i = 0; i < 48000 * 8; i += 128)
        {
            e->process (l.data(), r.data(), 128, nullptr);
            const Voice& v = e->tr[0].voice[e->tr[0].cur];
            if (e->tr[0].hitCount != last && e->tr[0].hitCount > 0)
            {
                last = e->tr[0].hitCount; ++trigs;
                const int k = e->tr[0].bAbs, Lp = e->brk.b[v.bs.brk].L16;
                if (v.bs.flags & 32) { ++swaps; other[v.bs.brk]++; }
                if (v.bs.slice == k % Lp) ++kept;
            }
        }
        check (swaps > trigs / 4 && swaps < trigs * 3 / 4 && other[0] > 0 && other[1] > 0, "SWAP 50 %: about half the trigs, into both other slots (swaps)", swaps);
        check (kept == trigs, "SWAP: every trig on its step's 16th in the loop it plays (trigs kept)", kept);
    }
    // 7. the slots: a knob on an empty slot plays the next one ready; a slot cleared while it plays goes silent; one
    //    handed over is read over the next blocks (silent until then), cheaply
    {
        setup (5, bpm0); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> A; render (2.0, A);
        setup (0, bpm0); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> B; render (2.0, B);
        check (diffDb (B, A) < -40.0, "SLOT 6 empty: it plays the next slot ready (slot 1 here) (difference, dB)", diffDb (B, A));
        setup (0, bpm0); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> C; render (1.0, C); e->breakClear (0); e->breakClear (1); e->breakClear (2); render (0.5, C);
        check (rmsDb (C, 48000 + 960, (int) C.size()) < -80.0, "every slot cleared while playing: silent (dB)", rmsDb (C, 48000 + 960, (int) C.size()));
        e->init (48000.f);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setParam (0, P_MACH, machineVal (M_BREAK)); e->setParam (0, P_VOL, 0.8f); e->setParam (0, P_A, 0.05f); e->setParam (0, P_D, (7 + 0.5f) * LN);
        e->setGlobal (G_BPM, 137.0f); e->g[G_MXON] = 0.0f;
        loadSlot (e, 0, A2, false);
        std::vector<float> l (128), r (128); int blocks = 0;
        auto c0 = std::chrono::high_resolution_clock::now();
        while (! e->brk.ready (0) && blocks < 10000) { e->process (l.data(), r.data(), 128, nullptr); ++blocks; }
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - c0).count();
        check (blocks > 10 && blocks < 400, "a 4-bar slot handed over: read within a few hundred blocks (blocks of 128)", blocks);
        check (ms / blocks < 0.5, "reading it costs little per block (ms per block, native)", ms / blocks);
        check (e->brk.b[0].L16 == 64 && fabsf (e->brk.b[0].bpm - 137.0f) < 0.05f, "read in steps as at once (16ths)", e->brk.b[0].L16);
        e->breakBars (0, 2); e->breakWork (1 << 30);
        check (e->brk.b[0].L16 == 32 && fabsf (e->brk.b[0].bpm - 68.5f) < 0.05f, "half the bars: half the tempo (BPM)", e->brk.b[0].bpm);
    }
    // 8. the transport stops: the loop lets go
    {
        setup (0, bpm0); e->setTrig (0, 0, 1); e->setPlay (1);
        std::vector<float> A; render (1.06, A); e->setPlay (0);
        const int st = (int) A.size();
        if (getenv ("BRKDBG"))
        {
            std::vector<float> l (128), r (128);
            for (int b = 0; b < 40; ++b)
            {
                e->process (l.data(), r.data(), 128, nullptr); A.insert (A.end(), l.begin(), l.end());
                double s2 = 0; for (float x : l) s2 += x * x;
                const Voice& v = e->tr[0].voice[e->tr[0].cur]; const Voice& w = e->tr[0].voice[1 - e->tr[0].cur];
                printf ("  block %d: %.1f dB  on %d rel %.3f gate %.0f active %d step %d segs %d%d%d%d | other on %d rel %.3f act %d m %d lvl %.3f\n", b, 10 * log10 (s2 / 128 + 1e-30), v.bs.on, v.bs.rel, v.bs.gate, v.active, v.step, v.bs.s[0].on, v.bs.s[1].on, v.bs.s[2].on, v.bs.s[3].on, w.bs.on, w.bs.rel, w.active, w.machine, w.level);
            }
        }
        else render (0.1, A);
        // (the loop lets go in 4 ms; the track's DC filter then settles: a kick cut short leaves it a short tail)
        check (rmsDb (A, st + 960, st + 4800) < -45.0, "stop: 20 ms after, below -45 dB (dB)", rmsDb (A, st + 960, st + 4800));
        check (rmsDb (A, st + 2160, st + 4800) < -90.0, "stop: 45 ms after, silent (dB)", rmsDb (A, st + 2160, st + 4800));
    }
    // 9. every slot: it plays, stays bounded, with DUST and chops at their top; the CPU of one track playing chopped
    {
        float worst = 0.0f, quiet = 99.0f; int bad = 0;
        for (int b = 0; b < 3; ++b)
            for (int v = 0; v < 2; ++v)
            {
                setup (b, (b == 0 ? A0 : b == 1 ? A1 : A2).bpm); e->setParam (0, P_B, v ? 0.75f : 0.25f);
                if (v) { e->setParam (0, P_E, 1.0f); e->setParam (0, P_BRND, 1.0f); e->setParam (0, P_BSTU, 0.5f); e->setParam (0, P_BREV, 0.5f); e->setParam (0, P_BPJM, 0.5f); e->setParam (0, P_BSWP, 0.5f); }
                for (int s = 0; s < 16; s += 1 + v) e->setTrig (0, s, 1); e->setPlay (1);
                std::vector<float> A; render (3.0, A);
                float pk = 0; for (float x : A) { if (! std::isfinite (x)) ++bad; pk = std::max (pk, fabsf (x)); }
                worst = std::max (worst, pk); quiet = std::min (quiet, (float) rmsDb (A, 0, (int) A.size()));
            }
        check (bad == 0 && worst < 1.5f, "every slot, plain and chopped: bounded (peak)", worst);
        check (quiet > -36.0f, "every slot: none silent (quietest, dB)", quiet);
        setup (2, 170.0f); e->setParam (0, P_BRND, 0.4f); e->setParam (0, P_BSTU, 0.3f); e->setParam (0, P_BREV, 0.2f); e->setParam (0, P_BSWP, 0.3f);
        for (int s = 0; s < 16; ++s) e->setTrig (0, s, 1); e->setPlay (1);
        std::vector<float> l (128), r (128);
        auto c0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 48000 * 10; i += 128) e->process (l.data(), r.data(), 128, nullptr);
        const double cms = std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - c0).count();
        setup (2, 170.0f); e->setParam (0, P_VOL, 0.0f); e->setPlay (1);
        c0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 48000 * 10; i += 128) e->process (l.data(), r.data(), 128, nullptr);
        const double base = std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - c0).count();
        check ((cms - base) / 10000.0 * 100.0 < 3.0, "CPU of one chopped BREAK track at 170 BPM (% of real time, native)", (cms - base) / 100.0);
    }
    printf ("%d failure(s)\n", fails);
    delete e;
    return fails ? 1 : 0;
}

// ----------------------------------------------------------------- the FX machines
// A RAM track (T1) plays a test signal written straight into its buffer (an impulse, a tone burst, a sine, noise, a
// drum loop), the FX machine on T2 takes it (SRC NBR); the master is set neutral (no EQ, compressor, drive) and the
// output read as mid / side.
struct FxRig
{
    Engine* e;
    void base (int machine, float bpm = 120.0f)
    {
        e->init (48000.f);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setGlobal (G_EQON, 0.0f); e->setGlobal (G_COON, 0.0f); e->setGlobal (G_DRON, 0.0f); e->g[G_MXON] = 0.0f;
        e->setGlobal (G_BPM, bpm);
        e->setParam (0, P_MACH, machineVal (M_RAM)); e->setParam (0, P_VOL, 0.8f);
        e->setParam (0, P_A, 0.0f); e->setParam (0, P_B, 1.0f); e->setParam (0, P_C, 0.1f); e->setParam (0, P_DEC, 1.0f);
        e->setParam (0, P_E, 0.05f); e->setParam (0, P_F, 0.0f); e->setParam (0, P_D, 0.0f);      // (the whole take: no slices)
        if (machine >= 0)
        {
            e->setParam (1, P_MACH, machineVal (machine)); e->setParam (1, P_VOL, 0.8f);
            e->setParam (1, P_A, 0.05f); e->setParam (1, P_E, 1.0f); e->setParam (1, P_DEC, 1.0f); e->setParam (1, P_F, 0.0f);
            e->setParam (1, P_XE, 0.0f);
        }
    }
    void source (const std::vector<float>& x, int t = 0, bool loop = false)
    {
        const int n = std::min ((int) x.size(), (int) RAM_LEN);
        for (int i = 0; i < n; ++i) e->ramMem[t][i] = (short) std::max (-32767.0, std::min (32767.0, std::round (x[(size_t) i] * 32767.0)));
        e->ramLoaded (t, n, 48000.f);
        if (loop) e->setParam (t, P_C, (2 + 0.5f) / 4.0f);
    }
    void play (int t = 0, float vel = 1.0f) { e->audition (t, vel, 0.0f); }
    // render: L / R, then a callback every block (automation)
    void render (double secs, std::vector<float>& M, std::vector<float>& S, const std::function<void (int)>& each = nullptr)
    {
        std::vector<float> l (128), r (128);
        const int blocks = (int) (secs * 48000 / 128);
        for (int b = 0; b < blocks; ++b)
        {
            if (each) each (b);
            e->process (l.data(), r.data(), 128, nullptr);
            for (int i = 0; i < 128; ++i) { M.push_back (0.5f * (l[i] + r[i])); S.push_back (0.5f * (l[i] - r[i])); }
        }
    }
};
static double eDb (const std::vector<float>& x, int a, int b)
{
    a = std::max (a, 0); b = std::min (b, (int) x.size());
    double s2 = 0; for (int i = a; i < b; ++i) s2 += (double) x[i] * x[i];
    return 10 * log10 (s2 / std::max (1, b - a) + 1e-30);
}
static double nullDb (const std::vector<float>& a, const std::vector<float>& b, int from, int lag = 0)
{
    double d2 = 0, s2 = 0;
    for (int i = from; i < (int) a.size() && i - lag < (int) b.size(); ++i) { if (i - lag < 0) continue; const double d = a[i] - b[i - lag]; d2 += d * d; s2 += (double) b[i - lag] * b[i - lag]; }
    return 10 * log10 (d2 / (s2 + 1e-30) + 1e-30);
}
static double spikeOf (const std::vector<float>& x, int a = 2, int b = -1)
{
    if (b < 0) b = (int) x.size();
    double m = 0; for (int i = std::max (a, 2); i < b; ++i) m = std::max (m, (double) fabs (x[i] - 2 * x[i - 1] + x[i - 2])); return m;
}
// the worst click: each sample's curvature against the curvature around it (±2 ms, a floor under it): a sine of
// any pitch reads √2, a jump in it far above (√193 at most)
static int clickAt = 0;
static double clickOf (const std::vector<float>& x, int a, double floor)
{
    const int W = 96, n = (int) x.size();
    std::vector<double> d2 ((size_t) n, 0.0), cs ((size_t) n + 1, 0.0);
    for (int i = 2; i < n; ++i) d2[(size_t) i] = fabs (x[(size_t) i] - 2 * x[(size_t) i - 1] + x[(size_t) i - 2]);
    for (int i = 0; i < n; ++i) cs[(size_t) i + 1] = cs[(size_t) i] + d2[(size_t) i] * d2[(size_t) i];
    double m = 0;
    for (int i = std::max (a, W + 2); i < n - W; ++i)
    {
        const double rms = sqrt ((cs[(size_t) (i + W + 1)] - cs[(size_t) (i - W)]) / (2 * W + 1));
        const double r = d2[(size_t) i] / (rms + floor);
        if (r > m) { m = r; clickAt = i; }
    }
    return m;
}
// a real FFT magnitude of x[a .. a + N) (N a power of two), Hann window
static std::vector<double> magSpec (const std::vector<float>& x, int a, int N)
{
    std::vector<double> re ((size_t) N), im ((size_t) N, 0.0);
    for (int i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * cos (2 * M_PI * i / N); re[(size_t) i] = (a + i < (int) x.size() ? x[(size_t) (a + i)] : 0.0) * w; }
    for (int i = 1, j = 0; i < N; ++i) { int bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { std::swap (re[(size_t) i], re[(size_t) j]); std::swap (im[(size_t) i], im[(size_t) j]); } }
    for (int len = 2; len <= N; len <<= 1)
    {
        const double ang = -2 * M_PI / len;
        for (int i = 0; i < N; i += len)
            for (int k = 0; k < len / 2; ++k)
            {
                const double wr = cos (ang * k), wi = sin (ang * k);
                const double ur = re[(size_t) (i + k)], ui = im[(size_t) (i + k)];
                const double vr = re[(size_t) (i + k + len / 2)] * wr - im[(size_t) (i + k + len / 2)] * wi, vi = re[(size_t) (i + k + len / 2)] * wi + im[(size_t) (i + k + len / 2)] * wr;
                re[(size_t) (i + k)] = ur + vr; im[(size_t) (i + k)] = ui + vi; re[(size_t) (i + k + len / 2)] = ur - vr; im[(size_t) (i + k + len / 2)] = ui - vi;
            }
    }
    std::vector<double> m ((size_t) (N / 2));
    for (int k = 0; k < N / 2; ++k) m[(size_t) k] = sqrt (re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k]);
    return m;
}
// the reverb's T30 (Schroeder integration of the energy, -5 .. -35 dB), as an RT60
static double rt60Of (const std::vector<float>& x, int a)
{
    std::vector<double> E (x.size() - (size_t) a);
    double acc = 0; for (int i = (int) x.size() - 1; i >= a; --i) { acc += (double) x[(size_t) i] * x[(size_t) i]; E[(size_t) (i - a)] = acc; }
    const double e0 = E[0];
    int i5 = -1, i35 = -1;
    for (int i = 0; i < (int) E.size(); ++i) { const double db = 10 * log10 (E[(size_t) i] / e0 + 1e-30); if (i5 < 0 && db < -5) i5 = i; if (i35 < 0 && db < -35) { i35 = i; break; } }
    if (i5 < 0 || i35 < 0) return -1;
    return 2.0 * (i35 - i5) / 48000.0;
}
// the normalised echo density (Abel and Huang): samples outside one standard deviation in a 20 ms window, over erfc (1 / sqrt 2)
static double nedAt (const std::vector<float>& x, int c)
{
    const int h = 480; double s2 = 0; int n = 0;
    for (int i = c - h; i < c + h; ++i) { s2 += (double) x[(size_t) i] * x[(size_t) i]; ++n; }
    const double sd = sqrt (s2 / n); int out = 0;
    for (int i = c - h; i < c + h; ++i) if (fabs (x[(size_t) i]) > sd) ++out;
    return (double) out / n / 0.3173;
}
// the frequency of x over [a, b) from its zero crossings (rising)
static double freqOf (const std::vector<float>& x, int a, int b)
{
    int first = -1, last = -1, cnt = 0;
    for (int i = a + 1; i < b; ++i) if (x[(size_t) (i - 1)] < 0 && x[(size_t) i] >= 0) { if (first < 0) first = i; last = i; ++cnt; }
    return cnt > 1 ? (cnt - 1) * 48000.0 / (last - first) : 0;
}

// ----------------------------------------------------------------- patterns, 64 steps, HOLD
// A RAM track playing a click (an impulse take): each hit is a click, found again in the output, to the sample
static int pattest()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-92s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    Engine* e = new Engine();
    FxRig R { e };
    const int SR = 48000;
    const double spt = 48000.0 * 60.0 / 120.0 / 4.0;      // samples a 16th at 120
    auto clickRig = [&] () {
        R.base (-1);
        std::vector<float> x ((size_t) SR, 0.0f); x[0] = 0.9f;
        R.source (x);
        e->setParam (0, P_DEC, 0.05f); e->setParam (0, P_B, 1.0f);
        for (int s = 0; s < NUM_STEPS; ++s) e->setTrig (0, s, 0);
    };
    auto run = [&] (double secs, std::vector<float>& M, const std::function<void (int)>& each = nullptr, int bs = 128) {
        std::vector<float> l (bs), r (bs);
        const int blocks = (int) (secs * SR / bs);
        for (int b = 0; b < blocks; ++b) { if (each) each (b * bs); e->process (l.data(), r.data(), bs, nullptr); for (int i = 0; i < bs; ++i) M.push_back (0.5f * (l[i] + r[i])); }
    };
    // the clicks' sample positions: the peak of each burst over half the loudest (then 20 ms of rest)
    auto clicks = [] (const std::vector<float>& M) {
        float mx = 0.0f; for (float v : M) mx = std::max (mx, fabsf (v));
        std::vector<int> c; int i = 0;
        while (i < (int) M.size())
        {
            if (fabsf (M[i]) > 0.5f * mx) { int b = i; for (int k = i; k < std::min ((int) M.size(), i + 48); ++k) if (fabsf (M[k]) > fabsf (M[b])) b = k; c.push_back (b); i = b + 960; }
            else ++i;
        }
        return c;
    };
    // where the first click of a track playing step 0 lands: the offset of everything else
    clickRig(); e->setTrig (0, 0, 1); e->setLen (0, 16); e->setPlay (1);
    std::vector<float> M0; run (0.5, M0);
    const std::vector<int> c0 = clicks (M0);
    const int off = c0.empty() ? 0 : c0[0];
    auto near = [&] (const std::vector<int>& c, double sixteenth) { int best = 1 << 30; for (int x : c) best = std::min (best, std::abs (x - (int) floor (sixteenth * spt + 1e-6) - off)); return best; };

    // 1. 64 steps: a track of LEN 64 plays its step 41 at the 41st 16th, starts again at the 65th
    {
        clickRig(); e->setLen (0, 64); e->setTrig (0, 0, 1); e->setTrig (0, 40, 1); e->setPlay (1);
        std::vector<float> M; run (64 * spt / SR + 0.6, M);
        const std::vector<int> c = clicks (M);
        const int d = std::max (near (c, 40.0), near (c, 64.0));
        check (c.size() == 3 && d <= 1, "64 steps: LEN 64 plays steps 1 and 41, then 1 again at the 65th 16th (worst error, samples)", d);
    }
    // 2. a pattern queued mid-way comes at the end of the one playing (AUTO: 16), to the sample, whatever the block size
    std::vector<float> Mq[2];
    for (int k = 0; k < 2; ++k)
    {
        const int bs = k ? 37 : 128;
        clickRig(); e->setTrig (0, 0, 1); e->setLen (0, 16);
        e->patEdit (1); e->setTrig (0, 0, 1); e->setTrig (0, 8, 1); e->setLen (0, 16); e->patEdit (0);
        e->setPlay (1);
        run (16 * spt / SR + 0.3 + 1.0, Mq[k], [&] (int at) { if (at <= (int) (0.3 * SR) && at + bs > (int) (0.3 * SR)) e->patQueue (1, 0); }, bs);
    }
    {
        const std::vector<int> c = clicks (Mq[0]);
        const int d = std::max (std::max (near (c, 0.0), near (c, 16.0)), near (c, 24.0));
        check (c.size() >= 3 && near (c, 4.0) > 100 && d <= 1 && e->curPat == 1, "SEQ: A02 queued at 0.3 s comes at the 17th 16th (its steps 1 and 9), not before (worst error, samples)", d);
        const std::vector<int> c37 = clicks (Mq[1]);
        int dd = (int) c.size() == (int) c37.size() ? 0 : 99999;
        for (size_t i = 0; i < c.size() && i < c37.size(); ++i) dd = std::max (dd, std::abs (c[i] - c37[i]));
        check (dd == 0, "SEQ: the change lands on the same sample with blocks of 37 (worst difference of the hits, samples)", dd);
    }
    // 2b. queued after A01 has gone round 3 times: at the end of its 4th time (the 65th 16th), not at once
    {
        clickRig(); e->setTrig (0, 0, 1); e->setLen (0, 16);
        e->patEdit (1); e->setTrig (0, 0, 1); e->setTrig (0, 8, 1); e->setLen (0, 16); e->patEdit (0);
        e->setPlay (1);
        const int qAt = (int) (3.3 * 16 * spt);
        std::vector<float> M; run (64 * spt / SR + 1.3, M, [&] (int at) { if (at <= qAt && at + 128 > qAt) e->patQueue (1, 0); });
        const std::vector<int> c = clicks (M);
        const int d = std::max (std::max (near (c, 48.0), near (c, 64.0)), near (c, 72.0));
        check (c.size() == 6 && near (c, 56.0) > 100 && d <= 1 && e->curPat == 1, "SEQ: A02 queued after A01 went round 3 times comes at the end of its 4th (the 65th 16th; worst error, samples)", d);
    }
    // 3. a chain A01 -> A02 -> A01: each its 16 steps in turn
    {
        clickRig(); e->setTrig (0, 0, 1);
        e->patEdit (1); e->setTrig (0, 0, 1); e->setTrig (0, 8, 1); e->patEdit (0);
        e->patChain (0, 0); e->patChain (1, 1); e->patChainLen (2);
        e->setPlay (1);
        std::vector<float> M; run (48 * spt / SR + 0.3, M);
        const std::vector<int> c = clicks (M);
        const int d = std::max (std::max (std::max (near (c, 16.0), near (c, 24.0)), near (c, 32.0)), near (c, 48.0));
        check (c.size() == 5 && near (c, 8.0) > 100 && near (c, 40.0) > 100 && d <= 1, "chain A01 A02: 1, then 17 and 25, then 33 (A01 again), 49 (A02), nothing at 9 or 41 (worst error, samples)", d);
    }
    // 4. START: at once, from its first step
    {
        clickRig(); e->setTrig (0, 0, 1);
        e->patEdit (1); e->setTrig (0, 0, 1); e->patEdit (0);
        e->setPlay (1);
        std::vector<float> M; run (1.0, M, [&] (int at) { if (at <= (int) (0.3 * SR) && at + 128 > (int) (0.3 * SR)) e->patQueue (1, 1); });
        const std::vector<int> c = clicks (M);
        const int at = c.size() >= 2 ? c[1] - off - (int) (0.3 * SR) : 99999;
        check (c.size() >= 2 && at >= -128 && at <= 128, "START: A02 starts at once from its first step (its first hit after the call, samples)", at);
    }
    // 5. an edit for the pattern the page shows lands there, even while the engine plays another; an empty pattern takes
    //    the sounds and no trigs
    {
        clickRig(); e->setTrig (0, 0, 1); e->setParam (0, P_VOL, 0.7f);
        e->setPlay (1);
        std::vector<float> M; run (0.1, M);
        e->patQueue (5, 1);                              // (never written)
        run (0.05, M);
        const bool fresh = e->curPat == 5 && e->tr[0].trig[0] == 0 && fabsf (e->tr[0].p[P_VOL] - 0.7f) < 1e-6f && e->tr[0].len == 16;
        // the page still edits A01: a trig on step 5 goes to A01's slot, not to A06 playing
        e->setTrig (0, 4, 1);
        const bool routed = e->tr[0].trig[4] == 0 && e->pats[0].trk[0].trig[4] == 1;
        e->patQueue (0, 1); run (0.05, M);
        check (fresh && routed && e->tr[0].trig[4] == 1 && e->tr[0].trig[0] == 1, "an empty pattern: the sounds stay, no trigs; an edit for the pattern shown lands in its slot, back with it", fresh + 2 * routed);
    }
    // 6. HOLD (DEC at its top): a pad held holds the note, let go it ends in about 0.25 s; DEC just under: it decays
    {
        auto holdRun = [&] (int m, float dec, double heldFor, double& lvHeld, double& lvAfter, double& lvStart) {
            R.base (-1);
            e->setMute (0, 1);
            e->setParam (2, P_MACH, machineVal (m)); e->setParam (2, P_VOL, 0.8f); e->setParam (2, P_DEC, dec);
            if (m == M_MACRO) e->setParam (2, P_A, 0.5f / 24.0f);
            e->audition (2, 1.0f);
            std::vector<float> M; run (heldFor, M);
            e->release (2);
            run (0.8, M);
            lvStart = eDb (M, (int) (0.05 * SR), (int) (0.3 * SR));
            lvHeld = eDb (M, (int) ((heldFor - 0.25) * SR), (int) (heldFor * SR));
            lvAfter = eDb (M, (int) ((heldFor + 0.5) * SR), (int) ((heldFor + 0.8) * SR));
        };
        double h, a, s0, h2, a2, s2;
        holdRun (M_SY_RAW, 1.0f, 3.0, h, a, s0);
        holdRun (M_SY_RAW, 0.98f, 3.0, h2, a2, s2);
        check (h - s0 > -1.0 && a - s0 < -40.0 && h2 - s2 < -6.0, "HOLD on SY-RAW: held 3 s at its level (dB against its start), let go -40 dB in 0.5 s; DEC 98 % decays", h - s0);
        holdRun (M_MACRO, 1.0f, 3.0, h, a, s0);
        check (h - s0 > -6.0 && a - s0 < -30.0, "HOLD on MACRO (the low-pass gate follows the gate): held 3 s (dB against its start), let go it closes", h - s0);
        holdRun (M_HALO, 0.5f, 4.0, h, a, s0);
        check (h - s0 > -6.0 && a - s0 < -20.0, "HALO, stopped: the chord holds while its pad is (4 s, dB against its start), let go it releases", h - s0);
    }
    // 7. HOLD from the sequencer: a trig's GATE holds the note (2 steps: 0.25 s at 120, then its release), GATE 0 holds
    // it until the track's next trig
    {
        auto seqRun = [&] (float gate, double& lvStart, double& lvMid, double& lvLate) {
            R.base (-1);
            e->setMute (0, 1);
            e->setParam (2, P_MACH, machineVal (M_SY_RAW)); e->setParam (2, P_VOL, 0.8f); e->setParam (2, P_DEC, 1.0f); e->setParam (2, P_GATE, gate);
            e->setLen (2, 64); e->setTrig (2, 0, 1); e->setPlay (1);
            std::vector<float> M; run (2.5, M);
            lvStart = eDb (M, (int) (0.03 * SR), (int) (0.2 * SR));
            lvMid = eDb (M, (int) (0.8 * SR), (int) (1.2 * SR));
            lvLate = eDb (M, (int) (2.1 * SR), (int) (2.5 * SR));
            e->setPlay (0);
        };
        double s1, m1, l1, s2, m2, l2;
        seqRun (0.25f, s1, m1, l1);
        seqRun (0.0f, s2, m2, l2);
        check (m1 - s1 < -40.0 && m2 - s2 > -1.0 && l2 - s2 > -1.0, "HOLD from the sequencer: GATE 2 steps lets go (dB 0.8 s on), GATE 0 holds until the next trig", m1 - s1);
    }
    // 8. a note held until its next trig (GATE 0) lets go on a stop, a mute, a change to a pattern without trigs; a
    //    RAM loop on HOLD and MACRO too (before: they went on for ever, only INIT stopped them)
    {
        // what: 0 stop, 1 mute, 2 an empty pattern; returns the level 0.4..0.8 s after it against the level before (dB)
        auto letGoRun = [&] (int m, int what) {
            R.base (-1);
            e->setMute (0, 1);
            const int t = m == M_RAM ? 0 : 2;
            if (m == M_RAM)
            {
                std::vector<float> x ((size_t) SR, 0.0f);
                for (int i = 0; i < SR; ++i) x[(size_t) i] = 0.5f * (float) sin (2 * M_PI * 220.0 * i / SR);
                R.source (x, 0, true);
                e->setMute (0, 0); e->setParam (0, P_DEC, 1.0f); e->setParam (0, P_GATE, 0.0f);
            }
            else
            {
                e->setParam (2, P_MACH, machineVal (m)); e->setParam (2, P_VOL, 0.8f); e->setParam (2, P_DEC, 1.0f); e->setParam (2, P_GATE, 0.0f);
                if (m == M_MACRO) e->setParam (2, P_A, 0.5f / 24.0f);
            }
            e->setLen (t, 64); e->setTrig (t, 0, 1); e->setPlay (1);
            std::vector<float> M; run (1.2, M);
            const double before = eDb (M, (int) (0.8 * SR), (int) (1.2 * SR));
            if (what == 0) e->setPlay (0);
            else if (what == 1) e->setMute (t, 1);
            else e->patQueue (9, 1);
            run (0.8, M);
            const double after = eDb (M, (int) (1.6 * SR), (int) (2.0 * SR));
            e->setPlay (0); e->setMute (t, 0); e->patQueue (0, 1);
            return after - before;
        };
        const double a0 = letGoRun (M_SY_RAW, 0), a1 = letGoRun (M_SY_RAW, 1), a2 = letGoRun (M_SY_RAW, 2);
        const double a3 = letGoRun (M_MACRO, 0), a4 = letGoRun (M_RAM, 0), a5 = letGoRun (M_ITERA, 0), a6 = letGoRun (M_ITERA, 1);
        check (a0 < -40.0 && a1 < -40.0 && a2 < -40.0, "HOLD until the next trig: a stop, a mute, an empty pattern let the note go (worst, dB 0.4 s after)", std::max (a0, std::max (a1, a2)));
        check (a3 < -30.0 && a4 < -40.0 && a5 < -40.0 && a6 < -40.0, "until the next trig: MACRO, a RAM loop on HOLD, ITERA's free run let go on a stop or a mute (worst, dB)", std::max (std::max (a3, a4), std::max (a5, a6)));
    }
    // 9. RCRV: a retrig of 4 hits at 1/16 closes in (right: faster and faster) or spreads out (left), the gaps changing
    //    by the same ratio, 16x from the first to the last at the ends; its last hit where the even roll puts it (60 BPM)
    {
        const double sp6 = 48000.0 * 60.0 / 60.0 / 4.0;
        auto roll = [&] (float crv) {
            R.base (-1, 60.0f);
            std::vector<float> x ((size_t) SR, 0.0f); x[0] = 0.9f;
            R.source (x);
            e->setParam (0, P_DEC, 0.05f); e->setParam (0, P_B, 1.0f);
            for (int s = 0; s < NUM_STEPS; ++s) e->setTrig (0, s, 0);
            e->setLen (0, 64); e->setTrig (0, 0, 1);
            e->setParam (0, P_RTRG, 3.5f / 12.0f); e->setParam (0, P_RCNT, 3.5f / 16.0f); e->setParam (0, P_RCRV, 0.5f + 0.5f * crv);
            e->setPlay (1);
            std::vector<float> M; run (5.0 * sp6 / SR + 0.3, M);
            e->setPlay (0);
            return clicks (M);
        };
        auto expect = [&] (float crv) {
            std::vector<double> at { 0.0 };
            if (crv == 0.0f) { for (int k = 1; k <= 4; ++k) at.push_back (k); return at; }
            const double ex = 4.0 * (crv > 0 ? -std::fabs (crv) : std::fabs (crv)), q = std::pow (2.0, ex / 3.0);
            double g = 4.0 * (1.0 - q) / (1.0 - std::pow (q, 4.0)), tt = 0.0;
            for (int k = 0; k < 4; ++k) { tt += g; at.push_back (tt); g *= q; }
            return at;
        };
        int worst = 0; bool shape = true;
        for (float crv : { 0.0f, 1.0f, -1.0f, 0.5f })
        {
            const std::vector<int> c = roll (crv);
            const std::vector<double> at = expect (crv);
            if (c.size() != at.size()) { worst = 99999; continue; }
            for (size_t k = 0; k < c.size(); ++k) worst = std::max (worst, std::abs (c[k] - off - (int) std::lround (at[k] * sp6)));
            for (size_t k = 2; k < c.size(); ++k)
            {
                const int g0 = c[k - 1] - c[k - 2], g1 = c[k] - c[k - 1];
                if (crv > 0 && g1 >= g0) shape = false;
                if (crv < 0 && g1 <= g0) shape = false;
            }
        }
        check (worst <= 3 && shape, "RCRV: the hits close in (right) or spread out (left), the last one where an even roll ends (worst error, samples)", worst);
    }
    // 10. JUMP: each trig that has one plays where the jumps have got to, then moves them on; past RNG, back to the note.
    //     Semitones; scale degrees on a quantised track; on one step only (a p-lock); again from 0 at a new play
    {
        auto jumps = [&] (int jump, int rng, bool deg, int lockStep, int count) {
            R.base (-1);
            e->setMute (0, 1);
            e->setParam (2, P_MACH, machineVal (M_SY_RAW)); e->setParam (2, P_VOL, 0.8f); e->setParam (2, P_DEC, 0.3f);
            for (int s = 0; s < NUM_STEPS; ++s) e->setTrig (2, s, s < 8 ? 1 : 0);
            e->setLen (2, 8);
            const float jv = (float) (jump + 12) + 0.5f;
            if (lockStep < 0) e->setParam (2, P_JUMP, jv / 25.0f);
            else { e->setParam (2, P_JUMP, 0.5f); e->setLock (2, lockStep, P_JUMP, jv / 25.0f); }
            e->setParam (2, P_JRNG, ((float) rng - 0.5f) / 24.0f);
            e->setScale (deg ? 1453u : 0u); e->setQuant (2, deg ? 1 : 0);      // (C minor)
            e->setPlay (1);
            std::vector<int> notes; int last = e->tr[2].hitCount;
            std::vector<float> M;
            run (((double) count - 0.5) * spt / SR, M, [&] (int) { if (e->tr[2].hitCount != last) { last = e->tr[2].hitCount; notes.push_back ((int) e->tr[2].voice[e->tr[2].cur].note); } });
            e->setPlay (0);
            if (lockStep >= 0) e->setLock (2, lockStep, P_JUMP, -1.0f);
            e->setScale (0u); e->setQuant (2, 0);
            std::string sq; for (int n : notes) sq += std::to_string (n) + ",";
            return sq;
        };
        const std::string up = jumps (2, 7, false, -1, 9), down = jumps (-3, 12, false, -1, 6), deg = jumps (2, 12, true, -1, 9);
        const std::string one = jumps (5, 12, false, 4, 32), again = jumps (2, 7, false, -1, 3);
        const bool ok = up == "0,2,4,6,0,2,4,6,0," && down == "0,-3,-6,-9,0,-3," && deg == "0,3,7,10,0,3,7,10,0,"
            && one == "0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0,0,10,0,0,0,0,0,0,0,0,0,0,0," && again == "0,2,4,";
        printf ("      jumps: %s | %s | %s | %s | %s\n", up.c_str(), down.c_str(), deg.c_str(), one.c_str(), again.c_str());
        check (ok, "JUMP: +2 in semitones, -3, +2 scale degrees (C minor), +5 on one step only, from 0 at a new play", ok);
    }
    printf ("%d failure(s)\n", fails);
    delete e;
    return fails ? 1 : 0;
}

static int fxmachtest()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-84s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    Engine* e = new Engine();
    FxRig R { e };
    const int SR = 48000;
    auto impulse = [&] (int at = 200, float a = 0.9f) { std::vector<float> x ((size_t) SR * 2, 0.0f); x[(size_t) at] = a; return x; };
    auto burst = [&] (float hz, float secs, float a = 0.5f) { std::vector<float> x ((size_t) (SR * (secs + 0.1))); for (int i = 0; i < (int) (secs * SR); ++i) x[(size_t) (i + 200)] = a * (float) (sin (2 * M_PI * hz * i / SR) * pow (sin (M_PI * i / (secs * SR)), 2)); return x; };
    auto sine = [&] (float hz, float secs, float a = 0.5f) { std::vector<float> x ((size_t) (SR * secs)); for (int i = 0; i < (int) x.size(); ++i) x[(size_t) i] = a * (float) sin (2 * M_PI * hz * i / SR); return x; };
    auto noise = [&] (float secs, float a = 0.25f) { Rng r; r.seed (77); std::vector<float> x ((size_t) (SR * secs)); for (auto& v : x) v = a * r.bi(); return x; };
    auto drums = [&] () {
        // a drum loop at 48 kHz, 2 bars at 120 (kick, snare, hats), as the BREAK tests make it
        const TestLoop T = synthLoop (120.0f, 2, 0);
        std::vector<float> x; for (size_t i = 0; i < T.x.size() * 3 / 2; ++i) { const double p = (double) i * 32000.0 / 48000.0; const int i0 = (int) p; const double f = p - i0;
            const double a = T.x[(size_t) std::min (i0, (int) T.x.size() - 1)], b = T.x[(size_t) std::min (i0 + 1, (int) T.x.size() - 1)]; x.push_back ((float) ((a + (b - a) * f) / 32767.0)); }
        return x; };
    const int FXM[5] = { M_FXVERB, M_FXECHO, M_FXBEND, M_FXTAPE, M_FXCODEC };
    const char* FXN[5] = { "VERB", "ECHO", "BEND", "TAPE", "CODEC" };

    // 1b. MIX 0 with every knob of the machine at the top (LOSS, BAND, JITR, PKTS... the effect pushed as far as it
    //     goes): still the source as it is
    {
        const std::vector<float> D = drums();
        R.base (-1); R.source (D); R.play();
        std::vector<float> A, As; R.render (3.0, A, As);
        for (int k = 0; k < 5; ++k)
        {
            double worst = -300.0;
            for (int v = 0; v < 3; ++v)
            {
                const float hi = v == 0 ? 1.0f : v == 1 ? 0.0f : 0.73f;
                R.base (FXM[k]);
                for (int q : { P_B, P_C, P_D, P_TUNE, P_XA, P_XB, P_XC, P_XD, P_XF, P_XG }) R.e->setParam (1, q, hi);
                R.e->setParam (1, P_E, 0.0f); R.source (D); R.play();
                std::vector<float> B, Bs; R.render (3.0, B, Bs);
                worst = std::max (worst, nullDb (B, A, 4800, FXM[k] == M_FXCODEC ? FX_CFN : 0));
            }
            char w[128]; snprintf (w, sizeof w, "%s, MIX 0, its knobs at the top, at the bottom, between: still the source (dB)", FXN[k]);
            check (worst < -90.0, w, worst);
        }
    }
    // 1c. the MIX section's unmasking on (full depth): an FX track is not one more track in it (it would collide with
    //     its own sources and be carved out, at MIX 0 too): MIX 0 is still the source, as the mix had it without the FX
    {
        const std::vector<float> D = drums();
        auto um = [&] () { R.e->g[G_MXON] = 1.0f; R.e->setGlobal (G_UMSK, 1.0f); R.e->setGlobal (G_CALM, 0.0f); };
        R.base (-1); um(); R.source (D); R.play();
        std::vector<float> A, As; R.render (3.0, A, As);
        for (int k : { 0, 4 })
        {
            R.base (FXM[k]); um(); R.e->setParam (1, P_B, 0.8f); R.e->setParam (1, P_E, 0.0f); R.source (D); R.play();
            std::vector<float> B, Bs; R.render (3.0, B, Bs);
            const double nd = nullDb (B, A, 4800, FXM[k] == M_FXCODEC ? FX_CFN : 0);
            char w[128]; snprintf (w, sizeof w, "%s, MIX 0, the mix's unmasking on: the source as the mix had it (difference, dB)", FXN[k]);
            check (nd < -90.0, w, nd);
        }
    }
    // 1. the insert: MIX 0 gives back the source exactly (once: the source no longer reaches the mix itself); CODEC
    //    256 samples late
    {
        const std::vector<float> D = drums();
        R.base (-1); R.source (D); R.play();
        std::vector<float> A, As; R.render (3.0, A, As);
        for (int k = 0; k < 5; ++k)
        {
            R.base (FXM[k]); R.e->setParam (1, P_E, 0.0f); R.source (D); R.play();
            std::vector<float> B, Bs; R.render (3.0, B, Bs);
            const double nd = nullDb (B, A, 4800, FXM[k] == M_FXCODEC ? FX_CFN : 0);
            char w[128]; snprintf (w, sizeof w, "%s, MIX 0: the source as it is, taken from the mix (difference, dB)", FXN[k]);
            check (nd < (FXM[k] == M_FXCODEC ? -60.0 : -90.0), w, nd);
        }
    }
    // 2. what it takes: SUM (every track before), one track named (the others stay in the mix), a chain, a mute
    {
        const std::vector<float> D = drums(), S2 = sine (330.0f, 3.0f, 0.3f);
        auto two = [&] (int fxTrack, float src, float vol3) {
            R.base (-1); R.source (D, 0); R.source (S2, 1);
            R.e->setParam (1, P_MACH, machineVal (M_RAM)); R.e->setParam (1, P_VOL, 0.8f); R.e->setParam (1, P_A, 0.0f); R.e->setParam (1, P_B, 1.0f);
            R.e->setParam (1, P_C, 0.1f); R.e->setParam (1, P_DEC, 1.0f); R.e->setParam (1, P_E, 0.05f); R.e->setParam (1, P_D, 0.0f);
            if (fxTrack >= 0) { R.e->setParam (fxTrack, P_MACH, machineVal (M_FXVERB)); R.e->setParam (fxTrack, P_VOL, vol3); R.e->setParam (fxTrack, P_A, src); R.e->setParam (fxTrack, P_E, 0.0f); R.e->setParam (fxTrack, P_DEC, 1.0f); }
            R.play (0); R.play (1);
            std::vector<float> M, S; R.render (2.0, M, S); return M; };
        const std::vector<float> both = two (-1, 0, 0);
        const std::vector<float> sum = two (2, (1 + 0.5f) / FX_SRCS, 0.8f);
        check (nullDb (sum, both, 4800) < -90.0, "SUM: every track before, through the effect (MIX 0: the same mix) (difference, dB)", nullDb (sum, both, 4800));
        const std::vector<float> one = two (2, (2 + 0.5f) / FX_SRCS, 0.0f);       // T1 taken by a silent FX track: only T2 is left
        R.base (-1); R.source (S2, 1); R.e->setParam (1, P_MACH, machineVal (M_RAM)); R.e->setParam (1, P_VOL, 0.8f); R.e->setParam (1, P_A, 0.0f); R.e->setParam (1, P_B, 1.0f);
        R.e->setParam (1, P_C, 0.1f); R.e->setParam (1, P_DEC, 1.0f); R.e->setParam (1, P_E, 0.05f); R.e->setParam (1, P_D, 0.0f); R.play (1);
        std::vector<float> M2, S2b; R.render (2.0, M2, S2b);
        check (nullDb (one, M2, 4800) < -90.0, "T1 named: T1 goes into the effect (VOL 0 here), T2 stays in the mix (difference, dB)", nullDb (one, M2, 4800));
        // a chain: T1 -> ECHO (T2) -> VERB (T3, NBR): the echoes reverberated; MIX 0 on both gives T1 back
        R.base (M_FXECHO); R.source (D); R.e->setParam (1, P_E, 0.0f);
        R.e->setParam (2, P_MACH, machineVal (M_FXVERB)); R.e->setParam (2, P_VOL, 0.8f); R.e->setParam (2, P_A, 0.05f); R.e->setParam (2, P_E, 0.0f); R.e->setParam (2, P_DEC, 1.0f);
        R.play(); std::vector<float> C, Cs; R.render (2.0, C, Cs);
        R.base (-1); R.source (D); R.play(); std::vector<float> A, As; R.render (2.0, A, As);
        check (nullDb (C, A, 4800) < -90.0, "a chain (T1 -> ECHO -> VERB), MIX 0 on both: T1 once (difference, dB)", nullDb (C, A, 4800));
        // the same chain with both on SUM (every track to their left): the second takes only the first (T1 is in it)
        R.base (M_FXECHO); R.source (D); R.e->setParam (1, P_E, 0.0f); R.e->setParam (1, P_A, 0.15f);
        R.e->setParam (2, P_MACH, machineVal (M_FXVERB)); R.e->setParam (2, P_VOL, 0.8f); R.e->setParam (2, P_A, 0.15f); R.e->setParam (2, P_E, 0.0f); R.e->setParam (2, P_DEC, 1.0f);
        R.play(); std::vector<float> Cu, Cus; R.render (2.0, Cu, Cus);
        const u32 m1 = R.e->fx[1].srcMask, m2 = R.e->fx[2].srcMask;
        check (nullDb (Cu, A, 4800) < -90.0 && m1 == 1u && m2 == 2u, "two FX on SUM in a row: a chain (the second takes the first, T1 once; difference, dB)", nullDb (Cu, A, 4800));
        R.base (M_FXVERB); R.source (D); R.e->setMute (1, 1); R.play();
        std::vector<float> Mu, Mus; R.render (1.0, Mu, Mus);
        check (eDb (Mu, 4800, (int) Mu.size()) < -100.0, "the FX track muted: its source with it (dB)", eDb (Mu, 4800, (int) Mu.size()));
    }
    // 3. the gate (IN): a trig opens the effect for DEC; before it (DEC under HOLD) nothing goes in
    {
        const std::vector<float> N = noise (4.0f);
        auto run = [&] (float dec, bool trig) {
            R.base (M_FXVERB); R.source (N); R.e->setParam (1, P_DEC, dec); R.e->setParam (1, P_C, 0.4f);
            R.play (0); if (trig) R.play (1);
            std::vector<float> M, S; R.render (3.0, M, S); return M; };
        const std::vector<float> none = run (0.5f, false), open = run (0.5f, true), held = run (1.0f, false);
        check (eDb (none, 4800, 48000) < -100.0, "IN, DEC 0.5, no trig: closed, nothing goes in (dB)", eDb (none, 4800, 48000));
        const double early = eDb (open, 480, 2400) - eDb (held, 480, 2400), late = eDb (open, 120000, 144000), heldL = eDb (held, 120000, 144000);
        if (getenv ("FXDBG")) printf ("  gate: early %.1f late %.1f held %.1f\n", early, late, heldL);
        check (early > -10.0 && late < heldL - 15.0, "a trig: open at once, closed again (2.5 s later, against HOLD: dB)", late - heldL);
    }
    // 4. VERB (after the Erbe-Verb's design): its decay as it says, longer with SIZE (a gain a pass), metal when small,
    // smoother large; sustains at 100 %, bounded at 125 %; glides when SIZE moves; freezes; plays its pre-delay backwards
    {
        auto verbIR = [&] (float size, float dcy, float dpth, float shape, std::vector<float>& M, std::vector<float>& S, double secs = 6.0) {
            R.base (M_FXVERB); R.source (impulse()); R.e->setParam (1, P_B, size); R.e->setParam (1, P_C, dcy); R.e->setParam (1, P_D, 0.0f);
            R.e->setParam (1, P_XA, 0.3f); R.e->setParam (1, P_XB, dpth); R.e->setParam (1, P_XC, 0.0f); R.e->setParam (1, P_XD, shape); R.play();
            M.clear(); S.clear(); R.render (secs, M, S); return (double) R.e->fx[1].vRt; };
        std::vector<float> M, S;
        const double said = verbIR (0.7f, 0.6f, 0.15f, 0.0f, M, S), rt = rt60Of (M, 2000);
        if (getenv ("FXDBG")) printf ("  verb rt %.2f (says %.2f)\n", rt, said);
        check (rt > said * 0.75 && rt < said * 1.3, "VERB: its decay as its readout says (RT60 from T30 against the readout, x)", rt / said);
        std::vector<float> M5, S5, M8, S8;
        const double rt5 = rt60Of ((verbIR (0.5f, 0.6f, 0.15f, 0.0f, M5, S5), M5), 2000), rt8 = rt60Of ((verbIR (0.85f, 0.6f, 0.15f, 0.0f, M8, S8), M8), 2000);
        check (rt8 > 1.8 * rt5, "VERB: the same DECAY lasts longer in a larger SIZE (RT60 at SIZE 85 % over 50 %, x)", rt8 / rt5);
        const double ned = nedAt (M, (int) (0.15 * SR));
        check (ned > 0.5, "VERB: dense by 150 ms (normalised echo density)", ned);
        // its colour: the tail's spectrum at 15 Hz resolution, its slope taken off: against an ideal tail (noise
        // decaying in the same time), a small room rings (metal), a large one modulated much less
        auto rough = [&] (const std::vector<float>& x, int at) {
            const std::vector<double> mg = magSpec (x, at, 65536);
            std::vector<double> pw (mg.size()); for (size_t k = 0; k < mg.size(); ++k) pw[k] = mg[k] * mg[k];
            std::vector<double> dbs;
            for (int k = 400; k < 11000; ++k)
            {
                double sm = 0, loc = 0; for (int j = k - 10; j <= k + 10; ++j) sm += pw[(size_t) j]; for (int j = k - 160; j <= k + 160; ++j) loc += pw[(size_t) j];
                dbs.push_back (10 * log10 ((sm / 21) / (loc / 321) + 1e-30));
            }
            std::sort (dbs.begin(), dbs.end());
            return dbs[(size_t) (dbs.size() * 99 / 100)] - dbs[dbs.size() / 2]; };
        std::vector<float> ideal (M.size()); { Rng rn; rn.seed (3); for (size_t i = 0; i < ideal.size(); ++i) ideal[i] = rn.bi() * (float) exp (-6.9 * (double) i / (rt > 0 ? rt : 2.0) / SR); }
        std::vector<float> Mc, Sc, Ml, Sl;
        verbIR (0.22f, 0.72f, 0.0f, 0.0f, Mc, Sc); verbIR (0.85f, 0.6f, 0.35f, 0.0f, Ml, Sl);
        const double ri = rough (ideal, (int) (0.3 * SR)), rc = rough (Mc, (int) (0.1 * SR)), rl = rough (Ml, (int) (0.3 * SR));
        if (getenv ("FXDBG")) printf ("  verb colour: ideal %.1f  small %.1f  large %.1f\n", ri, rc, rl);
        check (rc - ri > 6.0 && rl < rc - 3.0, "VERB: metal when small (its modes against an ideal tail, dB more), smoother large", rc - ri);
        const double sm = eDb (S, (int) (0.2 * SR), (int) (1.2 * SR)) - eDb (M, (int) (0.2 * SR), (int) (1.2 * SR));
        check (sm > -10.0 && sm < 3.0, "VERB: stereo: its lines left and right (side - mid in the tail, dB)", sm);
        // SIZE swept on a loop (the whole tail bends): no click
        const std::vector<float> D = drums();
        R.base (M_FXVERB); R.source (D, 0, true); R.e->setParam (1, P_C, 0.6f); R.e->setParam (1, P_E, 0.5f); R.play();
        std::vector<float> Ms, Ss; R.render (4.0, Ms, Ss, [&] (int b) { R.e->setParam (1, P_B, 0.15f + 0.8f * (float) (0.5 - 0.5 * cos (b * 0.012))); });
        R.base (M_FXVERB); R.source (D, 0, true); R.e->setParam (1, P_C, 0.6f); R.e->setParam (1, P_E, 0.5f); R.play();
        std::vector<float> Mf, Sf; R.render (4.0, Mf, Sf);
        const double fl = 0.02 * spikeOf (Mf, 4800), cs = clickOf (Ms, 4800, fl), cf = clickOf (Mf, 4800, fl);
        check (cs < cf * 1.5, "VERB: SIZE swept: no click (worst curvature against its neighbourhood's, against still)", cs / cf);
        // DECAY 100 %: no loss, the tail sustains; 125 % fed with noise for 10 s: the saturation holds it
        R.base (M_FXVERB); R.source (burst (220.0f, 0.1f, 0.2f)); R.e->setParam (1, P_C, 0.8f); R.e->setParam (1, P_D, 0.0f); R.e->setParam (1, P_XB, 0.1f); R.play();
        std::vector<float> Mh, Sh; R.render (5.0, Mh, Sh);
        const double sus = eDb (Mh, (int) (4.0 * SR), (int) (4.5 * SR)) - eDb (Mh, (int) (0.5 * SR), (int) (1.0 * SR));
        check (sus > -6.0, "VERB: DECAY 100 %: it sustains (3 s on, dB)", sus);
        R.base (M_FXVERB); R.source (noise (10.0f, 0.4f)); R.e->setParam (1, P_C, 1.0f); R.e->setParam (1, P_E, 1.0f); R.play();
        std::vector<float> Mi, Si; R.render (10.0, Mi, Si);
        float pk = 0; bool bad = false; for (float v : Mi) { pk = std::max (pk, fabsf (v)); if (! std::isfinite (v)) bad = true; }
        check (! bad && pk < 1.5f && eDb (Mi, 9 * SR, 10 * SR) > -30.0, "VERB: DECAY 125 %, fed for 10 s: bounded (peak)", pk);
        // SHAPE: grains of random lengths instead of the sine: another sound, as dense, bounded
        std::vector<float> Mg, Sg; verbIR (0.7f, 0.6f, 0.5f, 1.0f, Mg, Sg);
        std::vector<float> Mn, Sn; verbIR (0.7f, 0.6f, 0.5f, 0.0f, Mn, Sn);
        float pg = 0; for (float v : Mg) pg = std::max (pg, fabsf (v));
        check (nullDb (Mg, Mn, 4800) > -10.0 && nedAt (Mg, (int) (0.15 * SR)) > 0.5 && pg < 1.0f, "VERB: SHAPE: random lengths, not the sine (difference, dB)", nullDb (Mg, Mn, 4800));
        // FRZ: a trig freezes the tail (the input stops, the tail holds), the next releases it
        R.base (M_FXVERB); R.source (burst (220.0f, 0.8f)); R.e->setParam (1, P_C, 0.3f); R.e->setParam (1, P_XE, 0.5f); R.play();
        std::vector<float> Mz, Sz; R.render (4.0, Mz, Sz, [&] (int b) { if (b == (int) (0.7 * SR / 128)) R.play (1); if (b == (int) (3.0 * SR / 128)) R.play (1); });
        const double held = eDb (Mz, (int) (2.4 * SR), (int) (2.9 * SR)) - eDb (Mz, (int) (0.75 * SR), (int) (0.95 * SR));
        const double gone = eDb (Mz, (int) (3.7 * SR), (int) (4.0 * SR)) - eDb (Mz, (int) (2.4 * SR), (int) (2.9 * SR));
        check (held > -6.0 && gone < -12.0, "VERB: FRZ holds the tail (2 s later, dB), the next trig lets it go", held);
        // REV: the pre-delay played backwards while the gesture lasts (a hit swells in instead of starting)
        auto revRun = [&] (bool rev, std::vector<float>& M2, std::vector<float>& S2) {
            std::vector<float> pl ((size_t) SR); for (int i = 0; i < (int) (0.3 * SR); ++i) pl[(size_t) (i + 200)] = 0.6f * (float) (sin (2 * M_PI * 440.0 * i / SR) * exp (-i / (0.05 * SR)));
            R.base (M_FXVERB); R.source (pl); R.e->setParam (1, P_C, 0.2f); R.e->setParam (1, P_E, 1.0f); R.e->setParam (1, P_XC, 0.5f);
            R.e->setParam (1, P_XE, rev ? 0.9f : 0.0f); R.play(); if (rev) R.play (1);
            M2.clear(); S2.clear(); R.render (2.0, M2, S2); };
        std::vector<float> Mr, Sr, Mw, Sw; revRun (true, Mr, Sr); revRun (false, Mw, Sw);
        float pr = 0; for (float v : Mr) pr = std::max (pr, fabsf (v));
        check (nullDb (Mr, Mw, 4800) > -6.0 && eDb (Mr, 4800, (int) Mr.size()) > -50.0 && pr < 1.0f, "VERB: REV: the pre-delay backwards (difference against forward, dB)", nullDb (Mr, Mw, 4800));
    }
    // 5. ECHO: its time, its repeats, SYNC, a run-away held, glides, HOLD, the plucked zone
    {
        const float r04 = (float) (log (0.4 / 0.25) / log (1.6 / 0.25));
        R.base (M_FXECHO); R.source (burst (500.0f, 0.02f)); R.e->setParam (1, P_B, (2 + 0.5f) / 4.0f); R.e->setParam (1, P_C, r04); R.e->setParam (1, P_D, 0.5f); R.e->setParam (1, P_E, 0.5f); R.play();
        std::vector<float> M, S; R.render (2.0, M, S);
        // the first echo's peak, and the next one's
        auto peakIn = [&] (const std::vector<float>& x, int a, int b) { int at = a; float m = 0; for (int i = a; i < b; ++i) if (fabsf (x[(size_t) i]) > m) { m = fabsf (x[(size_t) i]); at = i; } return at; };
        const int p0 = peakIn (M, 0, 4000), p1 = peakIn (M, p0 + 4000, p0 + 30000), p2 = peakIn (M, p1 + 4000, p1 + 30000);
        check (fabs ((p1 - p0) / 48.0 - 400.0) < 1.0, "ECHO: RATE set for 400 ms (the first echo, ms)", (p1 - p0) / 48.0);
        const double rr = eDb (M, p2 - 600, p2 + 600) - eDb (M, p1 - 600, p1 + 600), want = 20 * log10 (0.55);
        check (fabs (rr - want) < 1.5, "ECHO: REPT 50 %: each repeat 5.2 dB down (dB)", rr);
        R.base (M_FXECHO, 120.0f); R.source (burst (500.0f, 0.02f)); R.e->setParam (1, P_B, (2 + 0.5f) / 4.0f); R.e->setParam (1, P_C, 0.03f);
        R.e->setParam (1, P_XD, 0.75f); R.e->setParam (1, P_D, 0.3f); R.e->setParam (1, P_E, 0.5f); R.play();
        std::vector<float> Ms, Ss; R.render (1.0, Ms, Ss);
        const int q0 = peakIn (Ms, 0, 4000), q1 = peakIn (Ms, q0 + 4000, q0 + 30000);
        check (fabs ((q1 - q0) / 48.0 - 250.0) < 1.0, "ECHO: SYNC at 120: an 8th (ms)", (q1 - q0) / 48.0);
        R.base (M_FXECHO); R.source (burst (300.0f, 0.3f)); R.e->setParam (1, P_B, (2 + 0.5f) / 4.0f); R.e->setParam (1, P_C, 0.2f); R.e->setParam (1, P_D, 1.0f); R.play();
        std::vector<float> Mr, Sr; R.render (10.0, Mr, Sr);
        float pk = 0; for (float v : Mr) pk = std::max (pk, fabsf (v));
        check (pk < 1.2f && eDb (Mr, 9 * SR, 10 * SR) > -30.0, "ECHO: REPT at the top runs away, held by its saturation (peak)", pk);
        // RATE moved under a held tone: it slides (no click)
        R.base (M_FXECHO); R.source (sine (220.0f, 5.0f, 0.4f)); R.e->setParam (1, P_B, (1 + 0.5f) / 4.0f); R.e->setParam (1, P_D, 0.4f); R.e->setParam (1, P_E, 0.5f); R.play();
        std::vector<float> Mg, Sg; R.render (4.0, Mg, Sg, [&] (int b) { R.e->setParam (1, P_C, 0.2f + 0.6f * (float) (0.5 - 0.5 * cos (b * 0.01))); });
        R.base (M_FXECHO); R.source (sine (220.0f, 5.0f, 0.4f)); R.e->setParam (1, P_B, (1 + 0.5f) / 4.0f); R.e->setParam (1, P_D, 0.4f); R.e->setParam (1, P_E, 0.5f); R.e->setParam (1, P_C, 0.5f); R.play();
        std::vector<float> Mg2, Sg2; R.render (4.0, Mg2, Sg2);
        check (spikeOf (Mg, 4800) / spikeOf (Mg2, 4800) < 1.6, "ECHO: RATE swept: the time slides like a tape (spike ratio against still)", spikeOf (Mg, 4800) / spikeOf (Mg2, 4800));
        // HOLD: the loop as it is, for as long as it is held
        R.base (M_FXECHO); R.source (noise (1.0f, 0.3f)); R.e->setParam (1, P_B, (2 + 0.5f) / 4.0f); R.e->setParam (1, P_C, 0.3f); R.e->setParam (1, P_D, 0.4f); R.e->setParam (1, P_XE, 0.5f); R.play();
        std::vector<float> Mh, Sh; R.render (5.0, Mh, Sh, [&] (int b) { if (b == (int) (0.9 * SR / 128)) R.play (1); });
        const double hd = eDb (Mh, (int) (3.5 * SR), (int) (4.5 * SR)) - eDb (Mh, (int) (1.5 * SR), (int) (2.5 * SR));
        check (fabs (hd) < 1.5, "ECHO: HOLD: the loop keeps its level (2 s later, dB)", hd);
        // KARP: a pluck at the pitch of its time
        const float rk = (float) (log ((1.0 / 220.0) / 0.0015) / log (0.03 / 0.0015));
        R.base (M_FXECHO); R.source (impulse()); R.e->setParam (1, P_B, 0.5f / 4.0f); R.e->setParam (1, P_C, rk); R.e->setParam (1, P_D, 0.85f); R.play();
        std::vector<float> Mk, Sk; R.render (1.0, Mk, Sk);
        // (the first lag that comes near the best: its period, not a multiple of it)
        std::vector<double> ac (960, 0.0); double bestC = -1;
        for (int L = 48; L < 960; ++L) { double c = 0, a = 0, b = 0; for (int i = 4800; i < 4800 + 9600; ++i) { c += (double) Mk[(size_t) i] * Mk[(size_t) (i + L)]; a += (double) Mk[(size_t) i] * Mk[(size_t) i]; b += (double) Mk[(size_t) (i + L)] * Mk[(size_t) (i + L)]; }
            ac[(size_t) L] = c / sqrt (a * b + 1e-30); bestC = std::max (bestC, ac[(size_t) L]); }
        int bestL = 48; while (bestL < 959 && ! (ac[(size_t) bestL] > 0.9 * bestC && ac[(size_t) bestL] >= ac[(size_t) (bestL + 1)])) ++bestL;
        const double fk = 48000.0 / bestL;
        check (fabs (fk - 220.0) < 3.0 && bestC > 0.8, "ECHO: KARP: a pluck at the pitch of its time, 220 Hz (its period, Hz)", fk);
        // FLIP: backwards, no click
        R.base (M_FXECHO); R.source (drums(), 0, true); R.e->setParam (1, P_B, (2 + 0.5f) / 4.0f); R.e->setParam (1, P_C, 0.2f); R.e->setParam (1, P_D, 0.3f); R.e->setParam (1, P_XE, 0.95f); R.play();
        std::vector<float> Mf, Sf; R.render (3.0, Mf, Sf, [&] (int b) { if (b == 20) R.play (1); });
        R.base (M_FXECHO); R.source (drums(), 0, true); R.e->setParam (1, P_B, (2 + 0.5f) / 4.0f); R.e->setParam (1, P_C, 0.2f); R.e->setParam (1, P_D, 0.3f); R.play();
        std::vector<float> Mn, Sn; R.render (3.0, Mn, Sn);
        check (spikeOf (Mf, 4800) / spikeOf (Mn, 4800) < 1.3 && eDb (Mf, SR, 3 * SR) > -40.0, "ECHO: FLIP plays the repeats backwards, no click (spike ratio)", spikeOf (Mf, 4800) / spikeOf (Mn, 4800));
    }
    // 6. BEND: clean through at rest; repeats; a bend in pitch; no click at its seams
    {
        const std::vector<float> D = drums();
        R.base (M_FXBEND); R.source (D); R.e->setParam (1, P_C, 0.0f); R.e->setParam (1, P_D, 0.0f); R.e->setParam (1, P_XD, 0.75f); R.play();
        std::vector<float> M, S; R.render (3.0, M, S);
        R.base (-1); R.source (D); R.play(); std::vector<float> A, As; R.render (3.0, A, As);
        check (nullDb (M, A, 4800) < -90.0, "BEND at rest: the source as it is (difference, dB)", nullDb (M, A, 4800));
        // RPTS 4 on 8ths at 120 (a division 250 ms): each one is the last one's first 16th... 62.5 ms, four times
        R.base (M_FXBEND); R.source (noise (4.0f, 0.3f)); R.e->setParam (1, P_B, (2 + 0.5f) / 6.0f); R.e->setParam (1, P_C, (3 + 0.5f) / 8.0f); R.e->setParam (1, P_XD, 0.75f);
        R.e->setPlay (1); R.play();
        std::vector<float> Mr, Sr; R.render (3.0, Mr, Sr);
        double c = 0, ca = 0, cb = 0; const int L = 3000;
        for (int i = (int) (1.0 * SR) + 300; i < (int) (1.0 * SR) + 300 + 2 * L; ++i) { c += (double) Mr[(size_t) i] * Mr[(size_t) (i + L)]; ca += (double) Mr[(size_t) i] * Mr[(size_t) i]; cb += (double) Mr[(size_t) (i + L)] * Mr[(size_t) (i + L)]; }
        check (c / sqrt (ca * cb) > 0.9, "BEND: RPTS 4 on an 8th: the same 16th four times (correlation at 62.5 ms)", c / sqrt (ca * cb));
        // BEND up an octave on a 220 Hz sine: 440
        R.base (M_FXBEND); R.source (sine (220.0f, 4.0f)); R.e->setParam (1, P_TUNE, 0.75f); R.e->setParam (1, P_B, (3 + 0.5f) / 6.0f); R.e->setParam (1, P_C, 0.0f); R.e->setParam (1, P_D, 0.0f);
        R.e->setParam (1, P_XD, 0.75f); R.e->setPlay (1); R.play();
        std::vector<float> Mb, Sb; R.render (3.0, Mb, Sb, [&] (int b) { if (getenv ("FXDBG") && b % 50 == 0) { const FxState& f = R.e->fx[1]; printf ("  bend b%d ev %d live %d rate %.2f len %.0f tune %.3f\n", b, f.bEv, f.bCur.live, f.bCur.rate, f.bCur.len, R.e->tr[1].p[P_TUNE]); } });
        const double fb = freqOf (Mb, (int) (1.1 * SR), (int) (1.4 * SR));
        check (fabs (fb - 440.0) < 6.0, "BEND +1 octave on a 220 Hz tone (Hz)", fb);
        // its seams, on a tone (the worst case): repeats, breaks, bends
        R.base (M_FXBEND); R.source (sine (220.0f, 6.0f, 0.4f)); R.e->setParam (1, P_B, (2 + 0.5f) / 6.0f); R.e->setParam (1, P_C, (4 + 0.5f) / 8.0f); R.e->setParam (1, P_D, 0.6f);
        R.e->setParam (1, P_TUNE, 0.6f); R.e->setParam (1, P_XD, 0.75f); R.e->setPlay (1); R.play();
        std::vector<float> Ms, Ss; R.render (5.0, Ms, Ss);
        R.base (M_FXBEND); R.source (sine (220.0f, 6.0f, 0.4f)); R.e->setParam (1, P_C, 0.0f); R.e->setParam (1, P_D, 0.0f); R.e->setParam (1, P_XD, 0.75f); R.e->setPlay (1); R.play();
        std::vector<float> Mq, Sq; R.render (5.0, Mq, Sq);
        // (each sample's curvature against its neighbourhood's, whatever the pitch it is bent to: √2 on a clean
        // tone, up to 14 on a jump; the floor, 2 % of the plain tone's, keeps the near-silences of a stop out)
        const double ck0 = clickOf (Mq, 4800, 0.02 * spikeOf (Mq, 4800)), ck = clickOf (Ms, 4800, 0.02 * spikeOf (Mq, 4800));
        if (getenv ("FXDBG")) printf ("  seams: %.2f (plain %.2f) at %d\n", ck, ck0, clickAt);
        check (ck < 3.0, "BEND: repeats, breaks and bends on a tone: faded seams (worst curvature against its neighbourhood's, x)", ck);
        float pk = 0; for (float v : Ms) pk = std::max (pk, fabsf (v));
        check (pk < 1.0f, "BEND: bounded (peak)", pk);
    }
    // 7. TAPE: nearly clean at rest, wow as set, saturation, the generations' top, a stop and a start, bounded
    {
        R.base (M_FXTAPE); R.source (sine (1000.0f, 3.0f, 0.3f)); R.e->setParam (1, P_XD, (2 + 0.5f) / 4.0f);
        R.e->setParam (1, P_B, 0.0f); R.e->setParam (1, P_C, 0.0f); R.e->setParam (1, P_D, 0.0f); R.e->setParam (1, P_XA, 0.0f); R.e->setParam (1, P_XB, 0.0f); R.e->setParam (1, P_XC, 0.0f); R.play();
        std::vector<float> M, S; R.render (2.0, M, S);
        R.base (-1); R.source (sine (1000.0f, 3.0f, 0.3f)); R.play(); std::vector<float> A, As; R.render (2.0, A, As);
        const double lv = eDb (M, SR, 2 * SR - 2000) - eDb (A, SR, 2 * SR - 2000);
        if (getenv ("FXDBG")) printf ("  tape rest: M %.1f dB  A %.1f dB  (A 0-0.5 s %.1f)\n", eDb (M, SR, 2 * SR - 2000), eDb (A, SR, 2 * SR - 2000), eDb (A, 0, SR / 2));
        check (fabs (lv) < 0.6, "TAPE at rest (REEL): a 1 kHz tone at its level (dB)", lv);
        // wow: the pitch of a 1 kHz tone, measured over 10 ms windows
        R.base (M_FXTAPE); R.source (sine (1000.0f, 8.0f, 0.3f)); R.e->setParam (1, P_XD, 0.5f / 4.0f); R.e->setParam (1, P_B, 0.6f); R.e->setParam (1, P_C, 0.0f); R.e->setParam (1, P_XC, 0.0f); R.play();
        std::vector<float> Mw, Sw; R.render (7.0, Mw, Sw);
        double lo = 1e9, hi = 0;
        for (int a = SR; a + 960 < 7 * SR - 2000; a += 960) { const double fr = freqOf (Mw, a, a + 960); if (fr > 0) { lo = std::min (lo, fr); hi = std::max (hi, fr); } }
        const double cents = 1200 * log2 (hi / lo);
        check (cents > 8.0 && cents < 90.0, "TAPE: WOW 60 % on CASS: the pitch drifts (its swing, cents)", cents);
        // saturation: the third harmonic of a 200 Hz tone at -6 dBFS
        auto h3 = [&] (float sat) {
            R.base (M_FXTAPE); R.source (sine (200.0f, 3.0f, 0.5f)); R.e->setParam (1, P_XD, (2 + 0.5f) / 4.0f); R.e->setParam (1, P_B, 0.0f); R.e->setParam (1, P_C, 0.0f);
            R.e->setParam (1, P_D, sat); R.e->setParam (1, P_XC, 0.0f); R.play();
            std::vector<float> Mh, Sh; R.render (2.0, Mh, Sh);
            const std::vector<double> mg = magSpec (Mh, SR, 32768);
            auto at = [&] (double hz) { const int k = (int) (hz * 32768.0 / 48000.0 + 0.5); double m = 0; for (int j = k - 3; j <= k + 3; ++j) m = std::max (m, mg[(size_t) j]); return m; };
            return 20 * log10 ((at (400) + at (600)) / at (200)); };
        const double t0 = h3 (0.0f), t1 = h3 (1.0f);
        check (t0 < -45.0 && t1 > -26.0, "TAPE: SAT: harmonics from nothing to rich (2nd + 3rd against the tone, SAT 100 %, dB)", t1);
        // the generations: the top goes
        R.base (M_FXTAPE); R.source (noise (3.0f, 0.2f)); R.e->setParam (1, P_XD, 0.5f / 4.0f); R.e->setParam (1, P_XA, 1.0f); R.e->setParam (1, P_B, 0.0f); R.e->setParam (1, P_C, 0.0f); R.e->setParam (1, P_XC, 0.0f); R.play();
        std::vector<float> Mg, Sg; R.render (2.0, Mg, Sg);
        const std::vector<double> sg = magSpec (Mg, SR / 2, 32768);
        auto band = [&] (double a, double b) { double s2 = 0; for (int k = (int) (a * 32768 / 48000); k < (int) (b * 32768 / 48000); ++k) s2 += sg[(size_t) k] * sg[(size_t) k]; return 10 * log10 (s2 / ((b - a) * 32768 / 48000)); };
        const double drop = band (6000, 8000) - band (500, 1000);
        check (drop < -15.0, "TAPE: GEN 100 % on CASS: the top lost (6-8 kHz against 0.5-1 kHz, dB)", drop);
        // STOP: down to nothing, then back on its feet, no click
        R.base (M_FXTAPE); R.source (drums(), 0, true); R.e->setParam (1, P_XE, 0.75f); R.e->setParam (1, P_DEC, 0.55f); R.e->setParam (1, P_XC, 0.0f); R.play();
        std::vector<float> Mt, St; R.render (5.0, Mt, St, [&] (int b) { if (b == (int) (1.0 * SR / 128)) R.play (1); });
        R.base (M_FXTAPE); R.source (drums(), 0, true); R.e->setParam (1, P_XE, 0.75f); R.e->setParam (1, P_DEC, 0.55f); R.e->setParam (1, P_XC, 0.0f); R.play();
        std::vector<float> Mq, Sq; R.render (5.0, Mq, Sq);
        // (DEC 0.55: the gesture lasts 0.46 s, the tape stops over 60 % of it, then waits)
        const double gs = 0.02 * pow (300.0, 0.55);
        const int s0 = (int) ((1.01 + 0.62 * gs) * SR), s1 = (int) ((1.0 + 0.97 * gs) * SR);
        const double tail = eDb (Mt, s0, s1) - eDb (Mq, s0, s1);
        const double back = eDb (Mt, (int) (3.5 * SR), (int) (4.5 * SR)) - eDb (Mq, (int) (3.5 * SR), (int) (4.5 * SR));
        if (getenv ("FXDBG")) printf ("  stop: tail %.1f back %.1f\n", tail, back);
        check (tail < -12.0 && fabs (back) < 3.0, "TAPE: STOP: the tape stops (late in the stop, dB), then plays again at its level", tail);
        // everything up, every machine: bounded
        float pk = 0; bool bad = false;
        for (int md = 0; md < 4; ++md)
        {
            R.base (M_FXTAPE); R.source (drums(), 0, true); R.e->setParam (1, P_XD, (md + 0.5f) / 4.0f);
            for (int p : { P_B, P_C, P_D, P_XA, P_XB, P_XC }) R.e->setParam (1, p, 1.0f);
            R.play(); std::vector<float> Mx, Sx; R.render (3.0, Mx, Sx);
            for (float v : Mx) { pk = std::max (pk, fabsf (v)); if (! std::isfinite (v)) bad = true; }
        }
        check (! bad && pk < 1.2f, "TAPE: every machine, every knob at the top: bounded (peak)", pk);
    }
    // 8. CODEC: transparent at rest (256 samples late), STD + INV = the source, packets, bounded
    {
        const std::vector<float> D = drums();
        auto codec = [&] (float loss, float band, float inv) {
            R.base (M_FXCODEC); R.source (D); R.e->setParam (1, P_B, loss); R.e->setParam (1, P_C, band); R.e->setParam (1, P_D, 0.0f);
            R.e->setParam (1, P_XA, 0.0f); R.e->setParam (1, P_XB, 0.0f); R.e->setParam (1, P_XC, inv); R.e->setParam (1, P_XD, 0.0f); R.play();
            std::vector<float> M, S; R.render (3.0, M, S); return M; };
        R.base (-1); R.source (D); R.play(); std::vector<float> A, As; R.render (3.0, A, As);
        const std::vector<float> clean = codec (0.0f, 1.0f, 0.0f);
        check (nullDb (clean, A, 4800, FX_CFN) < -60.0, "CODEC at rest: the source, 256 samples late (difference, dB)", nullDb (clean, A, 4800, FX_CFN));
        const std::vector<float> stdB = codec (0.0f, 0.35f, 0.0f), invB = codec (0.0f, 0.35f, 1.0f);
        std::vector<float> sum (stdB.size()); for (size_t i = 0; i < sum.size(); ++i) sum[i] = stdB[i] + invB[i];
        check (nullDb (sum, A, 4800, FX_CFN) < -50.0, "CODEC: what it keeps + what it throws away (INV) = the source (difference, dB)", nullDb (sum, A, 4800, FX_CFN));
        const std::vector<float> lossy = codec (0.8f, 0.5f, 0.0f);
        float pk = 0; bool bad = false; for (float v : lossy) { pk = std::max (pk, fabsf (v)); if (! std::isfinite (v)) bad = true; }
        check (! bad && pk < 1.0f && nullDb (lossy, A, 4800, FX_CFN) > -12.0, "CODEC: LOSS 80 %: it is a codec now (difference from the source, dB)", nullDb (lossy, A, 4800, FX_CFN));
    }
    // 9a. stereo: the sources come in as they would reach the mix (their pans), the effects keep their sides apart;
    // BASS, SHFT (VERB), PING and DUCK (ECHO), WIDE (TAPE), JOIN (CODEC)
    {
        auto LR = [] (const std::vector<float>& M, const std::vector<float>& S, std::vector<float>& L, std::vector<float>& Rr) {
            L.resize (M.size()); Rr.resize (M.size()); for (size_t i = 0; i < M.size(); ++i) { L[i] = M[i] + S[i]; Rr[i] = M[i] - S[i]; } };
        // T1 hard left, T2 hard right, the FX on T3 (SUM): MIX 0 gives the same stereo back
        auto two = [&] (int m, const std::vector<float>& a, const std::vector<float>& b, std::function<void()> set, std::vector<float>& M, std::vector<float>& S, double secs = 2.0) {
            R.base (-1); R.source (a, 0); R.source (b, 1);
            R.e->setParam (1, P_MACH, machineVal (M_RAM)); R.e->setParam (1, P_VOL, 0.8f); R.e->setParam (1, P_A, 0.0f); R.e->setParam (1, P_B, 1.0f);
            R.e->setParam (1, P_C, 0.1f); R.e->setParam (1, P_DEC, 1.0f); R.e->setParam (1, P_E, 0.05f); R.e->setParam (1, P_D, 0.0f);
            R.e->setParam (0, P_PAN, 0.0f); R.e->setParam (1, P_PAN, 1.0f);
            if (m >= 0)
            {
                R.e->setParam (2, P_MACH, machineVal (m)); R.e->setParam (2, P_VOL, 0.8f); R.e->setParam (2, P_A, 0.15f); R.e->setParam (2, P_E, 1.0f);
                R.e->setParam (2, P_DEC, 1.0f); R.e->setParam (2, P_F, 0.0f); R.e->setParam (2, P_XE, 0.0f);
                if (set) set();
            }
            R.play (0); R.play (1); M.clear(); S.clear(); R.render (secs, M, S); };
        const std::vector<float> NA = noise (2.0f, 0.2f), NB = sine (330.0f, 2.0f, 0.3f);
        std::vector<float> Md, Sd; two (-1, NA, NB, nullptr, Md, Sd);
        double worst = -300.0;
        for (int m : { M_FXVERB, M_FXECHO, M_FXBEND, M_FXTAPE, M_FXCODEC })
        {
            std::vector<float> M0, S0; two (m, NA, NB, [&] { R.e->setParam (2, P_E, 0.0f); }, M0, S0);
            const int lag = m == M_FXCODEC ? FX_CFN : 0;
            worst = std::max (worst, std::max (nullDb (M0, Md, 4800 + lag, lag), nullDb (S0, Sd, 4800 + lag, lag)));
        }
        check (worst < -90.0, "stereo: T1 left, T2 right through each effect, MIX 0: the same stereo back (worst difference, mid or side, dB)", worst);
        // VERB on the left source only: its tail is mostly left (true stereo), with some right
        std::vector<float> Mv, Sv, Lv, Rv;
        two (M_FXVERB, impulse(), std::vector<float> (SR * 2, 0.0f), [&] { R.e->setParam (2, P_B, 0.7f); R.e->setParam (2, P_C, 0.6f); }, Mv, Sv);
        LR (Mv, Sv, Lv, Rv);
        const double lr = eDb (Lv, (int) (0.02 * SR), (int) (0.25 * SR)) - eDb (Rv, (int) (0.02 * SR), (int) (0.25 * SR));
        check (lr > 2.0 && lr < 40.0, "VERB in stereo: a hit on the left rings mostly on the left (early tail, L - R, dB)", lr);
        // BASS: the lows last longer (low band against high band, 1.2 .. 1.8 s, BASS at the top against none)
        auto bands = [&] (float bass, double& lo, double& hi) {
            std::vector<float> M, S; R.base (M_FXVERB); R.source (impulse()); R.e->setParam (1, P_B, 0.75f); R.e->setParam (1, P_C, 0.62f); R.e->setParam (1, P_D, 0.1f); R.e->setParam (1, P_XF, bass); R.play(); R.render (2.5, M, S);
            const std::vector<double> mg = magSpec (M, (int) (1.2 * SR), 32768);
            lo = hi = 1e-30; for (size_t k = 0; k < mg.size(); ++k) { const double hz = k * (double) SR / 32768.0; if (hz > 40 && hz < 180) lo += mg[k] * mg[k]; else if (hz > 800 && hz < 4000) hi += mg[k] * mg[k]; } };
        double lo0, hi0, lo1, hi1; bands (0.0f, lo0, hi0); bands (1.0f, lo1, hi1);
        const double bassUp = 10 * log10 (lo1 / hi1) - 10 * log10 (lo0 / hi0);
        check (bassUp > 6.0, "VERB: BASS: its lows last longer (low against high band late in the tail, dB more)", bassUp);
        // SHFT: +12 lifts the tail, -12 lowers it (its spectrum's centre late on, against none)
        auto centre = [&] (float shf) {
            std::vector<float> M, S; R.base (M_FXVERB); R.source (burst (440.0f, 0.2f)); R.e->setParam (1, P_B, 0.7f); R.e->setParam (1, P_C, 0.7f); R.e->setParam (1, P_D, 0.0f); R.e->setParam (1, P_XG, shf); R.play(); R.render (3.0, M, S);
            const std::vector<double> mg = magSpec (M, (int) (1.5 * SR), 32768);
            double sw = 0, w = 0; for (size_t k = 8; k < mg.size(); ++k) { const double e = mg[k] * mg[k]; sw += e * k; w += e; } return sw / std::max (w, 1e-30); };
        const double c0 = centre (0.0f), cu = centre ((12 + 0.5f) / 13.0f), cd = centre ((1 + 0.5f) / 13.0f);
        if (getenv ("FXDBG")) printf ("  shft centres: off %.0f up %.0f down %.0f (bins)\n", c0, cu, cd);
        check (cu > 1.3 * c0 && cd < c0 / 1.05, "VERB: SHFT +12 lifts the tail, -12 lowers it (its centre against none, x)", cu / c0);
        // ECHO PING: a hit in the middle bounces, left, right, left
        std::vector<float> Me, Se; R.base (M_FXECHO); R.source (impulse()); R.e->setParam (1, P_B, (2 + 0.5f) / 4.0f); R.e->setParam (1, P_C, 0.25f); R.e->setParam (1, P_D, 0.5f);
        R.e->setParam (1, P_XA, 0.0f); R.e->setParam (1, P_XC, 0.0f); R.e->setParam (1, P_XF, 1.0f); R.play(); R.render (2.0, Me, Se);
        std::vector<float> Le, Re; LR (Me, Se, Le, Re);
        const double T = R.e->fx[1].eTime / 1000.0 * SR;
        auto at = [&] (const std::vector<float>& x, int k) { return eDb (x, (int) (200 + k * T - 0.01 * SR), (int) (200 + k * T + 0.01 * SR)); };
        const double p1 = at (Le, 1) - at (Re, 1), p2 = at (Re, 2) - at (Le, 2), p3 = at (Le, 3) - at (Re, 3);
        check (p1 > 20.0 && p2 > 20.0 && p3 > 20.0, "ECHO: PING: the repeats go left, right, left (each side against the other, worst dB)", std::min (p1, std::min (p2, p3)));
        // ECHO DUCK: under a sounding source the repeats step back, then come up when it stops
        auto duckRun = [&] (float dk) {
            std::vector<float> M, S; std::vector<float> src ((size_t) (SR * 3), 0.0f); for (int i = 0; i < SR; ++i) src[(size_t) i] = 0.3f * (float) sin (2 * M_PI * 220.0 * i / SR);
            R.base (M_FXECHO); R.source (src); R.e->setParam (1, P_B, (2 + 0.5f) / 4.0f); R.e->setParam (1, P_C, 0.3f); R.e->setParam (1, P_D, 0.6f); R.e->setParam (1, P_E, 1.0f); R.e->setParam (1, P_XG, dk); R.play(); R.render (3.0, M, S);
            return std::make_pair (eDb (M, (int) (0.6 * SR), (int) (0.95 * SR)), eDb (M, (int) (1.4 * SR), (int) (1.8 * SR))); };
        const auto d0 = duckRun (0.0f), d1 = duckRun (1.0f);
        if (getenv ("FXDBG")) printf ("  duck: none %.1f / %.1f, full %.1f / %.1f\n", d0.first, d0.second, d1.first, d1.second);
        check (d1.first - d0.first < -10.0 && d1.second - d0.second > -3.0, "ECHO: DUCK: under the source the repeats step back (dB), after it they come up", d1.first - d0.first);
        // TAPE WIDE: the two tracks wander apart (a centred source: L and R no longer the same)
        auto tapeLR = [&] (float wd) {
            std::vector<float> M, S; R.base (M_FXTAPE); R.source (sine (1000.0f, 3.0f, 0.3f)); R.e->setParam (1, P_B, 0.5f); R.e->setParam (1, P_C, 0.4f); R.e->setParam (1, P_XC, 0.0f); R.e->setParam (1, P_XF, wd); R.play(); R.render (3.0, M, S);
            return eDb (S, (int) (0.5 * SR), (int) (3.0 * SR)) - eDb (M, (int) (0.5 * SR), (int) (3.0 * SR)); };
        const double tw0 = tapeLR (0.0f), tw1 = tapeLR (1.0f);
        if (getenv ("FXDBG")) printf ("  tape wide: %.1f / %.1f\n", tw0, tw1);
        check (tw0 < -40.0 && tw1 > -30.0, "TAPE: WIDE: one transport (no side) or two tracks wandering apart (side against mid, dB)", tw1);
        // CODEC JOIN: the side gets fewer bits (a wide source narrows)
        // (two noises, one each side: as wide as can be)
        std::vector<float> NC (NA.size()); for (size_t i = 0; i < NC.size(); ++i) NC[i] = NA[(i + 7919) % NA.size()];
        auto sideOf = [&] (float jn) {
            std::vector<float> M, S; two (M_FXCODEC, NA, NC, [&] { R.e->setParam (2, P_B, 0.3f); R.e->setParam (2, P_XF, jn); }, M, S);
            return eDb (S, 9600, (int) S.size()) - eDb (M, 9600, (int) M.size()); };
        const double j0 = sideOf (0.0f), j1 = sideOf (1.0f);
        check (j1 < j0 - 6.0, "CODEC: JOIN: the side loses its bits first, the image folds in (side against mid, dB less)", j1 - j0);
    }
    // 9. every machine, random settings and trigs on every step for 3 s: bounded; the CPU of each one
    {
        Rng rr; rr.seed (5);
        float worst = 0; int bad = 0;
        for (int it = 0; it < 40; ++it)
        {
            const int k = it % 5;
            R.base (FXM[k]); R.source (drums(), 0, true);
            for (int p : { P_TUNE, P_DEC, P_B, P_C, P_D, P_E, P_XA, P_XB, P_XC, P_XD, P_XE }) R.e->setParam (1, p, rr.uni());
            for (int st = 0; st < 16; ++st) R.e->setTrig (1, st, rr.uni() < 0.4f ? 1 : 0);
            R.e->setPlay (1); R.play();
            std::vector<float> M, S; R.render (3.0, M, S);
            for (size_t i = 0; i < M.size(); ++i) { if (! std::isfinite (M[i]) || ! std::isfinite (S[i])) ++bad; worst = std::max (worst, std::max (fabsf (M[i] + S[i]), fabsf (M[i] - S[i]))); }
        }
        check (bad == 0 && worst < 1.6f, "every machine, 40 random settings with trigs: bounded (peak)", worst);
        for (int k = 0; k < 5; ++k)
        {
            // (v80: the best of three runs each, the machine's and the base: a shared machine's timing is noisy)
            std::vector<float> l (128), r (128);
            double ms = 1e30, base = 1e30;
            for (int run = 0; run < 3; ++run)
            {
                R.base (FXM[k]); R.source (drums(), 0, true); R.e->setParam (1, P_D, 0.5f); R.e->setParam (1, P_XA, 0.5f); R.e->setParam (1, P_XB, 0.5f); R.play();
                auto c0 = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < SR * 10; i += 128) R.e->process (l.data(), r.data(), 128, nullptr);
                ms = std::min (ms, std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - c0).count());
                R.base (-1); R.source (drums(), 0, true); R.play();
                c0 = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < SR * 10; i += 128) R.e->process (l.data(), r.data(), 128, nullptr);
                base = std::min (base, std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - c0).count());
            }
            char w[96]; snprintf (w, sizeof w, "CPU of %s (%% of real time, native)", FXN[k]);
            check ((ms - base) / 100.0 < 3.0, w, (ms - base) / 100.0);
        }
    }
    printf ("%d failure(s)\n", fails);
    delete e;
    return fails ? 1 : 0;
}

// ----------------------------------------------------------------- v60: HALO's and STRUM's AGE side
// the frequency (Hz) in [lo, hi] where x[a, a + n) is loudest (a Goertzel search, steps of a quarter cent), and its level
static double peakFreq (const std::vector<float>& x, int a, int n, double lo, double hi, double* mag = nullptr)
{
    double best = lo, bm = -1;
    for (double f = lo; f <= hi; f *= 1.000144)
    {
        const double w = 2 * M_PI * f / 48000.0, c = 2 * cos (w);
        double s1 = 0, s2 = 0;
        for (int i = 0; i < n; ++i) { const double h = 0.5 - 0.5 * cos (2 * M_PI * i / n); const double s0 = x[(size_t) (a + i)] * h + c * s1 - s2; s2 = s1; s1 = s0; }
        const double m = s1 * s1 + s2 * s2 - c * s1 * s2;
        if (m > bm) { bm = m; best = f; }
    }
    if (mag) *mag = 10 * log10 (bm + 1e-30);
    return best;
}
static double centsOf (double f, double ref) { return 1200.0 * log2 (f / ref); }
static int agetest()
{
    Engine* e = new Engine(); e->init (48000.f);
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-84s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    const float C3 = 130.8128f;
    // a clean HALO: sines, close, no drift, no air, no bloom, no width; C major, TRI (C E G)
    auto halo = [&] (std::initializer_list<std::pair<int, float>> ps) {
        e->init (48000.f);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setParam (0, P_MACH, machineVal (M_HALO)); e->setParam (0, P_VOL, 0.7f);
        e->setParam (0, P_A, 3.5f / HALO_SHAPES); e->setParam (0, P_B, 0.5f / HALO_PROGS);
        e->setParam (0, P_C, 0.0f); e->setParam (0, P_D, 0.0f); e->setParam (0, P_E, 0.0f);
        e->setParam (0, P_HSWL, 0.2f); e->setParam (0, P_HDRF, 0.0f); e->setParam (0, P_HAIR, 0.0f); e->setParam (0, P_HWID, 0.0f);
        e->setParam (0, P_DEC, 0.3f);
        for (auto& q : ps) e->setParam (0, q.first, q.second);
        e->setScale (2741u);
        e->g[G_MXON] = 0.0f;
        e->setTrig (0, 0, 1); e->setPlay (1);
    };
    auto voice = [&] () -> Voice& { Track& t = e->tr[0]; return t.voice[t.cur]; };
    auto render = [&] (double secs) {
        std::vector<float> l (128), r (128), out;
        for (int i = 0; i < (int) (secs * 48000); i += 128) { e->process (l.data(), r.data(), 128, nullptr); for (int k = 0; k < 128; ++k) out.push_back (0.5f * (l[k] + r[k])); }
        return out;
    };
    // 1. the cards: the furthest out a full card, the closest 4 %, the eight magnitudes all there; each track its own
    {
        float mx = 0, mn = 9; int distinct = 0;
        for (int c = 0; c < HALO_CARDS; ++c) { const float a = fabsf (cardErr (0, c)); mx = std::max (mx, a); mn = std::min (mn, a); }
        for (int c = 0; c < HALO_CARDS; ++c) if (cardErr (0, c) != cardErr (1, c)) ++distinct;
        check (mx == 1.0f && fabsf (mn - 0.04f) < 1e-6f && distinct >= 4, "the cards: one a full card out, the closest 4 %, another track's others (furthest)", mx);
    }
    // 2. DTUN: each note of the chord out by its card (50 cents at the furthest), measured; the chord's notes on cards
    //    of their own; at 0 every note in tune
    {
        for (int on = 0; on < 2; ++on)
        {
            halo ({ { P_GDTN, on ? 1.0f : 0.0f } });
            std::vector<float> x = render (1.6);
            const HaloState& h = voice().hs;
            double worst = 0; u32 cards = 0; int sounding = 0;
            for (int s = 0; s < HALO_SLOTS; ++s)
            {
                const HaloSlot& sl = h.s[s];
                if (sl.stage != 1 && sl.stage != 2) continue;
                ++sounding; cards |= 1u << sl.card;
                const double f = C3 * pow (2.0, sl.pitch / 12.0), want = on ? 50.0 * cardErr (0, sl.card) : 0.0;
                const double got = centsOf (peakFreq (x, 48000 / 2, 48000, f * 0.95, f * 1.05), f);
                worst = std::max (worst, fabs (got - want));
            }
            check (sounding == 3 && __builtin_popcount (cards) == 3 && worst < 1.5, on ? "DTUN 1: each note out by its own card's error (cents off what it should be, worst)"
                                                                                     : "DTUN 0: every note in tune (cents, worst)", worst);
        }
    }
    // 3. VARI: each note's level moves by its card's (dB, worst against what it should be: the same cards on and off)
    {
        double lv[2][3]; float want[3]; int k = 0, cardsOn[3] = { -1, -1, -1 }, cardsOff[3] = { -2, -2, -2 };
        for (int on = 0; on < 2; ++on)
        {
            halo ({ { P_GVAR, on ? 1.0f : 0.0f } });
            std::vector<float> x = render (1.6);
            const HaloState& h = voice().hs;
            k = 0;
            for (int s = 0; s < HALO_SLOTS && k < 3; ++s)
            {
                const HaloSlot& sl = h.s[s];
                if (sl.stage != 1 && sl.stage != 2) continue;
                const double f = C3 * pow (2.0, sl.pitch / 12.0);
                peakFreq (x, 48000 / 2, 48000, f * 0.995, f * 1.005, &lv[on][k]);
                want[k] = (float) (20 * log10 (exp2 (0.6 * cardUnit (0, sl.card, 3))));
                (on ? cardsOn : cardsOff)[k] = sl.card;
                ++k;
            }
        }
        double worst = 0, spread = 0;
        for (int a = 0; a < k; ++a) for (int b = 0; b < k; ++b)
        {
            const double da = lv[1][a] - lv[0][a], db = lv[1][b] - lv[0][b];
            worst = std::max (worst, fabs ((da - db) - (want[a] - want[b]))); spread = std::max (spread, fabs (da - db));
        }
        const bool same = cardsOn[0] == cardsOff[0] && cardsOn[1] == cardsOff[1] && cardsOn[2] == cardsOff[2];
        check (k == 3 && same && worst < 1.0, "VARI 1: each note's level moved by its card's (dB off, worst)", worst);
        printf ("INFO VARI 1: the notes moved %.1f dB apart at most\n", spread);
    }
    // 4. SHEN: a SAW chord's top (6 .. 16 kHz) up, its level kept; AIR: a breath over the sines' chord (6 .. 16 kHz)
    {
        halo ({ { P_D, 0.4f } }); std::vector<float> a = render (1.2);
        halo ({ { P_D, 0.4f }, { P_GSHN, 1.0f } }); std::vector<float> b = render (1.2);
        const double up = bandDb (b, 24000, 6000, 16000) - bandDb (a, 24000, 6000, 16000), lv = eDb (b, 24000, 57600) - eDb (a, 24000, 57600);
        check (up > 5.0, "SHEN 1: a saw chord's top (6 .. 16 kHz) raised (dB)", up);
        check (fabs (lv) < 1.5, "SHEN 1: its level kept (dB)", lv);
        halo ({}); std::vector<float> c = render (1.2);
        halo ({ { P_HAIR, 0.5f } }); std::vector<float> d = render (1.2);
        const double air = bandDb (d, 24000, 6000, 16000) - bandDb (c, 24000, 6000, 16000), alv = eDb (d, 24000, 57600) - eDb (c, 24000, 57600);
        check (air > 20.0, "AIR 0.5: a breath over a chord of sines, 6 .. 16 kHz (dB against none)", air);
        check (alv < 1.5, "AIR 0.5: the chord's level hardly moved (dB)", alv);
    }
    // 5. DROP: dips (10 ms under the median by 6 dB) on a held chord, often at the top; none at 0
    {
        for (int on = 0; on < 2; ++on)
        {
            halo ({ { P_GDRP, on ? 1.0f : 0.0f }, { P_D, 0.4f } });
            std::vector<float> x = render (8.0);
            std::vector<double> w;
            for (int i = 24000; i + 480 <= (int) x.size(); i += 480) w.push_back (eDb (x, i, i + 480));
            std::vector<double> srt = w; std::sort (srt.begin(), srt.end());
            const double med = srt[srt.size() / 2];
            int dips = 0; bool in = false;
            for (double d : w) { const bool lo = d < med - 6.0; if (lo && ! in) ++dips; in = lo; }
            check (on ? dips >= 8 : dips == 0, on ? "DROP 1: dropouts on a held chord, 7.5 s (dips)" : "DROP 0: none (dips)", dips);
        }
    }
    // 6. STRUM: AIR over its strings, DTUN (the bass string out by its own), VARI, all in their range
    {
        auto strum = [&] (std::initializer_list<std::pair<int, float>> ps) {
            e->init (48000.f);
            for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
            e->setParam (0, P_MACH, machineVal (M_STRUM)); e->setParam (0, P_VOL, 0.7f);
            e->setParam (0, P_A, 3.5f / HALO_SHAPES); e->setParam (0, P_B, 0.5f / HALO_PROGS);
            e->setParam (0, P_C, 0.35f); e->setParam (0, P_D, 0.0f); e->setParam (0, P_E, 0.5f / STRUM_DIRS);
            e->setParam (0, P_SHUM, 0.0f); e->setParam (0, P_HWID, 0.0f); e->setParam (0, P_DEC, 0.6f);
            for (auto& q : ps) e->setParam (0, q.first, q.second);
            e->setScale (2741u); e->g[G_MXON] = 0.0f;
            e->setTrig (0, 0, 1); e->setPlay (1);
        };
        strum ({}); std::vector<float> a = render (1.0);
        strum ({ { P_SAIR, 0.5f } }); std::vector<float> b = render (1.0);
        const double up = bandDb (b, 2400, 5000, 12000) - bandDb (a, 2400, 5000, 12000);
        check (up > 6.0, "STRUM AIR 0.5: the pick's breath and the strings' air, 5 .. 12 kHz (dB against none)", up);
        strum ({ { P_GDTN, 1.0f } }); std::vector<float> c = render (1.0);
        const StrumState& ss = voice().ss;
        int bv = -1; for (int k = 0; k < STRUM_VOICES; ++k) if (ss.s[k].on && ss.s[k].unit == 0) bv = k;
        double off = 99, want = 50.0 * cardErr (0, 0);
        if (bv >= 0)
        {
            const double f = C3 * pow (2.0, ss.s[bv].pitch / 12.0);
            off = centsOf (peakFreq (c, 4800, 19200, f * 0.95, f * 1.05), f) - want;
        }
        check (bv >= 0 && fabs (off) < 3.0, "STRUM DTUN 1: the bass string out by its own error (cents off it)", off);
        strum ({ { P_GVAR, 1.0f } }); std::vector<float> d = render (1.0);
        const double dl = eDb (d, 0, 48000) - eDb (a, 0, 48000);
        check (nullDb (d, a, 0) > -30.0 && fabs (dl) < 4.0, "STRUM VARI 1: the strings unequal, the level in its range (dB)", dl);
    }
    // 7. everything at its top together: finite, under the rails
    {
        halo ({ { P_D, 0.6f }, { P_HAIR, 1.0f }, { P_GDTN, 1.0f }, { P_GVAR, 1.0f }, { P_GSHN, 1.0f }, { P_GDRP, 1.0f }, { P_HUNI, 1.0f } });
        std::vector<float> x = render (3.0);
        float pk = 0; bool bad = false; for (float s : x) { pk = std::max (pk, fabsf (s)); bad |= ! std::isfinite (s); }
        check (! bad && pk < 1.2f, "HALO, AIR and AGE at their tops: finite, under the rails (peak)", pk);
        // what they cost: the whole engine with that HALO, 10 s, against the same chord with AIR and AGE at rest
        auto cost = [&] (std::initializer_list<std::pair<int, float>> ps) {
            halo (ps); auto t0 = std::chrono::high_resolution_clock::now(); render (10.0);
            return std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count() * 10.0; };
        const double c0 = cost ({ { P_D, 0.6f }, { P_HUNI, 1.0f } }), c1 = cost ({ { P_D, 0.6f }, { P_HUNI, 1.0f }, { P_HAIR, 1.0f }, { P_GDTN, 1.0f }, { P_GVAR, 1.0f }, { P_GSHN, 1.0f }, { P_GDRP, 1.0f } });
        printf ("INFO the engine with one HALO track (a triad, saws, UNIS): %.1f %% of real time; AIR and AGE at their tops: %.1f %%\n", c0, c1);
    }
    printf ("%d failure(s)\n", fails);
    if (! fails) printf ("AGE OK\n");
    delete e;
    return fails ? 1 : 0;
}

// ----------------------------------------------------------------- v62: ALLOY (metallic sounds)
// one voice of a machine, its own output (no track strip): its side pages at their defaults, then the settings; a gate
// open for gateSecs (0: none, a plain hit)
static std::vector<float> oneVoice (Engine* e, int m, std::initializer_list<std::pair<int, float>> ps, float secs, float gateSecs = 0.0f,
                                    float note = 0.0f, float vel = 0.9f, unsigned seed = 12345u)
{
    static Voice v; memset ((void*) &v, 0, sizeof v);
    float P[NUM_PARAMS]; for (int p = 0; p < NUM_PARAMS; ++p) P[p] = e->tr[0].p[p];
    float sd[NUM_SIDES]; const int ns = machineSideDefaults (m, sd); for (int q = 0; q < ns; ++q) P[sideParam (q)] = sd[q];
    const int na = machineAFDefaults (m, sd); for (int q = 0; q < na; ++q) P[P_A + q] = sd[q];
    P[P_MACH] = machineVal (m); P[P_TUNE] = 0.5f;
    if (m == M_ALLOY) P[PA_ORGN] = 0.0f;          // (ALLOY: every hit the same, unless a test asks for its life)
    for (auto& q : ps) P[q.first] = q.second;
    v.active = 1; v.machine = m; v.vel = vel; v.note = note; v.quant = 0; v.ti = 0;
    v.penv = v.menv = v.nenv = v.cenv = v.senv = v.ienv = v.i3env = 1.0f; v.rng.seed (seed);
    v.holdSec = gateSecs; v.aenv = 0.0f; v.astage = 0; v.aHoldLeft = gateSecs * 48000.0f;
    e->machineTrigger (v, P);
    std::vector<float> out; float buf[CHUNK];
    for (int i = 0; i < (int) (secs * 48000); i += CHUNK) { e->machineRender (v, P, CHUNK, buf); out.insert (out.end(), buf, buf + CHUNK); }
    return out;
}
static float peakOf (const std::vector<float>& x) { float pk = 0; for (float s : x) { if (! std::isfinite (s)) return 99.0f; pk = std::max (pk, fabsf (s)); } return pk; }
static double lastAbove (const std::vector<float>& x, float th) { int l = 0; for (int i = 0; i < (int) x.size(); ++i) if (fabsf (x[i]) > th) l = i; return l / 48000.0; }

// (a pitch: zero crossings over a window, for a pure tone; not from the very first samples: the filters' start)
static double zcPitch (const std::vector<float>& x, int a, int n)
{
    int first = -1, last = -1, cnt = 0;
    for (int i = a + 1; i < a + n && i < (int) x.size(); ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) { if (first < 0) first = i; last = i; ++cnt; }
    return cnt > 1 ? (cnt - 1) * 48000.0 / (last - first) : 0.0;
}

static int alloytest()
{
    Engine* e = new Engine(); e->init (48000.f);
    if (! REF) { REF = new Engine(); REF->init (48000.f); }
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-100s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    auto shp = [] (int k) { return (k + 0.5f) / AL_SHAPES; };
    static const char* SN[AL_SHAPES] = { "WIRE", "BAR", "TUBE", "PLATE", "DISC", "BELL", "GONG", "BOWL" };
    const double C3 = 130.8128;
    // (the FM's carrier alone: a sine on the note)
    const std::pair<int, float> SINE[] = { { PA_OBJ, 0.0f }, { PA_FM, 1.0f }, { PA_INDX, 0.0f }, { PA_PENV, 0.5f }, { PA_CLIK, 0.0f } };
    // 1. every shape at three sizes: finite, under 0 dB, it ends; their levels close to one another
    {
        float worst = 0; int bad = 0; double longest = 0, lo = 1e9, hi = -1e9; int loK = 0, hiK = 0;
        for (int k = 0; k < AL_SHAPES; ++k)
            for (float sz : { 0.0f, 0.5f, 1.0f })
            {
                const auto x = oneVoice (e, M_ALLOY, { { P_A, shp (k) }, { P_D, sz }, { P_DEC, 0.5f }, { PA_ORGN, 0.0f } }, 3.0f);
                const float pk = peakOf (x); if (pk > 50) ++bad; worst = std::max (worst, pk);
                longest = std::max (longest, lastAbove (x, 0.001f));
                if (sz == 0.5f) { const double r = eDb (x, 0, 24000); if (r < lo) { lo = r; loK = k; } if (r > hi) { hi = r; hiK = k; } }
            }
        check (! bad && worst < 1.0f, "ALLOY: 8 shapes x 3 sizes, finite, peak under 0 dB (peak)", worst);
        check (longest < 2.9, "ALLOY: at DEC 0.5, each has ended (-60 dB) within 3 s (s)", longest);
        char w[128]; snprintf (w, sizeof w, "ALLOY: the shapes' levels, first 0.5 s, within 9 dB (quietest %s, loudest %s; dB)", SN[loK], SN[hiK]);
        check (hi - lo < 9.0, w, hi - lo);
    }
    // 2. on its note: each shape's mode on the note (BELL: its prime, the hum an octave down) at C3, TUNE centre
    {
        double worst = 0; int wk = 0;
        for (int k : { 0, 1, 3, 4, 5, 6, 7 })
        {
            const auto x = oneVoice (e, M_ALLOY, { { P_A, shp (k) }, { P_E, 0.1f }, { P_DEC, 0.7f }, { PA_PENV, 0.5f }, { PA_ORGN, 0.0f } }, 1.0f);
            const double f = peakFreq (x, 2400, 16384, C3 * 0.97, C3 * 1.03);
            if (fabs (centsOf (f, C3)) > fabs (worst)) { worst = centsOf (f, C3); wk = k; }
        }
        char w[128]; snprintf (w, sizeof w, "ALLOY: every shape on its note, C3 at TUNE centre (worst: %s, cents)", SN[wk]);
        check (fabs (worst) < 5.0, w, worst);
    }
    // 3. STIF: as built, the wire's second mode on the octave; harder, stretched away (its exponent 2^(0.3 x 1.3))
    {
        const auto nat = oneVoice (e, M_ALLOY, { { P_A, shp (0) }, { P_DEC, 0.7f }, { PA_PENV, 0.5f }, { PA_ORGN, 0.0f } }, 1.0f);
        const auto hrd = oneVoice (e, M_ALLOY, { { P_A, shp (0) }, { P_B, 0.8f }, { P_DEC, 0.7f }, { PA_PENV, 0.5f }, { PA_ORGN, 0.0f } }, 1.0f);
        const double f2 = peakFreq (nat, 2400, 16384, 2 * C3 * 0.97, 2 * C3 * 1.03), want = C3 * pow (2.0, pow (2.0, 0.3 * 1.3));
        const double f2h = peakFreq (hrd, 2400, 16384, want * 0.97, want * 1.03);
        check (fabs (centsOf (f2, 2 * C3)) < 3.0 && fabs (centsOf (f2h, want)) < 5.0, "ALLOY STIF: the wire's 2nd mode on the octave as built, stretched where it should be harder (cents off)", fabs (centsOf (f2h, want)));
    }
    // 4. MASS: a dense bar keeps its highs, a light one loses them (2 .. 8 kHz at 0.4 s, against the whole)
    {
        const auto lt = oneVoice (e, M_ALLOY, { { P_A, shp (1) }, { P_C, 0.1f }, { P_DEC, 0.6f }, { PA_MALT, 0.9f }, { PA_PENV, 0.5f } }, 1.0f);
        const auto dn = oneVoice (e, M_ALLOY, { { P_A, shp (1) }, { P_C, 0.95f }, { P_DEC, 0.6f }, { PA_MALT, 0.9f }, { PA_PENV, 0.5f } }, 1.0f);
        const double hl = bandDb (lt, 19200, 2000, 8000) - eDb (lt, 19200, 27392), hd = bandDb (dn, 19200, 2000, 8000) - eDb (dn, 19200, 27392);
        check (hd - hl > 15.0, "ALLOY MASS: a dense bar's highs ring on, a light one's die (2 .. 8 kHz at 0.4 s, dB more)", hd - hl);
    }
    // 5. MALT: steel brighter than felt (centroid of the hit); a soft hit darker and quieter
    {
        const auto fl = oneVoice (e, M_ALLOY, { { P_A, shp (3) }, { PA_MALT, 0.05f }, { PA_CLIK, 0.0f }, { PA_PENV, 0.5f } }, 0.5f);
        const auto st = oneVoice (e, M_ALLOY, { { P_A, shp (3) }, { PA_MALT, 1.0f }, { PA_CLIK, 0.0f }, { PA_PENV, 0.5f } }, 0.5f);
        const double r = centroid (st, 0) / centroid (fl, 0);
        check (r > 2.0, "ALLOY MALT: steel against felt, the hit's centroid (times)", r);
        const auto sf = oneVoice (e, M_ALLOY, { { P_A, shp (3) }, { PA_MALT, 0.6f }, { PA_CLIK, 0.0f }, { PA_PENV, 0.5f }, { PA_ORGN, 0.0f } }, 0.5f, 0.0f, 0.0f, 0.25f);
        const auto hd = oneVoice (e, M_ALLOY, { { P_A, shp (3) }, { PA_MALT, 0.6f }, { PA_CLIK, 0.0f }, { PA_PENV, 0.5f }, { PA_ORGN, 0.0f } }, 0.5f, 0.0f, 0.0f, 1.0f);
        check (centroid (sf, 0) < centroid (hd, 0) && eDb (sf, 0, 24000) < eDb (hd, 0, 24000), "ALLOY: a soft hit darker than a hard one (centroid, Hz less)", centroid (hd, 0) - centroid (sf, 0));
    }
    // 6. PENV: +24 st in 6 ms, the hit far above; at 0.3 s on its note; -36 st: the sine three octaves under at the hit
    {
        const auto x = oneVoice (e, M_ALLOY, { { P_A, shp (0) }, { P_DEC, 0.7f }, { PA_PENV, 0.75f }, { PA_PTIM, 0.25f }, { PA_ORGN, 0.0f } }, 1.0f);
        const double f = peakFreq (x, 14400, 16384, C3 * 0.97, C3 * 1.03);
        const double hiE = bandDb (x, 0, 4 * C3 * 0.9, 4 * C3 * 1.1) - bandDb (x, 14400, 4 * C3 * 0.9, 4 * C3 * 1.1);
        check (fabs (centsOf (f, C3)) < 5.0 && hiE > 3.0, "ALLOY PENV: +24 st at the hit (two octaves up there, dB more than later), on its note at 0.3 s", centsOf (f, C3));
        const auto y = oneVoice (e, M_ALLOY, { SINE[0], SINE[1], SINE[2], SINE[4], { PA_PENV, 0.0f }, { PA_PTIM, 1.0f }, { P_TUNE, 1.0f }, { P_DEC, 0.7f } }, 0.6f);
        const double f0 = zcPitch (y, 96, 2400), note = 4 * C3;
        check (f0 > note / 8 && f0 < note / 5.6, "ALLOY PENV: -36 st, the sine more than 2.5 octaves under its note in the first 50 ms (Hz)", f0);
    }
    // 7. BNCE: to the right the mallet strikes again, sooner and softer (re-attacks in the level, 10 ms frames); to the
    // left the flams: four strikes within 50 ms (2.5 ms frames)
    {
        const auto x = oneVoice (e, M_ALLOY, { { P_A, shp (3) }, { P_DEC, 0.6f }, { PA_BNCE, 0.85f }, { PA_PENV, 0.5f }, { PA_ORGN, 0.0f } }, 1.5f);
        // (the strikes stand out in the highs: the level of the first difference, 10 ms frames)
        std::vector<float> d (x.size()); for (size_t i = 1; i < x.size(); ++i) d[i] = x[i] - x[i - 1];
        int hits = 0; double prev = 1e9;
        for (int f = 2; f < 140; ++f) { const double l = eDb (d, f * 480, f * 480 + 480); if (l > prev + 2.0) ++hits; prev = l; }
        check (hits >= 3, "ALLOY BNCE: three or more bounces after the hit (re-attacks)", hits);
        const auto c = oneVoice (e, M_ALLOY, { { PA_OBJ, 0.0f }, { PA_HISS, 1.0f }, { PA_NCOL, 0.7f }, { PA_NLEN, 0.4f }, { PA_BNCE, 0.0f }, { PA_ORGN, 0.0f }, { PA_CLIK, 0.0f } }, 0.3f);
        int fl = 0; prev = 1e9;
        for (int f = 1; f < 24; ++f) { const double l = eDb (c, f * 120, f * 120 + 120); if (l > prev + 3.0) ++fl; prev = l; }
        check (fl >= 3, "ALLOY BNCE: to the left, a clap: three more strikes within 60 ms (re-attacks)", fl);
    }
    // 8. NOIS held (a bow): the plate rings on while its gate is open, the damper brings it down after
    {
        const auto x = oneVoice (e, M_ALLOY, { { P_A, shp (3) }, { P_DEC, 1.0f }, { PA_NOIS, 1.0f }, { PA_NLEN, 1.0f } }, 2.5f, 1.5f);
        const double a = eDb (x, 19200, 28800), b = eDb (x, 57600, 67200), c = eDb (x, 96000, 105600);
        check (fabs (a - b) < 3.0 && b - c > 30.0, "ALLOY NOIS HELD: a bowed plate held while the gate is open (dB apart), down 30 dB after it", fabs (a - b));
    }
    // 9. FM: on RATI √2 the carrier's sidebands land between the harmonics (1 + √2); deep, high, finite and under 0 dB
    {
        const auto x = oneVoice (e, M_ALLOY, { { PA_OBJ, 0.0f }, { PA_FM, 1.0f }, { PA_RATI, 0.3f }, { PA_INDX, 0.5f }, { PA_IDEC, 0.8f }, { P_DEC, 0.7f } }, 1.0f);
        double m1, mS; const double sb = C3 * (1.0 + sqrt (2.0));
        peakFreq (x, 2400, 16384, C3 * 0.98, C3 * 1.02, &m1); const double fs = peakFreq (x, 2400, 16384, sb * 0.98, sb * 1.02, &mS);
        const auto y = oneVoice (e, M_ALLOY, { { P_TUNE, 1.0f }, { PA_OBJ, 0.0f }, { PA_FM, 1.0f }, { PA_RATI, 1.0f }, { PA_INDX, 1.0f }, { PA_IDEC, 1.0f }, { PA_PENV, 1.0f }, { PA_MENV, 1.0f } }, 1.0f);
        check (fabs (centsOf (fs, sb)) < 5.0 && mS > m1 - 30.0 && peakOf (y) < 1.0f, "ALLOY FM: RATI √2, its sideband at 1 + √2 times the note (cents); at its deepest and highest, under 0 dB", centsOf (fs, sb));
    }
    // 10. ADD: the sines around BAND (x 8: their centroid within an octave of 8 times the note); SIZE 0: one sine, on it
    {
        const float bv = (float) (log2 (8.0 / 2.0) / 4.585);
        const auto x = oneVoice (e, M_ALLOY, { { PA_OBJ, 0.0f }, { PA_ADD, 1.0f }, { PA_BAND, bv }, { P_DEC, 0.7f } }, 0.5f);
        const double c = centroid (x, 2400) / (8 * C3);
        check (c > 0.5 && c < 2.0, "ALLOY ADD: the sines around BAND x 8 (their centroid against 8 times the note)", c);
        const auto y = oneVoice (e, M_ALLOY, { { PA_OBJ, 0.0f }, { PA_ADD, 1.0f }, { PA_BAND, bv }, { P_D, 0.0f }, { PA_PENV, 0.5f }, { P_DEC, 0.7f }, { PA_ORGN, 0.0f } }, 0.5f);
        const double f = zcPitch (y, 2400, 4800);
        check (fabs (centsOf (f, 8 * C3)) < 10.0, "ALLOY ADD: at SIZE 0 one sine, on BAND (x 8: cents off)", centsOf (f, 8 * C3));
    }
    // 11. COMB: on the note, feedback +90 %: its harmonics; -90 %: the odd ones (the 3rd over the 2nd)
    {
        const auto al = oneVoice (e, M_ALLOY, { { P_A, shp (3) }, { PA_COMB, 1.0f }, { PA_CFB, 0.5f + 0.9f / 1.99f }, { P_DEC, 0.6f } }, 1.0f);
        const auto od = oneVoice (e, M_ALLOY, { { P_A, shp (3) }, { PA_COMB, 1.0f }, { PA_CFB, 0.5f - 0.9f / 1.99f }, { P_DEC, 0.6f } }, 1.0f);
        double a2, a3, o2, o3;
        peakFreq (al, 9600, 16384, 2 * C3 * 0.99, 2 * C3 * 1.01, &a2); peakFreq (al, 9600, 16384, 3 * C3 * 0.99, 3 * C3 * 1.01, &a3);
        peakFreq (od, 9600, 16384, 2 * C3 * 0.99, 2 * C3 * 1.01, &o2); peakFreq (od, 9600, 16384, 3 * C3 * 0.99, 3 * C3 * 1.01, &o3);
        check ((o3 - o2) - (a3 - a2) > 15.0, "ALLOY COMB: under 0 its odd harmonics (3rd over 2nd, dB more than over 0)", (o3 - o2) - (a3 - a2));
    }
    // 12. DAC: 8 bits at 11 kHz: the bar's tail turns to grain (8 .. 16 kHz at 1 s, against the clean one)
    {
        const auto cl = oneVoice (e, M_ALLOY, { { P_A, shp (1) }, { P_DEC, 0.7f } }, 1.5f);
        const auto dg = oneVoice (e, M_ALLOY, { { P_A, shp (1) }, { P_DEC, 0.7f }, { PA_DAC, 6.5f / 8 } }, 1.5f);
        const double d = bandDb (dg, 48000, 8000, 16000) - bandDb (cl, 48000, 8000, 16000);
        check (d > 20.0, "ALLOY DAC: 8 bits at 11 kHz, the tail's grain over 8 kHz (dB more than clean)", d);
    }
    // 13. GONG: its swell: at 1 s, 3 .. 10 kHz against the whole, higher than a disc's (no swell)
    {
        const auto gn = oneVoice (e, M_ALLOY, { { P_A, shp (6) }, { P_D, 1.0f }, { P_C, 0.9f }, { P_DEC, 0.8f }, { PA_MALT, 0.3f } }, 1.5f, 0.0f, -12.0f);
        const auto ds = oneVoice (e, M_ALLOY, { { P_A, shp (4) }, { P_D, 1.0f }, { P_C, 0.9f }, { P_DEC, 0.8f }, { PA_MALT, 0.3f } }, 1.5f, 0.0f, -12.0f);
        const double g = bandDb (gn, 48000, 3000, 10000) - eDb (gn, 48000, 56192), d = bandDb (ds, 48000, 3000, 10000) - eDb (ds, 48000, 56192);
        check (g - d > 10.0, "ALLOY GONG: its swell, 3 .. 10 kHz at 1 s against the whole (dB more than a disc)", g - d);
    }
    // 14. HISS: heard as it is (the object silent): there, and NCOL moves it (bright against dark, its centroid)
    {
        const auto dk = oneVoice (e, M_ALLOY, { { PA_OBJ, 0.0f }, { PA_HISS, 1.0f }, { PA_NCOL, 0.2f }, { PA_NLEN, 0.7f }, { PA_CLIK, 0.0f } }, 0.4f);
        const auto br = oneVoice (e, M_ALLOY, { { PA_OBJ, 0.0f }, { PA_HISS, 1.0f }, { PA_NCOL, 0.9f }, { PA_NLEN, 0.7f }, { PA_CLIK, 0.0f } }, 0.4f);
        const double r = centroid (br, 2400) / centroid (dk, 2400), ld = eDb (dk, 0, 9600) - eDb (br, 0, 9600);
        check (r > 6.0 && fabs (ld) < 4.0 && eDb (br, 0, 9600) > -30.0, "ALLOY HISS: heard as it is, NCOL dark .. thin (centroid times), the level kept (dB apart under 4)", r);
    }
    // 15. PRIS: the pitch at the hit reached after a rise: an arc (the sine low at the hit, high after it, on the note at 0.4 s)
    {
        const auto x = oneVoice (e, M_ALLOY, { SINE[0], SINE[1], SINE[2], SINE[4], { PA_PENV, 0.75f }, { PA_PRIS, (float) (log2 (30.0) / 8.6) },
            { PA_PTIM, (float) (log2 (80.0) / 7.64) }, { P_DEC, 0.7f } }, 0.6f);
        double b = 0; for (int w = 960; w <= 7200; w += 480) b = std::max (b, zcPitch (x, w, 960));
        const double a = zcPitch (x, 96, 960), c = zcPitch (x, 19200, 2400);
        check (a > 0 && b > a * 1.3 && b > c * 1.3 && fabs (centsOf (c, C3)) < 30.0, "ALLOY PRIS: an arc (its top against the hit's pitch, times)", b / a);
    }
    // 16. MENV: the modulator swept down to its ratio (+3 oct): the FM brighter at the hit than with no sweep
    {
        const auto sw = oneVoice (e, M_ALLOY, { { PA_OBJ, 0.0f }, { PA_FM, 1.0f }, { PA_RATI, 0.3f }, { PA_INDX, 0.4f }, { PA_IDEC, 1.0f }, { PA_MENV, 1.0f }, { PA_PTIM, 0.8f }, { PA_PENV, 0.5f } }, 0.5f);
        const auto no = oneVoice (e, M_ALLOY, { { PA_OBJ, 0.0f }, { PA_FM, 1.0f }, { PA_RATI, 0.3f }, { PA_INDX, 0.4f }, { PA_IDEC, 1.0f }, { PA_MENV, 0.5f }, { PA_PTIM, 0.8f }, { PA_PENV, 0.5f } }, 0.5f);
        const double r = centroid (sw, 0) / centroid (no, 0), late = centroid (sw, 19200) / centroid (no, 19200);
        check (r > 1.4 && fabs (late - 1.0) < 0.15, "ALLOY MENV: the ratio swept from three octaves up: brighter at the hit (times), as it was later", r);
    }
    // 17. IATK: the FM's depth rising (300 ms): darker at the hit than at 0.3 s
    {
        const auto x = oneVoice (e, M_ALLOY, { { PA_OBJ, 0.0f }, { PA_FM, 1.0f }, { PA_RATI, 0.3f }, { PA_INDX, 0.6f }, { PA_IDEC, 1.0f }, { PA_IATK, (float) (log2 (300.0) / 11) }, { PA_PENV, 0.5f }, { P_DEC, 0.8f } }, 0.6f);
        const double r = centroid (x, 14400) / centroid (x, 0);
        check (r > 1.5, "ALLOY IATK: the FM's depth rising, its centroid at 0.3 s against the hit (times)", r);
    }
    // 18. FDBK: past 3/4 the plate sings on by itself (held: as loud at 1.5 s as at 0.5 s), never past 0 dB; at 0, it decays
    {
        const auto x = oneVoice (e, M_ALLOY, { { P_A, shp (3) }, { P_DEC, 1.0f }, { PA_FDBK, 0.95f }, { PA_MALT, 0.5f } }, 2.0f, 2.0f);
        const auto y = oneVoice (e, M_ALLOY, { { P_A, shp (3) }, { P_DEC, 0.5f }, { PA_FDBK, 0.0f }, { PA_MALT, 0.5f } }, 2.0f);
        const double a = eDb (x, 24000, 33600), b = eDb (x, 72000, 81600), c = eDb (y, 72000, 81600);
        check (fabs (a - b) < 4.0 && b - c > 30.0 && peakOf (x) < 1.0f, "ALLOY FDBK: it sings on by itself (dB apart, 0.5 .. 1.5 s), under 0 dB", fabs (a - b));
        float worst = 0;
        for (int k = 0; k < AL_SHAPES; ++k) worst = std::max (worst, peakOf (oneVoice (e, M_ALLOY, { { P_A, shp (k) }, { P_D, 1.0f }, { P_DEC, 0.98f }, { PA_FDBK, 1.0f }, { PA_PENV, 1.0f } }, 2.0f)));
        check (worst < 1.0f, "ALLOY FDBK: at the top, every shape, finite and under 0 dB (peak)", worst);
    }
    // 19. DRIV: a sine driven: its odd harmonics up (the 3rd against the note), its level kept within 6 dB
    {
        const auto cl = oneVoice (e, M_ALLOY, { SINE[0], SINE[1], SINE[2], SINE[3], { P_DEC, 0.8f } }, 0.5f);
        const auto dr = oneVoice (e, M_ALLOY, { SINE[0], SINE[1], SINE[2], SINE[3], { P_DEC, 0.8f }, { PA_DRIV, 0.8f } }, 0.5f);
        double c1, c3, d1, d3;
        peakFreq (cl, 4800, 16384, C3 * 0.99, C3 * 1.01, &c1); peakFreq (cl, 4800, 16384, 3 * C3 * 0.99, 3 * C3 * 1.01, &c3);
        peakFreq (dr, 4800, 16384, C3 * 0.99, C3 * 1.01, &d1); peakFreq (dr, 4800, 16384, 3 * C3 * 0.99, 3 * C3 * 1.01, &d3);
        check ((d3 - d1) - (c3 - c1) > 25.0 && fabs (eDb (dr, 4800, 21184) - eDb (cl, 4800, 21184)) < 6.0 && peakOf (dr) < 1.0f,
               "ALLOY DRIV: the 3rd harmonic up (dB more against the note), the level kept", (d3 - d1) - (c3 - c1));
    }
    // 20. ORGN: no two hits alike (two seeds, the note's pitch apart), at 0 the same
    {
        const auto a = oneVoice (e, M_ALLOY, { SINE[0], SINE[1], SINE[2], SINE[3], { PA_ORGN, 1.0f }, { P_DEC, 0.8f } }, 0.5f, 0.0f, 0.0f, 0.9f, 11u);
        const auto b = oneVoice (e, M_ALLOY, { SINE[0], SINE[1], SINE[2], SINE[3], { PA_ORGN, 1.0f }, { P_DEC, 0.8f } }, 0.5f, 0.0f, 0.0f, 0.9f, 99u);
        const auto c = oneVoice (e, M_ALLOY, { SINE[0], SINE[1], SINE[2], SINE[3], { PA_ORGN, 0.0f }, { P_DEC, 0.8f } }, 0.5f, 0.0f, 0.0f, 0.9f, 11u);
        const auto d = oneVoice (e, M_ALLOY, { SINE[0], SINE[1], SINE[2], SINE[3], { PA_ORGN, 0.0f }, { P_DEC, 0.8f } }, 0.5f, 0.0f, 0.0f, 0.9f, 99u);
        const double ab = fabs (centsOf (zcPitch (a, 9600, 9600), zcPitch (b, 9600, 9600))), cd = fabs (centsOf (zcPitch (c, 9600, 9600), zcPitch (d, 9600, 9600)));
        check (ab > 2.0 && cd < 0.5, "ALLOY ORGN: two hits a little apart (cents), none at 0", ab);
    }
    // 21. CUT: all of it shut 100 ms after the hit (down 60 dB 60 ms later); without it, it rings on
    {
        const float cv = (float) (log2 (100.0 / 20.0) / 6.2);
        const auto x = oneVoice (e, M_ALLOY, { { P_A, shp (3) }, { P_DEC, 0.7f }, { PA_CUT, cv } }, 0.5f);
        const auto y = oneVoice (e, M_ALLOY, { { P_A, shp (3) }, { P_DEC, 0.7f }, { PA_CUT, 0.0f } }, 0.5f);
        check (eDb (x, 7680, 9600) < eDb (y, 7680, 9600) - 50.0 && eDb (x, 2400, 3600) > eDb (y, 2400, 3600) - 1.0, "ALLOY CUT: shut 100 ms after the hit (dB under the uncut one at 160 ms)", eDb (y, 7680, 9600) - eDb (x, 7680, 9600));
    }
    // 22. SPACE: its own reverb through the track (a metal box: its tail longer than the dry one's, wide); CUT shuts it too
    {
        auto hit = [&] (std::initializer_list<std::pair<int, float>> ps, std::vector<float>& side) {
            softReset (e, 48000.f);
            for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, t == 0 ? 0.8f : 0.0f);
            e->setParam (0, P_MACH, machineVal (M_ALLOY));
            float sd[NUM_SIDES]; const int ns = machineSideDefaults (M_ALLOY, sd); for (int q = 0; q < ns; ++q) e->setParam (0, sideParam (q), sd[q]);
            const int na = machineAFDefaults (M_ALLOY, sd); for (int q = 0; q < na; ++q) e->setParam (0, P_A + q, sd[q]);
            e->setParam (0, P_DEC, 0.3f);
            for (auto& p : ps) e->setParam (0, p.first, p.second);
            e->audition (0, 0.9f, 0.0f);
            std::vector<float> L (128), R (128), out; side.clear();
            for (int i = 0; i < 2 * 48000; i += 128) { e->process (L.data(), R.data(), 128, nullptr); for (int k = 0; k < 128; ++k) { out.push_back (0.5f * (L[k] + R[k])); side.push_back (0.5f * (L[k] - R[k])); } }
            return out;
        };
        std::vector<float> sd0, sd1, sd2;
        const auto dry = hit ({ { PA_VERB, 0.0f } }, sd0);
        const auto box = hit ({ { PA_VERB, 0.8f }, { PA_ROOM, 0.0f }, { PA_DCAY, 0.7f } }, sd1);
        const auto cut = hit ({ { PA_VERB, 0.8f }, { PA_ROOM, 0.0f }, { PA_DCAY, 0.7f }, { PA_CUT, (float) (log2 (150.0 / 20.0) / 6.2) } }, sd2);
        const double td = lastAbove (dry, 0.001f), tb = lastAbove (box, 0.001f), tc = lastAbove (cut, 0.001f);
        check (tb > td + 0.3 && eDb (sd1, 9600, 48000) > eDb (sd0, 9600, 48000) + 20.0, "ALLOY SPACE: a metal box rings on after the dry hit (s longer), wide", tb - td);
        check (tc < 0.4, "ALLOY SPACE: CUT shuts it with the voice (s, the last sound over -60 dB)", tc);
    }
    // 23. 120 random settings (a third at the ends): finite, under 0 dB
    {
        Rng r; r.seed (777); float worst = 0; int bad = 0;
        for (int it = 0; it < 120; ++it)
        {
            auto rv = [&] () { const float u = r.uni(); return it % 3 == 0 ? (u < 0.5f ? 0.0f : 1.0f) : u; };
            static Voice v; memset ((void*) &v, 0, sizeof v);
            float P[NUM_PARAMS]; for (int p = 0; p < NUM_PARAMS; ++p) P[p] = e->tr[0].p[p];
            P[P_MACH] = machineVal (M_ALLOY); P[P_TUNE] = r.uni(); P[P_DEC] = r.uni() * 0.98f;
            for (int p = P_A; p <= P_E; ++p) P[p] = rv();
            for (int p = P_S1; p <= P_S35; ++p) P[p] = rv();
            v.active = 1; v.machine = M_ALLOY; v.vel = r.uni(); v.rng.seed (it + 1); v.penv = v.menv = v.nenv = v.cenv = v.senv = v.ienv = v.i3env = 1.0f;
            e->machineTrigger (v, P);
            float buf[CHUNK];
            for (int i = 0; i < 48000; i += CHUNK) { e->machineRender (v, P, CHUNK, buf); for (int k = 0; k < CHUNK; ++k) { if (! std::isfinite (buf[k])) ++bad; worst = std::max (worst, fabsf (buf[k])); } }
        }
        check (! bad && worst < 1.0f, "ALLOY: 120 random settings (a third at the ends), finite, under 0 dB (peak)", worst);
    }
    // 24. CPU: one voice at its heaviest (48 modes fed back, the noise held and heard, flams, FM swept, 24 sines, the
    // comb swept, the phaser, the drive, the DAC)
    {
        auto t0 = std::chrono::steady_clock::now();
        const auto x = oneVoice (e, M_ALLOY, { { P_A, shp (3) }, { P_D, 1.0f }, { P_DEC, 1.0f }, { PA_NOIS, 0.6f }, { PA_HISS, 0.5f }, { PA_NLEN, 1.0f }, { PA_BNCE, 0.1f },
            { PA_FDBK, 0.9f }, { PA_DRIV, 0.5f }, { PA_ORGN, 0.5f }, { PA_PRIS, 0.3f }, { PA_FM, 0.6f }, { PA_INDX, 1.0f }, { PA_MENV, 0.8f }, { PA_IATK, 0.4f },
            { PA_ADD, 0.6f }, { PA_COMB, 0.6f }, { PA_CFB, 0.95f }, { PA_FLNG, 0.6f }, { PA_PHAS, 0.6f }, { PA_DAC, 4.5f / 8 } }, 4.0f, 4.0f);
        const double pc = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 4.0 * 100.0;
        check (pc < 8.0 && peakOf (x) < 1.0f, "ALLOY: one voice at its heaviest under 8 % of real time (%)", pc);
    }
    delete e;
    printf ("%d failure(s)\n%s\n", fails, fails ? "ALLOY FAILED" : "ALLOY OK");
    return fails ? 1 : 0;
}

// ----------------------------------------------------------------- v64: RPAN (the trig's pan drawn at each hit), SPRD
static int spreadtest()
{
    Engine* e = new Engine(); e->init (48000.f);
    if (! REF) { REF = new Engine(); REF->init (48000.f); }
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-96s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    // a hat on every 16th for two bars (track 1 alone): each hit's balance, (L - R) / (L + R) of its energy
    auto hats = [&] (float rpan, float pan) {
        softReset (e, 48000.f);
        for (int t = 1; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setGlobal (G_BPM, 120.0f); e->g[G_MXON] = 0.0f;
        e->setParam (0, P_MACH, machineVal (M_HH_MT)); e->setParam (0, P_DEC, 0.15f); e->setParam (0, P_VOL, 0.6f);
        e->setParam (0, P_PAN, pan); e->setParam (0, P_RPAN, rpan); e->setParam (0, P_REV, 0.0f); e->setParam (0, P_DLY, 0.0f);
        for (int st = 0; st < 16; ++st) e->setTrig (0, st, 1);
        e->setPlay (1);
        std::vector<float> L (128), R (128), l, r;
        for (int i = 0; i < 48000 * 4; i += 128) { e->process (L.data(), R.data(), 128, nullptr); l.insert (l.end(), L.begin(), L.end()); r.insert (r.end(), R.begin(), R.end()); }
        e->setPlay (0);
        std::vector<double> bal;
        const int sps = 6000;                                   // (a 16th at 120 BPM)
        for (int k = 1; k < 31; ++k)
        {
            double el = 0, er = 0; int a = -1;
            for (int i = k * sps - 600; i < k * sps + 1200 && i < (int) l.size(); ++i) if (fabsf (l[i]) + fabsf (r[i]) > 0.01f) { a = i; break; }
            if (a < 0) continue;
            for (int i = a; i < a + 1200; ++i) { el += l[i] * l[i]; er += r[i] * r[i]; }
            bal.push_back ((el - er) / (el + er + 1e-20));
        }
        return bal;
    };
    {
        const auto c = hats (0.5f, 0.5f);
        double w = 0; for (double b : c) w = std::max (w, fabs (b));
        check (c.size() >= 28 && w < 0.01, "RPAN at its centre: every hit where PAN puts it (worst balance)", w);
    }
    {
        const auto d = hats (1.0f, 0.5f);
        double mn = 1, mx = -1, sum = 0; int left = 0, right = 0;
        for (double b : d) { mn = std::min (mn, b); mx = std::max (mx, b); sum += b; if (b > 0.2) ++left; if (b < -0.2) ++right; }
        check (d.size() >= 28 && mx - mn > 1.2 && left >= 5 && right >= 5 && fabs (sum / d.size()) < 0.35,
               "RPAN to the right: each hit drawn on either side (balance's range; hits well left and well right)", mx - mn);
        const auto h = hats (0.75f, 0.5f);
        double hm = 0; for (double b : h) hm = std::max (hm, fabs (b));
        check (hm < 0.75 && hm > 0.2, "RPAN half way: the draws stay within half the way to each side (widest balance)", hm);
    }
    {
        const auto p = hats (0.0f, 0.5f);
        int alt = 0; double weak = 1;
        for (size_t i = 0; i < p.size(); ++i) { weak = std::min (weak, fabs (p[i])); if (i && p[i] * p[i - 1] < 0) ++alt; }
        check (p.size() >= 28 && alt == (int) p.size() - 1 && weak > 0.9, "RPAN to the left: ping-pong, hard left and hard right in turn (the weakest balance)", weak);
        const auto q = hats (0.0f, 0.8f);
        double lo = 1, hi = -1; for (double b : q) { lo = std::min (lo, b); hi = std::max (hi, b); }
        check (hi > lo + 0.5 && lo < -0.9, "RPAN around PAN: a track panned right, its ping-pong from the middle to the right edge", hi - lo);
    }
    // SPRD: a held saw (SY·RAW), 1.5 s; the sides over the crossover, none under it; as loud; in mono its highs lower
    {
        auto saw = [&] (float sprd, float mono, std::vector<float>& l, std::vector<float>& r) {
            softReset (e, 48000.f);
            for (int t = 1; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
            e->setParam (0, P_MACH, machineVal (M_SY_RAW)); e->setParam (0, P_DEC, 1.0f); e->setParam (0, P_VOL, 0.6f);
            e->setParam (0, P_TUNE, 0.5f - 12.0f / 48); e->setParam (0, P_SPRD, sprd); e->setParam (0, P_SMON, mono); e->setParam (0, P_REV, 0.0f);
            e->audition (0, 0.9f, 0.0f);
            std::vector<float> L (128), R (128); l.clear(); r.clear();
            for (int i = 0; i < 48000 * 3 / 2; i += 128) { e->process (L.data(), R.data(), 128, nullptr); l.insert (l.end(), L.begin(), L.end()); r.insert (r.end(), R.begin(), R.end()); }
        };
        std::vector<float> l0, r0, l1, r1;
        saw (0.0f, 0.5f, l0, r0); saw (1.0f, 0.5f, l1, r1);
        std::vector<float> m0 (l0.size()), s0 (l0.size()), m1 (l1.size()), s1 (l1.size());
        for (size_t i = 0; i < l0.size(); ++i) { m0[i] = 0.5f * (l0[i] + r0[i]); s0[i] = 0.5f * (l0[i] - r0[i]); m1[i] = 0.5f * (l1[i] + r1[i]); s1[i] = 0.5f * (l1[i] - r1[i]); }
        const int a = 24000;
        const double sHi = bandDb (s1, a, 1000, 8000) - bandDb (m1, a, 1000, 8000), sLo = bandDb (s1, a, 40, 60) - bandDb (m1, a, 40, 60);
        const double s0Hi = bandDb (s0, a, 1000, 8000) - bandDb (m0, a, 1000, 8000);
        check (sHi > -5.0 && s0Hi < -60.0, "SPRD at its top: over the crossover the sides as strong as the middle (side against mid, 1 .. 8 kHz, dB)", sHi);
        check (sLo < -18.0, "SPRD: under MONO (155 Hz) the track stays centred (side against mid, 40 .. 60 Hz, dB)", sLo);
        double e0 = 0, e1 = 0; for (int i = a; i < a + 24000; ++i) { e0 += l0[i] * l0[i] + r0[i] * r0[i]; e1 += l1[i] * l1[i] + r1[i] * r1[i]; }
        const double dl = 10 * log10 (e1 / e0);
        check (fabs (dl) < 1.5, "SPRD: as loud, left and right together (dB against none; a low saw, much of it near the crossover)", dl);
        const double mono = bandDb (m1, a, 1000, 8000) - bandDb (m0, a, 1000, 8000);
        check (mono < -3.0 && mono > -6.0, "SPRD: room made in the middle (in mono, 1 .. 8 kHz, dB against none)", mono);
        std::vector<float> l2, r2; saw (1.0f, 1.0f, l2, r2);
        std::vector<float> m2 (l2.size()), s2 (l2.size());
        for (size_t i = 0; i < l2.size(); ++i) { m2[i] = 0.5f * (l2[i] + r2[i]); s2[i] = 0.5f * (l2[i] - r2[i]); }
        const double s2Mid = bandDb (s2, a, 150, 300) - bandDb (m2, a, 150, 300), s1Mid = bandDb (s1, a, 150, 300) - bandDb (m1, a, 150, 300);
        check (s1Mid - s2Mid > 6.0, "SPRD MONO at 600 Hz: 150 .. 300 Hz kept centred (side against mid, dB less than at 155 Hz)", s1Mid - s2Mid);
        // turned up from 0 on the held saw: no click (the highs of the left, 5 kHz up: their peak while it turns against
        // after)
        {
            softReset (e, 48000.f);
            for (int t = 1; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
            e->setParam (0, P_MACH, machineVal (M_SY_RAW)); e->setParam (0, P_DEC, 1.0f); e->setParam (0, P_VOL, 0.6f);
            e->setParam (0, P_TUNE, 0.4f); e->setParam (0, P_SPRD, 0.0f); e->setParam (0, P_REV, 0.0f);
            e->audition (0, 0.9f, 0.0f);
            std::vector<float> L (128), R (128), o;
            for (int i = 0; i < 48000 * 3 / 2; i += 128)
            {
                if (i >= 24000 && i < 24000 + 7680 && (i % 768) == 0) e->setParam (0, P_SPRD, (float) (i - 24000 + 768) / 7680.0f);
                e->process (L.data(), R.data(), 128, nullptr); o.insert (o.end(), L.begin(), L.end());
            }
            const double w = 2 * M_PI * 5000.0 / 48000.0, al = sin (w) / (2 * 0.7071), cw = cos (w), a0 = 1 + al;
            const double b0 = (1 + cw) / 2 / a0, b1 = -(1 + cw) / a0, b2 = b0, a1 = -2 * cw / a0, a2 = (1 - al) / a0;
            double x1 = 0, x2 = 0, y1 = 0, y2 = 0, during = 0, after = 0, before = 0;
            for (int i = 0; i < (int) o.size(); ++i)
            {
                const double y = b0 * o[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = o[i]; y2 = y1; y1 = y;
                if (i >= 12000 && i < 22000) before = fmax (before, fabs (y));
                else if (i >= 23000 && i < 24000 + 7680 + 2400) during = fmax (during, fabs (y));
                else if (i >= 36000 && i < 46000) after = fmax (after, fabs (y));
            }
            // (the sides disperse the saw's edges: its peaks lower after; against the higher of before and after)
            const double r = during / (fmax (before, after) + 1e-9);
            check (r < 1.3, "SPRD turned up from 0 on a held saw: no click (peak while it turns, against before and after)", r);
        }
        bool bad = false; for (float v : l1) if (! std::isfinite (v)) bad = true;
        check (! bad && peakOf (l1) < 1.0f, "SPRD: finite, under 0 dB (peak)", peakOf (l1));
    }
    delete e;
    printf ("%d failure(s)\n%s\n", fails, fails ? "SPREAD FAILED" : "SPREAD OK");
    return fails ? 1 : 0;
}


// v65: RDEC (each hit's decay drawn: to the right at each hit, to the left a draw per step that comes back with the
// pattern) and the LFO's SLEW (a random line, SLW of each cycle to reach its draw) and LOOP (LEN draws, round again)
static int rdectest()
{
    Engine* e = new Engine(); e->init (48000.f);
    if (! REF) { REF = new Engine(); REF->init (48000.f); }
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-96s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    // a sine on every 16th for two bars (track 1 alone): each hit's tail against its head (dB), what its decay leaves (the
    // machine's own drift moves it by half a dB from hit to hit); and each hit's draw, as the engine keeps it (draws)
    std::vector<float> draws;
    auto hits = [&] (float rdec, float dec, float seed, int machine, int ratStep) {
        softReset (e, 48000.f);
        for (int t = 1; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setGlobal (G_BPM, 120.0f); e->g[G_MXON] = 0.0f;
        e->setParam (0, P_MACH, machineVal (machine)); e->setParam (0, P_DEC, dec); e->setParam (0, P_VOL, 0.6f);
        e->setParam (0, P_RDEC, rdec); e->setParam (0, P_RDSD, seed); e->setParam (0, P_REV, 0.0f); e->setParam (0, P_DLY, 0.0f);
        for (int st = 0; st < 16; ++st) e->setTrig (0, st, 1);
        if (ratStep >= 0) e->setRatchet (0, ratStep, 2);
        e->setPlay (1);
        std::vector<float> L (128), R (128), l;
        draws.clear(); int hc = e->tr[0].hitCount;
        for (int i = 0; i < 48000 * 4 + 6000; i += 128)
        {
            e->process (L.data(), R.data(), 128, nullptr); for (int k = 0; k < 128; ++k) l.push_back (L[k] + R[k]);
            if (e->tr[0].hitCount != hc) { hc = e->tr[0].hitCount; draws.push_back (e->tr[0].voice[e->tr[0].cur].rdec); }
        }
        e->setPlay (0);
        std::vector<double> tl;
        const int sps = 6000;                                   // (a 16th at 120 BPM)
        for (int k = 0; k < 32; ++k)
        {
            int a = -1;
            for (int i = k * sps; i < k * sps + 1200 && i < (int) l.size(); ++i) if (fabsf (l[i]) > 0.003f) { a = i; break; }
            if (a < 0) { tl.push_back (-200.0); continue; }
            double h = 0, t = 0;
            for (int i = a; i < a + 600; ++i) h += l[i] * l[i];
            for (int i = a + 600; i < a + 2800; ++i) t += l[i] * l[i];
            tl.push_back (10 * log10 ((t + 1e-20) / (h + 1e-20)));
        }
        return tl;
    };
    auto range = [] (const std::vector<double>& v) { double lo = 1e9, hi = -1e9; for (double x : v) { lo = std::min (lo, x); hi = std::max (hi, x); } return hi - lo; };
    {
        const auto c = hits (0.5f, 0.3f, 0.0f, M_SINUS, -1);
        double d0 = 0; for (float d : draws) d0 = std::max (d0, (double) fabsf (d));
        check (c.size() == 32 && range (c) < 1.0 && draws.size() >= 32 && d0 == 0.0, "RDEC at its centre: no draw, every hit decays alike (range of tail against head, dB)", range (c));
        const auto r = hits (1.0f, 0.3f, 0.0f, M_SINUS, -1);
        int diff = 0; for (int k = 0; k < 16; ++k) if (fabs (r[k] - r[k + 16]) > 0.5) ++diff;
        check (range (r) > 8.0 && diff >= 10, "RDEC to the right: each hit its own decay, bar 2 not bar 1 again (range, dB)", range (r));
        const auto h = hits (0.75f, 0.3f, 0.0f, M_SINUS, -1);
        check (range (h) < 0.75 * range (r) && range (h) > 2.0, "RDEC half way right: the draws within half as far (range, dB)", range (h));
        const auto p = hits (0.0f, 0.3f, 0.25f, M_SINUS, -1);
        double same = 0; for (int k = 0; k < 16; ++k) same = std::max (same, fabs (p[k] - p[k + 16]));
        double dsame = 0; for (int k = 0; k < 16 && draws.size() >= 32; ++k) dsame = std::max (dsame, (double) fabsf (draws[k] - draws[k + 16]));
        check (range (p) > 8.0 && same < 1.5 && draws.size() >= 32 && dsame == 0.0, "RDEC to the left (LOOP): each step its own decay, the same draws again in bar 2 (largest change, dB)", same);
        check (fabsf (draws[3] - 0.25f * loopDraw (0.25f, 3u, 3u * 64u)) < 1e-6f, "RDEC LOOP: step 4's draw is loopDraw's, a quarter of the knob's travel at its end", draws[3]);
        const auto q = hits (0.0f, 0.3f, 0.75f, M_SINUS, -1);
        double moved = 0; for (int k = 0; k < 16; ++k) moved = std::max (moved, fabs (p[k] - q[k]));
        check (moved > 4.0, "RDEC LOOP, another seed: another loop (largest change of a step, dB)", moved);
        const auto w = hits (0.25f, 0.3f, 0.25f, M_SINUS, -1);
        double sc = 0; int n = 0; for (int k = 0; k < 16; ++k) if (fabs (p[k] - c[k]) > 1.0) { sc += (w[k] - c[k]) / (p[k] - c[k]); ++n; }
        check (n >= 8 && sc / n > 0.3 && sc / n < 0.7, "RDEC LOOP half way: the same loop, half as far (share of the full offset, on average)", n ? sc / n : 0);
        // a ratchet on step 4: its second hit draws its own decay, and the roll comes back the same in bar 2
        const auto rr = hits (0.0f, 0.3f, 0.25f, M_SINUS, 4);
        // (17 hits a bar: step 5's two)
        double back = 0; for (int k = 0; k < 17 && draws.size() >= 34; ++k) back = std::max (back, (double) fabsf (draws[k] - draws[k + 17]));
        check (draws.size() >= 34 && back == 0.0 && draws[4] != draws[5], "RDEC LOOP: a ratchet's hits each their draw, the roll the same in bar 2 (largest change)", back);
    }
    // DEC at the top (where a machine holds or runs free): RDEC leaves it alone (bit for bit)
    {
        auto out = [&] (float rdec) {
            delete e; e = new Engine(); e->init (48000.f);
            for (int t = 1; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
            e->setGlobal (G_BPM, 120.0f);
            e->setParam (0, P_MACH, machineVal (M_ITERA)); e->setParam (0, P_DEC, 0.99f); e->setParam (0, P_RDEC, rdec);
            e->setTrig (0, 0, 1); e->setTrig (0, 8, 1);
            e->setPlay (1);
            std::vector<float> L (128), R (128), o;
            for (int i = 0; i < 48000; i += 128) { e->process (L.data(), R.data(), 128, nullptr); o.insert (o.end(), L.begin(), L.end()); }
            e->setPlay (0);
            return o;
        };
        const auto a = out (0.5f), b = out (1.0f);
        double d = 0; for (size_t i = 0; i < a.size(); ++i) d = std::max (d, (double) fabsf (a[i] - b[i]));
        check (d == 0.0, "RDEC with DEC at its top (ITERA's free run): nothing moves (largest difference)", d);
    }
    // the LFO's new waves, read where the engine keeps them (track 1's LFO 1, a cycle a step, 120 BPM: 6000 samples)
    auto lfoRun = [&] (int wave, float slw, int len, float seed, int trigEvery) {
        softReset (e, 48000.f);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setGlobal (G_BPM, 120.0f);
        e->setParam (0, P_LSPD, 11.5f / NUM_LFO_SPEEDS); e->setParam (0, P_LWAV, (wave + 0.5f) / NUM_LFO_WAVES);
        e->setParam (0, P_LDST, 1.5f / NUM_LFO_DEST); e->setParam (0, P_LDEP, 1.0f);
        e->setParam (0, P_L1SL, slw); e->setParam (0, P_L1LN, (len + 0.5f) / NUM_LFO_LENS); e->setParam (0, P_L1SD, seed);
        if (trigEvery > 0) for (int st = 0; st < 16; st += trigEvery) e->setTrig (0, st, 1);
        e->setPlay (1);
        std::vector<float> L (128), R (128), v;
        for (int i = 0; i < 48000 * 4; i += 128) { e->process (L.data(), R.data(), 128, nullptr); v.push_back (e->tr[0].lfo[0].val); }
        e->setPlay (0);
        return v;                                                     // (a value every 128 samples)
    };
    auto jump = [] (const std::vector<float>& v) { double j = 0; for (size_t i = 1; i < v.size(); ++i) j = std::max (j, (double) fabsf (v[i] - v[i - 1])); return j; };
    {
        const auto sh = lfoRun (LW_SH, 0.5f, 6, 0.0f, 0), s0 = lfoRun (LW_SLEW, 0.0f, 6, 0.0f, 0), s1 = lfoRun (LW_SLEW, 1.0f, 6, 0.0f, 0);
        check (jump (s0) > 0.5 && jump (sh) > 0.5, "SLEW with SLW at 0: it steps like S&H (largest step between two blocks)", jump (s0));
        check (jump (s1) < 0.12 && range (std::vector<double> (s1.begin(), s1.end())) > 1.0, "SLEW with SLW at 100 %: a random line, no step (largest move in a block)", jump (s1));
        // SLW 50 %: there by half the cycle, then still
        const auto s5 = lfoRun (LW_SLEW, 0.5f, 6, 0.0f, 0);
        double still = 0; int moving = 0;
        for (size_t i = 200; i + 1 < s5.size(); ++i)
        {
            const double ph = fmod ((double) (i + 1) * 128.0 / 6000.0, 1.0);
            if (ph > 0.55 && ph < 0.95) still = std::max (still, (double) fabsf (s5[i + 1] - s5[i]));
            else if (ph > 0.05 && ph < 0.45 && fabsf (s5[i + 1] - s5[i]) > 1e-4f) ++moving;
        }
        check (still < 1e-6 && moving > 50, "SLEW with SLW at 50 %: it glides through the first half of each cycle, then holds (move in the second half)", still);
    }
    {
        // LOOP, 8 draws (LEN index 6): the value in the middle of each cycle, 30 cycles
        auto mids = [&] (const std::vector<float>& v) { std::vector<float> m; for (int c = 0; c < 30; ++c) m.push_back (v[(size_t) ((c * 6000 + 3000) / 128)]); return m; };
        const auto a = mids (lfoRun (LW_LOOP, 0.5f, 6, 0.3f, 0));
        double rep = 0; for (int c = 0; c + 8 < 30; ++c) rep = std::max (rep, (double) fabsf (a[c] - a[c + 8]));
        double lo = 1, hi = -1; for (int c = 0; c < 8; ++c) { lo = std::min (lo, (double) a[c]); hi = std::max (hi, (double) a[c]); }
        check (rep == 0.0 && hi - lo > 0.8, "LOOP, LEN 8: eight draws, then the same eight again (largest change from a loop to the next)", rep);
        check (fabsf (a[0] - loopDraw (0.3f, 1u, 0u)) < 1e-6f && fabsf (a[5] - loopDraw (0.3f, 1u, 5u)) < 1e-6f, "LOOP: its draws are loopDraw's (the page draws the same loop)", a[5]);
        const auto b = mids (lfoRun (LW_LOOP, 0.5f, 6, 0.7f, 0));
        double other = 0; for (int c = 0; c < 8; ++c) other = std::max (other, (double) fabsf (a[c] - b[c]));
        check (other > 0.5, "LOOP, another seed: another loop (largest change)", other);
        const auto c3 = mids (lfoRun (LW_LOOP, 0.5f, 1, 0.3f, 0));
        double r3 = 0; for (int c = 0; c + 3 < 30; ++c) r3 = std::max (r3, (double) fabsf (c3[c] - c3[c + 3]));
        check (r3 == 0.0 && fabsf (c3[1] - a[1]) < 1e-6f, "LOOP, LEN 3: round again every third cycle, the same first draws", r3);
        // LOOP·T, a trig every 4 steps: the loop starts again with each one (its first four draws each time)
        const auto t4 = mids (lfoRun (LFO_SHAPES + LW_LOOP, 0.5f, 6, 0.3f, 4));
        double rs = 0; for (int c = 4; c + 4 < 30; ++c) rs = std::max (rs, (double) fabsf (t4[c] - t4[c - 4]));
        check (rs == 0.0 && fabsf (t4[2] - a[2]) < 1e-6f, "LOOP·T: each trig starts the loop again (largest change from one trig to the next)", rs);
    }
    printf (fails ? "%d FAILED\n" : "all passed\n", fails);
    delete e;
    return fails ? 1 : 0;
}


// v66: CHIMERA, the sampler on the bank: plays its samples as they are (stereo), TUNE, REV, DEC; FAMILLE (VARI's
// neighbours, RND and LOOP); GREFFE (A until the graft point, B after it, no click at the seam, SPLT)
static int chimtest()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-100s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    const auto t0 = std::chrono::steady_clock::now();
    const bool bank = loadBank();
    const double lms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
    check (bank && BANK.n >= 100 && BANK.ncat >= 8, "the bank: every sample decoded, its crc checked (samples; ms to load)", BANK.n + lms / 1e4);
    if (! bank) { printf ("%d FAILED\n", fails); return 1; }
    // (lite: the master's oversampling off, its fractional delay would blur a comparison sample by sample)
    Engine* e = new Engine(); e->init (48000.f, 1);
    if (! REF) { REF = new Engine(); REF->init (48000.f); }
    const int K = BANK.catFirst[0], CL = BANK.catFirst[2];             // the first kick, the first clap
    auto catOf = [] (int i) { return BANK.e[i].cat; };
    auto smpVal = [] (int i) { const int c = BANK.e[i].cat; return ((float) (i - BANK.catFirst[c]) + 0.5f) / (float) BANK.catCount[c]; };
    auto catVal = [] (int c) { return ((float) c + 0.5f) / (float) BANK.ncat; };
    // track 1 alone on CHIMERA, sample i, its side places at their defaults
    auto setup = [&] (int i) {
        softReset (e, 48000.f);
        for (int t = 1; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setGlobal (G_BPM, 120.0f); e->g[G_MXON] = 0.0f;
        e->g[G_EQON] = e->g[G_COON] = e->g[G_DRON] = 0.0f;          // (the master out of the way: the sample as it is)
        float sd[NUM_SIDES]; machineSideDefaults (M_CHIMERA, sd);
        for (int q = 0; q < NUM_SIDES; ++q) e->setParam (0, sideParam (q), sd[q]);
        e->setParam (0, P_MACH, machineVal (M_CHIMERA)); e->setParam (0, P_DEC, 1.0f); e->setParam (0, P_VOL, 1.0f);
        e->setParam (0, P_A, catVal (catOf (i))); e->setParam (0, P_B, smpVal (i)); e->setParam (0, P_C, 0.5f); e->setParam (0, P_D, 0.0f); e->setParam (0, P_E, 0.25f);
        e->setParam (0, P_FREQ, 1.0f); e->setParam (0, P_REV, 0.0f); e->setParam (0, P_DLY, 0.0f); e->setParam (0, P_VEL, 1.0f);
    };
    auto run = [&] (int frames, std::vector<float>& l, std::vector<float>& r) {
        std::vector<float> L (128), R (128); l.clear(); r.clear();
        for (int i = 0; i < frames; i += 128) { e->process (L.data(), R.data(), 128, nullptr); l.insert (l.end(), L.begin(), L.end()); r.insert (r.end(), R.begin(), R.end()); }
    };
    // the sample itself at 48 kHz (linear), times its gain: what the track should play
    auto ref = [&] (int i, int ch, int n, bool backwards = false) {
        const BankEntry& b = BANK.e[i]; std::vector<float> y ((size_t) n, 0.0f);
        for (int k = 0; k < n; ++k) { const double p = backwards ? (b.frames - 1) - k * b.sr / 48000.0 : k * b.sr / 48000.0;
            if (p < 0) break;
            const int j = (int) p; const float f = (float) (p - j);
            float q[4]; for (int u = 0; u < 4; ++u) q[u] = bankVal (b, j - 1 + u, ch);
            const float c1 = 0.5f * (q[2] - q[0]), c2 = q[0] - 2.5f * q[1] + 2.0f * q[2] - 0.5f * q[3], c3 = 0.5f * (q[3] - q[0]) + 1.5f * (q[1] - q[2]);
            y[(size_t) k] = b.gain * (((c3 * f + c2) * f + c1) * f + q[1]) / 32768.0f; }
        // (the voice's DC blocker, 10 Hz, as the track has it)
        const float R = 1.0f - TWO_PI * 10.0f / 48000.0f; float dx = 0, dy = 0;
        for (auto& x : y) { const float o = x - dx + R * dy; dx = x; dy = o; x = o; }
        return y; };
    auto lagOf = [] (const std::vector<float>& x, const std::vector<float>& y, int maxLag) {
        int best = 0; double bs = -1e30;
        for (int d = 0; d < maxLag; ++d) { double s = 0; for (size_t k = 0; k + d < x.size() && k < y.size() && k < 4096; ++k) s += x[k + d] * y[k]; if (s > bs) { bs = s; best = d; } }
        return best; };
    auto corr = [] (const std::vector<float>& x, int d, const std::vector<float>& y, int a, int n, double* gain) {
        double xy = 0, xx = 1e-20, yy = 1e-20;
        for (int k = a; k < a + n && k + d < (int) x.size() && k < (int) y.size(); ++k) { xy += x[k + d] * y[k]; xx += x[k + d] * x[k + d]; yy += y[k] * y[k]; }
        if (gain) *gain = 10 * log10 (xx / yy);
        return xy / sqrt (xx * yy); };
    std::vector<float> l, r;
    // a stereo sample, as it is: left and right where they should be, at its level
    int st = -1; for (int i = 0; i < BANK.n && st < 0; ++i) if (BANK.e[i].ch == 2 && BANK.e[i].cat == 2) st = i;
    {
        setup (st); e->audition (0, 1.0f, 0.0f); run (48000, l, r);
        const auto rl = ref (st, 0, 40000), rr = ref (st, 1, 40000);
        const int d = lagOf (l, rl, 600);
        double gl, gr; const double cl = corr (l, d, rl, 0, 20000, &gl), cr = corr (r, d, rr, 0, 20000, &gr);
        if (getenv ("CHIM_DBG")) printf ("DBG clap levels %.2f %.2f dB\n", gl, gr);
        check (cl > 0.999 && cr > 0.999 && fabs (gl - gr) < 0.1, "a stereo clap as it is: left and right match the sample (correlation; the same level on both sides)", std::min (cl, cr));
        double sd = 0, md = 0; for (size_t k = 0; k < l.size(); ++k) { sd += (l[k] - r[k]) * (l[k] - r[k]); md += (l[k] + r[k]) * (l[k] + r[k]); }
        check (sd > md * 1e-3, "its sides heard (side against middle, dB)", 10 * log10 (sd / md));
    }
    // TUNE +12: twice as fast (half as long); REV: the sample backwards; DEC: a shorter sound
    auto len = [] (const std::vector<float>& x) { float pk = 0; for (float v : x) pk = std::max (pk, fabsf (v)); int last = 0; for (size_t k = 0; k < x.size(); ++k) if (fabsf (x[k]) > pk * 0.01f) last = (int) k; return last; };
    {
        int lg = K; for (int i = BANK.catFirst[0]; i < BANK.catFirst[0] + BANK.catCount[0]; ++i) if (BANK.e[i].frames > 30000 && BANK.e[i].frames < 80000) { lg = i; break; }
        setup (lg); e->audition (0, 1.0f, 0.0f); run (96000, l, r); const int L0 = len (l);
        setup (lg); e->setParam (0, P_TUNE, 0.75f); e->audition (0, 1.0f, 0.0f); run (96000, l, r); const int L12 = len (l);
        check (fabs ((double) L12 / L0 - 0.5) < 0.05, "TUNE +12: the sample twice as fast (its length against as it is)", (double) L12 / L0);
        setup (lg); e->setParam (0, P_DEC, 0.3f); e->audition (0, 1.0f, 0.0f); run (96000, l, r); const int Ld = len (l);
        check (Ld < L0 * 0.8, "DEC under FULL: a shorter sound (its length against as it is)", (double) Ld / L0);
        setup (lg); e->setParam (0, P_E, 0.75f); e->audition (0, 1.0f, 0.0f); run (96000, l, r);
        const auto rev2 = ref (lg, 0, 90000, true); const int nz = 0;
        std::vector<float> fl0, fr0; setup (lg); e->audition (0, 1.0f, 0.0f); run (8192, fl0, fr0);
        const int d = lagOf (fl0, ref (lg, 0, 8192), 600);           // (the track's latency, found forward: the tail it starts with is too quiet)
        setup (lg); e->setParam (0, P_E, 0.75f); e->audition (0, 1.0f, 0.0f); run (96000, l, r);
        const double c = corr (l, d, rev2, 200, 60000, nullptr);
        if (getenv ("CHIM_DBG")) { setup (lg); e->audition (0, 1.0f, 0.0f); std::vector<float> fl, fr; run (96000, fl, fr); const auto fw = ref (lg, 0, 90000); const int fd = lagOf (fl, fw, 600);
            int f0 = 0; while (f0 < 96000 && fabsf (fl[(size_t) f0]) < 1e-6f) ++f0;
            printf ("DBG first nonzero %d gain %.3f\n", f0, BANK.e[lg].gain);
            for (int k = 100; k < 700; k += 25) printf ("DBG out[%d] %+.5f %+.5f   ref[%d] %+.5f %+.5f\n", k, fl[(size_t) k], fr[(size_t) k], k - 105, k >= 105 ? fw[(size_t) (k - 105)] : 0.0f, k >= 105 ? ref (lg, 1, 700)[(size_t) (k - 105)] : 0.0f);
            for (int k : { 0, 10, 100, 1000, 3000, 10000, 20000 }) printf ("DBG k %6d out %+.5f ref %+.5f (lag %d) ref@64 %+.5f\n", k, fl[(size_t) (k + fd)], fw[(size_t) k], fd, fw[(size_t) k] );
            printf ("DBG forward corr %.4f lag %d; rev lag %d nz %d frames %d\n", corr (fl, fd, fw, 0, 20000, nullptr), fd, d, nz, BANK.e[lg].frames); }
        check (c > 0.98, "PLAY REV: the sample backwards (correlation with it reversed)", c);
    }
    // FAMILLE: the sample each hit plays (chA), a kick on every 16th for two bars
    auto hits = [&] (int i, float vari, float seed, float sens, float meme) {
        setup (i); e->setParam (0, P_C, vari); e->setParam (0, PC_SEED, seed); e->setParam (0, PC_SENS, sens); e->setParam (0, PC_MEME, meme);
        for (int s2 = 0; s2 < 16; ++s2) e->setTrig (0, s2, 1);
        e->setPlay (1);
        std::vector<int> v; int hc = e->tr[0].chHits; std::vector<float> L (128), R (128);
        for (int k = 0; k < 48000 * 4 + 6000; k += 128) { e->process (L.data(), R.data(), 128, nullptr); if (e->tr[0].chHits != hc) { hc = e->tr[0].chHits; v.push_back (e->tr[0].chA); } }
        e->setPlay (0);
        return v; };
    {
        const auto c = hits (K, 0.5f, 0.0f, 0.5f, 0.75f);
        bool same = c.size() >= 32; for (int x : c) same = same && x == K;
        check (same, "VARI at its centre: every hit the sample chosen (hits)", (double) c.size());
        const auto rnd = hits (K, 1.0f, 0.0f, 0.5f, 0.75f);
        std::vector<int> u (rnd); std::sort (u.begin(), u.end()); u.erase (std::unique (u.begin(), u.end()), u.end());
        bool inCat = true; for (int x : rnd) inCat = inCat && BANK.e[x].cat == 0;
        int cand[CHIM_NEAR]; const int nc = bankNear (K, 0.0f, true, cand, CHIM_NEAR);
        bool inNear = true; for (int x : rnd) inNear = inNear && std::find (cand, cand + nc, x) != cand + nc;
        int diff = 0; for (int k = 0; k < 16; ++k) if (rnd[(size_t) k] != rnd[(size_t) k + 16]) ++diff;
        check (u.size() >= 6 && inCat && inNear && diff >= 8, "VARI to the right: a neighbour at each hit, kicks only (MEME), among its 12 nearest, bar 2 not bar 1 (how many)", (double) u.size());
        const auto lp = hits (K, 0.0f, 0.3f, 0.5f, 0.75f);
        bool rep = lp.size() >= 32; for (int k = 0; k < 16 && rep; ++k) rep = lp[(size_t) k] == lp[(size_t) k + 16];
        std::vector<int> ul (lp.begin(), lp.begin() + 16); std::sort (ul.begin(), ul.end()); ul.erase (std::unique (ul.begin(), ul.end()), ul.end());
        check (rep && ul.size() >= 5, "VARI to the left (LOOP): each step its neighbour, the same again in bar 2 (how many in a bar)", (double) ul.size());
        const auto lp2 = hits (K, 0.0f, 0.7f, 0.5f, 0.75f);
        int moved = 0; for (int k = 0; k < 16; ++k) if (lp[(size_t) k] != lp2[(size_t) k]) ++moved;
        check (moved >= 6, "VARI LOOP, another seed: another loop (steps changed)", moved);
        const auto sm = hits (K, 0.75f, 0.0f, 0.5f, 0.75f);
        int nh = 0; const int n6 = bankNear (K, 0.0f, true, cand, 1 + (int) (0.5f * 11 + 0.5f));
        for (int x : sm) nh += std::find (cand, cand + n6, x) != cand + n6;
        check (nh == (int) sm.size(), "VARI half way: among its 6 nearest only", (double) n6);
        // the nearest are alike: their distance against the bank's (features)
        double dn = 0, da = 0; int na = 0;
        auto dist = [] (int a, int b) { double s2 = 0; for (int f = 0; f < BANK_FEAT; ++f) { const double x = BANK.e[a].feat[f] - BANK.e[b].feat[f]; s2 += x * x; } return sqrt (s2); };
        for (int i = 0; i < BANK.n; i += 7) { int cc[6]; const int k = bankNear (i, 0.0f, false, cc, 6); for (int q = 1; q < k; ++q) dn += dist (i, cc[q]) / (k - 1); for (int j = 0; j < BANK.n; j += 13) { da += dist (i, j); ++na; } }
        dn /= (BANK.n + 6) / 7; da /= na;
        check (dn < 0.45 * da, "FAMILLE: the 5 nearest are alike (their distance against any two samples')", dn / da);
        int cs[6], ct[6]; bankNear (K + 3, -1.0f, false, cs, 6); bankNear (K + 3, 1.0f, false, ct, 6);
        int differ = 0; for (int q = 1; q < 6; ++q) differ += std::find (ct, ct + 6, cs[q]) == ct + 6;
        check (differ >= 2, "SENS: alike in shape and alike in timbre are other neighbours (of 5)", differ);
    }
    // GREFFE: a clap (A) on a long impact (B): A as it is until the graft point, B after it
    {
        int imp = -1; const int fx = 8;
        for (int i = BANK.catFirst[fx]; i < BANK.catFirst[fx] + BANK.catCount[fx]; ++i) if (strstr (BANK.e[i].name, "IMPACT") && BANK.e[i].frames > 60000) { imp = i; break; }
        if (imp < 0) imp = BANK.catFirst[fx];
        std::vector<float> la, ra, lg, rg, lb, rb;
        setup (CL); e->audition (0, 1.0f, 0.0f); run (96000, la, ra);
        setup (CL); e->setParam (0, PC_BCAT, ((float) (fx + 1) + 0.5f) / (float) (BANK.ncat + 1)); e->setParam (0, PC_BSMP, smpVal (imp));
        e->setParam (0, PC_POIN, logf (30.0f) / logf (150.0f)); e->setParam (0, PC_FOND, logf (16.0f) / logf (80.0f));
        e->audition (0, 1.0f, 0.0f); run (96000, lg, rg);
        // (the same onset: the first 30 ms bit for bit)
        int on = 0; while (on < (int) la.size() && fabsf (la[(size_t) on]) < 1e-6f) ++on;
        double d0 = 0; int dk = -1; for (int k = on; k < on + (int) (0.029 * 48000); ++k) { const double dd = fabsf (la[(size_t) k] - lg[(size_t) k]); if (dd > d0) { d0 = dd; if (dk < 0 && dd > 1e-4) dk = k - on; } }
        if (getenv ("CHIM_DBG")) printf ("DBG graft: on %d, first difference at %d samples\n", on, dk);
        check (d0 < 1e-6, "GREFFE: until the graft point (30 ms), the attack as it is (largest difference)", d0);
        const int Lc = len (la), Lg = len (lg), Li = (int) (BANK.e[imp].frames * 48000.0 / BANK.e[imp].sr);
        check (Lg > Lc * 1.5 && Lg > std::min (Li / 2, 90000), "GREFFE: after it, the body (the chimera as long as the impact, not the clap; s)", Lg / 48000.0);
        // the seam: no click (the highs over 8 kHz in 1 ms windows around it, against the loudest elsewhere)
        std::vector<float> hp (lg.size()); float z1 = 0; for (size_t k = 0; k < lg.size(); ++k) { hp[k] = lg[k] - z1; z1 = lg[k]; }
        auto win = [&] (int a) { double s2 = 0; for (int k = a; k < a + 48; ++k) s2 += hp[(size_t) k] * hp[(size_t) k]; return s2; };
        const int seam = on + (int) (0.030 * 48000), fade = (int) (0.016 * 48000);
        double sm = 0, el = 0; for (int a = seam - 96; a < seam + fade + 96; a += 24) sm = std::max (sm, win (a));
        for (int a = on; a < on + 48000; a += 24) if (a < seam - 400 || a > seam + fade + 400) el = std::max (el, win (a));
        check (sm < el * 1.5, "GREFFE: no click at the seam (its highs against the loudest of the sound, 1 ms windows)", sm / el);
        // SPLT at 1 kHz: after the graft, the clap's highs go on (as without a graft), the lows are the impact's
        setup (CL); e->setParam (0, PC_BCAT, ((float) (fx + 1) + 0.5f) / (float) (BANK.ncat + 1)); e->setParam (0, PC_BSMP, smpVal (imp));
        e->setParam (0, PC_POIN, logf (30.0f) / logf (150.0f)); e->setParam (0, PC_FOND, logf (16.0f) / logf (80.0f));
        e->setParam (0, PC_SPLT, 0.02f + 0.98f * logf (1000.0f / 60.0f) / logf (133.3333f));
        e->audition (0, 1.0f, 0.0f); run (96000, lb, rb);
        const int a2 = on + (int) (0.06 * 48000);
        const double hiA = bandDb (la, a2, 4000, 12000), hiS = bandDb (lb, a2, 4000, 12000), hiG = bandDb (lg, a2, 4000, 12000);
        const double loS = bandDb (lb, a2, 40, 300), loG = bandDb (lg, a2, 40, 300);
        check (fabs (hiS - hiA) < 3.0 && fabs (loS - loG) < 3.0, "SPLT 1 kHz: after the graft the clap's highs (against the clap alone, dB) and the impact's lows (against the graft without split)", fabs (hiS - hiA) + fabs (loS - loG));
        printf ("  INFO without SPLT the highs are the impact's: %+.1f dB from the clap's\n", hiG - hiA);
        check (fabs (e->tr[0].chAlign) <= 2.6f, "GREFFE: the seam lined up (B moved by, ms)", e->tr[0].chAlign);
    }
    // all of it on 8 tracks, ratchets, every knob drawn: no NaN, its cost
    {
        softReset (e, 48000.f);
        unsigned rs = 7; auto rnd = [&] () { rs = rs * 1664525u + 1013904223u; return (rs >> 8) / 16777216.0f; };
        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            e->setParam (t, P_MACH, machineVal (M_CHIMERA)); e->setParam (t, P_VOL, 0.5f);
            for (int p : { P_A, P_B, P_C, P_D, P_E, P_DEC, P_TUNE }) e->setParam (t, p, rnd());
            for (int q = 0; q < 11; ++q) e->setParam (t, sideParam (q), rnd());
            for (int s2 = 0; s2 < 16; ++s2) { e->setTrig (t, s2, 1); e->setRatchet (t, s2, 1 + (int) (rnd() * 4)); }
        }
        e->setPlay (1);
        std::vector<float> L (128), R (128); bool bad = false; float pk = 0;
        const auto c0 = std::chrono::steady_clock::now();
        for (int k = 0; k < 48000 * 6; k += 128) { e->process (L.data(), R.data(), 128, nullptr); for (int q = 0; q < 128; ++q) { if (! std::isfinite (L[q]) || ! std::isfinite (R[q])) bad = true; pk = std::max (pk, fabsf (L[q])); } }
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - c0).count();
        e->setPlay (0);
        check (! bad && pk > 0.01f, "8 tracks of CHIMERA, ratchets, grafts, VARI, every knob drawn: no NaN (peak)", pk);
        printf ("INFO 8 CHIMERA tracks: %.1f %% of real time (native)\n", 100.0 * ms / 6000.0);
    }
    printf (fails ? "%d FAILED\n" : "CHIMERA OK\n", fails);
    delete e;
    return fails ? 1 : 0;
}

// ----------------------------------------------------------------- v68: CHORUS, PHASER, the 1982 DELAY, FUZZ
// the amplitude of hz in x[a..b) (a Goertzel, Hann-windowed)
static double toneAmp (const std::vector<float>& x, int a, int b, double hz, double sr = 48000.0)
{
    a = std::max (a, 0); b = std::min (b, (int) x.size());
    const int n = b - a; if (n < 16) return 0.0;
    const double w = 2.0 * M_PI * hz / sr, c = 2.0 * cos (w);
    double s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { const double h = 0.5 - 0.5 * cos (2.0 * M_PI * i / (n - 1)); const double s = x[(size_t) (a + i)] * h + c * s1 - s2; s2 = s1; s1 = s; }
    const double re = s1 - s2 * cos (w), im = s2 * sin (w);
    return 4.0 * sqrt (re * re + im * im) / n;
}
static double db (double a) { return 20.0 * log10 (a + 1e-30); }
// the zero crossings' intervals (rising), their spread against their mean: a pitch that moves
static double zcSpread (const std::vector<float>& x, int a, int b)
{
    std::vector<double> t;
    for (int i = std::max (a, 1); i < b; ++i) if (x[(size_t) i - 1] < 0.0f && x[(size_t) i] >= 0.0f) t.push_back (i - 1 + x[(size_t) i - 1] / (x[(size_t) i - 1] - x[(size_t) i]));
    if (t.size() < 4) return 0.0;
    double m = 0, v = 0; const int k = (int) t.size() - 1;
    for (int i = 0; i < k; ++i) m += t[i + 1] - t[i];
    m /= k;
    for (int i = 0; i < k; ++i) v += (t[i + 1] - t[i] - m) * (t[i + 1] - t[i] - m);
    return sqrt (v / k) / m;
}
static double zcRate (const std::vector<float>& x, int a, int b)
{
    int z = 0; for (int i = std::max (a, 1); i < b; ++i) if (x[(size_t) i - 1] < 0.0f && x[(size_t) i] >= 0.0f) ++z;
    return z * 48000.0 / std::max (1, b - a);
}

static int v68test()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-92s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    const float SR = 48000.0f;
    auto sine = [&] (double hz, double secs, double a) { std::vector<float> x ((size_t) (SR * secs)); for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (a * sin (2 * M_PI * hz * i / SR)); return x; };

    // ---- CHORUS, on its own
    {
        auto run = [&] (int mode, int st, const std::vector<float>& inL, const std::vector<float>& inR, std::vector<float>& L, std::vector<float>& R) {
            static DimChorus c; c.init (SR);
            L.assign (inL.size(), 0.0f); R.assign (inL.size(), 0.0f);
            for (size_t i = 0; i + 32 <= inL.size(); i += 32) c.process (&inL[i], &inR[i], &L[i], &R[i], 32, ChorusParams { mode, st, 0.8f }, 1.0f);
        };
        std::vector<float> L, R, S, D;
        // (1) in mono the two lines cancel above the cross-over, add up under it
        for (double hz : { 3000.0, 80.0 })
        {
            const auto x = sine (hz, 3.0, 0.3);
            run (1, 0, x, x, L, R);
            S.resize (L.size()); D.resize (L.size()); for (size_t i = 0; i < L.size(); ++i) { S[i] = L[i] + R[i]; D[i] = L[i] - R[i]; }
            const double s = toneAmp (S, 48000, 144000, hz), d = toneAmp (D, 48000, 144000, hz);
            char w[160];
            if (hz > 1000) { snprintf (w, sizeof w, "CHORUS: at 3 kHz the left + right (mono) far under the left - right (dB)"); check (db (s) - db (d) < -15.0, w, db (s) - db (d)); }
            else { snprintf (w, sizeof w, "CHORUS: at 80 Hz the two lines add up in mono (L+R against the input x 2, dB)"); check (fabs (db (s) - db (0.6)) < 4.0, w, db (s) - db (0.6)); }
        }
        // (2) the modes: the sweep deepens from 1 to 4 (the pitch of a 1 kHz tone moves more)
        {
            const auto x = sine (1000.0, 6.0, 0.3);
            double sp[4];
            for (int m = 0; m < 4; ++m) { run (m, 0, x, x, L, R); sp[m] = zcSpread (L, 24000, 288000); }
            check (sp[0] < sp[1] && sp[1] < sp[2] && sp[2] < sp[3], "CHORUS: modes 1 < 2 < 3 < 4, how far the pitch moves (mode 4's spread)", sp[3]);
            check (sp[0] > 1e-4 && sp[3] < 0.05, "CHORUS: mode 1 still moves, mode 4 stays a chorus (no vibrato: spread under 5 %)", sp[0]);
        }
        // (3) the compander: a quiet tone and a loud one come out at their levels (the 2:1 and the 1:2 cancel)
        {
            double dev = 0;
            for (double a : { 0.01, 0.1, 0.5 })
            {
                const auto x = sine (80.0, 3.0, a);
                run (1, 0, x, x, L, R);
                for (size_t i = 0; i < L.size(); ++i) S[i] = L[i] + R[i];
                dev = std::max (dev, fabs (db (toneAmp (S, 48000, 144000, 80.0)) - db (2 * a)));
            }
            check (dev < 3.0, "CHORUS: the compander gives each level back (-40, -20, -6 dB at 80 Hz: worst error, dB)", dev);
        }
        // (4) silence: nothing at all, from the start and after a sound has died
        {
            std::vector<float> z ((size_t) SR * 2, 0.0f), x = sine (500.0, 0.5, 0.3); x.resize ((size_t) SR * 2, 0.0f);
            run (3, 1, z, z, L, R);
            double m = 0; for (float v : L) m = std::max (m, (double) fabs (v));
            check (m == 0.0, "CHORUS: silence in, silence out (exactly)", m);
            run (3, 1, x, x, L, R);
            m = 0; for (size_t i = (size_t) SR; i < L.size(); ++i) m = std::max (m, (double) fabs (L[i]));
            check (m < 1e-5, "CHORUS: half a second after the sound, nothing left (peak)", m);
        }
        // (5) STEREO input: a left-only send stays mostly left (each line its side)
        {
            const auto x = sine (80.0, 2.0, 0.3); std::vector<float> z (x.size(), 0.0f);
            run (1, 1, x, z, L, R);
            check (db (toneAmp (L, 24000, 96000, 80)) - db (toneAmp (R, 24000, 96000, 80)) > 8.0, "CHORUS STEREO: a left-only send, its low end stays left (the other side hears it high-passed; dB, 80 Hz)", db (toneAmp (L, 24000, 96000, 80)) - db (toneAmp (R, 24000, 96000, 80)));
        }
    }

    // ---- PHASER, on its own: the send gives back the phase-shifted sound; with the direct sound it notches
    {
        auto run = [&] (const PhaserParams& p, const std::vector<float>& x, std::vector<float>& L, std::vector<float>& R) {
            static BiPhaser ph; ph.init (SR);
            L.assign (x.size(), 0.0f); R.assign (x.size(), 0.0f);
            for (size_t i = 0; i + 32 <= x.size(); i += 32) ph.process (&x[i], &x[i], &L[i], &R[i], 32, p, 1.0f);
        };
        // the response of direct + phaser, the photocells at rest (depth 0: the darkest), from its impulse response
        const PhaserParams still { 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0 };
        std::vector<float> L, R;
        auto notches = [&] (int route, int side, double& deepest, double& top) {
            std::vector<float> x ((size_t) SR * 2, 0.0f); x[0] = 0.25f;
            PhaserParams q = still; q.route = route;
            run (q, x, L, R);
            std::vector<float>& h = side ? R : L;
            h[0] += 0.25f;
            int minima = 0; double prev = 0, prev2 = 0; deepest = 1e9; top = -1e9;
            for (int k = 0; k < 900; ++k)
            {
                const double hz = 8.0 * pow (2.0, k / 80.0);
                if (hz > 16000) break;
                double re = 0, im = 0; for (size_t i = 0; i < h.size(); ++i) { re += h[i] * cos (2 * M_PI * hz * i / SR); im -= h[i] * sin (2 * M_PI * hz * i / SR); }
                const double a = db (sqrt (re * re + im * im) / 0.25);
                deepest = std::min (deepest, a); top = std::max (top, a);
                if (k >= 2 && prev < prev2 && prev < a && prev < -10.0) ++minima;
                prev2 = prev; prev = a;
            }
            return minima;
        };
        double lo, hi, lo12, hi12;
        const int minima = notches (0, 0, lo, hi), minima12 = notches (2, 1, lo12, hi12);
        // the sweep: a 1 kHz tone through direct + phaser rises and falls by more than 20 dB as the sweep goes
        {
            const PhaserParams sw { 0.5f, 0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0, 0, 0 };
            const auto x = sine (1000.0, 4.0, 0.3);
            run (sw, x, L, R);
            for (size_t i = 0; i < x.size(); ++i) L[i] += x[i];
            double mn = 1e9, mx = -1e9;
            for (int b = 1; b < 40; ++b) { const double a = db (toneAmp (L, b * 4800, b * 4800 + 2400, 1000)); mn = std::min (mn, a); mx = std::max (mx, a); }
            check (mx - mn > 20.0, "PHASER: the sweep moves the notches (a 1 kHz tone, its swing in dB)", mx - mn);
        }
        // the photocell: it brightens fast, darkens slowly (a square sweep: up in under 15 ms, down in over 30)
        {
            static BiPhaser ph; ph.init (SR);
            const PhaserParams sq { 2.0f, 1.0f, 2.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0, 0, 0 };
            std::vector<float> x (32, 0.1f), oL (32), oR (32);
            std::vector<float> lt;
            for (int b = 0; b < 3000; ++b) { ph.process (x.data(), x.data(), oL.data(), oR.data(), 32, sq, 1.0f); lt.push_back (ph.lA); }
            // the edges: from 10 % to 90 % of the swing
            const float lmax = *std::max_element (lt.begin() + 1500, lt.end()), lmin = *std::min_element (lt.begin() + 1500, lt.end());
            int up = 0, dn = 0, ups = 0, dns = 0;
            for (size_t i = 1500; i + 1 < lt.size(); ++i)
            {
                const float u = (lt[i] - lmin) / (lmax - lmin);
                if (u > 0.1f && u < 0.9f) { if (lt[i + 1] > lt[i]) ++up; else ++dn; }
                if (lt[i] < lmin + 0.1f * (lmax - lmin) && lt[i + 1] >= lmin + 0.1f * (lmax - lmin)) ++ups;
                if (lt[i] > lmin + 0.9f * (lmax - lmin) && lt[i + 1] <= lmin + 0.9f * (lmax - lmin)) ++dns;
            }
            const double upMs = ups ? up * 32 / 48.0 / ups : 0, dnMs = dns ? dn * 32 / 48.0 / dns : 0;
            printf ("INFO PHASER photocell: 10-90 %% up %.1f ms, down %.1f ms\n", upMs, dnMs);
            check (upMs < 20.0 && dnMs > 2.5 * upMs, "PHASER: the photocell up fast, down slowly (its fall over its rise)", dnMs / std::max (upMs, 0.01));
        }
        // feedback at the top, a loud noise: it rings, it stays finite
        {
            PhaserParams fb { 0.3f, 0.0f, 0.4f, 0.0f, 0.8f, 1.0f, 0.8f, 1.0f, 1, 1, 2 };
            Rng r; r.seed (9); std::vector<float> x ((size_t) SR * 3); for (auto& v : x) v = 0.9f * r.bi();
            run (fb, x, L, R);
            double m = 0; bool ok = true; for (size_t i = 0; i < L.size(); ++i) { if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) ok = false; m = std::max (m, (double) std::max (fabs (L[i]), fabs (R[i]))); }
            check (ok && m < 8.0, "PHASER: feedback at the top, a loud noise in: finite, bounded (peak)", m);
        }
    }

    // ---- the engine: the sends reach the effects; at 0 they change nothing
    Engine* e = new Engine();
    FxRig Rg { e };
    {
        // (the bench's null tests cover the projects of v67: here, a track sending to each)
        const auto x = sine (700.0, 2.0, 0.4);
        std::vector<float> A, As, B, Bs, C, Cs;
        Rg.base (-1); Rg.source (x); Rg.play(); Rg.render (1.5, A, As);
        Rg.base (-1); Rg.e->setParam (0, P_CHO, 1.0f); Rg.source (x); Rg.play(); Rg.render (1.5, B, Bs);
        Rg.base (-1); Rg.e->setParam (0, P_PHS, 1.0f); Rg.source (x); Rg.play(); Rg.render (1.5, C, Cs);
        double dc = 0, dp = 0; for (size_t i = 4800; i < A.size(); ++i) { dc += (B[i] - A[i]) * (B[i] - A[i]) + (Bs[i] - As[i]) * (Bs[i] - As[i]); dp += (C[i] - A[i]) * (C[i] - A[i]) + (Cs[i] - As[i]) * (Cs[i] - As[i]); }
        check (dc > 1e-2 && dp > 1e-2, "the engine: SEND CHO and PHS reach the CHORUS and the PHASER (energy of the difference)", std::min (dc, dp));
        Rg.base (-1); Rg.e->setParam (0, P_CHO, 1.0f); Rg.e->setGlobal (G_CHON, 0.0f); Rg.source (x); Rg.play(); B.clear(); Bs.clear(); Rg.render (1.5, B, Bs);
        check (nullDb (B, A, 4800) < -200.0, "the engine: CHORUS off, its send is not heard (difference, dB)", nullDb (B, A, 4800));
    }

    // ---- the DELAY's 1982 model (a model change fades out and wipes the memory first: 50 ms of silence before each)
    {
        auto setup = [&] (int x2, int dtim, float model = 0.75f) {
            Rg.base (-1); Rg.e->setParam (0, P_DLY, 1.0f);
            Rg.e->setGlobal (G_DMDL, model); Rg.e->setGlobal (G_DBW, x2 ? 0.75f : 0.25f); Rg.e->setGlobal (G_DTIM, (dtim + 0.5f) / NUM_DLY_TIMES);
            Rg.e->setGlobal (G_DFB, 0.0f); Rg.e->setGlobal (G_DTON, 0.5f); Rg.e->setGlobal (G_DPNG, 0.0f);
            std::vector<float> M, S; Rg.render (0.2, M, S);
        };
        auto onset = [&] (const std::vector<float>& M) { for (size_t i = 0; i < M.size(); ++i) if (fabs (M[i]) > 0.02f) return (int) i; return 0; };
        // the first echo where TIME says (1/8 at 120: 250 ms), x1 and x2
        for (int x2 = 0; x2 < 2; ++x2)
        {
            setup (x2, 5);
            std::vector<float> im ((size_t) SR, 0.0f); for (int i = 0; i < 48; ++i) im[(size_t) (200 + i)] = 0.8f * (float) sin (M_PI * i / 48.0);
            Rg.source (im); Rg.play();
            std::vector<float> M, S; Rg.render (1.0, M, S);
            const int t0 = onset (M);
            int at = 0; for (size_t i = (size_t) t0 + 4000; i < M.size(); ++i) if (fabs (M[i]) > 0.02f) { at = (int) i; break; }
            const double ms = (at - t0) / 48.0;
            char w[160]; snprintf (w, sizeof w, "DELAY 1982 %s: the echo 250 ms after the sound (ms)", x2 ? "x2" : "x1");
            check (fabs (ms - 250.0) < 1.0, w, ms);
        }
        // the bandwidth: noise through one echo, its top against its middle (x1: 16 kHz, x2: 6 kHz)
        for (int x2 = 0; x2 < 2; ++x2)
        {
            setup (x2, 5);
            Rng r; r.seed (5); std::vector<float> nz ((size_t) (SR * 0.2), 0.0f); for (auto& v : nz) v = 0.3f * r.bi();
            Rg.source (nz); Rg.play();
            std::vector<float> M, S; Rg.render (0.6, M, S);
            const int a = onset (M) + 12000 + 400, b = a + 8000;
            const double mid = db (toneAmp (M, a, b, 1500)) , top = db (toneAmp (M, a, b, x2 ? 12000 : 19500));
            char w[160]; snprintf (w, sizeof w, "DELAY 1982 %s: the echo's top (%s) under its middle (dB)", x2 ? "x2" : "x1", x2 ? "12 kHz" : "19.5 kHz");
            check (top - mid < -30.0, w, top - mid);
        }
        // TIME moved while it repeats: the repeats glide down in pitch (the clock slows), then the new time
        {
            setup (0, 5); Rg.e->setGlobal (G_DFB, 0.9f);
            const auto x = sine (2000.0, 0.2, 0.4);
            Rg.source (x); Rg.play();
            std::vector<float> M, S;
            Rg.render (2.0, M, S, [&] (int b) { if (b == (int) (0.55 * 48000 / 128)) Rg.e->setGlobal (G_DTIM, 8.5f / NUM_DLY_TIMES); });
            const int t0 = onset (M);
            // the first repeat (250 .. 450 ms) at 2 kHz; the TIME change at 550 ms (1/8 to 1/4: half the clock); the second
            // repeat (500 .. 700 ms) comes out lower while the clock glides
            const double before = zcRate (M, t0 + (int) (0.27 * SR), t0 + (int) (0.43 * SR)), during = zcRate (M, (int) (0.58 * SR), (int) (0.66 * SR));
            check (before > 1900 && during < 0.9 * before, "DELAY 1982: TIME moves, the repeats glide down (their rate during the glide over before)", during / std::max (1.0, before));
        }
        // HOLD: the repeats loop as they are (no loss over 3 s), nothing new comes in (both models)
        for (int m = 0; m < 2; ++m)
        {
            setup (0, 3, m ? 0.75f : 0.25f); Rg.e->setGlobal (G_DFB, 0.5f);
            const auto x = sine (600.0, 0.1, 0.4);
            Rg.source (x); Rg.play();
            std::vector<float> M, S;
            Rg.render (5.0, M, S, [&] (int b) { if (b == (int) (0.3 * 48000 / 128)) Rg.e->setGlobal (G_DHLD, 1.0f); if (b == (int) (2.0 * 48000 / 128)) Rg.e->setParam (0, P_VOL, 0.8f); });
            const double e1 = eDb (M, (int) (0.5 * SR), (int) (1.5 * SR)), e2 = eDb (M, (int) (3.5 * SR), (int) (4.5 * SR));
            char w[160]; snprintf (w, sizeof w, "DELAY %s HOLD: the loop keeps its level over 3 s (dB)", m ? "1982" : "STD");
            check (fabs (e1 - e2) < 0.5 && e1 > -60.0, w, e1 - e2);
        }
        // the VCO: the repeats' pitch wobbles (both models)
        for (int m = 0; m < 2; ++m)
        {
            setup (0, 5, m ? 0.75f : 0.25f);
            Rg.e->setGlobal (G_DVDP, 0.3f); Rg.e->setGlobal (G_DVRT, 0.55f);
            const auto x = sine (1000.0, 1.6, 0.3);
            Rg.source (x); Rg.play();
            std::vector<float> M, S; Rg.render (2.0, M, S);
            std::vector<float> A2, As2; setup (0, 5, m ? 0.75f : 0.25f); Rg.source (x); Rg.play(); Rg.render (2.0, A2, As2);
            // (the echo alone: the take ends at 1.6 s, its repeat goes on to 1.85 s)
            const int t0 = onset (M);
            const double s1 = zcSpread (M, t0 + (int) (1.62 * SR), t0 + (int) (1.83 * SR)), s0 = zcSpread (A2, t0 + (int) (1.62 * SR), t0 + (int) (1.83 * SR));
            char w[160]; snprintf (w, sizeof w, "DELAY %s: the VCO makes the echo's pitch move (its spread with it; without: %.5f)", m ? "1982" : "STD", s0);
            check (s1 > 0.003 && s0 < 0.001, w, s1);
        }
        // a model change: it fades out, wipes, comes back, nothing wrong
        {
            setup (0, 5); Rg.e->setGlobal (G_DFB, 0.7f);
            const auto x = sine (400.0, 0.3, 0.4);
            Rg.source (x); Rg.play();
            std::vector<float> M, S; bool ok = true;
            Rg.render (2.0, M, S, [&] (int b) { if (b % 60 == 30) Rg.e->setGlobal (G_DMDL, (b / 60) % 2 ? 0.25f : 0.75f); });
            for (float v : M) if (! std::isfinite (v) || fabs (v) > 4.0f) ok = false;
            check (ok, "DELAY: the model switched every 160 ms while it repeats: finite", ok ? 1.0 : 0.0);
        }
    }

    // ---- FUZZ
    {
        auto base = [&] () { Rg.base (M_FXFUZZ); Rg.e->setParam (1, P_E, 1.0f); };
        // (1) MIX 0: the source, 87 samples late (the oversampling's round trip; its filters)
        {
            const auto x = sine (500.0, 2.0, 0.4);
            std::vector<float> A, As, B, Bs;
            Rg.base (-1); Rg.source (x); Rg.play(); Rg.render (1.5, A, As);
            base(); Rg.e->setParam (1, P_E, 0.0f); Rg.e->setParam (1, P_B, 1.0f); Rg.source (x); Rg.play(); Rg.render (1.5, B, Bs);
            int best = 0; double bd = 1e9; for (int lag = 80; lag < 95; ++lag) { const double d = nullDb (B, A, 4800, lag); if (d < bd) { bd = d; best = lag; } }
            printf ("INFO FUZZ, MIX 0: best lag %d (%.1f dB)\n", best, bd);
            const double nd = nullDb (B, A, 4800, 86);
            check (nd < -60.0, "FUZZ, MIX 0 (FUZZ at the top): the source, 86 samples late (difference, dB)", nd);
        }
        // (2) the circuit's operating point (9 V): Q2's collector about half-way (the published -4.5 V)
        // (3) FUZZ at the top: a square; at the bottom, the band clean: little distortion
        auto thd = [&] (const std::vector<float>& M, int a, int b, double f0) {
            const double h1 = toneAmp (M, a, b, f0); double hs = 0; for (int k = 2; k <= 9; ++k) hs += toneAmp (M, a, b, k * f0) * toneAmp (M, a, b, k * f0);
            return sqrt (hs) / (h1 + 1e-30); };
        {
            const auto x = sine (300.0, 2.0, 0.4);
            std::vector<float> M, S;
            // (the tone all in MID: the crossovers at their ends, LOW and HIGH clean; a sum of three bands' squares, each
            // in its phase, is not one square)
            base(); Rg.e->setParam (1, P_B, 1.0f); Rg.e->setParam (1, P_XA, 0.0f); Rg.e->setParam (1, P_XB, 0.7f); Rg.e->setParam (1, P_XC, 0.0f); Rg.e->setParam (1, P_D, 0.0f);
            Rg.e->setParam (1, P_XD, 0.0f); Rg.e->setParam (1, P_XF, 1.0f);
            Rg.source (x); Rg.play(); Rg.render (1.5, M, S);
            double pk = 0, s2 = 0; for (int i = 24000; i < 60000; ++i) { pk = std::max (pk, (double) fabs (M[(size_t) i])); s2 += M[(size_t) i] * M[(size_t) i]; }
            // (not a square: the circuit clips one side harder, and C1, charged by the base, moves the bias: a pulse)
            const double crest = pk / sqrt (s2 / 36000);
            printf ("INFO FUZZ at the top: crest factor %.2f (a sine 1.41, a square 1, a pulse of 1/4 1.73)\n", crest);
            check (thd (M, 24000, 60000, 300) > 0.4, "FUZZ at the top: clipped hard (its harmonics over its fundamental; a square's are 48 %)", thd (M, 24000, 60000, 300));
            M.clear(); S.clear();
            base(); Rg.e->setParam (1, P_B, 0.0f); Rg.e->setParam (1, P_XA, 0.0f); Rg.e->setParam (1, P_XB, 0.0f); Rg.e->setParam (1, P_XC, 0.0f);
            Rg.source (x); Rg.play(); Rg.render (1.5, M, S);
            check (thd (M, 24000, 60000, 300) < 0.03, "FUZZ at the bottom, every band clean: under 3 % of distortion", thd (M, 24000, 60000, 300));
            std::vector<float> A, As; Rg.base (-1); Rg.source (x); Rg.play(); Rg.render (1.5, A, As);
            const double lv = db (toneAmp (M, 24000, 60000, 300)) - db (toneAmp (A, 24000, 60000, 300));
            check (fabs (lv) < 0.5, "FUZZ clean: the source's level (dB against it without the FX track)", lv);
        }
        // (4) BIAS: the battery sagging, the waveform one-sided (its even harmonics against its fundamental)
        {
            const auto x = sine (200.0, 2.0, 0.3);
            double ev[2];
            for (int k = 0; k < 2; ++k)
            {
                std::vector<float> M, S;
                base(); Rg.e->setParam (1, P_B, 0.4f); Rg.e->setParam (1, P_C, k ? 0.0f : 0.5f); Rg.e->setParam (1, P_XE, 0.0f);
                Rg.e->setParam (1, P_XA, 0.67f); Rg.e->setParam (1, P_XB, 0.67f); Rg.e->setParam (1, P_XC, 0.67f);
                Rg.source (x); Rg.play(); Rg.render (1.5, M, S);
                ev[k] = (toneAmp (M, 24000, 60000, 400) + toneAmp (M, 24000, 60000, 800)) / toneAmp (M, 24000, 60000, 200);
            }
            printf ("INFO FUZZ even harmonics (2nd + 4th over the fundamental): biased %.3f, drained %.3f\n", ev[0], ev[1]);
            check (ev[1] > 0.15 && ev[1] > 1.5 * ev[0], "FUZZ BIAS: the battery drained, the wave one-sided (its even harmonics over the fundamental)", ev[1]);
        }
        // (5) the input capacitor: after a loud chord the bias shifts, a quieter note sputters out lower, then recovers
        {
            std::vector<float> x ((size_t) SR * 3, 0.0f);
            for (int i = 0; i < (int) SR; ++i) x[(size_t) i] = (float) (0.95 * sin (2 * M_PI * 110 * i / SR));
            for (int i = (int) SR; i < (int) (3 * SR); ++i) x[(size_t) i] = (float) (0.05 * sin (2 * M_PI * 220 * i / SR));
            std::vector<float> y ((size_t) SR * 3, 0.0f);
            for (int i = (int) SR; i < (int) (3 * SR); ++i) y[(size_t) i] = x[(size_t) i];
            auto go = [&] (const std::vector<float>& s, std::vector<float>& M) {
                std::vector<float> S; base(); Rg.e->setParam (1, P_B, 0.85f); Rg.e->setParam (1, P_C, 0.3f);
                Rg.e->setParam (1, P_XA, 0.67f); Rg.e->setParam (1, P_XB, 0.67f); Rg.e->setParam (1, P_XC, 0.67f);
                Rg.source (s); Rg.play(); Rg.render (2.9, M, S); };
            std::vector<float> M1, M2; go (x, M1); go (y, M2);
            const double after = eDb (M1, (int) (1.0 * SR) + 300, (int) (1.05 * SR)), clean = eDb (M2, (int) (1.0 * SR) + 300, (int) (1.05 * SR));
            const double late1 = eDb (M1, (int) (2.5 * SR), (int) (2.8 * SR)), late2 = eDb (M2, (int) (2.5 * SR), (int) (2.8 * SR));
            check (after < clean - 3.0, "FUZZ: right after a loud chord the bias has shifted, the note comes out lower (dB)", after - clean);
            check (fabs (late1 - late2) < 1.5, "FUZZ: ...and it recovers (1.5 s later, dB)", late1 - late2);
        }
        // (6) the bands: LOW clean, MID and HIGH driven: a 60 Hz sine stays clean, a 2 kHz one is fuzzed
        {
            std::vector<float> M, S;
            const auto lo = sine (60.0, 2.0, 0.3), hi = sine (2000.0, 2.0, 0.3);
            auto set = [&] () { base(); Rg.e->setParam (1, P_B, 0.7f); Rg.e->setParam (1, P_D, 0.0f); Rg.e->setParam (1, P_XD, 0.8f); Rg.e->setParam (1, P_XA, 0.0f); Rg.e->setParam (1, P_XB, 0.75f); Rg.e->setParam (1, P_XC, 0.75f); };
            set(); Rg.source (lo); Rg.play(); Rg.render (1.5, M, S);
            const double tl = thd (M, 24000, 60000, 60);
            M.clear(); S.clear();
            set(); Rg.source (hi); Rg.play(); Rg.render (1.5, M, S);
            // (2 kHz's harmonics: 6 kHz, 10 kHz)
            const double th = (toneAmp (M, 24000, 60000, 6000) + toneAmp (M, 24000, 60000, 10000)) / toneAmp (M, 24000, 60000, 2000);
            printf ("INFO FUZZ bands: 60 Hz THD %.4f\n", tl);
            check (tl < 0.05 && th > 0.15, "FUZZ bands: LOW clean (60 Hz THD), MID / HIGH driven (2 kHz's odd harmonics against it)", th);
        }
        // (7) GATE: after the source stops, the fuzz's tail is cut
        {
            std::vector<float> x ((size_t) SR * 2, 0.0f);
            Rng r; r.seed (3);
            for (int i = 0; i < (int) (SR * 0.5); ++i) x[(size_t) i] = (float) (0.4 * sin (2 * M_PI * 150 * i / SR));
            for (int i = (int) (SR * 0.5); i < (int) (SR * 2); ++i) x[(size_t) i] = 0.0015f * r.bi();      // (a hiss under -50 dB)
            double tail[2];
            for (int k = 0; k < 2; ++k)
            {
                std::vector<float> M, S;
                base(); Rg.e->setParam (1, P_B, 1.0f); Rg.e->setParam (1, P_D, 0.6f); Rg.e->setParam (1, P_XG, k ? 0.5f : 0.0f);
                Rg.source (x); Rg.play(); Rg.render (1.8, M, S);
                tail[k] = eDb (M, (int) (1.0 * SR), (int) (1.7 * SR));
            }
            check (tail[1] < tail[0] - 30.0, "FUZZ GATE: the hiss under the fuzz, gated (dB under it without the gate)", tail[1] - tail[0]);
        }
        // (8) STRV: a trig drains the battery for DEC (the screen's bias drops, then comes back)
        {
            base(); Rg.e->setParam (1, P_C, 0.5f); Rg.e->setParam (1, P_XE, 0.75f); Rg.e->setParam (1, P_DEC, 0.4f);
            const auto x = sine (200.0, 2.0, 0.3);
            Rg.source (x); Rg.play();
            std::vector<float> M, S; float lo = 1.0f, last = 0.0f;
            float info[FX_INFO];
            Rg.render (1.5, M, S, [&] (int b) { if (b == 100) Rg.e->audition (1, 1.0f); Rg.e->fxInfo (1, info); if (b > 100 && b < 140) lo = std::min (lo, info[11]); last = info[11]; });
            check (lo < 0.15f && last > 0.45f, "FUZZ STRV: a trig drains the battery (the bias it reached), then it comes back", lo);
        }
        // (9) everything at the top and at the bottom, a loud noise: finite
        {
            bool ok = true;
            for (float v : { 0.0f, 1.0f })
            {
                base();
                for (int q : { P_B, P_C, P_D, P_TUNE, P_XA, P_XB, P_XC, P_XD, P_XF, P_XG }) Rg.e->setParam (1, q, v);
                Rng r; r.seed (11); std::vector<float> x ((size_t) SR * 2); for (auto& s : x) s = 0.95f * r.bi();
                Rg.source (x); Rg.play();
                std::vector<float> M, S; Rg.render (1.5, M, S);
                for (size_t i = 0; i < M.size(); ++i) if (! std::isfinite (M[i]) || fabs (M[i]) > 8.0f) ok = false;
            }
            check (ok, "FUZZ: every knob at the top, then at the bottom, a loud noise: finite", ok ? 1 : 0);
        }
        // (10) its cost
        {
            base(); Rg.e->setParam (1, P_B, 0.9f); Rg.e->setParam (1, P_D, 0.5f);
            Rng r; r.seed (2); std::vector<float> x ((size_t) SR * 2); for (auto& s : x) s = 0.3f * r.bi();
            Rg.source (x, 0, true); Rg.play();
            std::vector<float> M, S;
            auto t0 = std::chrono::high_resolution_clock::now(); Rg.render (4.0, M, S); auto t1 = std::chrono::high_resolution_clock::now();
            Rg.base (-1); Rg.source (x, 0, true); Rg.play(); M.clear(); S.clear();
            auto t2 = std::chrono::high_resolution_clock::now(); Rg.render (4.0, M, S); auto t3 = std::chrono::high_resolution_clock::now();
            const double pct = (std::chrono::duration<double> (t1 - t0).count() - std::chrono::duration<double> (t3 - t2).count()) / 4.0 * 100.0;
            printf ("INFO FUZZ costs %.2f %% of a core (native)\n", pct);
        }
    }
    // the circuit's tables: how long they took once
    delete e;
    printf ("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}

// ----------------------------------------------------------------- v70: TONE's visualiser against the engine
// a sine through track 1's filter and its TONE (the RAM machine plays it), its level against the same sine with both
// at rest: the gains the visualiser draws (web/src/app.js fltDb, toneDb) come from these filters; printed as RESP
// lines (tools/test_web.js holds them and checks the page's curves against them)
static double AMP70 = 0.001;     // (small: the filters as drawn, their saturators idle; loud, the MS-20 squashes its own peak)
static int v70test()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-92s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    Engine* e = new Engine();
    FxRig Rg { e };
    auto level = [&] (double hz, const std::function<void ()>& set) {
        std::vector<float> x ((size_t) (48000 * 1.2));
        for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (AMP70 * sin (2 * M_PI * hz * i / 48000.0));
        std::vector<float> M, S;
        Rg.base (-1); e->setParam (0, P_FENV, 0.5f); e->setParam (0, P_DRV, 0.0f);
        set ();
        Rg.source (x); Rg.play(); Rg.render (1.0, M, S);
        return toneAmp (M, 24000, 46000, hz);
    };
    const char* FT[] = { "LP", "BP", "HP", "COMB" };
    struct Case { int ft; float fn, rs; };
    const Case cases[] = { { 0, 0.6f, 0.0f }, { 0, 0.6f, 0.5f }, { 0, 0.6f, 0.85f }, { 1, 0.6f, 0.5f }, { 2, 0.6f, 0.0f }, { 2, 0.6f, 0.5f } };
    double worst = 0;
    for (const Case& k : cases)
    {
        const double fc = 20.0 * exp2 (k.fn * 9.9658);
        for (double r : { 0.25, 0.5, 1.0, 2.0, 4.0 })
        {
            const double f = fc * r;
            const double a0 = level (f, [] () {});
            const double a1 = level (f, [&] () { e->setParam (0, P_FTYP, (k.ft + 0.5f) / 4.0f); e->setParam (0, P_FREQ, k.fn); e->setParam (0, P_RES, k.rs); });
            printf ("RESP flt %s %.3f %.3f %.3f %.2f\n", FT[k.ft], k.fn, k.rs, r, db (a1 / a0));
            worst = std::max (worst, fabs (db (a1 / a0)));
        }
    }
    {
        const double fc = 20.0 * exp2 (0.5 * 9.9658);
        for (double r : { 1.0, 1.5, 2.0 })
        {
            const double f = fc * r;
            const double a0 = level (f, [] () {});
            const double a1 = level (f, [&] () { e->setParam (0, P_FTYP, 3.5f / 4.0f); e->setParam (0, P_FREQ, 0.5f); e->setParam (0, P_RES, 0.7f); });
            printf ("RESP flt COMB 0.500 0.700 %.3f %.2f\n", r, db (a1 / a0));
        }
    }
    // TONE: HP at 0.3, LP at 0.5, LOW / MID / HIGH +12 dB at their middles
    struct Eq { const char* name; int p; float v; double f; };
    const Eq eqs[] = { { "HP", P_HPF, 0.3f, 60.0 }, { "HP", P_HPF, 0.3f, 121.0 }, { "HP", P_HPF, 0.3f, 400.0 }, { "LP", P_LPF, 0.5f, 1414.0 }, { "LP", P_LPF, 0.5f, 4000.0 },
        { "LOW", P_LOW, 0.9f, 50.0 }, { "LOW", P_LOW, 0.9f, 126.0 }, { "LOW", P_LOW, 0.9f, 1000.0 }, { "MID", P_MID, 0.9f, 1095.0 }, { "MID", P_MID, 0.9f, 300.0 },
        { "HIGH", P_HIGH, 0.9f, 4743.0 }, { "HIGH", P_HIGH, 0.9f, 12000.0 }, { "HIGH", P_HIGH, 0.9f, 500.0 } };
    for (const Eq& q : eqs)
    {
        const double a0 = level (q.f, [] () {});
        const double a1 = level (q.f, [&] () { e->setParam (0, q.p, q.v); });
        printf ("RESP eq %s %.3f %.1f %.2f\n", q.name, q.v, q.f, db (a1 / a0));
    }
    check (worst > 3.0, "the filters measured (some cut, some peak: the RESP lines)", worst);
    delete e;
    printf ("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}

// ----------------------------------------------------------------- v71: FUZZ's SUB page, HALO's SUB
static double estHz (const std::vector<float>& x, int a, int b)
{
    // rising zero crossings (interpolated) over [a, b): the mean period
    double first = -1, last = -1; int n = 0;
    for (int i = a + 1; i < b; ++i) if (x[(size_t) i - 1] < 0.0f && x[(size_t) i] >= 0.0f)
    {
        const double t = (i - 1) + x[(size_t) i - 1] / (x[(size_t) i - 1] - x[(size_t) i]);
        if (first < 0) first = t; last = t; ++n;
    }
    return n > 1 ? 48000.0 * (n - 1) / (last - first) : 0.0;
}
// ----------------------------------------------------------------- v74: CUBE, the dual wavetable voice
// the energy off the harmonics of f0 (dB under the total) in x[a, a + 16384): Blackman-Harris, the bins within 4 of
// each harmonic counted as it, up to 20 kHz
static double offHarm (const std::vector<float>& x, int a, double f0)
{
    const int N = 16384;
    std::vector<float> y ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const double t = 2 * M_PI * i / N;
        y[(size_t) i] = (float) ((a + i < (int) x.size() ? x[(size_t) (a + i)] : 0.0) * (0.35875 - 0.48829 * cos (t) + 0.14128 * cos (2 * t) - 0.01168 * cos (3 * t)));
    }
    const auto m = magSpec (y, 0, N);     // (its Hann on top: the sidelobes far under -100 dB, the main lobe +-6 bins)
    double tot = 0, on = 0;
    const double bin = 48000.0 / N;
    for (int b = 1; b < N / 2 && b * bin < 20000.0; ++b)
    {
        const double e = m[(size_t) b] * m[(size_t) b];
        tot += e;
        const double h = b * bin / f0, d = fabs (h - floor (h + 0.5)) * f0 / bin;
        if (d <= 8.0 && h > 0.5) on += e;
    }
    return 10 * log10 ((tot - on) / (tot + 1e-30) + 1e-30);
}
static int cubetest (bool tbl)
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-92s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    auto thd = [&] (const std::vector<float>& M, int a, int b, double f0) {
        const double h1 = toneAmp (M, a, b, f0); double hs = 0; for (int k = 2; k <= 12; ++k) if (k * f0 < 20000.0) hs += toneAmp (M, a, b, k * f0) * toneAmp (M, a, b, k * f0);
        return sqrt (hs) / (h1 + 1e-30); };
    auto rmsOf = [] (const std::vector<float>& x, int a, int b) { double s2 = 0; for (int i = a; i < b; ++i) s2 += (double) x[(size_t) i] * x[(size_t) i]; return sqrt (s2 / std::max (1, b - a)); };
    Engine* e = new Engine();
    const auto c0 = std::chrono::steady_clock::now();
    e->init (48000.f, 1);
    const double tInit = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - c0).count();
    const auto c1 = std::chrono::steady_clock::now();
    e->init (48000.f, 1);
    const double tInit2 = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - c1).count();
    printf ("INFO the engine's first init (lite), CUBE's factory cube made in it: %.1f ms, the next %.1f ms (native)\n", tInit, tInit2);
    float sd[NUM_SIDES]; machineSideDefaults (M_CUBE, sd);
    // one hit (or a held note: DEC at its top) on track 1, its settings, its NOTE
    auto cube = [&] (double secs, float note, const std::function<void ()>& set, const std::function<void (int)>& each = nullptr) {
        e->init (48000.f, 1);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setParam (0, P_MACH, machineVal (M_CUBE)); e->setParam (0, P_VOL, 0.8f);
        for (int q = 0; q < NUM_SIDES; ++q) e->setParam (0, sideParam (q), sd[q]);
        e->setParam (0, P_TUNE, 0.5f); e->setParam (0, P_DEC, 1.0f);
        e->setParam (0, P_A, 0.0f); e->setParam (0, P_B, 0.0f); e->setParam (0, P_C, 0.0f); e->setParam (0, P_D, 0.0f); e->setParam (0, P_E, 0.5f); e->setParam (0, P_F, 0.0f);
        e->setGlobal (G_EQON, 0.0f); e->setGlobal (G_COON, 0.0f); e->setGlobal (G_DRON, 0.0f); e->g[G_MXON] = 0.0f;
        e->setParam (0, P_HPF, 0.0f); e->setParam (0, P_LPF, 1.0f); e->setParam (0, P_FREQ, 1.0f);
        set ();
        e->audition (0, 1.0f, note);
        std::vector<float> L; std::vector<float> l (128), r (128);
        const int blocks = (int) (secs * 48000 / 128);
        for (int b = 0; b < blocks; ++b) { if (each) each (b); e->process (l.data(), r.data(), 128, nullptr); for (int i = 0; i < 128; ++i) L.push_back (0.5f * (l[i] + r[i])); }
        return L; };
    const double C2 = 65.41;
    // the default: X Y Z at 0, a sine on C2
    {
        const auto s = cube (1.0, 0.0f, [] () {});
        const double hz = estHz (s, 9600, 48000);
        printf ("INFO CUBE at rest: %.2f Hz, THD %.4f, RMS %.3f\n", hz, thd (s, 9600, 48000, hz), rmsOf (s, 9600, 48000));
        check (fabs (hz - C2) < 0.2 && thd (s, 9600, 48000, C2) < 0.01, "CUBE at rest (X Y Z 0): a sine on C2 (Hz; its distortion under 1 %)", hz);
    }
    // the shapes along X (BASIC): the square (X 3/7) odd harmonics only, the saw (2/7) all of them
    {
        const auto sq = cube (1.0, 12.0f, [&] () { e->setParam (0, P_A, 3.0f / 7.0f); });
        const auto sw = cube (1.0, 12.0f, [&] () { e->setParam (0, P_A, 2.0f / 7.0f); });
        const double f = 2 * C2;
        const double sq2 = db (toneAmp (sq, 9600, 48000, 2 * f) / toneAmp (sq, 9600, 48000, f)), sq3 = db (toneAmp (sq, 9600, 48000, 3 * f) / toneAmp (sq, 9600, 48000, f));
        const double sw2 = db (toneAmp (sw, 9600, 48000, 2 * f) / toneAmp (sw, 9600, 48000, f));
        printf ("INFO CUBE BASIC: the square's 2nd %.1f dB, 3rd %.1f dB; the saw's 2nd %.1f dB\n", sq2, sq3, sw2);
        check (sq2 < -40.0 && fabs (sq3 + 9.5) < 1.0 && fabs (sw2 + 6.0) < 1.0, "CUBE along X: a square (its 3rd at -9.5 dB, no 2nd), a saw (its 2nd at -6 dB)", sq3);
    }
    // band-limited: a saw on C7 (2093 Hz), what folds back
    {
        const auto hi = cube (1.0, 60.0f, [&] () { e->setParam (0, P_A, 2.0f / 7.0f); });
        const double oh = offHarm (hi, 12000, C2 * 32.0);
        printf ("INFO CUBE, a saw on C7: the energy off its harmonics %.1f dB\n", oh);
        check (oh < -60.0, "CUBE band-limited: a saw on C7, what folds back (dB under the whole)", oh);
        const auto sw = cube (1.0, 48.0f, [&] () { e->setParam (0, P_A, 2.0f / 7.0f); e->setParam (0, P_D, 0.0f); });
        check (offHarm (sw, 12000, C2 * 16.0) < -60.0, "CUBE band-limited: a saw on C6 (dB)", offHarm (sw, 12000, C2 * 16.0));
    }
    // a morph along X held and swept: no click; brighter along the way
    {
        const auto mx = cube (3.0, 12.0f, [] () {}, [&] (int b) { e->setParam (0, P_A, std::min (1.0f, b * 128.0f / (2.5f * 48000.0f))); });
        const double ck = clickOf (mx, 4800, 1e-4);
        const double c1 = centroid (mx, 9600), c2 = centroid (mx, 96000);
        printf ("INFO CUBE, X swept 0 .. 1 in 2.5 s: worst click %.2f, centroid %.0f -> %.0f Hz\n", ck, c1, c2);
        check (ck < 12.0 && c2 > 2.0 * c1, "CUBE X swept: no click, brighter (the worst curvature against its surroundings)", ck);
    }
    // the banks: each one's middle different from the others
    {
        double cs[8];
        for (int z = 0; z < 8; ++z) { const auto s = cube (0.6, 12.0f, [&] () { e->setParam (0, P_A, 0.5f); e->setParam (0, P_B, 0.5f); e->setParam (0, P_C, (float) z / 7.0f); }); cs[z] = centroid (s, 9600); }
        bool differ = true; for (int a = 0; a < 8; ++a) for (int b = a + 1; b < 8; ++b) if (fabs (cs[a] - cs[b]) < 20.0) differ = false;
        printf ("INFO CUBE, the eight banks at X Y 1/2: centroids"); for (int z = 0; z < 8; ++z) printf (" %.0f", cs[z]); printf (" Hz\n");
        check (differ, "CUBE: eight banks, eight colours (their centroids apart)", 0);
    }
    // FM: B on A, thru zero: sidebands, bounded, no drift
    {
        const auto f0 = cube (1.0, 12.0f, [] () {}), f1 = cube (1.0, 12.0f, [&] () { e->setParam (0, P_D, 0.7f); e->setParam (0, P_S1, 31.5f / 49.0f); });
        const double c0 = centroid (f0, 9600), c1 = centroid (f1, 9600);
        double mean = 0; for (int i = 9600; i < 48000; ++i) mean += f1[(size_t) i]; mean /= 38400;
        float pk = 0; for (float v : f1) pk = std::max (pk, fabsf (v));
        printf ("INFO CUBE FM 70 %%, B a fifth up: centroid %.0f -> %.0f Hz, peak %.3f, mean %.5f\n", c0, c1, pk, mean);
        check (c1 > 2.5 * c0 && pk < 1.5f && fabs (mean) < 0.01, "CUBE FM: sidebands (brighter), bounded, centred", c1 / c0);
    }
    // FM on a saw (X 2/7), B an octave up, C4: what folds back (the sidebands land on the harmonics), the version chosen
    // for the sidebands' spread
    {
        const double f4 = C2 * 4.0;
        for (float dq : { 0.35f, 0.5f, 0.7f })
        {
            const auto s = cube (1.0, 24.0f, [&] () { e->setParam (0, P_A, 2.0f / 7.0f); e->setParam (0, P_D, dq); e->setParam (0, P_S1, 36.5f / 49.0f); });
            printf ("INFO CUBE FM %.0f %% on a saw, C4, B an octave up: off the harmonics %.1f dB, centroid %.0f Hz\n", dq * 100.0, offHarm (s, 12000, f4), centroid (s, 12000));
        }
        const auto s = cube (1.0, 24.0f, [&] () { e->setParam (0, P_A, 2.0f / 7.0f); e->setParam (0, P_D, 0.5f); e->setParam (0, P_S1, 36.5f / 49.0f); });
        check (offHarm (s, 12000, f4) < -45.0, "CUBE FM 50 % on a saw at C4, B an octave up: what folds back stays low (dB off the harmonics)", offHarm (s, 12000, f4));
    }
    // UNIS: A doubled, beating; B heard (BLVL) a fifth up
    {
        const auto u = cube (2.0, 12.0f, [&] () { e->setParam (0, P_S6, 1.0f); });
        double mn = 1e9, mx = 0;
        for (int w = 0; w < 18; ++w) { const double r = rmsOf (u, 9600 + w * 4000, 9600 + w * 4000 + 2000); mn = std::min (mn, r); mx = std::max (mx, r); }
        printf ("INFO CUBE UNIS at its top: its level beats %.1f dB\n", db (mx / mn));
        check (db (mx / mn) > 3.0, "CUBE UNIS: two copies of A beating (dB between the loudest and the quietest)", db (mx / mn));
        const auto b = cube (1.0, 12.0f, [&] () { e->setParam (0, P_S5, 1.0f); e->setParam (0, P_S1, 31.5f / 49.0f); });
        const double a5 = db (toneAmp (b, 9600, 48000, 2 * C2 * 1.4983) / toneAmp (b, 9600, 48000, 2 * C2));
        check (fabs (a5) < 1.0, "CUBE B heard (BLVL at the top) a fifth over A, as loud as it (dB)", a5);
    }
    // the sweep (SWEP on X): brighter at the hit, back to X after STIM; the pitch envelope down to the note
    {
        const auto s = cube (1.5, 12.0f, [&] () { e->setParam (0, P_E, 1.0f); e->setParam (0, P_S9, 0.5f); });
        const double ca = centroid (s, 0), cb = centroid (s, 48000);
        printf ("INFO CUBE SWEP at its top, STIM 50 %%: centroid %.0f at the hit, %.0f a second later\n", ca, cb);
        check (ca > 2.0 * cb, "CUBE SWEP: a sweep along X at each hit, back where X is", ca / cb);
        const auto p = cube (1.0, 12.0f, [&] () { e->setParam (0, P_S10, 0.75f); e->setParam (0, P_S11, 0.7f); });
        const double h0 = estHz (p, 0, 600), h1 = estHz (p, 24000, 48000);
        printf ("INFO CUBE PENV +24 st, PTIM 70 %% (155 ms): %.0f Hz at the hit, %.2f Hz half a second later\n", h0, h1);
        check (h0 > 1.8 * h1 && fabs (h1 - 2 * C2) < 1.0, "CUBE PENV: from up there down to the note (Hz at the hit)", h0);
    }
    // STEP (X jumps), WARP (brighter), BITS (4: its noise)
    {
        const auto a = cube (0.6, 12.0f, [&] () { e->setParam (0, P_A, 0.33f); e->setParam (0, P_S12, 0.375f); });
        const auto b = cube (0.6, 12.0f, [&] () { e->setParam (0, P_A, 2.0f / 7.0f); });
        double d = 0; for (int i = 4800; i < 24000; ++i) d = std::max (d, (double) fabsf (a[(size_t) i] - b[(size_t) i]));
        check (d < 1e-4, "CUBE STEP on X: X 0.33 plays the saw itself, unmorphed (the difference)", d);
        const auto w = cube (0.6, 12.0f, [&] () { e->setParam (0, P_S13, 0.9f); });
        check (centroid (w, 9600) > 3.0 * 2 * C2, "CUBE WARP: the sine bent, its harmonics (centroid, Hz)", centroid (w, 9600));
        const auto q = cube (0.6, 12.0f, [&] () { e->setParam (0, P_S14, 5.5f / 6.0f); });
        const auto r = cube (0.6, 12.0f, [] () {});
        std::vector<float> dq (q.size()); for (size_t i = 0; i < q.size(); ++i) dq[i] = q[i] - r[i];
        const double snr = db (rmsOf (r, 9600, 24000) / rmsOf (dq, 9600, 24000));
        check (snr > 15.0 && snr < 35.0, "CUBE BITS 4: the quantisation's noise (signal against it, dB)", snr);
    }
    // USER: a bank loaded (64 waves: a saw in all of them), FACT's while its versions are made, then its own
    {
        std::vector<short> bank ((size_t) CUBE_BANK_N);
        for (int w = 0; w < 64; ++w) for (int i = 0; i < CUBE_SRC; ++i) bank[(size_t) (w * CUBE_SRC + i)] = (short) (16000.0 * (1.0 - 2.0 * i / 256.0));
        auto userRun = [&] (bool waitBuilt) {
            e->init (48000.f, 1);
            e->cubeSetBank (0, bank.data(), CUBE_BANK_N);
            if (waitBuilt) while (e->cube.busy >= 0) e->cubeStep (8);
            for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
            e->setParam (0, P_MACH, machineVal (M_CUBE)); e->setParam (0, P_VOL, 0.8f);
            for (int q = 0; q < NUM_SIDES; ++q) e->setParam (0, sideParam (q), sd[q]);
            e->setParam (0, P_S7, 0.75f); e->setParam (0, P_DEC, 1.0f); e->setParam (0, P_A, 0.0f); e->setParam (0, P_B, 0.0f); e->setParam (0, P_C, 0.0f);
            e->setParam (0, P_D, 0.0f); e->setParam (0, P_E, 0.5f); e->setParam (0, P_TUNE, 0.5f); e->setParam (0, P_F, 0.0f);
            e->setGlobal (G_EQON, 0.0f); e->setGlobal (G_COON, 0.0f); e->setGlobal (G_DRON, 0.0f); e->g[G_MXON] = 0.0f;
            e->audition (0, 1.0f, 12.0f);
            std::vector<float> L; std::vector<float> l (128), r (128);
            for (int b = 0; b < 375; ++b) { e->process (l.data(), r.data(), 128, nullptr); for (int i = 0; i < 128; ++i) L.push_back (0.5f * (l[i] + r[i])); }
            return L; };
        const auto first = userRun (false), built = userRun (true);
        const double f = 2 * C2;
        const double s2a = db (toneAmp (first, 0, 1200, 2 * f) / toneAmp (first, 0, 1200, f)), s2b = db (toneAmp (built, 9600, 48000, 2 * f) / toneAmp (built, 9600, 48000, f));
        printf ("INFO CUBE USER: right after the load the 2nd harmonic %.1f dB (FACT's sine), built %.1f dB (its saw)\n", s2a, s2b);
        check (s2a < -30.0 && fabs (s2b + 6.0) < 1.0, "CUBE USER: a bank loaded plays FACT's until its versions are made, then its own (2nd, dB)", s2b);
        int steps = 0; e->init (48000.f, 1); e->cubeSetBank (3, bank.data(), CUBE_BANK_N);
        std::vector<float> l (128), r (128);
        while (e->cube.busy >= 0 && steps < 1000) { e->process (l.data(), r.data(), 128, nullptr); ++steps; }
        check (e->cube.ready[3] == 2 && steps <= 20, "CUBE USER: a bank's versions made as the engine runs (blocks of 128)", steps);
        e->cubeClear (3);
        check (e->cube.ready[3] == 0, "CUBE USER: cleared, FACT's bank again", e->cube.ready[3]);
    }
    // every knob drawn, 8 tracks, ratchets: no NaN, bounded; its cost
    {
        e->init (48000.f, 1);
        unsigned rs = 21; auto rnd = [&] () { rs = rs * 1664525u + 1013904223u; return (rs >> 8) / 16777216.0f; };
        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            e->setParam (t, P_MACH, machineVal (M_CUBE)); e->setParam (t, P_VOL, 0.5f);
            for (int p : { P_A, P_B, P_C, P_D, P_E, P_DEC, P_TUNE }) e->setParam (t, p, rnd());
            for (int q = 0; q < 14; ++q) e->setParam (t, sideParam (q), rnd());
            for (int s2 = 0; s2 < 16; ++s2) { e->setTrig (t, s2, 1); e->setRatchet (t, s2, 1 + (int) (rnd() * 4)); }
        }
        e->setPlay (1);
        std::vector<float> L (128), R (128); bool bad = false; float pk = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < 48000 * 6; k += 128) { e->process (L.data(), R.data(), 128, nullptr); for (int q = 0; q < 128; ++q) { if (! std::isfinite (L[q]) || ! std::isfinite (R[q])) bad = true; pk = std::max (pk, fabsf (L[q])); } }
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        e->setPlay (0);
        check (! bad && pk > 0.01f && pk < 4.0f, "CUBE: 8 tracks, ratchets, every knob drawn: no NaN, bounded (peak)", pk);
        printf ("INFO 8 CUBE tracks: %.1f %% of real time (native)\n", 100.0 * ms / 6000.0);
    }
    (void) tbl;
    delete e;
    printf ("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}
// ----------------------------------------------------------------- v76: WEST, a west coast voice
static int westtest()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-96s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    auto rmsOf = [] (const std::vector<float>& x, int a, int b) { double s2 = 0; for (int i = a; i < b; ++i) s2 += (double) x[(size_t) i] * x[(size_t) i]; return sqrt (s2 / std::max (1, b - a)); };
    auto pkOf = [] (const std::vector<float>& x, int a, int b) { float p = 0; for (int i = a; i < b && i < (int) x.size(); ++i) p = std::max (p, fabsf (x[(size_t) i])); return (double) p; };
    Engine* e = new Engine(); e->init (48000.f, 0);
    float sd[NUM_SIDES]; machineSideDefaults (M_WEST, sd);
    const double C3 = 130.81;
    // v78: the side places (core/sk_west.cpp): S1 .. S35, then S36 ..
    enum { XFAD = P_S1, FOLD, GLID, DRFT, TAMT, VELO, WARM, MWAV, MDST, MKEY, MFRQ, IAMT, TMOD, CHB,
           CNT, BNCE, ACNT, SWNG, PROB, JITR, RAND, V1, V2, V3, V4, V5, LEN, DEST, G1MD, RESO, ATK, SUST, EMOD, SMIX, SDCY };
    enum { WAVE = P_S36, CKEY, PSRC, PAMT, TSRC, ISRC, FSRC, FAMT, RTRG, IVSR, ETRG, ECRV, PTRG, STRG, SPLS,
           G1LV, G1SR, G1AM, G2MD, G2IN, G2LV, G2SR, G2AM, CHA, PCSR, PCAM, SSIZ, SREF, SSPN, SDMP, SCHA, SSHK };
    // (the switches' places: sources SVS EG MO RAND PULS ED PRES INV; G1MD BOTH RESO VCA LPF; EMOD TRAN SUST CYCL OFF)
    auto src = [] (int k) { return (k + 0.5f) / 8.0f; };
    auto q4 = [] (int k) { return (k + 0.5f) / 4.0f; };
    auto q3 = [] (int k) { return (k + 0.5f) / 3.0f; };
    std::vector<float> R;
    std::vector<std::array<float, WS_INFO>> inf;    // (each block of 128)
    // one note on track 1: the page's defaults (no spring, PULS off), unless set
    auto ws = [&] (double secs, float note, const std::function<void ()>& set, const std::function<void (int)>& each = nullptr) {
        e->init (48000.f, 0);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setParam (0, P_MACH, machineVal (M_WEST)); e->setParam (0, P_VOL, 0.8f);
        for (int q = 0; q < NUM_SIDES; ++q) e->setParam (0, sideParam (q), sd[q]);
        e->setParam (0, SMIX, 0.0f);
        e->setParam (0, P_TUNE, 0.5f); e->setParam (0, P_DEC, 0.4f);
        e->setParam (0, P_A, 0.15f); e->setParam (0, P_B, 0.5f); e->setParam (0, P_C, 0.2f); e->setParam (0, P_D, 5.5f / 16.0f); e->setParam (0, P_E, 0.5f / 11.0f); e->setParam (0, P_F, 0.0f);
        e->setGlobal (G_EQON, 0.0f); e->setGlobal (G_COON, 0.0f); e->setGlobal (G_DRON, 0.0f); e->g[G_MXON] = 0.0f;
        e->setParam (0, P_HPF, 0.0f); e->setParam (0, P_LPF, 1.0f); e->setParam (0, P_FREQ, 1.0f);
        set ();
        e->audition (0, 1.0f, note);
        std::vector<float> L; R.clear(); inf.clear();
        std::vector<float> l (128), r (128);
        const int blocks = (int) (secs * 48000 / 128);
        for (int b = 0; b < blocks; ++b)
        {
            if (each) each (b);
            e->process (l.data(), r.data(), 128, nullptr);
            for (int i = 0; i < 128; ++i) { L.push_back (0.5f * (l[i] + r[i])); R.push_back (0.5f * (l[i] - r[i])); }
            std::array<float, WS_INFO> a; e->westInfo (0, a.data()); inf.push_back (a);
        }
        return L; };
    // a held tone (TIMB 0, no modulation, no CV on TIMB or MOD, DEC on HOLD): the voice's raw tone
    auto pure = [&] () { e->setParam (0, P_A, 0.0f); e->setParam (0, P_C, 0.0f); e->setParam (0, TAMT, 0.5f); e->setParam (0, IAMT, 0.5f); e->setParam (0, VELO, 0.0f); e->setParam (0, DRFT, 0.0f); e->setParam (0, WARM, 0.0f); e->setParam (0, P_DEC, 1.0f); };
    const float CELL = 0.75f, SINE = 0.25f;
    auto div = [] (int i) { return (i + 0.5f) / 11.0f; };
    auto cnt = [] (int i) { return (i + 0.5f) / 17.0f; };
    auto rat = [] (int i) { return (i + 0.5f) / 16.0f; };
    auto most = [&] (int k) { float m = 0; for (auto& a : inf) m = std::max (m, a[k]); return m; };

    // ---- a fresh track: it sounds, bounded, centred; the light up at once, out on its own; the voice ends
    {
        const auto s = ws (3.0, 0.0f, [] () {});
        double mean = 0; for (int i = 0; i < 48000; ++i) mean += s[(size_t) i]; mean /= 48000;
        const double y15 = inf[5][1], y15s = inf[(size_t) (1.5 * 48000 / 128)][1];
        printf ("INFO WEST as a fresh track has it: RMS %.3f (0.3 s), peak %.3f, mean %.5f; the light %.2f at 13 ms, %.4f at 1.5 s; playing at 2.9 s %.0f\n",
                rmsOf (s, 0, 14400), pkOf (s, 0, (int) s.size()), mean, y15, y15s, inf.back()[0]);
        check (rmsOf (s, 0, 14400) > 0.03 && pkOf (s, 0, (int) s.size()) < 1.0 && fabs (mean) < 0.002, "WEST fresh: it sounds, bounded, centred (RMS over 0.3 s)", rmsOf (s, 0, 14400));
        check (y15 > 0.4 && y15s < 0.01 && inf.back()[0] < 0.5f, "WEST: gate 1's vactrol lit within 13 ms, dark by 1.5 s, the voice over (the light at 13 ms)", y15);
    }
    // ---- the folders: CELL a sine at TIMB 0, brighter pushed in, clean at C6; SYMM's evens; WARM; SINE (EaseLFO's)
    {
        auto cell = [&] () { pure (); e->setParam (0, FOLD, CELL); };
        const auto a = ws (1.0, 0.0f, cell);
        const double f1 = toneAmp (a, 9600, 9600 + 19200, C3), f2 = toneAmp (a, 9600, 28800, 2 * C3), f3 = toneAmp (a, 9600, 28800, 3 * C3);
        const auto b = ws (1.0, 0.0f, [&] () { cell (); e->setParam (0, P_A, 0.7f); });
        const double ca = centroid (a, 9600), cb = centroid (b, 9600);
        printf ("INFO WEST CELL TIMB 0: 2nd %.1f dB, 3rd %.1f dB under the note; centroid %.0f Hz, at TIMB 70 %% %.0f Hz\n", db (f2 / f1), db (f3 / f1), ca, cb);
        check (db (f3 / f1) < -35.0 && db (f2 / f1) < -50.0, "WEST CELL TIMB 0: a sine (the 3rd under the note, dB)", db (f3 / f1));
        check (cb > 4.0 * ca, "WEST CELL TIMB: pushed into the folder, brighter (centroid ratio)", cb / ca);
        const auto c6 = ws (1.0, 36.0f, [&] () { cell (); e->setParam (0, P_A, 1.0f); });
        const auto c6b = ws (1.0, 36.0f, [&] () { cell (); e->setParam (0, P_A, 0.6f); e->setParam (0, P_B, 0.8f); });
        const double oa = offHarm (c6, 9600, 8.0 * C3), ob = offHarm (c6b, 9600, 8.0 * C3);
        printf ("INFO WEST CELL at C6, TIMB 100 %%: off the harmonics %.1f dB; TIMB 60 %% SYMM 80 %%: %.1f dB\n", oa, ob);
        check (oa < -45.0 && ob < -45.0, "WEST CELL: folded hard at C6, what folds back stays low (dB off the harmonics)", std::max (oa, ob));
        const auto sy = ws (1.0, 0.0f, [&] () { cell (); e->setParam (0, P_A, 0.4f); e->setParam (0, P_B, 0.85f); });
        const auto s0 = ws (1.0, 0.0f, [&] () { cell (); e->setParam (0, P_A, 0.4f); });
        const double e0 = db (toneAmp (s0, 9600, 28800, 2 * C3) / toneAmp (s0, 9600, 28800, C3)), e1 = db (toneAmp (sy, 9600, 28800, 2 * C3) / toneAmp (sy, 9600, 28800, C3));
        check (e0 < -50.0 && e1 > -25.0, "WEST SYMM: centred only odd harmonics, off centre the even ones (the 2nd, dB)", e1);
        const auto w0 = ws (1.0, 12.0f, [&] () { cell (); e->setParam (0, P_A, 0.7f); });
        const auto w1 = ws (1.0, 12.0f, [&] () { cell (); e->setParam (0, P_A, 0.7f); e->setParam (0, WARM, 1.0f); });
        check (centroid (w0, 9600) > 1.4 * centroid (w1, 9600), "WEST WARM: the output stage darkens it (centroid ratio)", centroid (w0, 9600) / centroid (w1, 9600));
        // SINE: EaseLFO's timbre, sin (8 (3 TIMB + 0.125) x): at 0 near a sine, then up to 25 times round
        const auto sa = ws (1.0, 0.0f, [&] () { pure (); e->setParam (0, FOLD, SINE); });
        const auto sb = ws (1.0, 0.0f, [&] () { pure (); e->setParam (0, FOLD, SINE); e->setParam (0, P_A, 0.4f); });
        const double h3 = db (toneAmp (sa, 9600, 28800, 3 * C3) / toneAmp (sa, 9600, 28800, C3));
        const auto s6 = ws (1.0, 36.0f, [&] () { pure (); e->setParam (0, FOLD, SINE); e->setParam (0, P_A, 0.6f); });
        const double oh = offHarm (s6, 9600, 8.0 * C3);
        printf ("INFO WEST FOLD SINE: TIMB 0, the 3rd %.1f dB under the note; centroid %.0f Hz at TIMB 40 %%; at C6, TIMB 60 %%: %.1f dB off the harmonics\n", h3, centroid (sb, 9600), oh);
        check (h3 < -20.0 && centroid (sb, 9600) > 4.0 * centroid (sa, 9600) && oh < -45.0, "WEST FOLD SINE: near a sine at 0, much brighter pushed in, clean at C6 (dB off the harmonics)", oh);
    }
    // ---- (v78) the complex oscillator's other wave: XFAD to TRI (odd harmonics, -19 dB the 3rd), SQR (-9.5 dB), SPKE
    // (the evens too); unfolded
    {
        auto xw = [&] (float w) { return [&, w] () { pure (); e->setParam (0, XFAD, 1.0f); e->setParam (0, WAVE, w); e->setParam (0, P_A, 0.8f); }; };
        const auto t = ws (1.0, 0.0f, xw (q3 (0))), q = ws (1.0, 0.0f, xw (q3 (1))), k = ws (1.0, 0.0f, xw (q3 (2)));
        auto h = [&] (const std::vector<float>& x, int n) { return db (toneAmp (x, 9600, 28800, n * C3) / toneAmp (x, 9600, 28800, C3)); };
        printf ("INFO WEST XFAD 100 %%: TRI 2nd %.1f 3rd %.1f dB; SQR 2nd %.1f 3rd %.1f dB; SPKE 2nd %.1f 3rd %.1f dB (TIMB 80 %%: not folded)\n", h (t, 2), h (t, 3), h (q, 2), h (q, 3), h (k, 2), h (k, 3));
        check (fabs (h (t, 3) + 19.1) < 2.0 && h (t, 2) < -40.0 && fabs (h (q, 3) + 9.5) < 1.5 && h (q, 2) < -35.0 && h (k, 2) > -6.0,
               "WEST XFAD: the folded sine to the other wave, TRI SQR SPKE as they are (the 3rds, dB)", h (q, 3));
    }
    // ---- the gate's control circuit (EaseLFO's): closed at 0 V, some 500 Hz at 5 V, open at 10 V, the zener's bend
    {
        const double r0 = westRf (0.0), r5 = westRf (5.0), r9 = westRf (9.0), r95 = westRf (9.5), r10 = westRf (10.0);
        bool mono = true; for (int i = 1; i <= 100; ++i) if (westRf (i * 0.1) > westRf ((i - 1) * 0.1)) mono = false;
        check (mono && r0 > 3e7 && r5 > 1.2e5 && r5 < 1.7e5 && r10 > 2.5e3 && r10 < 4.5e3 && r95 < 0.4 * r9, "WEST GATE: the control circuit, 0 .. 10 V to the cell's resistance (ohms at 5 V)", r5);
        ws (0.6, 0.0f, [&] () { e->setParam (0, P_DEC, 0.0f); });
        float pkY = 0; int at = 0; for (size_t q = 0; q < inf.size(); ++q) if (inf[q][1] > pkY) { pkY = inf[q][1]; at = (int) q; }
        const float y100 = inf[(size_t) (0.1 * 48000 / 128)][1];
        printf ("INFO WEST the vactrol pinged (DEC 0): its light at most %.2f, %.1f ms in; %.3f at 0.1 s\n", pkY, at * 128 / 48.0, y100);
        check (pkY > 0.6 && at * 128 < 480 && y100 < 0.15, "WEST GATE: the vactrol lit in a few ms, faded in a tenth of a second (its light at 0.1 s)", y100);
    }
    // ---- the modulation: FM harmonic on 2, metallic on 1.41; AM (EaseLFO's: 1 - index (0.5 + 0.5 x)): the note and its
    // sidebands; FREE: a vibrato; BAL: gate 2's input balanced by the modulator
    {
        const auto h = ws (1.0, 0.0f, [&] () { pure (); e->setParam (0, P_C, 0.6f); e->setParam (0, P_D, rat (7)); });
        const auto m = ws (1.0, 0.0f, [&] () { pure (); e->setParam (0, P_C, 0.6f); e->setParam (0, P_D, rat (5)); });
        const double oh = offHarm (h, 9600, C3), om = offHarm (m, 9600, C3);
        printf ("INFO WEST FM index 3.6: ratio 2 off the harmonics %.1f dB, ratio 1.41 %.1f dB\n", oh, om);
        check (oh < -40.0 && om > -12.0, "WEST FM: harmonic on 2, metallic on 1.41 (dB off the harmonics)", om);
        const auto am = ws (1.0, 0.0f, [&] () { pure (); e->setParam (0, MDST, q3 (1)); e->setParam (0, P_C, 1.0f); e->setParam (0, P_D, rat (6)); e->setParam (0, MWAV, q3 (2)); });
        const double n0 = toneAmp (am, 9600, 28800, C3), n1 = toneAmp (am, 9600, 28800, 2.5 * C3), n2 = toneAmp (am, 9600, 28800, 0.5 * C3);
        printf ("INFO WEST AM at its top, ratio 1.5: the note %.1f dB, 2.5x %.1f dB, 0.5x %.1f dB\n", db (n0), db (n1), db (n2));
        check (db (n1) > db (n0) - 12.0 && db (n2) > db (n0) - 12.0 && db (n0) > -40.0, "WEST AM: the note with its sum and difference (dB under the note)", db (n0) - db (n1));
        const auto fr = ws (1.0, 0.0f, [&] () { pure (); e->setParam (0, MKEY, 0.75f); e->setParam (0, MFRQ, 0.25f); e->setParam (0, P_C, 0.5f); });
        double mn = 1e9, mx = 0;
        for (int k = 0; k < 20; ++k) { const double f = estHz (fr, 9600 + k * 1600, 9600 + k * 1600 + 1600); if (f > 0) { mn = std::min (mn, f); mx = std::max (mx, f); } }
        check (mx > 1.3 * mn && mn > 30.0, "WEST MKEY FREE: the modulator at its own slow rate, a vibrato (the pitch's swing)", mx / mn);
        auto bal = [&] (float md) { return [&, md] () { pure (); e->setParam (0, MDST, md); e->setParam (0, CHA, 0.0f); e->setParam (0, CHB, 0.7071f); e->setParam (0, G2IN, q3 (2));
                                                         e->setParam (0, P_C, 1.0f); e->setParam (0, P_D, rat (10)); }; };
        const auto b0 = ws (1.0, 0.0f, bal (q3 (0))), b1 = ws (1.0, 0.0f, bal (q3 (2)));
        const double s0 = toneAmp (b0, 9600, 28800, 4.0 * C3), s1 = toneAmp (b1, 9600, 28800, 4.0 * C3);
        printf ("INFO WEST BAL, gate 2 on gate 1, ratio 3: the sum (4x) %.1f dB without BAL (FM), %.1f dB with it\n", db (s0), db (s1));
        check (db (s1) > db (toneAmp (b1, 9600, 28800, C3)) - 15.0 && rmsOf (b1, 9600, 28800) > 0.01, "WEST MDST BAL: gate 2's input balanced by the modulator (the 4x against the note, dB)", db (s1));
    }
    // ---- CKEY off: the note doesn't move it; on, it does
    {
        const auto on = ws (1.0, 12.0f, pure), off = ws (1.0, 12.0f, [&] () { pure (); e->setParam (0, CKEY, 0.75f); });
        const double fo = estHz (on, 9600, 28800), ff = estHz (off, 9600, 28800);
        printf ("INFO WEST CKEY: +12 played on %.1f Hz, off %.1f Hz\n", fo, ff);
        check (fabs (fo / C3 - 2.0) < 0.02 && fabs (ff / C3 - 1.0) < 0.02, "WEST CKEY: on the note, or not (the pitch off)", ff);
    }
    // ---- gate 1's modes: BOTH darkens as it closes, VCA only fades, LPF (EaseLFO's svf, 8 kHz) darkens most, RESO
    {
        auto g = [&] (float mode, float reso) { return [&, mode, reso] () { pure (); e->setParam (0, P_A, 0.2f); e->setParam (0, P_DEC, 0.3f); e->setParam (0, G1MD, mode); e->setParam (0, RESO, reso); }; };
        const auto c = ws (1.0, 12.0f, g (q4 (0), 0.0f)), v = ws (1.0, 12.0f, g (q4 (2), 0.0f)), l = ws (1.0, 12.0f, g (q4 (1), 0.95f)), lp = ws (1.0, 12.0f, g (q4 (3), 0.0f));
        const int late = (int) (0.12 * 48000), lateC = (int) (0.15 * 48000);
        const double rc = centroid (c, 0) / centroid (c, lateC), rv = centroid (v, 0) / centroid (v, late), rl = centroid (lp, 0) / centroid (lp, lateC);
        printf ("INFO WEST GATE 1: BOTH centroid %.0f -> %.0f Hz at 0.15 s, VCA %.0f -> %.0f Hz at 0.12 s, LPF %.0f -> %.0f Hz; RESO 95 %%: peak %.3f\n",
                centroid (c, 0), centroid (c, lateC), centroid (v, 0), centroid (v, late), centroid (lp, 0), centroid (lp, lateC), pkOf (l, 0, (int) l.size()));
        check (rc > 1.6 && rv < 1.35 && rc > 2.0 * rv && rmsOf (v, late, late + 4096) < 0.5 * rmsOf (v, 0, 4096), "WEST GATE BOTH: darker as it closes, VCA about the same colour as it fades (centroid ratios)", rc);
        check (rl > 1.5 && rmsOf (lp, 0, 4096) > 0.01, "WEST GATE LPF: the low-pass closing on its vactrol (centroid ratio)", rl);
        const auto l0 = ws (1.0, 12.0f, g (q4 (1), 0.0f));
        check (pkOf (l, 0, (int) l.size()) < 1.2 && fabs (centroid (l, 2400) - centroid (l0, 2400)) > 0.1 * centroid (l0, 2400), "WEST GATE RESO: its resonance changes the colour, bounded (peak)", pkOf (l, 0, (int) l.size()));
        ws (1.0, 0.0f, [&] () { e->setParam (0, P_DEC, 0.0f); });
        const double ys2 = inf[(size_t) (0.4 * 48000 / 128)][1];
        ws (1.5, 0.0f, [&] () { e->setParam (0, P_DEC, 0.7f); });
        const double yl = inf[(size_t) (0.5 * 48000 / 128)][1];
        check (ys2 < 0.01 && yl > 0.02, "WEST DEC: the vactrol follows the envelope, out at once at its short end (the light at 0.4 s)", ys2);
    }
    // ---- gate 2: the modulation oscillator heard (CHB), gate 1 into it, PRE (the voice's own output: bounded)
    {
        const auto m0 = ws (1.0, 0.0f, [&] () { pure (); e->setParam (0, CHA, 0.0f); e->setParam (0, CHB, 0.0f); });
        const auto m1 = ws (1.0, 0.0f, [&] () { pure (); e->setParam (0, CHA, 0.0f); e->setParam (0, CHB, 0.7071f); e->setParam (0, P_D, rat (10)); });
        const double a3 = toneAmp (m1, 9600, 28800, 3.0 * C3);
        printf ("INFO WEST gate 2 on the modulator (ratio 3, CHB 0 dB): %.1f dB at 3x, %.5f RMS with both channels down\n", db (a3), rmsOf (m0, 0, 48000));
        check (rmsOf (m0, 0, 48000) < 1e-5 && db (a3) > -30.0, "WEST GATE 2: the modulator heard through it (CHB), nothing with both down (dB at its pitch)", db (a3));
        const auto pr = ws (2.0, 0.0f, [&] () { e->setParam (0, CHB, 1.0f); e->setParam (0, G2IN, q3 (1)); e->setParam (0, G2LV, 1.0f); e->setParam (0, G2AM, 0.5f); });
        check (pkOf (pr, 0, (int) pr.size()) < 2.0 && rmsOf (pr, 0, 4800) > 0.01, "WEST GATE 2 PRE: the voice fed back into itself, bounded (peak)", pkOf (pr, 0, (int) pr.size()));
    }
    // ---- the routing: SVS on the pitch (PAMT, on the semitones), the pulser's ramp on gate 1, RANDOM, the inverter, the
    // envelope detector, PRES (the velocity)
    {
        auto sq = [&] () { pure (); e->setParam (0, P_E, div (7)); e->setParam (0, DEST, 0.875f); e->setParam (0, PSRC, src (0)); e->setParam (0, PAMT, 1.0f);
                           e->setParam (0, V1, 0.5f); e->setParam (0, V2, 0.75f); e->setParam (0, V3, 0.25f); e->setParam (0, LEN, 2.5f / 5.0f); e->setParam (0, P_DEC, 0.6f); };
        const auto s = ws (1.2, 0.0f, sq);
        const int B = 12000;
        const double f0 = estHz (s, 2000, B - 1000), f1 = estHz (s, B + 1500, 2 * B - 1000), f2 = estHz (s, 2 * B + 1500, 3 * B - 1000);
        printf ("INFO WEST PSRC SVS x 100 %% (0, +0.5, -0.5: +-12 semitones): %.1f %.1f %.1f Hz\n", f0, f1, f2);
        check (fabs (f0 / C3 - 1) < 0.01 && fabs (f1 / C3 - 2) < 0.02 && fabs (f2 / C3 - 0.5) < 0.01, "WEST PSRC SVS: the steps on the pitch through PAMT, on the semitones (the pitches)", f1 / C3);
        // the pulser's ramp as gate 1's level (EG off): each pulse lights it, falling till the next
        ws (1.0, 0.0f, [&] () { e->setParam (0, P_E, div (5)); e->setParam (0, EMOD, q4 (3)); e->setParam (0, G1SR, src (4)); });
        int ups = 0; for (size_t b = 1; b < inf.size(); ++b) if (inf[b][21] > inf[b - 1][21] + 0.2f) ++ups;
        printf ("INFO WEST G1SR PULS (EG off): gate 1's control rose %d times in 1 s; the envelope out at most %.2f\n", ups, most (23));
        check (ups >= 6 && most (23) == 0.0f && most (1) > 0.3f, "WEST G1SR PULS: the pulser's ramp opens gate 1, each pulse (rises in 1 s)", ups);
        // RANDOM drawn at each pulse; the inverter of the envelope; the detector of the voice
        ws (1.0, 0.0f, [&] () { e->setParam (0, P_E, div (5)); e->setParam (0, RTRG, q3 (2)); });
        std::vector<float> rv; for (size_t b = 0; b < inf.size(); b += 47) rv.push_back (inf[b][17]);
        bool varied = false; for (size_t k = 1; k < rv.size(); ++k) if (fabs (rv[k] - rv[k - 1]) > 0.05f) varied = true;
        check (varied, "WEST RANDOM: a new value at each pulse (RTRG PULS)", rv.size() > 1 ? rv[1] : 0);
        ws (0.5, 0.0f, [&] () { e->setParam (0, P_DEC, 1.0f); e->setParam (0, G1SR, src (7)); e->setParam (0, IVSR, (1.5f / 6.0f)); e->setParam (0, G1LV, 0.0f); });
        const float c1a = inf[10][21], ega = inf[10][23];
        printf ("INFO WEST G1SR INV (of the envelope, held at %.2f): gate 1's control %.2f; the detector at 0.3 s %.2f\n", ega, c1a, inf[110][18]);
        check (fabs (c1a - (1.0f - ega)) < 0.02f, "WEST INV: 1 - the envelope on gate 1 (its control)", c1a);
        ws (0.5, 0.0f, [&] () { e->setParam (0, P_DEC, 1.0f); e->setParam (0, P_A, 0.5f); });
        check (inf[110][18] > 0.5f, "WEST ED: the voice's own level followed (the detector at 0.3 s)", inf[110][18]);
        const auto v0 = ws (0.5, 0.0f, [&] () { e->setParam (0, G1SR, src (6)); e->setParam (0, P_DEC, 1.0f); });
        e->init (48000.f, 0);
        const double r0 = rmsOf (v0, 2400, 12000);
        check (r0 > 0.02, "WEST G1SR PRES: the velocity holds gate 1 open (RMS)", r0);
    }
    // ---- the pulser: CNT, endless, BNCE, ACNT, SWNG, PROB; PTRG SELF (it runs on through the trigs); PCSR (its period)
    {
        ws (1.5, 0.0f, [&] () { e->setParam (0, P_E, div (5)); e->setParam (0, CNT, cnt (2)); });
        const float n3 = most (5);
        ws (1.03, 0.0f, [&] () { e->setParam (0, P_E, div (5)); });
        const float ne = inf.back()[5], le = inf.back()[12];
        printf ("INFO WEST PULS 1/16 at 120 BPM: CNT 3 -> %.0f pulses in 1.5 s; endless -> %.0f in 1.03 s (left %.0f)\n", n3, ne, le);
        check (n3 == 3.0f && ne == 8.0f && le < 0.0f, "WEST PULS: CNT pulses after the trig, or endless on the tempo (pulses in 1 s)", ne);
        const auto ac = ws (1.0, 0.0f, [&] () { e->setParam (0, P_E, div (5)); e->setParam (0, ACNT, 0.9f); e->setParam (0, P_DEC, 0.1f); });
        double pk[5]; bool down = true;
        for (int k = 0; k < 5; ++k) { pk[k] = pkOf (ac, k * 6000, k * 6000 + 2000); if (k && pk[k] > pk[k - 1] * 0.95) down = false; }
        check (down && pk[4] < 0.5 * pk[0], "WEST ACNT: each pulse quieter (the fifth against the trig)", pk[4] / pk[0]);
        auto times = [&] () { std::vector<int> t; float last = 0; for (size_t b = 0; b < inf.size(); ++b) { if (inf[b][5] > last) t.push_back ((int) b * 128); last = inf[b][5]; } return t; };
        ws (1.0, 0.0f, [&] () { e->setParam (0, P_E, div (5)); e->setParam (0, SWNG, 0.5f); });
        const auto tw = times();
        const double r1 = tw.size() > 1 ? (double) tw[0] / (tw[1] - tw[0]) : 0;
        ws (3.0, 0.0f, [&] () { e->setParam (0, P_E, div (7)); e->setParam (0, BNCE, 0.85f); e->setParam (0, P_DEC, 0.15f); });
        const auto tb = times();
        bool closer = tb.size() > 4; for (size_t k = 2; k < tb.size(); ++k) if (tb[k] - tb[k - 1] > tb[k - 1] - (k >= 2 ? tb[k - 2] : 0) + 256) closer = false;
        check (r1 > 1.5 && r1 < 1.8, "WEST SWNG: long, short (the first interval against the second)", r1);
        check (closer && inf.back()[12] == 0.0f && tb.size() < 40, "WEST BNCE: the pulses close in like a ball bouncing, then come to rest (pulses)", (double) tb.size());
        const auto p0 = ws (1.0, 0.0f, [&] () { e->setParam (0, P_E, div (5)); e->setParam (0, PROB, 0.0f); e->setParam (0, P_DEC, 0.0f); });
        const auto p1 = ws (1.0, 0.0f, [&] () { e->setParam (0, P_E, div (5)); e->setParam (0, P_DEC, 0.0f); });
        check (rmsOf (p0, 24000, 48000) < 0.05 * rmsOf (p1, 24000, 48000), "WEST PROB 0: the pulses go by unheard (RMS after 0.5 s against PROB 100 %)", rmsOf (p0, 24000, 48000) / rmsOf (p1, 24000, 48000));
        // SELF: a second trig doesn't start it over (the count goes on)
        ws (1.0, 0.0f, [&] () { e->setParam (0, P_E, div (5)); e->setParam (0, PTRG, q3 (0)); e->setParam (0, CNT, cnt (3)); }, [&] (int b) { if (b == 200) e->audition (0, 1.0f, 0.0f); });
        const float nS = inf.back()[5];
        ws (1.0, 0.0f, [&] () { e->setParam (0, P_E, div (5)); e->setParam (0, CNT, cnt (3)); }, [&] (int b) { if (b == 200) e->audition (0, 1.0f, 0.0f); });
        const float nK = inf.back()[5];
        printf ("INFO WEST PTRG: SELF %.0f pulses so far after a second trig at 0.53 s, KEY %.0f (started over)\n", nS, nK);
        check (nS >= 7.0f && nK <= 4.0f, "WEST PTRG SELF: running on its own, the trigs don't restart it (pulses counted)", nS);
        // PCSR: the envelope (held) on its period, PCAM +: faster
        ws (1.0, 0.0f, [&] () { e->setParam (0, P_E, div (7)); e->setParam (0, P_DEC, 1.0f); e->setParam (0, PCSR, src (1)); e->setParam (0, PCAM, 1.0f); });
        const float nP = inf.back()[5];
        check (nP >= 10.0f, "WEST PCSR: the envelope speeding the pulser up (pulses in 1 s at 1/8)", nP);
    }
    // ---- the sequencer: STRG KEY (a step a trig), its stage pulses firing the envelope (ETRG SEQ); GLID
    {
        ws (1.0, 0.0f, [&] () { e->setParam (0, V2, 1.0f); }, [&] (int b) { if (b == 100 || b == 200) e->audition (0, 1.0f, 0.0f); });
        const int st0 = (int) inf[50][3], st1 = (int) inf[150][3], st2 = (int) inf[250][3];
        check (st0 == 0 && st1 == 1 && st2 == 2, "WEST SEQ AUTO: the pulser off, a step a trig", st2);
        // the pulser on 1/16, steps 2 and 4 of 4 firing: the envelope pinged on those only
        ws (1.0, 0.0f, [&] () { e->setParam (0, P_E, div (5)); e->setParam (0, ETRG, q3 (2)); e->setParam (0, SPLS, (10.5f) / 32.0f); e->setParam (0, LEN, 3.5f / 5.0f); e->setParam (0, P_DEC, 0.0f); });
        int pings = 0; for (size_t b = 1; b < inf.size(); ++b) if (inf[b][4] < inf[b - 1][4]) ++pings;
        printf ("INFO WEST ETRG SEQ (SPLS on steps 2 and 4, 4 steps at 1/16): %d pings in 1 s, %.0f pulses\n", pings, inf.back()[5]);
        check (pings >= 3 && pings <= 5, "WEST SPLS: the stage pulses ping the envelope (pings in 1 s: half the pulses)", pings);
        auto sq = [&] () { e->setParam (0, P_E, div (7)); e->setParam (0, V2, 1.0f); e->setParam (0, V3, 0.0f); e->setParam (0, LEN, 2.5f / 5.0f); pure (); e->setParam (0, P_DEC, 0.6f); };
        ws (0.6, 0.0f, [&] () { sq (); e->setParam (0, GLID, 0.5f); });
        const float g1 = inf[(size_t) (12000 + 2400) / 128][10];
        check (g1 > 4.0f && g1 < 11.5f, "WEST GLID: it glides to the next step (semitones 50 ms in)", g1);
    }
    // ---- the envelope: CYCL; OFF (gate 1 shut but for its level); HOLD; ECRV; a retrig: no click
    {
        ws (2.0, 0.0f, [&] () { e->setParam (0, EMOD, q4 (2)); e->setParam (0, P_DEC, 0.2f); e->setParam (0, SUST, 0.2f); });
        int ups = 0; for (size_t b = 1; b < inf.size(); ++b) if (inf[b][2] > 0.5f && inf[b - 1][2] <= 0.5f) ++ups;
        check (ups >= 10 && inf.back()[0] > 0.5f, "WEST EMOD CYCL: round again on its own (cycles in 2 s)", ups);
        const auto off = ws (0.5, 0.0f, [&] () { e->setParam (0, EMOD, q4 (3)); });
        const auto dr = ws (2.0, 0.0f, [&] () { e->setParam (0, EMOD, q4 (3)); e->setParam (0, G1LV, 0.6f); });
        printf ("INFO WEST EMOD OFF: RMS %.5f (after its first 0.1 s); with G1LV 60 %% (a drone) %.4f at 1.5 s\n", rmsOf (off, 4800, 23000), rmsOf (dr, 72000, 96000));
        check (rmsOf (off, 4800, 23000) < 1e-4 && rmsOf (dr, 72000, 96000) > 0.01, "WEST EMOD OFF: nothing opens the gate but its level (G1LV: a drone)", rmsOf (dr, 72000, 96000));
        const auto h = ws (3.0, 0.0f, [&] () { e->setParam (0, P_DEC, 1.0f); });
        check (rmsOf (h, 2 * 48000, 3 * 48000 - 2000) > 0.5 * rmsOf (h, 4800, 24000), "WEST HOLD: the note held (RMS after 2 s against at the start)", rmsOf (h, 2 * 48000, 3 * 48000 - 2000) / rmsOf (h, 4800, 24000));
        // ECRV: the envelope out at a third of its decay, EXP (as it was) under LIN under LOG
        auto eg = [&] (float c) { ws (1.0, 0.0f, [&, c] () { e->setParam (0, P_DEC, 0.7f); e->setParam (0, ECRV, c); }); return inf[(size_t) (0.12 * 48000 / 128)][23]; };
        const float ex = eg (1.0f), li = eg (0.5f), lo = eg (0.0f);
        printf ("INFO WEST ECRV: the envelope out at 0.12 s (DEC 70 %%): EXP %.3f, LIN %.3f, LOG %.3f\n", ex, li, lo);
        check (ex < li && li < lo, "WEST ECRV: the decay's curve, EXP under LIN under LOG (LIN at 0.12 s)", li);
        const auto rt = ws (1.0, 0.0f, [&] () { e->setParam (0, P_DEC, 0.5f); }, [&] (int b) { if (b == 60) e->audition (0, 1.0f, 0.0f); });
        check (clickOf (rt, 7000, 1e-4) < 12.0, "WEST: a trig on a voice still ringing pings it again where it is, no click (the worst curvature)", clickOf (rt, 7000, 1e-4));
    }
    // ---- the spring (BYOD's): mono without it; SMIX up, a stereo tail; SDCY longer; the trig's shake (SSHK)
    {
        ws (2.0, 0.0f, [] () {});
        const double s0 = rmsOf (R, 0, 48000);
        const auto m1 = ws (2.5, 0.0f, [&] () { e->setParam (0, SMIX, 0.6f); });
        const double s1 = rmsOf (R, 0, 48000), d1 = rmsOf (m1, 0, 48000), tail = rmsOf (m1, 36000, 48000);
        const auto m2 = ws (2.5, 0.0f, [&] () { e->setParam (0, SMIX, 0.6f); e->setParam (0, SDCY, 0.95f); });
        const double tail2 = rmsOf (m2, 72000, 96000), tail1 = rmsOf (m1, 72000, 96000);
        const auto m0 = ws (1.0, 0.0f, [] () {});
        const double dry = rmsOf (m0, 36000, 48000);
        printf ("INFO WEST SMIX: sides %.5f without, %.4f at 60 %% (the middle %.4f); 0.75 s in %.5f with it, %.6f without; at 1.5 s SDCY 45 %% %.6f, 95 %% %.5f\n",
                s0, s1, d1, tail, dry, tail1, tail2);
        check (s0 < 1e-5 && s1 > 0.05 * d1 && tail > 10.0 * dry, "WEST SMIX: mono without it, with it the spring in stereo and its tail (sides against the middle)", s1 / d1);
        check (tail2 > 3.0 * tail1, "WEST SDCY: the spring rings longer (its tail at 1.5 s, against 45 %)", tail2 / (tail1 + 1e-12));
        const auto k0 = ws (0.3, 0.0f, [&] () { e->setParam (0, SMIX, 0.6f); e->setParam (0, EMOD, q4 (3)); e->setParam (0, G1LV, 0.0f); e->setParam (0, P_A, 0.0f); });
        const auto k1 = ws (0.3, 0.0f, [&] () { e->setParam (0, SMIX, 0.6f); e->setParam (0, EMOD, q4 (3)); e->setParam (0, P_A, 0.0f); e->setParam (0, SSHK, 1.0f); });
        printf ("INFO WEST SSHK: the trig knocks the tank (no voice heard): RMS %.6f without, %.5f with\n", rmsOf (k0, 0, 14400), rmsOf (k1, 0, 14400));
        check (rmsOf (k1, 0, 14400) > 1e-4 && rmsOf (k0, 0, 14400) < 1e-5, "WEST SSHK: a trig knocks the spring (its RMS, the voice silent)", rmsOf (k1, 0, 14400));
    }
    // ---- a stop lets an endless pulser go
    {
        e->init (48000.f, 0);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setParam (0, P_MACH, machineVal (M_WEST)); e->setParam (0, P_VOL, 0.8f);
        for (int q = 0; q < NUM_SIDES; ++q) e->setParam (0, sideParam (q), sd[q]);
        e->setParam (0, P_E, div (5)); e->setParam (0, P_DEC, 0.2f);
        e->setTrig (0, 0, 1); e->setLen (0, 64);
        e->setPlay (1);
        std::vector<float> L (128), RR (128); float a[WS_INFO];
        for (int k = 0; k < 48000 / 2; k += 128) e->process (L.data(), RR.data(), 128, nullptr);
        e->westInfo (0, a); const float np0 = a[5];
        e->setPlay (0);
        for (int k = 0; k < 48000 * 2; k += 128) e->process (L.data(), RR.data(), 128, nullptr);
        e->westInfo (0, a);
        printf ("INFO WEST endless pulser: %.0f pulses before the stop, playing 2 s after it %.0f\n", np0, a[0]);
        check (np0 >= 3.0f && a[0] < 0.5f, "WEST: a stop lets an endless pulser go (the voice over)", a[0]);
    }
    // ---- every knob drawn, 8 tracks, ratchets: no NaN, bounded; its cost
    {
        e->init (48000.f, 0);
        unsigned rs = 76; auto rnd = [&] () { rs = rs * 1664525u + 1013904223u; return (rs >> 8) / 16777216.0f; };
        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            e->setParam (t, P_MACH, machineVal (M_WEST)); e->setParam (t, P_VOL, 0.5f);
            for (int p : { P_A, P_B, P_C, P_D, P_E, P_DEC, P_TUNE }) e->setParam (t, p, rnd());
            for (int q = 0; q < NUM_SIDES; ++q) e->setParam (t, sideParam (q), rnd());
            for (int s2 = 0; s2 < 16; ++s2) { e->setTrig (t, s2, 1); e->setRatchet (t, s2, 1 + (int) (rnd() * 4)); }
        }
        e->setPlay (1);
        std::vector<float> L (128), RR (128); bool bad = false; float pk = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < 48000 * 6; k += 128) { e->process (L.data(), RR.data(), 128, nullptr); for (int q = 0; q < 128; ++q) { if (! std::isfinite (L[q]) || ! std::isfinite (RR[q])) bad = true; pk = std::max (pk, fabsf (L[q])); } }
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        e->setPlay (0);
        check (! bad && pk > 0.01f && pk < 4.0f, "WEST: 8 tracks, ratchets, every knob drawn: no NaN, bounded (peak)", pk);
        printf ("INFO 8 WEST tracks: %.1f %% of real time (native)\n", 100.0 * ms / 6000.0);
        double base = 0;
        for (int c = 0; c < 2; ++c)
        {
            e->init (48000.f, 0);
            for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
            e->setParam (0, P_MACH, machineVal (M_WEST)); e->setParam (0, P_VOL, 0.6f);
            for (int q = 0; q < NUM_SIDES; ++q) e->setParam (0, sideParam (q), sd[q]);
            e->setParam (0, P_A, 1.0f); e->setParam (0, P_C, 0.8f); e->setParam (0, TMOD, 0.5f); e->setParam (0, CHB, 0.5f); e->setParam (0, P_E, div (1));
            e->setParam (0, G1MD, q4 (1)); e->setParam (0, RESO, 0.7f); e->setParam (0, SMIX, 0.5f); e->setParam (0, P_DEC, 0.5f); e->setParam (0, SCHA, 1.0f);
            if (c) e->audition (0, 1.0f, 12.0f);
            const auto t2 = std::chrono::steady_clock::now();
            for (int k = 0; k < 48000 * 4; k += 128) e->process (L.data(), RR.data(), 128, nullptr);
            const double pc = 100.0 * std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t2).count() / 4000.0;
            if (! c) base = pc; else printf ("INFO one WEST voice at its heaviest (its spring on): %.2f %% of real time above the engine's own %.2f %% (native)\n", pc - base, base);
        }
    }
    delete e;
    printf ("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}
// ----------------------------------------------------------------- v78: WEFT, Mutable Instruments' Warps as an FX machine
static int wefttest()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-96s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    auto rmsOf = [] (const std::vector<float>& x, int a, int b) { double s2 = 0; b = std::min (b, (int) x.size()); for (int i = a; i < b; ++i) s2 += (double) x[(size_t) i] * x[(size_t) i]; return sqrt (s2 / std::max (1, b - a)); };
    Engine* e = new Engine();
    FxRig Rg { e };
    auto tone = [] (double hz, double secs, double a) {
        std::vector<float> x ((size_t) (48000 * secs));
        for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (a * sin (2 * M_PI * hz * i / 48000.0));
        return x; };
    const double C3 = 130.8128;
    // track 1 WEFT on track 0's sound (SRC NBR), MIX 100 %, the internal carrier (SIN) at C3, the levels at 50 %
    auto wf = [&] (const std::vector<float>& src, double secs, const std::function<void ()>& set, std::vector<float>& M, std::vector<float>& S) {
        Rg.base (M_FXWEFT);
        e->setParam (1, P_D, 0.5f / 11.0f); e->setParam (1, P_TUNE, 0.5f); e->setParam (1, P_XA, 0.5f); e->setParam (1, P_XB, 0.5f); e->setParam (1, P_XC, 0.25f);
        e->setParam (1, P_B, 0.0f); e->setParam (1, P_C, 0.5f);
        set ();
        Rg.source (src); Rg.play();
        M.clear(); S.clear();
        Rg.render (secs, M, S); };
    std::vector<float> M, S;
    const auto a440 = tone (440.0, 2.0, 0.4);
    // ---- XFADE (ALGO 0): TIMB 0 the carrier alone, TIMB 1 the modulator alone
    {
        wf (a440, 1.0, [&] () { e->setParam (1, P_C, 0.0f); }, M, S);
        const double c0 = toneAmp (M, 9600, 28800, C3), m0 = toneAmp (M, 9600, 28800, 440.0);
        wf (a440, 1.0, [&] () { e->setParam (1, P_C, 1.0f); }, M, S);
        const double c1 = toneAmp (M, 9600, 28800, C3), m1 = toneAmp (M, 9600, 28800, 440.0);
        printf ("INFO WEFT XFADE: TIMB 0 the carrier %.1f dB, the source %.1f dB; TIMB 1 the carrier %.1f dB, the source %.1f dB\n", db (c0), db (m0), db (c1), db (m1));
        check (db (c0) > db (m0) + 30.0 && db (m1) > db (c1) + 30.0 && db (c0) > -30.0 && db (m1) > -30.0, "WEFT XFADE: the carrier at one end, the source at the other (dB between them)", db (m1) - db (c1));
    }
    // ---- the direct sound as late as the effect: at TIMB 1 (the source through) MIX 50 % stays one sine, not a comb
    {
        wf (a440, 1.0, [&] () { e->setParam (1, P_C, 1.0f); e->setParam (1, P_E, 0.0f); }, M, S);
        const double d0 = rmsOf (M, 9600, 28800);
        std::vector<float> M2, S2;
        wf (a440, 1.0, [&] () { e->setParam (1, P_C, 1.0f); e->setParam (1, P_E, 1.0f); }, M2, S2);
        // (the lag between them: the best match)
        int best = 0; double bv = 1e9;
        for (int l = 0; l < 300; ++l) { double d2 = 0; for (int i = 9600; i < 19200; ++i) { const double d = M2[(size_t) i] / (rmsOf (M2, 9600, 28800) + 1e-12) - M[(size_t) (i - l)] / (d0 + 1e-12); d2 += d * d; } if (d2 < bv) { bv = d2; best = l; } }
        printf ("INFO WEFT: the effect against the direct sound (both delayed): %d samples apart\n", best);
        check (best <= 1, "WEFT: the direct sound delayed as the effect is (samples apart)", best);
        // and as late as the source itself: the sound 106 samples after track 0's
        Rg.base (-1); Rg.source (a440); Rg.play(); std::vector<float> M0, S0; Rg.render (1.0, M0, S0);
        int lag = 0; bv = 1e9;
        for (int l = 0; l < 300; ++l) { double d2 = 0; for (int i = 9600; i < 19200; ++i) { const double d = M[(size_t) i] / (d0 + 1e-12) - M0[(size_t) (i - l)] / (rmsOf (M0, 9600, 28800) + 1e-12); d2 += d * d; } if (d2 < bv) { bv = d2; lag = l; } }
        printf ("INFO WEFT: its sound %d samples after the source's (WEFT_LAT %d)\n", lag, (int) WEFT_LAT);
        check (abs (lag - (int) WEFT_LAT) <= 1, "WEFT: 106 samples late at 48 kHz, as it says (samples)", lag);
    }
    // ---- the cross-modulation algorithms: ANALOG RING (the sum and the difference), DIGITAL RING, FOLD, XOR, COMPARE
    {
        // (Warps' diode ring: the carrier through the two diodes is rectified, the sidebands on twice its pitch)
        wf (a440, 1.0, [&] () { e->setParam (1, P_B, 2.0f / 8.0f); e->setParam (1, P_C, 0.5f); }, M, S);
        const double sum = toneAmp (M, 9600, 28800, 440.0 + 2 * C3), dif = toneAmp (M, 9600, 28800, 440.0 - 2 * C3), src = toneAmp (M, 9600, 28800, 440.0);
        wf (a440, 1.0, [&] () { e->setParam (1, P_B, 3.0f / 8.0f); e->setParam (1, P_C, 0.5f); }, M, S);
        const double dsum = toneAmp (M, 9600, 28800, 440.0 + C3), ddif = toneAmp (M, 9600, 28800, 440.0 - C3), dsrc = toneAmp (M, 9600, 28800, 440.0);
        printf ("INFO WEFT ANALOG RING: 440 +- 2 C3 %.1f %.1f dB, the source %.1f dB; DIGITAL RING: 440 +- C3 %.1f %.1f dB, the source %.1f dB\n", db (sum), db (dif), db (src), db (dsum), db (ddif), db (dsrc));
        check (db (sum) > db (src) - 6.0 && db (dif) > db (src) - 6.0 && db (dsum) > db (dsrc) + 30.0 && db (ddif) > db (dsrc) + 30.0,
               "WEFT RING: the analog one's diode sidebands, the digital one's sum and difference (dB over the source)", db (dsum) - db (dsrc));
        const char* NM[6] = { "XFADE", "FOLD", "ARING", "DRING", "XOR", "COMP" };
        double cen[6]; bool ok = true;
        for (int k = 0; k < 6; ++k)
        {
            wf (a440, 0.6, [&] () { e->setParam (1, P_B, k / 8.0f); e->setParam (1, P_C, 0.6f); }, M, S);
            float pk = 0; bool bad = false; for (float x : M) { if (! std::isfinite (x)) bad = true; pk = std::max (pk, fabsf (x)); }
            cen[k] = centroid (M, 4800);
            printf ("INFO WEFT %s (ALGO %.3f, TIMB 60 %%): RMS %.4f, peak %.3f, centroid %.0f Hz\n", NM[k], k / 8.0, rmsOf (M, 4800, 28000), pk, cen[k]);
            if (bad || pk > 1.6f || rmsOf (M, 4800, 28000) < 0.005) ok = false;
        }
        check (ok, "WEFT: each algorithm sounds, bounded (no NaN, peak under 1.6)", 0);
        check (cen[1] > 1.5 * C3 && cen[4] > 2.0 * cen[0], "WEFT FOLD, XOR: brighter than the carrier and the source crossfaded (centroid ratio)", cen[4] / cen[0]);
    }
    // ---- the vocoder: the carrier (SAW) shaped by the source; its release; frozen at ALGO's top (and with FRZ)
    {
        std::vector<float> burst = tone (440.0, 2.0, 0.0);
        for (int i = 0; i < 24000; ++i) { const double t = i / 48000.0; burst[(size_t) i] = (float) (0.4 * (sin (2 * M_PI * 300 * t) + 0.5 * sin (2 * M_PI * 1200 * t + 1.0) + 0.3 * sin (2 * M_PI * 2600 * t))); }
        wf (burst, 1.2, [&] () { e->setParam (1, P_B, 0.8f); e->setParam (1, P_C, 0.5f); }, M, S);
        const double on = rmsOf (M, 9600, 24000), off = rmsOf (M, 24000 + 14400, 24000 + 19200);
        // (frozen while it plays: ALGO to its top at 0.4 s)
        std::vector<float> M2, S2;
        wf (burst, 0.01, [&] () { e->setParam (1, P_B, 0.8f); e->setParam (1, P_C, 0.5f); }, M2, S2);
        Rg.render (1.2, M2, S2, [&] (int b) { if (b == 150) e->setParam (1, P_B, 1.0f); });
        const double fz = rmsOf (M2, 24000 + 14400, 24000 + 19200);
        printf ("INFO WEFT VOCODER: RMS %.4f while the source plays, %.5f 0.3 s after it stops; frozen (ALGO 100 %%) %.4f\n", on, off, fz);
        check (on > 0.01 && off < 0.1 * on && fz > 0.3 * on, "WEFT VOCODER: the carrier through the source's bands, released after it, held at the top (frozen against playing)", fz / on);
    }
    // ---- a track as the carrier: taken (not heard by itself), cross-modulated with the source
    {
        Rg.base (M_FXWEFT);
        e->setParam (1, P_D, 5.5f / 11.0f);                // (T3: track 2, CARR's sixth: SIN TRI SAW T1 T2 T3)
        e->setParam (1, P_XA, 0.5f); e->setParam (1, P_XB, 0.5f); e->setParam (1, P_XC, 0.25f); e->setParam (1, P_B, 0.0f); e->setParam (1, P_C, 0.0f);
        e->setParam (2, P_MACH, machineVal (M_RAM)); e->setParam (2, P_VOL, 0.8f);
        e->setParam (2, P_A, 0.0f); e->setParam (2, P_B, 1.0f); e->setParam (2, P_C, 0.1f); e->setParam (2, P_DEC, 1.0f); e->setParam (2, P_E, 0.05f); e->setParam (2, P_F, 0.0f); e->setParam (2, P_D, 0.0f);
        e->setParam (1, P_A, 0.05f);
        Rg.source (a440); Rg.source (tone (660.0, 2.0, 0.4), 2); Rg.play (0); Rg.play (2);
        M.clear(); S.clear(); Rg.render (1.0, M, S);
        const double c = toneAmp (M, 9600, 28800, 660.0), m = toneAmp (M, 9600, 28800, 440.0);
        printf ("INFO WEFT carrier T3 (660 Hz), XFADE TIMB 0: the carrier %.1f dB, the source %.1f dB\n", db (c), db (m));
        e->setParam (1, P_E, 0.0f);
        M.clear(); S.clear(); Rg.render (1.0, M, S);
        const double c0 = toneAmp (M, 9600, 28800, 660.0), m0 = toneAmp (M, 9600, 28800, 440.0);
        printf ("INFO WEFT MIX 0: the carrier track %.1f dB (taken: not in the mix), the source %.1f dB\n", db (c0), db (m0));
        check (db (c) > db (m) + 30.0 && db (c0) < db (m0) - 40.0, "WEFT CARR T3: the carrier from a track, which no longer reaches the mix by itself (dB under the source at MIX 0)", db (m0) - db (c0));
    }
    // ---- SHFT: the frequency shifter (ALGO 75 %: +11.3 Hz), one sideband a side
    {
        wf (a440, 1.5, [&] () { e->setParam (1, P_XC, 0.75f); e->setParam (1, P_B, 0.75f); e->setParam (1, P_C, 1.0f); e->setParam (1, P_XB, 1.0f); e->setParam (1, P_XA, 0.0f); }, M, S);
        std::vector<float> L (M.size()), Rr (M.size());
        for (size_t i = 0; i < M.size(); ++i) { L[i] = M[i] + S[i]; Rr[i] = M[i] - S[i]; }
        const double fl = estHz (L, 24000, 72000), fr = estHz (Rr, 24000, 72000);
        printf ("INFO WEFT SHFT +11.3 Hz on 440 Hz: left %.2f Hz, right %.2f Hz\n", fl, fr);
        check (fabs (fl - 451.3) < 1.5 && fabs (fr - 428.7) < 1.5, "WEFT SHFT: shifted up on the left, down on the right (Hz, left)", fl);
    }
    // ---- every knob drawn, 4 WEFT tracks in a chain: no NaN, bounded; the cost
    {
        Rg.base (-1);
        unsigned rs = 78; auto rnd = [&] () { rs = rs * 1664525u + 1013904223u; return (rs >> 8) / 16777216.0f; };
        Rg.source (a440, 0, true); Rg.play();
        for (int t = 1; t < 5; ++t)
        {
            e->setParam (t, P_MACH, machineVal (M_FXWEFT)); e->setParam (t, P_VOL, 0.6f); e->setParam (t, P_A, 0.05f);
            for (int p : { P_B, P_C, P_D, P_E, P_TUNE, P_XA, P_XB, P_XC }) e->setParam (t, p, rnd());
        }
        std::vector<float> L (128), RR (128); bool bad = false; float pk = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < 48000 * 4; k += 128)
        {
            if ((k / 128) % 40 == 0) for (int t = 1; t < 5; ++t) for (int p : { P_B, P_C, P_D, P_XC }) e->setParam (t, p, rnd());
            e->process (L.data(), RR.data(), 128, nullptr);
            for (int q = 0; q < 128; ++q) { if (! std::isfinite (L[q]) || ! std::isfinite (RR[q])) bad = true; pk = std::max (pk, fabsf (L[q])); }
        }
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        check (! bad && pk < 6.0f, "WEFT: 4 in a chain, every knob drawn and moved: no NaN, bounded (peak)", pk);
        printf ("INFO 4 WEFT tracks: %.1f %% of real time (native)\n", 100.0 * ms / 4000.0);
    }
    delete e;
    printf ("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}
// ----------------------------------------------------------------- v78: ECRV, the amplitude envelope's curve
static int ecrvtest()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-96s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    Engine* e = new Engine();
    // the envelope's curve itself: EXP as it was, LIN a line, LOG holding then falling; the attack's
    {
        bool same = true; for (int k = 0; k <= 100; ++k) { const float x = k / 100.0f; if (envCurve (x, 2, 1.0f) != x || envCurve (x, 0, 1.0f) != x || envCurve (x, 0, 0.5f) != x) same = false; }
        check (same, "ECRV EXP: the envelope as it always was (attack and decay)", 0);
        const float xm = powf (0.001f, 0.25f);   // (a quarter of the way to -60 dB)
        const float l = envCurve (xm, 2, 0.5f), q = envCurve (xm, 2, 0.0f), a = envCurve (0.25f, 0, 0.0f);
        printf ("INFO ECRV a quarter of the way to -60 dB (EXP %.3f): LIN %.3f, LOG %.3f; the attack a quarter in, LOG %.3f\n", xm, l, q, a);
        check (fabs (l - 0.5f) < 0.01f && q > 0.75f && q > l && a > 0.6f && envCurve (powf (0.001f, 0.55f), 2, 0.5f) == 0.0f, "ECRV LIN a line (done at half), LOG holding, its attack quick then slow (LIN a quarter in)", l);
    }
    // a machine on it (BD·VA at DEC 60 %): the level 0.1 s in, EXP < LIN < LOG; the voice still ends
    {
        auto run = [&] (float c) {
            e->init (48000.f, 0);
            for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
            e->setParam (0, P_MACH, machineVal (M_SY_RAW)); e->setParam (0, P_VOL, 0.8f); e->setParam (0, P_DEC, 0.6f); e->setParam (0, P_ECRV, c);
            e->setGlobal (G_EQON, 0.0f); e->setGlobal (G_COON, 0.0f); e->setGlobal (G_DRON, 0.0f); e->g[G_MXON] = 0.0f;
            e->audition (0, 1.0f, 0.0f);
            std::vector<float> l (128), r (128), M;
            for (int b = 0; b < 48000 * 3 / 128; ++b) { e->process (l.data(), r.data(), 128, nullptr); for (int i = 0; i < 128; ++i) M.push_back (0.5f * (l[i] + r[i])); }
            return M; };
        auto rms = [] (const std::vector<float>& x, int a, int b) { double s2 = 0; for (int i = a; i < b; ++i) s2 += (double) x[(size_t) i] * x[(size_t) i]; return sqrt (s2 / (b - a)); };
        const auto ex = run (1.0f), li = run (0.5f), lo = run (0.0f);
        const double a = rms (ex, 4800, 7200), b = rms (li, 4800, 7200), c = rms (lo, 4800, 7200);
        const bool over = e->tr[0].voice[e->tr[0].cur].active == 0;
        printf ("INFO ECRV on SY·RAW (DEC 60 %%): RMS at 0.1 s EXP %.4f, LIN %.4f, LOG %.4f; the voice over at 3 s %d\n", a, b, c, (int) over);
        check (a < b && b < c && over, "ECRV: the amplitude's curve on a machine, EXP < LIN < LOG at 0.1 s (LIN / EXP)", b / a);
    }
    delete e;
    printf ("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}
// ----------------------------------------------------------------- v73: FUZZ's circuits (CIRC, S5) and DRIV
static int v73test (bool table)
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-92s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    auto thd = [&] (const std::vector<float>& M, int a, int b, double f0) {
        const double h1 = toneAmp (M, a, b, f0); double hs = 0; for (int k = 2; k <= 9; ++k) if (k * f0 < 20000.0) hs += toneAmp (M, a, b, k * f0) * toneAmp (M, a, b, k * f0);
        return sqrt (hs) / (h1 + 1e-30); };
    auto even = [&] (const std::vector<float>& M, int a, int b, double f0) {
        double he = 0, ho = 0; for (int k = 2; k <= 9; ++k) { const double h = toneAmp (M, a, b, k * f0); (k % 2 ? ho : he) += h * h; } return sqrt (he / (ho + 1e-30)); };
    static const char* CN[8] = { "FUZZ", "PRE", "SAT", "XCIT", "MID", "XSTR", "GAIN", "BENT" };
    Engine* e = new Engine();
    FxRig Rg { e };
    auto tone = [] (double hz, double secs, double a) {
        std::vector<float> x ((size_t) (48000 * secs));
        for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (a * sin (2 * M_PI * hz * i / 48000.0));
        return x; };
    // the rig's source at 0.095 reaches the FX track at about -28 dBFS: a band at AREF
    auto fz = [&] (int circ, float drive, const std::vector<float>& x, double secs, const std::function<void ()>& set) {
        std::vector<float> M, S;
        Rg.base (M_FXFUZZ);
        e->setParam (1, P_S5, (circ + 0.5f) / 8.0f); e->setParam (1, P_B, drive); e->setParam (1, P_C, 0.5f); e->setParam (1, P_D, 0.0f);
        e->setParam (1, P_XA, 0.67f); e->setParam (1, P_XB, 0.67f); e->setParam (1, P_XC, 0.67f); e->setParam (1, P_XD, 0.5f); e->setParam (1, P_XF, 0.45f);
        e->setParam (1, P_XG, 0.0f); e->setParam (1, P_TUNE, 0.5f);
        set ();
        Rg.source (x); Rg.play(); Rg.render (secs, M, S);
        return M; };
    const double hzs[3] = { 100.0, 1000.0, 5000.0 };
    std::vector<float> src[3];
    for (int b = 0; b < 3; ++b) src[b] = tone (hzs[b], 1.0, 0.095);
    // the source's own level (MIX 0)
    double ref[3];
    for (int b = 0; b < 3; ++b) { const auto m = fz (0, 0.0f, src[b], 0.8, [&] () { e->setParam (1, P_E, 0.0f); }); ref[b] = toneAmp (m, 19200, 38400, hzs[b]); }
    // per circuit, DRIV 0 .. 1: each band's level (dB against the source) and its THD
    double lv[8][6][3], th[8][6][3];
    const float ds[6] = { 0.0f, 0.15f, 0.3f, 0.5f, 0.7f, 1.0f };
    const int c0 = getenv ("V73C") ? atoi (getenv ("V73C")) : 0, c1 = getenv ("V73C") ? c0 + 1 : 8;   // (V73C=n: that circuit only, its table)
    for (int c = 0; c < 8; ++c) for (int d = 0; d < 6; ++d) for (int b = 0; b < 3; ++b) lv[c][d][b] = th[c][d][b] = 0;
    for (int c = c0; c < c1; ++c)
        for (int d = 0; d < 6; ++d)
            for (int b = 0; b < 3; ++b)
            {
                const auto m = fz (c, ds[d], src[b], 0.8, [] () {});
                lv[c][d][b] = db (toneAmp (m, 19200, 38400, hzs[b]) / ref[b]);
                th[c][d][b] = thd (m, 19200, 38400, hzs[b]);
            }
    if (table)
    {
        printf ("circuit  DRIV:  level dB (100 Hz / 1 kHz / 5 kHz)  THD %% (same)\n");
        for (int c = c0; c < c1; ++c)
            for (int d = 0; d < 6; ++d)
                printf ("%-5s %4.2f   %6.1f %6.1f %6.1f    %6.2f %6.2f %6.2f\n", CN[c], ds[d], lv[c][d][0], lv[c][d][1], lv[c][d][2], 100 * th[c][d][0], 100 * th[c][d][1], 100 * th[c][d][2]);
    }
    if (c0 == 0 && c1 == 8)
    {
        // DRIV: subtle at 0 (a few harmonics, not none), more and more of them up to its top; the level: a band at
        // AREF comes out near it while the circuit is not saturated, some dB over it pushed (FUZZ: about at it)
        bool subtle = true, grows = true, level = true; double worstT = 0, worstL = 0, worstH = 0;
        for (int c = 0; c < 8; ++c)
        {
            auto mx = [&] (int d) { return std::max (th[c][d][0], std::max (th[c][d][1], th[c][d][2])); };
            if (mx (0) < 0.001 || mx (0) > 0.04) { subtle = false; worstT = mx (0); }
            if (! (mx (5) > 5.0 * mx (0) && mx (3) > mx (1))) grows = false;
            for (int d = 0; d < 3; ++d) for (int b = 0; b < 3; ++b) if (fabs (lv[c][d][b]) > fabs (worstL)) worstL = lv[c][d][b];
            if (c != 7) for (int d = 3; d < 6; ++d) for (int b = 0; b < 3; ++b) if (fabs (lv[c][d][b]) > fabs (worstH)) worstH = lv[c][d][b];
        }
        level = fabs (worstL) < 5.0 && fabs (worstH) < 12.0;
        printf ("INFO FUZZ's level against the source, the worst: DRIV 0 .. 30 %% %.1f dB, 50 .. 100 %% %.1f dB (BENT aside)\n", worstL, worstH);
        check (subtle, "FUZZ, every circuit at DRIV 0: subtle (its harmonics between 0.1 and 4 %, the worst)", worstT);
        check (grows, "FUZZ, every circuit: more and more harmonics with DRIV (at the top 5 x at 0, at 50 % over 15 %)", 0);
        check (level, "FUZZ, every circuit: the level within 5 dB of the source to DRIV 30 %, 12 dB beyond, pushed (the worst, dB)", worstH);
        printf ("INFO FUZZ's harmonics at 1 kHz, DRIV 0 / 30 / 50 / 100 %%:");
        for (int c = 0; c < 8; ++c) printf (" %s %.1f/%.1f/%.1f/%.1f", CN[c], 100 * th[c][0][1], 100 * th[c][2][1], 100 * th[c][3][1], 100 * th[c][5][1]);
        printf ("\n");
        check (th[0][5][1] > 0.2 && th[0][0][1] < 0.03, "FUZZ (the circuit) at DRIV 0: a dirty boost (THD %), at its top the fuzz", 100 * th[0][0][1]);
        check (lv[3][5][1] > -1.0 && lv[3][5][1] < 1.0 && th[3][5][0] < 0.01 && th[3][5][1] > 0.1,
               "XCIT at its top: 1 kHz's own level kept (dB), its harmonics added, 100 Hz untouched", lv[3][5][1]);
        // BIAS: hot, more even harmonics (SAT: none at the middle); starved, quieter and a quiet tone gated
        const auto s05 = fz (2, 0.5f, src[1], 0.8, [] () {}), s09 = fz (2, 0.5f, src[1], 0.8, [&] () { e->setParam (1, P_C, 0.95f); });
        const double ev05 = even (s05, 19200, 38400, 1000.0), ev09 = even (s09, 19200, 38400, 1000.0);
        printf ("INFO FUZZ SAT, BIAS 50 / 95 %%: even against odd harmonics %.3f / %.3f\n", ev05, ev09);
        check (ev09 > 0.5 && ev05 < 0.05, "FUZZ BIAS hot (SAT, 95 %): even harmonics (against odd)", ev09);
        const auto quiet = tone (1000.0, 1.0, 0.095 * 0.06);
        const auto qn = fz (1, 0.3f, quiet, 0.8, [] () {}), qs = fz (1, 0.3f, quiet, 0.8, [&] () { e->setParam (1, P_C, 0.05f); });
        const double gq = db (toneAmp (qs, 19200, 38400, 1000.0) / toneAmp (qn, 19200, 38400, 1000.0));
        check (gq < -10.0, "FUZZ BIAS starved (PRE, 5 %): a quiet sound gated (its level against BIAS 50 %, dB)", gq);
        // MIX 0: the source, 86 samples late, whatever the circuit
        double nul = -999;
        for (int c = 1; c < 8; ++c)
        {
            std::vector<float> A, As; Rg.base (-1); Rg.source (src[1]); Rg.play(); Rg.render (0.8, A, As);
            const auto m = fz (c, 1.0f, src[1], 0.8, [&] () { e->setParam (1, P_E, 0.0f); });
            double d2 = 0, a2 = 0; for (int i = 4800; i < 38000; ++i) { const double dd = m[(size_t) i] - A[(size_t) i - 86]; d2 += dd * dd; a2 += (double) A[(size_t) i - 86] * A[(size_t) i - 86]; }
            nul = std::max (nul, 10 * log10 (d2 / a2 + 1e-30));
        }
        check (nul < -60.0, "FUZZ MIX 0, every circuit at its top: the source, 86 samples late (the worst difference, dB)", nul);
        // every circuit, every knob drawn, on 8 tracks of noise and hits: no NaN, bounded; and what each one costs
        {
            unsigned rs = 11; auto rnd = [&] () { rs = rs * 1664525u + 1013904223u; return (rs >> 8) / 16777216.0f; };
            std::vector<float> nz (48000); for (auto& v : nz) v = 0.6f * (rnd() * 2.0f - 1.0f) * (rnd() < 0.02f ? 1.0f : 0.2f);
            bool bad = false; float pkv = 0;
            for (int run = 0; run < 16; ++run)
            {
                Rg.base (M_FXFUZZ);
                for (int p : { P_B, P_C, P_D, P_TUNE, P_XA, P_XB, P_XC, P_XD, P_XF, P_XG, P_S1, P_S2, P_S3, P_S4, P_S5 }) e->setParam (1, p, rnd());
                e->setParam (1, P_XE, rnd() < 0.5f ? 0.25f : 0.75f); e->setParam (1, P_DEC, rnd());
                Rg.source (nz, 0, true); Rg.play();
                std::vector<float> M, S; Rg.render (1.0, M, S, [&] (int b) { if (b % 40 == 0) { e->setParam (1, P_S5, rnd()); e->setParam (1, P_B, rnd()); e->audition (1, 1.0f); } });
                for (float v : M) { if (! std::isfinite (v)) bad = true; pkv = std::max (pkv, fabsf (v)); }
            }
            check (! bad && pkv < 4.0f, "FUZZ, 16 runs of every knob drawn, the circuit and DRIV changed as it plays: no NaN, bounded (peak)", pkv);
            printf ("INFO FUZZ's cost by circuit (native, %% of real time, DRIV 70 %%):");
            for (int c = 0; c < 8; ++c)
            {
                Rg.base (M_FXFUZZ); e->setParam (1, P_S5, (c + 0.5f) / 8.0f); e->setParam (1, P_B, 0.7f);
                Rg.source (nz, 0, true); Rg.play();
                std::vector<float> l (128), r (128);
                const auto t0 = std::chrono::steady_clock::now();
                for (int b = 0; b < 48000 * 5 / 128; ++b) e->process (l.data(), r.data(), 128, nullptr);
                printf (" %s %.2f", CN[c], std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 5.0 * 100.0);
            }
            printf ("\n");
        }
    }
    delete e;
    printf ("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}

static int v71test()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-92s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    auto thd = [&] (const std::vector<float>& M, int a, int b, double f0) {
        const double h1 = toneAmp (M, a, b, f0); double hs = 0; for (int k = 2; k <= 9; ++k) hs += toneAmp (M, a, b, k * f0) * toneAmp (M, a, b, k * f0);
        return sqrt (hs) / (h1 + 1e-30); };
    auto rms = [] (const std::vector<float>& x, int a, int b) { double s2 = 0; for (int i = a; i < b; ++i) s2 += (double) x[(size_t) i] * x[(size_t) i]; return sqrt (s2 / std::max (1, b - a)); };
    auto diff = [] (const std::vector<float>& a, const std::vector<float>& b) { std::vector<float> d (a.size()); for (size_t i = 0; i < a.size(); ++i) d[i] = a[i] - b[i]; return d; };
    // ---- FUZZ: its SUB page (S1 SUB1, S2 SUB2, S3 GRIT, S4 PATH)
    {
        Engine* e = new Engine();
        FxRig Rg { e };
        auto tone = [] (double hz, double secs, double a, double stop = 1e9) {
            std::vector<float> x ((size_t) (48000 * secs));
            for (size_t i = 0; i < x.size(); ++i) x[i] = i < stop * 48000 ? (float) (a * sin (2 * M_PI * hz * i / 48000.0)) : 0.0f;
            return x; };
        auto fz = [&] (const std::vector<float>& x, double secs, const std::function<void ()>& set) {
            std::vector<float> M, S;
            Rg.base (M_FXFUZZ); e->setParam (1, P_B, 0.6f); e->setParam (1, P_D, 0.0f);
            set ();
            Rg.source (x); Rg.play(); Rg.render (secs, M, S);
            return M; };
        const auto x110 = tone (110.0, 2.0, 0.3);
        const auto off = fz (x110, 1.5, [] () {});
        const auto on1 = fz (x110, 1.5, [&] () { e->setParam (1, P_S1, 1.0f); });
        const auto d1 = diff (on1, off);
        const double f1 = estHz (d1, 24000, 70000), a1 = toneAmp (d1, 24000, 70000, 55.0), as = toneAmp (off, 24000, 70000, 110.0);
        printf ("INFO FUZZ SUB1 on a 110 Hz tone: %.2f Hz, %.1f dB against the fuzz's 110 Hz, THD %.4f\n", f1, db (a1 / as), thd (d1, 24000, 70000, 55.0));
        check (fabs (f1 - 55.0) < 0.5 && thd (d1, 24000, 70000, 55.0) < 0.03, "FUZZ SUB1 (PURE, CLEAN): a sine an octave under the low band (55 Hz from 110; its distortion)", f1);
        check (db (a1 / as) > 2.0, "FUZZ SUB1 at the top: over the fuzz's own fundamental (dB)", db (a1 / as));
        // (the same 9 dB quieter: about the same sub, as the fuzz is about the same)
        const auto q110 = tone (110.0, 2.0, 0.3 * 0.355), qoff = fz (q110, 1.5, [] () {}), qon = fz (q110, 1.5, [&] () { e->setParam (1, P_S1, 1.0f); });
        const double lq = db (toneAmp (diff (qon, qoff), 24000, 70000, 55.0) / a1);
        check (fabs (lq) < 3.0, "FUZZ SUB1: like the fuzz, its level hardly follows the band's (9 dB quieter in: dB out against the louder)", lq);
        const auto on2 = fz (x110, 1.5, [&] () { e->setParam (1, P_S2, 1.0f); });
        const auto d2 = diff (on2, off);
        const double f2 = estHz (d2, 24000, 70000);
        check (fabs (f2 - 27.5) < 0.5 && thd (d2, 24000, 70000, 27.5) < 0.03, "FUZZ SUB2: two octaves under (27.5 Hz from 110; its distortion)", f2);
        const auto x80 = tone (80.0, 2.0, 0.3), off80 = fz (x80, 1.5, [] () {}), on80 = fz (x80, 1.5, [&] () { e->setParam (1, P_S1, 1.0f); });
        check (fabs (estHz (diff (on80, off80), 24000, 70000) - 40.0) < 0.5, "FUZZ SUB1 follows the band's pitch (40 Hz from 80)", estHz (diff (on80, off80), 24000, 70000));
        const auto half = fz (x110, 1.5, [&] () { e->setParam (1, P_S1, 0.5f); });
        const double lh = db (toneAmp (diff (half, off), 24000, 70000, 55.0) / a1);
        check (fabs (lh + 12.04) < 1.0, "FUZZ SUB1 at 50 %: 12 dB under the top (the knob's square, dB)", lh);
        const auto gr = fz (x110, 1.5, [&] () { e->setParam (1, P_S1, 1.0f); e->setParam (1, P_S3, 1.0f); });
        const double tg = thd (diff (gr, off), 24000, 70000, 55.0);
        check (tg > 0.08, "FUZZ GRIT at the top: the analog octaver's edge (the octave's harmonics over it)", tg);
        const auto pre = fz (x110, 1.5, [&] () { e->setParam (1, P_S1, 1.0f); e->setParam (1, P_S4, 0.75f); e->setParam (1, P_XA, 0.8f); });
        const auto dp = diff (pre, off);
        printf ("INFO FUZZ SUB1 into the circuit (PATH FUZZ, LOW driven): 55 Hz %.1f dB against CLEAN's, its harmonics %.3f\n", db (toneAmp (dp, 24000, 70000, 55.0) / a1), thd (dp, 24000, 70000, 55.0));
        check (thd (dp, 24000, 70000, 55.0) > 0.2, "FUZZ PATH FUZZ: the octave through the LOW band's circuit (its harmonics)", thd (dp, 24000, 70000, 55.0));
        // MIX 0: the source as it is (the sub follows the mix); after the source stops, the sub too
        std::vector<float> A, As;
        Rg.base (-1); Rg.source (x110); Rg.play(); Rg.render (1.5, A, As);
        const auto m0 = fz (x110, 1.5, [&] () { e->setParam (1, P_E, 0.0f); e->setParam (1, P_S1, 1.0f); e->setParam (1, P_S2, 1.0f); });
        printf ("INFO FUZZ's source in the rig: 110 Hz at %.1f dBFS, the fuzz's 110 Hz %.1f dBFS, SUB1's 55 Hz %.1f dBFS\n", db (toneAmp (A, 24000, 70000, 110.0)), db (as), db (a1));
        check (nullDb (m0, A, 4800, 86) < -60.0, "FUZZ MIX 0 with SUB1 and SUB2 at the top: the source, 86 samples late (difference, dB)", nullDb (m0, A, 4800, 86));
        const auto st = fz (tone (110.0, 2.0, 0.3, 0.6), 1.5, [&] () { e->setParam (1, P_S1, 1.0f); e->setParam (1, P_S2, 1.0f); });
        const double tail = 20 * log10 (rms (st, 48000 + 24000, 48000 + 48000) / rms (st, 12000, 24000) + 1e-12);
        check (tail < -60.0, "FUZZ SUB: the source stopped, the sub gone half a second later (dB under it)", tail);
        // its cost
        {
            double cost[2];
            for (int k = 0; k < 2; ++k)
            {
                Rg.base (M_FXFUZZ); e->setParam (1, P_S1, k ? 1.0f : 0.0f); e->setParam (1, P_S2, k ? 1.0f : 0.0f); e->setParam (1, P_S3, 0.5f);
                Rg.source (x110, 0, true); Rg.play();
                std::vector<float> l (128), r (128);
                const auto t0 = std::chrono::steady_clock::now();
                for (int b = 0; b < 48000 * 10 / 128; ++b) e->process (l.data(), r.data(), 128, nullptr);
                cost[k] = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 10.0 * 100.0;
            }
            printf ("INFO FUZZ's SUB (SUB1 and SUB2) costs %.2f %% of a core (native: the engine with it %.2f %%, without %.2f %%)\n", cost[1] - cost[0], cost[1], cost[0]);
        }
        delete e;
    }
    // ---- HALO: its SUB (TONE), an octave or two (S1) under the lowest note, sine or triangle (S2)
    {
        Engine* e = new Engine();
        const float SH = 1.0f / HALO_SHAPES, PR = 1.0f / HALO_PROGS;
        auto halo = [&] (int shape, int prog, double secs, const std::function<void ()>& set) {
            e->init (48000.f, 1);
            for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
            e->setParam (0, P_MACH, machineVal (M_HALO)); e->setParam (0, P_VOL, 0.7f);
            e->setParam (0, P_A, (shape + 0.5f) * SH); e->setParam (0, P_B, (prog + 0.5f) * PR);
            e->setParam (0, P_C, 0.0f); e->setParam (0, P_D, 0.4f); e->setParam (0, P_E, 0.0f);
            e->setParam (0, P_HSWL, 0.1f); e->setParam (0, P_HDRF, 0.0f); e->setParam (0, P_HAIR, 0.0f); e->setParam (0, P_HWID, 0.0f);
            e->setParam (0, P_HARM, 0.5f / 3.0f);
            e->setScale (1453u);
            e->g[G_MXON] = 0.0f;
            set ();
            e->setTrig (0, 0, 1); e->setSpeed (0, 0); e->setPlay (1);
            std::vector<float> L; std::vector<float> l (128), r (128);
            for (int i = 0; i < (int) (secs * 48000); i += 128) { e->process (l.data(), r.data(), 128, nullptr); L.insert (L.end(), l.begin(), l.end()); }
            return L; };
        const auto base = halo (3, 0, 2.0, [] () {});
        const auto s1 = halo (3, 0, 2.0, [&] () { e->setParam (0, P_HSUB, 1.0f); });
        const auto d = diff (s1, base);
        const double hz = estHz (d, 48000, 96000);
        printf ("INFO HALO SUB at the top (AUTO): %.2f Hz, its level %.1f dB against the chord's, THD %.4f\n", hz, 20 * log10 (rms (d, 48000, 96000) / rms (base, 48000, 96000)), thd (d, 48000, 96000, hz));
        check (fabs (hz - 65.41) < 0.3 && thd (d, 48000, 96000, 65.41) < 0.01, "HALO SUB AUTO: a pure sine an octave under the lowest note (C3 -> C2, 65.4 Hz; its distortion)", hz);
        const double lv = 20 * log10 (rms (d, 48000, 96000) / rms (base, 48000, 96000));
        check (lv > -3.0 && lv < 6.0, "HALO SUB at the top: about the chord's own level (dB against it)", lv);
        // v72, SOCT: AUTO (0), NOTE (1/3 .. 2/3), -1 OCT (2/3 .. 1)
        auto subHz = [&] (float s1v, const std::function<void ()>& more) {
            const auto a = halo (3, 0, 2.0, [&] () { more(); });
            const auto b = halo (3, 0, 2.0, [&] () { more(); e->setParam (0, P_HSUB, 1.0f); e->setParam (0, P_S1, s1v); });
            return estHz (diff (b, a), 48000, 96000); };
        // NOTE: the lowest note itself, in phase with it (its fundamental grows by the sub's whole amplitude)
        const auto un = halo (3, 0, 2.0, [&] () { e->setParam (0, P_HSUB, 1.0f); e->setParam (0, P_S1, 0.5f); });
        const auto du = diff (un, base);
        const double fu = estHz (du, 48000, 96000), aB = toneAmp (base, 48000, 96000, 130.81), aU = toneAmp (un, 48000, 96000, 130.81), aS = toneAmp (du, 48000, 96000, 130.81);
        printf ("INFO HALO SUB NOTE: %.2f Hz; C3 in the chord %.4f, with the sub %.4f, the sub alone %.4f\n", fu, aB, aU, aS);
        check (fabs (fu - 130.81) < 0.5, "HALO SUB NOTE: on the lowest note itself (C3, 130.8 Hz)", fu);
        check (aU > aB + 0.9 * aS, "HALO SUB NOTE: in phase with the note (its fundamental grows by the sub's amplitude: with, against chord + sub)", aU / (aB + aS));
        check (fabs (subHz (0.85f, [] () {}) - 65.41) < 0.3, "HALO SUB -1 OCT: an octave under C3 (C2)", subHz (0.85f, [] () {}));
        // a low bass (SPRD at the top drops it to C2): AUTO doubles it as it is, -1 OCT too (32.7 Hz would be under 45)
        const double lA = subHz (0.0f, [&] () { e->setParam (0, P_C, 1.0f); }), l1 = subHz (0.85f, [&] () { e->setParam (0, P_C, 1.0f); });
        printf ("INFO HALO SUB, the bass on C2 (SPRD at the top): AUTO %.2f Hz, -1 OCT %.2f Hz\n", lA, l1);
        check (fabs (lA - 65.41) < 0.3 && fabs (l1 - 65.41) < 0.3, "HALO SUB never under 45 Hz: a bass on C2 doubled as it is (AUTO and -1 OCT, Hz)", lA);
        // a high chord (NOTE +14 degrees): -1 OCT an octave under its lowest note, AUTO as many as it takes to land
        // between 45 and 90 Hz
        const double hA = subHz (0.0f, [&] () { e->setParam (0, P_NOTE, 0.5f + 14.0f / 48.0f); }), h1 = subHz (0.85f, [&] () { e->setParam (0, P_NOTE, 0.5f + 14.0f / 48.0f); });
        const double fL = 2.0 * h1, kO = log2 (fL / hA);
        printf ("INFO HALO SUB, a high chord (its lowest note %.2f Hz): AUTO %.2f Hz (%.2f octaves down), -1 OCT %.2f Hz\n", fL, hA, kO, h1);
        check (fL > 250.0 && hA >= 45.0 && hA < 90.0 && fabs (kO - floor (kO + 0.5)) < 0.01 && kO > 1.5,
               "HALO SUB AUTO under a high chord: octaves down into 45 .. 90 Hz (Hz)", hA);
        // v72, PRES at the top: the sub's 2nd, 3rd and 4th harmonics (0.7, 0.45, 0.2 of it), its fundamental as it was
        const auto tr = halo (3, 0, 2.0, [&] () { e->setParam (0, P_HSUB, 1.0f); e->setParam (0, P_S2, 1.0f); });
        const auto dt = diff (tr, base);
        const double f1 = toneAmp (dt, 48000, 96000, 65.41), p2 = db (toneAmp (dt, 48000, 96000, 2 * 65.41) / f1), p3 = db (toneAmp (dt, 48000, 96000, 3 * 65.41) / f1),
                     p4 = db (toneAmp (dt, 48000, 96000, 4 * 65.41) / f1), pf = db (f1 / toneAmp (d, 48000, 96000, 65.41));
        printf ("INFO HALO SUB PRES at the top: 2nd %.1f dB, 3rd %.1f dB, 4th %.1f dB under the fundamental, which moves %.2f dB\n", p2, p3, p4, pf);
        check (fabs (p2 + 3.1) < 0.5 && fabs (p3 + 6.9) < 0.5 && fabs (p4 + 14.0) < 0.7 && fabs (pf) < 0.2, "HALO SUB PRES: its 2nd, 3rd, 4th harmonics over it (2nd, dB), the fundamental kept", p2);
        // the lowest note, not the root: INV 60 % puts G3 lowest; AUTO takes it to G1 (49 Hz), -1 OCT to G2 (98 Hz)
        const double iA = subHz (0.0f, [&] () { e->setParam (0, P_HINV, 0.6f); }), i1 = subHz (0.85f, [&] () { e->setParam (0, P_HINV, 0.6f); });
        printf ("INFO HALO SUB, INV 60 %%: AUTO %.2f Hz, -1 OCT %.2f Hz\n", iA, i1);
        check (fabs (iA - 49.0) < 0.5 && fabs (i1 - 98.0) < 0.5, "HALO SUB follows the lowest note, not the root (INV: G3 lowest; AUTO G1, Hz)", iA);
        const auto hf = halo (3, 0, 2.0, [&] () { e->setParam (0, P_HSUB, 0.5f); });
        const double lh = 20 * log10 (rms (diff (hf, base), 48000, 96000) / rms (d, 48000, 96000));
        check (fabs (lh + 12.04) < 0.5, "HALO SUB at 50 %: 12 dB under the top (dB)", lh);
        // through a progression: the sub moves with the chords (AUTO: by octaves too, or onto the note), no click
        const auto pg = halo (3, 4, 6.0, [&] () { e->setParam (0, P_HSUB, 1.0f); e->setSpeed (0, 4); });
        const double ck = clickOf (pg, 4800, 1e-4);
        const auto pn = halo (3, 4, 6.0, [&] () { e->setParam (0, P_HSUB, 1.0f); e->setParam (0, P_S1, 0.5f); e->setParam (0, P_S2, 1.0f); e->setSpeed (0, 4); });
        const double ckn = clickOf (pn, 4800, 1e-4);
        const auto pl = halo (3, 4, 6.0, [&] () { e->setParam (0, P_HSUB, 1.0f); e->setParam (0, P_C, 0.3f); e->setSpeed (0, 4); });
        const double ckl = clickOf (pl, 4800, 1e-4);
        printf ("INFO HALO SUB through a progression: worst click %.2f (AUTO), %.2f (NOTE, PRES at the top), %.2f (AUTO, the bass dropped)\n", ck, ckn, ckl);
        check (ck < 12.0 && ckn < 12.0 && ckl < 12.0, "HALO SUB through a progression: no click (the worst curvature against its surroundings)", std::max (ck, std::max (ckn, ckl)));
        // its cost: 20 s of the progression, SUB off then on (median of 3)
        {
            double cost[2];
            for (int k = 0; k < 2; ++k)
            {
                double run[3];
                for (int n = 0; n < 3; ++n)
                {
                    const auto t0 = std::chrono::steady_clock::now();
                    halo (3, 4, 20.0, [&] () { e->setParam (0, P_HSUB, k ? 1.0f : 0.0f); e->setSpeed (0, 4); });
                    run[n] = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 20.0 * 100.0;
                }
                std::sort (run, run + 3); cost[k] = run[1];
            }
            printf ("INFO HALO's SUB costs %.2f %% of a core (native: the engine with it %.2f %%, without %.2f %%)\n", cost[1] - cost[0], cost[1], cost[0]);
        }
        delete e;
    }
    printf ("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}

// ----------------------------------------------------------------- v80: DUCK on all of a track's sound; HOLD let go on a change
static int ducktest()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-96s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    Engine* e = new Engine();
    // T1 a kick on every beat (hard left), T2 the machine (hard right: R is T2), a trig on every 16th, DUCK 0 or 100 %:
    // T2's level in the 40 ms after the second kick, ducked against not
    auto run = [&] (int m, int kt, int dt, float dk) {
        e->init (48000.f, 0);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setGlobal (G_EQON, 0.0f); e->setGlobal (G_COON, 0.0f); e->setGlobal (G_DRON, 0.0f); e->g[G_MXON] = 0.0f;
        e->setParam (kt, P_MACH, machineVal (M_BD_VA)); e->setParam (kt, P_DEC, 0.3f); e->setParam (kt, P_VOL, 0.8f); e->setParam (kt, P_PAN, 0.0f);
        for (int s = 0; s < 16; s += 4) e->setTrig (kt, s, 1);
        e->setParam (dt, P_MACH, machineVal (m)); e->setParam (dt, P_PAN, 1.0f); e->setParam (dt, P_VOL, 0.6f); e->setParam (dt, P_DEC, 0.8f);
        { float af[6]; const int na = machineAFDefaults (m, af); for (int q = 0; q < na; ++q) e->setParam (dt, P_A + q, af[q]); }
        { float sd[NUM_SIDES]; const int ns = machineSideDefaults (m, sd); for (int q = 0; q < ns; ++q) e->setParam (dt, sideParam (q), sd[q]); }
        e->setParam (dt, P_DUCK, dk);
        for (int s = 0; s < 16; ++s) e->setTrig (dt, s, 1);
        e->setPlay (1);
        std::vector<float> L (128), R (128), out;
        for (int i = 0; i < 48000 * 2; i += 128) { e->process (L.data(), R.data(), 128, nullptr); for (int k = 0; k < 128; ++k) out.push_back (R[k]); }
        return out; };
    auto lvl = [] (const std::vector<float>& x, double a, double b) { double s2 = 0; const int i0 = (int) (a * 48000), i1 = (int) (b * 48000); for (int i = i0; i < i1; ++i) s2 += (double) x[(size_t) i] * x[(size_t) i]; return 10 * log10 (s2 / (i1 - i0) + 1e-30); };
    double worst = -1e9; int worstM = -1;
    for (int m = 0; m < NUM_MACHINES; ++m)
    {
        if (! soundMachine (m) || m == M_BD_VA) continue;
        const auto a = run (m, 0, 1, 0.0f), b = run (m, 0, 1, 1.0f);
        const double d = lvl (b, 0.505, 0.545) - lvl (a, 0.505, 0.545), base = lvl (a, 0.505, 0.545);
        printf ("INFO DUCK %-8s dry %.1f dB, ducked by %.1f dB\n", MNAMES[m], base, d);
        if (base > -70.0 && d > worst) { worst = d; worstM = m; }
    }
    check (worst < -20.0, "DUCK 100 %: every machine 20 dB under track 1, its SPACE, spring and sides too (the least ducked, dB)", worst);
    if (worstM >= 0) printf ("INFO the least ducked: %s\n", MNAMES[worstM]);
    // the kick on track 3, CHAOS on track 2: not ducked (DUCK follows track 1 only, whatever it plays); a snare on
    // track 1 ducks it
    {
        const auto a = run (M_CHAOS, 2, 1, 0.0f), b = run (M_CHAOS, 2, 1, 1.0f);
        const double d = lvl (b, 0.505, 0.545) - lvl (a, 0.505, 0.545);
        check (absf ((float) d) < 0.5, "DUCK: follows track 1 only (a kick on track 3 leaves CHAOS on track 2 as it is, dB)", d);
    }
    {
        e->init (48000.f, 0);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setGlobal (G_EQON, 0.0f); e->setGlobal (G_COON, 0.0f); e->setGlobal (G_DRON, 0.0f); e->g[G_MXON] = 0.0f;
        e->setParam (0, P_MACH, machineVal (M_SD_VA)); e->setParam (0, P_VOL, 0.8f); e->setParam (0, P_PAN, 0.0f);
        for (int st = 0; st < 16; st += 4) e->setTrig (0, st, 1);
        e->setParam (1, P_MACH, machineVal (M_SY_RAW)); e->setParam (1, P_PAN, 1.0f); e->setParam (1, P_VOL, 0.6f); e->setParam (1, P_DEC, 0.8f); e->setParam (1, P_DUCK, 1.0f);
        for (int st = 0; st < 16; ++st) e->setTrig (1, st, 1);
        e->setPlay (1);
        std::vector<float> L (128), R (128), o;
        for (int i = 0; i < 48000; i += 128) { e->process (L.data(), R.data(), 128, nullptr); for (int k = 0; k < 128; ++k) o.push_back (R[k]); }
        check (lvl (o, 0.505, 0.525) < lvl (o, 0.40, 0.42) - 12.0, "DUCK: a snare on track 1 ducks the others as a kick does (dB under)", lvl (o, 0.505, 0.525) - lvl (o, 0.40, 0.42));
    }
    // HOLD: a note held until the next trig (GATE OFF), its trig taken away, then another machine on the track: the
    // held note lets go (as on a stop)
    {
        e->init (48000.f, 0);
        e->setGlobal (G_EQON, 0.0f); e->setGlobal (G_COON, 0.0f); e->setGlobal (G_DRON, 0.0f); e->g[G_MXON] = 0.0f;
        for (int t = 1; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setParam (0, P_MACH, machineVal (M_SY_RAW)); e->setParam (0, P_DEC, 1.0f); e->setParam (0, P_GATE, 0.0f); e->setParam (0, P_VOL, 0.7f);
        e->setTrig (0, 0, 1);
        e->setPlay (1);
        std::vector<float> L (128), R (128), o;
        for (int i = 0; i < 48000 * 3; i += 128)
        {
            if (i == 48000 * 3 / 2 - (48000 * 3 / 2) % 128) { e->setTrig (0, 0, 0); }
            if (i == 48000 * 2 - (48000 * 2) % 128) e->setParam (0, P_MACH, machineVal (M_BD_FM));
            e->process (L.data(), R.data(), 128, nullptr); for (int k = 0; k < 128; ++k) o.push_back (0.5f * (L[k] + R[k]));
        }
        const double held = lvl (o, 1.8, 1.95), after = lvl (o, 2.6, 2.9);
        printf ("INFO HOLD: held %.1f dB (its trig gone), 0.6 s after another machine %.1f dB\n", held, after);
        check (held > -40.0 && after < -90.0, "HOLD: a note held, another machine on the track lets it go (dB after)", after);
    }
    // v80, Copy / Move track: a take copied onto another track (the original kept), two takes swapped (one empty)
    {
        e->init (48000.f, 0);
        ramTestTake (*e, 2);
        const short s0 = e->ramMem[2][1000]; const int v2 = e->ram[2].ver;
        e->ramCopy (2, 6);
        const bool cp = e->ram[6].len == 48000 && e->ramMem[6][1000] == s0 && e->ram[2].len == 48000 && e->ram[6].ver != 0 && e->ram[2].ver == v2;
        e->ramSwap (2, 5);
        const bool sw = e->ram[5].len == 48000 && e->ramMem[5][1000] == s0 && e->ram[2].len == 0 && e->ram[2].ver != v2 && e->ram[5].rate == 48000.0f;
        e->ramSwap (5, 2); e->ramCopy (4, 6);   // (back again; an empty take copied: the track's take cleared)
        const bool bk = e->ram[2].len == 48000 && e->ramMem[2][1000] == s0 && e->ram[5].len == 0 && e->ram[6].len == 0;
        check (cp && sw && bk, "COPY / MOVE: a RAM take copied (the original kept), swapped (and back), an empty one copied", (cp ? 1 : 0) + (sw ? 2 : 0) + (bk ? 4 : 0));
    }
    delete e;
    printf ("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}
// v81: HOWL (a guitar held up to a screaming amp, its chords) and the DRONE mode
static int howltest()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-96s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    Engine* e = new Engine();
    float SR = 48000.0f;
    auto setup = [&] (int m, float sr) {
        SR = sr;
        e->init (sr, 0);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setGlobal (G_EQON, 0.0f); e->setGlobal (G_COON, 0.0f); e->setGlobal (G_DRON, 0.0f); e->g[G_MXON] = 0.0f;
        e->setParam (0, P_MACH, machineVal (m)); e->setParam (0, P_VOL, 0.8f);
        { float af[6]; const int na = machineAFDefaults (m, af); for (int q = 0; q < na; ++q) e->setParam (0, P_A + q, af[q]); }
        { float sd[NUM_SIDES]; const int ns = machineSideDefaults (m, sd); for (int q = 0; q < ns; ++q) e->setParam (0, sideParam (q), sd[q]); }
    };
    auto side = [&] (int q, float v) { e->setParam (0, P_S1 + q, v); };
    auto run = [&] (double secs) {
        std::vector<float> L (128), R (128), o;
        for (int i = 0; i < (int) (secs * SR); i += 128) { e->process (L.data(), R.data(), 128, nullptr); for (int k = 0; k < 128; ++k) o.push_back (0.5f * (L[k] + R[k])); }
        return o;
    };
    auto lvl = [&] (const std::vector<float>& o, double a, double b) {
        double s = 0.0; int n = 0;
        for (int i = (int) (a * SR); i < (int) (b * SR) && i < (int) o.size(); ++i) { s += (double) o[i] * o[i]; ++n; }
        return 10.0 * log10 (s / (double) std::max (n, 1) + 1e-30);
    };
    // f0 by normalised autocorrelation (the first peak near the best: not its multiples)
    auto f0 = [&] (const std::vector<float>& o, double a, double b) {
        const int i0 = (int) (a * SR), n = (int) ((b - a) * SR), lo = (int) (SR / 1000.0f), hi = (int) (SR / 20.0f);
        std::vector<double> rr ((size_t) hi + 2, 0.0); double best = 0.0;
        for (int lag = lo; lag < hi; ++lag)
        {
            double sx = 0, e1 = 0, e2 = 0;
            for (int i = 0; i < n; ++i) { sx += o[i0 + i] * o[i0 + i + lag]; e1 += o[i0 + i] * o[i0 + i]; e2 += o[i0 + i + lag] * o[i0 + i + lag]; }
            rr[lag] = sx / sqrt (e1 * e2 + 1e-30); best = std::max (best, rr[lag]);
        }
        int bl = lo;
        for (int lag = lo + 1; lag < hi - 1; ++lag) if (rr[lag] > 0.92 * best && rr[lag] >= rr[lag - 1] && rr[lag] >= rr[lag + 1]) { bl = lag; break; }
        const double ym = rr[bl - 1], y0 = rr[bl], yp = rr[bl + 1], d = 0.5 * (ym - yp) / (ym - 2.0 * y0 + yp);
        return SR / (bl + d);
    };
    float info[HALO_INFO];
    // 1. in tune: one string (POLY), no feedback, no drift, a clean amp, at three rates
    {
        double worst = 0.0;
        for (float sr : { 44100.0f, 48000.0f, 96000.0f })
            for (float st : { -12.0f, 0.0f, 7.0f, 19.0f })
            {
                setup (M_HOWL, sr);
                e->setParam (0, P_A, 14.5f / 15.0f); e->setParam (0, P_C, 0.5f / 5.0f); e->setParam (0, P_D, 0.0f); e->setParam (0, P_DEC, 0.8f);
                side (HW_DRFT, 0.0f); side (HW_WAMY, 0.0f); side (HW_DRIV, 0.0f); side (HW_FUZZ, 0.0f); side (HW_RESO, 0.0f);
                e->audition (0, 1.0f, st);
                const auto o = run (1.0);
                worst = std::max (worst, fabs (1200.0 * log2 (f0 (o, 0.25, 0.6) / (82.406889 * pow (2.0, st / 12.0)))));
            }
        check (worst < 2.0, "HOWL: a string in tune (POLY, E1 .. B3, 44.1 / 48 / 96 kHz; worst, cents)", worst);
    }
    // 2. the chords: every chord, voicing, DROP and root keeps its thirds, sevenths, seconds over their low limits (no mud
    //    under the bass), six strings, within E0 .. E6; MIN9 on E1 laid as a DRONE: E1 B1 E2 B2 E3 F#3
    {
        int bad = 0, n = 0;
        setup (M_HOWL, 48000.0f); e->setParam (0, P_E, 0.0f); e->setParam (0, P_D, 0.0f);
        auto limit = [] (int iv) { switch (((iv % 12) + 12) % 12) { case 0: return 0.0f; case 7: return 23.0f; case 5: return 40.0f; case 3: case 4: case 9: return 45.0f; default: return 50.0f; } };
        for (int ty = 0; ty < 13; ++ty)
            for (int vo = 0; vo < 8; ++vo)
                for (int dr = 0; dr < 5; dr += 2)
                    for (int rt = -12; rt <= 12; rt += 5)
                    {
                        e->setParam (0, P_A, (ty + 0.5f) / 15.0f); e->setParam (0, P_B, (vo + 0.5f) / 8.0f); e->setParam (0, P_C, (dr + 0.5f) / 5.0f);
                        e->setParam (0, P_TUNE, 0.5f + (float) rt / 48.0f);
                        e->audition (0, 1.0f, 0.0f); run (0.25); e->haloInfo (0, info);      // (TUNE glides there, about 12 ms)
                        const float root = info[8];
                        int ns = 0;
                        for (int k = 0; k < 6; ++k)
                        {
                            const float p = info[12 + 3 * k]; if (p < -900.0f) continue; ++ns;
                            const int iv = (int) floorf (p - root + 0.5f);
                            if (p < 16.0f - 0.01f || p > 88.0f + 0.01f || (iv % 12 != 0 && p > root + 0.5f && p < limit (iv) - 0.01f && ! (iv % 12 == 7 && p >= 23.0f))) ++bad;
                        }
                        if (ns != 6) ++bad;
                        ++n;
                    }
        e->setParam (0, P_A, 11.5f / 15.0f); e->setParam (0, P_B, 0.5f / 8.0f); e->setParam (0, P_C, 2.5f / 5.0f); e->setParam (0, P_TUNE, 0.5f);
        e->audition (0, 1.0f, 0.0f); run (0.25); e->haloInfo (0, info);
        float sorted[6]; for (int k = 0; k < 6; ++k) sorted[k] = info[12 + 3 * k];
        std::sort (sorted, sorted + 6);
        const float want[6] = { 28, 35, 40, 47, 52, 54 };
        bool same = true; for (int k = 0; k < 6; ++k) if (fabsf (sorted[k] - want[k]) > 0.01f) same = false;
        check (bad == 0 && same, "HOWL: chords over their low limits (13 chords x 8 voicings x 3 drops x 5 roots), MIN9 DRONE on E1", (double) bad);
    }
    // 3. the loop: without a pick, FDBK low stays silent, high howls from nothing (DEC short, long, ∞); with a pick, FDBK
    //    holds what would die away
    {
        bool okS = true, okH = true; double qmax = -999.0, hmin = 999.0;
        for (float dec : { 0.2f, 0.7f, 1.0f })
        {
            setup (M_HOWL, 48000.0f); e->setParam (0, P_DEC, dec); side (HW_EXCT, 0.0f); side (HW_BLOM, 0.0f); side (HW_NOIS, 0.0f); e->setParam (0, P_D, 0.1f);
            e->audition (0, 1.0f, 0.0f); auto q = run (5.0);
            setup (M_HOWL, 48000.0f); e->setParam (0, P_DEC, dec); side (HW_EXCT, 0.0f); side (HW_BLOM, 0.0f); side (HW_NOIS, 0.0f); e->setParam (0, P_D, 0.9f);
            e->audition (0, 1.0f, 0.0f); auto hw = run (5.0);
            qmax = std::max (qmax, lvl (q, 4.0, 5.0)); hmin = std::min (hmin, lvl (hw, 4.0, 5.0));
            if (lvl (q, 4.0, 5.0) > -60.0) okS = false;
            if (lvl (hw, 4.0, 5.0) < -30.0) okH = false;
        }
        check (okS && okH, "HOWL: no pick: FDBK 10 % silent, 90 % howls from the noise floor (DEC 0.2, 0.7, ∞: quiet max / howl min, dB)", qmax - hmin);
        setup (M_HOWL, 48000.0f); e->setParam (0, P_DEC, 0.3f); e->setParam (0, P_D, 0.0f); side (HW_BLOM, 0.0f);
        e->audition (0, 1.0f, 0.0f); auto a = run (3.0);
        setup (M_HOWL, 48000.0f); e->setParam (0, P_DEC, 0.3f); e->setParam (0, P_D, 0.7f); side (HW_BLOM, 0.0f);
        e->audition (0, 1.0f, 0.0f); auto b = run (3.0);
        const double dA = lvl (a, 2.5, 3.0) - lvl (a, 0.2, 0.5), dB = lvl (b, 2.5, 3.0) - lvl (b, 0.2, 0.5);
        check (dA < -20.0 && dB > -4.0, "HOWL: a pick dies away without feedback, held with FDBK 70 % (the fall without, dB)", dA);
    }
    // 4. a new chord morphs: the strings glide (MRPH), what rings carried over (no gap)
    {
        setup (M_HOWL, 48000.0f); e->setParam (0, P_E, 0.4f); e->setParam (0, P_D, 0.6f);
        e->audition (0, 1.0f, 0.0f); auto a = run (1.0);
        e->audition (0, 1.0f, 5.0f); auto b = run (0.15); e->haloInfo (0, info);
        const float mid = info[12 + 3 * 0], gl = info[14];
        auto c = run (1.2); e->haloInfo (0, info);
        float lo = 999.0f; for (int k = 0; k < 6; ++k) lo = std::min (lo, info[12 + 3 * k]);
        const double dip = std::min (lvl (b, 0.0, 0.15), lvl (c, 0.0, 0.3)) - lvl (a, 0.6, 1.0);
        check (gl > 0.05f && gl < 0.95f && mid > 28.0f && mid < 33.0f && fabsf (lo - 33.0f) < 0.01f && dip > -6.0,
               "HOWL: E to A, MRPH 0.37 s: the strings mid-glide, then on A1; no gap (dB)", dip);
    }
    // 5. POLY: three keys, three strings; one let go: its string damped
    {
        setup (M_HOWL, 48000.0f); e->setParam (0, P_A, 14.5f / 15.0f); e->setParam (0, P_C, 0.5f / 5.0f); e->setParam (0, P_E, 0.0f); e->setParam (0, P_D, 0.0f); e->setParam (0, P_DEC, 1.0f);
        e->audition (0, 1.0f, 0.0f); e->audition (0, 1.0f, 7.0f); e->audition (0, 1.0f, 12.0f);
        run (0.5); e->haloInfo (0, info);
        int on = 0; for (int k = 0; k < 6; ++k) if (info[12 + 3 * k] > -900.0f) ++on;
        float g7 = 0.0f; for (int k = 0; k < 6; ++k) if (fabsf (info[12 + 3 * k] - 47.0f) < 0.1f) g7 = info[13 + 3 * k];
        e->release (0, 7.0f); run (1.0); e->haloInfo (0, info);
        float g7b = 0.0f, g0 = 0.0f;
        for (int k = 0; k < 6; ++k) { if (fabsf (info[12 + 3 * k] - 47.0f) < 0.1f) g7b = info[13 + 3 * k]; if (fabsf (info[12 + 3 * k] - 40.0f) < 0.1f) g0 = info[13 + 3 * k]; }
        check (on == 3 && g7 > 0.01f && g7b < 0.05f * g7 && g0 > 0.2f * g7, "HOWL: POLY, three keys three strings; one let go, its string damped (what it keeps)", (double) (g7b / std::max (g7, 1e-9f)));
    }
    // 6. the gate: a trig's GATE ends the howl (the guitar turned away, ∞: the hand on the strings); a stop ends it
    {
        setup (M_HOWL, 48000.0f); e->setParam (0, P_DEC, 1.0f); e->setParam (0, P_D, 0.9f); e->setParam (0, P_GATE, 4.0f / 8.0f);   // (16 steps: 2 s at 120)
        e->setTrig (0, 0, 1); e->setLen (0, 64); e->setPlay (1);
        auto a = run (3.5);
        e->setTrig (0, 0, 0);
        check (lvl (a, 1.4, 1.9) > -30.0 && lvl (a, 3.0, 3.5) < lvl (a, 1.4, 1.9) - 30.0, "HOWL: the GATE's end turns the guitar away (dB after it)", lvl (a, 3.0, 3.5));
        setup (M_HOWL, 48000.0f); e->setParam (0, P_DEC, 0.7f); e->setParam (0, P_D, 0.9f);
        e->setTrig (0, 0, 1); e->setLen (0, 64); e->setPlay (1);
        auto b = run (1.5); e->setPlay (0); auto c = run (1.5);
        check (lvl (b, 1.0, 1.5) > -30.0 && lvl (c, 1.0, 1.5) < -60.0, "HOWL: a stop ends the howl (dB 1 s after)", lvl (c, 1.0, 1.5));
    }
    // 7. bounded: random settings at three rates, changes on the way, every value finite and the peak in reason
    {
        int bad = 0; float worst = 0.0f;
        Rng r; r.seed (11);
        for (int it = 0; it < 45; ++it)
        {
            setup (M_HOWL, it % 3 == 0 ? 44100.0f : it % 3 == 1 ? 48000.0f : 96000.0f);
            for (int p : { P_TUNE, P_DEC, P_A, P_B, P_C, P_D, P_E }) e->setParam (0, p, r.uni());
            if (it % 4 == 0) e->setParam (0, P_D, 1.0f);
            for (int q = 0; q < HW_NSIDE; ++q) side (q, it % 5 == 0 ? (r.uni() < 0.5f ? 0.0f : 1.0f) : r.uni());
            e->audition (0, 1.0f, 0.0f);
            auto o = run (1.0);
            for (int k = 0; k < 3; ++k) { e->audition (0, r.uni(), (float) (int) (r.bi() * 12.0f)); auto o2 = run (0.5); o.insert (o.end(), o2.begin(), o2.end()); }
            for (float x : o) { if (! std::isfinite (x)) ++bad; worst = std::max (worst, fabsf (x)); }
        }
        check (bad == 0 && worst < 1.5f, "HOWL: 45 random settings, changes on the way: bounded (peak)", worst);
    }
    // 7b. (v81) RESO at its top: the bridge moves energy between the strings, never makes any. With no feedback, struck,
    // the strings die away whatever SUST (on ∞ they hold: never above their pick); with the feedback howling, RESO
    // turned up while held, the howl goes on (the first coupling rang the strings up to a DC that silenced the amp, even
    // a retrig could not wake it)
    {
        float worstRise = -100.0f, lateShort = -100.0f;
        for (float dec : { 0.55f, 0.9f, 1.0f })
        {
            setup (M_HOWL, 48000.0f); e->setParam (0, P_DEC, dec); e->setParam (0, P_D, 0.0f); side (HW_RESO, 1.0f); side (HW_NOIS, 0.0f);
            e->audition (0, 1.0f, 0.0f);
            auto o = run (12.0);
            const Voice& v = e->tr[0].voice[e->tr[0].cur];
            float mx = 0.0f; for (int k = 0; k < HOWL_STR; ++k) mx = std::max (mx, v.hw.s[k].lvl);
            worstRise = std::max (worstRise, (float) (20.0 * log10 (mx + 1e-9) - 20.0 * log10 (0.15)));      // (a pick: about 0.1)
            if (dec < 0.6f) lateShort = (float) lvl (o, 11.0, 12.0);
        }
        check (worstRise < 0.0f && lateShort < -60.0, "HOWL RESO 100 %, no feedback: no string rings itself up (SUST short, long, ∞: dB over a pick, 12 s)", worstRise);
        setup (M_HOWL, 48000.0f); e->setParam (0, P_DEC, 1.0f); e->setParam (0, P_D, 0.9f);
        e->audition (0, 1.0f, 0.0f); auto a = run (2.0);
        side (HW_RESO, 1.0f); auto b = run (8.0);
        e->audition (0, 1.0f, 0.0f); auto c = run (2.0);
        side (HW_RESO, 0.3f); auto d = run (4.0);
        const double la = lvl (a, 1.0, 2.0), lb = lvl (b, 6.0, 8.0), lc = lvl (c, 1.0, 2.0), ld = lvl (d, 2.0, 4.0);
        check (lb > la - 6.0 && lc > la - 6.0 && ld > la - 6.0, "HOWL RESO turned to 100 % while howling (SUST ∞): it goes on, a retrig, back down (dB against before)", std::min (std::min (lb, lc), ld) - la);
    }
    // the strongest peak in o[a, a + 32768): its frequency, its share of the energy (dB, within 3 % of it)
    auto peakAt = [&] (const std::vector<float>& o, double t0, double& f, double& share) {
        const size_t N = 32768, a0 = (size_t) (t0 * SR); std::vector<std::complex<double>> x (N);
        for (size_t i = 0; i < N; ++i) x[i] = (a0 + i < o.size() ? o[a0 + i] : 0.0) * (0.5 - 0.5 * cos (2 * M_PI * i / N));
        for (size_t i = 1, j = 0; i < N; ++i) { size_t bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (x[i], x[j]); }
        for (size_t len = 2; len <= N; len <<= 1) { const std::complex<double> wl (cos (-2 * M_PI / len), sin (-2 * M_PI / len));
            for (size_t i = 0; i < N; i += len) { std::complex<double> w (1); for (size_t j = 0; j < len / 2; ++j) { auto u = x[i + j], v = x[i + j + len / 2] * w; x[i + j] = u + v; x[i + j + len / 2] = u - v; w *= wl; } } }
        std::vector<double> pw (N / 2); double tot = 0; size_t kb = 1;
        for (size_t k = 1; k < N / 2; ++k) { pw[k] = std::norm (x[k]); tot += pw[k]; if (pw[k] > pw[kb]) kb = k; }
        double e = 0; const size_t w = std::max<size_t> (2, (size_t) (kb * 0.03)); for (size_t k = kb > w ? kb - w : 1; k <= kb + w && k < N / 2; ++k) e += pw[k];
        f = kb * (double) SR / N; share = 10 * log10 (e / tot + 1e-30);
    };
    // 7c. (v82) LOOP: a drone grows its howl near COLR, held as LIMT says; the pickup squeals on TIME's pitch and glides
    // when TIME turns (a micro-delay's speed); NOIS's hiss, HUM's 50 Hz; v81's DIST onto TIME
    {
        auto drone = [&] (float fd, float lm) {
            setup (M_HOWL, 48000.0f); e->setParam (0, P_D, fd); side (HW_LIMT, lm); side (HW_MOVE, 0.0f);
            e->setParam (0, P_DRON, 1.0f); e->setPlay (1);
            auto o = run (14.0); double f, sh; peakAt (o, 13.3, f, sh); return std::make_pair (f, sh);
        };
        const auto h7 = drone (0.7f, 0.8f), h0 = drone (0.7f, 0.0f), hl = drone (0.35f, 0.8f), hh = drone (0.7f, 0.45f);
        check (h7.first > 400.0 && h7.first < 1500.0 && h7.second > -2.0 && h0.second < -4.0 && hl.second < -4.0 && hh.second < h7.second - 1.0 && hh.second > h0.second + 1.0,
               "HOWL LOOP: a drone's howl near COLR (810 Hz; FDBK 70 %, MOVE 0: most of the sound), none under FDBK's half; LIMT: 0 the chord, 45 % between (Hz)", h7.first);
        printf ("INFO HOWL LOOP: the howl's share of the sound at 13 s: LIMT 80 %% %.1f dB, 45 %% %.1f, 0 %.1f; FDBK 35 %% %.1f\n", h7.second, hh.second, h0.second, hl.second);
        setup (M_HOWL, 48000.0f); e->setParam (0, P_D, 0.7f); side (HW_PKUP, 1.0f); side (HW_EXCT, 0.0f); side (HW_MOVE, 0.0f); side (HW_FOCS, 0.6f); side (HW_TIME, 0.2f);
        e->setParam (0, P_DRON, 1.0f); e->setPlay (1);
        auto p0 = run (6.0); double f1, s1, fm, sm, f2, s2; peakAt (p0, 5.2, f1, s1);
        side (HW_TIME, 0.3f); auto p1 = run (2.0); peakAt (p1, 0.02, fm, sm); peakAt (p1, 1.2, f2, s2);
        check (s1 > -3.0 && s2 > -3.0 && f2 < 0.85 * f1 && fm < 0.98 * f1 && fm > 1.02 * f2,
               "HOWL PKUP: the pickup squeals on TIME (0.92 ms), turned to 1.6 ms it glides down there (Hz: before, on the way, after)", fm);
        printf ("INFO HOWL PKUP glide: %.0f -> %.0f -> %.0f Hz\n", f1, fm, f2);
        auto quiet = [&] (float nois, float hum) {
            setup (M_HOWL, 48000.0f); e->setParam (0, P_D, 0.0f); side (HW_EXCT, 0.0f); side (HW_NOIS, nois); side (HW_HUM, hum);
            e->audition (0, 1.0f, 0.0f); return run (3.0);
        };
        const auto n0 = quiet (0.0f, 0.0f), nd = quiet (0.2f, 0.0f), n1 = quiet (1.0f, 0.0f), hm = quiet (0.0f, 1.0f);
        const double l0 = lvl (n0, 2.0, 3.0), ld = lvl (nd, 2.0, 3.0), l1 = lvl (n1, 2.0, 3.0);
        check (l0 < -100.0 && ld > -75.0 && ld < -40.0 && l1 > ld + 20.0, "HOWL NOIS: the amp's hiss (none at 0, a light floor at its default, up 20 dB more at its top: dB at 20 %)", ld);
        const double h50 = db (toneAmp (hm, (int) (2.0 * SR), (int) (3.0 * SR), 50.0)), h100 = db (toneAmp (hm, (int) (2.0 * SR), (int) (3.0 * SR), 100.0)), h0n = db (toneAmp (n0, (int) (2.0 * SR), (int) (3.0 * SR), 50.0));
        check (h50 > -40.0 && h100 > -50.0 && h0n < -100.0, "HOWL HUM: the pickups' mains hum, 50 Hz and its buzz (dB at 50 Hz)", h50);
        bool okT = true; for (float d : { 0.0f, 0.4f, 1.0f }) if (fabs (0.0003 * pow (80.0 / 0.3, howlTimeFromDist (d)) / (0.002 * pow (40.0, d)) - 1.0) > 0.001) okT = false;
        check (okT, "HOWL v81 -> v82: DIST onto LOOP's TIME, the same distance (2 / 8.7 / 80 ms)", howlTimeFromDist (0.4f));
    }
    // 7d. (v83) BOW: the magnetic bow alone (no pick, no feedback, a short SUST): the string sings on at a steady level,
    // in tune (FUND), its octave (OCT) or twelfth (12TH); SWEL its swell, BOW its level; the gate's end lets it go; STRG
    // the string (1: the highest), HAND the others muted. CAB: TGHT takes the bass out from under the drive, CAB from
    // dark to crisp, MIC close or back; FDBK's threshold kept through them (the drone's howl)
    {
        auto bowed = [&] (int mode, float bow, float swel, double secs) {
            setup (M_HOWL, 48000.0f);
            e->setParam (0, P_A, 14.5f / 15.0f); e->setParam (0, P_C, 0.5f / 5.0f); e->setParam (0, P_D, 0.0f); e->setParam (0, P_DEC, 0.3f);
            side (HW_EXCT, 0.0f); side (HW_DRFT, 0.0f); side (HW_WAMY, 0.0f); side (HW_NOIS, 0.0f); side (HW_WOBL, 0.0f);
            side (HW_BOW, bow); side (HW_STRG, 0.5f / 7.0f); side (HW_BMOD, (mode + 0.5f) / 3.0f); side (HW_SWEL, swel);
            e->audition (0, 1.0f, 0.0f);       // (E2: 82.4 Hz)
            return run (secs);
        };
        const auto off = bowed (0, 0.0f, 0.3f, 2.0), fu = bowed (0, 0.6f, 0.3f, 6.0), oc = bowed (1, 0.6f, 0.3f, 4.0), tw = bowed (2, 0.6f, 0.3f, 4.0);
        double lo = 1e9, hi = -1e9; for (int k = 2; k < 6; ++k) { lo = std::min (lo, lvl (fu, k, k + 1)); hi = std::max (hi, lvl (fu, k, k + 1)); }
        const double cF = 1200.0 * log2 (f0 (fu, 4.0, 4.4) / 82.406889), cO = 1200.0 * log2 (f0 (oc, 3.0, 3.4) / 82.406889), cT = 1200.0 * log2 (f0 (tw, 3.0, 3.4) / 82.406889);
        check (lvl (off, 1.5, 2.0) < -100.0 && lo > -30.0 && hi - lo < 1.5 && fabs (cF) < 10.0 && fabs (cO - 1200.0) < 15.0 && fabs (cT - 1901.955) < 15.0,
               "HOWL BOW: no pick, no feedback, SUST short: the bowed string sings on, steady (2 .. 6 s); FUND, OCT, 12TH in tune (cents off FUND)", cF);
        printf ("INFO HOWL BOW: %.1f dB (spread %.2f), FUND %+.1f c, OCT %.1f c, 12TH %.1f c\n", lo, hi - lo, cF, cO, cT);
        const auto sl = bowed (0, 0.6f, 1.0f, 10.0);
        // (BOW's level through a clean amp: a driven one compresses it)
        auto clean = [&] (float bow) { setup (M_HOWL, 48000.0f); e->setParam (0, P_A, 14.5f / 15.0f); e->setParam (0, P_C, 0.5f / 5.0f); e->setParam (0, P_D, 0.0f); e->setParam (0, P_DEC, 0.3f);
            side (HW_EXCT, 0.0f); side (HW_NOIS, 0.0f); side (HW_DRIV, 0.0f); side (HW_FUZZ, 0.0f); side (HW_SAG, 0.0f); side (HW_BOW, bow); side (HW_STRG, 0.5f / 7.0f); side (HW_SWEL, 0.3f);
            e->audition (0, 1.0f, 0.0f); return run (4.0); };
        const auto b3 = clean (0.3f), b9 = clean (0.9f);
        check (lvl (sl, 0.5, 1.0) < lvl (fu, 0.5, 1.0) - 15.0 && fabs (lvl (sl, 9.0, 10.0) - lvl (fu, 5.0, 6.0)) < 2.0 && lvl (b9, 3.0, 4.0) > lvl (b3, 3.0, 4.0) + 6.0,
               "HOWL BOW: SWEL 6 s swells slowly to the same level; BOW 90 % louder than 30 % (dB at 1 s, SWEL 6 s against 0.1 s)", lvl (sl, 0.5, 1.0) - lvl (fu, 0.5, 1.0));
        // let go: the bow lifts
        setup (M_HOWL, 48000.0f);
        e->setParam (0, P_A, 14.5f / 15.0f); e->setParam (0, P_C, 0.5f / 5.0f); e->setParam (0, P_D, 0.0f); e->setParam (0, P_DEC, 0.3f);
        side (HW_EXCT, 0.0f); side (HW_NOIS, 0.0f); side (HW_BOW, 0.6f); side (HW_STRG, 0.5f / 7.0f); side (HW_SWEL, 0.3f);
        e->audition (0, 1.0f, 0.0f); auto g1 = run (2.0); e->release (0, 0.0f); auto g2 = run (2.0);
        check (lvl (g1, 1.5, 2.0) > -30.0 && lvl (g2, 1.5, 2.0) < -70.0, "HOWL BOW: let go, the bow lifts and the string dies away (dB 1.5 s after)", lvl (g2, 1.5, 2.0));
        // STRG 1 on a chord (5TH DRONE, no feedback, SUST short), HAND 100 %: the highest string alone, in tune
        setup (M_HOWL, 48000.0f); e->setParam (0, P_D, 0.0f); e->setParam (0, P_DEC, 0.3f); side (HW_DRFT, 0.0f); side (HW_WAMY, 0.0f); side (HW_NOIS, 0.0f); side (HW_WOBL, 0.0f);
        side (HW_BOW, 0.6f); side (HW_STRG, 1.5f / 7.0f); side (HW_HAND, 1.0f);
        e->audition (0, 1.0f, 0.0f); auto ch = run (4.0);
        float hi2[HALO_INFO]; e->haloInfo (0, hi2); float top = -999.0f; int nb = 0;
        for (int k = 0; k < HOWL_STR; ++k) { top = std::max (top, hi2[12 + 3 * k]); if (hi2[34 + k] > 0.5f) ++nb; }
        const double cS = 1200.0 * log2 (f0 (ch, 3.0, 3.4) / (440.0 * pow (2.0, (top - 69.0) / 12.0)));
        check (nb == 1 && fabs (cS) < 15.0 && lvl (ch, 3.0, 4.0) > -30.0, "HOWL BOW: STRG 1 on a chord, HAND 100 %: the highest string alone sings on (cents off its note)", cS);
        // CAB: a chord through the driven amp (no feedback)
        auto amp = [&] (int q1, float v1, int q2, float v2) {
            setup (M_HOWL, 48000.0f); e->setParam (0, P_D, 0.0f); e->setParam (0, P_DEC, 0.8f); side (HW_NOIS, 0.0f);
            if (q1 >= 0) side (q1, v1); if (q2 >= 0) side (q2, v2);
            e->audition (0, 1.0f, 0.0f); return run (1.5);
        };
        // (the share of the energy under 150 Hz, over 3 kHz: four one-poles, low- or high-passes)
        auto band = [&] (const std::vector<float>& o, double hz, bool low) {
            const double c = 1.0 - exp (-2.0 * M_PI * hz / SR); double st[4] = { 0, 0, 0, 0 }, el = 0, et = 0;
            for (int i = 0; i < (int) o.size(); ++i)
            {
                double y = o[i];
                for (int k = 0; k < 4; ++k) { st[k] += c * (y - st[k]); y = low ? st[k] : y - st[k]; }
                if (i >= (int) (0.3 * SR)) { el += y * y; et += (double) o[i] * o[i]; }
            }
            return 10.0 * log10 (el / (et + 1e-30) + 1e-30);
        };
        const auto a0 = amp (-1, 0, -1, 0), aT = amp (HW_TGHT, 1.0f, -1, 0), aD = amp (HW_CAB, 0.0f, -1, 0), aC = amp (HW_CAB, 1.0f, -1, 0), mC = amp (HW_MIC, 0.0f, -1, 0), mB = amp (HW_MIC, 1.0f, -1, 0);
        // (MIC: the level under 150 Hz itself, close against back: the cone's proximity)
        const double lT = band (aT, 150.0, true) - band (a0, 150.0, true), hC = band (aC, 3000.0, false) - band (aD, 3000.0, false);
        const double lM = band (mC, 150.0, true) + lvl (mC, 0.3, 1.5) - band (mB, 150.0, true) - lvl (mB, 0.3, 1.5);
        check (lT < -6.0 && hC > 6.0 && lM > 6.0, "HOWL CAB: TGHT takes the bass from under the drive (dB under 150 Hz); CAB crisp against dark over 3 kHz; MIC close against back under 150 Hz", lT);
        printf ("INFO HOWL CAB: TGHT %.1f dB under 150 Hz, CAB crisp - dark %.1f dB over 3 kHz, MIC close - back %.1f dB under 150 Hz\n", lT, hC, lM);
        // the drone's howl near COLR still: through a dark amp, tight, its depth up (the howl most of the sound), and a
        // crisp one, biting, its presence up (more of its own harmonics round the howl)
        auto howlCab = [&] (bool dark) {
            setup (M_HOWL, 48000.0f); e->setParam (0, P_D, 0.7f); side (HW_LIMT, 0.8f); side (HW_MOVE, 0.0f);
            if (dark) { side (HW_CAB, 0.1f); side (HW_TGHT, 0.6f); side (HW_DPTH, 0.8f); } else { side (HW_CAB, 0.9f); side (HW_BITE, 0.8f); side (HW_PRES, 0.9f); }
            e->setParam (0, P_DRON, 1.0f); e->setPlay (1);
            auto o = run (14.0); double f, sh; peakAt (o, 13.3, f, sh); double pkv = 0; for (float v : o) pkv = std::max (pkv, (double) fabs (v));
            return std::array<double, 3> { f, sh, pkv };
        };
        const auto hd = howlCab (true), hc = howlCab (false);
        check (hd[0] > 400.0 && hd[0] < 1500.0 && hd[1] > -3.0 && hc[0] > 400.0 && hc[0] < 1500.0 && hc[1] > -9.0 && hd[2] <= 1.0 && hc[2] <= 1.0,
               "HOWL CAB: FDBK's threshold kept through the cabinet: the drone's howl near COLR, dark and tight, crisp and biting (Hz, dark)", hd[0]);
    }
    // 8. the quantiser follows HOWL's chord (G_QSRC on its track): an E minor ninth's notes
    {
        setup (M_HOWL, 48000.0f); e->setParam (0, P_A, 11.5f / 15.0f); e->setGlobal (G_QSRC, 1.5f / 9.0f);
        e->audition (0, 1.0f, 0.0f); run (0.05);
        const u32 want = (1u << 4) | (1u << 7) | (1u << 11) | (1u << 2) | (1u << 6);      // E G B D F#
        check (e->quantMask() == want, "HOWL: the quantiser follows its chord (E G B D F#: the mask)", (double) e->quantMask());
        // (v81) its own root stays on the scale, not on its last chord: C minor, DROP -12, TUNE +0 +2 +6 +1 (by a key held
        // and turned too: the re-voice)
        setup (M_HOWL, 48000.0f); e->setScale (1453u); e->setQuant (0, 1); e->setGlobal (G_QSRC, 1.5f / 9.0f);
        const int tunes[4] = { 0, 2, 6, 1 }, want2[4] = { 27, 29, 34, 29 };
        int okR = 1; float info[66]; double got = 0.0;
        for (int k = 0; k < 4; ++k)
        {
            e->setParam (0, P_TUNE, 0.5f + (float) tunes[k] / 48.0f); e->audition (0, 1.0f, 0.0f); run (0.1); e->haloInfo (0, info);
            if ((int) lrintf (info[8]) != want2[k]) { okR = 0; got = info[8]; }
            e->release (0, 0.0f); run (0.1);
        }
        e->audition (0, 1.0f, 0.0f); run (0.1);
        for (int k = 3; k >= 0; --k)
        {
            e->setParam (0, P_TUNE, 0.5f + (float) tunes[k] / 48.0f); run (0.1); e->haloInfo (0, info);
            if ((int) lrintf (info[8]) != want2[k]) { okR = 0; got = 100.0 + info[8]; }
        }
        check (okR, "HOWL followed by the quantiser: its own root on the scale (D#1 F1 A#1 F1, struck and turned held)", got);
    }
    // 9. CPU (best of three): one HOWL track howling
    {
        double ms = 1e30, base = 1e30;
        for (int k = 0; k < 3; ++k)
        {
            setup (M_HOWL, 48000.0f); e->setParam (0, P_D, 0.8f); e->setParam (0, P_A, 11.5f / 15.0f); e->audition (0, 1.0f, 0.0f);
            std::vector<float> l (128), r (128);
            auto c0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < 48000 * 10; i += 128) e->process (l.data(), r.data(), 128, nullptr);
            ms = std::min (ms, std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - c0).count());
            setup (M_SY_RAW, 48000.0f); e->setParam (0, P_VOL, 0.0f);
            c0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < 48000 * 10; i += 128) e->process (l.data(), r.data(), 128, nullptr);
            base = std::min (base, std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - c0).count());
        }
        printf ("INFO HOWL: %.2f %% of real time (native)\n", (ms - base) / 100.0);
        check ((ms - base) / 100.0 < 6.0, "CPU of HOWL (% of real time, native)", (ms - base) / 100.0);
    }
    // 10. DRONE: no trig, the transport: the track plays (in its attack); a trig changes it (no dip, the new note); a
    //     mute lets it go (CHOIR: its own space's tail after it), unmuted it comes back; a stop lets it go; a machine
    //     that cannot hold one plays nothing
    {
        for (int m : { M_HOWL, M_SY_RAW, M_HALO, M_STRUM, M_CHOIR, M_CUBE })
        {
            setup (m, 48000.0f); e->setParam (0, P_DRON, 1.0f); e->setParam (0, P_ATK, 0.4f);
            e->setPlay (1);
            auto a = run (2.0);
            e->setTrig (0, 8, 1); e->setLock (0, 8, P_NOTE, 0.5f + 5.0f / 48.0f);     // (step 8: 1 s at 120 BPM)
            auto b = run (2.0);
            e->setTrig (0, 8, 0);
            double mn = 1e9; for (int w = 0; w < 20; ++w) mn = std::min (mn, lvl (b, 0.5 + w * 0.05, 0.55 + w * 0.05));
            const double dip = mn - lvl (b, 0.2, 0.45);
            e->setMute (0, 1); auto c = run (1.5); e->setMute (0, 0); auto d = run (1.5);
            e->setPlay (0); auto f = run (2.0);
            char w[128]; snprintf (w, sizeof w, "DRONE (%s): plays, changed without a dip, muted, back, stopped (dip, dB)",
                                    m == M_HOWL ? "HOWL" : m == M_SY_RAW ? "SY RAW" : m == M_HALO ? "HALO" : m == M_STRUM ? "STRUM" : m == M_CHOIR ? "CHOIR" : "CUBE");
            check (lvl (a, 1.0, 2.0) > -45.0 && dip > (m == M_HALO ? -9.0 : m == M_CHOIR ? -6.0 : -4.0) && lvl (c, 1.2, 1.5) < lvl (a, 1.0, 2.0) - (m == M_CHOIR ? 18.0 : 25.0)
                   && lvl (d, 1.0, 1.5) > lvl (a, 1.0, 2.0) - 6.0 && lvl (f, 1.5, 2.0) < lvl (a, 1.0, 2.0) - 30.0, w, dip);
        }
        // SY·RAW: the change glides to its new note (+5)
        setup (M_SY_RAW, 48000.0f); e->setParam (0, P_DRON, 1.0f); e->setPlay (1);
        auto a = run (0.9); e->setTrig (0, 8, 1); e->setLock (0, 8, P_NOTE, 0.5f + 5.0f / 48.0f); auto b = run (1.0);
        const double r = f0 (b, 0.6, 0.9) / f0 (a, 0.4, 0.8);
        check (fabs (12.0 * log2 (r) - 5.0) < 0.1, "DRONE: a trig on SY RAW's drone glides it to its note (+5, semitones)", 12.0 * log2 (r));
        // CP·NZ cannot hold one: nothing without a trig
        setup (M_CP_NZ, 48000.0f); e->setParam (0, P_DRON, 1.0f); e->setPlay (1);
        auto z = run (1.0);
        check (lvl (z, 0.0, 1.0) < -100.0, "DRONE: a machine that cannot hold one (CP NZ) plays nothing by itself (dB)", lvl (z, 0.0, 1.0));
        // a pattern change: a drone in both carries on (no gap), retuned to the new pattern's
        setup (M_SY_RAW, 48000.0f); e->setParam (0, P_DRON, 1.0f);
        e->patEdit (1); e->setParam (0, P_TUNE, 0.5f + 7.0f / 48.0f); e->patEdit (0);
        e->setPlay (1);
        auto p1 = run (0.6); e->patQueue (1, 1); auto p2 = run (1.0);
        double mn2 = 1e9; for (int w = 0; w < 16; ++w) mn2 = std::min (mn2, lvl (p2, w * 0.05, w * 0.05 + 0.05));
        const double rr = f0 (p2, 0.6, 0.95) / f0 (p1, 0.3, 0.55);
        check (mn2 > lvl (p1, 0.3, 0.55) - 4.0 && fabs (12.0 * log2 (rr) - 7.0) < 0.15, "DRONE: a pattern change carries it on (no gap) at the new pattern's TUNE (+7, semitones)", 12.0 * log2 (rr));
        // stopped: a pad on a drone track holds it while pressed (SY·RAW, DEC short)
        setup (M_SY_RAW, 48000.0f); e->setParam (0, P_DRON, 1.0f); e->setParam (0, P_DEC, 0.2f);
        e->audition (0, 1.0f, 0.0f); auto h1 = run (1.5); e->release (0, 0.0f); auto h2 = run (1.0);
        check (lvl (h1, 1.0, 1.5) > -30.0 && lvl (h2, 0.6, 1.0) < -60.0, "DRONE: stopped, a pad holds the drone while pressed (DEC short), let go it ends (dB held)", lvl (h1, 1.0, 1.5));
    }
    delete e;
    printf ("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}
// v83: STENCIL (core/sk_stencil), the stereo delay of zones: its repeats, KARP's string (plucked from nothing, NOTE
// transposing it), SYNC, SKEW, HOLD, FLIP, HALO, NOIS and its self-oscillation with no source, SRC NONE, and settings
// at random at three rates
static int stenciltest()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-96s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    Engine* e = new Engine();
    float SR = 48000.0f;
    // track 1: a click (IMPLS) on its fifth step of 4 bars (at T0: the knobs' glides from a fresh track done); track 2:
    // STENCIL taking it (SRC: 0 NONE, 1 NBR), its side defaults, MIX 100 %, IN always open (DEC HOLD), ZONE ECHO, RATE
    // .5, REPT 40 %, the rest off
    const double T0 = 0.5;
    auto base = [&] (int srcIdx, float sr) {
        SR = sr;
        e->init (sr, 0);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setGlobal (G_EQON, 0.0f); e->setGlobal (G_COON, 0.0f); e->setGlobal (G_DRON, 0.0f); e->g[G_MXON] = 0.0f;
        e->setGlobal (G_RVON, 0.0f); e->setGlobal (G_DLON, 0.0f);
        e->setParam (0, P_MACH, machineVal (M_IMPLS)); e->setParam (0, P_VOL, 0.8f); e->setParam (0, P_DEC, 0.1f);
        e->setLen (0, 64); e->setTrig (0, 4, 1);
        e->setParam (1, P_MACH, machineVal (M_FXSTENCIL)); e->setParam (1, P_VOL, 0.8f);
        e->setParam (1, P_A, (srcIdx + 0.5f) / 11.0f);
        e->setParam (1, P_B, 3.5f / 8.0f); e->setParam (1, P_C, 0.5f); e->setParam (1, P_D, 0.4f); e->setParam (1, P_E, 1.0f);
        e->setParam (1, P_F, 0.5f / 3.0f); e->setParam (1, P_TUNE, 0.5f); e->setParam (1, P_DEC, 1.0f);
        const float X[7] = { 0.0f, 0.0f, 0.5f, 0.25f, 0.125f, 0.0f, 0.0f };   // µ HALO SKEW SYNC (free) ACT (IN) PING NOIS
        const int XP[7] = { P_XA, P_XB, P_XC, P_XD, P_XE, P_XF, P_XG };
        for (int k = 0; k < 7; ++k) e->setParam (1, XP[k], X[k]);
        float sd[NUM_SIDES]; const int ns = machineSideDefaults (M_FXSTENCIL, sd); for (int q = 0; q < ns; ++q) e->setParam (1, sideParam (q), sd[q]);
        e->setLen (1, 64);
    };
    auto run = [&] (double secs) {
        std::vector<float> L (128), R (128), o;
        for (int i = 0; i < (int) (secs * SR); i += 128) { e->process (L.data(), R.data(), 128, nullptr); for (int k = 0; k < 128; ++k) o.push_back (L[k]); }
        return o;
    };
    auto pk = [&] (const std::vector<float>& o, double a, double b) { double m = 0; for (int i = std::max (0, (int) (a * SR)); i < (int) (b * SR) && i < (int) o.size(); ++i) m = std::max (m, (double) fabs (o[i])); return m; };
    auto lvl = [&] (const std::vector<float>& o, double a, double b) {
        double s = 0.0; int n = 0;
        for (int i = std::max (0, (int) (a * SR)); i < (int) (b * SR) && i < (int) o.size(); ++i) { s += (double) o[i] * o[i]; ++n; }
        return 10.0 * log10 (s / (double) std::max (n, 1) + 1e-30);
    };
    auto db = [] (double a) { return 20.0 * log10 (a + 1e-12); };
    // the period (samples) by autocorrelation, its peak interpolated
    auto period = [&] (const std::vector<float>& o, double a, double b, int lo, int hi) {
        const int i0 = (int) (a * SR), n = (int) ((b - a) * SR); int best = lo; double bv = -1e30;
        auto ac = [&] (int lag) { double sx = 0; for (int i = 0; i < n; ++i) sx += o[i0 + i] * o[i0 + i + lag]; return sx; };
        for (int lag = lo; lag < hi; ++lag) { const double sx = ac (lag); if (sx > bv) { bv = sx; best = lag; } }
        const double ym = ac (best - 1), y0 = bv, yp = ac (best + 1), den = ym - 2.0 * y0 + yp;
        return best + (fabs (den) > 1e-30 ? 0.5 * (ym - yp) / den : 0.0);
    };
    // 1. the repeats: ECHO, RATE .5, REPT 40 % (a gain of 50 % a pass): each repeat at the time the page shows, 5 .. 9 dB
    //    under the last, nothing between them
    {
        base (1, 48000.0f); e->setPlay (1);
        const auto o = run (3.0);
        const double T = stencilTime (3, 0.5f, 0, 0.125, 0.0f);
        auto rp = [&] (int k) { return db (pk (o, T0 + k * T - 0.002, T0 + k * T + 0.02)); };
        double worst = 0.0, fall = 0.0;
        for (int k = 2; k < 5; ++k) { const double d = rp (k - 1) - rp (k); fall += d / 3.0; worst = std::max (worst, fabs (d - 6.7)); }
        const double gap = db (pk (o, T0 + 3.3 * T, T0 + 3.8 * T)) - rp (3);
        check (worst < 2.3 && gap < -40.0, "STENCIL: REPT 40 %: a repeat at each T (ECHO, RATE .5), 5 .. 9 dB under the one before (mean, dB)", fall);
    }
    // 2. KARP and COMB: no source, ACT PLUK, a trig a quarter: a string at 1/T, flat or bright, low to high (its
    //    colour's filters taken out of its line); a trig's NOTE +12 an octave up
    {
        auto karp = [&] (int zone, float rate, float col, float note) {
            base (0, 48000.0f); e->setParam (0, P_VOL, 0.0f);
            e->setParam (1, P_B, (zone + 0.5f) / 8.0f); e->setParam (1, P_C, rate); e->setParam (1, P_D, 0.79f); e->setParam (1, P_XE, 3.5f / 4.0f);
            e->setParam (1, P_TUNE, col); e->setParam (1, P_NOTE, 0.5f + note / 48.0f);
            for (int s = 0; s < 64; s += 4) e->setTrig (1, s, 1);
            e->setPlay (1); return run (0.5);
        };
        double worst = 0.0, lo = 1.0;
        for (int zone : { 0, 1 }) for (float col : { 0.5f, 0.75f, 1.0f }) for (float rate : { 0.05f, 0.3f, 0.6f })
        {
            const double T = stencilTime (zone, rate, 0, 0.125, 0.0f) * SR;
            const auto o = karp (zone, rate, col, 0.0f);
            const double c = 1200.0 * log2 (period (o, 0.05, 0.2, (int) (0.7 * T), (int) (1.3 * T) + 2) / T);
            worst = std::max (worst, fabs (c)); lo = std::min (lo, pk (o, 0.05, 0.2));
        }
        check (worst < 8.0 && lo > 0.003, "STENCIL: KARP, COMB plucked from nothing (ACT PLUK), flat to bright, 25 Hz .. 760 Hz: in tune (worst cents)", worst);
        const auto o = karp (0, 0.3f, 0.5f, 0.0f), o12 = karp (0, 0.3f, 0.5f, 12.0f);
        const double T = stencilTime (0, 0.3f, 0, 0.125, 0.0f) * SR;
        const double r = period (o, 0.05, 0.2, (int) (0.7 * T), (int) (1.3 * T) + 2) / period (o12, 0.05, 0.2, (int) (0.35 * T), (int) (0.65 * T) + 2);
        check (fabs (1200.0 * log2 (r) - 1200.0) < 8.0, "STENCIL: KARP, a trig's NOTE +12 plays it an octave up (cents)", 1200.0 * log2 (r));
    }
    // 3. SYNC at 120 BPM: ECHO's RATE on the divisions (the time the engine plays: its info), SKEW in ratios (x 3/2 at
    //    its 6th seventh); free, SKEW at its right x 1.5
    {
        base (1, 48000.0f); e->setGlobal (G_BPM, 120.0f); e->setParam (1, P_XD, 0.75f); e->setParam (1, P_C, 0.3f); e->setParam (1, P_XC, 5.5f / 7.0f);
        e->setPlay (1); run (0.5);
        float info[FX_INFO]; e->fxInfo (1, info);
        const double want = stencilTime (3, 0.3f, 1, 0.125, 0.0f) * 1000.0;
        const double d16 = info[6] / 125.0;
        check (fabs (info[6] - want) < 0.5 && fabs (d16 * 3.0 - std::round (d16 * 3.0)) < 0.01 && fabs (info[7] / info[6] - 1.5) < 0.005,
               "STENCIL: SYNC (120 BPM): on a division of the tempo, the right line x 3/2 (SKEW; left ms)", info[6]);
        base (1, 48000.0f); e->setParam (1, P_XC, 1.0f); e->setPlay (1); run (0.5); e->fxInfo (1, info);
        check (fabs (info[7] / info[6] - 1.5) < 0.01, "STENCIL: free, SKEW at its right: the right line x 1.5 (ratio)", info[7] / info[6]);
    }
    // 4. HOLD: a trig on step 1 holds the loop at unity (ACT HOLD, DEC HOLD); the clicks going on do not enter it
    {
        base (1, 48000.0f); e->setParam (1, P_D, 0.5f); e->setParam (1, P_XE, 1.5f / 4.0f);
        e->setTrig (1, 5, 1); e->setPlay (1); const auto o = run (7.5);
        double lo = 1e9, hi = -1e9; for (int k = 1; k < 7; ++k) { lo = std::min (lo, lvl (o, T0 + k, T0 + k + 1)); hi = std::max (hi, lvl (o, T0 + k, T0 + k + 1)); }
        check (hi - lo < 1.5 && lo > -60.0, "STENCIL: HOLD: the loop held at unity over 6 s (spread of its level a second, dB)", hi - lo);
    }
    // 5. FLIP: the repeats backwards, there and finite
    {
        base (1, 48000.0f); e->setParam (1, P_D, 0.6f); e->setParam (1, P_XE, 2.5f / 4.0f); e->setTrig (1, 0, 1); e->setPlay (1);
        const auto o = run (3.0);
        check (lvl (o, T0 + 0.3, 3.0) > -50.0 && pk (o, 0, 3) <= 1.0, "STENCIL: FLIP: the repeats played backwards (dB)", lvl (o, T0 + 0.3, 3.0));
    }
    // 6. HALO: all of it, the repeats a reverb: their energy within 3 dB, their crest lower (a smear)
    {
        double en[2], cr[2];
        for (int i = 0; i < 2; ++i) { base (1, 48000.0f); e->setParam (1, P_XB, (float) i); e->setPlay (1); const auto o = run (4.0);
            en[i] = lvl (o, T0 + 0.2, T0 + 2.0); cr[i] = db (pk (o, T0 + 0.2, T0 + 2.0)) - en[i]; }
        check (fabs (en[1] - en[0]) < 3.0 && cr[1] < cr[0] - 2.0, "STENCIL: HALO 100 %: the repeats' energy kept (dB off), their crest lower", en[1] - en[0]);
    }
    // 7. NOIS, no source (SRC NONE), REPT 75 %: none, silence; all of it, the hiss heard
    {
        double l0 = 0, l1 = 0;
        for (int i = 0; i < 2; ++i) { base (0, 48000.0f); e->setParam (1, P_D, 0.6f); e->setParam (1, P_XG, (float) i); e->setPlay (1); const auto o = run (4.0); (i ? l1 : l0) = lvl (o, 2, 4); }
        check (l0 < -140.0 && l1 > -55.0 && l1 < -25.0, "STENCIL: NOIS: none, silence with no source; all of it, the loop's hiss (dB)", l1);
    }
    // 8. no source: REPT 125 %, NOIS 10 %: the loop starts on its own and stays bounded (SLAP, and TAPE with HALO)
    {
        double on = -1.0, mx = 0.0;
        base (0, 48000.0f); e->setParam (1, P_B, 2.5f / 8.0f); e->setParam (1, P_D, 1.0f); e->setParam (1, P_XG, 0.1f); e->setPlay (1);
        auto o = run (8.0); for (int k = 0; k < 80; ++k) if (lvl (o, k * 0.1, k * 0.1 + 0.1) > -30.0) { on = k * 0.1; break; }
        mx = pk (o, 0, 8);
        check (on >= 0.0 && on < 4.0 && mx <= 1.0 && lvl (o, 6, 8) > -24.0, "STENCIL: no source, REPT 125 %: it sings on its own (SLAP; s until -30 dB)", on);
        base (0, 48000.0f); e->setParam (1, P_B, 4.5f / 8.0f); e->setParam (1, P_D, 1.0f); e->setParam (1, P_XB, 1.0f); e->setParam (1, P_XG, 0.5f); e->setPlay (1);
        o = run (30.0);
        check (lvl (o, 27, 30) > -30.0 && pk (o, 0, 30) <= 1.0, "STENCIL: no source, TAPE, HALO 100 %, NOIS 50 %: the wash builds from nothing, bounded (dB after 27 s)", lvl (o, 27, 30));
    }
    // 9. SRC NONE: the click track next to it does not enter
    {
        base (0, 48000.0f); e->setPlay (1); const auto o = run (2.5);
        check (lvl (o, T0 + 0.15, 2.5) < -140.0, "STENCIL: SRC NONE: its neighbour does not enter (after its own click, dB)", lvl (o, T0 + 0.15, 2.5));
    }
    // 10b. (v84) the loop's ceiling: run away with no source (REPT 125 %, NOIS 30 %, MIX 50 %), with and without HALO, in
    //      KARP, SLAP and TAPE, it sings at about a track's level (-24 .. -12 dBFS RMS), steady (no pumping: 50 ms
    //      windows within 6 dB over 2 s: a wash moves, a pumping loop falls 40), its peaks under the master's knee
    {
        double lo = 1e9, hi = -1e9, sp = 0.0, pkx = 0.0;
        for (int z : { 0, 2, 4 }) for (float h : { 0.0f, 1.0f })
        {
            base (0, 48000.0f); e->setParam (0, P_VOL, 0.0f); e->setParam (1, P_B, (z + 0.5f) / 8.0f); e->setParam (1, P_D, 1.0f); e->setParam (1, P_XG, 0.3f);
            e->setParam (1, P_XB, h); e->setParam (1, P_E, 0.5f); e->setPlay (1);
            const auto o = run (22.0);
            const double l = lvl (o, 18.0, 22.0); lo = std::min (lo, l); hi = std::max (hi, l); pkx = std::max (pkx, pk (o, 18.0, 22.0));
            double a = 1e9, b = -1e9; for (int k = 0; k < 40; ++k) { const double w = lvl (o, 20.0 + 0.05 * k, 20.05 + 0.05 * k); a = std::min (a, w); b = std::max (b, w); }
            sp = std::max (sp, b - a);
        }
        check (lo > -24.0 && hi < -12.0 && sp < 6.0 && pkx < 0.63, "STENCIL (v84): run away with no source, HALO or not, KARP SLAP TAPE: at a track's level, steady (dB RMS, loudest)", hi);
        printf ("INFO STENCIL ceiling: %.1f .. %.1f dB RMS, 50 ms windows within %.1f dB, peak %.2f\n", lo, hi, sp, pkx);
    }
    // 10. settings at random (every knob, its side, trigs; RATE and ZONE jumping each second) at 44.1 / 48 / 96 kHz:
    //     finite, bounded
    {
        Rng r; r.seed (77); int bad = 0; float worst = 0.0f;
        for (float sr : { 44100.0f, 48000.0f, 96000.0f })
            for (int it = 0; it < 20; ++it)
            {
                base (it % 3, sr);
                for (int p : { P_B, P_C, P_D, P_TUNE, P_XA, P_XB, P_XC, P_XD, P_XE, P_XF, P_XG, P_DEC, P_NOTE }) e->setParam (1, p, r.uni());
                for (int q = 0; q < 7; ++q) e->setParam (1, sideParam (q), r.uni());
                if (it % 4 == 0) e->setParam (1, P_D, 1.0f);
                for (int s = 0; s < 16; ++s) if (r.uni() < 0.4f) e->setTrig (1, s, 1);
                e->setLen (1, 16); e->setPlay (1);
                std::vector<float> L (128), R (128);
                for (int i = 0; i < (int) (sr * 2.5f); i += 128)
                {
                    if (i % (int) sr < 128) { e->setParam (1, P_C, r.uni()); e->setParam (1, P_B, r.uni()); }
                    e->process (L.data(), R.data(), 128, nullptr);
                    for (int k = 0; k < 128; ++k) { if (! std::isfinite (L[k]) || ! std::isfinite (R[k])) ++bad; worst = std::max (worst, std::max (fabsf (L[k]), fabsf (R[k]))); }
                }
            }
        check (bad == 0 && worst <= 1.0f, "STENCIL: 60 random settings at 44.1 / 48 / 96 kHz, RATE and ZONE jumping: finite, bounded (worst peak)", worst);
    }
    delete e;
    printf ("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}
// v84: MANTRA (eight voices on one supply, their FM, the hyper LFO, the delay, the drive; the screen's pads)
static int mantratest()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what, double v) { printf ("%s %-96s %.4f\n", ok ? "PASS" : "FAIL", what, v); if (! ok) ++fails; };
    Engine* e = new Engine();
    float SR = 48000.0f;
    auto setup = [&] (float sr) {
        SR = sr;
        e->init (sr, 0);
        for (int t = 0; t < NUM_TRACKS; ++t) e->setParam (t, P_VOL, 0.0f);
        e->setGlobal (G_EQON, 0.0f); e->setGlobal (G_COON, 0.0f); e->setGlobal (G_DRON, 0.0f); e->g[G_MXON] = 0.0f;
        e->setParam (0, P_MACH, machineVal (M_MANTRA)); e->setParam (0, P_VOL, 0.8f); e->setParam (0, P_DEC, 0.72f);
        { float af[6]; const int na = machineAFDefaults (M_MANTRA, af); for (int q = 0; q < na; ++q) e->setParam (0, P_A + q, af[q]); }
        { float sd[NUM_SIDES]; const int ns = machineSideDefaults (M_MANTRA, sd); for (int q = 0; q < ns; ++q) e->setParam (0, sideParam (q), sd[q]); }
    };
    auto side = [&] (int q, float v) { e->setParam (0, sideParam (q), v); };
    auto sw = [] (int i, int n) { return ((float) i + 0.5f) / (float) n; };
    // a clean circuit: no tolerances, drift, vibrato, supply, bleed, bus, delay, distortion, noise, modulation
    auto clean = [&] () {
        side (MT_TOL, 0); side (MT_DRFT, 0); side (MT_VIB, 0); side (MT_SAG, 0); side (MT_BLED, 0); side (MT_BUS, 0);
        side (MT_DMIX, 0); side (MT_DSTM, 0); side (MT_NOIS, 0); e->setParam (0, P_D, 0); e->setParam (0, P_E, 0);
        for (int q : { MT_MOD12, MT_MOD34, MT_MOD56, MT_MOD78 }) side (q, 0);
    };
    auto run = [&] (double secs) {
        std::vector<float> L (128), R (128), o;
        for (int i = 0; i < (int) (secs * SR); i += 128) { e->process (L.data(), R.data(), 128, nullptr); for (int k = 0; k < 128; ++k) o.push_back (0.5f * (L[k] + R[k])); }
        return o;
    };
    auto lvl = [&] (const std::vector<float>& o, double a, double b) {
        double s = 0.0; int n = 0;
        for (int i = (int) (a * SR); i < (int) (b * SR) && i < (int) o.size(); ++i) { s += (double) o[i] * o[i]; ++n; }
        return 10.0 * log10 (s / (double) std::max (n, 1) + 1e-30);
    };
    auto pk = [&] (const std::vector<float>& o) { double m = 0; for (float x : o) m = std::max (m, (double) fabsf (x)); return m; };
    auto finite = [] (const std::vector<float>& o) { for (float x : o) if (! std::isfinite (x)) return false; return true; };
    // a Hann-windowed DFT's magnitude at f (dB), the strongest frequency in [f1, f2], f0 from rising zero crossings
    auto mag = [&] (const std::vector<float>& o, double a, double b, double f) {
        const int i0 = (int) (a * SR), n = (int) ((b - a) * SR); double re = 0, im = 0; const double w = 2 * M_PI * f / SR;
        for (int i = 0; i < n; ++i) { const double h = 0.5 - 0.5 * cos (2 * M_PI * i / n); re += h * o[i0 + i] * cos (w * i); im += h * o[i0 + i] * sin (w * i); }
        return 20.0 * log10 (2.0 * sqrt (re * re + im * im) / (0.5 * n) + 1e-30);
    };
    auto peakF = [&] (const std::vector<float>& o, double a, double b, double f1, double f2) {
        double best = -1e9, bf = f1; const double st = 0.25 / (b - a);
        for (double f = f1; f <= f2; f += st) { const double m = mag (o, a, b, f); if (m > best) { best = m; bf = f; } }
        double lo = bf - st, hi = bf + st;
        for (int it = 0; it < 30; ++it) { const double m1 = lo + (hi - lo) / 3, m2 = hi - (hi - lo) / 3; if (mag (o, a, b, m1) < mag (o, a, b, m2)) lo = m1; else hi = m2; }
        return 0.5 * (lo + hi);
    };
    auto zcF = [&] (const std::vector<float>& o, double a, double b) {
        const int i0 = (int) (a * SR), i1 = (int) (b * SR); double first = -1, last = -1, dc = 0; int n = 0;
        for (int i = i0; i < i1; ++i) dc += o[i];
        dc /= (double) (i1 - i0);
        // (with a hysteresis: a square's droop and its edges' ripple are not crossings)
        double rms = 0; for (int i = i0; i < i1; ++i) rms += (o[i] - dc) * (o[i] - dc);
        const double hy = 0.3 * sqrt (rms / (double) (i1 - i0));
        bool armed = false;
        for (int i = i0 + 1; i < i1; ++i)
        {
            if (o[i] - dc < -hy) armed = true;
            if (armed && o[i - 1] - dc < 0 && o[i] - dc >= 0) { const double t = i - 1 + (dc - o[i - 1]) / (o[i] - o[i - 1]); if (first < 0) first = t; else ++n; last = t; armed = false; }
        }
        return n > 0 ? SR * n / (last - first) : 0.0;
    };
    const int TUN[8] = { MT_TUN1, MT_TUN2, MT_TUN3, MT_TUN4, MT_TUN5, MT_TUN6, MT_TUN7, MT_TUN8 };
    const int SHP[4] = { MT_SHP12, MT_SHP34, MT_SHP56, MT_SHP78 };
    float info[HALO_INFO];
    // 1. in tune: one voice touched hard, clean, at its TUNE map's frequency (both shapes, three rates)
    {
        double worst = 0.0;
        for (float sr : { 44100.0f, 48000.0f, 96000.0f })
            for (int k : { 0, 3, 6, 7 })
                for (float x : { 0.25f, 0.6f, 0.95f })
                    for (float shp : { 1.0f, 0.0f })
                    {
                        setup (sr); clean (); side (TUN[k], x); side (SHP[k >> 1], shp);
                        e->mantraTouch (0, k, 1.0f);
                        const auto o = run (1.2);
                        worst = std::max (worst, fabs (1200.0 * log2 (zcF (o, 0.6, 1.2) / mantraTuneHz (k, x))));
                    }
        check (worst < 1.5, "MANTRA: a voice in tune with its TUNE (voices 1 4 7 8, three TUNEs, triangle and square, 44.1 / 48 / 96 kHz; worst, cents)", worst);
        bool ok = true;
        for (int k = 0; k < 8; ++k)
        {
            const float lo = mantraTuneHz (k, 0.0f), hi = mantraTuneHz (k, 1.0f), want = k < 2 ? 600.0f : k < 6 ? 1200.0f : 2400.0f;
            if (fabsf (hi - want) > 0.01f || fabsf (lo - want / 20.0f) > 0.01f) ok = false;
            for (int i = 1; i <= 100; ++i) if (mantraTuneHz (k, i / 100.0f) <= mantraTuneHz (k, (i - 1) / 100.0f)) ok = false;
        }
        check (ok, "MANTRA: TUNE spans 1 2 30 .. 600 Hz, 3 .. 6 60 .. 1200, 7 8 120 .. 2400, always rising", 0);
    }
    // 2. PITCH moves its group only; TUNE and a trig's note transpose all eight
    {
        setup (48000.0f); clean (); side (MT_PCH1, 1.0f); side (MT_TUN1, 0.5f); side (MT_TUN5, 0.5f); side (MT_SHP12, 1.0f); side (MT_SHP56, 1.0f);
        e->mantraTouch (0, 0, 1.0f); auto a = run (1.2);
        setup (48000.0f); clean (); side (MT_PCH1, 1.0f); side (MT_TUN5, 0.5f); side (MT_SHP56, 1.0f);
        e->mantraTouch (0, 4, 1.0f); auto b = run (1.2);
        const double c1 = 1200.0 * log2 (zcF (a, 0.6, 1.2) / mantraTuneHz (0, 0.5f)), c5 = 1200.0 * log2 (zcF (b, 0.6, 1.2) / mantraTuneHz (4, 0.5f));
        check (fabs (c1 - 720.0) < 3.0 && fabs (c5) < 1.5, "MANTRA: PCH1 at its top lifts voices 1 .. 4 by 720 cents, not 5 .. 8 (voice 1's, cents)", c1);
        setup (48000.0f); clean (); side (MT_TUN1, 0.4f); side (MT_SHP12, 1.0f); e->setParam (0, P_A, sw (0, MT_TCHS));
        e->audition (0, 1.0f, 12.0f); auto o = run (1.2);
        const double f12 = zcF (o, 0.6, 1.2);
        setup (48000.0f); clean (); side (MT_TUN1, 0.4f); side (MT_SHP12, 1.0f); e->setParam (0, P_A, sw (0, MT_TCHS)); e->setParam (0, P_TUNE, 0.25f);
        e->audition (0, 1.0f, 0.0f); o = run (1.2);
        const double fdn = zcF (o, 0.6, 1.2), base = mantraTuneHz (0, 0.4f);
        const double c12 = 1200.0 * log2 (f12 / base), cdn = 1200.0 * log2 (fdn / base);
        check (fabs (c12 - 1200.0) < 12.0 && fabs (cdn + 1200.0) < 12.0, "MANTRA: a trig's NOTE +12 an octave up, TUNE at a quarter an octave down (the first, cents; its touch: a little flat)", c12);
    }
    // 3. the hyper LFO: A's square at its frequency; OR a gradient (three levels), AND the product
    {
        setup (48000.0f); side (MT_FRQA, (float) (log10 (2.0 / 0.03) / 4.0)); side (MT_FRQB, (float) (log10 (0.31 / 0.03) / 4.0));
        e->mantraTouch (0, 0, 1.0f);
        std::vector<float> L (128), R (128); int edges = 0; double firstE = -1, lastE = -1; float prev = 0; std::vector<int> levels (5, 0);
        for (int i = 0; i < (int) (8.0 * SR); i += 128)
        {
            e->process (L.data(), R.data(), 128, nullptr); e->haloInfo (0, info);
            if (prev < 0.5f && info[27] >= 0.5f) { const double t = i / SR; if (firstE < 0) firstE = t; else ++edges; lastE = t; }
            prev = info[27];
            if (i > SR) levels[(int) std::lround (info[26] * 4.0f)]++;
        }
        const double fA = edges / (lastE - firstE);
        check (fabs (fA / 2.0 - 1.0) < 0.03, "MANTRA: the LFO's A at its FRQA (2 Hz; measured, Hz)", fA);
        check (levels[0] > 0 && levels[2] > 0 && levels[4] > 0, "MANTRA: OR mixes A and B into a gradient (its three levels all met)", levels[2]);
    }
    // 4. LFO into a pair: the lower a voice is tuned, the more it moves; MOD's start gentle, its top a stall
    {
        double sp[2][3];
        for (int hi = 0; hi < 2; ++hi)
            for (int mi = 0; mi < 3; ++mi)
            {
                const float md = mi == 0 ? 0.0f : mi == 1 ? 0.3f : 0.5f;
                setup (48000.0f); clean ();
                side (MT_SHP12, 1); side (MT_SRC12, sw (2, 3)); side (MT_MOD12, md); side (MT_FRQA, (float) (log10 (2.0 / 0.03) / 4.0)); side (MT_FRQB, (float) (log10 (2.0 / 0.03) / 4.0));
                side (MT_TUN1, hi ? 0.95f : 0.55f);
                e->mantraTouch (0, 0, 1.0f);
                const auto o = run (3.0);
                double mn = 1e9, mx = 0;
                for (double t = 1.0; t < 3.0; t += 0.05) { const double f = zcF (o, t, t + 0.05); if (f > 0) { mn = std::min (mn, f); mx = std::max (mx, f); } }
                sp[hi][mi] = 1200.0 * log2 (mx / mn);
            }
        check (sp[0][0] < 2.0 && sp[0][1] > 15.0 && sp[0][1] < 150.0, "MANTRA: LFO into pair 12: MOD 0 still, MOD .3 a gentle swing on a low voice (cents)", sp[0][1]);
        check (sp[0][2] > 4.0 * sp[1][2], "MANTRA: the same MOD moves a low voice far more than a high one (MOD .5: the low voice's swing, cents)", sp[0][2]);
        // pushed past its stall: the voice stops while the LFO is high (a gate)
        setup (48000.0f); clean (); side (MT_SRC12, sw (2, 3)); side (MT_MOD12, 1.0f); side (MT_TUN1, 0.4f); side (MT_FRQA, (float) (log10 (1.0 / 0.03) / 4.0)); side (MT_FRQB, (float) (log10 (1.0 / 0.03) / 4.0));
        e->mantraTouch (0, 0, 1.0f);
        const auto o = run (5.0);
        int quiet = 0, loud = 0;
        for (double t = 1.0; t < 5.0; t += 0.02) { const double l = lvl (o, t, t + 0.02); if (l < -60.0) ++quiet; else if (l > -40.0) ++loud; }
        printf ("     (stall: %d windows quiet, %d loud)\n", quiet, loud);
        check (quiet > 20 && loud >= 10, "MANTRA: MOD at its top stalls a low voice while the LFO is high (20 ms windows silent)", quiet);
    }
    // 5. one pair into the other (FM): the carrier leaves its own frequency; mutual FM stays finite and bounded
    {
        double a[2];
        for (int mi = 0; mi < 2; ++mi)
        {
            setup (48000.0f); clean (); side (MT_SRC12, sw (1, 3)); side (MT_MOD12, mi ? 0.6f : 0.0f); side (MT_SHP12, 0.2f);
            e->mantraTouch (0, 0, 1.0f); e->mantraTouch (0, 2, 1.0f);
            const auto o = run (2.0);
            a[mi] = mag (o, 1.0, 2.0, mantraTuneHz (0, 0.2778f));
        }
        check (a[0] - a[1] > 10.0, "MANTRA: FM from 34 at .6 pulls voice 1 off its own fundamental (dB lost there)", a[0] - a[1]);
        setup (48000.0f); side (MT_SRC12, sw (1, 3)); side (MT_SRC34, sw (1, 3)); side (MT_SRC56, sw (1, 3)); side (MT_SRC78, sw (1, 3)); side (MT_LOOP, 0.75f);
        for (int q : { MT_MOD12, MT_MOD34, MT_MOD56, MT_MOD78 }) side (q, 1.0f);
        for (int k = 0; k < 8; ++k) e->mantraTouch (0, k, 1.0f);
        const auto o = run (4.0);
        check (finite (o) && pk (o) < 0.9, "MANTRA: 1 LOOP, every pair at full FM: finite, bounded (peak)", pk (o));
    }
    // 6. HOLD: the level a group never falls under (none at 0, FAST pairs later); the releases; a light touch
    {
        double h[4];
        const float xs[4] = { 0.0f, 0.5f, 0.4f, 0.9f };
        for (int i = 0; i < 4; ++i)
        {
            setup (48000.0f); e->setParam (0, P_DEC, 0.2f); e->setParam (0, P_B, xs[i]); side (MT_FST12, i >= 2 ? 0.75f : 0.25f); side (MT_FST34, i >= 2 ? 0.75f : 0.25f);
            e->mantraTouch (0, 0, 0.6f); run (0.05); e->mantraTouch (0, 0, 0.0f);
            const auto o = run (5.0);
            h[i] = lvl (o, 4.0, 5.0);
        }
        check (h[0] < -80.0 && h[1] > -35.0, "MANTRA: HOLD 0 lets a voice fall silent, HOLD .5 keeps its group sounding (dB at 4 s)", h[1]);
        check (h[2] < -80.0 && h[3] > -35.0, "MANTRA: on a FAST pair HOLD opens later (.4 silent, .9 sounding; dB)", h[3]);
        double r[2][2];
        for (int fast = 0; fast < 2; ++fast)
        {
            setup (48000.0f); clean (); side (MT_FST12, fast ? 0.75f : 0.25f);
            e->mantraTouch (0, 0, 0.9f); const auto a = run (1.0); e->mantraTouch (0, 0, 0.0f); const auto b = run (1.5);
            r[fast][0] = lvl (a, 0.5, 1.0); r[fast][1] = lvl (b, 0.9, 1.1);
        }
        check (r[0][0] - r[0][1] < 12.0, "MANTRA: DEC .72: a second after the finger lifts, a slow voice is still there (dB lost)", r[0][0] - r[0][1]);
        check (r[1][0] - r[1][1] > 70.0, "MANTRA: FAST: gone a second after (dB lost)", r[1][0] - r[1][1]);
        double t6[2], fin[2];
        for (int i = 0; i < 2; ++i)
        {
            setup (48000.0f); clean ();
            e->mantraTouch (0, 0, i ? 1.0f : 0.25f); const auto a = run (2.0);
            fin[i] = lvl (a, 1.5, 2.0); t6[i] = 9.0;
            for (double t = 0; t < 1.5; t += 0.005) if (lvl (a, t, t + 0.005) > fin[i] - 6.0) { t6[i] = t; break; }
        }
        check (t6[0] > 4.0 * t6[1] && fin[0] < fin[1] - 3.0, "MANTRA: a light touch: slower and softer (its time to -6 dB, s)", t6[0]);
    }
    // 7. the shared supply: opening voices 5 .. 8 pulls voice 1 flat (SAG), two close voices pull together
    {
        double c[2];
        for (int si = 0; si < 2; ++si)
        {
            double f[2];
            for (int with = 0; with < 2; ++with)
            {
                setup (48000.0f); clean (); side (MT_SAG, si ? 0.35f : 0.0f); side (MT_TUN1, 0.55f); side (MT_SHP12, 0);
                for (int q : { MT_TUN5, MT_TUN6, MT_TUN7, MT_TUN8 }) side (q, 0.7f);
                e->mantraTouch (0, 0, 1.0f);
                if (with) for (int k = 4; k < 8; ++k) e->mantraTouch (0, k, 1.0f);
                const auto o = run (2.5);
                f[with] = peakF (o, 1.0, 2.5, mantraTuneHz (0, 0.55f) * 0.95, mantraTuneHz (0, 0.55f) * 1.05);
            }
            c[si] = 1200.0 * log2 (f[1] / f[0]);
        }
        check (fabs (c[0]) < 0.3 && c[1] < -3.0, "MANTRA: SAG .35: four more voices open pull voice 1 flat (SAG 0: none; cents)", c[1]);
        setup (48000.0f); clean (); side (MT_SAG, 1.0f); side (MT_TUN1, 0.55f); side (MT_TUN2, 0.5525f); side (MT_SHP12, 0.0f);
        e->mantraTouch (0, 0, 1.0f); e->mantraTouch (0, 1, 1.0f);
        const auto o = run (6.0);
        const double fa = mantraTuneHz (0, 0.55f), fb = mantraTuneHz (1, 0.5525f);
        const double p1 = peakF (o, 2, 6, fa - 0.6 * (fb - fa), fa + 0.5 * (fb - fa)), p2 = peakF (o, 2, 6, fa + 0.5 * (fb - fa), fb + 0.6 * (fb - fa));
        check (p2 - p1 < 0.5 * (fb - fa), "MANTRA: SAG at its top: two voices 1 Hz apart pull together (Hz left between them)", p2 - p1);
    }
    // 8. the mix's op-amp driven: the voices intermodulate (their difference tone)
    {
        double d[2];
        for (int bi = 0; bi < 2; ++bi)
        {
            setup (48000.0f); clean (); side (MT_BUS, bi ? 0.7f : 0.0f); side (MT_SHP12, 0); side (MT_SHP34, 0); side (MT_TUN1, 0.55f); side (MT_TUN3, 0.44f);
            e->mantraTouch (0, 0, 1.0f); e->mantraTouch (0, 2, 1.0f);
            const auto o = run (2.5);
            const double f1 = peakF (o, 1, 2.5, 100, 140), f3 = peakF (o, 1, 2.5, 150, 200);
            d[bi] = mag (o, 1, 2.5, f3 - f1) - mag (o, 1, 2.5, f1);
        }
        check (d[1] - d[0] > 30.0 && d[1] > -45.0, "MANTRA: BUS .7: two triangles' difference tone appears (dB under voice 1)", d[1]);
    }
    // 9. the delay: its times, its feedback (past the middle it sings alone, under its ceiling)
    {
        double worst = 0.0;
        for (float x : { 0.3f, 0.6f, 0.85f })
        {
            double on[2];
            for (int wet = 0; wet < 2; ++wet)
            {
                setup (48000.0f); clean (); side (MT_DMIX, (float) wet); side (MT_DMD1, 0); side (MT_DMD2, 0); side (MT_TIM1, x); side (MT_TIM2, x); side (MT_FST12, 0.75f); side (MT_SHP12, 1);
                run (0.2);
                e->mantraTouch (0, 0, 1.0f); auto a = run (0.05); e->mantraTouch (0, 0, 0.0f); const auto b = run (1.6);
                a.insert (a.end (), b.begin (), b.end ());
                on[wet] = -1; for (size_t i = 0; i < a.size (); ++i) if (fabsf (a[i]) > 0.01f) { on[wet] = i / SR; break; }
            }
            const double T = 0.008 * pow (137.5, x);
            worst = std::max (worst, fabs ((on[1] - on[0]) / T - 1.0));
        }
        check (worst < 0.02, "MANTRA: the delay's times (35, 154, 526 ms; worst error, fraction)", worst);
        double l[3]; double pmax = 0;
        const float fbs[3] = { 0.4f, 0.7f, 1.0f };
        for (int i = 0; i < 3; ++i)
        {
            setup (48000.0f); side (MT_DMIX, 1.0f); e->setParam (0, P_D, fbs[i]); side (MT_FST12, 0.75f);
            e->mantraTouch (0, 0, 1.0f); run (0.3); e->mantraTouch (0, 0, 0.0f);
            const auto b = run (8.0);
            l[i] = lvl (b, 7, 8); pmax = std::max (pmax, pk (b));
        }
        check (l[0] < -70.0 && l[1] > -40.0 && l[2] > -40.0, "MANTRA: DFB .4 dies away, .7 and 1 sing alone (dB at 7 s, DFB .7)", l[1]);
        check (pmax < 0.5, "MANTRA: its song held under the loop's ceiling (peak, DMIX 100 %)", pmax);
    }
    // 10. the sensors a trig touches (TCH), its tap, the screen's pads, a stop
    {
        setup (48000.0f); e->setParam (0, P_A, sw (12, MT_TCHS)); e->setParam (0, P_GATE, 0.0f);
        e->setTrig (0, 0, 1); e->setPlay (1);
        run (0.06); e->haloInfo (0, info);
        bool ok = true; for (int k = 0; k < 8; ++k) if ((info[18 + k] > 0.0f) != (k < 4)) ok = false;
        run (0.2); e->haloInfo (0, info);
        bool tap = true; for (int k = 0; k < 8; ++k) if (info[18 + k] > 0.0f) tap = false;
        check (ok && tap, "MANTRA: a trig with TCH 1234 touches voices 1 .. 4 only, for a tap", 0);
        e->setPlay (0);
        setup (48000.0f);
        const bool idle = ! e->tr[0].voice[e->tr[0].cur].active;
        e->mantraTouch (0, 5, 0.7f); run (0.1); e->haloInfo (0, info);
        bool one = info[1] == 8.0f; for (int k = 0; k < 8; ++k) if ((info[18 + k] > 0.0f) != (k == 5)) one = false;
        const double held = info[18 + 5];
        e->mantraTouch (0, 5, 0.0f); run (0.1); e->haloInfo (0, info);
        check (idle && one && fabs (held - 0.7) < 1e-6 && info[18 + 5] == 0.0f, "MANTRA: a pad on the screen wakes the track and touches its own voice only (its pressure)", held);
        side (MT_FST12, 0.75f); side (MT_FST34, 0.75f); side (MT_FST56, 0.75f); side (MT_FST78, 0.75f); side (MT_DMIX, 0.0f);
        run (30.0);
        check (! e->tr[0].voice[e->tr[0].cur].active, "MANTRA: lifted, no HOLD: the voice ends once silent", 0);
        setup (48000.0f); e->setParam (0, P_A, sw (14, MT_TCHS)); e->setParam (0, P_GATE, 1.0f); e->setParam (0, P_DEC, 1.0f);
        e->setTrig (0, 0, 1); e->setPlay (1); run (0.5); e->haloInfo (0, info);
        const double before = info[18];
        e->setPlay (0); run (0.1); e->haloInfo (0, info);
        check (before > 0.0 && info[18] == 0.0f, "MANTRA: a trig's GATE holds its touch; a stop lifts it", before);
    }
    // 11. everything up, random settings: finite, bounded; CPU
    {
        setup (48000.0f);
        for (int q : { MT_SRC12, MT_SRC34, MT_SRC56, MT_SRC78 }) side (q, sw (2, 3));
        for (int q : { MT_MOD12, MT_MOD34, MT_MOD56, MT_MOD78 }) side (q, 1);
        side (MT_TOTL, 0.75f); side (MT_BUS, 1); side (MT_DSTM, 1); side (MT_VOL, 1); side (MT_DMIX, 0.5f); side (MT_DSRC, 0.75f);
        e->setParam (0, P_D, 1); e->setParam (0, P_E, 1); e->setParam (0, P_B, 1); e->setParam (0, P_C, 1);
        e->audition (0, 1.0f, 0.0f);
        const auto o = run (6.0);
        check (finite (o) && pk (o) < 0.9, "MANTRA: every knob up, TOTAL FB on (peak)", pk (o));
        Rng rng; rng.seed (8484);
        float worst = 0.0f; int bad = 0;
        for (int it = 0; it < 90; ++it)
        {
            setup (it % 3 == 0 ? 44100.0f : it % 3 == 1 ? 48000.0f : 96000.0f);
            for (int q = 0; q < MT_NSIDE; ++q) side (q, rng.uni ());
            for (int p : { P_A, P_B, P_C, P_D, P_E, P_TUNE, P_DEC }) e->setParam (0, p, rng.uni ());
            e->audition (0, rng.uni (), (float) (int) (rng.bi () * 24.0f));
            for (int k = 0; k < 8; ++k) if (rng.uni () < 0.4f) e->mantraTouch (0, k, rng.uni ());
            std::vector<float> L (128), R (128);
            for (int c = 0; c < (int) (1.5 * SR) / 128; ++c)
            {
                if (c % 50 == 25) { e->setParam (0, sideParam ((int) (rng.uni () * MT_NSIDE)), rng.uni ()); e->mantraTouch (0, (int) (rng.uni () * 8), rng.uni () < 0.5f ? 0.0f : rng.uni ()); }
                e->process (L.data(), R.data(), 128, nullptr);
                for (int k = 0; k < 128; ++k) { if (! std::isfinite (L[k]) || ! std::isfinite (R[k])) ++bad; worst = std::max (worst, std::max (fabsf (L[k]), fabsf (R[k]))); }
            }
        }
        check (bad == 0 && worst <= 1.0f, "MANTRA: 90 random settings at 44.1 / 48 / 96 kHz, knobs and pads moving: finite, bounded (worst peak)", worst);
        setup (48000.0f);
        for (int t = 0; t < 8; ++t)
        {
            e->setParam (t, P_MACH, machineVal (M_MANTRA)); e->setParam (t, P_VOL, 0.3f);
            float af[6]; const int na = machineAFDefaults (M_MANTRA, af); for (int q = 0; q < na; ++q) e->setParam (t, P_A + q, af[q]);
            float sd[NUM_SIDES]; const int ns = machineSideDefaults (M_MANTRA, sd); for (int q = 0; q < ns; ++q) e->setParam (t, sideParam (q), sd[q]);
            e->setParam (t, P_D, 0.6f); e->audition (t, 1.0f, 0.0f);
        }
        const auto t0 = std::chrono::steady_clock::now (); run (5.0); const auto t1 = std::chrono::steady_clock::now ();
        const double rt = 5.0 / std::chrono::duration<double> (t1 - t0).count ();
        check (rt > 2.0, "MANTRA: eight tracks, all 64 voices open, the delays singing: faster than realtime (x)", rt);
    }
    delete e;
    printf ("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}

// ------------------------------------------------------------------ RADIO (v87, core/sk_radio): the decoder (an MP3 at
// 44.1 kHz stereo and 22.05 kHz mono, IMA ADPCM at a receiver's 12001.135 Hz), the feed (a burst, then in time; clocks
// 0.3 % fast and slow taken up; a dropout and back), LIVE (TUNE's shift up and down, SQL, the GATE), LOOP (what it
// took kept while the receiver changes; GRAB 0 and 1; transposed), DIG (further back), the AIR, the static alone, 90
// settings at random at 44.1, 48 and 96 kHz
static float radioPeakHz (const float* x, int n, float sr, float lo, float hi)
{
    float best = 0, bf = 0;
    for (float f = lo; f <= hi; f += 5)
    {
        double re = 0, im = 0;
        for (int i = 0; i < n; ++i) { const double w = 0.5 - 0.5 * cos (2 * M_PI * i / (n - 1)); re += x[i] * w * cos (2 * M_PI * f * i / sr); im += x[i] * w * sin (2 * M_PI * f * i / sr); }
        if ((float) (re * re + im * im) > best) { best = (float) (re * re + im * im); bf = f; }
    }
    return bf;
}
static float radioRms (const float* x, int n) { double s = 0; for (int i = 0; i < n; ++i) s += x[i] * x[i]; return (float) sqrt (s / n); }
static int radiotest (const char* dir)
{
    int fails = 0;
#define CHECK(c, ...) do { const bool ok_ = (c); printf ("%s ", ok_ ? "PASS" : "FAIL"); printf (__VA_ARGS__); printf ("\n"); if (! ok_) ++fails; } while (0)
    auto readFile = [] (const std::string& p) { std::vector<unsigned char> d; FILE* f = fopen (p.c_str(), "rb"); if (! f) return d; int c; while ((c = fgetc (f)) != EOF) d.push_back ((unsigned char) c); fclose (f); return d; };
    auto peakHz = radioPeakHz; auto rms = radioRms;

    // ---- the decoder: an MP3 at 44.1 kHz stereo, 1 kHz
    {
        auto dec = std::make_unique<RadioDec>(); dec->reset();
        auto mp = readFile (std::string (dir) + "/radio_1k.mp3");
        std::vector<short> out; short buf[4096];
        size_t pos = 0;
        while (pos < mp.size())
        {
            const int n = std::min ((int) (mp.size() - pos), std::min (1500, dec->inRoom()));
            dec->mp3Feed (mp.data() + pos, n); pos += n;
            int k; while ((k = dec->take (buf, 4096)) > 0) out.insert (out.end(), buf, buf + k);
            while (dec->mp3Work()) { while ((k = dec->take (buf, 4096)) > 0) out.insert (out.end(), buf, buf + k); }
        }
        int k; while ((k = dec->take (buf, 4096)) > 0) out.insert (out.end(), buf, buf + k);
        printf ("mp3: rate %d ch %d kbps %d frames %u skipped %u out %zu (%.2f s)\n", dec->rate, dec->chans, dec->kbps, dec->frames, dec->skipped, out.size(), out.size() / 24000.0);
        CHECK (dec->rate == 44100 && dec->chans == 2, "mp3 header 44100/2");
        CHECK (fabs (out.size() / 24000.0 - 3.0) < 0.25, "mp3 duration ~3 s at 24 kHz (%.3f)", out.size() / 24000.0);
        std::vector<float> f (8192); for (int i = 0; i < 8192; ++i) f[i] = out[24000 + i] / 32768.0f;
        const float hz = peakHz (f.data(), 8192, 24000, 500, 2000);
        CHECK (fabs (hz - 1000) <= 5, "mp3 tone 1000 Hz (%.0f)", hz);
        printf ("    level rms %.3f\n", rms (f.data(), 8192));
        // mono 22.05 kHz 440
        dec->restart();
        auto m2 = readFile (std::string (dir) + "/radio_440_22k.mp3");
        out.clear();
        dec->mp3Feed (m2.data(), std::min ((int) m2.size(), dec->inRoom()));
        for (pos = std::min ((size_t) dec->IN_CAP, m2.size()); ; )
        {
            while ((k = dec->take (buf, 4096)) > 0) out.insert (out.end(), buf, buf + k);
            if (! dec->mp3Work())
            {
                if (pos >= m2.size()) break;
                const int n = std::min ((int) (m2.size() - pos), dec->inRoom()); dec->mp3Feed (m2.data() + pos, n); pos += n;
            }
        }
        while ((k = dec->take (buf, 4096)) > 0) out.insert (out.end(), buf, buf + k);
        for (int i = 0; i < 8192; ++i) f[i] = out[12000 + i] / 32768.0f;
        const float hz2 = peakHz (f.data(), 8192, 24000, 200, 1000);
        CHECK (dec->rate == 22050 && dec->chans == 1 && fabs (hz2 - 440) <= 5, "mp3 22.05k mono 440 (%d, %.0f), %.2f s", dec->rate, hz2, out.size() / 24000.0);
        // ADPCM: a 700 Hz sine at 12001.135 Hz, encoded here, decoded
        dec->restart();
        std::vector<unsigned char> ad; int idx = 0, prev = 0;
        static const short ST[89] = { 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 };
        static const int IX[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };
        auto enc = [&] (int s) {
            int d = s - prev, code = 0; if (d < 0) { code = 8; d = -d; }
            const int st = ST[idx]; int diff = st >> 3;
            if (d >= st) { code |= 4; d -= st; diff += st; }
            if (d >= st >> 1) { code |= 2; d -= st >> 1; diff += st >> 1; }
            if (d >= st >> 2) { code |= 1; diff += st >> 2; }
            prev += (code & 8) ? -diff : diff; prev = std::max (-32768, std::min (32767, prev));
            idx = std::max (0, std::min (88, idx + IX[code])); return code; };
        const float kr = 12001.135f;
        for (int i = 0; i < 24000; i += 2)
        {
            const int a = enc ((int) (12000 * sin (2 * M_PI * 700 * i / kr))), b = enc ((int) (12000 * sin (2 * M_PI * 700 * (i + 1) / kr)));
            ad.push_back ((unsigned char) (a | (b << 4)));
        }
        out.clear();
        for (size_t i = 0; i < ad.size(); i += 512) { dec->adpcmFeed (ad.data() + i, (int) std::min ((size_t) 512, ad.size() - i), kr); while ((k = dec->take (buf, 4096)) > 0) out.insert (out.end(), buf, buf + k); }
        for (int i = 0; i < 8192; ++i) f[i] = out[8000 + i] / 32768.0f;
        const float hz3 = peakHz (f.data(), 8192, 24000, 300, 1500);
        CHECK (fabs (hz3 - 700) <= 5 && fabs (out.size() / 24000.0 - 24000 / kr) < 0.02, "adpcm 700 Hz (%.0f), %.3f s", hz3, out.size() / 24000.0);
    }
    // ---- the engine: a receiver fed a 500 Hz tone in real time, a track on LIVE
    auto E = std::make_unique<Engine>();
    Engine& e = *E;
    e.init (48000.0f);
    printf ("sizeof Engine %.1f MB, radio %.1f MB\n", sizeof (Engine) / 1048576.0, (sizeof (e.radio) + sizeof (e.radLoop)) / 1048576.0);
    float sd[NUM_SIDES]; machineSideDefaults (M_RADIO, sd);
    for (int q = 0; q < NUM_SIDES; ++q) e.setParam (0, sideParam (q), sd[q]);
    e.setParam (0, P_MACH, machineVal (M_RADIO));
    e.setParam (0, P_A, 0.1f); e.setParam (0, P_B, 0.1f); e.setParam (0, P_C, 0.99f);   // RX 1, LIVE, FREE
    e.setParam (0, P_D, 0.0f); e.setParam (0, P_E, 0.0f); e.setParam (0, P_F, 0.1f);
    e.setParam (0, P_DEC, 1.0f); e.setParam (0, P_TUNE, 0.5f);
    e.setParam (0, P_VOL, 0.8f);
    e.setParam (0, P_FREQ, 1.0f);
    e.radioSetState (0, RS_BUFFER, RK_WEB);
    CHECK (e.radioUse() == 1, "radioUse mask (%d)", e.radioUse());
    double ph = 0; float srcHz = 500.0f;
    std::vector<float> L (128), R (128), cap;
    long long fedN = 0; bool pulse = false;
    auto feed = [&] (int n, float hz) { std::vector<short> b (n); for (int i = 0; i < n; ++i) { const double a = pulse && (fedN % 4800) >= 1200 ? 0.01 : 1.0; b[i] = (short) (16000 * a * sin (ph)); ph += 2 * M_PI * hz / 24000.0; ++fedN; } return e.radioPush (0, b.data(), n); };
    // the network's burst: 2 s at once, then in time (with jitter)
    feed (48000, srcHz);
    e.radioSetState (0, RS_LIVE, RK_WEB);
    e.audition (0, 1.0f);
    double due = 0; int blocks = 0, start = 0;
    auto run = [&] (float secs, float hz, float jitter, bool capture) {
        const int nb = (int) (secs * 48000 / 128);
        for (int b = 0; b < nb; ++b)
        {
            due += 128 * 24000.0 / 48000.0 * (1.0 + jitter);
            if ((blocks % 20) == 0) { const int k = (int) due; if (k > 0) { feed (k, hz); due -= k; } }
            e.process (L.data(), R.data(), 128, nullptr);
            if (capture) cap.insert (cap.end(), L.begin(), L.end());
            ++blocks;
        }
    };
    run (3.0f, srcHz, 0.0f, true);
    float ri[RADIO_RX * RADIO_RX_INFO]; e.radioRxInfo (ri);
    printf ("rx0: state %.0f sig %.2f level %.2f fill %.2f s ratio %.4f under %.0f priming %.0f\n", ri[0], ri[1], ri[2], ri[3], ri[4], ri[5], ri[7]);
    CHECK (ri[0] == RS_LIVE && ri[1] > 0.9f && ri[7] == 0, "rx live, signal there");
    CHECK (ri[3] > 0.6f && ri[3] < 1.6f, "fill near its target (%.2f)", ri[3]);
    {
        const int n = 16384; const float* x = cap.data() + cap.size() - n;
        const float hz = peakHz (x, n, 48000, 200, 1500);
        CHECK (fabs (hz - 500) <= 5, "LIVE plays the receiver (%.0f Hz), rms %.3f", hz, rms (x, n));
    }
    // a clock 0.3 % fast: the engine keeps up (no dropouts, fill held)
    cap.clear();
    run (60.0f, srcHz, 0.003f, false);
    e.radioRxInfo (ri);
    printf ("after 20 s fast clock: fill %.2f ratio %.4f under %.0f\n", ri[3], ri[4], ri[5]);
    CHECK (ri[5] == 0 && ri[3] < 1.6f && ri[4] > 1.001f, "drift taken up (ratio %.4f)", ri[4]);
    run (60.0f, srcHz, -0.003f, false);
    e.radioRxInfo (ri);
    printf ("after 20 s slow clock: fill %.2f ratio %.4f under %.0f\n", ri[3], ri[4], ri[5]);
    CHECK (ri[5] == 0 && ri[3] > 0.5f && ri[4] < 0.999f, "drift taken up slow (ratio %.4f)", ri[4]);
    // TUNE on LIVE: +8 st of the knob -> +200 Hz
    e.setParam (0, P_TUNE, 0.5f + 8.0f / 48.0f);
    run (0.3f, srcHz, 0.0f, false);
    cap.clear(); run (0.5f, srcHz, 0.0f, true);
    {
        const int n = 16384; const float* x = cap.data() + cap.size() - n;
        const float hz = peakHz (x, n, 48000, 200, 1500);
        CHECK (fabs (hz - 700) <= 6, "TUNE shifts LIVE up 200 Hz (%.0f)", hz);
    }
    e.setParam (0, P_TUNE, 0.5f - 8.0f / 48.0f);
    run (0.3f, srcHz, 0.0f, false);
    cap.clear(); run (0.5f, srcHz, 0.0f, true);
    {
        const int n = 16384; const float* x = cap.data() + cap.size() - n;
        const float hz = peakHz (x, n, 48000, 100, 1500);
        CHECK (fabs (hz - 300) <= 6, "TUNE shifts LIVE down 200 Hz (%.0f)", hz);
    }
    e.setParam (0, P_TUNE, 0.5f);
    // underrun: the network stops 3 s: silence, then back
    {
        const int nb = (int) (3.0 * 48000 / 128);
        cap.clear();
        for (int b = 0; b < nb; ++b) { e.process (L.data(), R.data(), 128, nullptr); cap.insert (cap.end(), L.begin(), L.end()); }
        e.radioRxInfo (ri);
        CHECK (ri[5] >= 1 && ri[7] == 1, "dry network: a dropout, waiting (%.0f)", ri[5]);
        CHECK (rms (cap.data() + cap.size() - 4800, 4800) < 1e-4f, "silent while dry");
        due = 0; run (2.5f, srcHz, 0.0f, false);
        cap.clear(); run (0.5f, srcHz, 0.0f, true);
        CHECK (rms (cap.data(), (int) cap.size()) > 0.05f, "back after the dropout (rms %.3f)", rms (cap.data(), (int) cap.size()));
    }
    // LOOP: grab 1/4 bar of a tone, then the receiver changes pitch: the loop keeps the old one
    e.setParam (0, P_B, 0.5f);               // LOOP
    e.setParam (0, P_C, (3.5f) / 11.0f);     // 1/4 (4 sixteenths)
    e.setParam (0, P_D, 0.0f);               // GRAB 0: the first stays
    srcHz = 800.0f; run (1.5f, srcHz, 0.0f, false);   // (the receiver now on 800 Hz, 1.5 s in hand passed)
    e.audition (0, 1.0f);
    srcHz = 300.0f;
    cap.clear(); run (2.5f, srcHz, 0.0f, true);
    { float ii[RADIO_INFO]; e.radioInfo (0, ii); printf ("  info L %.2f pos %.2f lvl %.2f act %d\n", ii[8], ii[9], ii[10], e.tr[0].voice[e.tr[0].cur].active); }
    {
        const int n = 16384; const float* x = cap.data() + cap.size() - n;
        const float hz = peakHz (x, n, 48000, 100, 1500);
        CHECK (fabs (hz - 800) <= 6, "LOOP keeps what it took (%.0f Hz, the receiver on 300), rms %.3f", hz, rms (x, n));
        // continuity: no big jumps sample to sample (the seams crossfaded)
        float mx = 0; for (size_t i = 1; i < cap.size(); ++i) mx = std::max (mx, fabsf (cap[i] - cap[i - 1]));
        printf ("    max step %.4f\n", mx);
    }
    // a second trig, GRAB 0: still the old loop; GRAB 1: the new sound
    e.audition (0, 1.0f);
    cap.clear(); run (0.5f, srcHz, 0.0f, true);
    CHECK (fabs (peakHz (cap.data() + cap.size() - 16384, 16384, 48000, 100, 1500) - 800) <= 6, "GRAB 0: the loop stays");
    e.setParam (0, P_D, 1.0f);
    run (1.5f, srcHz, 0.0f, false);
    e.audition (0, 1.0f);
    cap.clear(); run (0.5f, srcHz, 0.0f, true);
    CHECK (fabs (peakHz (cap.data() + cap.size() - 16384, 16384, 48000, 100, 1500) - 300) <= 6, "GRAB 1: a new loop");
    // LOOP transposed up an octave (TUNE +12)
    e.setParam (0, P_TUNE, 0.5f + 12.0f / 48.0f);
    e.setParam (0, P_D, 0.0f);
    e.audition (0, 1.0f);
    cap.clear(); run (0.5f, srcHz, 0.0f, true);
    CHECK (fabs (peakHz (cap.data() + cap.size() - 16384, 16384, 48000, 100, 1500) - 600) <= 8, "LOOP transposed an octave (300 -> 600)");
    e.setParam (0, P_TUNE, 0.5f);
    // DIG: takes from further back (the receiver now 1200 Hz for 1 s, before it 300 for long: a deep dig finds 300)
    e.setParam (0, P_B, 0.9f); e.setParam (0, P_D, 1.0f);
    srcHz = 1200.0f; run (2.5f, srcHz, 0.0f, false);
    int found300 = 0, found1200 = 0;
    for (int k = 0; k < 12; ++k)
    {
        e.audition (0, 1.0f);
        cap.clear(); run (0.25f, srcHz, 0.0f, true);
        const float hz = peakHz (cap.data() + cap.size() - 8192, 8192, 48000, 100, 1500);
        if (fabs (hz - 1200) > 10) ++found300; else ++found1200;
    }
    CHECK (found300 >= 6, "DIG digs back (%d of 12 found the older sound, %d the newer)", found300, found1200);
    // the AIR: band, fading, crackle, whistle, drive; static alone when nothing comes; no NaN
    e.setParam (0, P_B, 0.1f); e.setParam (0, P_C, 0.99f); e.setParam (0, P_D, 0.0f);
    for (int q = 0; q < RA_NSIDE; ++q) e.setParam (0, sideParam (q), 0.8f);
    e.setParam (0, P_E, 0.6f);
    e.audition (0, 1.0f);
    cap.clear(); run (3.0f, srcHz, 0.0f, true);
    bool bad = false; float pk = 0; for (float s : cap) { if (! std::isfinite (s)) bad = true; pk = std::max (pk, fabsf (s)); }
    CHECK (! bad && pk < 1.5f && pk > 0.01f, "AIR all up: finite, peak %.3f", pk);
    e.radioSetState (0, RS_OFF, RK_WEB);
    e.setParam (0, P_D, 0.0f);
    run (0.5f, srcHz, 0.0f, false);
    cap.clear(); run (1.0f, srcHz, 0.0f, true);
    CHECK (rms (cap.data(), (int) cap.size()) > 0.005f, "STAT alone when off (rms %.3f)", rms (cap.data(), (int) cap.size()));
    for (int q = 0; q < RA_NSIDE; ++q) e.setParam (0, sideParam (q), sd[q]);
    e.setParam (0, P_E, 0.0f);
    cap.clear(); run (1.0f, srcHz, 0.0f, true);
    CHECK (rms (cap.data() + 4800, (int) cap.size() - 4800) < 1e-5f, "silent when off, STAT 0 (%.6f)", rms (cap.data() + 4800, (int) cap.size() - 4800));
    // SQL: a pulsing source (on 50 ms, off 150 ms at -40 dB): the squelch shuts the quiet part
    e.radioSetState (0, RS_LIVE, RK_WEB);
    e.setParam (0, P_D, 0.55f);   // -35 dB
    pulse = true;
    e.audition (0, 1.0f);
    run (2.0f, srcHz, 0.0f, false);
    cap.clear(); run (2.0f, srcHz, 0.0f, true);
    {
        // in 10 ms windows: loud ones near the source's peak, the rest near silence (the -40 dB part shut)
        int loud = 0, quiet = 0, mid = 0;
        for (size_t i = 0; i + 480 <= cap.size(); i += 480) { const float r = rms (cap.data() + i, 480); if (r > 0.05f) ++loud; else if (r < 0.0005f) ++quiet; else ++mid; }
        CHECK (quiet >= 30 && loud > 0, "SQL shuts the quiet part (loud %d quiet %d between %d)", loud, quiet, mid);
    }
    e.setParam (0, P_D, 0.0f);
    cap.clear(); run (1.0f, srcHz, 0.0f, true);
    {
        int quiet = 0; for (size_t i = 0; i + 480 <= cap.size(); i += 480) if (rms (cap.data() + i, 480) < 0.0005f) ++quiet;
        CHECK (quiet == 0, "SQL off: the quiet part heard (%d silent windows)", quiet);
    }
    pulse = false;
    run (1.5f, srcHz, 0.0f, false);
    // LEN on LIVE: a 1/16 gate (6000 samples at 120 BPM, 48 kHz)
    e.setParam (0, P_C, 1.5f / 11.0f);   // (2 sixteenths: 12000 samples)
    e.audition (0, 1.0f);
    cap.clear(); run (0.5f, srcHz, 0.0f, true);
    {
        const float a = rms (cap.data() + 1000, 10000), b = rms (cap.data() + 14000, 4000);
        CHECK (a > 0.05f && b < 1e-4f, "LEN gates LIVE (%.3f then %.5f)", a, b);
    }
    // 90 settings at random (30 at 44.1, 48 and 96 kHz), the knobs and the modes jumping, trigs and pads, the feed
    // with dropouts: nothing infinite, bounded
    {
        float worst = 0; bool bad = false;
        Rng rr; rr.seed (77);
        for (const float sr : { 44100.0f, 48000.0f, 96000.0f })
        {
            auto E2 = std::make_unique<Engine>(); Engine& f = *E2;
            f.init (sr);
            for (int q = 0; q < NUM_SIDES; ++q) f.setParam (0, sideParam (q), sd[q]);
            f.setParam (0, P_MACH, machineVal (M_RADIO)); f.setParam (0, P_VOL, 0.8f);
            f.radioSetState (0, RS_LIVE, RK_SW);
            double ph2 = 0;
            std::vector<float> l (128), r (128);
            for (int pass = 0; pass < 30; ++pass)
            {
                for (int p : { P_TUNE, P_DEC, P_A, P_B, P_C, P_D, P_E, P_F }) f.setParam (0, p, rr.uni());
                f.setParam (0, P_A, 0.1f);
                for (int q = 0; q < RA_NSIDE; ++q) f.setParam (0, sideParam (q), rr.uni());
                if (rr.uni() < 0.3f) f.radioFlush (0);
                for (int b = 0; b < (int) (0.25f * sr / 128); ++b)
                {
                    if ((b & 7) == 0 && rr.uni() < 0.9f)
                    {
                        std::vector<short> fb (1600);
                        for (auto& x : fb) { x = (short) (12000 * sin (ph2) + 4000 * rr.bi()); ph2 += 2 * M_PI * 640 / 24000.0; }
                        f.radioPush (0, fb.data(), (int) fb.size());
                    }
                    if ((b % 23) == 0) f.audition (0, 0.5f + 0.5f * rr.uni());
                    if ((b % 41) == 0) f.release (0);
                    f.process (l.data(), r.data(), 128, nullptr);
                    for (int i = 0; i < 128; ++i) { if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) bad = true; worst = std::max (worst, std::max (fabsf (l[i]), fabsf (r[i]))); }
                }
            }
        }
        CHECK (! bad && worst < 2.0f, "90 settings at random at 44.1, 48 and 96 kHz: finite, peak %.2f", worst);
    }
    // the cost: a RADIO track on LIVE with all its AIR and the shift, its receiver fed (native, 48 kHz)
    {
        auto E3 = std::make_unique<Engine>(); Engine& f = *E3;
        f.init (48000.0f);
        for (int q = 0; q < NUM_SIDES; ++q) f.setParam (0, sideParam (q), 0.7f);
        f.setParam (0, P_MACH, machineVal (M_RADIO)); f.setParam (0, P_VOL, 0.8f); f.setParam (0, P_DEC, 1.0f);
        f.setParam (0, P_A, 0.1f); f.setParam (0, P_B, 0.1f); f.setParam (0, P_C, 0.99f); f.setParam (0, P_E, 0.4f); f.setParam (0, P_TUNE, 0.6f);
        for (int t = 1; t < NUM_TRACKS; ++t) f.setParam (t, P_VOL, 0.0f);
        f.radioSetState (0, RS_LIVE, RK_WEB);
        std::vector<short> fb (24000); for (size_t i = 0; i < fb.size(); ++i) fb[i] = (short) (10000 * sin (i * 0.1));
        std::vector<float> l (128), r (128);
        auto t0 = std::chrono::high_resolution_clock::now();
        const int secs = 10;
        f.audition (0, 1.0f);
        for (int b = 0; b < secs * 48000 / 128; ++b) { if ((b % 187) == 0) f.radioPush (0, fb.data(), 24000); f.process (l.data(), r.data(), 128, nullptr); }
        const double el = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        // (the same engine without the track, to take the rest of the engine out)
        f.setParam (0, P_MACH, machineVal (M_BD_VA)); f.setParam (0, P_VOL, 0.0f);
        auto t1 = std::chrono::high_resolution_clock::now();
        for (int b = 0; b < secs * 48000 / 128; ++b) f.process (l.data(), r.data(), 128, nullptr);
        const double el0 = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t1).count();
        printf ("INFO RADIO on LIVE, all its AIR: %.2f %% of real time (the engine around it: %.2f %%)\n", 100.0 * (el - el0) / secs, 100.0 * el0 / secs);
    }
    printf ("%d failed\n", fails);
#undef CHECK
    return fails ? 1 : 0;
}


// site (v87): every machine's own sound, as the site's Machines page draws it (site/machines-data.json). Each one plays
// once at its defaults (the page's TUNE, DEC and A-F, its side pages), dry (no reverb or delay sends); an FX machine
// takes a snare from the track before it; RAM and BREAK play a short synthetic drum loop. Kept per machine: the
// envelope (200 columns over the time it sounds, 2 s at most, 0-255) and 24 frames of the waveform (128 points each,
// -127..127), base64.
//   render_test site <decs.txt: "tune dec" a line, in the machines' order> <out.json>
static std::string b64 (const std::vector<unsigned char>& v)
{
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o;
    for (size_t i = 0; i < v.size(); i += 3)
    {
        const unsigned n = (unsigned) v[i] << 16 | (i + 1 < v.size() ? (unsigned) v[i + 1] << 8 : 0u) | (i + 2 < v.size() ? v[i + 2] : 0u);
        o += T[n >> 18 & 63]; o += T[n >> 12 & 63];
        o += i + 1 < v.size() ? T[n >> 6 & 63] : '='; o += i + 2 < v.size() ? T[n & 63] : '=';
    }
    return o;
}
static std::vector<float> drumLoop (double sr, double secs)
{
    const int n = (int) (secs * sr); std::vector<float> x ((size_t) n, 0.0f);
    const char* pat = "K.H.S.HKK.H.S.HH";
    unsigned seed = 7; auto rnd = [&] { seed = seed * 1103515245u + 12345u; return (float) (seed >> 8) / 8388608.0f - 1.0f; };
    for (int k = 0; k < 16; ++k)
    {
        const char c = pat[k]; if (c == '.') continue;
        const int t0 = (int) ((double) k * n / 16); double ph = 0; float last = 0;
        for (int i = 0; i < (int) (0.35 * sr) && t0 + i < n; ++i)
        {
            const double t = i / sr; const float nz = rnd(), hp = nz - last; last = nz;
            float y = 0;
            if (c == 'K') { ph += 6.283185307 * (50 + 120 * std::exp (-t / 0.03)) / sr; y = 0.9f * (float) (std::sin (ph) * std::exp (-t / 0.2)); }
            if (c == 'S') y = (float) (0.5 * nz * std::exp (-t / 0.1) + 0.35 * std::sin (6.283185307 * 190 * t) * std::exp (-t / 0.06));
            if (c == 'H') y = (float) (0.3 * hp * std::exp (-t / 0.03));
            x[(size_t) (t0 + i)] += 0.6f * y;
        }
    }
    return x;
}
static int sitedata (const char* decsPath, const char* outPath)
{
    loadBank();
    REF = new Engine(); REF->init (48000.f);
    Engine* e = new Engine(); e->init (48000.f);
    FILE* dv = fopen (decsPath, "r"); if (! dv) { printf ("no %s\n", decsPath); return 1; }
    float tune[NUM_MACHINES], dec[NUM_MACHINES];
    for (int m = 0; m < NUM_MACHINES; ++m) if (fscanf (dv, "%f %f", &tune[m], &dec[m]) != 2) { tune[m] = 0.5f; dec[m] = 0.5f; }
    fclose (dv);
    FILE* f = fopen (outPath, "w");
    fprintf (f, "{\n");
    const int SR = 48000, N = 2 * SR, COLS = 200, FR = 24, PTS = 128, WIN = 1024;
    const std::vector<float> loop48 = drumLoop (48000.0, 2.0), loop32 = drumLoop (32000.0, 2.0);
    for (int m = 0; m < NUM_MACHINES; ++m)
    {
        softReset (e, 48000.f);
        for (int t = 0; t < NUM_TRACKS; ++t) { e->setParam (t, P_VOL, 0.0f); e->setParam (t, P_REV, 0.0f); e->setParam (t, P_DLY, 0.0f); }
        const bool fx = fxMachine (m);
        const int t = fx ? 1 : 0;
        auto setup = [&] (int tr, int mm)
        {
            e->setParam (tr, P_MACH, machineVal (mm)); e->setParam (tr, P_VOL, 0.8f);
            e->setParam (tr, P_TUNE, tune[mm]); e->setParam (tr, P_DEC, dec[mm]);
            float d[6]; const int k = machineAFDefaults (mm, d); for (int q = 0; q < k; ++q) e->setParam (tr, P_A + q, d[q]);
            float sd[NUM_SIDES]; const int ks = machineSideDefaults (mm, sd); for (int q = 0; q < ks; ++q) e->setParam (tr, sideParam (q), sd[q]);
        };
        if (fx) setup (0, M_SD_VA);
        setup (t, m);
        if (m == M_RAM)
        {
            for (int i = 0; i < (int) RAM_LEN && i < (int) loop48.size(); ++i) e->ramMem[t][i] = (short) (std::max (-1.0f, std::min (1.0f, loop48[(size_t) i])) * 32000.0f);
            e->ramLoaded (t, (int) loop48.size(), 48000.0f);
        }
        if (m == M_BREAK)
        {
            short* mem = e->brk.mem (0);
            for (size_t i = 0; i < loop32.size(); ++i) mem[i] = (short) (std::max (-1.0f, std::min (1.0f, loop32[i])) * 32000.0f);
            e->breakSet (0, (int) loop32.size(), 1);
        }
        if (m == M_BREAK) { for (int st = 0; st < 16; ++st) e->setTrig (t, st, 1); e->setGlobal (G_BPM, 120.0f); e->setPlay (1); }   // (a slice a step)
        else e->audition (fx ? 0 : t, 1.0f);
        std::vector<float> x; x.reserve (N);
        std::vector<float> L (128), R (128);
        for (int i = 0; i < N; i += 128) { e->process (L.data(), R.data(), 128, nullptr); for (int k = 0; k < 128; ++k) x.push_back (0.5f * (L[(size_t) k] + R[(size_t) k])); }
        float peak = 1e-6f; for (float v : x) peak = std::max (peak, std::fabs (v));
        // how long it sounds: to where it stays 40 dB under its peak (a quarter of a second at least, 2 s at most)
        int last = 0; for (int i = 0; i < N; ++i) if (std::fabs (x[(size_t) i]) > 0.01f * peak) last = i;
        const int D = std::max (SR / 4, std::min (N, (int) (last * 1.1) + SR / 50));
        std::vector<unsigned char> env (COLS), sc ((size_t) FR * PTS);
        for (int c = 0; c < COLS; ++c)
        {
            float p = 0; for (int i = c * D / COLS; i < (c + 1) * D / COLS; ++i) p = std::max (p, std::fabs (x[(size_t) i]));
            env[(size_t) c] = (unsigned char) std::lround (255.0f * p / peak);
        }
        // the frames: a window of 1024 samples (21 ms: a cycle of a 47 Hz kick), its points the strongest sample of each bin
        for (int fr = 0; fr < FR; ++fr)
        {
            const int at = (int) ((fr + 0.25) * D / FR);
            for (int q = 0; q < PTS; ++q)
            {
                float a = 0; for (int j = 0; j < WIN / PTS; ++j) { const float v = x[(size_t) std::max (0, std::min (N - 1, at + q * (WIN / PTS) + j))]; if (std::fabs (v) > std::fabs (a)) a = v; }
                sc[(size_t) (fr * PTS + q)] = (unsigned char) (signed char) std::lround (std::max (-127.0f, std::min (127.0f, 127.0f * a / peak)));
            }
        }
        printf ("%-8s peak %.3f%s\n", MNAMES[m], peak, fx ? " (fx on a snare)" : "");
        fprintf (f, "  \"%d\": {\"name\": \"%s\", \"peak\": %.4f, \"dur\": %.3f, \"fx\": %s, \"env\": \"%s\", \"scope\": \"%s\"}%s\n", m, MNAMES[m], peak,
                 (double) D / SR, fx ? "true" : "false", b64 (env).c_str(), b64 (sc).c_str(), m + 1 < NUM_MACHINES ? "," : "");
    }
    fprintf (f, "}\n");
    fclose (f);
    delete e;
    return 0;
}

int main (int argc, char** argv)
{
    if (argc >= 4 && ! strcmp (argv[1], "site")) return sitedata (argv[2], argv[3]);
    if (argc >= 2 && ! strcmp (argv[1], "master")) return mastertest();
    if (argc >= 2 && ! strcmp (argv[1], "org")) return orgtest();
    if (argc >= 2 && ! strcmp (argv[1], "stress")) return stress();
    if (argc >= 2 && ! strcmp (argv[1], "info")) return info();
    if (argc >= 2 && ! strcmp (argv[1], "fx")) return fxtest();
    if (argc >= 2 && ! strcmp (argv[1], "kit")) return kittest();
    if (argc >= 2 && ! strcmp (argv[1], "mod")) return modtest();
    if (argc >= 2 && ! strcmp (argv[1], "glitch")) return glitchtest();
    if (argc >= 2 && ! strcmp (argv[1], "halo")) return halotest();
    if (argc >= 2 && ! strcmp (argv[1], "break")) return breaktest();
    if (argc >= 2 && ! strcmp (argv[1], "fxm")) return fxmachtest();
    if (argc >= 2 && ! strcmp (argv[1], "pat")) return pattest();
    if (argc >= 2 && ! strcmp (argv[1], "harm")) return harmtest();
    if (argc >= 2 && ! strcmp (argv[1], "strum")) return strumtest();
    if (argc >= 2 && ! strcmp (argv[1], "choir")) return choirtest();
    if (argc >= 2 && ! strcmp (argv[1], "follow")) return followtest();
    if (argc >= 2 && ! strcmp (argv[1], "age")) return agetest();
    if (argc >= 2 && ! strcmp (argv[1], "alloy")) return alloytest();
    if (argc >= 2 && ! strcmp (argv[1], "spread")) return spreadtest();
    if (argc >= 2 && ! strcmp (argv[1], "rdec")) return rdectest();
    if (argc >= 2 && ! strcmp (argv[1], "chimera")) return chimtest();
    if (argc >= 2 && ! strcmp (argv[1], "v68")) return v68test();
    if (argc >= 2 && ! strcmp (argv[1], "v71")) return v71test();
    if (argc >= 2 && ! strcmp (argv[1], "v73")) return v73test (argc >= 3 && ! strcmp (argv[2], "table"));
    if (argc >= 2 && ! strcmp (argv[1], "cube")) return cubetest (argc >= 3 && ! strcmp (argv[2], "table"));
    if (argc >= 2 && ! strcmp (argv[1], "west")) return westtest();
    if (argc >= 2 && ! strcmp (argv[1], "weft")) return wefttest();
    if (argc >= 2 && ! strcmp (argv[1], "ecrv")) return ecrvtest();
    if (argc >= 2 && ! strcmp (argv[1], "duck")) return ducktest();
    if (argc >= 2 && ! strcmp (argv[1], "howl")) return howltest();
    if (argc >= 2 && ! strcmp (argv[1], "stencil")) return stenciltest();
    if (argc >= 2 && ! strcmp (argv[1], "mantra")) return mantratest();
    if (argc >= 2 && ! strcmp (argv[1], "radio")) return radiotest (argc >= 3 ? argv[2] : "tools/tests");
    if (argc >= 2 && ! strcmp (argv[1], "v70")) { if (argc >= 3) AMP70 = atof (argv[2]); return v70test(); }
    if (argc >= 5 && ! strcmp (argv[1], "ops")) return opsRender (argv[2], argv[3], (float) atof (argv[4]), argc >= 6 ? atoi (argv[5]) : -1);
    printf ("usage: render_test stress | kit | mod | ops <file> <wav> <seconds> [solo]\n");
    return 1;
}
