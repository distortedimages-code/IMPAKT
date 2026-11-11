#include "PluginEditor.h"
#include "BinaryData.h"

ImpaktEditor::ImpaktEditor (ImpaktProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    // a 16:9 window: the page lays its panel out at 1280 x 720 and scales it to fit (five pages: SEQ, TONE, MOD, FX, MASTR)
    setResizable (true, true);
    setResizeLimits (960, 540, 2560, 1440);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (16.0 / 9.0);
    setSize (1280, 720);

    // WebKit must be created on the main thread. Some hosts build the editor elsewhere (an AUv3 loaded
    // in-process on a background queue): then the web view is created as soon as the main thread is free.
    if (juce::MessageManager::existsAndIsCurrentThread())
        createWebView();
    else
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ImpaktEditor> (this)]
                                         { if (safe != nullptr) safe->createWebView(); });
}

void ImpaktEditor::createWebView()
{
    if (web != nullptr) return;
    using Opts = juce::WebBrowserComponent::Options;
    auto options = Opts{}
        .withBackend (Opts::Backend::webview2)
        .withWinWebView2Options (Opts::WinWebView2{}
                                     .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("IMPAKT-WebView2"))
                                     .withBackgroundColour (juce::Colour (0xff181b1a))
                                     .withStatusBarDisabled())
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
        .withEventListener ("impakt_ops",   [this] (juce::var v) { proc.pushOpsFromUi (v); })
        .withEventListener ("impakt_state", [this] (juce::var v) { proc.setStateFromUi (v.getProperty ("json", "").toString()); })
        .withEventListener ("impakt_hello", [this] (juce::var)   { pageReady = true; sendLoad(); proc.updates().checkIfDue(); })
        // updates (v87): OPTION > Updates and the banner (check, download, later, the release's page, the daily check)
        .withEventListener ("impakt_update", [this] (juce::var v)
        {
            auto& u = proc.updates();
            const auto op = v.getProperty ("op", "").toString();
            if (op == "check") u.checkNow();
            else if (op == "download") u.download();
            else if (op == "later") u.later();
            else if (op == "auto") u.setAutoCheck ((bool) v.getProperty ("on", true));
            else if (op == "page") { const auto pg = u.info().page; juce::URL (pg.isNotEmpty() ? pg : juce::String ("https://github.com/") + Updater::repo + "/releases").launchInDefaultBrowser(); }
            else if (op == "show") { const juce::File f (u.info().file); if (f.exists()) f.revealToUser(); }
            // (v88: the Mac's Terminal command, which the page sends: the WebView's own clipboard is not always there)
            else if (op == "copy") { const auto txt = v.getProperty ("text", "").toString(); if (txt.length() < 4096) juce::SystemClipboard::copyTextToClipboard (txt); }
        })
        // BREAK: a loop the page decoded into a slot (base64 of its 16-bit samples)
        .withEventListener ("impakt_brk",   [this] (juce::var v)
        {
            juce::MemoryOutputStream mo;
            if (! juce::Base64::convertFromBase64 (mo, v.getProperty ("data", "").toString())) return;
            proc.loadBreakSlot ((int) v.getProperty ("slot", -1), (int) v.getProperty ("bars", 1), v.getProperty ("name", "").toString(), mo.getMemoryBlock());
        })
        // RADIO (v87): a receiver tuned, a question for the stations' directory or the receivers' list
        .withEventListener ("impakt_radio", [this] (juce::var v) { proc.radioFromUi (v); })
        // CUBE: a bank the page read from a WAV (base64 of its 16-bit samples: 64 waves of 256)
        .withEventListener ("impakt_cube",  [this] (juce::var v)
        {
            juce::MemoryOutputStream mo;
            if (! juce::Base64::convertFromBase64 (mo, v.getProperty ("data", "").toString())) return;
            proc.loadCubeBank ((int) v.getProperty ("bank", -1), v.getProperty ("name", "").toString(), mo.getMemoryBlock());
        });

    web = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*web);
    web->setBounds (getLocalBounds());
    web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz (30);
}

ImpaktEditor::~ImpaktEditor()
{
    stopTimer();
}

void ImpaktEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181b1a));
}

void ImpaktEditor::resized()
{
    if (web != nullptr) web->setBounds (getLocalBounds());
}

std::optional<juce::WebBrowserComponent::Resource> ImpaktEditor::getResource (const juce::String& url)
{
    const auto path = url.upToFirstOccurrenceOf ("?", false, false);
    if (path == "/" || path == "/index.html")
    {
        const auto* bytes = reinterpret_cast<const std::byte*> (BinaryData::index_html);
        return juce::WebBrowserComponent::Resource { std::vector<std::byte> (bytes, bytes + BinaryData::index_htmlSize),
                                                     "text/html" };
    }
    return std::nullopt;
}

void ImpaktEditor::sendLoad()
{
    seenVersion = proc.getStateVersion();
    for (auto& v : ramPkSeen) v = -1;         // the page draws every buffer again
    brkSent = -1;                             // and every slot
    cubeSent = -1;                            // and CUBE's banks
    radSent = -1;                             // and RADIO's receivers
    updSent = -1;                             // and the updater
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("json", proc.getStateJson());
    web->emitEventIfBrowserIsVisible ("impakt_load", juce::var (obj));
}

void ImpaktEditor::timerCallback()
{
    if (! pageReady || web == nullptr) return;

    // the host loaded a preset / project while the editor is open
    if (proc.getStateVersion() != seenVersion) sendLoad();

    juce::Array<juce::var> steps, hits;
    for (int t = 0; t < sk::NUM_TRACKS; ++t)
    {
        steps.add (proc.pos.steps[t].load());
        hits.add (proc.pos.hits[t].load());
    }
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("steps", steps);
    obj->setProperty ("hits", hits);
    obj->setProperty ("run", proc.pos.running.load());
    obj->setProperty ("host", proc.pos.hostRunning.load());
    obj->setProperty ("bpm", proc.pos.bpm.load());
    { juce::Array<juce::var> pat; for (int k = 0; k < sk::PAT_INFO; ++k) pat.add (proc.pos.pat[k].load()); obj->setProperty ("pat", pat); }
    obj->setProperty ("qm", proc.pos.qmask.load());
    // master compressor gain reduction (dB) and output peak, CALM / unmasking display
    obj->setProperty ("gr", proc.meters.gr.exchange (0.0f));
    obj->setProperty ("out", proc.meters.out.exchange (0.0f));
    if (proc.meters.mixValid.load (std::memory_order_acquire))
    {
        juce::Array<juce::var> mix;
        mix.ensureStorageAllocated (ImpaktProcessor::mixInfoSize);
        for (int k = 0; k < ImpaktProcessor::mixInfoSize; ++k) mix.add (proc.meters.mix[k].load (std::memory_order_relaxed));
        obj->setProperty ("mix", mix);
    }
    if (proc.meters.specOn.load (std::memory_order_relaxed))
    {
        juce::Array<juce::var> spec;
        for (int k = 0; k < sk::Engine::SPEC_BANDS; ++k) spec.add (proc.meters.spec[k].load (std::memory_order_relaxed));
        obj->setProperty ("spec", spec);
    }
    if (proc.meters.fx[5].load (std::memory_order_relaxed) > 0.5f || proc.meters.fx[6].load (std::memory_order_relaxed) > 0.5f)
    {
        juce::Array<juce::var> fx;
        for (int k = 0; k < 7; ++k) fx.add (proc.meters.fx[k].load (std::memory_order_relaxed));
        obj->setProperty ("fx", fx);
    }
    if (proc.meters.glitch[19].load (std::memory_order_relaxed) > 0.5f)
    {
        juce::Array<juce::var> gl;
        for (int k = 0; k < 22; ++k) gl.add (proc.meters.glitch[k].load (std::memory_order_relaxed));
        obj->setProperty ("glitch", gl);
    }
    if (proc.meters.prismT.load (std::memory_order_relaxed) >= 0)
    {
        juce::Array<juce::var> pr;
        for (int k = 0; k < 2 * sk::PR_K; ++k) pr.add (proc.meters.prism[k].load (std::memory_order_relaxed));
        obj->setProperty ("prism", pr);
    }
    if (proc.meters.haloT.load (std::memory_order_relaxed) >= 0)
    {
        juce::Array<juce::var> hl;
        for (int k = 0; k < sk::HALO_INFO; ++k) hl.add (proc.meters.halo[k].load (std::memory_order_relaxed));
        obj->setProperty ("halo", hl);
    }
    if (proc.meters.brkT.load (std::memory_order_relaxed) >= 0)
    {
        juce::Array<juce::var> bk;
        for (int k = 0; k < sk::BRK_INFO; ++k) bk.add (proc.meters.brk[k].load (std::memory_order_relaxed));
        obj->setProperty ("brk", bk);
    }
    if (proc.meters.cubeT.load (std::memory_order_relaxed) >= 0)
    {
        juce::Array<juce::var> cb;
        for (int k = 0; k < sk::CUBE_INFO; ++k) cb.add (proc.meters.cube[k].load (std::memory_order_relaxed));
        obj->setProperty ("cube", cb);
    }
    if (proc.meters.westT.load (std::memory_order_relaxed) >= 0)
    {
        juce::Array<juce::var> ws;
        for (int k = 0; k < sk::WS_INFO; ++k) ws.add (proc.meters.west[k].load (std::memory_order_relaxed));
        obj->setProperty ("west", ws);
    }
    if (proc.meters.chimT.load (std::memory_order_relaxed) >= 0)
    {
        juce::Array<juce::var> cm;
        for (int k = 0; k < sk::CHIM_INFO; ++k) cm.add (proc.meters.chim[k].load (std::memory_order_relaxed));
        obj->setProperty ("chim", cm);
    }
    if (proc.meters.fxmT.load (std::memory_order_relaxed) >= 0)
    {
        juce::Array<juce::var> fm;
        for (int k = 0; k < sk::FX_INFO; ++k) fm.add (k == 4 || k == 5 || k == 12 ? proc.meters.fxm[k].exchange (0.0f, std::memory_order_relaxed) : proc.meters.fxm[k].load (std::memory_order_relaxed));
        obj->setProperty ("fxm", fm);
    }
    {
        juce::Array<juce::var> mod;
        mod.ensureStorageAllocated (8 * sk::NUM_TRACKS);
        for (int k = 0; k < 8 * sk::NUM_TRACKS; ++k) mod.add ((k & 7) < 4 ? proc.meters.mod[k].load (std::memory_order_relaxed) : proc.meters.mod[k].exchange (0.0f, std::memory_order_relaxed));
        obj->setProperty ("mod", mod);
    }
    // RAM machine: each buffer, and the peaks of those that changed
    {
        float info[sk::NUM_TRACKS * sk::Engine::RAM_INFO];
        proc.getRamInfo (info);
        juce::Array<juce::var> ram;
        for (float v : info) ram.add (v);
        obj->setProperty ("ram", ram);
        juce::DynamicObject::Ptr peaks;
        for (int t = 0; t < sk::NUM_TRACKS; ++t)
        {
            const int pv = (int) info[t * sk::Engine::RAM_INFO + 5];
            if (pv == ramPkSeen[t]) continue;
            ramPkSeen[t] = pv;
            const float* pk = proc.getRamPeaks (t);
            juce::Array<juce::var> a;
            for (int b = 0; b < sk::RAM_PEAKS; ++b) a.add (pk[b]);
            if (peaks == nullptr) peaks = new juce::DynamicObject();
            peaks->setProperty (juce::String (t), a);
        }
        if (peaks != nullptr) obj->setProperty ("peaks", juce::var (peaks.get()));
    }
    // BREAK's slots, when they changed: state, tempo, bars, seconds; names; waveforms
    if (const int bv = proc.meters.brkVer.load (std::memory_order_acquire); bv != brkSent)
    {
        brkSent = bv;
        auto* b = new juce::DynamicObject();
        juce::Array<juce::var> info, names, peaks;
        for (int k = 0; k < sk::BRK_SLOTS * sk::BRK_SLOT_INFO; ++k) info.add (proc.meters.brkSlots[k].load (std::memory_order_relaxed));
        for (int s = 0; s < sk::BRK_SLOTS; ++s)
        {
            names.add (proc.getBreakName (s));
            juce::Array<juce::var> pk;
            for (int k = 0; k < sk::BRK_NPEAK; ++k) pk.add (proc.meters.brkPeaks[s * sk::BRK_NPEAK + k].load (std::memory_order_relaxed));
            peaks.add (pk);
        }
        b->setProperty ("ver", bv); b->setProperty ("info", info); b->setProperty ("names", names); b->setProperty ("peaks", peaks);
        obj->setProperty ("brks", juce::var (b));
    }
    // CUBE's banks, when they changed: their names, which are ready, one being made
    if (const int cv = proc.meters.cubeVer.load (std::memory_order_acquire); cv != cubeSent)
    {
        cubeSent = cv;
        auto* c = new juce::DynamicObject();
        juce::Array<juce::var> names, ready;
        for (int k = 0; k < sk::CUBE_BANKS; ++k) { names.add (proc.getCubeName (k)); ready.add (proc.cubeReady (k)); }
        c->setProperty ("ver", cv); c->setProperty ("names", names); c->setProperty ("ready", ready);
        obj->setProperty ("cubes", juce::var (c));
    }
    // RADIO (v87): the track shown; the receivers (their sources and states when they changed; their levels while one is
    // tuned or played); the answers to the page's questions
    if (proc.meters.radT.load (std::memory_order_relaxed) >= 0)
    {
        juce::Array<juce::var> rd;
        for (int k = 0; k < sk::RADIO_INFO; ++k) rd.add (proc.meters.rad[k].load (std::memory_order_relaxed));
        obj->setProperty ("radio", rd);
    }
    {
        auto& net = proc.radioNet();
        const int rv = net.version(), use = proc.meters.radUse.load (std::memory_order_relaxed);
        bool any = use != 0;
        for (int k = 0; k < sk::RADIO_RX && ! any; ++k) any = net.getSource (k).kind != 0;
        if (rv != radSent || any)
        {
            auto* r = new juce::DynamicObject();
            r->setProperty ("use", use);
            juce::Array<juce::var> info;
            for (int k = 0; k < sk::RADIO_RX * sk::RADIO_RX_INFO; ++k) info.add (proc.meters.radRx[k].load (std::memory_order_relaxed));
            r->setProperty ("info", info);
            if (rv != radSent)
            {
                radSent = rv;
                juce::Array<juce::var> src, st;
                for (int k = 0; k < sk::RADIO_RX; ++k)
                {
                    src.add (net.getSource (k).toVar());
                    const auto s = net.getStatus (k);
                    auto* o = new juce::DynamicObject();
                    o->setProperty ("state", s.state); o->setProperty ("msg", s.msg); o->setProperty ("title", s.title);
                    o->setProperty ("rate", s.rate); o->setProperty ("kbps", s.kbps);
                    st.add (juce::var (o));
                }
                r->setProperty ("ver", rv); r->setProperty ("src", src); r->setProperty ("st", st);
            }
            obj->setProperty ("radios", juce::var (r));
        }
        auto answers = net.takeAnswers();
        if (! answers.empty())
        {
            juce::Array<juce::var> a;
            for (auto& v : answers) a.add (v);
            obj->setProperty ("radq", a);
        }
    }
    // updates (v87): what the updater knows, when it changed
    if (const int uv = proc.updates().version(); uv != updSent)
    {
        updSent = uv;
        const auto i = proc.updates().info();
        auto* u = new juce::DynamicObject();
        u->setProperty ("state", i.state); u->setProperty ("cur", i.current); u->setProperty ("latest", i.latest);
        u->setProperty ("notes", i.notes); u->setProperty ("file", i.file); u->setProperty ("msg", i.msg);
        u->setProperty ("progress", i.progress); u->setProperty ("auto", i.autoCheck); u->setProperty ("skipped", i.skipped);
       #if JUCE_MAC
        u->setProperty ("os", "mac");
       #elif JUCE_WINDOWS
        u->setProperty ("os", "win");
       #else
        u->setProperty ("os", "linux");
       #endif
        obj->setProperty ("upd", juce::var (u));
    }
    web->emitEventIfBrowserIsVisible ("impakt_pos", juce::var (obj));
}
