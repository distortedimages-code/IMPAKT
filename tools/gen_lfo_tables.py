#!/usr/bin/env python3
"""IMPAKT — the GRAIN LFO's waveform palette: 64 original waves in 8 families of 8 (classic shapes, more
shapes, steps, gates, grooves, accents, glitch, organic), 256 points a cycle. The rhythmic ones are 16-step
patterns, so at 1 BAR each step of the pattern lands on a 16th.

Writes core/sk_lfo_tables.cpp (the engine) and web/src/lfo_tables.js (the page draws them):
  python3 tools/gen_lfo_tables.py
Flags per wave: 1 = unipolar (rests at 0, pushes one way), 2 = stepped (read without interpolation).
"""
import base64, math, os, random

N = 256
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
UNI, STEP = 1, 2

waves = []          # (name, flags, list of N floats)


def add(name, flags, f):
    vals = [f(i / N, i) for i in range(N)]
    waves.append((name, flags, vals))


def frac(x):
    return x - math.floor(x)


def seq(levels, smooth=False):
    """a pattern of len(levels) equal steps; smooth: cosine between the levels"""
    n = len(levels)

    def f(t, i):
        p = t * n
        k = int(p) % n
        if not smooth:
            return levels[k]
        u = p - int(p)
        a, b = levels[k], levels[(k + 1) % n]
        w = 0.5 - 0.5 * math.cos(math.pi * u)
        return a + (b - a) * w
    return f


def gates(steps, n=16, width=0.5, levels=None):
    """pulses on the given steps of an n-step pattern, `width` of a step long"""
    def f(t, i):
        p = t * n
        k = int(p)
        if k in steps and p - k < width:
            return levels[steps.index(k)] if levels else 1.0
        return 0.0
    return f


def hits(times, decay=10.0, amps=None):
    """exponential decays starting at the given positions (fractions of the cycle), wrapping around"""
    def f(t, i):
        v = 0.0
        for j, s in enumerate(times):
            d = frac(t - s)
            a = amps[j] if amps else 1.0
            v = max(v, a * math.exp(-d * decay * 16 / 4))   # decay per step: ~ e^-(decay/4) after one step
        return v
    return f


def euclid(k, n):
    out, b = [], 0
    for i in range(n):
        b += k
        if b >= n:
            b -= n
            out.append(i)
    # rotate so the pattern starts on a hit
    s = out[0]
    return sorted((x - s) % n for x in out)


rng = random.Random(1979)

# ---------------------------------------------------------------- CLASSIC (bipolar)
add('SINE', 0, lambda t, i: math.sin(2 * math.pi * t))
add('TRI', 0, lambda t, i: 4 * t if t < 0.25 else (2 - 4 * t if t < 0.75 else 4 * t - 4))
add('SAW+', 0, lambda t, i: 2 * t - 1)
add('SAW-', 0, lambda t, i: 1 - 2 * t)
add('SQR', STEP, lambda t, i: 1.0 if t < 0.5 else -1.0)
add('PLS25', STEP, lambda t, i: 1.0 if t < 0.25 else -1.0)
E5 = math.exp(-5)
add('DECAY', 0, lambda t, i: 2 * (math.exp(-5 * t) - E5) / (1 - E5) - 1)
add('RISE', 0, lambda t, i: 2 * (1 - math.exp(-5 * t)) / (1 - E5) - 1)

# ---------------------------------------------------------------- SHAPES (bipolar)
add('HUMP', 0, lambda t, i: 2 * abs(math.sin(2 * math.pi * t)) - 1)
add('M-TRI', 0, lambda t, i: (lambda u: 4 * u if u < 0.25 else (2 - 4 * u if u < 0.75 else 4 * u - 4))(frac(2 * t)))
add('TRAPZ', 0, lambda t, i: max(-1, min(1, 3 * math.sin(2 * math.pi * t))))
add('SHARK', 0, lambda t, i: 2 * (1 - (1 - 2 * t) ** 3) - 1 if t < 0.5 else 2 * math.exp(-6 * (t - 0.5)) - 1)
add('PARAB', 0, lambda t, i: 1 - 2 * (2 * t - 1) ** 2)
add('STEP-S', STEP, lambda t, i: math.sin(2 * math.pi * (math.floor(t * 8) + 0.5) / 8))
add('SMSAW', 0, lambda t, i: 2 * (t * t * (3 - 2 * t)) - 1)
add('SKEW', 0, lambda t, i: -1 + 2 * t / 0.1 if t < 0.1 else 1 - 2 * (t - 0.1) / 0.9)

# ---------------------------------------------------------------- STEPS (bipolar, stepped)
add('UP 4', STEP, lambda t, i: math.floor(4 * t) / 3 * 2 - 1)
add('UP 8', STEP, lambda t, i: math.floor(8 * t) / 7 * 2 - 1)
add('DOWN 8', STEP, lambda t, i: 1 - math.floor(8 * t) / 7 * 2)
add('PYRMD', STEP, seq([-1, -1 / 3, 1 / 3, 1, 1 / 3, -1 / 3]))
add('OCTAV', STEP, seq([0, 0, 1, 0, -1, 0, 0.5, 0]))          # with PTCH at 50 %: octave jumps, a fifth
add('RND 8', STEP, seq([rng.uniform(-1, 1) for _ in range(8)]))
add('RND 16', STEP, seq([rng.uniform(-1, 1) for _ in range(16)]))
add('RNDSM', 0, seq([rng.uniform(-1, 1) for _ in range(8)], smooth=True))

# ---------------------------------------------------------------- GATES (unipolar, stepped, 16 steps)
add('4FLOOR', UNI | STEP, gates([0, 4, 8, 12]))
add('OFFBT', UNI | STEP, gates([2, 6, 10, 14]))
add('8THS', UNI | STEP, gates(list(range(0, 16, 2))))
add('16THS', UNI | STEP, gates(list(range(16))))
add('CLAVE', UNI | STEP, gates([0, 3, 6, 10, 12]))
add('TRESIL', UNI | STEP, gates([0, 3, 6, 8, 11, 14]))
add('EUC 5', UNI | STEP, gates(euclid(5, 16)))
add('EUC 7', UNI | STEP, gates(euclid(7, 16)))

# ---------------------------------------------------------------- GROOVES (unipolar, decaying hits)
s = 1 / 16
add('KICKS', UNI, hits([0, 6 * s, 10 * s, 11 * s], amps=[1, 0.8, 0.9, 0.6]))
add('DOTTED', UNI, hits([0, 3 * s, 6 * s, 9 * s, 12 * s]))
add('SWING', UNI, hits([(k + (0.66 if k % 2 else 0)) * s for k in range(16)], decay=14, amps=[1 if k % 4 == 0 else 0.55 for k in range(16)]))
bounce, gap, amp, b_amps = [], 0.3, 1.0, []
p = 0.0
while p < 0.97 and len(bounce) < 14:
    bounce.append(p); b_amps.append(amp)
    p += gap; gap *= 0.72; amp *= 0.85
add('BOUNCE', UNI, hits(bounce, decay=16, amps=b_amps))
decel = [1 - x for x in reversed(bounce)]
add('DECEL', UNI, hits([d % 1.0 for d in decel], decay=16, amps=list(reversed(b_amps))))
ratch = [0.0] + [0.25 + k / 8 for k in range(2)] + [0.5 + k / 16 for k in range(4)] + [0.75 + k / 32 for k in range(8)]
add('RATCH', UNI, hits(ratch, decay=22, amps=[1] + [0.9] * 2 + [0.8] * 4 + [0.7] * 8))
add('STUTT', UNI, lambda t, i: (1 - frac(t * 32)) if t < 0.375 else (1 - frac(t * 8) if 0.5 <= t < 0.625 else 0.0))
add('BROKEN', UNI, hits([0, 5 * s, 7 * s, 12 * s, 13 * s], decay=12, amps=[1, 0.7, 0.9, 0.6, 0.8]))

# ---------------------------------------------------------------- ACCENTS (unipolar, stepped)
add('ACCENT', UNI | STEP, seq([1, .2, .5, .2, .8, .2, .5, .3, 1, .2, .6, .2, .8, .4, .5, .3]))
add('3 OVR 4', UNI | STEP, seq([1 if k % 3 == 0 else 0.3 for k in range(16)]))
add('POLY34', UNI | STEP, seq([(0.5 if k % 3 == 0 else 0) + (0.5 if k % 4 == 0 else 0) for k in range(16)]))
add('POLY54', UNI | STEP, seq([(0.5 if k % 5 == 0 else 0) + (0.5 if k % 4 == 0 else 0) for k in range(16)]))
spiral = []
for g in [3, 3, 2, 3, 3, 2]:
    spiral += [(j + 1) / g for j in range(g)]
add('SPIRAL', UNI | STEP, seq(spiral))
add('ZIGZAG', UNI | STEP, seq([(k + 1) / 16 if k % 2 == 0 else 0.08 for k in range(16)]))
morse = '.. -- .--. .- -.- -'           # I M P A K T
units = []
for ch in morse:
    if ch == '.':
        units += [1, 0]
    elif ch == '-':
        units += [1, 1, 1, 0]
    else:
        units += [0, 0]
add('MORSE', UNI | STEP, seq([float(u) for u in units]))
add('BINARY', UNI | STEP, seq([((0b1011001110001011 >> (15 - k)) & 1) * (0.5 + 0.5 * ((k * 7) % 3) / 2) for k in range(16)]))

# ---------------------------------------------------------------- GLITCH (bipolar)
spikes = {rng.randrange(N): rng.choice([-1, 1]) * rng.uniform(0.5, 1) for _ in range(11)}
add('MICRO', STEP, lambda t, i: next((v for k, v in spikes.items() if 0 <= i - k < 3), 0.0))
cuts = sorted(set([0] + [rng.randrange(1, N) for _ in range(9)]))
add('CRUSH', STEP, lambda t, i: math.sin(2 * math.pi * max(c for c in cuts if c <= i) / N * 3))
scat = [(rng.random(), rng.choice([-1, 1]) * rng.uniform(0.3, 1)) for _ in range(9)]
add('SCATTR', 0, lambda t, i: max(-1, min(1, sum(a * math.exp(-frac(t - p0) * 40) for p0, a in scat))))
add('FM', 0, lambda t, i: math.sin(2 * math.pi * t + 2.5 * math.sin(2 * math.pi * 5 * t)))
add('CHIRP', 0, lambda t, i: math.sin(2 * math.pi * (2 * t + 7 * t * t)))
add('CHOP', 0, lambda t, i: math.sin(2 * math.pi * t) * (1 if frac(t * 16) < 0.5 else 0))
bits = [rng.choice([-1.0, 1.0]) for _ in range(32)]
add('BITS', STEP, seq(bits))
noise = [rng.uniform(-1, 1) for _ in range(N)]
add('SPRAY', 0, lambda t, i: noise[i] * math.exp(-4 * t))

# ---------------------------------------------------------------- ORGANIC (bipolar unless noted)
breath_n = [rng.uniform(-1, 1) for _ in range(N)]
def breath(t, i):
    if t < 0.6:                                      # a slow breath in
        u = t / 0.6
        v = -1 + 2 * u * u * (3 - 2 * u)
    else:                                            # out, faster
        v = -1 + 2 * math.exp(-9 * (t - 0.6))
    return max(-1, min(1, v + 0.04 * breath_n[i]))
add('BREATH', 0, breath)
add('HEART', UNI, hits([0, 0.12], decay=24, amps=[1, 0.7]))
tide = [0.6 * math.sin(2 * math.pi * k / N) + 0.3 * math.sin(2 * math.pi * 3 * k / N + 1) + 0.2 * math.sin(2 * math.pi * 5 * k / N + 2) for k in range(N)]
tm = max(abs(v) for v in tide)
add('TIDE', 0, lambda t, i: tide[i] / tm)
# Lorenz x over a stretch of the attractor, faded into a loop
x, y, z, dt, lor = 1.0, 1.0, 1.0, 0.004, []
for k in range(4000 + N * 3):
    dx, dy, dz = 10 * (y - x), x * (28 - z) - y, x * y - 8 / 3 * z
    x, y, z = x + dx * dt, y + dy * dt, z + dz * dt
    if k >= 4000 and (k - 4000) % 3 == 0:
        lor.append(x)
lm = max(abs(v) for v in lor)
lor = [v / lm for v in lor]
add('LORENZ', 0, lambda t, i: lor[i] * (1 - t) + lor[0] * t if i > N - 24 else lor[i])
add('SPRING', 0, lambda t, i: math.exp(-3 * t) * math.cos(2 * math.pi * 6 * t))
add('PENDUL', 0, lambda t, i: math.sin(2 * math.pi * (t + 0.15 * math.sin(2 * math.pi * t))))
add('TENSE', 0, lambda t, i: 2 * (math.exp(4 * t) - 1) / (math.exp(4) - 1) - 1)
walk, v = [], 0.0
for k in range(N):
    v += rng.uniform(-1, 1)
    walk.append(v)
walk = [w - (walk[-1] - walk[0]) * k / (N - 1) for k, w in enumerate(walk)]      # loops back
sm = [sum(walk[(k + j) % N] for j in range(-6, 7)) / 13 for k in range(N)]
mn, mx = min(sm), max(sm)
add('WALK', 0, lambda t, i: (sm[i] - mn) / (mx - mn) * 2 - 1)

FAMILIES = ['CLASSIC', 'SHAPES', 'STEPS', 'GATES', 'GROOVES', 'ACCENTS', 'GLITCH', 'ORGANIC']
assert len(waves) == 64, len(waves)

# quantise: bipolar -1..1 -> -127..127, unipolar 0..1 -> 0..127
data = bytearray()
rows = []
for name, flags, vals in waves:
    q = []
    for v in vals:
        v = max(-1.0, min(1.0, v))
        if flags & UNI:
            v = max(0.0, v)
        q.append(int(round(v * 127)))
    rows.append(q)
    data += bytes((x + 256) % 256 for x in q)

cpp = ['// generated by tools/gen_lfo_tables.py: the GRAIN LFO palette (64 waves x 256 points, -127..127)',
       '#include "sk_lfo_tables.h"', '', 'namespace sk {', '',
       'const signed char WLFO_TABLE[WLFO_WAVES][WLFO_LEN] = {']
for (name, flags, _), q in zip(waves, rows):
    cpp.append('    { // ' + name)
    for k in range(0, N, 32):
        cpp.append('        ' + ', '.join(str(x) for x in q[k:k + 32]) + ',')
    cpp.append('    },')
cpp += ['};', '', 'const unsigned char WLFO_FLAGS[WLFO_WAVES] = { ' + ', '.join(str(w[1]) for w in waves) + ' };', '', '} // namespace sk', '']
with open(os.path.join(ROOT, 'core/sk_lfo_tables.cpp'), 'w') as fh:
    fh.write('\n'.join(cpp))

js = ('// generated by tools/gen_lfo_tables.py: the GRAIN LFO palette (names, families of 8, flags: 1 unipolar,\n'
      '// 2 stepped; data: 64 x 256 signed bytes, base64)\n'
      'const IMPAKT_LFO = { names: ' + repr([w[0] for w in waves]).replace("'", '"') + ',\n'
      '  families: ' + repr(FAMILIES).replace("'", '"') + ',\n'
      '  flags: [' + ', '.join(str(w[1]) for w in waves) + '],\n'
      '  data: "' + base64.b64encode(bytes(data)).decode() + '" };\n'
      "if (typeof module !== 'undefined') module.exports = IMPAKT_LFO;\n")
with open(os.path.join(ROOT, 'web/src/lfo_tables.js'), 'w') as fh:
    fh.write(js)
print(len(waves), 'waves')
