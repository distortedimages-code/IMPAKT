        case M_ULOOP:
        {
            // render a 250 ms source once, the voice then loops tiny windows of it
            const int Ls = mini ((int) (0.25f * sr), LBUF_SIZE);
            v.lsrcLen = Ls;
            const float f = 220.0f * semis ((P[P_TUNE] - 0.5f) * 48.0f + v.note);
            const float srcp = P[P_A] * 3.0f; const int s0 = mini ((int) srcp, 2); const float sf = srcp - (float) s0;
            const float isr = 1.0f / sr;
            float pc = 0, pm = 0, pz = 0, q1 = 0, q2 = 0, q3 = 0;
            Svf bp; bp.reset();
            float src[4];
            for (int n = 0; n < Ls; ++n)
            {
                const float prog = (float) n / (float) Ls;
                for (int k = 0; k < 4; ++k) src[k] = 0.0f;
                if (s0 == 0) { pc += f * isr; pm += f * 2.71f * isr; pc -= floorf_ (pc); pm -= floorf_ (pm);
                    const float ix = 6.0f * (1.0f - prog) * (1.0f - prog) + 0.3f;
                    src[0] = sin2pi (pc + ix * sin2pi (pm) * 0.159f) * (1.0f - 0.5f * prog); }
                if (s0 == 0 || s0 == 1) { if ((n & 31) == 0) bp.set (f * (1.0f + 4.0f * (1.0f - prog)), 6.0f, sr);
                    float lp, b, hp; bp.tick (v.rng.bi(), lp, b, hp); src[1] = b * bp.k * 2.5f; }
                if (s0 == 1 || s0 == 2) { pz += f * 8.0f * exp2f_ (-prog * 4.0f) * isr; pz -= floorf_ (pz); src[2] = sin2pi (pz); }
                if (s0 == 2) { q1 += f * isr; q2 += f * 1.26f * isr; q3 += f * 1.5f * isr; q1 -= floorf_ (q1); q2 -= floorf_ (q2); q3 -= floorf_ (q3);
                    const float c = ((q1 < 0.5f ? 1.0f : -1.0f) + (q2 < 0.5f ? 1.0f : -1.0f) + (q3 < 0.5f ? 1.0f : -1.0f)) * 0.33f;
                    src[3] = floorf_ (c * 7.0f + 0.5f) / 7.0f; }
                v.lbuf[n] = (src[s0] + (src[s0 + 1] - src[s0]) * sf) * 0.8f;
            }
            v.llen = clampf (0.001f * powf_ (150.0f, P[P_B]) * sr, 16.0f, (float) Ls * 0.5f);
            v.lstart = P[P_E] * (float) Ls * 0.8f;
            v.lrate = 1.0f; v.lphase = 0.0f; v.ldir = 1;
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
        case M_CORDE:
        {
            for (int i = 0; i < COMB_SIZE; ++i) v.mcomb[i] = 0.0f;
            v.mcw = 0;
            v.exLen = 1.0f + P[P_A] * 0.02f * sr;
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
