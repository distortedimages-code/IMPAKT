// IMPAKT — GRAIN, master effect: drives the Beads processor (mi/beads, MIT, © Emilie Gillet) like beads.cc
// does, on the stereo master bus after the compressor. Differences from the module: no input gain control
// (the bus is already at line level), the module runs fully wet and DRY/WET is applied here (same law, the
// dry side untouched by the module's limiters), the four attenurandomizers follow one RND control, the
// codec rates are reached by resampling (48 kHz, or 32 kHz in VINT), and the SEED input can take the
// transport (a clock every 1, 4 or 16 steps).
#include "sk_grain.h"

#include <cmath>
#include <new>

#include "stmlib/stmlib.h"
#include "stmlib/utils/gate_flags.h"
#include "stmlib/dsp/dsp.h"
#include "beads/dsp/processor.h"

namespace sk {

namespace {

using namespace beads;

const int kBlock = 24;                         // the module's codec block
const int kBank = 262144, kBanks = 3;          // the module's SRAM: 3 banks of 256 kB
const int kReverbWords = 32768;
const float kWetTrim = 2.4f;                   // the wet side at about the level of a loud bus (measured on the demo)
const Processor::Media kMedia[GRAIN_MEDIA] = {
    Processor::MEDIA_DIGITAL, Processor::MEDIA_VINTAGE_DIGITAL, Processor::MEDIA_CLEAN_TAPE, Processor::MEDIA_DIRTY_TAPE };

// 2nd-order Butterworth low-pass (two in series for the anti-aliasing of the resamplers)
struct Lp
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (float fc)                          // fc: fraction of the sample rate (0 = bypass)
    {
        z1 = z2 = 0.0f;
        if (fc <= 0.0f || fc >= 0.49f) { b0 = 1; b1 = b2 = a1 = a2 = 0; return; }
        const float k = 1.0f / std::tan (3.14159265f * fc), q = 0.70710678f;
        const float n = 1.0f / (1.0f + k / q + k * k);
        b0 = n; b1 = 2.0f * n; b2 = n; a1 = 2.0f * (1.0f - k * k) * n; a2 = (1.0f - k / q + k * k) * n;
    }
    float run (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

inline float hermite (float xm, float x0, float x1, float x2, float t)
{
    const float c1 = 0.5f * (x1 - xm);
    const float c2 = xm - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

struct GrainUnit
{
    Processor proc;
    alignas(16) uint8_t bank[kBanks][kBank];
    uint16_t reverb[kReverbWords];

    float hostSr, codecSr;
    int media;
    // input: host rate -> codec rate (input driven: history of 4 host samples, position between h[1] and h[2])
    enum { IN_FIFO = 256, OUT_FIFO = 256 };
    float h[2][4]; double inT, inStep;
    Lp inLp[2][2];
    float inFifo[2][IN_FIFO]; int inN;
    float dryG, wetG;            // DRY / WET, smoothed
    // output: codec rate -> host rate (output driven, like MACRO)
    float outL[OUT_FIFO], outR[OUT_FIFO]; int outN; double outPos, outStep;
    Lp outLpL[2], outLpR[2];     // at the codec rate (before decimation) or at the host rate (after interpolation)
    bool outLpAtHost;
    // SEED: the transport's steps
    double clk; stmlib::GateFlags seed;
};

static_assert (sizeof (GrainUnit) <= GRAIN_BYTES, "GRAIN_BYTES too small");

inline GrainUnit* U (void* mem) { return static_cast<GrainUnit*> (mem); }

void configure (GrainUnit& u, int media)
{
    u.media = media;
    for (int b = 0; b < kBanks; ++b) for (int i = 0; i < kBank; ++i) u.bank[b][i] = 0;
    for (int i = 0; i < kReverbWords; ++i) u.reverb[i] = 0;
    u.proc.Configure (kMedia[media], false);     // stereo: the buffer holds 4 s in DIGI
    u.codecSr = u.proc.sample_rate();
    const float M = u.codecSr, R = u.hostSr;
    u.inStep = (double) R / (double) M;         // host samples per codec sample
    u.outStep = (double) M / (double) R;        // codec samples per host sample
    // anti-aliasing: before the codec when it runs slower than the host; after it, at the lower of the two rates
    const float fin = M < R ? 0.45f * M / R : 0.0f;
    for (auto& ch : u.inLp) for (auto& f : ch) f.set (fin);
    u.outLpAtHost = M < R;
    const float fout = M < R ? 0.45f * M / R : (R < M ? 0.45f * R / M : 0.0f);
    for (auto& f : u.outLpL) f.set (fout);
    for (auto& f : u.outLpR) f.set (fout);
    for (auto& ch : u.h) for (float& x : ch) x = 0.0f;
    u.inT = 0.0;
    // a little latency so a block of input is always there when the output needs one (~1 ms)
    u.inN = 2 * kBlock;
    for (int i = 0; i < u.inN; ++i) u.inFifo[0][i] = u.inFifo[1][i] = 0.0f;
    u.outN = 4; u.outPos = 1.0;
    for (int i = 0; i < u.outN; ++i) u.outL[i] = u.outR[i] = 0.0f;
}

// one codec block: kBlock samples from the input FIFO through the module, into the output FIFO
void runBlock (GrainUnit& u, const GrainControls& c, double incCodec, int running)
{
    FloatFrame in[kBlock], out[kBlock];
    const int take = u.inN < kBlock ? u.inN : kBlock;
    for (int i = 0; i < kBlock; ++i) { in[i].l = i < take ? u.inFifo[0][i] : 0.0f; in[i].r = i < take ? u.inFifo[1][i] : 0.0f; }
    for (int i = take; i < u.inN; ++i) { u.inFifo[0][i - take] = u.inFifo[0][i]; u.inFifo[1][i - take] = u.inFifo[1][i]; }
    u.inN -= take;

    stmlib::GateFlags seed[kBlock];
    bool freeze[kBlock];
    for (int i = 0; i < kBlock; ++i)
    {
        const double per = c.sync > 1 ? u.clk / (double) c.sync : u.clk;
        const bool g = running && per - std::floor (per) < 0.5;     // high for the first half of each period
        u.seed = seed[i] = stmlib::ExtractGateFlags (u.seed, g);
        u.clk += incCodec;
        freeze[i] = c.freeze != 0;
    }

    // the four attenurandomizers (no CV patched: positive = jumps to a random value on each grain)
    const float r = c.rnd < 0.0f ? 0.0f : (c.rnd > 1.0f ? 1.0f : c.rnd);
    Parameters p;
    p.time.base = c.time;   p.time.attenuverter = r;          p.time.cv = 0.0f;  p.time.has_cv = false;
    p.size.base = c.size;   p.size.attenuverter = 0.35f * r;  p.size.cv = 0.0f;  p.size.has_cv = false;
    p.shape.base = c.shape; p.shape.attenuverter = 0.25f * r; p.shape.cv = 0.0f; p.shape.has_cv = false;
    // PITCH: the module's knob law (detents on 0 / 1 / 3 / 4 / 7 / 12 / 24 semitones)
    {
        float detune = 2.0f * c.pitch - 1.0f;
        const float sign = detune < 0.0f ? -1.0f : 1.0f;
        detune = detune * sign * 6.0f;
        int di = (int) detune; if (di > 6) di = 6;
        float df = detune - (float) di;
        static const float intervals[8] = { 0.0f, 1.0f, 3.0f, 4.0f, 7.0f, 12.0f, 24.0f, 24.0f };
        df = (df - 0.33333333f) * 1.5f; if (df < 0.0f) df = 0.0f;
        const float a = intervals[di], b = intervals[di + 1];
        p.pitch.base = (a + (b - a) * stmlib::SmoothStep (df)) * sign + c.pitchMod;
    }
    p.pitch.attenuverter = -0.04f * r; p.pitch.cv = 0.0f; p.pitch.has_cv = false;   // a slight random detune
    p.monophonic_mode = false;

    ModulatedParameter d; d.base = c.density; d.cv = 0.0f;
    Processor::MixParameters m;
    m.feedback = c.feedback; m.dry_wet = 1.0f; m.reverb_amount = c.reverb; m.output_gate = false;

    u.proc.Process (SEED_BUTTON_STATE_LOCKED, c.sync ? seed : nullptr, freeze, d, p, m, in, out, kBlock);

    if (u.outN + kBlock > GrainUnit::OUT_FIFO) return;   // cannot happen (the output pulls one block at a time)
    for (int i = 0; i < kBlock; ++i)
    {
        float l = out[i].l, rr = out[i].r;
        if (! u.outLpAtHost) { l = u.outLpL[1].run (u.outLpL[0].run (l)); rr = u.outLpR[1].run (u.outLpR[0].run (rr)); }
        u.outL[u.outN] = l; u.outR[u.outN] = rr; ++u.outN;
    }
}

} // namespace

void grainInit (void* mem, float hostSr)
{
    GrainUnit* u = new (mem) GrainUnit();
    uint8_t* banks[kBanks] = { u->bank[0], u->bank[1], u->bank[2] };
    u->proc.Init (banks, u->reverb);
    u->hostSr = hostSr > 1000.0f ? hostSr : 48000.0f;
    u->clk = 0.0; u->seed = stmlib::GATE_FLAG_LOW;
    u->dryG = 1.0f; u->wetG = 0.0f;
    configure (*u, 0);
}

void grainSetRate (void* mem, float hostSr)
{
    GrainUnit& u = *U (mem);
    u.hostSr = hostSr > 1000.0f ? hostSr : 48000.0f;
    configure (u, u.media);
}

void grainClear (void* mem)
{
    GrainUnit& u = *U (mem);
    configure (u, u.media);
}

void grainProcess (void* mem, const GrainControls& c, float* L, float* R, int n, double pos, double inc, int running)
{
    GrainUnit& u = *U (mem);
    const int media = c.media < 0 ? 0 : (c.media >= GRAIN_MEDIA ? GRAIN_MEDIA - 1 : c.media);
    if (media != u.media) configure (u, media);

    // SEED clock: follows the transport, corrected gently (hard on a jump)
    const double incCodec = inc * u.inStep;
    if (running)
    {
        const double err = pos - u.clk;
        if (err > 0.25 || err < -0.25) u.clk = pos; else u.clk += err * 0.05;
    }

    // input -> codec rate
    for (int i = 0; i < n; ++i)
    {
        const float x[2] = { L[i], R[i] };
        for (int ch = 0; ch < 2; ++ch)
        {
            float* h = u.h[ch];
            h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; h[3] = u.inLp[ch][1].run (u.inLp[ch][0].run (x[ch]));
        }
        while (u.inT < 1.0)
        {
            if (u.inN < GrainUnit::IN_FIFO)
            {
                for (int ch = 0; ch < 2; ++ch) u.inFifo[ch][u.inN] = hermite (u.h[ch][0], u.h[ch][1], u.h[ch][2], u.h[ch][3], (float) u.inT);
                ++u.inN;
            }
            u.inT += u.inStep;
        }
        u.inT -= 1.0;
    }

    // DRY / WET with the module's law (both 0.75 at the middle), interpolated over the block
    const float m = c.mix < 0.0f ? 0.0f : (c.mix > 1.0f ? 1.0f : c.mix);
    const float dT = (1.0f - m) * (1.0f + m), wT = m * (2.0f - m) * kWetTrim;
    const float d0 = u.dryG, w0 = u.wetG, inv = 1.0f / (float) n;
    u.dryG = dT; u.wetG = wT;

    // codec -> host rate, pulling blocks through the module as needed
    for (int i = 0; i < n; ++i)
    {
        int ip = (int) u.outPos;
        while (ip + 2 >= u.outN)
        {
            if (u.outN + kBlock > GrainUnit::OUT_FIFO)
            {
                const int drop = ip - 1;
                for (int k = drop; k < u.outN; ++k) { u.outL[k - drop] = u.outL[k]; u.outR[k - drop] = u.outR[k]; }
                u.outN -= drop; u.outPos -= (double) drop; ip = 1;
            }
            runBlock (u, c, incCodec, running);
        }
        const float t = (float) (u.outPos - (double) ip);
        float l = hermite (u.outL[ip - 1], u.outL[ip], u.outL[ip + 1], u.outL[ip + 2], t);
        float r = hermite (u.outR[ip - 1], u.outR[ip], u.outR[ip + 1], u.outR[ip + 2], t);
        if (u.outLpAtHost) { l = u.outLpL[1].run (u.outLpL[0].run (l)); r = u.outLpR[1].run (u.outLpR[0].run (r)); }
        const float fr = (float) i * inv, dg = d0 + (dT - d0) * fr, wg = w0 + (wT - w0) * fr;
        L[i] = L[i] * dg + l * wg; R[i] = R[i] * dg + r * wg;
        u.outPos += u.outStep;
    }
    const int drop = (int) u.outPos - 1;
    if (drop > 0)
    {
        for (int k = drop; k < u.outN; ++k) { u.outL[k - drop] = u.outL[k]; u.outR[k - drop] = u.outR[k]; }
        u.outN -= drop; u.outPos -= (double) drop;
    }
}

} // namespace sk
