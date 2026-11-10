// Measures every factory preset through the real engine (WebAssembly) and writes the track VOL that
// brings it to a common loudness (BS.1770 K-weighting, loudest 100 ms window), with an offset per
// family so a fresh kit is already balanced. Also flags presets that are silent, clip or never end.
//   node tools/preset_levels.js            -> web/src/preset_levels.js + report
//   node tools/preset_levels.js ALLOY [--write]  -> one machine's report (--write: its levels into the file)
const fs = require('fs'), path = require('path');
const ROOT = path.resolve(__dirname, '..');
const PR = require(path.join(ROOT, 'web/src/presets.js'));
const { loadBank } = require('./bank_node.js');                     // (CHIMERA plays the bank)
const TONE = require(path.join(ROOT, 'web/src/preset_tone.js'));   // each preset's HP / LP (tools/preset_voicing.js --tone)
const app = fs.readFileSync(path.join(ROOT, 'web/src/app.js'), 'utf8');
// machine defaults (d: [TUNE, DEC, A..F]) from app.js, in machine order
const ORGN = ['RAMEAU','BRANCHE','BOIS','BAMBOU','FEUILLES','GRAINES','CHIPS','BISCOTTE','CÉLERI','COQUILLE','NOIX','FEU','ALLUMETTE',
  'GRAVIER','CAILLOUX','SABLE','NEIGE','GLAÇON','VERRE','GOUTTE','BULLES','PLUIE','PSCHITT','BOUCHON','PAPIER','SACHET','P.BULLE','ZIP',
  'VELCRO','CLAP','DOIGTS','LANGUE','PHALANGE','PINGPONG','PIÈCES','GRILLON'];
const block = app.slice(app.indexOf('const MACH = ['), app.indexOf('];', app.indexOf('const MACH = [')));
const defs = block.split('{ id: ').slice(1).map(chunk => {
  const d = chunk.match(/\bd: \[([^\]]+)\]/)[1];
  return new Function('mid', 'org', 'NORG', 'ULOOP_SRC', 'return [' + d + '];')(
    (i, n) => (i + 0.5) / n, nm => ORGN.indexOf(nm), 36, { length: 40 });
});
// ALLOY's side pages (sd), the same way
const sds = block.split('{ id: ').slice(1).map(chunk => {
  const m = chunk.match(/\bsd: \[([^\]]+)\]/);
  return m ? new Function('mid', 'return [' + m[1] + '];')((i, n) => (i + 0.5) / n) : null;
});
if (defs.length !== PR.MACH_IDS.length) throw new Error('machine defaults: ' + defs.length);
const wasm = new WebAssembly.Module(fs.readFileSync(path.join(ROOT, 'web/impakt-simd.wasm')));
const SR = 48000;
// BS.1770 K-weighting at 48 kHz
const K1 = { b: [1.53512485958697, -2.69169618940638, 1.19839281085285], a: [-1.69065929318241, 0.73248077421585] };
const K2 = { b: [1.0, -2.0, 1.0], a: [-1.99004745483398, 0.99007225036621] };
function kweight(x) {
  const y = new Float64Array(x.length);
  for (const f of [K1, K2]) {
    let x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    const src = f === K1 ? x : y;
    for (let i = 0; i < x.length; i++) {
      const v = src[i], o = f.b[0] * v + f.b[1] * x1 + f.b[2] * x2 - f.a[0] * y1 - f.a[1] * y2;
      x2 = x1; x1 = v; y2 = y1; y1 = o; y[i] = o;
    }
  }
  return y;
}
// family offsets (dB) for a kit that is balanced out of the box
const OFFSET = { 'BD·VA': 2.5, 'BD·FM': 2.5, 'SD·VA': -0.5, 'SD·FM': -0.5, 'HH·MT': -4, 'CP·NZ': -1.5, 'FM·X': -2, 'NOISE': -4, 'IMPLS': -3,
  'SY·RAW': -1, 'BYTE': -5, 'µLOOP': -2, 'BUG': -3, 'CHAOS': -5, 'VOX': -2, 'CZ·PD': -2, 'FRACT': -2, 'ITERA': 0.5, 'TRIO': -2,
  'SINE': 1, 'DATA': -3, 'FM·Z': 1, 'HARD': 1.5, 'HEX·BD': 2.5, 'HEX·SD': -0.5, 'HEX·TM': -1,
  'MACRO': -1, 'RAM': 0, 'HALO': -4, 'BREAK': 0, 'STRUM': -3, 'CHOIR': -4, 'ALLOY': -2, 'CHIMERA': 0, 'CUBE': -1, 'WEST': -2, 'HOWL': -3, 'MANTRA': -4 };
// CHIMERA: by the category of its sample (KICK SNARE CLAP HAT PERC 808 CYMB VOX FX LOOP), as the machines of each family
const CHIM_OFFSET = [2.5, -0.5, -1.5, -4, -3, 1.5, -4, -2, -2, -2];
const TARGET = -21;     // LUFS-like, loudest 100 ms, single hit at full velocity
// BREAK plays the user's loops: a loop made here (kick, snare, hats on every 16th, 120 BPM, 2 bars), levelled as the
// page levels a loop it loads (-16 dBFS RMS), in slot 1
function testLoop() {
  const R = 32000, n = Math.round(2 * 240 / 120 * R), x = new Float64Array(n), pat = 'K..H.S..K.K.S..H';
  let seed = 1234; const rnd = () => { seed = (seed * 1103515245 + 12345) >>> 0; return seed / 2147483648 - 1; };
  for (let k = 0; k < 32; k++) {
    const c = pat[k % 16], t0 = Math.round(k * n / 32 + (k % 2 ? 0.006 * R : 0));
    let ph = 0, last = 0;
    for (let i = 0; i < Math.min(n - t0, 0.4 * R); i++) {
      const t = i / R, nz = rnd(), hp = nz - last; last = nz;
      let y = (c === 'H' ? 0.22 : 0.12) * hp * Math.exp(-t / 0.03);
      if (c === 'K') { ph += 2 * Math.PI * (48 + 130 * Math.exp(-t / 0.03)) / R; y += 0.9 * Math.sin(ph) * Math.exp(-t / 0.22); }
      if (c === 'S') y += 0.45 * nz * Math.exp(-t / 0.11) + 0.35 * Math.sin(2 * Math.PI * 190 * t) * Math.exp(-t / 0.07);
      x[t0 + i] += y * Math.min(1, i / 8);
    }
  }
  let s2 = 0, pk = 0; for (const v of x) { s2 += v * v; pk = Math.max(pk, Math.abs(v)); }
  const g = Math.min(Math.pow(10, -16 / 20) / Math.sqrt(s2 / n), Math.pow(10, -0.5 / 20) / pk);
  return Int16Array.from(x, v => Math.max(-32767, Math.min(32767, Math.round(v * g * 32767))));
}
const LOOP = testLoop();
const ONLY = process.argv[2] || null;    // optional: one machine id, report only (no file written; --write: its levels into the file)
const WRITE1 = ONLY && process.argv.includes('--write');
const out = {}, report = [];
// the FX machines play another track's sound: their presets stay at unity (VOL 0.8), as the page leaves them
const FXM = ['VERB', 'ECHO', 'BEND', 'TAPE', 'CODEC', 'FUZZ', 'STENCIL'];
PR.MACH_IDS.forEach((id, m) => {
  out[id] = [];
  if (ONLY && id !== ONLY) return;
  if (FXM.includes(id)) return;
  (PR.LIST[id] || []).forEach(([tag, name, params], pi) => {   // (RAM has none)
    const x = new WebAssembly.Instance(wasm, {}).exports;
    x.sk_init(SR);
    for (let t = 1; t < 8; t++) x.sk_set_param(t, 17, 0);
    x.sk_set_param(0, 0, (m + 0.5) / 64);
    const p = PR.resolve(params, defs[m], 0.8, (TONE[id] || [])[pi], sds[m]);
    for (const k in p) x.sk_set_param(0, +k, p[k]);
    x.sk_set_param(0, 26, 1);                        // full velocity for the measure
    x.sk_set_global(48, 0);                          // DAC off, mix helpers off
    if (id === 'CHIMERA') loadBank(x);
    if (id === 'BREAK') {
      new Int16Array(x.memory.buffer, x.sk_break_mem(0), LOOP.length).set(LOOP);
      x.sk_break_set(0, LOOP.length, 2);
      for (let i = 0; i < 400; i++) x.sk_process(128);   // (the slot is read over a few blocks)
      x.sk_set_global(0, 120);
    }
    x.sk_audition(0, 1, 0);
    const N = SR * 4, L = new Float32Array(N), R = new Float32Array(N);
    const ol = new Float32Array(x.memory.buffer, x.sk_out_l(), 4096), or = new Float32Array(x.memory.buffer, x.sk_out_r(), 4096);
    for (let i = 0; i < N; i += 128) { x.sk_process(128); L.set(ol.subarray(0, 128), i); R.set(or.subarray(0, 128), i); }
    let peak = 0, bad = false, last = 0;
    for (let i = 0; i < N; i++) { const a = Math.max(Math.abs(L[i]), Math.abs(R[i])); if (!isFinite(a)) bad = true; peak = Math.max(peak, a); if (a > 1e-3) last = i; }
    const kl = kweight(L), kr = kweight(R), W = SR / 10, H = SR / 40;
    let mx = -200;
    for (let s = 0; s + W <= N; s += H) { let e = 0; for (let i = s; i < s + W; i++) e += kl[i] * kl[i] + kr[i] * kr[i]; mx = Math.max(mx, -0.691 + 10 * Math.log10(e / W + 1e-20)); }
    const want = TARGET + (id === 'CHIMERA' ? CHIM_OFFSET[Math.min(9, Math.floor(p[3] * 10))] : OFFSET[id]);
    let vol = 0.8 * Math.pow(10, (want - mx) / 20);
    const flags = [];
    if (mx < -50 || bad) flags.push(bad ? 'NAN' : 'SILENT');
    if (vol > 1) { flags.push('QUIET ' + (20 * Math.log10(vol)).toFixed(1) + ' dB'); vol = 1; }
    if (vol < 0.06) { flags.push('LOUD'); vol = 0.06; }
    // no preset clips on its own: a spiky sound gives up some loudness rather than its peaks
    let pkOut = peak * vol / 0.8;
    if (pkOut > 0.95) { flags.push('PEAK ' + pkOut.toFixed(2) + ' -> 0.95'); vol *= 0.95 / pkOut; pkOut = 0.95; }
    if (last > N - SR * 0.2) flags.push('LONG TAIL');
    out[id].push(+vol.toFixed(3));
    report.push(`${id.padEnd(7)} ${(tag + ' · ' + name).padEnd(30)} loud ${mx.toFixed(1).padStart(6)}  vol ${vol.toFixed(2)}  peak ${pkOut.toFixed(2)}  tail ${(last / SR).toFixed(2)}s ${flags.join(' ')}`);
  });
});
// (one machine with --write: its levels into the file, the others as they were: each is measured on its own)
const all = WRITE1 ? Object.assign(require(path.join(ROOT, 'web/src/preset_levels.js')), { [ONLY]: out[ONLY] }) : out;
if (!ONLY || WRITE1) fs.writeFileSync(path.join(ROOT, 'web/src/preset_levels.js'),
  '// generated by tools/preset_levels.js: track VOL per factory preset (common loudness, per-family offsets)\n' +
  'const IMPAKT_PRESET_VOL = ' + JSON.stringify(all) + ';\nif (typeof module !== \'undefined\') module.exports = IMPAKT_PRESET_VOL;\n');
console.log(report.join('\n'));
