// IMPAKT — HOWL, a guitar held up to a screaming amplifier, with its own chords (see sk_howl.h)
#include "sk_engine.h"

namespace sk {

float howlFbkS = 3.5f, howlFbkD = 1.0f;   // (calibration)
namespace {
constexpr float E2 = 40.0f;            // TUNE at 12 o'clock: the root on E2 (MIDI)
inline float midiHz (float m) { return 440.0f * exp2f_ ((m - 69.0f) * (1.0f / 12.0f)); }
inline int mod12 (int x) { return ((x % 12) + 12) % 12; }
inline float smooth01 (float x) { x = clampf (x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }

// atan (|x| <= 1) and atan2, ~1e-5 rad; the phase delay (samples) of the loop's one-pole low-pass y += (1 - p)(x - y)
// at f (cycles a sample): taken out of the line's length, the string plays in tune whatever its damping
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
inline float lpDelay (float p, float f)
{
    const float w = TWO_PI * f, s = sin2pi (f), c = cos2pi (f);
    return atan2f_ (p * s, 1.0f - p * c) / w;
}
// ln cosh, stable (the antiderivative of tanh: the preamp's first stage without aliasing, first order)
inline float lnCosh (float u) { const float a = absf (u); return a + log2f_ (1.0f + exp2f_ (-2.8853901f * a)) * LN2 - LN2; }

// v83, CAB: the biquads (after Robert Bristow-Johnson's Audio EQ Cookbook), their coefficients made once a knob moves;
// their magnitude where the loop sings (FDBK's threshold is made for it)
void bqSet (HowlBq& q, float b0, float b1, float b2, float a0, float a1, float a2)
{
    const float ia = 1.0f / a0;
    q.b0 = b0 * ia; q.b1 = b1 * ia; q.b2 = b2 * ia; q.a1 = a1 * ia; q.a2 = a2 * ia;
}
void bqPeak (HowlBq& q, float f, float Q, float dB, float sr)
{
    const float w = minf (f, 0.45f * sr) / sr, A = exp2f_ (dB * (0.1660964f * 0.5f)), cw = cos2pi (w), al = sin2pi (w) / (2.0f * Q);
    bqSet (q, 1.0f + al * A, -2.0f * cw, 1.0f - al * A, 1.0f + al / A, -2.0f * cw, 1.0f - al / A);
}
void bqShelf (HowlBq& q, float f, float dB, bool high, float sr)
{
    const float w = minf (f, 0.45f * sr) / sr, A = exp2f_ (dB * (0.1660964f * 0.5f)), cw = cos2pi (w);
    const float al = sin2pi (w) * 0.5f * sqrtf_ ((A + 1.0f / A) * (1.0f / 0.8f - 1.0f) + 2.0f), sa = 2.0f * sqrtf_ (A) * al;
    if (high) bqSet (q, A * ((A + 1.0f) + (A - 1.0f) * cw + sa), -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw), A * ((A + 1.0f) + (A - 1.0f) * cw - sa),
                     (A + 1.0f) - (A - 1.0f) * cw + sa, 2.0f * ((A - 1.0f) - (A + 1.0f) * cw), (A + 1.0f) - (A - 1.0f) * cw - sa);
    else bqSet (q, A * ((A + 1.0f) - (A - 1.0f) * cw + sa), 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw), A * ((A + 1.0f) - (A - 1.0f) * cw - sa),
                (A + 1.0f) + (A - 1.0f) * cw + sa, -2.0f * ((A - 1.0f) + (A + 1.0f) * cw), (A + 1.0f) + (A - 1.0f) * cw - sa);
}
void bqHp (HowlBq& q, float f, float Q, float sr)
{
    const float w = minf (f, 0.45f * sr) / sr, cw = cos2pi (w), al = sin2pi (w) / (2.0f * Q);
    bqSet (q, 0.5f * (1.0f + cw), -(1.0f + cw), 0.5f * (1.0f + cw), 1.0f + al, -2.0f * cw, 1.0f - al);
}
float bqMag (const HowlBq& q, float f, float sr)
{
    const float w = minf (f, 0.45f * sr) / sr, c1 = cos2pi (w), s1 = sin2pi (w), c2 = cos2pi (2.0f * w), s2 = sin2pi (2.0f * w);
    const float nr = q.b0 + q.b1 * c1 + q.b2 * c2, ni = -(q.b1 * s1 + q.b2 * s2);
    const float dr = 1.0f + q.a1 * c1 + q.a2 * c2, di = -(q.a1 * s1 + q.a2 * s2);
    return sqrtf_ ((nr * nr + ni * ni) / maxf (dr * dr + di * di, 1e-12f));
}

// the chords: the interval over the root of what plays the third, the fifth, the seventh and the colour (-1: none)
struct HType { signed char t, f, s, c; };
const HType TYPES[HOWL_KEY_T] = {
    { -1,  7, -1, -1 },     // 5TH: the power chord
    { -1, -1, -1, -1 },     // OCT: the root alone, in octaves
    {  2,  7, -1, -1 },     // SUS2
    {  5,  7, -1, -1 },     // SUS4
    {  4,  7, -1, -1 },     // MAJ
    {  3,  7, -1, -1 },     // MIN
    {  4,  7, -1, 14 },     // ADD9
    {  3,  7, -1, 14 },     // MADD9 (m add9)
    {  4,  7, 11, -1 },     // MAJ7
    {  3,  7, 10, -1 },     // MIN7
    {  4,  7, 10, -1 },     // DOM7
    {  3,  7, 10, 14 },     // MIN9
    { -1,  7,  8, -1 } };   // 5+♭6: the fifth and the minor sixth over it, the doom one
// the voicings: six strings, low to high, each a role (the root, fifth, third, seventh or colour) and an octave over
// the root
enum { RR, RF, RT, RS, RC };
struct HVoic { unsigned char r[HOWL_STR]; signed char o[HOWL_STR]; };
const HVoic VOICS[HOWL_VOICS] = {
    { { RR, RF, RR, RF, RR, RC }, { 0, 0, 1, 1, 2, 2 } },    // DRONE: the root and the fifth in octaves, the colour on top
    { { RR, RF, RR, RT, RF, RR }, { 0, 0, 1, 1, 1, 2 } },    // OPEN: the first position's shape
    { { RR, RF, RR, RT, RS, RF }, { 0, 0, 1, 1, 1, 2 } },    // BARRE: the shape a barre makes, its seventh in it
    { { RR, RF, RT, RS, RC, RF }, { 0, 0, 1, 1, 1, 2 } },    // SPREAD: every note once, wide apart
    { { RR, RR, RC, RT, RF, RS }, { 0, 1, 1, 1, 1, 1 } },    // CLUSTER: the chord tight in the octave over the bass
    { { RT, RF, RR, RT, RF, RR }, { 0, 0, 1, 1, 1, 2 } },    // 1ST: the third in the bass
    { { RF, RR, RT, RF, RR, RT }, { 0, 1, 1, 1, 2, 2 } },    // 2ND: the fifth in the bass
    { { RR, RF, RR, RT, RF, RC }, { 1, 1, 2, 2, 2, 2 } } };  // HIGH: an octave up, bright
// the low limits: the lowest a note may sit at that interval over the root (MIDI) before it turns to mud (a second, a
// seventh, a sixth under D3, a third under A2, a fourth under E2, a fifth under B0)
inline float lowLimit (int iv)
{
    switch (mod12 (iv))
    {
        case 0:  return 0.0f;
        case 7:  return 23.0f;
        case 5:  return 40.0f;
        case 3: case 4: case 9: return 45.0f;
        default: return 50.0f;
    }
}
// the chord's strings (MIDI, low to high): its roles in the voicing, each under its low limit an octave higher; DROP's
// bass string an octave down (SUB); none under E0 or over E6
int buildChord (HType ty, int voic, float root, bool sub, float* out)
{
    auto roleIv = [&] (int role) -> int {
        switch (role)
        {
            case RR: return 0;
            case RF: return ty.f >= 0 ? ty.f : 0;
            case RT: return ty.t >= 0 ? ty.t : (ty.c >= 0 ? ty.c : (ty.f >= 0 ? ty.f : 0));
            case RS: return ty.s >= 0 ? ty.s : (ty.c >= 0 ? ty.c : 0);
            default: return ty.c >= 0 ? ty.c : (ty.t >= 0 ? ty.t : (ty.f >= 0 ? ty.f : 0));
        }
    };
    const HVoic& V = VOICS[voic];
    for (int k = 0; k < HOWL_STR; ++k)
    {
        const int iv = roleIv (V.r[k]);
        float m = root + (float) (mod12 (iv) + 12 * V.o[k]);
        for (int up = 0; up < 6 && m < lowLimit (iv); ++up) m += 12.0f;
        out[k] = m;
    }
    for (int a = 1; a < HOWL_STR; ++a) for (int b = a; b > 0 && out[b] < out[b - 1]; --b) { const float x = out[b]; out[b] = out[b - 1]; out[b - 1] = x; }
    if (sub) out[0] -= 12.0f;
    for (int k = 0; k < HOWL_STR; ++k) { while (out[k] < 16.0f) out[k] += 12.0f; while (out[k] > 88.0f) out[k] -= 12.0f; }
    for (int a = 1; a < HOWL_STR; ++a) for (int b = a; b > 0 && out[b] < out[b - 1]; --b) { const float x = out[b]; out[b] = out[b - 1]; out[b - 1] = x; }
    return HOWL_STR;
}
// the chord's pitch classes (all of them, whatever the voicing plays: the quantiser may follow it)
u32 chordPcs (HType ty, float root)
{
    const int r = mod12 ((int) floorf_ (root + 0.5f));
    u32 pc = 1u << r;
    const int ivs[4] = { ty.t, ty.f, ty.s, ty.c };
    for (int k = 0; k < 4; ++k) if (ivs[k] >= 0) pc |= 1u << mod12 (r + ivs[k]);
    return pc;
}
// KEY: the scale's own chord on the root (its third, fifth, seventh and second, as the scale has them; a sus where it
// has no third); without a scale, the natural minor's (a minor ninth)
HType keyType (u32 mask, int rp)
{
    if (! (mask & 0xFFFu)) mask = 0;
    auto in = [&] (int s) { return mask ? ((mask >> mod12 (rp + s)) & 1u) != 0 : (s == 0 || s == 2 || s == 3 || s == 5 || s == 7 || s == 8 || s == 10); };
    HType t;
    t.t = (signed char) (in (3) && ! in (4) ? 3 : in (4) ? 4 : in (2) ? 2 : in (5) ? 5 : -1);
    t.f = (signed char) (in (7) ? 7 : in (6) ? 6 : in (8) ? 8 : -1);
    t.s = (signed char) (in (10) ? 10 : in (11) ? 11 : -1);
    t.c = (signed char) (t.t != 2 && in (2) ? 14 : -1);
    return t;
}
// the pick: a fresh period of triangle at the pick's place, a little noise; what the string rang is half kept
void pickString (HowlString& s, float vel, int wait)
{
    s.pend = maxi (wait, 0); s.pv = vel;
}
} // namespace

// the strings' settings for this machine as a fresh track has them (STRING, AMP: core machineSideDefaults)
void Engine::howlTrigger (Voice& v, const float* P)
{
    HowlState& h = v.hw;
    const float* S = P + P_S1;
    Track& t = tr[v.ti];
    if (h.fresh)
    {
        for (int i = 0; i < HOWL_STR * HOWL_LINE; ++i) v.lbuf[i] = 0.0f;
        for (int i = 0; i < COMB_SIZE; ++i) v.mcomb[i] = 0.0f;
        h.nr.seed (0x4A0Lu + 7919u * (u32) v.ti);
        for (int k = 0; k < HOWL_STR; ++k)
        {
            HowlString& s = h.s[k];
            s.on = 0; s.pitch = s.from = s.to = E2; s.gl = 1.0f; s.w = 0; s.lp = s.yz = 0.0f; s.dk = 1.0f; s.lvl = 0.0f;
            s.pend = -1; s.pv = 0.0f; s.ex = s.exLen = 0; s.exLp = 0.0f;
            s.off = h.nr.bi(); s.drN = s.drFrom = 0.0f; s.drTo = h.nr.bi(); s.drPh = h.nr.uni(); s.drRate = 0.07f + 0.28f * h.nr.uni();
            s.key = -999.0f; s.age = 0;
            s.bowE = s.bowLp = s.bh1 = s.bh2 = s.bf1 = s.bf2 = 0.0f;
        }
        { HowlBq* bq[7] = { &h.tg, &h.bt, &h.pr, &h.dp, &h.ck, &h.mc, &h.mh }; for (HowlBq* q : bq) { q->flat(); q->z1 = q->z2 = 0.0f; } }
        for (int k = 0; k < 7; ++k) h.cabK[k] = -1.0f;
        h.mrl = 0.0f; h.wbPh = 0.0f; h.wbN = 0.0f; h.wbTo = h.nr.bi();
        h.held = 0; h.holdLeft = -1.0f; h.seq = 0; h.gate = 0.0f; h.gateT = 0.0f; h.bloom = 0.0f; h.aw = 0; h.fbS = 0.0f;
        h.z1 = h.f1 = h.z2 = h.f2 = 0.0f; h.dcX = h.dcY = h.dc2X = h.dc2Y = 0.0f; h.env = 0.0f;
        for (int k = 0; k < 3; ++k) { h.ts[k] = 0.0f; h.ta[k] = 0.0f; h.tsK[k] = -1.0f; }
        for (int k = 0; k < 4; ++k) h.tb[k] = 0.0f;
        h.pwEnv = 0.0f; h.sag = 1.0f; h.cb1 = h.cb2 = h.cb3 = h.cb4 = h.hpX = h.hpY = 0.0f; h.ampOut = 0.0f;
        h.wmN = h.wmFrom = 0.0f; h.wmTo = h.nr.bi(); h.wmPh = 0.0f; h.outLv = h.loopLv = 0.0f;
        h.root = E2; h.type = 0; h.voic = 0; h.n = 0; h.ageN = 0;
        h.mixK = -1.0f;
        h.tmS = -1.0f; h.bp1 = h.bp2 = h.bp3 = h.bp4 = h.mb1 = h.mb2 = 0.0f; h.lim = 1.0f; h.limEnv = h.airEnv = 0.0f; h.gSl = 1.0f;
        for (int q = 0; q < 2; ++q) { h.mvN[q] = h.mvFrom[q] = 0.0f; h.mvTo[q] = h.nr.bi(); }
        h.mvPh = 0.0f; h.humPh = 0.0f; h.crk = (int) (sr * 0.5f);
        for (int k = 0; k < HOWL_STR; ++k) h.notes[k] = E2;
        h.fresh = 0;
    }
    const float tuneRaw = (P[P_TUNE] - 0.5f) * 48.0f + v.note;
    const u32 qm = v.quant ? harmMask (v.ti) : 0u;     // (the scale when the quantiser follows this track)
    const float rootSt = qm ? qPitch (tuneRaw, 82.406889f, qm) : tuneRaw;
    const int type = idxOf (P[P_A], HOWL_TYPES), voic = idxOf (P[P_B], HOWL_VOICS), drop = idxOf (P[P_C], HOWL_DROPS);
    const float oct = drop == 2 || drop == 3 ? -12.0f : drop == 4 ? -24.0f : 0.0f;
    const float root = E2 + rootSt + oct;
    // MRPH: the glide to a new chord (at once under 1 %)
    const float mrph = P[P_E] < 0.01f ? 0.0f : 0.02f * powf_ (1500.0f, P[P_E]);
    // EXCT: the pick (none at its left: the feedback alone wakes the strings)
    const float exct = S[HW_EXCT], pv = exct < 0.005f ? 0.0f : clampf (v.vel, 0.0f, 1.5f) * (0.25f + 0.75f * exct);
    auto retune = [&] (HowlString& s, float note, bool glide)
    {
        if (! glide || ! s.on || mrph <= 0.0f)
        {
            // at once: the string's ringing carried over (a part of it: the hand moved)
            if (s.on && absf (s.pitch - note) > 0.3f) { float* line = v.lbuf + (int) (&s - h.s) * HOWL_LINE; for (int q = 0; q < HOWL_LINE; ++q) line[q] *= 0.7f; s.lp *= 0.7f; }
            s.pitch = s.from = s.to = note; s.gl = 1.0f;
        }
        else { s.from = s.pitch; s.to = note; s.gl = 0.0f; }
        s.on = 1; s.dk = 1.0f;
    };
    const float strum = 0.004f + 0.03f * (1.0f - clampf (v.vel, 0.0f, 1.0f)) * (0.3f + 0.7f * exct);   // (a strum's roll)
    if (type == HOWL_POLY_T)
    {
        // POLY: the note takes a string (the one already on it; else a silent one; else the one that took its note
        // first), gliding there in MRPH
        const float note = root;
        int k = -1;
        for (int j = 0; j < HOWL_STR; ++j) if (h.s[j].on && absf (h.s[j].to - note) < 0.3f) { k = j; break; }
        if (k < 0) { unsigned best = 0xFFFFFFFFu; for (int j = 0; j < HOWL_STR; ++j) if (! h.s[j].on && h.s[j].age < best) { best = h.s[j].age; k = j; } }
        if (k < 0) { unsigned best = 0xFFFFFFFFu; for (int j = 0; j < HOWL_STR; ++j) if (h.s[j].age < best) { best = h.s[j].age; k = j; } }
        HowlString& s = h.s[k];
        retune (s, note, true);
        s.key = v.padHeld ? v.note : -999.0f;              // (the key's note, as release () names it)
        s.age = ++h.ageN;
        if (pv > 0.0f) pickString (s, pv, 0);
        h.n = 0; for (int j = 0; j < HOWL_STR; ++j) if (h.s[j].on) h.notes[h.n++] = h.s[j].to;
        // (the chord's pitch classes: the notes the strings hold)
        u32 pc = 0; for (int j = 0; j < h.n; ++j) pc |= 1u << mod12 ((int) floorf_ (h.notes[j] + 0.5f));
        t.chordPc = pc; t.chordRoot = mod12 ((int) floorf_ (note + 0.5f));
    }
    else
    {
        const HType ty = type == HOWL_KEY_T ? keyType (harmMask (v.ti), mod12 ((int) floorf_ (root + 0.5f))) : TYPES[type];
        float nt[HOWL_STR];
        buildChord (ty, voic, root, drop == 1 || drop == 3, nt);
        // the strings take the new notes in order, the lowest the lowest: each glides the least it can
        int ord[HOWL_STR]; for (int k = 0; k < HOWL_STR; ++k) ord[k] = k;
        for (int a = 1; a < HOWL_STR; ++a) for (int b = a; b > 0 && h.s[ord[b]].pitch < h.s[ord[b - 1]].pitch; --b) { const int x = ord[b]; ord[b] = ord[b - 1]; ord[b - 1] = x; }
        for (int k = 0; k < HOWL_STR; ++k)
        {
            HowlString& s = h.s[ord[k]];
            retune (s, nt[k], true);
            s.key = -999.0f; s.age = ++h.ageN;
            // a down-strum (an upstroke on a retrig, the top strings first)
            const int at = retrigHit ? HOWL_STR - 1 - k : k;
            if (pv > 0.0f) pickString (s, pv * (1.0f - 0.04f * (float) k), (int) (strum * (float) at / (float) (HOWL_STR - 1) * sr));
            h.notes[k] = nt[k];
        }
        h.n = HOWL_STR;
        t.chordPc = chordPcs (ty, root); t.chordRoot = mod12 ((int) floorf_ (root + 0.5f));
    }
    h.root = root; h.type = type; h.voic = voic;
    // a new chord: the feedback blooms on it again (from where it was, a quarter of it at most; from nothing at first)
    h.bloom = minf (h.bloom, 0.25f);
    h.held = 1;
    h.holdLeft = v.holdSec > 0.0f ? v.holdSec * sr : -1.0f;
    h.seq = v.step >= 0;
    h.gateT = 1.0f;
}

// a key let go (the voice POLY): the string it held is damped; the last one closes the gate
void Engine::howlRelease (Voice& v, float note)
{
    HowlState& h = v.hw;
    bool any = false;
    for (int k = 0; k < HOWL_STR; ++k)
    {
        HowlString& s = h.s[k];
        if (s.key > -900.0f && absf (s.key - note) < 0.5f)
        {
            s.key = -999.0f;
            const float f = midiHz (s.pitch);
            s.dk = minf (s.dk, expf_ (-1.0f / (0.3f * f)));
        }
        if (s.key > -900.0f) any = true;
    }
    if (! any && h.held) h.holdLeft = 0.5f;
}

// a stop, a mute, a drone ended: the hand on the strings, the guitar turned away from the amp
void Engine::howlLetGo (Voice& v, float tau)
{
    HowlState& h = v.hw;
    h.held = 0; h.seq = 0; h.gateT = 0.0f;
    for (int k = 0; k < HOWL_STR; ++k)
    {
        HowlString& s = h.s[k];
        s.pend = -1;
        if (s.on && tau > 0.0f) s.dk = minf (s.dk, expf_ (-1.0f / (tau * midiHz (s.pitch))));
    }
}

void Engine::howlRender (Voice& v, const float* P, float tuneSt, int n, float* out)
{
    (void) tuneSt;
    HowlState& h = v.hw;
    const Track& t = tr[v.ti];
    const float* S = P + P_S1;
    const float fn = (float) n;
    const bool holdOn = P[P_DEC] >= DEC_HOLD || v.drone;          // (a drone holds, whatever DEC)
    // the gate: a stop or a mute ends what the sequencer struck; the end of a GATE turns the guitar away from the amp
    // (∞: the hand on the strings too)
    if ((! running || t.mute) && h.seq && h.held) howlLetGo (v, 0.12f);
    else if (h.held && h.holdLeft >= 0.0f)
    {
        h.holdLeft -= fn;
        if (h.holdLeft <= 0.0f)
        {
            h.held = 0; h.gateT = 0.0f;
            if (holdOn) for (int k = 0; k < HOWL_STR; ++k) if (h.s[k].on) h.s[k].dk = minf (h.s[k].dk, expf_ (-1.0f / (0.4f * midiHz (h.s[k].pitch))));
        }
    }
    // a knob, a lock or an LFO that changes the chord re-voices it while it is held: the strings glide to it (MRPH)
    if (h.held && h.type != HOWL_POLY_T)
    {
        const float tuneRaw = (P[P_TUNE] - 0.5f) * 48.0f + v.note;
        const u32 qm = v.quant ? harmMask (v.ti) : 0u;     // (the scale when the quantiser follows this track)
        const float rootSt = qm ? qPitch (tuneRaw, 82.406889f, qm) : tuneRaw;
        const int type = idxOf (P[P_A], HOWL_TYPES), voic = idxOf (P[P_B], HOWL_VOICS), drop = idxOf (P[P_C], HOWL_DROPS);
        const float root = E2 + rootSt + (drop == 2 || drop == 3 ? -12.0f : drop == 4 ? -24.0f : 0.0f);
        if (type != HOWL_POLY_T && (absf (root - h.root) > 0.01f || type != h.type || voic != h.voic))
        {
            const HType ty = type == HOWL_KEY_T ? keyType (harmMask (v.ti), mod12 ((int) floorf_ (root + 0.5f))) : TYPES[type];
            float nt[HOWL_STR];
            buildChord (ty, voic, root, drop == 1 || drop == 3, nt);
            int ord[HOWL_STR]; for (int k = 0; k < HOWL_STR; ++k) ord[k] = k;
            for (int a = 1; a < HOWL_STR; ++a) for (int b = a; b > 0 && h.s[ord[b]].to < h.s[ord[b - 1]].to; --b) { const int x = ord[b]; ord[b] = ord[b - 1]; ord[b - 1] = x; }
            const bool slow = P[P_E] >= 0.01f;
            for (int k = 0; k < HOWL_STR; ++k)
            {
                HowlString& s = h.s[ord[k]];
                if (absf (s.to - nt[k]) < 0.001f) continue;
                if (slow && s.on) { s.from = s.pitch; s.to = nt[k]; s.gl = 0.0f; } else { s.pitch = s.from = s.to = nt[k]; s.gl = 1.0f; }
                h.notes[k] = nt[k];
            }
            h.root = root; h.type = type; h.voic = voic;
            Track& tw = tr[v.ti];
            tw.chordPc = chordPcs (ty, root); tw.chordRoot = mod12 ((int) floorf_ (root + 0.5f));
        }
    }
    // (the guitar brought before the amp in 150 ms, turned away in 40)
    h.gate += (h.gateT - h.gate) * (1.0f - expf_ (-fn / ((h.gateT > h.gate ? 0.15f : 0.04f) * sr)));
    // BLOM: the feedback grows on a new chord (0.05 .. 20 s; at once at its left)
    {
        const float bl = S[HW_BLOM];
        if (bl < 0.005f) h.bloom = 1.0f;
        else h.bloom += (1.0f - h.bloom) * (1.0f - expf_ (-fn / (0.05f * powf_ (400.0f, bl) * sr * 0.35f)));
    }
    // this chunk's strings: their glides (MRPH), the drift and the arm, their lines' lengths, damping, decay
    const float mrph = P[P_E] < 0.01f ? 0.0f : 0.02f * powf_ (1500.0f, P[P_E]);
    const float glStep = mrph > 0.0f ? fn / (mrph * sr) : 1.0f;
    const float drf = S[HW_DRFT], drC = drf * drf * 22.0f, offC = drf * 9.0f;
    const float wam = S[HW_WAMY], wamRate = 0.35f + 5.6f * wam * wam, wamC = 45.0f * wam;
    h.wmPh += fn * wamRate / sr;
    if (h.wmPh >= 1.0f) { h.wmPh -= floorf_ (h.wmPh); h.wmFrom = h.wmTo; h.wmTo = h.nr.bi(); }
    h.wmN = h.wmFrom + (h.wmTo - h.wmFrom) * smooth01 (h.wmPh);
    const float dmp = S[HW_DAMP], pB = clampf (0.04f + 0.82f * (1.0f - dmp) * (1.0f - dmp), 0.0f, 0.95f);
    // (∞, a drone: the strings hold what they ring, ten minutes to die away: the note stays under the howl)
    const float t60 = (holdOn && h.held) ? 600.0f : 0.4f * exp2f_ (minf (P[P_DEC], DEC_HOLD) * 6.3f);
    float Dl[HOWL_STR], pl[HOWL_STR], gl[HOWL_STR], fq[HOWL_STR];
    // (COLR: where the loop sings, 70 Hz .. 6 kHz; what FDBK's threshold is made for)
    const float bpF = minf (70.0f * powf_ (85.714f, S[HW_COLR]), 0.42f * sr), wF = minf (bpF, 0.2f * sr) / sr, cwF = cos2pi (wF);
    int   Di[HOWL_STR];
    float lMin = 1.0f; int nOn = 0;
    // BOW (v83): the magnetic bow over STRG's string (1 the highest; ALL: one over each) while the gate is open, its
    // swell (SWEL); what it senses there, how hard it drives it back: the string's own loss a pass made up where it is
    // bowed, a little more while it is under the level BOW asks for, less over it (it holds there whatever DEC; with
    // no feedback at all); WOBL the hand wavering (its distance, its place); HAND the hand muting the strings it is not on
    const float bowAmt = S[HW_BOW], wob = S[HW_WOBL], hand = S[HW_HAND];
    const int strg = idxOf (S[HW_STRG], HOWL_STRGS), bmod = idxOf (S[HW_BMOD], HOWL_BMODS);
    const bool bowOn = bowAmt > 0.002f && h.held;
    const float swel = 0.02f * powf_ (300.0f, S[HW_SWEL]);
    const float bowUp = 1.0f - expf_ (-fn / (0.25f * swel * sr)), bowDn = 1.0f - expf_ (-fn / (0.08f * sr));
    h.wbPh += fn * (2.5f + 3.0f * wob) / sr;
    if (h.wbPh >= 1.0f) { h.wbPh -= floorf_ (h.wbPh); h.wbN = h.wbTo; h.wbTo = h.nr.bi(); }
    const float wbv = h.wbN + (h.wbTo - h.wbN) * smooth01 (h.wbPh);
    const float bowA = (0.04f + 0.22f * bowAmt) * (1.0f + 0.5f * wob * wbv);
    const float bowPos = clampf (S[HW_BPOS] + 0.3f * wob * wbv, 0.0f, 1.0f);
    // (how fast it pulls the string up: from its magnet's hum to BOW's level, about 70 dB, in about SWEL's time; per
    // pass, at each string's pitch: 8.7 f g dB a second)
    const float bowRise = 10.0f / swel, bowK = 1.0f / 6.0f, bowLpC = 1.0f - expf_ (-TWO_PI * 900.0f / sr);
    float bwGm[HOWL_STR];
    bool bowT[HOWL_STR]; bool anyBow = false;
    for (int k = 0; k < HOWL_STR; ++k)
    {
        HowlString& s = h.s[k];
        int r = 1;
        for (int j = 0; j < HOWL_STR; ++j) if (j != k && h.s[j].on && (h.s[j].pitch > s.pitch || (h.s[j].pitch == s.pitch && j < k))) ++r;
        bowT[k] = bowOn && s.on && (strg == 0 || r == strg);
        s.bowE += ((bowT[k] ? 1.0f : 0.0f) - s.bowE) * (bowT[k] ? bowUp : bowDn);
        if (! s.on) s.bowE = 0.0f;
        if (s.bowE > 0.01f) anyBow = true;
    }
    float bwB[HOWL_STR], bwS[HOWL_STR], bhA1[HOWL_STR], bhA2[HOWL_STR], bhG[HOWL_STR], bfA1[HOWL_STR], bfA2[HOWL_STR], bfG[HOWL_STR];
    for (int k = 0; k < HOWL_STR; ++k)
    {
        HowlString& s = h.s[k];
        if (s.gl < 1.0f) { s.gl = minf (1.0f, s.gl + glStep); s.pitch = s.from + (s.to - s.from) * smooth01 (s.gl); }
        s.drPh += fn * s.drRate / sr;
        if (s.drPh >= 1.0f) { s.drPh -= floorf_ (s.drPh); s.drFrom = s.drTo; s.drTo = h.nr.bi(); }
        s.drN = s.drFrom + (s.drTo - s.drFrom) * smooth01 (s.drPh);
        const float cents = drC * s.drN + offC * s.off + wamC * h.wmN * (0.85f + 0.15f * s.off);
        float f = midiHz (s.pitch + cents * 0.01f);
        while (sr / f > (float) (HOWL_LINE - 8)) f *= 2.0f;
        f = minf (f, 0.22f * sr);
        fq[k] = f;
        const float p = clampf (pB * minf (110.0f / f, 2.0f), 0.0f, 0.96f);
        pl[k] = p;
        float D = sr / f - lpDelay (p, f / sr);
        if (D < 3.0f) D = 3.0f;
        Di[k] = (int) floorf_ (D); Dl[k] = D - (float) Di[k];
        const float T60f = t60 * clampf (powf_ (196.0f / f, 0.3f), 0.4f, 1.8f);
        gl[k] = exp2f_ (-9.9657843f / (f * maxf (T60f, 0.02f))) * s.dk;
        // (HAND: the bow's hand mutes the strings it is not on)
        if (anyBow && hand > 0.005f && ! bowT[k]) gl[k] *= exp2f_ (-9.9657843f / (f * (0.06f + 5.0f * (1.0f - hand) * (1.0f - hand))));
        // (BOW: what it senses, FUND the string through its place's tilt, a low-pass at 900 Hz over the neck and its
        // highs lifted at the bridge; OCT, 12TH the harmonic's band, the fundamental's pushed back; what the bowed
        // harmonic loses a pass, made up)
        bwB[k] = bwS[k] = 0.0f; bwGm[k] = 0.0f;
        if (s.bowE > 1e-4f)
        {
            bwGm[k] = clampf (bowRise / (f * (bmod == 0 ? 1.0f : bmod == 1 ? 2.0f : 3.0f)), 0.002f, 0.5f);
            const float Hh = bmod == 0 ? 1.0f : bmod == 1 ? 2.0f : 3.0f, wH = minf (f * Hh, 0.4f * sr) / sr;
            const float hlH = (1.0f - p) / sqrtf_ (maxf (1.0f - 2.0f * p * cos2pi (wH) + p * p, 1e-9f));
            float tilt = 1.0f;
            if (bmod == 0)
            {
                const float q = 1.0f - bowLpC, w = f / sr, dr = 1.0f - q * cos2pi (w), di = q * sin2pi (w), m = bowLpC / maxf (dr * dr + di * di, 1e-12f);
                const float lr = m * dr, li = -m * di, a = 1.0f - 1.8f * bowPos, tr = 1.8f * bowPos + a * lr, ti = a * li;
                tilt = sqrtf_ (tr * tr + ti * ti);
            }
            bwS[k] = 1.0f / maxf (tilt, 0.2f);
            bwB[k] = maxf (1.0f / maxf (gl[k] * minf (hlH, 1.0f), 1e-3f) - 1.0f, 0.0f) * bwS[k];
            const float g = tanpi (wH), g1 = tanpi (minf (f / sr, 0.45f));
            bhA1[k] = 1.0f / (1.0f + g * (g + bowK)); bhA2[k] = g * bhA1[k]; bhG[k] = g;
            bfA1[k] = 1.0f / (1.0f + g1 * (g1 + bowK)); bfA2[k] = g1 * bfA1[k]; bfG[k] = g1;
        }
        // (what a pass round the string loses where the loop sings, at COLR: its decay and damping)
        if (s.on)
        {
            const float hl = (1.0f - p) / sqrtf_ (maxf (1.0f - 2.0f * p * cwF + p * p, 1e-9f));
            lMin = minf (lMin, maxf (1.0f - gl[k] * minf (hl, 1.0f), 1e-5f)); ++nOn;
        }
    }
    const float loss = nOn ? lMin : 0.01f;     // (the string that loses least where the loop sings: the howl grows there)
    // the amp: the preamp's gain (DRIV), the fuzz (its bias, its second stage), the tone stack, the power stage (SAG), LEVL
    const float drv = S[HW_DRIV], fz = S[HW_FUZZ];
    const float g1 = 1.0f + 40.0f * drv * drv, b1 = 0.12f * fz, tb1 = tanhf_ (b1);
    const float g2 = 1.0f + 9.0f * fz, w2 = smooth01 (fz * 1.6f);
    const float sag = S[HW_SAG], th = 1.0f - 0.55f * sag, lev = 0.9f * S[HW_LEVL] * S[HW_LEVL];
    {
        // the tone stack (a passive treble / mid / bass network of the classic kind: H(s) after Yeh and Smith, the
        // bilinear transform), made again when its knobs move
        const float tt = clampf (S[HW_TONE], 0.0f, 1.0f), mm = clampf (S[HW_MID], 0.0f, 1.0f);
        const float ll = expf_ ((clampf (S[HW_BASS], 0.0f, 1.0f) - 1.0f) * 3.4f);     // (a log pot)
        if (absf (tt - h.tsK[0]) > 1e-4f || absf (mm - h.tsK[1]) > 1e-4f || absf (ll - h.tsK[2]) > 1e-4f)
        {
            h.tsK[0] = tt; h.tsK[1] = mm; h.tsK[2] = ll;
            const double R1 = 250e3, R2 = 1e6, R3 = 25e3, R4 = 56e3, C1 = 250e-12, C2 = 20e-9, C3 = 20e-9;
            const double T = tt, M = mm, L = ll;
            const double bb1 = T * C1 * R1 + M * C3 * R3 + L * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
            const double bb2 = T * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - M * M * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                             + M * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + L * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
                             + L * M * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
            const double bb3 = L * M * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4) - M * M * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                             + M * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) + T * C1 * C2 * C3 * R1 * R3 * R4 - T * M * C1 * C2 * C3 * R1 * R3 * R4
                             + T * L * C1 * C2 * C3 * R1 * R2 * R4;
            const double aa0 = 1.0;
            const double aa1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + M * C3 * R3 + L * (C1 * R2 + C2 * R2);
            const double aa2 = M * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + L * M * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                             - M * M * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + L * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
                             + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
            const double aa3 = L * M * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4) - M * M * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                             + M * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4) + L * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;
            const double c = 2.0 * (double) sr, c2 = c * c, c3 = c2 * c;
            const double B0 = -bb1 * c - bb2 * c2 - bb3 * c3, B1 = -bb1 * c + bb2 * c2 + 3.0 * bb3 * c3;
            const double B2 = bb1 * c + bb2 * c2 - 3.0 * bb3 * c3, B3 = bb1 * c - bb2 * c2 + bb3 * c3;
            const double A0 = -aa0 - aa1 * c - aa2 * c2 - aa3 * c3, A1 = -3.0 * aa0 - aa1 * c + aa2 * c2 + 3.0 * aa3 * c3;
            const double A2 = -3.0 * aa0 + aa1 * c + aa2 * c2 - 3.0 * aa3 * c3, A3 = -aa0 + aa1 * c - aa2 * c2 + aa3 * c3;
            h.tb[0] = (float) (B0 / A0); h.tb[1] = (float) (B1 / A0); h.tb[2] = (float) (B2 / A0); h.tb[3] = (float) (B3 / A0);
            h.ta[0] = (float) (A1 / A0); h.ta[1] = (float) (A2 / A0); h.ta[2] = (float) (A3 / A0);
        }
    }
    // CAB (v83): TGHT (a high-pass before the preamp, 25 .. 400 Hz: the bass tight under the drive), BITE (the upper mids
    // lifted into it, up to 13 dB about 2 kHz: its crunch), PRES (the power stage's presence: a shelf over 3.2 kHz, -9 ..
    // +9 dB), DPTH (its depth: the cabinet's low resonance, up to +10 dB where SIZE puts it: 140 Hz on a 1x12, 70 on a
    // 4x12), CAB (the speaker: dark, its top rolled off early with a warm, woody low mid; crisp, its top open with a
    // modern speaker's bite about 3 kHz; v82's at 12 o'clock), MIC (on what is recorded only: close, the cone's
    // proximity and its top; back, off the cone's axis, and the room's reflections)
    const float cabTg = S[HW_TGHT], cabBt = S[HW_BITE], cabPr = (S[HW_PRES] - 0.5f) * 2.0f, cabDp = S[HW_DPTH], cabU = (S[HW_CAB] - 0.5f) * 2.0f;
    const float cabSz = clampf (S[HW_SIZE], 0.0f, 1.0f), cabMic = (S[HW_MIC] - 0.5f) * 2.0f;
    {
        const float kn[7] = { cabTg, cabBt, cabPr, cabDp, cabU, cabSz, cabMic };
        bool moved = false; for (int k = 0; k < 7; ++k) if (absf (kn[k] - h.cabK[k]) > 1e-4f) moved = true;
        if (moved)
        {
            for (int k = 0; k < 7; ++k) h.cabK[k] = kn[k];
            if (cabTg < 0.005f) h.tg.flat(); else bqHp (h.tg, 25.0f * powf_ (16.0f, cabTg), 0.707f, sr);
            if (cabBt < 0.005f) h.bt.flat(); else bqPeak (h.bt, 1800.0f + 900.0f * cabBt, 0.9f, 13.0f * cabBt, sr);
            if (absf (cabPr) < 0.01f) h.pr.flat(); else bqShelf (h.pr, 3200.0f, 9.0f * cabPr, true, sr);
            if (cabDp < 0.005f) h.dp.flat(); else bqPeak (h.dp, 140.0f * exp2f_ (-cabSz), 1.4f, 10.0f * cabDp, sr);
            if (absf (cabU) < 0.01f) h.ck.flat(); else if (cabU < 0.0f) bqPeak (h.ck, 500.0f, 0.8f, -3.0f * cabU, sr); else bqPeak (h.ck, 3000.0f, 1.1f, 5.0f * cabU, sr);
            if (absf (cabMic) < 0.01f) { h.mc.flat(); h.mh.flat(); }
            else if (cabMic < 0.0f) { bqShelf (h.mc, 160.0f, -6.0f * cabMic, false, sr); bqPeak (h.mh, 5500.0f, 1.0f, -4.0f * cabMic, sr); }
            else { bqShelf (h.mc, 160.0f, -4.0f * cabMic, false, sr); bqShelf (h.mh, 6000.0f, -6.0f * cabMic, true, sr); }
        }
    }
    // (the speaker's roll-off: v82's 4.2 and 6.8 kHz at 12 o'clock, down to 2.1 and 3.4 dark, up to 8.5 and 13.5 crisp;
    // its low end held at 35 Hz on a 4x12, 70 on a 1x12)
    const float cbF1 = 4200.0f * powf_ (cabU < 0.0f ? 0.5f : 2.0238f, absf (cabU)), cbF2 = 6800.0f * powf_ (cabU < 0.0f ? 0.5f : 1.9853f, absf (cabU));
    // FDBK: the loop's gain. Round the loop, a string's harmonic at COLR gives back what reaches it over what a pass
    // loses (1 / loss), the amp multiplies it by its small-signal gain there (G: the preamp and the fuzz, the tone stack,
    // the power stage, the cabinet), the air and its colour by 1: the loop takes off where fb G / loss passes 1. So fb is
    // FDBK's gain times loss / G: half way (fK 1) the threshold, whatever SUST, DAMP, DRIV, the tone stack or COLR (a
    // chord ringing hard through a driven amp lowers its gain: it takes more). Blooming (BLOM), the guitar before the
    // amp or turned away
    // (the cabinet's two low-pass poles, 4.2 and 6.8 kHz at CAB's 12 o'clock)
    const float c1 = 1.0f - expf_ (-TWO_PI * minf (cbF1, 0.45f * sr) / sr), c2 = 1.0f - expf_ (-TWO_PI * minf (cbF2, 0.45f * sr) / sr);
    float ampG, fuzz0 = 1.0f, tube0 = 1.0f;    // (the amp's gain at COLR for what grows in the loop)
    {
        auto tsMag = [&] (float f) {
            const float w = f / sr, c1w = cos2pi (w), s1w = -sin2pi (w), c2w = cos2pi (2.0f * w), s2w = -sin2pi (2.0f * w), c3w = cos2pi (3.0f * w), s3w = -sin2pi (3.0f * w);
            const float nr = h.tb[0] + h.tb[1] * c1w + h.tb[2] * c2w + h.tb[3] * c3w, ni = h.tb[1] * s1w + h.tb[2] * s2w + h.tb[3] * s3w;
            const float dr = 1.0f + h.ta[0] * c1w + h.ta[1] * c2w + h.ta[2] * c3w, di = h.ta[0] * s1w + h.ta[1] * s2w + h.ta[2] * s3w;
            return sqrtf_ ((nr * nr + ni * ni) / maxf (dr * dr + di * di, 1e-12f));
        };
        auto cabMag = [&] (float f) {
            const float cw = cos2pi (minf (f, 0.45f * sr) / sr);
            auto lp = [&] (float c) { const float q = 1.0f - c; return c / sqrtf_ (maxf (1.0f - 2.0f * q * cw + q * q, 1e-12f)); };
            return lp (c1) * lp (c2);
        };
        const float fF = minf (bpF, 0.2f * sr);
        const float tube = 1.0f - tb1 * tb1, b0 = 0.35f * fz * 0.4f;
        const float fuzz = (1.0f - w2) + w2 * g2 * (1.0f + 0.6f * fz) / ((1.0f + b0) * (1.0f + b0));
        // (and how far what it plays already drives it: its slope measured over the last chunk)
        const float cabG = bqMag (h.tg, fF, sr) * bqMag (h.bt, fF, sr) * bqMag (h.pr, fF, sr) * bqMag (h.dp, fF, sr) * bqMag (h.ck, fF, sr);
        ampG = maxf (0.3f * g1 * tube * fuzz * tsMag (fF) * 3.2f * 1.4f * h.sag * cabMag (fF) * cabG * clampf (h.gSl, 0.01f, 1.0f), 1e-5f);
        fuzz0 = fuzz; tube0 = tube;
    }
    // (its gain: its threshold about half way, then past it eight times as much at its top: the howl takes over the
    // chord however hard the amp is driven; LIMT keeps it where it is wanted)
    const float fdbk = P[P_D], fK = fdbk <= 0.5f ? 4.0f * fdbk * fdbk : exp2f_ (6.0f * (fdbk - 0.5f));
    // (the guitar's angle squared: turned away, the loop goes however hard the amp's slope is made up for)
    const float fb = fK * loss * howlFbkS / ampG * h.gate * h.gate * (0.12f + 0.88f * h.bloom);
    const float fb0 = h.fbS, dFb = (fb - fb0) / fn;
    h.fbS = fb;
    const float nz = 2.5e-4f * minf (fK, 1.0f) * h.gate;
    // RESO: the strings ring each other through the bridge. A lossless mix (an orthogonal matrix: the Cayley transform of
    // a skew-symmetric coupling, every pair c apart): it moves energy between the strings, where they share a frequency,
    // and never makes any. (v81's first coupling added the other strings to each one: past RESO's middle, or on a long
    // SUST, the strings rang themselves up to their rail, a DC they all shared too, and the amp went silent.)
    if (absf (S[HW_RESO] - h.mixK) > 1e-5f)
    {
        h.mixK = S[HW_RESO];
        const double c = (double) (h.mixK * h.mixK) * 0.012 * 0.5;
        double a[HOWL_STR][2 * HOWL_STR];
        for (int r = 0; r < HOWL_STR; ++r)
            for (int q = 0; q < HOWL_STR; ++q)
            {
                const double b = r == q ? 0.0 : (r < q ? c : -c);
                a[r][q] = (r == q ? 1.0 : 0.0) - b;                 // I - B
                a[r][HOWL_STR + q] = (r == q ? 1.0 : 0.0) + b;      // I + B
            }
        // (I - B) M = I + B, by Gauss-Jordan (I - B is diagonally dominant: no pivoting needed)
        for (int r = 0; r < HOWL_STR; ++r)
        {
            const double d = 1.0 / a[r][r];
            for (int q = 0; q < 2 * HOWL_STR; ++q) a[r][q] *= d;
            for (int o = 0; o < HOWL_STR; ++o)
                if (o != r) { const double f = a[o][r]; if (f != 0.0) for (int q = 0; q < 2 * HOWL_STR; ++q) a[o][q] -= f * a[r][q]; }
        }
        for (int r = 0; r < HOWL_STR; ++r) for (int q = 0; q < HOWL_STR; ++q) h.mix[r * HOWL_STR + q] = (float) a[r][HOWL_STR + q];
    }
    const bool coupled = h.mixK > 0.0f;
    // LOOP (v82). MOVE: the guitar swaying before the amp, a slow wander of its distance (up to half an octave of the
    // air's length either way: another harmonic howls) and of its angle (the loop's gain)
    const float mv = S[HW_MOVE];
    h.mvPh += fn * (0.05f + 0.3f * mv) / sr;
    if (h.mvPh >= 1.0f) { h.mvPh -= floorf_ (h.mvPh); for (int q = 0; q < 2; ++q) { h.mvFrom[q] = h.mvTo[q]; h.mvTo[q] = h.nr.bi(); } }
    for (int q = 0; q < 2; ++q) h.mvN[q] = h.mvFrom[q] + (h.mvTo[q] - h.mvFrom[q]) * smooth01 (h.mvPh);
    // TIME: the air's length, 0.3 .. 80 ms: the micro-delay; it glides (120 ms): turned, it bends the howl as a delay's
    // speed would
    const float tmT = clampf (0.0003f * powf_ (266.66667f, S[HW_TIME]) * exp2f_ (0.5f * mv * h.mvN[0]) * sr, 3.0f, (float) (COMB_SIZE - 8));
    if (h.tmS < 0.0f) h.tmS = tmT;
    const float tmK = 1.0f - expf_ (-1.0f / (0.12f * sr));
    // COLR, FOCS: the loop's colour, two band-passes in the air (70 Hz .. 6 kHz, their peak at 1; Q 0.5 .. 8 each:
    // from a tone control on the whole loop, the chord sustained, to one harmonic singing; two, steep enough for a
    // string's harmonic at COLR to win over its fundamental, which loses far less a pass)
    const float bpQ = 0.5f * powf_ (16.0f, S[HW_FOCS]);
    const float bpG = tanpi (bpF / sr), bpK = 1.0f / bpQ, bpA1 = 1.0f / (1.0f + bpG * (bpG + bpK)), bpA2 = bpG * bpA1;
    // (LIMT's ear: one band-pass on COLR, half as sharp: the howl heard wherever near it the strings let it sing)
    const float mbK = 1.0f / maxf (0.5f, 0.5f * bpQ), mbA1 = 1.0f / (1.0f + bpG * (bpG + mbK)), mbA2 = bpG * mbA1;
    // PKUP: the pickup hearing the amp itself (a microphonic squeal): the air straight into the amp, the strings left
    // out: the loop's own pitch, on TIME and COLR (the strings' threshold, FDBK, for it too)
    const float pk = S[HW_PKUP], fbD = fK * pk * pk * howlFbkD / ampG * h.gate * h.gate * (1.0f + 0.35f * mv * h.mvN[1]);
    // LIMT: how high the howl may rise (a regulator on what feeds back, listening to the loop's band about COLR in the
    // air: 0.25 .. 0.6 of the amp's top, off at its top): low, the chord held, a howl singing under it (a chord alone
    // rings about 0.3 there); up, the howl takes it all (it saturates the amp about 0.6). Alone (no chord), the howl
    // held at that level
    const float limT = S[HW_LIMT] >= 0.99f ? 1e9f : 0.25f + 0.35f * S[HW_LIMT];
    const float limA = tcCoef (0.005f, sr), limR = tcCoef (0.25f, sr), limGA = tcCoef (0.01f, sr), limGR = tcCoef (0.4f, sr);
    // NOIS: the amp's hiss and crackle, reinjected with what it plays (the howl grows out of it); HUM: the pickups'
    // mains hum, 50 Hz and its buzz
    const float nzA = 0.004f * S[HW_NOIS] * S[HW_NOIS] * h.gate, crA = 0.02f * S[HW_NOIS] * S[HW_NOIS] * h.gate;
    const float humA = 0.02f * S[HW_HUM] * S[HW_HUM] * h.gate, humW = 50.0f / sr;
    const float fbMv = 1.0f + 0.35f * mv * h.mvN[1];
    // the pick (EXCT: its hardness), the power stage's and the supply's times, the cabinet
    const float exct = S[HW_EXCT], hard = 0.3f + 0.7f * exct;
    const float pwA = tcCoef (0.003f, sr), pwR = tcCoef (0.08f, sr), sgA = tcCoef (0.04f, sr), sgR = tcCoef (0.3f, sr);
    const float dcR = 1.0f - TWO_PI * 8.0f / sr, hpR = 1.0f - TWO_PI * 35.0f * exp2f_ (1.0f - cabSz) / sr;
    const float enR = tcCoef (0.02f, sr);
    const bool micOn = absf (cabMic) >= 0.01f;
    const float micRoom = maxf (cabMic, 0.0f), micLp = 1.0f - expf_ (-TWO_PI * 5000.0f / sr);
    const int micD1 = maxi (1, (int) (0.0031f * sr)), micD2 = maxi (1, (int) (0.0053f * sr));
    const int mask = HOWL_LINE - 1;
    float* lines = v.lbuf;
    float* air = v.mcomb;
    float lvMax = 0.0f, loopMax = 0.0f, outMax = 0.0f;
    const float lvRel = expf_ (-1.0f / (0.05f * sr));
    float slSum = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        // the amp's sound reaching the guitar
        h.tmS += (tmT - h.tmS) * tmK;
        const int dI = (int) floorf_ (h.tmS); const float dF = h.tmS - (float) dI;
        const int am = COMB_SIZE - 1, ra = h.aw - dI;
        const float a0 = air[(ra + 1) & am], a1 = air[ra & am], a2 = air[(ra - 1) & am], a3 = air[(ra - 2) & am];
        const float ac1 = 0.5f * (a2 - a0), ac2 = a0 - 2.5f * a1 + 2.0f * a2 - 0.5f * a3, ac3 = 0.5f * (a3 - a0) + 1.5f * (a1 - a2);
        const float aR = ((ac3 * dF + ac2) * dF + ac1) * dF + a1;
        // (the loop's colour: a TPT state-variable band-pass, normalised)
        const float bv1 = bpA1 * h.bp1 + bpA2 * (aR - h.bp2), bv2 = h.bp2 + bpG * bv1;
        h.bp1 = 2.0f * bv1 - h.bp1; h.bp2 = 2.0f * bv2 - h.bp2;
        const float bx = bv1 * bpK;
        const float bw1 = bpA1 * h.bp3 + bpA2 * (bx - h.bp4), bw2 = h.bp4 + bpG * bw1;
        h.bp3 = 2.0f * bw1 - h.bp3; h.bp4 = 2.0f * bw2 - h.bp4;
        const float a = bw1 * bpK;
        {
            const float mv1 = mbA1 * h.mb1 + mbA2 * (aR - h.mb2), mv2 = h.mb2 + bpG * mv1;
            h.mb1 = 2.0f * mv1 - h.mb1; h.mb2 = 2.0f * mv2 - h.mb2;
            const float ap = absf (mv1 * mbK), ar = absf (aR);
            h.limEnv = ap > h.limEnv ? ap + (h.limEnv - ap) * limA : ap + (h.limEnv - ap) * limR;
            h.airEnv = ar > h.airEnv ? ar + (h.airEnv - ar) * limA : ar + (h.airEnv - ar) * limR;
            // (over its share the loop turns against it: the strings on ∞ would hold what it rang into them, cutting it
            // would not take it back; turned round, the same path damps it until it is back down)
            const float sh = h.limEnv, lt = sh > limT ? maxf (1.0f - 4.0f * (sh / limT - 1.0f), -0.25f) : 1.0f;
            h.lim = lt < h.lim ? lt + (h.lim - lt) * limGA : lt + (h.lim - lt) * limGR;
        }
        const float fbI = (fb0 + dFb * (float) (i + 1)) * fbMv * h.lim;
        const float exc = a * fbI + nz * h.nr.bi();
        const float dIn = a * fbD * h.lim;
        float sum = 0.0f, ys[HOWL_STR], es[HOWL_STR];
        for (int k = 0; k < HOWL_STR; ++k)
        {
            HowlString& s = h.s[k];
            ys[k] = es[k] = 0.0f;
            if (! s.on) continue;
            float* line = lines + k * HOWL_LINE;
            // the pick: once its wait is over, a period of triangle at the pick's place
            float e = 0.0f;
            if (s.pend >= 0)
            {
                if (s.pend == 0)
                {
                    for (int q = 0; q < HOWL_LINE; ++q) line[q] *= 0.5f;
                    s.lp *= 0.5f;
                    s.ex = 0; s.exLen = maxi (2, (int) (sr / fq[k] + 0.5f));
                    s.exPos = clampf (0.18f + 0.05f * v.rng.bi() - 0.1f * exct, 0.05f, 0.45f);
                    s.exHard = 1.0f - expf_ (-TWO_PI * minf (500.0f * exp2f_ (hard * 4.5f) * (0.5f + s.pv), 0.45f * sr) / sr);
                    s.exNz = 0.06f + 0.12f * exct;
                    s.exLp = 0.0f;
                    s.lvl = maxf (s.lvl, 0.05f);
                }
                --s.pend;
            }
            if (s.ex < s.exLen)
            {
                const float u = (float) s.ex / (float) s.exLen, b = s.exPos;
                float sh = (u < b ? u / b : (1.0f - u) / (1.0f - b)) - 0.5f;
                sh += s.exNz * v.rng.bi() * (1.0f - u);
                s.exLp += s.exHard * (sh - s.exLp);
                e = s.exLp * 0.5f * s.pv;
                ++s.ex;
            }
            // the line read between its samples (a cubic: its length glides without the damping moving)
            const int rp = s.w - Di[k];
            const float x0 = line[(rp + 1) & mask], x1 = line[rp & mask], x2 = line[(rp - 1) & mask], x3 = line[(rp - 2) & mask];
            const float fr = Dl[k];
            const float c1h = 0.5f * (x2 - x0), c2h = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3, c3h = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
            const float x = ((c3h * fr + c2h) * fr + c1h) * fr + x1;
            s.lp += (1.0f - pl[k]) * (x - s.lp);
            const float y = s.lp * gl[k];
            ys[k] = y; es[k] = e;
            // the bow driving it back (a little hum of its magnet: it wakes a still string)
            if (s.bowE > 1e-4f)
            {
                float sense;
                if (bmod == 0) { s.bowLp += bowLpC * (y - s.bowLp); sense = 1.8f * bowPos * y + (1.0f - 1.8f * bowPos) * s.bowLp; }
                else
                {
                    const float v1 = bhA1[k] * s.bh1 + bhA2[k] * (y - s.bh2), v2 = s.bh2 + bhG[k] * v1;
                    s.bh1 = 2.0f * v1 - s.bh1; s.bh2 = 2.0f * v2 - s.bh2;
                    sense = v1 * bowK;
                }
                const float reg = clampf (bwGm[k] * (1.0f - s.lvl / bowA), -0.3f, bwGm[k]);
                float eb = s.bowE * (bwB[k] + reg * bwS[k]) * sense;
                if (bmod != 0)
                {
                    const float u1 = bfA1[k] * s.bf1 + bfA2[k] * (y - s.bf2), u2 = s.bf2 + bfG[k] * u1;
                    s.bf1 = 2.0f * u1 - s.bf1; s.bf2 = 2.0f * u2 - s.bf2;
                    eb -= s.bowE * 0.03f * u1 * bowK;
                }
                es[k] += eb + s.bowE * 1e-4f * h.nr.bi();
            }
            sum += y;
            const float aw = absf (y);
            s.lvl = aw > s.lvl ? aw : s.lvl * lvRel;
        }
        // what goes in: the strings through the bridge, the air, the pick
        for (int k = 0; k < HOWL_STR; ++k)
        {
            HowlString& s = h.s[k];
            if (! s.on) continue;
            float w = ys[k];
            if (coupled) { const float* m = h.mix + k * HOWL_STR; w = 0.0f; for (int j = 0; j < HOWL_STR; ++j) w += m[j] * ys[j]; }
            w += exc + es[k];
            if (w > 1.5f) w = 1.5f + tanhf_ (w - 1.5f); else if (w < -1.5f) w = -1.5f + tanhf_ (w + 1.5f);
            lines[k * HOWL_LINE + s.w] = w;
            s.w = (s.w + 1) & mask;
            s.yz = ys[k];
        }
        loopMax = maxf (loopMax, absf (sum));
        // the preamp: a tube stage (biased by FUZZ: even harmonics), then the fuzz (its knee, asymmetric, starved under
        // a falling signal: it sputters), both antialiased (first-order, by their antiderivatives)
        // (the stages' inputs held where their curves are flat: the antiderivatives' differences stay exact in floats)
        // what the pickup sends the amp: the strings, the air itself (PKUP), the hiss and its crackle, the hum
        float pu = sum + dIn, slT = 1.0f;
        if (nzA > 0.0f) pu += nzA * h.nr.bi();
        if (crA > 0.0f && --h.crk <= 0) { pu += crA * h.nr.bi(); h.crk = (int) (sr * (0.02f + 0.6f * h.nr.uni()) / (0.3f + 3.0f * S[HW_NOIS])); }
        if (humA > 0.0f)
        {
            h.humPh += humW; if (h.humPh >= 1.0f) h.humPh -= 1.0f;
            const float sn = sin2pi (h.humPh);
            pu += humA * (0.6f * sn + 0.8f * (absf (sn) - 0.63662f) + 0.25f * sn * sn * sn);
        }
        pu = h.bt.tick (h.tg.tick (pu));                // (CAB: TGHT, BITE before the preamp)
        const float u1 = clampf (pu * 0.3f * g1, -30.0f, 30.0f);
        float y1;
        {
            const float F = lnCosh (u1 + b1) - u1 * tb1, du = u1 - h.z1;
            y1 = absf (du) > 2e-3f ? (F - h.f1) / du : tanhf_ (0.5f * (u1 + h.z1) + b1) - tb1;
            h.z1 = u1; h.f1 = F;
            const float tt = tanhf_ (u1 + b1); slT = (1.0f - tt * tt) / tube0;
        }
        float yv = y1;
        if (w2 > 0.0f)
        {
            h.env = maxf (absf (y1), h.env * enR);
            const float bias = 0.35f * fz * (0.4f - minf (h.env, 0.4f));
            const float u2 = clampf (y1 * g2 + bias, -40.0f, 40.0f);
            { const float d2 = 1.0f + absf (u2); slT *= ((1.0f - w2) + w2 * g2 * (1.0f + 0.6f * fz) / (d2 * d2)) / fuzz0; }
            const float F = absf (u2) - log2f_ (1.0f + absf (u2)) * LN2, du = u2 - h.z2;
            const float y2 = absf (du) > 2e-3f ? (F - h.f2) / du : (0.5f * (u2 + h.z2)) / (1.0f + absf (0.5f * (u2 + h.z2)));
            h.z2 = u2; h.f2 = F;
            // (a starved fuzz: the bottom of its swing gated)
            const float gate = 0.6f + 0.4f * smooth01 (h.env * (8.0f - 6.0f * fz));
            yv = y1 + (y2 * (1.0f + 0.6f * fz) * (1.0f - 0.35f * fz * (1.0f - gate)) - y1) * w2;
        }
        float o = yv - h.dcX + dcR * h.dcY; h.dcX = yv; h.dcY = o;
        // the tone stack (its loss made up)
        {
            const float yt = h.tb[0] * o + h.ts[0];
            h.ts[0] = h.tb[1] * o - h.ta[0] * yt + h.ts[1];
            h.ts[1] = h.tb[2] * o - h.ta[1] * yt + h.ts[2];
            h.ts[2] = h.tb[3] * o - h.ta[2] * yt;
            o = yt * 3.2f;
        }
        // the power stage: its compression (2:1 over the threshold SAG lowers), the supply giving way under a long note
        const float ao = absf (o);
        h.pwEnv = ao > h.pwEnv ? ao + (h.pwEnv - ao) * pwA : ao + (h.pwEnv - ao) * pwR;
        if (h.pwEnv > th) { const float pg = sqrtf_ (th / h.pwEnv); o *= pg; slT *= pg; }
        const float sT = 1.0f - 0.45f * sag * minf (h.pwEnv, 1.2f);
        h.sag = sT < h.sag ? sT + (h.sag - sT) * sgA : sT + (h.sag - sT) * sgR;
        { const float xs = o * h.sag * 1.4f, x2 = xs * xs; if (x2 < 9.0f) { const float d = (9.0f - x2) / (3.0f * (3.0f + x2)); slT *= d * d; } else slT = 0.0f; }
        slSum += slT;
        o = softclip (o * h.sag * 1.4f);
        o = h.dp.tick (h.pr.tick (o));                    // (CAB: PRES, DPTH)
        // the cabinet: its low end held (35 Hz; 70 on a 1x12), its top rolled off (4.2 and 6.8 kHz at CAB's 12 o'clock),
        // the speaker's own colour
        float c = o - h.hpX + hpR * h.hpY; h.hpX = o; h.hpY = c;
        h.cb1 += c1 * (c - h.cb1); h.cb2 += c2 * (h.cb1 - h.cb2);
        const float ampOut = h.ck.tick (h.cb2);
        // what is recorded: MIC close (the cone's proximity, its top), or back (off its axis, the room's floor and wall)
        float rec = ampOut;
        if (micOn)
        {
            rec = h.mh.tick (h.mc.tick (rec));
            if (micRoom > 0.0f)
            {
                const float r1 = air[(h.aw - micD1) & (COMB_SIZE - 1)], r2 = air[(h.aw - micD2) & (COMB_SIZE - 1)];
                h.mrl += micLp * (0.4f * r1 + 0.25f * r2 - h.mrl);
                rec += micRoom * h.mrl;
            }
        }
        air[h.aw] = ampOut;
        h.aw = (h.aw + 1) & (COMB_SIZE - 1);
        out[i] = rec * lev;
        outMax = maxf (outMax, absf (ampOut));
    }
    // (the amp's slope over this chunk, for the next one's loop gain: what grows in the loop is amplified by that much)
    h.gSl += (slSum / fn - h.gSl) * (1.0f - expf_ (-fn / (0.03f * sr)));
    // (a numeric failure: the strings and the amp start again from silence)
    bool bad = isBad (h.tg.z1) || isBad (h.bt.z1) || isBad (h.pr.z1) || isBad (h.dp.z1) || isBad (h.ck.z1) || isBad (h.mc.z1) || isBad (h.mh.z1) || isBad (h.mrl) || isBad (h.gSl) || isBad (h.mb1) || isBad (h.bp1) || isBad (h.bp2) || isBad (h.bp3) || isBad (h.bp4) || isBad (h.lim) || isBad (h.ts[0]) || isBad (h.ts[1]) || isBad (h.ts[2]) || isBad (h.cb2) || isBad (h.f1) || isBad (h.f2);
    for (int k = 0; k < HOWL_STR; ++k) if (isBad (h.s[k].lp) || isBad (h.s[k].bh1) || isBad (h.s[k].bf1) || isBad (h.s[k].bowLp)) bad = true;
    if (bad)
    {
        for (int q = 0; q < HOWL_STR * HOWL_LINE; ++q) lines[q] = 0.0f;
        for (int q = 0; q < COMB_SIZE; ++q) air[q] = 0.0f;
        for (int k = 0; k < HOWL_STR; ++k) { h.s[k].lp = h.s[k].yz = 0.0f; h.s[k].lvl = 0.0f; }
        h.z1 = h.f1 = h.z2 = h.f2 = h.dcX = h.dcY = h.hpX = h.hpY = h.cb1 = h.cb2 = 0.0f;
        h.bp1 = h.bp2 = h.bp3 = h.bp4 = h.mb1 = h.mb2 = 0.0f; h.lim = 1.0f; h.limEnv = h.airEnv = 0.0f; h.gSl = 1.0f;
        for (int k = 0; k < 3; ++k) h.ts[k] = 0.0f;
        { HowlBq* bq[7] = { &h.tg, &h.bt, &h.pr, &h.dp, &h.ck, &h.mc, &h.mh }; for (HowlBq* q : bq) q->z1 = q->z2 = 0.0f; }
        h.mrl = 0.0f;
        for (int k = 0; k < HOWL_STR; ++k) { HowlString& s = h.s[k]; s.bowLp = s.bh1 = s.bh2 = s.bf1 = s.bf2 = 0.0f; }
        for (int i = 0; i < n; ++i) out[i] = 0.0f;
        outMax = 0.0f;
    }
    // a string fallen silent (damped, not held by the loop) stops
    for (int k = 0; k < HOWL_STR; ++k)
    {
        HowlString& s = h.s[k];
        if (s.on && s.pend < 0 && s.ex >= s.exLen && s.lvl < 1e-6f && (h.gate < 0.01f || fb <= 0.0f) && s.dk < 1.0f) { s.on = 0; s.yz = 0.0f; }
        lvMax = maxf (lvMax, s.lvl);
    }
    h.outLv = maxf (outMax, h.outLv * expf_ (-fn / (0.15f * sr)));
    h.loopLv = maxf (loopMax, h.loopLv * expf_ (-fn / (0.15f * sr)));
    bool pending = false;
    for (int k = 0; k < HOWL_STR; ++k) if (h.s[k].pend >= 0) pending = true;
    // (alive while the gate is open and the loop could wake, whatever the strings ring)
    const float alive = h.held && fb > 0.0f ? 0.01f : 0.0f;
    v.level = maxf (maxf (lvMax, outMax), pending ? 0.01f : alive);
    v.ns += (u32) n;
}

// for the screen (through HALO's): 4, the strings sounding, their notes; the chord (root, type, voicing), what the
// strings hold; then each string: its note (-999: silent), level, glide; the output, the bloom, the gate, the supply
void Engine::howlInfo (int ti, float* out) const
{
    const Track& t = tr[ti];
    const Voice& v = t.voice[t.cur];
    out[0] = 4.0f;
    for (int s = 0; s < HALO_SLOTS; ++s) { out[12 + 3 * s] = -999.0f; out[13 + 3 * s] = 0.0f; out[14 + 3 * s] = 1.0f; }
    if (! v.active || v.machine != M_HOWL) return;
    const HowlState& h = v.hw;
    int n = 0;
    for (int k = 0; k < HOWL_STR; ++k)
    {
        const HowlString& s = h.s[k];
        out[12 + 3 * k] = s.on ? s.pitch : -999.0f;
        out[13 + 3 * k] = s.on ? s.lvl : 0.0f;
        out[14 + 3 * k] = s.gl;
        if (s.on && n < HOWL_STR) out[2 + n++] = s.pitch;
    }
    out[1] = (float) n;
    out[8] = h.root; out[9] = (float) h.type; out[10] = (float) h.voic; out[11] = h.loopLv;
    out[30] = h.outLv; out[31] = h.bloom; out[32] = h.gate; out[33] = h.sag;
    for (int k = 0; k < HOWL_STR; ++k) out[34 + k] = h.s[k].on ? h.s[k].bowE : 0.0f;     // (v83: the bow on each string)
}

} // namespace sk
