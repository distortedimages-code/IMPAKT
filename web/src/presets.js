// IMPAKT — factory presets, 20 per machine, an homage to the Warp Records family and its orbit.
// Each preset is [tag, name, params]. Tags name the inspiration (techniques, gear, records), never
// a claim of authorship. Params use short keys (see KEYS below); everything a preset does not set
// falls back to BASE, so a preset always sounds the same whatever was on the track before.
// The levels (VOL) are measured by tools/preset_levels.js and stored in preset_levels.js.
const IMPAKT_PRESETS = (() => {
  const st = n => 0.5 + n / 48;              // TUNE / NOTE in semitones
  const ix = (i, n) => (i + 0.5) / n;        // stepped control
  const r = i => ix(i, 24);                  // FM ratio list: ¼ ½ ¾ 1 1.25 1.41 1.5 1.73 2 2.5 2.73 3 3.14 3.5 4 4.5 5 6 7 8 9.25 11 13.5 16
  const iv = n => ix(n + 24, 49);            // TRIO interval in semitones
  const ORG = ['RAMEAU','BRANCHE','BOIS','BAMBOU','FEUILLES','GRAINES','CHIPS','BISCOTTE','CÉLERI','COQUILLE','NOIX',
    'FEU','ALLUMETTE','GRAVIER','CAILLOUX','SABLE','NEIGE','GLAÇON','VERRE','GOUTTE','BULLES','PLUIE','PSCHITT','BOUCHON',
    'PAPIER','SACHET','P.BULLE','ZIP','VELCRO','CLAP','DOIGTS','LANGUE','PHALANGE','PINGPONG','PIÈCES','GRILLON'];
  const IRS = ['OFF','JOUET','TÉLÉ','CANETTE','TALKIE','RADIO','B.MUSIQ','MÉGA','TUBE','BIDON','AMP 8','CARTON',
    'AMP 12','BASS 15','4×12','PIANO','PLACARD','AUTO','CABINE','BAIN','SALON','GARAGE','ESCALIER','TUNNEL','ÉGLISE'];
  const DEST = ['OFF','TUNE','DEC','A','B','C','D','E','F','FREQ','RES','DRV','BITS','SRR','VOL','PAN','DLY','REV',
    'FENV','IR','IMIX','IDCY','IPRE','ITON','RTRG','RCNT','RVEL','PMIX','WAVE','WARP','CNTR','WDTH','SPED','BLUR','TILT',
    'HP','LP','MFRQ'];
  const org = n => { const i = ORG.indexOf(n); if (i < 0) throw new Error('org ' + n); return ix(i, 36); };
  const loop = n => { const s = ['FM', 'BRUIT', 'ZAP', 'ACCORD']; const i = s.indexOf(n); return ix(i >= 0 ? i : 4 + ORG.indexOf(n), 40); };
  const seed = i => ix(i, 16);
  // HALO: a shape of CHRD, a progression of PROG
  const ch = n => { const i = ['5TH', 'SUS2', 'SUS4', 'TRI', 'ADD9', '7TH', '9TH', '11TH', '6/9', 'QRTL', '5THS', 'CLST'].indexOf(n); if (i < 0) throw new Error('chord ' + n); return ix(i, 12); };
  // BREAK: a LEN, SYNC / VARI (its MODE, b)
  const len = n => { const i = ['1/32', '1/16', '1/8', '3/16', '1/4', '1/2', '1 BAR', 'FREE'].indexOf(n); if (i < 0) throw new Error('len ' + n); return ix(i, 8); };
  const VARI = ix(1, 2);
  // FX: ECHO's zones, SYNC / FREE (ECHO, BEND), BEND's divisions and repeats and corrupt modes, TAPE's machines, CODEC's
  // INV, what a trig does (ACT: IN, or the gesture)
  const pick = (list, n, what) => { const i = list.indexOf(n); if (i < 0) throw new Error(what + ' ' + n); return ix(i, list.length); };
  const zn = n => pick(['KARP', 'SLAP', 'ECHO', 'LONG'], n, 'zone');
  // v83, STENCIL (core/sk_stencil): its zones; RATE for a time (s) in a zone, or a pitch (Hz: KARP, COMB), or (SYNC) a
  // division at 120 BPM (the nearest is taken at any tempo); REPT in % (0 .. 125); SKEW for a ratio (free: x0.67 ..
  // x1.5; SYNC: the nearest of its ratios)
  const STZ = ['KARP', 'COMB', 'SLAP', 'ECHO', 'TAPE', 'LONG', 'VAST', 'HUGE'];
  const SZL = [0.0003, 0.004, 0.03, 0.12, 0.35, 0.8, 1.5, 2.5], SZH = [0.012, 0.045, 0.22, 0.6, 1.2, 2.0, 3.5, 5.4];
  const szn = n => pick(STZ, n, 'zone');
  const srate = (zone, sec) => { const z = STZ.indexOf(zone); if (z < 0) throw new Error('zone ' + zone); const r = Math.log(sec / SZH[z]) / Math.log(SZL[z] / SZH[z]); if (r < 0 || r > 1) throw new Error('rate ' + zone + ' ' + sec); return r; };
  const shz = (zone, hz) => srate(zone, 1 / hz);
  const sdv = (zone, n) => { const d = { '1/16': 1, '1/8T': 4 / 3, '1/16D': 1.5, '1/8': 2, '1/4T': 8 / 3, '1/8D': 3, '1/4': 4, '1/2T': 16 / 3, '1/4D': 6, '1/2': 8, '1/2D': 12, '1 BAR': 16, '2 BAR': 32 }[n]; if (!d) throw new Error('division ' + n); return srate(zone, d * 0.125); };
  const srp = pct => pct / 125;
  // v87, RADIO (core/sk_radio.h): its modes, LEN (rlen: sixteenths, FREE), SQL (rsql: dB), GRAB, DPTH (rdpt: s), DEC
  // (rdec: s), the shift on LIVE (rshf: Hz), AIR's BAND (rband: Hz, null FULL), CNTR (rcntr: Hz), RATE (rrate: Hz)
  const rmode = n => pick(['LIVE', 'LOOP', 'DIG'], n, 'radio mode');
  const rlen = n => pick(['1/16', '1/8', '3/16', '1/4', '3/8', '1/2', '3/4', '1 BAR', '1½BAR', '2 BAR', 'FREE'], n, 'radio len');
  const rsql = db => (db + 66) / 56, rdpt = sec => Math.log(sec) / Math.log(20), rdec = sec => Math.log(sec / 0.02) / Math.log(400);
  const rshf = hz => 0.5 + hz / 25 / 48, rband = hz => hz ? Math.log(hz / 12000) / Math.log(250 / 12000) : 0;
  const rcntr = hz => Math.log10(hz / 300), rrate = hz => Math.log(hz / 0.04) / Math.log(100);
  const sskw = r => 0.5 + Math.log2(r) / 0.585 / 2;
  const SSK = { '×1/2': 0.5 / 7, '×2/3': 1.5 / 7, '×3/4': 2.5 / 7, '×1': 3.5 / 7, '×4/3': 4.5 / 7, '×3/2': 5.5 / 7, '×2': 6.5 / 7 };
  const sskq = n => { if (!(n in SSK)) throw new Error('skew ' + n); return SSK[n]; };
  const SYNC = ix(1, 2), FREE = ix(0, 2);
  const bdv = n => pick(['1/32', '1/16', '1/8', '1/4', '1/2', '1 BAR'], n, 'division');
  const rpt = n => pick(['OFF', '2', '3', '4', '6', '8', '12', '16'], n, 'repeats');
  const cmd = n => pick(['DECIM', 'DROP', 'DSTR'], n, 'corrupt');
  const tm = n => pick(['CASS', 'VHS', 'REEL', 'MICRO'], n, 'tape');
  const INV = ix(1, 2);
  const act = (i, n) => ix(i, n);
  // VERB's SHFT (none, then from an octave down to one up) and BASS (×1 .. ×4 how much longer the lows last)
  const shf = n => pick(['OFF', '-12', '-7', '-5', '-3', '-1', '-10c', '+10c', '+1', '+3', '+5', '+7', '+12'], n, 'shift');
  const bass = x => Math.log(x) / Math.log(4);
  const PROGS = ['OFF', 'AEON', 'DUSK', 'TIDE', 'VAMP', 'DAWN', 'FALL', 'RISE', '4THS', 'AMBR', 'SLIP', 'MEDI',
    'CANON', 'AXIS', '50S', 'CADN', 'TURN', 'II-V', 'ROYL', 'EPIC', 'FOLIA', 'ROMA', 'HYMN', 'PEDL', 'TIN↓', 'TIN↑', 'MIRR', 'WALK'];
  const pg = n => { const i = PROGS.indexOf(n); if (i < 0) throw new Error('prog ' + n); return ix(i, PROGS.length); };
  // v59: HARM (HALO, STRUM), STRUM's materials and directions, CHOIR's vowels (a: 0 MM .. 1 EE), parts, the SPACE's shimmer
  const hm = n => pick(['FREE', 'PURE', 'LEAD'], n, 'harm');
  const mat = n => pick(['NYLON', 'STEEL', 'ELEC', 'HARP', 'KOTO', 'SYNTH'], n, 'material');
  const dir = n => pick(['DOWN', 'UP', 'D/U', 'RAND'], n, 'direction');
  // v73: FUZZ's circuits (CIRC)
  const cir = n => pick(['FUZZ', 'PRE', 'SAT', 'XCIT', 'MID', 'XSTR', 'GAIN', 'BENT'], n, 'circuit');
  // v74: CUBE: a place in the cube (cx: 1 .. 8, along X, Y and Z), an offset in waves (co: -7 .. +7; SWEP, BX, BY,
  // SWPY), B's detune (cents), UNIS (cents), STIM and PTIM (ms), the pitch envelope (semitones), STEP, WARP (-1 .. 1),
  // BITS, DEC (seconds; 1 = HOLD)
  const cx = w => (w - 1) / 7, co = w => 0.5 + w / 14, cdt = c => 0.5 + c / 100, cun = c => c / 25;
  const cst = ms => Math.log(ms / 5) / Math.log(800), cpe = st => 0.5 + st / 96, cpt = ms => Math.log(ms / 2) / Math.log(500);
  const cstp = n => pick(['OFF', 'X', 'X+Y', 'XYZ'], n, 'step'), cwp = x => 0.5 + x / 2, cbit = n => pick(['OFF', '12', '10', '8', '6', '4'], n, 'bits');
  const cdc = sec => Math.log(sec / 0.01) / Math.log(400);
  // v76: WEST: DEC (wdc: seconds), RATI by name (wrt), PULS (wdv: OFF or the division), CNT (wcnt: 1 .. 16 or '∞'), a
  // bipolar knob (wb: -1 .. 1), a step in semitones (wst: -12 .. 12; DEST PTCH), the times (watk, wsus, wglid: s), the
  // spring's decay (wsdc: s), the modulator's free rate (wfr: Hz), WARM (wwm: Hz), the switches by name
  const wdc = sec => Math.log(sec / 0.005) / Math.log(1400);
  const wrt = n => pick(['¼', '½', '¾', '1', '5/4', '√2', '1.5', '2', '1+√2', '2.76', '3', '3.5', '4', '5.40', '7', '8.93'], n, 'ratio');
  const wdv = n => pick(['OFF', '1/64', '1/48', '1/32', '1/24', '1/16', '1/12', '1/8', '3/16', '1/4', '1/2'], n, 'division');
  const wcnt = n => n === '∞' ? ix(16, 17) : ix(n - 1, 17), wb = x => 0.5 + x / 2, wst = st => 0.5 + st / 24;
  const watk = sec => Math.log(sec / 0.001) / Math.log(2000), wsus = sec => Math.sqrt(sec / 2), wglid = sec => Math.log(sec / 0.003) / Math.log(300);
  const wsdc = sec => Math.log10(sec / 0.4) / 1.2, wfr = hz => Math.log(hz / 0.1) / Math.log(20000), wwm = hz => Math.log(hz / 20000) / Math.log(0.0665);
  // (v78, EaseLFO's panel: the modulator's waves SAW SQR TRI, gate 1's modes BOTH RESO VCA LPF, the envelope's TRAN SUST
  // CYCL OFF, the modulation's destinations FM AM BAL)
  const wwv = n => pick(['SAW', 'SQR', 'TRI'], n, 'wave'), wgm = n => pick(['BOTH', 'RESO', 'VCA', 'LPF'], n, 'gate');
  const wenv = n => pick(['TRAN', 'SUST', 'CYCL', 'OFF'], n, 'envelope'), wdst = n => pick(['PTCH', 'TIMB', 'MOD', 'LVL'], n, 'destination');
  const wlen = n => ix(n - 1, 5), WAM = ix(1, 3), WFREE = ix(1, 2), wfold = n => pick(['SINE', 'CELL'], n, 'fold');
  // (a sequence: up to five steps, its LEN from how many)
  const wseq = (vals, dest, f = wb) => { const o = { wln: wlen(vals.length), wds: wdst(dest) }; vals.forEach((v, i) => { o['wv' + (i + 1)] = f(v); }); return o; };
  // v81: HOWL: its chords (htp), voicings (hvc), DROP (hdr), MRPH (hmr: seconds, 0 = at once), SUST (hsu: seconds; 1 =
  // ∞), the feedback's bloom (hblm: seconds, 0 = at once); v82: the air's length (htime: ms, LOOP's TIME), the loop's
  // colour (hcolr: Hz)
  const htp = n => pick(['5TH', 'OCT', 'SUS2', 'SUS4', 'MAJ', 'MIN', 'ADD9', 'MADD9', 'MAJ7', 'MIN7', 'DOM7', 'MIN9', '5+♭6', 'KEY', 'POLY'], n, 'chord');
  const hvc = n => pick(['DRONE', 'OPEN', 'BARRE', 'SPRD', 'CLUST', '1ST', '2ND', 'HIGH'], n, 'voicing');
  const hdr = n => pick(['0', 'SUB', '-12', '-12S', '-24'], n, 'drop');
  const hmr = sec => sec ? Math.log(sec / 0.02) / Math.log(1500) : 0, hsu = sec => sec === '∞' ? 1 : Math.log2(sec / 0.4) / 6.3;
  const htime = ms => Math.log(ms / 0.3) / Math.log(80 / 0.3), hblm = sec => sec ? Math.log(sec / 0.0525) / Math.log(400) : 0;
  const hcolr = hz => Math.log(hz / 70) / Math.log(6000 / 70);
  // v83, HOWL's BOW: which string (ALL, 1 the highest .. 6), its mode, its swell (s)
  const hstr = n => pick(['ALL', '1', '2', '3', '4', '5', '6'], n, 'string');
  const hbmd = n => pick(['FUND', 'OCT', '12TH'], n, 'bow mode');
  const hswl = sec => Math.log(sec / 0.02) / Math.log(300);
  // v84, MANTRA: what a trig touches (ytch), a voice's TUNE for the note it sounds (ynote: the voice 0 .. 7, the note,
  // its cents, its group's PITCH in semitones; core mantraTuneHz), a group's PITCH (ypch: semitones), a pair's source
  // (ysrc), the LFO's frequency (ylfo: Hz), the delay's times (ydt: ms), DEC's slow release (yrel: s); its switches
  const ytch = n => pick(['1', '2', '3', '4', '5', '6', '7', '8', '12', '34', '56', '78', '1234', '5678', 'ALL'], n, 'touch');
  const NOTE_I = { C: 0, 'C#': 1, D: 2, 'D#': 3, E: 4, F: 5, 'F#': 6, G: 7, 'G#': 8, A: 9, 'A#': 10, B: 11 };
  const ynote = (k, n, cents = 0, pst = 0) => {
    const m = /^([A-G]#?)(-?\d)$/.exec(n); if (!m) throw new Error('note ' + n);
    const hz = 440 * Math.pow(2, (12 * (+m[2] + 1) + NOTE_I[m[1]] - 69 + cents / 100 - pst) / 12), hi = k < 2 ? 600 : k < 6 ? 1200 : 2400;
    const u = (hi / hz - 1) / 19; if (u < 0 || u > 1) throw new Error('MANTRA voice ' + (k + 1) + ' cannot sound ' + n);
    return 1 - Math.sqrt(u);
  };
  const ypch = st => 0.85 + st / 48, ysrc = n => pick(['OFF', 'FM', 'LFO'], n, 'source');
  const ylfo = hz => Math.log10(hz / 0.03) / 4, ydt = ms => Math.log(ms / 8) / Math.log(137.5), yrel = sec => Math.log2(sec / 0.5) / 6;
  const Y_ON = ix(1, 2), Y_FAST = Y_ON, Y_AND = Y_ON, Y_SELF = Y_ON, Y_SQR = Y_ON, Y_1LOOP = Y_ON;
  const vow = n => { const i = ['MM', 'OO', 'OH', 'AH', 'EH', 'EE'].indexOf(n); if (i < 0) throw new Error('vowel ' + n); return i / 5; };
  const part = n => pick(['SOLO', '+3RD', '+5TH', 'TRIAD', '-3RD', '-OCT'], n, 'part');
  const shim = n => pick(['OFF', 'OCT↑', '5TH↑', 'OCT↓'], n, 'shimmer');
  const MODELS = ['VA', 'WSHP', 'FM', 'GRAN', 'HARM', 'WTBL', 'CHRD', 'SPCH', 'SWRM', 'NOIS', 'PART', 'STRG', 'MODL', 'BD', 'SD', 'HH',
    'VCF', 'PD', '6OP·A', '6OP·B', '6OP·C', 'TERR', 'STRM', 'CHIP'];
  const mo = n => { const i = MODELS.indexOf(n); if (i < 0) throw new Error('model ' + n); return ix(i, 24); };   // MACRO model
  // v62: ALLOY's shapes (sh), the pitch at the hit (pe: semitones, -36 .. +48), its time (pt: ms), the noise's length (nl:
  // ms; 1 = HELD), the FM ratio (ra: 0.5 .. 16 on a log scale), its depth's fall (ic: ms; 1 = HOLD), the sines' band
  // (bd: times the note), the comb's feedback (cf: %, under 0 the odd harmonics), the converter (dc)
  const sh = n => pick(['WIRE', 'BAR', 'TUBE', 'PLATE', 'DISC', 'BELL', 'GONG', 'BOWL'], n, 'shape');
  const pe = x => x >= 0 ? 0.5 + x / 96 : 0.5 + x / 72;
  const pt = ms => Math.log2(ms) / 7.64, nl = ms => Math.log2(ms) / 11, ic = ms => Math.log2(ms / 2) / 10;
  const ra = x => Math.log2(x / 0.5) / 5, bd = x => Math.log2(x / 2) / 4.585, cf = pc => 0.5 + pc / 199;
  const dc = n => pick(['OFF', '16', '12', '12·32k', '12·24k', '10·16k', '8·11k', '6·8k'], n, 'dac');
  // v63: the pitch's rise (pr: ms), BNCE (bo: -1 four flams .. 0 none .. 1 bounces), the FM depth's rise (ia: ms), the
  // ratio's sweep (me: octaves, -3 .. +3), the space's decay (dy: seconds), the gate (ct: ms; 0 = OFF)
  const pr = ms => Math.log2(ms) / 8.6, bo = x => 0.5 + x / 2, ia = ms => Math.log2(ms) / 11, me = o => 0.5 + o / 6;
  const dy = s => Math.log(s / 0.08) / Math.log(50), ct = ms => ms ? Math.log2(ms / 20) / 6.2 : 0;
  const dx = i => (i + 0.5) / (32 * 1.02);   // MACRO six-op: voice i of the bank (HARMONICS)
  // v66: CHIMERA's samples by name (web/src/chimera_names.js, made with the bank by tools/make_bank.py): sm (the sample,
  // A and B), bod (the GRAFT's body, BODY and BSMP); VARI with n neighbours (vr: 'RND' or 'LOOP'); the seam (ms), the
  // crossfade (ms), the split (Hz, 0 = none), the body's level (dB)
  const CN = typeof IMPAKT_CHIM_NAMES !== 'undefined' ? IMPAKT_CHIM_NAMES : (typeof require === 'function' ? require('./chimera_names.js') : { cats: [], names: [] });
  const cpos = nm => { for (let c = 0; c < CN.names.length; c++) { const i = CN.names[c].indexOf(nm); if (i >= 0) return [c, i]; } throw new Error('CHIMERA sample ' + nm); };
  const sm = nm => { const [c, i] = cpos(nm); return { a: ix(c, CN.cats.length), b: ix(i, CN.names[c].length) }; };
  const bod = nm => { const [c, i] = cpos(nm); return { gbc: ix(c + 1, CN.cats.length + 1), gbs: ix(i, CN.names[c].length) }; };
  const vr = (mode, n) => 0.5 + (mode === 'RND' ? 1 : -1) * (n - 1) / 22;
  const sea = ms => Math.log(ms) / Math.log(150), xfd = ms => Math.log(ms / 0.5) / Math.log(80);
  const spl = hz => hz ? 0.02 + 0.98 * Math.log(hz / 60) / Math.log(133.3333) : 0, blv = db => (db + 24) / 36;
  // the LFO waves (v65: 16, SLEW and LOOP after EXP in both halves)
  const W = { SIN: 0, TRI: 1, SAW: 2, SQR: 3, SH: 4, EXP: 5, SLEW: 6, LOOP: 7, SINT: 8, TRIT: 9, SAWT: 10, SQRT: 11, SHT: 12, EXPT: 13, SLEWT: 14, LOOPT: 15 };
  // LFO speeds: 128 64 32 16 12 8 6 4 3 2 3/2 1 3/4 1/2 1/3 1/4 steps per cycle
  const L = (spd, depth, dest, wave) => [spd, depth, dest, W[wave || 'SIN']];

  // track parameter indices (mirror core/sk_engine.h)
  const P = { MACH:0, TUNE:1, DEC:2, A:3, B:4, C:5, D:6, E:7, F:8, FTYP:9, FREQ:10, RES:11, FENV:12, FDEC:13,
    DRV:14, BITS:15, SRR:16, VOL:17, PAN:18, DLY:19, REV:20, LSPD:21, LDEP:22, LDST:23, LWAV:24,
    NOTE:25, VEL:26, PROB:27, MICR:28, RTRG:29, RCNT:30, RVEL:31, GATE:32,
    SPC:33, ATK:34, CHOK:35, DUCK:36, L2SP:37, L2DP:38, L2DS:39, L2WV:40,
    IR:41, IMIX:42, IPRE:43, IDCY:44, IGAT:45, ITON:46, IFB:47, IREV:48, PNCH:49,
    HPF:71, LPF:72, LOW:73, LOWF:74, MID:75, MIDF:76, HIGH:77, HIGF:78, E1SR:79, E2SR:87,
    HSWL:95, HGLD:96, HMOD:97, HINV:98, HDRF:99, HAIR:100, HWID:101,
    BRND:102, BREV:103, BSTU:104, BPJM:105, BSKP:106, BSED:107, BSWP:108,
    XA:109, XB:110, XC:111, XD:112, XE:113, XF:114, XG:115, RCRV:116, JUMP:117, JRNG:118,
    HARM:119, HUNI:120, HSUB:121, HCUT:122, HOPN:123, HVIB:124, HWRM:125,
    SHUM:126, SACC:127, SRNG:128, SMAT:129, SPCK:130, SDMP:131, SBDY:132, SMUT:133, SDBL:134, SSTF:135,
    CSWL:136, CFRM:137, CEFF:138, CVRT:139, CPRT:140, CSCP:141, CDRF:142,
    CSPC:143, CSIZ:144, CDCY:145, CDMP:146, CMOD:147, CSHM:148, CPRE:149,
    GDTN:150, GVAR:151, GSHN:152, GDRP:153, SAIR:154,
    S1:155, S2:156, S3:157, S4:158, S5:159, S6:160, S7:161, S8:162, S9:163, S10:164, S11:165, S12:166, S13:167, S14:168,
    S15:169, S16:170, S17:171, S18:172, S19:173, S20:174, S21:175,
    S22:176, S23:177, S24:178, S25:179, S26:180, S27:181, S28:182, S29:183, S30:184, S31:185, S32:186, S33:187, S34:188, S35:189,
    RPAN:190, SPRD:191, SMON:192, RDEC:193, RDSD:194, L1SL:195, L1LN:196, L1SD:197, L2SL:198, L2LN:199, L2SD:200, CHO:201, PHS:202,
    // v78: S36 .. S70 (after the rest), the amplitude envelope's curve
    ...Object.fromEntries(Array.from({ length: 35 }, (_, i) => ['S' + (36 + i), 203 + i])), ECRV:238 };
  const SQ = q => q < 35 ? P.S1 + q : P.S36 + (q - 35);   // (the param of a side place, app.js SQ)
  const KEYS = { T: P.TUNE, D: P.DEC, a: P.A, b: P.B, c: P.C, d: P.D, e: P.E, f: P.F, fq: P.FREQ, rs: P.RES, fe: P.FENV,
    fd: P.FDEC, dr: P.DRV, bt: P.BITS, sr: P.SRR, at: P.ATK, dl: P.DLY, rv: P.REV, ch: P.CHO, ph: P.PHS, im: P.IMIX, ip: P.IPRE, id: P.IDCY,
    ig: P.IGAT, it: P.ITON, ib: P.IFB, gt: P.GATE, rvl: P.RVEL, vo: P.VOL, pn: P.PNCH,
    // HALO, VOICE side: swell, glide, mode (stepped: use ix(i, 12)), inversion, drift, air, width
    sw: P.HSWL, gl: P.HGLD, md: P.HMOD, iv: P.HINV, df: P.HDRF, ai: P.HAIR, wd: P.HWID,
    // BREAK, CHOP side: SWAP, RAND, REV, STUT, PJMP, SKIP (chances), SEED (stepped: ix(i, 33), 0 = free)
    xp: P.BSWP, rn: P.BRND, rr: P.BREV, su: P.BSTU, pj: P.BPJM, sk: P.BSKP, sd: P.BSED,
    // the FX machines' MORE side (what each is: app.js fxMeta; xe is ACT)
    xa: P.XA, xb: P.XB, xc: P.XC, xd: P.XD, xe: P.XE, xf: P.XF, xg: P.XG,
    // v59: HARM (stepped: hm); HALO's TONE side; STRUM's PLAY side (with md, iv, harm, wd) and STRING side (matl: mat);
    // CHOIR's VOICE side (part: part) and SPACE side (shim: shim)
    harm: P.HARM, unis: P.HUNI, sub: P.HSUB, cut: P.HCUT, open: P.HOPN, vib: P.HVIB, warm: P.HWRM,
    humn: P.SHUM, accn: P.SACC, ring: P.SRNG, matl: P.SMAT, pick: P.SPCK, brit: P.SDMP, body: P.SBDY, mute: P.SMUT, dbl: P.SDBL, stif: P.SSTF,
    swel: P.CSWL, form: P.CFRM, efrt: P.CEFF, vrat: P.CVRT, part: P.CPRT, scop: P.CSCP, drft: P.CDRF,
    smix: P.CSPC, size: P.CSIZ, dcay: P.CDCY, damp: P.CDMP, mod: P.CMOD, shim: P.CSHM, pre: P.CPRE,
    // v60: AGE (HALO, STRUM): the cards' tuning, their variance, the sheen, the dropouts; STRUM's AIR
    dtun: P.GDTN, vari: P.GVAR, shen: P.GSHN, drop: P.GDRP, sair: P.SAIR,
    // v63: ALLOY's sides (app.js SIDE_META). IMPACT: the mallet, the transient, the pitch at the hit (pe), its time (pt)
    // and its rise (pr), the bounces or flams (bo), the life (ORGN); NOISE: into the object, heard as it is (HISS), their
    // colour, their length (nl), the feedback, the drive, the converter (dc); FM: the object's level, the FM's level,
    // ratio (ra), depth, its rise (ia) and its fall (ic), the ratio's sweep (me); COLOR: the sines' level and band (bd),
    // the comb's level, pitch (iv: semitones), feedback (cf), the flanger, the phaser; SPACE: the reverb's level, its
    // room, its decay (dy), its damping, its shift (shf), the gate (ct), the LFO
    mlt: P.S1, clk: P.S2, pen: P.S3, prs: P.S4, ptm: P.S5, bnc: P.S6, org: P.S7,
    noi: P.S8, his: P.S9, ncl: P.S10, nln: P.S11, fdb: P.S12, dri: P.S13, dac: P.S14,
    obj: P.S15, fm: P.S16, rat: P.S17, idx: P.S18, iat: P.S19, idc: P.S20, men: P.S21,
    add: P.S22, bnd: P.S23, cmb: P.S24, ctn: P.S25, cfb: P.S26, flg: P.S27, pha: P.S28,
    vrb: P.S29, rom: P.S30, dcy: P.S31, dmp: P.S32, sft: P.S33, cut: P.S34, rte: P.S35,
    // v64: the spread (TONE's MORE page) and its crossover (a preset owns them, as it owns its HP / LP; the trig's
    // RPAN is the track's, as the other trig settings)
    spr: P.SPRD, smo: P.SMON,
    // v66: CHIMERA's sides (app.js SIDE_META): GRAFT (the body's category and sample: bod(), the seam, the crossfade,
    // the split, the body's tuning, its level) and FAMILY (alike in, its category only, what VARI varies, at the pitch)
    gbc: P.S1, gbs: P.S2, gsm: P.S3, gxf: P.S4, gsp: P.S5, gbt: P.S6, gbl: P.S7, flk: P.S8, fsm: P.S9, fvy: P.S10, fpt: P.S11,
    // v71: FUZZ's SUB side (the octaver: one octave down, two, PURE .. GRIT, its path: ix(i, 2), 1 = into the fuzz);
    // HALO's SUB, where it sits (ix(i, 3): 0 AUTO, between 45 and 90 Hz, 1 the note itself, 2 an octave down) and
    // (v72) its harmonics (pres: 0 a pure sine)
    zs1: P.S1, zs2: P.S2, zgr: P.S3, zpt: P.S4, soct: P.S1, pres: P.S2,
    // v73: FUZZ's circuit (cir())
    zc: P.S5,
    // v74: CUBE's sides: OSC (B's interval: iv(), detune: cdt(), place against A's: co(), level; UNIS: cun(); SET stays
    // FACT, the factory cube) and MOD (the sweep on Y: co(), STIM: cst(), the pitch envelope: cpe(), PTIM: cpt(), STEP:
    // cstp(), WARP: cwp(), BITS: cbit())
    qbr: P.S1, qbd: P.S2, qbx: P.S3, qby: P.S4, qbl: P.S5, qun: P.S6, qsy: P.S8, qst: P.S9, qpe: P.S10, qpt: P.S11, qsp: P.S12, qwp: P.S13, qbt: P.S14,
    // v76: WEST's sides: OSC (wsh the wave, wfd FOLD (v77): wfold(), wgl GLID: wglid(), wdr DRFT, wte TENV: wb(), wve VELO, wwa WARM:
    // wwm()), MOD (wmw the wave: wwv(), wmo FM / AM: WAM, wky KEYD / FREE: WFREE, wfq the detune or the free rate:
    // wfr(), wme MENV: wb(), wtm TMOD, wmx MOSC), PULS (wcn CNT: wcnt(), wbn BNCE, wac ACNT: wb(), wsw SWNG, wpr PROB,
    // wji JITR, wrn RAND), SEQ (wv1 .. wv5, wln LEN, wds DEST: wseq()), GATE (wgm its mode: wgm(), wre RESO, wat ATK:
    // watk(), wsu SUST: wsus(), wen ENV: wenv(), wsp SPRG, wsd SDCY: wsdc())
    wsh: P.S1, wfd: P.S2, wgl: P.S3, wdr: P.S4, wte: P.S5, wve: P.S6, wwa: P.S7, wmw: P.S8, wmo: P.S9, wky: P.S10, wfq: P.S11, wme: P.S12, wtm: P.S13, wmx: P.S14,
    wcn: P.S15, wbn: P.S16, wac: P.S17, wsw: P.S18, wpr: P.S19, wji: P.S20, wrn: P.S21, wv1: P.S22, wv2: P.S23, wv3: P.S24, wv4: P.S25, wv5: P.S26, wln: P.S27, wds: P.S28,
    wgm: P.S29, wre: P.S30, wat: P.S31, wsu: P.S32, wen: P.S33, wsp: P.S34, wsd: P.S35,
    // v81: HOWL's sides: STRING (hex EXCT, hdp DAMP, hrs RESO, hhm HUM, hbl BLOM: hblm(), hdf DRFT, hwm WAMY), AMP (hdv
    // DRIV, hfz FUZZ, htn TONE, hbs BASS, hmd MID, hsg SAG, hlv LEVL) and (v82) LOOP (htm TIME: htime(), hcl COLR:
    // hcolr(), hfc FOCS, hpk PKUP, hns NOIS, hmv MOVE, hlm LIMT)
    hex: P.S1, hdp: P.S2, hrs: P.S3, hhm: P.S4, hbl: P.S5, hdf: P.S6, hwm: P.S7,
    hdv: P.S8, hfz: P.S9, htn: P.S10, hbs: P.S11, hmd: P.S12, hsg: P.S13, hlv: P.S14,
    htm: P.S15, hcl: P.S16, hfc: P.S17, hpk: P.S18, hns: P.S19, hmv: P.S20, hlm: P.S21,
    // v83: HOWL's CAB (hgt TGHT, hbt BITE, hps PRES, hdh DPTH, hcb CAB, hsz SIZE, hmc MIC) and BOW (hbw BOW, hsl SWEL:
    // hswl(), hst STRG: hstr(), hbm MODE: hbmd(), hbo POS, hwo WOBL, hhn HAND)
    hgt: P.S22, hbt: P.S23, hps: P.S24, hdh: P.S25, hcb: P.S26, hsz: P.S27, hmc: P.S28,
    hbw: P.S29, hsl: P.S30, hst: P.S31, hbm: P.S32, hbo: P.S33, hwo: P.S34, hhn: P.S35,
    // v83: STENCIL's SPACE side (the halo's SIZE, DCAY, DAMP, DRFT; µ's speed; DUCK; WIDE)
    tsz: P.S1, tdy: P.S2, tdm: P.S3, tdf: P.S4, tms: P.S5, tdk: P.S6, twd: P.S7,
    // v87: RADIO's AIR side (core/sk_radio.h): BAND (airb: rband()), CNTR (airc: rcntr()), FADE, RATE (airr: rrate()),
    // HET, CRKL, DRIV
    airb: P.S1, airc: P.S2, airf: P.S3, airr: P.S4, airh: P.S5, airk: P.S6, aird: P.S7 };
  // v84: MANTRA's sides (core/sk_mantra.h, its places in order): V1-4 (yt1 .. yt4 TUN: ynote(), ysa ysb SHP12 SHP34, yp1
  // PCH1: ypch()), V5-8 (yt5 .. yt8, ysc ysd, yp2), FM1-4 (yma yca yfa: MOD12 SRC12 FST12; ymb ycb yfb; ylp LOOP), FM5-8
  // (ymc ycc yfc; ymd ycd yfd; yto TOTL), LFO (yla ylb FRQA FRQB: ylfo(); ylg LGIC, ylk LINK, yvb VIB, yvd VDPT, ysg SAG),
  // DELAY (yd1 yd2 TIM1 TIM2: ydt(); ym1 ym2 DMD1 DMD2; yds DSRC, ydw DWAV, ydx DMIX), OUT (yxm DSTM, yvl VOL, ybs BUS,
  // ybl BLED, ytl TOL, ydf DRFT, yns NOIS)
  ['yt1', 'yt2', 'yt3', 'yt4', 'ysa', 'ysb', 'yp1', 'yt5', 'yt6', 'yt7', 'yt8', 'ysc', 'ysd', 'yp2', 'yma', 'yca', 'yfa', 'ymb', 'ycb', 'yfb', 'ylp',
    'ymc', 'ycc', 'yfc', 'ymd', 'ycd', 'yfd', 'yto', 'yla', 'ylb', 'ylg', 'ylk', 'yvb', 'yvd', 'ysg', 'yd1', 'yd2', 'ym1', 'ym2', 'yds', 'ydw', 'ydx',
    'yxm', 'yvl', 'ybs', 'ybl', 'ytl', 'ydf', 'yns'].forEach((key, i) => { KEYS[key] = SQ(i); });
  // TONE, in its own units: hp / lp in Hz (0 / null: off), the bands in dB, their frequencies in Hz
  const TONE = { hp: [P.HPF, hz => hz ? Math.log(hz / 20) / Math.log(400) : 0], lp: [P.LPF, hz => hz ? Math.log(hz / 100) / Math.log(200) : 1],
    lo: [P.LOW, db => 0.5 + db / 30], lof: [P.LOWF, hz => Math.log10(hz / 40)], md: [P.MID, db => 0.5 + db / 30],
    mdf: [P.MIDF, hz => Math.log(hz / 150) / Math.log(53.333)], hi: [P.HIGH, db => 0.5 + db / 30], hif: [P.HIGF, hz => Math.log10(hz / 1500)] };
  // an envelope follower: [source, gain dB, band, trigger dB (null: follower), attack ms, decay ms, depth -1..1, destination]
  const EF_SRC = ['SELF', 'T1', 'T2', 'T3', 'T4', 'T5', 'T6', 'T7', 'T8', 'IN', 'MAIN'], EF_BAND = ['FULL', 'LOW', 'MID', 'HIGH'];
  const efVals = ([src, gain, band, trg, atk, dec, depth, dest]) => {
    const si = EF_SRC.indexOf(src), bi = EF_BAND.indexOf(band), di = DEST.indexOf(dest);
    if (si < 0 || bi < 0 || di < 0) throw new Error('EF ' + src + ' ' + band + ' ' + dest);
    return [ix(si, 11), (gain + 12) / 48, ix(bi, 4), trg === null ? 0 : (trg + 48) / 48, Math.log(atk / 0.1) / Math.log(10000),
      Math.log(dec / 5) / Math.log(800), 0.5 + depth / 2, ix(di, DEST.length)];
  };
  const EF_REST = efVals(['SELF', 0, 'FULL', null, 5, 150, 0, 'OFF']);
  // what a preset owns: everything else on the track (pan, sends to nothing, trig settings) is left alone
  const BASE = { [P.FTYP]: ix(0, 4), [P.FREQ]: 1, [P.RES]: 0.1, [P.FENV]: 0.5, [P.FDEC]: 0.3, [P.DRV]: 0, [P.BITS]: 0, [P.SRR]: 0,
    [P.ATK]: 0, [P.DLY]: 0, [P.REV]: 0, [P.CHO]: 0, [P.PHS]: 0, [P.LSPD]: ix(7, 16), [P.LDEP]: 0.5, [P.LDST]: ix(0, DEST.length), [P.LWAV]: ix(0, 16),
    [P.L2SP]: ix(5, 16), [P.L2DP]: 0.5, [P.L2DS]: ix(0, DEST.length), [P.L2WV]: ix(0, 16), [P.GATE]: 0, [P.RTRG]: ix(0, 12),
    [P.RCNT]: ix(3, 16), [P.RVEL]: 0.5, [P.RCRV]: 0.5, [P.IR]: ix(0, 25), [P.IMIX]: 0.5, [P.IPRE]: 0, [P.IDCY]: 1, [P.IGAT]: 1,
    [P.ITON]: 0.5, [P.IFB]: 0, [P.IREV]: ix(0, 2), [P.PNCH]: 0,
    // HALO's VOICE side at its defaults (a preset of another machine leaves it as a fresh track has it)
    [P.HSWL]: 0.55, [P.HGLD]: 0, [P.HMOD]: ix(0, 12), [P.HINV]: 0, [P.HDRF]: 0.3, [P.HAIR]: 0.15, [P.HWID]: 0.5,
    // BREAK's CHOP side at its defaults: no chops, free draws, no swap
    [P.BRND]: 0, [P.BREV]: 0, [P.BSTU]: 0, [P.BPJM]: 0, [P.BSKP]: 0, [P.BSED]: ix(0, 33), [P.BSWP]: 0,
    // the FX machines' MORE side as a fresh track has it (an FX machine's own defaults come with machineDefaults)
    [P.XA]: 0, [P.XB]: 0, [P.XC]: 0, [P.XD]: 0, [P.XE]: 0, [P.XF]: 0, [P.XG]: 0,
    // v59: HARM on PURE; HALO's TONE at rest; STRUM's and CHOIR's sides as a fresh track has them
    [P.HARM]: ix(1, 3), [P.HUNI]: 0, [P.HSUB]: 0, [P.HCUT]: 1, [P.HOPN]: 0, [P.HVIB]: 0, [P.HWRM]: 0,
    [P.SHUM]: 0.25, [P.SACC]: 0.5, [P.SRNG]: 0.35, [P.SMAT]: ix(0, 6), [P.SPCK]: 0.55, [P.SDMP]: 0.55, [P.SBDY]: 0.6, [P.SMUT]: 0, [P.SDBL]: 0, [P.SSTF]: 0,
    [P.CSWL]: 0.45, [P.CFRM]: 0.5, [P.CEFF]: 0.45, [P.CVRT]: 0.4, [P.CPRT]: ix(0, 6), [P.CSCP]: 0.35, [P.CDRF]: 0.4,
    [P.CSPC]: 0.35, [P.CSIZ]: 0.6, [P.CDCY]: 0.6, [P.CDMP]: 0.35, [P.CMOD]: 0.4, [P.CSHM]: ix(0, 4), [P.CPRE]: 0.15,
    // v60: AGE at rest, STRUM's AIR a breath (as a fresh track has them); v61: the side places at 0 (ALLOY's own come
    // with its side defaults)
    [P.GDTN]: 0, [P.GVAR]: 0, [P.GSHN]: 0, [P.GDRP]: 0, [P.SAIR]: 0.3, ...Object.fromEntries(Array.from({ length: 70 }, (_, i) => [SQ(i), 0])),
    // v78: the amplitude envelope's curve at EXP (as it always was)
    [P.ECRV]: 1,
    // v64: the spread at the machine's (resolve), centred under 155 Hz
    [P.SPRD]: 0, [P.SMON]: 0.5,
    // v65: the LFOs' MORE page at rest (SLEW's glide half the cycle, LOOP's 8 draws) and their seeds
    [P.L1SL]: 0.5, [P.L1LN]: ix(6, 10), [P.L1SD]: 0, [P.L2SL]: 0.5, [P.L2LN]: ix(6, 10), [P.L2SD]: 0,
    // TONE flat, the followers at rest
    [P.HPF]: 0, [P.LPF]: 1, [P.LOW]: 0.5, [P.LOWF]: 0.5, [P.MID]: 0.5, [P.MIDF]: 0.5, [P.HIGH]: 0.5, [P.HIGF]: 0.5,
    ...Object.fromEntries(EF_REST.flatMap((v, i) => [[P.E1SR + i, v], [P.E2SR + i, v]])) };
  const OWNED = [P.TUNE, P.DEC, P.A, P.B, P.C, P.D, P.E, P.F, P.VOL, ...Object.keys(BASE).map(Number)];

  const TAGS = {
    AE: 'Autechre', AFX: 'Aphex Twin', BOC: 'Boards of Canada', SQPR: 'Squarepusher', LFO: 'LFO', PLAID: 'Plaid',
    BJÖRK: 'Björk', GSFL: 'Gesaffelstein', FLYLO: 'Flying Lotus', CLARK: 'Clark', OPN: 'Oneohtrix Point Never',
    BRDCST: 'Broadcast', SEEFEEL: 'Seefeel', HUDMO: 'Hudson Mohawke', RUSTIE: 'Rustie', SENNI: 'Lorenzo Senni',
    BLEEP: 'Sheffield bleep techno (Sweet Exorcist, Forgemasters, Nightmares on Wax)', BIBIO: 'Bibio', B12: 'B12 / Artificial Intelligence',
    PNSC: 'Pan Sonic', IKEDA: 'Ryoji Ikeda', NOTO: 'Alva Noto', MCRPT: 'Micropoint',
    ELECTRO: 'Early-eighties electro, synth-pop and new wave: the hexagonal pads',
    GOLDIE: 'Goldie', PHOTEK: 'Photek', BUKEM: 'LTJ Bukem', VSNARE: 'Venetian Snares', DJSHDW: 'DJ Shadow', PREFUSE: 'Prefuse 73',
    DUB: 'Jamaican dub mixing (King Tubby, Lee “Scratch” Perry, Scientist)', BASIC: 'Basic Channel / Rhythm & Sound dub techno',
    GAS: 'Gas (Wolfgang Voigt)', CAT: 'The Caretaker', DISINT: 'William Basinski, The Disintegration Loops', BURIAL: 'Burial',
    PÄRT: 'Arvo Pärt: the tintinnabuli (a triad and a melody walking over it)',
    SOPHIE: 'SOPHIE: synthesised metals and materials (chrome, latex, plastic), struck, snapped, bounced, cold and clean; her kicks, snares, claps and squeaks, gated and dry',
    FBTNS: 'Fuck Buttons (Street Horrrsing, Tarot Sport): toy keyboards and a karaoke microphone fuzzed into drones, screamed vocals, tribal toms',
    // v87: RADIO's
    KURZWELLEN: 'Karlheinz Stockhausen, Kurzwellen (1968): performers improvising on shortwave receivers, the static and the voices they find',
    CAGE: 'John Cage, Imaginary Landscape No. 4 (1951): twelve radios, their tuning and volume as the score',
    CZUKAY: 'Holger Czukay: shortwave radio as an instrument (Canaxis, Movies, Radio Wave Surfer)',
    NUMBERS: 'Numbers stations: shortwave voices reading digits, chimes and tones (as collected by The Conet Project)',
    SWL: 'Shortwave listening: fading, selective notches, heterodynes, the crackle of storms far away',
    // v81: HOWL's
    'SUNN O)))': 'Sunn O))): monumental drones, chords tuned very low, thick saturation, endless feedback',
    EARTH: 'Earth (Earth 2): minimal, deep guitars, very long resonances',
    BORIS: 'Boris (Absolutego, Flood, Feedbacker): living feedback, grainy saturation, instability held in check',
    NADJA: 'Nadja: layers of saturated guitars, enveloping textures',
    JESU: 'Jesu: dense chords, melodic and clear in their harmony',
    LANOIS: 'Daniel Lanois: ambient guitars, bowed and swelling, their echoes',
    MBV: 'My Bloody Valentine (Loveless): subtle detuning, wavering pitch, moving textures',
    // v83: STENCIL's
    ENO: 'Brian Eno: tape delays and loops left to themselves, ambient',
    FRIPP: 'Robert Fripp (Frippertronics): two tape machines, long repeats stacked into a loop',
    // v84: MANTRA's
    RADIGUE: 'Éliane Radigue: drones that move so slowly they seem still, their beatings and partials drifting',
    NIBLOCK: 'Phill Niblock: walls of close tones, microtonal beatings, very long, very loud',
    LAMONTE: 'La Monte Young: sustained tones in just intonation, the harmonic series held for hours',
    OLIVEROS: 'Pauline Oliveros (Deep Listening): tones swelling and sinking into a long resonance',
    CONRAD: 'Tony Conrad: amplified drones on strict ratios, their beating',
    DAVACHI: 'Sarah Davachi: slow organ-like tones, their harmonics unfolding',
  };

  const LIST = {
  // ------------------------------------------------------------------ BD·VA: analog kick
  'BD·VA': [
    ['AFX', 'ANALORD 808', { T: st(-1), D: .64, a: 0, b: .45, c: .35, d: .08, e: .1, f: .1 }],
    ['AFX', 'ACRID 909', { T: st(4), D: .42, a: .25, b: .6, c: .2, d: .55, e: .35, f: .02 }],
    ['AE', 'INCUNABULA 606', { T: st(2), D: .38, a: .1, b: .35, c: .3, d: .15, e: .15, fq: .64, rs: .2 }],
    ['AE', 'BIKE THUMP', { T: st(-3), D: .3, a: 0, b: .3, c: .4, d: .3, e: .2, ir: 'CARTON', im: .3 }],
    ['BOC', 'DUSTY BOOMBAP', { T: st(1), D: .45, a: .15, b: .4, c: .3, d: .2, e: .25, bt: .3, sr: .22, fq: .58, rs: .15 }],
    ['BOC', 'ROYGBIV TAPE', { T: st(-2), D: .5, a: .05, b: .4, c: .35, d: .1, e: .3, fq: .55, l1: L(3, .004, 'TUNE') }],
    ['LFO', 'BLEEP SUB', { T: st(-5), D: .7, a: 0, b: .25, c: .45, d: 0, e: .05, f: .3 }],
    ['SQPR', 'DRILL KICK', { T: st(5), D: .25, a: .3, b: .7, c: .15, d: .6, e: .4 }],
    ['GSFL', 'INDUSTRIAL', { T: st(2), D: .48, a: .4, b: .55, c: .25, d: .5, e: .85, dr: .35, fq: .72 }],
    ['CLARK', 'TURNING DRAGON', { T: st(0), D: .55, a: .3, b: .65, c: .3, d: .45, e: 1, dr: .5, bt: .15 }],
    ['FLYLO', 'WONKY BOOM', { T: st(-4), D: .6, a: 0, b: .7, c: .55, d: .05, e: .2, f: .15 }],
    ['OPN', 'REPLICA GHOST', { T: st(1), D: .35, a: .2, b: .3, c: .3, d: .1, e: .1, fq: .45, rv: .25 }],
    ['BJÖRK', 'HOMOGENIC', { T: st(-1), D: .5, a: .5, b: .5, c: .3, d: .4, e: .7, dr: .25, ir: 'GARAGE', im: .25 }],
    ['PLAID', 'ROUND TOM KICK', { T: st(3), D: .45, a: .1, b: .35, c: .3, d: .2, e: .1 }],
    ['AFX', 'CHEETAH CLICK', { T: st(6), D: .3, a: .6, b: .5, c: .2, d: .8, e: .3 }],
    ['AE', 'GANTZ BLIP', { T: st(10), D: .12, a: .5, b: .8, c: .1, d: .7, e: .5 }],
    ['HUDMO', 'TRAP 808', { T: st(-2), D: .78, a: 0, b: .3, c: .5, d: .1, e: .45, f: .4 }],
    ['SEEFEEL', 'DUB PULSE', { T: st(-1), D: .45, a: 0, b: .2, c: .4, d: 0, e: .1, fq: .42, dl: .2 }],
    ['BLEEP', 'TESTONE', { T: st(7), D: .3, a: 0, b: .9, c: .12, d: .2, e: 0 }],
    ['AE', 'LP5 SATURN', { T: st(-6), D: .55, a: .2, b: .5, c: .4, d: .3, e: .6, fq: .5, rs: .35, ir: 'TUNNEL', im: .2 }],
  ],
  // ------------------------------------------------------------------ BD·FM: FM kick
  'BD·FM': [
    ['AE', 'CONFIELD FM', { T: st(1), D: .4, a: r(8), b: .55, c: .25, d: .3, e: .45, f: .2 }],
    ['AFX', 'DRUKQS METAL', { T: st(2), D: .35, a: r(11), b: .7, c: .2, d: .45, e: .5, f: .35 }],
    ['SQPR', 'BASS DRUM', { T: st(-2), D: .5, a: r(3), b: .4, c: .3, d: .2, e: .35, f: 0 }],
    ['GSFL', 'ACID KICK', { T: st(0), D: .45, a: r(8), b: .6, c: .3, d: .6, e: .55, f: .6, dr: .3 }],
    ['CLARK', 'BODY RIDDLE', { T: st(0), D: .42, a: r(6), b: .65, c: .35, d: .35, e: .6, f: .4, dr: .25 }],
    ['LFO', 'FREQUENCIES', { T: st(-4), D: .6, a: r(3), b: .3, c: .5, d: .1, e: .2, f: 0 }],
    ['AE', 'TRI REPETAE', { T: st(0), D: .4, a: r(5), b: .5, c: .2, d: .25, e: .4, f: .1, bt: .2 }],
    ['PLAID', 'BELL KICK', { T: st(3), D: .4, a: r(14), b: .45, c: .3, d: .1, e: .3, f: 0 }],
    ['OPN', 'CHROME', { T: st(0), D: .38, a: r(12), b: .5, c: .4, d: .5, e: .3, f: .3, fq: .62, rs: .3 }],
    ['AFX', 'POLYNOMIAL', { T: st(1), D: .3, a: r(10), b: .6, c: .15, d: .2, e: .65, f: .15 }],
    ['BOC', 'TAPE THUD', { T: st(-1), D: .45, a: r(3), b: .3, c: .4, d: .1, e: .3, f: .1, fq: .5, bt: .25, sr: .15 }],
    ['HUDMO', 'CHUNK', { T: st(0), D: .42, a: r(8), b: .75, c: .2, d: .7, e: .7, f: .5, dr: .2 }],
    ['BJÖRK', 'PLUTO', { T: st(-1), D: .45, a: r(1), b: .8, c: .3, d: .6, e: .6, f: .7, dr: .4 }],
    ['AE', 'DRAFT SUB', { T: st(-3), D: .55, a: r(0), b: .5, c: .5, d: .2, e: .25, f: 0 }],
    ['RUSTIE', 'GLASS SWORDS', { T: st(2), D: .4, a: r(16), b: .55, c: .25, d: .3, e: .5, f: .2, rv: .1 }],
    ['FLYLO', 'COSMOGRAMMA', { T: st(-2), D: .45, a: r(7), b: .45, c: .35, d: .4, e: .45, f: .25, fq: .55 }],
    ['SQPR', 'ULTRAVISITOR', { T: st(3), D: .3, a: r(9), b: .65, c: .15, d: .5, e: .75, f: .3 }],
    ['AE', 'OVERSTEPS', { T: st(0), D: .5, a: r(13), b: .35, c: .6, d: .15, e: .2, f: 0, rv: .2, fq: .5 }],
    ['BLEEP', 'FORGEMASTERS', { T: st(-2), D: .45, a: r(3), b: .2, c: .6, d: 0, e: .8, f: 0 }],
    ['BRDCST', 'RADIOPHONIC', { T: st(1), D: .4, a: r(20), b: .4, c: .3, d: .3, e: .3, f: .1, ir: 'RADIO', im: .5 }],
  ],
  // ------------------------------------------------------------------ SD·VA: analog snare
  'SD·VA': [
    ['AFX', '909 CRACK', { T: st(2), D: .3, a: .5, b: .6, c: .45, d: .55, e: .6, f: .2 }],
    ['AE', '606 SNAP', { T: st(5), D: .25, a: .3, b: .7, c: .35, d: .65, e: .7, f: .3 }],
    ['BOC', 'BOOMBAP', { T: st(0), D: .35, a: .4, b: .5, c: .5, d: .4, e: .4, f: .2, fq: .62, bt: .3, sr: .2 }],
    ['SQPR', 'AMEN GHOST', { T: st(3), D: .2, a: .6, b: .55, c: .3, d: .6, e: .8, f: .3 }],
    ['GSFL', 'HARSH', { T: st(0), D: .32, a: .7, b: .75, c: .5, d: .7, e: .9, f: .5, dr: .4 }],
    ['LFO', 'CLAP SNARE', { T: st(0), D: .3, a: .2, b: .8, c: .4, d: .5, e: .3, f: .1, rv: .15 }],
    ['AFX', 'DRILL ROLL', { T: st(4), D: .18, a: .5, b: .6, c: .25, d: .6, e: .7, f: .2, rt: 5, rc: 8, rvl: .72 }],
    ['CLARK', 'CRUSH', { T: st(0), D: .32, a: .45, b: .6, c: .55, d: .55, e: .7, f: .3, dr: .6, bt: .2 }],
    ['BJÖRK', 'ARMY OF ME', { T: st(-1), D: .35, a: .35, b: .65, c: .55, d: .45, e: .8, f: .25, ir: 'GARAGE', im: .35, dr: .2 }],
    ['FLYLO', 'DILLA RIM', { T: st(7), D: .15, a: .8, b: .3, c: .2, d: .8, e: .9, f: .6 }],
    ['OPN', 'GATED', { T: st(0), D: .3, a: .5, b: .7, c: .6, d: .5, e: .5, f: .2, ir: 'ESCALIER', im: .5, ig: .45 }],
    ['PLAID', 'TUNED', { T: st(-2), D: .35, a: .25, b: .3, c: .35, d: .45, e: .4, f: .35 }],
    ['BOC', 'CASSETTE', { T: st(-1), D: .35, a: .4, b: .55, c: .45, d: .35, e: .3, f: .1, fq: .55, sr: .25, bt: .35 }],
    ['AE', 'ROUND NOISE', { T: st(0), D: .3, a: .6, b: .9, c: .7, d: .3, e: .2, f: .7 }],
    ['HUDMO', 'STADIUM', { T: st(1), D: .38, a: .5, b: .6, c: .65, d: .6, e: .7, f: .2, rv: .35 }],
    ['SEEFEEL', 'DUB SNARE', { T: st(0), D: .32, a: .4, b: .6, c: .5, d: .4, e: .4, f: .2, dl: .35, rv: .2 }],
    ['AFX', 'WINDOWLICKER', { T: st(3), D: .22, a: .55, b: .65, c: .4, d: .7, e: .85, f: .4 }],
    ['BRDCST', 'TAPE ROOM', { T: st(-1), D: .32, a: .35, b: .5, c: .5, d: .4, e: .3, f: .15, ir: 'SALON', im: .4, fq: .62 }],
    ['SQPR', 'JUNGLE', { T: st(6), D: .22, a: .65, b: .55, c: .3, d: .65, e: .75, f: .25 }],
    ['AE', 'STUD', { T: st(9), D: .12, a: .9, b: .85, c: .25, d: .85, e: 1, f: .8, bt: .1 }],
  ],
  // ------------------------------------------------------------------ SD·FM: FM snare
  'SD·FM': [
    ['AE', 'CHIASTIC', { T: st(0), D: .3, a: r(10), b: .55, c: .25, d: .5, e: .4, f: .6 }],
    ['AFX', 'VORDHOSBN', { T: st(3), D: .2, a: r(9), b: .7, c: .2, d: .6, e: .25, f: .65, rt: 4, rc: 4, rvl: .6 }],
    ['SQPR', 'SELECTOR', { T: st(2), D: .25, a: r(11), b: .6, c: .15, d: .6, e: .3, f: .7 }],
    ['PLAID', 'CLINK', { T: st(5), D: .25, a: r(16), b: .45, c: .3, d: .3, e: .3, f: .65 }],
    ['GSFL', 'STEEL', { T: st(0), D: .3, a: r(12), b: .75, c: .3, d: .7, e: .45, f: .7, dr: .35 }],
    ['CLARK', 'RIDDLE', { T: st(0), D: .3, a: r(7), b: .6, c: .25, d: .55, e: .45, f: .5, bt: .15 }],
    ['OPN', 'DIGITAL', { T: st(4), D: .25, a: r(20), b: .5, c: .2, d: .35, e: .35, f: .8 }],
    ['BOC', 'ZEPHYR', { T: st(-2), D: .32, a: r(3), b: .35, c: .4, d: .5, e: .5, f: .4, fq: .62, bt: .3 }],
    ['AE', 'VLETRMX', { T: st(1), D: .28, a: r(14), b: .65, c: .3, d: .45, e: .4, f: .55, rv: .15 }],
    ['AFX', 'DIGERIDOO', { T: st(-3), D: .32, a: r(8), b: .7, c: .35, d: .4, e: .3, f: .5, dr: .2 }],
    ['HUDMO', 'BUTTER', { T: st(0), D: .32, a: r(6), b: .55, c: .35, d: .7, e: .55, f: .5, rv: .3 }],
    ['LFO', 'TONE SNARE', { T: st(-1), D: .3, a: r(3), b: .25, c: .5, d: .4, e: .45, f: .45 }],
    ['BJÖRK', 'VESPERTINE CLICK', { T: st(12), D: .12, a: r(18), b: .4, c: .15, d: .2, e: .15, f: .9 }],
    ['FLYLO', 'MASHED', { T: st(0), D: .3, a: r(5), b: .6, c: .3, d: .5, e: .4, f: .4, bt: .3, sr: .25 }],
    ['RUSTIE', 'LASER', { T: st(7), D: .25, a: r(19), b: .8, c: .4, d: .3, e: .3, f: .8, rv: .2 }],
    ['AE', 'DRAFT 7.30', { T: st(0), D: .25, a: r(11), b: .65, c: .25, d: .55, e: .3, f: .6, ir: 'CANETTE', im: .4 }],
    ['SQPR', 'FAT SNARE', { T: st(-2), D: .35, a: r(4), b: .5, c: .35, d: .65, e: .5, f: .45, dr: .2 }],
    ['SENNI', 'POINTILLIST', { T: st(12), D: .1, a: r(15), b: .45, c: .1, d: .1, e: .1, f: .9 }],
    ['BRDCST', 'HAUNTED', { T: st(-5), D: .35, a: r(2), b: .4, c: .5, d: .4, e: .6, f: .35, rv: .35, fq: .55 }],
    ['AE', 'NIL', { T: st(2), D: .22, a: r(22), b: .55, c: .2, d: .45, e: .35, f: .75 }],
  ],
  // ------------------------------------------------------------------ HH·MT: metallic hats
  'HH·MT': [
    ['AFX', '909 CLOSED', { D: .18, a: .5, b: .6, c: .5, d: .3, e: .3, f: .4 }],
    ['AFX', 'ANALORD OPEN', { D: .55, a: .5, b: .55, c: .4, d: .3, e: .3, f: .3 }],
    ['AE', '606 TICK', { D: .12, a: .4, b: .75, c: .3, d: .2, e: .2, f: .6 }],
    ['BOC', 'TAPE HAT', { D: .22, a: .45, b: .5, c: .6, d: .1, e: .3, f: .35, fq: .84, bt: .22, sr: .12 }],
    ['SQPR', 'RIDE', { D: .7, a: .6, b: .5, c: .3, d: .5, e: .4, f: .3 }],
    ['GSFL', 'CRASH METAL', { D: .75, a: .8, b: .4, c: .4, d: .7, e: .5, f: .5, dr: .3 }],
    ['LFO', 'SHAKER', { D: .2, a: .3, b: .8, c: .8, d: .1, e: .2, f: .1 }],
    ['AE', 'GLASS SCRAPE', { D: .4, a: .9, b: .3, c: .2, d: .9, e: .6, f: .2 }],
    ['CLARK', 'HISS', { D: .3, a: .5, b: .9, c: .9, d: .1, e: .1, f: .3 }],
    ['PLAID', 'COWBELL', { T: st(7), D: .34, a: .1, b: .25, c: 0, d: .25, e: .5, f: .6 }],
    ['FLYLO', 'LOOSE HAT', { D: .25, a: .5, b: .6, c: .5, d: .2, e: .3, f: .2, bt: .2 }],
    ['OPN', 'CYBER BELL', { T: st(5), D: .5, a: .2, b: .3, c: .05, d: .6, e: .7, f: .3, rv: .2 }],
    ['AFX', 'DRILL HATS', { D: .14, a: .5, b: .7, c: .5, d: .3, e: .3, f: .5, rt: 5, rc: 6, rvl: .3 }],
    ['BJÖRK', 'ICE', { D: .35, a: .7, b: .85, c: .3, d: .6, e: .5, f: .7, rv: .3 }],
    ['HUDMO', 'TRAP HAT', { D: .1, a: .5, b: .7, c: .55, d: .2, e: .2, f: .6 }],
    ['SEEFEEL', 'SHIMMER', { D: .6, a: .6, b: .5, c: .4, d: .5, e: .5, f: .1, dl: .3 }],
    ['AE', 'NORD CLICK', { D: .09, a: .3, b: .9, c: .3, d: .4, e: .8, f: .9 }],
    ['BLEEP', 'TIN', { T: st(3), D: .28, a: .2, b: .4, c: .2, d: .4, e: .4, f: .5, ir: 'CANETTE', im: .3 }],
    ['BRDCST', 'TAMBOURINE', { D: .38, a: .75, b: .62, c: .55, d: .45, e: .4, f: .2, rt: 4, rc: 3, rvl: .35 }],
    ['GSFL', 'TICK TICK', { D: .08, a: .5, b: .65, c: .3, d: .5, e: .4, f: .8, dr: .3 }],
  ],
  // ------------------------------------------------------------------ CP·NZ: clap
  'CP·NZ': [
    ['AFX', '909 CLAP', { D: .4, a: ix(3, 8), b: .45, c: .35, d: .4, e: .2, f: .1 }],
    ['AE', 'BURST', { D: .25, a: ix(6, 8), b: .3, c: .5, d: .2, e: .7, f: .3 }],
    ['BOC', 'ROOM CLAP', { D: .4, a: ix(2, 8), b: .5, c: .3, d: .5, e: .3, f: .2, rv: .25, fq: .62, bt: .25, ncl: .7 }],
    ['SQPR', 'SNAPPY', { T: st(4), D: .3, a: ix(1, 8), b: .35, c: .45, d: .3, e: .2, f: .2 }],
    ['GSFL', 'WAREHOUSE', { D: .4, a: ix(4, 8), b: .5, c: .4, d: .6, e: .3, f: .4, ir: 'GARAGE', im: .4, dr: .2 }],
    ['LFO', '808 CLAP', { D: .45, a: ix(3, 8), b: .5, c: .25, d: .55, e: .15, f: .05 }],
    ['FLYLO', 'FINGER SNAP', { T: st(9), D: .15, a: ix(0, 8), b: .2, c: .7, d: .1, e: .1, f: .1 }],
    ['AE', 'SCATTER', { D: .3, a: ix(7, 8), b: .7, c: .4, d: .3, e: 1, f: .5 }],
    ['CLARK', 'DIRTY', { D: .38, a: ix(4, 8), b: .45, c: .5, d: .5, e: .4, f: .8, dr: .3 }],
    ['HUDMO', 'WIDE CLAP', { D: .4, a: ix(5, 8), b: .4, c: .35, d: .6, e: .3, f: .1, rv: .3, dl: .15 }],
    ['BJÖRK', 'HIDDEN PLACE', { T: st(12), D: .12, a: ix(2, 8), b: .25, c: .6, d: .15, e: .5, f: .4 }],
    ['AFX', 'GHOST CLAP', { D: .45, a: ix(2, 8), b: .6, c: .3, d: .7, e: .25, f: .1, fq: .55 }],
    ['OPN', 'GATED CLAP', { D: .4, a: ix(3, 8), b: .4, c: .4, d: .7, e: .2, f: .2, ir: 'ESCALIER', im: .5, ig: .4 }],
    ['PLAID', 'CLICKY', { T: st(7), D: .25, a: ix(1, 8), b: .3, c: .8, d: .2, e: .2, f: .1 }],
    ['SQPR', 'SWING', { T: st(-3), D: .3, a: ix(2, 8), b: .62, c: .6, d: .25, e: .55, f: .3 }],
    ['AE', 'CRUMBLE', { D: .2, a: ix(7, 8), b: .92, c: .15, d: .15, e: 1, f: .9, bt: .45, sr: .2 }],
    ['GSFL', 'HANDS', { D: .35, a: ix(4, 8), b: .5, c: .3, d: .4, e: .3, f: .3, dr: .4 }],
    ['BOC', 'VINYL', { D: .4, a: ix(3, 8), b: .55, c: .25, d: .4, e: .3, f: .3, sr: .3, bt: .3 }],
    ['SEEFEEL', 'DUB CLAP', { D: .38, a: ix(2, 8), b: .45, c: .3, d: .45, e: .2, f: .1, dl: .35 }],
    ['RUSTIE', 'RAVE STAB', { T: st(5), D: .32, a: ix(5, 8), b: .35, c: .55, d: .3, e: .2, f: .3, rv: .2 }],
  ],
  // ------------------------------------------------------------------ FM·X: 3-operator FM
  'FM·X': [
    ['AE', 'NORD BELL', { T: st(12), D: .5, a: ix(0, 4), b: r(14), c: r(21), d: .45, e: .5, f: .1 }],
    ['AFX', 'DX BASS', { T: st(-12), D: .35, a: ix(1, 4), b: r(3), c: r(8), d: .5, e: .35, f: .2 }],
    ['PLAID', 'GAMELAN', { T: st(12), D: .6, a: ix(2, 4), b: r(12), c: r(20), d: .4, e: .55, f: .05, rv: .25 }],
    ['AE', 'GANTZ SHARD', { T: st(5), D: .2, a: ix(3, 4), b: r(21), c: r(23), d: .8, e: .2, f: .8 }],
    ['SQPR', 'FRETLESS', { T: st(-12), D: .5, a: ix(1, 4), b: r(3), c: r(11), d: .35, e: .5, f: .15, l1: L(10, .002, 'TUNE') }],
    ['BOC', 'DX PIANO', { T: st(12), D: .55, a: ix(0, 4), b: r(3), c: r(17), d: .35, e: .4, f: 0, fq: .66, l1: L(4, .003, 'TUNE') }],
    ['OPN', 'ECCO BELL', { T: st(19), D: .65, a: ix(2, 4), b: r(11), c: r(19), d: .5, e: .6, f: .2, rv: .3 }],
    ['GSFL', 'DARK STAB', { T: st(0), D: .3, a: ix(1, 4), b: r(8), c: r(1), d: .7, e: .3, f: .5, dr: .3 }],
    ['CLARK', 'METAL HIT', { T: st(0), D: .3, a: ix(3, 4), b: r(13), c: r(22), d: .7, e: .25, f: .6 }],
    ['AFX', 'SAW2 GLASS', { T: st(24), D: .7, a: ix(0, 4), b: r(16), c: r(8), d: .3, e: .7, f: 0, rv: .35, at: .3 }],
    ['LFO', 'BLEEP', { T: st(24), D: .25, a: ix(0, 4), b: r(3), c: r(3), d: .1, e: .3, f: 0 }],
    ['AE', 'ROTARY', { T: st(0), D: .4, a: ix(2, 4), b: r(7), c: r(10), d: .55, e: .45, f: .35, l1: L(11, .2, 'D', 'SH') }],
    ['BJÖRK', 'MUSIC BOX', { T: st(24), D: .45, a: ix(0, 4), b: r(14), c: r(21), d: .3, e: .35, f: 0, ir: 'B.MUSIQ', im: .22 }],
    ['FLYLO', 'PLUCK', { T: st(0), D: .3, a: ix(1, 4), b: r(8), c: r(14), d: .5, e: .25, f: .1 }],
    ['SENNI', 'TRANCE BLIP', { T: st(24), D: .15, a: ix(0, 4), b: r(8), c: r(3), d: .35, e: .15, f: .3, dl: .25 }],
    ['HUDMO', 'BRASS', { T: st(0), D: .5, a: ix(1, 4), b: r(3), c: r(8), d: .6, e: .6, f: .3, at: .1 }],
    ['AE', 'DRAFT KNOCK', { T: st(-5), D: .2, a: ix(3, 4), b: r(5), c: r(11), d: .6, e: .2, f: .7 }],
    ['SQPR', 'BASS SLAP', { T: st(-12), D: .3, a: ix(1, 4), b: r(8), c: r(3), d: .65, e: .15, f: .25 }],
    ['BRDCST', 'THEREMIN', { T: st(12), D: .8, a: ix(0, 4), b: r(3), c: r(3), d: .15, e: .8, f: .05, at: .45, l1: L(10, .003, 'TUNE') }],
    ['AE', 'CIRCLONT', { T: st(0), D: .35, a: ix(3, 4), b: r(22), c: r(9), d: .9, e: .4, f: 1 }],
  ],
  // ------------------------------------------------------------------ NOISE
  'NOISE': [
    ['AE', 'STATIC BURST', { T: .55, D: .3, a: .67, b: .7, c: .3, d: .3, e: .3, f: .3 }],
    ['AFX', 'SWEEP RISER', { T: .35, D: .7, a: .2, b: 1, c: .9, d: .8, e: .4, f: 0 }],
    ['BOC', 'TAPE HISS', { T: .6, D: .7, a: .33, b: 1, c: .5, d: .5, e: .1, f: 0, fq: .62, at: .3 }],
    ['LFO', 'LASER ZAP', { T: .72, D: .3, a: .1, b: 1, c: .1, d: .3, e: .7, f: 0, dr: .3 }],
    ['GSFL', 'INDUSTRIAL HISS', { T: .55, D: .35, a: 0, b: .9, c: .6, d: .4, e: .4, f: .2, dr: .4 }],
    ['AE', 'GRAIN SPRAY', { T: .5, D: .4, a: .5, b: .6, c: .5, d: .3, e: .3, f: .8 }],
    ['SQPR', 'SNARE BODY', { T: .65, D: .25, a: 0, b: 1, c: .4, d: .2, e: .2, f: 0 }],
    ['CLARK', 'BITCRUSH', { T: .5, D: .3, a: .67, b: .4, c: .5, d: .3, e: .2, f: .2, bt: .5 }],
    ['BJÖRK', 'BREATH', { T: .55, D: .45, a: .33, b: 1, c: .55, d: .6, e: .5, f: .1, at: .2 }],
    ['AFX', 'DUST CRACKLE', { T: .5, D: .6, a: 1, b: 1, c: .5, d: .5, e: .1, f: .1 }],
    ['OPN', 'WIND', { T: .45, D: .8, a: .33, b: 1, c: .6, d: .8, e: .7, f: 0, at: .4, l1: L(3, .15, 'TUNE', 'TRI') }],
    ['AE', 'LFSR TONE', { T: .55, D: .3, a: .67, b: .55, c: .5, d: .5, e: .6, f: 0 }],
    ['FLYLO', 'SHAKER', { T: .8, D: .2, a: 0, b: 1, c: .5, d: .2, e: .2, f: .5 }],
    ['HUDMO', 'DOWNLIFTER', { T: .6, D: .65, a: .2, b: 1, c: .1, d: .7, e: .5, f: 0 }],
    ['SEEFEEL', 'SURF', { T: .5, D: .75, a: .33, b: 1, c: .55, d: .7, e: .3, f: .2, rv: .3, dl: .2 }],
    ['PLAID', 'RAIN', { T: .7, D: .55, a: 1, b: .9, c: .5, d: .5, e: .6, f: .4 }],
    ['AE', 'SAMPLE HOLD', { T: .5, D: .3, a: 0, b: .35, c: .5, d: .5, e: .5, f: 0 }],
    ['BRDCST', 'RADIO TUNING', { T: .55, D: .5, a: .67, b: .6, c: .8, d: .6, e: .8, f: .3 }],
    ['RUSTIE', 'NOISE ROLL', { T: .65, D: .2, a: 0, b: 1, c: .45, d: .2, e: .3, f: 0, rt: 4, rc: 8, rvl: .8 }],
    ['GSFL', 'CYMBAL WASH', { T: .85, D: .52, a: 0, b: 1, c: .6, d: .9, e: .55, f: .1, ft: 'HP', fq: .78 }],
  ],
  // ------------------------------------------------------------------ IMPLS: impulse train
  'IMPLS': [
    ['AE', 'BOUNCING BALL', { D: .5, a: ix(11, 32), b: .55, c: .3, d: .4, e: .3, f: .1 }],
    ['AFX', 'RATCHET', { D: .4, a: ix(7, 32), b: .35, c: .5, d: .5, e: .4, f: .05 }],
    ['AE', 'GANTZ GRAF', { D: .35, a: ix(19, 32), b: .3, c: .2, d: .6, e: .7, f: .5 }],
    ['SQPR', 'GLITCH ROLL', { D: .3, a: ix(15, 32), b: .25, c: .55, d: .4, e: .5, f: .2 }],
    ['LFO', 'CLICK TRACK', { T: st(12), D: .3, a: ix(0, 32), b: .5, c: .5, d: .7, e: .55, f: 0 }],
    ['CLARK', 'SPEED UP', { D: .5, a: ix(23, 32), b: .5, c: .15, d: .5, e: .6, f: .1 }],
    ['BJÖRK', 'MICROBEATS', { T: st(12), D: .25, a: ix(5, 32), b: .2, c: .5, d: .3, e: .2, f: .6 }],
    ['AE', 'COMB RING', { T: st(-12), D: .55, a: ix(3, 32), b: .45, c: .5, d: .55, e: .72, f: 0 }],
    ['PLAID', 'PING PONG', { D: .45, a: ix(9, 32), b: .5, c: .3, d: .5, e: .4, f: .1, dl: .3 }],
    ['OPN', 'DIGITAL RAIN', { D: .4, a: ix(29, 32), b: .2, c: .5, d: .4, e: .5, f: .8, rv: .2 }],
    ['AFX', 'DRILL SNARE', { T: st(7), D: .25, a: ix(11, 32), b: .2, c: .5, d: .3, e: .3, f: 0 }],
    ['GSFL', 'MACHINE GUN', { D: .35, a: ix(13, 32), b: .3, c: .5, d: .6, e: .5, f: 0, dr: .4 }],
    ['BOC', 'RICKETY', { D: .35, a: ix(4, 32), b: .45, c: .6, d: .45, e: .4, f: .4, sr: .12, fq: .7 }],
    ['AE', 'DECELERATE', { D: .5, a: ix(17, 32), b: .3, c: .85, d: .5, e: .6, f: .2 }],
    ['FLYLO', 'STUTTER', { D: .3, a: ix(2, 32), b: .4, c: .5, d: .4, e: .3, f: .3 }],
    ['SQPR', 'ACID CLICK', { T: st(12), D: .3, a: ix(7, 32), b: .3, c: .5, d: .65, e: .55, f: 0 }],
    ['SENNI', 'ARP PULSE', { T: st(24), D: .4, a: ix(6, 32), b: .4, c: .5, d: .7, e: .5, f: 0 }],
    ['AE', 'METAL TRAIN', { D: .5, a: ix(25, 32), b: .35, c: .3, d: .45, e: .78, f: .3 }],
    ['HUDMO', 'ROLL UP', { D: .4, a: ix(9, 32), b: .45, c: .2, d: .5, e: .4, f: 0 }],
    ['BRDCST', 'GEIGER', { D: .6, a: ix(27, 32), b: .6, c: .5, d: .45, e: .3, f: 1 }],
  ],
  // ------------------------------------------------------------------ SY·RAW: raw VA (acid, pads, leads)
  'SY·RAW': [
    ['AFX', 'ACID 303', { T: st(12), D: .35, a: 0, b: .1, c: 0, d: 0, e: 0, f: .3, fq: .3, rs: .78, fe: .82, fd: .3, dr: .25 }],
    ['SQPR', 'SQUARE ACID', { T: st(12), D: .35, a: 1, b: .1, c: 0, d: 0, e: 0, f: .2, fq: .28, rs: .8, fe: .85, fd: .28, dr: .3 }],
    ['BOC', 'DETUNED PAD', { T: st(12), D: .9, a: 0, b: .55, c: 0, d: .6, e: .2, f: .3, at: .45, fq: .55, rv: .3, l1: L(3, .004, 'TUNE', 'TRI') }],
    ['AE', 'SYNC LEAD', { T: st(12), D: .5, a: .3, b: .2, c: .5, d: 0, e: 0, f: .3, fq: .7, rs: .3, fe: .65, fd: .4 }],
    ['GSFL', 'DARK BASS', { T: st(0), D: .5, a: 0, b: .3, c: 0, d: .3, e: .6, f: .3, fq: .38, rs: .45, fe: .7, fd: .35, dr: .35 }],
    ['LFO', 'REESE SUB', { T: st(0), D: .7, a: 0, b: .7, c: 0, d: 0, e: .8, f: .3, fq: .4, rs: .2 }],
    ['OPN', 'JUNO ARP', { T: st(24), D: .3, a: .9, b: .3, c: 0, d: 0, e: .3, f: .45, fq: .6, rs: .35, fe: .65, fd: .3, dl: .3 }],
    ['HUDMO', 'TRAP LEAD', { T: st(24), D: .6, a: 0, b: .6, c: 0, d: .7, e: 0, f: .3, fq: .8, rv: .2 }],
    ['FLYLO', 'WONKY SYNTH', { T: st(12), D: .5, a: .5, b: .4, c: .2, d: .2, e: .2, f: .5, fq: .55, rs: .3, l1: L(5, .006, 'TUNE', 'SH') }],
    ['AFX', 'SH-101 BASS', { T: st(0), D: .35, a: .8, b: 0, c: 0, d: 0, e: .7, f: .35, fq: .35, rs: .55, fe: .75, fd: .25 }],
    ['CLARK', 'HOOVER', { T: st(12), D: .5, a: 0, b: .8, c: .3, d: 1, e: .3, f: .3, fq: .7, rs: .2, dr: .2 }],
    ['SENNI', 'TRANCE SAW', { T: st(24), D: .25, a: 0, b: .5, c: 0, d: .85, e: 0, f: .3, fq: .65, rs: .2, fe: .65, fd: .25, dl: .25 }],
    ['BOC', 'WARM BASS', { T: st(0), D: .5, a: .3, b: .2, c: 0, d: 0, e: .6, f: .3, fq: .3, rs: .2, fe: .6, fd: .4, bt: .25 }],
    ['AE', 'SYNC SWEEP', { T: st(0), D: .45, a: 0, b: 0, c: .5, d: 0, e: 0, f: .3, fq: .75, l1: L(3, .45, 'C', 'TRI') }],
    ['RUSTIE', 'SUPERSAW', { T: st(12), D: .55, a: 0, b: .5, c: 0, d: .9, e: .3, f: .3, fq: .85, rv: .3 }],
    ['GSFL', 'PULSE STAB', { T: st(12), D: .2, a: 1, b: .3, c: .2, d: .2, e: 0, f: .55, fq: .55, rs: .4, fe: .7, fd: .2, dr: .4 }],
    ['SQPR', 'ELECTRIC BASS', { T: st(0), D: .3, a: .6, b: .1, c: 0, d: 0, e: .5, f: .2, fq: .4, rs: .3, fe: .7, fd: .2 }],
    ['BRDCST', 'TONE OSC', { T: st(12), D: .7, a: .9, b: 0, c: 0, d: 0, e: 0, f: 0, fq: .5, rs: .15, l1: L(10, .002, 'TUNE') }],
    ['PLAID', 'SOFT PLUCK', { T: st(24), D: .3, a: .4, b: .2, c: 0, d: 0, e: .2, f: .3, fq: .45, rs: .25, fe: .75, fd: .3, dl: .3 }],
    ['AE', 'RAW BUZZ', { T: st(0), D: .4, a: 1, b: .2, c: .6, d: .3, e: .3, f: .8, fq: .7, rs: .5, dr: .3 }],
  ],
  // ------------------------------------------------------------------ BYTE: bytebeat
  'BYTE': [
    ['AE', 'CODE', { T: .5, D: .4, a: ix(0, 8), b: ix(5, 16), c: ix(8, 16), d: ix(7, 8), e: ix(0, 32), f: .2 }],
    ['AFX', 'CHIPTUNE', { T: .6, D: .35, a: ix(2, 8), b: ix(4, 16), c: ix(7, 16), d: ix(3, 8), e: ix(2, 32), f: .1 }],
    ['AE', 'SERIAL', { T: .45, D: .45, a: ix(1, 8), b: ix(6, 16), c: ix(10, 16), d: ix(6, 8), e: ix(4, 32), f: .3 }],
    ['SQPR', '8-BIT BASS', { T: .25, D: .4, a: ix(4, 8), b: ix(3, 16), c: ix(9, 16), d: ix(7, 8), e: ix(0, 32), f: .6, fq: .5, rs: .3 }],
    ['OPN', 'CORRUPTED', { T: .55, D: .5, a: ix(7, 8), b: ix(9, 16), c: ix(13, 16), d: ix(5, 8), e: ix(6, 32), f: .2, rv: .2 }],
    ['BRDCST', 'TELETEXT', { T: .65, D: .3, a: ix(3, 8), b: ix(2, 16), c: ix(5, 16), d: ix(2, 8), e: ix(1, 32), f: .4 }],
    ['CLARK', 'GLITCH SYNTH', { T: .5, D: .4, a: ix(5, 8), b: ix(7, 16), c: ix(11, 16), d: ix(7, 8), e: ix(9, 32), f: .1, dr: .3 }],
    ['GSFL', 'DIGITAL GRIND', { T: .4, D: .45, a: ix(6, 8), b: ix(8, 16), c: ix(12, 16), d: ix(7, 8), e: ix(14, 32), f: .25, dr: .5, fq: .7 }],
    ['LFO', 'ARCADE', { T: .7, D: .25, a: ix(0, 8), b: ix(3, 16), c: ix(6, 16), d: ix(4, 8), e: ix(3, 32), f: .15 }],
    ['HUDMO', 'GAMEBOY', { T: .6, D: .3, a: ix(2, 8), b: ix(5, 16), c: ix(4, 16), d: ix(3, 8), e: ix(5, 32), f: .2 }],
    ['BJÖRK', 'CRYSTALLINE', { T: .75, D: .5, a: ix(1, 8), b: ix(10, 16), c: ix(3, 16), d: ix(5, 8), e: ix(7, 32), f: .5, rv: .3 }],
    ['AE', 'PIR', { T: .5, D: .35, a: ix(4, 8), b: ix(11, 16), c: ix(2, 16), d: ix(6, 8), e: ix(12, 32), f: .35 }],
    ['AFX', 'FINGERBIB BYTE', { T: .55, D: .4, a: ix(3, 8), b: ix(6, 16), c: ix(9, 16), d: ix(4, 8), e: ix(2, 32), f: .55, fq: .6 }],
    ['BOC', 'DATA TAPE', { T: .45, D: .45, a: ix(0, 8), b: ix(4, 16), c: ix(6, 16), d: ix(5, 8), e: ix(1, 32), f: .7, bt: .2, sr: .2, fq: .55 }],
    ['SENNI', 'RAVE CODE', { T: .65, D: .2, a: ix(2, 8), b: ix(7, 16), c: ix(8, 16), d: ix(6, 8), e: ix(3, 32), f: .1, dl: .25 }],
    ['OPN', 'AGE OF', { T: .5, D: .55, a: ix(5, 8), b: ix(12, 16), c: ix(14, 16), d: ix(7, 8), e: ix(20, 32), f: .4, rv: .25 }],
    ['AE', 'CRUSHED FORM', { T: .35, D: .4, a: ix(6, 8), b: ix(2, 16), c: ix(15, 16), d: ix(2, 8), e: ix(18, 32), f: .05 }],
    ['FLYLO', 'BEAT CODE', { T: .55, D: .3, a: ix(4, 8), b: ix(5, 16), c: ix(10, 16), d: ix(6, 8), e: ix(8, 32), f: .3, bt: .2 }],
    ['RUSTIE', 'NEON', { T: .7, D: .35, a: ix(7, 8), b: ix(4, 16), c: ix(7, 16), d: ix(5, 8), e: ix(11, 32), f: .2, rv: .2 }],
    ['CLARK', 'STATIC CODE', { T: .4, D: .3, a: ix(1, 8), b: ix(13, 16), c: ix(5, 16), d: ix(7, 8), e: ix(25, 32), f: .15 }],
  ],
  // ------------------------------------------------------------------ µLOOP: micro-loops (12-bit)
  'µLOOP': [
    ['AE', 'GANTZ CHIPS', { D: .45, a: loop('CHIPS'), b: .55, c: .4, d: .5, e: 0, f: .15 }],
    ['AFX', 'DRILL STUTTER', { D: .45, a: loop('FM'), b: .5, c: .25, d: .6, e: 0, f: 0 }],
    ['AE', 'ICE GLITCH', { D: .5, a: loop('GLAÇON'), b: .6, c: .2, d: .75, e: .1, f: .2 }],
    ['BOC', 'TAPE STOP', { D: .55, a: loop('ACCORD'), b: .85, c: 1, d: .35, e: 0, f: 0, fq: .6, bt: .2 }],
    ['SQPR', 'BREAK CHOP', { D: .4, a: loop('BRUIT'), b: .6, c: .5, d: .5, e: .2, f: .5 }],
    ['BJÖRK', 'CARD SHUFFLE', { D: .5, a: loop('PAPIER'), b: .7, c: .6, d: .5, e: .2, f: .6 }],
    ['FLYLO', 'ZAP REWIND', { D: .45, a: loop('ZAP'), b: .65, c: .3, d: .3, e: .3, f: .1 }],
    ['CLARK', 'GRIND', { D: .5, a: loop('GRAVIER'), b: .45, c: .3, d: .55, e: .3, f: .4, dr: .4 }],
    ['OPN', 'VOICE CHOP', { D: .5, a: loop('LANGUE'), b: .82, c: .45, d: .6, e: .05, f: .25, rv: .2 }],
    ['AE', 'BUBBLE MATH', { D: .55, a: loop('BULLES'), b: .6, c: .2, d: .7, e: .1, f: .3 }],
    ['AFX', 'RUBBER LOOP', { D: .45, a: loop('P.BULLE'), b: .55, c: .35, d: .4, e: .4, f: .1 }],
    ['PLAID', 'PING SHRINK', { D: .5, a: loop('PINGPONG'), b: .75, c: .3, d: .6, e: 0, f: 0 }],
    ['BOC', 'SNOW TAPE', { D: .6, a: loop('NEIGE'), b: .9, c: .7, d: .45, e: .2, f: .2, sr: .25, bt: .2 }],
    ['GSFL', 'METAL STUTTER', { D: .45, a: loop('PIÈCES'), b: .55, c: .3, d: .5, e: .2, f: .2, dr: .3 }],
    ['SEEFEEL', 'DUB LOOP', { D: .6, a: loop('GOUTTE'), b: .8, c: .9, d: .45, e: .02, f: .1, dl: .4 }],
    ['AE', 'ZIP SPIRAL', { D: .45, a: loop('ZIP'), b: .5, c: .25, d: .8, e: .1, f: .1 }],
    ['HUDMO', 'CHOPPED FM', { D: .4, a: loop('FM'), b: .7, c: .6, d: .58, e: .5, f: .3 }],
    ['BRDCST', 'CRACKLE REEL', { D: .55, a: loop('ALLUMETTE'), b: .85, c: .55, d: .42, e: .02, f: .4, dr: .35 }],
    ['AFX', 'FULL TO DUST', { D: .55, a: loop('BISCOTTE'), b: 1, c: .3, d: .55, e: 0, f: .2 }],
    ['AE', 'VELCRO SAW', { D: .45, a: loop('VELCRO'), b: .62, c: .45, d: .5, e: .08, f: .6 }],
  ],
  // ------------------------------------------------------------------ BUG: buffer errors
  'BUG': [
    ['AE', 'BUFFER ROT', { T: st(0), D: .5, a: .5, b: .3, c: .6, d: .2, e: .2, f: seed(3) }],
    ['AFX', 'SKIP CD', { T: st(12), D: .45, a: .75, b: .5, c: .7, d: .1, e: .1, f: seed(5) }],
    ['OPN', 'DROPOUT', { T: st(0), D: .55, a: .25, b: .45, c: .2, d: .5, e: .1, f: seed(7), rv: .2 }],
    ['CLARK', 'BIT FLIP', { T: st(-12), D: .45, a: .5, b: .25, c: .3, d: .1, e: .7, f: seed(2), dr: .2 }],
    ['AE', 'FM CORRUPT', { T: st(0), D: .45, a: 1, b: .35, c: .5, d: .3, e: .4, f: seed(9) }],
    ['SQPR', 'GLITCH BASS', { T: st(-12), D: .38, a: .5, b: .55, c: .4, d: .15, e: .2, f: seed(4), fq: .56, rs: .4, dr: .2 }],
    ['BOC', 'WARPED TAPE', { T: st(0), D: .6, a: .25, b: .7, c: .3, d: .25, e: 0, f: seed(1), fq: .55, bt: .25 }],
    ['GSFL', 'SAW ERROR', { T: st(0), D: .4, a: .5, b: .4, c: .5, d: .2, e: .5, f: seed(11), dr: .4 }],
    ['FLYLO', 'WOBBLE ERR', { T: st(12), D: .45, a: .75, b: .6, c: .4, d: .3, e: .1, f: seed(6) }],
    ['AE', 'MICRO SKIPS', { T: st(12), D: .4, a: .5, b: .1, c: .8, d: .1, e: .3, f: seed(13) }],
    ['AFX', 'SQUARE TRASH', { T: st(0), D: .4, a: .75, b: .3, c: .5, d: .4, e: .6, f: seed(8) }],
    ['BJÖRK', 'BROKEN MUSIC BOX', { T: st(24), D: .5, a: 0, b: .45, c: .5, d: .3, e: .1, f: seed(10), ir: 'B.MUSIQ', im: .4 }],
    ['BRDCST', 'LOOSE SPOOL', { T: st(12), D: .55, a: .25, b: .6, c: .55, d: .35, e: 0, f: seed(12) }],
    ['AE', 'DEAD PIXEL', { T: st(24), D: .3, a: .75, b: .05, c: .9, d: .05, e: .5, f: seed(14) }],
    ['HUDMO', 'STUTTER LEAD', { T: st(12), D: .45, a: .5, b: .35, c: .7, d: .1, e: .1, f: seed(15), rv: .2 }],
    ['PLAID', 'HICCUP', { T: st(12), D: .4, a: .25, b: .5, c: .6, d: .2, e: .05, f: seed(3) }],
    ['RUSTIE', 'GLITCH STAB', { T: st(12), D: .3, a: .5, b: .25, c: .5, d: .1, e: .3, f: seed(5), rv: .25 }],
    ['CLARK', 'SHRED', { T: st(0), D: .4, a: 1, b: .15, c: .3, d: .5, e: .8, f: seed(1), dr: .35 }],
    ['SEEFEEL', 'DUB FAULT', { T: st(0), D: .55, a: .25, b: .65, c: .4, d: .4, e: 0, f: seed(2), dl: .4 }],
    ['AE', 'RANDOM FAULT', { T: st(0), D: .45, a: .6, b: .4, c: .5, d: .3, e: .3, f: seed(0) }],
  ],
  // ------------------------------------------------------------------ CHAOS: chaotic maps
  'CHAOS': [
    ['AE', 'LOGISTIC', { T: .5, D: .4, a: ix(0, 4), b: .7, c: .3, d: .2, e: 0, f: seed(5) }],
    ['AE', 'HENON SNARE', { T: .55, D: .3, a: ix(1, 4), b: .6, c: .2, d: .3, e: .2, f: seed(2) }],
    ['AFX', 'LORENZ DRIFT', { T: .45, D: .6, a: ix(2, 4), b: .5, c: .6, d: .1, e: 0, f: seed(1) }],
    ['OPN', 'IKEDA', { T: .5, D: .45, a: ix(3, 4), b: .8, c: .4, d: .3, e: .1, f: seed(8), rv: .2 }],
    ['CLARK', 'NOISE STORM', { T: .6, D: .4, a: ix(0, 4), b: 1, c: .1, d: .6, e: .5, f: seed(3), dr: .3 }],
    ['SQPR', 'CHAOS BASS', { T: .3, D: .45, a: ix(2, 4), b: .45, c: .7, d: .3, e: .45, f: seed(4), fq: .58, dr: .3 }],
    ['AE', 'FOLDED', { T: .5, D: .35, a: ix(1, 4), b: .55, c: .3, d: .4, e: .8, f: seed(6) }],
    ['GSFL', 'HARSH CHAOS', { T: .55, D: .35, a: ix(0, 4), b: .9, c: .1, d: .5, e: .7, f: seed(7), dr: .5 }],
    ['BRDCST', 'SPUTNIK', { T: .7, D: .5, a: ix(3, 4), b: .5, c: .5, d: .2, e: .1, f: seed(9) }],
    ['FLYLO', 'COSMIC DRIP', { T: .65, D: .4, a: ix(2, 4), b: .65, c: .45, d: .3, e: .3, f: seed(10), dl: .3 }],
    ['AE', 'SELF MOD', { T: .5, D: .45, a: ix(3, 4), b: .6, c: .2, d: .9, e: .2, f: seed(11) }],
    ['BOC', 'BROKEN RADIO', { T: .45, D: .55, a: ix(1, 4), b: .45, c: .6, d: .2, e: .1, f: seed(12), fq: .6, bt: .25 }],
    ['PLAID', 'CHAOS BELL', { T: .65, D: .5, a: ix(3, 4), b: .35, c: .5, d: .1, e: .4, f: seed(13), rv: .25 }],
    ['AFX', 'DRILL CHAOS', { T: .6, D: .25, a: ix(0, 4), b: .85, c: .15, d: .5, e: .3, f: seed(14), rt: 5, rc: 6, rvl: .4 }],
    ['BJÖRK', 'INSECT', { T: .75, D: .3, a: ix(1, 4), b: .75, c: .25, d: .4, e: .2, f: seed(15) }],
    ['HUDMO', 'LASER CHAOS', { T: .6, D: .35, a: ix(2, 4), b: .7, c: .3, d: .6, e: .1, f: seed(4), rv: .2 }],
    ['AE', 'STRANGE ATTRACTOR', { T: .5, D: .6, a: ix(2, 4), b: .8, c: .5, d: .5, e: .5, f: seed(0) }],
    ['SEEFEEL', 'DRONE STORM', { T: .4, D: .8, a: ix(2, 4), b: .35, c: .8, d: .1, e: 0, f: seed(2), at: .35, rv: .3 }],
    ['CLARK', 'TEXTURE', { T: .5, D: .5, a: ix(3, 4), b: .55, c: .35, d: .3, e: .6, f: seed(7) }],
    ['RUSTIE', 'ZAPPER', { T: .7, D: .3, a: ix(0, 4), b: .75, c: .2, d: .7, e: .2, f: seed(1) }],
  ],
  // ------------------------------------------------------------------ VOX: formants
  'VOX': [
    ['AFX', 'WINDOWLICKER VOX', { T: st(12), D: .45, a: .25, b: .7, c: .35, d: .6, e: .1, f: .55 }],
    ['BJÖRK', 'BREATH CHOIR', { T: st(12), D: .6, a: .75, b: .5, c: .5, d: .5, e: .45, f: .5, at: .3, rv: .3 }],
    ['AE', 'TALK BOX', { T: st(0), D: .4, a: 0, b: .85, c: .3, d: .7, e: .1, f: .4 }],
    ['BOC', 'CHILDREN CHOIR', { T: st(24), D: .55, a: .5, b: .55, c: .5, d: .45, e: .2, f: .65, fq: .62, l1: L(3, .004, 'TUNE', 'TRI') }],
    ['OPN', 'REPLICA VOX', { T: st(12), D: .45, a: .25, b: .3, c: .3, d: .7, e: .1, f: .45, rv: .25 }],
    ['SQPR', 'VOWEL BASS', { T: st(-12), D: .4, a: .75, b: .7, c: .25, d: .6, e: .05, f: .4 }],
    ['FLYLO', 'GHOST VOX', { T: st(12), D: .5, a: 1, b: .3, c: .4, d: .55, e: .35, f: .6, dl: .3 }],
    ['CLARK', 'GROWL', { T: st(-12), D: .4, a: .5, b: .9, c: .2, d: .8, e: .2, f: .3, dr: .35 }],
    ['PLAID', 'OOH', { T: st(12), D: .5, a: .75, b: .5, c: .5, d: .4, e: .1, f: .5, rv: .2 }],
    ['AE', 'FORMANT CLICK', { T: st(12), D: .2, a: .5, b: .9, c: .1, d: .9, e: 0, f: .6 }],
    ['HUDMO', 'YEAH STAB', { T: st(12), D: .35, a: 0, b: .6, c: .2, d: .6, e: .2, f: .55, rv: .25 }],
    ['GSFL', 'DEMON', { T: st(-12), D: .45, a: .9, b: .3, c: .4, d: .7, e: .3, f: .15, dr: .4 }],
    ['BRDCST', 'ROBOT', { T: st(0), D: .45, a: .25, b: .5, c: .5, d: .9, e: 0, f: .5 }],
    ['AFX', 'COME TO DADDY', { T: st(-5), D: .4, a: .1, b: .8, c: .2, d: .75, e: .25, f: .25, dr: .3 }],
    ['SEEFEEL', 'WHISPER', { T: st(12), D: .55, a: .5, b: .5, c: .5, d: .3, e: .9, f: .5, rv: .35 }],
    ['AE', 'VOWEL MORPH', { T: st(0), D: .5, a: .5, b: .5, c: .5, d: .6, e: .1, f: .5, l1: L(4, .4, 'A', 'TRI') }],
    ['BJÖRK', 'COCOON BREATH', { T: st(12), D: .35, a: .75, b: .5, c: .5, d: .3, e: .8, f: .55 }],
    ['LFO', 'ROBOT HEY', { T: st(0), D: .3, a: .25, b: .75, c: .2, d: .65, e: .1, f: .5 }],
    ['OPN', 'CHOIR PAD', { T: st(12), D: .8, a: 1, b: .5, c: .5, d: .4, e: .25, f: .5, at: .5, rv: .4 }],
    ['RUSTIE', 'VOX LASER', { T: st(24), D: .35, a: .25, b: 1, c: .3, d: .8, e: .05, f: .7, rv: .2 }],
  ],
  // ------------------------------------------------------------------ CZ·PD: phase distortion
  'CZ·PD': [
    ['AFX', 'CZ BASS', { T: st(-12), D: .4, a: ix(0, 8), b: .3, c: .6, d: .25, e: 0, f: ix(1, 8) }],
    ['AE', 'RESO SWEEP', { T: st(0), D: .45, a: ix(5, 8), b: .2, c: .8, d: .35, e: 0, f: ix(1, 8) }],
    ['BOC', 'CZ PAD', { T: st(12), D: .85, a: ix(1, 8), b: .25, c: .3, d: .6, e: .1, f: ix(3, 8), at: .4, rv: .3, l1: L(3, .003, 'TUNE', 'TRI') }],
    ['SQPR', 'PD LEAD', { T: st(12), D: .5, a: ix(4, 8), b: .4, c: .5, d: .3, e: .2, f: ix(3, 8) }],
    ['OPN', 'GLASS RES', { T: st(12), D: .55, a: ix(6, 8), b: .5, c: .4, d: .4, e: .3, f: ix(4, 8), rv: .25 }],
    ['PLAID', 'RING BELL', { T: st(12), D: .55, a: ix(2, 8), b: .3, c: .3, d: .5, e: .7, f: ix(6, 8) }],
    ['GSFL', 'PD STAB', { T: st(0), D: .3, a: ix(3, 8), b: .6, c: .6, d: .2, e: .2, f: ix(3, 8), dr: .35 }],
    ['AE', 'DOUBLE SINE', { T: st(0), D: .45, a: ix(3, 8), b: .15, c: .5, d: .3, e: 0, f: ix(1, 8) }],
    ['CLARK', 'NASTY RES', { T: st(0), D: .4, a: ix(7, 8), b: .7, c: .3, d: .3, e: .4, f: ix(7, 8), dr: .3 }],
    ['FLYLO', 'WARM PLUCK', { T: st(12), D: .35, a: ix(1, 8), b: .1, c: .7, d: .2, e: 0, f: ix(1, 8) }],
    ['HUDMO', 'SYNTH BRASS', { T: st(0), D: .55, a: ix(0, 8), b: .4, c: .4, d: .5, e: .1, f: ix(3, 8), at: .1 }],
    ['LFO', 'PULSE BLEEP', { T: st(24), D: .25, a: ix(2, 8), b: .5, c: 0, d: .1, e: 0, f: ix(1, 8) }],
    ['BRDCST', 'ORGAN', { T: st(12), D: .7, a: ix(4, 8), b: .3, c: 0, d: .5, e: .25, f: ix(3, 8) }],
    ['AE', 'RES3 TOM', { T: st(-5), D: .3, a: ix(7, 8), b: .4, c: .8, d: .15, e: 0, f: ix(1, 8) }],
    ['AFX', 'ACID PD', { T: st(0), D: .35, a: ix(0, 8), b: .2, c: .9, d: .2, e: 0, f: ix(1, 8), dr: .2 }],
    ['BJÖRK', 'GLASS HARMONICA', { T: st(24), D: .6, a: ix(5, 8), b: .6, c: .2, d: .5, e: .5, f: ix(5, 8), at: .2, rv: .3 }],
    ['SENNI', 'PD ARP', { T: st(24), D: .2, a: ix(1, 8), b: .5, c: .5, d: .15, e: 0, f: ix(1, 8), dl: .25 }],
    ['SEEFEEL', 'HAZE', { T: st(12), D: .8, a: ix(6, 8), b: .3, c: .2, d: .7, e: .2, f: ix(2, 8), at: .45, dl: .3 }],
    ['AE', 'RATIO SEVEN', { T: st(0), D: .4, a: ix(4, 8), b: .5, c: .4, d: .3, e: .8, f: ix(7, 8) }],
    ['RUSTIE', 'SQUARE PUMP', { T: st(12), D: .45, a: ix(1, 8), b: .8, c: .3, d: .3, e: .1, f: ix(3, 8), rv: .2 }],
  ],
  // ------------------------------------------------------------------ FRACT: organic particle cloud (12-bit)
  'FRACT': [
    ['BJÖRK', 'VESPERTINE CARDS', { D: .4, a: org('PAPIER'), b: ix(7, 32), c: .4, d: .3, e: .3, f: .4 }],
    ['BJÖRK', 'ICE CRACK', { D: .45, a: org('GLAÇON'), b: ix(5, 32), c: .35, d: .25, e: .4, f: 1 }],
    ['AE', 'TWIG CRUNCH', { D: .4, a: org('RAMEAU'), b: ix(4, 32), c: .35, d: .2, e: .45, f: 1 }],
    ['AE', 'GRAVEL GRAIN', { D: .5, a: org('GRAVIER'), b: ix(15, 32), c: .5, d: .45, e: .6, f: .35 }],
    ['BOC', 'CAMPFIRE', { D: .7, a: org('FEU'), b: ix(3, 32), c: .4, d: .5, e: .5, f: 1, fq: .7, bt: .2 }],
    ['AFX', 'FINGER SNAPS', { D: .35, a: org('DOIGTS'), b: ix(1, 32), c: .1, d: 0, e: .3, f: 1 }],
    ['SQPR', 'PINGPONG ROLL', { D: .45, a: org('PINGPONG'), b: ix(9, 32), c: .6, d: .2, e: .3, f: 1 }],
    ['OPN', 'GLASS RAIN', { D: .6, a: org('VERRE'), b: ix(11, 32), c: .55, d: .5, e: .5, f: .45, rv: .25 }],
    ['FLYLO', 'CRISP SNARE', { D: .35, a: org('CHIPS'), b: ix(7, 32), c: .2, d: .2, e: .3, f: 1 }],
    ['CLARK', 'SAND STORM', { D: .55, a: org('SABLE'), b: ix(27, 32), c: .6, d: .8, e: .6, f: .6 }],
    ['PLAID', 'COINS', { D: .45, a: org('PIÈCES'), b: ix(5, 32), c: .4, d: .2, e: .5, f: 1 }],
    ['BRDCST', 'BUBBLES', { D: .5, a: org('BULLES'), b: ix(7, 32), c: .5, d: .4, e: .4, f: 1 }],
    ['AE', 'CRICKET SWARM', { D: .6, a: org('GRILLON'), b: ix(11, 32), c: .6, d: .6, e: .6, f: .5 }],
    ['GSFL', 'BONE CLAP', { D: .4, a: org('CLAP'), b: ix(5, 32), c: .2, d: .1, e: .3, f: 1, dr: .3 }],
    ['SEEFEEL', 'RAIN DUB', { D: .7, a: org('PLUIE'), b: ix(9, 32), c: .7, d: .6, e: .5, f: 1, dl: .35 }],
    ['AFX', 'KNUCKLE CRACK', { D: .35, a: org('PHALANGE'), b: ix(2, 32), c: .15, d: 0, e: .4, f: 1 }],
    ['BJÖRK', 'SNOW STEPS', { D: .5, a: org('NEIGE'), b: ix(4, 32), c: .4, d: .3, e: .3, f: 1 }],
    ['AE', 'LEAF SPRAY', { D: .55, a: org('FEUILLES'), b: ix(19, 32), c: .5, d: .6, e: .5, f: .6 }],
    ['BOC', 'CELERY SNAP', { D: .35, a: org('CÉLERI'), b: ix(1, 32), c: .1, d: .05, e: .3, f: 1, sr: .2 }],
    ['HUDMO', 'BUBBLE WRAP', { D: .4, a: org('P.BULLE'), b: ix(9, 32), c: .4, d: .2, e: .4, f: 1, rv: .2 }],
  ],
  // ------------------------------------------------------------------ ITERA: BIA-style additive / FM drum voice
  'ITERA': [
    ['AE', 'SKIN KICK', { T: st(-2), D: .5, a: .1, b: .15, c: 0, d: .1, e: .5, f: ix(1, 3) }],
    ['AE', 'METAL TOM', { T: st(7), D: .45, a: .7, b: .65, c: .2, d: .25, e: .5, f: ix(2, 3) }],
    ['AFX', 'LIQUID BOOM', { T: st(0), D: .6, a: .25, b: .4, c: .1, d: .2, e: .5, f: ix(1, 3) }],
    ['CLARK', 'FOLD KICK', { T: st(0), D: .5, a: .3, b: .45, c: .2, d: .7, e: .5, f: ix(1, 3), dr: .2 }],
    ['PLAID', 'BELL TONE', { T: st(24), D: .6, a: .9, b: .7, c: 0, d: 0, e: .55, f: ix(0, 3), rv: .2 }],
    ['GSFL', 'IRON HIT', { T: st(5), D: .4, a: .8, b: .8, c: .5, d: .5, e: .45, f: ix(2, 3), dr: .3 }],
    ['SQPR', 'SNARE BODY', { T: st(12), D: .3, a: .5, b: .5, c: .3, d: .3, e: .1, f: ix(0, 3) }],
    ['AE', 'FREE BAR', { T: st(12), D: .5, a: 1, b: .75, c: .1, d: .1, e: .5, f: ix(0, 3) }],
    ['OPN', 'GHOST TONE', { T: st(12), D: .7, a: .4, b: .6, c: .4, d: .2, e: .9, f: ix(0, 3), rv: .3 }],
    ['LFO', 'SUB DROP', { T: st(-5), D: .65, a: 0, b: .1, c: 0, d: 0, e: .5, f: ix(1, 3) }],
    ['AFX', 'NOISE SNARE', { T: st(10), D: .3, a: .6, b: .5, c: .4, d: .4, e: 0, f: ix(2, 3) }],
    ['BJÖRK', 'PRISM', { T: st(19), D: .55, a: .55, b: .8, c: .66, d: .3, e: .6, f: ix(0, 3), dl: .25 }],
    ['AE', 'ALIEN METAL', { T: st(0), D: .5, a: .6, b: 1, c: .5, d: .6, e: .5, f: ix(2, 3) }],
    ['HUDMO', 'TRAP TOM', { T: st(3), D: .5, a: .2, b: .3, c: .1, d: .15, e: .5, f: ix(1, 3) }],
    ['FLYLO', 'WOOD KNOCK', { T: st(12), D: .25, a: .45, b: .55, c: .2, d: .1, e: .45, f: ix(0, 3) }],
    ['BOC', 'SOFT THUMP', { T: st(-3), D: .45, a: .2, b: .3, c: .1, d: 0, e: .6, f: ix(1, 3), fq: .5, bt: .2 }],
    ['CLARK', 'PULSE CRUSH', { T: st(0), D: .45, a: .5, b: .6, c: .7, d: .95, e: .5, f: ix(2, 3) }],
    ['AE', 'SLOW SWELL', { T: st(7), D: .6, a: .5, b: .6, c: .3, d: .2, e: .85, f: ix(0, 3) }],
    ['SENNI', 'PLINK', { T: st(24), D: .38, a: .35, b: .7, c: .5, d: .15, e: .5, f: ix(0, 3), dl: .25 }],
    ['GSFL', 'DRONE HIT', { T: st(-7), D: .97, a: .6, b: .6, c: .5, d: .4, e: .7, f: ix(2, 3), fq: .55 }],
  ],
  // ------------------------------------------------------------------ TRIO: a mini Prophet (CEM3340 VCOs, 4-pole ladder on LP)
  // WAVE: 0 TRI  .25 SAW  .5 SAW+PULSE  .75 SQUARE  1 narrow PULSE · MODE: ix(0..3, 4) FREE SYNC RING PMOD
  // in SYNC and PMOD, C is the sweep that falls with the filter envelope (fd)
  'TRIO': [
    ['BOC', 'MINOR CHORD PAD', { T: st(12), D: .85, a: iv(3), b: iv(7), c: .45, d: .25, e: .6, f: ix(0, 4), at: .45, fq: .55, rs: .15, rv: .3, l1: L(3, .004, 'TUNE', 'TRI') }],
    ['AFX', 'FIFTH BASS', { T: st(0), D: .45, a: iv(7), b: iv(12), c: .2, d: .5, e: .4, f: ix(0, 4), fq: .4, rs: .5, fe: .75, fd: .3, pn: .3 }],
    ['AE', 'SYNC SCREAM', { T: st(0), D: .5, a: iv(12), b: iv(19), c: .65, d: .25, e: .8, f: ix(1, 4), fq: .72, rs: .3, fe: .62, fd: .45 }],
    ['PLAID', 'MAJOR SEVENTH', { T: st(12), D: .55, a: iv(4), b: iv(11), c: .3, d: .5, e: .55, f: ix(0, 4), fq: .65, rs: .2, fe: .6, fd: .3, dl: .3 }],
    ['SQPR', 'POLYMOD BASS', { T: st(0), D: .45, a: iv(12), b: iv(19), c: .35, d: .75, e: .35, f: ix(3, 4), fq: .5, rs: .4, fe: .72, fd: .25, dr: .15, pn: .35 }],
    ['GSFL', 'DARK OCTAVES', { T: st(0), D: .55, a: iv(-12), b: iv(12), c: .35, d: .5, e: .6, f: ix(0, 4), fq: .42, rs: .55, fe: .7, fd: .35, dr: .3, pn: .25 }],
    ['OPN', 'PROPHET CHORD', { T: st(12), D: .45, a: iv(5), b: iv(9), c: .5, d: .5, e: .55, f: ix(0, 4), fq: .62, rs: .3, fe: .64, fd: .35, rv: .25 }],
    ['LFO', 'BLEEP STACK', { T: st(24), D: .38, a: iv(12), b: iv(7), c: 0, d: 0, e: .4, f: ix(0, 4) }],
    ['AE', 'RING CLUSTER', { T: st(0), D: .55, a: iv(6), b: iv(13), c: .2, d: .25, e: .6, f: ix(2, 4), fq: .7 }],
    ['FLYLO', 'DETUNED KEYS', { T: st(12), D: .5, a: iv(0), b: iv(12), c: .7, d: .5, e: .5, f: ix(0, 4), fq: .6, l1: L(5, .004, 'TUNE', 'SH') }],
    ['HUDMO', 'HYPE LEAD', { T: st(24), D: .5, a: iv(7), b: iv(12), c: .6, d: .5, e: .65, f: ix(0, 4), fq: .85, rs: .25, rv: .25 }],
    ['BJÖRK', 'GLASS ORGAN', { T: st(12), D: .7, a: iv(12), b: iv(19), c: .1, d: 0, e: .6, f: ix(0, 4), at: .25, rv: .3 }],
    ['CLARK', 'RING GRIT', { T: st(0), D: .45, a: iv(1), b: iv(13), c: .3, d: .75, e: .7, f: ix(2, 4), fq: .65, rs: .35, dr: .35 }],
    ['SENNI', 'TRANCE CHORD', { T: st(12), D: .4, a: iv(3), b: iv(7), c: .65, d: .25, e: .6, f: ix(0, 4), fq: .74, rs: .25, fe: .66, fd: .25, dl: .25 }],
    ['AE', 'POLYMOD SHARDS', { T: st(12), D: .35, a: iv(17), b: iv(-5), c: .5, d: .25, e: .8, f: ix(3, 4), fq: .8, rs: .2, fd: .3 }],
    ['BRDCST', 'SPACE ORGAN', { T: st(12), D: .7, a: iv(12), b: iv(7), c: .25, d: 0, e: .5, f: ix(0, 4), l1: L(10, .003, 'TUNE') }],
    ['AFX', 'SUB + FIFTH', { T: st(-12), D: .55, a: iv(12), b: iv(19), c: .05, d: .25, e: .3, f: ix(0, 4), fq: .45, rs: .3, fe: .6, fd: .3, pn: .3 }],
    ['RUSTIE', 'MAX STACK', { T: st(12), D: .55, a: iv(4), b: iv(7), c: .7, d: .5, e: .7, f: ix(0, 4), fq: .9, rv: .3 }],
    ['SEEFEEL', 'DUB CHORD', { T: st(12), D: .4, a: iv(3), b: iv(10), c: .35, d: .75, e: .55, f: ix(0, 4), fq: .52, rs: .35, dl: .45 }],
    ['GSFL', 'SYNC OCTAVE STAB', { T: st(0), D: .3, a: iv(12), b: iv(24), c: .5, d: .5, e: .6, f: ix(1, 4), fq: .6, rs: .45, fe: .7, fd: .2, dr: .35, pn: .3 }],
  ],
  // ------------------------------------------------------------------ SINE: pure sine, sub, ring, start phase, gate
  // ring: 0 OFF 1 ½ 2 ¾ 3 ×1 4 ×1.5 5 ×2 6 ×2.76 7 ×3 8 ×3.5 9 ×4.2 10 ×5.4 11 ×6 12 ×7.1 13 ×8 14 ×11 15 ×16
  'SINE': [
    ['PNSC', 'VAKIO PULSE', { T: st(-12), D: .5, a: .5, b: 0, c: ix(0, 16), d: 0, e: 1, f: 0 }],
    ['PNSC', 'KESTO SUB', { T: st(-14), D: .8, a: .7, b: .1, c: ix(0, 16), d: 0, e: .6, f: 0 }],
    ['PNSC', 'RING TONE', { T: st(5), D: .5, a: 0, b: 0, c: ix(6, 16), d: .5, e: 1, f: 0 }],
    ['PNSC', 'KATODI CLICK', { T: st(0), D: .3, a: .3, b: .3, c: ix(0, 16), d: 1, e: .8, f: .2 }],
    ['PNSC', 'GRAVITON', { T: st(-12), D: .6, a: .6, b: .55, c: ix(0, 16), d: 0, e: .2, f: 0 }],
    ['PNSC', 'HIGH BEEP', { T: st(24), D: .45, a: 0, b: 0, c: ix(0, 16), d: .3, e: 1, f: 0 }],
    ['PNSC', 'NOISE GATE', { T: st(0), D: .42, a: .2, b: .2, c: ix(0, 16), d: .5, e: 1, f: .9 }],
    ['PNSC', 'AALTO TICK', { T: st(24), D: .22, a: 0, b: 0, c: ix(15, 16), d: 1, e: .9, f: .8 }],
    ['PNSC', 'MIKA RIM', { T: st(19), D: .25, a: 0, b: .4, c: ix(9, 16), d: 1, e: .6, f: 0 }],
    ['IKEDA', 'SINE KICK', { T: st(-10), D: .55, a: .4, b: .35, c: ix(0, 16), d: 1, e: 0, f: 0 }],
    ['IKEDA', '0°C', { T: st(12), D: .45, a: 0, b: 0, c: ix(0, 16), d: 0, e: 1, f: 0 }],
    ['NOTO', 'XERROX HUM', { T: st(-5), D: .85, a: .3, b: 0, c: ix(1, 16), d: 0, e: .7, f: 0 }],
    ['NOTO', 'UNITXT BLIP', { T: st(17), D: .2, a: 0, b: .7, c: ix(4, 16), d: .5, e: 1, f: 0 }],
    ['LFO', 'BLEEP SUB', { T: st(-7), D: .55, a: .5, b: .15, c: ix(0, 16), d: 0, e: .5, f: 0 }],
    ['AE', 'TRI SINE', { T: st(-3), D: .45, a: .2, b: .6, c: ix(4, 16), d: .2, e: .3, f: 0 }],
    ['AFX', 'AMBIENT HUM', { T: st(-12), D: .9, a: .5, b: 0, c: ix(2, 16), d: 0, e: .5, f: 0 }],
    ['SQPR', 'SUB DROP', { T: st(-17), D: .7, a: .8, b: .8, c: ix(0, 16), d: 0, e: .1, f: 0 }],
    ['CLARK', 'METAL RING', { T: st(12), D: .5, a: 0, b: .2, c: ix(12, 16), d: .5, e: .2, f: 0 }],
    ['OPN', 'ECCO SINE', { T: st(7), D: .7, a: .1, b: 0, c: ix(7, 16), d: 0, e: 0, f: 0, rv: .3 }],
    ['GSFL', 'DARK SINE', { T: st(-9), D: .65, a: .6, b: .4, c: ix(5, 16), d: .3, e: .3, f: 0 }],
  ],
  // ------------------------------------------------------------------ DATA: binary patterns read cell by cell
  // CELL: .1 = 0.1 ms  .3 = 0.4 ms  .5 = 1.6 ms  .7 = 6.3 ms  .9 = 25 ms · TONE: 0 noise, .5 pulse, 1 sine · BITS: ix(n - 1, 32)
  'DATA': [
    ['IKEDA', 'DATAMATICS', { T: st(0), D: .4, a: seed(3), b: .35, c: .55, d: 0, e: ix(15, 32), f: 0 }],
    ['IKEDA', 'TEST PATTERN', { T: st(0), D: .45, a: seed(5), b: .55, c: .5, d: .5, e: ix(15, 32), f: 0 }],
    ['IKEDA', '+/- CLICK', { T: st(0), D: .2, a: seed(2), b: .15, c: .3, d: .5, e: ix(2, 32), f: .6 }],
    ['IKEDA', 'SUPERCODEX', { T: st(0), D: .65, a: seed(9), b: .6, c: .7, d: 0, e: ix(31, 32), f: 0 }],
    ['IKEDA', 'MATRIX SINE', { T: st(12), D: .55, a: seed(4), b: .45, c: .6, d: 1, e: ix(11, 32), f: 0 }],
    ['IKEDA', 'DATAPLEX BUZZ', { T: st(0), D: .35, a: seed(6), b: .1, c: .5, d: .5, e: ix(7, 32), f: 0 }],
    ['IKEDA', 'SPECTRA', { T: st(24), D: .3, a: seed(8), b: .3, c: .8, d: 1, e: ix(5, 32), f: 0 }],
    ['IKEDA', 'FORMULA KICK', { T: st(-2), D: .5, a: seed(1), b: .5, c: .2, d: .5, e: ix(3, 32), f: 1 }],
    ['IKEDA', 'SINE BIT HAT', { T: st(24), D: .15, a: seed(10), b: .2, c: .5, d: 1, e: ix(2, 32), f: 0 }],
    ['NOTO', 'XERROX DATA', { T: st(0), D: .5, a: seed(11), b: .4, c: .45, d: .25, e: ix(23, 32), f: 0 }],
    ['NOTO', 'UNI ACRONYM', { T: st(5), D: .3, a: seed(12), b: .25, c: .65, d: .8, e: ix(9, 32), f: 0 }],
    ['NOTO', 'TRANSFORM', { T: st(0), D: .6, a: seed(13), b: .7, c: .4, d: .5, e: ix(15, 32), f: .2 }],
    ['NOTO', 'PROTOTYPE HAT', { T: st(12), D: .2, a: seed(14), b: .2, c: .6, d: 0, e: ix(4, 32), f: 0 }],
    ['AE', 'GANTZ DATA', { T: st(0), D: .45, a: seed(7), b: .5, c: .6, d: .3, e: ix(12, 32), f: 0 }],
    ['AE', 'DRANE BITS', { T: st(-5), D: .55, a: seed(15), b: .65, c: .5, d: .6, e: ix(6, 32), f: .3 }],
    ['AFX', 'BOUNCING BITS', { T: st(3), D: .7, a: seed(3), b: .75, c: .8, d: .4, e: ix(31, 32), f: 0 }],
    ['SQPR', 'DRILL BITS', { T: st(3), D: .3, a: seed(5), b: .4, c: .7, d: .2, e: ix(5, 32), f: 0 }],
    ['LFO', 'BLEEP DATA', { T: st(-5), D: .45, a: seed(1), b: .55, c: .7, d: 1, e: ix(3, 32), f: .5 }],
    ['SENNI', 'PULSE CODE', { T: st(7), D: .25, a: seed(2), b: .45, c: .9, d: .5, e: ix(7, 32), f: 0 }],
    ['CLARK', 'GLITCH CELL', { T: st(0), D: .4, a: seed(0), b: .55, c: .5, d: .35, e: ix(15, 32), f: 0 }],
  ],
  // ------------------------------------------------------------------ FM·Z: wild FM, zap, clean sub
  // INDX: .5 = 10  .7 = 20  .9 = 32  1 = 40 · IDEC: .3 = 3 ms  .5 = 13 ms  .7 = 60 ms  .9 = 280 ms · ZAP: d × 7 octaves
  'FM·Z': [
    ['AE', 'CONFIELD ZAP', { T: st(-5), D: .5, a: r(8), b: .7, c: .45, d: .5, e: .5, f: .2 }],
    ['AE', 'GESCOM FM', { T: st(0), D: .4, a: r(12), b: .9, c: .55, d: .3, e: .3, f: .6 }],
    ['AE', 'XMOD NOISE', { T: st(0), D: .45, a: r(5), b: 1, c: .75, d: .2, e: .2, f: 1 }],
    ['SQPR', 'BASS ZAP', { T: st(-12), D: .45, a: r(3), b: .55, c: .4, d: .7, e: .6, f: 0 }],
    ['SQPR', 'SNARE FM', { T: st(14), D: .35, a: r(17), b: .9, c: .6, d: .3, e: .1, f: .8 }],
    ['AFX', 'DRUKQS FM', { T: st(5), D: .3, a: r(21), b: 1, c: .5, d: .4, e: .2, f: .4 }],
    ['AFX', 'FM HAT', { T: st(24), D: .27, a: r(20), b: 1, c: .55, d: 0, e: 0, f: .7, ncl: .9, fm: .6, add: .5, bnd: bd(17.6) }],
    ['LFO', 'FREQUENCY SUB', { T: st(-10), D: .65, a: r(3), b: .35, c: .3, d: .4, e: .9, f: 0 }],
    ['CLARK', 'BODY RIOT', { T: st(-3), D: .5, a: r(10), b: .8, c: .5, d: .6, e: .4, f: .5 }],
    ['OPN', 'GARDEN ZAP', { T: st(12), D: .55, a: r(16), b: .6, c: .7, d: .2, e: .1, f: .1, rv: .2 }],
    ['OPN', 'SUB GLIDE', { T: st(-9), D: .8, a: r(3), b: .3, c: .6, d: .25, e: 1, f: 0 }],
    ['HUDMO', 'LAZER KICK', { T: st(-12), D: .6, a: r(3), b: .5, c: .3, d: .9, e: .7, f: 0 }],
    ['RUSTIE', 'GLASS ZAP', { T: st(19), D: .35, a: r(19), b: .7, c: .5, d: .8, e: 0, f: .1 }],
    ['SENNI', 'FM PLUCK', { T: st(12), D: .3, a: r(14), b: .5, c: .45, d: .1, e: 0, f: 0 }],
    ['GSFL', 'DARK FM SUB', { T: st(-12), D: .7, a: r(1), b: .6, c: .55, d: .3, e: .8, f: .3 }],
    ['PLAID', 'BELL ZAP', { T: st(14), D: .5, a: r(13), b: .55, c: .7, d: .15, e: 0, f: 0 }],
    ['B12', 'ELECTRO TOM', { T: st(-2), D: .45, a: r(6), b: .5, c: .5, d: .5, e: .3, f: .1 }],
    ['BLEEP', 'BLEEP ZAP', { T: st(10), D: .3, a: r(3), b: .3, c: .4, d: .6, e: .2, f: 0 }],
    ['MCRPT', 'FM HARDCORE', { T: st(-7), D: .5, a: r(22), b: 1, c: .5, d: .7, e: .5, f: .9 }],
    ['NOTO', 'NULL FM', { T: st(7), D: .28, a: r(23), b: .8, c: .3, d: .2, e: 0, f: .3 }],
  ],
  // ------------------------------------------------------------------ HARD: industrial kick, shaper, clean sub
  // SWP: a × 7 octaves · STIM: .3 = 3.7 ms  .5 = 9 ms  .7 = 21 ms · DRIV: .5 = ×8  .7 = ×18  .9 = ×40 · SHAP: 0 soft, .33 hard, .67 fold, 1 step
  'HARD': [
    ['MCRPT', 'NEUROPHONIE', { T: st(0), D: .6, a: .6, b: .35, c: .75, d: .45, e: .35, f: .7 }],
    ['MCRPT', 'GABBER 909', { T: st(3), D: .55, a: .55, b: .3, c: .7, d: .33, e: .1, f: .6 }],
    ['MCRPT', 'INDUS RUMBLE', { T: st(-2), D: .8, a: .45, b: .5, c: .6, d: .2, e: .5, f: .8 }],
    ['MCRPT', 'FOLD BOMB', { T: st(0), D: .6, a: .7, b: .4, c: .85, d: .67, e: .3, f: .7 }],
    ['MCRPT', 'STEP CRUSH', { T: st(0), D: .5, a: .6, b: .3, c: .7, d: 1, e: .2, f: .6 }],
    ['MCRPT', 'TERROR KICK', { T: st(5), D: .45, a: .8, b: .2, c: .9, d: .4, e: .4, f: .5 }],
    ['MCRPT', 'NOISE WALL', { T: st(0), D: .7, a: .4, b: .4, c: .9, d: .5, e: .9, f: .6 }],
    ['MCRPT', 'SUB TAIL', { T: st(-4), D: .7, a: .5, b: .35, c: .6, d: .33, e: .1, f: 1 }],
    ['GSFL', 'PURSUIT', { T: st(-1), D: .5, a: .5, b: .35, c: .55, d: .3, e: .2, f: .6 }],
    ['GSFL', 'DARK ALEPH', { T: st(-3), D: .65, a: .45, b: .4, c: .5, d: .15, e: .45, f: .7 }],
    ['GSFL', 'WALL OF SUB', { T: st(-6), D: .75, a: .5, b: .4, c: .45, d: .05, e: .2, f: 1 }],
    ['CLARK', 'TOTEMS KICK', { T: st(0), D: .55, a: .55, b: .4, c: .6, d: .55, e: .25, f: .5 }],
    ['CLARK', 'TOM STEP', { T: st(12), D: .4, a: .4, b: .5, c: .5, d: .9, e: .1, f: .2 }],
    ['AFX', 'HARDCORE 909', { T: st(2), D: .45, a: .6, b: .3, c: .65, d: .33, e: .15, f: .5 }],
    ['SQPR', 'DRILL HARD', { T: st(7), D: .35, a: .7, b: .25, c: .55, d: .2, e: .3, f: .4 }],
    ['AE', 'SPLITTER FOLD', { T: st(4), D: .4, a: .4, b: .5, c: .7, d: .8, e: .1, f: .3 }],
    ['OPN', 'RIFT DISTORT', { T: st(-5), D: .7, a: .3, b: .6, c: .45, d: .1, e: .6, f: .6 }],
    ['RUSTIE', 'GLASS HAMMER', { T: st(9), D: .35, a: .8, b: .3, c: .8, d: .67, e: .1, f: .4 }],
    ['HUDMO', 'DISTO 808', { T: st(-5), D: .8, a: .25, b: .5, c: .5, d: .1, e: 0, f: .9 }],
    ['PNSC', 'KULMA KICK', { T: st(-3), D: .55, a: .35, b: .45, c: .4, d: .25, e: .2, f: .8 }],
  ],
  // ------------------------------------------------------------------ HEX·BD: the hexagonal-pad analog kick
  // a FLTR (noise cutoff) · b BEND (.5 flat, right drops, left climbs) · c BAL tone <-> noise · d CLIK · e SWP (filter closes) · f ATK blip
  'HEX·BD': [
    ['ELECTRO', 'HEX CLASSIC', { T: st(0), D: .55, a: .45, b: .72, c: .4, d: .45, e: .6, f: .25 }],
    ['ELECTRO', 'NOISE THUD', { T: st(-2), D: .5, a: .35, b: .68, c: .62, d: .5, e: .7, f: .2 }],
    ['ELECTRO', 'DEEP DROP', { T: st(-5), D: .7, a: .3, b: .85, c: .3, d: .3, e: .55, f: .15 }],
    ['ELECTRO', 'GATED BOOM', { T: st(-3), D: .62, a: .42, b: .7, c: .45, d: .4, e: .6, f: .2, ir: 'GARAGE', im: .35, ig: .5 }],
    ['ELECTRO', 'PAD SLAP', { T: st(2), D: .35, a: .6, b: .62, c: .55, d: .75, e: .5, f: .35 }],
    ['AFX', 'DRUKQS HEX', { T: st(3), D: .3, a: .55, b: .8, c: .5, d: .6, e: .4, f: .5 }],
    ['AFX', 'ANALOG BOUNCE', { T: st(1), D: .4, a: .4, b: .9, c: .35, d: .35, e: .45, f: .3 }],
    ['SQPR', 'SLAP BASS DRUM', { T: st(4), D: .28, a: .7, b: .6, c: .45, d: .85, e: .3, f: .6 }],
    ['PLAID', 'ROUND HEX', { T: st(0), D: .48, a: .3, b: .6, c: .2, d: .25, e: .4, f: .1 }],
    ['B12', 'ELECTRO BREAK', { T: st(-1), D: .45, a: .5, b: .74, c: .48, d: .55, e: .65, f: .3, dr: .15 }],
    ['BLEEP', 'SHEFFIELD PAD', { T: st(-4), D: .6, a: .25, b: .66, c: .25, d: .2, e: .5, f: .05 }],
    ['LFO', 'FREQUENCY HEX', { T: st(-7), D: .75, a: .2, b: .58, c: .15, d: .15, e: .4, f: 0 }],
    ['CLARK', 'NOISE PILLAR', { T: st(-2), D: .5, a: .75, b: .7, c: .8, d: .6, e: .8, f: .3, dr: .3 }],
    ['AE', 'RISING HEX', { T: st(0), D: .45, a: .45, b: .32, c: .5, d: .45, e: .5, f: .3 }],
    ['GSFL', 'HEX INDUSTRIAL', { T: st(-1), D: .55, a: .55, b: .75, c: .55, d: .6, e: .7, f: .35, dr: .45, fq: .8 }],
    ['RUSTIE', 'ARENA SYN', { T: st(3), D: .4, a: .65, b: .82, c: .5, d: .7, e: .55, f: .45, rv: .2 }],
    ['HUDMO', 'HEX 808', { T: st(-5), D: .82, a: .25, b: .6, c: .1, d: .2, e: .3, f: .2 }],
    ['BOC', 'WORN PAD', { T: st(-2), D: .5, a: .35, b: .7, c: .4, d: .3, e: .6, f: .15, bt: .25, sr: .2, fq: .6 }],
    ['OPN', 'GHOST HEX', { T: st(1), D: .4, a: .45, b: .66, c: .6, d: .2, e: .7, f: .1, rv: .3, fq: .55 }],
    ['FLYLO', 'WONKY HEX', { T: st(-3), D: .6, a: .35, b: .92, c: .45, d: .35, e: .55, f: .25, dl: .15 }],
  ],
  // ------------------------------------------------------------------ HEX·SD: the hexagonal-pad analog snare
  // a FLTR · b BEND · c BAL · d CLIK · e RESO (the noise filter) · f RIM (FM crack on hard hits)
  'HEX·SD': [
    ['ELECTRO', 'HEX SNARE', { T: st(0), D: .4, a: .7, b: .62, c: .62, d: .5, e: .55, f: .35 }],
    ['ELECTRO', 'NOISE CRACK', { T: st(2), D: .35, a: .82, b: .58, c: .78, d: .65, e: .45, f: .5 }],
    ['ELECTRO', 'GATED HEX', { T: st(-1), D: .5, a: .72, b: .6, c: .66, d: .55, e: .5, f: .3, ir: 'ESCALIER', im: .4, ig: .45 }],
    ['ELECTRO', 'RIMSHOT PAD', { T: st(5), D: .25, a: .65, b: .55, c: .4, d: .7, e: .35, f: .9 }],
    ['ELECTRO', 'LASER SNARE', { T: st(3), D: .38, a: .6, b: .85, c: .5, d: .4, e: .6, f: .2 }],
    ['AFX', 'HEX WIRE', { T: st(4), D: .3, a: .9, b: .6, c: .85, d: .6, e: .7, f: .4 }],
    ['SQPR', 'FUSION CRACK', { T: st(6), D: .22, a: .78, b: .66, c: .55, d: .85, e: .5, f: .7 }],
    ['PLAID', 'SOFT HEX', { T: st(-2), D: .45, a: .5, b: .58, c: .55, d: .25, e: .3, f: .1 }],
    ['B12', 'ELECTRO CRACK', { T: st(1), D: .32, a: .74, b: .55, c: .72, d: .6, e: .65, f: .2 }],
    ['BLEEP', 'BLEEP SNARE', { T: st(7), D: .3, a: .6, b: .52, c: .45, d: .5, e: .75, f: .15 }],
    ['LFO', 'TUNED HEX', { T: st(-3), D: .42, a: .55, b: .6, c: .35, d: .35, e: .8, f: .25 }],
    ['CLARK', 'BURNT SNARE', { T: st(0), D: .4, a: .8, b: .64, c: .7, d: .7, e: .6, f: .4, dr: .4 }],
    ['AE', 'RESO CHAMBER', { T: st(-5), D: .55, a: .45, b: .5, c: .75, d: .3, e: .95, f: 0 }],
    ['GSFL', 'METAL HEX', { T: st(2), D: .35, a: .85, b: .6, c: .7, d: .75, e: .7, f: .6, dr: .3, fq: .85 }],
    ['RUSTIE', 'ARENA SNARE', { T: st(4), D: .45, a: .76, b: .7, c: .6, d: .65, e: .55, f: .5, rv: .25 }],
    ['HUDMO', 'TRAP HEX', { T: st(5), D: .28, a: .7, b: .56, c: .65, d: .8, e: .45, f: .3 }],
    ['BOC', 'TAPE HEX', { T: st(-1), D: .42, a: .6, b: .6, c: .6, d: .35, e: .5, f: .2, bt: .2, sr: .25, fq: .6 }],
    ['OPN', 'HAZE SNARE', { T: st(0), D: .55, a: .55, b: .62, c: .7, d: .2, e: .6, f: .1, rv: .35 }],
    ['FLYLO', 'DUSTY HEX', { T: st(-2), D: .38, a: .62, b: .66, c: .58, d: .45, e: .5, f: .3, sr: .3, bt: .15 }],
    ['SENNI', 'TRANCE ROLL', { T: st(3), D: .25, a: .7, b: .56, c: .6, d: .55, e: .5, f: .25, rt: 3, rc: 4 }],
  ],
  // ------------------------------------------------------------------ HEX·TM: the hexagonal-pad analog toms
  // a FLTR · b BEND · c BAL · d CLIK · e SWP · f SIZE (HI / MID / LOW)
  'HEX·TM': [
    ['ELECTRO', 'HEX TOM HI', { T: st(0), D: .5, a: .5, b: .78, c: .45, d: .4, e: .65, f: ix(0, 3) }],
    ['ELECTRO', 'HEX TOM MID', { T: st(0), D: .52, a: .5, b: .78, c: .45, d: .4, e: .65, f: ix(1, 3) }],
    ['ELECTRO', 'HEX TOM LOW', { T: st(0), D: .55, a: .48, b: .78, c: .45, d: .4, e: .65, f: ix(2, 3) }],
    ['ELECTRO', 'PEW PEW', { T: st(5), D: .45, a: .55, b: .95, c: .35, d: .3, e: .6, f: ix(0, 3) }],
    ['ELECTRO', 'SYN TOM FILL', { T: st(-2), D: .6, a: .45, b: .85, c: .5, d: .45, e: .7, f: ix(1, 3), rt: 3, rc: 4 }],
    ['ELECTRO', 'NOISE TOM', { T: st(0), D: .5, a: .62, b: .74, c: .72, d: .5, e: .75, f: ix(1, 3) }],
    ['ELECTRO', 'GATED TOMS', { T: st(-1), D: .6, a: .5, b: .8, c: .5, d: .45, e: .65, f: ix(2, 3), ir: 'GARAGE', im: .4, ig: .5 }],
    ['AFX', 'CHEETAH TOM', { T: st(7), D: .35, a: .6, b: .9, c: .3, d: .55, e: .5, f: ix(0, 3) }],
    ['AFX', 'RISING TOM', { T: st(-5), D: .75, a: .4, b: .25, c: .35, d: .3, e: .55, f: ix(2, 3) }],
    ['SQPR', 'FUSION TOM', { T: st(3), D: .4, a: .55, b: .7, c: .3, d: .6, e: .5, f: ix(1, 3) }],
    ['PLAID', 'MELODIC HEX', { T: st(0), D: .55, a: .4, b: .6, c: .2, d: .25, e: .4, f: ix(1, 3) }],
    ['B12', 'ROBOT TOM', { T: st(-3), D: .5, a: .55, b: .82, c: .4, d: .5, e: .6, f: ix(2, 3), dl: .2 }],
    ['BLEEP', 'BLEEP TOM', { T: st(12), D: .3, a: .45, b: .66, c: .1, d: .3, e: .4, f: ix(0, 3) }],
    ['LFO', 'SUB TOM', { T: st(-7), D: .7, a: .3, b: .7, c: .15, d: .2, e: .5, f: ix(2, 3) }],
    ['CLARK', 'HEAVY TOM', { T: st(-2), D: .6, a: .7, b: .8, c: .6, d: .65, e: .75, f: ix(2, 3), dr: .35 }],
    ['AE', 'ZIP TOM', { T: st(9), D: .25, a: .65, b: 1, c: .45, d: .5, e: .55, f: ix(0, 3) }],
    ['RUSTIE', 'ARENA TOM', { T: st(2), D: .55, a: .6, b: .84, c: .45, d: .55, e: .6, f: ix(1, 3), rv: .25 }],
    ['BOC', 'WARBLE TOM', { T: st(0), D: .55, a: .45, b: .72, c: .4, d: .3, e: .6, f: ix(1, 3), bt: .2, l1: L(3, .006, 'TUNE') }],
    ['OPN', 'ECHO TOM', { T: st(-4), D: .6, a: .5, b: .76, c: .5, d: .35, e: .65, f: ix(2, 3), dl: .35, rv: .2 }],
    ['FLYLO', 'COSMIC TOM', { T: st(1), D: .5, a: .5, b: .88, c: .55, d: .4, e: .7, f: ix(0, 3), dl: .25 }],
  ],
  // ------------------------------------------------------------------ MACRO: the 24 models of the macro oscillator
  // a = model (mo), b / c / d = HARMONICS / TIMBRE / MORPH, e = envelope on TIMBRE, f = OUT / AUX; the six-op models
  // pick a voice of their bank with b (dx)
  'MACRO': [
    ['AE', 'MODEL KICK', { T: st(-17), D: .5, a: mo('BD'), b: .3, c: .45, d: .55, e: .5, f: 0 }],
    ['GSFL', 'DRIVEN BODY', { T: st(-15), D: .5, a: mo('BD'), b: .78, c: .6, d: .45, e: .5, f: 0 }],
    ['HUDMO', 'SYNTH BOOM', { T: st(-19), D: .5, a: mo('BD'), b: .35, c: .3, d: .75, e: .5, f: 1 }],
    ['SQPR', 'MODEL SNARE', { T: st(4), D: .5, a: mo('SD'), b: .55, c: .5, d: .45, e: .5, f: 0 }],
    ['IKEDA', 'METAL HAT', { T: st(12), D: .5, a: mo('HH'), b: .65, c: .7, d: .18, e: .5, f: 0 }],
    ['RUSTIE', 'NOISE OPEN HAT', { T: st(7), D: .5, a: mo('HH'), b: .4, c: .6, d: .6, e: .5, f: 1 }],
    ['PLAID', 'MODAL BELL', { T: st(12), D: .55, a: mo('MODL'), b: .62, c: .6, d: .45, e: .6, f: 0 }],
    ['BOC', 'INHARMONIC PLUCK', { T: st(0), D: .55, a: mo('STRG'), b: .15, c: .7, d: .35, e: .55, f: 0 }],
    ['AE', 'PARTICLE DUST', { T: st(5), D: .45, a: mo('PART'), b: .55, c: .6, d: .35, e: .5, f: 0, rv: .15 }],
    ['OPN', 'MINOR STAB', { T: st(0), D: .35, a: mo('CHRD'), b: .3, c: .45, d: .25, e: .75, f: 0 }],
    ['BLEEP', 'BLEEP SQUARE', { T: st(12), D: .3, a: mo('VA'), b: 0, c: .95, d: .1, e: .5, f: 0 }],
    ['FLYLO', 'TABLE BASS', { T: st(-12), D: .45, a: mo('WTBL'), b: .3, c: .35, d: .5, e: .7, f: 0 }],
    ['CLARK', 'FOLD LEAD', { T: st(0), D: .4, a: mo('WSHP'), b: .6, c: .45, d: .4, e: .75, f: 0 }],
    ['BJÖRK', 'SPEECH SYNTH', { T: st(0), D: .6, a: mo('SPCH'), b: .3, c: .5, d: .5, e: .5, f: 0 }],
    ['AFX', 'TWO-OP TICK', { T: st(7), D: .2, a: mo('FM'), b: .7, c: .55, d: .35, e: .8, f: 0 }],
    ['LFO', 'VCF ACID', { T: st(-12), D: .35, a: mo('VCF'), b: .7, c: .35, d: .2, e: .85, f: 0 }],
    ['SQPR', 'DX SOLID BASS', { T: st(-12), D: .5, a: mo('6OP·A'), b: dx(0), c: .5, d: .5, e: .5, f: 0, gt: .15 }],
    ['BOC', 'DX E.PIANO', { T: st(0), D: .5, a: mo('6OP·B'), b: dx(0), c: .5, d: .5, e: .5, f: 0, gt: .2 }],
    ['PLAID', 'DX MARIMBA', { T: st(0), D: .5, a: mo('6OP·B'), b: dx(17), c: .5, d: .5, e: .5, f: 0, gt: .02 }],
    ['SENNI', 'CHIP ARP', { T: st(0), D: .45, a: mo('CHIP'), b: .35, c: .6, d: .3, e: .75, f: 0 }],
  ],
  // ------------------------------------------------------------------ HALO: chord pads, harmonic spread
  // a CHRD (ch) · b PROG (pg) · c SPRD · d WAVE (0 SINE .2 SOFT .4 SAW .6 PULSE .8 GLASS 1 METAL) · e BLOOM · D the let-go (50 ms .. 20 s)
  // sw SWEL (5 ms .. 8 s) · gl GLID · md MODE (KEY: the project's scale) · iv INV · df DRFT · ai AIR · wd WIDE
  'HALO': [
    ['BOC', 'OLSON DAWN', { D: .6, a: ch('9TH'), b: pg('AEON'), c: .45, d: .38, e: .15, sw: .62, df: .55, ai: .2, wd: .6, fq: .66, rs: .12, rv: .35, dtun: .35, vari: .3 }],
    ['BOC', 'ROYGBIV WASH', { D: .55, a: ch('7TH'), b: pg('DUSK'), c: .3, d: .42, e: .1, sw: .55, df: .65, ai: .15, wd: .7, fq: .58, rs: .15, rv: .3, l1: L(0, .12, 'FREQ'), dtun: .4, drop: .2 }],
    ['BOC', 'TAPE CHOIR', { D: .7, a: ch('ADD9'), b: pg('TIDE'), c: .5, d: .2, e: .35, sw: .7, df: .8, ai: .45, wd: .5, rv: .45, dtun: .45, vari: .4, drop: .35 }],
    ['BOC', 'DAYVAN GLIDE', { D: .6, a: ch('6/9'), b: pg('VAMP'), c: .55, d: .5, e: .2, sw: .5, gl: .45, df: .5, ai: .15, wd: .65, fq: .7, rv: .4 }],
    ['BOC', 'GEOGADDI HAZE', { D: .72, a: ch('11TH'), b: pg('FALL'), c: .6, d: .3, e: .25, sw: .75, df: .7, ai: .3, wd: .75, bt: .12, rv: .4, dtun: .5, vari: .25, drop: .25 }],
    ['BOC', 'CAMPFIRE SUS', { D: .55, a: ch('SUS2'), b: pg('RISE'), c: .4, d: .12, e: .2, sw: .5, gl: .25, df: .45, ai: .25, wd: .5, rv: .3, dtun: .3 }],
    ['AE', 'AMBER BELLS', { D: .75, a: ch('QRTL'), b: pg('AMBR'), c: .7, d: .8, e: .4, sw: .3, df: .1, ai: .1, wd: .8, rv: .5, shen: .35 }],
    ['AE', 'TRI REPETAE', { D: .7, a: ch('9TH'), b: pg('4THS'), c: .65, d: .86, e: .3, sw: .45, gl: .3, df: .2, wd: .7, fq: .72, rv: .45, l1: L(0, .25, 'C', 'TRI') }],
    ['AE', 'INCUNABULA MIST', { D: .65, a: ch('7TH'), b: pg('MEDI'), c: .5, d: .6, e: .2, sw: .65, df: .3, ai: .35, wd: .6, rv: .4 }],
    ['AE', 'METAL CLUSTER', { D: .5, a: ch('CLST'), b: pg('SLIP'), c: .85, d: 1, e: .5, sw: .2, df: .25, ai: .15, wd: .9, rv: .35, ir: 'CANETTE', im: .25 }],
    ['AE', 'OVERSTEPS DRIFT', { D: .7, a: ch('11TH'), b: pg('WALK'), c: .75, d: .9, e: .45, sw: .5, gl: .5, md: ix(11, 12), df: .35, ai: .2, wd: .8, rv: .5 }],
    ['AE', 'GARBAGE HALO', { D: .6, a: ch('5THS'), b: pg('4THS'), c: .5, d: .55, e: .6, sw: .4, df: .4, ai: .25, wd: .5, bt: .35, sr: .2, rv: .3 }],
    ['SEEFEEL', 'SUCK SPREAD', { D: .8, a: ch('SUS4'), b: pg('TIDE'), c: .8, d: .15, e: .55, sw: .8, df: .5, ai: .5, wd: 1, rv: .55, dl: .25 }],
    ['OPN', 'REPLICA GLASS', { D: .7, a: ch('6/9'), b: pg('DAWN'), c: .6, d: .75, e: .5, sw: .55, gl: .2, df: .3, ai: .2, wd: .7, rv: .45, shen: .4, drop: .12 }],
    ['AFX', 'SELECTED AMBIENT', { D: .85, a: ch('9TH'), b: pg('AEON'), c: .35, d: .05, e: .3, sw: .85, df: .4, ai: .6, wd: .8, rv: .5, dtun: .25, drop: .15 }],
    ['BIBIO', 'FOLK DRONE', { D: .65, a: ch('5TH'), b: pg('OFF'), c: .6, d: .3, e: .4, sw: .6, df: .75, ai: .55, wd: .6, rv: .35, dtun: .3, vari: .35 }],
    ['BRDCST', 'TENDER BUTTONS', { D: .55, a: ch('TRI'), b: pg('RISE'), c: .4, d: .48, e: .15, sw: .45, gl: .4, df: .55, wd: .5, fq: .6, rs: .2, rv: .3 }],
    ['B12', 'ELECTRO SOMA', { D: .5, a: ch('7TH'), b: pg('DUSK'), c: .45, d: .45, e: .1, sw: .45, gl: .25, df: .2, wd: .55, fq: .7, rs: .3, dl: .2, rv: .25 }],
    ['PLAID', 'NOT FOR THREES', { D: .6, a: ch('ADD9'), b: pg('4THS'), c: .5, d: .7, e: .3, sw: .5, gl: .35, df: .25, ai: .1, wd: .6, rv: .4 }],
    ['CLARK', 'TURNING DUST', { D: .7, a: ch('11TH'), b: pg('FALL'), c: .9, d: .95, e: .7, sw: .4, df: .45, ai: .3, wd: .9, bt: .2, rv: .45, vari: .3, drop: .3 }],
    // v71: SUB, a pure sine under the lowest note (AUTO: between 45 and 90 Hz)
    ['AFX', 'RHUBARB DEPTHS', { D: .85, a: ch('9TH'), b: pg('AEON'), c: .15, d: .1, e: .25, sw: .8, df: .35, ai: .4, wd: .75, rv: .45, sub: .7, soct: ix(0, 3), dtun: .2 }],
  ],
  // ------------------------------------------------------------------ BREAK: the break slicer
  // (ways of playing the loops the user loads: which slot is the track's, a preset leaves it; SWAP reaches the others)
  'BREAK': [
    ['GOLDIE', 'TIMELESS ROLL', { rn: .1, sd: ix(3, 33), e: .12, rv: .12 }],
    ['GOLDIE', 'INNER CITY SWAP', { xp: .15, rn: .12, su: .06, sd: ix(9, 33), e: .1 }],
    ['PHOTEK', 'NINJA CUTS', { rn: .28, su: .12, rr: .06, sd: ix(5, 33), fq: .82 }],
    ['PHOTEK', 'HIDDEN CAMERA', { d: len('1/8'), e: .15, rv: .15, fq: .78 }],
    ['BUKEM', 'ATMOSPHERIC ROLL', { xp: .1, e: .25, rv: .25 }],
    ['BUKEM', 'HORIZON DUB', { sk: .1, e: .3, rv: .3, dl: .15 }],
    ['SQPR', 'COME ON MY SELECTOR', { xp: .3, rn: .45, su: .35, rr: .15, pj: .2 }],
    ['SQPR', 'BIG LOADER SWAP', { xp: .5, rn: .15, su: .1, sd: ix(14, 33) }],
    ['AFX', 'DRILL STUTTER', { su: .5, rn: .3, pj: .25, rr: .1 }],
    ['AFX', 'CORNISH SHUFFLE', { su: .18, sk: .08, e: .2 }],
    ['VSNARE', 'ROSZ CHOPS', { d: len('1/16'), xp: .35, rn: .6, su: .5, rr: .25, pj: .3 }],
    ['DJSHDW', 'ENDTRODUCING DUST', { b: VARI, e: .55, fq: .72 }],
    ['DJSHDW', 'LEVEE WEIGHT', { b: VARI, T: st(-2), e: .2, dr: .2 }],
    ['BOC', 'HI SCORES BAP', { b: VARI, T: st(-3), e: .6, fq: .68, bt: .1 }],
    ['FLYLO', 'LOFI SWAY', { xp: .1, sk: .12, e: .45, fq: .7 }],
    ['PREFUSE', 'GLITCH HOP SNIPS', { d: len('1/8'), rn: .3, su: .2, sk: .15, sd: ix(12, 33) }],
    ['CLARK', 'BACKWARDS', { rr: .35, rn: .2, sd: ix(7, 33) }],
    ['AE', 'ANVIL VAPOUR', { xp: .25, rn: .3, rr: .2, su: .2, e: .2, bt: .2 }],
    ['PLAID', 'BROKEN ROOMS', { xp: .2, rn: .12, ir: 'SALON', im: .25 }],
    ['HUDMO', 'BIG BEAT BUTTER', { b: VARI, dr: .25, pn: .3 }],
  ],
  // ------------------------------------------------------------------ the FX machines: effects on another track's sound
  // (SRC and the side shown stay the track's; at unity gain, VOL 0.8; D at the top: HOLD, the effect always open)
  // VERB (after the Erbe-Verb, eight lines, in stereo): b SIZE (small: metal), c DCAY (a gain a pass: 80 % sustains, 125 %
  // at the top saturates), d ABSR, T TILT, e MIX; xf BASS, xg SHFT, xa SPED, xb DPTH, xc PRE, xd SHAP (sine .. random
  // lengths), xe ACT (IN, FRZ, REV)
  'VERB': [
    ['BOC', 'MUSIC HAS THE RIGHT ROOM', { b: .62, c: .5, d: .5, T: .42, e: .28, xa: .3, xb: .3, xc: .06, xf: bass(1.8) }],
    ['AE', 'ANTIROOM', { b: .35, c: .62, d: .2, T: .6, e: .32, xa: .55, xb: .2 }],
    ['AE', 'METAL CUPBOARD', { b: .22, c: .74, d: .12, T: .6, e: .4, xa: .2, xb: .05 }],
    ['NOTO', 'GLASS CELL', { b: .15, c: .72, d: .05, T: .75, e: .35, xa: .9, xb: .2 }],
    ['SEEFEEL', 'QUIQUE HALL', { b: .85, c: .58, d: .45, T: .45, e: .4, xa: .25, xb: .35, xc: .15, xd: .3, xf: bass(2) }],
    ['GAS', 'POP FOREST', { b: .92, c: .7, d: .6, T: .38, e: .45, xa: .15, xb: .4, xc: .1, xd: .6, xf: bass(2.5) }],
    ['OPN', 'ECCO INFINITE', { b: .78, c: .8, d: .4, e: .5, xa: .2, xb: .3, xf: bass(1.5) }],
    ['OPN', 'STRESSED SPACE', { b: .7, c: .62, d: .3, e: .45, xa: .75, xb: .7, xd: 1 }],
    ['SQPR', 'SATURATED DECAY', { b: .55, c: .95, d: .45, e: .4, xa: .3, xb: .2 }],
    ['DUB', 'SPRING THROW', { D: .45, b: .3, c: .66, d: .3, T: .62, e: .6, xa: .45, xb: .15 }],
    ['BURIAL', 'NIGHT BUS', { b: .75, c: .6, d: .7, T: .3, e: .38, xa: .4, xb: .45, xc: .2, xd: .5, xf: bass(2.2) }],
    ['AFX', 'FREEZE PEDAL', { b: .65, c: .6, d: .35, e: .45, xa: .3, xb: .25, xe: act(1, 3), xf: bass(1.6) }],
    ['AFX', 'BACKWARDS ROOM', { D: .6, b: .65, c: .6, d: .3, e: .5, xc: .4, xe: act(2, 3) }],
    ['BRDCST', 'TENDER PLATE', { b: .45, c: .55, d: .15, T: .65, e: .32, xa: .5, xb: .15, xc: .03 }],
    ['CLARK', 'BODY RIDDLE BLOOM', { b: .95, c: .66, d: .3, T: .55, e: .55, xa: .7, xb: .7, xc: .3, xf: bass(2) }],
    ['BIBIO', 'AMBIVALENT ROOM', { b: .5, c: .45, d: .55, T: .4, e: .22, xa: .25, xb: .2, xf: bass(1.4) }],
    ['GAS', 'CATHEDRAL SHIMMER', { b: .88, c: .7, d: .45, T: .45, e: .45, xa: .2, xb: .25, xc: .08, xf: bass(1.8), xg: shf('+12') }],
    ['BOC', 'FIFTHS IN THE FOG', { b: .8, c: .66, d: .55, T: .38, e: .4, xa: .3, xb: .3, xf: bass(2), xg: shf('+7') }],
    ['OPN', 'FALLING SPIRAL', { b: .75, c: .68, d: .3, e: .45, xa: .25, xb: .2, xf: bass(1.2), xg: shf('-12') }],
    ['SEEFEEL', 'WIDE DETUNE', { b: .7, c: .6, d: .4, T: .48, e: .4, xa: .35, xb: .25, xf: bass(1.6), xg: shf('+10c') }],
    ['AFX', 'BIG BOTTOM', { b: .9, c: .62, d: .5, T: .4, e: .4, xa: .2, xb: .2, xc: .05, xf: bass(4) }],
  ],
  // ECHO: b ZONE, c TIME, d REPT (above 100 % it runs away), T COLR, e MIX; xa µ, xb HALO, xc SKEW, xd SYNC, xe ACT (IN, HOLD, FLIP)
  'ECHO': [
    ['DUB', 'BLACK ARK', { T: .25, b: zn('ECHO'), c: .35, d: .6, e: .4, xa: .25, xb: .15, xc: .25, xd: SYNC }],
    ['BASIC', 'CHORD THROW', { D: .4, T: .35, b: zn('ECHO'), c: .33, d: .72, e: .55, xa: .15, xb: .45, xc: .3, xd: SYNC, xf: .35, xg: .3 }],
    ['SQPR', 'SLAPBACK', { T: .55, b: zn('SLAP'), c: .45, d: .15, e: .3, xd: FREE }],
    ['AE', 'KARPLUS COMB', { T: .6, b: zn('KARP'), c: .5, d: .85, e: .5, xa: .05, xd: FREE }],
    ['BOC', 'WOW ECHO', { T: .3, b: zn('ECHO'), c: .4, d: .45, e: .35, xa: .6, xb: .25, xd: FREE }],
    ['AFX', 'RUNAWAY', { T: .2, b: zn('ECHO'), c: .3, d: .95, e: .45, xa: .2, xb: .3, xd: SYNC }],
    ['OPN', 'LONG MEMORY', { T: .4, b: zn('LONG'), c: .3, d: .55, e: .4, xb: .6, xd: FREE }],
    ['FLYLO', 'HOLD THE LOOP', { T: .45, b: zn('ECHO'), c: .45, d: .5, e: .45, xb: .3, xd: SYNC, xe: act(1, 3) }],
    ['CLARK', 'FLIP TAPE', { D: .5, T: .4, b: zn('ECHO'), c: .4, d: .55, e: .5, xb: .2, xd: SYNC, xe: act(2, 3) }],
    ['SEEFEEL', 'HALO CLOUD', { T: .45, b: zn('ECHO'), c: .5, d: .7, e: .45, xa: .35, xb: .9, xc: .4, xd: FREE }],
    ['PLAID', 'PING PONG', { T: .58, b: zn('ECHO'), c: .3, d: .5, e: .35, xd: SYNC, xf: 1 }],
    ['PLAID', 'SKEWED PING', { T: .58, b: zn('ECHO'), c: .3, d: .5, e: .35, xc: .7, xd: SYNC, xf: .5 }],
    ['BRDCST', 'DUCKING ECHO', { T: .45, b: zn('ECHO'), c: .4, d: .55, e: .5, xa: .1, xb: .2, xd: SYNC, xf: .3, xg: .8 }],
    ['NOTO', 'THIN LINE', { T: .9, b: zn('SLAP'), c: .7, d: .6, e: .35, xd: SYNC }],
  ],
  // BEND: b TIME (a division, or free), c RPTS, d BRK, T BEND, e MIX; xa CRPT, xb CMOD, xc SPRD, xd SYNC, xe ACT (IN, HOLD)
  'BEND': [
    ['AFX', 'DRILL REPEATS', { b: bdv('1/16'), c: rpt('4'), d: .2, xc: .3 }],
    ['SQPR', 'BREAKS EVERYWHERE', { b: bdv('1/8'), c: rpt('OFF'), d: .6, xc: .4 }],
    ['AE', 'GLITCH BENDS', { b: bdv('1/16'), c: rpt('3'), d: .35, T: .56, xa: .25, xb: cmd('DSTR'), xc: .5 }],
    ['VSNARE', 'STUTTER MACHINE', { b: bdv('1/8'), c: rpt('8'), d: .15, xc: .2 }],
    ['PREFUSE', 'CUT-UP', { b: bdv('1/16'), c: rpt('OFF'), d: .45, xa: .15, xb: cmd('DROP'), xc: .3 }],
    ['CLARK', 'PITCH LADDER', { b: bdv('1/4'), c: rpt('6'), d: .1, T: st(6) }],
    ['IKEDA', 'DECIMATED', { b: bdv('1/32'), c: rpt('2'), d: .25, xa: .55, xb: cmd('DECIM') }],
    ['OPN', 'HOLD THE LAST', { b: bdv('1/4'), c: rpt('OFF'), d: 0, xe: act(1, 2) }],
    ['BOC', 'SLOW STOPS', { b: bdv('1/2'), c: rpt('OFF'), d: .5, T: .47, xc: .25 }],
    ['HUDMO', 'ROLL UP', { b: bdv('1/8'), c: rpt('16'), d: .1, T: .53 }],
    ['AFX', 'FREE SPLICE', { b: .4, c: rpt('3'), d: .4, xc: .6, xd: FREE }],
    ['NOTO', 'DROPOUT GRID', { b: bdv('1/16'), c: rpt('OFF'), d: .3, xa: .5, xb: cmd('DROP') }],
  ],
  // TAPE: b WOW, c FLUT, d SAT, T TONE, e MIX; xa GEN, xb FAIL, xc HISS, xd MACH, xe ACT (IN, STOP)
  'TAPE': [
    ['BOC', 'GEOGADDI CASSETTE', { b: .45, c: .25, d: .35, T: .42, xa: .35, xb: .08, xc: .25, xd: tm('CASS') }],
    ['DISINT', 'DISINTEGRATION', { b: .3, c: .15, d: .3, T: .38, xa: .7, xb: .55, xc: .3, xd: tm('REEL'), xf: .4 }],
    ['CAT', 'HAUNTED BALLROOM', { b: .35, c: .2, d: .25, T: .3, xa: .6, xb: .15, xc: .45, xd: tm('MICRO') }],
    ['BIBIO', 'FOUR-TRACK', { b: .2, c: .15, d: .5, T: .48, xa: .15, xc: .2, xd: tm('CASS') }],
    ['OPN', 'VHS MEMORY', { b: .4, c: .35, d: .3, T: .4, xa: .45, xb: .1, xc: .3, xd: tm('VHS'), xf: .5 }],
    ['AFX', 'TAPE STOP', { D: .6, b: .1, c: .1, d: .4, xa: .1, xc: .1, xd: tm('REEL'), xe: act(1, 2) }],
    ['BURIAL', 'CRACKLE BUS', { b: .25, c: .2, d: .45, T: .35, xa: .4, xb: .3, xc: .5, xd: tm('CASS') }],
    ['BRDCST', 'REEL TO REEL', { b: .1, c: .08, d: .55, T: .55, xa: .05, xc: .08, xd: tm('REEL') }],
    ['SEEFEEL', 'SEASICK', { b: .8, c: .3, d: .25, xa: .25, xc: .2, xd: tm('CASS'), xf: .8 }],
    ['AE', 'EATEN TAPE', { b: .3, c: .6, d: .6, xa: .5, xb: .8, xc: .3, xd: tm('MICRO') }],
    ['GAS', 'GRAMOPHONE', { b: .15, c: .2, d: .4, T: .36, xa: .55, xc: .6, xd: tm('MICRO') }],
    ['FLYLO', 'WONKY DECK', { b: .6, c: .5, d: .45, T: .45, xa: .25, xb: .12, xd: tm('CASS') }],
  ],
  // CODEC: b LOSS, c BAND, d SPED, T FILT, e MIX; xa JITR, xb PKTS, xc MODE (STD, INV), xd RESO, xe ACT (IN, BRST)
  'CODEC': [
    ['AE', 'CONFIELD STREAM', { b: .55, c: .6, d: .6, xa: .3, xb: .12 }],
    ['IKEDA', 'DATAPLEX', { b: .75, c: .5, d: .85, xa: .6, xb: .2, xd: .2, xf: .3 }],
    ['NOTO', 'LOW BITRATE', { b: .5, c: .75, d: .3, xa: .1, xb: .05, xf: .5 }],
    ['OPN', 'REPLICA STREAM', { b: .65, c: .45, d: .4, xa: .2, xb: .3, xd: .3 }],
    ['AFX', 'PACKET BURST', { D: .3, b: .4, c: .7, d: .5, xb: .1, xe: act(1, 2) }],
    ['CLARK', 'INVERSE CODEC', { b: .6, c: .8, d: .5, xc: INV, xd: .4 }],
    ['AE', 'GANTZ STREAM', { b: .85, c: .35, d: .95, xa: .8, xb: .35 }],
    ['BURIAL', 'PHONE LINE', { b: .5, c: .15, d: .35, T: .62, xa: .15, xb: .08, xf: 1 }],
    ['PNSC', 'RESONANT LOSS', { b: .7, c: .4, d: .2, xd: .7 }],
    ['SEEFEEL', 'SMEARED', { b: .6, c: .6, d: 0, xa: .4 }],
    ['SQPR', 'BROKEN STREAM', { b: .45, c: .6, d: .7, xb: .5 }],
    ['BRDCST', 'AM RADIO', { b: .4, c: .2, d: .3, T: .4, xa: .1, xd: .15, xf: 1 }],
  ],
  // FUZZ (v68): b FUZZ (v73: DRIV), zc the circuit (v73: cir()), c BIAS (0.5: 9 V; left the battery sags; right hotter), d SUST, T TONE, e MIX; xa LOW, xb MID,
  // xc HIGH (each band's drive, 0: CLEAN), xd XLO, xf XHI, xg GATE, xe ACT (IN, STRV: a trig drains the battery for D)
  'FUZZ': [
    ['FBTNS', 'SWEET LOVE DRONE', { b: .85, c: .5, d: .85, T: .45, xa: .7, xb: .8, xc: .62, xd: .5, xf: .45 }],
    ['FBTNS', 'RIBS OUT', { b: .95, c: .3, d: .45, T: .55, xa: .75, xb: .85, xc: .7, xg: .45 }],
    ['FBTNS', 'BRIGHT TOMORROW', { b: .7, c: .55, d: .6, T: .64, xa: .35, xb: .7, xc: .88, xf: .35 }],
    ['FBTNS', 'KARAOKE SCREAM', { b: 1, c: .42, d: .35, T: .6, xa: 0, xb: .92, xc: .8, xd: .7, xf: .5, xg: .3 }],
    ['FBTNS', 'CASIO WALL', { b: .8, c: .5, d: 1, T: .5, xa: .6, xb: .75, xc: .75, xd: .45, xf: .4 }],
    ['FBTNS', 'TRIBAL TOMS', { b: .8, c: .22, d: .2, T: .45, xa: .65, xb: .7, xc: .5, xg: .55 }],
    ['FBTNS', 'DYING BATTERY', { b: .75, c: .04, d: .4, T: .5, xa: .7, xb: .7, xc: .6, xg: .25 }],
    ['FBTNS', 'STARVED HITS', { D: .45, b: .85, c: .55, d: .3, xa: .7, xb: .8, xc: .7, xe: act(1, 2) }],
    ['FBTNS', 'SUB INTACT', { b: .9, c: .5, d: .5, xa: 0, xb: .85, xc: .85, xd: .65 }],
    ['FBTNS', 'OLYMPIAN SWELL', { b: .6, c: .62, d: .7, T: .42, xa: .55, xb: .65, xc: .55, e: .75 }],
    ['FBTNS', 'WOOLLY HOT', { b: .6, c: .95, d: .5, T: .35, xa: .6, xb: .6, xc: .5 }],
    ['FBTNS', 'SURF SOLAR FIZZ', { b: .65, c: .5, d: .55, T: .7, xa: 0, xb: .4, xc: 1, xf: .2 }],
    // v71: the octaver (zs1 one octave down, zs2 two; zgr PURE .. GRIT; zpt: ix(1, 2) into the LOW band's circuit)
    ['FBTNS', 'THORAX', { b: .75, c: .5, d: .6, T: .45, xa: .6, xb: .75, xc: .6, zs1: .7, zs2: .25 }],
    ['FBTNS', 'CEILING DROP', { b: .8, c: .45, d: .7, T: .4, xa: .5, xb: .8, xc: .7, xd: .4, zs1: .4, zs2: .65 }],
    ['FBTNS', 'OCTAVE GRIND', { b: .9, c: .5, d: .5, xa: .75, xb: .8, xc: .6, zs1: .8, zgr: .7, zpt: ix(1, 2) }],
    ['FBTNS', 'SUB WALL', { b: .7, c: .55, d: 1, T: .5, xa: .65, xb: .75, xc: .7, xd: .45, zs1: .55, zs2: .5, zgr: .25 }],
    // v73: the circuits (zc), b DRIV (low: subtle; high: grunge)
    ['FBTNS', 'DIRTY BOOST', { zc: cir('FUZZ'), b: .2, c: .5, d: .1 }],
    ['BOC', 'WARM DESK', { zc: cir('PRE'), b: .25, c: .58, d: .1, T: .45 }],
    ['BURIAL', 'TAPE GLUE', { zc: cir('SAT'), b: .18, c: .5, T: .42, xc: .55 }],
    ['BASIC', 'DUB SATURATION', { zc: cir('SAT'), b: .45, c: .55, d: .3, T: .4 }],
    ['IKEDA', 'GLASS EXCITE', { zc: cir('XCIT'), b: .55, c: .5, T: .52 }],
    ['CLARK', 'MID CRUNCH', { zc: cir('MID'), b: .6, c: .55, d: .2, T: .5 }],
    ['AE', 'TRANSISTOR SPIT', { zc: cir('XSTR'), b: .75, c: .4, d: .2, xg: .3 }],
    ['PNSC', 'HIGH GAIN WALL', { zc: cir('GAIN'), b: .8, c: .5, d: .5, T: .45, xg: .35 }],
    ['SQPR', 'BENT TOY', { zc: cir('BENT'), b: .55, c: .45, T: .55 }],
    ['MCRPT', 'BENT STUTTER', { zc: cir('BENT'), b: .9, c: .35, d: .3, xg: .25 }],
  ],
  // ------------------------------------------------------------------ STRUM: strummed chords, plucked strings
  // a CHRD (ch) · b PROG (pg) · c SPRD · d STRM (0 a block .. 1 a slow roll) · e DIR (dir) · D the ring (HOLD at the top)
  // md MODE · iv INV · harm (hm) · humn · accn (0.5 even) · ring · wd WIDE · matl (mat) · pick · brit · body · mute · dbl · stif
  'STRUM': [
    ['BIBIO', 'FIRE ANT NYLON', { D: .5, a: ch('TRI'), b: pg('CANON'), c: .3, d: .3, e: dir('D/U'), harm: hm('PURE'), humn: .35, ring: .4, matl: mat('NYLON'), pick: .45, brit: .5, body: .75, rv: .2 }],
    ['BIBIO', 'SUMMER STRUM', { D: .55, a: ch('ADD9'), b: pg('AXIS'), c: .35, d: .26, e: dir('D/U'), harm: hm('PURE'), humn: .4, ring: .45, matl: mat('STEEL'), pick: .6, brit: .55, body: .6, wd: .5, rv: .15, sair: .4, shen: .2 }],
    ['BOC', 'DAYVAN TWELVE', { D: .6, a: ch('6/9'), b: pg('VAMP'), c: .4, d: .3, e: dir('D/U'), harm: hm('PURE'), humn: .45, ring: .5, matl: mat('STEEL'), pick: .5, brit: .5, body: .55, dbl: .7, wd: .75, bt: .12, l1: L(1, .004, 'TUNE'), dtun: .3, drop: .2 }],
    ['BOC', 'OLSON HARP', { D: .7, a: ch('9TH'), b: pg('AEON'), c: .45, d: .62, e: dir('DOWN'), harm: hm('LEAD'), humn: .2, ring: .8, matl: mat('HARP'), pick: .35, brit: .6, body: .6, wd: .6, rv: .35 }],
    ['PÄRT', 'SPIEGEL HARP', { D: .72, a: ch('TRI'), b: pg('MIRR'), c: .3, d: .55, e: dir('DOWN'), humn: .1, ring: .9, matl: mat('HARP'), pick: .3, brit: .55, body: .7, wd: .5, rv: .4, shen: .2 }],
    ['PÄRT', 'ALINA STRINGS', { D: .75, a: ch('5TH'), b: pg('TIN↓'), c: .25, d: .5, e: dir('DOWN'), humn: .08, ring: .85, matl: mat('NYLON'), pick: .25, brit: .35, body: .85, rv: .45 }],
    ['AE', 'KOTO GRID', { D: .45, a: ch('QRTL'), b: pg('4THS'), c: .5, d: .2, e: dir('UP'), harm: hm('PURE'), humn: .15, ring: .3, matl: mat('KOTO'), pick: .8, brit: .65, body: .7, stif: .2, rv: .2 }],
    ['AE', 'METAL ROLL', { D: .55, a: ch('CLST'), b: pg('SLIP'), c: .6, d: .45, e: dir('RAND'), harm: hm('FREE'), humn: .3, ring: .6, matl: mat('STEEL'), pick: .7, brit: .7, body: .3, stif: .65, wd: .7, rv: .3 }],
    ['FLYLO', 'WONKY JAZZ', { D: .45, a: ch('9TH'), b: pg('II-V'), c: .4, d: .22, e: dir('D/U'), harm: hm('LEAD'), humn: .5, ring: .35, matl: mat('ELEC'), pick: .4, brit: .45, mute: .25, wd: .4, rv: .15 }],
    ['HUDMO', 'SYNTH PLUCK', { D: .4, a: ch('7TH'), b: pg('EPIC'), c: .45, d: .12, e: dir('DOWN'), harm: hm('LEAD'), humn: .1, ring: .3, matl: mat('SYNTH'), pick: .7, brit: .7, wd: .6, dl: .2, rv: .15 }],
    ['SEEFEEL', 'DUB SKANK', { D: .25, a: ch('TRI'), b: pg('VAMP'), c: .25, d: .08, e: dir('UP'), harm: hm('PURE'), humn: .2, ring: .1, matl: mat('ELEC'), pick: .55, brit: .5, mute: .6, dl: .35, rv: .1 }],
    ['BRDCST', 'HAUNTED NYLON', { D: .55, a: ch('SUS2'), b: pg('DUSK'), c: .35, d: .35, e: dir('D/U'), harm: hm('PURE'), humn: .4, ring: .5, matl: mat('NYLON'), pick: .4, brit: .4, body: .7, bt: .2, sr: .1, rv: .25, dtun: .35, vari: .3, drop: .3 }],
    ['CLARK', 'FOLIA GRIND', { D: .4, a: ch('TRI'), b: pg('FOLIA'), c: .3, d: .18, e: dir('D/U'), harm: hm('PURE'), humn: .3, ring: .3, matl: mat('STEEL'), pick: .75, brit: .6, mute: .2, dr: .3 }],
    ['PLAID', 'MUSIC BOX ARP', { D: .6, a: ch('ADD9'), b: pg('ROYL'), c: .55, d: .7, e: dir('DOWN'), harm: hm('LEAD'), humn: .1, ring: .7, matl: mat('HARP'), pick: .7, brit: .85, body: .4, stif: .3, ir: 'B.MUSIQ', im: .2, shen: .35 }],
    ['OPN', 'REPLICA KOTO', { D: .6, a: ch('SUS4'), b: pg('MEDI'), c: .45, d: .4, e: dir('DOWN'), harm: hm('PURE'), humn: .2, ring: .7, matl: mat('KOTO'), pick: .6, brit: .55, wd: .6, rv: .35 }],
    ['GSFL', 'DARK RIFF', { D: .35, a: ch('5TH'), b: pg('FALL'), c: .2, d: .05, e: dir('DOWN'), harm: hm('PURE'), humn: .15, ring: .2, matl: mat('ELEC'), pick: .8, brit: .4, mute: .5, dr: .4 }],
    ['BJÖRK', 'VESPERTINE HARP', { D: .65, a: ch('7TH'), b: pg('HYMN'), c: .5, d: .5, e: dir('DOWN'), harm: hm('LEAD'), humn: .15, ring: .7, matl: mat('HARP'), pick: .4, brit: .7, body: .6, wd: .6, rv: .4, shen: .3, sair: .4 }],
    ['B12', 'ELECTRO NYLON', { D: .5, a: ch('7TH'), b: pg('TURN'), c: .35, d: .2, e: dir('D/U'), harm: hm('LEAD'), humn: .25, ring: .4, matl: mat('NYLON'), pick: .5, brit: .5, body: .7, dl: .2 }],
    ['AFX', 'AVRIL PEDAL', { D: .6, a: ch('ADD9'), b: pg('PEDL'), c: .4, d: .32, e: dir('D/U'), harm: hm('LEAD'), humn: .3, ring: .5, matl: mat('NYLON'), pick: .4, brit: .45, body: .8, rv: .3 }],
    ['SQPR', 'FUSION RAKE', { D: .45, a: ch('11TH'), b: pg('CADN'), c: .45, d: .16, e: dir('D/U'), harm: hm('LEAD'), humn: .35, ring: .35, matl: mat('ELEC'), pick: .55, brit: .6, mute: .1, rt: 4, rc: 2 }],
  ],
  // ------------------------------------------------------------------ CHOIR: a sung voice, its ribbon, its space
  // a VOWL (vow: MM OO OH AH EH EE) · b GLID · c BRTH · d VIBR · e SING (1 .. 4 singers) · D the fade
  // swel · form (0.5 the alto) · efrt · vrat · part (part) · scop · drft · smix · size · dcay · damp · mod · shim (shim) · pre
  'CHOIR': [
    ['PÄRT', 'TINTINNABULI ALTO', { D: .55, a: vow('OO'), b: .3, c: .2, d: .45, e: .3, swel: .5, efrt: .35, smix: .45, size: .8, dcay: .8, damp: .4 }],
    ['BJÖRK', 'VESPERTINE CHOIR', { D: .5, a: vow('AH'), b: .25, c: .25, d: .5, e: .8, part: part('TRIAD'), efrt: .5, smix: .5, size: .7, dcay: .7, shim: shim('OCT↑') }],
    ['BURIAL', 'SPECTRAL OOH', { D: .6, a: vow('OO'), b: .5, c: .45, d: .3, e: .5, form: .62, scop: .6, drft: .55, smix: .6, size: .75, dcay: .75, bt: .15 }],
    ['BOC', 'TAPE CHOIR VOICE', { D: .55, a: .3, b: .35, c: .3, d: .4, e: .55, drft: .6, smix: .4, sr: .15, l1: L(2, .005, 'TUNE') }],
    ['CAT', 'BALLROOM ECHO', { D: .65, a: vow('AH'), b: .3, c: .3, d: .55, e: 1, smix: .7, size: .9, dcay: .85, damp: .6, bt: .15, lp: 6000 }],
    ['OPN', 'REPLICA CHANT', { D: .5, a: vow('MM'), b: .2, c: .2, d: .25, e: .66, part: part('-OCT'), efrt: .4, smix: .5, size: .7 }],
    ['AFX', 'AVRIL HUM', { D: .45, a: vow('MM'), b: .3, c: .15, d: .35, e: 0, efrt: .3, smix: .35 }],
    ['BRDCST', 'TENDER ALTO', { D: .5, a: .8, b: .3, c: .3, d: .55, e: 0, efrt: .5, smix: .3, size: .5 }],
    ['SEEFEEL', 'SUCK CHOIR', { D: .7, a: vow('AH'), b: .45, c: .35, d: .3, e: .7, part: part('+5TH'), smix: .65, size: .85, dcay: .8, shim: shim('5TH↑') }],
    ['DISINT', 'LOOP CHOIR', { D: .7, a: vow('OH'), b: .35, c: .3, d: .4, e: .9, drft: .85, smix: .7, dcay: .85, sr: .2, bt: .25 }],
    ['GAS', 'POP VOICES', { D: .75, a: vow('OO'), b: .4, c: .25, d: .3, e: .6, part: part('TRIAD'), smix: .75, size: .95, dcay: .85, damp: .55, lp: 5000 }],
    ['BIBIO', 'FOLK HARMONY', { D: .45, a: vow('AH'), b: .25, c: .3, d: .45, e: .35, part: part('+3RD'), efrt: .5, smix: .25, size: .45 }],
    ['AE', 'FORMANT DRIFT', { D: .5, a: .5, b: .35, c: .2, d: .25, e: .2, smix: .4, l1: L(0, .45, 'A', 'TRI') }],
    ['B12', 'ANDROID ALTO', { D: .4, a: vow('EE'), b: .15, c: .05, d: 0, e: 0, efrt: .8, scop: 0, drft: 0, smix: .3 }],
    ['PLAID', 'GLASS SOPRANO', { D: .55, a: vow('EE'), b: .3, c: .2, d: .5, e: .3, form: .8, efrt: .55, smix: .5, size: .7, shim: shim('OCT↑') }],
    ['CLARK', 'DARK CONTRALTO', { D: .55, a: vow('OH'), b: .3, c: .3, d: .45, e: .4, form: .15, efrt: .3, smix: .45, size: .7 }],
    ['BJÖRK', 'GOSPEL STACK', { D: .45, a: vow('AH'), b: .2, c: .25, d: .6, e: 1, part: part('TRIAD'), efrt: .65, vrat: .5, smix: .35 }],
    ['OPN', 'ONDES GLIDE', { D: .55, a: vow('OO'), b: .75, c: .1, d: .55, e: 0, efrt: .25, scop: .5, smix: .5, size: .7, dcay: .7 }],
    ['BURIAL', 'NIGHT BUS SIGH', { D: .5, a: .45, b: .4, c: .7, d: .3, e: .2, scop: .7, smix: .55, size: .65, bt: .15 }],
    ['AFX', 'SELECTED HYMN', { D: .7, a: vow('OH'), b: .35, c: .2, d: .4, e: .6, part: part('-3RD'), smix: .6, dcay: .85, shim: shim('OCT↓') }],
  ],
  // ------------------------------------------------------------------ ALLOY: metallic sounds (an impact, an object, FM, sines, comb, DAC, a space)
  // a SHAP (sh) · b STIF (soft .. as built .. hard) · c MASS (light .. dense) · d SIZE (6 .. 48 modes) · e HIT (edge .. middle) · D the ring
  // IMPACT mlt clk pen (pe) prs (pr) ptm (pt) bnc (bo) org · NOISE noi his ncl nln (nl) fdb dri dac (dc) · FM obj fm rat (ra) idx iat (ia) idc (ic) men (me)
  // COLOR add bnd (bd) cmb ctn (iv) cfb (cf) flg pha · SPACE vrb rom dcy (dy) dmp sft (shf) cut (ct) rte
  'ALLOY': [
    ['SOPHIE', 'SINE DROP KICK', { T: st(-22), D: .55, obj: 0, fm: 1, idx: 0, pen: pe(26), ptm: pt(36), clk: .3, dri: .2, cut: ct(200), vrb: 0, org: .2 }],
    ['SOPHIE', 'STOMP ROOM', { T: st(-5), D: .6, obj: 0, fm: 1, idx: 0, pen: pe(14), ptm: pt(20), his: .6, ncl: .28, nln: nl(120), vrb: .7, rom: .45, dcy: dy(.9), dmp: .6, cut: ct(450), clk: .3, dri: .2 }],
    ['SOPHIE', 'HOT SUB', { T: st(-16), D: .85, obj: 0, fm: 1, idx: .1, rat: ra(1), idc: ic(60), pen: pe(18), ptm: pt(25), dri: .75, clk: .2, vrb: 0 }],
    ['SOPHIE', 'CHROME KICK', { T: st(-15), D: .26, obj: 0, fm: 1, rat: ra(1), idx: .6, idc: ic(50), pen: pe(21), ptm: pt(40), dri: .55, clk: .4, vrb: 0 }],
    ['SOPHIE', 'GATED SNARE', { T: st(7), D: .6, a: sh('PLATE'), d: 0, obj: .3, add: .9, bnd: bd(11), his: .7, ncl: .7, nln: nl(300), pen: pe(5), ptm: pt(8), cut: ct(100), clk: .4, vrb: .3, rom: .25, dcy: dy(.4) }],
    ['SOPHIE', 'SNAP SNARE', { T: st(10), D: .25, a: sh('DISC'), d: .05, obj: .15, add: .7, bnd: bd(15), his: .8, ncl: .72, nln: nl(30), pen: pe(7), ptm: pt(6), clk: .5, dri: .2, vrb: 0 }],
    ['SOPHIE', 'TIN SNARE', { T: st(12), D: .4, a: sh('PLATE'), d: .3, obj: .4, mlt: 1, his: .75, ncl: .75, nln: nl(60), vrb: .7, rom: .08, dcy: dy(.35), dmp: .2, clk: .4 }],
    ['SOPHIE', 'LATEX CLAP', { obj: 0, his: .9, ncl: .72, nln: nl(70), bnc: bo(-.6), vrb: .45, rom: .3, dcy: dy(.3), cut: ct(200), org: .4 }],
    ['SOPHIE', 'ROOM CLAP', { obj: 0, his: .85, ncl: .72, nln: nl(50), bnc: bo(-.8), vrb: .8, rom: .45, dcy: dy(.8), dmp: .35, org: .4 }],
    ['SOPHIE', 'NEEDLE HAT', { T: st(12), D: .3, obj: 0, his: .9, ncl: .74, nln: nl(35), d: 0, add: .5, bnd: bd(17), cut: ct(100), vrb: 0 }],
    ['SOPHIE', 'FM HAT', { T: st(24), D: .2, obj: 0, fm: .8, rat: ra(Math.PI), idx: .8, idc: ic(30), his: .5, ncl: 1, nln: nl(25), dac: dc('12'), cut: ct(50), vrb: 0 }],
    ['SOPHIE', 'OPEN SHIMMER', { T: st(24), D: .45, a: sh('PLATE'), c: .9, d: .8, obj: .4, mlt: 1, his: .7, ncl: .85, nln: nl(130), cut: ct(310), vrb: 0, add: .3, bnd: bd(12.4) }],
    ['SOPHIE', 'RUBBER SQUEAK', { T: st(24), D: .4, a: sh('WIRE'), b: .5, c: .6, d: .15, e: .25, mlt: .4, pen: pe(-24), ptm: pt(45), fdb: .75, noi: .3, nln: nl(120), vrb: 0, at: .45 }],
    ['SOPHIE', 'BALLOON SQUEAK', { T: st(19), D: .45, obj: 0, fm: 1, rat: ra(5), idx: .5, idc: ic(600), men: me(-2.5), ptm: pt(90), pen: pe(-5), iat: ia(60), vrb: 0, at: .35 }],
    ['SOPHIE', 'RISING BOING', { T: st(22), D: .4, a: sh('WIRE'), obj: .8, fm: 1, rat: ra(1), idx: .35, idc: ic(150), pen: pe(-36), ptm: pt(45), cut: ct(165), vrb: 0, at: .4, dri: .5 }],
    ['SOPHIE', 'PURE PING', { T: st(20), D: .36, obj: 0, fm: 1, idx: 0, pen: pe(0), vrb: 0 }],
    ['SOPHIE', 'WHIP CRACK', { obj: 0, his: 1, ncl: .6, nln: nl(150), bnc: bo(-1), vrb: .5, rom: .4, dcy: dy(.5) }],
    ['SOPHIE', 'RUBBER SMACK', { T: st(19), a: sh('DISC'), obj: .25, his: .9, ncl: .78, nln: nl(80), bnc: bo(-.3), vrb: .9, rom: .7, dcy: dy(2.5), dmp: .2 }],
    ['SOPHIE', 'STEEL RIM', { T: st(17), D: .35, a: sh('BAR'), d: .25, mlt: 1, clk: .7, his: .5, ncl: .55, nln: nl(10), vrb: .55, rom: .25, dcy: dy(.5) }],
    ['SOPHIE', 'DROP TOM', { T: st(-4), D: .3, obj: 0, fm: 1, idx: .1, rat: ra(1), pen: pe(26), ptm: pt(10), clk: .3, vrb: 0, dri: .3 }],
    ['SOPHIE', 'SINGING METAL', { T: st(24), D: .85, a: sh('WIRE'), fdb: .95, mlt: .3, org: .5, b: .55, d: .3, at: .4, his: .12, ncl: .5, nln: nl(300) }],
    ['SOPHIE', 'FLANGE CRASH', { T: st(12), D: .92, a: sh('GONG'), b: .6, c: .95, d: 1, e: .2, mlt: .9, his: .8, ncl: .85, nln: nl(1500), cmb: .3, cfb: cf(60), flg: .7, rte: .15, vrb: .4, rom: .6, dcy: dy(2), obj: .35 }],
    ['SOPHIE', 'FALLING IMPACT', { T: st(-19), D: .7, a: sh('PLATE'), obj: .6, fm: .8, idx: .1, pen: pe(24), ptm: pt(150), his: .6, ncl: .5, nln: nl(250), cmb: .5, ctn: iv(24), cfb: cf(85), vrb: .8, rom: .85, dcy: dy(1.5), dri: .4, dmp: .6 }],
    ['SOPHIE', 'CHROME SNAP', { T: st(5), D: .48, a: sh('BAR'), b: .72, c: .88, d: .75, e: .2, mlt: .95, clk: .5, pen: pe(36), ptm: pt(8), fm: .45, rat: ra(Math.SQRT2), idx: .55, idc: ic(40), dac: dc('12') }],
    ['SOPHIE', 'HYPER BELL', { T: st(12), D: .72, a: sh('BELL'), b: .55, c: .9, d: .8, e: .3, mlt: .85, add: .55, bnd: bd(16), fm: .35, rat: ra(Math.PI), idx: .5, idc: ic(300), dac: dc('12') }],
    ['SOPHIE', 'ZING', { T: st(7), D: .55, a: sh('DISC'), b: .65, c: .85, d: .6, e: .25, mlt: .9, pen: pe(12), ptm: pt(3), cmb: .5, ctn: iv(12), cfb: cf(94), flg: .45, rte: .7 }],
    ['PNSC', 'TAM-TAM', { T: st(-12), D: .88, a: sh('GONG'), b: .5, c: .9, d: 1, e: .7, mlt: .3 }],
    ['GAS', 'BOWED PLATE', { T: st(0), D: .99, a: sh('PLATE'), b: .5, c: .9, d: .9, e: .35, noi: 1, nln: 1, mlt: .5, cmb: .25, cfb: cf(70), pha: .3, rte: .15 }],
  ],
  // v66: CHIMERA, the kit's own sounds: a family drawn from at each hit (VARI RND) or kept per step (LOOP); an attack
  // grafted on another's body (gbc / gbs, the seam, the split); played backwards (e: REV)
  'CHIMERA': [
    ['SOPHIE', 'RUBBER KICK', { ...sm('KICK 1 (AFTERPARTY)'), D: 1, c: vr('LOOP', 3) }],
    ['SOPHIE', 'KICK FAMILY', { ...sm('KICK 2 (TAXI)'), D: 1, c: vr('RND', 7) }],
    ['SOPHIE', 'STOMP FLOOR', { ...sm('STOMP (AFTERPARTY)'), D: 1, c: vr('LOOP', 4), flk: .2 }],
    ['SOPHIE', 'KICK ON 808', { ...sm('KICK (PARADISE)'), D: 1, ...bod('808 (QUEEN LIZZY)'), gsm: sea(45), gxf: xfd(20), gsp: spl(220), gbl: blv(-3) }],
    ['SOPHIE', 'CHROME CLAP', { ...sm('CLAP 1 (AFTERPARTY)'), D: 1, ...bod('IMPACT 2 (NO ANGEL)'), gsm: sea(25), gxf: xfd(10), gsp: spl(1500), gbl: blv(-6) }],
    ['SOPHIE', 'BOOM CLAP', { ...sm('CLAP (LIPGLOSS)'), D: 1, ...bod('808 (LIPGLOSS)'), gsm: sea(30), gxf: xfd(16), gsp: spl(300), gbl: blv(-4) }],
    ['SOPHIE', 'CLAP SHUFFLE', { ...sm('CLAP 2 (GOOD GIRLS)'), D: 1, c: vr('RND', 6) }],
    ['SOPHIE', 'GLASS SNARE', { ...sm('SNARE (LIPGLOSS)'), D: 1, ...bod('IMPACT 1 (LIPGLOSS)'), gsm: sea(15), gxf: xfd(8), gsp: spl(2000), gbl: blv(-6) }],
    ['SOPHIE', 'SNARE ACCENTS', { ...sm('SNARE ACCENT 1 (WATERFALL)'), D: 1, c: vr('LOOP', 7) }],
    ['SOPHIE', 'METAL SNAP', { ...sm('SNAP (VROOM VROOM)'), D: 1, ...bod('LIGHTNING IMPACT 1 (CAN YOU HEAR ME)'), gsm: sea(10), gxf: xfd(6), gbl: blv(-8) }],
    ['SOPHIE', 'HAT SHUFFLE', { ...sm('HAT 1 (AFTERPARTY)'), D: 1, c: vr('RND', 6) }],
    ['SOPHIE', 'HAT LOOP', { ...sm('HAT (LIPGLOSS)'), D: 1, c: vr('LOOP', 8) }],
    ['SOPHIE', 'SQUEAK KIT', { ...sm('SQUEAK 1 (DIE 4)'), D: 1, c: vr('RND', 4) }],
    ['SOPHIE', 'SMACK METAL', { ...sm('SMACK 1 (WATERFALL)'), D: 1, ...bod('IMPACT (AFTERPARTY)'), gsm: sea(20), gxf: xfd(10), gsp: spl(1000), gbl: blv(-5) }],
    ['SOPHIE', 'WHIP BODY', { ...sm('WHIP (GOOD GIRLS)'), D: 1, ...bod('IMPACT 3 (LIPGLOSS)'), gsm: sea(18), gxf: xfd(12), gbl: blv(-6) }],
    ['SOPHIE', 'TOM SWAP', { ...sm('TOM (NO ANGEL)'), D: 1, c: vr('RND', 5), fsm: .25, flk: .15 }],
    ['SOPHIE', '808 FAMILY', { ...sm('808 (VROOM VROOM)'), D: .55, c: vr('LOOP', 4), fpt: .75 }],
    ['SOPHIE', 'REVERSE IMPACT', { ...sm('IMPACT 2 (PARADISE)'), D: 1, e: .75 }],
    ['SOPHIE', 'REVERSE CHANT', { ...sm('CHANT 1 (ROLL WITH ME)'), D: 1, e: .75 }],
    ['SOPHIE', 'CHANT CHOPS', { ...sm('CHANT 1 (TAXI)'), D: .7, c: vr('LOOP', 6) }],
  ],
  // ------------------------------------------------------------------ CUBE (v74): dual wavetable voice in a cube of waves
  // (Z: 1 BASIC, 2 HARM, 3 VOWEL, 4 FM, 5 FOLD, 6 BITS, 7 GLASS, 8 DUST; the factory cube: SET on FACT)
  'CUBE': [
    ['AE', 'SYNC ZAP', { D: cdc(.3), a: cx(3), b: cx(1), qsy: co(6), qst: cst(90) }],
    ['AFX', 'WAVE KICK', { T: st(-5), D: cdc(.4), a: cx(1), e: co(3), qst: cst(14), qpe: cpe(36), qpt: cpt(70) }],
    ['GOLDIE', 'REESE CUBE', { T: st(-12), D: cdc(1.2), a: cx(3), b: cx(2), qun: cun(22), qbr: iv(0), qbd: cdt(14), qbl: .8, fq: .52, rs: .2 }],
    ['AE', 'ADDITIVE DROP', { D: cdc(.6), c: cx(2), a: cx(1), b: cx(3), e: co(7), qst: cst(380) }],
    ['OPN', 'VOWEL DRIFT', { T: st(12), D: cdc(3), c: cx(3), a: cx(3), b: cx(5), qun: cun(9), l1: L(1, .35, 'A', 'TRI'), l2: L(0, .25, 'B', 'SIN'), rv: .25 }],
    ['SQPR', 'FM SNARE', { T: st(14), D: cdc(.24), dr: .15, c: cx(8), a: cx(7), d: .75, qbr: iv(19), qbd: cdt(37), qpe: cpe(12), qpt: cpt(30) }],
    ['CLARK', 'FOLD TOM', { T: st(7), D: cdc(.5), c: cx(5), a: cx(5), b: cx(3), qpe: cpe(9), qpt: cpt(140), e: co(-3), qst: cst(120) }],
    ['AE', 'WAVE STEPPER', { T: st(12), D: cdc(.22), c: cx(2), a: cx(4), qsp: cstp('X'), l1: L(9, .7, 'A', 'SAW') }],
    ['BOC', 'DUST PAD', { D: cdc(3.5), c: cx(8), a: cx(3), b: cx(4), qun: cun(18), qbr: iv(12), qbd: cdt(8), qbl: .45, l1: L(0, .2, 'B', 'TRI'), fq: .62, rv: .3, dl: .15 }],
    ['HUDMO', 'RAVE STACK', { T: st(12), D: cdc(1.4), a: cx(7), qun: cun(25), qbr: iv(12), qbd: cdt(-15), qbl: .6, e: co(-2), qst: cst(250) }],
    ['SENNI', 'CHIP GATE', { T: st(24), D: cdc(.5), c: cx(6), a: cx(4), b: cx(2), qbt: cbit('8') }],
    ['PNSC', 'ZAPPER', { T: st(24), D: cdc(.12), a: cx(1), qpe: cpe(48), qpt: cpt(25) }],
    ['SQPR', 'WARP BASS', { D: cdc(.4), c: cx(4), a: cx(2), b: cx(3), qwp: cwp(.6), e: co(2), qst: cst(60) }],
    ['FLYLO', 'FORMANT PLUCK', { T: st(12), D: cdc(.35), c: cx(3), a: cx(5), b: cx(2), e: co(-4), qst: cst(120) }],
    ['IKEDA', 'SYNC HAT', { T: st(24), D: cdc(.18), a: cx(4), b: cx(8), d: .6, qbr: iv(17), qbd: cdt(23), hp: 1800 }],
    ['GSFL', 'FOLD BASS', { D: cdc(.3), c: cx(5), a: cx(4), b: cx(2), e: co(3), qst: cst(60), dr: .3 }],
    ['BJÖRK', 'VOCAL CHOP', { T: st(12), D: cdc(.45), c: cx(3), a: cx(4), b: cx(6), l1: L(7, .5, 'B', 'SH') }],
    ['OPN', 'BANK MORPH', { D: cdc(3.5), c: cx(1.5), a: cx(3), b: cx(2), qun: cun(12), l1: L(0, .12, 'C', 'TRI'), rv: .2 }],
    ['PLAID', 'GLASS BELL', { T: st(12), D: cdc(1.6), c: cx(7), a: cx(4), b: cx(5), qbr: iv(12), qbl: .3, d: .2 }],
    ['AFX', 'FM CLANG', { T: st(5), D: cdc(.7), c: cx(4), a: cx(3), b: cx(6), d: .45, qbr: iv(7), qbd: cdt(-9), qpe: cpe(5), qpt: cpt(40) }],
  ],
  // ------------------------------------------------------------------ WEST (v76): a west coast voice
  // (a: TIMB, b: SYMM, c: MOD, d: RATI wrt(), e: PULS wdv(); a sequence with wseq())
  'WEST': [
    ['SUBOTNICK', 'SILVER BONGO', { D: wdc(.09), a: .22, c: .18, d: wrt('2'), wte: wb(.5) }],
    ['SUBOTNICK', 'SILVER APPLES', { T: st(12), D: wdc(.1), a: .3, c: .2, d: wrt('3'), e: wdv('1/16'), wac: wb(0), wgl: wglid(.012), ...wseq([0, 7, 12, 3, -5], 'PTCH', wst), wsp: .2 }],
    ['CIANI', 'PULSAR RAIN', { T: st(24), D: wdc(.06), a: .55, c: .4, d: wrt('2.76'), e: wdv('1/32'), wbn: wb(.55), wac: wb(.25), wrn: .35, wsp: .3 }],
    ['AE', 'METAL ROLL', { T: st(7), D: wdc(.08), a: .5, c: .55, d: wrt('5.40'), e: wdv('1/32'), wcn: wcnt(6), wac: wb(.6), wsw: .3, wgm: wgm('VCA') }],
    ['AFX', 'FOLD KICK', { T: st(-17), D: wdc(.3), a: .35, c: 0, wte: wb(.9), wgm: wgm('VCA'), wve: .5 }],
    ['BOC', 'WARM PLUCK', { T: st(12), D: wdc(.5), a: .4, c: .12, d: wrt('1'), wfd: wfold('SINE'), wwa: wwm(4000), wte: wb(.5), wdr: .5, wsp: .3, rv: .15 }],
    ['CLARK', 'BELL TREE', { T: st(19), D: wdc(1.4), a: .12, c: .45, d: wrt('5.40'), wte: wb(.2), wme: wb(.5), wsp: .35, wgm: wgm('VCA'), wwa: wwm(5000) }],
    ['OPN', 'SPRING DRONE', { T: st(-12), D: 1, a: .45, b: .62, c: .3, d: wrt('1+√2'), wtm: .3, wky: WFREE, wfq: wfr(.4), wsp: .5, wsd: wsdc(4), wdr: .6 }],
    ['IKEDA', 'SPIKE HAT', { T: st(24), D: wdc(.035), a: .6, c: .75, d: wrt('8.93'), wmw: wwv('SAW'), wgm: wgm('VCA'), hp: 3000 }],
    ['SQPR', 'BOUNCE SNARE', { T: st(10), D: wdc(.06), a: .75, b: .7, c: .5, d: wrt('7'), wmw: wwv('SQR'), wmx: .45, e: wdv('1/32'), wcn: wcnt(5), wbn: wb(.7), wac: wb(.6) }],
    ['CIANI', 'WAVE SEQUENCE', { T: st(5), D: wdc(.18), a: .35, c: .25, d: wrt('1.5'), e: wdv('1/8'), ...wseq([-.6, .5, -.2, .9], 'TIMB'), wsp: .25 }],
    ['AFX', 'ACID WEST', { T: st(-12), D: wdc(.2), a: .3, c: .1, d: wrt('1'), wgm: wgm('RESO'), wre: .82, e: wdv('1/16'), wgl: wglid(.04), ...wseq([0, 12, 0, 7, 10], 'PTCH', wst), wac: wb(0) }],
    ['BOC', 'DETUNED BELL', { T: st(12), D: wdc(1.8), a: .15, c: .35, d: wrt('1'), wfq: wb(.07), wte: wb(.3), wsp: .3, rv: .2 }],
    ['SEEFEEL', 'RING CHIME', { T: st(7), D: wdc(1.8), a: .1, c: 1, d: wrt('1.5'), wmo: WAM, wmw: wwv('TRI'), wsp: .25, wgm: wgm('VCA') }],
    ['PLAID', 'WOOD MARIMBA', { T: st(12), D: wdc(.3), a: .15, c: .2, d: wrt('4'), wte: wb(.3), wsh: .4, wfd: wfold('SINE') }],
    ['SUBOTNICK', 'UNCERTAIN TOUCH', { T: st(12), D: wdc(.07), a: .35, c: .3, d: wrt('√2'), e: wdv('1/16'), wrn: .7, wpr: .7, wji: .25, wac: wb(.1), wsp: .25 }],
    ['CLARK', 'GATED FM BASS', { T: st(-12), D: wdc(.3), a: .32, c: .45, d: wrt('1'), wgm: wgm('VCA'), wme: wb(.6), wte: wb(.4) }],
    ['AE', 'FREE VIBRATO', { T: st(12), D: 1, a: .3, c: .2, wky: WFREE, wfq: wfr(5.5), wsh: .2, wsp: .25, rv: .15 }],
    ['OPN', 'CYCLE PING', { T: st(5), D: wdc(.08), a: .4, c: .3, d: wrt('2.76'), wen: wenv('CYCL'), wsu: wsus(.06), ...wseq([0, 5, 7, -12], 'PTCH', wst), wsp: .35 }],
    ['IKEDA', 'DATA CLICK', { T: st(31), D: wdc(.006), a: 1, c: .8, d: wrt('7'), wgm: wgm('VCA'), e: wdv('1/64'), wcn: wcnt(3), hp: 1500 }],
  ],
  // ------------------------------------------------------------------ STENCIL (v83): a stereo delay of zones
  // b ZONE (szn), c RATE (srate: s; shz: Hz in KARP, COMB; sdv: a division with SYNC), d REPT (srp: %, past 100 % it
  // runs away), T COLR (left dark tape, right bright), e MIX; xa µ, xb HALO, xc SKEW (sskw: a ratio; sskq on SYNC), xd
  // SYNC, xe ACT (IN, HOLD, FLIP, PLUK), xf PING, xg NOIS; SPACE: tsz SIZE, tdy DCAY, tdm DAMP, tdf DRFT, tms µSPD, tdk
  // DUCK, twd WIDE. (SRC stays the track's: NONE plays the loop alone, from its hiss)
  'STENCIL': [
    ['DUB', 'TUBBY SKANK', { T: .3, b: szn('ECHO'), c: sdv('ECHO', '1/8D'), d: srp(72), e: .4, xa: .2, xb: .2, xd: SYNC, xf: .3, tdk: .25 }],
    ['BASIC', 'CHORD SMEAR', { T: .36, b: szn('TAPE'), c: sdv('TAPE', '1/4D'), d: srp(80), e: .45, xa: .15, xb: .6, xc: sskq('×1'), xd: SYNC, xf: .2, tsz: .55, tdy: .6, tdk: .4 }],
    ['AFX', 'RUNAWAY LOOP', { T: .3, b: szn('ECHO'), c: sdv('ECHO', '1/8'), d: srp(110), e: .45, xa: .25, xb: .25, xd: SYNC, xg: .25 }],
    ['AE', 'PLUCKED NOTHING', { D: .5, T: .56, b: szn('KARP'), c: shz('KARP', 110), d: srp(97), e: .6, xd: SYNC, xe: act(3, 4), xg: 0 }],
    ['NOTO', 'COMB RESONATOR', { T: .66, b: szn('COMB'), c: shz('COMB', 55), d: srp(92), e: .45, xa: .05, xd: SYNC, xg: 0 }],
    ['CLARK', 'FLANGE COMB', { T: .55, b: szn('COMB'), c: shz('COMB', 120), d: srp(70), e: .5, xa: .7, xd: FREE, tms: .3 }],
    ['OPN', 'NO-INPUT CHOIR', { T: .26, b: szn('SLAP'), c: srate('SLAP', 0.07), d: srp(118), e: .55, xa: .35, xb: .5, xc: sskw(1.25), xd: FREE, xg: .5, tsz: .45, tdy: .55 }],
    ['ENO', 'LONG SHADOW', { T: .3, b: szn('LONG'), c: srate('LONG', 1.3), d: srp(86), e: .4, xa: .3, xb: .5, xc: sskw(1.25), xd: FREE, tsz: .6, tdy: .6, tdm: .55 }],
    ['FRIPP', 'LOOP STACK', { T: .42, b: szn('HUGE'), c: srate('HUGE', 4.5), d: srp(98), e: .5, xa: .1, xb: .2, xc: sskw(1.08), xd: FREE, xf: .15 }],
    ['DISINT', 'WORN TAPE HOLD', { D: 1, T: .22, b: szn('TAPE'), c: sdv('TAPE', '1/2'), d: srp(75), e: .5, xa: .45, xb: .35, xd: SYNC, xe: act(1, 4), xg: .2, tms: .2 }],
    ['BOC', 'WARPED CASSETTE', { T: .25, b: szn('TAPE'), c: srate('TAPE', 0.55), d: srp(55), e: .4, xa: .6, xb: .2, xd: FREE, tms: .22 }],
    ['PLAID', 'SKEWED PING', { T: .62, b: szn('ECHO'), c: sdv('ECHO', '1/8'), d: srp(55), e: .38, xc: sskq('×3/2'), xd: SYNC, xf: .8 }],
    ['SEEFEEL', 'HALO WASH', { T: .45, b: szn('ECHO'), c: sdv('ECHO', '1/4'), d: srp(78), e: .5, xa: .2, xb: 1, xd: SYNC, tsz: .72, tdy: .75, tdf: .45 }],
    ['FLYLO', 'FLIP THE ROOM', { D: .55, T: .4, b: szn('ECHO'), c: sdv('ECHO', '1/8'), d: srp(60), e: .5, xb: .3, xd: SYNC, xe: act(2, 4) }],
    ['BURIAL', 'VAST CRACKLE', { T: .28, b: szn('VAST'), c: srate('VAST', 2.4), d: srp(70), e: .35, xa: .25, xb: .55, xd: FREE, xg: .35, tdm: .6, tdk: .35 }],
    ['SQPR', 'SLAPBACK', { T: .55, b: szn('SLAP'), c: srate('SLAP', 0.09), d: srp(18), e: .3, xd: FREE, twd: .7 }],
  ],
  // ------------------------------------------------------------------ HOWL (v81): a guitar held up to a screaming amp
  // (T: the root in semitones from E2, before DROP)
  'HOWL': [
    ['SUNN O)))', 'MONOLITH', { hlm: .45, T: st(5), D: hsu('∞'), a: htp('5TH'), b: hvc('DRONE'), c: hdr('-12'), d: .7, e: hmr(4), hex: .6, hdp: .35, hrs: .4, htm: htime(25), hbl: hblm(3), hdf: .2, hdv: .8, hfz: .7, htn: .4, hbs: .9, hmd: .25, hsg: .6, hlv: .7 }],
    ['SUNN O)))', 'ALTAR OF FEEDBACK', { hlm: .85, hfc: .55, hmv: .3, T: st(-2), D: hsu('∞'), a: htp('OCT'), b: hvc('DRONE'), c: hdr('-12S'), d: .82, e: hmr(8), hex: .4, hdp: .3, hrs: .55, htm: htime(40), hbl: hblm(6), hdf: .25, hdv: .9, hfz: .8, htn: .35, hbs: 1, hmd: .2, hsg: .75, hlv: .65, rv: .2 }],
    ['SUNN O)))', 'BLACK ONE', { hcl: hcolr(500), hlm: .7, hns: .3, T: st(1), D: hsu('∞'), a: htp('5+♭6'), b: hvc('DRONE'), c: hdr('-12'), d: .75, e: hmr(6), hex: .55, hdp: .3, hrs: .45, htm: htime(30), hbl: hblm(4), hdf: .3, hdv: .85, hfz: .85, htn: .3, hbs: .95, hmd: .2, hsg: .7, hlv: .65 }],
    ['SUNN O)))', 'PILGRIM CHORD', { T: st(5), D: hsu(12), a: htp('MIN'), b: hvc('BARRE'), c: hdr('-12'), d: .6, e: hmr(3), hex: .7, hdp: .4, hrs: .35, htm: htime(18), hbl: hblm(2), hdf: .15, hdv: .75, hfz: .6, htn: .45, hbs: .85, hmd: .3, hsg: .5, hlv: .7 }],
    ['EARTH', 'EARTH 2 SLAB', { hlm: .25, hfc: .2, T: st(0), D: hsu('∞'), a: htp('OCT'), b: hvc('DRONE'), c: hdr('-12'), d: .48, e: hmr(10), hex: .75, hdp: .45, hrs: .3, htm: htime(60), hbl: hblm(8), hdf: .1, hdv: .55, hfz: .35, htn: .5, hbs: .8, hmd: .35, hsg: .45, hlv: .75, rv: .15 }],
    ['EARTH', 'LONG RESONANCE', { hlm: .3, hfc: .15, T: st(-2), D: hsu(25), a: htp('5TH'), b: hvc('OPEN'), c: hdr('-12'), d: .42, e: hmr(6), hex: .8, hdp: .55, hrs: .65, htm: htime(50), hbl: hblm(5), hdf: .08, hdv: .45, hfz: .2, htn: .55, hbs: .75, hmd: .45, hsg: .3, hlv: .75, rv: .25 }],
    ['EARTH', 'OPEN STRINGS', { T: st(0), D: hsu(20), a: htp('POLY'), c: hdr('-12'), d: .45, e: hmr(1.5), hex: .65, hdp: .5, hrs: .5, htm: htime(35), hbl: hblm(3), hdf: .1, hdv: .5, hfz: .25, htn: .5, hbs: .75, hmd: .4, hsg: .35, hlv: .75, rv: .2 }],
    ['BORIS', 'ABSOLUTEGO', { hmv: .45, hlm: .8, hns: .35, T: st(3), D: hsu('∞'), a: htp('5TH'), b: hvc('DRONE'), c: hdr('-12'), d: .85, e: hmr(2), hex: .7, hdp: .3, hrs: .4, htm: htime(12), hbl: hblm(.8), hdf: .4, hwm: .1, hdv: 1, hfz: 1, htn: .45, hbs: .85, hmd: .3, hsg: .8, hlv: .6 }],
    ['BORIS', 'FLOOD', { hcl: hcolr(1400), hfc: .6, hmv: .35, hlm: .55, T: st(7), D: hsu('∞'), a: htp('MAJ7'), b: hvc('SPRD'), c: hdr('0'), d: .55, e: hmr(12), hex: .45, hdp: .55, hrs: .55, htm: htime(45), hbl: hblm(10), hdf: .2, hwm: .15, hdv: .55, hfz: .3, htn: .55, hbs: .6, hmd: .45, hsg: .4, hlv: .7, rv: .35, dl: .15 }],
    ['BORIS', 'FEEDBACKER', { htm: htime(1.2), hpk: .55, hcl: hcolr(1100), hfc: .65, hns: .45, hmv: .5, hlm: .9, T: st(2), D: hsu('∞'), a: htp('SUS2'), b: hvc('BARRE'), c: hdr('-12'), d: .95, e: hmr(1), hex: .5, hdp: .25, hrs: .35, hbl: hblm(.4), hdf: .35, hwm: .05, hdv: .9, hfz: .9, htn: .55, hbs: .8, hmd: .35, hsg: .7, hlv: .55 }],
    ['NADJA', 'THAUMOGENESIS', { hmv: .4, hlm: .6, T: st(0), D: hsu('∞'), a: htp('MIN9'), b: hvc('CLUST'), c: hdr('-12'), d: .62, e: hmr(9), hex: .4, hdp: .4, hrs: .6, htm: htime(35), hbl: hblm(7), hdf: .35, hwm: .1, hdv: .75, hfz: .65, htn: .4, hbs: .8, hmd: .3, hsg: .6, hlv: .65, rv: .3, ch: .2 }],
    ['NADJA', 'RADIANT FOG', { hns: .4, hmv: .55, hfc: .3, hlm: .5, T: st(-4), D: hsu('∞'), a: htp('MADD9'), b: hvc('SPRD'), c: hdr('-12'), d: .58, e: hmr(14), hex: .35, hdp: .35, hrs: .7, htm: htime(55), hbl: hblm(12), hdf: .4, hwm: .15, hdv: .7, hfz: .7, htn: .35, hbs: .85, hmd: .25, hsg: .55, hlv: .65, rv: .4 }],
    ['NADJA', 'KEY DRONE', { hmv: .35, hlm: .55, T: st(0), D: hsu('∞'), a: htp('KEY'), b: hvc('OPEN'), c: hdr('-12'), d: .6, e: hmr(6), hex: .45, hdp: .4, hrs: .5, htm: htime(30), hbl: hblm(5), hdf: .25, hwm: .05, hdv: .7, hfz: .55, htn: .45, hbs: .8, hmd: .35, hsg: .5, hlv: .7, rv: .25 }],
    ['JESU', 'CONQUEROR', { T: st(-3), D: hsu(15), a: htp('MAJ7'), b: hvc('OPEN'), c: hdr('-12'), d: .5, e: hmr(2.5), hex: .7, hdp: .5, hrs: .45, htm: htime(28), hbl: hblm(2.5), hdf: .12, hdv: .7, hfz: .45, htn: .55, hbs: .75, hmd: .45, hsg: .5, hlv: .7, rv: .25 }],
    ['JESU', 'SILVER', { T: st(0), D: hsu(12), a: htp('ADD9'), b: hvc('HIGH'), c: hdr('0'), d: .45, e: hmr(1.5), hex: .65, hdp: .6, hrs: .5, htm: htime(22), hbl: hblm(2), hdf: .1, hwm: .05, hdv: .6, hfz: .35, htn: .6, hbs: .6, hmd: .5, hsg: .4, hlv: .7, rv: .3, dl: .12 }],
    ['JESU', 'HEART ASCENDING', { hcl: hcolr(1800), hfc: .55, hlm: .5, T: st(5), D: hsu('∞'), a: htp('SUS2'), b: hvc('BARRE'), c: hdr('-12'), d: .55, e: hmr(5), hex: .6, hdp: .45, hrs: .5, htm: htime(32), hbl: hblm(4), hdf: .15, hdv: .75, hfz: .55, htn: .5, hbs: .8, hmd: .4, hsg: .55, hlv: .7, rv: .2 }],
    ['MBV', 'GLIDE GUITAR', { T: st(4), D: hsu(8), a: htp('MAJ'), b: hvc('BARRE'), c: hdr('0'), d: .4, e: hmr(.3), hex: .7, hdp: .55, hrs: .4, htm: htime(20), hbl: hblm(1.5), hdf: .5, hwm: .6, hdv: .65, hfz: .55, htn: .6, hbs: .55, hmd: .5, hsg: .45, hlv: .7, rv: .3 }],
    ['MBV', 'LOVELESS HAZE', { hmv: .6, hlm: .4, T: st(7), D: hsu('∞'), a: htp('MADD9'), b: hvc('SPRD'), c: hdr('0'), d: .5, e: hmr(.6), hex: .55, hdp: .5, hrs: .55, htm: htime(25), hbl: hblm(2.5), hdf: .55, hwm: .45, hdv: .7, hfz: .6, htn: .55, hbs: .55, hmd: .45, hsg: .5, hlv: .65, rv: .35, ch: .25 }],
    ['MBV', 'TREMOLO BEND', { T: st(2), D: hsu(10), a: htp('DOM7'), b: hvc('OPEN'), c: hdr('0'), d: .38, e: hmr(.2), hex: .75, hdp: .6, hrs: .35, htm: htime(15), hbl: hblm(1), hdf: .45, hwm: .8, hdv: .6, hfz: .5, htn: .6, hbs: .5, hmd: .5, hsg: .4, hlv: .7, rv: .25 }],
    ['MBV', 'SOFT CLUSTER', { hlm: .35, T: st(9), D: hsu('∞'), a: htp('MAJ7'), b: hvc('CLUST'), c: hdr('0'), d: .46, e: hmr(1), hex: .5, hdp: .55, hrs: .6, htm: htime(30), hbl: hblm(3), hdf: .6, hwm: .3, hdv: .55, hfz: .45, htn: .6, hbs: .5, hmd: .55, hsg: .4, hlv: .7, rv: .4 }],
    // v83: CAB (the amp from dark to crisp, its bass defined) and BOW (a magnetic bow over the strings)
    ['BORIS', 'CRISP STACK', { hgt: .45, hbt: .6, hps: .72, hcb: .85, hsz: .6, hlm: .6, T: st(2), D: hsu('∞'), a: htp('5TH'), b: hvc('BARRE'), c: hdr('-12'), d: .62, e: hmr(1.5), hex: .65, hdp: .45, hrs: .35, htm: htime(14), hbl: hblm(1), hdf: .2, hdv: .8, hfz: .55, htn: .6, hbs: .55, hmd: .55, hsg: .45, hlv: .65 }],
    ['SUNN O)))', 'DARK CABINET', { hgt: .2, hps: .3, hdh: .65, hcb: .12, hsz: 1, hmc: .3, hlm: .55, T: st(3), D: hsu('∞'), a: htp('5TH'), b: hvc('DRONE'), c: hdr('-12'), d: .72, e: hmr(5), hex: .55, hdp: .3, hrs: .4, htm: htime(30), hbl: hblm(4), hdf: .2, hdv: .85, hfz: .8, htn: .35, hbs: .85, hmd: .3, hsg: .6, hlv: .65 }],
    ['LANOIS', 'MAGNETIC SWELL', { hbw: .55, hsl: hswl(1.2), hst: hstr('1'), hbm: hbmd('FUND'), hbo: .4, hwo: .25, hcb: .4, hmc: .35, T: st(0), D: hsu('∞'), a: htp('ADD9'), b: hvc('OPEN'), c: hdr('0'), d: .2, e: hmr(2), hex: .45, hdp: .55, hrs: .45, htm: htime(25), hbl: hblm(2), hdf: .1, hwm: .05, hdv: .3, hfz: .1, htn: .55, hbs: .6, hmd: .5, hsg: .3, hlv: .75, rv: .35 }],
    ['LANOIS', 'TWELFTH ABOVE', { hbw: .5, hsl: hswl(2), hst: hstr('1'), hbm: hbmd('12TH'), hbo: .7, hwo: .2, hcb: .55, T: st(-3), D: hsu('∞'), a: htp('MAJ7'), b: hvc('BARRE'), c: hdr('0'), d: .25, e: hmr(3), hex: .4, hdp: .55, hrs: .5, htm: htime(22), hbl: hblm(2.5), hdf: .12, hwm: .05, hdv: .35, hfz: .15, htn: .6, hbs: .55, hmd: .5, hsg: .3, hlv: .75, rv: .4 }],
    ['NADJA', 'BOWED OCTAVE', { hbw: .65, hsl: hswl(0.8), hst: hstr('2'), hbm: hbmd('OCT'), hbo: .55, hwo: .35, hhn: .2, hlm: .5, T: st(0), D: hsu('∞'), a: htp('MIN9'), b: hvc('CLUST'), c: hdr('-12'), d: .5, e: hmr(9), hex: .3, hdp: .4, hrs: .55, htm: htime(35), hbl: hblm(6), hdf: .3, hwm: .1, hdv: .7, hfz: .6, htn: .45, hbs: .8, hmd: .45, hsg: .5, hlv: .65 }],
    ['EARTH', 'ONE STRING BOWED', { hbw: .6, hsl: hswl(3), hst: hstr('6'), hbm: hbmd('FUND'), hbo: .2, hwo: .15, hhn: .85, hgt: .2, hdh: .45, hcb: .25, T: st(-2), D: hsu('∞'), a: htp('5TH'), b: hvc('DRONE'), c: hdr('-12'), d: .3, e: hmr(6), hex: .25, hdp: .45, hrs: .3, htm: htime(50), hbl: hblm(5), hdf: .08, hdv: .55, hfz: .3, htn: .45, hbs: .8, hmd: .4, hsg: .4, hlv: .7 }],
  ],
  // ------------------------------------------------------------------ MANTRA (v84): eight voices driving each other
  // (the notes as they sound at TUNE 0: ynote(); HLD1 HLD2 their floors, b c)
  'MANTRA': [
    ['RADIGUE', 'SLOW BEATINGS', { D: yrel(20), a: ytch('ALL'), b: .62, c: .55, d: .22, e: .05,
      yt1: ynote(0, 'A1'), yt2: ynote(1, 'A1', 7), yt3: ynote(2, 'E2'), yt4: ynote(3, 'E2', -5), yt5: ynote(4, 'A2', 3), yt6: ynote(5, 'A2', -4), yt7: ynote(6, 'E3', 4), yt8: ynote(7, 'C#4', -14),
      ysa: .03, ysb: .05, ysc: .08, ysd: .1, ysg: .5, ybs: .35, ydx: .18, yd1: ydt(420), yd2: ydt(610), ym1: .15, ym2: .2, ydf: .35, ytl: .25, yns: .08, yxm: .1 }],
    ['NIBLOCK', 'WALL OF CLOSE TONES', { D: yrel(16), a: ytch('ALL'), b: .7, c: .7, d: .3, e: .35,
      yt1: ynote(0, 'D3', -9), yt2: ynote(1, 'D3', 6), yt3: ynote(2, 'D3', -3), yt4: ynote(3, 'D3', 11), yt5: ynote(4, 'D3', -14), yt6: ynote(5, 'D3', 2), yt7: ynote(6, 'D4', -6), yt8: ynote(7, 'D4', 8),
      ysa: .45, ysb: .5, ysc: .4, ysd: .55, ysg: .75, ybs: .75, ybl: .45, ydx: .25, yxm: .35, ytl: .2 }],
    ['LAMONTE', 'HARMONIC SERIES', { D: yrel(24), a: ytch('ALL'), b: .75, c: .7, d: .15, e: 0,
      yt1: ynote(0, 'A1'), yt2: ynote(1, 'A2'), yt3: ynote(2, 'E3', 2), yt4: ynote(3, 'A3'), yt5: ynote(4, 'C#4', -14), yt6: ynote(5, 'E4', 2), yt7: ynote(6, 'G4', -31), yt8: ynote(7, 'A4'),
      ysa: 0, ysb: .05, ysc: .05, ysd: .08, ytl: 0, ydf: .1, ysg: .3, ybs: .4, ydx: .12, yns: .05, yxm: 0 }],
    ['OLIVEROS', 'DEEP LISTENING', { D: yrel(28), a: ytch('ALL'), b: 0, c: 0, d: .46, e: .1,
      yt1: ynote(0, 'G1'), yt2: ynote(1, 'D2'), yt3: ynote(2, 'G2'), yt4: ynote(3, 'A2'), yt5: ynote(4, 'D3'), yt6: ynote(5, 'G3'), yt7: ynote(6, 'A3'), yt8: ynote(7, 'D4'),
      yd1: ydt(780), yd2: ydt(1100), ym1: .2, ym2: .25, ydx: .45, ysg: .4, ybs: .4, yvb: Y_ON, yvd: .2, rv: .3 }],
    ['CONRAD', 'STRICT RATIOS', { D: yrel(12), a: ytch('1234'), b: .8, c: 0, d: .2, e: .45,
      yt1: ynote(0, 'G1'), yt2: ynote(1, 'G2'), yt3: ynote(2, 'D3', 2), yt4: ynote(3, 'G3'), ysa: .85, ysb: .9, ysg: .55, ybs: .8, yxm: .55, ytl: .15, ydf: .1, ydx: .15 }],
    ['DAVACHI', 'ORGAN UNFOLDING', { D: yrel(18), a: ytch('ALL'), b: .45, c: .35, d: .28, e: .05,
      yt1: ynote(0, 'C2'), yt2: ynote(1, 'G2'), yt3: ynote(2, 'C3'), yt4: ynote(3, 'E3'), yt5: ynote(4, 'G3'), yt6: ynote(5, 'B3'), yt7: ynote(6, 'D4'), yt8: ynote(7, 'G4'),
      ysa: .3, ysb: .35, ysc: .4, ysd: .45, yvb: Y_ON, yvd: .18, ysg: .3, ybs: .3, ydx: .3, yd1: ydt(300), yd2: ydt(450), rv: .25 }],
    ['PNSC', 'RAW OSCILLATORS', { D: yrel(1.2), a: ytch('12'), b: 0, c: 0, d: .18, e: .5,
      yt1: ynote(0, 'A1'), yt2: ynote(1, 'A2'), yt3: ynote(2, 'A3'), yt4: ynote(3, 'E4'), yt5: ynote(4, 'C5'), yt6: ynote(5, 'A4'), yt7: ynote(6, 'E5'), yt8: ynote(7, 'A5'),
      ysa: 1, ysb: 1, ysc: 1, ysd: 1, yfa: Y_FAST, yfb: Y_FAST, yfc: Y_FAST, yfd: Y_FAST, yca: ysrc('FM'), yma: .5, ybs: .6, yxm: .6, ydx: .12, ytl: .1, ydf: 0, yns: .2 }],
    ['PNSC', 'LFO CHOP', { D: yrel(10), a: ytch('1234'), b: .75, c: 0, d: .25, e: .35,
      yca: ysrc('LFO'), ycb: ysrc('LFO'), yma: .95, ymb: .8, yla: ylfo(4), ylb: ylfo(6), ylg: Y_AND,
      yt1: ynote(0, 'E1'), yt2: ynote(1, 'E2'), yt3: ynote(2, 'B2'), yt4: ynote(3, 'E3'), ysa: .7, ysb: .8, ybs: .55, yxm: .4 }],
    ['FBTNS', 'TOY DRONE FUZZ', { D: yrel(14), a: ytch('ALL'), b: .7, c: .65, d: .35, e: .8,
      yt1: ynote(0, 'D2'), yt2: ynote(1, 'A2'), yt3: ynote(2, 'D3'), yt4: ynote(3, 'F3'), yt5: ynote(4, 'A3'), yt6: ynote(5, 'C4'), yt7: ynote(6, 'D4'), yt8: ynote(7, 'A4'),
      ysa: .9, ysb: .9, ysc: .85, ysd: .95, yxm: .85, ybs: .85, ycb: ysrc('FM'), ymb: .35, ytl: .5, yns: .3 }],
    ['SUNN O)))', 'LOW SUPPLY', { D: yrel(30), a: ytch('1234'), b: .85, c: 0, d: .3, e: .55, yp1: ypch(-12),
      yt1: ynote(0, 'A0', 0, -12), yt2: ynote(1, 'A0', 9, -12), yt3: ynote(2, 'E1', 0, -12), yt4: ynote(3, 'A1', -6, -12),
      ysa: .55, ysb: .6, ysg: .95, ybs: .95, yxm: .65, ydf: .3, ydx: .15, yns: .25 }],
    ['BORIS', 'THE DELAY SINGS', { D: yrel(6), a: ytch('34'), b: 0, c: 0, d: .74, e: .45,
      yd1: ydt(260), yd2: ydt(390), ym1: .45, ym2: .55, yds: Y_SELF, ydx: .5, yt3: ynote(2, 'E3'), yt4: ynote(3, 'B3'), ysb: .6, ybs: .6, yxm: .55 }],
    ['AFX', 'CROSS FM ORGANISM', { D: yrel(10), a: ytch('ALL'), b: .6, c: .6, d: .25, e: .3, ylp: Y_1LOOP,
      yca: ysrc('FM'), ycb: ysrc('FM'), ycc: ysrc('FM'), ycd: ysrc('FM'), yma: .55, ymb: .4, ymc: .5, ymd: .35,
      yt1: ynote(0, 'F#1'), yt2: ynote(1, 'C#2'), yt3: ynote(2, 'F#2'), yt4: ynote(3, 'A2'), yt5: ynote(4, 'C#3'), yt6: ynote(5, 'E3'), yt7: ynote(6, 'F#3'), yt8: ynote(7, 'C#4'),
      ysa: .4, ysb: .35, ysc: .3, ysd: .45, ybs: .6, yxm: .3 }],
    ['OPN', 'HYPER SWEEP', { D: yrel(16), a: ytch('ALL'), b: .55, c: .5, d: .3, e: .15,
      yca: ysrc('LFO'), ycb: ysrc('LFO'), ycc: ysrc('LFO'), ycd: ysrc('LFO'), yma: .35, ymb: .3, ymc: .25, ymd: .2, yla: ylfo(0.18), ylb: ylfo(1.3), ylk: Y_ON,
      yvb: Y_ON, yvd: .25, ym1: .35, ym2: .4, ydx: .35,
      yt1: ynote(0, 'B1'), yt2: ynote(1, 'F#2'), yt3: ynote(2, 'B2'), yt4: ynote(3, 'D3'), yt5: ynote(4, 'F#3'), yt6: ynote(5, 'A3'), yt7: ynote(6, 'C#4'), yt8: ynote(7, 'F#4') }],
    ['ENO', 'ECHO TOUCH', { D: yrel(8), a: ytch('1'), b: 0, c: 0, d: .42, e: .05, yvl: .95, yd1: ydt(340), yd2: ydt(510), ym1: .08, ym2: .1, ydx: .42,
      yt1: ynote(0, 'C2'), yt2: ynote(1, 'G2'), yt3: ynote(2, 'C3'), yt4: ynote(3, 'D3'), yt5: ynote(4, 'E3'), yt6: ynote(5, 'G3'), yt7: ynote(6, 'A3'), yt8: ynote(7, 'C4'),
      ysa: .1, ysb: .1, ysc: .12, ysd: .15 }],
    ['BOC', 'DETUNED WARMTH', { D: yrel(14), a: ytch('ALL'), b: .5, c: .45, d: .3, e: .2, ytl: .85, ydf: .65, yvb: Y_ON, yvd: .3,
      ysa: .2, ysb: .2, ysc: .25, ysd: .25, yxm: .25, ym1: .3, ym2: .35, ydx: .3,
      yt1: ynote(0, 'D2'), yt2: ynote(1, 'A2'), yt3: ynote(2, 'D3'), yt4: ynote(3, 'F#3'), yt5: ynote(4, 'A3'), yt6: ynote(5, 'C#4'), yt7: ynote(6, 'E4'), yt8: ynote(7, 'A4') }],
    ['GAS', 'FOREST PULSE', { D: yrel(6), a: ytch('5678'), b: .55, c: 0, d: .38, e: .2, yfc: Y_FAST, yfd: Y_FAST, yd1: ydt(375), yd2: ydt(250), ydx: .35,
      yt1: ynote(0, 'C2'), yt2: ynote(1, 'G2'), yt3: ynote(2, 'C3'), yt4: ynote(3, 'D#3'), yt5: ynote(4, 'G3'), yt6: ynote(5, 'C4'), yt7: ynote(6, 'D4'), yt8: ynote(7, 'G4'),
      ysc: .55, ysd: .6 }],
    ['AE', 'SELF-PLAYING', { D: yrel(12), a: ytch('ALL'), b: .65, c: .6, d: .3, e: .3, yto: Y_ON,
      yca: ysrc('LFO'), ycb: ysrc('LFO'), ycc: ysrc('LFO'), ycd: ysrc('LFO'), yma: .45, ymb: .35, ymc: .4, ymd: .3, ybs: .55,
      yt1: ynote(0, 'G#1'), yt2: ynote(1, 'D#2'), yt3: ynote(2, 'G#2'), yt4: ynote(3, 'B2'), yt5: ynote(4, 'D#3'), yt6: ynote(5, 'F#3'), yt7: ynote(6, 'A#3'), yt8: ynote(7, 'D#4') }],
    ['BURIAL', 'NIGHT HUM', { D: yrel(20), a: ytch('12'), b: .45, c: 0, d: .4, e: .15, yns: .65, ybl: .55, yd1: ydt(900), yd2: ydt(650), ydx: .35,
      yt1: ynote(0, 'C#2'), yt2: ynote(1, 'G#2'), yt3: ynote(2, 'C#3'), yt4: ynote(3, 'E3'), ysa: .15, ysb: .2, ydf: .4 }],
  ],
  // ------------------------------------------------------------------ RADIO (v87): the radio, live, looped, dug into
  // (the receiver itself, RX, stays the track's: a preset is how it is played)
  'RADIO': [
    ['KURZWELLEN', 'BETWEEN STATIONS', { b: rmode('LIVE'), c: rlen('FREE'), e: .35, T: rshf(-50), airb: rband(4500), airc: rcntr(900), airf: .45, airr: rrate(0.25), airh: .3, airk: .2 }],
    ['CAGE', 'TWELVE RADIOS', { b: rmode('LIVE'), c: rlen('1/8'), D: rdec(0.4), e: .1, airb: rband(5000), airc: rcntr(1000), airf: .2, airr: rrate(0.6) }],
    ['CZUKAY', 'RADIO WAVE SURFER', { b: rmode('LOOP'), c: rlen('1 BAR'), d: .25, e: .12, airb: rband(6000), airc: rcntr(1100), aird: .25 }],
    ['CZUKAY', 'PERSIAN LOOP', { b: rmode('LOOP'), c: rlen('2 BAR'), d: 0, e: .08, airb: rband(3500), airc: rcntr(950), airf: .3, airr: rrate(0.15) }],
    ['NUMBERS', 'NUMBERS STATION', { b: rmode('LIVE'), c: rlen('FREE'), d: rsql(-38), e: .25, airb: rband(2800), airc: rcntr(1000), airf: .5, airr: rrate(0.4), airh: .45, airk: .1 }],
    ['NUMBERS', 'SQUELCH GATE', { b: rmode('LIVE'), c: rlen('1/16'), D: rdec(0.15), d: rsql(-30), e: .05, airb: rband(3000), airc: rcntr(1000) }],
    ['SWL', 'DEEP FADING', { b: rmode('LIVE'), c: rlen('FREE'), e: .3, airb: rband(5000), airc: rcntr(900), airf: .8, airr: rrate(0.12), airk: .35 }],
    ['SWL', 'STORM ON 49M', { b: rmode('LIVE'), c: rlen('FREE'), e: .55, airb: rband(4000), airc: rcntr(800), airf: .5, airr: rrate(0.3), airk: .75 }],
    ['SWL', 'SSB DUCK', { b: rmode('LIVE'), c: rlen('FREE'), T: rshf(250), e: .2, airb: rband(2400), airc: rcntr(1500), airf: .25 }],
    ['BOC', 'TAPE OF THE NIGHT', { b: rmode('DIG'), c: rlen('1/2'), d: rdpt(10), T: 0.5 - 1 / 48, e: .2, airb: rband(5000), airc: rcntr(850), aird: .35, airf: .2 }],
    ['DISINT', 'BROADCAST DECAY', { b: rmode('LOOP'), c: rlen('2 BAR'), d: .06, e: .3, airb: rband(3000), airc: rcntr(700), airf: .55, airr: rrate(0.08), aird: .5 }],
    ['CAT', 'BALLROOM SET', { b: rmode('DIG'), c: rlen('1 BAR'), d: rdpt(18), T: 0.5 - 2 / 48, e: .4, airb: rband(3500), airc: rcntr(800), airk: .4, aird: .3 }],
    ['BURIAL', 'PIRATE NIGHT BUS', { b: rmode('DIG'), c: rlen('1/4'), d: rdpt(6), e: .25, airb: rband(4500), airc: rcntr(700), airk: .3 }],
    ['AE', 'DIAL CHOPS', { b: rmode('DIG'), c: rlen('1/16'), d: rdpt(4), D: rdec(0.12), e: .05, aird: .4 }],
    ['AFX', 'TUNED STUTTER', { b: rmode('LOOP'), c: rlen('1/16'), d: .5, T: 0.5 + 12 / 48, e: .05, aird: .2 }],
    ['DUB', 'SOUNDSYSTEM RELAY', { b: rmode('LOOP'), c: rlen('1/2'), d: .2, e: .1, dl: .35, airb: rband(7000), airc: rcntr(600) }],
  ],
  };

  // v78: WEST's FOLD is SINE by default now (EaseLFO's) and its TIMB goes three times as far round the sine: a preset
  // of v77 keeps its folder (CELL unless it said SINE) and, on SINE, its drive (TIMB a third)
  for (const pr of LIST.WEST) { const o = pr[2]; if (!('wfd' in o)) o.wfd = wfold('CELL'); else if (o.wfd < 0.5 && 'a' in o) o.a = o.a / 3; }

  const MACH_IDS = ['BD·VA', 'BD·FM', 'SD·VA', 'SD·FM', 'HH·MT', 'CP·NZ', 'FM·X', 'NOISE', 'IMPLS', 'SY·RAW', 'BYTE',
    'µLOOP', 'BUG', 'CHAOS', 'VOX', 'CZ·PD', 'FRACT', 'ITERA', 'TRIO', 'SINE', 'DATA', 'FM·Z', 'HARD', 'HEX·BD', 'HEX·SD', 'HEX·TM',
    'MACRO', 'RAM', 'HALO', 'BREAK', 'VERB', 'ECHO', 'BEND', 'TAPE', 'CODEC', 'STRUM', 'CHOIR', 'ALLOY', 'CHIMERA', 'FUZZ', 'CUBE', 'WEST', 'WEFT', 'HOWL', 'STENCIL', 'MANTRA', 'RADIO'];   // RAM, WEFT: no factory presets (RAM plays what it records)

  // preset -> { paramIndex: value } over every owned parameter (machine defaults supply TUNE/DEC/A..F gaps, side defaults
  // ALLOY's sides; tone: the measured [HP, LP] in Hz of tools/preset_voicing.js, unless the preset sets its own)
  function resolve(params, machineDefaults, vol, tone, sideDefaults, spread) {
    const out = Object.assign({}, BASE);
    if (spread) out[P.SPRD] = spread;     // (the SYNTH machines a little wide: app.js SPRD_DEF)
    if (machineDefaults) {
      out[P.TUNE] = machineDefaults[0]; out[P.DEC] = machineDefaults[1]; for (let i = 0; i < 6; i++) out[P.A + i] = machineDefaults[2 + i];
      if (machineDefaults.length > 8) for (let i = 0; i < machineDefaults.length - 8; i++) out[P.XA + i] = machineDefaults[8 + i];   // an FX machine: its MORE side (XA..XE, XF, XG)
    }
    if (sideDefaults) sideDefaults.forEach((v, i) => { out[SQ(i)] = v; });
    out[P.VOL] = vol || 0.8;
    if (tone) {
      if (!('hp' in params)) out[P.HPF] = Math.min(1, Math.max(0, TONE.hp[1](tone[0])));
      if (!('lp' in params)) out[P.LPF] = Math.min(1, Math.max(0, TONE.lp[1](tone[1])));
    }
    for (const k in params) {
      const v = params[k];
      if (k in KEYS) out[KEYS[k]] = Math.min(1, Math.max(0, v));
      else if (k === 'ft') out[P.FTYP] = ix(['LP', 'BP', 'HP', 'CB'].indexOf(v), 4);
      else if (k === 'ir') { const i = IRS.indexOf(v); if (i < 0) throw new Error('IR ' + v); out[P.IR] = ix(i, 25); }
      else if (k === 'ix') out[P.IREV] = ix(v ? 1 : 0, 2);
      else if (k in TONE) out[TONE[k][0]] = Math.min(1, Math.max(0, TONE[k][1](v)));
      else if (k === 'e1' || k === 'e2') efVals(v).forEach((x, i) => { out[(k === 'e1' ? P.E1SR : P.E2SR) + i] = Math.min(1, Math.max(0, x)); });
      else if (k === 'rt') out[P.RTRG] = ix(v, 12);
      else if (k === 'rc') out[P.RCNT] = ix(v - 1, 16);
      else if (k === 'l1' || k === 'l2') {
        const [spd, dep, dest, wave] = v, d = DEST.indexOf(dest);
        if (d < 0) throw new Error('LFO dest ' + dest);
        const base = k === 'l1' ? [P.LSPD, P.LDEP, P.LDST, P.LWAV] : [P.L2SP, P.L2DP, P.L2DS, P.L2WV];
        out[base[0]] = ix(spd, 16); out[base[1]] = 0.5 + dep / 2; out[base[2]] = ix(d, DEST.length); out[base[3]] = ix(wave, 16);
      } else throw new Error('preset key ' + k);
    }
    return out;
  }

  return { LIST, MACH_IDS, TAGS, OWNED, resolve, P };
})();
if (typeof module !== 'undefined') module.exports = IMPAKT_PRESETS;
