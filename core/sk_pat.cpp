// IMPAKT — patterns, Elektron style: 8 banks of 16 (A01 .. H16). Own implementation.
//
// The pattern playing lives in the tracks (tr[]); the others in their slots (pats[]): per track its sounds (every
// parameter), its steps (trigs, ratchets, REC trigs, lock-only steps), length, speed and quantiser, its p-locks as a
// list. A change waits for the end of the pattern: its CHG length (16ths, AUTO = its longest track), counted from where
// it started (patStart, in effective 16ths); or it comes at once, from the pattern's first step (START) or where the
// playhead is (JUMP). A chain plays its patterns in turn and starts again. The mutes stay the project's (they do not
// change with the pattern). A pattern never written takes the sounds of the one it comes from, and no trigs.
//
// The edits (the parameter, trig, lock... setters) go to editPat: the pattern playing goes to the tracks, another to
// its slot. The page tells the engine which pattern it edits; when the engine changes pattern on its own (the end of a
// pattern, a chain), the page's edits still land in the pattern it shows until it has followed (patEdit).
#include "sk_engine.h"

namespace sk {

static const float PAT_SPEEDS[NUM_SPEEDS] = { 0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };

// a slot written for the first time: the sounds and quantiser of the pattern playing, no trigs, 16 steps at 1x
static void slotFresh (PatSlot& sl, const Track* tr)
{
    sl.used = 1; sl.chg = 0; sl.nlk = 0;
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        PatTrack& k = sl.trk[t];
        for (int p = 0; p < NUM_PARAMS; ++p) k.p[p] = tr[t].p[p];
        for (int s = 0; s < NUM_STEPS; ++s) { k.trig[s] = 0; k.rat[s] = 1; k.rec[s] = 0; k.tlk[s] = 0; }
        k.len = 16; k.spd = 4; k.q = (u8) tr[t].q; k.pad = 0;
    }
}

PatSlot* Engine::patTarget (int t)
{
    if (editPat == curPat || editPat < 0 || editPat >= NUM_PATS || t < 0 || t >= NUM_TRACKS) return nullptr;
    PatSlot& sl = pats[editPat];
    if (! sl.used) slotFresh (sl, tr);
    return &sl;
}

void Engine::slotLock (PatSlot& sl, int t, int s, int p, float v)
{
    for (int i = 0; i < sl.nlk; ++i)
    {
        PatLock& l = sl.lk[i];
        if (l.t != t || l.s != s || l.p != p) continue;
        if (v < 0.0f) sl.lk[i] = sl.lk[--sl.nlk];
        else l.v = clampf (v, 0.0f, 1.0f);
        return;
    }
    if (v >= 0.0f && sl.nlk < PAT_LOCKS) sl.lk[sl.nlk++] = { (u8) t, (u8) s, (unsigned short) p, clampf (v, 0.0f, 1.0f) };
}

// ----------------------------------------------------------------- the page's calls
void Engine::patCur (int i)
{
    if (i < 0 || i >= NUM_PATS) return;
    curPat = editPat = i; queued = -1;
    patSerial++;
}

void Engine::patAll()
{
    for (int i = 0; i < NUM_PATS; ++i) { pats[i].used = 0; pats[i].chg = 0; pats[i].nlk = 0; }
    curPat = editPat = 0; queued = -1; qMode = 0; curChg = 0;
    chainLen = 0; chainPos = -1; patStart = 0.0;
    patSerial++;
}

void Engine::patEdit (int i) { if (i >= 0 && i < NUM_PATS) editPat = i; }

void Engine::patClear (int i)
{
    if (i < 0 || i >= NUM_PATS || i == curPat) return;
    pats[i].used = 0; pats[i].chg = 0; pats[i].nlk = 0;
}

void Engine::patQueue (int i, int mode)
{
    if (i < 0 || i >= NUM_PATS) return;
    if (! running) { patSwitch (i, 0.0); queued = -1; return; }   // stopped: at once (it starts from its first step)
    queued = i; qMode = mode < 0 ? 0 : (mode > 2 ? 2 : mode);
}

void Engine::patChain (int k, int i) { if (k >= 0 && k < CHAIN_MAX && i >= 0 && i < NUM_PATS) chain[k] = i; }

void Engine::patChainLen (int n)
{
    chainLen = n < 0 ? 0 : (n > CHAIN_MAX ? CHAIN_MAX : n);
    if (chainLen > 0) queued = -1;
    // where the chain is: at the pattern playing when it is one of its own (a project loaded), else before its first
    chainPos = -1;
    for (int k = 0; k < chainLen; ++k) if (chain[k] == curPat) { chainPos = k; break; }
}

void Engine::patChg (int n)
{
    n = n < 0 ? 0 : (n > 1024 ? 1024 : n);
    if (editPat == curPat) curChg = n;
    else if (editPat >= 0 && editPat < NUM_PATS) { PatSlot& sl = pats[editPat]; if (! sl.used) slotFresh (sl, tr); sl.chg = n; }
}

// ----------------------------------------------------------------- internals
double Engine::patChangeLen() const
{
    if (curChg > 0) return (double) curChg;
    double m = 1.0;
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        const double d = (double) tr[t].len / (double) PAT_SPEEDS[tr[t].spd];
        if (d > m) m = d;
    }
    return (double) (int) (m + 1.0 - 1e-6);
}

int Engine::patNext() const
{
    if (queued >= 0) return queued;
    if (chainLen > 0) return chain[(chainPos + 1) % chainLen];
    return -1;
}

void Engine::patSave (int i)
{
    if (i < 0 || i >= NUM_PATS) return;
    PatSlot& sl = pats[i];
    sl.used = 1; sl.chg = curChg; sl.nlk = 0;
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        const Track& k = tr[t];
        PatTrack& d = sl.trk[t];
        // a knob still gliding: where it goes
        for (int p = 0; p < NUM_PARAMS; ++p) { const int b = slewBit (p); d.p[p] = b >= 0 && ((k.slew >> b) & 1u) ? k.pt[p] : k.p[p]; }
        for (int s = 0; s < NUM_STEPS; ++s)
        {
            d.trig[s] = k.trig[s]; d.rat[s] = k.rat[s]; d.rec[s] = k.rec[s]; d.tlk[s] = k.tlk[s];
            for (int p = 0; p < NUM_PARAMS; ++p)
                if (k.lock[s][p] >= 0.0f && sl.nlk < PAT_LOCKS) sl.lk[sl.nlk++] = { (u8) t, (u8) s, (unsigned short) p, k.lock[s][p] };
        }
        d.len = (u8) k.len; d.spd = (u8) k.spd; d.q = (u8) k.q;
    }
}

void Engine::patLoad (int i)
{
    if (i < 0 || i >= NUM_PATS) return;
    const PatSlot& sl = pats[i];
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        Track& k = tr[t];
        for (int s = 0; s < NUM_STEPS; ++s)
        {
            k.trig[s] = 0; k.rat[s] = 1; k.rec[s] = 0; k.tlk[s] = 0;
            for (int p = 0; p < NUM_PARAMS; ++p) k.lock[s][p] = -1.0f;
        }
        k.rtLeft = 0; k.jAcc = 0;                                // (the jumps start again with the pattern)
        if (! sl.used) { k.len = 16; k.spd = 4; continue; }     // never written: the sounds stay, no trigs
        const PatTrack& d = sl.trk[t];
        for (int p = 0; p < NUM_PARAMS; ++p) setParamLive (t, p, d.p[p]);
        for (int s = 0; s < NUM_STEPS; ++s) { k.trig[s] = d.trig[s]; k.rat[s] = d.rat[s] ? d.rat[s] : 1; k.rec[s] = d.rec[s]; k.tlk[s] = d.tlk[s]; }
        k.len = mini (maxi ((int) d.len, 1), NUM_STEPS); k.spd = mini ((int) d.spd, NUM_SPEEDS - 1); k.q = d.q ? 1 : 0;
    }
    if (sl.used)
        for (int j = 0; j < sl.nlk; ++j)
        {
            const PatLock& l = sl.lk[j];
            if (l.t < NUM_TRACKS && l.s < NUM_STEPS && l.p < NUM_PARAMS) tr[l.t].lock[l.s][l.p] = l.v;
        }
    curChg = sl.used ? sl.chg : 0;
}

void Engine::patSwitch (int i, double start)
{
    if (i < 0 || i >= NUM_PATS) return;
    patSave (curPat);
    // the notes the old pattern held until their next trig let go (the new one may not have any); v81: a drone carries
    // on into the new pattern when it has one too on the same machine (its settings change under it, as a trig would
    // change it: HALO's next chord, HOWL's strings gliding), else it lets go
    for (int t = 0; t < NUM_TRACKS; ++t) for (int k = 0; k < 2; ++k) if (tr[t].voice[k].sus && ! tr[t].voice[k].drone) letGo (tr[t].voice[k]);
    patLoad (i);
    static float NOLOCK_P[NUM_PARAMS];
    for (int p = 0; p < NUM_PARAMS; ++p) NOLOCK_P[p] = -1.0f;
    for (int t = 0; t < NUM_TRACKS; ++t)
        for (int k = 0; k < 2; ++k)
        {
            Voice& v = tr[t].voice[k];
            if (! v.drone || ! v.active || v.choking) continue;
            if (droneWanted (tr[t]) && v.machine == machineOf (tr[t].p[P_MACH])) droneChange (tr[t], t, v, NOLOCK_P, -1, lock (tr[t], -1, P_VEL), 0.0f);
            else droneEnd (v);
        }
    // the page still edits the pattern it shows (patEdit follows from the page): its edits go to that slot
    curPat = i;
    patStart = start;
    patSerial++;
}

const float* Engine::getPatInfo()
{
    const double C = patChangeLen();
    const double e = loopOn ? loopMapRaw (pos16) + loopOffset() : pos16;
    const int nx = patNext();
    // (the pattern goes round every C 16ths from its start: where it is in this round, what is left of it)
    double k = __builtin_floor ((e - patStart) / C + 1e-9);
    if (k < 0.0) k = 0.0;
    double left = -1.0;
    if (nx >= 0 && running) left = queued >= 0 && qMode != 0 ? 0.0 : patStart + (k + 1.0) * C - e;
    patInfo[0] = (float) curPat; patInfo[1] = (float) nx; patInfo[2] = (float) chainLen; patInfo[3] = (float) chainPos;
    patInfo[4] = (float) patSerial; patInfo[5] = (float) C; patInfo[6] = (float) (e - patStart - k * C); patInfo[7] = (float) left;
    return patInfo;
}

} // namespace sk
