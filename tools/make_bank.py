#!/usr/bin/env python3
"""IMPAKT — builds CHIMERA's sample bank (bank/) from a kit: a zip or a folder of WAV files, one folder a category.

    python3 tools/make_bank.py <kit.zip | kit folder> [bank]

Each sample: its silence trimmed (from where it starts, -60 dB under its peak, less a millisecond; to where it falls
under -70 dB), 10 s at most (a 0.5 s fade then), mono when its sides are silent (-40 dB), 16-bit. Then what CHIMERA
needs to know of it (core/sk_bank.h):
  - its gain: its loudest 100 ms (K-weighted, ITU-R BS.1770) brought to -14 dB, at most +24 dB;
  - its pitch (YIN over its body, 40 .. 240 or 120 .. 420 ms after its attack, the surer; 0 when it has none to speak of);
  - eight features, as z-scores over the bank (FAMILLE's neighbours): length (to -30 dB under its peak), attack (to
    its peak), brightness (spectral centroid), noisiness (flatness), lows (under 150 Hz), highs (over 5 kHz),
    pitchedness (YIN's confidence), width (sides against middle);
  - its place on the screen's map: brightness across, length up (ranks: the bank spread evenly);
  - its outline for the screen: 64 peaks on a time axis from 1 ms to its end, logarithmic (0..255: 60 dB).
The samples are coded by build/bank_tool (the engine's codec, lossless) into chimera.N.bin files of 14 MB at most,
their index into chimera.idx; bank_tool check then decodes the whole bank again.
"""
import os, re, struct, subprocess, sys, tempfile, zipfile, unicodedata
import numpy as np
import soundfile as sf
from scipy.signal import lfilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOL = os.path.join(ROOT, 'build', 'bank_tool')
# the categories (their folders), in the order of CAT
CATS = [('kick', 'KICK'), ('snare', 'SNARE'), ('clap', 'CLAP'), ('hat', 'HAT'), ('perc', 'PERC'), ('808', '808'),
        ('cymbal', 'CYMB'), ('vox', 'VOX'), ('fx', 'FX'), ('loops', 'LOOP')]
MAX_S, FADE_S, CHUNK = 10.0, 0.5, 14 * 1000 * 1000


def kweight(x, sr):
    """BS.1770's two filters (a high shelf, then a high-pass), for any rate."""
    def biquad(G, Q, fc, kind):
        A = 10 ** (G / 40); w0 = 2 * np.pi * fc / sr; al = np.sin(w0) / (2 * Q); c = np.cos(w0)
        if kind == 'shelf':
            b = [A * ((A + 1) + (A - 1) * c + 2 * np.sqrt(A) * al), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - 2 * np.sqrt(A) * al)]
            a = [(A + 1) - (A - 1) * c + 2 * np.sqrt(A) * al, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - 2 * np.sqrt(A) * al]
        else:
            b = [(1 + c) / 2, -(1 + c), (1 + c) / 2]; a = [1 + al, -2 * c, 1 - al]
        return np.array(b) / a[0], np.array(a) / a[0]
    b1, a1 = biquad(3.99984385, 0.7071752, 1681.974450955533, 'shelf')
    b2, a2 = biquad(0.0, 0.5003270373, 38.13547087602444, 'hp')
    return lfilter(b2, a2, lfilter(b1, a1, x, axis=0), axis=0)


def yin(x, sr, fmin=30.0, fmax=1000.0):
    """The fundamental of x (Hz) and how sure of it (0..1), YIN's cumulative mean normalised difference."""
    tmax, tmin = int(sr / fmin), max(2, int(sr / fmax))
    W = len(x) - tmax
    if W < 256: return 0.0, 0.0
    x = x - x.mean()
    d = np.array([np.sum((x[:W] - x[t:t + W]) ** 2) for t in range(tmax + 1)])
    cm = np.ones_like(d); s = np.cumsum(d[1:]); cm[1:] = d[1:] * np.arange(1, tmax + 1) / np.maximum(s, 1e-20)
    cand = np.where(cm[tmin:] < 0.25)[0]
    if len(cand):
        t = tmin + cand[0]
        while t + 1 <= tmax and cm[t + 1] < cm[t]: t += 1
    else:
        t = tmin + int(np.argmin(cm[tmin:]))
    conf = float(max(0.0, 1.0 - cm[t]))
    if 1 <= t < tmax:                                   # (between two lags)
        a, b, c = cm[t - 1], cm[t], cm[t + 1]; den = a - 2 * b + c
        t = t + (0.5 * (a - c) / den if abs(den) > 1e-12 else 0.0)
    return float(sr / t), conf


def analyse(y, sr):
    """y: float, (n, ch). The features (raw), gain, pitch, onset."""
    m = y.mean(axis=1)
    hop = int(0.005 * sr)
    env = np.sqrt(np.convolve(m ** 2, np.ones(hop) / hop, mode='same') + 1e-20)
    pk = int(np.argmax(env)); epk = env[pk]
    on = int(np.argmax(env > epk * 10 ** (-30 / 20)))
    attack = max(0.0, (pk - on) / sr * 1000.0)
    above = np.where(env > epk * 10 ** (-30 / 20))[0]
    length = max(1.0, (above[-1] - pk) / sr * 1000.0) if len(above) else 1.0
    seg = m[:min(len(m), int(1.0 * sr))]
    n = 1 << int(np.ceil(np.log2(max(len(seg), 2048))))
    P = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n)) ** 2
    f = np.fft.rfftfreq(n, 1 / sr)
    tot = P[f > 20].sum() + 1e-20
    cent = (P * f)[f > 20].sum() / tot
    band = (f > 100) & (f < 16000)
    flat = np.exp(np.mean(np.log(P[band] + 1e-20))) / (np.mean(P[band]) + 1e-20)
    low = P[(f > 20) & (f < 150)].sum() / tot
    high = P[f > 5000].sum() / tot
    # (its body: two windows after the attack, the surer one; a kick's sweep has settled in the second)
    hz, conf = 0.0, 0.0
    for t0, t1 in ((0.04, 0.24), (0.12, 0.42)):
        body = m[on + int(t0 * sr):on + int(t1 * sr)]
        if len(body) > int(0.12 * sr):
            h, c = yin(body, sr, fmin=25.0)
            if c > conf: hz, conf = h, c
    pitch = hz if conf > 0.7 else 0.0
    if y.shape[1] == 2:
        S = (y[:, 0] - y[:, 1]) / 2
        width = 10 * np.log10((S ** 2).sum() / ((m ** 2).sum() + 1e-20) + 1e-6)
    else:
        width = -60.0
    k = kweight(m, sr); w = int(0.1 * sr)
    loud = np.sqrt(np.convolve(k ** 2, np.ones(w) / w, mode='valid').max() + 1e-20) if len(k) > w else np.sqrt(np.mean(k ** 2) + 1e-20)
    gain = min(10 ** (24 / 20), 10 ** (-14 / 20) / loud)
    feats = [np.log(length), np.log1p(attack), np.log2(max(cent, 20)), np.log(flat + 1e-6), np.log(low + 1e-4), np.log(high + 1e-4),
             conf, max(-40.0, min(0.0, width))]
    return feats, float(gain), float(pitch), on / sr


def outline(y, sr):
    """64 peaks, bin k from 1 ms (k = 0: from 0) to the end on a log axis, in 60 dB above 0 (0..255)."""
    a = np.abs(y).max(axis=1); L = len(a) / sr; pk = a.max() + 1e-12
    edges = [0.0] + [0.001 * (max(L, 0.002) / 0.001) ** (k / 63) for k in range(64)]
    out = bytearray()
    for k in range(64):
        i0, i1 = int(edges[k] * sr), max(int(edges[k] * sr) + 1, int(edges[k + 1] * sr))
        v = a[i0:i1].max() if i0 < len(a) else 0.0
        out.append(int(round(255 * min(1.0, max(0.0, (20 * np.log10(v / pk + 1e-9) + 60) / 60)))))
    return bytes(out)


def nkey(s):
    return [int(t) if t.isdigit() else t for t in re.split(r'(\d+)', s.lower())]


def clean(name):
    s = unicodedata.normalize('NFKD', name).encode('ascii', 'ignore').decode()
    s = re.sub(r'\.wav$', '', s, flags=re.I).replace('wav', '').replace('_', ' ')
    return re.sub(r'\s+', ' ', s).strip().rstrip('.').strip().upper()[:39]


def main():
    src = sys.argv[1]; out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'bank')
    if not os.path.exists(TOOL): sys.exit('build/bank_tool first (see tools/bank_tool.cpp)')
    tmp = tempfile.mkdtemp(prefix='impakt-bank-')
    if zipfile.is_zipfile(src):
        with zipfile.ZipFile(src) as z: z.extractall(os.path.join(tmp, 'kit'))
        src = os.path.join(tmp, 'kit')
    found = []
    for dp, _, fs in os.walk(src):
        for fn in fs:
            if not fn.lower().endswith('.wav') or fn.startswith('._'): continue
            folder = os.path.basename(dp).lower()
            ci = next((i for i, (d, _) in enumerate(CATS) if d == folder), None)
            if ci is None: print('skipped (no category):', os.path.join(dp, fn)); continue
            found.append((ci, nkey(fn), os.path.join(dp, fn), fn))
    found.sort()
    rows = []
    for ci, _, path, fn in found:
        x, sr = sf.read(path, always_2d=True, dtype='float64')
        if x.shape[1] > 2: x = x[:, :2]
        a = np.abs(x).max(axis=1); pk = a.max()
        if pk < 1e-6: print('skipped (silent):', fn); continue
        st = max(0, int(np.argmax(a > pk * 10 ** (-60 / 20))) - int(0.001 * sr))
        en = int(np.where(a > pk * 10 ** (-70 / 20))[0][-1]) + 1
        y = x[st:en].copy()
        cap = int(MAX_S * sr)
        if len(y) > cap:
            y = y[:cap]; fl = int(FADE_S * sr); y[-fl:] *= np.linspace(1, 0, fl)[:, None]
        if y.shape[1] == 2:
            M, S = (y[:, 0] + y[:, 1]) / 2, (y[:, 0] - y[:, 1]) / 2
            if (S ** 2).sum() < (M ** 2).sum() * 1e-4: y = M[:, None]
        feats, gain, pitch, onset = analyse(y, sr)
        q = np.clip(np.round(y * 32767), -32768, 32767).astype('<i2')
        rows.append(dict(cat=ci, name=clean(fn), sr=sr, ch=q.shape[1], frames=len(q), pcm=q, feats=feats, gain=gain, pitch=pitch, onset=onset, env=outline(y, sr)))
    # the features as z-scores, the map's ranks
    F = np.array([r['feats'] for r in rows], dtype=np.float64)
    Z = (F - F.mean(axis=0)) / (F.std(axis=0) + 1e-9)
    rank = lambda v: (np.argsort(np.argsort(v)) + 0.5) / len(v)
    MX, MY = rank(F[:, 2]), rank(F[:, 0])
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(out):
        if f.startswith('chimera.'): os.remove(os.path.join(out, f))
    files, cur, coded = [], bytearray(), []
    for i, r in enumerate(rows):
        raw = os.path.join(tmp, 's.raw'); skc = os.path.join(tmp, 's.skc')
        r['pcm'].tofile(raw)
        n, crc = subprocess.check_output([TOOL, 'enc', raw, str(r['ch']), skc]).split()
        data = open(skc, 'rb').read(); assert len(data) == int(n)
        if len(cur) + len(data) > CHUNK and len(cur): files.append(cur); cur = bytearray()
        r['file'], r['foff'], r['coded'], r['crc'] = len(files), len(cur), len(data), int(crc)
        cur += data
    files.append(cur)
    for k, b in enumerate(files): open(os.path.join(out, 'chimera.%d.bin' % k), 'wb').write(b)
    hdr = bytearray(b'SKB1') + struct.pack('<III', 1, len(rows), len(CATS))
    for _, label in CATS: hdr += bytes([len(label)]) + label.encode()
    for i, r in enumerate(rows):
        nm = r['name'].encode('ascii')
        hdr += struct.pack('<BBH', r['cat'], r['ch'], len(nm))
        hdr += struct.pack('<IIIIII', r['frames'], r['sr'], r['crc'], r['coded'], r['file'], r['foff'])
        hdr += struct.pack('<fff', r['gain'], r['pitch'], r['onset'])
        hdr += struct.pack('<8f', *Z[i]) + struct.pack('<ff', MX[i], MY[i]) + nm + r['env']
    open(os.path.join(out, 'chimera.idx'), 'wb').write(hdr)
    # the names, for the presets (web/src/chimera_names.js: a preset names its samples)
    names = [[r['name'] for r in rows if r['cat'] == c] for c in range(len(CATS))]
    js = ('// generated by tools/make_bank.py: CHIMERA\'s categories and the names of their samples, in the bank\'s order\n'
          'const IMPAKT_CHIM_NAMES = ' + __import__('json').dumps({'cats': [l for _, l in CATS], 'names': names}) + ';\n'
          "if (typeof module !== 'undefined') module.exports = IMPAKT_CHIM_NAMES;\n")
    open(os.path.join(ROOT, 'web', 'src', 'chimera_names.js'), 'w').write(js)
    raw = sum(r['frames'] * r['ch'] * 2 for r in rows); cod = sum(len(b) for b in files)
    counts = [sum(1 for r in rows if r['cat'] == c) for c in range(len(CATS))]
    print('%d samples (%s), %.1f s, %.1f MB raw, %.1f MB coded (%.0f %%) in %d files, index %d bytes' % (
        len(rows), ' '.join('%s %d' % (CATS[c][1], counts[c]) for c in range(len(CATS))), sum(r['frames'] / r['sr'] for r in rows),
        raw / 1e6, cod / 1e6, 100 * cod / raw, len(files), len(hdr)))
    print('pitched: %d' % sum(1 for r in rows if r['pitch'] > 0))
    subprocess.check_call([TOOL, 'check', out])


if __name__ == '__main__':
    main()
