void Engine::renderChunk (int n, float* L, float* R)
{
    for (int i = 0; i < n; ++i) mL[i] = mR[i] = sDL[i] = sDR[i] = sRv[i] = sSp[i] = 0.0f;
    const float steps = (float) ((double) bpm / 60.0 * 4.0 / (double) sr * n);

    for (int ti = 0; ti < NUM_TRACKS; ++ti)
    {
        Track& t = tr[ti];
        // LFOs use the lock snapshot of the current voice
        const float* lk = t.voice[t.cur].lk;
        auto P = [&] (int p) { return lk[p] >= 0.0f ? lk[p] : t.p[p]; };
        lfoAdvance (t.lfo[0], t.rng, P (P_LSPD), P (P_LWAV), steps);
        lfoAdvance (t.lfo[1], t.rng, P (P_L2SP), P (P_L2WV), steps);

        for (int i = 0; i < n; ++i) t.bus[i] = 0.0f;
        for (int k = 0; k < 2; ++k)
            if (t.voice[k].active) renderVoice (t, t.voice[k], n);
        trackPost (t, ti, n);
    }

    delayProcess (n);
    reverbProcess (n, sRv, mL, mR);
    SpecParams sp { g[G_STIM], g[G_SBLR], g[G_SWRP], g[G_SREF], g[G_SATM], g[G_SFRZ], g[G_SREV], g[G_SLVL] };
    if (sp.level > 0.0005f || ! spec.idle) spec.process (sSp, mL, mR, n, sp);

    float drv = g[G_MDRV];
    float dg = 1.0f + drv * drv * 12.0f;
    float vol = g[G_MVOL] * g[G_MVOL] * 1.5f;
    for (int i = 0; i < n; ++i)
    {
        float l = mL[i] * 0.7f, r = mR[i] * 0.7f;
        if (drv > 0.001f) { l = softclip (l * dg) / (1.0f + drv); r = softclip (r * dg) / (1.0f + drv); }
        l = softclip (l * vol); r = softclip (r * vol);
        L[i] = l; R[i] = r;
    }
}

