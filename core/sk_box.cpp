// IMPAKT — impulse-response bank. Every IR is synthesised from a physical sketch of the
// object or space (speaker + box filters, modal resonances, image-source rooms with a
// frequency-dependent diffuse tail), so nothing has to be shipped as audio files.
#include "sk_box.h"

namespace sk {

namespace {

struct BQ
{
    float b0, b1, b2, a1, a2, z1, z2;
    enum { LP, HP, BP, PEAK };
    void set (int type, float f, float q, float gainDb, float sr)
    {
        f = clampf (f, 10.0f, 0.45f * sr);
        const float w = f / sr, cw = cos2pi (w), sw = sin2pi (w), al = sw / (2.0f * q);
        const float A = powf_ (10.0f, gainDb / 40.0f);
        float a0 = 1.0f + al;
        switch (type)
        {
            case LP:  b0 = (1.0f - cw) * 0.5f; b1 = 1.0f - cw; b2 = b0; a1 = -2.0f * cw; a2 = 1.0f - al; break;
            case HP:  b0 = (1.0f + cw) * 0.5f; b1 = -(1.0f + cw); b2 = b0; a1 = -2.0f * cw; a2 = 1.0f - al; break;
            case BP:  b0 = al; b1 = 0.0f; b2 = -al; a1 = -2.0f * cw; a2 = 1.0f - al; break;
            default:  b0 = 1.0f + al * A; b1 = -2.0f * cw; b2 = 1.0f - al * A; a0 = 1.0f + al / A; a1 = -2.0f * cw; a2 = 1.0f - al / A; break;
        }
        b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0; z1 = z2 = 0.0f;
    }
    void run (float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float in = x[i], y = b0 * in + z1;
            z1 = b1 * in - a1 * y + z2;
            z2 = b2 * in - a2 * y;
            x[i] = y;
        }
    }
};

struct Ctx
{
    float* h; float* t; float* t2; int L; float sr; Rng* rng;
    void zero (float* x) const { for (int i = 0; i < L; ++i) x[i] = 0.0f; }
    void norm (float* x) const
    {
        double e = 0.0; for (int i = 0; i < L; ++i) e += (double) x[i] * x[i];
        if (e < 1e-20) return;
        const float g = 1.0f / sqrtf_ ((float) e);
        for (int i = 0; i < L; ++i) x[i] *= g;
    }
    void add (const float* src, float w) const { for (int i = 0; i < L; ++i) h[i] += src[i] * w; }
    // speaker / device: impulse through a filter chain, plus a faint diffuse burst through the same chain
    void chain (float* x, const float spec[][4], int nb, float burst) const
    {
        zero (x);
        x[0] = 1.0f;
        const int bl = mini (L, (int) (0.003f * sr) + 1);
        for (int i = 1; i < bl; ++i) x[i] = rng->bi() * burst * (1.0f - (float) i / (float) bl);
        for (int k = 0; k < nb; ++k) { BQ q; q.set ((int) spec[k][0], spec[k][1], spec[k][2], spec[k][3], sr); q.run (x, L); }
    }
    void mode (float* x, float f, float t60, float amp) const
    {
        if (f <= 0.0f || f >= 0.45f * sr) return;
        const float r = decayCoef (t60, sr), w = f / sr, ph = rng->uni();
        const float c = 2.0f * r * cos2pi (w), r2 = r * r;
        float y2 = amp * sin2pi (ph), y1 = amp * r * sin2pi (w + ph);
        x[0] += y2; if (L > 1) x[1] += y1;
        for (int i = 2; i < L; ++i)
        {
            const float y = c * y1 - r2 * y2;
            x[i] += y; y2 = y1; y1 = y;
            if ((i & 1023) == 0 && absf (y1) + absf (y2) < 1e-7f * amp) break;
        }
    }
    void comb (float* x, float ms, float g) const   // open-back / reflection: x[n] += g x[n-d]
    {
        const int d = (int) (ms * 0.001f * sr);
        if (d < 1) return;
        for (int i = L - 1; i >= d; --i) x[i] += g * x[i - d];
    }
    void fadeEnd (float frac) const
    {
        const int n = (int) (L * frac), s = L - n;
        for (int i = s; i < L; ++i) { const float u = (float) (i - s) / (float) n; h[i] *= 0.5f + 0.5f * cos2pi (u * 0.5f); }
    }
    void click (float* x, float amp) const { x[0] += amp; if (L > 2) { x[1] -= amp * 0.5f; } }

    // shoebox room: image sources for the early part + 4-band exponential noise tail
    void room (float X, float Y, float Z, float beta, float hf)
    {
        zero (h);
        float sx = 0.31f * X, sy = 0.42f * Y, sz = 0.38f * Z;
        float mx = 0.68f * X, my = 0.61f * Y, mz = 0.52f * Z;
        if (Z > 4.0f * maxf (X, Y)) { sz = 0.06f * Z; mz = 0.11f * Z; }
        const float V = X * Y * Z, S = 2.0f * (X * Y + X * Z + Y * Z);
        const float lenS = (float) L / sr;
        const float tIS = minf (clampf (0.012f + 0.004f * sqrtf_ (V), 0.012f, 0.09f), lenS * 0.8f);
        const float maxD = tIS * 343.0f;
        int kx = (int) (maxD / X) + 1, ky = (int) (maxD / Y) + 1, kz = (int) (maxD / Z) + 1;
        while ((2 * kx + 1) * (2 * ky + 1) * (2 * kz + 1) > 90000) { if (kx > 1) --kx; if (ky > 1) --ky; if (kz > 1) --kz; }
        for (int i = -kx; i <= kx; ++i)
        {
            const float px = (float) i * X + ((i & 1) ? X - sx : sx), dx = px - mx;
            for (int j = -ky; j <= ky; ++j)
            {
                const float py = (float) j * Y + ((j & 1) ? Y - sy : sy), dy = py - my;
                for (int k = -kz; k <= kz; ++k)
                {
                    const float pz = (float) k * Z + ((k & 1) ? Z - sz : sz), dz = pz - mz;
                    const float d = sqrtf_ (dx * dx + dy * dy + dz * dz);
                    float ts = d / 343.0f;
                    if (ts > tIS) continue;
                    const int refl = (i < 0 ? -i : i) + (j < 0 ? -j : j) + (k < 0 ? -k : k);
                    if (refl > 1) ts += rng->bi() * 0.00012f * sqrtf_ ((float) refl);
                    const float g = powf_ (beta, (float) refl) / maxf (d, 0.25f);
                    const float pos = ts * sr; const int i0 = (int) pos; const float fr = pos - (float) i0;
                    if (i0 >= 0 && i0 + 1 < L) { h[i0] += g * (1.0f - fr); h[i0 + 1] += g * fr; }
                }
            }
        }
        { BQ q; q.set (BQ::LP, 11000.0f * (1.0f - 0.5f * hf), 0.6f, 0.0f, sr); q.run (h, L); }

        // diffuse tail, per band Sabine decay
        zero (t);
        const float t0 = tIS * 0.5f;
        const int i0 = (int) (t0 * sr);
        const float bands[4][3] = { { 0, 280, 0.7f }, { 2, 900, 0.6f }, { 2, 3000, 0.6f }, { 1, 6500, 0.7f } };
        const float bmul[4] = { 1.0f, 1.0f - 0.1f * hf, 1.0f - 0.35f * hf, 1.0f - 0.7f * hf };
        for (int b = 0; b < 4; ++b)
        {
            for (int n = 0; n < L; ++n) t2[n] = n >= i0 ? rng->bi() : 0.0f;
            BQ q; q.set (b == 0 ? BQ::LP : (b == 3 ? BQ::HP : BQ::BP), bands[b][1], bands[b][2], 0.0f, sr); q.run (t2, L);
            const float be = clampf (beta * bmul[b], 0.05f, 0.995f);
            const float rt = clampf (0.161f * V / (S * (1.0f - be * be)), 0.04f, 12.0f);
            const float c = decayCoef (rt, sr);
            float env = 1.0f;
            for (int n = i0; n < L; ++n) { t[n] += t2[n] * env; env *= c; }
        }
        // fade the tail in over [t0, tIS] and match its level to the image sources there
        const int i1 = mini (L - 1, (int) (tIS * sr));
        double ee = 0.0, et = 0.0;
        const int w0 = (int) (0.7f * (float) i1);
        for (int n = w0; n <= i1; ++n) { ee += (double) h[n] * h[n]; et += (double) t[n] * t[n]; }
        float k = et > 1e-20 ? sqrtf_ ((float) (ee / et)) * 0.9f : 0.0f;
        if (ee < 1e-20) k = 0.05f;
        for (int n = i0; n < L; ++n)
        {
            const float f = n < i1 ? (float) (n - i0) / (float) maxi (1, i1 - i0) : 1.0f;
            h[n] += t[n] * k * f;
        }
        fadeEnd (0.12f);
    }
};

} // namespace

int IrBank::generate (int id)
{
    static const float dur[NUM_IRS + 1] = {
        0, 0.05f, 0.04f, 0.35f, 0.06f, 0.09f, 0.2f, 0.15f, 0.7f, 0.9f, 0.045f, 0.14f, 0.05f, 0.06f, 0.06f,
        1.0f, 0.25f, 0.2f, 0.25f, 0.9f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f };
    Ctx c { h, tmp, tmp2, mini (IR_MAX, (int) (dur[id] * sr) + 1), sr, &rng };
    const int L = c.L;
    c.zero (h);
    const float LP = BQ::LP, HP = BQ::HP, PK = BQ::PEAK;
    switch (id)
    {
    case 1: {   // JOUET — toy speaker in a plastic shell
        const float ch[][4] = { { HP, 650, 2.0f, 0 }, { PK, 1500, 3.0f, 9 }, { PK, 3100, 4.0f, 6 }, { LP, 4800, 0.9f, 0 }, { LP, 4800, 0.6f, 0 } };
        c.chain (h, ch, 5, 0.25f); c.norm (h);
        c.zero (tmp); c.mode (tmp, 2050, 0.03f, 1); c.mode (tmp, 2870, 0.025f, 0.8f); c.mode (tmp, 4300, 0.02f, 0.6f); c.norm (tmp);
        c.add (tmp, 0.55f); break; }
    case 2: {   // TÉLÉ — telephone handset
        const float ch[][4] = { { HP, 300, 0.7f, 0 }, { HP, 300, 0.7f, 0 }, { PK, 1000, 1.5f, 5 }, { PK, 2600, 2.5f, 7 }, { LP, 3400, 0.7f, 0 }, { LP, 3400, 0.7f, 0 } };
        c.chain (h, ch, 6, 0.2f); c.norm (h);
        c.zero (tmp); c.mode (tmp, 1800, 0.02f, 1); c.mode (tmp, 2300, 0.015f, 0.7f); c.norm (tmp); c.add (tmp, 0.35f); break; }
    case 3: {   // CANETTE — soda can: closed-open air column + thin aluminium shell
        c.zero (tmp);
        for (int n = 1; n <= 5; ++n) c.mode (tmp, (2 * n - 1) * 343.0f / (4.0f * 0.12f), 0.09f / (float) n + 0.02f, 1.0f / (float) n);
        c.norm (tmp); c.add (tmp, 1.0f);
        c.zero (tmp);
        const float sh[] = { 1870, 2950, 4210, 5330, 6720, 8100, 9650 };
        for (int k = 0; k < 7; ++k) c.mode (tmp, sh[k], 0.35f - 0.035f * k, 1.0f - 0.1f * k);
        c.norm (tmp); c.add (tmp, 0.75f); c.click (h, 0.6f); break; }
    case 4: {   // TALKIE — walkie-talkie
        const float ch[][4] = { { HP, 450, 0.7f, 0 }, { HP, 450, 0.7f, 0 }, { PK, 1250, 2.5f, 8 }, { LP, 2800, 0.7f, 0 }, { LP, 2800, 0.7f, 0 } };
        c.chain (h, ch, 5, 0.3f); c.comb (h, 0.9f, 0.3f); c.norm (h);
        c.zero (tmp); c.mode (tmp, 1650, 0.03f, 1); c.mode (tmp, 2450, 0.02f, 0.7f); c.norm (tmp); c.add (tmp, 0.4f); break; }
    case 5: {   // RADIO — transistor radio
        const float ch[][4] = { { HP, 180, 1.8f, 0 }, { PK, 950, 1.2f, 4 }, { PK, 2800, 2.0f, 5 }, { LP, 4500, 0.8f, 0 }, { LP, 4500, 0.6f, 0 } };
        c.chain (h, ch, 5, 0.2f); c.norm (h);
        c.zero (tmp); c.mode (tmp, 620, 0.04f, 1); c.mode (tmp, 980, 0.035f, 0.8f); c.mode (tmp, 1450, 0.03f, 0.6f); c.norm (tmp); c.add (tmp, 0.45f); break; }
    case 6: {   // B.MUSIQ — small wooden box (15 x 10 x 8 cm)
        c.zero (tmp);
        const float X = 0.15f, Y = 0.10f, Z = 0.08f;
        for (int a = 0; a <= 2; ++a) for (int b = 0; b <= 2; ++b) for (int d = 0; d <= 2; ++d)
        {
            if (a + b + d == 0) continue;
            const float f = 171.5f * sqrtf_ ((a / X) * (a / X) + (b / Y) * (b / Y) + (d / Z) * (d / Z));
            c.mode (tmp, f, 0.05f, 1.0f / (float) (a + b + d));
        }
        c.norm (tmp); c.add (tmp, 1.0f);
        c.zero (tmp); const float wd[] = { 420, 610, 870, 1130, 1490 };
        for (int k = 0; k < 5; ++k) c.mode (tmp, wd[k], 0.12f, 1.0f - 0.15f * k);
        c.norm (tmp); c.add (tmp, 0.8f); c.click (h, 0.4f); break; }
    case 7: {   // MÉGA — megaphone horn
        const float ch[][4] = { { HP, 500, 0.7f, 0 }, { PK, 2100, 3.0f, 6 }, { LP, 4500, 0.7f, 0 }, { LP, 4500, 0.7f, 0 } };
        c.chain (h, ch, 4, 0.3f); c.norm (h);
        c.zero (tmp);
        for (int n = 1; n <= 10; ++n) { const float f = 429.0f * n, gs = (f - 2000.0f) / 1500.0f; c.mode (tmp, f, 0.06f, expf_ (-gs * gs)); }
        c.norm (tmp); c.add (tmp, 0.8f); break; }
    case 8: {   // TUBE — 1 m PVC pipe, open both ends
        c.zero (tmp);
        for (int n = 1; n <= 60; ++n) c.mode (tmp, 171.5f * n, 0.6f / (1.0f + n * 0.06f), 1.0f / sqrtf_ ((float) n));
        c.norm (tmp); c.add (tmp, 1.0f); c.click (h, 0.3f); break; }
    case 9: {   // BIDON — steel jerrycan: air modes + plate modes
        c.zero (tmp);
        const float X = 0.30f, Y = 0.20f, Z = 0.35f;
        for (int a = 0; a <= 3; ++a) for (int b = 0; b <= 3; ++b) for (int d = 0; d <= 3; ++d)
        {
            if (a + b + d == 0) continue;
            const float f = 171.5f * sqrtf_ ((a / X) * (a / X) + (b / Y) * (b / Y) + (d / Z) * (d / Z));
            c.mode (tmp, f, 0.25f, 1.0f / (float) (a + b + d));
        }
        c.norm (tmp); c.add (tmp, 0.7f);
        c.zero (tmp);
        for (int m = 1; m <= 6; ++m) for (int n = 1; n <= 5; ++n)
        {
            const float f = 40.0f * ((m / X) * (m / X) * 0.09f + (n / Z) * (n / Z) * 0.12f) + 140.0f;
            // (the draws in their own statements, in order: a function's arguments are evaluated in an order C++ leaves
            // to the compiler — clang left to right, gcc and MSVC right to left —, and the plugin's IR has to be the page's)
            const float u0 = rng.uni(), u1 = rng.uni();
            c.mode (tmp, f * (0.97f + 0.06f * u0), 0.9f / (1.0f + 0.15f * (m + n)), 0.3f + 0.7f * u1);
        }
        c.norm (tmp); c.add (tmp, 1.0f); c.click (h, 0.5f); break; }
    case 10: {  // AMP 8 — open-back practice amp
        const float ch[][4] = { { HP, 120, 1.8f, 0 }, { PK, 600, 1.0f, -3 }, { PK, 1800, 2.0f, 6 }, { LP, 4000, 0.8f, 0 }, { LP, 4000, 0.6f, 0 } };
        c.chain (h, ch, 5, 0.15f); c.comb (h, 1.2f, -0.35f); break; }
    case 11: {  // CARTON — cardboard box 40 x 30 x 30 cm
        c.zero (tmp);
        const float X = 0.40f, Y = 0.30f, Z = 0.30f;
        for (int a = 0; a <= 4; ++a) for (int b = 0; b <= 4; ++b) for (int d = 0; d <= 4; ++d)
        {
            if (a + b + d == 0) continue;
            const float f = 171.5f * sqrtf_ ((a / X) * (a / X) + (b / Y) * (b / Y) + (d / Z) * (d / Z));
            c.mode (tmp, f, 0.05f * 600.0f / (600.0f + f * 0.3f), 1.0f / (float) (a + b + d));
        }
        c.norm (tmp); c.add (tmp, 1.0f);
        c.zero (tmp); c.mode (tmp, 140, 0.04f, 1); c.norm (tmp); c.add (tmp, 0.5f); c.click (h, 0.5f); break; }
    case 12: {  // AMP 12 — 1x12 combo
        const float ch[][4] = { { HP, 90, 1.5f, 0 }, { PK, 120, 1.0f, 3 }, { PK, 500, 1.0f, -4 }, { PK, 2600, 1.8f, 7 }, { LP, 5000, 0.8f, 0 }, { LP, 5000, 0.6f, 0 } };
        c.chain (h, ch, 6, 0.15f); c.comb (h, 1.6f, -0.28f); break; }
    case 13: {  // BASS 15 — bass cabinet
        const float ch[][4] = { { HP, 45, 1.3f, 0 }, { PK, 75, 1.0f, 4 }, { PK, 300, 1.0f, -3 }, { PK, 1200, 1.5f, 3 }, { LP, 2500, 0.8f, 0 }, { LP, 2500, 0.6f, 0 } };
        c.chain (h, ch, 6, 0.1f); break; }
    case 14: {  // 4×12 — closed guitar cabinet
        const float ch[][4] = { { HP, 85, 2.0f, 0 }, { PK, 105, 1.2f, 4 }, { PK, 450, 1.0f, -5 }, { PK, 2200, 2.0f, 6 }, { PK, 3600, 3.0f, 4 }, { LP, 5600, 0.8f, 0 }, { LP, 5600, 0.6f, 0 } };
        c.chain (h, ch, 7, 0.15f); c.norm (h);
        c.zero (tmp); c.mode (tmp, 229, 0.03f, 1); c.mode (tmp, 490, 0.025f, 0.7f); c.norm (tmp); c.add (tmp, 0.15f); break; }
    case 15: {  // PIANO — open piano: sympathetic strings + soundboard
        c.zero (tmp);
        const float notes[] = { 65.41f, 82.41f, 98.0f, 130.81f, 196.0f, 261.63f, 329.63f };
        for (int s = 0; s < 7; ++s)
            for (int k = 1; k * notes[s] < 5000.0f && k <= 40; ++k)
            {
                const float f = k * notes[s] * sqrtf_ (1.0f + 0.0002f * k * k);
                c.mode (tmp, f, 1.0f / (1.0f + k * 0.08f), 1.0f / (float) k);
            }
        c.norm (tmp); c.add (tmp, 1.0f);
        c.zero (tmp);
        for (int k = 0; k < 40; ++k)
        {
            const float u0 = rng.uni(), u1 = rng.uni(), u2 = rng.uni();     // (in order: see BIDON)
            c.mode (tmp, 80.0f * powf_ (30.0f, u0), 0.05f + 0.1f * u1, 0.3f + 0.7f * u2);
        }
        c.norm (tmp); c.add (tmp, 0.6f); c.click (h, 0.4f); break; }
    case 16: c.room (1.0f, 0.6f, 2.0f, 0.70f, 0.5f);  break;  // PLACARD
    case 17: c.room (2.0f, 1.4f, 1.1f, 0.65f, 0.5f);  break;  // AUTO
    case 18: c.room (2.2f, 2.0f, 2.2f, 0.55f, 0.4f);  break;  // CABINE
    case 19: c.room (2.5f, 2.0f, 2.4f, 0.93f, 0.15f); break;  // BAIN
    case 20: c.room (5.0f, 4.0f, 2.6f, 0.80f, 0.35f); break;  // SALON
    case 21: c.room (6.0f, 6.0f, 2.7f, 0.92f, 0.2f);  break;  // GARAGE
    case 22: c.room (3.0f, 3.0f, 12.0f, 0.90f, 0.25f); break; // ESCALIER
    case 23: c.room (4.0f, 4.0f, 60.0f, 0.90f, 0.2f); break;  // TUNNEL
    default: c.room (30.0f, 15.0f, 18.0f, 0.94f, 0.25f); break; // ÉGLISE
    }
    if (id < 16) c.fadeEnd (0.2f);
    c.norm (h);
    for (int i = 0; i < L; ++i) if (isBad (h[i])) h[i] = 0.0f;
    return L;
}

// partition an IR (forward and reversed) into stage A / B spectra stored in the pool
void IrBank::store (int id, int L, RFFT<2 * BOX_NA>& fa, RFFT<2 * BOX_NB>& fb)
{
    {   // peak magnitude response (first 16384 samples are enough for the resonant peaks)
        for (int i = 0; i < 16384; ++i) tmp[i] = i < L ? h[i] : 0.0f;
        fpk.forward (tmp, sre, sim);
        float pk = 0.0f;
        for (int k = 0; k <= 8192; ++k) pk = maxf (pk, sre[k] * sre[k] + sim[k] * sim[k]);
        e[id].peak = sqrtf_ (pk);
    }
    for (int rev = 0; rev < 2; ++rev)
    {
        if (rev) for (int i = 0; i < L / 2; ++i) { const float t = h[i]; h[i] = h[L - 1 - i]; h[L - 1 - i] = t; }
        const int la = mini (L, BOX_NA * BOX_PA);
        int pa = (la + BOX_NA - 1) / BOX_NA;
        const int lb = L > BOX_NA * BOX_PA ? L - BOX_NA * BOX_PA + BOX_NA : 0;
        int pb = mini ((lb + BOX_NB - 1) / BOX_NB, BOX_PBMAX);
        const int needA = pa * 2 * (BOX_NA + 1);
        while (pb > 0 && used + needA + pb * 2 * (BOX_NB + 1) > IR_POOL) --pb;
        if (used + needA > IR_POOL) { pa = 0; pb = 0; }
        const int offA = used;
        for (int p = 0; p < pa; ++p)
        {
            for (int i = 0; i < 2 * BOX_NA; ++i) { const int s = p * BOX_NA + i; pad[i] = (i < BOX_NA && s < la) ? h[s] : 0.0f; }
            fa.forward (pad, pool + used, pool + used + BOX_NA + 1);
            used += 2 * (BOX_NA + 1);
        }
        const int offB = used;
        for (int p = 0; p < pb; ++p)
        {
            for (int i = 0; i < 2 * BOX_NB; ++i)
            {
                const int j = p * BOX_NB + i;               // index into hB = [64 zeros, h[512..]]
                const int s = j + BOX_NA * BOX_PA - BOX_NA; // h index
                pad[i] = (i < BOX_NB && j >= BOX_NA && s < L) ? h[s] : 0.0f;
            }
            fb.forward (pad, pool + used, pool + used + BOX_NB + 1);
            used += 2 * (BOX_NB + 1);
        }
        IrEntry& E = e[id];
        E.len = L; E.pa = pa; E.pb = pb;
        if (rev) { E.offAr = offA; E.offBr = offB; } else { E.offA = offA; E.offB = offB; }
    }
}

void IrBank::build (float sampleRate, RFFT<2 * BOX_NA>& fa, RFFT<2 * BOX_NB>& fb)
{
    sr = sampleRate;
    used = 0;
    rng.seed (0xB0C5u);
    fpk.init (16384);
    for (int id = 1; id <= NUM_IRS; ++id) store (id, generate (id), fa, fb);
    e[0].len = 0; e[0].pa = e[0].pb = 0; e[0].offA = e[0].offB = e[0].offAr = e[0].offBr = 0; e[0].peak = 1.0f;
}

} // namespace sk
