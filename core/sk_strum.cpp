// IMPAKT — STRUM, strummed chords on plucked strings (see sk_strum.h)
#include "sk_engine.h"

namespace sk {

namespace {
const float C3 = 130.8128f;

// atan (|x| <= 1) and atan2, ~1e-5 rad
inline float atanf_ (float x)
{
    const float x2 = x * x;
    return x * (0.99997726f + x2 * (-0.33262347f + x2 * (0.19354346f + x2 * (-0.11643287f + x2 * (0.05265332f + x2 * -0.01172120f)))));
}
inline float atan2f_ (float y, float x)
{
    const float ax = absf (x), ay = absf (y);
    if (ax < 1e-30f && ay < 1e-30f) return 0.0f;
    float a = ax >= ay ? atanf_ (ay / ax) : 1.5707963f - atanf_ (ax / ay);
    if (x < 0.0f) a = PI - a;
    return y < 0.0f ? -a : a;
}
// the phase delay (samples) at f (cycles a sample) of the loop's one-pole low-pass y += (1 - p)(x - y), and of a
// first-order allpass (c + z^-1) / (1 + c z^-1)
inline float lpDelay (float p, float f)
{
    const float w = TWO_PI * f, s = sin2pi (f), c = cos2pi (f);
    return atan2f_ (p * s, 1.0f - p * c) / w;
}
inline float apDelay (float a, float f)
{
    const float w = TWO_PI * f, s = sin2pi (f), c = cos2pi (f);
    return -(atan2f_ (-s, a + c) - atan2f_ (-a * s, 1.0f + a * c)) / w;
}

// MATL: the pluck's noise, the pick's hardness, the loop's damping, how long it rings, where it is plucked (an offset),
// the pluck's overshoot, stiffness, the body (its share, its set), the level; SYNTH plucks a square
struct Mat { float noise, hard, damp, t60, pick, bend, disp, body, out; int bset; int synth; };
const Mat MATS[STRUM_MATS] = {
    { 0.06f, 0.55f, 1.25f, 0.85f,  0.00f, 1.0f, 0.00f, 1.0f, 1.00f,  0, 0 },     // NYLON: round, warm, a deep body
    { 0.10f, 1.00f, 0.60f, 1.15f,  0.00f, 1.0f, 0.06f, 0.9f, 0.90f,  1, 0 },     // STEEL: bright, long, a little stiff
    { 0.03f, 0.85f, 0.50f, 1.60f,  0.04f, 0.6f, 0.03f, 0.0f, 1.00f, -1, 0 },     // ELEC: no body, long, a pickup's top
    { 0.00f, 0.50f, 0.85f, 1.70f, -0.06f, 0.5f, 0.00f, 0.7f, 1.00f,  2, 0 },     // HARP: fingers at the middle, a soundboard
    { 0.12f, 1.20f, 0.70f, 0.90f,  0.10f, 2.4f, 0.04f, 0.8f, 0.90f,  3, 0 },     // KOTO: by the bridge, bent at first, a box
    { 0.00f, 1.00f, 0.90f, 0.70f,  0.00f, 0.3f, 0.00f, 0.0f, 0.80f, -1, 1 } };   // SYNTH: a square plucked, no body
// the bodies (classical guitar, steel-string guitar, harp, koto): four resonances each, Hz, Q, level
const float BODY_F[4][4] = { { 98.0f, 196.0f, 410.0f, 2400.0f }, { 112.0f, 228.0f, 470.0f, 3100.0f },
                             { 160.0f, 340.0f, 920.0f, 2600.0f }, { 190.0f, 400.0f, 1250.0f, 3800.0f } };
const float BODY_Q[4][4] = { { 5.0f, 7.0f, 4.0f, 1.4f }, { 6.0f, 8.0f, 5.0f, 1.2f }, { 3.5f, 4.0f, 3.0f, 1.5f }, { 4.0f, 5.0f, 3.5f, 2.0f } };
const float BODY_G[4][4] = { { 0.9f, 0.7f, 0.45f, 0.25f }, { 0.8f, 0.75f, 0.5f, 0.4f }, { 0.55f, 0.5f, 0.45f, 0.3f }, { 0.7f, 0.6f, 0.45f, 0.35f } };

inline int mod12 (int x) { return ((x % 12) + 12) % 12; }

// the strings of a chord: from its lowest note up (INV turning its lowest notes over first), every note of the chord
// (in any octave), no two strings closer than minI semitones (a note the strings still lack: 3), up to six strings and
// two octaves and a third over the bass; with LEAD, the top string the chord's note nearest the last top (a fifth over
// the bass at least). SPRD over half drops the bass an octave
int voiceStrings (const float* notes, int m, float spr, float inv, float top, float* str)
{
    int nt[HALO_MAXN];
    for (int k = 0; k < m; ++k) nt[k] = (int) floorf_ (notes[k] + 0.5f);
    const int turn = (int) floorf_ (inv * (float) m + 0.5f);
    for (int k = 0; k < turn && k < m - 1; ++k) nt[k] += 12;
    for (int a = 1; a < m; ++a) for (int b = a; b > 0 && nt[b] < nt[b - 1]; --b) { const int t = nt[b]; nt[b] = nt[b - 1]; nt[b - 1] = t; }
    const int bass = nt[0];
    bool inChord[12] = {};
    for (int k = 0; k < m; ++k) inChord[mod12 (nt[k])] = true;
    int lim = bass + 28;
    if (top > -900.0f)
    {
        float bd = 1e9f; int bt = lim;
        for (int k = 0; k < m; ++k)
            for (int o = -1; o <= 3; ++o)
            {
                const int c = nt[k] + 12 * o;
                if (c < bass + 7 || c > bass + 31) continue;
                const float d = absf ((float) c - top);
                if (d < bd) { bd = d; bt = c; }
            }
        lim = bt;
    }
    int cand[64], nc = 0;
    for (int o = 0; o <= 3; ++o)
        for (int k = 0; k < m; ++k)
        {
            const int c = nt[k] + 12 * o;
            if (c <= bass || c > lim || nc >= 64) continue;
            bool dup = false;
            for (int j = 0; j < nc; ++j) if (cand[j] == c) dup = true;
            if (! dup) cand[nc++] = c;
        }
    for (int a = 1; a < nc; ++a) for (int b = a; b > 0 && cand[b] < cand[b - 1]; --b) { const int t = cand[b]; cand[b] = cand[b - 1]; cand[b - 1] = t; }
    const int minI = 2 + (int) floorf_ (clampf (spr, 0.0f, 1.0f) * 5.0f + 0.5f);
    int s[STRUM_MAXS], ns = 0;
    bool have[12] = {};
    s[ns++] = bass; have[mod12 (bass)] = true;
    for (int j = 0; j < nc && ns < STRUM_MAXS; ++j)
    {
        const int c = cand[j], pc = mod12 (c);
        if (c - s[ns - 1] >= (have[pc] ? minI : mini (3, minI))) { s[ns++] = c; have[pc] = true; }
    }
    // a note of the chord still missing takes the place of the doubling nearest it
    for (int pc = 0; pc < 12; ++pc)
    {
        if (! inChord[pc] || have[pc]) continue;
        int bj = -1; float bd = 1e9f; int bc = 0;
        for (int j = 1; j < ns; ++j)
        {
            int count = 0;
            for (int q = 0; q < ns; ++q) if (mod12 (s[q]) == mod12 (s[j])) ++count;
            if (count < 2) continue;
            int c = s[j] - mod12 (s[j] - pc); if (s[j] - c > 6) c += 12;
            const float d = absf ((float) (c - s[j]));
            if (c > bass && d < bd) { bd = d; bj = j; bc = c; }
        }
        if (bj >= 0) { s[bj] = bc; have[pc] = true; }
    }
    for (int a = 1; a < ns; ++a) for (int b = a; b > 0 && s[b] < s[b - 1]; --b) { const int t = s[b]; s[b] = s[b - 1]; s[b - 1] = t; }
    if (spr > 0.55f && ns > 1) s[0] -= 12;
    for (int j = 0; j < ns; ++j) str[j] = (float) s[j];
    return ns;
}

// a string struck: the same note on the voice the string played is struck again (what it still rings keeps on);
// another note takes a free voice, or the quietest one that is not being struck
void pluck (StrumState& ss, int slot, float pitch, float vel, int wait)
{
    const int idx = ss.cur[slot];
    if (idx >= 0 && ss.s[idx].on == 2 && ss.s[idx].pend < 0 && absf (ss.s[idx].pitch - pitch) < 0.05f)
    {
        StrumVoice& sv = ss.s[idx];
        sv.pend = maxi (wait, 0); sv.vel = vel; sv.re = 0.35f; sv.unit = slot;
        return;
    }
    int best = -1;
    for (int k = 0; k < STRUM_VOICES; ++k) if (ss.s[k].on == 0 && ss.s[k].pend < 0) { best = k; break; }
    if (best < 0)
    {
        float bl = 1e9f;
        for (int k = 0; k < STRUM_VOICES; ++k)
        {
            const StrumVoice& sv = ss.s[k];
            if (sv.pend >= 0 || sv.on == 1) continue;
            if (sv.lvl < bl) { bl = sv.lvl; best = k; }
        }
    }
    if (best < 0) return;
    for (int j = 0; j < 2 * STRUM_MAXS; ++j) if (ss.cur[j] == best) ss.cur[j] = -1;
    StrumVoice& sv = ss.s[best];
    sv.on = 1; sv.pend = maxi (wait, 0); sv.pitch = pitch; sv.vel = vel; sv.re = 0.0f; sv.dk = 1.0f; sv.lvl = 0.0f;
    sv.ex = sv.exLen = 0; sv.unit = slot; sv.air = 0.0f;
    ss.cur[slot] = best;
}
}

void Engine::strumTrigger (Voice& v, const float* P)
{
    StrumState& ss = v.ss;
    HaloState& h = v.hs;
    Track& t = tr[v.ti];
    if (ss.fresh)
    {
        for (int k = 0; k < STRUM_VOICES; ++k) { ss.s[k].on = 0; ss.s[k].pend = -1; ss.s[k].lvl = 0.0f; ss.s[k].w = 0; }
        for (int j = 0; j < 2 * STRUM_MAXS; ++j) ss.cur[j] = -1;
        for (int j = 0; j < 8; ++j) ss.body[j] = 0.0f;
        ss.n = 0; ss.dir = -1; ss.top = -999.0f; ss.held = 0;
        h.n = 0; for (int k = 0; k < HALO_KEY; ++k) h.key[k] = -99999;
        ss.aH1 = ss.aH2 = ss.aLp = ss.aL2 = 0.0f; ss.arng.seed (0x5A1Au + 7919u * (u32) v.ti);
        for (int k = 0; k < STRUM_VOICES; ++k) ss.s[k].air = 0.0f;
        ageReset (v);
        ss.fresh = 0;
    }
    const float tuneRaw = (P[P_TUNE] - 0.5f) * 48.0f + v.note;
    const u32 qm = v.quant ? harmMask (v.ti) : 0u;
    const int dirMode = idxOf (P[P_E], STRUM_DIRS);
    // a retrig, a ratchet: the same chord again, the other way
    const bool again = retrigHit && ss.n > 0;
    int dir;
    if (again) dir = -ss.dir;
    else
    {
        haloProg (v, P);
        float notes[HALO_MAXN]; int key[HALO_KEY];
        const int m = haloChord (v, P, tuneRaw, qm, notes, key, nullptr);
        for (int k = 0; k < HALO_KEY; ++k) h.key[k] = key[k];
        h.n = m; for (int k = 0; k < m; ++k) h.chord[k] = notes[k];
        // (its pitch classes, from C3: the quantiser's when it follows this track)
        if (m > 0) { u32 pc = 0; for (int k = 0; k < m; ++k) pc |= 1u << mod12 ((int) floorf_ (notes[k] + 0.5f)); t.chordPc = pc; t.chordRoot = mod12 (h.root); }
        const bool lead = idxOf (P[P_HARM], NUM_HARM) == HARM_LEAD;
        float strs[STRUM_MAXS];
        const int ns = m > 0 ? voiceStrings (notes, m, P[P_C], P[P_HINV], lead && ss.n > 0 ? ss.top : -999.0f, strs) : 0;
        // the hand leaves what it will not strike again: damped after RING (at once what clashes with the new chord)
        const bool pure = idxOf (P[P_HARM], NUM_HARM) >= HARM_PURE;
        const float tauR = 0.03f * powf_ (200.0f, P[P_SRNG]);
        for (int k = 0; k < STRUM_VOICES; ++k)
        {
            StrumVoice& sv = ss.s[k];
            if (sv.on != 2) continue;
            bool again2 = false, clash = false;
            for (int j = 0; j < ns; ++j)
            {
                if (absf (sv.pitch - strs[j]) < 0.05f || absf (sv.pitch - strs[j] - 12.0f) < 0.05f) again2 = true;
                const int d = mod12 ((int) floorf_ (sv.pitch - strs[j] + 0.5f));
                if (d == 1 || d == 11) clash = true;
            }
            if (again2 && ! clash) continue;
            const float tau = pure && clash ? minf (tauR, 0.05f) : tauR;
            const float f = C3 * exp2f_ (sv.pitch * (1.0f / 12.0f));
            sv.dk = minf (sv.dk, expf_ (-1.0f / (tau * f)));
        }
        ss.n = ns; for (int j = 0; j < ns; ++j) ss.str[j] = strs[j];
        if (ns > 0) ss.top = strs[ns - 1];
        if (dirMode == 0) dir = 1;
        else if (dirMode == 1) dir = -1;
        else if (dirMode == 2) dir = t.hProg <= 1 ? 1 : -ss.dir;      // (down and up in turn, retrigs included)
        else dir = v.rng.uni() < 0.5f ? 1 : -1;
    }
    const int ns = ss.n;
    // an upstroke in D/U or of a retrig skips the bass strings
    int lo = 0;
    if (dir < 0 && (again || dirMode == 2) && ns > 3) lo = ns - maxi (3, ns - 2);
    const int cnt = ns - lo;
    const float hum = P[P_SHUM], acc = (P[P_SACC] - 0.5f) * 2.0f, dbl = P[P_SDBL];
    float T = P[P_D] < 0.01f ? 0.0f : 0.003f * powf_ (500.0f, P[P_D]);
    T *= 1.0f + 0.3f * hum * v.rng.bi();
    const float gap = cnt > 1 ? T / (float) (cnt - 1) : 0.0f;
    for (int k = 0; k < cnt; ++k)
    {
        const int j = dir > 0 ? lo + k : ns - 1 - k;              // the string (0: the bass)
        const float at = maxf (0.0f, gap * (float) k + hum * 0.008f * v.rng.bi());
        float vel = v.vel * (1.0f + acc * 0.55f * (cnt > 1 ? 2.0f * (float) k / (float) (cnt - 1) - 1.0f : 0.0f));
        vel *= (dir < 0 ? 0.85f : 1.0f) * (1.0f + 0.25f * hum * v.rng.bi());
        const float cents = hum * 4.0f * v.rng.bi();
        pluck (ss, j, ss.str[j] + cents * 0.01f, clampf (vel, 0.0f, 1.5f), (int) (at * sr));
        // DBL: the course's second string, an octave over the low ones, a unison (a few cents up) over the high ones
        if (dbl > 0.002f)
            pluck (ss, STRUM_MAXS + j, ss.str[j] + (j < ns - 2 ? 12.0f : 0.0f) + 0.07f + cents * 0.01f, clampf (vel * dbl, 0.0f, 1.5f), (int) ((at + 0.0018f) * sr));
    }
    ss.dir = dir;
    ss.seq = v.step >= 0;
    ss.held = 1;
    ss.holdLeft = v.holdSec > 0.0f ? v.holdSec * sr : -1.0f;
}

void Engine::strumRender (Voice& v, const float* P, float tuneRaw, u32 qm, int n, float* out)
{
    (void) tuneRaw; (void) qm;
    StrumState& ss = v.ss;
    const Track& t = tr[v.ti];
    const bool holdOn = P[P_DEC] >= DEC_HOLD || v.drone;          // (v81: a drone holds, whatever DEC)
    // the gate: HOLD sustains while it is open; the end of a trig's GATE damps (a staccato); a stop or a mute damps
    // what the sequencer struck (and what it was about to strike is not)
    auto dampAll = [&] (float tau)
    {
        for (int k = 0; k < STRUM_VOICES; ++k)
        {
            StrumVoice& sv = ss.s[k];
            if (sv.on == 0) continue;
            const float f = C3 * exp2f_ (sv.pitch * (1.0f / 12.0f));
            sv.dk = minf (sv.dk, expf_ (-1.0f / (tau * f)));
        }
    };
    if ((! running || t.mute) && ss.seq)
    {
        ss.seq = 0; ss.held = 0;
        for (int k = 0; k < STRUM_VOICES; ++k) { StrumVoice& sv = ss.s[k]; sv.pend = -1; if (sv.on == 1) sv.on = 0; }
        dampAll (0.08f);
    }
    else if (ss.held && ss.holdLeft >= 0.0f)
    {
        ss.holdLeft -= (float) n;
        if (ss.holdLeft <= 0.0f)
        {
            ss.held = 0;
            const float tau = holdOn ? 0.25f : (v.step >= 0 && ! v.padHeld ? 0.08f : -1.0f);
            if (tau > 0.0f) dampAll (tau);
        }
    }
    const Mat& M = MATS[idxOf (P[P_SMAT], STRUM_MATS)];
    const float mute = P[P_SMUT], pick = P[P_SPCK], stif = P[P_SSTF];
    // DEC: 0.25 .. 25 s (HOLD while held: a minute); MUTE shortens it, the palm darkens the loop
    const float t60 = (holdOn && ss.held) ? 60.0f : 0.25f * exp2f_ (minf (P[P_DEC], DEC_HOLD) * 6.7f);
    const float tM = t60 * M.t60 * (1.0f - 0.9f * mute);
    const float dmp = clampf (P[P_SDMP], 0.0f, 1.0f), pB = clampf ((0.05f + 0.85f * (1.0f - dmp) * (1.0f - dmp)) * M.damp, 0.0f, 0.97f);
    const float pMute = pB + (0.95f - pB) * mute * 0.8f;
    const float cD = -clampf (M.disp + stif * 0.65f, 0.0f, 0.85f);
    const float hard = clampf (pick * M.hard * (1.0f - 0.6f * mute), 0.0f, 1.3f);
    const float bendK = expf_ (-(float) n / (0.045f * sr * (M.bend > 2.0f ? 1.6f : 1.0f)));
    float* lines = v.lbuf;
    for (int i = 0; i < n; ++i) out[i] = 0.0f;
    // AGE: the strings' tuning (cents at the string the furthest out), how unequal they are; AIR: its level, the
    // pick's breath's fall, what of the strings' own vibration shapes the noise
    const float dtC = P[P_GDTN] * P[P_GDTN] * 50.0f, var = P[P_GVAR], sair = P[P_SAIR];
    const bool varOn = var > 0.0005f, airOn = sair > 0.002f;
    float aEnv[CHUNK];
    if (airOn) for (int i = 0; i < n; ++i) aEnv[i] = 0.0f;
    const float airK = expf_ (-1.0f / (0.016f * sr));
    float lvMax = 0.0f;
    const float lvRel = expf_ (-1.0f / (0.05f * sr));
    for (int k = 0; k < STRUM_VOICES; ++k)
    {
        StrumVoice& sv = ss.s[k];
        if (sv.on == 0 && sv.pend < 0) continue;
        float* line = lines + k * STRUM_LINE;
        // this chunk's loop: the pitch (the pluck's overshoot dying away), the low-pass, the stiffness, the tuning
        float f = dtC > 0.0f ? C3 * exp2f_ ((sv.pitch + sv.bend * 0.01f + dtC * cardErr (v.ti, sv.unit) * 0.01f) * (1.0f / 12.0f))
                             : C3 * exp2f_ ((sv.pitch + sv.bend * 0.01f) * (1.0f / 12.0f));
        while (sr / f > (float) (STRUM_LINE - 8)) f *= 2.0f;
        f = minf (f, 0.2f * sr);
        sv.bend *= bendK;
        const float fc = f / sr;
        float p = clampf (pMute * minf (110.0f / f, 2.0f), 0.0f, 0.97f);
        // VARI: this string's brightness, ring and level
        float lvS = 1.0f, t60S = 1.0f;
        if (varOn)
        {
            p = clampf (p * exp2f_ (0.8f * var * cardUnit (v.ti, sv.unit, 1)), 0.0f, 0.97f);
            t60S = exp2f_ (0.7f * var * cardUnit (v.ti, sv.unit, 2));
            lvS = exp2f_ (0.5f * var * cardUnit (v.ti, sv.unit, 3));
        }
        const float tau = lpDelay (p, fc) + 2.0f * apDelay (cD, fc);
        float D = sr / f - tau;
        if (D < 2.5f) D = 2.5f;
        const int Di = (int) floorf_ (D - 0.5f);
        const float fr = D - (float) Di, eta = (1.0f - fr) / (1.0f + fr);
        const float T60f = varOn ? tM * clampf (powf_ (196.0f / f, 0.35f), 0.35f, 1.8f) * t60S : tM * clampf (powf_ (196.0f / f, 0.35f), 0.35f, 1.8f);
        const float g = exp2f_ (-9.9657843f / (f * maxf (T60f, 0.02f))) * sv.dk;
        const int mask = STRUM_LINE - 1;
        float lv = sv.lvl;
        for (int i = 0; i < n; ++i)
        {
            if (sv.pend >= 0)
            {
                if (sv.pend == 0)
                {
                    // the pluck: a fresh string (or what is left of the last one), a period of triangle at the pick's place
                    if (sv.on == 2 && sv.re > 0.0f) { for (int q = 0; q < STRUM_LINE; ++q) line[q] *= sv.re; sv.apX *= sv.re; sv.apY *= sv.re; sv.lp *= sv.re; sv.d1 *= sv.re; sv.d2 *= sv.re; }
                    else { for (int q = 0; q < STRUM_LINE; ++q) line[q] = 0.0f; sv.apX = sv.apY = sv.lp = sv.d1 = sv.d2 = 0.0f; sv.w = 0; }
                    sv.on = 2; sv.dk = 1.0f; sv.re = 0.0f;
                    sv.ex = 0; sv.exLen = maxi (2, (int) (sr / f + 0.5f));
                    sv.exPos = clampf (0.42f - 0.36f * pick + M.pick + 0.02f * v.rng.bi(), 0.04f, 0.48f);
                    sv.exAmp = 0.55f * sv.vel; sv.exNz = M.noise * (0.5f + sv.vel);
                    sv.exHard = 1.0f - expf_ (-TWO_PI * minf (600.0f * exp2f_ (hard * 5.0f) * (0.5f + sv.vel), 0.45f * sr) / sr);
                    sv.exLp = 0.0f;
                    sv.bend = M.bend * (6.0f + 18.0f * sv.vel);
                    sv.air = airOn ? 0.5f * sv.vel * (0.5f + 0.5f * hard) : 0.0f;
                    lv = maxf (lv, 0.05f);
                }
                --sv.pend;
            }
            if (sv.on != 2) continue;
            float e = 0.0f;
            if (sv.ex < sv.exLen)
            {
                const float u = (float) sv.ex / (float) sv.exLen, b = sv.exPos;
                float sh = M.synth ? (u < 0.5f ? 0.5f : -0.5f) : (u < b ? u / b : (1.0f - u) / (1.0f - b)) - 0.5f;
                sh += sv.exNz * v.rng.bi() * (1.0f - u);
                sv.exLp += sv.exHard * (sh - sv.exLp);
                e = sv.exLp * sv.exAmp;
                ++sv.ex;
            }
            const float x = line[(sv.w - Di) & mask];
            const float ya = eta * x + sv.apX - eta * sv.apY;
            sv.apX = x; sv.apY = ya;
            sv.lp += (1.0f - p) * (ya - sv.lp);
            float y = sv.lp;
            float o = cD * y + sv.d1; sv.d1 = y - cD * o; y = o;
            o = cD * y + sv.d2; sv.d2 = y - cD * o; y = o;
            y *= g;
            const float w = y + e;
            line[sv.w] = w;
            sv.w = (sv.w + 1) & mask;
            out[i] += w * lvS;
            if (airOn) { aEnv[i] += (sv.air + 0.25f * absf (w)) * lvS; sv.air *= airK; }
            const float aw = absf (w);
            lv = aw > lv ? aw : lv * lvRel;
        }
        sv.lvl = lv;
        if (isBad (sv.lp) || isBad (sv.apY) || isBad (sv.d1) || isBad (sv.d2))
        { sv.on = 0; sv.pend = -1; sv.lvl = 0.0f; sv.lp = sv.apX = sv.apY = sv.d1 = sv.d2 = 0.0f; for (int q = 0; q < STRUM_LINE; ++q) line[q] = 0.0f; }
        if (sv.on == 2 && sv.pend < 0 && sv.ex >= sv.exLen && lv < 2e-5f) { sv.on = 0; sv.lvl = 0.0f; }
        lvMax = maxf (lvMax, sv.lvl);
    }
    // the body (after the strings, all of them): four resonances added; ELEC: a pickup's resonant top
    const float bodyAmt = P[P_SBDY] * 1.2f * M.body;
    if (M.bset >= 0 && bodyAmt > 0.002f)
    {
        float a1[4], a2[4], a3[4], kk[4];
        for (int r = 0; r < 4; ++r)
        {
            const float gg = tanpi (minf (BODY_F[M.bset][r], 0.45f * sr) / sr);
            kk[r] = 1.0f / BODY_Q[M.bset][r];
            a1[r] = 1.0f / (1.0f + gg * (gg + kk[r])); a2[r] = gg * a1[r]; a3[r] = gg * a2[r];
        }
        for (int i = 0; i < n; ++i)
        {
            const float x = out[i];
            float b = 0.0f;
            for (int r = 0; r < 4; ++r)
            {
                float& z1 = ss.body[2 * r]; float& z2 = ss.body[2 * r + 1];
                const float v3 = x - z2, v1 = a1[r] * z1 + a2[r] * v3, v2 = z2 + a2[r] * z1 + a3[r] * v3;
                z1 = 2.0f * v1 - z1; z2 = 2.0f * v2 - z2;
                b += v1 * kk[r] * BODY_G[M.bset][r];
            }
            out[i] = x + bodyAmt * b;
        }
        for (int j = 0; j < 8; ++j) if (isBad (ss.body[j])) { for (int q = 0; q < 8; ++q) ss.body[q] = 0.0f; break; }
    }
    else if (M.bset < 0 && ! M.synth)
    {
        const float gg = tanpi (minf (4200.0f, 0.45f * sr) / sr), kq = 1.0f / 1.4f;
        const float b1 = 1.0f / (1.0f + gg * (gg + kq)), b2 = gg * b1, b3 = gg * b2;
        for (int i = 0; i < n; ++i)
        {
            float& z1 = ss.body[0]; float& z2 = ss.body[1];
            const float v3 = out[i] - z2, v1 = b1 * z1 + b2 * v3, v2 = z2 + b2 * z1 + b3 * v3;
            z1 = 2.0f * v1 - z1; z2 = 2.0f * v2 - z2;
            out[i] = v2;
        }
        if (isBad (ss.body[0]) || isBad (ss.body[1])) ss.body[0] = ss.body[1] = 0.0f;
    }
    // AIR: one noise for all the strings, shaped by their breaths and vibrations; over 2.5 kHz, softened over 12
    if (airOn)
    {
        const float hg = tanpi (minf (2500.0f, 0.42f * sr) / sr), hk = 1.3f, h1 = 1.0f / (1.0f + hg * (hg + hk)), h2 = hg * h1, h3 = hg * h2;
        const float lg = tanpi (minf (9500.0f, 0.42f * sr) / sr), lk = 1.3f, l1 = 1.0f / (1.0f + lg * (lg + lk)), l2 = lg * l1, l3 = lg * l2;
        const float aG = sair * (0.45f + 0.55f * sair) * 0.75f;
        for (int i = 0; i < n; ++i)
        {
            float x = ss.arng.bi() * aEnv[i];
            float v3 = x - ss.aH2, v1 = h1 * ss.aH1 + h2 * v3, v2 = ss.aH2 + h2 * ss.aH1 + h3 * v3;
            ss.aH1 = 2.0f * v1 - ss.aH1; ss.aH2 = 2.0f * v2 - ss.aH2;
            x -= hk * v1 + v2;
            v3 = x - ss.aL2; v1 = l1 * ss.aLp + l2 * v3; v2 = ss.aL2 + l2 * ss.aLp + l3 * v3;
            ss.aLp = 2.0f * v1 - ss.aLp; ss.aL2 = 2.0f * v2 - ss.aL2;
            out[i] += v2 * aG;
        }
        if (isBad (ss.aH1) || isBad (ss.aH2) || isBad (ss.aLp) || isBad (ss.aL2)) ss.aH1 = ss.aH2 = ss.aLp = ss.aL2 = 0.0f;
    }
    const float gn = 0.42f * M.out;
    for (int i = 0; i < n; ++i) out[i] *= gn;
    ageFx (v, P, n, out);
    bool pending = false;
    for (int k = 0; k < STRUM_VOICES; ++k) if (ss.s[k].pend >= 0) pending = true;
    v.level = pending ? maxf (lvMax, 0.01f) : lvMax;
    v.ns += (u32) n;
}

// for the screen (after haloInfo's chord): the strings' count, then each voice: its pitch (-999: silent), its level, the
// string it plays (-1: none)
void Engine::strumInfo (int ti, float* out) const
{
    const Track& t = tr[ti];
    const Voice& v = t.voice[t.cur];
    out[0] = 2.0f;
    for (int s = 0; s < HALO_SLOTS; ++s) { out[12 + 3 * s] = -999.0f; out[13 + 3 * s] = 0.0f; out[14 + 3 * s] = -1.0f; }
    if (! v.active || v.machine != M_STRUM) return;
    const StrumState& ss = v.ss;
    for (int s = 0; s < STRUM_VOICES && s < HALO_SLOTS; ++s)
    {
        const StrumVoice& sv = ss.s[s];
        int str = -1;
        for (int j = 0; j < 2 * STRUM_MAXS; ++j) if (ss.cur[j] == s) str = j % STRUM_MAXS;
        out[12 + 3 * s] = sv.on == 2 ? sv.pitch : -999.0f;
        out[13 + 3 * s] = sv.on == 2 ? sv.lvl : 0.0f;
        out[14 + 3 * s] = (float) str;
    }
}

} // namespace sk
