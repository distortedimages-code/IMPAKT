// v74: the plugin test's projects, made from the page: the demo, a CHIMERA project, a CUBE one, (v76) a WEST one and (v78) a WEFT one made from it, (v81) a HOWL one, each as
// the page saves it (JSON) and as the ops it sends (for the native harness to render the reference).
//   node tools/plugin_fixtures.js <dir>
//   then, from the repository (CHIMERA reads bank/): build/render_test ops <dir>/demo.ops <dir>/demo.wav 4 (and chim, cube, west, weft, howl)
//   and: impakt_test <dir>/demo.json <dir>/demo.wav <dir>/chim.json <dir>/chim.wav (or cube, west, weft, howl)
const { chromium } = require('/opt/npm-tools/node_modules/playwright');
const fs = require('fs'), path = require('path');
const out = path.resolve(process.argv[2] || '.');
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  await page.goto('file://' + path.resolve(__dirname, '../dist/local/index.html'));
  const r = await page.evaluate(() => {
    const I = window.__impakt, P = I.P, mv = m => (m + 0.5) / 64, SQ = q => q < 35 ? P.S1 + q : P.S36 + q - 35;
    const demo = I.demoState();
    const chim = I.demoState();
    // CHIMERA on T6..T8: another sample, a graft (a snare's body under its attack, the seam later), one reversed
    [[5, { [P.B]: 0.6 }], [6, { [P.S1]: 2.5 / 11, [P.S3]: 0.5 }], [7, { [P.E]: 0.75 }]].forEach(([t, ps]) => {
      const tr = chim.tracks[t]; tr.p[P.MACH] = mv(38); const d = I.MACH[38].d; tr.p[P.TUNE] = d[0]; tr.p[P.DEC] = d[1]; for (let i = 0; i < 6; i++) tr.p[P.A + i] = d[2 + i];
      I.MACH[38].sd.forEach((v, i) => { tr.p[P.S1 + i] = v; }); for (const k in ps) tr.p[k] = ps[k]; tr.mute = 0;
    });
    const cube = I.demoState();
    // CUBE on T7 and T8: a sync sweep with FM, a pad on the VOWEL bank with unison, a p-lock moving Z
    [[6, { [P.A]: 2 / 7, [P.D]: 0.4, [P.S8]: 0.85, [P.S9]: 0.35, [P.DEC]: 0.4 }], [7, { [P.C]: 2 / 7, [P.A]: 3 / 7, [P.S6]: 0.6, [P.S5]: 0.4, [P.S1]: 36.5 / 49, [P.DEC]: 0.8, [P.S13]: 0.7 }]].forEach(([t, ps]) => {
      const tr = cube.tracks[t]; tr.p[P.MACH] = mv(40); const d = I.MACH[40].d; tr.p[P.TUNE] = d[0]; tr.p[P.DEC] = d[1]; for (let i = 0; i < 6; i++) tr.p[P.A + i] = d[2 + i];
      I.MACH[40].sd.forEach((v, i) => { tr.p[P.S1 + i] = v; }); for (const k in ps) tr.p[k] = ps[k]; tr.mute = 0;
      for (let s = 0; s < 16; s += 4) tr.steps[s] = { on: 1, l: s === 8 ? { [P.C]: 6 / 7 } : {} };
    });
    const west = I.demoState();
    // WEST on T7 and T8 (v76): its defaults, the pulser at 1/16 moving a sequence on the pitch; a bell on 5.40 with its
    // spring, a p-lock moving TIMB
    [[6, { [P.E]: 5.5 / 11, [P.S23]: 19 / 24, [P.S24]: 0.25, [P.S27]: 0.5, [P.DEC]: 0.25 }], [7, { [P.D]: 13.5 / 16, [P.C]: 0.45, [P.S34]: 0.4, [P.DEC]: 0.6 }]].forEach(([t, ps]) => {
      const tr = west.tracks[t]; tr.p[P.MACH] = mv(41); const d = I.MACH[41].d; tr.p[P.TUNE] = d[0]; tr.p[P.DEC] = d[1]; for (let i = 0; i < 6; i++) tr.p[P.A + i] = d[2 + i];
      I.MACH[41].sd.forEach((v, i) => { tr.p[SQ(i)] = v; }); for (const k in ps) tr.p[k] = ps[k]; tr.mute = 0; tr.p[P.VOL] = 0.4;
      for (let s = 0; s < 16; s += 8) tr.steps[s] = { on: 1, l: s === 8 ? { [P.A]: 0.7 } : {} };
    });
    // (v78: WEST's gate 2 on its modulator too, heard; the second's pulser SELF, its envelope LOG)
    west.tracks[6].p[SQ(13)] = 0.5; west.tracks[7].p[SQ(47)] = 0.5 / 3; west.tracks[7].p[SQ(46)] = 0;
    const weft = I.demoState();
    // WEFT (v78) on T8, on T7's sound (NBR): Warps' algorithms at XOR's door, the internal carrier; a p-lock to the
    // vocoder; ECRV LIN on T1
    { const tr = weft.tracks[7]; tr.p[P.MACH] = mv(42); const d = I.MACH[42].d; tr.p[P.TUNE] = d[0]; tr.p[P.DEC] = d[1]; for (let i = 0; i < 6; i++) tr.p[P.A + i] = d[2 + i];
      [P.XA, P.XB, P.XC, P.XD, P.XE, P.XF, P.XG].forEach((q, i) => { tr.p[q] = d[8 + i]; });
      tr.p[P.A] = 0.5 / 10; tr.p[P.B] = 0.45; tr.p[P.E] = 0.8; tr.mute = 0; tr.p[P.VOL] = 0.6;
      for (let s = 0; s < 16; s++) tr.steps[s] = { on: s === 8 ? 1 : 0, l: s === 8 ? { [P.B]: 0.85 } : {} };
      weft.tracks[0].p[P.ECRV] = 0.5; }
    const howl = I.demoState();
    // HOWL (v81) on T7 and T8: a chord struck on 1 and 9 (a p-lock to MIN7, gliding), the feedback over its threshold;
    // a DRONE on T8 playing with no trig, a step moving it a fourth up (and a chord on KEY)
    [[6, { [P.DEC]: 0.6, [P.D]: 0.7, [P.E]: 0.3 }, 1], [7, { [P.A]: 13.5 / 15, [P.DEC]: 1, [P.DRON]: 1, [P.D]: 0.6 }, 0]].forEach(([t, ps, seq]) => {
      const tr = howl.tracks[t]; tr.p[P.MACH] = mv(43); const d = I.MACH[43].d; tr.p[P.TUNE] = d[0]; tr.p[P.DEC] = d[1]; for (let i = 0; i < 6; i++) tr.p[P.A + i] = d[2 + i];
      I.MACH[43].sd.forEach((v, i) => { tr.p[P.S1 + i] = v; }); for (const k in ps) tr.p[k] = ps[k]; tr.mute = 0; tr.p[P.VOL] = 0.3;
      for (let s = 0; s < 16; s++) tr.steps[s] = seq ? { on: s % 8 === 0 ? 1 : 0, l: s === 8 ? { [P.A]: 9.5 / 15 } : {} } : { on: s === 8 ? 1 : 0, l: s === 8 ? { [P.TUNE]: d[0] + 5 / 48 } : {} };
    });
    // (v83: T7's amp further, CAB: TGHT, BITE, crisp, PRES, MIC back; T8 bowed, its highest string, the harmonic an
    // octave up, the hand wavering)
    { const p7 = howl.tracks[6].p; p7[SQ(21)] = 0.4; p7[SQ(22)] = 0.5; p7[SQ(23)] = 0.7; p7[SQ(25)] = 0.8; p7[SQ(27)] = 0.8;
      const p8 = howl.tracks[7].p; p8[SQ(28)] = 0.6; p8[SQ(29)] = 0.3; p8[SQ(30)] = 1.5 / 7; p8[SQ(31)] = 1.5 / 3; p8[SQ(33)] = 0.4; p8[SQ(34)] = 0.5; }
    // STENCIL (v83) on T8, on the sum of the others: SLAP on the tempo, the halo half in, PING; a trig on 5 holds the loop
    // (ACT HOLD), a p-lock to KARP on 13 (plucked: its NOTE a fifth up)
    const stc = I.demoState();
    { const tr = stc.tracks[7]; tr.p[P.MACH] = mv(44); const d = I.MACH[44].d; tr.p[P.TUNE] = d[0]; tr.p[P.DEC] = d[1]; for (let i = 0; i < 6; i++) tr.p[P.A + i] = d[2 + i];
      [P.XA, P.XB, P.XC, P.XD, P.XE, P.XF, P.XG].forEach((q, i) => { tr.p[q] = d[8 + i]; }); I.MACH[44].sd.forEach((v, i) => { tr.p[SQ(i)] = v; });
      tr.p[P.B] = 2.5 / 8; tr.p[P.D] = 0.7; tr.p[P.E] = 0.45; tr.p[P.XB] = 0.5; tr.p[P.XF] = 0.4; tr.p[P.XE] = 1.5 / 4; tr.p[P.DEC] = 0.4; tr.mute = 0; tr.p[P.VOL] = 0.7;
      for (let s = 0; s < 16; s++) tr.steps[s] = { on: s === 4 || s === 12 ? 1 : 0, l: s === 12 ? { [P.B]: 0.5 / 8, [P.XE]: 3.5 / 4, [P.NOTE]: 0.5 + 7 / 48 } : {} }; }
    // MANTRA (v84) on T7 and T8: T7 touched on 1 and 9 (TCH 12; a p-lock on 9 to 1234), pair 12 in FM from 34, its HOLD
    // a little open; T8 a DRONE (no trig), its second group held, the delay singing, pair 56 on the LFO
    const mant = I.demoState();
    [[6, { [P.A]: 8.5 / 15, [P.B]: 0.35, [P.D]: 0.3, [P.E]: 0.3 }, 1], [7, { [P.A]: 13.5 / 15, [P.C]: 0.6, [P.D]: 0.62, [P.DRON]: 1 }, 0]].forEach(([t, ps, seq]) => {
      const tr = mant.tracks[t]; tr.p[P.MACH] = mv(45); const d = I.MACH[45].d; tr.p[P.TUNE] = d[0]; tr.p[P.DEC] = d[1]; for (let i = 0; i < 6; i++) tr.p[P.A + i] = d[2 + i];
      I.MACH[45].sd.forEach((v, i) => { tr.p[SQ(i)] = v; }); for (const k in ps) tr.p[k] = ps[k]; tr.mute = 0; tr.p[P.VOL] = 0.4;
      for (let s = 0; s < 16; s++) tr.steps[s] = seq ? { on: s % 8 === 0 ? 1 : 0, l: s === 8 ? { [P.A]: 12.5 / 15 } : {} } : { on: 0, l: {} };
    });
    { const p7 = mant.tracks[6].p; p7[SQ(15)] = 1.5 / 3; p7[SQ(14)] = 0.5; p7[SQ(44)] = 0.7;
      const p8 = mant.tracks[7].p; p8[SQ(22)] = 2.5 / 3; p8[SQ(21)] = 0.45; p8[SQ(41)] = 0.5; p8[SQ(39)] = 0.75; }
    // (as the page saves them: marked "wst", v76's mark: a project without it was saved before, its machine 42 SPHERE's;
    // v78's "v78")
    const js = x => JSON.stringify(Object.assign({}, x, { wst: 1, v78: 1, v82: 1, v83: 1 }));
    // (v82: the HOWL project as v81 saved it: no "v82", DIST where HUM is now, LOOP's places at rest, SIDE on three pages;
    // its ops as the page plays it once migrated: the plugin's own migration has to give the same)
    const h81 = JSON.parse(js(howl)); delete h81.v82; delete h81.v83;
    [6, 7].forEach((t, i) => { const p = h81.tracks[t].p; p[P.S4] = i ? 0.55 : 0.4; for (let q = 14; q < 35; q++) p[SQ(q)] = 0; p[P.F] = 0.5 / 3; });
    const m81 = I.migrate(JSON.parse(JSON.stringify(h81)));
    // (v83: the HOWL project as v82 saved it: no "v83", CAB's and BOW's places at rest, SIDE on four pages (LOOP shown))
    const h82 = JSON.parse(js(howl)); delete h82.v83;
    [6, 7].forEach(t => { const p = h82.tracks[t].p; for (let q = 21; q < 35; q++) p[SQ(q)] = 0; p[P.F] = 3.5 / 4; });
    const m82 = I.migrate(JSON.parse(JSON.stringify(h82)));
    return { demo: [js(demo), I.stateOps(demo)], chim: [js(chim), I.stateOps(chim)], cube: [js(cube), I.stateOps(cube)], west: [js(west), I.stateOps(west)], weft: [js(weft), I.stateOps(weft)], howl: [js(howl), I.stateOps(howl)], howl81: [JSON.stringify(h81), I.stateOps(m81)], howl82: [JSON.stringify(h82), I.stateOps(m82)], stc: [js(stc), I.stateOps(stc)], mant: [js(mant), I.stateOps(mant)] };
  });
  for (const k in r) { fs.writeFileSync(path.join(out, k + '.json'), r[k][0]); fs.writeFileSync(path.join(out, k + '.ops'), r[k][1].map(o => o.join(' ')).join('\n') + '\n'); console.log(k, r[k][1].length, 'ops'); }
  await browser.close();
})();
