// IMPAKT — CUBE (v74): the dual wavetable voice in a cube of 512 waves (sk_cube.h). Own implementation.
#include "sk_engine.h"
#include "sk_fft.h"

namespace sk {

namespace {

CubeTables FACT;
bool factReady = false;

inline float frac1 (float p) { return p - floorf_ (p); }

// a wave's band-limited versions from its 256 samples: its harmonics (its DC taken out) up to 127, 64, 32 .. 2, each
// made again at 512 samples (twice the wave's resolution), 16-bit (16384 = 1)
void makeMips (RFFT<256>& f256, RFFT<512>& f512, const float* x, short (*out)[CUBE_LEN])
{
    float re[129], im[129], R[257], I[257], y[CUBE_LEN];
    f256.forward (x, re, im);
    for (int L = 0; L < CUBE_MIPS; ++L)
    {
        const int H = L == 0 ? 127 : 128 >> L;
        for (int k = 0; k <= 256; ++k) { R[k] = 0.0f; I[k] = 0.0f; }
        for (int k = 1; k <= H; ++k) { R[k] = 2.0f * re[k]; I[k] = 2.0f * im[k]; }
        f512.inverse (R, I, y);
        for (int i = 0; i < CUBE_LEN; ++i)
        {
            const float v = y[i] * 16384.0f;
            out[L][i] = (short) (v > 32767.0f ? 32767 : v < -32767.0f ? -32767 : (int) (v + (v >= 0.0f ? 0.5f : -0.5f)));
        }
    }
}

// ---- the factory cube: eight banks of its own, each 8 x 8 waves that turn into one another along X and Y
u32 hashU (u32 a) { a ^= a >> 16; a *= 0x7feb352du; a ^= a >> 15; a *= 0x846ca68bu; a ^= a >> 16; return a; }
float hashF (u32 a) { return (float) (hashU (a) >> 8) * (1.0f / 16777216.0f); }       // [0, 1)
float sawF (float t) { return 1.0f - 2.0f * frac1 (t); }
float triF (float t) { const float u = frac1 (t + 0.25f); return u < 0.5f ? 4.0f * u - 1.0f : 3.0f - 4.0f * u; }
float pulseF (float t, float w) { return frac1 (t) < w ? 1.0f : -1.0f; }

void factoryWave (int z, int x, int y, float* w)
{
    const float fy = (float) y / 7.0f;
    for (int n = 0; n < CUBE_SRC; ++n) w[n] = 0.0f;
    switch (z)
    {
        case 0:
        {
            // BASIC: along X sine, triangle, saw, square, pulses (30 %, 15 %), a saw and its octave, a 6 % pulse;
            // along Y the same through a hard sync, its ratio 1 .. 4.5 (bright, vocal)
            const float r = 1.0f + 0.5f * (float) y;
            for (int n = 0; n < CUBE_SRC; ++n)
            {
                const float t = frac1 ((float) n / (float) CUBE_SRC * r);
                float s;
                switch (x)
                {
                    case 0: s = sin2pi (t); break;
                    case 1: s = triF (t); break;
                    case 2: s = sawF (t); break;
                    case 3: s = pulseF (t, 0.5f); break;
                    case 4: s = pulseF (t, 0.3f); break;
                    case 5: s = pulseF (t, 0.15f); break;
                    case 6: s = 0.6f * sawF (t) + 0.4f * sawF (2.0f * t); break;
                    default: s = pulseF (t, 0.06f); break;
                }
                w[n] = s;
            }
            break;
        }
        case 1:
        {
            // HARM: the harmonics one by one (1, 2, 3, 5, 8 .. 34 of them along X), their slope gentler and the even
            // ones fainter along Y (from a round organ to a hollow, buzzing reed)
            static const int NH[8] = { 1, 2, 3, 5, 8, 13, 21, 34 };
            const float slope = 1.4f - 0.15f * (float) y, ev = 1.0f - 0.12f * (float) y;
            for (int k = 1; k <= NH[x]; ++k)
            {
                const float a = powf_ ((float) k, -slope) * (k % 2 == 0 ? ev : 1.0f);
                for (int n = 0; n < CUBE_SRC; ++n) w[n] += a * sin2pi ((float) (k * n) / (float) CUBE_SRC);
            }
            break;
        }
        case 2:
        {
            // VOWEL: two formants over the harmonics, the first rising along X (the 2nd .. the 13th), the second
            // further from it along Y (from an "oo" to an "ee"), a little of the fundamental under them
            const float h1 = 1.5f + 1.6f * (float) x, h2 = h1 * (1.6f + 0.25f * (float) y), bw = 1.2f + 0.25f * (float) x;
            for (int k = 1; k <= 64; ++k)
            {
                const float d1 = ((float) k - h1) / bw, d2 = ((float) k - h2) / (1.4f * bw);
                const float a = expf_ (-d1 * d1) + 0.7f * expf_ (-d2 * d2) + 0.05f / (float) k;
                const float sg = (k & 1) ? 1.0f : -1.0f;
                for (int n = 0; n < CUBE_SRC; ++n) w[n] += sg * a * sin2pi ((float) (k * n) / (float) CUBE_SRC);
            }
            break;
        }
        case 3:
        {
            // FM: a carrier on the note and a modulator on 1 .. 8 times it (X), its depth growing along Y
            const float r = (float) (x + 1), I = 0.3f + 0.85f * (float) y;
            for (int n = 0; n < CUBE_SRC; ++n)
            {
                const float t = (float) n / (float) CUBE_SRC;
                w[n] = sin2pi (t + I * sin2pi (r * t) * (1.0f / TWO_PI));
            }
            break;
        }
        case 4:
        {
            // FOLD: a sine folded over itself, harder along X (0.8 .. 7 times), lifted off its centre along Y (its
            // even harmonics)
            const float A = 0.8f + 0.9f * (float) x, b = 0.11f * (float) y;
            for (int n = 0; n < CUBE_SRC; ++n)
            {
                const float u = A * sin2pi ((float) n / (float) CUBE_SRC) + b;
                w[n] = sin2pi (u * 0.25f);                           // (a sine of it: a smooth fold)
            }
            break;
        }
        case 5:
        {
            // BITS: steps of a shift register's draws, 2 .. 32 of them a cycle along X, another register along Y
            static const int NS[8] = { 2, 3, 4, 6, 8, 12, 16, 32 };
            u32 s = 0xACE1u + 131u * (u32) y;
            float lv[32];
            for (int j = 0; j < NS[x]; ++j)
            {
                for (int b = 0; b < 3; ++b) { const u32 bit = ((s >> 0) ^ (s >> 2) ^ (s >> 3) ^ (s >> 5)) & 1u; s = (s >> 1) | (bit << 15); }
                lv[j] = (float) (s & 3u) * (2.0f / 3.0f) - 1.0f;
            }
            for (int n = 0; n < CUBE_SRC; ++n) w[n] = lv[n * NS[x] / CUBE_SRC];
            break;
        }
        case 6:
        {
            // GLASS: sparse high partials over a soft fundamental, the cluster higher along X (around the 3rd .. 24th
            // harmonic), longer and brighter along Y
            const float hc = 3.0f + 3.0f * (float) x;
            static const float SP[5] = { 1.0f, 1.5f, 2.2f, 3.1f, 4.3f };
            for (int n = 0; n < CUBE_SRC; ++n) w[n] = 0.5f * sin2pi ((float) n / (float) CUBE_SRC);
            for (int j = 0; j < 5; ++j)
            {
                const int k = (int) (hc * SP[j] + 0.5f);
                if (k < 2 || k > 120) continue;
                const float a = powf_ (0.6f, (float) j * (1.0f - fy * 0.7f)), ph = hashF (0x6A55u + 31u * (u32) j + 977u * (u32) x);
                for (int n = 0; n < CUBE_SRC; ++n) w[n] += a * sin2pi ((float) (k * n) / (float) CUBE_SRC + ph);
            }
            break;
        }
        default:
        {
            // DUST: a spectrum of random levels and phases, dark .. bright along X (its tilt), another draw along Y
            const float tilt = 1.8f - 0.22f * (float) x;
            for (int k = 1; k <= 120; ++k)
            {
                const u32 key = 0xD057u + 7919u * (u32) y + 104729u * (u32) k;
                const float a = powf_ ((float) k, -tilt) * (0.3f + 0.7f * hashF (key)), ph = hashF (key ^ 0x5bd1e995u);
                for (int n = 0; n < CUBE_SRC; ++n) w[n] += a * sin2pi ((float) (k * n) / (float) CUBE_SRC + ph);
            }
            break;
        }
    }
    // its peak at 0.95
    float pk = 0.0f;
    for (int n = 0; n < CUBE_SRC; ++n) pk = maxf (pk, absf (w[n]));
    if (pk > 1e-9f) for (int n = 0; n < CUBE_SRC; ++n) w[n] *= 0.95f / pk;
}

// the common amplitude envelope (as sk_engine.cpp's)
inline float ampTickC (Voice& v, float attInc, float coef)
{
    if (v.astage == 0)      { v.aenv += attInc; if (v.aenv >= 1.0f) { v.aenv = 1.0f; v.astage = 1; } }
    else if (v.astage == 1) { if (v.aHoldLeft > 0.0f) v.aHoldLeft -= 1.0f; else v.astage = 2; }
    else                    v.aenv *= v.hrel > 0.0f ? v.hrel : coef;
    return v.ecrv < 0.9995f ? envCurve (v.aenv, v.astage, v.ecrv) : v.aenv;
}

// the version (L) and its crossfade to the next (lm) for a pitch: its top harmonic under 0.3 x the rate (a version
// crossfaded with the next one up to 0.6: what folds back lands over 0.4 x the rate)
inline void mipFor (float hz, float sr, int& L, float& lm)
{
    const float l = log2f_ (maxf (128.0f * hz / (0.3f * sr), 1e-9f));
    if (l <= 0.0f) { L = 0; lm = 0.0f; return; }
    L = (int) l; lm = l - (float) L;
    if (L >= CUBE_MIPS - 1) { L = CUBE_MIPS - 1; lm = 0.0f; }
}

// WARP: the phase bent around its middle (bipolar: one half faster, the other slower); how much faster at most
inline float warpPh (float ph, float m) { return ph < m ? 0.5f * ph / m : 0.5f + 0.5f * (ph - m) / (1.0f - m); }

// the cube at (x, y, z) (in waves, 0 .. 7): the 8 waves around it, each read at phase ph in version L (and L + 1, lm);
// STEP's axes jump from a wave to the next
struct CubeSrc { const CubeTables* f; const CubeTables* u; const int* ready; };
inline const short* waveAt (const CubeSrc& s, int z, int y, int x, int L)
{
    const int i = (z * 8 + y) * 8 + x;
    return (s.u && s.ready[z] == 2 ? s.u->w[i][L] : s.f->w[i][L]);
}
inline float cubeRead (const CubeSrc& s, float x, float y, float z, int step, float ph, int L, float lm)
{
    auto axis = [] (float v, bool st, int& i, float& f) {
        v = clampf (v, 0.0f, 7.0f);
        if (st) { i = (int) (v + 0.5f); if (i > 7) i = 7; f = 0.0f; return; }
        i = (int) v; f = v - (float) i; if (i >= 7) { i = 7; f = 0.0f; } };
    int ix, iy, iz; float fx, fy, fz;
    axis (x, step >= 1, ix, fx); axis (y, step >= 2, iy, fy); axis (z, step >= 3, iz, fz);
    const float p = ph * (float) CUBE_LEN;
    int i0 = (int) p; const float fr = p - (float) i0; i0 &= CUBE_LEN - 1; const int i1 = (i0 + 1) & (CUBE_LEN - 1);
    const int L1 = L + 1 < CUBE_MIPS ? L + 1 : L;
    float acc = 0.0f;
    for (int dz = 0; dz < (fz > 0.0f ? 2 : 1); ++dz)
    {
        const float wz = dz ? fz : 1.0f - fz;
        for (int dy = 0; dy < (fy > 0.0f ? 2 : 1); ++dy)
        {
            const float wy = wz * (dy ? fy : 1.0f - fy);
            for (int dx = 0; dx < (fx > 0.0f ? 2 : 1); ++dx)
            {
                const float wgt = wy * (dx ? fx : 1.0f - fx);
                const short* a = waveAt (s, iz + dz, iy + dy, ix + dx, L);
                float v = (float) a[i0] + (float) (a[i1] - a[i0]) * fr;
                if (lm > 0.0f)
                {
                    const short* b = waveAt (s, iz + dz, iy + dy, ix + dx, L1);
                    const float vb = (float) b[i0] + (float) (b[i1] - b[i0]) * fr;
                    v += (vb - v) * lm;
                }
                acc += wgt * v;
            }
        }
    }
    return acc * (1.0f / 16384.0f);
}

} // namespace

void cubeFactory()
{
    if (factReady) return;
    RFFT<256> f256; RFFT<512> f512;
    f256.init (256); f512.init (512);
    float w[CUBE_SRC];
    for (int z = 0; z < CUBE_BANKS; ++z)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
            {
                factoryWave (z, x, y, w);
                makeMips (f256, f512, w, FACT.w[(z * 8 + y) * 8 + x]);
            }
    factReady = true;
}
const CubeTables& cubeFact() { return FACT; }

// ---- USER: a bank from the page (as loaded: 16-bit, up to 64 waves of 256 samples), its versions made a few waves at a
// time as the engine runs (cubeStep); until they are, the bank plays FACT's
void Engine::cubeSetBank (int bank, const short* data, int n)
{
    if (bank < 0 || bank >= CUBE_BANKS) return;
    n = n < 0 ? 0 : (n > CUBE_BANK_N ? CUBE_BANK_N : n);
    for (int i = 0; i < CUBE_BANK_N; ++i) cube.raw[bank][i] = i < n ? data[i] : 0;
    cube.len[bank] = n;
    cube.ready[bank] = n > 0 ? 1 : 0;
    if (n > 0 && cube.busy < 0) { cube.busy = bank; cube.wave = 0; }
    ++cube.ver;
}
void Engine::cubeClear (int bank)
{
    if (bank < 0 || bank >= CUBE_BANKS) return;
    cube.len[bank] = 0; cube.ready[bank] = 0;
    if (cube.busy == bank) { cube.busy = -1; for (int b = 0; b < CUBE_BANKS; ++b) if (cube.ready[b] == 1) { cube.busy = b; cube.wave = 0; break; } }
    ++cube.ver;
}
void Engine::cubeStep (int waves)
{
    if (cube.busy < 0) return;
    RFFT<256> f256; RFFT<512> f512;
    f256.init (256); f512.init (512);
    float w[CUBE_SRC];
    for (int k = 0; k < waves && cube.busy >= 0; ++k)
    {
        const int b = cube.busy, j = cube.wave;
        for (int n = 0; n < CUBE_SRC; ++n) w[n] = (float) cube.raw[b][j * CUBE_SRC + n] * (1.0f / 32768.0f);
        makeMips (f256, f512, w, cube.t.w[b * CUBE_BANK_W + j]);
        if (++cube.wave >= CUBE_BANK_W)
        {
            cube.ready[b] = 2; ++cube.ver;
            cube.busy = -1;
            for (int q = 0; q < CUBE_BANKS; ++q) if (cube.ready[q] == 1) { cube.busy = q; cube.wave = 0; break; }
        }
    }
}

// ---- the voice
// B's intervals (BRAT): -24 .. +24 semitones; UNIS: A doubled, up to 25 cents apart; the sweep (STIM) 5 ms .. 4 s; the
// pitch envelope (PENV) +-48 semitones, PTIM 2 ms .. 1 s; BITS OFF, 12, 10, 8, 6, 4
static const float CUBE_BITQ[CUBE_BITS] = { 0.0f, 2048.0f, 512.0f, 128.0f, 32.0f, 8.0f };
void Engine::cubeTrigger (Voice& v, const float* P)
{
    (void) P;
    CubeState& c = v.cb;
    c.pa = 0.0; c.pa2 = 0.37; c.pb = 0.0;
    c.swe = 1.0f; c.pe = 1.0f;
    c.fresh = 1;
    v.aenv = 1.0f; v.astage = 1;                 // (the attack: ATK, on the voice)
}

void Engine::cubeRender (Voice& v, const float* P, float tuneSt, int n, float* out)
{
    CubeState& c = v.cb;
    const CubeSrc src { &FACT, &cube.t, cube.ready };
    const CubeSrc fsrc { &FACT, nullptr, cube.ready };
    const CubeSrc& S = idxOf (P[P_S7], 2) == 1 ? src : fsrc;
    const float isr = 1.0f / sr;
    // the settings: where A is (X Y Z), FM, the sweep on X (and Y), B's interval and offset and level, UNIS, the envelopes
    const float X = P[P_A] * 7.0f, Y = P[P_B] * 7.0f, Z = P[P_C] * 7.0f;
    const float fmI = 8.0f * P[P_D] * P[P_D];     // (the index against B's frequency: up to 8)
    const float swX = (P[P_E] - 0.5f) * 2.0f * 7.0f, swY = (P[P_S8] - 0.5f) * 2.0f * 7.0f;
    const float swC = decayCoef (0.005f * powf_ (800.0f, P[P_S9]), sr);
    const float bSt = (float) (idxOf (P[P_S1], 49) - 24) + (P[P_S2] - 0.5f) * 1.0f;
    const float bX = (P[P_S3] - 0.5f) * 2.0f * 7.0f, bY = (P[P_S4] - 0.5f) * 2.0f * 7.0f, bL = P[P_S5];
    const float uni = P[P_S6], uR = semis (uni * 0.25f), uG = uni > 0.002f ? 0.7071f : 1.0f;
    const float peSt = (P[P_S10] - 0.5f) * 2.0f * 48.0f, peC = decayCoef (0.002f * powf_ (500.0f, P[P_S11]), sr);
    const int step = idxOf (P[P_S12], CUBE_STEPS);
    const float wp = (P[P_S13] - 0.5f) * 2.0f, wm = 0.5f - 0.45f * wp, wf = maxf (0.5f / wm, 0.5f / (1.0f - wm));
    const bool warp = absf (wp) > 0.002f;
    const float bq = CUBE_BITQ[idxOf (P[P_S14], CUBE_BITS)];
    const float T = 0.01f * powf_ (400.0f, P[P_DEC]);
    const float dc = decayCoef (T, sr), att = 1.0f / (0.0002f * sr);
    const float gOut = 0.9f / (1.0f + 0.6f * bL);
    // the pitch (C2 at TUNE 0), B's ratio; the versions for this chunk (A's for 70 % of the widest swing FM gives it:
    // brighter than its very top, what folds back still some 50 dB down; WARP's)
    const float base = 65.41f * semis (tuneSt + peSt * c.pe), rB = semis (bSt);
    int La, Lb; float lma, lmb;
    mipFor (base * (1.0f + 0.7f * fmI * rB) * (warp ? wf : 1.0f) * (uni > 0.002f ? uR : 1.0f), sr, La, lma);
    mipFor (base * rB * (warp ? wf : 1.0f), sr, Lb, lmb);
    const bool bOn = fmI > 0.0f || bL > 0.002f;
    float lvl = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        // (the pitch envelope per sample; the versions are the chunk's)
        const float f = peSt != 0.0f ? 65.41f * semis (tuneSt + peSt * c.pe) : base;
        const float ax = X + swX * c.swe, ay = Y + swY * c.swe;
        float b = 0.0f;
        if (bOn)
        {
            const float phB = warp ? warpPh ((float) c.pb, wm) : (float) c.pb;
            b = cubeRead (S, ax + bX, ay + bY, Z, step, phB, Lb, lmb);
            c.pb += (double) (f * rB * isr); c.pb -= __builtin_floor (c.pb);
        }
        // A: thru-zero FM (its increment goes through zero and back), UNIS its detuned copy
        const double incA = (double) ((f + fmI * f * rB * b) * isr);
        float a;
        {
            const float ph = warp ? warpPh ((float) c.pa, wm) : (float) c.pa;
            a = cubeRead (S, ax, ay, Z, step, ph, La, lma);
            c.pa += incA; c.pa -= __builtin_floor (c.pa);
        }
        if (uni > 0.002f)
        {
            const float ph = warp ? warpPh ((float) c.pa2, wm) : (float) c.pa2;
            a = (a + cubeRead (S, ax, ay, Z, step, ph, La, lma)) * uG;
            c.pa2 += incA * (double) uR; c.pa2 -= __builtin_floor (c.pa2);
        }
        float y = (a + bL * b) * gOut;
        if (bq > 0.0f) y = floorf_ (y * bq + 0.5f) / bq;
        const float e = ampTickC (v, att, dc);
        out[i] = y * e;
        lvl = maxf (lvl, e);
        c.swe *= swC; c.pe *= peC;
    }
    c.ax = clampf (X + swX * c.swe, 0.0f, 7.0f); c.ay = clampf (Y + swY * c.swe, 0.0f, 7.0f); c.az = clampf (Z, 0.0f, 7.0f);
    c.bx = clampf (c.ax + bX, 0.0f, 7.0f); c.by = clampf (c.ay + bY, 0.0f, 7.0f);
    c.fresh = 0;
    v.level = lvl;
    v.ns += (u32) n;
}

// ---- the screen: the track's voice (or its settings, silent): playing, where A and B are, the set, the banks loaded (a
// bit each) and being made, the changes' count; then A's wave there, 128 points
void Engine::cubeInfo (int ti, float* out)
{
    for (int k = 0; k < CUBE_INFO; ++k) out[k] = 0.0f;
    if (ti < 0 || ti >= NUM_TRACKS) return;
    const Track& t = tr[ti];
    const Voice& v = t.voice[t.cur];
    const bool on = v.active && v.machine == M_CUBE && v.level > 1e-3f;
    const float* P = t.p;
    float ax, ay, az, bx, by;
    if (on) { ax = v.cb.ax; ay = v.cb.ay; az = v.cb.az; bx = v.cb.bx; by = v.cb.by; }
    else
    {
        ax = P[P_A] * 7.0f; ay = P[P_B] * 7.0f; az = P[P_C] * 7.0f;
        bx = clampf (ax + (P[P_S3] - 0.5f) * 14.0f, 0.0f, 7.0f); by = clampf (ay + (P[P_S4] - 0.5f) * 14.0f, 0.0f, 7.0f);
    }
    const bool user = idxOf (P[P_S7], 2) == 1;
    int mask = 0; for (int b = 0; b < CUBE_BANKS; ++b) if (cube.ready[b] == 2) mask |= 1 << b;
    out[0] = on ? 1.0f : 0.0f; out[1] = ax / 7.0f; out[2] = ay / 7.0f; out[3] = az / 7.0f; out[4] = bx / 7.0f; out[5] = by / 7.0f;
    out[6] = user ? 1.0f : 0.0f; out[7] = (float) mask; out[8] = cube.busy >= 0 ? 1.0f : 0.0f; out[9] = (float) cube.ver;
    const CubeSrc S { &FACT, user ? &cube.t : nullptr, cube.ready };
    const int step = idxOf (P[P_S12], CUBE_STEPS);
    for (int j = 0; j < 128; ++j) out[10 + j] = cubeRead (S, ax, ay, az, step, (float) j / 128.0f, 0, 0.0f);
}

} // namespace sk
