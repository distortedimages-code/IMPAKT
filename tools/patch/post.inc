// ----------------------------------------------------------------- parameters with locks + LFOs
void Engine::effParams (const Track& t, const float* lk, float* P) const
{
    for (int p = 0; p < NUM_PARAMS; ++p) P[p] = lk[p] >= 0.0f ? lk[p] : t.p[p];
    const int d1 = LFO_DEST[idxOf (P[P_LDST], NUM_LFO_DEST)];
    const int d2 = LFO_DEST[idxOf (P[P_L2DS], NUM_LFO_DEST)];
    const float a1 = (P[P_LDEP] - 0.5f) * 2.0f, a2 = (P[P_L2DP] - 0.5f) * 2.0f;
    if (d1 >= 0) P[d1] = clampf (P[d1] + t.lfo[0].val * a1, 0.0f, 1.0f);
    if (d2 >= 0) P[d2] = clampf (P[d2] + t.lfo[1].val * a2, 0.0f, 1.0f);
}

static void lfoAdvance (Lfo& l, Rng& rng, float spdP, float wavP, float steps)
{
    const int wv = idxOf (wavP, NUM_LFO_WAVES), shape = wv % 6;
    const bool trigMode = wv >= 6;
    l.ph += steps / LFO_STEPS[idxOf (spdP, NUM_LFO_SPEEDS)];
    if (trigMode && shape == 5) { if (l.ph > 64.0f) l.ph = 64.0f; }
    else if (l.ph >= 1.0f) { l.ph -= floorf_ (l.ph); l.sh = rng.bi(); }
    const float ph = l.ph;
    switch (shape)
    {
        case 0:  l.val = sin2pi (ph); break;
        case 1:  l.val = 1.0f - 4.0f * absf (ph - 0.5f); break;
        case 2:  l.val = 2.0f * ph - 1.0f; break;
        case 3:  l.val = ph < 0.5f ? 1.0f : -1.0f; break;
        case 4:  l.val = l.sh; break;
        default: l.val = exp2f_ (-ph * 6.0f); break;
    }
}

// ----------------------------------------------------------------- voice chain (into the track bus)
void Engine::renderVoice (Track& t, Voice& v, int n)
{
    float P[NUM_PARAMS];
    effParams (t, v.lk, P);

    float* x = vb;
    machineRender (v, P, n, x);

    int ft = idxOf (P[P_FTYP], NUM_FTYPES);
    float fenvAmt = (P[P_FENV] - 0.5f) * 2.0f;
    float fnorm = clampf (P[P_FREQ] + fenvAmt * v.fenv, 0.0f, 1.0f);
    float fc = 20.0f * exp2f_ (fnorm * 9.9658f);
    float q = 0.5f + P[P_RES] * P[P_RES] * 24.0f;
    if (ft != FT_COMB) v.flt.set (fc, q, sr);
    float fcoef = decayCoef (0.005f * powf_ (400.0f, P[P_FDEC]), sr);
    v.fenv *= powf_ (fcoef, (float) n);
    float combDel = clampf (sr / fc, 2.0f, (float) (COMB_SIZE - 4));
    int cdi = (int) combDel; float cdf = combDel - (float) cdi;
    float combFb = P[P_RES] * 0.97f;

    float srr = P[P_SRR]; bool srrOn = srr > 0.002f; float srrDiv = 1.0f + srr * srr * 63.0f;
    float bits = P[P_BITS]; bool bitsOn = bits > 0.002f; float levels = exp2f_ (12.0f - bits * 11.0f);
    float drv = P[P_DRV]; float dg = 1.0f + drv * drv * 24.0f;
    float dmix = minf (1.0f, drv * 20.0f), dcomp = 1.0f / (1.0f + drv * 1.5f);
    float amp = v.vel * P[P_VOL];
    float R = 1.0f - TWO_PI * 10.0f / sr;
    float chokeDec = 1.0f / (0.003f * sr);
    float atkInc = P[P_ATK] > 0.002f ? 1.0f / (0.001f * powf_ (1500.0f, P[P_ATK]) * sr) : 1.0f;
    float* bus = t.bus;

    for (int i = 0; i < n; ++i)
    {
        float s = x[i];
        if (srrOn)
        {
            v.srrCnt += 1.0f;
            if (v.srrCnt >= srrDiv) { v.srrCnt -= srrDiv; if (v.srrCnt > srrDiv) v.srrCnt = 0.0f; v.srrHold = s; }
            s = v.srrHold;
        }
        if (bitsOn) s = floorf_ (s * levels + 0.5f) / levels;
        float lp, bp, hp;
        switch (ft)
        {
            case FT_LP: v.flt.tick (s, lp, bp, hp); s = lp; break;
            case FT_BP: v.flt.tick (s, lp, bp, hp); s = bp * v.flt.k; break;
            case FT_HP: v.flt.tick (s, lp, bp, hp); s = hp; break;
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
        if (dmix > 0.0f) s = s + (tanhf_ (s * dg) * dcomp - s) * dmix;
        float y = s - v.dcx + R * v.dcy; v.dcx = s; v.dcy = y; s = y;
        if (v.choking) { v.choke -= chokeDec; if (v.choke < 0.0f) v.choke = 0.0f; }
        if (v.atk < 1.0f) { v.atk += atkInc; if (v.atk > 1.0f) v.atk = 1.0f; }
        bus[i] += s * amp * v.choke * v.atk;
    }

    if (isBad (v.dcy) || isBad (v.flt.ic1) || isBad (v.flt.ic2)) { v.active = 0; return; }
    if (v.choking && v.choke <= 0.0f) { v.active = 0; return; }
    if (v.level < 1e-4f)
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
    }
}

// track bus -> BOX insert -> ducking -> pan / sends
void Engine::trackPost (Track& t, int ti, int n)
{
    float P[NUM_PARAMS];
    effParams (t, t.voice[t.cur].lk, P);
    TrackBox& b = t.box;
    float* x = t.bus;
    float* out = vb;

    // the dry path always carries the wet-path latency, so switching the BOX never shifts the groove
    for (int i = 0; i < n; ++i) { out[i] = b.dry[b.dryw]; b.dry[b.dryw] = x[i]; b.dryw = (b.dryw + 1) % BOX_NA; }

    const int ir = idxOf (P[P_IR], NUM_IRS + 1);
    if (ir > 0 && bank.e[ir].pa > 0)
    {
        float inPeak = 0.0f;
        for (int i = 0; i < n; ++i) inPeak = maxf (inPeak, absf (x[i]));
        const float fb = P[P_IFB] * 0.97f;
        const int preS = mini ((int) (P[P_IPRE] * P[P_IPRE] * 0.25f * sr), PRE_MAX - 1);
        const IrEntry& E = bank.e[ir];
        if (inPeak > 1e-6f || fb > 0.001f) b.quiet = 0; else b.quiet += n;
        const int tailLen = E.len + preS + 2 * BOX_NB + BOX_NA;
        if (! b.active || (b.idle && b.quiet == 0))
        {
            b.a.reset (0); b.b.reset (b.phase);
            for (int i = 0; i < PRE_MAX; ++i) b.pre[i] = 0.0f;
            b.wetPrev = b.toneLp = b.toneHp = 0.0f;
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
            const float mix = P[P_IMIX];
            for (int i = 0; i < n; ++i)
            {
                float w = wa[i] + wb[i];
                b.toneLp += cl * (w - b.toneLp); w = b.toneLp;
                b.toneHp += ch * (w - b.toneHp); w -= b.toneHp;
                if (isBad (w)) w = 0.0f;
                b.pre[b.prew] = x[i];
                const float pd = b.pre[(b.prew - preS + PRE_MAX) % PRE_MAX];
                b.prew = (b.prew + 1) % PRE_MAX;
                cin[i] = pd + fb * softclip (b.wetPrev);
                b.wetPrev = w;
                out[i] = out[i] * (1.0f - mix) + w * mix;
            }
            b.a.push (cin, n, fftA);
            b.b.push (cin, n, fftB);
        }
    }
    else b.active = 0;

    // ducking from track 1
    if (ti == 0)
    {
        const float rel = tcCoef (0.12f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float a = absf (out[i]);
            duckState = a > duckState ? a : duckState * rel;
            duckEnv[i] = duckState;
        }
    }
    else if (P[P_DUCK] > 0.001f)
        for (int i = 0; i < n; ++i) out[i] *= 1.0f - P[P_DUCK] * minf (1.0f, duckEnv[i] * 1.5f);

    const float th = P[P_PAN] * 0.25f;
    const float gl = cos2pi (th) * 1.41421f, gr = sin2pi (th) * 1.41421f;
    const float sd = P[P_DLY], srv = P[P_REV], ssp = P[P_SPC];
    for (int i = 0; i < n; ++i)
    {
        const float s = out[i];
        const float l = s * gl, r = s * gr;
        mL[i] += l; mR[i] += r;
        sDL[i] += l * sd; sDR[i] += r * sd;
        sRv[i] += s * srv;
        sSp[i] += s * ssp;
    }
}

