// IMPAKT — AudioWorklet processor. Runs the shared C++ engine (WebAssembly, or the
// wasm2js fallback registered as globalThis.impaktAsm when WebAssembly is not allowed).
class ImpaktProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const o = (options && options.processorOptions) || {};
    this.x = null;
    try {
      if (o.wasm) {
        const mod = new WebAssembly.Module(o.wasm);
        this.x = new WebAssembly.Instance(mod, {}).exports;
      } else if (typeof globalThis.impaktAsm === 'function') {
        this.x = globalThis.impaktAsm();
      }
    } catch (e) {
      this.port.postMessage({ type: 'error', msg: String(e && e.message || e) });
    }
    if (!this.x) {
      this.port.postMessage({ type: 'error', msg: 'engine unavailable' });
      return;
    }
    // the plain-JS build is too slow for the IR and the spectral reverb: lite mode skips them (and the IR bank)
    if (o.wasm) this.x.sk_init(sampleRate);
    else this.x.sk_init_lite(sampleRate);
    const buf = this.x.memory.buffer;
    this.vl = new Float32Array(buf, this.x.sk_out_l(), 4096);
    this.vr = new Float32Array(buf, this.x.sk_out_r(), 4096);
    // RAM machine: the input the engine reads, what the page knows of each buffer (version saved, peaks shown),
    // and when a buffer last changed (a take is saved once it has rested a moment)
    this.il = new Float32Array(buf, this.x.sk_in_l(), 4096);
    this.ir = new Float32Array(buf, this.x.sk_in_r(), 4096);
    this.inSilent = true;
    this.ramSaved = new Array(8).fill(0);
    this.ramPk = new Array(8).fill(-1);
    this.ramMoved = new Array(8).fill(0);
    this.count = 0; this.ticks = 0; this.prsmT = -1; this.haloT = -1; this.brkT = -1; this.fxmT = -1; this.brkVer = -1; this.cubeT = -1; this.westT = -1; this.radT = -1;
    // CHIMERA's bank: the samples still to decode (index, file, offset, bytes), its files, one at work; the transport
    this.chmT = -1; this.bankJobs = null; this.bankFiles = null; this.bankBusy = false; this.playing = 0;
    this.port.onmessage = (e) => this.handle(e.data);
    this.port.postMessage({ type: 'ready', engine: o.wasm ? 'wasm' : 'js' });
  }

  handle(msg) {
    const x = this.x;
    if (!x || !msg) return;
    // RAM: a take kept by the page (it is already saved: no echo)
    if (msg.ram) { this.ramLoad(msg.ram); return; }
    // BREAK: a loop the page decoded into a slot (mono, 32 kHz, cut to its bars); the engine reads it over the next blocks
    if (msg.brk) { this.brkLoad(msg.brk); return; }
    // CUBE (v74): a bank the page read from a WAV (64 waves of 256 samples, 16-bit) for one of its 8 places (Z)
    if (msg.cube) { this.cubeLoad(msg.cube); return; }
    // CHIMERA: the bank (its index, its files, what is where); the engine decodes it over the next blocks
    if (msg.bank) { this.bankLoad(msg.bank); return; }
    // RADIO (v87): a receiver's state, its sound flushed, what the page fetched for it (an MP3 stream's bytes, a
    // shortwave receiver's IMA ADPCM or 16-bit sound), its S-meter
    if (msg.radio) { this.radioIn(msg.radio); return; }
    if (!msg.ops) return;
    for (const o of msg.ops) {
      switch (o[0]) {
        case 'p':     x.sk_set_param(o[1], o[2], o[3]); break;
        case 'trig':  x.sk_set_trig(o[1], o[2], o[3]); break;
        case 'lock':  x.sk_set_lock(o[1], o[2], o[3], o[4]); break;
        case 'clrl':  x.sk_clear_locks(o[1], o[2]); break;
        case 'rat':   x.sk_set_ratchet(o[1], o[2], o[3]); break;
        case 'len':   x.sk_set_len(o[1], o[2]); break;
        case 'spd':   x.sk_set_speed(o[1], o[2]); break;
        case 'mute':  x.sk_set_mute(o[1], o[2]); break;
        case 'g':     x.sk_set_global(o[1], o[2]); break;
        case 'play':  x.sk_set_play(o[1]); this.playing = o[1]; break;
        case 'aud':   x.sk_audition(o[1], o[2], o[3] || 0); break;
        case 'reset': x.sk_reset(); break;
        case 'panic': x.sk_panic(); break;
        case 'scale': x.sk_set_scale(o[1]); break;
        case 'qtrk':  x.sk_set_qtrk(o[1], o[2]); break;
        case 'spec':  if (x.sk_set_spec) { x.sk_set_spec(o[1]); this.spec = o[1] >= 0; } break;
        case 'rec':   x.sk_set_rec(o[1], o[2], o[3]); break;
        case 'tlk':   x.sk_set_tlk(o[1], o[2], o[3]); break;
        case 'rnow':  x.sk_ram_start(o[1]); break;
        case 'rclr':  x.sk_ram_clear(o[1]); this.ramSaved[o[1]] = this.ramInfo()[o[1] * 6 + 2]; break;
        case 'rcpy':  if (x.sk_ram_copy) x.sk_ram_copy(o[1], o[2]); break;     // (v80: Paste track, Move track: the takes follow)
        case 'rswp':  if (x.sk_ram_swap) x.sk_ram_swap(o[1], o[2]); break;
        case 'prsm':  this.prsmT = o[1]; break;
        case 'halo':  this.haloT = o[1]; break;
        case 'brk':   this.brkT = o[1]; break;
        case 'fxm':   this.fxmT = o[1]; break;
        case 'chm':   this.chmT = o[1]; break;
        case 'bclr':  if (x.sk_break_clear) x.sk_break_clear(o[1]); break;
        case 'cube':  this.cubeT = o[1]; break;
        case 'west':  this.westT = o[1]; break;
        case 'radt':  this.radT = o[1]; break;
        case 'cclr':  if (x.sk_cube_clear) x.sk_cube_clear(o[1]); break;
        case 'bbar':  if (x.sk_break_bars) x.sk_break_bars(o[1], o[2]); break;
        // a pad or a key let go (DEC on HOLD: its note ends); the patterns: the one playing, all emptied, the one edited,
        // one emptied, a change (mode 0 at the end, 1 at once from its start, 2 at once where it is), the chain, CHG
        case 'rel':   if (x.sk_release) x.sk_release(o[1], o[2] || 0); break;
        case 'rib':   if (x.sk_ribbon) x.sk_ribbon(o[1], o[2] || 0, o[3] | 0, o[4] == null ? 1 : o[4]); break;   // (gate: 0 let go, 1 touched, 2 touched on SNAP)
        case 'pad':   if (x.sk_touch) x.sk_touch(o[1], o[2] | 0, o[3] || 0); break;   // (v84: MANTRA's pads: a finger's pressure, 0 lifted)
        case 'pcur':  if (x.sk_pat_cur) x.sk_pat_cur(o[1]); break;
        case 'pall':  if (x.sk_pat_all) x.sk_pat_all(); break;
        case 'epat':  if (x.sk_pat_edit) x.sk_pat_edit(o[1]); break;
        case 'pclr':  if (x.sk_pat_clear) x.sk_pat_clear(o[1]); break;
        case 'pq':    if (x.sk_pat_queue) x.sk_pat_queue(o[1], o[2] || 0); break;
        case 'pch':   if (x.sk_pat_chain) x.sk_pat_chain(o[1], o[2]); break;
        case 'pchn':  if (x.sk_pat_chainlen) x.sk_pat_chainlen(o[1]); break;
        case 'pchg':  if (x.sk_pat_chg) x.sk_pat_chg(o[1]); break;
      }
    }
  }

  radioIn(r) {
    const x = this.x, k = r.rx | 0;
    if (!x.sk_radio_stage || k < 0 || k > 3) return;
    if (r.state != null) x.sk_radio_state(k, r.state | 0, r.kind | 0);
    if (r.flush) x.sk_radio_flush(k);
    if (r.meter != null) x.sk_radio_meter(k, r.meter);
    if (!r.data) return;
    const d = r.data instanceof Uint8Array ? r.data : new Uint8Array(r.data), cap = x.sk_radio_cap();
    for (let off = 0; off < d.length; off += cap) {
      let part = d.subarray(off, Math.min(d.length, off + cap));
      if (r.fmt === 'pcm') part = part.subarray(0, part.length & ~1);
      new Uint8Array(x.memory.buffer, x.sk_radio_stage(), part.length).set(part);
      if (r.fmt === 'mp3') x.sk_radio_mp3(k, part.length);
      else if (r.fmt === 'adpcm') x.sk_radio_adpcm(k, part.length, r.rate || 12000);
      else if (r.fmt === 'pcm') x.sk_radio_pcm(k, part.length >> 1, 1, r.rate || 12000);
    }
  }
  ramInfo() { return new Float32Array(this.x.memory.buffer, this.x.sk_ram_info(), 48); }
  bankLoad(b) {
    const x = this.x;
    if (!x.sk_bank_index || !b.idx || !b.files) return;
    const idx = new Uint8Array(b.idx);
    if (idx.length > x.sk_bank_cap()) return;
    new Uint8Array(x.memory.buffer, x.sk_bank_stage(), idx.length).set(idx);
    if (x.sk_bank_index(idx.length) < 0) return;
    this.bankFiles = b.files.map(f => new Uint8Array(f));
    this.bankJobs = b.jobs.slice(); this.bankBusy = false;
  }
  // a sample a time: its bytes into the stage, then a few thousand values decoded each block (more while the transport
  // is stopped), so the audio never waits (the engine decodes ~60 M values a second: 6144 a block is ~0.1 ms of the
  // 2.7 ms a block lasts, the whole bank in ~16 s while it plays; 32768 when stopped, ~3 s)
  bankWork() {
    const x = this.x;
    if (!this.bankBusy) {
      const j = this.bankJobs.shift();
      if (!j) { this.bankJobs = null; this.bankFiles = null; return; }
      const [i, file, off, len] = j, src = this.bankFiles[file];
      if (!src || off + len > src.length || len > x.sk_bank_cap()) { x.sk_bank_feed(-1, 0); return; }
      new Uint8Array(x.memory.buffer, x.sk_bank_stage(), len).set(src.subarray(off, off + len));
      x.sk_bank_feed(i, len); this.bankBusy = true;
    }
    if (x.sk_bank_step(this.playing ? 6144 : 32768)) this.bankBusy = false;
  }
  brkLoad(b) {
    const x = this.x, s = b.slot | 0;
    if (!x.sk_break_mem || s < 0 || s > 9 || !b.data) return;
    const n = Math.min(b.data.length, x.sk_break_cap());
    new Int16Array(x.memory.buffer, x.sk_break_mem(s), n).set(b.data.subarray(0, n));
    x.sk_break_set(s, n, b.bars | 0);
  }
  cubeLoad(c) {
    const x = this.x, b = c.bank | 0;
    if (!x.sk_cube_mem || b < 0 || b > 7 || !c.data) return;
    const n = Math.min(c.data.length, x.sk_cube_cap());
    new Int16Array(x.memory.buffer, x.sk_cube_mem(), n).set(c.data.subarray(0, n));
    x.sk_cube_set(b, n);
  }
  ramLoad(r) {
    const x = this.x, t = r.t | 0;
    if (t < 0 || t > 7 || !r.data) return;
    const n = Math.min(r.data.length, x.sk_ram_cap());
    new Int16Array(x.memory.buffer, x.sk_ram_mem(t), n).set(r.data.subarray(0, n));
    x.sk_ram_loaded(t, n, r.rate);
    this.ramSaved[t] = this.ramInfo()[t * 6 + 2];
  }

  process(inputs, outputs) {
    const x = this.x;
    if (!x) return true;
    const out = outputs[0];
    const n = out[0].length;
    // the input (RAM: IN source, monitor): the first input's channels, or silence
    const inp = inputs && inputs[0];
    if (inp && inp.length && inp[0].length === n) {
      this.il.set(inp[0]); this.ir.set(inp.length > 1 ? inp[1] : inp[0]); this.inSilent = false;
    } else if (!this.inSilent) { this.il.fill(0, 0, n); this.ir.fill(0, 0, n); this.inSilent = true; }
    x.sk_process(n);
    if (this.bankJobs) this.bankWork();
    out[0].set(this.vl.subarray(0, n));
    if (out[1]) out[1].set(this.vr.subarray(0, n));
    if (++this.count >= 8) {
      this.count = 0;
      const steps = [], hits = [];
      for (let t = 0; t < 8; t++) { steps.push(x.sk_get_step(t)); hits.push(x.sk_get_hits(t)); }
      const gr = x.sk_meter_gr(), out = x.sk_meter_out();
      x.sk_meter_reset();
      // MIX page display: calm slots (Hz, dB) and the unmasking gains (8 tracks x 10 bands, dB)
      const mix = x.sk_mix_info ? Array.from(new Float32Array(x.memory.buffer, x.sk_mix_info(), 96)) : null;
      // looper: on, window start, length (16ths), effective position, real position
      const loop = x.sk_loop_info ? Array.from(new Float32Array(x.memory.buffer, x.sk_loop_info(), 5)) : null;
      // spectrum of the track being edited (24 bands, linear peaks), when the page asks for it
      const spec = this.spec && x.sk_spec_info ? Array.from(new Float32Array(x.memory.buffer, x.sk_spec_info(), 24)) : null;
      // DICE: gates fired since the last message, the voltages, on (only sent while it runs)
      // GRAIN's LFO (phase, value) and OTT's gain per band (only while one of them is on)
      let fx = null;
      if (x.sk_fx_info) { const f = new Float32Array(x.memory.buffer, x.sk_fx_info(), 7); if (f[5] > 0.5 || f[6] > 0.5) fx = Array.from(f); }
      // RAM: each buffer (length, rate, version, state, progress, peaks counter), the peaks that changed, and the
      // takes to keep: a buffer whose version moved, once nothing has changed for half a second
      let ram = null, peaks = null;
      if (x.sk_ram_info) {
        const info = this.ramInfo(); ram = Array.from(info);
        this.ticks++;
        for (let t = 0; t < 8; t++) {
          const b = t * 6;
          if (info[b + 5] !== this.ramPk[t]) {
            this.ramPk[t] = info[b + 5]; this.ramMoved[t] = this.ticks;
            (peaks || (peaks = {}))[t] = Array.from(new Float32Array(x.memory.buffer, x.sk_ram_peaks(t), 128));
          }
          if (info[b + 2] !== this.ramSaved[t] && info[b + 3] === 0 && this.ticks - this.ramMoved[t] > 24) {
            this.ramSaved[t] = info[b + 2];
            const len = info[b] | 0, data = new Int16Array(len);
            data.set(new Int16Array(x.memory.buffer, x.sk_ram_mem(t), len));
            this.port.postMessage({ type: 'ramdata', t, rate: info[b + 1], data }, [data.buffer]);
          }
        }
      }
      // PRISM of the track the page shows on its PRSM side: its oscillators (Hz, amplitude)
      const prism = this.prsmT >= 0 && x.sk_prism_info ? Array.from(new Float32Array(x.memory.buffer, x.sk_prism_info(this.prsmT), 32)) : null;
      // LFO 1 / 2 (phase, value) and EF 1 / 2 (value, input) of every track, for the TRACK screen
      const mod = x.sk_mod_info ? Array.from(new Float32Array(x.memory.buffer, x.sk_mod_info(), 128)) : null;
      // GLITCH (only while it is in): its last divisions, the current one, the codec's bandwidth
      let glitch = null;
      if (x.sk_glitch_info) { const f = new Float32Array(x.memory.buffer, x.sk_glitch_info(), 22); if (f[19] > 0.5) glitch = Array.from(f); }
      const halo = this.haloT >= 0 && x.sk_halo_info ? Array.from(new Float32Array(x.memory.buffer, x.sk_halo_info(this.haloT), 66)) : null;
      // BREAK on the page: the break under its knob, the slice playing
      const brk = this.brkT >= 0 && x.sk_break_info ? Array.from(new Float32Array(x.memory.buffer, x.sk_break_info(this.brkT), 272)) : null;
      // an FX track on the page: its machine, source, gate, gesture, levels, readouts
      const fxm = this.fxmT >= 0 && x.sk_fxm_info ? Array.from(new Float32Array(x.memory.buffer, x.sk_fxm_info(this.fxmT), 13)) : null;
      // CHIMERA on the page: the bank (state, decoded, samples, refused), the track's last hit and its neighbours
      const chim = this.chmT >= 0 && x.sk_chim_info ? Array.from(new Float32Array(x.memory.buffer, x.sk_chim_info(this.chmT), 24)) : null;
      // CUBE on the page: playing, where A and B are, the set, the banks ready and being made, the wave A reads
      const cube = this.cubeT >= 0 && x.sk_cube_info ? Array.from(new Float32Array(x.memory.buffer, x.sk_cube_info(this.cubeT), 138)) : null;
      // WEST on the page: the light, the envelope, the step, the pulses, the folder, the gate, the spring
      const west = this.westT >= 0 && x.sk_west_info ? Array.from(new Float32Array(x.memory.buffer, x.sk_west_info(this.westT), 24)) : null;     // (v78: WS_INFO 24)
      // BREAK's slots, when they changed: state, tempo, bars, seconds, and their waveforms
      let brks = null;
      if (x.sk_break_ver && x.sk_break_ver() !== this.brkVer) {
        this.brkVer = x.sk_break_ver();
        const info = Array.from(new Float32Array(x.memory.buffer, x.sk_break_slots(), 41)).slice(1), pk = [];
        for (let s = 0; s < 10; s++) pk.push(Array.from(new Float32Array(x.memory.buffer, x.sk_break_peaks(s), 128)));
        brks = { ver: this.brkVer, info, peaks: pk };
      }
      // the patterns: playing, next, chain length and position, serial (a change), CHG, position in it, 16ths to the change
      // RADIO: the track shown (its receiver, the loop, the history's peaks); the four receivers, the ones the tracks play
      const radio = this.radT >= 0 && x.sk_radio_info ? Array.from(new Float32Array(x.memory.buffer, x.sk_radio_info(this.radT), 144)) : null;
      const radios = x.sk_radio_rx ? { use: x.sk_radio_use(), info: Array.from(new Float32Array(x.memory.buffer, x.sk_radio_rx(), 32)) } : null;
      const pat = x.sk_pat_info ? Array.from(new Float32Array(x.memory.buffer, x.sk_pat_info(), 8)) : null;
      // the quantiser's notes now (its scale, or the chord of the HALO / STRUM track it follows)
      const qm = x.sk_qmask ? x.sk_qmask() : -1;
      this.port.postMessage({ type: 'pos', steps, hits, run: x.sk_running(), gr, out, mix, loop, spec, fx, ram, peaks, prism, mod, glitch, halo, brk, brks, fxm, chim, cube, west, radio, radios, pat, qm });
    }
    return true;
  }
}
registerProcessor('impakt', ImpaktProcessor);
