// IMPAKT — shared drum-machine engine implementation.
// Machines are inspired by classic analog drum circuits (808/909), the FM / noise /
// impulse voices of modern Scandinavian grooveboxes, modal physical models, bytebeat,
// micro-looping and buffer-error glitches, Karplus-Strong strings, chaotic maps,
// formant filters and Casio-style phase distortion, plus four clean digital voices after Pan Sonic
// (pure sines), Ryoji Ikeda (binary data), wild FM with a separate sub, and industrial hardcore kicks; the HEX analog
// kit and the three wavetable machines live in sk_kit.cpp.
#include "sk_engine.h"
#if defined (__wasm_simd128__)
 #include <wasm_simd128.h>
#endif

namespace sk {

// ----------------------------------------------------------------- tables
static const float SPEEDS[NUM_SPEEDS] = { 0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };

static const float RATIOS[NUM_RATIOS] = {
    0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.41f, 1.5f, 1.73f, 2.0f, 2.5f, 2.73f, 3.0f,
    3.14f, 3.5f, 4.0f, 4.5f, 5.0f, 6.0f, 7.0f, 8.0f, 9.25f, 11.0f, 13.5f, 16.0f };

static const float LFO_STEPS[NUM_LFO_SPEEDS] = {
    128.0f, 64.0f, 32.0f, 16.0f, 12.0f, 8.0f, 6.0f, 4.0f, 3.0f, 2.0f, 1.5f, 1.0f,
    0.75f, 0.5f, 0.33333f, 0.25f };

// reference pitch of TUNE = 0 for every machine (the quantiser snaps absolute pitch); 0 = not pitched
static const float BASE_HZ[NUM_MACHINES] = {
    50.0f, 50.0f, 180.0f, 200.0f, 205.3f, 0.0f,          // BD·VA BD·FM SD·VA SD·FM HH·MT CP·NZ
    110.0f, 0.0f, 220.0f, 65.41f, 0.0f,                  // FM·X NOISE IMPLS SY·RAW BYTE
    261.63f, 110.0f, 0.0f, 110.0f, 110.0f,               // µLOOP BUG CHAOS VOX CZ·PD
    261.63f, 65.41f, 65.41f,                             // FRACT ITERA TRIO
    110.0f, 1760.0f, 110.0f, 50.0f,                      // SINUS DATA FM·Z HARD
    0.0f, 0.0f, 0.0f,                                    // HEX·BD SD TM (quantised inside, against their own pitch)
    130.81f,                                             // MACRO (C3 = MIDI 48 at TUNE 0)
    0.0f,                                                // RAM (a recording: its pitch is unknown)
    130.8128f,                                           // HALO (C3: the chord's root at TUNE 0; it quantises itself)
    0.0f,                                                // BREAK (TUNE is its speed)
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f,                        // the FX machines (no pitch)
    130.8128f,                                           // STRUM (C3, as HALO: it quantises itself)
    261.63f,                                             // CHOIR (C4: the alto's middle)
    130.8128f,                                           // ALLOY (C3: the object's note at TUNE 0)
    0.0f,                                                // CHIMERA (a sample: its pitch is its own; TUNE transposes it)
    0.0f,                                                // FUZZ (an FX machine)
    65.41f,                                              // CUBE (C2 at TUNE 0)
    130.81f,                                             // WEST (C3)
    0.0f,                                                // WEFT (v78: an FX machine)
    82.406889f };                                        // HOWL (v81: E2, the chord's root at TUNE 0)

// RAM: take lengths (steps), slices, the converter's age on its voices
static const double RAM_STEPS[RAM_TAKES] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64 };
static const int    RAM_SLICE_N[RAM_SLICES] = { 1, 2, 4, 8, 16 };
static const float  RAM_DAC_AGE = 0.4f;

// sequence looper lengths, in 16ths: stutters under a step, then 1 step .. 2 bars
static const double LOOP_LENS[] = { 0.125, 0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0, 32.0 };
enum { NUM_LOOP_LENS = 12 };


// ITERA loudness compensation, measured (K-weighted, loudest 100 ms) over the model / fold / harmonics /
// morph grid and interpolated, so turning a knob changes the colour, not the level
static const float ITERA_COMP[3][5][5][7] = {   // [MODL][FOLD 0..1 /4][HARM 0..1 /4][MRPH 0..1 /6], dB
    {
        { { -0.72f, 0.03f, 0.83f, 7.70f, 0.48f, 4.71f, -4.09f }, { 0.64f, 1.36f, 2.12f, 9.21f, 1.94f, 6.28f, -2.61f }, { 0.27f, 0.96f, 1.70f, 8.87f, 1.49f, 6.00f, -2.58f }, { 0.34f, 1.02f, 1.75f, 8.95f, 1.59f, 6.19f, -2.47f }, { 1.75f, 2.35f, 2.97f, 9.85f, 2.86f, 7.47f, -0.96f } },
        { { -0.68f, -0.09f, 0.61f, 7.10f, 0.09f, 3.90f, -3.41f }, { 1.47f, 2.08f, 2.75f, 9.12f, 1.97f, 5.90f, -1.68f }, { 1.17f, 1.69f, 2.26f, 8.38f, 1.18f, 5.33f, -0.48f }, { 1.08f, 1.62f, 2.23f, 8.37f, 1.23f, 5.48f, -0.82f }, { 2.70f, 3.22f, 3.76f, 9.32f, 2.57f, 6.80f, -0.02f } },
        { { 0.89f, 1.06f, 1.39f, 5.20f, 0.99f, 2.01f, 0.57f }, { 2.32f, 2.71f, 3.24f, 7.87f, 2.60f, 4.39f, 0.03f }, { 2.14f, 2.37f, 2.72f, 6.56f, 1.63f, 3.95f, 0.58f }, { 1.95f, 2.19f, 2.55f, 6.45f, 1.40f, 3.63f, 1.87f }, { 3.32f, 3.65f, 4.00f, 7.41f, 2.23f, 5.33f, 0.83f } },
        { { 2.88f, 2.79f, 2.71f, 3.33f, 3.17f, 3.82f, 5.42f }, { 5.74f, 5.76f, 5.76f, 6.61f, 5.67f, 6.16f, 7.06f }, { 4.64f, 4.92f, 4.94f, 4.78f, 4.22f, 4.73f, 6.65f }, { 3.54f, 3.72f, 3.90f, 3.96f, 3.25f, 3.67f, 2.79f }, { 3.99f, 4.08f, 4.25f, 3.92f, 2.70f, 3.83f, 3.00f } },
        { { -1.04f, -1.15f, -1.21f, -0.49f, -0.75f, -0.14f, 1.14f }, { 1.70f, 1.69f, 1.70f, 2.44f, 1.77f, 2.37f, 2.72f }, { 0.64f, 1.00f, 1.06f, 0.74f, 0.32f, 0.86f, 2.06f }, { -0.47f, -0.22f, -0.08f, -0.04f, -0.73f, -0.15f, -0.83f }, { -0.10f, 0.15f, 0.38f, -0.06f, -1.22f, -0.12f, -1.10f } }
    },
    {
        { { -2.45f, -1.69f, -0.87f, 6.69f, -1.16f, 3.90f, -5.51f }, { -0.96f, -0.20f, 0.63f, 8.31f, 0.35f, 5.54f, -4.00f }, { -1.15f, -0.42f, 0.37f, 7.97f, 0.07f, 5.20f, -3.90f }, { -0.90f, -0.19f, 0.59f, 8.10f, 0.30f, 5.46f, -3.63f }, { 0.56f, 1.20f, 1.88f, 9.06f, 1.64f, 6.80f, -2.09f } },
        { { -2.47f, -1.90f, -1.18f, 6.03f, -1.44f, 3.14f, -4.87f }, { -0.57f, 0.09f, 0.85f, 8.18f, 0.35f, 5.27f, -3.21f }, { -0.60f, -0.05f, 0.60f, 7.46f, -0.03f, 4.61f, -1.82f }, { -0.39f, 0.16f, 0.80f, 7.52f, -0.01f, 4.82f, -1.96f }, { 1.28f, 1.81f, 2.37f, 8.58f, 1.37f, 6.23f, -1.20f } },
        { { -0.42f, -0.35f, -0.12f, 4.05f, -0.36f, 1.26f, -0.67f }, { 0.75f, 0.97f, 1.40f, 6.82f, 1.15f, 3.98f, -1.01f }, { 0.71f, 0.84f, 1.08f, 5.58f, 0.77f, 3.08f, -0.39f }, { 0.71f, 0.88f, 1.18f, 5.54f, 0.54f, 2.96f, 0.86f }, { 1.83f, 2.15f, 2.52f, 6.61f, 1.05f, 4.57f, -0.17f } },
        { { 1.58f, 1.68f, 1.76f, 2.36f, 2.45f, 2.70f, 3.19f }, { 4.61f, 4.78f, 4.56f, 5.44f, 4.93f, 5.44f, 5.56f }, { 2.80f, 3.09f, 3.25f, 3.88f, 3.09f, 3.79f, 5.42f }, { 2.09f, 2.32f, 2.51f, 3.09f, 2.22f, 3.05f, 1.77f }, { 2.84f, 2.78f, 2.94f, 3.12f, 1.88f, 2.89f, 2.33f } },
        { { -2.30f, -2.19f, -2.11f, -1.49f, -1.46f, -1.16f, -0.76f }, { 0.58f, 0.82f, 0.69f, 1.38f, 1.00f, 1.52f, 1.49f }, { -1.06f, -0.84f, -0.67f, -0.17f, -0.81f, -0.08f, 1.05f }, { -1.90f, -1.67f, -1.51f, -0.95f, -1.71f, -0.80f, -1.84f }, { -1.26f, -1.14f, -0.96f, -0.94f, -2.12f, -1.07f, -1.90f } }
    },
    {
        { { -3.31f, -2.62f, -1.87f, 4.44f, -2.32f, 1.59f, -6.48f }, { -1.96f, -1.16f, -0.30f, 6.22f, -1.46f, 2.86f, -5.00f }, { -3.87f, -3.06f, -2.15f, 6.39f, -2.75f, 2.91f, -6.21f }, { -3.67f, -2.85f, -1.85f, 5.50f, -3.04f, 2.82f, -5.30f }, { -4.28f, -3.76f, -3.02f, 6.01f, -3.06f, 2.72f, -4.68f } },
        { { -3.30f, -2.74f, -2.06f, 3.89f, -2.60f, 0.89f, -5.98f }, { -1.95f, -1.14f, -0.28f, 6.23f, -1.34f, 2.54f, -4.88f }, { -3.27f, -2.84f, -2.17f, 6.12f, -2.64f, 2.50f, -3.76f }, { -3.61f, -2.94f, -2.10f, 5.11f, -3.19f, 2.38f, -4.23f }, { -3.36f, -2.90f, -2.22f, 6.34f, -2.35f, 2.64f, -4.25f } },
        { { -2.03f, -1.84f, -1.50f, 2.12f, -1.90f, -0.74f, -2.50f }, { -1.11f, -0.76f, -0.24f, 5.18f, -0.70f, 1.77f, -3.16f }, { -2.00f, -1.66f, -1.41f, 4.57f, -1.89f, 1.13f, -3.37f }, { -2.87f, -2.69f, -2.17f, 3.47f, -2.73f, 0.72f, -2.29f }, { -2.99f, -2.74f, -2.48f, 4.58f, -2.77f, 0.89f, -2.72f } },
        { { -0.21f, -0.30f, -0.38f, 0.26f, 0.08f, 0.74f, 2.26f }, { 2.57f, 2.46f, 2.12f, 3.40f, 2.41f, 3.74f, 3.59f }, { -0.42f, -0.66f, -0.66f, 1.81f, 0.69f, 1.23f, 2.63f }, { -0.85f, -0.93f, -1.15f, 0.59f, -1.29f, 0.15f, -0.15f }, { 2.24f, 2.26f, 1.92f, 0.67f, 1.21f, -0.32f, 0.47f } },
        { { -3.93f, -4.03f, -4.09f, -3.35f, -3.62f, -2.99f, -1.84f }, { -1.26f, -1.29f, -1.47f, -0.51f, -1.29f, -0.23f, -0.33f }, { -4.09f, -4.31f, -4.31f, -1.74f, -3.11f, -2.48f, -1.14f }, { -4.67f, -4.73f, -4.91f, -3.20f, -4.97f, -3.47f, -3.65f }, { -2.52f, -2.55f, -2.78f, -3.51f, -3.28f, -4.15f, -3.58f } }
    }
};

// SINUS ring modulator: ratio of the second sine to the first (0 = off)
enum { NUM_RING = 16 };
static const float RING_R[NUM_RING] = {
    0.0f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 2.76f, 3.0f, 3.5f, 4.2f, 5.4f, 6.0f, 7.1f, 8.0f, 11.0f, 16.0f };

static const int LFO_DEST[NUM_LFO_DEST] = {
    -1, P_TUNE, P_DEC, P_A, P_B, P_C, P_D, P_E, P_F,
    P_FREQ, P_RES, P_DRV, P_BITS, P_SRR, P_VOL, P_PAN, P_DLY, P_REV,
    P_FENV, P_IR, P_IMIX, P_IDCY, P_IPRE, P_ITON,        // slot 18 was the spectral send until v4
    P_RTRG, P_RCNT, P_RVEL,                              // the retrig (rate, count, velocity curve), since v5
    P_PMIX, P_PWAV, P_PWRP, P_PCTR, P_PWID, P_PSPD, P_PBLR, P_PTLT,     // PRISM, since v39 (projects v8)
    P_HPF, P_LPF, P_MIDF };                                             // TONE, since v42 (projects v10)


// v90: a route's destinations. The LFO's 38 first (a project made before keeps its routes, moved onto this grid of 256),
// then every other setting of the track but those it makes no sense to move: the machine, the choices a hit or the
// sequencer reads once (NOTE, PROB, MICR, GATE, the choke group, JUMP), the seeds, the RAM's recording, the routes
// themselves and the modulators' types
int MOD_DEST[MOD_DEST_SLOTS];
int NUM_MOD_DEST = 0;
void modDestInit()
{
    if (NUM_MOD_DEST) return;
    bool used[NUM_PARAMS] = {};
    int n = 0;
    for (int i = 0; i < NUM_LFO_DEST; ++i) { MOD_DEST[n++] = LFO_DEST[i]; if (LFO_DEST[i] >= 0) used[LFO_DEST[i]] = true; }
    static const int NO[] = { P_MACH, P_FTYP, P_NOTE, P_PROB, P_MICR, P_GATE, P_SPC, P_CHOK, P_JUMP, P_JRNG, P_HARM, P_DRON,
                              P_RSRC, P_RLEN, P_RLEV, P_RTHR, P_ROVD, P_RMON, P_RBUF, P_BSED, P_RDSD, P_L1SD, P_L2SD, P_RTSD, P_PQNT };
    for (int p : NO) used[p] = true;
    for (int m = 0; m < MOD_N; ++m)
    {
        used[MOD_TY[m]] = true;
        for (int r = 0; r < MOD_ROUTES; ++r) used[MOD_DST[m][r]] = used[MOD_DEP[m][r]] = true;
    }
    for (int p = 0; p < NUM_PARAMS && n < MOD_DEST_SLOTS; ++p) if (! used[p]) MOD_DEST[n++] = p;
    NUM_MOD_DEST = n;
    for (int i = n; i < MOD_DEST_SLOTS; ++i) MOD_DEST[i] = -1;
}

static const float RETRIG_DEN[NUM_RETRIG] = { 0, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256 };

static const float DLY_STEPS[NUM_DLY_TIMES] = {
    0.5f, 0.66667f, 1.0f, 1.33333f, 1.5f, 2.0f, 2.66667f, 3.0f, 4.0f, 6.0f, 8.0f };

static const float B808[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };

// modal partial tables: harmonic, membrane, bell, free bar
static float NOLOCK[NUM_PARAMS];
// LP fully open (top 6 % of FREQ) and HP fully open (bottom 5 %) leave the voice untouched (see renderVoice)
static const bool OPEN_BYPASS = true;
// the settings that glide when they are turned while the track sounds (Engine::setParam): one bit each
static const int SLEW_PS[32] = { P_TUNE, P_DEC, P_A, P_B, P_C, P_D, P_E, P_F, P_FREQ, P_RES, P_FENV, P_DRV, P_VOL, P_PAN,
    P_DLY, P_REV, P_LDEP, P_L2DP, P_IMIX, P_ITON, P_PNCH, P_DUCK, P_HPF, P_LPF, P_LOW, P_LOWF, P_MID, P_MIDF, P_HIGH,
    P_HIGF, P_E1DP, P_E2DP };
static signed char SLEW_BIT[NUM_PARAMS];
// RAND (GLITCH): the synthesis settings it may move, and how far at most (share of each knob's travel). Left
// alone: the level, pan, sends, TUNE and NOTE, the trig settings, the LFOs and followers, BOX, TONE, the choices
// of a machine (tables, models, algorithms, switches) and the whole RAM machine
static const int   RND_PS[12] = { P_DEC, P_A, P_B, P_C, P_D, P_E, P_F, P_FREQ, P_RES, P_FENV, P_FDEC, P_DRV };
static const float RND_SC[12] = { 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.6f, 0.4f, 0.7f, 0.6f, 0.5f };
// which of A..F each machine reads as a choice (a table, a model, a count, a switch): those never glide
static const unsigned char STEPPED_AF[NUM_MACHINES] = {
    0, 1, 0, 1, 0, 1, 7, 0, 1, 0, 31, 1, 32, 33, 0, 33, 3, 32, 35, 4, 17, 1, 0, 0, 0, 32, 1, 60, 35, 47,
    33, 35, 35, 33, 33, 51, 32, 33, 51, 33, 32, 56, 41, 39, 35, 33, 39 };  // (v78, WEFT: SRC, CARR, SIDE; v81, HOWL: CHRD, VOIC, DROP, SIDE; v83, STENCIL: SRC, ZONE, SIDE; v84, MANTRA: TCH, PAGE; v87, RADIO: RX, MODE, LEN, SIDE)
// RAND on a voice's settings: its offsets, scaled by the knob (a curve: gentle at first, wild at the top)
// WTF?! (v89; LIVE before it): the hit's draws (LIVE, LOOMND) or the LFO (LFO) on the synthesis settings, by the
// knob (its curve, the square root of its cube: gentle at first); n: the samples this chunk (0: the hit itself)
void Engine::wtfApply (Voice& v, float* P, int n)
{
    if (! v.rndOn) return;
    // (HIT, ONE: the voice's phase runs on, whatever the knob says)
    const double ph0 = v.wtfPh;
    if (n > 0 && wtfMode == WTF_LFO && wtfTrig != 0) v.wtfPh += wtfRate * (double) n;
    if (xrndS <= 0.0f) return;
    const float amt = xrndS, a = amt * sqrtf_ (amt);
    if (wtfMode != WTF_LFO)
    {
        for (int k = 0; k < 12; ++k) if (v.rnd[k] != 0.0f) P[RND_PS[k]] = clampf (P[RND_PS[k]] + v.rnd[k] * RND_SC[k] * a, 0.0f, 1.0f);
        return;
    }
    // the LFO: RUN, the panel's value this chunk; HIT, ONE: the voice's own phase from its hit (ONE: one cycle, then
    // where it ends)
    const bool own = wtfTrig != 0;
    const double ph = (wtfTrig == 2 && ph0 > 0.9999) ? 0.9999 : ph0;
    for (int k = 0; k < 12; ++k)
        if (v.rnd[k] != 0.0f)
            P[RND_PS[k]] = clampf (P[RND_PS[k]] + (own ? wtfAt (ph, k) : wtfVal[k]) * RND_SC[k] * a, 0.0f, 1.0f);
}
// the LFO's value for setting k at phase ph (cycles): each setting a share of a cycle later as SPRD opens (at 50 %
// half a cycle apart at most), then half of them turned upside down (at 100 % the odd ones mirror the even ones)
float Engine::wtfAt (double ph, int k) const
{
    static const float PH[12] = { 0.0f, 0.5f, 0.25f, 0.75f, 0.125f, 0.625f, 0.375f, 0.875f, 0.0625f, 0.5625f, 0.3125f, 0.8125f };
    const float sp = wtfSpr;
    const double x = ph + (double) (minf (sp * 2.0f, 1.0f) * PH[k]);
    const double c = __builtin_floor (x);
    const float f = (float) (x - c);
    // (the cycle's number for the draws of S&H and SLEW, on 16 bits: no 64-bit integers, which the plain-JS build lacks)
    const unsigned ci = (unsigned) (int) (c - 65536.0 * __builtin_floor (c * (1.0 / 65536.0)));
    float w;
    switch (wtfWave)
    {
        default:
        case 0: w = sin2pi (f); break;                                        // SIN
        case 1: w = f < 0.25f ? 4.0f * f : f < 0.75f ? 2.0f - 4.0f * f : 4.0f * f - 4.0f; break;   // TRI
        case 2: w = 2.0f * f - 1.0f; break;                                   // SAW up
        case 3: w = 1.0f - 2.0f * f; break;                                   // SAW down
        case 4: w = f < 0.5f ? 1.0f : -1.0f; break;                           // SQR
        case 5: w = loopDraw (g[G_WTFS], 40u + (unsigned) k, ci); break;   // S&H: a draw a cycle
        case 6:                                                               // SLEW: from one draw to the next
        {
            const float a = loopDraw (g[G_WTFS], 40u + (unsigned) k, ci);
            const float b = loopDraw (g[G_WTFS], 40u + (unsigned) k, (ci + 1u) & 65535u);
            const float s = f * f * (3.0f - 2.0f * f);
            w = a + (b - a) * s; break;
        }
        case 7: w = 2.0f * expf_ (-4.0f * f) - 1.0f; break;                  // EXP: up at once, falling away
    }
    const float flip = clampf ((sp - 0.5f) * 2.0f, 0.0f, 1.0f);
    return (k & 1) ? w * (1.0f - 2.0f * flip) : w;
}
// once a chunk: the mode and the LFO's settings, its rate, and its phase (on the tempo: from the song position while
// the transport runs; free, or stopped: counted on), each setting's value for RUN
void Engine::wtfTick (int n)
{
    wtfMode = idxOf (g[G_WTFM], WTF_MODES); wtfWave = idxOf (g[G_WTFW], WTF_WAVES); wtfTrig = idxOf (g[G_WTFT], WTF_TRIGS); wtfSpr = g[G_WTFP];
    const bool sync = g[G_WTFY] < 0.5f;
    const double spsx = (double) sr * 60.0 / (double) bpm / 4.0;          // samples a 16th
    wtfRate = sync ? 1.0 / ((double) WTF_SYNC[idxOf (g[G_WTFR], WTF_SYNCS)] * spsx)
                   : 0.05 * (double) exp2f_ (8.643856f * g[G_WTFR]) / (double) sr;     // 0.05 .. 20 Hz
    if (sync && running) wtfPh = (posAnchor + (now - clockAnchor) / spsx) / (double) WTF_SYNC[idxOf (g[G_WTFR], WTF_SYNCS)];
    else wtfPh += wtfRate * (double) n;
    if (wtfMode == WTF_LFO && wtfTrig == 0) for (int k = 0; k < 12; ++k) wtfVal[k] = wtfAt (wtfPh, k);
}

// RDEC (v65): the machines whose DEC is a decay (or a release: HALO, CHOIR), every one but the FX machines; at its ends
// a hit's DEC moves by up to a quarter of the knob's travel (a decay up to four times shorter or longer), never past
// the top where a machine holds or runs free (a decay is not drawn into HOLD, nor a held one out of it)
static inline bool decayMachine (int m) { return ! fxMachine (m); }
constexpr float RDEC_RANGE = 0.25f, RDEC_TOP = 0.98f;
static inline void rdecApply (const Voice& v, float* P)
{
    if (v.rdec == 0.0f || P[P_DEC] >= RDEC_TOP) return;
    P[P_DEC] = clampf (P[P_DEC] + v.rdec, 0.0f, RDEC_TOP);
}
// RTUN (v88): every machine but the FX machines (whose TUNE is not a pitch); at its ends a hit's TUNE moves by up to an
// octave either way (TUNE's 48 semitones over the knob), by the square of the knob: a few cents near its centre, a
// human drift, then whole notes; a quantised track lands on its scale (the scale is applied to TUNE + NOTE)
constexpr float RTUN_RANGE = 12.0f / 48.0f;
static inline void rtunApply (const Voice& v, float* P)
{
    if (v.rtun != 0.0f) P[P_TUNE] = clampf (P[P_TUNE] + v.rtun, 0.0f, 1.0f);
}

static inline double floord (double x) { return __builtin_floor (x); }
static inline double minf_d (double a, double b) { return a < b ? a : b; }
static inline double maxd_ (double a, double b) { return a > b ? a : b; }
static inline double absd (double x)   { return x < 0.0 ? -x : x; }
// triangle wavefolder (period 4, |y| <= 1, identity on [-1, 1]) and its antiderivative (ADAA)
static inline float triFold (float x)
{
    float t = x + 1.0f; t -= 4.0f * floorf_ (t * 0.25f);
    return t < 2.0f ? t - 1.0f : 3.0f - t;
}
static inline float triFoldInt (float x)
{
    float t = x + 1.0f; t -= 4.0f * floorf_ (t * 0.25f);
    return t < 2.0f ? 0.5f * (t - 1.0f) * (t - 1.0f) - 0.5f : 3.0f * t - 0.5f * t * t - 4.0f;
}

static void zeroBytes (void* p, __SIZE_TYPE__ n) { __builtin_memset (p, 0, n); }

static void defaultTrack (Track& t, int machine)
{
    t.slew = 0;
    for (int p = 0; p < NUM_PARAMS; ++p) t.p[p] = 0.5f;
    t.p[P_MACH] = machineVal (machine);
    t.p[P_DEC]  = 0.4f;
    t.p[P_FTYP] = 0.5f / (float) NUM_FTYPES;
    t.p[P_FREQ] = 1.0f;  t.p[P_RES] = 0.1f; t.p[P_FENV] = 0.5f; t.p[P_FDEC] = 0.3f;
    t.p[P_DRV]  = 0.0f;  t.p[P_BITS] = 0.0f; t.p[P_SRR] = 0.0f;
    t.p[P_VOL]  = 0.8f;  t.p[P_PAN] = 0.5f; t.p[P_DLY] = 0.0f; t.p[P_REV] = 0.0f;
    t.p[P_LSPD] = 7.5f / (float) NUM_LFO_SPEEDS; t.p[P_LDEP] = 0.5f;
    t.p[P_LDST] = 0.5f / (float) MOD_DEST_SLOTS; t.p[P_LWAV] = 0.5f / (float) NUM_LFO_WAVES;
    t.p[P_NOTE] = 0.5f;  t.p[P_VEL] = 100.0f / 127.0f; t.p[P_PROB] = 1.0f; t.p[P_MICR] = 0.5f;
    t.p[P_RTRG] = 0.5f / (float) NUM_RETRIG; t.p[P_RCNT] = 3.5f / (float) NUM_RCOUNT; t.p[P_RVEL] = 0.5f; t.p[P_GATE] = 0.0f;
    t.p[P_RCRV] = 0.5f; t.p[P_JUMP] = 0.5f; t.p[P_JRNG] = 11.5f / (float) NUM_JRANGE; t.jAcc = 0;
    t.p[P_SPC] = 0.0f; t.p[P_ATK] = 0.0f; t.p[P_CHOK] = 0.5f / (float) NUM_CHOKE; t.p[P_DUCK] = 0.0f;
    t.p[P_L2SP] = 5.5f / (float) NUM_LFO_SPEEDS; t.p[P_L2DP] = 0.5f; t.p[P_L2DS] = 0.5f / (float) MOD_DEST_SLOTS; t.p[P_L2WV] = 0.5f / (float) NUM_LFO_WAVES;
    // v65: RDEC off, SLW half the cycle, LOOP's 8 draws, the seeds at 0 (the page draws new ones)
    t.p[P_RDEC] = 0.5f; t.p[P_RDSD] = 0.0f;
    t.p[P_RTUN] = 0.5f; t.p[P_RTSD] = 0.0f;     // v88: RTUN off
    t.p[P_L1SL] = 0.5f; t.p[P_L1LN] = 6.5f / (float) NUM_LFO_LENS; t.p[P_L1SD] = 0.0f;
    t.p[P_L2SL] = 0.5f; t.p[P_L2LN] = 6.5f / (float) NUM_LFO_LENS; t.p[P_L2SD] = 0.0f;
    t.p[P_CHO] = 0.0f; t.p[P_PHS] = 0.0f;                       // v68: the CHORUS and PHASER sends, at 0
    t.p[P_IR] = 0.5f / (float) (NUM_IRS + 1); t.p[P_IMIX] = 0.5f; t.p[P_IPRE] = 0.0f; t.p[P_IDCY] = 1.0f;
    t.p[P_IGAT] = 1.0f; t.p[P_ITON] = 0.5f; t.p[P_IFB] = 0.0f; t.p[P_IREV] = 0.25f;
    t.p[P_PNCH] = 0.0f;
    // RAM, REC side: MAIN, 16 steps, 0 dB, no threshold, no overdub, no monitor, its own buffer
    t.p[P_RSRC] = 1.5f / (float) RAM_SRCS; t.p[P_RLEN] = 7.5f / (float) RAM_TAKES; t.p[P_RLEV] = 0.5f;
    t.p[P_RTHR] = 0.0f; t.p[P_ROVD] = 0.0f; t.p[P_RMON] = 0.0f; t.p[P_RBUF] = 0.5f / (float) (NUM_TRACKS + 1);
    // PRISM: off (MIX 0), 8 oscillators, a little blur and glide, the whole band (WDTH at 12 o'clock), the take at its
    // own speed; sines, no DYN, octave 0, no warp, no slices, flat tilt, quantiser off
    t.p[P_PMIX] = 0.0f; t.p[P_PVOI] = 7.5f / (float) PR_K; t.p[P_PBLR] = 0.2f; t.p[P_PGLD] = 0.2f;
    t.p[P_PCTR] = 0.5f; t.p[P_PWID] = 0.5f; t.p[P_PSPD] = 0.5f;
    t.p[P_PWAV] = 0.0f; t.p[P_PDYN] = 0.0f; t.p[P_POCT] = 0.5f; t.p[P_PWRP] = 0.5f; t.p[P_PSLC] = 0.5f / (float) PR_SLICES;
    t.p[P_PTLT] = 0.5f; t.p[P_PQNT] = 0.25f;
    // TONE: flat (HP and LP off, the three bands at 0 dB); envelope followers: on the track itself, 0 dB, the whole
    // band, follower, 5 ms attack, 150 ms decay, no depth, no destination
    t.p[P_HPF] = 0.0f; t.p[P_LPF] = 1.0f;
    // HALO, VOICE side: a 0.3 s swell, no glide, the quantiser's scale, root position, a little drift and air, half wide
    t.p[P_HSWL] = 0.55f; t.p[P_HGLD] = 0.0f; t.p[P_HMOD] = 0.5f / (float) HALO_MODES; t.p[P_HINV] = 0.0f;
    t.p[P_HDRF] = 0.3f; t.p[P_HAIR] = 0.15f; t.p[P_HWID] = 0.5f;
    // BREAK, CHOP side: no chops, free draws, no swap
    t.p[P_BRND] = 0.0f; t.p[P_BREV] = 0.0f; t.p[P_BSTU] = 0.0f; t.p[P_BPJM] = 0.0f; t.p[P_BSKP] = 0.0f;
    t.p[P_BSED] = 0.5f / (float) BRK_SEEDS; t.p[P_BSWP] = 0.0f;
    // the FX machines' MORE side (each machine starts with its own, from the page)
    t.p[P_XA] = 0.0f; t.p[P_XB] = 0.0f; t.p[P_XC] = 0.0f; t.p[P_XD] = 0.0f; t.p[P_XE] = 0.0f; t.p[P_XF] = 0.0f; t.p[P_XG] = 0.0f;
    // v59: HARM FREE (what a project of v58 played; the page starts a new track on PURE); HALO's TONE at rest
    t.p[P_HARM] = 0.5f / (float) NUM_HARM;
    t.p[P_HUNI] = 0.0f; t.p[P_HSUB] = 0.0f; t.p[P_HCUT] = 1.0f; t.p[P_HOPN] = 0.0f; t.p[P_HVIB] = 0.0f; t.p[P_HWRM] = 0.0f;
    // STRUM: a little humanised, an even strum, ringing a little; nylon, the pick past the middle, fairly bright, the
    // body in, no mute, no doubled courses, no stiffness
    t.p[P_SHUM] = 0.25f; t.p[P_SACC] = 0.5f; t.p[P_SRNG] = 0.35f; t.p[P_SMAT] = 0.5f / (float) STRUM_MATS; t.p[P_SPCK] = 0.55f;
    t.p[P_SDMP] = 0.55f; t.p[P_SBDY] = 0.6f; t.p[P_SMUT] = 0.0f; t.p[P_SDBL] = 0.0f; t.p[P_SSTF] = 0.0f;
    // CHOIR: a 0.2 s swell, the alto's tract, a middle effort, a 5.4 Hz vibrato, solo, a little scoop and drift; its
    // SPACE: a third in, a large hall, long, a little dark, gently moving, no shimmer, a short pre-delay
    t.p[P_CSWL] = 0.45f; t.p[P_CFRM] = 0.5f; t.p[P_CEFF] = 0.45f; t.p[P_CVRT] = 0.4f; t.p[P_CPRT] = 0.5f / (float) CHOIR_PARTN;
    t.p[P_CSCP] = 0.35f; t.p[P_CDRF] = 0.4f;
    t.p[P_CSPC] = 0.35f; t.p[P_CSIZ] = 0.6f; t.p[P_CDCY] = 0.6f; t.p[P_CDMP] = 0.35f; t.p[P_CMOD] = 0.4f;
    t.p[P_CSHM] = 0.5f / (float) CHOIR_SHIMS; t.p[P_CPRE] = 0.15f;
    // v60: AGE at rest (in tune, even, no sheen, no dropouts), STRUM's AIR off (what a project of v59 played); v61:
    // the side places at 0 (a track gets its machine's own from the page, or from machineSideDefaults: ALLOY's)
    t.p[P_GDTN] = 0.0f; t.p[P_GVAR] = 0.0f; t.p[P_GSHN] = 0.0f; t.p[P_GDRP] = 0.0f; t.p[P_SAIR] = 0.0f;
    for (int q = 0; q < NUM_SIDES; ++q) t.p[sideParam (q)] = 0.0f;
    t.p[P_ECRV] = 1.0f;                                         // v78: the amplitude envelope's curve at EXP (as it was)
    t.p[P_DRON] = 0.0f;                                         // v81: no drone
    // v64: no pan drawn at the hits, no spread (centred under 155 Hz when there is)
    t.p[P_RPAN] = 0.5f; t.p[P_SPRD] = 0.0f; t.p[P_SMON] = 0.5f;
    for (int e = 0; e < 2; ++e)
    {
        const int b = e ? P_E2SR : P_E1SR;
        t.p[b] = 0.5f / (float) EF_SRCS; t.p[b + 1] = 0.25f; t.p[b + 2] = 0.5f / (float) EF_BANDS; t.p[b + 3] = 0.0f;
        t.p[b + 4] = 0.4247f; t.p[b + 5] = 0.5088f; t.p[b + 6] = 0.5f; t.p[b + 7] = 0.5f / (float) MOD_DEST_SLOTS;
    }
    // v90: M1, M2 WAVE (the LFOs), M3, M4 FOLLOW (the followers), their new settings at rest (WAVE: no phase, SYNC), all of
    // their routes at full AMT, routes 2 .. 4 off
    for (int m = 0; m < MOD_N; ++m)
    {
        t.p[MOD_TY[m]] = ((m < 2 ? MT_WAVE : MT_FOLLOW) + 0.5f) / (float) MOD_TYPES;
        for (int k = m < 2 ? 5 : 6; k < MOD_SLOTS; ++k) t.p[MOD_SLOT[m][k]] = 0.0f;
        if (m < 2) t.p[MOD_SLOT[m][6]] = 0.25f;
        t.p[MOD_AM[m]] = 1.0f;
        for (int r = 1; r < MOD_ROUTES; ++r) { t.p[MOD_DST[m][r]] = 0.5f / (float) MOD_DEST_SLOTS; t.p[MOD_DEP[m][r]] = 0.5f; }
    }
    for (int s = 0; s < NUM_STEPS; ++s)
    {
        t.trig[s] = 0; t.rat[s] = 1; t.rec[s] = 0; t.tlk[s] = 0;
        for (int p = 0; p < NUM_PARAMS; ++p) t.lock[s][p] = -1.0f;
    }
    t.len = 16; t.spd = 4; t.mute = 0; t.q = 1;
}

// ----------------------------------------------------------------- setup
void Engine::init (float sampleRate, int liteMode)
{
    modDestInit();
    zeroBytes (this, sizeof (Engine));
    lite = liteMode ? 1 : 0;
    for (int p = 0; p < NUM_PARAMS; ++p) { NOLOCK[p] = -1.0f; SLEW_BIT[p] = -1; }
    for (int b = 0; b < 32; ++b) SLEW_BIT[SLEW_PS[b]] = (signed char) b;
    sr = sampleRate > 8000.0f ? sampleRate : 48000.0f;
    alloyTables (sr, alHiss);             // (ALLOY's modes, once; its noise's levels at this rate)
    fuzzTables();                         // (FUZZ's circuit, once)
    cubeFactory();                        // (CUBE's factory cube, once)
    cube.busy = -1;
    smoothC = 1.0f - expf_ (-1.0f / (0.0005f * sr)); smoothC4 = 1.0f - expf_ (-4.0f / (0.0005f * sr));
    {
        const float gg = tanpi (20.0f / sr);
        acA1 = 1.0f / (1.0f + gg * (gg + 1.41421356f)); acA2 = gg * acA1; acA3 = gg * acA2;
    }
    rng.seed (0x9E3779B9u);
    g[G_BPM] = 120.0f; g[G_SWING] = 0.0f;
    g[G_DTIM] = 5.5f / (float) NUM_DLY_TIMES; g[G_DFB] = 0.4f; g[G_DTON] = 0.4f; g[G_DPNG] = 0.6f;
    g[G_RSIZ] = 0.5f; g[G_RDMP] = 0.4f; g[G_MDRV] = 0.0f; g[G_MVOL] = 0.8f;
    g[G_RALG] = 1.5f / (float) NUM_REV_ALGOS; g[G_RTIM] = 0.6f; g[G_RPRE] = 0.0f;
    g[G_RCH1] = 0.5f; g[G_RCH2] = 0.5f; g[G_RLVL] = 0.6f;
    g[G_DACM] = 0.0f; g[G_DACA] = 0.35f;
    g[G_UMSK] = 0.0f; g[G_UKCK] = 0.5f; g[G_CALM] = 0.0f; g[G_CSHP] = 0.5f;
    g[G_CSEL] = 0.5f; g[G_CSPD] = 0.5f; g[G_CFOC] = 0.6f; g[G_CDLT] = 0.0f;
    g[G_STIM] = 0.6f; g[G_SBLR] = 0.3f; g[G_SWRP] = 0.5f; g[G_SREF] = 0.2f;
    g[G_SATM] = 0.55f; g[G_SFRZ] = 0.0f; g[G_SREV] = 0.0f; g[G_SLVL] = 0.6f;
    // master: everything neutral (EQ flat, compressor and drive off)
    g[G_EQLF] = 2.5f / 4.0f; g[G_EQLB] = 0.0f; g[G_EQLA] = 0.0f; g[G_EQHF] = 3.5f / 7.0f;
    g[G_EQHB] = 0.0f; g[G_EQBW] = 0.5f; g[G_EQHA] = 0.0f; g[G_EQHS] = 1.5f / 3.0f;
    g[G_CINP] = 0.33f; g[G_CATK] = 0.4f; g[G_CREL] = 0.4f; g[G_CRAT] = 0.5f / (float) NUM_COMP_RATIOS;
    g[G_CDST] = 0.5f / 3.0f; g[G_CDET] = 0.5f / 4.0f; g[G_CMIX] = 1.0f; g[G_COUT] = 0.5f;
    g[G_DMOD] = 0.5f / (float) NUM_DRIVE_MODELS; g[G_DDRV] = 0.2f; g[G_DBIA] = 0.5f; g[G_DTNE] = 0.5f;
    g[G_DCHR] = 0.5f; g[G_DMIX] = 1.0f; g[G_DOUT] = 0.5f; g[G_DPOS] = 0.75f;
    bpm = 120.0f;
    // patterns: A01 plays, nothing queued, no chain, every slot empty (zeroBytes)
    curPat = editPat = 0; queued = -1; qMode = 0; curChg = 0; chainLen = 0; chainPos = -1; patStart = 0.0;
    const int kit[NUM_TRACKS] = { M_BD_VA, M_SD_FM, M_HH_MT, M_CP_NZ, M_FM_X, M_IMPLS, M_ALLOY, M_SY_RAW };
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        defaultTrack (tr[t], kit[t]);
        tr[t].rng.seed (1234u + (u32) t * 7919u);
        tr[t].voice[0].rng.seed (777u + (u32) t * 131u);
        tr[t].voice[1].rng.seed (999u + (u32) t * 257u);
        tr[t].curStep = -1;
        for (int k = 0; k < 2; ++k) for (int p = 0; p < NUM_PARAMS; ++p) tr[t].voice[k].lk[p] = -1.0f;
    }
    fxInit();
    org.build();
    brk.reset();                 // (BREAK's loops come from the host: breakSet)
    radio.reset();               // (RADIO's receivers too: radioPush)
    for (int t = 0; t < NUM_TRACKS; ++t) { radLast[t] = -1; radLen[t][0] = radLen[t][1] = 0; }
    for (int t = 0; t < NUM_TRACKS; ++t) fxReset (t);
    fxTaken = 0;
    fftA.init (2 * BOX_NA);
    fftB.init (2 * BOX_NB);
    if (! lite) bank.build (sr, fftA, fftB);
    boxInit();
    spec.init (sr);
    specTrack = -1; specInit();
    master.init (sr, lite);
    um.init (sr);
    calm.init (sr);
    scaleMask = 0;
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        ram[t].rate = sr;
        for (int k = 0; k < 2; ++k) tr[t].voice[k].dac.init (sr);
    }
    // PRISM analysis: periodic Hann window
    prFft.init (PR_WIN);
    for (int i = 0; i < PR_WIN; ++i) prWin[i] = 0.5f - 0.5f * cos2pi ((float) i / (float) PR_WIN);
    prRR = 0;
    if (! lite) prismTables();
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        tr[t].mac = macMem[t];
        macroInit (tr[t].mac, sr);
        for (int k = 0; k < 2; ++k) tr[t].voice[k].mac = tr[t].mac;
    }
    // (v67: DICE is gone; its places in the globals stay, at 0, so the projects keep their layout)
    for (int k = G_DON; k <= G_DPW; ++k) g[k] = 0.0f;
    // GRAIN (off): grains of ~0.25 s from the last half second, regular and fairly dense, a little randomness;
    // its LFO: a sine over one bar, every attenuverter at 0
    grainInit (grainMem, sr);
    g[G_GTIM] = 0.08f; g[G_GSIZ] = 0.7f; g[G_GSHP] = 0.5f; g[G_GPIT] = 0.5f; g[G_GDEN] = 0.2f;
    g[G_GFDB] = 0.3f; g[G_GVRB] = 0.55f; g[G_GRND] = 0.3f; g[G_GMED] = 0.5f / (float) GRAIN_MEDIA;
    g[G_GSYN] = 0.0f; g[G_GFRZ] = 0.0f;
    g[G_GON] = 0.0f; g[G_GMIX] = 0.35f; g[G_GLRT] = 10.5f / (float) NUM_GLFO_RATES; g[G_GLWV] = 0.5f / (float) WLFO_WAVES;
    g[G_GLPH] = 0.5f / 16.0f;
    for (int k = G_GA0; k <= G_GA_END; ++k) g[k] = 0.5f;
    glfoPh = 0.0; glfoVal = 0.0f; postWas = 0;
    // the chain's ON keys: EQ, COMP, DRV on (they are neutral at their defaults), OTT off with its defaults
    g[G_EQON] = g[G_COON] = g[G_DRON] = 1.0f;
    ott.init (sr);
    g[G_OON] = 0.0f; g[G_OTIM] = 1.0f; g[G_OUP] = 1.0f; g[G_ODN] = 1.0f; g[G_OLO] = g[G_OMD] = g[G_OHI] = 0.5f;
    g[G_XON] = 0.0f; g[G_XRND] = 0.0f; g[G_XTIM] = 2.5f / (float) GL_GRIDS; g[G_XRPT] = g[G_XBND] = g[G_XBRK] = 0.0f;
    g[G_XCRP] = 0.0f; g[G_XLOS] = 0.0f; g[G_XMIX] = 1.0f; g[G_XCMD] = 0.5f / (float) GL_CMODES; g[G_XLMD] = 0.5f / (float) GL_LMODES;
    g[G_XSPD] = 0.5f; g[G_XHLD] = 0.0f; g[G_XMSK] = 1.0f;
    g[G_XRES] = 0.35f; g[G_XANA] = 0.0f; g[G_XAMD] = 0.5f / (float) GL_AMODES;
    g[G_QSRC] = 0.0f;
    glitch.init (sr); xrng.seed (0x51C0A7u); xrndS = 0.0f;
    // v89: WTF?! in LIVE (as it was), LOOMND's loop 16 steps, its seed 0; the LFO a sine of a bar, on the tempo, running,
    // its settings half apart
    g[G_WTFM] = 0.5f / WTF_MODES; g[G_WTFL] = 9.5f / WTF_LENS; g[G_WTFS] = 0.0f; g[G_WTFW] = 0.5f / WTF_WAVES;
    g[G_WTFR] = 7.5f / WTF_SYNCS; g[G_WTFY] = 0.25f; g[G_WTFT] = 0.5f / WTF_TRIGS; g[G_WTFP] = 0.5f;
    wtfPh = 0.0; wtfRate = 0.0; for (float& w : wtfVal) w = 0.0f; wtfMode = 0; wtfWave = 0; wtfTrig = 0; wtfSpr = 0.5f;
    g[G_OOUT] = 0.5f; g[G_OMIX] = 1.0f;
    g[G_RVON] = g[G_DLON] = g[G_DCON] = g[G_MXON] = 1.0f;
    // v68: the CHORUS on mode 2, its input in stereo, its level at 80 %, on; the PHASER's generators 0.6 and 1 Hz sines,
    // A and B 70 % deep with 30 % of feedback, B on generator 1 against A (REV), in stereo, its level at 80 %, on; the
    // DELAY on STD, the 1982's bandwidth x1, its VCO still (a 0.55 Hz sine, no envelope), HOLD off
    g[G_CHMD] = 1.5f / (float) CHO_MODES; g[G_CHIN] = 0.75f; g[G_CHLV] = 0.8f; g[G_CHON] = 1.0f;
    g[G_PRT1] = 0.35f; g[G_PSH1] = 0.0f; g[G_PRT2] = 0.45f; g[G_PSH2] = 0.0f;
    g[G_PDPA] = 0.7f; g[G_PFBA] = 0.3f; g[G_PDPB] = 0.7f; g[G_PFBB] = 0.3f;
    g[G_PBSW] = 0.25f; g[G_PSYN] = 0.75f; g[G_PRTE] = 0.5f / (float) PHS_ROUTES; g[G_PLVL] = 0.8f; g[G_PHON] = 1.0f;
    g[G_DMDL] = 0.25f; g[G_DBW] = 0.25f; g[G_DVDP] = 0.0f; g[G_DVRT] = 0.4f; g[G_DVSH] = 0.0f; g[G_DVEN] = 0.5f; g[G_DHLD] = 0.0f;
    g[G_LOOP] = 0.0f; g[G_LLEN] = (6.0f + 0.5f) / (float) NUM_LOOP_LENS; g[G_LPOS] = 0.5f;
    loopOn = 0; loopBase = loopAnchor = 0.0; loopLen = 4.0;
}

void Engine::resetPattern()
{
    for (int t = 0; t < NUM_TRACKS; ++t)
        for (int s = 0; s < NUM_STEPS; ++s)
        {
            tr[t].trig[s] = 0; tr[t].rat[s] = 1; tr[t].rec[s] = 0; tr[t].tlk[s] = 0;
            for (int p = 0; p < NUM_PARAMS; ++p) tr[t].lock[s][p] = -1.0f;
        }
    for (int t = 0; t < NUM_TRACKS; ++t) tr[t].chordPc = 0;
}

// the quantiser's notes now: the chord of the HALO or STRUM track it follows (G_QSRC), once that one has played a
// chord; else its scale
u32 Engine::quantMask() const
{
    const int src = idxOf (g[G_QSRC], NUM_TRACKS + 1);
    if (src > 0)
    {
        const Track& s = tr[src - 1];
        const int m = machineOf (s.p[P_MACH]);
        if ((m == M_HALO || m == M_STRUM || m == M_HOWL) && (s.chordPc & 0xFFFu)) return s.chordPc & 0xFFFu;
    }
    return scaleMask & 0xFFFu;
}
u32 Engine::harmMask (int ti) const
{
    return ti == idxOf (g[G_QSRC], NUM_TRACKS + 1) - 1 ? scaleMask & 0xFFFu : quantMask();
}
int Engine::harmRoot (int ti) const
{
    const int src = idxOf (g[G_QSRC], NUM_TRACKS + 1) - 1;
    if (src < 0 || src == ti) return -1;
    const Track& s = tr[src];
    const int m = machineOf (s.p[P_MACH]);
    return (m == M_HALO || m == M_STRUM || m == M_HOWL) && (s.chordPc & 0xFFFu) ? s.chordRoot : -1;
}

void Engine::setSampleRate (float sampleRate)
{
    if (sampleRate < 8000.0f) return;
    if (absf (sampleRate - sr) < 0.5f) return;
    sr = sampleRate;
    panic();
    for (int t = 0; t < NUM_TRACKS; ++t) macroSetRate (tr[t].mac, sr);
    grainSetRate (grainMem, sr);
    ott.init (sr);
    glitch.init (sr);
    for (int t = 0; t < NUM_TRACKS; ++t) fxReset (t);
    fxInit();
    if (! lite) bank.build (sr, fftA, fftB);
    boxInit();
    spec.init (sr);
    specTrack = -1; specInit();
    master.init (sr, lite);
    um.init (sr);
    calm.init (sr);
    // RAM: the buffers keep their own rate (playback follows it); a take in progress stops where it is
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        if (ram[t].state != RAM_IDLE) { ram[t].state = RAM_IDLE; ram[t].ver++; }
        for (int k = 0; k < 2; ++k) tr[t].voice[k].dac.init (sr);
    }
}

void Engine::panic()
{
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        tr[t].voice[0].active = tr[t].voice[1].active = 0;
        tr[t].rtLeft = 0;
    }
}

void Engine::fxInit()
{
    glitch.reset(); xrndS = 0.0f; wtfPh = 0.0;
    for (int i = 0; i < DLY_SIZE; ++i) dl[i] = dr[i] = 0.0f;
    dw = 0; dLpL = dLpR = dHpL = dHpR = 0.0f;
    dlyOnF = 1.0f; dlyOff = 0; dlyClr = DLY_SIZE;
    dTime = DLY_STEPS[idxOf (g[G_DTIM], NUM_DLY_TIMES)] * sr * 60.0f / bpm / 4.0f;
    dlyModel = idxOf (g[G_DMDL], 2); e82.clear();
    cho.init (sr); phs.init (sr); choOnF = phsOnF = 1.0f;

    Reverb& r = rv;
    r.sr = sr;
    const float s = sr / 29761.0f;
    r.pre.clear(); r.pre.len = mini ((int) (0.012f * sr) + 1, 4095);
    r.ap1.d.clear(); r.ap1.d.len = mini ((int) (142 * s) + 1, 1023); r.ap1.g = 0.75f;
    r.ap2.d.clear(); r.ap2.d.len = mini ((int) (107 * s) + 1, 1023); r.ap2.g = 0.75f;
    r.ap3.d.clear(); r.ap3.d.len = mini ((int) (379 * s) + 1, 4095); r.ap3.g = 0.625f;
    r.ap4.d.clear(); r.ap4.d.len = mini ((int) (277 * s) + 1, 2047); r.ap4.g = 0.625f;
    r.mapL.clear(); r.mapR.clear(); r.d1L.clear(); r.d2L.clear(); r.d1R.clear(); r.d2R.clear();
    r.apL.d.clear(); r.apR.d.clear();
    r.size = -1.0f;
    plateSize (g[G_RSIZ]);
    r.bwState = r.dampL = r.dampR = r.fbL = r.fbR = 0.0f;
    r.lfoPh = 0.0f;

    for (int i = 0; i < PRE_LEN; ++i) preBuf[i] = 0.0f;
    preW = 0; preLen = 0.0f;
    fdn.init (sr);
    spring.init (sr);
    revCur = -1; revIdle = 0; revFade = 1.0f;
    revOnF = 1.0f; revOff = 0;
}

// plate tank dimensions: SIZE scales every line of the tank (0.5x .. 1.5x of Dattorro's figure-of-eight)
void Engine::plateSize (float size)
{
    Reverb& r = rv;
    if (absf (size - r.size) < 0.004f) return;
    r.size = size;
    const float s = sr / 29761.0f * (0.5f + size);
    r.scale = s;
    r.mapLenL = 672.0f * s; r.mapLenR = 908.0f * s;
    r.mapL.len = mini ((int) ((672 + 40) * s) + 4, 8191);
    r.mapR.len = mini ((int) ((908 + 40) * s) + 4, 8191);
    r.d1L.len = mini ((int) (4453 * s) + 1, 32767);
    r.d2L.len = mini ((int) (3720 * s) + 1, 32767);
    r.d1R.len = mini ((int) (4217 * s) + 1, 32767);
    r.d2R.len = mini ((int) (3163 * s) + 1, 32767);
    r.apL.d.len = mini ((int) (1800 * s) + 1, 16383);
    r.apR.d.len = mini ((int) (2656 * s) + 1, 32767);
    DLine<8192>* m[2] = { &r.mapL, &r.mapR };
    for (auto* d : m) if (d->w >= d->len) d->w = 0;
    DLine<32768>* l[5] = { &r.d1L, &r.d2L, &r.d1R, &r.d2R, &r.apR.d };
    for (auto* d : l) if (d->w >= d->len) d->w = 0;
    if (r.apL.d.w >= r.apL.d.len) r.apL.d.w = 0;
}

// ----------------------------------------------------------------- edits
// (each goes to the pattern edited: the tracks when it is the one playing, else its slot: sk_pat.cpp)
int Engine::slewBit (int p) { return p >= 0 && p < NUM_PARAMS ? SLEW_BIT[p] : -1; }
void Engine::setParam (int t, int p, float v)
{
    if (t < 0 || t >= NUM_TRACKS || p < 0 || p >= NUM_PARAMS) return;
    if (PatSlot* sl = patTarget (t)) { sl->trk[t].p[p] = clampf (v, 0.0f, 1.0f); return; }
    setParamLive (t, p, v);
}
void Engine::setParamLive (int t, int p, float v)
{
    if (t < 0 || t >= NUM_TRACKS || p < 0 || p >= NUM_PARAMS) return;
    v = clampf (v, 0.0f, 1.0f);
    Track& k = tr[t];
    // v80: another machine on the track: a note held (HOLD until the next trig, a pad, a drone) lets go, as on a stop
    if (p == P_MACH && machineOf (v) != machineOf (k.p[P_MACH]))
        for (int i = 0; i < 2; ++i)
        {
            Voice& o = k.voice[i];
            if (o.active && o.machine != machineOf (v) && o.drone) droneEnd (o);
            else if (o.active && o.machine != machineOf (v) && (o.sus || o.padHeld))
            {
                o.padHeld = 0; letGo (o);
                if (o.machine == M_HOWL) howlLetGo (o, 0.25f);            // (HOWL's strings, its loop: the hand on them)
                if (o.machine == M_MANTRA) mantraLetGo (o);              // (MANTRA: the fingers off its sensors)
            }
        }
    const int b = SLEW_BIT[p];
    // a knob turned while the track sounds glides there (about 12 ms, renderChunk) instead of jumping in steps of
    // a UI message; silent, or a choice (a table, a model, a switch), it takes the value at once
    if (b >= 0)
    {
        const bool stepped = p >= P_A && p <= P_F && ((STEPPED_AF[machineOf (k.p[P_MACH])] >> (p - P_A)) & 1);
        if (! stepped && (k.voice[0].active || k.voice[1].active)) { k.pt[p] = v; k.slew |= 1u << b; return; }
        k.slew &= ~(1u << b);
    }
    k.p[p] = v;
}
void Engine::setTrig (int t, int s, int on)
{
    if (t < 0 || t >= NUM_TRACKS || s < 0 || s >= NUM_STEPS) return;
    if (PatSlot* sl = patTarget (t)) { sl->trk[t].trig[s] = on ? 1 : 0; return; }
    tr[t].trig[s] = on ? 1 : 0;
}
void Engine::setTrigless (int t, int s, int on)
{
    if (t < 0 || t >= NUM_TRACKS || s < 0 || s >= NUM_STEPS) return;
    if (PatSlot* sl = patTarget (t)) { sl->trk[t].tlk[s] = on ? 1 : 0; return; }
    tr[t].tlk[s] = on ? 1 : 0;
}
void Engine::setRec (int t, int s, int on)
{
    if (t < 0 || t >= NUM_TRACKS || s < 0 || s >= NUM_STEPS) return;
    if (PatSlot* sl = patTarget (t)) { sl->trk[t].rec[s] = on ? 1 : 0; return; }
    tr[t].rec[s] = on ? 1 : 0;
}
void Engine::setInput (const float* l, const float* r) { inL = l; inR = r ? r : l; }
void Engine::setLock (int t, int s, int p, float v)
{
    if (t < 0 || t >= NUM_TRACKS || s < 0 || s >= NUM_STEPS || p < 0 || p >= NUM_PARAMS) return;
    if (PatSlot* sl = patTarget (t)) { slotLock (*sl, t, s, p, v); return; }
    tr[t].lock[s][p] = v < 0.0f ? -1.0f : clampf (v, 0.0f, 1.0f);
}
void Engine::clearLocks (int t, int s)
{
    if (t < 0 || t >= NUM_TRACKS || s < 0 || s >= NUM_STEPS) return;
    if (PatSlot* sl = patTarget (t))
    {
        for (int i = 0; i < sl->nlk; ) { if (sl->lk[i].t == t && sl->lk[i].s == s) sl->lk[i] = sl->lk[--sl->nlk]; else ++i; }
        return;
    }
    for (int p = 0; p < NUM_PARAMS; ++p) tr[t].lock[s][p] = -1.0f;
}
void Engine::setRatchet (int t, int s, int n)
{
    if (t < 0 || t >= NUM_TRACKS || s < 0 || s >= NUM_STEPS) return;
    if (PatSlot* sl = patTarget (t)) { sl->trk[t].rat[s] = (u8) mini (maxi (n, 1), 16); return; }
    tr[t].rat[s] = (u8) mini (maxi (n, 1), 16);
}
void Engine::setLen (int t, int l)
{
    if (t < 0 || t >= NUM_TRACKS) return;
    if (PatSlot* sl = patTarget (t)) { sl->trk[t].len = (u8) mini (maxi (l, 1), NUM_STEPS); return; }
    tr[t].len = mini (maxi (l, 1), NUM_STEPS);
}
void Engine::setSpeed (int t, int i)
{
    if (t < 0 || t >= NUM_TRACKS) return;
    if (PatSlot* sl = patTarget (t)) { sl->trk[t].spd = (u8) mini (maxi (i, 0), NUM_SPEEDS - 1); return; }
    tr[t].spd = mini (maxi (i, 0), NUM_SPEEDS - 1);
}
void Engine::setQuant (int t, int on)
{
    if (t < 0 || t >= NUM_TRACKS) return;
    if (PatSlot* sl = patTarget (t)) { sl->trk[t].q = on ? 1 : 0; return; }
    tr[t].q = on ? 1 : 0;
}
void Engine::setMute (int t, int m)  { if (t >= 0 && t < NUM_TRACKS) { tr[t].mute = m ? 1 : 0; if (m) tr[t].rtLeft = 0; } }
void Engine::setGlobal (int gi, float v)
{
    if (gi < 0 || gi >= NUM_GLOBALS) return;
    if (gi == G_BPM) g[gi] = clampf (v, 20.0f, 400.0f);
    else g[gi] = clampf (v, 0.0f, 1.0f);
}
void Engine::setPlay (int on)
{
    if (on && ! internalPlay) pos16 = 0.0;
    internalPlay = on ? 1 : 0;
    if (! on) for (int t = 0; t < NUM_TRACKS; ++t) tr[t].rtLeft = 0;
}
void Engine::audition (int t, float vel, float note)
{
    if (t < 0 || t >= NUM_TRACKS) return;
    Track& tk = tr[t];
    float v = lock (tk, -1, P_VEL) * clampf (vel, 0.0f, 1.0f);
    float hold = lock (tk, -1, P_GATE) * 8.0f * (float) (samplesPerStep (tk) / sr);
    padHit = 1;                     // (startVoice: a pad or a key holds the note while it is pressed, DEC on HOLD)
    startVoice (tk, t, NOLOCK, -1, v, note, hold);
    padHit = 0;
    tk.hitCount++;
}

// a pad or a key let go: the note it holds ends (DEC on HOLD: in HOLD_REL; HALO lets its chord go; MACRO's gate falls)
void Engine::release (int t, float note)
{
    if (t < 0 || t >= NUM_TRACKS) return;
    for (int k = 0; k < 2; ++k)
    {
        Voice& v = tr[t].voice[k];
        // (HOWL on POLY: the key's own string, whatever note came after it)
        if (v.active && v.padHeld && v.machine == M_HOWL && v.hw.type == HOWL_POLY_T) { howlRelease (v, note); if (! v.hw.held || v.hw.holdLeft >= 0.0f) v.padHeld = 0; continue; }
        if (! v.active || ! v.padHeld || absf (v.note - note) > 0.5f) continue;
        v.padHeld = 0;
        if (v.machine == M_HOWL) { if (v.hw.held) v.hw.holdLeft = 0.5f; continue; }
        if (v.machine == M_MANTRA) { mantraRelease (v); continue; }
        if (v.machine == M_HALO) { if (v.hs.held) v.hs.holdLeft = 0.5f; continue; }
        if (v.machine == M_STRUM) { if (v.ss.held) v.ss.holdLeft = 0.5f; continue; }
        if (v.machine == M_CHOIR) { if (v.cs.held && ! v.cs.rib) v.cs.holdLeft = 0.5f; continue; }
        letGo (v);
    }
}

// the ribbon: touched, it plays the track like a pad held at its pitch (CHOIR follows it from then on: the pitch
// along it, the expression up it, until it is let go); on another machine, a pad at the nearest semitone
void Engine::ribbon (int t, float semis, int gate, float expr)
{
    if (t < 0 || t >= NUM_TRACKS) return;
    Track& tk = tr[t];
    expr = clampf (expr, 0.0f, 1.0f);
    semis = clampf (semis, -48.0f, 48.0f);
    if (gate && ! tk.ribOn)
    {
        tk.ribOn = 1; tk.ribSemis = semis; tk.ribExpr = expr;
        const bool choir = machineOf (lock (tk, -1, P_MACH)) == M_CHOIR;
        const float note = choir ? semis : floorf_ (semis + 0.5f);
        padHit = 1;
        startVoice (tk, t, NOLOCK, -1, lock (tk, -1, P_VEL) * (choir ? 0.55f + 0.45f * expr : 1.0f), note, 0.0f);
        padHit = 0;
        tk.hitCount++;
        Voice& v = tk.voice[tk.cur];
        if (v.machine == M_CHOIR) { v.cs.rib = 1; v.cs.ribT = semis; v.cs.expr = expr; v.cs.ribSnap = gate == 2; v.cs.ribQ = -999.0f; }
        return;
    }
    if (! tk.ribOn) return;
    if (gate)
    {
        tk.ribSemis = semis; tk.ribExpr = expr;
        for (int k = 0; k < 2; ++k)
        {
            Voice& v = tk.voice[k];
            if (v.active && v.machine == M_CHOIR && v.cs.rib) { v.cs.ribT = semis; v.cs.expr = expr; v.note = semis; v.cs.ribSnap = gate == 2; }
        }
        return;
    }
    // let go
    tk.ribOn = 0;
    for (int k = 0; k < 2; ++k)
    {
        Voice& v = tk.voice[k];
        if (! v.active) continue;
        if (v.machine == M_CHOIR && v.cs.rib) { v.cs.rib = 0; v.padHeld = 0; v.cs.holdLeft = 0.0f; continue; }
        if (v.padHeld && v.step < 0 && absf (v.note - floorf_ (tk.ribSemis + 0.5f)) < 0.5f)
        {
            v.padHeld = 0;
            if (v.machine == M_HALO) { if (v.hs.held) v.hs.holdLeft = 0.5f; continue; }
            if (v.machine == M_STRUM) { if (v.ss.held) v.ss.holdLeft = 0.5f; continue; }
            if (v.machine == M_HOWL) { if (v.hw.held) v.hw.holdLeft = 0.5f; continue; }
            if (v.machine == M_MANTRA) { mantraRelease (v); continue; }
            letGo (v);
        }
    }
}

float Engine::lock (const Track& t, int step, int p) const
{
    if (step >= 0) { float l = t.lock[step][p]; if (l >= 0.0f) return l; }
    return t.p[p];
}

// does a modulator of this step target parameter p? (bit 4m + r: modulator m's route r)
int Engine::lfoTargets (const Track& t, int step, int p) const
{
    int b = 0;
    for (int m = 0; m < MOD_N; ++m)
        for (int r = 0; r < MOD_ROUTES; ++r)
            if (modDest (lock (t, step, MOD_DST[m][r])) == p) b |= 1 << (4 * m + r);
    return b;
}

// a parameter read once per trig (the retrig settings): its value with the modulators at that instant, like a
// sample & hold of them on every hit
float Engine::trigParam (const Track& t, int step, int p) const
{
    float v = lock (t, step, p);
    const int b = lfoTargets (t, step, p);
    for (int m = 0; m < MOD_N; ++m)
        if (b & (15 << (4 * m)))
        {
            const float amt = lock (t, step, MOD_AM[m]), mv = modValue (t, m);
            for (int r = 0; r < MOD_ROUTES; ++r)
                if (b & (1 << (4 * m + r))) v += mv * (lock (t, step, MOD_DEP[m][r]) - 0.5f) * 2.0f * amt;
        }
    return b ? clampf (v, 0.0f, 1.0f) : v;
}

double Engine::samplesPerStep (const Track& t) const
{
    return (double) sr * 60.0 / (double) bpm / 4.0 / (double) SPEEDS[t.spd];
}

// ----------------------------------------------------------------- triggering


void Engine::triggerTrack (int ti, int step, float velMul, int isRetrig)
{
    Track& t = tr[ti];
    if (! isRetrig)
    {
        float prob = lock (t, step, P_PROB);
        if (prob < 0.995f && t.rng.uni() >= prob) return;
        int ri = idxOf (trigParam (t, step, P_RTRG), NUM_RETRIG);
        const int nr = step >= 0 ? (int) t.rat[step] : 1;
        t.rtCurve = (trigParam (t, step, P_RVEL) - 0.5f) * 2.0f;
        if (nr > 1 || ri > 0)
        {
            // a ratchet splits the step into nr even hits; otherwise the retrig rate / count apply
            int cnt = nr > 1 ? nr - 1 : 1 + idxOf (trigParam (t, step, P_RCNT), NUM_RCOUNT);
            t.rtLeft = cnt; t.rtTotal = cnt; t.rtStep = step; t.rtRatchet = nr > 1;
            t.rtBase = nr > 1 ? samplesPerStep (t) / (double) nr
                              : samplesPerStep (t) * 16.0 / (double) RETRIG_DEN[ri];
            // RCRV: the hits close in (right) or spread out (left), the last one where an even roll ends it (cnt gaps
            // of the rate): the gaps change by the same ratio from one to the next, up to 16x from the first to the last
            t.rtScale = 1.0; t.rtRatio = 1.0;
            const float crv = (trigParam (t, step, P_RCRV) - 0.5f) * 2.0f;
            if (cnt > 1 && absf (crv) > 0.01f)
            {
                const float e = 4.0f * (crv > 0.0f ? -absf (crv) : absf (crv));       // log2 of the last gap against the first
                const double q = (double) exp2f_ (e / (float) (cnt - 1));
                t.rtRatio = q;
                t.rtScale = (double) cnt * (1.0 - q) / (1.0 - (double) exp2f_ (e * (float) cnt / (float) (cnt - 1)));
            }
            t.rtInterval = t.rtBase * t.rtScale;
            t.rtNext = now + t.rtInterval;
            velMul = t.rtCurve < 0.0f ? 1.0f : 1.0f - t.rtCurve;
        }
        else t.rtLeft = 0;
    }
    const float* lk = step >= 0 ? t.lock[step] : NOLOCK;
    float vel  = lock (t, step, P_VEL) * velMul;
    float nraw = (lock (t, step, P_NOTE) - 0.5f) * 48.0f;
    float note = (float) (int) (nraw + (nraw >= 0.0f ? 0.5f : -0.5f));
    note = jumpNote (t, step, note);
    float hold = lock (t, step, P_GATE) * 8.0f * (float) (samplesPerStep (t) / sr);
    retrigHit = isRetrig;
    startVoice (t, ti, lk, step, vel, note, hold);
    retrigHit = 0;
    t.hitCount++;
}

// a lock-only step: its p-locks replace the ones the playing voice started with (until the next trig); nothing is
// retriggered, the envelopes and oscillators run on. The machine itself cannot change under a voice.
void Engine::triglessStep (int ti, int step)
{
    if (step < 0 || step >= NUM_STEPS) return;
    Track& t = tr[ti];
    Voice& v = t.voice[t.cur];
    if (! v.active || v.choking) return;
    const float* lk = t.lock[step];
    for (int p = 1; p < NUM_PARAMS; ++p) if (lk[p] >= 0.0f) v.lk[p] = lk[p];
}

// JUMP: a trig that has one (the track's, or a p-lock on its step) plays where the jumps have got to, then moves them on
// by its value; past the range (RNG, semitones either way) they start again from the note itself. On a quantised track
// (a pitched machine, a scale on) the jumps count scale degrees: +2 = two notes of the scale up. HALO's NOTE is a degree
// already, BREAK's a slice: their jumps move those. The retrigs and ratchets of a trig jump too; a pad or a key does not
float Engine::jumpNote (Track& t, int step, float note)
{
    const int j = idxOf (lock (t, step, P_JUMP), NUM_JUMPS) - (NUM_JUMPS - 1) / 2;
    if (j == 0) return note;
    const float range = (float) (1 + idxOf (lock (t, step, P_JRNG), NUM_JRANGE));
    const int m = machineOf (lock (t, step, P_MACH));
    const u32 mask = quantMask();
    // (the HEX voices quantise against their own pitch: sk_kit.cpp hexRender)
    static const float HEX_TOM_HZ[3] = { 233.0f, 155.0f, 98.0f };
    const float base = m == M_HEXBD ? 58.0f : m == M_HEXSD ? 196.0f : m == M_HEXTM ? HEX_TOM_HZ[idxOf (lock (t, step, P_F), 3)] : BASE_HZ[m];
    const bool deg = t.q && mask && base > 0.0f && m != M_HALO && m != M_STRUM;
    // the semitones `a` jumps away from the note (degrees: walked along the scale from the scale note nearest the pitch)
    int n0 = 0;
    if (deg)
    {
        const float st = (lock (t, step, P_TUNE) - 0.5f) * 48.0f + note;
        n0 = (int) floorf_ (69.0f + 12.0f * log2f_ (base * (1.0f / 440.0f)) + qPitch (st, base, mask) + 0.5f);
    }
    auto semis = [&] (int a) -> float {
        if (! deg) return (float) a;
        int n = n0, d = a > 0 ? 1 : -1;
        for (int k = 0; k != a; k += d)
            do n += d; while (! ((mask >> (((n % 12) + 12) % 12)) & 1u));
        return (float) (n - n0);
    };
    const float out = note + semis (t.jAcc);
    t.jAcc += j;
    if (absf (semis (t.jAcc)) >= range - 1e-3f) t.jAcc = 0;
    return out;
}

int machineSideDefaults (int m, float* out)
{
    // (the same values as the page's MACH entries, d[8..]; test_web checks they agree)
    // ALLOY: IMPACT (a mallet between felt and steel, a little transient and snap of pitch, no bounce, a little life),
    // NOISE (none, ready bright and short; no feedback, no drive, the DAC off), FM (the object alone; the FM ready on
    // √2, its depth at once, no sweep), COLOR (the sines ready on 8 times the note, the comb off, on the note, ready at
    // half its feedback), SPACE (a fifth in, a small metal room, short and a little damped, no shift, no gate; the LFO
    // at 0.6 Hz)
    static const float ALY[NUM_SIDES] = { 0.75f, 0.2f, 0.56f, 0.0f, 0.35f, 0.5f, 0.3f,
                                          0.0f, 0.0f, 0.76f, 0.3f, 0.0f, 0.0f, 0.0f,
                                          1.0f, 0.0f, 0.3f, 0.4f, 0.0f, 0.35f, 0.5f,
                                          0.0f, 0.5f, 0.0f, 24.5f / 49.0f, 0.75f, 0.0f, 0.0f,
                                          0.2f, 0.25f, 0.4f, 0.4f, 0.5f / 13.0f, 0.0f, 0.4f };
    // CHIMERA (v66): GREFFE off (no body; 25 ms, 8 ms, no split, at A's pitch, B at its level), FAMILLE alike in both,
    // within its category, on the attack, at their own pitch; the LOOP's seed 0
    static const float CHM[NUM_SIDES] = { 0.0f, 0.0f, 0.64241f, 0.63271f, 0.0f, 0.5f, 0.66667f,
                                          0.5f, 0.75f, 1.0f / 6.0f, 0.25f, 0.0f, 0.0f, 0.0f };
    if (m == M_CHIMERA) { for (int i = 0; i < NUM_SIDES; ++i) out[i] = CHM[i]; return NUM_SIDES; }
    // CUBE (v74): OSC (B on A's note, in A's place, not heard; no unison; FACT), MOD (no sweep on Y, 0.13 s; no pitch
    // envelope, 22 ms; every axis morphing, no warp, no bits)
    static const float CUB[14] = { 24.5f / 49.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 0.25f,
                                   0.5f, 0.4f, 0.5f, 0.3f, 0.125f, 0.5f, 1.0f / 12.0f };
    if (m == M_CUBE) { for (int i = 0; i < NUM_SIDES; ++i) out[i] = i < 14 ? CUB[i] : 0.0f; return NUM_SIDES; }
    // WEST (v78: EaseLFO's whole panel, sk_west.h): S1 .. S35 (XFAD none, FOLD SINE, no glide, a little drift, the
    // envelope a little on TIMB (TAMT), some velocity, a little warm; the modulator a triangle, FM, on the note, the
    // envelope a little on its index (IAMT), nothing on TIMB, gate 2 not heard (CHB); the pulser endless, even, each
    // pulse a little quieter, no swing, every pulse, steady, no draws; five steps at 0, on the pitch; gate 1 BOTH, no
    // resonance, no attack nor sustain, TRAN; a little spring, middling), then S36 .. (the complex oscillator's TRI, on
    // the note; the CVs: SVS on the pitch (none), the envelope on TIMB and MOD, SVS on the modulator (none); RANDOM on
    // the pulses, the inverter on the envelope; the envelope on the pulses, EXP; the pulser on the trigs, the sequencer
    // AUTO, no stage pulses; gate 1 opened by the envelope, gate 2 BOTH on the modulator, by the envelope; gate 1 at
    // 0 dB; no CV on the pulser; the spring middling, no chaos, no shake)
    static const float WST[NUM_SIDES] = { 0.0f, 0.25f, 0.0f, 0.15f, 0.65f, 0.3f, 0.25f,
                                          2.5f / 3.0f, 0.5f / 3.0f, 0.25f, 0.5f, 0.6f, 0.0f, 0.0f,
                                          16.5f / 17.0f, 0.5f, 0.6f, 0.0f, 1.0f, 0.0f, 0.0f,
                                          0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.9f, 0.125f,
                                          0.125f, 0.0f, 0.0f, 0.0f, 0.125f, 0.15f, 0.45f,
                                          0.5f / 3.0f, 0.25f, 0.5f / 8.0f, 0.5f, 1.5f / 8.0f, 1.5f / 8.0f, 0.5f / 8.0f, 0.5f,
                                          2.5f / 3.0f, 1.5f / 6.0f, 1.5f / 3.0f, 1.0f, 1.5f / 3.0f, 0.125f, 0.5f / 32.0f,
                                          0.0f, 1.5f / 8.0f, 1.0f, 0.5f / 3.0f, 0.5f / 3.0f, 0.0f, 1.5f / 8.0f, 1.0f, 0.70710678f,
                                          0.5f / 8.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    if (m == M_WEST) { for (int i = 0; i < NUM_SIDES; ++i) out[i] = WST[i]; return NUM_SIDES; }
    // HOWL (v81): STRING (a pick past the middle, a little dark, the strings ringing each other a little, no hum, the
    // feedback blooming in about a second, a little out of tune, the arm still), AMP (a hot preamp, half fuzz, the tone
    // stack's knobs a little bassy and scooped, some sag, its level), LOOP (v82: the guitar a step from the amp, the
    // loop's colour in the middle and a little focused, no pickup squeal, a little hiss, the guitar swaying a little,
    // the howl singing under the chord), CAB (v83: as v82's amp: no TGHT or BITE, PRES and DPTH flat, the speaker
    // as it was, a 4x12, the mic neither close nor back), BOW (v83: off; its swell about a quarter of a second, on the
    // highest string, FUND, half way along, the hand a little unsteady, the other strings left ringing)
    static const float HWL[HW_NSIDE] = { 0.55f, 0.45f, 0.3f, 0.0f, 0.45f, 0.25f, 0.0f,
                                         0.65f, 0.5f, 0.5f, 0.7f, 0.35f, 0.4f, 0.7f,
                                         0.6f, 0.55f, 0.45f, 0.0f, 0.2f, 0.25f, 0.5f,
                                         0.0f, 0.0f, 0.5f, 0.0f, 0.5f, 1.0f, 0.5f,
                                         0.0f, 0.45f, 1.5f / 7.0f, 0.5f / 3.0f, 0.5f, 0.2f, 0.0f };
    if (m == M_HOWL) { for (int i = 0; i < NUM_SIDES; ++i) out[i] = i < HW_NSIDE ? HWL[i] : 0.0f; return NUM_SIDES; }
    // MANTRA (v84): tuned to a drone on A (A1 twice, a little apart; E2 A2; E3 A3; C#4 E4), a little SHARP; no FM yet
    // (its sources off), the pairs slow; the LFO slow, on OR, no vibrato; the delay's lines at 120 and 200 ms, a little
    // modulated by the triangles, a third in; a little drive in the mix, the bus driven, some crosstalk, the parts'
    // tolerances, a little drift and hiss
    static const float MTR[MT_NSIDE] = { 0.2778f, 0.2807f, 0.1551f, 0.2778f, 0.15f, 0.2f, 0.85f,
                                         0.4250f, 0.5158f, 0.3651f, 0.4250f, 0.25f, 0.3f, 0.85f,
                                         0.25f, 0.5f / 3.0f, 0.25f, 0.25f, 0.5f / 3.0f, 0.25f, 0.25f,
                                         0.25f, 0.5f / 3.0f, 0.25f, 0.25f, 0.5f / 3.0f, 0.25f, 0.25f,
                                         0.35f, 0.5f, 0.25f, 0.25f, 0.25f, 0.35f, 0.35f,
                                         0.55f, 0.65f, 0.1f, 0.15f, 0.25f, 0.25f, 0.3f,
                                         0.25f, 0.7f, 0.45f, 0.3f, 0.4f, 0.25f, 0.15f };
    if (m == M_MANTRA) { for (int i = 0; i < NUM_SIDES; ++i) out[i] = i < MT_NSIDE ? MTR[i] : 0.0f; return NUM_SIDES; }
    // STENCIL (v83): its SPACE side (the halo middle-sized, half its decay, a little dark, drifting a little; µ's speed,
    // no ducking, the stereo as it is)
    static const float STC[7] = { 0.45f, 0.55f, 0.45f, 0.3f, 0.35f, 0.0f, 0.5f };
    if (m == M_FXSTENCIL) { for (int i = 0; i < NUM_SIDES; ++i) out[i] = i < 7 ? STC[i] : 0.0f; return NUM_SIDES; }
    // RADIO (v87): AIR (the band open, centred near 950 Hz when it narrows; no fading, a slow one ready; no whistle,
    // no crackle, no drive)
    static const float RAD[RA_NSIDE] = { 0.0f, 0.5f, 0.0f, 0.4f, 0.0f, 0.0f, 0.0f };
    if (m == M_RADIO) { for (int i = 0; i < NUM_SIDES; ++i) out[i] = i < RA_NSIDE ? RAD[i] : 0.0f; return NUM_SIDES; }
    if (m != M_ALLOY) return 0;
    for (int i = 0; i < NUM_SIDES; ++i) out[i] = ALY[i];
    return NUM_SIDES;
}

int machineAFDefaults (int m, float* out)
{
    // (the page's MACH entry, d[2..7]: ALLOY a free bar as built, of steel, middle-sized, struck a third of the way in;
    // test_web checks them)
    static const float ALY[6] = { 1.5f / 8.0f, 0.5f, 0.7f, 0.65f, 0.35f, 0.5f / 4.0f };
    // CHIMERA: the first kick, VARI off, from its start, forward, the PLAY page
    static const float CHM[6] = { 0.05f, 0.0f, 0.5f, 0.0f, 0.25f, 0.5f / 3.0f };
    if (m == M_CHIMERA) { for (int i = 0; i < 6; ++i) out[i] = CHM[i]; return 6; }
    // WEST (v76: a track of a v75 project on SPHERE, its number then): TIMB 30 %, SYMM centred, MOD 20 %, RATI on
    // root 2, the pulser off, MAIN
    static const float WST[6] = { 0.15f, 0.5f, 0.2f, 5.5f / 16.0f, 0.5f / 11.0f, 1.5f / 7.0f };    // (v79: F its page, COLOUR of 7)
    if (m == M_WEST) { for (int i = 0; i < 6; ++i) out[i] = WST[i]; return 6; }
    // HOWL (v81): a power chord laid as a drone, an octave down, FDBK 55 %, MRPH 1.5 s, the CHORD page (v83: of six)
    static const float HWL[6] = { 0.5f / 15.0f, 0.5f / 8.0f, 2.5f / 5.0f, 0.6f, 0.64f, 0.5f / 6.0f };
    if (m == M_HOWL) { for (int i = 0; i < 6; ++i) out[i] = HWL[i]; return 6; }
    // MANTRA (v84): a trig touches all eight, no HOLD, the delay's feedback under its middle, a little drive, PLAY
    static const float MTA[6] = { 14.5f / 15.0f, 0.0f, 0.0f, 0.3f, 0.2f, 0.5f / 8.0f };
    if (m == M_MANTRA) { for (int i = 0; i < 6; ++i) out[i] = MTA[i]; return 6; }
    if (m != M_ALLOY) return 0;
    for (int i = 0; i < 6; ++i) out[i] = ALY[i];
    return 6;
}

// DEC on HOLD (its top), on a machine with an amplitude envelope: the note holds while its gate is open (a pad or a key
// pressed, the trig's GATE, or until the next trig), then ends in HOLD_REL. HALO holds its chord while its pad is held.
static inline bool holdMachine (int m)
{
    // (RAM keeps its own HOLD: a take plays out, whatever the gate; a loop holds like the others)
    return (m != M_CP_NZ && m != M_FRACT && m != M_ITERA && m != M_HALO && m != M_BREAK && m != M_RAM && m < M_FXVERB)
        || m == M_ALLOY || m == M_CUBE || m == M_WEST || m == M_RADIO;
}
void Engine::holdSetup (Voice& v, const float* P, int m, float& holdSec)
{
    // v81: a drone (the transport started it), or a pad or a key on a drone's track (stopped): held whatever DEC
    const bool dr = droneHit || (padHit && droneWanted (tr[v.ti]));
    // RAM on HOLD (its own: DEC from 99.5 %) playing a loop: it would never end, so it holds like a gate too
    const bool ramLoop = m == M_RAM && (P[P_DEC] >= 0.995f || dr) && idxOf (P[P_C], RAM_MODES) >= 2;
    // ITERA's own DEC at its top: its partials ring free until the next trig (its GATE does not end them)
    const bool iteraRun = m == M_ITERA && P[P_DEC] > 0.985f;
    v.hold = ((P[P_DEC] >= DEC_HOLD || dr) && holdMachine (m)) || ramLoop;
    v.padHeld = ! droneHit && padHit && (v.hold || m == M_HALO || m == M_STRUM || m == M_CHOIR || m == M_HOWL || m == M_MANTRA || iteraRun);
    v.hrel = v.hold ? decayCoef (HOLD_REL, sr) : 0.0f;
    v.sus = droneHit || (v.hold && ! v.padHeld && holdSec <= 0.0f) || (iteraRun && ! v.padHeld);
    v.gateShut = 0;
    v.drone = droneHit;
    if (v.padHeld || (v.hold && v.sus) || droneHit) holdSec = HOLD_OPEN;
}
void Engine::letGo (Voice& v)
{
    v.sus = 0;
    if (! v.active || v.machine == M_HALO || v.machine == M_STRUM || v.machine == M_CHOIR || v.machine == M_HOWL || v.machine == M_MANTRA) return;   // (they let go themselves on a stop or a mute)
    v.gateShut = 1;
    v.aHoldLeft = 0.0f;
    if (v.astage == 1) v.astage = 2;
    v.hrel = decayCoef (HOLD_REL, sr);
    const float el = (float) v.ns / sr;                 // (SINE's square gate shuts too)
    if (v.holdSec > el) v.holdSec = el;
    if (v.machine == M_MACRO && v.mac) macroRelease (v.mac);
}
// LOOP's lengths (LEN): how many draws before the first comes round again
static const int LFO_LENS[NUM_LFO_LENS] = { 2, 3, 4, 5, 6, 7, 8, 12, 16, 32 };
// an LFO's SLEW and LOOP settings (SLW, LEN, the seed), and which LFO it is (the salt of its draws)
struct LfoMore { float slw, len, seed; unsigned salt; };
// SLEW: from where the cycle started towards its draw, in SLW of the cycle (an S-curve; at 0 it steps like S&H)
static inline float slewAt (const Lfo& l, float slw)
{
    if (slw <= 0.001f) return l.sh;
    const float x = minf (l.ph / slw, 1.0f);
    return l.from + (l.sh - l.from) * x * x * (3.0f - 2.0f * x);
}
static inline float loopAt (Lfo& l, const LfoMore& m)
{
    const int n = LFO_LENS[idxOf (m.len, NUM_LFO_LENS)], i = l.k % n;
    l.lp = ((float) i + minf (l.ph, 1.0f)) / (float) n;
    return loopDraw (m.seed, m.salt, (unsigned) i);
}
// an LFO in a trig mode starts its cycle again: its value there (what the hit reads), a new draw for S&H (SLEW glides
// from where it was to it; LOOP starts its loop again)
static void lfoRestart (Lfo& l, Rng& rng, float wavP, const LfoMore& m)
{
    l.ph = 0.0f; l.from = l.val; l.sh = rng.bi(); l.k = 0; l.lp = -1.0f;
    switch (idxOf (wavP, NUM_LFO_WAVES) % LFO_SHAPES)
    {
        case LW_SIN:  l.val = 0.0f; break;
        case LW_TRI:  l.val = -1.0f; break;
        case LW_SAW:  l.val = -1.0f; break;
        case LW_SQR:  l.val = 1.0f; break;
        case LW_SH:   l.val = l.sh; break;
        case LW_SLEW: l.val = slewAt (l, m.slw); break;
        case LW_LOOP: l.val = loopAt (l, m); break;
        default:      l.val = 1.0f; break;     // the decay
    }
}
// v81: DRONE. The machines that can hold one: those with an amplitude envelope (DEC's HOLD), the chord and voice machines
// (they hold their own), HOWL, and RAM on a loop; not CP·NZ, FRACT, ITERA, BREAK nor the FX machines (no note to hold)
static inline bool droneMachine (int m, const float* p)
{
    if (m == M_RAM) return idxOf (p[P_C], RAM_MODES) >= 2;
    return holdMachine (m) || m == M_HALO || m == M_STRUM || m == M_CHOIR || m == M_HOWL || m == M_MANTRA;
}
bool Engine::droneWanted (const Track& t) const { return t.p[P_DRON] >= 0.5f && droneMachine (machineOf (t.p[P_MACH]), t.p); }
// the transport runs, the track plays: its drone starts (the track's settings, its own velocity, in its attack)
void Engine::droneStart (Track& t, int ti)
{
    droneHit = 1;
    startVoice (t, ti, NOLOCK, -1, lock (t, -1, P_VEL), 0.0f, 0.0f);
    droneHit = 0;
    t.hitCount++;
}
// a trig (or a pad, a key) on a drone: its locks replace the voice's (as a lock-only step's: its machine stays), its
// note is glided to; the chord and voice machines take it as theirs (HALO its next chord, voice-led; STRUM strums it;
// CHOIR sings on, legato; HOWL's strings glide to the new chord, picked again by EXCT); the trig-mode LFOs start again
void Engine::droneChange (Track& t, int ti, Voice& v, const float* lk, int step, float vel, float note)
{
    for (int p = 1; p < NUM_PARAMS; ++p) v.lk[p] = lk[p];
    v.lk[P_MACH] = -1.0f;
    v.step = step; v.vel = vel;
    modHit (ti, lk, vel, 0.0f);
    const int m = v.machine;
    if (m == M_HALO || m == M_STRUM || m == M_CHOIR || m == M_HOWL || m == M_MANTRA)
    {
        float P[NUM_PARAMS];
        effParams (t, v.lk, P);
        v.note = note; v.noteT = note; v.holdSec = HOLD_OPEN;
        const int ph = padHit; padHit = 0;                 // (a pad on a drone changes it: it never holds it)
        machineTrigger (v, P);
        padHit = ph;
    }
    else v.noteT = note;
    t.hitCount++;
}
// the drone let go: in its release (as long as its attack, a quarter of a second at least); HALO, STRUM, CHOIR in theirs,
// HOWL's hand on its strings, the guitar turned away
void Engine::droneEnd (Voice& v)
{
    v.drone = 0; v.sus = 0;
    if (! v.active || v.choking) return;
    switch (v.machine)
    {
        case M_HALO:  if (v.hs.held) v.hs.holdLeft = 0.5f; return;
        case M_STRUM: if (v.ss.held) v.ss.holdLeft = 0.5f; return;
        case M_CHOIR: if (v.cs.held) v.cs.holdLeft = 0.5f; return;
        case M_HOWL:  howlLetGo (v, 0.5f); return;
        case M_MANTRA: mantraLetGo (v); return;
        default: break;
    }
    letGo (v);
    const Track& t = tr[v.ti];
    const float atk = v.lk[P_ATK] >= 0.0f ? v.lk[P_ATK] : t.p[P_ATK];
    if (atk > 0.002f) v.hrel = decayCoef (maxf (HOLD_REL, 0.001f * powf_ (1500.0f, atk)), sr);
}
void Engine::startVoice (Track& t, int ti, const float* lk, int step, float vel, float note, float holdSec)
{
    Voice& old = t.voice[t.cur];
    const int newM = machineOf (lk[P_MACH] >= 0.0f ? lk[P_MACH] : t.p[P_MACH]);
    // v81: the track's drone playing: a trig (or a pad, a key) changes it instead of striking it again
    if (! droneHit && old.active && old.drone && ! old.choking && old.machine == newM && droneWanted (t))
    {
        droneChange (t, ti, old, lk, step, vel, note);
        return;
    }
    // MACRO: the track has one Plaits unit, so a MACRO hit always retriggers a running MACRO voice (even
    // a fading one) instead of starting a second voice on the same unit. HALO: a new chord comes on the voice
    // that holds the last one (its notes lead into the next)
    // BREAK: the next slice comes on the voice that plays the last one (it carries on, or cuts it itself)
    // STRUM: the strings ring into the next strum; CHOIR: the voice sings on into the next note (legato, its glide)
    // WEST: pinged again where it is (the vactrol's light, its phases, its glide, its pulser carry on)
    // HOWL: the strings glide to the new chord, the loop and the amp carry on (MRPH)
    // MANTRA: its voices run on, touched again (their envelopes, the supply, the LFO, the delay carry on)
    const bool reuse = (newM == M_MACRO || newM == M_HALO || newM == M_BREAK || newM == M_STRUM || newM == M_CHOIR || newM == M_WEST || newM == M_HOWL || newM == M_MANTRA) && old.active && old.machine == newM;
    if (! reuse)
    {
        if (old.active) old.choking = 1;
        const int lastVar = old.ovar;
        t.cur ^= 1;
        t.voice[t.cur].ovar = lastVar;
    }
    Voice& v = t.voice[t.cur];
    v.ti = ti;

    for (int p = 0; p < NUM_PARAMS; ++p) v.lk[p] = lk[p];
    // the LFOs in a trig mode start again with this hit (at the start of their cycle), then the settings as the hit
    // finds them: its locks, and what the LFOs and the envelope followers do to them now (what a machine reads only
    // when it is struck — STRUM's chord and spread, a model, a count... — follows them from hit to hit)
    modHit (ti, lk, vel, holdSec);
    float P[NUM_PARAMS];
    effParams (t, lk, P);
    // RPAN: this hit's pan around the track's: to the right a draw on either side (as far as the knob says, at its top
    // from one edge to the other), to the left left and right in turn, the first to the left (no draw at its centre)
    {
        const float rp = P[P_RPAN] - 0.5f;
        if (absf (rp) > 0.005f)
        {
            const float a = absf (rp) * 2.0f;
            if (rp > 0.0f) t.rpanT = a * 0.5f * t.rng.bi();
            else { t.rpanS = t.rpanS < 0 ? 1 : -1; t.rpanT = a * 0.5f * (float) t.rpanS; }
        }
        else t.rpanT = 0.0f;
    }
    // (the hit's place in a roll, for the loops: RDEC's, RTUN's, LOOMND's)
    if (step >= 0) { t.rdSub = (step == t.rdStep && retrigHit) ? t.rdSub + 1 : 0; t.rdStep = step; }
    else t.rdSub = (t.rdSub + 1) & 15;
    // RAND (v89: WTF?!): a new draw for this hit on the tracks marked r (never RAM, never the machine's choices). LIVE:
    // a draw at each hit (as it always was); LOOMND, to the left of its centre: the draw of the hit's place in a loop
    // of LEN steps (the track's steps counted from the start, each hit of a roll its own), the same each time round;
    // LFO: no draw, the LFO moves them (the settings it may move marked 1)
    {
        const int mask = (int) (g[G_XMSK] * 255.0f + 0.5f), mach = machineOf (P[P_MACH]);
        const int wm = idxOf (g[G_WTFM], WTF_MODES);
        v.rndOn = ((mask >> ti) & 1) && mach != M_RAM;
        v.wtfPh = 0.0;
        if (v.rndOn)
        {
            const bool loop = wm == WTF_LOOMND && g[G_XRND] < 0.495f;
            const unsigned len = (unsigned) WTF_LEN[idxOf (g[G_WTFL], WTF_LENS)];
            const unsigned li = step >= 0 ? (unsigned) (((t.bAbs < 0 ? 0 : t.bAbs) % (int) len) * 64) + (unsigned) minf ((float) t.rdSub, 63.0f) : 8192u + (unsigned) t.rdSub;
            for (int k = 0; k < 12; ++k)
            {
                const bool stepped = k >= 1 && k <= 6 && ((STEPPED_AF[mach] >> (k - 1)) & 1);
                v.rnd[k] = stepped ? 0.0f : wm == WTF_LFO ? 1.0f : loop ? loopDraw (g[G_WTFS], 16u + (unsigned) k, li) : xrng.bi();
            }
        }
        wtfApply (v, P, 0);
    }
    // RDEC: this hit's decay around the track's (see rdecApply): to the right a draw at each hit, to the left the draw
    // of its step (each hit of a roll its own), the same each time the pattern comes round (no draw at its centre)
    // RTUN (v88): this hit's pitch around the track's (see rtunApply), drawn as RDEC draws its decay (its own seed, its
    // own salt: the two loops do not move together)
    {
        const float rd = P[P_RDEC] - 0.5f, rt = P[P_RTUN] - 0.5f;
        v.rdec = 0.0f; v.rtun = 0.0f;
        // (a hit off the steps — MIDI, the pads — draws by its count: 16, then round again)
        const unsigned li = step >= 0 ? (unsigned) step * 64u + (unsigned) minf ((float) t.rdSub, 63.0f) : 4096u + (unsigned) t.rdSub;
        const int mach = machineOf (P[P_MACH]);
        if (absf (rd) > 0.005f && decayMachine (mach))
        {
            const float a = absf (rd) * 2.0f * RDEC_RANGE;
            v.rdec = rd > 0.0f ? a * t.rng.bi() : a * loopDraw (P[P_RDSD], 3u, li);
        }
        if (absf (rt) > 0.005f && ! fxMachine (mach))
        {
            const float a = absf (rt) * 2.0f, s = a * a * RTUN_RANGE;
            v.rtun = rt > 0.0f ? s * t.rng.bi() : s * loopDraw (P[P_RTSD], 7u, li);
        }
        rdecApply (v, P);
        rtunApply (v, P);
    }
    if (reuse)
    {
        holdSetup (v, P, newM, holdSec);
        v.step = step; v.vel = vel; v.note = note; v.noteT = note; v.holdSec = holdSec;
        v.choke = 1.0f; v.choking = 0; v.tail = 0; v.level = 1.0f; v.ns = 0;
        if (P[P_ATK] > 0.002f && newM != M_HALO && newM != M_BREAK && newM != M_STRUM && newM != M_CHOIR && newM != M_WEST && newM != M_HOWL && newM != M_MANTRA) v.atk = 0.0f;
        v.fenv = 1.0f; v.pe1 = v.pe2 = 1.0f;
        v.quant = t.q;
        machineTrigger (v, P);
        goto voiceStarted;
    }

    holdSetup (v, P, newM, holdSec);
    v.active = 1; v.step = step; v.vel = vel; v.note = note; v.noteT = note; v.holdSec = holdSec;
    v.choke = 1.0f; v.choking = 0; v.ns = 0; v.tail = 0; v.level = 1.0f;
    v.atk = P[P_ATK] > 0.002f ? 0.0f : 1.0f;
    v.machine = machineOf (P[P_MACH]);
    v.aenv = 0.0f; v.astage = 0; v.aHoldLeft = holdSec * sr;
    v.penv = v.menv = v.nenv = v.cenv = v.senv = v.ienv = v.i3env = 1.0f;
    for (int i = 0; i < 8; ++i) { v.ph[i] = 0.0f; v.y1[i] = v.y2[i] = 0.0f; }
    v.fb1 = v.fb2 = v.lastOut = 0.0f;
    v.s1.reset(); v.s2.reset(); v.s3.reset(); v.lp1 = v.lp2 = 0.0f;
    v.srrCnt = 1e9f; v.srrHold = 0.0f;
    v.flt.reset(); v.ms.reset(); v.fenv = 1.0f; v.dcx = v.dcy = v.acI1 = v.acI2 = 0.0f; v.smOk = 0; v.drvPrev = 0.0f; v.drvLc = 0.0f;
    v.spPh = 1.0f; v.spHold = v.spLp1 = v.spLp2 = 0.0f;
    for (int k = 0; k < 4; ++k) v.lad[k] = 0.0f;
    v.pe1 = v.pe2 = 1.0f; v.pLp = 0.0f;
    v.wLo = v.wEnv = v.wPost = v.wPost2 = 0.0f;
    v.quant = t.q;
    v.hs.fresh = 1; v.ss.fresh = 1; v.cs.fresh = 1; v.hw.fresh = 1; v.mt.fresh = 1; v.rd.fresh = 1;
    v.bs.on = 0; for (int j = 0; j < BRK_SEGS; ++j) v.bs.s[j].on = 0;
    v.bs.flags = 0; v.bs.pj = 0; v.bs.skip = 0; v.bs.slice = 0; v.bs.k0 = 0; v.bs.brk = 0;
    v.bs.lp1 = v.bs.lp2 = v.bs.crk = v.bs.hp = 0.0f; v.bs.pos = 0.0f;
    if (idxOf (P[P_FTYP], NUM_FTYPES) == FT_COMB)
    {
        for (int i = 0; i < COMB_SIZE; ++i) v.comb[i] = 0.0f;
        v.cw = 0;
    }
    machineTrigger (v, P);
voiceStarted:

    // choke group: cut the other tracks of the same group (open / closed hats...)
    const int grp = idxOf (P[P_CHOK], NUM_CHOKE);
    if (grp > 0)
        for (int o = 0; o < NUM_TRACKS; ++o)
            if (o != ti && idxOf (tr[o].p[P_CHOK], NUM_CHOKE) == grp)
                for (int k = 0; k < 2; ++k) if (tr[o].voice[k].active) tr[o].voice[k].choking = 1;

    // (the LFOs in a trig mode started again above, before the settings were read)
}

void Engine::machineTrigger (Voice& v, const float* P)
{
    switch (v.machine)
    {
        case M_BD_VA:
            v.aHoldLeft += P[P_F] * 0.25f * sr;
            v.os2.reset (HB1);
            break;
        case M_HH_MT:
            for (int k = 0; k < 6; ++k) v.ph[k] = v.rng.uni();
            break;
        case M_CP_NZ:
        {
            int nb = 1 + idxOf (P[P_A], 8);
            float spacing = 0.003f * powf_ (12.0f, P[P_B]) * sr;
            float jit = P[P_E];
            float t = 0.0f;
            for (int k = 0; k < nb; ++k)
            {
                v.burstAt[k] = t;
                t += spacing * (1.0f + jit * (v.rng.uni() * 1.2f - 0.6f));
            }
            v.nBursts = nb; v.burstIdx = 0; v.benv = 0.0f; v.tenv = 0.0f;
            break;
        }
        case M_IMPLS:
            v.impLeft = 1 + idxOf (P[P_A], 32);
            v.nextImp = 0.0f;
            v.curInt  = 0.001f * powf_ (150.0f, P[P_B]) * sr;
            v.impAmp  = 1.0f;
            for (int i = 0; i < COMB_SIZE; ++i) v.mcomb[i] = 0.0f;
            v.mcw = 0;
            break;
        case M_NOISE:
            v.lfsr = 0x7FFFu; v.shCnt = 1.0f; v.shVal = 0.0f;
            v.grainLeft = 0.0f; v.gate = 1.0f; v.gateS = 1.0f;
            break;
        case M_SY_RAW:
            for (int k = 3; k < 7; ++k) v.ph[k] = v.rng.uni();
            break;
        case M_BYTE:
            v.bt = 0; v.btc = 0.0f; v.btOut = 0.0f;
            break;
        case M_ULOOP:
        {
            // the voice loops tiny windows of a source: a synthetic burst rendered now, or a bank sample
            const int si = idxOf (P[P_A], NUM_ULOOP_SRC);
            float tuneSt = (P[P_TUNE] - 0.5f) * 48.0f + v.note;
            const u32 qmU = v.quant ? quantMask() : 0u;
            if (qmU) tuneSt = qPitch (tuneSt, si >= NUM_ULOOP_SYNTH ? 261.63f : 220.0f, qmU);
            int Ls; float srcSr;
            if (si >= NUM_ULOOP_SYNTH)
            {
                const int id = si - NUM_ULOOP_SYNTH, var = pickVar (v);
                v.lsrc = org.data (id, var);
                Ls = maxi (org.len[id][var], 64);
                srcSr = (float) ORG_SR;
                v.lrate = clampf (semis (tuneSt) * srcSr / sr, 0.0625f, 8.0f);
            }
            else
            {
                Ls = mini ((int) (0.25f * sr), LBUF_SIZE);
                srcSr = sr;
                const float f = 220.0f * semis (tuneSt);
                const float isr = 1.0f / sr;
                float pc = 0, pm = 0, pz = 0, q1 = 0, q2 = 0, q3 = 0;
                Svf bp; bp.reset(); bp.set (f * 5.0f, 6.0f, sr);
                for (int n = 0; n < Ls; ++n)
                {
                    const float prog = (float) n / (float) Ls;
                    float x = 0.0f;
                    if (si == 0)
                    {
                        pc += f * isr; pm += f * 2.71f * isr; pc -= floorf_ (pc); pm -= floorf_ (pm);
                        const float ix = 6.0f * (1.0f - prog) * (1.0f - prog) + 0.3f;
                        x = sin2pi (pc + ix * sin2pi (pm) * 0.159f) * (1.0f - 0.5f * prog);
                    }
                    else if (si == 1)
                    {
                        if ((n & 31) == 0) bp.set (f * (1.0f + 4.0f * (1.0f - prog)), 6.0f, sr);
                        float lp, b, hp; bp.tick (v.rng.bi(), lp, b, hp); x = b * bp.k * 2.5f;
                    }
                    else if (si == 2) { pz += f * 8.0f * exp2f_ (-prog * 4.0f) * isr; pz -= floorf_ (pz); x = sin2pi (pz); }
                    else
                    {
                        q1 += f * isr; q2 += f * 1.26f * isr; q3 += f * 1.5f * isr; q1 -= floorf_ (q1); q2 -= floorf_ (q2); q3 -= floorf_ (q3);
                        const float c = ((q1 < 0.5f ? 1.0f : -1.0f) + (q2 < 0.5f ? 1.0f : -1.0f) + (q3 < 0.5f ? 1.0f : -1.0f)) * 0.33f;
                        x = floorf_ (c * 7.0f + 0.5f) / 7.0f;
                    }
                    v.lbuf[n] = x * 0.8f;
                }
                v.lsrc = v.lbuf;
                v.lrate = 1.0f;
            }
            v.lsrcLen = Ls;
            // LEN at the top of its travel plays the whole source before the loops start shrinking
            const int full = P[P_B] >= 0.98f;
            v.lstart = P[P_E] * (float) Ls * (full ? 0.95f : 0.8f);
            v.lmax = full ? (float) Ls : (float) Ls * 0.5f;
            v.llen = full ? maxf ((float) Ls - v.lstart, 16.0f)
                          : clampf (0.001f * powf_ (150.0f, P[P_B]) * srcSr, 16.0f, (float) Ls * 0.5f);
            v.lphase = 0.0f; v.ldir = 1;
            break;
        }
        case M_FRACT:
        {
            // a crowd of particles: NUM onsets spread over SPRD (denser at the start), then a tail
            v.pId = idxOf (P[P_A], NUM_ORG);
            const int num = 1 + idxOf (P[P_B], MAX_BURST);
            const float spread = 0.3f * P[P_C] * P[P_C] * sr;
            v.pAt[0] = 0.0f;
            for (int k = 1; k < num; ++k)
            {
                const float u = v.rng.uni();
                const float x = u * u * spread;
                int j = k - 1;
                while (j >= 0 && v.pAt[j] > x) { v.pAt[j + 1] = v.pAt[j]; --j; }
                v.pAt[j + 1] = x;
            }
            v.pNum = num; v.pIdx = 0;
            v.tailEnv = 1.0f;
            v.pgain = 1.0f / (1.0f + 0.12f * (float) (num - 1));
            for (int k = 0; k < MAX_PART; ++k) v.part[k].on = 0;
            break;
        }
        case M_BUG:
        {
            const int sd = idxOf (P[P_F], 16);
            if (sd > 0) v.rng.seed (0xB06u + (u32) sd * 7919u);
            v.bw = 0; v.bmode = 0; v.bleft = 0; v.bread = 0; v.bflip = 0; v.bhold = 0.0f; v.lstart = 0.0f; v.llen = 0.0f;
            for (int i = 0; i < 4096; ++i) v.lbuf[i] = 0.0f;
            break;
        }
        case M_CHAOS:
        {
            const int sd = idxOf (P[P_F], 16);
            if (sd > 0) { v.cx = 0.11 + 0.037 * sd; v.cy = 0.2 + 0.011 * sd; v.cz = 0.3; }
            else { v.cx = 0.1 + 0.8 * v.rng.uni(); v.cy = 0.1 + 0.3 * v.rng.uni(); v.cz = 0.2 + 0.5 * v.rng.uni(); }
            v.ccnt = 0.0f; v.cprev = v.ccur = 0.0f;
            break;
        }
        case M_VOX: v.s3.reset(); break;
        case M_ITERA:
        {
            // centre of ATTACK and left of it: the partials start at their peak (analog pop)
            const float ph0 = P[P_E] <= 0.5f ? 0.25f : 0.0f;
            for (int k = 0; k < 6; ++k) { v.ph[k] = ph0; v.y1[k] = 1.0f; v.y2[k] = 0.0f; }
            v.aenv = P[P_E] > 0.5f ? 0.0f : 1.0f;
            v.nenv = 1.0f; v.penv = 1.0f;
            v.fb1 = 0.0f; v.fb2 = triFoldInt (0.0f);
            break;
        }
        case M_TRIO:
            // free-running VCOs: each note catches the oscillators wherever they are; a little static
            // mistuning per note and a slow drift (y1[0..2] static cents, y1[3..5] drifting cents)
            for (int k = 0; k < 3; ++k) { v.ph[k] = v.rng.uni(); v.y1[k] = v.rng.bi() * 1.5f; v.y1[k + 3] = v.rng.bi() * 0.8f; }
            v.os2.reset (HB1);
            break;
        case M_SINUS:
            // PHAS: the sine starts anywhere from 0 to 90 degrees; past 0 its first sample is a click
            v.ph[0] = P[P_D] * 0.25f; v.ph[1] = v.ph[0] * 0.5f; v.ph[2] = 0.25f;
            v.aenv = 1.0f; v.astage = 1;
            break;
        case M_DATA:
        {
            // PATN #1..15 replays the same bits and the same noise on every hit, RND draws new ones
            const int sd = idxOf (P[P_A], 16);
            if (sd > 0) v.rng.seed (0xDA7Au + (u32) sd * 7919u);
            const float dens = 0.05f + 0.95f * P[P_C];
            u32 bits = 1u;                                 // the first cell always plays: the attack
            for (int k = 1; k < 32; ++k) if (v.rng.uni() < dens) bits |= 1u << k;
            v.dbits = bits; v.didx = 0; v.dcnt = 0.0f;
            v.aenv = 1.0f; v.astage = 1;
            break;
        }
        case M_FMZ:
        case M_HARD:
            v.os2.reset (HB1);
            v.aenv = 1.0f; v.astage = 1;
            break;
        case M_MACRO:
            // a rising edge on the unit's TRIGGER (gate high for GATE, at least a few ms)
            macroTrigger (v.mac, v.holdSec);
            break;
        case M_HEXBD: case M_HEXSD: case M_HEXTM:
            hexTrigger (v, P);
            break;
        case M_ALLOY:
            alloyTrigger (v, P);
            break;
        case M_CHIMERA:
            chimTrigger (v, P);
            break;
        case M_CUBE:
            cubeTrigger (v, P);
            break;
        case M_WEST:
            westTrigger (v, P);
            break;
        case M_HALO:
            haloTrigger (v, P);
            break;
        case M_STRUM:
            strumTrigger (v, P);
            break;
        case M_HOWL:
            howlTrigger (v, P);
            break;
        case M_MANTRA:
            mantraTrigger (v, P);
            break;
        case M_RADIO:
            radioTrigger (v, P);
            break;
        case M_CHOIR:
            choirTrigger (v, P);
            break;
        case M_BREAK:
            breakTrigger (v, P);
            break;
        case M_FXVERB: case M_FXECHO: case M_FXBEND: case M_FXTAPE: case M_FXCODEC: case M_FXFUZZ: case M_FXWEFT: case M_FXSTENCIL:
            // the FX machines: the trig goes to the track's effect (its gate, its gesture); the voice stays silent
            fx[v.ti].trig = 1; fx[v.ti].trigVel = clampf (v.vel, 0.0f, 1.0f); fx[v.ti].trigNote = v.note;
            break;
        case M_RAM:
        {
            // BUF: its own buffer, or another track's
            const int b = idxOf (P[P_RBUF], NUM_TRACKS + 1);
            v.rbuf = b > 0 ? b - 1 : v.ti;
            v.rmode = idxOf (P[P_C], RAM_MODES);
            v.rdir = v.rmode == 1 ? -1 : 1;
            ramWindow (v, P);
            v.rpos = v.rdir > 0 ? (double) v.rbeg : (double) v.rend - 1.0;
            v.rdone = 0;
            v.dac.configure (idxOf (P[P_E], NUM_DAC_MODELS), RAM_DAC_AGE);
            v.dac.reset();
            // PRISM: an empty bank; its first frame comes in at once (a hit stays a hit)
            for (int j = 0; j < PR_K; ++j) { v.pAmp[j] = v.pAmpT[j] = 0.0f; v.pRe[j] = 1.0f; v.pIm[j] = 0.0f; v.pCs[j] = 1.0f; v.pSn[j] = 0.0f; v.pInc[j] = 0.0f; v.pPh[j] = 0.0f; }
            v.pAct = 0; v.pFresh = 1; v.pKey[0] = -1.0f; v.pTab = 0; v.pPk = 0.0f; v.pSlL = 0.0f; v.pSlF = 0;
            break;
        }
        default: break;
    }
}

// RAM: the part of the buffer a voice plays. STRT and LEN are fractions of the take; with SLCE, STRT picks one of
// the slices and LEN is a fraction of that slice
void Engine::ramWindow (Voice& v, const float* P) const
{
    const float L = (float) ram[v.rbuf].len;
    const int ns = RAM_SLICE_N[idxOf (P[P_D], RAM_SLICES)];
    float beg, len;
    if (ns > 1) { const float sl = L / (float) ns; beg = (float) idxOf (P[P_A], ns) * sl; len = sl * P[P_B]; }
    else { beg = P[P_A] * L; len = P[P_B] * L; }
    beg = minf (beg, maxf (L - 4.0f, 0.0f));
    v.rbeg = beg;
    v.rend = minf (beg + maxf (len, 32.0f), L);
}

// bank variant for the next hit: never the one this track played last (Fracture-style rotation)
int Engine::pickVar (Voice& v)
{
    int k = idxOf (v.rng.uni(), ORG_VAR - 1);
    if (k >= v.ovar) k++;
    v.ovar = k;
    return k;
}

void Engine::spawnParticle (Voice& v, float rate, float vari, int full, float gsec, float gain)
{
    int slot = -1;
    for (int k = 0; k < MAX_PART; ++k) if (! v.part[k].on) { slot = k; break; }
    if (slot < 0) return;
    const int var = pickVar (v);
    const int L = org.len[v.pId][var];
    if (L < 16) return;
    Particle& p = v.part[slot];
    p.src  = org.data (v.pId, var);
    p.rate = clampf (rate * semis (vari * 12.0f * v.rng.bi()), 0.0625f, 8.0f);
    const float last = (float) (L - 2);
    if (full)
    {
        p.start = 0.0f; p.end = last;
        p.fin = 0.0f; p.fout = 1.0f / maxf (0.002f * sr * p.rate, 1.0f);
    }
    else
    {
        // a grain of GRN seconds, starting further into the sample as VARI grows
        const float span = minf (gsec * sr * p.rate, last);
        p.start = vari * v.rng.uni() * (last - span);
        p.end = p.start + span;
        const float fade = maxf (minf (span * 0.5f, 0.001f * sr * p.rate), 1.0f);
        p.fin = p.start > 0.0f ? 1.0f / fade : 0.0f;
        p.fout = 1.0f / fade;
    }
    p.pos = p.start;
    p.gain = gain;
    p.on = 1;
}

// common amplitude envelope: linear attack, hold, exponential decay (v78: its curve, ECRV: envCurve)
static inline float ampTick (Voice& v, float attInc, float coef)
{
    if (v.astage == 0)      { v.aenv += attInc; if (v.aenv >= 1.0f) { v.aenv = 1.0f; v.astage = 1; } }
    else if (v.astage == 1) { if (v.aHoldLeft > 0.0f) v.aHoldLeft -= 1.0f; else v.astage = 2; }
    else                    v.aenv *= v.hrel > 0.0f ? v.hrel : coef;      // (DEC on HOLD: the gate shut, its release)
    return v.ecrv < 0.9995f ? envCurve (v.aenv, v.astage, v.ecrv) : v.aenv;
}

static inline float oscSawPulse (float ph, float dt, float wave, float pw)
{
    float saw = 2.0f * ph - 1.0f - polyblep (ph, dt);
    float t2 = ph - pw; if (t2 < 0.0f) t2 += 1.0f;
    float pulse = (ph < pw ? 1.0f : -1.0f) + polyblep (ph, dt) - polyblep (t2, dt);
    return saw + (pulse - saw) * wave;
}

static inline u32 bytebeat (int form, u32 t, u32 a, u32 b, u32 m)
{
    switch (form)
    {
        case 0:  return t * ((t >> a) | (t >> b));
        case 1:  return t * (((t >> a) | (t >> b)) & (m + 31u) & (t >> 4));
        case 2:  return ((t * m) & (t >> a)) | ((t * 3u) & (t >> b));
        case 3:  return (t * (t >> a)) ^ ((t >> b) * m);
        case 4:  return t * ((t >> a) & (t >> b));
        case 5:  return ((t * m) & (t >> a)) - ((t >> b) & t);
        case 6:  return (t * ((t >> a) | (t >> b))) >> ((t >> 14) & 3u);
        default: { u32 x = ((t >> a) | 1u) ^ (((t - 1280u) ^ t) >> b); return t * x * m; }
    }
}

// sine -> triangle -> saw -> square, band-limited saw and square (polyBLEP)
static inline float oscMorph (float ph, float dt, float m)
{
    float tt = ph + 0.25f; tt -= floorf_ (tt);
    const float tri = 1.0f - 4.0f * absf (tt - 0.5f);
    if (m < 0.33333f) { const float s = sin2pi (ph); return s + (tri - s) * m * 3.0f; }
    const float saw = 2.0f * ph - 1.0f - polyblep (ph, dt);
    if (m < 0.66667f) return tri + (saw - tri) * (m - 0.33333f) * 3.0f;
    float t2 = ph - 0.5f; if (t2 < 0.0f) t2 += 1.0f;
    const float sq = (ph < 0.5f ? 1.0f : -1.0f) + polyblep (ph, dt) - polyblep (t2, dt);
    return saw + (sq - saw) * (m - 0.66667f) * 3.0f;
}

// ----------------------------------------------------------------- machines
// AC coupling before the VCA, as on an analog voice: a 2-pole high-pass at 20 Hz on the raw source of the machines
// whose waves are not centred (bytebeat, chaotic maps, impulse trains, held samples, pulse and phase-distortion
// shapes, FM feedback). Their offset otherwise rides the envelope as an infrasonic thump (half the energy of some
// presets, below 20 Hz) that eats headroom, pumps the bus compressors and crowds the low end for nothing
static inline float acCouple (Voice& v, float x, float a1, float a2, float a3)
{
    const float v3 = x - v.acI2, v1 = a1 * v.acI1 + a2 * v3, v2 = v.acI2 + a2 * v.acI1 + a3 * v3;
    v.acI1 = 2.0f * v1 - v.acI1; v.acI2 = 2.0f * v2 - v.acI2;
    return x - 1.41421356f * v1 - v2;
}
#define AC(x) acCouple (v, (x), acA1, acA2, acA3)

void Engine::machineRender (Voice& v, const float* P, int n, float* out)
{
    const float tuneRaw = (P[P_TUNE] - 0.5f) * 48.0f + v.note;
    // (HALO and STRUM: the notes they build their chords in; the track the quantiser follows, in the scale itself)
    const u32   qm = v.quant ? (v.machine == M_HALO || v.machine == M_STRUM ? harmMask (v.ti) : quantMask()) : 0u;
    const float tuneSt = qm && BASE_HZ[v.machine] > 0.0f ? qPitch (tuneRaw, BASE_HZ[v.machine], qm) : tuneRaw;
    const float isr = 1.0f / sr;
    const float att = 1.0f / (0.0002f * sr);

    switch (v.machine)
    {
    // --- micro-looper: loops shrinking/growing windows of a rendered burst (Autechre-style stutter)
    case M_ULOOP:
    {
        const int Ls = maxi (v.lsrcLen, 64);
        const float* src = v.lsrc ? v.lsrc : v.lbuf;
        const float shrink = 0.5f + P[P_C];
        const float pr = semis ((P[P_D] - 0.5f) * 24.0f);
        const float jit = P[P_F];
        const float dc = decayCoef (0.02f * powf_ (200.0f, P[P_DEC]), sr);
        // SP-1200 reader: a 26.04 kHz clock fetches one 12-bit word per tick (drop-sample, no interpolation)
        const float tickInc = (float) SP_RATE / sr, tickStep = sr / (float) SP_RATE;
        const float oc = expf_ (-TWO_PI * minf (12500.0f, 0.45f * sr) / sr);
        for (int i = 0; i < n; ++i)
        {
            v.spPh += tickInc;
            while (v.spPh >= 1.0f)
            {
                v.spPh -= 1.0f;
                const float pos = v.ldir > 0 ? v.lphase : v.llen - v.lphase;
                float rp = v.lstart + pos;
                while (rp >= (float) Ls) rp -= (float) Ls;
                if (rp < 0.0f) rp = 0.0f;
                const float e = clampf (minf (v.lphase, v.llen - v.lphase) * 0.125f, 0.0f, 1.0f);
                v.spHold = q12 (src[(int) rp]) * e;
                v.lphase += v.lrate * tickStep;
                if (v.lphase >= v.llen)
                {
                    v.lphase -= v.llen; if (v.lphase >= v.llen) v.lphase = 0.0f;
                    v.llen = clampf (v.llen * shrink, 8.0f, maxf (v.lmax, 8.0f));
                    v.lrate = clampf (v.lrate * pr, 0.0625f, 8.0f);
                    if (jit > 0.001f)
                    {
                        v.lstart += v.rng.bi() * jit * v.llen * 2.0f;
                        while (v.lstart < 0.0f) v.lstart += (float) Ls;
                        while (v.lstart >= (float) Ls) v.lstart -= (float) Ls;
                        v.ldir = v.rng.uni() < jit * 0.5f ? -1 : 1;
                    }
                }
            }
            // zero-order hold, then the fixed output filter (two poles)
            v.spLp1 = v.spHold + (v.spLp1 - v.spHold) * oc;
            v.spLp2 = v.spLp1 + (v.spLp2 - v.spLp1) * oc;
            out[i] = v.spLp2 * 2.0f * ampTick (v, att, dc);
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
    // --- multi-particle percussion (after the WMD Fracture): a crowd of organic grains
    case M_FRACT:
    {
        const float base = semis (tuneSt) * (float) ORG_SR / sr;
        const float vari = P[P_E];
        const float dens = 300.0f * P[P_D] * P[P_D];
        const float tailC = decayCoef (0.05f * powf_ (100.0f, P[P_DEC]), sr);
        const int   full = P[P_F] >= 0.98f;
        const float gsec = 0.001f * powf_ (300.0f, P[P_F]);
        const float pTail = dens / sr;
        const float tickInc = (float) SP_RATE / sr, tickStep = sr / (float) SP_RATE;
        const float oc = expf_ (-TWO_PI * minf (12500.0f, 0.45f * sr) / sr);
        for (int i = 0; i < n; ++i)
        {
            while (v.pIdx < v.pNum && v.pAt[v.pIdx] <= (float) v.ns)
            {
                spawnParticle (v, base, vari, full, gsec, v.pgain * (1.0f - 0.5f * vari * v.rng.uni()));
                v.pIdx++;
            }
            if (pTail > 0.0f && v.tailEnv > 1e-3f && v.rng.uni() < pTail * v.tailEnv)
                spawnParticle (v, base, vari, full, gsec, v.pgain * v.tailEnv * (0.4f + 0.6f * v.rng.uni()));
            v.tailEnv *= tailC;
            // SP-1200 reader: every particle fetches a 12-bit word on the 26.04 kHz clock (drop-sample)
            v.spPh += tickInc;
            while (v.spPh >= 1.0f)
            {
                v.spPh -= 1.0f;
                float acc = 0.0f;
                for (int k = 0; k < MAX_PART; ++k)
                {
                    Particle& p = v.part[k];
                    if (! p.on) continue;
                    float w = 1.0f;
                    if (p.fin > 0.0f) w = minf (w, (p.pos - p.start) * p.fin);
                    w = minf (w, (p.end - p.pos) * p.fout);
                    acc += q12 (p.src[(int) p.pos]) * w * p.gain;
                    p.pos += p.rate * tickStep;
                    if (p.pos >= p.end) p.on = 0;
                }
                v.spHold = acc;
            }
            v.spLp1 = v.spHold + (v.spLp1 - v.spHold) * oc;
            v.spLp2 = v.spLp1 + (v.spLp2 - v.spLp1) * oc;
            out[i] = v.spLp2 * 2.0f;
            v.ns++;
        }
        int busy = v.pIdx < v.pNum || (pTail > 0.0f && v.tailEnv > 1e-3f);
        for (int k = 0; k < MAX_PART && ! busy; ++k) busy = v.part[k].on;
        v.level = busy ? 1.0f : 0.0f;
        break;
    }
    // --- buffer errors: an oscillator through a broken playback buffer (repeats, dropouts, bit flips)
    case M_BUG:
    {
        const float f = 110.0f * semis (tuneSt), dt = minf (f * isr, 0.45f);
        const float src = P[P_A] * 4.0f; const int s0 = mini ((int) src, 3); const float sf = src - (float) s0;
        const float chunk = 0.002f * sr * powf_ (40.0f, P[P_B]);
        const float rpt = P[P_C], drop = P[P_D], flip = P[P_E];
        const float dc = decayCoef (0.02f * powf_ (200.0f, P[P_DEC]), sr);
        static const float MUL[4] = { 0.25f, 0.5f, 1.0f, 1.5f };
        for (int i = 0; i < n; ++i)
        {
            float ph = v.ph[0] + dt; if (ph >= 1.0f) ph -= 1.0f; v.ph[0] = ph;
            float w[5];
            w[0] = sin2pi (ph);
            float tt = ph + 0.25f; tt -= floorf_ (tt); w[1] = 1.0f - 4.0f * absf (tt - 0.5f);
            w[2] = 2.0f * ph - 1.0f - polyblep (ph, dt);
            float t2 = ph - 0.5f; if (t2 < 0.0f) t2 += 1.0f;
            w[3] = (ph < 0.5f ? 1.0f : -1.0f) + polyblep (ph, dt) - polyblep (t2, dt);
            w[4] = sin2pi (ph + 0.8f * sin2pi (ph * 7.13f + v.lp1));
            v.lp1 += 0.0007f;
            const float x = w[s0] + (w[s0 + 1] - w[s0]) * sf;
            v.lbuf[v.bw] = x;
            if (v.bleft <= 0)
            {
                const int len = maxi (16, (int) (chunk * MUL[v.rng.next() & 3u]));
                const float r = v.rng.uni();
                const int prevMode = v.bmode;
                if (r < rpt)
                {
                    v.bmode = 1;
                    if (prevMode != 1) { v.llen = (float) len; v.lstart = (float) ((v.bw - len) & LBUF_MASK); }
                    v.bread = (int) v.lstart;
                    v.bleft = (int) v.llen;
                }
                else
                {
                    v.bmode = r < rpt + drop * (1.0f - rpt) ? (v.rng.uni() < 0.5f ? 2 : 3) : 0;
                    v.bleft = len;
                }
                v.bflip = v.rng.uni() < flip ? (int) ((v.rng.next() & 0x7F00u) | 0x100u) : 0;
            }
            v.bw = (v.bw + 1) & LBUF_MASK;
            float y;
            switch (v.bmode)
            {
                case 1:  y = v.lbuf[v.bread]; v.bread = (v.bread + 1) & LBUF_MASK; break;
                case 2:  y = 0.0f; break;
                case 3:  y = v.bhold; break;
                default: y = x; break;
            }
            if (v.bflip) { int q = (int) (clampf (y, -1.0f, 1.0f) * 32767.0f); q ^= v.bflip; y = (float) (short) q * (1.0f / 32768.0f); }
            if (v.bmode != 3) v.bhold = y;
            v.bleft--;
            out[i] = AC (y) * 0.75f * ampTick (v, att, dc);
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
    // --- chaotic maps at audio rate: logistic, Hénon, Lorenz, Ikeda
    case M_CHAOS:
    {
        const int map = idxOf (P[P_A], 4);
        const float ch = P[P_B];
        const float rate0 = 1500.0f * semis (tuneSt);
        const float sm = 1.0f - P[P_C] * 0.97f;
        const float self = P[P_D];
        const float fold = P[P_E];
        const float fg = 1.0f + fold * 5.0f, fmix = minf (1.0f, fold * 4.0f);
        const float dc = decayCoef (0.02f * powf_ (200.0f, P[P_DEC]), sr);
        for (int i = 0; i < n; ++i)
        {
            const float inc = clampf (rate0 * (1.0f + self * v.lp1 * 2.0f) * isr, 0.0f, 4.0f);
            v.ccnt += inc;
            while (v.ccnt >= 1.0f)
            {
                v.ccnt -= 1.0f; v.cprev = v.ccur;
                double x = v.cx, y = v.cy, z = v.cz, o;
                switch (map)
                {
                    case 0: { const double r = 3.56 + ch * 0.44; x = r * x * (1.0 - x); if (x <= 0.0 || x >= 1.0) x = 0.3 + 0.4 * v.rng.uni(); o = 2.0 * x - 1.0; break; }
                    case 1: { const double a = 1.0 + ch * 0.42; const double nx = 1.0 - a * x * x + y; y = 0.3 * x; x = nx;
                              if (x > 4.0 || x < -4.0) { x = 0.1; y = 0.1; } o = x * 0.7; break; }
                    case 2: { const double rho = 20.0 + ch * 30.0, dt = 0.008;
                              const double dx = 10.0 * (y - x), dy = x * (rho - z) - y, dz = x * y - 2.6666667 * z;
                              x += dx * dt; y += dy * dt; z += dz * dt;
                              if (x > 200.0 || x < -200.0 || z > 300.0) { x = 0.1; y = 0.0; z = 20.0; } o = x / 22.0; break; }
                    default: { const double u = 0.6 + ch * 0.39; const double tt = 0.4 - 6.0 / (1.0 + x * x + y * y);
                               const float c = cos2pi ((float) (tt * 0.159154943)), s = sin2pi ((float) (tt * 0.159154943));
                               const double nx = 1.0 + u * (x * c - y * s), ny = u * (x * s + y * c); x = nx; y = ny;
                               o = (x - 0.6) * 0.8; break; }
                }
                v.cx = x; v.cy = y; v.cz = z;
                v.ccur = clampf ((float) o, -1.5f, 1.5f);
            }
            const float yv = v.cprev + (v.ccur - v.cprev) * v.ccnt;
            v.lp1 += sm * (yv - v.lp1);
            float o = v.lp1;
            if (fmix > 0.0f) o = o + (sin2pi (0.25f * o * fg) - o) * fmix;
            out[i] = AC (o) * 1.7f * ampTick (v, att, dc);
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
    // --- formant percussion: glottal saw + breath through three vowel resonators with a sweep
    case M_VOX:
    {
        static const float VW[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
        const float f0 = 110.0f * semis (tuneSt), dt = minf (f0 * isr, 0.45f);
        const float pos = clampf (P[P_A] + (P[P_B] - 0.5f) * 2.0f * (1.0f - v.senv), 0.0f, 1.0f) * 4.0f;
        const int vi = mini ((int) pos, 3); const float vf = pos - (float) vi;
        const float shift = exp2f_ ((P[P_F] - 0.5f) * 2.0f);
        const float q = 2.0f + P[P_D] * P[P_D] * 18.0f;
        const float nz = P[P_E];
        v.senv *= powf_ (tcCoef (0.005f * powf_ (200.0f, P[P_C]), sr), (float) n);
        v.s1.set ((VW[vi][0] + (VW[vi + 1][0] - VW[vi][0]) * vf) * shift, q, sr);
        v.s2.set ((VW[vi][1] + (VW[vi + 1][1] - VW[vi][1]) * vf) * shift, q, sr);
        v.s3.set ((VW[vi][2] + (VW[vi + 1][2] - VW[vi][2]) * vf) * shift, q, sr);
        const float comp = 0.6f + q * 0.12f;
        const float dc = decayCoef (0.02f * powf_ (200.0f, P[P_DEC]), sr);
        for (int i = 0; i < n; ++i)
        {
            float ph = v.ph[0] + dt; if (ph >= 1.0f) ph -= 1.0f; v.ph[0] = ph;
            const float saw = 2.0f * ph - 1.0f - polyblep (ph, dt);
            const float ex = saw * (1.0f - nz) + v.rng.bi() * nz * 0.8f;
            float lp, b1, b2, b3, hp;
            v.s1.tick (ex, lp, b1, hp); v.s2.tick (ex, lp, b2, hp); v.s3.tick (ex, lp, b3, hp);
            const float o = (b1 * v.s1.k + b2 * v.s2.k * 0.7f + b3 * v.s3.k * 0.45f) * comp * 3.2f;
            out[i] = o * ampTick (v, att, dc);
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
    // --- phase distortion (Casio CZ): 8 waves incl. resonant windows, DCW envelope, ring with line 2
    case M_CZPD:
    {
        static const float R2[8] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 7.0f };
        const float f = 110.0f * semis (tuneSt), dt = minf (f * isr, 0.45f);
        const int wave = idxOf (P[P_A], 8);
        const float dcw = P[P_B], denv = P[P_C];
        const float ec = decayCoef (0.005f * powf_ (400.0f, P[P_D]), sr);
        const float ring = P[P_E], r2 = R2[idxOf (P[P_F], 8)];
        const float dc = decayCoef (0.02f * powf_ (200.0f, P[P_DEC]), sr);
        for (int i = 0; i < n; ++i)
        {
            const float d = clampf (dcw + denv * v.menv, 0.0f, 0.99f); v.menv *= ec;
            float p = v.ph[0] + dt; if (p >= 1.0f) p -= 1.0f; v.ph[0] = p;
            float p2 = v.ph[1] + dt * r2; p2 -= floorf_ (p2); v.ph[1] = p2;
            float y;
            switch (wave)
            {
                case 0: { const float x0 = 0.5f - 0.49f * d; y = cos2pi (p < x0 ? 0.5f * p / x0 : 0.5f + 0.5f * (p - x0) / (1.0f - x0)); break; }
                case 1: { const float w = 0.5f - 0.49f * d; const float hp = p < 0.5f ? p : p - 0.5f;
                          y = cos2pi ((p < 0.5f ? 0.0f : 0.5f) + minf (1.0f, hp / w) * 0.5f); break; }
                case 2: { const float w = 1.0f - 0.98f * d; y = cos2pi (minf (1.0f, p / w)); break; }
                case 3: { const float x0 = 0.5f - 0.49f * d; y = cos2pi (2.0f * (p < x0 ? 0.5f * p / x0 : 0.5f + 0.5f * (p - x0) / (1.0f - x0))); break; }
                case 4: { const float w = 0.5f - 0.49f * d; y = cos2pi (p < 0.5f ? p : 0.5f + minf (0.5f, (p - 0.5f) / w * 0.5f)); break; }
                case 5: y = (1.0f - p) * cos2pi (p * (1.0f + d * 15.0f)); break;
                case 6: y = (1.0f - absf (2.0f * p - 1.0f)) * cos2pi (p * (1.0f + d * 15.0f)); break;
                default: y = minf (1.0f, 2.0f * (1.0f - p)) * cos2pi (p * (1.0f + d * 15.0f)); break;
            }
            const float o = y * (1.0f - ring) + y * cos2pi (p2) * ring;
            out[i] = AC (o) * 0.9f * ampTick (v, att, dc);
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
    // --- analog kick: sine/tri with exponential pitch sweep, click, and a warm drive stage rendered at 2x:
    //     the lows are pushed harder than the click (pre-emphasis), an asymmetric transistor-like curve whose
    //     bias follows the signal (even harmonics, a little sag on the loud part), then a post low-pass that
    //     closes as the drive rises, the way the output stage of an analog machine rolls the fizz off.
    case M_BD_VA:
    {
        const float sr2 = 2.0f * sr, isr2 = 0.5f * isr;
        const float f0 = 50.0f * semis (tuneSt);
        const float wave = P[P_A], swOct = P[P_B] * 5.0f;
        const float pc = tcCoef (0.001f * powf_ (200.0f, P[P_C]), sr2);
        const float click = P[P_D];
        const float drv = P[P_E];
        // the drive fades in over its first 6 % (and from one chunk to the next) instead of switching on: its
        // stage has gain and a bias of its own, so a hard switch clicked
        const float mT = wetOf (drv), m0 = v.smOk ? v.mWet : mT, dm = (mT - m0) / (float) n;
        const bool warm = mT > 0.0f || m0 > 0.0f, prime = warm && m0 <= 0.0f;
        v.mWet = mT;
        const float dg = 1.0f + drv * drv * 14.0f;
        const float b0 = 0.15f + 0.35f * drv;                            // operating point: more asymmetry when pushed
        const float lowB = 1.2f * drv;                                    // extra drive for the body below ~180 Hz
        const float aLo = 1.0f - expf_ (-TWO_PI * 180.0f / sr2);
        const float aEnv = 1.0f - expf_ (-1.0f / (0.012f * sr2));
        const float fPost = minf (16000.0f - 9000.0f * drv, 0.45f * sr2);
        const float aPost = 1.0f - expf_ (-TWO_PI * fPost / sr2);
        const float dc = decayCoef (0.02f * powf_ (200.0f, P[P_DEC]), sr);
        const float cc = tcCoef (0.0025f, sr2);
        const float invN = warm ? 1.0f / (tanhf_ (dg * (1.0f + lowB * 0.5f) + b0) - tanhf_ (b0)) : 1.0f;
        float m = m0;
        // CLIK, the beater: the noise burst into the body (it knocks the drive) and, after the drive and its
        // rounding, a tick and a noise burst through a band-pass around 3 kHz (the attack you hear in a mix), 2 ms
        const float ckG = click * click * 10.0f, kc = tcCoef (0.002f, sr2);
        const float bg = tanpi (3000.0f / sr2), bk = 1.2f, ba1 = 1.0f / (1.0f + bg * (bg + bk)), ba2 = bg * ba1, ba3 = bg * ba2;
        for (int i = 0; i < n; ++i)
        {
            const float a = ampTick (v, att, dc);
            m += dm;
            float xs[2];
            for (int k = 0; k < 2; ++k)
            {
                const float f = f0 * exp2f_ (swOct * v.penv); v.penv *= pc;
                v.ph[0] += f * isr2; v.ph[0] -= floorf_ (v.ph[0]);
                const float sn = sin2pi (v.ph[0]);
                float tt = v.ph[0] + 0.25f; tt -= floorf_ (tt);
                const float tri = 1.0f - 4.0f * absf (tt - 0.5f);
                float o = sn + (tri - sn) * wave;
                const float cl = (v.ns == 0 && k == 0 ? 1.0f : 0.0f) + v.rng.bi() * 0.5f;
                o += cl * click * v.cenv * 1.2f; v.cenv *= cc;
                if (warm)
                {
                    // (starting: the stage's filters begin where the signal is, not where they were left)
                    if (prime && i == 0 && k == 0) { v.wLo = o; v.wEnv = absf (o + lowB * o); }
                    v.wLo += (o - v.wLo) * aLo;
                    const float u = o + lowB * v.wLo;
                    v.wEnv += (absf (u) - v.wEnv) * aEnv;
                    const float b = b0 * (0.6f + 0.6f * minf (v.wEnv, 1.0f));
                    float y = (tanhf_ (dg * u + b) - tanhf_ (b)) * invN;
                    if (prime && i == 0 && k == 0) v.wPost = v.wPost2 = y;
                    v.wPost += (y - v.wPost) * aPost;
                    v.wPost2 += (v.wPost - v.wPost2) * aPost;
                    o += (v.wPost2 - o) * m;
                }
                // the beater's envelope runs from the hit whether CLIK is up or not: turning it up mid-note
                // does not fire a burst
                // (the noise is drawn either way: the voice's random stream stays the same whatever CLIK does)
                const float rn = v.rng.bi();
                if (ckG > 0.0f)
                {
                    const float xn = (v.ns == 0 && k == 0 ? 6.0f : 0.0f) + rn;
                    const float v3 = xn - v.y2[0], v1 = ba1 * v.y1[0] + ba2 * v3, v2 = v.y2[0] + ba2 * v.y1[0] + ba3 * v3;
                    v.y1[0] = 2.0f * v1 - v.y1[0]; v.y2[0] = 2.0f * v2 - v.y2[0];
                    o += bk * v1 * ckG * v.ienv;
                }
                v.ienv *= kc;
                xs[k] = o * a;
            }
            out[i] = v.os2.process (xs[0], xs[1]);
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
    // --- FM kick: 2-op with modulator feedback, pitch sweep and wavefolder
    case M_BD_FM:
    {
        float f0 = 50.0f * semis (tuneSt);
        float ratio = RATIOS[idxOf (P[P_A], NUM_RATIOS)];
        float I = P[P_B] * P[P_B] * 16.0f;
        float mc = decayCoef (0.002f * powf_ (500.0f, P[P_C]), sr);
        float fb = P[P_D] * 1.6f;
        float swOct = P[P_E] * 4.0f;
        float pc = tcCoef (0.04f, sr);
        float fg = 1.0f + P[P_F] * 5.0f, fmix = minf (1.0f, P[P_F] * 4.0f);
        float dc = decayCoef (0.02f * powf_ (200.0f, P[P_DEC]), sr);
        for (int i = 0; i < n; ++i)
        {
            float f = f0 * exp2f_ (swOct * v.penv); v.penv *= pc;
            v.ph[1] += f * ratio * isr; v.ph[1] -= floorf_ (v.ph[1]);
            float mod = sin2pi (v.ph[1] + fb * 0.5f * (v.fb1 + v.fb2) * 0.15915f);
            v.fb2 = v.fb1; v.fb1 = mod;
            v.ph[0] += f * isr; v.ph[0] -= floorf_ (v.ph[0]);
            float o = sin2pi (v.ph[0] + I * v.menv * mod * 0.15915f);
            v.menv *= mc;
            if (fmix > 0.0f) o = o + (sin2pi (0.25f * o * fg) - o) * fmix;
            out[i] = o * ampTick (v, att, dc);
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
    // --- analog snare: two tuned sines with snap + filtered noise
    case M_SD_VA:
    {
        float f0 = 180.0f * semis (tuneSt);
        float r2 = 1.2f + P[P_A] * 1.2f;
        float bal = P[P_B];
        float nc = decayCoef (0.01f * powf_ (100.0f, P[P_C]), sr);
        v.s1.set (500.0f * powf_ (24.0f, P[P_D]), 0.5f + P[P_F] * P[P_F] * 8.0f, sr);
        float snap = P[P_E] * 2.0f;
        float pc = tcCoef (0.012f, sr);
        float dc = decayCoef (0.02f * powf_ (100.0f, P[P_DEC]), sr);
        for (int i = 0; i < n; ++i)
        {
            float pe = exp2f_ (snap * v.penv); v.penv *= pc;
            v.ph[0] += f0 * pe * isr;      v.ph[0] -= floorf_ (v.ph[0]);
            v.ph[1] += f0 * r2 * pe * isr; v.ph[1] -= floorf_ (v.ph[1]);
            float body = 0.65f * sin2pi (v.ph[0]) + 0.45f * sin2pi (v.ph[1]);
            float a = ampTick (v, att, dc);
            float lp, bp, hp; v.s1.tick (v.rng.bi(), lp, bp, hp);
            float nz = (hp * 0.6f + bp * v.s1.k * 0.8f) * v.nenv; v.nenv *= nc;
            out[i] = body * a * (1.0f - bal) + nz * bal * 1.5f;
            v.ns++;
        }
        v.level = maxf (v.aenv, v.nenv);
        break;
    }
    // --- FM snare: FM body, noise also frequency-modulates the carrier
    case M_SD_FM:
    {
        float f0 = 200.0f * semis (tuneSt);
        float ratio = RATIOS[idxOf (P[P_A], NUM_RATIOS)];
        float I = P[P_B] * P[P_B] * 12.0f;
        float mc = decayCoef (0.002f * powf_ (500.0f, P[P_C]), sr);
        float nlev = P[P_D];
        float nc = decayCoef (0.01f * powf_ (100.0f, P[P_E]), sr);
        v.s1.set (800.0f * powf_ (18.0f, P[P_F]), 0.9f, sr);
        float pc = tcCoef (0.01f, sr);
        float dc = decayCoef (0.02f * powf_ (100.0f, P[P_DEC]), sr);
        for (int i = 0; i < n; ++i)
        {
            float pe = exp2f_ (v.penv); v.penv *= pc;
            float nz = v.rng.bi();
            v.ph[1] += f0 * ratio * pe * isr; v.ph[1] -= floorf_ (v.ph[1]);
            float mod = sin2pi (v.ph[1]);
            v.ph[0] += f0 * pe * isr; v.ph[0] -= floorf_ (v.ph[0]);
            float c = sin2pi (v.ph[0] + (I * v.menv * mod + nlev * v.nenv * nz * 1.5f) * 0.15915f);
            v.menv *= mc;
            float a = ampTick (v, att, dc);
            float lp, bp, hp; v.s1.tick (nz, lp, bp, hp);
            float nn = (hp * 0.7f + bp * v.s1.k * 0.6f) * v.nenv * nlev; v.nenv *= nc;
            out[i] = c * a * 0.85f + nn * 1.3f;
            v.ns++;
        }
        v.level = maxf (v.aenv, v.nenv * nlev);
        break;
    }
    // --- metallic: six band-limited pulse oscillators (808 ratios) -> BP -> HP
    case M_HH_MT:
    {
        float tr_ = semis (tuneSt);
        float spread = 0.2f + 1.6f * P[P_A];
        float dt[6];
        for (int k = 0; k < 6; ++k)
            dt[k] = minf (205.3f * tr_ * (1.0f + (B808[k] / 205.3f - 1.0f) * spread) * isr, 0.45f);
        float color = 3000.0f * powf_ (4.6667f, P[P_B]);
        v.s1.set (color, 1.4f, sr);
        v.s2.set (color * 0.75f, 0.7f, sr);
        float nmix = P[P_C], ring = P[P_D], pw = 0.5f - P[P_E] * 0.42f, trans = P[P_F];
        float dc = decayCoef (0.005f * powf_ (400.0f, P[P_DEC]), sr);
        float cc = tcCoef (0.0015f, sr);
        for (int i = 0; i < n; ++i)
        {
            float sq[6];
            for (int k = 0; k < 6; ++k)
            {
                float ph = v.ph[k] + dt[k]; if (ph >= 1.0f) ph -= 1.0f; v.ph[k] = ph;
                float t2 = ph - pw; if (t2 < 0.0f) t2 += 1.0f;
                sq[k] = (ph < pw ? 1.0f : -1.0f) + polyblep (ph, dt[k]) - polyblep (t2, dt[k]);
            }
            float sum = (sq[0] + sq[1] + sq[2] + sq[3] + sq[4] + sq[5]) * 0.16667f;
            float rg  = (sq[0] * sq[1] + sq[2] * sq[3] + sq[4] * sq[5]) * 0.33333f;
            float m = sum + (rg - sum) * ring;
            m = m + (v.rng.bi() - m) * nmix;
            float lp, bp, hp; v.s1.tick (m, lp, bp, hp);
            float b = bp * v.s1.k;
            v.s2.tick (b, lp, bp, hp);
            float a = ampTick (v, att, dc);
            out[i] = hp * (a * 3.8f + trans * v.cenv * 4.2f);
            v.cenv *= cc;
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
    // --- clap: multi-burst noise envelope through a resonant band-pass + tail
    case M_CP_NZ:
    {
        float center = 1100.0f * semis (tuneSt);
        float q = 0.7f + P[P_C] * P[P_C] * 10.0f;
        v.s1.set (center, q, sr);
        float spacing = 0.003f * powf_ (12.0f, P[P_B]);
        float bc = tcCoef (spacing * 0.3f, sr);
        float lastDec = 0.02f * powf_ (60.0f, P[P_DEC]);
        float lc = decayCoef (lastDec, sr);
        float tc = decayCoef (lastDec * 4.0f + 0.1f, sr);
        float tail = P[P_D], grit = P[P_F];
        for (int i = 0; i < n; ++i)
        {
            if (v.burstIdx < v.nBursts && (float) v.ns >= v.burstAt[v.burstIdx])
            {
                v.benv = 1.0f; v.burstIdx++;
                if (v.burstIdx == v.nBursts) v.tenv = tail * 0.6f;
            }
            bool last = v.burstIdx >= v.nBursts;
            v.benv *= last ? lc : bc;
            v.tenv *= tc;
            float nz = v.rng.bi();
            float d = v.rng.uni() < 0.04f ? (v.rng.bi() > 0.0f ? 3.0f : -3.0f) : 0.0f;
            float src = nz + (d - nz) * grit;
            float lp, bp, hp; v.s1.tick (src, lp, bp, hp);
            out[i] = bp * v.s1.k * (v.benv + v.tenv) * 5.5f;
            v.ns++;
        }
        v.level = (v.burstIdx >= v.nBursts) ? maxf (v.benv, v.tenv) : 1.0f;
        break;
    }
    // --- extreme percussive FM: 3 operators, 4 algorithms, feedback chaos
    case M_FM_X:
    {
        float f1 = 110.0f * semis (tuneSt);
        int algo = idxOf (P[P_A], 4);
        float r2 = RATIOS[idxOf (P[P_B], NUM_RATIOS)], r3 = RATIOS[idxOf (P[P_C], NUM_RATIOS)];
        float I = P[P_D] * P[P_D] * 24.0f * 0.15915f;
        float idec = 0.001f * powf_ (2000.0f, P[P_E]);
        float ic = decayCoef (idec, sr), i3c = decayCoef (idec * 0.4f, sr);
        float fb = P[P_F] * 2.0f;
        float dc = decayCoef (0.01f * powf_ (400.0f, P[P_DEC]), sr);
        for (int i = 0; i < n; ++i)
        {
            v.ph[0] += f1 * isr;      v.ph[0] -= floorf_ (v.ph[0]);
            v.ph[1] += f1 * r2 * isr; v.ph[1] -= floorf_ (v.ph[1]);
            v.ph[2] += f1 * r3 * isr; v.ph[2] -= floorf_ (v.ph[2]);
            float I1 = I * v.ienv, I3 = I * v.i3env;
            float fbp = fb * 0.5f * (v.fb1 + v.fb2) * 0.15915f;
            float o1, o2, o3, o;
            switch (algo)
            {
                case 0:  o3 = sin2pi (v.ph[2] + fbp); o2 = sin2pi (v.ph[1] + I3 * o3);
                         o1 = sin2pi (v.ph[0] + I1 * o2); o = o1; break;
                case 1:  o3 = sin2pi (v.ph[2] + fbp); o2 = sin2pi (v.ph[1]);
                         o1 = sin2pi (v.ph[0] + (I1 * o2 + I3 * o3) * 0.7f); o = o1; break;
                case 2:  o3 = sin2pi (v.ph[2] + fbp); o2 = sin2pi (v.ph[1] + I3 * o3);
                         o1 = sin2pi (v.ph[0] + I1 * o3 * 0.5f); o = (o1 + o2 * 0.7f) * 0.65f; break;
                default: o3 = sin2pi (v.ph[2] + fb * v.lastOut * 0.3f); o2 = sin2pi (v.ph[1] + I3 * o3);
                         o1 = sin2pi (v.ph[0] + I1 * o2); o = o1; v.lastOut = o1; break;
            }
            v.fb2 = v.fb1; v.fb1 = o3;
            v.ienv *= ic; v.i3env *= i3c;
            out[i] = AC (o) * 1.4f * ampTick (v, att, dc);
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
    // --- noise generator: white/pink/LFSR/dust, sample&hold, swept SVF, grains
    case M_NOISE:
    {
        float fc0 = 1000.0f * exp2f_ ((P[P_TUNE] - 0.5f) * 8.0f + v.note * (1.0f / 12.0f));
        float type = P[P_A] * 3.0f; int ti = mini ((int) type, 2); float tf = type - (float) ti;
        float shInc = 80.0f * powf_ (sr / 80.0f, P[P_B]) * isr;
        float swOct = (P[P_C] - 0.5f) * 12.0f;
        float scoef = decayCoef (0.005f * powf_ (400.0f, P[P_D]), sr);
        float q = 0.5f + P[P_E] * P[P_E] * 20.0f;
        float gran = P[P_F];
        float fc = clampf (fc0 * exp2f_ (swOct * v.senv), 20.0f, 0.45f * sr);
        v.s1.set (fc, q, sr);
        v.senv *= powf_ (scoef, (float) n);
        float lpMix = 0.5f * (1.0f - P[P_E]);
        float dc = decayCoef (0.01f * powf_ (400.0f, P[P_DEC]), sr);
        for (int i = 0; i < n; ++i)
        {
            v.shCnt += shInc;
            if (v.shCnt >= 1.0f)
            {
                v.shCnt -= floorf_ (v.shCnt);
                float w = v.rng.bi();
                v.lp1 += 0.08f * (w - v.lp1); v.lp2 += 0.02f * (w - v.lp2);
                float pink = v.lp1 * 2.5f + v.lp2 * 2.0f;
                u32 bit = (v.lfsr ^ (v.lfsr >> 1)) & 1u;
                v.lfsr = (v.lfsr >> 1) | (bit << 14);
                float lf = (v.lfsr & 1u) ? 1.0f : -1.0f;
                float dust = v.rng.uni() < 0.03f ? v.rng.bi() * 3.0f : 0.0f;
                float src[4] = { w, pink, lf, dust };
                v.shVal = src[ti] + (src[ti + 1] - src[ti]) * tf;
            }
            float x = v.shVal;
            if (gran > 0.001f)
            {
                v.grainLeft -= 1.0f;
                if (v.grainLeft <= 0.0f)
                {
                    v.grainLeft = (0.001f + v.rng.uni() * 0.025f) * sr;
                    v.gate = v.rng.uni() < (1.0f - gran * 0.85f) ? 1.0f : 0.0f;
                }
                v.gateS += (v.gate - v.gateS) * 0.05f;
                x *= v.gateS;
            }
            float lp, bp, hp; v.s1.tick (x, lp, bp, hp);
            float o = bp * v.s1.k * 0.8f + lp * lpMix;
            out[i] = AC (o) * 4.0f * ampTick (v, att, dc);
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
    // --- impulse train: bouncing/accelerating clicks -> resonator -> comb
    case M_IMPLS:
    {
        float f = minf (220.0f * semis (tuneSt), 0.45f * sr);
        float accel = 0.6f + P[P_C] * 0.8f;
        float reso = P[P_D];
        float combFb = P[P_E] * 0.97f;
        float jit = P[P_F];
        float rr = decayCoef (0.0005f * powf_ (800.0f, reso), sr);
        float w = f * isr;
        float a1 = 2.0f * rr * cos2pi (w), a2 = rr * rr, b = sin2pi (w);
        float del = clampf (sr / f, 2.0f, (float) (COMB_SIZE - 4));
        int di = (int) del; float dfr = del - (float) di;
        float dc = decayCoef (0.02f * powf_ (200.0f, P[P_DEC]), sr);
        for (int i = 0; i < n; ++i)
        {
            float x = 0.0f;
            if (v.impLeft > 0 && (float) v.ns >= v.nextImp)
            {
                float amp = v.impAmp;
                if (jit > 0.0f)
                {
                    amp *= 1.0f - jit * v.rng.uni() * 0.7f;
                    if (v.rng.uni() < jit * 0.5f) amp = -amp;
                }
                x = amp;
                v.impAmp *= 0.92f;
                v.curInt *= accel;
                v.nextImp += maxf (v.curInt * (1.0f + jit * (v.rng.uni() - 0.5f) * 0.8f), 1.0f);
                v.impLeft--;
            }
            float y = a1 * v.y1[0] - a2 * v.y2[0] + b * x;
            v.y2[0] = v.y1[0]; v.y1[0] = y;
            float o = x * (1.0f - reso * 0.6f) * 1.5f + y * reso * 1.2f;
            int r0 = (v.mcw - di) & COMB_MASK, r1 = (v.mcw - di - 1) & COMB_MASK;
            float d = v.mcomb[r0] + (v.mcomb[r1] - v.mcomb[r0]) * dfr;
            float c = o + combFb * d;
            v.mcomb[v.mcw] = c; v.mcw = (v.mcw + 1) & COMB_MASK;
            o = combFb > 0.001f ? c * (1.0f - combFb * 0.5f) : o;
            out[i] = AC (o) * 3.0f * ampTick (v, att, dc);
            v.ns++;
        }
        v.level = v.impLeft > 0 ? 1.0f : v.aenv;
        break;
    }
    // --- virtual analog: saw/pulse, detune, hard sync, swarm, sub
    case M_SY_RAW:
    {
        float f1 = 65.41f * semis (tuneSt);
        float wave = P[P_A];
        float cents = P[P_B] * P[P_B] * 50.0f;
        float sync = P[P_C];
        float swarm = P[P_D];
        float sub = P[P_E];
        float pw = 0.5f - P[P_F] * 0.45f;
        float r2 = semis (cents * 0.01f);
        if (sync > 0.01f) r2 *= 1.0f + sync * 7.0f;
        float dt1 = minf (f1 * isr, 0.45f), dt2 = minf (f1 * r2 * isr, 0.45f);
        float spc = (5.0f + swarm * 35.0f) * 0.01f;
        float swr[4] = { semis (spc), semis (-spc), semis (spc * 0.5f), semis (-spc * 0.5f) };
        float a = 1.0f / (0.0015f * sr);
        float dc = decayCoef (0.01f * powf_ (400.0f, P[P_DEC]), sr);
        for (int i = 0; i < n; ++i)
        {
            v.ph[0] += dt1;
            bool wrapped = false; float wfrac = 0.0f;
            if (v.ph[0] >= 1.0f) { v.ph[0] -= 1.0f; wrapped = true; wfrac = v.ph[0] / dt1; }
            float o1 = oscSawPulse (v.ph[0], dt1, wave, pw);
            if (sync > 0.01f && wrapped) v.ph[1] = wfrac * dt2;
            else { v.ph[1] += dt2; if (v.ph[1] >= 1.0f) v.ph[1] -= 1.0f; }
            float o2 = oscSawPulse (v.ph[1], dt2, wave, pw);
            v.ph[2] += dt1 * 0.5f; if (v.ph[2] >= 1.0f) v.ph[2] -= 1.0f;
            float sb = oscSawPulse (v.ph[2], dt1 * 0.5f, 1.0f, 0.5f);
            float sw = 0.0f;
            if (swarm > 0.001f)
                for (int k = 0; k < 4; ++k)
                {
                    float dk = minf (dt1 * swr[k], 0.45f);
                    v.ph[3 + k] += dk; if (v.ph[3 + k] >= 1.0f) v.ph[3 + k] -= 1.0f;
                    sw += 2.0f * v.ph[3 + k] - 1.0f - polyblep (v.ph[3 + k], dk);
                }
            float o = (o1 + o2) * 0.5f + sw * swarm * 0.22f + sb * sub * 0.6f;
            out[i] = AC (o) * 1.7f * ampTick (v, a, dc);
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
    // --- additive / FM drum voice after the Noise Engineering Basimilus Iteritas: six partials with their
    //     own envelopes. SPREAD harmonic -> prime -> free-bar series, HARM fades in the 2nd partial, then
    //     lengthens the decays, then raises the upper four; MORPH sine/tri/saw/square; FOLD infinite folder
    //     (anti-aliased) then a pulse train; ATTACK noise / pop / slow; models SKIN / LIQUID (pitch env) /
    //     METAL (partials phase-modulate each other in a ring). DEC at max = free-running.
    case M_ITERA:
    {
        static const float HAR[6] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
        static const float PRI[6] = { 1.0f, 2.0f, 3.0f, 5.0f, 7.0f, 11.0f };
        static const float BAR[6] = { 1.0f, 2.756f, 5.404f, 8.933f, 13.344f, 18.64f };
        const float f0 = 65.41f * semis (tuneSt);
        const float sp = P[P_A], h = P[P_B], morph = P[P_C], fold = P[P_D], atk = P[P_E];
        const int model = idxOf (P[P_F], 3);
        const float dsec = 0.012f * powf_ (300.0f, P[P_DEC]);
        const bool freeRun = P[P_DEC] > 0.985f;
        const float hd = clampf (h * 3.0f - 1.0f, 0.0f, 1.0f);
        float rat[6], amp[6], ec[6], asum = 0.0f;
        for (int k = 0; k < 6; ++k)
        {
            rat[k] = sp < 0.5f ? HAR[k] + (PRI[k] - HAR[k]) * sp * 2.0f : PRI[k] + (BAR[k] - PRI[k]) * (sp - 0.5f) * 2.0f;
            amp[k] = k == 0 ? 1.0f : k == 1 ? clampf (h * 3.0f, 0.0f, 1.0f) * 0.8f
                               : clampf (h * 3.0f - 2.0f, 0.0f, 1.0f) * 0.75f / sqrtf_ ((float) k);
            const float dm = k == 0 ? 1.0f : (0.15f + 0.85f * hd) / (1.0f + 0.3f * (float) k);
            ec[k] = freeRun ? (v.gateShut ? v.hrel : 1.0f) : decayCoef (dsec * dm, sr);   // (free: until let go)
            asum += amp[k];
        }
        const float norm = 1.0f / (0.6f + 0.4f * asum), invA = 1.0f / asum;
        const float noiseAmt = atk < 0.5f ? (0.5f - atk) * 2.0f : 0.0f;
        const float atkInc = atk > 0.5f ? 1.0f / (0.0005f * powf_ (400.0f, (atk - 0.5f) * 2.0f) * sr) : 1.0f;
        const float nc = decayCoef (0.006f + 0.05f * noiseAmt, sr);
        const float pc = tcCoef (0.045f, sr);
        const float fa = minf (fold * (4.0f / 3.0f), 1.0f);
        const float th = 1.0f - 0.88f * fa, fin = 1.0f / th, fcomp = 0.55f + 0.45f * th;
        bool foldWas = v.smOk && v.mWet > 0.0f;          // the folder ran in the last chunk (its ADAA state is current)
        v.mWet = fa > 0.002f ? 1.0f : 0.0f;
        const float pulse = maxf (0.0f, (fold - 0.75f) * 4.0f);
        const float fmI = model == 2 ? 0.35f + 1.4f * h : 0.0f;
        float outG = model == 2 ? 1.7f : 1.15f;
        {
            const float fd = clampf (fold, 0.0f, 1.0f) * 4.0f, fb = clampf (h, 0.0f, 1.0f) * 4.0f, fcm = clampf (morph, 0.0f, 1.0f) * 6.0f;
            const int d0 = mini ((int) fd, 3), b0 = mini ((int) fb, 3), c0 = mini ((int) fcm, 5);
            const float dx = fd - (float) d0, bx = fb - (float) b0, cx = fcm - (float) c0;
            float db = 0.0f;
            for (int a = 0; a < 2; ++a) for (int b = 0; b < 2; ++b) for (int c = 0; c < 2; ++c)
                db += ITERA_COMP[model][d0 + a][b0 + b][c0 + c] * (a ? dx : 1.0f - dx) * (b ? bx : 1.0f - bx) * (c ? cx : 1.0f - cx);
            outG *= exp2f_ (db * 0.16609640f);                          // dB -> gain
        }
        for (int i = 0; i < n; ++i)
        {
            float pm = 1.0f;
            if (model == 1) { pm = exp2f_ (1.6f * v.penv); v.penv *= pc; }
            if (v.aenv < 1.0f) { v.aenv += atkInc; if (v.aenv > 1.0f) v.aenv = 1.0f; }
            float sum = 0.0f, esum = 0.0f;
            const float last = v.y2[0];
            for (int k = 0; k < 6; ++k)
            {
                const float f = f0 * rat[k] * pm * isr;
                if (f >= 0.45f) { v.y2[k] = 0.0f; continue; }      // above Nyquist: silent
                v.ph[k] += f; v.ph[k] -= floorf_ (v.ph[k]);
                float ph = v.ph[k];
                if (fmI > 0.0f) { ph += (k < 5 ? v.y2[k + 1] : last) * fmI; ph -= floorf_ (ph); }
                const float o = oscMorph (ph, f, morph) * v.y1[k] * amp[k];
                v.y2[k] = o;
                esum += v.y1[k] * amp[k];
                if (v.aenv >= 1.0f) v.y1[k] *= ec[k];          // attack, then decay
                sum += o;
            }
            sum = sum * norm + v.rng.bi() * noiseAmt * v.nenv * 0.6f;
            v.nenv *= nc;
            sum *= v.aenv;
            float y = sum;
            if (fa > 0.002f)
            {
                const float x = sum * fin;
                if (! foldWas) { v.fb1 = x; v.fb2 = triFoldInt (x) ; foldWas = true; }   // starting: no stale step
                const float dx = x - v.fb1, F = triFoldInt (x);
                y = (absf (dx) > 1e-4f ? (F - v.fb2) / dx : triFold (0.5f * (x + v.fb1))) * fcomp;
                v.fb1 = x; v.fb2 = F;
                if (pulse > 0.0f) y += pulse * (tanhf_ (y * 5.0f) * 0.8f - y) * 0.7f;
                y *= 1.0f - fa + fa * esum * invA;      // envelope shape re-applied after the folder
            }
            out[i] = y * outG;
            v.level = esum * invA * v.aenv + v.nenv * noiseAmt;
            v.ns++;
        }
        break;
    }
    // --- TRIO, a small Prophet (the Pro-One / Prophet-5 rev3 lineage), rendered at 2x. Three CEM3340-style
    //     VCOs: rising ramp (10 Vpp), triangle folded from it (5 Vpp), comparator pulse (13.5 Vpp, louder),
    //     free-running phases, per-note mistuning and slow drift. WAVE: TRI > SAW > SAW+PULSE > SQUARE >
    //     narrow PULSE. MODE: FREE (C = detune), SYNC (2 and 3 hard-synced to 1, C = sweep that falls with the
    //     filter envelope), RING (1 x 2, plus 3), PMOD (poly-mod: osc 3 and the filter envelope bend osc 1).
    //     The mixer saturates softly into the filter; with the track filter on LP, TRIO gets a 4-pole ladder.
    case M_TRIO:
    {
        const float sr2 = 2.0f * sr, isr2 = 0.5f * isr;
        const int mode = idxOf (P[P_F], 4);
        const float st2 = (float) (idxOf (P[P_A], 49) - 24), st3 = (float) (idxOf (P[P_B], 49) - 24);
        const bool envMod = mode == 1 || mode == 3;
        const float c = P[P_C];
        const float det = envMod ? 0.0f : c * c * 0.5f;
        const float sweep = envMod ? c * c * 24.0f : 0.0f;
        const float sc = decayCoef (0.005f * powf_ (400.0f, P[P_FDEC]), sr2);
        // (quantised: OSC2 and OSC3 in steps of the scale over the note, as a chord in it; never a tritone or a minor
        // second over the note where the scale has better)
        const float t2 = qm ? qInterval (tuneSt, (int) st2, 65.41f, qm) : tuneSt + st2;
        const float t3 = qm ? qInterval (tuneSt, (int) st3, 65.41f, qm) : tuneSt + st3;
        // drift: a slow random walk of about a cent per oscillator, updated per block
        for (int k = 0; k < 3; ++k) v.y1[k + 3] = clampf (v.y1[k + 3] * 0.995f + v.rng.bi() * 0.06f, -1.5f, 1.5f);
        const float c1 = (v.y1[0] + v.y1[3]) * 0.01f, c2 = (v.y1[1] + v.y1[4]) * 0.01f, c3 = (v.y1[2] + v.y1[5]) * 0.01f;
        const float f1b = 65.41f * semis (tuneSt + c1);
        const float f2b = 65.41f * semis (t2 + det + c2), f3b = 65.41f * semis (t3 - det + c3);
        // WAVE: weights of triangle, ramp and pulse, and the pulse width
        const float w = P[P_D] * 4.0f;
        float wT = 0.0f, wS = 0.0f, wP = 0.0f, pw = 0.5f;
        if (w < 1.0f)      { wT = 1.0f - w; wS = w; }
        else if (w < 2.0f) { wS = 1.0f; wP = w - 1.0f; }
        else if (w < 3.0f) { wS = 3.0f - w; wP = 1.0f; }
        else               { wP = 1.0f; pw = 0.5f - 0.4f * (w - 3.0f); }
        wT *= 0.5f; wP *= 1.35f;                                           // CEM3340 output levels relative to the ramp
        const float pdc = (2.0f * pw - 1.0f) * wP;
        const float mix = P[P_E];
        const float a = 1.0f / (0.0005f * sr);
        const float dc = decayCoef (0.01f * powf_ (400.0f, P[P_DEC]), sr);
        const float fmax = 0.45f * sr2;
        auto vco = [&] (float ph, float dt) {
            const float ramp = 2.0f * ph - 1.0f - polyblep (ph, dt);
            float o = 0.0f;
            if (wS > 0.0f) o += wS * ramp;
            if (wT > 0.0f) o += wT * (1.0f - 2.0f * absf (2.0f * ph - 1.0f)) * 2.0f - wT;   // folded from the ramp
            if (wP > 0.0f)
            {
                float tp = ph - pw; if (tp < 0.0f) tp += 1.0f;
                o += wP * ((ph < pw ? 1.0f : -1.0f) + polyblep (ph, dt) - polyblep (tp, dt));
            }
            return o - pdc;
        };
        for (int i = 0; i < n; ++i)
        {
            const float amp = ampTick (v, a, dc);
            float xs[2];
            for (int k = 0; k < 2; ++k)
            {
                const float es = v.senv; v.senv *= sc;
                float f1 = f1b, f2 = f2b, f3 = f3b;
                if (mode == 1) { const float sw = semis (sweep * es); f2 *= sw; f3 *= sw; }
                float o3m = 0.0f;
                if (mode == 3)
                {
                    o3m = sin2pi (v.ph[2]);
                    f1 *= exp2f_ (mix * 2.0f * o3m) * semis (sweep * es);
                }
                const float dt1 = minf (f1 * isr2, 0.45f), dt2 = minf (f2 * isr2, 0.45f), dt3 = minf (f3 * isr2, 0.45f);
                v.ph[0] += dt1;
                bool wrapped = false; float wfrac = 0.0f;
                if (v.ph[0] >= 1.0f) { v.ph[0] -= 1.0f; wrapped = true; wfrac = v.ph[0] / dt1; }
                if (mode == 1 && wrapped) { v.ph[1] = wfrac * dt2; v.ph[2] = wfrac * dt3; }
                else
                {
                    v.ph[1] += dt2; if (v.ph[1] >= 1.0f) v.ph[1] -= 1.0f;
                    v.ph[2] += dt3; if (v.ph[2] >= 1.0f) v.ph[2] -= 1.0f;
                }
                const float o1 = vco (v.ph[0], dt1);
                float o;
                switch (mode)
                {
                    case 1:  o = o1 * (1.0f - 0.6f * mix) + (vco (v.ph[1], dt2) + vco (v.ph[2], dt3)) * 0.6f * mix; break;
                    case 2:  o = o1 * (1.0f - 0.45f * mix) + (o1 * vco (v.ph[1], dt2) * 0.8f + vco (v.ph[2], dt3)) * 0.6f * mix; break;
                    case 3:  o = o1 + vco (v.ph[1], dt2) * 0.5f; break;
                    default: o = o1 * (1.0f - 0.45f * mix) + (vco (v.ph[1], dt2) + vco (v.ph[2], dt3)) * 0.6f * mix; break;
                }
                // mixer into the filter input: soft saturation once the waves pile up
                o = tanhf_ (o * 0.5f) * 1.6f;
                (void) fmax;
                xs[k] = o;
            }
            out[i] = AC (v.os2.process (xs[0], xs[1]));   // (the VCA is in the voice strip, after the ladder)
            v.vca[i] = amp;
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
    // --- pure sine (Pan Sonic): sub octave, pitch drop, ring modulator, start phase (click), exponential
    //     to square gate, gated high noise. No filter, no drive: every sample is a sine value.
    case M_SINUS:
    {
        const float f0 = 110.0f * semis (tuneSt);
        const float sub = P[P_A], dropOct = P[P_B] * 5.0f;
        const float pc = tcCoef (0.004f + 0.04f * P[P_B], sr);
        const float rr = RING_R[idxOf (P[P_C], NUM_RING)];
        const float gate = P[P_E], noise = P[P_F];
        const float T = 0.01f * powf_ (400.0f, P[P_DEC]);
        const float dc = decayCoef (T, sr);
        const float holdS = T * 0.3f * sr + v.holdSec * sr, relInv = 1.0f / (0.0015f * sr);
        const float Tn = clampf (T * 0.3f, 0.005f, 0.4f);
        const float nc = decayCoef (Tn, sr), nHold = Tn * 0.3f * sr + v.holdSec * sr;
        const float fmax = 0.45f * sr;
        const float norm = 1.0f / (1.0f + 0.55f * sub);
        if (noise > 0.0f) v.s1.set (minf (5000.0f, 0.4f * sr), 0.6f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float f = minf (dropOct > 0.0f ? f0 * exp2f_ (dropOct * v.penv) : f0, fmax);
            v.penv *= pc;
            v.ph[0] += f * isr;        v.ph[0] -= floorf_ (v.ph[0]);
            v.ph[1] += 0.5f * f * isr; v.ph[1] -= floorf_ (v.ph[1]);
            float s = sin2pi (v.ph[0]);
            if (rr > 0.0f)
            {
                v.ph[2] += minf (f * rr, fmax) * isr; v.ph[2] -= floorf_ (v.ph[2]);
                s *= cos2pi (v.ph[2] - 0.25f);
            }
            const float ns = (float) v.ns;
            const float aExp = ampTick (v, att, dc);
            const float aSq = ns < holdS ? 1.0f : maxf (0.0f, 1.0f - (ns - holdS) * relInv);
            const float a = aExp + (aSq - aExp) * gate;
            float nz = 0.0f, ne = 0.0f;
            if (noise > 0.0f)
            {
                const float nSq = ns < nHold ? 1.0f : maxf (0.0f, 1.0f - (ns - nHold) * relInv);
                ne = v.nenv + (nSq - v.nenv) * gate; v.nenv *= nc;
                float lp, bp, hp; v.s1.tick (v.rng.bi(), lp, bp, hp);
                nz = hp * ne * noise * 0.9f;
            }
            out[i] = (s + sub * sin2pi (v.ph[1])) * norm * a + nz;
            v.level = maxf (a, ne * noise);
            v.ns++;
        }
        break;
    }
    // --- binary data (Ryoji Ikeda / Alva Noto): a 1..32-bit pattern read cell by cell; a 1 lets the
    //     content through with hard edges (noise -> pulse -> sine at the tone pitch), a 0 is silence.
    //     Tiny cells turn the pattern into a pitched digital buzz. SUB: pure sine five octaves down.
    case M_DATA:
    {
        const float ft = minf (1760.0f * semis (tuneSt), 0.45f * sr);
        const float dt = ft * isr;
        const float cell = maxf (1.0f, 0.00005f * powf_ (1000.0f, P[P_B]) * sr);
        const float tone = P[P_D], sub = P[P_F];
        const int len = 1 + idxOf (P[P_E], 32);
        const float dc = decayCoef (0.01f * powf_ (300.0f, P[P_DEC]), sr);
        const float sc = decayCoef (0.04f + 1.2f * P[P_DEC] * P[P_DEC], sr);
        for (int i = 0; i < n; ++i)
        {
            v.dcnt += 1.0f;
            if (v.dcnt >= cell) { v.dcnt -= cell; if (++v.didx >= len) v.didx = 0; }
            const bool on = (v.dbits >> v.didx) & 1u;
            float ph = v.ph[0] + dt; ph -= floorf_ (ph); v.ph[0] = ph;
            float x = 0.0f;
            const float nz = v.rng.bi();               // drawn every sample so a seeded pattern stays identical
            if (on)
            {
                float t2 = ph - 0.5f; if (t2 < 0.0f) t2 += 1.0f;
                const float pulse = ((ph < 0.5f ? 1.0f : -1.0f) + polyblep (ph, dt) - polyblep (t2, dt)) * 0.8f;
                x = tone < 0.5f ? nz + (pulse - nz) * tone * 2.0f : pulse + (sin2pi (ph) - pulse) * (tone - 0.5f) * 2.0f;
            }
            v.ph[1] += dt * (1.0f / 32.0f); v.ph[1] -= floorf_ (v.ph[1]);
            const float a = ampTick (v, att, dc);
            const float se = v.senv; v.senv *= sc;
            out[i] = x * 0.85f * a + sin2pi (v.ph[1]) * sub * se;
            v.level = maxf (a, se * sub);
            v.ns++;
        }
        break;
    }
    // --- wild FM (2x oversampled): carrier + modulator at RATIO, index spike decaying in IDEC, ZAP pitch
    //     dive on both, XMOD carrier -> modulator cross-feedback while the index is up (chaos only in the
    //     attack), and a clean sub sine an octave below with its own longer decay.
    case M_FMZ:
    {
        const float sr2 = 2.0f * sr, isr2 = 0.5f * isr;
        const float f0 = 110.0f * semis (tuneSt);
        const float R = RATIOS[idxOf (P[P_A], NUM_RATIOS)];
        const float Ipk = 40.0f * P[P_B] * P[P_B];
        const float ic = decayCoef (0.0003f * powf_ (2000.0f, P[P_C]), sr2);
        const float zapOct = 7.0f * P[P_D];
        const float zc = tcCoef (0.0015f + 0.012f * P[P_D], sr2);
        const float sub = P[P_E], xm = P[P_F] * 2.5f * 0.15915f;
        const float T = 0.01f * powf_ (400.0f, P[P_DEC]);
        const float dc = decayCoef (T, sr), sdc = decayCoef (T * 1.6f + 0.02f, sr);
        const float nyq = 0.45f * sr2, fcap = 0.4f * sr2;
        const float dsub = 0.5f * f0 * isr2;
        for (int i = 0; i < n; ++i)
        {
            const float a = ampTick (v, att, dc);
            const float sa = v.senv; v.senv *= sdc;
            float xs[2];
            for (int k = 0; k < 2; ++k)
            {
                const float f = minf (zapOct > 0.0f ? f0 * exp2f_ (zapOct * v.penv) : f0, fcap);
                v.penv *= zc;
                const float fm = f * R;
                const float I = minf (Ipk * v.ienv, maxf (0.0f, nyq / fm - 1.0f));   // Carson: keep the sidebands under Nyquist
                const float xmod = xm * v.ienv * v.lastOut;
                v.ienv *= ic;
                v.ph[1] += fm * isr2; v.ph[1] -= floorf_ (v.ph[1]);
                const float mod = sin2pi (v.ph[1] + xmod);
                v.ph[0] += f * isr2; v.ph[0] -= floorf_ (v.ph[0]);
                const float c = sin2pi (v.ph[0] + I * mod * 0.15915f);
                v.lastOut = c;
                v.ph[2] += dsub; v.ph[2] -= floorf_ (v.ph[2]);
                xs[k] = c * a + sin2pi (v.ph[2]) * sub * sa;
            }
            out[i] = v.os2.process (xs[0], xs[1]) * 0.85f;
            v.level = maxf (a, sa * sub);
            v.ns++;
        }
        break;
    }
    // --- industrial hardcore kick (2x oversampled): sine with a deep fast sweep and noise grit into a
    //     shaper (SOFT tanh / HARD clip / FOLD sine fold / STEP 5-level crush), plus a clean sub sine
    //     added after the shaper so the low end stays round under the distortion.
    case M_HARD:
    {
        const float sr2 = 2.0f * sr, isr2 = 0.5f * isr;
        const float f0 = 50.0f * semis (tuneSt);
        const float swOct = 7.0f * P[P_A], subOct = minf (swOct, 1.0f);
        const float pc = tcCoef (0.001f * powf_ (80.0f, P[P_B]), sr2);
        const float dg = powf_ (60.0f, P[P_C]);
        const float shp = P[P_D] * 3.0f; const int si = mini ((int) shp, 2); const float sf = shp - (float) si;
        const float nois = P[P_E], ndc = decayCoef (0.004f + 0.12f * P[P_E] * P[P_E], sr2);
        const float sub = P[P_F];
        const float T = 0.02f * powf_ (200.0f, P[P_DEC]);
        const float dc = decayCoef (T, sr), sdc = decayCoef (T * 1.4f + 0.03f, sr);
        const float fcap = 0.4f * sr2;
        for (int i = 0; i < n; ++i)
        {
            const float a = ampTick (v, att, dc);
            const float sa = v.senv; v.senv *= sdc;
            float xs[2];
            for (int k = 0; k < 2; ++k)
            {
                const float pe = v.penv; v.penv *= pc;
                v.ph[0] += minf (f0 * exp2f_ (swOct * pe), fcap) * isr2; v.ph[0] -= floorf_ (v.ph[0]);
                v.ph[1] += f0 * exp2f_ (subOct * pe) * isr2;            v.ph[1] -= floorf_ (v.ph[1]);
                const float nz = nois > 0.0f ? v.rng.bi() * nois * v.nenv : 0.0f;
                v.nenv *= ndc;
                const float u = (sin2pi (v.ph[0]) + nz) * dg;
                const float cl = clampf (u, -1.0f, 1.0f);
                float y0, y1;
                switch (si)
                {
                    case 0:  y0 = (tanhf_ (u + 0.3f) - 0.29131f) * 0.9f; y1 = cl; break;   // warm: biased, even harmonics
                    case 1:  y0 = cl; y1 = sin2pi (u * 0.25f); break;
                    default: y0 = sin2pi (u * 0.25f); y1 = floorf_ (cl * 2.0f + 0.5f) * 0.5f; break;
                }
                xs[k] = (y0 + (y1 - y0) * sf) * a * 0.8f + sin2pi (v.ph[1]) * sub * sa;
            }
            out[i] = v.os2.process (xs[0], xs[1]);
            v.level = maxf (a, sa * sub);
            v.ns++;
        }
        break;
    }
    // --- MACRO: Plaits 1.2 (sk_macro.cpp). A = model, B / C / D = HARMONICS / TIMBRE / MORPH, E = the TIMBRE
    //     attenuverter on the internal envelope, F = OUT -> AUX, DEC = Plaits' DECAY; the velocity is the accent
    case M_MACRO:
    {
        MacroControls c;
        c.model = idxOf (P[P_A], MACRO_MODELS);
        c.note = 48.0f + tuneSt;
        c.harmonics = P[P_B]; c.timbre = P[P_C]; c.morph = P[P_D];
        c.timbreEnv = (P[P_E] - 0.5f) * 2.0f;
        c.aux = P[P_F];
        c.decay = v.hold ? 0.3f : P[P_DEC];   // (HOLD: the gate holds the low-pass gate open; let go, it closes)
        c.hold = v.hold;
        c.colour = 0.5f;
        c.accent = clampf (v.vel, 0.0f, 1.0f);
        v.level = macroRender (v.mac, c, out, n);
        v.ns += (u32) n;
        break;
    }
    // --- HEX·BD / SD / TM: sk_kit.cpp
    case M_HEXBD: case M_HEXSD: case M_HEXTM:
        hexRender (v, P, tuneRaw, qm, n, out);
        break;
    // --- ALLOY (sk_alloy.cpp)
    case M_ALLOY:
        alloyRender (v, P, tuneSt, n, out);
        break;
    // --- CHIMERA (sk_chimera.cpp): a sample of the bank, a neighbour of it (FAMILLE), grafted on another (GREFFE)
    case M_CHIMERA:
        chimRender (v, P, tuneSt, n, out);
        break;
    // --- CUBE (sk_cube.cpp): a dual wavetable voice in a cube of 512 waves
    case M_CUBE:
        cubeRender (v, P, tuneSt, n, out);
        break;
    // --- WEST (sk_west.cpp): a west coast voice; its spring runs on the track (trackPost)
    case M_WEST:
        westRender (v, P, tuneSt, n, out);
        break;
    // --- HOWL (sk_howl.cpp): a guitar before a screaming amp, its own chords; it builds them itself
    case M_HOWL:
        howlRender (v, P, tuneSt, n, out);
        break;
    // --- MANTRA (sk_mantra.cpp): eight voices, their supply, the hyper LFO, the delay, the distortion
    case M_MANTRA:
        mantraRender (v, P, tuneSt, n, out);
        break;
    // --- RADIO (sk_radio.cpp): a receiver, live, looped or dug into; its air
    case M_RADIO:
        radioRender (v, P, tuneSt, n, out);
        break;
    // --- HALO: chords for pads (sk_halo.cpp); it builds them in its scale itself
    case M_HALO:
        haloRender (v, P, tuneRaw, qm, n, out);
        break;
    // --- STRUM: strummed chords on plucked strings (sk_strum.cpp), HALO's harmony
    case M_STRUM:
        strumRender (v, P, tuneRaw, qm, n, out);
        break;
    // --- CHOIR: a sung voice (sk_choir.cpp); its SPACE runs on the track (trackPost)
    case M_CHOIR:
        choirRender (v, P, tuneRaw, tuneSt, qm, n, out);
        break;
    // --- BREAK: the break slicer (sk_break.cpp); TUNE is its speed, NOTE moves the slice
    case M_BREAK:
        breakRender (v, P, n, out);
        break;
    // --- the FX machines play nothing themselves (their effect runs on the track: trackPost)
    case M_FXVERB: case M_FXECHO: case M_FXBEND: case M_FXTAPE: case M_FXCODEC: case M_FXFUZZ: case M_FXWEFT: case M_FXSTENCIL:
        for (int i = 0; i < n; ++i) out[i] = 0.0f;
        v.level = 0.0f;
        break;
    // --- RAM: plays its buffer (or another track's, BUF) at TUNE + NOTE, over its window (STRT, LEN, SLCE), forward,
    //     reversed, looped or ping-pong; DEC fades it (HOLD at the top); DAC: a converter model on the way out.
    //     Cubic Hermite between the stored samples; short fades where the window cuts the sound.
    case M_RAM:
    {
        const RamBuf& rb = ram[v.rbuf];
        const short* m = ramMem[v.rbuf];
        const int Lb = rb.len;
        ramWindow (v, P);
        const double span = (double) v.rend - (double) v.rbeg;
        if (v.rdone || Lb < 4 || span < 4.0)
        {
            for (int i = 0; i < n; ++i) out[i] = 0.0f;
            v.rdone = 1; v.level = 0.0f; v.ns += (u32) n;
            break;
        }
        const float dc = P[P_DEC] >= 0.995f ? 1.0f : decayCoef (0.02f * powf_ (300.0f, P[P_DEC]), sr);
        // PRISM, on when its MIX is up: the playhead then runs at SPED (0 = frozen) and TUNE only transposes the
        // resynthesis; off, the take plays as a sample (TUNE sets its speed). The plain-JS fallback (lite) has no PRISM
        const float mix = lite ? 0.0f : P[P_PMIX];
        const bool prism = mix > 0.001f;
        const float spd = P[P_PSPD] < 0.02f ? 0.0f : exp2f_ ((P[P_PSPD] - 0.5f) * 6.0f);
        const double inc = (double) ((prism ? spd : semis (tuneSt)) * rb.rate / sr);
        const float fade = minf (0.002f * rb.rate, (float) span * 0.25f), fadeInv = 1.0f / maxf (fade, 1.0f);
        const double beg = v.rbeg, end = v.rend;
        // a window that moved away from the voice (STRT turned, an LFO): a loop jumps back in
        if (v.rmode >= 2 && (v.rpos < beg || v.rpos >= end)) v.rpos = v.rdir > 0 ? beg : end - 1.0;
        if (prism) prismBank (v, P, tuneSt, n);
        const float dry = prism ? 1.0f - mix : 1.0f;
        const float K = 1.0f / 32768.0f;
        for (int i = 0; i < n; ++i)
        {
            if (v.rdone) { out[i] = 0.0f; v.ns++; continue; }
            const double pp = v.rpos;
            float y = 0.0f;
            if (dry > 0.0005f)
            {
                const int i1 = (int) pp;
                const float t = (float) (pp - (double) i1);
                const float h0 = (float) m[i1 > 0 ? i1 - 1 : 0] * K, h1 = (float) m[i1 < Lb ? i1 : Lb - 1] * K;
                const float h2 = (float) m[i1 + 1 < Lb ? i1 + 1 : Lb - 1] * K, h3 = (float) m[i1 + 2 < Lb ? i1 + 2 : Lb - 1] * K;
                const float c1 = 0.5f * (h2 - h0), c2 = h0 - 2.5f * h1 + 2.0f * h2 - 0.5f * h3, c3 = 0.5f * (h3 - h0) + 1.5f * (h1 - h2);
                y = (((c3 * t + c2) * t + c1) * t + h1) * dry;
            }
            if (prism) y += prOut[i] * mix;
            // FWD / REV fade out at the end of their travel, LOOP at both edges; PING turns round without a cut
            float edge = 1.0f;
            if (v.rmode == 0)      edge = (float) (end - pp) * fadeInv;
            else if (v.rmode == 1) edge = (float) (pp - beg) * fadeInv;
            else if (v.rmode == 2) edge = (float) minf_d (pp - beg, end - pp) * fadeInv;
            out[i] = y * clampf (edge, 0.0f, 1.0f) * ampTick (v, att, dc);
            v.ns++;
            if (v.rdir > 0)
            {
                v.rpos += inc;
                if (v.rpos >= end)
                {
                    if (v.rmode == 2) { while (v.rpos >= end) v.rpos -= span; }
                    else if (v.rmode == 3) { v.rpos = 2.0 * end - v.rpos; v.rdir = -1; if (v.rpos < beg) v.rpos = beg; }
                    else v.rdone = 1;
                }
            }
            else
            {
                v.rpos -= inc;
                if (v.rpos < beg)
                {
                    if (v.rmode == 3) { v.rpos = 2.0 * beg - v.rpos; v.rdir = 1; if (v.rpos >= end) v.rpos = end - 1.0; }
                    else v.rdone = 1;
                }
            }
        }
        v.dac.configure (idxOf (P[P_E], NUM_DAC_MODELS), RAM_DAC_AGE);
        v.dac.processMono (out, n);
        v.level = v.rdone ? 0.0f : v.aenv;
        break;
    }
    // --- bytebeat: integer formulas clocked at a tunable rate
    case M_BYTE:
    default:
    {
        float cr = 8000.0f * exp2f_ ((P[P_TUNE] - 0.5f) * 4.0f + v.note * (1.0f / 12.0f));
        float inc = cr * isr;
        int form = idxOf (P[P_A], 8);
        u32 a = (u32) idxOf (P[P_B], 16), b = (u32) idxOf (P[P_C], 16);
        u32 mask = (1u << (u32) (1 + idxOf (P[P_D], 8))) - 1u;
        u32 m = 1u + (u32) idxOf (P[P_E], 32);
        float lpc = 1.0f - P[P_F] * 0.97f;
        float inv = 2.0f / (float) mask;
        float dc = decayCoef (0.02f * powf_ (200.0f, P[P_DEC]), sr);
        for (int i = 0; i < n; ++i)
        {
            v.btc += inc;
            while (v.btc >= 1.0f)
            {
                v.btc -= 1.0f; v.bt++;
                u32 r = bytebeat (form, v.bt, a, b, m) & mask;
                v.btOut = (float) r * inv - 1.0f;
            }
            v.lp1 += (v.btOut - v.lp1) * lpc;
            out[i] = AC (v.lp1) * ampTick (v, att, dc);
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
    }
}

// ----------------------------------------------------------------- parameters with locks + LFOs
void Engine::effParams (const Track& t, const float* lk, float* P) const
{
    for (int p = 0; p < NUM_PARAMS; ++p) P[p] = lk[p] >= 0.0f ? lk[p] : t.p[p];
    // v90: the modulators' routes, in order (M1's four, then M2's...: the LFOs then the followers, as before); their
    // destinations, depths and AMT read before any modulation lands (AMT with the routes that land on it: a modulator's
    // amount moved by another)
    float dep[MOD_N][MOD_ROUTES], amt[MOD_N]; int dst[MOD_N][MOD_ROUTES];
    for (int m = 0; m < MOD_N; ++m)
    {
        amt[m] = modded (t, lk, MOD_AM[m]);
        for (int r = 0; r < MOD_ROUTES; ++r) { dst[m][r] = modDest (P[MOD_DST[m][r]]); dep[m][r] = (P[MOD_DEP[m][r]] - 0.5f) * 2.0f * amt[m]; }
    }
    for (int m = 0; m < MOD_N; ++m)
    {
        const float mv = modValue (t, m);
        for (int r = 0; r < MOD_ROUTES; ++r)
            if (dst[m][r] >= 0) P[dst[m][r]] = clampf (P[dst[m][r]] + mv * dep[m][r], 0.0f, 1.0f);
    }
}

static void lfoAdvance (Lfo& l, Rng& rng, float spdP, float wavP, float steps, const LfoMore& m, float phas = 0.0f, float hz = 0.0f, float secs = 0.0f)
{
    const int wv = idxOf (wavP, NUM_LFO_WAVES), shape = wv % LFO_SHAPES;
    const bool trigMode = wv >= LFO_SHAPES;
    // (v90: on the tempo, as it always was; FREE: hz cycles a second)
    l.ph += hz > 0.0f ? hz * secs : steps / LFO_STEPS[idxOf (spdP, NUM_LFO_SPEEDS)];
    if (trigMode && shape == LW_EXP) { if (l.ph > 64.0f) l.ph = 64.0f; }
    else if (l.ph >= 1.0f) { l.ph -= floorf_ (l.ph); l.from = l.val; l.sh = rng.bi(); l.k = l.k >= (1 << 30) ? 0 : l.k + 1; }
    // (v90: PHAS moves the periodic waves along their cycle)
    const float ph = phas > 0.0f && shape <= LW_SQR ? wrap01 (l.ph + phas) : l.ph;
    l.lp = -1.0f;
    switch (shape)
    {
        case LW_SIN:  l.val = sin2pi (ph); break;
        case LW_TRI:  l.val = 1.0f - 4.0f * absf (ph - 0.5f); break;
        case LW_SAW:  l.val = 2.0f * ph - 1.0f; break;
        case LW_SQR:  l.val = ph < 0.5f ? 1.0f : -1.0f; break;
        case LW_SH:   l.val = l.sh; break;
        case LW_SLEW: l.val = slewAt (l, m.slw); break;
        case LW_LOOP: l.val = loopAt (l, m); break;
        default:      l.val = exp2f_ (-ph * 6.0f); break;
    }
}

// ----------------------------------------------------------------- the modulators (v90)
// a setting as the modulators move it (the routes of this chunk that land on it): what a modulator reads of its own
// settings, and AMT; untouched, the setting itself
float Engine::modded (const Track& t, const float* lk, int p) const
{
    float v = lk[p] >= 0.0f ? lk[p] : t.p[p];
    bool on = false;
    for (int i = 0; i < t.rN; ++i) if (t.rDst[i] == p) { v += modValue (t, t.rSrc[i]) * t.rAmt[i]; on = true; }
    return on ? clampf (v, 0.0f, 1.0f) : v;
}
// each track's routes for this chunk, from the settings of the step its current voice plays
void Engine::modPrep()
{
    for (int ti = 0; ti < NUM_TRACKS; ++ti)
    {
        Track& t = tr[ti];
        const float* lk = t.voice[t.cur].lk;
        auto Q = [&] (int p) { return lk[p] >= 0.0f ? lk[p] : t.p[p]; };
        t.rN = 0;
        for (int m = 0; m < MOD_N; ++m)
            for (int r = 0; r < MOD_ROUTES; ++r)
            {
                const int d = modDest (Q (MOD_DST[m][r]));
                if (d < 0) continue;
                t.rDst[t.rN] = (short) d; t.rSrc[t.rN] = (signed char) m; t.rAmt[t.rN] = (Q (MOD_DEP[m][r]) - 0.5f) * 2.0f * Q (MOD_AM[m]);
                ++t.rN;
            }
    }
}
// the salt of a modulator's draws (WAVE's LOOP: M1 and M2 as LFO 1 and 2 were; RANDOM its own)
static inline unsigned waveSalt (int m) { return m == 0 ? 1u : m == 1 ? 2u : 24u + (unsigned) m; }
static inline unsigned randSalt (int m) { return 28u + (unsigned) m; }
static const int MOD_RLEN_N[MOD_RLENS] = { 0, 2, 3, 4, 5, 6, 7, 8, 12, 16, 32 };
// RANDOM: its loop drawn again from its seed (the transport starts, a new seed)
static void randReset (ModRnd& r, float seed, int m)
{
    r.seed = seed; r.i = 0; r.k = 1; r.cnt = 0; r.ph = 0.0f;     // (k: drawn)
    for (int j = 0; j < MOD_RLEN; ++j) r.v[j] = loopDraw (seed, randSalt (m), (unsigned) j);
    r.from = r.val; r.to = r.v[0];
}
// RANDOM, once a chunk: at each cycle the next value of its loop (LEN of them; FREE: a new draw each time), which VARI
// may draw anew (the loop changes as it goes), reached in SMTH of the cycle (an S-curve; at 0 a step)
static void randAdvance (ModRnd& r, int m, float rate, float lenP, float vari, float smth, float seed, float hz, float secs, float steps)
{
    if (! r.k || seed != r.seed) randReset (r, seed, m);
    const int len = MOD_RLEN_N[idxOf (lenP, MOD_RLENS)];
    r.ph += hz > 0.0f ? hz * secs : steps / LFO_STEPS[idxOf (rate, NUM_LFO_SPEEDS)];
    if (r.ph >= 1.0f)
    {
        r.ph -= floorf_ (r.ph);
        r.from = r.val;
        if (len == 0) r.to = loopDraw (seed, randSalt (m) + 8u, r.cnt++);
        else
        {
            r.i = (r.i + 1) % len;
            if (vari > 0.0f && (loopDraw (seed, randSalt (m) + 4u, r.cnt) + 1.0f) * 0.5f < vari) r.v[r.i] = loopDraw (seed, randSalt (m) + 8u, r.cnt);
            ++r.cnt;
            r.to = r.v[r.i];
        }
    }
    if (smth <= 0.001f) r.val = r.to;
    else { const float x = minf (r.ph / smth, 1.0f); r.val = r.from + (r.to - r.from) * x * x * (3.0f - 2.0f * x); }
    r.lp = len ? ((float) r.i + minf (r.ph, 1.0f)) / (float) len : r.ph;
}
// ENV's curve: k at 0 a straight line; towards EXP a rise that starts slow (and a fall that starts fast), towards LOG the
// other way
static inline float envShape (float x, float k)
{
    if (absf (k) < 0.02f) return x;
    const float a = 6.0f * k;
    return (expf_ (a * x) - 1.0f) / (expf_ (a) - 1.0f);
}
// ENV, once a chunk: attack to the top, decay to SUS, held while the hit's gate (GATE) is open, released; at the hit's velocity
// as far as VEL says
static void envAdvance (ModEnv& e, float atk, float dec, float sus, float rel, float crv, float velAmt, float secs)
{
    const float k = (crv - 0.5f) * 2.0f;
    const float tA = 0.0005f * powf_ (4000.0f, atk), tD = 0.005f * powf_ (1600.0f, dec), tR = 0.005f * powf_ (1600.0f, rel);
    float y = 0.0f;
    // (the gate counts from the hit; it is let go no sooner than the end of the decay: GATE OFF, an AD then the release)
    if (e.stage >= 1 && e.stage <= 3) e.gate -= secs;
    switch (e.stage)
    {
        case 1: e.x += secs / tA; if (e.x >= 1.0f) { e.x = 0.0f; e.stage = 2; y = 1.0f; } else y = e.from + (1.0f - e.from) * envShape (e.x, k); break;
        case 2: e.x += secs / tD; if (e.x >= 1.0f) { e.x = 0.0f; e.stage = 3; y = sus; } else y = sus + (1.0f - sus) * (1.0f - envShape (e.x, -k)); break;
        case 3: y = sus; if (e.gate <= 0.0f || sus <= 0.0f) { e.stage = 4; e.x = 0.0f; e.from = sus; } break;
        case 4: e.x += secs / tR; if (e.x >= 1.0f) { e.stage = 0; y = 0.0f; } else y = e.from * (1.0f - envShape (e.x, -k)); break;
        default: y = 0.0f; break;
    }
    e.val = y * (1.0f - velAmt + velAmt * e.vel);
    e.pk = maxf (e.pk, e.val);
}
// a track's WAVE, RANDOM and ENV modulators, once a chunk (FOLLOW's run in efAdvance, before the tracks), with their
// settings as the routes move them
void Engine::modAdvance (Track& t, float steps, int n)
{
    const float* lk = t.voice[t.cur].lk;
    const float secs = (float) n / sr;
    auto S = [&] (int m, int k) { return modded (t, lk, MOD_SLOT[m][k]); };
    for (int m = 0; m < MOD_N; ++m)
    {
        const int ty = modType (t, m);
        if (ty == MT_WAVE)
        {
            const float hz = S (m, 6) >= 0.5f ? 0.05f * powf_ (400.0f, S (m, 0)) : 0.0f;
            lfoAdvance (t.lfo[m], t.rng, S (m, 0), S (m, 1), steps, { S (m, 2), S (m, 3), S (m, 4), waveSalt (m) }, S (m, 5), hz, secs);
        }
        else if (ty == MT_RAND)
        {
            const float hz = S (m, 6) >= 0.5f ? 0.05f * powf_ (400.0f, S (m, 0)) : 0.0f;
            const float seed = lk[MOD_SLOT[m][4]] >= 0.0f ? lk[MOD_SLOT[m][4]] : t.p[MOD_SLOT[m][4]];
            randAdvance (t.mrn[m], m, S (m, 0), S (m, 1), S (m, 2), S (m, 3), seed, hz, secs, steps);
        }
        else if (ty == MT_ENV)
            envAdvance (t.men[m], S (m, 0), S (m, 1), S (m, 2), S (m, 3), S (m, 4), S (m, 6), secs);
    }
}
// a hit on track ti: its WAVE modulators in a trig mode start their cycle again, its RANDOM ones on HIT their loop, and
// the ENV modulators of every track that listens to it (SELF, or this track) fire
void Engine::modHit (int ti, const float* lk, float vel, float holdSec)
{
    Track& t = tr[ti];
    auto at = [&] (int p) { return lk[p] >= 0.0f ? lk[p] : t.p[p]; };
    for (int m = 0; m < MOD_N; ++m)
    {
        const int ty = modType (t, m);
        if (ty == MT_WAVE)
        {
            const float wv = at (MOD_SLOT[m][1]);
            if (idxOf (wv, NUM_LFO_WAVES) >= LFO_SHAPES)
                lfoRestart (t.lfo[m], t.rng, wv, { at (MOD_SLOT[m][2]), at (MOD_SLOT[m][3]), at (MOD_SLOT[m][4]), waveSalt (m) });
        }
        else if (ty == MT_RAND && at (MOD_SLOT[m][5]) >= 0.5f)
        {
            ModRnd& r = t.mrn[m];
            r.i = 0; r.ph = 0.0f; r.from = r.val; r.to = r.v[0];
        }
    }
    for (int tt = 0; tt < NUM_TRACKS; ++tt)
        for (int m = 0; m < MOD_N; ++m)
        {
            Track& o = tr[tt];
            if (modType (o, m) != MT_ENV) continue;
            const int src = idxOf (tt == ti ? at (MOD_SLOT[m][5]) : o.p[MOD_SLOT[m][5]], MOD_ESRCS);
            if (! ((src == 0 && tt == ti) || src == ti + 1)) continue;
            ModEnv& e = o.men[m];
            e.stage = 1; e.x = 0.0f; e.from = e.val; e.gate = holdSec; e.vel = vel;
        }
}

// ----------------------------------------------------------------- voice chain (into the track bus)
void Engine::renderVoice (Track& t, Voice& v, int n)
{
    float P[NUM_PARAMS];
    effParams (t, v.lk, P);
    wtfApply (v, P, n);
    rdecApply (v, P);
    rtunApply (v, P);
    v.ecrv = P[P_ECRV];             // (v78: the amplitude envelope's curve, ampTick)

    float* x = vb;
    v.quant = t.q;
    // v81: a drone changed: legato to its new note (45 ms)
    if (v.drone && v.note != v.noteT)
    {
        v.note += (v.noteT - v.note) * (1.0f - expf_ (-(float) n / (0.045f * sr)));
        if (absf (v.noteT - v.note) < 0.002f) v.note = v.noteT;
    }
    machineRender (v, P, n, x);

    int ft = idxOf (P[P_FTYP], NUM_FTYPES);
    float fenvAmt = (P[P_FENV] - 0.5f) * 2.0f;
    const float fnorm = clampf (P[P_FREQ] + fenvAmt * v.fenv, 0.0f, 1.0f);
    const float rs = P[P_RES];
    const float drv = P[P_DRV];
    const float amp = v.vel * P[P_VOL];
    // what moved since the last chunk (a knob, an LFO, a follower, the filter envelope) glides across this one
    // instead of stepping at its edge: the filter is re-tuned every 4 samples, drive and level every sample
    const bool fresh = ! v.smOk;
    const float fn0 = fresh ? fnorm : v.smFn, rs0 = fresh ? rs : v.smRs, drv0 = fresh ? drv : v.smDrv, amp0 = fresh ? amp : v.smAmp;
    v.smOk = 1; v.smFn = fnorm; v.smRs = rs; v.smDrv = drv; v.smAmp = amp;
    if (fresh) { v.smF1 = fnorm; v.smR1 = rs; }
    const bool fMove = fn0 != fnorm || rs0 != rs || v.smF1 != fnorm || v.smR1 != rs;
    // TRIO on LP: a CEM3320-style 4-pole ladder (zero-delay feedback solved linearly, the loop input clipped
    // like the chip's OTA, partial make-up for the bass the resonance takes away, self-oscillation at the top)
    const bool ladder = v.machine == M_TRIO && ft == FT_LP;
    const int msMode = ft == FT_LP ? MS_LP : (ft == FT_BP ? MS_BP : MS_HP);
    float lG = 0.0f, lI = 1.0f, lk = 0.0f, lMk = 1.0f, lG2 = 0.0f, lG4 = 0.0f;
    float combDel = 2.0f, cdf = 0.0f, combFb = 0.0f, open = 0.0f; int cdi = 2;
    // the filter at one position: MS-20 (Korg35; RES maps to the loop gain K, 2 = self-oscillation, Q = 1 / (2 - K)),
    // the ladder or the comb. Wide open (LP at the top, HP at the bottom) it steps out of the way: a filter that
    // filters nothing must not round the top off nor squash the peaks in its loop saturator
    auto tune = [&] (float fnv, float rsv)
    {
        const float fc = 20.0f * exp2f_ (fnv * 9.9658f);
        const float rs2 = rsv * rsv, rs8 = rs2 * rs2 * rs2 * rs2;
        const float K = 2.0f - 1.0f / (0.5f + 24.0f * rs2) + 0.141f * rs8;
        if (ft == FT_COMB)
        {
            combDel = clampf (sr / fc, 2.0f, (float) (COMB_SIZE - 4));
            cdi = (int) combDel; cdf = combDel - (float) cdi; combFb = rsv * 0.97f;
            open = 0.0f;
            return;
        }
        if (ladder)
        {
            const float lg = tanpi (minf (fc, 0.45f * sr) / sr);
            lG = lg / (1.0f + lg); lI = 1.0f / (1.0f + lg);
            lk = 4.4f * rsv * (0.35f + 0.65f * rsv);
            lMk = 1.0f + 0.35f * lk;
            lG2 = lG * lG; lG4 = lG2 * lG2;
        }
        else v.ms.set (fc, K, sr);
        if (OPEN_BYPASS && ft == FT_LP)      { const float o = clampf ((fnv - 0.94f) * (1.0f / 0.06f), 0.0f, 1.0f); open = o * o * (3.0f - 2.0f * o); }
        else if (OPEN_BYPASS && ft == FT_HP) { const float o = clampf (1.0f - fnv * (1.0f / 0.05f), 0.0f, 1.0f); open = o * o * (3.0f - 2.0f * o); }
        else open = 0.0f;
    };
    tune (fnorm, rs);
    float fcoef = decayCoef (0.005f * powf_ (400.0f, P[P_FDEC]), sr);
    v.fenv *= powf_ (fcoef, (float) n);

    float srr = P[P_SRR]; bool srrOn = srr > 0.002f; float srrDiv = 1.0f + srr * srr * 63.0f;
    float bits = P[P_BITS]; bool bitsOn = bits > 0.002f; float levels = exp2f_ (12.0f - bits * 11.0f);
    // drive and level: the chunk's ramp, then a one-pole (0.5 ms) that rounds the corners between two ramps
    if (fresh) { v.smD1 = drv; v.smA1 = amp; }
    const bool drvMove = drv0 != drv || v.smD1 != drv, ampMove = amp0 != amp || v.smA1 != amp;
    const bool drvOn = drv > 0.0f || drv0 > 0.0f || v.smD1 > 0.0f;
    bool dPrime = drvOn && ! (drv0 > 0.0f || v.smD1 > 0.0f);   // the drive starts: its anti-aliasing begins here
    float dg = 1.0f + drv * drv * 24.0f, dmix = minf (1.0f, drv * 20.0f), dcomp = 1.0f / (1.0f + drv * 1.5f);
    float ampI = amp;
    const float invN = 1.0f / (float) n, sm1 = smoothC, smF = smoothC4;
    float R = 1.0f - TWO_PI * 10.0f / sr;
    float chokeDec = 1.0f / (0.003f * sr);
    float atkInc = P[P_ATK] > 0.002f ? 1.0f / (0.001f * powf_ (1500.0f, P[P_ATK]) * sr) : 1.0f;
    // PUNCH: a transient shaper locked to the onset of the note, up to +10 dB over the first ~7 ms and a
    // brighter first 1.5 ms (the high band above ~1.5 kHz pushed); zero = untouched
    const float pn = P[P_PNCH];
    const bool punch = pn > 0.002f;
    const float pg = 2.2f * pn, ph = 1.4f * pn;
    const float pc1 = tcCoef (0.007f, sr), pc2 = tcCoef (0.0015f, sr);
    const float aHF = 1.0f - expf_ (-TWO_PI * 1500.0f / sr);
    float* bus = t.bus;
    // CHIMERA: its sides, filtered as the middle is, go to the track's with the voice's level (trackPost)
    float* cside = nullptr;
    if (v.machine == M_CHIMERA && v.chm.hasSide) { chimSides (v, ft, fnorm, n); cside = v.chm.side; t.sAct = LATENCY / CHUNK + 2; }

    for (int i = 0; i < n; ++i)
    {
        // the glides: the filter at the end of each 8-sample block, drive and level on every sample
        if (fMove && (i & 3) == 0)
        {
            const float u = minf (1.0f, (float) (i + 4) * invN);
            v.smF1 += (fn0 + (fnorm - fn0) * u - v.smF1) * smF; v.smR1 += (rs0 + (rs - rs0) * u - v.smR1) * smF;
            tune (v.smF1, v.smR1);
        }
        if (drvMove || ampMove)
        {
            const float u = (float) (i + 1) * invN;
            if (drvMove)
            {
                v.smD1 += (drv0 + (drv - drv0) * u - v.smD1) * sm1;
                const float d = v.smD1; dg = 1.0f + d * d * 24.0f; dmix = minf (1.0f, d * 20.0f); dcomp = 1.0f / (1.0f + d * 1.5f);
            }
            if (ampMove) { v.smA1 += (amp0 + (amp - amp0) * u - v.smA1) * sm1; ampI = v.smA1; }
        }
        float s = x[i];
        if (srrOn)
        {
            v.srrCnt += 1.0f;
            if (v.srrCnt >= srrDiv) { v.srrCnt -= srrDiv; if (v.srrCnt > srrDiv) v.srrCnt = 0.0f; v.srrHold = s; }
            s = v.srrHold;
        }
        if (bitsOn) s = floorf_ (s * levels + 0.5f) / levels;
        const float dry = s;
        switch (ft)
        {
            case FT_LP:
                if (ladder)
                {
                    float* z = v.lad;
                    const float sig = lG2 * lG * z[0] * lI + lG2 * z[1] * lI + lG * z[2] * lI + z[3] * lI;
                    const float y4 = (lG4 * s + sig) / (1.0f + lk * lG4);
                    float u = s - lk * y4;
                    u = tanhf_ (u * 0.8f) * 1.25f;
                    for (int st = 0; st < 4; ++st)
                    {
                        const float vv = (u - z[st]) * lG;
                        const float y = vv + z[st];
                        z[st] = y + vv;
                        u = y;
                    }
                    s = u * lMk;
                    break;
                }
                s = v.ms.tick (s, msMode); break;
            case FT_BP: case FT_HP: s = v.ms.tick (s, msMode); break;
            default:
            {
                int r0 = (v.cw - cdi) & COMB_MASK, r1 = (v.cw - cdi - 1) & COMB_MASK;
                float d = v.comb[r0] + (v.comb[r1] - v.comb[r0]) * cdf;
                float y = s + combFb * d;
                v.comb[v.cw] = y; v.cw = (v.cw + 1) & COMB_MASK;
                s = y * (1.0f - combFb * 0.5f);
                break;
            }
        }
        if (open > 0.0f) s += (dry - s) * open;
        if (v.machine == M_TRIO) s *= v.vca[i];
        if (drvOn && dmix > 0.0f)
        {
            // tanh with first-order antiderivative anti-aliasing: (lncosh(u) - lncosh(u1)) / (u - u1)
            const float u = s * dg;
            const float au = absf (u), lc = au + log2f_ (1.0f + exp2f_ (-2.8853901f * au)) * LN2 - LN2;
            if (dPrime) { v.drvPrev = u; v.drvLc = lc; dPrime = false; }
            const float du = u - v.drvPrev;
            const float th = absf (du) > 1e-4f ? (lc - v.drvLc) / du : tanhf_ (0.5f * (u + v.drvPrev));
            v.drvPrev = u; v.drvLc = lc;
            s = s + (th * dcomp - s) * dmix;
        }
        if (punch)
        {
            v.pLp += (s - v.pLp) * aHF;
            s = (s + ph * (s - v.pLp) * v.pe2) * (1.0f + pg * v.pe1);
            v.pe1 *= pc1; v.pe2 *= pc2;
        }
        float y = s - v.dcx + R * v.dcy; v.dcx = s; v.dcy = y; s = y;
        if (v.choking) { v.choke -= chokeDec; if (v.choke < 0.0f) v.choke = 0.0f; }
        if (v.atk < 1.0f) { v.atk += atkInc; if (v.atk > 1.0f) v.atk = 1.0f; }
        bus[i] += s * ampI * v.choke * v.atk;
        if (cside) t.sbus[i] += cside[i] * ampI * v.choke * v.atk;
    }

    if (absf (v.smD1 - drv) < 1e-6f) v.smD1 = drv;          // close enough: settle (the fast path again)
    if (absf (v.smF1 - fnorm) < 1e-6f && absf (v.smR1 - rs) < 1e-6f) { v.smF1 = fnorm; v.smR1 = rs; }
    if (absf (v.smA1 - amp) < 1e-7f) v.smA1 = amp;
    if (isBad (v.dcy) || isBad (v.ms.a.z) || isBad (v.ms.b.z) || isBad (v.ms.c.z) || isBad (v.ms.d.z) || isBad (v.lad[3])) { v.active = 0; return; }
    if (v.choking && v.choke <= 0.0f) { v.active = 0; return; }
    // (v83: HOWL held stays, however quiet: its bow, or its feedback, may be swelling from nothing through a clean amp)
    if (v.level < 1e-4f && ! (v.machine == M_HOWL && v.hw.held) && ! (v.machine == M_MANTRA && mantraHeld (v)))
    {
        v.tail += n;
        int tmax = ft == FT_COMB ? (int) (sr * 1.5f) : (int) (sr * 0.03f);
        if (v.tail > tmax) v.active = 0;
    }
    else v.tail = 0;
}

// ----------------------------------------------------------------- BOX
void Engine::boxInit()
{
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        TrackBox& b = tr[t].box;
        b.phase = (t * 64) % BOX_NB;
        b.a.reset (0); b.b.reset (b.phase);
        b.a.setIR (bank.pool, 0, 0, b.gA); b.b.setIR (bank.pool, 0, 0, b.gB);
        for (int i = 0; i < PRE_MAX; ++i) b.pre[i] = 0.0f;
        for (int i = 0; i < BOX_NA; ++i) b.dry[i] = 0.0f;
        b.prew = 0; b.dryw = 0; b.wetPrev = 0.0f; b.toneLp = 0.0f; b.toneHp = 0.0f;
        b.irCur = -1; b.revCur = -1; b.dcyCur = -1.0f; b.gateCur = -1.0f;
        b.quiet = 0; b.idle = 0; b.active = 0;
        // (CHIMERA's sides: their line has the same latency)
        Track& k = tr[t];
        for (int i = 0; i < LATENCY; ++i) k.sdly[i] = 0.0f;
        for (int i = 0; i < CHUNK; ++i) k.sbus[i] = 0.0f;
        k.sdw = 0; k.sAct = 0;
    }
}

// track bus -> BOX insert -> ducking -> pan / sends
// SPRD (TONE's MORE page): the track wider, room made in its middle: over MONO (40 .. 600 Hz) its sound goes partly to
// the sides, through four all-passes (a diffuse width: no comb, nothing moving in pitch), the centre giving up as much
// (equal power: as loud, its middle out of the way of a kick, a snare, a voice); under MONO it stays centred. In mono
// the sides cancel: what is left is the centre (its highs lower by as much)
void Engine::trackSpread (Track& t, const float* P, float* out, float* side, int n)
{
    TrackSpread& s = t.spr;
    const float target = P[P_SPRD];
    if (! s.on)
    {
        for (int k = 0; k < SPD_AP; ++k) for (int i = 0; i < SPD_LEN; ++i) s.buf[k][i] = 0.0f;
        s.w = 0; s.hp.reset(); s.amt = 0.0f; s.on = 1;
    }
    static const float MS[SPD_AP] = { 1.7f, 3.1f, 4.3f, 5.9f };
    int L[SPD_AP];
    for (int k = 0; k < SPD_AP; ++k) L[k] = mini (maxi ((int) (MS[k] * 0.001f * sr + 0.5f), 1), SPD_LEN - 1);
    s.hp.set (40.0f * powf_ (15.0f, P[P_SMON]), 0.7071f, sr);
    // (the amount glides 30 ms, its angle from its ends: up to 53°, the centre 0.6, the sides 0.8)
    const float a0 = s.amt;
    s.amt += (target - s.amt) * (1.0f - expf_ (-(float) n / (0.03f * sr)));
    const float t0 = 0.148f * a0, t1 = 0.148f * s.amt;                // (turns: 0.93 rad at the top)
    const float c0 = cos2pi (t0), c1 = cos2pi (t1), s0 = sin2pi (t0), s1 = sin2pi (t1), in = 1.0f / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const float u = (float) (i + 1) * in, cg = c0 + (c1 - c0) * u, sg = s0 + (s1 - s0) * u;
        const float x = out[i];
        float lo, bp, hi; s.hp.tick (x, lo, bp, hi);
        float y = hi;
        for (int k = 0; k < SPD_AP; ++k)
        {
            float* b = s.buf[k];
            const float d = b[(s.w - L[k]) & (SPD_LEN - 1)], v = y + 0.5f * d;
            b[s.w] = absf (v) < 1e-15f ? 0.0f : v; y = d - 0.5f * v;      // (silence rests at zero: no denormals)
        }
        s.w = (s.w + 1) & (SPD_LEN - 1);
        out[i] = x - hi * (1.0f - cg);
        side[i] += y * sg;
    }
    for (int k = 0; k < SPD_AP; ++k) if (isBad (s.buf[k][(s.w - 1) & (SPD_LEN - 1)])) { s.on = 0; return; }
    s.hp.sanitize();
    if (absf (s.hp.ic1) + absf (s.hp.ic2) < 1e-15f) s.hp.reset();
    if (target <= 0.0f && s.amt < 1e-5f) s.on = 0;
}

void Engine::trackPost (Track& t, int ti, int n)
{
    float P[NUM_PARAMS];
    effParams (t, t.voice[t.cur].lk, P);
    // an FX machine: the track's sound is its source through the effect (it runs whether a trig plays or not)
    const bool isFx = fxMachine (machineOf (P[P_MACH]));
    bool fxSide = false;
    if (isFx)
    {
        const double pos = glRun ? glPos0 + (now - glNow0) * glInc : glFree;
        fxRun (t, ti, P, n, pos, glInc);
        float pk = 0.0f;
        for (int i = 0; i < n; ++i) pk = maxf (pk, absf (fx[ti].side[i]));
        fxSide = pk > 1e-9f;
    }
    TrackBox& b = t.box;
    float* x = t.bus;
    float* out = vb;

    // the dry path always carries the wet-path latency, so switching the BOX never shifts the groove. An FX track
    // has none of its own: its source carries it already (no BOX on an FX track: an insert never moves the groove)
    for (int i = 0; i < n; ++i) { out[i] = isFx ? x[i] : b.dry[b.dryw]; b.dry[b.dryw] = x[i]; b.dryw = (b.dryw + 1) % BOX_NA; }

    const int ir = idxOf (P[P_IR], NUM_IRS + 1);
    if (ir > 0 && bank.e[ir].pa > 0 && ! lite && ! isFx)
    {
        float inPeak = 0.0f;
        for (int i = 0; i < n; ++i) inPeak = maxf (inPeak, absf (x[i]));
        const float fbAmt = P[P_IFB] * 0.97f;
        const int preS = mini ((int) (P[P_IPRE] * P[P_IPRE] * 0.25f * sr), PRE_MAX - 1);
        const IrEntry& E = bank.e[ir];
        const float fb = fbAmt / maxf (E.peak, 1.0f);   // loop gain stays below fbAmt at every frequency
        if (inPeak > 1e-6f || fbAmt > 0.001f) b.quiet = 0; else b.quiet += n;
        const int tailLen = E.len + preS + 2 * BOX_NB + BOX_NA;
        if (! b.active || (b.idle && b.quiet == 0))
        {
            b.a.reset (0); b.b.reset (b.phase);
            for (int i = 0; i < PRE_MAX; ++i) b.pre[i] = 0.0f;
            b.wetPrev = b.toneLp = b.toneHp = b.fbEnv = 0.0f;
            b.active = 1; b.idle = 0;
            b.irCur = -1;
        }
        if (b.quiet > tailLen) b.idle = 1;
        if (! b.idle)
        {
            const int rev = idxOf (P[P_IREV], 2);
            const float dcy = P[P_IDCY], gate = P[P_IGAT];
            if (ir != b.irCur || rev != b.revCur || absf (dcy - b.dcyCur) > 1e-4f || absf (gate - b.gateCur) > 1e-4f)
            {
                const float alpha = (1.0f - dcy) * (1.0f - dcy) * 60.0f;
                const float lenS = (float) E.len / sr;
                const float tg = gate >= 0.999f ? 1e9f : 0.01f * powf_ (maxf (lenS / 0.01f, 1.0f), gate);
                int ua = 0, ub = 0;
                for (int p = 0; p < E.pa; ++p)
                {
                    const float tp = (float) (p * BOX_NA) / sr;
                    b.gA[p] = expf_ (-alpha * tp);
                    if (tp <= tg) ua = p + 1;
                }
                for (int p = 0; p < E.pb; ++p)
                {
                    const float tp = (float) (p * BOX_NB + BOX_NA * BOX_PA) / sr;
                    b.gB[p] = expf_ (-alpha * tp);
                    if (tp <= tg) ub = p + 1;
                }
                b.a.setIR (bank.pool + (rev ? E.offAr : E.offA), E.pa, ua, b.gA);
                b.b.setIR (bank.pool + (rev ? E.offBr : E.offB), E.pb, ub, b.gB);
                b.irCur = ir; b.revCur = rev; b.dcyCur = dcy; b.gateCur = gate;
            }
            b.a.peek (wa, n);
            b.b.peek (wb, n);
            const float tn = (P[P_ITON] - 0.5f) * 2.0f;
            const float cl = tn < 0.0f ? 1.0f - expf_ (-TWO_PI * 20000.0f * exp2f_ (tn * 5.3f) / sr) : 1.0f;
            const float ch = tn > 0.0f ? 1.0f - expf_ (-TWO_PI * 20.0f * exp2f_ (tn * 6.6f) / sr) : 0.0f;
            const float mix = P[P_IMIX], mix0 = t.smOk ? t.smMix : mix, dMix = (mix - mix0) / (float) n;
            const float fbRel = tcCoef (0.05f, sr);
            for (int i = 0; i < n; ++i)
            {
                float w = wa[i] + wb[i];
                b.toneLp += cl * (w - b.toneLp); w = b.toneLp;
                b.toneHp += ch * (w - b.toneHp); w -= b.toneHp;
                if (isBad (w)) w = 0.0f;
                b.pre[b.prew] = x[i];
                const float pd = b.pre[(b.prew - preS + PRE_MAX) % PRE_MAX];
                b.prew = (b.prew + 1) % PRE_MAX;
                // feedback through the box: an envelope follower keeps self-oscillation around -10 dBFS
                const float aw = absf (w);
                b.fbEnv = aw > b.fbEnv ? aw : b.fbEnv * fbRel;
                const float fbEff = fb * minf (1.0f, 0.3f / (b.fbEnv + 1e-6f));
                cin[i] = pd + fbEff * softclip (b.wetPrev);
                b.wetPrev = w;
                const float mx = mix0 + dMix * (float) (i + 1);
                out[i] = out[i] * (1.0f - mx) + w * mx;
            }
            b.a.push (cin, n, fftA);
            b.b.push (cin, n, fftB);
        }
    }
    else b.active = 0;

    // CHIMERA: its voices' sides, through the latency its middle has here (the BOX's), then ducked and TONEd as it is
    const bool chs = ! isFx && t.sAct > 0;
    float* csd = chSide;
    if (chs)
    {
        for (int i = 0; i < n; ++i) { csd[i] = t.sdly[t.sdw]; t.sdly[t.sdw] = t.sbus[i]; t.sbus[i] = 0.0f; t.sdw = (t.sdw + 1) % LATENCY; }
        t.sAct--;
    }

    // TONE: high-pass, low-pass, the three bands (what the mix, the spectrum, RAM and the followers then hear)
    toneProcess (t.tone, P, out, n);
    if (fxSide) toneProcess (fxTone[ti], P, fx[ti].side, n);
    if (chs) toneProcess (t.sTone, P, csd, n);

    // CHOIR, ALLOY: their SPACE (their own reverb, after their TONE), WEST its spring: the middle into the track, the
    // sides with WIDE's (a spring still ringing as the track leaves WEST rings out on it)
    const int pm = machineOf (P[P_MACH]);
    const bool cvRun = ! isFx && (pm == M_CHOIR || pm == M_ALLOY || pm == M_WEST || t.cvOn) && ! lite;
    if (cvRun)
    {
        if (pm == M_ALLOY) alloySpace (t, ti, P, out, cvSide, n);
        else if (pm == M_WEST || (pm != M_CHOIR && fx[ti].kind == WS_KIND)) westSpace (t, ti, P, out, cvSide, n);
        else choirSpace (t, ti, P, out, cvSide, n);
    }

    // spectral room between tracks (analysis of the dry track, gains from the previous frame). Not on an FX track: its
    // sound is its sources (already measured and given their room, each as a track) and their effect; measured as one
    // more track it would collide with its own sources and be carved out (at MIX 0 too: it would sound coded)
    if (! lite && ! isFx)
    {
        um.push (ti, out, n);
        if ((g[G_UMSK] > 0.001f && g[G_MXON] > 0.5f) || um.gC[ti][0] < -0.05f) um.apply (ti, out, n);
    }

    if (ti == specTrack) specProcess (out, n);

    // RAM: what a take of this track records
    for (int i = 0; i < n; ++i) trkOut[ti][i] = out[i];

    // HALO, STRUM: their stereo ensemble (WIDE), to the sides; an FX machine: the side its effect makes; CHOIR: its SPACE's
    const bool cvSd = cvRun && t.cvOn;
    const bool sprd = P[P_SPRD] > 0.0005f || t.spr.on;      // (SPRD: its sides too)
    const bool wide = ((pm == M_HALO || pm == M_STRUM) && P[P_HWID] > 0.0005f) || (! isFx && t.wide.on) || fxSide || cvSd || sprd || chs;
    if (fxSide) for (int i = 0; i < n; ++i) hwSide[i] = fx[ti].side[i];
    else if (wide)
    {
        if (((pm == M_HALO || pm == M_STRUM) && P[P_HWID] > 0.0005f) || t.wide.on) haloWide (t, P, out, hwSide, n);
        else for (int i = 0; i < n; ++i) hwSide[i] = 0.0f;
        if (cvSd) for (int i = 0; i < n; ++i) hwSide[i] += cvSide[i];
        if (chs) for (int i = 0; i < n; ++i) hwSide[i] += csd[i];
    }
    if (sprd) trackSpread (t, P, out, hwSide, n);

    // DUCK: under track 1 (whatever it plays), never track 1 itself. The follower (instant up, 120 ms down): what track 1
    // played this chunk, then where the last chunk left it. v80: on the whole of this track's sound, the machine's, its
    // SPACE or spring, its ensemble and sides (until v79 those went round it)
    {
        const float dk = P[P_DUCK], dk0 = t.smOk ? t.smDuck : dk;
        if ((dk > 0.001f || dk0 > 0.001f) && duckSrc != (1u << ti))
        {
            const float rel = tcCoef (0.12f, sr), dd = (dk - dk0) / (float) n, hasSide = wide || fxSide ? 1.0f : 0.0f;
            float st = duckState;
            for (int i = 0; i < n; ++i)
            {
                const float a = duckAcc[i];
                st = a > st ? a : st * rel;
                const float gk = 1.0f - (dk0 + dd * (float) (i + 1)) * minf (1.0f, st * 1.5f);
                out[i] *= gk;
                if (hasSide > 0.0f) hwSide[i] *= gk;
            }
        }
        if ((duckSrc >> ti) & 1u) for (int i = 0; i < n; ++i) { const float a = absf (out[i]); if (a > duckAcc[i]) duckAcc[i] = a; }
    }

    // RPAN: the hit's pan, there from its attack (the pan's own ramp over the chunk and its smoothing take the old tail
    // across without a click); at its centre, back where PAN is
    if (absf (P[P_RPAN] - 0.5f) <= 0.005f) t.rpanT = 0.0f;
    t.rpan = t.rpanT;
    const float panP = clampf (P[P_PAN] + t.rpan, 0.0f, 1.0f);

    // a track taken by an FX track (an insert): it goes on through that one, not to the mix (nor to the sends): its
    // stereo as it would have reached the mix, its pan and its sides, is what the FX track takes
    if (fxTaken & (1u << ti))
    {
        const float th = panP * 0.25f;
        const float gl = cos2pi (th) * 1.41421f, gr = sin2pi (th) * 1.41421f;
        const float gl0 = t.smOk ? t.smGl : gl, gr0 = t.smOk ? t.smGr : gr, in = 1.0f / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const float u = (float) (i + 1) * in, a = gl0 + (gl - gl0) * u, b = gr0 + (gr - gr0) * u, s = out[i];
            const float sw = wide ? hwSide[i] * 0.5f * (a + b) : 0.0f;
            trkL[ti][i] = s * a + sw; trkR[ti][i] = s * b - sw;
        }
        t.smOk = 1; t.smGl = t.smGl1 = gl; t.smGr = t.smGr1 = gr; t.smSd = t.smSd1 = P[P_DLY]; t.smRv = t.smRv1 = P[P_REV];
        t.smMix = P[P_IMIX]; t.smDuck = P[P_DUCK]; t.smCh = P[P_CHO]; t.smPh = P[P_PHS];
        return;
    }

    // pan and sends: from where the last chunk left them to where they are now, sample by sample
    const float th = panP * 0.25f;
    const float gl = cos2pi (th) * 1.41421f, gr = sin2pi (th) * 1.41421f;
    const float sd = P[P_DLY], srv = P[P_REV];
    const bool fresh = ! t.smOk;
    const float gl0 = fresh ? gl : t.smGl, gr0 = fresh ? gr : t.smGr, sd0 = fresh ? sd : t.smSd, rv0 = fresh ? srv : t.smRv;
    if (fresh) { t.smGl1 = gl; t.smGr1 = gr; t.smSd1 = sd; t.smRv1 = srv; }
    // v68: the CHORUS and PHASER sends, in stereo after the pan (as the delay's), each a ramp over the chunk from the last
    const float sch = P[P_CHO], sph = P[P_PHS], ch0 = fresh ? sch : t.smCh, ph0 = fresh ? sph : t.smPh;
    const bool xs = sch > 0.0f || sph > 0.0f || ch0 > 0.0f || ph0 > 0.0f;
    const float xin = 1.0f / (float) n;
    auto xsend = [&] (int i, float l, float r) {
        const float u = (float) (i + 1) * xin, a = ch0 + (sch - ch0) * u, b = ph0 + (sph - ph0) * u;
        sChL[i] += l * a; sChR[i] += r * a; sPhL[i] += l * b; sPhR[i] += r * b;
    };
    t.smCh = sch; t.smPh = sph;
    if (wide)
    {
        // the same ramps, the ensemble's sides added (at the pan's middle gain)
        const float in = 1.0f / (float) n, c = smoothC;
        for (int i = 0; i < n; ++i)
        {
            const float u = (float) (i + 1) * in, s = out[i];
            t.smGl1 += (gl0 + (gl - gl0) * u - t.smGl1) * c; t.smGr1 += (gr0 + (gr - gr0) * u - t.smGr1) * c;
            t.smSd1 += (sd0 + (sd - sd0) * u - t.smSd1) * c; t.smRv1 += (rv0 + (srv - rv0) * u - t.smRv1) * c;
            const float sw = hwSide[i] * 0.5f * (t.smGl1 + t.smGr1);
            const float l = s * t.smGl1 + sw, r = s * t.smGr1 - sw;
            mL[i] += l; mR[i] += r;
            sDL[i] += l * t.smSd1; sDR[i] += r * t.smSd1;
            sRv[i] += s * t.smRv1;
            if (xs) xsend (i, l, r);
        }
        if (absf (t.smGl1 - gl) < 1e-6f && absf (t.smGr1 - gr) < 1e-6f && absf (t.smSd1 - sd) < 1e-6f && absf (t.smRv1 - srv) < 1e-6f)
        { t.smGl1 = gl; t.smGr1 = gr; t.smSd1 = sd; t.smRv1 = srv; }
    }
    else if (gl0 == gl && gr0 == gr && sd0 == sd && rv0 == srv && t.smGl1 == gl && t.smGr1 == gr && t.smSd1 == sd && t.smRv1 == srv)
        for (int i = 0; i < n; ++i)
        {
            const float s = out[i];
            const float l = s * gl, r = s * gr;
            mL[i] += l; mR[i] += r;
            sDL[i] += l * sd; sDR[i] += r * sd;
            sRv[i] += s * srv;
            if (xs) xsend (i, l, r);
        }
    else
    {
        // the chunk's ramp, then the same 0.5 ms rounding as the voices'
        const float in = 1.0f / (float) n, c = smoothC;
        for (int i = 0; i < n; ++i)
        {
            const float u = (float) (i + 1) * in, s = out[i];
            t.smGl1 += (gl0 + (gl - gl0) * u - t.smGl1) * c; t.smGr1 += (gr0 + (gr - gr0) * u - t.smGr1) * c;
            t.smSd1 += (sd0 + (sd - sd0) * u - t.smSd1) * c; t.smRv1 += (rv0 + (srv - rv0) * u - t.smRv1) * c;
            const float l = s * t.smGl1, r = s * t.smGr1;
            mL[i] += l; mR[i] += r;
            sDL[i] += l * t.smSd1; sDR[i] += r * t.smSd1;
            sRv[i] += s * t.smRv1;
            if (xs) xsend (i, l, r);
        }
        // close enough: settle (the fast path again)
        if (absf (t.smGl1 - gl) < 1e-6f && absf (t.smGr1 - gr) < 1e-6f && absf (t.smSd1 - sd) < 1e-6f && absf (t.smRv1 - srv) < 1e-6f)
        { t.smGl1 = gl; t.smGr1 = gr; t.smSd1 = sd; t.smRv1 = srv; }
    }
    t.smOk = 1; t.smGl = gl; t.smGr = gr; t.smSd = sd; t.smRv = srv; t.smMix = P[P_IMIX]; t.smDuck = P[P_DUCK];
}

// ----------------------------------------------------------------- track spectrum (UI)
// constant-peak band-pass biquads (RBJ, Q 3) on a log grid, then a peak follower that falls in ~0.2 s
void Engine::specInit()
{
    for (int b = 0; b < SPEC_BANDS; ++b)
    {
        float f = 40.0f * powf_ (400.0f, (float) b / (float) (SPEC_BANDS - 1));
        f = minf (f, 0.45f * sr);
        const float w = f / sr, alpha = sin2pi (w) / (2.0f * 3.0f), a0 = 1.0f + alpha;
        specB0[b] = alpha / a0; specA1[b] = -2.0f * cos2pi (w) / a0; specA2[b] = (1.0f - alpha) / a0;
        specS1[b] = specS2[b] = specEnv[b] = 0.0f;
    }
    specRel = expf_ (-1.0f / (0.2f * sr));
}
void Engine::setSpecTrack (int t)
{
    t = (t >= 0 && t < NUM_TRACKS) ? t : -1;
    if (t == specTrack) return;
    specTrack = t;
    for (int b = 0; b < SPEC_BANDS; ++b) specS1[b] = specS2[b] = specEnv[b] = 0.0f;
}
void Engine::specProcess (const float* x, int n)
{
    if (lite) return;
    for (int b = 0; b < SPEC_BANDS; ++b)
    {
        const float b0 = specB0[b], a1 = specA1[b], a2 = specA2[b], rel = specRel;
        float s1 = specS1[b], s2 = specS2[b], e = specEnv[b];
        for (int i = 0; i < n; ++i)
        {
            const float y = b0 * x[i] + s1;          // transposed direct form II, b1 = 0, b2 = -b0
            s1 = s2 - a1 * y;
            s2 = -b0 * x[i] - a2 * y;
            const float a = absf (y);
            e = a > e ? a : e * rel;
        }
        if (e < 1e-7f) { s1 = s2 = 0.0f; e = 0.0f; }  // let it rest (no denormals in the tails)
        specS1[b] = s1; specS2[b] = s2; specEnv[b] = e;
    }
}

// ----------------------------------------------------------------- fx
// the VCO on the delay's clock (v68; both models): its LFO (RATE 0.05 .. 20 Hz, SHAPE sine to square; DEPTH up to an
// octave either way, on a square law) and the send's envelope (ENV: to the right the louder the faster the clock, to the
// left the slower; up to an octave). In octaves, at the end of this chunk
float Engine::delayVco (int n, float& envIn)
{
    Echo82& E = e82;
    float pk = 0.0f;
    for (int i = 0; i < n; ++i) pk = maxf (pk, maxf (absf (sDL[i]), absf (sDR[i])));
    const float ka = 1.0f - expf_ (-(float) n / (0.005f * sr)), kr = 1.0f - expf_ (-(float) n / (0.12f * sr));
    E.env += (pk - E.env) * (pk > E.env ? ka : kr);
    envIn = E.env;
    const float dep = g[G_DVDP], ven = (g[G_DVEN] - 0.5f) * 2.0f;
    E.vph = wrap01 (E.vph + 0.05f * powf_ (400.0f, g[G_DVRT]) * (float) n / sr);
    if (dep <= 0.0005f && absf (ven) <= 0.01f) return 0.0f;
    return dep * dep * sweepShape (sin2pi (E.vph), g[G_DVSH]) + (absf (ven) > 0.01f ? ven * minf (1.0f, 4.0f * E.env) : 0.0f);
}

void Engine::delayProcess (int n)
{
    // ON key: off, the return fades out over 10 ms, then the delay stops and its buffer is wiped a slice per
    // chunk, so it comes back empty. A new model (v68) goes the same way: out, wiped, then the other one from silence
    const int want = idxOf (g[G_DMDL], 2);
    const int on = g[G_DLON] > 0.5f && want == dlyModel;
    if (! on && dlyOnF <= 0.0f)
    {
        if (! dlyOff) { dlyOff = 1; dlyClr = 0; dLpL = dLpR = dHpL = dHpR = 0.0f; }
        const int m = mini (DLY_SIZE - dlyClr, 8192);
        for (int k = 0; k < m; ++k) dl[dlyClr + k] = dr[dlyClr + k] = 0.0f;
        dlyClr += m;
        if (want != dlyModel && dlyClr >= DLY_SIZE) { dlyModel = want; e82.clear(); }
        return;
    }
    if (dlyOff)
    {
        for (int k = dlyClr; k < DLY_SIZE; ++k) dl[k] = dr[k] = 0.0f;
        dlyClr = DLY_SIZE; dlyOff = 0;
    }
    float envIn = 0.0f;
    const float vA = e82.vco, vB = delayVco (n, envIn);
    e82.vco = vB;
    const int hold = g[G_DHLD] > 0.5f;
    const float onStep = 1.0f / (0.01f * sr);
    float target = DLY_STEPS[idxOf (g[G_DTIM], NUM_DLY_TIMES)] * sr * 60.0f / bpm / 4.0f;
    target = clampf (target, 16.0f, (float) (DLY_SIZE - 8));
    float fb = g[G_DFB] * 1.1f;
    float t = g[G_DTON];
    float lpf = t < 0.5f ? 500.0f * powf_ (40.0f, t * 2.0f) : 20000.0f;
    float hpf = t > 0.5f ? 20.0f * powf_ (150.0f, (t - 0.5f) * 2.0f) : 20.0f;
    float la = 1.0f - expf_ (-TWO_PI * minf (lpf, 0.45f * sr) / sr);
    float ha = 1.0f - expf_ (-TWO_PI * hpf / sr);
    float p = g[G_DPNG];
    if (dlyModel == 1) { delay82 (n, on, fb, la, ha, p, vA, vB, hold); return; }
    float slew = 1.0f - tcCoef (0.05f, sr);
    const bool vco = vA != 0.0f || vB != 0.0f;
    const float vin = 1.0f / (float) n;
    for (int i = 0; i < n; ++i)
    {
        dTime += (target - dTime) * slew;
        // (the VCO: a faster clock, a shorter delay)
        float rp = vco ? (float) dw - clampf (dTime * exp2f_ (-(vA + (vB - vA) * (float) (i + 1) * vin)), 4.0f, (float) (DLY_SIZE - 8))
                       : (float) dw - dTime;
        float fl = floorf_ (rp); float fr = rp - fl;
        int i0 = ((int) fl) & DLY_MASK, i1 = (i0 + 1) & DLY_MASK;
        float dLo = dl[i0] + (dl[i1] - dl[i0]) * fr;
        float dRo = dr[i0] + (dr[i1] - dr[i0]) * fr;
        float cl = dLo * (1.0f - p) + dRo * p;
        float cr = dRo * (1.0f - p) + dLo * p;
        dLpL += (cl - dLpL) * la; dLpR += (cr - dLpR) * la;
        cl = dLpL; cr = dLpR;
        dHpL += (cl - dHpL) * ha; dHpR += (cr - dHpR) * ha;
        cl -= dHpL; cr -= dHpR;
        float inL = sDL[i] + p * sDR[i];
        float inR = sDR[i] * (1.0f - p);
        if (! on) inL = inR = 0.0f;
        float wl, wr;
        if (hold) { wl = dLo; wr = dRo; }          // HOLD: the repeats loop as they are, nothing comes in
        else { wl = softclip (inL + fb * cl); wr = softclip (inR + fb * cr); }
        if (absf (wl) < 1e-20f) wl = 0.0f;
        if (absf (wr) < 1e-20f) wr = 0.0f;
        dl[dw] = wl; dr[dw] = wr; dw = (dw + 1) & DLY_MASK;
        if (on && dlyOnF >= 1.0f) { mL[i] += dLo; mR[i] += dRo; }
        else
        {
            dlyOnF = on ? minf (1.0f, dlyOnF + onStep) : maxf (0.0f, dlyOnF - onStep);
            mL[i] += dLo * dlyOnF; mR[i] += dRo * dlyOnF;
        }
    }
    if (isBad (dLpL) || isBad (dLpR) || isBad (dHpL) || isBad (dHpR)) { dLpL = dLpR = dHpL = dHpR = 0.0f; }
}

// the 1982 model (v68): its memory clocked by a VCO. TIME sets the clock against the memory's length (the pitch of the
// repeats glides while it moves; out of 0.5 .. 2x the memory takes a new length, a step, as the unit's range keys do);
// x1: a 40 kHz clock at rest, 16 kHz of bandwidth; x2: 20 kHz, 6 kHz (twice the time in the same memory). Into the
// memory, at each tick of the clock: the send through the limiter and the anti-alias filter, sampled, converted (12 bits
// that range their gain); out of it: the DAC holds each sample until the next tick, then the reconstruction filter.
// The repeats go round the whole way (fed back before the limiter: each pass converted again, a little darker, a little
// grainier); HOLD loops the memory as it is (in the digital domain: nothing lost). The filters follow the clock down
// (under 0.42 of it)
void Engine::delay82 (int n, int on, float fb, float la, float ha, float p, float vA, float vB, int hold)
{
    Echo82& E = e82;
    const int x2 = idxOf (g[G_DBW], 2);
    const float fs0 = x2 ? 20000.0f : 40000.0f, bw = x2 ? 6000.0f : 16000.0f;
    const float Ds = DLY_STEPS[idxOf (g[G_DTIM], NUM_DLY_TIMES)] * 60.0f / bpm / 4.0f;
    const int maxN = DLY_SIZE - 8;
    auto lenFor = [&] () { return mini (maxN, maxi (16, (int) (fs0 * Ds + 0.5f))); };
    if (E.N <= 0 || E.fs0 != fs0) { E.N = lenFor(); E.fs0 = fs0; E.c = (float) E.N / (fs0 * Ds); }
    float cT = (float) E.N / (fs0 * Ds);
    if (cT < 0.5f || cT > 2.0f) { E.N = lenFor(); cT = (float) E.N / (fs0 * Ds); E.c = cT; }
    const float slew = 1.0f - tcCoef (0.05f, sr);
    {
        const float cut = minf (minf (bw, 0.42f * fs0 * E.c * exp2f_ (vB)), 0.45f * sr);
        if (absf (cut - E.cut) > 0.005f * E.cut)
        {
            E.cut = cut;
            for (int k = 0; k < 2; ++k) { setLp4 (E.aa[k], cut, sr); setLp4 (E.rc[k], cut, sr); }
        }
    }
    const float onStep = 1.0f / (0.01f * sr), vin = 1.0f / (float) n;
    const float limTh = 0.7f, limRel = expf_ (-1.0f / (0.3f * sr));
    for (int i = 0; i < n; ++i)
    {
        E.c += (cT - E.c) * slew;
        const double r = (double) (E.c * exp2f_ (vA + (vB - vA) * (float) (i + 1) * vin)) * (double) fs0 / (double) sr;
        // the DAC's held samples, through the reconstruction filter: the return
        const float yL = lp4 (E.rc[0], E.hold[0]), yR = lp4 (E.rc[1], E.hold[1]);
        // the repeats, crossed (PING) and filtered (TONE), back into the input
        float cl = yL * (1.0f - p) + yR * p;
        float cr = yR * (1.0f - p) + yL * p;
        dLpL += (cl - dLpL) * la; dLpR += (cr - dLpR) * la;
        cl = dLpL; cr = dLpR;
        dHpL += (cl - dHpL) * ha; dHpR += (cr - dHpR) * ha;
        cl -= dHpL; cr -= dHpR;
        float xl = sDL[i] + p * sDR[i], xr = sDR[i] * (1.0f - p);
        if (! on) xl = xr = 0.0f;
        xl += fb * cl; xr += fb * cr;
        // the limiter: it catches a peak at once, lets go in 0.3 s; 10:1 above -3 dB
        const float pk = maxf (absf (xl), absf (xr));
        E.limEnv = pk > E.limEnv ? pk : E.limEnv * limRel;
        const float lg = E.limEnv > limTh ? (limTh + (E.limEnv - limTh) * 0.1f) / E.limEnv : 1.0f;
        xl = lp4 (E.aa[0], xl * lg); xr = lp4 (E.aa[1], xr * lg);
        // the clock's ticks during this sample: the input where each falls, converted, into the memory; out of it, N back
        E.ph += r;
        while (E.ph >= 1.0)
        {
            E.ph -= 1.0;
            const float a = (float) (E.ph / r);
            const int wi = E.w & DLY_MASK, rd = (E.w - E.N) & DLY_MASK;
            const float oL = dl[rd], oR = dr[rd];
            if (hold) { dl[wi] = oL; dr[wi] = oR; }
            else { dl[wi] = conv12 (xl + (E.xPrev[0] - xl) * a); dr[wi] = conv12 (xr + (E.xPrev[1] - xr) * a); }
            E.hold[0] = oL; E.hold[1] = oR;
            E.w = (E.w + 1) & DLY_MASK;
        }
        E.xPrev[0] = xl; E.xPrev[1] = xr;
        if (on && dlyOnF >= 1.0f) { mL[i] += yL; mR[i] += yR; }
        else
        {
            dlyOnF = on ? minf (1.0f, dlyOnF + onStep) : maxf (0.0f, dlyOnF - onStep);
            mL[i] += yL * dlyOnF; mR[i] += yR * dlyOnF;
        }
    }
    if (isBad (dLpL) || isBad (dLpR) || isBad (dHpL) || isBad (dHpR)) { dLpL = dLpR = dHpL = dHpR = 0.0f; }
    for (int k = 0; k < 2; ++k)
        if (isBad (E.aa[k][0].ic1) || isBad (E.aa[k][1].ic1) || isBad (E.rc[k][0].ic1) || isBad (E.rc[k][1].ic1))
        { E.aa[k][0].reset(); E.aa[k][1].reset(); E.rc[k][0].reset(); E.rc[k][1].reset(); E.hold[k] = E.xPrev[k] = 0.0f; }
}

// the CHORUS and the PHASER (v68): their sends through each, back into the mix at LEVEL (and their ON keys' fades: off,
// the return fades out over 10 ms, then nothing runs; back on, they start from silence)
void Engine::sendFx (int n)
{
    const float step = (float) n / (0.01f * sr);
    {
        const int on = g[G_CHON] > 0.5f;
        if (on || choOnF > 0.0f)
        {
            const float f0 = choOnF;
            choOnF = on ? minf (1.0f, choOnF + step) : maxf (0.0f, choOnF - step);
            const ChorusParams cp { idxOf (g[G_CHMD], CHO_MODES), idxOf (g[G_CHIN], 2), g[G_CHLV] };
            cho.process (sChL, sChR, mL, mR, n, cp, 1.25f * g[G_CHLV] * 0.5f * (f0 + choOnF));
            if (choOnF <= 0.0f) cho.init (sr);
        }
    }
    {
        const int on = g[G_PHON] > 0.5f;
        if (on || phsOnF > 0.0f)
        {
            const float f0 = phsOnF;
            phsOnF = on ? minf (1.0f, phsOnF + step) : maxf (0.0f, phsOnF - step);
            const PhaserParams pp { 0.1f * powf_ (180.0f, g[G_PRT1]), g[G_PSH1], 0.1f * powf_ (180.0f, g[G_PRT2]), g[G_PSH2],
                g[G_PDPA], g[G_PFBA], g[G_PDPB], g[G_PFBB], idxOf (g[G_PBSW], 2), idxOf (g[G_PSYN], 2), idxOf (g[G_PRTE], PHS_ROUTES) };
            phs.process (sPhL, sPhR, mL, mR, n, pp, 1.25f * g[G_PLVL] * 0.5f * (f0 + phsOnF));
            if (phsOnF <= 0.0f) phs.init (sr);
        }
    }
}

// PLATE: Dattorro's figure-of-eight tank (JAES 1997). TIME = decay, SIZE = tank size,
// DAMP = high-frequency loss, DIFF = input diffusion, MOD = excursion of the modulated all-passes.
void Engine::plateProcess (int n, const float* inM, float* oL, float* oR)
{
    Reverb& r = rv;
    plateSize (g[G_RSIZ]);
    float decay = 0.25f + g[G_RTIM] * 0.735f;
    float damp = g[G_RDMP] * 0.9f;
    float dd2 = clampf (decay + 0.15f, 0.25f, 0.5f);
    r.apL.g = dd2; r.apR.g = dd2;
    const float dm = 0.4f + 1.2f * g[G_RCH1];
    r.ap1.g = minf (0.75f * dm, 0.9f); r.ap2.g = minf (0.75f * dm, 0.9f);
    r.ap3.g = minf (0.625f * dm, 0.9f); r.ap4.g = minf (0.625f * dm, 0.9f);
    float s = r.scale;
    float exc = minf (12.0f * s * 2.0f * g[G_RCH2], 38.0f * s);
    float lfoInc = (0.6f + 0.6f * g[G_RCH2]) / sr;
    for (int i = 0; i < n; ++i)
    {
        float x = r.pre.oldest(); r.pre.push (inM[i] * 0.6f);
        r.bwState += (x - r.bwState) * 0.7f; x = r.bwState;
        x = r.ap1.tick (x); x = r.ap2.tick (x); x = r.ap3.tick (x); x = r.ap4.tick (x);

        r.lfoPh += lfoInc; if (r.lfoPh >= 1.0f) r.lfoPh -= 1.0f;
        float mL_ = sin2pi (r.lfoPh) * exc, mR_ = cos2pi (r.lfoPh) * exc;
        float prevL = r.fbL, prevR = r.fbR;

        // left half
        float l = x + prevR * decay;
        {
            float del = r.mapL.tapF (r.mapLenL + mL_ - 1.0f);
            float v = l - 0.7f * del; r.mapL.push (v); l = del + 0.7f * v;
        }
        { float o = r.d1L.oldest(); r.d1L.push (l); l = o; }
        r.dampL = l * (1.0f - damp) + r.dampL * damp; l = r.dampL * decay;
        l = r.apL.tick (l);
        { float o = r.d2L.oldest(); r.d2L.push (l); r.fbL = o; }

        // right half
        float rr = x + prevL * decay;
        {
            float del = r.mapR.tapF (r.mapLenR + mR_ - 1.0f);
            float v = rr - 0.7f * del; r.mapR.push (v); rr = del + 0.7f * v;
        }
        { float o = r.d1R.oldest(); r.d1R.push (rr); rr = o; }
        r.dampR = rr * (1.0f - damp) + r.dampR * damp; rr = r.dampR * decay;
        rr = r.apR.tick (rr);
        { float o = r.d2R.oldest(); r.d2R.push (rr); r.fbR = o; }

        float yl = r.d1R.tap ((int) (266 * s)) + r.d1R.tap ((int) (2974 * s)) - r.apR.d.tap ((int) (1913 * s))
                 + r.d2R.tap ((int) (1996 * s)) - r.d1L.tap ((int) (1990 * s)) - r.apL.d.tap ((int) (187 * s))
                 - r.d2L.tap ((int) (1066 * s));
        float yr = r.d1L.tap ((int) (353 * s)) + r.d1L.tap ((int) (3627 * s)) - r.apL.d.tap ((int) (1228 * s))
                 + r.d2L.tap ((int) (2673 * s)) - r.d1R.tap ((int) (2111 * s)) - r.apR.d.tap ((int) (335 * s))
                 - r.d2R.tap ((int) (121 * s));
        oL[i] += yl * 0.6f;
        oR[i] += yr * 0.6f;
    }
    if (isBad (r.fbL) || isBad (r.fbR) || isBad (r.dampL) || isBad (r.dampR))
    {
        r.mapL.clear(); r.mapR.clear(); r.d1L.clear(); r.d2L.clear(); r.d1R.clear(); r.d2R.clear();
        r.apL.d.clear(); r.apR.d.clear(); r.ap1.d.clear(); r.ap2.d.clear(); r.ap3.d.clear(); r.ap4.d.clear();
        r.pre.clear(); r.bwState = r.dampL = r.dampR = r.fbL = r.fbR = 0.0f;
    }
    if (absf (r.fbL) < 1e-20f) r.fbL = 0.0f;
    if (absf (r.fbR) < 1e-20f) r.fbR = 0.0f;
    if (absf (r.dampL) < 1e-20f) r.dampL = 0.0f;
    if (absf (r.dampR) < 1e-20f) r.dampR = 0.0f;
}

void Engine::mixInfo (float* out) const
{
    for (int k = 0; k < CALM_K; ++k) { out[2 * k] = calm.s[k].f; out[2 * k + 1] = calm.s[k].g; }
    for (int t = 0; t < NUM_TRACKS; ++t)
        for (int b = 0; b < UM_BANDS; ++b) out[2 * CALM_K + t * UM_BANDS + b] = um.gC[t][b];
}

int Engine::revAlgo() const
{
    int a = idxOf (g[G_RALG], NUM_REV_ALGOS);
    if (a == REV_SPECTRAL && lite) a = REV_HALL;      // the plain-JS fallback has no spectral reverb
    return a;
}

// One reverb at a time, chosen by ALGO; a shared pre-delay in front. Switching algorithm fades the
// old one out over ~20 ms, then the new one starts from silence.
// GRAIN's CLK switch: FREE, then a clock every 1, 4 or 16 steps on the module's SEED input
static const int GRAIN_CLOCK[4] = { 0, 1, 4, 16 };
// GRAIN's LFO: cycle lengths in steps (1/16, 1/8T, 1/8, 1/4T, 1/8D, 1/4, 1/2T, 1/4D, 1/2, 1/2D, 1 to 16 bars)
static const double GLFO_STEPS[NUM_GLFO_RATES] = { 1.0, 4.0 / 3.0, 2.0, 8.0 / 3.0, 3.0, 4.0, 16.0 / 3.0, 6.0, 8.0, 12.0, 16.0, 32.0, 64.0, 128.0, 256.0 };

// GRAIN on the master bus: its LFO (on the transport when it runs, so the patterns sit on the bar) moves the
// eight controls through their attenuverters (full scale at ±100 %; PITCH: ±24 semitones)
void Engine::grainRun (float* L, float* R, int n)
{
    const double inc = (double) bpm / 60.0 * 4.0 / (double) sr;
    const double pos = running ? posAnchor + (now - clockAnchor) * inc : 0.0;
    const double len = GLFO_STEPS[idxOf (g[G_GLRT], NUM_GLFO_RATES)];
    const double shift = (double) idxOf (g[G_GLPH], 16) / 16.0;
    if (running) glfoPh = pos / len + shift;
    else glfoPh += inc * (double) n / len;
    glfoPh -= floord (glfoPh);
    const int w = idxOf (g[G_GLWV], WLFO_WAVES);
    const float x = (float) (glfoPh * WLFO_LEN);
    const int i0 = (int) x;
    const signed char* tab = WLFO_TABLE[w];
    float v = (float) tab[i0 & (WLFO_LEN - 1)];
    if (! (WLFO_FLAGS[w] & WLFO_STEP)) v += ((float) tab[(i0 + 1) & (WLFO_LEN - 1)] - v) * (x - (float) i0);
    glfoVal = v * (1.0f / 127.0f);
    float m[8];
    for (int k = 0; k < 8; ++k) m[k] = (g[G_GA0 + k] - 0.5f) * 2.0f * glfoVal;
    const float size = g[G_GSIZ] >= 0.99f ? g[G_GSIZ] : clampf (g[G_GSIZ] + m[1], 0.0f, 0.98f);   // never into the delay mode
    GrainControls gc { clampf (g[G_GTIM] + m[0], 0.0f, 1.0f), size, clampf (g[G_GSHP] + m[2], 0.0f, 1.0f), g[G_GPIT],
                       clampf (g[G_GDEN] + m[4], 0.0f, 1.0f), clampf (g[G_GFDB] + m[5], 0.0f, 1.0f), clampf (g[G_GVRB] + m[6], 0.0f, 1.0f),
                       g[G_GRND], clampf (g[G_GMIX] + m[7], 0.0f, 1.0f), m[3] * 24.0f,
                       idxOf (g[G_GMED], GRAIN_MEDIA), GRAIN_CLOCK[idxOf (g[G_GSYN], 4)], g[G_GFRZ] > 0.5f };
    grainProcess (grainMem, gc, L, R, n, pos, inc, running);
}

void Engine::reverbProcess (int n, float* inM, float* oL, float* oR)
{
    static const float TRIM[NUM_REV_ALGOS] = { 0.9f, 1.0f, 1.0f, 1.0f, 0.87f, 0.95f };   // similar loudness at TIME 0.5
    const int want = revAlgo();
    if (revCur < 0) revCur = want;
    const float fadeStep = (float) n / (0.02f * sr);
    // ON key: off, the return fades out over 10 ms, then nothing runs; back on, the algorithm and the
    // pre-delay start from silence
    const int on = g[G_RVON] > 0.5f;
    if (! on && revOnF <= 0.0f) { revOff = 1; return; }
    if (revOff)
    {
        revOff = 0; revCur = want; revFade = 1.0f; revIdle = 0;
        revStart (want);
        for (int i = 0; i < PRE_LEN; ++i) preBuf[i] = 0.0f;
    }
    const float onStep = 1.0f / (0.01f * sr);

    // pre-delay
    const float pms = g[G_RPRE] * g[G_RPRE] * 250.0f;
    const float target = minf (pms * 0.001f * sr, (float) (PRE_LEN - 4));
    const float slew = 1.0f - tcCoef (0.08f, sr);
    float inE = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        preBuf[preW] = on ? inM[i] : 0.0f;
        preLen += (target - preLen) * slew;
        float rp = (float) preW - preLen;
        if (rp < 0.0f) rp += (float) PRE_LEN;
        const int i0 = (int) rp; const float fr = rp - (float) i0;
        const float a0 = preBuf[i0 & (PRE_LEN - 1)], a1 = preBuf[(i0 + 1) & (PRE_LEN - 1)];
        rvIn[i] = a0 + (a1 - a0) * fr;
        preW = (preW + 1) & (PRE_LEN - 1);
        inE += absf (inM[i]);
        rvL[i] = rvR[i] = 0.0f;
    }

    // idle: skip the algorithm once it has been silent for 2 s (input and output)
    if (inE > 1e-9f || want != revCur) revIdle = 0;
    if (revIdle > (int) (2.0f * sr)) return;

    const float time = g[G_RTIM], size = g[G_RSIZ], damp = g[G_RDMP], c1 = g[G_RCH1], c2 = g[G_RCH2];
    switch (revCur)
    {
        case REV_PLATE: plateProcess (n, rvIn, rvL, rvR); break;
        case REV_SPRING:
            spring.configure (time, size, damp, c1, c2);
            spring.process (rvIn, rvL, rvR, n, 1.0f);
            break;
        case REV_SPECTRAL:
        {
            // TIME, BLUR (size), ATMO (damp), MODE (pre: normal / freeze / reverse / both), WARP, REFLECT
            const int mode = idxOf (g[G_RPRE], 4);
            SpecParams sp { time, size, c1, c2 * c2, damp, (mode & 1) ? 1.0f : 0.0f, (mode & 2) ? 1.0f : 0.0f, 0.6f };
            spec.process (inM, rvL, rvR, n, sp);
            break;
        }
        default:
            fdn.configure (revCur, time, size, damp, c1, c2);
            fdn.process (rvIn, rvL, rvR, n, 1.0f);
            break;
    }

    float gain = g[G_RLVL] * (1.0f / 0.6f) * TRIM[revCur];
    float outE = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        if (want != revCur) revFade = maxf (0.0f, revFade - fadeStep / (float) n);
        if (! on || revOnF < 1.0f) revOnF = on ? minf (1.0f, revOnF + onStep) : maxf (0.0f, revOnF - onStep);
        const float k = gain * revFade * revOnF;
        oL[i] += rvL[i] * k; oR[i] += rvR[i] * k;
        outE += absf (rvL[i]) + absf (rvR[i]);
    }
    if (inE < 1e-9f && outE < 1e-7f * (float) n) revIdle += n; else revIdle = 0;
    if (want != revCur && revFade <= 0.0f)
    {
        // start the new algorithm from silence
        revCur = want; revFade = 1.0f; revIdle = 0;
        revStart (want);
    }
}

// an algorithm from silence
void Engine::revStart (int a)
{
    if (a == REV_PLATE)
    {
        Reverb& r = rv;
        r.mapL.clear(); r.mapR.clear(); r.d1L.clear(); r.d2L.clear(); r.d1R.clear(); r.d2R.clear();
        r.apL.d.clear(); r.apR.d.clear(); r.ap1.d.clear(); r.ap2.d.clear(); r.ap3.d.clear(); r.ap4.d.clear();
        r.pre.clear(); r.bwState = r.dampL = r.dampR = r.fbL = r.fbR = 0.0f;
        plateSize (g[G_RSIZ]);
    }
    else if (a == REV_SPRING) spring.init (sr);
    else if (a == REV_SPECTRAL) spec.init (sr);
    else fdn.init (sr);
}

// ----------------------------------------------------------------- render
void Engine::renderChunk (int n, float* L, float* R)
{
    for (int i = 0; i < n; ++i) mL[i] = mR[i] = sDL[i] = sDR[i] = sRv[i] = 0.0f;
    for (int i = 0; i < n; ++i) sChL[i] = sChR[i] = sPhL[i] = sPhR[i] = 0.0f;
    const float steps = (float) ((double) bpm / 60.0 * 4.0 / (double) sr * n);
    wtfTick (n);
    // the host input under this chunk, in mono (RAM: IN source, monitor)
    {
        const int off = (int) (now - inStart);
        if (inL != nullptr && off >= 0) for (int i = 0; i < n; ++i) inM[i] = 0.5f * (inL[off + i] + inR[off + i]);
        else for (int i = 0; i < n; ++i) inM[i] = 0.0f;
    }
    prismAnalyze();
    modPrep();
    efAdvance (n);
    radioAdvance (n);
    fxPrepass();
    // DUCK's source: track 1 (what it plays this chunk, gathered as it renders)
    duckSrc = 1u;
    for (int i = 0; i < n; ++i) duckAcc[i] = 0.0f;

    for (int ti = 0; ti < NUM_TRACKS; ++ti)
    {
        Track& t = tr[ti];
        // knobs turned while the track sounds: a step towards where they were left
        if (t.slew)
        {
            const float a = 1.0f - expf_ (-(float) n / (0.012f * sr));
            for (int b = 0; b < 32; ++b)
            {
                if (! (t.slew & (1u << b))) continue;
                const int p = SLEW_PS[b];
                t.p[p] += (t.pt[p] - t.p[p]) * a;
                if (absf (t.pt[p] - t.p[p]) < 2e-5f) { t.p[p] = t.pt[p]; t.slew &= ~(1u << b); }
            }
        }
        // LFOs use the lock snapshot of the current voice
        const float* lk = t.voice[t.cur].lk;
        auto P = [&] (int p) { return lk[p] >= 0.0f ? lk[p] : t.p[p]; };
        modAdvance (t, steps, n);

        for (int i = 0; i < n; ++i) t.bus[i] = 0.0f;
        for (int k = 0; k < 2; ++k)
            if (t.voice[k].active) renderVoice (t, t.voice[k], n);
        // RAM recording the input: MONI lets it through the track (its BOX, pan and sends), at the take's level
        if (inL != nullptr && t.p[P_RMON] > 0.001f && machineOf (t.p[P_MACH]) == M_RAM && idxOf (t.p[P_RSRC], RAM_SRCS) == RAM_IN)
        {
            const float lv = 2.0f * t.p[P_RLEV], mg = t.p[P_RMON] * t.p[P_RMON] * lv * lv;
            for (int i = 0; i < n; ++i) t.bus[i] += inM[i] * mg;
        }
        trackPost (t, ti, n);
        const int m = t.voice[t.cur].active ? t.voice[t.cur].machine : machineOf (t.p[P_MACH]);
        const int mm = m == M_MACRO ? idxOf (P (P_A), MACRO_MODELS) : -1;      // MACRO: its BD / SD / HH models
        role[ti] = mm == 13 ? ROLE_KICK : mm == 14 ? ROLE_SNARE : mm == 15 ? ROLE_HAT
                 : (m == M_BD_VA || m == M_BD_FM || m == M_SINUS || m == M_FMZ || m == M_HARD || m == M_HEXBD) ? ROLE_KICK
                 : (m == M_SD_VA || m == M_SD_FM || m == M_CP_NZ || m == M_HEXSD) ? ROLE_SNARE
                 : (m == M_HH_MT || m == M_DATA) ? ROLE_HAT : ROLE_OTHER;
    }
    // (DUCK's follower through what the kicks played)
    { const float rel = tcCoef (0.12f, sr); for (int i = 0; i < n; ++i) { const float a = duckAcc[i]; duckState = a > duckState ? a : duckState * rel; } }
    const int mixOn = g[G_MXON] > 0.5f;
    if (! lite) um.advance (n, role, mixOn ? g[G_UMSK] : 0.0f, g[G_UKCK]);

    delayProcess (n);
    reverbProcess (n, sRv, mL, mR);
    sendFx (n);

    // master section (EQ, compressor, drive, clipper, volume), 4x oversampled
    for (int i = 0; i < n; ++i) { L[i] = mL[i] * 0.7f; R[i] = mR[i] * 0.7f; }
    if (! lite)
    {
        const CalmParams cp { mixOn ? g[G_CALM] : 0.0f, g[G_CSHP], g[G_CSEL], g[G_CSPD], g[G_CFOC], mixOn ? g[G_CDLT] : 0.0f };
        calm.process (L, R, n, cp);
    }
    MasterParams mp;
    mp.eqLf = g[G_EQLF]; mp.eqLb = g[G_EQLB]; mp.eqLa = g[G_EQLA]; mp.eqHf = g[G_EQHF];
    mp.eqHb = g[G_EQHB]; mp.eqBw = g[G_EQBW]; mp.eqHa = g[G_EQHA]; mp.eqHs = g[G_EQHS];
    mp.cIn = g[G_CINP]; mp.cAtk = g[G_CATK]; mp.cRel = g[G_CREL]; mp.cRatio = g[G_CRAT];
    mp.cDist = g[G_CDST]; mp.cDet = g[G_CDET]; mp.cMix = g[G_CMIX]; mp.cOut = g[G_COUT];
    mp.dModel = g[G_DMOD]; mp.dDrive = g[G_DDRV]; mp.dBias = g[G_DBIA]; mp.dTone = g[G_DTNE];
    mp.dChar = g[G_DCHR]; mp.dMix = g[G_DMIX]; mp.dOut = g[G_DOUT]; mp.dPos = g[G_DPOS];
    mp.clip = g[G_MDRV]; mp.vol = g[G_MVOL];
    mp.eqOn = g[G_EQON] > 0.5f; mp.compOn = g[G_COON] > 0.5f; mp.drvOn = g[G_DRON] > 0.5f;
    // GRAIN and OTT sit between the compressor / drive and the final clipper: the master then runs in two
    // stages (untouched single pass when both are off)
    const int grainOn = g[G_GON] > 0.5f, ottOn = g[G_OON] > 0.5f, glOn = g[G_XON] > 0.5f || glitch.busy();
    if (grainOn || ottOn || glOn)
    {
        if (! postWas) { master.osL2.reset(); master.osR2.reset(); }
        master.process (L, R, n, mp, 1);
        if (grainOn) grainRun (L, R, n);
        if (ottOn)
        {
            const OttParams op { g[G_OTIM], g[G_OUP], g[G_ODN], g[G_OLO], g[G_OMD], g[G_OHI], g[G_OOUT], g[G_OMIX] };
            ott.process (L, R, n, op);
        }
        if (glOn)
        {
            // the grid follows the song when it runs, a free clock at the tempo when it does not
            const double pos = glRun ? glPos0 + (now - glNow0) * glInc : glFree;
            const GlitchParams gp { g[G_XON] > 0.5f ? 1 : 0, g[G_XTIM], g[G_XRPT], g[G_XBRK], g[G_XBND], g[G_XCRP],
                idxOf (g[G_XCMD], GL_CMODES), g[G_XLOS], idxOf (g[G_XLMD], GL_LMODES), g[G_XSPD], g[G_XMIX], g[G_XHLD] > 0.5f ? 1 : 0,
                g[G_XRES], g[G_XANA] > 0.5f ? 1 : 0, idxOf (g[G_XAMD], GL_AMODES) };
            glitch.process (L, R, n, gp, pos, glInc);
        }
        master.process (L, R, n, mp, 2);
    }
    else master.process (L, R, n, mp);
    if (! ottOn) ott.gainDb[0] = ott.gainDb[1] = ott.gainDb[2] = 0.0f;
    if (ottOn && ! (postWas & 2)) ott.reset();
    postWas = (grainOn ? 1 : 0) | (ottOn ? 2 : 0) | (glOn ? 4 : 0);
    if (! glRun) glFree += n * glInc;
    // LIVE (the temporary random of the SEQ page; v89 WTF?!): glided, so that turning it never steps the sounds (LOOMND:
    // its distance from the centre, either way)
    {
        const float tgt = idxOf (g[G_WTFM], WTF_MODES) == WTF_LOOMND ? absf (g[G_XRND] - 0.5f) * 2.0f : g[G_XRND];
        xrndS += (tgt - xrndS) * (1.0f - expf_ (-(float) n / (0.02f * sr)));
        if (absf (tgt - xrndS) < 1e-5f) xrndS = tgt;
    }
    // RAM: the takes in progress take this chunk from their source (MAIN: the output as heard)
    for (int t = 0; t < NUM_TRACKS; ++t) if (ram[t].state != RAM_IDLE) { ramRecord (n, L, R); break; }
    // the envelope followers read this chunk (the master as heard) at the start of the next one
    for (int i = 0; i < n; ++i) efMain[i] = 0.5f * (L[i] + R[i]);
    efPrevN = n;
}

void Engine::renderSegment (int n, float* L, float* R)
{
    // chunks follow an absolute 32-sample grid so control-rate updates do not depend on the host block size
    int done = 0;
    while (done < n)
    {
        int phase = (int) (now - floord (now / (double) CHUNK) * (double) CHUNK);
        if (phase < 0 || phase >= CHUNK) phase = 0;
        int c = mini (CHUNK - phase, n - done);
        renderChunk (c, L + done, R + done);
        done += c;
        now += c;
    }
}

// kind: 0 a trig, 1 a REC trig (RAM), 3 a lock-only step
struct Ev { int off; int t; int step; float vel; int kind; int abs; };

void Engine::process (float* L, float* R, int n, const HostTime* ht)
{
    if (n <= 0) return;
    if (cube.busy >= 0) cubeStep (4);          // (CUBE: a bank loaded, its versions made a few waves a block)
    if (n > PROC_MAXB)
    {
        // long host blocks are split (event times are absolute, so the result does not change)
        const float* l0 = inL; const float* r0 = inR;
        for (int done = 0; done < n; )
        {
            const int k = mini (PROC_MAXB, n - done);
            HostTime sub; const HostTime* hp = nullptr;
            if (ht) { sub = *ht; if (sub.valid) sub.ppq = ht->ppq + (double) done * ht->bpm / 60.0 / (double) sr; hp = &sub; }
            inL = l0 != nullptr ? l0 + done : nullptr; inR = r0 != nullptr ? r0 + done : nullptr;
            process (L + done, R + done, k, hp);
            done += k;
        }
        inL = inR = nullptr;
        return;
    }
    // BREAK: a loop handed over is read a little every block (16 samples per sample of the block, 8192 at least)
    brk.work (maxi (8192, 16 * n));
    inStart = sampleClock;
    int hostRun = ht && ht->valid && ht->playing;
    int jump = 0;
    if (hostRun)
    {
        bpm = (float) (ht->bpm < 20.0 ? 20.0 : (ht->bpm > 400.0 ? 400.0 : ht->bpm));
        double hp = ht->ppq * 4.0;
        if (! wasRunning || absd (hp - pos16) > 0.05) { pos16 = hp; jump = 1; }
        running = 1;
    }
    else if (internalPlay) { bpm = g[G_BPM]; running = 1; }
    else { bpm = g[G_BPM]; running = 0; }

    if (running && ! wasRunning)
        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            tr[t].rtLeft = 0; tr[t].hProg = 0; tr[t].hWalk = 0; tr[t].jAcc = 0; tr[t].wsNext = 0;
            // (the free waves start their cycle with the transport; LOOP its first draw)
            // (v90: every WAVE modulator; RANDOM's loop as its seed draws it)
            for (int m = 0; m < MOD_N; ++m)
            {
                const int ty = modType (tr[t], m);
                if (ty == MT_WAVE && idxOf (tr[t].p[MOD_SLOT[m][1]], NUM_LFO_WAVES) < LFO_SHAPES) { tr[t].lfo[m].ph = 0.0f; tr[t].lfo[m].k = 0; }
                else if (ty == MT_RAND) randReset (tr[t].mrn[m], tr[t].p[MOD_SLOT[m][4]], m);
            }
            tr[t].rdStep = -1; tr[t].rdSub = 0;
        }
    if (! running) for (int t = 0; t < NUM_TRACKS; ++t) { tr[t].rtLeft = 0; tr[t].curStep = -1; }
    // a stop (the transport, the host's) or a mute: the notes held until their next trig let go (HOLD, GATE off)
    for (int t = 0; t < NUM_TRACKS; ++t)
        if (! running || tr[t].mute)
            for (int k = 0; k < 2; ++k) if (tr[t].voice[k].sus) { if (tr[t].voice[k].drone) droneEnd (tr[t].voice[k]); else letGo (tr[t].voice[k]); }
    // v81: DRONE: the tracks that play on their own while the transport runs (a stop, a mute, the switch off, another
    // machine let theirs go, in their release; back on, it starts again, in its attack)
    for (int ti = 0; ti < NUM_TRACKS; ++ti)
    {
        Track& t = tr[ti];
        const bool want = running && ! t.mute && droneWanted (t);
        bool has = false;
        for (int k = 0; k < 2; ++k)
        {
            Voice& v = t.voice[k];
            if (! v.drone) continue;
            if (! v.active || v.choking) { v.drone = 0; continue; }
            if (! want || v.machine != machineOf (t.p[P_MACH])) droneEnd (v); else has = true;
        }
        if (want && ! has) droneStart (t, ti);
    }

    const double inc = (double) bpm / 60.0 * 4.0 / (double) sr;
    glInc = inc; glRun = running; glNow0 = now;
    if (running)
    {
        if (! wasRunning || jump || absf (bpm - anchorBpm) > 1e-4f) { posAnchor = pos16; clockAnchor = sampleClock; anchorBpm = bpm; }
        pos16 = posAnchor + (sampleClock - clockAnchor) * inc;
    }
    // --- sequence looper: fold the song position into a window (EP-133 style). The real clock keeps
    //     running underneath, so leaving the loop lands where the song would have been (slip).
    {
        const int want = running && g[G_LOOP] > 0.5f;
        const double Ll = LOOP_LENS[idxOf (g[G_LLEN], NUM_LOOP_LENS)];
        if (! want) loopOn = 0;
        else if (! loopOn || Ll != loopLen)
        {
            // effective position now (without the POS offset), then a window aligned to its own length
            // (or to the current step for stutters); the anchor keeps the phase continuous
            const double pe = loopOn ? loopMapRaw (pos16) : pos16;
            const double q = Ll >= 1.0 ? Ll : 1.0;
            const double base = floord (pe / q + 1e-9) * q;
            loopBase = base; loopLen = Ll; loopOn = 1;
            loopAnchor = pos16 - (pe - base);
        }
    }
    // --- patterns: the transport (re)started or the host jumped before the pattern's start: it starts on its grid; a
    //     change at once (START, JUMP, or queued while the transport starts); or at the end of the pattern (its CHG
    //     length since it started), sample-exact: the block is split there (not while the looper folds the position)
    if (running)
    {
        if ((! wasRunning || jump) && pos16 < patStart - 1e-9) { const double C = patChangeLen(); patStart = maxd_ (0.0, floord (pos16 / C) * C); }
        int nx = patNext();
        if (nx >= 0 && queued >= 0 && (qMode != 0 || ! wasRunning))
        {
            const double e0 = loopOn ? loopMapRaw (pos16) + loopOffset() : pos16;
            queued = -1;
            patSwitch (nx, qMode == 2 && wasRunning ? patStart : e0);
            nx = -1;
        }
        if (nx >= 0 && ! loopOn)
        {
            // the pattern repeats every C 16ths from its start: the change comes at the next end of it on that grid (the
            // looper let go, a jump forward: the next one too). Right on one (the block starts there): now
            const double C = patChangeLen();
            double k = floord ((pos16 - patStart) / C + 1e-9);
            if (k < 0.0) k = 0.0;                     // (a hair before its start: the pattern has just begun)
            const bool onIt = k >= 1.0 && pos16 - (patStart + k * C) < 0.5 * inc;
            const double B = patStart + (onIt ? k : k + 1.0) * C;
            const double at = floord (clockAnchor + (B - posAnchor) / inc + 1e-6);   // the sample of the change
            const int sB = onIt ? 0 : (int) (at - sampleClock);
            auto take = [this] () { int i = queued; if (i >= 0) queued = -1; else { chainPos = (chainPos + 1) % chainLen; i = chain[chainPos]; } return i; };
            if (sB <= 0) patSwitch (take(), B);
            else if (sB < n)
            {
                HostTime sub; const HostTime* hp = nullptr;
                if (ht) { sub = *ht; hp = &sub; }
                const float* l0 = inL; const float* r0 = inR;
                process (L, R, sB, hp);
                patSwitch (take(), B);
                if (ht && sub.valid) sub.ppq = ht->ppq + (double) sB * ht->bpm / 60.0 / (double) sr;
                inL = l0 != nullptr ? l0 + sB : nullptr; inR = r0 != nullptr ? r0 + sB : nullptr;
                process (L + sB, R + sB, n - sB, hp);
                inL = inR = nullptr;
                return;
            }
        }
    }
    glPos0 = pos16;
    Ev ev[256]; int ne = 0;
    if (running)
    {
        double swingOff = (double) g[G_SWING] * 0.5;
        // segments of this block in effective time: [eA, eA + (rB - rA)) for real [rA, rB)
        struct Seg { double rA, rB, eA; } seg[16]; int nseg = 0;
        const double pA = pos16, pB = pos16 + n * inc;
        if (! loopOn) seg[nseg++] = { pA, pB, pA };
        else
        {
            const double off = loopOffset();
            double r = pA;
            while (r < pB && nseg < 16)
            {
                double ph = r - loopAnchor; ph -= floord (ph / loopLen) * loopLen;
                const double end = minf_d (pB, r + (loopLen - ph));
                double e = loopBase + off + ph; if (e < 0.0) e = 0.0;
                seg[nseg++] = { r, end, e };
                r = end;
            }
        }
        for (int ti = 0; ti < NUM_TRACKS; ++ti)
        {
            Track& t = tr[ti];
            double s = SPEEDS[t.spd];
            int len = t.len;
            const int plays = ! t.mute;
            const int recs = machineOf (t.p[P_MACH]) == M_RAM;     // RAM: its REC trigs record (muted too)
            if (plays || recs)
                for (int si = 0; si < nseg; ++si)
                {
                    const Seg& g_ = seg[si];
                    const double eB = g_.eA + (g_.rB - g_.rA), delta = g_.eA - g_.rA;
                    double a = (g_.eA - patStart) * s, b = (eB - patStart) * s;
                    int k0 = (int) floord (a) - 1, k1 = (int) floord (b) + 1;
                    for (int k = k0; k <= k1; ++k)
                    {
                        if (k < 0) continue;
                        int st = k % len;
                        // kind 0: a REC trig (on the grid, first), kind 1: a PLAY trig or a lock-only step (with its micro-timing)
                        for (int kind = recs && t.rec[st] ? 0 : 1; kind < 2; ++kind)
                        {
                            if (kind == 1 && ! (plays && (t.trig[st] || t.tlk[st]))) continue;
                            double e = (double) k + ((k & 1) ? swingOff : 0.0) + (kind ? ((double) lock (t, st, P_MICR) - 0.5) : 0.0);
                            if (loopOn && (e / s + patStart < g_.eA - 1e-9 || e / s + patStart >= eB - 1e-9)) continue;   // inside this segment only
                            // absolute sample of the event, from the anchor (not from this block's start)
                            const double at = floord (clockAnchor + (e / s + patStart - delta - posAnchor) / inc + 1e-6);
                            if (at >= sampleClock && at < sampleClock + (double) n && ne < 256)
                            {
                                ev[ne].off = (int) (at - sampleClock);
                                ev[ne].t = ti; ev[ne].step = st; ev[ne].vel = kind ? 1.0f : -1.0f;
                                ev[ne].kind = kind == 0 ? 1 : (t.trig[st] ? 0 : 3); ev[ne].abs = k; ne++;
                            }
                        }
                    }
                }
            const Seg& last = seg[nseg - 1];
            double fb_ = floord ((last.eA + (last.rB - last.rA) - patStart) * s);
            t.curStep = fb_ < 0.0 ? 0 : ((int) fb_) % len;
        }
        // sort by offset (stable insertion sort). The track the quantiser follows comes first at a time it shares: its
        // new chord is there for the notes struck with it
        const int qSrc = idxOf (g[G_QSRC], NUM_TRACKS + 1) - 1;
        auto before = [qSrc] (const Ev& a, const Ev& b) { return a.off < b.off || (a.off == b.off && a.t == qSrc && b.t != qSrc); };
        for (int i = 1; i < ne; ++i)
        {
            Ev e = ev[i]; int j = i - 1;
            while (j >= 0 && before (e, ev[j])) { ev[j + 1] = ev[j]; --j; }
            ev[j + 1] = e;
        }
    }

    double blockStart = sampleClock;
    now = blockStart;
    int cur = 0, ei = 0;
    while (cur < n)
    {
        int next = n;
        if (ei < ne && ev[ei].off < next) next = ev[ei].off;
        for (int ti = 0; ti < NUM_TRACKS; ++ti)
            if (tr[ti].rtLeft > 0)
            {
                double ro = tr[ti].rtNext - blockStart;
                int o = ro <= (double) cur ? cur : (int) floord (ro);
                if (o < next) next = o;
            }
        if (next > cur) { renderSegment (next - cur, L + cur, R + cur); cur = next; }
        now = blockStart + cur;
        if (cur >= n) break;
        while (ei < ne && ev[ei].off <= cur)
        {
            if (ev[ei].kind == 1) ramStart (ev[ei].t, ev[ei].step);      // a REC trig
            else if (ev[ei].kind == 3) triglessStep (ev[ei].t, ev[ei].step);   // a lock-only step
            else { tr[ev[ei].t].bAbs = ev[ei].abs; triggerTrack (ev[ei].t, ev[ei].step, 1.0f, 0); }
            ++ei;
        }
        for (int ti = 0; ti < NUM_TRACKS; ++ti)
        {
            Track& t = tr[ti];
            while (t.rtLeft > 0 && t.rtNext - blockStart < (double) cur + 1.0)
            {
                int k = t.rtTotal - t.rtLeft + 1;
                float frac = (float) k / (float) t.rtTotal;
                // an LFO on RVEL reshapes the roll as it plays; on RTRG it speeds it up or slows it down
                // (an LFO that reaches OFF keeps the last rate: the roll ends with its count)
                if (lfoTargets (t, t.rtStep, P_RVEL)) t.rtCurve = (trigParam (t, t.rtStep, P_RVEL) - 0.5f) * 2.0f;
                if (! t.rtRatchet && lfoTargets (t, t.rtStep, P_RTRG))
                {
                    const int ri = idxOf (trigParam (t, t.rtStep, P_RTRG), NUM_RETRIG);
                    if (ri > 0) { t.rtBase = samplesPerStep (t) * 16.0 / (double) RETRIG_DEN[ri]; t.rtInterval = t.rtBase * t.rtScale; }
                }
                float gain = t.rtCurve < 0.0f ? 1.0f + t.rtCurve * frac : 1.0f - t.rtCurve * (1.0f - frac);
                t.rtLeft--;
                // RCRV: the next gap (the same ratio each time; even: the rate's gap, exactly)
                if (t.rtRatio != 1.0) { t.rtScale *= t.rtRatio; t.rtInterval = maxd_ (t.rtBase * t.rtScale, 16.0); }
                t.rtNext += t.rtInterval;
                if (! t.mute) triggerTrack (ti, t.rtStep, gain, 1);
            }
        }
    }

    sampleClock = blockStart + n;
    if (running) pos16 = posAnchor + (sampleClock - clockAnchor) * inc;
    wasRunning = running;
    loopInfo[0] = (float) loopOn;
    loopInfo[1] = loopOn ? (float) maxd_ (0.0, loopBase + loopOffset()) : 0.0f;
    loopInfo[2] = (float) LOOP_LENS[idxOf (g[G_LLEN], NUM_LOOP_LENS)];
    loopInfo[3] = loopOn ? (float) (loopMapRaw (pos16) + loopOffset()) : (float) pos16;
    loopInfo[4] = (float) pos16;
    inL = inR = nullptr;          // the input was for this call only
}

// ----------------------------------------------------------------- RAM machine: takes
// A REC trig starts a take: TAKE steps of the track, from SRC at LEVL (at a half, a quarter or an eighth of the rate
// when it would not fit), over what was there (OVDB keeps that much of it). THRS holds it until the source passes the
// threshold. The tracks and the master come out LATENCY samples late (the BOX), so their takes start that much later
// and line up with the steps. The buffer plays while it records: behind the head the new take, ahead of it the old.
void Engine::ramStart (int ti, int step)
{
    if (ti < 0 || ti >= NUM_TRACKS) return;
    if (step >= NUM_STEPS) step = -1;
    const Track& t = tr[ti];
    RamBuf& r = ram[ti];
    // a take cut short by the next REC trig (a take longer than the track's loop) stays as it is: it counts as a
    // change of the buffer (the hosts keep it)
    if (r.state != RAM_IDLE && r.wpos > 0) r.ver++;
    r.src = idxOf (lock (t, step, P_RSRC), RAM_SRCS);
    const double want = RAM_STEPS[idxOf (lock (t, step, P_RLEN), RAM_TAKES)] * samplesPerStep (t);
    int div = 1;
    while (want / (double) div > (double) RAM_LEN && div < 8) div *= 2;
    r.div = div;
    r.total = maxi ((int) minf_d (want / (double) div, (double) RAM_LEN), 16);
    const float lv = 2.0f * lock (t, step, P_RLEV);
    r.gain = lv * lv;                                     // 0 dB at 12 o'clock, +12 dB at the top
    const float th = lock (t, step, P_RTHR);
    r.thr = th < 0.002f ? 0.0f : exp2f_ ((-60.0f + 54.0f * th) * 0.16609640f);   // -60 .. -6 dBFS
    r.ovd = lock (t, step, P_ROVD);
    r.oldLen = r.len;
    r.rate = sr / (float) div;
    r.wpos = 0; r.dcnt = 0; r.acc = 0.0f;
    // the tracks leave the BOX LATENCY samples late; MAIN also crosses the master's 4x oversampling (twice when
    // GRAIN or OTT sit in the chain)
    const int osPasses = lite ? 0 : (g[G_GON] > 0.5f || g[G_OON] > 0.5f ? 2 : 1);
    r.wait = r.src == RAM_IN ? 0 : (int) LATENCY + (r.src == RAM_MAIN ? (osPasses * OS4_DELAY_X2 + 1) / 2 : 0);
    r.prNext = 0;              // PRISM: the analysis follows the new take (its old frames stay ahead of the head)
    r.state = r.wait > 0 ? RAM_WAIT : (r.thr > 0.0f ? RAM_ARMED : RAM_REC);
    for (int b = 0; b < RAM_PEAKS; ++b) r.pk[b] = 0.0f;
    r.pkVer++;
}

void Engine::ramRecord (int n, const float* L, const float* R)
{
    int mainDone = 0;
    for (int ti = 0; ti < NUM_TRACKS; ++ti)
    {
        RamBuf& r = ram[ti];
        if (r.state == RAM_IDLE) continue;
        const float* x;
        if (r.src == RAM_IN) x = inM;
        else if (r.src == RAM_MAIN)
        {
            if (! mainDone) { for (int i = 0; i < n; ++i) mainM[i] = 0.5f * (L[i] + R[i]); mainDone = 1; }
            x = mainM;
        }
        else x = trkOut[mini (r.src - 2, NUM_TRACKS - 1)];
        short* m = ramMem[ti];
        const float keep = r.ovd * (1.0f / 32768.0f), invDiv = 1.0f / (float) r.div;
        int wrote = 0;
        for (int i = 0; i < n && r.state != RAM_IDLE; ++i)
        {
            if (r.state == RAM_WAIT) { if (--r.wait <= 0) r.state = r.thr > 0.0f ? RAM_ARMED : RAM_REC; continue; }
            const float s = x[i] * r.gain;
            if (r.state == RAM_ARMED) { if (absf (s) < r.thr) continue; r.state = RAM_REC; }
            r.acc += s;
            if (++r.dcnt < r.div) continue;
            float y = r.acc * invDiv;
            r.acc = 0.0f; r.dcnt = 0;
            if (keep > 0.0f && r.wpos < r.oldLen) y += (float) m[r.wpos] * keep;
            if (isBad (y)) y = 0.0f;
            const int q = (int) floorf_ (clampf (y, -1.0f, 1.0f) * 32768.0f + 0.5f);
            m[r.wpos] = (short) (q > 32767 ? 32767 : q);
            float& pk = r.pk[(u32) r.wpos * (u32) RAM_PEAKS / (u32) r.total];
            pk = maxf (pk, absf ((float) m[r.wpos] * (1.0f / 32768.0f)));
            r.wpos++; wrote = 1;
            if (r.wpos > r.len) r.len = r.wpos;
            if (r.wpos >= r.total) { r.len = r.total; r.state = RAM_IDLE; r.ver++; }
        }
        if (wrote) r.pkVer++;
    }
}

void Engine::ramLoaded (int ti, int len, float rate)
{
    if (ti < 0 || ti >= NUM_TRACKS) return;
    RamBuf& r = ram[ti];
    r.state = RAM_IDLE;
    r.len = mini (maxi (len, 0), (int) RAM_LEN);
    r.prNext = 0;              // PRISM: analysed again from the start; no frame of the old content plays meanwhile
    for (int f = 0; f < PR_FRAMES; ++f) prN[ti][f] = 0;
    r.rate = rate >= 1000.0f && rate <= 400000.0f ? rate : sr;
    for (int b = 0; b < RAM_PEAKS; ++b) r.pk[b] = 0.0f;
    for (int i = 0; i < r.len; ++i)
    {
        float& pk = r.pk[(u32) i * (u32) RAM_PEAKS / (u32) r.len];
        pk = maxf (pk, absf ((float) ramMem[ti][i] * (1.0f / 32768.0f)));
    }
    r.ver++; r.pkVer++;
}

// v80: a track's take copied onto another (Paste track: a RAM track pasted), or two tracks' takes exchanged (Move
// track): as if the host had loaded them there (their versions move on: the hosts keep them where they now are)
void Engine::ramCopy (int from, int to)
{
    if (from < 0 || from >= NUM_TRACKS || to < 0 || to >= NUM_TRACKS || from == to) return;
    const int len = ram[from].state == RAM_IDLE ? ram[from].len : 0;
    if (len <= 0) { ramClear (to); return; }
    for (int i = 0; i < len; ++i) ramMem[to][i] = ramMem[from][i];
    ramLoaded (to, len, ram[from].rate);
}
void Engine::ramSwap (int a, int b)
{
    if (a < 0 || a >= NUM_TRACKS || b < 0 || b >= NUM_TRACKS || a == b) return;
    const int la = ram[a].state == RAM_IDLE ? ram[a].len : 0, lb = ram[b].state == RAM_IDLE ? ram[b].len : 0;
    const float ra = ram[a].rate, rb = ram[b].rate;
    for (int i = 0, n = maxi (la, lb); i < n; ++i) { const short x = ramMem[a][i]; ramMem[a][i] = ramMem[b][i]; ramMem[b][i] = x; }
    if (lb > 0) ramLoaded (a, lb, rb); else ramClear (a);
    if (la > 0) ramLoaded (b, la, ra); else ramClear (b);
}

void Engine::ramInfo (float* out) const
{
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        const RamBuf& r = ram[t];
        float* o = out + t * RAM_INFO;
        o[0] = (float) r.len; o[1] = r.rate; o[2] = (float) r.ver;
        o[3] = r.state == RAM_IDLE ? 0.0f : (r.state == RAM_REC ? 2.0f : 1.0f);
        o[4] = r.state == RAM_IDLE || r.total <= 0 ? 0.0f : (float) r.wpos / (float) r.total;
        o[5] = (float) r.pkVer;
    }
}

// ----------------------------------------------------------------- PRISM (the RAM machine's spectral side)
// The buffers some PRISM voice can play (a RAM track with its MIX up, or locked up on a step: its own buffer and
// its BUF) are analysed a few frames per chunk: while a take is recorded the analysis follows the write head (one
// frame every 512 samples), after a buffer is loaded it catches up in the background. Never more than PR_BUDGET FFTs
// in a chunk, whatever the number of buffers.
void Engine::prismAnalyze()
{
    if (lite) return;               // the plain-JS fallback has no PRISM
    u32 want = 0;
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        const Track& tk = tr[t];
        if (machineOf (tk.p[P_MACH]) != M_RAM) continue;
        bool on = tk.p[P_PMIX] > 0.001f;
        for (int s = 0; s < NUM_STEPS && ! on; ++s) on = tk.lock[s][P_PMIX] > 0.001f;
        if (! on) continue;
        want |= 1u << t;
        const int b = idxOf (tk.p[P_RBUF], NUM_TRACKS + 1);
        if (b > 0) want |= 1u << (b - 1);
        for (int s = 0; s < NUM_STEPS; ++s)
            if (tk.lock[s][P_RBUF] >= 0.0f) { const int lb = idxOf (tk.lock[s][P_RBUF], NUM_TRACKS + 1); if (lb > 0) want |= 1u << (lb - 1); }
    }
    if (! want) return;
    int budget = PR_BUDGET;
    for (int k = 0; k < NUM_TRACKS && budget > 0; ++k)
    {
        const int b = (prRR + k) % NUM_TRACKS;
        if (! ((want >> b) & 1u)) continue;
        RamBuf& r = ram[b];
        const bool rec = r.state != RAM_IDLE;
        const int lim = rec ? r.wpos : r.len;
        while (budget > 0 && r.prNext < PR_FRAMES)
        {
            const int c = r.prNext * PR_HOP;
            // recording: the frame's whole window must be written; idle: every frame that starts inside the take
            if (rec ? c + PR_WIN > lim : c >= lim) break;
            prismFrame (b, r.prNext, lim);
            r.prNext++; budget--;
        }
    }
    prRR = (prRR + 1) % NUM_TRACKS;
}

// one frame: the window from f * PR_HOP (it stands for the sound around its centre, f * PR_HOP + PR_WIN / 2; zeros past
// the end of the take), its peaks (parabolic interpolation on the log magnitude), the PR_PK strongest, strongest first
void Engine::prismFrame (int b, int f, int lim)
{
    const short* m = ramMem[b];
    const int c0 = f * PR_HOP;
    const float K = 1.0f / 32768.0f;
    for (int i = 0; i < PR_WIN; ++i) { const int k = c0 + i; prX[i] = k >= 0 && k < lim ? (float) m[k] * K * prWin[i] : 0.0f; }
    prFft.forward (prX, prRe, prIm);
    const int H = PR_WIN / 2;
    for (int k = 0; k <= H; ++k) prM[k] = prRe[k] * prRe[k] + prIm[k] * prIm[k];
    // a sine of amplitude A peaks at A * PR_WIN / 4 (Hann): below -80 dBFS a peak is ignored
    const float g = 4.0f / (float) PR_WIN, thr = (1e-4f / g) * (1e-4f / g);
    float fA[PR_PK], fF[PR_PK]; int nt = 0;
    for (int k = 2; k < H - 1; ++k)
    {
        const float b0 = prM[k];
        if (b0 <= thr || b0 <= prM[k - 1] || b0 < prM[k + 1]) continue;
        const float la = log2f_ (prM[k - 1] + 1e-30f), lb = log2f_ (b0), lc = log2f_ (prM[k + 1] + 1e-30f);
        const float den = la - 2.0f * lb + lc;
        const float d = den < -1e-9f ? clampf (0.5f * (la - lc) / den, -0.5f, 0.5f) : 0.0f;
        const float lp = lb - 0.25f * (la - lc) * d;                       // log2 of the peak's squared magnitude
        const float amp = exp2f_ (0.5f * lp) * g;
        if (nt == PR_PK && amp <= fA[PR_PK - 1]) continue;
        int j = nt < PR_PK ? nt++ : PR_PK - 1;
        while (j > 0 && fA[j - 1] < amp) { fA[j] = fA[j - 1]; fF[j] = fF[j - 1]; --j; }
        fA[j] = amp; fF[j] = ((float) k + d) / (float) PR_WIN;
    }
    for (int j = 0; j < nt; ++j) { prF[b][f][j] = fF[j]; prA[b][f][j] = fA[j]; }
    prN[b][f] = (unsigned char) nt;
}

// The wave tables of the PRISM oscillators: SINE, TRI, SAW (falling ramp) and PULSE (25 %), each one band-limited per
// octave (octave k holds the first 2^k harmonics, so a partial read from it at up to 0.45 / 2^k cycles per sample never
// folds over), written by an inverse FFT from the shapes' Fourier series, every shape at the RMS of the sine (the morph
// keeps its loudness). The fundamental of every shape is a sine in phase with SINE: the morph never cancels it.
void Engine::prismTables()
{
    static_assert ((int) PRT_N == (int) PR_WIN, "the tables use the analysis FFT");
    const int H = PRT_N / 2;
    const float half = 0.5f * (float) PRT_N;
    for (int s = 0; s < PRT_SHAPES; ++s)
    {
        for (int k = 0; k < PRT_LEV; ++k)
        {
            for (int i = 0; i <= H; ++i) prRe[i] = prIm[i] = 0.0f;
            const int nh = 1 << k;
            for (int h = 1; h <= nh && h < H; ++h)
            {
                const float fh = (float) h;
                float a = 0.0f, b = 0.0f;                    // cos and sin coefficients
                if (s == 0) b = h == 1 ? 1.0f : 0.0f;
                else if (s == 1) b = (h & 1) ? (8.0f / (PI * PI * fh * fh)) * (((h - 1) / 2) & 1 ? -1.0f : 1.0f) : 0.0f;
                else if (s == 2) b = 2.0f / (PI * fh);
                else
                {
                    const float c = 2.0f / (PI * fh) * sin2pi (0.5f * fh * 0.25f);   // sin (pi h d), d = 0.25, centred on 1/4
                    if (h & 1) b = c * (((h - 1) / 2) & 1 ? -1.0f : 1.0f);
                    else       a = c * ((h / 2) & 1 ? -1.0f : 1.0f);
                }
                prRe[h] = a * half; prIm[h] = -b * half;
            }
            prFft.inverse (prRe, prIm, prX);
            float* t = prTab[s][k];
            for (int i = 0; i < PRT_N; ++i) t[i] = prX[i];
            t[PRT_N] = t[0];
        }
        // the same scale for every octave of a shape: the RMS of its richest table, brought to the sine's
        double e = 0.0;
        for (int i = 0; i < PRT_N; ++i) e += (double) prTab[s][PRT_LEV - 1][i] * (double) prTab[s][PRT_LEV - 1][i];
        const float rms = sqrtf_ ((float) (e / (double) PRT_N));
        const float gn = rms > 1e-9f ? 0.70710678f / rms : 1.0f;
        for (int k = 0; k < PRT_LEV; ++k) for (int i = 0; i <= PRT_N; ++i) prTab[s][k][i] *= gn;
    }
}

// atan2 (im, re) in cycles, [0, 1) (|error| < 2e-6 cycle): a phasor's phase, when the bank moves to the tables
static inline float phase01 (float re, float im)
{
    const float ax = absf (re), ay = absf (im), mx = maxf (ax, ay);
    if (mx < 1e-20f) return 0.0f;
    const float a = minf (ax, ay) / mx, s = a * a;
    float r = (((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s) * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (re < 0.0f) r = PI - r;
    if (im < 0.0f) r = -r;
    const float ph = r * (1.0f / TWO_PI);
    return ph < 0.0f ? ph + 1.0f : (ph >= 1.0f ? ph - 1.0f : ph);
}

// SLIC: how long the spectrum is held, in steps of the track (OFF, 1/32, 1/16, 1/8, 1/4, 1/2, 1 bar)
static const float PR_SLICE_ST[PR_SLICES] = { 0.0f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };

// A PRISM voice, once per chunk: the frame under the playhead (with SLIC, the one caught at the start of the slice), its
// strongest partials up to VOIC of them, those inside the band (WDTH right of 12 o'clock) or outside the notch (left),
// tilted (TILT), warped around the band centre (WARP), transposed (TUNE, NOTE, OCT) and, with QNT, snapped to the scale
// (partials landing on the same note become one). Each one is matched to the oscillator already playing within 4 % of
// it (it glides there, GLID) or given a silent oscillator (a sounding one is never taken: no click); the others fade.
// Amplitudes follow their targets (BLUR). The bank then writes the chunk into prOut: rotating phasors (four at a time)
// for sines, or the wave tables once WAVE or DYN is up (DYN: the louder a partial, against the voice's loudest, the
// further its wave goes towards PULSE).
void Engine::prismBank (Voice& v, const float* P, float tuneSt, int n)
{
    const RamBuf& rb = ram[v.rbuf];
    const int voices = 1 + idxOf (P[P_PVOI], PR_K);
    // the band around CNTR (20 Hz .. 20 kHz): 12 o'clock passes everything; to the right a band narrows from 10 octaves
    // to 0.1, to the left a notch widens from 0.1 octave to 6
    const float wd = P[P_PWID];
    const int band = wd > 0.51f ? 1 : (wd < 0.49f ? -1 : 0);
    const float cHz = 20.0f * exp2f_ (P[P_PCTR] * 10.0f);
    float lo = 0.0f, hi = 0.0f;
    if (band != 0)
    {
        const float hw = band > 0 ? exp2f_ (0.05f + (1.0f - wd) * 2.0f * 4.95f) : exp2f_ (0.05f + (0.49f - wd) * (2.95f / 0.49f));
        lo = cHz / hw; hi = cHz * hw;
    }
    const float we = exp2f_ ((P[P_PWRP] - 0.5f) * 1.4f);        // WARP: f' = c (f / c)^e, e from 0.62 to 1.62
    const bool warp = absf (we - 1.0f) > 0.002f;
    const float tl = (P[P_PTLT] - 0.5f) * 1.5f;                 // TILT: +-4.5 dB per octave around 1 kHz
    const bool tilt = absf (tl) > 0.004f;
    const int oct = idxOf (P[P_POCT], PR_OCTS) - 2;
    const float pitch = semis (tuneSt + 12.0f * (float) oct), isr = 1.0f / sr;
    const bool qnt = idxOf (P[P_PQNT], 2) == 1;
    const u32 qm0 = quantMask(), qm = qm0 ? qm0 : 0xFFFu;          // no scale: semitones
    int f = (int) ((v.rpos - (double) (PR_WIN / 2)) * (1.0 / (double) PR_HOP) + 0.5);   // the frame centred nearest
    f = f < 0 ? 0 : (f >= PR_FRAMES ? PR_FRAMES - 1 : f);
    // SLIC: the spectrum is caught at the start of each slice (from the voice's hit, in steps of the track) and held
    const int sl = idxOf (P[P_PSLC], PR_SLICES);
    if (sl > 0)
    {
        if (v.pFresh || v.pSlL <= 0.0f)
        {
            const float len = maxf ((float) samplesPerStep (tr[v.ti]) * PR_SLICE_ST[sl], (float) CHUNK);
            v.pSlL = v.pFresh ? len : v.pSlL + len;
            if (v.pSlL <= 0.0f) v.pSlL = len;
            v.pSlF = f;
        }
        f = v.pSlF;
        v.pSlL -= (float) n;
    }
    // the same frame read the same way as last chunk (and not analysed again since): the targets stand
    const float key[10] = { (float) f, (float) voices, lo * (float) band, hi, cHz, pitch, we, tl, qnt ? (float) qm : -1.0f,
                            (float) (rb.prNext > f ? 1 : 0) + (float) (rb.ver & 0xFFFF) * 2.0f };
    bool same = true;
    for (int k = 0; k < 10; ++k) if (key[k] != v.pKey[k]) { same = false; v.pKey[k] = key[k]; }
    if (! same) {
    const int np = rb.len > 0 ? (int) prN[v.rbuf][f] : 0;
    const float* fF = prF[v.rbuf][f];
    const float* fA = prA[v.rbuf][f];
    float cInc[PR_K], cAmp[PR_K];
    int nc = 0;
    for (int k = 0; k < np && nc < voices; ++k)
    {
        const float hz = fF[k] * rb.rate;
        if (band > 0 && (hz < lo || hz > hi)) continue;
        if (band < 0 && hz > lo && hz < hi) continue;
        float amp = fA[k];
        if (tilt) amp *= minf (exp2f_ (tl * log2f_ (hz * 0.001f)), 6.0f);
        float out = (warp ? cHz * exp2f_ (we * log2f_ (hz / cHz)) : hz) * pitch;
        if (qnt && out > 1.0f) out *= semis (qPitch (0.0f, out, qm));
        const float inc = out * isr;
        if (inc <= 0.0f || inc >= 0.45f) continue;
        if (qnt)
        {
            int d = -1;
            for (int c = 0; c < nc && d < 0; ++c) if (absf (cInc[c] - inc) < 1e-3f * inc) d = c;
            if (d >= 0) { cAmp[d] = sqrtf_ (cAmp[d] * cAmp[d] + amp * amp); continue; }
        }
        cInc[nc] = inc; cAmp[nc] = amp; ++nc;
    }
    u32 used = 0;
    int pend[PR_K], npend = 0;
    for (int ci = 0; ci < nc; ++ci)
    {
        // the nearest oscillator within 4 % (0.7 semitone); compared without a division
        int best = -1; float bd = 0.04f * cInc[ci];
        for (int j = 0; j < v.pAct; ++j)
        {
            if (((used >> j) & 1u) || (v.pAmpT[j] <= 0.0f && v.pAmp[j] < 1e-6f) || v.pHz[j] <= 0.0f) continue;
            const float d = absf (cInc[ci] - v.pHz[j]);
            if (d < bd) { bd = d; best = j; }
        }
        if (best < 0) { pend[npend++] = ci; continue; }
        used |= 1u << best;
        v.pHz[best] = cInc[ci]; v.pIncT[best] = cInc[ci]; v.pAmpT[best] = cAmp[ci];
    }
    for (int j = 0; j < v.pAct; ++j) if (! ((used >> j) & 1u)) v.pAmpT[j] = 0.0f;
    for (int q = 0; q < npend; ++q)
    {
        int j = 0;
        while (j < PR_K && (((used >> j) & 1u) || v.pAmp[j] > 1e-5f)) ++j;
        if (j >= PR_K) break;
        const int ci = pend[q];
        used |= 1u << j;
        const float ph = v.rng.uni();
        v.pRe[j] = cos2pi (ph); v.pIm[j] = sin2pi (ph); v.pPh[j] = ph;
        v.pHz[j] = cInc[ci]; v.pInc[j] = v.pIncT[j] = cInc[ci]; v.pAmp[j] = 0.0f; v.pAmpT[j] = cAmp[ci];
        v.pCs[j] = cos2pi (cInc[ci]); v.pSn[j] = sin2pi (cInc[ci]);
        if (j >= v.pAct) v.pAct = j + 1;
    }
    }   // ! same
    // sines or tables: a change of reading carries each oscillator's phase over
    const float wav = clampf (P[P_PWAV], 0.0f, 1.0f), dyn = clampf (P[P_PDYN], 0.0f, 1.0f);
    const int tab = wav > 0.001f || dyn > 0.001f ? 1 : 0;
    if (tab != v.pTab)
    {
        for (int j = 0; j < PR_K; ++j)
        {
            if (tab) v.pPh[j] = phase01 (v.pRe[j], v.pIm[j]);
            else { v.pRe[j] = cos2pi (v.pPh[j]); v.pIm[j] = sin2pi (v.pPh[j]); v.pCs[j] = cos2pi (v.pInc[j]); v.pSn[j] = sin2pi (v.pInc[j]); }
        }
        v.pTab = tab;
    }
    // glide and blur for this chunk (a fresh voice reaches its first frame within the chunk)
    const float gS = P[P_PGLD] < 0.002f ? 1.0f : 1.0f - expf_ (-(float) n / (0.001f * powf_ (1000.0f, P[P_PGLD]) * sr));
    const float bS = v.pFresh ? 1.0f : 1.0f - expf_ (-(float) n / (0.003f * powf_ (600.0f, P[P_PBLR]) * sr));
    v.pFresh = 0;
    alignas (16) float cs[PR_K], sn[PR_K], a0[PR_K], da[PR_K];
    const int nAct = (v.pAct + 3) & ~3;
    const float invN = 1.0f / (float) n;
    float loud = 0.0f;
    for (int j = 0; j < nAct; ++j)
    {
        if (j >= v.pAct) { cs[j] = 1.0f; sn[j] = 0.0f; a0[j] = da[j] = 0.0f; continue; }
        // the frequency moves (a glide, a new target): the phasors' rotation follows (the tables need nothing)
        if (v.pInc[j] != v.pIncT[j])
        {
            const float d = v.pIncT[j] - v.pInc[j];
            v.pInc[j] = absf (d) < 1e-7f * v.pIncT[j] ? v.pIncT[j] : v.pInc[j] + d * gS;
            if (! tab) { v.pCs[j] = cos2pi (v.pInc[j]); v.pSn[j] = sin2pi (v.pInc[j]); }
        }
        const float a1 = v.pAmp[j] + (v.pAmpT[j] - v.pAmp[j]) * bS;
        cs[j] = v.pCs[j]; sn[j] = v.pSn[j];
        a0[j] = v.pAmp[j]; da[j] = (a1 - v.pAmp[j]) * invN;
        v.pAmp[j] = a1;
        loud = maxf (loud, a1);
    }
    // DYN: the loudest partial of the voice, released over ~0.5 s
    v.pPk = maxf (loud, v.pPk * expf_ (-(float) n / (0.5f * sr)));
    if (tab)
    {
        for (int i = 0; i < n; ++i) prOut[i] = 0.0f;
        const float N = (float) PRT_N;
        const float invPk = v.pPk > 1e-9f ? 1.0f / v.pPk : 0.0f;
        for (int j = 0; j < v.pAct; ++j)
        {
            const float inc = v.pInc[j];
            float ph = v.pPh[j];
            if (a0[j] < 1e-7f && v.pAmp[j] < 1e-7f) { ph += inc * (float) n; v.pPh[j] = ph - floorf_ (ph); continue; }
            int lev = (int) log2f_ (0.45f / maxf (inc, 1e-7f));
            lev = lev < 0 ? 0 : (lev >= PRT_LEV ? PRT_LEV - 1 : lev);
            float w = wav;
            if (dyn > 0.001f) w = wav + (1.0f - wav) * dyn * sqrtf_ (minf (v.pAmp[j] * invPk, 1.0f));
            const float x3 = clampf (w, 0.0f, 1.0f) * 3.0f;
            int sa = (int) x3; if (sa > 2) sa = 2;
            const float wf = x3 - (float) sa;
            const float* ta = prTab[sa][lev];
            const float* tb = prTab[sa + 1][lev];
            float a = a0[j]; const float d = da[j];
            if (wf < 1e-4f || wf > 0.9999f)
            {
                const float* t = wf < 1e-4f ? ta : tb;
                for (int i = 0; i < n; ++i)
                {
                    const float x = ph * N; const int ii = (int) x; const float fr = x - (float) ii;
                    prOut[i] += a * (t[ii] + fr * (t[ii + 1] - t[ii]));
                    a += d; ph += inc; if (ph >= 1.0f) ph -= 1.0f;
                }
            }
            else
                for (int i = 0; i < n; ++i)
                {
                    const float x = ph * N; const int ii = (int) x; const float fr = x - (float) ii;
                    const float ya = ta[ii] + fr * (ta[ii + 1] - ta[ii]), yb = tb[ii] + fr * (tb[ii + 1] - tb[ii]);
                    prOut[i] += a * (ya + wf * (yb - ya));
                    a += d; ph += inc; if (ph >= 1.0f) ph -= 1.0f;
                }
            v.pPh[j] = ph;
        }
    }
    else
    {
    alignas (16) float acc[4 * CHUNK];
    for (int i = 0; i < 4 * n; ++i) acc[i] = 0.0f;
    for (int j0 = 0; j0 < nAct; j0 += 4)
    {
        if (a0[j0] + a0[j0 + 1] + a0[j0 + 2] + a0[j0 + 3] < 1e-7f && v.pAmp[j0] + v.pAmp[j0 + 1] + v.pAmp[j0 + 2] + v.pAmp[j0 + 3] < 1e-7f) continue;
#if defined (__wasm_simd128__)
        const v128_t C = wasm_v128_load (cs + j0), S = wasm_v128_load (sn + j0), D = wasm_v128_load (da + j0);
        v128_t RE = wasm_v128_load (v.pRe + j0), IM = wasm_v128_load (v.pIm + j0), A = wasm_v128_load (a0 + j0);
        for (int i = 0; i < n; ++i)
        {
            const v128_t r = wasm_f32x4_sub (wasm_f32x4_mul (RE, C), wasm_f32x4_mul (IM, S));
            IM = wasm_f32x4_add (wasm_f32x4_mul (RE, S), wasm_f32x4_mul (IM, C));
            RE = r;
            wasm_v128_store (acc + 4 * i, wasm_f32x4_add (wasm_v128_load (acc + 4 * i), wasm_f32x4_mul (A, IM)));
            A = wasm_f32x4_add (A, D);
        }
        wasm_v128_store (v.pRe + j0, RE); wasm_v128_store (v.pIm + j0, IM);
#else
        float re[4], im[4], a[4];
        for (int l = 0; l < 4; ++l) { re[l] = v.pRe[j0 + l]; im[l] = v.pIm[j0 + l]; a[l] = a0[j0 + l]; }
        for (int i = 0; i < n; ++i)
            for (int l = 0; l < 4; ++l)
            {
                const float r = re[l] * cs[j0 + l] - im[l] * sn[j0 + l];
                im[l] = re[l] * sn[j0 + l] + im[l] * cs[j0 + l];
                re[l] = r;
                acc[4 * i + l] += a[l] * im[l];
                a[l] += da[j0 + l];
            }
        for (int l = 0; l < 4; ++l) { v.pRe[j0 + l] = re[l]; v.pIm[j0 + l] = im[l]; }
#endif
        // the phasors' length back to 1 (one Newton step)
        for (int l = 0; l < 4; ++l)
        {
            const int j = j0 + l;
            const float k = 1.5f - 0.5f * (v.pRe[j] * v.pRe[j] + v.pIm[j] * v.pIm[j]);
            v.pRe[j] *= k; v.pIm[j] *= k;
        }
    }
    for (int i = 0; i < n; ++i) prOut[i] = (acc[4 * i] + acc[4 * i + 1]) + (acc[4 * i + 2] + acc[4 * i + 3]);
    }
    while (v.pAct > 0 && v.pAmp[v.pAct - 1] < 1e-7f && v.pAmpT[v.pAct - 1] <= 0.0f) --v.pAct;
}

const float* Engine::prismInfo (int t)
{
    for (int k = 0; k < 2 * PR_K; ++k) prInfo[k] = 0.0f;
    if (t < 0 || t >= NUM_TRACKS) return prInfo;
    const Voice& v = tr[t].voice[tr[t].cur];
    if (! v.active || v.machine != M_RAM) return prInfo;
    for (int j = 0; j < v.pAct; ++j) { prInfo[2 * j] = v.pInc[j] * sr; prInfo[2 * j + 1] = v.pAmp[j]; }
    return prInfo;
}

// POS: slides the window by -16 .. +16 steps (tempo locked: the repeat keeps its phase)
double Engine::loopOffset() const
{
    return (double) (idxOf (g[G_LPOS], 33) - 16);
}

} // namespace sk
