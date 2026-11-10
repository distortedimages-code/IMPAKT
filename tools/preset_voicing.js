// Voicing check of the factory presets through the real engine (WebAssembly): where each sound puts its energy.
// One hit at full velocity, 2 s, Welch power spectrum (Hann 4096, hop 1024) summed over the hit, so the loud part
// weighs most. Per preset: energy per band (% of the total), the frequencies under which 1 / 5 / 50 / 95 / 99 % of
// the energy lies, DC, crest factor (peak / RMS of the loudest 50 ms, in dB) and the length of the hit.
//   node tools/preset_voicing.js [machine] [--flat]  -> JSON on stdout (one line per preset), a summary on stderr
//   --flat: the presets without their TONE settings (HP / LP / EQ flat), to see the raw machine
//   --tone: measures the raw machines (flat) and chooses each preset's HP / LP (rules below), written to
//           web/src/preset_tone.js (then run tools/preset_levels.js: the levels are measured through them)
//   --cached: with --tone, the rules again on the last measure (build/voicing_cache.json) without rendering
const fs = require('fs'), path = require('path');
const ROOT = path.resolve(__dirname, '..');
const PR = require(path.join(ROOT, 'web/src/presets.js'));
let PTONE = {}; try { PTONE = require(path.join(ROOT, 'web/src/preset_tone.js')); } catch (e) {}
const app = fs.readFileSync(path.join(ROOT, 'web/src/app.js'), 'utf8');
const ORGN = ['RAMEAU','BRANCHE','BOIS','BAMBOU','FEUILLES','GRAINES','CHIPS','BISCOTTE','CÉLERI','COQUILLE','NOIX','FEU','ALLUMETTE',
  'GRAVIER','CAILLOUX','SABLE','NEIGE','GLAÇON','VERRE','GOUTTE','BULLES','PLUIE','PSCHITT','BOUCHON','PAPIER','SACHET','P.BULLE','ZIP',
  'VELCRO','CLAP','DOIGTS','LANGUE','PHALANGE','PINGPONG','PIÈCES','GRILLON'];
const block = app.slice(app.indexOf('const MACH = ['), app.indexOf('];', app.indexOf('const MACH = [')));
const defs = block.split('{ id: ').slice(1).map(chunk => {
  const d = chunk.match(/\bd: \[([^\]]+)\]/)[1];
  return new Function('mid', 'org', 'NORG', 'ULOOP_SRC', 'return [' + d + '];')((i, n) => (i + 0.5) / n, nm => ORGN.indexOf(nm), 36, { length: 40 });
});
// ALLOY's side pages (sd), the same way
const sds = block.split('{ id: ').slice(1).map(chunk => {
  const m = chunk.match(/\bsd: \[([^\]]+)\]/);
  return m ? new Function('mid', 'return [' + m[1] + '];')((i, n) => (i + 0.5) / n) : null;
});
const wasm = new WebAssembly.Module(fs.readFileSync(path.join(ROOT, 'web/impakt-simd.wasm')));
const SR = 48000, N = SR * 2, FN = 4096, HOP = 1024;
const args = process.argv.slice(2), TONE_MODE = args.includes('--tone'), CACHED = args.includes('--cached'), FLAT = TONE_MODE || args.includes('--flat'), ONLY = args.find(a => !a.startsWith('--')) || null;
const TONE = [71, 72, 73, 74, 75, 76, 77, 78], TONE_FLAT = [0, 1, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5];

// radix-2 FFT, in place
function fft(re, im) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; i++) { let b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) { [re[i], re[j]] = [re[j], re[i]]; [im[i], im[j]] = [im[j], im[i]]; } }
  for (let len = 2; len <= n; len <<= 1) {
    const a = -2 * Math.PI / len, wr = Math.cos(a), wi = Math.sin(a);
    for (let i = 0; i < n; i += len) {
      let cr = 1, ci = 0;
      for (let k = 0; k < len / 2; k++) {
        const ur = re[i + k], ui = im[i + k], vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci, vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
        re[i + k] = ur + vr; im[i + k] = ui + vi; re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
        const t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
      }
    }
  }
}
// A-weighting (IEC 61672), as a power ratio
function aWeight(f) {
  const f2 = f * f, ra = 12194 ** 2 * f2 * f2 / ((f2 + 20.6 ** 2) * Math.sqrt((f2 + 107.7 ** 2) * (f2 + 737.9 ** 2)) * (f2 + 12194 ** 2));
  return ra * ra * 1.2589;
}
const win = Float64Array.from({ length: FN }, (_, i) => 0.5 - 0.5 * Math.cos(2 * Math.PI * i / FN));
const EDGES = [30, 60, 120, 250, 500, 1000, 2000, 4000, 8000, 16000];
const BAND_NAMES = ['<30', '30-60', '60-120', '120-250', '250-500', '500-1k', '1-2k', '2-4k', '4-8k', '8-16k', '>16k'];

function measure(m, params, tone) {
  const x = new WebAssembly.Instance(wasm, {}).exports;
  x.sk_init(SR);
  for (let t = 1; t < 8; t++) x.sk_set_param(t, 17, 0);
  x.sk_set_param(0, 0, (m + 0.5) / 64);
  const p = PR.resolve(params, defs[m], 0.8, tone, sds[m]);
  if (FLAT) TONE.forEach((k, i) => { p[k] = TONE_FLAT[i]; });
  for (const k in p) x.sk_set_param(0, +k, p[k]);
  x.sk_set_param(0, 26, 1);
  x.sk_set_global(48, 0);
  x.sk_audition(0, 1, 0);
  const M = new Float32Array(N);
  const ol = new Float32Array(x.memory.buffer, x.sk_out_l(), 4096), or = new Float32Array(x.memory.buffer, x.sk_out_r(), 4096);
  for (let i = 0; i < N; i += 128) { x.sk_process(128); for (let k = 0; k < 128; k++) M[i + k] = 0.5 * (ol[k] + or[k]); }
  let dc = 0, peak = 0, last = 0;
  for (let i = 0; i < N; i++) { dc += M[i]; const a = Math.abs(M[i]); peak = Math.max(peak, a); if (a > 1e-3) last = i; }
  dc /= Math.max(1, last);
  // the loudest 50 ms: crest factor
  const W = SR / 20; let e = 0, emax = 0;
  for (let i = 0; i < N; i++) { e += M[i] * M[i]; if (i >= W) e -= M[i - W] * M[i - W]; emax = Math.max(emax, e); }
  const crest = 20 * Math.log10(peak / Math.sqrt(emax / W + 1e-30) + 1e-30);
  // the power spectrum over the hit
  const P = new Float64Array(FN / 2), re = new Float64Array(FN), im = new Float64Array(FN);
  for (let s = 0; s + FN <= Math.min(N, last + FN); s += HOP) {
    for (let i = 0; i < FN; i++) { re[i] = M[s + i] * win[i]; im[i] = 0; }
    fft(re, im);
    for (let k = 0; k < FN / 2; k++) P[k] += re[k] * re[k] + im[k] * im[k];
  }
  // the lows in detail: one long transform of the whole hit (0.37 Hz bins): below 10, 10-20, 20-30, 30-40 Hz
  const LN = 1 << 17, lr = new Float64Array(LN), li = new Float64Array(LN);
  for (let i = 0; i < N; i++) lr[i] = M[i];
  fft(lr, li);
  let ltot = 0; const low = [0, 0, 0, 0];
  for (let k = 0; k < LN / 2; k++) { const e = lr[k] * lr[k] + li[k] * li[k], f = k * SR / LN; ltot += e; if (f < 40) low[Math.floor(f / 10)] += e; }
  const lows = low.map(v => +(100 * v / ltot).toFixed(1));
  // and in twelfths of an octave from 5 Hz (energy, and A-weighted energy) for the TONE choice
  const spec = [];
  for (let k = 1; k < LN / 2; k++) {
    const f = k * SR / LN, e = lr[k] * lr[k] + li[k] * li[k], b = Math.floor(12 * Math.log2(f / 5));
    if (b < 0) continue;
    if (!spec[b]) spec[b] = { f: 5 * Math.pow(2, (b + 0.5) / 12), e: 0, a: 0 };
    spec[b].e += e; spec[b].a += e * aWeight(f);
  }
  let tot = 0; for (let k = 1; k < FN / 2; k++) tot += P[k];
  const band = new Array(EDGES.length + 1).fill(0), df = SR / FN;
  const pct = [0.01, 0.05, 0.5, 0.95, 0.99], fq = []; let cum = 0, pi = 0;
  for (let k = 1; k < FN / 2; k++) {
    const f = k * df; let b = 0; while (b < EDGES.length && f >= EDGES[b]) b++;
    band[b] += P[k] / tot;
    cum += P[k] / tot;
    while (pi < pct.length && cum >= pct[pi]) { fq.push(Math.round(f)); pi++; }
  }
  return { peak: +peak.toFixed(3), crest: +crest.toFixed(1), dc: +dc.toFixed(4), len: +(last / SR).toFixed(2), band: band.map(v => +(100 * v).toFixed(1)), f: fq, lows,
    tone: TONE.map(k => +(+p[k]).toFixed(3)), spec: spec.filter(Boolean) };
}

// ---- TONE per preset. What each role may lose to its HP (share of the energy) and how high the HP may go; the
// LP only takes what is barely there (A-weighted share and energy share), never on the hats
const ROLE = {
  K: { ids: ['BD·VA', 'BD·FM', 'HARD', 'HEX·BD', 'SINE', 'FM·Z'], hpMax: 30, hpLoss: 0.01, lpMin: 10000 },
  B: { ids: ['SY·RAW', 'TRIO', 'CZ·PD', 'FM·X', 'ITERA', 'MACRO', 'VOX', 'ALLOY', 'CUBE', 'WEST'], hpMax: 120, hpLoss: 0.01, lpMin: 8000, pitched: true },
  S: { ids: ['SD·VA', 'SD·FM', 'HEX·SD'], hpMax: 120, hpLoss: 0.02, lpMin: 12000 },
  T: { ids: ['HEX·TM'], hpMax: 60, hpLoss: 0.02, lpMin: 10000 },
  C: { ids: ['CP·NZ'], hpMax: 300, hpLoss: 0.03, lpMin: 12000 },
  H: { ids: ['HH·MT'], hpMax: 650, hpLoss: 0.03, lpMin: null },
  P: { ids: ['NOISE', 'IMPLS', 'BYTE', 'µLOOP', 'BUG', 'CHAOS', 'FRACT', 'DATA'], hpMax: 400, hpLoss: 0.02, lpMin: 10000, pitched: true },
};
const roleOf = id => Object.values(ROLE).find(r => r.ids.includes(id));
const HP_L = [20, 25, 30, 35, 40, 50, 60, 70, 80, 100, 120, 150, 180, 220, 270, 330, 400, 500, 650];
const LP_L = [6500, 8000, 10000, 12000, 14000, 16000];
function lossOf(spec, fc, hp) {
  let te = 0, ta = 0, le = 0, la = 0;
  for (const b of spec) { const x4 = Math.pow(b.f / fc, 4), g = hp ? x4 / (1 + x4) : 1 / (1 + x4); te += b.e; ta += b.a; le += b.e * (1 - g); la += b.a * (1 - g); }
  return [le / te, la / ta];
}
function chooseTone(r) {
  const ro = roleOf(r.id);
  // HP: the highest corner the role allows that takes no more than its share (and never 1 % of what is heard);
  // a pitched sound keeps an octave below its lowest strong partial free, for notes played lower
  const cap = Math.min(ro.hpMax, ro.pitched ? r.f[0] / 2 : Infinity);
  let hp = 0;
  for (const fc of HP_L) { if (fc > cap) break; const [le, la] = lossOf(r.spec, fc, true); if (le <= ro.hpLoss && la <= 0.01) hp = fc; }
  // LP: the lowest corner above the role's floor that leaves the sound as heard (0.5 % A-weighted, 0.3 % energy)
  let lp = 0;
  if (ro.lpMin) for (const fc of LP_L) { if (fc < ro.lpMin) continue; const [le, la] = lossOf(r.spec, fc, false); if (le <= 0.003 && la <= 0.005) { lp = fc; break; } }
  return [hp, lp];
}

const rows = [], CACHE = path.join(ROOT, 'build/voicing_cache.json');
if (CACHED) for (const r of JSON.parse(fs.readFileSync(CACHE, 'utf8'))) { r.choice = chooseTone(r); rows.push(r); }
else PR.MACH_IDS.forEach((id, m) => {
  if (ONLY && id !== ONLY) return;
  if (['VERB', 'ECHO', 'BEND', 'TAPE', 'CODEC', 'FUZZ', 'STENCIL'].includes(id)) return;   // (the FX machines: no sound of their own, TONE flat)
  if (TONE_MODE && !roleOf(id)) return;                                 // (HALO, BREAK, STRUM, CHOIR: no role, TONE flat)
  (PR.LIST[id] || []).forEach(([tag, name, params], pi) => {
    const r = { id, tag, name, ...measure(m, params, (PTONE[id] || [])[pi]) };
    if (TONE_MODE) r.choice = chooseTone(r);
    rows.push(r); const { spec, ...shown } = r; process.stdout.write(JSON.stringify(shown) + '\n');
  });
});
if (TONE_MODE && !ONLY && !CACHED) { fs.mkdirSync(path.dirname(CACHE), { recursive: true }); fs.writeFileSync(CACHE, JSON.stringify(rows)); }
if (TONE_MODE && !ONLY) {
  const out = {};
  for (const id of PR.MACH_IDS) if (roleOf(id) || id === 'RAM') out[id] = rows.filter(r => r.id === id).map(r => r.choice);
  fs.writeFileSync(path.join(ROOT, 'web/src/preset_tone.js'),
    '// generated by tools/preset_voicing.js --tone: [HP, LP] in Hz per factory preset (0: off), from where each sound\n' +
    '// puts its energy (the preset\'s own hp / lp, when it has them, win)\n' +
    'const IMPAKT_PRESET_TONE = ' + JSON.stringify(out) + ';\n' +
    "if (typeof module !== 'undefined') module.exports = IMPAKT_PRESET_TONE;\n");
}
// summary per machine: median of each band and of the percentiles
const med = a => { const s = [...a].sort((x, y) => x - y); return s[Math.floor(s.length / 2)]; };
process.stderr.write('machine  n   ' + BAND_NAMES.map(b => b.padStart(7)).join('') + '    f1    f5   f50   f95   f99  crest\n');
for (const id of PR.MACH_IDS) {
  const rs = rows.filter(r => r.id === id); if (!rs.length) continue;
  process.stderr.write(id.padEnd(8) + String(rs.length).padStart(3) + '  ' + BAND_NAMES.map((_, b) => med(rs.map(r => r.band[b])).toFixed(1).padStart(7)).join('')
    + [0, 1, 2, 3, 4].map(i => String(med(rs.map(r => r.f[i]))).padStart(6)).join('') + med(rs.map(r => r.crest)).toFixed(1).padStart(7) + '\n');
}
