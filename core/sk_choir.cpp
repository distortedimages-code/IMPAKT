// IMPAKT — CHOIR, a sung voice (see sk_choir.h)
#include "sk_engine.h"

namespace sk {

namespace {
// the vowels, sung by a female alto: MM (the mouth closed) OO OH AH EH EE. F1..F5 (Hz), B1..B5 (Hz), the level each
// needs to sound as loud as the others, the closed mouth's share
const float VF[CHOIR_VOWELS][CHOIR_FORM] = {
    { 280.0f, 1050.0f, 2400.0f, 3500.0f, 4500.0f },
    { 340.0f,  870.0f, 2600.0f, 3500.0f, 4500.0f },
    { 460.0f,  860.0f, 2700.0f, 3600.0f, 4600.0f },
    { 820.0f, 1200.0f, 2800.0f, 3700.0f, 4700.0f },
    { 560.0f, 1900.0f, 2750.0f, 3700.0f, 4700.0f },
    { 330.0f, 2500.0f, 3200.0f, 3900.0f, 4800.0f } };
const float VB[CHOIR_VOWELS][CHOIR_FORM] = {
    { 60.0f, 250.0f, 300.0f, 350.0f, 400.0f },
    { 60.0f,  90.0f, 150.0f, 200.0f, 250.0f },
    { 70.0f,  90.0f, 140.0f, 200.0f, 260.0f },
    { 90.0f, 110.0f, 160.0f, 220.0f, 280.0f },
    { 70.0f, 100.0f, 160.0f, 220.0f, 280.0f },
    { 60.0f, 100.0f, 180.0f, 230.0f, 280.0f } };
const float VG[CHOIR_VOWELS] = { 2.74f, 1.33f, 1.075f, 0.476f, 1.05f, 1.79f };
const float VHUM[CHOIR_VOWELS] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
// PART: the lines sung with it, in steps of a seven-note scale (on a quantised track) or in semitones (otherwise):
// SOLO, a third over, a fifth over, both (a triad), a third under, an octave under
const int PART_N[CHOIR_PARTN] = { 1, 2, 2, 3, 2, 2 };
const int PART_DEG[CHOIR_PARTN][CHOIR_PARTS] = { { 0, 0, 0 }, { 0, 2, 0 }, { 0, 4, 0 }, { 0, 2, 4 }, { 0, -2, 0 }, { 0, -7, 0 } };
const int PART_SEMI[CHOIR_PARTN][CHOIR_PARTS] = { { 0, 0, 0 }, { 0, 4, 0 }, { 0, 7, 0 }, { 0, 4, 7 }, { 0, -3, 0 }, { 0, -12, 0 } };
// SHIM: the SPACE's shift (the VERB machine's SHFT): none, an octave up, a fifth up, an octave down
const int SHIM_IDX[CHOIR_SHIMS] = { 0, 12, 11, 1 };
const float BASE_C4 = 261.63f;

inline float smooth01 (float x) { const float o = clampf (x, 0.0f, 1.0f); return o * o * (3.0f - 2.0f * o); }

// a part's interval: `deg` steps of the scale from the scale note nearest abs (MIDI; an octave: -7), else `semi`
float partSemis (float abs, int deg, int semi, u32 mask)
{
    if (deg == -7 || ! mask) return (float) semi;
    int n0 = (int) floorf_ (abs + 0.5f);
    for (int d = 0; d < 7; ++d)
    {
        if ((mask >> (((n0 + d) % 12 + 12) % 12)) & 1u) { n0 += d; break; }
        if ((mask >> (((n0 - d) % 12 + 12) % 12)) & 1u) { n0 -= d; break; }
    }
    int n = n0; const int s = deg > 0 ? 1 : -1;
    for (int k = 0; k != deg; k += s)
    {
        int guard = 0;
        do { n += s; } while (! ((mask >> (((n % 12) + 12) % 12)) & 1u) && ++guard < 12);
    }
    return (float) (n - n0);
}
}

void Engine::choirTrigger (Voice& v, const float* P)
{
    ChoirState& c = v.cs;
    const float tuneRaw = (P[P_TUNE] - 0.5f) * 48.0f + v.note;
    const u32 qm = v.quant ? quantMask() : 0u;
    const float tgt = qm ? qPitch (tuneRaw, BASE_C4, qm) : tuneRaw;
    const bool sounding = ! c.fresh && c.stage != 0 && c.env > 1e-3f;
    if (! sounding)
    {
        // a new phrase: every singer from silence, the onset a little under the note, the breath first
        for (int k = 0; k < CHOIR_VOX; ++k)
        {
            ChoirSinger& s = c.s[k];
            s.ph = v.rng.uni(); s.dph0 = 0.0f; s.oq = 0.6f; s.amp = 1.0f; s.per = 1.0f;
            s.tilt = s.tilt2 = 0.0f; s.hum = 0.0f; s.nz1 = s.nz2 = 0.0f;
            for (int f = 0; f < CHOIR_FORM; ++f) { s.r1[f] = s.r2[f] = 0.0f; }
            const int e = k % CHOIR_SING;
            s.size = e == 0 ? 1.0f : 1.0f + 0.035f * v.rng.bi();
            s.det = e == 0 ? 0.0f : v.rng.bi();
            s.vibPh = v.rng.uni(); s.vibRate = 1.0f + 0.07f * v.rng.bi();
            s.drift = s.driftT = 0.0f; s.driftLeft = 0;
            s.wait = e == 0 ? 0.0f : (0.006f + 0.04f * v.rng.uni()) * sr;
            s.g = 0.0f;
        }
        c.pitch = c.from = c.to = tgt; c.gx = 1.0f; c.gInc = 0.0f;
        c.scoop = -P[P_CSCP] * 1.2f;
        c.env = 0.0f; c.vowel = P[P_A]; c.breath = 1.0f; c.hp = 0.0f; c.lvl = 0.0f;
        c.age = 0.0f; c.poffOk = 0; c.exS = c.rib ? 0.35f + 0.65f * c.expr : 1.0f;
        c.fresh = 0;
    }
    else
    {
        // legato: the voice sings on, gliding to the new note; its vibrato settles again
        c.age = minf (c.age, 0.25f);
    }
    if (! c.rib) c.expr = 1.0f;
    c.stage = 1;
    c.held = 1;
    c.holdLeft = v.holdSec > 0.0f ? v.holdSec * sr : -1.0f;
}

void Engine::choirRender (Voice& v, const float* P, float tuneRaw, float tuneSt, u32 qm, int n, float* out)
{
    ChoirState& c = v.cs;
    const Track& t = tr[v.ti];
    const float fn = (float) n;
    // the gate: GATE, a pad, a key or the ribbon held, or until the next trig while the transport runs and the track
    // plays (a stop or a mute lets it go)
    if (c.held)
    {
        bool letGo = false;
        if (c.rib) { }
        else if (c.holdLeft >= 0.0f) { c.holdLeft -= fn; if (c.holdLeft <= 0.0f) letGo = true; }
        else if (! running || t.mute) letGo = true;
        if (letGo) { c.held = 0; c.stage = 3; }
    }
    else if (c.stage != 3) c.stage = 3;
    // the note: the ribbon (followed closely; on SNAP, the quantiser's note nearest it, gliding from one to the next
    // in GLID), or the trig's (gliding there in GLID)
    const u32 qmask = qm & 0xFFFu;
    const bool snap = c.rib && c.ribSnap && qmask;
    float tgt = c.rib ? (P[P_TUNE] - 0.5f) * 48.0f + c.ribT : (qm ? tuneSt : tuneRaw);
    if (snap)
    {
        // the nearest note of the mask (from C4); the one held stays until the finger is clearly nearer another
        const int n0 = (int) floorf_ (tgt + 0.5f);
        int best = n0; float bd = 1e9f;
        for (int d = -12; d <= 12; ++d)
        {
            const int n = n0 + d;
            if (! ((qmask >> (((n % 12) + 12) % 12)) & 1u)) continue;
            const float dd = absf ((float) n - tgt);
            if (dd < bd) { bd = dd; best = n; }
        }
        float q = (float) best;
        const bool first = c.ribQ <= -900.0f;
        if (! first && q != c.ribQ && ((qmask >> ((((int) c.ribQ % 12) + 12) % 12)) & 1u) && absf (tgt - c.ribQ) < bd + 0.2f) q = c.ribQ;
        c.ribQ = q;
        tgt = q;
        // (a phrase the finger starts, from silence: on its note at once)
        if (first && c.env < 1e-3f) { c.pitch = c.from = c.to = q; c.gx = 1.0f; }
    }
    else c.ribQ = -999.0f;
    if (c.rib && ! snap)
    {
        if (c.env < 1e-3f && c.stage == 1) c.pitch = tgt;
        c.pitch += (tgt - c.pitch) * (1.0f - expf_ (-fn / (0.022f * sr)));
        c.from = c.to = c.pitch; c.gx = 1.0f;
    }
    else
    {
        if (absf (tgt - c.to) > 1e-4f)
        {
            const float T = P[P_B] < 0.01f ? 0.012f : 0.03f * powf_ (50.0f, P[P_B]);
            c.from = c.pitch; c.to = tgt; c.gx = 0.0f; c.gInc = 1.0f / (T * sr);
        }
        if (c.gx < 1.0f) { c.gx = minf (1.0f, c.gx + fn * c.gInc); c.pitch = c.from + (c.to - c.from) * smooth01 (c.gx); }
        else c.pitch = c.to;
    }
    // the envelope: a smooth swell (SWEL), a fade (DEC); the onset's scoop, the vibrato's onset
    const float env0 = c.env;
    if (c.stage == 1 || c.stage == 2)
    {
        const float tS = 0.02f * exp2f_ (P[P_CSWL] * 7.0f);
        c.env += (1.0f - c.env) * (1.0f - expf_ (-fn * 2.2f / (tS * sr)));
        if (c.env > 0.999f) { c.env = 1.0f; c.stage = 2; }
    }
    else if (c.stage == 3)
    {
        const float tR = 0.05f * exp2f_ (P[P_DEC] * 7.0f);
        c.env *= expf_ (-fn * 6.9f / (tR * sr));
        if (c.env < 1e-5f) { c.env = 0.0f; c.stage = 0; }
    }
    const float env1 = c.env;
    c.scoop *= expf_ (-fn / (0.07f * sr));
    c.age += fn / sr;
    c.breath *= expf_ (-fn / (0.06f * sr));
    c.vowel += (P[P_A] - c.vowel) * (1.0f - expf_ (-fn / (0.03f * sr)));
    // the voice's settings this chunk
    const float vw = clampf (c.vowel, 0.0f, 1.0f) * (float) (CHOIR_VOWELS - 1);
    const int vi = mini ((int) vw, CHOIR_VOWELS - 2); const float vf = vw - (float) vi;
    const float form = exp2f_ ((P[P_CFRM] - 0.5f) * 0.6f);
    const float brth = P[P_C];
    const float eff = clampf (P[P_CEFF] + 0.3f * (v.vel - 0.75f) + 0.25f * (c.expr - 0.6f), 0.0f, 1.0f);
    const float oqB = clampf (0.72f - 0.32f * eff + 0.12f * brth, 0.3f, 0.9f);
    const float tiltA = 1.0f - expf_ (-TWO_PI * minf (900.0f * exp2f_ (eff * 4.0f), 0.45f * sr) / sr);
    // (the source's presence: above 1.2 kHz lifted, more with the effort, as a voice's upper formants come out)
    const float preA = 1.0f - expf_ (-TWO_PI * 1500.0f / sr), preG = 1.8f + 2.6f * eff;
    const float vibC = P[P_D] * P[P_D] * 80.0f * smooth01 ((c.age - 0.2f) * 2.0f);
    const float vibHz = 4.2f + 3.0f * P[P_CVRT];
    // (v61: a cleaner voice: each cycle's length and level wander less from the last — a trained singer's, not a
    // tired one's —, the breath a third of what it was, only while the glottis opens, in a narrower band)
    const float drf = P[P_CDRF], jitA = 0.0008f + 0.005f * drf, shmA = 0.006f + 0.03f * drf;
    const float fall = c.stage == 3 ? -P[P_CSCP] * 0.4f * (1.0f - c.env) : 0.0f;
    const float ens = clampf (P[P_E], 0.0f, 1.0f), cnt = 1.0f + 3.0f * ens, spread = 4.0f + 14.0f * ens;
    const int part = idxOf (P[P_CPRT], CHOIR_PARTN), np = PART_N[part];
    const float hum = VHUM[vi] + (VHUM[vi + 1] - VHUM[vi]) * vf, vg = VG[vi] + (VG[vi + 1] - VG[vi]) * vf;
    const float humA = 1.0f - expf_ (-TWO_PI * 520.0f / sr);
    const float aspG = brth * brth * 0.3f * (0.4f + 0.6f * (1.0f - eff)) + brth * 0.12f * c.breath;
    const float nzA = 1.0f - expf_ (-TWO_PI * 1800.0f / sr), nzB = 1.0f - expf_ (-TWO_PI * 4500.0f / sr);
    const float drK = 1.0f - expf_ (-fn / (1.4f * sr));
    const u32 mask = qmask;
    // the parts' intervals (from the sung note, in the scale when quantised; a change glides in 25 ms)
    const float pk25 = c.poffOk ? 1.0f - expf_ (-fn / (0.025f * sr)) : 1.0f;
    c.poffOk = 1;
    float pOff[CHOIR_PARTS];
    for (int p = 0; p < CHOIR_PARTS; ++p)
    {
        const float o = p > 0 && p < np ? partSemis (60.0f + c.pitch, PART_DEG[part][p], PART_SEMI[part][p], mask) : 0.0f;
        c.poff[p] += (o - c.poff[p]) * pk25;
        pOff[p] = c.poff[p];
    }
    // the ensemble's weights (singers fade in one after another), the parts', their level together
    float gS[CHOIR_SING], sumG = 0.0f;
    for (int e = 0; e < CHOIR_SING; ++e) { gS[e] = clampf (cnt - (float) e, 0.0f, 1.0f); sumG += gS[e] * gS[e]; }
    const float pG[CHOIR_PARTS] = { 1.0f, 0.78f, 0.7f };
    float sumP = 0.0f; for (int p = 0; p < np; ++p) sumP += pG[p] * pG[p];
    const float norm = 1.0f / sqrtf_ (sumG * sumP);
    for (int i = 0; i < n; ++i) out[i] = 0.0f;
    float pk = 0.0f;
    for (int p = 0; p < np; ++p)
        for (int e = 0; e < CHOIR_SING; ++e)
        {
            ChoirSinger& s = c.s[p * CHOIR_SING + e];
            const float gT = gS[e] * pG[p];
            if (gT <= 0.0f && s.g <= 1e-4f) { s.g = 0.0f; continue; }
            // its pitch: the note, the scoop, the fall, the part, its detune, drift and vibrato
            s.driftLeft -= n;
            if (s.driftLeft <= 0) { s.driftT = v.rng.bi() * drf * 10.0f; s.driftLeft = (int) ((1.5f + 1.5f * v.rng.uni()) * sr); }
            s.drift += (s.driftT - s.drift) * drK;
            s.vibPh += fn * vibHz * s.vibRate / sr; s.vibPh -= floorf_ (s.vibPh);
            const float cents = s.det * spread + s.drift + vibC * sin2pi (s.vibPh);
            const float st = c.pitch + c.scoop + fall + pOff[p] + cents * 0.01f;
            const float f0 = BASE_C4 * exp2f_ (st * (1.0f / 12.0f));
            const float dph = minf (f0 / sr, 0.25f);
            // its tract: the vowel, its size and FORM; the first formant raised to the pitch when the pitch passes it
            const float sz = s.size * form;
            float F[CHOIR_FORM], B[CHOIR_FORM];
            for (int f = 0; f < CHOIR_FORM; ++f)
            {
                F[f] = (VF[vi][f] + (VF[vi + 1][f] - VF[vi][f]) * vf) * sz;
                B[f] = VB[vi][f] + (VB[vi + 1][f] - VB[vi][f]) * vf;
            }
            const float f1T = f0 * 1.12f;
            if (f1T > F[0] * 0.8f) { const float u = smooth01 ((f1T - F[0] * 0.8f) / (F[0] * 0.4f)); F[0] += (maxf (f1T, F[0]) - F[0]) * u; }
            if (F[1] < F[0] * 1.3f + 120.0f) F[1] = F[0] * 1.3f + 120.0f;
            for (int f = 0; f < CHOIR_FORM; ++f)
            {
                const float ff = minf (F[f], 0.42f * sr), bw = B[f] * (1.0f + 0.25f * (sz - 1.0f));
                const float r = expf_ (-PI * bw / sr);
                s.cc[f] = -r * r; s.cb[f] = 2.0f * r * cos2pi (ff / sr); s.ca[f] = 1.0f - s.cb[f] - s.cc[f];
                if (p == 0 && e == 0) c.fShow[f] = ff;
            }
            const float g0 = s.g, dg = (gT - g0) / fn;
            for (int i = 0; i < n; ++i)
            {
                if (s.wait > 0.0f) { s.wait -= 1.0f; continue; }
                // the glottis: a cycle open for OQ, closing abruptly (band-limited where it closes)
                const float dt = dph * s.per;
                s.ph += dt;
                if (s.ph >= 1.0f)
                {
                    s.ph -= 1.0f;
                    s.per = 1.0f + jitA * v.rng.bi();
                    s.amp = 1.0f + shmA * v.rng.bi();
                    s.oq = clampf (oqB + 0.01f * v.rng.bi(), 0.25f, 0.92f);
                }
                const float oq = s.oq;
                float gl = 0.0f;
                if (s.ph < oq) { const float u = s.ph / oq; gl = 2.0f * u - 3.0f * u * u; }
                float t2 = s.ph - oq; if (t2 < 0.0f) t2 += 1.0f;
                gl += 0.5f * polyblep (t2, dt);
                gl *= s.amp;
                s.tilt2 += preA * (gl - s.tilt2);
                gl += preG * (gl - s.tilt2);
                s.tilt += tiltA * (gl - s.tilt);
                float x = s.tilt;
                // the breath: noise in a band, stronger while the glottis is open
                if (aspG > 0.0f)
                {
                    const float nz = v.rng.bi();
                    s.nz1 += nzB * (nz - s.nz1); s.nz2 += nzA * (s.nz1 - s.nz2);
                    x += (s.nz1 - s.nz2) * aspG * (s.ph < oq ? 1.0f : 0.1f);
                }
                // the tract: five resonators in cascade
                for (int f = 0; f < CHOIR_FORM; ++f)
                {
                    const float y = s.ca[f] * x + s.cb[f] * s.r1[f] + s.cc[f] * s.r2[f];
                    s.r2[f] = s.r1[f]; s.r1[f] = y; x = y;
                }
                if (hum > 0.001f) { s.hum += humA * (x - s.hum); x += (s.hum - x) * hum; }
                const float gg = g0 + dg * (float) (i + 1);
                out[i] += x * gg;
            }
            s.g = gT;
            for (int f = 0; f < CHOIR_FORM; ++f) if (isBad (s.r1[f]) || isBad (s.r2[f])) { for (int q = 0; q < CHOIR_FORM; ++q) s.r1[q] = s.r2[q] = 0.0f; break; }
            if (isBad (s.tilt) || isBad (s.hum) || isBad (s.nz1)) { s.tilt = s.hum = s.nz1 = s.nz2 = 0.0f; }
        }
    // the level: the envelope across the chunk, the vowel's, the ensemble's, the ribbon's height, DC out
    c.exS += ((c.rib ? 0.35f + 0.65f * c.expr : 1.0f) - c.exS) * (1.0f - expf_ (-fn / (0.02f * sr)));
    const float gl = 0.13f * vg * norm * c.exS;
    const float de = (env1 - env0) / fn;
    for (int i = 0; i < n; ++i)
    {
        float y = out[i] * gl * (env0 + de * (float) (i + 1));
        c.hp += (y - c.hp) * (1.0f - 0.9985f);
        y -= c.hp;
        out[i] = y;
        pk = maxf (pk, absf (y));
    }
    c.lvl = maxf (pk, c.lvl * 0.9f);
    v.level = c.stage == 0 ? 0.0f : maxf (c.env, 1e-3f);
    v.ns += (u32) n;
}

// SPACE: the machine's own reverb, on the track's way out (its TONE before it): the VERB machine's network, set from
// the knobs for big, smooth spaces; the middle back into the track, the sides with WIDE's. It sleeps when it has
// nothing left to ring (its memory kept: no clearing when it wakes)
void Engine::choirSpace (Track& t, int ti, const float* P, float* out, float* side, int n)
{
    FxState& f = fx[ti];
    const bool isChoir = machineOf (P[P_MACH]) == M_CHOIR;
    const float target = isChoir ? P[P_CSPC] : 0.0f;
    if (! t.cvOn)
    {
        float pk = 0.0f;
        for (int i = 0; i < n; ++i) pk = maxf (pk, absf (out[i]));
        if (target <= 0.0005f || pk < 1e-7f) return;
        if (f.kind != CHOIR_KIND || f.sr != sr) { fxSetup (f, M_FXVERB); f.kind = CHOIR_KIND; }
        t.cvOn = 1; t.cvQuiet = 0.0f;
        if (t.cvMix <= 0.0f) t.cvMix = target;
    }
    if (f.kind != CHOIR_KIND) { fxSetup (f, M_FXVERB); f.kind = CHOIR_KIND; }
    float Pv[NUM_PARAMS];
    for (int k = 0; k < NUM_PARAMS; ++k) Pv[k] = 0.0f;
    const float dmp = P[P_CDMP], mod = P[P_CMOD];
    Pv[P_TUNE] = 0.5f - (dmp - 0.5f) * 0.3f;               // (the tail's tilt: darker as it is damped)
    Pv[P_B] = 0.55f + 0.43f * P[P_CSIZ];                    // SIZE: a hall .. a cathedral
    Pv[P_C] = 0.35f + 0.44f * P[P_CDCY];                    // DECAY (just under endless at the top)
    Pv[P_D] = 0.08f + 0.6f * dmp;                           // ABSRB
    Pv[P_XA] = 0.22f + 0.3f * mod;                          // the lines' movement: speed, depth
    Pv[P_XB] = 0.08f + 0.42f * mod;
    Pv[P_XC] = P[P_CPRE] * 0.65f;                           // PRE
    Pv[P_XD] = 0.0f;                                        // (sines, each line an eighth of a turn from the next)
    Pv[P_XE] = 0.5f / 3.0f;                                 // (ACT: IN, no gesture)
    Pv[P_XF] = 0.35f;                                       // BASS
    Pv[P_XG] = ((float) SHIM_IDX[idxOf (P[P_CSHM], CHOIR_SHIMS)] + 0.5f) / 13.0f;
    float g1[CHUNK];
    for (int i = 0; i < n; ++i) g1[i] = 1.0f;
    fxVerb (f, Pv, out, out, g1, wa, wb, n);
    const float k = 1.0f - expf_ (-1.0f / (0.03f * sr));
    float pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        t.cvMix += (target - t.cvMix) * k;
        const float m = t.cvMix, wet = 0.55f * m;
        const float mid = 0.5f * (wa[i] + wb[i]), sd = 0.5f * (wa[i] - wb[i]);
        out[i] = out[i] * (1.0f - 0.3f * m * m) + wet * mid;
        side[i] = wet * sd;
        pk = maxf (pk, absf (wa[i]) + absf (wb[i]));
    }
    // asleep when its tail has died away (or its mix has, off CHOIR)
    if (pk < 3e-6f || (target <= 0.0f && t.cvMix < 1e-4f)) t.cvQuiet += (float) n; else t.cvQuiet = 0.0f;
    if (t.cvQuiet > 1.5f * sr || (! isChoir && t.cvMix < 1e-4f)) { t.cvOn = 0; t.cvQuiet = 0.0f; if (! isChoir) t.cvMix = 0.0f; }
}

// for the screen: 3 (CHOIR), sounding, its pitch (MIDI), where it goes, its level, the vowel (0..5), the first
// singer's formants (Hz), the ribbon (playing, its height), the singers, the SPACE's mix, the output's level
void Engine::choirInfo (int ti, float* out) const
{
    const Track& t = tr[ti];
    const Voice& v = t.voice[t.cur];
    out[0] = 3.0f;
    const bool on = v.active && v.machine == M_CHOIR && v.cs.stage != 0;
    const ChoirState& c = v.cs;
    out[1] = on ? 1.0f : 0.0f;
    out[2] = on ? 60.0f + c.pitch : 0.0f;
    out[3] = on ? 60.0f + c.to : 0.0f;
    out[4] = on ? c.env : 0.0f;
    out[5] = clampf (on ? c.vowel : t.p[P_A], 0.0f, 1.0f) * (float) (CHOIR_VOWELS - 1);
    for (int f = 0; f < CHOIR_FORM; ++f) out[6 + f] = on ? c.fShow[f] : 0.0f;
    out[11] = (float) t.ribOn;
    out[12] = t.ribExpr;
    const int part = idxOf (t.p[P_CPRT], CHOIR_PARTN);
    out[13] = (float) PART_N[part] * (1.0f + 3.0f * clampf (t.p[P_E], 0.0f, 1.0f));
    out[14] = t.cvOn ? t.cvMix : 0.0f;
    out[15] = on ? c.lvl : 0.0f;
    out[16] = 60.0f + t.ribSemis + (t.p[P_TUNE] - 0.5f) * 48.0f;
}

} // namespace sk
