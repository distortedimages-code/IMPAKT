// IMPAKT — RADIO (v87): the plugin's receivers on the network. Each of the four has its own thread: tuned to an internet
// station (WEB: an MP3 stream over HTTP or HTTPS, its titles read as they come) or to a shortwave receiver on the
// network (SW: its WebSocket, AM / SSB / CW at a frequency), it fetches the sound, decodes it (sk::RadioDec) and hands
// it to the engine (radioPush), as long as a RADIO track plays that receiver. The lists the page browses (the stations'
// directory, the receivers' public list) are fetched on a worker thread, their answers handed to the page.
//
// Manners: one connection a receiver, never two on the same shortwave receiver; let go 45 s after no track plays it;
// a receiver that says it is busy, refuses or ends the connection is not asked again until the page tunes it again;
// a network error is retried three times (3, 10, 30 s), then left; the frequency changed at most five times a second;
// the client's name sent to the receivers (IMPAKT); each station tuned counted in the directory (as it asks).
#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <deque>
#include <memory>
#include "sk_engine.h"

class RadioNet
{
public:
    // what a receiver is tuned to
    struct Source
    {
        int kind = 0;                      // 0 nothing, 1 WEB, 2 SW
        bool on = true;
        juce::String name, url, uuid, country, codec;   // WEB
        int kbps = 0;
        juce::String host, rxName;         // SW: the receiver (host, port, its own name)
        int port = 8073;
        double khz = 9420.0;               // SW: the frequency, kHz, and the mode (am, usb, lsb, cw)
        juce::String mode { "am" };
        juce::var toVar() const;
        static Source fromVar (const juce::var& v);
        bool sameStream (const Source& o) const;   // (the same connection: for SW the same receiver, whatever its frequency)
    };
    // what the page shows of a receiver
    struct Status
    {
        int state = sk::RS_NONE;
        juce::String msg, title;           // why it failed / what it says; the station's title now
        int rate = 0, kbps = 0, users = -1, usersMax = -1;
    };

    explicit RadioNet (sk::Engine& e);
    ~RadioNet();

    // the message thread (the page, a project)
    void   setSource (int rx, const Source& s);
    Source getSource (int rx) const;
    Status getStatus (int rx) const;
    int    version() const noexcept { return ver.load(); }        // (moves when a source or a status changes)
    // the audio thread: the receivers the tracks play (bits)
    void   setUse (int mask) noexcept { use.store (mask, std::memory_order_relaxed); }
    // the page's questions (their answers in takeAnswers): the stations' directory, the receivers' list
    void   search (int id, const juce::var& q);
    void   receivers (int id, bool fresh);
    std::vector<juce::var> takeAnswers();
    // the project
    void   write (juce::OutputStream& out) const;
    void   read (juce::InputStream& in);
    void   clearAll();

    // (for the tests: a directory or a receivers' list elsewhere, a shorter standby)
    juce::String dirOverride, listOverride;
    int standbyMs = 45000;

private:
    struct Rx;
    struct Watch;
    sk::Engine& engine;
    std::unique_ptr<Rx> rx[sk::RADIO_RX];
    std::unique_ptr<Watch> sup;            // (ends the sessions that should end: a blocking HTTPS read among them)
    std::atomic<int> use { 0 }, ver { 0 };
    std::unique_ptr<juce::ThreadPool> pool;
    juce::CriticalSection ansLock;
    std::vector<juce::var> answers;
    juce::CriticalSection dirLock;
    juce::String dirHost;                  // the directory's server (picked once)
    juce::var    rxList; juce::int64 rxListAt = 0;
    void   answer (const juce::var& v);
    juce::String directoryHost();
    void   countClick (const juce::String& uuid);
    friend struct Rx;
    friend struct Watch;
    JUCE_DECLARE_NON_COPYABLE (RadioNet)
};
