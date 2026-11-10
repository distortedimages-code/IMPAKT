// IMPAKT — updates (v87): the plugin and the app ask the project's GitHub releases whether a newer IMPAKT is out (once a
// session when the page opens, or when asked: OPTION > Updates), and fetch the installer of this system into the
// Downloads folder: the Mac's disk image (opened: its installer and the plugins wait there), Windows' setup, Linux's
// archive (shown in the file manager). Nothing is replaced under a running host: the installer runs once it is closed.
// One updater for the process (every instance of the plugin shares it: juce::SharedResourcePointer).
#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>

class Updater : private juce::Thread
{
public:
    // what the page shows
    enum State { Idle = 0, Checking, UpToDate, Available, Downloading, Downloaded, Failed };
    struct Info
    {
        int state = Idle;
        juce::String current, latest, notes, page, file, msg;   // (file: the installer fetched; msg: why it failed)
        float progress = 0.0f;
        bool autoCheck = true, skipped = false;                  // (skipped: LATER was pressed on this version)
    };

    Updater();
    ~Updater() override;

    // the message thread (the editor)
    void   checkIfDue();                     // (the page opened: once a session, unless turned off)
    void   checkNow();                       // (asked)
    void   download();
    void   later();                          // (no banner for this version)
    void   setAutoCheck (bool on);
    Info   info() const;
    int    version() const noexcept { return ver.load(); }   // (moves when the info changes)

    // where the releases are (the repository on GitHub: owner/name)
    static constexpr const char* repo = "distortedimages-code/IMPAKT";
    static juce::String assetName();         // (this system's installer, as the releases name it)
    static int compareVersions (const juce::String& a, const juce::String& b);

private:
    struct Request;
    void run() override;
    void doCheck();
    void doDownload();
    void set (const std::function<void (Info&)>& f);
    juce::PropertiesFile& settings();

    mutable juce::CriticalSection lock;
    Info inf;
    juce::String assetUrl;
    std::atomic<int> ver { 0 }, job { 0 };    // (job: 1 check, 2 download)
    std::unique_ptr<juce::PropertiesFile> props;
    juce::CriticalSection liveLock;
    juce::WebInputStream* live = nullptr;    // (the request under way: the destructor cancels it)

    JUCE_DECLARE_NON_COPYABLE (Updater)
};
