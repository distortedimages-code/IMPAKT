#include <cstring>
#include "PluginProcessor.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

struct FakeHead : juce::AudioPlayHead
{
    double ppq = 0, bpm = 124; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setIsPlaying (playing); p.setPpqPosition (ppq); p.setBpm (bpm); return p;
    }
};

static int fails = 0;
static void check (const char* name, bool ok, const juce::String& info = {})
{
    std::printf ("%s %s %s\n", ok ? "PASS" : "FAIL", name, info.toRawUTF8());
    if (! ok) ++fails;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto json = juce::File (argv[1]).loadFileAsString();
    juce::MemoryBlock mb; { juce::MemoryOutputStream o (mb, false); o.writeString ("IMPAKT1"); o.writeString (json); }

    ImpaktProcessor proc;
    // CHIMERA's bank: the first processor reads it in the background ($IMPAKT_BANK: the repository's bank/)
    for (int k = 0; k < 300 && sk::BANK.state != 2 && sk::BANK.state != -1; ++k) juce::Thread::sleep (50);
    check ("CHIMERA: the bank read in the background (samples decoded)", sk::BANK.state == 2 && sk::BANK.done == sk::BANK.n && sk::BANK.n > 100,
           juce::String (sk::BANK.done) + "/" + juce::String (sk::BANK.n) + " from " + ImpaktProcessor::bankFolder().getFullPathName());
    FakeHead head; proc.setPlayHead (&head);
    proc.setRateAndBufferSizeDetails (48000.0, 256);
    proc.prepareToPlay (48000.0, 256);
    proc.setStateInformation (mb.getData(), (int) mb.getSize());

    // render 4 s following the host transport, compare with the reference render
    const int total = 48000 * 4; const int bs = std::getenv ("BS") ? std::atoi (std::getenv ("BS")) : 256;
    std::vector<float> L (total), R (total);
    juce::AudioBuffer<float> buf (2, bs); juce::MidiBuffer midi;
    for (int i = 0; i < total; i += bs)
    {
        buf.clear();
        proc.processBlock (buf, midi);
        head.ppq += bs * head.bpm / 60.0 / 48000.0;
        for (int k = 0; k < bs; ++k) { L[(size_t) (i + k)] = buf.getSample (0, k); R[(size_t) (i + k)] = buf.getSample (1, k); }
    }
    if (const char* dump = std::getenv ("DUMP"))    // the rendered left channel, raw float32 (debugging the comparison)
        juce::File (dump).replaceWithData (L.data(), L.size() * sizeof (float));
    double e = 0; for (auto v : L) e += v * v;
    check ("host-synced playback produces audio", e / total > 1e-4, juce::String (10 * std::log10 (e / total + 1e-12), 1) + " dB");

    // reference: native render of the same pattern through the web ops (written by render_test)
    juce::File ref (argv[2]);
    if (ref.existsAsFile())
    {
        juce::MemoryBlock wav; ref.loadFileAsData (wav);
        auto* s16 = reinterpret_cast<const int16_t*> (static_cast<const char*> (wav.getData()) + 44);
        double maxd = 0;
        for (int i = 0; i < total; ++i) maxd = std::max (maxd, (double) std::abs (L[(size_t) i] - s16[2 * i] / 32768.0f));
        int first = -1;
        for (int i = 0; i < total && first < 0; ++i) if (std::abs (L[(size_t) i] - s16[2 * i] / 32768.0f) > 0.002f) first = i;
        check ("plugin state load == web ops render", maxd < 0.002, "max diff " + juce::String (maxd, 5) + (first >= 0 ? " from sample " + juce::String (first) : juce::String()));
    }
    // CHIMERA (v66): a project with it (a family per step, a graft) in the plugin, against its ops rendered by the native
    // engine (argv[3], argv[4]: tools made them from the page)
    if (argc >= 5)
    {
        const auto cj = juce::File (argv[3]).loadFileAsString();
        juce::MemoryBlock cm; { juce::MemoryOutputStream o (cm, false); o.writeString ("IMPAKT1"); o.writeString (cj); }
        ImpaktProcessor pc; FakeHead hc; pc.setPlayHead (&hc);
        pc.setRateAndBufferSizeDetails (48000.0, bs); pc.prepareToPlay (48000.0, bs);
        pc.setStateInformation (cm.getData(), (int) cm.getSize());
        std::vector<float> cl (total);
        for (int i = 0; i < total; i += bs)
        {
            buf.clear(); pc.processBlock (buf, midi);
            hc.ppq += bs * hc.bpm / 60.0 / 48000.0;
            for (int q = 0; q < bs; ++q) cl[(size_t) (i + q)] = buf.getSample (0, q);
        }
        juce::MemoryBlock wv; const bool got = juce::File (argv[4]).loadFileAsData (wv) && (int) wv.getSize() >= 44 + total * 4;
        double md = 1.0, ce = 0.0;
        if (got)
        {
            auto* s16 = reinterpret_cast<const int16_t*> (static_cast<const char*> (wv.getData()) + 44);
            md = 0.0; for (int q = 0; q < total; ++q) { md = std::max (md, (double) std::abs (cl[(size_t) q] - s16[2 * q] / 32768.0f)); ce += cl[(size_t) q] * cl[(size_t) q]; }
        }
        check ("CHIMERA: a project with it in the plugin == its ops rendered natively", got && md < 0.002 && ce > 1.0,
               "max diff " + juce::String (md, 5) + ", energy " + juce::String (ce, 1));
    }

    // state round trip: the project as loaded (its JSON first, then the RAM and BREAK sections, empty here), saved,
    // loaded again and saved again: the same bytes
    juce::MemoryBlock out; proc.getStateInformation (out);
    const bool sameHead = out.getSize() > mb.getSize() && std::memcmp (out.getData(), mb.getData(), mb.getSize()) == 0;
    proc.setStateInformation (out.getData(), (int) out.getSize());
    juce::MemoryBlock out2; proc.getStateInformation (out2);
    check ("state round-trip", sameHead && out2 == out, juce::String ((int) out.getSize()) + " bytes");

    // UI ops: clear track 1 trigs, then MIDI note 36 auditions it with transport stopped
    head.playing = false;
    juce::Array<juce::var> ops;
    for (int s = 0; s < 16; ++s) ops.add (juce::Array<juce::var> { "trig", 0, s, 0 });
    ops.add (juce::Array<juce::var> { "p", 0, 1, 0.7 });
    auto* obj = new juce::DynamicObject(); obj->setProperty ("ops", ops);
    proc.pushOpsFromUi (juce::var (obj));
    for (int i = 0; i < 48000 * 3 / bs; ++i) { buf.clear(); proc.processBlock (buf, midi); }   // let tails die
    buf.clear(); proc.processBlock (buf, midi);
    check ("quiet when stopped (reverb tails only)", buf.getMagnitude (0, bs) < 0.05f, juce::String (buf.getMagnitude (0, bs)));
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 120), 10);
    double em = 0;
    for (int i = 0; i < 20; ++i) { buf.clear(); proc.processBlock (buf, midi); midi.clear(); em += buf.getRMSLevel (0, 0, bs); }
    check ("MIDI note 36 plays track 1", em > 0.05, juce::String (em, 3));

    // internal play button while host is stopped
    juce::Array<juce::var> ops2; ops2.add (juce::Array<juce::var> { juce::var ("play"), juce::var (1) });
    auto* obj2 = new juce::DynamicObject(); obj2->setProperty ("ops", ops2);
    proc.pushOpsFromUi (juce::var (obj2));
    for (int i = 0; i < 100; ++i) { buf.clear(); proc.processBlock (buf, midi); }
    check ("internal clock runs when host stopped", proc.pos.running.load() == 1 && proc.pos.hostRunning.load() == 0,
           "running=" + juce::String (proc.pos.running.load()) + " host=" + juce::String (proc.pos.hostRunning.load()) + " step0=" + juce::String (proc.pos.steps[0].load()));

    // v67: DICE is gone: its settings (as a v66 project had them: on, t1 -> track 1 on TRIG, x1 -> its NOTE) do nothing,
    // track 1 (MACRO, its steps cleared above) stays silent
    {
        juce::Array<juce::var> o;
        auto g = [&] (int i, double v) { o.add (juce::Array<juce::var> { "g", i, v }); };
        g (sk::G_DON, 1.0); g (sk::G_DR0, 1.5 / 9.0); g (sk::G_DR0 + 1, 0.5 / sk::NUM_DICE_DEST);
        g (sk::G_DR0 + 9, 1.5 / 9.0); g (sk::G_DR0 + 10, 1.5 / sk::NUM_DICE_DEST);
        o.add (juce::Array<juce::var> { "p", 0, sk::P_MACH, sk::machineVal (sk::M_MACRO) });
        o.add (juce::Array<juce::var> { "p", 0, sk::P_A, 13.5 / 24.0 });
        auto* ob = new juce::DynamicObject(); ob->setProperty ("ops", o);
        proc.pushOpsFromUi (juce::var (ob));
        const int h0 = proc.pos.hits[0].load();
        bool finite = true;
        for (int i = 0; i < 48000 * 3 / bs; ++i)
        {
            buf.clear(); proc.processBlock (buf, midi);
            for (int k = 0; k < bs; ++k) if (! std::isfinite (buf.getSample (0, k))) finite = false;
        }
        const int dh = proc.pos.hits[0].load() - h0;
        check ("v67: DICE gone (a project's DICE settings play nothing on a track without steps)", dh == 0 && finite, "hits=" + juce::String (dh));
    }

    // GRAIN (Beads) and OTT on the master chain, GRAIN clocked by the transport (VINT), its LFO on DENS
    {
        juce::Array<juce::var> o;
        auto g = [&] (int i, double v) { o.add (juce::Array<juce::var> { "g", i, v }); };
        g (sk::G_GON, 1.0); g (sk::G_GMIX, 0.6); g (sk::G_GMED, 1.5 / sk::GRAIN_MEDIA); g (sk::G_GSYN, 1.0); g (sk::G_GDEN, 0.1);
        g (sk::G_GLWV, 24.5 / sk::WLFO_WAVES); g (sk::G_GA0 + 4, 1.0); g (sk::G_OON, 1.0);
        auto* ob = new juce::DynamicObject(); ob->setProperty ("ops", o);
        proc.pushOpsFromUi (juce::var (ob));
        double e = 0; bool finite = true;
        for (int i = 0; i < 48000 * 3 / bs; ++i)
        {
            buf.clear(); proc.processBlock (buf, midi);
            for (int k = 0; k < bs; ++k) { const float v = buf.getSample (1, k); if (! std::isfinite (v)) finite = false; e += v * v; }
        }
        check ("GRAIN + OTT on the master chain (VINT, transport clock, LFO)", finite && e > 1e-3, juce::String (10 * std::log10 (e / (48000.0 * 3) + 1e-12), 1) + " dB");
    }

    // FX: track 8 a VERB on every track before it (SUM), the page showing it: the engine says what it takes, its
    // levels come through (the peaks since the page last read them); its MORE side as a project before v16 left it
    {
        juce::Array<juce::var> o;
        auto p = [&] (int t, int i, double v) { o.add (juce::Array<juce::var> { "p", t, i, v }); };
        p (7, sk::P_MACH, sk::machineVal (sk::M_FXVERB)); p (7, sk::P_VOL, 0.8); p (7, sk::P_A, 1.5 / 10.0); p (7, sk::P_E, 0.5);
        p (7, sk::P_DEC, 1.0); p (7, sk::P_B, 0.6); p (7, sk::P_C, 0.6); p (7, sk::P_XE, 0.25);
        o.add (juce::Array<juce::var> { juce::var ("fxm"), juce::var (7) });
        auto* ob = new juce::DynamicObject(); ob->setProperty ("ops", o);
        proc.pushOpsFromUi (juce::var (ob));
        double e = 0; bool finite = true; float pkIn = 0, pkOut = 0;
        for (int i = 0; i < 48000 * 2 / bs; ++i)
        {
            buf.clear(); proc.processBlock (buf, midi);
            for (int k = 0; k < bs; ++k) { const float v = buf.getSample (0, k); if (! std::isfinite (v)) finite = false; e += v * v; }
            if (i % 8 == 7) { pkIn = std::max (pkIn, proc.meters.fxm[4].exchange (0.0f)); pkOut = std::max (pkOut, proc.meters.fxm[5].exchange (0.0f)); }
        }
        const int m = (int) proc.meters.fxm[0].load(), mask = (int) proc.meters.fxm[1].load();
        check ("FX: VERB on T8 takes T1..T7 (SUM), its levels to the page", finite && e > 1e-3 && m == sk::M_FXVERB && mask == 127 && pkIn > 0.01f && pkOut > 0.01f,
               "machine " + juce::String (m) + " mask " + juce::String (mask) + " in " + juce::String (pkIn, 3) + " out " + juce::String (pkOut, 3));
    }

    // BREAK: a loop into slot 4 (as the page sends it: mono, 32 kHz, 16-bit, one bar at 120), read over the next blocks,
    // kept in the project (a new instance has it at once), cleared
    {
        const int n = 64000;
        juce::MemoryBlock smp ((size_t) n * 2, true);
        auto* x = static_cast<short*> (smp.getData());
        for (int i = 0; i < n; ++i) x[i] = (short) (9000.0 * std::sin (i * 0.05) * std::exp (-(i % 4000) / 900.0));
        proc.loadBreakSlot (3, 1, "TEST LOOP", smp);
        for (int i = 0; i < 400; ++i) { buf.clear(); proc.processBlock (buf, midi); }
        const float st = proc.meters.brkSlots[3 * sk::BRK_SLOT_INFO].load(), bpm = proc.meters.brkSlots[3 * sk::BRK_SLOT_INFO + 1].load();
        check ("BREAK: a loop into a slot, read (ready, 120 BPM, its name)", st == 2.0f && std::abs (bpm - 120.0f) < 0.1f && proc.getBreakName (3) == "TEST LOOP",
               "state " + juce::String (st) + " bpm " + juce::String (bpm));
        juce::MemoryBlock saved; proc.getStateInformation (saved);
        ImpaktProcessor p2;
        p2.setRateAndBufferSizeDetails (48000.0, bs); p2.prepareToPlay (48000.0, bs);
        p2.setStateInformation (saved.getData(), (int) saved.getSize());
        buf.clear(); p2.processBlock (buf, midi);
        juce::MemoryBlock again; p2.getStateInformation (again);
        check ("BREAK: the slot kept in the project (read at once, the same project saved again)", p2.meters.brkSlots[3 * sk::BRK_SLOT_INFO].load() == 2.0f
               && p2.getBreakName (3) == "TEST LOOP" && again == saved, juce::String ((int) saved.getSize()) + " bytes");
        auto* ob = new juce::DynamicObject(); ob->setProperty ("ops", juce::JSON::parse ("[[\"bclr\", 3]]"));   // (as the page sends it)
        p2.pushOpsFromUi (juce::var (ob));
        buf.clear(); p2.processBlock (buf, midi);
        check ("BREAK: a slot cleared", p2.meters.brkSlots[3 * sk::BRK_SLOT_INFO].load() == 0.0f && p2.getBreakName (3).isEmpty(),
               "state " + juce::String (p2.meters.brkSlots[3 * sk::BRK_SLOT_INFO].load()) + " name '" + p2.getBreakName (3) + "' ver " + juce::String (p2.meters.brkVer.load()));
    }

    // CUBE (v74): a bank into place 3 (as the page sends it: 64 waves of 256 samples, 16-bit), its versions made over the
    // next blocks, kept in the project (a new instance has it made at once), cleared
    {
        juce::MemoryBlock smp ((size_t) sk::CUBE_BANK_N * 2, true);
        auto* x = static_cast<short*> (smp.getData());
        for (int w = 0; w < 64; ++w) for (int i = 0; i < 256; ++i) x[w * 256 + i] = (short) (12000.0 * std::sin (2 * M_PI * i / 256.0 * (1 + w % 8)));
        proc.loadCubeBank (2, "TEST CUBE", smp);
        const int r0 = proc.cubeReady (2);
        for (int i = 0; i < 40; ++i) { buf.clear(); proc.processBlock (buf, midi); }
        check ("CUBE: a bank loaded, its versions made as the engine runs (being made, then ready; its name)", r0 == 1 && proc.cubeReady (2) == 2 && proc.getCubeName (2) == "TEST CUBE",
               "state " + juce::String (r0) + " -> " + juce::String (proc.cubeReady (2)));
        juce::MemoryBlock saved; proc.getStateInformation (saved);
        ImpaktProcessor p2;
        p2.setRateAndBufferSizeDetails (48000.0, bs); p2.prepareToPlay (48000.0, bs);
        p2.setStateInformation (saved.getData(), (int) saved.getSize());
        juce::MemoryBlock again; p2.getStateInformation (again);
        check ("CUBE: the bank kept in the project (made at once, the same project saved again)", p2.cubeReady (2) == 2 && p2.getCubeName (2) == "TEST CUBE"
               && again == saved && saved.getSize() > (size_t) sk::CUBE_BANK_N * 2, juce::String ((int) saved.getSize()) + " bytes");
        auto* ob = new juce::DynamicObject(); ob->setProperty ("ops", juce::JSON::parse ("[[\"cube\", 0], [\"cclr\", 2]]"));
        p2.pushOpsFromUi (juce::var (ob));
        buf.clear(); p2.processBlock (buf, midi);
        check ("CUBE: a bank cleared (FACT's again), the page's meters", p2.cubeReady (2) == 0 && p2.getCubeName (2).isEmpty() && p2.meters.cubeT.load() == 0
               && p2.meters.cube[7].load() == 0.0f, "state " + juce::String (p2.cubeReady (2)) + " mask " + juce::String (p2.meters.cube[7].load()));
    }

    // patterns (v18): a project with A02 in its slot (sparse steps, 32 steps on track 1, a trig on step 21) and a chain
    // A01 A02: the host plays, the engine plays A01 then A02 at the end of A01 (AUTO: 16 16ths here), then A01 again
    {
        juce::var pj = juce::JSON::parse (json);
        auto* o = pj.getDynamicObject();
        o->setProperty ("v", 18);
        juce::Array<juce::var> pats, tr2;
        const auto* trs = o->getProperty ("tracks").getArray();
        for (int t = 0; t < sk::NUM_TRACKS; ++t)
        {
            auto* k = new juce::DynamicObject();
            k->setProperty ("p", (*trs)[t].getProperty ("p", juce::var()));
            k->setProperty ("len", t == 0 ? 32 : 16); k->setProperty ("spd", 4); k->setProperty ("q", 1);
            auto* stp = new juce::DynamicObject();
            if (t == 0) stp->setProperty ("20", juce::JSON::parse ("{\"on\":1,\"l\":{}}"));
            k->setProperty ("st", juce::var (stp));
            tr2.add (juce::var (k));
        }
        auto* a02 = new juce::DynamicObject(); a02->setProperty ("chg", 0); a02->setProperty ("tr", tr2);
        for (int i = 0; i < sk::NUM_PATS; ++i) pats.add (i == 1 ? juce::var (a02) : juce::var());
        o->setProperty ("pats", pats); o->setProperty ("pat", 0); o->setProperty ("chain", juce::Array<juce::var> { 0, 1 });
        juce::MemoryBlock pm; { juce::MemoryOutputStream os (pm, false); os.writeString ("IMPAKT1"); os.writeString (juce::JSON::toString (pj, true)); }
        ImpaktProcessor p3; FakeHead h3; p3.setPlayHead (&h3);
        p3.setRateAndBufferSizeDetails (48000.0, bs); p3.prepareToPlay (48000.0, bs);
        p3.setStateInformation (pm.getData(), (int) pm.getSize());
        juce::String seq; int last = -1; bool finite = true; float chainLen = 0, next0 = -2;
        for (int i = 0; i < 48000 * 6 / bs; ++i)
        {
            buf.clear(); p3.processBlock (buf, midi);
            h3.ppq += bs * h3.bpm / 60.0 / 48000.0;
            for (int k = 0; k < bs; ++k) if (! std::isfinite (buf.getSample (0, k))) finite = false;
            const int c = (int) p3.pos.pat[0].load();
            if (i == 4) { chainLen = p3.pos.pat[2].load(); next0 = p3.pos.pat[1].load(); }
            if (c != last) { seq << (seq.isEmpty() ? "" : ",") << c; last = c; }
        }
        check ("patterns: a project's slots and chain loaded, A01 A02 A01 played in turn (host transport)", finite && seq.startsWith ("0,1,0") && chainLen == 2.0f && next0 == 1.0f,
               "seq " + seq + " chain " + juce::String (chainLen) + " next " + juce::String (next0));
    }

    // v21 (v61), v22 (v62): a v20 project, MODAL on track 1, WT·GRL on track 2, HALO on track 3 (v20's numbers 9, 31,
    // 33), their MORE page up: both on ALLOY at its defaults (A..F and the sides), HALO on its new number, its sides at rest
    {
        juce::var pj = juce::JSON::parse (json);
        auto* o = pj.getDynamicObject();
        o->setProperty ("v", 20);
        auto* trs = o->getProperty ("tracks").getArray();
        const double ms[3] = { 9, 31, 33 };
        for (int t = 0; t < 3; ++t)
        {
            auto* p = (*trs)[t].getProperty ("p", juce::var()).getArray();
            p->resize (161);
            p->set (sk::P_MACH, (ms[t] + 0.5) / 64.0);
            p->set (sk::P_A, 0.97);
            for (int q = 155; q < 161; ++q) p->set (q, 0.6);
        }
        juce::MemoryBlock pm; { juce::MemoryOutputStream os (pm, false); os.writeString ("IMPAKT1"); os.writeString (juce::JSON::toString (pj, true)); }
        ImpaktProcessor p6; FakeHead h6; h6.playing = false; p6.setPlayHead (&h6);
        p6.setRateAndBufferSizeDetails (48000.0, bs); p6.prepareToPlay (48000.0, bs);
        p6.setStateInformation (pm.getData(), (int) pm.getSize());
        buf.clear(); p6.processBlock (buf, midi);
        float af[6], sd[sk::NUM_SIDES];
        sk::machineAFDefaults (sk::M_ALLOY, af); sk::machineSideDefaults (sk::M_ALLOY, sd);
        bool ok = sk::machineOf (p6.getTrackParam (0, sk::P_MACH)) == sk::M_ALLOY && sk::machineOf (p6.getTrackParam (1, sk::P_MACH)) == sk::M_ALLOY
               && sk::machineOf (p6.getTrackParam (2, sk::P_MACH)) == sk::M_HALO && std::abs (p6.getTrackParam (2, sk::P_A) - 0.97f) < 1e-6f;
        for (int i = 0; i < 6; ++i) ok = ok && std::abs (p6.getTrackParam (0, sk::P_A + i) - af[i]) < 1e-6f && std::abs (p6.getTrackParam (1, sk::P_A + i) - af[i]) < 1e-6f;
        for (int i = 0; i < sk::NUM_SIDES; ++i) ok = ok && std::abs (p6.getTrackParam (0, sk::sideParam (i)) - sd[i]) < 1e-6f && std::abs (p6.getTrackParam (2, sk::sideParam (i))) < 1e-6f
                                          && std::abs (p6.getTrackParam (1, sk::sideParam (i)) - sd[i]) < 1e-6f;
        check ("v21: a v20 project's MODAL and WT·GRL load as ALLOY at its defaults, HALO on its new number, the MORE page gone", ok,
               "machines " + juce::String (sk::machineOf (p6.getTrackParam (0, sk::P_MACH))) + " " + juce::String (sk::machineOf (p6.getTrackParam (1, sk::P_MACH))) + " "
               + juce::String (sk::machineOf (p6.getTrackParam (2, sk::P_MACH))));
    }
    // v22 (v62): a v21 project, MATTER on track 1 and LATEX on track 2 (v21's 37, 38), their sides up, a step of track 3
    // (HALO, 28) locked onto LATEX with a side lock, a step of track 1 with an A lock: both tracks on ALLOY at its defaults,
    // the step of track 3 on ALLOY with its sides as locks, track 1's A lock gone
    {
        juce::var pj = juce::JSON::parse (json);
        auto* o = pj.getDynamicObject();
        o->setProperty ("v", 21);
        auto* trs = o->getProperty ("tracks").getArray();
        const double ms[3] = { 37, 38, 28 };
        for (int t = 0; t < 3; ++t)
        {
            auto* p = (*trs)[t].getProperty ("p", juce::var()).getArray();
            p->set (sk::P_MACH, (ms[t] + 0.5) / 64.0);
            p->set (sk::P_A, 0.97);
            for (int q = sk::P_S1; q <= sk::P_S21; ++q) p->set (q, 0.6);
        }
        for (int t : { 0, 2 })
        {
            auto* steps = (*trs)[t].getProperty ("steps", juce::var()).getArray();
            juce::DynamicObject::Ptr l = new juce::DynamicObject();
            if (t == 2) { l->setProperty ("0", (38 + 0.5) / 64.0); l->setProperty (juce::String (sk::P_S5), 0.9); }
            else l->setProperty (juce::String (sk::P_A), 0.05);
            juce::DynamicObject::Ptr stp = new juce::DynamicObject(); stp->setProperty ("on", 1); stp->setProperty ("l", juce::var (l.get()));
            steps->set (5, juce::var (stp.get()));
        }
        juce::MemoryBlock pm; { juce::MemoryOutputStream os (pm, false); os.writeString ("IMPAKT1"); os.writeString (juce::JSON::toString (pj, true)); }
        ImpaktProcessor p7; FakeHead h7; h7.playing = false; p7.setPlayHead (&h7);
        p7.setRateAndBufferSizeDetails (48000.0, bs); p7.prepareToPlay (48000.0, bs);
        p7.setStateInformation (pm.getData(), (int) pm.getSize());
        buf.clear(); p7.processBlock (buf, midi);
        float af[6], sd[sk::NUM_SIDES];
        sk::machineAFDefaults (sk::M_ALLOY, af); sk::machineSideDefaults (sk::M_ALLOY, sd);
        bool ok = sk::machineOf (p7.getTrackParam (0, sk::P_MACH)) == sk::M_ALLOY && sk::machineOf (p7.getTrackParam (1, sk::P_MACH)) == sk::M_ALLOY
               && sk::machineOf (p7.getTrackParam (2, sk::P_MACH)) == sk::M_HALO && std::abs (p7.getTrackParam (2, sk::P_A) - 0.97f) < 1e-6f;
        for (int i = 0; i < 6; ++i) ok = ok && std::abs (p7.getTrackParam (0, sk::P_A + i) - af[i]) < 1e-6f && std::abs (p7.getTrackParam (1, sk::P_A + i) - af[i]) < 1e-6f;
        for (int i = 0; i < sk::NUM_SIDES; ++i) ok = ok && std::abs (p7.getTrackParam (0, sk::sideParam (i)) - sd[i]) < 1e-6f && std::abs (p7.getTrackParam (1, sk::sideParam (i)) - sd[i]) < 1e-6f;
        const float l3m = p7.getTrackLock (2, 5, sk::P_MACH), l3s = p7.getTrackLock (2, 5, sk::P_S5), l3a = p7.getTrackLock (2, 5, sk::P_A), l1a = p7.getTrackLock (0, 5, sk::P_A);
        ok = ok && l3m >= 0.0f && sk::machineOf (l3m) == sk::M_ALLOY && std::abs (l3s - sd[4]) < 1e-6f && std::abs (l3a - af[0]) < 1e-6f && l1a < 0.0f;
        check ("v22: a v21 project's MATTER and LATEX load as ALLOY at its defaults; a step locked onto LATEX on ALLOY with its sides; their A locks gone", ok,
               "machines " + juce::String (sk::machineOf (p7.getTrackParam (0, sk::P_MACH))) + " " + juce::String (sk::machineOf (p7.getTrackParam (1, sk::P_MACH)))
               + " locks " + juce::String (l3m) + " " + juce::String (l3s) + " " + juce::String (l1a));
    }
    // v76: a v75 project (v26, no "wst") with track 1 on SPHERE (42nd machine, WEST's number now: A and its sides set,
    // an A lock) and a step of track 3 locked onto it (with a side lock): track 1 on WEST at its defaults, its A lock
    // gone, the step on WEST with WEST's side in its lock; the same project marked "wst" (saved by v76) as it is
    {
        auto load76 = [&] (bool marked, ImpaktProcessor& pp) {
            juce::var pj = juce::JSON::parse (json);
            auto* o = pj.getDynamicObject();
            o->setProperty ("v", 26);
            if (marked) o->setProperty ("wst", 1); else o->removeProperty ("wst");
            auto* trs = o->getProperty ("tracks").getArray();
            auto* p = (*trs)[0].getProperty ("p", juce::var()).getArray();
            p->set (sk::P_MACH, (41 + 0.5) / 64.0); p->set (sk::P_A, 0.97); p->set (sk::P_DEC, 0.9);
            for (int q = sk::P_S1; q <= sk::P_S35; ++q) p->set (q, 0.6);
            for (int t : { 0, 2 })
            {
                auto* steps = (*trs)[t].getProperty ("steps", juce::var()).getArray();
                juce::DynamicObject::Ptr l = new juce::DynamicObject();
                if (t == 2) { l->setProperty ("0", (41 + 0.5) / 64.0); l->setProperty (juce::String (sk::P_S5), 0.9); }
                else l->setProperty (juce::String (sk::P_A), 0.05);
                juce::DynamicObject::Ptr stp = new juce::DynamicObject(); stp->setProperty ("on", 1); stp->setProperty ("l", juce::var (l.get()));
                steps->set (5, juce::var (stp.get()));
            }
            juce::MemoryBlock pm; { juce::MemoryOutputStream os (pm, false); os.writeString ("IMPAKT1"); os.writeString (juce::JSON::toString (pj, true)); }
            pp.setRateAndBufferSizeDetails (48000.0, bs); pp.prepareToPlay (48000.0, bs);
            pp.setStateInformation (pm.getData(), (int) pm.getSize());
            buf.clear(); pp.processBlock (buf, midi);
        };
        ImpaktProcessor p8; FakeHead h8; h8.playing = false; p8.setPlayHead (&h8); load76 (false, p8);
        ImpaktProcessor p9; FakeHead h9; h9.playing = false; p9.setPlayHead (&h9); load76 (true, p9);
        float af[6], sd[sk::NUM_SIDES];
        sk::machineAFDefaults (sk::M_WEST, af); sk::machineSideDefaults (sk::M_WEST, sd);
        bool ok = sk::machineOf (p8.getTrackParam (0, sk::P_MACH)) == sk::M_WEST && std::abs (p8.getTrackParam (0, sk::P_DEC) - 0.4f) < 1e-6f;
        for (int i = 0; i < 6; ++i) ok = ok && std::abs (p8.getTrackParam (0, sk::P_A + i) - af[i]) < 1e-6f;
        for (int i = 0; i < sk::NUM_SIDES; ++i) ok = ok && std::abs (p8.getTrackParam (0, sk::sideParam (i)) - sd[i]) < 1e-6f;
        const float la = p8.getTrackLock (0, 5, sk::P_A), ls = p8.getTrackLock (2, 5, sk::P_S5), lm = p8.getTrackLock (2, 5, sk::P_MACH);
        ok = ok && la < 0.0f && std::abs (ls - sd[4]) < 1e-6f && lm >= 0.0f && sk::machineOf (lm) == sk::M_WEST;
        const bool kept = std::abs (p9.getTrackParam (0, sk::P_A) - 0.97f) < 1e-6f && std::abs (p9.getTrackParam (0, sk::P_S1 + 4) - 0.6f) < 1e-6f
                       && std::abs (p9.getTrackLock (0, 5, sk::P_A) - 0.05f) < 1e-6f && std::abs (p9.getTrackLock (2, 5, sk::P_S5) - 0.9f) < 1e-6f;
        check ("v76: a v75 project's SPHERE loads as WEST at its defaults (its locks gone, a step locked onto it with WEST's sides); a v76 one as it is", ok && kept,
               "A " + juce::String (p8.getTrackParam (0, sk::P_A)) + " lock " + juce::String (la) + " side lock " + juce::String (ls) + " kept " + juce::String ((int) kept));
    }

    // v23 (v63): a v22 project (its tracks 176 parameters), ALLOY on track 1 with its 21 sides of v62, a step of track 1
    // with two side locks, a step of track 3 (HALO) locked onto ALLOY with two: track 1's sides onto the five pages
    // (sk::alloySide23), its locks moved, the step of track 3 on ALLOY with its locks moved and the new ones at rest
    {
        juce::var pj = juce::JSON::parse (json);
        auto* o = pj.getDynamicObject();
        o->setProperty ("v", 22);
        auto* trs = o->getProperty ("tracks").getArray();
        float old[21];
        for (int t = 0; t < sk::NUM_TRACKS; ++t)
        {
            auto* p = (*trs)[t].getProperty ("p", juce::var()).getArray();
            p->resize (176);
            if (t == 0) { p->set (sk::P_MACH, (sk::M_ALLOY + 0.5) / 64.0); for (int q = 0; q < 21; ++q) { old[q] = 0.05f * (float) (q % 13) + 0.1f; p->set (sk::P_S1 + q, old[q]); } }
            if (t == 2) p->set (sk::P_MACH, (sk::M_HALO + 0.5) / 64.0);
        }
        for (int t : { 0, 2 })
        {
            auto* steps = (*trs)[t].getProperty ("steps", juce::var()).getArray();
            juce::DynamicObject::Ptr l = new juce::DynamicObject();
            if (t == 2) { l->setProperty ("0", (sk::M_ALLOY + 0.5) / 64.0); l->setProperty (juce::String (sk::P_S7), 0.4); l->setProperty (juce::String (sk::P_S20), 0.9); }
            else { l->setProperty (juce::String (sk::P_S5), 0.2); l->setProperty (juce::String (sk::P_S1), 0.3); }
            juce::DynamicObject::Ptr stp = new juce::DynamicObject(); stp->setProperty ("on", 1); stp->setProperty ("l", juce::var (l.get()));
            steps->set (6, juce::var (stp.get()));
        }
        juce::MemoryBlock pm; { juce::MemoryOutputStream os (pm, false); os.writeString ("IMPAKT1"); os.writeString (juce::JSON::toString (pj, true)); }
        ImpaktProcessor p8; FakeHead h8; h8.playing = false; p8.setPlayHead (&h8);
        p8.setRateAndBufferSizeDetails (48000.0, bs); p8.prepareToPlay (48000.0, bs);
        p8.setStateInformation (pm.getData(), (int) pm.getSize());
        buf.clear(); p8.processBlock (buf, midi);
        float nw[sk::NUM_SIDES]; sk::alloySide23 (old, nw);
        bool ok = sk::machineOf (p8.getTrackParam (0, sk::P_MACH)) == sk::M_ALLOY;
        juce::String bad;
        for (int i = 0; i < sk::NUM_SIDES; ++i)
            if (std::abs (p8.getTrackParam (0, sk::sideParam (i)) - nw[i]) >= 1e-6f) { ok = false; bad << " q" << i << "=" << p8.getTrackParam (0, sk::sideParam (i)) << "/" << nw[i]; }
        const float a1 = p8.getTrackLock (0, 6, sk::PA_PENV), a2 = p8.getTrackLock (0, 6, sk::P_S1), a3 = p8.getTrackLock (0, 6, sk::P_S5);
        const float b1 = p8.getTrackLock (2, 6, sk::PA_BNCE), b2 = p8.getTrackLock (2, 6, sk::PA_RATE), b3 = p8.getTrackLock (2, 6, sk::PA_MENV), b4 = p8.getTrackLock (2, 6, sk::PA_NCOL);
        ok = ok && std::abs (a1 - 0.3f) < 1e-6f && std::abs (a2 - 0.3f) < 1e-6f && a3 < 0.0f
                && std::abs (b1 - 0.7f) < 1e-6f && std::abs (b2 - 0.9f) < 1e-6f && std::abs (b3 - 0.5f) < 1e-6f && std::abs (b4 - 0.7625f) < 1e-6f;
        check ("v23: a v22 project's ALLOY, its sides onto the five pages; its locks moved; a step locked onto ALLOY with the new ones at rest", ok,
               "locks " + juce::String (a1) + " " + juce::String (a2) + " " + juce::String (a3) + " " + juce::String (b1) + " " + juce::String (b2) + " " + juce::String (b3) + " " + juce::String (b4) + (bad.isEmpty() ? "" : " sides" + bad));
    }

    // v25 (v65): a v24 project (its tracks 193 parameters), track 1's LFO 1 on S&H·T and LFO 2 on EXP (v64's grid of 12), a
    // step of track 1 locked onto EXP·T: the same waves on the grid of 16, the lock too; RDEC off, SLW and LEN at rest. And a
    // v20 project: the parameters after the side places at the engine's defaults (RPAN centred, not at 0)
    {
        auto mid = [] (int i, int n) { return (i + 0.5) / n; };
        juce::var pj = juce::JSON::parse (json);
        auto* o = pj.getDynamicObject();
        o->setProperty ("v", 24);
        auto* trs = o->getProperty ("tracks").getArray();
        for (int t = 0; t < sk::NUM_TRACKS; ++t)
        {
            auto* p = (*trs)[t].getProperty ("p", juce::var()).getArray();
            p->resize (193);
            p->set (sk::P_LWAV, t == 0 ? mid (10, 12) : mid (0, 12)); p->set (sk::P_L2WV, t == 0 ? mid (5, 12) : mid (0, 12));
        }
        {
            auto* steps = (*trs)[0].getProperty ("steps", juce::var()).getArray();
            juce::DynamicObject::Ptr l = new juce::DynamicObject(); l->setProperty (juce::String (sk::P_LWAV), mid (11, 12));
            juce::DynamicObject::Ptr stp = new juce::DynamicObject(); stp->setProperty ("on", 1); stp->setProperty ("l", juce::var (l.get()));
            steps->set (5, juce::var (stp.get()));
        }
        juce::MemoryBlock pm; { juce::MemoryOutputStream os (pm, false); os.writeString ("IMPAKT1"); os.writeString (juce::JSON::toString (pj, true)); }
        ImpaktProcessor p9; FakeHead h9; h9.playing = false; p9.setPlayHead (&h9);
        p9.setRateAndBufferSizeDetails (48000.0, bs); p9.prepareToPlay (48000.0, bs);
        p9.setStateInformation (pm.getData(), (int) pm.getSize());
        buf.clear(); p9.processBlock (buf, midi);
        const float w1 = p9.getTrackParam (0, sk::P_LWAV), w2 = p9.getTrackParam (0, sk::P_L2WV), lk = p9.getTrackLock (0, 5, sk::P_LWAV);
        bool ok = std::abs (w1 - (float) mid (12, 16)) < 1e-6f && std::abs (w2 - (float) mid (5, 16)) < 1e-6f && std::abs (lk - (float) mid (13, 16)) < 1e-6f
               && std::abs (p9.getTrackParam (1, sk::P_LWAV) - (float) mid (0, 16)) < 1e-6f;
        for (int t = 0; t < sk::NUM_TRACKS; ++t)
            ok = ok && p9.getTrackParam (t, sk::P_RDEC) == 0.5f && p9.getTrackParam (t, sk::P_L1SL) == 0.5f && std::abs (p9.getTrackParam (t, sk::P_L2LN) - 0.65f) < 1e-6f;
        // (v20)
        o->setProperty ("v", 20);
        for (int t = 0; t < sk::NUM_TRACKS; ++t) (*trs)[t].getProperty ("p", juce::var()).getArray()->resize (161);
        juce::MemoryBlock pm2; { juce::MemoryOutputStream os (pm2, false); os.writeString ("IMPAKT1"); os.writeString (juce::JSON::toString (pj, true)); }
        ImpaktProcessor p10; FakeHead h10; h10.playing = false; p10.setPlayHead (&h10);
        p10.setRateAndBufferSizeDetails (48000.0, bs); p10.prepareToPlay (48000.0, bs);
        p10.setStateInformation (pm2.getData(), (int) pm2.getSize());
        buf.clear(); p10.processBlock (buf, midi);
        bool ok20 = true;
        for (int t = 0; t < sk::NUM_TRACKS; ++t) ok20 = ok20 && p10.getTrackParam (t, sk::P_RPAN) == 0.5f && p10.getTrackParam (t, sk::P_SMON) == 0.5f && p10.getTrackParam (t, sk::P_RDEC) == 0.5f;
        check ("v25: a v24 project's LFO waves on the grid of 16 (S&H·T, EXP, an EXP·T lock); RDEC off, SLW, LEN at rest; a v20 one with RPAN centred", ok && ok20,
               "waves " + juce::String (w1 * 16 - 0.5f) + " " + juce::String (w2 * 16 - 0.5f) + " lock " + juce::String (lk * 16 - 0.5f) + " rpan(v20) " + juce::String (p10.getTrackParam (0, sk::P_RPAN)));
    }

    // v68 (projects v26): a v25 project (201 parameters a track, 158 globals) loads with no CHORUS nor PHASER send, the new
    // effects and the DELAY's model at the engine's defaults; a v26 one with a FUZZ track, a CHORUS and a PHASER send and
    // the 1982 delay plays them (finite, louder than without)
    {
        juce::var pj = juce::JSON::parse (json);
        auto* o = pj.getDynamicObject();
        o->setProperty ("v", 25);
        auto* trs = o->getProperty ("tracks").getArray();
        for (int t = 0; t < sk::NUM_TRACKS; ++t) (*trs)[t].getProperty ("p", juce::var()).getArray()->resize (201);
        o->getProperty ("g").getArray()->resize (158);
        juce::MemoryBlock pm; { juce::MemoryOutputStream os (pm, false); os.writeString ("IMPAKT1"); os.writeString (juce::JSON::toString (pj, true)); }
        ImpaktProcessor q; FakeHead hq; hq.playing = false; q.setPlayHead (&hq);
        q.setRateAndBufferSizeDetails (48000.0, bs); q.prepareToPlay (48000.0, bs);
        q.setStateInformation (pm.getData(), (int) pm.getSize());
        buf.clear(); q.processBlock (buf, midi);
        bool ok = true;
        for (int t = 0; t < sk::NUM_TRACKS; ++t) ok = ok && q.getTrackParam (t, sk::P_CHO) == 0.0f && q.getTrackParam (t, sk::P_PHS) == 0.0f;
        ok = ok && std::abs (q.getGlobal (sk::G_CHMD) - 1.5f / 4.0f) < 1e-6f && q.getGlobal (sk::G_CHON) == 1.0f && std::abs (q.getGlobal (sk::G_PLVL) - 0.8f) < 1e-6f
                && q.getGlobal (sk::G_DMDL) == 0.25f && q.getGlobal (sk::G_DVDP) == 0.0f && q.getGlobal (sk::G_DVEN) == 0.5f;
        // a v26 one: T3 on FUZZ (SUM: T1, T2 through it), T1 sends to the CHORUS and the PHASER, the delay on 1982
        o->setProperty ("v", 26);
        for (int t = 0; t < sk::NUM_TRACKS; ++t) (*trs)[t].getProperty ("p", juce::var()).getArray()->resize (sk::NUM_PARAMS);
        auto& g = *o->getProperty ("g").getArray();
        g.resize (sk::NUM_GLOBALS);
        const float gd[24] = { 0.375f, 0.75f, 0.8f, 1, 0.35f, 0, 0.45f, 0, 0.7f, 0.3f, 0.7f, 0.3f, 0.25f, 0.75f, 0.5f / 3.0f, 0.8f, 1, 0.75f, 0.25f, 0.2f, 0.4f, 0, 0.5f, 0 };
        for (int k = 0; k < 24; ++k) g.set (sk::G_CHMD + k, gd[k]);
        auto* p0 = (*trs)[0].getProperty ("p", juce::var()).getArray();
        p0->set (sk::P_CHO, 0.7); p0->set (sk::P_PHS, 0.6); p0->set (sk::P_DLY, 0.4);
        auto* p2 = (*trs)[2].getProperty ("p", juce::var()).getArray();
        p2->set (sk::P_MACH, sk::machineVal (sk::M_FXFUZZ)); p2->set (sk::P_A, (1 + 0.5) / 10.0); p2->set (sk::P_B, 0.9); p2->set (sk::P_E, 1.0); p2->set (sk::P_DEC, 1.0); p2->set (sk::P_VOL, 0.8);
        juce::MemoryBlock pm2; { juce::MemoryOutputStream os (pm2, false); os.writeString ("IMPAKT1"); os.writeString (juce::JSON::toString (pj, true)); }
        ImpaktProcessor q2; FakeHead h2; h2.playing = true; q2.setPlayHead (&h2);
        q2.setRateAndBufferSizeDetails (48000.0, bs); q2.prepareToPlay (48000.0, bs);
        q2.setStateInformation (pm2.getData(), (int) pm2.getSize());
        double e = 0; bool fin = true;
        for (int b = 0; b < 48000 * 3 / bs; ++b)
        {
            h2.ppq = (double) b * bs / 48000.0 * h2.bpm / 60.0;
            buf.clear(); q2.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) { const float v = buf.getSample (c, i); if (! std::isfinite (v)) fin = false; e += (double) v * v; }
        }
        const bool ok2 = fin && e > 1.0 && sk::machineOf (q2.getTrackParam (2, sk::P_MACH)) == sk::M_FXFUZZ && std::abs (q2.getTrackParam (0, sk::P_CHO) - 0.7f) < 1e-6f
                         && q2.getGlobal (sk::G_DMDL) == 0.75f;
        check ("v68: a v25 project, no CHORUS nor PHASER send, the new effects at their defaults; a v26 one with FUZZ, CHO, PHS and the 1982 delay plays", ok && ok2,
               "energy " + juce::String (e, 1) + " machine " + juce::String (sk::machineOf (q2.getTrackParam (2, sk::P_MACH))));
    }

    // DEC turned to its top on every track (CONTROL ALL), GATE off: the notes hold until their next trig; the host
    // stops, they let go (before: they went on, only INIT stopped them)
    {
        ImpaktProcessor p4; FakeHead h4; p4.setPlayHead (&h4);
        p4.setRateAndBufferSizeDetails (48000.0, bs); p4.prepareToPlay (48000.0, bs);
        p4.setStateInformation (mb.getData(), (int) mb.getSize());
        juce::Array<juce::var> o;
        for (int t = 0; t < sk::NUM_TRACKS; ++t) { o.add (juce::Array<juce::var> { "p", t, sk::P_DEC, 1.0 }); o.add (juce::Array<juce::var> { "p", t, sk::P_GATE, 0.0 }); }
        auto* ob = new juce::DynamicObject(); ob->setProperty ("ops", o);
        p4.pushOpsFromUi (juce::var (ob));
        auto blockRun = [&] (double secs, bool play, float& mag) {
            h4.playing = play; mag = 0.0f;
            for (int i = 0; i < (int) (48000 * secs) / bs; ++i)
            {
                buf.clear(); p4.processBlock (buf, midi);
                if (play) h4.ppq += bs * h4.bpm / 60.0 / 48000.0;
                mag = std::max (mag, buf.getMagnitude (0, bs));
            }
        };
        float playing = 0, tail = 0, late = 0;
        blockRun (3.0, true, playing); blockRun (2.0, false, tail); blockRun (0.5, false, late);
        check ("HOLD: a host stop lets go of the notes held until their next trig (peak 2 s after the stop)", late < 0.05f && playing > 0.1f,
               juce::String (late, 4) + " (playing " + juce::String (playing, 3) + ")");
    }

    // v19: a v18 project with a HALO on track 1 on WALK (the 13th of the old PROG list) plays WALK, the last of the new
    // list (the engine says so through the page's HALO screen)
    {
        juce::var pj = juce::JSON::parse (json);
        auto* o = pj.getDynamicObject();
        o->setProperty ("v", 18);
        auto* trs = o->getProperty ("tracks").getArray();
        auto* p = (*trs)[0].getProperty ("p", juce::var()).getArray();
        p->resize (119);
        p->set (sk::P_MACH, (33 + 0.5) / 64.0);      // (HALO on v20's numbers: 33)
        // (the other tracks on BD·VA, 0 on any numbers: the demo's machines, read on v20's numbers, would be others, an
        // FX machine among them taking the ribbon's track)
        for (int t = 1; t < sk::NUM_TRACKS; ++t) (*trs)[t].getProperty ("p", juce::var()).getArray()->set (sk::P_MACH, 0.5 / 64.0);
        p->set (sk::P_B, (12 + 0.5) / 13.0);
        p->set (sk::P_F, 0.75);
        juce::MemoryBlock pm; { juce::MemoryOutputStream os (pm, false); os.writeString ("IMPAKT1"); os.writeString (juce::JSON::toString (pj, true)); }
        ImpaktProcessor p5; FakeHead h5; h5.playing = false; p5.setPlayHead (&h5);
        p5.setRateAndBufferSizeDetails (48000.0, bs); p5.prepareToPlay (48000.0, bs);
        p5.setStateInformation (pm.getData(), (int) pm.getSize());
        auto* ob = new juce::DynamicObject(); ob->setProperty ("ops", juce::JSON::parse ("[[\"halo\", 0]]"));
        p5.pushOpsFromUi (juce::var (ob));
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);     // (the screen shows a sounding chord)
        for (int i = 0; i < 8; ++i) { buf.clear(); p5.processBlock (buf, midi); midi.clear(); }
        midi.addEvent (juce::MidiMessage::noteOff (1, 36), 0);
        buf.clear(); p5.processBlock (buf, midi); midi.clear();
        const float kind = p5.meters.halo[0].load(), prog = p5.meters.halo[9].load();
        check ("v19: a v18 HALO on WALK still plays WALK (the PROG list grown to 28)", kind == 1.0f && prog == (float) (sk::HALO_PROGS - 1),
               "kind " + juce::String (kind) + " prog " + juce::String (prog));

        // CHOIR's ribbon, as the page sends it: it sings while touched, follows the finger, lets go
        juce::Array<juce::var> o2;
        o2.add (juce::Array<juce::var> { "p", 1, sk::P_MACH, sk::machineVal (sk::M_CHOIR) });
        o2.add (juce::Array<juce::var> { "p", 1, sk::P_DEC, 0.2 });
        o2.add (juce::Array<juce::var> { "p", 1, sk::P_VOL, 0.8 });
        for (int i : { (int) sk::P_DLY, (int) sk::P_REV, (int) sk::P_CSPC, (int) sk::P_IMIX, (int) sk::P_IGAT }) o2.add (juce::Array<juce::var> { "p", 1, i, 0.0 });
        o2.add (juce::Array<juce::var> { "p", 1, sk::P_TUNE, 0.5 });
        o2.add (juce::Array<juce::var> { "rib", 1, 5.0, 1, 0.8 });
        auto* ob2 = new juce::DynamicObject(); ob2->setProperty ("ops", o2);
        p5.pushOpsFromUi (juce::var (ob2));
        auto ops1 = [&] (const char* js) { auto* x = new juce::DynamicObject(); x->setProperty ("ops", juce::JSON::parse (js)); p5.pushOpsFromUi (juce::var (x)); };
        ops1 ("[[\"halo\", 1]]");
        auto run = [&] (double secs) { float m = 0.0f; for (int i = 0; i < (int) (48000 * secs) / bs; ++i) { buf.clear(); p5.processBlock (buf, midi); m = std::max (m, buf.getMagnitude (0, bs)); } return m; };
        const float sing = run (0.6);
        const float at = p5.meters.halo[2].load(), rib = p5.meters.halo[11].load();
        ops1 ("[[\"rib\", 1, -3.0, 1, 0.8]]");
        run (0.4);
        const float moved = p5.meters.halo[2].load();
        ops1 ("[[\"rib\", 1, -3.0, 0, 0]]");
        run (1.5);
        const float late = run (0.3);
        check ("CHOIR: the ribbon (\"rib\") sings at +5, follows to -3, lets go", sing > 0.02f && rib == 1.0f && std::abs (at - 65.0f) < 0.3f
               && std::abs (moved - 57.0f) < 0.3f && late < 0.01f * sing,
               "peak " + juce::String (sing, 3) + " at " + juce::String (at, 2) + " moved " + juce::String (moved, 2) + " late " + juce::String (late, 5));
        // SNAP (gate 2) on C major: the finger at C4 + 1.4 sings D4 (the engine holds it to the quantiser's notes)
        ops1 ("[[\"scale\", 2741], [\"qtrk\", 1, 1], [\"rib\", 1, 1.4, 2, 0.8]]");
        run (1.2);
        const float snapped = p5.meters.halo[2].load();
        ops1 ("[[\"rib\", 1, 1.4, 0, 0]]");
        run (1.5);
        check ("CHOIR: the ribbon on SNAP (gate 2) holds to the scale: C4 + 1.4 sings D4", std::abs (snapped - 62.0f) < 0.05f, "at " + juce::String (snapped, 3));

        // v81: HOWL on track 3, played on MIDI channel 4: a note plays its chord on that root (A2, DROP -12: A1), its
        // note-off lets it go; on POLY (track 4, channel 5) two notes take two strings
        {
            auto howlOn = [&] (int t, float chrd)
            {
                juce::Array<juce::var> o3;
                o3.add (juce::Array<juce::var> { "p", t, sk::P_MACH, sk::machineVal (sk::M_HOWL) });
                float af[8], sd[sk::NUM_SIDES]; sk::machineAFDefaults (sk::M_HOWL, af); const int ns = sk::machineSideDefaults (sk::M_HOWL, sd);
                o3.add (juce::Array<juce::var> { "p", t, sk::P_TUNE, 0.5 });
                for (int i = 0; i < 6; ++i) o3.add (juce::Array<juce::var> { "p", t, sk::P_A + i, i == 0 && chrd >= 0.0f ? chrd : af[i] });
                for (int q = 0; q < ns; ++q) o3.add (juce::Array<juce::var> { "p", t, sk::sideParam (q), sd[q] });
                o3.add (juce::Array<juce::var> { "p", t, sk::P_VOL, 0.8 });
                auto* ob3 = new juce::DynamicObject(); ob3->setProperty ("ops", o3);
                p5.pushOpsFromUi (juce::var (ob3));
                // (as the page sends them; a two-element juce::Array would take the pointer-and-count constructor)
                ops1 (("[[\"qtrk\", " + juce::String (t) + ", 0], [\"halo\", " + juce::String (t) + "]]").toRawUTF8());
            };
            auto blk = [&] { buf.clear(); p5.processBlock (buf, midi); midi.clear(); };
            howlOn (2, -1.0f);
            midi.addEvent (juce::MidiMessage::noteOn (4, 45, (juce::uint8) 110), 0); blk();
            const float held = run (0.8);
            const float root = p5.meters.halo[8].load(), n = p5.meters.halo[1].load(), kind = p5.meters.halo[0].load();
            midi.addEvent (juce::MidiMessage::noteOff (4, 45), 0); blk();
            run (3.0);
            const float after = run (0.3);
            check ("HOWL: MIDI channel 4 plays track 3's chord on the note (A2 -> A1 with DROP -12), its note-off lets it go",
                   kind == 4.0f && n == 6.0f && std::abs (root - 33.0f) < 0.01f && held > 0.02f && after < 0.1f * held,
                   "root " + juce::String (root, 2) + " strings " + juce::String (n) + " held " + juce::String (held, 3) + " after " + juce::String (after, 4));
            howlOn (3, (float) ((sk::HOWL_POLY_T + 0.5) / sk::HOWL_TYPES));
            midi.addEvent (juce::MidiMessage::noteOn (5, 40, (juce::uint8) 110), 0);
            midi.addEvent (juce::MidiMessage::noteOn (5, 47, (juce::uint8) 110), 64); blk();
            run (0.5);
            const float np = p5.meters.halo[1].load(), p0 = p5.meters.halo[2].load(), p1 = p5.meters.halo[3].load();
            midi.addEvent (juce::MidiMessage::noteOff (5, 40), 0);
            midi.addEvent (juce::MidiMessage::noteOff (5, 47), 0); blk();
            run (0.2);
            check ("HOWL on POLY: two notes on its channel (E2, B2; DROP -12), two strings", np == 2.0f && std::abs (std::min (p0, p1) - 28.0f) < 0.01f && std::abs (std::max (p0, p1) - 35.0f) < 0.01f,
                   "strings " + juce::String (np) + " at " + juce::String (p0, 2) + ", " + juce::String (p1, 2));
        }
        // v84: MANTRA on track 6, its screen's pads as the page sends them ("pad", track, pad, pressure): a finger on the
        // fourth wakes it, its voice alone (its pressure on the screen); lifted, a FAST pair falls silent
        {
            juce::Array<juce::var> o4;
            const int t = 5;
            o4.add (juce::Array<juce::var> { "p", t, sk::P_MACH, sk::machineVal (sk::M_MANTRA) });
            float af[8], sd[sk::NUM_SIDES]; const int na = sk::machineAFDefaults (sk::M_MANTRA, af); const int ns = sk::machineSideDefaults (sk::M_MANTRA, sd);
            for (int i = 0; i < na; ++i) o4.add (juce::Array<juce::var> { "p", t, sk::P_A + i, af[i] });
            for (int q = 0; q < ns; ++q) o4.add (juce::Array<juce::var> { "p", t, sk::sideParam (q), q == sk::MT_FST34 ? 0.75f : q == sk::MT_DMIX ? 0.0f : sd[q] });
            o4.add (juce::Array<juce::var> { "p", t, sk::P_VOL, 0.8 });
            auto* ob4 = new juce::DynamicObject(); ob4->setProperty ("ops", o4);
            p5.pushOpsFromUi (juce::var (ob4));
            ops1 ("[[\"halo\", 5], [\"pad\", 5, 3, 0.9]]");
            const float played = run (0.8);
            const float kind = p5.meters.halo[0].load(), n = p5.meters.halo[1].load(), pr3 = p5.meters.halo[18 + 3].load(), pr2 = p5.meters.halo[18 + 2].load();
            ops1 ("[[\"pad\", 5, 3, 0]]");
            run (1.0);
            run (0.3);
            const float env3 = p5.meters.halo[2 + 3].load(), pr3up = p5.meters.halo[18 + 3].load();
            check ("MANTRA: a pad on the screen (\"pad\") wakes track 6, its fourth voice alone; lifted, its FAST pair falls silent",
                   kind == 5.0f && n == 8.0f && std::abs (pr3 - 0.9f) < 1e-4f && pr2 == 0.0f && played > 0.01f && env3 < 1e-3f && pr3up == 0.0f,
                   "peak " + juce::String (played, 3) + " pressure " + juce::String (pr3, 3) + " level after " + juce::String (env3, 5));
        }
    }

    std::printf ("%d failure(s)\n", fails);
    return fails ? 1 : 0;
}
