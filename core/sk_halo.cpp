// IMPAKT — HALO, chords for pads and ambiences (see sk_halo.h)
#include "sk_engine.h"

namespace sk {

namespace {
// CHRD, in steps of a seven-note scale (a scale of another size scales them): 5TH SUS2 SUS4 TRI ADD9 7TH 9TH 11TH
// 6/9 QRTL (fourths) 5THS (fifths) CLST (seconds)
const int SHAPE_N[HALO_SHAPES] = { 3, 3, 3, 3, 4, 4, 5, 6, 5, 4, 4, 4 };
const int SHAPE[HALO_SHAPES][HALO_MAXN] = {
    { 0, 4, 7 }, { 0, 1, 4 }, { 0, 3, 4 }, { 0, 2, 4 }, { 0, 2, 4, 8 }, { 0, 2, 4, 6 }, { 0, 2, 4, 6, 8 },
    { 0, 2, 4, 6, 8, 10 }, { 0, 2, 4, 5, 8 }, { 0, 3, 6, 9 }, { 0, 4, 8, 12 }, { 0, 1, 2, 3 } };
// PROG: a chord per trig, as degrees of the scale (and chromatic shifts of the whole chord for the outside ones).
// OFF AEON (i VI iv VII) DUSK (i VII VI VII) TIDE (i VI) VAMP (i IV) DAWN (i III VI v) FALL (i VII VI v) RISE (i ii III iv)
// 4THS (round the fourths) AMBR (i bII i VII) SLIP (a semitone down and back up) MEDI (major-third mediants); then the
// classical ones (v59; in the scale, so in a minor mode they come out minor): CANON (I V vi iii IV I IV V, the canon's
// ground) AXIS (I V vi IV) 50S (I vi IV V) CADN (I IV V I) TURN (I vi ii V) II-V (ii V I I) ROYL (IV V iii vi) EPIC (vi IV
// I V) FOLIA (i V i VII III VII i V) ROMA (the romanesca: III VII i V) HYMN (I IV vi V); PEDL (i VII VI iv over the tonic
// held in the bass); the tintinnabuli (kind 2: the tonic's chord, a melody — degrees over the tonic an octave up — and the
// chord's note under it): TIN↓ (from the octave down the scale) TIN↑ (up it) MIRR (round the third, from above and below,
// further each time); WALK (random)
struct Prog { int len, kind; signed char deg[24]; signed char semi[24]; };
const Prog PROGS[HALO_PROGS] = {
    { 1, 0, { 0 }, { 0 } },
    { 4, 0, { 0, 5, 3, 6 }, { 0 } },
    { 4, 0, { 0, 6, 5, 6 }, { 0 } },
    { 2, 0, { 0, 5 }, { 0 } },
    { 2, 0, { 0, 3 }, { 0 } },
    { 4, 0, { 0, 2, 5, 4 }, { 0 } },
    { 4, 0, { 0, 6, 5, 4 }, { 0 } },
    { 4, 0, { 0, 1, 2, 3 }, { 0 } },
    { 7, 0, { 0, 3, 6, 2, 5, 1, 4 }, { 0 } },
    { 4, 0, { 0, 1, 0, 6 }, { 0 } },
    { 4, 0, { 0, 0, 0, 0 }, { 0, -1, 0, 1 } },
    { 3, 0, { 0, 0, 0 }, { 0, 4, 8 } },
    { 8, 0, { 0, 4, 5, 2, 3, 0, 3, 4 }, { 0 } },
    { 4, 0, { 0, 4, 5, 3 }, { 0 } },
    { 4, 0, { 0, 5, 3, 4 }, { 0 } },
    { 4, 0, { 0, 3, 4, 0 }, { 0 } },
    { 4, 0, { 0, 5, 1, 4 }, { 0 } },
    { 4, 0, { 1, 4, 0, 0 }, { 0 } },
    { 4, 0, { 3, 4, 2, 5 }, { 0 } },
    { 4, 0, { 5, 3, 0, 4 }, { 0 } },
    { 8, 0, { 0, 4, 0, 6, 2, 6, 0, 4 }, { 0 } },
    { 4, 0, { 2, 6, 0, 4 }, { 0 } },
    { 4, 0, { 0, 3, 5, 4 }, { 0 } },
    { 4, 1, { 0, 6, 5, 3 }, { 0 } },
    { 8, 2, { 7, 6, 5, 4, 3, 2, 1, 0 }, { 0 } },
    { 8, 2, { 0, 1, 2, 3, 4, 5, 6, 7 }, { 0 } },
    { 18, 2, { 3, 2, 1, 2, 4, 3, 2, 0, 1, 2, 5, 4, 3, 2, -1, 0, 1, 2 }, { 0 } },
    { 0, 0, { 0 }, { 0 } } };
enum { PROG_WALK = HALO_PROGS - 1 };
// MODE: KEY (the quantiser's scale; aeolian without one), then HALO's own on TUNE's key (bit k = k semitones up):
// AEOL DOR PHRY LYD MIXO ION HARM (harmonic minor) MPNT (minor pentatonic) WHOL HIRA (hirajoshi) OCTA (octatonic)
const u32 MODE_MASK[HALO_MODES] = { 1453u, 1453u, 1709u, 1451u, 2773u, 1717u, 2741u, 2477u, 1193u, 1365u, 397u, 2925u };
// SPRD: where each place in the chord goes (an octave at a time, faded over 12 % of the knob): the bass falls first,
// then the others open out upwards, the 4th and 6th voices by two octaves
const float SPR_TH[HALO_MAXN][2] = { { 0.22f, 9.0f }, { 0.42f, 9.0f }, { 0.62f, 9.0f }, { 0.52f, 0.86f }, { 0.72f, 9.0f }, { 0.34f, 0.80f } };
const float SPR_D[HALO_MAXN] = { -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
const float C3_HZ = 130.8128f;
// how alike two neighbouring shapes are: their crossfade is kept at a steady level (SINE-SOFT .. GLASS-METAL)
const float SHAPE_RHO[5] = { 0.95f, 0.75f, 0.6f, 0.3f, 0.15f };

inline float smooth01 (float x) { const float o = clampf (x, 0.0f, 1.0f); return o * o * (3.0f - 2.0f * o); }
inline int   floorDiv (int a, int b) { const int q = a / b; return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q; }
inline float wrap1 (float p) { return p - floorf_ (p); }
// AGE: a new note takes a card none of the sounding notes holds (one of them at random)
inline int takeCard (HaloState& h)
{
    u32 busy = 0;
    for (int s = 0; s < HALO_SLOTS; ++s) if (h.s[s].stage == 1 || h.s[s].stage == 2) busy |= 1u << (h.s[s].card & 7);
    int fr[HALO_CARDS], nf = 0;
    for (int c = 0; c < HALO_CARDS; ++c) if (! ((busy >> c) & 1u)) fr[nf++] = c;
    const float u = h.arng.uni();
    if (nf == 0) return mini ((int) (u * (float) HALO_CARDS), HALO_CARDS - 1);
    return fr[mini ((int) (u * (float) nf), nf - 1)];
}
// AIR: the breath's band sits on the note's harmonic nearest AIR_F (Hz); its top over AIR_TOP
constexpr float AIR_F = 3000.0f, AIR_TOP = 5000.0f, AIR_SOFT = 11000.0f, AIR_TG = 1.3f;

// the octave a place in the chord sits at: SPRD's moves and INV's turn (tones below the turn go up an octave)
inline float placeOct (int k, int m, float spr, float inv)
{
    float o = 0.0f;
    if (k < HALO_MAXN)
        for (int j = 0; j < 2; ++j) if (SPR_TH[k][j] < 2.0f) o += SPR_D[k] * smooth01 ((spr - SPR_TH[k][j]) * (1.0f / 0.12f) + 0.5f);
    o += clampf (inv * (float) m - (float) k, 0.0f, 1.0f);
    return o;
}
// the swell's curve and back: gain g = smoothstep (x)
inline float swellX (float g)
{
    float lo = 0.0f, hi = 1.0f;
    for (int it = 0; it < 14; ++it) { const float mi = 0.5f * (lo + hi); if (smooth01 (mi) < g) lo = mi; else hi = mi; }
    return 0.5f * (lo + hi);
}
// one cycle of WAVE's shape i (0 SINE, 1 SOFT, 2 SAW, 3 PULSE, 4 GLASS, 5 METAL), at about the same loudness
inline float haloShape (int i, float ph, float dt, float mph, float pw, float fm)
{
    switch (i)
    {
        case 0:  return 0.85f * sin2pi (ph);
        case 1:  return 0.86f * (0.8f * sin2pi (ph) + 0.28f * sin2pi (wrap1 (2.0f * ph)));
        case 2:  return 0.92f * (1.0f - 2.0f * ph + polyblep (ph, dt));                          // (falling: in phase with the sine)
        case 3:  { float t2 = ph - pw; if (t2 < 0.0f) t2 += 1.0f; return 0.55f * ((ph < pw ? 1.0f : -1.0f) + polyblep (ph, dt) - polyblep (t2, dt)); }
        case 4:  return 0.85f * sin2pi (wrap1 (ph + fm * 0.22f * sin2pi (wrap1 (2.0f * ph))));   // GLASS: FM 1:2
        default: return 0.85f * sin2pi (wrap1 (ph + fm * 0.18f * sin2pi (mph)));                // METAL: FM 1:3.53
    }
}
// SUB (v71): a sine; v72, PRES: its 2nd, 3rd and 4th harmonics over it (h2..h4, in phase: the fundamental stays as
// it is, the harmonics make it heard where it is not, on small speakers or played softly); never brought under SUB_LO (Hz)
inline float subWave (float ph, float h2, float h3, float h4)
{
    const float s = sin2pi (ph);
    if (h2 <= 0.0f) return s;
    const float c = cos2pi (ph), s2 = 2.0f * s * c, c2 = 1.0f - 2.0f * s * s;
    return s + h2 * s2 + h3 * s * (3.0f - 4.0f * s * s) + h4 * 2.0f * s2 * c2;
}
constexpr float SUB_LO = 45.0f;
}

// the chord: the scale (the quantiser's or the mode's), the root nearest TUNE + NOTE, the shape stacked on it
// from the progression's degree; HARM's corrections, the pedal, the tintinnabuli's voices. Returns the number of notes
// (semitones from C3, ascending) and its key; with cmp, -1 when the key is that one (nothing to rebuild)
int Engine::haloChord (Voice& v, const float* P, float tuneRaw, u32 qm, float* notes, int* key, const int* cmp)
{
    HaloState& h = v.hs;
    const int mode = idxOf (P[P_HMOD], HALO_MODES);
    u32 mask;
    if (mode == 0 && qm) mask = qm & 0xFFFu;
    else
    {
        const u32 rel = MODE_MASK[mode];
        const int tonic = ((int) floorf_ ((P[P_TUNE] - 0.5f) * 48.0f + 0.5f) % 12 + 12) % 12;
        mask = ((rel << tonic) | (rel >> (12 - tonic))) & 0xFFFu;
    }
    int pcs[12], L = 0;
    for (int c = 0; c < 12; ++c) if ((mask >> c) & 1u) pcs[L++] = c;
    if (L < 2) { L = 12; for (int c = 0; c < 12; ++c) pcs[c] = c; mask = 0xFFFu; }
    // playing along with the chord track the quantiser follows (MODE KEY): its chord's notes are the scale, its root
    // the root (in this track's register), and this track's own progression is set aside
    const int along = mode == 0 && qm ? harmRoot (v.ti) : -1;
    const int pDeg = along >= 0 ? 0 : h.progDeg, pSemi = along >= 0 ? 0 : h.progSemi, pKind = along >= 0 ? 0 : h.progKind, pTint = along >= 0 ? 0 : h.tint;
    // the root: the scale note nearest TUNE + NOTE (the lower one on a tie); playing along, the followed root's nearest
    const int n0 = (int) floorf_ (tuneRaw + 0.5f);
    int best = n0; float bd = 1e9f;
    for (int d = -6; d <= 6; ++d)
    {
        const int nn = n0 + d, pc = ((nn % 12) + 12) % 12;
        if (along >= 0 ? pc != along : ! ((mask >> pc) & 1u)) continue;
        const float dist = absf ((float) nn - tuneRaw);
        if (dist < bd - 1e-4f) { bd = dist; best = nn; }
    }
    const int oct = floorDiv (best, 12), pcb = best - 12 * oct;
    int deg = 0; while (deg < L && pcs[deg] != pcb) ++deg;
    if (deg >= L) deg = 0;
    const int shape = idxOf (P[P_A], HALO_SHAPES), m = SHAPE_N[shape], harm = idxOf (P[P_HARM], NUM_HARM);
    key[0] = best; key[1] = shape; key[2] = (int) mask; key[3] = pDeg; key[4] = pSemi; key[5] = harm; key[6] = pKind * 256 + pTint + 64;
    if (cmp)
    {
        bool same = true;
        for (int k = 0; k < HALO_KEY; ++k) if (key[k] != cmp[k]) same = false;
        if (same) return -1;
    }
    // a scale step as a note, a step of the seven-note shapes on a scale of another size
    auto stepNote = [&] (int s) { const int o = floorDiv (s, L); return 12 * (oct + o) + pcs[s - o * L] + pSemi; };
    auto scaleSt  = [&] (int st) { return L == 7 ? st : (int) floorf_ ((float) st * (float) L / 7.0f + 0.5f); };
    const int s0 = deg + scaleSt (pDeg);
    h.root = stepNote (s0);
    int out[HALO_MAXN + 2], stp[HALO_MAXN + 2], cnt = 0;
    for (int k = 0; k < m; ++k)
    {
        const int s = s0 + scaleSt (SHAPE[shape][k]);
        const int note = stepNote (s);
        bool dup = false;
        for (int j = 0; j < cnt; ++j) if (out[j] == note) dup = true;
        if (! dup) { out[cnt] = note; stp[cnt] = s; ++cnt; }
    }
    const bool tint = pKind == 2, pedal = pKind == 1;
    auto drop = [&] (int k) { for (int j = k; j < cnt - 1; ++j) { out[j] = out[j + 1]; stp[j] = stp[j + 1]; } --cnt; };
    auto pcDup = [&] (int k, int note) { for (int j = 0; j < cnt; ++j) if (j != k && (((out[j] - note) % 12) + 12) % 12 == 0) return true; return false; };
    if (harm >= HARM_PURE && ! tint)
    {
        // a note that clashes moves a step up (or down when that note is in the chord already), or goes
        auto move = [&] (int k)
        {
            for (int d = 1; d >= -1; d -= 2)
            {
                const int s = stp[k] + d, nn = stepNote (s);
                if (! pcDup (k, nn)) { out[k] = nn; stp[k] = s; return; }
            }
            drop (k);
        };
        // a diminished or an augmented fifth (in a seven-note scale)
        if (L == 7)
            for (int k = 1; k < cnt; ++k)
                if (stp[k] - stp[0] == 4)
                {
                    const int iv = (((out[k] - out[0]) % 12) + 12) % 12;
                    if (iv == 6 || iv == 8) move (k);
                    break;
                }
        // the low register opened out: under E3 a third or a second over the root goes up an octave
        for (int k = 1; k < cnt; ++k)
            if (out[k] < 4 && out[k] - out[0] < 7)
            {
                const int up = out[k] + 12;
                bool dup = false;
                for (int j = 0; j < cnt; ++j) if (out[j] == up) dup = true;
                if (dup) { drop (k); --k; } else out[k] = up;
            }
        // a note a semitone, a minor ninth (or two octaves and a semitone) from another: the extension of the two (not
        // the root, third or fifth) takes another octave where it clashes with nothing (a ninth under its root becomes
        // a seventh over it), else a step up or down, else it goes
        auto clash = [&] (int k, int note)
        {
            for (int j = 0; j < cnt; ++j)
            {
                if (j == k) continue;
                const int d = note > out[j] ? note - out[j] : out[j] - note;
                if (d == 1 || d == 13 || d == 25 || d == 0) return true;
            }
            return false;
        };
        auto core = [&] (int k) { const int d = stp[k] - stp[0]; return L == 7 ? (d == 0 || d == 2 || d == 4) : k < 3; };
        for (int pass = 0; pass < 8; ++pass)
        {
            int ba = -1, bb = -1;
            for (int a2 = 0; a2 < cnt && ba < 0; ++a2)
                for (int b2 = 0; b2 < cnt; ++b2) { const int d = out[b2] - out[a2]; if (d == 1 || d == 13 || d == 25) { ba = a2; bb = b2; break; } }
            if (ba < 0) break;
            const int x = (ba != 0 && ! core (ba) && core (bb)) ? ba : bb;
            if (x == 0) break;
            bool done = false;
            static const int OCT[4] = { 12, -12, 24, -24 };
            for (int o = 0; o < 4 && ! done; ++o)
            {
                const int c2 = out[x] + OCT[o];
                if (c2 <= out[0] || c2 > out[0] + 36 || (c2 < 4 && c2 - out[0] < 7) || clash (x, c2)) continue;
                out[x] = c2; done = true;
            }
            if (! done)
                for (int d = 1; d >= -1 && ! done; d -= 2)
                {
                    const int s2 = stp[x] + d, nn = stepNote (s2);
                    if (nn > out[0] && ! (nn < 4 && nn - out[0] < 7) && ! pcDup (x, nn) && ! clash (x, nn)) { out[x] = nn; stp[x] = s2; done = true; }
                }
            if (! done) drop (x);
        }
    }
    if (tint)
    {
        // the M voice: the melody's degree over the tonic an octave up; the T voice: the chord's highest note under it
        const int M = stepNote (deg + L + scaleSt (pTint));
        int T = -9999;
        for (int j = 0; j < cnt; ++j) { int c = out[j]; while (c + 12 < M) c += 12; while (c >= M) c -= 12; if (c > T) T = c; }
        while (cnt > HALO_MAXN - 2) --cnt;
        const int add[2] = { T, M };
        for (int j = 0; j < 2; ++j)
        {
            bool dup = false;
            for (int i = 0; i < cnt; ++i) if (out[i] == add[j]) dup = true;
            if (! dup) { out[cnt] = add[j]; stp[cnt] = 0; ++cnt; }
        }
    }
    if (pedal)
    {
        // the tonic an octave down, under the chord (the chord's top note makes room)
        const int pd = best - 12;
        bool dup = false;
        for (int j = 0; j < cnt; ++j) if (out[j] == pd) dup = true;
        if (! dup)
        {
            if (cnt >= HALO_MAXN) { int hi = 0; for (int j = 1; j < cnt; ++j) if (out[j] > out[hi]) hi = j; drop (hi); }
            out[cnt] = pd; stp[cnt] = 0; ++cnt;
        }
    }
    for (int a2 = 1; a2 < cnt; ++a2) for (int b2 = a2; b2 > 0 && out[b2] < out[b2 - 1]; --b2) { const int t = out[b2]; out[b2] = out[b2 - 1]; out[b2 - 1] = t; }
    if (harm == HARM_LEAD && ! tint && cnt > 2)
    {
        // the upper voices: each to the octave nearest the middle of the last chord's (drawn back towards the fifth
        // over the tonic: they never wander off), always over the bass
        const float home = (float) best + 7.0f;
        float c = home;
        if (h.n > 2) { float sum = 0.0f; for (int j = 1; j < h.n; ++j) sum += h.chord[j]; c = clampf (sum / (float) (h.n - 1), home - 5.0f, home + 5.0f); }
        const int bass = out[0];
        auto place = [&] (int u)
        {
            int bu = u; float bdd = 1e9f;
            for (int o = -4; o <= 4; ++o) { const int cu = u + 12 * o; if (cu <= bass + 2) continue; const float d = absf ((float) cu - c); if (d < bdd) { bdd = d; bu = cu; } }
            return bu;
        };
        for (int k = 1; k < cnt; ++k) out[k] = place (out[k]);
        // a note a semitone (in any octave) over a lower one: the two swapped (a seventh under its root is a cluster)
        for (int pass = 0; pass < 8; ++pass)
        {
            bool moved = false;
            for (int a2 = 1; a2 < cnt && ! moved; ++a2)
                for (int b2 = 1; b2 < cnt && ! moved; ++b2)
                {
                    const int d = out[b2] - out[a2];
                    if (d <= 0 || d % 12 != 1) continue;
                    const int dn = out[b2] - 12 * ((d + 11) / 12), up = out[a2] + 12 * ((d + 11) / 12);
                    if (dn > bass + 2 && absf ((float) dn - c) <= absf ((float) up - c)) out[b2] = dn; else out[a2] = up;
                    moved = true;
                }
            if (! moved) break;
        }
        for (int a2 = 1; a2 < cnt; ++a2) for (int b2 = a2; b2 > 0 && out[b2] < out[b2 - 1]; --b2) { const int t = out[b2]; out[b2] = out[b2 - 1]; out[b2 - 1] = t; }
        for (int k = 1; k < cnt; ++k) if (out[k] == out[k - 1]) { drop (k); --k; }
    }
    for (int k = 0; k < cnt; ++k) notes[k] = (float) out[k];
    return cnt;
}

// a new chord on the slots: the notes it shares with the one sounding stay, the others glide to the nearest new notes
// (GLID) or let go, and what is still missing swells in
void Engine::haloAssign (Voice& v, const float* P, const float* notes, int m, bool trig)
{
    HaloState& h = v.hs;
    const bool glide = P[P_HGLD] > 0.002f;
    bool used[HALO_SLOTS] = {}, done[HALO_MAXN] = {};
    // 1. the notes it shares
    for (int k = 0; k < m; ++k)
        for (int s = 0; s < HALO_SLOTS; ++s)
        {
            HaloSlot& sl = h.s[s];
            if (used[s] || sl.stage == 0 || absf (sl.target - notes[k]) > 0.01f) continue;
            used[s] = true; done[k] = true; sl.tone = k;
            if (sl.stage == 3) { sl.stage = 1; sl.fast = 0; sl.x = swellX (sl.g); }
            break;
        }
    // 2. the voices that move: each note left takes the nearest sounding voice left
    if (glide)
        for (int k = 0; k < m; ++k)
        {
            if (done[k]) continue;
            int bs = -1; float bd = 1e9f;
            for (int s = 0; s < HALO_SLOTS; ++s)
            {
                const HaloSlot& sl = h.s[s];
                if (used[s] || (sl.stage != 1 && sl.stage != 2)) continue;
                const float d = absf (sl.target - notes[k]);
                if (d < bd) { bd = d; bs = s; }
            }
            if (bs < 0) continue;
            HaloSlot& sl = h.s[bs];
            sl.target = notes[k]; sl.tone = k;
            used[bs] = true; done[k] = true;
        }
    // what was already letting go (an older chord) now goes fast: the last chord fades over the new one, never more
    for (int s = 0; s < HALO_SLOTS; ++s) if (! used[s] && h.s[s].stage == 3) h.s[s].fast = 1;
    // 3. the new notes: a free slot, or the quietest one letting go
    const float spr = P[P_C], inv = P[P_HINV];
    for (int k = 0; k < m; ++k)
    {
        if (done[k]) continue;
        int bs = -1; float bg = 1e9f;
        for (int s = 0; s < HALO_SLOTS; ++s) if (! used[s] && h.s[s].stage == 0) { bs = s; break; }
        if (bs < 0)
            for (int s = 0; s < HALO_SLOTS; ++s) if (! used[s] && h.s[s].stage == 3 && h.s[s].g < bg) { bg = h.s[s].g; bs = s; }
        if (bs < 0) continue;
        HaloSlot& sl = h.s[bs];
        const bool quiet = sl.stage == 0 || sl.g < 1e-4f;
        sl.card = takeCard (h);
        sl.stage = 1; sl.fast = 0; sl.x = quiet ? 0.0f : swellX (sl.g); if (quiet) sl.g = 0.0f;
        sl.pitch = sl.target = notes[k]; sl.tone = k;
        sl.oct = placeOct (k, m, spr, inv);
        sl.oBase = (int) floorf_ (sl.oct + 1e-5f);
        if (quiet)
        {
            for (int j = 0; j < 4; ++j) { sl.ph[j] = v.rng.uni(); sl.mph[j] = v.rng.uni(); }
            for (int j = 0; j < 6; ++j) sl.bph[j] = v.rng.uni();
            for (int j = 0; j < 3; ++j) { sl.bAmp[j] = 0.4f + 0.6f * v.rng.uni(); sl.bTgt[j] = sl.bAmp[j]; }
            sl.drift = sl.driftT = 0.0f; sl.driftLeft = 0; sl.pwPh = v.rng.uni(); sl.a1 = sl.a2 = 0.0f;
            // TONE (from the slot's own phases: the draws above stay those of v58)
            for (int j = 0; j < 4; ++j) { sl.uph[j] = wrap1 (sl.ph[j] + 0.37f + 0.21f * (float) j); sl.umph[j] = wrap1 (sl.mph[j] + 0.5f); }
            sl.sph[0] = sl.sph[1] = 0.0f; sl.fz1 = sl.fz2 = 0.0f; sl.subA = sl.subU = 0.0f; sl.subLow = 0;
            sl.vph = wrap1 (sl.pwPh * 7.0f); sl.vrate = 4.9f + 0.8f * wrap1 (sl.pwPh * 13.0f);
        }
        used[bs] = true; done[k] = true;
    }
    // 4. what is left of the last chord lets go (PURE, LEAD: a note a semitone from one of the new chord's in 150 ms)
    const bool pure = idxOf (P[P_HARM], NUM_HARM) >= HARM_PURE;
    for (int s = 0; s < HALO_SLOTS; ++s)
        if (! used[s] && (h.s[s].stage == 1 || h.s[s].stage == 2))
        {
            h.s[s].stage = 3; h.s[s].fast = 0;
            if (pure)
                for (int k = 0; k < m; ++k)
                {
                    const int d = (((int) floorf_ (h.s[s].target - notes[k] + 0.5f) % 12) + 12) % 12;
                    if (d == 1 || d == 11) { h.s[s].fast = 2; break; }
                }
        }
    h.n = m;
    for (int k = 0; k < m; ++k) h.chord[k] = notes[k];
    // its pitch classes (from C3), for the quantiser that follows the track
    u32 pc = 0;
    for (int k = 0; k < m; ++k) pc |= 1u << ((((int) floorf_ (notes[k] + 0.5f)) % 12 + 12) % 12);
    if (m > 0) { tr[v.ti].chordPc = pc; tr[v.ti].chordRoot = ((h.root % 12) + 12) % 12; }
    (void) trig;
}

// the progression's next chord: its degree (and shift), its kind, the melody's degree (back to its first when the
// transport starts). HALO and STRUM
void Engine::haloProg (Voice& v, const float* P)
{
    HaloState& h = v.hs;
    Track& t = tr[v.ti];
    const int pr = idxOf (P[P_B], HALO_PROGS);
    h.progKind = 0; h.tint = 0;
    if (pr == PROG_WALK)
    {
        // a random walk on the degrees, mostly small steps, never far from home
        static const int MOVES[8] = { -1, 1, -2, 2, 3, -3, 4, -4 };
        if (t.hProg == 0) t.hWalk = 0;
        else
        {
            const float u = v.rng.uni();
            int mv = MOVES[u < 0.5f ? (int) (u * 4.0f) : (int) (2.0f + (u - 0.5f) * 12.0f)];
            if (t.hWalk + mv > 5 || t.hWalk + mv < -5) mv = -mv;
            t.hWalk += mv;
        }
        h.progDeg = t.hWalk; h.progSemi = 0;
    }
    else
    {
        // (the degrees up a fourth or more come from below: the chords stay round the root)
        const Prog& g = PROGS[pr];
        const int i = t.hProg % g.len;
        h.progKind = g.kind;
        if (g.kind == 2) { h.progDeg = 0; h.progSemi = 0; h.tint = g.deg[i]; }
        else { h.progDeg = g.deg[i] > 3 ? g.deg[i] - 7 : g.deg[i]; h.progSemi = g.semi[i]; }
    }
    t.hProg++;
}

void Engine::haloTrigger (Voice& v, const float* P)
{
    HaloState& h = v.hs;
    if (h.fresh)
    {
        for (int s = 0; s < HALO_SLOTS; ++s) { h.s[s].stage = 0; h.s[s].g = 0.0f; h.s[s].x = 0.0f; h.s[s].fast = 0; h.s[s].subA = h.s[s].subU = 0.0f; h.s[s].subLow = 0; }
        h.n = 0; for (int k = 0; k < HALO_KEY; ++k) h.key[k] = -99999;
        h.wowPh = v.rng.uni(); h.wrA = h.wrB = 0.0f; h.wrPos = 0.0f; h.norm = 0.0f; h.warmG = 1.0f;
        h.at1 = h.at2 = h.al1 = h.al2 = 0.0f; h.arng.seed (0xCA4D5u + 7919u * (u32) v.ti);
        ageReset (v);
        h.fresh = 0;
    }
    haloProg (v, P);
    const float tuneRaw = (P[P_TUNE] - 0.5f) * 48.0f + v.note;
    const u32 qm = v.quant ? harmMask (v.ti) : 0u;
    float notes[HALO_MAXN]; int key[HALO_KEY];
    const int m = haloChord (v, P, tuneRaw, qm, notes, key, nullptr);
    haloAssign (v, P, notes, m, true);
    for (int k = 0; k < HALO_KEY; ++k) h.key[k] = key[k];
    h.held = 1;
    h.holdLeft = v.holdSec > 0.0f ? v.holdSec * sr : -1.0f;
}

void Engine::haloRender (Voice& v, const float* P, float tuneRaw, u32 qm, int n, float* out)
{
    HaloState& h = v.hs;
    const Track& t = tr[v.ti];
    // a knob, a lock or an LFO that changes the chord re-voices it while it is held; not a chord held until its next
    // trig once the transport stopped or the track is muted: it waits for its swell to be in to let go (below), and a
    // modulator re-voicing it over and over (v90.1: a random one on TUNE or CHRD, quicker than a long SWELL) kept it
    // swelling, never let go, its chords changing on for ever after the stop
    const bool ending = h.holdLeft < 0.0f && (! running || t.mute);
    if (h.held && ! ending)
    {
        float notes[HALO_MAXN]; int key[HALO_KEY];
        const int m = haloChord (v, P, tuneRaw, qm, notes, key, h.key);
        if (m >= 0) { haloAssign (v, P, notes, m, false); for (int k = 0; k < HALO_KEY; ++k) h.key[k] = key[k]; }
    }
    // the hold: GATE, or until the next trig while the transport runs and the track plays (a stop lets go once the
    // swell is in)
    if (h.held)
    {
        bool letGo = false;
        if (h.holdLeft >= 0.0f) { h.holdLeft -= (float) n; if (h.holdLeft <= 0.0f) letGo = true; }
        else if (! running || t.mute)
        {
            letGo = true;
            for (int s = 0; s < HALO_SLOTS; ++s) if (h.s[s].stage == 1) letGo = false;
        }
        if (letGo)
        {
            h.held = 0;
            for (int s = 0; s < HALO_SLOTS; ++s) if (h.s[s].stage == 1 || h.s[s].stage == 2) h.s[s].stage = 3;
        }
    }
    // this chunk's settings
    const float fn = (float) n;
    const float tS = 0.005f * powf_ (1600.0f, P[P_HSWL]), xInc = 1.0f / (tS * sr);
    const float tR = 0.05f * powf_ (400.0f, P[P_DEC]), rc0 = expf_ (-6.9f / (tR * sr)), rcF = expf_ (-6.9f / (0.06f * sr));
    const float gT = 0.02f * powf_ (100.0f, P[P_HGLD]), gk = P[P_HGLD] > 0.002f ? 1.0f - expf_ (-fn * 4.6f / (gT * sr)) : 1.0f;
    const float ok = 1.0f - expf_ (-fn / (0.025f * sr));
    const float spr = P[P_C], inv = P[P_HINV];
    const float w5 = clampf (P[P_D], 0.0f, 1.0f) * 5.0f; const int wi = mini ((int) w5, 4); const float wf = w5 - (float) wi;
    const float wN = 1.0f / sqrtf_ ((1.0f - wf) * (1.0f - wf) + wf * wf + 2.0f * SHAPE_RHO[wi] * wf * (1.0f - wf));
    const float drf = P[P_HDRF], det = drf * drf * 16.0f;
    const float bl = P[P_E], bL[3] = { smooth01 (bl * 3.0f) * 0.42f, smooth01 (bl * 3.0f - 1.0f) * 0.3f, smooth01 (bl * 3.0f - 2.0f) * 0.22f };
    // AIR: the knob's curve (a breath at its first quarter already), the bands' levels
    const float air = P[P_HAIR] * (0.35f + 0.65f * P[P_HAIR]);
    float top[CHUNK];
    if (air > 0.0f) for (int i = 0; i < n; ++i) top[i] = 0.0f;
    // AGE: the cards' tuning (cents at the card the furthest out) and how unequal they are
    const float dtC = P[P_GDTN] * P[P_GDTN] * 50.0f, var = P[P_GVAR];
    const bool varOn = var > 0.0005f;
    const float bk = 1.0f - expf_ (-fn / (0.8f * sr)), dk = 1.0f - expf_ (-fn / (1.2f * sr));
    // TONE: the unison's spread and level, the sub, the low-pass (octaves over each note; OPEN: how far under, at the
    // start of the swell), the vibrato's depth (cents), WARM. At their rest they cost nothing and change nothing
    const float uni = P[P_HUNI], sub = P[P_HSUB], cut = P[P_HCUT], opn = P[P_HOPN], vib = P[P_HVIB], wrm = P[P_HWRM];
    const bool uniOn = uni > 0.002f, subOn = sub > 0.002f, fltOn = cut < 0.998f || opn > 0.002f, vibOn = vib > 0.002f;
    const float uDet = exp2f_ ((9.0f + 22.0f * uni) * (1.0f / 1200.0f)), uG = uni * 0.85f, uN = 1.0f / sqrtf_ (1.0f + uG * uG);
    const float vibD = vib * vib * 45.0f, rcC = expf_ (-6.9f / (0.15f * sr));
    // SUB (v71): a pure sine (v72: PRES, S2, its harmonics over it) under the lowest note of the chord, past the voices'
    // low-pass, WARM and their level against the number of notes; played by the lowest of the notes sounding (not
    // letting go: that one keeps its sub as it fades), its share glided over 20 ms (from a chord to the next, the old
    // note's sub fades out with it as the new one's comes in); up to about the chord's own level, and over (SUB: its square).
    // v72, where it sits (S1): AUTO, the lowest note brought down as many octaves as it can while staying at SUB_LO or
    // over (its sub between 45 and 90 Hz: a low bass is doubled as it is, a high chord gets its sub all the same); NOTE,
    // the note itself; -1 OCT, an octave down if it stays at SUB_LO or over. On the note itself, it is the voice's own
    // phase (its first oscillator, each copy as the voice crossfades them), so it adds to the note's fundamental
    const float subG = sub * sub * 0.25f, subK = 1.0f - expf_ (-fn / (0.02f * sr));
    const int subMode = idxOf (P[P_S1], 3);
    const float pres = clampf (P[P_S2], 0.0f, 1.0f), sH2 = 0.7f * pres, sH3 = 0.45f * pres, sH4 = 0.2f * pres;
    float subBuf[CHUNK];
    int lowS = -1;
    if (subOn)
    {
        for (int i = 0; i < n; ++i) subBuf[i] = 0.0f;
        float lowSt = 1e9f;
        for (int s = 0; s < HALO_SLOTS; ++s)
        {
            const HaloSlot& sl = h.s[s];
            if (sl.stage != 1 && sl.stage != 2) continue;
            const float st = sl.pitch + 12.0f * sl.oct;
            if (st < lowSt) { lowSt = st; lowS = s; }
        }
        for (int s = 0; s < HALO_SLOTS; ++s) { HaloSlot& sl = h.s[s]; if (sl.stage == 1 || sl.stage == 2) sl.subLow = s == lowS; }
    }
    // the shared wobble (DRFT: a slow sine and a smooth random, like a tired tape)
    h.wowPh += fn * 0.33f / sr; h.wowPh -= floorf_ (h.wowPh);
    h.wrPos += fn * 0.21f / sr;
    if (h.wrPos >= 1.0f) { h.wrPos -= 1.0f; h.wrA = h.wrB; h.wrB = v.rng.bi(); }
    const float wrv = h.wrA + (h.wrB - h.wrA) * smooth01 (h.wrPos);
    const float wowC = drf * drf * 9.0f * (0.6f * sin2pi (h.wowPh) + 0.4f * wrv);
    // the level against the number of notes
    const float nt = 1.0f / sqrtf_ ((float) maxi (h.n, 1));
    if (h.norm <= 0.0f) h.norm = nt; else h.norm += (nt - h.norm) * minf (1.0f, fn / (0.05f * sr));
    for (int i = 0; i < n; ++i) out[i] = 0.0f;
    float lvMax = 0.0f;
    const float nyq = 0.45f * sr;
    for (int s = 0; s < HALO_SLOTS; ++s)
    {
        HaloSlot& sl = h.s[s];
        if (sl.stage == 0) continue;
        float rc = sl.fast == 2 ? minf (rcC, rc0) : sl.fast ? minf (rcF, rc0) : rc0;
        // VARI: this card's swell and release, level, the wave's place
        float xIncS = xInc, lvS = 1.0f, wfS = wf, wNS = wN; int wiS = wi;
        if (varOn)
        {
            const int cd = sl.card & 7;
            xIncS = xInc * exp2f_ (-0.9f * var * cardUnit (v.ti, cd, 1));
            if (! sl.fast) rc = powf_ (rc0, exp2f_ (-0.7f * var * cardUnit (v.ti, cd, 2)));
            lvS = exp2f_ (0.6f * var * cardUnit (v.ti, cd, 3));
            const float w5s = clampf (P[P_D] + 0.14f * var * cardUnit (v.ti, cd, 4), 0.0f, 1.0f) * 5.0f;
            wiS = mini ((int) w5s, 4); wfS = w5s - (float) wiS;
            wNS = 1.0f / sqrtf_ ((1.0f - wfS) * (1.0f - wfS) + wfS * wfS + 2.0f * SHAPE_RHO[wiS] * wfS * (1.0f - wfS));
        }
        // pitch: the glide, the slot's own drift, the wobble
        sl.pitch += (sl.target - sl.pitch) * gk;
        if (absf (sl.target - sl.pitch) < 1e-4f) sl.pitch = sl.target;
        sl.driftLeft -= n;
        if (sl.driftLeft <= 0) { sl.driftT = v.rng.bi() * drf * 12.0f; sl.driftLeft = (int) ((2.0f + 3.0f * v.rng.uni()) * sr); }
        sl.drift += (sl.driftT - sl.drift) * dk;
        // the octave (SPRD, INV), faded between two copies an octave apart
        const float oT = placeOct (sl.tone, h.n, spr, inv);
        sl.oct += (oT - sl.oct) * ok;
        if (absf (oT - sl.oct) < 1e-4f) sl.oct = oT;
        const float oLo = floorf_ (sl.oct + 1e-5f), of = sl.oct - oLo;
        // crossing an octave: the copy that was sounding keeps its phases (the upper becomes the lower, or back)
        while ((int) oLo > sl.oBase)
        {
            sl.ph[0] = sl.ph[2]; sl.ph[1] = sl.ph[3]; sl.mph[0] = sl.mph[2]; sl.mph[1] = sl.mph[3];
            for (int j = 0; j < 3; ++j) sl.bph[j] = sl.bph[3 + j];
            sl.ph[2] = v.rng.uni(); sl.ph[3] = v.rng.uni();
            ++sl.oBase;
        }
        while ((int) oLo < sl.oBase)
        {
            sl.ph[2] = sl.ph[0]; sl.ph[3] = sl.ph[1]; sl.mph[2] = sl.mph[0]; sl.mph[3] = sl.mph[1];
            for (int j = 0; j < 3; ++j) sl.bph[3 + j] = sl.bph[j];
            sl.ph[0] = v.rng.uni(); sl.ph[1] = v.rng.uni();
            --sl.oBase;
        }
        const float wLo = cos2pi (of * 0.25f), wHi = sin2pi (of * 0.25f);
        float cents = sl.drift + wowC;
        if (vibOn) { sl.vph = wrap1 (sl.vph + fn * sl.vrate / sr); cents += vibD * sl.g * sin2pi (sl.vph); }
        if (dtC > 0.0f) cents += dtC * cardErr (v.ti, sl.card & 7);
        const float st = sl.pitch + cents * 0.01f + 12.0f * oLo;
        const float f0 = C3_HZ * exp2f_ (st * (1.0f / 12.0f));
        const float dA = exp2f_ (det * (1.0f / 1200.0f)), dB = 1.0f / dA;
        float inc[4] = { f0 * dA / sr, f0 * dB / sr, 2.0f * f0 * dA / sr, 2.0f * f0 * dB / sr };
        float cw[2] = { wLo, wHi };
        if (f0 * 1.01f > nyq) cw[0] = 0.0f;
        if (2.02f * f0 > nyq) cw[1] = 0.0f;
        const bool c0 = cw[0] > 1e-3f, c1 = cw[1] > 1e-3f;
        // FM shapes fade their index out towards the top (no aliasing)
        const float fmS[2] = { clampf (1.0f - f0 / 5000.0f, 0.0f, 1.0f), clampf (1.0f - 2.0f * f0 / 5000.0f, 0.0f, 1.0f) };
        sl.pwPh += fn * (0.11f + 0.017f * (float) s) / sr; sl.pwPh -= floorf_ (sl.pwPh);
        const float pw = 0.5f + 0.17f * sin2pi (sl.pwPh);
        // BLOOM: the octave, the twelfth and the double octave, each breathing on its own
        for (int j = 0; j < 3; ++j)
        {
            if (v.rng.uni() < fn / (1.5f * sr)) sl.bTgt[j] = 0.35f + 0.65f * v.rng.uni();
            sl.bAmp[j] += (sl.bTgt[j] - sl.bAmp[j]) * bk;
        }
        static const float BR[3] = { 2.0f, 3.0f, 4.0f };
        float bInc[6], bG[6];
        for (int c = 0; c < 2; ++c)
            for (int j = 0; j < 3; ++j)
            {
                const float f = f0 * (c ? 2.0f : 1.0f) * BR[j];
                bInc[c * 3 + j] = f / sr;
                bG[c * 3 + j] = f < nyq ? bL[j] * sl.bAmp[j] * cw[c] * clampf ((nyq - f) / (0.1f * sr), 0.0f, 1.0f) : 0.0f;
            }
        const bool bloom = bl > 0.002f;
        // AIR: a breath pulsing with the note (noise shaped by its period), through a band on its harmonic nearest
        // 3 kHz (the same breath whatever the note's height); its top goes to the chord's high-pass
        const bool hiC = c1 && wHi > wLo;
        const float fr = f0 * (hiC ? 2.0f : 1.0f);
        const float af = minf (maxf (2.0f, floorf_ (AIR_F / fr + 0.5f)) * fr, 0.4f * sr);
        const float ag = tanpi (af / sr), aq = 1.0f / 2.2f, aa1 = 1.0f / (1.0f + ag * (ag + aq)), aa2 = ag * aa1, aa3 = ag * aa2;
        const float airG = air * 1.1f * sqrtf_ (AIR_F / af);
        const int aPh = hiC ? 2 : 0;
        // TONE: the unison's increments, the sub (the bass only), the low-pass
        float uInc[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (uniOn) { uInc[0] = f0 * uDet / sr; uInc[1] = f0 / uDet / sr; uInc[2] = 2.0f * uInc[0]; uInc[3] = 2.0f * uInc[1]; }
        // the sub: this note's (its pitch as it glides, its drift and vibrato, both copies' octave as it fades) an octave or
        // two down, its share
        const bool subS = subOn && (sl.subLow || sl.subA > 1e-4f || sl.subU > 1e-4f);
        float sInc = 0.0f, sA0 = sl.subA, sA1 = sl.subA, sU0 = sl.subU, sU1 = sl.subU;
        if (subS)
        {
            // how many octaves down (from where the note is going: a chord change, not its glide, moves it)
            int kd = 0;
            if (subMode != 1)
            {
                float ft = C3_HZ * exp2f_ ((sl.target + 12.0f * oT) * (1.0f / 12.0f));
                while (kd < 6 && ft * 0.5f >= SUB_LO) { ft *= 0.5f; ++kd; }
                if (subMode == 2) kd = mini (kd, 1);
            }
            sInc = C3_HZ * exp2f_ ((sl.pitch + cents * 0.01f + 12.0f * sl.oct) * (1.0f / 12.0f)) * exp2f_ (-(float) maxi (kd, 1)) / sr;
            const float tA = sl.subLow && kd > 0 ? 1.0f : 0.0f, tU = sl.subLow && kd == 0 ? 1.0f : 0.0f;
            sA1 = sl.subA + (tA - sl.subA) * subK; if (absf (sA1 - tA) < 1e-4f) sA1 = tA;
            sU1 = sl.subU + (tU - sl.subU) * subK; if (absf (sU1 - tU) < 1e-4f) sU1 = tU;
            sl.subA = sA1; sl.subU = sU1;
        }
        float fa1 = 0.0f, fa2 = 0.0f, fa3 = 0.0f;
        if (fltOn)
        {
            const float co = 0.5f + 7.5f * cut - opn * 6.0f * (1.0f - sl.g);
            const float fc = clampf (f0 * exp2f_ (co), 30.0f, 0.45f * sr), gF = tanpi (fc / sr), kF = 1.1f;
            fa1 = 1.0f / (1.0f + gF * (gF + kF)); fa2 = gF * fa1; fa3 = gF * fa2;
        }
        for (int i = 0; i < n; ++i)
        {
            // the envelope: a smoothstep swell, an exponential let-go
            if (sl.stage == 1) { sl.x += xIncS; if (sl.x >= 1.0f) { sl.x = 1.0f; sl.stage = 2; } sl.g = smooth01 (sl.x); }
            else if (sl.stage == 3) { sl.g *= rc; if (sl.g < 1e-5f) { sl.g = 0.0f; sl.stage = 0; } }
            float y = 0.0f;
            const float uP0 = sl.ph[0], uP1 = sl.ph[2];       // (SUB on the note: the phases this sample plays)
            for (int c = 0; c < 2; ++c)
            {
                if (! (c ? c1 : c0)) continue;
                float yc = 0.0f;
                for (int o = 0; o < 2; ++o)
                {
                    const int q = c * 2 + o;
                    const float ph = sl.ph[q], dt = inc[q];
                    float a = haloShape (wiS, ph, dt, sl.mph[q], pw, fmS[c] * (wiS == 5 ? 1.8f : 1.4f));
                    if (wfS > 0.001f) { const float b = haloShape (wiS + 1, ph, dt, sl.mph[q], pw, fmS[c] * (wiS + 1 == 5 ? 1.8f : 1.4f)); a = (a + (b - a) * wfS) * wNS; }
                    yc += a;
                    sl.ph[q] = wrap1 (ph + dt);
                    sl.mph[q] = wrap1 (sl.mph[q] + dt * 3.53f);
                }
                if (uniOn)
                {
                    // UNIS: two more, spread wider than DRFT's pair
                    float yu = 0.0f;
                    for (int o = 0; o < 2; ++o)
                    {
                        const int q = c * 2 + o;
                        const float ph = sl.uph[q], dt = uInc[q];
                        float a = haloShape (wiS, ph, dt, sl.umph[q], pw, fmS[c] * (wiS == 5 ? 1.8f : 1.4f));
                        if (wfS > 0.001f) { const float b = haloShape (wiS + 1, ph, dt, sl.umph[q], pw, fmS[c] * (wiS + 1 == 5 ? 1.8f : 1.4f)); a = (a + (b - a) * wfS) * wNS; }
                        yu += a;
                        sl.uph[q] = wrap1 (ph + dt);
                        sl.umph[q] = wrap1 (sl.umph[q] + dt * 3.53f);
                    }
                    yc = (yc + uG * yu) * uN;
                }
                y += yc * 0.5f * cw[c];
                if (bloom)
                    for (int j = 0; j < 3; ++j)
                    {
                        const int q = c * 3 + j;
                        if (bG[q] > 0.0f) y += sin2pi (sl.bph[q]) * bG[q];
                        sl.bph[q] = wrap1 (sl.bph[q] + bInc[q]);
                    }
            }
            if (subS)
            {
                const float r = (float) (i + 1) / (float) n, gA = sA0 + (sA1 - sA0) * r, gU = sU0 + (sU1 - sU0) * r;
                float w = 0.0f;
                if (gA > 0.0f) w += gA * subWave (sl.sph[0], sH2, sH3, sH4);
                if (gU > 0.0f) w += gU * ((c0 ? cw[0] * subWave (uP0, sH2, sH3, sH4) : 0.0f) + (c1 ? cw[1] * subWave (uP1, sH2, sH3, sH4) : 0.0f));
                subBuf[i] += w * subG * sl.g;
                sl.sph[0] = wrap1 (sl.sph[0] + sInc);
            }
            if (fltOn)
            {
                const float v3 = y - sl.fz2, v1 = fa1 * sl.fz1 + fa2 * v3, v2 = sl.fz2 + fa2 * sl.fz1 + fa3 * v3;
                sl.fz1 = 2.0f * v1 - sl.fz1; sl.fz2 = 2.0f * v2 - sl.fz2;
                y = v2;
            }
            if (air > 0.0f)
            {
                const float x = v.rng.bi() * (0.3f + 1.4f * absf (sl.ph[aPh] - 0.5f));
                const float v3 = x - sl.a2, v1 = aa1 * sl.a1 + aa2 * v3, v2 = sl.a2 + aa2 * sl.a1 + aa3 * v3;
                sl.a1 = 2.0f * v1 - sl.a1; sl.a2 = 2.0f * v2 - sl.a2;
                y += v1 * airG;
                top[i] += x * sl.g;
            }
            out[i] += y * (sl.g * lvS);
        }
        if (isBad (sl.a1) || isBad (sl.a2)) { sl.a1 = sl.a2 = 0.0f; }
        if (isBad (sl.fz1) || isBad (sl.fz2)) { sl.fz1 = sl.fz2 = 0.0f; }
        lvMax = maxf (lvMax, sl.g);
    }
    // AIR: the top of the chord's breath (over AIR_TOP, softened over AIR_SOFT)
    if (air > 0.0f)
    {
        const float tg = tanpi (minf (AIR_TOP, 0.42f * sr) / sr), tk = 1.4f, t1 = 1.0f / (1.0f + tg * (tg + tk)), t2 = tg * t1, t3 = tg * t2;
        const float lg = tanpi (minf (AIR_SOFT, 0.42f * sr) / sr), lk = 1.3f, l1 = 1.0f / (1.0f + lg * (lg + lk)), l2 = lg * l1, l3 = lg * l2;
        const float tG = air * AIR_TG;
        for (int i = 0; i < n; ++i)
        {
            float x = top[i], v3 = x - h.at2, v1 = t1 * h.at1 + t2 * v3, v2 = h.at2 + t2 * h.at1 + t3 * v3;
            h.at1 = 2.0f * v1 - h.at1; h.at2 = 2.0f * v2 - h.at2;
            x -= tk * v1 + v2;
            v3 = x - h.al2; v1 = l1 * h.al1 + l2 * v3; v2 = h.al2 + l2 * h.al1 + l3 * v3;
            h.al1 = 2.0f * v1 - h.al1; h.al2 = 2.0f * v2 - h.al2;
            out[i] += v2 * tG;
        }
        if (isBad (h.at1) || isBad (h.at2) || isBad (h.al1) || isBad (h.al2)) h.at1 = h.at2 = h.al1 = h.al2 = 0.0f;
    }
    const float gn = h.norm * 0.3f;            // (a held chord is dense: it sits under the drums at the same VOL)
    for (int i = 0; i < n; ++i) out[i] *= gn;
    if (wrm > 0.002f)
    {
        // WARM: a soft clip, biased (its even harmonics), its slope at rest put back; what it squashes is given back
        // (the level in against the level out, glided over 60 ms): the level stays
        const float K = 2.5f * (1.0f + 1.6f * wrm), bias = 0.3f * wrm, b0 = softclip (bias), mixW = minf (1.0f, wrm * 5.0f);
        const float slope = (softclip (bias + 1e-3f) - softclip (bias - 1e-3f)) * 500.0f, iK = 1.0f / (K * slope);
        float eIn = 0.0f, eOut = 0.0f;
        const float g0 = h.warmG > 0.0f ? h.warmG : 1.0f;
        for (int i = 0; i < n; ++i)
        {
            const float x = out[i], w = (softclip (x * K + bias) - b0) * iK;
            const float y = x + (w * g0 - x) * mixW;
            eIn += x * x; eOut += y * y; out[i] = y;
        }
        const float gt = eOut > 1e-12f ? clampf (sqrtf_ (eIn / eOut) * g0, 0.5f, 2.5f) : g0;
        h.warmG = g0 + (gt - g0) * minf (1.0f, fn / (0.06f * sr));
    }
    else h.warmG = 1.0f;
    if (subOn) for (int i = 0; i < n; ++i) out[i] += subBuf[i];
    ageFx (v, P, n, out);
    v.level = lvMax;
    v.ns += (u32) n;
}

void Engine::ageReset (Voice& v)
{
    AgeFx& a = v.ag;
    a.b1 = a.b2 = a.env = a.h1 = a.h2 = a.s1 = a.s2 = 0.0f;
    a.dLeft = 0.0f; a.dDepth = 0.0f; a.d = 0.0f; a.lp = 0.0f;
    a.rng.seed (0xA6E0u + 104729u * (u32) v.ti);
}

// AGE, its last stages (HALO, STRUM), on the machine's output: at their rest they change not a sample
//  SHEN  the band over 3 kHz (to 9: what it makes stays under the top), full-wave rectified: its octave and the
//        differences of its partials, made of the sound itself (the louder, the brighter), high-passed over 6 kHz; and
//        a shelf: what is over 8 kHz, raised (+7 dB at the top)
//  DROP  now and then (more often, longer and deeper the higher) the tape loses the head: the highs go first, then
//        the level (in over 4 ms, back over 30)
void Engine::ageFx (Voice& v, const float* P, int n, float* out)
{
    AgeFx& a = v.ag;
    const float shen = P[P_GSHN], drop = P[P_GDRP];
    if (shen > 0.002f)
    {
        const float bg = tanpi (minf (5200.0f, 0.42f * sr) / sr), bk = 1.4f, b1 = 1.0f / (1.0f + bg * (bg + bk)), b2 = bg * b1, b3 = bg * b2;
        const float hg = tanpi (minf (6000.0f, 0.42f * sr) / sr), hk = 1.2f, h1 = 1.0f / (1.0f + hg * (hg + hk)), h2 = hg * h1, h3 = hg * h2;
        // (the shelf: Simper's state-variable high shelf, A the square root of its gain)
        const float sA = exp2f_ (shen * 7.0f / 12.04f), sg = tanpi (minf (8000.0f, 0.42f * sr) / sr) * sqrtf_ (sA), sq = 1.4142f;
        const float s1 = 1.0f / (1.0f + sg * (sg + sq)), s2 = sg * s1, s3 = sg * s2;
        const float m1 = sq * (1.0f - sA) * sA, m2 = 1.0f - sA * sA, m0 = sA * sA;
        const float gS = shen * shen * 3.5f;
        for (int i = 0; i < n; ++i)
        {
            const float x = out[i];
            float v3 = x - a.b2, v1 = b1 * a.b1 + b2 * v3, v2 = a.b2 + b2 * a.b1 + b3 * v3;
            a.b1 = 2.0f * v1 - a.b1; a.b2 = 2.0f * v2 - a.b2;
            const float r = absf (v1);
            v3 = r - a.h2; v1 = h1 * a.h1 + h2 * v3; v2 = a.h2 + h2 * a.h1 + h3 * v3;
            a.h1 = 2.0f * v1 - a.h1; a.h2 = 2.0f * v2 - a.h2;
            const float sh = r - hk * v1 - v2;
            v3 = x - a.s2; v1 = s1 * a.s1 + s2 * v3; v2 = a.s2 + s2 * a.s1 + s3 * v3;
            a.s1 = 2.0f * v1 - a.s1; a.s2 = 2.0f * v2 - a.s2;
            out[i] = m0 * x + m1 * v1 + m2 * v2 + gS * sh;
        }
        if (isBad (a.b1) || isBad (a.b2) || isBad (a.h1) || isBad (a.h2) || isBad (a.s1) || isBad (a.s2)) a.b1 = a.b2 = a.h1 = a.h2 = a.s1 = a.s2 = 0.0f;
    }
    else a.b1 = a.b2 = a.h1 = a.h2 = a.s1 = a.s2 = 0.0f;
    if (drop > 0.002f || a.d > 1e-5f)
    {
        if (a.dLeft <= 0.0f && drop > 0.002f && a.rng.uni() < (0.08f + 2.6f * drop * drop) * (float) n / sr)
        {
            const float u = a.rng.uni(), w = a.rng.uni();
            a.dLeft = (0.025f + 0.3f * u * u * (0.4f + 0.6f * drop)) * sr;
            a.dDepth = clampf ((0.3f + 0.7f * w) * (0.35f + 0.65f * drop), 0.0f, 0.97f);
        }
        const float tgt = a.dLeft > 0.0f ? a.dDepth : 0.0f;
        a.dLeft -= (float) n;
        const float kin = 1.0f - expf_ (-1.0f / (0.004f * sr)), kout = 1.0f - expf_ (-1.0f / (0.03f * sr));
        const float c = 1.0f - expf_ (-TWO_PI * minf (16000.0f * exp2f_ (-5.0f * a.d), 0.45f * sr) / sr);
        for (int i = 0; i < n; ++i)
        {
            a.d += (tgt - a.d) * (tgt > a.d ? kin : kout);
            const float x = out[i];
            a.lp += c * (x - a.lp);
            out[i] = (x + (a.lp - x) * minf (1.0f, 4.0f * a.d)) * (1.0f - a.d);
        }
        if (isBad (a.lp) || isBad (a.d)) { a.lp = 0.0f; a.d = 0.0f; }
    }
    else { a.lp = 0.0f; a.d = 0.0f; a.dLeft = 0.0f; }
}

// WIDE: the track's output through two taps of a short delay, swung in opposite directions (an ensemble). Their
// difference goes to the sides (the mono sum keeps the track as it was), a little of their sum thickens the middle
void Engine::haloWide (Track& t, const float* P, const float* x, float* side, int n)
{
    HaloWide& w = t.wide;
    const int wm = machineOf (P[P_MACH]);
    const float target = wm == M_HALO || wm == M_STRUM ? P[P_HWID] : 0.0f;
    if (! w.on)
    {
        for (int i = 0; i < 2048; ++i) w.buf[i] = 0.0f;
        w.w = 0; w.ph1 = 0.0f; w.ph2 = 0.25f; w.amt = 0.0f; w.on = 1;
    }
    const float ak = 1.0f - expf_ (-1.0f / (0.03f * sr));
    const float base = 0.0058f * sr, dep = 0.0019f * sr;
    const float i1 = 0.47f / sr, i2 = 0.113f / sr;
    float* X = const_cast<float*> (x);
    for (int i = 0; i < n; ++i)
    {
        w.amt += (target - w.amt) * ak;
        w.buf[w.w] = X[i];
        w.ph1 += i1; if (w.ph1 >= 1.0f) w.ph1 -= 1.0f;
        w.ph2 += i2; if (w.ph2 >= 1.0f) w.ph2 -= 1.0f;
        const float lfo = 0.8f * sin2pi (w.ph1) + 0.2f * sin2pi (w.ph2);
        float rd[2];
        for (int k = 0; k < 2; ++k)
        {
            const float d = base + dep * (k ? -lfo : lfo);
            float p = (float) w.w - d; if (p < 0.0f) p += 2048.0f;
            const int i0 = (int) p; const float fr = p - (float) i0;
            const float a = w.buf[i0 & 2047], b = w.buf[(i0 + 1) & 2047];
            rd[k] = a + (b - a) * fr;
        }
        w.w = (w.w + 1) & 2047;
        side[i] = (rd[0] - rd[1]) * 0.5f * w.amt;
        X[i] += (0.5f * (rd[0] + rd[1]) - X[i]) * 0.25f * w.amt;
    }
    if (target <= 0.0f && w.amt < 1e-5f) w.on = 0;
}

// for the screen: on, the number of notes, the chord (close voicing, semitones from C3), the shape, the progression,
// its step (WALK: the degree), the scale (pitch-class mask); then each slot: its sounding pitch (octave included, the
// lower copy), its gain and the weight of its upper copy
void Engine::haloInfo (int ti, float* out) const
{
    for (int k = 0; k < HALO_INFO; ++k) out[k] = 0.0f;
    if (ti < 0 || ti >= NUM_TRACKS) return;
    const Track& t = tr[ti];
    const Voice& v = t.voice[t.cur];
    const int mm = v.active ? v.machine : machineOf (t.p[P_MACH]);
    if (mm == M_CHOIR) { choirInfo (ti, out); return; }
    if (mm == M_MANTRA) { mantraInfo (ti, out); return; }
    if (mm == M_HOWL) { howlInfo (ti, out); return; }
    if (! v.active || (v.machine != M_HALO && v.machine != M_STRUM)) { if (mm == M_STRUM) strumInfo (ti, out); return; }
    const HaloState& h = v.hs;
    out[0] = 1.0f; out[1] = (float) h.n;
    for (int k = 0; k < h.n && k < HALO_MAXN; ++k) out[2 + k] = h.chord[k];
    const int pr = idxOf (t.p[P_B], HALO_PROGS);
    out[8] = (float) h.key[1]; out[9] = (float) pr;
    out[10] = pr == PROG_WALK ? (float) t.hWalk : (float) ((t.hProg + PROGS[pr].len - 1) % PROGS[pr].len);
    out[11] = (float) h.key[2];
    if (v.machine == M_STRUM) { strumInfo (ti, out); return; }
    for (int s = 0; s < HALO_SLOTS; ++s)
    {
        const HaloSlot& sl = h.s[s];
        const float oLo = floorf_ (sl.oct + 1e-5f);
        out[12 + 3 * s] = sl.stage ? sl.pitch + 12.0f * oLo : -999.0f;
        out[13 + 3 * s] = sl.stage ? sl.g : 0.0f;
        out[14 + 3 * s] = sl.oct - oLo;
    }
}

} // namespace sk
