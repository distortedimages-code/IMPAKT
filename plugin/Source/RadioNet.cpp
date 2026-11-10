#include "RadioNet.h"
#include "sk_radio_dec.h"

namespace
{
const char* const USER_AGENT = "IMPAKT/0.87";
juce::int64 nowMs() { return juce::Time::currentTimeMillis(); }

// ------------------------------------------------------------------ a URL's parts
struct Url
{
    juce::String scheme, host, path { "/" };
    int port = 80;
    bool ok = false;
};
Url parseUrl (juce::String u)
{
    Url r;
    u = u.trim();
    const int se = u.indexOf ("://");
    if (se <= 0) return r;
    r.scheme = u.substring (0, se).toLowerCase();
    auto rest = u.substring (se + 3);
    const int sl = rest.indexOfChar ('/');
    auto hp = sl >= 0 ? rest.substring (0, sl) : rest;
    r.path = sl >= 0 ? rest.substring (sl) : juce::String ("/");
    if (const int at = hp.lastIndexOfChar ('@'); at >= 0) hp = hp.substring (at + 1);   // (no credentials)
    r.port = r.scheme == "https" || r.scheme == "wss" ? 443 : 80;
    if (hp.startsWithChar ('['))
    {
        const int e = hp.indexOfChar (']');
        r.host = hp.substring (1, e);
        if (hp.substring (e + 1).startsWithChar (':')) r.port = hp.substring (e + 2).getIntValue();
    }
    else if (const int c = hp.lastIndexOfChar (':'); c >= 0) { r.host = hp.substring (0, c); r.port = hp.substring (c + 1).getIntValue(); }
    else r.host = hp;
    r.ok = r.host.isNotEmpty() && r.port > 0 && r.port < 65536;
    return r;
}

// ------------------------------------------------------------------ a plain socket (HTTP without TLS, the WebSocket)
// (raw sockets: no system policy in the way of a station that only speaks plain HTTP)
struct Sock
{
    juce::StreamingSocket s;
    std::vector<juce::uint8> pre;          // (bytes read past the headers)
    size_t preAt = 0;
    std::atomic<bool>* abort = nullptr;

    bool connect (const juce::String& host, int port, int timeoutMs) { return s.connect (host, port, timeoutMs); }
    bool aborted() const { return abort != nullptr && abort->load(); }
    // some bytes (> 0), none yet (0), or ended (-1)
    int readSome (juce::uint8* d, int max, int timeoutMs)
    {
        if (preAt < pre.size())
        {
            const int n = juce::jmin (max, (int) (pre.size() - preAt));
            memcpy (d, pre.data() + preAt, (size_t) n); preAt += (size_t) n;
            return n;
        }
        const int w = s.waitUntilReady (true, timeoutMs);
        if (w < 0) return -1;
        if (w == 0) return 0;
        const int n = s.read (d, max, false);
        return n > 0 ? n : -1;
    }
    bool readExact (juce::uint8* d, int n, int timeoutMs)
    {
        const auto end = nowMs() + timeoutMs;
        while (n > 0)
        {
            if (aborted() || nowMs() > end) return false;
            const int r = readSome (d, n, 100);
            if (r < 0) return false;
            d += r; n -= r;
        }
        return true;
    }
    bool writeAll (const void* p, int n)
    {
        auto* b = static_cast<const char*> (p);
        while (n > 0)
        {
            if (s.waitUntilReady (false, 2000) <= 0) return false;
            const int w = s.write (b, n);
            if (w <= 0) return false;
            b += w; n -= w;
        }
        return true;
    }
    bool writeStr (const juce::String& t) { const auto u = t.toStdString(); return writeAll (u.data(), (int) u.size()); }
};

struct HttpResp
{
    int status = 0;
    juce::StringPairArray h;
    juce::String location;
    bool chunked = false;
    juce::String header (const juce::String& k) const { return h.getValue (k.toLowerCase(), {}); }
};
// the request on a connected socket, its status line ("HTTP/1.x" or "ICY") and its headers
bool httpExchange (Sock& k, const Url& u, const juce::String& extra, HttpResp& r, int timeoutMs, juce::String& err, const juce::String& method = "GET")
{
    juce::String hostHdr = u.host.containsChar (':') ? "[" + u.host + "]" : u.host;
    if (u.port != 80) hostHdr << ":" << u.port;
    juce::String req;
    req << method << " " << u.path << " HTTP/1.1\r\nHost: " << hostHdr << "\r\nUser-Agent: " << USER_AGENT << "\r\nAccept: */*\r\n" << extra << "\r\n";
    if (! k.writeStr (req)) { err = "could not send the request"; return false; }
    // the headers: up to the empty line (64 KB at most)
    std::vector<juce::uint8> buf;
    const auto end = nowMs() + timeoutMs;
    size_t hdrEnd = 0;
    while (hdrEnd == 0)
    {
        if (k.aborted() || nowMs() > end) { err = "no answer"; return false; }
        juce::uint8 tmp[2048];
        const int n = k.readSome (tmp, sizeof (tmp), 100);
        if (n < 0) { err = "the connection closed"; return false; }
        buf.insert (buf.end(), tmp, tmp + n);
        for (size_t i = buf.size() >= (size_t) n + 3 ? buf.size() - (size_t) n - 3 : 0; i + 3 < buf.size(); ++i)
            if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') { hdrEnd = i + 4; break; }
        if (hdrEnd == 0 && buf.size() > 65536) { err = "a malformed answer"; return false; }
    }
    k.pre.assign (buf.begin() + (long) hdrEnd, buf.end()); k.preAt = 0;
    const auto text = juce::String::fromUTF8 ((const char*) buf.data(), (int) hdrEnd);
    auto lines = juce::StringArray::fromLines (text);
    if (lines.isEmpty()) { err = "a malformed answer"; return false; }
    const auto first = lines[0].trim();
    if (! first.startsWithIgnoreCase ("HTTP/") && ! first.startsWithIgnoreCase ("ICY")) { err = "not an HTTP answer"; return false; }
    r.status = first.fromFirstOccurrenceOf (" ", false, false).getIntValue();
    for (int i = 1; i < lines.size(); ++i)
    {
        const auto ln = lines[i];
        const int c = ln.indexOfChar (':');
        if (c <= 0) continue;
        r.h.set (ln.substring (0, c).trim().toLowerCase(), ln.substring (c + 1).trim());
    }
    r.location = r.header ("location");
    r.chunked = r.header ("transfer-encoding").containsIgnoreCase ("chunked");
    return true;
}

// ------------------------------------------------------------------ a byte stream: an HTTP body, plain or JUCE's (HTTPS)
struct Src
{
    virtual ~Src() = default;
    // some bytes (> 0), none yet (0), ended (-1)
    virtual int  read (juce::uint8* d, int max, int timeoutMs) = 0;
    virtual void cancel() {}
    juce::StringPairArray h;
    int status = 0;
    juce::String header (const juce::String& k) const { return h.getValue (k.toLowerCase(), {}); }
};
struct SockSrc : Src
{
    Sock k;
    bool chunked = false;
    juce::int64 left = 0;                  // (chunked: what is left of the chunk; -1: the last one passed)
    int read (juce::uint8* d, int max, int timeoutMs) override
    {
        if (! chunked) return k.readSome (d, max, timeoutMs);
        if (left < 0) return -1;
        if (left == 0)
        {
            // the chunk's size line ("1a2b\r\n"; after the first, its data's \r\n before it)
            juce::String line;
            const auto end = nowMs() + juce::jmax (timeoutMs, 5000);
            for (;;)
            {
                juce::uint8 c;
                if (! k.readExact (&c, 1, (int) juce::jmax ((juce::int64) 1, end - nowMs()))) return -1;
                if (c == '\n') { if (line.trim().isNotEmpty()) break; line.clear(); continue; }
                if (c != '\r') line += (char) c;
                if (line.length() > 64) return -1;
            }
            left = line.trim().upToFirstOccurrenceOf (";", false, false).getHexValue64();
            if (left <= 0) { left = -1; return -1; }
        }
        const int n = k.readSome (d, (int) juce::jmin ((juce::int64) max, left), timeoutMs);
        if (n > 0) left -= n;
        return n;
    }
    void cancel() override { k.s.close(); }
};
struct WebSrc : Src
{
    std::unique_ptr<juce::WebInputStream> w;
    std::atomic<bool> cancelled { false };
    int read (juce::uint8* d, int max, int) override
    {
        // (blocking until it has max bytes: kept small, so that a station at 32 kbit/s still answers in a quarter second)
        if (cancelled || w == nullptr) return -1;
        const int n = w->read (d, juce::jmin (max, 1024));
        return n > 0 ? n : -1;
    }
    void cancel() override { cancelled = true; if (w != nullptr) w->cancel(); }
};

std::unique_ptr<Src> openUrl (juce::String url, const juce::String& extra, int timeoutMs, std::atomic<bool>& abort, juce::String& err)
{
    for (int hop = 0; hop < 6; ++hop)
    {
        const auto u = parseUrl (url);
        if (! u.ok) { err = "a malformed address"; return {}; }
        if (u.scheme == "https")
        {
            auto s = std::make_unique<WebSrc>();
            s->w = std::make_unique<juce::WebInputStream> (juce::URL (url), false);
            s->w->withExtraHeaders (juce::String ("User-Agent: ") + USER_AGENT + "\r\n" + extra)
                 .withConnectionTimeout (timeoutMs).withNumRedirectsToFollow (5);
            if (! s->w->connect (nullptr) || s->w->isError()) { err = "could not connect"; return {}; }
            s->status = s->w->getStatusCode();
            const auto hh = s->w->getResponseHeaders();
            for (int i = 0; i < hh.size(); ++i) s->h.set (hh.getAllKeys()[i].toLowerCase(), hh.getAllValues()[i]);
            if (s->status >= 400) { err = "the server answered " + juce::String (s->status); return {}; }
            return s;
        }
        if (u.scheme != "http") { err = "unsupported address (" + u.scheme + ")"; return {}; }
        auto s = std::make_unique<SockSrc>();
        s->k.abort = &abort;
        if (! s->k.connect (u.host, u.port, timeoutMs)) { err = "could not reach " + u.host; return {}; }
        HttpResp r;
        if (! httpExchange (s->k, u, extra + "Connection: close\r\n", r, timeoutMs, err)) return {};
        if (r.status >= 300 && r.status < 400 && r.location.isNotEmpty())
        {
            url = r.location.startsWith ("/") ? u.scheme + "://" + u.host + (u.port != 80 ? ":" + juce::String (u.port) : juce::String()) + r.location : r.location;
            continue;
        }
        if (r.status != 200) { err = "the server answered " + juce::String (r.status); return {}; }
        s->status = r.status; s->h = r.h; s->chunked = r.chunked;
        return s;
    }
    err = "too many redirections";
    return {};
}

// a small document, whole (JSON, HTML): at most maxBytes
bool fetchText (const juce::String& url, juce::String& out, int maxBytes, int timeoutMs, juce::String& err)
{
    std::atomic<bool> abort { false };
    auto s = openUrl (url, {}, timeoutMs, abort, err);
    if (s == nullptr) return false;
    juce::MemoryOutputStream mo;
    juce::uint8 buf[8192];
    const auto end = nowMs() + timeoutMs * 3;
    for (;;)
    {
        if (nowMs() > end) { err = "too slow"; return false; }
        const int n = s->read (buf, sizeof (buf), 500);
        if (n < 0) break;
        mo.write (buf, (size_t) n);
        if ((int) mo.getDataSize() > maxBytes) { err = "too large"; return false; }
    }
    out = juce::String::fromUTF8 ((const char*) mo.getData(), (int) mo.getDataSize());
    return true;
}

juce::String urlDecode (const juce::String& s) { return juce::URL::removeEscapeChars (s.replace ("+", " ")); }
// the few HTML entities the lists use
juce::String unEntity (juce::String s)
{
    s = s.replace ("&lt;", "<").replace ("&gt;", ">").replace ("&quot;", "\"").replace ("&#39;", "'").replace ("&apos;", "'");
    for (int p = s.indexOf ("&#"); p >= 0; p = s.indexOf (p + 1, "&#"))
    {
        const int e = s.indexOfChar (p, ';');
        if (e < 0 || e - p > 8) continue;
        const auto num = s.substring (p + 2, e);
        const int c = num.startsWithIgnoreCase ("x") ? num.substring (1).getHexValue32() : num.getIntValue();
        if (c > 0 && c < 0x110000) s = s.substring (0, p) + juce::String::charToString ((juce::juce_wchar) c) + s.substring (e + 1);
    }
    return s.replace ("&amp;", "&");
}
juce::String esc (const juce::String& s) { return juce::URL::addEscapeChars (s, true); }
} // namespace

// ------------------------------------------------------------------ a source
juce::var RadioNet::Source::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("kind", kind == 1 ? "web" : kind == 2 ? "sw" : "");
    o->setProperty ("on", on);
    o->setProperty ("name", name); o->setProperty ("url", url); o->setProperty ("uuid", uuid);
    o->setProperty ("country", country); o->setProperty ("codec", codec); o->setProperty ("kbps", kbps);
    o->setProperty ("host", host); o->setProperty ("port", port); o->setProperty ("rxname", rxName);
    o->setProperty ("khz", khz); o->setProperty ("mode", mode);
    return juce::var (o);
}
RadioNet::Source RadioNet::Source::fromVar (const juce::var& v)
{
    Source s;
    const auto k = v.getProperty ("kind", "").toString();
    s.kind = k == "web" ? 1 : k == "sw" ? 2 : 0;
    s.on = (bool) v.getProperty ("on", true);
    s.name = v.getProperty ("name", "").toString().substring (0, 120);
    s.url = v.getProperty ("url", "").toString().substring (0, 1000);
    s.uuid = v.getProperty ("uuid", "").toString().substring (0, 64);
    s.country = v.getProperty ("country", "").toString().substring (0, 64);
    s.codec = v.getProperty ("codec", "").toString().substring (0, 16);
    s.kbps = (int) v.getProperty ("kbps", 0);
    s.host = v.getProperty ("host", "").toString().substring (0, 255);
    s.port = juce::jlimit (1, 65535, (int) v.getProperty ("port", 8073));
    s.rxName = v.getProperty ("rxname", "").toString().substring (0, 160);
    s.khz = juce::jlimit (0.0, 32000.0, (double) v.getProperty ("khz", 9420.0));
    s.mode = v.getProperty ("mode", "am").toString().toLowerCase();
    if (! juce::StringArray { "am", "amn", "usb", "lsb", "cw" }.contains (s.mode)) s.mode = "am";
    if (s.kind == 1 && s.url.isEmpty()) s.kind = 0;
    if (s.kind == 2 && s.host.isEmpty()) s.kind = 0;
    return s;
}
bool RadioNet::Source::sameStream (const Source& o) const
{
    if (kind != o.kind || on != o.on) return false;
    if (kind == 1) return url == o.url;
    if (kind == 2) return host == o.host && port == o.port;
    return true;
}

// ------------------------------------------------------------------ a receiver's thread
struct RadioNet::Rx : juce::Thread
{
    RadioNet& net;
    const int k;
    juce::CriticalSection lock;
    Source src; int srcVer = 0;
    Status st;
    std::atomic<bool> abort { false };
    std::atomic<juce::int64> usedAt { 0 }, lastData { 0 };
    juce::CriticalSection ioLock;
    Src* io = nullptr;
    std::unique_ptr<sk::RadioDec> dec;
    int failVer = -1, tries = 0, clickedVer = -1;
    juce::String lastErr;

    Rx (RadioNet& n, int kk) : juce::Thread ("IMPAKT radio " + juce::String (kk + 1)), net (n), k (kk)
    {
        dec = std::make_unique<sk::RadioDec>();
        dec->reset();
        usedAt = nowMs();
    }

    void setState (int s, const juce::String& msg = {}, bool keepTitle = false)
    {
        int kind;
        {
            const juce::ScopedLock sl (lock);
            const bool same = st.state == s && st.msg == msg;
            st.state = s; st.msg = msg;
            if (! keepTitle && s != sk::RS_LIVE) st.title.clear();
            kind = src.kind == 2 ? sk::RK_SW : sk::RK_WEB;
            if (same) { net.engine.radioSetState (k, s, kind); return; }
        }
        net.engine.radioSetState (k, s, kind);
        ++net.ver;
    }
    void setTitle (const juce::String& t)
    {
        { const juce::ScopedLock sl (lock); if (st.title == t) return; st.title = t; }
        ++net.ver;
    }
    void setInfo (int rate, int kbps)
    {
        { const juce::ScopedLock sl (lock); if (st.rate == rate && st.kbps == kbps) return; st.rate = rate; st.kbps = kbps; }
        ++net.ver;
    }
    bool changed (int v) { const juce::ScopedLock sl (lock); return srcVer != v; }
    Source current (int* v = nullptr) { const juce::ScopedLock sl (lock); if (v) *v = srcVer; return src; }
    bool used() const { return (net.use.load() >> k & 1) != 0 || nowMs() - usedAt.load() < net.standbyMs; }
    void setIo (Src* s) { const juce::ScopedLock sl (ioLock); io = s; }
    void cancelIo() { const juce::ScopedLock sl (ioLock); if (io != nullptr) io->cancel(); }
    // (the supervisor: a session to end now)
    bool shouldEnd (int v) { return abort.load() || threadShouldExit() || changed (v); }
    // waits ms, or less if the session should end
    void nap (int ms, int v) { const auto end = nowMs() + ms; while (nowMs() < end && ! shouldEnd (v)) wait (50); }

    // the decoded sound to the engine (it waits when the engine has enough in hand: the network waits with it; but when
    // the engine reads nothing for two seconds, the host not playing, the sound is dropped and the stream read on, so
    // that nothing piles up meanwhile)
    bool push (int v)
    {
        short pcm[2048];
        for (;;)
        {
            const int n = dec->take (pcm, 2048);
            if (n <= 0) return true;
            int done = 0;
            const auto until = nowMs() + 2000;
            while (done < n)
            {
                const int m = net.engine.radioPush (k, pcm + done, n - done);
                done += m;
                if (m == 0) { if (shouldEnd (v)) return false; if (nowMs() > until) break; wait (20); }
            }
        }
    }

    enum { R_CHANGED = 0, R_UNUSED, R_RETRY, R_FATAL };

    void run() override
    {
        while (! threadShouldExit())
        {
            int v; const Source s = current (&v);
            if (s.kind == 0) { setState (sk::RS_NONE); wait (200); continue; }
            if (! s.on) { setState (sk::RS_OFF); wait (200); continue; }
            if (failVer == v) { wait (200); continue; }
            if (! used()) { setState (sk::RS_STANDBY, "waiting for a track to play it"); wait (200); continue; }
            abort = false;
            const auto began = nowMs();
            const int r = s.kind == 1 ? runWeb (s, v) : runSw (s, v);
            if (threadShouldExit()) break;
            if (nowMs() - began > 60000) tries = 0;
            if (r == R_RETRY)
            {
                if (++tries > 3) { failVer = v; tries = 0; setState (sk::RS_FAIL, lastErr); continue; }
                const int secs = tries == 1 ? 3 : tries == 2 ? 10 : 30;
                setState (sk::RS_CONNECT, lastErr + " (again in " + juce::String (secs) + " s)");
                nap (secs * 1000, v);
            }
            else if (r == R_FATAL) { failVer = v; tries = 0; }
            else if (r == R_CHANGED) tries = 0;
        }
        setState (sk::RS_NONE);
    }

    // ---------------------------------------------------------------- WEB: an MP3 stream
    int runWeb (Source s, int v)
    {
        setState (sk::RS_CONNECT, "connecting");
        net.engine.radioFlush (k);
        dec->restart();
        setInfo (0, 0);
        if (clickedVer != v && s.uuid.isNotEmpty()) { clickedVer = v; net.countClick (s.uuid); }
        juce::String url = s.url, err;
        std::unique_ptr<Src> in;
        for (int hop = 0; ; ++hop)
        {
            in = openUrl (url, "Icy-MetaData: 1\r\n", 10000, abort, err);
            if (shouldEnd (v)) return R_CHANGED;
            if (in == nullptr) { lastErr = err; return R_RETRY; }
            const auto ct = in->header ("content-type").toLowerCase();
            if (ct.contains ("aac") || ct.contains ("ogg") || ct.contains ("opus") || ct.contains ("flac") || ct.contains ("x-mpegurl") && url.containsIgnoreCase (".m3u8"))
            {
                setState (sk::RS_FAIL, "this station streams " + (ct.contains ("aac") ? juce::String ("AAC") : ct.contains ("mpegurl") ? juce::String ("HLS") : juce::String ("OGG"))
                                       + ": only MP3 streams play here");
                return R_FATAL;
            }
            // a playlist (.m3u, .pls): its first stream
            if ((ct.contains ("mpegurl") || ct.contains ("scpls") || ct.startsWith ("text/")) && hop < 2)
            {
                juce::MemoryOutputStream mo; juce::uint8 b[4096];
                const auto end = nowMs() + 5000;
                while (mo.getDataSize() < 16384 && nowMs() < end) { const int n = in->read (b, sizeof (b), 300); if (n < 0) break; mo.write (b, (size_t) n); }
                const auto text = mo.toString();
                juce::String next;
                for (auto ln : juce::StringArray::fromLines (text))
                {
                    ln = ln.trim();
                    if (ln.startsWithIgnoreCase ("file") && ln.containsChar ('=')) ln = ln.fromFirstOccurrenceOf ("=", false, false).trim();
                    if (ln.startsWithIgnoreCase ("http://") || ln.startsWithIgnoreCase ("https://")) { next = ln; break; }
                }
                if (text.contains ("#EXT-X-")) { setState (sk::RS_FAIL, "this station streams HLS: only MP3 streams play here"); return R_FATAL; }
                if (next.isEmpty()) { setState (sk::RS_FAIL, "no stream in the station's playlist"); return R_FATAL; }
                url = next;
                continue;
            }
            break;
        }
        const int metaint = in->header ("icy-metaint").getIntValue();
        lastData = nowMs();
        setIo (in.get());
        setState (sk::RS_BUFFER, "");
        int toMeta = metaint;                  // (ICY: audio bytes to the next metadata block)
        int metaLen = -1; juce::MemoryOutputStream meta;
        juce::int64 bytes = 0;
        bool live = false;
        juce::uint8 buf[4096], audio[4096];
        int result = R_RETRY;
        for (;;)
        {
            if (shouldEnd (v)) { result = R_CHANGED; break; }
            if (! used()) { result = R_UNUSED; break; }
            const int n = in->read (buf, sizeof (buf), 250);
            if (n < 0)
            {
                if (shouldEnd (v)) { result = R_CHANGED; break; }
                if (! used()) { result = R_UNUSED; break; }
                lastErr = nowMs() - lastData > 12000 ? "the station stopped sending" : "the stream ended";
                break;
            }
            if (n == 0) { if (nowMs() - lastData > 12000) { lastErr = "the station stopped sending"; break; } continue; }
            lastData = nowMs(); bytes += n;
            // ICY: the metadata taken out of the audio
            int na = 0;
            for (int i = 0; i < n; ++i)
            {
                if (metaint <= 0 || toMeta > 0) { audio[na++] = buf[i]; if (metaint > 0) --toMeta; continue; }
                if (metaLen < 0) { metaLen = buf[i] * 16; meta.reset(); if (metaLen == 0) { metaLen = -1; toMeta = metaint; } continue; }
                meta.writeByte ((char) buf[i]);
                if ((int) meta.getDataSize() >= metaLen)
                {
                    auto m = juce::String::fromUTF8 ((const char*) meta.getData(), (int) meta.getDataSize());
                    if (m.isEmpty() && meta.getDataSize() > 0) m = juce::String ((const char*) meta.getData(), meta.getDataSize());
                    const int p = m.indexOf ("StreamTitle='");
                    if (p >= 0) setTitle (m.substring (p + 13).upToFirstOccurrenceOf ("';", false, false).trim().substring (0, 160));
                    metaLen = -1; toMeta = metaint;
                }
            }
            int off = 0;
            while (off < na)
            {
                const int m = juce::jmin (na - off, dec->inRoom());
                if (m > 0) { dec->mp3Feed (audio + off, m); off += m; }
                if (! push (v)) { result = R_CHANGED; goto done; }
                while (dec->mp3Work()) if (! push (v)) { result = R_CHANGED; goto done; }
                if (m <= 0 && dec->inRoom() <= 0) break;
            }
            if (! live && dec->frames > 0) { live = true; setState (sk::RS_LIVE, {}, true); setInfo (dec->rate, dec->kbps); }
            if (live && (dec->frames & 255) == 0) setInfo (dec->rate, dec->kbps);
            if (! live && bytes > 300000) { setState (sk::RS_FAIL, "this is not an MP3 stream"); result = R_FATAL; break; }
        }
    done:
        setIo (nullptr);
        in->cancel();
        if (result == R_UNUSED) setState (sk::RS_STANDBY, "waiting for a track to play it");
        return result;
    }

    // ---------------------------------------------------------------- SW: a receiver's WebSocket
    struct Ws
    {
        Sock& k;
        juce::Random rnd;
        bool send (int opcode, const void* data, int n)
        {
            std::vector<juce::uint8> f;
            f.push_back ((juce::uint8) (0x80 | opcode));
            if (n < 126) f.push_back ((juce::uint8) (0x80 | n));
            else { f.push_back (0x80 | 126); f.push_back ((juce::uint8) (n >> 8)); f.push_back ((juce::uint8) n); }
            juce::uint8 mk[4]; for (auto& m : mk) m = (juce::uint8) rnd.nextInt (256);
            f.insert (f.end(), mk, mk + 4);
            auto* d = static_cast<const juce::uint8*> (data);
            for (int i = 0; i < n; ++i) f.push_back (d[i] ^ mk[i & 3]);
            return k.writeAll (f.data(), (int) f.size());
        }
        bool text (const juce::String& t) { const auto u = t.toStdString(); return send (1, u.data(), (int) u.size()); }
        // a whole message (0: none came in timeoutMs, -1: closed or broken)
        int recv (std::vector<juce::uint8>& msg, int timeoutMs)
        {
            msg.clear();
            for (;;)
            {
                juce::uint8 h[2];
                const int r = k.readSome (h, 1, msg.empty() ? timeoutMs : 5000);
                if (r <= 0) return msg.empty() && r == 0 ? 0 : -1;
                if (! k.readExact (h + 1, 1, 5000)) return -1;
                const int op = h[0] & 15; const bool fin = (h[0] & 0x80) != 0;
                juce::uint64 len = h[1] & 127;
                if (len == 126) { juce::uint8 e[2]; if (! k.readExact (e, 2, 5000)) return -1; len = (juce::uint64) (e[0] << 8 | e[1]); }
                else if (len == 127) { juce::uint8 e[8]; if (! k.readExact (e, 8, 5000)) return -1; len = 0; for (auto b : e) len = len << 8 | b; }
                if (len > (1u << 22)) return -1;
                juce::uint8 mk[4] = {};
                if (h[1] & 0x80) { if (! k.readExact (mk, 4, 5000)) return -1; }
                std::vector<juce::uint8> p ((size_t) len);
                if (len > 0 && ! k.readExact (p.data(), (int) len, 10000)) return -1;
                if (h[1] & 0x80) for (size_t i = 0; i < p.size(); ++i) p[i] ^= mk[i & 3];
                if (op == 8) return -1;                                              // close
                if (op == 9) { send (10, p.data(), (int) p.size()); continue; }       // ping: pong
                if (op == 10) continue;
                msg.insert (msg.end(), p.begin(), p.end());
                if (fin) return 1;
            }
        }
    };
    static juce::String modCmd (const Source& s, double foff)
    {
        int lo = -4900, hi = 4900;
        if (s.mode == "amn") { lo = -2500; hi = 2500; }
        else if (s.mode == "usb") { lo = 300; hi = 2700; }
        else if (s.mode == "lsb") { lo = -2700; hi = -300; }
        else if (s.mode == "cw") { lo = 300; hi = 700; }
        const double f = juce::jmax (0.0, s.khz - foff);
        return "SET mod=" + s.mode + " low_cut=" + juce::String (lo) + " high_cut=" + juce::String (hi) + " freq=" + juce::String (f, 3);
    }
    int runSw (Source s, int v)
    {
        setState (sk::RS_CONNECT, "connecting to " + (s.rxName.isNotEmpty() ? s.rxName : s.host));
        net.engine.radioFlush (k);
        net.engine.radioMeter (k, -999.0f);
        dec->restart();
        setInfo (0, 0);
        // never two connections on the same receiver
        for (int o = 0; o < sk::RADIO_RX; ++o)
        {
            if (o == k) continue;
            const Source os = net.rx[o]->current();
            if (os.kind == 2 && os.on && os.host.equalsIgnoreCase (s.host) && os.port == s.port && net.rx[o]->getStatusState() >= sk::RS_CONNECT && net.rx[o]->getStatusState() <= sk::RS_TUNING)
            {
                setState (sk::RS_BUSY, "RX " + juce::String (o + 1) + " already listens to this receiver");
                return R_FATAL;
            }
        }
        juce::String host = s.host; int port = s.port;
        Sock k2; k2.abort = &abort;
        bool up = false;
        for (int hop = 0; hop < 4 && ! up; ++hop)
        {
            k2.s.close(); k2.pre.clear(); k2.preAt = 0;
            if (! k2.connect (host, port, 8000)) { lastErr = shouldEnd (v) ? juce::String() : "could not reach the receiver"; return shouldEnd (v) ? R_CHANGED : R_RETRY; }
            juce::uint8 key[16]; juce::Random r; for (auto& b : key) b = (juce::uint8) r.nextInt (256);
            const auto ts = juce::String ((juce::int64) (juce::Time::currentTimeMillis() / 1000) & 0x7fffffff);
            Url u; u.host = host; u.port = port; u.path = "/" + ts + "/SND"; u.ok = true;
            HttpResp hr; juce::String err;
            if (! httpExchange (k2, u, "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + juce::Base64::toBase64 (key, 16)
                                       + "\r\nSec-WebSocket-Version: 13\r\n", hr, 8000, err))
            { lastErr = err; return shouldEnd (v) ? R_CHANGED : R_RETRY; }
            if (hr.status == 101) { up = true; break; }
            if (hr.status >= 300 && hr.status < 400 && hr.location.isNotEmpty())
            {
                const auto nu = parseUrl (hr.location);
                if (! nu.ok) break;
                host = nu.host; port = nu.port;
                continue;
            }
            if (hr.status == 404) { setState (sk::RS_FAIL, "the receiver is offline"); return R_FATAL; }
            lastErr = "the receiver answered " + juce::String (hr.status);
            return R_RETRY;
        }
        if (! up) { lastErr = "the receiver could not be reached"; return R_RETRY; }
        Ws ws { k2 };
        setIo (nullptr);
        ws.text ("SET auth t=kiwi p=");
        setState (sk::RS_BUFFER, "");
        double rate = 0.0, foff = 0.0;
        bool ready = false, live = false;
        juce::int64 lastKeep = nowMs(), lastMsg = nowMs(), lastTune = 0, liveAt = 0;
        juce::String pendingMod;
        std::vector<juce::uint8> m;
        int result = R_RETRY;
        lastErr = "the connection to the receiver broke";
        for (;;)
        {
            if (abort.load() || threadShouldExit()) { result = R_CHANGED; break; }
            if (! used()) { result = R_UNUSED; break; }
            // a new frequency on the same receiver: tuned over the same connection (five times a second at most)
            if (changed (v))
            {
                int nv; const Source ns = current (&nv);
                if (! ns.sameStream (s)) { result = R_CHANGED; break; }
                s = ns; v = nv;
                pendingMod = modCmd (s, foff);
                setState (sk::RS_TUNING, {}, false);
            }
            if (pendingMod.isNotEmpty() && ready && nowMs() - lastTune >= 200)
            {
                ws.text (pendingMod); pendingMod.clear(); lastTune = nowMs();
                net.engine.radioFlush (k);
                live = false;
            }
            if (nowMs() - lastKeep >= 1000) { ws.text ("SET keepalive"); lastKeep = nowMs(); }
            const int r = ws.recv (m, 250);
            if (r < 0)
            {
                // the receiver closed it: after playing a while, it ended it (its time limit, its inactivity): not again
                if (liveAt > 0 && nowMs() - liveAt > 20000) { setState (sk::RS_FAIL, "the receiver ended the connection"); result = R_FATAL; }
                break;
            }
            if (r == 0) { if (nowMs() - lastMsg > 15000) { lastErr = "the receiver stopped sending"; break; } continue; }
            lastMsg = nowMs();
            if (m.size() < 3) continue;
            const juce::String tag ((const char*) m.data(), 3);
            if (tag == "MSG")
            {
                const auto body = juce::String::fromUTF8 ((const char*) m.data() + 4, juce::jmax (0, (int) m.size() - 4));
                for (const auto& kv : juce::StringArray::fromTokens (body, " ", ""))
                {
                    const auto key = kv.upToFirstOccurrenceOf ("=", false, false), val = kv.fromFirstOccurrenceOf ("=", false, false);
                    if (key == "too_busy") { setState (sk::RS_BUSY, "all its channels are taken (" + val + ")"); result = R_FATAL; goto done; }
                    if (key == "badp" && val != "0")
                    {
                        setState (sk::RS_BUSY, val == "1" ? "no channel free without a password" : val == "5" ? "it takes one connection from here at a time" : "it refused the connection");
                        result = R_FATAL; goto done;
                    }
                    if (key == "down") { setState (sk::RS_FAIL, "the receiver is down"); result = R_FATAL; goto done; }
                    if (key == "redirect")
                    {
                        const auto nu = parseUrl (urlDecode (val));
                        if (nu.ok) { const juce::ScopedLock sl (lock); src.host = nu.host; src.port = nu.port; ++srcVer; }
                        result = R_CHANGED; goto done;
                    }
                    if (key == "audio_rate") ws.text ("SET AR OK in=" + juce::String (val.getIntValue()) + " out=48000");
                    if (key == "freq_offset") foff = val.getDoubleValue();
                    if (key == "sample_rate")
                    {
                        rate = val.getDoubleValue();
                        ws.text ("SET squelch=0 max=0");
                        ws.text ("SET genattn=0");
                        ws.text ("SET gen=0 mix=-1");
                        ws.text ("SET ident_user=IMPAKT");
                        ws.text (modCmd (s, foff)); lastTune = nowMs();
                        ws.text ("SET agc=1 hang=0 thresh=-100 slope=6 decay=1000 manGain=50");
                        ws.text ("SET compression=1");
                        ws.text ("SET keepalive"); lastKeep = nowMs();
                        ready = true;
                        setInfo ((int) (rate + 0.5), 0);
                    }
                }
            }
            else if (tag == "SND" && m.size() >= 10 && rate > 0.0)
            {
                const int flags = m[3];
                const int smeter = m[8] << 8 | m[9];
                const juce::uint8* d = m.data() + 10; const int n = (int) m.size() - 10;
                if (flags & 0x10) dec->adpcmFeed (d, n, (float) rate);
                else
                {
                    std::vector<short> pcm ((size_t) (n / 2));
                    const bool le = (flags & 0x80) != 0;      // (big-endian, unless it says otherwise)
                    for (int i = 0; i < n / 2; ++i) pcm[(size_t) i] = (short) (le ? d[2 * i + 1] << 8 | d[2 * i] : d[2 * i] << 8 | d[2 * i + 1]);
                    dec->pcmFeed (pcm.data(), n / 2, 1, (float) rate);
                }
                net.engine.radioMeter (k, 0.1f * (float) smeter - 127.0f);
                if (! push (v)) { result = R_CHANGED; break; }
                if (! live && pendingMod.isEmpty()) { live = true; if (liveAt == 0) liveAt = nowMs(); setState (sk::RS_LIVE); }
            }
        }
    done:
        if (k2.s.isConnected()) { const juce::uint8 code[2] = { 0x03, 0xe9 }; ws.send (8, code, 2); }   // (going away)
        k2.s.close();
        net.engine.radioMeter (k, -999.0f);
        if (result == R_UNUSED) setState (sk::RS_STANDBY, "waiting for a track to play it");
        return result;
    }
    int getStatusState() { const juce::ScopedLock sl (lock); return st.state; }
};

// ------------------------------------------------------------------ the supervisor: ends the sessions that should end
struct RadioNet::Watch : juce::Thread
{
    RadioNet& net;
    explicit Watch (RadioNet& n) : juce::Thread ("IMPAKT radio watch"), net (n) {}
    void run() override
    {
        while (! threadShouldExit())
        {
            const int u = net.use.load();
            for (int k = 0; k < sk::RADIO_RX; ++k)
            {
                auto& r = *net.rx[k];
                if (u >> k & 1) r.usedAt = nowMs();
                // (an HTTPS read blocks: a new source, no track for a while, or a silent station end it from here)
                int v; r.current (&v);
                const bool stall = r.lastData.load() > 0 && nowMs() - r.lastData.load() > 15000;
                if (r.changed (v) || ! r.used() || stall || r.threadShouldExit()) r.cancelIo();
            }
            wait (200);
        }
    }
};

// ------------------------------------------------------------------ RadioNet
RadioNet::RadioNet (sk::Engine& e) : engine (e)
{
    for (int k = 0; k < sk::RADIO_RX; ++k) rx[k] = std::make_unique<Rx> (*this, k);
    for (auto& r : rx) r->startThread();
    pool = std::make_unique<juce::ThreadPool> (2);
    sup = std::make_unique<Watch> (*this);
    sup->startThread();
}
RadioNet::~RadioNet()
{
    sup->signalThreadShouldExit();
    for (auto& r : rx) { r->signalThreadShouldExit(); r->abort = true; r->cancelIo(); r->notify(); }
    for (auto& r : rx) r->stopThread (4000);
    sup->stopThread (1000);
    pool->removeAllJobs (true, 4000);
}

void RadioNet::setSource (int k, const Source& s)
{
    if (k < 0 || k >= sk::RADIO_RX) return;
    auto& r = *rx[k];
    {
        const juce::ScopedLock sl (r.lock);
        const bool same = r.src.toVar().toString() == s.toVar().toString();   // (cheap enough: a few fields)
        if (same && r.failVer != r.srcVer) return;
        r.src = s; ++r.srcVer;
        r.st.msg.clear();
    }
    r.usedAt = nowMs();
    r.cancelIo();
    r.notify();
    ++ver;
}
RadioNet::Source RadioNet::getSource (int k) const { if (k < 0 || k >= sk::RADIO_RX) return {}; return rx[k]->current(); }
RadioNet::Status RadioNet::getStatus (int k) const
{
    if (k < 0 || k >= sk::RADIO_RX) return {};
    const juce::ScopedLock sl (rx[k]->lock);
    return rx[k]->st;
}

void RadioNet::answer (const juce::var& v) { const juce::ScopedLock sl (ansLock); answers.push_back (v); ++ver; }
std::vector<juce::var> RadioNet::takeAnswers() { const juce::ScopedLock sl (ansLock); std::vector<juce::var> a; a.swap (answers); return a; }

// the directory's server: one of its mirrors, picked at random from its list (else its round-robin name)
juce::String RadioNet::directoryHost()
{
    if (dirOverride.isNotEmpty()) return dirOverride;
    { const juce::ScopedLock sl (dirLock); if (dirHost.isNotEmpty()) return dirHost; }
    juce::String text, err, pick = "https://all.api.radio-browser.info";
    if (fetchText ("https://all.api.radio-browser.info/json/servers", text, 65536, 8000, err))
    {
        const auto j = juce::JSON::parse (text);
        juce::StringArray names;
        if (auto* a = j.getArray()) for (const auto& o : *a) { const auto n = o.getProperty ("name", "").toString(); if (n.isNotEmpty() && ! names.contains (n)) names.add (n); }
        if (! names.isEmpty()) pick = "https://" + names[juce::Random::getSystemRandom().nextInt (names.size())];
    }
    const juce::ScopedLock sl (dirLock);
    dirHost = pick;
    return dirHost;
}

void RadioNet::countClick (const juce::String& uuid)
{
    pool->addJob ([this, uuid] {
        juce::String text, err;
        fetchText (directoryHost() + "/json/url/" + esc (uuid), text, 65536, 8000, err);
    });
}

void RadioNet::search (int id, const juce::var& q)
{
    pool->addJob ([this, id, q] {
        auto* o = new juce::DynamicObject(); juce::var res (o);
        o->setProperty ("id", id); o->setProperty ("q", "search");
        juce::String url = directoryHost() + "/json/stations/search?codec=MP3&hidebroken=true&limit=80";
        const auto name = q.getProperty ("name", "").toString().trim(), tag = q.getProperty ("tag", "").toString().trim(),
                   cc = q.getProperty ("cc", "").toString().trim().toUpperCase(), order = q.getProperty ("order", "clickcount").toString();
        if (name.isNotEmpty()) url << "&name=" << esc (name);
        if (tag.isNotEmpty()) url << "&tag=" << esc (tag);
        if (cc.length() == 2) url << "&countrycode=" << esc (cc);
        url << "&order=" << (juce::StringArray { "clickcount", "votes", "random", "name", "bitrate", "clicktrend" }.contains (order) ? order : juce::String ("clickcount"));
        if (order != "name" && order != "random") url << "&reverse=true";
        url << "&offset=" << juce::jlimit (0, 100000, (int) q.getProperty ("offset", 0));
        juce::String text, err;
        if (! fetchText (url, text, 4 << 20, 10000, err))
        {
            // (that mirror is down: the directory's round-robin name, and another mirror picked next time)
            const auto host = directoryHost();
            if (dirOverride.isEmpty()) { const juce::ScopedLock sl (dirLock); dirHost.clear(); }
            const juce::String alt = "https://all.api.radio-browser.info";
            if (dirOverride.isNotEmpty() || host == alt || ! fetchText (url.replace (host, alt), text, 4 << 20, 10000, err))
            { o->setProperty ("ok", false); o->setProperty ("err", err); answer (res); return; }
        }
        const auto j = juce::JSON::parse (text);
        juce::Array<juce::var> out;
        if (auto* a = j.getArray())
            for (const auto& st : *a)
            {
                if ((int) st.getProperty ("hls", 0) != 0) continue;
                if (! st.getProperty ("codec", "").toString().containsIgnoreCase ("mp3")) continue;
                auto u = st.getProperty ("url_resolved", "").toString();
                if (u.isEmpty()) u = st.getProperty ("url", "").toString();
                if (! u.startsWithIgnoreCase ("http")) continue;
                auto* s = new juce::DynamicObject();
                s->setProperty ("name", st.getProperty ("name", "").toString().trim().substring (0, 120));
                s->setProperty ("url", u);
                s->setProperty ("uuid", st.getProperty ("stationuuid", "").toString());
                s->setProperty ("cc", st.getProperty ("countrycode", "").toString());
                s->setProperty ("country", st.getProperty ("country", "").toString().substring (0, 64));
                s->setProperty ("tags", st.getProperty ("tags", "").toString().substring (0, 120));
                s->setProperty ("kbps", (int) st.getProperty ("bitrate", 0));
                s->setProperty ("votes", (int) st.getProperty ("votes", 0));
                out.add (juce::var (s));
            }
        o->setProperty ("ok", true); o->setProperty ("res", out);
        answer (res);
    });
}

// the receivers' public list: each one's fields in HTML comments ("<!-- name=... -->") after its "cl-info" block, its
// address the link after them
void RadioNet::receivers (int id, bool fresh)
{
    pool->addJob ([this, id, fresh] {
        auto* o = new juce::DynamicObject(); juce::var res (o);
        o->setProperty ("id", id); o->setProperty ("q", "receivers");
        {
            const juce::ScopedLock sl (dirLock);
            if (! fresh && rxList.isArray() && nowMs() - rxListAt < 600000) { o->setProperty ("ok", true); o->setProperty ("res", rxList); answer (res); return; }
        }
        juce::String text, err;
        if (! fetchText (listOverride.isNotEmpty() ? listOverride : juce::String ("http://kiwisdr.com/public/"), text, 16 << 20, 15000, err))
        { o->setProperty ("ok", false); o->setProperty ("err", err); answer (res); return; }
        juce::Array<juce::var> out;
        int at = 0;
        for (;;)
        {
            // (its class in either quotes)
            const int p = text.indexOf (at, "cl-info");
            if (p < 0) break;
            const int q = text.indexOf (p + 7, "cl-info");
            const auto block = text.substring (p, q < 0 ? text.length() : q);
            at = p + 10;
            juce::StringPairArray f;
            int c = 0;
            for (;;)
            {
                const int a = block.indexOf (c, "<!-- ");
                if (a < 0) break;
                const int b = block.indexOf (a, " -->");
                if (b < 0) break;
                const auto kv = block.substring (a + 5, b);
                if (kv.containsChar ('=')) f.set (kv.upToFirstOccurrenceOf ("=", false, false).trim(), kv.fromFirstOccurrenceOf ("=", false, false).trim());
                c = b + 4;
            }
            auto get = [&] (const char* key) { return f[key]; };
            juce::String link = block.fromFirstOccurrenceOf ("<a href=", false, false).trimCharactersAtStart ("'\"").upToFirstOccurrenceOf ("'", false, false).upToFirstOccurrenceOf ("\"", false, false).trim();
            if (link.isEmpty()) link = block.fromLastOccurrenceOf ("-->", false, false).fromFirstOccurrenceOf (">", false, false).upToFirstOccurrenceOf ("</a>", false, false).trim();
            if (link.isEmpty()) link = get ("url");
            const auto u = parseUrl (link);
            if (! u.ok || u.scheme != "http") continue;
            if (get ("offline") == "yes" || get ("status") == "offline" || get ("status") == "inactive") continue;
            auto* r = new juce::DynamicObject();
            r->setProperty ("name", unEntity (get ("name")).substring (0, 160));
            r->setProperty ("host", u.host); r->setProperty ("port", u.port);
            r->setProperty ("loc", get ("loc").substring (0, 80));
            r->setProperty ("antenna", get ("antenna").substring (0, 120));
            r->setProperty ("users", get ("users").getIntValue()); r->setProperty ("max", get ("users_max").getIntValue());
            const auto bands = get ("bands");
            r->setProperty ("lo", bands.upToFirstOccurrenceOf ("-", false, false).getDoubleValue() / 1000.0);
            r->setProperty ("hi", bands.fromFirstOccurrenceOf ("-", false, false).getDoubleValue() / 1000.0);
            const auto gps = get ("gps").removeCharacters ("()");
            r->setProperty ("lat", gps.upToFirstOccurrenceOf (",", false, false).trim().getDoubleValue());
            r->setProperty ("lon", gps.fromFirstOccurrenceOf (",", false, false).trim().getDoubleValue());
            out.add (juce::var (r));
        }
        // the free ones first
        std::stable_sort (out.begin(), out.end(), [] (const juce::var& a, const juce::var& b) {
            const int fa = (int) a["max"] - (int) a["users"], fb = (int) b["max"] - (int) b["users"];
            return fa > fb; });
        if (out.size() > 500) out.removeRange (500, out.size() - 500);
        {
            const juce::ScopedLock sl (dirLock);
            rxList = out; rxListAt = nowMs();
        }
        o->setProperty ("ok", true); o->setProperty ("res", out);
        answer (res);
    });
}

// ------------------------------------------------------------------ the project: "RAD1", how many, each one's JSON
static constexpr int radMagic = 0x31444152;   // "RAD1"
void RadioNet::write (juce::OutputStream& out) const
{
    out.writeInt (radMagic);
    out.writeInt (sk::RADIO_RX);
    for (int k = 0; k < sk::RADIO_RX; ++k) out.writeString (juce::JSON::toString (getSource (k).toVar(), true));
}
void RadioNet::read (juce::InputStream& in)
{
    clearAll();
    if (in.getNumBytesRemaining() < 8 || in.readInt() != radMagic) return;
    const int n = in.readInt();
    for (int k = 0; k < n && k < 16 && ! in.isExhausted(); ++k)
    {
        const auto s = in.readString();
        if (k < sk::RADIO_RX) setSource (k, Source::fromVar (juce::JSON::parse (s)));
    }
}
void RadioNet::clearAll() { for (int k = 0; k < sk::RADIO_RX; ++k) if (getSource (k).kind != 0) setSource (k, Source {}); }
