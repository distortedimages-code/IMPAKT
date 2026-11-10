// IMPAKT CPU benchmark and null test, on the WebAssembly engine the page runs (node, V8 like Chrome).
//
//   node tools/bench.js                       all scenarios, CPU in % of real time + worst 128-frame block
//   node tools/bench.js --save <dir>          also writes each scenario's output (reference for null tests)
//   node tools/bench.js --check <dir>         compares each scenario's output with a saved reference
//   node tools/bench.js --wasm web/impakt.wasm --only demo,heavy --runs 5
//
// Scenarios: idle (transport running, every track muted: master, reverb, helpers), demo (the factory demo),
// kit (the demo with the HEX machines and ALLOY on six tracks, the heaviest settings), heavy (demo + 8 tracks of ratchets, the 8 longest IRs, SPECTRAL
// reverb, unmasking, CALM, master compressor and drive), prism (8 RAM tracks resynthesising takes of noise with 16
// sines, both voices of every track sounding), prismw (the same read from the wave tables, WAVE + DYN + QNT + SLIC +
// WARP: the heaviest PRISM can get), glitch (the demo through GLITCH: LIVE, repeats, bends, breaks, DSTR, the codec
// in PKT·R with RESO), tape (the same with LOSS on its WORN tape), halo (the demo with HALO on track 8, its heaviest: an
// 11th, a progression a step, glides, SPRD, BLOOM, AIR, DRFT and WIDE all up), break (the demo with BREAK on track 8,
// its heaviest: three slots filled, a trig on every step, SWAP RAND STUT REV PJMP SKIP, VARI, DUST), fx (the five FX machines in a
// chain on T4..T8, each at its heaviest), strum and choir (the demo with STRUM or CHOIR on track 8 at its heaviest: v59), chimera
// (the demo with CHIMERA on T6..T8: families, grafts with the split, a reversed loop: v66), sends (the demo with every
// track sent to the CHORUS, the PHASER in SERIES with square-ish sweeps and feedback, and the 1982 delay with its VCO:
// v68), fuzz (the demo through FUZZ on T8, SRC SUM, driven hard, SUST, GATE, every band through the circuit: v68), and
// one per machine (track 1 alone,
// every step with 1/32 ratchets, long decay) reported above the idle cost.
const fs = require('fs'), path = require('path');
const ROOT = path.resolve(__dirname, '..');
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const WASM = arg('--wasm', path.join(ROOT, 'web/impakt-simd.wasm'));
const SAVE = arg('--save', null), CHECK = arg('--check', null);
const ONLY = arg('--only', null), RUNS = +arg('--runs', 3), SECS = +arg('--secs', 12);
const SR = 48000, BLOCK = 128;
const PR = require(path.join(ROOT, 'web/src/presets.js'));
const { loadBank } = require('./bank_node.js');
const mod = new WebAssembly.Module(fs.readFileSync(WASM));
const P = PR.P;
const G = { RALG: 42, RLVL: 47, RTIM: 43, UMSK: 50, CALM: 52, CINP: 26, CRAT: 29, CMIX: 32, DMOD: 34, DDRV: 35, DMIX: 39,
  XON: 140, XRND: 141, XTIM: 142, XRPT: 143, XBND: 144, XBRK: 145, XCRP: 146, XLOS: 147, XCMD: 149, XLMD: 150, XRES: 154, XANA: 155, XAMD: 156 };
// the RAM machine and its PRISM side (core/sk_engine.h)
const RAM_M = 27, RP = { RBUF: 56, PMIX: 57, PVOI: 58, PBLR: 59, PGLD: 60, PCTR: 61, PWID: 62, PSPD: 63,
  PWAV: 64, PDYN: 65, POCT: 66, PWRP: 67, PSLC: 68, PTLT: 69, PQNT: 70 };

const readOps = f => fs.readFileSync(path.join(__dirname, 'bench', f), 'utf8').trim().split('\n').map(l => l.trim().split(/\s+/));
const demo = readOps('demo.ops');
const mid = (i, n) => (i + 0.5) / n;

function heavyOps() {
  const ops = demo.slice();
  for (let t = 0; t < 8; t++) {
    for (let s = 0; s < 16; s++) ops.push(['trig', t, s, 1]);
    ops.push(['p', t, P.RTRG, mid(6, 12)], ['p', t, P.RCNT, mid(7, 16)], ['p', t, P.DEC, 0.8]);
    ops.push(['p', t, P.IR, mid(24 - t, 25)], ['p', t, P.IMIX, 0.6], ['p', t, P.REV, 0.4], ['p', t, P.DLY, 0.3]);
    ops.push(['p', t, P.FTYP, mid(1, 4)], ['p', t, P.RES, 0.6], ['p', t, P.DRV, 0.4]);
  }
  ops.push(['g', G.RALG, mid(3, 6)], ['g', G.RLVL, 0.8], ['g', G.UMSK, 1], ['g', G.CALM, 1]);
  ops.push(['g', G.CINP, 0.5], ['g', G.CRAT, mid(4, 10)], ['g', G.CMIX, 1], ['g', G.DMOD, mid(1, 8)], ['g', G.DDRV, 0.5], ['g', G.DMIX, 1]);
  return ops;
}
// waves: the oscillators read from the wave tables (WAVE between TRI and SAW, DYN, the quantiser, slices, warp)
function prismOps(waves) {
  const ops = demo.filter(o => o[0] === 'reset' || o[0] === 'g' || o[0] === 'scale');
  for (let t = 0; t < 8; t++) {
    ops.push(['ramfill', t, 96000, 7 + t]);
    ops.push(['p', t, P.MACH, mid(RAM_M, 64)], ['p', t, P.VOL, 0.3], ['p', t, P.DEC, 1], ['p', t, P.A, 0], ['p', t, P.B, 1], ['p', t, P.C, mid(2, 4)]);
    ops.push(['p', t, RP.PMIX, 1], ['p', t, RP.PVOI, mid(15, 16)], ['p', t, RP.PWID, 0.5], ['p', t, RP.PSPD, 0.5], ['p', t, RP.PBLR, 0.2], ['p', t, RP.PGLD, 0.2]);
    if (waves) ops.push(['p', t, RP.PWAV, 0.5], ['p', t, RP.PDYN, 0.6], ['p', t, RP.PQNT, 0.75], ['p', t, RP.PSLC, mid(2, 7)], ['p', t, RP.PWRP, 0.7], ['p', t, RP.PTLT, 0.6]);
    ops.push(['p', t, P.PROB, 1], ['p', t, P.RTRG, mid(5, 12)], ['p', t, P.RCNT, mid(3, 16)]);
    for (let s = 0; s < 16; s++) ops.push(['trig', t, s, 1]);
  }
  return ops;
}
// the demo with HEX·BD / HEX·SD / HEX·TM and ALLOY three times at its heaviest on tracks 1..6 (v63: its 35 sides): a
// plate of 48 modes, the noise held and heard, flams, fed back and singing, driven, FM swept, 24 sines, the comb swept,
// the phaser, the DAC at 24 kHz, its space (a metal box shifted up); a gong (its bloom), FM, sines, comb, phaser, a room;
// a bowed bell, held, fed back, a hall
function kitOps() {
  const ops = demo.slice();
  const set = (t, m, af, side) => { ops.push(['p', t, P.MACH, mid(m, 64)]); af.forEach((v, i) => ops.push(['p', t, P.A + i, v]));
    for (let i = 0; i < 35; i++) ops.push(['p', t, P.S1 + i, (side || [])[i] || 0]); };
  set(0, 23, [.45, .72, .4, .45, .6, .25]); set(1, 24, [.7, .62, .62, .5, .55, .35]); set(2, 25, [.5, .78, .45, .4, .65, .5]);
  set(3, 37, [mid(3, 8), .7, .9, 1, .4, mid(0, 6)], [.9, .5, .7, .3, .4, .1, .5,  .6, .4, .7, 1, .9, .5, mid(4, 8),  1, .6, .3, 1, .3, 1, .8,
    .6, .5, .6, mid(31, 49), .9, .6, .6,  .5, .05, .6, .4, mid(12, 13), 0, .5]);
  set(4, 37, [mid(6, 8), .5, .9, 1, .7, mid(0, 6)], [.5, .3, .56, 0, .35, .5, .3,  0, 0, .6, .3, 0, 0, mid(2, 8),  1, .5, .55, .8, 0, .6, .5,
    .5, .7, .5, mid(24, 49), .2, .3, .5,  .4, .6, .7, .3, mid(0, 13), 0, .6]);
  set(5, 37, [mid(5, 8), .5, .9, 1, .3, mid(0, 6)], [.5, .2, .5, 0, .35, .5, .3,  1, .3, .6, 1, .95, .3, 0,  1, .3, .3, .4, 0, .35, .5,
    .4, .5, .3, mid(12, 49), .8, 0, .4,  .3, .8, .8, .5, mid(0, 13), 0, .3]);
  ops.push(['p', 3, P.DEC, 0.95], ['p', 4, P.DEC, 0.9], ['p', 5, P.DEC, 1]);
  return ops;
}
// the demo through GLITCH, everything at work: RAND, repeats, bends, breaks, DSTR corruption, the codec (PKT·R)
function glitchOps() {
  const ops = demo.slice();
  ops.push(['g', G.XON, 1], ['g', G.XRND, 0.5], ['g', G.XTIM, mid(2, 7)], ['g', G.XRPT, 0.5], ['g', G.XBND, 0.4], ['g', G.XBRK, 0.4],
    ['g', G.XCRP, 0.5], ['g', G.XCMD, mid(2, 3)], ['g', G.XLOS, 0.6], ['g', G.XLMD, mid(4, 5)], ['g', G.XRES, 0.6]);
  return ops;
}
// the demo with HALO on track 8 at its heaviest: six notes, a new chord every 4 steps (gliding), everything up
function haloOps() {
  const ops = demo.slice(), t = 7, H = { HSWL: 95, HGLD: 96, HMOD: 97, HINV: 98, HDRF: 99, HAIR: 100, HWID: 101 };
  ops.push(['p', t, P.MACH, mid(28, 64)], ['p', t, P.A, mid(7, 12)], ['p', t, P.B, mid(1, 28)], ['p', t, P.C, 0.6], ['p', t, P.D, 0.5],
    ['p', t, P.E, 1], ['p', t, P.VOL, 0.4], ['p', t, H.HGLD, 0.5], ['p', t, H.HDRF, 1], ['p', t, H.HAIR, 1], ['p', t, H.HWID, 1], ['mute', t, 0]);
  for (let s = 0; s < 16; s++) ops.push(['trig', t, s, s % 4 === 0 ? 1 : 0]);
  return ops;
}
// v59: the demo with STRUM on track 8 at its heaviest: an 11th on the pop axis (LEAD), doubled courses, a slow roll down
// and up, a strum a step with a ratchet, the strings ringing on; and with CHOIR on track 8 at its heaviest: four singers
// a part, a triad, breath, its SPACE with a shimmer
function strumOps() {
  const ops = demo.slice(), t = 7;
  ops.push(['p', t, P.MACH, mid(35, 64)], ['p', t, P.A, mid(7, 12)], ['p', t, P.B, mid(13, 28)], ['p', t, P.C, 0.5], ['p', t, P.D, 0.5],
    ['p', t, P.E, mid(2, 4)], ['p', t, P.DEC, 0.95], ['p', t, P.VOL, 0.4], ['p', t, P.HWID, 1], ['p', t, P.SRNG, 1], ['p', t, P.SDBL, 1],
    ['p', t, P.HARM, mid(2, 3)], ['mute', t, 0]);
  for (let s = 0; s < 16; s++) { ops.push(['trig', t, s, 1]); ops.push(['rat', t, s, 2]); }
  return ops;
}
function choirOps() {
  const ops = demo.slice(), t = 7;
  ops.push(['p', t, P.MACH, mid(36, 64)], ['p', t, P.E, 1], ['p', t, P.CPRT, mid(3, 6)], ['p', t, P.C, 0.6], ['p', t, P.CSPC, 0.6],
    ['p', t, P.CSHM, mid(1, 4)], ['p', t, P.VOL, 0.4], ['mute', t, 0]);
  for (let s = 0; s < 16; s++) ops.push(['trig', t, s, s % 4 === 0 ? 1 : 0]);
  return ops;
}
// v66: the demo with CHIMERA on T6..T8 at its heaviest: T8 a kit's kick drawing among its 12 neighbours at each hit,
// grafted on an 808 with the split, a trig a step and ratchets; T7 a cymbal's family on LOOP, grafted on a voice; T6 a
// loop reversed
function chimeraOps() {
  const ops = demo.slice(), S = n => P.S1 + n - 1;
  const set = (t, kv) => { for (const [q, v] of kv) ops.push(['p', t, q, v]); ops.push(['mute', t, 0]); };
  set(7, [[P.MACH, mid(38, 64)], [P.A, mid(0, 10)], [P.B, 0.3], [P.C, 1], [P.D, 0], [P.E, 0], [P.DEC, 1], [P.VOL, 0.4], [S(1), mid(6, 11)], [S(5), 0.5]]);
  set(6, [[P.MACH, mid(38, 64)], [P.A, mid(6, 10)], [P.B, 0.5], [P.C, 0], [P.D, 0], [P.E, 0], [P.DEC, 1], [P.VOL, 0.4], [S(1), mid(8, 11)]]);
  set(5, [[P.MACH, mid(38, 64)], [P.A, mid(9, 10)], [P.B, 0.2], [P.C, 0.5], [P.D, 0], [P.E, 1], [P.DEC, 1], [P.VOL, 0.4]]);
  for (let s = 0; s < 16; s++) { ops.push(['trig', 7, s, 1], ['rat', 7, s, 2], ['trig', 6, s, 1], ['trig', 5, s, s % 4 === 0 ? 1 : 0]); }
  return ops;
}
// the demo with BREAK on track 8 at its heaviest: three slots filled, a trig a step, every chop (SWAP between the
// slots), VARI, DUST
function breakOps() {
  const ops = demo.slice(), t = 7, B = { BRND: 102, BREV: 103, BSTU: 104, BPJM: 105, BSKP: 106, BSWP: 108 };
  ops.push(['brkfill', 0, 2, 120, 7], ['brkfill', 1, 1, 96, 11], ['brkfill', 2, 4, 150, 13]);
  ops.push(['p', t, P.MACH, mid(29, 64)], ['p', t, P.A, mid(0, 10)], ['p', t, P.B, mid(1, 2)], ['p', t, P.C, 0], ['p', t, P.D, mid(7, 8)],
    ['p', t, P.E, 0.6], ['p', t, P.DEC, 1], ['p', t, P.VOL, 0.4], ['p', t, B.BRND, 0.4], ['p', t, B.BREV, 0.2], ['p', t, B.BSTU, 0.3],
    ['p', t, B.BPJM, 0.2], ['p', t, B.BSWP, 0.3], ['p', t, B.BSKP, 0.05], ['mute', t, 0]);
  for (let s = 0; s < 16; s++) ops.push(['trig', t, s, 1]);
  return ops;
}
// the five FX machines in a chain on T4..T8 (BEND on T1..T3, then each on the one before), each at its heaviest:
// BEND repeating and breaking, TAPE worn, VERB with both modulations (half sine, half grains) deep, ECHO with HALO
// and µ, CODEC losing packets with RESO
function fxOps() {
  const ops = demo.slice(), X = { XA: 109, XB: 110, XC: 111, XD: 112, XE: 113 };
  const set = (t, m, src, kv) => { ops.push(['p', t, P.MACH, mid(m, 64)], ['p', t, P.VOL, 0.8], ['p', t, P.A, mid(src, 10)], ['p', t, P.DEC, 1], ['mute', t, 0]);
    for (const k in kv) ops.push(['p', t, k in X ? X[k] : P[k], kv[k]]); for (let s = 0; s < 16; s++) ops.push(['trig', t, s, 0]); };
  set(3, 32, 1, { B: mid(1, 6), C: mid(3, 8), D: 0.5, E: 1, XA: 0.4, XB: mid(2, 3), XC: 0.6, XD: mid(1, 2) });
  set(4, 33, 0, { B: 0.5, C: 0.4, D: 0.6, E: 1, XA: 0.6, XB: 0.6, XC: 0.4 });
  set(5, 30, 0, { B: 0.8, C: 0.7, D: 0.3, E: 0.5, XA: 0.5, XB: 0.6, XC: 0.3, XD: 0.5 });
  set(6, 31, 0, { B: mid(2, 4), C: 0.4, D: 0.7, E: 0.5, XA: 0.5, XB: 0.8, XC: 0.5, XD: mid(1, 2) });
  set(7, 34, 0, { B: 0.7, C: 0.5, D: 0.8, E: 1, XA: 0.5, XB: 0.4, XD: 0.6 });
  return ops;
}
// v68: the demo with every track sending to the CHORUS (mode 4) and the PHASER (in series, square sweeps, feedback),
// the delay on 1982 with its VCO and the envelope on it
function sendsOps() {
  const ops = demo.slice(), GX = { CHMD: 158, PSH1: 163, PSH2: 165, PFBA: 167, PFBB: 169, PRTE: 172, DMDL: 175, DVDP: 177, DVRT: 178, DVEN: 180 };
  for (let t = 0; t < 8; t++) ops.push(['p', t, P.CHO, 0.4], ['p', t, P.PHS, 0.4], ['p', t, P.DLY, 0.3]);
  ops.push(['g', GX.CHMD, mid(3, 4)], ['g', GX.PSH1, 0.7], ['g', GX.PSH2, 0.4], ['g', GX.PFBA, 0.7], ['g', GX.PFBB, 0.6], ['g', GX.PRTE, mid(2, 3)],
    ['g', GX.DMDL, mid(1, 2)], ['g', GX.DVDP, 0.3], ['g', GX.DVRT, 0.5], ['g', GX.DVEN, 0.8]);
  return ops;
}
// v68: the demo through FUZZ on T8 (SUM: T1..T7), at its heaviest: driven hard, SUST, its gate, every band through it
function fuzzOps() {
  const ops = demo.slice(), X = { XA: 109, XB: 110, XC: 111, XD: 112, XE: 113, XF: 114, XG: 115 }, t = 7;
  ops.push(['p', t, P.MACH, mid(39, 64)], ['p', t, P.VOL, 0.8], ['p', t, P.A, mid(1, 10)], ['p', t, P.DEC, 1], ['p', t, P.B, 0.9], ['p', t, P.C, 0.35],
    ['p', t, P.D, 0.6], ['p', t, P.E, 1], ['p', t, X.XA, 0.7], ['p', t, X.XB, 0.8], ['p', t, X.XC, 0.7], ['p', t, X.XG, 0.3], ['mute', t, 0]);
  for (let s = 0; s < 16; s++) ops.push(['trig', t, s, 0]);
  return ops;
}
// the same with LOSS on the tape (WORN: every failure at work)
function tapeOps() { return glitchOps().concat([['g', G.XANA, 1], ['g', G.XAMD, mid(4, 5)]]); }
function idleOps() { const ops = demo.slice(); for (let t = 0; t < 8; t++) ops.push(['mute', t, 1]); return ops; }
// WEST's side places as a fresh track has them (app.js MACH, core machineSideDefaults)
// (v78: its 70, S1 .. S35 then S36 ..: 203 + q - 35)
const WEST_SD = [0, 0.25, 0, 0.15, 0.65, 0.3, 0.25, 2.5 / 3, 0.5 / 3, 0.25, 0.5, 0.6, 0, 0, 16.5 / 17, 0.5, 0.6, 0, 1, 0, 0,
  0.5, 0.5, 0.5, 0.5, 0.5, 0.9, 0.125, 0.125, 0, 0, 0, 0.125, 0.15, 0.45,
  0.5 / 3, 0.25, 0.5 / 8, 0.5, 1.5 / 8, 1.5 / 8, 0.5 / 8, 0.5, 2.5 / 3, 1.5 / 6, 1.5 / 3, 1, 1.5 / 3, 0.125, 0.5 / 32,
  0, 1.5 / 8, 1, 0.5 / 3, 0.5 / 3, 0, 1.5 / 8, 1, Math.SQRT1_2, 0.5 / 8, 0.5, 0.5, 0.5, 0.5, 0.5, 0, 0, 0, 0, 0];
const wsQ = q => q < 35 ? 155 + q : 203 + q - 35;
function machineOps(m) {
  const ops = demo.filter(o => o[0] === 'reset' || o[0] === 'g' || o[0] === 'scale');
  for (let t = 0; t < 8; t++) { ops.push(['p', t, P.VOL, 0]); for (let s = 0; s < 16; s++) ops.push(['trig', t, s, 0]); }
  ops.push(['p', 0, P.MACH, mid(m, 64)], ['p', 0, P.VOL, 0.7], ['p', 0, P.DEC, 0.7], ['p', 0, P.PROB, 1]);
  for (let s = 0; s < 16; s++) ops.push(['trig', 0, s, 1]);
  ops.push(['p', 0, P.RTRG, mid(5, 12)], ['p', 0, P.RCNT, mid(3, 16)]);
  if (m === 38) ops.push(['p', 0, P.A, mid(0, 10)], ['p', 0, P.E, 0]);    // (CHIMERA: a kick, forward)
  // (v76: WEST with its side places as a fresh track has them, its pulser at 1/32: the heaviest it plays)
  if (m === 41) { WEST_SD.forEach((v, i) => ops.push(['p', 0, wsQ(i), v])); ops.push(['p', 0, P.E, mid(3, 11)]); }
  return ops;
}

function apply(x, o) {
  const n = o.slice(1).map(Number);
  switch (o[0]) {
    case 'p': x.sk_set_param(n[0], n[1], n[2]); break;
    case 'trig': x.sk_set_trig(n[0], n[1], n[2]); break;
    case 'lock': x.sk_set_lock(n[0], n[1], n[2], n[3]); break;
    case 'clrl': x.sk_clear_locks(n[0], n[1]); break;
    case 'rat': x.sk_set_ratchet(n[0], n[1], n[2]); break;
    case 'len': x.sk_set_len(n[0], n[1]); break;
    case 'spd': x.sk_set_speed(n[0], n[1]); break;
    case 'mute': x.sk_set_mute(n[0], n[1]); break;
    case 'g': x.sk_set_global(n[0], n[1]); break;
    case 'reset': x.sk_reset(); break;
    case 'scale': x.sk_set_scale(n[0]); break;
    case 'qtrk': x.sk_set_qtrk(n[0], n[1]); break;
    // a BREAK loop (seeded): a decaying noise burst on every 16th, louder on the beats: slot, bars, tempo, seed
    case 'brkfill': {
      const len = Math.round(n[1] * 240 / n[2] * 32000), m = new Int16Array(x.memory.buffer, x.sk_break_mem(n[0]), len), st = len / (16 * n[1]);
      let s = n[3] >>> 0 || 1;
      for (let i = 0; i < len; i++) {
        s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0;
        const k = Math.floor(i / st), t = (i - k * st) / 32000;
        m[i] = Math.round((((s >>> 16) & 0xFFFF) - 32768) * (k % 4 ? 0.15 : 0.5) * Math.exp(-t / 0.05));
      }
      x.sk_break_set(n[0], len, n[1]);
      break;
    }
    // a RAM take of white noise (seeded): track, samples, seed
    case 'ramfill': {
      const m = new Int16Array(x.memory.buffer, x.sk_ram_mem(n[0]), n[1]);
      let s = n[2] >>> 0 || 1;
      for (let i = 0; i < n[1]; i++) { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; m[i] = ((s >>> 16) & 0xFFFF) - 32768 >> 2; }
      x.sk_ram_loaded(n[0], n[1], SR);
      break;
    }
  }
}

// one render: ops (with "at <s>" events), SECS of audio in 128-frame blocks, every block timed
function run(ops, keep) {
  const x = new WebAssembly.Instance(mod, {}).exports;
  x.sk_init(SR);
  // CHIMERA plays the bank (decoded before the clock starts: the page's worklet decodes it over its first seconds)
  if (ops.some(o => o[0] === 'p' && +o[2] === P.MACH && Math.floor(+o[3] * 64) === 38)) loadBank(x);
  const timed = []; let at = 0;
  for (const o of ops) { if (o[0] === 'at') { at = +o[1]; continue; } if (at > 0) timed.push([at, o]); else apply(x, o); }
  x.sk_set_play(1);
  const N = Math.floor(SECS * SR / BLOCK);
  const ol = new Float32Array(x.memory.buffer, x.sk_out_l(), 4096), or = new Float32Array(x.memory.buffer, x.sk_out_r(), 4096);
  const out = keep ? new Float32Array(N * BLOCK * 2) : null;
  const times = new Float64Array(N);
  let ti = 0;
  for (let b = 0; b < 400; b++) x.sk_process(BLOCK);          // warm-up (JIT tiers, caches), not timed nor kept
  let bad = 0;
  for (let b = 0; b < N; b++) {
    while (ti < timed.length && timed[ti][0] * SR <= b * BLOCK) apply(x, timed[ti++][1]);
    const t0 = performance.now();
    x.sk_process(BLOCK);
    times[b] = performance.now() - t0;
    if (out) { out.set(ol.subarray(0, BLOCK), b * BLOCK * 2); out.set(or.subarray(0, BLOCK), b * BLOCK * 2 + BLOCK); }
    if (!Number.isFinite(ol[0]) || !Number.isFinite(or[BLOCK - 1])) bad++;
  }
  const sorted = Array.from(times).sort((a, b) => a - b);
  const total = times.reduce((a, b) => a + b, 0);
  return { cpu: 100 * total / (SECS * 1000), p99: sorted[Math.floor(N * 0.99)], worst: sorted[N - 1], out, bad };
}

function bench(name, ops) {
  const rs = [];
  for (let r = 0; r < RUNS; r++) rs.push(run(ops, r === 0 && (SAVE || CHECK)));
  const med = k => rs.map(x => x[k]).sort((a, b) => a - b)[Math.floor(rs.length / 2)];
  const res = { name, cpu: med('cpu'), best: Math.min(...rs.map(x => x.cpu)), p99: med('p99'), worst: Math.min(...rs.map(x => x.worst)), bad: rs[0].bad };
  const out = rs[0].out;
  if (SAVE && out) fs.writeFileSync(path.join(SAVE, name + '.f32'), Buffer.from(out.buffer));
  if (CHECK && out) {
    const f = path.join(CHECK, name + '.f32');
    if (fs.existsSync(f)) {
      const b = fs.readFileSync(f), ref = new Float32Array(b.buffer, b.byteOffset, b.length >> 2);
      let md = 0, e = 0, s = 0, same = 0;
      for (let i = 0; i < ref.length; i++) { const d = out[i] - ref[i]; md = Math.max(md, Math.abs(d)); e += d * d; s += ref[i] * ref[i]; if (d === 0) same++; }
      res.null = md === 0 ? 'bit-exact' : `max ${md.toExponential(1)}, residual ${(10 * Math.log10(e / (s || 1e-30))).toFixed(0)} dB`;
    }
  }
  return res;
}

if (SAVE) fs.mkdirSync(SAVE, { recursive: true });
// --dump <dir>: write the scenarios as op files (for the native harness: render_test ops <file> <wav> <s>)
if (arg('--dump', null)) {
  const d = arg('--dump'); fs.mkdirSync(d, { recursive: true });
  const w = (n, ops) => fs.writeFileSync(path.join(d, n + '.ops'), ops.map(o => o.join(' ')).join('\n') + '\n');
  w('idle', idleOps()); w('demo', demo); w('kit', kitOps()); w('heavy', heavyOps()); w('prism', prismOps()); w('prismw', prismOps(true)); w('glitch', glitchOps()); w('tape', tapeOps()); w('halo', haloOps()); w('break', breakOps()); w('fx', fxOps()); w('strum', strumOps()); w('choir', choirOps()); w('chimera', chimeraOps()); w('sends', sendsOps()); w('fuzz', fuzzOps());
  PR.MACH_IDS.forEach((id, m) => w('m-' + id.replace(/[^A-Za-z0-9]/g, ''), machineOps(m)));
  process.exit(0);
}
const budget = BLOCK / SR * 1000;
const scen = [['idle', idleOps()], ['demo', demo], ['kit', kitOps()], ['heavy', heavyOps()], ['prism', prismOps()], ['prismw', prismOps(true)], ['glitch', glitchOps()], ['tape', tapeOps()], ['halo', haloOps()], ['break', breakOps()], ['fx', fxOps()], ['strum', strumOps()], ['choir', choirOps()], ['chimera', chimeraOps()], ['sends', sendsOps()], ['fuzz', fuzzOps()]];
const want = n => !ONLY || ONLY.split(',').some(w => n === w || (w === 'machines' && n.startsWith('m:')));
console.log(`engine ${path.basename(WASM)} · ${SECS}s per run · median of ${RUNS} · block ${BLOCK} frames = ${budget.toFixed(2)} ms at 48 kHz`);
console.log('scenario'.padEnd(12) + 'CPU %'.padStart(7) + 'best %'.padStart(8) + 'p99 ms'.padStart(9) + 'worst ms'.padStart(10) + '  null test');
let idle = 0;
for (const [n, ops] of scen) {
  if (!want(n)) continue;
  const r = bench(n, ops);
  if (n === 'idle') idle = r.best;
  console.log(n.padEnd(12) + r.cpu.toFixed(1).padStart(7) + r.best.toFixed(1).padStart(8) + r.p99.toFixed(3).padStart(9) + r.worst.toFixed(3).padStart(10) + '  ' + (r.null || '') + (r.bad ? '  NaN!' : ''));
}
if (want('m:x') || (ONLY && ONLY.includes('machines'))) {
  if (!idle) idle = bench('idle', idleOps()).best;
  console.log('\nper machine (track 1 alone, ratchets, CPU above idle):');
  const rows = [];
  PR.MACH_IDS.forEach((id, m) => {
    const r = bench('m-' + id.replace(/[^A-Za-z0-9]/g, ''), machineOps(m));
    rows.push([id, r.best - idle, r.p99, r.worst, r.null || '']);
  });
  console.log('  machine    CPU %   p99 ms  worst ms');
  rows.sort((a, b) => b[1] - a[1]).forEach(([id, c, p, w, nl]) => console.log('  ' + id.padEnd(8) + c.toFixed(2).padStart(7) + p.toFixed(3).padStart(9) + w.toFixed(3).padStart(10) + '  ' + nl));
}
