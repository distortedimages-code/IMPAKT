// IMPAKT v87, RADIO: the plugin's receivers on the network (plugin/Source/RadioNet), against local stand-ins
// (tools/tests/radio_mock.py PORT MP3): a station (its titles, a redirection, a playlist, an AAC one refused), the
// directory (a search filtered, a click counted), the receivers' list, a shortwave receiver (tuned over its connection),
// one with no channel free (not asked again), two receivers on the same one (refused), the standby.
#include "RadioNet.h"
#include <cstdio>
#include <cmath>
#include <vector>

static int fails = 0;
static void check (const char* name, bool ok, const juce::String& info = {})
{
    std::printf ("%s %s %s\n", ok ? "PASS" : "FAIL", name, info.toRawUTF8());
    std::fflush (stdout);
    if (! ok) ++fails;
}
static float peakHz (const std::vector<float>& x, size_t from, int n, float sr, float lo, float hi)
{
    float best = 0, bf = 0;
    for (float f = lo; f <= hi; f += 5)
    {
        double re = 0, im = 0;
        for (int i = 0; i < n; ++i) { const double w = 0.5 - 0.5 * cos (2 * M_PI * i / (n - 1)); re += x[from + (size_t) i] * w * cos (2 * M_PI * f * i / sr); im += x[from + (size_t) i] * w * sin (2 * M_PI * f * i / sr); }
        if (re * re + im * im > best) { best = (float) (re * re + im * im); bf = f; }
    }
    return bf;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const int port = argc > 1 ? juce::String (argv[1]).getIntValue() : 18400;
    const juce::String base = "http://127.0.0.1:" + juce::String (port);
    auto engine = std::make_unique<sk::Engine>();
    sk::Engine& e = *engine;
    e.init (48000.0f);
    float sd[sk::NUM_SIDES]; sk::machineSideDefaults (sk::M_RADIO, sd);
    for (int t = 0; t < 2; ++t)
    {
        for (int q = 0; q < sk::NUM_SIDES; ++q) e.setParam (t, sk::sideParam (q), sd[q]);
        e.setParam (t, sk::P_MACH, sk::machineVal (sk::M_RADIO));
        e.setParam (t, sk::P_A, (t + 0.5f) / 4.0f); e.setParam (t, sk::P_B, 0.1f); e.setParam (t, sk::P_C, 0.99f);
        e.setParam (t, sk::P_D, 0.0f); e.setParam (t, sk::P_E, 0.0f); e.setParam (t, sk::P_DEC, 1.0f); e.setParam (t, sk::P_VOL, 0.8f);
        e.setParam (t, sk::P_FREQ, 1.0f);
        e.setParam (t, sk::P_PAN, t == 0 ? 0.0f : 1.0f);      // (track 1 left, track 2 right)
    }
    RadioNet net (e);
    net.dirOverride = base;
    net.listOverride = base + "/public/";
    net.standbyMs = 3000;

    // the audio, in real time
    std::vector<float> L, R;
    std::vector<float> bl (256), br (256);
    double done = 0; const auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto run = [&] (double secs, bool keep) {
        const double until = juce::Time::getMillisecondCounterHiRes() + secs * 1000.0;
        while (juce::Time::getMillisecondCounterHiRes() < until)
        {
            const double due = (juce::Time::getMillisecondCounterHiRes() - t0) * 48.0;
            while (done + 256 <= due)
            {
                e.process (bl.data(), br.data(), 256, nullptr);
                net.setUse (e.radioUse());
                if (keep) { L.insert (L.end(), bl.begin(), bl.end()); R.insert (R.end(), br.begin(), br.end()); }
                done += 256;
            }
            juce::Thread::sleep (2);
        }
    };
    auto waitState = [&] (int rx, int want, double secs) {
        const double until = juce::Time::getMillisecondCounterHiRes() + secs * 1000.0;
        while (juce::Time::getMillisecondCounterHiRes() < until) { if (net.getStatus (rx).state == want) return true; run (0.05, false); }
        return false;
    };
    auto stats = [&] { juce::String t, err; juce::URL u (base + "/stats"); auto s = u.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs (3000));
        return s ? juce::JSON::parse (s->readEntireStreamAsString()) : juce::var(); };

    // ---- a station
    RadioNet::Source s; s.kind = 1; s.url = base + "/redir"; s.name = "Mock"; s.uuid = "u-1";
    net.setSource (0, s);
    check ("WEB: the station plays (through a redirection)", waitState (0, sk::RS_LIVE, 8), net.getStatus (0).msg);
    e.audition (0, 1.0f);
    L.clear(); R.clear(); run (2.5, true);
    {
        const float hz = L.size() > 30000 ? peakHz (L, L.size() - 16384, 16384, 48000, 500, 2000) : 0;
        double ss = 0; for (size_t i = L.size() - 16384; i < L.size(); ++i) ss += L[i] * L[i];
        check ("WEB: its sound reaches the track (1 kHz)", std::abs (hz - 1000) <= 6, juce::String (hz) + " Hz, rms " + juce::String (std::sqrt (ss / 16384), 3));
    }
    run (3.0, false);
    check ("WEB: its title read (ICY)", net.getStatus (0).title.startsWith ("Mock Song"), net.getStatus (0).title);
    check ("WEB: its format", net.getStatus (0).rate == 44100 && net.getStatus (0).kbps == 128, juce::String (net.getStatus (0).rate) + " / " + juce::String (net.getStatus (0).kbps));
    float ri[sk::RADIO_RX * sk::RADIO_RX_INFO]; e.radioRxInfo (ri);
    check ("WEB: the engine keeps about a second in hand (two at most)", ri[3] > 0.5f && ri[3] <= 2.0f && ri[5] == 0, juce::String (ri[3], 2) + " s, " + juce::String (ri[5]) + " dropouts");
    {
        const auto st = stats();
        bool clicked = false; if (auto* a = st["clicks"].getArray()) for (auto& c : *a) clicked |= c.toString() == "u-1";
        check ("WEB: the click counted in the directory", clicked);
    }
    // a playlist
    s.url = base + "/playlist.pls"; s.uuid = {};
    net.setSource (0, s);
    run (0.3, false);
    check ("WEB: a playlist's stream plays", waitState (0, sk::RS_LIVE, 8), net.getStatus (0).msg);
    // AAC: refused, not asked again
    s.url = base + "/aac";
    net.setSource (0, s);
    check ("WEB: an AAC station refused", waitState (0, sk::RS_FAIL, 6), net.getStatus (0).msg);
    check ("WEB: ... said so", net.getStatus (0).msg.contains ("AAC"), net.getStatus (0).msg);

    // ---- the directory, the receivers' list
    juce::var q (new juce::DynamicObject()); q.getDynamicObject()->setProperty ("name", "mock");
    net.search (7, q);
    std::vector<juce::var> ans;
    for (int i = 0; i < 100 && ans.size() < 1; ++i) { run (0.05, false); for (auto& a : net.takeAnswers()) ans.push_back (a); }
    check ("directory: answered", ans.size() == 1 && (int) ans[0]["id"] == 7 && (bool) ans[0]["ok"]);
    if (! ans.empty()) check ("directory: HLS and AAC left out", ans[0]["res"].size() == 1 && ans[0]["res"][0]["name"].toString() == "Mock Jazz", juce::JSON::toString (ans[0]["res"], true));
    net.receivers (8, true);
    ans.clear();
    for (int i = 0; i < 100 && ans.size() < 1; ++i) { run (0.05, false); for (auto& a : net.takeAnswers()) ans.push_back (a); }
    check ("receivers: answered", ans.size() == 1 && (bool) ans[0]["ok"]);
    if (! ans.empty())
    {
        const auto& r = ans[0]["res"];
        check ("receivers: the list parsed (chunked), the offline one left out, the freest first",
               r.size() == 3 && r[0]["name"].toString() == "Mock emptiest" && (int) r[0]["max"] == 8 && r[1]["name"].toString() == "Mock receiver & antenna"
               && (int) r[1]["port"] == port + 1 && r[1]["loc"].toString() == "Paris" && std::abs ((double) r[1]["hi"] - 30000.0) < 1, juce::JSON::toString (r, true).substring (0, 400));
    }

    // ---- a shortwave receiver (track 2, RX 2)
    RadioNet::Source w; w.kind = 2; w.host = "127.0.0.1"; w.port = port + 1; w.khz = 9420.0; w.mode = "am"; w.rxName = "Mock receiver";
    net.setSource (1, w);
    check ("SW: the receiver plays", waitState (1, sk::RS_LIVE, 8), net.getStatus (1).msg);
    e.audition (1, 1.0f);
    L.clear(); R.clear(); run (2.0, true);
    check ("SW: its sound reaches the track (500 Hz)", std::abs (peakHz (R, R.size() - 16384, 16384, 48000, 200, 1500) - 500) <= 6, juce::String (peakHz (R, R.size() - 16384, 16384, 48000, 200, 1500)));
    e.radioRxInfo (ri);
    check ("SW: its S-meter", std::abs (ri[sk::RADIO_RX_INFO + 6] + 73.0f) < 0.5f, juce::String (ri[sk::RADIO_RX_INFO + 6]));
    check ("SW: its rate", net.getStatus (1).rate == 12001, juce::String (net.getStatus (1).rate));
    w.khz = 11850.0;
    net.setSource (1, w);
    run (1.5, false);
    L.clear(); R.clear(); run (1.0, true);
    check ("SW: retuned over its connection (900 Hz)", std::abs (peakHz (R, R.size() - 16384, 16384, 48000, 200, 1500) - 900) <= 6, juce::String (peakHz (R, R.size() - 16384, 16384, 48000, 200, 1500)));
    {
        const auto st = stats();
        check ("SW: one connection, two frequencies", (int) st["ws"] == 1 && st["mods"].size() == 2 && std::abs ((double) st["mods"][1] - 11850.0) < 0.01, juce::JSON::toString (st, true));
    }
    // the same receiver on RX 4: refused
    net.setSource (3, w);
    check ("SW: never two connections on one receiver", waitState (3, sk::RS_BUSY, 3), net.getStatus (3).msg);
    // a receiver with no channel free (RX 3): BUSY, not asked again
    RadioNet::Source b = w; b.port = port + 2;
    e.setParam (0, sk::P_A, 2.5f / 4.0f);              // (track 1 on RX 3)
    net.setSource (2, b);
    check ("SW: a busy receiver", waitState (2, sk::RS_BUSY, 6), net.getStatus (2).msg);
    run (5.0, false);
    check ("SW: ... not asked again", (int) stats()["busy"] == 1, juce::String ((int) stats()["busy"]));
    // standby: no track on RX 2 for 3 s: let go; back when a track plays it
    e.setParam (1, sk::P_MACH, sk::machineVal (sk::M_BD_VA));
    check ("standby: let go when no track plays it", waitState (1, sk::RS_STANDBY, 8), juce::String (net.getStatus (1).state));
    e.setParam (1, sk::P_MACH, sk::machineVal (sk::M_RADIO));
    check ("standby: back when a track plays it", waitState (1, sk::RS_LIVE, 8), juce::String (net.getStatus (1).state));
    check ("SW: a second connection after the standby", (int) stats()["ws"] == 2, juce::String ((int) stats()["ws"]));
    // the project: the sources written and read
    juce::MemoryOutputStream mo; net.write (mo);
    {
        RadioNet n2 (e);
        juce::MemoryInputStream mi (mo.getData(), mo.getDataSize(), false);
        n2.read (mi);
        check ("project: the sources kept", n2.getSource (1).kind == 2 && n2.getSource (1).khz == 11850.0 && n2.getSource (0).url == base + "/aac" && n2.getSource (2).port == port + 2);
        for (int k = 0; k < sk::RADIO_RX; ++k) n2.setSource (k, {});
    }
    std::printf ("%d failed\n", fails);
    return fails ? 1 : 0;
}
