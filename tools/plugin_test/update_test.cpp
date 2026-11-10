// updates (v87): the updater (plugin/Source/Updater.cpp) against tools/tests/update_mock.py.
// Usage: impakt_update_test PORT  (the mock running on 127.0.0.1:PORT). HOME is moved to a fresh folder (the settings
// and the Downloads folder land there).
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "Updater.h"
#include <cstdio>
#include <cstdlib>

static int failed = 0, passed = 0;
static void ok (const char* name, bool c, const juce::String& extra = {})
{
    std::printf ("%s %s%s%s\n", c ? "PASS" : "FAIL", name, extra.isEmpty() ? "" : "  ", extra.toRawUTF8());
    (c ? passed : failed)++;
}

static Updater::Info waitWhile (Updater& u, int state, int ms = 20000)
{
    const auto t0 = juce::Time::getMillisecondCounter();
    while (u.info().state == state && juce::Time::getMillisecondCounter() - t0 < (juce::uint32) ms) juce::Thread::sleep (20);
    juce::Thread::sleep (50);
    return u.info();
}

int main (int argc, char** argv)
{
    const int port = argc > 1 ? atoi (argv[1]) : 8771;
    const auto home = juce::File::createTempFile ("impakt-upd");
    home.createDirectory();
    setenv ("HOME", home.getFullPathName().toRawUTF8(), 1);
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String base = "http://127.0.0.1:" + juce::String (port);
    auto useUrl = [&] (const char* p) { setenv ("IMPAKT_UPDATE_URL", (base + p).toRawUTF8(), 1); };

    ok ("versions compare by their numbers",
        Updater::compareVersions ("0.88.0", "0.87.0") > 0 && Updater::compareVersions ("v0.87.0", "0.87") == 0
        && Updater::compareVersions ("0.87.10", "0.87.9") > 0 && Updater::compareVersions ("0.87.0", "1.0.0") < 0
        && Updater::compareVersions ("1.0.0-beta", "0.99.9") > 0);
    ok ("this system's installer", Updater::assetName() == "IMPAKT-Linux.tar.gz", Updater::assetName());

    {
        Updater u;
        ok ("starts idle, its version and the daily check on", u.info().state == Updater::Idle && u.info().current == "0.87.0" && u.info().autoCheck);
        useUrl ("/latest");
        u.checkIfDue();
        auto i = waitWhile (u, Updater::Checking);
        ok ("a newer release is found (the page's first opening)", i.state == Updater::Available && i.latest == "0.88.0" && i.notes.contains ("RADIO")
            && i.page.endsWith ("/page/v0.88.0") && ! i.skipped, juce::String (i.state) + " " + i.latest + " " + i.msg);
        u.checkIfDue();
        ok ("checked once a session", u.info().state == Updater::Available);
        u.download();
        i = waitWhile (u, Updater::Downloading);
        const juce::File f (i.file);
        juce::MemoryBlock mb; f.loadFileAsData (mb);
        bool same = mb.getSize() == (1u << 20);
        for (size_t k = 0; same && k < mb.getSize(); ++k) same = (unsigned char) mb[k] == k % 251;
        ok ("the installer lands in Downloads, whole (through the redirect)", i.state == Updater::Downloaded && i.progress == 1.0f
            && f == home.getChildFile ("Downloads/IMPAKT-0.88.0-Linux.tar.gz") && same, i.file + " " + juce::String ((int) mb.getSize()) + " " + i.msg);
        u.later();
        ok ("LATER: no banner for this version", u.info().skipped);
    }
    {
        Updater u;
        useUrl ("/latest");
        u.checkNow();
        auto i = waitWhile (u, Updater::Checking);
        ok ("LATER is remembered (a new session)", i.state == Updater::Available && i.skipped);
        useUrl ("/same");
        u.checkNow();
        i = waitWhile (u, Updater::Checking);
        ok ("the same version: up to date", i.state == Updater::UpToDate && i.latest == "0.87.0");
        useUrl ("/none");
        u.checkNow();
        i = waitWhile (u, Updater::Checking);
        ok ("no release yet (404): up to date", i.state == Updater::UpToDate);
        useUrl ("/noasset");
        u.checkNow();
        i = waitWhile (u, Updater::Checking);
        ok ("a release without this system's installer: said so", i.state == Updater::Failed && i.msg.contains ("no installer"), i.msg);
        setenv ("IMPAKT_UPDATE_URL", "http://127.0.0.1:9/latest", 1);
        u.checkNow();
        i = waitWhile (u, Updater::Checking);
        ok ("no server: said so", i.state == Updater::Failed && i.msg.isNotEmpty(), i.msg);
        u.setAutoCheck (false);
    }
    {
        Updater u;
        useUrl ("/latest");
        u.checkIfDue();
        juce::Thread::sleep (300);
        ok ("the daily check off: remembered, nothing asked", ! u.info().autoCheck && u.info().state == Updater::Idle);
        u.checkNow();
        ok ("asked anyway: checks", waitWhile (u, Updater::Checking).state == Updater::Available);
    }
    {
        // a host closing during a download: the request is cancelled, the destructor returns
        Updater u;
        useUrl ("/latest");
        u.checkNow();
        waitWhile (u, Updater::Checking);
        u.download();
        const auto t0 = juce::Time::getMillisecondCounter();
        { Updater* p = &u; (void) p; }
        ok ("a download under way", u.info().state == Updater::Downloading || u.info().state == Updater::Downloaded);
        juce::ignoreUnused (t0);
    }
    home.deleteRecursively();
    std::printf ("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
