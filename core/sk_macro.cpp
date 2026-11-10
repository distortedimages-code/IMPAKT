// IMPAKT — MACRO machine: drives the Plaits 1.2 engines (mi/plaits, MIT, © Emilie Gillet) the way the
// module does with its TRIGGER input patched and LEVEL unpatched: internal decay envelope, low-pass gate,
// TIMBRE attenuverter on the envelope. Differences from plaits/dsp/voice.cc: float output (no 16-bit
// stage, same hard clip at full scale), no 1 ms trigger delay (IMPAKT's trigs are sample-accurate), the
// model is chosen directly, the accent follows the velocity.
#include "sk_macro.h"

#include <new>

#include "stmlib/stmlib.h"
#include "stmlib/dsp/limiter.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/buffer_allocator.h"

#include "plaits/dsp/dsp.h"
#include "plaits/dsp/envelope.h"
#include "plaits/dsp/fx/low_pass_gate.h"
#include "plaits/resources.h"

#include "plaits/dsp/engine/additive_engine.h"
#include "plaits/dsp/engine/bass_drum_engine.h"
#include "plaits/dsp/engine/chord_engine.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/engine/fm_engine.h"
#include "plaits/dsp/engine/grain_engine.h"
#include "plaits/dsp/engine/hi_hat_engine.h"
#include "plaits/dsp/engine/modal_engine.h"
#include "plaits/dsp/engine/noise_engine.h"
#include "plaits/dsp/engine/particle_engine.h"
#include "plaits/dsp/engine/snare_drum_engine.h"
#include "plaits/dsp/engine/speech_engine.h"
#include "plaits/dsp/engine/string_engine.h"
#include "plaits/dsp/engine/swarm_engine.h"
#include "plaits/dsp/engine/virtual_analog_engine.h"
#include "plaits/dsp/engine/waveshaping_engine.h"
#include "plaits/dsp/engine/wavetable_engine.h"
#include "plaits/dsp/engine2/chiptune_engine.h"
#include "plaits/dsp/engine2/phase_distortion_engine.h"
#include "plaits/dsp/engine2/six_op_engine.h"
#include "plaits/dsp/engine2/string_machine_engine.h"
#include "plaits/dsp/engine2/virtual_analog_vcf_engine.h"
#include "plaits/dsp/engine2/wave_terrain_engine.h"

namespace sk {

namespace {

using namespace plaits;

const int kEngines = 24;
const size_t kRam = 16384;     // the module's shared buffer (the engines take turns in it)

// output stage of one channel: limiter for the engines with a negative gain, then the low-pass gate
struct Post
{
    stmlib::Limiter limiter;
    LowPassGate lpg;
    void init() { lpg.Init(); limiter.Init(); }
    void reset() { limiter.Init(); }
    void process (float gain, bool bypass, float g, float f, float hf, float* io, size_t n)
    {
        if (gain < 0.0f) limiter.Process (-gain, io, n);
        const float post = gain < 0.0f ? 1.0f : gain;
        if (! bypass) lpg.Process (post * g, f, hf, io, n);
        else for (size_t i = 0; i < n; ++i) io[i] *= post;
        for (size_t i = 0; i < n; ++i) io[i] = io[i] > 1.0f ? 1.0f : (io[i] < -1.0f ? -1.0f : io[i]);
    }
};

class MacroVoice
{
public:
    void init (stmlib::BufferAllocator* allocator)
    {
        engines_.Init();
        engines_.RegisterInstance (&virtual_analog_vcf_engine_, false, 1.0f, 1.0f);
        engines_.RegisterInstance (&phase_distortion_engine_, false, 0.7f, 0.7f);
        engines_.RegisterInstance (&six_op_engine_, true, 1.0f, 1.0f);
        engines_.RegisterInstance (&six_op_engine_, true, 1.0f, 1.0f);
        engines_.RegisterInstance (&six_op_engine_, true, 1.0f, 1.0f);
        engines_.RegisterInstance (&wave_terrain_engine_, false, 0.7f, 0.7f);
        engines_.RegisterInstance (&string_machine_engine_, false, 0.8f, 0.8f);
        engines_.RegisterInstance (&chiptune_engine_, false, 0.5f, 0.5f);

        engines_.RegisterInstance (&virtual_analog_engine_, false, 0.8f, 0.8f);
        engines_.RegisterInstance (&waveshaping_engine_, false, 0.7f, 0.6f);
        engines_.RegisterInstance (&fm_engine_, false, 0.6f, 0.6f);
        engines_.RegisterInstance (&grain_engine_, false, 0.7f, 0.6f);
        engines_.RegisterInstance (&additive_engine_, false, 0.8f, 0.8f);
        engines_.RegisterInstance (&wavetable_engine_, false, 0.6f, 0.6f);
        engines_.RegisterInstance (&chord_engine_, false, 0.8f, 0.8f);
        engines_.RegisterInstance (&speech_engine_, false, -0.7f, 0.8f);

        engines_.RegisterInstance (&swarm_engine_, false, -3.0f, 1.0f);
        engines_.RegisterInstance (&noise_engine_, false, -1.0f, -1.0f);
        engines_.RegisterInstance (&particle_engine_, false, -2.0f, 1.0f);
        engines_.RegisterInstance (&string_engine_, true, -1.0f, 0.8f);
        engines_.RegisterInstance (&modal_engine_, true, -1.0f, 0.8f);
        engines_.RegisterInstance (&bass_drum_engine_, true, 0.8f, 0.8f);
        engines_.RegisterInstance (&snare_drum_engine_, true, 0.8f, 0.8f);
        engines_.RegisterInstance (&hi_hat_engine_, true, 0.8f, 0.8f);
        for (int i = 0; i < engines_.size(); ++i)
        {
            allocator->Free();          // all the engines share the same RAM, as on the module
            engines_.get (i)->Init (allocator);
        }
        previous_engine_ = -1;
        trigger_state_ = false;
        decay_envelope_.Init();
        lpg_envelope_.Init();
        out_post_.init(); aux_post_.init();
    }

    // one block of kBlockSize samples
    void render (int engine_index, const MacroControls& c, bool rising, bool high, float* out, float* aux)
    {
        trigger_state_ = high || rising;
        if (rising) { lpg_envelope_.Trigger(); decay_envelope_.Trigger(); }

        Engine* e = engines_.get (engine_index);
        if (engine_index != previous_engine_)
        {
            e->LoadUserData (engine_index >= 2 && engine_index <= 4 ? fm_patches_table[engine_index - 2] : nullptr);
            e->Reset();
            out_post_.reset();
            previous_engine_ = engine_index;
        }
        EngineParameters p;
        p.trigger = (rising ? TRIGGER_RISING_EDGE : TRIGGER_LOW) | (trigger_state_ ? TRIGGER_HIGH : TRIGGER_LOW);

        const float short_decay = (200.0f * kBlockSize) / kSampleRate * stmlib::SemitonesToRatio (-96.0f * c.decay);
        decay_envelope_.Process (short_decay * 2.0f);
        p.accent = c.accent;

        p.harmonics = c.harmonics;
        CONSTRAIN (p.harmonics, 0.0f, 1.0f);
        float env_amp = 1.0f, env_amp_timbre = 1.0f;
        if (engine_index == 15)
        {
            env_amp = 2.0f - p.harmonics * 6.0f;
            CONSTRAIN (env_amp, 0.0f, 1.0f);
            speech_engine_.set_prosody_amount (0.0f);
            speech_engine_.set_speed (0.0f);
        }
        else if (engine_index == 7)
        {
            env_amp_timbre = 0.0f;
            chiptune_engine_.set_envelope_shape (c.timbreEnv);
        }
        const float dv = decay_envelope_.value();
        p.note = modulate (c.note, 0.0f, env_amp * dv * dv * 48.0f, -119.0f, 120.0f);
        p.timbre = modulate (c.timbre, c.timbreEnv, env_amp_timbre * dv, 0.0f, 1.0f);
        p.morph = modulate (c.morph, 0.0f, env_amp * dv, 0.0f, 1.0f);

        const PostProcessingSettings& pp = e->post_processing_settings;
        bool already_enveloped = pp.already_enveloped;
        e->Render (p, out, aux, kBlockSize, &already_enveloped);

        const bool bypass = already_enveloped;
        if (! bypass)
        {
            const float hf = c.colour;
            const float decay_tail = (20.0f * kBlockSize) / kSampleRate
                * stmlib::SemitonesToRatio (-72.0f * c.decay + 12.0f * hf) - short_decay;
            const float attack = NoteToFrequency (p.note) * float (kBlockSize) * 2.0f;
            if (c.hold) lpg_envelope_.ProcessLP (trigger_state_ ? 1.0f : 0.0f, short_decay, decay_tail, hf);   // (LEVEL patched)
            else lpg_envelope_.ProcessPing (attack, short_decay, decay_tail, hf);
        }
        else lpg_envelope_.Init();
        out_post_.process (pp.out_gain, bypass, lpg_envelope_.gain(), lpg_envelope_.frequency(), lpg_envelope_.hf_bleed(), out, kBlockSize);
        aux_post_.process (pp.aux_gain, bypass, lpg_envelope_.gain(), lpg_envelope_.frequency(), lpg_envelope_.hf_bleed(), aux, kBlockSize);
    }

private:
    // Plaits' attenuverter law with the internal envelope as the modulation source
    static float modulate (float base, float amount, float envelope, float lo, float hi)
    {
        const float m = fabsf (amount) - 0.05f;
        amount *= m > 0.05f ? m : 0.05f;
        amount *= 1.05f;
        float v = base + amount * envelope;
        CONSTRAIN (v, lo, hi);
        return v;
    }

    VirtualAnalogEngine virtual_analog_engine_;
    WaveshapingEngine waveshaping_engine_;
    FMEngine fm_engine_;
    GrainEngine grain_engine_;
    AdditiveEngine additive_engine_;
    WavetableEngine wavetable_engine_;
    ChordEngine chord_engine_;
    SpeechEngine speech_engine_;
    SwarmEngine swarm_engine_;
    NoiseEngine noise_engine_;
    ParticleEngine particle_engine_;
    StringEngine string_engine_;
    ModalEngine modal_engine_;
    BassDrumEngine bass_drum_engine_;
    SnareDrumEngine snare_drum_engine_;
    HiHatEngine hi_hat_engine_;
    VirtualAnalogVCFEngine virtual_analog_vcf_engine_;
    PhaseDistortionEngine phase_distortion_engine_;
    SixOpEngine six_op_engine_;
    WaveTerrainEngine wave_terrain_engine_;
    StringMachineEngine string_machine_engine_;
    ChiptuneEngine chiptune_engine_;

    EngineRegistry<kEngines> engines_;
    int previous_engine_;
    bool trigger_state_;
    DecayEnvelope decay_envelope_;
    LPGEnvelope lpg_envelope_;
    Post out_post_, aux_post_;
};

// one unit: the voice, its RAM, and a small FIFO that resamples the 48 kHz stream to the host rate
struct MacroUnit
{
    MacroVoice voice;
    alignas (16) char ram[kRam];
    enum { FIFO = 160 };
    float fifo[FIFO];
    int   fifoN;
    double pos, step;
    int   gateLeft;       // 48 kHz samples left with the gate high
    int   pendingTrig;
    float blockOut[kMaxBlockSize], blockAux[kMaxBlockSize];
};

static_assert (sizeof (MacroUnit) <= MACRO_BYTES, "MACRO_BYTES too small");

inline MacroUnit* U (void* mem) { return static_cast<MacroUnit*> (mem); }

// MACRO model order (the 16 classic models, then the 8 of v1.2) -> Plaits engine index
inline int engineOf (int model)
{
    model = model < 0 ? 0 : (model >= kEngines ? kEngines - 1 : model);
    return model < 16 ? model + 8 : model - 16;
}

void renderBlock (MacroUnit& u, const MacroControls& c)
{
    const bool rising = u.pendingTrig != 0;
    u.pendingTrig = 0;
    const bool high = u.gateLeft > 0;
    u.gateLeft -= (int) kBlockSize;
    if (u.gateLeft < 0) u.gateLeft = 0;
    u.voice.render (engineOf (c.model), c, rising, high, u.blockOut, u.blockAux);
    const float a = c.aux < 0.0f ? 0.0f : (c.aux > 1.0f ? 1.0f : c.aux);
    for (size_t i = 0; i < kBlockSize; ++i)
        u.fifo[u.fifoN++] = u.blockOut[i] + (u.blockAux[i] - u.blockOut[i]) * a;
}

} // namespace

void macroInit (void* mem, float hostSr)
{
    MacroUnit* u = new (mem) MacroUnit();
    stmlib::BufferAllocator allocator (u->ram, kRam);
    u->voice.init (&allocator);
    for (int i = 0; i < MacroUnit::FIFO; ++i) u->fifo[i] = 0.0f;
    u->fifoN = 4;            // a few samples of silence before the stream (the interpolator looks back one)
    u->pos = 1.0;
    u->gateLeft = 0; u->pendingTrig = 0;
    macroSetRate (mem, hostSr);
}

void macroSetRate (void* mem, float hostSr)
{
    U (mem)->step = (double) plaits::kSampleRate / (double) (hostSr > 1000.0f ? hostSr : 48000.0f);
}

void macroTrigger (void* mem, float gateSec)
{
    MacroUnit& u = *U (mem);
    u.pendingTrig = 1;
    const int g = (int) ((gateSec < 3600.0f ? gateSec : 3600.0f) * plaits::kSampleRate);
    u.gateLeft = g > (int) (2 * plaits::kBlockSize) ? g : (int) (2 * plaits::kBlockSize);
}

void macroRelease (void* mem) { U (mem)->gateLeft = 0; }

float macroRender (void* mem, const MacroControls& c, float* out, int n)
{
    MacroUnit& u = *U (mem);
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        int ip = (int) u.pos;
        while (ip + 2 >= u.fifoN)
        {
            if (u.fifoN + (int) plaits::kBlockSize > MacroUnit::FIFO)    // make room: keep one sample behind the read point
            {
                const int drop = ip - 1;
                for (int k = drop; k < u.fifoN; ++k) u.fifo[k - drop] = u.fifo[k];
                u.fifoN -= drop; u.pos -= (double) drop; ip = 1;
            }
            renderBlock (u, c);
        }
        const float fr = (float) (u.pos - (double) ip);
        const float xm = u.fifo[ip - 1], x0 = u.fifo[ip], x1 = u.fifo[ip + 1], x2 = u.fifo[ip + 2];
        // 4-point Hermite; at 48 kHz the position stays on whole samples and this returns x0 exactly
        const float c1 = 0.5f * (x1 - xm);
        const float c2 = xm - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm) + 1.5f * (x0 - x1);
        const float y = ((c3 * fr + c2) * fr + c1) * fr + x0;
        out[i] = y;
        const float a = y < 0.0f ? -y : y;
        if (a > peak) peak = a;
        u.pos += u.step;
    }
    const int drop = (int) u.pos - 1;
    if (drop > 0)
    {
        for (int k = drop; k < u.fifoN; ++k) u.fifo[k - drop] = u.fifo[k];
        u.fifoN -= drop;
        u.pos -= (double) drop;
    }
    return peak;
}

} // namespace sk
