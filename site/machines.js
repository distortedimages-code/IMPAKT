// IMPAKT's Machines page: each machine's screen plays back its own sound, as the engine renders it
// (site/machines-data.json: the envelope over the time it sounds, and 24 frames of its waveform). A red playhead runs
// over the envelope; the waveform under it is the one the machine makes at that moment. Only the screens in view move;
// with reduced motion each one shows a still frame.
(function () {
  const data = JSON.parse(document.getElementById('scopes').textContent);
  const dec = b => Uint8Array.from(atob(b), c => c.charCodeAt(0));
  const css = getComputedStyle(document.documentElement);
  const ink = () => css.getPropertyValue('--lcd-ink').trim() || '#E3EAE5';
  const LED = '#E9472B', FR = 24, PTS = 128;
  const still = matchMedia('(prefers-reduced-motion: reduce)').matches;
  const scopes = [...document.querySelectorAll('canvas.scope')].map(cv => {
    const d = data[cv.dataset.m];
    if (!d) return null;
    const env = dec(d.e), raw = dec(d.s), wave = new Float32Array(raw.length);
    for (let i = 0; i < raw.length; i++) wave[i] = (raw[i] > 127 ? raw[i] - 256 : raw[i]) / 127;
    // a hit is drawn over at least a second (a hat lasts 30 ms), a drone over its two seconds; then a short rest
    const sweep = Math.min(2.6, Math.max(1.1, d.d * 2)), rest = 0.7;
    return { cv, env, wave, sweep, cycle: sweep + rest, phase: Math.random() * (sweep + rest), on: false, w: 0, h: 0 };
  }).filter(Boolean);

  function size(s) {
    const r = s.cv.getBoundingClientRect(), dpr = Math.min(2, window.devicePixelRatio || 1);
    const w = Math.max(1, Math.round(r.width * dpr)), h = Math.max(1, Math.round(r.height * dpr));
    if (s.cv.width !== w || s.cv.height !== h) { s.cv.width = w; s.cv.height = h; }
    s.w = w; s.h = h; s.dpr = dpr;
  }

  function draw(s, t) {
    const g = s.cv.getContext('2d'), { w, h, dpr } = s, mid = h / 2, col = ink();
    g.clearRect(0, 0, w, h);
    const u = t / s.sweep;                      // 0..1 over the sound, beyond: the rest
    const pos = Math.min(1, u);
    // the envelope: mirrored bars, lit up to the playhead
    const n = s.env.length, bw = w / n;
    for (let c = 0; c < n; c++) {
      const a = s.env[c] / 255 * (mid - 3 * dpr);
      if (a < 0.5) continue;
      g.globalAlpha = c / n <= pos ? 0.34 : 0.13;
      g.fillStyle = col;
      g.fillRect(c * bw, mid - a, Math.max(1, bw - 0.6 * dpr), 2 * a);
    }
    g.globalAlpha = 1;
    // the waveform at the playhead (between two frames), fading out in the rest
    const f = Math.min(FR - 1.001, pos * (FR - 1)), f0 = Math.floor(f), k = f - f0;
    const fade = u <= 1 ? 1 : Math.max(0, 1 - (t - s.sweep) / (0.6 * (s.cycle - s.sweep)));
    g.strokeStyle = col; g.lineWidth = 1.4 * dpr; g.lineJoin = 'round'; g.globalAlpha = fade;
    g.beginPath();
    for (let q = 0; q < PTS; q++) {
      const v = s.wave[f0 * PTS + q] * (1 - k) + s.wave[(f0 + 1) * PTS + q] * k;
      const x = q / (PTS - 1) * w, y = mid - v * (mid - 4 * dpr);
      q ? g.lineTo(x, y) : g.moveTo(x, y);
    }
    g.stroke();
    g.globalAlpha = 1;
    // the playhead
    if (u <= 1) { g.fillStyle = LED; g.fillRect(Math.round(pos * (w - 2 * dpr)), 0, 2 * dpr, h); }
  }

  scopes.forEach(s => { size(s); draw(s, s.sweep * 0.12); });
  if (still) return;
  const io = new IntersectionObserver(es => es.forEach(e => {
    const s = scopes.find(x => x.cv === e.target);
    if (s) s.on = e.isIntersecting;
  }), { rootMargin: '80px' });
  scopes.forEach(s => io.observe(s.cv));
  addEventListener('resize', () => scopes.forEach(size));
  let last = performance.now();
  function frame(now) {
    const dt = Math.min(0.1, (now - last) / 1000); last = now;
    for (const s of scopes) {
      if (!s.on) continue;
      s.phase = (s.phase + dt) % s.cycle;
      draw(s, s.phase);
    }
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
  // a tap on a screen starts its sound over
  scopes.forEach(s => s.cv.addEventListener('pointerdown', () => { s.phase = 0; }));
})();
