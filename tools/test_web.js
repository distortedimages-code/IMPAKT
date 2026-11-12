// Headless check of the web build: audio engine boots, sequencer runs, p-lock shortcuts work.
const { chromium } = require('/opt/npm-tools/node_modules/playwright');
const path = require('path');
const http = require('http');
const fs = require('fs');

const dir = path.resolve(__dirname, '../dist/local');
const shotDir = process.argv[2] || '/tmp';
const server = http.createServer((req, res) => {
  if (req.url.includes('favicon')) { res.writeHead(204); return res.end(); }
  const f = path.join(dir, decodeURIComponent(req.url.split('?')[0]).replace(/^\/$/, '/index.html'));
  if (!f.startsWith(dir) || !fs.existsSync(f)) { if (!req.url.includes('favicon')) console.error('404', req.url); res.writeHead(404); return res.end(); }
  const type = f.endsWith('.js') ? 'text/javascript' : f.endsWith('.html') ? 'text/html' : 'application/octet-stream';
  res.writeHead(200, { 'Content-Type': type }); fs.createReadStream(f).pipe(res);
}).listen(8765);

// a test loop as a WAV file (44.1 kHz stereo, 16-bit): kick, snare, hats on every 16th, the off 16ths a little late
// v74: a CUBE bank as a WAV file (mono, 16-bit, 44.1 kHz): 64 waves of 256 samples, each a pulse narrowing along the bank
function cubeWav() {
  const n = 64 * 256, b = Buffer.alloc(44 + n * 2);
  b.write('RIFF', 0); b.writeUInt32LE(36 + n * 2, 4); b.write('WAVE', 8); b.write('fmt ', 12); b.writeUInt32LE(16, 16); b.writeUInt16LE(1, 20); b.writeUInt16LE(1, 22);
  b.writeUInt32LE(44100, 24); b.writeUInt32LE(88200, 28); b.writeUInt16LE(2, 32); b.writeUInt16LE(16, 34); b.write('data', 36); b.writeUInt32LE(n * 2, 40);
  for (let w = 0; w < 64; w++) for (let i = 0; i < 256; i++) b.writeInt16LE(i / 256 < 0.5 - 0.45 * w / 63 ? 20000 : -20000, 44 + (w * 256 + i) * 2);
  return b;
}
function testWav(bpm, bars, variant) {
  const R = 44100, n = Math.round(bars * 240 / bpm * R), x = new Float64Array(n), pats = ['K..H.S..K.K.S..H', 'K..K.S.HK.K..S.S', 'K.H.S..KH.K.S.H.'];
  const pat = pats[variant % 3]; let seed = 99 + variant; const rnd = () => { seed = (seed * 1103515245 + 12345) >>> 0; return seed / 2147483648 - 1; };
  for (let k = 0; k < 16 * bars; k++) {
    const c = pat[k % 16], t0 = Math.round(k * n / (16 * bars) + (k % 2 ? 0.006 * R : 0)); let ph = 0, last = 0;
    for (let i = 0; i < Math.min(n - t0, 0.4 * R); i++) {
      const t = i / R, nz = rnd(), hp = nz - last; last = nz;
      let y = (c === 'H' ? 0.22 : 0.12) * hp * Math.exp(-t / 0.03);
      if (c === 'K') { ph += 2 * Math.PI * (48 + 130 * Math.exp(-t / 0.03)) / R; y += 0.9 * Math.sin(ph) * Math.exp(-t / 0.22); }
      if (c === 'S') y += 0.45 * nz * Math.exp(-t / 0.11) + 0.35 * Math.sin(2 * Math.PI * 190 * t) * Math.exp(-t / 0.07);
      x[t0 + i] += 0.5 * y * Math.min(1, i / 8);
    }
  }
  const b = Buffer.alloc(44 + n * 4);
  b.write('RIFF', 0); b.writeUInt32LE(36 + n * 4, 4); b.write('WAVE', 8); b.write('fmt ', 12); b.writeUInt32LE(16, 16); b.writeUInt16LE(1, 20); b.writeUInt16LE(2, 22);
  b.writeUInt32LE(R, 24); b.writeUInt32LE(R * 4, 28); b.writeUInt16LE(4, 32); b.writeUInt16LE(16, 34); b.write('data', 36); b.writeUInt32LE(n * 4, 40);
  for (let i = 0; i < n; i++) { const v = Math.max(-32767, Math.min(32767, Math.round(x[i] * 32767))); b.writeInt16LE(v, 44 + 4 * i); b.writeInt16LE(v, 46 + 4 * i); }
  return b;
}

(async () => {
  const browser = await chromium.launch({
    // (the machine's Chromium: the newest under /opt/pw-browsers, or PW_CHROME)
    executablePath: process.env.PW_CHROME || (() => { const d = '/opt/pw-browsers', c = fs.existsSync(d) ? fs.readdirSync(d).filter(n => /^chromium-\d+$/.test(n)).sort((a, b) => +b.split('-')[1] - +a.split('-')[1]) : [];
      for (const n of c) for (const sub of ['chrome-linux64/chrome', 'chrome-linux/chrome']) if (fs.existsSync(path.join(d, n, sub))) return path.join(d, n, sub); return undefined; })(),
    args: ['--autoplay-policy=no-user-gesture-required', '--use-fake-ui-for-media-stream'],
  });
  const results = []; global.__results = results;
  const ok = (name, cond, extra) => { results.push((cond ? 'PASS ' : 'FAIL ') + name + (extra ? '  ' + extra : '')); };

  for (const [w, h, scheme, label] of [[390, 844, 'light', 'phone-light'], [390, 844, 'dark', 'phone-dark'], [1000, 680, 'light', 'wide-light'], [1000, 680, 'dark', 'wide-dark']]) {
    const page = await browser.newPage({ viewport: { width: w, height: h }, colorScheme: scheme, hasTouch: w < 500 });
    const errors = [];
    page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
    page.on('pageerror', e => errors.push(String(e)));
    page.on('response', r => { if (r.status() >= 400) errors.push('HTTP ' + r.status() + ' ' + r.url()); });
    await page.goto('http://localhost:8765/index.html');
    await page.waitForTimeout(400);
    // v67: five pages; a module shown with its page (TONE, MOD, FX, MASTR), the sequencer's page
    const openMod = async id => { await page.evaluate(id => window.__impakt.openModule(id), id); await page.waitForTimeout(80); };
    const seqPage = async () => { await page.evaluate(() => window.__impakt.setPTab('ms')); await page.waitForTimeout(60); };
    if (label === 'phone-light') {
      // phone: five pages: SEQ (machine, pads, steps, sequencer controls on the black plate), TONE, MOD, FX (the track's
      // screen over their modules), MASTR
      const vis = sel => page.evaluate(sel => { const e = document.querySelector(sel); return !!e && getComputedStyle(e).display !== 'none' && e.getBoundingClientRect().height > 0; }, sel);
      const ph = { keys: await page.evaluate(() => [...document.querySelectorAll('#ptabs .ptab')].map(b => b.textContent).join()),
        ms: [await vis('#machmod'), await vis('#pads'), await vis('#steps'), await vis('#lenv'), await vis('#trkmod'), await vis('#mstmod'), await vis('#km-FLT')] };
      const s16 = await page.locator('.step').nth(15).boundingBox();
      ph.stepsFirstScreen = !!s16 && s16.y + s16.height <= 844;
      ph.plate = await page.evaluate(() => getComputedStyle(document.getElementById('machmod')).backgroundColor);
      await page.screenshot({ path: `${shotDir}/impakt-${label}-page-ms.png` });
      await page.click('#ptabs [data-tab="tn"]');
      ph.tn = [await vis('#machmod'), await vis('#steps'), await vis('#trkmod'), await vis('#km-FLT'), await vis('#km-TONE'), await vis('#km-AMP'), await vis('#mstmod')];
      await page.screenshot({ path: `${shotDir}/impakt-${label}-page-tn.png`, fullPage: true });
      // the TRACK screen: arrows go from track to track
      await page.locator('#tstrip [data-d="1"]').click();
      const tn1 = await page.evaluate(() => ({ t: window.__impakt.ui.t, lab: document.querySelector('#tstrip .mlab b').textContent, on: [...document.querySelectorAll('#tstrip .tdots i')].findIndex(i => i.classList.contains('on')) }));
      await page.locator('#tstrip [data-d="-1"]').click(); await page.locator('#tstrip [data-d="-1"]').click();
      const tn2 = await page.evaluate(() => ({ t: window.__impakt.ui.t, lab: document.querySelector('#tstrip .mlab b').textContent }));
      await page.locator('#tstrip [data-d="1"]').click();
      ok('TRACK screen arrows step through the tracks', tn1.t === 1 && tn1.lab === 'T2' && tn1.on === 1 && tn2.t === 7 && tn2.lab === 'T8', JSON.stringify({ tn1, tn2 }));
      await page.click('#ptabs [data-tab="md"]');
      ph.md = [await vis('#trkmod'), await vis('#km-LFO1'), await vis('#km-LFO2'), await vis('#km-EF1'), await vis('#km-EF2'), await vis('#km-FLT')];
      await page.screenshot({ path: `${shotDir}/impakt-${label}-page-md.png`, fullPage: true });
      // v69: MOD shows its LFOs and followers one at a time under their tabs (one row); the track's screen follows the
      // tab (FOLLOWER 1: its envelope) and stays on the first screen with the module under it; a routed LFO lights its
      // tab's pixel, a p-lock in a hidden one puts a dot on its tab
      await page.click('#km-LFO1 .rtab[data-ret="EF1"]'); await page.waitForTimeout(120);
      ph.mdt = await page.evaluate(() => { const I = window.__impakt, r = e => document.querySelector(e).getBoundingClientRect();
        return { vis: ['LFO1', 'LFO2', 'EF1', 'EF2'].map(id => getComputedStyle(document.getElementById('km-' + id)).display !== 'none').join(), view: I.trackView(),
          tabs: [...document.querySelectorAll('#km-EF1 .rtab')].map(b => b.textContent).join(','),
          rows: new Set([...document.querySelectorAll('#km-EF1 .rtab')].map(b => Math.round(b.getBoundingClientRect().top))).size,
          scr: r('#tspec').top >= 0 && r('#tspec').bottom <= innerHeight, mod: r('#km-EF1').bottom <= innerHeight, page: I.PAGES[I.ui.page].id }; });
      await page.screenshot({ path: `${shotDir}/impakt-${label}-page-md-ef1.png` });
      await page.evaluate(() => { const I = window.__impakt, P = I.P, tr = I.state.tracks[I.ui.t]; tr.p[P.LDST] = I.modDestVal(P.FREQ); tr.p[P.LDEP] = 0.8; tr.steps[2].l[P.L2SP] = 0.3; I.renderAll(); });
      ph.mdt.lit = await page.evaluate(() => [...document.querySelectorAll('#km-EF1 .rtab')].map(b => (b.classList.contains('on') ? 'o' : '-') + (b.classList.contains('haslock') ? 'L' : '')).join(','));
      await page.evaluate(() => { const I = window.__impakt, P = I.P, tr = I.state.tracks[I.ui.t]; tr.p[P.LDST] = I.modDestVal(-1); tr.p[P.LDEP] = 0.5; delete tr.steps[2].l[P.L2SP]; I.renderAll(); });
      await page.click('#km-EF1 .rtab[data-ret="LFO2"]'); await page.waitForTimeout(80);
      ph.mdt.lfo2 = await page.evaluate(() => ({ view: window.__impakt.trackView(), page: window.__impakt.PAGES[window.__impakt.ui.page].id }));
      await page.click('#km-LFO2 .rtab[data-ret="LFO1"]'); await page.waitForTimeout(80);
      ok('v69 phone (v90: M1..M4): MOD, one module at a time under its tabs (one row), the modulators\' screen on the first screen with it; a routed modulator lit, a hidden lock dotted',
        ph.mdt.vis === 'false,false,true,false' && ph.mdt.view === 'MODS' && ph.mdt.page === 'EF1' && ph.mdt.tabs === 'M1,M2,M3,M4' && ph.mdt.rows === 1
        && ph.mdt.scr && ph.mdt.mod && ph.mdt.lit === 'o,-L,-,-' && ph.mdt.lfo2.view === 'MODS' && ph.mdt.lfo2.page === 'LFO2', JSON.stringify(ph.mdt));
      // v69: OPTION at the end of the transport, on every page: its menu (Clear track, Clear all, Kits, Init, Demo) on the
      // screen; a tap elsewhere closes it; Kits opens the kits panel and closes the menu; Escape closes the panel
      const op = { bar: await page.evaluate(() => document.getElementById('projbtn').closest('#bar') !== null && document.getElementById('projbtn').textContent) };
      await page.click('#projbtn'); await page.waitForTimeout(60);
      op.menu = await page.evaluate(() => [...document.querySelectorAll('#projrow .btn')].filter(b => { const r = b.getBoundingClientRect(); return r.height > 0 && r.left >= 0 && r.right <= innerWidth && r.bottom <= innerHeight; }).map(b => b.textContent).join(','));
      op.exp = await page.getAttribute('#projbtn', 'aria-expanded');
      await page.mouse.click(30, 700); await page.waitForTimeout(60);
      op.closed = await page.isHidden('#projrow');
      await page.click('#projbtn'); await page.click('#kitsbtn'); await page.waitForTimeout(80);
      op.kits = { panel: await page.isVisible('#kpanel'), menu: await page.isHidden('#projrow'),
        fit: await page.evaluate(() => { const r = document.getElementById('kpanel').getBoundingClientRect(); return r.left >= 0 && r.right <= innerWidth; }) };
      await page.screenshot({ path: `${shotDir}/impakt-${label}-option-kits.png` });
      await page.keyboard.press('Escape'); await page.waitForTimeout(60);
      op.esc = await page.isHidden('#kpanel');
      op.seq = await page.evaluate(() => !document.querySelector('#seqmod #projbtn') && !document.querySelector('#seqmod .proj'));
      ok('v69 phone: OPTION in the bar (its menu on the screen, closed by a tap elsewhere; Kits opens the panel, Escape closes it), nothing left in the sequencer',
        op.bar === 'Option' && op.menu === 'Copy track,Paste track,Move track,Clear track,Clear all,Kits,Init,Demo,Updates' && op.exp === 'true' && op.closed && op.kits.panel && op.kits.menu && op.kits.fit && op.esc && op.seq, JSON.stringify(op));
      await page.click('#ptabs [data-tab="fx"]');
      ph.fx = [await vis('#trkmod'), await vis('#km-BOX'), await vis('#km-SEND'), await vis('#km-REV'), await vis('#km-DLY'), await vis('#mstmod')];
      // v68: the returns one at a time, under their tabs
      await page.click('#km-REV .rtab[data-ret="DLY"]'); await page.waitForTimeout(80);
      ph.fx2 = [await vis('#km-REV'), await vis('#km-DLY'), await vis('#km-CHO'), await vis('#km-PHS')];
      await page.click('#km-DLY .rtab[data-ret="REV"]'); await page.waitForTimeout(80);
      await page.screenshot({ path: `${shotDir}/impakt-${label}-page-fx.png`, fullPage: true });
      await page.click('#ptabs [data-tab="mt"]');
      ph.mt = [await vis('#trkmod'), await vis('#mstmod'), await vis('#km-OUT'), await vis('#machmod')];
      ph.grow = await page.evaluate(() => new Set([...document.querySelectorAll('#gtabs .tab')].map(b => Math.round(b.getBoundingClientRect().top))).size);
      await page.screenshot({ path: `${shotDir}/impakt-${label}-page-mt.png`, fullPage: true });
      await page.click('#ptabs [data-tab="ms"]');
      const kt = await page.evaluate(() => {
        const k = document.querySelectorAll('#mknobs .knob')[1], v0 = window.__impakt.state.tracks[0].p[2];
        const ev = (el, type, y) => el.dispatchEvent(new PointerEvent(type, { bubbles: true, cancelable: true, pointerId: 9, pointerType: 'touch', isPrimary: true, clientX: 10, clientY: y }));
        ev(k.querySelector('.kl'), 'pointerdown', 300); ev(k, 'pointermove', 220); ev(k, 'pointerup', 220);
        const v1 = window.__impakt.state.tracks[0].p[2];
        ev(k.querySelector('svg'), 'pointerdown', 300); ev(k, 'pointermove', 240); ev(k, 'pointerup', 240);
        return { label: v1 === v0, dial: window.__impakt.state.tracks[0].p[2] > v1 };
      });
      ph.touch = kt;
      ok('phone: a touch on a knob label scrolls, only the dial turns', kt.label && kt.dial, JSON.stringify(kt));
      ok('phone: five pages (SEQ, TONE, MOD, FX, MASTR), steps on the first screen, the machine in a black block, the modules on their page',
        ph.keys === 'Seq,Tone,Mod,FX,Mastr' && ph.ms.join() === 'true,true,true,true,false,false,false' && ph.stepsFirstScreen && ph.plate === 'rgb(22, 24, 25)'
        && ph.tn.join() === 'false,false,true,true,false,false,false' && ph.md.join() === 'true,true,false,false,false,false' && ph.fx.join() === 'true,false,true,true,false,false' && ph.fx2.join() === 'false,true,false,false'
        && ph.mt.join() === 'false,true,true,false' && ph.grow === 1, JSON.stringify(ph));
    }
    if (label === 'wide-light') {
      // Clear / Init / Demo sit behind the project key
      const proj = async id => { if (!(await page.isVisible('#' + id))) await page.click('#projbtn'); await page.click('#' + id); };
      // v69: MOD on the wide screen: one module under its tabs, the track's screen and the module both on the first
      // screen (1000 x 680), for each of the four; OPTION at the end of the transport, its menu under it
      {
        await page.click('#ptabs [data-tab="md"]'); await page.waitForTimeout(80);
        const wm = { fit: [] };
        for (const id of ['LFO1', 'LFO2', 'EF1', 'EF2']) {
          await page.click(`#kp-md .kret.cur .rtab[data-ret="${id}"]`); await page.waitForTimeout(60);
          wm.fit.push(await page.evaluate(id => { const a = document.getElementById('tspec').getBoundingClientRect(), m = document.getElementById('km-' + id).getBoundingClientRect();
            return a.top >= 0 && m.bottom <= innerHeight && document.documentElement.scrollHeight <= innerHeight + 1 && getComputedStyle(document.getElementById('km-' + id)).display !== 'none'; }, id));
        }
        wm.view = await page.evaluate(() => window.__impakt.trackView());
        await page.screenshot({ path: `${shotDir}/impakt-${label}-mod-ef2.png` });
        await page.click('#kp-md .kret.cur .rtab[data-ret="LFO1"]');
        wm.opt = await page.evaluate(() => { const b = document.getElementById('projbtn').getBoundingClientRect(), s = document.getElementById('swg').getBoundingClientRect(); return b.left > s.right && Math.abs(b.top - s.top) < 12; });
        await page.click('#projbtn'); await page.waitForTimeout(60);
        wm.menu = await page.evaluate(() => { const m = document.getElementById('projrow').getBoundingClientRect(), b = document.getElementById('projbtn').getBoundingClientRect(); return m.top >= b.bottom && m.right <= innerWidth && m.height > 150; });
        await page.screenshot({ path: `${shotDir}/impakt-${label}-option.png` });
        await page.click('#projbtn');
        wm.shut = await page.isHidden('#projrow');
        await page.click('#ptabs [data-tab="ms"]'); await page.waitForTimeout(60);
        ok('v69 wide: MOD, each of its four modules under its tab with the track\'s screen on the first screen (no scrolling); OPTION after SWING, its menu under it',
          wm.fit.join() === 'true,true,true,true' && wm.view === 'MODS' && wm.opt && wm.menu && wm.shut, JSON.stringify(wm));
      }
      // v70: TONE, its four modules under their tabs (FILTER, TONE, AMP, BOX), the visualiser above them and a module on
      // the first screen; its handles: MID dragged up and right (the band's gain and frequency, TONE's tab), the filter
      // dragged left (its cutoff, FILTER's tab), HP pulled in from its end (on), a double tap (MID flat again), a drag
      // on a step held up (a p-lock, the sound itself unchanged); its curves against the engine's (render_test v70:
      // the RESP lines, a sine through the filters at a low level)
      {
        const RESP = [
    ['f','LP',0.600,0.000,0.250,-0.52], ['f','LP',0.600,0.000,0.500,-1.93], ['f','LP',0.600,0.000,1.000,-6.02], ['f','LP',0.600,0.000,2.000,-14.08],
    ['f','LP',0.600,0.000,4.000,-25.18], ['f','LP',0.600,0.500,0.250,0.55], ['f','LP',0.600,0.500,0.500,2.44], ['f','LP',0.600,0.500,1.000,16.29],
    ['f','LP',0.600,0.500,2.000,-9.75], ['f','LP',0.600,0.500,4.000,-24.18], ['f','LP',0.600,0.850,0.250,0.56], ['f','LP',0.600,0.850,0.500,2.49],
    ['f','LP',0.600,0.850,1.000,34.31], ['f','LP',0.600,0.850,2.000,-9.70], ['f','LP',0.600,0.850,4.000,-24.17], ['f','BP',0.600,0.500,0.250,-18.83],
    ['f','BP',0.600,0.500,0.500,-10.92], ['f','BP',0.600,0.500,1.000,8.96], ['f','BP',0.600,0.500,2.000,-10.99], ['f','BP',0.600,0.500,4.000,-19.16],
    ['f','HP',0.600,0.000,0.250,-12.32], ['f','HP',0.600,0.000,0.500,-7.00], ['f','HP',0.600,0.000,1.000,-3.01], ['f','HP',0.600,0.000,2.000,-0.96],
    ['f','HP',0.600,0.000,4.000,-0.25], ['f','HP',0.600,0.500,0.250,-11.25], ['f','HP',0.600,0.500,0.500,-2.63], ['f','HP',0.600,0.500,1.000,19.30],
    ['f','HP',0.600,0.500,2.000,3.37], ['f','HP',0.600,0.500,4.000,0.76], ['f','COMB',0.500,0.700,1.000,6.26], ['f','COMB',0.500,0.700,1.500,-8.10],
    ['f','COMB',0.500,0.700,2.000,6.24], ['e','HP',0.300,60.0,-12.40], ['e','HP',0.300,121.0,-2.99], ['e','HP',0.300,400.0,-0.04],
    ['e','LP',0.500,1414.0,-3.01], ['e','LP',0.500,4000.0,-18.48], ['e','LOW',0.900,50.0,11.62], ['e','LOW',0.900,126.0,6.04],
    ['e','LOW',0.900,1000.0,0.00], ['e','MID',0.900,1095.0,12.00], ['e','MID',0.900,300.0,1.05], ['e','HIGH',0.900,4743.0,6.00],
    ['e','HIGH',0.900,12000.0,11.83], ['e','HIGH',0.900,500.0,0.00]];
        await page.evaluate(() => window.__impakt.selectTrack(0));
        await page.click('#ptabs [data-tab="tn"]'); await page.waitForTimeout(120);
        const tz = { fit: [] };
        for (const id of ['FLT', 'TONE', 'AMP', 'BOX']) {
          await page.click(`#kp-tn .kret.cur .rtab[data-ret="${id}"]`); await page.waitForTimeout(60);
          tz.fit.push(await page.evaluate(id => { const a = document.getElementById('tspec').getBoundingClientRect(), m = document.getElementById('km-' + id).getBoundingClientRect();
            return a.top >= 0 && a.height > 150 && m.bottom <= innerHeight && document.documentElement.scrollHeight <= innerHeight + 1 && getComputedStyle(document.getElementById('km-' + id)).display !== 'none'; }, id));
        }
        tz.tabs = await page.evaluate(() => [...document.querySelectorAll('#km-BOX .rtab')].map(b => b.textContent).join());
        await page.click('#kp-tn .kret.cur .rtab[data-ret="FLT"]'); await page.waitForTimeout(150);
        const hnd = id => page.evaluate(id => { const n = window.__impakt.ui.tz.nodes.find(n => n.id === id), r = document.getElementById('tspec').getBoundingClientRect(); return { x: r.left + n.x, y: r.top + n.y }; }, id);
        const par = () => page.evaluate(() => { const I = window.__impakt, P = I.P, p = I.state.tracks[0].p; return { mid: p[P.MID], midf: p[P.MIDF], freq: p[P.FREQ], res: p[P.RES], hpf: p[P.HPF], tab: I.ui.tonet || 'FLT' }; });
        const drag = async (id, dx, dy) => { const h = await hnd(id); await page.mouse.move(h.x, h.y); await page.mouse.down(); await page.mouse.move(h.x + dx / 2, h.y + dy / 2, { steps: 3 }); await page.mouse.move(h.x + dx, h.y + dy, { steps: 3 }); return h; };
        tz.n = await page.evaluate(() => window.__impakt.ui.tz.nodes.map(n => n.id).join());
        tz.p0 = await par();
        await drag('MID', 40, -30);
        tz.held = await page.evaluate(() => window.__impakt.ui.tzDrag && window.__impakt.ui.tzDrag.moved);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-tone-drag.png` });
        await page.mouse.up();
        tz.p1 = await par();
        await drag('FLT', -120, 0); await page.mouse.up();
        tz.p2 = await par();
        await drag('HP', 60, 0); await page.mouse.up();
        tz.p3 = await par();
        { const h = await hnd('MID'); await page.mouse.click(h.x, h.y); await page.mouse.click(h.x, h.y); }
        tz.p4 = await par();
        // a step held up: the drag writes a p-lock there
        await page.evaluate(() => { const I = window.__impakt; I.ui.lock = new Set([2]); I.renderAll(); });
        await page.waitForTimeout(60);
        await drag('MID', 0, -25); await page.mouse.up();
        tz.lock = await page.evaluate(() => { const I = window.__impakt, P = I.P, tr = I.state.tracks[0]; return { l: tr.steps[2].l[P.MID], base: tr.p[P.MID] }; });
        await page.evaluate(() => { const I = window.__impakt, P = I.P, tr = I.state.tracks[0]; delete tr.steps[2].l[P.MID]; delete tr.steps[2].l[P.MIDF]; I.ui.lock = null;
          for (const [q, v] of [[P.FREQ, 1], [P.RES, 0.1], [P.HPF, 0], [P.MID, 0.5], [P.MIDF, 0.5]]) I.setTrackParam(q, v); I.renderAll(); });
        // the curves against the engine
        tz.curve = await page.evaluate(RESP => { const I = window.__impakt, FT = { LP: 0, BP: 1, HP: 2, COMB: 3 }, K = { HP: 'hp', LP: 'lp', LOW: 'lo', MID: 'md', HIGH: 'hi' }, q0 = I.toneParams(0);
          let worst = 0;
          for (const r of RESP) {
            const v = r[0] === 'f' ? I.fltDb(20 * Math.pow(2, r[2] * 9.9658) * r[4], { ft: FT[r[1]], fn: r[2], rs: r[3], ladder: false }) : I.toneDb(r[3], { ...q0, [K[r[1]]]: r[2] });
            worst = Math.max(worst, Math.abs(v - r[r.length - 1]));
          }
          return +worst.toFixed(2); }, RESP);
        await page.click('#ptabs [data-tab="fx"]'); await page.waitForTimeout(100);
        tz.fx = await page.evaluate(() => ({ spec: getComputedStyle(document.getElementById('tspec')).display, box: !document.querySelector('#kp-fx #km-BOX'), send: getComputedStyle(document.getElementById('km-SEND')).display !== 'none',
          side: (() => { const s = document.getElementById('km-SEND').getBoundingClientRect(), r = document.querySelector('#kp-fx .kret.cur').getBoundingClientRect(); return Math.abs(s.top - r.top) < 2 && s.right < r.left; })() }));
        await page.click('#ptabs [data-tab="ms"]'); await page.waitForTimeout(60);
        ok('v70 wide: TONE under its four tabs (FILTER TONE AMP BOX), the visualiser and the module on the first screen; FX: no track screen, SEND beside the return, no BOX',
          tz.fit.join() === 'true,true,true,true' && tz.tabs === 'FILTER,TONE,AMP,BOX' && tz.n === 'FLT,HP,LP,LO,MID,HI' && tz.fx.spec === 'none' && tz.fx.box && tz.fx.send && tz.fx.side, JSON.stringify({ fit: tz.fit, tabs: tz.tabs, n: tz.n, fx: tz.fx }));
        ok('v70: the visualiser\'s handles: MID up and right (gain, frequency, TONE\'s tab), the filter left (its cutoff, FILTER\'s tab), HP pulled in, a double tap flat again, a drag on a held step writes a p-lock',
          tz.held && tz.p1.mid > 0.6 && tz.p1.midf > tz.p0.midf + 0.02 && tz.p1.tab === 'TONE' && tz.p2.freq < tz.p0.freq - 0.1 && tz.p2.tab === 'FLT' && tz.p3.hpf > 0.05 && tz.p3.tab === 'TONE'
          && Math.abs(tz.p4.mid - 0.5) < 1e-9 && tz.lock.l > 0.55 && Math.abs(tz.lock.base - 0.5) < 1e-9, JSON.stringify({ p0: tz.p0, p1: tz.p1, p2: tz.p2, p3: tz.p3, p4: tz.p4, lock: tz.lock, held: tz.held }));
        ok('v70: the visualiser draws the engine\'s own filters (MS-20 LP BP HP, COMB, TONE\'s HP LP and bands: within 1 dB of a sine measured through them)', tz.curve < 1, String(tz.curve));
      }
      await page.click('#play');
      await page.waitForTimeout(2500);
      const pos = await page.evaluate(() => window.__impakt.ui.pos);
      ok('engine runs + playhead moves', pos.run === 1 && pos.steps.every(s => s >= 0), JSON.stringify(pos.steps));
      const notice = await page.evaluate(() => { const n = document.getElementById('notice'); return { hidden: n.hidden, text: n.textContent }; });
      ok('audio started without error', notice.hidden, notice.text);
      const hits = await page.evaluate(() => window.__impakt.ui.pos.hits);
      ok('tracks are triggering', hits.filter(x => x > 0).length >= 6, JSON.stringify(hits));
      ok('worst process time fits', true);
      // single tap toggles step 2 of track 1
      const before = await page.evaluate(() => window.__impakt.state.tracks[0].steps[1].on);
      await page.locator('.step').nth(1).dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true });
      const after = await page.evaluate(() => window.__impakt.state.tracks[0].steps[1].on);
      ok('tap toggles a step', before !== after);
      await page.waitForTimeout(500);
      // drag up on step 5 -> ratchet (step 5 is off in the demo: the press sets it, the drag splits it)
      await page.locator('.step').nth(4).scrollIntoViewIfNeeded();
      const sb = await page.locator('.step').nth(4).boundingBox();
      await page.mouse.move(sb.x + sb.width / 2, sb.y + sb.height / 2);
      await page.mouse.down(); await page.mouse.move(sb.x + sb.width / 2, sb.y + sb.height / 2 - 44, { steps: 6 }); await page.mouse.up();
      const r1 = await page.evaluate(() => { const st = window.__impakt.state.tracks[0].steps[4]; return { on: st.on, r: st.r, segs: document.querySelectorAll('.step')[4].querySelectorAll('.rt i').length, scr: document.getElementById('pval').textContent }; });
      ok('drag up on a step sets ratchets', r1.on === 1 && r1.r === 4 && r1.segs === 4 && r1.scr === '×4', JSON.stringify(r1));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-ratchet.png`, fullPage: true });
      await page.mouse.move(sb.x + sb.width / 2, sb.y + sb.height / 2);
      await page.mouse.down(); await page.mouse.move(sb.x + sb.width / 2, sb.y + sb.height / 2 + 30, { steps: 6 }); await page.mouse.up();
      const r2 = await page.evaluate(() => { const st = window.__impakt.state.tracks[0].steps[4]; return { on: st.on, r: st.r || 1 }; });
      ok('drag down reduces ratchets, step stays on', r2.on === 1 && r2.r === 2, JSON.stringify(r2));
      await page.mouse.move(sb.x + sb.width / 2, sb.y + sb.height / 2);
      await page.mouse.down(); await page.mouse.up();
      const r3 = await page.evaluate(() => window.__impakt.state.tracks[0].steps[4].on);
      ok('plain tap (press + release) switches an active step off', r3 === 0);
      // double tap on step 3 (empty): it comes up for p-locks as a change of the sound only (yellow LED), the trig as it was
      for (let i = 0; i < 2; i++) { await page.locator('.step').nth(2).dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true }); await page.waitForTimeout(60); }
      const stepInfo = () => page.evaluate(() => { const st = window.__impakt.state.tracks[0].steps[2], el = document.querySelectorAll('.step')[2];
        return { lock: window.__impakt.ui.lock ? [...window.__impakt.ui.lock] : null, on: st.on, tl: st.tl || 0, up: el.classList.contains('sel'), yellow: el.classList.contains('tl'),
          lift: getComputedStyle(el).transform, bar: document.querySelector('#lockbar .lsteps b') ? document.querySelector('#lockbar .lsteps b').className : '' }; });
      const lk = await stepInfo();
      ok('double tap brings the step up for p-lock (an empty step: a change of the sound only)', lk.lock && lk.lock.length === 1 && lk.lock[0] === 2 && lk.on === 0 && lk.tl === 1
        && lk.up && lk.yellow && lk.lift !== 'none' && lk.bar === 'pc', JSON.stringify(lk));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-lift.png`, fullPage: true });
      // a tap on the step that is up: a trig that comes with its locks (red LED)
      const tapUp = async () => { const el = page.locator('.step').nth(2); await el.dispatchEvent('pointerdown', { pointerId: 2, isPrimary: true }); await el.dispatchEvent('pointerup', { pointerId: 2, isPrimary: true }); await page.waitForTimeout(30); };
      await tapUp();
      const lkb = await stepInfo();
      ok('a tap on a step that is up: trig <-> change of the sound only', lkb.on === 1 && lkb.tl === 0 && lkb.up && !lkb.yellow && lkb.bar === "tg", JSON.stringify(lkb));
      // turn knob 1 (TUNE) up -> lock stored on step 3 only, track value unchanged
      const tuneBefore = await page.evaluate(() => window.__impakt.state.tracks[0].p[1]);
      await page.locator('.knob').nth(0).scrollIntoViewIfNeeded();
      const box = await page.locator('.knob').nth(0).boundingBox();
      await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
      await page.mouse.down(); await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2 - 60, { steps: 6 }); await page.mouse.up();
      const lockState = await page.evaluate(() => ({ l: window.__impakt.state.tracks[0].steps[2].l, tune: window.__impakt.state.tracks[0].p[1] }));
      ok('knob in lock mode writes a p-lock', lockState.l[1] !== undefined && lockState.l[1] > tuneBefore && lockState.tune === tuneBefore, JSON.stringify(lockState));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-lock.png`, fullPage: true });
      await page.click('#lkok');
      // a lock-only step: up, made a sound change, a lock, down by a swipe; its LED stays yellow, the engine gets it
      await page.evaluate(() => { window.__ops = window.__ops || []; });
      for (let i = 0; i < 2; i++) { await page.locator('.step').nth(8).dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true }); await page.waitForTimeout(60); }
      const tl0 = await page.evaluate(() => ({ on: window.__impakt.state.tracks[0].steps[8].on, tl: window.__impakt.state.tracks[0].steps[8].tl, up: !!window.__impakt.ui.lock }));
      await page.locator('#mknobs .knob').nth(0).focus(); await page.keyboard.press('ArrowRight');
      { await page.locator('.step').nth(8).scrollIntoViewIfNeeded(); const sb8 = await page.locator('.step').nth(8).boundingBox();
        await page.mouse.move(sb8.x + sb8.width / 2, sb8.y + sb8.height / 2); await page.mouse.down();
        await page.mouse.move(sb8.x + sb8.width / 2, sb8.y + sb8.height / 2 + 30, { steps: 5 }); await page.mouse.up(); }
      const tl1 = await page.evaluate(() => { const st = window.__impakt.state.tracks[0].steps[8], el = document.querySelectorAll('.step')[8];
        return { lock: window.__impakt.ui.lock, on: st.on, tl: st.tl, l: Object.keys(st.l).length, yellow: el.classList.contains('tl'), up: el.classList.contains('sel') }; });
      ok('a lock-only step keeps its locks after a swipe down, yellow LED', tl0.on === 0 && tl0.tl === 1 && tl0.up && tl1.lock === null && tl1.on === 0 && tl1.tl === 1 && tl1.l === 1 && tl1.yellow && !tl1.up,
        JSON.stringify({ tl0, tl1 }));
      // a lock-only step without locks goes back to an empty step when it comes down
      for (let i = 0; i < 2; i++) { await page.locator('.step').nth(9).dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true }); await page.waitForTimeout(60); }
      await page.click('#lkok');
      const tl2 = await page.evaluate(() => { const st = window.__impakt.state.tracks[0].steps[9]; return { on: st.on, tl: st.tl || 0 }; });
      ok('an empty step brought up and down without a lock stays empty', tl2.on === 0 && tl2.tl === 0, JSON.stringify(tl2));
      // CONTROL ALL: one knob drag moves the same parameter on all 8 tracks, relatively
      await page.click('#allbtn');
      const decBefore = await page.evaluate(() => window.__impakt.state.tracks.map(t => t.p[2]));
      await page.locator('.knob').nth(1).scrollIntoViewIfNeeded();
      const kb = await page.locator('.knob').nth(1).boundingBox();
      await page.mouse.move(kb.x + kb.width / 2, kb.y + kb.height / 2);
      await page.mouse.down(); await page.mouse.move(kb.x + kb.width / 2, kb.y + kb.height / 2 - 36, { steps: 4 }); await page.mouse.up();
      const decAfter = await page.evaluate(() => window.__impakt.state.tracks.map(t => t.p[2]));
      const deltas = decAfter.map((v, i) => +(v - decBefore[i]).toFixed(3));
      ok('CONTROL ALL moves all 8 tracks', deltas.every(d => d > 0.05), JSON.stringify(deltas));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-all.png`, fullPage: true });
      // a track left out (its key in the ALL bar) stays where it is; REVERT brings every track back to before ALL
      await page.locator('#lockbar .aled').nth(1).click();
      await page.locator('.knob').nth(1).scrollIntoViewIfNeeded();
      const kb2 = await page.locator('.knob').nth(1).boundingBox();
      await page.mouse.move(kb2.x + kb2.width / 2, kb2.y + kb2.height / 2);
      await page.mouse.down(); await page.mouse.move(kb2.x + kb2.width / 2, kb2.y + kb2.height / 2 - 36, { steps: 4 }); await page.mouse.up();
      const decOut = await page.evaluate(() => window.__impakt.state.tracks.map(t => t.p[2]));
      const ledOff = await page.evaluate(() => [...document.querySelectorAll('#lockbar .aled')].map(b => b.classList.contains('on') ? 1 : 0).join(''));
      // locked in ALL: the LFO destinations and waves (MOD: LFO 1, LFO 2)
      // (v69: on the web MOD shows them one at a time, under their tabs)
      const lockedOf = q => page.evaluate(q => [...document.querySelectorAll(q + ' .knob')].map(k => k.classList.contains('alllock') ? 'L' : '-').join(''), q);
      await openMod('LFO1'); const alk1 = await lockedOf('#k-LFO1');
      await openMod('LFO2'); const lockedK = alk1 + '|' + await lockedOf('#k-LFO2');
      await openMod('LFO1');
      await seqPage();
      await page.click('#allrev');
      const decRev = await page.evaluate(() => window.__impakt.state.tracks.map(t => t.p[2]));
      ok('CONTROL ALL: a track left out does not move, REVERT goes back to before ALL, the modulators\' types and the LFO waves locked',
        Math.abs(decOut[1] - decAfter[1]) < 1e-9 && decOut[0] > decAfter[0] + 0.05 && ledOff === '10111111' && lockedK === 'L-L-----|L-L-----'
        && decRev.every((v, i) => Math.abs(v - decBefore[i]) < 1e-9), JSON.stringify({ decOut, ledOff, lockedK, decRev }));
      await page.locator('#lockbar .aled').nth(1).click();
      await page.click('#allok');
      // BOX (FX page): pick an IR, check the engine label
      await openMod('BOX');
      await page.locator('#k-BOX .knob').nth(0).scrollIntoViewIfNeeded();
      const irBox = await page.locator('#k-BOX .knob').nth(0).boundingBox();
      await page.mouse.move(irBox.x + irBox.width / 2, irBox.y + irBox.height / 2);
      await page.mouse.down(); await page.mouse.move(irBox.x + irBox.width / 2, irBox.y + irBox.height / 2 - 120, { steps: 8 }); await page.mouse.up();
      const irName = await page.locator('#k-BOX .knob').nth(0).locator('.kv').textContent();
      ok('BOX IR knob selects a response', irName !== 'OFF', irName);
      await page.screenshot({ path: `${shotDir}/impakt-${label}-box.png`, fullPage: true });
      // REVERB (FX page): its screen's algorithms switch to SPECTRAL, the knobs relabel, MODE reaches FREEZE
      await openMod('REV');
      await page.locator('#ks-REV .algos button', { hasText: 'SPECTRAL' }).click();
      const revL = await page.evaluate(() => ({ alg: Math.floor(window.__impakt.state.g[42] * 6), labels: [...document.querySelectorAll('#k-REV .knob .lt')].map(e => e.textContent).join(',') }));
      ok('reverb strip selects SPECTRAL, knobs relabel', revL.alg === 3 && revL.labels === 'ALGO,TIME,BLUR,ATMO,MODE,WARP,REFL,LVL', JSON.stringify(revL));
      // MODE has 4 positions: a labelled switch; tap FREEZE, then drag down one position
      await page.locator('#k-REV .knob').nth(4).locator('.sw b', { hasText: 'FREEZE' }).click();
      const mode = await page.locator('#k-REV .knob').nth(4).locator('.sw b.on').textContent();
      await page.locator('#k-REV .knob').nth(4).scrollIntoViewIfNeeded();
      const mb = await page.locator('#k-REV .knob').nth(4).boundingBox();
      await page.mouse.move(mb.x + mb.width / 2, mb.y + mb.height / 2);
      await page.mouse.down(); await page.mouse.move(mb.x + mb.width / 2, mb.y + mb.height / 2 + 30, { steps: 5 }); await page.mouse.up();
      const mode2 = await page.locator('#k-REV .knob').nth(4).locator('.sw b.on').textContent();
      ok('spectral MODE switch: tap FREEZE, drag down to the next', mode === 'FREEZE' && mode2 === 'REVERSE', mode + ' ' + mode2);
      await page.screenshot({ path: `${shotDir}/impakt-${label}-spec.png`, fullPage: true });
      await page.locator('#ks-REV .algos button', { hasText: 'SPACE' }).click();
      const spL = await page.evaluate(() => [...document.querySelectorAll('#k-REV .knob .lt')].map(e => e.textContent).join(','));
      ok('SPACE labels (SHIM, MOD)', spL === 'ALGO,TIME,SIZE,DAMP,PRE,SHIM,MOD,LVL', spL);
      // GRAIN on the master chain: the ON key, its knobs, MOD (its LFO's attenuverters), the 64 waves, MEDIA / CLK / FREEZE / RAND
      await openMod('GRN'); await page.waitForTimeout(100);
      const grOff = await page.evaluate(() => ({ on: document.getElementById('mon').textContent, tab: document.querySelector('#gtabs .tab.cur').classList.contains('byp') }));
      await page.click('#mon');
      const knobL = () => page.evaluate(() => [...document.querySelectorAll('#gknobs .knob .lt')].map(e => e.textContent).join(','));
      const grL = await knobL();
      await page.locator('#gstrip .grain button', { hasText: 'VINT' }).click();
      await page.locator('#gstrip .grain button', { hasText: '1/4' }).click();
      await page.click('#gfrz');
      const gb = await page.locator('#grnd').boundingBox();
      await page.mouse.move(gb.x + gb.width / 2, gb.y + gb.height / 2); await page.mouse.down();
      await page.mouse.move(gb.x + gb.width / 2, gb.y + gb.height / 2 - 30, { steps: 4 }); await page.mouse.up();
      await page.click('#gmod');
      const modL = await knobL();
      await page.locator('#gknobs .knob').nth(7).focus();
      for (let i = 0; i < 10; i++) await page.keyboard.press('ArrowRight');
      await page.click('#gwave');
      const nW = await page.locator('.wpick button').count();
      await page.locator('.wpick button[data-w="29"]').click();
      await page.waitForTimeout(900);
      const gr = await page.evaluate(() => { const g = window.__impakt.state.g; return { on: g[111], med: Math.floor(g[108] * 4), clk: Math.floor(g[109] * 4), frz: g[110], rnd: g[107],
        wave: Math.floor(g[114] * 64), name: document.getElementById('glwv').textContent, attMix: g[123], fx: window.__impakt.ui.fx,
        lit: [...document.querySelectorAll('#gstrip .grain button.on')].map(b => b.textContent).join(',') }; });
      await page.screenshot({ path: `${shotDir}/impakt-${label}-grain.png`, fullPage: true });
      await page.click('#gmod'); await page.click('#gfrz');
      ok('GRAIN on the master: ON key, knobs, MOD = attenuverters, 64 waves, screen buttons, LFO running',
        grOff.on === 'OFF' && grOff.tab && grL === 'TIME,SIZE,SHAP,PTCH,DENS,FDBK,VERB,MIX' && modL === '~TIME,~SIZE,~SHAP,~PTCH,~DENS,~FDBK,~VERB,~MIX' && nW === 64
        && gr.on > 0.5 && gr.med === 1 && gr.clk === 2 && gr.frz > 0.5 && gr.rnd > 0.4 && gr.wave === 29 && gr.name === 'TRESIL' && gr.attMix > 0.55 && gr.lit === 'VINT,1/4,FREEZE'
        && gr.fx && gr.fx[5] === 1, JSON.stringify({ grOff, grL, modL, nW, gr }));
      // OTT, last of the chain
      await page.locator('#gtabs .tab', { hasText: 'OTT' }).click(); await page.click('#mon'); await page.waitForTimeout(900);
      const ot = await page.evaluate(() => ({ labels: [...document.querySelectorAll('#gknobs .knob .lt')].map(e => e.textContent).join(','), vals: [...document.querySelectorAll('#gknobs .knob .kv')].map(e => e.textContent).join(','),
        on: window.__impakt.state.g[127], fx: window.__impakt.ui.fx, low: document.getElementById('ottlv').textContent }));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-ott.png`, fullPage: true });
      ok('OTT at the end of the chain: its defaults, gain per band on the screen', ot.labels === 'TIME,UPWD,DNWD,LOW,MID,HIGH,OUT,MIX' && ot.vals === '100%,100%,100%,0dB,0dB,0dB,0dB,100%'
        && ot.on > 0.5 && ot.fx && ot.fx[6] === 1 && ot.fx[2] < -1 && ot.low !== '0.0', JSON.stringify(ot));
      await page.click('#mon');
      await page.locator('#gtabs .tab', { hasText: 'GRN' }).click(); await page.click('#mon');
      // EQ's ON key
      await page.locator('#gtabs .tab', { hasText: 'EQ' }).click(); await page.click('#mon');
      const eqOff = await page.evaluate(() => ({ g: window.__impakt.state.g[124], byp: document.querySelector('#gtabs .tab.cur').classList.contains('byp') }));
      await page.click('#mon');
      const eqOn = await page.evaluate(() => window.__impakt.state.g[124]);
      ok('ON keys take the chain effects out and back in', eqOff.g < 0.5 && eqOff.byp && eqOn > 0.5, JSON.stringify({ eqOff, eqOn }));
      await openMod('REV');
      await page.screenshot({ path: `${shotDir}/impakt-${label}-rev.png`, fullPage: true });
      // DELAY (FX page) and OUT (MASTR): the delay's four, the clipper and the volume
      await openMod('DLY');
      const fxL = await page.evaluate(() => ['#k-DLY', '#k-CHO', '#k-PHS', '#k-OUT'].map(q => [...document.querySelectorAll(q + ' .knob .lt')].map(e => e.textContent).join(',')).join('|'));
      await openMod('CHO'); const choL = await page.evaluate(() => [...document.querySelectorAll('#k-CHO .knob .lt')].map(e => e.textContent).join(','));
      await openMod('PHS'); const phsL = await page.evaluate(() => [...document.querySelectorAll('#k-PHS .knob .lt')].map(e => e.textContent).join(','));
      await openMod('MIX'); const outL = await page.evaluate(() => [...document.querySelectorAll('#k-OUT .knob .lt')].map(e => e.textContent).join(','));
      ok('DELAY (and its VCO), CHORUS, PHASER on FX (v68), OUT (CLIP, VOL) on MASTR', fxL.split('|')[0] === 'TIME,FDBK,TONE,PING,VCO,RATE,SHAP,ENV' && choL === 'MODE,INPT,LEVL'
        && phsL === 'RAT1,SHP1,DEPA,FDBA,B ON,ROUT,RAT2,SHP2,DEPB,FDBB,SYNC,LEVL' && outL === 'CLIP,VOL', [fxL, choL, phsL, outL].join(' / '));
      await seqPage();
      // quantiser: clear, pick C and E and G, name and engine op; Q and r chips
      await page.click('#qclr');
      for (const n of [0, 4, 7]) await page.locator(`#kb .key[data-n="${n}"]`).dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true });
      const q = await page.evaluate(() => ({ mask: window.__impakt.state.scale, name: document.getElementById('qname').textContent, lit: document.querySelectorAll('#kb .key.on').length }));
      ok('quantiser keys build a scale', q.mask === (1 | 16 | 128) && q.lit === 3 && q.name === 'CUSTOM · 3', JSON.stringify(q));
      await page.click('#qname');
      const q2 = await page.evaluate(() => ({ mask: window.__impakt.state.scale, name: document.getElementById('qname').textContent }));
      ok('scale preset button (C MAJ)', q2.name === 'C MAJ' && q2.mask === 0xAB5, JSON.stringify(q2));
      await page.click('#qup');
      const q3 = await page.evaluate(() => document.getElementById('qname').textContent);
      ok('transpose up (C# MAJ)', q3 === 'C# MAJ', q3);
      await page.locator('#tsub .tchip.q').nth(1).click();
      await page.locator('#tsub .tchip.r').nth(2).click();
      const ch = await page.evaluate(() => ({ q1: window.__impakt.state.tracks[1].q, r2: window.__impakt.state.tracks[2].rx, cls: document.querySelectorAll('#tsub .tchip.q')[1].className }));
      ok('Q and R toggles under the pads', ch.q1 === 0 && ch.r2 === 1 && !ch.cls.includes(' on'), JSON.stringify(ch));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-quant.png`, fullPage: true });
      // random: one track, then all but the excluded one, then undo
      const p0 = await page.evaluate(() => window.__impakt.state.tracks.map(t => t.p.slice()));
      await page.click('#rndt');
      const p1 = await page.evaluate(() => window.__impakt.state.tracks.map(t => t.p.slice()));
      const changed = p1.map((p, t) => p.filter((v, i) => v !== p0[t][i]).length);
      ok('Rnd piste changes only the current track (machine and volume kept)', changed[0] > 10 && changed.slice(1).every(c => c === 0) && p1[0][0] === p0[0][0] && p1[0][17] === p0[0][17], JSON.stringify(changed));
      await page.click('#rnda');
      const p2 = await page.evaluate(() => window.__impakt.state.tracks.map(t => t.p.slice()));
      const ch2 = p2.map((p, t) => p.filter((v, i) => v !== p1[t][i]).length);
      ok('Rnd tout skips the excluded track', ch2[2] === 0 && ch2.filter(c => c > 5).length === 7, JSON.stringify(ch2));
      await page.click('#rndu');
      const p3 = await page.evaluate(() => window.__impakt.state.tracks.map(t => t.p.slice()));
      ok('undo restores the previous values', JSON.stringify(p3) === JSON.stringify(p1));
      // LIVE: the temporary random beside the random tools. A drag up sends it, the sounds stay as they are written
      // (nothing changes in the project), a double tap brings it back to 0
      {
        await page.evaluate(() => { window.__ops = []; });
        const box = await page.locator('#rlive').boundingBox();
        const cx = box.x + 18, cy = box.y + box.height / 2;
        await page.mouse.move(cx, cy); await page.mouse.down(); await page.mouse.move(cx, cy - 40, { steps: 6 }); await page.mouse.up();
        const lv = await page.evaluate(() => { const I = window.__impakt; return { g: I.state.g[I.G.XRND], txt: document.getElementById('rlivev').textContent, on: document.getElementById('rlive').classList.contains('on'),
          arc: document.querySelector('#rlive .val').getAttribute('d') || '', p: JSON.stringify(I.state.tracks.map(t => t.p)) }; });
        await page.mouse.click(cx, cy); await page.waitForTimeout(60); await page.mouse.click(cx, cy);
        const lv0 = await page.evaluate(() => { const I = window.__impakt; return { g: I.state.g[I.G.XRND], txt: document.getElementById('rlivev').textContent, on: document.getElementById('rlive').classList.contains('on') }; });
        ok('LIVE: drag up plays the temporary random, the tracks unchanged, double tap back to 0', lv.g > 0.15 && lv.txt === Math.round(lv.g * 100) + '%' && lv.on && lv.arc.length > 10
          && lv.p === JSON.stringify(p3) && lv0.g === 0 && lv0.txt === '0%' && !lv0.on, JSON.stringify({ g: lv.g, txt: lv.txt, on: lv.on, lv0 }));
      }
      // v89: WTF?! (LIVE's key): a tap on its name opens its settings in place of the looper and the scale; a new mode keeps
      // how much (LOOMND takes it to the right); LOOMND's length, NEW, its loop drawn; the LFO's wave, rate, clock, how it
      // runs, its spread, its wave drawn; all of it sent; × brings the looper and the scale back
      {
        const lb = await page.locator('#rlive .lvl').boundingBox();
        await page.mouse.click(lb.x + lb.width / 2, lb.y + lb.height / 2);
        const vis = () => page.evaluate(() => ({ open: !document.getElementById('wtfbar').hidden, loop: getComputedStyle(document.getElementById('loopbar')).display, qz: getComputedStyle(document.getElementById('qz')).display,
          exp: document.getElementById('rlive').getAttribute('aria-expanded'), lab: document.querySelector('#rlive .lvl small').textContent }));
        const w0 = await vis();
        await page.evaluate(() => { window.__ops = []; const I = window.__impakt; I.setGlobal(I.G.XRND, 0.4); I.renderAll(); });
        await page.locator('#wmodes [data-m="1"]').click();
        const w1 = await page.evaluate(() => { const I = window.__impakt, g = I.state.g, G = I.G; return { m: Math.floor(g[G.WTFM] * 3), x: g[G.XRND], txt: document.getElementById('rlivev').textContent,
          loom: !document.getElementById('wloom').hidden, bars: document.querySelectorAll('#wloop rect').length }; });
        await page.locator('#wlens [data-i="3"]').click();
        const seed0 = await page.evaluate(() => window.__impakt.state.g[window.__impakt.G.WTFS]);
        await page.locator('#wnew').click();
        const w2 = await page.evaluate(() => { const I = window.__impakt, g = I.state.g, G = I.G; return { len: Math.floor(g[G.WTFL] * 13), seed: g[G.WTFS], bars: document.querySelectorAll('#wloop rect').length }; });
        await page.locator('#wmodes [data-m="2"]').click();
        await page.locator('#wwaves [data-i="1"]').click();
        await page.locator('#wclk').click();
        await page.locator('#wtrig [data-i="1"]').click();
        const w3 = await page.evaluate(() => { const I = window.__impakt, g = I.state.g, G = I.G; return { m: Math.floor(g[G.WTFM] * 3), x: Math.round(g[G.XRND] * 100), txt: document.getElementById('rlivev').textContent,
          wave: Math.floor(g[G.WTFW] * 8), free: g[G.WTFY] > 0.5, rate: document.getElementById('wratev').textContent, trig: Math.floor(g[G.WTFT] * 3), paths: document.querySelectorAll('#wwave path').length,
          sent: true }; });
        await page.locator('#wmodes [data-m="0"]').click();
        await page.locator('#wclose').click();
        const w4 = await vis();
        await page.evaluate(() => { const I = window.__impakt; I.setGlobal(I.G.XRND, 0); I.renderAll(); });
        ok('WTF?! (v89): its name opens its settings in place of LOOP + SCALE; LOOMND keeps the amount to the right (RND), LEN, NEW, its loop drawn; LFO: wave, FREE (Hz), HIT, its wave drawn; × back',
          w0.open && w0.loop === 'none' && w0.qz === 'none' && w0.exp === 'true' && w0.lab === 'WTF?!'
          && w1.m === 1 && Math.abs(w1.x - 0.7) < 1e-6 && w1.txt === 'RND 40' && w1.loom && w1.bars === 16
          && w2.len === 3 && w2.seed !== seed0 && w2.bars === 4
          && w3.m === 2 && w3.x === 40 && w3.txt === 'LFO 40' && w3.wave === 1 && w3.free && /Hz$/.test(w3.rate) && w3.trig === 1 && w3.paths === 2
          && !w4.open && w4.loop !== 'none' && w4.qz !== 'none' && w4.exp === 'false', JSON.stringify({ w0, w1, w2, w3, w4 }));
      }
      // v90: the modulators M1..M4. The destinations as the engine's (sk_mod_dest), a fresh track's modulators as the
      // engine's defaults; MAP: tap M1 (it stays on, the banner), drag FREQ (a route, its ring), tap FREQ (turned over),
      // tap it twice (gone), OK; a TYPE chosen starts from its own settings (RANDOM, ENV); the routes beside the screen:
      // a depth dragged, × removes
      {
        const mo = {};
        const wx = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(path.resolve(__dirname, '../web/impakt.wasm'))), {}).exports;
        wx.sk_init(48000);
        mo.dest = await page.evaluate(() => window.__impakt.MOD_DEST.slice());
        const eng = []; for (let i = 0; i < 256; i++) eng.push(wx.sk_mod_dest(i));
        mo.destSame = mo.dest.length <= 256 && eng.slice(0, mo.dest.length).every((p, i) => p === mo.dest[i]) && eng.slice(mo.dest.length).every(p => p === -1);
        mo.defsSame = await page.evaluate(engDefs => { const I = window.__impakt, P = I.P, t = I.demoState().tracks[0].p;
          return Object.keys(P).filter(n => /^M[1-4]/.test(n)).every(n => Math.abs(t[P[n]] - engDefs[P[n]]) < 1e-6 || /X1$|SEED/.test(n)); }, Array.from({ length: 284 }, (_, p) => wx.sk_param_def(0, p)));
        await page.evaluate(() => { const I = window.__impakt; I.selectTrack(0); for (let m = 0; m < 4; m++) for (let r = 0; r < 4; r++) I.clearRoute(m, r); I.setPTab('tn'); I.openModule('FLT'); I.renderAll(); }); await page.waitForTimeout(120);
        const k1 = await page.locator('#mkeysT .mkey[data-m="0"]').boundingBox();
        await page.mouse.click(k1.x + k1.width / 2, k1.y + k1.height / 2);
        mo.on = await page.evaluate(() => ({ m: window.__impakt.ui.map && window.__impakt.ui.map.m, bar: !document.getElementById('mapbar').hidden }));
        const fk = page.locator('#k-FLT .knob').filter({ has: page.locator('.lt', { hasText: /^FREQ$/ }) }).first();
        await fk.scrollIntoViewIfNeeded();
        const fb = await fk.locator('svg').boundingBox(), fx = fb.x + fb.width / 2, fy = fb.y + fb.height / 2;
        await page.mouse.move(fx, fy); await page.mouse.down(); await page.mouse.move(fx, fy + 50, { steps: 6 }); await page.mouse.up();
        mo.made = await page.evaluate(() => { const I = window.__impakt; return { r: I.modRoutes(0, 0), ring: !!document.querySelector('#k-FLT .knob .mrg'), sign: (document.querySelector('#k-FLT .knob .mgl') || {}).textContent }; });
        await page.waitForTimeout(400); await page.mouse.click(fx, fy);
        mo.over = await page.evaluate(() => window.__impakt.modRoutes(0, 0));
        await page.waitForTimeout(400); await page.mouse.click(fx, fy); await page.waitForTimeout(60); await page.mouse.click(fx, fy);
        mo.gone = await page.evaluate(() => ({ r: window.__impakt.modRoutes(0, 0), ring: !!document.querySelector('#k-FLT .knob .mrg') }));
        await page.click('#mapok');
        mo.off = await page.evaluate(() => ({ map: window.__impakt.ui.map, bar: !document.getElementById('mapbar').hidden }));
        // TYPE: RANDOM, then ENV (the modules on MOD)
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.M2TY, 1.5 / 4); I.setPTab('md'); I.openModule('LFO2'); I.renderAll(); }); await page.waitForTimeout(120);
        mo.rand = await page.evaluate(() => { const I = window.__impakt, P = I.P, t = I.state.tracks[0].p;
          return { labels: [...document.querySelectorAll('#k-LFO2 .knob .lt')].map(e => e.textContent).join(), len: document.querySelectorAll('#k-LFO2 .knob')[2].querySelector('.kv').textContent, seed: t[P.L2SD] }; });
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.M2TY, 2.5 / 4); I.renderAll(); });
        mo.env = await page.evaluate(() => [...document.querySelectorAll('#k-LFO2 .knob .lt')].map(e => e.textContent).join());
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.M2TY, 0.5 / 4); I.renderAll(); });
        // the routes beside the screen: M1 on DEC (+20), its bar dragged right, then ×
        await page.evaluate(() => { const I = window.__impakt; I.ui.modSel = 0; I.setRoute(0, 0, I.P.DEC, 0.3); I.renderAll(); }); await page.waitForTimeout(80);
        const bar = await page.locator('#mroutes .mbar').first().boundingBox();
        await page.mouse.move(bar.x + bar.width * 0.65, bar.y + bar.height / 2); await page.mouse.down(); await page.mouse.move(bar.x + bar.width * 0.9, bar.y + bar.height / 2, { steps: 5 }); await page.mouse.up();
        mo.drag = await page.evaluate(() => ({ r: window.__impakt.modRoutes(0, 0), txt: document.getElementById('mroutes').textContent }));
        await page.click('#mroutes .mx');
        mo.cleared = await page.evaluate(() => window.__impakt.modRoutes(0, 0).length);
        await seqPage();
        ok('v90 modulators: the destinations as the engine\'s; MAP M1 (tap: on, banner), FREQ dragged (a route, its ring, ▲), tapped (turned over), tapped twice (gone), OK; RANDOM, ENV start from their settings; a route\'s depth dragged, × removes',
          mo.destSame && mo.dest.length > 200 && mo.defsSame && mo.on.m === 0 && mo.on.bar
          && mo.made.r.length === 1 && mo.made.r[0].p === 10 && mo.made.r[0].d < -0.2 && mo.made.ring && mo.made.sign === '▲'
          && mo.over.length === 1 && Math.abs(mo.over[0].d + mo.made.r[0].d) < 1e-6 && mo.gone.r.length === 0 && !mo.gone.ring && mo.off.map === null && !mo.off.bar
          && mo.rand.labels === 'TYPE,RATE,LEN,VARI,SMTH,TRIG,CLK,AMT' && mo.rand.len === '8' && mo.rand.seed > 0 && mo.env === 'TYPE,ATK,DEC,SUS,REL,CURV,TRIG,AMT'
          && mo.drag.r.length === 1 && mo.drag.r[0].d > 0.4 && /DEC/.test(mo.drag.txt) && mo.cleared === 0, JSON.stringify(mo));
      }
      // v90: a project of v89 (no "v90"): its LFOs' and followers' destinations (and their locks, in its patterns too)
      // move from the 38 of before onto the grid of 256, the same destination; marked
      {
        const mg = await page.evaluate(() => { const I = window.__impakt, P = I.P, s = JSON.parse(JSON.stringify(I.state));
          delete s.v90; const t = s.tracks[1];
          t.p[P.LDST] = (9 + 0.5) / 38; t.p[P.E2DS] = (14 + 0.5) / 38; t.steps[3].l[P.L2DS] = (2 + 0.5) / 38;
          s.pats[5] = { tracks: JSON.parse(JSON.stringify(s.tracks)), chg: 0 }; s.pats[5].tracks[1].p[P.LDST] = (37 + 0.5) / 38;
          const r = I.migrate(s), f = v => r.tracks[1] ? I.modDestOf(v) : -9;
          return { v90: r.v90, d: f(r.tracks[1].p[P.LDST]), e: f(r.tracks[1].p[P.E2DS]), lk: f(r.tracks[1].steps[3].l[P.L2DS]), pt: r.pats[5] && r.pats[5].tracks ? f(r.pats[5].tracks[1].p[P.LDST]) : -1, P: [P.FREQ, P.VOL, P.DEC, P.MIDF] }; });
        ok('v90: a v89 project\'s routes onto the grid of 256 (FREQ, VOL, a lock on DEC, a pattern\'s MFRQ), marked v90',
          mg.v90 === 1 && mg.d === mg.P[0] && mg.e === mg.P[1] && mg.lk === mg.P[2] && mg.pt === mg.P[3], JSON.stringify(mg));
      }
      await proj('demo'); await proj('demo');
      const eng = await page.evaluate(() => window.__impakt.ui.engine);
      ok('engine is WebAssembly SIMD in Chromium', eng === 'wasm-simd', eng);
      // clear all (two taps to confirm)
      await proj('clrall'); await proj('clrall');
      const anyTrig = await page.evaluate(() => window.__impakt.state.tracks.some(t => t.steps.some(st => st.on)));
      ok('Vider tout clears every track', !anyTrig);
      await proj('demo'); await proj('demo');
      // shift+click (plugin shortcut) on step 6
      await page.locator('.step').nth(5).dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true, shiftKey: true });
      const sk = await page.evaluate(() => window.__impakt.ui.lock ? [...window.__impakt.ui.lock] : null);
      ok('shift+click enters p-lock', sk && sk[0] === 5, JSON.stringify(sk));
      await page.keyboard.press('Escape');
      // machine picker
      await page.click('#mid'); await page.waitForTimeout(100);
      await page.locator('#mpick button[data-m=\"10\"]').click();
      const mach = await page.evaluate(() => window.__impakt.state.tracks[0].p[0]);
      ok('machine picker switches to BYTE', Math.floor(mach * 64) === 10, String(mach));
      // FRACT (multi-particle) machine: pick it, audition, check the source names (transport stopped: while the
      // sequence runs a pad only selects its track)
      await page.click('#play'); await page.waitForTimeout(300);
      await page.click('#mid'); await page.waitForTimeout(100);
      await page.locator('#mpick button[data-m=\"16\"]').click();
      const h0 = await page.evaluate(() => window.__impakt.ui.pos.hits[0]);
      await page.locator('.pad').nth(0).dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true });
      await page.waitForTimeout(300);
      const fr = await page.evaluate(() => ({ name: document.getElementById('mname').textContent, src: document.querySelectorAll('.knob')[2].querySelector('.kv').textContent, hits: window.__impakt.ui.pos.hits[0] }));
      ok('FRACT machine selectable and plays', fr.name === 'FRACT' && fr.src === 'CLAP' && fr.hits > h0, JSON.stringify(fr));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-fract.png`, fullPage: true });
      for (const [i, name] of [[17, 'ITERA'], [18, 'TRIO'], [19, 'SINE'], [20, 'DATA'], [21, 'FM·Z'], [22, 'HARD'], [23, 'HEX·BD'], [24, 'HEX·SD'], [25, 'HEX·TM'], [37, 'ALLOY'], [26, 'MACRO']]) {
        await page.click('#mid'); await page.waitForTimeout(100);
        await page.locator(`#mpick button[data-m=\"${i}\"]`).click();
        const hh = await page.evaluate(() => window.__impakt.ui.pos.hits[0]);
        await page.locator('.pad').nth(0).dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true });
        await page.waitForTimeout(250);
        const r = await page.evaluate(() => ({ name: document.getElementById('mname').textContent, labels: [...document.querySelectorAll('#mknobs .knob .lt')].slice(2).map(e => e.textContent).join(','), hits: window.__impakt.ui.pos.hits[0], m: Math.floor(window.__impakt.state.tracks[0].p[0] * 64) }));
        ok(`${name} machine selectable and plays`, r.name === name && r.hits > hh && r.m === i, JSON.stringify(r));
        await page.screenshot({ path: `${shotDir}/impakt-${label}-${name.toLowerCase().replace('·', '')}.png`, fullPage: true });
      }
      await page.click('#play'); await page.waitForTimeout(300);
      // MACRO: the model knob walks the 24 models, HARMONICS / TIMBRE / MORPH take their names from it
      const mcL = () => page.evaluate(() => [...document.querySelectorAll('#mknobs .knob .lt')].slice(2, 8).map(e => e.textContent).join(','));
      const mc0 = await mcL();
      await page.locator('#mknobs .knob').nth(2).focus();
      for (let i = 0; i < 19; i++) await page.keyboard.press('ArrowRight');
      await page.waitForTimeout(80);
      const mc = { va: mc0, dx: await mcL(), model: await page.evaluate(() => document.querySelectorAll('#mknobs .knob')[2].querySelector('.kv').textContent),
        patch: await page.evaluate(() => document.querySelectorAll('#mknobs .knob')[3].querySelector('.kv').textContent), m: await page.evaluate(() => Math.floor(window.__impakt.state.tracks[0].p[3] * 24)) };
      ok('MACRO: 24 models, the labels follow the model', mc.va === 'MODL,DTUN,SQR,SAW,TENV,AUX' && mc.dx === 'MODL,PRST,BRIT,ENV,TENV,AUX' && mc.model === '6OP·B' && mc.m === 19 && /^P\d+$/.test(mc.patch), JSON.stringify(mc));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-macro.png`, fullPage: true });
      // CONTROL ALL leaves the algorithm selectors alone: MACRO's model stays, the other tracks' A moves
      await page.click('#allbtn');
      const aBefore = await page.evaluate(() => window.__impakt.state.tracks.map(t => t.p[3]));
      await page.locator('#mknobs .knob').nth(2).focus();
      for (let i = 0; i < 6; i++) await page.keyboard.press('ArrowRight');
      const aAfter = await page.evaluate(() => window.__impakt.state.tracks.map(t => t.p[3]));
      await page.click('#allok');
      const moved = aAfter.map((v, t) => Math.abs(v - aBefore[t]) > 1e-6);
      // (v62: T7 on ALLOY, its SHAP a selector too: ALL leaves it)
      ok('CONTROL ALL leaves MACRO\'s model alone', !moved[0] && moved.slice(1).filter(Boolean).length >= 4, JSON.stringify({ aBefore, aAfter }));
      // TRIO (still selected from the loop above, last but four): its LP is the 4-pole; AMP page has PUNCH, RND depth moved to the footer
      await page.click('#mid'); await page.waitForTimeout(100);
      await page.locator('#mpick button[data-m=\"18\"]').click();
      await openMod('FLT');
      const lp4 = await page.evaluate(() => document.querySelectorAll('#k-FLT .knob')[0].querySelector('.kv').textContent);
      await openMod('AMP'); await openMod('SEND');
      const amp = await page.evaluate(() => ({ labels: [...document.querySelectorAll('#k-AMP .knob .lt')].map(e => e.textContent).join(','), send: [...document.querySelectorAll('#k-SEND .knob .lt')].map(e => e.textContent).join(','),
        rnd: document.getElementById('rndd').textContent, curv: document.querySelectorAll('#k-AMP .knob')[3].querySelector('.kv').textContent }));
      // (v78: CURV, the amplitude envelope's curve: EXP as it was; LIN at its middle)
      await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.ECRV, 0.5); I.renderAll(); });
      await openMod('AMP');
      amp.lin = await page.evaluate(() => document.querySelectorAll('#k-AMP .knob')[3].querySelector('.kv').textContent);
      await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.ECRV, 1); I.renderAll(); });
      await seqPage();
      ok('TRIO filter is the 4-pole, AMP has PUNCH, (v78) CURV (EXP, LIN at its middle; the sends on FX) and (v81) DRONE, RND depth in the footer', lp4 === 'LP4' && amp.labels === 'VOL,PAN,ATK,CURV,PNCH,DUCK,CHOK,DRONE'
        && amp.curv === 'EXP' && amp.lin === 'LIN' && amp.send === 'DLY,REV,CHO,PHS' && /%$/.test(amp.rnd), JSON.stringify({ lp4, amp }));
      // HEX: the six knobs; on the toms SIZE is a 3-position switch (tap LOW: the base pitch drops); BEND reads DN / UP
      await page.click('#mid'); await page.waitForTimeout(100);
      await page.locator('#mpick button[data-m=\"25\"]').click(); await page.waitForTimeout(80);
      const kL = () => page.evaluate(() => [...document.querySelectorAll('#mknobs .knob .lt')].slice(2, 8).map(e => e.textContent).join(','));
      const kV = i => page.evaluate(i => document.querySelectorAll('#mknobs .knob')[i].querySelector('.kv').textContent, i);
      const kL8 = () => page.evaluate(() => [...document.querySelectorAll('#mknobs .knob .lt')].map(e => e.textContent).join(','));
      const swOf = (i, bank) => page.evaluate(([i, bank]) => { const k = document.querySelectorAll((bank || '#mknobs') + ' .knob')[i];
        return { sw: k.classList.contains('isw'), opts: [...k.querySelectorAll('.sw b')].map(b => b.textContent).join(','), on: (k.querySelector('.sw b.on') || {}).textContent }; }, [i, bank]);
      const tm1 = { labels: await kL(), size: await swOf(7), bend: await kV(3), bal: await kV(4) };
      await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(2).click(); await page.waitForTimeout(80);
      const tm2 = { size: await swOf(7), f: await page.evaluate(() => Math.floor(window.__impakt.state.tracks[0].p[8] * 3)) };
      ok('HEX·TM: SIZE is a HI / MID / LOW switch, BEND reads DN', tm1.labels === 'FLTR,BEND,BAL,CLIK,SWP,SIZE' && tm1.size.sw && tm1.size.opts === 'HI,MID,LOW' && tm1.size.on === 'MID'
        && tm1.bend === 'DN 1.1' && tm1.bal === 'N 45%' && tm2.f === 2 && tm2.size.on === 'LOW', JSON.stringify({ tm1, tm2 }));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-hextm-switch.png`, fullPage: true });
      // HEX·SD: RESO and RIM
      await page.click('#mid'); await page.waitForTimeout(100);
      await page.locator('#mpick button[data-m=\"24\"]').click(); await page.waitForTimeout(80);
      const sd = await kL();
      ok('HEX·SD knobs', sd === 'FLTR,BEND,BAL,CLIK,RESO,RIM', JSON.stringify({ sd }));
      // v63: ALLOY (OBJECT, IMPACT, NOISE, FM, COLOR, SPACE: its side pages in the shared places, SIDE a six-way switch);
      // PENV down to -36 st, BNCE's flams to the left, NCOL as a frequency, RATI on its detents (√2, π), MENV's sweep, CFB
      // under 0 (the odd harmonics), the space's SHFT and CUT; what a side knob turns reaches the engine's track
      {
        const al = {};
        const page6 = async i => { await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(i).click(); await page.waitForTimeout(80); };
        const setP = (q, v) => page.evaluate(([q, v]) => { const I = window.__impakt; I.setTrackParam(I.P[q], v); I.renderAll(); }, [q, v]);
        await page.click('#mid'); await page.waitForTimeout(100);
        await page.locator('#mpick button[data-m="37"]').click(); await page.waitForTimeout(80);
        al.labels = await kL8(); al.shap = await kV(2); al.stif = await kV(3); al.mass = await kV(4); al.size = await kV(5); al.hit = await kV(6); al.side = await swOf(7); al.dec = await kV(1);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-alloy.png`, fullPage: true });
        await page6(1);
        al.imp = await kL8(); al.malt = await kV(0); al.penv = await kV(2); al.bnce = await kV(5); al.orgn = await kV(6);
        await page.locator('#mknobs .knob').nth(2).focus(); for (let i = 0; i < 4; i++) await page.keyboard.press('ArrowRight'); await page.waitForTimeout(60);
        al.penv2 = await kV(2);
        await setP('S3', 0); al.penv3 = await kV(2); await setP('S6', 0); al.flam = await kV(5);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-alloy-impact.png`, fullPage: true });
        await page6(2);
        al.noi = await kL8(); al.ncol = await kV(2); al.nlen = await kV(3); al.fdbk = await kV(4); al.dac = await kV(6);
        await setP('S12', 0.9); al.fdbk2 = await kV(4);
        await page6(3);
        al.fmp = await kL8(); al.obj = await kV(0); al.fm = await kV(1); al.rati = await kV(2); al.menv = await kV(6);
        await setP('S17', Math.log2(Math.PI / 0.5) / 5); al.pi = await kV(2); await setP('S21', 1); al.menv2 = await kV(6);
        await page6(4);
        al.col = await kL8(); al.band = await kV(1); al.ctun = await kV(3); al.cfb = await kV(4);
        await setP('S26', 0.05); al.cfb2 = await kV(4);
        await page6(5);
        al.spc = await kL8(); al.verb = await kV(0); al.room = await kV(1); al.dcay = await kV(2); al.shft = await kV(4); al.cut = await kV(5); al.rate = await kV(6);
        await setP('S34', Math.log2(100 / 20) / 6.2); al.cut2 = await kV(5);
        al.side6 = await swOf(7);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-alloy-space.png`, fullPage: true });
        await page6(0);
        al.back = await kL();
        al.eng = await page.evaluate(() => { const I = window.__impakt, p = I.state.tracks[I.ui.t].p; return [p[I.P.S17], p[I.P.S26], p[I.P.S34]].map(v => Math.round(v * 1000) / 1000).join(); });
        ok('ALLOY: OBJECT (SHAP STIF MASS SIZE HIT SIDE), IMPACT (PENV to −36, flams), NOISE, FM (RATI on √2, then π; MENV), COLOR (CFB under 0: odd), SPACE (CUT); a six-way switch; the side knobs reach the track',
          al.labels === 'TUNE,DEC,SHAP,STIF,MASS,SIZE,HIT,SIDE' && al.shap === 'BAR' && al.stif === 'NAT' && al.mass === 'STEEL 70' && al.size === '23 MODES' && al.hit === '38%'
          && al.side.sw && al.side.opts === 'OBJECT,IMPACT,NOISE,FM,COLOR,SPACE' && al.side.on === 'OBJECT' && al.dec === '1.9s'
          && al.imp === 'MALT,CLIK,PENV,PRIS,PTIM,BNCE,ORGN,SIDE' && al.malt === '290µs' && al.penv === '+6st' && al.penv2 !== al.penv && al.penv3 === '−36st'
          && al.bnce === 'OFF' && al.flam === '×4 10ms' && al.orgn === '30%'
          && al.noi === 'NOIS,HISS,NCOL,NLEN,FDBK,DRIV,DAC,SIDE' && /k$/.test(al.ncol) && /ms$/.test(al.nlen) && al.fdbk === 'OFF' && al.fdbk2 === 'SING 90' && al.dac === 'OFF'
          && al.fmp === 'OBJ,FM,RATI,INDX,IATK,IDEC,MENV,SIDE' && al.obj === '100%' && al.fm === 'OFF' && al.rati === '×√2' && al.pi === '×π' && al.menv === 'OFF' && al.menv2 === '+3.0oct'
          && al.col === 'ADD,BAND,COMB,CTUN,CFB,FLNG,PHAS,SIDE' && /^×\d/.test(al.band) && al.ctun === 'UNI' && al.cfb === '+50%' && al.cfb2 === '−90%'
          && al.spc === 'VERB,ROOM,DCAY,DAMP,SHFT,CUT,RATE,SIDE' && al.verb === '20%' && al.room === '25%' && /ms$|s$/.test(al.dcay) && al.shft === 'OFF' && al.cut === 'OFF' && al.cut2 === '100ms' && /Hz$/.test(al.rate)
          && al.side6.on === 'SPACE' && al.back === 'SHAP,STIF,MASS,SIZE,HIT,SIDE' && al.eng === '0.53,0.05,0.375', JSON.stringify(al));
      }
      // v66: CHIMERA (PLAY, GRAFT, FAMILY: SIDE a three-way switch); CAT / SMPL from the bank's index, VARI's two sides,
      // its LOOP seed drawn again when VARI leaves the middle to the left; the page and the engine agree on its defaults;
      // the bank fetched and decoded by the worklet, a hit played and drawn on the map, VARI RND drawing neighbours, a graft
      {
        const ch = {};
        const page3 = async i => { await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(i).click(); await page.waitForTimeout(80); };
        const setP = (q, v) => page.evaluate(([q, v]) => { const I = window.__impakt; I.setTrackParam(I.P[q], v); I.renderAll(); }, [q, v]);
        const hit = async () => { const b = page.locator('.pad').nth(0); await b.dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true }); await page.waitForTimeout(120);
          await b.dispatchEvent('pointerup', { pointerId: 1, isPrimary: true }); await page.waitForTimeout(60); };
        const info = () => page.evaluate(() => { const c = window.__impakt.ui.chim; return c ? Array.from(c) : null; });
        // the next sound the track plays (the pad, or its own steps while the transport runs: then a pad only selects)
        const next = async () => { const h0 = ((await info()) || [])[6] || 0; await hit(); let h = await info();
          for (let k = 0; k < 60 && !(h && h[6] > h0); k++) { await page.waitForTimeout(50); h = await info(); } return h; };
        await page.click('#mid'); await page.waitForTimeout(100);
        await page.locator('#mpick button[data-m="38"]').click(); await page.waitForTimeout(80);
        ch.labels = await kL8(); ch.dec = await kV(1); ch.cat = await kV(2); ch.smpl = await kV(3); ch.vari = await kV(4); ch.strt = await kV(5); ch.play = await swOf(6); ch.side = await swOf(7);
        ch.cats = await page.evaluate(() => { const I = window.__impakt; return I.paramMeta(I.ui.t, I.P.A).n; });
        await setP('C', 0.9); ch.rnd = await kV(4);
        ch.seed0 = await page.evaluate(() => { const I = window.__impakt; return I.state.tracks[I.ui.t].p[I.P.S15]; });
        await setP('C', 0.2); ch.loop = await kV(4);
        ch.seed1 = await page.evaluate(() => { const I = window.__impakt; return I.state.tracks[I.ui.t].p[I.P.S15]; });
        await setP('C', 0.5); ch.off = await kV(4);
        await page3(1);
        ch.graft = await kL8(); ch.body = await kV(0); ch.bsmp = await kV(1); ch.seam = await kV(2); ch.xfad = await kV(3); ch.splt = await kV(4); ch.btun = await kV(5); ch.blvl = await kV(6);
        await setP('S1', 2.5 / 11); ch.body2 = await kV(0); ch.bsmp2 = await kV(1); await setP('S5', 0.6); ch.splt2 = await kV(4);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-chimera-graft.png`, fullPage: true });
        await setP('S1', 0); await setP('S5', 0);
        await page3(2);
        ch.fam = await kL8(); ch.like = await kV(0); ch.same = await kV(1); ch.vary = await kV(2); ch.ptch = await kV(3);
        await page3(0);
        ch.back = await kL();
        // the page's defaults (MACH: A..F, the side places) against the engine's (sk_af_def, sk_side_def)
        const pg = await page.evaluate(() => { const I = window.__impakt, m = I.MACH[38]; return { d: m.d.slice(2), sd: m.sd }; });
        const wx = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(path.resolve(__dirname, '../web/impakt.wasm'))), {}).exports;
        ch.defs = pg.d.every((v, i) => Math.abs(wx.sk_af_def(38, i) - v) < 1e-4) && pg.sd.every((v, i) => Math.abs(wx.sk_side_def(38, i) - v) < 1e-4);
        // the bank: fetched once a track plays CHIMERA, decoded by the engine (stopped, a few seconds)
        let st = null; for (let k = 0; k < 120 && !(st && st[0] === 2); k++) { await page.waitForTimeout(250); st = await info(); }
        ch.bank = st ? { state: st[0], done: st[1], n: st[2], failed: st[3] } : null;
        const h1 = await next(); await page.waitForTimeout(150);
        ch.lit = await page.evaluate(() => { const cv = document.getElementById('ovw'); let lit = 0;
          if (cv) { const d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data; for (let i = 3; i < d.length; i += 4) if (d[i]) lit++; } return lit; });
        await page.screenshot({ path: `${shotDir}/impakt-${label}-chimera.png`, fullPage: true });
        ch.hit = h1 ? { a: h1[4], b: h1[5], hits: h1[6] } : null;
        // VARI RND 12: a neighbour drawn at each hit
        await setP('C', 1); const seen = new Set();
        for (let k = 0; k < 8; k++) { const h = await next(); if (h) seen.add(h[4]); }
        ch.rndSeen = seen.size; ch.nc = (await info() || [])[9];
        await page.screenshot({ path: `${shotDir}/impakt-${label}-chimera-vari.png`, fullPage: true });
        await setP('C', 0.5);
        // GRAFT: a snare's body under the kick's attack
        await setP('S1', 2.5 / 11); await next(); const hg = await next();
        ch.graftHit = hg ? { a: hg[4], b: hg[5], align: Math.round(hg[7] * 100) / 100 } : null;
        await setP('S1', 0);
        ok('CHIMERA: PLAY (CAT SMPL VARI STRT PLAY SIDE), GRAFT (BODY BSMP SEAM XFAD SPLT BTUN BLVL), FAMILY (LIKE SAME VARY PTCH); VARI RND / LOOP (a new seed); defaults as the engine\'s',
          ch.labels === 'TUNE,DEC,CAT,SMPL,VARI,STRT,PLAY,SIDE' && ch.dec === 'FULL' && ch.cat === 'KICK' && /^1\/\d+$/.test(ch.smpl) && ch.vari === 'OFF' && ch.strt === '0%'
          && ch.play.sw && ch.play.opts === 'FWD,REV' && ch.play.on === 'FWD' && ch.side.sw && ch.side.opts === 'PLAY,GRAFT,FAMILY' && ch.side.on === 'PLAY' && ch.cats === 10
          && ch.rnd === 'RND 10' && ch.loop === 'LOOP 8' && ch.off === 'OFF' && ch.seed1 !== ch.seed0 && ch.seed1 > 0
          && ch.graft === 'BODY,BSMP,SEAM,XFAD,SPLT,BTUN,BLVL,SIDE' && ch.body === 'OFF' && ch.bsmp === '—' && ch.seam === '25.0ms' && ch.xfad === '8.00ms' && ch.splt === 'OFF'
          && ch.btun === 'AUTO' && ch.blvl === '0dB' && ch.body2 === 'SNARE' && /^1\/\d+$/.test(ch.bsmp2) && /k$|^\d+$/.test(ch.splt2)
          && ch.fam === 'LIKE,SAME,VARY,PTCH,—,—,—,SIDE' && ch.like === 'BOTH' && ch.same === 'ON' && ch.vary === 'A' && ch.ptch === 'OFF'
          && ch.back === 'CAT,SMPL,VARI,STRT,PLAY,SIDE' && ch.defs, JSON.stringify(ch));
        ok('CHIMERA: the bank fetched and decoded in the page, a hit on its map, VARI RND drawing neighbours, a graft (B lined up at the seam)',
          ch.bank && ch.bank.state === 2 && ch.bank.done === ch.bank.n && ch.bank.n > 100 && ch.bank.failed === 0
          && ch.hit && ch.hit.a >= 0 && ch.hit.b === -1 && ch.hit.hits > 0 && ch.lit > 500 && ch.rndSeen >= 3 && ch.nc === 12
          && ch.graftHit && ch.graftHit.a >= 0 && ch.graftHit.b >= 0 && Math.abs(ch.graftHit.align) <= 2.6, JSON.stringify({ bank: ch.bank, hit: ch.hit, lit: ch.lit, rndSeen: ch.rndSeen, nc: ch.nc, graftHit: ch.graftHit }));
      }
      // v74: CUBE (MAIN, OSC, MOD: SIDE a three-way switch), its knobs' readings; the page and the engine agree on its side
      // places; a WAV bank read in the LOAD panel (into place 3), its waves made by the engine, played on USER (the Z knob
      // names it, the screen draws its wave), cleared (the factory's again)
      {
        const cb = {};
        const page3 = async i => { await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(i).click(); await page.waitForTimeout(80); };
        const setP = (q, v) => page.evaluate(([q, v]) => { const I = window.__impakt; I.setTrackParam(I.P[q], v); I.renderAll(); }, [q, v]);
        const wave = () => page.evaluate(() => { const c = window.__impakt.ui.cube; return c ? Array.from(c).slice(10, 138) : null; });
        await page.click('#mid'); await page.waitForTimeout(100);
        await page.locator('#mpick button[data-m="40"]').click(); await page.waitForTimeout(80);
        cb.labels = await kL8(); cb.dec = await kV(1); cb.x = await kV(2); cb.y = await kV(3); cb.z = await kV(4); cb.fm = await kV(5); cb.swep = await kV(6); cb.side = await swOf(7);
        await setP('C', 3 / 7); cb.z3 = await kV(4); await setP('C', 3.5 / 7); cb.z35 = await kV(4); await setP('C', 0);
        await setP('E', 0.75); cb.swep2 = await kV(6); await setP('E', 0.5);
        await setP('DEC', 1); cb.hold = await kV(1); await setP('DEC', 0.45);
        await page3(1);
        cb.osc = await kL8(); cb.brat = await kV(0); cb.bdet = await kV(1); cb.bx = await kV(2); cb.blvl = await kV(4); cb.unis = await kV(5); cb.set = await swOf(6);
        await page3(2);
        cb.mod = await kL8(); cb.swpy = await kV(0); cb.stim = await kV(1); cb.penv = await kV(2); cb.ptim = await kV(3); cb.step = await swOf(4); cb.warp = await kV(5); cb.bits = await kV(6);
        await page3(0);
        cb.back = await kL();
        const pg = await page.evaluate(() => window.__impakt.MACH[40].sd);
        const wx = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(path.resolve(__dirname, '../web/impakt.wasm'))), {}).exports;
        cb.defs = pg.length === 14 && pg.every((v, i) => Math.abs(wx.sk_side_def(40, i) - v) < 1e-4) && wx.sk_side_def(40, 14) === 0;
        // the wave the factory's VOWEL bank gives there (Z 3, FACT), then the bank read into place 3
        await setP('C', 2 / 7); await page.waitForTimeout(200);
        const fact = await wave();
        cb.loadBtn = await page.evaluate(() => !document.getElementById('mload').hidden);
        await page.click('#mload'); await page.waitForTimeout(80);
        cb.panel = await page.evaluate(() => ({ title: document.getElementById('ltitle').textContent, rows: document.querySelectorAll('#llist .lrow').length,
          empty: document.querySelectorAll('#llist .lrow.empty').length, first: document.querySelector('#llist .lrow .lname').textContent, accept: document.getElementById('lfile').accept }));
        await page.evaluate(() => { window.__impakt.ui.brkTarget = 2; });
        await page.setInputFiles('#lfile', [{ name: 'Narrowing Pulses.wav', mimeType: 'audio/wav', buffer: cubeWav() }]);
        await page.waitForFunction(() => window.__impakt.cubeState(2) === 2, null, { timeout: 8000 }).catch(() => {});
        await page.waitForTimeout(150);
        cb.loaded = await page.evaluate(() => { const I = window.__impakt, r = document.querySelector('#llist .lrow[data-s="2"]'), t = I.state.tracks[I.ui.t];
          return { st: I.cubeState(2), name: r.querySelector('.lname').textContent, meta: r.querySelector('.lmeta').textContent, sel: r.classList.contains('sel'),
            mem: !!I.cubeMem[2] && I.cubeMem[2].data.length, z: Math.round(t.p[I.P.C] * 7 * 100) / 100, user: Math.floor(t.p[I.P.S7] * 2),
            zk: document.querySelectorAll('#mknobs .knob')[4].querySelector('.kv').textContent, mask: I.ui.cube ? I.ui.cube[7] : -1 }; });
        const user = await wave();
        // (the wave at X 3: the bank's third wave, a pulse just under half its cycle: high, then low)
        cb.userWave = user ? { hi: user.slice(4, 56).reduce((a, v) => a + v, 0) / 52, lo: user.slice(68, 120).reduce((a, v) => a + v, 0) / 52 } : null;
        cb.differ = !!(fact && user) && fact.some((v, i) => Math.abs(v - user[i]) > 0.1);
        await page.locator('.pad').nth(0).dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true }); await page.waitForTimeout(160);
        cb.lit = await page.evaluate(() => { const cv = document.getElementById('ovw'); let lit = 0;
          if (cv) { const d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data; for (let i = 3; i < d.length; i += 4) if (d[i]) lit++; } return lit; });
        await page.screenshot({ path: `${shotDir}/impakt-${label}-cube.png`, fullPage: true });
        await page.locator('.pad').nth(0).dispatchEvent('pointerup', { pointerId: 1, isPrimary: true }); await page.waitForTimeout(60);
        // cleared: the factory's bank there again
        await page.locator('#llist .lrow[data-s="2"] .lsm[data-a="del"]').click(); await page.locator('#llist .lrow[data-s="2"] .lsm[data-a="del"]').click();
        await page.waitForTimeout(300);
        cb.cleared = await page.evaluate(() => { const I = window.__impakt; return { st: I.cubeState(2), mem: I.cubeMem[2], zk: document.querySelectorAll('#mknobs .knob')[4].querySelector('.kv').textContent,
          row: document.querySelector('#llist .lrow[data-s="2"]').classList.contains('empty') }; });
        await page.click('#lclose');
        ok('CUBE: MAIN (X Y Z FM SWEP SIDE), OSC (BRAT BDET BX BY BLVL UNIS SET), MOD (SWPY STIM PENV PTIM STEP WARP BITS); Z names its bank; defaults as the engine\'s',
          cb.labels === 'TUNE,DEC,X,Y,Z,FM,SWEP,SIDE' && cb.dec === '148ms' && cb.hold === 'HOLD' && cb.x === '3.0' && cb.y === '1.0' && cb.z === 'BASIC' && cb.fm === 'OFF' && cb.swep === 'OFF'
          && cb.side.sw && cb.side.opts === 'MAIN,OSC,MOD' && cb.side.on === 'MAIN' && cb.z3 === 'FM' && cb.z35 === '4.5' && cb.swep2 === '+3.5'
          && cb.osc === 'BRAT,BDET,BX,BY,BLVL,UNIS,SET,SIDE' && cb.brat === 'UNI' && cb.bdet === '0ct' && cb.bx === '0' && cb.blvl === 'OFF' && cb.unis === 'OFF'
          && cb.set.sw && cb.set.opts === 'FACT,USER' && cb.set.on === 'FACT'
          && cb.mod === 'SWPY,STIM,PENV,PTIM,STEP,WARP,BITS,SIDE' && cb.swpy === 'OFF' && cb.stim === '72.5ms' && cb.penv === 'OFF' && cb.ptim === '12.9ms'
          && cb.step.sw && cb.step.opts === 'OFF,X,X+Y,XYZ' && cb.step.on === 'OFF' && cb.warp === 'OFF' && cb.bits === 'OFF'
          && cb.back === 'X,Y,Z,FM,SWEP,SIDE' && cb.defs, JSON.stringify(cb));
        ok('CUBE: a WAV bank read into a place (LOAD panel), made by the engine, played on USER (Z names it, its wave on the screen), cleared',
          cb.loadBtn && cb.panel.title === 'WAVETABLES' && cb.panel.rows === 8 && cb.panel.empty === 8 && /BASIC/.test(cb.panel.first) && /wav/.test(cb.panel.accept)
          && cb.loaded.st === 2 && cb.loaded.name === 'Narrowing Pulses' && cb.loaded.meta === '64 waves · ready' && cb.loaded.sel && cb.loaded.mem === 16384
          && cb.loaded.z === 2 && cb.loaded.user === 1 && cb.loaded.zk === 'NARROW' && (cb.loaded.mask & 4) === 4
          && cb.userWave && cb.userWave.hi > 0.6 && cb.userWave.lo < -0.6 && cb.differ && cb.lit > 300
          && cb.cleared.st === 0 && cb.cleared.mem === null && cb.cleared.zk === 'VOWEL' && cb.cleared.row, JSON.stringify({ loadBtn: cb.loadBtn, panel: cb.panel, loaded: cb.loaded, userWave: cb.userWave, differ: cb.differ, lit: cb.lit, cleared: cb.cleared }));
      }
      // v76: WEST; v78: EaseLFO's whole panel; v79: its own panel, by what you hear: seven pages as tabs (PITCH COLOUR
      // SHAPE LAYER RHYTHM MOTION SPACE) in place of the eight knobs (those in CONTROL ALL), two rows a page (what you
      // reach for, the fine tuning), every control named in words; sliders for the envelope's times and the stages (a key
      // under each for its pulse); a CV's source as a jack beside its depth, drawn in the source's colour; the screen
      // drawing the page; a touched control's hint on the screen; the ring where a CV takes TIMBRE while a pad is held;
      // its 70 side places' defaults as the engine's
      {
        const ws = {};
        const setP = (q, v) => page.evaluate(([q, v]) => { const I = window.__impakt; I.setTrackParam(I.P[q], v); I.renderAll(); }, [q, v]);
        const tab = async i => { await page.locator('#wtabs .wtab').nth(i).click(); await page.waitForTimeout(80); };
        const wL = bank => page.evaluate(b => [...document.querySelectorAll(b + ' .knob:not([hidden]) .lt')].map(e => e.textContent).join(','), bank);
        const wV = (bank, i) => page.evaluate(([b, i]) => [...document.querySelectorAll(b + ' .knob:not([hidden])')][i].querySelector('.kv').textContent, [bank, i]);
        const wK = (bank, i) => page.evaluate(([b, i]) => { const k = [...document.querySelectorAll(b + ' .knob:not([hidden])')][i];
          return { cls: [...k.classList].filter(c => /^(isfd|isw|tint|pairl|pairr|stg)$/.test(c)).join(' '), arc: k.style.getPropertyValue('--arc'),
            src: (k.querySelector('.sw.src em span') || {}).textContent || '', dots: k.querySelectorAll('.sw.src p b').length,
            opts: [...k.querySelectorAll('.sw:not(.src) b')].map(b => b.textContent).join(','), on: (k.querySelector('.sw:not(.src) b.on') || {}).textContent || '',
            icons: k.querySelectorAll('.sw b svg').length }; }, [bank, i]);
        const lit = () => page.evaluate(() => { const cv = document.getElementById('ovw'); let n = 0;
          if (cv) { const d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data; for (let i = 3; i < d.length; i += 4) if (d[i]) n++; } return n; });
        await page.click('#mid'); await page.waitForTimeout(100);
        await page.locator('#mpick button[data-m="41"]').click(); await page.waitForTimeout(80);
        ws.panel = await page.evaluate(() => ({ mk: document.getElementById('mknobs').hidden, wp: document.getElementById('wpanel').hidden,
          tabs: [...document.querySelectorAll('#wtabs .wtab span')].map(e => e.textContent).join(','), role: document.getElementById('wtabs').getAttribute('role') }));
        ws.lit = [];
        await tab(0); ws.pitch = { main: await wL('#wmain'), fine: await wL('#wfine'), tune: await wV('#wmain', 0), ratio: await wV('#wmain', 1), keys: await wK('#wmain', 3),
          psrc: await wK('#wfine', 0), pamt: await wK('#wfine', 1), say: await page.evaluate(() => document.getElementById('wsay').textContent), sel: await page.evaluate(() => document.querySelector('#wtabs .wtab.on span').textContent) };
        ws.lit.push(await lit());
        await setP('S10', 0.75); ws.rate = { l: await wL('#wfine'), v: await wV('#wfine', 5) }; await setP('S10', 0.25);
        await tab(1); ws.colour = { main: await wL('#wmain'), fine: await wL('#wfine'), timb: await wV('#wmain', 0), index: await wV('#wmain', 2), other: await wK('#wfine', 2), tsrc: await wK('#wfine', 8), tamt: await wK('#wfine', 9), tamtV: await wV('#wfine', 9) };
        ws.lit.push(await lit());
        // the hint of a control touched (the keyboard: up, then down again)
        await page.locator('#wmain .knob').nth(0).focus(); await page.keyboard.press('ArrowUp'); await page.waitForTimeout(40);
        ws.hint = await page.evaluate(() => document.getElementById('mlong').textContent); await page.keyboard.press('ArrowDown'); await page.waitForTimeout(40);
        await tab(2); ws.shape = { main: await wL('#wmain'), fine: await wL('#wfine'), atk: await wK('#wmain', 0), dec: await wK('#wmain', 2), decV: await wV('#wmain', 2), curv: await wK('#wmain', 3), mode: await wK('#wfine', 0) };
        ws.lit.push(await lit());
        await tab(3); ws.layer = { main: await wL('#wmain'), fine: await wL('#wfine'), inp: await wK('#wmain', 1), out2: await wV('#wmain', 0) };
        ws.lit.push(await lit());
        await tab(4); ws.rhythm = { main: await wL('#wmain'), fine: await wL('#wfine'), puls: await wV('#wmain', 0), cnt: await wV('#wmain', 1), start: await wK('#wfine', 0) };
        ws.lit.push(await lit());
        await tab(5); ws.motion = { main: await wL('#wmain'), fine: await wL('#wfine'), s1: await wK('#wmain', 0), v1: await wV('#wmain', 0), dest: await wK('#wmain', 5) };
        await setP('S23', 0.75); ws.motion.v2st = await wV('#wmain', 1); await setP('S28', 1.5 / 4); ws.motion.v2tb = await wV('#wmain', 1); await setP('S28', 0.5 / 4); await setP('S23', 0.5);
        // a stage's key: its pulse on, then off
        await page.locator('#wmain .knob').nth(0).locator('.spk').click(); await page.waitForTimeout(40);
        ws.motion.spl = await page.evaluate(() => { const I = window.__impakt; return [Math.floor(I.state.tracks[I.ui.t].p[I.P.S36 + 49 - 35] * 32), document.querySelector('#wmain .knob .spk').classList.contains('on')]; });
        await page.locator('#wmain .knob').nth(0).locator('.spk').click(); await page.waitForTimeout(40);
        ws.motion.spl0 = await page.evaluate(() => { const I = window.__impakt; return Math.floor(I.state.tracks[I.ui.t].p[I.P.S36 + 49 - 35] * 32); });
        ws.lit.push(await lit());
        await tab(6); ws.space = { main: await wL('#wmain'), fine: await wL('#wfine'), mix: await wV('#wmain', 0), dcy: await wV('#wmain', 2) };
        ws.lit.push(await lit());
        // CONTROL ALL: the eight knobs again
        await page.evaluate(() => { window.__impakt.setAll(true); }); await page.waitForTimeout(60);
        ws.all = { mk: await page.evaluate(() => document.getElementById('mknobs').hidden), wp: await page.evaluate(() => document.getElementById('wpanel').hidden), l: await kL8() };
        await page.evaluate(() => { window.__impakt.setAll(false); }); await page.waitForTimeout(60);
        const pg = await page.evaluate(() => window.__impakt.MACH[41].sd);
        const wx = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(path.resolve(__dirname, '../web/impakt.wasm'))), {}).exports;
        ws.defs = pg.length === 70 && pg.every((v, i) => Math.abs(wx.sk_side_def(41, i) - v) < 1e-4)
          && (await page.evaluate(() => window.__impakt.MACH[41].d)).slice(2).every((v, i) => Math.abs(wx.sk_af_def(41, i) - v) < 1e-4);
        // a v75 project (no "wst") with SPHERE on track 1 and a step of track 3 locked onto it: WEST at its defaults, its
        // A lock gone, the step with WEST's side in its lock; marked (saved by v76), as it is; what the page saves is marked
        ws.mig = await page.evaluate(() => { const I = window.__impakt, P = I.P, SQ = q => q < 35 ? P.S1 + q : P.S36 + q - 35;
          const s = I.demoState(); delete s.wst; const t = s.tracks[0]; t.p[P.MACH] = 41.5 / 64; t.p[P.A] = 0.97; for (let q = 0; q < 35; q++) t.p[P.S1 + q] = 0.6;
          t.steps[5] = Object.assign({}, t.steps[5], { on: 1, l: { [P.A]: 0.05 } }); s.tracks[2].steps[5] = Object.assign({}, s.tracks[2].steps[5], { on: 1, l: { [P.MACH]: 41.5 / 64, [P.S5]: 0.9 } });
          const m = I.migrate(JSON.parse(JSON.stringify(s))), d = I.MACH[41].d, sd = I.MACH[41].sd;
          const s2 = JSON.parse(JSON.stringify(s)); s2.wst = 1; const k = I.migrate(s2);
          return { af: [0, 1, 2, 3, 4, 5].every(i => Math.abs(m.tracks[0].p[P.A + i] - d[2 + i]) < 1e-9), sides: sd.every((v, i) => Math.abs(m.tracks[0].p[SQ(i)] - v) < 1e-9),
            lockA: P.A in m.tracks[0].steps[5].l, lockS: Math.abs(m.tracks[2].steps[5].l[P.S5] - sd[4]) < 1e-9, kept: k.tracks[0].p[P.A], keptL: k.tracks[2].steps[5].l[P.S5],
            wst: m.wst, saved: JSON.parse(I.serialState()).wst }; });
        // v78: a v77 project (203 params, no "v78"): WEST's new places at its own, not at rest; a step locked onto WEST
        // from another machine gets them in its lock (CHA: gate 1 heard); a new one's pad ECRV at EXP; marked when saved
        ws.mig78 = await page.evaluate(() => { const I = window.__impakt, P = I.P, SQ = q => q < 35 ? P.S1 + q : P.S36 + q - 35;
          const s = I.demoState(); s.wst = 1; delete s.v78;
          for (const t of s.tracks) t.p = t.p.slice(0, 203);
          s.tracks[0].p[P.MACH] = 41.5 / 64; s.tracks[3].p[P.MACH] = 0.5 / 64;
          s.tracks[3].steps[2] = Object.assign({}, s.tracks[3].steps[2], { on: 1, l: { [P.MACH]: 41.5 / 64 } });
          const m = I.migrate(JSON.parse(JSON.stringify(s))), sd = I.MACH[41].sd;
          return { np: m.tracks.map(t => t.p.length).join(','), w: [35, 51, 52, 58].every(q => Math.abs(m.tracks[0].p[SQ(q)] - sd[q]) < 1e-9),
            other: m.tracks[3].p[SQ(58)], lock: m.tracks[3].steps[2].l[SQ(58)], ecrv: m.tracks[5].p[P.ECRV], v78: m.v78, saved: JSON.parse(I.serialState()).v78 }; });
        // the pulser at 1/64, a step on +7: a pad held (the pattern plays on: each of the track's trigs starts the pulser
        // over, the most pulses seen between two of them); on COLOUR, the ring where the envelope takes TIMBRE; the
        // MOTION tab showing the stage, the SHAPE tab the envelope's level
        await tab(1);
        await setP('E', 1.5 / 11); await setP('S23', 0.5 + 7 / 24); await setP('S27', 1.5 / 5);
        const b = page.locator('.pad').nth(0);
        await b.dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true });
        ws.info = null; ws.ring = false; ws.num = ''; ws.lv = 0;
        for (let k = 0; k < 14; k++) {
          await page.waitForTimeout(50);
          const i = await page.evaluate(() => { const s = window.__impakt.ui.west; return s ? { on: s[0], y: Math.round(s[1] * 1000) / 1000, np: s[5], left: s[12], iv: Math.round(s[13] * 1000), lvl: s[9], n: s.length } : null; });
          if (i && (!ws.info || i.np > ws.info.np)) ws.info = i;
          const r = await page.evaluate(() => { const k = document.querySelector('#wmain .knob'); const t = document.querySelectorAll('#wtabs .wtab');
            return { ring: +(k.querySelector('.ghd').getAttribute('r')) > 0 && !!k.querySelector('.gho').getAttribute('d'), num: t[5].querySelector('.wnum').textContent,
              lv: parseFloat((t[2].querySelector('.wlv').style.transform.match(/[\d.]+/) || ['0'])[0]) }; });
          ws.ring = ws.ring || r.ring; ws.num = ws.num || r.num; ws.lv = Math.max(ws.lv, r.lv);
        }
        ws.litOn = await lit();
        await page.screenshot({ path: `${shotDir}/impakt-${label}-west.png`, fullPage: true });
        await b.dispatchEvent('pointerup', { pointerId: 1, isPrimary: true }); await page.waitForTimeout(60);
        await setP('E', 0.5 / 11); await setP('S23', 0.5); await setP('S27', 0.9);
        ok('WEST (v79): its own panel, seven pages by what you hear as tabs, in place of the eight knobs (those in CONTROL ALL)',
          ws.panel.mk && !ws.panel.wp && ws.panel.tabs === 'PITCH,COLOUR,SHAPE,LAYER,RHYTHM,MOTION,SPACE' && ws.panel.role === 'tablist' && ws.pitch.sel === 'PITCH' && /^Which note/.test(ws.pitch.say)
          && !ws.all.mk && ws.all.wp && ws.all.l === 'TUNE,DECAY,TIMBRE,SYMMETRY,INDEX,RATIO,PULSER,PAGE', JSON.stringify({ panel: ws.panel, all: ws.all }));
        ok('WEST (v79): every page\'s two rows, named in words, their readings; a CV\'s source jack beside its depth (in the source\'s colour); sliders for the envelope and the stages; drawn switches; the stages\' readings; defaults as the engine\'s',
          ws.pitch.main === 'TUNE,RATIO,GLIDE,KEYS' && ws.pitch.fine === 'PITCH CV,DEPTH,FREQ CV,DEPTH,DRIFT,MOD FINE,MOD TRACK' && ws.pitch.tune === '0.0' && ws.pitch.ratio === '√2'
          && ws.pitch.keys.opts === 'FOLLOW,FIXED' && ws.pitch.keys.on === 'FOLLOW' && ws.pitch.psrc.src === 'SEQ' && ws.pitch.psrc.dots === 8 && /pairl/.test(ws.pitch.psrc.cls) && /tint/.test(ws.pitch.pamt.cls) && /pairr/.test(ws.pitch.pamt.cls)
          && ws.rate.l.split(',')[5] === 'MOD RATE' && ws.rate.v === '14Hz'
          && ws.colour.main === 'TIMBRE,SYMMETRY,INDEX,BLEND' && ws.colour.fine === 'FOLDER,WARMTH,OTHER WAVE,MOD WAVE,MOD DOES,VELOCITY,FROM MOD,RANDOM,TIMBRE CV,DEPTH,INDEX CV,DEPTH'
          && ws.colour.timb === '15%' && ws.colour.index === '20%' && ws.colour.other.opts === 'TRI,SQR,SPIKE' && ws.colour.other.icons === 3 && ws.colour.tsrc.src === 'ENV' && ws.colour.tamt.arc === '#E9472B' && ws.colour.tamtV === '+ 30'
          && ws.shape.main === 'ATTACK,SUSTAIN,DECAY,CURVE' && ws.shape.fine === 'MODE,FIRED BY,GATE 1,LEVEL 1,RESONANCE,OUT 1,GATE 1 CV,DEPTH' && /isfd/.test(ws.shape.atk.cls) && /isfd/.test(ws.shape.dec.cls)
          && !/isfd/.test(ws.shape.curv.cls) && ws.shape.decV === '91ms' && ws.shape.mode.opts === 'ONCE,HOLD,LOOP,OFF' && ws.shape.mode.icons === 4
          && ws.layer.main === 'OUT 2,INPUT,GATE 2,LEVEL 2' && ws.layer.fine === 'GATE 2 CV,DEPTH' && ws.layer.inp.opts === 'MOD OSC,LOOP,GATE 1' && ws.layer.out2 === 'OFF'
          && ws.rhythm.main === 'PULSER,COUNT,BOUNCE,ACCENT' && ws.rhythm.fine === 'START,SWING,CHANCE,JITTER,RATE CV,DEPTH' && ws.rhythm.puls === 'OFF' && ws.rhythm.cnt === '∞' && ws.rhythm.start.on === 'TRIG'
          && ws.motion.main === '1,2,3,4,5,GOES TO' && ws.motion.fine === 'LENGTH,CLOCK,RANDOM ON,INVERTS' && /isfd/.test(ws.motion.s1.cls) && /stg/.test(ws.motion.s1.cls) && ws.motion.v1 === '0'
          && ws.motion.dest.opts === 'PITCH,TIMBRE,INDEX,LEVEL' && ws.motion.v2st === '+6st' && ws.motion.v2tb === '+50' && ws.motion.spl[0] === 1 && ws.motion.spl[1] && ws.motion.spl0 === 0
          && ws.space.main === 'MIX,SIZE,DECAY,SHAKE' && ws.space.fine === 'REFLECT,SPIN,DAMPING,CHAOS' && ws.space.mix === '15%' && /s$/.test(ws.space.dcy)
          && ws.hint === 'FOLDS THE WAVE OVER ITSELF: MORE = BRIGHTER' && ws.defs, JSON.stringify(ws));
        ok('WEST: a v75 project\'s SPHERE becomes WEST at its defaults (its locks gone, a step locked onto it with WEST\'s sides), a v76 one stays as it is, the page marks what it saves',
          ws.mig.af && ws.mig.sides && !ws.mig.lockA && ws.mig.lockS && ws.mig.kept === 0.97 && ws.mig.keptL === 0.9 && ws.mig.wst === 1 && ws.mig.saved === 1, JSON.stringify(ws.mig));
        ok('WEST (v78): a v77 project\'s tracks padded to 284 (v90), WEST\'s new places at its own, a step locked onto it from another machine with them, ECRV at EXP, marked',
          ws.mig78.np === '284,284,284,284,284,284,284,284' && ws.mig78.w && ws.mig78.other === 0 && Math.abs(ws.mig78.lock - Math.SQRT1_2) < 1e-6 && ws.mig78.ecrv === 1
          && ws.mig78.v78 === 1 && ws.mig78.saved === 1, JSON.stringify(ws.mig78));
        ok('WEST: a pad held, the pulser at 1/64 pings it on the tempo (the light, the pulses); every page drawn on the screen; the ring where the envelope takes TIMBRE, the stage on MOTION\'s tab, the envelope\'s level on SHAPE\'s',
          ws.info && ws.info.on === 1 && ws.info.np >= 2 && ws.info.left === -1 && ws.info.iv > 15 && ws.info.iv < 60 && ws.info.lvl > 0.005 && ws.info.n === 24
          && ws.lit.length === 7 && ws.lit.every(n => n > 200) && ws.litOn > 300 && ws.ring && /^[1-5]$/.test(ws.num) && ws.lv > 0.05, JSON.stringify({ info: ws.info, lit: ws.lit, litOn: ws.litOn, ring: ws.ring, num: ws.num, lv: ws.lv }));
      }
      // v81: HOWL (N°44, CHORDS + VOICE): CHORD (ROOT SUST CHRD VOIC DROP FDBK MRPH SIDE), STRING, AMP; defaults as the
      // engine's; a pad held: its strings on the screen, its chord's root; DRONE on AMP (— on a machine that cannot hold
      // one), the pad's ∞; WEFT's katakana (missing until v80)
      {
        const hw = {};
        const page3h = async i => { await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(i).click(); await page.waitForTimeout(80); };
        await page.click('#mid'); await page.waitForTimeout(100);
        hw.pick = await page.evaluate(() => { const b = document.querySelector('#mpick button[data-m="43"]'); return b ? [b.closest('.pgrp').querySelector('.pgl').textContent, b.querySelector('.pn').textContent, b.querySelector('.pid').textContent, b.querySelector('.pk').textContent].join('|') : null; });
        hw.weftKana = await page.evaluate(() => document.querySelector('#mpick button[data-m="42"] .pk').textContent);
        await page.locator('#mpick button[data-m="43"]').click(); await page.waitForTimeout(80);
        hw.labels = await kL8(); hw.root = await kV(0); hw.sust = await kV(1); hw.chrd = await kV(2); hw.voic = await kV(3); hw.drop = await swOf(4); hw.fdbk = await kV(5); hw.mrph = await kV(6); hw.side = await swOf(7);
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.DEC, 1); I.setTrackParam(I.P.E, 0); I.setTrackParam(I.P.A, 14.5 / 15); I.renderAll(); });
        hw.inf = await kV(1); hw.now = await kV(6); hw.poly = await kV(2);
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.DEC, 0.55); I.setTrackParam(I.P.E, 0.64); I.setTrackParam(I.P.A, 0.5 / 15); I.renderAll(); });
        await page3h(1); hw.str = await kL8(); hw.exct = await kV(0); hw.hum = await kV(3); hw.blom = await kV(4);
        await page3h(2); hw.amp = await kL8(); hw.fuzz = await kV(1); hw.levl = await kV(6);
        await page3h(3); hw.loop = await kL8(); hw.time = await kV(0); hw.colr = await kV(1); hw.focs = await kV(2); hw.pkup = await kV(3); hw.limt = await kV(6);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-howl-loop.png`, fullPage: true });
        // (v83: CAB, BOW)
        await page3h(4); hw.cab = await kL8(); hw.tght = await kV(0); hw.pres = await kV(2); hw.cabv = await kV(4); hw.size = await kV(5); hw.mic = await kV(6);
        await page3h(5); hw.bow = await kL8(); hw.bowv = await kV(0); hw.swel = await kV(1); hw.strg = await kV(2); hw.bmod = await swOf(3); hw.pos = await kV(4);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-howl-bow.png`, fullPage: true });
        await page3h(0);
        const pgd = await page.evaluate(() => { const I = window.__impakt, m = I.MACH[43]; return { d: m.d.slice(2), sd: m.sd }; });
        // v82: a v81 project (no "v82"): HOWL's DIST onto LOOP's TIME (the same distance), no hum, LOOP at its defaults, SIDE
        // on four pages; on its steps; a step of another track locked onto HOWL gets LOOP's places
        hw.mig = await page.evaluate(() => {
          const I = window.__impakt, P = I.P, SQ = q => q < 35 ? P.S1 + q : P.S36 + q - 35, M = I.MACH[43];
          const s = JSON.parse(JSON.stringify(Object.assign({}, I.state, { pats: [], chain: null })));
          delete s.v82; delete s.v83;
          const t = s.tracks[2]; t.p[P.MACH] = 43.5 / 64; t.p[SQ(3)] = 0.4; t.p[P.F] = 1.5 / 3; for (let q = 14; q < 21; q++) t.p[SQ(q)] = 0;
          t.steps[4] = { on: 1, l: { [SQ(3)]: 0.2, [P.F]: 2.5 / 3 } };
          const u = s.tracks[3]; u.p[P.MACH] = 0.5 / 64; u.steps[2] = { on: 1, l: { [P.MACH]: 43.5 / 64, [SQ(3)]: 0.4 } };
          const r = I.migrate(s), a = r.tracks[2], b = r.tracks[3].steps[2].l, l = a.steps[4].l;
          const tm = d => Math.log2((0.002 * Math.pow(40, d)) / 0.0003) / Math.log2(80 / 0.3);
          return { time: Math.abs(a.p[SQ(14)] - tm(0.4)) < 1e-6, hum: a.p[SQ(3)], loop: [15, 16, 17, 18, 19, 20].every(q => Math.abs(a.p[SQ(q)] - M.sd[q]) < 1e-9),
            side: Math.abs(a.p[P.F] - 1.5 / 6) < 1e-9, lockT: Math.abs(l[SQ(14)] - tm(0.2)) < 1e-6, lockD: SQ(3) in l, lockF: Math.abs(l[P.F] - 2.5 / 6) < 1e-9,
            onto: Math.abs(b[SQ(14)] - tm(0.4)) < 1e-6 && [15, 16, 17, 18, 19, 20].every(q => Math.abs(b[SQ(q)] - M.sd[q]) < 1e-9), mark: r.v82 };
        });
        // v83: a v82 project (no "v83"): SIDE on four pages onto six (LOOP stays LOOP), CAB and BOW at HOWL's defaults (the
        // amp as it was, the bow off); its locks too; a step of another track locked onto HOWL gets CAB's and BOW's places
        hw.mig83 = await page.evaluate(() => {
          const I = window.__impakt, P = I.P, SQ = q => q < 35 ? P.S1 + q : P.S36 + q - 35, M = I.MACH[43];
          const s = JSON.parse(JSON.stringify(Object.assign({}, I.state, { pats: [], chain: null })));
          delete s.v83; s.v82 = 1;
          const t = s.tracks[2]; t.p[P.MACH] = 43.5 / 64; t.p[P.F] = 3.5 / 4; for (let q = 21; q < 35; q++) t.p[SQ(q)] = 0;
          t.steps[4] = { on: 1, l: { [P.F]: 2.5 / 4 } };
          const u = s.tracks[3]; u.p[P.MACH] = 0.5 / 64; u.steps[2] = { on: 1, l: { [P.MACH]: 43.5 / 64 } };
          const r = I.migrate(s), a = r.tracks[2], b = r.tracks[3].steps[2].l, l = a.steps[4].l, cb = [];
          for (let q = 21; q < 35; q++) cb.push(q);
          return { side: Math.abs(a.p[P.F] - 3.5 / 6) < 1e-9, cab: cb.every(q => Math.abs(a.p[SQ(q)] - M.sd[q]) < 1e-9), lockF: Math.abs(l[P.F] - 2.5 / 6) < 1e-9,
            onto: cb.every(q => Math.abs(b[SQ(q)] - M.sd[q]) < 1e-9), mark: r.v83 };
        });
        const wxh = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(path.resolve(__dirname, '../web/impakt.wasm'))), {}).exports;
        hw.defs = pgd.d.every((v, i) => Math.abs(wxh.sk_af_def(43, i) - v) < 1e-4) && pgd.sd.length === 35 && pgd.sd.every((v, i) => Math.abs(wxh.sk_side_def(43, i) - v) < 1e-4) && wxh.sk_side_def(43, 35) === 0;
        // a pad held (stopped): six strings on the screen, its root as TUNE and DROP say (the track quantised: on the scale)
        // (the transport stopped for it: running, a step of the track with a TUNE lock re-voices the chord under the
        // held pad, and the root on the screen is that step's)
        const tH = await page.evaluate(() => window.__impakt.ui.t), howlRun = await page.evaluate(() => !!window.__impakt.ui.pos.run);
        if (howlRun) { await page.click('#play'); await page.waitForTimeout(300); }
        await page.locator('.pad').nth(tH).dispatchEvent('pointerdown', { pointerId: 9, isPrimary: true }); await page.waitForTimeout(900);
        hw.scr = await page.evaluate(() => { const hi = window.__impakt.ui.halo, cv = document.getElementById('ovw'); let lit = 0;
          if (cv) { const d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data; for (let i = 3; i < d.length; i += 4) if (d[i]) lit++; }
          const I = window.__impakt, tp = I.state.tracks[I.ui.t].p;
          return { kind: hi ? hi[0] : -1, n: hi ? hi[1] : 0, root: hi ? Math.round(hi[8]) : 0, lit, why: { play: I.ui.playing, run: I.ui.pos.run, rt: [0, 1, 2, 3].map(m => I.modRoutes(I.ui.t, m).map(r => r.p + ':' + r.d.toFixed(2)).join()).join('|'), tune: tp[I.P.TUNE], note: tp[I.P.NOTE], jump: tp[I.P.JUMP], lock: I.ui.lock, qsrc: Math.floor((I.state.g[I.G.QSRC] || 0) * 9) - 1, mach: I.state.tracks.map(t => Math.floor(t.p[I.P.MACH] * 64)).join(), q: I.state.tracks[I.ui.t].q, hi: hi ? Array.from(hi).slice(2, 11).map(x => Math.round(x * 10) / 10) : null } }; });
        await page.screenshot({ path: `${shotDir}/impakt-${label}-howl.png`, fullPage: true });
        { const m = /^([A-G]#?)(-?\d)$/.exec(hw.root); hw.rootN = m ? ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'].indexOf(m[1]) + 12 * (+m[2] + 1) : -1; }
        await page.locator('.pad').nth(tH).dispatchEvent('pointerup', { pointerId: 9, isPrimary: true }); await page.waitForTimeout(60);
        if (howlRun) { await page.click('#play'); await page.waitForTimeout(200); }
        // DRONE: AMP's last switch; on, the pad shows ∞ (and says so); on CP·NZ it has nothing to hold: —
        await openMod('AMP');
        hw.drn = await swOf(7, '#k-AMP');
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.DRON, 1); I.renderAll(); });
        hw.pad = await page.evaluate(t => { const b = document.querySelectorAll('.pad')[t]; return { drn: b.classList.contains('drn'), aria: b.getAttribute('aria-label'), sign: getComputedStyle(b, '::after').content }; }, tH);
        await seqPage(); await page.click('#mid'); await page.waitForTimeout(80);
        await page.locator('#mpick button[data-m="5"]').click(); await page.waitForTimeout(80);
        await openMod('AMP');
        hw.cpnz = await page.evaluate(() => { const k = document.querySelectorAll('#k-AMP .knob')[7]; return k.querySelector('.lt').textContent + '=' + (k.querySelector('.kv') || {}).textContent + (k.classList.contains('off') ? ':off' : ''); });
        hw.padCp = await page.evaluate(t => document.querySelectorAll('.pad')[t].classList.contains('drn'), tH);
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.DRON, 0); I.renderAll(); });
        await seqPage();
        ok('HOWL (v81): N°44 in CHORDS + VOICE; CHORD (ROOT SUST CHRD VOIC DROP FDBK MRPH), STRING (EXCT DAMP RESO HUM BLOM DRFT WAMY), AMP (DRIV FUZZ TONE BASS MID SAG LEVL), v82 LOOP (TIME COLR FOCS PKUP NOIS MOVE LIMT); defaults as the engine\'s; WEFT\'s katakana',
          hw.pick === 'CHORDS + VOICE|N°44|HOWL|ハウル' && hw.weftKana === 'ウェフト' && hw.labels === 'ROOT,SUST,CHRD,VOIC,DROP,FDBK,MRPH,SIDE' && /^[A-G]#?[0-9]$/.test(hw.root)
          && hw.sust === '4.4s' && hw.chrd === '5TH' && hw.voic === 'DRONE' && hw.drop.sw && hw.drop.opts === '0,SUB,−12,−12S,−24' && hw.drop.on === '−12' && hw.fdbk === '60%' && hw.mrph === '2.2s'
          && hw.side.opts === 'CHORD,STRING,AMP,LOOP,CAB,BOW' && hw.side.on === 'CHORD' && hw.inf === '∞' && hw.now === 'NOW' && hw.poly === 'POLY'
          && hw.str === 'EXCT,DAMP,RESO,HUM,BLOM,DRFT,WAMY,SIDE' && hw.exct === '55%' && hw.hum === 'OFF' && /ms$|s$/.test(hw.blom)
          && hw.loop === 'TIME,COLR,FOCS,PKUP,NOIS,MOVE,LIMT,SIDE' && /^8\.[5-9]\dms$/.test(hw.time) && /^8\d\d$/.test(hw.colr) && hw.focs === '45%' && hw.pkup === 'OFF' && hw.limt === '43%'
          && hw.amp === 'DRIV,FUZZ,TONE,BASS,MID,SAG,LEVL,SIDE' && hw.fuzz === '50%' && hw.levl === '0dB' && hw.defs, JSON.stringify(hw));
        ok('HOWL (v83): CAB (TGHT BITE PRES DPTH CAB SIZE MIC), BOW (BOW SWEL STRG MODE POS WOBL HAND), at their defaults (the amp as it was, the bow off); a v82 project: SIDE on six pages, CAB and BOW at their defaults, its locks, a step locked onto HOWL',
          hw.cab === 'TGHT,BITE,PRES,DPTH,CAB,SIZE,MIC,SIDE' && hw.tght === 'OFF' && hw.pres === 'FLAT' && hw.cabv === 'MID' && hw.size === '4x12' && hw.mic === 'MID'
          && hw.bow === 'BOW,SWEL,STRG,MODE,POS,WOBL,HAND,SIDE' && hw.bowv === 'OFF' && hw.swel === '260ms' && hw.strg === '1' && hw.bmod.opts === 'FUND,OCT,12TH' && hw.bmod.on === 'FUND' && hw.pos === '50%'
          && hw.mig83.side && hw.mig83.cab && hw.mig83.lockF && hw.mig83.onto && hw.mig83.mark === 1, JSON.stringify({ cab: hw.cab, bow: hw.bow, tght: hw.tght, pres: hw.pres, cabv: hw.cabv, size: hw.size, mic: hw.mic, bowv: hw.bowv, swel: hw.swel, strg: hw.strg, bmod: hw.bmod, pos: hw.pos, mig83: hw.mig83 }));
        ok('HOWL (v82): a v81 project, DIST onto LOOP\'s TIME (the same distance), no hum, LOOP at its defaults, SIDE on four pages; its locks too; a step locked onto HOWL with LOOP',
          hw.mig.time && hw.mig.hum === 0 && hw.mig.loop && hw.mig.side && hw.mig.lockT && !hw.mig.lockD && hw.mig.lockF && hw.mig.onto && hw.mig.mark === 1, JSON.stringify(hw.mig));
        ok('HOWL: a pad held plays its six strings (the screen), on the root its ROOT knob shows; DRONE on AMP, the pad\'s ∞; on CP·NZ DRONE has nothing to hold (—, no ∞)',
          hw.scr.kind === 4 && hw.scr.n === 6 && hw.scr.root === hw.rootN && hw.scr.lit > 400
          && hw.drn.sw && hw.drn.opts === 'OFF,ON' && hw.pad.drn && /drone/.test(hw.pad.aria) && /∞/.test(hw.pad.sign) && /^DRONE=—/.test(hw.cpnz) && !hw.padCp, JSON.stringify({ scr: hw.scr, root: hw.root, rootN: hw.rootN, drn: hw.drn, pad: hw.pad, cpnz: hw.cpnz, padCp: hw.padCp }));
      }
      // v84: MANTRA (N°46, CHORDS + VOICE): PLAY (TUNE DEC TCH HLD1 HLD2 DFB DRIV SIDE: eight pages), V1-4, V5-8 (each
      // voice's TUNE as the note it sounds, SHARP, PITCH), FM1-4, FM5-8, LFO, DELAY, OUT; defaults as the engine's; its
      // presets; its screen: eight pads, a finger each (two at once), its pressure where it presses, lifted; a trig touches
      // the sensors TCH names
      {
        const mt = {};
        const pg = async i => { await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(i).click(); await page.waitForTimeout(80); };
        await page.click('#mid'); await page.waitForTimeout(100);
        mt.pick = await page.evaluate(() => { const b = document.querySelector('#mpick button[data-m="45"]'); return b ? [b.closest('.pgrp').querySelector('.pgl').textContent, b.querySelector('.pn').textContent, b.querySelector('.pid').textContent, b.querySelector('.pk').textContent].join('|') : null; });
        await page.locator('#mpick button[data-m="45"]').click(); await page.waitForTimeout(100);
        mt.labels = await kL8(); mt.dec = await kV(1); mt.tch = await kV(2); mt.hld1 = await kV(3); mt.dfb = await kV(5); mt.driv = await kV(6); mt.side = await swOf(7);
        mt.pre = await page.evaluate(() => window.__impakt.presetEntries(45).length);
        await pg(1); mt.v14 = await kL8(); mt.tun1 = await kV(0); mt.tun2 = await kV(1); mt.pch1 = await kV(6);
        await pg(2); mt.v58 = await kL8(); mt.tun7 = await kV(2);
        await pg(3); mt.fm14 = await kL8(); mt.src12 = await swOf(1); mt.fst12 = await swOf(2); mt.loop = await swOf(6);
        await pg(4); mt.fm58 = await kL8(); mt.totl = await swOf(6);
        await pg(5); mt.lfo = await kL8(); mt.frqa = await kV(0); mt.frqb = await kV(1); mt.lgic = await swOf(2);
        await pg(6); mt.dly = await kL8(); mt.tim1 = await kV(0); mt.tim2 = await kV(1); mt.dsrc = await swOf(4);
        await pg(7); mt.out = await kL8(); mt.vol = await kV(1); mt.bus = await kV(2);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-mantra-out.png`, fullPage: true });
        await pg(0);
        const mdf = await page.evaluate(() => { const m = window.__impakt.MACH[45]; return { d: m.d.slice(2), sd: m.sd }; });
        const wxm = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(path.resolve(__dirname, '../web/impakt.wasm'))), {}).exports;
        mt.defs = mdf.d.every((v, i) => Math.abs(wxm.sk_af_def(45, i) - v) < 1e-4) && mdf.sd.length === 49 && mdf.sd.every((v, i) => Math.abs(wxm.sk_side_def(45, i) - v) < 1e-4) && wxm.sk_side_def(45, 49) === 0;
        // its pads (the transport stopped: no trig of the pattern touches them): a finger on the third (high on it: hard),
        // then a second on the sixth (low: light); lifted one by one
        mt.wasRunning = await page.evaluate(() => window.__impakt.ui.pos.run);
        if (mt.wasRunning) { await page.click('#play'); await page.waitForTimeout(300); }
        await page.locator('#ovw').scrollIntoViewIfNeeded(); await page.waitForTimeout(60);
        const at = (k, fy) => page.evaluate(([k, fy]) => { const cv = document.getElementById('ovw'), r = cv.getBoundingClientRect(), w = r.width, x0 = 1, x1 = w - 18;
          const pw = (x1 - x0 - 4 * 3 - 2 * 7 - 13) / 8; let x = x0; for (let j = 0; j < k; j++) x += pw + (j === 3 ? 13 : j & 1 ? 7 : 3);
          return { x: r.left + x + pw / 2, y: r.top + 17 + fy * (r.height - 27) }; }, [k, fy]);
        const pt = async (type, id, p) => page.locator('#ovw').dispatchEvent(type, { pointerId: id, isPrimary: id === 31, clientX: p.x, clientY: p.y, bubbles: true });
        const hiOf = () => page.evaluate(() => { const hi = window.__impakt.ui.halo, cv = document.getElementById('ovw'); let lit = 0;
          if (cv) { const d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data; for (let i = 3; i < d.length; i += 4) if (d[i]) lit++; }
          return hi ? { kind: hi[0], n: hi[1], pr: Array.from(hi).slice(18, 26).map(x => +x.toFixed(2)), env: Array.from(hi).slice(2, 10).map(x => +x.toFixed(2)), lit } : null; });
        const a3 = await at(2, 0.15), a6 = await at(5, 0.85);
        await pt('pointerdown', 31, a3); await page.waitForTimeout(500);
        mt.one = await hiOf();
        await pt('pointerdown', 32, a6); await page.waitForTimeout(500);
        mt.two = await hiOf();
        await page.screenshot({ path: `${shotDir}/impakt-${label}-mantra.png`, fullPage: true });
        await pt('pointerup', 31, a3); await page.waitForTimeout(300);
        mt.lift1 = await hiOf();
        await pt('pointerup', 32, a6); await page.waitForTimeout(300);
        mt.lift2 = await hiOf();
        // a trig (the track's pad) with TCH 1234: voices 1 .. 4 touched, the others not
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.A, 12.5 / 15); I.renderAll(); });
        mt.tch1234 = await kV(2);
        const tM = await page.evaluate(() => window.__impakt.ui.t);
        await page.locator('.pad').nth(tM).dispatchEvent('pointerdown', { pointerId: 9, isPrimary: true }); await page.waitForTimeout(500);
        mt.trig = await hiOf();
        await page.locator('.pad').nth(tM).dispatchEvent('pointerup', { pointerId: 9, isPrimary: true }); await page.waitForTimeout(200);
        mt.trigUp = await hiOf();
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.A, 14.5 / 15); I.renderAll(); });
        if (mt.wasRunning) { await page.click('#play'); await page.waitForTimeout(200); }
        ok('MANTRA (v84): N°46 in CHORDS + VOICE; PLAY (TUNE DEC TCH HLD1 HLD2 DFB DRIV, SIDE on eight pages), V1-4 V5-8 (the notes their TUNEs sound), FM1-4 FM5-8, LFO, DELAY, OUT; defaults as the engine\'s; 18 presets',
          mt.pick === 'CHORDS + VOICE|N°46|MANTRA|マントラ' && mt.labels === 'TUNE,DEC,TCH,HLD1,HLD2,DFB,DRIV,SIDE' && /^(9\.\d|10\.0)s$/.test(mt.dec) && mt.tch === 'ALL' && mt.hld1 === 'OFF'
          && mt.dfb === '30%' && mt.driv === '20%' && mt.side.opts === 'PLAY,V1-4,V5-8,FM1-4,FM5-8,LFO,DELAY,OUT' && mt.side.on === 'PLAY' && mt.pre === 19
          && mt.v14 === 'TUN1,TUN2,TUN3,TUN4,SHP12,SHP34,PCH1,SIDE' && mt.tun1 === 'A1' && mt.tun2 === 'A1+13' && mt.pch1 === '0'
          && mt.v58 === 'TUN5,TUN6,TUN7,TUN8,SHP56,SHP78,PCH2,SIDE' && mt.tun7 === 'C#4'
          && mt.fm14 === 'MOD12,SRC12,FST12,MOD34,SRC34,FST34,LOOP,SIDE' && mt.src12.opts === 'OFF,FM,LFO' && mt.src12.on === 'OFF' && mt.fst12.opts === 'SLOW,FAST' && mt.loop.opts === '2 LOOP,1 LOOP'
          && mt.fm58 === 'MOD56,SRC56,FST56,MOD78,SRC78,FST78,TOTL,SIDE' && mt.totl.on === 'OFF'
          && mt.lfo === 'FRQA,FRQB,LGIC,LINK,VIB,VDPT,SAG,SIDE' && mt.frqa === '0.75Hz' && mt.frqb === '3.0Hz' && mt.lgic.opts === 'OR,AND'
          && mt.dly === 'TIM1,TIM2,DMD1,DMD2,DSRC,DWAV,DMIX,SIDE' && mt.tim1 === '120ms' && mt.dsrc.opts === 'LFO,SELF'
          && mt.out === 'DSTM,VOL,BUS,BLED,TOL,DRFT,NOIS,SIDE' && mt.vol === '70%' && mt.bus === '45%' && mt.defs, JSON.stringify(mt));
        ok('MANTRA: its screen\'s pads: a finger wakes the third (hard, high on it), a second the sixth (light, low on it), each lifted on its own; a trig with TCH 1234 touches voices 1 .. 4 only',
          mt.one && mt.one.kind === 5 && mt.one.n === 8 && mt.one.pr[2] > 0.7 && mt.one.pr.filter((x, i) => i !== 2).every(x => x === 0) && mt.one.lit > 1500
          && mt.two.pr[2] > 0.7 && mt.two.pr[5] > 0 && mt.two.pr[5] < 0.4 && mt.two.env[2] > 0.5
          && mt.lift1.pr[2] === 0 && mt.lift1.pr[5] > 0 && mt.lift2.pr.every(x => x === 0)
          && mt.tch1234 === '1234' && mt.trig.pr.slice(0, 4).every(x => x > 0) && mt.trig.pr.slice(4).every(x => x === 0) && mt.trigUp.pr.every(x => x === 0),
          JSON.stringify({ one: mt.one, two: mt.two, lift1: mt.lift1, lift2: mt.lift2, trig: mt.trig, trigUp: mt.trigUp }));
      }
      // v87: RADIO (N°47, LOOP + TEXTURE): PLAY by MODE (LIVE: SHFT, GATE, SQL; LOOP: TUNE, LEN, GRAB; DIG: DPTH), AIR;
      // defaults as the engine's; its presets; the TUNE panel (four receivers; the public list: the plugin's); the engine
      // of the page decodes an MP3 stream for a receiver and keeps it in hand (WebAssembly, no libc)
      {
        const rd = {};
        await page.click('#mid'); await page.waitForTimeout(100);
        rd.pick = await page.evaluate(() => { const b = document.querySelector('#mpick button[data-m="46"]'); return b ? [b.closest('.pgrp').querySelector('.pgl').textContent, b.querySelector('.pn').textContent, b.querySelector('.pid').textContent, b.querySelector('.pk').textContent].join('|') : null; });
        await page.locator('#mpick button[data-m="46"]').click(); await page.waitForTimeout(100);
        rd.live = await kL8(); rd.dec = await kV(1); rd.rx = await swOf(2); rd.mode = await swOf(3); rd.gate = await kV(4); rd.sql = await kV(5); rd.side = await swOf(7);
        const setP = (p, v) => page.evaluate(([p, v]) => { const I = window.__impakt; I.setTrackParam(I.P[p], v); I.renderAll(); }, [p, v]);
        await setP('B', 1.5 / 3); rd.loop = await kL8(); rd.grab = await kV(5);
        await setP('B', 2.5 / 3); rd.dig = await kL8(); rd.dpth = await kV(5);
        await setP('B', 0.5 / 3); await setP('F', 0.75); rd.air = await kL8(); rd.band = await kV(0); rd.cntr = await kV(1);
        await setP('F', 0.25);
        rd.pre = await page.evaluate(() => window.__impakt.presetEntries(46).length);
        const rsd = await page.evaluate(() => window.__impakt.MACH[46].sd);
        const wxr = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(path.resolve(__dirname, '../web/impakt.wasm'))), {}).exports;
        rd.defs = rsd.length === 7 && rsd.every((v, i) => Math.abs(wxr.sk_side_def(46, i) - v) < 1e-4) && wxr.sk_side_def(46, 7) === 0;
        // the panel: four receivers; SHORTWAVE in the page: the list only in the plugin
        rd.tune = await page.evaluate(() => !document.getElementById('mtune').hidden);
        await page.click('#mtune'); await page.waitForTimeout(100);
        rd.tabs = await page.evaluate(() => document.querySelectorAll('#rxtabs .rxtab').length);
        await page.click('#rkind button[data-k="sw"]'); await page.waitForTimeout(150);
        rd.swMsg = await page.evaluate(() => document.getElementById('rrlist').textContent);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-radio-panel.png`, fullPage: true });
        await page.click('#rclose');
        // the page's engine: an MP3 stream for receiver 1 (its state LIVE), decoded, about a second kept in hand
        wxr.sk_init(48000);
        wxr.sk_radio_state(0, 4, 0);
        const mp3 = fs.readFileSync(path.resolve(__dirname, 'tests/radio_1k.mp3'));
        const cap = wxr.sk_radio_cap();
        for (let off = 0; off < mp3.length; off += 4096) {
          const part = mp3.subarray(off, Math.min(mp3.length, off + 4096));
          new Uint8Array(wxr.memory.buffer, wxr.sk_radio_stage(), part.length).set(part);
          wxr.sk_radio_mp3(0, part.length);
        }
        rd.fmt = [wxr.sk_radio_fmt(0, 0), wxr.sk_radio_fmt(0, 1), wxr.sk_radio_fmt(0, 2)].join('/');
        for (let i = 0; i < 40; i++) wxr.sk_process(128);
        const rxi = new Float32Array(wxr.memory.buffer, wxr.sk_radio_rx(), 32);
        rd.engine = { state: rxi[0], fill: +rxi[3].toFixed(2), level: +rxi[2].toFixed(2), waiting: rxi[7] };
        ok('RADIO (v87): N°47 in LOOP + TEXTURE; PLAY by MODE (LIVE: SHFT GATE SQL; LOOP: TUNE LEN GRAB; DIG: DPTH), AIR (BAND CNTR FADE RATE HET CRKL DRIV); defaults as the engine\'s; 16 presets; TUNE opens four receivers',
          rd.pick === 'LOOP + TEXTURE|N°47|RADIO|ラジオ' && rd.live === 'SHFT,DEC,RX,MODE,GATE,SQL,STAT,SIDE' && rd.dec === 'HOLD' && rd.rx.opts === '1,2,3,4' && rd.rx.on === '1'
          && rd.mode.opts === 'LIVE,LOOP,DIG' && rd.mode.on === 'LIVE' && rd.gate === 'FREE' && rd.sql === 'OFF' && rd.side.opts === 'PLAY,AIR'
          && rd.loop === 'TUNE,DEC,RX,MODE,LEN,GRAB,STAT,SIDE' && rd.grab === 'KEEP' && rd.dig === 'TUNE,DEC,RX,MODE,LEN,DPTH,STAT,SIDE' && rd.dpth === '1.0s'
          && rd.air === 'BAND,CNTR,FADE,RATE,HET,CRKL,DRIV,SIDE' && rd.band === 'FULL' && rd.cntr === '949' && rd.pre === 17 && rd.defs
          && rd.tune && rd.tabs === 4 && /plugin/i.test(rd.swMsg), JSON.stringify(rd));
        ok('RADIO: the page\'s engine decodes an MP3 stream for a receiver (44.1 kHz stereo, 128 kbit/s) and keeps it in hand', rd.fmt === '44100/2/128' && rd.engine.state === 4 && rd.engine.fill > 0.5,
          JSON.stringify({ fmt: rd.fmt, engine: rd.engine }));
      }
      // HALO: its CHORD page (REL for DEC, the shapes, the progressions, SIDE a two-way switch), its VOICE page; it plays a
      // held chord, in the scale, shown on the screen with its name
      {
        await page.click('#mid'); await page.waitForTimeout(100);
        await page.locator('#mpick button[data-m="28"]').click(); await page.waitForTimeout(80);
        const hl = { labels: await page.evaluate(() => [...document.querySelectorAll('#mknobs .knob .lt')].map(e => e.textContent).join(',')),
          chrd: await kV(2), prog: await kV(3), wave: await kV(5), side: await swOf(7), rel: await kV(1) };
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(1).click(); await page.waitForTimeout(80);
        hl.voice = await page.evaluate(() => [...document.querySelectorAll('#mknobs .knob .lt')].map(e => e.textContent).join(','));
        hl.mode = await kV(2); hl.harm = await swOf(4);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-halo-voice.png`, fullPage: true });
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(2).click(); await page.waitForTimeout(80);
        hl.tone = await page.evaluate(() => [...document.querySelectorAll('#mknobs .knob .lt')].map(e => e.textContent).join(','));
        hl.cut = await kV(2); hl.soct = await swOf(5); hl.pres = await kV(6);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-halo-tone.png`, fullPage: true });
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(3).click(); await page.waitForTimeout(80);
        hl.age = await page.evaluate(() => [...document.querySelectorAll('#mknobs .knob .lt')].map(e => e.textContent).join(','));
        hl.dtun = await kV(0);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-halo-age.png`, fullPage: true });
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(0).click(); await page.waitForTimeout(80);
        hl.back = await kL();
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.B, (1 + 0.5) / 28); I.renderAll(); });
        const h0 = await page.evaluate(() => window.__impakt.ui.pos.hits[0]);
        await page.locator('.pad').nth(0).dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true });
        await page.waitForTimeout(700);
        hl.play = await page.evaluate(() => { const I = window.__impakt, hi = I.ui.halo, cv = document.getElementById('ovw');
          let lit = 0; if (cv) { const d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data; for (let i = 3; i < d.length; i += 4) if (d[i]) lit++; }
          return { info: hi ? hi.length : 0, on: hi ? hi[0] : 0, n: hi ? hi[1] : 0, notes: hi ? hi.slice(2, 2 + hi[1]) : [], hits: I.ui.pos.hits[0], lit,
            name: hi ? I.haloChordName(hi.slice(2, 2 + hi[1]), hi[8]) : '' }; });
        await page.screenshot({ path: `${shotDir}/impakt-${label}-halo.png`, fullPage: true });
        ok('HALO: CHORD page (REL, CHRD, PROG, SPRD, WAVE, BLOOM, SIDE), VOICE, TONE (v71: SUB, SOCT on AUTO NOTE −1 OCT and PRES since v72) and AGE (v71: VIB, WARM) pages, a held chord on its screen', hl.labels === 'TUNE,REL,CHRD,PROG,SPRD,WAVE,BLOOM,SIDE'
          && hl.chrd === '9TH' && hl.prog === 'OFF' && hl.side.sw && hl.side.opts === 'CHORD,VOICE,TONE,AGE' && hl.side.on === 'CHORD' && /s$/.test(hl.rel)
          && hl.age === 'DTUN,VARI,SHEN,DROP,VIB,WARM,—,SIDE' && hl.dtun === 'OFF'
          && hl.voice === 'SWEL,GLID,MODE,INV,HARM,DRFT,WIDE,SIDE' && hl.mode === 'KEY' && hl.harm.opts === 'FREE,PURE,LEAD' && hl.harm.on === 'PURE'
          && hl.tone === 'AIR,UNIS,CUT,OPEN,SUB,SOCT,PRES,SIDE' && hl.cut === 'OPEN' && hl.soct.sw && hl.soct.opts === 'AUTO,NOTE,−1 OCT' && hl.soct.on === 'AUTO'
          && hl.pres === 'PURE' && hl.back === 'CHRD,PROG,SPRD,WAVE,BLOOM,SIDE'
          && hl.play.info === 66 && hl.play.on === 1 && hl.play.n === 5 && hl.play.hits > h0 && hl.play.lit > 400 && /^[A-G]#? /.test(hl.play.name), JSON.stringify(hl));
      }
      // STRUM: its CHORD page (DEC, CHRD, PROG, SPRD, STRM, DIR a four-way switch, SIDE a three-way one), its PLAY and STRING
      // pages; a pad strums the chord's strings, shown on its screen
      {
        const lab = () => page.evaluate(() => [...document.querySelectorAll('#mknobs .knob .lt')].map(e => e.textContent).join(','));
        await page.click('#mid'); await page.waitForTimeout(100);
        await page.locator('#mpick button[data-m="35"]').click(); await page.waitForTimeout(80);
        const st = { labels: await lab(), side: await swOf(7), dir: await swOf(6), strm: await kV(5), prog: await kV(3) };
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(1).click(); await page.waitForTimeout(80);
        st.play = await lab();
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(2).click(); await page.waitForTimeout(80);
        st.str = await lab(); st.matl = await kV(0);
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(3).click(); await page.waitForTimeout(80);
        st.age = await lab(); st.air = await kV(0);
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(0).click(); await page.waitForTimeout(80);
        await page.locator('.pad').nth(0).dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true });
        await page.waitForTimeout(600);
        st.info = await page.evaluate(() => { const hi = window.__impakt.ui.halo, cv = document.getElementById('ovw'); let v = 0, lit = 0;
          if (hi) for (let s = 0; s < 18; s++) if (hi[12 + 3 * s] > -900 && hi[13 + 3 * s] > 0) v++;
          if (cv) { const d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data; for (let i = 3; i < d.length; i += 4) if (d[i]) lit++; }
          return { kind: hi ? hi[0] : 0, n: hi ? hi[1] : 0, voices: v, lit }; });
        await page.locator('.pad').nth(0).dispatchEvent('pointerup', { pointerId: 1, isPrimary: true });
        await page.screenshot({ path: `${shotDir}/impakt-${label}-strum.png`, fullPage: true });
        ok('STRUM: CHORD page (DEC, CHRD, PROG, SPRD, STRM, DIR, SIDE), PLAY, STRING and AGE pages; a pad strums its strings onto the screen',
          st.labels === 'TUNE,DEC,CHRD,PROG,SPRD,STRM,DIR,SIDE' && st.side.opts === 'CHORD,PLAY,STRING,AGE' && st.dir.opts === 'DOWN,UP,D/U,RAND' && st.dir.on === 'DOWN'
          && st.age === 'AIR,DTUN,VARI,SHEN,DROP,—,—,SIDE' && st.air === '30%'
          && st.play === 'MODE,INV,HARM,HUMN,ACCN,RING,WIDE,SIDE' && st.str === 'MATL,PICK,BRIT,BODY,MUTE,DBL,STIF,SIDE' && st.matl === 'NYLON'
          && st.info.kind === 2 && st.info.n >= 3 && st.info.voices >= 4 && st.info.lit > 300, JSON.stringify(st));
      }
      // CHOIR: its SING page (REL, VOWL, GLID, BRTH, VIBR, SING, SIDE), its VOICE and SPACE pages; its screen is a ribbon: a
      // finger on it sings where it is, follows it, lets go
      {
        const lab = () => page.evaluate(() => [...document.querySelectorAll('#mknobs .knob .lt')].map(e => e.textContent).join(','));
        await page.click('#mid'); await page.waitForTimeout(100);
        await page.locator('#mpick button[data-m="36"]').click(); await page.waitForTimeout(80);
        const ch = { labels: await lab(), vowel: await kV(2), side: await swOf(7), ribbon: await page.evaluate(() => document.getElementById('screen').classList.contains('ribbon')) };
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(1).click(); await page.waitForTimeout(80);
        ch.voice = await lab(); ch.part = await kV(4);
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(2).click(); await page.waitForTimeout(80);
        ch.space = await lab(); ch.shim = await swOf(5);
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(0).click(); await page.waitForTimeout(80);
        const box = await page.locator('#ovw').boundingBox();
        ch.box = box ? Math.round(box.width) + 'x' + Math.round(box.height) : 'none';
        // (the transport stopped: running, the track's own trigs would sing over the ribbon)
        ch.wasRunning = await page.evaluate(() => !!window.__impakt.ui.pos.run);
        if (ch.wasRunning) { await page.click('#play'); await page.waitForTimeout(300); }
        if (box) {
          await page.mouse.move(box.x + box.width * 0.75, box.y + box.height * 0.6); await page.mouse.down();
          await page.waitForTimeout(500);
          ch.sing = await page.evaluate(() => { const hi = window.__impakt.ui.halo; return hi ? { kind: hi[0], on: hi[1], midi: hi[2], rib: hi[11], env: hi[4] } : null; });
          await page.mouse.move(box.x + box.width * 0.25, box.y + box.height * 0.6, { steps: 8 });
          await page.waitForTimeout(400);
          ch.moved = await page.evaluate(() => { const hi = window.__impakt.ui.halo; return hi ? hi[2] : 0; });
          await page.screenshot({ path: `${shotDir}/impakt-${label}-choir.png`, fullPage: true });
          await page.mouse.up();
          await page.waitForTimeout(2600);
          ch.after = await page.evaluate(() => { const hi = window.__impakt.ui.halo; return hi ? { on: hi[1], rib: hi[11], env: hi[4] } : null; });
          ch.rel = await kV(1);
        }
        if (ch.wasRunning) { await page.click('#play'); await page.waitForTimeout(200); }
        ok('CHOIR: SING page, VOICE and SPACE pages; its screen a ribbon: it sings where it is touched, follows the finger, lets go',
          ch.labels === 'TUNE,REL,VOWL,GLID,BRTH,VIBR,SING,SIDE' && ch.vowel === 'AH' && ch.side.opts === 'SING,VOICE,SPACE' && ch.ribbon
          && ch.voice === 'SWEL,FORM,EFRT,VRAT,PART,SCOP,DRFT,SIDE' && ch.part === 'SOLO' && ch.space === 'MIX,SIZE,DCAY,DAMP,MOD,SHIM,PRE,SIDE' && ch.shim.opts.split(',').length === 4
          && ch.sing && ch.sing.kind === 3 && ch.sing.on === 1 && ch.sing.rib === 1 && Math.abs(ch.sing.midi - 66) < 1.5 && ch.moved < ch.sing.midi - 9
          && ch.after && ch.after.rib === 0 && (ch.after.on === 0 || ch.after.env < 0.1 * ch.sing.env), JSON.stringify(ch));     // (faded, or fading in a long REL)
      }
      // v59.1: the quantiser follows a STRUM's chords (a square a chord track, in a place of its own, grey without one):
      // its keys light the chord the engine plays, the scale's other notes dim, the STRUM's own Q out of play; the
      // ribbon on SNAP holds the voice to the quantiser's notes (the engine snaps it: a note of the scale, not the
      // finger's quarter tone), on FREE it does not
      {
        const I0 = await page.evaluate(() => ({ orig: Math.floor(window.__impakt.state.tracks[2].p[0] * 64), t: window.__impakt.ui.t,
          none: document.querySelectorAll('#qfol .qf.none').length, sq: [...document.querySelectorAll('#qfol .qf[data-t]')].map(b => b.textContent).join(' ') }));
        await page.evaluate(() => window.__impakt.selectTrack(2)); await page.waitForTimeout(60);
        await page.click('#mid'); await page.waitForTimeout(100);
        await page.locator('#mpick button[data-m="35"]').click(); await page.waitForTimeout(80);
        await page.evaluate(t => window.__impakt.selectTrack(t), I0.t); await page.waitForTimeout(60);
        const fw = { before: I0.sq || (I0.none ? 'none' : ''), scale0: await page.evaluate(() => document.getElementById('qlab').textContent),
          sq: await page.evaluate(() => [...document.querySelectorAll('#qfol .qf[data-t]')].map(b => b.textContent).join(' ')) };
        await page.locator('#qfol .qf[data-t="2"]').click(); await page.waitForTimeout(60);
        fw.on = await page.evaluate(() => { const I = window.__impakt, b = document.querySelector('#qfol .qf[data-t="2"]'), c = document.querySelectorAll('#tsub .tchip.q')[2];
          return { txt: document.getElementById('qlab').textContent, cls: b.classList.contains('on'), src: Math.floor(I.state.g[I.G.QSRC] * 9), chip: c.classList.contains('src') && c.disabled }; });
        if (!(await page.evaluate(() => window.__impakt.ui.pos.run))) await page.click('#play');
        await page.waitForFunction(() => { const u = window.__impakt.ui; return u.qm > 0 && u.qm !== window.__impakt.state.scale; }, null, { timeout: 6000 }).catch(() => {});
        fw.keys = await page.evaluate(() => ({ qm: window.__impakt.ui.qm, lit: document.querySelectorAll('#kb .key.on').length, base: document.querySelectorAll('#kb .key.base').length }));
        await page.click('#play'); await page.waitForTimeout(300);
        await page.locator('#qfol .qf[data-t="2"]').click(); await page.waitForTimeout(60);
        fw.back = await page.evaluate(() => { const I = window.__impakt; return { txt: document.getElementById('qlab').textContent, src: Math.floor(I.state.g[I.G.QSRC] * 9),
          chip: document.querySelectorAll('#tsub .tchip.q')[2].classList.contains('src') }; });
        // the ribbon on the project's scale: the finger a semitone and a half over C4; SNAP: the scale's note nearest it
        // (the lower one of two as near), FREE: there
        if (!(await page.evaluate(() => window.__impakt.state.scale))) await page.click('#qname');
        fw.want = await page.evaluate(() => { const m = window.__impakt.state.scale, x = 61.5; let best = 0, bd = 1e9;
          for (let n = 50; n <= 74; n++) if ((m >> (n % 12)) & 1) { const d = Math.abs(n - x); if (d < bd) { bd = d; best = n; } } return best; });
        const box = await page.locator('#ovw').boundingBox();
        const sing = async () => {
          await page.mouse.move(box.x + box.width * (0.5 + 1.5 / 24), box.y + box.height * 0.6); await page.mouse.down();
          await page.waitForTimeout(700);
          const m = await page.evaluate(() => { const hi = window.__impakt.ui.halo; return hi ? hi[2] : -1; });
          await page.mouse.up(); await page.waitForTimeout(900);
          return m;
        };
        fw.snap = await sing();
        await page.mouse.click(box.x + box.width - 12, box.y + 6); await page.waitForTimeout(60);    // (SNAP / FREE)
        fw.free = await sing();
        await page.mouse.click(box.x + box.width - 12, box.y + 6); await page.waitForTimeout(60);
        await page.evaluate(() => window.__impakt.selectTrack(2)); await page.waitForTimeout(60);
        await page.click('#mid'); await page.waitForTimeout(100);
        await page.locator(`#mpick button[data-m="${I0.orig}"]`).click(); await page.waitForTimeout(80);
        await page.evaluate(t => window.__impakt.selectTrack(t), I0.t); await page.waitForTimeout(60);
        fw.after = await page.evaluate(() => ({ none: document.querySelectorAll('#qfol .qf.none').length, sq: document.querySelectorAll('#qfol .qf[data-t]').length }));
        ok('quantiser: a square for STRUM T3 (none, grey, before), lit it follows its chords (its keys the chord it plays, its Q out of play), unlit the scale again; the ribbon on SNAP sings a note of the scale, on FREE where the finger is',
          fw.before === 'none' && fw.sq === 'T3' && fw.scale0 === 'SCALE' && fw.on.txt === 'CHORDS OF T3' && fw.on.cls && fw.on.src === 3 && fw.on.chip && fw.keys.lit >= 3 && fw.keys.lit <= 6 && fw.keys.base > 0
          && fw.back.txt === 'SCALE' && fw.back.src === 0 && !fw.back.chip && fw.after.none === 1 && fw.after.sq === 0 && Math.abs(fw.snap - fw.want) < 0.08 && Math.abs(fw.free - 61.5) < 0.08, JSON.stringify(fw));
      }
      // BREAK: its PLAY page (HOLD for DEC, SLOT, SYNC / VARI, STEP, FREE, SIDE), its CHOP page; the LOAD panel: ten
      // slots, loops loaded from files (the tempo from the name or the length, ÷2 / ×2), the track playing its slot,
      // shown on the screen; KEY mode on its slices; the slots kept over a reload, cleared
      {
        await page.click('#mid'); await page.waitForTimeout(100);
        await page.locator('#mpick button[data-m="29"]').click(); await page.waitForTimeout(80);
        const bk = { labels: await page.evaluate(() => [...document.querySelectorAll('#mknobs .knob .lt')].map(e => e.textContent).join(',')),
          slot: await kV(2), mode: await swOf(3), slce: await kV(4), len: await kV(5), side: await swOf(7), dec: await kV(1),
          loadBtn: await page.evaluate(() => !document.getElementById('mload').hidden) };
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(1).click(); await page.waitForTimeout(80);
        bk.chop = await page.evaluate(() => [...document.querySelectorAll('#mknobs .knob .lt')].map(e => e.textContent).join(','));
        bk.seed = await kV(6); bk.skip = await kV(5); bk.swap = await kV(0);
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(0).click(); await page.waitForTimeout(80);
        bk.back = await kL();
        await page.click('#mload'); await page.waitForTimeout(80);
        bk.panel = await page.evaluate(() => ({ open: !document.getElementById('lpanel').hidden, rows: document.querySelectorAll('#llist .lrow').length,
          empty: document.querySelectorAll('#llist .lrow.empty').length }));
        // the loops: two bars at 120 (BPM in the name), one bar at 96 (from its length), 44.1 kHz stereo WAV files
        await page.setInputFiles('#lfile', [{ name: 'amen_120bpm.wav', mimeType: 'audio/wav', buffer: testWav(120, 2, 0) }]);
        await page.waitForFunction(() => window.__impakt.ui.brks && window.__impakt.ui.brks.info[0] === 2, null, { timeout: 8000 }).catch(() => {});
        bk.one = await page.evaluate(() => { const I = window.__impakt, r = document.querySelector('#llist .lrow[data-s="0"]');
          return { st: I.ui.brks.info[0], bpm: Math.round(I.ui.brks.info[1] * 10) / 10, bars: I.ui.brks.info[2], name: r.querySelector('.lname').textContent,
            meta: r.querySelector('.lmeta').textContent, sel: r.classList.contains('sel'), slot: document.querySelectorAll('#mknobs .knob')[2].querySelector('.kv').textContent }; });
        await page.setInputFiles('#lfile', [{ name: 'think loop.wav', mimeType: 'audio/wav', buffer: testWav(96, 1, 1) }, { name: 'third.wav', mimeType: 'audio/wav', buffer: testWav(150, 4, 2) }]);
        await page.waitForFunction(() => { const i = window.__impakt.ui.brks.info; return i[4] === 2 && i[8] === 2; }, null, { timeout: 8000 }).catch(() => {});
        bk.more = await page.evaluate(() => { const i = window.__impakt.ui.brks.info; return [i[4], Math.round(i[5]), i[6], i[8], Math.round(i[9]), i[10], window.__impakt.brkNames.slice(0, 3).join('|')]; });
        await page.locator('#llist .lrow[data-s="0"] .lsm[data-a="half"]').click();
        await page.waitForFunction(() => window.__impakt.ui.brks.info[2] === 1 && window.__impakt.ui.brks.info[0] === 2, null, { timeout: 4000 }).catch(() => {});
        bk.half = await page.evaluate(() => [window.__impakt.ui.brks.info[1], window.__impakt.ui.brks.info[2]]);
        await page.locator('#llist .lrow[data-s="0"] .lsm[data-a="dbl"]').click();
        await page.waitForFunction(() => window.__impakt.ui.brks.info[2] === 2 && window.__impakt.ui.brks.info[0] === 2, null, { timeout: 4000 }).catch(() => {});
        await page.screenshot({ path: `${shotDir}/impakt-${label}-break-loops.png`, fullPage: true });
        await page.click('#lclose'); await page.waitForTimeout(60);
        // (stopped: the pad plays the track, held; while the sequence runs it only selects it)
        if (await page.evaluate(() => window.__impakt.ui.pos.run)) { await page.click('#play'); await page.waitForTimeout(200); }
        const h0 = await page.evaluate(() => window.__impakt.ui.pos.hits[0]);
        await page.locator('.pad').nth(0).dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true });
        await page.waitForTimeout(300);           // (a slice auditioned plays about 0.6 s)
        bk.play = await page.evaluate(() => { const I = window.__impakt, bi = I.ui.brk, cv = document.getElementById('ovw');
          let lit = 0; if (cv) { const d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data; for (let i = 3; i < d.length; i += 4) if (d[i]) lit++; }
          return { info: bi ? bi.length : 0, on: bi ? bi[0] : 0, slot: bi ? bi[1] : -1, len: bi ? bi[2] : 0, bpm: bi ? Math.round(bi[9]) : 0, mask0: bi ? bi[16] : 0, hits: I.ui.pos.hits[0], lit }; });
        await page.screenshot({ path: `${shotDir}/impakt-${label}-break.png`, fullPage: true });
        await page.evaluate(() => { const I = window.__impakt; I.ui.key = true; I.renderAll(); });
        bk.keys = await page.evaluate(() => { const I = window.__impakt, kl = I.keyLayout(); return { brk: !!kl.brk, first: kl.keys[0], last: kl.keys[15],
          lbl: [...document.querySelectorAll('#steps .sn')].slice(0, 3).map(e => e.textContent).join(','), bar: document.querySelector('#lockbar .lt small').textContent }; });
        await page.evaluate(() => { const I = window.__impakt; I.ui.key = false; I.renderAll(); });
        ok('BREAK: PLAY page (HOLD, SLOT, MODE, SLCE, LEN, DUST, SIDE), CHOP page (SWAP first), LOAD, its loop on the screen, KEY on its slices', bk.labels === 'TUNE,DEC,SLOT,MODE,SLCE,LEN,DUST,SIDE'
          && bk.slot === '1 —' && bk.mode.sw && bk.mode.opts === 'SYNC,VARI' && bk.mode.on === 'SYNC' && bk.slce === 'STEP' && bk.len === 'FREE' && bk.dec === 'HOLD' && bk.loadBtn
          && bk.side.sw && bk.side.opts === 'PLAY,CHOP' && bk.chop === 'SWAP,RAND,REV,STUT,PJMP,SKIP,SEED,SIDE' && bk.seed === 'FREE' && bk.skip === 'OFF' && bk.swap === 'OFF'
          && bk.back === 'SLOT,MODE,SLCE,LEN,DUST,SIDE' && bk.play.info === 272 && bk.play.on === 1 && bk.play.slot === 0 && bk.play.len === 32 && bk.play.bpm === 120 && (bk.play.mask0 & 3) === 1
          && bk.play.hits > h0 && bk.play.lit > 300 && bk.keys.brk && bk.keys.first === 0 && bk.keys.last === 15 && bk.keys.lbl === '1,2,3' && /SLOT 1 amen_120bpm/i.test(bk.keys.bar), JSON.stringify(bk));
        ok('BREAK: the LOAD panel: 10 slots, a WAV read (tempo from its name, from its length), into the empty slots, ÷2 / ×2',
          bk.panel.open && bk.panel.rows === 10 && bk.panel.empty === 10 && bk.one.st === 2 && bk.one.bpm === 120 && bk.one.bars === 2 && /amen_120bpm/i.test(bk.one.name)
          && bk.one.meta === '120 BPM · 2 bars' && bk.one.sel && bk.one.slot === '1' && bk.more.join() === '2,96,1,2,150,4,amen_120bpm|think loop|third'
          && bk.half[0] === 60 && bk.half[1] === 1, JSON.stringify({ panel: bk.panel, one: bk.one, more: bk.more, half: bk.half }));
        // kept over a reload (this browser), the engine reads them again; a slot cleared
        await page.reload(); await page.waitForTimeout(400);
        await page.evaluate(() => { document.body.click(); });
        await page.waitForFunction(() => { const b = window.__impakt.ui.brks; return b && b.ver >= 0 && b.info[0] === 2 && b.info[4] === 2 && b.info[8] === 2; }, null, { timeout: 8000 }).catch(() => {});
        const kept = await page.evaluate(() => { const I = window.__impakt, i = I.ui.brks.info; return { st: [i[0], i[4], i[8], i[12]], names: I.brkNames.slice(0, 3).join('|'), bars: i[2] }; });
        await page.click('#mload'); await page.waitForTimeout(60);
        await page.locator('#llist .lrow[data-s="1"] .lsm[data-a="del"]').click();
        await page.locator('#llist .lrow[data-s="1"] .lsm[data-a="del"]').click();
        await page.waitForFunction(() => window.__impakt.ui.brks.info[4] === 0, null, { timeout: 3000 }).catch(() => {});
        kept.cleared = await page.evaluate(() => ({ st: window.__impakt.ui.brks.info[4], empty: !!document.querySelector('#llist .lrow[data-s="1"].empty') }));
        await page.click('#lclose');
        ok('BREAK: the slots are kept over a reload, a slot cleared', kept.st.join() === '2,2,2,0' && kept.names === 'amen_120bpm|think loop|third' && kept.bars === 2
          && kept.cleared.st === 0 && kept.cleared.empty, JSON.stringify(kept));
      }
      // FX: T8 on VERB (from the picker's FX family), its MAIN and MORE pages, SRC on SUM (the pads before it marked), its
      // screen (what it takes, its levels, its decay), to ECHO with SRC and the side kept, a preset keeping SRC, TIME on the tempo
      {
        await page.evaluate(() => window.__impakt.selectTrack(7));
        await page.click('#mid'); await page.waitForTimeout(100);
        const fam = await page.evaluate(() => [...document.querySelectorAll('#mpick .pgrp')].map(g => g.querySelector('.pgl').textContent + ':' + g.querySelectorAll('button').length).pop());
        // (what the sound before leaves: a lock of E, DUCK, a level, a pan, an HP, a follower)
        const before = await page.evaluate(() => { const I = window.__impakt, P = I.P, tr = I.state.tracks[7]; tr.steps[3].l[P.E] = 0.7; tr.steps[3].l[P.B] = 0.9;
          tr.p[P.DUCK] = 0.5; tr.p[P.VOL] = 0.55; tr.p[P.PAN] = 0.8; tr.p[P.HPF] = 0.3; tr.p[P.E1DS] = 0.25;
          return tr.steps.reduce((a, st) => a + Object.keys(st.l).length, 0); });
        await page.locator('#mpick button[data-m="30"]').click(); await page.waitForTimeout(80);
        const fx = { fam, labels: await kL8(), src: await kV(2), dec: await kV(1), dcay: await kV(4), side: await swOf(7) };
        fx.clean = await page.evaluate(() => { const I = window.__impakt, P = I.P, tr = I.state.tracks[7];
          return { locks: tr.steps.reduce((a, st) => a + Object.keys(st.l).length, 0), duck: tr.p[P.DUCK], vol: tr.p[P.VOL], pan: tr.p[P.PAN], hpf: tr.p[P.HPF], lpf: tr.p[P.LPF], e1: tr.p[P.E1DS] < 0.03, ldst: tr.p[P.LDST] < 0.03, rev: tr.p[P.REV], dly: tr.p[P.DLY] }; });
        fx.clean.before = before;
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.A, 1.5 / 10); I.renderAll(); });
        fx.pads = await page.evaluate(() => [...document.querySelectorAll('.pad')].map(b => b.classList.contains('fxin') ? 1 : 0).join(''));
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(1).click(); await page.waitForTimeout(80);
        fx.more = await kL8(); fx.act = await swOf(6); fx.shap = await kV(5);
        if (!(await page.evaluate(() => window.__impakt.ui.pos.run))) await page.click('#play');
        await page.waitForTimeout(1500);
        fx.scr = await page.evaluate(() => { const I = window.__impakt, f = I.ui.fxm, cv = document.getElementById('ovw');
          let lit = 0; if (cv) { const d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data; for (let i = 3; i < d.length; i += 4) if (d[i]) lit++; }
          return { m: f ? f[0] : -1, mask: f ? f[1] : -1, hist: (I.ui.fxmHist || []).length, rt: f ? Math.round(f[6] * 10) / 10 : 0, lit }; });
        await page.screenshot({ path: `${shotDir}/impakt-${label}-fx-verb.png`, fullPage: true });
        await page.click('#mid'); await page.waitForTimeout(80);
        await page.locator('#mpick button[data-m="31"]').click(); await page.waitForTimeout(80);
        fx.echo = { more: await kL8(), src: await page.evaluate(() => { const I = window.__impakt; return I.state.tracks[7].p[I.P.A]; }), side: await page.evaluate(() => { const I = window.__impakt; return I.state.tracks[7].p[I.P.F]; }) };
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(0).click(); await page.waitForTimeout(80);
        fx.echo.labels = await kL8(); fx.echo.time = await kV(4);
        fx.pre = await page.evaluate(() => { const I = window.__impakt, es = I.presetEntries(31); return { n: es.length, first: es[1] && es[1].name }; });
        await page.screenshot({ path: `${shotDir}/impakt-${label}-fx-echo.png` });
        ok('FX: VERB from the FX family (a clean insert: the sound before leaves no lock, ducking, level, pan, TONE, follower), MAIN (TILT DEC SRC SIZE DCAY ABSR MIX SIDE), MORE (BASS SHFT SPED DPTH PRE SHAP ACT), SRC SUM marks T1..T7, its screen; ECHO keeps SRC',
          fx.fam === 'FX:8' && fx.labels === 'TILT,DEC,SRC,SIZE,DCAY,ABSR,MIX,SIDE' && fx.src === 'SUM' && fx.dec === 'HOLD' && /s$/.test(fx.dcay) && fx.side.sw && fx.side.opts === 'MAIN,MORE'
          && fx.pads === '11111110' && fx.more === 'BASS,SHFT,SPED,DPTH,PRE,SHAP,ACT,SIDE' && fx.act.sw && fx.act.opts === 'IN,FRZ,REV' && fx.shap === 'SINE'
          && fx.scr.m === 30 && fx.scr.mask === 127 && fx.scr.hist > 20 && fx.scr.rt > 0.3 && fx.scr.lit > 600
          && fx.echo.more === 'PING,DUCK,µ,HALO,SKEW,SYNC,ACT,SIDE' && fx.echo.labels === 'COLR,DEC,SRC,ZONE,TIME,REPT,MIX,SIDE' && Math.abs(fx.echo.src - 0.15) < 1e-9 && fx.echo.side > 0.5 && /^1\//.test(fx.echo.time)
          && fx.pre.n === 15
          && fx.clean.before >= 2 && fx.clean.locks === 0 && fx.clean.duck === 0 && fx.clean.vol === 0.8 && fx.clean.pan === 0.5 && fx.clean.hpf === 0 && fx.clean.lpf === 1
          && fx.clean.e1 && fx.clean.ldst && fx.clean.rev === 0 && fx.clean.dly === 0, JSON.stringify(fx));
        // v68: FUZZ (N°40, in the FX family), SRC kept from ECHO; MAIN (FUZZ BIAS SUST), MORE (the three bands, GATE, the
        // crossovers, ACT IN / STRV); its screen (the bands, the gate); its 12 presets
        await page.click('#mid'); await page.waitForTimeout(80);
        await page.locator('#mpick button[data-m="39"]').click(); await page.waitForTimeout(80);
        const fz = { labels: await kL8(), src: await page.evaluate(() => { const I = window.__impakt; return I.state.tracks[7].p[I.P.A]; }), circ: await kV(1), fuzz: await kV(2), bias: await kV(3) };
        // v73: CIRC, the circuit (FUZZ, then PRE .. BENT); on PRE the screen names it
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.S5, 1.5 / 8); I.renderAll(); });
        fz.circPre = await kV(1);
        await page.waitForTimeout(400);
        fz.scrPre = await page.evaluate(() => window.__impakt.ui.fxm ? 1 : 0);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-fx-fuzz-circ.png`, fullPage: true });
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.S5, 0.5 / 8); I.renderAll(); });
        await page.waitForTimeout(900);
        fz.scr = await page.evaluate(() => { const I = window.__impakt, f = I.ui.fxm, cv = document.getElementById('ovw');
          let lit = 0; if (cv) { const d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data; for (let i = 3; i < d.length; i += 4) if (d[i]) lit++; }
          return { m: f ? f[0] : -1, lit }; });
        await page.screenshot({ path: `${shotDir}/impakt-${label}-fx-fuzz.png`, fullPage: true });
        fz.side = await swOf(7);
        // v71: its SUB side, the octaver (SUB1 SUB2 GRIT PATH); the screen's SUB bar lit once it is on
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(1).click(); await page.waitForTimeout(80);
        fz.sub = await kL8(); fz.sub1 = await kV(0); fz.grit = await kV(2); fz.path = await swOf(3); fz.sust = await kV(4);
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.S1, 0.75); I.renderAll(); });
        fz.sub1on = await kV(0);
        await page.waitForTimeout(500);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-fx-fuzz-sub.png`, fullPage: true });
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.S1, 0); I.renderAll(); });
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(2).click(); await page.waitForTimeout(80);
        fz.more = await kL8(); fz.act = await swOf(6); fz.low = await kV(0); fz.gate = await kV(3);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-fx-fuzz-more.png`, fullPage: true });
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(0).click(); await page.waitForTimeout(80);
        fz.pre = await page.evaluate(() => { const I = window.__impakt, es = I.presetEntries(39); return { n: es.length, first: es[1] && es[1].name }; });
        ok('FUZZ (v68): N°40 in the FX family, SRC kept; MAIN (v73: SRC CIRC DRIV BIAS TONE DEC MIX), SUB (v71: SUB1 SUB2 GRIT PATH, v73: SUST), MORE (LOW MID HIGH GATE XLO XHI ACT), its screen, its 26 presets',
          fz.labels === 'SRC,CIRC,DRIV,BIAS,TONE,DEC,MIX,SIDE' && Math.abs(fz.src - 0.15) < 1e-9 && fz.circ === 'FUZZ' && fz.fuzz === '60%' && fz.bias === '9V' && fz.circPre === 'PRE'
          && fz.side.sw && fz.side.opts === 'MAIN,SUB,MORE' && fz.side.on === 'MAIN'
          && fz.sub === 'SUB1,SUB2,GRIT,PATH,SUST,—,—,SIDE' && fz.sust === '20%' && fz.sub1 === 'OFF' && fz.grit === 'PURE' && fz.path.sw && fz.path.opts === 'CLEAN,FUZZ' && fz.path.on === 'CLEAN'
          && fz.sub1on === '75%'
          && fz.more === 'LOW,MID,HIGH,GATE,XLO,XHI,ACT,SIDE' && fz.act.sw && fz.act.opts === 'IN,STRV'
          && fz.scr.m === 39 && fz.scr.lit > 300 && fz.pre.n === 27, JSON.stringify(fz));
        // v78: WEFT (N°43, Mutable Instruments' Warps, in the FX family), SRC kept; MAIN (NOTE DEC SRC ALGO TIMB CARR MIX),
        // MORE (LVL1 LVL2 MODE, ACT IN / FRZ); MODE SHFT: ALGO the shift, the levels FEED and D/W; its screen; CARR a track
        // takes it (the pads)
        await page.click('#mid'); await page.waitForTimeout(80);
        await page.locator('#mpick button[data-m="42"]').click(); await page.waitForTimeout(80);
        const wf = { labels: await kL8(), src: await page.evaluate(() => { const I = window.__impakt; return I.state.tracks[7].p[I.P.A]; }), note: await kV(0), algo: await kV(3), timb: await kV(4), carr: await kV(5), side: await swOf(7) };
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(1).click(); await page.waitForTimeout(80);
        wf.more = await kL8(); wf.act = await swOf(6); wf.mode = await swOf(2);
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.XC, 0.75); I.renderAll(); });
        wf.moreS = await kL8();
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(0).click(); await page.waitForTimeout(80);
        wf.shft = { labels: await kL8(), algo: await kV(3) };
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.XC, 0.25); I.setTrackParam(I.P.B, 0.6); I.renderAll(); });
        wf.xor = await kV(3);
        await page.waitForTimeout(900);
        wf.scr = await page.evaluate(() => { const I = window.__impakt, f = I.ui.fxm, cv = document.getElementById('ovw');
          let lit = 0; if (cv) { const d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data; for (let i = 3; i < d.length; i += 4) if (d[i]) lit++; }
          return { m: f ? f[0] : -1, lit }; });
        await page.screenshot({ path: `${shotDir}/impakt-${label}-fx-weft.png`, fullPage: true });
        ok('WEFT (v78): N°43 in the FX family, SRC kept; MAIN (NOTE DEC SRC ALGO TIMB CARR MIX), MORE (LVL1 LVL2 MODE ACT), SHFT, its screen',
          wf.labels === 'NOTE,DEC,SRC,ALGO,TIMB,CARR,MIX,SIDE' && Math.abs(wf.src - 0.15) < 1e-9 && wf.note === 'C3' && wf.algo === 'ARING' && wf.timb === '50%' && wf.carr === 'SIN'
          && wf.side.opts === 'MAIN,MORE' && wf.more === 'LVL1,LVL2,MODE,—,—,—,ACT,SIDE' && wf.act.opts === 'IN,FRZ' && wf.mode.opts === 'WARP,SHFT' && wf.mode.on === 'WARP'
          && wf.moreS === 'FEED,D/W,MODE,—,—,—,ACT,SIDE' && wf.shft.labels === 'NOTE,DEC,SRC,SHFT,BAND,CARR,MIX,SIDE' && wf.shft.algo === '−11Hz' && wf.xor === 'XOR›'
          && wf.scr.m === 42 && wf.scr.lit > 300, JSON.stringify(wf));
        // v83: STENCIL (N°45, in the FX family): SRC kept (its list NONE first: SUM there too), MAIN (COLR DEC SRC ZONE RATE
        // REPT MIX, SIDE: MAIN MORE SPACE), MORE (µ HALO SKEW PING NOIS SYNC ACT: IN HOLD FLIP PLUK), SPACE (SIZE DCAY DAMP
        // DRFT µSPD DUCK WIDE); RATE on SYNC a division of the tempo, in KARP a note (free: Hz); SKEW on SYNC a ratio; NOTE
        // not off (it transposes KARP); its presets; its screen; back onto WEFT: SUM again
        await page.click('#mid'); await page.waitForTimeout(80);
        await page.locator('#mpick button[data-m="44"]').click(); await page.waitForTimeout(80);
        const sc = { labels: await kL8(), src: await page.evaluate(() => { const I = window.__impakt; return I.state.tracks[7].p[I.P.A]; }), colr: await kV(0), srcV: await kV(2),
          zone: await kV(3), rate: await kV(4), rept: await kV(5), side: await swOf(7),
          note: await page.evaluate(() => { const I = window.__impakt, m = I.paramMeta(7, I.P.NOTE); return !!m && !m.off; }),
          pre: await page.evaluate(() => window.__impakt.presetEntries(44).length), kana: await page.evaluate(() => window.__impakt.KANA[44]) };
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(1).click(); await page.waitForTimeout(80);
        sc.more = await kL8(); sc.act = await swOf(6); sc.sync = await swOf(5); sc.skew = await kV(2);
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(2).click(); await page.waitForTimeout(80);
        sc.space = await kL8(); sc.wide = await kV(6); sc.mspd = await kV(4);
        await page.locator('#mknobs .knob').nth(7).locator('.sw b').nth(0).click(); await page.waitForTimeout(80);
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.B, 0.5 / 8); I.renderAll(); });
        sc.karp = await kV(4);
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.XD, 0.25); I.renderAll(); });
        sc.karpHz = await kV(4);
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.B, 3.5 / 8); I.setTrackParam(I.P.XD, 0.75); I.setTrackParam(I.P.D, 0.6); I.renderAll(); });
        await page.waitForTimeout(900);
        sc.scr = await page.evaluate(() => { const I = window.__impakt, f = I.ui.fxm, cv = document.getElementById('ovw');
          let lit = 0; if (cv) { const d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data; for (let i = 3; i < d.length; i += 4) if (d[i]) lit++; }
          return { m: f ? f[0] : -1, lit, tl: f ? Math.round(f[6]) : -1 }; });
        await page.screenshot({ path: `${shotDir}/impakt-${label}-fx-stencil.png`, fullPage: true });
        await page.click('#mid'); await page.waitForTimeout(80);
        await page.locator('#mpick button[data-m="42"]').click(); await page.waitForTimeout(80);
        sc.back = await page.evaluate(() => { const I = window.__impakt; return I.state.tracks[7].p[I.P.A]; });
        ok('STENCIL (v83): N°45 in the FX family, SRC kept (SUM, NONE first in its list); MAIN (COLR DEC SRC ZONE RATE REPT MIX), MORE (µ HALO SKEW PING NOIS SYNC ACT), SPACE; RATE on SYNC, in KARP; its screen; back on WEFT, SUM',
          sc.labels === 'COLR,DEC,SRC,ZONE,RATE,REPT,MIX,SIDE' && Math.abs(sc.src - 2.5 / 11) < 1e-9 && sc.srcV === 'SUM' && sc.colr === 'DARK 12' && sc.zone === 'ECHO'
          && sc.rate === '1/8' && sc.rept === '50%' && sc.side.opts === 'MAIN,MORE,SPACE' && sc.side.on === 'MAIN' && sc.note && sc.pre === 17 && sc.kana === 'ステンシル'
          && sc.more === 'µ,HALO,SKEW,PING,NOIS,SYNC,ACT,SIDE' && sc.act.opts === 'IN,HOLD,FLIP,PLUK' && sc.act.on === 'IN' && sc.sync.opts === 'FREE,SYNC' && sc.sync.on === 'SYNC' && sc.skew === 'R×1'
          && sc.space === 'SIZE,DCAY,DAMP,DRFT,µSPD,DUCK,WIDE,SIDE' && sc.wide === '100%' && sc.mspd === '0.37Hz'
          && sc.karp === 'C5' && sc.karpHz === '527Hz' && sc.scr.m === 44 && sc.scr.lit > 300 && sc.scr.tl > 0 && Math.abs(sc.back - 0.15) < 1e-9, JSON.stringify(sc));
        await page.evaluate(() => window.__impakt.selectTrack(0));
      }
      // v18: the PAGE keys (page 2 tapped: LEN 32, its keys 17..32; held, a step key sets LEN), a trig on page 2; PTN (the
      // keys are the 16 patterns of a bank, the pads the 8 banks): A03 queued while playing comes at the end of the
      // pattern and the page follows (an empty pattern: the sounds, no trigs), A01 back with its trigs, a chain A01 A03
      // plays in turn, COPY / PASTE onto A05, CLEAR it; DEC shows HOLD at its top on a synth, not on CP·NZ
      {
        const v = {};
        await page.evaluate(() => window.__impakt.selectTrack(0));
        v.pg0 = await page.evaluate(() => [...document.querySelectorAll('#pages .pgk')].map(b => b.classList.contains('on') ? 'on' : b.classList.contains('off') ? 'off' : '-').join(''));
        await page.locator('#pages .pgk').nth(1).click(); await page.waitForTimeout(80);
        v.pg1 = await page.evaluate(() => { const I = window.__impakt; return { len: I.state.tracks[0].len, sp: I.ui.spage, k: document.querySelectorAll('#steps .step .sn')[0].textContent, lenv: document.getElementById('lenv').textContent }; });
        await page.locator('#steps .step').nth(1).dispatchEvent('pointerdown', { pointerId: 8, isPrimary: true });
        await page.locator('#steps .step').nth(1).dispatchEvent('pointerup', { pointerId: 8, isPrimary: true }); await page.waitForTimeout(50);
        v.t18 = await page.evaluate(() => window.__impakt.state.tracks[0].steps[17].on);
        { const bb = await page.locator('#pages .pgk').nth(1).boundingBox(); await page.mouse.move(bb.x + bb.width / 2, bb.y + bb.height / 2); await page.mouse.down();
          await page.locator('#steps .step').nth(4).dispatchEvent('pointerdown', { pointerId: 9, isPrimary: false }); await page.mouse.up(); await page.waitForTimeout(60); }
        v.len21 = await page.evaluate(() => window.__impakt.state.tracks[0].len);
        await page.waitForTimeout(420);
        await page.locator('#steps .step').nth(1).dispatchEvent('pointerdown', { pointerId: 10, isPrimary: true });
        await page.locator('#steps .step').nth(1).dispatchEvent('pointerup', { pointerId: 10, isPrimary: true }); await page.waitForTimeout(50);
        v.t18off = await page.evaluate(() => window.__impakt.state.tracks[0].steps[17].on);
        // v60: a PAGE key tapped twice copies its page, three times pastes it onto another (trigs and locks), UNDO takes it back
        {
          await page.evaluate(() => { const I = window.__impakt; I.setLen(32); const t = I.state.tracks[0];
            for (let i = 0; i < 32; i++) { t.steps[i].on = i < 16 && i % 3 === 0 ? 1 : 0; t.steps[i].l = i === 3 ? { 3: 0.2 } : {}; } I.renderAll(); });
          const tap = async (p, n) => { const b = page.locator(`#pages .pgk[data-p="${p}"]`); for (let i = 0; i < n; i++) { await b.dispatchEvent('pointerdown', { pointerId: 21, isPrimary: true }); await b.dispatchEvent('pointerup', { pointerId: 21, isPrimary: true }); await page.waitForTimeout(70); } };
          const pages = () => page.evaluate(() => { const t = window.__impakt.state.tracks[0]; return { a: t.steps.slice(0, 16).map(s => s.on).join(''), b: t.steps.slice(16, 32).map(s => s.on).join(''), lb: JSON.stringify(t.steps[19].l) }; });
          await tap(0, 2); await page.waitForTimeout(500);
          v.copyMsg = await page.evaluate(() => document.getElementById('lockbar').textContent);
          await tap(1, 3); await page.waitForTimeout(150);
          v.pasted = await pages();
          v.pasteMsg = await page.evaluate(() => document.getElementById('lockbar').textContent);
          await page.click('#pgundo'); await page.waitForTimeout(100);
          v.undone = await pages();
        }
        await page.evaluate(() => { const I = window.__impakt; I.setLen(16); I.renderAll(); });
        v.back = await page.evaluate(() => ({ len: window.__impakt.state.tracks[0].len, sp: window.__impakt.ui.spage }));
        await page.screenshot({ path: `${shotDir}/impakt-${label}-pages.png` });
        // PTN
        await page.click('#ptnbtn'); await page.waitForTimeout(80);
        v.ptn = await page.evaluate(() => ({ keys: [...document.querySelectorAll('#steps .step .sn')].map(e => e.textContent).join(','), pads: [...document.querySelectorAll('#pads .pad .n')].map(e => e.textContent).join(''),
          cur: document.querySelectorAll('#steps .step')[0].classList.contains('pcur'), trigs: window.__impakt.state.tracks.reduce((a, t) => a + t.steps.filter(s => s.on).length, 0) }));
        const trigs0 = v.ptn.trigs;
        await page.screenshot({ path: `${shotDir}/impakt-${label}-ptn.png` });
        const waitPat = (p, ms) => page.waitForFunction(p => window.__impakt.state.pat === p, p, { timeout: ms }).then(() => true).catch(() => false);
        if (!(await page.evaluate(() => window.__impakt.ui.pos.run))) await page.click('#play');
        // (tapped early in the pattern: the change has to wait)
        await page.waitForFunction(() => { const d = window.__impakt.ui.patInfo; return window.__impakt.ui.pos.run && d && d[6] > 0.5 && d[6] < 6; }, null, { timeout: 5000 }).catch(() => {});
        await page.locator('#steps .step').nth(2).click(); await page.waitForTimeout(120);
        v.queued = await page.evaluate(() => ({ pat: window.__impakt.state.pat, next: window.__impakt.ui.patInfo && Math.round(window.__impakt.ui.patInfo[1]) }));
        v.toA03 = await waitPat(2, 5000);
        v.a03 = await page.evaluate(() => { const I = window.__impakt; return { trigs: I.state.tracks.reduce((a, t) => a + t.steps.filter(s => s.on).length, 0), a01: !!I.state.pats[0], btn: document.querySelector('#ptnbtn b').textContent }; });
        await page.locator('#steps .step').nth(0).click();
        v.toA01 = await waitPat(0, 5000);
        v.a01trigs = await page.evaluate(() => window.__impakt.state.tracks.reduce((a, t) => a + t.steps.filter(s => s.on).length, 0));
        // a chain: A01, A03
        await page.click('#pchain'); await page.locator('#steps .step').nth(0).click(); await page.locator('#steps .step').nth(2).click(); await page.click('#pchain');
        v.chain = await page.evaluate(() => window.__impakt.state.chain.join());
        const seen = new Set();
        for (let k = 0; k < 40; k++) { seen.add(await page.evaluate(() => window.__impakt.state.pat)); if (seen.has(2) && seen.has(0) && k > 2) break; await page.waitForTimeout(150); }
        v.chainPlays = [...seen].sort().join();
        await page.locator('#steps .step').nth(0).click();          // a single pattern: the chain stops
        v.chainOff = await page.evaluate(() => window.__impakt.state.chain.length);
        await waitPat(0, 5000);
        // copy A01 onto A05, then clear A05
        await page.click('#pcopy'); await page.click('#ppaste'); await page.locator('#steps .step').nth(4).click(); await page.waitForTimeout(60);
        v.paste = await page.evaluate(() => { const I = window.__impakt, pt = I.state.pats[4]; return { has: !!pt, trigs: pt ? pt.tracks.reduce((a, t) => a + t.steps.filter(s => s.on).length, 0) : 0, used: document.querySelectorAll('#steps .step')[4].classList.contains('pused') }; });
        await page.click('#pclear'); await page.locator('#steps .step').nth(4).click(); await page.waitForTimeout(60);
        v.cleared = await page.evaluate(() => window.__impakt.state.pats[4] === null);
        await page.click('#ptnbtn'); await page.waitForTimeout(60);
        v.hold = await page.evaluate(() => { const I = window.__impakt, P = I.P, t = I.state.tracks[0], m0 = t.p[P.MACH];
          const r = {}; t.p[P.MACH] = (9 + 0.5) / 64; r.syn = I.paramMeta(0, P.DEC).f(1); t.p[P.MACH] = (5 + 0.5) / 64; r.cp = I.paramMeta(0, P.DEC).f(1); t.p[P.MACH] = m0; return r; });
        ok('v18: PAGE keys (page 2: LEN 32, keys 17..32, a trig on step 18; held, a step sets LEN 21); PTN: banks on the pads, A03 queued comes at the end of the pattern (empty: no trigs), A01 back, a chain A01 A03, COPY / PASTE / CLEAR; DEC HOLD on SY·RAW only',
          v.pg0 === 'onoffoffoff' && v.pg1.len === 32 && v.pg1.sp === 1 && v.pg1.k === '17' && v.pg1.lenv === '32' && v.t18 === 1 && v.len21 === 21 && v.t18off === 0 && v.back.len === 16 && v.back.sp === 0
          && v.ptn.keys.startsWith('A01,A02,A03') && v.ptn.pads === 'ABCDEFGH' && v.ptn.cur && v.queued.pat === 0 && v.queued.next === 2 && v.toA03 && v.a03.trigs === 0 && v.a03.a01 && v.a03.btn === 'A03'
          && v.toA01 && v.a01trigs === trigs0 && v.chain === '0,2' && v.chainPlays === '0,2' && v.chainOff === 0 && v.paste.has && v.paste.trigs === trigs0 && v.paste.used && v.cleared
          && v.hold.syn === 'HOLD' && v.hold.cp !== 'HOLD', JSON.stringify(v));
        ok('v60: a PAGE key tapped twice copies its page, three times pastes it onto another (trigs and locks), UNDO takes it back',
          /COPIED|COPY/i.test(v.copyMsg) && v.pasted.b === v.pasted.a && v.pasted.a === '1001001001001001' && v.pasted.lb === '{"3":0.2}' && /PASTE/i.test(v.pasteMsg)
          && v.undone.b === '0000000000000000' && v.undone.lb === '{}', JSON.stringify({ copyMsg: v.copyMsg, pasted: v.pasted, pasteMsg: v.pasteMsg, undone: v.undone }));
      }
      // v58: the TRIG row's MORE page: RPAN (v64, where NOTE was) JUMP RNG CURV and the retrig where they were; JUMP OFF / +2 (° on a quantised
      // track, the scale on), RNG 12st at its default, CURV EVEN in the middle; the engine gets them; TRIG again
      {
        const r = {};
        await page.evaluate(() => window.__impakt.selectTrack(4));      // (ITERA, pitched)
        const rl = () => page.evaluate(() => [...document.querySelectorAll('#rknobs .knob .lt')].map(e => e.textContent).join());
        const rv = i => page.evaluate(i => document.querySelectorAll('#rknobs .knob .kv')[i].textContent, i);
        r.p1 = await rl();
        await page.click('#tpk1'); await page.waitForTimeout(60);
        r.p2 = await rl(); r.on = await page.evaluate(() => document.getElementById('tpk1').classList.contains('on') && !document.getElementById('tpk0').classList.contains('on'));
        r.v = [await rv(1), await rv(2), await rv(3)];
        // v64: RPAN in the first place (OFF at its centre, RND to the right, PING to the left), the engine's track gets it
        r.rp = [await rv(0)];
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.RPAN, 1); I.renderAll(); }); r.rp.push(await rv(0));
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.RPAN, 0.2); I.renderAll(); }); r.rp.push(await rv(0));
        r.rpEng = await page.evaluate(() => { const I = window.__impakt; return Math.round(I.state.tracks[I.ui.t].p[I.P.RPAN] * 100) / 100; });
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.RPAN, 0.5); I.renderAll(); });
        // v65: RDEC in the last place (OFF, RND to the right, LOOP to the left); a new seed each time it leaves its centre to
        // the left (not while it moves on that side); on an FX machine it has nothing to draw
        r.rd = [await rv(7)];
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.RDEC, 1); I.renderAll(); }); r.rd.push(await rv(7));
        r.sd = await page.evaluate(() => { const I = window.__impakt, P = I.P, t = I.state.tracks[I.ui.t], s = [];
          I.setTrackParam(P.RDEC, 0.5); t.p[P.RDSD] = 0; I.setTrackParam(P.RDEC, 0.2); s.push(t.p[P.RDSD]); I.setTrackParam(P.RDEC, 0.1); s.push(t.p[P.RDSD]);
          I.setTrackParam(P.RDEC, 0.5); I.setTrackParam(P.RDEC, 0.3); s.push(t.p[P.RDSD]); I.renderAll(); return s; });
        r.rd.push(await rv(7));
        r.rdFx = await page.evaluate(() => { const I = window.__impakt, P = I.P, t = I.state.tracks[7], was = t.p[0];
          t.p[0] = (I.MACH.findIndex(x => x.id === 'VERB') + 0.5) / 64; const s = I.paramMeta(7, P.RDEC).f(1); t.p[0] = was; return s; });
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.RDEC, 0.5); I.renderAll(); });
        // v88: RTUN beside RDEC (OFF, RND to the right, LOOP to the left; the range by the square of the knob: ±12 at an
        // end, cents near the centre), its own seed, nothing to draw on an FX machine
        r.rt = [await rv(6)];
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.RTUN, 1); I.renderAll(); }); r.rt.push(await rv(6));
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.RTUN, 0.6); I.renderAll(); }); r.rt.push(await rv(6));
        r.rts = await page.evaluate(() => { const I = window.__impakt, P = I.P, t = I.state.tracks[I.ui.t], s = [];
          I.setTrackParam(P.RTUN, 0.5); t.p[P.RTSD] = 0; I.setTrackParam(P.RTUN, 0.25); s.push(t.p[P.RTSD]); s.push(t.p[P.RDSD]); I.renderAll(); return s; });
        r.rt.push(await rv(6));
        r.rtFx = await page.evaluate(() => { const I = window.__impakt, P = I.P, t = I.state.tracks[7], was = t.p[0];
          t.p[0] = (I.MACH.findIndex(x => x.id === 'VERB') + 0.5) / 64; const s = I.paramMeta(7, P.RTUN).f(1); t.p[0] = was; return s; });
        await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.RTUN, 0.5); I.renderAll(); });
        const sc0 = await page.evaluate(() => window.__impakt.state.scale);
        await page.evaluate(() => { const I = window.__impakt, P = I.P; I.state.scale = 0; I.setTrackParam(P.JUMP, (14 + 0.5) / 25); I.setTrackParam(P.RCRV, 0.9); I.renderAll(); });
        r.j = await rv(1); r.c = await rv(3);
        await page.evaluate(() => { const I = window.__impakt; I.state.scale = 0xAB5; I.state.tracks[4].q = 1; I.renderAll(); });
        r.deg = await rv(1);
        await page.evaluate(sc => { const I = window.__impakt, P = I.P; I.state.scale = sc; I.setTrackParam(P.JUMP, 0.5); I.setTrackParam(P.RCRV, 0.5); I.renderAll(); }, sc0);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-trig-more.png` });
        await page.click('#tpk0'); await page.waitForTimeout(60);
        r.back = await rl();
        await page.evaluate(() => window.__impakt.selectTrack(0));
        ok('v58: TRIG MORE page (RPAN JUMP RNG CURV RTRG RCNT RTUN RDEC; v64 RPAN: OFF, RND, PING; v65 RDEC: OFF, RND, LOOP, its seed; v88 RTUN: OFF, RND ±12, ±48c, LOOP, its own seed), JUMP OFF / +2 / +2° on a quantised track, RNG 12st, CURV EVEN; back to TRIG',
          r.p1 === 'NOTE,VEL,PROB,MICR,RTRG,RCNT,RVEL,GATE' && r.p2 === 'RPAN,JUMP,RNG,CURV,RTRG,RCNT,RTUN,RDEC' && r.on && r.v.join() === 'OFF,12st,EVEN'
          && r.rt.join() === 'OFF,RND ±12,RND ±48c,LOOP ±3.0' && r.rts[0] > 0 && r.rts[0] !== r.rts[1] && r.rtFx === '—'
          && r.rp.join() === 'OFF,RND 100,PING 60' && r.rpEng === 0.2
          && r.rd.join() === 'OFF,RND 100,LOOP 40' && r.sd[0] > 0 && r.sd[1] === r.sd[0] && r.sd[2] > 0 && r.sd[2] !== r.sd[0] && r.rdFx === '—'
          && r.j === '+2' && r.c === '+51' && r.deg === '+2°' && r.back === r.p1, JSON.stringify(r));
      }
      // switches elsewhere: choke groups (5 positions) on AMP
      await openMod('AMP');
      const rmb5 = await swOf(6, '#k-AMP');     // (v78: after CURV)
      await seqPage();
      ok('CHOK is a 5-position switch', rmb5.sw && rmb5.opts === '—,G1,G2,G3,G4', JSON.stringify(rmb5));
      // the machine's lockup on the LCD (mark in its square, name, katakana, number); BYTE in fat pixels
      await page.click('#mid'); await page.waitForTimeout(100);
      await page.locator('#mpick button[data-m=\"10\"]').click(); await page.waitForTimeout(80);
      const lk1 = await page.evaluate(() => ({ name: document.getElementById('mname').textContent, kana: document.getElementById('mkana').textContent, num: document.getElementById('mnum').textContent,
        px: [...document.querySelectorAll('#mlogo rect')].map(r => r.getAttribute('width')), bg: getComputedStyle(document.getElementById('mlogo')).backgroundColor,
        knobs: document.querySelectorAll('#mknobs .knob').length, inLcdRow: document.getElementById('machmod').contains(document.getElementById('mknobs')) }));
      ok('LCD lockup; BYTE mark in 4×4 fat pixels; SYN encoders in the MACHINE section', lk1.name === 'BYTE' && lk1.kana === 'バイト' && lk1.num === 'N°11' && lk1.px.length === 10 && lk1.px.every(w => w === '10.8')
        && lk1.bg !== 'rgba(0, 0, 0, 0)' && lk1.knobs === 8 && lk1.inLcdRow, JSON.stringify(lk1));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-byte.png`, fullPage: true });
      await page.click('#mid'); await page.waitForTimeout(100);
      await page.locator('#mpick button[data-m=\"0\"]').click(); await page.waitForTimeout(80);
      // mute: swipe a pad down (the track stays selected as it was); again to unmute; the M key mutes the selected track
      await page.locator('.pad').nth(1).scrollIntoViewIfNeeded();
      const pb = await page.locator('.pad').nth(1).boundingBox();
      const swipe = async () => { await page.mouse.move(pb.x + pb.width / 2, pb.y + pb.height / 2); await page.mouse.down(); await page.mouse.move(pb.x + pb.width / 2, pb.y + pb.height / 2 + 40, { steps: 6 }); await page.mouse.up(); await page.waitForTimeout(60); };
      await swipe();
      const mu = await page.evaluate(() => ({ m: window.__impakt.state.tracks[1].mute, pad: document.querySelectorAll('.pad')[1].classList.contains('muted'), t: window.__impakt.ui.t, chips: document.querySelectorAll('#mchips, #mutemode').length }));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-mute.png`, fullPage: true });
      await swipe();
      const mu2 = await page.evaluate(() => window.__impakt.state.tracks[1].mute);
      await page.locator('.pad').nth(2).dispatchEvent('pointerdown', { pointerId: 5, isPrimary: true });
      await page.locator('.pad').nth(2).dispatchEvent('pointerup', { pointerId: 5, isPrimary: true });
      const sel2 = await page.evaluate(() => window.__impakt.ui.t);
      await page.evaluate(() => document.activeElement && document.activeElement.blur && document.activeElement.blur());
      await page.keyboard.press('m'); const mk = await page.evaluate(() => window.__impakt.state.tracks[2].mute);
      await page.keyboard.press('m'); const mk2 = await page.evaluate(() => window.__impakt.state.tracks[2].mute);
      await page.locator('.pad').nth(0).dispatchEvent('pointerdown', { pointerId: 6, isPrimary: true });
      await page.locator('.pad').nth(0).dispatchEvent('pointerup', { pointerId: 6, isPrimary: true });
      ok('swipe down on a pad mutes / unmutes, a tap selects, M key', mu.m === 1 && mu.pad && mu.t === 0 && mu.chips === 0 && mu2 === 0 && sel2 === 2 && mk === 1 && mk2 === 0, JSON.stringify({ mu, mu2, sel2, mk, mk2 }));
      // track length: tap the 8th cell; speed: drag the readout up two notches, double-tap back to 1×
      await page.locator('#lenbar').scrollIntoViewIfNeeded();
      const lc = await page.locator('#lencells i').nth(7).boundingBox();
      await page.mouse.click(lc.x + lc.width / 2, lc.y + lc.height / 2);
      const len8 = await page.evaluate(() => ({ len: window.__impakt.state.tracks[0].len, lit: document.querySelectorAll('#lencells i.on').length, v: document.getElementById('lenv').textContent, out: document.querySelectorAll('.step.out').length }));
      const sb2 = await page.locator('#spd').boundingBox();
      await page.mouse.move(sb2.x + sb2.width / 2, sb2.y + sb2.height / 2); await page.mouse.down();
      await page.mouse.move(sb2.x + sb2.width / 2, sb2.y + sb2.height / 2 - 34, { steps: 5 }); await page.mouse.up();
      const spd2 = await page.evaluate(() => ({ spd: window.__impakt.state.tracks[0].spd, v: document.getElementById('spdv').textContent }));
      await page.locator('#spd').dblclick();
      const lc16 = await page.locator('#lencells i').nth(15).boundingBox();
      await page.mouse.click(lc16.x + lc16.width / 2, lc16.y + lc16.height / 2);
      const back = await page.evaluate(() => ({ spd: window.__impakt.state.tracks[0].spd, len: window.__impakt.state.tracks[0].len }));
      ok('LEN cells set the track length, the SPD readout drags', len8.len === 8 && len8.lit === 8 && len8.v === '8' && len8.out === 8 && spd2.spd === 6 && spd2.v === '2×' && back.spd === 4 && back.len === 16,
        JSON.stringify({ len8, spd2, back }));
      // looper: tap on, the engine reports the window, LEN / POS readouts, tap off
      if (!(await page.evaluate(() => window.__impakt.ui.pos.run))) await page.click('#play');
      await page.waitForTimeout(300);
      await page.locator('#loopbtn').dispatchEvent('pointerdown', { pointerId: 3, isPrimary: true });
      await page.locator('#loopbtn').dispatchEvent('pointerup', { pointerId: 3, isPrimary: true });
      await page.waitForTimeout(500);
      const lpOn = await page.evaluate(() => ({ g: window.__impakt.state.g[58], info: window.__impakt.ui.loop && [...window.__impakt.ui.loop], len: document.getElementById('llenv').textContent,
        pos: document.getElementById('lposv').textContent, on: document.getElementById('loopbtn').classList.contains('on'), looping: document.getElementById('steps').classList.contains('looping') }));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-loop.png`, fullPage: true });
      await page.locator('#loopbtn').dispatchEvent('pointerdown', { pointerId: 3, isPrimary: true });
      await page.locator('#loopbtn').dispatchEvent('pointerup', { pointerId: 3, isPrimary: true });
      await page.waitForTimeout(300);
      const lpOff = await page.evaluate(() => ({ g: window.__impakt.state.g[58], info: window.__impakt.ui.loop && window.__impakt.ui.loop[0] }));
      ok('sequence looper on / off', lpOn.g > 0.5 && lpOn.info && lpOn.info[0] === 1 && lpOn.info[2] === 4 && lpOn.len === '1/4' && lpOn.pos === '0' && lpOn.on && lpOn.looping && lpOff.g < 0.5 && lpOff.info === 0,
         JSON.stringify({ lpOn, lpOff }));
      // REC: playing, a knob held and turned slowly writes its moves as p-locks on the trigs it passes; × Mod removes them
      await page.evaluate(() => window.__impakt.selectTrack(0));
      if (!(await page.evaluate(() => window.__impakt.ui.pos.run))) await page.click('#play');
      await page.click('#rec');
      const rk = page.locator('#mknobs .knob').nth(1); await rk.scrollIntoViewIfNeeded();
      const rb = await rk.boundingBox();
      await page.mouse.move(rb.x + rb.width / 2, rb.y + rb.height / 2); await page.mouse.down();
      for (let i = 1; i <= 24; i++) { await page.mouse.move(rb.x + rb.width / 2, rb.y + rb.height / 2 + (i % 2 ? -3 : 3) - i * 1.5); await page.waitForTimeout(70); }
      await page.mouse.up();
      const recd = await page.evaluate(() => { const st = window.__impakt.state.tracks[0].steps;
        return { n: st.filter(x => 2 in x.l).length, offTrig: st.some(x => !x.on && Object.keys(x.l).length), trigs: st.filter(x => x.on).length,
          app: document.querySelector('.app').classList.contains('recording'), btn: document.getElementById('rec').classList.contains('on') }; });
      await page.click('#rec');
      await page.click('#clrmod'); const arm = await page.locator('#clrmod').textContent(); await page.click('#clrmod');
      const cleared = await page.evaluate(() => { const st = window.__impakt.state.tracks[0].steps; return { locks: st.filter(x => Object.keys(x.l).length).length, trigs: st.filter(x => x.on).length,
        rec: window.__impakt.ui.rec, app: document.querySelector('.app').classList.contains('recording') }; });
      ok('REC writes knob moves as p-locks on trigs, × Mod clears them, trigs stay', recd.n >= 2 && !recd.offTrig && recd.app && recd.btn && arm === 'Sure?' && cleared.locks === 0 && cleared.trigs === recd.trigs && !cleared.rec && !cleared.app,
        JSON.stringify({ recd, arm, cleared }));
      // machine families: the picker in groups, the LCD arrows walk the families' order
      const fam = await page.evaluate(() => { const I = window.__impakt, id = n => I.MACH.findIndex(m => m.id === n);
        return { groups: document.querySelectorAll('#mpick .pgrp').length, labels: [...document.querySelectorAll('#mpick .pgl')].map(e => e.textContent).join('|'), order: I.ORDER.length, uniq: new Set(I.ORDER).size, bdva: id('BD·VA'), bdfm: id('BD·FM'), last: I.ORDER[I.ORDER.length - 1] }; });
      await page.click('#mid'); await page.locator(`#mpick button[data-m="${fam.bdva}"]`).click();
      const lx0 = await page.locator('.mnav').boundingBox();
      await page.click('#mnext'); const m1 = await page.evaluate(() => Math.floor(window.__impakt.state.tracks[0].p[0] * 64));
      const lx1 = await page.locator('.mnav').boundingBox();
      await page.click('#mprev'); await page.click('#mprev'); const m2 = await page.evaluate(() => Math.floor(window.__impakt.state.tracks[0].p[0] * 64));
      await page.click('#mnext');
      ok('machine families: 7 groups (KICK, SNARE + PERC, METAL + NOISE, LOOP + TEXTURE, SYNTH, CHORDS + VOICE, FX), LCD arrows follow them and stay put', fam.groups === 7 && fam.order === 47 && fam.uniq === 47
        && fam.labels === 'KICK|SNARE + PERC|METAL + NOISE|LOOP + TEXTURE|SYNTH|CHORDS + VOICE|FX' && m1 === fam.bdfm && m2 === fam.last && Math.abs(lx0.x - lx1.x) < 0.5,
        JSON.stringify({ fam, m1, m2, lx0: lx0.x, lx1: lx1.x }));
      // TRACK screen (TONE, MOD, FX): the machine's mark, and the spectrum of the track (with the cutoff, FILTER in focus)
      await openMod('FLT');
      await page.waitForTimeout(900);
      const sp = await page.evaluate(() => { const I = window.__impakt, cv = document.getElementById('tspec'), c = cv.getContext('2d');
        const d = c.getImageData(0, 0, cv.width, cv.height).data; let lit = 0; for (let i = 3; i < d.length; i += 4) if (d[i]) lit++;
        return { n: I.ui.spec && I.ui.spec.length, max: I.ui.spec ? Math.max(...I.ui.spec) : 0, mark: !!document.querySelector('#tmk svg'), name: document.getElementById('tname').textContent, w: cv.width, lit }; });
      ok('TRACK screen: machine mark + live spectrum of the track', sp.n === 24 && sp.max > 1e-3 && sp.mark && sp.name === 'BD·VA' && sp.w > 100 && sp.lit > 1000, JSON.stringify(sp));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-trkspec.png`, fullPage: true });
      // TONE (HP / LP, three bands) and the two envelope followers; the TRACK screen draws the module in focus: TONE's
      // visualiser (v70: on all four of TONE's tabs, the BOX's pictograms gone), the LFOs, the follower's history
      const fp = () => page.evaluate(() => { const cv = document.getElementById('tspec'), d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data;
        let h = 0, lit = 0; for (let i = 0; i < d.length; i += 4) if (d[i + 3]) { lit++; h = (h * 31 + d[i] + 7 * d[i + 1] + 13 * d[i + 2] + i) % 1000003; } return { lit, h }; });
      const knobLabels = id => page.evaluate(id => [...document.querySelectorAll('#k-' + id + ' .knob .lt')].map(e => e.textContent).join(','), id);
      const setP = (k, v) => page.evaluate(([k, v]) => { window.__impakt.setTrackParam(window.__impakt.P[k], v); window.__impakt.renderAll(); }, [k, v]);
      const tv = {};
      for (const id of ['TONE', 'EF1', 'EF2', 'LFO1', 'BOX']) {
        await openMod(id); await page.waitForTimeout(150);
        tv[id] = { labels: await knobLabels(id), view: await page.evaluate(() => window.__impakt.trackView()), ...(await fp()) };
      }
      // EF1 follows the track itself and ducks its VOL: the follower moves, its history scrolls, DEST shows VOL
      await openMod('EF1');
      await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.E1DS, I.modDestVal(I.P.VOL)); I.ui.modSel = 2; I.renderAll(); });
      await setP('E1DP', 0.1); await page.waitForTimeout(1500);
      tv.ef = await page.evaluate(() => { const I = window.__impakt, m = I.ui.mod, hst = I.ui.modHist[2];
        return { n: m && m.length, max: Math.max(...hst.map(x => x[0])), len: hst.length, dest: document.getElementById('mroutes').textContent }; });
      await page.evaluate(() => { const I = window.__impakt; I.setTrackParam(I.P.E1DS, I.modDestVal(-1)); I.ui.modSel = 0; I.renderAll(); }); await setP('E1DP', 0.5);
      ok('TONE + M1..M4, the TRACK screen draws the module in focus (TONE\'s visualiser, also on BOX; the modulators\' lanes, M3 a follower onto VOL)',
        tv.TONE.labels === 'HP,LP,LOW,LFRQ,MID,MFRQ,HIGH,HFRQ' && tv.EF1.labels === 'TYPE,SRC,GAIN,BAND,TRG,ATK,DEC,AMT' && tv.EF2.labels === tv.EF1.labels
        && tv.TONE.h !== tv.EF1.h && tv.TONE.view === 'TVIZ' && tv.BOX.view === 'TVIZ' && tv.EF1.view === 'MODS' && tv.LFO1.view === 'MODS' && tv.BOX.labels.startsWith('IR,MIX')
        && tv.ef.n === 128 && tv.ef.len > 20 && tv.ef.max > 0.05 && /VOL/.test(tv.ef.dest), JSON.stringify(tv));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-ef1.png`, fullPage: true });
      // v64: TONE's MORE page (the MORE key in TONE's header): SPRD and MONO; a SYNTH machine starts a little wide, the
      // others centred; back to TONE; no other module has the key
      {
        const tm = {};
        await page.evaluate(() => window.__impakt.selectTrack(0)); await page.waitForTimeout(60);
        await openMod('TONE');
        tm.keys = await page.evaluate(() => [...document.querySelectorAll('.kmore')].map(b => b.closest('.kmod').dataset.pg).join());
        await page.click('#kmore-TONE'); await page.waitForTimeout(80);
        tm.labels = await knobLabels('TONE'); tm.on = await page.evaluate(() => document.getElementById('kmore-TONE').classList.contains('on'));
        tm.mono = await page.evaluate(() => document.querySelectorAll('#k-TONE .knob')[1].querySelector('.kv').textContent);
        await setP('SPRD', 0.5); tm.sprd = await page.evaluate(() => document.querySelectorAll('#k-TONE .knob')[0].querySelector('.kv').textContent);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-tone-more.png`, fullPage: true });
        await seqPage();
        const m0 = await page.evaluate(() => { const I = window.__impakt; return Math.floor(I.state.tracks[I.ui.t].p[0] * 64); });
        await page.click('#mid'); await page.waitForTimeout(100);
        await page.locator('#mpick button[data-m="6"]').click(); await page.waitForTimeout(80);
        tm.fmx = await page.evaluate(() => { const I = window.__impakt; return I.state.tracks[I.ui.t].p[I.P.SPRD]; });
        await page.click('#mid'); await page.waitForTimeout(100);
        await page.locator('#mpick button[data-m="' + m0 + '"]').click(); await page.waitForTimeout(80);
        tm.back0 = await page.evaluate(() => { const I = window.__impakt; return I.state.tracks[I.ui.t].p[I.P.SPRD]; });
        await openMod('TONE');
        await page.click('#kmore-TONE'); await page.waitForTimeout(80);
        tm.back = await knobLabels('TONE');
        ok('v64: TONE MORE (the key on TONE only): SPRD, MONO 155Hz; FM·X starts at 30 %, another machine at 0; back to TONE',
          tm.keys === 'TONE' && tm.on && tm.labels === 'SPRD,MONO,—,—,—,—,—,—' && tm.mono === '155Hz' && tm.sprd === '50%'
          && tm.fmx === 0.3 && tm.back0 === 0 && tm.back === 'HP,LP,LOW,LFRQ,MID,MFRQ,HIGH,HFRQ', JSON.stringify(tm));
      }
      // v67: MOD: LFO 1 and LFO 2 side by side, each with its SLW and LEN (v65's MORE page); the waves SLEW and LOOP (and
      // their ·T); choosing LOOP draws its loop's seed, its depth leaving 0 another (not while it moves on), LOOP·T keeps it
      {
        const lm = {};
        await page.evaluate(() => window.__impakt.selectTrack(0)); await page.waitForTimeout(60);
        await openMod('LFO1');
        lm.l1 = await knobLabels('LFO1'); await openMod('LFO2'); lm.l2 = await knobLabels('LFO2'); await openMod('LFO1');
        lm.vals = await page.evaluate(() => [...document.querySelectorAll('#k-LFO1 .knob')].map(k => k.querySelector('.kv').textContent).slice(5, 7).join());
        lm.waves = await page.evaluate(() => { const I = window.__impakt, m = I.paramMeta(I.ui.t, I.P.LWAV); return [4, 6, 7, 14, 15].map(i => m.f((i + 0.5) / 16)).join(); });
        lm.sd = await page.evaluate(() => { const I = window.__impakt, P = I.P, t = I.state.tracks[I.ui.t], s = [];
          I.setTrackParam(P.LDEP, 0.5); t.p[P.L1SD] = 0;
          I.setTrackParam(P.LWAV, 7.5 / 16); s.push(t.p[P.L1SD]);
          I.setTrackParam(P.LDEP, 0.8); s.push(t.p[P.L1SD]);
          I.setTrackParam(P.LDEP, 0.9); s.push(t.p[P.L1SD]);
          I.setTrackParam(P.LWAV, 15.5 / 16); s.push(t.p[P.L1SD]);
          I.setTrackParam(P.LWAV, 7.5 / 16); I.setTrackParam(P.LDST, I.modDestVal(P.TUNE)); I.renderAll(); return s; });
        await page.waitForTimeout(400);
        await page.screenshot({ path: `${shotDir}/impakt-${label}-mod.png`, fullPage: true });
        await page.evaluate(() => { const I = window.__impakt, P = I.P; I.setTrackParam(P.LDEP, 0.5); I.setTrackParam(P.LDST, I.modDestVal(-1)); I.setTrackParam(P.LWAV, 0.5 / 16); I.renderAll(); });
        ok('v67 (v90: M1, M2 WAVE): MOD: the LFOs each with SLW and LEN (50 %, 8); S&H, SLEW, LOOP, their ·T; LOOP draws a seed, its depth out of 0 another',
          lm.l1 === 'TYPE,RATE,SHAP,PHAS,CLK,SLW,LEN,AMT' && lm.l2 === lm.l1 && lm.vals === '50%,8'
          && lm.waves === 'S&H,SLEW,LOOP,SLEW·T,LOOP·T' && lm.sd[0] > 0 && lm.sd[1] > 0 && lm.sd[1] !== lm.sd[0] && lm.sd[2] === lm.sd[1] && lm.sd[3] === lm.sd[2], JSON.stringify(lm));
      }
      await seqPage();
      // Init: a clean project (master neutral, no drive, empty pattern)
      await proj('init'); await proj('init'); await page.waitForTimeout(150);
      const ini = await page.evaluate(() => { const s = window.__impakt.state; return { trig: s.tracks.some(t => t.steps.some(st => st.on)), drv: s.tracks.every(t => t.p[14] === 0 && t.p[19] === 0 && t.p[20] === 0 && t.p[49] === 0),
        kickDriv: s.tracks[0].p[7], comp: Math.floor(s.g[29] * 10), drive: Math.floor(s.g[34] * 8), dac: Math.floor(s.g[48] * 5), calm: s.g[52], umsk: s.g[50] }; });
      ok('Init gives a clean project', !ini.trig && ini.drv && ini.kickDriv === 0 && ini.comp === 0 && ini.drive === 0 && ini.dac === 0 && ini.calm === 0 && ini.umsk === 0, JSON.stringify(ini));
      await proj('demo'); await proj('demo'); await page.waitForTimeout(100);
      // MASTR: the master chain's seven pages, the strip with meters, a compressor setting reaches the engine
      const tabsN = await page.evaluate(() => [document.querySelectorAll('#ptabs .ptab').length, document.querySelectorAll('#gtabs .tab').length].join('+'));
      await openMod('COMP');
      await page.waitForTimeout(900);
      const mst = await page.evaluate(() => ({ strip: !!document.querySelector('#gstrip .meter'), gr: document.getElementById('mgrv').textContent, out: document.getElementById('moutv').textContent,
        tabs: [...document.querySelectorAll('#gtabs .tab')].map(b => b.textContent).join(','), labels: [...document.querySelectorAll('#gknobs .knob .lt')].map(e => e.textContent).join(','),
        trk: [...document.querySelectorAll('#k-AMP .knob .lt')].map(e => e.textContent).join(',') }));
      ok('MASTR: the chain\'s pages and meters, apart from the track\'s modules', tabsN === '5+7' && mst.tabs === 'EQ,COMP,DRV,GRN,OTT,GLCH,MIX' && mst.strip && mst.labels.startsWith('INPT,ATK,REL,RATIO') && mst.out !== '−∞' && mst.trk.startsWith('VOL,PAN'), JSON.stringify({ tabsN, mst }));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-comp.png`, fullPage: true });
      // GLITCH: its knobs, its ON key, the corrupt / loss modes, HOLD; an r key above the pads changes RAND's tracks;
      // its screen shows the last divisions once it runs
      await page.locator('#gtabs .tab', { hasText: 'GLCH' }).click(); await page.waitForTimeout(80);
      const gl = {};
      gl.labels = await page.evaluate(() => [...document.querySelectorAll('#gknobs .knob .lt')].map(e => e.textContent).join(','));
      gl.off = await page.evaluate(() => document.querySelector('#gtabs .tab.cur').classList.contains('byp'));
      await page.evaluate(() => { window.__ops = []; });
      await page.click('#mon');
      await page.locator('.glch button', { hasText: 'DROP' }).click();
      await page.locator('.glch button', { hasText: 'PKT·R' }).click();
      await page.click('#xhld');
      gl.g = await page.evaluate(() => { const I = window.__impakt, G = I.G, g = I.state.g; return { on: g[G.XON], cmd: Math.floor(g[G.XCMD] * 3), lmd: Math.floor(g[G.XLMD] * 5), hld: g[G.XHLD] }; });
      await page.click('#xhld');
      await page.evaluate(() => { const I = window.__impakt; I.setGlobal(I.G.XRPT, 0.8); I.setGlobal(I.G.XLOS, 0.5); I.renderAll(); });
      await page.waitForTimeout(1500);
      gl.screen = await page.evaluate(() => { const cv = document.getElementById('xviz'); if (!cv) return null; const d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data; let lit = 0; for (let i = 3; i < d.length; i += 4) if (d[i]) lit++;
        const gi = window.__impakt.ui.glitch; return { lit, info: gi ? gi.length : 0, events: gi ? gi.slice(0, 16).filter(x => x > 0).length : 0, bw: gi ? Math.round(gi[18]) : 0 }; });
      const xm0 = await page.evaluate(() => Math.round(window.__impakt.state.g[window.__impakt.G.XMSK] * 255));
      await seqPage();
      await page.locator('#tsub .tchip.r').nth(2).click();
      const xm1 = await page.evaluate(() => Math.round(window.__impakt.state.g[window.__impakt.G.XMSK] * 255));
      await page.locator('#tsub .tchip.r').nth(2).click();
      await openMod('GLCH');
      gl.mask = [xm0, xm1];
      ok('GLCH page: TIME / RPT / BEND / BRK / CRPT / LOSS / RESO / MIX, ON key, modes, HOLD, r keys, live screen', gl.labels === 'TIME,RPT,BEND,BRK,CRPT,LOSS,RESO,MIX' && gl.off
        && gl.g.on > 0.5 && gl.g.cmd === 1 && gl.g.lmd === 4 && gl.g.hld > 0.5 && gl.screen && gl.screen.info === 22 && gl.screen.events > 3 && gl.screen.lit > 300 && gl.screen.bw > 1000
        && gl.mask[0] === 255 && gl.mask[1] === 255 - 4, JSON.stringify(gl));
      // LOSS analog: DIG / ANA, the five tapes in place of the codec's modes, SPD from WOW to FLUT, the tape on the screen
      await page.evaluate(() => { window.__ops = []; });
      await page.locator('.glch .xana button', { hasText: 'ANA' }).click();
      const an = {};
      an.modes = await page.evaluate(() => [...document.querySelectorAll('.glch button[data-g]')].map(b => b.textContent).join(','));
      await page.locator('.glch button', { hasText: 'WORN' }).click();
      await page.evaluate(() => { const I = window.__impakt; I.setGlobal(I.G.XSPD, 0); I.renderAll(); });
      an.spd0 = await page.evaluate(() => document.getElementById('xspd').textContent);
      await page.evaluate(() => { const I = window.__impakt; I.setGlobal(I.G.XSPD, 1); I.renderAll(); });
      an.spd1 = await page.evaluate(() => document.getElementById('xspd').textContent);
      await page.evaluate(() => { const I = window.__impakt; I.setGlobal(I.G.XSPD, 0.5); I.renderAll(); });
      an.g = await page.evaluate(() => { const I = window.__impakt, G = I.G, g = I.state.g; return { ana: g[G.XANA], amd: Math.floor(g[G.XAMD] * 5) }; });
      await page.waitForTimeout(800);
      an.tape = await page.evaluate(() => { const gi = window.__impakt.ui.glitch; return gi ? [gi[20], Math.round(gi[18])] : null; });
      ok('GLCH LOSS: DIG / ANA, the five tapes, SPD from WOW to FLUT, the tape on the screen', an.modes === 'DECIM,DROP,DSTR,CASS,VHS,MICRO,TOY,WORN'
        && an.g.ana > 0.5 && an.g.amd === 4 && an.spd0 === 'WOW' && an.spd1 === 'FLUT' && an.tape && an.tape[0] === 1 && an.tape[1] > 1000, JSON.stringify(an));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-glch.png`, fullPage: true });
      await page.locator('.glch .xana button', { hasText: 'DIG' }).click();
      await page.evaluate(() => { const I = window.__impakt; [I.G.XON, I.G.XRPT, I.G.XLOS].forEach(g => I.setGlobal(g, 0)); I.setGlobal(I.G.XCMD, 0.1); I.setGlobal(I.G.XLMD, 0.1); I.renderAll(); });
      await page.locator('#gtabs .tab', { hasText: 'COMP' }).click();
      await seqPage();
      // the trig row under the trig keys (NOTE .. GATE); the modules of TONE, MOD and FX
      const trgRow = await page.evaluate(() => ({ labels: [...document.querySelectorAll('#rknobs .knob .lt')].map(e => e.textContent).join(','),
        tabs: ['tn', 'md', 'fx', 'mt'].map(t => [...document.querySelectorAll('#kp-' + t + ' .kmod')].map(m => m.dataset.pg).join(',')).join('|'), p0: window.__impakt.state.tracks[window.__impakt.ui.t].p[27] }));
      await page.locator('#rknobs .knob').nth(2).focus();
      for (let i = 0; i < 12; i++) await page.keyboard.press('ArrowDown');
      trgRow.p1 = await page.evaluate(() => window.__impakt.state.tracks[window.__impakt.ui.t].p[27]);
      ok('trig row under the steps (NOTE .. GATE), PROB turns from it; TONE: FILTER TONE AMP BOX (v70), MOD: M1 M2 M3 M4 (v90), FX: SEND REVERB DELAY CHORUS PHASER, MASTR: OUT', trgRow.labels === 'NOTE,VEL,PROB,MICR,RTRG,RCNT,RVEL,GATE'
        && trgRow.tabs === 'FLT,TONE,AMP,BOX|LFO1,LFO2,EF1,EF2|SEND,REV,DLY,CHO,PHS|OUT' && trgRow.p1 < trgRow.p0, JSON.stringify(trgRow));
      for (let i = 0; i < 12; i++) await page.keyboard.press('ArrowUp');
      // ON keys on every effect: REVERB and DELAY (their screens on FX), MIX (MASTR); crossed out when the key is off
      const onk = {};
      await openMod('REV'); await page.click('#rmon'); await page.waitForTimeout(60);
      onk.rev = await page.evaluate(() => ({ g: window.__impakt.state.g[136], key: document.getElementById('rmon').textContent, byp: document.getElementById('km-REV').classList.contains('byp') }));
      await page.click('#rmon');
      await openMod('DLY');
      onk.fxKeys = await page.evaluate(() => [...document.querySelectorAll('#ks-DLY .mon')].map(b => b.textContent).join(','));
      onk.fxStrip = await page.evaluate(() => document.querySelector('#ks-DLY .mlab').textContent);
      await page.click('#dmon'); await page.waitForTimeout(60);
      onk.dly = await page.evaluate(() => ({ g: window.__impakt.state.g[137], byp: document.getElementById('km-DLY').classList.contains('byp') }));
      await page.click('#dmon');
      await openMod('MIX'); await page.click('#mon'); await page.waitForTimeout(60);
      onk.mix = await page.evaluate(() => window.__impakt.state.g[139]);
      await page.click('#mon');
      onk.after = await page.evaluate(() => [136, 137, 138, 139].map(g => window.__impakt.state.g[g]).join());
      ok('ON keys: reverb, delay (on FX), MIX; a module or a page is crossed out when its key is off', onk.rev.g === 0 && onk.rev.key === 'OFF' && onk.rev.byp
        && onk.fxKeys === 'HOLD,ON' && onk.fxStrip === 'DELAYSTEREO' && onk.dly.g === 0 && onk.dly.byp
        && onk.mix === 0 && onk.after === '1,1,1,1', JSON.stringify(onk));
      await openMod('COMP');
      await openMod('DRV');
      const drvLabel = await page.evaluate(() => document.querySelectorAll('#gknobs .knob .lt')[4].textContent);
      ok('drive CHAR knob follows the model (TAPE -> IPS)', drvLabel === 'IPS', drvLabel);
      await seqPage();
      // presets: open the browser on T1 (back on BD·VA), 20 factory presets + INIT, load one, save it as a user preset
      await page.click('#mid'); await page.waitForTimeout(100);
      await page.locator('#mpick button[data-m=\"0\"]').click();
      await page.click('#pname');
      const pl = await page.evaluate(() => ({ items: document.querySelectorAll('#plist .pitem').length, tags: document.querySelectorAll('#ptags .ptchip').length, title: document.getElementById('ptitle').textContent }));
      ok('preset browser lists INIT + 20 presets with artist filters', pl.items === 21 && pl.tags > 5, JSON.stringify(pl));
      const h1 = await page.evaluate(() => window.__impakt.ui.pos.hits[0]);
      await page.locator('#plist .pitem', { hasText: 'ANALORD' }).first().click();
      await page.waitForTimeout(250);
      const lp = await page.evaluate(() => { const t = window.__impakt.state.tracks[0]; return { name: document.getElementById('ptxt').textContent, tag: document.getElementById('ptag').textContent, dec: t.p[2], vol: t.p[17], pre: t.pre, hits: window.__impakt.ui.pos.hits[0] }; });
      ok('loading a preset sets the sound and auditions it', lp.name.includes('ANALORD') && lp.tag === 'AFX' && Math.abs(lp.dec - 0.64) < 1e-6 && lp.vol > 0.1 && lp.vol <= 1 && lp.hits > h1, JSON.stringify(lp));
      await page.click('#pnext'); await page.waitForTimeout(100);
      const nx = await page.evaluate(() => document.getElementById('ptxt').textContent);
      ok('next preset button', nx.includes('ACRID'), nx);
      await page.fill('#pinput', 'mon kick'); await page.click('#psavebtn');
      const sv = await page.evaluate(() => ({ list: window.__impakt.userPresets.map(x => x.name), shown: [...document.querySelectorAll('#plist .pitem.user .pin')].map(e => e.textContent), stored: (localStorage.getItem('impakt.presets.user.v1') || '').includes('MON KICK') }));
      ok('user preset saved (list, browser, storage)', sv.list.includes('MON KICK') && sv.shown.includes('MON KICK') && sv.stored, JSON.stringify(sv));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-presets.png`, fullPage: true });
      await page.click('#pclose');
      // MIX page: labels, the live display receives data from the engine
      await openMod('MIX');
      await page.waitForTimeout(800);
      const mx = await page.evaluate(() => ({ labels: [...document.querySelectorAll('#gknobs .knob .lt')].map(e => e.textContent).join(','), cv: !!document.getElementById('mixcv'), n: (window.__impakt.ui.mix || []).length,
        g: window.__impakt.state.g.slice(50) }));
      ok('MIX page (unmask + calm) with live display', mx.labels === 'UNMK,KICK,CALM,SHRP,SELC,SPD,FOCS,DLTA' && mx.cv && mx.n === 96 && mx.g[0] === 0.5 && mx.g[2] === 0.35, JSON.stringify(mx));
      await page.screenshot({ path: `${shotDir}/impakt-${label}-mix.png`, fullPage: true });
      await seqPage();
      // v67: DICE gone: no module, no key; a project's DICE settings (kept in their places) play nothing
      const nd = await page.evaluate(() => ({ dice: !!document.getElementById('dicemod') || !!document.querySelector('#ptabs [data-tab="dc"]'), ng: window.__impakt.state.g.length }));
      ok('v67: no DICE (no page, no module), the project keeps its globals (190 since v89)', !nd.dice && nd.ng === 190, JSON.stringify(nd));
      await page.evaluate(() => window.__impakt.selectTrack(0));
      // logos in the picker
      const logos = await page.evaluate(() => { document.getElementById('mid').click(); return document.querySelectorAll('#mpick button svg').length; });
      ok('47 machine logos in the picker', logos === 47, String(logos));
      await page.evaluate(() => document.body.click());
      // persistence
      await page.waitForTimeout(600);
      const saved = await page.evaluate(() => { try { return !!localStorage.getItem('impakt.state.v4'); } catch (e) { return false; } });
      ok('state saved locally', saved);
      await page.reload(); await page.waitForTimeout(300);
      const up = await page.evaluate(() => window.__impakt.userPresets.map(x => x.name));
      ok('user preset survives a reload', up.includes('MON KICK'), JSON.stringify(up));
      await page.evaluate(() => localStorage.clear());
      // a project saved by version 1 is migrated (12 -> 18 machines, 33 -> 50 parameters)
      await page.evaluate(() => {
        const s = window.__impakt.demoState20();
        s.v = 1; s.g = s.g.slice(0, 10);
        s.tracks.forEach(t => { t.p = t.p.slice(0, 33); t.p[0] = (10 + 0.5) / 12; t.p[23] = (3 + 0.5) / 18; });
        localStorage.setItem('impakt.state.v1', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const mig = await page.evaluate(() => { const s = window.__impakt.state; return { v: s.v, np: s.tracks[0].p.length, m: Math.floor(s.tracks[0].p[0] * 64), d: Math.floor(s.tracks[0].p[23] * 256), ng: s.g.length, alg: Math.floor(s.g[42] * 6) }; });
      ok('version-1 project migrated', mig.v === 26 && mig.np === 284 && mig.m === 9 && mig.d === 3 && mig.ng === 190 && mig.alg === 1, JSON.stringify(mig));
      await page.evaluate(() => localStorage.clear());
      // a project saved by version 2 (18 machines, µLOOP morphing 4 synth sources)
      await page.evaluate(() => {
        const s = window.__impakt.demoState20();
        s.v = 2; s.g = s.g.slice(0, 18);
        s.tracks.forEach(t => { t.p[0] = 0.5 / 18; t.p[33] = 0; t.p[20] = 0; });
        s.tracks.forEach(t => { t.steps.forEach(st => delete st.r); });
        s.tracks[0].p[0] = (17 + 0.5) / 18;                       // CZ-PD
        s.tracks[3].p[0] = (12 + 0.5) / 18; s.tracks[3].p[3] = 0.7; // µLOOP between ZAP and ACCORD -> ZAP
        s.tracks[3].steps[15].l = { 3: 0.95 };                      // -> ACCORD
        localStorage.setItem('impakt.state.v2', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const mig2 = await page.evaluate(() => { const s = window.__impakt.state; return { v: s.v, m0: Math.floor(s.tracks[0].p[0] * 64), m3: Math.floor(s.tracks[3].p[0] * 64), src: Math.floor(s.tracks[3].p[3] * 40), lsrc: Math.floor(s.tracks[3].steps[15].l[3] * 40), ng: s.g.length }; });
      ok('version-2 project migrated', mig2.v === 26 && mig2.ng === 190 && mig2.m0 === 15 && mig2.m3 === 11 && mig2.src === 2 && mig2.lsrc === 3, JSON.stringify(mig2));
      await page.evaluate(() => localStorage.clear());
      // a project saved by version 3 that only used the spectral send: it becomes the SPECTRAL algorithm
      await page.evaluate(() => {
        const s = window.__impakt.demoState20();
        s.v = 3; delete s.scale; delete s.qroot;
        s.g = s.g.slice(0, 42); s.g[10] = 0.7; s.g[15] = 0.9; s.g[17] = 0.55;   // STIM, FREEZE on, LVL
        s.tracks.forEach(t => { t.p[0] = (Math.floor(t.p[0] * 64) + 0.5) / 19; t.p[20] = 0; t.p[33] = 0; delete t.q; delete t.rx; t.steps.forEach(st => { delete st.l[20]; delete st.l[33]; }); });
        s.tracks[3].p[33] = 0.4; s.tracks[6].steps[3].l[33] = 0.8;
        s.tracks[4].p[0] = (17 + 0.5) / 19;                                       // CZ-PD (v3 had no ITERA)
        localStorage.setItem('impakt.state.v3', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const mig3 = await page.evaluate(() => { const s = window.__impakt.state; return { v: s.v, ng: s.g.length, alg: Math.floor(s.g[42] * 6), tim: s.g[43], mode: Math.floor(s.g[44] * 4), lvl: s.g[47],
        rev3: s.tracks[3].p[20], rnd3: s.tracks[3].p[33], lk: s.tracks[6].steps[3].l, m4: Math.floor(s.tracks[4].p[0] * 64), scale: s.scale, q: s.tracks[0].q }; });
      ok('version-3 project migrated (spectral send -> SPECTRAL algorithm)', mig3.v === 26 && mig3.ng === 190 && mig3.alg === 3 && mig3.tim === 0.7 && mig3.mode === 1 && mig3.lvl === 0.55
        && mig3.rev3 === 0.4 && mig3.rnd3 === 0.5 && mig3.lk[20] === 0.8 && !(33 in mig3.lk) && mig3.m4 === 15 && mig3.scale === 0 && mig3.q === 1, JSON.stringify(mig3));
      await page.evaluate(() => localStorage.clear());
      // a project saved by version 4: 24 LFO destinations -> 27 (+ RTRG, RCNT, RVEL), no DICE yet
      await page.evaluate(() => {
        const s = window.__impakt.demoState20();
        s.v = 4; s.g = s.g.slice(0, 61); s.g[42] = (5 + 0.5) / 6;      // SPACE on the 6-algorithm grid
        s.tracks.forEach(t => { t.p[0] = (Math.floor(t.p[0] * 64) + 0.5) / 32; });
        s.tracks[2].p[23] = (17 + 0.5) / 24; s.tracks[2].p[39] = (23 + 0.5) / 24; s.tracks[2].steps[4].l[23] = (9 + 0.5) / 24;
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const mig4 = await page.evaluate(() => { const s = window.__impakt.state, t = s.tracks[2]; return { v: s.v, ng: s.g.length, d1: Math.floor(t.p[23] * 256), d2: Math.floor(t.p[39] * 256), lk: Math.floor(t.steps[4].l[23] * 256),
        dice: s.g[61], r0: Math.floor(s.g[78] * 9), dst: Math.floor(s.g[79] * 44), stp: s.g[72], alg: Math.floor(s.g[42] * 6), dv: s.g[67] }; });
      ok('version-4 project migrated (LFO destinations, DICE off)', mig4.v === 26 && mig4.ng === 190 && mig4.d1 === 17 && mig4.d2 === 23 && mig4.lk === 9 && mig4.dice === 0 && mig4.r0 === 0 && mig4.dst === 0 && Math.abs(mig4.stp - 0.6) < 1e-6 && mig4.alg === 5 && mig4.dv === 0.5, JSON.stringify(mig4));
      await page.evaluate(() => localStorage.clear());
      // a project saved by version 5: 6 reverb algorithms (ALGO keeps its algorithm), DEJA VU at LOCK unless a lock was on
      await page.evaluate(() => {
        const s = window.__impakt.demoState20();
        s.v = 5; s.g = s.g.slice(0, 99); s.tracks.forEach(t => { t.p[0] = (Math.floor(t.p[0] * 64) + 0.5) / 32; });
        s.g[42] = (3 + 0.5) / 6; s.g[67] = 0; s.g[69] = 0.5 / 4;
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const mig5 = await page.evaluate(() => { const s = window.__impakt.state; return { v: s.v, ng: s.g.length, alg: Math.floor(s.g[42] * 6), dv: s.g[67], pw: s.g[99], gtim: s.g[100] }; });
      ok('version-5 project migrated (DICE lock)', mig5.v === 26 && mig5.ng === 190 && mig5.alg === 3 && mig5.dv === 0.5 && mig5.pw === 0.5 && mig5.gtim === 0.08, JSON.stringify(mig5));
      await page.evaluate(() => localStorage.clear());
      // version 6: GRAIN was a 7th reverb algorithm; it moves to the master chain, the reverb goes back to HALL
      for (const [alg, want] of [[6, 'grain'], [2, 'spring']]) {
        await page.evaluate(a => {
          const s = window.__impakt.demoState20();
          s.v = 6; s.g = s.g.slice(0, 111); s.g[42] = (a + 0.5) / 7; s.tracks.forEach(t => { t.p[0] = (Math.floor(t.p[0] * 64) + 0.5) / 32; });
          localStorage.setItem('impakt.state.v4', JSON.stringify(s));
        }, alg);
        await page.reload(); await page.waitForTimeout(300);
        const mig6 = await page.evaluate(() => { const s = window.__impakt.state; return { v: s.v, ng: s.g.length, alg: Math.floor(s.g[42] * 6), gon: s.g[111], gmix: s.g[112], ott: s.g[127], eq: s.g[124] }; });
        ok(`version-6 project migrated (${want})`, mig6.v === 26 && mig6.ng === 190 && mig6.ott === 0 && mig6.eq === 1
          && (want === 'grain' ? mig6.alg === 4 && mig6.gon === 1 && mig6.gmix === 0.35 : mig6.alg === 2 && mig6.gon === 0), JSON.stringify(mig6));
        await page.evaluate(() => localStorage.clear());
      }
      {
      // a project saved by v38 (state v7): 27 LFO and 34 DICE destinations, PRISM's one-sided WDTH, the 3-position switch
      await page.evaluate(() => {
        const s = window.__impakt.demoState20();
        s.v = 7; s.tracks.forEach(t => { t.p = t.p.slice(0, 64); t.p[62] = 1; });   // (v38 stored the whole band as 1)
        s.tracks.forEach(t => { t.p[0] = (Math.floor(t.p[0] * 64) + 0.5) / 32; });
        const t4 = s.tracks[4]; t4.p[0] = (30 + 0.5) / 32; t4.p[8] = 0.5; t4.p[62] = 1; t4.p[58] = (15 + 0.5) / 32; t4.p[23] = (26 + 0.5) / 27; t4.steps[3].l[62] = 0.2;
        s.g[79] = (33 + 0.5) / 34;
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const mig7 = await page.evaluate(() => { const s = window.__impakt.state, t = s.tracks[4]; return { v: s.v, np: t.p.length, side: Math.floor(t.p[8] * 4), w: t.p[62], wl: t.steps[3].l[62],
        voic: Math.floor(t.p[58] * 16) + 1, d1: Math.floor(t.p[23] * 256), dice: Math.floor(s.g[79] * 44), wave: t.p[64], oct: t.p[66], w0: s.tracks[0].p[62] }; });
      ok('version-7 project migrated (PRISM v2, the new LFO / DICE destinations)', mig7.v === 26 && mig7.np === 284 && mig7.side === 2 && Math.abs(mig7.w - 0.5) < 1e-9
        && Math.abs(mig7.wl - 0.9) < 1e-9 && mig7.voic === 8 && mig7.d1 === 26 && mig7.dice === 33 && mig7.wave === 0 && mig7.oct === 0.5 && Math.abs(mig7.w0 - 0.5) < 1e-9, JSON.stringify(mig7));
      await page.evaluate(() => localStorage.clear());
      await page.reload(); await page.waitForTimeout(300);
      // a project saved by v39 (state v8): P_MACH on 32 slots, RUMBLE voices on 25..28 (a track, a lock, an RMB·4 track),
      // and a RUMBLE user preset: the tracks go to the nearest machine at its defaults, TUNE / DEC kept, the preset goes
      await page.evaluate(() => {
        const s = window.__impakt.demoState20();
        s.v = 8; s.rmb = 1;
        s.tracks.forEach(t => { t.p[0] = (Math.floor(t.p[0] * 64) + 0.5) / 32; });
        const t1 = s.tracks[1]; t1.p[0] = 25.5 / 32; t1.p[1] = 0.6; t1.p[2] = 0.3; t1.p[3] = 0.99; t1.steps[4].l = { 3: 0.1, 1: 0.7 };
        s.tracks[2].steps[5].l = { 0: 27.5 / 32, 3: 0.9 };
        s.tracks[5].p[0] = 28.5 / 32;
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
        localStorage.setItem('impakt.presets.user.v1', JSON.stringify([{ m: 25, name: 'OLD BOLT', p: { 1: 0.5 }, rmb: 1, dv: 8 }, { m: 0, name: 'KEPT KICK', p: { 1: 0.5 }, rmb: 1, dv: 8 }]));
      });
      await page.reload(); await page.waitForTimeout(300);
      const mig8 = await page.evaluate(() => { const s = window.__impakt.state, t1 = s.tracks[1], f = v => Math.floor(v * 64);
        return { v: s.v, m: s.tracks.map(t => f(t.p[0])), tune: t1.p[1], dec: t1.p[2], a: t1.p[3], lk: t1.steps[4].l, lm: f(s.tracks[2].steps[5].l[0]), la: s.tracks[2].steps[5].l[3],
          a5: s.tracks[5].p[3], u: window.__impakt.userPresets.map(x => x.name) }; });
      ok('version-8 project migrated (64 slots, RUMBLE -> HEX·BD / HEX·SD / HH·MT, presets)', mig8.v === 26 && mig8.m.join() === '0,23,4,11,17,4,37,18' && mig8.tune === 0.6 && mig8.dec === 0.3
        && mig8.a === 0.45 && !(3 in mig8.lk) && mig8.lk[1] === 0.7 && mig8.lm === 24 && mig8.la === 0.7 && mig8.a5 === 0.5 && mig8.u.join() === 'KEPT KICK', JSON.stringify(mig8));
      // a project saved by v40 / v41 (state v9): 71 parameters, LFO destinations on 35, DICE on 42: TONE flat, followers
      // off, the destinations kept
      await page.evaluate(() => {
        const s = window.__impakt.demoState20();
        s.v = 9;
        s.tracks.forEach(t => { t.p = t.p.slice(0, 71); });
        const t2 = s.tracks[2]; t2.p[23] = 17.5 / 35; t2.p[39] = 34.5 / 35; t2.steps[4].l = { 23: 9.5 / 35 };
        s.g[78] = 1.5 / 9; s.g[79] = 41.5 / 42;
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const mig9 = await page.evaluate(() => { const s = window.__impakt.state, t = s.tracks[2], P = window.__impakt.P, f = v => Math.floor(v * 256);
        return { v: s.v, np: t.p.length, d1: f(t.p[23]), d2: f(t.p[39]), lk: f(t.steps[4].l[23]), dice: Math.floor(s.g[79] * 44), hp: t.p[P.HPF], lp: t.p[P.LPF], mid: t.p[P.MID],
          e1: f(t.p[P.E1DS]), e2: f(t.p[P.E2DS]), gn: t.p[P.E1GN] }; });
      ok('version-9 project migrated (TONE flat, followers off, LFO destinations 35 -> 38, DICE 42 -> 44)', mig9.v === 26 && mig9.np === 284 && mig9.d1 === 17 && mig9.d2 === 34
        && mig9.lk === 9 && mig9.dice === 41 && mig9.hp === 0 && mig9.lp === 1 && mig9.mid === 0.5 && mig9.e1 === 0 && mig9.e2 === 0 && mig9.gn === 0.25, JSON.stringify(mig9));
      // a project saved by v42 .. v44 (state v10): 140 globals; GLITCH arrives off, its r tracks from the r keys
      await page.evaluate(() => {
        const s = window.__impakt.demoState20();
        s.v = 10; s.g = s.g.slice(0, 140); s.tracks[3].rx = 1; s.tracks[6].rx = 1;
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const mig10 = await page.evaluate(() => { const s = window.__impakt.state, G = window.__impakt.G;
        return { v: s.v, ng: s.g.length, on: s.g[G.XON], rnd: s.g[G.XRND], mix: s.g[G.XMIX], msk: Math.round(s.g[G.XMSK] * 255) }; });
      ok('version-10 project migrated (GLITCH off, RAND on the r tracks)', mig10.v === 26 && mig10.ng === 190 && mig10.on === 0 && mig10.rnd === 0 && mig10.mix === 1
        && mig10.msk === (255 & ~(1 << 3) & ~(1 << 6)), JSON.stringify(mig10));
      // v11 -> v12: RAND (now LIVE) only played with GLITCH on; RESO 35 %, DIG, CASS
      for (const on of [0, 1]) {
        await page.evaluate(on => {
          const I = window.__impakt, s = I.demoState20();
          s.v = 11; s.g = s.g.slice(0, 154); s.g[I.G.XON] = on; s.g[I.G.XRND] = 0.4;
          localStorage.setItem('impakt.state.v4', JSON.stringify(s));
        }, on);
        await page.reload(); await page.waitForTimeout(300);
        const m11 = await page.evaluate(() => { const s = window.__impakt.state, G = window.__impakt.G;
          return { v: s.v, ng: s.g.length, rnd: s.g[G.XRND], res: s.g[G.XRES], ana: s.g[G.XANA], amd: Math.floor(s.g[G.XAMD] * 5), live: document.getElementById('rlivev').textContent }; });
        ok(`version-11 project migrated (GLITCH ${on ? 'on: LIVE keeps' : 'off: LIVE at 0'})`, m11.v === 26 && m11.ng === 190 && m11.rnd === (on ? 0.4 : 0) && m11.res === 0.35 && m11.ana === 0 && m11.amd === 0
          && m11.live === (on ? '40%' : '0%'), JSON.stringify(m11));
      }
      await page.evaluate(() => {
        const I = window.__impakt, s = I.demoState20();
        s.v = 13; s.tracks.forEach(t => { t.p = t.p.slice(0, 102); });
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const m13 = await page.evaluate(() => { const s = window.__impakt.state, P = window.__impakt.P, t = s.tracks[3].p;
        return { v: s.v, np: t.length, rnd: t[P.BRND], skp: t[P.BSKP], seed: Math.floor(t[P.BSED] * 33), swp: t[P.BSWP] }; });
      ok('version-13 project migrated (BREAK: its CHOP side at rest)', m13.v === 26 && m13.np === 284 && m13.rnd === 0 && m13.skp === 0 && m13.seed === 0 && m13.swp === 0, JSON.stringify(m13));
      // v14: BREAK on its own breaks (BRK, KIT, the ghost notes' weight, MODE at the end): slot 1, SYNC / VARI from MODE
      await page.evaluate(() => {
        const I = window.__impakt, s = I.demoState20(), P = I.P;
        s.v = 14; const t = s.tracks[2];
        t.p[P.MACH] = (34 + 0.5) / 64; t.p[P.A] = 0.9; t.p[P.B] = 0.6; t.p[106] = 0.5; t.p[108] = 0.75;
        t.steps[0].l = { [P.A]: 0.3, [P.B]: 0.1, [106]: 0.9, [108]: 0.25, [P.C]: 0.2 };
        s.tracks[4].p[106] = 0.5; s.tracks[4].p[108] = 0.25;
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const m14 = await page.evaluate(() => { const s = window.__impakt.state, P = window.__impakt.P, t = s.tracks[2], l = t.steps[0].l;
        return { v: s.v, a: t.p[P.A], b: t.p[P.B], skp: t.p[106], swp: t.p[108], lk: Object.keys(l).sort().join(), lb: l[P.B], lc: l[P.C], o: [s.tracks[4].p[106], s.tracks[4].p[108]].join() }; });
      ok('version-14 project migrated (BREAK: slot 1, MODE kept, no SKIP / SWAP, its locks)', m14.v === 26 && m14.a === 0.05 && m14.b === 0.75 && m14.skp === 0 && m14.swp === 0
        && m14.lk === '4,5' && m14.lb === 0.25 && m14.lc === 0.2 && m14.o === '0,0', JSON.stringify(m14));
      // v15: no FX machines: their MORE side arrives at 0 (as the engine starts it)
      await page.evaluate(() => {
        const I = window.__impakt, s = I.demoState20();
        s.v = 15; s.tracks.forEach(t => { t.p = t.p.slice(0, 109); });
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const m15 = await page.evaluate(() => { const s = window.__impakt.state, P = window.__impakt.P;
        return { v: s.v, np: s.tracks.map(t => t.p.length).join(), xs: s.tracks.flatMap(t => [t.p[P.XA], t.p[P.XB], t.p[P.XC], t.p[P.XD], t.p[P.XE], t.p[P.XF], t.p[P.XG]]).every(x => x === 0) }; });
      ok('version-15 project migrated (the FX machines\' MORE side at 0)', m15.v === 26 && m15.np === '284,284,284,284,284,284,284,284' && m15.xs, JSON.stringify(m15));
      // v17 -> v18: 16 steps padded to 64, DEC at its top on a synth keeps its decay (0.985), RAM's HOLD kept; a v18
      // project keeps its patterns (packed when saved: sparse steps), the one playing, its chain
      await page.evaluate(() => {
        const I = window.__impakt, P = I.P, s = I.demoState20();
        s.v = 17; s.tracks.forEach(t => { t.steps = t.steps.slice(0, 16); }); delete s.pats; delete s.pat; delete s.chain;
        s.tracks[7].p[P.DEC] = 1; s.tracks[7].steps[3].l[P.DEC] = 1;                // TRIO
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const m17 = await page.evaluate(() => { const I = window.__impakt, s = I.state, P = I.P;
        return { v: s.v, ns: s.tracks.map(t => t.steps.length).join(), dec: s.tracks[7].p[P.DEC], ldec: s.tracks[7].steps[3].l[P.DEC], pats: s.pats.length, pat: s.pat }; });
      ok('version-17 project migrated (64 steps, DEC at its top keeps its decay, the patterns empty)', m17.v === 26 && m17.ns === '64,64,64,64,64,64,64,64' && m17.dec === 0.985 && m17.ldec === 0.985 && m17.pats === 128 && m17.pat === 0, JSON.stringify(m17));
      await page.evaluate(() => {
        const I = window.__impakt, s = I.demoState20();
        const pt = { chg: 32, tracks: s.tracks.map(t => ({ p: t.p.slice(), len: 32, spd: t.spd, q: t.q, pre: null, steps: t.steps.map((st, i) => i === 20 ? { on: 1, l: { 3: 0.25 }, r: 3 } : { on: 0, l: {} }) })) };
        s.pats[9] = pt; s.pat = 0; s.chain = [0, 9]; s.pmode = 1;
        const json = JSON.stringify(Object.assign({}, s, { pats: s.pats.map(p => p && { chg: p.chg, tr: p.tracks.map(t => ({ p: t.p, len: t.len, spd: t.spd, q: t.q, st: Object.fromEntries(t.steps.map((st, i) => [i, st]).filter(([i, st]) => st.on)) })) }) }));
        localStorage.setItem('impakt.state.v4', json);
      });
      await page.reload(); await page.waitForTimeout(300);
      const m18 = await page.evaluate(() => { const I = window.__impakt, s = I.state, pt = s.pats[9];
        const packed = JSON.parse(I.serialState()).pats[9];
        return { v: s.v, has: !!pt, chg: pt && pt.chg, len: pt && pt.tracks[0].len, st: pt && pt.tracks[2].steps[20], n: pt && pt.tracks[2].steps.length, chain: s.chain.join(), mode: s.pmode,
          packed: packed && Object.keys(packed.tr[2].st).join() }; });
      // v18 -> v19: a HALO track's PROG (WALK, last of 13) and its switch (VOICE, of 2) onto the new grids, its locks too
      await page.evaluate(() => {
        const I = window.__impakt, s = I.demoState20(), P = I.P, t = s.tracks[3];
        t.p = t.p.slice(0, 119); t.p[P.MACH] = (33 + 0.5) / 64; t.p[P.B] = (12 + 0.5) / 13; t.p[P.F] = (1 + 0.5) / 2;
        t.steps[4] = { on: 1, l: { [P.B]: (1 + 0.5) / 13 } };
        s.tracks.forEach(x => { x.p = x.p.slice(0, 119); });
        s.v = 18; s.pats = []; s.chain = [];
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const m19 = await page.evaluate(() => { const I = window.__impakt, s = I.state, P = I.P, t = s.tracks[3];
        return { v: s.v, np: t.p.length, prog: Math.floor(t.p[P.B] * 28), side: Math.floor(t.p[P.F] * 4), lk: Math.floor(t.steps[4].l[P.B] * 28), harm: Math.floor(t.p[P.HARM] * 3), cut: t.p[P.HCUT] }; });
      ok('version-18 project migrated (HALO: PROG WALK stays WALK, VOICE stays VOICE, its locks; HARM FREE, TONE at rest)', m19.v === 26 && m19.np === 284 && m19.prog === 27
        && m19.side === 1 && m19.lk === 1 && m19.harm === 0 && m19.cut === 1, JSON.stringify(m19));
      // v19 -> v20: HALO's and STRUM's switch a fourth side (AGE): their F (TONE, STRING) and a lock onto the new grid;
      // AGE at rest, STRUM's AIR off (what v59 played)
      await page.evaluate(() => {
        const I = window.__impakt, s = I.demoState20(), P = I.P;
        s.tracks.forEach(x => { x.p = x.p.slice(0, 150); });
        const h = s.tracks[3], st = s.tracks[5];
        h.p[P.MACH] = (33 + 0.5) / 64; h.p[P.F] = (2 + 0.5) / 3; h.steps[6] = { on: 1, l: { [P.F]: (1 + 0.5) / 3 } };
        st.p[P.MACH] = (40 + 0.5) / 64; st.p[P.F] = (2 + 0.5) / 3;
        s.v = 19; s.pats = []; s.chain = [];
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const m20 = await page.evaluate(() => { const I = window.__impakt, s = I.state, P = I.P, h = s.tracks[3], st = s.tracks[5];
        return { v: s.v, np: h.p.length, hs: Math.floor(h.p[P.F] * 4), hl: Math.floor(h.steps[6].l[P.F] * 4), ss: Math.floor(st.p[P.F] * 4),
          age: [P.GDTN, P.GVAR, P.GSHN, P.GDRP, P.SAIR, P.S1, P.S6].map(q => st.p[q]).join() }; });
      ok('version-19 project migrated (HALO on TONE, STRUM on STRING: the four sides; AGE at rest, STRUM\'s AIR off)', m20.v === 26 && m20.np === 284 && m20.hs === 2
        && m20.hl === 1 && m20.ss === 2 && m20.age === '0,0,0,0,0,0,0', JSON.stringify(m20));
      ok('version-18 project: its patterns kept (packed: sparse steps), its chain and change mode', m18.v === 26 && m18.has && m18.chg === 32 && m18.len === 32 && m18.n === 64 && m18.st.on === 1 && m18.st.r === 3 && m18.st.l[3] === 0.25
        && m18.chain === '0,9' && m18.mode === 1 && m18.packed === '20', JSON.stringify(m18));
      // v20 -> v21 (v61) and v22 (v62): the machines renumbered; MODAL, STRING, WT·DRM, WT·GRL and WT·ZAP onto ALLOY (A..F
      // and the sides at its defaults, the old A..F locks gone), a lock onto WT·GRL an ALLOY lock (its sides locked at
      // theirs), the MORE page and its locks gone; HALO and CHOIR on their new numbers; a MODAL user preset gone, a HALO one moved
      await page.evaluate(() => {
        const I = window.__impakt, s = I.demoState20(), P = I.P;
        const ms = [9, 14, 28, 31, 32, 33, 41, 0];
        s.tracks.forEach((t, i) => { t.p = t.p.slice(0, 161); t.p[P.MACH] = (ms[i] + 0.5) / 64; for (let q = 155; q < 161; q++) t.p[q] = 0.6; t.p[P.A] = 0.97; });
        s.tracks[0].steps[2] = { on: 1, l: { [P.A]: 0.11, 155: 0.8, [P.TUNE]: 0.6 } };
        s.tracks[7].steps[5] = { on: 1, l: { [P.MACH]: (31 + 0.5) / 64, [P.B]: 0.2 } };
        s.tracks[5].pre = { m: 33, kind: 'u', name: 'HALO PAD' }; s.tracks[0].pre = { m: 9, kind: 'f', name: 'GAMELAN' };
        s.v = 20; s.pats = []; s.chain = [];
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
        localStorage.setItem('impakt.presets.user.v1', JSON.stringify([{ m: 9, name: 'MODAL KEEP', p: { 3: 0.5 }, dv: 20 }, { m: 33, name: 'HALO PAD', p: { 3: 0.2, 155: 0.7 }, dv: 20 }]));
      });
      await page.reload(); await page.waitForTimeout(300);
      const m21 = await page.evaluate(() => { const I = window.__impakt, s = I.state, P = I.P, f = v => Math.floor(v * 64), mt = I.ALLOY_M, lx = I.ALLOY_M;
        const sd = m => I.sideDefs(m).map(v => Math.round(v * 1000) / 1000).join(), sOf = t => [...Array(70).keys()].map(i => Math.round(t.p[i < 35 ? P.S1 + i : P.S36 + i - 35] * 1000) / 1000).join();
        const l0 = s.tracks[0].steps[2].l, l7 = s.tracks[7].steps[5].l;
        return { v: s.v, np: s.tracks[0].p.length, m: s.tracks.map(t => f(t.p[0])).join(), a: s.tracks.map(t => t.p[P.A]).join(),
          sm: [0, 1, 2].every(i => sOf(s.tracks[i]) === sd(mt)), sl: [3, 4].every(i => sOf(s.tracks[i]) === sd(lx)), s0: sOf(s.tracks[5]) === sd(0) && sOf(s.tracks[7]) === sd(0),
          l0: Object.keys(l0).sort((a, b) => a - b).join(), l7m: f(l7[P.MACH]), l7a: l7[P.A], l7b: l7[P.B], l7s: Math.round(l7[P.S2] * 1000) / 1000, lxs2: Math.round(I.sideDefs(lx)[1] * 1000) / 1000,
          pre5: s.tracks[5].pre && s.tracks[5].pre.m, pre0: s.tracks[0].pre, u: I.userPresets.map(x => x.m + ':' + x.name).join() }; });
      ok('version-20 project migrated (v61, v62: the machines renumbered, the five gone onto ALLOY at its defaults, the MORE page gone, presets)',
        m21.v === 26 && m21.np === 284 && m21.m === '37,37,37,37,37,28,36,0' && m21.a === '0.1875,0.1875,0.1875,0.1875,0.1875,0.97,0.97,0.97' && m21.sm && m21.sl && m21.s0
        && m21.l0 === '1' && m21.l7m === 37 && m21.l7a === 0.1875 && m21.l7b === 0.5 && m21.l7s === m21.lxs2 && m21.pre5 === 28 && m21.pre0 === null && m21.u === '28:HALO PAD', JSON.stringify(m21));
      // v21 -> v22 (v62): MATTER (37) and LATEX (38) both onto ALLOY at its defaults (A..F, the sides), the A..F and side
      // locks made for them gone, a step locked onto MATTER on another machine's track: ALLOY with its sides as locks; a
      // MATTER user preset gone, a HALO one kept, the track's MATTER preset forgotten
      await page.evaluate(() => {
        const I = window.__impakt, s = I.demoState21(), P = I.P;
        s.tracks[6].steps[1] = { on: 1, l: { [P.A]: 0.11, [P.S3]: 0.8, [P.TUNE]: 0.6 } };
        s.pats = []; s.chain = [];
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
        localStorage.setItem('impakt.presets.user.v1', JSON.stringify([{ m: 37, name: 'MATTER KEEP', p: { 3: 0.5 }, dv: 21 }, { m: 28, name: 'HALO PAD', p: { 3: 0.2 }, dv: 21 }]));
      });
      await page.reload(); await page.waitForTimeout(300);
      const m22 = await page.evaluate(() => { const I = window.__impakt, s = I.state, P = I.P, f = v => Math.floor(v * 64), al = I.ALLOY_M;
        const sd = m => I.sideDefs(m).map(v => Math.round(v * 1000) / 1000).join(), sOf = t => [...Array(70).keys()].map(i => Math.round(t.p[i < 35 ? P.S1 + i : P.S36 + i - 35] * 1000) / 1000).join();
        const l6 = s.tracks[6].steps[1].l, l5 = s.tracks[4].steps[3].l;
        return { v: s.v, m: s.tracks.map(t => f(t.p[0])).join(), a5: s.tracks[5].p[P.A], a6: s.tracks[6].p[P.A], s56: [5, 6].every(i => sOf(s.tracks[i]) === sd(al)),
          l6: Object.keys(l6).sort((a, b) => a - b).join(), l5m: f(l5[P.MACH]), l5a: l5[P.A], l5s5: Math.round(l5[P.S5] * 1000) / 1000, sd5: Math.round(I.sideDefs(al)[4] * 1000) / 1000,
          pre6: s.tracks[6].pre, u: I.userPresets.map(x => x.m + ':' + x.name).join() }; });
      ok('version-21 project migrated (v62: MATTER and LATEX onto ALLOY at its defaults, their A..F and side locks gone, a MATTER lock an ALLOY one, presets)',
        m22.v === 26 && m22.m === '0,3,4,11,17,37,37,18' && m22.a5 === 0.1875 && m22.a6 === 0.1875 && m22.s56 && m22.l6 === '1' && m22.l5m === 37 && m22.l5a === 0.1875
        && m22.l5s5 === m22.sd5 && m22.pre6 === null && m22.u === '28:HALO PAD', JSON.stringify(m22));
      // v22 -> v23 (v63): ALLOY's 21 sides onto its 35 (where each went; BNCE bipolar, its bounces on the right; PENV's
      // under-range three octaves now, the same pitch; the new ones at rest, NCOL from MALT, no reverb); its locks moved;
      // a step locked onto ALLOY from another machine: its locks moved, the new ones at rest; an ALLOY user preset of v22
      // moved, the track's parameters as long as today's
      await page.evaluate(() => {
        const I = window.__impakt, s = I.demoState22(), P = I.P;
        s.tracks[6].steps[2] = { on: 1, l: { [P.S5]: 0.2, [P.S21]: 0.75 } };
        s.pats = []; s.chain = [];
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
        localStorage.setItem('impakt.presets.user.v1', JSON.stringify([{ m: 37, name: 'OLD ALLOY', p: { 3: 0.5, [P.S1]: 0.4, [P.S7]: 0.2, [P.S13]: 0.6 }, dv: 22 }]));
      });
      await page.reload(); await page.waitForTimeout(300);
      const m23 = await page.evaluate(() => { const I = window.__impakt, s = I.state, P = I.P, r = v => v === undefined ? 'none' : Math.round(v * 10000) / 10000;
        const t = s.tracks[6], l = t.steps[2].l, k = s.tracks[4].steps[3].l, u = I.userPresets.find(x => x.name === 'OLD ALLOY');
        return { v: s.v, np: t.p.length, m: Math.floor(t.p[P.MACH] * 64), malt: r(t.p[P.S1]), clik: r(t.p[P.S2]), penv: r(t.p[P.S3]), bnce: r(t.p[P.S6]), nois: r(t.p[P.S8]),
          ncol: r(t.p[P.S10]), nlen: r(t.p[P.S11]), dac: r(t.p[P.S14]), obj: r(t.p[P.S15]), menv: r(t.p[P.S21]), verb: r(t.p[P.S29]), room: r(t.p[P.S30]), rate: r(t.p[P.S35]),
          lpenv: r(l[P.S3]), ldac: r(l[P.S14]), lold: r(l[P.S5]), kb: r(k[P.S6]), kr: r(k[P.S35]), km: r(k[P.S21]), kn: r(k[P.S10]), kold: r(k[P.S7]),
          um: u && u.m, udv: u && u.dv, umalt: u && r(u.p[P.S1]), ubn: u && r(u.p[P.S6]), uadd: u && r(u.p[P.S22]), uverb: u && r(u.p[P.S29]) }; });
      ok('version-22 project migrated (v63: ALLOY\'s sides onto its five pages, BNCE and PENV on their new ranges, the new ones at rest; its locks; a lock onto ALLOY; a v22 preset)',
        m23.v === 26 && m23.np === 284 && m23.m === 37 && m23.malt === 0.6 && m23.clik === 0 && m23.penv === 0.3 && m23.bnce === 0.75 && m23.nois === 0.7 && m23.ncol === 0.65
        && m23.nlen === 0.4 && m23.dac === 0.3125 && m23.obj === 1 && m23.menv === 0.5 && m23.verb === 0 && m23.room === 0.3 && m23.rate === 0
        && m23.lpenv === 0.3 && m23.ldac === 0.75 && m23.lold === 'none' && m23.kb === 0.7 && m23.kr === 0.9 && m23.km === 0.5 && m23.kn === 0.7625 && m23.kold === 0
        && m23.um === 37 && m23.udv === 26 && m23.umalt === 0.4 && m23.ubn === 0.6 && m23.uadd === 0.6 && m23.uverb === 0, JSON.stringify(m23));
      // v23 -> v24 (v64): RPAN at its centre, no spread on any track (a SYNTH one too: as it sounded), MONO at its middle
      await page.evaluate(() => {
        const I = window.__impakt, s = I.demoState24();
        for (const t of s.tracks) t.p.length = 190;
        s.v = 23; s.pats = []; s.chain = [];
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
      });
      await page.reload(); await page.waitForTimeout(300);
      const m24 = await page.evaluate(() => { const I = window.__impakt, s = I.state, P = I.P;
        return { v: s.v, np: s.tracks.map(t => t.p.length).join(), rp: s.tracks.every(t => t.p[P.RPAN] === 0.5), sp: s.tracks.every(t => t.p[P.SPRD] === 0),
          mo: s.tracks.every(t => t.p[P.SMON] === 0.5), synth: s.tracks.map(t => I.MACH[Math.floor(t.p[0] * 64)].id).filter(id => ['FM·X', 'SY·RAW', 'TRIO'].includes(id)).length }; });
      ok('version-23 project migrated (v64: RPAN centred, no spread, a SYNTH track too, MONO at its middle)',
        m24.v === 26 && m24.np === '284,284,284,284,284,284,284,284' && m24.rp && m24.sp && m24.mo && m24.synth >= 1, JSON.stringify(m24));
      // v24 -> v25 (v65): the LFO waves onto the 16 (S&H·T stays S&H·T, EXP stays EXP, EXP·T in a lock too), RDEC off, SLW and
      // LEN at rest; a user preset of v64 with a TRI·T keeps it
      await page.evaluate(() => {
        const I = window.__impakt, P = I.P, s = I.demoState24(), m = (i, n) => (i + 0.5) / n;
        s.tracks[0].p[P.LWAV] = m(10, 12); s.tracks[0].p[P.L2WV] = m(4, 12); s.tracks[1].p[P.LWAV] = m(5, 12);
        s.tracks[0].steps[2] = { on: 1, l: { [P.LWAV]: m(11, 12) } };
        s.pats = []; s.chain = [];
        localStorage.setItem('impakt.state.v4', JSON.stringify(s));
        localStorage.setItem('impakt.presets.user.v1', JSON.stringify([{ name: 'OLD LFO', m: 0, dv: 24, p: { [P.LWAV]: m(7, 12), [P.DEC]: 0.4 } }]));
      });
      await page.reload(); await page.waitForTimeout(300);
      const m25 = await page.evaluate(() => { const I = window.__impakt, s = I.state, P = I.P, r = x => Math.round(x * 1e6) / 1e6, u = I.userPresets.find(x => x.name === 'OLD LFO');
        return { v: s.v, np: s.tracks.map(t => t.p.length).join(), w0: r(s.tracks[0].p[P.LWAV] * 16 - 0.5), w2: r(s.tracks[0].p[P.L2WV] * 16 - 0.5),
          w1: r(s.tracks[1].p[P.LWAV] * 16 - 0.5), lk: r(s.tracks[0].steps[2].l[P.LWAV] * 16 - 0.5), rd: s.tracks.every(t => t.p[P.RDEC] === 0.5),
          sl: s.tracks.every(t => t.p[P.L1SL] === 0.5 && t.p[P.L2SL] === 0.5), ln: s.tracks.every(t => t.p[P.L1LN] === 0.65 && t.p[P.L2LN] === 0.65),
          name: I.paramMeta(0, P.LWAV).f(s.tracks[0].p[P.LWAV]), udv: u && u.dv, uw: u && r(u.p[P.LWAV] * 16 - 0.5) }; });
      ok('version-24 project migrated (v65: the LFO waves onto 16, S&H·T and EXP kept, an EXP·T lock too; RDEC off, SLW 50 %, LEN 8; a user preset\'s TRI·T)',
        m25.v === 26 && m25.np === '284,284,284,284,284,284,284,284' && m25.w0 === 12 && m25.w2 === 4 && m25.w1 === 5 && m25.lk === 13 && m25.rd && m25.sl && m25.ln
        && m25.name === 'S&H·T' && m25.udv === 26 && m25.uw === 9, JSON.stringify(m25));
      await page.evaluate(() => localStorage.clear());
      await page.reload(); await page.waitForTimeout(300);
      // presets: save the track's sound, make it the machine's default (the picker then starts the machine with it),
      // rename it (the default follows), delete it (the default goes)
      const lsGet = k => page.evaluate(k => JSON.parse(localStorage.getItem(k) || 'null'), k);
      await page.evaluate(() => window.__impakt.selectTrack(0));
      await page.click('#pname');
      await page.fill('#pinput', 'my kick'); await page.click('#psavebtn');
      const pr1 = await page.evaluate(() => ({ u: window.__impakt.userPresets.map(x => x.m + ':' + x.name), acts: !document.getElementById('pacts').hidden && [...document.querySelectorAll('#pacts .btn')].map(b => b.textContent).join(','),
        dec: window.__impakt.state.tracks[0].p[2] }));
      await page.click('#pdefb');
      const def1 = await lsGet('impakt.defaults.v1');
      await page.screenshot({ path: `${shotDir}/impakt-${label}-presets.png`, fullPage: true });
      await page.click('#pclose');
      await page.locator('#mknobs .knob').nth(1).focus(); for (let i = 0; i < 20; i++) await page.keyboard.press('ArrowRight');
      await page.click('#mid'); await page.locator('#mpick button[data-m="2"]').click();
      await page.click('#mid'); await page.locator('#mpick button[data-m="0"]').click();
      const bk = await page.evaluate(() => ({ dec: window.__impakt.state.tracks[0].p[2], pre: window.__impakt.state.tracks[0].pre }));
      await page.click('#pname'); await page.click('#pren'); await page.fill('#pinput', 'kick two'); await page.click('#psavebtn');
      const def2 = await lsGet('impakt.defaults.v1');
      const u2 = await page.evaluate(() => window.__impakt.userPresets.map(x => x.name).join());
      await page.click('#pdel'); await page.click('#pdel');
      const def3 = await lsGet('impakt.defaults.v1');
      const u3 = await page.evaluate(() => window.__impakt.userPresets.length);
      await page.click('#pclose');
      ok('preset manager: save, default for the machine, rename, delete', pr1.u.join() === '0:MY KICK' && pr1.acts === '☆ Default,Update,Rename,Delete'
        && def1 && def1.sound['BD·VA'] && def1.sound['BD·VA'].name === 'MY KICK' && Math.abs(bk.dec - pr1.dec) < 1e-9 && bk.pre.name === 'MY KICK'
        && u2 === 'KICK TWO' && def2.sound['BD·VA'].name === 'KICK TWO' && u3 === 0 && !def3.sound['BD·VA'], JSON.stringify({ pr1, def1, bk, def2, u2, def3, u3 }));
      // kits: save the 8 sounds + master, change a sound and a trig, load the kit back (the sound returns, the pattern
      // stays); with + PATTERN the trigs come too; the default kit is what Init starts from (on an empty pattern)
      await proj('kitsbtn');
      await page.fill('#kinput', 'kit a'); await page.click('#ksavebtn');
      const vol0 = await page.evaluate(() => window.__impakt.state.tracks[2].p[17]);
      await page.evaluate(() => { window.__impakt.selectTrack(2); });
      await openMod('AMP');
      await page.locator('#k-AMP .knob').nth(0).focus(); for (let i = 0; i < 15; i++) await page.keyboard.press('ArrowLeft');
      await seqPage();
      await page.locator('.step').nth(1).dispatchEvent('pointerdown', { pointerId: 1, isPrimary: true });
      const kch = await page.evaluate(() => ({ vol: window.__impakt.state.tracks[2].p[17], on: window.__impakt.state.tracks[2].steps[1].on }));
      await page.locator('#klist .kitem').nth(0).click(); const armedTxt = await page.locator('#klist .kitem').nth(0).textContent();
      await page.locator('#klist .kitem').nth(0).click();
      const ld = await page.evaluate(() => ({ vol: window.__impakt.state.tracks[2].p[17], on: window.__impakt.state.tracks[2].steps[1].on }));
      await page.click('#kpat'); await page.fill('#kinput', 'kit b'); await page.click('#ksavebtn');
      const kits = await lsGet('impakt.kits.v1');
      await page.locator('#klist .kitem', { hasText: 'KIT A' }).click(); await page.locator('#klist .kitem', { hasText: 'LOAD?' }).click();
      await page.click('#kdefb');
      await page.screenshot({ path: `${shotDir}/impakt-${label}-kits.png`, fullPage: true });
      await proj('init'); await proj('init');
      const ini = await page.evaluate(() => ({ vol: window.__impakt.state.tracks[2].p[17], trigs: window.__impakt.state.tracks.reduce((n, t) => n + t.steps.filter(s => s.on).length, 0) }));
      await page.locator('#klist .kitem', { hasText: 'KIT B' }).click(); await page.locator('#klist .kitem', { hasText: 'LOAD?' }).click();
      const kitB = await page.evaluate(() => ({ trigs: window.__impakt.state.tracks.reduce((n, t) => n + t.steps.filter(s => s.on).length, 0), on: window.__impakt.state.tracks[2].steps[1].on }));
      const defk = await lsGet('impakt.defaults.v1');
      ok('kits: save, load (the sounds; the pattern only with + PATTERN), the default kit for Init',
        Math.abs(kch.vol - vol0) > 0.05 && armedTxt.includes('LOAD?') && Math.abs(ld.vol - vol0) < 1e-4 && ld.on === kch.on
        && kits.map(k => k.name + ':' + k.pat).join() === 'KIT B:1,KIT A:0' && defk.kit === 'KIT A' && Math.abs(ini.vol - vol0) < 1e-4 && ini.trigs === 0
        && kitB.trigs > 10 && kitB.on === kch.on, JSON.stringify({ vol0, kch, armedTxt, ld, kits: kits.map(k => k.name + ':' + k.pat), defk, ini, kitB }));
      await page.click('#kclose');
      await page.evaluate(() => localStorage.clear());
      }
    }
    const overflow = await page.evaluate(() => document.documentElement.scrollWidth > window.innerWidth + 1);
    ok(`${label}: no horizontal overflow`, !overflow);
    const height = await page.evaluate(() => document.documentElement.scrollHeight);
    results.push(`INFO ${label}: page height ${height}px`);
    await page.screenshot({ path: `${shotDir}/impakt-${label}.png`, fullPage: true });
    ok(`${label}: no console errors`, errors.length === 0, errors.join(' | '));
    await page.close();
  }
  // the plugin page (plugin/Source/ui, a stub JUCE backend): a 1280 x 720 panel scaled to the window, five pages
  // that each fit, pads that only select while the sequence runs, the track cells on the track's screen
  {
    const pg = await browser.newPage({ viewport: { width: 1920, height: 1080 } });
    const perr = [];
    pg.on('pageerror', e => perr.push(String(e)));
    await pg.addInitScript(() => { const L = {}; window.__ops = [];
      window.__JUCE__ = { backend: { emitEvent(n, p) { if (n === 'impakt_ops') window.__ops.push(...p.ops); }, addEventListener(n, f) { L[n] = f; } } };
      window.__fire = (n, p) => L[n] && L[n](p); });
    await pg.goto('file://' + path.resolve(__dirname, '../plugin/Source/ui/index.html')); await pg.waitForTimeout(500);
    const vis = sel => pg.evaluate(sel => { const e = document.querySelector(sel); return !!e && e.getClientRects().length > 0 && getComputedStyle(e).visibility !== 'hidden'; }, sel);
    const fits = () => pg.evaluate(() => { const a = document.querySelector('.app'), r = a.getBoundingClientRect(), s = r.width / 1280; let out = 0;
      a.querySelectorAll('*').forEach(e => { if (!e.getClientRects().length || e.closest('[hidden]') || e.closest('.mpick')) return; const b = e.getBoundingClientRect();
        if (b.height && (b.bottom > r.bottom + 1 || b.right > r.right + 1)) out++; }); return { out, s: +s.toFixed(3), w: Math.round(r.width), h: Math.round(r.height) }; });
    const pl = { plug: await pg.evaluate(() => document.documentElement.classList.contains('plug')), keys: await pg.evaluate(() => [...document.querySelectorAll('#ptabs .ptab')].map(b => b.textContent).join(',')) };
    pl.ms = [await vis('#machmod'), await vis('#pads'), await vis('#steps'), await vis('#rknobs'), await vis('#qz'), await vis('#trkmod'), await vis('#km-FLT')];
    pl.msFit = await fits();
    // the machine picker: every family on the panel (two columns), each machine reachable under the pointer
    await pg.click('#mid'); await pg.waitForTimeout(150);
    pl.pick = await pg.evaluate(() => { const a = document.querySelector('.app').getBoundingClientRect(), bs = [...document.querySelectorAll('#mpick button')];
      const hit = b => { const r = b.getBoundingClientRect(), e = document.elementFromPoint((r.left + r.right) / 2, (r.top + r.bottom) / 2); return !!e && b.contains(e); };
      return { n: bs.length, out: bs.filter(b => { const r = b.getBoundingClientRect(); return r.bottom > a.bottom + 1 || r.right > a.right + 1; }).length, reach: bs.filter(hit).length,
        cols: new Set([...document.querySelectorAll('#mpick .pgrp')].map(g => Math.round(g.getBoundingClientRect().left))).size }; });
    await pg.screenshot({ path: `${shotDir}/impakt-plugin-picker.png` });
    await pg.keyboard.press('Escape'); await pg.evaluate(() => { window.__impakt.ui.picker = false; window.__impakt.renderAll(); }); await pg.waitForTimeout(100);
    await pg.screenshot({ path: `${shotDir}/impakt-plugin-seq.png` });
    await pg.click('#ptabs [data-tab="tn"]'); await pg.waitForTimeout(150);
    pl.tn = [await vis('#tstrip'), await vis('#km-FLT'), await vis('#km-TONE'), await vis('#km-AMP'), await vis('#steps')];
    pl.tnFit = await fits();
    // v70: TONE's four modules side by side (BOX with them, no tabs), the visualiser and its six handles over them
    pl.tn70 = await pg.evaluate(() => ({ box: getComputedStyle(document.getElementById('km-BOX')).display !== 'none' && !!document.querySelector('#kp-tn #km-BOX'), tabs: document.querySelectorAll('#kp-tn .rtabs').length,
      h: document.getElementById('tspec').clientHeight, view: window.__impakt.trackView(), n: window.__impakt.ui.tz ? window.__impakt.ui.tz.nodes.length : 0 }));
    await pg.screenshot({ path: `${shotDir}/impakt-plugin-tone.png` });
    await pg.click('#tdots i[data-t="5"]'); pl.cell = await pg.evaluate(() => window.__impakt.ui.t);
    await pg.click('#ptabs [data-tab="md"]'); await pg.waitForTimeout(150);
    pl.md = [await vis('#tstrip'), await vis('#km-LFO1'), await vis('#km-LFO2'), await vis('#km-EF1'), await vis('#km-EF2')];
    pl.mdFit = await fits();
    await pg.screenshot({ path: `${shotDir}/impakt-plugin-mod.png` });
    // v69: the plugin keeps MOD's four modules side by side (no tabs: they fit under the screen); OPTION's menu and the
    // kits panel on the panel
    pl.mdTabs = await pg.evaluate(() => document.querySelectorAll('#kp-md .rtabs').length);
    await pg.click('#projbtn'); await pg.waitForTimeout(80);
    pl.opt = { menu: await vis('#projrow'), fit: await fits() };
    await pg.screenshot({ path: `${shotDir}/impakt-plugin-option.png` });
    await pg.click('#kitsbtn'); await pg.waitForTimeout(80);
    pl.opt.kits = await vis('#kpanel'); pl.opt.kfit = await fits();
    await pg.click('#kclose'); await pg.waitForTimeout(60);
    await pg.click('#ptabs [data-tab="fx"]'); await pg.waitForTimeout(150);
    pl.fx = [await vis('#tstrip'), await vis('#km-BOX'), await vis('#km-SEND'), await vis('#ks-REV'), await vis('#ks-DLY')];
    pl.fxFit = await fits();
    await pg.screenshot({ path: `${shotDir}/impakt-plugin-fx.png` });
    // v68: each return under its tab, each fits (DELAY, CHORUS, PHASER: their screens)
    pl.rets = {};
    for (const r of ['DLY', 'CHO', 'PHS']) {
      await pg.click(`#kp-fx .kret.cur .rtab[data-ret="${r}"]`); await pg.waitForTimeout(120);
      pl.rets[r] = { vis: await vis('#ks-' + r), fit: await fits() };
      await pg.screenshot({ path: `${shotDir}/impakt-plugin-fx-${r.toLowerCase()}.png` });
    }
    await pg.click('#kp-fx .kret.cur .rtab[data-ret="REV"]'); await pg.waitForTimeout(80);
    await pg.click('#ptabs [data-tab="mt"]'); await pg.waitForTimeout(150);
    pl.mt = [await vis('#gstrip'), await vis('#gknobs'), await vis('#km-OUT'), await vis('#tstrip')];
    pl.mtFit = await fits();
    await pg.screenshot({ path: `${shotDir}/impakt-plugin-mastr.png` });
    await pg.locator('#gtabs .tab', { hasText: 'GRN' }).click(); await pg.click('#gwave'); await pg.waitForTimeout(100);
    pl.wpickFit = await fits();
    await pg.evaluate(() => { window.__impakt.ui.wpick = false; window.__impakt.renderAll(); });
    await pg.locator('#gtabs .tab', { hasText: 'GLCH' }).click(); await pg.waitForTimeout(100);
    pl.glchFit = await fits();
    await pg.screenshot({ path: `${shotDir}/impakt-plugin-glch.png` });
    await pg.click('#ptabs [data-tab="ms"]');
    // pads: running -> select only; stopped -> audition too
    const padTap = async t => { const b = pg.locator('.pad').nth(t); await b.dispatchEvent('pointerdown', { pointerId: 3, isPrimary: true }); await b.dispatchEvent('pointerup', { pointerId: 3, isPrimary: true }); await pg.waitForTimeout(50); };
    await pg.evaluate(() => { window.__fire('impakt_pos', { steps: [1, 1, 1, 1, 1, 1, 1, 1], hits: [0, 0, 0, 0, 0, 0, 0, 0], run: 1, host: 1, bpm: 120 }); window.__ops.length = 0; });
    await padTap(2);
    pl.run = await pg.evaluate(() => ({ t: window.__impakt.ui.t, aud: window.__ops.some(o => o[0] === 'aud') }));
    await pg.evaluate(() => { window.__fire('impakt_pos', { steps: [-1, -1, -1, -1, -1, -1, -1, -1], hits: [0, 0, 0, 0, 0, 0, 0, 0], run: 0, host: 0, bpm: 120 }); window.__ops.length = 0; });
    await padTap(3);
    pl.stop = await pg.evaluate(() => ({ t: window.__impakt.ui.t, aud: window.__ops.some(o => o[0] === 'aud'), rel: window.__ops.some(o => o[0] === 'rel' && o[1] === 3) }));
    ok('plugin: 1280 x 720 panel scaled to the window, five pages (SEQ, TONE, MOD, FX, MASTR) that fit', pl.plug && pl.keys === 'Seq,Tone,Mod,FX,Mastr'
      && pl.ms.join() === 'true,true,true,true,true,false,false' && pl.tn.join() === 'true,true,true,true,false' && pl.md.join() === 'true,true,true,true,true'
      && pl.fx.join() === 'true,false,true,true,false' && pl.mt.join() === 'true,true,true,false' && ['DLY', 'CHO', 'PHS'].every(r => pl.rets[r].vis && pl.rets[r].fit.out === 0)
      && [pl.msFit, pl.tnFit, pl.mdFit, pl.fxFit, pl.mtFit, pl.wpickFit, pl.glchFit].every(f => f.out === 0 && f.s === 1.5 && f.w === 1920 && f.h === 1080), JSON.stringify(pl));
    ok('v69 plugin: MOD keeps its four modules side by side (no tabs); OPTION\'s menu and the kits panel on the panel', pl.mdTabs === 0 && pl.opt.menu && pl.opt.fit.out === 0
      && pl.opt.kits && pl.opt.kfit.out === 0, JSON.stringify({ mdTabs: pl.mdTabs, opt: pl.opt }));
    ok('v70 plugin: TONE\'s four modules side by side with the BOX (no tabs), the visualiser (its handles) over them, all on the panel', pl.tn70.box && pl.tn70.tabs === 0 && pl.tn70.h > 180
      && pl.tn70.view === 'TVIZ' && pl.tn70.n === 6 && pl.tnFit.out === 0, JSON.stringify(pl.tn70));
    ok('plugin: the machine picker holds all 47 machines on the panel, in two columns', pl.pick.n === 47 && pl.pick.out === 0 && pl.pick.reach === 47 && pl.pick.cols === 2, JSON.stringify(pl.pick));
    ok('plugin: a pad only selects while the sequence runs, auditions when stopped (and lets go when released: HOLD); track cells on the track\'s screen', pl.run.t === 2 && !pl.run.aud && pl.stop.t === 3 && pl.stop.aud && pl.stop.rel && pl.cell === 5,
      JSON.stringify({ run: pl.run, stop: pl.stop, cell: pl.cell }));
    // v89: WTF?! in the plugin: its row on the panel, in place of LOOP + SCALE, in one line; its settings sent to the engine
    {
      await pg.evaluate(() => { window.__impakt.setPTab('ms'); window.__impakt.renderAll(); window.__ops.length = 0; });
      const lb = await pg.locator('#rlive .lvl').boundingBox();
      await pg.mouse.click(lb.x + lb.width / 2, lb.y + lb.height / 2);
      const fit = async () => pg.evaluate(() => { const b = document.getElementById('wtfbar').getBoundingClientRect(), a = document.querySelector('.app').getBoundingClientRect(), r = document.querySelector('.seqrow').getBoundingClientRect();
        const kids = [...document.querySelectorAll('#wtfbar *')].filter(e => e.getClientRects().length).map(e => e.getBoundingClientRect());
        return { open: b.height > 0, h: Math.round(b.height / (a.width / 1280)), qz: 48, inPanel: b.left >= a.left && b.right <= a.right + 0.5 && b.bottom <= a.bottom + 0.5, out: kids.filter(k => k.right > b.right + 0.5 || k.left < b.left - 0.5 || k.bottom > b.bottom + 0.5).length, row: Math.round(r.height) }; });
      const f0 = await fit();
      await pg.locator('#wmodes [data-m="1"]').click(); const f1 = await fit();
      await pg.locator('#wmodes [data-m="2"]').click(); const f2 = await fit();
      await pg.locator('#wrate').dispatchEvent('dblclick');
      const sent = await pg.evaluate(() => { const G = window.__impakt.G; return [G.WTFM, G.WTFR, G.XRND].every(gi => window.__ops.some(o => o[0] === 'g' && o[1] === gi)); });
      await pg.locator('#wmodes [data-m="0"]').click(); await pg.locator('#wclose').click();
      ok('v89 plugin: WTF?! opens its row on the panel in one line (LIVE, LOOMND, LFO), its settings sent', [f0, f1, f2].every(f => f.open && f.inPanel && f.out === 0 && f.h <= 50) && sent, JSON.stringify({ f0, f1, f2, sent }));
    }
    // KEY: the trig keys as a keyboard: on T1 (on the scale, C minor) 16 notes of the scale; on T3 (off the scale) a
    // piano octave; a key plays the note; with REC while running, the note goes on the playing step (trig + NOTE p-lock)
    const keyTap = async s => { const b = pg.locator('#steps .step').nth(s); await b.dispatchEvent('pointerdown', { pointerId: 4, isPrimary: true }); await b.dispatchEvent('pointerup', { pointerId: 4, isPrimary: true }); };
    const kx = {};
    await padTap(0); await pg.click('#keybtn'); await pg.waitForTimeout(80);
    kx.fold = await pg.evaluate(() => [...document.querySelectorAll('#steps .step .sn')].map(e => e.textContent).join(' '));
    await pg.evaluate(() => { window.__ops.length = 0; }); await keyTap(7);
    kx.aud = await pg.evaluate(() => JSON.stringify(window.__ops.filter(o => o[0] === 'aud')));
    await pg.click('#octup'); await pg.evaluate(() => { window.__ops.length = 0; }); await keyTap(7);
    kx.aud2 = await pg.evaluate(() => JSON.stringify(window.__ops.filter(o => o[0] === 'aud'))); await pg.click('#octdn');
    await padTap(2);
    kx.piano = await pg.evaluate(() => [...document.querySelectorAll('#steps .step')].map(e => e.classList.contains('noff') ? '·' : e.classList.contains('nblk') ? e.textContent.trim().toLowerCase() : e.textContent.trim()).join(' '));
    await padTap(0);
    await pg.evaluate(() => { window.__fire('impakt_pos', { steps: [6, 6, 6, 6, 6, 6, 6, 6], hits: [0, 0, 0, 0, 0, 0, 0, 0], run: 1, host: 0, bpm: 120 }); window.__ops.length = 0; });
    await pg.click('#rec'); await keyTap(4); await pg.click('#rec');
    kx.rec = await pg.evaluate(() => JSON.stringify(window.__ops.filter(o => o[0] !== 'aud' && o[0] !== 'spec')));
    kx.on = await pg.evaluate(() => window.__impakt.state.tracks[0].steps[6].on === 1);
    await pg.click('#keybtn'); kx.back = await pg.evaluate(() => document.querySelectorAll('#steps .step .sn')[0].textContent === '1' && !document.getElementById('steps').classList.contains('keys'));
    ok('plugin: KEY turns the trig keys into a keyboard: the scale folded on 16 keys, a piano octave off the scale, REC writes the note',
      kx.fold === 'C D D# F G G# A# C D D# F G G# A# C D' && kx.aud === '[["aud",0,1,0]]' && kx.aud2 === '[["aud",0,1,12]]'
      && kx.piano === '· c# d# · f# g# a# · C D E F G A B C' && /\["lock",0,6,\d+,0\.3958/.test(kx.rec) && kx.on && kx.back, JSON.stringify(kx));
    // RAM: the machine's switch (F) picks what the 16 keys place and what the 8 knobs show: PLAY trigs and the playback,
    // or REC trigs and the takes; a p-lock on the REC side locks the take's settings; the switch itself is never locked
    const rm = {};
    const mLabels = () => pg.evaluate(() => [...document.querySelectorAll('#mknobs .knob .lt')].map(e => e.textContent).join(','));
    const side = async i => { const k = pg.locator('#mknobs .knob').nth(7); await k.locator('.sw b').nth(i).dispatchEvent('pointerdown', { pointerId: 7, isPrimary: true }); await k.dispatchEvent('pointerup', { pointerId: 7, isPrimary: true }); await pg.waitForTimeout(40); };
    const ops = () => pg.evaluate(() => JSON.stringify(window.__ops.filter(o => ['rec', 'trig', 'rnow', 'lock', 'prsm'].includes(o[0]) || (o[0] === 'p' && o[2] === 8))));
    await padTap(4); await pg.click('#mid'); await pg.locator('#mpick button[data-m="27"]').click(); await pg.waitForTimeout(60);
    rm.play = await mLabels();
    rm.screen = await pg.evaluate(() => document.getElementById('mname').textContent + ' · ' + document.getElementById('mkana').textContent);
    await pg.evaluate(() => { window.__ops.length = 0; });
    // OSC and SPEC: PRISM's two pages (their knobs, the PLAY trigs, the oscillators on the screen)
    await side(1);
    rm.prism = await mLabels();
    rm.prsmOp = await pg.evaluate(() => JSON.stringify(window.__ops.filter(o => o[0] === 'prsm').slice(-1)));
    await pg.evaluate(() => { window.__fire('impakt_pos', { steps: [-1, -1, -1, -1, -1, -1, -1, -1], hits: [0, 0, 0, 0, 0, 0, 0, 0], run: 0, host: 0, bpm: 120,
      prism: Array.from({ length: 64 }, (_, i) => i % 2 ? 0.3 / (1 + (i >> 1)) : 110 * (1 + (i >> 1))) }); });
    await pg.locator('#mknobs .knob').nth(0).focus(); await pg.keyboard.press('ArrowRight'); await pg.waitForTimeout(80);
    rm.prismInk = await pg.evaluate(() => { const c = document.getElementById('ovw'), d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data; let n = 0; for (let i = 3; i < d.length; i += 4) if (d[i] > 0) n++; return n; });
    rm.prismSteps = await pg.evaluate(() => !document.getElementById('steps').classList.contains('recside'));
    await pg.screenshot({ path: `${shotDir}/impakt-plugin-prism-osc.png` });
    await side(2);
    rm.spec = await mLabels();
    rm.specInk = await pg.evaluate(() => { const c = document.getElementById('ovw'), d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data; let n = 0; for (let i = 3; i < d.length; i += 4) if (d[i] > 0) n++; return n; });
    await pg.screenshot({ path: `${shotDir}/impakt-plugin-prism-spec.png` });
    await pg.evaluate(() => { window.__ops.length = 0; });
    await side(3);
    rm.rec = await mLabels();
    rm.bar = await pg.evaluate(() => document.getElementById('lockbar').classList.contains('recs') && document.querySelector('#lockbar .lt').textContent);
    await keyTap(0); await keyTap(7); await pg.waitForTimeout(420); await keyTap(7); await pg.waitForTimeout(30);   // (two taps closer than 0.38 s: p-lock)
    rm.recSt = await pg.evaluate(() => { const st = window.__impakt.state.tracks[4].steps, k = document.querySelectorAll('#steps .step');
      return { rec0: st[0].rec, on0: st[0].on, rec7: !!st[7].rec, side: document.getElementById('steps').classList.contains('recside'), k0: k[0].classList.contains('on'), k2: k[2].className }; });
    await pg.click('#rnow');
    // p-lock on the REC side: Shift + click on step 9 (a REC trig appears), SRC one step up, the switch exits the lock
    await pg.locator('#steps .step').nth(8).dispatchEvent('pointerdown', { pointerId: 8, isPrimary: true, shiftKey: true });
    await pg.locator('#mknobs .knob').nth(0).focus(); await pg.keyboard.press('ArrowRight');
    rm.lockLabels = await mLabels();
    await side(0);
    rm.after = await pg.evaluate(() => ({ lock: !!window.__impakt.ui.lock, k0: document.querySelectorAll('#steps .step')[0].className, f: window.__impakt.state.tracks[4].p[8] }));
    await keyTap(3); await pg.waitForTimeout(30);
    rm.ops = await ops();
    // the screen: the take as a waveform (peaks from the engine), the window of the PLAY trigs, a take in progress
    await pg.evaluate(() => { const ram = []; for (let t = 0; t < 8; t++) ram.push(t === 4 ? 96000 : 0, 48000, 2, t === 4 ? 2 : 0, t === 4 ? 0.5 : 0, 1);
      window.__fire('impakt_pos', { steps: [-1, -1, -1, -1, -1, -1, -1, -1], hits: [0, 0, 0, 0, 0, 0, 0, 0], run: 0, host: 0, bpm: 120, ram,
        peaks: { 4: Array.from({ length: 128 }, (_, i) => i < 64 ? 0.8 * Math.abs(Math.sin(i / 5)) : 0) } }); });
    await pg.waitForTimeout(150);
    rm.ink = await pg.evaluate(() => { const c = document.getElementById('ovw'), d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data; let n = 0; for (let i = 3; i < d.length; i += 4) if (d[i] > 0) n++; return n; });
    rm.strt = await pg.evaluate(() => document.querySelectorAll('#mknobs .knob .kv')[2].textContent + '|' + document.querySelectorAll('#mknobs .knob .kv')[3].textContent);
    await pg.screenshot({ path: `${shotDir}/impakt-plugin-ram.png` });
    ok('plugin: RAM: its switch picks the layer of the keys (PLAY / REC trigs) and the knobs; REC p-locks; TAKE now; the waveform',
      rm.play === 'TUNE,DEC,STRT,LEN,MODE,SLCE,DAC,SIDE' && rm.screen === 'RAM · サンプラー' && rm.rec === 'SRC,TAKE,LEVL,THRS,OVDB,MONI,BUF,SIDE'
      && rm.prism === 'MIX,VOIC,WAVE,DYN,OCT,GLID,QNT,SIDE' && rm.spec === 'SPED,SLIC,BLUR,TILT,CNTR,WDTH,WARP,SIDE' && rm.prsmOp === '[["prsm",4]]'
      && rm.prismInk > 1500 && rm.specInk > 1500 && rm.prismSteps
      && /^REC TRIGS · T5/.test(rm.bar) && rm.recSt.rec0 === 1 && !rm.recSt.rec7 && rm.recSt.side && rm.recSt.k0 && /other/.test(rm.recSt.k2)
      && rm.lockLabels === rm.rec && !rm.after.lock && /other/.test(rm.after.k0) && !/\bon\b/.test(rm.after.k0) && Math.abs(rm.after.f - 1 / 8) < 1e-6
      && rm.ops === JSON.stringify([['p', 4, 8, 7 / 8], ['prsm', -1], ['rec', 4, 0, 1], ['rec', 4, 7, 1], ['rec', 4, 7, 0], ['rnow', 4], ['rec', 4, 8, 1], ['lock', 4, 8, 50, 0.25], ['p', 4, 8, 1 / 8], ['trig', 4, 3, 1]])
      && rm.ink > 2000 && rm.strt === '0ms|2.0s', JSON.stringify(rm));
    // v80: OPTION · Copy track / Paste track (onto the track shown) / Move track (a tap on a pad: the two tracks swap places,
    // in every pattern; what names a track by its number follows it; the mute, "Rnd all" and the RAM take go with it)
    const cp = {};
    await pg.evaluate(() => { const I = window.__impakt, P = I.P, G = I.G, s = I.state, mid = (i, n) => (i + 0.5) / n;
      I.ui.lock = null;
      const a = s.tracks[1]; a.steps.forEach(st => { st.on = 0; st.l = {}; }); a.steps[3].on = 1; a.steps[3].l[P.TUNE] = 0.7; a.len = 12; a.mute = 1; a.rx = 1;
      const f = s.tracks[3]; f.p[P.MACH] = mid(30, 64); f.p[P.A] = mid(3, 10); f.steps[5].l[P.A] = mid(7, 10);     // T4: an FX taking T2 (a step: T6)
      s.tracks[6].p[P.E1SR] = mid(2, 11);                                                                     // T7's follower 1: T2
      s.g[G.QSRC] = mid(2, 9);                                                                                // the quantiser's track: T2
      const pt = { chg: 0, tracks: s.tracks.map(t => ({ p: t.p.slice(), len: 16, spd: t.spd, q: 1, pre: null, steps: t.steps.map(() => ({ on: 0, l: {} })) })) };
      pt.tracks[1].len = 7; pt.tracks[3].p[P.MACH] = mid(30, 64); pt.tracks[3].p[P.A] = mid(7, 10);           // pattern A06: T2 7 steps, T4 an FX taking T6
      s.pats[5] = pt; I.selectTrack(1); I.renderAll(); window.__ops.length = 0; });
    await pg.click('#projbtn'); cp.menu = await pg.evaluate(() => [...document.querySelectorAll('#projrow .btn')].map(b => b.textContent + (b.disabled ? '(off)' : '')).join(','));
    cp.menuFit = await fits();
    await pg.click('#tmove'); await pg.waitForTimeout(60);
    cp.bar = await pg.evaluate(() => ({ mv: document.getElementById('lockbar').classList.contains('mv'), t: document.querySelector('#lockbar .lt').textContent,
      pads: document.getElementById('pads').classList.contains('mv'), src: [...document.querySelectorAll('.pad')].map(b => b.classList.contains('mvsrc') ? 1 : 0).join('') }));
    await pg.screenshot({ path: `${shotDir}/impakt-plugin-move.png` });
    await padTap(5);
    cp.mv = await pg.evaluate(() => { const I = window.__impakt, P = I.P, G = I.G, s = I.state, ix = (v, n) => Math.min(n - 1, Math.floor(v * n)), pt = s.pats[5];
      const ops = window.__ops;
      return { t: I.ui.t, len: s.tracks[5].len + '/' + s.tracks[1].len, on: s.tracks[5].steps[3].on, lk: s.tracks[5].steps[3].l[P.TUNE], mute: s.tracks[5].mute + '' + s.tracks[1].mute,
        rx: s.tracks[5].rx + '' + s.tracks[1].rx, fx: ix(s.tracks[3].p[P.A], 10), fxL: ix(s.tracks[3].steps[5].l[P.A], 10), ef: ix(s.tracks[6].p[P.E1SR], 11),
        q: ix(s.g[G.QSRC], 9), pLen: pt.tracks[5].len + '/' + pt.tracks[1].len, pFx: ix(pt.tracks[3].p[P.A], 10), bar: document.getElementById('lockbar').classList.contains('mv'),
        first: JSON.stringify(ops.filter(o => o[0] !== 'spec')[0]), swp: JSON.stringify(ops.filter(o => o[0] === 'rswp')), epat: (e => e.includes(5) && e[e.length - 1] === s.pat && !e.slice(0, -1).includes(s.pat))(ops.filter(o => o[0] === 'epat').map(o => o[1])),
        xmOk: Math.round(s.g[G.XMSK] * 255) === s.tracks.reduce((m, t, i) => m | (t.rx ? 0 : 1 << i), 0) && !(Math.round(s.g[G.XMSK] * 255) & 32) && !!(Math.round(s.g[G.XMSK] * 255) & 2),
        pall: ops.some(o => o[0] === 'pall' || o[0] === 'pcur'), mutes: ops.filter(o => o[0] === 'mute').map(o => o[2]).join(''),
        lenOps: ops.filter(o => o[0] === 'len' && o[1] === 5).map(o => o[2]).join(','), g: JSON.stringify(ops.filter(o => o[0] === 'g').map(o => [o[1], +o[2].toFixed(4)])) }; });
    // Copy T6 (the one moved there), Paste onto T1; a RAM track pasted: its take too
    await pg.evaluate(() => { window.__ops.length = 0; });
    await pg.click('#projbtn'); await pg.click('#tcopy');
    cp.clip = await pg.evaluate(() => window.__impakt.ui.tclip && window.__impakt.ui.tclip.name);
    await padTap(0); await pg.click('#projbtn');
    cp.paste = await pg.evaluate(() => { const b = document.getElementById('tpaste'); return { off: b.disabled, title: b.title }; });
    await pg.click('#tpaste'); await pg.click('#tpaste'); await pg.waitForTimeout(40);       // (armed: a second tap does it)
    cp.pasted = await pg.evaluate(() => { const I = window.__impakt, P = I.P, s = I.state, ops = window.__ops;
      return { len: s.tracks[0].len, on: s.tracks[0].steps[3].on, lk: s.tracks[0].steps[3].l[P.TUNE], same: s.tracks[0].steps !== s.tracks[5].steps, mute: s.tracks[0].mute,
        clr: ops.filter(o => o[0] === 'clrl' && o[1] === 0).length, trig: JSON.stringify(ops.filter(o => o[0] === 'trig' && o[1] === 0 && o[3] === 1)), rcpy: ops.some(o => o[0] === 'rcpy') }; });
    await pg.evaluate(() => { const I = window.__impakt, P = I.P, s = I.state; s.tracks[2].p[P.MACH] = (27 + 0.5) / 64; I.selectTrack(2); I.copyTrack(); I.selectTrack(4); window.__ops.length = 0; I.pasteTrack(); });
    cp.ram = await pg.evaluate(() => JSON.stringify(window.__ops.filter(o => o[0] === 'rcpy')));
    // Move, then Cancel (Escape): nothing moves
    await pg.click('#projbtn'); await pg.click('#tmove'); await pg.keyboard.press('Escape'); await pg.waitForTimeout(40);
    cp.esc = await pg.evaluate(() => window.__impakt.ui.moveFrom == null && !document.getElementById('lockbar').classList.contains('mv'));
    ok('v80 plugin: OPTION · Copy / Paste / Move track: the bar while moving, the two tracks swap in every pattern (refs, mute, Rnd all, take, the quantiser follow), a paste copies a track (and a RAM take)',
      cp.menu === 'Copy track,Paste track(off),Move track,Clear track,Clear all,Kits,Init,Demo,Updates' && cp.menuFit.out === 0
      && cp.bar.mv && /^MOVE T2/.test(cp.bar.t) && cp.bar.pads && cp.bar.src === '01000000'
      && cp.mv.t === 5 && cp.mv.len === '12/16' && cp.mv.on === 1 && cp.mv.lk === 0.7 && cp.mv.mute === '10' && cp.mv.rx === '10' && cp.mv.xmOk
      && cp.mv.fx === 7 && cp.mv.fxL === 3 && cp.mv.ef === 6 && cp.mv.q === 6 && cp.mv.pLen === '7/16' && cp.mv.pFx === 3 && !cp.mv.bar
      && cp.mv.first === '["reset"]' && cp.mv.swp === '[["rswp",1,5]]' && !cp.mv.pall && cp.mv.epat && cp.mv.mutes.length === 8 && cp.mv.mutes[5] === '1'
      && /^12,(\d+,)*7(,|$)/.test(cp.mv.lenOps)
      && /^T6 · [A-H]\d\d$/.test(cp.clip) && !cp.paste.off && /T6/.test(cp.paste.title)
      && cp.pasted.len === 12 && cp.pasted.on === 1 && cp.pasted.lk === 0.7 && cp.pasted.same && cp.pasted.mute === 0 && cp.pasted.clr === 64 && /\["trig",0,3,1\]/.test(cp.pasted.trig) && !cp.pasted.rcpy
      && cp.ram === '[["rcpy",2,4]]' && cp.esc, JSON.stringify(cp));
    ok('plugin: no page errors', perr.length === 0, perr.join(' | '));
    await pg.close();
  }
  // forced fallback: block WebAssembly to exercise the plain-JS engine
  const page = await browser.newPage({ viewport: { width: 390, height: 844 } });
  const errors = [];
  page.on('pageerror', e => errors.push(String(e)));
  await page.addInitScript(() => { WebAssembly.compile = () => Promise.reject(new Error('blocked by CSP (test)')); });
  await page.goto('http://localhost:8765/index.html');
  await page.click('#play');
  // the plain-JS engine builds its tables in the worklet (about 2.5 s on a desktop, 10 s on a slow shared machine): wait
  // for it (up to 30 s)
  for (let i = 0; i < 120; i++) { await page.waitForTimeout(250); if (await page.evaluate(() => window.__impakt.ui.pos.run === 1 && window.__impakt.ui.pos.hits.some(h => h > 0))) break; }
  const pos = await page.evaluate(() => window.__impakt.ui.pos);
  ok('JS fallback engine runs when WebAssembly is blocked', pos.run === 1, JSON.stringify(pos.hits) + ' ' + errors.join('|'));
  await browser.close();
  server.close();
  console.log(results.join('\n'));
})().catch(e => { console.log((global.__results || []).join('\n')); console.error(e); server.close(); process.exit(1); });
