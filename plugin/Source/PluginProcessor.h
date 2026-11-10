#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include "sk_engine.h"
#include "RadioNet.h"
#include "Updater.h"

// One edit for the audio thread. The web UI sends the same ops in the browser and in the plugin.
struct EngineOp
{
    enum Type : int { Param, Trig, Lock, ClearLocks, Ratchet, Len, Speed, Mute, Global, Play, Audition, Reset, Panic, Scale, QTrack, Spec,
                      Rec, RamStart, RamClear, Prism, Trigless, Halo, Break, BreakClear, BreakBars, Fxm,
                      Release, PatCur, PatAll, PatEdit, PatClear, PatQueue, PatChain, PatChainLen, PatChg, Ribbon, Chim, Cube, CubeClear, West, RamCopy, RamSwap, Touch, Radio };
    int type = Param;
    int t = 0, s = 0, p = 0;
    float v = 0.0f;
};

class ImpaktProcessor : public juce::AudioProcessor
{
public:
    ImpaktProcessor();
    ~ImpaktProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "IMPAKT"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Kit"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- called from the editor (message thread)
    void pushOpsFromUi (const juce::var& payload);
    void setStateFromUi (const juce::String& json);
    juce::String getStateJson() const;
    int getStateVersion() const noexcept { return stateVersion.load(); }

    // ---- RAM machine, for the editor: each buffer (length, rate, version, state, progress, peaks counter) and its peaks
    void getRamInfo (float* out) const        { engine->ramInfo (out); }
    const float* getRamPeaks (int t) const    { return engine->ram[juce::jlimit (0, sk::NUM_TRACKS - 1, t)].pk; }
    // a track's parameter as the engine holds it (the tests: a project's conversion)
    float getTrackParam (int t, int p) const  { return engine->tr[juce::jlimit (0, sk::NUM_TRACKS - 1, t)].p[juce::jlimit (0, sk::NUM_PARAMS - 1, p)]; }
    // ... a global (v68: the tests)
    float getGlobal (int g) const             { return engine->g[juce::jlimit (0, sk::NUM_GLOBALS - 1, g)]; }
    // ... and a step's lock (-1: none)
    float getTrackLock (int t, int s, int p) const { return engine->tr[juce::jlimit (0, sk::NUM_TRACKS - 1, t)].lock[juce::jlimit (0, sk::NUM_STEPS - 1, s)][juce::jlimit (0, sk::NUM_PARAMS - 1, p)]; }

    // ---- BREAK's slots (message thread): a loop the page decoded (mono, 32 kHz, 16-bit, cut to its bars), its name
    void loadBreakSlot (int slot, int bars, const juce::String& name, const juce::MemoryBlock& samples);
    juce::String getBreakName (int slot) const { const juce::ScopedLock sl (stateLock); return brkNames[juce::jlimit (0, sk::BRK_SLOTS - 1, slot)]; }
    // CUBE (v74): a bank the page read from a WAV (64 waves of 256 samples, 16-bit), its name
    void loadCubeBank (int bank, const juce::String& name, const juce::MemoryBlock& samples);
    int cubeReady (int bank) const { return engine->cube.ready[juce::jlimit (0, sk::CUBE_BANKS - 1, bank)]; }   // 0 none, 1 being made, 2 ready
    juce::String getCubeName (int bank) const { const juce::ScopedLock sl (stateLock); return cubeNames[juce::jlimit (0, sk::CUBE_BANKS - 1, bank)]; }
    // RADIO (v87): the four receivers on the network (their sources, their states, the page's questions)
    RadioNet& radioNet() { return *radio; }
    // updates (v87): one updater for the process, shared by every instance
    Updater& updates() { return *updater; }
    // the page's RADIO messages: a receiver tuned (op "set"), a search of the stations' directory, the receivers' list
    void radioFromUi (const juce::var& v);

    // ---- read by the editor for the playhead display
    struct UiPos
    {
        std::atomic<int> steps[sk::NUM_TRACKS];
        std::atomic<int> hits[sk::NUM_TRACKS];
        std::atomic<int> running { 0 }, hostRunning { 0 };
        std::atomic<float> bpm { 120.0f };
        std::atomic<float> pat[sk::PAT_INFO];     // the patterns: playing, next, chain length / position, serial, CHG, position, to the change
        std::atomic<int> qmask { 0 };              // the quantiser's notes now (its scale, or the chord of the track it follows)
        UiPos() { for (auto& p : pat) p.store (0.0f); }
    } pos;

    // ---- master meters and MIX page data (CALM slots + unmasking gains), same as the web worklet sends.
    // The audio thread accumulates peaks; the editor takes them (and resets) at 30 Hz.
    static constexpr int mixInfoSize = 2 * sk::CALM_K + sk::NUM_TRACKS * sk::UM_BANDS;
    struct UiMeters
    {
        std::atomic<float> gr { 0.0f }, out { 0.0f };
        std::atomic<float> mix[mixInfoSize];
        std::atomic<int> mixValid { 0 };
        std::atomic<float> spec[sk::Engine::SPEC_BANDS];          // spectrum of the track the UI edits (TRACK page)
        std::atomic<int> specOn { 0 };
        std::atomic<float> fx[7];                                  // GRAIN's LFO (phase, value), OTT's gain per band, GRAIN on, OTT on
        std::atomic<float> glitch[22];                             // GLITCH: last 16 divisions, progress, detail, bandwidth, on, tape, cents
        std::atomic<int> prismT { -1 };                            // PRISM shown by the page: the track, its oscillators (Hz, amplitude)
        std::atomic<float> prism[2 * sk::PR_K];
        std::atomic<int> haloT { -1 };                             // HALO shown by the page: the track, its chord and voices
        std::atomic<float> halo[sk::HALO_INFO];
        std::atomic<int> brkT { -1 };                              // BREAK shown by the page: its break, the slice playing
        std::atomic<float> brk[sk::BRK_INFO];
        std::atomic<int> fxmT { -1 };                              // an FX track shown by the page: its machine, source, gate, gesture, levels, readouts
        std::atomic<float> fxm[sk::FX_INFO];                       // (the levels, 4 and 5, and FUZZ's SUB, 12, are the peaks since the page last read them)
        std::atomic<int> cubeT { -1 };                             // CUBE shown by the page: where A and B are, the banks, the wave
        std::atomic<float> cube[sk::CUBE_INFO];
        std::atomic<int> cubeVer { 0 };                            // CUBE's banks: a change (loaded, made, cleared)
        std::atomic<int> westT { -1 };                             // WEST shown by the page: its light, step, pulses, folder, gate
        std::atomic<float> west[sk::WS_INFO];
        std::atomic<int> chimT { -1 };                             // CHIMERA shown by the page: the bank, the track's last hit, its neighbours
        std::atomic<float> chim[sk::CHIM_INFO];
        std::atomic<float> mod[8 * sk::NUM_TRACKS];                // LFO 1 / 2 (phase, value), EF 1 / 2 (value, input), per track
        // BREAK's slots (state, tempo, bars, seconds) and their waveforms, copied when they change (brkVer moves last)
        std::atomic<float> brkSlots[sk::BRK_SLOTS * sk::BRK_SLOT_INFO];
        std::atomic<float> brkPeaks[sk::BRK_SLOTS * sk::BRK_NPEAK];
        std::atomic<int> brkVer { 0 };
        // RADIO (v87): the track the page shows (its receiver, the loop, the history's peaks), the four receivers
        std::atomic<int> radT { -1 };
        std::atomic<float> rad[sk::RADIO_INFO];
        std::atomic<float> radRx[sk::RADIO_RX * sk::RADIO_RX_INFO];
        std::atomic<int> radUse { 0 };
        UiMeters() { for (auto& m : mix) m.store (0.0f); for (auto& m : spec) m.store (0.0f); for (auto& m : fx) m.store (0.0f); for (auto& m : prism) m.store (0.0f); for (auto& m : halo) m.store (0.0f); for (auto& m : brk) m.store (0.0f); for (auto& m : fxm) m.store (0.0f); for (auto& m : chim) m.store (-1.0f); for (auto& m : cube) m.store (0.0f); for (auto& m : west) m.store (0.0f); for (auto& m : brkSlots) m.store (0.0f); for (auto& m : brkPeaks) m.store (0.0f); for (auto& m : rad) m.store (0.0f); for (auto& m : radRx) m.store (0.0f); }
    } meters;

    // CHIMERA's bank (v66): read and decoded once per process, in the background (every instance shares it), from the
    // plugin's bundle (Contents/Resources/bank), beside its binary, the user's IMPAKT/bank folder, or $IMPAKT_BANK
    static void loadBank();
    static juce::File bankFolder();

private:
    void pushOp (const EngineOp& op);
    void pushStateOps (const juce::var& state);
    void applyOp (const EngineOp& op);

    std::unique_ptr<sk::Engine> engine;
    std::unique_ptr<RadioNet> radio;            // (after the engine: its threads end before it goes)
    juce::SharedResourcePointer<Updater> updater;
    float globalDefaults[sk::NUM_GLOBALS] {};
    float paramDefaults[sk::NUM_PARAMS] {};     // for parameters a project does not have yet
    std::vector<float> monoBuf;
    std::vector<float> inBufL, inBufR;          // the input bus, copied before the engine writes its output

    static constexpr int fifoSize = 262144;     // (a project of many patterns is a lot of ops)
    juce::AbstractFifo fifo { fifoSize };
    std::vector<EngineOp> fifoData;
    juce::SpinLock producerLock;

    mutable juce::CriticalSection stateLock;
    juce::String stateJson;
    juce::String brkNames[sk::BRK_SLOTS];
    juce::String cubeNames[sk::CUBE_BANKS];
    int brkSeen = -1;                           // (audio thread) the slots' version last copied to the meters
    std::atomic<int> stateVersion { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImpaktProcessor)
};
