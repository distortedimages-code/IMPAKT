#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <thread>
#include <mutex>
#include <cstdlib>

// CHIMERA's bank: one copy for the VST3, the AU and the app (v87.1: the installers put it in the system's shared data
// folder), or one inside the plugin itself (a build that embeds it), or the folder $IMPAKT_BANK names
juce::File ImpaktProcessor::bankFolder()
{
    juce::Array<juce::File> dirs;
    if (const char* env = std::getenv ("IMPAKT_BANK")) dirs.add (juce::File (juce::String (env)));
    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    dirs.add (exe.getParentDirectory().getParentDirectory().getChildFile ("Resources").getChildFile ("bank"));   // (inside a bundle)
    dirs.add (exe.getParentDirectory().getChildFile ("bank"));                                                   // (beside a binary)
    const auto common = juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory);
    const auto user   = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC || JUCE_IOS
    // /Library/Application Support/IMPAKT/bank (the installer), ~/Library/Application Support/IMPAKT/bank
    dirs.add (common.getChildFile ("Application Support/IMPAKT/bank"));
    dirs.add (user.getChildFile ("Application Support/IMPAKT/bank"));
   #elif JUCE_WINDOWS
    // C:\ProgramData\IMPAKT\bank (the installer), %APPDATA%\IMPAKT\bank
    dirs.add (common.getChildFile ("IMPAKT").getChildFile ("bank"));
   #else
    // ~/.local/share/IMPAKT/bank (install.sh), /usr/local/share, /usr/share
    const char* xdg = std::getenv ("XDG_DATA_HOME");
    dirs.add ((xdg != nullptr && *xdg ? juce::File (juce::String (xdg)) : juce::File ("~/.local/share")).getChildFile ("IMPAKT/bank"));
    dirs.add (juce::File ("/usr/local/share/IMPAKT/bank"));
    dirs.add (juce::File ("/usr/share/IMPAKT/bank"));
   #endif
    dirs.add (user.getChildFile ("IMPAKT").getChildFile ("bank"));
    for (const auto& d : dirs) if (d.getChildFile ("chimera.idx").existsAsFile()) return d;
    return {};
}

void ImpaktProcessor::loadBank()
{
    static std::once_flag once;
    std::call_once (once, [] {
        std::thread ([] {
            const auto dir = bankFolder();
            if (! dir.exists()) return;
            juce::MemoryBlock idx;
            if (! dir.getChildFile ("chimera.idx").loadFileAsData (idx) || ! sk::bankIndex ((const sk::u8*) idx.getData(), (int) idx.getSize())) return;
            // (the files stay with the process: the decoder reads from them as it goes)
            static std::vector<juce::MemoryBlock> files;
            for (int k = 0;; ++k)
            {
                const auto f = dir.getChildFile ("chimera." + juce::String (k) + ".bin");
                if (! f.existsAsFile()) break;
                files.emplace_back(); f.loadFileAsData (files.back());
            }
            for (int i = 0; i < sk::BANK.n; ++i)
            {
                const sk::BankEntry& e = sk::BANK.e[i];
                if (e.file >= 0 && e.file < (int) files.size() && e.foff + e.coded <= (int) files[(size_t) e.file].getSize())
                    sk::bankDecodeNow (i, (const sk::u8*) files[(size_t) e.file].getData() + e.foff, e.coded);
                else { sk::bankFeed (-1, nullptr, 0); }
            }
            if (sk::BANK.done + sk::BANK.failed >= sk::BANK.n) sk::BANK.state = 2;
        }).detach();
    });
}

ImpaktProcessor::ImpaktProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    loadBank();
    engine = std::make_unique<sk::Engine>();
    engine->init (48000.0f);
    radio = std::make_unique<RadioNet> (*engine);
    for (int i = 0; i < sk::NUM_GLOBALS; ++i) globalDefaults[i] = engine->g[i];
    for (int i = 0; i < sk::NUM_PARAMS; ++i) paramDefaults[i] = engine->tr[0].p[i];
    fifoData.resize ((size_t) fifoSize);
    for (int t = 0; t < sk::NUM_TRACKS; ++t) { pos.steps[t] = -1; pos.hits[t] = 0; }
}

void ImpaktProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine->setSampleRate ((float) sampleRate);
    setLatencySamples (sk::LATENCY);   // the BOX convolution delays every track by 64 samples
    monoBuf.assign ((size_t) juce::jmax (samplesPerBlock, 4096), 0.0f);
    inBufL.assign ((size_t) juce::jmax (samplesPerBlock, 4096), 0.0f);
    inBufR.assign ((size_t) juce::jmax (samplesPerBlock, 4096), 0.0f);
}

// stereo or mono out; the input (the RAM machine's IN source) stereo, mono or off
bool ImpaktProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && (in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

// ----------------------------------------------------------------------------- ops
void ImpaktProcessor::pushOp (const EngineOp& op)
{
    const juce::SpinLock::ScopedLockType sl (producerLock);
    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0) fifoData[(size_t) scope.startIndex1] = op;
    else if (scope.blockSize2 > 0) fifoData[(size_t) scope.startIndex2] = op;
}

void ImpaktProcessor::applyOp (const EngineOp& o)
{
    auto& e = *engine;
    switch (o.type)
    {
        case EngineOp::Param:      e.setParam (o.t, o.p, o.v); break;
        case EngineOp::Trig:       e.setTrig (o.t, o.s, o.v > 0.5f ? 1 : 0); break;
        case EngineOp::Lock:       e.setLock (o.t, o.s, o.p, o.v); break;
        case EngineOp::ClearLocks: e.clearLocks (o.t, o.s); break;
        case EngineOp::Ratchet:    e.setRatchet (o.t, o.s, (int) o.v); break;
        case EngineOp::Len:        e.setLen (o.t, (int) o.v); break;
        case EngineOp::Speed:      e.setSpeed (o.t, (int) o.v); break;
        case EngineOp::Mute:       e.setMute (o.t, o.v > 0.5f ? 1 : 0); break;
        case EngineOp::Global:     e.setGlobal (o.p, o.v); break;
        case EngineOp::Play:       e.setPlay (o.v > 0.5f ? 1 : 0); break;
        case EngineOp::Audition:   e.audition (o.t, o.v, (float) o.p); break;
        case EngineOp::Reset:      e.resetPattern(); break;
        case EngineOp::Panic:      e.panic(); break;
        case EngineOp::Scale:      e.setScale ((sk::u32) o.p); break;
        case EngineOp::QTrack:     e.setQuant (o.t, o.v > 0.5f ? 1 : 0); break;
        case EngineOp::Spec:       e.setSpecTrack (o.t); meters.specOn.store (o.t >= 0 ? 1 : 0, std::memory_order_relaxed); break;
        case EngineOp::Rec:        e.setRec (o.t, o.s, o.v > 0.5f ? 1 : 0); break;
        case EngineOp::Trigless:   e.setTrigless (o.t, o.s, o.v > 0.5f ? 1 : 0); break;
        case EngineOp::RamStart:   e.ramStart (o.t, -1); break;
        case EngineOp::RamClear:   e.ramClear (o.t); break;
        case EngineOp::RamCopy:    e.ramCopy (o.t, o.s); break;
        case EngineOp::RamSwap:    e.ramSwap (o.t, o.s); break;
        case EngineOp::Prism:      meters.prismT.store (o.t, std::memory_order_relaxed); break;
        case EngineOp::Halo:       meters.haloT.store (o.t, std::memory_order_relaxed); break;
        case EngineOp::Break:      meters.brkT.store (o.t, std::memory_order_relaxed); break;
        case EngineOp::Fxm:        meters.fxmT.store (o.t, std::memory_order_relaxed); break;
        case EngineOp::Chim:       meters.chimT.store (o.t, std::memory_order_relaxed); break;
        case EngineOp::BreakClear: e.breakClear (o.t); break;
        case EngineOp::BreakBars:  e.breakBars (o.t, o.p); break;
        case EngineOp::Cube:       meters.cubeT.store (o.t, std::memory_order_relaxed); break;
        case EngineOp::CubeClear:  e.cubeClear (o.t); break;
        case EngineOp::West:       meters.westT.store (o.t, std::memory_order_relaxed); break;
        case EngineOp::Release:    e.release (o.t, (float) o.p); break;
        case EngineOp::PatCur:     e.patCur (o.t); break;
        case EngineOp::PatAll:     e.patAll(); break;
        case EngineOp::PatEdit:    e.patEdit (o.t); break;
        case EngineOp::PatClear:   e.patClear (o.t); break;
        case EngineOp::PatQueue:   e.patQueue (o.t, o.p); break;
        case EngineOp::PatChain:   e.patChain (o.s, o.t); break;
        case EngineOp::PatChainLen: e.patChainLen (o.t); break;
        case EngineOp::PatChg:     e.patChg (o.t); break;
        case EngineOp::Ribbon:     e.ribbon (o.t, o.v, o.p, (float) o.s * 1e-4f); break;
        case EngineOp::Touch:      e.mantraTouch (o.t, o.p, o.v); break;
        case EngineOp::Radio:      meters.radT.store (o.t, std::memory_order_relaxed); break;
        default: break;
    }
}

// JS ops look like ["p", t, p, v], ["trig", t, s, on], ["lock", t, s, p, v] ...
void ImpaktProcessor::pushOpsFromUi (const juce::var& payload)
{
    const auto* ops = payload.getProperty ("ops", juce::var()).getArray();
    if (ops == nullptr) return;
    for (const auto& item : *ops)
    {
        const auto* a = item.getArray();
        if (a == nullptr || a->isEmpty()) continue;
        const auto name = (*a)[0].toString();
        auto num = [a] (int i) { return i < a->size() ? (float) (double) (*a)[i] : 0.0f; };
        auto inum = [&num] (int i) { return (int) num (i); };
        EngineOp op;
        if      (name == "p")     { op.type = EngineOp::Param; op.t = inum (1); op.p = inum (2); op.v = num (3); }
        else if (name == "trig")  { op.type = EngineOp::Trig; op.t = inum (1); op.s = inum (2); op.v = num (3); }
        else if (name == "lock")  { op.type = EngineOp::Lock; op.t = inum (1); op.s = inum (2); op.p = inum (3); op.v = num (4); }
        else if (name == "clrl")  { op.type = EngineOp::ClearLocks; op.t = inum (1); op.s = inum (2); }
        else if (name == "rat")   { op.type = EngineOp::Ratchet; op.t = inum (1); op.s = inum (2); op.v = num (3); }
        else if (name == "len")   { op.type = EngineOp::Len; op.t = inum (1); op.v = num (2); }
        else if (name == "spd")   { op.type = EngineOp::Speed; op.t = inum (1); op.v = num (2); }
        else if (name == "mute")  { op.type = EngineOp::Mute; op.t = inum (1); op.v = num (2); }
        else if (name == "g")     { op.type = EngineOp::Global; op.p = inum (1); op.v = num (2); }
        else if (name == "play")  { op.type = EngineOp::Play; op.v = num (1); }
        else if (name == "aud")   { op.type = EngineOp::Audition; op.t = inum (1); op.v = num (2); op.p = inum (3); }   // p: the note (KEY mode)
        else if (name == "reset") { op.type = EngineOp::Reset; }
        else if (name == "panic") { op.type = EngineOp::Panic; }
        else if (name == "scale") { op.type = EngineOp::Scale; op.p = inum (1); }
        else if (name == "qtrk")  { op.type = EngineOp::QTrack; op.t = inum (1); op.v = num (2); }
        else if (name == "spec")  { op.type = EngineOp::Spec; op.t = inum (1); }
        else if (name == "rec")   { op.type = EngineOp::Rec; op.t = inum (1); op.s = inum (2); op.v = num (3); }
        else if (name == "tlk")   { op.type = EngineOp::Trigless; op.t = inum (1); op.s = inum (2); op.v = num (3); }
        else if (name == "rnow")  { op.type = EngineOp::RamStart; op.t = inum (1); }
        else if (name == "rclr")  { op.type = EngineOp::RamClear; op.t = inum (1); }
        else if (name == "rcpy")  { op.type = EngineOp::RamCopy; op.t = inum (1); op.s = inum (2); }
        else if (name == "rswp")  { op.type = EngineOp::RamSwap; op.t = inum (1); op.s = inum (2); }
        else if (name == "prsm")  { op.type = EngineOp::Prism; op.t = inum (1); }
        else if (name == "halo")  { op.type = EngineOp::Halo; op.t = inum (1); }
        else if (name == "brk")   { op.type = EngineOp::Break; op.t = inum (1); }
        else if (name == "fxm")   { op.type = EngineOp::Fxm; op.t = inum (1); }
        else if (name == "chm")   { op.type = EngineOp::Chim; op.t = inum (1); }
        else if (name == "bclr")  { op.type = EngineOp::BreakClear; op.t = inum (1); if (op.t >= 0 && op.t < sk::BRK_SLOTS) { const juce::ScopedLock sl (stateLock); brkNames[op.t] = {}; } }
        else if (name == "bbar")  { op.type = EngineOp::BreakBars; op.t = inum (1); op.p = inum (2); }
        else if (name == "cube")  { op.type = EngineOp::Cube; op.t = inum (1); }
        else if (name == "west")  { op.type = EngineOp::West; op.t = inum (1); }
        else if (name == "cclr")  { op.type = EngineOp::CubeClear; op.t = inum (1); if (op.t >= 0 && op.t < sk::CUBE_BANKS) { const juce::ScopedLock sl (stateLock); cubeNames[op.t] = {}; } }
        else if (name == "rel")   { op.type = EngineOp::Release; op.t = inum (1); op.p = inum (2); }   // p: the note (KEY mode)
        else if (name == "pcur")  { op.type = EngineOp::PatCur; op.t = inum (1); }
        else if (name == "pall")  { op.type = EngineOp::PatAll; }
        else if (name == "epat")  { op.type = EngineOp::PatEdit; op.t = inum (1); }
        else if (name == "pclr")  { op.type = EngineOp::PatClear; op.t = inum (1); }
        else if (name == "pq")    { op.type = EngineOp::PatQueue; op.t = inum (1); op.p = inum (2); }
        else if (name == "pch")   { op.type = EngineOp::PatChain; op.s = inum (1); op.t = inum (2); }
        else if (name == "pchn")  { op.type = EngineOp::PatChainLen; op.t = inum (1); }
        else if (name == "pchg")  { op.type = EngineOp::PatChg; op.t = inum (1); }
        else if (name == "rib")   { op.type = EngineOp::Ribbon; op.t = inum (1); op.v = num (2); op.p = inum (3); op.s = (int) (num (4) * 10000.0f + 0.5f); }
        else if (name == "radt")  { op.type = EngineOp::Radio; op.t = inum (1); }   // (v87: the RADIO track the page shows)
        else if (name == "pad")   { op.type = EngineOp::Touch; op.t = inum (1); op.p = inum (2); op.v = num (3); }   // (v84: MANTRA's pads: a voice, its pressure)   // CHOIR's ribbon: semitones, touched (2: on SNAP), its height
        else continue;
        pushOp (op);
    }
}

// RADIO (v87): the page tunes a receiver ({ op: "set", rx, src }), asks the stations' directory ({ op: "search", id, name,
// tag, cc, order, offset }) or the receivers' list ({ op: "receivers", id, fresh }); the answers go back with its position
void ImpaktProcessor::radioFromUi (const juce::var& v)
{
    const auto op = v.getProperty ("op", "").toString();
    if (op == "set") radio->setSource ((int) v.getProperty ("rx", -1), RadioNet::Source::fromVar (v.getProperty ("src", juce::var())));
    else if (op == "search") radio->search ((int) v.getProperty ("id", 0), v);
    else if (op == "receivers") radio->receivers ((int) v.getProperty ("id", 0), (bool) v.getProperty ("fresh", false));
}

// Mirrors stateOps() in the web UI (and its migrate() for projects saved by older versions:
// v1 had 12 machines, v2 had 18 machines and µLOOP sources morphing between 4 synths, v3 had 19
// machines on a 19-step P_MACH grid, a plate send and a separate spectral send; v6 had GRAIN as a 7th reverb; v7 had 27
// LFO and 34 DICE destinations and a one-sided PRISM WDTH; v8 had P_MACH on 32 slots and the four RUMBLE voices; v9 had
// 35 LFO and 42 DICE destinations, no TONE and no envelope followers; v14 had BREAK on its own breaks, with kits).
void ImpaktProcessor::pushStateOps (const juce::var& st)
{
    const auto* g = st.getProperty ("g", juce::var()).getArray();
    const auto* tracks = st.getProperty ("tracks", juce::var()).getArray();
    if (g == nullptr || tracks == nullptr || tracks->size() != sk::NUM_TRACKS) return;

    const int ver = (int) st.getProperty ("v", 4);
    const int oldMachines = ver == 1 ? 12 : (ver == 2 ? 18 : 19);
    auto idx = [] (double v, int n) { return juce::jlimit (0, n - 1, (int) (v * n)); };
    auto mid = [] (int i, int n) { return (float) ((i + 0.5) / n); };
    auto srcMap = [&] (double v) { return mid ((int) std::round (juce::jlimit (0.0, 1.0, v) * 3.0), sk::NUM_ULOOP_SRC); };
    // v4..v8: P_MACH on 32 slots (64 since v9). A project saved before the RUMBLE voices (no "rmb") had its snare on slot
    // 26 (then RUMBLE 3). The four RUMBLE voices (25..28) are gone: a track or a lock on one of them goes to the nearest
    // machine (HEX·BD, WT·DRM, HEX·SD, HH·MT), A..F at its defaults (as the UI's INIT has them), TUNE and DEC kept
    const bool preRmb = ver >= 4 && ver < 9 && ! st.hasProperty ("rmb");
    auto oldSlot = [&] (double v) { int m = juce::jmin (idx (v, 32), 30); if (preRmb && m == 26) m = 27; return m; };
    auto wasRmb = [&] (double v) { if (ver < 4 || ver >= 9) return false; const int m = oldSlot (v); return m >= 25 && m <= 28; };
    // the machine as the project numbered it (v20's numbers: 42 machines), then onto today's (v21: STRING, MODAL and the
    // three WT machines gone, MATTER and LATEX in; v22: those two gone, ALLOY in; a track on one of the machines gone goes
    // to ALLOY, A .. F and the side pages at its defaults)
    auto machine20 = [&] (double v) {
        if (ver < 4) return idx (v, oldMachines);
        if (ver >= 9) return juce::jlimit (0, (int) sk::NUM_MACHINES - 1, (int) (v * sk::MACH_SLOTS));
        static const int RMB_TO[4] = { 25, 28, 26, 4 };       // (v20: HEX·BD, WT·DRM, HEX·SD, HH·MT)
        const int m = oldSlot (v);
        return m >= 25 && m <= 28 ? RMB_TO[m - 25] : m;
    };
    auto machine21 = [&] (double v) { return juce::jlimit (0, 38, (int) (v * sk::MACH_SLOTS)); };
    auto machineIn = [&] (double v) {
        if (ver < 21) return sk::oldMachine22 (sk::oldMachine21 (machine20 (v)));
        return ver < 22 ? sk::oldMachine22 (machine21 (v)) : sk::machineOf ((float) v);
    };
    auto removed21 = [&] (double v) { return (ver < 21 && sk::removedMachine21 (machine20 (v))) || (ver == 21 && sk::replacedMachine22 (machine21 (v))); };
    auto afDefault = [] (int machine, int i) {
        static const float HEXBD[6] = { .45f, .72f, .4f, .45f, .6f, .25f }, HEXSD[6] = { .7f, .62f, .62f, .5f, .55f, .35f };
        static const float HHMT[6] = { .5f, .6f, .15f, .3f, .2f, .4f };
        float af[6];
        if (sk::machineAFDefaults (machine, af)) return af[i - sk::P_A];
        const float* d = machine == sk::M_HEXBD ? HEXBD : (machine == sk::M_HEXSD ? HEXSD : HHMT);
        return d[i - sk::P_A];
    };
    // the side pages (P_S1 ..): a machine's defaults (ALLOY), else at rest
    auto sideDefault = [] (int machine, int i) { float d[sk::NUM_SIDES]; const int k = sk::machineSideDefaults (machine, d); const int q = sk::sideIndex (i); return q >= 0 && q < k ? d[q] : 0.0f; };
    // v22 -> v23 (v63): ALLOY's 21 sides onto its 35 (sk::alloySide23: where each went, BNCE bipolar, PENV's under-range
    // three octaves, the new ones at rest)
    const bool al23 = ver == 22;
    // v76: SPHERE (v75's 42nd machine) is gone, WEST in its place: a project saved before (the page marks what it saves,
    // "wst") has its tracks on it at WEST's defaults (A .. F, DEC, the sides), their locks made for SPHERE gone; a step
    // locked onto it from another machine takes WEST's A .. F and sides in its locks (as the page's migrate does)
    const bool sph75 = ver >= 26 && ! st.hasProperty ("wst");
    // v82: HOWL's LOOP page: a project saved before it (no "v82") has v81's DIST (STRING's fourth place) as LOOP's
    // TIME, the same distance; no hum where DIST was; LOOP's other places at HOWL's defaults; its SIDE switch on four
    // pages (as the page's migrate does); on its steps too, and on a step locked onto HOWL from another machine
    const bool pre82 = ! st.hasProperty ("v82");
    const int hDist = sk::sideParam (sk::HW_HUM), hTime = sk::sideParam (sk::HW_TIME);
    // v83: HOWL's CAB and BOW pages: a project saved before them (no "v83") has its SIDE switch on four pages (now six;
    // v81's on three), CAB's and BOW's places at HOWL's defaults (the amp as it was, the bow off); on its steps too, and
    // on a step locked onto HOWL from another machine (as the page's migrate does)
    const bool pre83 = ! st.hasProperty ("v83");
    auto sideH = [&] (double v) { return mid (idx (v, pre82 ? 3 : 4), 6); };
    auto westOwn = [] (int i) { return (i >= sk::P_A && i <= sk::P_F) || sk::sideP (i); };
    // LFO destinations: 18 entries in v1, 24 until v4 (slot 18 was the spectral send before v4), 27 until v7 (the
    // retrig ones appended), 35 until v9 (PRISM appended, v8), then NUM_LFO_DEST (TONE appended, v10): same
    // destination, new grid
    auto destIn = [&] (double raw) {
        int d = idx (raw, ver == 1 ? 18 : (ver < 5 ? 24 : (ver < 8 ? 27 : 35)));
        if (ver < 4 && d == 18) d = 0;
        return mid (d, sk::NUM_LFO_DEST);
    };
    // v7 -> v8: PRISM's WDTH becomes bipolar (12 o'clock = the whole band, a band narrows to the right): the old
    // width (0 narrow .. 1 the whole band) moves to the right half; the DICE destinations grow from 34 to 42
    auto widthIn = [] (double raw) { return (float) (0.5 + 0.5 * (1.0 - juce::jlimit (0.0, 1.0, raw))); };
    auto diceDestIn = [&] (double raw) { return mid (idx (raw, ver < 8 ? 34 : 42), sk::NUM_DICE_DEST); };
    // v24 -> v25 (v65): the LFO waves, 12 (six shapes, then the six restarting with each trig) onto 16 (SLEW and LOOP after
    // EXP in both halves): each wave stays itself on the new grid, its locks too (as the page's migrate does)
    auto waveIn = [&] (double raw) { const int i = idx (raw, 12); return mid (i < 6 ? i : i + 2, sk::NUM_LFO_WAVES); };
    auto isWave = [] (int i) { return i == sk::P_LWAV || i == sk::P_L2WV; };

    pushOp ({ EngineOp::Reset });
    float gv[sk::NUM_GLOBALS];
    // globals a project does not have yet (it was saved before they existed) take the engine's defaults
    for (int i = 0; i < sk::NUM_GLOBALS; ++i) gv[i] = i < g->size() ? (float) (double) (*g)[i] : globalDefaults[i];
    gv[sk::G_LOOP] = 0.0f;
    bool usedRev = false, usedSpc = false;
    if (ver < 4)
    {
        // v3 -> v4: one reverb with six algorithms. The plate keeps its decay (old SIZE) when it was in
        // use, else the spectral settings move over; the DAC starts on the CD-80 model.
        for (int t = 0; t < sk::NUM_TRACKS; ++t)
            if (const auto* p = (*tracks)[t].getProperty ("p", juce::var()).getArray())
            {
                if (p->size() > sk::P_REV && (double) (*p)[sk::P_REV] > 0.001) usedRev = true;
                if (p->size() > sk::P_SPC && (double) (*p)[sk::P_SPC] > 0.001) usedSpc = true;
            }
        const bool spectral = usedSpc && ! usedRev;
        const float oldSize = g->size() > sk::G_RSIZ ? gv[sk::G_RSIZ] : 0.6f;
        if (spectral)
        {
            const int mode = (gv[sk::G_SFRZ] > 0.5f ? 1 : 0) + (gv[sk::G_SREV] > 0.5f ? 2 : 0);
            gv[sk::G_RALG] = mid (sk::REV_SPECTRAL, sk::NUM_REV_ALGOS);
            gv[sk::G_RTIM] = gv[sk::G_STIM]; gv[sk::G_RSIZ] = gv[sk::G_SBLR]; gv[sk::G_RDMP] = gv[sk::G_SATM];
            gv[sk::G_RPRE] = mid (mode, 4); gv[sk::G_RCH1] = gv[sk::G_SWRP];
            gv[sk::G_RCH2] = std::sqrt (juce::jlimit (0.0f, 1.0f, gv[sk::G_SREF])); gv[sk::G_RLVL] = gv[sk::G_SLVL];
        }
        else
        {
            gv[sk::G_RALG] = mid (sk::REV_PLATE, sk::NUM_REV_ALGOS);
            gv[sk::G_RTIM] = oldSize; gv[sk::G_RSIZ] = 0.5f;
            gv[sk::G_RPRE] = 0.0f; gv[sk::G_RCH1] = 0.5f; gv[sk::G_RCH2] = 0.5f; gv[sk::G_RLVL] = 0.6f;
        }
        gv[sk::G_DACM] = mid (sk::DAC_CD80, sk::NUM_DAC_MODELS); gv[sk::G_DACA] = 0.35f;
    }
    // v5 -> v6: DEJA VU starts at LOCK when no DICE lock was on (the knob did nothing then)
    if (ver < 6 && idx (gv[sk::G_DDVM], 4) == 0) gv[sk::G_DDV] = 0.5f;
    if (ver < 10)
        for (int r = 0; r < sk::DICE_OUTS; ++r)
            if (sk::G_DR0 + 3 * r + 1 < g->size()) gv[sk::G_DR0 + 3 * r + 1] = diceDestIn (gv[sk::G_DR0 + 3 * r + 1]);
    // v6 -> v7: GRAIN leaves the reverb (7 algorithms then) for the master chain; a project that used it
    // as its reverb gets HALL back and GRAIN on the master, partly mixed
    if (ver == 6)
    {
        const int a = idx (gv[sk::G_RALG], 7);
        if (a == 6) { gv[sk::G_RALG] = mid (sk::REV_HALL, sk::NUM_REV_ALGOS); gv[sk::G_GON] = 1.0f; gv[sk::G_GMIX] = 0.35f; }
        else gv[sk::G_RALG] = mid (a, sk::NUM_REV_ALGOS);
    }
    // v11 -> v12: RAND (now LIVE, on the SEQ page) only played with GLITCH on
    if (ver == 11 && gv[sk::G_XON] < 0.5f) gv[sk::G_XRND] = 0.0f;
    // LIVE (RAND) moves the tracks marked r (the keys above the pads): the mask comes from them
    {
        int m = 0;
        for (int t = 0; t < sk::NUM_TRACKS && t < tracks->size(); ++t) if (! (int) (*tracks)[t].getProperty ("rx", 0)) m |= 1 << t;
        gv[sk::G_XMSK] = (float) m / 255.0f;
    }
    for (int i = 0; i < sk::NUM_GLOBALS; ++i)
        pushOp ({ EngineOp::Global, 0, 0, i, gv[i] });

    // scale quantiser
    {
        EngineOp op; op.type = EngineOp::Scale; op.p = ver < 4 ? 0 : (int) st.getProperty ("scale", 0);
        pushOp (op);
    }

    // v17 -> v18: DEC at its top becomes HOLD on the machines with an amplitude envelope: an older project keeps its
    // longest decay (as the page's migrate does)
    auto holdM = [] (int m) { return m != sk::M_CP_NZ && m != sk::M_FRACT && m != sk::M_ITERA && m != sk::M_HALO && m != sk::M_BREAK && m != sk::M_RAM && m < sk::M_FXVERB; };
    auto decIn = [&] (int m, float v) { return ver < 18 && v >= sk::DEC_HOLD && holdM (m) ? 0.985f : v; };
    // v18 -> v19: PROG's list grew (the classical progressions, the pedal and the tintinnabuli before WALK) and HALO's
    // switch has a third side (TONE): a HALO track's PROG and F, and their locks, onto the new grids (as the page's
    // migrate does). HARM and the TONE side start where v58 played (the engine's defaults: FREE, at rest)
    auto progIn = [&] (double v) { const int i = idx (v, 13); return mid (i == 12 ? sk::HALO_PROGS - 1 : i, sk::HALO_PROGS); };
    // v19 -> v20: HALO's and STRUM's switch a fourth side (AGE): F onto the four sides (AGE and STRUM's AIR start at
    // rest, the engine's defaults: what v59 played)
    auto sideIn = [&] (int m, double v) {
        if (ver < 19 && m == sk::M_HALO) v = mid (idx (v, 2), 3);
        if (ver < 20 && (m == sk::M_HALO || m == sk::M_STRUM)) v = mid (idx (v, 3), 4);
        return (float) v;
    };
    // the tracks of a pattern: the one playing (the project's tracks: with their mutes, steps as a list) or one in its
    // slot (pats: steps sparse, "st": { "step": {...} })
    auto loadTrack = [&] (int t, const juce::var& tr, bool live)
    {
        if (live) pushOp ({ EngineOp::Mute,  t, 0, 0, (float) (double) tr.getProperty ("mute", 0) });
        pushOp ({ EngineOp::Len,   t, 0, 0, (float) (double) tr.getProperty ("len", 16) });
        pushOp ({ EngineOp::Speed, t, 0, 0, (float) (double) tr.getProperty ("spd", 4) });
        pushOp ({ EngineOp::QTrack, t, 0, 0, (float) (double) tr.getProperty ("q", 1) });
        int machine = 0; bool trackRmb = false, trackGone = false;
        const auto* p0 = tr.getProperty ("p", juce::var()).getArray();
        if (const auto* p = p0)
        {
            // parameters added since the project was saved (the RAM machine's REC side...) start at their defaults
            for (int i = p->size(); i < sk::NUM_PARAMS; ++i) pushOp ({ EngineOp::Param, t, 0, i, paramDefaults[i] });
            for (int i = 0; i < juce::jmin ((int) sk::NUM_PARAMS, p->size()); ++i)
            {
                const double raw = (double) (*p)[i];
                float v = (float) raw;
                if (i == sk::P_MACH) { machine = machineIn (raw); v = sk::machineVal (machine); trackRmb = wasRmb (raw); trackGone = removed21 (raw); }
                else if (sph75 && machine == sk::M_WEST && (westOwn (i) || i == sk::P_DEC)) v = i == sk::P_DEC ? 0.4f : (i <= sk::P_F ? afDefault (machine, i) : sideDefault (machine, i));
                else if ((trackRmb || trackGone) && i >= sk::P_A && i <= sk::P_F) v = afDefault (machine, i);
                else if ((ver < 21 || trackGone) && sk::sideP (i)) v = sideDefault (machine, i);   // (v20's MORE page; v21's MATTER, LATEX)
                else if (al23 && machine == sk::M_ALLOY && i >= sk::P_S1) continue;                  // (v22's ALLOY: below)
                else if (ver < 10 && (i == sk::P_LDST || i == sk::P_L2DS)) v = destIn (raw);
                else if (ver < 25 && isWave (i)) v = waveIn (raw);
                else if (ver < 8 && i == sk::P_PWID) v = widthIn (raw);
                else if (ver < 3 && i == sk::P_A && machine == sk::M_ULOOP) v = srcMap (raw);
                else if (ver < 4 && i == sk::P_REV) v = (float) juce::jmax (raw, p->size() > sk::P_SPC ? (double) (*p)[sk::P_SPC] : 0.0);
                else if (ver < 4 && i == sk::P_SPC) v = 0.5f;      // the old spectral send becomes the RND depth
                // v14 -> v15: BREAK plays the user's slots: its break knob goes to slot 1, KIT to SYNC / VARI (from the
                // old MODE), the ghost notes' weight to SKIP (none), the old MODE to SWAP (none)
                else if (ver == 14 && (i == sk::P_BSKP || i == sk::P_BSWP)) v = 0.0f;
                else if (ver == 14 && machine == sk::M_BREAK && i == sk::P_A) v = mid (0, sk::BRK_SLOTS);
                else if (ver == 14 && machine == sk::M_BREAK && i == sk::P_B) v = p->size() > sk::P_BSWP ? (float) (double) (*p)[sk::P_BSWP] : mid (0, 2);
                else if (ver < 19 && machine == sk::M_HALO && i == sk::P_B) v = progIn (raw);
                else if (ver < 20 && (machine == sk::M_HALO || machine == sk::M_STRUM) && i == sk::P_F) v = sideIn (machine, raw);
                else if (i == sk::P_DEC) v = decIn (machine, v);
                pushOp ({ EngineOp::Param, t, 0, i, v });
            }
            if (pre82 && machine == sk::M_HOWL)
            {
                const float d = hDist < p->size() ? (float) (double) (*p)[hDist] : 0.4f;
                pushOp ({ EngineOp::Param, t, 0, hTime, sk::howlTimeFromDist (d) });
                pushOp ({ EngineOp::Param, t, 0, hDist, 0.0f });
                for (int q = sk::HW_TIME + 1; q < sk::HW_NSIDE; ++q) pushOp ({ EngineOp::Param, t, 0, sk::sideParam (q), sideDefault (machine, sk::sideParam (q)) });
                if (sk::P_F < p->size()) pushOp ({ EngineOp::Param, t, 0, sk::P_F, sideH ((double) (*p)[sk::P_F]) });
            }
            else if (pre83 && machine == sk::M_HOWL)
            {
                for (int q = sk::HW_TGHT; q < sk::HW_NSIDE; ++q) pushOp ({ EngineOp::Param, t, 0, sk::sideParam (q), sideDefault (machine, sk::sideParam (q)) });
                if (sk::P_F < p->size()) pushOp ({ EngineOp::Param, t, 0, sk::P_F, sideH ((double) (*p)[sk::P_F]) });
            }
            // (a v20 project has no side pages: ALLOY's at its defaults)
            if (ver < 21)
                for (int i = juce::jmax ((int) sk::P_S1, p->size()); i <= sk::P_S35; ++i) pushOp ({ EngineOp::Param, t, 0, i, sideDefault (machine, i) });
            // (v22's ALLOY: its sides onto the five pages)
            if (al23 && machine == sk::M_ALLOY)
            {
                float o[21], nw[sk::NUM_SIDES];
                for (int q = 0; q < 21; ++q) o[q] = sk::P_S1 + q < p->size() ? (float) (double) (*p)[sk::P_S1 + q] : 0.0f;
                sk::alloySide23 (o, nw);
                for (int q = 0; q < sk::NUM_SIDES; ++q) pushOp ({ EngineOp::Param, t, 0, sk::sideParam (q), nw[q] });
            }
            // v78: the side places added since (S36 ..) at its machine's own (WEST's whole panel), not at rest (the older
            // places, S1 .. S35, done above by their own versions)
            for (int i = juce::jmax (p->size(), (int) sk::P_S36); i < sk::NUM_PARAMS; ++i) if (sk::sideP (i)) pushOp ({ EngineOp::Param, t, 0, i, sideDefault (machine, i) });
        }
        const int savedParams = p0 ? p0->size() : 0;
        const auto* steps = tr.getProperty ("steps", juce::var()).getArray();
        const auto* sparse = tr.getProperty ("st", juce::var()).getDynamicObject();
        const int nSteps = live ? (steps ? juce::jmin ((int) sk::NUM_STEPS, steps->size()) : 0) : (sparse ? (int) sk::NUM_STEPS : 0);
            for (int s = 0; s < nSteps; ++s)
            {
                const juce::var st_ = live ? (*steps)[s] : sparse->getProperty (juce::Identifier (juce::String (s)));
                if (st_.isVoid()) continue;
                if ((int) st_.getProperty ("on", 0) != 0)
                    pushOp ({ EngineOp::Trig, t, s, 0, 1.0f });
                if ((int) st_.getProperty ("rec", 0) != 0)
                    pushOp ({ EngineOp::Rec, t, s, 0, 1.0f });
                if ((int) st_.getProperty ("tl", 0) != 0)
                    pushOp ({ EngineOp::Trigless, t, s, 0, 1.0f });
                const int r = (int) st_.getProperty ("r", 1);
                if (r > 1)
                    pushOp ({ EngineOp::Ratchet, t, s, 0, (float) r });
                if (auto* locks = st_.getProperty ("l", juce::var()).getDynamicObject())
                {
                    int stepMachine = machine;
                    bool lockRmb = false, lockGone = false;
                    if (locks->hasProperty ("0"))
                    {
                        stepMachine = machineIn ((double) locks->getProperty ("0"));
                        lockRmb = wasRmb ((double) locks->getProperty ("0"));
                        lockGone = removed21 ((double) locks->getProperty ("0"));
                    }
                    // a step locked onto a RUMBLE voice or a machine gone in v21 or v22: its new machine at its defaults (its
                    // side pages too); on such a track, the A..F and side locks made for the old machine go
                    const bool dropAF = lockRmb || lockGone || ((trackRmb || trackGone) && ! locks->hasProperty ("0"));
                    const bool dropS = ver < 21 || lockGone || (trackGone && ! locks->hasProperty ("0"));
                    if (lockRmb || lockGone)
                        for (int pi = sk::P_A; pi <= sk::P_F; ++pi) pushOp ({ EngineOp::Lock, t, s, pi, afDefault (stepMachine, pi) });
                    if (lockGone && stepMachine != machine)
                    {
                        float d[sk::NUM_SIDES]; const int k = sk::machineSideDefaults (stepMachine, d);
                        for (int q = 0; q < k; ++q) pushOp ({ EngineOp::Lock, t, s, sk::sideParam (q), d[q] });
                    }
                    // v22: a step that plays ALLOY, its side locks onto the five pages; locked onto ALLOY from another
                    // machine, the new ones at rest in it too (NCOL from its MALT)
                    const bool lockAl23 = al23 && stepMachine == sk::M_ALLOY && ! dropS;
                    if (lockAl23 && machine != sk::M_ALLOY)
                    {
                        float o[21], nw[sk::NUM_SIDES]; bool to[sk::NUM_SIDES] = {};
                        for (int q = 0; q < 21; ++q) { o[q] = 0.0f; to[sk::alloySide23To (q)] = true; }
                        o[0] = locks->hasProperty (juce::Identifier (juce::String ((int) sk::P_S1))) ? (float) (double) locks->getProperty (juce::Identifier (juce::String ((int) sk::P_S1))) : 0.75f;
                        sk::alloySide23 (o, nw);
                        for (int q = 0; q < sk::NUM_SIDES; ++q) if (! to[q]) pushOp ({ EngineOp::Lock, t, s, sk::sideParam (q), nw[q] });
                    }
                    // v78: a step locked onto WEST from another machine, saved before its places S36 .. were: WEST's own
                    // in its locks (else they would be the other machine's, at rest: no output)
                    if (locks->hasProperty ("0") && stepMachine == sk::M_WEST && machine != sk::M_WEST && savedParams > 0 && savedParams <= sk::P_S36)
                    {
                        float d[sk::NUM_SIDES]; const int k = sk::machineSideDefaults (sk::M_WEST, d);
                        for (int q = sk::NUM_SIDES_A; q < k; ++q) pushOp ({ EngineOp::Lock, t, s, sk::sideParam (q), d[q] });
                    }
                    const bool lockW76 = sph75 && stepMachine == sk::M_WEST;
                    // (v82: HOWL's locks: DIST onto TIME, SIDE on four pages; locked onto HOWL from another machine, LOOP's
                    // other places at its defaults in the lock)
                    const bool lockH82 = pre82 && stepMachine == sk::M_HOWL, lockH83 = pre83 && stepMachine == sk::M_HOWL;
                    if (lockH83 && locks->hasProperty ("0") && machine != sk::M_HOWL)
                        for (int q = pre82 ? sk::HW_TIME + 1 : sk::HW_TGHT; q < sk::HW_NSIDE; ++q) pushOp ({ EngineOp::Lock, t, s, sk::sideParam (q), sideDefault (sk::M_HOWL, sk::sideParam (q)) });
                    for (const auto& kv : locks->getProperties())
                    {
                        const int pi = kv.name.toString().getIntValue();
                        const double raw = (double) kv.value;
                        float v = (float) raw;
                        if (ver < 4 && pi == sk::P_SPC) continue;        // spectral-send locks are dropped
                        if (lockH82 && pi == hDist) { pushOp ({ EngineOp::Lock, t, s, hTime, sk::howlTimeFromDist (v) }); continue; }
                        if (lockH83 && pi == sk::P_F) { pushOp ({ EngineOp::Lock, t, s, pi, sideH (raw) }); continue; }
                        if (lockW76 && westOwn (pi))                     // (v75's SPHERE: WEST's defaults, or gone)
                        {
                            if (locks->hasProperty ("0")) pushOp ({ EngineOp::Lock, t, s, pi, pi <= sk::P_F ? afDefault (stepMachine, pi) : sideDefault (stepMachine, pi) });
                            continue;
                        }
                        if (dropS && sk::sideP (pi)) continue;           // (v20's MORE page, gone with STRING and MODAL; MATTER's, LATEX's)
                        if (dropAF && pi >= sk::P_A && pi <= sk::P_F) continue;
                        if (ver == 14 && stepMachine == sk::M_BREAK && (pi == sk::P_A || pi == sk::P_B || pi == sk::P_BSKP)) continue;
                        if (ver == 14 && pi == sk::P_BSWP) { if (stepMachine == sk::M_BREAK) pushOp ({ EngineOp::Lock, t, s, sk::P_B, v }); continue; }
                        if (lockAl23 && pi >= sk::P_S1 && pi < sk::P_S1 + 21)
                        {
                            const int q = pi - sk::P_S1;
                            pushOp ({ EngineOp::Lock, t, s, sk::P_S1 + sk::alloySide23To (q), sk::alloySide23Val (q, v) });
                            continue;
                        }
                        if (pi == sk::P_MACH) v = sk::machineVal (stepMachine);
                        else if (ver < 10 && (pi == sk::P_LDST || pi == sk::P_L2DS)) v = destIn (raw);
                        else if (ver < 25 && isWave (pi)) v = waveIn (raw);
                        else if (ver < 8 && pi == sk::P_PWID) v = widthIn (raw);
                        else if (ver < 3 && pi == sk::P_A && stepMachine == sk::M_ULOOP) v = srcMap (raw);
                        else if (ver < 19 && stepMachine == sk::M_HALO && pi == sk::P_B) v = progIn (raw);
                        else if (ver < 20 && (stepMachine == sk::M_HALO || stepMachine == sk::M_STRUM) && pi == sk::P_F) v = sideIn (stepMachine, raw);
                        else if (pi == sk::P_DEC) v = decIn (stepMachine, v);
                        pushOp ({ EngineOp::Lock, t, s, pi, v });
                    }
                }
            }
    };

    // the patterns (v18): the one playing (no change), then the others into their slots, CHG, the chain
    pushOp ({ EngineOp::PatAll });
    const int cur = juce::jlimit (0, sk::NUM_PATS - 1, (int) st.getProperty ("pat", 0));
    pushOp ({ EngineOp::PatCur, cur });
    for (int t = 0; t < sk::NUM_TRACKS; ++t) loadTrack (t, (*tracks)[t], true);
    if (const auto* pats = st.getProperty ("pats", juce::var()).getArray())
        for (int i = 0; i < juce::jmin ((int) sk::NUM_PATS, pats->size()); ++i)
        {
            const auto& pt = (*pats)[i];
            const auto* ptr = pt.getProperty ("tr", juce::var()).getArray();
            if (i == cur || ptr == nullptr || ptr->size() != sk::NUM_TRACKS) continue;
            pushOp ({ EngineOp::PatEdit, i });
            pushOp ({ EngineOp::PatClear, i });
            for (int t = 0; t < sk::NUM_TRACKS; ++t) loadTrack (t, (*ptr)[t], false);
            pushOp ({ EngineOp::PatChg, (int) pt.getProperty ("chg", 0) });
        }
    pushOp ({ EngineOp::PatEdit, cur });
    pushOp ({ EngineOp::PatChg, (int) st.getProperty ("pchg", 0) });
    if (const auto* ch = st.getProperty ("chain", juce::var()).getArray())
    {
        const int n = juce::jmin ((int) sk::CHAIN_MAX, ch->size());
        for (int k = 0; k < n; ++k) { EngineOp op; op.type = EngineOp::PatChain; op.s = k; op.t = juce::jlimit (0, sk::NUM_PATS - 1, (int) (*ch)[k]); pushOp (op); }
        pushOp ({ EngineOp::PatChainLen, n });
    }
}

void ImpaktProcessor::setStateFromUi (const juce::String& json)
{
    const juce::ScopedLock sl (stateLock);
    stateJson = json;
}

juce::String ImpaktProcessor::getStateJson() const
{
    const juce::ScopedLock sl (stateLock);
    return stateJson;
}

// The project: "IMPAKT1", the page's JSON, then the RAM machine's buffers ("RAM1", how many, and for each one its
// track, length, sample rate and 16-bit samples, little-endian), then BREAK's slots ("BRK1", how many, and for each one
// its slot, length, bars, name and samples: mono, 32 kHz), then CUBE's banks ("CUB1", how many, and for each one its
// place, length, name and 16-bit samples: up to 64 waves of 256). A project saved before v37 ends after the JSON, one
// saved before v51 after the buffers, one saved before v74 after the slots.
static constexpr int ramMagic = 0x314D4152;   // "RAM1"
static constexpr int brkMagic = 0x314B5242;   // "BRK1"
static constexpr int cubMagic = 0x31425543;   // "CUB1"

// a loop from the page: written while the audio thread waits, then read by the engine over the next blocks
void ImpaktProcessor::loadBreakSlot (int slot, int bars, const juce::String& name, const juce::MemoryBlock& samples)
{
    if (slot < 0 || slot >= sk::BRK_SLOTS) return;
    const int n = juce::jmin ((int) (samples.getSize() / sizeof (short)), (int) sk::BRK_CAP);
    suspendProcessing (true);
    if (n > 0) memcpy (engine->brk.mem (slot), samples.getData(), (size_t) n * sizeof (short));
    engine->breakSet (slot, n, bars);
    suspendProcessing (false);
    const juce::ScopedLock sl (stateLock);
    brkNames[slot] = name.substring (0, 40);
}

// a CUBE bank from the page: written while the audio thread waits; the engine makes its versions over the next blocks
void ImpaktProcessor::loadCubeBank (int bank, const juce::String& name, const juce::MemoryBlock& samples)
{
    if (bank < 0 || bank >= sk::CUBE_BANKS) return;
    const int n = juce::jmin ((int) (samples.getSize() / sizeof (short)), (int) sk::CUBE_BANK_N);
    suspendProcessing (true);
    engine->cubeSetBank (bank, static_cast<const short*> (samples.getData()), n);
    suspendProcessing (false);
    const juce::ScopedLock sl (stateLock);
    cubeNames[bank] = name.substring (0, 40);
}

void ImpaktProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream out (destData, false);
    out.writeString ("IMPAKT1");
    out.writeString (getStateJson());
    int count = 0;
    for (int t = 0; t < sk::NUM_TRACKS; ++t) if (engine->ram[t].len > 0) ++count;
    out.writeInt (ramMagic);
    out.writeInt (count);
    for (int t = 0; t < sk::NUM_TRACKS; ++t)
    {
        const int len = engine->ram[t].len;
        if (len <= 0) continue;
        out.writeInt (t); out.writeInt (len); out.writeFloat (engine->ram[t].rate);
        out.write (engine->ramMem[t], (size_t) len * sizeof (short));
    }
    int nb = 0;
    for (int k = 0; k < sk::BRK_SLOTS; ++k) if (engine->brk.state[k] > 0) ++nb;
    out.writeInt (brkMagic);
    out.writeInt (nb);
    for (int k = 0; k < sk::BRK_SLOTS; ++k)
    {
        if (engine->brk.state[k] <= 0) continue;
        const int len = engine->brk.len[k];
        out.writeInt (k); out.writeInt (len); out.writeInt (engine->brk.bars[k]);
        out.writeString (getBreakName (k));
        out.write (engine->brk.mem (k), (size_t) len * sizeof (short));
    }
    int nc = 0;
    for (int k = 0; k < sk::CUBE_BANKS; ++k) if (engine->cube.len[k] > 0) ++nc;
    out.writeInt (cubMagic);
    out.writeInt (nc);
    for (int k = 0; k < sk::CUBE_BANKS; ++k)
    {
        const int len = engine->cube.len[k];
        if (len <= 0) continue;
        out.writeInt (k); out.writeInt (len);
        out.writeString (getCubeName (k));
        out.write (engine->cube.raw[k], (size_t) len * sizeof (short));
    }
    radio->write (out);
}

void ImpaktProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream in (data, (size_t) sizeInBytes, false);
    if (in.readString() != "IMPAKT1") return;
    const auto json = in.readString();
    const auto parsed = juce::JSON::parse (json);
    if (! parsed.isObject()) return;
    {
        const juce::ScopedLock sl (stateLock);
        stateJson = json;
    }
    pushStateOps (parsed);
    // the buffers: the project's own (none in an older project), written while the audio thread waits
    suspendProcessing (true);
    for (int t = 0; t < sk::NUM_TRACKS; ++t) engine->ramClear (t);
    if (in.getNumBytesRemaining() >= 8 && in.readInt() == ramMagic)
    {
        const int count = in.readInt();
        for (int k = 0; k < count && in.getNumBytesRemaining() >= 12; ++k)
        {
            const int t = in.readInt(), len = in.readInt();
            const float rate = in.readFloat();
            if (t < 0 || t >= sk::NUM_TRACKS || len < 0 || len > sk::RAM_LEN || in.getNumBytesRemaining() < (juce::int64) len * 2) break;
            in.read (engine->ramMem[t], len * (int) sizeof (short));
            engine->ramLoaded (t, len, rate);
        }
    }
    // BREAK's slots (none in an older project), read at once: a bounce may follow
    juce::String names[sk::BRK_SLOTS];
    for (int k = 0; k < sk::BRK_SLOTS; ++k) engine->breakClear (k);
    if (in.getNumBytesRemaining() >= 8 && in.readInt() == brkMagic)
    {
        const int count = in.readInt();
        for (int c = 0; c < count && in.getNumBytesRemaining() >= 12; ++c)
        {
            const int k = in.readInt(), len = in.readInt(), bars = in.readInt();
            const auto name = in.readString();
            if (k < 0 || k >= sk::BRK_SLOTS || len < 0 || len > sk::BRK_CAP || in.getNumBytesRemaining() < (juce::int64) len * 2) break;
            in.read (engine->brk.mem (k), len * (int) sizeof (short));
            engine->breakSet (k, len, bars);
            names[k] = name;
        }
    }
    engine->breakWork (1 << 30);
    // CUBE's banks (none in an older project), their versions made at once: a bounce may follow
    juce::String cnames[sk::CUBE_BANKS];
    for (int k = 0; k < sk::CUBE_BANKS; ++k) engine->cubeClear (k);
    if (in.getNumBytesRemaining() >= 8 && in.readInt() == cubMagic)
    {
        const int count = in.readInt();
        std::vector<short> tmp ((size_t) sk::CUBE_BANK_N);
        for (int c = 0; c < count && in.getNumBytesRemaining() >= 8; ++c)
        {
            const int k = in.readInt(), len = in.readInt();
            const auto name = in.readString();
            if (k < 0 || k >= sk::CUBE_BANKS || len < 0 || len > sk::CUBE_BANK_N || in.getNumBytesRemaining() < (juce::int64) len * 2) break;
            in.read (tmp.data(), len * (int) sizeof (short));
            engine->cubeSetBank (k, tmp.data(), len);
            cnames[k] = name;
        }
    }
    // RADIO's receivers (v87; none in an older project): their sources (they connect when a track plays them)
    radio->read (in);
    while (engine->cube.busy >= 0) engine->cubeStep (64);
    suspendProcessing (false);
    {
        const juce::ScopedLock sl (stateLock);
        for (int k = 0; k < sk::BRK_SLOTS; ++k) brkNames[k] = names[k];
        for (int k = 0; k < sk::CUBE_BANKS; ++k) cubeNames[k] = cnames[k];
    }
    ++stateVersion;
}

// ----------------------------------------------------------------------------- audio
void ImpaktProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    {   // apply pending edits from the UI
        const auto scope = fifo.read (fifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i) applyOp (fifoData[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i) applyOp (fifoData[(size_t) (scope.startIndex2 + i)]);
    }

    sk::HostTime ht {};
    if (auto* ph = getPlayHead())
        if (auto p = ph->getPosition())
        {
            ht.valid = 1;
            ht.playing = p->getIsPlaying() ? 1 : 0;
            ht.ppq = p->getPpqPosition().orFallback (0.0);
            ht.bpm = p->getBpm().orFallback (120.0);
        }

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    // the input bus (RAM: IN), copied before the engine writes its output over the same channels
    {
        const int nIn = getTotalNumInputChannels();
        if ((int) inBufL.size() < numSamples) { inBufL.resize ((size_t) numSamples); inBufR.resize ((size_t) numSamples); }
        if (nIn > 0)
        {
            juce::FloatVectorOperations::copy (inBufL.data(), buffer.getReadPointer (0), numSamples);
            juce::FloatVectorOperations::copy (inBufR.data(), buffer.getReadPointer (nIn > 1 ? 1 : 0), numSamples);
        }
        else
        {
            juce::FloatVectorOperations::clear (inBufL.data(), numSamples);
            juce::FloatVectorOperations::clear (inBufR.data(), numSamples);
        }
    }
    const int numOut = juce::jmax (1, getTotalNumOutputChannels());
    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 && numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    if (R == nullptr)
    {
        if ((int) monoBuf.size() < numSamples) monoBuf.resize ((size_t) numSamples);
        R = monoBuf.data();
    }

    // split the block at MIDI notes so pads (notes 36..43 = tracks 1..8) are sample accurate
    const double sr = getSampleRate() > 0 ? getSampleRate() : 48000.0;
    int done = 0;
    auto renderTo = [&] (int end)
    {
        if (end <= done) return;
        sk::HostTime sub = ht;
        if (sub.valid) sub.ppq = ht.ppq + (double) done * ht.bpm / 60.0 / sr;
        engine->setInput (inBufL.data() + done, inBufR.data() + done);
        engine->process (L + done, R + done, end - done, &sub);
        done = end;
    };
    // v81: channel 1 (and 10 ..) plays the pads as before; channels 2 .. 9 play tracks 1 .. 8 as a keyboard: C4 the track's
    // own pitch (HOWL: the note is its chord's root, E2 at TUNE's 12 o'clock; on POLY, each note a string)
    auto keyNote = [&] (int t, int note) {
        const bool howl = sk::machineOf (engine->tr[t].p[sk::P_MACH]) == sk::M_HOWL;
        return (float) juce::jlimit (-48, 48, note - (howl ? 40 : 60));
    };
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            renderTo (juce::jlimit (0, numSamples, meta.samplePosition));
            const int note = msg.getNoteNumber(), ch = msg.getChannel();
            if (ch >= 2 && ch <= 1 + sk::NUM_TRACKS) engine->audition (ch - 2, msg.getFloatVelocity(), keyNote (ch - 2, note));
            else if (note >= 36 && note < 36 + sk::NUM_TRACKS)
                engine->audition (note - 36, msg.getFloatVelocity());
        }
        else if (msg.isNoteOff())
        {
            // DEC on HOLD: the note held since its note-on ends
            renderTo (juce::jlimit (0, numSamples, meta.samplePosition));
            const int note = msg.getNoteNumber(), ch = msg.getChannel();
            if (ch >= 2 && ch <= 1 + sk::NUM_TRACKS) engine->release (ch - 2, keyNote (ch - 2, note));
            else if (note >= 36 && note < 36 + sk::NUM_TRACKS) engine->release (note - 36);
        }
    }
    renderTo (numSamples);
    if (numOut == 1) for (int i = 0; i < numSamples; ++i) L[i] = 0.5f * (L[i] + R[i]);
    for (int c = numOut == 1 ? 1 : 2; c < numCh; ++c) buffer.clear (c, 0, numSamples);

    for (int t = 0; t < sk::NUM_TRACKS; ++t) { pos.steps[t] = engine->getStep (t); pos.hits[t] = engine->getHits (t); }
    pos.running = engine->running;
    pos.hostRunning = ht.valid && ht.playing ? 1 : 0;
    pos.bpm = engine->bpm;
    { const float* pi = engine->getPatInfo(); for (int k = 0; k < sk::PAT_INFO; ++k) pos.pat[k].store (pi[k], std::memory_order_relaxed); }
    pos.qmask.store ((int) engine->quantMask(), std::memory_order_relaxed);

    // meters for the editor: keep the highest peak until the editor reads it
    auto atomicMax = [] (std::atomic<float>& a, float v)
    {
        float cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    };
    atomicMax (meters.gr, engine->meterGr());
    atomicMax (meters.out, engine->meterOut());
    engine->meterReset();
    float mix[mixInfoSize];
    engine->mixInfo (mix);
    for (int k = 0; k < mixInfoSize; ++k) meters.mix[k].store (mix[k], std::memory_order_relaxed);
    meters.mixValid.store (1, std::memory_order_release);
    meters.fx[0].store ((float) engine->glfoPh, std::memory_order_relaxed); meters.fx[1].store (engine->glfoVal, std::memory_order_relaxed);
    for (int b = 0; b < 3; ++b) meters.fx[2 + b].store (engine->ott.gainDb[b], std::memory_order_relaxed);
    meters.fx[5].store (engine->g[sk::G_GON] > 0.5f ? 1.0f : 0.0f, std::memory_order_relaxed);
    meters.fx[6].store (engine->g[sk::G_OON] > 0.5f ? 1.0f : 0.0f, std::memory_order_relaxed);
    {
        float gi[22]; engine->glitchInfo (gi);
        for (int k = 0; k < 22; ++k) meters.glitch[k].store (gi[k], std::memory_order_relaxed);
    }
    const float* sp = engine->specInfo();
    for (int k = 0; k < sk::Engine::SPEC_BANDS; ++k) meters.spec[k].store (sp[k], std::memory_order_relaxed);
    // the TRACK screen's modulation view: LFO 1 / 2 (phase, value), EF 1 / 2 (value, input) of every track
    {
        float mi[sk::MOD_INFO * sk::NUM_TRACKS];
        engine->modInfo (mi);
        // (v90) places and values: the latest; peaks and inputs: the highest until the editor reads them (it resets them)
        for (int k = 0; k < sk::MOD_INFO * sk::NUM_TRACKS; ++k)
            if ((k % sk::MOD_INFO) < 8 || mi[k] > meters.mod[k].load (std::memory_order_relaxed)) meters.mod[k].store (mi[k], std::memory_order_relaxed);
    }
    if (const int pt = meters.prismT.load (std::memory_order_relaxed); pt >= 0)
    {
        const float* pi = engine->prismInfo (pt);
        for (int k = 0; k < 2 * sk::PR_K; ++k) meters.prism[k].store (pi[k], std::memory_order_relaxed);
    }
    if (const int ht = meters.haloT.load (std::memory_order_relaxed); ht >= 0)
    {
        float hi[sk::HALO_INFO]; engine->haloInfo (ht, hi);
        for (int k = 0; k < sk::HALO_INFO; ++k) meters.halo[k].store (hi[k], std::memory_order_relaxed);
    }
    if (const int bt = meters.brkT.load (std::memory_order_relaxed); bt >= 0)
    {
        float bi[sk::BRK_INFO]; engine->breakInfo (bt, bi);
        for (int k = 0; k < sk::BRK_INFO; ++k) meters.brk[k].store (bi[k], std::memory_order_relaxed);
    }
    if (const int ft = meters.fxmT.load (std::memory_order_relaxed); ft >= 0)
    {
        float fi[sk::FX_INFO]; engine->fxInfo (ft, fi);
        for (int k = 0; k < sk::FX_INFO; ++k)
            if (k == 4 || k == 5 || k == 12) meters.fxm[k].store (juce::jmax (meters.fxm[k].load (std::memory_order_relaxed), fi[k]), std::memory_order_relaxed);   // (the page takes them)
            else meters.fxm[k].store (fi[k], std::memory_order_relaxed);
    }
    if (const int ct = meters.chimT.load (std::memory_order_relaxed); ct >= 0)
    {
        float ci[sk::CHIM_INFO]; engine->chimInfo (ct, ci);
        for (int k = 0; k < sk::CHIM_INFO; ++k) meters.chim[k].store (ci[k], std::memory_order_relaxed);
    }
    if (const int cb = meters.cubeT.load (std::memory_order_relaxed); cb >= 0)
    {
        float ci[sk::CUBE_INFO]; engine->cubeInfo (cb, ci);
        for (int k = 0; k < sk::CUBE_INFO; ++k) meters.cube[k].store (ci[k], std::memory_order_relaxed);
    }
    meters.cubeVer.store (engine->cube.ver, std::memory_order_release);
    if (const int wt = meters.westT.load (std::memory_order_relaxed); wt >= 0)
    {
        float wi[sk::WS_INFO]; engine->westInfo (wt, wi);
        for (int k = 0; k < sk::WS_INFO; ++k) meters.west[k].store (wi[k], std::memory_order_relaxed);
    }
    // RADIO: the receivers the tracks play (the network keeps those connected), the track the page shows, the receivers
    {
        const int u = engine->radioUse();
        radio->setUse (u);
        meters.radUse.store (u, std::memory_order_relaxed);
        float ri[sk::RADIO_RX * sk::RADIO_RX_INFO]; engine->radioRxInfo (ri);
        for (int k = 0; k < sk::RADIO_RX * sk::RADIO_RX_INFO; ++k) meters.radRx[k].store (ri[k], std::memory_order_relaxed);
        if (const int rt = meters.radT.load (std::memory_order_relaxed); rt >= 0)
        {
            float ti[sk::RADIO_INFO]; engine->radioInfo (rt, ti);
            for (int k = 0; k < sk::RADIO_INFO; ++k) meters.rad[k].store (ti[k], std::memory_order_relaxed);
        }
    }
    // BREAK's slots, when they change: their state and waveforms for the page's LOAD panel
    if (engine->brk.ver != brkSeen)
    {
        brkSeen = engine->brk.ver;
        float si[1 + sk::BRK_SLOTS * sk::BRK_SLOT_INFO]; engine->breakSlots (si);
        for (int k = 0; k < sk::BRK_SLOTS * sk::BRK_SLOT_INFO; ++k) meters.brkSlots[k].store (si[1 + k], std::memory_order_relaxed);
        float pk[sk::BRK_NPEAK];
        for (int s = 0; s < sk::BRK_SLOTS; ++s)
        {
            engine->breakPeaks (s, pk);
            for (int k = 0; k < sk::BRK_NPEAK; ++k) meters.brkPeaks[s * sk::BRK_NPEAK + k].store (pk[k], std::memory_order_relaxed);
        }
        meters.brkVer.store (brkSeen, std::memory_order_release);
    }
}

juce::AudioProcessorEditor* ImpaktProcessor::createEditor()
{
    return new ImpaktEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ImpaktProcessor();
}
