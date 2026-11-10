// IMPAKT — WEFT (v78, sk_weft.h): Mutable Instruments' Warps (core/mi/warps, MIT, © Emilie Gillet) as an FX machine.
// Warps' Modulator runs as released at its own rate (96 kHz), fed and read through the engine's halfbands, in blocks
// of 48 (a FIFO: its vocoder's filter bank decimates by 12).
#include "sk_weft.h"
#include "sk_os.h"

#include <new>

#include "stmlib/stmlib.h"
#include "warps/dsp/modulator.h"

namespace sk {

namespace {

enum { BLOCK = 48 };

struct WeftUnit
{
    warps::Modulator mod;
    warps::FloatFrame in[BLOCK], out[BLOCK];
    int   k;                              // the FIFO's place (in and out together: a block late)
    HalfbandUp<80>   upC, upM;
    HalfbandDown<80> dnL, dnR;
    float dry[2][WEFT_DRY]; int dw;       // the direct sound, delayed as the effect is
    float sr, noteOff;                    // the track's rate; the internal carrier's correction (Warps' tables: 96 kHz)
    float vocPk, algoNow;
};

inline WeftUnit* unitOf (void* mem)
{
    unsigned char* p = static_cast<unsigned char*> (mem);
    const __SIZE_TYPE__ a = (__SIZE_TYPE__) p;
    return reinterpret_cast<WeftUnit*> (p + ((16 - (a & 15)) & 15));
}

} // namespace

unsigned weftBytes() { return (unsigned) sizeof (WeftUnit) + 16u; }

void weftInit (void* mem, float sr)
{
    WeftUnit* u = unitOf (mem);
    unsigned char* b = reinterpret_cast<unsigned char*> (u);
    for (unsigned i = 0; i < (unsigned) sizeof (WeftUnit); ++i) b[i] = 0;
    new (&u->mod) warps::Modulator();
    u->mod.Init (96000.0f);
    u->upC.reset (HB1); u->upM.reset (HB1); u->dnL.reset (HB1); u->dnR.reset (HB1);
    u->k = 0; u->dw = 0;
    u->sr = sr;
    // (the internal oscillator's pitch from Warps' tables, made for 96 kHz, at twice this rate)
    u->noteOff = 12.0f * log2f_ (96000.0f / (2.0f * sr));
    u->vocPk = 0.0f; u->algoNow = 0.0f;
}

void weftProcess (void* mem, const WeftParams& p, const float* car, const float* mod, const float* xL, const float* xR,
                  float* wL, float* wR, float* dL, float* dR, int n)
{
    WeftUnit* u = unitOf (mem);
    // the parameters, as Warps' CV scaler leaves them (ALGO's bottom stretched: no wavefolder bleed)
    warps::Parameters* wp = u->mod.mutable_parameters();
    float algo = clampf (p.algo, 0.0f, 1.0f);
    if (algo <= 0.125f) algo = clampf (algo * 1.08f - 0.01f, 0.0f, 1.0f);
    if (p.freeze && algo >= 0.7f) algo = 1.0f;
    wp->channel_drive[0] = clampf (p.lvl1, 0.0f, 1.0f) * clampf (p.lvl1, 0.0f, 1.0f);
    wp->channel_drive[1] = clampf (p.lvl2, 0.0f, 1.0f) * clampf (p.lvl2, 0.0f, 1.0f);
    wp->modulation_algorithm = algo;
    wp->modulation_parameter = clampf (p.timb, 0.0f, 1.0f);
    wp->note = p.note + u->noteOff;
    wp->carrier_shape = p.carrier < 0 ? 0 : (p.carrier > 3 ? 3 : p.carrier);
    // the shifter: ALGO its shift (and the external carrier's phase), no CV
    wp->frequency_shift_pot = algo;
    wp->frequency_shift_cv = 0.0f;
    wp->phase_shift = algo;
    u->mod.set_easter_egg (p.shift != 0);
    u->algoNow = algo;
    float pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float c0, c1, m0, m1;
        u->upC.process (car ? car[i] : 0.0f, c0, c1);
        u->upM.process (mod[i], m0, m1);
        float o[2][2];
        const float ci[2] = { c0, c1 }, mi[2] = { m0, m1 };
        for (int j = 0; j < 2; ++j)
        {
            u->in[u->k].l = ci[j]; u->in[u->k].r = mi[j];
            o[j][0] = u->out[u->k].l; o[j][1] = u->out[u->k].r;
            if (++u->k >= BLOCK)
            {
                u->k = 0;
                u->mod.Process (u->in, u->out, BLOCK);
            }
        }
        const float yl = u->dnL.process (o[0][0], o[1][0]);
        const float yr = p.shift ? u->dnR.process (o[0][1], o[1][1]) : yl;     // (WARP: its main out a side; SHFT: the other sideband right)
        wL[i] = isBad (yl) ? 0.0f : yl;
        wR[i] = isBad (yr) ? 0.0f : yr;
        pk = maxf (pk, absf (yl));
        // the direct sound, as late
        u->dry[0][u->dw] = xL[i]; u->dry[1][u->dw] = xR[i];
        const int r = (u->dw - WEFT_LAT + WEFT_DRY) % WEFT_DRY;
        dL[i] = u->dry[0][r]; dR[i] = u->dry[1][r];
        u->dw = (u->dw + 1) % WEFT_DRY;
    }
    u->vocPk = maxf (pk, u->vocPk * 0.9f);
}

void weftInfo (void* mem, float* out3)
{
    WeftUnit* u = unitOf (mem);
    out3[0] = u->vocPk;
    const float a = minf (u->algoNow * 8.0f, 5.999f);
    out3[1] = u->algoNow;
    out3[2] = a - floorf_ (a);
}

} // namespace sk
