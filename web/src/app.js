(() => {
'use strict';

// ------------------------------------------------------------------ model constants (mirror core/sk_engine.h)
const NT = 8, NS = 16, NSTEP = 64, NPAT = 128, NP = 240, NG = 182, NM = 47, MS = 64, MS32 = 32, NIR = 24, NORG = 36, RAT_MAX = 16;
// LFO destinations (core LFO_DEST): 24 until v4, then the retrig rate / count / velocity curve, then (v8) PRISM's,
// then (v10) HP, LP and the mid frequency of TONE. The envelope followers use the same list
const NDEST = 38, NDEST_V4 = 24, NDEST_V7 = 27, NDEST_V9 = 35;
// state version
const SV = 26, SV25 = 25, SV24 = 24, SV23 = 23, SV22 = 22, SV21 = 21, SV20 = 20, SV19 = 19, SV18 = 18, SV17 = 17;
const P = { MACH:0, TUNE:1, DEC:2, A:3, B:4, C:5, D:6, E:7, F:8, FTYP:9, FREQ:10, RES:11, FENV:12, FDEC:13,
  DRV:14, BITS:15, SRR:16, VOL:17, PAN:18, DLY:19, REV:20, LSPD:21, LDEP:22, LDST:23, LWAV:24,
  NOTE:25, VEL:26, PROB:27, MICR:28, RTRG:29, RCNT:30, RVEL:31, GATE:32,
  SPC:33, ATK:34, CHOK:35, DUCK:36, L2SP:37, L2DP:38, L2DS:39, L2WV:40,
  IR:41, IMIX:42, IPRE:43, IDCY:44, IGAT:45, ITON:46, IFB:47, IREV:48, PNCH:49,
  // RAM machine, its REC side: source, take length, level, threshold, overdub, monitor, the buffer its PLAY trigs read
  RSRC:50, RLEN:51, RLEV:52, RTHR:53, ROVD:54, RMON:55, RBUF:56,
  // its PRISM side: dry / resynthesis, oscillators, blur, glide, band centre and width (bipolar), speed of the take;
  // then (v8) the wave, DYN, octave, warp, slices, tilt, the scale quantiser
  PMIX:57, PVOI:58, PBLR:59, PGLD:60, PCTR:61, PWID:62, PSPD:63,
  PWAV:64, PDYN:65, POCT:66, PWRP:67, PSLC:68, PTLT:69, PQNT:70,
  // v10: TONE (high-pass, low-pass, low shelf + its frequency, mid bell + its frequency, high shelf + its frequency)
  HPF:71, LPF:72, LOW:73, LOWF:74, MID:75, MIDF:76, HIGH:77, HIGF:78,
  // v10: the envelope followers (source, input gain, band, trigger threshold, attack, decay, depth, destination)
  E1SR:79, E1GN:80, E1BD:81, E1TG:82, E1AT:83, E1DC:84, E1DP:85, E1DS:86,
  E2SR:87, E2GN:88, E2BD:89, E2TG:90, E2AT:91, E2DC:92, E2DP:93, E2DS:94,
  // v13: HALO, its VOICE side (swell, glide, mode, inversion, drift, air, width)
  HSWL:95, HGLD:96, HMOD:97, HINV:98, HDRF:99, HAIR:100, HWID:101,
  // v14: BREAK, its CHOP side (the chances of another slice, a reversed one, a stutter, a pitch jump, a silent slice
  // (v15: was the ghost notes' weight); the seed of the draws; the chance of the same 16th in another slot (v15: was
  // SYNC / VARI, now B)
  BRND:102, BREV:103, BSTU:104, BPJM:105, BSKP:106, BSED:107, BSWP:108,
  // v16: the FX machines' MORE side (what each means: fxMeta; XE is ACT, what a trig does); v17: two more, XF and XG
  XA:109, XB:110, XC:111, XD:112, XE:113, XF:114, XG:115,
  // v58: TRIG, its MORE page: the retrig's curve, the transposition jump and its range
  RCRV:116, JUMP:117, JRNG:118,
  // v59 (projects v19): HALO and STRUM's harmony (FREE, PURE, LEAD); HALO's TONE side (unison, sub, the voices'
  // low-pass and its opening, vibrato, warmth); STRUM's PLAY side (humanise, accent, ring) and STRING side (material,
  // pick, damping, body, mute, doubled courses, stiffness); CHOIR's VOICE side (swell, formant, effort, vibrato rate,
  // parts, scoop, drift) and SPACE side (mix, size, decay, damping, modulation, shimmer, pre-delay)
  HARM:119, HUNI:120, HSUB:121, HCUT:122, HOPN:123, HVIB:124, HWRM:125,
  SHUM:126, SACC:127, SRNG:128, SMAT:129, SPCK:130, SDMP:131, SBDY:132, SMUT:133, SDBL:134, SSTF:135,
  CSWL:136, CFRM:137, CEFF:138, CVRT:139, CPRT:140, CSCP:141, CDRF:142,
  CSPC:143, CSIZ:144, CDCY:145, CDMP:146, CMOD:147, CSHM:148, CPRE:149,
  // v60 (projects v20): HALO's and STRUM's AGE side (cards out of tune, unequal, sheen, dropouts), STRUM's AIR
  GDTN:150, GVAR:151, GSHN:152, GDRP:153, SAIR:154,
  // v61 (projects v21): the side pages of a machine with three of them, shared (what each one is on each machine:
  // SIDE_META; v20 had the MORE page of STRING and MODAL in the first six); since v62 (projects v22) ALLOY's; v63
  // (projects v23): 35 of them, ALLOY's five pages
  S1:155, S2:156, S3:157, S4:158, S5:159, S6:160, S7:161, S8:162, S9:163, S10:164, S11:165, S12:166, S13:167, S14:168,
  S15:169, S16:170, S17:171, S18:172, S19:173, S20:174, S21:175,
  S22:176, S23:177, S24:178, S25:179, S26:180, S27:181, S28:182, S29:183, S30:184, S31:185, S32:186, S33:187, S34:188, S35:189,
  // v64 (projects v24): the trig's pan drawn at each hit (RPAN, the TRIG row's MORE page), the track's spread and the
  // frequency under which it stays centred (SPRD, MONO: TONE's MORE page)
  RPAN:190, SPRD:191, SMON:192,
  // v65 (projects v25): the trig's decay drawn at each hit (RDEC, the TRIG row's MORE page) and the seed of its LOOP's draws;
  // per LFO, SLW (the SLEW wave's glide), LEN (the LOOP wave's draws) and the seed of those (the LFO page's MORE page)
  RDEC:193, RDSD:194, L1SL:195, L1LN:196, L1SD:197, L2SL:198, L2LN:199, L2SD:200,
  // v68 (projects v26): the sends to the CHORUS and the PHASER (the FX page's SEND, with DLY and REV)
  CHO:201, PHS:202,
  // v78: 35 more side places (S36 .. S70, after the rest: a project saved before keeps its numbers; SQ) for WEST's whole
  // panel, and the amplitude envelope's curve (ECRV, AMP: LOG .. LIN .. EXP, EXP at its top as it always was)
  ...Object.fromEntries(Array.from({ length: 35 }, (_, i) => ['S' + (36 + i), 203 + i])), ECRV:238,
  // v81: DRONE (AMP): the track plays on its own while the transport runs; its trigs change it instead of striking it
  DRON:239 };
const G = { BPM:0, SWING:1, DTIM:2, DFB:3, DTON:4, DPNG:5, RSIZ:6, RDMP:7, MDRV:8, MVOL:9,
  STIM:10, SBLR:11, SWRP:12, SREF:13, SATM:14, SFRZ:15, SREV:16, SLVL:17,
  EQLF:18, EQLB:19, EQLA:20, EQHF:21, EQHB:22, EQBW:23, EQHA:24, EQHS:25,     // master: Pultec EQ
  CINP:26, CATK:27, CREL:28, CRAT:29, CDST:30, CDET:31, CMIX:32, COUT:33,     // master: compressor
  DMOD:34, DDRV:35, DBIA:36, DTNE:37, DCHR:38, DMIX:39, DOUT:40, DPOS:41,     // master: drive
  RALG:42, RTIM:43, RPRE:44, RCH1:45, RCH2:46, RLVL:47,                       // reverb (SIZE / DAMP are RSIZ / RDMP)
  DACM:48, DACA:49,                                                           // unused since v37 (the DAC is in the RAM machine)
  UMSK:50, UKCK:51, CALM:52, CSHP:53, CSEL:54, CSPD:55, CFOC:56, CDLT:57,      // MIX: unmasking, resonance calming
  LOOP:58, LLEN:59, LPOS:60,                                                  // sequence looper
  // DICE (core/sk_dice, Marbles): on, the t section, deja vu, the x / y section, then 7 routes x (track, destination, amount)
  DON:61, DRAT:62, DTRN:63, DTMD:64, DTBI:65, DJIT:66, DDV:67, DLEN:68, DDVM:69,
  DSPR:70, DXBI:71, DSTP:72, DXMD:73, DXRN:74, DSCL:75, DXCK:76, DYDV:77, DR0:78,
  DPW:99,                                                                      // DICE: t pulse width (v6)
  // GRAIN reverb algorithm (core/sk_grain, Beads): its knobs, then RAND, MEDIA, the clock on SEED, FREEZE
  GTIM:100, GSIZ:101, GSHP:102, GPIT:103, GDEN:104, GFDB:105, GVRB:106, GRND:107, GMED:108, GSYN:109, GFRZ:110,
  // v7: GRAIN on the master (on, dry / wet), its LFO (rate, wave, phase) and 8 attenuverters; the chain's ON keys; OTT
  GON:111, GMIX:112, GLRT:113, GLWV:114, GLPH:115, GA0:116,
  EQON:124, COON:125, DRON:126,
  OON:127, OTIM:128, OUP:129, ODN:130, OLO:131, OMD:132, OHI:133, OOUT:134, OMIX:135,
  // the remaining ON keys: reverb, delay, DAC (unused since v37), MIX (on by default; appended to v7)
  RVON:136, DLON:137, DCON:138, MXON:139,
  // v11: GLITCH (page GLCH, the end of the master chain)
  XON:140, XRND:141, XTIM:142, XRPT:143, XBND:144, XBRK:145, XCRP:146, XLOS:147, XMIX:148, XCMD:149, XLMD:150, XSPD:151, XHLD:152, XMSK:153,
  XRES:154, XANA:155, XAMD:156,
  // v59.1: what the quantiser holds to: its scale (0), or the chord of a HALO or STRUM track (T1..T8, on a grid of 9)
  QSRC:157,
  // v68 (projects v26): the CHORUS (mode, input, level, on), the PHASER (generator 1's rate and shape, generator 2's, A's
  // depth and feedback, B's, what sweeps B, sync, route, level, on), the DELAY's model, the 1982's bandwidth, the clock's
  // VCO (depth, rate, shape, envelope), HOLD
  CHMD:158, CHIN:159, CHLV:160, CHON:161,
  PRT1:162, PSH1:163, PRT2:164, PSH2:165, PDPA:166, PFBA:167, PDPB:168, PFBB:169, PBSW:170, PSYN:171, PRTE:172, PLVL:173, PHON:174,
  DMDL:175, DBW:176, DVDP:177, DVRT:178, DVSH:179, DVEN:180, DHLD:181 };

const clamp = (x, a, b) => Math.min(b, Math.max(a, x));
const idx = (v, n) => Math.min(n - 1, Math.max(0, Math.floor(v * n)));
const mid = (i, n) => (i + 0.5) / n;
// P_MACH sits on a fixed grid of slots (64 since v9, 32 before), so adding machines never moves saved ones
const mv = m => mid(m, MS);
const mv32 = m => mid(m, MS32);
const mof = v => Math.min(idx(v, MS), NM - 1);

const SPEEDS = ['1/8×', '1/4×', '1/2×', '3/4×', '1×', '3/2×', '2×'];
const RATIO_L = ['¼','½','¾','1','1.25','1.41','1.5','1.73','2','2.5','2.73','3','3.14','3.5','4','4.5','5','6','7','8','9.25','11','13.5','16'];
const LFO_SPD = ['128','64','32','16','12','8','6','4','3','2','3/2','1','3/4','1/2','1/3','1/4'];
// the LFO waves: eight shapes free-running, then the same restarting with each trig (v65: SLEW and LOOP; 12 waves before)
const LFO_WAV = ['SIN','TRI','SAW','SQR','S&H','EXP','SLEW','LOOP','SIN·T','TRI·T','SAW·T','SQR·T','S&H·T','EXP·T','SLEW·T','LOOP·T'];
const NWAV = 16, NSHAPE = 8, W_SLEW = 6, W_LOOP = 7;
// LOOP's lengths (LEN: core/sk_engine.cpp LFO_LENS) and its draws (the engine's loopDraw: the page draws the same loop)
const LFO_LEN = [2, 3, 4, 5, 6, 7, 8, 12, 16, 32], NLEN = 10;
function loopDraw(seed, salt, i) {
  let h = (Math.imul(Math.floor(Math.fround(seed) * 1048576) >>> 0, 0x9E3779B1) + Math.imul(salt, 0x632BE5AB) + Math.imul(i, 0x85EBCA77)) >>> 0;
  h = (h ^ (h >>> 15)) >>> 0; h = Math.imul(h, 0x2C1B3C6D) >>> 0; h = (h ^ (h >>> 12)) >>> 0; h = Math.imul(h, 0x297A2D39) >>> 0; h = (h ^ (h >>> 15)) >>> 0;
  return (h >>> 8) * (2 / 16777216) - 1;
}
// a new seed (2^20 of them, exact in a float)
const newSeed = () => (Math.floor(Math.random() * 1048576) + 0.5) / 1048576;
// DICE's destinations (until v66: the projects' migrations still move them onto their grids)
const DICE_DEST = ['TRIG', 'NOTE', 'VEL', 'PROB', 'GATE', 'TUNE', 'DEC', 'A', 'B', 'C', 'D', 'E', 'F', 'FREQ', 'RESO', 'FENV', 'FDEC', 'DRIV',
  'BITS', 'SRR', 'VOL', 'PAN', 'ATK', 'PNCH', 'DLY', 'REV', 'RTRG', 'RCNT', 'RVEL', 'IR', 'BMIX', 'BDCY', 'BPRE', 'BTON',
  'PMIX', 'WAVE', 'WARP', 'CNTR', 'WDTH', 'SPED', 'BLUR', 'TILT',   // PRISM (v8)
  'HP', 'LP'];                                                      // TONE (v10)
const NDD = DICE_DEST.length, NDD_V7 = 34, NDD_V9 = 42;
const RETRIG = ['OFF','1/8','1/12','1/16','1/24','1/32','1/48','1/64','1/96','1/128','1/192','1/256'];
const DLY_T = ['1/32','1/16T','1/16','1/8T','1/16D','1/8','1/4T','1/8D','1/4','1/4D','1/2'];
// impulse-response bank, from the smallest space to the largest (core/sk_box.cpp)
const IR_NAMES = ['OFF','TOY','TV','CAN','WALKIE','RADIO','MUS.BOX','MEGAPH.','TUBE','BARREL','AMP 8','CARTON',
  'AMP 12','BASS 15','4×12','PIANO','CLOSET','CAR','BOOTH','BATH','LOUNGE','GARAGE','STAIRS','TUNNEL','CHURCH'];
// organic one-shot bank (core/sk_org.cpp), 4 variants each, grouped by family
const ORG_NAMES = ['TWIG','BRANCH','WOOD','BAMBOO','LEAVES','SEEDS','CHIPS','CRACKER','CELERY','SHELL','WALNUT',
  'FIRE','MATCH','GRAVEL','PEBBLES','SAND','SNOW','ICE','GLASS','DROP','BUBBLES','RAIN','FIZZ','CORK',
  'PAPER','BAG','BUB.WRAP','ZIP','VELCRO','CLAP','FINGERS','TONGUE','KNUCKLE','PINGPONG','COINS','CRICKET'];
const ULOOP_SRC = ['FM', 'NOISE', 'ZAP', 'CHORD', ...ORG_NAMES];
const org = name => ORG_NAMES.indexOf(name);
const COMP_RATIOS = ['OFF', '1:1', '2:1', '3:1', '4:1', '6:1', '10:1·OPT', '20:1', 'NUKE', 'BRIT'];
const DRIVE_MODELS = ['OFF', 'TAPE', 'PREAMP', 'TRIODE', 'PENTODE', 'TRANSIST.', 'DIODE', 'BENT'];
const REV_ALGOS = ['ROOM', 'PLATE', 'SPRING', 'SPECTRAL', 'HALL', 'SPACE'];
const REV_SPECTRAL = 3, NREV = REV_ALGOS.length;
// GRAIN (Beads, on the master): media, clock on SEED, buffer length in each media (stereo), grain-rate dividers
const GRAIN_MEDIA = ['DIGI', 'VINT', 'TAPE', 'DIRT'], GRAIN_SYNC = ['FREE', '1/16', '1/4', 'BAR'];
const GRAIN_BUF = [4, 8, 256000 / 24000, 16], GRAIN_DIV = [1, 2, 3, 4, 6, 8, 12, 16];
// its LFO: cycle lengths (core GLFO_STEPS), the 64 waves (lfo_tables.js), the 8 controls it can move
// GLITCH: its grid, corrupt and loss modes
const GL_TIMES = ['1/64', '1/32', '1/16', '1/8', '1/4', '1/2', '1 BAR'], GL_CMODES = ['DECIM', 'DROP', 'DSTR'], GL_LMODES = ['STD', 'INV', 'JIT', 'PKT·L', 'PKT·R'],
  GL_AMODES = ['CASS', 'VHS', 'MICRO', 'TOY', 'WORN'];
const GLFO_RATE_L = ['1/16', '1/8T', '1/8', '1/4T', '1/8D', '1/4', '1/2T', '1/4D', '1/2', '1/2D', '1 BAR', '2 BAR', '4 BAR', '8 BAR', '16 BAR'];
const GRAIN_PS = ['TIME', 'SIZE', 'SHAP', 'PTCH', 'DENS', 'FDBK', 'VERB', 'MIX'];
let LFO_DATA = null;
const lfoData = () => { if (!LFO_DATA) { const b = atob(IMPAKT_LFO.data); LFO_DATA = new Int8Array(b.length); for (let i = 0; i < b.length; i++) LFO_DATA[i] = b.charCodeAt(i) << 24 >> 24; } return LFO_DATA; };
// the master chain and its ON keys (GRAIN and OTT sit between the compressor / drive and the final clipper)
const CHAIN_ON = { EQ: G.EQON, COMP: G.COON, DRV: G.DRON, GRN: G.GON, OTT: G.OON, GLCH: G.XON };
const chainOn = id => state.g[CHAIN_ON[id]] > 0.5;
// every effect page's ON key; a page whose keys are all off is crossed out
const PAGE_SW = { REV: [['REVERB', G.RVON]], DLY: [['DLY', G.DLON]], CHO: [['CHORUS', G.CHON]], PHS: [['PHASER', G.PHON]], EQ: [['EQ', G.EQON]], COMP: [['COMP', G.COON]],
  DRV: [['DRV', G.DRON]], GRN: [['GRAIN', G.GON]], OTT: [['OTT', G.OON]], GLCH: [['GLITCH', G.XON]], MIX: [['MIX', G.MXON]] };
const swOn = g => state.g[g] > 0.5;
const pageBypassed = id => !!PAGE_SW[id] && PAGE_SW[id].every(([, g]) => !swOn(g));
const DAC_MODELS = ['OFF', 'CD-80', 'PCM-14', 'SP-12', 'LO-8'];
const IR_DUR = [0,0.05,0.04,0.35,0.06,0.09,0.2,0.15,0.7,0.9,0.045,0.14,0.05,0.06,0.06,1,0.25,0.2,0.25,0.9,0.8,1,1,1,1];

// formatters
const fU = v => String(Math.round(v * 127));
const fB = v => { const x = Math.round((v - 0.5) * 128); return (x > 0 ? '+' : '') + x; };
const fL = list => v => list[idx(v, list.length)];
const fI = (lo, n) => v => String(lo + idx(v, n));
const fPct = v => Math.round(v * 100) + '%';
const fPan = v => { const x = Math.round((v - 0.5) * 128); return x === 0 ? 'C' : (x < 0 ? 'L' + (-x) : 'R' + x); };
const fSemi = v => { const s = (v - 0.5) * 48; return (s > 0.05 ? '+' : '') + s.toFixed(1); };
const fNote = v => { const n = Math.round((v - 0.5) * 48); return (n > 0 ? '+' : '') + n; };
const fHz = v => { const hz = 20 * Math.pow(2, v * 9.9658); return hz >= 1000 ? (hz / 1000).toFixed(hz >= 10000 ? 1 : 2) + 'k' : Math.round(hz) + ''; };
const fNear = names => v => { const i = Math.round(v * (names.length - 1)); const off = v * (names.length - 1) - i; return names[i] + (Math.abs(off) > 0.08 ? (off > 0 ? '›' : '‹') : ''); };
const fOff = f => v => v < 0.002 ? 'OFF' : f(v);
const fSlw = v => v < 0.005 ? 'STEP' : fPct(v);       // (SLEW: no glide, it steps like S&H)
const fSec = s => s < 1 ? Math.round(s * 1000) + 'ms' : (s < 10 ? s.toFixed(1) : Math.round(s)) + 's';
const fSt = range => v => { const st = Math.round((v - 0.5) * 2 * range); return (st > 0 ? '+' : '') + st + 'st'; };
const fOnOff = v => idx(v, 2) ? 'ON' : 'OFF';
const fSeed = v => { const i = idx(v, 16); return i ? '#' + i : 'RND'; };
const fDb = range => v => { const d = (v - 0.5) * 2 * range; return Math.abs(d) < 0.05 ? '0dB' : (d > 0 ? '+' : '') + d.toFixed(1) + 'dB'; };
const fDbUp = max => v => '+' + (v * max).toFixed(1) + 'dB';
const fTime = s => s < 0.001 ? Math.round(s * 1e6) + 'µs' : s < 0.1 ? (s * 1000).toFixed(s < 0.01 ? 2 : 1) + 'ms' : fSec(s);
const fTen = v => (v * 10).toFixed(1);                       // Pultec-style 0..10 scale
const fInt = v => { const st = idx(v, 49) - 24; return st === 0 ? 'UNI' : (st > 0 ? '+' : '') + st + 'st'; };   // TRIO intervals
// JUMP (v58): -12 .. +12 (semitones; scale degrees on a quantised track), its range 1 .. 24 semitones
const NJUMP = 25, NJRNG = 24;
const jumpOf = v => idx(v, NJUMP) - (NJUMP - 1) / 2;
const fJump = v => { const j = jumpOf(v); return j === 0 ? 'OFF' : (j > 0 ? '+' : '') + j; };
// sequence looper lengths (16ths in the engine), shown as note values
const LOOP_L = ['1/128', '1/64', '1/32', '1/16', '1/8', '3/16', '1/4', '3/8', '1/2', '3/4', '1 BAR', '2 BAR'];
const RING_L = ['OFF', '×½', '×¾', '×1', '×1.5', '×2', '×2.76', '×3', '×3.5', '×4.2', '×5.4', '×6', '×7.1', '×8', '×11', '×16'];   // SINE ring ratios
// RAM machine (core M_RAM): sources, take lengths in steps, play modes, slices, the buffer played
const RAM_M = 27, HALO_M = 28, BREAK_M = 29, STRUM_M = 35, CHOIR_M = 36, ALLOY_M = 37, CHIM_M = 38, FUZZ_M = 39;
const CUBE_M = 40;     // (v74)
const WEST_M = 41;     // (v76)
const WEFT_M = 42;     // (v78: an FX machine, Mutable Instruments' Warps)
const HOWL_M = 43;     // (v81: a guitar held up to a screaming amp, its own chords)
const STENCIL_M = 44;  // (v83: an FX machine, a stereo delay of zones, the Mimeophon's homage)
const MANTRA_M = 45;   // (v84: eight voices on one supply, driving each other)
const RADIO_M = 46;    // (v87: the radio: four receivers on the network, an internet station or a shortwave receiver each)
// v61 (projects v21): STRING, MODAL and the three WT machines are gone, MATTER and LATEX in, the machines renumbered. The
// conversions of older projects run on v20's numbers (M20, mof20), then toV21 moves them onto today's (om21)
const NM20 = 42, M20 = { ULOOP: 12, RAM: 30, HALO: 33, BREAK: 34, STRUM: 40, CHOIR: 41 };
const MACH20 = ['BD·VA', 'BD·FM', 'SD·VA', 'SD·FM', 'HH·MT', 'CP·NZ', 'FM·X', 'NOISE', 'IMPLS', 'MODAL', 'SY·RAW', 'BYTE',
  'µLOOP', 'BUG', 'STRING', 'CHAOS', 'VOX', 'CZ·PD', 'FRACT', 'ITERA', 'TRIO', 'SINE', 'DATA', 'FM·Z', 'HARD', 'HEX·BD', 'HEX·SD', 'HEX·TM', 'WT·DRM',
  'MACRO', 'RAM', 'WT·GRL', 'WT·ZAP', 'HALO', 'BREAK', 'VERB', 'ECHO', 'BEND', 'TAPE', 'CODEC', 'STRUM', 'CHOIR'];
// (core/sk_engine.h oldMachine21: the five gone went onto MATTER or LATEX in v21; since v22 both are ALLOY, where an older
// project's five go straight on)
const GONE21 = { 'MODAL': 'ALLOY', 'STRING': 'ALLOY', 'WT·DRM': 'ALLOY', 'WT·GRL': 'ALLOY', 'WT·ZAP': 'ALLOY' };
// v62 (projects v22): MATTER (37) and LATEX (38), the last two of v21, are gone, ALLOY (37) in their place
// (core/sk_engine.h oldMachine22)
const NM21 = 39, mof21 = v => Math.min(idx(v, MS), NM21 - 1), gone22 = m => m === 37 || m === 38;
const mof20 = v => Math.min(idx(v, MS), NM20 - 1);
// the chord machines (HALO's harmony: its shapes, progressions, modes, HARM)
const chordM = m => m === HALO_M || m === STRUM_M;
// the FX machines (core/sk_fx), 30..34 and (v68) FUZZ, 39: a track that plays another one's sound through its effect (an
// insert: SRC, MIX); fxI: its place in their lists
const FX_M0 = 30, FX_N = 5, fxM = m => (m >= FX_M0 && m < FX_M0 + FX_N) || m === FUZZ_M || m === 42 || m === 44;
const fxI = m => m === FUZZ_M ? FX_N : m === 42 ? FX_N + 1 : m === 44 ? FX_N + 2 : m - FX_M0;     // (42: WEFT, v78; 44: STENCIL, v83)
const FX_SRC = ['NBR', 'SUM', 'T1', 'T2', 'T3', 'T4', 'T5', 'T6', 'T7', 'T8'];
const FX_NAMES = ['VERB', 'ECHO', 'BEND', 'TAPE', 'CODEC', 'FUZZ', 'WEFT', 'STENCIL'];
// (v83: STENCIL's SRC, NONE first: its loop alone, from its hiss)
const STC_SRC = ['NONE', ...FX_SRC];
const STC_ZONES = ['KARP', 'COMB', 'SLAP', 'ECHO', 'TAPE', 'LONG', 'VAST', 'HUGE'];   // (its zones: core/sk_stencil)
// ACT (XE): IN (a trig opens the effect for DEC), or the machine's gesture (a trig fires it for DEC; HOLD: until the next)
const FX_ACTS = [['IN', 'FRZ', 'REV'], ['IN', 'HOLD', 'FLIP'], ['IN', 'HOLD'], ['IN', 'STOP'], ['IN', 'BRST'], ['IN', 'STRV'], ['IN', 'FRZ'], ['IN', 'HOLD', 'FLIP', 'PLUK']];
// v73: FUZZ's circuits (CIRC; core/sk_fuzz)
const FZ_CIRCS = ['FUZZ', 'PRE', 'SAT', 'XCIT', 'MID', 'XSTR', 'GAIN', 'BENT'];
const FX_INFO = 13, FX_XS = [P.XA, P.XB, P.XC, P.XD, P.XE, P.XF, P.XG];
// the tracks an FX track takes (a bit each), as the engine reads SRC: the one before, every one before (but those an
// FX track before it already takes: two in a row are a chain), or one
// (v83: STENCIL's list starts with NONE)
const fxSrcMask = (t, v, taken, m) => { const s = m === STENCIL_M ? idx(v, FX_SRCS_N + 1) - 1 : idx(v, FX_SRCS_N); if (s < 0) return 0; if (s === 0) return t > 0 ? 1 << (t - 1) : 0; if (s === 1) return ((1 << t) - 1) & ~(taken || 0); return s - 2 !== t ? 1 << (s - 2) : 0; };
// every track's (0 when it is not on an FX machine), from the left, as the engine does it each chunk
// v78, WEFT: its carrier (CARR, D: SIN TRI SAW, then T1 .. T8): the track it takes too (never itself)
const fxCarMask = (t, v) => { const c = idx(v, 11) - 3; return c >= 0 && c !== t ? 1 << c : 0; };
function fxMasks() {
  let taken = 0;
  return state.tracks.map((tr, t) => { const mm = mof(tr.p[P.MACH]); if (!fxM(mm)) return 0;
    const cm = mm === WEFT_M ? fxCarMask(t, tr.p[P.D]) : 0, m = fxSrcMask(t, tr.p[P.A], taken, mm) & ~cm; taken |= m | cm; return m; });
}
const FX_SRCS_N = 10;   // (NBR, SUM, T1..T8)
const fxSrcName = mask => {
  const ts = []; for (let t = 0; t < NT; t++) if (mask & (1 << t)) ts.push(t + 1);
  if (!ts.length) return 'NOTHING';
  if (ts.length > 2 && ts[ts.length - 1] - ts[0] === ts.length - 1) return 'T' + ts[0] + '–T' + ts[ts.length - 1];
  return ts.map(n => 'T' + n).join('+');
};
// BREAK (core/sk_break): ten slots of loops the user loads (shared by the BREAK tracks of the project), each read at
// 32 kHz mono, at most 20 s and 8 bars; LEN; the engine's info for the screen (BRK_INFO) and for the slots
// (ui.brks: per slot its state — 0 empty, 1 being read, 2 ready —, tempo, bars, seconds; the waveforms; the names)
const BRK_SLOTS = 10, BRK_RATE = 32000, BRK_CAP_S = 20, BRK_MAXBARS = 8;
const BRK_LENS = ['1/32', '1/16', '1/8', '3/16', '1/4', '1/2', '1 BAR', 'FREE'];
const BRK_MAXLEN = 16 * BRK_MAXBARS, BRK_SEEDS = 33, BRK_HEAD = 16, BRK_INFO = BRK_HEAD + 2 * BRK_MAXLEN;
const fSlice = v => { const i = idx(v, BRK_MAXLEN + 1); return i ? 'SL ' + i : 'STEP'; };
const brkState = s => ui.brks ? ui.brks.info[s * 4] | 0 : 0;
const brkReady = s => brkState(s) === 2;
// the slot a knob on s plays: s, else the next one ready (-1: none)
const brkResolve = s => { for (let k = 0; k < BRK_SLOTS; k++) if (brkReady((s + k) % BRK_SLOTS)) return (s + k) % BRK_SLOTS; return -1; };
const brkBars = s => s >= 0 && ui.brks ? Math.max(1, ui.brks.info[s * 4 + 2] | 0) : 1;
const brkName = s => (ui.brkNames && ui.brkNames[s]) || '';
const fSlot = v => { const s = idx(v, BRK_SLOTS), r = brkResolve(s); return r === s ? String(s + 1) : r < 0 ? (s + 1) + ' —' : (s + 1) + '→' + (r + 1); };
// HALO: CHRD's shapes, PROG's progressions (each trig a chord), MODE's scales (KEY: the quantiser's), WAVE's shapes
const HALO_SHAPES = ['5TH', 'SUS2', 'SUS4', 'TRI', 'ADD9', '7TH', '9TH', '11TH', '6/9', 'QRTL', '5THS', 'CLST'];
// (v59: the classical ones after MEDI — a canon, the pop axis, the fifties, a cadence, a turnaround, ii-V-I, the royal road,
// an epic, a folia, a romanesca, a hymn —, a pedal, the tintinnabuli — the tonic's chord, a melody walking down or up the
// scale over it, or round its third as in a mirror —; WALK last)
const HALO_PROGS = ['OFF', 'AEON', 'DUSK', 'TIDE', 'VAMP', 'DAWN', 'FALL', 'RISE', '4THS', 'AMBR', 'SLIP', 'MEDI',
  'CANON', 'AXIS', '50S', 'CADN', 'TURN', 'II-V', 'ROYL', 'EPIC', 'FOLIA', 'ROMA', 'HYMN', 'PEDL', 'TIN↓', 'TIN↑', 'MIRR', 'WALK'];
const HALO_PROG_LEN = [1, 4, 4, 2, 2, 4, 4, 4, 7, 4, 4, 3, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 8, 8, 18, 0];
const NPROG = HALO_PROGS.length, PROG_WALK = NPROG - 1, NPROG18 = 13;
const HARMS = ['FREE', 'PURE', 'LEAD'];
// STRUM (core/sk_strum): its materials, directions; CHOIR (core/sk_choir): its vowels, parts, the SPACE's shimmer
const STRUM_MATS = ['NYLON', 'STEEL', 'ELEC', 'HARP', 'KOTO', 'SYNTH'], STRUM_DIRS = ['DOWN', 'UP', 'D/U', 'RAND'];
const CHOIR_VOWELS = ['MM', 'OO', 'OH', 'AH', 'EH', 'EE'], CHOIR_PARTS = ['SOLO', '+3RD', '+5TH', 'TRIAD', '-3RD', '-OCT'];
const CHOIR_SHIMS = ['OFF', 'OCT↑', '5TH↑', 'OCT↓'];
const fVowel = v => { const w = clamp(v, 0, 1) * 5, i = Math.round(w); return Math.abs(w - i) < 0.12 ? CHOIR_VOWELS[i] : CHOIR_VOWELS[Math.floor(w)] + '›' + CHOIR_VOWELS[Math.min(5, Math.floor(w) + 1)]; };
const fStrum = v => v < 0.01 ? 'BLOCK' : fSec(0.003 * Math.pow(500, v));
const HALO_MODES = ['KEY', 'AEOL', 'DOR', 'PHRY', 'LYD', 'MIXO', 'ION', 'HARM', 'MPNT', 'WHOL', 'HIRA', 'OCTA'];
const HALO_WAVES = ['SINE', 'SOFT', 'SAW', 'PLS', 'GLAS', 'METL'];
const HALO_SLOTS = 18, HALO_INFO = 12 + 3 * HALO_SLOTS;   // (core/sk_halo.h: the engine's info for the screen)
const fHWave = v => { const w = clamp(v, 0, 1) * 5, i = Math.round(w); return Math.abs(w - i) < 0.1 ? HALO_WAVES[i] : HALO_WAVES[Math.floor(w)] + '›' + HALO_WAVES[Math.min(5, Math.floor(w) + 1)]; };
const RAM_SRC = ['IN', 'MAIN', 'T1', 'T2', 'T3', 'T4', 'T5', 'T6', 'T7', 'T8'];
const RAM_TAKE = [1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64];
const RAM_MODES = ['FWD', 'REV', 'LOOP', 'PING'];
const RAM_SLICES = [1, 2, 4, 8, 16];
const RAM_BUF = ['OWN', 'T1', 'T2', 'T3', 'T4', 'T5', 'T6', 'T7', 'T8'];
const fTake = n => n % 16 === 0 ? (n / 16) + ' BAR' : n + ' ST';
// PRISM (core prismBank): band centre 20 Hz .. 20 kHz; WDTH 12 o'clock = the whole band, to the right a band from 10
// octaves down to 0.1, to the left a notch from 0.1 octave up to 6; speed 1/8 .. 8 (0 = frozen); WARP f' = c (f / c)^e;
// TILT +-4.5 dB per octave; the wave morphs SINE > TRI > SAW > PULSE; slices in steps
const prismHz = v => 20 * Math.pow(2, v * 10);
const prismSpeed = v => v < 0.02 ? 0 : Math.pow(2, (v - 0.5) * 6);
const prismBand = v => v > 0.51 ? 1 : v < 0.49 ? -1 : 0;
const prismHalfOct = v => v > 0.51 ? 0.05 + (1 - v) * 2 * 4.95 : 0.05 + (0.49 - v) * (2.95 / 0.49);   // half the width, octaves
const prismWarp = v => Math.pow(2, (v - 0.5) * 1.4);
const PR_WAVES = ['SINE', 'TRI', 'SAW', 'PULS'], PR_SLICE = ['OFF', '1/32', '1/16', '1/8', '1/4', '1/2', '1 BAR'], PR_OCT = ['-2', '-1', '0', '+1', '+2'];
const fPWave = v => { const x = clamp(v, 0, 1) * 3, a = Math.min(2, Math.floor(x)), f = x - a;
  return f < 0.04 ? PR_WAVES[a] : f > 0.96 ? PR_WAVES[a + 1] : PR_WAVES[a].slice(0, 3) + '›' + PR_WAVES[a + 1].slice(0, 3); };
const fPWidth = v => { const b = prismBand(v); if (!b) return 'FULL'; const o = 2 * prismHalfOct(v); return (b < 0 ? 'NTCH ' : '') + o.toFixed(o < 10 ? 1 : 0) + (b > 0 ? 'oct' : ''); };
const fKHz = hz => hz >= 1000 ? (hz / 1000).toFixed(hz >= 10000 ? 1 : 2) + 'k' : Math.round(hz) + '';

// HEX (core/sk_kit): the noise filter's cutoff, BEND (right of 12 o'clock the hit drops, left it climbs), the tone / noise balance
const fHexF = v => fKHz(150 * Math.pow(93, v));
const fBend = v => { const o = (v - 0.5) * 4; return Math.abs(o) < 0.04 ? 'FLAT' : (o > 0 ? 'DN ' : 'UP ') + Math.abs(o).toFixed(1); };
const fBal = v => v < 0.02 ? 'TONE' : v > 0.98 ? 'NOISE' : 'N ' + Math.round(v * 100) + '%';
// ALLOY (core/sk_alloy): its shapes; STIF the series squeezed (SOFT) or stretched (HARD) around the shape as built (NAT);
// MASS a material's density (light: its high modes die at once; dense: they ring on); SIZE how many modes ring; HIT
// from the edge to the middle; MALT the mallet's contact at the hardest hit; PENV from two octaves under the note to
// four over it; RATI on its detents (the whole numbers, the halves, and the inharmonic ratios of metal: √2, φ, √3, e, π)
const AL_SHAPES = ['WIRE', 'BAR', 'TUBE', 'PLATE', 'DISC', 'BELL', 'GONG', 'BOWL'];
const fStif = v => Math.abs(v - 0.5) < 0.01 ? 'NAT' : (v < 0.5 ? 'SOFT ' : 'HARD ') + Math.round(Math.abs(v - 0.5) * 200);
const fMass = v => (v < 0.2 ? 'RUBR ' : v < 0.4 ? 'WOOD ' : v < 0.6 ? 'ALU ' : v < 0.82 ? 'STEEL ' : 'BRASS ') + Math.round(v * 100);
const fSize = v => Math.floor(6 + 42 * v * v) + ' MODES';
const fHit = v => v < 0.01 ? 'EDGE' : v > 0.99 ? 'MID' : Math.round((0.02 + 0.48 * v) * 200) + '%';
const fMalt = v => fTime((0.00008 + 0.008 * (1 - v) * (1 - v)) * 0.5);
const fPenv = v => Math.abs(v - 0.5) < 0.01 ? 'OFF' : v > 0.5 ? '+' + Math.round((v - 0.5) * 96) + 'st' : '−' + Math.round((0.5 - v) * 72) + 'st';
// v63: BNCE to the left the flams (a grace note .. four strikes, a clap), to the right the bounces; MENV the ratio's
// sweep (octaves); the gate (CUT) and the space's decay
const fBnce = v => { if (Math.abs(v - 0.5) < 0.002) return 'OFF'; if (v > 0.5) return 'BNC ' + Math.round((v - 0.5) * 200); const b = (0.5 - v) * 2; return '×' + (1 + Math.round(b * 3)) + ' ' + Math.round((0.03 - 0.02 * b) * 1000) + 'ms'; };
const fMenv = v => Math.abs(v - 0.5) < 0.01 ? 'OFF' : (v > 0.5 ? '+' : '−') + (Math.abs(v - 0.5) * 6).toFixed(1) + 'oct';
// NCOL: the noise's colour as where its weight lies (its centroid, measured: core/sk_alloy noiseCol)
const NCOL_HZ = [[0, 200], [.2, 830], [.35, 1600], [.5, 3000], [.6, 4300], [.7, 6000], [.8, 8600], [.9, 12000], [1, 13600]];
const fNcol = v => { let i = 0; while (i < NCOL_HZ.length - 2 && v > NCOL_HZ[i + 1][0]) i++;
  const [a, fa] = NCOL_HZ[i], [b, fb] = NCOL_HZ[i + 1], hz = fa * Math.pow(fb / fa, clamp((v - a) / (b - a), 0, 1));
  return hz >= 1000 ? (hz / 1000).toFixed(1) + 'k' : Math.round(hz) + 'Hz'; };
const AL_NICE = [[0.5, '½'], [1, '1'], [Math.SQRT2, '√2'], [1.5, '1.5'], [1.6180340, 'φ'], [Math.sqrt(3), '√3'], [2, '2'], [2.5, '2.5'],
  [Math.E, 'e'], [3, '3'], [Math.PI, 'π'], [3.5, '3.5'], [4, '4'], [5, '5'], [6, '6'], [7, '7'], [8, '8'], [9, '9'], [10, '10'],
  [11, '11'], [12, '12'], [13, '13'], [14, '14'], [16, '16']];
const alRatio = v => { const r = 0.5 * Math.pow(2, clamp(v, 0, 1) * 5); const n = AL_NICE.find(([x]) => Math.abs(r - x) < 0.012 * x); return n ? n : [r, r.toFixed(r < 10 ? 2 : 1)]; };
const fAlRatio = v => '×' + alRatio(v)[1];
const fCfb = v => { const f = (v - 0.5) * 2 * 99.5; return Math.abs(f) < 1 ? '0%' : (f > 0 ? '+' : '−') + Math.round(Math.abs(f)) + '%'; };
const fLfoHz = hz => hz < 1 ? hz.toFixed(2) + 'Hz' : hz.toFixed(1) + 'Hz';
const AL_DACS = ['OFF', '16', '12', '12·32k', '12·24k', '10·16k', '8·11k', '6·8k'];
// (core/sk_fx VSHIFT: none at the left, then from an octave down to an octave up; VERB's SHFT, ALLOY's)
const VERB_SHIFT = ['OFF', '−12', '−7', '−5', '−3', '−1', '−10¢', '+10¢', '+1', '+3', '+5', '+7', '+12'];
// TONE: high-pass 20 Hz .. 8 kHz (OFF at the left), low-pass 100 Hz .. 20 kHz (OFF at the right), the bands +-15 dB;
// envelope followers: the sources, the bands, the input gain (-12 .. +36 dB), the threshold (-48 .. 0 dB)
const hpHz = v => 20 * Math.pow(400, v), lpHz = v => 100 * Math.pow(200, v);
const lowHz = v => 40 * Math.pow(10, v), midHz = v => 150 * Math.pow(53.333, v), highHz = v => 1500 * Math.pow(10, v);
const hpOn = v => v > 0.005, lpOn = v => v < 0.995;
const EF_SRC = ['SELF', 'T1', 'T2', 'T3', 'T4', 'T5', 'T6', 'T7', 'T8', 'IN', 'MAIN'], EF_BAND = ['FULL', 'LOW', 'MID', 'HIGH'];
const efGainDb = v => -12 + 48 * v, efTrgDb = v => -48 + 48 * v, efTrgOn = v => v > 0.01;
const fDbS = d => (Math.abs(d) < 0.05 ? '0' : (d > 0 ? '+' : '') + d.toFixed(Math.abs(d) < 10 ? 1 : 0)) + 'dB';
// machines: id, long name, A..F params, defaults [TUNE, DEC, A..F]
const k = (l, f, n) => ({ l, f: f || fU, n });
// FUZZ's BIAS: under 12 o'clock the battery (9 V down to 3.4 V), over it the first stage hotter (core/sk_fuzz)
const fBias = v => Math.abs(v - 0.5) < 0.02 ? '9V' : v < 0.5 ? (3.4 + 5.6 * v / 0.5).toFixed(1) + 'V' : 'HOT ' + Math.round((v - 0.5) * 200);
// CUBE (v74): its factory banks (Z), a place in the cube (1 .. 8 along X and Y), an offset in waves (SWEP, BX, BY,
// SWPY: -7 .. +7), the pitch envelope (+-48 semitones), WARP (-100 .. +100)
const CUBE_BANKS = ['BASIC', 'HARM', 'VOWEL', 'FM', 'FOLD', 'BITS', 'GLASS', 'DUST'];
const fCubeAx = v => (1 + v * 7).toFixed(1);
const fCubeOff = v => { const w = (v - 0.5) * 14; return Math.abs(w) < 0.05 ? 'OFF' : (w > 0 ? '+' : '−') + Math.abs(w).toFixed(1); };
const fCubeRel = v => { const w = (v - 0.5) * 14; return Math.abs(w) < 0.05 ? '0' : (w > 0 ? '+' : '−') + Math.abs(w).toFixed(1); };
const fCubePe = v => { const st = Math.round((v - 0.5) * 96); return st === 0 ? 'OFF' : (st > 0 ? '+' : '−') + Math.abs(st) + 'st'; };
const fCubeWarp = v => { const x = Math.round((v - 0.5) * 200); return x === 0 ? 'OFF' : (x > 0 ? '+' : '−') + Math.abs(x); };
// WEST (v76): RATI (the modulator against the note), PULS (OFF, then the tempo's divisions), CNT (pulses after the trig),
// a bipolar knob (its centre: none), the times (core sk_west: ATK, SUST, GLID, DEC), the modulator's detune or rate,
// WARM (the folder's output stage), the spring's decay
const WS_RATL = ['¼', '½', '¾', '1', '5/4', '√2', '1.5', '2', '1+√2', '2.76', '3', '3.5', '4', '5.40', '7', '8.93'];
const WS_DIVL = ['OFF', '1/64', '1/48', '1/32', '1/24', '1/16', '1/12', '1/8', '3/16', '1/4', '1/2'];
const WS_CNTL = [...Array.from({ length: 16 }, (_, i) => '+' + (i + 1)), '∞'];
const fWsBip = (lo, hi, zero) => v => { const e = Math.round((v - 0.5) * 200); return e === 0 ? zero : (e < 0 ? lo : hi) + ' ' + Math.abs(e); };
const fWsDec = v => v >= 0.99 ? 'HOLD' : fSec(0.005 * Math.pow(1400, Math.min(v, 0.98)));
const fWsAtk = v => v < 0.005 ? 'OFF' : fTime(0.001 * Math.pow(2000, v));
const fWsSus = v => v < 0.002 ? 'OFF' : fTime(Math.max(0.005, 2 * v * v));
const fWsGlid = v => v < 0.005 ? 'OFF' : fTime(0.003 * Math.pow(300, v));
const fWsCt = range => v => { const c = Math.round((v - 0.5) * 2 * range); return c === 0 ? '0ct' : (c > 0 ? '+' : '−') + Math.abs(c) + 'ct'; };
const fWsWarm = v => v < 0.002 ? 'OPEN' : fKHz(20000 * Math.pow(0.0665, v));
const fWsSdcy = v => fSec(0.4 * Math.pow(10, 1.2 * v));
// v78 (EaseLFO's whole panel): the sources of the CV routes, the routes' amounts in semitones, the channels' levels
// (CHA, CHB: 2 x the knob squared, 0 dB at 71 %), the envelope's curve (Streams: LOG .. LIN .. EXP), the stage pulses
// (the steps that fire one), the spring's decay (BYOD's T60, on SIZE)
const WS_SRCL = ['SVS', 'EG', 'MO', 'RAND', 'PULS', 'ED', 'PRES', 'INV'];
const fWsStN = n => v => { const st = Math.round((v - 0.5) * 2 * n); return st === 0 ? 'OFF' : (st > 0 ? '+' : '−') + Math.abs(st) + 'st'; };
const fWsDb = v => v < 0.002 ? 'OFF' : (d => (d > 0.05 ? '+' : d < -0.05 ? '−' : '') + Math.abs(d).toFixed(1) + 'dB')(20 * Math.log10(2 * v * v));
const fCurve = v => v >= 0.995 ? 'EXP' : v <= 0.005 ? 'LOG' : Math.abs(v - 0.5) < 0.01 ? 'LIN' : v > 0.5 ? 'EXP ' + Math.round((v - 0.5) * 200) : 'LOG ' + Math.round((0.5 - v) * 200);
const fSpls = v => { const m = idx(v, 32); if (!m) return 'NONE'; const s = []; for (let i = 0; i < 5; i++) if (m >> i & 1) s.push(i + 1); return s.join('·'); };
const wsT60 = (dcy, size) => 0.5 * Math.pow(9, 0.95 * dcy - 0.7 * (1 - size * size));
// v78, WEFT: ALGO, Warps' algorithms morphing (core/mi/warps modulator: x 8, six of them; the vocoder from 0.7, frozen
// at the top), SHFT's frequency shift (its easter egg: Hz down or up), the internal carrier's note
const WEFT_ALGS = ['XFADE', 'FOLD', 'ARING', 'DRING', 'XOR', 'COMP'];
const fWeftAlgo = v => { if (v >= 0.995) return 'V·FRZ'; if (v >= 0.725) return 'VOCO'; if (v > 0.675) return 'COMP›V';
  const a = Math.min(v * 8, 5.999), i = Math.floor(a), f = a - i; return f < 0.1 ? WEFT_ALGS[i] : f > 0.9 ? WEFT_ALGS[Math.min(5, i + 1)] : WEFT_ALGS[i] + '›'; };
const weftShift = v => { const d = v - 0.5; let f = 2 * Math.abs(d); f = f <= 0.4 ? f * f * f * 62.5 : 4 * Math.pow(2, 180 * (f - 0.4) / 12); return d < 0 ? -f : f; };
const fWeftShift = v => { const f = weftShift(v); return Math.abs(f) < 0.05 ? '0Hz' : (f > 0 ? '+' : '−') + (Math.abs(f) < 10 ? Math.abs(f).toFixed(1) : Math.round(Math.abs(f))) + 'Hz'; };
const NOTE_N = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
// v81, HOWL (core/sk_howl): its chords (KEY: the scale's own on the root; POLY: the notes played), the voicings, DROP
const HOWL_TYPES = ['5TH', 'OCT', 'SUS2', 'SUS4', 'MAJ', 'MIN', 'ADD9', 'MADD9', 'MAJ7', 'MIN7', 'DOM7', 'MIN9', '5+♭6', 'KEY', 'POLY'];
const HOWL_VOICS = ['DRONE', 'OPEN', 'BARRE', 'SPRD', 'CLUST', '1ST', '2ND', 'HIGH'], HOWL_DROPS = ['0', 'SUB', '−12', '−12S', '−24'];
const HOWL_KEY = 13, HOWL_POLY = 14, HOWL_PAGES = ['CHORD', 'STRING', 'AMP', 'LOOP', 'CAB', 'BOW'];
// v84, MANTRA (core/sk_mantra): the sensors a trig touches (TCH: core mantraTouchMask), its pages, a voice's frequency
// for its TUNE (core mantraTuneHz: 1 2 bass, 3 .. 6 middle, 7 8 high), the LFO's (0.03 .. 300 Hz), the delay's times
// (8 ms .. 1.1 s), the slow release (DEC: 0.5 .. 32 s)
const MT_TCHS = ['1', '2', '3', '4', '5', '6', '7', '8', '12', '34', '56', '78', '1234', '5678', 'ALL'];
const MT_MASKS = [1, 2, 4, 8, 16, 32, 64, 128, 3, 12, 48, 192, 15, 240, 255];
const MT_PAGES = ['PLAY', 'V1-4', 'V5-8', 'FM1-4', 'FM5-8', 'LFO', 'DELAY', 'OUT'];
const mtTuneHz = (k, x) => (k < 2 ? 600 : k < 6 ? 1200 : 2400) / (1 + 19 * (1 - x) * (1 - x));
const mtLfoHz = v => 0.03 * Math.pow(10000, v), mtDly = v => 0.008 * Math.pow(137.5, v), mtRel = v => 0.5 * Math.pow(2, 6 * v);
// (v82: LOOP's TIME where v81's DIST was on STRING, the same distance: 0.3 .. 80 ms against 2 .. 80; core howlTimeFromDist)
const howlTime = d => clamp((Math.log2(0.002 / 0.0003) + d * Math.log2(40)) / Math.log2(80 / 0.3), 0, 1);
const noteOct = n => NOTE_N[((n % 12) + 12) % 12] + (Math.floor(n / 12) - 1);
const fWeftNote = v => { const n = 48 + Math.round((v - 0.5) * 48); return NOTE_N[((n % 12) + 12) % 12] + (Math.floor(n / 12) - 1); };
// v87, RADIO (core/sk_radio.h): its modes, LEN (sixteenths, FREE last), SQL's threshold (dB), the AIR side's BAND
// (FULL .. 250 Hz), CNTR (300 Hz .. 3 kHz), RATE (0.04 .. 4 Hz), SHFT on LIVE (25 Hz a semitone of TUNE: +- 600 Hz)
const RADIO_MODES = ['LIVE', 'LOOP', 'DIG'];
const RADIO_LENS = ['1/16', '1/8', '3/16', '1/4', '3/8', '1/2', '3/4', '1 BAR', '1½BAR', '2 BAR', 'FREE'];
const fRSql = v => v < 0.005 ? 'OFF' : '−' + Math.abs(Math.round(-66 + 56 * v)) + 'dB';
const fRShift = v => { const hz = Math.round((v - 0.5) * 48 * 25); return hz === 0 ? '0Hz' : (hz > 0 ? '+' : '−') + Math.abs(hz) + 'Hz'; };
const fRBand = v => v < 0.01 ? 'FULL' : fKHz(12000 * Math.pow(250 / 12000, v));
const MACH = [
  { id: 'BD·VA', long: 'ANALOG KICK', p: [k('WAVE'), k('SWP'), k('STIM'), k('CLIK'), k('DRIV'), k('HOLD')], d: [.5, .45, .15, .55, .35, .45, .3, .05] },
  { id: 'BD·FM', long: '2-OP FM KICK', p: [k('RATI', fL(RATIO_L), 24), k('FM'), k('FDEC'), k('FDBK'), k('SWP'), k('FOLD')], d: [.5, .42, mid(3, 24), .45, .3, .15, .4, .1] },
  { id: 'SD·VA', long: 'ANALOG SNARE', p: [k('TON2'), k('BAL'), k('NDEC'), k('NCOL'), k('SNAP'), k('NRES')], d: [.5, .35, .4, .55, .35, .6, .4, .2] },
  { id: 'SD·FM', long: 'FM SNARE + NOISE', p: [k('RATI', fL(RATIO_L), 24), k('FM'), k('FDEC'), k('NOIS'), k('NDEC'), k('NCOL')], d: [.5, .35, mid(6, 24), .45, .3, .6, .35, .55] },
  { id: 'HH·MT', long: 'METAL · 6 OSCILLATORS', p: [k('SPRD'), k('COLR'), k('NOIS'), k('RING'), k('PW'), k('TRNS')], d: [.5, .18, .5, .6, .15, .3, .2, .4] },
  { id: 'CP·NZ', long: 'MULTI-BURST CLAP', p: [k('NUM', fI(1, 8), 8), k('RATE'), k('RESO'), k('TAIL'), k('JITR'), k('GRIT')], d: [.5, .4, mid(3, 8), .45, .35, .4, .2, .1] },
  { id: 'FM·X', long: 'EXTREME FM · 3 OPS', p: [k('ALGO', fL(['3›2›1', '2+3›1', '3›1+2', 'X·FB']), 4), k('RAT2', fL(RATIO_L), 24), k('RAT3', fL(RATIO_L), 24), k('INDX'), k('IDEC'), k('FDBK')], d: [.5, .3, mid(0, 4), mid(14, 24), mid(19, 24), .55, .3, .25] },
  { id: 'NOISE', long: 'NOISE · LFSR · S&H', p: [k('TYPE', fNear(['WHT', 'PNK', 'LFSR', 'DUST'])), k('S&H'), k('SWP', fB), k('SDEC'), k('RESO'), k('GRAN')], d: [.5, .35, .1, 1, .65, .3, .35, 0] },
  { id: 'IMPLS', long: 'IMPULSE TRAIN', p: [k('DENS', fI(1, 32), 32), k('RATE'), k('ACCL', v => '×' + (0.6 + v * 0.8).toFixed(2)), k('RESO'), k('COMB'), k('JITR')], d: [.55, .45, mid(5, 32), .45, .35, .45, .3, .2] },
  { id: 'SY·RAW', long: 'RAW VA · SYNC · SWARM', p: [k('WAVE'), k('DET'), k('SYNC'), k('SWRM'), k('SUB'), k('PW')], d: [.5, .3, .2, .4, 0, 0, .5, .3] },
  { id: 'BYTE', long: 'BYTEBEAT', p: [k('FORM', fI(1, 8), 8), k('SHFA', fI(0, 16), 16), k('SHFB', fI(0, 16), 16), k('BITS', fI(1, 8), 8), k('MULT', v => '×' + (1 + idx(v, 32)), 32), k('SMTH')], d: [.5, .4, mid(0, 8), mid(5, 16), mid(8, 16), mid(7, 8), mid(0, 32), .2] },
  { id: 'µLOOP', long: 'MICRO-LOOPS', p: [k('SRC', fL(ULOOP_SRC), ULOOP_SRC.length), k('LEN', v => v >= 0.98 ? 'FULL' : fSec(0.001 * Math.pow(150, v))), k('SHRK', v => '×' + (0.5 + v).toFixed(2)), k('PTCH', fSt(12)), k('POS'), k('JITR')], d: [.5, .55, mid(4 + org('CHIPS'), ULOOP_SRC.length), .45, .4, .55, 0, .15] },
  { id: 'BUG', long: 'BUFFER ERRORS', p: [k('SRC', fNear(['SIN', 'TRI', 'SAW', 'SQR', 'FM'])), k('CHNK', v => fSec(0.002 * Math.pow(40, v))), k('RPT', fPct), k('DROP', fPct), k('FLIP', fPct), k('SEED', fSeed, 16)], d: [.5, .45, .6, .35, .45, .2, .15, mid(3, 16)] },
  { id: 'CHAOS', long: 'CHAOTIC MAPS', p: [k('MAP', fL(['LOGI', 'HENON', 'LORENZ', 'IKEDA']), 4), k('CHAO'), k('SMTH'), k('SELF'), k('FOLD'), k('SEED', fSeed, 16)], d: [.5, .3, mid(1, 4), .7, .3, .2, 0, mid(5, 16)] },
  { id: 'VOX', long: 'FORMANTS', p: [k('VOWL', fNear(['A', 'E', 'I', 'O', 'U'])), k('SWP', fB), k('SDEC'), k('RESO'), k('NOIS'), k('SHFT', v => '×' + Math.pow(2, (v - 0.5) * 2).toFixed(2))], d: [.5, .35, 0, .75, .3, .45, .1, .5] },
  { id: 'CZ·PD', long: 'PHASE DISTORTION', p: [k('WAVE', fL(['SAW', 'SQR', 'PLS', 'DBL', 'S·P', 'RES1', 'RES2', 'RES3']), 8), k('DCW'), k('DENV'), k('DDEC'), k('RING'), k('RAT2', fL(['½', '1', '1.5', '2', '3', '4', '5', '7']), 8)], d: [.5, .4, mid(5, 8), .2, .7, .3, 0, mid(1, 8)] },
  { id: 'FRACT', long: 'ORGANIC SWARM', p: [k('SRC', fL(ORG_NAMES), NORG), k('NUM', fI(1, 32), 32), k('SPRD', v => fSec(0.3 * v * v)), k('DENS', fOff(v => Math.round(300 * v * v) + '/s')), k('VARI', fPct), k('GRN', v => v >= 0.98 ? 'FULL' : fSec(0.001 * Math.pow(300, v)))], d: [.5, .4, mid(org('CLAP'), NORG), mid(5, 32), .4, .3, .3, 1] },
  { id: 'ITERA', long: 'ADDITIVE · FOLDER · SKIN LIQUID METAL', p: [k('SPRD', fNear(['HARM', 'PRIME', 'BAR'])), k('HARM'), k('MRPH', fNear(['SIN', 'TRI', 'SAW', 'SQR'])), k('FOLD'),
      k('ATCK', v => v < 0.48 ? 'NOISE ' + Math.round((0.5 - v) * 200) : v > 0.52 ? fTime(0.0005 * Math.pow(400, (v - 0.5) * 2)) : 'POP'), k('MODL', fL(['SKIN', 'LIQUID', 'METAL']), 3)], d: [.5, .55, .25, .45, .1, .2, .5, mid(1, 3)] },
  { id: 'TRIO', long: 'MINI PROPHET · 3 VCOS · 4-POLE', p: [k('OSC2', fInt, 49), k('OSC3', fInt, 49), k('DTUN', v => Math.round(v * v * 50) + 'ct'), k('WAVE', fNear(['TRI', 'SAW', 'S+P', 'SQR', 'PLS'])),
      k('MIX'), k('MODE', fL(['FREE', 'SYNC', 'RING', 'PMOD']), 4)], d: [.5, .35, mid(24, 49), mid(36, 49), .2, .25, .3, mid(0, 4)] },
  { id: 'SINE', long: 'PURE SINE · SUB · RING', p: [k('SUB'), k('DROP', v => (v * 5).toFixed(1) + 'oct'), k('RING', fL(RING_L), 16),
      k('PHAS', v => Math.round(v * 90) + '°'), k('GATE', v => v < 0.02 ? 'EXP' : v > 0.98 ? 'SQR' : Math.round(v * 100) + '%'), k('NOIS')],
    d: [.5, .45, .6, .35, mid(0, 16), 0, .2, 0] },
  { id: 'DATA', long: 'BINARY DATA · CLICKS', p: [k('PATN', fSeed, 16), k('CELL', v => fTime(0.00005 * Math.pow(1000, v))), k('DENS', v => Math.round(5 + v * 95) + '%'),
      k('TONE', fNear(['NOISE', 'PULSE', 'SINE'])), k('BITS', fI(1, 32), 32), k('SUB')], d: [.5, .4, mid(3, 16), .35, .55, .65, mid(7, 32), .35] },
  { id: 'FM·Z', long: 'WILD FM · ZAP · SUB', p: [k('RATI', fL(RATIO_L), 24), k('INDX', v => (40 * v * v).toFixed(1)), k('IDEC', v => fTime(0.0003 * Math.pow(2000, v))),
      k('ZAP', v => (v * 7).toFixed(1) + 'oct'), k('SUB'), k('XMOD')], d: [.5, .45, mid(3, 24), .6, .35, .45, .55, .1] },
  { id: 'HARD', long: 'INDUSTRIAL KICK · CLEAN SUB', p: [k('SWP', v => (v * 7).toFixed(1) + 'oct'), k('STIM', v => fTime(0.001 * Math.pow(80, v))),
      k('DRIV', v => '×' + Math.round(Math.pow(60, v))), k('SHAP', fNear(['SOFT', 'HARD', 'FOLD', 'STEP'])), k('NOIS'), k('SUB')], d: [.5, .5, .55, .35, .5, .3, .15, .5] },
  // HEX: the hexagonal-pad analog kit (core/sk_kit): triangle VCO + transistor noise through a 4-pole, BAL between them,
  // CLICK, BEND (timed by DEC), the velocity opening the filter and hardening the attack
  { id: 'HEX·BD', long: 'HEX ANALOG KICK · NOISE · BEND', p: [k('FLTR', fHexF), k('BEND', fBend), k('BAL', fBal), k('CLIK'), k('SWP', fOff(fPct)), k('ATK', fOff(v => (2 * v).toFixed(1) + 'oct'))],
    d: [.5, .55, .45, .72, .4, .45, .6, .25] },
  { id: 'HEX·SD', long: 'HEX ANALOG SNARE · NOISE · RIM', p: [k('FLTR', fHexF), k('BEND', fBend), k('BAL', fBal), k('CLIK'), k('RESO', fPct), k('RIM', fOff(fPct))],
    d: [.5, .4, .7, .62, .62, .5, .55, .35] },
  { id: 'HEX·TM', long: 'HEX ANALOG TOMS · NOISE · BEND', p: [k('FLTR', fHexF), k('BEND', fBend), k('BAL', fBal), k('CLIK'), k('SWP', fOff(fPct)), k('SIZE', fL(['HI', 'MID', 'LOW']), 3)],
    d: [.5, .5, .5, .78, .45, .4, .65, mid(1, 3)] },
  // MACRO: the 24 models of Plaits 1.2 (core/sk_macro, Mutable Instruments, MIT); B / C / D change meaning with the model (macroMeta)
  { id: 'MACRO', long: 'MACRO OSCILLATOR · 24 MODELS', p: [k('MODL', v => MACRO_MODELS[idx(v, 24)], 24), k('HARM'), k('TIMB'), k('MORP'), k('TENV', fB),
      k('AUX', v => v < 0.02 ? 'OUT' : v > 0.98 ? 'AUX' : fPct(v))], d: [.5, .5, mid(0, 24), .5, .5, .5, .5, 0] },
  // RAM: a live sampler. F is its switch: PLAY (these knobs, the PLAY trigs), OSC and SPEC (PRISM's two pages, the PLAY
  // trigs) or REC (RAM_REC_PAGE, the REC trigs). (PLAY and REC keep their values from 2 and 3 positions; PRSM became SPEC)
  // STRT / LEN follow the slices and the take (ramMeta), DEC holds at the top
  { id: 'RAM', long: 'LIVE SAMPLER · REC + PLAY', p: [k('STRT', fPct), k('LEN', fPct), k('MODE', fL(RAM_MODES), 4),
      k('SLCE', fL(RAM_SLICES.map(n => n > 1 ? String(n) : 'OFF')), 5), k('DAC', fL(DAC_MODELS), 5), k('SIDE', fL(['PLAY', 'OSC', 'SPEC', 'REC']), 4)],
    d: [.5, 1, 0, 1, mid(0, 4), mid(0, 5), mid(0, 5), mid(0, 4)] },
  // HALO: chords for pads (core/sk_halo). TUNE + NOTE are a degree of the scale (the quantiser's, or MODE's on TUNE's key);
  // DEC is the let-go; F is its switch: CHORD (these knobs), VOICE (HALO_VOICE_PAGE), TONE (HALO_TONE_PAGE) or AGE (HALO_AGE_PAGE)
  { id: 'HALO', long: 'CHORD PADS · HARMONIC SPREAD', p: [k('CHRD', fL(HALO_SHAPES), 12), k('PROG', fL(HALO_PROGS), NPROG), k('SPRD', fPct),
      k('WAVE', fHWave), k('BLOOM', fOff(fPct)), k('SIDE', fL(['CHORD', 'VOICE', 'TONE', 'AGE']), 4)], d: [.5, .55, mid(6, 12), mid(0, 28), .3, .3, .2, mid(0, 4)] },
  // BREAK: the break slicer (core/sk_break). A trig plays its slot's loop from a slice: its step's (STEP) or SLCE's, plus
  // NOTE; for LEN 16ths or on to the next trig (FREE). SYNC keeps the loop on the tempo without moving its pitch, VARI
  // plays it faster or slower; TUNE transposes, DEC fades each slice (HOLD at the top); F is its switch: PLAY (these
  // knobs) or CHOP (BRK_CHOP_PAGE). The loops: the LOAD panel (ten slots)
  { id: 'BREAK', long: 'BREAK SLICER · LOOP SLOTS', p: [k('SLOT', fSlot, BRK_SLOTS), k('MODE', fL(['SYNC', 'VARI']), 2), k('SLCE', fSlice, BRK_MAXLEN + 1),
      k('LEN', fL(BRK_LENS), 8), k('DUST', fOff(fPct)), k('SIDE', fL(['PLAY', 'CHOP']), 2)], d: [.5, 1, mid(0, 10), mid(0, 2), 0, mid(7, 8), 0, mid(0, 2)] },
  // the FX machines (core/sk_fx). A plays no sound of its own: SRC (A) is what it takes (SUM, at first: every track to
  // its left, but those an FX track before it takes; NBR the track just before; or one), MIX (E) the effect against the direct sound, F its switch: MAIN (these knobs) or MORE
  // (FX_MORE_PAGE: XF, XG, XA..XE, ACT last). TUNE and DEC: fxMeta. d: [TUNE, DEC, A..F, XA..XE, XF, XG]
  { id: 'VERB', long: 'RESONANT SPACES · 8 LINES · SHIMMER', p: [k('SRC', fL(FX_SRC), 10), k('SIZE', fPct), k('DCAY'),
      k('ABSR', fPct), k('MIX', fPct), k('SIDE', fL(['MAIN', 'MORE']), 2)], d: [.5, 1, mid(1, 10), .7, .55, .35, .32, mid(0, 2), .3, .25, .05, 0, mid(0, 3), .34, 0] },
  { id: 'ECHO', long: 'STEREO TAPE DELAY · ZONES · PING', p: [k('SRC', fL(FX_SRC), 10), k('ZONE', fL(['KARP', 'SLAP', 'ECHO', 'LONG']), 4), k('TIME'),
      k('REPT', v => Math.round(v * 110) + '%'), k('MIX', fPct), k('SIDE', fL(['MAIN', 'MORE']), 2)], d: [.42, 1, mid(1, 10), mid(2, 4), .35, .45, .35, mid(0, 2), .15, .2, .2, mid(1, 2), mid(0, 3), 0, 0] },
  { id: 'BEND', long: 'CIRCUIT-BENT BUFFER · REPEATS', p: [k('SRC', fL(FX_SRC), 10), k('TIME'), k('RPTS', fL(['OFF', '2', '3', '4', '6', '8', '12', '16']), 8),
      k('BRK', fOff(fPct)), k('MIX', fPct), k('SIDE', fL(['MAIN', 'MORE']), 2)], d: [.5, 1, mid(1, 10), mid(2, 6), mid(0, 8), .3, 1, mid(0, 2), 0, mid(0, 3), .3, mid(1, 2), mid(0, 2), 0, 0] },
  { id: 'TAPE', long: 'WORN TAPE · GENERATIONS · STOP', p: [k('SRC', fL(FX_SRC), 10), k('WOW', fOff(fPct)), k('FLUT', fOff(fPct)),
      k('SAT', fOff(fPct)), k('MIX', fPct), k('SIDE', fL(['MAIN', 'MORE']), 2)], d: [.5, 1, mid(1, 10), .25, .2, .35, 1, mid(0, 2), .2, .08, .15, mid(0, 4), mid(0, 2), .25, 0] },
  { id: 'CODEC', long: 'LOW-BITRATE CODEC · PACKET LOSS', p: [k('SRC', fL(FX_SRC), 10), k('LOSS', fOff(fPct)), k('BAND', v => v >= 0.98 ? 'FULL' : fKHz(1000 * Math.pow(20, v))),
      k('SPED', fPct), k('MIX', fPct), k('SIDE', fL(['MAIN', 'MORE']), 2)], d: [.5, 1, mid(1, 10), .45, .65, .4, 1, mid(0, 2), .15, .08, mid(0, 2), 0, mid(0, 2), .2, 0] },
  // STRUM: strummed chords on plucked strings (core/sk_strum), HALO's harmony. TUNE + NOTE a degree; DEC how long the
  // strings ring (HOLD at the top); F its switch: CHORD (these knobs), PLAY (STRUM_PLAY_PAGE), STRING (STRUM_STR_PAGE) or
  // AGE (STRUM_AGE_PAGE)
  { id: 'STRUM', long: 'STRUMMED CHORDS · PLUCKED STRINGS', p: [k('CHRD', fL(HALO_SHAPES), 12), k('PROG', fL(HALO_PROGS), NPROG), k('SPRD', fPct),
      k('STRM', fStrum), k('DIR', fL(STRUM_DIRS), 4), k('SIDE', fL(['CHORD', 'PLAY', 'STRING', 'AGE']), 4)], d: [.5, .55, mid(3, 12), mid(0, 28), .35, .3, mid(0, 4), mid(0, 4)] },
  // CHOIR: a sung voice (core/sk_choir), an alto. DEC its fade; F its switch: SING (these knobs), VOICE (CHOIR_VOICE_PAGE)
  // or SPACE (its reverb: CHOIR_SPACE_PAGE). The ribbon under the knobs plays it
  { id: 'CHOIR', long: 'SUNG VOICE · RIBBON · SPACE', p: [k('VOWL', fVowel), k('GLID', fOff(v => fSec(0.03 * Math.pow(50, v)))), k('BRTH', fOff(fPct)),
      k('VIBR', fOff(v => Math.round(80 * v * v) + 'ct')), k('SING', v => (1 + 3 * v).toFixed(1)), k('SIDE', fL(['SING', 'VOICE', 'SPACE']), 3)], d: [.5, .5, .6, .35, .15, .55, 0, mid(0, 3)] },
  // ALLOY: metallic sounds (core/sk_alloy): an impact (IMPACT) rings an object of modes (SHAP, its stiffness, density,
  // size, where it is hit), a noise in it or heard as it is, fed back, driven, converted (NOISE), with an FM pair over
  // it (FM), pure sines, a resonant comb, a phaser (COLOR), its own reverb (SPACE). DEC how long it rings (HOLD at the
  // top: as long as its gate is open); F its switch: OBJECT (these knobs), IMPACT, NOISE, FM, COLOR or SPACE
  // (ALLOY_PAGES). sd: its side pages as a fresh track has them (S1 .. S35; core/sk_engine.cpp machineSideDefaults,
  // test_web checks)
  { id: 'ALLOY', long: 'METAL · IMPACT · FM · COMB · SPACE', p: [k('SHAP', fL(AL_SHAPES), 8), k('STIF', fStif), k('MASS', fMass),
      k('SIZE', fSize), k('HIT', fHit), k('SIDE', fL(['OBJECT', 'IMPACT', 'NOISE', 'FM', 'COLOR', 'SPACE']), 6)], d: [.5, .62, mid(1, 8), .5, .7, .65, .35, mid(0, 6)],
    sd: [.75, .2, .56, 0, .35, .5, .3,  0, 0, .76, .3, 0, 0, 0,  1, 0, .3, .4, 0, .35, .5,  0, .5, 0, mid(24, 49), .75, 0, 0,
      .2, .25, .4, .4, mid(0, 13), 0, .4] },
  // CHIMERA (v66, core/sk_chimera, sk_bank): a sampler on the bank (bank/, its index in the page: CHIM). CAT and SMPL
  // the sample (chimMeta), VARI its FAMILY (to the right a neighbour drawn at each hit, to the left one per step, the
  // same again each time round the pattern; how far: how many neighbours), STRT where it starts, PLAY forward or
  // backwards, F its switch: PLAY (these knobs), GRAFT (another sample's body under its attack), FAMILY (what alike
  // means). DEC: FULL at its top (the sample as it is). sd: its side places (core machineSideDefaults)
  { id: 'CHIMERA', long: 'SAMPLER · FAMILY · GRAFT', p: [k('CAT'), k('SMPL'), Object.assign(k('VARI', v => fVari(v)), { bip: true, sc: ['LOOP', 'RND', 'OFF'] }),
      k('STRT', fPct), k('PLAY', fL(['FWD', 'REV']), 2), k('SIDE', fL(['PLAY', 'GRAFT', 'FAMILY']), 3)], d: [.5, 1, .05, 0, .5, 0, mid(0, 2), mid(0, 3)],
    sd: [0, 0, 0.64241, 0.63271, 0, .5, 2 / 3, .5, .75, 1 / 6, .25, 0, 0, 0] },
  // FUZZ (v68, core/sk_fuzz): an FX machine, three bands each through the germanium fuzz circuit of 1966 (its schematic
  // solved). FUZZ the pot, then a booster into it; BIAS (left: the battery sags, starved and gated; right: hotter,
  // woollier); SUST each band lifted into it (a wall); its MORE side: TONE, each band's drive (0: CLEAN), the two
  // crossovers, GATE (keyed on the source), ACT (IN, or STRV: a trig drains the battery for DEC). v71: its SUB side,
  // an octaver (S1 .. S4: one octave down, two octaves down, PURE .. GRIT, its PATH); SIDE: MAIN, SUB, MORE (a project
  // saved on MORE stays on it). v73: CIRC (S5), the circuit each band goes through (FUZZ, then PRE SAT XCIT MID XSTR
  // GAIN BENT), FUZZ's knob now DRIV (subtle at its bottom, more and more grunge); its MAIN page SRC CIRC DRIV BIAS,
  // TONE DEC MIX (FUZZ_MAIN_PAGE), SUST on the SUB page
  { id: 'FUZZ', long: 'MULTIBAND DRIVE · 8 CIRCUITS · OCTAVER', p: [k('SRC', fL(FX_SRC), 10), k('DRIV', fPct), k('BIAS', fBias),
      k('SUST', fOff(fPct)), k('MIX', fPct), k('SIDE', fL(['MAIN', 'SUB', 'MORE']), 3)], d: [.5, 1, mid(1, 10), .6, .5, .2, 1, mid(0, 3), .55, .67, .6, .5, mid(0, 2), .45, 0] },
  // CUBE (v74, core/sk_cube): a dual wavetable voice in a cube of 512 waves (8 banks, Z, of 8 x 8 waves, X and Y): A and
  // B read it where X Y Z say, each wave band-limited for the pitch it plays at; FM (B on A, linear, thru zero), SWEP (a
  // sweep along X at each hit, bipolar: it starts there and comes back over STIM); F its switch: MAIN (these knobs), OSC
  // (B's interval, detune, place against A's, level; UNIS; SET: the factory banks, or those of the LOAD panel) or MOD
  // (the sweep on Y, STIM; the pitch envelope, PTIM; STEP; WARP; BITS). Z and DEC: cubeMeta. sd: its side places (core
  // machineSideDefaults)
  { id: 'CUBE', long: 'DUAL WAVETABLE · 3D CUBE · FM', p: [k('X', fCubeAx), k('Y', fCubeAx), k('Z'), k('FM', fOff(fPct)),
      Object.assign(k('SWEP', fCubeOff), { bip: true, sc: ['−7', '+7', 'OFF'] }), k('SIDE', fL(['MAIN', 'OSC', 'MOD']), 3)],
    d: [.5, .45, 2 / 7, 0, 0, 0, .5, mid(0, 3)],
    sd: [mid(24, 49), .5, .5, .5, 0, 0, mid(0, 2), .5, .4, .5, .3, mid(0, 4), .5, mid(0, 6)] },
  // WEST (v76, core/sk_west; v78: EaseLFO's whole panel): a west coast voice: the complex oscillator (a sine through the
  // folder, TIMB SYMM, crossfaded to its other wave; FM, AM or BAL from the modulation oscillator at RATI, MOD its
  // index) into two low-pass gates, opened by their levels and the CV sources (the envelope, the pulser, the five-step
  // sequencer, RANDOM, the envelope detector, the velocity, the inverter); the pulser (PULS: OFF, or a division of the
  // tempo); BYOD's spring. F its page (v79: by what you hear, WS_PAGES: PITCH COLOUR SHAPE LAYER RHYTHM MOTION
  // SPACE). DEC 5 ms .. 7 s, HOLD at its top. sd: its side places (core machineSideDefaults)
  { id: 'WEST', long: 'WEST COAST · FOLDER · LOW-PASS GATES · PULSER', p: [k('TIMBRE', fPct), Object.assign(k('SYMMETRY', fWsBip('−', '+', '0')), { bip: true, sc: ['−', '+', '0'] }),
      k('INDEX', fPct), k('RATIO', fL(WS_RATL), 16), k('PULSER', fL(WS_DIVL), 11), k('PAGE', fL(['PITCH', 'COLOUR', 'SHAPE', 'LAYER', 'RHYTHM', 'MOTION', 'SPACE']), 7)],
    d: [.5, .4, .15, .5, .2, mid(5, 16), mid(0, 11), mid(1, 7)],
    sd: [0, mid(0, 2), 0, .15, .65, .3, .25,  mid(2, 3), mid(0, 3), mid(0, 2), .5, .6, 0, 0,  mid(16, 17), .5, .6, 0, 1, 0, 0,
      .5, .5, .5, .5, .5, .9, mid(0, 4),  mid(0, 4), 0, 0, 0, mid(0, 4), .15, .45,
      mid(0, 3), mid(0, 2), mid(0, 8), .5, mid(1, 8), mid(1, 8), mid(0, 8), .5,  mid(2, 3), mid(1, 6), mid(1, 3), 1, mid(1, 3), mid(0, 4), mid(0, 32),
      0, mid(1, 8), 1, mid(0, 3), mid(0, 3), 0, mid(1, 8), 1, Math.SQRT1_2,  mid(0, 8), .5, .5, .5, .5, .5, 0, 0, 0, 0, 0] },
  // WEFT (v78, core/sk_weft, Mutable Instruments' Warps): an FX machine, the source (SRC) the modulator; ALGO morphs
  // through Warps' algorithms (XFADE FOLD ANALOG RING DIGITAL RING XOR COMPARE) into its vocoder; TIMB their parameter;
  // CARR the carrier (its internal oscillators SIN TRI SAW, at NOTE, or a track, taken as SRC is). MORE: LVL1 LVL2 (the
  // amplifiers' drive), MODE (WARP, or SHFT: the frequency shifter, one sideband a side), ACT (IN, or FRZ: the vocoder
  // held for DEC). d: [NOTE, DEC, A..F, XA..XG]
  { id: 'WEFT', long: 'META-MODULATOR · RING · XOR · VOCODER · SHIFTER', p: [k('SRC', fL(FX_SRC), 10), k('ALGO', fWeftAlgo), k('TIMB', fPct),
      k('CARR', fL(['SIN', 'TRI', 'SAW', 'T1', 'T2', 'T3', 'T4', 'T5', 'T6', 'T7', 'T8']), 11), k('MIX', fPct), k('SIDE', fL(['MAIN', 'MORE']), 2)],
    d: [.5, 1, mid(1, 10), .25, .5, mid(0, 11), 1, mid(0, 2), .5, .5, mid(0, 2), 0, mid(0, 2), 0, 0] },
  // HOWL (v81, core/sk_howl): an electric guitar held up to a screaming amp (after Ululo's topology: the amp's sound
  // through the air excites six strings, their sum through the amp closes the loop), its own chords laid on the strings.
  // TUNE the root (ROOT: E2 at 12 o'clock), DEC how long they ring (∞ at the top); CHRD (POLY: the notes played, a string
  // each), VOIC, DROP, FDBK (past about half it sustains, further it howls), MRPH (a new chord: the strings glide there);
  // F its page: CHORD (these knobs), STRING, AMP, LOOP (v82), CAB and BOW (v83). sd: its side places (core
  // machineSideDefaults)
  { id: 'HOWL', long: 'GUITAR FEEDBACK · CHORDS · AMP · DRONE', p: [k('CHRD', fL(HOWL_TYPES), HOWL_TYPES.length), k('VOIC', fL(HOWL_VOICS), 8),
      k('DROP', fL(HOWL_DROPS), 5), k('FDBK', fPct), k('MRPH', v => v < 0.01 ? 'NOW' : fSec(0.02 * Math.pow(1500, v))), Object.assign(k('SIDE', fL(HOWL_PAGES), 6), { sw: true })],
    d: [.5, .55, mid(0, 15), mid(0, 8), mid(2, 5), .6, .64, mid(0, 6)],
    sd: [.55, .45, .3, 0, .45, .25, 0,  .65, .5, .5, .7, .35, .4, .7,  .6, .55, .45, 0, .2, .25, .5,
      0, 0, .5, 0, .5, 1, .5,  0, .45, mid(1, 7), mid(0, 3), .5, .2, 0] },
  // STENCIL (v83, core/sk_stencil): an FX machine, a stereo delay of zones (an homage to the Mimeophon), our VERB in its
  // repeats. SRC as the others' but NONE first (its loop alone: NOIS and REPT past 100 % start it from nothing); ZONE the
  // range RATE moves in (KARP 0.3 .. 12 ms, a string; COMB; SLAP; ECHO; TAPE; LONG; VAST; HUGE up to 5.4 s), RATE (to
  // the right faster), REPT (up to 125 %: past 100 % it runs away, held by its saturation), MIX; F its switch: MAIN,
  // MORE (STC_MORE_PAGE), SPACE (STC_SPACE_PAGE). TUNE: COLR. ACT: IN, HOLD (the loop at unity), FLIP (backwards),
  // PLUK (a trig plucks the loop). NOTE transposes KARP and COMB. d: TUNE, DEC, A..F, XA..XG; sd: SPACE (core
  // machineSideDefaults)
  { id: 'STENCIL', long: 'STEREO DELAY · 8 ZONES · HALO · SELF-OSCILLATION', p: [k('SRC', fL(STC_SRC), 11), k('ZONE', fL(STC_ZONES), 8), k('RATE'),
      k('REPT', v => Math.round(v * 125) + '%'), k('MIX', fPct), k('SIDE', fL(['MAIN', 'MORE', 'SPACE']), 3)],
    d: [.44, 1, mid(2, 11), mid(3, 8), .5, .4, .4, mid(0, 3), .15, .2, .5, mid(1, 2), mid(0, 4), 0, .1],
    sd: [.45, .55, .45, .3, .35, 0, .5] },
  // MANTRA (v84, core/sk_mantra): eight voices that breathe and drive each other (an homage to an organismic drone
  // synthesizer), each one a circuit: a Schmitt-trigger oscillator, its envelope its gate's pull-up, a transistor VCA;
  // all eight on one supply, their sum into an op-amp that saturates; each pair's FM, a hyper LFO, two lo-fi delay
  // lines on one loop, a distortion. TUNE transposes the eight (a trig's NOTE too), DEC their slow release; TCH the
  // sensors a trig touches (MT_TCHS), HLD1 HLD2 each group's HOLD (the level its voices never fall under), DFB the
  // delay's feedback (past the middle it sings alone), DRIV the distortion; F its pages (MT_PAGES). The screen's eight
  // pads are its sensors. sd: its side places (core machineSideDefaults)
  { id: 'MANTRA', long: '8 VOICES · FM · HYPER LFO · DELAY · DRIVE', p: [k('TCH', fL(MT_TCHS), 15), k('HLD1', fOff(fPct)), k('HLD2', fOff(fPct)),
      Object.assign(k('DFB', fPct), { sc: ['0', 'SING'] }), k('DRIV', fOff(fPct)), Object.assign(k('SIDE', fL(MT_PAGES), 8), { sw: true })],
    d: [.5, .72, mid(14, 15), 0, 0, .3, .2, mid(0, 8)],
    sd: [.2778, .2807, .1551, .2778, .15, .2, .85,  .425, .5158, .3651, .425, .25, .3, .85,  .25, mid(0, 3), .25, .25, mid(0, 3), .25, .25,
      .25, mid(0, 3), .25, .25, mid(0, 3), .25, .25,  .35, .5, .25, .25, .25, .35, .35,  .55, .65, .1, .15, .25, .25, .3,
      .25, .7, .45, .3, .4, .25, .15] },
  // RADIO (v87, core/sk_radio): the radio. Four receivers (the TUNE panel: an internet station, or a shortwave receiver
  // on the network at a frequency), shared by the RADIO tracks. TUNE (LIVE: SHFT, the signal moved up or down as a
  // receiver tuned beside it; LOOP, DIG: the transposition), DEC (HOLD at its top); RX, MODE (LIVE: what it plays now,
  // a trig opening it; LOOP: a trig takes the last LEN; DIG: a trig takes LEN from further back), LEN (LIVE: GATE, how
  // long a trig opens it), D (LIVE: SQL, the squelch; LOOP: GRAB, the chance a trig takes a new loop; DIG: DPTH, how far
  // back), STAT (the static), SIDE (PLAY, AIR). sd: AIR (core machineSideDefaults)
  { id: 'RADIO', long: 'INTERNET + SHORTWAVE RADIO · LIVE · LOOP · DIG', p: [k('RX', fL(['1', '2', '3', '4']), 4), k('MODE', fL(RADIO_MODES), 3),
      k('GATE', fL(RADIO_LENS), 11), Object.assign(k('SQL', fRSql), { sc: ['OFF', '−10'] }), k('STAT', fOff(fPct)), k('SIDE', fL(['PLAY', 'AIR']), 2)],
    d: [.5, 1, mid(0, 4), mid(0, 3), mid(10, 11), 0, 0, mid(0, 2)],
    sd: [0, .5, 0, .4, 0, 0, 0] },
];
const MACRO_M = 26;
// MACRO models: the 16 of the first Plaits, then the 8 of version 1.2; what HARMONICS / TIMBRE / MORPH do in each
const MACRO_MODELS = ['VA', 'WSHP', 'FM', 'GRAN', 'HARM', 'WTBL', 'CHRD', 'SPCH', 'SWRM', 'NOIS', 'PART', 'STRG', 'MODL', 'BD', 'SD', 'HH',
  'VCF', 'PD', '6OP·A', '6OP·B', '6OP·C', 'TERR', 'STRM', 'CHIP'];
const MACRO_LBL = [['DTUN', 'SQR', 'SAW'], ['SHAP', 'FOLD', 'ASYM'], ['RATI', 'INDX', 'FDBK'], ['RATI', 'FRMT', 'WDTH'], ['BMPS', 'PEAK', 'SHAP'],
  ['BANK', 'ROW', 'COL'], ['CHRD', 'INV', 'WAVE'], ['SYNT', 'SPEC', 'WORD'], ['RAND', 'DENS', 'DUR'], ['RESP', 'CLK', 'RESO'], ['RAND', 'DENS', 'FILT'],
  ['INHR', 'BRIT', 'DCAY'], ['MATR', 'BRIT', 'DCAY'], ['PNCH', 'TONE', 'DCAY'], ['NOIS', 'MODE', 'DCAY'], ['METL', 'HPF', 'DCAY'],
  ['RESO', 'CUT', 'WAVE'], ['RATI', 'AMNT', 'PW'], ['PRST', 'BRIT', 'ENV'], ['PRST', 'BRIT', 'ENV'], ['PRST', 'BRIT', 'ENV'],
  ['TERR', 'RAD', 'OFFS'], ['CHRD', 'FILT', 'REGS'], ['CHRD', 'ARP', 'SHAP']];
function macroMeta(t, p) {
  const model = idx(t === ui.t ? shownValue(P.A).v : state.tracks[t].p[P.A], 24);
  if (p >= P.B && p <= P.D) {
    const l = MACRO_LBL[model][p - P.B];
    if (l === 'PRST') return k('PRST', v => 'P' + (1 + Math.min(31, Math.floor(v * 1.02 * 32))));
    return k(l);
  }
  if (p === P.E && model === 23) return k('ENVS', fB);   // chiptune: the attenuverter picks its envelope shape
  return null;
}
// RAM: STRT picks a slice when SLCE is on; STRT / LEN read in seconds of the take the track plays; DEC holds at the top
const RAM_DEC = k('DEC', v => v >= 0.995 ? 'HOLD' : fSec(0.02 * Math.pow(300, v)));
// DEC on the machines with an amplitude envelope: at its top HOLD (a pad or a key held, the trig's GATE, or until the next trig)
const DEC_HOLDK = k('DEC', v => v >= 0.99 ? 'HOLD' : fU(v));
function ramPlayed(t) { const b = idx(state.tracks[t].p[P.RBUF], RAM_BUF.length); return b ? b - 1 : t; }
function ramSeconds(t) { const i = ui.ramInfo, b = ramPlayed(t) * 6; return i && i[b] > 0 && i[b + 1] > 0 ? i[b] / i[b + 1] : 0; }
function ramMeta(t, p) {
  if (p === P.DEC) return RAM_DEC;
  if (p !== P.A && p !== P.B) return null;
  const val = q => (t === ui.t ? shownValue(q).v : state.tracks[t].p[q]);
  const ns = RAM_SLICES[idx(val(P.D), RAM_SLICES.length)], take = ramSeconds(t);
  if (p === P.A) return ns > 1 ? k('STRT', v => 'SL ' + (idx(v, ns) + 1) + '/' + ns) : k('STRT', v => take ? fSec(v * take) : fPct(v));
  return k('LEN', v => take ? fSec(v * take / ns) : fPct(v));
}
// a control the machine does not have: shown greyed out, does nothing
const OFF_K = { l: '—', f: () => '', off: true };
MACH.forEach(m => { m.p = m.p.map(x => x || OFF_K); });
// a control with 2..5 positions is a labelled switch, not a knob
const isSwitch = meta => !!meta && !meta.off && meta.n >= 2 && (meta.n <= 5 || !!meta.sw);


// ------------------------------------------------------------------ machine logos
// Each machine is drawn as its own little brand: flat geometric marks on a 48 grid, one colour
// (currentColor), cut-outs done with masks. A katakana tag goes with each mark.
const KANA = ['キック', 'ドラム', 'スネア', 'スナップ', 'メタル', 'クラップ', 'エックス', 'ノイズ', 'パルス',
  'シンセ', 'バイト', 'ループ', 'バグ', 'カオス', 'ボイス', 'フェイズ', 'フラクト', 'イテラ', 'トリオ',
  'サイン', 'データ', 'ザップ', 'ハード', 'ヘックス', 'パッド', 'タム', 'マクロ', 'サンプラー', 'コード', 'ブレイク',
  'リバーブ', 'エコー', 'ベンド', 'テープ', 'コーデック', 'ストラム', 'クワイア', 'アロイ', 'キメラ', 'ファズ', 'キューブ', 'ウエスト', 'ウェフト', 'ハウル', 'ステンシル', 'マントラ', 'ラジオ'];
const cutMask = (u, cuts) => `<mask id="lm${u}" maskUnits="userSpaceOnUse" x="0" y="0" width="48" height="48"><rect width="48" height="48" fill="#fff"/>${cuts}</mask>`;
const NOISE_BITS = '101101011010110101010111101100110011';
// a pointy-top hexagon (the HEX pads)
const hexPts = (cx, cy, r) => [-90, -30, 30, 90, 150, 210].map(a => `${(cx + r * Math.cos(a * Math.PI / 180)).toFixed(2)},${(cy + r * Math.sin(a * Math.PI / 180)).toFixed(2)}`).join(' ');
const LOGOS = [
  // BD·VA — pressure disc
  u => cutMask(u, '<rect y="26.6" width="48" height="2.8" fill="#000"/><rect y="32" width="48" height="2.3" fill="#000"/><rect y="36.9" width="48" height="1.8" fill="#000"/><rect y="41" width="48" height="1.3" fill="#000"/>')
    + `<circle cx="24" cy="24" r="19.5" mask="url(#lm${u})"/>`,
  // BD·FM — carrier ring and modulator satellite
  u => cutMask(u, '<circle cx="36.5" cy="11.5" r="10" fill="#000"/>')
    + `<circle cx="21.5" cy="26.5" r="14.5" fill="none" stroke="currentColor" stroke-width="7" mask="url(#lm${u})"/><circle cx="36.5" cy="11.5" r="6"/>`,
  // SD·VA — half tone, half noise
  u => cutMask(u, [0, 1, 2, 3, 4, 5].map(i => `<rect x="25" y="${8.6 + i * 5.6}" width="20" height="2.6" fill="#000"/>`).join(''))
    + `<rect x="5" y="5" width="38" height="38" rx="8" mask="url(#lm${u})"/>`,
  // SD·FM — a modulated wave cut through the noise body
  u => cutMask(u, '<path d="M0 31 C6 31 8 21.5 14 21.5 S22 31 28 31 S36 21.5 42 21.5 S48 27 50 27" stroke="#000" stroke-width="3.4" fill="none"/>')
    + `<path d="M24 3.5 L45.5 41 H2.5 Z" stroke="currentColor" stroke-width="1" stroke-linejoin="round" mask="url(#lm${u})"/><rect x="2.5" y="43.5" width="43" height="2.4" rx="1.2"/>`,
  // HH·MT — six-oscillator hexagon
  () => '<polygon points="24,5.5 40,14.75 40,33.25 24,42.5 8,33.25 8,14.75" fill="none" stroke="currentColor" stroke-width="5" stroke-linejoin="round"/>'
    + '<polygon points="24,17.5 29.6,20.75 29.6,27.25 24,30.5 18.4,27.25 18.4,20.75"/>',
  // CP·NZ — four slanted bursts
  () => '<path d="M5 43 L12.5 7 H18.5 L11 43 Z M16.5 43 L22.5 15 H28 L22 43 Z M27.5 43 L32 23 H37 L33 43 Z M38 43 L41 31 H45.5 L43.5 43 Z"/>',
  // FM·X — crossed operators
  u => cutMask(u, '<rect x="18" y="18" width="12" height="12" transform="rotate(45 24 24)" fill="#000"/>')
    + `<path d="M9 9 L39 39 M39 9 L9 39" stroke="currentColor" stroke-width="8.5" stroke-linecap="round" fill="none" mask="url(#lm${u})"/><rect x="21" y="21" width="6" height="6" transform="rotate(45 24 24)"/>`,
  // NOISE — dither field
  () => [...NOISE_BITS].map((b, i) => b === '1' ? `<rect x="${3 + (i % 6) * 7}" y="${3 + Math.floor(i / 6) * 7}" width="5.4" height="5.4" rx=".6"/>` : '').join(''),
  // IMPLS — accelerating impulse train
  () => [4, 14.5, 23, 29.5, 34.5, 38.5, 41.7, 44.3].map((x, i) => `<rect x="${x}" y="${5 + i * 1.5}" width="${3.2 - i * 0.18}" height="${38 - i * 1.5}" rx=".5"/>`).join(''),
  // SY·RAW — two raw saw teeth
  () => '<path d="M3 41 L22 7 V41 Z M26 41 L45 7 V41 Z"/><rect x="3" y="43.6" width="42" height="1.8"/>',
  // BYTE — a b in four fat pixels a side, with a stray bit
  () => ['1001', '1110', '1010', '1110'].map((row, r) => [...row].map((b, c) => b === '1' ? `<rect x="${c * 12 + 0.6}" y="${r * 12 + 0.6}" width="10.8" height="10.8"/>` : '').join('')).join(''),
  // µLOOP — closing square spiral
  () => '<path d="M6 41 H41 V7 H7 V34 H34 V14 H14 V27 H27 V21" fill="none" stroke="currentColor" stroke-width="4.4" stroke-linejoin="miter"/>',
  // BUG — misaligned slices
  () => '<rect x="6" y="6" width="34" height="11" rx="1.5"/><rect x="12" y="19.5" width="33" height="9" rx="1.5"/><rect x="3" y="31" width="34" height="11" rx="1.5"/>',
  // CHAOS — strange attractor, two uneven lobes
  () => '<ellipse cx="17" cy="25" rx="13" ry="7.6" transform="rotate(-32 17 25)" fill="none" stroke="currentColor" stroke-width="4.2"/>'
    + '<ellipse cx="33" cy="21.5" rx="10" ry="5.6" transform="rotate(44 33 21.5)" fill="none" stroke="currentColor" stroke-width="4.2"/><circle cx="12" cy="31" r="2.6"/>',
  // VOX — formant mouth
  () => '<rect x="5" y="11" width="38" height="26" rx="13" fill="none" stroke="currentColor" stroke-width="4.6"/><path d="M12.5 24 Q24 13.5 35.5 24 Q24 34.5 12.5 24 Z"/>',
  // CZ·PD — bent phase
  () => '<path d="M4 31 C9 31 11 11 18 11 H21 L30 36 C34 36 38 25 44 25" fill="none" stroke="currentColor" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>'
    + '<rect x="4" y="42" width="40" height="2.2"/><rect x="38" y="6" width="6" height="6"/>',
  // FRACT — a disc breaking into uneven shards, particles flying off
  () => [[-38, 48, 1.2], [48, 170, 2.2], [170, 228, 6.5], [228, 322, 2.6]].map(([a0, a1, off]) => {
      const rad = d => d * Math.PI / 180, cx = 20, cy = 27.5, r = 15.5, m = rad((a0 + a1) / 2);
      const ox = cx + off * Math.cos(m), oy = cy + off * Math.sin(m);
      const pt = d => `${(ox + r * Math.cos(rad(d))).toFixed(2)} ${(oy + r * Math.sin(rad(d))).toFixed(2)}`;
      return `<path d="M${ox.toFixed(2)} ${oy.toFixed(2)} L${pt(a0)} A${r} ${r} 0 0 1 ${pt(a1)} Z"/>`;
    }).join('')
    + '<path d="M36.5 9.5 L42 6.5 L41 12.5 Z"/><rect x="43" y="15" width="3.4" height="3.4" transform="rotate(-15 44.7 16.7)"/><rect x="31" y="2.5" width="2.8" height="2.8" transform="rotate(30 32.4 3.9)"/><rect x="45" y="5" width="2" height="2"/>',
  // ITERA — six partials, spacing stretched toward the inharmonic, cut by the folder's zigzag
  u => cutMask(u, '<path d="M-2 31 L6 24 L14 31 L22 24 L30 31 L38 24 L46 31 L54 24" stroke="#000" stroke-width="2.6" fill="none" stroke-linejoin="miter"/>')
    + `<g mask="url(#lm${u})">` + [[3, 7.2, 39], [13.2, 5.6, 31], [21.6, 4.6, 25], [28.8, 3.8, 20], [35.2, 3.2, 16], [41.2, 2.6, 12.5]]
      .map(([x, w, h]) => `<rect x="${x}" y="${44 - h}" width="${w}" height="${h}" rx=".6"/>`).join('') + '</g><rect x="3" y="45.2" width="41" height="1.6"/>',
  // TRIO — three oscillators, three pitches: stacked discs, each cut clear of the one above
  u => `<mask id="lm${u}a" maskUnits="userSpaceOnUse" x="0" y="0" width="48" height="48"><rect width="48" height="48" fill="#fff"/><circle cx="26" cy="22" r="10.2" fill="#000"/><circle cx="37" cy="11.5" r="7.6" fill="#000"/></mask>`
    + `<mask id="lm${u}b" maskUnits="userSpaceOnUse" x="0" y="0" width="48" height="48"><rect width="48" height="48" fill="#fff"/><circle cx="37" cy="11.5" r="7.6" fill="#000"/></mask>`
    + `<circle cx="13.5" cy="33.5" r="11" mask="url(#lm${u}a)"/><circle cx="26" cy="22" r="8" mask="url(#lm${u}b)"/><circle cx="37" cy="11.5" r="5.4"/>`
    + '<rect x="3" y="4" width="11" height="2.2"/><rect x="3" y="8.2" width="7.5" height="2.2"/><rect x="3" y="12.4" width="4" height="2.2"/>',
  // SINE — the sine as geometry: a turning radius on the unit circle, its height carried out to the edge
  () => '<circle cx="18" cy="26" r="14" fill="none" stroke="currentColor" stroke-width="4.6"/>'
    + '<path d="M18 26 L28 16 H45" fill="none" stroke="currentColor" stroke-width="3.2" stroke-linejoin="miter"/><circle cx="28" cy="16" r="4.2"/><rect x="40.5" y="12.5" width="7" height="7"/>',
  // DATA — a barcode over its row of bits
  () => [[3, 3.4], [8.6, 1.4], [12, 4.6], [18.8, 1.4], [22.4, 3], [27.6, 1.4], [31, 5.2], [38.4, 1.4], [41.6, 3.4]]
      .map(([x, w]) => `<rect x="${x}" y="5" width="${w}" height="30"/>`).join('')
    + [1, 0, 1, 1, 0, 1, 0, 0, 1].map((b, i) => `<rect x="${3 + i * 4.85}" y="${b ? 39 : 41}" width="3.3" height="${b ? 5 : 1.4}"/>`).join(''),
  // FM·Z — a zap: one stroke folded into a Z, the modulator riding its first corner
  () => '<path d="M5 11 H38 L11 38 H43" fill="none" stroke="currentColor" stroke-width="6" stroke-linejoin="miter" stroke-miterlimit="8"/><circle cx="8" cy="11" r="5.5"/>',
  // HARD — a sine clipped flat: two solid lobes
  () => '<path d="M2 24 L9 5 H20 L24 24 Z M24 24 L28 43 H39 L46 24 Z"/><rect x="2" y="23" width="44" height="2"/>',
  // HEX·BD — the pad, its lower half in pressure lines (as the analog kick's disc)
  u => cutMask(u, '<rect y="26.6" width="48" height="2.8" fill="#000"/><rect y="32" width="48" height="2.3" fill="#000"/><rect y="36.9" width="48" height="1.8" fill="#000"/><rect y="41" width="48" height="1.3" fill="#000"/>')
    + `<polygon points="${hexPts(24, 24, 21.5)}" stroke="currentColor" stroke-width="1" stroke-linejoin="round" mask="url(#lm${u})"/>`,
  // HEX·SD — the pad, half tone, half noise
  u => cutMask(u, [0, 1, 2, 3, 4, 5].map(i => `<rect x="25" y="${7.6 + i * 5.8}" width="24" height="2.7" fill="#000"/>`).join(''))
    + `<polygon points="${hexPts(24, 24, 21.5)}" stroke="currentColor" stroke-width="1" stroke-linejoin="round" mask="url(#lm${u})"/>`,
  // HEX·TM — three pads, high to low
  () => `<polygon points="${hexPts(6.4, 37, 5.4)}"/><polygon points="${hexPts(18.6, 30, 8)}"/><polygon points="${hexPts(35.6, 21, 11)}"/>`
    + '<rect x="2" y="44.4" width="44" height="1.9"/>',
  // MACRO — many engines in one: a stack of cards, a wave cut through the front one
  u => cutMask(u, '<path d="M19.5 31.5 C22.5 24.5 26 24.5 29.5 31.5 S36.5 38.5 40.5 31.5" stroke="#000" stroke-width="3.4" fill="none"/>')
    + `<mask id="lm${u}b" maskUnits="userSpaceOnUse" x="0" y="0" width="48" height="48"><rect width="48" height="48" fill="#fff"/><rect x="13" y="13" width="36" height="36" rx="8.5" fill="#000"/></mask>`
    + `<g mask="url(#lm${u}b)" fill="none" stroke="currentColor" stroke-width="3.2"><rect x="2.6" y="2.6" width="24" height="24" rx="5"/><rect x="9.6" y="9.6" width="24" height="24" rx="5"/></g>`
    + `<rect x="16.5" y="16.5" width="29" height="29" rx="6.5" mask="url(#lm${u})"/>`,
  // RAM — tape reels: two spools with their three windows, the tape drawn tight under them
  u => cutMask(u, [12.5, 35.5].map(cx => [-90, 30, 150].map(a => `<circle cx="${(cx + 5.4 * Math.cos(a * Math.PI / 180)).toFixed(2)}" cy="${(21 + 5.4 * Math.sin(a * Math.PI / 180)).toFixed(2)}" r="2.5" fill="#000"/>`).join('')
      + `<circle cx="${cx}" cy="21" r="1.3" fill="#000"/>`).join(''))
    + `<g mask="url(#lm${u})"><circle cx="12.5" cy="21" r="10.5"/><circle cx="35.5" cy="21" r="10.5"/></g>`
    + '<rect x="12.5" y="31.5" width="23" height="3"/><rect x="3" y="40" width="42" height="2.2"/>',
  // HALO — a chord in a halo: three notes stacked like a voicing, cut through a disc, two rings fading out around it
  u => cutMask(u, [[13.5, 18.2, 12.5], [17.5, 23, 13], [21.5, 27.8, 11]].map(([x, y, w]) => `<rect x="${x}" y="${y}" width="${w}" height="2.5" rx="1" fill="#000"/>`).join(''))
    + '<circle cx="24" cy="24" r="21.6" fill="none" stroke="currentColor" stroke-width="1.5" opacity=".42"/>'
    + '<circle cx="24" cy="24" r="16.6" fill="none" stroke="currentColor" stroke-width="2.4" opacity=".72"/>'
    + `<circle cx="24" cy="24" r="11.6" mask="url(#lm${u})"/>`,
  // BREAK — the break chopped: a disc cut in four slices, the third one knocked out of line
  u => `<clipPath id="bk${u}"><rect x="0" y="0" width="14.6" height="48"/><rect x="17" y="0" width="6.4" height="48"/><rect x="33.6" y="0" width="14.4" height="48"/></clipPath>`
    + `<clipPath id="bs${u}"><rect x="25.8" y="0" width="5.4" height="48"/></clipPath>`
    + `<circle cx="24" cy="24" r="19" clip-path="url(#bk${u})"/><circle cx="24" cy="31" r="19" clip-path="url(#bs${u})"/>`,
  // VERB — a room seen down its length: three walls nested off-centre, thinner as they go, the sound a dot in the last
  () => '<rect x="3" y="3" width="42" height="42" rx="7" fill="none" stroke="currentColor" stroke-width="4.6"/>'
    + '<rect x="11.5" y="9.5" width="28" height="28" rx="5" fill="none" stroke="currentColor" stroke-width="3" opacity=".75"/>'
    + '<rect x="19" y="15" width="15" height="15" rx="3" fill="none" stroke="currentColor" stroke-width="1.8" opacity=".5"/><circle cx="26.5" cy="22.5" r="3"/>',
  // ECHO — a hit and its repeats, each smaller and fainter, the last one turned back (FLIP)
  () => '<circle cx="11" cy="24" r="9"/><circle cx="26" cy="24" r="6.4" opacity=".72"/><circle cx="37" cy="24" r="4.4" opacity=".48"/>'
    + '<path d="M47 20.5 L41.5 24 L47 27.5 Z" opacity=".42"/><rect x="3" y="38" width="42" height="2.2"/>',
  // BEND — a tone held flat, then bent up: one thick stroke, a break cut through its bend
  u => cutMask(u, '<rect x="23" y="-2" width="2.6" height="52" fill="#000" transform="rotate(28 24 24)"/>')
    + `<path d="M3 37 H17 C27 37 31 31 33.5 21 L37 7.5" fill="none" stroke="currentColor" stroke-width="7" stroke-linecap="butt" mask="url(#lm${u})"/>`
    + '<path d="M33 3 L45 9 L35.5 15 Z"/>',
  // TAPE — a cassette: the shell, its two hubs and the window cut out, the head opening below
  u => cutMask(u, '<circle cx="15" cy="20" r="4.6" fill="#000"/><circle cx="33" cy="20" r="4.6" fill="#000"/><rect x="20.5" y="16.5" width="7" height="7" rx="1" fill="#000"/>'
      + '<path d="M13 41 L16 33 H32 L35 41 Z" fill="#000"/>')
    + `<rect x="2.5" y="8" width="43" height="33" rx="4" mask="url(#lm${u})"/>`,
  // CODEC — a disc rebuilt from blocks, a run of them lost
  () => { let o = '';
      for (let r = 0; r < 7; r++) for (let c = 0; c < 7; c++) {
        if (Math.hypot(c - 3, r - 3) > 3.35) continue;
        const lost = (r === 2 && c >= 4) || (r === 3 && c === 6) || (r === 4 && c === 0);
        o += `<rect x="${2.5 + c * 6.3}" y="${2.5 + r * 6.3}" width="5.3" height="5.3" rx=".6"${lost ? ' opacity=".18"' : ''}/>`;
      }
      return o; },
  // STRUM — six strings on a slant, the pick crossing them, the strings it has struck still shaking (the lower ones)
  u => cutMask(u, '<path d="M27.5 13.5 C33 11.5 40 12.5 42.5 17 C44.5 21.5 39.5 30.5 33.5 36.5 C30.5 33 26.5 25 25.6 19.6 C25.2 16.6 25.8 14.3 27.5 13.5 Z" fill="none" stroke="#000" stroke-width="3.2"/>')
    + `<g mask="url(#lm${u})">` + [0, 1, 2, 3, 4, 5].map(i => {
        const y = 9 + i * 6.2, w = i >= 3 ? 1.3 : 0;
        return w ? `<path d="M3 ${y + 3} Q 12 ${y + 3 - w * 1.6} 21 ${y + 1.5} T 45 ${y - 2}" fill="none" stroke="currentColor" stroke-width="${2.6 - i * 0.22}"/>`
                 : `<path d="M3 ${y + 3} L45 ${y - 2}" fill="none" stroke="currentColor" stroke-width="${2.6 - i * 0.22}"/>`; }).join('') + '</g>'
    + '<path d="M28 15 C32.5 13.3 38.6 14 40.6 17.6 C42.3 21.2 38.2 28.8 33.4 34 C31 31.1 28 24.6 27.2 20 C26.8 17.6 26.8 15.6 28 15 Z"/>',
  // CHOIR — a voice's spectrum: its harmonics in a row, their tops tracing three formants, the first the highest
  () => { let o = '';
      const env = x => 3 + 28 * Math.exp(-((x - 11) ** 2) / 26) + 19 * Math.exp(-((x - 25) ** 2) / 34) + 10 * Math.exp(-((x - 37) ** 2) / 22);
      for (let k = 0; k < 11; k++) { const x = 3.4 + k * 4.2, h = Math.min(40, env(x)); o += `<rect x="${(x - 1.15).toFixed(2)}" y="${(43 - h).toFixed(2)}" width="2.3" height="${h.toFixed(2)}" rx="1.1"/>`; }
      return o + '<rect x="2" y="44.6" width="44" height="1.6"/>'; },
  // ALLOY — an ingot, its top face cut in, the ring of a hit dying away over it
  u => cutMask(u, '<path d="M14.5 23.6 L33.5 23.6 L31.6 27.4 L16.4 27.4 Z" fill="#000"/>')
    + `<path d="M11 21 L37 21 L45 42 L3 42 Z" mask="url(#lm${u})"/>`
    + '<path d="M5 10.5 L9 4.5 L13 14.5 L17 6.5 L21 13 L25 8.2 L29 11.8 L33 9.2 L37 10.9 L41 10" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linejoin="round" stroke-linecap="round"/>',
  // CHIMERA — a hit's attack (the spike) grafted on another sound's body (the wave dying away), the seam stitched between
  () => '<path d="M3 41 L9.5 4 L14 41 Z"/>'
    + [7, 14, 21, 28, 35].map(y => `<rect x="16.4" y="${y}" width="2" height="3.6" rx=".6"/>`).join('')
    + '<path d="M21 24 Q24 5 27.5 24 Q31 39 34.5 24 Q37 14 39.5 24 Q41.5 30.5 43.5 24 L45.5 24" fill="none" stroke="currentColor" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round"/>'
    + '<rect x="2" y="44.6" width="44" height="1.6"/>',
  // FUZZ — a germanium transistor's can (its rim, the collector's dot), a square wave cut through it, its three legs
  u => cutMask(u, '<path d="M7 24.5 H15.5 V15.5 H24 V24.5 H32.5 V15.5 H41" stroke="#000" stroke-width="3.2" fill="none" stroke-linejoin="miter"/><circle cx="35.5" cy="8.6" r="2.6" fill="#000"/>')
    + `<rect x="6" y="3.5" width="36" height="27" rx="5" mask="url(#lm${u})"/><rect x="3.5" y="30.5" width="41" height="3" rx="1"/>`
    + '<rect x="12.5" y="33.5" width="3" height="11.5"/><rect x="22.5" y="33.5" width="3" height="11.5"/><rect x="32.5" y="33.5" width="3" height="8"/><rect x="32.5" y="39.5" width="9" height="3"/>',
  // CUBE — a cube of waves: its top face solid, three waves running across its left face, a grid of them on its right
  () => {
      const xy = (x, y) => x.toFixed(2) + ' ' + y.toFixed(2);
      let o = '<path d="M24 3.5 L42.5 14 L24 24.5 L5.5 14 Z"/>'
        + '<path d="M5.5 14 L5.5 34.5 L24 45 L42.5 34.5 L42.5 14 M24 24.5 L24 45" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linejoin="round"/>';
      [0.27, 0.5, 0.73].forEach((v, r) => {
        let d = '';
        for (let i = 0; i <= 24; i++) { const u = 0.08 + 0.84 * i / 24; d += (i ? ' L' : 'M') + xy(5.5 + 18.5 * u, 14 + 10.5 * u + 20.5 * v - 2.6 * Math.sin(2 * Math.PI * u * (1 + 0.5 * r))); }
        o += `<path d="${d}" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"/>`;
      });
      for (const a of [0.22, 0.5, 0.78]) for (const b of [0.22, 0.5, 0.78]) o += `<circle cx="${(24 + 18.5 * a).toFixed(2)}" cy="${(24.5 - 10.5 * a + 20.5 * b).toFixed(2)}" r="1.7"/>`;
      return o; },
  // WEST — a sine folded over on itself (its peaks turned back by the folder), over the five steps of a sequence
  () => {
      const fold = v => { let x = v; for (let i = 0; i < 3 && Math.abs(x) > 1; i++) x = Math.sign(x) * 2 - x; return x; };
      let d = '';
      for (let i = 0; i <= 64; i++) { const u = i / 64, y = 17 - 12 * fold(2.1 * Math.sin(2 * Math.PI * u)); d += (i ? ' L' : 'M') + (3 + 42 * u).toFixed(2) + ' ' + y.toFixed(2); }
      return `<path d="${d}" fill="none" stroke="currentColor" stroke-width="2.6" stroke-linejoin="round" stroke-linecap="round"/>`
        + [7, 12, 4, 9, 14].map((hh, i) => `<rect x="${(4.5 + i * 8.2).toFixed(1)}" y="${45 - hh}" width="5.6" height="${hh}" rx=".8"/>`).join(''); },
  // WEFT — two waves woven: the warp's threads standing, the weft's wave passing over and under them
  u => cutMask(u, [9, 19, 29, 39].map(x => `<rect x="${x - 2.6}" y="3" width="5.2" height="42" fill="#000"/>`).join(''))
    + [9, 19, 29, 39].map(x => `<rect x="${x - 1.4}" y="3" width="2.8" height="42" rx="1"/>`).join('')
    + (() => { let d = ''; for (let i = 0; i <= 48; i++) { const u2 = i / 48, y = 24 - 13 * Math.sin(2 * Math.PI * u2 * 1.5); d += (i ? ' L' : 'M') + (3 + 42 * u2).toFixed(2) + ' ' + y.toFixed(2); }
        return `<path d="${d}" fill="none" stroke="currentColor" stroke-width="3.2" stroke-linecap="round" mask="url(#lm${u})"/>`; })(),
  // HOWL — the amp's speaker (its cone, its dust cap), its sound going back through the air to the strings it wakes
  () => '<circle cx="33" cy="24" r="11.4" fill="none" stroke="currentColor" stroke-width="4.6"/><circle cx="33" cy="24" r="4.6"/>'
    + '<path d="M19.9 14.8 A16 16 0 0 0 19.9 33.2 M14.8 13.5 A21 21 0 0 0 14.8 34.5" fill="none" stroke="currentColor" stroke-width="2.6" stroke-linecap="round"/>'
    + [3.4, 6.7, 10].map(x => `<rect x="${(x - 0.85).toFixed(2)}" y="4.5" width="1.7" height="39" rx=".85"/>`).join(''),
  // STENCIL — the duplicator's stencil: a plate, its cut-outs the repeats (two rows, the left line and the right, a
  // little later), each one smaller
  u => cutMask(u, [[11, 16.5, 4.8], [21.5, 16.5, 3.6], [30.5, 16.5, 2.6], [38, 16.5, 1.8], [14, 31.5, 4.8], [25, 31.5, 3.6], [34, 31.5, 2.6], [41, 31.5, 1.8]]
      .map(([cx, cy, r]) => `<circle cx="${cx}" cy="${cy}" r="${r}" fill="#000"/>`).join(''))
    + `<rect x="3" y="7" width="42" height="34" rx="5" mask="url(#lm${u})"/>`,
  // MANTRA — eight sensors round a breath: the petals in pairs (a pair's voices side by side), a heart apart from them
  u => cutMask(u, '<circle cx="24" cy="24" r="7.4" fill="#000"/>')
    + `<g mask="url(#lm${u})">` + [0, 1, 2, 3, 4, 5, 6, 7].map(i => `<ellipse cx="24" cy="10" rx="3.7" ry="8.6" transform="rotate(${(Math.floor(i / 2) * 90 + (i & 1 ? 13 : -13)).toFixed(1)} 24 24)"/>`).join('') + '</g>'
    + '<circle cx="24" cy="24" r="4.6"/>',
  // RADIO — a dial's window: its scale, the needle across it, the aerial out of the cabinet
  u => cutMask(u, '<rect x="7.5" y="16" width="33" height="17" rx="2" fill="#000"/>')
    + `<rect x="3" y="11.5" width="42" height="30" rx="5" mask="url(#lm${u})"/>`
    + [10.5, 14.5, 18.5, 22.5, 26.5, 30.5, 34.5].map((x, i) => `<rect x="${x}" y="${i & 1 ? 24.5 : 21}" width="1.8" height="${i & 1 ? 5 : 8.5}"/>`).join('')
    + '<rect x="27.4" y="17.5" width="3.2" height="14"/><path d="M31 11.5 L41 2.5" stroke="currentColor" stroke-width="3.2" stroke-linecap="round" fill="none"/>'
    + '<circle cx="12" cy="37" r="2"/><circle cx="36" cy="37" r="2"/>',
];
let logoSeq = 0;
function logoSvg(m, cls) {
  const u = 'g' + (logoSeq++);
  return `<svg class="${cls || 'lg'}" viewBox="0 0 48 48" fill="currentColor" aria-hidden="true">${LOGOS[m](u)}</svg>`;
}

const destNames = m => ['OFF', 'TUNE', 'DEC', ...MACH[m].p.map((x, i) => (m === RAM_M && i === 5) || (fxM(m) && (i === 0 || i === 5)) ? '—' : x.l), 'FREQ', 'RESO', 'DRIV', 'BITS', 'SRR', 'VOL', 'PAN', 'DLY', 'REV',
  'FENV', 'IR', 'BMIX', 'BDCY', 'BPRE', 'BTON', 'RTRG', 'RCNT', 'RVEL', 'PMIX', 'WAVE', 'WARP', 'CNTR', 'WDTH', 'SPED', 'BLUR', 'TILT',
  'HP', 'LP', 'MFRQ'];

const TP = {
  [P.TUNE]: k('TUNE', fSemi), [P.DEC]: k('DEC'),
  [P.FTYP]: k('TYPE', fL(['LP', 'BP', 'HP', 'COMB']), 4), [P.FREQ]: k('FREQ', fHz), [P.RES]: k('RESO'),
  [P.FENV]: k('ENV', fB), [P.FDEC]: k('EDEC'), [P.DRV]: k('DRIV'),
  [P.BITS]: k('BITS', fOff(v => (12 - v * 11).toFixed(1))), [P.SRR]: k('SRR', fOff(fU)),
  [P.VOL]: k('VOL'), [P.PAN]: k('PAN', fPan), [P.DLY]: k('DLY'), [P.REV]: k('REV'), [P.CHO]: k('CHO'), [P.PHS]: k('PHS'), [P.SPC]: k('RND', fPct),
  [P.ATK]: k('ATK', fOff(v => fSec(0.001 * Math.pow(1500, v)))), [P.ECRV]: Object.assign(k('CURV', fCurve), { sc: ['LOG', 'EXP'] }), [P.CHOK]: k('CHOK', fL(['—', 'G1', 'G2', 'G3', 'G4']), 5),
  [P.DUCK]: Object.assign(k('DUCK', fOff(fU)), { frame: 'UNDER T1' }), [P.DRON]: k('DRONE', fOnOff, 2), [P.PNCH]: k('PNCH', fOff(v => '+' + (20 * Math.log10(1 + 2.2 * v)).toFixed(1) + 'dB')),
  [P.LSPD]: k('SPD1', fL(LFO_SPD), 16), [P.LDEP]: k('DEP1', fB), [P.LWAV]: k('WAV1', fL(LFO_WAV), NWAV),
  [P.L2SP]: k('SPD2', fL(LFO_SPD), 16), [P.L2DP]: k('DEP2', fB), [P.L2WV]: k('WAV2', fL(LFO_WAV), NWAV),
  // v65: the LFOs' MORE page: SLEW's glide (a share of the cycle; at 0 it steps) and LOOP's length
  [P.L1SL]: k('SLW1', fSlw), [P.L1LN]: k('LEN1', fL(LFO_LEN), NLEN), [P.L2SL]: k('SLW2', fSlw), [P.L2LN]: k('LEN2', fL(LFO_LEN), NLEN),
  [P.NOTE]: k('NOTE', fNote), [P.VEL]: k('VEL'), [P.PROB]: k('PROB', fPct), [P.MICR]: k('MICR', fB),
  [P.RTRG]: k('RTRG', fL(RETRIG), 12), [P.RCNT]: k('RCNT', fI(1, 16), 16), [P.RVEL]: k('RVEL', fB), [P.GATE]: k('GATE', fOff(v => (v * 8).toFixed(2))),
  // v58: the retrig's curve (EVEN in the middle), the jump (-12 .. +12) and its range (1 .. 24 semitones)
  [P.RCRV]: k('CURV', v => Math.abs(v - 0.5) < 0.005 ? 'EVEN' : fB(v)), [P.JUMP]: k('JUMP', fJump, NJUMP), [P.JRNG]: k('RNG', v => (1 + idx(v, NJRNG)) + 'st', NJRNG),
  // v64: the hit's pan drawn (to the right: either side, as far as the knob says; to the left: ping-pong), the spread
  // (TONE's MORE page) and under what it stays centred
  [P.RPAN]: k('RPAN', v => Math.abs(v - 0.5) <= 0.005 ? 'OFF' : (v > 0.5 ? 'RND ' : 'PING ') + Math.round(Math.abs(v - 0.5) * 200)),
  [P.SPRD]: k('SPRD', fOff(fPct)), [P.SMON]: k('MONO', v => Math.round(40 * Math.pow(15, v)) + 'Hz'),
  // v65: the hit's decay drawn (to the right: at each hit; to the left: LOOP, a draw per step, round again with the pattern)
  [P.RDEC]: k('RDEC', v => Math.abs(v - 0.5) <= 0.005 ? 'OFF' : (v > 0.5 ? 'RND ' : 'LOOP ') + Math.round(Math.abs(v - 0.5) * 200)),
  [P.IR]: k('IR', fL(IR_NAMES), NIR + 1), [P.IMIX]: k('MIX', fPct), [P.IPRE]: k('PRE', fOff(v => fSec(v * v * 0.25))),
  [P.IDCY]: k('DCY', v => v >= 0.999 ? 'NAT' : fU(v)), [P.ITON]: k('TONE', fB), [P.IFB]: k('FDBK', fOff(fU)), [P.IREV]: k('RVRS', fOnOff, 2),
  // RAM, REC side (LEVL: 0 dB at 12 o'clock, +12 dB at the top; THRS: -60 .. -6 dBFS)
  [P.RSRC]: k('SRC', fL(RAM_SRC), RAM_SRC.length), [P.RLEN]: k('TAKE', v => fTake(RAM_TAKE[idx(v, RAM_TAKE.length)]), RAM_TAKE.length),
  [P.RLEV]: k('LEVL', fOff(v => { const d = 40 * Math.log10(2 * v); return Math.abs(d) < 0.05 ? '0dB' : (d > 0 ? '+' : '') + d.toFixed(1) + 'dB'; })),
  [P.RTHR]: k('THRS', fOff(v => Math.round(-60 + 54 * v) + 'dB')), [P.ROVD]: k('OVDB', fOff(fPct)), [P.RMON]: k('MONI', fOff(fPct)),
  [P.RBUF]: k('BUF', fL(RAM_BUF), RAM_BUF.length),
  // RAM, PRISM: OSC page (the oscillators) and SPEC page (what is read from the take)
  [P.PMIX]: k('MIX', fOff(fPct)), [P.PVOI]: k('VOIC', fI(1, 16), 16), [P.PBLR]: k('BLUR', v => fSec(0.003 * Math.pow(600, v))),
  [P.PGLD]: k('GLID', fOff(v => fSec(0.001 * Math.pow(1000, v)))), [P.PCTR]: k('CNTR', v => fKHz(prismHz(v))),
  [P.PWID]: k('WDTH', fPWidth),
  [P.PSPD]: k('SPED', v => { const s = prismSpeed(v); return s ? '×' + (s < 1 ? s.toFixed(2) : s.toFixed(s < 2 ? 2 : 1)) : 'FRZ'; }),
  [P.PWAV]: k('WAVE', fPWave), [P.PDYN]: k('DYN', fOff(fPct)), [P.POCT]: k('OCT', fL(PR_OCT), 5),
  [P.PWRP]: k('WARP', v => { const e = prismWarp(v); return Math.abs(v - 0.5) < 0.01 ? 'OFF' : '^' + e.toFixed(2); }),
  [P.PSLC]: k('SLIC', fL(PR_SLICE), PR_SLICE.length),
  [P.PTLT]: k('TILT', v => { const d = (v - 0.5) * 1.5 * 6.02; return Math.abs(d) < 0.05 ? 'FLAT' : (d > 0 ? '+' : '') + d.toFixed(1) + 'dB'; }),
  [P.PQNT]: k('QNT', fOnOff, 2),
  // HALO, VOICE side
  [P.HSWL]: k('SWEL', v => fSec(0.005 * Math.pow(1600, v))), [P.HGLD]: k('GLID', fOff(v => fSec(0.02 * Math.pow(100, v)))),
  [P.HMOD]: k('MODE', fL(HALO_MODES), 12), [P.HINV]: k('INV', fOff(fPct)), [P.HDRF]: k('DRFT', fOff(fPct)),
  [P.HAIR]: k('AIR', fOff(fPct)), [P.HWID]: k('WIDE', fOff(fPct)),
  // v59: HARM (HALO, STRUM); HALO's TONE side; STRUM's PLAY and STRING sides; CHOIR's VOICE and SPACE sides
  [P.HARM]: k('HARM', fL(HARMS), 3),
  [P.HUNI]: k('UNIS', fOff(fPct)), [P.HSUB]: k('SUB', fOff(fPct)), [P.HCUT]: k('CUT', v => v >= 0.998 ? 'OPEN' : '+' + (0.5 + 7.5 * v).toFixed(1) + 'oct'),
  [P.HOPN]: k('OPEN', fOff(fPct)), [P.HVIB]: k('VIB', fOff(v => Math.round(45 * v * v) + 'ct')), [P.HWRM]: k('WARM', fOff(fPct)),
  [P.SHUM]: k('HUMN', fOff(fPct)), [P.SACC]: k('ACCN', v => Math.abs(v - 0.5) < 0.02 ? 'EVEN' : (v < 0.5 ? 'BASS ' : 'TOP ') + Math.round(Math.abs(v - 0.5) * 200)),
  [P.SRNG]: k('RING', v => fSec(0.03 * Math.pow(200, v))), [P.SMAT]: k('MATL', fL(STRUM_MATS), 6), [P.SPCK]: k('PICK', fPct),
  [P.SDMP]: k('BRIT', fPct), [P.SBDY]: k('BODY', fOff(fPct)), [P.SMUT]: k('MUTE', fOff(fPct)), [P.SDBL]: k('DBL', fOff(fPct)), [P.SSTF]: k('STIF', fOff(fPct)),
  [P.CSWL]: k('SWEL', v => fSec(0.02 * Math.pow(2, v * 7))), [P.CFRM]: k('FORM', v => Math.abs(v - 0.5) < 0.02 ? 'ALTO' : '×' + Math.pow(2, (v - 0.5) * 0.6).toFixed(2)),
  [P.CEFF]: k('EFRT', fPct), [P.CVRT]: k('VRAT', v => (4.2 + 3 * v).toFixed(1) + 'Hz'), [P.CPRT]: k('PART', fL(CHOIR_PARTS), 6),
  [P.CSCP]: k('SCOP', fOff(fPct)), [P.CDRF]: k('DRFT', fOff(fPct)),
  [P.CSPC]: k('MIX', fOff(fPct)), [P.CSIZ]: k('SIZE', fPct), [P.CDCY]: k('DCAY', fPct), [P.CDMP]: k('DAMP', fPct), [P.CMOD]: k('MOD', fPct),
  [P.CSHM]: k('SHIM', fL(CHOIR_SHIMS), 4), [P.CPRE]: k('PRE', v => fSec(0.5 * (0.65 * v) * (0.65 * v))),
  // v60: AGE (HALO, STRUM): DTUN the card furthest out (cents), VARI, SHEN, DROP; STRUM's AIR
  [P.GDTN]: k('DTUN', fOff(v => '±' + Math.round(50 * v * v) + 'ct')), [P.GVAR]: k('VARI', fOff(fPct)), [P.GSHN]: k('SHEN', fOff(fPct)),
  [P.GDRP]: k('DROP', fOff(fPct)), [P.SAIR]: k('AIR', fOff(fPct)),
  // BREAK, CHOP side
  [P.BRND]: k('RAND', fOff(fPct)), [P.BREV]: k('REV', fOff(fPct)), [P.BSTU]: k('STUT', fOff(fPct)), [P.BPJM]: k('PJMP', fOff(fPct)),
  [P.BSKP]: k('SKIP', fOff(fPct)), [P.BSED]: k('SEED', v => idx(v, BRK_SEEDS) ? String(idx(v, BRK_SEEDS)) : 'FREE', BRK_SEEDS),
  [P.BSWP]: k('SWAP', fOff(fPct)),
  // TONE
  [P.HPF]: k('HP', v => hpOn(v) ? fKHz(hpHz(v)) : 'OFF'), [P.LPF]: k('LP', v => lpOn(v) ? fKHz(lpHz(v)) : 'OFF'),
  [P.LOW]: k('LOW', fDb(15)), [P.LOWF]: k('LFRQ', v => fKHz(lowHz(v))), [P.MID]: k('MID', fDb(15)), [P.MIDF]: k('MFRQ', v => fKHz(midHz(v))),
  [P.HIGH]: k('HIGH', fDb(15)), [P.HIGF]: k('HFRQ', v => fKHz(highHz(v))),
  // the envelope followers (the destination comes from paramMeta, like the LFOs')
  ...Object.fromEntries([P.E1SR, P.E2SR].flatMap(b => [
    [b, k('SRC', fL(EF_SRC), EF_SRC.length)], [b + 1, k('GAIN', v => fDbS(efGainDb(v)))], [b + 2, k('BAND', fL(EF_BAND), 4)],
    [b + 3, k('TRG', v => efTrgOn(v) ? Math.round(efTrgDb(v)) + 'dB' : 'OFF')], [b + 4, k('ATK', v => fTime(0.0001 * Math.pow(10000, v)))],
    [b + 5, k('DEC', v => fTime(0.005 * Math.pow(800, v)))], [b + 6, k('DPTH', fB)]])),
};
FX_XS.forEach(q => { TP[q] = OFF_K; });   // (the MORE side means something on an FX machine only: fxMeta)
const TRIO_FTYP = k('TYPE', fL(['LP4', 'BP', 'HP', 'COMB']), 4);
const BIPOLAR = new Set([P.FENV, P.PAN, P.LDEP, P.L2DP, P.NOTE, P.MICR, P.RVEL, P.RCRV, P.JUMP, P.RPAN, P.RDEC, P.TUNE, P.ITON, P.PSPD, P.PWID, P.PWRP, P.PTLT,
  P.LOW, P.MID, P.HIGH, P.E1DP, P.E2DP]);
// v68: the sends' levels (80 % gives back what goes in), the PHASER's generators (0.1 .. 18 Hz) and shapes, its routes,
// the DELAY's models
const fLvl = v => v < 0.002 ? 'OFF' : (v >= 0.79 && v <= 0.81 ? '0dB' : fDbS(20 * Math.log10(v * 1.25)));
const fPhRate = v => fHzLow(0.1 * Math.pow(180, v));
const fShape = v => v < 0.02 ? 'SINE' : v > 0.98 ? 'SQR' : 'SQ ' + Math.round(v * 100);
const PH_ROUTES = ['STEREO', 'MONO', 'SERIES'], DLY_MODELS = ['STD', '1982'];
const GP = {
  [G.DTIM]: k('TIME', fL(DLY_T), 11), [G.DFB]: k('FDBK'), [G.DTON]: k('TONE', fB), [G.DPNG]: k('PING'),
  [G.MDRV]: k('CLIP', fOff(fU)), [G.MVOL]: k('VOL'),
  [G.RALG]: k('ALGO', fL(REV_ALGOS), NREV), [G.RLVL]: k('LVL'),
  [G.UMSK]: k('UNMK', fOff(fPct)), [G.UKCK]: k('KICK', fPct), [G.CALM]: k('CALM', fOff(fPct)), [G.CSHP]: k('SHRP', fPct),
  [G.CSEL]: k('SELC', fPct), [G.CSPD]: k('SPD', fPct), [G.CFOC]: k('FOCS', v => fHz(clamp(Math.log2(250 * Math.pow(40, v) / 20) / 9.9658, 0, 1))),
  [G.CDLT]: k('DLTA', fOnOff, 2),
  [G.LOOP]: k('LOOP', fOnOff, 2), [G.LLEN]: k('LEN', fL(LOOP_L), 12), [G.LPOS]: k('POS', v => { const o = idx(v, 33) - 16; return o > 0 ? '+' + o : o < 0 ? '−' + (-o) : '0'; }, 33),
  // master EQ, laid out like the EQP-1A front panel
  [G.EQLF]: k('LFRQ', fL(['20', '30', '60', '100']), 4), [G.EQLB]: k('LBST', fTen), [G.EQLA]: k('LATT', fTen),
  [G.EQHF]: k('HFRQ', fL(['3k', '4k', '5k', '8k', '10k', '12k', '16k']), 7), [G.EQHB]: k('HBST', fTen),
  [G.EQBW]: k('BAND', v => v < 0.2 ? 'SHARP' : v > 0.8 ? 'BROAD' : fTen(v)), [G.EQHA]: k('HATT', fTen),
  [G.EQHS]: k('ASEL', fL(['5k', '10k', '20k']), 3),
  // master compressor
  [G.CINP]: k('INPT', fDbUp(30)), [G.CATK]: k('ATK', v => fTime(0.00005 * Math.pow(600, v))), [G.CREL]: k('REL', v => fTime(0.05 * Math.pow(70, v))),
  [G.CRAT]: k('RATIO', fL(COMP_RATIOS), COMP_RATIOS.length), [G.CDST]: k('DIST', fL(['OFF', 'DIST 2', 'DIST 3']), 3),
  [G.CDET]: k('DET', fL(['FLAT', 'HP', 'BAND', 'HP+BD']), 4), [G.CMIX]: k('MIX', fPct), [G.COUT]: k('OUT', fDb(12)),
  // master drive
  [G.DMOD]: k('MODL', fL(DRIVE_MODELS), DRIVE_MODELS.length), [G.DDRV]: k('DRIV', fDbUp(30)), [G.DBIA]: k('BIAS', fB),
  [G.DTNE]: k('TONE', fB), [G.DCHR]: k('CHAR'), [G.DMIX]: k('MIX', fPct), [G.DOUT]: k('OUT', fDb(12)),
  [G.DPOS]: k('POS', fL(['PRE', 'POST']), 2),
  [G.GTIM]: k('TIME', v => grainDelay() ? 'POS ' + Math.round(v * 100) + '%' : fSec(v * GRAIN_BUF[grainMedia()])),
  [G.GSIZ]: k('SIZE', fGrainSize), [G.GSHP]: k('SHAP', fPct), [G.GPIT]: k('PTCH', v => { const st = grainPitch(v); return (st > 0 ? '+' : '') + (Math.abs(st - Math.round(st)) < 0.05 ? Math.round(st) : st.toFixed(1)) + 'st'; }),
  [G.GDEN]: k('DENS', fGrainDens), [G.GFDB]: k('FDBK', fPct), [G.GVRB]: k('VERB', fPct), [G.GRND]: k('RAND', fPct),
  [G.GMED]: k('MEDIA', fL(GRAIN_MEDIA), 4), [G.GSYN]: k('CLK', fL(GRAIN_SYNC), 4), [G.GFRZ]: k('FRZ', fOnOff, 2),
  [G.GON]: k('GRAIN', fOnOff, 2), [G.GMIX]: k('MIX', fPct),
  [G.GLRT]: k('RATE', fL(GLFO_RATE_L), 15), [G.GLWV]: k('WAVE', v => IMPAKT_LFO.names[idx(v, 64)], 64), [G.GLPH]: k('PHAS', v => '+' + idx(v, 16) + '/16', 16),
  ...Object.fromEntries(GRAIN_PS.map((l, i) => [G.GA0 + i, k('~' + l, v => { const x = Math.round((v - 0.5) * 200); return x === 0 ? '0' : (x > 0 ? '+' : '') + x + '%'; })])),
  [G.EQON]: k('EQ', fOnOff, 2), [G.COON]: k('COMP', fOnOff, 2), [G.DRON]: k('DRV', fOnOff, 2), [G.OON]: k('OTT', fOnOff, 2),
  [G.OTIM]: k('TIME', fPct), [G.OUP]: k('UPWD', fPct), [G.ODN]: k('DNWD', fPct),
  [G.OLO]: k('LOW', fDb(12)), [G.OMD]: k('MID', fDb(12)), [G.OHI]: k('HIGH', fDb(12)), [G.OOUT]: k('OUT', fDb(12)), [G.OMIX]: k('MIX', fPct),
  [G.XON]: k('GLITCH', fOnOff, 2), [G.XRND]: k('LIVE', fPct), [G.XTIM]: k('TIME', fL(GL_TIMES), 7), [G.XRPT]: k('RPT', fPct), [G.XBND]: k('BEND', fPct),
  [G.XBRK]: k('BRK', fPct), [G.XCRP]: k('CRPT', fPct), [G.XLOS]: k('LOSS', fPct), [G.XMIX]: k('MIX', fPct),
  [G.XCMD]: k('CMOD', fL(GL_CMODES), 3), [G.XLMD]: k('LMOD', fL(GL_LMODES), 5), [G.XSPD]: k('SPD', v => { const a = state && state.g[G.XANA] > 0.5;
    return v < 0.02 ? (a ? 'WOW' : 'SMEAR') : v > 0.98 ? (a ? 'FLUT' : 'CHOP') : fPct(v); }),
  [G.XHLD]: k('HOLD', fOnOff, 2), [G.XRES]: k('RESO', fPct), [G.XANA]: k('LOSS', v => (v > 0.5 ? 'ANA' : 'DIG'), 2),
  [G.XAMD]: k('TAPE', fL(GL_AMODES), 5),
  // v68: the CHORUS, the PHASER, the DELAY's model and its VCO
  [G.CHMD]: k('MODE', fL(['1', '2', '3', '4']), 4), [G.CHIN]: k('INPT', fL(['MONO', 'ST']), 2), [G.CHLV]: k('LEVL', fLvl), [G.CHON]: k('CHORUS', fOnOff, 2),
  [G.PRT1]: k('RAT1', fPhRate), [G.PSH1]: k('SHP1', fShape), [G.PRT2]: k('RAT2', fPhRate), [G.PSH2]: k('SHP2', fShape),
  [G.PDPA]: k('DEPA', fPct), [G.PFBA]: k('FDBA', fOff(fPct)), [G.PDPB]: k('DEPB', fPct), [G.PFBB]: k('FDBB', fOff(fPct)),
  [G.PBSW]: k('B ON', fL(['GEN1', 'GEN2']), 2), [G.PSYN]: k('SYNC', fL(['NORM', 'REV']), 2), [G.PRTE]: k('ROUT', fL(PH_ROUTES), 3),
  [G.PLVL]: k('LEVL', fLvl), [G.PHON]: k('PHASER', fOnOff, 2),
  [G.DMDL]: k('MODL', fL(DLY_MODELS), 2), [G.DBW]: k('BW', fL(['×1', '×2']), 2),
  [G.DVDP]: k('VCO', fOff(v => '±' + Math.round(1200 * v * v) + 'ct')), [G.DVRT]: k('RATE', v => fHzLow(0.05 * Math.pow(400, v))),
  [G.DVSH]: k('SHAP', fShape), [G.DVEN]: k('ENV', v => { const e = (v - 0.5) * 2; return Math.abs(e) < 0.02 ? 'OFF' : (e > 0 ? '+' : '−') + Math.round(Math.abs(e) * 100); }),
  [G.DHLD]: k('HOLD', fOnOff, 2),
};
// GRAIN readouts, following the module's laws (core/mi/beads)
const grainMedia = () => idx(state.g[G.GMED], 4);
const grainDelay = () => state.g[G.GSIZ] >= 0.99;     // SIZE at the end: the delay mode
const grainClockSteps = () => [0, 1, 4, 16][idx(state.g[G.GSYN], 4)];
function grainPitch(v) {
  let d = 2 * v - 1; const sign = d < 0 ? -1 : 1; d = d * sign * 6;
  const di = Math.min(6, Math.floor(d)), iv = [0, 1, 3, 4, 7, 12, 24, 24];
  const f = Math.max(0, (d - di - 1 / 3) * 1.5), sm = f * f * (3 - 2 * f);
  return (iv[di] + (iv[di + 1] - iv[di]) * Math.min(1, sm)) * sign;
}
function fGrainSize(v) {
  if (v >= 0.99) return 'DELAY';
  const rev = v < 1 / 3, sh = Math.min(1, 1.025 * (rev ? v * 3 * (2 - v * 3) : 1 - 1.5 * (v - 1 / 3)));
  return (rev ? '◂' : '') + fSec(4 * Math.pow(2, -sh * 9));
}
const DLY_RATIO = [1/16, 1/16, 1/8, 1/8, 1/4, 1/4, 1/2, 1/2, 1, 1, 1, 1, 1/2, 1/3, 1/4, 1/6, 1/8, 1/10, 1/12, 1/16, 1/16, 1/16];
function fNoteLen(steps) {
  const names = [[1/4, '1/64'], [1/2, '1/32'], [2/3, '1/16T'], [1, '1/16'], [4/3, '1/8T'], [1.6, '1/10'], [2, '1/8'], [8/3, '1/4T'], [3, '1/8D'],
    [4, '1/4'], [16/3, '1/2T'], [6, '1/4D'], [8, '1/2'], [16, '1 BAR']];
  let best = names[0]; for (const n of names) if (Math.abs(Math.log(n[0] / steps)) < Math.abs(Math.log(best[0] / steps))) best = n;
  return best[1];
}
function fGrainDens(v) {
  const clk = grainClockSteps();
  if (grainDelay()) {   // the delay time: a ratio of the clock, or free (12 o'clock = the whole buffer)
    if (clk) return fNoteLen(clk * DLY_RATIO[Math.min(20, Math.floor(v * 20 + 0.5))]);
    return fSec(GRAIN_BUF[grainMedia()] * Math.pow(2, -Math.abs(v - 0.5) * 18));
  }
  if (v >= 0.48 && v <= 0.52) return 'OFF';
  if (clk) return v < 0.48 ? '÷' + GRAIN_DIV[Math.min(7, Math.floor(v * 2.15 * 8))] : Math.round(Math.min(1, (v - 0.52) * 2.15) ** 2 * 100) + '%';
  const hz = v < 0.48 ? 130.81 * Math.pow(2, -v * 16) : 0.5 * 130.81 * Math.pow(2, -(1 - v) * 16);
  return (v > 0.52 ? '~' : '') + (hz >= 10 ? Math.round(hz) : hz.toFixed(1)) + '/s';
}
const GBIP = new Set([G.DVEN, G.DTON, G.COUT, G.DOUT, G.DTNE, G.DBIA, G.DTBI, G.DXBI, G.DSTP, G.DDV, G.OLO, G.OMD, G.OHI, G.OOUT, ...GRAIN_PS.map((_, i) => G.GA0 + i)]);
const revAlgo = () => idx(state.g[G.RALG], NREV);
const gBip = g => (g === G.RDMP || g === G.RCH1) ? revAlgo() === REV_SPECTRAL : GBIP.has(g);
// reverb knobs: what each one does depends on the algorithm
const fT60 = s => s > 40 ? '∞' : fSec(s);
const REV_SEC = [
  v => 0.2 * Math.pow(15, v),                                                             // ROOM
  v => 1.2 * (0.5 + state.g[G.RSIZ]) / -Math.log(0.25 + 0.735 * v),                     // PLATE (measured fit)
  v => 0.6 * Math.pow(10, v),                                                             // SPRING
  v => 0.25 * Math.pow(120, v),                                                           // SPECTRAL
  v => 0.8 * Math.pow(12.5, v),                                                           // HALL
  v => 2 * Math.pow(15, v),                                                               // SPACE
];
const REV_TIME = REV_SEC.map(f => v => fT60(f(v)));
const fWarp = v => { const w = (v - 0.5) * 2; return Math.abs(w) < 0.03 ? '0' : fSt(36)(v); };
function revMeta(g) {
  const a = revAlgo(), spec = a === REV_SPECTRAL;
  switch (g) {
    case G.RTIM: return k('TIME', REV_TIME[a]);
    case G.RSIZ: return a === 2 ? k('LEN', v => Math.round(25 + 45 * v) + 'ms') : spec ? k('BLUR') : k('SIZE', fPct);
    case G.RDMP: return spec ? k('ATMO', fB) : k('DAMP');
    case G.RPRE: return spec ? k('MODE', fL(['NORM', 'FREEZE', 'REVERSE', 'FRZ+REV']), 4) : k('PRE', v => v < 0.002 ? '0ms' : fTime(0.25 * v * v));
    case G.RCH1: return [k('EARL'), k('DIFF'), k('TENS'), k('WARP', fWarp), k('DIFF'), k('SHIM')][a];
    case G.RCH2: return [k('DIFF'), k('MOD'), k('DRIP'), k('REFL'), k('MOD'), k('MOD')][a];
  }
  return null;
}
// model-dependent labels for the drive's BIAS and CHAR knobs
function gMeta(g) {
  if (g === G.RTIM || g === G.RSIZ || g === G.RDMP || g === G.RPRE || g === G.RCH1 || g === G.RCH2) return revMeta(g);
  if (g === G.DCHR || g === G.DBIA) {
    const m = idx(state.g[G.DMOD], DRIVE_MODELS.length);
    const L = g === G.DCHR
      ? [['CHAR', fU], ['IPS', v => { const ips = 3.75 * Math.pow(2, v * 3); return (ips < 10 ? ips.toFixed(2) : ips.toFixed(1)) + ' ips'; }],
         ['IRON', fU], ['BLOK', fU], ['SAG', fU], ['FDBK', fU], ['FIZZ', v => fHz(clamp(Math.log2(2000 * Math.pow(12, v) / 20) / 9.9658, 0, 1))], ['STRV', fU]][m]
      : [['BIAS', fB], ['BIAS', fB], ['CLAS', fB], ['BIAS', fB], ['A↔B', fB], ['BIAS', fB], ['ASYM', fU], ['BIAS', fB]][m];
    return k(L[0], L[1]);
  }
  return GP[g];
}
const PAGES = [
  { id: 'SYN', ps: [P.TUNE, P.DEC, P.A, P.B, P.C, P.D, P.E, P.F] },
  { id: 'FLT', ps: [P.FTYP, P.FREQ, P.RES, P.FENV, P.FDEC, P.DRV, P.BITS, P.SRR] },
  // TONE (v10): the high-pass and low-pass that make room in the mix, then three bands
  { id: 'TONE', ps: [P.HPF, P.LPF, P.LOW, P.LOWF, P.MID, P.MIDF, P.HIGH, P.HIGF] },
  // v67: AMP without the sends (they are on the FX page, SEND); the two LFOs apart, each with its SLW and LEN
  // (v78: ECRV, the amplitude envelope's curve, after its attack)
  // (v81: DRONE, after them)
  { id: 'AMP', ps: [P.VOL, P.PAN, P.ATK, P.ECRV, P.PNCH, P.DUCK, P.CHOK, P.DRON] },
  { id: 'LFO1', ps: [P.LSPD, P.LDEP, P.LDST, P.LWAV, P.L1SL, P.L1LN] },
  { id: 'LFO2', ps: [P.L2SP, P.L2DP, P.L2DS, P.L2WV, P.L2SL, P.L2LN] },
  // the envelope followers (v10)
  { id: 'EF1', ps: [P.E1SR, P.E1GN, P.E1BD, P.E1TG, P.E1AT, P.E1DC, P.E1DP, P.E1DS] },
  { id: 'EF2', ps: [P.E2SR, P.E2GN, P.E2BD, P.E2TG, P.E2AT, P.E2DC, P.E2DP, P.E2DS] },
  // TRG: not a track page but a row of knobs under the trig keys (the per-trig settings, the ones p-locked most)
  { id: 'TRG', seq: true, ps: [P.NOTE, P.VEL, P.PROB, P.MICR, P.RTRG, P.RCNT, P.RVEL, P.GATE] },
  { id: 'BOX', ps: [P.IR, P.IMIX, P.IPRE, P.IDCY, P.IGAT, P.ITON, P.IFB, P.IREV] },
  // v68: four sends (DLY, REV, CHO, PHS)
  { id: 'SEND', ps: [P.DLY, P.REV, P.CHO, P.PHS] },
  { id: 'REV', glob: true, ps: [G.RALG, G.RTIM, G.RSIZ, G.RDMP, G.RPRE, G.RCH1, G.RCH2, G.RLVL] },
  // the delay; the final clipper and the volume (OUT, on MASTR)
  // (v68: its clock's VCO under them; the model, the 1982's bandwidth and HOLD on its screen)
  { id: 'DLY', glob: true, ps: [G.DTIM, G.DFB, G.DTON, G.DPNG, G.DVDP, G.DVRT, G.DVSH, G.DVEN] },
  // v68: the CHORUS and the PHASER, two more sends
  { id: 'CHO', glob: true, ps: [G.CHMD, G.CHIN, G.CHLV] },
  { id: 'PHS', glob: true, ps: [G.PRT1, G.PSH1, G.PDPA, G.PFBA, G.PBSW, G.PRTE, G.PRT2, G.PSH2, G.PDPB, G.PFBB, G.PSYN, G.PLVL] },
  { id: 'OUT', glob: true, ps: [G.MDRV, G.MVOL] },
  { id: 'EQ', glob: true, master: true, ps: [G.EQLF, G.EQLB, G.EQLA, G.EQHF, G.EQHB, G.EQBW, G.EQHA, G.EQHS] },
  { id: 'COMP', glob: true, master: true, ps: [G.CINP, G.CATK, G.CREL, G.CRAT, G.CDST, G.CDET, G.CMIX, G.COUT] },
  { id: 'DRV', glob: true, master: true, ps: [G.DMOD, G.DDRV, G.DBIA, G.DTNE, G.DCHR, G.DMIX, G.DOUT, G.DPOS] },
  // GRAIN (Beads) and OTT, after the compressor; MOD (the strip) turns GRAIN's knobs into its LFO's attenuverters
  { id: 'GRN', glob: true, master: true, ps: [G.GTIM, G.GSIZ, G.GSHP, G.GPIT, G.GDEN, G.GFDB, G.GVRB, G.GMIX] },
  { id: 'OTT', glob: true, master: true, ps: [G.OTIM, G.OUP, G.ODN, G.OLO, G.OMD, G.OHI, G.OOUT, G.OMIX] },
  { id: 'GLCH', glob: true, master: true, ps: [G.XTIM, G.XRPT, G.XBND, G.XBRK, G.XCRP, G.XLOS, G.XRES, G.XMIX] },
  { id: 'MIX', glob: true, ps: [G.UMSK, G.UKCK, G.CALM, G.CSHP, G.CSEL, G.CSPD, G.CFOC, G.CDLT] },
];
const PAGE_BY_ID = Object.fromEntries(PAGES.map(p => [p.id, p]));
// v67: the pages of the bar (SEQ: the machine, the pads, the sequencer) and the modules each one shows at once, with
// their names; MASTR shows the master chain's pages one at a time (its tabs) beside OUT
const PTABS = [['ms', 'Seq'], ['tn', 'Tone'], ['md', 'Mod'], ['fx', 'FX'], ['mt', 'Mastr']];
const KMODS = {
  // (v70: the BOX on TONE, after AMP: it is the track's sound)
  tn: [['FLT', 'FILTER'], ['TONE', 'TONE'], ['AMP', 'AMP'], ['BOX', 'BOX']],
  md: [['LFO1', 'LFO 1'], ['LFO2', 'LFO 2'], ['EF1', 'FOLLOWER 1'], ['EF2', 'FOLLOWER 2']],
  fx: [['SEND', 'SEND'], ['REV', 'REVERB'], ['DLY', 'DELAY'], ['CHO', 'CHORUS'], ['PHS', 'PHASER']],
  mt: [['OUT', 'OUT']],
};
// modules shown one at a time under their names as tabs (the one shown: ui[key]): the FX page's returns (v68) and,
// on the web, the MOD page's LFOs and followers (v69) and TONE's four (v70): the track's screen above them never
// scrolls away (the plugin has the room for them side by side). A tab's pixel: the return's effect on (crossed out
// when off), the modulator routed (a destination, a depth), the module changing the sound; a dot when its module
// holds a p-lock
const RET_IDS = ['REV', 'DLY', 'CHO', 'PHS'], MODT_IDS = ['LFO1', 'LFO2', 'EF1', 'EF2'], TONET_IDS = ['FLT', 'TONE', 'AMP', 'BOX'];
const TG_NAME = { REV: 'REVERB', DLY: 'DELAY', CHO: 'CHORUS', PHS: 'PHASER', LFO1: 'LFO 1', LFO2: 'LFO 2', EF1: 'FOLLOWER 1', EF2: 'FOLLOWER 2',
  FLT: 'FILTER', TONE: 'TONE', AMP: 'AMP', BOX: 'BOX' };
// a TONE module changes the sound: the filter not wide open (or its drive, bits, rate), the EQ not flat (or SPRD), an
// attack, a punch, ducking, a pan off the centre or a choke group, an IR
function toneActive(id) {
  const p = state.tracks[ui.t].p, on = v => v > 0.002;
  if (id === 'FLT') { const ft = idx(p[P.FTYP], 4); return !(ft === 0 && p[P.FREQ] >= 0.999 || ft === 2 && p[P.FREQ] <= 0.001) || Math.abs(p[P.FENV] - 0.5) > 0.004 || on(p[P.DRV]) || on(p[P.BITS]) || on(p[P.SRR]); }
  if (id === 'TONE') return hpOn(p[P.HPF]) || lpOn(p[P.LPF]) || [P.LOW, P.MID, P.HIGH].some(q => Math.abs(p[q] - 0.5) > 0.002) || on(p[P.SPRD]);
  if (id === 'AMP') return on(p[P.ATK]) || on(p[P.PNCH]) || on(p[P.DUCK]) || Math.abs(p[P.PAN] - 0.5) > 0.004 || idx(p[P.CHOK], 5) > 0 || p[P.ECRV] < 0.995 || p[P.DRON] >= 0.5;
  return idx(p[P.IR], NIR + 1) > 0;
}
const MOD_ROUTE = { LFO1: [P.LDST, P.LDEP], LFO2: [P.L2DS, P.L2DP], EF1: [P.E1DS, P.E1DP], EF2: [P.E2DS, P.E2DP] };
const modRouted = id => { const [d, a] = MOD_ROUTE[id], p = state.tracks[ui.t].p; return idx(p[d], NDEST) > 0 && Math.abs(p[a] - 0.5) > 1e-3; };
const TGROUPS = [
  { key: 'ret', label: 'Returns', ids: RET_IDS, on: id => !pageBypassed(id), strike: true },
  { key: 'modt', label: 'Modulators', ids: MODT_IDS, on: id => modRouted(id), strike: false, web: true },
  { key: 'tonet', label: 'Tone', ids: TONET_IDS, on: id => toneActive(id), strike: false, web: true },
];
const tgOf = id => TGROUPS.find(g => g.ids.includes(id) && !(g.web && inPlugin)) || null;
const tgCur = g => ui[g.key] || g.ids[0];
// the track's screen on TONE, MOD and FX: what it shows at first on each (then the module last touched)
const PT_FOCUS = { tn: 'FLT', md: 'LFO1', fx: 'SEND' };
// printed legends for the controls whose ends say more as words than as values (DENSITY, DEJA VU, STEPS)
GP[G.GDEN].sc = ['REG', 'RND', '0'];
GP[G.DVRT].sc = ['.05', '20Hz']; GP[G.PRT1].sc = GP[G.PRT2].sc = ['.1', '18Hz'];
GP[G.GPIT].sc = ['-24st', '+24st', '0'];
TP[P.PGLD].sc = ['OFF', '1s'];
TP[P.RCRV].sc = ['SLOW', 'FAST', 'EVEN']; TP[P.JUMP].sc = ['-12', '+12', 'OFF']; TP[P.JRNG].sc = ['1', '24'];
TP[P.RPAN].sc = ['PING', 'RND', 'OFF']; TP[P.SMON].sc = ['40', '600']; TP[P.RDEC].sc = ['LOOP', 'RND', 'OFF'];
TP[P.L1SL].sc = TP[P.L2SL].sc = ['STEP', 'LINE'];
// JUMP on a quantised track counts scale degrees (°), on a machine with a pitch (core/sk_engine.cpp BASE_HZ)
const NO_PITCH = new Set(['CP·NZ', 'NOISE', 'BYTE', 'CHAOS']);
const JUMP_DEG = Object.assign({}, TP[P.JUMP], { f: v => fJump(v) + (jumpOf(v) ? '°' : '') });
const jumpDeg = (t, m) => quantOn() && state.tracks[t].q !== 0 && !chordM(m) && m !== BREAK_M && m !== RAM_M && !fxM(m) && !NO_PITCH.has(MACH[m].id);
[P.E1AT, P.E2AT].forEach(p => { TP[p].sc = ['100µs', '1s']; });
TP[P.PWID].sc = ['NTCH', 'BAND', 'FULL'];
TP[P.PWAV].sc = ['SINE', 'PULS'];
TP[P.HSWL].sc = ['5ms', '8s']; TP[P.HGLD].sc = ['OFF', '2s']; TP[P.HINV].sc = ['ROOT', 'TURN']; TP[P.HWID].sc = ['MONO', 'WIDE'];
TP[P.HCUT].sc = ['DARK', 'OPEN']; TP[P.SACC].sc = ['BASS', 'TOP', 'EVEN']; TP[P.SRNG].sc = ['DAMP', 'RING']; TP[P.SPCK].sc = ['SOFT', 'HARD'];
TP[P.SDMP].sc = ['DARK', 'BRIT']; TP[P.CFRM].sc = ['DARK', 'BRIT', 'ALTO']; TP[P.CEFF].sc = ['SOFT', 'FULL']; TP[P.CSWL].sc = ['20ms', '2.5s'];
TP[P.CSIZ].sc = ['HALL', 'VAST']; TP[P.CDCY].sc = ['SHORT', 'LONG'];
MACH.find(m => m.id === 'CHOIR').p[0].sc = ['MM', 'EE']; MACH.find(m => m.id === 'STRUM').p[3].sc = ['BLOCK', 'ROLL'];
MACH.find(m => m.id === 'HALO').p[3].sc = ['SINE', 'METL', 'PLS'];
MACH.find(m => m.id === 'BREAK').p[0].sc = ['1', '10'];
MACH.find(m => m.id === 'ITERA').p[4].sc = ['NOISE', '200ms', 'POP'];
// GRAIN with MOD on: the same eight places, each one the attenuverter of its LFO on that control
const GRN_MOD_PAGE = { id: 'GRN', glob: true, master: true, mod: true, ps: GRAIN_PS.map((_, i) => G.GA0 + i) };
// RAM switched to REC: the machine's knobs set the takes; to OSC / SPEC: its spectral resynthesis (PRISM), the
// oscillators on one page, what they read from the take on the other; F stays the switch
const RAM_REC_PS = [P.RSRC, P.RLEN, P.RLEV, P.RTHR, P.ROVD, P.RMON, P.RBUF];
const RAM_REC_PAGE = { id: 'SYN', ramRec: true, ps: [...RAM_REC_PS, P.F] };
const RAM_OSC_PS = [P.PMIX, P.PVOI, P.PWAV, P.PDYN, P.POCT, P.PGLD, P.PQNT];
const RAM_SPEC_PS = [P.PSPD, P.PSLC, P.PBLR, P.PTLT, P.PCTR, P.PWID, P.PWRP];
const RAM_PRISM_PS = [...RAM_OSC_PS, ...RAM_SPEC_PS];
const RAM_OSC_PAGE = { id: 'SYN', ramPrism: 'OSC', ps: [...RAM_OSC_PS, P.F] };
const RAM_SPEC_PAGE = { id: 'SYN', ramPrism: 'SPEC', ps: [...RAM_SPEC_PS, P.F] };
// the side of a RAM track's switch: 0 PLAY, 1 OSC, 2 SPEC, 3 REC (-1: not a RAM track)
const RAM_SIDES = 4, SIDE_REC = 3;
const ramSide = t => mof(state.tracks[t].p[P.MACH]) === RAM_M ? idx(state.tracks[t].p[P.F], RAM_SIDES) : -1;
// the RAM track shown is on its REC side: the trig keys place REC trigs, the knobs show RAM_REC_PAGE
const isRamRec = t => ramSide(t) === SIDE_REC;
const recSide = () => !ui.all && isRamRec(ui.t);
const prismSide = () => !ui.all && (ramSide(ui.t) === 1 || ramSide(ui.t) === 2);
const prismPage = () => ramSide(ui.t) === 1 ? 'OSC' : 'SPEC';
// HALO switched to VOICE, TONE or AGE, STRUM to PLAY, STRING or AGE, CHOIR to VOICE or SPACE: their other knobs (F stays
// the switch; an AGE page leaves its last places empty)
const HALO_VOICE_PS = [P.HSWL, P.HGLD, P.HMOD, P.HINV, P.HARM, P.HDRF, P.HWID];
// (v71: SUB an oscillator under the lowest note, where it sits and (v72) its harmonics beside it, on a row of their
// own; VIB and WARM onto AGE)
const HALO_TONE_PS = [P.HAIR, P.HUNI, P.HCUT, P.HOPN, P.HSUB, P.S1, P.S2];
const STRUM_PLAY_PS = [P.HMOD, P.HINV, P.HARM, P.SHUM, P.SACC, P.SRNG, P.HWID];
const STRUM_STR_PS = [P.SMAT, P.SPCK, P.SDMP, P.SBDY, P.SMUT, P.SDBL, P.SSTF];
const CHOIR_VOICE_PS = [P.CSWL, P.CFRM, P.CEFF, P.CVRT, P.CPRT, P.CSCP, P.CDRF];
const CHOIR_SPACE_PS = [P.CSPC, P.CSIZ, P.CDCY, P.CDMP, P.CMOD, P.CSHM, P.CPRE];
const HALO_AGE_PS = [P.GDTN, P.GVAR, P.GSHN, P.GDRP, P.HVIB, P.HWRM, null];
const STRUM_AGE_PS = [P.SAIR, P.GDTN, P.GVAR, P.GSHN, P.GDRP, null, null];
const HALO_VOICE_PAGE = { id: 'SYN', side: 'VOICE', ps: [...HALO_VOICE_PS, P.F] }, HALO_TONE_PAGE = { id: 'SYN', side: 'TONE', ps: [...HALO_TONE_PS, P.F] };
const STRUM_PLAY_PAGE = { id: 'SYN', side: 'PLAY', ps: [...STRUM_PLAY_PS, P.F] }, STRUM_STR_PAGE = { id: 'SYN', side: 'STRING', ps: [...STRUM_STR_PS, P.F] };
const CHOIR_VOICE_PAGE = { id: 'SYN', side: 'VOICE', ps: [...CHOIR_VOICE_PS, P.F] }, CHOIR_SPACE_PAGE = { id: 'SYN', side: 'SPACE', ps: [...CHOIR_SPACE_PS, P.F] };
const HALO_AGE_PAGE = { id: 'SYN', side: 'AGE', ps: [...HALO_AGE_PS, P.F] }, STRUM_AGE_PAGE = { id: 'SYN', side: 'AGE', ps: [...STRUM_AGE_PS, P.F] };
const SIDE_PAGES = { [HALO_M]: [HALO_VOICE_PAGE, HALO_TONE_PAGE, HALO_AGE_PAGE], [STRUM_M]: [STRUM_PLAY_PAGE, STRUM_STR_PAGE, STRUM_AGE_PAGE],
  [CHOIR_M]: [CHOIR_VOICE_PAGE, CHOIR_SPACE_PAGE] };
// v61: a machine switched to one of its side pages: seven more knobs, in the shared places S1 .. S35 (what each is:
// SIDE_META; F stays the switch). Since v63, ALLOY's: IMPACT, NOISE, FM, COLOR, SPACE
// (v78: 70 places, S1 .. S35 then S36 .. S70 after the params that came between: SQ the param of a place, sideIx the
// place of a param)
const NSIDE_A = 35, NSIDE = 70;
const SQ = q => q < NSIDE_A ? P.S1 + q : P.S36 + (q - NSIDE_A);
const sideIx = p => p >= P.S1 && p <= P.S35 ? p - P.S1 : p >= P.S36 && p <= P.S70 ? NSIDE_A + p - P.S36 : -1;
const isSide = p => sideIx(p) >= 0;
const SP = n => Array.from({ length: n }, (_, i) => SQ(i));
const ALLOY_SIDES = ['IMPACT', 'NOISE', 'FM', 'COLOR', 'SPACE'];
const ALLOY_PAGES = ALLOY_SIDES.map((side, i) => ({ id: 'SYN', side, ps: [...SP(7 * i + 7).slice(7 * i), P.F] }));
SIDE_PAGES[ALLOY_M] = ALLOY_PAGES;
const SIDE_META = {
  [ALLOY_M]: [
    // IMPACT: the mallet (its contact, felt .. steel), the transient, the pitch at the hit, how long it takes to get
    // there (an arc) and how fast it falls, the flams or the bounces, the life in it
    k('MALT', fMalt), k('CLIK', fOff(fPct)), k('PENV', fPenv), k('PRIS', fOff(v => fTime(0.001 * Math.pow(2, v * 8.6)))),
    k('PTIM', v => fTime(0.001 * Math.pow(2, v * 7.64))), k('BNCE', fBnce), k('ORGN', fOff(fPct)),
    // NOISE: into the impact, heard as it is, their colour and length (HELD: a bow, a scrape), the feedback (past 3/4
    // it sings), the drive, the converter
    k('NOIS', fOff(fPct)), k('HISS', fOff(fPct)), k('NCOL', fNcol), k('NLEN', v => v >= 0.99 ? 'HELD' : fTime(0.001 * Math.pow(2, v * 11))),
    k('FDBK', fOff(v => (v > 0.75 ? 'SING ' : '') + Math.round(v * 100))), k('DRIV', fOff(fPct)), k('DAC', fL(AL_DACS), 8),
    // FM: the object's level; the FM pair (its level, ratio, depth, how long the depth takes to rise and how fast it
    // falls, the ratio's sweep)
    k('OBJ', fOff(fPct)), k('FM', fOff(fPct)), k('RATI', fAlRatio), k('INDX', v => (v * v * 12).toFixed(1)),
    k('IATK', fOff(v => fTime(0.001 * Math.pow(2, v * 11)))), k('IDEC', v => v >= 0.99 ? 'HOLD' : fTime(0.002 * Math.pow(2, v * 10))), k('MENV', fMenv),
    // COLOR: the sines over the top (their level, where); the comb (its level, pitch from the note, feedback: under 0
    // the odd harmonics), the flanger in it and the phaser after it
    k('ADD', fOff(fPct)), k('BAND', v => '×' + (2 * Math.pow(2, v * 4.585)).toFixed(1)),
    k('COMB', fOff(fPct)), k('CTUN', fInt, 49), k('CFB', fCfb), k('FLNG', fOff(fPct)), k('PHAS', fOff(fPct)),
    // SPACE: its own reverb (level, room, decay, damping, shift), the gate on all of it, the LFO (FLNG, PHAS, the room)
    k('VERB', fOff(fPct)), k('ROOM', fPct), k('DCAY', v => fSec(0.08 * Math.pow(50, v))), k('DAMP', fPct),
    k('SHFT', fL(VERB_SHIFT), 13), k('CUT', fOff(v => fSec(0.02 * Math.pow(2, v * 6.2)))), k('RATE', v => fLfoHz(0.05 * Math.pow(2, v * 8.6)))] };
// (their printed legends: the ends as words)
{ const M = SIDE_META[ALLOY_M], X = MACH[ALLOY_M].p;
  M[0].sc = ['FELT', 'STEEL']; M[2].sc = ['−36', '+48', 'OFF']; M[5].sc = ['FLAM', 'BNCE', 'OFF']; M[9].sc = ['DARK', 'THIN'];
  M[10].sc = ['1ms', 'HELD']; M[16].sc = ['×½', '×16']; M[20].sc = ['−3', '+3', 'OFF']; M[25].sc = ['ODD', 'ALL', '0'];
  M[29].sc = ['BOX', 'HALL']; M[34].sc = ['.05', '19Hz'];
  X[1].sc = ['SOFT', 'HARD', 'NAT']; X[2].sc = ['LIGHT', 'DENSE']; X[3].sc = ['6', '48']; X[4].sc = ['EDGE', 'MID']; X[5].sw = true; }
function sideMeta(t, m, p) {
  const list = SIDE_META[m];
  const q = sideIx(p);
  if (!list || q < 0 || q >= list.length || !list[q]) return OFF_K;
  return list[q];
}
// DEC: ALLOY's ring (40 ms .. 20 s), HOLD at the top
const ALLOY_DEC = k('DEC', v => v >= 0.99 ? 'HOLD' : fSec(0.04 * Math.pow(2, v * 9)));

// ------------------------------------------------------------------ CHIMERA (v66)
// The bank's index (core/sk_bank.h; tools/make_bank.py made it from the kit): every sample's category, name, rate,
// length, gain, pitch, features, place on the map and outline; where its sound is (file, offset, bytes)
const CHIM = (() => {
  const o = { n: 0, ncat: 0, cats: [], e: [], first: [], count: [], files: 0, bytes: null };
  try {
    if (typeof CHIM_IDX_B64 !== 'string' || !CHIM_IDX_B64) return o;
    const b = b64ToBytes(CHIM_IDX_B64), d = new DataView(b.buffer, b.byteOffset, b.byteLength), u32 = p => d.getUint32(p, true), f32 = p => d.getFloat32(p, true);
    if (String.fromCharCode(b[0], b[1], b[2], b[3]) !== 'SKB1') return o;
    const n = u32(8), nc = u32(12); let p = 16;
    for (let c = 0; c < nc; c++) { const l = b[p]; o.cats.push(String.fromCharCode(...b.subarray(p + 1, p + 1 + l))); p += 1 + l; o.first.push(0); o.count.push(0); }
    for (let i = 0; i < n; i++) {
      const cat = b[p], ch = b[p + 1], nl = b[p + 2] | (b[p + 3] << 8);
      const e = { cat, ch, frames: u32(p + 4), sr: u32(p + 8), coded: u32(p + 16), file: u32(p + 20), foff: u32(p + 24), gain: f32(p + 28), pitch: f32(p + 32),
        feat: Array.from({ length: 8 }, (_, k) => f32(p + 40 + 4 * k)), mx: f32(p + 72), my: f32(p + 76) };
      p += 80; e.name = String.fromCharCode(...b.subarray(p, p + nl)); p += nl;
      e.env = b.slice(p, p + 64); p += 64;
      if (o.count[cat]++ === 0) o.first[cat] = i;
      o.files = Math.max(o.files, e.file + 1);
      o.e.push(e);
    }
    o.n = n; o.ncat = nc; o.bytes = b;
  } catch (err) { o.n = 0; }
  return o;
})();
// (without the index the knobs still read: the factory bank's categories)
const CHIM_CATS0 = ['KICK', 'SNARE', 'CLAP', 'HAT', 'PERC', '808', 'CYMB', 'VOX', 'FX', 'LOOP'];
const chimCats = () => CHIM.ncat ? CHIM.cats : CHIM_CATS0;
// the sample at CAT / SMPL values (-1: none); its category's count
const chimAt = (catV, smpV) => { const c = idx(catV, chimCats().length), n = CHIM.count[c] || 0; return n ? CHIM.first[c] + idx(smpV, n) : -1; };
function fVari(v) { const a = Math.abs(v - 0.5); return a <= 0.005 ? 'OFF' : (v > 0.5 ? 'RND ' : 'LOOP ') + (1 + Math.round(a * 2 * 11)); }
const fChimHz = v => { if (v < 0.02) return 'OFF'; const hz = 60 * Math.pow(133.3333, (v - 0.02) / 0.98); return hz >= 1000 ? (hz / 1000).toFixed(1) + 'k' : Math.round(hz) + ''; };
const CHIM_DEC = k('DEC', v => v >= 0.99 ? 'FULL' : fSec(0.02 * Math.pow(300, v)));
// its pages: GRAFT (the body: its category, OFF = none, and sample; where the seam is, the crossfade, the split, the
// body's tuning around the attack's pitch, its level) and FAMILY (alike in shape or timbre, in its category only, what
// VARI varies, a neighbour at the pitch of the sample it stands in for); S15: the seed of VARI's LOOP (no knob)
const CHIM_GRAFT_PS = SP(7), CHIM_FAM_PS = [P.S8, P.S9, P.S10, P.S11, null, null, null];
SIDE_PAGES[CHIM_M] = [{ id: 'SYN', side: 'GRAFT', ps: [...CHIM_GRAFT_PS, P.F] }, { id: 'SYN', side: 'FAMILY', ps: [...CHIM_FAM_PS, P.F] }];
SIDE_META[CHIM_M] = [
  k('BODY'), k('BSMP'),
  k('SEAM', v => fTime(0.001 * Math.pow(150, v))), k('XFAD', v => fTime(0.0005 * Math.pow(80, v))), k('SPLT', fChimHz),
  Object.assign(k('BTUN', v => { const st = (v - 0.5) * 24; return Math.abs(st) < 0.05 ? 'AUTO' : (st > 0 ? '+' : '') + st.toFixed(1); }), { bip: true, sc: ['−12', '+12', 'AUTO'] }),
  k('BLVL', v => { const d = 36 * v - 24; return Math.abs(d) < 0.05 ? '0dB' : (d > 0 ? '+' : '') + d.toFixed(1) + 'dB'; }),
  Object.assign(k('LIKE', v => { const s2 = (v - 0.5) * 2; return Math.abs(s2) < 0.05 ? 'BOTH' : (s2 < 0 ? 'SHAPE ' : 'TONE ') + Math.round(Math.abs(s2) * 100); }), { bip: true, sc: ['SHAPE', 'TONE', 'BOTH'] }),
  k('SAME', fOnOff, 2), k('VARY', fL(['A', 'B', 'A+B']), 3), k('PTCH', fOnOff, 2)];
SIDE_META[CHIM_M][4].sc = ['OFF', '8k']; SIDE_META[CHIM_M][2].sc = ['1ms', '150']; SIDE_META[CHIM_M][3].sc = ['.5ms', '40'];
// a sample's knob: its number in its category (the screen shows its name)
const chimSmpMeta = (cat, l) => { const n = CHIM.count[cat] || 0; return k(l, v => n ? (idx(v, n) + 1) + '/' + n : '—', n || undefined); };
function chimMeta(t, p) {
  const val = q => (t === ui.t ? shownValue(q).v : state.tracks[t].p[q]);
  const cats = chimCats(), nc = cats.length;
  if (p === P.A) return k('CAT', fL(cats), nc);
  if (p === P.B) return chimSmpMeta(idx(val(P.A), nc), 'SMPL');
  if (p === P.DEC) return CHIM_DEC;
  if (p === P.S1) return k('BODY', fL(['OFF', ...cats]), nc + 1);
  if (p === P.S2) { const c = idx(val(P.S1), nc + 1); return c ? chimSmpMeta(c - 1, 'BSMP') : k('BSMP', () => '—'); }
  return null;
}

// the params a machine with sides owns beyond A..F (what RND moves, what a preset of another machine leaves)
const SIDE_OWN = { [HALO_M]: [...HALO_VOICE_PS, ...HALO_TONE_PS, ...HALO_AGE_PS.filter(q => q != null)],
  [STRUM_M]: [...STRUM_PLAY_PS, ...STRUM_STR_PS, ...STRUM_AGE_PS.filter(q => q != null)], [CHOIR_M]: [...CHOIR_VOICE_PS, ...CHOIR_SPACE_PS],
  [ALLOY_M]: SP(NSIDE_A) };
// their switches: HALO, STRUM four sides (v60: AGE), CHOIR three, ALLOY six (v63)
const SIDE_N = { [HALO_M]: 4, [STRUM_M]: 4, [CHOIR_M]: 3, [ALLOY_M]: 6, [CHIM_M]: 3, [FUZZ_M]: 3 };
// v71: HALO's SUB, where it sits under the lowest note (S1; v72: AUTO, between 45 and 90 Hz, NOTE, the note itself,
// -1 OCT, an octave down; v71's -1 OCT becomes AUTO, its -2 OCT -1 OCT) and (v72) PRES, its harmonics over it (S2; v71's
// SINE / TRI switch: PURE / 75 %); FUZZ's SUB side (S1 .. S4: FXK)
SIDE_META[HALO_M] = [k('SOCT', fL(['AUTO', 'NOTE', '−1 OCT']), 3), k('PRES', v => v < 0.005 ? 'PURE' : fPct(v))];
SIDE_META[HALO_M][1].sc = ['PURE', 'RICH'];
SIDE_OWN[FUZZ_M] = [P.S1, P.S2, P.S3, P.S4, P.S5];
SIDE_N[STENCIL_M] = 3; SIDE_OWN[STENCIL_M] = [P.S1, P.S2, P.S3, P.S4, P.S5, P.S6, P.S7];      // (v83: MAIN MORE SPACE)
const SIDE3 = new Set([HALO_M, STRUM_M, CHOIR_M, ALLOY_M, CHIM_M]);
SIDE_OWN[CHIM_M] = SP(15);       // (CHIMERA: GRAFT, FAMILY and VARI's seed)
// v74: CUBE's sides: OSC (B: its interval, detune, place in the cube against A's, level; UNIS, A doubled; SET, the
// factory banks or the LOAD panel's) and MOD (the sweep on Y, STIM; the pitch envelope, PTIM; STEP, the axes that jump
// from a wave to the next; WARP, the phase bent; BITS)
SIDE_PAGES[CUBE_M] = [{ id: 'SYN', side: 'OSC', ps: [...SP(7), P.F] }, { id: 'SYN', side: 'MOD', ps: [...SP(14).slice(7), P.F] }];
SIDE_META[CUBE_M] = [
  k('BRAT', fInt, 49), Object.assign(k('BDET', v => { const c = Math.round((v - 0.5) * 100); return c === 0 ? '0ct' : (c > 0 ? '+' : '−') + Math.abs(c) + 'ct'; }), { bip: true, sc: ['−50', '+50', '0'] }),
  Object.assign(k('BX', fCubeRel), { bip: true, sc: ['−7', '+7', '0'] }), Object.assign(k('BY', fCubeRel), { bip: true, sc: ['−7', '+7', '0'] }),
  k('BLVL', fOff(fPct)), k('UNIS', fOff(v => Math.round(v * 25) + 'ct')), k('SET', fL(['FACT', 'USER']), 2),
  Object.assign(k('SWPY', fCubeOff), { bip: true, sc: ['−7', '+7', 'OFF'] }), k('STIM', v => fTime(0.005 * Math.pow(800, v))),
  Object.assign(k('PENV', fCubePe), { bip: true, sc: ['−48', '+48', 'OFF'] }), k('PTIM', v => fTime(0.002 * Math.pow(500, v))),
  k('STEP', fL(['OFF', 'X', 'X+Y', 'XYZ']), 4), Object.assign(k('WARP', fCubeWarp), { bip: true, sc: ['−', '+', 'OFF'] }),
  k('BITS', fL(['OFF', '12', '10', '8', '6', '4']), 6)];
SIDE_META[CUBE_M][8].sc = ['5ms', '4s']; SIDE_META[CUBE_M][10].sc = ['2ms', '1s'];
SIDE_N[CUBE_M] = 3; SIDE3.add(CUBE_M); SIDE_OWN[CUBE_M] = SP(14);
// v81: HOWL's pages (core/sk_howl.h): STRING (the pick, the damping, the strings ringing each other, v82: the pickups'
// hum, the feedback's bloom, the drift, the arm), AMP (the preamp, the fuzz, the tone stack's TONE BASS MID, the power
// stage's sag, the level) and (v82) LOOP: the feedback's own controls (the air's length, a micro-delay gliding when
// turned; the loop's colour and its focus; the pickup squealing on its own; the hiss reinjected; the guitar swaying;
// how much of the sound the howl may take)
// (v83) CAB: the amp further (TGHT, a high-pass before the preamp: the bass tight; BITE, the upper mids into it: its
// crunch; PRES, the presence; DPTH, the cabinet's low resonance; CAB, the speaker from dark to crisp; SIZE, 1x12 ..
// 4x12; MIC, on what is recorded only: close or back) and BOW: a magnetic bow held over the strings (BOW its level,
// SWEL its swell, STRG which string, MODE FUND or its harmonics, POS over the neck or at the bridge, WOBL the hand's
// wobble, HAND the other strings muted)
SIDE_PAGES[HOWL_M] = [{ id: 'SYN', side: 'STRING', ps: [...SP(7), P.F] }, { id: 'SYN', side: 'AMP', ps: [...SP(14).slice(7), P.F] },
  { id: 'SYN', side: 'LOOP', ps: [...SP(21).slice(14), P.F] }, { id: 'SYN', side: 'CAB', ps: [...SP(28).slice(21), P.F] },
  { id: 'SYN', side: 'BOW', ps: [...SP(35).slice(28), P.F] }];
const fBipW = (lo, hi, zero) => v => { const c = (v - 0.5) * 2; return Math.abs(c) < 0.01 ? zero : (c < 0 ? lo : hi) + ' ' + Math.round(Math.abs(c) * 100); };
const fHowlHz = v => { const hz = 70 * Math.pow(85.714, v); return hz >= 1000 ? (hz / 1000).toFixed(hz >= 10000 ? 1 : 2) + 'k' : Math.round(hz) + ''; };
SIDE_META[HOWL_M] = [
  Object.assign(k('EXCT', v => v < 0.005 ? 'NONE' : fPct(v)), { sc: ['NONE', 'HARD'] }), Object.assign(k('DAMP', fPct), { sc: ['DARK', 'BRIT'] }),
  k('RESO', fOff(fPct)), k('HUM', fOff(fPct)),
  Object.assign(k('BLOM', v => v < 0.005 ? 'NOW' : fSec(0.0525 * Math.pow(400, v))), { sc: ['NOW', '21s'] }), k('DRFT', fOff(fPct)), k('WAMY', fOff(fPct)),
  k('DRIV', fPct), Object.assign(k('FUZZ', v => v < 0.01 ? 'TUBE' : fPct(v)), { sc: ['TUBE', 'FUZZ'] }), k('TONE', fPct), k('BASS', fPct), k('MID', fPct),
  k('SAG', fOff(fPct)), k('LEVL', v => { const d = 20 * Math.log10(Math.max(0.9 * v * v, 1e-6) / 0.441); return v < 0.01 ? 'OFF' : (Math.abs(d) < 0.05 ? '0' : (d > 0 ? '+' : '') + d.toFixed(1)) + 'dB'; }),
  Object.assign(k('TIME', v => fTime(0.0003 * Math.pow(266.6667, v))), { sc: ['.3ms', '80'] }), Object.assign(k('COLR', fHowlHz), { sc: ['DARK', 'BRIT'] }),
  Object.assign(k('FOCS', fPct), { sc: ['WIDE', 'SING'] }), k('PKUP', fOff(fPct)), k('NOIS', fOff(fPct)), k('MOVE', fOff(fPct)),
  Object.assign(k('LIMT', v => v >= 0.99 ? 'OFF' : Math.round(100 * (0.25 + 0.35 * v)) + '%'), { sc: ['CHORD', 'HOWL'] }),
  Object.assign(k('TGHT', v => v < 0.005 ? 'OFF' : Math.round(25 * Math.pow(16, v)) + 'Hz'), { sc: ['OFF', '400'] }),
  Object.assign(k('BITE', fOff(fDbUp(13))), { sc: ['OFF', '+13'] }),
  Object.assign(k('PRES', v => { const c = (v - 0.5) * 2; return Math.abs(c) < 0.01 ? 'FLAT' : (c > 0 ? '+' : '−') + (9 * Math.abs(c)).toFixed(1) + 'dB'; }), { bip: true, sc: ['−9', '+9', 'FLAT'] }),
  Object.assign(k('DPTH', fOff(fDbUp(10))), { sc: ['OFF', '+10'] }),
  Object.assign(k('CAB', fBipW('DARK', 'CRSP', 'MID')), { bip: true, sc: ['DARK', 'CRSP', 'MID'] }),
  Object.assign(k('SIZE', fNear(['1x12', '2x12', '4x12'])), { sc: ['1x12', '4x12'] }),
  Object.assign(k('MIC', fBipW('CLOSE', 'BACK', 'MID')), { bip: true, sc: ['CLOSE', 'BACK', 'MID'] }),
  Object.assign(k('BOW', fOff(fPct)), { sc: ['OFF', 'FULL'] }),
  Object.assign(k('SWEL', v => fSec(0.02 * Math.pow(300, v))), { sc: ['20ms', '6s'] }),
  k('STRG', fL(['ALL', '1', '2', '3', '4', '5', '6']), 7), k('MODE', fL(['FUND', 'OCT', '12TH']), 3),
  Object.assign(k('POS', fPct), { sc: ['NECK', 'BRDG'] }), k('WOBL', fOff(fPct)), Object.assign(k('HAND', fOff(fPct)), { sc: ['OPEN', 'MUTE'] })];
SIDE_N[HOWL_M] = 6; SIDE3.add(HOWL_M); SIDE_OWN[HOWL_M] = SP(35);
{ const M = MACH[HOWL_M].p; M[3].sc = ['0', 'HOWL']; M[4].sc = ['NOW', '30s']; }
// v84: MANTRA's pages (core/sk_mantra.h): V1-4 and V5-8 (each voice's TUNE, each pair's SHARP from triangle to square,
// the group's PITCH), FM1-4 and FM5-8 (each pair's modulation: its depth, its source OFF FM LFO, FAST; 2 LOOP or 1
// LOOP; TOTL: the LFO's place taken by the instrument's own output), LFO (A, B, OR or AND, LINK; the vibrato, its depth;
// SAG, the shared supply), DELAY (the two lines' times, their modulation, by the LFO or SELF, by triangles or the
// squares' AND; its mix) and OUT (the distortion's mix, the volume, the mix's op-amp driven, the crosstalk, the parts'
// tolerances, their drift, the hiss and the hum)
SIDE_PAGES[MANTRA_M] = MT_PAGES.slice(1).map((side, i) => ({ id: 'SYN', side, ps: [...SP(7 * (i + 1)).slice(7 * i), P.F] }));
const fMtHz = hz => (hz >= 1000 ? (hz / 1000).toFixed(2) + 'k' : hz >= 100 ? Math.round(hz) + '' : hz.toFixed(1)) + 'Hz';
const fMtNote = hz => { const m = 69 + 12 * Math.log2(hz / 440), n = Math.round(m), c = Math.round((m - n) * 100); return noteOct(n) + (Math.abs(c) >= 5 ? (c > 0 ? '+' : '−') + Math.abs(c) : ''); };
const fMtPch = v => { const st = (v - 0.85) * 48; return Math.abs(st) < 0.05 ? '0' : (st > 0 ? '+' : '−') + Math.abs(st).toFixed(1) + 'st'; };
const MT_TUN_SC = j => j < 2 ? ['30', '600'] : j < 6 ? ['60', '1.2k'] : ['120', '2.4k'];
const mtTunK = j => Object.assign(k('TUN' + (j + 1), v => fMtHz(mtTuneHz(j, v))), { sc: MT_TUN_SC(j) });
const mtShpK = l => Object.assign(k(l, fPct), { sc: ['TRI', 'SQR'] });
const mtPchK = l => Object.assign(k(l, fMtPch), { sc: ['−41', '+7'] });
const mtModK = l => k(l, fOff(fPct)), mtSrcK = l => k(l, fL(['OFF', 'FM', 'LFO']), 3), mtFstK = l => k(l, fL(['SLOW', 'FAST']), 2);
const mtLfoK = l => Object.assign(k(l, v => fHzLow(mtLfoHz(v))), { sc: ['.03', '300'] }), mtDlyK = l => Object.assign(k(l, v => fTime(mtDly(v))), { sc: ['8ms', '1.1s'] });
SIDE_META[MANTRA_M] = [
  mtTunK(0), mtTunK(1), mtTunK(2), mtTunK(3), mtShpK('SHP12'), mtShpK('SHP34'), mtPchK('PCH1'),
  mtTunK(4), mtTunK(5), mtTunK(6), mtTunK(7), mtShpK('SHP56'), mtShpK('SHP78'), mtPchK('PCH2'),
  mtModK('MOD12'), mtSrcK('SRC12'), mtFstK('FST12'), mtModK('MOD34'), mtSrcK('SRC34'), mtFstK('FST34'), k('LOOP', fL(['2 LOOP', '1 LOOP']), 2),
  mtModK('MOD56'), mtSrcK('SRC56'), mtFstK('FST56'), mtModK('MOD78'), mtSrcK('SRC78'), mtFstK('FST78'), k('TOTL', fOnOff, 2),
  mtLfoK('FRQA'), mtLfoK('FRQB'), k('LGIC', fL(['OR', 'AND']), 2), k('LINK', fOnOff, 2), k('VIB', fOnOff, 2), k('VDPT', fPct), k('SAG', fOff(fPct)),
  mtDlyK('TIM1'), mtDlyK('TIM2'), k('DMD1', fOff(fPct)), k('DMD2', fOff(fPct)), k('DSRC', fL(['LFO', 'SELF']), 2), k('DWAV', fL(['TRI', 'SQR']), 2),
  Object.assign(k('DMIX', fPct), { sc: ['DRY', 'WET'] }),
  Object.assign(k('DSTM', fPct), { sc: ['DRY', 'WET'] }), k('VOL', fPct), k('BUS', fOff(fPct)), k('BLED', fOff(fPct)), k('TOL', fOff(fPct)),
  k('DRFT', fOff(fPct)), k('NOIS', fOff(fPct))];
SIDE_N[MANTRA_M] = 8; SIDE3.add(MANTRA_M); SIDE_OWN[MANTRA_M] = SP(49);
// v87: RADIO's AIR (core/sk_radio.h): BAND (the receiver's bandwidth), CNTR (where the band sits when it narrows), FADE
// (the signal fading in and out, a notch wandering through it), RATE (how fast), HET (a carrier's whistle beating against
// it), CRKL (crackle, a storm's crashes far away), DRIV (the detector and the small speaker pushed)
SIDE_PAGES[RADIO_M] = [{ id: 'SYN', side: 'AIR', ps: [...SP(7), P.F] }];
SIDE_META[RADIO_M] = [Object.assign(k('BAND', fRBand), { sc: ['FULL', '250'] }), Object.assign(k('CNTR', v => fKHz(300 * Math.pow(10, v))), { sc: ['300', '3k'] }),
  k('FADE', fOff(fPct)), Object.assign(k('RATE', v => fHzLow(0.04 * Math.pow(100, v))), { sc: ['.04', '4Hz'] }),
  k('HET', fOff(fPct)), k('CRKL', fOff(fPct)), k('DRIV', fOff(fPct))];
SIDE_N[RADIO_M] = 2; SIDE3.add(RADIO_M); SIDE_OWN[RADIO_M] = SP(7);
// TUNE (LIVE: SHFT), DEC, LEN (LIVE: GATE) and D (SQL, GRAB, DPTH) by MODE
const RADIO_DEC = k('DEC', v => v >= 0.99 ? 'HOLD' : fSec(0.02 * Math.pow(400, v)));
const RADIO_SHFT = Object.assign(k('SHFT', fRShift), { bip: true, sc: ['−600', '+600', '0'] });
const RADIO_D = [MACH[RADIO_M].p[3], Object.assign(k('GRAB', v => v < 0.005 ? 'KEEP' : fPct(v)), { sc: ['KEEP', 'NEW'] }),
  Object.assign(k('DPTH', v => fSec(Math.pow(20, v))), { sc: ['1s', '20s'] })];
const RADIO_LEN = k('LEN', fL(RADIO_LENS), 11);
function radioMeta(t, p) {
  if (p === P.DEC) return RADIO_DEC;
  if (p !== P.TUNE && p !== P.C && p !== P.D) return null;
  const mode = idx(t === ui.t ? shownValue(P.B).v : state.tracks[t].p[P.B], 3);
  if (p === P.TUNE) return mode === 0 ? RADIO_SHFT : null;
  if (p === P.C) return mode === 0 ? null : RADIO_LEN;
  return RADIO_D[mode];
}
// DEC: the slow release (a FAST pair has its own, short); a voice's TUNE as the note it sounds (its group's PITCH and
// the track's TUNE in it)
const MANTRA_DEC = k('DEC', v => fSec(mtRel(v)));
const MT_TUNQ = [0, 1, 2, 3, 7, 8, 9, 10];
function mantraMeta(t, p) {
  if (p === P.DEC) return MANTRA_DEC;
  const j = MT_TUNQ.findIndex(q => SQ(q) === p);
  if (j < 0) return null;
  const val = q => (t === ui.t ? shownValue(q).v : state.tracks[t].p[q]);
  const f = Math.pow(2, (val(SQ(j < 4 ? 6 : 13)) - 0.85) * 4 + (val(P.TUNE) - 0.5) * 4);
  return Object.assign(k('TUN' + (j + 1), v => fMtNote(mtTuneHz(j, v) * f)), { sc: MT_TUN_SC(j) });
}
// ROOT (TUNE): the chord's root as it sounds (DROP's octaves in it), on the scale when the track is quantised; SUST (DEC)
// how long the strings ring, ∞ at its top
const HOWL_SUST = k('SUST', v => v >= 0.99 ? '∞' : fSec(0.4 * Math.pow(2, v * 6.3)));
// (the scale's note nearest, as the engine's qPitch picks it: the lower one on a tie)
const qSnap = (n, mask) => { if (!(mask & 0xFFF)) return n; for (let d = 0; d <= 6; d++) { if (mask >> (((n - d) % 12 + 12) % 12) & 1) return n - d; if (mask >> (((n + d) % 12 + 12) % 12) & 1) return n + d; } return n; };
function howlMeta(t, p) {
  if (p === P.DEC) return HOWL_SUST;
  if (p !== P.TUNE) return null;
  const val = q => (t === ui.t ? shownValue(q).v : state.tracks[t].p[q]);
  const d = idx(val(P.C), 5), oct = d === 2 || d === 3 ? -12 : d === 4 ? -24 : 0;
  const mask = state.tracks[t].q !== 0 && quantOn() ? (qFollow() === t ? state.scale | 0 : qLive()) : 0;   // (its own chord followed: the scale)
  return { l: 'ROOT', f: v => noteOct(qSnap(40 + Math.round((v - 0.5) * 48), mask) + oct) };
}
// v79: WEST by what you hear: seven pages (F; WS_PAGES: PITCH COLOUR SHAPE LAYER RHYTHM MOTION SPACE) holding every
// place of core/sk_west.cpp, the machine's own A .. F and DEC among them, two rows a page: what you reach for (big; the
// envelope's times and the five stages on sliders) and the fine tuning (small; a CV's source beside its depth, the depth
// drawn in the source's colour). Every control named in words, with a hint for the screen when it is touched. The CV
// sources: SEQ (the stages), ENV, MOD (the modulation oscillator), RANDOM, PULSER (its falling ramp), FOLLOW (the
// envelope detector), VELO (the trig's velocity), INVERT, each in its colour
const WS_SRCN = ['SEQ', 'ENV', 'MOD', 'RANDOM', 'PULSER', 'FOLLOW', 'VELO', 'INVERT'];
const WS_SRCC = ['#F3AF1B', '#E9472B', '#5AA2F0', '#3DBE5C', '#B98CF5', '#38C9C6', '#E8EAE9', '#8A9290'];
const WS_ICON = { saw: 'M1 11 L10 1 V11 L19 1', sqr: 'M1 11 V1 H10 V11 H19 V1', tri: 'M1 11 L5.5 1 L10 11 L14.5 1 L19 11', spk: 'M1 11 H7 L10 1 L13 11 H19',
  once: 'M1 11 L4 1 L19 11', hold: 'M1 11 L4 1 H12 L19 11', loop: 'M1 11 L4 1 L9 11 L12 1 L17 11', off: 'M1 6 H19' };
const wsK = (l, f, n, o) => Object.assign(k(l, f, n), o || {});
const wsBipK = (l, lo, hi, zero, o) => wsK(l, fWsBip(lo, hi, zero), 0, Object.assign({ bip: true, sc: [lo, hi, zero] }, o));
// a CV's source (a jack: tap for the next one, a dot for that one) and its depth beside it, in the source's colour
const wsSrcK = (l, hint, n) => wsK(l, fL(WS_SRCN.slice(0, n || 8)), n || 8, { src: true, sw: true, pair: n ? '' : 'l', hint });
const wsStK = (n, q, rl) => wsK('DEPTH', fWsStN(n), 0, { bip: true, sc: ['−' + n, '+' + n, 'OFF'], tint: q, pair: 'r', rl, hint: 'HOW FAR THE SOURCE MOVES IT, UP OR DOWN' });
const wsDepK = (q, rl, lo, hi) => wsK('DEPTH', fWsBip(lo || '−', hi || '+', 'OFF'), 0, { bip: true, sc: [lo || '−', hi || '+', 'OFF'], tint: q, pair: 'r', rl, hint: 'HOW FAR THE SOURCE MOVES IT, UP OR DOWN' });
SIDE_META[WEST_M] = [
  /* 0 */ wsK('BLEND', fNear(['SINE', 'WAVE']), 0, { sc: ['SINE', 'WAVE'], hint: 'SLIDES THE SINE TO THE OTHER WAVE, UNFOLDED' }),
  /* 1 */ wsK('FOLDER', fL(['SINE', 'CELL']), 2, { hint: 'SINE: FOLDS ROUND · CELL: FIVE DIODE CELLS' }),
  /* 2 */ wsK('GLIDE', fWsGlid, 0, { sc: ['OFF', '.9s'], hint: 'SLIDES FROM ONE NOTE TO THE NEXT' }),
  /* 3 */ wsK('DRIFT', fOff(fPct), 0, { hint: 'A SLOW WANDER OF THE PITCH' }),
  /* 4 */ wsDepK(39, 'TIMBRE CV'),
  /* 5 */ wsK('VELOCITY', fOff(fPct), 0, { hint: 'HARDER HITS FOLD BRIGHTER' }),
  /* 6 */ wsK('WARMTH', fWsWarm, 0, { sc: ['OPEN', '1.3k'], hint: 'SOFTENS THE TOP OF THE FOLDS' }),
  /* 7 */ wsK('MOD WAVE', fL(['SAW', 'SQR', 'TRI']), 3, { icons: ['saw', 'sqr', 'tri'], hint: "THE MOD OSC'S WAVE" }),
  /* 8 */ wsK('MOD DOES', fL(['FM', 'AM', 'BAL']), 3, { hint: 'FM: THE PITCH · AM: THE LEVEL · BAL: GATE 2' }),
  /* 9 */ wsK('MOD TRACK', fL(['NOTE', 'FREE']), 2, { hint: 'NOTE: A RATIO OF IT · FREE: ITS OWN RATE' }),
  /* 10 */ wsK('MOD FINE', fWsCt(100), 0, { bip: true, sc: ['−', '+', '0'], hint: 'DETUNES THE MOD OSC AGAINST ITS RATIO' }),
  /* 11 */ wsDepK(40, 'INDEX CV'),
  /* 12 */ wsK('FROM MOD', fOff(fPct), 0, { hint: 'THE MOD OSC MOVES THE TIMBRE' }),
  /* 13 */ wsK('OUT 2', fWsDb, 0, { sc: ['OFF', '+6'], hint: 'HOW LOUD THE SECOND LAYER IS' }),
  /* 14 */ wsK('COUNT', fL(WS_CNTL), 17, { sc: ['+1', '∞'], hint: 'HOW MANY HITS FOLLOW EACH TRIG' }),
  /* 15 */ wsBipK('BOUNCE', 'SPRD', 'BNCE', 'EVEN', { hint: 'THE HITS SPREAD OUT OR BOUNCE CLOSER' }),
  /* 16 */ wsBipK('ACCENT', 'UP', 'DOWN', 'FLAT', { hint: 'THE HITS FADE DOWN OR SWELL UP' }),
  /* 17 */ wsK('SWING', fOff(v => Math.round(50 + 25 * v) + '%'), 0, { hint: 'EVERY OTHER HIT A LITTLE LATE' }),
  /* 18 */ wsK('CHANCE', fPct, 0, { hint: 'THE CHANCE EACH HIT PLAYS' }),
  /* 19 */ wsK('JITTER', fOff(fPct), 0, { hint: 'LOOSENS THE TIMING' }),
  /* 20 */ wsK('RANDOM', fOff(fPct), 0, { hint: 'A RANDOM TIMBRE ON EACH HIT' }),
  /* 21 .. 25: the stages (westMeta) */ k('1'), k('2'), k('3'), k('4'), k('5'),
  /* 26 */ wsK('LENGTH', fI(1, 5), 5, { hint: 'HOW MANY STAGES PLAY' }),
  /* 27 */ wsK('GOES TO', fL(['PITCH', 'TIMBRE', 'INDEX', 'LEVEL']), 4, { hint: 'WHAT THE STAGES PLAY DIRECTLY' }),
  /* 28 */ wsK('GATE 1', fL(['LPG', 'RESO', 'VCA', 'LPF']), 4, { hint: 'LPG: LOUDER AND BRIGHTER TOGETHER' }),
  /* 29 */ wsK('RESONANCE', fOff(fPct), 0, { hint: "GATE 1'S RING (RESO, LPF)" }),
  /* 30 */ wsK('ATTACK', fWsAtk, 0, { sc: ['OFF', '2s'], fader: true, hint: 'HOW FAST EACH HIT RISES' }),
  /* 31 */ wsK('SUSTAIN', fWsSus, 0, { sc: ['OFF', '2s'], fader: true, hint: 'HOW LONG THE TOP HOLDS' }),
  /* 32 */ wsK('MODE', fL(['ONCE', 'HOLD', 'LOOP', 'OFF']), 4, { icons: ['once', 'hold', 'loop', 'off'], hint: 'ONCE · HOLD WHILE HELD · LOOP · OFF' }),
  /* 33 */ wsK('MIX', fOff(fPct), 0, { sc: ['DRY', 'WET'], hint: "THE SPRING'S SHARE OF THE SOUND" }),
  /* 34: its T60 (westMeta) */ k('DECAY', fPct),
  /* 35 */ wsK('OTHER WAVE', fL(['TRI', 'SQR', 'SPIKE']), 3, { icons: ['tri', 'sqr', 'spk'], hint: 'THE WAVE THAT BLEND SLIDES TO' }),
  /* 36 */ wsK('KEYS', fL(['FOLLOW', 'FIXED']), 2, { hint: 'FOLLOW: PLAYS THE NOTE · FIXED: TUNE ONLY' }),
  /* 37 */ wsSrcK('PITCH CV', 'WHAT MOVES THE PITCH'),
  /* 38 */ wsStK(24, 37, 'PITCH CV'),
  /* 39 */ wsSrcK('TIMBRE CV', 'WHAT MOVES THE TIMBRE'),
  /* 40 */ wsSrcK('INDEX CV', 'WHAT MOVES THE INDEX'),
  /* 41 */ wsSrcK('FREQ CV', "WHAT MOVES THE MOD OSC'S PITCH"),
  /* 42 */ wsStK(48, 41, 'FREQ CV'),
  /* 43 */ wsK('RANDOM ON', fL(['OFF', 'TRIG', 'PULSER']), 3, { hint: 'WHEN RANDOM DRAWS A NEW VALUE' }),
  /* 44 */ wsSrcK('INVERTS', 'INVERT: THIS SOURCE UPSIDE DOWN', 6),
  /* 45 */ wsK('FIRED BY', fL(['TRIG', 'PULSER', 'STAGE']), 3, { hint: 'WHAT STARTS THE ENVELOPE' }),
  /* 46 */ wsK('CURVE', fCurve, 0, { sc: ['LOG', 'EXP'], hint: 'BENDS THE FALL: LOG HOLDS, EXP DROPS' }),
  /* 47 */ wsK('START', fL(['FREE', 'TRIG', 'STAGE']), 3, { hint: 'TRIG: A BURST A TRIG · FREE: IT RUNS ON' }),
  /* 48 */ wsK('CLOCK', fL(['AUTO', 'TRIG', 'PULSER', 'OFF']), 4, { hint: 'WHAT MOVES THE STAGES ON' }),
  /* 49: the keys under the stages */ k('PULSES', fSpls, 32),
  /* 50 */ wsK('LEVEL 1', fOff(fPct), 0, { hint: 'OPENS GATE 1 BY ITSELF: A DRONE', live: wi => wi[21], liveQ: 51 }),
  /* 51 */ wsSrcK('GATE 1 CV', 'WHAT OPENS GATE 1'),
  /* 52 */ wsDepK(51, 'GATE 1 CV'),
  /* 53 */ wsK('GATE 2', fL(['LPG', 'VCA', 'LPF']), 3, { hint: 'LPG: LOUDER AND BRIGHTER TOGETHER' }),
  /* 54 */ wsK('INPUT', fL(['MOD OSC', 'LOOP', 'GATE 1']), 3, { hint: 'WHAT GATE 2 PLAYS' }),
  /* 55 */ wsK('LEVEL 2', fOff(fPct), 0, { fader: true, hint: 'OPENS GATE 2 BY ITSELF: A DRONE', live: wi => wi[22], liveQ: 56 }),
  /* 56 */ wsSrcK('GATE 2 CV', 'WHAT OPENS GATE 2'),
  /* 57 */ wsDepK(56, 'GATE 2 CV'),
  /* 58 */ wsK('OUT 1', fWsDb, 0, { sc: ['OFF', '+6'], hint: 'HOW LOUD GATE 1 IS' }),
  /* 59 */ wsSrcK('RATE CV', "WHAT MOVES THE PULSER'S RATE"),
  /* 60 */ wsDepK(59, 'RATE CV', 'SLOW', 'FAST'),
  /* 61 */ wsK('SIZE', fPct, 0, { hint: 'HOW BIG THE TANK IS' }),
  /* 62 */ wsK('REFLECT', fPct, 0, { hint: 'THE EARLY REFLECTIONS' }),
  /* 63 */ wsK('SPIN', fPct, 0, { hint: "THE SPRING'S DISPERSION: ITS CHIRP" }),
  /* 64 */ wsK('DAMPING', fPct, 0, { sc: ['BRIT', 'DARK'], hint: 'DARKENS THE TAIL' }),
  /* 65 */ wsK('CHAOS', fOff(fPct), 0, { hint: 'THE DELAYS WANDER' }),
  /* 66 */ wsK('SHAKE', fOff(fPct), 0, { hint: 'KNOCKS THE TANK ON EVERY HIT' }), null, null, null];
// (a depth's colour: its source's place, a side place index)
SIDE_META[WEST_M].forEach(m => { if (m && m.tint != null) m.tint = SQ(m.tint); if (m && m.liveQ != null) m.liveQ = SQ(m.liveQ); });
{ const M = MACH[WEST_M].p;
  M[0].sc = ['SINE', 'FOLD']; M[4].sc = ['OFF', '1/2']; M[5].sw = true;
  Object.assign(M[0], { hint: 'FOLDS THE WAVE OVER ITSELF: MORE = BRIGHTER',
    live: wi => { const A = wi[6]; return idx(shownValue(SQ(1)).v, 2) === 0 ? (A / 8 - 0.125) / 3 : Math.sqrt(Math.max(0, (A - 0.55) / 7.5)); }, liveQ: SQ(39) });
  Object.assign(M[1], { hint: 'TILTS THE FOLDS: EVEN HARMONICS' });
  Object.assign(M[2], { hint: 'HOW HARD THE MOD OSC SHAKES THE WAVE', live: wi => wi[7], liveQ: SQ(40) });
  Object.assign(M[3], { hint: "THE MOD OSC'S PITCH AGAINST THE NOTE" });
  Object.assign(M[4], { hint: 'A BURST OF HITS ON THE TEMPO AFTER EACH TRIG' }); }
// the pages: what you reach for, the fine tuning (a CV's source beside its depth: never split across a row of four or six)
const WS_PAGES = [
  { id: 'PITCH', icon: 'M2 12 H8 V4 H18 V9 H24', say: 'Which note it plays and how it gets there: the tune, the mod osc against it, a glide between hits.',
    main: [P.TUNE, P.D, SQ(2), SQ(36)], fine: [SQ(37), SQ(38), SQ(41), SQ(42), SQ(3), SQ(10), SQ(9)] },
  { id: 'COLOUR', icon: 'M2 8 C4 1 6 1 7 6 C8 12 10 12 11 6 C12 0 15 0 16 6 C17 12 19 12 20 6 C21 1 23 1 24 8', say: 'How bright and metallic each hit sounds: the folder bends the sine, the index makes it ring.',
    main: [P.A, P.B, P.C, SQ(0)], fine: [SQ(1), SQ(6), SQ(35), SQ(7), SQ(8), SQ(5), SQ(12), SQ(20), SQ(39), SQ(4), SQ(40), SQ(11)] },
  { id: 'SHAPE', icon: 'M2 14 L6 2 L11 7 L17 7 L24 14', say: 'How each hit rises and falls: the envelope draws it, gate 1 makes it louder and brighter together.',
    main: [SQ(30), SQ(31), P.DEC, SQ(46)], fine: [SQ(32), SQ(45), SQ(28), SQ(50), SQ(29), SQ(58), SQ(51), SQ(52)] },
  { id: 'LAYER', icon: 'M2 5 H24 M2 11 H24 M6 2 V8 M18 8 V14', say: 'A second layer through gate 2, on top of the first: the mod osc, its own loop or gate 1.',
    main: [SQ(13), SQ(54), SQ(53), SQ(55)], fine: [SQ(56), SQ(57)] },
  { id: 'RHYTHM', icon: 'M2 13 H5 V3 H6 V13 H11 V5 H12 V13 H17 V7 H18 V13 H24', say: 'The burst after each trig: how fast, how many hits, how they spread and fade.',
    main: [P.E, SQ(14), SQ(15), SQ(16)], fine: [SQ(47), SQ(17), SQ(18), SQ(19), SQ(59), SQ(60)] },
  { id: 'MOTION', icon: 'M3 14 V9 M8 14 V4 M13 14 V10 M18 14 V2 M23 14 V7', say: 'Five stages, one a hit: draw a line with the sliders, choose what it moves. A lit key fires a pulse.',
    main: [SQ(21), SQ(22), SQ(23), SQ(24), SQ(25), SQ(27)], fine: [SQ(26), SQ(48), SQ(43), SQ(44)] },
  { id: 'SPACE', icon: 'M2 8 C4 1 6 1 6 8 C6 15 8 15 8 8 C8 1 10 1 10 8 C10 15 12 15 12 8 C12 1 14 1 14 8 H24', say: 'The spring tank after the gates: how much, how big, how long; shake it on every hit.',
    main: [SQ(33), SQ(61), SQ(34), SQ(66)], fine: [SQ(62), SQ(63), SQ(64), SQ(65)] }];
WS_PAGES.forEach(pg => { pg.mainPg = { id: 'SYN', ps: pg.main }; pg.finePg = { id: 'SYN', ps: pg.fine }; });
SIDE_PAGES[WEST_M] = [];      // (the machine's knobs stay on MAIN: the pages are WEST's own panel, bank w)
SIDE_N[WEST_M] = WS_PAGES.length; SIDE3.add(WEST_M); SIDE_OWN[WEST_M] = SP(NSIDE);
// its DEC (5 ms .. 7 s, HOLD at its top); MOD's DTUN, its RATE when the modulator is FREE; the steps: semitones on PTCH,
// a share of the knob elsewhere; the spring's decay, its T60 on its SIZE
const WEST_DEC = Object.assign(k('DECAY', fWsDec), { fader: true, hint: 'HOW LONG EACH HIT RINGS' });
const WS_RATE = Object.assign(k('MOD RATE', v => fHzLow(0.1 * Math.pow(20000, v))), { sc: ['.1Hz', '2k'], hint: "THE MOD OSC'S OWN RATE" });
const fWsSt = v => { const st = Math.round((v - 0.5) * 24); return st === 0 ? '0' : (st > 0 ? '+' : '−') + Math.abs(st) + 'st'; };
const fWsShare = v => { const e = Math.round((v - 0.5) * 200); return e === 0 ? '0' : (e > 0 ? '+' : '−') + Math.abs(e); };
const WS_STEPK = [0, 1, 2, 3, 4].map(i => [Object.assign(k(String(i + 1), fWsSt), { bip: true, sc: ['−12', '+12', '0'], fader: true, stage: i, rl: 'STAGE ' + (i + 1), hint: 'STAGE ' + (i + 1) + ': ITS NOTE' }),
  Object.assign(k(String(i + 1), fWsShare), { bip: true, sc: ['−', '+', '0'], fader: true, stage: i, rl: 'STAGE ' + (i + 1), hint: 'STAGE ' + (i + 1) + ': ITS VALUE' })]);
function westMeta(t, p) {
  if (p === P.DEC) return WEST_DEC;
  const val = q => (t === ui.t ? shownValue(q).v : state.tracks[t].p[q]);
  if (p === P.S11 && idx(val(P.S10), 2) === 1) return WS_RATE;
  if (p >= P.S22 && p <= P.S26) return WS_STEPK[p - P.S22][idx(val(P.S28), 4) === 0 ? 0 : 1];
  if (p === P.S35) { const size = val(SQ(61)); return Object.assign(k('DECAY', v => fSec(wsT60(v, size))), { sc: [fSec(wsT60(0, size)), fSec(wsT60(1, size))], hint: 'HOW LONG THE SPRING RINGS' }); }
  return null;
}
// its Z: the bank's name where Z sits on one (the factory's, or the one the LOAD panel read there when SET is USER),
// else the place between two; DEC 10 ms .. 4 s, HOLD at its top
const CUBE_DEC = k('DEC', v => v >= 0.99 ? 'HOLD' : fSec(0.01 * Math.pow(400, v)));
function cubeMeta(t, p) {
  if (p === P.DEC) return CUBE_DEC;
  if (p !== P.C) return null;
  const user = idx(t === ui.t ? shownValue(P.S7).v : state.tracks[t].p[P.S7], 2) === 1;
  return k('Z', v => { const z = v * 7, i = Math.round(z); return Math.abs(z - i) < 0.05 ? cubeBankLabel(i, user) : (1 + z).toFixed(1); });
}
const side3 = t => { const m = mof(state.tracks[t].p[P.MACH]); return SIDE3.has(m) ? idx(state.tracks[t].p[P.F], SIDE_N[m]) : -1; };
const haloSide = t => mof(state.tracks[t].p[P.MACH]) === HALO_M ? idx(state.tracks[t].p[P.F], SIDE_N[HALO_M]) : -1;
const sidePage = () => { if (ui.all) return null; const s = side3(ui.t); return s > 0 ? SIDE_PAGES[mof(trk().p[P.MACH])][s - 1] : null; };
const HALO_REL = k('REL', v => fSec(0.05 * Math.pow(400, v)));
const STRUM_DEC = k('DEC', v => v >= 0.99 ? 'HOLD' : fSec(0.25 * Math.pow(2, v * 6.7)));
const CHOIR_REL = k('REL', v => fSec(0.05 * Math.pow(2, v * 7)));
// BREAK switched to CHOP: what each trig may do instead (F stays the switch)
const BRK_CHOP_PS = [P.BSWP, P.BRND, P.BREV, P.BSTU, P.BPJM, P.BSKP, P.BSED];
const BRK_CHOP_PAGE = { id: 'SYN', brkChop: true, ps: [...BRK_CHOP_PS, P.F] };
const brkSide = t => mof(state.tracks[t].p[P.MACH]) === BREAK_M ? idx(state.tracks[t].p[P.F], 2) : -1;
const brkChop = () => !ui.all && brkSide(ui.t) === 1;
// an FX machine switched to MORE: its seven other knobs, ACT last (F stays the switch)
const FX_MORE_PAGE = { id: 'SYN', fxMore: true, ps: [P.XF, P.XG, P.XA, P.XB, P.XC, P.XD, P.XE, P.F] };
// (v68: FUZZ's, its bands in a row: LOW MID HIGH GATE, then the crossovers, ACT)
const FUZZ_MORE_PAGE = { id: 'SYN', fxMore: true, ps: [P.XA, P.XB, P.XC, P.XG, P.XD, P.XF, P.XE, P.F] };
// (v83: STENCIL's: µ HALO SKEW PING NOIS SYNC ACT; its SPACE side: the halo, µ's speed, DUCK, WIDE)
const STC_MORE_PAGE = { id: 'SYN', fxMore: true, ps: [P.XA, P.XB, P.XC, P.XF, P.XG, P.XD, P.XE, P.F] };
const STC_SPACE_PAGE = { id: 'SYN', ps: [P.S1, P.S2, P.S3, P.S4, P.S5, P.S6, P.S7, P.F] };
const stcSpace = () => !ui.all && curMachine() === STENCIL_M && fxSide(ui.t) === 2;
// (v78: WEFT's: LVL1 LVL2 MODE, ACT)
const WEFT_MORE_PAGE = { id: 'SYN', fxMore: true, ps: [P.XA, P.XB, P.XC, null, null, null, P.XE, P.F] };
// (v71: its SUB page, the octaver: SUB1 SUB2 GRIT PATH; v73: SUST beside it, the wall)
const FUZZ_SUB_PAGE = { id: 'SYN', fxSub: true, ps: [P.S1, P.S2, P.S3, P.S4, P.D, null, null, P.F] };
// (v73: its MAIN page, the circuit first: SRC CIRC DRIV BIAS, then TONE DEC MIX)
const FUZZ_MAIN_PAGE = { id: 'SYN', ps: [P.A, P.S5, P.B, P.C, P.TUNE, P.DEC, P.E, P.F] };
const fuzzMain = () => !ui.all && curMachine() === FUZZ_M && fxSide(ui.t) === 0;
// the TRIG row, its MORE page (v58): NOTE and the retrig where they are, JUMP and its range, the retrig's curve
// (v64: RPAN in the first place, NOTE on the TRIG page)
// (v65: RDEC in GATE's place, GATE on the TRIG page under it)
const TRG_MORE_PAGE = { id: 'TRG', seq: true, more: true, ps: [P.RPAN, P.JUMP, P.JRNG, P.RCRV, P.RTRG, P.RCNT, P.RVEL, P.RDEC] };
// v64: TONE's MORE page (the track strip's MORE key, on TONE): the spread and its crossover
const TONE_MORE_PAGE = { id: 'TONE', more: true, ps: [P.SPRD, P.SMON, null, null, null, null, null, null] };
// v65: the LFO page's MORE page, under each LFO's SPD and DEP: SLEW's glide and LOOP's length
// the track pages with a MORE page (the track strip's MORE key, on them), and whether it is shown
const MORE_PAGES = { TONE: TONE_MORE_PAGE };
const moreOn = id => id === 'TONE' ? !!ui.tonemore : false;
// (v71: FUZZ three sides, MAIN SUB MORE)
const fxSide = t => { const m = mof(state.tracks[t].p[P.MACH]); return fxM(m) ? idx(state.tracks[t].p[P.F], SIDE_N[m] || 2) : -1; };
const fxMore = () => !ui.all && fxSide(ui.t) === (curMachine() === FUZZ_M ? 2 : 1);
const fuzzSub = () => !ui.all && curMachine() === FUZZ_M && fxSide(ui.t) === 1;
// DEC: how long a trig opens the effect (IN) or holds its gesture; at the top HOLD (IN: always open; a gesture: a trig
// starts it, the next one stops it)
const FX_DEC = k('DEC', v => v >= 0.99 ? 'HOLD' : fSec(0.02 * Math.pow(300, v)));
const fxActK = m => k('ACT', fL(FX_ACTS[fxI(m)]), FX_ACTS[fxI(m)].length);
const fCol = (lo, hi) => v => { const c = (v - 0.5) * 2; return Math.abs(c) < 0.03 ? 'FLAT' : (c < 0 ? lo : hi) + ' ' + Math.round(Math.abs(c) * 100); };
const fHzLow = hz => (hz < 1 ? hz.toFixed(2) : hz < 10 ? hz.toFixed(1) : Math.round(hz)) + 'Hz';
const FX_SYNC = k('SYNC', fL(['FREE', 'SYNC']), 2);
// ECHO's zones (s), its divisions on the tempo (16ths) and their names; BEND's divisions
const EZ_LO = [0.0015, 0.03, 0.25, 1.0], EZ_HI = [0.03, 0.35, 1.6, 5.0];
const EDIV = [0.5, 2 / 3, 1, 4 / 3, 1.5, 2, 8 / 3, 3, 4, 16 / 3, 6, 8, 12, 16, 32];
const EDIV_L = ['1/32', '1/16T', '1/16', '1/8T', '1/16D', '1/8', '1/4T', '1/8D', '1/4', '1/2T', '1/4D', '1/2', '1/2D', '1 BAR', '2 BAR'];
const BDIV_L = ['1/32', '1/16', '1/8', '1/4', '1/2', '1 BAR'];
const fxBpm = () => (inPlugin && ui.pos.bpm > 0 ? ui.pos.bpm : state.g[G.BPM]) || 120;
// ECHO's TIME as the engine reads it: in its zone, or (SYNC, but KARP) the division nearest
function echoTime(zone, sync, v) {
  let T = EZ_LO[zone] * Math.pow(EZ_HI[zone] / EZ_LO[zone], v);
  if (sync && zone >= 1) {
    const spt = 60 / fxBpm() / 4, t16 = T / spt;
    let best = 0; for (let i = 1; i < EDIV.length; i++) if (Math.abs(Math.log2(EDIV[i] / t16)) < Math.abs(Math.log2(EDIV[best] / t16))) best = i;
    return { T: EDIV[best] * spt, div: best };
  }
  return { T, div: -1 };
}
// v83, STENCIL (core/sk_stencil stencilTime, stencilSkew): its zones (s: RATE from the longest, at its left, to the
// shortest), the tempo's divisions (16ths) and their names, SKEW's ratios on SYNC
const SZ_LO = [0.0003, 0.004, 0.03, 0.12, 0.35, 0.8, 1.5, 2.5], SZ_HI = [0.012, 0.045, 0.22, 0.6, 1.2, 2.0, 3.5, 5.4];
const SDIV = [0.25, 0.5, 2 / 3, 1, 4 / 3, 1.5, 2, 8 / 3, 3, 4, 16 / 3, 6, 8, 12, 16, 24, 32, 64];
const SDIV_L = ['1/64', '1/32', '1/16T', '1/16', '1/8T', '1/16D', '1/8', '1/4T', '1/8D', '1/4', '1/2T', '1/4D', '1/2', '1/2D', '1 BAR', '1.5 BAR', '2 BAR', '4 BAR'];
const SSKEW = [0.5, 2 / 3, 0.75, 1, 4 / 3, 1.5, 2], SSKEW_L = ['×1/2', '×2/3', '×3/4', '×1', '×4/3', '×3/2', '×2'];
// the left line's time (s), and its division (SYNC; -1: free) or its semitone (KARP, COMB on SYNC)
function stcTime(zone, sync, rate, note) {
  zone = clamp(zone, 0, 7);
  let T = SZ_HI[zone] * Math.pow(SZ_LO[zone] / SZ_HI[zone], clamp(rate, 0, 1));
  if (zone <= 1) {
    let midi = -1;
    if (sync) { midi = Math.round(69 + 12 * Math.log2(1 / (T * 440))); T = 1 / (440 * Math.pow(2, (midi - 69) / 12)); }
    T = clamp(T * Math.pow(2, -(note || 0) / 12), 0.0001, 0.2);
    return { T, div: -1, midi };
  }
  if (sync) {
    const spt = 60 / fxBpm() / 4, t16 = T / spt;
    let best = 0, bd = 1e9;
    for (let i = 0; i < SDIV.length; i++) { if (SDIV[i] * spt > 5.4) break; const d = Math.abs(Math.log2(SDIV[i] / t16)); if (d < bd) { bd = d; best = i; } }
    return { T: SDIV[best] * spt, div: best, midi: -1 };
  }
  return { T, div: -1, midi: -1 };
}
const stcSkew = (v, sync) => { const sk = clamp((v - 0.5) * 2, -1, 1); return sync ? SSKEW[clamp(Math.floor((sk + 1) * 0.5 * 7), 0, 6)] : Math.pow(2, 0.585 * sk); };
const fStcSkew = (v, sync) => { if (sync) return 'R' + SSKEW_L[clamp(Math.floor(((v - 0.5) * 2 + 1) * 0.5 * 7), 0, 6)]; const r = stcSkew(v, false); return Math.abs(r - 1) < 0.005 ? 'R×1' : 'R×' + r.toFixed(2); };
const fStcTime = (zone, sync, v) => { const e = stcTime(zone, sync, v, 0);
  return e.div >= 0 ? SDIV_L[e.div] : e.midi >= 0 ? NOTE_NAMES[((e.midi % 12) + 12) % 12] + (Math.floor(e.midi / 12) - 1) : zone <= 1 ? fHzLow(1 / e.T) : fTime(e.T); };
const STC_SKEW_SYNC = Object.assign(k('SKEW', v => fStcSkew(v, true)), { bip: true, sc: ['×1/2', '×2', '×1'] });
// (core/sk_fx fxVerbGain)
// FUZZ: a band's drive (0: CLEAN, the band past the circuit)
const fBand = v => v < 0.02 ? 'CLEAN' : fDbS(-48 + 66 * v - 0);
const verbGain = v => v >= 0.8 ? 1 + 0.25 * (v - 0.8) / 0.2 : 1 - 0.92 * Math.pow(1 - v / 0.8, 1.5);
const FXK = [
  // VERB: TILT (+-6 dB around 650 Hz), the lines' modulation (SPED 0.1 .. 30 Hz, DPTH up to their whole length, SHAP from
  // the sine to grains of random lengths), PRE-delay
  { [P.TUNE]: k('TILT', fDb(6)), [P.XA]: k('SPED', v => fHzLow(0.1 * Math.pow(300, v))), [P.XB]: k('DPTH', fOff(v => Math.round(v * v * 97) + '%')),
    [P.XC]: k('PRE', v => v < 0.002 ? '0ms' : fTime(v * v * 0.5)), [P.XD]: k('SHAP', v => v < 0.02 ? 'SINE' : v > 0.98 ? 'RAND' : 'RND ' + Math.round(v * 100) + '%'),
    // BASS: under 220 Hz the tail lasts up to 4 times longer (its body); SHFT: half of four lines shifted a pass
    [P.XF]: k('BASS', v => '×' + Math.pow(4, v).toFixed(2)), [P.XG]: k('SHFT', fL(VERB_SHIFT), 13) },
  // ECHO: COLR (left a dark, saturated tape, right a thin bright line), µ (its drift), HALO (repeats into a cloud), SKEW (R later)
  { [P.TUNE]: k('COLR', fCol('DARK', 'THIN')), [P.XA]: k('µ', fOff(fPct)), [P.XB]: k('HALO', fOff(fPct)),
    [P.XC]: k('SKEW', fOff(v => 'R+' + Math.round(35 * v) + '%')), [P.XD]: FX_SYNC,
    // PING: the repeats cross sides; DUCK: they step back while the source plays
    [P.XF]: k('PING', fOff(fPct)), [P.XG]: k('DUCK', fOff(fPct)) },
  // BEND: the bend (+-2 octaves), CRPT and its mode, SPRD (the replays' stereo smear)
  { [P.TUNE]: k('BEND', fSt(24)), [P.XA]: k('CRPT', fOff(fPct)), [P.XB]: k('CMOD', fL(['DECIM', 'DROP', 'DSTR']), 3),
    [P.XC]: k('SPRD', fOff(fPct)), [P.XD]: FX_SYNC },
  // TAPE: TONE (+-6 dB around 900 Hz), the generations, the failures, the hiss, the machine
  { [P.TUNE]: k('TONE', fDb(6)), [P.XA]: k('GEN', fOff(fPct)), [P.XB]: k('FAIL', fOff(fPct)), [P.XC]: k('HISS', fOff(fPct)),
    [P.XD]: k('MACH', fL(['CASS', 'VHS', 'REEL', 'MICRO']), 4),
    // WIDE: the right track's wow and flutter its own, its head a little out of azimuth
    [P.XF]: k('WIDE', fOff(fPct)) },
  // CODEC: FILT (left a low-pass, right a high-pass), JITR (phase errors), PKTS (packets lost), MODE, RESO
  { [P.TUNE]: k('FILT', v => { const f = (v - 0.5) * 2; return Math.abs(f) < 0.02 ? 'FLAT' : f < 0 ? 'LP ' + fKHz(20000 * Math.pow(0.015, -f)) : 'HP ' + fKHz(20 * Math.pow(200, f)); }),
    [P.XA]: k('JITR', fOff(fPct)), [P.XB]: k('PKTS', fOff(fPct)), [P.XC]: k('MODE', fL(['STD', 'INV']), 2), [P.XD]: k('RESO', fOff(fPct)),
    // JOIN: joint stereo, the side's bits go first, then above a frequency only how far each band leans
    [P.XF]: k('JOIN', fOff(fPct)) },
  // FUZZ (v68): TONE (a tilt of +-9 dB about 1 kHz), each band's drive (0: CLEAN, past the circuit; then -48 .. +18 dB
  // from FUZZ's), the crossovers (60 .. 600 Hz, 0.9 .. 8.1 kHz), GATE (keyed on the source: its threshold)
  { [P.TUNE]: k('TONE', fDb(9)), [P.XA]: k('LOW', fBand), [P.XB]: k('MID', fBand), [P.XC]: k('HIGH', fBand),
    [P.XD]: k('XLO', v => fKHz(60 * Math.pow(10, v))), [P.XF]: k('XHI', v => fKHz(900 * Math.pow(9, v))),
    [P.XG]: k('GATE', v => v < 0.02 ? 'OFF' : Math.round(-72 + 50 * v) + 'dB'),
    // v71, its SUB side (core/sk_fuzz): an octaver on the LOW band's fundamental, one octave down (SUB1) and two (SUB2),
    // each the square of its knob (50 %: -12 dB); GRIT from a pure sine to the band's own waveform at the octave; PATH:
    // CLEAN under the fuzz, or FUZZ into the LOW band's circuit
    [P.S1]: k('SUB1', fOff(fPct)), [P.S2]: k('SUB2', fOff(fPct)), [P.S3]: k('GRIT', v => v < 0.005 ? 'PURE' : fPct(v)),
    [P.S4]: k('PATH', fL(['CLEAN', 'FUZZ']), 2),
    // v73, CIRC (S5): the circuit (core/sk_fuzz: FUZZ the germanium one, then from the gentlest)
    [P.S5]: k('CIRC', fL(FZ_CIRCS), 8) },
  // WEFT (v78): NOTE (the internal carrier's, C1 .. C5), LVL1 (the carrier's drive; SHFT: the feedback), LVL2 (the
  // modulator's; SHFT: dry .. wet inside the shifter), MODE (WARP, SHFT)
  { [P.TUNE]: k('NOTE', fWeftNote), [P.XA]: k('LVL1', fPct), [P.XB]: k('LVL2', fPct), [P.XC]: k('MODE', fL(['WARP', 'SHFT']), 2) },
  // STENCIL (v83, core/sk_stencil): COLR in the loop (left the dark tape, saturated, its head bump; right a thin bright
  // line), µ (the drift: a flanger in the short zones, a tape's wow in the long ones), HALO (the repeats through the
  // reverb's eight lines), SKEW (the right line's time against the left's; SYNC: in ratios), PING (the repeats cross
  // sides), NOIS (the loop's hiss: past 100 % it starts the loop from nothing). Its SPACE side: the halo's SIZE, DCAY,
  // DAMP, DRFT, then µ's speed, DUCK (the repeats step back while the source plays) and WIDE (the stereo, 50 %: as it is)
  { [P.TUNE]: k('COLR', fCol('DARK', 'BRIT')), [P.XA]: k('µ', fOff(fPct)), [P.XB]: k('HALO', fOff(fPct)),
    [P.XC]: Object.assign(k('SKEW', v => fStcSkew(v, false)), { bip: true, sc: ['×0.67', '×1.5', '×1'] }), [P.XD]: FX_SYNC,
    [P.XF]: k('PING', fOff(fPct)), [P.XG]: k('NOIS', fOff(fPct)),
    [P.S1]: k('SIZE', fPct), [P.S2]: k('DCAY', fPct), [P.S3]: k('DAMP', fPct), [P.S4]: k('DRFT', fOff(fPct)),
    [P.S5]: k('µSPD', v => fHzLow(0.08 * Math.pow(80, v))), [P.S6]: k('DUCK', fOff(fPct)), [P.S7]: k('WIDE', v => Math.round(v * 200) + '%') },
];
FXK[7][P.TUNE].sc = ['DARK', 'BRIT']; FXK[7][P.S7].sc = ['MONO', '200%'];
FXK[5][P.S3].sc = ['PURE', 'GRIT'];
FXK[6][P.TUNE].sc = ['C1', 'C5'];
FXK[5][P.TUNE].sc = ['DARK', 'BRIT']; FXK[5][P.XG].sc = ['OFF', '-22'];
FXK[0][P.TUNE].sc = ['DARK', 'BRIT']; FXK[0][P.XD].sc = ['SINE', 'RAND']; FXK[0][P.XG].sc = ['OFF', '+12']; FXK[0][P.XF].sc = ['×1', '×4']; FXK[1][P.TUNE].sc = ['DARK', 'THIN']; FXK[3][P.TUNE].sc = ['DARK', 'BRIT']; FXK[4][P.TUNE].sc = ['LP', 'HP'];
const WEFT_SHFT_K = Object.assign(k('SHFT', fWeftShift), { bip: true, sc: ['−2k', '+2k', '0'] });
const WEFT_SB_K = Object.assign(k('BAND', fNear(['DOWN', 'BOTH', 'UP'])), { sc: ['DOWN', 'UP'] });
const WEFT_FB_K = k('FEED', fOff(fPct)), WEFT_DW_K = Object.assign(k('D/W', fPct), { sc: ['DRY', 'WET'] });
// what does nothing on an FX track: the voice's filter, drive and crush, its attack, punch and choke, the BOX, NOTE, GATE
// (v78: the amplitude's curve)
const FX_OFF = new Set([P.FTYP, P.FREQ, P.RES, P.FENV, P.FDEC, P.DRV, P.BITS, P.SRR, P.ATK, P.PNCH, P.CHOK, P.NOTE, P.GATE, P.ECRV,
  P.IR, P.IMIX, P.IPRE, P.IDCY, P.IGAT, P.ITON, P.IFB, P.IREV]);
function fxMeta(t, m, p) {
  const K = FXK[fxI(m)], val = q => (t === ui.t ? shownValue(q).v : state.tracks[t].p[q]);
  if (FX_OFF.has(p) && !(m === STENCIL_M && p === P.NOTE)) return OFF_K;     // (v83: STENCIL's NOTE transposes KARP and COMB)
  if (p === P.DEC) return FX_DEC;
  if (p === P.XE) return fxActK(m);
  // WEFT: in SHFT (before its own table: LVL1 LVL2 there), ALGO the frequency shift, TIMB the sidebands, the levels the feedback and the shifter's dry / wet
  if (m === WEFT_M && idx(val(P.XC), 2) === 1) {
    if (p === P.B) return WEFT_SHFT_K;
    if (p === P.C) return WEFT_SB_K;
    if (p === P.XA) return WEFT_FB_K;
    if (p === P.XB) return WEFT_DW_K;
  }
  if (p in K) return K[p];
  // VERB: DECAY is the lines' gain a pass (as on the Erbe-Verb): how long it lasts follows SIZE; from 80 % on it sustains
  // (∞), up to 125 % (it saturates)
  if (m === FX_M0 && p === P.C) {
    const sc = 0.004 * Math.pow(250, val(P.B)), L = (196 * sc + 10.35 * Math.pow(sc, 0.7)) / 1000;
    return k('DCAY', v => { const g = verbGain(v); return g >= 0.9999 ? (g > 1.002 ? '∞ ' + Math.round(g * 100) + '%' : '∞') : fSec(3 * L / -Math.log10(Math.max(g, 1e-4))); });
  }
  // ECHO: TIME in its zone (KARP: as a pitch), or on the tempo
  if (m === FX_M0 + 1 && p === P.C) {
    const zone = idx(val(P.B), 4), sync = idx(val(P.XD), 2) === 1;
    return k('TIME', v => { const e = echoTime(zone, sync, v); return e.div >= 0 ? EDIV_L[e.div] : zone === 0 ? fHzLow(1 / e.T) : fTime(e.T); });
  }
  // STENCIL: RATE in its zone (KARP, COMB: as a pitch; on SYNC its note), or on the tempo; SKEW on SYNC in ratios
  if (m === STENCIL_M && p === P.C) { const zone = idx(val(P.B), 8), sync = idx(val(P.XD), 2) === 1; return k('RATE', v => fStcTime(zone, sync, v)); }
  if (m === STENCIL_M && p === P.XC && idx(val(P.XD), 2) === 1) return STC_SKEW_SYNC;
  // BEND: TIME on the tempo's divisions, or free
  if (m === FX_M0 + 2 && p === P.B) return idx(val(P.XD), 2) === 1 ? k('TIME', fL(BDIV_L), 6) : k('TIME', v => fSec(0.04 * Math.pow(60, v)));
  return null;
}

function trackMachine(t) {
  return t === ui.t ? curMachine() : mof(state.tracks[t].p[P.MACH]);
}
const TRIO_M = 18, TRIO_SWP = k('SWP', v => '+' + Math.round(v * v * 24) + 'st');
const RDEC_NA = { l: 'RDEC', f: () => '—', off: true };
// (v80: DUCK follows track 1: on track 1 itself it has nothing to follow)
const DUCK_T1 = { l: 'DUCK', f: () => '—', off: true, frame: 'T1 LEADS' };
// (v81: DRONE on a machine that cannot hold one)
const DRON_NA = { l: 'DRONE', f: () => '—', off: true };
function paramMeta(t, p) {
  if (p === P.DUCK && t === 0 && !ui.all) return DUCK_T1;
  const m = trackMachine(t);
  if (p === P.DRON && !ui.all && !droneM(m, state.tracks[t].p)) return DRON_NA;
  if (m === HOWL_M) { const r = howlMeta(t, p); if (r) return r; }
  if (m === MANTRA_M) { const r = mantraMeta(t, p); if (r) return r; }
  if (m === RADIO_M) { const r = radioMeta(t, p); if (r) return r; }
  if (p === P.RDEC && fxM(m)) return RDEC_NA;            // (an FX machine has no decay to draw)
  if (fxM(m)) { const r = fxMeta(t, m, p); if (r) return r; }
  if (m === CHIM_M) { const r = chimMeta(t, p); if (r) return r; }
  if (m === CUBE_M) { const r = cubeMeta(t, p); if (r) return r; }
  if (m === WEST_M) { const r = westMeta(t, p); if (r) return r; }
  if (isSide(p)) return sideMeta(t, m, p);
  // TRIO: in SYNC and PMOD the C knob is the envelope sweep; on LP the track filter is its 4-pole
  if (m === TRIO_M && p === P.C) {
    const mode = idx(t === ui.t ? shownValue(P.F).v : state.tracks[t].p[P.F], 4);
    if (mode === 1 || mode === 3) return TRIO_SWP;
  }
  if (m === TRIO_M && p === P.FTYP) return TRIO_FTYP;
  if (p === P.JUMP && jumpDeg(t, m)) return JUMP_DEG;
  if (m === MACRO_M) { const r = macroMeta(t, p); if (r) return r; }
  if (m === RAM_M) { const r = ramMeta(t, p); if (r) return r; }
  if (m === HALO_M && p === P.DEC) return HALO_REL;
  if (m === STRUM_M && p === P.DEC) return STRUM_DEC;
  if (m === CHOIR_M && p === P.DEC) return CHOIR_REL;
  if (m === ALLOY_M && p === P.DEC) return ALLOY_DEC;
  if (p === P.DEC && holdM(m)) return DEC_HOLDK;          // (at its top the note holds while its gate is open)
  if (m === BREAK_M && p === P.DEC) return RAM_DEC;
  if (p >= P.A && p <= P.F) return MACH[m].p[p - P.A];
  if (p === P.LDST || p === P.L2DS) { const names = destNames(m); return { l: p === P.LDST ? 'DST1' : 'DST2', n: NDEST, f: v => names[idx(v, NDEST)] }; }
  if (p === P.E1DS || p === P.E2DS) { const names = destNames(m); return { l: 'DEST', n: NDEST, f: v => names[idx(v, NDEST)] }; }
  if (p === P.IGAT) {
    const irv = t === ui.t ? shownValue(P.IR).v : state.tracks[t].p[P.IR];
    const len = IR_DUR[idx(irv, NIR + 1)] || 1;
    return { l: 'GATE', f: v => v >= 0.999 ? 'FULL' : fSec(0.01 * Math.pow(Math.max(len / 0.01, 1), v)) };
  }
  return TP[p];
}

// ------------------------------------------------------------------ state
function defaultParams(m) {
  const p = new Array(NP).fill(0.5);
  p[P.MACH] = mv(m); p[P.FTYP] = mid(0, 4); p[P.FREQ] = 1; p[P.RES] = 0.1; p[P.FENV] = 0.5; p[P.FDEC] = 0.3;
  p[P.DRV] = 0; p[P.BITS] = 0; p[P.SRR] = 0; p[P.VOL] = 0.8; p[P.PAN] = 0.5; p[P.DLY] = 0; p[P.REV] = 0;
  p[P.LSPD] = mid(7, 16); p[P.LDEP] = 0.5; p[P.LDST] = mid(0, NDEST); p[P.LWAV] = mid(0, NWAV);
  p[P.NOTE] = 0.5; p[P.VEL] = 100 / 127; p[P.PROB] = 1; p[P.MICR] = 0.5; p[P.RTRG] = mid(0, 12);
  p[P.RCNT] = mid(3, 16); p[P.RVEL] = 0.5; p[P.GATE] = 0; p[P.RCRV] = 0.5; p[P.JUMP] = mid(12, NJUMP); p[P.JRNG] = mid(11, NJRNG);
  p[P.SPC] = 0.5; p[P.ATK] = 0; p[P.CHOK] = mid(0, 5); p[P.DUCK] = 0;
  p[P.L2SP] = mid(5, 16); p[P.L2DP] = 0.5; p[P.L2DS] = mid(0, NDEST); p[P.L2WV] = mid(0, NWAV);
  p[P.IR] = mid(0, NIR + 1); p[P.IMIX] = 0.5; p[P.IPRE] = 0; p[P.IDCY] = 1; p[P.IGAT] = 1; p[P.ITON] = 0.5; p[P.IFB] = 0; p[P.IREV] = mid(0, 2);
  p[P.PNCH] = 0; p[P.ECRV] = 1; p[P.DRON] = 0;
  Object.assign(p, RAM_PAD, V10_PAD, HALO_PAD, BRK_PAD, FX_PAD, V19_PAD, V20_PAD);
  p[P.HARM] = mid(1, 3); p[P.SAIR] = STRUM_AIR0;
  machineDefaults(p, m);
  return p;
}
// v64: the SYNTH machines a little wider (SPRD), the others centred
const SPRD_DEF = { 'SY·RAW': 0.3, 'TRIO': 0.3, 'CZ·PD': 0.3, 'FM·X': 0.3, 'VOX': 0.3, 'MACRO': 0.3 };
const sprDef = m => SPRD_DEF[MACH[m].id] || 0;
function machineDefaults(p, m) {
  const d = MACH[m].d;
  p[P.SPRD] = sprDef(m); p[P.SMON] = 0.5;
  p[P.TUNE] = d[0]; p[P.DEC] = d[1];
  for (let i = 0; i < 6; i++) p[P.A + i] = d[2 + i];
  if (d.length > 8) FX_XS.forEach((q, i) => { p[q] = d[8 + i]; });   // an FX machine: its MORE side
  SP(NSIDE).forEach((q, i) => { p[q] = MACH[m].sd ? (MACH[m].sd[i] || 0) : 0; });   // ALLOY, WEST ...: their sides
}
function newTrack(m) {
  return { p: defaultParams(m), len: 16, spd: 4, mute: 0, q: 1, rx: 0, steps: emptySteps() };
}
const nv = st => clamp(0.5 + st / 48, 0, 1); // semitones -> NOTE value
const irv = name => mid(IR_NAMES.indexOf(name), NIR + 1);
const srcv = name => mid(4 + org(name), ULOOP_SRC.length);   // µLOOP source
const hpv = hz => Math.log(hz / 20) / Math.log(400), lpv = hz => Math.log(hz / 100) / Math.log(200);   // TONE corners in Hz
const orgv = name => mid(org(name), NORG);                  // FRACT source

// master section values: neutral (EQ flat, compressor and drive off) and the demo's light glue
const MASTER_NEUTRAL = [mid(2, 4), 0, 0, mid(3, 7), 0, 0.5, 0, mid(1, 3),
                        0.33, 0.4, 0.4, mid(0, 10), mid(0, 3), mid(0, 4), 1, 0.5,
                        mid(0, 8), 0.2, 0.5, 0.5, 0.5, 1, 0.5, mid(1, 2)];
const MASTER_DEMO = [mid(2, 4), 0.18, 0.12, mid(4, 7), 0.4, 0.6, 0.05, mid(2, 3),     // 60 Hz boost + cut, 10 kHz air
                     0.2, 0.45, 0.35, mid(2, 10), mid(1, 3), mid(1, 4), 0.8, 0.625,    // 2:1, Dist 2, detector HP, +3 dB
                     mid(1, 8), 0.2, 0.5, 0.5, 2 / 3, 1, 0.5, mid(1, 2)];               // tape at 15 ips, after the comp
// reverb (ALGO TIME PRE CH1 CH2 LVL) and DAC (model, age) for new projects, after the master block
const REV_DAC_DEFAULT = [mid(4, NREV), 0.45, 0, 0.5, 0.5, 0.6, mid(1, 5), 0.35];
// MIX: unmasking 50 %, kick priority 50 %, calm 35 %, sharpness, selectivity, speed, focus (~2.3 kHz), delta off
const MIX_DEFAULT = [0.5, 0.5, 0.35, 0.5, 0.5, 0.5, 0.6, mid(0, 2)];
// looper: off, one beat, no offset
const LOOP_DEFAULT = [0, mid(6, 12), mid(16, 33)];
// DICE: off, the module's defaults; routes off (t: TRIG, x: NOTE, y: FREQ, amount 100 %)
const DICE_DEFAULT = [0, mid(4, 9), mid(1, 3), mid(0, 7), 0.5, 0, 0.5, mid(57, 75), mid(0, 4),
  0.5, 0.5, 0.6, mid(0, 3), mid(0, 3), mid(0, 6), mid(0, 4), mid(6, 12),
  ...[0, 1, 2, 3, 4, 5, 6].flatMap(r => [mid(0, 9), mid(r < 3 ? 0 : r < 6 ? 1 : 13, NDD), 1])];
// v6: DICE pulse width, then GRAIN: grains of ~0.25 s from the last half second, regular, a little randomness
const V6_DEFAULT = [0.5, 0.08, 0.7, 0.5, 0.5, 0.2, 0.3, 0.55, 0.3, mid(0, 4), 0, 0];
// v7: GRAIN off (MIX 35 %), its LFO (a sine over a bar, attenuverters at 0); EQ, COMP, DRV on; OTT off at its defaults;
// the ON keys of the reverb, the delay, the DAC and MIX
const V7_DEFAULT = [0, 0.35, mid(10, 15), mid(0, 64), mid(0, 16), ...Array(8).fill(0.5), 1, 1, 1, 0, 1, 1, 1, 0.5, 0.5, 0.5, 0.5, 1,
  1, 1, 1, 1];   // then REV, DLY, DAC, MIX on
// v11: GLITCH off, every amount at zero (TIME 1/16, MIX 100 %, DECIM, STD, SPEED in the middle, HOLD off), the r tracks
const V11_DEFAULT = [0, 0, mid(2, 7), 0, 0, 0, 0, 0, 1, mid(0, 3), mid(0, 5), 0.5, 0, 1];
// v12: LOSS's RESO at 35 %, the codec (DIG), the CASS tape
const V12_DEFAULT = [0.35, 0, mid(0, 5)];
const V591_DEFAULT = [0];
// v68 (core/sk_engine.cpp init): the CHORUS on mode 2, in stereo, at 80 %, on; the PHASER's generators 0.6 and 1 Hz sines,
// A and B 70 % deep with 30 % of feedback, B on generator 1 against A, in stereo, at 80 %, on; the DELAY on STD, x1,
// its VCO still (0.55 Hz, a sine, no envelope), HOLD off
const V68_DEFAULT = [mid(1, 4), 0.75, 0.8, 1, 0.35, 0, 0.45, 0, 0.7, 0.3, 0.7, 0.3, 0.25, 0.75, mid(0, 3), 0.8, 1, 0.25, 0.25, 0, 0.4, 0, 0.5, 0];
function gDefault(g) {
  if (g >= G.CHMD) return V68_DEFAULT[g - G.CHMD];
  if (g >= G.QSRC) return V591_DEFAULT[g - G.QSRC];
  if (g >= G.XRES) return V12_DEFAULT[g - G.XRES];
  if (g >= G.XON) return V11_DEFAULT[g - G.XON];
  if (g >= G.GON) return V7_DEFAULT[g - G.GON];
  if (g >= G.DPW) return V6_DEFAULT[g - G.DPW];
  if (g >= G.DON) return DICE_DEFAULT[g - G.DON];
  if (g >= G.LOOP) return LOOP_DEFAULT[g - G.LOOP];
  if (g >= G.UMSK) return MIX_DEFAULT[g - G.UMSK];
  if (g >= G.RALG) return REV_DAC_DEFAULT[g - G.RALG];
  if (g >= 18) return MASTER_NEUTRAL[g - 18];
  if (g === G.RSIZ) return 0.5;
  if (g === G.RDMP) return 0.4;
  return demoState().g[g];
}
// scales for the quantiser (pitch classes from the root)
const SCALES = [
  ['MAJ', [0, 2, 4, 5, 7, 9, 11]], ['MIN', [0, 2, 3, 5, 7, 8, 10]], ['DOR', [0, 2, 3, 5, 7, 9, 10]],
  ['PHRY', [0, 1, 3, 5, 7, 8, 10]], ['LYD', [0, 2, 4, 6, 7, 9, 11]], ['MIXO', [0, 2, 4, 5, 7, 9, 10]],
  ['HMIN', [0, 2, 3, 5, 7, 8, 11]], ['HIJAZ', [0, 1, 4, 5, 7, 8, 10]], ['PENT', [0, 2, 4, 7, 9]],
  ['PMIN', [0, 3, 5, 7, 10]], ['BLUES', [0, 3, 5, 6, 7, 10]], ['WHOLE', [0, 2, 4, 6, 8, 10]], ['CHROM', [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]],
];
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const scaleMask = (notes, root) => notes.reduce((m, n) => m | (1 << ((n + root) % 12)), 0);
const rotMask = (m, d) => { d = ((d % 12) + 12) % 12; return ((m << d) | (m >> (12 - d))) & 0xFFF; };
function scaleName(mask, root) {
  if (!mask) return 'OFF';
  for (const r of [root, ...Array.from({ length: 12 }, (_, i) => i)])
    for (const [name, notes] of SCALES) if (scaleMask(notes, r) === mask) return NOTE_NAMES[r] + ' ' + name;
  let n = 0; for (let i = 0; i < 12; i++) n += (mask >> i) & 1;
  return 'CUSTOM · ' + n;
}
function demoState() {
  const kit = [0, 3, 4, 11, 17, 16, ALLOY_M, TRIO_M];
  const s = { v: SV, scale: scaleMask(SCALES[1][1], 0), qroot: 0,
              g: [124, 0.12, mid(7, 11), 0.42, 0.38, 0.7, 0.5, 0.45, 0.12, 0.8,
                  0.6, 0.3, 0.5, 0.2, 0.55, 0, 0, 0.6, ...MASTER_DEMO,
                  mid(5, NREV), 0.32, 0.18, 0.22, 0.4, 0.62, mid(1, 5), 0.45, ...MIX_DEFAULT, ...LOOP_DEFAULT, ...DICE_DEFAULT, ...V6_DEFAULT, ...V7_DEFAULT, ...V11_DEFAULT, ...V12_DEFAULT, ...V591_DEFAULT, ...V68_DEFAULT], tracks: kit.map(newTrack) };
  const on = (t, list, locks) => list.forEach(i => { s.tracks[t].steps[i].on = 1; if (locks && locks[i]) s.tracks[t].steps[i].l = locks[i]; });
  const rat = (t, list) => list.forEach(([i, n]) => { s.tracks[t].steps[i].r = n; });
  const set = (t, vals) => Object.assign(s.tracks[t].p, vals);
  // T1 kick: syncopated, a pitched lock and a fade-in retrig roll
  on(0, [0, 6, 10, 13, 15], {
    13: { [P.TUNE]: 0.62, [P.DEC]: 0.3 },
    15: { [P.RTRG]: mid(7, 12), [P.RCNT]: mid(5, 16), [P.RVEL]: 0.85, [P.VEL]: 0.6 },
  });
  set(0, { [P.VOL]: 0.85 });
  // T2 FM snare through a gated stairwell
  on(1, [4, 12], { 12: { [P.E]: 0.55, [P.B]: 0.7, [P.IGAT]: 0.75 } });
  set(1, { [P.IR]: irv('STAIRS'), [P.IMIX]: 0.4, [P.IGAT]: 0.45 });
  // T3 metal hats in a tin can, open on the off-beats
  on(2, [0, 2, 4, 6, 8, 9, 10, 12, 14], {
    2: { [P.VEL]: 0.45 }, 6: { [P.DEC]: 0.5 }, 9: { [P.VEL]: 0.35, [P.A]: 0.8 }, 14: { [P.DEC]: 0.55 },
  });
  set(2, { [P.PAN]: 0.42, [P.VOL]: 0.82, [P.IR]: irv('CAN'), [P.IMIX]: 0.4 });
  rat(2, [[10, 2], [14, 3]]);
  // T4 micro-loops on crunched crisps (12-bit, SP-1200 style), then a shrinking ice-cube stutter rising in pitch, into the space reverb
  on(3, [11, 15], { 15: { [P.C]: 0.2, [P.D]: 0.75, [P.A]: srcv('ICE') } });
  set(3, { [P.REV]: 0.4, [P.PAN]: 0.4, [P.VOL]: 0.6 });
  // T5 ITERA metal percussion (BIA-style), 12-step polymeter, the folder opens on the last hit
  s.tracks[4].len = 12;
  on(4, [2, 5, 9], { 5: { [P.NOTE]: nv(7) }, 9: { [P.NOTE]: nv(12), [P.D]: 0.6, [P.B]: 0.85 } });
  set(4, { [P.TUNE]: 0.6, [P.DEC]: 0.42, [P.A]: 0.7, [P.B]: 0.65, [P.C]: 0.2, [P.D]: 0.25, [P.E]: 0.5, [P.F]: mid(2, 3),
           [P.DLY]: 0.3, [P.PAN]: 0.32, [P.VOL]: 0.55 });
  // T6 FRACT: a crowd of snapping twigs, then a cloud of short crisp grains, inside a steel jerrycan
  on(5, [7, 15], { 15: { [P.A]: orgv('CHIPS'), [P.B]: mid(11, 32), [P.D]: 0.5, [P.F]: 0.35, [P.E]: 0.7 } });
  set(5, { [P.A]: orgv('TWIG'), [P.B]: mid(4, 32), [P.C]: 0.35, [P.D]: 0.2, [P.E]: 0.45,
           [P.IR]: irv('BARREL'), [P.IMIX]: 0.4, [P.REV]: 0.45, [P.PAN]: 0.68, [P.VOL]: 0.7 });
  // T7 ALLOY: a steel bar under a felt mallet, a breath of sines over it, through an open piano; 5-step loop at 3/4 speed
  s.tracks[6].len = 5; s.tracks[6].spd = 3;
  on(6, [0, 3], { 3: { [P.NOTE]: nv(5) } });
  set(6, { [P.TUNE]: 0.75, [P.DEC]: 0.66, [P.A]: mid(1, 8), [P.B]: 0.55, [P.C]: 0.75, [P.D]: 0.55, [P.S1]: 0.35, [P.S13]: 0.3, [P.S14]: 0.4,
           [P.IR]: irv('PIANO'), [P.IMIX]: 0.35, [P.REV]: 0.5, [P.VOL]: 0.55 });
  // T8 TRIO bass (root, octave, minor third) through its 4-pole ladder, saw + pulse, a bit of punch, ducked by the kick
  on(7, [0, 3, 6, 10, 13], { 6: { [P.NOTE]: nv(12) }, 10: { [P.NOTE]: nv(3) }, 13: { [P.NOTE]: nv(-2), [P.FENV]: 0.95 } });
  rat(7, [[13, 2]]);
  set(7, { [P.FREQ]: 0.38, [P.RES]: 0.5, [P.FENV]: 0.78, [P.FDEC]: 0.28, [P.GATE]: 0.08, [P.VOL]: 0.55, [P.DUCK]: 0.5, [P.D]: 0.5, [P.E]: 0.35, [P.PNCH]: 0.3 });
  // TONE: each voice keeps its own band; the kick and the bass own the lows, the rest is high-passed out of their way
  [[0, 25], [1, 120], [2, 500], [3, 180], [4, 90], [5, 250], [6, 100], [7, 30]].forEach(([t, hz]) => { s.tracks[t].p[P.HPF] = hpv(hz); });
  set(1, { [P.MID]: 0.5 - 2 / 30, [P.MIDF]: Math.log(450 / 150) / Math.log(53.333) });   // the snare: 2 dB less box
  // an envelope follower: the hats tick the bass filter open (T3's highs, fast attack, short decay)
  set(7, { [P.E1SR]: mid(3, 11), [P.E1GN]: (6 + 12) / 48, [P.E1BD]: mid(3, 4), [P.E1AT]: 0.25, [P.E1DC]: Math.log(90 / 5) / Math.log(800),
           [P.E1DP]: 0.6, [P.E1DS]: mid(9, NDEST) });
  // the hats stay off the scale: their metal ratios are part of the sound
  s.tracks[2].q = 0;
  patFix(s);
  return s;
}

// a new, clean project: standard kit, empty pattern, every sound at its INIT, master and MIX neutral
// (EQ flat, compressor, drive and clip off, DAC off, no unmasking or calming), no sends
function initState() {
  const kit = [0, 2, 4, 5, ALLOY_M, 6, 9, TRIO_M];   // BD·VA SD·VA HH·MT CP·NZ ALLOY FM·X SY·RAW TRIO
  const s = { v: SV, scale: 0, qroot: 0,
              g: [124, 0, mid(7, 11), 0.42, 0.38, 0.7, 0.5, 0.45, 0, 0.8,
                  0.6, 0.3, 0.5, 0.2, 0.55, 0, 0, 0.6, ...MASTER_NEUTRAL,
                  mid(4, NREV), 0.45, 0, 0.5, 0.5, 0.6, mid(0, 5), 0.35,
                  0, 0.5, 0, 0.5, 0.5, 0.5, 0.6, mid(0, 2), ...LOOP_DEFAULT, ...DICE_DEFAULT, ...V6_DEFAULT, ...V7_DEFAULT, ...V11_DEFAULT, ...V12_DEFAULT, ...V591_DEFAULT, ...V68_DEFAULT],
              tracks: kit.map(m => {
                const t = newTrack(m), st = startSound(m);
                Object.assign(t.p, st.vals);
                t.pre = st.pre;
                return t;
              }) };
  // the default kit, when there is one: its sounds and master on the empty pattern
  const dk = defaultKit(), ks = dk ? kitState(dk) : null;
  if (ks) kitApply(s, ks, false);
  patFix(s);
  return s;
}

function validState(s) {
  return s && s.v === SV && Array.isArray(s.g) && s.g.length === NG && Array.isArray(s.tracks) && s.tracks.length === NT &&
    s.tracks.every(t => Array.isArray(t.p) && t.p.length === NP && Array.isArray(t.steps) && t.steps.length === NSTEP);
}

// older projects are converted in place: v1 (12 machines, 33 parameters), v2 (18 machines, µLOOP sources morphing 4 synths),
// v3 (19 machines on a 19-slot grid, plate send + spectral send)
// v18: 64 steps a track, the patterns (8 banks of 16), DEC at its top is HOLD. A v18 project: its patterns unpacked; an
// older one: through the older conversions, then onto v18 (toV18)
// v76: SPHERE (v75's 42nd machine) is gone, WEST in its place: a project saved before (no "wst": serialState marks
// what it saves) has its tracks on it at WEST's defaults (A..F, DEC, the sides), their locks made for SPHERE gone; a
// step locked onto it from another machine takes WEST's sides and A..F in its locks (as the v75 picker locked SPHERE's)
function v76Track(t) {
  const m = mof(t.p[P.MACH]), d = MACH[WEST_M].d, sd = MACH[WEST_M].sd;
  if (m === WEST_M) { t.p[P.DEC] = d[1]; for (let i = 0; i < 6; i++) t.p[P.A + i] = d[2 + i]; sd.forEach((v, i) => { t.p[SQ(i)] = v; }); }
  for (const st of t.steps || []) {
    const l = st && st.l; if (!l) continue;
    const onto = P.MACH in l;
    if ((onto ? mof(l[P.MACH]) : m) !== WEST_M) continue;
    for (let i = 0; i < 6; i++) if ((P.A + i) in l) { if (onto) l[P.A + i] = d[2 + i]; else delete l[P.A + i]; }
    for (let i = 0; i < NSIDE; i++) if (SQ(i) in l) { if (onto) l[SQ(i)] = sd[i]; else delete l[SQ(i)]; }
  }
}
function toV76(s) {
  for (const t of s.tracks) v76Track(t);
  for (const pt of s.pats || []) if (pt && pt.tracks) for (const t of pt.tracks) v76Track(t);
  s.wst = 1;
}
// v78: a step locked onto WEST from another machine, saved before its places S36 .. were (no "v78"): WEST's own in its
// locks for them (else they are the other machine's, at rest: no output); its tracks padded by padParams / patUnpack
function v78Track(t) {
  const m = mof(t.p[P.MACH]), sd = MACH[WEST_M].sd;
  if (m === WEST_M) return;
  for (const st of t.steps || []) {
    const l = st && st.l;
    if (!l || !(P.MACH in l) || mof(l[P.MACH]) !== WEST_M) continue;
    for (let q = NSIDE_A; q < NSIDE; q++) if (!(SQ(q) in l)) l[SQ(q)] = sd[q];
  }
}
function toV78(s) {
  for (const t of s.tracks) v78Track(t);
  for (const pt of s.pats || []) if (pt && pt.tracks) for (const t of pt.tracks) v78Track(t);
  s.v78 = 1;
}
// v82: HOWL's LOOP page: a project saved before it (no "v82") has v81's DIST, on STRING, as TIME on LOOP (the same
// distance), no hum where DIST was, LOOP's other places at HOWL's defaults, its SIDE switch on four pages; on its steps
// too, and on a step locked onto HOWL from another machine
function v82Places(o, isLock) {
  const sd = MACH[HOWL_M].sd;
  if (SQ(3) in o) { o[SQ(14)] = howlTime(o[SQ(3)]); if (isLock) delete o[SQ(3)]; else o[SQ(3)] = 0; }
  else if (!isLock) o[SQ(14)] = sd[14];
  for (let q = 15; q < 21; q++) if (!isLock || !(SQ(q) in o)) o[SQ(q)] = sd[q];
  if (P.F in o) o[P.F] = mid(idx(o[P.F], 3), 4);
}
function v82Track(t) {
  const m = mof(t.p[P.MACH]);
  if (m === HOWL_M) v82Places(t.p, false);
  for (const st of t.steps || []) {
    const l = st && st.l; if (!l) continue;
    const onto = P.MACH in l;
    if ((onto ? mof(l[P.MACH]) : m) !== HOWL_M) continue;
    if (onto && m !== HOWL_M) v82Places(l, true);
    else { if (SQ(3) in l) { l[SQ(14)] = howlTime(l[SQ(3)]); delete l[SQ(3)]; } if (P.F in l) l[P.F] = mid(idx(l[P.F], 3), 4); }
  }
}
function toV82(s) {
  for (const t of s.tracks) v82Track(t);
  for (const pt of s.pats || []) if (pt && pt.tracks) for (const t of pt.tracks) v82Track(t);
  s.v82 = 1;
}
// v83: HOWL's CAB and BOW pages: a project saved before them (no "v83") has its SIDE switch on four pages (now six),
// CAB's and BOW's places at HOWL's defaults (the amp as it was, the bow off); on its steps too, and on a step locked
// onto HOWL from another machine
function v83Places(o, isLock) {
  const sd = MACH[HOWL_M].sd;
  for (let q = 21; q < 35; q++) if (!isLock || !(SQ(q) in o)) o[SQ(q)] = sd[q];
  if (P.F in o) o[P.F] = mid(idx(o[P.F], 4), 6);
}
function v83Track(t) {
  const m = mof(t.p[P.MACH]);
  if (m === HOWL_M) v83Places(t.p, false);
  for (const st of t.steps || []) {
    const l = st && st.l; if (!l) continue;
    const onto = P.MACH in l;
    if ((onto ? mof(l[P.MACH]) : m) !== HOWL_M) continue;
    if (onto && m !== HOWL_M) v83Places(l, true);
    else if (P.F in l) l[P.F] = mid(idx(l[P.F], 4), 6);
  }
}
function toV83(s) {
  for (const t of s.tracks) v83Track(t);
  for (const pt of s.pats || []) if (pt && pt.tracks) for (const t of pt.tracks) v83Track(t);
  s.v83 = 1;
}
function migrate(s) {
  const r = migrate26(s);
  if (r && !r.wst) toV76(r);
  if (r && !r.v78) toV78(r);
  if (r && !r.v82) toV82(r);
  if (r && !r.v83) toV83(r);
  return r;
}
function migrate26(s) {
  if (!s || !Array.isArray(s.tracks) || !Array.isArray(s.g)) return null;
  if (s.v === SV) { padParams(s); patFix(s); return s; }
  // v68: the CHORUS and PHASER sends at 0, their effects, the DELAY's model and VCO at their defaults (padParams)
  if (s.v === SV25) { padParams(s); patFix(s); s.v = SV; return s; }
  // (v65: every older project's LFO waves onto the 16 of today, after the rest of its conversion)
  const r = migrate24(s);
  if (r) toV25(r);
  return r;
}
function migrate24(s) {
  if (s.v === SV24) { padParams(s); patFix(s); s.v = SV; return s; }      // (v65: RDEC off, SLW and LEN at rest)
  if (s.v === SV23) { padParams(s); patFix(s); s.v = SV; return s; }      // (v64: RPAN, SPRD and MONO at rest)
  if (s.v === SV22) { padParams(s); patFix(s); toV23(s); return s; }
  if (s.v === SV21) { padParams(s); patFix(s); toV22(s); return s; }
  if (s.v === SV20) { padParams(s); patFix(s); toV21(s); return s; }
  if (s.v === SV19) { padParams(s); patFix(s); toV20(s); toV21(s); return s; }
  if (s.v === SV18) { padParams(s); patFix(s); toV19(s); toV20(s); toV21(s); return s; }
  const r = migrate17(s);
  if (r) { toV18(r); toV19(r); toV20(r); toV21(r); }
  return r;
}
function migrate17(s) {
  if (!s || !Array.isArray(s.tracks) || !Array.isArray(s.g)) return null;
  // what the project held before anything is padded: PRISM's WDTH (v38 projects) and the DICE routes move to new grids
  // in v8, the values padded in meanwhile are already on them
  s.__had = { g: s.g.length, p: s.tracks.map(t => (Array.isArray(t.p) ? t.p.length : 0)) };
  if (s.v === SV17) { delete s.__had; padParams(s); return s; }
  // v15 -> v16: the FX machines arrive (their MORE side padded: FX_PAD); v16 -> v17: XF and XG (padded at 0: BASS ×1,
  // SHFT off, PING, DUCK, WIDE, JOIN none)
  if (s.v === 15 || s.v === 16) { delete s.__had; padParams(s); s.v = SV17; return s; }
  // v14 -> v15: BREAK plays the user's slots (brkV15)
  if (s.v === 14) { delete s.__had; padParams(s); brkV15(s); s.v = SV17; return s; }
  // v13 -> v14: BREAK arrives (its CHOP side padded at its defaults)
  if (s.v === 13) { delete s.__had; padParams(s); s.v = SV17; return s; }
  // v12 -> v13: HALO arrives (its VOICE side padded at its defaults)
  if (s.v === 12) { delete s.__had; padParams(s); s.v = SV17; return s; }
  // v11 -> v12: RAND leaves GLITCH for the SEQ page (LIVE): it only played with GLITCH on; RESO, DIG, CASS (padParams)
  if (s.v === 11) { delete s.__had; if (!(s.g[G.XON] > 0.5)) s.g[G.XRND] = 0; padParams(s); s.v = SV17; return s; }
  // v10 -> v11: GLITCH arrives off (padParams)
  if (s.v === 10) { delete s.__had; padParams(s); s.v = SV17; return s; }
  if (s.v === 9) { delete s.__had; padParams(s); return migrate9(s); }
  if (s.v === 8) { delete s.__had; padParams(s); rmbSlots(s); return migrate8(s); }
  if (s.v === 7) { rmbSlots(s); return migrate7(s); }
  if (s.v === 6) { padParams(s); rmbSlots(s); return migrate6(s); }
  if (s.v === 5) { padParams(s); rmbSlots(s); return migrate5(s); }
  if (s.v === 4) { padParams(s); rmbSlots(s); return migrate4(s); }
  if (s.v === 3) return migrate4(migrate3(s));
  if (s.v !== 1 && s.v !== 2) return null;
  const remap = (v, from, to) => mid(idx(v, from), to);
  if (s.v === 1) {
    for (const t of s.tracks) {
      t.p[P.MACH] = remap(t.p[P.MACH], 12, 18);
      t.p[P.LDST] = remap(t.p[P.LDST], 18, 24);
      for (const st of t.steps) {
        if (P.MACH in st.l) st.l[P.MACH] = remap(st.l[P.MACH], 12, 18);
        if (P.LDST in st.l) st.l[P.LDST] = remap(st.l[P.LDST], 18, 24);
      }
    }
    const dg = demoState().g;
    while (s.g.length < 18) s.g.push(dg[s.g.length]);
  }
  // v2 -> v3: 19 machines; µLOOP SRC becomes a list (4 synth sources, then the organic bank)
  const src = v => mid(Math.round(clamp(v, 0, 1) * 3), ULOOP_SRC.length);
  for (const t of s.tracks) {
    const m = idx(t.p[P.MACH], 18);
    const def = defaultParams(om21(m < 18 ? m : 0));
    const n0 = t.p.length;
    while (t.p.length < NP) t.p.push(def[t.p.length]);
    if (n0 <= P.SPC) t.p[P.SPC] = 0;     // v1 had no spectral send
    t.p[P.MACH] = mid(m, 19);
    if (m === M20.ULOOP) t.p[P.A] = src(t.p[P.A]);
    for (const st of t.steps) {
      const sm = P.MACH in st.l ? idx(st.l[P.MACH], 18) : m;
      if (P.MACH in st.l) st.l[P.MACH] = mid(sm, 19);
      if (sm === M20.ULOOP && P.A in st.l) st.l[P.A] = src(st.l[P.A]);
    }
  }
  s.v = 3;
  return migrate4(migrate3(s));
}
// v3 -> v4: P_MACH on 32 fixed slots (64 since v9: migrate8); one reverb with six algorithms instead of plate + spectral sends
// (the plate keeps its decay if it was in use, else the spectral settings move over); the spectral send
// becomes the RND depth; DAC on, CD-80; scale quantiser off; every track follows the quantiser
function migrate3(s) {
  while (s.g.length < 42) s.g.push(MASTER_NEUTRAL[s.g.length - 18]);
  const g = s.g;
  const usedRev = s.tracks.some(t => t.p[P.REV] > 0.001 || t.steps.some(st => st.l[P.REV] > 0.001));
  const usedSpc = s.tracks.some(t => t.p[P.SPC] > 0.001 || t.steps.some(st => st.l[P.SPC] > 0.001));
  let rev;
  if (usedSpc && !usedRev) {
    const mode = (g[G.SFRZ] > 0.5 ? 1 : 0) + (g[G.SREV] > 0.5 ? 2 : 0);
    g[G.RSIZ] = g[G.SBLR]; g[G.RDMP] = g[G.SATM];
    rev = [mid(REV_SPECTRAL, 6), g[G.STIM], mid(mode, 4), g[G.SWRP], Math.sqrt(clamp(g[G.SREF], 0, 1)), g[G.SLVL]];
  } else {
    rev = [mid(1, 6), g[G.RSIZ], 0, 0.5, 0.5, 0.6];
    g[G.RSIZ] = 0.5;
  }
  s.g = g.slice(0, 42).concat(rev, [mid(1, 5), 0.35], MIX_DEFAULT);
  for (const t of s.tracks) {
    t.p[P.MACH] = mv32(idx(t.p[P.MACH], 19));
    t.p[P.REV] = Math.max(t.p[P.REV], t.p[P.SPC]);
    t.p[P.SPC] = 0.5;
    if (idx(t.p[P.LDST], 24) === 18) t.p[P.LDST] = mid(0, 24);
    if (idx(t.p[P.L2DS], 24) === 18) t.p[P.L2DS] = mid(0, 24);
    t.q = 1; t.rx = 0;
    for (const st of t.steps) {
      if (P.MACH in st.l) st.l[P.MACH] = mv32(idx(st.l[P.MACH], 19));
      if (P.SPC in st.l) { st.l[P.REV] = Math.max(P.REV in st.l ? st.l[P.REV] : t.p[P.REV], st.l[P.SPC]); delete st.l[P.SPC]; }
    }
  }
  s.scale = 0; s.qroot = 0;
  s.v = 4;
  s.rmb = 1;
  padParams(s);
  return s;
}
// v4 -> v5: the LFO destination list grows from 24 to 27 entries (the retrig ones are appended), so the
// stored DST values move to the new grid (same destination)
const destV5 = v => mid(idx(v, NDEST_V4), NDEST_V7);   // (then destV8, v7 -> v8)
function migrate4(s) {
  for (const t of s.tracks) {
    t.p[P.LDST] = destV5(t.p[P.LDST]); t.p[P.L2DS] = destV5(t.p[P.L2DS]);
    for (const st of t.steps) {
      if (P.LDST in st.l) st.l[P.LDST] = destV5(st.l[P.LDST]);
      if (P.L2DS in st.l) st.l[P.L2DS] = destV5(st.l[P.L2DS]);
    }
  }
  s.v = 5;
  padParams(s);
  return migrate5(s);
}
// v5 -> v6: the DICE lock keys arrive: DEJA VU starts at LOCK when no lock was on (the knob did nothing then)
function migrate5(s) {
  if (idx(s.g[G.DDVM], 4) === 0) s.g[G.DDV] = 0.5;
  s.v = 7;                  // (its ALGO is already on the 6-algorithm grid: nothing for v6 -> v7 to do)
  return migrate7(s);
}
// v6 -> v7: GRAIN leaves the reverb (7 algorithms then) for the master chain; a project that used it as its
// reverb gets HALL back and GRAIN on the master, partly mixed
function migrate6(s) {
  padParams(s);
  const a = idx(s.g[G.RALG], 7);
  if (a === 6) { s.g[G.RALG] = mid(4, NREV); s.g[G.GON] = 1; s.g[G.GMIX] = 0.35; }
  else s.g[G.RALG] = mid(a, NREV);
  s.v = 7;
  return migrate7(s);
}
// v7 -> v8: the LFO destinations grow from 27 to 35 entries and the DICE ones from 34 to 42 (PRISM appended): stored
// values move to the new grids (same destination). PRISM's WDTH becomes bipolar (12 o'clock = the whole band, a band
// narrows to the right): the old width (0 narrow .. 1 the whole band) moves to the right half
const destV8 = v => mid(idx(v, NDEST_V7), NDEST_V9);
const widthV8 = v => 0.5 + 0.5 * (1 - clamp(v, 0, 1));
function migrate7(s) {
  const had = s.__had || { g: 0, p: [] };
  delete s.__had;
  s.tracks.forEach((t, i) => {
    const prism = (had.p[i] || 0) > P.PWID;
    t.p[P.LDST] = destV8(t.p[P.LDST]); t.p[P.L2DS] = destV8(t.p[P.L2DS]);
    if (prism) t.p[P.PWID] = widthV8(t.p[P.PWID]);
    for (const st of t.steps) {
      if (P.LDST in st.l) st.l[P.LDST] = destV8(st.l[P.LDST]);
      if (P.L2DS in st.l) st.l[P.L2DS] = destV8(st.l[P.L2DS]);
      if (P.PWID in st.l) st.l[P.PWID] = widthV8(st.l[P.PWID]);
    }
  });
  for (let r = 0; r < 7; r++) { const i = G.DR0 + 3 * r + 1; if (i < had.g) s.g[i] = mid(idx(s.g[i], NDD_V7), NDD_V9); }
  s.v = 8;
  padParams(s);
  return migrate8(s);
}
// v8 -> v9: P_MACH moves to 64 slots, and the four RUMBLE voices (slots 25..28) are gone: a track or a lock on one of them
// goes to the nearest machine now (RMB·1 HEX·BD, RMB·2 WT·DRM, RMB·3 HEX·SD, RMB·4 HH·MT), A..F at its defaults, TUNE and
// DEC kept; the A..F locks made for the old voice go
const RMB_TO = { 25: 25, 26: 28, 27: 26, 28: 4 };
const AF = [P.A, P.B, P.C, P.D, P.E, P.F];
function migrate8(s) {
  const old = v => Math.min(idx(v, MS32), 30);
  const isOld = v => { const m = old(v); return m >= 25 && m <= 28; };
  const conv = v => { const m = old(v); return mv(m in RMB_TO ? RMB_TO[m] : m); };
  const defs = v => mach20(mof20(v)).d.slice(2, 8);
  for (const t of s.tracks) {
    const was = isOld(t.p[P.MACH]);
    t.p[P.MACH] = conv(t.p[P.MACH]);
    if (was) { defs(t.p[P.MACH]).forEach((d, i) => { t.p[AF[i]] = d; }); t.pre = null; }
    for (const st of t.steps) {
      if (P.MACH in st.l) {
        const lw = isOld(st.l[P.MACH]);
        st.l[P.MACH] = conv(st.l[P.MACH]);
        if (lw) defs(st.l[P.MACH]).forEach((d, i) => { st.l[AF[i]] = d; });   // the step's own machine, at its defaults
      } else if (was) AF.forEach(p => { delete st.l[p]; });
    }
  }
  s.v = 9;
  return migrate9(s);
}
// v9 -> v10: the LFO destinations grow from 35 to 38 entries and the DICE ones from 42 to 44 (TONE appended): stored
// values move to the new grids (same destination); TONE and the envelope followers arrive at rest (padParams)
const destV10 = v => mid(idx(v, NDEST_V9), NDEST);
function migrate9(s) {
  for (const t of s.tracks) {
    t.p[P.LDST] = destV10(t.p[P.LDST]); t.p[P.L2DS] = destV10(t.p[P.L2DS]);
    for (const st of t.steps) {
      if (P.LDST in st.l) st.l[P.LDST] = destV10(st.l[P.LDST]);
      if (P.L2DS in st.l) st.l[P.L2DS] = destV10(st.l[P.L2DS]);
    }
  }
  for (let r = 0; r < 7; r++) { const i = G.DR0 + 3 * r + 1; if (i < s.g.length) s.g[i] = mid(idx(s.g[i], NDD_V9), NDD); }
  s.v = SV17;
  padParams(s);
  return s;
}
// slots 25 / 26 held two Rumble-inspired machines (kick, snare) before the four RUMBLE voices took
// slots 25..28: the kick stays on RUMBLE 1, the snare (RES / SLAP / KS, same order) moves to RUMBLE 3
function rmbSlots(s) {
  if (s.rmb) return;
  s.rmb = 1;
  const fix = v => (idx(v, MS32) === 26 ? mv32(27) : v);
  for (const t of s.tracks) {
    t.p[P.MACH] = fix(t.p[P.MACH]);
    for (const st of t.steps) if (P.MACH in st.l) st.l[P.MACH] = fix(st.l[P.MACH]);
  }
}
// parameters appended after v4 shipped (PUNCH, the RAM machine's REC side): older projects get them at their defaults
const RAM_PAD = { [P.RSRC]: mid(1, RAM_SRC.length), [P.RLEN]: mid(7, RAM_TAKE.length), [P.RLEV]: 0.5, [P.RTHR]: 0, [P.ROVD]: 0, [P.RMON]: 0,
  [P.RBUF]: mid(0, RAM_BUF.length),
  // PRISM: off, 8 oscillators, a little blur and glide, the whole band, the take at its own speed; sines, no DYN,
  // octave 0, no warp, no slices, flat, quantiser off
  [P.PMIX]: 0, [P.PVOI]: mid(7, 16), [P.PBLR]: 0.2, [P.PGLD]: 0.2, [P.PCTR]: 0.5, [P.PWID]: 0.5, [P.PSPD]: 0.5,
  [P.PWAV]: 0, [P.PDYN]: 0, [P.POCT]: mid(2, 5), [P.PWRP]: 0.5, [P.PSLC]: mid(0, 7), [P.PTLT]: 0.5, [P.PQNT]: mid(0, 2) };
// v10: TONE flat; the followers on the track itself, 0 dB, the whole band, following, 5 ms / 150 ms, no depth, no target
const V10_PAD = { [P.HPF]: 0, [P.LPF]: 1, [P.LOW]: 0.5, [P.LOWF]: 0.5, [P.MID]: 0.5, [P.MIDF]: 0.5, [P.HIGH]: 0.5, [P.HIGF]: 0.5,
  ...Object.fromEntries([P.E1SR, P.E2SR].flatMap(b => [[b, mid(0, EF_SRC.length)], [b + 1, 0.25], [b + 2, mid(0, 4)], [b + 3, 0],
    [b + 4, 0.4247], [b + 5, 0.5088], [b + 6, 0.5], [b + 7, mid(0, NDEST)]])) };
// v13: HALO's VOICE side: a 0.3 s swell, no glide, KEY, root position, a little drift and air, half wide
const HALO_PAD = { [P.HSWL]: 0.55, [P.HGLD]: 0, [P.HMOD]: mid(0, 12), [P.HINV]: 0, [P.HDRF]: 0.3, [P.HAIR]: 0.15, [P.HWID]: 0.5 };
// v14: BREAK's CHOP side: no chops, free draws, no swap
const BRK_PAD = { [P.BRND]: 0, [P.BREV]: 0, [P.BSTU]: 0, [P.BPJM]: 0, [P.BSKP]: 0, [P.BSED]: mid(0, BRK_SEEDS), [P.BSWP]: 0 };
// v19 (v59): HARM, HALO's TONE, STRUM's and CHOIR's sides. An older project keeps HARM FREE (its chords as they were)
// and HALO's TONE at rest (core/sk_engine.cpp defaultTrack); a new track starts on PURE (defaultParams)
const V19_PAD = { [P.HARM]: mid(0, 3), [P.HUNI]: 0, [P.HSUB]: 0, [P.HCUT]: 1, [P.HOPN]: 0, [P.HVIB]: 0, [P.HWRM]: 0,
  [P.SHUM]: 0.25, [P.SACC]: 0.5, [P.SRNG]: 0.35, [P.SMAT]: mid(0, 6), [P.SPCK]: 0.55, [P.SDMP]: 0.55, [P.SBDY]: 0.6, [P.SMUT]: 0, [P.SDBL]: 0, [P.SSTF]: 0,
  [P.CSWL]: 0.45, [P.CFRM]: 0.5, [P.CEFF]: 0.45, [P.CVRT]: 0.4, [P.CPRT]: mid(0, 6), [P.CSCP]: 0.35, [P.CDRF]: 0.4,
  [P.CSPC]: 0.35, [P.CSIZ]: 0.6, [P.CDCY]: 0.6, [P.CDMP]: 0.35, [P.CMOD]: 0.4, [P.CSHM]: mid(0, 4), [P.CPRE]: 0.15 };
// v14 -> v15: BREAK leaves its own breaks for the user's slots: its break knob goes to slot 1, KIT becomes SYNC / VARI
// (the old MODE), the ghost notes' weight SKIP (none), the old MODE SWAP (none); so do their p-locks
function brkV15(s) {
  for (const t of s.tracks) {
    const isB = mof20(t.p[P.MACH]) === M20.BREAK, mode = t.p[P.BSWP];
    if (isB) { t.p[P.A] = mid(0, BRK_SLOTS); t.p[P.B] = typeof mode === 'number' ? mode : mid(0, 2); }
    t.p[P.BSKP] = 0; t.p[P.BSWP] = 0;
    if (t.pre && t.pre.m === M20.BREAK) delete t.pre;
    for (const st of t.steps || []) {
      const l = st && st.l; if (!l) continue;
      const onB = P.MACH in l ? mof20(l[P.MACH]) === M20.BREAK : isB;
      if (onB) { delete l[P.A]; delete l[P.B]; if (P.BSWP in l) l[P.B] = l[P.BSWP]; }
      delete l[P.BSKP]; delete l[P.BSWP];
    }
  }
}
// v16: the FX machines' MORE side (a track on another machine keeps them at 0, as the engine starts them)
const FX_PAD = { [P.XA]: 0, [P.XB]: 0, [P.XC]: 0, [P.XD]: 0, [P.XE]: 0, [P.XF]: 0, [P.XG]: 0 };
// v20 (v60): AGE at rest (in tune, even, no sheen, no dropouts), STRUM's AIR off (what a project of v59 played; a new
// track starts with a breath of it: defaultParams); v21 (v61): the side places at 0 (toV21 gives ALLOY its own)
const V20_PAD = { [P.GDTN]: 0, [P.GVAR]: 0, [P.GSHN]: 0, [P.GDRP]: 0, [P.SAIR]: 0, ...Object.fromEntries(SP(NSIDE).map(q => [q, 0])),
  // v64: an older project, no pan drawn, no spread (MONO at its middle)
  [P.RPAN]: 0.5, [P.SPRD]: 0, [P.SMON]: 0.5,
  // v65: no decay drawn; SLEW's glide half the cycle, LOOP's 8 draws (the seeds: new ones when they are first played)
  [P.RDEC]: 0.5, [P.RDSD]: 0, [P.L1SL]: 0.5, [P.L1LN]: mid(6, 10), [P.L1SD]: 0, [P.L2SL]: 0.5, [P.L2LN]: mid(6, 10), [P.L2SD]: 0,
  // v68: no CHORUS nor PHASER send
  [P.CHO]: 0, [P.PHS]: 0 };
const STRUM_AIR0 = 0.3;
const padVal = i => i === P.PNCH ? 0 : i === P.JRNG ? mid(11, NJRNG) : i in V20_PAD ? V20_PAD[i] : i in V19_PAD ? V19_PAD[i] : i in RAM_PAD ? RAM_PAD[i] : i in V10_PAD ? V10_PAD[i] : i in HALO_PAD ? HALO_PAD[i] : i in BRK_PAD ? BRK_PAD[i] : i in FX_PAD ? FX_PAD[i] : 0.5;
// v78: a track's params padded, its machine known: the side places added (S36 ..) at its machine's own, ECRV at EXP
const padValM = (i, m) => isSide(i) ? (MACH[m] && MACH[m].sd ? MACH[m].sd[sideIx(i)] || 0 : 0) : i === P.ECRV ? 1 : i === P.DRON ? 0 : padVal(i);
function padTrackP(p) { const m = mof(p[P.MACH]); while (p.length < NP) p.push(padValM(p.length, m)); }
function padParams(s) {
  for (const t of s.tracks) padTrackP(t.p);
  while (s.g.length < NG) s.g.push(gDefault(s.g.length));
  s.g[G.XMSK] = rMask(s);
  s.g[G.LOOP] = 0;   // the looper is a performance control: a project never opens looping
}

// ------------------------------------------------------------------ v18: 64 steps, patterns, HOLD
const emptySteps = () => Array.from({ length: NSTEP }, () => ({ on: 0, l: {} }));
// the machines whose DEC turns into HOLD at its top (an amplitude envelope; core/sk_engine.cpp holdMachine). RAM keeps
// its own HOLD (the take plays out)
const NO_HOLD = new Set(['CP·NZ', 'FRACT', 'ITERA', 'HALO', 'BREAK', 'RAM', 'VERB', 'ECHO', 'BEND', 'TAPE', 'CODEC', 'STRUM', 'CHOIR', 'CHIMERA', 'FUZZ', 'WEFT', 'HOWL', 'STENCIL', 'MANTRA']);
const holdM = m => MACH[m] && !NO_HOLD.has(MACH[m].id);
// v81: the machines that can hold a drone (core/sk_engine.cpp droneMachine): those with HOLD, the chord and voice
// machines, HOWL, (v84) MANTRA, RAM on a loop
const droneM = (m, p) => holdM(m) || m === HALO_M || m === STRUM_M || m === CHOIR_M || m === HOWL_M || m === MANTRA_M || (m === RAM_M && idx(p[P.C], 4) >= 2);
const droneOn = t => { const tr = state.tracks[t]; return tr.p[P.DRON] >= 0.5 && droneM(mof(tr.p[P.MACH]), tr.p); };
const holdM20 = m => !!MACH20[m] && !NO_HOLD.has(MACH20[m]);
// an older track: 64 steps, and DEC at its top keeps its longest decay (it would hold now; RAM held already)
function v18Track(t) {
  if (!Array.isArray(t.steps)) t.steps = [];
  while (t.steps.length < NSTEP) t.steps.push({ on: 0, l: {} });
  const m = mof20(t.p[P.MACH]), fix = holdM20;
  if (fix(m) && t.p[P.DEC] >= 0.99) t.p[P.DEC] = 0.985;
  for (const st of t.steps) if (st.l && P.DEC in st.l && st.l[P.DEC] >= 0.99 && fix(P.MACH in st.l ? mof20(st.l[P.MACH]) : m)) st.l[P.DEC] = 0.985;
}
function toV18(s) {
  for (const t of s.tracks) v18Track(t);
  s.v = SV18;
  patFix(s);
}
// v18 -> v19 (v59): PROG's list grew (the classical progressions, the pedal and the tintinnabuli, before WALK) and HALO's
// switch a third side (TONE): a HALO track's PROG and F, and their locks, onto the new grids (in its patterns too)
const progV19 = v => { const i = idx(v, NPROG18); return mid(i === NPROG18 - 1 ? PROG_WALK : i, NPROG); };
const sideV19 = v => mid(idx(v, 2), 3);
function v19Track(t) {
  const m = mof20(t.p[P.MACH]);
  if (m === M20.HALO) { t.p[P.B] = progV19(t.p[P.B]); t.p[P.F] = sideV19(t.p[P.F]); }
  for (const st of t.steps || []) {
    if (!st || !st.l) continue;
    const sm = P.MACH in st.l ? mof20(st.l[P.MACH]) : m;
    if (sm !== M20.HALO) continue;
    if (P.B in st.l) st.l[P.B] = progV19(st.l[P.B]);
    if (P.F in st.l) st.l[P.F] = sideV19(st.l[P.F]);
  }
}
function toV19(s) {
  for (const t of s.tracks) v19Track(t);
  for (const pt of s.pats || []) if (pt && pt.tracks) for (const t of pt.tracks) v19Track(t);
  s.v = SV19;
}
// v19 -> v20 (v60): HALO's and STRUM's switch a fourth side (AGE): their F (and its locks) onto the new grid
const sideV20 = v => mid(idx(v, 3), 4);
const side4M20 = m => m === M20.HALO || m === M20.STRUM;
function v20Track(t) {
  const m = mof20(t.p[P.MACH]);
  if (side4M20(m)) t.p[P.F] = sideV20(t.p[P.F]);
  for (const st of t.steps || []) {
    if (!st || !st.l || !(P.F in st.l)) continue;
    if (side4M20(P.MACH in st.l ? mof20(st.l[P.MACH]) : m)) st.l[P.F] = sideV20(st.l[P.F]);
  }
}
function toV20(s) {
  for (const t of s.tracks) v20Track(t);
  for (const pt of s.pats || []) if (pt && pt.tracks) for (const t of pt.tracks) v20Track(t);
  s.v = SV20;
}
// v20 -> v21 (v61): the machines renumbered; a track or a lock on one of the five gone goes to MATTER or LATEX then, to
// ALLOY now (v62: straight onto today's numbers), A..F and the side pages at its defaults (the A..F locks made for the
// old machine go); the side places (v20: STRING's and MODAL's MORE page in the first six, and their locks) at the
// machine's defaults; a preset of a machine gone, forgotten
const om21 = m => { const id = MACH20[m] || 'BD·VA'; return MACH.findIndex(x => x.id === (GONE21[id] || id)); };
const mach20 = m => MACH[om21(m)];
const sideDefs = m => SP(NSIDE).map((q, i) => (MACH[m].sd ? MACH[m].sd[i] || 0 : 0));
function v21Track(t) {
  const m20 = mof20(t.p[P.MACH]), gone = MACH20[m20] in GONE21, m = om21(m20);
  t.p[P.MACH] = mv(m);
  if (gone) { const d = MACH[m].d; for (let i = 0; i < 6; i++) t.p[P.A + i] = d[2 + i]; }
  sideDefs(m).forEach((v, i) => { t.p[SQ(i)] = v; });
  if (t.pre) { const pm = t.pre.m; t.pre = typeof pm === 'number' && MACH20[pm] && !(MACH20[pm] in GONE21) ? Object.assign({}, t.pre, { m: om21(pm) }) : null; }
  for (const st of t.steps || []) {
    const l = st && st.l; if (!l) continue;
    SP(NSIDE).forEach(q => { delete l[q]; });
    if (P.MACH in l) {
      const lm20 = mof20(l[P.MACH]), lgone = MACH20[lm20] in GONE21, lm = om21(lm20);
      l[P.MACH] = mv(lm);
      if (lgone) { const d = MACH[lm].d; AF.forEach((q, i) => { l[q] = d[2 + i]; }); }
      if (lgone && lm !== m) sideDefs(lm).forEach((v, i) => { if (MACH[lm].sd) l[SQ(i)] = v; });
    } else if (gone) AF.forEach(q => { delete l[q]; });
  }
}
function toV21(s) {
  for (const t of s.tracks) v21Track(t);
  for (const pt of s.pats || []) if (pt && pt.tracks) for (const t of pt.tracks) v21Track(t);
  s.v = SV;
}
// v21 -> v22 (v62): MATTER and LATEX (37, 38) both onto ALLOY (37), A..F and the side pages at its defaults (the A..F
// and side locks made for them go); a step locked onto either: ALLOY at its defaults; their presets, forgotten
function v22Track(t) {
  const m21 = mof21(t.p[P.MACH]), gone = gone22(m21), m = gone ? ALLOY_M : m21;
  t.p[P.MACH] = mv(m);
  if (gone) { const d = MACH[m].d; for (let i = 0; i < 6; i++) t.p[P.A + i] = d[2 + i]; sideDefs(m).forEach((v, i) => { t.p[SQ(i)] = v; }); }
  if (t.pre && typeof t.pre.m === 'number' && gone22(t.pre.m)) t.pre = null;
  for (const st of t.steps || []) {
    const l = st && st.l; if (!l) continue;
    if (P.MACH in l) {
      const lm21 = mof21(l[P.MACH]), lgone = gone22(lm21);
      l[P.MACH] = mv(lgone ? ALLOY_M : lm21);
      if (lgone) {
        const d = MACH[ALLOY_M].d; AF.forEach((q, i) => { l[q] = d[2 + i]; });
        for (let q = P.S1; q <= P.S35; q++) delete l[q];
        if (!gone) sideDefs(ALLOY_M).forEach((v, i) => { l[SQ(i)] = v; });
      }
    } else if (gone) { AF.forEach(q => { delete l[q]; }); for (let q = P.S1; q <= P.S35; q++) delete l[q]; }
  }
}
function toV22(s) {
  for (const t of s.tracks) v22Track(t);
  for (const pt of s.pats || []) if (pt && pt.tracks) for (const t of pt.tracks) v22Track(t);
  s.v = SV;
}
// v22 -> v23 (v63): ALLOY's 21 sides (EXCITE, LAYERS, COLOR) onto its 35 (IMPACT, NOISE, FM, COLOR, SPACE; core/sk_engine.h
// alloySide23): where each went, BNCE bipolar now (its old range on the right half), PENV down to three octaves under
// now (the same pitch), the new ones at rest (NCOL the colour MALT gave the noise, no reverb); the locks of a step
// that plays ALLOY with them (one locked onto ALLOY from another machine: the new ones at rest in it too). (A project
// older than v22 had no ALLOY of its own: the conversions put today's defaults in)
const AL23_TO = [0, 7, 10, 1, 2, 4, 5, 14, 15, 16, 17, 19, 21, 22, 23, 24, 25, 26, 27, 34, 13];
const AL23_REST = { 20: 0.5, 29: 0.3, 30: 0.4, 31: 0.4, 32: mid(0, 13) };
const al23Val = (q, v) => q === 6 ? 0.5 + 0.5 * v : (q === 4 && v < 0.5) ? 0.5 - (0.5 - v) * 2 / 3 : v;
const al23Sides = o => {
  const n = Array.from({ length: NSIDE_A }, (_, i) => AL23_REST[i] || 0);
  for (let q = 0; q < 21; q++) n[AL23_TO[q]] = al23Val(q, o[q] || 0);
  n[9] = 0.2 + 0.75 * (o[0] || 0);
  return n;
};
// (a dict of side locks or a preset's params: those of the 21 there moved; full: the new ones at rest too)
function al23Dict(l, full) {
  const old = {};
  for (let q = 0; q < 21; q++) if ((P.S1 + q) in l) { old[q] = l[P.S1 + q]; delete l[P.S1 + q]; }
  for (const q in old) l[P.S1 + AL23_TO[q]] = al23Val(+q, old[q]);
  // (the new ones only: an old one not there was the other machine's, at rest, as it still is)
  if (full) for (let i = 0; i < NSIDE_A; i++) if (!AL23_TO.includes(i)) l[P.S1 + i] = i === 9 ? 0.2 + 0.75 * (old[0] !== undefined ? old[0] : 0.75) : (AL23_REST[i] || 0);
}
function v23Track(t) {
  const m = mof(t.p[P.MACH]);
  if (m === ALLOY_M) al23Sides(SP(21).map(q => t.p[q])).forEach((v, i) => { t.p[P.S1 + i] = v; });
  for (const st of t.steps || []) {
    const l = st && st.l; if (!l) continue;
    const lm = P.MACH in l ? mof(l[P.MACH]) : m;
    if (lm === ALLOY_M) al23Dict(l, m !== ALLOY_M);
  }
}
function toV23(s) {
  for (const t of s.tracks) v23Track(t);
  for (const pt of s.pats || []) if (pt && pt.tracks) for (const t of pt.tracks) v23Track(t);
  s.v = SV;
}
// v24 -> v25 (v65): the LFO waves, 12 (six shapes, then the same six restarting with each trig) onto 16 (SLEW and LOOP after
// EXP, in both halves): each wave stays the one it was, on the new grid, its locks too (the new settings come at rest:
// padParams). For any older project, after its other conversions
const waveV25 = v => { const i = idx(v, 12); return mid(i < 6 ? i : i + 2, NWAV); };
const LFO_WAVES = [P.LWAV, P.L2WV];
function v25Track(t) {
  for (const q of LFO_WAVES) if (t.p.length > q) t.p[q] = waveV25(t.p[q]);
  for (const st of t.steps || []) { const l = st && st.l; if (l) for (const q of LFO_WAVES) if (q in l) l[q] = waveV25(l[q]); }
}
function toV25(s) {
  for (const t of s.tracks) v25Track(t);
  for (const pt of s.pats || []) if (pt && pt.tracks) for (const t of pt.tracks) v25Track(t);
  s.v = SV;
}
// a pattern in its slot (in memory): per track its sound, steps, length, speed, quantiser and preset; its CHG. Saved
// packed: its steps sparse (st: { step: {...} }), a pattern never written: null. The one playing lives in s.tracks.
const PAT_TK = ['p', 'len', 'spd', 'q', 'pre'];
function patOf(tracks, chg) {
  return { chg: chg | 0, tracks: tracks.map(t => ({ p: t.p.slice(), len: t.len, spd: t.spd, q: t.q === 0 ? 0 : 1, pre: t.pre ? Object.assign({}, t.pre) : null,
    steps: t.steps.map(st => JSON.parse(JSON.stringify(st))) })) };
}
function patPack(pt) {
  if (!pt) return null;
  return { chg: pt.chg | 0, tr: pt.tracks.map(t => {
    const o = {}; for (const k of PAT_TK) if (t[k] != null) o[k] = t[k];
    o.st = {}; t.steps.forEach((st, i) => { if (st.on || st.rec || st.tl || (st.r > 1) || Object.keys(st.l).length) o.st[i] = st; });
    return o; }) };
}
function patUnpack(o) {
  if (!o || !Array.isArray(o.tr) || o.tr.length !== NT) return null;
  return { chg: o.chg | 0, tracks: o.tr.map(t => {
    const p = Array.isArray(t.p) ? t.p.slice() : defaultParams(0);
    padTrackP(p);
    const steps = emptySteps();
    if (t.st) for (const k in t.st) { const i = +k; if (i >= 0 && i < NSTEP && t.st[k]) steps[i] = Object.assign({ on: 0, l: {} }, t.st[k]); }
    return { p, len: clamp(t.len | 0 || 16, 1, NSTEP), spd: t.spd == null ? 4 : t.spd, q: t.q === 0 ? 0 : 1, pre: t.pre || null, steps };
  }) };
}
// a project's patterns: unpacked (or empty), the chain, the change mode, CHG of the pattern playing
function patFix(s) {
  const src = Array.isArray(s.pats) ? s.pats : [];
  s.pats = Array.from({ length: NPAT }, (_, i) => { const x = src[i]; return x && x.tr ? patUnpack(x) : x && x.tracks ? x : null; });
  s.pat = clamp(s.pat | 0, 0, NPAT - 1); s.pats[s.pat] = null;
  s.chain = Array.isArray(s.chain) ? s.chain.map(i => clamp(i | 0, 0, NPAT - 1)).slice(0, 64) : [];
  s.pmode = clamp(s.pmode | 0, 0, 2); s.pchg = s.pchg | 0;
  for (const t of s.tracks) { if (!Array.isArray(t.steps)) t.steps = []; while (t.steps.length < NSTEP) t.steps.push({ on: 0, l: {} }); }
}
// the project as it is saved (its patterns packed)
const serialState = () => JSON.stringify(Object.assign({}, state, { wst: 1, v78: 1, v82: 1, v83: 1, pats: state.pats.map(patPack) }));

// GLITCH's RAND moves the tracks marked r (rx = 0): their mask, for the engine
const rMask = s => s.tracks.reduce((m, t, i) => m | (t.rx ? 0 : 1 << i), 0) / 255;
// a track's sound and steps as ops (on steps cleared before: only what is there is sent)
function trackOps(ops, t, ti) {
  ops.push(['len', ti, t.len], ['spd', ti, t.spd], ['qtrk', ti, t.q === 0 ? 0 : 1]);
  t.p.forEach((v, p) => ops.push(['p', ti, p, v]));
  t.steps.forEach((st, si) => {
    if (st.on) ops.push(['trig', ti, si, 1]);
    if (st.rec) ops.push(['rec', ti, si, 1]);
    if (st.tl && !st.on) ops.push(['tlk', ti, si, 1]);
    if (st.r > 1) ops.push(['rat', ti, si, st.r]);
    for (const key in st.l) ops.push(['lock', ti, si, +key, st.l[key]]);
  });
}
// a pattern into its slot (the engine edits it there: epat), then back to the one playing
function patOps(ops, i, pt) {
  ops.push(['epat', i], ['pclr', i]);
  if (pt) { pt.tracks.forEach((t, ti) => trackOps(ops, t, ti)); ops.push(['pchg', pt.chg | 0]); }
  ops.push(['epat', state.pat]);
}
function chainOps(ops, chain) {
  ops.push(['pchn', 0]);
  chain.forEach((p, k) => ops.push(['pch', k, p]));
  ops.push(['pchn', chain.length]);
}
function stateOps(s) {
  const ops = [['reset'], ['pall'], ['pcur', s.pat | 0]];
  // (messages from before this reach the page a moment later: it waits for the engine to play this pattern)
  ui.patWait = { p: s.pat | 0, t: performance.now() }; ui.spage = 0;
  s.g.forEach((v, i) => ops.push(['g', i, i === G.XMSK ? rMask(s) : v]));
  ops.push(['scale', s.scale | 0]);
  s.tracks.forEach((t, ti) => { ops.push(['mute', ti, t.mute]); trackOps(ops, t, ti); });
  // the other patterns into their slots, then the edits back on the one playing, its CHG, the chain
  (s.pats || []).forEach((pt, i) => { if (pt && i !== (s.pat | 0)) { ops.push(['epat', i], ['pclr', i]); pt.tracks.forEach((t, ti) => trackOps(ops, t, ti)); ops.push(['pchg', pt.chg | 0]); } });
  ops.push(['epat', s.pat | 0], ['pchg', s.pchg | 0]);
  chainOps(ops, s.chain || []);
  return ops;
}

let state = demoState();

// ------------------------------------------------------------------ backends
const inPlugin = !!(window.__JUCE__ && window.__JUCE__.backend) || location.hostname === 'juce.backend';
// the plugin: one 1280 x 720 panel in five pages, scaled to the window (a 16:9 one: the editor keeps the ratio)
const PLUG_W = 1280, PLUG_H = 720;
let uiScale = 1;
function fitPlug() {
  const vw = window.innerWidth || PLUG_W, vh = window.innerHeight || PLUG_H;
  uiScale = Math.min(vw / PLUG_W, vh / PLUG_H) || 1;
  const st = document.documentElement.style;
  st.setProperty('--uis', String(uiScale));
  st.setProperty('--uix', ((vw - PLUG_W * uiScale) / 2).toFixed(2) + 'px');
  st.setProperty('--uiy', ((vh - PLUG_H * uiScale) / 2).toFixed(2) + 'px');
}
if (inPlugin) {
  document.documentElement.classList.add('plug');
  // on a phone-sized view (iOS) the page still lays out at the panel's width
  const vp = document.querySelector('meta[name="viewport"]');
  if (vp && window.innerWidth < 760) vp.setAttribute('content', 'width=' + PLUG_W);
  fitPlug();
  window.addEventListener('resize', fitPlug);
}
// canvas pixels: the screen's density, times the plugin's scale
const DPR = () => (window.devicePixelRatio || 1) * uiScale;
const STORE = 'impakt.state.v4', OLD_STORES = ['impakt.state.v3', 'impakt.state.v2', 'impakt.state.v1'];

const ui = {
  t: 0, page: 1, gpage: PAGES.findIndex(p => p.id === 'EQ'), ptab: 'ms', lock: null, all: false, lastP: P.TUNE, playing: false, picker: false, ratStep: -1, undo: null,
  pos: { steps: new Array(NT).fill(-1), hits: new Array(NT).fill(0), run: 0, bpm: 0, host: 0 },
  seen: new Array(NT).fill(0), flash: new Array(NT).fill(0), engine: '', ramInfo: null, ramPeaks: new Array(NT).fill(null),
  brks: null, brkNames: new Array(BRK_SLOTS).fill(''), brkBusy: -1, brkTarget: -1, lpanel: false, lmsg: '',
  cube: null, cubes: null, cubeNames: new Array(8).fill(''), cubeMask: -1,
  radio: null, radios: null, rpanel: false, radT: -1,
  upd: null, upanel: false, updToastOff: false,     // (v87: the updater, its panel, the banner put aside)
  mtr: { gr: 0, out: 0, grShow: 0, outShow: -60, t: 0 },
};
const $ = id => document.getElementById(id);
const backend = inPlugin ? makeJuceBackend() : makeWebBackend();

function makeJuceBackend() {
  let batch = [], scheduled = false, saveTimer = 0;
  const be = () => window.__JUCE__ && window.__JUCE__.backend;
  const flush = () => { scheduled = false; if (batch.length && be()) be().emitEvent('impakt_ops', { ops: batch }); batch = []; };
  // BREAK: a loop goes to the plugin as base64 of its 16-bit samples (the project keeps it)
  const b64 = a => { const u = new Uint8Array(a.buffer, a.byteOffset, a.byteLength); let bin = ''; for (let i = 0; i < u.length; i += 0x8000) bin += String.fromCharCode.apply(null, u.subarray(i, i + 0x8000)); return btoa(bin); };
  return {
    kind: 'plugin',
    brkLoad(slot, rec) { ui.brkNames[slot] = rec.name; if (be()) be().emitEvent('impakt_brk', { slot, bars: rec.bars, name: rec.name, data: b64(rec.data) }); },
    brkClear(slot) { ui.brkNames[slot] = ''; this.send(['bclr', slot]); },
    brkBars(slot, bars) { this.send(['bbar', slot, bars]); },
    // CUBE: a bank goes to the plugin as base64 of its 16-bit samples (the project keeps it)
    cubeLoad(bank, rec) { ui.cubeNames[bank] = rec.name; cubeMem[bank] = rec; if (be()) be().emitEvent('impakt_cube', { bank, name: rec.name, data: b64(rec.data) }); },
    cubeClear(bank) { ui.cubeNames[bank] = ''; cubeMem[bank] = null; this.send(['cclr', bank]); },
    // RADIO (v87): a receiver tuned (the plugin connects it and keeps it in the project), the stations' directory, the
    // receivers' list (their answers come back with the position: radq)
    radioSet(k, src) { if (be()) be().emitEvent('impakt_radio', { op: 'set', rx: k, src: src || {} }); },
    radioSearch(id, q) { if (be()) be().emitEvent('impakt_radio', Object.assign({ op: 'search', id }, q)); },
    radioReceivers(id, fresh) { if (be()) be().emitEvent('impakt_radio', { op: 'receivers', id, fresh: !!fresh }); },
    enableInput() {},          // the host routes its input to the plugin
    send(op) { batch.push(op); if (!scheduled) { scheduled = true; Promise.resolve().then(flush); } },
    sendAll(ops) { flush(); if (be()) be().emitEvent('impakt_ops', { ops }); },
    ensureAudio() {},
    save() { clearTimeout(saveTimer); saveTimer = setTimeout(() => { if (be()) be().emitEvent('impakt_state', { json: serialState() }); }, 300); },
    init() {
      const go = () => {
        const b = be();
        if (!b) { setTimeout(go, 50); return; }
        let answered = false;
        // first load: restore the saved project (or push the demo); later loads: host recalled a preset
        b.addEventListener('impakt_load', payload => {
          let s = null;
          try { s = payload && payload.json ? JSON.parse(payload.json) : null; } catch (e) { s = null; }
          const old = s && s.v !== SV;
          s = migrate(s);
          if (validState(s)) {
            state = s; ui.lock = null; renderAll();
            if (old) { this.sendAll(stateOps(state)); this.save(); }
          } else if (!answered) { this.sendAll(stateOps(state)); this.save(); }
          answered = true;
        });
        b.addEventListener('impakt_pos', onPos);
        b.emitEvent('impakt_hello', {});
        setTimeout(() => { if (!answered) { answered = true; this.sendAll(stateOps(state)); this.save(); } }, 1200);
      };
      go();
      $('notice').hidden = true;
    },
  };
}

function b64ToBytes(b64) {
  const bin = atob(b64), out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
  return out;
}

// RAM machine: the takes are kept in this browser (IndexedDB), one per track. Where IndexedDB is not allowed they
// last until the page closes.
const ramStore = {
  db: null,
  open() {
    if (this.db) return this.db;
    this.db = new Promise((res, rej) => {
      try {
        const r = indexedDB.open('impakt.ram', 1);
        r.onupgradeneeded = () => r.result.createObjectStore('take');
        r.onsuccess = () => res(r.result); r.onerror = () => rej(r.error);
      } catch (e) { rej(e); }
    });
    return this.db;
  },
  async run(mode, f) {
    const db = await this.open();
    return new Promise((res, rej) => { const tx = db.transaction('take', mode), q = f(tx.objectStore('take')); tx.oncomplete = () => res(q && q.result); tx.onerror = () => rej(tx.error); });
  },
  put(t, rate, data) { return this.run('readwrite', s => s.put({ rate, data }, t)).catch(() => {}); },
  del(t) { return this.run('readwrite', s => s.delete(t)).catch(() => {}); },
  async all() {
    try {
      const out = [];
      for (let t = 0; t < NT; t++) { const r = await this.run('readonly', s => s.get(t)); if (r && r.data && r.data.length) out.push({ t, rate: r.rate, data: r.data }); }
      return out;
    } catch (e) { return []; }
  },
};


// BREAK's slots. In the browser the loops are kept here (IndexedDB), one per slot: the name, the bars and the samples as
// the engine reads them (mono, 32 kHz, 16-bit); the page keeps them too (brkMem) to hand them to the engine when it
// starts. In the plugin they live in the project, which the host saves.
const brkStore = {
  db: null,
  open() {
    if (this.db) return this.db;
    this.db = new Promise((res, rej) => {
      try {
        const r = indexedDB.open('impakt.brk', 1);
        r.onupgradeneeded = () => r.result.createObjectStore('slot');
        r.onsuccess = () => res(r.result); r.onerror = () => rej(r.error);
      } catch (e) { rej(e); }
    });
    return this.db;
  },
  async run(mode, f) {
    const db = await this.open();
    return new Promise((res, rej) => { const tx = db.transaction('slot', mode), q = f(tx.objectStore('slot')); tx.oncomplete = () => res(q && q.result); tx.onerror = () => rej(tx.error); });
  },
  put(s, rec) { return this.run('readwrite', st => st.put({ name: rec.name, bars: rec.bars, data: rec.data }, s)).catch(() => {}); },
  del(s) { return this.run('readwrite', st => st.delete(s)).catch(() => {}); },
  async all() {
    try {
      const out = [];
      for (let s = 0; s < BRK_SLOTS; s++) { const r = await this.run('readonly', st => st.get(s)); if (r && r.data && r.data.length) out.push({ slot: s, name: r.name || '', bars: r.bars | 0, data: r.data }); }
      return out;
    } catch (e) { return []; }
  },
};
const brkMem = new Array(BRK_SLOTS).fill(null);
// RADIO (v87): the recordings the page plays as receivers (the browser: where the network is closed to it)
const radioFileStore = {
  db: null,
  open() {
    if (this.db) return this.db;
    this.db = new Promise((res, rej) => {
      try { const r = indexedDB.open('impakt.radio', 1); r.onupgradeneeded = () => r.result.createObjectStore('rx'); r.onsuccess = () => res(r.result); r.onerror = () => rej(r.error); }
      catch (e) { rej(e); }
    });
    return this.db;
  },
  async run(mode, f) {
    const db = await this.open();
    return new Promise((res, rej) => { const tx = db.transaction('rx', mode), q = f(tx.objectStore('rx')); tx.oncomplete = () => res(q && q.result); tx.onerror = () => rej(tx.error); });
  },
  put(k, pcm) { return this.run('readwrite', st => st.put(pcm, k)).catch(() => {}); },
  async all() {
    const out = [];
    try { for (let k = 0; k < 4; k++) { const pcm = await this.run('readonly', st => st.get(k)); if (pcm && pcm.length) out.push({ k, pcm }); } } catch (e) {}
    return out;
  },
};

// a file's audio, mixed to mono at 32 kHz (the browser decodes it: WAV, AIFF, MP3, M4A / AAC, CAF, and FLAC or OGG
// where it reads them)
async function brkDecodeAudio(bytes) {
  const AC = window.OfflineAudioContext || window.webkitOfflineAudioContext;
  if (!AC) throw new Error('this browser cannot decode audio');
  const ctx = new AC(1, BRK_RATE, BRK_RATE);
  const buf = await new Promise((res, rej) => {
    try { const q = ctx.decodeAudioData(bytes, res, rej); if (q && q.then) q.then(res, rej); } catch (e) { rej(e); }
  });
  const n = buf.length, ch = buf.numberOfChannels, x = new Float32Array(n);
  for (let c = 0; c < ch; c++) { const d = buf.getChannelData(c); for (let i = 0; i < n; i++) x[i] += d[i] / ch; }
  if (Math.abs(buf.sampleRate - BRK_RATE) < 1) return x;
  // (a browser that kept the file's rate: read again at 32 kHz)
  const r = buf.sampleRate / BRK_RATE, m = Math.floor(n / r), y = new Float32Array(m);
  for (let i = 0; i < m; i++) { const q = i * r, i0 = Math.floor(q), f = q - i0; y[i] = x[i0] + ((i0 + 1 < n ? x[i0 + 1] : 0) - x[i0]) * f; }
  return y;
}
// the loop's tempo and bars: a BPM in its name ("amen_137bpm", "137 BPM", "_137_") and the bars nearest its length;
// else from its length (1, 2, 4 or 8 bars, the tempo between 70 and 190 nearest 130); a longer file: the project's
// tempo, as many bars as fit (20 s at most)
function brkTempo(name, secs, projBpm) {
  const m = name.match(/(\d{2,3}(?:[.,]\d+)?)\s*_?bpm/i) || name.match(/(?:^|[_\- ])(\d{2,3})(?=[_\- .]|$)/);
  const hint = m ? parseFloat(m[1].replace(',', '.')) : 0;
  const BARS = [1, 2, 4, 8], fit = (bpm, b) => b * 240 / bpm <= BRK_CAP_S + 1e-6;
  if (hint >= 60 && hint <= 200) {
    let best = 1;
    for (const b of BARS) if (fit(hint, b) && Math.abs(b * 240 / hint - secs) < Math.abs(best * 240 / hint - secs)) best = b;
    return { bpm: hint, bars: best, from: 'name' };
  }
  if (secs <= BRK_CAP_S + 0.05) {
    let best = null;
    for (const b of BARS) { const t = b * 240 / secs; if (t >= 70 && t <= 190 && (!best || Math.abs(t - 130) < Math.abs(best.bpm - 130))) best = { bpm: t, bars: b, from: 'length' }; }
    if (best) return best;
  }
  const bpm = clamp(projBpm || 120, 40, 300);
  let bars = 1; for (const b of BARS) if (b * 240 / bpm <= Math.min(secs, BRK_CAP_S) + 0.05) bars = b;
  return { bpm, bars, from: 'project' };
}
// the samples a slot gets: from just before the first sound, cut to the bars, a 2 ms fade where the loop wraps,
// levelled (-16 dBFS RMS, the peaks under -0.5 dBFS), 16-bit
function brkPrepare(x, name, projBpm) {
  let pk = 0; for (let i = 0; i < x.length; i++) pk = Math.max(pk, Math.abs(x[i]));
  if (!(pk > 1e-5)) throw new Error('silent');
  let i0 = 0; while (i0 < x.length && Math.abs(x[i0]) < pk * 0.05) i0++;
  const start = Math.max(0, i0 - Math.round(0.002 * BRK_RATE));
  const t = brkTempo(name, (x.length - start) / BRK_RATE, projBpm);
  const n = Math.min(Math.round(t.bars * 240 / t.bpm * BRK_RATE), BRK_CAP_S * BRK_RATE);
  const y = new Float32Array(n);
  y.set(x.subarray(start, Math.min(x.length, start + n)));
  const fd = Math.round(0.002 * BRK_RATE); for (let i = 0; i < fd && i < n; i++) y[n - 1 - i] *= i / fd;
  let s2 = 0, p2 = 0; for (let i = 0; i < n; i++) { s2 += y[i] * y[i]; p2 = Math.max(p2, Math.abs(y[i])); }
  const g = Math.min(Math.pow(10, -16 / 20) / (Math.sqrt(s2 / n) + 1e-12), Math.pow(10, -0.5 / 20) / (p2 + 1e-12));
  const data = new Int16Array(n);
  for (let i = 0; i < n; i++) data[i] = Math.max(-32767, Math.min(32767, Math.round(y[i] * g * 32767)));
  return { bars: t.bars, bpm: t.bpm, from: t.from, data };
}
// the page's own idea of the slots (before the engine runs): ready, the tempo from the bars and the length, the waveform
function brkLocal() {
  const info = new Array(BRK_SLOTS * 4).fill(0), peaks = [];
  for (let s = 0; s < BRK_SLOTS; s++) {
    const r = brkMem[s], pk = new Array(128).fill(0);
    if (r) {
      const n = r.data.length;
      info.splice(s * 4, 4, 2, r.bars * 240 * BRK_RATE / n, r.bars, n / BRK_RATE);
      for (let i = 0; i < 128; i++) { let m = 0; for (let j = Math.floor(i * n / 128); j < Math.floor((i + 1) * n / 128); j++) m = Math.max(m, Math.abs(r.data[j])); pk[i] = Math.min(1, m / (0.95 * 32767)); }
    }
    peaks.push(pk);
  }
  return { ver: -1, info, peaks };
}
const BRK_ACCEPT = 'audio/*,.wav,.wave,.aif,.aiff,.mp3,.m4a,.aac,.caf,.flac,.ogg';
// files into the slots: the first into slot s, the next ones into the empty slots after it
async function brkLoadFiles(files, s) {
  files = [...files].filter(Boolean);
  if (!files.length) return;
  backend.ensureAudio();
  const wasReady = curMachine() === BREAK_M && brkReady(idx(trk().p[P.A], BRK_SLOTS));
  const used = new Set();
  let first = -1, done = 0, failed = [];
  for (const f of files) {
    let slot = -1;
    if (!used.size && s >= 0) slot = s;
    else for (let k = 0; k < BRK_SLOTS; k++) { const q = ((s < 0 ? 0 : s) + k) % BRK_SLOTS; if (!used.has(q) && brkState(q) === 0) { slot = q; break; } }
    if (slot < 0) { failed.push(f.name + ' (the 10 slots are full)'); continue; }
    used.add(slot);
    ui.brkBusy = slot; brkMsg('reading “' + f.name + '”…');
    try {
      const name = f.name.replace(/\.[a-z0-9]{2,5}$/i, '').slice(0, 40);
      const x = await brkDecodeAudio(await f.arrayBuffer());
      const r = brkPrepare(x, name, state.g[G.BPM]);
      backend.brkLoad(slot, { name, bars: r.bars, data: r.data });
      if (first < 0) first = slot;
      done++;
    } catch (e) { failed.push(f.name); }
  }
  ui.brkBusy = -1;
  // the track shown plays the first slot loaded: the one tapped, or any when its own slot was empty
  if (first >= 0 && !ui.all && curMachine() === BREAK_M && !ui.lock && (s >= 0 || !wasReady)) setTrackParam(P.A, mid(first, BRK_SLOTS));
  brkMsg(failed.length ? (done ? done + ' loaded · ' : '') + 'not loaded: ' + failed.join(', ') : done + (done > 1 ? ' loops loaded' : ' loop loaded'));
  try { if (navigator.storage && navigator.storage.persist) navigator.storage.persist().catch(() => {}); } catch (e) {}
  brkChanged();
}
function brkMsg(m) {
  ui.lmsg = m; renderLoops();
  clearTimeout(brkMsg.t); brkMsg.t = setTimeout(() => { ui.lmsg = ''; renderLoops(); }, 4000);
}
// the slots changed (a loop read, cleared, its bars): the panel, the SLOT knob, the screen, the keyboard
function brkChanged() {
  renderLoops();
  if (curMachine() === BREAK_M) { renderKnobs(); renderScreen(); if (ui.key) { renderLockbar(); renderSteps(); } }
}
function brkWave(pk) {
  if (!pk) return '';
  let d = '';
  for (let i = 0; i < 64; i++) { const a = Math.max(pk[2 * i] || 0, pk[2 * i + 1] || 0), h = Math.max(0.4, a * 7.5); d += `M${i + 0.5} ${(8 - h).toFixed(2)}v${(2 * h).toFixed(2)}`; }
  return `<svg class="lwave" viewBox="0 0 64 16" preserveAspectRatio="none" aria-hidden="true"><path d="${d}"/></svg>`;
}
function renderLoops() {
  const panel = $('lpanel'), btn = $('mload');
  // (the same panel for CUBE's wavetables: renderCubes)
  const shownB = !ui.all && curMachine() === BREAK_M, shownC = !ui.all && curMachine() === CUBE_M;
  if (btn) { btn.hidden = !shownB && !shownC; btn.title = shownC ? 'The wavetables CUBE reads: eight places' : 'The loops BREAK plays: ten slots'; btn.setAttribute('aria-expanded', ui.lpanel ? 'true' : 'false'); }
  if (!panel) return;
  if (!shownB && !shownC) ui.lpanel = false;
  panel.hidden = !ui.lpanel;
  panel.setAttribute('aria-label', shownC ? 'Wavetables' : 'Loops');
  if ($('lfile')) $('lfile').accept = shownC ? '.wav,.wave,audio/wav,audio/x-wav' : BRK_ACCEPT;
  if (!ui.lpanel) return;
  if (shownC) { renderCubes(); return; }
  $('ltitle').textContent = 'LOOPS'; $('lload').title = 'Load loops into the empty slots'; $('llist').classList.remove('c8');
  if (ui.brkHint) $('lhint').textContent = ui.brkHint;
  const info = ui.brks ? ui.brks.info : null, n = info ? [...Array(BRK_SLOTS).keys()].filter(brkReady).length : 0;
  const cur = idx(trk().p[P.A], BRK_SLOTS);
  $('lsub').textContent = ui.lmsg || (n ? n + ' of 10 · shared by the BREAK tracks · tap one to play it on T' + (ui.t + 1) : 'up to 10 loops, shared by the BREAK tracks');
  $('llist').innerHTML = [...Array(BRK_SLOTS).keys()].map(s => {
    const st = brkState(s), busy = st === 1 || ui.brkBusy === s, nm = esc(brkName(s) || (st ? 'LOOP ' + (s + 1) : ''));
    const bpm = info ? info[s * 4 + 1] : 0, bars = info ? info[s * 4 + 2] | 0 : 0;
    if (!st && !busy) return `<div class="lrow empty${s === cur ? ' sel' : ''}" data-s="${s}" role="button" tabindex="0" aria-label="Slot ${s + 1}: empty, load a loop">`
      + `<span class="lnum">${s + 1}</span><span class="lname">Empty</span><span class="lmeta">tap to load</span></div>`;
    return `<div class="lrow${s === cur ? ' sel' : ''}${busy ? ' busy' : ''}" data-s="${s}" role="button" tabindex="0" aria-label="Slot ${s + 1}: ${nm}">`
      + `<span class="lnum">${s + 1}</span>${brkWave(ui.brks && ui.brks.peaks ? ui.brks.peaks[s] : null)}<span class="lname">${nm}</span>`
      + `<span class="lmeta">${busy ? 'reading…' : (Math.round(bpm * 10) / 10) + ' BPM · ' + bars + (bars > 1 ? ' bars' : ' bar')}</span>`
      + `<span class="lacts"><button class="lsm" data-a="half" aria-label="Half the tempo"${bars <= 1 || busy ? ' disabled' : ''}>÷2</button>`
      + `<button class="lsm" data-a="dbl" aria-label="Double the tempo"${bars >= BRK_MAXBARS || busy ? ' disabled' : ''}>×2</button>`
      + `<button class="lsm del" data-a="del" aria-label="Clear the slot">×</button></span></div>`;
  }).join('');
  $('llist').querySelectorAll('.lrow').forEach(row => {
    const s = +row.dataset.s;
    const open = () => { ui.brkTarget = s; const f = $('lfile'); f.value = ''; f.click(); };
    row.onclick = e => {
      const a = e.target.closest('.lsm');
      if (a) {
        e.stopPropagation();
        if (a.disabled) return;
        if (a.dataset.a === 'del') {
          if (!a.classList.contains('armed')) { a.classList.add('armed'); a.textContent = 'Clear?'; clearTimeout(a.t); a.t = setTimeout(() => { a.classList.remove('armed'); a.textContent = '×'; }, 2500); return; }
          backend.brkClear(s); brkMsg('slot ' + (s + 1) + ' cleared'); brkChanged(); return;
        }
        const bars = info[s * 4 + 2] | 0, nb = a.dataset.a === 'half' ? bars / 2 : bars * 2;
        if (nb >= 1 && nb <= BRK_MAXBARS && Number.isInteger(nb)) backend.brkBars(s, nb);
        return;
      }
      if (brkState(s) === 0) { open(); return; }
      if (curMachine() === BREAK_M && !ui.all) { setTrackParam(P.A, mid(s, BRK_SLOTS)); backend.ensureAudio(); send(['aud', ui.t, 1]); renderLoops(); }
    };
    row.onkeydown = e => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); row.click(); } };
    // a computer: files dropped on a slot
    row.ondragover = e => { e.preventDefault(); row.classList.add('drop'); };
    row.ondragleave = () => row.classList.remove('drop');
    row.ondrop = e => { e.preventDefault(); e.stopPropagation(); row.classList.remove('drop'); brkLoadFiles(e.dataTransfer.files, s); };
  });
}
function buildLoops() {
  const f = $('lfile');
  if (!f) return;
  f.accept = BRK_ACCEPT;
  ui.brkHint = $('lhint').textContent;
  const cubeOn = () => !ui.all && curMachine() === CUBE_M;
  f.addEventListener('change', () => { const t = ui.brkTarget; ui.brkTarget = -1; if (cubeOn()) cubeLoadFiles(f.files, t >= 0 ? t : -1); else brkLoadFiles(f.files, t >= 0 ? t : -1); });
  $('mload').addEventListener('click', () => { ui.lpanel = !ui.lpanel; if (ui.lpanel) { ui.ppanel = false; ui.rpanel = false; renderPresets(); renderRadioPanel(); } renderLoops(); });
  $('lclose').addEventListener('click', () => { ui.lpanel = false; renderLoops(); });
  $('lload').addEventListener('click', () => { ui.brkTarget = -1; f.value = ''; f.click(); });
  const panel = $('lpanel');
  panel.addEventListener('dragover', e => e.preventDefault());
  panel.addEventListener('drop', e => { e.preventDefault(); if (cubeOn()) cubeLoadFiles(e.dataTransfer.files, -1); else brkLoadFiles(e.dataTransfer.files, -1); });
}

// ------------------------------------------------------------------ RADIO (v87): its four receivers, the TUNE panel
// Shared by the RADIO tracks (core/sk_radio.h): each listens to an internet station (WEB: an MP3 stream) or to a
// shortwave receiver on the network at a frequency (SW). The plugin connects them itself (plugin/Source/RadioNet, its
// sources kept in the project); the browser as far as it is allowed to (makeRadioWeb, kept with the project).
// ui.radios: per receiver its source and state (src, st), the engine's view of each (info: RADIO_RX_INFO values: state,
// signal, level, seconds in hand, speed, dropouts, S-meter, waiting), the receivers the tracks play (use)
const RADIO_RX = 4, RADIO_NPEAK = 128, RADIO_HEAD = 16, RADIO_RXI = 8;
const RS = { NONE: 0, OFF: 1, CONNECT: 2, BUFFER: 3, LIVE: 4, TUNING: 5, BUSY: 6, FAIL: 7, STANDBY: 8 };
const RS_NAMES = ['NOT TUNED', 'OFF', 'CONNECTING', 'BUFFERING', 'ON AIR', 'TUNING', 'BUSY', 'FAILED', 'STANDBY'];
const SW_MODES = ['am', 'usb', 'lsb', 'cw'];
// the broadcast bands (a frequency in each, its usual mode), two amateur bands, the time signals
const SW_BANDS = [['LW', 198, 'am'], ['MW', 999, 'am'], ['120M', 2400, 'am'], ['90M', 3300, 'am'], ['75M', 3950, 'am'], ['60M', 4900, 'am'],
  ['49M', 6000, 'am'], ['41M', 7300, 'am'], ['31M', 9600, 'am'], ['25M', 11800, 'am'], ['22M', 13700, 'am'], ['19M', 15400, 'am'],
  ['16M', 17700, 'am'], ['13M', 21600, 'am'], ['11M', 26000, 'am'], ['HAM 40M', 7150, 'lsb'], ['HAM 20M', 14200, 'usb'], ['TIME', 10000, 'am']];
const RADIO_TAGS = ['jazz', 'news', 'ambient', 'classical', 'electronic', 'talk', 'world', 'experimental', 'lofi', 'folk', 'soul', 'rock'];
const radioIdle = () => ({ use: 0, info: new Array(RADIO_RX * RADIO_RXI).fill(0), src: [null, null, null, null], st: Array.from({ length: RADIO_RX }, () => ({ state: 0, msg: '', title: '', rate: 0, kbps: 0 })) });
const radioSrcOk = s => s && (s.kind === 'web' ? !!s.url : s.kind === 'sw' ? !!s.host : s.kind === 'file' ? !!s.name : false) ? s : null;
const radioSrc = k => radioSrcOk(ui.radios && ui.radios.src[k]);
const radioSt = k => (ui.radios && ui.radios.st[k]) || { state: 0, msg: '', title: '' };
// what the receiver does, as shown: LIVE while the engine still fills up reads BUFFERING
function radioShownState(k) {
  const st = radioSt(k).state | 0, i = ui.radios ? ui.radios.info : null;
  return st === RS.LIVE && i && i[k * RADIO_RXI + 7] > 0.5 ? RS.BUFFER : st;
}
const fKhzL = khz => (Math.round(khz * 100) / 100).toFixed(khz < 10000 ? 2 : 1) + ' kHz';
function radioLabel(s, short) {
  if (!s) return '';
  if (s.kind === 'file') return s.name;
  if (s.kind === 'web') return s.name || s.url.replace(/^https?:\/\//, '').slice(0, 40);
  const nm = (s.rxname || s.host).replace(/\s*\|.*$/, '').trim();
  return (short ? '' : (nm.length > 26 ? nm.slice(0, 25) + '…' : nm) + ' · ') + fKhzL(+s.khz || 0) + ' ' + String(s.mode || 'am').toUpperCase();
}
// S-units: S9 at -73 dBm, 6 dB each, over S9 in dB
const sUnits = dbm => { if (!(dbm > -200)) return ''; const s = 9 + (dbm + 73) / 6; return s <= 9 ? 'S' + Math.max(0, Math.round(s)) : 'S9+' + Math.round(dbm + 73); };
// a source from the page: tuned now on receiver k (the plugin or the browser connects it), kept with the project
function radioTune(k, src) {
  src = radioSrcOk(src);
  if (!ui.radios) ui.radios = radioIdle();
  ui.radios.src[k] = src ? Object.assign({ on: true }, src) : null;
  ui.radios.st[k] = Object.assign({}, ui.radios.st[k], { state: src ? (src.on === false ? RS.OFF : RS.CONNECT) : RS.NONE, msg: '', title: '' });
  if (backend.radioSet) backend.radioSet(k, ui.radios.src[k]);
  if (backend.kind !== 'plugin') { state.radio = ui.radios.src.map(x => x ? Object.assign({}, x) : null); backend.save(); }
  renderRadioPanel(); renderScreen();
}
// what the plugin (or the browser's receivers) says: sources, states, levels; the answers to the page's questions
function radioUpdate(r) {
  if (!ui.radios) ui.radios = radioIdle();
  if (Array.isArray(r.info)) ui.radios.info = r.info;
  if (typeof r.use === 'number') ui.radios.use = r.use;
  if (Array.isArray(r.src)) ui.radios.src = r.src.map(radioSrcOk);
  if (Array.isArray(r.st)) ui.radios.st = r.st;
  {
    const key = JSON.stringify([ui.radios.src, ui.radios.st, [0, 1, 2, 3].map(radioShownState)]);
    if (key !== ui.radKey) { ui.radKey = key; renderRadioPanel(); if (radioShown()) renderScreen(); }
    else if (ui.rpanel) radioPanelMeters();
  }
}
function radioAnswer(a) {
  if (!a || !ui.rq) return;
  if (a.q === 'search' && a.id === ui.rq.web.id) ui.rq.web = { id: a.id, busy: false, list: a.ok ? a.res || [] : null, err: a.ok ? '' : a.err || 'no answer' };
  else if (a.q === 'receivers' && a.id === ui.rq.sw.id) ui.rq.sw = { id: a.id, busy: false, list: a.ok ? a.res || [] : null, err: a.ok ? '' : a.err || 'no answer' };
  else return;
  renderRadioPanel();
}
let radioQid = 0;
function radioAsk(kind, q) {
  ui.rq = ui.rq || { web: {}, sw: {} };
  const id = ++radioQid;
  ui.rq[kind] = Object.assign({}, ui.rq[kind], { id, busy: true, err: '' });
  if (kind === 'web') { ui.rq.webQ = q; if (backend.radioSearch) backend.radioSearch(id, q); }
  else if (backend.radioReceivers) backend.radioReceivers(id, !!q);
  renderRadioPanel();
}
// the engine sends the RADIO track shown (its receiver, the loop, the history's peaks)
const radioShown = () => !ui.all && curMachine() === RADIO_M;
function syncRadio() {
  const want = radioShown() ? ui.t : -1;
  if (want !== ui.radT) { ui.radT = want; send(['radt', want]); }
}

// the TUNE panel: the four receivers (tabs), the one chosen (what it plays, ON / OFF, CLEAR, played by the track), then
// INTERNET (search the stations: a name, a country, a genre, at random) or SHORTWAVE (the receivers on the public
// list, the free ones first, or an address; the frequency, its mode, the bands)
function renderRadioPanel() {
  const panel = $('rpanel'), btn = $('mtune');
  const shown = radioShown();
  if (btn) { btn.hidden = !shown; btn.setAttribute('aria-expanded', ui.rpanel ? 'true' : 'false'); btn.classList.toggle('on', !!ui.rpanel); }
  if (!panel) return;
  if (!shown) ui.rpanel = false;
  panel.hidden = !ui.rpanel;
  if (!ui.rpanel) return;
  ui.rq = ui.rq || { web: {}, sw: {} };
  const k = ui.rsel | 0, src = radioSrc(k), st = radioSt(k), sst = radioShownState(k), trkRx = idx(trk().p[P.A], RADIO_RX);
  let kind = ui.rkind || (src && src.kind) || 'web';
  if (kind === 'file' && backend.kind === 'plugin') kind = 'web';
  $('rxtabs').innerHTML = [0, 1, 2, 3].map(j => {
    const s = radioSrc(j), ss = radioShownState(j), cur = j === trkRx;
    return `<button class="rxtab${j === k ? ' sel' : ''}" data-k="${j}" role="tab" aria-selected="${j === k}"><b>RX${j + 1}${cur ? ' · T' + (ui.t + 1) : ''}</b>`
      + `<i class="rdot s${ss}" aria-hidden="true"></i><span>${esc(s ? radioLabel(s, s.kind === 'sw') : 'not tuned')}</span></button>`;
  }).join('');
  // what it plays
  const say = [RS_NAMES[sst], st.msg, src && src.kind === 'web' && st.kbps ? st.kbps + ' kbit/s' : '', src && src.kind === 'sw' && st.rate ? (st.rate / 1000).toFixed(1) + ' kHz audio' : ''].filter(Boolean).join(' · ');
  $('rxnow').innerHTML = !src ? `<div class="rnm dim">RX${k + 1}: nothing tuned</div><div class="rmeta">Pick a station or a receiver below</div>`
    : `<div class="rnm">${esc(radioLabel(src))}</div><div class="rmeta${sst === RS.FAIL || sst === RS.BUSY ? ' bad' : ''}">${esc(say)}</div>`
      + (st.title ? `<div class="rtitle">♪ ${esc(st.title)}</div>` : '')
      + `<div class="racts"><button class="lsm" data-a="pow">${src.on === false ? 'Turn on' : 'Turn off'}</button>`
      + (sst === RS.FAIL || sst === RS.BUSY ? '<button class="lsm" data-a="again">Try again</button>' : '')
      + `<button class="lsm" data-a="clr">Clear</button>`
      + (trkRx !== k ? `<button class="lsm" data-a="use">Play on T${ui.t + 1}</button>` : '') + '</div>'
      + `<span class="rmeter" aria-hidden="true"><i id="rlvl"></i></span>`;
  $('rkind').querySelectorAll('button').forEach(b => { b.classList.toggle('on', b.dataset.k === kind); b.setAttribute('aria-selected', b.dataset.k === kind ? 'true' : 'false'); });
  $('rweb').hidden = kind !== 'web'; $('rsw').hidden = kind !== 'sw';
  $('rfile').hidden = kind !== 'file';
  $('rkind').querySelector('[data-k="file"]').hidden = backend.kind === 'plugin';
  if (kind === 'file') $('rfmsg').textContent = ui.rfmsg || (src && src.kind === 'file' ? 'On air: ' + src.name + (src.secs ? ' · ' + fSec(src.secs) + ', looped' : '') : 'No recording on RX' + (k + 1) + ' yet.');
  if (kind === 'web') {
    const q = ui.rq.webQ || {};
    $('rtags').innerHTML = RADIO_TAGS.map(t => `<button class="ptchip${q.tag === t ? ' on' : ''}" data-tag="${t}">${t.toUpperCase()}</button>`).join('')
      + `<button class="ptchip${q.order === 'random' ? ' on' : ''}" data-tag="*">RANDOM</button>`;
    const r = ui.rq.web;
    $('rlist').innerHTML = r.busy ? '<p class="lhint">Searching…</p>' : r.err ? `<p class="lhint bad">${esc('The directory did not answer: ' + r.err)}</p>`
      : !r.list ? '<p class="lhint">Search by name, country (two letters) or genre. MP3 stations only.</p>'
      : !r.list.length ? '<p class="lhint">No MP3 station found.</p>'
      : r.list.map((s, i) => `<div class="lrow rrow${src && src.kind === 'web' && src.url === s.url ? ' sel' : ''}" data-i="${i}" role="button" tabindex="0">`
        + `<span class="lnum">${esc(s.cc || '')}</span><span class="lname">${esc(s.name)}</span>`
        + `<span class="lmeta">${esc([s.kbps ? s.kbps + ' kbit/s' : '', (s.tags || '').split(',').slice(0, 3).join(' '), s.country].filter(Boolean).join(' · '))}</span></div>`).join('');
  } else {
    const ks = src && src.kind === 'sw' ? src : null;
    if (!$('rkhz').matches(':focus')) $('rkhz').value = (ks ? +ks.khz : ui.rkhz || 9600).toFixed(2);
    const mode = ks ? ks.mode : ui.rmode || 'am';
    $('rmodes').querySelectorAll('button').forEach(b => b.classList.toggle('on', b.dataset.m === mode));
    const khz = ks ? +ks.khz : ui.rkhz || 9600;
    $('rbands').innerHTML = SW_BANDS.map(([n, f, m]) => `<button class="ptchip${Math.abs(khz - f) < 1 ? ' on' : ''}" data-f="${f}" data-m="${m}">${n}</button>`).join('');
    const r = ui.rq.sw, filt = (ui.rfilt || '').toLowerCase();
    if (!r.list && !r.busy && !r.err && !r.asked) { r.asked = true; setTimeout(() => radioAsk('sw', false)); }
    const list = (r.list || []).filter(x => !filt || (x.name + ' ' + x.loc + ' ' + x.antenna).toLowerCase().includes(filt));
    $('rrlist').innerHTML = r.busy ? '<p class="lhint">Fetching the receivers…</p>' : r.err ? `<p class="lhint bad">${esc('The receivers\' list did not come: ' + r.err)}. Type a receiver's address above.</p>`
      : !list.length ? '<p class="lhint">No receiver.</p>'
      : list.slice(0, 120).map((x, i) => { const free = (x.max | 0) - (x.users | 0);
        return `<div class="lrow rrow${ks && ks.host === x.host && +ks.port === +x.port ? ' sel' : ''}${free <= 0 ? ' full' : ''}" data-h="${esc(x.host)}" data-p="${x.port}" data-n="${esc(x.name)}" role="button" tabindex="0">`
          + `<span class="lnum">${free > 0 ? free + ' FREE' : 'FULL'}</span><span class="lname">${esc(x.name)}</span>`
          + `<span class="lmeta">${esc([x.loc, x.antenna, x.users + '/' + x.max + ' listening'].filter(Boolean).join(' · '))}</span></div>`; }).join('');
  }
  radioPanelMeters();
}
// (between two renders: the level of the receiver chosen)
function radioPanelMeters() {
  const el = $('rlvl'); if (!el || !ui.radios) return;
  const i = ui.radios.info, k = ui.rsel | 0;
  el.style.width = Math.round(Math.min(1, Math.sqrt(i[k * RADIO_RXI + 2] || 0)) * 100) + '%';
}
function radioSetKhz(khz, mode) {
  const k = ui.rsel | 0, src = radioSrc(k);
  khz = Math.max(10, Math.min(32000, Math.round(khz * 100) / 100));
  ui.rkhz = khz; if (mode) ui.rmode = mode;
  if (src && src.kind === 'sw') radioTune(k, Object.assign({}, src, { khz, mode: mode || src.mode }));
  else renderRadioPanel();
}
function buildRadio() {
  const panel = $('rpanel'); if (!panel) return;
  ui.rsel = 0; ui.rkhz = 9600; ui.rmode = 'am';
  $('mtune').addEventListener('click', () => {
    ui.rpanel = !ui.rpanel;
    if (ui.rpanel) { ui.rsel = idx(trk().p[P.A], RADIO_RX); ui.rkind = (radioSrc(ui.rsel) || {}).kind || ui.rkind; ui.ppanel = false; ui.lpanel = false; renderPresets(); renderLoops(); }
    renderRadioPanel();
  });
  $('rclose').addEventListener('click', () => { ui.rpanel = false; renderRadioPanel(); });
  $('rxtabs').addEventListener('click', e => { const b = e.target.closest('.rxtab'); if (!b) return; ui.rsel = +b.dataset.k; ui.rkind = (radioSrc(ui.rsel) || {}).kind || ui.rkind; renderRadioPanel(); });
  $('rkind').addEventListener('click', e => { const b = e.target.closest('button'); if (!b) return; ui.rkind = b.dataset.k; renderRadioPanel(); });
  $('rxnow').addEventListener('click', e => {
    const b = e.target.closest('.lsm'); if (!b) return;
    const k = ui.rsel | 0, src = radioSrc(k);
    if (b.dataset.a === 'clr') radioTune(k, null);
    else if (b.dataset.a === 'pow' && src) radioTune(k, Object.assign({}, src, { on: src.on === false }));
    else if (b.dataset.a === 'again' && src) { radioTune(k, null); radioTune(k, src); }
    else if (b.dataset.a === 'use') { setTrackParam(P.A, mid(k, RADIO_RX)); renderAll(); }
  });
  // INTERNET: a search (Enter, the button), a genre, at random; a station tuned
  const search = extra => { const q = Object.assign({ name: $('rq').value.trim(), cc: $('rcc').value.trim().toUpperCase(), order: 'clickcount' }, extra || {}); radioAsk('web', q); };
  $('rgo').addEventListener('click', () => search());
  $('rq').addEventListener('keydown', e => { if (e.key === 'Enter') { e.preventDefault(); search(); } e.stopPropagation(); });
  $('rcc').addEventListener('keydown', e => { if (e.key === 'Enter') { e.preventDefault(); search(); } e.stopPropagation(); });
  $('rtags').addEventListener('click', e => { const b = e.target.closest('.ptchip'); if (!b) return;
    if (b.dataset.tag === '*') search({ order: 'random', name: '' }); else search({ tag: b.dataset.tag, name: '' }); });
  const pickWeb = row => { const s = ui.rq.web.list && ui.rq.web.list[+row.dataset.i]; if (!s) return;
    radioTune(ui.rsel | 0, { kind: 'web', name: s.name, url: s.url, uuid: s.uuid, country: s.country || s.cc || '', codec: 'MP3', kbps: s.kbps | 0 });
    backend.ensureAudio(); if (radioShown()) send(['aud', ui.t, 1]); };
  $('rlist').addEventListener('click', e => { const row = e.target.closest('.rrow'); if (row) pickWeb(row); });
  $('rlist').addEventListener('keydown', e => { const row = e.target.closest('.rrow'); if (row && (e.key === 'Enter' || e.key === ' ')) { e.preventDefault(); pickWeb(row); } });
  // SHORTWAVE: the frequency (typed: Enter or leaving the field; nudged), its mode, a band; a receiver tuned
  const khzIn = $('rkhz');
  khzIn.addEventListener('keydown', e => { e.stopPropagation(); if (e.key === 'Enter') { e.preventDefault(); khzIn.blur(); } });
  khzIn.addEventListener('change', () => { const v = parseFloat(khzIn.value.replace(',', '.')); if (v > 0) radioSetKhz(v); });
  const nudge = e => { const b = e.target.closest('button'); if (!b) return; const src = radioSrc(ui.rsel | 0);
    radioSetKhz((src && src.kind === 'sw' ? +src.khz : ui.rkhz || 9600) + parseFloat(b.dataset.d)); };
  $('rnudge').addEventListener('click', nudge); $('rnudge2').addEventListener('click', nudge);
  $('rmodes').addEventListener('click', e => { const b = e.target.closest('button'); if (!b) return; const src = radioSrc(ui.rsel | 0);
    radioSetKhz(src && src.kind === 'sw' ? +src.khz : ui.rkhz || 9600, b.dataset.m); });
  $('rbands').addEventListener('click', e => { const b = e.target.closest('.ptchip'); if (!b) return; radioSetKhz(+b.dataset.f, b.dataset.m); });
  $('rfilt').addEventListener('input', () => { ui.rfilt = $('rfilt').value; renderRadioPanel(); });
  $('rfilt').addEventListener('keydown', e => e.stopPropagation());
  $('rrefresh').addEventListener('click', () => radioAsk('sw', true));
  const pickSw = (host, port, name) => {
    const src = radioSrc(ui.rsel | 0), ks = src && src.kind === 'sw' ? src : null;
    radioTune(ui.rsel | 0, { kind: 'sw', host, port: port | 0 || 8073, rxname: name || host, khz: ks ? +ks.khz : ui.rkhz || 9600, mode: ks ? ks.mode : ui.rmode || 'am' });
    backend.ensureAudio(); if (radioShown()) send(['aud', ui.t, 1]);
  };
  $('rrlist').addEventListener('click', e => { const row = e.target.closest('.rrow'); if (row) pickSw(row.dataset.h, +row.dataset.p, row.dataset.n); });
  $('rrlist').addEventListener('keydown', e => { const row = e.target.closest('.rrow'); if (row && (e.key === 'Enter' || e.key === ' ')) { e.preventDefault(); pickSw(row.dataset.h, +row.dataset.p, row.dataset.n); } });
  // a recording as a receiver (the page: where the network is closed to it)
  $('rfbtn').addEventListener('click', () => { const f = $('rfin'); f.value = ''; f.click(); });
  $('rfin').addEventListener('change', async () => {
    const f = $('rfin').files && $('rfin').files[0]; if (!f || !backend.radioFile) return;
    const k = ui.rsel | 0;
    ui.rfmsg = 'Reading “' + f.name + '”…'; renderRadioPanel();
    try {
      const secs = await backend.radioFile(k, f);
      ui.rfmsg = '';
      radioTune(k, { kind: 'file', name: f.name.replace(/\.[^.]+$/, '').slice(0, 60), secs });
      backend.ensureAudio(); if (radioShown()) send(['aud', ui.t, 1]);
    } catch (e) { ui.rfmsg = 'Could not read it: ' + (e && e.message || e); renderRadioPanel(); }
  });
  const addr = () => { const v = $('raddr').value.trim().replace(/^(https?|wss?):\/\//i, '').replace(/\/.*$/, ''); if (!v) return;
    const m = v.match(/^\[?([^\]]+?)\]?(?::(\d+))?$/); if (m) pickSw(m[1], m[2] ? +m[2] : 8073, m[1]); };
  $('raddrgo').addEventListener('click', addr);
  $('raddr').addEventListener('keydown', e => { e.stopPropagation(); if (e.key === 'Enter') { e.preventDefault(); addr(); } });
}

// RADIO's screen: the receiver and what it plays, its state on the right; under them the last 2.7 s of its sound (the
// newest on the right, LIVE's playhead there), what a trig takes on LOOP and DIG shaded with where the loop plays, the
// level; along the bottom, the station's title (WEB) or the frequency, its mode and the S-meter (SW)
function drawRadio(c, col, w, h, now) {
  const sv = q => shownValue(q).v, ri = ui.radio;
  const k = idx(sv(P.A), RADIO_RX), mode = idx(sv(P.B), 3), src = radioSrc(k), sst = radioShownState(k), stx = radioSt(k);
  const mine = ri && Math.round(ri[0]) === k;
  const top = 14, H = h - top;
  const right = lcdTag(c, col, RS_NAMES[sst], w, 0, sst === RS.LIVE ? 'box' : 'inv', true, sst === RS.FAIL || sst === RS.BUSY ? col.red : null);
  const mw = lcdTag(c, col, RADIO_MODES[mode], w - right - 4, 0, 'box', true);
  c.font = col.font;
  let lab = 'RX' + (k + 1) + ' ' + (src ? radioLabel(src) : '');
  const room = w - right - mw - 14;
  while (lab.length > 4 && c.measureText(lcdText(lab)).width + 8 > room) lab = lab.slice(0, -2) + '…';
  lcdTag(c, col, lab, 0, 0, 'inv');
  lcdFrame(c, col, 0, top, w, H, [], null);
  if (!src) {
    c.font = col.font; c.fillStyle = col.dim; c.textAlign = 'center'; c.textBaseline = 'middle';
    c.fillText(lcdText('Nothing tuned · TUNE: a station, or a shortwave receiver'), w / 2, top + H / 2);
    c.textAlign = 'left';
    return;
  }
  lcdClip(c, 0, top, w, H);
  const foot = 11, ah = H - foot - 2, mid0 = top + 1 + ah / 2, x0 = 1, aw = w - 3;
  const span = RADIO_NPEAK * 512 / 24000;
  // LOOP, DIG: the loop's length against the history (LOOP takes the newest of it)
  if (mine && mode > 0 && ri[8] > 0) {
    const L = Math.min(span, ri[8]), lw = Math.max(2, Math.round(aw * L / span));
    c.fillStyle = col.lo; c.fillRect(Math.round(x0 + aw - lw), top + 1, lw, ah);
    if (ri[9] >= 0) { c.fillStyle = col.red; c.fillRect(Math.round(x0 + aw - lw + ri[9] * lw), top + 1, 1, ah); }
  }
  if (mine) {
    c.fillStyle = col.ink;
    const half = ah / 2 - 1, bw = aw / RADIO_NPEAK;
    for (let i = 0; i < RADIO_NPEAK; i++) {
      const a = ri[RADIO_HEAD + i] || 0;
      if (a < 0.01) continue;
      const yh = Math.max(0.5, a * half);
      c.fillRect(Math.round(x0 + i * bw), Math.round(mid0 - yh), Math.max(1, Math.round(bw) - (bw > 2 ? 1 : 0)), Math.max(1, Math.round(2 * yh)));
    }
    if (mode === 0) { c.fillStyle = col.red; c.fillRect(x0 + aw - 1, top + 1, 1, ah); }
    // the squelch shut: the band greyed
    if (mode === 0 && ri[12] < 0.5 && sv(P.D) > 0.005) { c.fillStyle = col.bg; c.globalAlpha = 0.55; c.fillRect(x0, top + 1, aw, ah); c.globalAlpha = 1; }
  }
  // the bottom line
  const y = top + H - foot - 1;
  c.fillStyle = col.dim; c.fillRect(1, y, w - 2, 1);
  const info = ui.radios ? ui.radios.info : null, dbm = info ? info[k * RADIO_RXI + 6] : -999;
  const txt = src.kind === 'file' ? 'RECORDING' + (src.secs ? ' · ' + fSec(src.secs) + ' LOOPED' : '') + (stx.msg ? ' · ' + stx.msg : '')
    : src.kind === 'web' ? (stx.title ? '♪ ' + stx.title : stx.msg || (stx.kbps ? stx.kbps + ' KBIT/S' : (src.country || '')))
    : fKhzL(+src.khz || 0) + ' ' + String(src.mode || 'am').toUpperCase() + (dbm > -200 ? ' · ' + sUnits(dbm) + ' ' + Math.round(dbm) + 'DBM' : '') + (stx.msg ? ' · ' + stx.msg : '');
  c.font = col.font; c.textBaseline = 'top'; c.fillStyle = col.ink;
  let t = lcdText(txt);
  const lvW = 34;
  while (t.length > 4 && c.measureText(t).width > w - lvW - 12) t = t.slice(0, -2) + '…';
  c.fillText(t, 4, y + 2);
  // the level
  const lv = info ? Math.min(1, Math.sqrt(info[k * RADIO_RXI + 2] || 0)) : 0;
  c.strokeStyle = col.dim; c.lineWidth = 1; c.strokeRect(w - lvW - 3.5, y + 2.5, lvW, 6);
  c.fillStyle = col.ink; c.fillRect(w - lvW - 2, y + 4, Math.round((lvW - 3) * lv), 3);
  c.restore();
}

// the browser's receivers (makeWebBackend): an internet station fetched as a stream (when its server lets the page read
// it), a shortwave receiver's WebSocket (from a plain-HTTP page only: a secure one may not open it); the engine decodes
// both (sk_radio_mp3, sk_radio_adpcm). Polite as the plugin: one connection a receiver, let go 45 s after no track plays
// it, a refusal or a failure not asked again until the page tunes it again
function makeRadioWeb(postRadio, onChange) {
  const rx = Array.from({ length: RADIO_RX }, (_, k) => ({ k, src: null, ver: 0, conn: null, failVer: -1, usedAt: 0, st: { state: 0, msg: '', title: '', rate: 0, kbps: 0 } }));
  let use = 0;
  const kindOf = s => s && s.kind === 'sw' ? 1 : 0;
  const set = (r, state, msg, more) => { r.st = Object.assign({}, r.st, { state, msg: msg || '' }, more || {}); postRadio({ rx: r.k, state, kind: kindOf(r.src) }); onChange(); };
  const stop = r => { if (r.conn) { const c = r.conn; r.conn = null; try { c.close(); } catch (e) {} } };
  const want = r => r.src && r.src.on !== false && r.failVer !== r.ver && ((use >> r.k & 1) || Date.now() - r.usedAt < 45000);
  const sameRx = (a, b) => a && b && a.kind === 'sw' && b.kind === 'sw' && a.host === b.host && +a.port === +b.port;
  const modCmd = s => { const pb = { am: [-4900, 4900], amn: [-2500, 2500], usb: [300, 2700], lsb: [-2700, -300], cw: [300, 700] }[s.mode] || [-4900, 4900];
    return 'SET mod=' + (s.mode || 'am') + ' low_cut=' + pb[0] + ' high_cut=' + pb[1] + ' freq=' + Math.max(0, +s.khz).toFixed(3); };
  function fail(r, v, state, msg) { if (r.ver !== v) return; r.failVer = v; stop(r); set(r, state, msg); }
  function startWeb(r) {
    const v = r.ver, src = r.src, ctl = new AbortController();
    r.conn = { close: () => ctl.abort() };
    postRadio({ rx: r.k, flush: 1 });
    set(r, RS.CONNECT, 'connecting', { title: '', kbps: src.kbps | 0 });
    if (location.protocol === 'https:' && /^http:/i.test(src.url)) { fail(r, v, RS.FAIL, 'a secure page cannot play a plain-HTTP station (the plugin can; here, a RECORDING can)'); return; }
    fetch(src.url, { signal: ctl.signal, cache: 'no-store' }).then(async res => {
      if (!res.ok) throw new Error('the station answered ' + res.status);
      const ct = (res.headers.get('content-type') || '').toLowerCase();
      if (/aac|ogg|opus|flac/.test(ct)) { fail(r, v, RS.FAIL, 'only MP3 streams play here'); return; }
      const rd = res.body.getReader();
      set(r, RS.BUFFER);
      let live = false;
      for (;;) {
        const { done, value } = await rd.read();
        if (done || r.ver !== v || !r.conn) break;
        postRadio({ rx: r.k, fmt: 'mp3', data: value });
        if (!live) { live = true; set(r, RS.LIVE); }
      }
      if (r.ver === v && r.conn) fail(r, v, RS.FAIL, 'the stream ended');
    }).catch(e => {
      if (r.ver !== v || ctl.signal.aborted) return;
      const m = String(e && e.message || e);
      fail(r, v, RS.FAIL, /fetch|network|cors/i.test(m) ? 'this page cannot reach the station (the plugin can; here, a RECORDING can)' : m);
    });
  }
  // a recording (decoded here, kept in this browser): handed over in time, about a second ahead, round and round
  const files = new Array(RADIO_RX).fill(null);
  function startFile(r) {
    const v = r.ver, pcm = files[r.k];
    postRadio({ rx: r.k, flush: 1 });
    if (!pcm || !pcm.length) { fail(r, v, RS.FAIL, 'load the recording again (this browser no longer has it)'); return; }
    set(r, RS.BUFFER, '', { title: '', rate: 24000 });
    const t0 = performance.now(); let sent = 0, at = 0;
    const tick = () => {
      if (r.ver !== v || !r.conn) return;
      const due = Math.round((performance.now() - t0) / 1000 * 24000) + 24000;
      while (sent < due) {
        const n = Math.min(4800, due - sent, pcm.length - at), chunk = pcm.slice(at, at + n);
        postRadio({ rx: r.k, fmt: 'pcm', rate: 24000, data: new Uint8Array(chunk.buffer) });
        sent += n; at += n; if (at >= pcm.length) at = 0;
      }
      if (r.st.state !== RS.LIVE) set(r, RS.LIVE);
    };
    const iv = setInterval(tick, 50);
    r.conn = { close: () => clearInterval(iv) };
    tick();
  }
  function startSw(r) {
    const v = r.ver, src = r.src;
    if (rx.some(o => o !== r && o.conn && sameRx(o.src, src))) { fail(r, v, RS.BUSY, 'another receiver here already listens to it'); return; }
    if (location.protocol === 'https:') { fail(r, v, RS.FAIL, 'a secure page cannot reach a receiver (the plugin can; here, a RECORDING can)'); return; }
    postRadio({ rx: r.k, flush: 1 });
    set(r, RS.CONNECT, 'connecting to ' + (src.rxname || src.host), { title: '' });
    let ws;
    try { ws = new WebSocket('ws://' + (src.host.includes(':') ? '[' + src.host + ']' : src.host) + ':' + (src.port || 8073) + '/' + (Date.now() / 1000 | 0) + '/SND'); }
    catch (e) { fail(r, v, RS.FAIL, 'could not reach the receiver'); return; }
    ws.binaryType = 'arraybuffer';
    let rate = 0, live = false, liveAt = 0, closing = false, cur = Object.assign({}, src), lastMod = 0, pend = null;
    const keep = setInterval(() => {
      if (ws.readyState !== 1) return;
      ws.send('SET keepalive');
      if (pend && Date.now() - lastMod >= 200) { ws.send(pend); pend = null; lastMod = Date.now(); postRadio({ rx: r.k, flush: 1 }); live = false; set(r, RS.TUNING); }
    }, 200);
    r.conn = { close: () => { closing = true; clearInterval(keep); try { ws.close(1000); } catch (e) {} },
               retune: s => { cur = Object.assign({}, s); pend = modCmd(s); } };
    ws.onopen = () => ws.send('SET auth t=kiwi p=');
    ws.onmessage = e => {
      const b = new Uint8Array(e.data instanceof ArrayBuffer ? e.data : new TextEncoder().encode(String(e.data)).buffer);
      if (b.length < 3) return;
      const tag = String.fromCharCode(b[0], b[1], b[2]);
      if (tag === 'MSG') {
        for (const kv of new TextDecoder().decode(b.subarray(4)).split(' ')) {
          const [key, val = ''] = kv.split('=');
          if (key === 'too_busy') { fail(r, v, RS.BUSY, 'all its channels are taken (' + val + ')'); return; }
          if (key === 'badp' && val !== '0') { fail(r, v, RS.BUSY, val === '1' ? 'no channel free without a password' : 'it refused the connection'); return; }
          if (key === 'down') { fail(r, v, RS.FAIL, 'the receiver is down'); return; }
          if (key === 'audio_rate') ws.send('SET AR OK in=' + (+val | 0) + ' out=48000');
          if (key === 'sample_rate') {
            rate = +val;
            for (const m of ['SET squelch=0 max=0', 'SET genattn=0', 'SET gen=0 mix=-1', 'SET ident_user=IMPAKT', modCmd(cur),
              'SET agc=1 hang=0 thresh=-100 slope=6 decay=1000 manGain=50', 'SET compression=1', 'SET keepalive']) ws.send(m);
            lastMod = Date.now();
            r.st = Object.assign({}, r.st, { rate: Math.round(rate) }); onChange();
          }
        }
      } else if (tag === 'SND' && rate > 0 && b.length > 10) {
        const flags = b[3], smeter = b[8] << 8 | b[9], data = b.slice(10);
        if (flags & 0x10) postRadio({ rx: r.k, fmt: 'adpcm', rate, data, meter: 0.1 * smeter - 127 });
        else { const pcm = new Int16Array(data.length >> 1); for (let i = 0; i < pcm.length; i++) pcm[i] = (data[2 * i] << 8 | data[2 * i + 1]) << 16 >> 16;
          postRadio({ rx: r.k, fmt: 'pcm', rate, data: new Uint8Array(pcm.buffer), meter: 0.1 * smeter - 127 }); }
        if (!live && !pend) { live = true; if (!liveAt) liveAt = Date.now(); set(r, RS.LIVE); }
      }
    };
    ws.onclose = () => {
      clearInterval(keep);
      if (closing || r.ver !== v) return;
      fail(r, v, RS.FAIL, liveAt && Date.now() - liveAt > 20000 ? 'the receiver ended the connection' : 'the connection to the receiver broke');
    };
    ws.onerror = () => {};
  }
  function tick() {
    for (const r of rx) {
      if (use >> r.k & 1) r.usedAt = Date.now();
      const w = want(r);
      if (w && !r.conn) (r.src.kind === 'web' ? startWeb : r.src.kind === 'file' ? startFile : startSw)(r);
      else if (!w && r.conn) { stop(r); set(r, !r.src ? RS.NONE : r.src.on === false ? RS.OFF : RS.STANDBY, !r.src || r.src.on === false ? '' : 'waiting for a track to play it'); }
      else if (!r.conn && r.src && r.failVer !== r.ver && r.st.state !== (r.src.on === false ? RS.OFF : RS.STANDBY)) set(r, r.src.on === false ? RS.OFF : RS.STANDBY, r.src.on === false ? '' : 'waiting for a track to play it');
    }
  }
  setInterval(tick, 500);
  return {
    set(k, src) {
      const r = rx[k]; if (!r) return;
      const was = r.src;
      r.src = radioSrcOk(src) ? Object.assign({}, src) : null;
      r.ver++; r.usedAt = Date.now();
      // a new frequency on the same receiver: tuned over its connection
      if (r.conn && r.conn.retune && sameRx(was, r.src) && r.src.on !== false) { r.conn.retune(r.src); return; }
      stop(r);
      if (!r.src) set(r, RS.NONE); else tick();
    },
    setUse(u) { use = u | 0; },
    setFile(k, pcm) { files[k] = pcm; },
    status() { return rx.map(r => r.st); },
    sources() { return rx.map(r => r.src); },
  };
}
// the directory (from the browser: when it lets the page ask), the receivers' list (the plugin only: its server does not
// let a page read it)
async function radioWebSearch(q) {
  const p = new URLSearchParams({ codec: 'MP3', hidebroken: 'true', limit: '80', order: q.order || 'clickcount' });
  if (q.order !== 'name' && q.order !== 'random') p.set('reverse', 'true');
  if (q.name) p.set('name', q.name);
  if (q.tag) p.set('tag', q.tag);
  if (q.cc && q.cc.length === 2) p.set('countrycode', q.cc);
  const res = await fetch('https://all.api.radio-browser.info/json/stations/search?' + p.toString());
  if (!res.ok) throw new Error('the directory answered ' + res.status);
  const j = await res.json();
  return (Array.isArray(j) ? j : []).filter(s => !s.hls && /mp3/i.test(s.codec || '') && /^https?:/i.test(s.url_resolved || s.url || ''))
    .map(s => ({ name: String(s.name || '').trim().slice(0, 120), url: s.url_resolved || s.url, uuid: s.stationuuid, cc: s.countrycode || '', country: s.country || '',
      tags: s.tags || '', kbps: s.bitrate | 0, votes: s.votes | 0 }));
}

// ------------------------------------------------------------------ CUBE (v74): the banks of its USER cube
// A bank is a WAV of 64 waves of 256 samples (the wavetable editors' format); the USER cube has 8 places (its Z). In
// the browser they are kept here (IndexedDB) and in cubeMem, handed to the engine when it starts; in the plugin they
// live in the project, which the host saves. A place left empty plays the factory's bank there
const CUBE_N = 8, CUBE_BANK_N = 64 * 256;
const cubeStore = {
  db: null,
  open() {
    if (this.db) return this.db;
    this.db = new Promise((res, rej) => {
      try {
        const r = indexedDB.open('impakt.cube', 1);
        r.onupgradeneeded = () => r.result.createObjectStore('bank');
        r.onsuccess = () => res(r.result); r.onerror = () => rej(r.error);
      } catch (e) { rej(e); }
    });
    return this.db;
  },
  async run(mode, f) {
    const db = await this.open();
    return new Promise((res, rej) => { const tx = db.transaction('bank', mode), q = f(tx.objectStore('bank')); tx.oncomplete = () => res(q && q.result); tx.onerror = () => rej(tx.error); });
  },
  put(b, rec) { return this.run('readwrite', st => st.put({ name: rec.name, data: rec.data }, b)).catch(() => {}); },
  del(b) { return this.run('readwrite', st => st.delete(b)).catch(() => {}); },
  async all() {
    try {
      const out = [];
      for (let b = 0; b < CUBE_N; b++) { const r = await this.run('readonly', st => st.get(b)); if (r && r.data && r.data.length) out.push({ bank: b, name: r.name || '', data: r.data }); }
      return out;
    } catch (e) { return []; }
  },
};
const cubeMem = new Array(CUBE_N).fill(null);
// a place: 2 ready (the engine plays it), 1 read but not made yet (or the audio not started), 0 empty
function cubeState(b) {
  if (ui.cubes && ui.cubes.ready) return ui.cubes.ready[b] | 0;          // (the plugin: as its engine has it)
  if (ui.cube && (Math.round(ui.cube[7]) >> b) & 1) return 2;
  return cubeMem[b] ? 1 : 0;
}
const cubeShort = n => String(n || '').toUpperCase().replace(/[^A-Z0-9]+/g, '').slice(0, 6);
// the name a Z knob shows on place i: what the track plays there
function cubeBankLabel(i, user) { return user && cubeState(i) === 2 && cubeShort(ui.cubeNames[i]) ? cubeShort(ui.cubeNames[i]) : CUBE_BANKS[i]; }

// a WAV's samples, its channels mixed (PCM 8 .. 32-bit, float 32 / 64), and the frame size its 'clm ' chunk gives (the
// editors that write one: "<!>2048")
function wavRead(buf) {
  const d = new DataView(buf), L = buf.byteLength, tag = o => String.fromCharCode(d.getUint8(o), d.getUint8(o + 1), d.getUint8(o + 2), d.getUint8(o + 3));
  if (L < 12 || tag(0) !== 'RIFF' || tag(8) !== 'WAVE') throw new Error('not a WAV file');
  let p = 12, fmt = null, data = null, frame = 0;
  while (p + 8 <= L) {
    const id = tag(p), n = d.getUint32(p + 4, true), b = p + 8;
    if (id === 'fmt ' && n >= 16) {
      fmt = { tag: d.getUint16(b, true), ch: d.getUint16(b + 2, true), bits: d.getUint16(b + 14, true) };
      if (fmt.tag === 0xFFFE && n >= 26) fmt.tag = d.getUint16(b + 24, true);     // (EXTENSIBLE: its sub-format)
    } else if (id === 'data') data = [b, Math.min(n, L - b)];
    else if (id === 'clm ') { const m = String.fromCharCode(...new Uint8Array(buf, b, Math.min(n, 16, L - b))).match(/<!>(\d+)/); if (m) frame = +m[1]; }
    p = b + n + (n & 1);
  }
  if (!fmt || !data) throw new Error('no audio in it');
  const { ch, bits } = fmt, by = bits >> 3, flt = fmt.tag === 3;
  if (!ch || !(flt ? bits === 32 || bits === 64 : bits >= 8 && bits <= 32)) throw new Error('its format is not read here');
  const n = Math.floor(data[1] / (by * ch)), x = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    let s = 0;
    for (let c = 0; c < ch; c++) {
      const o = data[0] + (i * ch + c) * by;
      s += flt ? (bits === 32 ? d.getFloat32(o, true) : d.getFloat64(o, true))
        : bits === 8 ? (d.getUint8(o) - 128) / 128 : bits === 16 ? d.getInt16(o, true) / 32768
        : bits === 24 ? ((d.getUint8(o) | (d.getUint8(o + 1) << 8) | (d.getInt8(o + 2) << 16))) / 8388608 : d.getInt32(o, true) / 2147483648;
    }
    x[i] = s / ch;
  }
  return { x, frame };
}
// a file's waves as banks of 64 x 256: 16384 samples are a bank as it is (a multiple of it, as many banks); frames of
// 256 (fewer than 64), of the size its 'clm ' chunk gives, or of 2048, 1024, 512 or 4096 (the other editors') are
// read again at 256 (their first 127 harmonics) and spread over the 64 places (between two frames, a crossfade); a
// single cycle fills the bank. Levelled to a peak of -0.5 dB, 16-bit
function cubeBanks(x, hint) {
  const n = x.length;
  if (n < 64) throw new Error('too short for a wavetable');
  const level = y => {
    let pk = 0; for (let i = 0; i < y.length; i++) pk = Math.max(pk, Math.abs(y[i]));
    if (!(pk > 1e-5)) throw new Error('silent');
    const g = 0.944 / pk, out = new Int16Array(CUBE_BANK_N);
    for (let i = 0; i < CUBE_BANK_N; i++) out[i] = Math.max(-32767, Math.min(32767, Math.round(y[i] * g * 32767)));
    return out;
  };
  if ((!hint || hint === 256) && n % CUBE_BANK_N === 0) return Array.from({ length: Math.min(n / CUBE_BANK_N, CUBE_N) }, (_, b) => level(x.subarray(b * CUBE_BANK_N, (b + 1) * CUBE_BANK_N)));
  let F = hint > 0 && hint <= n ? hint : 0;
  if (!F) F = n % 256 === 0 && n / 256 <= 64 ? 256 : [2048, 1024, 512, 4096].find(f => n % f === 0) || (n <= 4096 ? n : 0);
  if (!F) throw new Error('not a wavetable: 64 waves of 256 samples expected');
  const nf = Math.max(1, Math.floor(n / F)), frames = [];
  for (let f = 0; f < nf; f++) {
    const src = x.subarray(f * F, f * F + F);
    if (F === 256) { frames.push(Float32Array.from(src)); continue; }
    // (its harmonics, then made again at 256 samples)
    const H = Math.min(127, Math.floor(F / 2) - 1), re = new Float64Array(H + 1), im = new Float64Array(H + 1);
    for (let k = 1; k <= H; k++) { let a = 0, b = 0; const w = 2 * Math.PI * k / F; for (let i = 0; i < F; i++) { a += src[i] * Math.cos(w * i); b += src[i] * Math.sin(w * i); } re[k] = a * 2 / F; im[k] = b * 2 / F; }
    const y = new Float32Array(256);
    for (let i = 0; i < 256; i++) { let s = 0; for (let k = 1; k <= H; k++) { const w = 2 * Math.PI * k * i / 256; s += re[k] * Math.cos(w) + im[k] * Math.sin(w); } y[i] = s; }
    frames.push(y);
  }
  const bank = new Float32Array(CUBE_BANK_N);
  for (let j = 0; j < 64; j++) {
    const q = nf === 64 ? j : j * (nf - 1) / 63, i0 = Math.floor(q), i1 = Math.min(nf - 1, i0 + 1), fr = q - i0;
    for (let i = 0; i < 256; i++) bank[j * 256 + i] = frames[i0][i] + (frames[i1][i] - frames[i0][i]) * fr;
  }
  return [level(bank)];
}
// files into the places: the first one (all its banks) from place z, the next ones into the empty places after it
async function cubeLoadFiles(files, z) {
  files = [...files].filter(Boolean).sort((a, b) => a.name.localeCompare(b.name, undefined, { numeric: true }));
  if (!files.length) return;
  backend.ensureAudio();
  const used = new Set(), failed = [];
  let first = -1, done = 0;
  const place = () => {
    if (!used.size && z >= 0) return z;
    for (let k = 0; k < CUBE_N; k++) { const q = ((z < 0 ? 0 : z) + k) % CUBE_N; if (!used.has(q) && cubeState(q) === 0) return q; }
    return -1;
  };
  for (const f of files) {
    try {
      const name = f.name.replace(/\.[a-z0-9]{2,5}$/i, '').slice(0, 40);
      ui.lmsg = 'reading “' + f.name + '”…'; renderLoops();
      const w = wavRead(await f.arrayBuffer()), banks = cubeBanks(w.x, w.frame);
      for (let b = 0; b < banks.length; b++) {
        const q = place();
        if (q < 0) { failed.push(f.name + ' (the 8 places are full)'); break; }
        used.add(q);
        backend.cubeLoad(q, { name: banks.length > 1 ? name.slice(0, 36) + ' ' + (b + 1) : name, data: banks[b] });
        if (first < 0) first = q;
        done++;
      }
    } catch (e) { failed.push(f.name + (e && e.message ? ' (' + e.message + ')' : '')); }
  }
  // the track shown plays the first place loaded, on its USER cube
  if (first >= 0 && !ui.all && curMachine() === CUBE_M && !ui.lock) { setTrackParam(P.C, first / 7); setTrackParam(P.S7, mid(1, 2)); }
  brkMsg(failed.length ? (done ? done + ' loaded · ' : '') + 'not loaded: ' + failed.join(', ') : done + (done > 1 ? ' banks loaded' : ' bank loaded'));
  try { if (navigator.storage && navigator.storage.persist) navigator.storage.persist().catch(() => {}); } catch (e) {}
  cubeChanged();
}
// the banks changed (read, made, cleared): the panel, the Z knob, the screen
function cubeChanged() {
  renderLoops();
  if (!ui.all && curMachine() === CUBE_M) { renderKnobs(); renderScreen(); }
}
// a bank's outline for the panel: its first wave (solid) and its last (dashed), 64 points
function cubeWave(data) {
  if (!data) return '';
  const path = (o, dash) => { let d = ''; for (let i = 0; i < 64; i++) { const v = data[o + i * 4] / 32767; d += (i ? 'L' : 'M') + (i + 0.5) + ' ' + (8 - v * 7).toFixed(2); } return `<path d="${d}"${dash ? ' stroke-dasharray="2 2" opacity=".55"' : ''}/>`; };
  return `<svg class="lwave cw" viewBox="0 0 64 16" preserveAspectRatio="none" aria-hidden="true">${path(63 * 256, true)}${path(0, false)}</svg>`;
}
function renderCubes() {
  const zv = trk().p[P.C] * 7, cur = Math.round(zv), user = idx(trk().p[P.S7], 2) === 1;
  const n = [...Array(CUBE_N).keys()].filter(b => cubeState(b)).length;
  $('ltitle').textContent = 'WAVETABLES';
  $('lload').title = 'Load wavetables into the empty places';
  $('lhint').textContent = 'WAV banks of 64 waves of 256 samples (the wavetable editors’ format), or frames of 2048 and others, read again. Several files: the next empty places. SET on USER plays them; an empty place plays the factory’s. On a computer, drop files on a place.';
  $('lsub').textContent = ui.lmsg || (n ? n + ' of 8 · shared by the CUBE tracks · tap one to play it on T' + (ui.t + 1) + (user ? '' : ' (SET goes to USER)') : '8 places, shared by the CUBE tracks');
  $('llist').classList.add('c8');
  $('llist').innerHTML = [...Array(CUBE_N).keys()].map(b => {
    const st = cubeState(b), sel = b === cur && user;
    if (!st) return `<div class="lrow empty${sel ? ' sel' : ''}" data-s="${b}" role="button" tabindex="0" aria-label="Place ${b + 1}: empty, plays ${CUBE_BANKS[b]}; load a wavetable">`
      + `<span class="lnum">${b + 1}</span><span class="lname">Empty · ${CUBE_BANKS[b]}</span><span class="lmeta">tap to load</span></div>`;
    const nm = esc(ui.cubeNames[b] || 'BANK ' + (b + 1));
    return `<div class="lrow${sel ? ' sel' : ''}${st === 1 ? ' busy' : ''}" data-s="${b}" role="button" tabindex="0" aria-label="Place ${b + 1}: ${nm}">`
      + `<span class="lnum">${b + 1}</span>${cubeWave(cubeMem[b] && cubeMem[b].data)}<span class="lname">${nm}</span>`
      + `<span class="lmeta">${st === 2 ? '64 waves · ready' : backend.kind === 'plugin' || ui.engine ? 'making its waves…' : 'waiting for the audio'}</span>`
      + `<span class="lacts"><button class="lsm del" data-a="del" aria-label="Clear the place">×</button></span></div>`;
  }).join('');
  $('llist').querySelectorAll('.lrow').forEach(row => {
    const b = +row.dataset.s;
    const open = () => { ui.brkTarget = b; const f = $('lfile'); f.value = ''; f.click(); };
    row.onclick = e => {
      const a = e.target.closest('.lsm');
      if (a) {
        e.stopPropagation();
        if (!a.classList.contains('armed')) { a.classList.add('armed'); a.textContent = 'Clear?'; clearTimeout(a.t); a.t = setTimeout(() => { a.classList.remove('armed'); a.textContent = '×'; }, 2500); return; }
        backend.cubeClear(b); brkMsg('place ' + (b + 1) + ' cleared'); cubeChanged(); return;
      }
      if (!cubeState(b)) { open(); return; }
      if (curMachine() === CUBE_M && !ui.all) { setTrackParam(P.C, b / 7); setTrackParam(P.S7, mid(1, 2)); backend.ensureAudio(); send(['aud', ui.t, 1]); cubeChanged(); }
    };
    row.onkeydown = e => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); row.click(); } };
    row.ondragover = e => { e.preventDefault(); row.classList.add('drop'); };
    row.ondragleave = () => row.classList.remove('drop');
    row.ondrop = e => { e.preventDefault(); e.stopPropagation(); row.classList.remove('drop'); cubeLoadFiles(e.dataTransfer.files, b); };
  });
}
// the screen: the wave A reads where it is (its 128 points, from the engine), the X · Y plane (A filled, B ringed when
// it is heard or modulates A, a line between), the Z column (the 8 places: lit when the track reads a bank loaded
// there, Z in red); on top the bank under Z, FACT or USER, where A is
const cubeShown = () => !ui.all && curMachine() === CUBE_M;
function syncCube() {
  const want = cubeShown() ? ui.t : -1;
  if (want !== ui.cubeT) { ui.cubeT = want; send(['cube', want]); }
}
function drawCube(c, col, w, h) {
  const sv = q => shownValue(q).v, ci = ui.cube, user = idx(sv(P.S7), 2) === 1;
  const ax = ci ? ci[1] * 7 : sv(P.A) * 7, ay = ci ? ci[2] * 7 : sv(P.B) * 7, az = ci ? ci[3] * 7 : sv(P.C) * 7;
  const bx = ci ? ci[4] * 7 : clamp(ax + (sv(P.S3) - 0.5) * 14, 0, 7), by = ci ? ci[5] * 7 : clamp(ay + (sv(P.S4) - 0.5) * 14, 0, 7);
  const bOn = sv(P.D) > 0.002 || sv(P.S5) > 0.002;
  const zi = Math.round(az), zf = Math.abs(az - zi) < 0.05;
  const z0 = Math.floor(az), z1 = Math.min(7, z0 + 1);
  let x = lcdTag(c, col, zf ? cubeBankLabel(zi, user) : cubeBankLabel(z0, user) + '›' + cubeBankLabel(z1, user), 0, 0, 'inv') + 4;
  x += lcdTag(c, col, user ? 'USER' : 'FACT', x, 0, 'box') + 4;
  lcdTag(c, col, 'X ' + (1 + ax).toFixed(1) + '  Y ' + (1 + ay).toFixed(1) + '  Z ' + (1 + az).toFixed(1), x, 0, 'txt');
  const bSt = idx(sv(P.S1), 49) - 24, fm = sv(P.D);
  if (bOn) lcdTag(c, col, 'B ' + (bSt ? (bSt > 0 ? '+' : '') + bSt + 'st' : 'UNI') + (fm > 0.002 ? ' · FM ' + Math.round(fm * 100) + '%' : ''), w, 0, 'box', true);
  const top = 14, H = h - top, S = H, ZW = 12;
  const zx = w - ZW, gx = zx - 6 - S, W1 = gx - 6;
  // the wave
  lcdFrame(c, col, 0, top, W1, H, [0.25, 0.5, 0.75], [0.5]);
  lcdGrid(c, col, 0, top, W1, H, null, [0.5]);
  if (ci) {
    lcdClip(c, 0, top, W1, H);
    c.strokeStyle = col.ink; c.lineWidth = 1; c.beginPath();
    const amp = H / 2 - 3, y0 = top + H / 2;
    for (let i = 0; i <= 128; i++) { const v = ci[10 + (i & 127)], X = 2 + (W1 - 4) * i / 128, Y = y0 - clamp(v, -1.2, 1.2) * amp; if (i) c.lineTo(X, Y); else c.moveTo(X, Y); }
    c.stroke(); c.restore();
  } else lcdTag(c, col, 'WAITING FOR THE AUDIO', 4, top + 3, 'txt', false, col.dim);
  // the plane
  lcdFrame(c, col, gx, top, S, S);
  const px = v => gx + 3 + (S - 6) * v / 7, py = v => top + S - 3 - (S - 6) * v / 7;
  c.fillStyle = col.dim;
  for (let i = 0; i < 8; i++) for (let j = 0; j < 8; j++) c.fillRect(Math.round(px(i)), Math.round(py(j)), 1, 1);
  if (bOn) {
    c.strokeStyle = col.dim; c.lineWidth = 1; c.beginPath(); c.moveTo(px(ax), py(ay)); c.lineTo(px(bx), py(by)); c.stroke();
    c.strokeStyle = col.ink; c.beginPath(); c.arc(px(bx), py(by), 3, 0, Math.PI * 2); c.stroke();
  }
  c.fillStyle = ci && ci[0] > 0.5 ? col.red : col.ink; c.fillRect(Math.round(px(ax)) - 2, Math.round(py(ay)) - 2, 5, 5);
  // the Z column: the 8 places, the first at the bottom
  const ch = (S - 2) / 8;
  for (let b = 0; b < 8; b++) {
    const yy = Math.round(top + S - 1 - (b + 1) * ch), hh = Math.max(1, Math.round(ch) - 1);
    if (user && cubeState(b) === 2) { c.fillStyle = col.ink; c.fillRect(zx + 2, yy + 1, ZW - 4, hh); }
    else { c.strokeStyle = col.dim; c.lineWidth = 1; c.strokeRect(zx + 2.5, yy + 1.5, ZW - 5, Math.max(0, hh - 1)); }
  }
  const zy = Math.round(top + S - 1 - (az + 0.5) * ch);
  c.fillStyle = col.red; c.fillRect(zx, zy, ZW, 1);
  if (ci && ci[8] > 0.5) lcdTag(c, col, 'MAKING A BANK…', 4, h - 13, 'inv');
}

// ------------------------------------------------------------------ WEST (v76): its screen
// The folder's curve (CELL's five cells or the SINE; the span TIMB and SYMM send the wave across lit, the rest dim; its
// drive in volts, or how many times round the sine), the gates' vactrols (v78: gate 1's, gate 2's beside it when it is
// heard: their LEDs lit by their controls, their cells as they open; gate 1's mode), the five steps (their values from
// the middle, LEN's outlined, the one playing filled, a dot over it at each ping, a tick under those that fire a stage
// pulse); along the top the pulser, CNT, the modulator's destination and ratio, the spring
const westShown = () => !ui.all && curMachine() === WEST_M;
// v79: WEST's panel (by what you hear: WS_PAGES), in place of the machine's eight knobs (in CONTROL ALL: those)
const wsOn = westShown;
const wsPage = () => idx(trk().p[P.F], WS_PAGES.length);
const WS_ALLPS = [...new Set(WS_PAGES.flatMap(pg => [...pg.main, ...pg.fine]).concat([SQ(49)]))];
function buildWestPanel() {
  const host = $('wtabs');
  WS_PAGES.forEach((pg, i) => {
    const b = document.createElement('button');
    b.className = 'wtab'; b.type = 'button'; b.setAttribute('role', 'tab'); b.dataset.i = String(i);
    // (RHYTHM: a LED on each of the pulser's hits; MOTION: the stage playing; SHAPE: the envelope's level)
    b.title = pg.say;   // (v85: the plugin's band has no room for the sentence: the tab's tooltip)
    b.innerHTML = `<svg viewBox="0 0 26 16" aria-hidden="true"><path d="${pg.icon}"/></svg><span>${pg.id}</span>`
      + (pg.id === 'RHYTHM' ? `<i class="wled" style="--wc:${WS_SRCC[4]}"></i>` : pg.id === 'MOTION' ? '<b class="wnum"></b>' : pg.id === 'SHAPE' ? '<u class="wlv"></u>' : '');
    b.addEventListener('click', () => { backend.ensureAudio(); ui.lastP = P.F; setTrackParam(P.F, mid(i, WS_PAGES.length)); renderAll(); });
    host.appendChild(b);
  });
  host.addEventListener('keydown', e => {
    const d = e.key === 'ArrowRight' ? 1 : e.key === 'ArrowLeft' ? -1 : 0;
    if (!d) return;
    e.preventDefault();
    const i = (wsPage() + d + WS_PAGES.length) % WS_PAGES.length;
    ui.lastP = P.F; setTrackParam(P.F, mid(i, WS_PAGES.length)); renderAll();
    host.children[i].focus();
  });
  buildKnobs('wm', 'wmain', 6); buildKnobs('wf', 'wfine', 12);
}
function renderWestPanel() {
  const on = wsOn();
  $('wpanel').hidden = !on; $('mknobs').hidden = on;
  $('screen').classList.toggle('west', on); $('machmod').classList.toggle('west', on);
  if (!on) return;
  const pi = wsPage();
  [...$('wtabs').children].forEach((b, i) => {
    const cur = i === pi, pg = WS_PAGES[i];
    b.classList.toggle('on', cur); b.setAttribute('aria-selected', String(cur)); b.tabIndex = cur ? 0 : -1;
    b.classList.toggle('haslock', !cur && [...pg.main, ...pg.fine].some(p => trackHasLock(ui.t, p)));
  });
  $('wsay').textContent = WS_PAGES[pi].say;
  // (v85: the plugin's fine row, one column per control: up to twelve on one line)
  $('wfine').style.setProperty('--wn', String(Math.max(6, WS_PAGES[pi].fine.length)));
}
// the ring (a slider: the glow on its rail) where a CV takes a control now, in its source's colour; g null: none
function westGhost(K, g, col) {
  if (g == null) {
    if (K.gLast === -1) return;
    K.gLast = -1; K.el.classList.remove('hasg');
    if (K.gho) K.gho.setAttribute('d', ''); if (K.ghd) K.ghd.setAttribute('r', '0');
    return;
  }
  if (K.gLast != null && K.gLast >= 0 && Math.abs(g - K.gLast) < 0.004) return;
  K.gLast = g;
  const v = clamp(K.curV || 0, 0, 1), lo = Math.min(v, g), hi = Math.max(v, g);
  K.el.style.setProperty('--gc', col);
  if (K.el.classList.contains('isfd')) { K.el.classList.add('hasg'); K.el.style.setProperty('--g0', lo.toFixed(4)); K.el.style.setProperty('--g1', hi.toFixed(4)); return; }
  K.gho.setAttribute('d', hi - lo < 0.004 ? '' : arcPath(24, A0 + SWEEP * lo, A0 + SWEEP * hi));
  const [x, y] = polar(24, A0 + SWEEP * g);
  K.ghd.setAttribute('cx', x.toFixed(2)); K.ghd.setAttribute('cy', y.toFixed(2)); K.ghd.setAttribute('r', '2.6');
}
// each frame: the rings, the stage playing on its slider, the tabs' LEDs (the pulser's hits, the stage, the envelope)
function westLive(now) {
  if (!wsOn() || ui.ptab !== 'ms') return;
  const wi = ui.west, on = !!wi && wi[0] > 0.5;
  for (const K of banks.wm.concat(banks.wf)) {
    if (K.el.hidden) continue;
    if (K.live) {
      const g = on ? clamp(K.live.live(wi), 0, 1) : null;
      westGhost(K, g != null && Math.abs(g - clamp(K.curV || 0, 0, 1)) > 0.012 ? g : null, K.live.liveQ != null ? WS_SRCC[idx(shownValue(K.live.liveQ).v, 8)] : 'var(--led-r)');
    }
    if (K.stage >= 0) {
      const len = 1 + idx(shownValue(SQ(26)).v, 5);
      K.el.classList.toggle('wplay', !!wi && K.stage < len && Math.round(wi[3]) % len === K.stage);
    } else K.el.classList.remove('wplay');
  }
  const tabs = $('wtabs').children;
  if (wi && wi[5] !== ui.wsNp) { ui.wsNp = wi[5]; if (on && wi[5] > 0) ui.wsFlash = now + 90; }
  const led = tabs[4].querySelector('.wled'); if (led) led.classList.toggle('on', (ui.wsFlash || 0) > now);
  const num = tabs[5].querySelector('.wnum');
  if (num) { const len = 1 + idx(shownValue(SQ(26)).v, 5), t = on ? String(Math.round(wi[3]) % len + 1) : ''; num.classList.toggle('on', !!t); if (num.textContent !== t) num.textContent = t; }
  const lv = tabs[2].querySelector('.wlv'); if (lv) lv.style.transform = 'scaleX(' + (on ? clamp(wi[23], 0, 1) : 0).toFixed(3) + ')';
}
function syncWest() {
  const want = westShown() ? ui.t : -1;
  if (want !== ui.westT) { ui.westT = want; send(['west', want]); }
}
// the folder's circuit (core sk_west: foldf), a third of it (1: a sine just touching it)
const WS_FT = [0.6, 2.994, 5.46, 1.8, 4.08], WS_FC = [-12 * 0.8333, -27.777 * 0.3768, -21.428 * 0.2829, 17.647 * 0.5743, 36.363 * 0.2673];
const wsFold = x => { const a = Math.abs(x), sg = Math.sign(x); let y = 5 * x; for (let q = 0; q < 5; q++) if (a > WS_FT[q]) y += WS_FC[q] * (a - WS_FT[q]) * sg; return y / 3; };
// v79: the screen follows the page: PITCH (a ruler of semitones: the tune, the stages' notes, the note playing, the mod
// osc against it), COLOUR (the folder's curve and the wave out of it), SHAPE (the envelope drawn, a dot where it is, gate
// 1's vactrol), LAYER (gate 2: what it plays, its vactrol, its level), RHYTHM (the burst a trig sets off, the hit playing
// lit), MOTION (the five stages), SPACE (the spring's tail)
const WS_RATV = [0.25, 0.5, 0.75, 1, 1.25, Math.SQRT2, 1.5, 2, 1 + Math.SQRT2, 2.76, 3, 3.5, 4, 5.4, 7, 8.93];
const envCurveJs = (x, stage, c) => {
  if (c >= 0.9995 || x <= 0) return x;
  if (stage === 0) { if (c >= 0.5) return x; const r = (1 - Math.exp(-4 * x)) * 1.0186573; return x + (r - x) * (1 - 2 * c); }
  if (x >= 1) return 1;
  const t = -Math.log2(x) / 9.9657843, lin = Math.max(0, 1 - 2 * t);
  if (c >= 0.5) return lin + (x - lin) * (2 * c - 1);
  const u = Math.min(1, 2.5 * t), q = 1 - Math.pow(u, 3.32);
  return q + (lin - q) * 2 * c;
};
function drawWest(c, col, w, h, now) {
  const sv = q => shownValue(q).v, wi = ui.west, on = !!wi && wi[0] > 0.5, page = WS_PAGES[wsPage()].id;
  const top = 15, H = h - top - 1, base = h - 2;
  const tags = list => { let x = 0; for (const t of list) if (t) x += lcdTag(c, col, t[0], x, 0, t[1] || 'box') + 4; return x; };
  const line = (pts, ink, lw) => { c.strokeStyle = ink; c.lineWidth = lw; c.beginPath(); pts.forEach(([x, y], i) => (i ? c.lineTo(x, y) : c.moveTo(x, y))); c.stroke(); };
  const txt = (t, x, y, ink, align) => { c.font = col.font; c.textBaseline = 'top'; c.textAlign = align || 'left'; c.fillStyle = ink || col.ink; c.fillText(lcdText(t), x, y); c.textAlign = 'left'; };
  // gate's vactrol: an LED lit by its control, a cell lit as it opens
  const vactrol = (cx, cy, rr, ctl, light) => {
    c.strokeStyle = col.ink; c.lineWidth = 1;
    c.beginPath(); c.moveTo(cx - 13, cy - rr); c.lineTo(cx - 13, cy + rr); c.lineTo(cx - 13 + rr * 1.2, cy); c.closePath();
    if (on && ctl > 0.02) { c.globalAlpha = Math.min(1, 0.25 + ctl); c.fillStyle = col.red; c.fill(); c.globalAlpha = 1; }
    c.stroke(); c.fillStyle = col.ink; c.fillRect(Math.round(cx - 13 + rr * 1.2), cy - rr, 1, 2 * rr + 1);
    c.beginPath(); c.arc(cx + 5 + rr * 0.4, cy, rr, 0, Math.PI * 2);
    c.globalAlpha = 0.12 + 0.88 * Math.sqrt(Math.max(0, Math.min(1, light))); c.fillStyle = on ? col.red : col.lo; c.fill(); c.globalAlpha = 1; c.stroke(); };
  const sine = idx(sv(SQ(1)), 2) === 0, fold = v => (sine ? Math.sin(v) : wsFold(v) / 1.5);
  const T = sv(P.A), sy = (sv(P.B) - 0.5) * 1.2, Ax = on ? wi[6] : (sine ? 8 * (3 * T + 0.125) : 0.55 + 7.5 * T * T);
  if (page === 'PITCH') {
    const tune = (sv(P.TUNE) - 0.5) * 48, keys = idx(sv(SQ(36)), 2) === 0, free = idx(sv(SQ(9)), 2) === 1;
    tags([['TUNE ' + (Math.abs(tune) < 0.05 ? '0' : (tune > 0 ? '+' : '−') + Math.abs(tune).toFixed(1)), 'box'], [keys ? 'KEYS FOLLOW' : 'KEYS FIXED', 'txt'], [sv(SQ(2)) > 0.005 ? 'GLIDE ' + fWsGlid(sv(SQ(2))) : '', 'txt']]);
    lcdTag(c, col, free ? 'MOD FREE' : 'MOD ×' + WS_RATL[idx(sv(P.D), 16)], w, 0, 'txt', true, col.dim);
    const X = st => 4 + (clamp(st, -24, 36) + 24) / 60 * (w - 8), ry = top + Math.round(H * 0.62);
    c.fillStyle = col.lo; c.fillRect(4, ry, w - 8, 1);
    for (let st = -24; st <= 36; st++) { const oct = st % 12 === 0; c.fillStyle = oct ? col.dim : col.lo; c.fillRect(Math.round(X(st)), ry - (oct ? 6 : 2), 1, oct ? 12 : 5); }
    for (const st of [-24, -12, 0, 12, 24, 36]) txt((st > 0 ? '+' : st < 0 ? '−' : '') + Math.abs(st), X(st), ry + 8, col.dim, st === -24 ? 'left' : st === 36 ? 'right' : 'center');
    // the stages' notes (GOES TO PITCH), the mod osc on its ratio, the tune, the note playing
    const len = 1 + idx(sv(SQ(26)), 5), dest = idx(sv(SQ(27)), 4);
    if (dest === 0) for (let i = 0; i < len; i++) { const st = tune + Math.round((sv(SQ(21 + i)) - 0.5) * 24); c.fillStyle = col.dim; c.fillRect(Math.round(X(st)) - 1, ry - 12, 3, 3); }
    if (!free) { const ms = tune + 12 * Math.log2(WS_RATV[idx(sv(P.D), 16)]); c.strokeStyle = col.dim; c.lineWidth = 1; c.strokeRect(Math.round(X(ms)) - 2.5, ry - 22.5, 5, 5); txt('MOD', Math.round(X(ms)) + 6, ry - 24, col.dim); }
    const tri = (x, y, ink, fill) => { c.beginPath(); c.moveTo(x - 4, y - 7); c.lineTo(x + 4, y - 7); c.lineTo(x, y); c.closePath(); if (fill) { c.fillStyle = ink; c.fill(); } else { c.strokeStyle = ink; c.lineWidth = 1; c.stroke(); } };
    tri(Math.round(X(tune)) + 0.5, ry - 1, col.ink, true);
    txt('TUNE', Math.round(X(tune)) + 7, ry - 8, col.ink);
    if (on) { tri(Math.round(X(tune + wi[10])) + 0.5, ry - 14, col.red, true); txt('NOW', Math.round(X(tune + wi[10])) + 7, ry - 21, col.red); }
  } else if (page === 'COLOUR') {
    tags([[sine ? 'SINE ×' + Ax.toFixed(1) : Ax.toFixed(1) + 'V', on ? 'inv' : 'box'], [['FM', 'AM', 'BAL'][idx(sv(SQ(8)), 3)] + ' ' + fPct(on ? wi[7] : sv(P.C)), 'txt'], [sv(SQ(0)) > 0.02 ? 'BLEND ' + fNear(['SINE', 'WAVE'])(sv(SQ(0))) : '', 'txt']]);
    // the folder's curve (the span the wave crosses lit), the wave out of it
    const FW = Math.round(w * 0.36), R = sine ? Math.max(9, Math.ceil(Ax * (1 + Math.abs(sy)))) : 9;
    const lo = Math.max(-R, Ax * (-1 + sy)), hi = Math.min(R, Ax * (1 + sy)), mid = top + H / 2;
    const X = v => 2 + (v + R) / (2 * R) * (FW - 4), Y = v => mid - v * (H / 2 - 2);
    c.fillStyle = col.lo; c.fillRect(2, Math.round(mid), FW - 4, 1); c.fillRect(Math.round(X(0)), top, 1, H);
    const curve = (a, b, ink, lw) => line(Array.from({ length: 121 }, (_, i) => { const v = a + (b - a) * i / 120; return [X(v), Y(fold(v))]; }), ink, lw);
    curve(-R, R, col.dim, 1); if (hi > lo) curve(lo, hi, on ? col.red : col.ink, 2);
    const xf = sv(SQ(0)), other = idx(sv(SQ(35)), 3), ow = ph => other === 0 ? 1 - 4 * Math.abs(ph - 0.5) : other === 1 ? (ph < 0.5 ? 1 : -1) : (Math.abs(ph - 0.5) < 0.05 ? 1 - Math.abs(ph - 0.5) * 20 : 0);
    const WX = FW + 10, WW = w - WX - 2;
    c.fillStyle = col.lo; c.fillRect(WX, Math.round(mid), WW, 1);
    line(Array.from({ length: 241 }, (_, i) => { const t = i / 240, ph = (t * 2) % 1, x = Math.sin(2 * Math.PI * ph); return [WX + t * WW, Y((1 - xf) * fold(Ax * (x + sy)) + xf * ow((ph + 0.25) % 1))]; }), on ? col.red : col.ink, 1.5);
  } else if (page === 'SHAPE') {
    const cv = sv(SQ(46)), em = idx(sv(SQ(32)), 4);
    tags([[['ONCE', 'HOLD', 'LOOP', 'OFF'][em], 'box'], ['FIRED BY ' + ['TRIG', 'PULSER', 'STAGE'][idx(sv(SQ(45)), 3)], 'txt'], [fCurve(cv), 'txt']]);
    // the times (a square-root scale, as heard), then the curve; the dot: where the envelope is now
    const aV = sv(SQ(30)), sV = sv(SQ(31)), dV = sv(P.DEC);
    const aT = aV < 0.005 ? 0 : 0.001 * Math.pow(2000, aV), sT = sV < 0.002 ? 0 : Math.max(0.005, 2 * sV * sV), dT = 0.005 * Math.pow(1400, Math.min(dV, 0.98)), hold = dV >= 0.99;
    const EW = Math.round(w * 0.68) - 4, wA0 = Math.sqrt(aT), wS0 = Math.sqrt(sT), wD0 = Math.sqrt(dT) * 1.6, k = (EW - 6) / Math.max(0.05, wA0 + wS0 + wD0);
    const xA = 2, wA = Math.max(1, wA0 * k), xS = xA + wA, wS = wS0 * k, xD = xS + wS, wD = Math.max(8, Math.min(EW - xD, wD0 * k)), Yv = v => base - clamp(v, 0, 1) * (H - 3);
    c.fillStyle = col.lo; c.fillRect(2, base, EW, 1);
    const pts = [];
    for (let i = 0; i <= 24; i++) { const x = i / 24; pts.push([xA + x * wA, Yv(envCurveJs(x, 0, cv))]); }
    pts.push([xD, Yv(1)]);
    for (let i = 0; i <= 80; i++) { const t = i / 80; pts.push([xD + t * wD, Yv(hold ? 1 : envCurveJs(Math.pow(10, -3 * t), 2, cv))]); }
    line(pts, col.ink, 1.5);
    if (on && em !== 3) {
      const est = Math.round(wi[15]), e = wi[2];
      const x = est === 0 ? xA + clamp(e, 0, 1) * wA : est === 1 ? xS + wS / 2 : est === 2 ? xD + clamp(Math.log10(Math.max(e, 1e-4)) / -3, 0, 1) * wD : -1;
      if (x >= 0) { c.fillStyle = col.red; c.beginPath(); c.arc(x, Yv(est === 2 ? envCurveJs(clamp(e, 0, 1), 2, cv) : est === 0 ? envCurveJs(clamp(e, 0, 1), 0, cv) : 1), 3, 0, Math.PI * 2); c.fill(); }
    }
    const vx = EW + 10 + (w - EW - 10) / 2 - 6, rr = Math.max(4, Math.min(8, Math.floor((H - 14) / 2)));
    vactrol(vx, Math.round(top + (H - 10) / 2), rr, on ? wi[21] : 0, on ? wi[1] : 0);
    txt('GATE 1 ' + ['LPG', 'RESO', 'VCA', 'LPF'][idx(sv(SQ(28)), 4)], vx + 2, h - 10, col.ink, 'center');
  } else if (page === 'LAYER') {
    const out = sv(SQ(13)), inp = idx(sv(SQ(54)), 3);
    tags([['GATE 2 ' + ['LPG', 'VCA', 'LPF'][idx(sv(SQ(53)), 3)], out > 0.002 ? 'box' : 'txt'], ['INPUT ' + ['MOD OSC', 'LOOP', 'GATE 1'][inp], 'txt']]);
    const cy = Math.round(top + (H - 4) / 2), rr = Math.max(4, Math.min(8, Math.floor((H - 14) / 2)));
    // what it plays (the mod osc's wave, the loop, gate 1), its vactrol, its level
    const IW = Math.round(w * 0.3);
    if (inp === 0) { const mw = idx(sv(SQ(7)), 3); line(Array.from({ length: 61 }, (_, i) => { const t = i / 60, ph = (t * 2) % 1; const y = mw === 0 ? 1 - 2 * ph : mw === 1 ? (ph < 0.5 ? 1 : -1) : 1 - 4 * Math.abs(ph - 0.5); return [4 + t * (IW - 8), cy - y * (rr + 2)]; }), col.ink, 1.5); }
    else if (inp === 1) { c.strokeStyle = col.ink; c.lineWidth = 1.5; c.beginPath(); c.arc(IW / 2, cy, rr + 2, 0.3, Math.PI * 2 - 0.3); c.stroke(); txt('ITS OWN OUTPUT', IW / 2, h - 10, col.dim, 'center'); }
    else { vactrol(IW / 2, cy, Math.max(3, rr - 2), on ? wi[21] : 0, on ? wi[1] : 0); }
    c.fillStyle = col.dim; c.fillRect(IW, cy, 18, 1);
    const vx = IW + 44; vactrol(vx, cy, rr, on ? wi[22] : 0, on ? wi[16] : 0);
    const bx = vx + 30, bw = Math.max(20, w - bx - 6);
    c.strokeStyle = col.dim; c.lineWidth = 1; c.strokeRect(bx + 0.5, cy - 3.5, bw - 1, 7);
    c.fillStyle = out > 0.002 ? col.ink : col.lo; c.fillRect(bx + 2, cy - 2, Math.round((bw - 4) * clamp(out / 0.71, 0, 1.41) / 1.41), 4);
    txt(out > 0.002 ? 'OUT 2 ' + fWsDb(out) : 'OUT 2 OFF: TURN IT UP TO HEAR IT', bx, cy + 8, out > 0.002 ? col.ink : col.dim);
  } else if (page === 'RHYTHM') {
    const div = idx(sv(P.E), 11), cnt = idx(sv(SQ(14)), 17);
    tags([['PULSER ' + WS_DIVL[div], div ? 'inv' : 'box'], [div ? 'COUNT ' + WS_CNTL[cnt] : '', 'txt'], [div ? 'START ' + ['FREE', 'TRIG', 'STAGE'][idx(sv(SQ(47)), 3)] : '', 'txt']]);
    c.fillStyle = col.lo; c.fillRect(2, base, w - 4, 1);
    if (!div) { c.fillStyle = col.ink; c.fillRect(6, top + 2, 4, H - 3); txt('PULSER OFF · ONE HIT A TRIG', 18, top + H / 2 - 4, col.dim); }
    else {
      // the burst: a hit every interval, BOUNCE closing or opening them, ACCENT fading or swelling them, until COUNT
      // (or the ball at rest); SWING the odd ones late; the hit playing lit
      const b = (sv(SQ(15)) - 0.5) * 2, ac = (sv(SQ(16)) - 0.5) * 2, swg = sv(SQ(17)), n = cnt === 16 ? 64 : cnt + 1;
      const unit = (w - 12) / 9;
      let x = 6, iv = 1, pl = 1;
      const np = on ? Math.round(wi[5]) : -1;
      for (let i = 0; i < n && x < w - 4; i++) {
        const xs = x + (i % 2 ? swg * 0.5 * iv * unit : 0), hh = Math.max(2, pl * (H - 3));
        c.fillStyle = i === np && on ? col.red : i === 0 ? col.ink : (sv(SQ(18)) < 0.999 ? col.dim : col.ink);
        c.fillRect(Math.round(xs), Math.round(base - hh), 3, Math.round(hh));
        if (ac > 0) pl *= 1 - 0.5 * ac; else pl += (1 - pl) * (-ac) * 0.5;
        iv *= b > 0 ? 1 - 0.45 * b : 1 - 0.6 * b;
        if (iv * unit < 1.5 || iv > 8 || pl < 0.02) break;
        x += iv * unit;
      }
    }
  } else if (page === 'MOTION') {
    const len = 1 + idx(sv(SQ(26)), 5), dest = idx(sv(SQ(27)), 4), spl = idx(sv(SQ(49)), 32);
    tags([['GOES TO ' + ['PITCH', 'TIMBRE', 'INDEX', 'LEVEL'][dest], 'box'], ['LENGTH ' + len, 'txt'], ['CLOCK ' + ['AUTO', 'TRIG', 'PULSER', 'OFF'][idx(sv(SQ(48)), 4)], 'txt']]);
    const st = wi ? Math.round(wi[3]) % len : -1, cw = (w - 4) / 5, sh = (H - 14) / 2, smid = top + 1 + sh;
    for (let i = 0; i < 5; i++) {
      const v = (sv(SQ(21 + i)) - 0.5) * 2, bx = Math.round(2 + i * cw + 3), bw = Math.max(3, Math.round(cw - 6));
      const yv = Math.round(smid - v * (sh - 1)), y0 = Math.min(yv, Math.round(smid)), hh = Math.max(1, Math.abs(yv - Math.round(smid)));
      const play = i === st, act = i < len;
      c.strokeStyle = play && on ? col.red : (act ? col.dim : col.lo); c.lineWidth = 1;
      c.strokeRect(bx + 0.5, Math.round(top + 1) + 0.5, bw - 1, Math.round(2 * sh) - 1);
      c.fillStyle = !act ? col.lo : play ? (on ? col.red : col.ink) : col.dim;
      c.fillRect(bx + 3, y0, bw - 6, hh);
      if (spl >> i & 1) { c.fillStyle = act ? col.ink : col.lo; c.fillRect(bx + 1, Math.round(top + 2 * sh) + 3, bw - 2, 2); }
      txt(dest === 0 ? fWsSt(sv(SQ(21 + i))) : fWsShare(sv(SQ(21 + i))), bx + bw / 2, h - 9, act ? col.ink : col.dim, 'center');
    }
  } else {
    const mix = sv(SQ(33)), size = sv(SQ(61)), t60 = wsT60(sv(SQ(34)), size), damp = sv(SQ(64)), chaos = sv(SQ(65));
    tags([['MIX ' + fOff(fPct)(mix), mix > 0.002 ? 'box' : 'txt'], ['SIZE ' + fPct(size), 'txt'], ['DECAY ' + fSec(t60), 'txt'], [sv(SQ(66)) > 0.002 ? 'SHAKE ' + fPct(sv(SQ(66))) : '', 'txt']]);
    // the tail: echoes a reflection apart (SIZE), falling to -60 dB over DECAY, darker late with DAMPING, wandering with CHAOS
    c.fillStyle = col.lo; c.fillRect(2, base, w - 4, 1);
    const L = Math.max(0.4, Math.min(12, t60 * 1.15)), gap = 3 + size * 4, n = Math.floor((w - 6) / gap);
    for (let i = 0; i < n; i++) {
      const t = i / n * L, g = Math.pow(10, -3 * t / t60) * (0.55 + 0.45 * Math.abs(Math.sin(i * 1.7 + chaos * Math.sin(i * 0.37) * 3)));
      const hh = Math.max(0, g * (H - 3) * (0.35 + 0.65 * mix / Math.max(mix, 0.15)));
      c.fillStyle = t < L * (0.5 - 0.35 * damp) ? col.ink : col.dim;
      if (hh >= 0.5) c.fillRect(Math.round(4 + i * gap), Math.round(base - hh), 2, Math.max(1, Math.round(hh)));
    }
  }
  if (!wi) lcdTag(c, col, 'WAITING FOR THE AUDIO', w, top, 'txt', true, col.dim);
}

function makeWebBackend() {
  let ctx = null, node = null, starting = null, pending = [], saveTimer = 0, silent = null, input = null;
  // RADIO (v87): the receivers, connected from here; what they fetch goes to the engine (it decodes it)
  // (made at first use: its constants come later in the page's script)
  let radioM = null;
  const radio = () => radioM || (radioM = makeRadioWeb(m => { if (node) node.port.postMessage({ radio: m }); },
    () => { if (!ui.radios) ui.radios = radioIdle(); ui.radios.st = radioM.status(); ui.radios.src = radioM.sources().map(radioSrcOk); radioUpdate({ st: ui.radios.st, src: ui.radios.src }); }));
  const radioUse = u => radio().setUse(u);
  // CHIMERA's bank: its files (fetched once, kept by the browser's cache when it has one), handed to the engine once
  let bankBufs = null, bankReq = null, bankSent = false;
  function sendBank() {
    if (!node || !bankBufs || bankSent) return;
    bankSent = true;
    node.port.postMessage({ bank: { idx: CHIM.bytes.slice().buffer, files: bankBufs, jobs: CHIM.e.map((e, i) => [i, e.file, e.foff, e.coded]) } });
  }
  function fetchBank() {
    if (bankReq) return bankReq;
    // (each file as text beside the page: its base64 in parts, bank/chimera.N.P.txt; CHIM_PARTS: its length, its parts)
    bankReq = (async () => {
      const parts = typeof CHIM_PARTS !== 'undefined' ? CHIM_PARTS : [];
      if (parts.length < CHIM.files) throw new Error('no bank files in this build');
      let cache = null;
      try { cache = await caches.open('impakt-bank-' + CHIM.n + '-' + parts.map(p => p.join('.')).join('-')); } catch (e) {}
      const dec = t => typeof Uint8Array.fromBase64 === 'function' ? Uint8Array.fromBase64(t) : b64ToBytes(t);
      const total = parts.reduce((a, p) => a + p[1], 0), out = [];
      let got = 0;
      for (let k = 0; k < CHIM.files; k++) {
        const [size, np] = parts[k], buf = new Uint8Array(size);
        let at = 0;
        for (let q = 0; q < np; q++) {
          const url = 'bank/chimera.' + k + '.' + q + '.txt';
          let r = null;
          try { r = cache && await cache.match(url); } catch (e) {}
          if (!r) {
            r = await fetch(url);
            if (!r.ok) throw new Error(r.status === 404 ? 'its files are not beside the page' : 'HTTP ' + r.status);
            if (cache) try { await cache.put(url, r.clone()); } catch (e) {}
          }
          const b = dec((await r.text()).trim());
          if (at + b.length > size) throw new Error('a file of the bank is not the one this page expects');
          buf.set(b, at); at += b.length;
          ui.chimFetch = ++got / total;
        }
        if (at !== size) throw new Error('a file of the bank is cut short');
        out.push(buf.buffer);
      }
      return out;
    })().catch(e => { ui.chimErr = /fetch|network|load/i.test(String(e && e.message)) ? 'OPEN THE PAGE FROM A SERVER' : String(e && e.message || e).toUpperCase(); return null; });
    return bankReq;
  }
  const post = ops => node && node.port.postMessage({ ops });
  async function start() {
    try { if (navigator.audioSession) navigator.audioSession.type = 'playback'; } catch (e) {}
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC || !('AudioWorkletNode' in window)) throw new Error('This browser does not support AudioWorklet.');
    ctx = new AC({ latencyHint: 'interactive' });
    const resumed = ctx.resume();
    // a single-file build carries the worklet inline (it opens from the disk, without a server)
    const inline = typeof WORKLET_SRC === 'string';
    if (!inline) await ctx.audioWorklet.addModule('impakt-worklet.js');
    else {
      try { await ctx.audioWorklet.addModule(URL.createObjectURL(new Blob([WORKLET_SRC], { type: 'text/javascript' }))); }
      catch (e) { await ctx.audioWorklet.addModule('data:text/javascript;charset=utf-8,' + encodeURIComponent(WORKLET_SRC)); }
    }
    const makeNode = wasm => new Promise((res, rej) => {
      const n = new AudioWorkletNode(ctx, 'impakt', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2], processorOptions: { wasm } });
      n.port.onmessage = e => {
        const d = e.data;
        if (d.type === 'ready') res(n);
        else if (d.type === 'error') rej(new Error(d.msg));
        else if (d.type === 'pos') { if (d.radios) radioUse(d.radios.use); onPos(d); }
        else if (d.type === 'ramdata') { if (d.data.length) ramStore.put(d.t, d.rate, d.data); else ramStore.del(d.t); }
      };
    });
    // engine choice: WebAssembly SIMD, plain WebAssembly, then the plain-JS build
    const candidates = [];
    try { const b = b64ToBytes(WASM_SIMD_B64); if (WebAssembly.validate(b)) candidates.push(['wasm-simd', b]); } catch (e) {}
    try { candidates.push(['wasm', b64ToBytes(WASM_B64)]); } catch (e) {}
    for (const [label, bytes] of candidates) {
      try { await WebAssembly.compile(bytes); node = await makeNode(bytes.buffer); ui.engine = label; break; } catch (e) { node = null; }
    }
    if (!node && inline) throw new Error('this browser blocks WebAssembly.');
    if (!node) { await ctx.audioWorklet.addModule('impakt-asm.js'); node = await makeNode(null); ui.engine = 'js'; }
    node.connect(ctx.destination);
    await resumed;
    post(stateOps(state));
    // RAM: the takes kept from the last session, and the input if a RAM track records it
    for (const r of await ramStore.all()) node.port.postMessage({ ram: r });
    // BREAK: the loops of the slots
    for (let s = 0; s < BRK_SLOTS; s++) if (brkMem[s]) node.port.postMessage({ brk: { slot: s, bars: brkMem[s].bars, data: brkMem[s].data } });
    // CUBE: the banks of its USER cube
    for (let b = 0; b < CUBE_N; b++) if (cubeMem[b]) node.port.postMessage({ cube: { bank: b, data: cubeMem[b].data } });
    // CHIMERA: the bank, if it was fetched before the audio started
    sendBank();
    if (state.tracks.some(t => mof(t.p[P.MACH]) === RAM_M && idx(t.p[P.RSRC], RAM_SRC.length) === 0)) backend.enableInput();
    if (pending.length) post(pending);
    pending = [];
    unlock();
    const showLat = () => { const l = latency(); if (l && $('lat')) $('lat').textContent = `${(l.rate / 1000).toFixed(1)} kHz · ${Math.round(l.total)} ms`; };
    showLat(); setInterval(showLat, 3000);
    if (ui.engine === 'js') {
      $('tagline').textContent += ' · JS MODE';
      const n = $('notice'); n.hidden = false; n.classList.remove('err');
      n.textContent = 'This browser blocks WebAssembly: running the fallback JavaScript engine, without the BOX section or the SPECTRAL reverb.';
    }
  }
  // a silent WAV at the audio context's own rate: a 44.1 kHz file would pull an iPhone's output down to
  // 44.1 kHz while the engine runs at 48 kHz, and the resampling in between adds latency
  function silentWav(rate) {
    const b = new Uint8Array(48), d = new DataView(b.buffer), w = (o, t) => [...t].forEach((c, i) => { b[o + i] = c.charCodeAt(0); });
    w(0, 'RIFF'); d.setUint32(4, 40, true); w(8, 'WAVE'); w(12, 'fmt '); d.setUint32(16, 16, true); d.setUint16(20, 1, true); d.setUint16(22, 1, true);
    d.setUint32(24, rate, true); d.setUint32(28, rate * 2, true); d.setUint16(32, 2, true); d.setUint16(34, 16, true); w(36, 'data'); d.setUint32(40, 4, true);
    let bin = ''; b.forEach(x => { bin += String.fromCharCode(x); });
    return 'data:audio/wav;base64,' + btoa(bin);
  }
  function unlock() {
    // A looping silent HTML audio element puts iOS in the playback session, so the mute switch does not
    // silence the drums (the audio session API alone is not enough everywhere, e.g. in an app's web view)
    if (!silent && ctx) {
      try {
        silent = new Audio(silentWav(Math.round(ctx.sampleRate)));
        silent.loop = true; silent.setAttribute('playsinline', '');
      } catch (e) { silent = false; }
    }
    if (silent) silent.play().catch(() => {});
    if (ctx && ctx.state !== 'running') ctx.resume().catch(() => {});
  }
  // what the device adds between the engine and the speaker (base + output latency), plus the engine's own ~4 ms
  function latency() {
    if (!ctx) return null;
    const dev = (ctx.baseLatency || 0) + (ctx.outputLatency || 0);
    return { rate: ctx.sampleRate, dev: dev * 1000, total: dev * 1000 + 190 / ctx.sampleRate * 1000 };
  }
  return {
    kind: 'web',
    latency,
    // CHIMERA: its bank, fetched now, decoded by the engine as it plays (a sample at a time)
    bankLoad() {
      if (!CHIM.n || !CHIM.files) return;
      ui.chimFetch = 0;
      fetchBank().then(b => { if (b) { bankBufs = b; sendBank(); } });
    },
    // BREAK: a loop into a slot (kept here, handed to the engine now or when it starts), cleared, its bars
    brkLoad(slot, rec) {
      brkMem[slot] = rec; ui.brkNames[slot] = rec.name; brkStore.put(slot, rec);
      if (node) node.port.postMessage({ brk: { slot, bars: rec.bars, data: rec.data } });
      else ui.brks = brkLocal();
    },
    brkClear(slot) {
      brkMem[slot] = null; ui.brkNames[slot] = ''; brkStore.del(slot);
      if (node) post([['bclr', slot]]); else ui.brks = brkLocal();
    },
    // CUBE: a bank into a place (kept here, handed to the engine now or when it starts), cleared
    cubeLoad(bank, rec) {
      cubeMem[bank] = rec; ui.cubeNames[bank] = rec.name; cubeStore.put(bank, rec);
      if (node) node.port.postMessage({ cube: { bank, data: rec.data } });
    },
    cubeClear(bank) {
      cubeMem[bank] = null; ui.cubeNames[bank] = ''; cubeStore.del(bank);
      if (node) post([['cclr', bank]]);
    },
    radioSet(k, src) {
      // (the engine gets the receiver's state from radio's own messages, once it runs)
      radio().set(k, src);
    },
    radioSearch(id, q) {
      radioWebSearch(q).then(res => radioAnswer({ id, q: 'search', ok: true, res }))
        .catch(e => radioAnswer({ id, q: 'search', ok: false, err: /fetch|network|cors/i.test(String(e && e.message)) ? 'this page cannot reach it (the plugin can; here, a RECORDING can play)' : String(e && e.message || e) }));
    },
    radioReceivers(id) { setTimeout(() => radioAnswer({ id, q: 'receivers', ok: false, err: 'the public list opens in the plugin only' })); },
    // a recording as receiver k: decoded to mono at 24 kHz (five minutes at most), kept in this browser
    async radioFile(k, file) {
      const AC = window.OfflineAudioContext || window.webkitOfflineAudioContext;
      if (!AC) throw new Error('this browser cannot decode audio');
      const bytes = await file.arrayBuffer();
      const ac = new AC(1, 24000, 24000);
      const buf = await new Promise((res, rej) => { try { const q = ac.decodeAudioData(bytes, res, rej); if (q && q.then) q.then(res, rej); } catch (e) { rej(e); } });
      const ch = buf.numberOfChannels, r = buf.sampleRate / 24000, n = Math.min(Math.floor(buf.length / r), 24000 * 300);
      const mono = new Float32Array(buf.length);
      for (let c = 0; c < ch; c++) { const d = buf.getChannelData(c); for (let i = 0; i < d.length; i++) mono[i] += d[i] / ch; }
      let pk = 1e-6; for (let i = 0; i < mono.length; i++) pk = Math.max(pk, Math.abs(mono[i]));
      const g = Math.min(4, 0.9 / pk), pcm = new Int16Array(n);
      for (let i = 0; i < n; i++) { const q = i * r, i0 = Math.floor(q), f = q - i0; pcm[i] = Math.round(32767 * g * (mono[i0] + ((i0 + 1 < mono.length ? mono[i0 + 1] : 0) - mono[i0]) * f)); }
      radio().setFile(k, pcm);
      radioFileStore.put(k, pcm);
      return n / 24000;
    },
    brkBars(slot, bars) {
      const r = brkMem[slot]; if (!r) return;
      r.bars = bars; brkStore.put(slot, r);
      if (node) post([['bbar', slot, bars]]); else { ui.brks = brkLocal(); brkChanged(); }
    },
    send(op) {
      if (node) post([op]); else if (op[0] === 'play' || op[0] === 'aud') pending.push(op);
      if (op[0] === 'rclr') ramStore.del(op[1]);
    },
    // RAM: the IN source (microphone / audio interface), asked for the first time a track records it
    enableInput() {
      if (input || !ctx || !node || !navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) return;
      input = navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false } })
        .then(stream => { ctx.createMediaStreamSource(stream).connect(node); })
        .catch(e => { input = null; const n = $('notice'); n.hidden = false; n.classList.add('err'); n.textContent = 'Input unavailable: ' + (e && e.message || e); });
    },
    sendAll(ops) { post(ops); },
    ensureAudio() {
      if (ctx && ctx.state !== 'running') ctx.resume();
      if (!starting) {
        starting = start().then(() => { if (ui.engine !== 'js') $('notice').hidden = true; })
          .catch(e => { starting = null; const n = $('notice'); n.hidden = false; n.classList.add('err'); n.textContent = 'Audio unavailable: ' + e.message; });
      }
    },
    save() { clearTimeout(saveTimer); saveTimer = setTimeout(() => { try { localStorage.setItem(STORE, serialState()); } catch (e) {} }, 400); },
    init() {
      // CUBE: the banks kept in this browser (a place loaded meanwhile wins)
      cubeStore.all().then(rs => {
        rs = rs.filter(r => !cubeMem[r.bank]);
        for (const r of rs) { cubeMem[r.bank] = { name: r.name, data: r.data }; ui.cubeNames[r.bank] = r.name; }
        if (!rs.length) return;
        if (node) for (const r of rs) node.port.postMessage({ cube: { bank: r.bank, data: r.data } });
        cubeChanged();
      });
      // BREAK: the loops kept in this browser
      brkStore.all().then(rs => {
        // (a slot loaded meanwhile wins; the engine, if it already runs, gets them now)
        rs = rs.filter(r => !brkMem[r.slot]);
        for (const r of rs) { brkMem[r.slot] = { name: r.name, bars: Math.min(BRK_MAXBARS, Math.max(1, r.bars || 1)), data: r.data }; ui.brkNames[r.slot] = r.name; }
        if (!rs.length) return;
        if (node) for (const r of rs) node.port.postMessage({ brk: { slot: r.slot, bars: brkMem[r.slot].bars, data: brkMem[r.slot].data } });
        else ui.brks = brkLocal();
        brkChanged();
      });
      try {
        let s = migrate(JSON.parse(localStorage.getItem(STORE) || 'null'));
        for (const key of OLD_STORES) if (!s) s = migrate(JSON.parse(localStorage.getItem(key) || 'null'));
        if (validState(s)) state = s;
      } catch (e) {}
      // RADIO: the receivers the project keeps (they connect when a track plays them), the recordings this browser keeps
      radioFileStore.all().then(rs => {
        for (const x of rs) radio().setFile(x.k, x.pcm);
        if (Array.isArray(state.radio)) state.radio.forEach((src, k) => { if (k < RADIO_RX && radioSrcOk(src)) radio().set(k, src); });
      });
      const gesture = () => { this.ensureAudio(); unlock(); };
      document.addEventListener('touchend', gesture, { passive: true });
      document.addEventListener('click', gesture);
      document.addEventListener('visibilitychange', () => { if (!document.hidden && ctx && ctx.state !== 'running') ctx.resume().catch(() => {}); });
    },
  };
}

// ------------------------------------------------------------------ helpers
const trk = () => state.tracks[ui.t];
const send = op => backend.send(op);
const touched = () => backend.save();

function onPos(d) {
  ui.pos.steps = d.steps || ui.pos.steps;
  ui.pos.hits = d.hits || ui.pos.hits;
  ui.pos.run = d.run;
  ui.pos.bpm = d.bpm || 0;
  ui.pos.host = d.host || 0;
  if (typeof d.gr === 'number') { ui.mtr.gr = Math.max(ui.mtr.gr, d.gr); ui.mtr.out = Math.max(ui.mtr.out, d.out); }
  if (d.mix) ui.mix = d.mix;
  if (d.loop) ui.loop = d.loop;
  if (d.pat) patFollow(d.pat);
  // the quantiser's notes now: following a track, its keys move with the chord
  if (typeof d.qm === 'number' && d.qm >= 0 && d.qm !== ui.qm) { ui.qm = d.qm; if (qFollow() >= 0 && ui.qShown !== qLive()) renderQuantKeys(); }
  renderQFol();
  ui.spec = d.spec || null;
  ui.fx = d.fx || null;
  ui.glitch = d.glitch || null;
  // RAM: each buffer (length, rate, version, state, progress, peaks counter), the peaks that changed
  if (d.ram) {
    ui.ramInfo = d.ram;
    // STRT / LEN read in seconds of the take: redraw the knobs when the take of the track shown changes
    const o = ramPlayed(ui.t) * 6, key = d.ram[o] + '/' + d.ram[o + 1];
    if (key !== ui.ramKey) { ui.ramKey = key; if (mof(trk().p[P.MACH]) === RAM_M) { renderKnobs(); renderScreen(); } }
  }
  if (d.peaks) for (const t in d.peaks) ui.ramPeaks[+t] = d.peaks[t];
  ui.prism = d.prism || null;
  ui.halo = d.halo || null;
  ui.brk = d.brk || null;
  ui.chim = d.chim || null;
  // CUBE on the page: where A and B are, the banks made (a change: the panel, the Z knob), the wave A reads; the
  // plugin sends the banks' names and states when they change
  ui.cube = d.cube || null;
  ui.west = d.west || null;
  // RADIO: the track shown (its receiver, the loop, the history's peaks); the receivers; the answers to the panel's questions
  ui.radio = d.radio || null;
  if (d.radios) radioUpdate(d.radios);
  if (d.radq) d.radq.forEach(radioAnswer);
  if (d.upd) { ui.upd = d.upd; renderUpdates(); }   // (v87: the updater)
  if (d.cubes) { ui.cubes = d.cubes; if (d.cubes.names) ui.cubeNames = d.cubes.names.slice(0, 8); cubeChanged(); }
  else if (d.cube) { const mk = Math.round(d.cube[7]) + (d.cube[8] > 0.5 ? 256 : 0); if (mk !== ui.cubeMask) { ui.cubeMask = mk; cubeChanged(); } }
  // FX: the machine of the track shown, its source, gate, gesture, levels and readouts; kept as a history for its screen
  ui.fxm = d.fxm || null;
  if (d.fxm) {
    const key = ui.t + '/' + Math.round(d.fxm[0]);
    if (ui.fxmKey !== key) { ui.fxmKey = key; ui.fxmHist = []; ui.fxmEv = []; }
    ui.fxmHist.push(d.fxm.slice(1));
    if (ui.fxmHist.length > 240) ui.fxmHist.shift();
    // BEND: its divisions as they come (a new event, or the same one again after a while)
    if (Math.round(d.fxm[0]) === FX_M0 + 2) {
      const ev = [Math.round(d.fxm[6]), Math.round(d.fxm[7]), d.fxm[9], Math.round(d.fxm[10])], evs = ui.fxmEv || (ui.fxmEv = []), last = evs[evs.length - 1];
      if (!last || last[3] !== ev[3]) { evs.push(ev); if (evs.length > 12) evs.shift(); }
      else { last[0] = ev[0]; last[1] = ev[1]; last[2] = ev[2]; }
    }
  }
  // BREAK's slots, when they changed (the plugin sends their names with them)
  if (d.brks) {
    ui.brks = d.brks;
    if (d.brks.names) ui.brkNames = d.brks.names.slice(0, BRK_SLOTS);
    brkChanged();
  }
  // LFO / EF of every track; the followers of the track shown, as a history for its screen
  if (d.mod) {
    ui.mod = d.mod;
    if (ui.efT !== ui.t) { ui.efT = ui.t; ui.efHist = [[], []]; }
    const now = performance.now();
    for (let e = 0; e < 2; e++) {
      const hst = ui.efHist[e]; hst.push([d.mod[ui.t * 8 + 4 + 2 * e], d.mod[ui.t * 8 + 5 + 2 * e], now]);
      let old = 0; while (old < hst.length && now - hst[old][2] > EF_WIN + 500) old++;
      if (old) hst.splice(0, old);
    }
  }
  // an engine started (or restarted) after the page chose the track: ask again
  if (!d.prism && prismSide() && performance.now() - (ui.prsmAsk || 0) > 1000) { ui.prsmAsk = performance.now(); send(['prsm', ui.t]); }
  if (!d.halo && haloShown() && performance.now() - (ui.haloAsk || 0) > 1000) { ui.haloAsk = performance.now(); send(['halo', ui.t]); }
  if (!d.brk && brkShown() && performance.now() - (ui.brkAsk || 0) > 1000) { ui.brkAsk = performance.now(); send(['brk', ui.t]); }
  if (!d.fxm && fxmShown() && performance.now() - (ui.fxmAsk || 0) > 1000) { ui.fxmAsk = performance.now(); send(['fxm', ui.t]); }
  if (!d.chim && chimShown() && performance.now() - (ui.chimAsk || 0) > 1000) { ui.chimAsk = performance.now(); send(['chm', ui.t]); }
  if (!d.cube && cubeShown() && performance.now() - (ui.cubeAsk || 0) > 1000) { ui.cubeAsk = performance.now(); send(['cube', ui.t]); }
  if (!d.west && westShown() && performance.now() - (ui.westAsk || 0) > 1000) { ui.westAsk = performance.now(); send(['west', ui.t]); }
  if (!d.radio && radioShown() && performance.now() - (ui.radAsk || 0) > 1000) { ui.radAsk = performance.now(); send(['radt', ui.t]); }
  // the engine forgets which track to analyse when it (re)starts: ask again
  if (!d.spec && ui.specT !== undefined && performance.now() - (ui.specAsk || 0) > 1000) { ui.specAsk = performance.now(); send(['spec', ui.t]); }
}

// ------------------------------------------------------------------ edits
function quant(meta, v) { return meta && meta.n ? mid(idx(v, meta.n), meta.n) : clamp(v, 0, 1); }

// v65: the seeds of the loops (RDEC's LOOP, the LOOP wave's): a new loop each time its knob leaves where it rests (RDEC to
// its LOOP side, an LFO's depth out of 0) or the LOOP wave is chosen, the same loop then as long as it plays. A lock
// draws one only for a track that has none yet
const isLoopW = v => idx(v, NWAV) % NSHAPE === W_LOOP;
function reseed(t, p, was, v, lock) {
  const tr = state.tracks[t];
  let q = -1;
  if (p === P.RDEC) { if (v < 0.495 && !(was < 0.495)) q = P.RDSD; }
  else if (p === P.C && mof(tr.p[P.MACH]) === CHIM_M) { if (v < 0.495 && !(was < 0.495)) q = P.S15; }   // (CHIMERA's VARI, to its LOOP side)
  else if (p === P.LDEP || p === P.L2DP) { if (Math.abs(was - 0.5) <= 0.005 && Math.abs(v - 0.5) > 0.005 && isLoopW(tr.p[p === P.LDEP ? P.LWAV : P.L2WV])) q = p === P.LDEP ? P.L1SD : P.L2SD; }
  else if (p === P.LWAV || p === P.L2WV) { if (isLoopW(v) && !isLoopW(was)) q = p === P.LWAV ? P.L1SD : P.L2SD; }
  if (q < 0 || (lock && tr.p[q] !== 0)) return;
  tr.p[q] = newSeed(); send(['p', t, q, tr.p[q]]);
}
function setTrackParam(p, v) {
  const tr = trk(), meta = paramMeta(ui.t, p);
  v = quant(meta, v);
  // HALO: its CHORD / VOICE switch belongs to the track (never locked): it changes the page; BREAK's PLAY / CHOP and the
  // FX machines' MAIN / MORE too
  if (p === P.F && (SIDE3.has(mof(tr.p[P.MACH])) || mof(tr.p[P.MACH]) === BREAK_M || fxM(mof(tr.p[P.MACH])))) {
    const ns = SIDE_N[mof(tr.p[P.MACH])] || 2, was = idx(tr.p[P.F], ns);
    tr.p[P.F] = v; send(['p', ui.t, P.F, v]); ui.lastP = p; touched();
    if (idx(v, ns) !== was) renderAll();
    return;
  }
  // RAM: its PLAY / REC switch belongs to the track (never locked): it changes what the trig keys place
  if (p === P.F && mof(tr.p[P.MACH]) === RAM_M) {
    const was = idx(tr.p[P.F], RAM_SIDES);
    tr.p[P.F] = v; send(['p', ui.t, P.F, v]); ui.lastP = p; touched();
    // (from a PRISM page to the other one the trig keys stay the same: a p-lock in progress stays)
    if (idx(v, RAM_SIDES) !== was) { if (was === SIDE_REC || idx(v, RAM_SIDES) === SIDE_REC) { dropLock(); ui.ratStep = -1; } renderAll(); }
    if (idx(v, RAM_SIDES) === SIDE_REC && idx(tr.p[P.RSRC], RAM_SRC.length) === 0) backend.enableInput();
    return;
  }
  if (ui.lock) { for (const s of ui.lock) { reseed(ui.t, p, p in tr.steps[s].l ? tr.steps[s].l[p] : tr.p[p], v, true); tr.steps[s].l[p] = v; send(['lock', ui.t, s, p, v]); } }
  else { reseed(ui.t, p, tr.p[p], v); tr.p[p] = v; send(['p', ui.t, p, v]); if (ui.rec && !RAM_REC_PS.includes(p)) recordLock(p); }
  if (p === P.RSRC && idx(v, RAM_SRC.length) === 0) backend.enableInput();
  if (p === P.A && fxM(mof(tr.p[P.MACH]))) renderPads();     // (FX: the pads show what goes into it)
  ui.lastP = p; touched();
}
// REC: while the sequence runs, the knob being moved (or held) is written as a p-lock on each trig of
// the track that plays; the track's own value follows the knob too. Clear mod removes them all.
function recordLock(p) {
  const tr = trk(), s = ui.pos.run ? ui.pos.steps[ui.t] : -1;
  if (s < 0 || !tr.steps[s] || !tr.steps[s].on || p === P.MACH || tr.steps[s].l[p] === tr.p[p]) return;
  tr.steps[s].l[p] = tr.p[p]; send(['lock', ui.t, s, p, tr.p[p]]);
  touched(); renderSteps(); renderTabs();
}
function setRec(on) { ui.rec = on; ui.recStep = -1; renderTransport(); if (ui.key) renderLockbar(); }
// KEY: the trig keys as a keyboard for the track shown. Without the scale (or on a track that does not follow it): an
// octave laid out as a piano, keys 9 .. 16 the white notes C .. C, keys 2 3 and 5 6 7 the black ones above them, 1 4 8
// rest. With the scale on for the track: 16 notes of the scale in a row from its root, every key playable, the row
// centred on the machine's own pitch. Notes are semitones from that pitch (NOTE 0), within NOTE's ±24; OCT moves the row
const PIANO = [null, 1, 3, null, 6, 8, 10, null, 0, 2, 4, 5, 7, 9, 11, 12];
function keyLayout(oct = ui.oct || 0) {
  // BREAK: the keys are its slices, 16 a bar (OCT moves a bar)
  if (curMachine() === BREAK_M) {
    const L = 16 * brkBars(brkResolve(idx(trk().p[P.A], BRK_SLOTS)));
    return { fold: true, brk: true, keys: [...Array(NS).keys()].map(i => { const n = i + 16 * oct; return n >= 0 && n < L ? n : null; }) };
  }
  const fold = !!state.scale && trk().q !== 0;
  let keys;
  if (!fold) keys = PIANO.map(n => n == null ? null : n + 12 * oct);
  else {
    const r = ((state.qroot || 0) % 12 + 12) % 12, pcs = [];
    for (let i = 0; i < 12; i++) if (state.scale & (1 << ((r + i) % 12))) pcs.push(i);
    const k = pcs.length, span = Math.floor(15 / k) * 12 + pcs[15 % k], o0 = Math.round((-span / 2 - r) / 12);
    keys = [...Array(NS).keys()].map(i => r + 12 * (o0 + oct) + Math.floor(i / k) * 12 + pcs[i % k]);
  }
  keys = keys.map(n => n == null || n < -24 || n > 24 ? null : n);
  return { fold, keys };
}
const noteName = n => NOTE_NAMES[((n % 12) + 12) % 12];
function setKey(on) { ui.key = on; if (on) { dropLock(); ui.all = false; ui.picker = false; ui.ptn = false; ui.chainEdit = false; ui.patArm = null; } renderAll(); }
function setOct(d) {
  const b = curMachine() === BREAK_M, o = clamp((ui.oct || 0) + d, b ? 0 : -3, b ? BRK_MAXBARS - 1 : 3);
  if (keyLayout(o).keys.some(n => n != null)) { ui.oct = o; renderSteps(); renderLockbar(); }
}
// a key: the note on the track shown; with REC while the sequence runs, it goes on the step that plays (a trig and a
// NOTE p-lock), like a played part
function keyPlay(s) {
  const kl = keyLayout(), n = kl.keys[s];
  if (n == null) return null;
  backend.ensureAudio();
  if (kl.brk) {
    // BREAK: the key's slice (NOTE moves the slice from SLCE's; REC writes SLCE on the step)
    const sc = idx(trk().p[P.C], BRK_MAXLEN + 1);
    send(['aud', ui.t, 1, n - (sc ? sc - 1 : 0)]);
    if (ui.rec && ui.pos.run) {
      const tr = trk(), st = ui.pos.steps[ui.t];
      if (st >= 0 && tr.steps[st]) {
        if (!tr.steps[st].on) setTrig(st, 1);
        const v = mid(n + 1, BRK_MAXLEN + 1); tr.steps[st].l[P.C] = v; send(['lock', ui.t, st, P.C, v]);
        if (P.NOTE in tr.steps[st].l) { delete tr.steps[st].l[P.NOTE]; send(['lock', ui.t, st, P.NOTE, -1]); }
        touched(); renderTabs();
      }
    }
    return null;
  }
  send(['aud', ui.t, 1, n]);
  if (ui.rec && ui.pos.run) {
    const tr = trk(), st = ui.pos.steps[ui.t];
    if (st >= 0 && tr.steps[st]) {
      if (!tr.steps[st].on) setTrig(st, 1);
      const v = 0.5 + n / 48; tr.steps[st].l[P.NOTE] = v; send(['lock', ui.t, st, P.NOTE, v]);
      touched(); renderTabs();
    }
  }
  return n;            // (its release: the note a held key lets go, DEC on HOLD)
}
// ------------------------------------------------------------------ patterns (Elektron style)
// 8 banks (A .. H) of 16. PTN turns the 16 keys into the patterns of the bank shown and the 8 pads into the banks. A
// pattern tapped comes at the end of the one playing (its CHG length), or at once (START: from its first step, JUMP:
// where the playhead is), at once when stopped. CHAIN: the patterns tapped play in turn. COPY / PASTE / CLEAR on a key.
// Each pattern keeps its sounds and steps; the mutes stay the project's. The engine changes pattern on its own (the end
// of a pattern, a chain): the page follows it (patFollow)
const patName = i => 'ABCDEFGH'[i >> 4] + String((i & 15) + 1).padStart(2, '0');
const PAT_CHG = [0, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128];
const PAT_MODES = ['SEQ', 'START', 'JUMP'];
const patHas = pt => !!pt && pt.tracks.some(t => t.steps.some(st => st.on || st.rec || st.tl || Object.keys(st.l).length));
const patUsed = i => i === state.pat || patHas(state.pats[i]);
const patNextShown = () => { const pi = ui.patInfo; return pi && pi[1] >= 0 ? Math.round(pi[1]) : -1; };
function setPtn(on) {
  ui.ptn = on;
  if (on) { dropLock(); ui.key = false; ui.all = false; ui.picker = false; ui.bank = state.pat >> 4; ui.moveFrom = null; }
  else { ui.chainEdit = false; ui.patArm = null; }
  renderAll();
}
// a key in PTN mode: paste / clear when armed, a chain being built, else the pattern queued (the chain stops)
function patKey(i) {
  const p = (ui.bank | 0) * NS + i;
  if (ui.patArm === 'paste') { ui.patArm = null; patPaste(p); renderAll(); return; }
  if (ui.patArm === 'clear') { ui.patArm = null; patClear(p); renderAll(); return; }
  if (ui.chainEdit) {
    if (!ui.chainNew) { state.chain = []; ui.chainNew = true; }
    if (state.chain.length < 64) state.chain.push(p);
    if (state.chain.length === 1 && !ui.pos.run && p !== state.pat) patGo(p);
    const ops = []; chainOps(ops, state.chain); backend.sendAll(ops);
    touched(); renderLockbar(); renderSteps();
    return;
  }
  if (state.chain.length) { state.chain = []; const ops = []; chainOps(ops, []); backend.sendAll(ops); }
  patGo(p);
}
function patGo(p) {
  send(['pq', p, state.pmode | 0]);
  if (!ui.pos.run) patSwap(p);                      // stopped: at once
  touched(); renderLockbar(); renderSteps();
}
// the engine plays another pattern: the one shown goes back into its slot, the new one comes into the tracks (a pattern
// never written: the sounds of this one, no trigs, 16 steps at 1x — as the engine does)
function patSwap(next) {
  const old = state.pat;
  if (next === old) return;
  state.pats[old] = patOf(state.tracks, state.pchg);
  const pt = state.pats[next];
  state.tracks.forEach((t, i) => {
    const n = pt ? pt.tracks[i] : null;
    if (n) { t.p = n.p.slice(); t.len = n.len; t.spd = n.spd; t.q = n.q; t.pre = n.pre ? Object.assign({}, n.pre) : null; t.steps = n.steps.map(st => JSON.parse(JSON.stringify(st))); }
    else { t.steps = emptySteps(); t.len = 16; t.spd = 4; }
  });
  state.pchg = pt ? pt.chg | 0 : 0;
  state.pats[next] = null; state.pat = next;
  send(['epat', next]);
  ui.patWait = { p: next, t: performance.now() };
  dropLock(); ui.undo = null;
  if ((ui.spage | 0) * NS >= trk().len) ui.spage = 0;
  touched(); renderAll();
}
function patFollow(d) {
  const prev = ui.patInfo; ui.patInfo = d;
  const cur = Math.round(d[0]);
  // (a message from before a change the page made itself: wait for the engine to get there)
  if (ui.patWait) { if (cur === ui.patWait.p || performance.now() - ui.patWait.t > 1500) ui.patWait = null; else return; }
  if (cur !== state.pat && cur >= 0 && cur < NPAT) { patSwap(cur); return; }
  const k = a => a ? [Math.round(a[1]), Math.round(a[2]), Math.round(a[3])].join() : '';
  if (k(prev) !== k(d)) { renderLockbar(); if (ui.ptn) renderSteps(); }
  else if (ui.ptn) { const el = $('ptncnt'); if (el) el.textContent = patCount(); }
}
function patCount() {
  const d = ui.patInfo; if (!d || !ui.pos.run || d[7] < 0) return '';
  return 'CHANGE IN ' + Math.max(1, Math.ceil(d[7] - 1e-6)) + ' / ' + Math.round(d[5]);
}
function patCopy() { ui.clip = patOf(state.tracks, state.pchg); ui.clipName = patName(state.pat); }
function patPaste(p) {
  if (!ui.clip) return;
  const c = JSON.parse(JSON.stringify(ui.clip));
  if (p === state.pat) {
    // into the pattern playing: its sounds and steps now (the mutes stay)
    state.tracks.forEach((t, i) => { const n = c.tracks[i]; t.p = n.p.slice(); t.len = n.len; t.spd = n.spd; t.q = n.q; t.pre = n.pre; t.steps = n.steps; });
    state.pchg = c.chg | 0;
    const ops = [['reset']]; state.tracks.forEach((t, ti) => trackOps(ops, t, ti)); ops.push(['pchg', state.pchg]);
    backend.sendAll(ops);
  } else {
    state.pats[p] = c;
    const ops = []; patOps(ops, p, c); backend.sendAll(ops);
  }
  touched();
}
function patClear(p) {
  if (p === state.pat) {
    // the pattern playing: its trigs and p-locks go, its sounds and lengths stay
    state.tracks.forEach(t => { t.steps = emptySteps(); });
    send(['reset']);
  } else { state.pats[p] = null; send(['pclr', p]); }
  touched();
}
function renderPatKeys() {
  const cur = state.pat, nx = patNextShown(), b0 = (ui.bank | 0) * NS, inChain = new Set(state.chain);
  steps.forEach((b, i) => {
    const p = b0 + i, sn = b.querySelector('.sn');
    for (const c of ['on', 'other', 'lk', 'tl', 'sel', 'out', 'rat', 'noff', 'nblk', 'nroot']) b.classList.remove(c);
    b.firstChild.innerHTML = ''; b.dataset.r = '1';
    b.classList.toggle('pcur', p === cur);
    b.classList.toggle('pnext', p === nx && nx !== cur && ui.pos.run);
    b.classList.toggle('pused', patUsed(p));
    b.classList.toggle('pchn', inChain.has(p));
    b.classList.toggle('parm', !!ui.patArm);
    const txt = patName(p);
    if (sn.textContent !== txt) sn.textContent = txt;
    b.setAttribute('aria-pressed', String(p === cur));
    b.setAttribute('aria-label', `Pattern ${txt}` + (p === cur ? ', playing' : '') + (p === nx && nx !== cur ? ', next' : '') + (patUsed(p) ? '' : ', empty'));
  });
}
function clearMods(t) {
  const tr = state.tracks[t];
  tr.steps.forEach((st, s) => { if (Object.keys(st.l).length) { st.l = {}; send(['clrl', t, s]); } });
  touched();
}
// CONTROL ALL: the same relative move on every track (differences between tracks are kept). It leaves alone, shown
// locked, what has no sense as a move shared by different sounds: the machines' model / algorithm selectors (MACRO's
// model, FM·X ALGO, the wavetables and WT·DRM's structures, the toms' SIZE, RAM's SIDE and slices, the sources), the choke groups, the LFOs' destinations
// and waves, the filter type, the BOX impulse and its reverse key. A track can be left out (its LED in the ALL bar).
// Entering ALL keeps every track's sound as it was: REVERT goes back to it, in ALL or after (until ALL is entered again)
const SEL_LABELS = new Set(['ALGO', 'MODE', 'MODL', 'MAP', 'FORM', 'SIDE', 'SLCE', 'SRC', 'TBL', 'STRC', 'SIZE', 'SHAP', 'DAC']);
const ALL_LOCKED = new Set([P.CHOK, P.LDST, P.L2DS, P.LWAV, P.L2WV, P.FTYP, P.IR, P.IREV, P.RSRC, P.RBUF,
  P.E1SR, P.E1BD, P.E1DS, P.E2SR, P.E2BD, P.E2DS]);
const allLocked = (t, p) => {
  if (ALL_LOCKED.has(p)) return true;
  if (p < P.A || p > P.F) return false;
  const m = paramMeta(t, p); return !!m && !!m.n && SEL_LABELS.has(m.l);
};
const allOut = t => !!state.tracks[t].ax;
const allSkips = (t, p) => allOut(t) || allLocked(t, p);
function setAllParam(p, bases, delta) {
  for (let t = 0; t < NT; t++) {
    if (allSkips(t, p)) continue;
    const v = quant(paramMeta(t, p), bases[t] + delta);
    reseed(t, p, state.tracks[t].p[p], v);
    state.tracks[t].p[p] = v; send(['p', t, p, v]);
  }
  ui.lastP = p; touched();
}
function setAll(on) {
  ui.all = on;
  if (on) { dropLock(); ui.picker = false; ui.key = false; ui.allSnap = state.tracks.map(t => t.p.slice()); }
  renderAll();
}
// the snapshot differs from the tracks now (REVERT has something to do)
const allChanged = () => !!ui.allSnap && state.tracks.some((t, i) => t.p.some((v, p) => Math.abs(v - ui.allSnap[i][p]) > 1e-6));
function allRevert() {
  if (!ui.allSnap) return;
  state.tracks.forEach((t, i) => t.p.forEach((v, p) => {
    const o = ui.allSnap[i][p];
    if (Math.abs(v - o) > 1e-6) { t.p[p] = o; send(['p', i, p, o]); }
  }));
  if (!ui.all) ui.allSnap = null;            // after ALL: once done, the key goes away
  touched(); renderAll();
}
function setAllOut(t, out) {
  if (out) state.tracks[t].ax = 1; else delete state.tracks[t].ax;
  touched(); renderLockbar(); renderKnobs();
}
function clearParamLock(p) {
  if (!ui.lock) return;
  const tr = trk();
  for (const s of ui.lock) { delete tr.steps[s].l[p]; send(['lock', ui.t, s, p, -1]); }
  touched();
}
function setGlobal(g, v) {
  const meta = gMeta(g);
  v = g === G.BPM ? clamp(Math.round(v), 30, 300) : quant(meta, v);
  state.g[g] = v; send(['g', g, v]); touched();
}
function setTrig(s, on) {
  const st = trk().steps[s];
  st.on = on ? 1 : 0;
  send(['trig', ui.t, s, st.on]);
  touched();
}
// RAM: a REC trig (a take starts on the step); kept in the project only when set
function setRecTrig(s, on) {
  const st = trk().steps[s];
  if (on) st.rec = 1; else delete st.rec;
  send(['rec', ui.t, s, on ? 1 : 0]);
  touched();
}
// the layer the trig keys edit: the REC trigs on a RAM track switched to REC, else the trigs
const layerOn = s => recSide() ? !!trk().steps[s].rec : !!trk().steps[s].on;
const setLayer = (s, on) => (recSide() ? setRecTrig(s, on) : setTrig(s, on));
const RATP = 999;   // ui.lastP value while the screen shows a ratchet
const RNDP = 998;   // ... and after a random
function setRatchet(s, n) {
  const st = trk().steps[s];
  n = clamp(Math.round(n), 1, RAT_MAX);
  if ((st.r || 1) === n) return false;
  if (n > 1) st.r = n; else delete st.r;
  send(['rat', ui.t, s, n]);
  touched();
  return true;
}
function changeMachine(m) {
  const tr = trk();
  if (ui.lock) {
    setTrackParam(P.MACH, mv(m));
    // (v75: a step locked onto a machine with side pages plays them as a fresh track of it has them, where the step
    // does not lock them already: the track's own places mean another machine's knobs)
    if (MACH[m].sd && m !== mof(tr.p[P.MACH]))
      for (const s of ui.lock) { const l = tr.steps[s].l; SP(NSIDE).forEach((q, i) => { if (!(q in l)) { l[q] = MACH[m].sd[i] || 0; send(['lock', ui.t, s, q, l[q]]); } }); }
  }
  else {
    // the machine's default sound when one is set (the whole preset), else its clean INIT on the machine's controls
    const st = startSound(m);
    // from an FX machine to another: what it takes (SRC) and the side shown stay
    const keep = fxKeep(m, ui.t) ? [[P.A, fxSrcConv(tr.p[P.A], mof(tr.p[P.MACH]), m)], [P.F, tr.p[P.F]]] : [];
    // into an FX machine from a sound (or back): nothing of the sound before is carried over. Its p-locks set another
    // machine's knobs (on an FX track a lock of E would open MIX on its steps, one of A move SRC); its level, pan, TONE,
    // ducking, sends, LFOs and followers would shape the whole insert. The FX track starts as a clean insert (unity
    // gain, centred, flat, nothing modulated: MIX 0 is the source as it is); the trigs stay (they play its IN or gesture)
    const cross = fxM(m) !== fxM(mof(tr.p[P.MACH]));
    if (cross) clearMods(ui.t);
    if (cross && fxM(m)) {
      const vals = { ...st.vals, [P.DUCK]: 0, [P.PAN]: 0.5 };
      tr.p[P.MACH] = mv(m); tr.pre = st.pre;
      for (const k in vals) tr.p[k] = vals[k];
      [P.MACH, ...Object.keys(vals).map(Number)].forEach(p => send(['p', ui.t, p, tr.p[p]]));
      touched(); renderAll();
      return;
    }
    tr.p[P.MACH] = mv(m);
    tr.pre = st.pre;
    if (st.own) {
      for (const k in st.vals) tr.p[k] = st.vals[k];
      for (const [q, v] of keep) tr.p[q] = v;
      [P.MACH, ...Object.keys(st.vals).map(Number)].forEach(p => send(['p', ui.t, p, tr.p[p]]));
    } else {
      machineDefaults(tr.p, m);
      Object.assign(tr.p, CLEAN_INIT[MACH[m].id] || {});
      if (m === RAM_M) Object.assign(tr.p, RAM_PAD);
      for (const [q, v] of keep) tr.p[q] = v;
      // (v75: the side places too, the machine's own when it has them: ALLOY, CHIMERA, CUBE, WEST; the engine kept the
      // last machine's before)
      [P.MACH, P.TUNE, P.DEC, P.A, P.B, P.C, P.D, P.E, P.F, ...(m === RAM_M ? Object.keys(RAM_PAD).map(Number) : []), ...(fxM(m) ? FX_XS : []), ...SP(NSIDE)].forEach(p => send(['p', ui.t, p, tr.p[p]]));
    }
    touched();
  }
  renderAll();
}
function shownValue(p) {
  const tr = trk();
  if (ui.lock) {
    const s = Math.min(...ui.lock);
    if (p in tr.steps[s].l) return { v: tr.steps[s].l[p], locked: true };
  }
  return { v: tr.p[p], locked: false };
}
function trackHasLock(t, p) { return state.tracks[t].steps.some(st => p in st.l); }

// p-lock: a step double-tapped comes up (several can be up); the knobs then write its p-locks. A step that is up and
// tapped switches between a trig (its locks come with a new hit, red LED) and a change of the sound only (the locks
// reach the sound already playing, no new hit: yellow LED). Swiped down, or OK, it goes back down. On a RAM track's
// REC side the step is a REC trig (its locks set the take).
function setTrigless(s, on) {
  const st = trk().steps[s];
  if (!!st.tl === !!on) return;
  if (on) st.tl = 1; else delete st.tl;
  send(['tlk', ui.t, s, on ? 1 : 0]);
  touched();
}
function liftStep(s) {
  if (!ui.lock) { ui.lock = new Set(); ui.all = false; }
  ui.lock.add(s);
  if (recSide()) { if (!layerOn(s)) setLayer(s, 1); }
  else if (!trk().steps[s].on) setTrigless(s, 1);    // an empty step comes up as a change of the sound
  renderAll();
}
// a step that goes down: a lock-only step left without locks is an empty step again
function settleStep(s) {
  const st = trk().steps[s];
  if (st.tl && !Object.keys(st.l).length) setTrigless(s, 0);
}
function lowerStep(s) {
  if (!ui.lock || !ui.lock.has(s)) return;
  ui.lock.delete(s); settleStep(s);
  if (!ui.lock.size) { ui.lock = null; ui.plockName = ''; }
  renderAll();
}
function toggleLift(s) { if (ui.lock && ui.lock.has(s)) lowerStep(s); else liftStep(s); }
// a step that is up, tapped: trig <-> change of the sound only (REC side: its REC trig on / off)
function flipStepKind(s) {
  const st = trk().steps[s];
  if (recSide()) setLayer(s, st.rec ? 0 : 1);
  else if (st.on) { setTrig(s, 0); setTrigless(s, 1); }
  else { setTrigless(s, 0); setTrig(s, 1); }
  renderSteps(); renderLockbar(); renderScreen();
}
// every step back down (before the track shown changes, or a mode takes the trig keys)
function dropLock() {
  if (ui.lock) for (const s of ui.lock) settleStep(s);
  ui.lock = null; ui.plockName = '';
}
function exitLock() { dropLock(); renderAll(); }

// ------------------------------------------------------------------ random
// Every track is randomised around its current sound; its RND knob (AMP page) sets how far. The machine,
// volume, choke group and ducking are kept, ranges stay musical (no screaming resonance, sends and
// bit-crush used sparingly), pitches move in semitones and follow the scale when the quantiser is on.
const R = Math.random;
const lerp = (a, b, x) => a + (b - a) * x;
const rr = (lo, hi) => lo + (hi - lo) * R();
const chance = x => R() < x;
const RND_RANGE = {
  [P.DEC]: [0.15, 0.85], [P.RES]: [0, 0.68], [P.FENV]: [0.3, 0.85], [P.FDEC]: [0.1, 0.7], [P.DRV]: [0, 0.4],
  [P.PAN]: [0.25, 0.75], [P.LDEP]: [0.25, 0.75], [P.L2DP]: [0.25, 0.75], [P.VEL]: [0.6, 1], [P.MICR]: [0.42, 0.58],
  [P.RVEL]: [0.3, 0.8], [P.GATE]: [0, 0.25], [P.IMIX]: [0.15, 0.6], [P.IPRE]: [0, 0.3], [P.IDCY]: [0.5, 1],
  [P.IGAT]: [0.5, 1], [P.ITON]: [0.3, 0.7], [P.IFB]: [0, 0.3], [P.PNCH]: [0, 0.5],
  // v60: AGE moves within taste (a sour chord, a worn tape: a little), STRUM's AIR
  [P.GDTN]: [0, 0.5], [P.GVAR]: [0, 0.6], [P.GSHN]: [0, 0.6], [P.GDRP]: [0, 0.3], [P.SAIR]: [0, 0.7],
};
const RND_KEEP = new Set([P.MACH, P.VOL, P.CHOK, P.DUCK, P.SPC, P.JUMP, P.JRNG, P.HARM, ...RAM_REC_PS, ...RAM_PRISM_PS,   // (RAM: its takes and PRISM stay; HARM)
  ...Array.from({ length: 24 }, (_, i) => P.HPF + i), P.RPAN, P.SPRD, P.SMON, P.RDEC, P.RDSD, P.L1SD, P.L2SD]);                     // (TONE and the followers too, its MORE page; RPAN)
// random walk inside [lo, hi]: each press moves a value by up to ±depth of its range and bounces off the
// edges, so repeated presses keep exploring instead of drifting back to the middle; 100 % = a fresh value
function walk(v, lo, hi, d) {
  if (d >= 0.98) return rr(lo, hi);
  v = clamp(v, lo, hi);
  let n = v + (R() * 2 - 1) * d * (hi - lo);
  for (let k = 0; k < 4 && (n < lo || n > hi); k++) n = n < lo ? 2 * lo - n : 2 * hi - n;
  return clamp(n, lo, hi);
}
function randomParams(t) {
  const tr = state.tracks[t], p = tr.p.slice(), d = clamp(tr.p[P.SPC], 0, 1), m = mof(p[P.MACH]);
  const pick = meta => mid(Math.floor(R() * meta.n), meta.n);
  for (let i = 1; i < NP; i++) {
    if (RND_KEEP.has(i)) continue;
    const v = p[i];
    let meta = i >= P.A && i <= P.F ? MACH[m].p[i - P.A] : (i === P.LDST || i === P.L2DS ? { n: NDEST } : isSide(i) ? sideMeta(t, m, i) : TP[i] || null);
    let n = v;
    // FX: what it takes (SRC), its switch and what a trig does (ACT) stay; the MORE side means nothing elsewhere
    if (FX_XS.includes(i) && !fxM(m)) continue;
    if (fxM(m)) { if (i === P.A || i === P.F || i === P.XE || FX_OFF.has(i)) continue; meta = fxMeta(t, m, i) || meta; }
    if ((m === MACRO_M || m === BREAK_M || m === RADIO_M) && i === P.A) continue;   // the MACRO model, BREAK's slot and RADIO's receiver stay, like the machine
    if ((m === RAM_M || SIDE3.has(m) || m === BREAK_M) && i === P.F) continue;     // ... and the RAM, HALO, STRUM, CHOIR and BREAK switches
    if (m === CUBE_M && i === P.S7) continue;                                       // (CUBE's SET: the banks it reads stay, like BREAK's slot)
    if (m === CUBE_M && i === P.S14) { p[i] = chance(d * 0.15) ? mid(1 + Math.floor(R() * 5), 6) : (chance(d) ? mid(0, 6) : v); continue; }   // (its BITS, sparingly)
    if (i >= P.HARM && !(SIDE_OWN[m] || []).includes(i)) continue;                // (v59's sides mean nothing on another machine)
    if (i >= P.A && i <= P.F) n = meta.n ? (chance(d) ? pick(meta) : v) : walk(v, 0, 1, d);
    else if (i in RND_RANGE) n = walk(v, ...RND_RANGE[i], d);
    else switch (i) {
      case P.TUNE: { const st = clamp(Math.round((v - 0.5) * 48 + (R() * 2 - 1) * 12 * d), -12, 12); n = 0.5 + st / 48; break; }
      case P.NOTE: if (chance(d * 0.5)) n = nv([-12, -7, -5, 0, 0, 3, 5, 7, 12][Math.floor(R() * 9)]); break;
      case P.FTYP: if (chance(d * 0.35)) n = mid(chance(0.12) ? 3 : Math.floor(R() * 3), 4); break;
      case P.BITS: case P.SRR: n = chance(d * 0.2) ? rr(0.05, 0.45) : lerp(v, 0, d); break;
      case P.DLY: case P.REV: n = chance(0.5) ? lerp(v, rr(0, 0.45), d) : lerp(v, 0, d * 0.5); break;
      case P.ATK: n = chance(d * 0.15) ? rr(0, 0.3) : lerp(v, 0, d); break;
      case P.PROB: n = chance(d * 0.25) ? rr(0.55, 1) : lerp(v, 1, d); break;
      case P.RTRG: n = chance(d * 0.15) ? mid(1 + Math.floor(R() * 7), 12) : (chance(d) ? mid(0, 12) : v); break;
      case P.IR: if (chance(d * 0.4)) n = chance(0.3) ? mid(0, NIR + 1) : mid(1 + Math.floor(R() * NIR), NIR + 1); break;
      case P.IREV: n = chance(d * 0.1) ? mid(1, 2) : (chance(d) ? mid(0, 2) : v); break;
      // LFO targets: the sound destinations only (the retrig ones would start rolls out of nowhere)
      case P.LDST: case P.L2DS: if (chance(d)) n = mid(Math.floor(R() * NDEST_V4), NDEST); break;
      default: n = meta && meta.n ? (chance(d) ? pick(meta) : v) : walk(v, 0, 1, d);   // LFO speeds / waves / targets, RCNT
    }
    p[i] = quant(meta, clamp(n, 0, 1));
  }
  // the cutoff follows the (possibly new) filter type: low-pass stays open-ish, high-pass low-ish
  const ft = idx(p[P.FTYP], 4), fr = [[0.45, 1], [0.3, 0.8], [0, 0.45], [0.3, 0.8]][ft];
  p[P.FREQ] = ft !== idx(tr.p[P.FTYP], 4) ? rr(...fr) : walk(p[P.FREQ], ...fr, d);
  return p;
}
function randomize(list) {
  if (!list.length) { ui.rndInfo = { label: 'RND · NO TRACK', value: '—' }; ui.lastP = RNDP; renderScreen(); return; }
  const undo = [], now = performance.now();
  let changed = 0;
  for (const t of list) {
    const tr = state.tracks[t];
    undo.push([t, tr.p.slice()]);
    randomParams(t).forEach((v, i) => { if (v !== tr.p[i]) { reseed(t, i, tr.p[i], v); tr.p[i] = v; send(['p', t, i, v]); changed++; } });
    ui.flash[t] = now + 260;
  }
  ui.undo = undo;
  ui.rndInfo = { label: (list.length === 1 ? `RND T${list[0] + 1}` : `RND ${list.length} TRACKS`) + ' · PARAM.', value: String(changed) };
  ui.lastP = RNDP;
  touched(); renderAll();
}
function undoRandom() {
  if (!ui.undo) return;
  for (const [t, p] of ui.undo) {
    const tr = state.tracks[t];
    p.forEach((v, i) => { if (v !== tr.p[i]) { tr.p[i] = v; send(['p', t, i, v]); } });
  }
  ui.undo = null;
  ui.rndInfo = { label: 'RANDOM', value: 'UNDONE' }; ui.lastP = RNDP;
  touched(); renderAll();
}

// ------------------------------------------------------------------ presets
// 20 factory presets per machine (web/src/presets.js, levels measured by tools/preset_levels.js) plus the
// user's own. A preset owns the machine, its sound parameters, the filter, drive / bits, attack, the
// delay / reverb / chorus / phaser sends, the LFOs, the BOX and the trig retrig; pan, trig settings and the RND depth stay.
// In p-lock mode a preset becomes a sound lock on the selected steps.
const PR = IMPAKT_PRESETS;
const USER_KEY = 'impakt.presets.user.v1';
// user presets saved before v9 on machines 25..28 were RUMBLE sounds: their machines are gone, so are they
// (v76: a user preset of v75's SPHERE, its number then WEST's, goes: WEST's are marked "wst")
const keptPreset = x => x && typeof x.name === 'string' && x.p && ((x.dv || 4) >= 9 || !(x.m >= 25 && x.m <= 28)) && !(x.m === WEST_M && !x.wst)
  // (v21: before it, on v20's numbers; one of a machine gone goes with it; v22: MATTER's and LATEX's go too)
  && ((x.dv || 4) >= SV22 || ((x.dv || 4) === SV21 ? !gone22(x.m) : (MACH20[x.m] && !(MACH20[x.m] in GONE21))));
// presets saved before v5 hold their LFO destinations on the 24-entry grid, before v8 on the 27-entry one (and a
// one-sided PRISM WDTH)
const destPreset = x => {
  // (v82: a user preset of v81's HOWL: DIST onto LOOP's TIME, LOOP at its defaults, SIDE on four pages)
  if (x.m === HOWL_M && !x.h82) { const p = Object.assign({}, x.p); v82Places(p, false); x = Object.assign({}, x, { p, h82: 1 }); }
  // (v83: one of v82's: SIDE on six pages, CAB and BOW at their defaults)
  if (x.m === HOWL_M && !x.h83) { const p = Object.assign({}, x.p); v83Places(p, false); x = Object.assign({}, x, { p, h83: 1 }); }
  if (x.dv === SV) return x;
  const p = Object.assign({}, x.p), dv = x.dv || 4;
  if (dv < 10) {
    const dest = v => destV10(dv < 5 ? destV8(destV5(v)) : dv < 8 ? destV8(v) : v);
    if (P.LDST in p) p[P.LDST] = dest(p[P.LDST]);
    if (P.L2DS in p) p[P.L2DS] = dest(p[P.L2DS]);
    if (dv < 8 && P.PWID in p) p[P.PWID] = widthV8(p[P.PWID]);
  }
  // v19: a HALO preset's PROG and F onto their new grids; one saved before HARM keeps its chords as they were (FREE)
  if (dv < 19 && x.m === M20.HALO) {
    if (P.B in p) p[P.B] = progV19(p[P.B]);
    if (P.F in p) p[P.F] = sideV19(p[P.F]);
    for (const q of [P.HARM, P.HUNI, P.HSUB, P.HCUT, P.HOPN, P.HVIB, P.HWRM]) if (!(q in p)) p[q] = V19_PAD[q];
  }
  // v20: a HALO or STRUM preset's F onto the four sides; one saved before AGE keeps it at rest (and STRUM's AIR off)
  if (dv < 20 && side4M20(x.m)) {
    if (P.F in p) p[P.F] = sideV20(p[P.F]);
    for (const q of [P.GDTN, P.GVAR, P.GSHN, P.GDRP, P.SAIR]) if (!(q in p)) p[q] = V20_PAD[q];
  }
  // v21: its machine on today's number, the side places at rest (v20's MORE page with STRING and MODAL); a v21 preset
  // keeps its number (MATTER's and LATEX's are gone: keptPreset)
  let m = x.m;
  if (dv < SV21) { m = om21(x.m); SP(NSIDE).forEach(q => { delete p[q]; }); }
  // v23: an ALLOY preset of v22, its sides onto the five pages
  if (dv === SV22 && m === ALLOY_M) al23Dict(p, true);
  // v25: its LFO waves onto the 16 (SLW and LEN at rest: resolve's BASE)
  for (const q of LFO_WAVES) if (q in p) p[q] = waveV25(p[q]);
  delete x.rmb;
  return Object.assign({}, x, { m, p, dv: SV });
};
let userPresets = [];
try { const u = JSON.parse(localStorage.getItem(USER_KEY) || '[]'); if (Array.isArray(u)) userPresets = u.filter(keptPreset).map(destPreset); } catch (e) {}
// kits: the 8 sounds and the master as a whole (and, when saved with it, the pattern); the defaults: per machine, the
// sound its INIT gives (a preset, or the clean INIT), and the kit Init starts from (none: the standard clean kit)
const KIT_KEY = 'impakt.kits.v1', DEF_KEY = 'impakt.defaults.v1';
const validKit = x => x && typeof x.name === 'string' && x.s && Array.isArray(x.s.tracks) && x.s.tracks.length === NT && Array.isArray(x.s.g);
let userKits = [];
try { const u = JSON.parse(localStorage.getItem(KIT_KEY) || '[]'); if (Array.isArray(u)) userKits = u.filter(validKit); } catch (e) {}
let defaults = { sound: {}, kit: '' };
const validDefaults = d => d && typeof d === 'object' && d.sound && typeof d.sound === 'object';
try { const d = JSON.parse(localStorage.getItem(DEF_KEY) || 'null'); if (validDefaults(d)) defaults = { sound: d.sound, kit: typeof d.kit === 'string' ? d.kit : '' }; } catch (e) {}
const presetStore = {
  ref: null, kref: null, dref: null, started: false,
  // the claude.ai viewer keeps the user's presets, kits and defaults in the artifact's private store (every device);
  // anywhere else (local copy, plugin) they live in this browser only
  async connect() {
    if (this.started) return; this.started = true;
    try {
      if (!window.claude || typeof window.claude.use !== 'function') return;
      const [db, user] = await Promise.all([window.claude.use('db'), window.claude.use('user')]);
      if (!db || !user) return;
      const id = await user.id();
      if (!id) return;
      const col = db.collection('data/users/' + id);
      this.ref = col.doc('presets'); this.kref = col.doc('kits'); this.dref = col.doc('defaults');
      const [snap, ksnap, dsnap] = await Promise.all([this.ref.get(), this.kref.get().catch(() => null), this.dref.get().catch(() => null)]);
      const remote = (snap.exists && Array.isArray((snap.data() || {}).list) ? snap.data().list : [])
        .filter(keptPreset).map(destPreset);
      const key = x => x.m + '|' + x.name;
      const seen = new Set(remote.map(key));
      const merged = remote.concat(userPresets.filter(x => !seen.has(key(x))));
      const pushUp = merged.length !== remote.length || ((snap.data() || {}).list || []).length !== remote.length;
      userPresets = merged;
      try { localStorage.setItem(USER_KEY, JSON.stringify(userPresets)); } catch (e) {}
      if (pushUp) await this.ref.set({ list: userPresets });
      // kits: the same merge by name
      const rk = ksnap && ksnap.exists && Array.isArray((ksnap.data() || {}).list) ? ksnap.data().list.filter(validKit) : [];
      const kseen = new Set(rk.map(x => x.name));
      const kmerged = rk.concat(userKits.filter(x => !kseen.has(x.name)));
      const kpush = kmerged.length !== rk.length;
      userKits = kmerged;
      try { localStorage.setItem(KIT_KEY, JSON.stringify(userKits)); } catch (e) {}
      if (kpush) this.kref.set({ list: userKits }).catch(() => {});
      // defaults: the store's when it has some, else this browser's go up
      const rd = dsnap && dsnap.exists ? dsnap.data() : null;
      if (validDefaults(rd)) { defaults = { sound: rd.sound, kit: typeof rd.kit === 'string' ? rd.kit : '' }; try { localStorage.setItem(DEF_KEY, JSON.stringify(defaults)); } catch (e) {} }
      else if (Object.keys(defaults.sound).length || defaults.kit) this.dref.set(defaults).catch(() => {});
      renderPresets(); renderKits();
    } catch (e) { this.ref = this.kref = this.dref = null; }
  },
  save() {
    try { localStorage.setItem(USER_KEY, JSON.stringify(userPresets)); } catch (e) {}
    if (this.ref) this.ref.set({ list: userPresets }).catch(() => {});
  },
  saveKits() {
    try { localStorage.setItem(KIT_KEY, JSON.stringify(userKits)); } catch (e) {}
    if (this.kref) this.kref.set({ list: userKits }).catch(() => {});
  },
  saveDefaults() {
    try { localStorage.setItem(DEF_KEY, JSON.stringify(defaults)); } catch (e) {}
    if (this.dref) this.dref.set(defaults).catch(() => {});
  },
};
// INIT: the machine as designed and nothing added: no drive, fold, grit or feedback in the machine, no
// filter drive, bitcrush, sends, IR, punch or ducking. (The factory presets keep their own colour.)
const CLEAN_INIT = { 'BD·VA': { [P.E]: 0 }, 'BD·FM': { [P.D]: 0, [P.F]: 0 }, 'CP·NZ': { [P.F]: 0 }, 'FM·X': { [P.F]: 0 },
  'ITERA': { [P.D]: 0 }, 'HARD': { [P.C]: 0 }, 'FM·Z': { [P.F]: 0 } };
function initValues(m) {
  const vals = PR.resolve({}, MACH[m].d, 0.8, null, MACH[m].sd, sprDef(m));
  Object.assign(vals, CLEAN_INIT[MACH[m].id] || {});
  vals[P.DUCK] = 0;
  if (m === RAM_M) Object.assign(vals, RAM_PAD);   // RAM: its takes and PRISM back to their defaults too
  return vals;
}
// what a user preset keeps: the sound (PR.OWNED); for RAM also how it records and its PRISM
const ownedParams = m => (m === RAM_M ? [...PR.OWNED, ...RAM_REC_PS, ...RAM_PRISM_PS] : PR.OWNED);
function presetEntries(m) {
  const u = userPresets.filter(x => x.m === m).map(x => ({ kind: 'u', ref: x, tag: 'USER', name: x.name }));
  const f = (PR.LIST[MACH[m].id] || []).map(([tag, name], i) => ({ kind: 'f', i, tag, name }));
  return [{ kind: 'init', tag: 'INIT', name: MACH[m].long }, ...u, ...f];
}
function presetValues(m, e) {
  if (e.kind === 'u') { const out = {}; for (const k in e.ref.p) out[+k] = e.ref.p[k]; return out; }
  if (e.kind === 'init') return initValues(m);
  const vol = (IMPAKT_PRESET_VOL[MACH[m].id] || [])[e.i], tone = (IMPAKT_PRESET_TONE[MACH[m].id] || [])[e.i];
  return PR.resolve(PR.LIST[MACH[m].id][e.i][2], MACH[m].d, vol, tone, MACH[m].sd, sprDef(m));
}
// BREAK: a sound is how the loop is played; which slot it plays stays the track's (a preset or INIT leaves it)
const brkKeep = (m, t) => (m === BREAK_M || m === RADIO_M) && mof(state.tracks[t].p[P.MACH]) === m;   // (v87: RADIO's receiver too)
// FX: a sound is the effect; what it takes (SRC) and the side shown stay the track's (from one FX machine to another too)
const fxKeep = (m, t) => fxM(m) && fxM(mof(state.tracks[t].p[P.MACH]));
// (v83: SRC kept from one FX machine to another, the same source: STENCIL's list has NONE first; NONE elsewhere: SUM)
const fxSrcConv = (v, mFrom, mTo) => { const a = mFrom === STENCIL_M, b = mTo === STENCIL_M; if (a === b) return v;
  const i = a ? idx(v, FX_SRCS_N + 1) - 1 : idx(v, FX_SRCS_N); return b ? mid(i + 1, FX_SRCS_N + 1) : mid(i < 0 ? 1 : i, FX_SRCS_N); };
const sameEntry = (a, b) => a && b && a.kind === b.kind && a.name === b.name;
function trackPreset(t) {
  const tr = state.tracks[t], pre = tr.pre;
  if (!pre) return null;
  const m = mof(tr.p[P.MACH]);
  if (pre.m !== m) return null;
  return presetEntries(m).find(e => sameEntry(e, pre)) || null;
}
function presetModified(t) {
  const e = trackPreset(t); if (!e) return false;
  const m = mof(state.tracks[t].p[P.MACH]), vals = presetValues(m, e), p = state.tracks[t].p;
  if (m === BREAK_M || m === RADIO_M || fxM(m)) delete vals[P.A];
  if (fxM(m)) delete vals[P.F];
  for (const k in vals) if (Math.abs(p[k] - vals[k]) > 1e-4) return true;
  return false;
}
function loadPreset(m, e) {
  const vals = presetValues(m, e), tr = trk();
  if (m === RAM_M && mof(tr.p[P.MACH]) === RAM_M) delete vals[P.F];   // RAM: a preset leaves its PLAY / REC switch alone
  if (brkKeep(m, ui.t)) delete vals[P.A];
  if (fxKeep(m, ui.t)) { const m0 = mof(tr.p[P.MACH]); if (m0 === m) delete vals[P.A]; else vals[P.A] = fxSrcConv(tr.p[P.A], m0, m); delete vals[P.F]; }
  if (ui.lock) {
    for (const s of ui.lock) {
      const l = tr.steps[s].l;
      l[P.MACH] = mv(m); send(['lock', ui.t, s, P.MACH, l[P.MACH]]);
      for (const k in vals) { l[k] = vals[k]; send(['lock', ui.t, s, +k, vals[k]]); }
    }
    ui.plockName = e.name;
  } else {
    tr.p[P.MACH] = mv(m); send(['p', ui.t, P.MACH, tr.p[P.MACH]]);
    for (const k in vals) { tr.p[k] = vals[k]; send(['p', ui.t, +k, vals[k]]); }
    tr.pre = { m, kind: e.kind, name: e.name };
  }
  ui.lastP = P.MACH;
  touched(); renderAll();
  backend.ensureAudio();
  send(['aud', ui.t, 1]);
}
function stepPreset(d) {
  const m = curMachine(), list = presetEntries(m);
  const cur = list.findIndex(e => sameEntry(e, trk().pre && trk().pre.m === m ? trk().pre : null));
  const i = ((cur < 0 ? (d > 0 ? -1 : 0) : cur) + d + list.length) % list.length;
  loadPreset(m, list[i]);
}
function savePreset(name) {
  const tr = trk(), m = mof(tr.p[P.MACH]);
  name = (name || '').trim().toUpperCase().slice(0, 24) || ('USER ' + (userPresets.filter(x => x.m === m).length + 1));
  const p = {};
  for (const k of ownedParams(m)) p[k] = tr.p[k];
  const old = userPresets.findIndex(x => x.m === m && x.name === name);
  const entry = { m, name, p, dv: SV, ...(m === WEST_M ? { wst: 1 } : {}), ...(m === HOWL_M ? { h82: 1, h83: 1 } : {}) };
  if (old >= 0) userPresets[old] = entry; else userPresets.unshift(entry);
  presetStore.save();
  tr.pre = { m, kind: 'u', name };
  touched();
  return name;
}
function deletePreset(ref) {
  userPresets = userPresets.filter(x => x !== ref);
  presetStore.save();
  for (const tr of state.tracks) if (tr.pre && tr.pre.kind === 'u' && tr.pre.name === ref.name && tr.pre.m === ref.m) tr.pre = null;
  const d = defaults.sound[MACH[ref.m].id];
  if (d && d.kind === 'u' && d.name === ref.name) { delete defaults.sound[MACH[ref.m].id]; presetStore.saveDefaults(); }
  touched();
}
const cleanName = n => (n || '').trim().toUpperCase().slice(0, 24);
// a user preset renamed (null: the name is taken by another preset of the machine)
function renamePreset(ref, name) {
  name = cleanName(name);
  if (!name || name === ref.name) return ref.name;
  if (userPresets.some(x => x !== ref && x.m === ref.m && x.name === name)) return null;
  const old = ref.name;
  ref.name = name;
  for (const tr of state.tracks) if (tr.pre && tr.pre.kind === 'u' && tr.pre.m === ref.m && tr.pre.name === old) tr.pre.name = name;
  const d = defaults.sound[MACH[ref.m].id];
  if (d && d.kind === 'u' && d.name === old) { d.name = name; presetStore.saveDefaults(); }
  presetStore.save(); touched();
  return name;
}
// a user preset takes the track's sound as it is now
function updatePreset(ref) {
  const tr = trk();
  for (const k of ownedParams(ref.m)) ref.p[k] = tr.p[k];
  ref.dv = SV;
  tr.pre = { m: ref.m, kind: 'u', name: ref.name };
  presetStore.save(); touched();
}
// the sound a machine starts with (INIT, the machine picker): the preset made its default, else the clean INIT
function defaultEntry(m) {
  const d = defaults.sound[MACH[m].id];
  return d ? presetEntries(m).find(e => e.kind === d.kind && e.name === d.name) || null : null;
}
const isDefault = (m, e) => { const d = defaultEntry(m); return !!e && (d ? sameEntry(d, e) : e.kind === 'init'); };
function setDefaultSound(m, e) {
  if (!e || e.kind === 'init') delete defaults.sound[MACH[m].id]; else defaults.sound[MACH[m].id] = { kind: e.kind, name: e.name };
  presetStore.saveDefaults();
}
function startSound(m) {
  const e = defaultEntry(m);
  return e ? { vals: presetValues(m, e), pre: { m, kind: e.kind, name: e.name }, own: true }
           : { vals: initValues(m), pre: { m, kind: 'init', name: MACH[m].long }, own: false };
}

// ------------------------------------------------------------------ kits
// A kit: the 8 tracks' sounds and the master (every setting but the tempo, the swing and the looper), the scale; saved
// with the pattern it brings the trigs, locks, lengths and speeds too. Init starts from the default kit (its sounds, an
// empty pattern) when there is one.
const kitRound = v => Math.round(v * 1e5) / 1e5;
function kitSnapshot(withPattern) {
  // (the sounds and the master, with the pattern playing when asked: never the project's other patterns)
  const s = JSON.parse(JSON.stringify(Object.assign({}, state, { wst: 1, v78: 1, v82: 1, v83: 1, pats: null, chain: null })));
  delete s.pats; delete s.chain; delete s.pat; delete s.pmode; delete s.pchg;
  s.g = s.g.map(kitRound); s.g[G.LOOP] = 0;
  s.tracks.forEach(t => {
    t.p = t.p.map(kitRound);
    if (!withPattern) { t.steps = emptySteps(); t.len = 16; t.spd = 4; t.mute = 0; }
  });
  return s;
}
const kitState = x => { const s = migrate(JSON.parse(JSON.stringify(x.s))); return validState(s) ? s : null; };
function kitApply(dst, ks, withPattern) {
  ks.tracks.forEach((kt, i) => {
    const t = dst.tracks[i];
    t.p = kt.p.slice(); t.q = kt.q === 0 ? 0 : 1; t.rx = kt.rx ? 1 : 0; t.pre = kt.pre ? Object.assign({}, kt.pre) : null;
    if (kt.ax) t.ax = 1; else delete t.ax;
    if (withPattern) { t.steps = JSON.parse(JSON.stringify(kt.steps)); t.len = kt.len; t.spd = kt.spd; t.mute = kt.mute ? 1 : 0; }
  });
  const keep = withPattern ? [G.LOOP] : [G.BPM, G.SWING, G.LOOP, G.LLEN, G.LPOS];
  dst.g = ks.g.map((v, i) => (keep.includes(i) ? dst.g[i] : v));
  dst.scale = ks.scale | 0; dst.qroot = ks.qroot | 0;
}
function saveKit(name, withPattern) {
  name = cleanName(name) || ('KIT ' + (userKits.length + 1));
  const x = { name, pat: withPattern ? 1 : 0, s: kitSnapshot(withPattern) };
  const old = userKits.findIndex(k => k.name === name);
  if (old >= 0) userKits[old] = x; else userKits.unshift(x);
  presetStore.saveKits();
  ui.kit = name;
  return name;
}
function loadKit(x) {
  const ks = kitState(x);
  if (!ks) return false;
  dropLock(); ui.undo = null;
  kitApply(state, ks, !!x.pat);
  backend.sendAll(stateOps(state)); touched(); ui.kit = x.name;
  renderAll();
  backend.ensureAudio();
  return true;
}
function renameKit(x, name) {
  name = cleanName(name);
  if (!name || name === x.name) return x.name;
  if (userKits.some(k => k !== x && k.name === name)) return null;
  if (defaults.kit === x.name) { defaults.kit = name; presetStore.saveDefaults(); }
  if (ui.kit === x.name) ui.kit = name;
  x.name = name; presetStore.saveKits();
  return name;
}
function deleteKit(x) {
  userKits = userKits.filter(k => k !== x);
  if (defaults.kit === x.name) { defaults.kit = ''; presetStore.saveDefaults(); }
  if (ui.kit === x.name) ui.kit = '';
  presetStore.saveKits();
}
function setDefaultKit(name) { defaults.kit = name || ''; presetStore.saveDefaults(); }
const defaultKit = () => (defaults.kit ? userKits.find(k => k.name === defaults.kit) || null : null);

// ------------------------------------------------------------------ scale quantiser
// what it holds to (G.QSRC): its scale (-1), or the chord a HALO or STRUM track plays (0..7). That track builds its
// chords in the scale itself; the others, CHOIR's ribbon on SNAP among them, hold to the chord's notes
// (a track it names that is no longer a HALO or STRUM is not followed: the engine holds to the scale again)
// (v81: HOWL's chords too: the quantiser can follow them)
const chordSrcM = m => chordM(m) || m === HOWL_M;
const chordTracks = () => state.tracks.map((t, i) => chordSrcM(mof(t.p[P.MACH])) ? i : -1).filter(i => i >= 0);
const qFollow = () => { const k = idx(state.g[G.QSRC] || 0, NT + 1) - 1; return k >= 0 && chordSrcM(mof(state.tracks[k].p[P.MACH])) ? k : -1; };
const quantOn = () => !!state.scale || qFollow() >= 0;
// the notes it holds to now: the followed chord, as the engine plays it (the scale until that track has played one)
const qLive = () => (qFollow() >= 0 && ui.qm > 0 ? ui.qm : state.scale) | 0;
function setQSrc(k) {
  state.g[G.QSRC] = k < 0 ? 0 : mid(k + 1, NT + 1);
  send(['g', G.QSRC, state.g[G.QSRC]]); touched(); renderQuant(); renderChips();
}
// the squares of the chord tracks, in a place of their own (grey, a dash, when there is none): a tap follows its
// chords, a tap on the lit one goes back to the scale. Redrawn only when the chord tracks or the choice change
function renderQFol() {
  const host = $('qfol'), f = qFollow(), cands = chordTracks(), key = cands.join(',') + '|' + f;
  if (host.dataset.k === key) return;
  host.dataset.k = key;
  host.classList.toggle('many', cands.length > 4);
  host.innerHTML = cands.length
    ? cands.map(t => `<button class="qf${t === f ? ' on' : ''}" data-t="${t}" aria-pressed="${t === f}" title="${t === f ? `The quantizer follows T${t + 1}'s chords: tap to go back to the scale` : `Follow T${t + 1}'s chords (${MACH[mof(state.tracks[t].p[P.MACH])].id})`}">${cands.length > 4 ? t + 1 : 'T' + (t + 1)}</button>`).join('')
    : '<span class="qf none" title="No HALO or STRUM track: no chords to follow">—</span>';
}
function setScale(mask, root) {
  state.scale = mask & 0xFFF;
  if (root !== undefined) state.qroot = ((root % 12) + 12) % 12;
  send(['scale', state.scale]); touched(); renderQuant(); renderChips();
}
const KB_WHITE = [0, 2, 4, 5, 7, 9, 11], KB_BLACK = [[1, 1], [3, 2], [6, 4], [8, 5], [10, 6]];   // [pitch class, after white key]
function buildQuant() {
  const kb = $('kb');
  // drawn on the LCD: 7 white cells of 20 x 40, black cells of 13 x 24 between them, a red mark on the root
  const key = (n, cls, x, w, h) => `<g class="key ${cls}" data-n="${n}" tabindex="0" role="switch" aria-label="${NOTE_NAMES[n]}">`
    + `<rect class="k" x="${x}" y="0" width="${w}" height="${h}" rx="1"/>`
    + `<rect class="rd" x="${x + w / 2 - 3}" y="${h - 7}" width="6" height="3"/></g>`;
  kb.innerHTML = '<svg viewBox="0 0 140 40" preserveAspectRatio="none">' + KB_WHITE.map((n, i) => key(n, 'w', i * 20 + 1, 18, 40)).join('')
    + KB_BLACK.map(([n, i]) => key(n, 'b', i * 20 - 6.5, 13, 24)).join('') + '</svg>';
  const toggle = n => { setScale(state.scale ^ (1 << n), state.scale ? undefined : n); };
  kb.querySelectorAll('.key').forEach(g => {
    g.addEventListener('pointerdown', e => { e.preventDefault(); toggle(+g.dataset.n); });
    g.addEventListener('keydown', e => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); toggle(+g.dataset.n); } });
  });
  $('qname').addEventListener('click', () => {
    const r = state.qroot || 0;
    const cur = SCALES.findIndex(([, notes]) => scaleMask(notes, r) === state.scale);
    setScale(scaleMask(SCALES[(cur + 1) % SCALES.length][1], r), r);
  });
  $('qdn').addEventListener('click', () => setScale(rotMask(state.scale, -1), (state.qroot || 0) - 1));
  $('qup').addEventListener('click', () => setScale(rotMask(state.scale, 1), (state.qroot || 0) + 1));
  $('qclr').addEventListener('click', () => { if (qFollow() >= 0) setQSrc(-1); setScale(0); });
  $('qfol').addEventListener('click', e => {
    const b = e.target.closest('.qf[data-t]'); if (!b) return;
    const t = +b.dataset.t; setQSrc(qFollow() === t ? -1 : t);
  });
}
function renderQuant() {
  if (ui.key) { renderSteps(); renderLockbar(); }
  const m = state.scale | 0, r = state.qroot || 0, f = qFollow();
  $('qz').classList.toggle('off', !quantOn());
  const nm = scaleName(m, r);
  $('qname').textContent = nm;
  $('qname').classList.toggle('long', nm.length > 7);
  $('qlab').textContent = f >= 0 ? 'CHORDS OF T' + (f + 1) : 'SCALE';
  renderQFol();
  renderQuantKeys();
}
// the keys: the notes it holds to now lit (following a track: its chord, as it changes), the scale's other notes
// dim, the root marked
function renderQuantKeys() {
  const m = state.scale | 0, r = state.qroot || 0, live = qLive(), f = qFollow() >= 0;
  ui.qShown = live;
  $('kb').querySelectorAll('.key').forEach(g => {
    const n = +g.dataset.n, on = !!((live >> n) & 1), inS = !!((m >> n) & 1);
    g.classList.toggle('on', on);
    g.classList.toggle('base', f && inS && !on);
    g.classList.toggle('root', !!m && n === r);
    g.setAttribute('aria-checked', inS ? 'true' : 'false');
  });
}

// per track, under the pads: R = included in "Rnd all", Q = follows the quantiser
function buildChips() {
  const host = $('tsub');
  for (let t = 0; t < NT; t++) {
    const cell = document.createElement('div');
    cell.className = 'tcell';
    for (const kind of ['r', 'q']) {
      const b = document.createElement('button');
      b.className = 'tchip ' + kind;
      b.textContent = kind === 'r' ? 'R' : 'Q';
      b.title = kind === 'r' ? `T${t + 1}: included in “Rnd all”` : `T${t + 1}: follows the quantizer`;
      b.setAttribute('aria-label', (kind === 'r' ? 'Random, track ' : 'Quantizer, track ') + (t + 1));
      b.addEventListener('click', () => {
        const tr = state.tracks[t];
        if (kind === 'r') { tr.rx = tr.rx ? 0 : 1; state.g[G.XMSK] = rMask(state); send(['g', G.XMSK, state.g[G.XMSK]]); }
        else { tr.q = tr.q === 0 ? 1 : 0; send(['qtrk', t, tr.q]); }
        touched(); renderChips();
      });
      cell.appendChild(b);
    }
    host.appendChild(cell);
  }
}
function renderChips() {
  const qOn = quantOn();
  document.querySelectorAll('#tsub .tchip.r').forEach((b, t) => { const on = !state.tracks[t].rx; b.classList.toggle('on', on); b.setAttribute('aria-pressed', String(on)); });
  const f = qFollow();
  document.querySelectorAll('#tsub .tchip.q').forEach((b, t) => {
    const on = state.tracks[t].q !== 0, src = t === f;
    b.classList.toggle('on', on && !src); b.classList.toggle('idle', !qOn); b.classList.toggle('src', src); b.setAttribute('aria-pressed', String(on));
    b.disabled = src;
    b.title = src ? `T${t + 1}: the quantizer follows its chords (it builds them in the scale, never on its own)` : `T${t + 1}: follows the quantizer`;
  });
}

// ------------------------------------------------------------------ preset bar and browser
ui.pfilter = 'ALL';
const esc = t => String(t).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);
function renderPresets() {
  const m = curMachine(), e = trackPreset(ui.t), mod = !ui.lock && presetModified(ui.t);
  const bar = $('pbar');
  bar.classList.toggle('dis', ui.all);
  bar.classList.toggle('lk', !!ui.lock);
  $('ptag').textContent = ui.lock ? 'LOCK' : e ? e.tag : '—';
  $('ptxt').textContent = ui.lock ? (ui.plockName ? ui.plockName + ' → STEP ' : 'SOUND LOCK → STEP ') + [...ui.lock].sort((a, b) => a - b).map(s => s + 1).join(' ')
    : e ? e.name : MACH[m].long;
  $('pmod').textContent = mod ? '•' : '';
  $('pmod').title = mod ? 'edited since the preset' : '';
  const panel = $('ppanel');
  panel.hidden = !ui.ppanel;
  $('pname').setAttribute('aria-expanded', ui.ppanel ? 'true' : 'false');
  if (!ui.ppanel) return;
  const list = presetEntries(m);
  $('ptitle').textContent = MACH[m].id + ' · PRESETS';
  if (!ui.pmsg) $('psub').textContent = ui.lock ? 'tap = sound lock on the selected steps' : (list.length - 1) + ' sounds · ★ = what the machine starts with';
  // filter chips: every tag of this machine, USER first when present
  const tags = ['ALL', ...new Set(list.filter(x => x.kind !== 'init').map(x => x.tag))];
  if (!tags.includes(ui.pfilter)) ui.pfilter = 'ALL';
  $('ptags').innerHTML = tags.map(t => `<button class="ptchip${t === ui.pfilter ? ' on' : ''}" data-t="${t}" title="${t === 'ALL' ? 'All' : t === 'USER' ? 'My presets' : (PR.TAGS[t] || t)}">${t}</button>`).join('');
  $('ptags').querySelectorAll('button').forEach(b => { b.onclick = () => { ui.pfilter = b.dataset.t; renderPresets(); }; });
  const shown = list.filter(x => ui.pfilter === 'ALL' || x.tag === ui.pfilter || x.kind === 'init');
  $('plist').innerHTML = shown.map((x, i) => {
    const sel = sameEntry(x, e) && !ui.lock, def = isDefault(m, x);
    return `<button class="pitem${sel ? ' sel' : ''}${x.kind === 'u' ? ' user' : ''}" data-i="${i}" title="${x.kind === 'f' ? esc(PR.TAGS[x.tag] || '') : ''}">`
      + `<span class="pit">${esc(x.tag)}${def ? '<b class="pdef" aria-label="default"> ★</b>' : ''}</span><span class="pin">${esc(x.name)}</span></button>`;
  }).join('');
  $('plist').querySelectorAll('.pitem').forEach(b => { b.onclick = () => { ui.prename = null; ui.pmsg = ''; loadPreset(m, shown[+b.dataset.i]); }; });
  // what can be done with the preset the track plays: make it the machine's default; a user preset: update it with the
  // sound as it is now, rename it, delete it
  const acts = $('pacts');
  acts.hidden = !!ui.lock || !e;
  if (!ui.lock && e) {
    const def = isDefault(m, e), user = e.kind === 'u';
    acts.innerHTML = `<span class="pacur">${esc(e.name)}</span>`
      + `<button class="btn pdefb${def ? ' on' : ''}" id="pdefb" aria-pressed="${def}" title="${def ? 'The machine starts with this sound' : 'Start the machine with this sound (INIT, the machine picker)'}">${def ? '★' : '☆'} Default</button>`
      + (user ? `<button class="btn" id="pupd"${mod ? '' : ' disabled'} title="Save the sound as it is now in this preset">Update</button>`
              + `<button class="btn${ui.prename ? ' on' : ''}" id="pren" aria-pressed="${!!ui.prename}">Rename</button>`
              + `<button class="btn" id="pdel">Delete</button>` : '');
    $('pdefb').onclick = () => { setDefaultSound(m, def ? null : e); ui.pmsg = def ? 'back to the clean INIT' : '“' + e.name + '” is the default'; flashSub('psub'); renderPresets(); };
    if (user) {
      $('pupd').onclick = () => { updatePreset(e.ref); ui.pmsg = '“' + e.name + '” updated'; flashSub('psub'); renderPresets(); };
      $('pren').onclick = () => {
        ui.prename = ui.prename ? null : e.ref;
        $('pinput').value = ui.prename ? e.name : ''; renderPresets();
        if (ui.prename) { $('pinput').focus(); $('pinput').select(); }
      };
      armed($('pdel'), 'Delete', () => { deletePreset(e.ref); ui.prename = null; ui.pmsg = '“' + e.name + '” deleted'; flashSub('psub'); renderAll(); });
    }
  }
  $('psavebtn').textContent = ui.prename ? 'Rename' : 'Save';
  $('pinput').placeholder = ui.prename ? 'New name' : 'Preset name';
}
// the panel's subtitle tells what just happened, for a moment
function flashSub(id) {
  $(id).textContent = id === 'psub' ? ui.pmsg : ui.kmsg;
  clearTimeout(flashSub[id]);
  flashSub[id] = setTimeout(() => { if (id === 'psub') { ui.pmsg = ''; renderPresets(); } else { ui.kmsg = ''; renderKits(); } }, 2500);
}
function buildPresets() {
  $('pname').addEventListener('click', () => { if (ui.all) return; ui.ppanel = !ui.ppanel; ui.prename = null; if (ui.ppanel) { presetStore.connect(); ui.lpanel = false; ui.rpanel = false; renderLoops(); renderRadioPanel(); } renderPresets(); });
  $('pprev').addEventListener('click', () => { if (!ui.all) stepPreset(-1); });
  $('pnext').addEventListener('click', () => { if (!ui.all) stepPreset(1); });
  $('pclose').addEventListener('click', () => { ui.ppanel = false; ui.prename = null; renderPresets(); });
  const doSave = () => {
    presetStore.connect();
    if (ui.prename) {
      const ref = ui.prename, n = renamePreset(ref, $('pinput').value);
      if (n === null) { ui.pmsg = 'a preset of this machine has that name'; flashSub('psub'); return; }
      ui.prename = null; $('pinput').value = ''; ui.pmsg = 'renamed “' + n + '”'; flashSub('psub'); renderAll();
      return;
    }
    const name = savePreset($('pinput').value);
    $('pinput').value = '';
    ui.pfilter = 'ALL';
    ui.pmsg = '“' + name + '” saved'; flashSub('psub');
    renderPresets(); renderAll();
  };
  $('psavebtn').addEventListener('click', doSave);
  $('pinput').addEventListener('keydown', e => {
    if (e.key === 'Enter') { e.preventDefault(); doSave(); }
    else if (e.key === 'Escape' && ui.prename) { e.preventDefault(); ui.prename = null; $('pinput').value = ''; renderPresets(); }
    e.stopPropagation();
  });
}

// the kits, in the project row: the list (★ = what Init starts from, PTN = saved with its pattern), a tap arms the
// load (Load?), a second one loads; the kit last loaded or saved can be made the default, updated, renamed, deleted
function renderKits() {
  const panel = $('kpanel');
  if (!panel) return;
  panel.hidden = !ui.kpanel;
  $('kitsbtn').setAttribute('aria-expanded', ui.kpanel ? 'true' : 'false');
  if (!ui.kpanel) return;
  if (!ui.kmsg) $('ksub').textContent = userKits.length ? userKits.length + ' kit' + (userKits.length > 1 ? 's' : '') + ' · ★ = what Init starts from' : 'none yet: save the 8 sounds and the master';
  const cur = userKits.find(k => k.name === ui.kit) || null;
  $('klist').innerHTML = userKits.map((x, i) => `<button class="pitem kitem${x === cur ? ' sel' : ''}${ui.karm === x.name ? ' armed' : ''}" data-i="${i}">`
    + `<span class="pit">${x.pat ? 'KIT + PTN' : 'KIT'}${defaults.kit === x.name ? '<b class="pdef" aria-label="default"> ★</b>' : ''}</span><span class="pin">${ui.karm === x.name ? 'LOAD?' : esc(x.name)}</span></button>`).join('')
    || '<p class="kempty">The kit holds the 8 tracks’ sounds, the master and the scale. With + PATTERN, the trigs and p-locks too.</p>';
  $('klist').querySelectorAll('.kitem').forEach(b => {
    b.onclick = () => {
      const x = userKits[+b.dataset.i];
      if (ui.karm !== x.name) { ui.karm = x.name; clearTimeout(renderKits.t); renderKits.t = setTimeout(() => { ui.karm = ''; renderKits(); }, 2500); renderKits(); return; }
      ui.karm = ''; ui.krename = null;
      if (loadKit(x)) { ui.kmsg = '“' + x.name + '” loaded'; flashSub('ksub'); }
      renderKits();
    };
  });
  const acts = $('kacts');
  acts.hidden = !cur;
  if (cur) {
    const def = defaults.kit === cur.name;
    acts.innerHTML = `<span class="pacur">${esc(cur.name)}</span>`
      + `<button class="btn pdefb${def ? ' on' : ''}" id="kdefb" aria-pressed="${def}" title="${def ? 'Init starts from this kit' : 'Start Init from this kit (its sounds, an empty pattern)'}">${def ? '★' : '☆'} Default</button>`
      + `<button class="btn" id="kupd" title="Save the project as it is now in this kit">Update</button>`
      + `<button class="btn${ui.krename ? ' on' : ''}" id="kren" aria-pressed="${!!ui.krename}">Rename</button>`
      + `<button class="btn" id="kdel">Delete</button>`;
    $('kdefb').onclick = () => { setDefaultKit(def ? '' : cur.name); ui.kmsg = def ? 'Init: the standard kit' : 'Init starts from “' + cur.name + '”'; flashSub('ksub'); renderKits(); };
    $('kupd').onclick = () => { saveKit(cur.name, !!cur.pat); ui.kmsg = '“' + cur.name + '” updated'; flashSub('ksub'); renderKits(); };
    $('kren').onclick = () => {
      ui.krename = ui.krename ? null : cur;
      $('kinput').value = ui.krename ? cur.name : ''; renderKits();
      if (ui.krename) { $('kinput').focus(); $('kinput').select(); }
    };
    armed($('kdel'), 'Delete', () => { deleteKit(cur); ui.krename = null; ui.kmsg = '“' + cur.name + '” deleted'; flashSub('ksub'); renderKits(); });
  }
  $('kpat').classList.toggle('on', !!ui.kpat); $('kpat').setAttribute('aria-pressed', String(!!ui.kpat));
  $('kpat').hidden = !!ui.krename;
  $('ksavebtn').textContent = ui.krename ? 'Rename' : 'Save';
  $('kinput').placeholder = ui.krename ? 'New name' : 'Kit name';
}
function buildKits() {
  $('kitsbtn').addEventListener('click', () => { ui.kpanel = !ui.kpanel; ui.krename = null; if (ui.kpanel) presetStore.connect(); setProj(false); renderKits(); });
  $('kclose').addEventListener('click', () => { ui.kpanel = false; ui.krename = null; renderKits(); });
  $('kpat').addEventListener('click', () => { ui.kpat = !ui.kpat; renderKits(); });
  const doSave = () => {
    presetStore.connect();
    if (ui.krename) {
      const n = renameKit(ui.krename, $('kinput').value);
      if (n === null) { ui.kmsg = 'a kit has that name'; flashSub('ksub'); return; }
      ui.krename = null; $('kinput').value = ''; ui.kmsg = 'renamed “' + n + '”'; flashSub('ksub'); renderKits();
      return;
    }
    const name = saveKit($('kinput').value, !!ui.kpat);
    $('kinput').value = '';
    ui.kmsg = '“' + name + '” saved' + (ui.kpat ? ' with its pattern' : ''); flashSub('ksub');
    renderKits();
  };
  $('ksavebtn').addEventListener('click', doSave);
  $('kinput').addEventListener('keydown', e => {
    if (e.key === 'Enter') { e.preventDefault(); doSave(); }
    else if (e.key === 'Escape' && ui.krename) { e.preventDefault(); ui.krename = null; $('kinput').value = ''; renderKits(); }
    e.stopPropagation();
  });
}

// ------------------------------------------------------------------ updates (v87): OPTION > Updates, and the banner
// The plugin and the app ask the project's releases on GitHub (Updater.cpp: once a session, or when asked) and fetch
// this system's installer into Downloads; the page shows where it stands (d.upd). In a browser the page is always the
// latest version: the panel says so and points to the plugin's downloads.
const SITE_URL = 'https://distortedimages-code.github.io/IMPAKT/';
function updSend(op, extra) { const b = window.__JUCE__ && window.__JUCE__.backend; if (b) b.emitEvent('impakt_update', Object.assign({ op }, extra || {})); }
function renderUpdates() {
  const u = ui.upd, panel = $('upanel');
  const avail = !!u && u.state === 3, busy = !!u && (u.state === 4 || u.state === 5);
  $('updbtn').classList.toggle('hasupd', avail || busy);
  // the banner: a newer IMPAKT (unless LATER was pressed on it), its download, where it went
  const toast = $('updtoast');
  const showToast = inPlugin && !!u && !ui.updToastOff && ((avail && !u.skipped) || busy);
  toast.hidden = !showToast;
  if (showToast) {
    $('updtxt').textContent = u.state === 4 ? 'DOWNLOADING IMPAKT ' + u.latest + ' · ' + Math.round(100 * (u.progress || 0)) + '%'
      : u.state === 5 ? 'IMPAKT ' + u.latest + ' IS IN YOUR DOWNLOADS' : 'IMPAKT ' + u.latest + ' IS OUT';
    $('updgo').hidden = u.state !== 3;
    $('updmore').textContent = u.state === 5 ? 'How to install' : 'What’s new';
    $('updlater').hidden = u.state === 4;
  }
  panel.hidden = !ui.upanel;
  $('updbtn').setAttribute('aria-expanded', ui.upanel ? 'true' : 'false');
  if (!ui.upanel) return;
  const cur = (u && u.cur) || IMPAKT_VERSION;
  $('usub').textContent = 'THIS IS ' + cur + (inPlugin ? (u && u.os === 'mac' ? ' · MAC' : u && u.os === 'win' ? ' · WINDOWS' : u && u.os === 'linux' ? ' · LINUX' : '') : ' · BROWSER');
  let stat = '', sub = '', notes = '', acts = '';
  const link = (href, label) => `<a class="btn" href="${href}" target="_blank" rel="noopener">${label}</a>`;
  if (!inPlugin) {
    stat = 'ALWAYS THE LATEST';
    sub = 'This page updates itself. The plugin (VST3, AU) and the app are on the downloads page, with how to install them.';
    acts = link(SITE_URL + 'install.html', 'Get the plugin') + link(SITE_URL + 'manuel.html', 'Manual');
  } else {
    const st = u ? u.state : 0;
    const how = !u ? '' : u.os === 'mac' ? 'The disk image is open: quit your music software, then run Install IMPAKT.'
      : u.os === 'win' ? 'Close your music software, then run the setup in your Downloads folder.'
      : 'Close your music software, unpack the archive in your Downloads folder and run install.sh.';
    if (st === 0) { stat = 'NOT CHECKED YET'; sub = u && !u.auto ? 'The check at start is off.' : ''; }
    else if (st === 1) stat = 'CHECKING…';
    else if (st === 2) { stat = 'UP TO DATE'; sub = u.latest ? 'The latest release is ' + u.latest + '.' : 'No release published yet.'; }
    else if (st === 3) { stat = 'IMPAKT ' + u.latest + ' IS OUT'; sub = u.msg || 'Download it here: it lands in your Downloads folder, nothing is replaced while your music software runs.'; notes = u.notes || ''; }
    else if (st === 4) { stat = 'DOWNLOADING ' + Math.round(100 * (u.progress || 0)) + '%'; sub = 'IMPAKT ' + u.latest; }
    else if (st === 5) { stat = 'DOWNLOADED'; sub = how; notes = u.notes || ''; }
    else if (st === 6) { stat = 'NO ANSWER'; sub = u.msg || ''; }
    if (st === 3) acts += '<button class="btn go" data-u="download">Download ' + esc(u.latest) + '</button>';
    if (st === 5) acts += '<button class="btn" data-u="show">Show the file</button>';
    if (st === 3 || st === 5) acts += '<button class="btn" data-u="page">Release page</button>';
    if (st !== 1 && st !== 4) acts += '<button class="btn" data-u="check">' + (st === 6 ? 'Try again' : 'Check now') + '</button>';
    acts += `<button class="btn ptog${u && u.auto ? ' on' : ''}" data-u="auto" aria-pressed="${u && u.auto ? 'true' : 'false'}" title="Ask once when IMPAKT opens">Check at start</button>`;
    acts += link(SITE_URL + 'install.html', 'Install guide');
  }
  $('ustat').textContent = stat;
  $('uline').textContent = sub; $('uline').hidden = !sub;
  const bar = $('ubar'); bar.hidden = !(u && u.state === 4); if (u) $('ubarv').style.width = Math.round(100 * (u.progress || 0)) + '%';
  $('unotes').textContent = notes; $('unotes').hidden = !notes;
  const a = $('uacts'); a.innerHTML = acts;
  a.querySelectorAll('[data-u]').forEach(b => b.addEventListener('click', () => {
    const op = b.dataset.u;
    if (op === 'auto') updSend('auto', { on: !(ui.upd && ui.upd.auto) });
    else updSend(op);
  }));
}
function buildUpdates() {
  $('updbtn').addEventListener('click', () => { ui.upanel = !ui.upanel; setProj(false); renderUpdates(); });
  $('uclose').addEventListener('click', () => { ui.upanel = false; renderUpdates(); });
  $('updgo').addEventListener('click', () => { updSend('download'); ui.upanel = true; renderUpdates(); });
  $('updmore').addEventListener('click', () => { ui.upanel = true; renderUpdates(); });
  $('updlater').addEventListener('click', () => { if (ui.upd && ui.upd.state === 3) updSend('later'); ui.updToastOff = true; renderUpdates(); });
  renderUpdates();
}

// ------------------------------------------------------------------ build DOM
const pads = [], knobs = [], steps = [];

// a pad plays its track at once (only while the sequence is stopped) and selects it on release; a swipe down mutes or
// unmutes the track instead
const MUTE_PX = 22;
function toggleMute(t) {
  const tr = state.tracks[t];
  tr.mute = tr.mute ? 0 : 1; send(['mute', t, tr.mute]); touched(); renderPads();
}
function selectTrack(t) {
  if (ui.t === t) return;
  dropLock(); ui.t = t; ui.picker = false;
  if (ui.lastP === RATP || ui.lastP === RNDP) ui.lastP = P.TUNE;
  if ((ui.spage | 0) * NS >= trk().len) ui.spage = pagesOf(trk()) - 1;   // (the page shown stays within the track's LEN)
  renderAll();
}
function buildPads() {
  const host = $('pads');
  for (let t = 0; t < NT; t++) {
    const b = document.createElement('button');
    b.className = 'pad';
    b.innerHTML = `<span class="n">${t + 1}</span><span class="m"></span>`;
    let pd = null;
    const end = (e, ok) => {
      if (!pd || pd.id !== e.pointerId) return;
      const d = pd; pd = null;
      b.classList.remove('drag'); b.style.removeProperty('--sy');
      if (d.aud) send(['rel', t, 0]);           // (DEC on HOLD: the note held while the pad was)
      if (ok && !d.muted) selectTrack(t);
    };
    b.addEventListener('pointerdown', e => {
      e.preventDefault();
      backend.ensureAudio();
      if (ui.ptn) { ui.bank = t; renderPads(); renderSteps(); renderLockbar(); return; }   // PTN: the pads are the banks
      if (ui.moveFrom != null) { moveTrack(ui.moveFrom, t); return; }                  // (v80: Move track: its new place)
      const aud = !ui.pos.run;
      if (aud) send(['aud', t, 1]);             // while the sequence runs, a pad only selects its track
      try { b.setPointerCapture(e.pointerId); } catch (err) {}
      pd = { id: e.pointerId, y: e.clientY, moved: false, muted: false, aud };
    });
    b.addEventListener('pointermove', e => {
      if (!pd || pd.id !== e.pointerId) return;
      const dy = e.clientY - pd.y;
      if (!pd.moved && dy < 3) return;
      pd.moved = true;
      b.classList.add('drag'); b.style.setProperty('--sy', clamp(dy, 0, 10) + 'px');
      if (!pd.muted && dy > MUTE_PX) { pd.muted = true; toggleMute(t); }
    });
    b.addEventListener('pointerup', e => end(e, true));
    b.addEventListener('pointercancel', e => end(e, false));
    b.addEventListener('keydown', e => {
      if ((e.key === 'Enter' || e.key === ' ') && ui.moveFrom != null) { e.preventDefault(); e.stopPropagation(); moveTrack(ui.moveFrom, t); return; }
      if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); e.stopPropagation(); if (ui.ptn) { ui.bank = t; renderPads(); renderSteps(); renderLockbar(); return; } backend.ensureAudio(); if (!ui.pos.run) send(['aud', t, 1]); selectTrack(t); }
      else if (e.key === 'm' || e.key === 'M') { e.preventDefault(); e.stopPropagation(); toggleMute(t); }
    });
    host.appendChild(b); pads.push(b);
  }
}

// MASTR: the master chain's pages, one at a time (EQ .. MIX); the other pages show their modules side by side
function buildTabs() {
  PAGES.forEach((pg, i) => {
    if (!MASTER_PAGES.includes(pg)) return;
    const b = document.createElement('button');
    b.className = 'tab'; b.dataset.i = i; b.innerHTML = `${pg.id}<span class="dot"></span>`;
    b.addEventListener('click', () => { ui.gpage = i; ui.wpick = false; renderAll(); });
    $('gtabs').appendChild(b);
  });
}
// the modules of TONE, MOD, FX and OUT on MASTR: each a section with its name, its knobs (a bank of its own: P:<page>)
// and, for REVERB and DELAY, their screen; TONE's MORE key in its header
function buildModules() {
  for (const tab in KMODS) {
    const host = $('kp-' + tab);
    for (const [id, name] of KMODS[tab]) {
      const pg = PAGE_BY_ID[id], sec = document.createElement('section');
      sec.className = 'mod kmod' + (pg.glob ? ' gmod' : '') + ' km-' + id.toLowerCase(); sec.id = 'km-' + id; sec.dataset.pg = id;
      sec.setAttribute('aria-label', name);
      const more = MORE_PAGES[id] ? `<button class="kmore" id="kmore-${id}" aria-pressed="false" title="${id}'s MORE page: SPRD, the track wider and its middle out of the centre; MONO, under it the track stays centred">MORE</button>` : '';
      const ret = RET_IDS.includes(id), tg = tgOf(id);
      const strip = ret ? `<div class="gstrip kstrip" id="ks-${id}"></div>` : '';
      // a module of a tab group: its name among the others', a tab each (the one shown reversed; its pixel)
      const head = tg ? `<div class="rtabs${tg.strike ? '' : ' nostrike'}" role="tablist" aria-label="${tg.label}">${tg.ids.map(r => `<button class="rtab${r === id ? ' cur' : ''}" role="tab" aria-selected="${r === id}" data-ret="${r}"><i></i>${TG_NAME[r]}</button>`).join('')}</div>`
        : `<h2 class="silk">${name}</h2>`;
      if (tg) sec.classList.add('kret');
      sec.innerHTML = `<div class="mhd">${head}<span class="kdot" aria-hidden="true"></span>${more}</div>${strip}<div class="knobs kn${pg.ps.length}" id="k-${id}" data-vlab="${name}"></div>`;
      host.appendChild(sec);
      buildKnobs('P:' + id, 'k-' + id, pg.ps.length);
      if (more) $('kmore-' + id).addEventListener('click', () => setToneMore(!ui.tonemore));
      if (tg) {
        sec.querySelectorAll('.rtab').forEach(b => b.addEventListener('click', () => setRet(b.dataset.ret)));
        // (the arrows move along the tabs, as in any tab list)
        sec.querySelector('.rtabs').addEventListener('keydown', e => {
          const k = e.key === 'ArrowRight' ? 1 : e.key === 'ArrowLeft' ? -1 : e.key === 'Home' ? -99 : e.key === 'End' ? 99 : 0;
          if (!k) return;
          e.preventDefault();
          const i = tg.ids.indexOf(tgCur(tg)), j = Math.abs(k) > 1 ? (k < 0 ? 0 : tg.ids.length - 1) : (i + k + tg.ids.length) % tg.ids.length;
          setRet(tg.ids[j], true);
        });
      }
    }
  }
}
// the module shown in its tab group (v68: the FX page's return; v69: the MOD page's modulator, which the track's
// screen then shows: an LFO, or that follower)
function setRet(id, focus) {
  const g = tgOf(id);
  if (!g || tgCur(g) === id) return;
  ui[g.key] = id;
  const pg = PAGE_BY_ID[id];
  if (pg && !pg.glob) ui.page = PAGES.indexOf(pg);
  renderAll();
  const b = document.querySelector(`#km-${id} .rtab[data-ret="${id}"]`);
  if (b && (focus || (document.activeElement && document.activeElement.classList.contains('rtab')))) b.focus();
}

const A0 = 135, SWEEP = 270;
const polar = (r, a) => [32 + r * Math.cos(a * Math.PI / 180), 32 + r * Math.sin(a * Math.PI / 180)];

// the silkscreen around each encoder, printed on the panel: a tick per position on a stepped control (11 on a
// smooth one), the range written at both ends and, on a centred or odd-stepped control, its middle. The text
// comes from the control's own display (so it follows the machine, the algorithm, the model), shortened to fit.
const SCL_VB = '-15 -10 94 76', SCL_R0 = 27.5, SCL_R1 = 30, SCL_R2 = 31.8;
function sclText(v) {
  if (v === undefined || v === null) return '';
  let s = String(v).trim()
    .replace(/(\.\d*?)0+(?=\D|$)/g, '$1').replace(/\.(?=\D|$)/g, '')       // 20.0k -> 20k, ×0.60 -> ×0.6
    .replace(/^\+0(?=\D|$)/, '0');                                         // +0dB -> 0dB
  if (s.length > 5) s = s.replace(/(\d+)\.\d+/, '$1');                     // 8.15ms -> 8ms
  if (s.length > 7) s = s.replace(/\s+/g, '');
  return s.length > 7 ? s.slice(0, 6) + '.' : s;
}
function knobScale(meta, bip) {
  const n = meta.n || 0, stepped = n >= 2, fine = stepped && n <= 32;
  const ticks = [];
  for (let i = 0, c = fine ? n : 11; i < c; i++) ticks.push(fine ? mid(i, n) : i / 10);
  const at = v => { try { return sclText(meta.f(v)); } catch (e) { return ''; } };
  const lo = stepped ? mid(0, n) : 0, hi = stepped ? mid(n - 1, n) : 0.9999;
  // a control can print its own legend: [low end, high end, middle]
  const labels = meta.sc ? [[lo, meta.sc[0]], [hi, meta.sc[1]]].concat(meta.sc[2] ? [[0.5, meta.sc[2]]] : [])
    : [[lo, at(lo)], [hi, at(hi)]].concat(bip || (stepped && n % 2 === 1 && n >= 3 && n <= 9) ? [[0.5, at(0.5)]] : []);
  return { ticks, labels, major: [fine ? lo : 0, fine ? hi : 1].concat(bip || (meta.sc && meta.sc[2]) ? [0.5] : []) };
}
function renderScale(K, meta, bip) {
  const sc = knobScale(meta, bip);
  const key = sc.labels.map(l => l[1]).join('|') + '|' + sc.ticks.length + (bip ? 'b' : '');
  if (K.sclKey === key) return;
  K.sclKey = key;
  const ang = v => A0 + SWEEP * clamp(v, 0, 1);
  const isMajor = v => sc.major.some(m => Math.abs(m - v) < 1e-6);
  let h = '';
  for (const v of sc.ticks) {
    const [x1, y1] = polar(SCL_R0, ang(v)), [x2, y2] = polar(isMajor(v) ? SCL_R2 : SCL_R1, ang(v));
    h += `<line class="${isMajor(v) ? 'mj' : ''}" x1="${x1.toFixed(2)}" y1="${y1.toFixed(2)}" x2="${x2.toFixed(2)}" y2="${y2.toFixed(2)}"/>`;
  }
  for (const [v, txt] of sc.labels) {
    if (!txt) continue;
    const a = ang(v), c = Math.cos(a * Math.PI / 180), sn = Math.sin(a * Math.PI / 180);
    let [x, y] = polar(SCL_R2 + 2, a);
    const anchor = c < -0.3 ? 'end' : c > 0.3 ? 'start' : 'middle';
    y += sn > 0.3 ? 7.5 : sn < -0.3 ? -1.5 : 3;
    const esc = txt.replace(/&/g, '&amp;').replace(/</g, '&lt;');
    h += `<text x="${x.toFixed(2)}" y="${y.toFixed(2)}" text-anchor="${anchor}">${esc}</text>`;
  }
  K.scl.innerHTML = h;
}
function arcPath(r, a0, a1) {
  const [x0, y0] = polar(r, a0), [x1, y1] = polar(r, a1);
  const large = Math.abs(a1 - a0) > 180 ? 1 : 0, sweep = a1 >= a0 ? 1 : 0;
  return `M${x0.toFixed(2)} ${y0.toFixed(2)} A${r} ${r} 0 ${large} ${sweep} ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}

// the banks of knobs: the machine's (SYN, beside the LCD), the TRIG row, the master page shown (MASTR), and one per
// module (P:FLT, P:LFO1, P:REV...)
const banks = { m: [], r: [], g: [] };
const TRG_PAGE = PAGES.findIndex(p => p.seq);
const bankPage = bank => {
  if (bank === 'wm' || bank === 'wf') { const pg = WS_PAGES[wsPage()]; return bank === 'wm' ? pg.mainPg : pg.finePg; }
  if (bank[0] === 'P') { const pm = PAGE_BY_ID[bank.slice(2)]; return MORE_PAGES[pm.id] && moreOn(pm.id) ? MORE_PAGES[pm.id] : pm; }
  const pg = PAGES[bank === 'm' ? 0 : bank === 'r' ? TRG_PAGE : ui.gpage];
  if (bank === 'r' && ui.tmore) return TRG_MORE_PAGE;
  if (bank === 'm' && recSide()) return RAM_REC_PAGE;
  if (bank === 'm' && prismSide()) return prismPage() === 'OSC' ? RAM_OSC_PAGE : RAM_SPEC_PAGE;
  if (bank === 'm' && sidePage()) return sidePage();
  if (bank === 'm' && brkChop()) return BRK_CHOP_PAGE;
  if (bank === 'm' && fuzzSub()) return FUZZ_SUB_PAGE;
  if (bank === 'm' && fuzzMain()) return FUZZ_MAIN_PAGE;
  if (bank === 'm' && stcSpace()) return STC_SPACE_PAGE;
  if (bank === 'm' && fxMore()) return curMachine() === FUZZ_M ? FUZZ_MORE_PAGE : curMachine() === WEFT_M ? WEFT_MORE_PAGE : curMachine() === STENCIL_M ? STC_MORE_PAGE : FX_MORE_PAGE;
  return pg.id === 'GRN' && ui.gmod ? GRN_MOD_PAGE : pg; };
function buildKnobs(bank, hostId, n) {
  const host = $(hostId);
  const dots = '<circle r="2.6"/>'.repeat(NT);
  if (!banks[bank]) banks[bank] = [];
  for (let i = 0; i < (n || 8); i++) {
    const el = document.createElement('div');
    el.className = 'knob'; el.tabIndex = 0; el.setAttribute('role', 'slider');
    // (v79: the ring where a CV takes it; WEST's banks, w: a slider, a stage's pulse key under it)
    const wb = bank[0] === 'w';
    el.innerHTML = `<div class="face"><svg viewBox="${SCL_VB}" aria-hidden="true"><g class="scl"></g><path class="trk" d="${arcPath(24, A0, A0 + SWEEP)}"/><path class="gho"/><path class="val"/><g class="dots">${dots}</g><circle class="knurl" cx="32" cy="32" r="16.1"/><circle class="cap" cx="32" cy="32" r="15"/><line class="ind" x1="32" y1="32" x2="32" y2="20"/><circle class="ghd" cx="32" cy="32" r="0"/></svg><div class="sw" aria-hidden="true"></div>`
      + (wb ? '<div class="fad" aria-hidden="true"><i class="rl"></i><i class="gh"></i><i class="fl"></i><b class="cp"></b><s class="wsl"></s></div>' : '')
      + `</div><span class="kl"><span class="lt"></span><i></i></span><span class="kv"></span>` + (wb ? '<u class="spk" aria-hidden="true"></u>' : '');
    const K = { el, i, bank, val: el.querySelector('.val'), ind: el.querySelector('.ind'), dots: [...el.querySelectorAll('.dots circle')], scl: el.querySelector('.scl'), sclKey: '',
      lt: el.querySelector('.lt'), kv: el.querySelector('.kv'), sw: el.querySelector('.sw'), swKey: '', drag: null, lastTap: 0,
      gho: el.querySelector('.gho'), ghd: el.querySelector('.ghd'), fad: el.querySelector('.fad'), spk: el.querySelector('.spk') };
    el.addEventListener('pointerdown', e => knobDown(K, e));
    el.addEventListener('focus', () => { if (bank[0] === 'P' && !PAGE_BY_ID[bank.slice(2)].glob && ui.page !== PAGES.indexOf(PAGE_BY_ID[bank.slice(2)])) { focusModule(bank); renderTStrip(); } });
    el.addEventListener('pointermove', e => knobMove(K, e));
    el.addEventListener('pointerup', e => knobUp(K, e));
    el.addEventListener('pointercancel', e => knobUp(K, e));
    el.addEventListener('keydown', e => {
      const tgt0 = knobTarget(K);
      if (!tgt0 || tgt0.meta.off) return;
      if ((e.key === 'Enter' || e.key === ' ') && isSwitch(tgt0.meta)) {
        e.preventDefault();
        swSelect(tgt0, (idx(tgt0.cur, tgt0.meta.n) + 1) % tgt0.meta.n);
        return;
      }
      // switches: right / down = the next position (they read left to right, top to bottom)
      const sw = isSwitch(tgt0.meta);
      const d = e.key === 'ArrowUp' ? (sw ? -1 : 1) : e.key === 'ArrowRight' ? 1 : e.key === 'ArrowDown' ? (sw ? 1 : -1) : e.key === 'ArrowLeft' ? -1 : 0;
      if (!d) return; e.preventDefault();
      const tgt = tgt0;
      if (sw) { swSelect(tgt, clamp(idx(tgt.cur, tgt.meta.n) + d, 0, tgt.meta.n - 1)); return; }
      const step = tgt.meta.n ? 1 / tgt.meta.n : (e.shiftKey ? 0.002 : 1 / 127);
      if (tgt.all) setAllParam(tgt.p, state.tracks.map(tr => tr.p[tgt.p]), d * step);
      else applyKnob(tgt, tgt.cur + d * step);
      refreshAfterKnob(tgt);
    });
    host.appendChild(el); knobs.push(K); banks[bank].push(K);
  }
}

// the track's screen follows the module last touched (TONE's curve, the filter's cutoff, the LFOs, a follower, the BOX)
function focusModule(bank) {
  if (bank[0] !== 'P') return;
  const pg = PAGE_BY_ID[bank.slice(2)];
  if (pg && !pg.glob) ui.page = PAGES.indexOf(pg);
}
function knobTarget(K) {
  const pg = bankPage(K.bank), p = pg.ps[K.i];
  if (p == null) return { glob: true, p: -1, meta: OFF_K, cur: 0 };
  if (pg.glob) return { glob: true, p, meta: gMeta(p), cur: state.g[p] };
  const sv = shownValue(p), all = ui.all && !ui.lock;
  let meta = paramMeta(ui.t, p);
  // locked in ALL when no track can move with it (a selector on one machine leaves the others free: its dot goes)
  if (all && state.tracks.every((t, i) => allSkips(i, p))) meta = { l: meta.l, f: () => 'LOCK', off: true, alllock: true };
  return { glob: false, p, meta, cur: sv.v, locked: sv.locked, all };
}
function applyKnob(tgt, v) {
  if (tgt.glob) setGlobal(tgt.p, v); else setTrackParam(tgt.p, v);
}
function refreshAfterKnob(tgt) {
  ui.lastP = tgt.glob ? -1 - tgt.p : tgt.p;
  renderKnobs(); renderScreen(); renderSteps(); renderTabs();
  if (tgt.all && $('allrev') && $('allrev').disabled === !allChanged()) renderLockbar();   // REVERT has something to do
  if (tgt.p === P.IR || tgt.all) renderPads();
  if (tgt.glob && (tgt.p === G.DPOS || tgt.p === G.RALG || tgt.p === G.CHMD || tgt.p === G.PRTE)) { renderGStrip(); renderFxStrips(); }
}
function knobDown(K, e) {
  // on a touch screen only the dial (or the switch) turns: a drag on the label or the value scrolls the page
  if (e.pointerType && e.pointerType !== 'mouse' && !(e.target.closest && e.target.closest('svg, .sw, .fad, .spk'))) return;
  e.preventDefault();
  backend.ensureAudio();
  const tgt = knobTarget(K); if (!tgt || tgt.meta.off) return;
  // v79: the key under a stage: its stage pulse on or off (SPLS, a bit a stage)
  if (K.spk && tgt.meta.stage != null && e.target.closest && e.target.closest('.spk')) {
    const q = SQ(49), m = idx(shownValue(q).v, 32) ^ (1 << tgt.meta.stage);
    setTrackParam(q, mid(m, 32)); ui.lastP = q; renderKnobs(); renderScreen(); renderSteps(); renderTabs();
    return;
  }
  focusModule(K.bank);
  const seg = e.target.closest && e.target.closest('.sw b');
  try { K.el.setPointerCapture(e.pointerId); } catch (err) {}
  K.drag = { y: e.clientY, x: e.clientX, base: tgt.cur, moved: false, tgt, bases: tgt.all ? state.tracks.map(tr => tr.p[tgt.p]) : null,
    opt: seg && isSwitch(tgt.meta) ? +seg.dataset.i : -1,
    fdH: K.fad && K.el.classList.contains('isfd') ? Math.max(30, K.fad.querySelector('.rl').getBoundingClientRect().height) : 0 };
  ui.ratStep = -1;
  ui.lastP = tgt.glob ? -1 - tgt.p : tgt.p;
  ui.recHold = !tgt.glob && !tgt.all && !ui.lock ? tgt.p : null; ui.recStep = -1;
  K.el.classList.add('active');
  renderScreen();
}
function knobMove(K, e) {
  if (!K.drag) return;
  const sw = isSwitch(K.drag.tgt.meta), fd = K.drag.fdH;
  const dy = (K.drag.y - e.clientY) * (sw ? -1 : 1) + (fd ? 0 : (e.clientX - K.drag.x) * 0.25);
  if (!K.drag.moved && Math.abs(dy) < 3) return;
  K.drag.moved = true;
  const n = K.drag.tgt.meta.n;
  // (a slider: its cap under the finger)
  const sens = fd ? (e.shiftKey && !ui.lock ? 0.2 : 1) / fd : sw ? 1 / (n * 24) : n ? Math.min(1 / 180, 1 / (n * 16)) : (e.shiftKey && !ui.lock ? 1 / 900 : 1 / 180);
  const raw = K.drag.base + dy * sens;
  if (K.drag.bases) setAllParam(K.drag.tgt.p, K.drag.bases, raw - K.drag.base);
  else applyKnob(K.drag.tgt, raw);
  refreshAfterKnob(K.drag.tgt);
}
function knobUp(K) {
  if (!K.drag) return;
  const d = K.drag; K.drag = null;
  K.el.classList.remove('active');
  ui.recHold = null;
  if (d.moved) return;
  const tgt = d.tgt;
  const now = performance.now();
  if (isSwitch(tgt.meta)) {
    const n = tgt.meta.n, cur = idx(tgt.cur, n);
    // pressed beside the positions: step to the next one
    const opt = d.opt >= 0 ? d.opt : (cur + 1) % n;
    if (opt !== cur || tgt.all || (!tgt.glob && ui.lock && !tgt.locked)) { K.lastTap = 0; swSelect(tgt, opt); return; }
    // the lit position, tapped twice: reset (or remove the lock)
    if (now - K.lastTap >= 320) { K.lastTap = now; return; }
  }
  if (now - K.lastTap < 320) {
    K.lastTap = 0;
    if (!tgt.glob && ui.lock) clearParamLock(tgt.p);
    else if (tgt.glob) setGlobal(tgt.p, gDefault(tgt.p));
    else if (tgt.all) {
      for (let t = 0; t < NT; t++) {
        if (allSkips(t, tgt.p)) continue;
        const v = defaultParams(mof(state.tracks[t].p[P.MACH]))[tgt.p];
        state.tracks[t].p[tgt.p] = v; send(['p', t, tgt.p, v]);
      }
      touched();
    } else setTrackParam(tgt.p, defaultParams(mof(trk().p[P.MACH]))[tgt.p]);
    renderAll();
  } else K.lastTap = now;
}

function swSelect(tgt, i) {
  const v = mid(i, tgt.meta.n);
  if (tgt.all) setAllParam(tgt.p, state.tracks.map(() => v), 0); else applyKnob(tgt, v);
  refreshAfterKnob(tgt);
}

// steps: tap toggles, drag up / down splits the step into ratchets, double-tap (or Shift + click) brings the step up
// for p-locks. A step that is up: tap = trig / change of the sound only, swipe down = back down
const RAT_PX = 14, LOWER_PX = 18;
const tap = { s: -1, n: 0, t: 0, orig: 0 };
let sdrag = null;
function showRatchet(s) { ui.lastP = RATP; ui.ratStep = s; renderScreen(); }
// the 16 keys show a page of the track (Elektron style: 4 pages of 16, PAGE keys by LEN): key i is step stepAt(i). In PTN
// mode they are the 16 patterns of the bank shown; in KEY mode, notes
const stepAt = i => (ui.spage || 0) * NS + i;
function buildSteps() {
  const host = $('steps');
  let grp = null;
  for (let i = 0; i < NS; i++) {
    if (i % 4 === 0) { grp = document.createElement('div'); grp.className = 'sgrp'; host.appendChild(grp); }
    const b = document.createElement('button');
    b.className = 'step' + (i % 4 === 0 ? ' q' : '');
    b.innerHTML = `<span class="rt" aria-hidden="true"></span><span class="sn">${i + 1}</span>`;
    b.addEventListener('pointerdown', e => {
      e.preventDefault();
      backend.ensureAudio();
      if (ui.ptn) { patKey(i); return; }
      if (ui.key) { b.classList.add('press'); b.dataset.kn = String(keyPlay(i)); return; }
      // a PAGE key held: the track ends on this step
      if (ui.pageHold != null) { setLen(ui.pageHold * NS + i + 1); ui.pageHeldUsed = true; return; }
      const s = stepAt(i);
      if (e.shiftKey) { tap.s = -1; toggleLift(s); return; }
      try { b.setPointerCapture(e.pointerId); } catch (err) {}
      // a step that is up: decided on release (a tap) or by a swipe down
      if (ui.lock && ui.lock.has(s)) { tap.s = -1; sdrag = { s, id: e.pointerId, y: e.clientY, up: true, moved: false }; return; }
      const now = performance.now();
      if (tap.s === s && now - tap.t < 380) tap.n++;
      else { tap.s = s; tap.n = 1; tap.orig = layerOn(s); }
      tap.t = now;
      if (tap.n >= 2) {
        // the second tap: the trig goes back to what it was, the step comes up
        tap.n = 0; tap.s = -1; sdrag = null;
        if (layerOn(s) !== tap.orig) setLayer(s, tap.orig);
        liftStep(s);
        return;
      }
      // a new trig is set right away; switching one off waits for the release (it may become a drag)
      const wasOn = layerOn(s);
      if (!wasOn) setLayer(s, 1);
      sdrag = { s, id: e.pointerId, y: e.clientY, base: trk().steps[s].r || 1, moved: false, pendingOff: wasOn };
      b.classList.add('press');
      renderSteps(); renderTabs(); renderKnobs();
    });
    b.addEventListener('pointermove', e => {
      if (!sdrag || sdrag.id !== e.pointerId) return;
      const s = sdrag.s;
      if (stepAt(i) !== s) return;
      const dy = sdrag.y - e.clientY;
      if (sdrag.up) {
        if (!sdrag.moved && dy < -LOWER_PX) { sdrag.moved = true; lowerStep(s); }
        else if (!sdrag.moved && Math.abs(dy) > LOWER_PX) sdrag.moved = true;   // pushed up: nothing
        return;
      }
      if (recSide()) return;                     // REC trigs have no ratchet
      if (!sdrag.moved && Math.abs(dy) < 8) return;
      if (!sdrag.moved) {
        sdrag.moved = true; sdrag.pendingOff = false; tap.s = -1;
        if (!trk().steps[s].on) setTrig(s, 1);
      }
      setRatchet(s, sdrag.base + Math.round(dy / RAT_PX));
      showRatchet(s);
      renderSteps();
    });
    const release = e => {
      if (ui.key) { b.classList.remove('press'); if (b.dataset.kn) { const n = +b.dataset.kn; delete b.dataset.kn; if (Number.isFinite(n)) send(['rel', ui.t, n]); } }
      if (!sdrag || sdrag.s !== stepAt(i)) return;
      const d = sdrag, s = d.s; sdrag = null;
      b.classList.remove('press');
      if (d.up) { if (!d.moved && e.type === 'pointerup' && ui.lock && ui.lock.has(s)) flipStepKind(s); return; }
      if (!d.moved && d.pendingOff && e.type === 'pointerup') setLayer(s, 0);
      renderSteps(); renderTabs(); renderKnobs();
    };
    b.addEventListener('pointerup', release);
    b.addEventListener('pointercancel', release);
    b.addEventListener('pointerleave', () => { if (ui.key) b.classList.remove('press'); });
    b.addEventListener('keydown', e => {
      if (ui.ptn) { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); patKey(i); } return; }
      if (ui.key) { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); keyPlay(i); } return; }
      const s = stepAt(i);
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        if (e.shiftKey) toggleLift(s);
        else if (ui.lock && ui.lock.has(s)) flipStepKind(s);
        else { setLayer(s, layerOn(s) ? 0 : 1); renderSteps(); }
      } else if (e.key === 'ArrowDown' && ui.lock && ui.lock.has(s)) { e.preventDefault(); lowerStep(s); }
      else if ((e.key === 'ArrowUp' || e.key === 'ArrowDown') && !recSide()) {
        e.preventDefault();
        if (!trk().steps[s].on) setTrig(s, 1);
        setRatchet(s, (trk().steps[s].r || 1) + (e.key === 'ArrowUp' ? 1 : -1));
        showRatchet(s); renderSteps();
      }
    });
    grp.appendChild(b); steps.push(b);
  }
}

// the picker shows the machines by family; < > on the LCD step through them in the same order
// six families, by what the machine does in a track (v48: ten before)
const GROUPS = [
  ['KICK', ['BD·VA', 'BD·FM', 'HEX·BD', 'HARD', 'SINE', 'FM·Z']],
  ['SNARE + PERC', ['SD·VA', 'SD·FM', 'CP·NZ', 'HEX·SD', 'HEX·TM']],
  ['METAL + NOISE', ['HH·MT', 'ALLOY', 'ITERA', 'DATA', 'NOISE', 'IMPLS', 'BYTE']],
  ['LOOP + TEXTURE', ['CHIMERA', 'BREAK', 'RAM', 'RADIO', 'µLOOP', 'FRACT', 'BUG', 'CHAOS']],
  ['SYNTH', ['SY·RAW', 'TRIO', 'CZ·PD', 'FM·X', 'VOX', 'MACRO', 'CUBE', 'WEST']],
  ['CHORDS + VOICE', ['HALO', 'STRUM', 'CHOIR', 'HOWL', 'MANTRA']],
  ['FX', ['VERB', 'ECHO', 'BEND', 'TAPE', 'CODEC', 'FUZZ', 'WEFT', 'STENCIL']],
];
const ORDER = GROUPS.flatMap(([, ids]) => ids.map(id => MACH.findIndex(m => m.id === id)));
const stepMachine = d => ORDER[(ORDER.indexOf(curMachine()) + d + NM) % NM];
function buildPicker() {
  const host = $('mpick');
  // the plugin lays the families out in two columns (the whole list on its 16:9 panel): the first family of the
  // second column, and how many rows a column has
  const rows = Math.ceil(GROUPS.length / 2);
  host.style.setProperty('--prows', String(rows));
  GROUPS.forEach(([name, ids], gi) => {
    const g = document.createElement('div');
    g.className = 'pgrp' + (gi === rows ? ' ctop' : '');
    g.innerHTML = `<span class="pgl">${name}</span>`;
    ids.forEach(id => {
      const i = MACH.findIndex(m => m.id === id), m = MACH[i];
      const b = document.createElement('button');
      b.dataset.m = i;
      b.innerHTML = `<span class="pn">N°${String(i + 1).padStart(2, '0')}</span><span class="pmk">${logoSvg(i)}</span><span class="pid">${m.id}</span><span class="pk">${KANA[i]}</span>`;
      b.setAttribute('aria-label', m.id + ' — ' + m.long);
      b.addEventListener('click', e => { e.stopPropagation(); ui.picker = false; changeMachine(i); });
      g.appendChild(b);
    });
    host.appendChild(g);
  });
}

// ------------------------------------------------------------------ render
function curMachine() {
  const tr = trk();
  if (ui.lock) { const s = Math.min(...ui.lock); if (P.MACH in tr.steps[s].l) return mof(tr.steps[s].l[P.MACH]); }
  return mof(tr.p[P.MACH]);
}
// the FX track each track goes into (an insert takes it from the mix), -1: none (the last one taking it, as the engine)
function fxInto() {
  const into = new Array(NT).fill(-1);
  fxMasks().forEach((mk, f) => { for (let t = 0; t < NT; t++) if (mk & (1 << t)) into[t] = f; });
  return into;
}
function renderPads() {
  const into = fxInto();
  $('pads').classList.toggle('banks', !!ui.ptn);
  $('pads').classList.toggle('mv', ui.moveFrom != null && !ui.ptn);
  pads.forEach((b, t) => b.classList.toggle('mvsrc', ui.moveFrom === t && !ui.ptn));
  if (ui.ptn) {
    // PTN: the 8 banks (the one shown, the one playing, how many patterns each holds)
    pads.forEach((b, t) => {
      for (const c of ['fxin', 'muted', 'boxed']) b.classList.remove(c);
      b.classList.toggle('cur', t === (ui.bank | 0));
      b.classList.toggle('plays', t === state.pat >> 4);
      const n = Array.from({ length: NS }, (_, i) => patUsed(t * NS + i)).filter(Boolean).length;
      b.querySelector('.n').textContent = 'ABCDEFGH'[t];
      b.querySelector('.m').innerHTML = `<span>${n ? n + ' PTN' : 'EMPTY'}</span>`; b.dataset.m = '';
      b.setAttribute('aria-pressed', String(t === (ui.bank | 0)));
      b.setAttribute('aria-label', `Bank ${'ABCDEFGH'[t]}, ${n} patterns` + (t === state.pat >> 4 ? ', playing' : ''));
      b.title = '';
    });
    return;
  }
  pads.forEach((b, t) => {
    const tr = state.tracks[t];
    b.classList.remove('plays');
    if (b.querySelector('.n').textContent !== String(t + 1)) b.querySelector('.n').textContent = String(t + 1);
    b.classList.toggle('fxin', into[t] >= 0);
    b.classList.toggle('cur', t === ui.t);
    b.classList.toggle('muted', !!tr.mute);
    b.classList.toggle('boxed', idx(tr.p[P.IR], NIR + 1) > 0);
    b.classList.toggle('drn', droneOn(t));                // (v81: a drone: ∞)
    const pm = mof(tr.p[P.MACH]);
    if (b.dataset.m !== String(pm)) { b.dataset.m = String(pm); b.querySelector('.m').innerHTML = logoSvg(pm) + `<span>${MACH[pm].id}</span>`; }
    b.setAttribute('aria-pressed', t === ui.t ? 'true' : 'false');
    b.setAttribute('aria-label', `Track ${t + 1}, ${MACH[pm].id}${droneOn(t) ? ', drone' : ''}${tr.mute ? ', muted' : ''}${into[t] >= 0 ? ', into track ' + (into[t] + 1) : ''}`);
    b.title = into[t] >= 0 ? `T${t + 1} plays through T${into[t] + 1} (${MACH[mof(state.tracks[into[t]].p[P.MACH])].id})` : '';
  });
}
function renderScreen() {
  const m = curMachine();
  $('screen').classList.toggle('locking', !!ui.lock);
  // CHOIR: its screen is the ribbon (shown on a phone too)
  $('screen').classList.toggle('ribbon', choirShown());
  if (!choirShown() && ui.rib) ribLetGo();
  // MANTRA: its screen is its eight pads (on a phone too)
  $('screen').classList.toggle('pads', mantraShown());
  if ((!mantraShown() || ui.mtT !== ui.t) && ui.mtPress) mtLetGo();
  $('mchip').textContent = ui.all ? 'ALL' : 'T' + (ui.t + 1);
  $('mname').textContent = MACH[m].id;
  if (ui.logoShown !== m) { ui.logoShown = m; $('mlogo').innerHTML = logoSvg(m); $('mkana').textContent = KANA[m]; $('mnum').textContent = 'N°' + String(m + 1).padStart(2, '0'); }
  // (v79: WEST: what the control touched does, in words)
  const hint = !ui.lock && !ui.all && m === WEST_M && ui.lastP != null && ui.lastP >= 0 && ui.lastP !== RATP && ui.lastP !== RNDP && ui.lastP !== P.MACH ? paramMeta(ui.t, ui.lastP).hint : '';
  $('mlong').textContent = ui.lock ? 'P-LOCK · STEP ' + [...ui.lock].sort((a, b) => a - b).map(s => s + 1).join(' · ')
    : ui.all ? 'ALL · 8 TRACKS' : hint || MACH[m].long;
  const mp = $('mpick'); mp.hidden = !ui.picker;
  mp.querySelectorAll('button').forEach(b => b.classList.toggle('sel', +b.dataset.m === m));
  let label = '', value = '';
  if (ui.lastP === RATP) {
    const r = trk().steps[ui.ratStep] ? trk().steps[ui.ratStep].r || 1 : 1;
    label = 'RATCHET · STEP ' + (ui.ratStep + 1); value = r > 1 ? '×' + r : 'OFF';
  } else if (ui.lastP === RNDP) {
    label = ui.rndInfo ? ui.rndInfo.label : 'RANDOM'; value = ui.rndInfo ? ui.rndInfo.value : '';
  } else if (ui.lastP < 0) { const g = -1 - ui.lastP; const gm = gMeta(g); label = gm ? gm.l : ''; value = gm ? gm.f(state.g[g]) : ''; }
  else if (ui.lastP === P.MACH) { label = 'MACH'; value = MACH[m].id; }
  else { const meta = paramMeta(ui.t, ui.lastP); const sv = shownValue(ui.lastP); label = meta.rl || meta.l; value = meta.f(sv.v); }
  $('plabel').textContent = label; $('pval').textContent = value;
}
// the master pages' names, written out (the plugin prints the page shown beside its knobs)
const PAGE_NAME = { EQ: 'EQ', COMP: 'COMPRESSOR', DRV: 'DRIVE', GRN: 'GRAIN', OTT: 'OTT', GLCH: 'GLITCH', MIX: 'MIX' };
const MASTER_PAGES = ['EQ', 'COMP', 'DRV', 'GRN', 'OTT', 'GLCH', 'MIX'].map(id => PAGE_BY_ID[id]);
// a module's knobs and its MORE page hold a p-lock (its header's dot)
const pageHasLock = pg => !pg.glob && [...pg.ps, ...(MORE_PAGES[pg.id] ? MORE_PAGES[pg.id].ps : [])].some(p => p != null && trackHasLock(ui.t, p));
function renderTabs() {
  document.querySelectorAll('#gtabs .tab').forEach(b => {
    const i = +b.dataset.i, pg = PAGES[i];
    b.classList.toggle('cur', i === ui.gpage);
    b.classList.toggle('byp', pageBypassed(pg.id));
  });
  // the modules: a dot when they hold a lock, crossed out when their effect is off; TONE's MORE key
  for (const tab in KMODS) for (const [id] of KMODS[tab]) {
    const sec = $('km-' + id), pg = PAGE_BY_ID[id];
    if (!sec) continue;
    sec.classList.toggle('haslock', pageHasLock(pg));
    sec.classList.toggle('byp', pageBypassed(id));
    const mb = $('kmore-' + id);
    if (mb) { const on = moreOn(id); mb.classList.toggle('on', on); mb.setAttribute('aria-pressed', String(on)); mb.classList.toggle('haslock', MORE_PAGES[id].ps.some(p => p != null && trackHasLock(ui.t, p))); }
  }
  // the tab groups (the FX page's returns, the MOD page's modulators): the one shown, each tab's pixel and lock dot
  for (const g of TGROUPS) for (const id of g.ids) {
    const sec = $('km-' + id); if (!sec || tgOf(id) !== g) continue;
    sec.classList.toggle('cur', retShown(id));
    sec.querySelectorAll('.rtab').forEach(b => { const r = b.dataset.ret, cur = r === tgCur(g);
      b.classList.toggle('cur', cur); b.setAttribute('aria-selected', String(cur)); b.classList.toggle('on', g.on(r)); b.tabIndex = cur ? 0 : -1;
      b.classList.toggle('haslock', !cur && pageHasLock(PAGE_BY_ID[r])); });
  }
  // a page key: a dot when one of its modules holds a lock
  document.querySelectorAll('#ptabs .ptab').forEach(b => { const m = KMODS[b.dataset.tab]; b.classList.toggle('haslock', !!m && m.some(([id]) => pageHasLock(PAGE_BY_ID[id]))); });
  $('machmod').classList.toggle('haslock', (wsOn() ? WS_ALLPS : bankPage('m').ps).some(p => trackHasLock(ui.t, p)));
  // the TRIG row's two pages
  [[$('tpk0'), PAGES[TRG_PAGE], !ui.tmore], [$('tpk1'), TRG_MORE_PAGE, !!ui.tmore]].forEach(([b, pg, on]) => {
    if (!b) return;
    b.classList.toggle('on', on); b.setAttribute('aria-pressed', String(on));
    b.classList.toggle('haslock', pg.ps.some(p => trackHasLock(ui.t, p)));
  });
  // the plugin's MASTR page writes the page shown along the left of its knobs, with its place in the row of tabs
  const gk = $('gknobs'), gp = PAGES[ui.gpage];
  if (gk && gp) { gk.dataset.vlab = PAGE_NAME[gp.id] || gp.id; gk.dataset.vidx = (MASTER_PAGES.indexOf(gp) + 1) + '/' + MASTER_PAGES.length; }
}
// the knobs on the page shown (the others are drawn when their page comes up: renderAll)
const retShown = id => { const g = tgOf(id); return !g || tgCur(g) === id; };
const bankShown = bank => bank[0] === 'P' ? !!KMODS[ui.ptab] && KMODS[ui.ptab].some(([id]) => id === bank.slice(2)) && retShown(bank.slice(2)) : bank === 'g' ? ui.ptab === 'mt'
  : bank[0] === 'w' ? ui.ptab === 'ms' && wsOn() : bank === 'm' ? ui.ptab === 'ms' && !wsOn() : ui.ptab === 'ms';
function renderKnobs() {
  knobs.forEach(K => {
    if (!bankShown(K.bank)) return;
    const pg = bankPage(K.bank);
    // (v79: WEST's banks: as many as the page has)
    if (K.bank[0] === 'w') { const gone = K.i >= pg.ps.length; K.el.hidden = gone; if (gone) return; }
    const tgt = knobTarget(K);
    const v = tgt.cur;
    const off = !!tgt.meta.off;
    K.el.classList.toggle('off', off);
    K.el.classList.toggle('alllock', !!tgt.meta.alllock);
    // (v80: a knob in a frame, its silkscreen over it: DUCK, under track 1)
    K.el.classList.toggle('framed', !!tgt.meta.frame);
    if (tgt.meta.frame) { if (K.el.dataset.frame !== tgt.meta.frame) K.el.dataset.frame = tgt.meta.frame; } else if (K.el.dataset.frame) delete K.el.dataset.frame;
    K.el.setAttribute('aria-disabled', off ? 'true' : 'false');
    const sw = isSwitch(tgt.meta);
    K.el.classList.toggle('isw', sw);
    K.el.setAttribute('role', sw ? 'radiogroup' : 'slider');
    if (sw) renderSwitch(K, tgt);
    K.lt.textContent = tgt.meta.l;
    K.kv.textContent = tgt.meta.f(v);
    K.el.classList.toggle('glob', !!pg.glob);
    K.el.classList.toggle('all', !!tgt.all);
    K.el.classList.toggle('locked', !!(ui.lock && !pg.glob && tgt.locked));
    K.el.classList.toggle('inherit', !!(ui.lock && !pg.glob && !tgt.locked));
    K.el.classList.toggle('haslock', !pg.glob && !ui.lock && trackHasLock(ui.t, tgt.p));
    K.el.setAttribute('aria-label', tgt.meta.rl || tgt.meta.l);
    K.el.setAttribute('aria-valuetext', tgt.meta.f(v));
    // v79 (WEST's panel): a CV's source beside its depth (a pair), the depth in the source's colour, a slider, the ring
    // where a CV takes it now (westLive draws it), a stage's LED and pulse key
    if (K.bank[0] === 'w') {
      const mt = tgt.meta, fd = !!K.fad && !!mt.fader && !off;
      K.el.classList.toggle('pairl', mt.pair === 'l'); K.el.classList.toggle('pairr', mt.pair === 'r');
      K.el.classList.toggle('tint', mt.tint != null);
      if (mt.tint != null) K.el.style.setProperty('--arc', WS_SRCC[idx(shownValue(mt.tint).v, 8)]);
      K.el.classList.toggle('isfd', fd);
      K.el.classList.toggle('stg', fd && mt.stage != null);
      K.stage = fd && mt.stage != null ? mt.stage : -1;
      K.curV = v;
      K.live = !off && !tgt.all && mt.live ? mt : null;
      if (!K.live) westGhost(K, null);
      if (fd) {
        const x = clamp(v, 0, 1), bp = !!mt.bip;
        K.el.style.setProperty('--v', x.toFixed(4));
        K.el.style.setProperty('--b0', (bp ? Math.min(0.5, x) : 0).toFixed(4));
        K.el.style.setProperty('--b1', (bp ? Math.max(0.5, x) : x).toFixed(4));
        if (mt.stage != null) K.spk.classList.toggle('on', !!(idx(shownValue(SQ(49)).v, 32) >> mt.stage & 1));
        K.gLast = null;
        return;
      }
      K.gLast = null;
    }
    if (sw || off) return;
    const bip = pg.glob ? gBip(tgt.p) : BIPOLAR.has(tgt.p) || !!tgt.meta.bip;
    renderScale(K, tgt.meta, bip);
    const a = A0 + SWEEP * clamp(v, 0, 1);
    const from = bip ? 270 : A0;
    K.val.setAttribute('d', Math.abs(a - from) < 0.5 ? '' : arcPath(24, Math.min(from, a), Math.max(from, a)));
    const [x2, y2] = polar(12, a);
    K.ind.setAttribute('x2', x2.toFixed(2)); K.ind.setAttribute('y2', y2.toFixed(2));
    if (tgt.all) K.dots.forEach((c, t) => {
      const [x, y] = polar(29, A0 + SWEEP * clamp(state.tracks[t].p[tgt.p], 0, 1));
      c.setAttribute('cx', x.toFixed(2)); c.setAttribute('cy', y.toFixed(2));
      c.classList.toggle('me', t === ui.t);
      c.classList.toggle('out', allSkips(t, tgt.p));
    });
  });
}
// a labelled switch: one cell per position, the current one lit; in ALL mode, each track's dot sits on its position
function renderSwitch(K, tgt) {
  const n = tgt.meta.n, labels = Array.from({ length: n }, (_, i) => tgt.meta.f(mid(i, n)));
  if (tgt.meta.src) { renderSrc(K, tgt, labels); return; }
  const icons = tgt.meta.icons, key = labels.join('|') + (icons ? '|i' : '');
  if (K.swKey !== key) {
    K.swKey = key;
    K.sw.className = 'sw n' + n + (n >= 4 && Math.max(...labels.map(l => l.length)) >= 4 ? ' lg' : '') + (icons ? ' ic' : '');   // (long words in a grid: smaller; drawn ones: a column)
    K.sw.innerHTML = labels.map((l, i) => `<b data-i="${i}">${icons ? `<svg viewBox="0 0 20 12" aria-hidden="true"><path d="${WS_ICON[icons[i]]}"/></svg>` : ''}<span>${l}</span><i></i></b>`).join('');
  }
  const cur = idx(tgt.cur, n);
  [...K.sw.children].forEach((b, i) => {
    b.classList.toggle('on', i === cur);
    const dots = b.querySelector('i');
    dots.innerHTML = tgt.all ? state.tracks.map((tr, t) => !allOut(t) && idx(tr.p[tgt.p], n) === i ? `<u${t === ui.t ? ' class="me"' : ''}></u>` : '').join('') : '';
  });
}
// v79: a CV's source (WEST): its jack (the source's name, its colour), a dot for each source (tapped: that one; the jack
// tapped: the next one)
function renderSrc(K, tgt, labels) {
  const n = labels.length, cur = idx(tgt.cur, n), key = 'src|' + labels.join('|');
  if (K.swKey !== key) {
    K.swKey = key; K.sw.className = 'sw src';
    K.sw.innerHTML = '<em><i></i><span></span></em><p>' + labels.map((l, i) => `<b data-i="${i}" style="--c:${WS_SRCC[i]}"></b>`).join('') + '</p>';
  }
  const em = K.sw.firstChild;
  em.style.setProperty('--c', WS_SRCC[cur]); em.lastChild.textContent = labels[cur];
  K.sw.lastChild.querySelectorAll('b').forEach((b, i) => b.classList.toggle('on', i === cur));
}
const REV_DESC = ['8-LINE FDN · EARLY REFLECTIONS', 'DATTORRO · FIGURE-8 TANK', 'DISPERSIVE SPRING', 'STFT · AURORA-STYLE', '8-LINE FDN · MODULATED', 'LONG FDN · SHIMMER', 'GRANULAR · DELAY · REVERB'];
// over the trig keys: the ALL key and the hint, or what p-lock / CONTROL ALL is doing
const KEY_ICON = '<svg viewBox="0 0 15 10" aria-hidden="true"><path d="M.5 .5h14v9H.5z M5.17 .5v9 M9.83 .5v9" fill="none" stroke="currentColor" stroke-width="1"/><rect x="3.4" y=".5" width="3.5" height="5.5"/><rect x="8.1" y=".5" width="3.5" height="5.5"/></svg>';
// v85: the gestures' card (the plugin's ? key) closes on a press anywhere else
document.addEventListener('pointerdown', e => {
  if (!ui.gpop || e.target.closest('#gpop, #ghelp')) return;
  ui.gpop = false; renderLockbar();
}, true);
function renderLockbar() {
  const lb = $('lockbar');
  if (ui.gpop && (ui.lock || ui.all || ui.key || ui.ptn || ui.moveFrom != null)) ui.gpop = false;
  // v80: Move track: what a pad does now
  lb.classList.toggle('mv', ui.moveFrom != null);
  if (ui.moveFrom != null) {
    for (const c of ['on', 'all', 'key', 'ptn', 'recs']) lb.classList.remove(c);
    lb.innerHTML = `<div class="lt">MOVE T${ui.moveFrom + 1}<small>TAP A PAD: ITS NEW PLACE · THE TWO TRACKS SWAP, IN EVERY PATTERN</small></div><button class="btn" id="mvno">Cancel</button>`;
    $('mvno').onclick = () => setMove(false);
    return;
  }
  lb.classList.toggle('on', !!ui.lock);
  lb.classList.toggle('all', !ui.lock && ui.all);
  lb.classList.toggle('key', !ui.lock && !ui.all && !ui.ptn && !!ui.key);
  lb.classList.toggle('ptn', !ui.lock && !ui.all && !!ui.ptn);
  const keyBtn = on => `<button class="btn keybtn${on ? ' on' : ''}" id="keybtn" aria-pressed="${on}" title="Keyboard: the trig keys play notes on the track (K)">${KEY_ICON}Key</button>`
    + ptnBtn();
  // PTN: the pattern playing (and the next one, while the change waits)
  const ptnBtn = () => {
    const nx = patNextShown(), q = ui.pos.run && nx >= 0 && nx !== state.pat;
    return `<button class="btn ptnbtn${ui.ptn ? ' on' : ''}${q ? ' q' : ''}" id="ptnbtn" aria-pressed="${!!ui.ptn}" title="Patterns: the trig keys pick one of the 16 of a bank, the pads the banks (P)"><small>PTN</small><b>${patName(state.pat)}</b>${q ? `<i>›${patName(nx)}</i>` : ''}</button>`;
  };
  const bindKey = () => { $('keybtn').onclick = () => setKey(!ui.key); $('ptnbtn').onclick = () => setPtn(!ui.ptn); };
  const allIcon = '<svg viewBox="0 0 17 9" aria-hidden="true">' + [0, 1, 2, 3].map(i => `<rect x="${i * 4.5}" y="0" width="3.5" height="3.5"/><rect x="${i * 4.5}" y="5.5" width="3.5" height="3.5"/>`).join('') + '</svg>';
  // after ALL, while the tracks still differ from the moment it was pressed: a key to go back there
  const revOk = !ui.all && allChanged();
  const allBtn = () => `<button class="btn allbtn" id="allbtn" aria-pressed="false" title="Control all: the knobs move the 8 tracks">${allIcon}All</button>`
    + (revOk ? `<button class="btn allrev" id="allrev" title="Back to the sounds of every track before Control all">↶</button>` : '');
  const bindAll = () => { $('allbtn').onclick = () => setAll(true); if (revOk) $('allrev').onclick = allRevert; };
  if (!ui.lock && !ui.all && ui.ptn) {
    const nx = patNextShown(), ch = state.chain, arm = ui.patArm;
    const chainTxt = ch.length ? 'CHAIN ' + ch.map((p, k) => (ui.patInfo && Math.round(ui.patInfo[3]) === k && p === state.pat ? '▸' : '') + patName(p)).join(' ') : (ui.chainEdit ? 'CHAIN: TAP THE PATTERNS IN TURN' : 'TAP: NEXT PATTERN · CHAIN: SEVERAL IN TURN');
    const what = arm === 'paste' ? 'PASTE ' + (ui.clipName || '') + ' ONTO: TAP A PATTERN' : arm === 'clear' ? 'CLEAR: TAP A PATTERN' : chainTxt;
    const chg = state.pchg | 0;
    lb.innerHTML = keyBtn(false) + `<div class="lt">PATTERN ${patName(state.pat)}${ui.pos.run && nx >= 0 && nx !== state.pat ? ' → ' + patName(nx) : ''} · BANK ${'ABCDEFGH'[ui.bank | 0]}<small><span id="ptncnt">${patCount()}</span>${patCount() ? ' · ' : ''}${what}</small></div>`
      + `<div class="prow"><button class="btn${ui.chainEdit ? ' on' : ''}" id="pchain" aria-pressed="${!!ui.chainEdit}" title="Chain: the patterns tapped play in turn, then again">Chain</button>`
      + `<button class="btn" id="pcopy" title="Copy the pattern playing (its sounds and steps)">Copy</button>`
      + `<button class="btn${arm === 'paste' ? ' armed' : ''}" id="ppaste"${ui.clip ? '' : ' disabled'} title="Paste the copy onto the pattern tapped next">Paste</button>`
      + `<button class="btn${arm === 'clear' ? ' armed' : ''}" id="pclear" title="Clear the pattern tapped next (the one playing keeps its sounds)">Clear</button>`
      + `<button class="btn pchg" id="pchg" title="CHG: when a change comes, in 16ths from the pattern's start (AUTO: its longest track)"><small>CHG</small>${chg ? chg : 'AUTO'}</button>`
      + `<button class="btn pmode" id="pmode" title="How a change comes: SEQ at the end of the pattern, START at once from its first step, JUMP at once where the playhead is"><small>MODE</small>${PAT_MODES[state.pmode | 0]}</button></div>`;
    bindKey();
    $('pchain').onclick = () => { ui.chainEdit = !ui.chainEdit; ui.chainNew = false; ui.patArm = null; renderLockbar(); renderSteps(); };
    $('pcopy').onclick = () => { patCopy(); renderLockbar(); };
    $('ppaste').onclick = () => { ui.patArm = arm === 'paste' ? null : 'paste'; ui.chainEdit = false; renderLockbar(); renderSteps(); };
    $('pclear').onclick = () => { ui.patArm = arm === 'clear' ? null : 'clear'; ui.chainEdit = false; renderLockbar(); renderSteps(); };
    $('pchg').onclick = () => { const v = PAT_CHG[(PAT_CHG.indexOf(chg) + 1) % PAT_CHG.length]; state.pchg = v; send(['pchg', v]); touched(); renderLockbar(); };
    $('pmode').onclick = () => { state.pmode = ((state.pmode | 0) + 1) % 3; touched(); renderLockbar(); };
    return;
  }
  if (!ui.lock && !ui.all && ui.key) {
    const kl = keyLayout(), tr = trk();
    const bs = brkResolve(idx(tr.p[P.A], BRK_SLOTS));
    const what = kl.brk ? (bs < 0 ? 'NO LOOP LOADED' : 'SLOT ' + (bs + 1) + (brkName(bs) ? ' ' + esc(brkName(bs)) : '')) + ' · ITS SLICES, A BAR A ROW' : kl.fold ? scaleName(state.scale, state.qroot) + ' · 16 NOTES' : 'CHROMATIC · ' + (state.scale && tr.q === 0 ? 'TRACK OFF SCALE' : 'NO SCALE');
    const o = ui.oct || 0;
    lb.innerHTML = allBtn() + keyBtn(true) + `<div class="lt">KEYBOARD · T${ui.t + 1}<small>${what}${ui.rec ? ' · REC: NOTES ON THE PLAYING STEP' : ''}</small></div>`
      + `<button class="btn octb" id="octdn" aria-label="Octave down"${keyLayout(o - 1).keys.some(n => n != null) && o > (kl.brk ? 0 : -3) ? '' : ' disabled'}>−</button>`
      + `<b class="oct" aria-live="polite"><small>${kl.brk ? 'BAR' : 'OCT'}</small>${kl.brk ? o + 1 : o > 0 ? '+' + o : o}</b>`
      + `<button class="btn octb" id="octup" aria-label="Octave up"${keyLayout(o + 1).keys.some(n => n != null) && o < (kl.brk ? BRK_MAXBARS - 1 : 3) ? '' : ' disabled'}>+</button>`;
    bindKey(); bindAll(); $('octdn').onclick = () => setOct(-1); $('octup').onclick = () => setOct(1);
    return;
  }
  lb.classList.toggle('recs', !ui.lock && !ui.all && !ui.key && recSide());
  if (!ui.lock && !ui.all && !ui.key && recSide()) {
    // RAM on its REC side: the keys place REC trigs; what a take does; a take right now
    const tr = trk(), src = RAM_SRC[idx(tr.p[P.RSRC], RAM_SRC.length)], take = fTake(RAM_TAKE[idx(tr.p[P.RLEN], RAM_TAKE.length)]);
    lb.innerHTML = allBtn() + keyBtn(false) + `<div class="lt">REC TRIGS · T${ui.t + 1}<small>A TRIG RECORDS ${take} FROM ${src}${tr.mute ? ' · MUTED, STILL RECORDING' : ''}</small></div>`
      + `<button class="btn rnow" id="rnow" title="Record a take now (from ${src}, ${take})"><i aria-hidden="true"></i>Take</button>`;
    bindAll(); bindKey();
    $('rnow').onclick = () => { backend.ensureAudio(); if (src === 'IN') backend.enableInput(); send(['rnow', ui.t]); };
    return;
  }
  if (ui.lock) {
    const tr = trk(), up = [...ui.lock].sort((a, b) => a - b), rs = recSide();
    const list = up.map(s => `<b class="${rs ? 'rc' : tr.steps[s].on ? 'tg' : 'pc'}">${String(s + 1).padStart(2, '0')}</b>`).join('');
    const what = rs ? 'TAP = REC TRIG ON / OFF' : 'TAP = <i class="ltg"></i>TRIG / <i class="lpc"></i>SOUND CHANGE';
    lb.innerHTML = `<div class="lt">P-LOCK <span class="lsteps">${list}</span><small>↻ KNOB = LOCK · ${what} · SWIPE ↓ = DONE</small></div><button class="btn" id="lkclr">Clear</button><button class="btn" id="lkok">OK</button>`;
    $('lkclr').onclick = () => { const tr = trk(); for (const s of ui.lock) { tr.steps[s].l = {}; send(['clrl', ui.t, s]); } touched(); renderAll(); };
    $('lkok').onclick = exitLock;
  } else if (ui.all) {
    // the eight tracks: a lit LED moves with the knobs, tapped it goes dark (the track is left out)
    const leds = state.tracks.map((t, i) => `<button class="aled${t.ax ? '' : ' on'}${i === ui.t ? ' me' : ''}" data-t="${i}" aria-pressed="${!t.ax}" aria-label="Track ${i + 1}${t.ax ? ', left out' : ''}" title="T${i + 1}: ${t.ax ? 'left out' : 'moves with the knobs'}"><span>${i + 1}</span></button>`).join('');
    const n = state.tracks.filter(t => !t.ax).length, ch = allChanged();
    const pmA = ui.pageMsg && performance.now() < ui.pageMsg.until ? ui.pageMsg : null;
    lb.innerHTML = `<span class="leds">${leds}</span><div class="lt">CONTROL ALL<small>${pmA ? esc(pmA.txt) : `<span class="ln">↻ KNOB = </span>${n} TRACK${n === 1 ? '' : 'S'}<span class="ln"> · TAP A NUMBER: LEAVE IT OUT</span>`}</small></div>`
      + (pmA && pmA.undo ? '<button class="btn" id="pgundo" title="Back to the page as it was">↶ Page</button>' : '')
      + `<button class="btn" id="allrev"${ch ? '' : ' disabled'} title="Back to the sounds of every track before Control all" aria-label="Revert">↶<span class="rv"> Revert</span></button><button class="btn" id="allok">OK</button>`;
    if (pmA && pmA.undo) $('pgundo').onclick = () => { const u = pmA.undo; ui.pageMsg = null; u(); renderLockbar(); };
    lb.querySelectorAll('.aled').forEach(b => { b.onclick = () => setAllOut(+b.dataset.t, !state.tracks[+b.dataset.t].ax); });
    $('allrev').onclick = allRevert;
    $('allok').onclick = () => setAll(false);
  } else {
    // the gestures as pictograms: a trig key tapped twice (or Shift + click), dragged up / down; a pad swiped down
    const hint = `<span title="Double-tap a step (or Shift + click): it comes up for p-locks"><b>${inPlugin ? '⇧+' : '2×'}</b><i class="ik"></i>P-LOCK</span>`
      + `<span title="Drag a step up / down: ratchets"><i class="ik"></i><b>↕</b>RATCHET</span>`
      + `<span title="Swipe a pad down: mute"><i class="ik ikp"></i><b>↓</b>MUTE</span>`;
    const arm = ui.clrArm && performance.now() - ui.clrArm < 2500;
    const pm = ui.pageMsg && performance.now() < ui.pageMsg.until ? ui.pageMsg : null;
    // v85: in the plugin the bar beside the track bar has no room for the five gestures: a ? key shows them on a card
    const gestures = inPlugin && !pm
      ? `<button class="btn ghelp${ui.gpop ? ' on' : ''}" id="ghelp" aria-expanded="${!!ui.gpop}" aria-controls="gpop" title="Gestures">?</button>`
        + (ui.gpop ? `<div class="gpop" id="gpop" role="dialog" aria-label="Gestures"><b class="gph">GESTURES</b>`
          + `<i class="ik"></i><em>⇧ + CLICK A STEP</em><span>P-LOCK IT</span>`
          + `<i class="ik"></i><em>DRAG A STEP ↕</em><span>RATCHETS</span>`
          + `<i class="ik ikp"></i><em>SWIPE A PAD ↓</em><span>MUTE THE TRACK</span>`
          + `<i class="ik ikg"></i><em>TAP A PAGE KEY 2×</em><span>COPY THE PAGE</span>`
          + `<i class="ik ikg"></i><em>TAP A PAGE KEY 3×</em><span>PASTE IT THERE</span></div>` : '')
      : '';
    const hintHtml = pm ? `<div class="hint pmsg"><span><b>${esc(pm.txt)}</b></span>${pm.undo ? '<button class="btn" id="pgundo" title="Back to the page as it was">↶</button>' : ''}</div>`
      : inPlugin ? gestures
      : `<div class="hint">${hint}<span title="Tap a page key twice: copy the page"><b>2×</b><i class="ik ikg"></i>PAGE COPY</span>`
        + `<span title="Tap a page key three times: paste the page copied there"><b>3×</b><i class="ik ikg"></i>PASTE</span></div>`;
    lb.innerHTML = allBtn() + keyBtn(false)
      + `<button class="btn clrmod${arm ? ' armed' : ''}" id="clrmod" title="Clear mod: remove the recorded moves (every p-lock) of this track, the trigs stay">${arm ? 'Sure?' : '× Mod'}</button>${hintHtml}`;
    bindAll(); bindKey();
    if ($('ghelp')) {
      $('ghelp').onclick = () => { ui.gpop = !ui.gpop; renderLockbar(); };
      const gp = $('gpop');
      if (gp) gp.style.left = Math.max(0, $('ghelp').offsetLeft + $('ghelp').offsetWidth - 300) + 'px';
    }
    if (pm && pm.undo) $('pgundo').onclick = () => { const u = pm.undo; ui.pageMsg = null; u(); renderLockbar(); };
    $('clrmod').onclick = () => {
      if (ui.clrArm && performance.now() - ui.clrArm < 2500) { ui.clrArm = 0; clearMods(ui.t); renderAll(); return; }
      ui.clrArm = performance.now(); renderLockbar();
      setTimeout(() => { if (ui.clrArm && performance.now() - ui.clrArm >= 2500) { ui.clrArm = 0; renderLockbar(); } }, 2600);
    };
  }
}
// TRACK: which track the pages edit (its logo, its number, the 8 tracks as cells), arrows to go from one to the
// next, and the track's spectrum
function renderTStrip() {
  const m = curMachine();
  if (ui.tmkShown !== m) { ui.tmkShown = m; $('tmk').innerHTML = logoSvg(m); }
  $('tnum').textContent = ui.all ? 'ALL' : 'T' + (ui.t + 1);
  $('tname').textContent = ui.all ? '8 TRACKS' : MACH[m].id;
  $('tdots').innerHTML = state.tracks.map((tr, t) => `<i data-t="${t}" class="${t === ui.t ? 'on' : ''}${tr.mute ? ' mu' : ''}" title="Track ${t + 1}"><b>${t + 1}</b></i>`).join('');
  $('tlk').hidden = !ui.lock;
  if (ui.specT !== ui.t) { ui.specT = ui.t; ui.spec = null; send(['spec', ui.t]); }
}
function setToneMore(on) { ui.tonemore = !!on; ui.page = PAGES.indexOf(PAGE_BY_ID.TONE); renderKnobs(); renderTStrip(); renderTabs(); }
// the track's screen: TONE's curve, the spectrum with the filter's cutoff (FILTER, AMP), the LFOs, a follower, the BOX
function trackView() {
  if (ui.all) return 'SPEC';
  const fid = PAGES[ui.page] && PAGES[ui.page].id;
  if (ui.ptab === 'md') { const g = tgOf('LFO1'), f = g ? tgCur(g) : fid; return f === 'EF1' || f === 'EF2' ? f : 'LFO'; }
  // (v70: FX has no screen of its own, its returns carry theirs; TONE its visualiser)
  if (ui.ptab === 'fx') return 'NONE';
  return ui.ptab === 'tn' ? 'TVIZ' : 'SPEC';
}
function buildTStrip() {
  $('tstrip').addEventListener('click', e => {
    const b = e.target.closest('[data-d]');
    if (b) selectTrack((ui.t + +b.dataset.d + NT) % NT);
    const c = e.target.closest('.tdots i');      // a track's cell: straight to it
    if (c) selectTrack(+c.dataset.t);
  });
}
// ---- the screens, drawn in lines like an oscilloscope: 1 px frames with ticks on their edges, a faint graticule,
// the data in 1 px lines (dashed when secondary, red only for "now"), the values in tags (framed, or reversed)
function lcdFrame(c, col, x, y, w, h, tx, ty, ink) {
  x = Math.round(x); y = Math.round(y); w = Math.round(w); h = Math.round(h);
  c.strokeStyle = ink || col.dim; c.lineWidth = 1; c.strokeRect(x + 0.5, y + 0.5, w - 1, h - 1);
  c.fillStyle = ink || col.dim;
  if (tx) for (const t of tx) { const xx = Math.round(x + t * w); if (xx > x + 2 && xx < x + w - 3) { c.fillRect(xx, y + 1, 1, 3); c.fillRect(xx, y + h - 4, 1, 3); } }
  if (ty) for (const t of ty) { const yy = Math.round(y + t * h); if (yy > y + 2 && yy < y + h - 3) { c.fillRect(x + 1, yy, 3, 1); c.fillRect(x + w - 4, yy, 3, 1); } }
}
function lcdGrid(c, col, x, y, w, h, xs, ys) {
  x = Math.round(x); y = Math.round(y); w = Math.round(w); h = Math.round(h);
  c.fillStyle = col.lo;
  if (xs) for (const t of xs) { const xx = Math.round(x + t * w); if (xx > x && xx < x + w - 1) c.fillRect(xx, y + 1, 1, h - 2); }
  if (ys) for (const t of ys) { const yy = Math.round(y + t * h); if (yy > y && yy < y + h - 1) c.fillRect(x + 1, yy, w - 2, 1); }
}
// a tag, 11 px high: 'box' framed, 'inv' reversed (ink ground), 'txt' bare; with right set, x is its right edge.
// Returns its width
const lcdText = t => String(t).toUpperCase().replace(/Μ/g, 'µ');
function lcdTag(c, col, txt, x, y, kind, right, ink) {
  txt = lcdText(txt);
  c.font = col.font; c.textBaseline = 'top'; c.textAlign = 'left';
  const w = Math.ceil(c.measureText(txt).width) + 7, X = Math.round(right ? x - w : x), Y = Math.round(y), fg = ink || col.ink;
  if (kind === 'inv') { c.fillStyle = fg; c.fillRect(X, Y, w, 11); c.fillStyle = col.bg; }
  else {
    c.fillStyle = col.bg; c.fillRect(X, Y, w, 11);
    if (kind !== 'txt') { c.strokeStyle = col.dim; c.lineWidth = 1; c.strokeRect(X + 0.5, Y + 0.5, w - 1, 10); }
    c.fillStyle = fg;
  }
  c.fillText(txt, X + 4, Y + 2);
  return w;
}
// keep what follows inside a frame's inner edge
function lcdClip(c, x, y, w, h) { c.save(); c.beginPath(); c.rect(Math.round(x) + 1, Math.round(y) + 1, Math.round(w) - 2, Math.round(h) - 2); c.clip(); }

const SPEC_N = 24, SPEC_COLS = 40, SPEC_ROWS = 8;
// the track's spectrum: 40 Hz .. 16 kHz across (the engine's 24 bands, joined by a smooth line), -54 .. -6 dB up,
// the level as a line over a faint fill, its peaks held as a dashed line; on the filter page, the cutoff in red.
// The height follows the width (as the LED version did, so nothing moves)
function drawSpec(now) {
  const cv = $('tspec'), w = cv.clientWidth;
  if (!w) return;
  const pid = trackView();
  if (pid === 'NONE') return;
  // (its height follows its width, up to a band's worth on a wide screen and the plugin; v70: TONE's visualiser
  // taller, the page showing one module under it)
  const pitch = w / SPEC_COLS, gap = Math.max(1, Math.round(pitch * 0.22));
  const h = pid === 'TVIZ' ? (inPlugin ? 214 : w < 500 ? 160 : 180) : Math.min(Math.round(SPEC_ROWS * pitch - gap), inPlugin ? 104 : 140);
  if (Math.abs(cv.clientHeight - h) > 0.5) cv.style.height = h + 'px';
  const dpr = DPR();
  if (cv.width !== Math.round(w * dpr) || cv.height !== Math.round(h * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); }
  if (!ovwColors || now - ovwColorsAt > 1000) { ovwColors = readColors(); ovwColorsAt = now; }
  const col = ovwColors, c = cv.getContext('2d');
  c.setTransform(dpr, 0, 0, dpr, 0, 0); c.clearRect(0, 0, w, h);
  // the modulation modules have their own views, TONE its visualiser; CONTROL ALL the spectrum alone
  if (pid === 'LFO') return drawLfoView(c, col, w, h);
  if (pid === 'EF1' || pid === 'EF2') return drawEfView(c, col, w, h, pid === 'EF1' ? 0 : 1);
  if (pid === 'TVIZ') return drawToneViz(c, col, w, h, now);
  const LO = -54, HI = -6;
  const lv = ui.specLv || (ui.specLv = new Float32Array(SPEC_N)), pk = ui.specPk || (ui.specPk = new Float32Array(SPEC_N).fill(LO - 6));
  const dt = Math.min(0.1, (now - (ui.specAt || now)) / 1000); ui.specAt = now;
  for (let b = 0; b < SPEC_N; b++) {
    const v = ui.spec ? ui.spec[b] : 0;
    lv[b] = Math.max(LO - 6, v > 1e-6 ? 20 * Math.log10(v) : -120);
    pk[b] = Math.max(lv[b], pk[b] - dt * 24);
  }
  const fr = d => 1 - (d - LO) / (HI - LO);
  const oct = [80, 160, 320, 640, 1280, 2560, 5120, 10240].map(f => Math.log(f / 40) / Math.log(400));
  lcdGrid(c, col, 0, 0, w, h, oct, [-42, -30, -18].map(fr));
  lcdFrame(c, col, 0, 0, w, h, oct, [-48, -36, -24, -12].map(fr));
  const X = u => 1 + u * (w - 3), Y = d => 1 + fr(d) * (h - 3);
  const n = Math.max(2, Math.round(w - 2));
  lcdClip(c, 0, 0, w, h);
  c.setLineDash([2, 2]); c.strokeStyle = col.dim; c.lineWidth = 1; c.beginPath();
  for (let i = 0; i <= n; i += 2) { const u = i / n, y = Y(specAt(pk, u)); i ? c.lineTo(X(u), y) : c.moveTo(X(u), y); }
  c.stroke(); c.setLineDash([]);
  c.beginPath();
  for (let i = 0; i <= n; i++) { const u = i / n, y = Y(specAt(lv, u)); i ? c.lineTo(X(u), y) : c.moveTo(X(u), y); }
  c.strokeStyle = col.ink; c.lineWidth = 1.2; c.stroke();
  c.lineTo(X(1), h); c.lineTo(X(0), h); c.closePath();
  c.globalAlpha = 0.09; c.fillStyle = col.ink; c.fill(); c.globalAlpha = 1;
  c.restore();
}
// the spectrum between its bands (40 Hz .. 16 kHz across u, 0 .. 1): Catmull-Rom
function specAt(a, u) {
  const bp = clamp(u, 0, 1) * (SPEC_N - 1), i = Math.min(SPEC_N - 2, Math.floor(bp)), t = bp - i;
  const p0 = a[Math.max(0, i - 1)], p1 = a[i], p2 = a[i + 1], p3 = a[Math.min(SPEC_N - 1, i + 2)];
  return 0.5 * (2 * p1 + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (3 * p1 - p0 - 3 * p2 + p3) * t * t * t);
}

// ---- TONE: the response of the five filters over the spectrum (+-18 dB over the height, 0 dB dashed in the middle),
// the corners of the high-pass and the low-pass in red, their values in tags
function toneParams(t) {
  const v = q => (t === ui.t ? shownValue(q).v : state.tracks[t].p[q]);
  return { hp: v(P.HPF), lp: v(P.LPF), lo: v(P.LOW), lof: v(P.LOWF), md: v(P.MID), mdf: v(P.MIDF), hi: v(P.HIGH), hif: v(P.HIGF) };
}
// the analog prototypes of the engine's filters (core/sk_mod.cpp): |N(jx)|^2 / |D(jx)|^2 of two second-order polynomials
const bq2 = (a2, a1, a0, b2, b1, b0, x) => { const n = (a0 - a2 * x * x) ** 2 + (a1 * x) ** 2, d = (b0 - b2 * x * x) ** 2 + (b1 * x) ** 2; return n / d; };
function toneDb(f, q) {
  let g = 1;
  const S2 = Math.SQRT2;
  if (hpOn(q.hp)) g *= bq2(1, 0, 0, 1, S2, 1, f / hpHz(q.hp));
  if (lpOn(q.lp)) g *= bq2(0, 0, 1, 1, S2, 1, f / lpHz(q.lp));
  const lo = (q.lo - 0.5) * 30, md = (q.md - 0.5) * 30, hi = (q.hi - 0.5) * 30;
  // shelves: the SVF's cutoff moved by sqrt(A), so that the corner sits at half the gain
  if (Math.abs(lo) > 0.05) { const A = Math.pow(10, lo / 40), r = Math.sqrt(A) * S2; g *= bq2(A, A * r, A * A, A, r, 1, f / lowHz(q.lof)); }
  if (Math.abs(md) > 0.05) { const A = Math.pow(10, md / 40), Q = 0.6 + 0.04 * Math.abs(md); g *= bq2(1, A / Q, 1, 1, 1 / (A * Q), 1, f / midHz(q.mdf)); }
  if (Math.abs(hi) > 0.05) { const A = Math.pow(10, hi / 40), r = Math.sqrt(A) * S2; g *= bq2(A * A, A * r, A, 1, r, A, f / highHz(q.hif)); }
  return 10 * Math.log10(Math.max(g, 1e-12));
}
// ---- v70: TONE's visualiser, to sculpt the track by eye. Under it the track as it plays (its spectrum after its
// TONE, a soft fill and its peaks held, dotted), over it the response of its filter and of its EQ together (the thick
// line, what they add or take away filled against 0 dB; each one's own dashed when both shape it; the filter
// envelope's reach dotted), and a handle for each thing to grab: FLT (the filter: its cutoff along, its resonance up,
// the peak), TONE's HP and LP (along; off at their ends) and its LO, MID and HI bands (along: the frequency; up: the
// gain). Grabbing one brings its module's tab and writes like its knob (a p-lock on the steps held up, REC); its
// values while held; a double tap resets it. The curves are the engine's own filters: core/sk_vintage.h (the MS-20's
// prototypes), core/sk_engine.cpp (the TRIO ladder, the comb, the bypass when wide open), core/sk_mod.cpp (TONE)
const TZ_F0 = 20, TZ_F1 = 20000, TZ_LO = -24, TZ_HI = 24;
const tzU = f => Math.log(f / TZ_F0) / Math.log(TZ_F1 / TZ_F0), tzF = u => TZ_F0 * Math.pow(TZ_F1 / TZ_F0, u);
const fltHz = v => 20 * Math.pow(2, v * 9.9658);
const fltK = rs => { const r2 = rs * rs, r8 = r2 * r2 * r2 * r2; return 2 - 1 / (0.5 + 24 * r2) + 0.141 * r8; };
const FLT_NAME = ['LP', 'BP', 'HP', 'COMB'];
// the filter as its knobs (or their locks) set it
function fltState(fn, rs) {
  const v = q => shownValue(q).v, ft = idx(v(P.FTYP), 4);
  return { ft, fn: clamp(fn === undefined ? v(P.FREQ) : fn, 0, 1), rs: rs === undefined ? v(P.RES) : rs, ladder: curMachine() === TRIO_M && ft === 0 };
}
// its magnitude (dB) at f
function fltDb(f, st) {
  const fc = fltHz(st.fn), x = f / fc, rs = st.rs;
  if (st.ft === 3) {
    // COMB: y = x + g y[n - D], D = sr / fc; out (1 - g / 2) y
    const g = rs * 0.97, m = (1 - g / 2) / Math.sqrt(Math.max(1e-9, 1 - 2 * g * Math.cos(2 * Math.PI * x) + g * g));
    return 20 * Math.log10(m);
  }
  const div = (nr, ni, dr, di) => { const d = dr * dr + di * di || 1e-12; return [(nr * dr + ni * di) / d, (ni * dr - nr * di) / d]; };
  let re, im;
  if (st.ladder) {
    // the CEM3320-style ladder: its make-up over (1 + s)^4 + k
    const k = 4.4 * rs * (0.35 + 0.65 * rs), a = 1 - x * x, b = 2 * x;
    [re, im] = div(1 + 0.35 * k, 0, a * a - b * b + k, 2 * a * b);
  } else {
    // the Korg35: LP 1 / D, HP s (1 + s) / D, BP s / D (and its gain), D = s^2 + (2 - K) s + 1
    const K = fltK(rs), dr = 1 - x * x, di = (2 - K) * x;
    if (st.ft === 0) [re, im] = div(1, 0, dr, di);
    else if (st.ft === 2) [re, im] = div(-x * x, x, dr, di);
    else { [re, im] = div(0, x, dr, di); re *= 2 - 0.85 * K; im *= 2 - 0.85 * K; }
  }
  // wide open (LP at the top, HP at the bottom) it steps out of the way
  const sm = o => { o = clamp(o, 0, 1); return o * o * (3 - 2 * o); };
  const open = st.ft === 0 ? sm((st.fn - 0.94) / 0.06) : st.ft === 2 ? sm(1 - st.fn / 0.05) : 0;
  if (open > 0) { re += (1 - re) * open; im -= im * open; }
  return 10 * Math.log10(Math.max(re * re + im * im, 1e-12));
}
const fltOpen = st => st.ft === 0 && st.fn >= 0.999 || st.ft === 2 && st.fn <= 0.001;
// the resonance that puts the peak (the response at the cutoff) at d dB: a bisection (it only rises with RES; the
// ladder's past its self-oscillation counts as the top)
function fltResFor(d, fn) {
  const st = fltState(fn, 0), pk = rs => { st.rs = rs; return st.ladder && rs > 0.944 ? 99 : Math.min(99, fltDb(fltHz(fn), st)); };
  if (d <= pk(0)) return 0;
  if (d >= pk(1)) return 1;
  let lo = 0, hi = 1;
  for (let i = 0; i < 28; i++) { const m = (lo + hi) / 2; if (pk(m) < d) lo = m; else hi = m; }
  return (lo + hi) / 2;
}
// the handles: where they sit (frequency, dB), the knobs they move, their module
const TZ_PS = { FLT: [P.FREQ, P.RES], HP: [P.HPF], LP: [P.LPF], LO: [P.LOW, P.LOWF], MID: [P.MID, P.MIDF], HI: [P.HIGH, P.HIGF] };
function tzHandles() {
  const st = fltState(), q = toneParams(ui.t), fc = fltHz(st.fn), g = v => (v - 0.5) * 30;
  const hpf = hpOn(q.hp) ? hpHz(q.hp) : TZ_F0, lpf = lpOn(q.lp) ? lpHz(q.lp) : TZ_F1;
  return [
    { id: 'FLT', mod: 'FLT', f: fc, db: fltDb(fc, st), on: !fltOpen(st), lab: 'FLT', type: st.ladder ? 'LP4' : FLT_NAME[st.ft] },
    { id: 'HP', mod: 'TONE', f: hpf, db: toneDb(hpf, q), on: hpOn(q.hp), lab: 'HP' },
    { id: 'LP', mod: 'TONE', f: lpf, db: toneDb(lpf, q), on: lpOn(q.lp), lab: 'LP' },
    { id: 'LO', mod: 'TONE', f: lowHz(q.lof), db: g(q.lo), on: Math.abs(g(q.lo)) > 0.05, lab: 'LO' },
    { id: 'MID', mod: 'TONE', f: midHz(q.mdf), db: g(q.md), on: Math.abs(g(q.md)) > 0.05, lab: 'MID' },
    { id: 'HI', mod: 'TONE', f: highHz(q.hif), db: g(q.hi), on: Math.abs(g(q.hi)) > 0.05, lab: 'HI' },
  ];
}
// a handle moved to (f, d): the knobs it stands for (0 dB catches the bands within half a dB)
function tzSet(id, f, d) {
  const set = (p, v) => { v = clamp(v, 0, 1); if (Math.abs(shownValue(p).v - v) > 1e-5) setTrackParam(p, v); };
  const gain = d => 0.5 + (Math.abs(d) < 0.5 ? 0 : clamp(d, -15, 15)) / 30;
  if (id === 'FLT') { const fn = clamp(Math.log2(f / 20) / 9.9658, 0, 1); set(P.FREQ, fn); set(P.RES, fltResFor(d, fn)); }
  else if (id === 'HP') { const v = Math.log(f / 20) / Math.log(400); set(P.HPF, v < 0.01 ? 0 : v); }
  else if (id === 'LP') { const v = Math.log(f / 100) / Math.log(200); set(P.LPF, v > 0.99 ? 1 : v); }
  else if (id === 'LO') { set(P.LOWF, Math.log(f / 40) / Math.log(10)); set(P.LOW, gain(d)); }
  else if (id === 'MID') { set(P.MIDF, Math.log(f / 150) / Math.log(53.333)); set(P.MID, gain(d)); }
  else if (id === 'HI') { set(P.HIGF, Math.log(f / 1500) / Math.log(10)); set(P.HIGH, gain(d)); }
}
// a double tap: the filter open, HP and LP off, a band flat (its frequency stays)
function tzReset(id) {
  const def = defaultParams(curMachine());
  const ps = id === 'LO' ? [P.LOW] : id === 'MID' ? [P.MID] : id === 'HI' ? [P.HIGH] : TZ_PS[id];
  for (const p of ps) if (Math.abs(shownValue(p).v - def[p]) > 1e-6) setTrackParam(p, def[p]);
  refreshAfterKnob({ glob: false, p: ps[0] });
}
// the curves only change with the knobs: computed again when one of them (or the width) does
let tzCache = { key: '' };
function tzCurves(w, A) {
  const st = fltState(), q = toneParams(ui.t), fenv = shownValue(P.FENV).v;
  const key = [w, st.ft, st.fn, st.rs, st.ladder, fenv, q.hp, q.lp, q.lo, q.lof, q.md, q.mdf, q.hi, q.hif].join();
  if (tzCache.key === key) return tzCache;
  // (a comb's teeth: four points a pixel, its line drawn through them)
  const sub = st.ft === 3 ? 4 : 1, n = Math.max(2, Math.round((A.x1 - A.x0) * sub));
  const xs = new Float32Array(n + 1), fl = new Float32Array(n + 1), eq = new Float32Array(n + 1), gh = new Float32Array(n + 1);
  const ge = clamp(st.fn + (fenv - 0.5) * 2, 0, 1), gst = Math.abs(fenv - 0.5) > 0.004 ? fltState(ge) : null;
  for (let i = 0; i <= n; i++) {
    const u = i / n, f = tzF(u);
    xs[i] = A.x0 + u * (A.x1 - A.x0); fl[i] = fltDb(f, st); eq[i] = toneDb(f, q); if (gst) gh[i] = fltDb(f, gst);
  }
  const shapes = a => a.some(d => Math.abs(d) > 0.25);
  tzCache = { key, n, xs, fl, eq, gh: gst ? gh : null, fShape: shapes(fl), eShape: shapes(eq) };
  return tzCache;
}
function drawToneViz(c, col, w, h, now) {
  const A = { x0: 1, x1: w - 1, y0: 1, y1: h - 1 };
  A.X = f => A.x0 + tzU(f) * (A.x1 - A.x0);
  const top = A.y0 + 16, bot = A.y1 - 14;
  A.Y = d => top + (1 - (clamp(d, TZ_LO, TZ_HI) - TZ_LO) / (TZ_HI - TZ_LO)) * (bot - top);
  A.dbAt = y => TZ_LO + (1 - (y - top) / (bot - top)) * (TZ_HI - TZ_LO);
  // (the curves run on past the frame, cut at its edge)
  const Yu = d => top + (1 - (clamp(d, TZ_LO - 40, TZ_HI + 40) - TZ_LO) / (TZ_HI - TZ_LO)) * (bot - top);
  c.font = col.font; c.textBaseline = 'alphabetic';
  // the grid: 100 / 1k / 10k ruled and written along the foot, the steps between them dotted; 0 dB dashed, +-12 dotted
  for (const f of [30, 40, 50, 60, 70, 80, 90, 200, 300, 400, 500, 600, 700, 800, 900, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000]) {
    const x = Math.round(A.X(f)); c.fillStyle = col.lo; for (let y = top; y < bot; y += 4) c.fillRect(x, y, 1, 1);
  }
  c.fillStyle = col.lo;
  for (const f of [100, 1000, 10000]) c.fillRect(Math.round(A.X(f)), A.y0, 1, A.y1 - A.y0);
  for (const d of [-12, 12]) { const y = Math.round(A.Y(d)); for (let x = A.x0; x < A.x1; x += 3) c.fillRect(x, y, 1, 1); }
  { const y = Math.round(A.Y(0)) + 0.5; c.strokeStyle = col.dim; c.lineWidth = 1; c.setLineDash([4, 3]); c.beginPath(); c.moveTo(A.x0, y); c.lineTo(A.x1, y); c.stroke(); c.setLineDash([]); }
  c.fillStyle = col.dim; c.textAlign = 'left';
  for (const [f, t] of [[100, '100'], [1000, '1K'], [10000, '10K']]) c.fillText(t, Math.round(A.X(f)) + 3, A.y1 - 3);
  c.textAlign = 'right';
  for (const d of [-12, 12]) c.fillText((d > 0 ? '+' : '') + d, A.x1 - 3, Math.round(A.Y(d)) - 3);
  c.textAlign = 'left';
  // the track as it plays: its own scale, -72 .. -6 dBFS over the height
  const LO = -72, HI = -6;
  const lv = ui.specLv || (ui.specLv = new Float32Array(SPEC_N)), pk = ui.specPk || (ui.specPk = new Float32Array(SPEC_N).fill(LO));
  if (ui.specAt !== now) {
    const dt = Math.min(0.1, (now - (ui.specAt || now)) / 1000); ui.specAt = now;
    for (let b = 0; b < SPEC_N; b++) { const v = ui.spec ? ui.spec[b] : 0; lv[b] = Math.max(LO, v > 1e-6 ? 20 * Math.log10(v) : -120); pk[b] = Math.max(lv[b], pk[b] - dt * 20); }
  }
  const sY = d => A.y0 + (1 - (clamp(d, LO, HI) - LO) / (HI - LO)) * (A.y1 - A.y0), xa = A.X(40), xb = A.X(16000), sn = Math.max(2, Math.round(xb - xa));
  const sAt = (a, i) => sY(specAt(a, i / sn));
  // (silent: no line along the foot)
  let loud = false; for (let b = 0; b < SPEC_N; b++) if (pk[b] > LO + 1) { loud = true; break; }
  if (loud) {
  c.beginPath(); c.moveTo(xa, A.y1);
  for (let i = 0; i <= sn; i++) c.lineTo(xa + i * (xb - xa) / sn, sAt(lv, i));
  c.lineTo(xb, A.y1); c.closePath();
  const gr = c.createLinearGradient(0, A.y0, 0, A.y1); gr.addColorStop(0, col.ink); gr.addColorStop(1, col.bg);
  c.globalAlpha = 0.22; c.fillStyle = gr; c.fill(); c.globalAlpha = 0.5;
  c.beginPath(); for (let i = 0; i <= sn; i++) { const x = xa + i * (xb - xa) / sn, y = sAt(lv, i); i ? c.lineTo(x, y) : c.moveTo(x, y); }
  c.strokeStyle = col.ink; c.lineWidth = 1; c.stroke();
  c.globalAlpha = 0.4; c.setLineDash([1, 3]); c.beginPath();
  for (let i = 0; i <= sn; i += 2) { const x = xa + i * (xb - xa) / sn, y = sAt(pk, i); i ? c.lineTo(x, y) : c.moveTo(x, y); }
  c.stroke(); c.setLineDash([]); c.globalAlpha = 1;
  }
  // the response: the filter and the EQ added up, and each on its own when both shape it
  const cv = tzCurves(w, A), path = (k, from) => { c.beginPath(); for (let i = 0; i <= cv.n; i++) { const y = Yu(from(i)); i ? c.lineTo(cv.xs[i], y) : c.moveTo(cv.xs[i], y); } };
  lcdClip(c, A.x0 - 1, A.y0 - 1, A.x1 - A.x0 + 2, A.y1 - A.y0 + 2);
  if (cv.gh) { path(0, i => cv.gh[i] + cv.eq[i]); c.strokeStyle = col.dim; c.lineWidth = 1; c.setLineDash([1, 2]); c.stroke(); c.setLineDash([]); }
  if (cv.fShape && cv.eShape) {
    c.strokeStyle = col.dim; c.lineWidth = 1; c.setLineDash([4, 3]);
    path(0, i => cv.fl[i]); c.stroke(); path(0, i => cv.eq[i]); c.stroke(); c.setLineDash([]);
  }
  const tot = i => cv.fl[i] + cv.eq[i], y0 = A.Y(0);
  path(0, tot); c.lineTo(cv.xs[cv.n], y0); c.lineTo(cv.xs[0], y0); c.closePath();
  c.globalAlpha = 0.1; c.fillStyle = col.ink; c.fill(); c.globalAlpha = 1;
  path(0, tot); c.lineJoin = 'round';
  c.globalAlpha = 0.16; c.strokeStyle = col.ink; c.lineWidth = 6; c.stroke(); c.globalAlpha = 1;
  c.lineWidth = 2; c.stroke();
  c.restore();
  // the handles: rings on their own curves (filled when they act; those of the tab shown in ink, the others dim; the
  // one held red), their names
  const tab = tgOf('FLT') ? tgCur(tgOf('FLT')) : (PAGES[ui.page] && PAGES[ui.page].id), drag = ui.tzDrag && ui.tzDrag.moved ? ui.tzDrag.id : null;
  const hs = tzHandles(), nodes = [];
  const boxes = [];
  for (const n of hs) {
    // (HP and LP off rest a little inside the ends, clear of the filter's handle when it is wide open)
    const rest = !n.on && (n.id === 'HP' || n.id === 'LP') ? 26 : 7;
    const x = clamp(A.X(n.f), A.x0 + rest, A.x1 - rest), y = clamp(A.Y(n.db), top - 6, bot + 6), cur = n.mod === tab, held = drag === n.id, hot = held || ui.tzHot === n.id;
    nodes.push({ ...n, x, y });
    const r = hot ? 7.5 : 5.5, ink = held ? col.red : cur ? col.ink : col.dim;
    if (hot) { c.globalAlpha = 0.18; c.fillStyle = ink; c.beginPath(); c.arc(x, y, r + 6, 0, Math.PI * 2); c.fill(); c.globalAlpha = 1; }
    c.beginPath(); c.arc(x, y, r, 0, Math.PI * 2);
    c.fillStyle = held ? col.red : n.on ? ink : col.bg; c.fill();
    c.lineWidth = 1.5; c.strokeStyle = ink; c.stroke();
    c.fillStyle = cur || hot ? col.ink : col.dim;
    // its name above and beside it, below when another name is there already
    const tw = c.measureText(n.lab).width, right = x + r + 4 + tw > A.x1 - 2, lx = right ? x - r - 4 - tw : x + r + 4;
    let ly = y - r - 1;
    if (boxes.some(b => lx < b[2] + 4 && lx + tw > b[0] - 4 && ly - 8 < b[3] && ly > b[1])) ly = y + r + 9;
    boxes.push([lx, ly - 8, lx + tw, ly]);
    c.fillText(n.lab, lx, ly);
  }
  ui.tz = { A, nodes };
  // what the handle held (or under the pointer) is at; else the filter's type
  const sh = nodes.find(n => n.id === (drag || ui.tzHot));
  if (sh) {
    const txt = TZ_PS[sh.id].map(p => { const m = paramMeta(ui.t, p), v = shownValue(p).v; return m.l + ' ' + (m.f ? m.f(v) : fPct(v)); }).join(' · ');
    lcdTag(c, col, txt, 4, 3, drag ? 'inv' : 'box');
  } else lcdTag(c, col, 'FILTER ' + hs[0].type + (hs[0].on ? '' : ' · OPEN'), 4, 3, 'txt', false, col.dim);
}
// the visualiser's handles: grabbed (the nearest within a fingertip), dragged, let go; a double tap resets one
function buildToneViz() {
  const cv = $('tspec');
  // (the plugin's panel is scaled: back to the canvas's own pixels)
  const pos = e => { const r = cv.getBoundingClientRect(), k = cv.clientWidth / (r.width || 1); return [(e.clientX - r.left) * k, (e.clientY - r.top) * k]; };
  const hit = (x, y, touch) => {
    const z = ui.tz; if (!z || trackView() !== 'TVIZ') return null;
    let best = null, bd = touch ? 24 : 13;
    for (const n of z.nodes) { const d = Math.hypot(n.x - x, n.y - y); if (d < bd) { bd = d; best = n; } }
    return best;
  };
  cv.addEventListener('pointerdown', e => {
    const [x, y] = pos(e), n = hit(x, y, e.pointerType && e.pointerType !== 'mouse');
    if (!n) return;
    e.preventDefault(); backend.ensureAudio();
    try { cv.setPointerCapture(e.pointerId); } catch (err) {}
    const now = performance.now();
    if (ui.tzTap && ui.tzTap.id === n.id && now - ui.tzTap.t < 320) { ui.tzTap = null; ui.tzDrag = null; tzReset(n.id); return; }
    ui.tzTap = { id: n.id, t: now };
    ui.tzDrag = { id: n.id, dx: n.x - x, dy: n.y - y, x0: x, y0: y, moved: false };
    ui.tzHot = n.id;
    // its module's tab (TONE on its main page)
    if (n.mod === 'TONE' && ui.tonemore) setToneMore(false);
    if (tgOf(n.mod)) setRet(n.mod);
    if (PAGES[ui.page] !== PAGE_BY_ID[n.mod]) { ui.page = PAGES.indexOf(PAGE_BY_ID[n.mod]); renderAll(); }
  });
  cv.addEventListener('pointermove', e => {
    const [x, y] = pos(e), d = ui.tzDrag;
    if (!d) { const n = hit(x, y, false); ui.tzHot = n ? n.id : null; cv.style.cursor = n ? 'grab' : ''; return; }
    if (!d.moved && Math.hypot(x - d.x0, y - d.y0) < 2) return;
    d.moved = true; cv.style.cursor = 'grabbing';
    const A = ui.tz.A, X = clamp(x + d.dx, A.x0, A.x1), Y = y + d.dy;
    tzSet(d.id, tzF((X - A.x0) / (A.x1 - A.x0)), A.dbAt(Y));
    refreshAfterKnob({ glob: false, p: TZ_PS[d.id][0] });
  });
  const up = e => { if (!ui.tzDrag) return; ui.tzDrag = null; cv.style.cursor = ''; if (e && e.pointerType && e.pointerType !== 'mouse') ui.tzHot = null; };
  cv.addEventListener('pointerup', up); cv.addEventListener('pointercancel', up);
  cv.addEventListener('pointerleave', () => { if (!ui.tzDrag) { ui.tzHot = null; cv.style.cursor = ''; } });
}

// ---- LFO: the two LFOs side by side, one cycle each, the wave scaled by its depth (dashed: the full swing), the phase
// in red with the value it gives; destination and rate in tags (trig waves restart on each hit: '·T')
// a wave across the view (u: 0..1). S&H and SLEW: six draws for the drawing (SLEW gliding SLW of each); LOOP: its own
// loop, LEN draws of its seed, as the engine plays them (o: the LFO's SLW, LEN, seed and salt)
const shDraw = k => { const r = Math.sin((k + 1) * 12.9898) * 43758.5453; return 2 * (r - Math.floor(r)) - 1; };
function lfoShape(shape, ph, o) {
  switch (shape) {
    case 0: return Math.sin(2 * Math.PI * ph);
    case 1: return 1 - 4 * Math.abs(ph - 0.5);
    case 2: return 2 * ph - 1;
    case 3: return ph < 0.5 ? 1 : -1;
    case 5: return Math.pow(2, -6 * ph);
    case W_SLEW: {
      const k = Math.min(5, Math.floor(ph * 6)), f = ph * 6 - k, a = shDraw(k - 1), b = shDraw(k), sl = o ? o.slw : 0.5;
      if (sl <= 0.001) return b;
      const x = Math.min(f / sl, 1);
      return a + (b - a) * x * x * (3 - 2 * x);
    }
    case W_LOOP: return o ? loopDraw(o.seed, o.salt, Math.min(o.len - 1, Math.floor(ph * o.len))) : shDraw(Math.floor(ph * 8));
    default: return shDraw(Math.floor(ph * 6));   // S&H (for the drawing)
  }
}
function drawLfoView(c, col, w, h) {
  const gap = 6, fw = (w - gap) / 2, m = ui.mod, t = ui.t, names = destNames(curMachine());
  // (under the MOD page's tabs, the LFO shown has its name reversed, the other one framed)
  const tg = tgOf('LFO1'), hl = !tg ? -1 : tgCur(tg) === 'LFO2' ? 1 : 0;
  for (let l = 0; l < 2; l++) {
    const x0 = Math.round(l * (fw + gap)), W = Math.round(fw);
    const ps = l ? [P.L2SP, P.L2DP, P.L2DS, P.L2WV] : [P.LSPD, P.LDEP, P.LDST, P.LWAV];
    const v = q => shownValue(q).v;
    const wv = idx(v(ps[3]), NWAV), shape = wv % NSHAPE, dep = (v(ps[1]) - 0.5) * 2, dest = idx(v(ps[2]), NDEST);
    const o = { slw: v(l ? P.L2SL : P.L1SL), len: LFO_LEN[idx(v(l ? P.L2LN : P.L1LN), NLEN)], seed: v(l ? P.L2SD : P.L1SD), salt: l + 1 };
    const ph = m ? m[t * 8 + 2 * l] : 0, val = m ? m[t * 8 + 2 * l + 1] : lfoShape(shape, 0, o);
    lcdGrid(c, col, x0, 0, W, h, [0.25, 0.5, 0.75], [0.25, 0.75]);
    lcdFrame(c, col, x0, 0, W, h, [0.25, 0.5, 0.75], [0.5]);
    const X = u => x0 + 2 + u * (W - 5), uni = shape === 5;
    const Y = y => 2 + (uni ? 1 - y : 0.5 - y * 0.5) * (h - 5);
    const n = Math.max(2, Math.round(W - 4));
    lcdClip(c, x0, 0, W, h);
    // the full swing, dashed; the wave as the depth plays it
    c.setLineDash([2, 2]); c.strokeStyle = col.dim; c.lineWidth = 1; c.beginPath();
    for (let i = 0; i <= n; i++) { const u = i / n, y = Y(lfoShape(shape, u, o)); i ? c.lineTo(X(u), y) : c.moveTo(X(u), y); }
    c.stroke(); c.setLineDash([]);
    if (dest) {
      c.beginPath();
      // S&H: the value held for this cycle; the others: the wave as the depth plays it (LOOP: its whole loop)
      const at = u => shape === 4 ? val : lfoShape(shape, u, o);
      for (let i = 0; i <= n; i++) { const u = i / n, y = Y(uni ? at(u) * Math.abs(dep) : at(u) * dep); i ? c.lineTo(X(u), y) : c.moveTo(X(u), y); }
      c.strokeStyle = col.ink; c.lineWidth = 1.6; c.stroke();
      const xp = Math.round(X(clamp(ph, 0, 1))) + 0.5, yp = Y(uni ? val * Math.abs(dep) : val * dep);
      c.strokeStyle = col.red; c.lineWidth = 1; c.beginPath(); c.moveTo(xp, 1); c.lineTo(xp, h - 1); c.stroke();
      c.fillStyle = col.red; c.beginPath(); c.arc(xp, yp, 2.6, 0, Math.PI * 2); c.fill();
    }
    c.restore();
    const tw = lcdTag(c, col, 'LFO' + (l + 1), x0 + 4, 4, hl < 0 || hl === l ? 'inv' : 'box');
    lcdTag(c, col, dest ? names[dest] : 'OFF', x0 + 6 + tw, 4, 'box', false, dest ? col.ink : col.dim);
    lcdTag(c, col, LFO_SPD[idx(v(ps[0]), 16)] + (shape === W_LOOP ? ' ×' + o.len : '') + (wv >= NSHAPE ? ' ·T' : ''), x0 + W - 4, 4, 'box', true);
    if (dest) lcdTag(c, col, fB(v(ps[1])), x0 + W - 4, h - 15, 'txt', true, col.dim);
  }
}

// ---- envelope follower: the band's input level (dashed) and the envelope (line over a faint fill) scrolling from the
// right over the last EF_WIN ms, the trigger threshold in red when it fires an envelope; source, band, mode and
// destination in tags, the envelope now as a bar on the right edge
const EF_WIN = 4000;
function drawEfView(c, col, w, h, e) {
  const b = e ? P.E2SR : P.E1SR, v = q => shownValue(q).v, names = destNames(curMachine());
  const src = EF_SRC[idx(v(b), EF_SRC.length)], band = EF_BAND[idx(v(b + 2), 4)], trg = v(b + 3), dest = idx(v(b + 7), NDEST);
  const hist = (ui.efHist && ui.efHist[e]) || [], barW = 10, W = w - barW - 4;
  lcdGrid(c, col, 0, 0, W, h, [0.2, 0.4, 0.6, 0.8], [0.25, 0.5, 0.75]);
  lcdFrame(c, col, 0, 0, W, h, [0.2, 0.4, 0.6, 0.8], [0.25, 0.5, 0.75]);
  const Y = y => 2 + (1 - clamp(y, 0, 1)) * (h - 5), tNow = performance.now(), X = t => W - 3 - (tNow - t) / EF_WIN * (W - 5);
  const pts = hist.filter(p => tNow - p[2] <= EF_WIN + 100);
  lcdClip(c, 0, 0, W, h);
  if (pts.length > 1) {
    c.setLineDash([2, 2]); c.strokeStyle = col.dim; c.lineWidth = 1; c.beginPath();
    pts.forEach((p, i) => { const x = X(p[2]), y = Y(p[1]); i ? c.lineTo(x, y) : c.moveTo(x, y); });
    c.stroke(); c.setLineDash([]);
    c.beginPath();
    pts.forEach((p, i) => { const x = X(p[2]), y = Y(p[0]); i ? c.lineTo(x, y) : c.moveTo(x, y); });
    c.strokeStyle = col.ink; c.lineWidth = 1.5; c.stroke();
    c.lineTo(X(pts[pts.length - 1][2]), h); c.lineTo(X(pts[0][2]), h); c.closePath(); c.globalAlpha = 0.1; c.fillStyle = col.ink; c.fill(); c.globalAlpha = 1;
  }
  if (efTrgOn(trg)) {
    const y = Math.round(Y(Math.pow(10, efTrgDb(trg) / 20))) + 0.5;
    c.setLineDash([4, 3]); c.strokeStyle = col.red; c.lineWidth = 1; c.beginPath(); c.moveTo(1, y); c.lineTo(W - 1, y); c.stroke(); c.setLineDash([]);
  }
  c.restore();
  // the envelope now
  const now = pts.length ? pts[pts.length - 1][0] : 0, bx = w - barW;
  lcdFrame(c, col, bx, 0, barW, h, null, [0.25, 0.5, 0.75]);
  c.fillStyle = dest ? col.ink : col.dim; const bh = Math.round(clamp(now, 0, 1) * (h - 4)); c.fillRect(bx + 2, h - 2 - bh, barW - 4, bh);
  const tw = lcdTag(c, col, 'EF' + (e + 1), 4, 4, 'inv');
  const tw2 = lcdTag(c, col, src + ' · ' + band, 6 + tw, 4, 'box');
  lcdTag(c, col, efTrgOn(trg) ? 'TRG ' + Math.round(efTrgDb(trg)) + 'dB' : 'FOLLOW', 8 + tw + tw2, 4, 'box', false, efTrgOn(trg) ? col.red : col.ink);
  lcdTag(c, col, dest ? '→ ' + names[dest] + ' ' + fB(v(b + 6)) : '→ OFF', W - 4, h - 15, 'box', true, dest ? col.ink : col.dim);
}

// (the BOX's screen and its pictograms of v50 went in v70: the BOX is a module of TONE, the visualiser above it)
// MASTER: a small LCD over the knobs (the chain, the reverb algorithms, the MIX display) and LED meters
let gsKey = '';
function renderGStrip() {
  const gs = $('gstrip'), pg = PAGES[ui.gpage], pid = pg.id;
  const order = idx(state.g[G.DPOS], 2) === 0 ? ['EQ', 'DRV', 'COMP', 'GRN', 'OTT', 'GLCH'] : ['EQ', 'COMP', 'DRV', 'GRN', 'OTT', 'GLCH'];
  const a = revAlgo();
  const key = [pid, order.join(), a, ui.engine, ui.gmod ? 1 : 0, ui.wpick ? 1 : 0, Object.values(PAGE_SW).flat().map(([, g]) => swOn(g)).join(),
    pid === 'GRN' ? [G.GMED, G.GSYN, G.GFRZ, G.GLWV].map(g => state.g[g]).join() : pid === 'GLCH' ? [G.XCMD, G.XLMD, G.XHLD, G.XANA, G.XAMD].map(g => state.g[g]).join() : ''].join('|');
  if (key === gsKey && gs.firstChild) {
    // the readouts follow their values (a kit, a project, the host may move them)
    const xs = pid === 'GLCH' && $('xspd'); if (xs) { const t = GP[G.XSPD].f(state.g[G.XSPD]); if (xs.textContent !== t) xs.textContent = t; }
    return;
  }
  gsKey = key; vizKeys.mviz = '';
  const meter = (id, l, cls, v) => `<div class="meter"><span class="ml">${l}</span><div class="bar ${cls}"><i id="${id}"></i></div><b id="${id}v">${v}</b></div>`;
  const out = meter('mout', 'OUT', 'out', '−∞');
  // the page's ON keys, at the end of its screen: a lit pixel when the effect is in
  const sw = PAGE_SW[pid] || [];
  const onKeys = sw.map(([name, g], i) => { const on = swOn(g);
    return `<button class="mon${on ? ' on' : ''}" id="${i ? 'mon' + (i + 1) : 'mon'}" data-g="${g}" aria-pressed="${on}" title="Switch ${name} in or out">`
      + `<i></i>${sw.length > 1 ? name : on ? 'ON' : 'OFF'}</button>`; }).join('');
  // the plugin's FX page keeps the output meter on every master page
  const plugOut = inPlugin ? `<div class="meters">${out}</div>` : '';
  const bindOn = () => gs.querySelectorAll('.mon').forEach(bt => { bt.onclick = () => { const g = +bt.dataset.g; setGlobal(g, swOn(g) ? 0 : 1); ui.lastP = -1 - g; renderTabs(); renderScreen(); renderGStrip(); }; });
  if (pg.master) {
    // the chain: EQ › COMP › DRV (or DRV before COMP) › GRAIN › OTT, what is off is crossed out
    const chain = order.map(x => `<button class="${x === pid ? 'on' : ''}${chainOn(x) ? '' : ' byp'}" data-pg="${x}">${x}</button>`).join('<i>›</i>');
    let rows = '';
    if (pid === 'GRN') {
      const grp = (lab, g, names) => `<span class="ggrp"><i>${lab}</i>${names.map((n, i) => `<button class="${idx(state.g[g], names.length) === i ? 'on' : ''}" data-g="${g}" data-i="${i}">${n}</button>`).join('')}</span>`;
      rows = `<div class="chain grain">${grp('MEDIA', G.GMED, GRAIN_MEDIA)}${grp('CLK', G.GSYN, GRAIN_SYNC)}`
        + `<span class="ggrp"><button class="gfrz${state.g[G.GFRZ] > 0.5 ? ' on' : ''}" id="gfrz" aria-pressed="${state.g[G.GFRZ] > 0.5}">FREEZE</button></span>`
        + `<span class="ggrp"><i>RAND</i><b class="grnd" id="grnd" tabindex="0" role="slider" aria-label="Grain randomness" title="Randomness from grain to grain: drag up or down">${fPct(state.g[G.GRND])}</b></span></div>`;
      // the LFO: its wave (tap it for the 64), rate, phase, and MOD: the knobs become its attenuverters
        rows += `<div class="chain glfo"><span class="ggrp"><i>LFO</i><canvas class="gwave" id="gwave" aria-label="LFO wave: tap to choose"></canvas></span>`
          + `<span class="ggrp"><i>WAVE</i><b class="grnd gro" id="glwv" tabindex="0" role="slider" aria-label="LFO wave" title="Drag, or tap the wave for all of them">${GP[G.GLWV].f(state.g[G.GLWV])}</b></span>`
          + `<span class="ggrp"><i>RATE</i><b class="grnd gro" id="glrt" tabindex="0" role="slider" aria-label="LFO rate">${GP[G.GLRT].f(state.g[G.GLRT])}</b></span>`
          + `<span class="ggrp"><i>PHAS</i><b class="grnd gro" id="glph" tabindex="0" role="slider" aria-label="LFO phase">${GP[G.GLPH].f(state.g[G.GLPH])}</b></span>`
          + `<span class="ggrp"><button class="gmod${ui.gmod ? ' on' : ''}" id="gmod" aria-pressed="${!!ui.gmod}" title="The knobs set how much the LFO moves each control">MOD</button></span></div>`;
      // the 64 waves: a panel over the knobs (the strip keeps its size)
      if (ui.wpick) {
        const w = idx(state.g[G.GLWV], 64), data = lfoData();
        const thumb = i => { let d = ''; for (let j = 0; j <= 32; j++) { const v = data[i * 256 + Math.min(255, j * 8)] / 127; d += (j ? 'L' : 'M') + (j * 1.5).toFixed(1) + ' ' + (7 - v * 6).toFixed(1); } return `<svg viewBox="0 0 48 14" aria-hidden="true"><path d="${d}"/></svg>`; };
        rows += `<div class="wpick" role="listbox" aria-label="LFO waves">` + IMPAKT_LFO.families.map((f, r) => `<span class="wfam">${f}</span>`
          + [0, 1, 2, 3, 4, 5, 6, 7].map(c => { const i = r * 8 + c; return `<button class="${i === w ? 'on' : ''}" data-w="${i}" role="option" aria-selected="${i === w}" title="${IMPAKT_LFO.names[i]}">${thumb(i)}<b>${IMPAKT_LFO.names[i]}</b></button>`; }).join('')).join('') + `</div>`;
      }
    } else if (pid === 'GLCH') {
      // GLITCH: the corrupt modes, LOSS digital (the codec's modes) or analog (the tapes), the loss speed (or wow /
      // flutter), HOLD; then the last divisions of the grid
      const grp = (lab, g, names) => `<span class="ggrp">${lab ? `<i>${lab}</i>` : ''}${names.map((n, i) => `<button class="${idx(state.g[g], names.length) === i ? 'on' : ''}" data-g="${g}" data-i="${i}" data-n="${names.length}">${n}</button>`).join('')}</span>`;
      const ana = state.g[G.XANA] > 0.5;
      const da = `<span class="xana" role="group" aria-label="Loss: digital or analog">${['DIG', 'ANA'].map((n, i) => `<button class="${(i === 1) === ana ? 'on' : ''}" data-ana="${i}" aria-pressed="${(i === 1) === ana}" title="${i ? 'Analog: a tape (wow, flutter, saturation, failures, hiss)' : 'Digital: a codec'}">${n}</button>`).join('')}</span>`;
      const lg = ana ? G.XAMD : G.XLMD, ln = ana ? GL_AMODES : GL_LMODES;
      const lm = ln.map((n, i) => `<button class="${idx(state.g[lg], ln.length) === i ? 'on' : ''}" data-g="${lg}" data-i="${i}" data-n="${ln.length}">${n}</button>`).join('');
      rows = `<div class="chain grain glch">${grp('CRPT', G.XCMD, GL_CMODES)}<span class="ggrp xloss"><i>LOSS</i>${da}${lm}</span></div>`
        + `<div class="chain glfo glch"><span class="ggrp"><i>SPD</i><b class="grnd" id="xspd" tabindex="0" role="slider" aria-label="${ana ? 'Wow or flutter' : 'Loss speed'}" title="${ana ? 'Tape: wow (slow) to flutter (fast), drag up or down' : 'Loss speed: smeared to chopped, drag up or down'}">${GP[G.XSPD].f(state.g[G.XSPD])}</b></span>`
        + `<span class="ggrp"><button class="gfrz${state.g[G.XHLD] > 0.5 ? ' on' : ''}" id="xhld" aria-pressed="${state.g[G.XHLD] > 0.5}" title="Loop the last division (tap again to let go)">HOLD</button></span>`
        + `<canvas class="xviz" id="xviz" aria-label="The last divisions of the grid"></canvas></div>`;
    } else if (pid === 'OTT') {
      rows = `<div class="meters">${meter('ottl', 'LOW', 'gr ott', '0.0')}${meter('ottm', 'MID', 'gr ott', '0.0')}${meter('otth', 'HIGH', 'gr ott', '0.0')}${out}</div>`;
    } else rows = `<div class="meters">${meter('mgr', 'GR', 'gr', '0.0')}${out}</div>`;
    const inRows = pid === 'GRN' || pid === 'GLCH';
    const viz = inRows || pid === 'OTT' ? '' : `<canvas class="mviz" id="mviz" aria-hidden="true"></canvas>`;
    gs.innerHTML = `<div class="sl"><span class="mlab" title="${ui.engine === 'js' ? 'JS mode, no oversampling' : 'oversampled 4×'}"><b>${ui.engine === 'js' ? '1×' : '4×'}</b>OS</span><span class="chain">${chain}</span>${onKeys}${inRows ? rows : viz}</div>`
      + (inRows ? plugOut : rows);
    bindOn();
    gs.querySelectorAll('.chain button[data-pg]').forEach(bt => { bt.onclick = () => { ui.gpage = PAGES.findIndex(p => p.id === bt.dataset.pg); ui.wpick = false; renderAll(); }; });
    if (pid === 'GRN') bindGrainStrip();
    if (pid === 'GLCH') bindGlitchStrip();
  } else if (pid === 'MIX') {
    // what the background helpers are doing right now: unmasking per track and band, calmed resonances
    gs.innerHTML = `<div class="sl"><span class="mlab"><b>MIX</b>${ui.engine === 'js' ? 'OFF' : ''}</span><canvas class="mixcv" id="mixcv" aria-label="Current reductions"></canvas>${onKeys}</div>${plugOut}`;
    bindOn();
  }
}
// FX: the returns' screens: REVERB's (its six algorithms, its ON key, the impulse response), DELAY's (v68: its two models,
// the 1982's bandwidth, HOLD, its ON key, its echoes), CHORUS's (its four modes, its ON key, the two lines' sweep) and
// PHASER's (its ON key, the notches moving)
let fxsKey = '';
function renderFxStrips() {
  const rs = $('ks-REV'), ds = $('ks-DLY'), cs = $('ks-CHO'), ps = $('ks-PHS'); if (!rs || !ds || !cs || !ps) return;
  const a = revAlgo(), dm = idx(state.g[G.DMDL], 2), bw = idx(state.g[G.DBW], 2), cm = idx(state.g[G.CHMD], 4);
  const key = [a, ui.engine, swOn(G.RVON), swOn(G.DLON), swOn(G.CHON), swOn(G.PHON), dm, bw, swOn(G.DHLD), cm, idx(state.g[G.PRTE], 3)].join('|');
  if (key === fxsKey && rs.firstChild) return;
  fxsKey = key; vizKeys.rviz = vizKeys.dviz = vizKeys.cviz = vizKeys.pviz = '';
  const onKey = (id, name, g) => { const on = swOn(g);
    return `<button class="mon${on ? ' on' : ''}" id="${id}" data-g="${g}" aria-pressed="${on}" title="Switch ${name} in or out"><i></i>${on ? 'ON' : 'OFF'}</button>`; };
  const btns = REV_ALGOS.map((n, i) => `<button class="${i === a ? 'on' : ''}" data-a="${i}"${i === REV_SPECTRAL && ui.engine === 'js' ? ' disabled' : ''}>${n}</button>`).join('');
  rs.innerHTML = `<div class="sl"><span class="mlab rvl"><b>REVERB</b>${NREV} ALGOS</span><span class="chain algos">${btns}</span>${onKey('rmon', 'the reverb', G.RVON)}<canvas class="mviz" id="rviz" aria-hidden="true"></canvas></div>`;
  const sel = (g, list, cur, n) => list.map((x, i) => `<button class="${i === cur ? 'on' : ''}" data-g="${g}" data-i="${i}" data-n="${n}"${g === G.DBW && dm === 0 ? ' disabled' : ''}>${x}</button>`).join('');
  const hold = swOn(G.DHLD);
  ds.innerHTML = `<div class="sl"><span class="mlab"><b>DELAY</b>STEREO</span><span class="chain algos dmodl">${sel(G.DMDL, DLY_MODELS, dm, 2)}</span>`
    + `<span class="chain algos dbw" title="The 1982's bandwidth: x1 16 kHz, x2 6 kHz">${sel(G.DBW, ['×1', '×2'], bw, 2)}</span>`
    + `<button class="mon hold${hold ? ' on' : ''}" id="dhold" data-g="${G.DHLD}" aria-pressed="${hold}" title="HOLD: the repeats loop as they are, nothing comes in"><i></i>HOLD</button>`
    + `${onKey('dmon', 'the delay', G.DLON)}<canvas class="mviz" id="dviz" aria-hidden="true"></canvas></div>`;
  cs.innerHTML = `<div class="sl"><span class="mlab"><b>CHORUS</b>2 LINES</span><span class="chain algos cmode">${sel(G.CHMD, ['MODE 1', 'MODE 2', 'MODE 3', 'MODE 4'], cm, 4)}</span>`
    + `${onKey('cmon', 'the chorus', G.CHON)}<canvas class="mviz" id="cviz" aria-hidden="true"></canvas></div>`;
  const rt = idx(state.g[G.PRTE], 3);
  ps.innerHTML = `<div class="sl"><span class="mlab"><b>PHASER</b>2 × 6 STAGES</span><span class="chain algos proute">${sel(G.PRTE, PH_ROUTES, rt, 3)}</span>`
    + `${onKey('pmon', 'the phaser', G.PHON)}<canvas class="mviz" id="pviz" aria-hidden="true"></canvas></div>`;
  for (const host of [rs, ds, cs, ps]) host.querySelectorAll('.mon').forEach(bt => {
    bt.onclick = () => { const g = +bt.dataset.g; setGlobal(g, swOn(g) ? 0 : 1); ui.lastP = -1 - g; renderTabs(); renderKnobs(); renderScreen(); renderFxStrips(); };
  });
  rs.querySelectorAll('.algos button').forEach(bt => {
    bt.onclick = () => { setGlobal(G.RALG, mid(+bt.dataset.a, NREV)); ui.lastP = -1 - G.RALG; renderKnobs(); renderScreen(); renderFxStrips(); };
  });
  for (const host of [ds, cs, ps]) host.querySelectorAll('.algos button[data-g]').forEach(bt => {
    bt.onclick = () => { const g = +bt.dataset.g; setGlobal(g, mid(+bt.dataset.i, +bt.dataset.n)); ui.lastP = -1 - g; renderKnobs(); renderScreen(); renderFxStrips(); };
  });
}
// GLITCH's strip: the corrupt / loss mode keys, the loss speed to drag, HOLD
function bindGlitchStrip() {
  const gs = $('gstrip');
  gs.querySelectorAll('.glch button[data-g]').forEach(bt => {
    bt.onclick = () => { const g = +bt.dataset.g; setGlobal(g, mid(+bt.dataset.i, +bt.dataset.n)); ui.lastP = -1 - g; renderKnobs(); renderScreen(); renderGStrip(); };
  });
  gs.querySelectorAll('.glch button[data-ana]').forEach(bt => {
    bt.onclick = () => { setGlobal(G.XANA, +bt.dataset.ana); ui.lastP = -1 - G.XANA; renderKnobs(); renderScreen(); renderGStrip(); };
  });
  $('xhld').onclick = () => { setGlobal(G.XHLD, state.g[G.XHLD] > 0.5 ? 0 : 1); ui.lastP = -1 - G.XHLD; renderScreen(); renderGStrip(); };
  const el = $('xspd');
  dragReadout(el, () => state.g[G.XSPD], v => { setGlobal(G.XSPD, clamp(v, 0, 1)); el.textContent = GP[G.XSPD].f(state.g[G.XSPD]); ui.lastP = -1 - G.XSPD; renderScreen(); }, 1 / 150);
  el.addEventListener('dblclick', () => { setGlobal(G.XSPD, gDefault(G.XSPD)); el.textContent = GP[G.XSPD].f(state.g[G.XSPD]); });
}
// GRAIN's strip: media / clock buttons, freeze, the readouts to drag (RAND, the LFO's wave, rate and phase), MOD
function bindGrainStrip() {
  const gs = $('gstrip');
  gs.querySelectorAll('.grain button[data-g]').forEach(bt => {
    bt.onclick = () => { const g = +bt.dataset.g; setGlobal(g, mid(+bt.dataset.i, 4)); ui.lastP = -1 - g; renderKnobs(); renderScreen(); renderGStrip(); };
  });
  $('gfrz').onclick = () => { setGlobal(G.GFRZ, state.g[G.GFRZ] > 0.5 ? 0 : 1); ui.lastP = -1 - G.GFRZ; renderScreen(); renderGStrip(); };
  const ro = (id, g, perPx) => {
    const el = $(id); if (!el) return;
    dragReadout(el, () => state.g[g], v => { setGlobal(g, clamp(v, 0, 1)); el.textContent = gMeta(g).f(state.g[g]); ui.lastP = -1 - g; renderScreen(); if (g === G.GLWV) gsWave = -1; }, perPx);
    el.addEventListener('dblclick', () => { setGlobal(g, gDefault(g)); el.textContent = gMeta(g).f(state.g[g]); gsWave = -1; });
  };
  ro('grnd', G.GRND, 1 / 150); ro('glwv', G.GLWV, 1 / 640); ro('glrt', G.GLRT, 1 / 240); ro('glph', G.GLPH, 1 / 240);
  const wv = $('gwave');
  if (wv) wv.onclick = () => { ui.wpick = !ui.wpick; renderGStrip(); };
  gs.querySelectorAll('.wpick button').forEach(bt => {
    bt.onclick = () => { setGlobal(G.GLWV, mid(+bt.dataset.w, 64)); ui.lastP = -1 - G.GLWV; ui.wpick = false; gsWave = -1; renderScreen(); renderGStrip(); };
  });
  const md = $('gmod');
  if (md) md.onclick = () => { ui.gmod = !ui.gmod; renderKnobs(); renderScreen(); renderGStrip(); };
}
// GRAIN's LFO on its screen (the wave, where it is), OTT's gain per band
let gsWave = -1;
function drawFx(now) {
  const pid = PAGES[ui.gpage].id;
  if (pid === 'GRN') {
    const cv = $('gwave'); if (!cv) return;
    const w = cv.clientWidth, h = cv.clientHeight; if (!w) return;
    const dpr = DPR();
    if (cv.width !== Math.round(w * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); }
    if (!ovwColors || now - ovwColorsAt > 1000) { ovwColors = readColors(); ovwColorsAt = now; }
    const col = ovwColors, c = cv.getContext('2d'), wave = idx(state.g[G.GLWV], 64), data = lfoData(), uni = IMPAKT_LFO.flags[wave] & 1;
    c.setTransform(dpr, 0, 0, dpr, 0, 0); c.clearRect(0, 0, w, h);
    const y = v => uni ? h - 3 - v * (h - 6) : h / 2 - v * (h / 2 - 3);
    lcdGrid(c, col, 0, 0, w, h, [0.25, 0.5, 0.75], [(Math.round(y(0)) + 0.5) / h]);
    lcdFrame(c, col, 0, 0, w, h, [0.25, 0.5, 0.75], [0.5]);
    lcdClip(c, 0, 0, w, h);
    c.strokeStyle = col.ink; c.lineWidth = 1.2; c.beginPath();
    for (let x = 1; x <= w - 1; x++) { const v = data[wave * 256 + Math.min(255, Math.floor((x - 1) / (w - 2) * 256))] / 127; x > 1 ? c.lineTo(x, y(v)) : c.moveTo(x, y(v)); }
    c.stroke(); c.restore();
    const fx = ui.fx;
    if (fx && fx[5] > 0.5) {
      const px = 1 + fx[0] * (w - 2);
      c.fillStyle = col.red; c.fillRect(Math.round(px), 1, 1, h - 2);
      c.beginPath(); c.arc(px + 0.5, y(fx[1]), 2.4, 0, Math.PI * 2); c.fill();
    }
  } else if (pid === 'GLCH') drawGlitchViz(now);
  else if (pid === 'OTT') {
    const fx = ui.fx;
    [['ottl', 2], ['ottm', 3], ['otth', 4]].forEach(([id, k]) => {
      const el = $(id); if (!el) return;
      const g = fx && fx[6] > 0.5 ? fx[k] : 0;
      el.style.clipPath = g < 0 ? `inset(0 0 0 ${(100 - clamp(-g / 30, 0, 1) * 100).toFixed(1)}%)` : `inset(0 ${(100 - clamp(g / 30, 0, 1) * 100).toFixed(1)}% 0 0)`;
      el.parentNode.classList.toggle('lift', g > 0);
      $(id + 'v').textContent = Math.abs(g) < 0.05 ? '0.0' : (g > 0 ? '+' : '−') + Math.abs(g).toFixed(1);
    });
  }
}
// GLITCH's screen: the last 16 divisions of the grid, oldest on the left, each drawn as what it did (a straight line
// when it passed through; REPEAT: as many bars as repeats; JUMP: an arrow back; DROP: an empty cell; BEND: the
// speed as a slope, backwards as a falling one, a tape stop as a curve to the floor; HOLD: a loop), the one playing
// with its progress; the codec's bandwidth and RAND in tags
const GL_TYPE_NAME = ['', 'RPT', 'JUMP', 'DROP', 'BEND', 'HOLD'];
function drawGlitchViz(now) {
  const cv = $('xviz'); if (!cv) return;
  const w = cv.clientWidth, h = cv.clientHeight; if (!w || !h) return;
  const dpr = DPR();
  if (cv.width !== Math.round(w * dpr) || cv.height !== Math.round(h * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); }
  if (!ovwColors || now - ovwColorsAt > 1000) { ovwColors = readColors(); ovwColorsAt = now; }
  const col = ovwColors, c = cv.getContext('2d'), gi = ui.glitch, on = state.g[G.XON] > 0.5, ink = on ? col.ink : col.dim;
  c.setTransform(dpr, 0, 0, dpr, 0, 0); c.clearRect(0, 0, w, h);
  // the tags on top, the cells under them
  const N = 16, cw = (w - 2) / N, top = 14, y0 = top + 2, ch = h - top - 4;
  lcdFrame(c, col, 0, top, w, h - top, Array.from({ length: N - 1 }, (_, i) => (i + 1) / N), null);
  for (let k = 0; k < N; k++) {
    const t = gi ? gi[k] : -1, x = 1 + k * cw, cx = x + cw / 2, cur = k === N - 1;
    c.strokeStyle = cur ? col.red : ink; c.fillStyle = cur ? col.red : ink; c.lineWidth = 1.2;
    c.beginPath();
    if (t === 0) { c.moveTo(x + 2, y0 + ch / 2); c.lineTo(x + cw - 2, y0 + ch / 2); }
    else if (t === 1) {
      const n = cur && gi ? Math.max(2, Math.round(gi[17])) : 3, bw = (cw - 4) / n;
      for (let j = 0; j < n; j++) c.rect(x + 2 + j * bw + 0.5, y0 + 3.5, Math.max(1, bw - 1.5), ch - 7);
    } else if (t === 2) { c.moveTo(x + cw - 3, y0 + ch / 2); c.lineTo(x + 3, y0 + ch / 2); c.moveTo(x + 6, y0 + ch / 2 - 3); c.lineTo(x + 3, y0 + ch / 2); c.lineTo(x + 6, y0 + ch / 2 + 3); }
    else if (t === 3) { c.rect(x + 3.5, y0 + 3.5, cw - 7, ch - 7); }
    else if (t === 4) {
      const r = cur && gi ? gi[17] : 1.5;
      if (r === 0) { c.moveTo(x + 2, y0 + 3); c.quadraticCurveTo(x + cw * 0.6, y0 + 3, x + cw - 2, y0 + ch - 3); }
      else if (r < 0) { c.moveTo(x + 2, y0 + 3); c.lineTo(x + cw - 2, y0 + ch - 3); }
      else { c.moveTo(x + 2, y0 + ch - 3); c.lineTo(x + cw - 2, y0 + 3); }
    } else if (t === 5) { c.ellipse(cx, y0 + ch / 2, cw / 2 - 3, ch / 2 - 4, 0, 0, Math.PI * 2); }
    if (t === 1 || t === 3) c.stroke(); else c.stroke();
    if (cur && gi && t >= 0) c.fillRect(Math.round(x + gi[16] * (cw - 1)), y0, 1, ch);
  }
  // tags: what plays now, the codec's bandwidth (or the tape, its high cut and its speed error)
  const tags = [];
  if (gi && gi[N - 1] > 0) tags.push(GL_TYPE_NAME[gi[N - 1]] + (gi[N - 1] === 1 ? ' ×' + Math.round(gi[17]) : gi[N - 1] === 4 ? (gi[17] === 0 ? ' STOP' : gi[17] < 0 ? ' REV' : ' ×' + gi[17]) : gi[N - 1] === 2 ? ' −' + Math.round(gi[17]) : ''));
  if (gi && gi[18] > 0) {
    if (gi[20] > 0.5) { const ct = Math.round(gi[21] || 0); tags.push(GL_AMODES[idx(state.g[G.XAMD], 5)] + ' ' + fKHz(gi[18]) + ' ' + (ct > 0 ? '+' : ct < 0 ? '−' : '±') + Math.abs(ct) + '¢'); }
    else tags.push('LOSS ' + fKHz(gi[18]));
  }
  let tx = 0;
  for (const t of tags) tx += lcdTag(c, col, t, tx, 0, tx ? 'box' : 'inv') + 3;
}
function renderSteps() {
  const pt = !!ui.ptn, kl = !pt && ui.key ? keyLayout() : null, rs = !pt && !kl && recSide();
  $('steps').classList.toggle('recside', rs);
  $('steps').classList.toggle('keys', !!kl);
  $('steps').classList.toggle('fold', !!(kl && kl.fold));
  $('steps').classList.toggle('ptns', pt);
  steps.forEach(b => { for (const c of ['pcur', 'pnext', 'pused', 'pchn', 'parm']) b.classList.remove(c); });
  if (pt) { renderPatKeys(); renderPages(); return; }
  steps.forEach((b, i) => {
    const n = kl ? kl.keys[i] : null, sn = b.querySelector('.sn');
    b.classList.toggle('noff', !!kl && n == null);
    b.classList.toggle('nblk', !!kl && !kl.fold && n != null && i < 8);
    b.classList.toggle('nroot', !!kl && n != null && (kl.brk ? n % 4 === 0 : ((n - (kl.fold ? state.qroot || 0 : 0)) % 12 + 12) % 12 === 0));
    const txt = kl ? (n == null ? '' : kl.brk ? String(n + 1) : noteName(n)) : String(stepAt(i) + 1);
    if (sn.textContent !== txt) sn.textContent = txt;
  });
  renderPages();
  if (kl) { steps.forEach((b, s) => { const n = kl.keys[s]; for (const c of ['on', 'other', 'lk', 'tl', 'sel', 'out', 'rat']) b.classList.remove(c); b.firstChild.innerHTML = ''; b.dataset.r = '1'; b.setAttribute('aria-pressed', 'false'); b.setAttribute('aria-label', n == null ? 'No note' : kl.brk ? 'Slice ' + (n + 1) : 'Note ' + noteName(n) + (n >= 0 ? ' +' : ' ') + n); }); return; }
  // RAM: on its REC side the keys show the REC trigs (and the trigs as a mark), on its PLAY side the other way round
  const tr = trk(), ram = mof(tr.p[P.MACH]) === RAM_M;
  steps.forEach((b, i) => {
    const s = stepAt(i), st = tr.steps[s], on = rs ? !!st.rec : !!st.on;
    b.classList.toggle('on', on);
    b.classList.toggle('other', ram && (rs ? !!st.on : !!st.rec));
    b.classList.toggle('lk', Object.keys(st.l).length > 0);
    b.classList.toggle('tl', !rs && !st.on && !!st.tl);
    b.classList.toggle('sel', !!(ui.lock && ui.lock.has(s)));
    b.classList.toggle('out', s >= tr.len);
    const r = rs ? 1 : st.r || 1;
    if (b.dataset.r !== String(r)) {
      b.dataset.r = String(r);
      b.classList.toggle('rat', r > 1);
      b.firstChild.innerHTML = r > 1 ? '<i></i>'.repeat(r) : '';
    }
    b.setAttribute('aria-pressed', on ? 'true' : 'false');
    b.setAttribute('aria-label', `Step ${s + 1}` + (rs ? ', REC trig' : '') + (!rs && !st.on && st.tl ? ', sound change only' : '')
      + (r > 1 ? `, ratchet ×${r}` : '') + (ui.lock && ui.lock.has(s) ? ', up for p-locks' : ''));
  });
}
function renderFooter() {
  const tr = trk();
  $('lenv').textContent = tr.len;
  $('lenbar').setAttribute('aria-valuenow', String(tr.len));
  [...$('lencells').children].forEach((c, i) => c.classList.toggle('on', i < tr.len - (ui.spage | 0) * NS));
  $('spdv').textContent = SPEEDS[tr.spd];
  $('rndu').disabled = !ui.undo;
  $('rndd').textContent = Math.round(tr.p[P.SPC] * 100) + '%';
  renderLive();
}
// LIVE (the temporary random): its arc, its value
function renderLive() {
  const el = $('rlive'); if (!el) return;
  const v = clamp(state.g[G.XRND], 0, 1), a = A0 + SWEEP * v;
  el.classList.toggle('on', v > 0.004);
  el.querySelector('.trk').setAttribute('d', arcPath(25, A0, A0 + SWEEP));
  el.querySelector('.val').setAttribute('d', v > 0.004 ? arcPath(25, A0, a) : '');
  const [x2, y2] = polar(11, a);
  const ind = el.querySelector('.ind'); ind.setAttribute('x2', x2.toFixed(2)); ind.setAttribute('y2', y2.toFixed(2));
  ind.setAttribute('x1', polar(3, a)[0].toFixed(2)); ind.setAttribute('y1', polar(3, a)[1].toFixed(2));
  $('rlivev').textContent = Math.round(v * 100) + '%';
  el.setAttribute('aria-valuenow', String(Math.round(v * 100)));
}
function buildLive() {
  const el = $('rlive');
  const set = v => { setGlobal(G.XRND, clamp(v, 0, 1)); renderLive(); };
  let d = null, lastUp = 0;
  el.addEventListener('pointerdown', e => { e.preventDefault(); backend.ensureAudio(); try { el.setPointerCapture(e.pointerId); } catch (err) {} d = { y: e.clientY, x: e.clientX, base: state.g[G.XRND], moved: false }; });
  el.addEventListener('pointermove', e => {
    if (!d) return;
    const dy = d.y - e.clientY + (e.clientX - d.x) * 0.25;
    if (!d.moved && Math.abs(dy) < 3) return;
    d.moved = true; set(d.base + dy * (e.shiftKey ? 1 / 600 : 1 / 150));
  });
  const up = () => {
    if (!d) return;
    const moved = d.moved; d = null;
    if (moved) return;
    const now = performance.now();
    if (now - lastUp < 320) { set(0); lastUp = 0; } else lastUp = now;     // a double tap: back to the original sounds
  };
  el.addEventListener('pointerup', up); el.addEventListener('pointercancel', () => { d = null; });
  el.addEventListener('keydown', e => {
    const k = e.key, s = k === 'ArrowUp' || k === 'ArrowRight' ? 0.01 : k === 'ArrowDown' || k === 'ArrowLeft' ? -0.01 : k === 'PageUp' ? 0.1 : k === 'PageDown' ? -0.1 : 0;
    if (k === 'Home' || k === 'Delete' || k === 'Backspace') { e.preventDefault(); set(0); return; }
    if (s) { e.preventDefault(); set(state.g[G.XRND] + s); }
  });
}
function renderTransport() {
  $('play').classList.toggle('on', ui.playing || !!ui.pos.host);
  $('rec').classList.toggle('on', !!ui.rec); $('rec').setAttribute('aria-pressed', String(!!ui.rec));
  $('app').classList.toggle('recording', !!ui.rec);
  const host = !!ui.pos.host;
  $('bpm').classList.toggle('host', host);
  $('bpmv').textContent = host && ui.pos.bpm ? Math.round(ui.pos.bpm) : Math.round(state.g[G.BPM]);
  $('swgv').textContent = Math.round(50 + state.g[G.SWING] * 25) + '%';
  renderLoop();
}
// ------------------------------------------------------------------ sequence looper (EP-133 style)
// LOOP folds the song position into a window: LEN sets its size (1/128 .. 2 bars, stutters under a
// step), POS slides it by steps. The real clock keeps running, so leaving the loop lands in time.
const LOOP_LENS = [0.125, 0.25, 0.5, 1, 2, 3, 4, 6, 8, 12, 16, 32];
const loopOn = () => state.g[G.LOOP] > 0.5;
function setLoop(on) { setGlobal(G.LOOP, on ? 1 : 0); renderLoop(); renderStepsLoop(true); }
function renderLoop() {
  const on = loopOn();
  $('loopbtn').classList.toggle('on', on);
  $('loopbtn').setAttribute('aria-pressed', String(on));
  $('llenv').textContent = LOOP_L[idx(state.g[G.LLEN], 12)];
  $('lposv').textContent = GP[G.LPOS].f(state.g[G.LPOS]);
}
// the window the engine reports (or, when off, the one LOOP would catch now)
function loopWindow() {
  const info = ui.loop;
  const len = LOOP_LENS[idx(state.g[G.LLEN], 12)], off = idx(state.g[G.LPOS], 33) - 16;
  if (info && info[0] > 0.5) return { on: true, start: info[1], len: info[2], eff: info[3], real: info[4] };
  const real = info ? info[4] : 0, q = Math.max(len, 1);
  return { on: false, start: Math.max(0, Math.floor(real / q + 1e-9) * q + off), len, eff: real, real };
}
let lpKey = '';
function renderStepsLoop(force) {
  const w = loopWindow(), run = ui.pos.run, host = $('steps');
  const active = w.on && run;
  const tr = trk(), sp = [0.125, 0.25, 0.5, 0.75, 1, 1.5, 2][tr.spd];
  const key = active ? [ui.t, tr.len, tr.spd, w.start.toFixed(3), w.len, ui.spage | 0, !!ui.ptn, !!ui.key].join() : 'off';
  if (key === lpKey && !force) return;
  lpKey = key;
  host.classList.toggle('looping', active);
  const inW = new Set();
  if (active) {
    const a = Math.floor(w.start * sp + 1e-6), b = Math.max(a + 1, Math.ceil((w.start + w.len) * sp - 1e-6));
    for (let k = a; k < b && k < a + tr.len; k++) inW.add(((k % tr.len) + tr.len) % tr.len);
  }
  steps.forEach((b, i) => b.classList.toggle('lpin', !ui.ptn && !ui.key && inW.has(stepAt(i))));
}
// one bar of 16 cells: the window (filled), the effective playhead, and where the song really is
function drawLoopMap() {
  const cv = $('lmap'), w = cv.clientWidth, h = cv.clientHeight;
  if (!w || !h) return;
  const dpr = DPR();
  if (cv.width !== Math.round(w * dpr) || cv.height !== Math.round(h * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); }
  const now = performance.now();
  if (!ovwColors || now - ovwColorsAt > 1000) { ovwColors = readColors(); ovwColorsAt = now; }
  const col = ovwColors, c = cv.getContext('2d');
  c.setTransform(dpr, 0, 0, dpr, 0, 0); c.clearRect(0, 0, w, h);
  const L = loopWindow(), run = !!ui.pos.run, pad = 6, gap = 2, cw = (w - pad * 2 - gap * 15 - 6) / 16, y0 = 9, ch = h - 18;
  const X = i => pad + i * (cw + gap) + Math.floor(i / 4) * 2;
  const cellIn = i => { if (L.len >= 16) return true; const d = ((i - L.start) % 16 + 16) % 16; return d < Math.max(L.len, 1) - 1e-6 || (L.len < 1 && d < 1); };
  for (let i = 0; i < 16; i++) {
    const inside = cellIn(i);
    c.globalAlpha = inside && !L.on ? 0.38 : 1;
    c.fillStyle = inside ? col.ink : col.lo;
    c.fillRect(X(i), y0, cw, ch);
  }
  // stutter: tick marks inside the step for each repeat
  if (L.len < 1) {
    const i = ((Math.floor(L.start) % 16) + 16) % 16, n = Math.round(1 / L.len);
    c.globalAlpha = 0.9; c.fillStyle = col.bg;
    for (let k = 1; k < n; k++) c.fillRect(X(i) + cw * k / n - 0.5, y0, 1, ch);
  }
  c.globalAlpha = 1;
  if (run) {
    const pe = ((L.eff % 16) + 16) % 16, pr = ((L.real % 16) + 16) % 16;
    const px = p => X(Math.floor(p)) + cw * (p - Math.floor(p));
    if (L.on) { c.strokeStyle = col.dim; c.lineWidth = 1; c.strokeRect(px(pr) - 2, 3, 4, h - 6); }   // the song, underneath
    c.fillStyle = col.bg; c.fillRect(px(pe) - 2.5, 2, 5.5, h - 4);
    c.fillStyle = col.ink; c.fillRect(px(pe) - 1, 3, 2.5, h - 6);
  }
}
// phone: two pages, MACHINE + SEQ and MOD + FX
function renderPTabs() {
  $('app').dataset.tab = ui.ptab;
  document.querySelectorAll('#ptabs .ptab').forEach(b => { const on = b.dataset.tab === ui.ptab; b.classList.toggle('cur', on); b.setAttribute('aria-pressed', String(on)); });
}
function buildPTabs() {
  document.querySelectorAll('#ptabs .ptab').forEach(b => b.addEventListener('click', () => setPTab(b.dataset.tab)));
}
// a module's page, shown with it in focus (the track's screen on its view; on MASTR, the page shown)
function openModule(id) {
  const pg = PAGE_BY_ID[id];
  const tab = Object.keys(KMODS).find(t => KMODS[t].some(([m]) => m === id)) || (pg && MASTER_PAGES.includes(pg) ? 'mt' : null);
  if (!tab) return;
  if (tab === 'mt' && MASTER_PAGES.includes(pg)) { ui.gpage = PAGES.indexOf(pg); ui.wpick = false; }
  ui.ptab = tab; ui.picker = false;
  const tg = tgOf(id); if (tg) ui[tg.key] = id;
  if (pg && !pg.glob) ui.page = PAGES.indexOf(pg);
  renderAll();
}
// a page of the bar: SEQ, TONE, MOD, FX, MASTR (the track's screen starts on the page's first module)
function setPTab(tab) {
  if (!PTABS.some(([t]) => t === tab)) return;
  // (on MOD and TONE with their tabs: the module shown)
  const tfirst = { md: 'LFO1', tn: 'FLT' }[tab], foc = tfirst && tgOf(tfirst) ? tgCur(tgOf(tfirst)) : PT_FOCUS[tab];
  if (tab !== ui.ptab && foc) ui.page = PAGES.indexOf(PAGE_BY_ID[foc]);
  ui.ptab = tab;
  if (tab !== 'ms') ui.picker = false;
  renderAll();
}
// the engine sends the oscillators of the track shown on its PRSM side (none otherwise)
function syncPrism() {
  const want = prismSide() ? ui.t : -1;
  if (want !== ui.prsmT) { ui.prsmT = want; send(['prsm', want]); }
}
// HALO: the engine sends the chord and the voices of the track shown (none otherwise)
const haloShown = () => !ui.all && (curMachine() === HALO_M || curMachine() === STRUM_M || curMachine() === CHOIR_M || curMachine() === HOWL_M || curMachine() === MANTRA_M);
const choirShown = () => !ui.all && curMachine() === CHOIR_M;
const mantraShown = () => !ui.all && curMachine() === MANTRA_M;
function syncHalo() {
  const want = haloShown() ? ui.t : -1;
  if (want !== ui.haloT) { ui.haloT = want; send(['halo', want]); }
}
// BREAK: the engine sends the break of the track shown and the slice playing (none otherwise)
const brkShown = () => !ui.all && curMachine() === BREAK_M;
function syncBreak() {
  const want = brkShown() ? ui.t : -1;
  if (want !== ui.brkT) { ui.brkT = want; send(['brk', want]); }
}
// FX: the engine sends what the FX track shown is doing (none otherwise)
const fxmShown = () => !ui.all && fxM(curMachine());
function syncFxm() {
  const want = fxmShown() ? ui.t : -1;
  if (want !== ui.fxmT) { ui.fxmT = want; send(['fxm', want]); }
}
function renderAll() {
  syncPrism(); syncHalo(); syncBreak(); syncFxm(); syncChim(); syncCube(); syncWest(); syncRadio();
  // CHIMERA: the bank's sounds, the first time a track (or a pattern) plays it
  if (!ui.bankAsked && (state.tracks.some(t => mof(t.p[P.MACH]) === CHIM_M) || state.pats.some(pt => pt && pt.tracks.some(t => mof(t.p[P.MACH]) === CHIM_M)))) {
    ui.bankAsked = true; if (backend.bankLoad) backend.bankLoad();
  }
  renderPTabs(); renderPads(); renderChips(); renderQuant(); renderWestPanel(); renderScreen(); renderPresets(); renderTabs(); renderKnobs(); renderLockbar(); renderTStrip(); renderGStrip(); renderFxStrips(); renderSteps(); renderFooter(); renderTransport(); renderKits(); renderLoops(); renderRadioPanel();
}

// pattern overview (wide screens): 8 rows x 16 steps, per-track playheads
const ovw = $('ovw');
let ovwColors = null, ovwColorsAt = -1e9;
function readColors() {
  const cs = getComputedStyle(document.documentElement);
  const v = n => cs.getPropertyValue(n).trim();
  return { ink: v('--lcd-ink'), dim: v('--lcd-dim'), bg: v('--lcd'), lo: v('--lcd-lo'), red: v('--led-r'), font: '8px ' + (v('--f-px') || 'monospace') };
}
function drawOverview(now) {
  const w = ovw.clientWidth, h = ovw.clientHeight;
  if (!w || !h) return;
  if (!ovwColors || now - ovwColorsAt > 1000) { ovwColors = readColors(); ovwColorsAt = now; }
  const col = ovwColors, dpr = DPR();
  const W = Math.round(w * dpr), H = Math.round(h * dpr);
  if (ovw.width !== W || ovw.height !== H) { ovw.width = W; ovw.height = H; }
  const c = ovw.getContext('2d');
  c.setTransform(dpr, 0, 0, dpr, 0, 0);
  c.clearRect(0, 0, w, h);
  if (!ui.all && curMachine() === RAM_M) { if (prismSide()) drawPrism(c, col, w, h); else drawRam(c, col, w, h, now); return; }
  if (choirShown()) { drawChoir(c, col, w, h, now); return; }
  if (haloShown() && curMachine() === HOWL_M) { drawHowl(c, col, w, h, now); return; }
  if (mantraShown()) { drawMantra(c, col, w, h, now); return; }
  if (haloShown() && curMachine() === STRUM_M) { drawStrum(c, col, w, h, now); return; }
  if (haloShown()) { drawHalo(c, col, w, h); return; }
  if (brkShown()) { drawBreak(c, col, w, h); return; }
  if (fxmShown()) { drawFxm(c, col, w, h, now); return; }
  if (chimShown()) { drawChim(c, col, w, h, now); return; }
  if (cubeShown()) { drawCube(c, col, w, h); return; }
  if (westShown()) { drawWest(c, col, w, h, now); return; }
  if (radioShown()) { drawRadio(c, col, w, h, now); return; }
  const gap = 2, lab = 12, cols = NS, rows = NT;
  const cw = (w - lab - gap * (cols - 1) - 6) / cols, rh = (h - gap * (rows - 1)) / rows;
  for (let t = 0; t < rows; t++) {
    const tr = state.tracks[t], y = t * (rh + gap), sel = t === ui.t || ui.all;
    c.globalAlpha = 1; c.fillStyle = sel ? col.ink : col.dim;
    c.fillRect(0, y, sel ? 8 : 3, rh);
    const ph = ui.pos.run ? ui.pos.steps[t] : -1, pg = (ui.spage | 0) * NS;
    for (let c0 = 0; c0 < cols; c0++) {
      const x = lab + c0 * (cw + gap) + Math.floor(c0 / 4) * 2, s = pg + c0;
      const st = tr.steps[s];
      if (s >= tr.len) continue;
      if (st.on) { c.globalAlpha = tr.mute ? 0.25 : sel ? 1 : 0.6; c.fillStyle = col.ink; }
      else { c.globalAlpha = 1; c.fillStyle = col.lo; }
      c.fillRect(x, y, cw, rh);
      if (st.on && st.r > 1) {
        c.globalAlpha = 1; c.fillStyle = col.bg;
        for (let k = 1; k < st.r; k++) c.fillRect(x + (cw * k) / st.r - 0.5, y + rh * 0.45, 1, rh * 0.55);
      }
      if (st.on && Object.keys(st.l).length) {
        c.globalAlpha = 1; c.fillStyle = col.bg;
        c.beginPath(); c.moveTo(x + cw, y); c.lineTo(x + cw - Math.min(5, cw * 0.5), y); c.lineTo(x + cw, y + Math.min(5, rh * 0.7)); c.fill();
      }
      if (s === ph) { c.globalAlpha = 1; c.strokeStyle = col.ink; c.lineWidth = 1.5; c.strokeRect(x + 0.75, y + 0.75, cw - 1.5, rh - 1.5); }
    }
  }
  c.globalAlpha = 1;
}

// RAM on the machine's screen: the take the track plays (its own on the REC side) as a waveform, the window its PLAY
// trigs read bright and the rest dim, the slices as dashed lines; a take being recorded draws itself, its head in red
// HALO's screen: the chord's name, the progression's step and the scale on top; under them the voices on a piano roll
// (C1 to C7): a bar per note, as high as it sounds, the copy an octave up as it fades in (SPRD, INV), the root marked;
// the scale's notes along the bottom
function haloChordName(notes, shape) {
  // (the screen's font is all capitals: MIN / MAJ spelled out, -5 for a flat fifth)
  if (!notes.length) return '—';
  const r = notes[0], root = noteName(r);
  if (shape >= 9) return root + ' ' + HALO_SHAPES[shape];
  const s = new Set(notes.map(n => ((n - r) % 12 + 12) % 12)), has = x => s.has(x);
  const min = has(3) && !has(4), maj = has(4);
  const b7 = has(10), m7 = has(11) && !b7, ext = (min || maj) ? (has(5) ? 11 : has(2) ? 9 : 0) : 0;
  let q;
  if (!min && !maj) {
    q = has(5) && has(2) ? 'SUS2/4' : has(5) ? 'SUS4' : has(2) ? 'SUS2' : has(7) ? '5' : '';
    if (b7) q = '7' + q; else if (m7) q = 'MAJ7' + q;
  } else {
    const dim = min && has(6) && !has(7);
    if (b7 || m7) q = (min ? 'MIN' : '') + (m7 ? (min ? '(MAJ' + (ext || 7) + ')' : 'MAJ' + (ext || 7)) : String(ext || 7)) + (dim ? '-5' : '');
    else if (dim) q = 'DIM';
    else if (has(9)) q = (min ? 'MIN' : '') + (has(2) ? '6/9' : '6');
    else q = (min ? 'MIN' : 'MAJ') + (ext === 9 ? ' ADD9' : ext === 11 ? ' ADD11' : '');
    if (maj && has(8) && !has(7)) q += '+5';
    if (has(1)) q += '(-9)';
  }
  return root + (q ? ' ' + q : '');
}
// CHIMERA's screen. PLAY and FAMILY: the bank as a map (brightness across, length up), the sample chosen as a square,
// the neighbours VARI draws from ringed, the one the last hit played lit in red; its name on top. GRAFT: the sound the
// track makes, on a log time axis (1 ms .. its end): the attack's outline up to the seam, the body's after it (what is
// not heard, dim), the seam in red. While the bank loads (or when it is not there): what is going on
const chimShown = () => !ui.all && curMachine() === CHIM_M;
function syncChim() {
  const want = chimShown() ? ui.t : -1;
  if (want !== ui.chimT) { ui.chimT = want; send(['chm', want]); }
}
// the n nearest to sample i (itself first), as the engine finds them (core/sk_bank.cpp bankNear): for the screen
function chimNear(i, sens, same, n) {
  const E = CHIM.e, out = [i];
  if (!E[i] || n < 2) return out;
  const sh = sens > 0 ? 1 - 0.85 * sens : 1, tb = sens < 0 ? 1 + 0.85 * sens : 1, W = [sh, sh, tb, tb, tb, tb, tb, 0.3];
  const ds = [];
  for (let j = 0; j < E.length; j++) {
    if (j === i || (same && E[j].cat !== E[i].cat)) continue;
    let d = 0; for (let f = 0; f < 8; f++) { const x = E[j].feat[f] - E[i].feat[f]; d += W[f] * x * x; }
    if (!same && E[j].cat !== E[i].cat) d += 1;
    ds.push([d, j]);
  }
  ds.sort((a, b) => a[0] - b[0]);
  for (let q = 0; q < Math.min(n - 1, ds.length); q++) out.push(ds[q][1]);
  return out;
}
function drawChim(c, col, w, h, now) {
  const sv = q => shownValue(q).v, hi = ui.chim, page = side3(ui.t);
  const a = chimAt(sv(P.A), sv(P.B)), cats = chimCats();
  const bc = idx(sv(P.S1), cats.length + 1), b = bc ? chimAt(mid(bc - 1, cats.length), sv(P.S2)) : -1;
  lcdFrame(c, col, 0, 0, w, h, [0.25, 0.5, 0.75], [0.5]);
  // the bank: not in the page, loading, refused
  const st = hi ? Math.round(hi[0]) : 0, done = hi ? Math.round(hi[1]) : 0, n = CHIM.n;
  let msg = '';
  if (!n) msg = 'NO BANK IN THIS BUILD';
  else if (ui.chimErr) msg = 'BANK NOT FOUND · ' + ui.chimErr;
  else if (st !== 2) msg = st === 1 ? `BANK · DECODING ${done}/${n}` : ui.chimFetch != null && ui.chimFetch < 1 ? `BANK · LOADING ${Math.round(ui.chimFetch * 100)}%` : 'BANK · WAITING FOR THE AUDIO';
  else if (hi && hi[3] > 0) msg = `BANK · ${Math.round(hi[3])} SAMPLES REFUSED`;
  const name = a >= 0 ? CHIM.e[a].name : '—';
  if (page === 1) {
    // GRAFT: the outlines on a log time axis, 1 ms .. the longer one's end
    const L = i => i >= 0 ? CHIM.e[i].frames / CHIM.e[i].sr : 0;
    const T = Math.max(0.05, L(a), b >= 0 ? L(b) : 0), X0 = 6, X1 = w - 6, Y0 = 16, Y1 = h - 14;
    const tx = t => X0 + (X1 - X0) * Math.log(Math.max(t, 0.001) / 0.001) / Math.log(T / 0.001);
    const envAt = (i, t) => {
      const e = CHIM.e[i], Ls = Math.max(L(i), 0.002); if (t > Ls) return 0;
      const k = t <= 0.001 ? 0 : Math.min(63, Math.floor(63 * Math.log(t / 0.001) / Math.log(Ls / 0.001)) + 1);
      return e.env[k] / 255;
    };
    const seam = 0.001 * Math.pow(150, sv(P.S3)), fade = 0.0005 * Math.pow(80, sv(P.S4)), split = sv(P.S5) >= 0.02;
    lcdGrid(c, col, 0, 0, w, h, [0.25, 0.5, 0.75], [0.5]);
    const mid0 = (Y0 + Y1) / 2, amp = (Y1 - Y0) / 2 - 1;
    const lane = (i, from, to, ink, alpha) => {
      if (i < 0) return;
      c.globalAlpha = alpha; c.fillStyle = ink;
      for (let x = Math.floor(tx(from)); x <= Math.ceil(tx(to)) && x < X1; x++) {
        const t = 0.001 * Math.pow(T / 0.001, (x - X0) / (X1 - X0)), v = envAt(i, t) * amp;
        if (v > 0.4) c.fillRect(x, Math.round(mid0 - v), 1, Math.max(1, Math.round(2 * v)));
      }
      c.globalAlpha = 1;
    };
    if (b >= 0) {
      lane(a, seam + fade, T, col.dim, 0.5); lane(b, 0.001, seam, col.dim, 0.5);
      lane(a, 0.001, seam + fade * 0.5, col.ink, 1); lane(b, seam + fade * 0.5, T, col.ink, 1);
      if (split) lane(a, seam, T, col.ink, 0.45);
      const xs = Math.round(tx(seam)) + 0.5;
      c.strokeStyle = col.red; c.lineWidth = 1; c.beginPath(); c.moveTo(xs, 12); c.lineTo(xs, h - 12); c.stroke();
      lcdTag(c, col, 'SEAM ' + fTime(seam), Math.min(xs + 3, w - 70), h - 13, 'txt', false, col.red);
    } else lane(a, 0.001, T, col.ink, 1);
    const tw = lcdTag(c, col, 'A', 4, 3, 'inv');
    lcdTag(c, col, name, 6 + tw, 3, 'box');
    if (b >= 0) { const tb = lcdTag(c, col, 'B', w - 4, 3, 'inv', true); lcdTag(c, col, CHIM.e[b].name, w - 6 - tb, 3, 'box', true); }
    else lcdTag(c, col, 'NO BODY · BODY ON THIS PAGE', w - 4, 3, 'box', true, col.dim);
    if (split) lcdTag(c, col, 'SPLIT ' + fChimHz(sv(P.S5)), w - 4, h - 13, 'txt', true, col.dim);
    if (msg) lcdTag(c, col, msg, 4, h - 13, 'inv');
    return;
  }
  // PLAY, FAMILY: the map
  const X0 = 8, X1 = w - 8, Y0 = 18, Y1 = h - 14, px = e => X0 + e.mx * (X1 - X0), py = e => Y1 - e.my * (Y1 - Y0);
  lcdGrid(c, col, 0, 0, w, h, [0.25, 0.5, 0.75], [0.5]);
  const ca = a >= 0 ? CHIM.e[a].cat : -1;
  for (const e of CHIM.e) {
    c.globalAlpha = e.cat === ca ? 0.9 : 0.35; c.fillStyle = e.cat === ca ? col.ink : col.dim;
    c.fillRect(Math.round(px(e)) - 1, Math.round(py(e)) - 1, 2, 2);
  }
  c.globalAlpha = 1;
  // the neighbours VARI draws from (the engine's, after a hit of this sample; until then the page's own count)
  const vr = sv(P.C) - 0.5, N = 1 + Math.round(Math.abs(vr) * 2 * 11);
  let cand = [];
  if (a >= 0 && Math.abs(vr) > 0.005) {
    const tgt = idx(sv(P.S10), 3);
    const base = tgt === 1 && b >= 0 ? b : a;
    if (hi && Math.round(hi[9]) > 0 && Math.round(hi[10]) === base) cand = hi.slice(10, 10 + Math.round(hi[9])).map(Math.round);
    else cand = chimNear(base, (sv(P.S8) - 0.5) * 2, idx(sv(P.S9), 2) === 1, N);
  }
  c.strokeStyle = col.ink; c.lineWidth = 1;
  for (const j of cand.slice(1)) { const e = CHIM.e[j]; if (!e) continue; c.strokeRect(Math.round(px(e)) - 3.5, Math.round(py(e)) - 3.5, 7, 7); }
  if (b >= 0) { const e = CHIM.e[b]; c.strokeStyle = col.ink; c.lineWidth = 1.5; c.beginPath(); c.arc(px(e), py(e), 5, 0, Math.PI * 2); c.stroke(); }
  if (a >= 0) { const e = CHIM.e[a]; c.fillStyle = col.ink; c.fillRect(Math.round(px(e)) - 3, Math.round(py(e)) - 3, 6, 6); }
  // the last hit: the sample it played, lit for a moment
  if (hi && hi[6] !== ui.chimHits) { ui.chimHits = hi[6]; ui.chimLit = now; }
  const lastA = hi ? Math.round(hi[4]) : -1;
  if (lastA >= 0 && CHIM.e[lastA] && now - (ui.chimLit || 0) < 240) {
    const e = CHIM.e[lastA]; c.fillStyle = col.red; c.fillRect(Math.round(px(e)) - 3, Math.round(py(e)) - 3, 6, 6);
  }
  const tw = lcdTag(c, col, a >= 0 ? cats[CHIM.e[a].cat] : '—', 4, 3, 'inv');
  lcdTag(c, col, name, 6 + tw, 3, 'box');
  lcdTag(c, col, 'VARI ' + fVari(sv(P.C)), w - 4, 3, 'box', true, Math.abs(vr) > 0.005 ? col.ink : col.dim);
  lcdTag(c, col, msg || 'DARK ← BRIGHT → · LONG ↑', msg ? 4 : w - 4, h - 13, msg ? 'inv' : 'txt', !msg, msg ? undefined : col.dim);
}
function drawHalo(c, col, w, h) {
  const sv = q => shownValue(q).v, hi = ui.halo, on = hi && hi[0] > 0.5;
  const n = on ? Math.round(hi[1]) : 0, notes = on ? Array.from({ length: n }, (_, k) => hi[2 + k]) : [];
  const shape = on ? Math.round(hi[8]) : idx(sv(P.A), 12), pr = idx(sv(P.B), NPROG);
  let x = lcdTag(c, col, on ? haloChordName(notes, shape) : HALO_SHAPES[shape], 0, 0, 'inv') + 4;
  // (playing along with the chord track the quantiser follows: no progression of its own)
  const alongT = idx(sv(P.HMOD), 12) === 0 && trk().q !== 0 && qFollow() >= 0 && qFollow() !== ui.t;
  if (pr && !alongT) x += lcdTag(c, col, HALO_PROGS[pr] + (on ? (pr === PROG_WALK ? ' ' + (hi[10] > 0 ? '+' : '') + Math.round(hi[10]) : ' ' + (Math.round(hi[10]) + 1) + '/' + HALO_PROG_LEN[pr]) : ''), x, 0, 'box') + 4;
  const harm = idx(sv(P.HARM), 3);
  if (harm) x += lcdTag(c, col, HARMS[harm], x, 0, 'txt') + 2;
  const mode = idx(sv(P.HMOD), 12), tonic = ((Math.round((sv(P.TUNE) - 0.5) * 48) % 12) + 12) % 12;
  // (MODE KEY under a quantiser that follows another chord track: its chords are built in that track's)
  const fol = qFollow(), along = mode === 0 && trk().q !== 0 && fol >= 0 && fol !== ui.t;
  const keyed = mode === 0 && (state.scale || along) && trk().q !== 0;
  lcdTag(c, col, along ? 'CHORDS T' + (fol + 1) : keyed ? scaleName(state.scale, state.qroot || 0) : NOTE_NAMES[tonic] + ' ' + HALO_MODES[mode || 1], w, 0, along ? 'inv' : 'box', true);
  const top = 14, H = h - top, LO = -24, HI = 48, X = st => 1 + (st - LO) / (HI - LO) * (w - 3);
  const octs = Array.from({ length: 5 }, (_, i) => (i + 1) / 6);
  lcdFrame(c, col, 0, top, w, H, octs, null);
  lcdClip(c, 0, top, w, H);
  // the scale along the bottom (the notes the chords are built from)
  const mask = along ? qLive() : keyed ? state.scale : (() => { const rel = [1453, 1453, 1709, 1451, 2773, 1717, 2741, 2477, 1193, 1365, 397, 2925][mode]; return ((rel << tonic) | (rel >> (12 - tonic))) & 0xFFF; })();
  const sw = (w - 3) / (HI - LO);
  for (let st = LO; st < HI; st++) {
    const pc = ((st % 12) + 12) % 12;
    c.fillStyle = (mask >> pc) & 1 ? col.dim : col.lo;
    c.fillRect(Math.round(X(st)), top + H - 4, Math.max(1, Math.round(sw) - 1), 3);
  }
  // the voices
  const bw = Math.max(2, Math.round(sw) - 1), base = top + H - 6, span = H - 12;
  if (on) {
    for (let s = 0; s < HALO_SLOTS; s++) {
      const p = hi[12 + 3 * s], g = hi[13 + 3 * s], f = hi[14 + 3 * s];
      if (p < -900 || g <= 0.002) continue;
      for (const [pp, wt] of [[p, 1 - f], [p + 12, f]]) {
        if (wt < 0.02 || pp < LO || pp >= HI) continue;
        const bh = Math.max(1, Math.round(span * g * wt));
        c.globalAlpha = 0.35 + 0.65 * g; c.fillStyle = col.ink;
        c.fillRect(Math.round(X(pp)), base - bh, bw, bh);
      }
    }
    c.globalAlpha = 1;
    // the root: a mark over its lowest sounding copy
    const r = notes[0];
    let rp = null;
    for (let s = 0; s < HALO_SLOTS; s++) { const p = hi[12 + 3 * s]; if (p > -900 && hi[13 + 3 * s] > 0.01 && Math.abs(((p - r) % 12 + 12) % 12) < 0.01 && (rp === null || p < rp)) rp = p; }
    if (rp !== null) { const xr = Math.round(X(rp)) + bw / 2; c.fillStyle = col.red; c.beginPath(); c.moveTo(xr - 3, top + 2); c.lineTo(xr + 3, top + 2); c.lineTo(xr, top + 6); c.fill(); }
  }
  c.restore();
  // the octave names under the frame's marks
  c.font = col.font; c.textBaseline = 'top'; c.fillStyle = col.dim;
  for (let o = 1; o <= 6; o++) { const st = LO + 12 * (o - 1); if (st > LO) c.fillText('C' + o, Math.round(X(st)) + 2, top + 2); }
}
// STRUM's screen: the chord's name, the progression's step, the direction and the scale on top; under them its strings,
// the bass at the bottom, each named by the note it plays and shaking as loud as it rings (a doubled course's second
// string beside it)
function drawStrum(c, col, w, h, now) {
  const sv = q => shownValue(q).v, hi = ui.halo, n = hi && hi[0] > 0.5 ? Math.round(hi[1]) : 0;
  const notes = Array.from({ length: n }, (_, k) => hi[2 + k]);
  const shape = n ? Math.round(hi[8]) : idx(sv(P.A), 12), pr = idx(sv(P.B), NPROG);
  let x = lcdTag(c, col, n ? haloChordName(notes, shape) : HALO_SHAPES[shape], 0, 0, 'inv') + 4;
  // (playing along with the chord track the quantiser follows: no progression of its own)
  const alongT = idx(sv(P.HMOD), 12) === 0 && trk().q !== 0 && qFollow() >= 0 && qFollow() !== ui.t;
  if (pr && !alongT) x += lcdTag(c, col, HALO_PROGS[pr] + (n ? (pr === PROG_WALK ? ' ' + (hi[10] > 0 ? '+' : '') + Math.round(hi[10]) : ' ' + (Math.round(hi[10]) + 1) + '/' + HALO_PROG_LEN[pr]) : ''), x, 0, 'box') + 4;
  x += lcdTag(c, col, STRUM_DIRS[idx(sv(P.E), 4)] + ' · ' + STRUM_MATS[idx(sv(P.SMAT), 6)], x, 0, 'box') + 4;
  const mode = idx(sv(P.HMOD), 12), tonic = ((Math.round((sv(P.TUNE) - 0.5) * 48) % 12) + 12) % 12;
  // (MODE KEY under a quantiser that follows another chord track: its chords are built in that track's)
  const fol = qFollow(), along = mode === 0 && trk().q !== 0 && fol >= 0 && fol !== ui.t;
  const keyed = mode === 0 && (state.scale || along) && trk().q !== 0;
  lcdTag(c, col, along ? 'CHORDS T' + (fol + 1) : keyed ? scaleName(state.scale, state.qroot || 0) : NOTE_NAMES[tonic] + ' ' + HALO_MODES[mode || 1], w, 0, along ? 'inv' : 'box', true);
  const top = 14, H = h - top;
  lcdFrame(c, col, 0, top, w, H, null, null);
  lcdClip(c, 0, top, w, H);
  // the strings: per string its pitch and level (and its double's)
  const str = Array.from({ length: 6 }, () => ({ p: null, g: 0, g2: 0 }));
  if (hi && hi[0] > 0.5)
    for (let s = 0; s < HALO_SLOTS; s++) {
      const p = hi[12 + 3 * s], g = hi[13 + 3 * s], j = Math.round(hi[14 + 3 * s]);
      if (p < -900 || j < 0 || j > 5) continue;
      const S = str[j];
      if (S.p === null || p < S.p - 6) { if (S.p !== null) S.g2 = Math.max(S.g2, S.g); S.p = p; S.g = Math.max(S.g, g); }
      else if (Math.abs(p - S.p) < 0.5) S.g = Math.max(S.g, g); else S.g2 = Math.max(S.g2, g);
    }
  const rows = 6, gap = (H - 6) / rows, x0 = 22, t = now / 1000;
  c.font = col.font; c.textBaseline = 'middle';
  for (let j = 0; j < rows; j++) {
    const y = Math.round(top + H - 4 - gap * (j + 0.5)) + 0.5, S = str[j];
    const a = S.p === null ? 0 : Math.min(gap * 0.42, Math.max(0, (Math.log10(S.g + 1e-6) + 3.2) / 3.2) * gap * 0.42);
    c.fillStyle = S.p === null ? col.lo : col.dim;
    c.fillText(S.p === null ? '·' : lcdText(noteName(Math.round(S.p))), 4, y);
    c.strokeStyle = a > 0.3 ? col.ink : col.lo; c.lineWidth = 1;
    c.beginPath();
    for (let xx = x0; xx <= w - 3; xx += 2) {
      const u = (xx - x0) / (w - 3 - x0), yy = y + a * Math.sin(Math.PI * u) * Math.sin(t * 38 + j * 1.7);
      if (xx === x0) c.moveTo(xx, yy); else c.lineTo(xx, yy);
    }
    c.stroke();
    if (S.g2 > 0.002) {
      const a2 = Math.min(gap * 0.3, Math.max(0, (Math.log10(S.g2 + 1e-6) + 3.2) / 3.2) * gap * 0.3);
      c.strokeStyle = col.dim; c.beginPath();
      for (let xx = x0; xx <= w - 3; xx += 2) { const u = (xx - x0) / (w - 3 - x0), yy = y - 3 + a2 * Math.sin(Math.PI * u) * Math.sin(t * 52 + j); if (xx === x0) c.moveTo(xx, yy); else c.lineTo(xx, yy); }
      c.stroke();
    }
  }
  c.restore();
}
// v81: HOWL's screen: its chord (root, chord, voicing), the six strings (their notes, shaking as loud as they ring, a
// glide drawn as the string sliding), and the loop: how hard the amp feeds back (HOWL, a meter), the bloom, the guitar
// turned away when the gate shuts
function drawHowl(c, col, w, h, now) {
  const sv = q => shownValue(q).v, hi = ui.halo, ok = hi && Math.round(hi[0]) === 4;
  const type = ok ? Math.round(hi[9]) : idx(sv(P.A), 15), voic = ok ? Math.round(hi[10]) : idx(sv(P.B), 8);
  const root = ok ? Math.round(hi[8]) : 40 + Math.round((sv(P.TUNE) - 0.5) * 48);
  let x = lcdTag(c, col, type === HOWL_POLY ? 'POLY' : noteOct(root) + ' ' + HOWL_TYPES[type], 0, 0, 'inv') + 4;
  if (type !== HOWL_POLY) x += lcdTag(c, col, HOWL_VOICS[voic] + (type === HOWL_KEY && state.scale ? ' · ' + scaleName(state.scale, state.qroot || 0) : ''), x, 0, 'box') + 4;
  const loop = ok ? hi[11] : 0, bloom = ok ? hi[31] : 0, gate = ok ? hi[32] : 0, fd = sv(P.D);
  lcdTag(c, col, (fd < 0.002 ? 'NO FEEDBACK' : 'FDBK ' + Math.round(fd * 100) + '%') + (ok && gate < 0.5 && loop > 0.001 ? ' · AWAY' : ''), w, 0, ok && loop > 0.25 && gate > 0.5 ? 'inv' : 'box', true);
  const top = 14, H = h - top, mw = 14;
  lcdFrame(c, col, 0, top, w - mw - 2, H, null, null);
  lcdClip(c, 0, top, w - mw - 2, H);
  const rows = 6, gap = (H - 6) / rows, x0 = 30, x1 = w - mw - 5, t = now / 1000;
  c.font = col.font; c.textBaseline = 'middle';
  // (lowest string at the bottom, as on a neck seen from the player)
  const strs = [];
  if (ok) for (let k = 0; k < 6; k++) { const p = hi[12 + 3 * k]; if (p > -900) strs.push({ p, g: hi[13 + 3 * k], gl: hi[14 + 3 * k], b: hi[34 + k] || 0 }); }
  const bowX = x0 + (0.3 + 0.55 * sv(SQ(32))) * (x1 - x0);    // (v83: the bow over the strings it drives, at POS)
  strs.sort((a, b) => a.p - b.p);
  for (let j = 0; j < rows; j++) {
    const y = Math.round(top + H - 4 - gap * (j + 0.5)) + 0.5, S = strs[j];
    const a = !S ? 0 : Math.min(gap * 0.44, Math.max(0, (Math.log10(S.g + 1e-6) + 3.4) / 3.4) * gap * 0.44);
    c.fillStyle = S ? col.dim : col.lo;
    c.fillText(S ? lcdText(noteOct(Math.round(S.p))) : '·', 4, y);
    c.strokeStyle = a > 0.3 ? col.ink : col.lo; c.lineWidth = 1;
    c.beginPath();
    // (a feedback's wave: slow and wide where the loop holds it, a glide bending the string)
    const bend = S && S.gl < 0.999 ? (1 - S.gl) * gap * 0.3 : 0;
    for (let xx = x0; xx <= x1; xx += 2) {
      const u = (xx - x0) / (x1 - x0), yy = y + a * Math.sin(Math.PI * u) * Math.sin(t * (26 + 4 * j) + j * 1.7) - bend * Math.sin(Math.PI * u);
      if (xx === x0) c.moveTo(xx, yy); else c.lineTo(xx, yy);
    }
    c.stroke();
    if (S && S.b > 0.02) { c.globalAlpha = Math.min(1, 0.25 + S.b); c.fillStyle = col.red; c.fillRect(Math.round(bowX) - 5, Math.round(y - gap * 0.3), 10, Math.max(2, Math.round(gap * 0.6))); c.globalAlpha = 1; }
  }
  c.restore();
  // the loop's meter: how much the strings hold (the howl), the bloom under it
  const mx = w - mw, mh = H - 2, lv = Math.max(0, Math.min(1, (Math.log10(loop + 1e-6) + 3) / 3));
  lcdFrame(c, col, mx, top, mw, H, null, null);
  c.fillStyle = lv > 0.82 ? col.red : col.ink;
  c.fillRect(mx + 3, top + 1 + mh * (1 - lv), mw - 6, mh * lv);
  c.fillStyle = col.dim; c.fillRect(mx + 3, top + H - 3, (mw - 6) * Math.max(0, Math.min(1, bloom)), 2);
}
// CHOIR's screen is its ribbon: two octaves round the track's pitch (NOTE 0 in the middle), its keys printed under it as
// on the ondes' (the scale's notes lit when the track follows it), the voice where it sings (red) and where it has been
// (a trail falling away), the finger where it touches (up: louder). On top: the note and its cents, the vowel, the
// singers, SPACE; SNAP / FREE on the right (a tap: the ribbon holds to the scale, gliding between its notes, or not)
const ribSnapOn = () => !ui.ribFree && quantOn() && trk().q !== 0;
function ribBase() { return 60 + (shownValue(P.TUNE).v - 0.5) * 48; }
// where the finger is (semitones from the track's pitch); on SNAP the engine holds the voice to the quantiser's notes
// (its scale, or the chord of the track it follows) and glides from one to the next in GLID
function ribSemis(fx) { return clamp(-12 + 24 * fx, -24, 24); }
const ribGate = () => ribSnapOn() ? 2 : 1;
function drawChoir(c, col, w, h, now) {
  const sv = q => shownValue(q).v, hi = ui.halo, ok = hi && Math.round(hi[0]) === 3, on = ok && hi[1] > 0.5;
  const base = ribBase();
  let x = 0;
  if (on) {
    const m = hi[2], r = Math.round(m), ct = Math.round((m - r) * 100);
    x = lcdTag(c, col, noteName(r) + (Math.floor(r / 12) - 1) + (ct ? (ct > 0 ? ' +' : ' ') + ct + 'C' : ''), 0, 0, 'inv') + 4;
  } else x = lcdTag(c, col, '—', 0, 0, 'inv') + 4;
  x += lcdTag(c, col, fVowel(sv(P.A)), x, 0, 'box') + 4;
  const part = idx(sv(P.CPRT), 6), sing = 1 + 3 * sv(P.E);
  x += lcdTag(c, col, (part ? CHOIR_PARTS[part] + ' ' : '') + '×' + sing.toFixed(1), x, 0, 'box') + 4;
  if (sv(P.CSPC) > 0.005) lcdTag(c, col, 'SPACE ' + Math.round(sv(P.CSPC) * 100) + '%' + (idx(sv(P.CSHM), 4) ? ' ' + CHOIR_SHIMS[idx(sv(P.CSHM), 4)] : ''), x, 0, 'txt');
  const snap = ribSnapOn();
  lcdTag(c, col, snap ? 'SNAP' : 'FREE', w, 0, snap ? 'inv' : 'box', true);
  const top = 14, H = h - top, X = st => 1 + (st + 12) / 24 * (w - 3);
  lcdFrame(c, col, 0, top, w, H, null, null);
  lcdClip(c, 0, top, w, H);
  // the keys: a tick a semitone (the black ones shorter), the scale's notes lit, the octaves' C named
  const mask = quantOn() && trk().q !== 0 ? qLive() : 0;
  c.font = col.font; c.textBaseline = 'bottom';
  for (let k = -12; k <= 12; k++) {
    const n = Math.round(base + k), pc = ((n % 12) + 12) % 12, black = [1, 3, 6, 8, 10].includes(pc);
    const xx = Math.round(X(k)) + 0.5, inS = mask ? (mask >> pc) & 1 : !black;
    c.fillStyle = inS ? col.dim : col.lo;
    c.fillRect(xx - 0.5, top + H - (black ? 6 : 10), 1, black ? 5 : 9);
    if (pc === 0) { c.fillStyle = col.dim; c.fillText('C' + (Math.floor(n / 12) - 1), xx + 2, top + H - 1); }
  }
  // the trail (the last 1.5 s, newest on top), the voice now, the finger
  const hist = ui.choirHist || (ui.choirHist = []), tNow = now / 1000;
  if (on) { hist.push([hi[2] - base, hi[4], tNow]); }
  while (hist.length && tNow - hist[0][2] > 1.5) hist.shift();
  c.lineWidth = 1;
  for (let i = 1; i < hist.length; i++) {
    const [p0, e0, t0] = hist[i - 1], [p1, e1, t1] = hist[i];
    if (t1 - t0 > 0.2) continue;
    const y0 = top + 2 + (tNow - t0) / 1.5 * (H - 16), y1 = top + 2 + (tNow - t1) / 1.5 * (H - 16);
    c.globalAlpha = Math.max(0.08, (1 - (tNow - t1) / 1.5)) * (0.3 + 0.7 * Math.min(1, e1 * 1.4));
    c.strokeStyle = col.ink; c.beginPath(); c.moveTo(X(p0), y0); c.lineTo(X(p1), y1); c.stroke();
  }
  c.globalAlpha = 1;
  if (on) {
    const xv = Math.round(X(hi[2] - base)), e = hi[4];
    c.fillStyle = col.red; c.fillRect(xv - 1, top + 1, 3, Math.max(3, Math.round((H - 14) * Math.min(1, e))));
  }
  if (ui.rib) {
    const xr = Math.round(X(ui.rib.semis)) + 0.5, yr = top + 2 + (1 - ui.rib.expr) * (H - 16);
    c.strokeStyle = col.ink; c.lineWidth = 1; c.beginPath(); c.moveTo(xr, top + 1); c.lineTo(xr, top + H - 11); c.stroke();
    c.fillStyle = col.ink; c.beginPath(); c.arc(xr, yr, 3.5, 0, Math.PI * 2); c.fill();
  }
  c.restore();
}
// the ribbon's touch: on CHOIR's screen (ovw), a finger at a time
function ribAt(e) {
  const r = ovw.getBoundingClientRect();
  const fx = clamp((e.clientX - r.left) / Math.max(1, r.width), 0, 1), fy = clamp(1 - (e.clientY - r.top - 16) / Math.max(1, r.height - 28), 0, 1);
  return { semis: ribSemis(fx), expr: fy };
}
function ribLetGo() {
  if (!ui.rib) return;
  send(['rib', ui.rib.t, ui.rib.semis, 0, ui.rib.expr]);
  ui.rib = null;
}
ovw.addEventListener('pointerdown', e => {
  if (!choirShown()) return;
  e.preventDefault();
  const r = ovw.getBoundingClientRect();
  // SNAP / FREE: the tag on the right of the top line
  if (e.clientY - r.top < 14 && e.clientX > r.right - 44) { ui.ribFree = !ui.ribFree; return; }
  backend.ensureAudio();
  try { ovw.setPointerCapture(e.pointerId); } catch (err) {}
  if (ui.rib) ribLetGo();
  const a = ribAt(e);
  ui.rib = { id: e.pointerId, t: ui.t, semis: a.semis, expr: a.expr };
  send(['rib', ui.t, a.semis, ribGate(), a.expr]);
});
ovw.addEventListener('pointermove', e => {
  if (!ui.rib || ui.rib.id !== e.pointerId) return;
  const a = ribAt(e);
  if (Math.abs(a.semis - ui.rib.semis) < 1e-4 && Math.abs(a.expr - ui.rib.expr) < 0.004) return;
  ui.rib.semis = a.semis; ui.rib.expr = a.expr;
  send(['rib', ui.rib.t, a.semis, ribGate(), a.expr]);
});
const ribUp = e => { if (ui.rib && ui.rib.id === e.pointerId) ribLetGo(); };
ovw.addEventListener('pointerup', ribUp); ovw.addEventListener('pointercancel', ribUp);
// v84: MANTRA's screen is its eight sensors, in their pairs (12 34 | 56 78): each pad filled as far as its voice is
// open (its group's HOLD drawn across it, the floor it never falls under), the note it sounds, lit while it is touched
// (a finger, a trig), the finger where it presses (up: harder); under each pair where its modulation comes from (its
// line as long as it moves it); on top what a trig touches (TCH), 2 LOOP or 1 LOOP, the LFO's squares, the delay's
// feedback (SING past the middle); on the right the output and the delay
const mtHold = (x, fast) => { const u = clamp(fast ? (x - 0.45) / 0.55 : (x - 0.08) / 0.8, 0, 1); return u * u * (3 - 2 * u); };
function mtPads(w, h) {
  const x0 = 1, x1 = w - 18, y0 = 17, y1 = h - 10, g1 = 3, g2 = 7, g3 = 13, pw = (x1 - x0 - 4 * g1 - 2 * g2 - g3) / 8, out = [];
  let x = x0;
  for (let k = 0; k < 8; k++) { out.push({ x, y: y0, w: pw, h: y1 - y0 }); x += pw + (k === 3 ? g3 : k & 1 ? g2 : g1); }
  return out;
}
function drawMantra(c, col, w, h, now) {
  const sv = q => shownValue(q).v, hi = ui.halo, ok = !!hi && Math.round(hi[0]) === 5 && hi[1] > 0.5;
  const tch = idx(sv(P.A), 15), oneLoop = idx(sv(SQ(20)), 2) === 1, total = idx(sv(SQ(27)), 2) === 1;
  let x = lcdTag(c, col, 'TCH ' + MT_TCHS[tch], 0, 0, 'inv') + 4;
  x += lcdTag(c, col, oneLoop ? '1 LOOP' : '2 LOOP', x, 0, 'box') + 4;
  // the LFO: A and B, lit while high
  const lw = lcdTag(c, col, idx(sv(SQ(30)), 2) ? 'LFO AND' : 'LFO OR', x, 0, 'box');
  for (let j = 0; j < 2; j++) {
    const on = ok && hi[27 + j] > 0.5, xx = Math.round(x + lw + 3 + 8 * j);
    if (on) { c.fillStyle = col.red; c.fillRect(xx, 3, 5, 5); } else { c.strokeStyle = col.dim; c.lineWidth = 1; c.strokeRect(xx + 0.5, 3.5, 4, 4); }
  }
  const dfb = sv(P.D), sing = dfb > 0.5;
  lcdTag(c, col, 'DFB ' + Math.round(dfb * 100) + '%' + (sing ? ' SING' : ''), w - 16, 0, sing && ok && hi[29] > 0.01 ? 'inv' : 'box', true);
  const pads = mtPads(w, h), mask = MT_MASKS[tch];
  const tr = Math.pow(2, (sv(P.TUNE) - 0.5) * 4), TQ = [0, 1, 2, 3, 7, 8, 9, 10];
  const fast = [16, 19, 23, 26].map(q => idx(sv(SQ(q)), 2) === 1);
  const fingers = ui.mtPress && ui.mtT === ui.t ? [...ui.mtPress.values()] : [];
  c.font = col.font; c.textBaseline = 'top'; c.textAlign = 'center';
  for (let k = 0; k < 8; k++) {
    const P0 = pads[k], X = Math.round(P0.x), Y = Math.round(P0.y), W = Math.round(P0.w), H = Math.round(P0.h), pr = k >> 1;
    const hd = mtHold(sv(k < 4 ? P.B : P.C), fast[pr]);
    const g = ok ? clamp(hi[2 + k], 0, 1) : hd, touched = ok && hi[18 + k] > 0;
    const hz = ok && hi[10 + k] > 0 ? hi[10 + k] : mtTuneHz(k, sv(SQ(TQ[k]))) * Math.pow(2, (sv(SQ(k < 4 ? 6 : 13)) - 0.85) * 4) * tr;
    const body = H - 11, fh = Math.round(body * g);
    // its voice, as far as it is open; the floor its HOLD keeps
    if (fh > 0) { c.fillStyle = col.ink; c.globalAlpha = 0.35 + 0.65 * g; c.fillRect(X + 2, Y + H - 2 - fh, W - 4, fh); c.globalAlpha = 1; }
    if (hd > 0.005) { c.fillStyle = col.red; c.fillRect(X + 2, Math.round(Y + H - 2 - body * hd), W - 4, 1); }
    // its frame: lit while touched; the sensors a trig touches marked under it
    lcdFrame(c, col, X, Y, W, H, null, null, touched ? col.ink : null);
    if (touched) { c.strokeStyle = col.ink; c.lineWidth = 1; c.strokeRect(X + 1.5, Y + 1.5, W - 3, H - 3); }
    if ((mask >> k) & 1) { c.fillStyle = col.dim; c.fillRect(X + Math.round(W / 2) - 2, Y + H + 1, 4, 2); }
    c.fillStyle = col.ink; c.fillText(lcdText(noteOct(Math.round(69 + 12 * Math.log2(Math.max(hz, 1) / 440)))), X + W / 2, Y + 2);
    // the fingers on it, where they press
    for (const f of fingers) if (f.k === k) {
      const yy = Y + (1 - (f.p - 0.12) / 0.88) * (H - 1), xx = X + W / 2;
      c.fillStyle = col.bg; c.beginPath(); c.arc(xx, yy, 5, 0, Math.PI * 2); c.fill();
      c.fillStyle = col.red; c.beginPath(); c.arc(xx, yy, 3.5, 0, Math.PI * 2); c.fill();
    }
  }
  // each pair's modulation: its source, a line as long as it moves the pair
  c.textBaseline = 'bottom';
  const partner = [oneLoop ? '78' : '34', '12', oneLoop ? '34' : '78', '56'];
  for (let pr = 0; pr < 4; pr++) {
    const a = pads[2 * pr], b = pads[2 * pr + 1], x0 = a.x, x1 = b.x + b.w, src = idx(sv(SQ([15, 18, 22, 25][pr])), 3);
    const txt = src === 0 ? '' : src === 1 ? 'FM ' + partner[pr] : total ? 'OUT' : 'LFO';
    c.fillStyle = src ? col.ink : col.dim; c.fillText(lcdText(txt), (x0 + x1) / 2, h);
    const amt = ok ? Math.min(1, hi[31 + pr] / 0.6) : 0;
    if (src && amt > 0.01) { c.fillStyle = col.red; c.fillRect(Math.round(x0), h - 10, Math.max(1, Math.round((x1 - x0) * amt)), 1); }
  }
  c.textAlign = 'left';
  // the output and the delay
  const mx = w - 14, top = 14, H = h - top, lv = v => Math.max(0, Math.min(1, (Math.log10(v + 1e-6) + 3) / 3));
  lcdFrame(c, col, mx, top, 14, H, null, null);
  const o = ok ? lv(hi[30]) : 0, d = ok ? lv(hi[29]) : 0, mh = H - 2;
  c.fillStyle = o > 0.9 ? col.red : col.ink; c.fillRect(mx + 2, top + 1 + mh * (1 - o), 5, mh * o);
  c.fillStyle = col.dim; c.fillRect(mx + 8, top + 1 + mh * (1 - d), 4, mh * d);
}
// its pads: a finger each (several at once), its pressure where it is on the pad (up: harder), sliding from pad to pad;
// a pad lifted when no finger is on it
function mtPadAt(e) {
  const r = ovw.getBoundingClientRect(), x = e.clientX - r.left, y = e.clientY - r.top, pads = mtPads(r.width, r.height);
  for (let k = 0; k < 8; k++) {
    const p = pads[k];
    if (x >= p.x - 2 && x <= p.x + p.w + 2 && y >= p.y - 6 && y <= p.y + p.h + 8) return { k, p: clamp(0.12 + 0.88 * (1 - (y - p.y) / p.h), 0.12, 1) };
  }
  return null;
}
function mtSync() {
  const want = new Array(8).fill(0), sent = ui.mtSent || (ui.mtSent = new Array(8).fill(0));
  if (ui.mtPress) for (const f of ui.mtPress.values()) if (f.k >= 0) want[f.k] = Math.max(want[f.k], f.p);
  for (let k = 0; k < 8; k++)
    if (Math.abs(want[k] - sent[k]) > 0.004 || (want[k] > 0) !== (sent[k] > 0)) { sent[k] = want[k]; send(['pad', ui.mtT, k, want[k]]); }
}
function mtLetGo() {
  if (!ui.mtPress) return;
  ui.mtPress.clear(); mtSync(); ui.mtPress = null;
}
ovw.addEventListener('pointerdown', e => {
  if (!mantraShown()) return;
  e.preventDefault();
  const a = mtPadAt(e);
  if (!a) return;
  backend.ensureAudio();
  try { ovw.setPointerCapture(e.pointerId); } catch (err) {}
  if (ui.mtPress && ui.mtT !== ui.t) mtLetGo();
  if (!ui.mtPress) { ui.mtPress = new Map(); ui.mtT = ui.t; ui.mtSent = new Array(8).fill(0); }
  ui.mtPress.set(e.pointerId, a);
  mtSync();
});
ovw.addEventListener('pointermove', e => {
  if (!ui.mtPress || !ui.mtPress.has(e.pointerId)) return;
  ui.mtPress.set(e.pointerId, mtPadAt(e) || { k: -1, p: 0 });
  mtSync();
});
const mtUp = e => { if (ui.mtPress && ui.mtPress.delete(e.pointerId)) mtSync(); };
ovw.addEventListener('pointerup', mtUp); ovw.addEventListener('pointercancel', mtUp);
// BREAK's screen: the slot, its tempo, SYNC / VARI on top, what the last trig did on the right; under them the loop's
// waveform with a column per 16th (what it starts with: a kick a tall mark, a snare a mid one, the rest a short one,
// as hard as it is), the bars marked, the slice that plays lit, the playhead in red (dotted while SWAP plays another
// slot at the same place)
function drawBreak(c, col, w, h) {
  const sv = q => shownValue(q).v, bi = ui.brk;
  const knob = idx(sv(P.A), BRK_SLOTS), slot = brkResolve(knob);
  const top = 14, H = h - top;
  if (slot < 0) {
    lcdTag(c, col, 'SLOT ' + (knob + 1), 0, 0, 'inv');
    lcdFrame(c, col, 0, top, w, H, [], null);
    c.font = col.font; c.fillStyle = col.dim; c.textAlign = 'center'; c.textBaseline = 'middle';
    c.fillText(lcdText('No loop · tap LOAD: up to 10 (WAV, AIFF, MP3, M4A)'), w / 2, top + H / 2);
    c.textAlign = 'left';
    return;
  }
  const mine = bi && Math.round(bi[1]) === slot;
  const L = mine ? Math.round(bi[2]) : 16 * brkBars(slot), on = mine && bi[0] > 0.5;
  const vari = idx(sv(P.B), 2) === 1;
  let x = lcdTag(c, col, 'SLOT ' + (slot + 1) + (brkName(slot) ? ' ' + brkName(slot).slice(0, 18) : ''), 0, 0, 'inv') + 4;
  const bpm = ui.brks ? ui.brks.info[slot * 4 + 1] : 0;
  if (bpm) x += lcdTag(c, col, (Math.round(bpm * 10) / 10) + ' BPM', x, 0, 'box') + 4;
  if (vari) lcdTag(c, col, 'VARI', x, 0, 'box');
  const fl = on ? Math.round(bi[6]) : 0, pj = on ? Math.round(bi[7]) : 0, play = on ? Math.round(bi[12]) : -1;
  const what = [fl & 32 ? 'SWAP ' + (play + 1) : '', fl & 1 ? 'RAND' : '', fl & 2 ? 'REV' : '', fl & 4 ? 'STUT' : '', fl & 8 ? (pj > 0 ? '+' : '') + pj : '', fl & 16 ? 'SKIP' : ''].filter(Boolean).join(' ');
  if (what) lcdTag(c, col, what, w, 0, 'inv', true, col.red);
  else lcdTag(c, col, on ? 'SL ' + (Math.round(bi[4]) + 1) + '/' + L : L / 16 + (L > 16 ? ' BARS' : ' BAR'), w, 0, 'box', true);
  const bars = Math.max(1, L / 16);
  lcdFrame(c, col, 0, top, w, H, Array.from({ length: bars - 1 }, (_, i) => (i + 1) / bars), null);
  lcdClip(c, 0, top, w, H);
  const cw = (w - 3) / L, X = i => 1 + i * cw, mid0 = top + H / 2;
  // the slice that plays (its LEN)
  if (on && play === slot) {
    const s0 = Math.round(bi[4]), n = Math.max(1, bi[5] > 0 ? bi[5] : 1);
    c.fillStyle = col.lo; c.fillRect(Math.round(X(s0)), top + 1, Math.max(1, Math.round(cw * n)), H - 2);
  }
  // the waveform
  const pk = mine ? bi.slice(BRK_HEAD + BRK_MAXLEN, BRK_HEAD + BRK_MAXLEN + 128) : ui.brks && ui.brks.peaks ? ui.brks.peaks[slot] : null;
  if (pk) {
    const n = Math.max(2, Math.round(w - 2)), half = (H - 10) / 2;
    c.fillStyle = col.dim;
    for (let i = 0; i < n; i++) {
      const a = Math.min(1, (pk[Math.min(127, Math.floor(i / n * 128))] || 0) * 1.1);
      if (a < 0.004) continue;
      const yh = Math.max(0.5, a * half);
      c.fillRect(Math.round(1 + i / n * (w - 3)), Math.round(mid0 - yh), 1, Math.max(1, Math.round(2 * yh)));
    }
  }
  // what each 16th starts with, along the bottom
  if (mine) {
    const bw = Math.max(1, Math.round(cw) - 1);
    for (let i = 0; i < L; i++) {
      const m = Math.round(bi[BRK_HEAD + i] || 0), cls = m & 3, st = (m >> 2) / 15;
      const hgt = Math.max(1, Math.round((cls === 1 ? 7 : cls === 2 ? 5 : 2) * (0.45 + 0.55 * st)));
      c.fillStyle = col.ink; c.fillRect(Math.round(X(i)), top + H - 2 - hgt, bw, hgt);
    }
  }
  // the playhead (on the place SWAP plays in another slot: dotted)
  if (on && bi[3] >= 0) {
    const xp = Math.round(X(play === slot ? bi[3] : bi[3] % L));
    c.fillStyle = col.red;
    if (play === slot) c.fillRect(xp, top + 1, 1, H - 2);
    else for (let y = top + 1; y < top + H - 1; y += 3) c.fillRect(xp, y, 1, 2);
  }
  c.restore();
}
// FX's screen: the machine and what it takes, its main readout, what a trig does (lit while it acts) on top; under them,
// on the left, the last 5 s: what comes in (dim, around the middle), what goes out (the line), the gesture (shaded), the
// gate (red along the bottom) and the machine's own trace (dashed: TAPE its pitch, CODEC its bandwidth, BEND its events);
// on the right a picture of the machine now: VERB its decay, ECHO its repeats, BEND its last divisions, TAPE its reels
// (turning at the tape's speed), CODEC its bands (kept, lost)
const BEND_EV = ['LIVE', 'RPT', 'BACK', 'REV', 'STOP', 'DROP', '2×', 'BENT'];
function drawFxm(c, col, w, h, now) {
  const m = curMachine(), k = fxI(m), sv = q => shownValue(q).v;
  const fi = ui.fxm && Math.round(ui.fxm[0]) === m ? ui.fxm : null, hist = fi ? ui.fxmHist || [] : [];
  const act = idx(sv(P.XE), FX_ACTS[k].length), hold = sv(P.DEC) >= 0.99;
  const mask = fi ? Math.round(fi[1]) : fxMasks()[ui.t];
  const v6 = fi ? fi[6] : 0, v7 = fi ? fi[7] : 0, v8 = fi ? fi[8] : 0, v9 = fi ? fi[9] : 0;
  // the tags
  let x = lcdTag(c, col, FX_NAMES[k] + ' ← ' + fxSrcName(mask), 0, 0, 'inv') + 4;
  const zone = idx(sv(P.B), 4), rate = v9;
  const main = !fi ? '' : k === 0 ? 'RT ' + (v6 < 0 ? '∞' : fSec(v6)) + (v9 > 1 ? ' · PRE ' + Math.round(v9) + 'MS' : '')
    : k === 1 ? (zone === 0 ? fHzLow(1000 / Math.max(v6, 0.01)) : fTime(v6 / 1000)) + ' · ' + Math.round(v7 * 100) + '%'
    : k === 2 ? BEND_EV[Math.round(v6)] + (Math.round(v6) === 1 ? ' ' + Math.round(v7) : '') + (rate !== 1 && Math.round(v6) !== 0 ? ' ' + fSt(24)(0.5 + Math.log2(Math.max(rate, 1e-3)) / 4) : '')
    : k === 3 ? (v8 < 0.999 ? 'SPEED ' + Math.round(v8 * 100) + '%' : (v6 >= 0 ? '+' : '') + v6.toFixed(1) + '¢')
    : k === 5 ? (() => { const ci = idx(sv(P.S5), 8), ch = ci === 0 || ci === 5;      // (v73: the circuit; FUZZ's C1, XSTR's coupling)
      return FZ_CIRCS[ci] + ' · ' + fBias(fi[11]) + (ch ? ' · ' + (ci ? 'C ' : 'C1 ') + (fi[10] < 0.01 ? '0' : fi[10].toFixed(2)) + (ci ? '' : 'V') : '') + (v9 < 0.5 ? ' · GATED' : ''); })()
    : k === 7 ? (() => {                       // (v83, STENCIL: the two lines' times, KARP and COMB as pitches; REPT)
      const z = idx(sv(P.B), 8), ft = ms => z <= 1 ? fHzLow(1000 / Math.max(ms, 0.01)) : fTime(ms / 1000);
      return (Math.abs(v7 - v6) < 0.01 * v6 ? ft(v6) : ft(v6) + ' | ' + ft(v7)) + ' · ' + Math.round(v8 * 100) + '%' + (v8 > 1.001 && fi[10] < 0.5 ? ' SELF' : ''); })()
    : k === 6 ? (idx(sv(P.XC), 2) ? 'SHFT ' + fWeftShift(sv(P.B)) : fWeftAlgo(sv(P.B))) + ' · ' + (idx(sv(P.D), 11) < 3 ? ['SIN', 'TRI', 'SAW'][idx(sv(P.D), 11)] + ' ' + fWeftNote(sv(P.TUNE)) : 'CARR T' + (idx(sv(P.D), 11) - 2))
    : (v6 >= 23999 ? 'FULL' : fKHz(v6)) + ' · ' + Math.round(v7 * 100) + '% KEPT';
  if (main) x += lcdTag(c, col, main, x, 0, 'box') + 4;
  const gesture = fi && fi[3] > 0.5, open = fi && fi[2] > 0.05;
  const actL = act === 0 ? (hold ? 'IN · OPEN' : 'IN') : FX_ACTS[k][act] + (hold ? ' · HOLD' : '');
  if (act > 0 && gesture) lcdTag(c, col, actL, w, 0, 'inv', true, col.red);
  else if (act === 0 && !hold && open) lcdTag(c, col, actL, w, 0, 'inv', true);
  else lcdTag(c, col, actL, w, 0, 'box', true);
  const top = 14, H = h - top, lw = Math.round(w * 0.62), rx = lw + 4, rw = w - rx;
  // the last 5 s
  lcdFrame(c, col, 0, top, lw, H, [0.2, 0.4, 0.6, 0.8], [0.5]);
  lcdClip(c, 0, top, lw, H);
  const N = 240, cw = (lw - 2) / N, X = i => 1 + (N - hist.length + i) * cw, mid0 = top + H / 2, half = H / 2 - 2;
  const lv = a => clamp((20 * Math.log10(Math.max(a, 1e-6)) + 48) / 48, 0, 1);
  hist.forEach((e, i) => {
    if (e[2] > 0.5) { c.fillStyle = col.lo; c.fillRect(Math.round(X(i)), top + 1, Math.ceil(cw), H - 2); }
    const a = lv(e[3]) * half;
    if (a > 0.5) { c.fillStyle = col.dim; c.fillRect(Math.round(X(i)), Math.round(mid0 - a), Math.max(1, Math.ceil(cw) - (cw > 2 ? 1 : 0)), Math.max(1, Math.round(2 * a))); }
  });
  // what goes out: its envelope, above and below
  c.strokeStyle = col.ink; c.lineWidth = 1;
  for (const sg of [-1, 1]) {
    c.beginPath();
    hist.forEach((e, i) => { const y = Math.round(mid0 + sg * lv(e[4]) * half) + 0.5; i ? c.lineTo(X(i) + cw / 2, y) : c.moveTo(X(i) + cw / 2, y); });
    c.stroke();
  }
  // the gate (IN): along the bottom
  if (act === 0 && !hold) {
    c.fillStyle = col.red;
    hist.forEach((e, i) => { if (e[1] > 0.02) { const gh = Math.max(1, Math.round(e[1] * 3)); c.fillRect(Math.round(X(i)), top + H - 1 - gh, Math.ceil(cw), gh); } });
  }
  // the machine's trace
  const dash = (f, lo, hi) => {
    c.save(); c.setLineDash([2, 2]); c.strokeStyle = col.ink; c.beginPath();
    let pen = false;
    hist.forEach((e, i) => { const v = f(e); if (v == null) { pen = false; return; } const y = Math.round(top + H - 2 - clamp((v - lo) / (hi - lo), 0, 1) * (H - 4)) + 0.5; pen ? c.lineTo(X(i) + cw / 2, y) : c.moveTo(X(i) + cw / 2, y); pen = true; });
    c.stroke(); c.restore();
  };
  if (k === 3) dash(e => e[7] < 0.999 ? -50 + 50 * e[7] : e[5], -50, 50);                       // TAPE: its pitch (cents; a stop: its speed)
  if (k === 4) dash(e => Math.log2(Math.max(1000, e[5]) / 1000) / Math.log2(24), 0, 1);         // CODEC: its bandwidth (1 .. 24 kHz)
  if (k === 2) hist.forEach((e, i) => { if (Math.round(e[5])) { c.fillStyle = col.ink; c.fillRect(Math.round(X(i)), top + 1, Math.ceil(cw), 2); } });   // BEND: an event
  if (k === 5) dash(e => e[9], 0, 1);                                                             // FUZZ: the charge C1 holds (0 .. 1 V)
  if (k === 6) dash(e => e[6], 0, 1);                                                             // WEFT: ALGO (where it morphs)
  if (k === 7) dash(e => lv(e[8]), 0, 1);                                                         // STENCIL: the loop's level
  c.restore();
  // the machine now
  if (rw < 24) return;
  lcdFrame(c, col, rx, top, rw, H, null, null);
  lcdClip(c, rx, top, rw, H);
  const R0 = rx + 3, R1 = rx + rw - 3, T0 = top + 3, T1 = top + H - 3;
  c.strokeStyle = col.ink; c.fillStyle = col.ink; c.lineWidth = 1;
  if (k === 0) {
    // VERB: the tail falling 60 dB over RT (frozen: flat, in red), after PRE; SIZE as its first reflections
    const rt = v6, frz = v8 > 0.5 || rt < 0, span = rt > 0 ? Math.max(0.4, rt * 1.25) : 6, pre = Math.max(0, v9) / 1000;
    const TX = t => R0 + clamp(t / span, 0, 1) * (R1 - R0), DY = db => T0 + clamp(-db / 66, 0, 1) * (T1 - T0);
    c.fillStyle = col.lo; for (let xx = R0; xx < R1; xx += 3) c.fillRect(Math.round(xx), Math.round(DY(-60)), 1, 1);
    c.strokeStyle = frz ? col.red : col.ink; c.beginPath();
    for (let i = 0; i <= 40; i++) { const t = pre + (span - pre) * i / 40, db = frz ? -6 : -6 - 60 * (t - pre) / Math.max(rt, 1e-3); const y = Math.round(DY(db)) + 0.5; i ? c.lineTo(TX(t), y) : c.moveTo(TX(pre), y); }
    c.stroke();
    const sz = Math.max(v7, 5) / 1000; c.fillStyle = col.dim;
    for (let t = pre + sz * 0.5, n = 0; t < span && n < 14; t += sz, n++) { const y = DY(frz ? -6 : -6 - 60 * (t - pre) / Math.max(rt, 1e-3)); c.fillRect(Math.round(TX(t)), Math.round(y), 1, Math.max(1, Math.round(T1 - y))); }
  } else if (k === 1) {
    // ECHO: the direct sound and its repeats (HOLD: the loop as it is; FLIP: the repeats played backwards, hollow)
    const fb = act === 1 && gesture ? 1 : Math.min(1.1, v7), n = 7, gap = (R1 - R0) / (n + 0.5), bw = Math.max(2, Math.round(gap * 0.35));
    for (let i = 0; i <= n; i++) {
      const a = i === 0 ? 1 : Math.min(1, Math.pow(Math.max(fb, 0), i)), bh = Math.max(1, Math.round(a * (T1 - T0)));
      const xx = Math.round(R0 + i * gap);
      if (i === 0) { c.fillStyle = col.dim; c.fillRect(xx, T1 - bh, bw, bh); continue; }
      if (act === 2 && gesture) { c.strokeStyle = col.red; c.strokeRect(xx + 0.5, T1 - bh + 0.5, bw - 1, bh - 1); }
      else { c.fillStyle = act === 1 && gesture ? col.red : col.ink; c.fillRect(xx, T1 - bh, bw, bh); }
    }
  } else if (k === 2) {
    // BEND: its last divisions, the newest on the right, each drawn as what it did
    const evs = ui.fxmEv || [], cells = 6, cwid = (R1 - R0) / cells;
    for (let j = 0; j < cells; j++) {
      const e = evs[evs.length - cells + j]; if (!e) continue;
      const x0 = Math.round(R0 + j * cwid) + 1, x1 = Math.round(R0 + (j + 1) * cwid) - 1, ev = e[0];
      c.strokeStyle = j === cells - 1 ? col.ink : col.dim; c.fillStyle = c.strokeStyle; c.beginPath();
      if (ev === 0) { c.moveTo(x0, T1); c.lineTo(x1, T0); }                                                    // straight on
      else if (ev === 1) { const r = Math.max(2, Math.min(8, e[1])), q = (x1 - x0) / r; for (let i = 0; i < r; i++) { c.moveTo(x0 + i * q, T1); c.lineTo(x0 + (i + 1) * q - 0.5, T0 + (T1 - T0) * 0.6); } }   // repeats
      else if (ev === 2) { c.moveTo(x0, T0 + (T1 - T0) * 0.5); c.lineTo(x1, T0); }                             // a jump back
      else if (ev === 3) { c.moveTo(x0, T0); c.lineTo(x1, T1); }                                               // backwards
      else if (ev === 4) { c.moveTo(x0, T1); c.quadraticCurveTo(x0 + (x1 - x0) * 0.4, T0, x1, T0 + (T1 - T0) * 0.6); } // a tape stop
      else if (ev === 5) { c.setLineDash([1, 2]); c.moveTo(x0, T1); c.lineTo(x1, T1); }                       // a dropout
      else if (ev === 6) { const q = (x1 - x0) / 2; for (let i = 0; i < 2; i++) { c.moveTo(x0 + i * q, T1); c.lineTo(x0 + (i + 1) * q - 0.5, T0); } }   // double speed
      else { c.moveTo(x0, T1); c.lineTo(x1, T0 + (T1 - T0) * (e[2] > 1 ? -0.2 : 0.45)); }                     // bent
      c.stroke(); c.setLineDash([]);
    }
  } else if (k === 3) {
    // TAPE: the reels, turning at the tape's speed; the tape between them, broken up by a dropout, wavy by a crinkle
    ui.fxmReel = (ui.fxmReel || 0) + (fi ? v8 : 1) * Math.min(0.1, (now - (ui.fxmReelT || now)) / 1000) * 2.2;
    ui.fxmReelT = now;
    const rr = Math.min((T1 - T0) / 2 - 1, (R1 - R0) / 5), cy = T0 + rr + 1, cx0 = R0 + rr + 2, cx1 = R1 - rr - 2;
    for (const cx of [cx0, cx1]) {
      c.beginPath(); c.arc(cx, cy, rr, 0, 2 * Math.PI); c.stroke();
      for (let s = 0; s < 3; s++) { const a = ui.fxmReel * 2 * Math.PI + s * 2 * Math.PI / 3; c.beginPath(); c.moveTo(cx + Math.cos(a) * rr * 0.25, cy + Math.sin(a) * rr * 0.25); c.lineTo(cx + Math.cos(a) * rr * 0.8, cy + Math.sin(a) * rr * 0.8); c.stroke(); }
    }
    const drop = fi && v7 < 0.9, crk = fi && v9 < 0.9, ty = Math.round(T1) + 0.5;
    c.strokeStyle = drop ? col.red : col.ink; if (drop) c.setLineDash([2, 2]);
    c.beginPath(); c.moveTo(cx0, ty);
    for (let xx = cx0; xx <= cx1; xx += 2) c.lineTo(xx, ty - (crk ? Math.sin(xx * 1.7 + now / 40) * 1.5 : 0));
    c.stroke(); c.setLineDash([]);
  } else if (k === 5) {
    // FUZZ: its octaver (v71: SUB, its level; off: hollow) and its three bands (each one's level out of its circuit; a
    // CLEAN one hollow), the gate (red along the bottom when it has closed), the battery (how much is left: BIAS, a
    // drained one in red)
    const lab = ['SUB', 'LOW', 'MID', 'HIGH'], subOn = sv(P.S1) > 0.0005 || sv(P.S2) > 0.0005;
    const drv = [subOn ? 1 : 0, sv(P.XA), sv(P.XB), sv(P.XC)], bw = (R1 - R0 - 14) / 4, pk = [12, 6, 7, 8];
    for (let b = 0; b < 4; b++) {
      const lvv = fi ? lv(fi[pk[b]]) : 0, bh = Math.max(1, Math.round(lvv * (T1 - T0 - 9))), xx = Math.round(R0 + b * bw) + 1, ww = Math.max(2, Math.round(bw) - 3);
      if (drv[b] < 0.02) { c.strokeStyle = col.dim; c.strokeRect(xx + 0.5, T1 - bh + 0.5, ww - 1, Math.max(1, bh - 1)); }
      else { c.fillStyle = col.ink; c.fillRect(xx, T1 - bh, ww, bh); }
      c.fillStyle = col.dim; c.font = col.font; c.textBaseline = 'top'; c.fillText(lab[b], xx, T0);
    }
    if (fi && fi[9] < 0.5) { c.fillStyle = col.red; c.fillRect(R0, T1 - 1, Math.round(4 * bw) - 2, 2); }
    const bx = R1 - 9, bh2 = T1 - T0 - 4, bias = fi ? fi[11] : sv(P.C), charge = bias >= 0.5 ? 1 : 0.15 + 0.85 * bias / 0.5;
    c.strokeStyle = col.ink; c.strokeRect(bx + 0.5, T0 + 3.5, 7, bh2 - 1); c.fillRect(bx + 2, T0 + 1, 4, 2);
    c.fillStyle = bias < 0.2 ? col.red : col.ink; const fh = Math.round((bh2 - 3) * charge); c.fillRect(bx + 2, T0 + 2 + bh2 - fh, 4, fh - 1);
  } else if (k === 6) {
    // WEFT: Warps' six algorithms and its vocoder in a row, ALGO between them (the two it blends lit); SHFT: the two
    // sidebands, left down and right up, apart by the shift; its level along the bottom
    const shft = idx(sv(P.XC), 2) === 1, a = fi ? v7 : sv(P.B), lvv = fi ? lv(v6) : 0;
    c.font = col.font; c.textBaseline = 'top';
    if (shft) {
      const f = weftShift(a), sp = clamp(Math.log2(1 + Math.abs(f)) / 11, 0, 1), cx = (R0 + R1) / 2, dx = 4 + sp * ((R1 - R0) / 2 - 8);
      const tb = sv(P.C), hU = Math.round((T1 - T0 - 10) * tb), hD = Math.round((T1 - T0 - 10) * (1 - tb));
      c.fillStyle = col.dim; c.fillRect(Math.round(cx) - 1, T0 + 8, 2, T1 - T0 - 8);
      c.fillStyle = col.ink; c.fillRect(Math.round(cx - dx) - 2, T1 - hD, 4, Math.max(1, hD)); c.fillRect(Math.round(cx + dx) - 2, T1 - hU, 4, Math.max(1, hU));
      c.fillStyle = col.dim; c.fillText(fWeftShift(a), R0, T0);
    } else {
      const lab = ['XF', 'FD', 'AR', 'DR', 'XR', 'CP', 'VC'], n = 7, gw = (R1 - R0) / n;
      const voc = a >= 0.7, pos = voc ? 6 : Math.min(a * 8, 5.999), i0 = Math.floor(pos), f0 = pos - i0;
      for (let i = 0; i < n; i++) {
        const xx = Math.round(R0 + i * gw), ww = Math.max(2, Math.round(gw) - 2);
        const w8 = voc ? (i === 6 ? 1 : 0) : i === i0 ? 1 - f0 : i === i0 + 1 ? f0 : 0;
        const bh = Math.max(1, Math.round((T1 - T0 - 12) * (0.15 + 0.85 * w8)));
        if (w8 > 0.02) { c.fillStyle = i === 6 && a >= 0.995 ? col.red : col.ink; c.fillRect(xx, T1 - 3 - bh, ww, bh); }
        else { c.strokeStyle = col.lo; c.strokeRect(xx + 0.5, T1 - 3 - bh + 0.5, ww - 1, bh - 1); }
        c.fillStyle = w8 > 0.02 ? col.ink : col.dim; c.fillText(lab[i], xx, T0);
      }
    }
    c.fillStyle = col.red; c.fillRect(R0, T1 - 1, Math.round((R1 - R0) * lvv), 1);
  } else if (k === 7) {
    // STENCIL: the repeats of each line in time (the left above, the right below, mirrored; SKEW apart), as loud as
    // REPT leaves them; past 100 % or held, in red; FLIP hollow; HALO a cloud spreading under them, PING a line from
    // each repeat across to the other side
    const tl = fi ? Math.max(v6, 0.05) : 250, tr = fi ? Math.max(v7, 0.05) : 250, fb = fi ? v8 : sv(P.D) * 1.25;
    const held = fi && fi[10] > 0.5, flip = fi && fi[11] > 0.5, halo = sv(P.XB), ping = sv(P.XF);
    const span = 6.2 * Math.max(tl, tr), cy = (T0 + T1) / 2, hh = (T1 - T0) / 2 - 1;
    const TX = t => R0 + (t / span) * (R1 - R0), hot = held || fb > 1.001;
    for (const [row, tt] of [[-1, tl], [1, tr]]) {
      for (let i = 0; tt * i <= span && i < 16; i++) {
        const a = i === 0 ? 1 : Math.min(1, Math.pow(Math.max(held ? 1 : fb, 0), i)), bh = Math.max(1, Math.round(a * (hh - 1)));
        const xx = Math.round(TX(tt * i)), y0 = row < 0 ? Math.round(cy) - 1 - bh : Math.round(cy) + 1;
        if (halo > 0.01 && i > 0) {
          c.fillStyle = col.lo; const sp = Math.round(Math.min(1, halo * i * 0.5) * (TX(tt) - TX(0)));
          c.fillRect(xx, y0 + (row < 0 ? Math.round(bh * 0.4) : 0), Math.max(1, sp), Math.max(1, Math.round(bh * 0.6)));
        }
        if (i === 0) { c.fillStyle = col.dim; c.fillRect(xx, y0, 2, bh); continue; }
        if (flip) { c.strokeStyle = hot ? col.red : col.ink; c.strokeRect(xx + 0.5, y0 + 0.5, 1, Math.max(1, bh - 1)); }
        else { c.fillStyle = hot ? col.red : col.ink; c.fillRect(xx, y0, 2, bh); }
        if (ping > 0.05 && i % 2 === 1) { c.strokeStyle = col.dim; c.beginPath(); c.moveTo(xx + 1, Math.round(cy) + 0.5 - row * 2); c.lineTo(xx + 1 + Math.round(ping * 6), Math.round(cy) + 0.5 + row * 2); c.stroke(); }
      }
    }
    c.fillStyle = col.dim; c.fillRect(R0, Math.round(cy), R1 - R0, 1);
  } else {
    // CODEC: its bands (low to high), lit up to its bandwidth and as many as it keeps; a packet lost: hollow, in red
    const nb = 21, edges = [0, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400, 5300, 6400, 9500, 24000];
    const bwHz = fi ? v6 : 20000, kept = fi ? v7 : 1, lost = fi && v8 > 0, gw = (R1 - R0) / nb;
    for (let b = 0; b < nb; b++) {
      const inBand = edges[b] < bwHz, on = inBand && ((b * 7 + 3) % 10) / 10 < kept;
      const bh = Math.round((T1 - T0) * (0.35 + 0.65 * (1 - b / nb)));
      const xx = Math.round(R0 + b * gw), ww = Math.max(1, Math.round(gw) - 1);
      if (lost) { c.strokeStyle = col.red; c.strokeRect(xx + 0.5, T1 - bh + 0.5, ww - 1, bh - 1); }
      else if (on) { c.fillStyle = col.ink; c.fillRect(xx, T1 - bh, ww, bh); }
      else if (inBand) { c.fillStyle = col.lo; c.fillRect(xx, T1 - bh, ww, bh); }
    }
  }
  c.restore();
}
function drawRam(c, col, w, h, now) {
  const rs = recSide(), b = rs ? ui.t : ramPlayed(ui.t), own = b === ui.t;
  const info = ui.ramInfo, o = b * 6;
  const len = info ? info[o] : 0, rate = info ? info[o + 1] : 0, st = info ? info[o + 3] : 0, prog = info ? info[o + 4] : 0;
  const pk = ui.ramPeaks[b], blink = Math.floor(now / 400) % 2 === 0;
  // the tags: whose buffer, its length (and its rate when the take went to a lower one), the state or the play mode
  let x = lcdTag(c, col, rs ? 'REC SIDE' : own ? 'OWN BUFFER' : 'BUF · T' + (b + 1), 0, 0, 'inv') + 4;
  if (len > 0) x += lcdTag(c, col, fSec(len / rate) + (rate < 44000 ? ' · ' + Math.round(rate / 1000) + 'K' : ''), x, 0, 'box') + 4;
  if (st === 2) { if (blink || prog > 0) lcdTag(c, col, 'REC ' + Math.round(prog * 100) + '%', w, 0, 'inv', true, col.red); }
  else if (st === 1) { if (blink) lcdTag(c, col, 'ARMED', w, 0, 'box', true, col.red); }
  else lcdTag(c, col, rs ? 'FROM ' + RAM_SRC[idx(trk().p[P.RSRC], RAM_SRC.length)] : RAM_MODES[idx(shownValue(P.C).v, 4)], w, 0, 'box', true);
  const top = 14, H = h - top, mid = top + H / 2;
  // the window (PLAY side): STRT / LEN, on the slice grid when SLCE is on
  const ns = RAM_SLICES[idx(shownValue(P.D).v, RAM_SLICES.length)];
  let w0 = 0, w1 = 1;
  if (!rs) {
    const a = shownValue(P.A).v, l = shownValue(P.B).v;
    if (ns > 1) { w0 = idx(a, ns) / ns; w1 = Math.min(1, w0 + l / ns); } else { w0 = Math.min(a, 0.999); w1 = Math.min(1, w0 + l); }
  }
  lcdGrid(c, col, 0, top, w, H, ns > 1 ? Array.from({ length: ns - 1 }, (_, i) => (i + 1) / ns) : [0.25, 0.5, 0.75], [0.5]);
  lcdFrame(c, col, 0, top, w, H, [0.25, 0.5, 0.75], [0.5]);
  lcdClip(c, 0, top, w, H);
  if (!pk || !(len > 0 || st)) {
    c.font = col.font; c.fillStyle = col.dim; c.textAlign = 'center'; c.textBaseline = 'middle';
    c.fillText(rs ? 'A REC TRIG RECORDS A TAKE' : 'EMPTY · SWITCH TO REC, PLACE REC TRIGS', w / 2, mid);
    c.textAlign = 'left'; c.restore(); return;
  }
  const n = Math.max(2, Math.round(w - 2)), X = u => 1 + u * (w - 3), half = (H - 6) / 2;
  for (let i = 0; i < n; i++) {
    const u = i / n, a = Math.min(1, pk[Math.min(127, Math.floor(u * 128))] * 1.15);
    if (a < 0.004) continue;
    c.fillStyle = !rs && (u < w0 || u >= w1) ? col.dim : col.ink;
    const yh = Math.max(0.5, a * half);
    c.fillRect(Math.round(X(u)), Math.round(mid - yh), 1, Math.max(1, Math.round(2 * yh)));
  }
  if (!rs) {
    // the window's edges
    c.fillStyle = col.ink;
    for (const u of [w0, w1]) c.fillRect(Math.round(X(u)), top + 1, 1, H - 2);
  }
  if (st === 2 && own) { const xh = Math.round(X(prog)); c.fillStyle = col.red; c.fillRect(xh, top + 1, 1, H - 2); }
  c.restore();
}

// PRISM on the machine's screen, one view per page. Both draw the oscillators sounding now as lines on a log frequency
// axis (20 Hz .. 20 kHz, the height in dB down to -60). OSC: the wave they play (one cycle, and with DYN the one a
// loud partial reaches, dotted), the octave, and with QNT the notes they may land on (a comb along the bottom).
// SPEC: what is read from the take: the band or the notch (dimmed where nothing passes), the warp around the centre
// (where the octave marks go, along the top), the tilt (a dashed slope through 1 kHz), the speed and the slices.
const prismWaveY = (w, ph) => {           // one sample of the morphed wave (naive shapes, for the drawing only)
  const x = clamp(w, 0, 1) * 3, a = Math.min(2, Math.floor(x)), f = x - a;
  const sh = [ph => Math.sin(2 * Math.PI * ph), ph => { const q = (ph + 0.75) % 1; return q < 0.5 ? 4 * q - 1 : 3 - 4 * q; },
    ph => 1 - 2 * ph, ph => (Math.abs(ph - 0.25) < 0.125 || Math.abs(ph - 1.25) < 0.125 ? 0.75 : -0.25) * 1.6];
  return sh[a](ph) * (1 - f) + sh[a + 1](ph) * f;
};
function drawPrism(c, col, w, h) {
  const tr = trk(), sv = q => shownValue(q).v, page = prismPage();
  const mix = sv(P.PMIX), voices = 1 + idx(sv(P.PVOI), 16), spd = prismSpeed(sv(P.PSPD));
  let x = lcdTag(c, col, 'PRISM · ' + page, 0, 0, 'inv') + 4;
  if (page === 'OSC') {
    x += lcdTag(c, col, voices + ' VOIC', x, 0, 'box') + 4;
    const oct = idx(sv(P.POCT), 5) - 2;
    if (oct) x += lcdTag(c, col, 'OCT ' + PR_OCT[oct + 2], x, 0, 'box') + 4;
    if (idx(sv(P.PQNT), 2)) x += lcdTag(c, col, 'QNT ' + (state.scale ? scaleName(state.scale, state.qroot || 0) : 'SEMI'), x, 0, 'box') + 4;
    lcdTag(c, col, mix > 0.001 ? 'MIX ' + Math.round(mix * 100) + '%' : 'MIX OFF', w, 0, mix > 0.001 ? 'box' : 'inv', true);
  } else {
    x += lcdTag(c, col, spd ? 'SPED ×' + spd.toFixed(spd < 2 ? 2 : 1) : 'FREEZE', x, 0, spd ? 'box' : 'inv') + 4;
    const sl = idx(sv(P.PSLC), PR_SLICE.length);
    if (sl) x += lcdTag(c, col, 'SLIC ' + PR_SLICE[sl], x, 0, 'box') + 4;
    const band = prismBand(sv(P.PWID));
    lcdTag(c, col, band > 0 ? 'BAND' : band < 0 ? 'NOTCH' : 'FULL BAND', w, 0, 'box', true);
  }
  const top = 14, H = h - top, X = hz => 1 + Math.log2(Math.max(20, hz) / 20) / 10 * (w - 3);
  const octs = Array.from({ length: 9 }, (_, i) => (i + 1) / 10);
  lcdGrid(c, col, 0, top, w, H, octs, [1 / 3, 2 / 3]);
  lcdFrame(c, col, 0, top, w, H, octs, [1 / 3, 2 / 3]);
  lcdClip(c, 0, top, w, H);
  const cHz = prismHz(sv(P.PCTR));
  if (page === 'SPEC') {
    // the band or the notch: dimmed where nothing passes, dashed edges
    const wd = sv(P.PWID), band = prismBand(wd);
    if (band) {
      const hw = Math.pow(2, prismHalfOct(wd)), x0 = X(cHz / hw), x1 = X(cHz * hw);
      c.globalAlpha = 0.6; c.fillStyle = col.bg;
      if (band > 0) { c.fillRect(0, top, Math.max(0, x0), H); c.fillRect(x1, top, Math.max(0, w - x1), H); }
      else c.fillRect(x0, top, Math.max(0, x1 - x0), H);
      c.globalAlpha = 1;
      if (band < 0) {   // hatched: the notch
        c.strokeStyle = col.dim; c.lineWidth = 1;
        c.save(); c.beginPath(); c.rect(x0, top, Math.max(0, x1 - x0), H); c.clip();
        c.beginPath(); for (let xx = Math.floor(x0) - H; xx < x1; xx += 5) { c.moveTo(xx, top + H); c.lineTo(xx + H, top); } c.stroke(); c.restore();
      }
      c.setLineDash([2, 2]); c.strokeStyle = col.ink; c.lineWidth = 1; c.beginPath();
      for (const xx of [x0, x1]) { c.moveTo(Math.round(xx) + 0.5, top + 1); c.lineTo(Math.round(xx) + 0.5, top + H - 1); }
      c.stroke(); c.setLineDash([]);
    }
    // the centre (the warp's pivot): a mark on the top edge; the octave marks where WARP moves them, along the top
    const xc = Math.round(X(cHz));
    c.fillStyle = col.ink; c.beginPath(); c.moveTo(xc - 3, top + 1); c.lineTo(xc + 3, top + 1); c.lineTo(xc, top + 5); c.fill();
    const e = prismWarp(sv(P.PWRP));
    if (Math.abs(e - 1) > 0.002) for (let f = 40; f < 20000; f *= 2) {
      const fw = cHz * Math.pow(f / cHz, e); if (fw < 20 || fw > 20000) continue;
      c.fillStyle = col.dim; c.fillRect(Math.round(X(f)), top + 1, 1, 2);
      c.fillStyle = col.ink; c.fillRect(Math.round(X(fw)), top + 1, 1, 4);
    }
    // the tilt: a dashed slope through 1 kHz at mid height (+-4.5 dB per octave over a 60 dB height)
    const tl = (sv(P.PTLT) - 0.5) * 1.5 * 6.02;
    if (Math.abs(tl) > 0.05) {
      const Y = hz => top + H / 2 - (tl * Math.log2(hz / 1000)) / 60 * (H - 3);
      c.setLineDash([3, 2]); c.strokeStyle = col.dim; c.lineWidth = 1; c.beginPath();
      c.moveTo(X(20), Y(20)); c.lineTo(X(20000), Y(20000)); c.stroke(); c.setLineDash([]);
    }
  } else if (idx(sv(P.PQNT), 2)) {
    // the notes the oscillators may land on
    const m = state.scale || 0xFFF;
    c.fillStyle = col.dim;
    for (let n = 16; n < 136; n++) if ((m >> (n % 12)) & 1) { const hz = 440 * Math.pow(2, (n - 69) / 12); c.fillRect(Math.round(X(hz)), top + H - 3, 1, 2); }
  }
  if (mix <= 0.001) {
    c.font = col.font; c.fillStyle = col.dim; c.textAlign = 'center'; c.textBaseline = 'middle';
    c.fillText('MIX AT 0: PRISM IS OFF, THE TAKE PLAYS AS A SAMPLE', w / 2, top + H / 2);
    c.textAlign = 'left';
  } else {
    const pr = ui.prism;
    let n = 0;
    if (pr) for (let j = 0; j < 16; j++) {
      const hz = pr[2 * j], a = pr[2 * j + 1];
      if (!(hz > 0) || !(a > 1e-5)) continue;
      const db = 20 * Math.log10(a), u = clamp((db + 60) / 60, 0, 1), xx = Math.round(X(hz));
      c.fillStyle = col.ink; c.fillRect(xx, Math.round(top + H - 1 - u * (H - 3)), 1, Math.max(1, Math.round(u * (H - 3))));
      n++;
    }
    if (!n) {
      c.font = col.font; c.fillStyle = col.dim; c.textAlign = 'center'; c.textBaseline = 'middle';
      c.fillText(tr.mute ? 'TRACK MUTED' : 'PLAY THE TRACK: ITS PARTIALS SHOW HERE', w / 2, top + H / 2);
      c.textAlign = 'left';
    }
  }
  if (page === 'OSC') {
    // the wave, one cycle, in a box at the top right (with DYN, dotted: the wave of the loudest partial)
    const bw = Math.min(54, Math.round(w * 0.22)), bh = Math.min(22, Math.round(H * 0.42)), bx = w - bw - 3, by = top + 3;
    c.fillStyle = col.bg; c.fillRect(bx, by, bw, bh);
    c.strokeStyle = col.dim; c.lineWidth = 1; c.strokeRect(bx + 0.5, by + 0.5, bw - 1, bh - 1);
    const wave = sv(P.PWAV), dyn = sv(P.PDYN), curve = (wv, dash) => {
      c.setLineDash(dash); c.strokeStyle = col.ink; c.lineWidth = 1; c.beginPath();
      for (let i = 0; i <= bw - 6; i++) { const yy = by + bh / 2 - prismWaveY(wv, i / (bw - 6)) * (bh / 2 - 3) * 0.8; if (i) c.lineTo(bx + 3 + i, yy); else c.moveTo(bx + 3, yy); }
      c.stroke(); c.setLineDash([]);
    };
    if (dyn > 0.001) curve(wave + (1 - wave) * dyn, [1, 2]);
    curve(wave, []);
    c.font = col.font; c.fillStyle = col.ink; c.textBaseline = 'top'; c.textAlign = 'right';
    c.fillText(lcdText(fPWave(wave)), bx + bw - 3, by + bh + 2); c.textAlign = 'left';
  }
  c.restore();
}

// MIX display: x = frequency (40 Hz - 16 kHz, log), downwards = cut. Hairlines: unmasking per track and band (8 side
// by side, 12 dB the full height); the top edge bends down where a resonance is being calmed (24 dB the full height),
// a small square at the bottom of each notch
function drawMix(now) {
  const cv = $('mixcv'); if (!cv) return;
  const w = cv.clientWidth, h = cv.clientHeight; if (!w || !h) return;
  if (!ovwColors || now - ovwColorsAt > 1000) { ovwColors = readColors(); ovwColorsAt = now; }
  const col = ovwColors, dpr = DPR();
  if (cv.width !== Math.round(w * dpr) || cv.height !== Math.round(h * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); }
  const c = cv.getContext('2d');
  c.setTransform(dpr, 0, 0, dpr, 0, 0); c.clearRect(0, 0, w, h);
  const u = f => clamp(Math.log2(f / 40) / Math.log2(400), 0, 1), X = f => 1 + u(f) * (w - 3);
  const dec = [100, 1000, 10000].map(u), oct = [80, 160, 320, 640, 1280, 2560, 5120, 10240].map(u);
  lcdGrid(c, col, 0, 0, w, h, dec, [1 / 3, 2 / 3]);
  lcdFrame(c, col, 0, 0, w, h, oct, [1 / 3, 2 / 3]);
  const mx = ui.mix; if (!mx) return;
  lcdClip(c, 0, 0, w, h);
  c.fillStyle = col.ink; c.globalAlpha = 0.55;
  for (let b = 0; b < 10; b++) {
    const xc = X(50 * Math.pow(1.78, b));
    for (let t = 0; t < 8; t++) {
      const g = mx[16 + t * 10 + b];
      if (g <= -0.2) c.fillRect(Math.round(xc - 8 + t * 2), 1, 1, Math.min(h - 2, -g / 12 * (h - 2)));
    }
  }
  c.globalAlpha = 1;
  const nt = [];
  for (let k = 0; k < 8; k++) if (mx[2 * k + 1] <= -0.3) nt.push([mx[2 * k], Math.min(h - 4, -mx[2 * k + 1] / 24 * (h - 2))]);
  c.restore();
  if (!nt.length) return;
  const dip = x => { const f = 40 * Math.pow(400, (x - 1) / (w - 3)); let d = 0; for (const [fk, dk] of nt) d += dk * Math.exp(-Math.pow(Math.log2(f / fk) / 0.1, 2)); return Math.min(h - 3, d); };
  c.strokeStyle = col.ink; c.lineWidth = 1.2; c.beginPath();
  let pen = false;
  for (let x = 1; x <= w - 2; x++) {
    const d = dip(x);
    if (d > 0.35) { if (!pen) { c.moveTo(x - 1, 0.5); pen = true; } c.lineTo(x, 0.5 + d); }
    else if (pen) { c.lineTo(x, 0.5); pen = false; }
  }
  c.stroke();
  for (const [fk] of nt) { const x = Math.round(X(fk)), y = Math.round(0.5 + dip(X(fk))); c.fillStyle = col.bg; c.fillRect(x - 3, y - 2, 6, 6); c.strokeStyle = col.ink; c.lineWidth = 1; c.strokeRect(x - 2.5, y - 1.5, 5, 5); }
}

// the MASTER screen's drawing of the page shown (redrawn when a setting changes), in lines: a header (what it is,
// reversed; the values, framed), then the plot in a frame with its graticule. REV: the impulse response (the
// reflections as hairlines, the envelope, the highs' decay dashed, the pre-delay bracketed); FX: the delay's echoes
// (left up, right down) over 2 bars; EQ: its curve; COMP: the transfer curve in a square, then the gain reduction of
// a burst over time (ATK, REL); DRV: the shaper in a square, then a sine through it. An effect that is off: in dim
const DLY_STEPS = [0.5, 2 / 3, 1, 4 / 3, 1.5, 2, 8 / 3, 3, 4, 6, 8];
const COMP_R = [1, 1, 2, 3, 4, 6, 10, 20, 60, 30];
const vizKeys = { mviz: '', rviz: '', dviz: '', cviz: '', pviz: '' };
function drawMasterViz() { drawViz('mviz', PAGES[ui.gpage].id); drawViz('rviz', 'REV'); drawViz('dviz', 'DLY'); drawViz('cviz', 'CHO'); drawViz('pviz', 'PHS'); }
// the PHASER's photocells over a stretch of time (as core/sk_send.cpp: brightening in 4 ms, darkening in 15 .. 80 ms), the
// stages' corner they give (Hz)
function phaserCorners(g, secs, n) {
  const shp = (x, sh) => { const k = 40 * sh * sh; return x * (1 + k) / (1 + k * Math.abs(x)); };
  const r1 = 0.1 * Math.pow(180, g[G.PRT1]), r2 = 0.1 * Math.pow(180, g[G.PRT2]), bG2 = idx(g[G.PBSW], 2), rev = idx(g[G.PSYN], 2);
  const LMIN = 0.008, sr = 2000, dt = 1 / sr, kOn = 1 - Math.exp(-dt / 0.004), kOff = 1 - Math.exp(-dt / 0.015), kOffD = 1 - Math.exp(-dt / 0.08);
  let lA = LMIN, lB = LMIN; const A = [], B = [];
  const steps = Math.round(secs * sr), warm = Math.round(Math.max(1, 3 / Math.min(r1, r2)) * sr);
  for (let i = -Math.min(warm, 20000); i < steps; i++) {
    const t = Math.max(0, i) * dt, g1 = shp(Math.sin(2 * Math.PI * r1 * (i * dt)), g[G.PSH1]), g2 = shp(Math.sin(2 * Math.PI * (r2 * i * dt + 0.25)), g[G.PSH2]);
    const gb = (bG2 ? g2 : g1) * (rev ? -1 : 1);
    const tA = LMIN + g[G.PDPA] * (0.5 + 0.5 * g1) * (1 - LMIN), tB = LMIN + g[G.PDPB] * (0.5 + 0.5 * gb) * (1 - LMIN);
    lA += (tA - lA) * (tA > lA ? kOn : kOff + (kOffD - kOff) * (1 - lA)); lB += (tB - lB) * (tB > lB ? kOn : kOff + (kOffD - kOff) * (1 - lB));
    if (i >= 0 && i % Math.max(1, Math.round(steps / n)) === 0) { A.push(4800 * Math.pow(Math.max(lA, LMIN), 0.8)); B.push(4800 * Math.pow(Math.max(lB, LMIN), 0.8)); }
    void t;
  }
  return [A, B];
}
function drawViz(cvId, pid) {
  const cv = $(cvId); if (!cv) return;
  const w = cv.clientWidth, h = cv.clientHeight; if (!w || !h) return;
  const g = state.g;
  const key = [pid, w, h, DPR(), document.documentElement.dataset.theme || '', revAlgo(), document.fonts ? document.fonts.status : '',
    ...(pid === 'REV' ? [G.RTIM, G.RSIZ, G.RDMP, G.RPRE, G.RVON] : pid === 'DLY' ? [G.DTIM, G.DFB, G.DPNG, G.DLON, G.DMDL, G.DBW, G.DVDP, G.DVRT, G.DVSH, G.DHLD]
      : pid === 'CHO' ? [G.CHMD, G.CHIN, G.CHLV, G.CHON] : pid === 'PHS' ? [G.PRT1, G.PSH1, G.PRT2, G.PSH2, G.PDPA, G.PDPB, G.PBSW, G.PSYN, G.PRTE, G.PHON]
      : pid === 'EQ' ? [G.EQLF, G.EQLB, G.EQLA, G.EQHF, G.EQHB, G.EQBW, G.EQHA, G.EQHS, G.EQON] : pid === 'COMP' ? [G.CINP, G.CATK, G.CREL, G.CRAT, G.CMIX, G.COON]
      : pid === 'DRV' ? [G.DMOD, G.DDRV, G.DBIA, G.DMIX, G.DRON] : []).map(k => g[k].toFixed(4))].join();
  if (key === vizKeys[cvId]) return;
  vizKeys[cvId] = key;
  const dpr = DPR();
  if (cv.width !== Math.round(w * dpr) || cv.height !== Math.round(h * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); }
  const col = ovwColors || (ovwColors = readColors()), c = cv.getContext('2d');
  c.setTransform(dpr, 0, 0, dpr, 0, 0); c.clearRect(0, 0, w, h);
  const sw = { REV: G.RVON, DLY: G.DLON, CHO: G.CHON, PHS: G.PHON, EQ: G.EQON, COMP: G.COON, DRV: G.DRON }[pid];
  const on = sw == null || g[sw] > 0.5, ink = on ? col.ink : col.dim;
  const py = 14, ph = h - py;                                                  // the plot, under the header
  // the header: the name reversed on the left, the values framed from the right (as many as fit)
  const head = (name, vals) => {
    const x0 = lcdTag(c, col, name, 0, 0, 'inv', false, ink) + 8;
    c.font = col.font;
    let v = vals.filter(([t]) => t).map(([t, kind]) => [t, kind || 'box', Math.ceil(c.measureText(lcdText(t)).width) + 10]);
    const room = () => v.reduce((a, x) => a + x[2], 0) <= w - x0;
    while (v.length && !room()) { const i = v.findIndex(x => x[1] === 'txt'); v.splice(i >= 0 ? i : v.length - 1, 1); }
    let x = w - v.reduce((a, x) => a + x[2], 0) + 3;
    for (const [t, kind] of v) x += lcdTag(c, col, t, x, 0, kind, false, kind === 'txt' ? col.dim : null) + 3;
  };
  const path = (n, fx, fy) => { c.beginPath(); for (let i = 0; i <= n; i++) { const x = fx(i), y = fy(i); i ? c.lineTo(x, y) : c.moveTo(x, y); } };
  const square = (x, y, s) => { c.fillStyle = col.bg; c.fillRect(Math.round(x) - 3, Math.round(y) - 3, 7, 7); c.strokeStyle = ink; c.lineWidth = 1; c.strokeRect(Math.round(x) - 2.5, Math.round(y) - 2.5, 5, 5); };
  const gv = k => gMeta(k).l + ' ' + gMeta(k).f(g[k]);
  if (pid === 'REV') {
    const t60 = Math.min(40, REV_SEC[revAlgo()](g[G.RTIM])), pre = 0.25 * g[G.RPRE] * g[G.RPRE];
    const span = Math.max(0.6, pre + t60 * 1.15), X = t => 3 + t / span * (w - 5), base = py + ph - 2, amp = ph - 9;
    const div = [0.05, 0.1, 0.2, 0.25, 0.5, 1, 2, 5, 10, 20].find(d => span / d <= 8) || 20;
    const xs = []; for (let t = div; t < span; t += div) xs.push(X(t) / w);
    head(REV_DESC[revAlgo()], [[fTime(div) + '/DIV', 'txt'], [pre > 0.002 ? 'PRE ' + fTime(pre) : ''], ['RT60 ' + fT60(t60)]]);
    lcdGrid(c, col, 0, py, w, ph, xs, [0.5]);
    lcdFrame(c, col, 0, py, w, ph, xs, [0.25, 0.5, 0.75]);
    lcdClip(c, 0, py, w, ph);
    c.fillStyle = ink; c.fillRect(Math.round(X(0)), py + 4, 1, ph - 5);                        // the hit
    let seed = 11; const rnd = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
    c.globalAlpha = 0.55;
    for (let x = Math.ceil(X(pre)) + 1; x < w - 1; x += 2) {                                   // the reflections
      const t = (x - 3) / (w - 5) * span - pre, a = Math.exp(-6.9 * t / t60) * (0.5 + 0.5 * rnd()), hh = a * amp;
      if (hh >= 0.6) c.fillRect(x, base - hh, 1, hh);
    }
    c.globalAlpha = 1;
    const x0 = X(pre), n = Math.max(2, Math.round(w - x0));
    path(n, i => x0 + i / n * (w - x0), i => base - Math.exp(-6.9 * (i / n * (w - x0)) / (w - 5) * span / t60) * amp);
    c.strokeStyle = ink; c.lineWidth = 1.2; c.stroke();
    const hf = Math.max(0.02, t60 * (1 - 0.85 * g[G.RDMP]));                                    // the highs die sooner (DAMP)
    path(n, i => x0 + i / n * (w - x0), i => base - Math.exp(-6.9 * (i / n * (w - x0)) / (w - 5) * span / hf) * amp);
    c.setLineDash([2, 2]); c.strokeStyle = col.dim; c.lineWidth = 1; c.stroke(); c.setLineDash([]);
    if (pre > 0.002) {                                                                          // the pre-delay, bracketed
      const a = Math.round(X(0)) + 0.5, b = Math.round(X(pre)) + 0.5, y = py + 3.5;
      c.strokeStyle = col.dim; c.lineWidth = 1; c.beginPath(); c.moveTo(a, y); c.lineTo(b, y); c.moveTo(b, y - 1); c.lineTo(b, y + 3); c.stroke();
    }
    c.restore();
  } else if (pid === 'DLY') {
    const dt = DLY_STEPS[idx(g[G.DTIM], 11)], fb = Math.min(1.05, g[G.DFB] * 1.1), p = g[G.DPNG];
    const mid = Math.round(py + ph / 2), amp = ph / 2 - 4, X = st => Math.round(3 + st / 32 * (w - 5));
    const beats = [], bars = [];
    for (let b = 4; b < 32; b += 4) (b % 16 ? beats : bars).push((X(b) + 0.5) / w);
    const m82 = idx(g[G.DMDL], 2) === 1;
    head(m82 ? 'DELAY 1982 ' + (idx(g[G.DBW], 2) ? '×2 6K' : '×1 16K') : 'DELAY', [['2 BAR', 'txt'], [g[G.DHLD] > 0.5 ? 'HOLD' : ''], [gv(G.DTIM)], [gv(G.DFB)], [g[G.DVDP] > 0.002 ? gv(G.DVDP) : gv(G.DPNG)]]);
    lcdGrid(c, col, 0, py, w, ph, beats, [0.5]);
    lcdFrame(c, col, 0, py, w, ph, [...beats, ...bars], [0.25, 0.75]);
    lcdClip(c, 0, py, w, ph);
    c.fillStyle = col.dim; for (const b of bars) for (let y = py + 2; y < py + ph - 2; y += 3) c.fillRect(Math.round(b * w), y, 1, 1);   // the bar line, dotted
    c.fillStyle = ink; c.fillRect(X(0), mid - amp, 1, 2 * amp + 1);                            // the hit, both sides
    let a = 0.9;
    const hld = g[G.DHLD] > 0.5, wob = g[G.DVDP] * g[G.DVDP];
    for (let k = 1; X(k * dt) < w - 2 && a > 0.03 && k < 64; k++, a *= hld ? 1 : fb) {
      const odd = k % 2 === 1, l = a * (odd ? 1 : 1 - p), r = a * (odd ? 1 - p : 1), x = X(k * dt * (1 + 0.12 * wob * Math.sin(k * 1.7)));
      if (l * amp >= 0.5) { const t = Math.round(mid - l * amp); c.fillRect(x, t, 1, mid - t); }
      if (r * amp >= 0.5) { const t = Math.round(mid + 1 + r * amp); c.fillRect(x, mid + 1, 1, t - mid - 1); }
    }
    // how the echoes die away, both sides, dashed
    const e = st => 0.9 * Math.pow(fb, st / dt - 1) * amp, x1 = X(dt);
    let x2 = w - 2; if (fb < 0.999) x2 = Math.min(x2, X(dt * (1 + Math.log(1 / (0.9 * amp)) / Math.log(fb))));   // until it is under a pixel
    const n = Math.max(2, Math.round(x2 - x1));
    if (fb > 0.02 && x2 > x1 + 4) for (const sg of [-1, 1]) {
      path(n, i => x1 + i / n * (x2 - x1) + 0.5, i => mid + (sg > 0 ? 1 : 0) + sg * e(dt + i / n * (x2 - x1) / (w - 5) * 32));
      c.setLineDash([2, 2]); c.strokeStyle = col.dim; c.lineWidth = 1; c.stroke(); c.setLineDash([]);
    }
    c.restore();
  } else if (pid === 'CHO') {
    // CHORUS: the two lines' delays over 8 s (left solid, right dashed), swept in antiphase by the mode's triangle
    const m = idx(g[G.CHMD], 4), rate = [0.25, 0.25, 0.25, 0.5][m], dep = [0.067, 0.122, 0.189, 0.244][m], d0 = 4.5, span = 8;
    const lo = d0 / (1 + 0.26), hi = d0 / (1 - 0.26), Y = ms => py + 2 + (1 - (ms - lo) / (hi - lo)) * (ph - 5), X = t => 2 + t / span * (w - 4);
    head('CHORUS · MODE ' + (m + 1), [[span + ' S', 'txt'], [rate + 'HZ'], ['±' + Math.round(dep * 100) + '%'], [idx(g[G.CHIN], 2) ? 'ST' : 'MONO']]);
    const xs = []; for (let t = 1; t < span; t++) xs.push(X(t) / w);
    lcdGrid(c, col, 0, py, w, ph, xs, [(Y(d0) - py) / ph]);
    lcdFrame(c, col, 0, py, w, ph, xs, [0.5]);
    lcdClip(c, 0, py, w, ph);
    const n = Math.max(2, Math.round(w - 4)), tri = t => { const ph0 = (t * rate) % 1; return 4 * Math.abs(ph0 - 0.5) - 1; };
    for (const sg of [1, -1]) {
      path(n, i => X(i / n * span), i => Y(d0 / (1 + sg * dep * tri(i / n * span))));
      if (sg < 0) c.setLineDash([2, 2]);
      c.strokeStyle = ink; c.lineWidth = sg > 0 ? 1.2 : 1; c.stroke(); c.setLineDash([]);
    }
    c.restore();
  } else if (pid === 'PHS') {
    // PHASER: where its notches are over 4 s (a log axis, 40 Hz .. 16 kHz): A's three (solid), B's (dashed; SERIES: six)
    const span = 4, X = t => 2 + t / span * (w - 4), lf = f => Math.log(f / 40) / Math.log(400), Y = f => py + ph - 2 - clamp(lf(f), 0, 1) * (ph - 4);
    const route = idx(g[G.PRTE], 3);
    head('PHASER · ' + PH_ROUTES[route], [[span + ' S', 'txt'], [gv(G.PRT1)], [idx(g[G.PBSW], 2) ? gv(G.PRT2) : 'B ON GEN1'], [idx(g[G.PSYN], 2) ? 'REV' : 'NORM']]);
    lcdGrid(c, col, 0, py, w, ph, [1, 2, 3].map(t => X(t) / w), [100, 1000, 10000].map(f => (Y(f) - py) / ph));
    lcdFrame(c, col, 0, py, w, ph, [1, 2, 3].map(t => X(t) / w), [100, 1000, 10000].map(f => (Y(f) - py) / ph));
    lcdClip(c, 0, py, w, ph);
    const n = Math.max(2, Math.round(w - 4)), [A, B] = phaserCorners(g, span, n);
    const N6 = [0.268, 1, 3.732], N12 = [0.132, 0.414, 0.767, 1.303, 2.414, 7.596];
    const lines = (arr, mult, dashed, combine) => { for (const q of mult) {
      c.beginPath(); for (let i = 0; i < arr.length; i++) { const f = (combine ? Math.sqrt(arr[i] * combine[i]) : arr[i]) * q, x = X(i / arr.length * span), y = Y(f); i ? c.lineTo(x, y) : c.moveTo(x, y); }
      if (dashed) c.setLineDash([2, 2]); c.strokeStyle = dashed ? col.dim : ink; c.lineWidth = 1; c.stroke(); c.setLineDash([]); } };
    lines(A, N6, false);
    if (route === 2) lines(B, N12, true, A); else lines(B, N6, true);
    c.restore();
  } else if (pid === 'EQ') {
    const F = [20, 30, 60, 100][idx(g[G.EQLF], 4)], HF = [3, 4, 5, 8, 10, 12, 16][idx(g[G.EQHF], 7)] * 1000;
    const HS = [5, 10, 20][idx(g[G.EQHS], 3)] * 1000, bw = 0.4 + 2.2 * g[G.EQBW];
    const lb = g[G.EQLB] * 13.5, la = -g[G.EQLA] * 17.5, hb = g[G.EQHB] * 16, ha = -g[G.EQHA] * 16;
    const db = f => lb / (1 + Math.pow(f / F, 2)) + la / (1 + Math.pow(f / (F * 1.6), 2)) + hb / (1 + Math.pow(Math.log2(f / HF) / (bw * 0.5), 2))
      + ha / (1 + Math.pow(HS / f, 2));
    const u = f => Math.log(f / 20) / Math.log(1000), X = f => 1 + u(f) * (w - 3), Y = d => py + ph / 2 - d / 18 * (ph / 2 - 3);
    const fy = d => (Y(d) - py) / ph;
    head('EQ', [['±18DB', 'txt'], [gv(G.EQLF)], [gv(G.EQHF)], [gv(G.EQHS)]]);
    lcdGrid(c, col, 0, py, w, ph, [100, 1000, 10000].map(u), [12, 0, -12].map(fy));
    lcdFrame(c, col, 0, py, w, ph, [50, 100, 200, 500, 1000, 2000, 5000, 10000].map(u), [12, 6, 0, -6, -12].map(fy));
    lcdClip(c, 0, py, w, ph);
    const n = Math.max(2, Math.round(w - 3)), fq = i => 20 * Math.pow(1000, i / n);
    path(n, i => 1 + i / n * (w - 3), i => Y(db(fq(i))));
    c.lineTo(w - 2, Y(0)); c.lineTo(1, Y(0)); c.closePath();
    c.globalAlpha = 0.09; c.fillStyle = ink; c.fill(); c.globalAlpha = 1;
    path(n, i => 1 + i / n * (w - 3), i => Y(db(fq(i))));
    c.strokeStyle = ink; c.lineWidth = 1.2; c.stroke();
    c.restore();
    square(X(F), Y(db(F))); square(X(HF), Y(db(HF)));
  } else if (pid === 'COMP') {
    const r = COMP_R[idx(g[G.CRAT], COMP_R.length)], thr = -16 - g[G.CINP] * 30, knee = 6, mix = g[G.CMIX];
    const S = ph, X = d => 1 + (d + 60) / 60 * (S - 3), Y = d => py + ph - 2 - (d + 60) / 60 * (ph - 3);
    const out = d => { const q = d - thr; let y = d; if (2 * q > knee) y = thr + q / r; else if (2 * Math.abs(q) <= knee) y = d + (1 / r - 1) * Math.pow(q + knee / 2, 2) / (2 * knee); return mix * y + (1 - mix) * d; };
    head('COMP', [[gv(G.CRAT)], [gv(G.CINP)], [gv(G.CATK)], [gv(G.CREL)]]);
    const q = [0.2, 0.4, 0.6, 0.8];
    lcdGrid(c, col, 0, py, S, ph, q, q);
    lcdFrame(c, col, 0, py, S, ph, q, q);
    lcdClip(c, 0, py, S, ph);
    c.setLineDash([2, 2]); c.strokeStyle = col.dim; c.lineWidth = 1; c.beginPath(); c.moveTo(X(-60), Y(-60)); c.lineTo(X(0), Y(0)); c.stroke(); c.setLineDash([]);
    path(120, i => X(-60 + i / 2), i => Y(out(-60 + i / 2)));
    c.strokeStyle = ink; c.lineWidth = 1.2; c.stroke();
    c.restore();
    square(X(thr), Y(out(thr)));
    // a burst at 0 dB through it: the gain reduction, downwards, with the attack and the release
    const x2 = S + 6, w2 = w - x2;
    if (w2 > 40) {
      const atk = 0.00005 * Math.pow(600, g[G.CATK]), rel = 0.05 * Math.pow(70, g[G.CREL]);
      const gr0 = Math.max(0, -out(0)), gr = -20 * Math.log10(mix * Math.pow(10, -gr0 / 20) + (1 - mix)) || 0;
      const span = 1.25 * (8 * atk + 4 * rel) + 0.02, t0 = 0.06 * span, t1 = 0.4 * span;
      const env = t => t < t0 ? 0 : t < t1 ? gr * (1 - Math.exp(-(t - t0) / atk)) : gr * (1 - Math.exp(-(t1 - t0) / atk)) * Math.exp(-(t - t1) / rel);
      const TX = t => x2 + 1 + t / span * (w2 - 3), GY = d => py + 0.5 + Math.min(24, d) / 24 * (ph - 4);
      lcdGrid(c, col, x2, py, w2, ph, null, [1 / 4, 2 / 4, 3 / 4]);
      c.fillStyle = col.lo; c.globalAlpha = 0.6; c.fillRect(Math.round(TX(t0)), py + 1, Math.round(TX(t1) - TX(t0)), ph - 2); c.globalAlpha = 1;   // the burst
      lcdFrame(c, col, x2, py, w2, ph, [(TX(t0) - x2) / w2, (TX(t1) - x2) / w2], [1 / 4, 2 / 4, 3 / 4]);
      const n = Math.max(2, Math.round(w2 - 3));
      c.beginPath(); let pen = false;
      for (let i = 0; i <= n; i++) {
        const t = i / n * span, d = env(t);
        if (d > 0.05) { if (!pen) { c.moveTo(TX(t), py + 0.5); pen = true; } c.lineTo(TX(t), GY(d)); }
        else if (pen) { c.lineTo(TX(t), py + 0.5); pen = false; }
      }
      c.strokeStyle = ink; c.lineWidth = 1.2; c.stroke();
      lcdTag(c, col, 'GR ' + (gr < 0.05 ? '0.0' : '−' + gr.toFixed(1)) + 'DB', w - 3, py + ph - 14, 'box', true);
    }
  } else if (pid === 'DRV') {
    const m = idx(g[G.DMOD], DRIVE_MODELS.length), k = Math.pow(10, g[G.DDRV] * 30 / 20), b = (g[G.DBIA] - 0.5) * 0.6, mix = g[G.DMIX];
    const shape = x => { const u = x * k + b;
      switch (m) { case 0: return x; case 1: return Math.tanh(u); case 2: return Math.tanh(u + 0.25 * u * u); case 3: return u > 0 ? Math.tanh(u) : Math.tanh(u * 0.6) / 0.6 * 0.7;
        case 4: return Math.tanh(1.6 * u); case 5: return Math.max(-1, Math.min(1, 1.2 * u - 0.2 * u * u * u)); case 6: return Math.sign(u) * (1 - Math.exp(-2.5 * Math.abs(u)));
        default: return Math.sin(Math.max(-3, Math.min(3, u)) * 1.3); } };
    const tf = x => Math.max(-1, Math.min(1, mix * shape(x) + (1 - mix) * x));
    const S = ph, X = x => 1 + (x + 1) / 2 * (S - 3), Y = y => py + 1 + (1 - (y + 1) / 2) * (ph - 3);
    head(DRIVE_MODELS[m], [[gv(G.DDRV)], [gv(G.DBIA)], [gv(G.DMIX)]]);
    const q = [0.25, 0.5, 0.75];
    lcdGrid(c, col, 0, py, S, ph, q, q);
    lcdFrame(c, col, 0, py, S, ph, q, q);
    lcdClip(c, 0, py, S, ph);
    c.setLineDash([2, 2]); c.strokeStyle = col.dim; c.lineWidth = 1; c.beginPath(); c.moveTo(X(-1), Y(-1)); c.lineTo(X(1), Y(1)); c.stroke(); c.setLineDash([]);
    path(160, i => X(-1 + i / 80), i => Y(tf(-1 + i / 80)));
    c.strokeStyle = ink; c.lineWidth = 1.2; c.stroke();
    c.restore();
    // two cycles of a sine through it: in (dashed), out
    const x2 = S + 6, w2 = w - x2;
    if (w2 > 40) {
      const SX = i => x2 + 1 + i / 400 * (w2 - 3), SY = v => py + ph / 2 - v * (ph / 2 - 3), sn = i => 0.9 * Math.sin(i / 400 * 4 * Math.PI);
      lcdGrid(c, col, x2, py, w2, ph, [0.25, 0.5, 0.75], [0.5]);
      lcdFrame(c, col, x2, py, w2, ph, [0.25, 0.5, 0.75], [0.25, 0.5, 0.75]);
      lcdClip(c, x2, py, w2, ph);
      path(400, SX, i => SY(sn(i))); c.setLineDash([2, 2]); c.strokeStyle = col.dim; c.lineWidth = 1; c.stroke(); c.setLineDash([]);
      path(400, SX, i => SY(tf(sn(i)))); c.strokeStyle = ink; c.lineWidth = 1.2; c.stroke();
      c.restore();
    }
  }
}

// master meters: gain reduction (dB) and output peak (dBFS), with a short hold and a fall-off
function drawMeters(now) {
  const m = ui.mtr, dt = Math.min(0.1, (now - (m.t || now)) / 1000); m.t = now;
  m.grShow = Math.max(m.gr, m.grShow - dt * 18);
  const outDb = m.out > 1e-5 ? 20 * Math.log10(m.out) : -90;
  m.outShow = Math.max(outDb, m.outShow - dt * 20);
  m.gr = 0; m.out = 0;
  const gr = $('mgr'), out = $('mout');
  if (gr) {
    gr.style.clipPath = `inset(0 0 0 ${(100 - clamp(m.grShow / 20, 0, 1) * 100).toFixed(1)}%)`;
    $('mgrv').textContent = m.grShow < 0.05 ? '0.0' : '−' + m.grShow.toFixed(1);
  }
  if (out) {
    out.style.clipPath = `inset(0 ${(100 - clamp((m.outShow + 40) / 40, 0, 1) * 100).toFixed(1)}% 0 0)`;
    out.classList.toggle('hot', m.outShow > -1);
    $('moutv').textContent = m.outShow < -60 ? '−∞' : m.outShow.toFixed(1);
  }
}

// playhead + pad flashes
let lastPh = -1;
function frame(now) {
  drawFx(now);
  const run = ui.pos.run;
  if (ui.rec && ui.recHold != null && run && ui.pos.steps[ui.t] !== ui.recStep) { ui.recStep = ui.pos.steps[ui.t]; recordLock(ui.recHold); }
  // the playhead on the page shown (the PAGE key of the page it plays lit red)
  const phAbs = run && !ui.ptn && !ui.key ? ui.pos.steps[ui.t] : -1, ph = phAbs >= 0 && Math.floor(phAbs / NS) === (ui.spage | 0) ? phAbs % NS : -1;
  if (ph !== lastPh) {
    if (lastPh >= 0 && steps[lastPh]) steps[lastPh].classList.remove('ph');
    if (ph >= 0 && steps[ph]) steps[ph].classList.add('ph');
    lastPh = ph;
  }
  const php = phAbs >= 0 ? Math.floor(phAbs / NS) : -1;
  if (php !== ui.phPage) { ui.phPage = php; renderPages(); }
  if (ui.ptn && ui.patBlink !== (Math.floor(now / 250) & 1)) { ui.patBlink = Math.floor(now / 250) & 1; $('steps').classList.toggle('blink', !!ui.patBlink); }
  for (let t = 0; t < NT; t++) {
    const h = ui.pos.hits[t];
    if (h !== ui.seen[t]) { ui.seen[t] = h; ui.flash[t] = now + 90; }
    pads[t].classList.toggle('hit', ui.flash[t] > now);
  }
  if (inPlugin) renderTransport();
  drawOverview(now);
  westLive(now);
  drawMeters(now);
  drawMix(now);
  drawMasterViz();
  drawSpec(now);
  drawLoopMap();
  renderStepsLoop(false);
  requestAnimationFrame(frame);
}

// ------------------------------------------------------------------ controls
function dragReadout(el, get, set, perPx) {
  let d = null;
  el.addEventListener('pointerdown', e => { e.preventDefault(); el.setPointerCapture(e.pointerId); d = { y: e.clientY, base: get() }; });
  el.addEventListener('pointermove', e => { if (!d) return; set(d.base + (d.y - e.clientY) * perPx); renderTransport(); });
  el.addEventListener('pointerup', () => { d = null; });
  el.addEventListener('pointercancel', () => { d = null; });
  el.addEventListener('keydown', e => {
    const s = e.key === 'ArrowUp' ? 1 : e.key === 'ArrowDown' ? -1 : 0;
    if (s) { e.preventDefault(); set(get() + s * perPx * 4); renderTransport(); }
  });
}

function armed(btn, label, action) {
  let timer = 0;
  btn.addEventListener('click', () => {
    if (btn.classList.contains('armed')) { clearTimeout(timer); btn.classList.remove('armed'); btn.textContent = label; action(); return; }
    btn.classList.add('armed'); btn.textContent = 'Sure?';
    timer = setTimeout(() => { btn.classList.remove('armed'); btn.textContent = label; }, 2500);
  });
}

// track length: touch a cell (or drag along them) and the track ends there; speed: a readout to drag
function setLen(n) {
  const tr = trk(); n = clamp(n, 1, NSTEP);
  if (tr.len === n) return;
  tr.len = n; send(['len', ui.t, n]);
  if ((ui.spage | 0) * NS >= n) ui.spage = Math.floor((n - 1) / NS);
  touched(); renderSteps(); renderFooter(); renderStepsLoop(true);
}
// PAGE keys: 4 pages of 16 steps (as many lit as the track's LEN reaches). A tap shows a page (one past LEN: the track
// grows to its end); held, a step key sets LEN there
const pagesOf = tr => Math.ceil(tr.len / NS);
function pageTap(p) {
  const tr = trk();
  if (p >= pagesOf(tr)) setLen((p + 1) * NS);
  ui.spage = p; renderSteps(); renderFooter(); renderStepsLoop(true); renderScreen();
}
// a page key tapped twice copies its 16 steps (the track's; under Control all, every track's in it), three times pastes
// the copy there (a copy of one track goes onto the track shown, one of several onto the same tracks). The lockbar
// says what was done, with an undo after a paste
const PAGE_TAP_MS = 380;
function pageMulti(p) {
  const now = performance.now(), k = ui.pgTaps;
  const n = k && k.p === p && now - k.t < PAGE_TAP_MS ? k.n + 1 : 1;
  ui.pgTaps = n >= 3 ? null : { p, n, t: now };
  clearTimeout(ui.pgTimer);
  if (n === 2) ui.pgTimer = setTimeout(() => pageCopy(p), PAGE_TAP_MS);
  else if (n >= 3) pagePaste(p);
}
const pageSteps = (tr, p) => tr.steps.slice(p * NS, p * NS + NS).map(st => JSON.parse(JSON.stringify(st)));
function pageSay(txt, undo) { ui.pageMsg = { txt, undo: undo || null, until: performance.now() + (undo ? 5000 : 2200) }; renderLockbar();
  clearTimeout(ui.pageMsgT); ui.pageMsgT = setTimeout(() => { ui.pageMsg = null; renderLockbar(); }, undo ? 5100 : 2300); }
function pageCopy(p) {
  const ts = ui.all ? state.tracks.map((t, i) => (t.ax ? -1 : i)).filter(i => i >= 0) : [ui.t];
  ui.pageClip = { p, tracks: ts.map(t => ({ t, steps: pageSteps(state.tracks[t], p) })) };
  pageSay(`PAGE ${p + 1} COPIED · ${ts.length > 1 ? ts.length + ' TRACKS' : 'T' + (ts[0] + 1)} · 3× ON A PAGE: PASTE`);
}
// the steps of a page onto a track, the engine told (a step's trig, locks, ratchet, trigless lock, REC trig)
function pageWrite(t, p, steps) {
  const tr = state.tracks[t], ops = [];
  steps.forEach((src, k) => {
    const si = p * NS + k, st = JSON.parse(JSON.stringify(src));
    if (!st.l) st.l = {};
    tr.steps[si] = st;
    ops.push(['trig', t, si, st.on ? 1 : 0], ['clrl', t, si], ['rat', t, si, st.r > 1 ? st.r : 1], ['tlk', t, si, st.tl && !st.on ? 1 : 0], ['rec', t, si, st.rec ? 1 : 0]);
    for (const key in st.l) ops.push(['lock', t, si, +key, st.l[key]]);
  });
  backend.sendAll(ops);
}
function pagePaste(p) {
  clearTimeout(ui.pgTimer);
  const c = ui.pageClip;
  if (!c) { pageSay('NOTHING COPIED · 2× ON A PAGE: COPY'); return; }
  const dst = c.tracks.length === 1 ? [{ t: ui.t, steps: c.tracks[0].steps }] : c.tracks;
  const before = dst.map(d => ({ t: d.t, steps: pageSteps(state.tracks[d.t], p) }));
  for (const d of dst) pageWrite(d.t, p, d.steps);
  touched(); renderSteps(); renderFooter(); renderScreen();
  pageSay(`PAGE ${c.p + 1} PASTED ONTO PAGE ${p + 1} · ${dst.length > 1 ? dst.length + ' TRACKS' : 'T' + (dst[0].t + 1)}`, () => {
    for (const b of before) pageWrite(b.t, p, b.steps);
    touched(); renderSteps(); renderFooter(); renderScreen();
  });
}
function buildPages() {
  const host = $('pages');
  for (let p = 0; p < NSTEP / NS; p++) {
    const b = document.createElement('button');
    b.className = 'pgk'; b.dataset.p = String(p); b.innerHTML = `<i></i><span>${p + 1}</span>`;
    b.title = `Page ${p + 1}: steps ${p * NS + 1}–${(p + 1) * NS}. Hold it and tap a step: the track's length. Tap it twice: copy the page; three times: paste`;
    b.addEventListener('pointerdown', e => { e.preventDefault(); if (ui.ptn) return; ui.pageHold = p; ui.pageHeldUsed = false; try { b.setPointerCapture(e.pointerId); } catch (err) {} });
    const up = e => { if (ui.pageHold !== p) return; ui.pageHold = null; if (!ui.pageHeldUsed && e.type === 'pointerup') { pageTap(p); pageMulti(p); } };
    b.addEventListener('pointerup', up); b.addEventListener('pointercancel', up);
    b.addEventListener('keydown', e => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); pageTap(p); } });
    host.appendChild(b);
  }
}
function renderPages() {
  const host = $('pages'); if (!host) return;
  const tr = trk(), n = pagesOf(tr), dis = !!ui.ptn || !!ui.key;
  host.classList.toggle('dis', dis);
  [...host.children].forEach((b, p) => {
    b.classList.toggle('on', !dis && p === (ui.spage | 0));
    b.classList.toggle('off', p >= n);
    b.classList.toggle('ph', !dis && p === ui.phPage);
    b.setAttribute('aria-pressed', String(!dis && p === (ui.spage | 0)));
    b.setAttribute('aria-label', `Page ${p + 1}` + (p >= n ? ', past the track length' : '') + (p === ui.phPage ? ', playing' : ''));
  });
}
function setSpd(n) {
  const tr = trk(); n = clamp(n, 0, SPEEDS.length - 1);
  if (tr.spd === n) return;
  tr.spd = n; send(['spd', ui.t, n]); touched(); renderFooter();
}
function buildLenSpd() {
  const bar = $('lenbar'), cells = $('lencells');
  cells.innerHTML = '<i></i>'.repeat(NS);
  const cellAt = (x, loose) => {
    const r = cells.getBoundingClientRect();
    if (!loose && (x < r.left - 6 || x > r.right + 6)) return 0;
    return (ui.spage | 0) * NS + clamp(Math.floor((x - r.left) / r.width * NS) + 1, 1, NS);
  };
  let d = null;
  bar.addEventListener('pointerdown', e => { d = { id: e.pointerId, x: e.clientX, y: e.clientY, moved: false }; });
  bar.addEventListener('pointermove', e => {
    if (!d || d.id !== e.pointerId) return;
    if (!d.moved) {
      const dx = Math.abs(e.clientX - d.x), dy = Math.abs(e.clientY - d.y);
      if (dx < 4) { if (dy > 8) d = null; return; }        // a vertical move: the page scrolls
      d.moved = true;
      try { bar.setPointerCapture(e.pointerId); } catch (err) {}
    }
    setLen(cellAt(e.clientX, true));
  });
  bar.addEventListener('pointerup', e => { if (d && d.id === e.pointerId && !d.moved) { const n = cellAt(e.clientX); if (n) setLen(n); } d = null; });
  bar.addEventListener('pointercancel', () => { d = null; });
  bar.addEventListener('keydown', e => {
    const k = e.key === 'ArrowRight' || e.key === 'ArrowUp' ? 1 : e.key === 'ArrowLeft' || e.key === 'ArrowDown' ? -1 : 0;
    if (k) { e.preventDefault(); setLen(trk().len + k); }
  });
  const sp = $('spd');
  let s = null;
  sp.addEventListener('pointerdown', e => { e.preventDefault(); try { sp.setPointerCapture(e.pointerId); } catch (err) {} s = { y: e.clientY, base: trk().spd }; });
  sp.addEventListener('pointermove', e => { if (s) setSpd(s.base + Math.round((s.y - e.clientY) / 16)); });
  sp.addEventListener('pointerup', () => { s = null; });
  sp.addEventListener('pointercancel', () => { s = null; });
  sp.addEventListener('dblclick', () => setSpd(SPEEDS.indexOf('1×')));
  sp.addEventListener('keydown', e => {
    const k = e.key === 'ArrowUp' || e.key === 'ArrowRight' ? 1 : e.key === 'ArrowDown' || e.key === 'ArrowLeft' ? -1 : 0;
    if (k) { e.preventDefault(); setSpd(trk().spd + k); }
  });
}
// v69: OPTION's menu (in the transport, on every page; over the kits panel when that is open)
function setProj(on) {
  ui.proj = on; $('projrow').hidden = !on; $('projbtn').setAttribute('aria-expanded', String(on));
  // (v80: Paste track, once a track is copied; which one)
  const tp = $('tpaste'); tp.disabled = !ui.tclip; tp.title = ui.tclip ? 'Paste ' + ui.tclip.name + ' onto the track shown (in this pattern)' : 'Copy a track first';
}

// v80: a track copied (its sound, its steps and p-locks, its length, speed and quantiser; a RAM track its take), pasted
// onto the track shown (in the pattern playing: from another pattern too)
function copyTrack() {
  const t = trk();
  ui.tclip = { from: ui.t, ram: mof(t.p[P.MACH]) === RAM_M, name: 'T' + (ui.t + 1) + ' · ' + patName(state.pat),
    t: { p: t.p.slice(), len: t.len, spd: t.spd, q: t.q === 0 ? 0 : 1, pre: t.pre ? Object.assign({}, t.pre) : null, steps: JSON.parse(JSON.stringify(t.steps)) } };
}
function pasteTrack() {
  const c = ui.tclip; if (!c) return;
  const ti = ui.t, t = state.tracks[ti], n = JSON.parse(JSON.stringify(c.t));
  t.p = n.p; t.len = n.len; t.spd = n.spd; t.q = n.q; t.pre = n.pre; t.steps = n.steps;
  const ops = [];
  for (let s = 0; s < NSTEP; s++) ops.push(['trig', ti, s, 0], ['clrl', ti, s], ['rat', ti, s, 1], ['rec', ti, s, 0], ['tlk', ti, s, 0]);
  trackOps(ops, t, ti);
  if (c.ram && c.from !== ti) ops.push(['rcpy', c.from, ti]);
  backend.sendAll(ops);
  dropLock(); ui.undo = null; touched();
}
// v80: a track moved: it and the one where it goes swap places, in every pattern of the project (its mute, its take, its
// place in "Rnd all"); what names a track by its number follows it (an FX machine's source T1 .. T8, WEFT's carrier,
// the followers' sources, RAM's source and BUF, the quantiser's chord track). NBR and SUM (an FX machine's neighbour,
// the tracks before it) stay as they are: places, not tracks; DUCK follows track 1, whatever is there
const trackRefs = m => [[P.RSRC, RAM_SRC.length, 2], [P.RBUF, RAM_BUF.length, 1], [P.E1SR, EF_SRC.length, 1], [P.E2SR, EF_SRC.length, 1],
  ...(m === STENCIL_M ? [[P.A, STC_SRC.length, 3]] : fxM(m) ? [[P.A, FX_SRC.length, 2]] : []), ...(m === WEFT_M ? [[P.D, 11, 3]] : [])];
function remapTrackRefs(t, map) {
  const fix = (get, set, m) => { for (const [p, n, off] of trackRefs(m)) { const v = get(p); if (v == null) continue; const i = idx(v, n) - off; if (i >= 0 && i < NT && map[i] !== i) set(p, mid(map[i] + off, n)); } };
  const m0 = mof(t.p[P.MACH]);
  fix(p => t.p[p], (p, v) => { t.p[p] = v; }, m0);
  for (const st of t.steps) { const m = P.MACH in st.l ? mof(st.l[P.MACH]) : m0; fix(p => (p in st.l ? st.l[p] : null), (p, v) => { st.l[p] = v; }, m); }
}
function moveTrack(a, b) {
  ui.moveFrom = null;
  if (a !== b) {
    const map = [...Array(NT).keys()]; map[a] = b; map[b] = a;
    for (const trs of [state.tracks, ...state.pats.filter(Boolean).map(pt => pt.tracks)]) {
      trs.forEach(t => remapTrackRefs(t, map));
      const x = trs[a]; trs[a] = trs[b]; trs[b] = x;
    }
    const q = idx(state.g[G.QSRC] || 0, NT + 1) - 1;
    if (q === a || q === b) state.g[G.QSRC] = mid(map[q] + 1, NT + 1);
    state.g[G.XMSK] = rMask(state);
    if (ui.t === a) ui.t = b; else if (ui.t === b) ui.t = a;
    dropLock(); ui.undo = null;
    // the pattern playing (its steps cleared, every track again, the mutes), every other pattern into its slot, the
    // globals, the two takes
    const ops = [['reset']];
    state.tracks.forEach((t, ti) => { ops.push(['mute', ti, t.mute]); trackOps(ops, t, ti); });
    state.pats.forEach((pt, i) => { if (pt && i !== state.pat) patOps(ops, i, pt); });
    ops.push(['g', G.QSRC, state.g[G.QSRC] || 0], ['g', G.XMSK, state.g[G.XMSK]], ['rswp', a, b]);
    backend.sendAll(ops);
    const pk = ui.ramPeaks[a]; ui.ramPeaks[a] = ui.ramPeaks[b]; ui.ramPeaks[b] = pk;
    touched();
  }
  renderAll();
}
function setMove(on) { ui.moveFrom = on ? ui.t : null; renderAll(); }
function clearTrack(t) {
  const tr = state.tracks[t];
  tr.steps.forEach((st, s) => {
    st.on = 0; st.l = {}; delete st.r; send(['trig', t, s, 0]); send(['clrl', t, s]); send(['rat', t, s, 1]);
    if (st.rec) { delete st.rec; send(['rec', t, s, 0]); }
    if (st.tl) { delete st.tl; send(['tlk', t, s, 0]); }
  });
}

function togglePlay() {
  backend.ensureAudio();
  ui.playing = !ui.playing;
  send(['play', ui.playing ? 1 : 0]);
  renderTransport();
}

function setTMore(on) { ui.tmore = on; renderKnobs(); renderTabs(); }
function wire() {
  $('play').addEventListener('click', togglePlay);
  $('tpk0').addEventListener('click', () => setTMore(false));
  $('tpk1').addEventListener('click', () => setTMore(true));
  $('rec').addEventListener('click', () => setRec(!ui.rec));
  dragReadout($('bpm'), () => state.g[G.BPM], v => setGlobal(G.BPM, v), 0.25);
  dragReadout($('swg'), () => state.g[G.SWING], v => setGlobal(G.SWING, clamp(v, 0, 1)), 1 / 200);
  dragReadout($('rndd'), () => trk().p[P.SPC], v => { trk().p[P.SPC] = clamp(v, 0, 1); touched(); renderFooter(); }, 1 / 150);
  $('mprev').addEventListener('click', () => { if (!ui.all) changeMachine(stepMachine(-1)); });
  $('mnext').addEventListener('click', () => { if (!ui.all) changeMachine(stepMachine(1)); });
  $('mid').addEventListener('click', () => { if (!ui.all) { ui.picker = !ui.picker; renderScreen(); } });
  $('mid').addEventListener('keydown', e => { if (e.key === 'Enter' && !ui.all) { ui.picker = !ui.picker; renderScreen(); } });
  buildLenSpd(); buildLive(); buildPages();
  $('projbtn').addEventListener('click', () => setProj(!ui.proj));
  $('tcopy').addEventListener('click', () => { copyTrack(); setProj(false); renderAll(); });
  armed($('tpaste'), 'Paste track', () => { pasteTrack(); setProj(false); renderAll(); });
  $('tmove').addEventListener('click', () => { setProj(false); setMove(true); });
  armed($('clr'), 'Clear track', () => { clearTrack(ui.t); ui.lock = null; touched(); setProj(false); renderAll(); });
  armed($('clrall'), 'Clear all', () => { for (let t = 0; t < NT; t++) clearTrack(t); ui.lock = null; touched(); setProj(false); renderAll(); });
  $('rndt').addEventListener('click', () => { backend.ensureAudio(); randomize([ui.t]); });
  $('rnda').addEventListener('click', () => { backend.ensureAudio(); randomize([...Array(NT).keys()].filter(t => !state.tracks[t].rx)); });
  $('rndu').addEventListener('click', undoRandom);
  // looper: tap toggles, a hold loops only while held (momentary), LEN / POS drag like the tempo
  let lpHold = null;
  const lb = $('loopbtn');
  lb.addEventListener('pointerdown', e => {
    e.preventDefault(); backend.ensureAudio();
    const wasOn = loopOn();
    if (!wasOn) setLoop(true);
    lpHold = { t: performance.now(), wasOn };
    try { lb.setPointerCapture(e.pointerId); } catch (err) {}
  });
  const lpUp = () => {
    if (!lpHold) return;
    if (lpHold.wasOn || performance.now() - lpHold.t > 350) setLoop(false);
    lpHold = null;
  };
  lb.addEventListener('pointerup', lpUp);
  lb.addEventListener('pointercancel', lpUp);
  lb.addEventListener('click', e => { if (e.detail === 0) setLoop(!loopOn()); });   // keyboard
  dragReadout($('llen'), () => state.g[G.LLEN], v => setGlobal(G.LLEN, clamp(v, 0, 1)), 1 / 168);
  dragReadout($('lpos'), () => state.g[G.LPOS], v => setGlobal(G.LPOS, clamp(v, 0, 1)), 1 / 330);
  $('lpos').addEventListener('dblclick', () => { setGlobal(G.LPOS, mid(16, 33)); renderLoop(); });
  // a new project (or the demo) starts with empty RAM buffers
  const clearRam = () => { for (let t = 0; t < NT; t++) send(['rclr', t]); };
  armed($('init'), 'Init', () => {
    state = initState(); ui.lock = null; ui.t = 0; ui.undo = null;
    backend.sendAll(stateOps(state)); clearRam(); touched(); setProj(false); renderAll();
  });
  armed($('demo'), 'Demo', () => {
    state = demoState(); ui.lock = null; ui.t = 0; ui.undo = null;
    backend.sendAll(stateOps(state)); clearRam(); touched(); setProj(false); renderAll();
  });
  document.addEventListener('keydown', e => {
    if (e.target && (e.target.classList.contains('knob') || e.target.classList.contains('step') || e.target.tagName === 'INPUT')) return;
    if (e.key === 'Escape' && ui.moveFrom != null) { setMove(false); return; }
    if (e.key === 'Escape' && ui.gpop) { ui.gpop = false; renderLockbar(); return; }
    if (e.key === 'Escape') { if (ui.proj) setProj(false); else if (ui.upanel) { ui.upanel = false; renderUpdates(); } else if (ui.kpanel) { ui.kpanel = false; ui.krename = null; renderKits(); } else if (ui.picker) { ui.picker = false; renderScreen(); } else if (ui.lock) exitLock(); else if (ui.all) setAll(false); else if (ui.key) setKey(false); else if (ui.ptn) setPtn(false); return; }
    if (inPlugin) return; // leave Space and digits to the host
    if (e.code === 'Space') { e.preventDefault(); togglePlay(); }
    else if (/^[1-8]$/.test(e.key)) { dropLock(); ui.t = +e.key - 1; if (ui.lastP === RATP || ui.lastP === RNDP) ui.lastP = P.TUNE; renderAll(); }
    else if (e.key === 'l' || e.key === 'L') { if (!e.repeat) { backend.ensureAudio(); setLoop(!loopOn()); } }
    else if (e.key === 'm' || e.key === 'M') { if (!e.repeat) toggleMute(ui.t); }
    else if (e.key === 'r' || e.key === 'R') { if (!e.repeat) setRec(!ui.rec); }
    else if (e.key === 'k' || e.key === 'K') { if (!e.repeat) setKey(!ui.key); }
    else if (e.key === 'p' || e.key === 'P') { if (!e.repeat) setPtn(!ui.ptn); }
  });
  document.addEventListener('pointerdown', e => {
    if (ui.picker && !$('machmod').contains(e.target)) { ui.picker = false; renderScreen(); }
    // (OPTION's menu closes on a tap anywhere else; the kits panel stays open while you play, until its × or OPTION)
    if (ui.proj && !$('optw').contains(e.target)) setProj(false);
  });
}

backend.init();
buildPTabs(); buildTStrip(); buildToneViz(); buildPads(); buildChips(); buildQuant(); buildPresets(); buildKits(); buildUpdates(); buildLoops(); buildRadio(); buildTabs(); buildKnobs('m', 'mknobs'); buildWestPanel(); buildKnobs('r', 'rknobs'); buildKnobs('g', 'gknobs'); buildModules(); buildSteps(); buildPicker();
wire(); renderAll();
requestAnimationFrame(frame);
// (the tests' older projects: the demo on v20's machine numbers, ALLOY as STRING, the side places at 0; on v21's, ALLOY
// as MATTER (37), its side places as MATTER had them, and a track as LATEX (38))
// (the older demos: their LFO waves on the 12 of then, as an older project holds them)
const wave12 = v => { const i = idx(v, NWAV), sh = i % NSHAPE; return mid(Math.min(sh, 5) + (i >= NSHAPE ? 6 : 0), 12); };
function waves12(s) {
  for (const t of s.tracks) {
    for (const q of LFO_WAVES) t.p[q] = wave12(t.p[q]);
    for (const st of t.steps) for (const q of LFO_WAVES) if (q in st.l) st.l[q] = wave12(st.l[q]);
  }
  return s;
}
// (v24's: the parameters as long as v24's, its waves on their grid)
function demoState24() {
  const s = waves12(demoState());
  for (const t of s.tracks) t.p.length = 193;
  s.v = SV24;
  return s;
}
function demoState20() {
  const s = waves12(demoState()), to20 = v => { const id = MACH[mof(v)].id; return mv(id === 'ALLOY' ? 14 : MACH20.indexOf(id)); };
  for (const t of s.tracks) {
    t.p[P.MACH] = to20(t.p[P.MACH]); for (let q = P.S1; q <= P.S35; q++) t.p[q] = 0; t.pre = null;
    for (const st of t.steps) if (P.MACH in st.l) st.l[P.MACH] = to20(st.l[P.MACH]);
  }
  s.v = SV20;
  return s;
}
function demoState21() {
  const s = waves12(demoState());
  for (const t of s.tracks) if (mof(t.p[P.MACH]) === ALLOY_M) { for (let q = P.S1; q <= P.S35; q++) t.p[q] = 0.3; t.pre = { m: 37, kind: 'f', name: 'ROSEWOOD MARIMBA' }; }
  s.tracks[5].p[P.MACH] = mv(38); s.tracks[5].p[P.A] = 0.9; s.tracks[4].steps[3] = { on: 1, l: { [P.MACH]: mv(37), [P.S5]: 0.9 } };
  s.v = SV21;
  return s;
}
// (v22's: ALLOY's 21 sides as v62 had them (T7: a mallet, a noise burst, bounces, a pitch from under), a step of T5
// locked onto ALLOY with two of them, the parameters as long as v22's)
function demoState22() {
  const s = waves12(demoState());
  for (const t of s.tracks) {
    if (mof(t.p[P.MACH]) === ALLOY_M) { for (let q = 0; q < 21; q++) t.p[P.S1 + q] = 0; [[0, .6], [1, .7], [2, .4], [4, .2], [6, .5], [7, 1], [20, mid(2, 8)]].forEach(([q, v]) => { t.p[P.S1 + q] = v; }); }
    t.p.length = 176;
  }
  s.tracks[4].steps[3] = { on: 1, l: { [P.MACH]: mv(ALLOY_M), [P.S7]: 0.4, [P.S20]: 0.9 } };
  s.v = SV22;
  return s;
}
window.__impakt = { get state() { return state; }, ui, setAll, WS_PAGES, copyTrack, pasteTrack, moveTrack, cubeState, cubeMem, ORDER, selectTrack, stateOps, demoState, demoState20, demoState21, demoState22, demoState24, migrate, setToneMore, setPTab, openModule, KMODS, PAGE_BY_ID, loopDraw, SIDE_META, sideDefs, ALLOY_M, setLen, setPtn, patKey, patName, serialState, setTMore, logoSvg, MACH, KANA, randomParams, scaleName, gMeta, PAGES, recSide,
  knobScale, paramMeta, BIPOLAR, gBip, GRN_MOD_PAGE, renderAll, setTrackParam, P, G, setGlobal, haloChordName, keyLayout,
  presetEntries, presetValues, get userPresets() { return userPresets; }, brkDecodeAudio, brkTempo, brkPrepare, brkV15, brkResolve,
  get brkNames() { return ui.brkNames; }, trackView, setRet, tzHandles, fltDb, fltState, toneDb, toneParams };
})();
