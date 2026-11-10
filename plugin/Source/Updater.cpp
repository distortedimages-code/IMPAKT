#include "Updater.h"
#include <cstdlib>

namespace
{
    const char* const kAuto = "updAuto", * const kLast = "updLast", * const kSkip = "updSkip";

    juce::String userAgent() { return juce::String ("IMPAKT/") + JucePlugin_VersionString; }

}

// a request to GitHub, which a closing host can cancel (the destructor): its stream, its status
struct Updater::Request
{
    Updater& u;
    std::unique_ptr<juce::WebInputStream> in;
    int status = 0;
    explicit Request (Updater& o) : u (o) {}
    ~Request() { const juce::ScopedLock sl (u.liveLock); u.live = nullptr; }
    bool open (const juce::URL& url, const juce::String& accept)
    {
        in = std::make_unique<juce::WebInputStream> (url, false);
        in->withExtraHeaders ("User-Agent: " + userAgent() + "\r\nAccept: " + accept)
           .withConnectionTimeout (15000)
           .withNumRedirectsToFollow (8);
        {
            const juce::ScopedLock sl (u.liveLock);
            if (u.threadShouldExit()) return false;
            u.live = in.get();
        }
        const bool ok = in->connect (nullptr);
        status = in->getStatusCode();
        return ok && status >= 200 && status < 300;
    }
};

juce::String Updater::assetName()
{
   #if JUCE_MAC
    return "IMPAKT-macOS.dmg";
   #elif JUCE_WINDOWS
    return "IMPAKT-Windows-Setup.exe";
   #else
    return "IMPAKT-Linux.tar.gz";
   #endif
}

int Updater::compareVersions (const juce::String& a, const juce::String& b)
{
    auto parts = [] (juce::String s)
    {
        s = s.trim().trimCharactersAtStart ("vV");
        juce::StringArray p; p.addTokens (s.upToFirstOccurrenceOf ("-", false, false), ".", "");
        while (p.size() < 3) p.add ("0");
        return p;
    };
    const auto pa = parts (a), pb = parts (b);
    for (int i = 0; i < juce::jmax (pa.size(), pb.size()); ++i)
    {
        const int x = pa[i].getIntValue(), y = pb[i].getIntValue();
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

Updater::Updater() : juce::Thread ("IMPAKT updater")
{
    inf.current = JucePlugin_VersionString;
    inf.autoCheck = settings().getBoolValue (kAuto, true);
}

Updater::~Updater()
{
    {
        const juce::ScopedLock sl (liveLock);
        signalThreadShouldExit();
        if (live != nullptr) live->cancel();
    }
    stopThread (8000);
}

juce::PropertiesFile& Updater::settings()
{
    if (props == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "IMPAKT";
        o.filenameSuffix = ".settings";
        o.folderName = "IMPAKT";
        o.osxLibrarySubFolder = "Application Support";
        props = std::make_unique<juce::PropertiesFile> (o);
    }
    return *props;
}

void Updater::set (const std::function<void (Info&)>& f)
{
    { const juce::ScopedLock sl (lock); f (inf); }
    ++ver;
}

Updater::Info Updater::info() const
{
    const juce::ScopedLock sl (lock);
    return inf;
}

void Updater::checkIfDue()
{
    // once a session (the first page opened), unless turned off
    if (settings().getBoolValue (kAuto, true) && info().state == Idle) checkNow();
}

void Updater::checkNow()
{
    if (isThreadRunning()) return;
    set ([] (Info& i) { i.state = Checking; i.msg = {}; });
    job = 1;
    startThread();
}

void Updater::download()
{
    if (isThreadRunning() || info().state != Available) return;
    set ([] (Info& i) { i.state = Downloading; i.progress = 0.0f; i.msg = {}; });
    job = 2;
    startThread();
}

void Updater::later()
{
    const auto v = info().latest;
    settings().setValue (kSkip, v);
    settings().saveIfNeeded();
    set ([] (Info& i) { i.skipped = true; });
}

void Updater::setAutoCheck (bool on)
{
    settings().setValue (kAuto, on);
    settings().saveIfNeeded();
    set ([on] (Info& i) { i.autoCheck = on; });
}

void Updater::run()
{
    if (job == 1) doCheck();
    else if (job == 2) doDownload();
    job = 0;
}

void Updater::doCheck()
{
    Request r (*this);
    // (IMPAKT_UPDATE_URL: another server, for the tests)
    const char* other = std::getenv ("IMPAKT_UPDATE_URL");
    const juce::URL url (other != nullptr ? juce::String (other) : juce::String ("https://api.github.com/repos/") + repo + "/releases/latest");
    if (! r.open (url, "application/vnd.github+json"))
    {
        const int st = r.status;
        if (threadShouldExit()) return;
        // (404: no release published yet)
        if (st == 404) { set ([] (Info& i) { i.state = UpToDate; i.latest = {}; }); return; }
        set ([st] (Info& i) { i.state = Failed;
                              i.msg = st == 0 ? "No connection to the update server"
                                    : st == 403 || st == 429 ? "The update server is busy: try again later"
                                    : "The update server answered " + juce::String (st); });
        return;
    }
    const auto json = juce::JSON::parse (r.in->readEntireStreamAsString());
    if (threadShouldExit()) return;
    const auto tag = json.getProperty ("tag_name", "").toString();
    if (tag.isEmpty()) { set ([] (Info& i) { i.state = Failed; i.msg = "The update server's answer is unreadable"; }); return; }

    juce::String asset;
    if (const auto* assets = json.getProperty ("assets", {}).getArray())
        for (const auto& a : *assets)
            if (a.getProperty ("name", "").toString() == assetName())
                asset = a.getProperty ("browser_download_url", "").toString();

    settings().setValue (kLast, juce::String (juce::Time::currentTimeMillis()));
    settings().saveIfNeeded();
    const auto latest = tag.trimCharactersAtStart ("vV");
    auto notes = json.getProperty ("body", "").toString().replace ("\r", "").trim();
    if (notes.length() > 900) notes = notes.substring (0, 900).upToLastOccurrenceOf ("\n", false, false) + "\n…";
    const auto page = json.getProperty ("html_url", "").toString();
    const bool newer = compareVersions (latest, JucePlugin_VersionString) > 0;
    const bool skipped = settings().getValue (kSkip) == latest;
    { const juce::ScopedLock sl (lock); assetUrl = asset; }
    set ([&] (Info& i)
    {
        i.latest = latest; i.notes = notes; i.page = page; i.skipped = skipped;
        i.state = ! newer ? UpToDate : Available;
        if (newer && asset.isEmpty()) { i.state = Failed; i.msg = "IMPAKT " + latest + " has no installer for this system yet"; }
    });
}

void Updater::doDownload()
{
    juce::String urlText, latest;
    { const juce::ScopedLock sl (lock); urlText = assetUrl; latest = inf.latest; }
    auto folder = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads");
    if (! folder.isDirectory() && ! folder.createDirectory()) folder = juce::File::getSpecialLocation (juce::File::tempDirectory);
    const auto name = assetName();
    const auto target = folder.getChildFile (name.upToFirstOccurrenceOf ("-", false, false) + "-" + latest + "-"
                                             + name.fromFirstOccurrenceOf ("-", false, false));
    auto fail = [this] (const juce::String& why) { set ([why] (Info& i) { i.state = Available; i.msg = why; }); };

    Request r (*this);
    if (! r.open (juce::URL (urlText), "application/octet-stream")) { if (! threadShouldExit()) fail ("The download did not start (" + juce::String (r.status) + ")"); return; }
    juce::TemporaryFile tmp (target);
    {
        juce::FileOutputStream out (tmp.getFile());
        if (! out.openedOk()) { fail ("Cannot write in " + folder.getFullPathName()); return; }
        const auto total = r.in->getTotalLength();
        juce::HeapBlock<char> buf (1 << 16);
        juce::int64 got = 0;
        juce::uint32 shown = 0;
        while (! r.in->isExhausted())
        {
            if (threadShouldExit()) return;
            const int n = r.in->read (buf, 1 << 16);
            if (n < 0 || (n == 0 && r.in->isError())) { fail ("The download stopped: try again"); return; }
            if (n == 0) break;
            if (! out.write (buf, (size_t) n)) { fail ("The disk is full?"); return; }
            got += n;
            if (total > 0 && juce::Time::getMillisecondCounter() - shown > 150)
            {
                shown = juce::Time::getMillisecondCounter();
                const float p = (float) ((double) got / (double) total);
                set ([p] (Info& i) { i.progress = p; });
            }
        }
        out.flush();
        if (total > 0 && got != total) { fail ("The download is incomplete: try again"); return; }
    }
    if (! tmp.overwriteTargetFileWithTemporary()) { fail ("Cannot write " + target.getFullPathName()); return; }
    set ([target] (Info& i) { i.state = Downloaded; i.progress = 1.0f; i.file = target.getFullPathName(); });

    // the Mac's disk image opens (its installer and the plugins in a Finder window); elsewhere the file is shown
    juce::MessageManager::callAsync ([target]
    {
       #if JUCE_MAC
        if (! target.startAsProcess()) target.revealToUser();
       #else
        target.revealToUser();
       #endif
    });
}
