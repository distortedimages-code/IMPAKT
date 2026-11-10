    // --- micro-looper: loops shrinking/growing windows of a rendered burst (Autechre-style stutter)
    case M_ULOOP:
    {
        const int Ls = maxi (v.lsrcLen, 64);
        const float shrink = 0.5f + P[P_C];
        const float pr = semis ((P[P_D] - 0.5f) * 24.0f);
        const float jit = P[P_F];
        const float dc = decayCoef (0.02f * powf_ (200.0f, P[P_DEC]), sr);
        for (int i = 0; i < n; ++i)
        {
            const float pos = v.ldir > 0 ? v.lphase : v.llen - v.lphase;
            float rp = v.lstart + pos;
            while (rp >= (float) Ls) rp -= (float) Ls;
            if (rp < 0.0f) rp = 0.0f;
            const int i0 = (int) rp; const float fr = rp - (float) i0;
            const int i1 = i0 + 1 < Ls ? i0 + 1 : 0;
            const float s = v.lbuf[i0] + (v.lbuf[i1] - v.lbuf[i0]) * fr;
            const float e = clampf (minf (v.lphase, v.llen - v.lphase) * 0.125f, 0.0f, 1.0f);
            out[i] = s * e * 1.4f * ampTick (v, att, dc);
            v.lphase += v.lrate;
            if (v.lphase >= v.llen)
            {
                v.lphase -= v.llen; if (v.lphase >= v.llen) v.lphase = 0.0f;
                v.llen = clampf (v.llen * shrink, 8.0f, (float) Ls * 0.5f);
                v.lrate = clampf (v.lrate * pr, 0.0625f, 8.0f);
                if (jit > 0.001f)
                {
                    v.lstart += v.rng.bi() * jit * v.llen * 2.0f;
                    while (v.lstart < 0.0f) v.lstart += (float) Ls;
                    while (v.lstart >= (float) Ls) v.lstart -= (float) Ls;
                    v.ldir = v.rng.uni() < jit * 0.5f ? -1 : 1;
                }
            }
            v.ns++;
        }
        v.level = v.aenv;
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
            out[i] = y * 0.75f * ampTick (v, att, dc);
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
    // --- extended Karplus-Strong: pick position, dispersion, pitch bend, saturation in the loop
    case M_CORDE:
    {
        const float f0 = 110.0f * semis (tuneSt);
        const float bendSt = (P[P_E] - 0.5f) * 24.0f;
        const float pc = tcCoef (0.06f, sr);
        const float lc = 0.04f + 0.96f * P[P_B] * P[P_B];
        const float ap = -P[P_C] * 0.85f;
        const float posf = 0.05f + P[P_D] * 0.45f;
        const float grit = P[P_F];
        const float decSec = 0.05f * powf_ (80.0f, P[P_DEC]);
        const float lgK = -3.0f * 3.3219281f / (decSec * sr);
        const float dc = decayCoef (decSec * 2.0f, sr);
        const bool impulse = P[P_A] < 0.05f;
        const float lpDelay = (1.0f - lc) / lc;                       // one-pole low-pass phase delay at DC
        const float apDelay = 2.0f * (1.0f - ap) / (1.0f + ap);       // two first-order allpasses
        for (int i = 0; i < n; ++i)
        {
            const float f = f0 * semis (bendSt * v.penv); v.penv *= pc;
            const float L = clampf (sr / f - lpDelay - apDelay, 2.0f, (float) (COMB_SIZE - 4));
            float e = 0.0f;
            if ((float) v.ns < v.exLen)
            {
                e = impulse ? (v.ns == 0 ? 1.0f : 0.0f) : v.rng.bi() * (1.0f - (float) v.ns / v.exLen);
                if (v.ns < (u32) LBUF_SIZE) v.lbuf[v.ns] = e;
                const int D = (int) (posf * L);
                if (D > 0 && v.ns >= (u32) D && v.ns < (u32) LBUF_SIZE) e -= v.lbuf[v.ns - (u32) D];
            }
            const float rp = (float) v.mcw - L;
            const float fl = floorf_ (rp); const float fr = rp - fl;
            const int i0 = ((int) fl) & COMB_MASK, i1 = (i0 + 1) & COMB_MASK;
            float y = v.mcomb[i0] + (v.mcomb[i1] - v.mcomb[i0]) * fr;
            v.lp1 += lc * (y - v.lp1); y = v.lp1;
            float o = ap * y + v.y1[0]; v.y1[0] = y - ap * o; y = o;
            o = ap * y + v.y1[1]; v.y1[1] = y - ap * o; y = o;
            y *= exp2f_ (lgK * L);
            if (grit > 0.001f) y = softclip (y * (1.0f + grit * 3.0f)) / (1.0f + grit * 0.6f);
            const float w = e + y;
            v.mcomb[v.mcw] = isBad (w) ? 0.0f : w; v.mcw = (v.mcw + 1) & COMB_MASK;
            const float a = ampTick (v, att, dc);
            out[i] = w * 0.9f * a;
            v.lastOut = maxf (absf (w), v.lastOut * 0.9995f);
            v.ns++;
        }
        v.level = minf (v.lastOut, v.aenv);
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
            out[i] = o * 0.8f * ampTick (v, att, dc);
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
            const float o = (b1 * v.s1.k + b2 * v.s2.k * 0.7f + b3 * v.s3.k * 0.45f) * comp;
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
            out[i] = o * 0.9f * ampTick (v, att, dc);
            v.ns++;
        }
        v.level = v.aenv;
        break;
    }
