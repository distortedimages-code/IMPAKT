#!/usr/bin/env python3
"""The manual's diagrams (docs/fig/*.svg), drawn as the instrument draws its screens: a black LCD, 1 px lines, pixel
labels, red for a trig and for "now", amber for a p-lock, green for RAM's REC trigs.

  python3 docs/make_figs.py

The site (site/build_site.py) puts them inline in the page, where its pixel font applies; GitHub shows them as images
(the labels then fall back to a monospace font)."""
import math, os, random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'docs', 'fig')
W = 480
BG, INK, DIM, LO, RED, AMB, GRN = '#0E1110', '#E3EAE5', '#7A8480', '#262D2A', '#E9472B', '#FFB000', '#46C97A'
FONT = "Silkscreen, 'Martian Mono', ui-monospace, monospace"


class Fig:
    def __init__(self, name, h, title):
        self.name, self.h, self.title, self.parts = name, h, title, []

    def add(self, s):
        self.parts.append(s)

    def rect(self, x, y, w, h, fill='none', stroke=None, sw=1, rx=0, op=None):
        a = ' stroke="%s" stroke-width="%g"' % (stroke, sw) if stroke else ''
        o = ' opacity="%g"' % op if op is not None else ''
        self.add('<rect x="%g" y="%g" width="%g" height="%g" rx="%g" fill="%s"%s%s/>' % (x, y, w, h, rx, fill, a, o))

    def line(self, x1, y1, x2, y2, col=INK, sw=1, dash=None, op=None):
        d = ' stroke-dasharray="%s"' % dash if dash else ''
        o = ' opacity="%g"' % op if op is not None else ''
        self.add('<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s" stroke-width="%g"%s%s/>' % (x1, y1, x2, y2, col, sw, d, o))

    def path(self, d, col=INK, sw=1, fill='none', op=None, dash=None, join='miter'):
        o = ' opacity="%g"' % op if op is not None else ''
        ds = ' stroke-dasharray="%s"' % dash if dash else ''
        self.add('<path d="%s" fill="%s" stroke="%s" stroke-width="%g" stroke-linejoin="%s"%s%s/>' % (d, fill, col, sw, join, o, ds))

    def text(self, x, y, s, col=INK, size=10, anchor='start', op=None):
        o = ' opacity="%g"' % op if op is not None else ''
        s = s.replace('&', '&amp;').replace('<', '&lt;')
        self.add('<text x="%g" y="%g" fill="%s" font-size="%g" text-anchor="%s"%s>%s</text>' % (x, y, col, size, anchor, o, s))

    def svg(self):
        return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" role="img" aria-labelledby="t-%s">'
                '<title id="t-%s">%s</title>'
                '<rect width="%d" height="%d" rx="6" fill="%s"/>'
                '<g font-family="%s" shape-rendering="crispEdges">%s</g></svg>\n') % (
            W, self.h, W, self.h, self.name, self.name, self.title, W, self.h, BG, FONT.replace("'", '&apos;'), ''.join(self.parts))

    def save(self):
        open(os.path.join(OUT, self.name + '.svg'), 'w', encoding='utf-8').write(self.svg())


def steps(f, y, marks, x0=16, pitch=28, w=24, h=14, n=16, nums=False):
    """A row of step keys: marks[i] = 'trig' (red), 'lock' (red, a black corner: it holds locks), 'change' (amber),
    'rec' (green)."""
    for i in range(n):
        x = x0 + i * pitch
        m = marks.get(i)
        if m in ('trig', 'lock'):
            f.rect(x, y, w, h, RED)
        elif m == 'change':
            f.rect(x, y, w, h, AMB)
        elif m == 'rec':
            f.rect(x, y, w, h, GRN)
        else:
            f.rect(x + .5, y + .5, w - 1, h - 1, 'none', DIM)
        if m == 'lock':
            f.add('<path d="M%g %gh8v8z" fill="%s"/>' % (x + w - 8, y, BG))
        if nums:
            f.text(x + w / 2, y + h + 11, str(i + 1), DIM, 9, 'middle')


# ------------------------------------------------------------------ p-lock
def plock():
    f = Fig('plock', 200, "P-lock : le pas 9 joue avec son propre réglage, le pas 15 change le son sans nouveau coup")
    X = lambda i: 16 + i * 28
    f.text(16, 18, 'PISTE 1', DIM)
    steps(f, 26, {0: 'trig', 4: 'trig', 8: 'lock', 12: 'trig', 14: 'change'}, nums=True)
    # the value (FREQ): the track's, the lock on 9 (until the next trig), the change on 15 (until the next trig)
    base, hi, lo = 104, 78, 120
    f.text(16, 70, 'FREQ', DIM)
    f.path('M16 %gH%g' % (base, X(8)), INK, 1.5)
    f.path('M%g %gV%gH%gV%g' % (X(8), base, hi, X(12), base), AMB, 2)
    f.path('M%g %gH%g' % (X(12), base, X(14)), INK, 1.5)
    f.path('M%g %gV%gH464' % (X(14), base, lo), AMB, 2)
    f.text(X(8) + 4, hi - 6, 'TRIG + LOCK', AMB)
    f.text(X(14) - 4, lo + 2, 'CHANGEMENT DE SON', AMB, anchor='end')
    # the sound: each hit's envelope; 9 brighter, the tail of 13 darker once 15 has closed the filter
    top, bot = 140, 188
    f.text(16, top - 4, 'LE SON', DIM)
    f.line(16, bot + .5, 464, bot + .5, LO)
    for i, op in ((0, .3), (4, .3), (8, .75), (12, .3)):
        x = X(i)
        pts = ['M%g %g' % (x, bot)]
        for k in range(0, 41):
            t = k / 40
            pts.append('L%g %g' % (x + t * 104, bot - (bot - top + 6) * math.exp(-3.2 * t)))
        pts.append('L%g %g Z' % (x + 104, bot))
        f.path(''.join(pts), 'none', 0, INK, op)
    f.rect(X(14), top, 464 - X(14), bot - top, BG, op=.55)
    f.save()


# ------------------------------------------------------------------ ratchets, retrigs and their curve
def rolls():
    f = Fig('rolls', 176, "Un pas en six coups : réguliers (EVEN), qui accélèrent (FAST) ou qui ralentissent (SLOW), leur vélocité qui descend (RVEL)")
    x0, x1 = 84, 460
    f.text(16, 18, 'UN PAS · 6 COUPS · RVEL −', DIM)
    f.line(x0 + .5, 28, x0 + .5, 166, DIM)
    f.line(x1 + .5, 28, x1 + .5, 166, DIM, dash='2 3')
    f.text(x1 - 4, 18, 'PAS SUIVANT', DIM, anchor='end')
    for row, (name, r) in enumerate((('EVEN', 1.0), ('FAST', .72), ('SLOW', 1 / .72))):
        y = 70 + row * 48
        f.text(16, y - 4, name, INK if row else INK)
        f.line(x0, y + .5, x1, y + .5, LO)
        for k in range(6):
            p = k / 6 if r == 1 else (1 - r ** k) / (1 - r ** 6)
            x = round(x0 + p * (x1 - x0))
            hgt = 34 * (1 - .14 * k)
            f.rect(x, y - hgt, 4, hgt, RED if k == 0 else INK)
    f.save()


# ------------------------------------------------------------------ JUMP
def jump():
    f = Fig('jump', 188, "JUMP +3, RNG 12 : chaque trig monte de trois demi-tons ; arrivé à la plage, on repart de la note")
    X = lambda i: 40 + i * 26.5
    Y = lambda s: 150 - s * 9
    f.text(16, 18, 'JUMP +3 · RNG 12', DIM)
    f.line(40, Y(12) + .5, 464, Y(12) + .5, DIM, dash='2 3')
    f.text(464, Y(12) - 5, 'RNG', DIM, anchor='end')
    f.line(40, Y(0) + .5, 464, Y(0) + .5, LO)
    f.text(32, Y(0) + 4, '0', DIM, anchor='end')
    f.text(32, Y(12) + 4, '+12', DIM, anchor='end')
    for k, s in enumerate((0, 3, 6, 9, 0, 3, 6, 9)):
        i = 2 * k
        f.rect(X(i), Y(s) - 3, 44, 6, INK)
        f.rect(X(i), Y(s) - 3, 4, 6, RED)
        f.text(X(i) + 22, Y(s) - 9, '+%d' % s if s else 'NOTE', INK if s else DIM, 9, 'middle')
    for i in range(16):
        f.rect(X(i), 166, 22, 10, RED if i % 2 == 0 else 'none', None if i % 2 == 0 else DIM)
    f.save()


# ------------------------------------------------------------------ LEN and SPD: polymetry
def poly():
    f = Fig('poly', 176, "Quatre pistes de longueurs et de vitesses différentes : elles se décalent, puis se retrouvent au pas 49")
    x0, p = 84, 7.75
    f.text(16, 18, '48 PAS', DIM)
    f.text(x0 + 48 * p, 18, 'PAS 49 : ENSEMBLE', RED, anchor='end')
    rows = (('LEN 16', 16, 1, (0, 4, 8, 12)), ('LEN 12', 12, 1, (0, 3, 7)), ('LEN 6', 6, 1, (0, 4)), ('16 · 2×', 16, 2, (0, 3, 6, 10, 12)))
    for r, (lab, ln, spd, trigs) in enumerate(rows):
        y = 34 + r * 34
        f.text(16, y + 11, lab, INK)
        cells = int(48 * spd)
        cp = p / spd
        for c in range(cells):
            x = x0 + c * cp
            local = c % ln
            if local == 0:
                f.line(x - 1.5, y - 4, x - 1.5, y + 18, RED if c == 0 else DIM)
            if local in trigs:
                f.rect(x, y, cp - 1.5, 14, INK)
            else:
                f.rect(x, y + 12, cp - 1.5, 2, LO)
    f.line(x0 + 48 * p - 1, 26, x0 + 48 * p - 1, 168, RED, 1)
    f.save()


# ------------------------------------------------------------------ pattern changes
def patterns():
    f = Fig('patterns', 176, "Changer de pattern : SEQ attend la fin du pattern, START part tout de suite du pas 1, JUMP tout de suite là où en est la lecture")
    x0, x1 = 76, 460
    sp = (x1 - x0) / 32
    X = lambda s: x0 + s * sp
    tap = 6
    f.text(X(tap), 18, 'TAP A02', RED, anchor='middle')
    for b in (0, 16, 32):
        f.line(X(b) + .5, 26, X(b) + .5, 170, LO)
    rows = (('SEQ', 16, 1), ('START', tap, 1), ('JUMP', tap, tap + 1))
    for r, (lab, at, first) in enumerate(rows):
        y = 36 + r * 46
        f.text(16, y + 17, lab, INK)
        f.rect(X(0), y, X(at) - X(0) - 2, 26, LO)
        f.text(X(0) + 5, y + 17, 'A01', DIM)
        f.rect(X(at), y, X(32) - X(at), 26, 'none', INK, 1)
        f.text(X(at) + 5, y + 17, 'A02 · %d' % first, INK)
        # where A02 starts over (its step 1)
        nxt = at + (16 - (first - 1))
        while nxt < 32:
            f.line(X(nxt) + .5, y + 3, X(nxt) + .5, y + 23, INK)
            f.text(X(nxt) + 4, y + 17, '1', INK)
            nxt += 16
    f.line(X(tap) + .5, 24, X(tap) + .5, 170, RED)
    f.save()


# ------------------------------------------------------------------ the sequence looper
def looper():
    f = Fig('looper', 164, "Le looper : allumé au pas 5 sur une fenêtre d'un temps, il rejoue 5 à 8 ; éteint, la lecture retombe au pas 13, où la séquence en serait")
    X = lambda i: 16 + i * 28
    f.text(16, 18, 'LA SÉQUENCE', DIM)
    f.text(X(4), 18, 'LOOP · LEN 1/4', INK)
    for i in range(16):
        inwin = 4 <= i <= 7
        f.rect(X(i) + .5, 26.5, 23, 15, 'none', INK if inwin else DIM, 2 if inwin else 1)
        f.text(X(i) + 12, 38, str(i + 1), INK if inwin else DIM, 9, 'middle')
    f.text(16, 70, 'CE QUI JOUE', DIM)
    played = [1, 2, 3, 4, 5, 6, 7, 8, 5, 6, 7, 8, 13, 14, 15, 16]
    for i, s in enumerate(played):
        on = 4 <= i <= 11
        f.rect(X(i), 78, 24, 16, INK if on else LO)
        f.text(X(i) + 12, 90, str(s), BG if on else INK, 9, 'middle')
    f.text(16, 118, "L'HORLOGE", DIM)
    for i in range(16):
        f.rect(X(i) + .5, 126.5, 23, 15, 'none', LO)
        f.text(X(i) + 12, 138, str(i + 1), DIM, 9, 'middle')
    f.line(X(4) - 2.5, 72, X(4) - 2.5, 100, RED, 2)
    f.line(X(12) - 2.5, 72, X(12) - 2.5, 100, RED, 2)
    f.text(X(4) - 2, 112, 'ON', RED, 9, 'middle')
    f.text(X(12) - 2, 112, 'OFF', RED, 9, 'middle')
    f.save()


# ------------------------------------------------------------------ RAM
def ram():
    f = Fig('ram', 196, "RAM : un trig REC enregistre une mesure de la sortie ; les trigs PLAY en rejouent des tranches")
    X = lambda i: 16 + i * 28
    f.text(16, 18, 'REC', GRN)
    steps(f, 24, {0: 'rec'})
    f.text(56, 18, 'UN TRIG REC : UNE MESURE DE MAIN', GRN, 9)
    # the take: four slices
    y0, y1 = 58, 106
    f.rect(16.5, y0 + .5, 447, y1 - y0 - 1, 'none', GRN, 1)
    rnd = random.Random(7)
    pts = []
    for k in range(0, 447, 2):
        beat = (k % 112) / 112
        a = (math.exp(-5 * beat) * .9 + .12) * (y1 - y0 - 8) / 2 * (0.6 + 0.4 * rnd.random())
        pts.append('%s%g %g' % ('M' if not pts else 'L', 16 + k, (y0 + y1) / 2 + (a if (k // 2) % 2 else -a)))
    f.path(''.join(pts), GRN, 1, op=.85, join='round')
    for s in range(1, 4):
        f.line(16 + s * 112 + .5, y0, 16 + s * 112 + .5, y1, GRN, 1, '2 2')
    for s in range(4):
        f.text(16 + s * 112 + 5, y0 + 11, 'SL %d' % (s + 1), GRN, 9)
    f.text(16, 138, 'PLAY', RED)
    plays = {2: (2, 'SL 3'), 6: (0, 'SL 1'), 10: (3, 'SL 4 · REV'), 13: (1, 'SL 2')}
    steps(f, 146, {i: 'trig' for i in plays})
    for i, (sl, lab) in plays.items():
        sx = 16 + sl * 112 + 56
        f.line(sx, y1 + 1, X(i) + 12, 145, INK, 1, '1 2')
        f.text(X(i) + 12, 176, lab, INK, 9, 'middle')
    f.save()


# ------------------------------------------------------------------ FX tracks
def fx():
    f = Fig('fx', 160, "Machines FX : la piste 5 (VERB, SRC SUM) prend les pistes 1 à 4, la piste 6 (TAPE, SRC NBR) prend la piste 5 ; tout arrive au mix")
    X = lambda i: 16 + i * 57
    names = ['BD·VA', 'SD·VA', 'HH·MT', 'SY·RAW', 'VERB', 'TAPE', 'HALO', 'CP·NZ']
    yb = 30
    for i, n in enumerate(names):
        fxk = i in (4, 5)
        f.rect(X(i), yb, 50, 34, INK if fxk else 'none', None if fxk else DIM, rx=3)
        f.text(X(i) + 25, yb + 14, 'T%d' % (i + 1), BG if fxk else INK, 10, 'middle')
        f.text(X(i) + 25, yb + 27, n, BG if fxk else DIM, 8, 'middle')
    # T1..T4 into T5
    yc = 84
    for i in range(4):
        f.line(X(i) + 25, yb + 34, X(i) + 25, yc, INK)
    f.line(X(0) + 25, yc + .5, X(4) + 25, yc + .5, INK)
    f.path('M%g %gV%g' % (X(4) + 25, yc, yb + 34), INK, 1)
    f.add('<path d="M%g %gl-4 7h8z" fill="%s"/>' % (X(4) + 25, yb + 35, INK))
    # T5 into T6
    f.path('M%g %gH%g' % (X(4) + 50, yb + 17, X(5) - 1), INK, 1)
    f.add('<path d="M%g %gl-7 -4v8z" fill="%s"/>' % (X(5), yb + 17, INK))
    f.text(X(5) + 25, yb - 6, 'SRC NBR', AMB, 9, 'middle')
    f.text(X(4) + 25, yb - 6, 'SRC SUM', AMB, 9, 'middle')
    # to the mix: T6, T7, T8
    ym = 118
    for i in (5, 6, 7):
        f.line(X(i) + 25, yb + 34, X(i) + 25, ym, INK)
        f.add('<path d="M%g %gl-4 -7h8z" fill="%s"/>' % (X(i) + 25, ym, INK))
    f.rect(16, ym, 448, 26, LO, rx=3)
    f.text(240, ym + 17, 'MIX', INK, 10, 'middle')
    f.save()


# ------------------------------------------------------------------ the path of the sound
def path():
    f = Fig('path', 232, "Le chemin du son : la machine, le filtre, l'ampli, la BOX, TONE et le pan de la piste, ses envois, le mix, puis le master")

    def box(x, y, w, lab, sub=None, inv=False):
        f.rect(x + .5, y + .5, w - 1, 31 if sub else 25, INK if inv else 'none', None if inv else INK, rx=3)
        f.text(x + w / 2, y + 15, lab, BG if inv else INK, 10, 'middle')
        if sub:
            f.text(x + w / 2, y + 27, sub, BG if inv else DIM, 7, 'middle')

    def head(x, y, d):
        tip = {'r': 'M%g %gl-6 -3.5v7z', 'd': 'M%g %gl-3.5 -6h7z'}[d]
        f.add(('<path d="' + tip + '" fill="%s"/>') % (x, y, DIM))

    # the track (page TONE, the machine on SEQ)
    f.text(16, 18, 'LA PISTE', DIM)
    f.path('M100 14.5H464', LO, 1)
    f.rect(250, 9, 64, 10, BG)
    f.text(282, 18, 'PAGE TONE', DIM, 8, 'middle')
    xs = [(16, 64, 'MACHINE', 'A–F'), (100, 70, 'FILTER', 'MS-20 DRIVE'), (190, 60, 'AMP', 'ATK PNCH VOL'), (270, 48, 'BOX', 'IR'),
          (338, 56, 'TONE', 'HP LP EQ'), (414, 50, 'PAN', 'DUCK')]
    for i, (x, w, lab, sub) in enumerate(xs):
        box(x, 26, w, lab, sub, inv=(i == 0))
        if i:
            px, pw = xs[i - 1][0], xs[i - 1][1]
            f.line(px + pw, 42.5, x - 5, 42.5, DIM)
            head(x, 42.5, 'r')
    # into the mix, and out to the sends (page FX), whose returns come back into the mix
    f.line(439.5, 58, 439.5, 101, DIM)
    head(439.5, 104, 'd')
    f.path('M439.5 82.5H41.5V101', DIM, 1)
    for k in range(4):
        f.line(41.5 + k * 58, 82.5, 41.5 + k * 58, 101, DIM)
        head(41.5 + k * 58, 104, 'd')
        box(16 + k * 58, 104, 50, ('REV', 'DLY', 'CHO', 'PHS')[k])
    f.path('M41.5 130V138.5H254.5V117.5H264', DIM, 1)
    for k in range(1, 4):
        f.line(41.5 + k * 58, 130, 41.5 + k * 58, 138.5, DIM)
    head(270, 117.5, 'r')
    f.text(16, 152, 'ENVOIS · PAGE FX', DIM, 8)
    box(270, 104, 194, 'MIX', 'UNMK · KICK')
    # the master
    f.path('M367.5 136V162.5H43.5V171', DIM, 1)
    head(43.5, 174, 'd')
    ms = ('EQ', 'COMP', 'DRV', 'GRAIN', 'OTT', 'GLITCH', 'OUT')
    w, g = 54, 11
    for k, n in enumerate(ms):
        x = 16 + k * (w + g)
        box(x, 174, w, n, inv=(k == len(ms) - 1))
        if k:
            f.line(x - g, 187.5, x - 5, 187.5, DIM)
            head(x, 187.5, 'r')
    f.text(16, 220, 'LE MASTER · PAGE MASTR', DIM, 8)
    f.save()


# ------------------------------------------------------------------ LFO: SLEW and LOOP
def lfo():
    f = Fig('lfo', 196, "Deux formes de LFO aléatoires : SLEW, qui rejoint chaque nouvelle valeur en marche (SLW 0) ou en douceur (SLW 50 %), et LOOP, qui rejoue ses LEN valeurs")
    x0, x1, n = 124, 462, 8
    cw = (x1 - x0) / n
    rnd = random.Random(11)
    vals = [rnd.uniform(-1, 1) for _ in range(n + 1)]
    loopv = [.8, -.5, .2, -.9]
    f.text(16, 18, '8 CYCLES', DIM)
    for c in range(n + 1):
        f.line(x0 + c * cw + .5, 24, x0 + c * cw + .5, 176, LO)
    rows = (('SLEW', 'SLW 0', 0.0), ('SLEW', 'SLW 50 %', .5), ('LOOP', 'LEN 4', None))
    for r, (lab, sub, slw) in enumerate(rows):
        yc, amp = 52 + r * 50, 18
        f.text(16, yc - 2, lab, INK)
        f.text(16, yc + 11, sub, DIM, 9)
        f.line(x0, yc + .5, x1, yc + .5, LO, dash='1 2')
        pts = []
        if slw is None:
            for c in range(n):
                v = loopv[c % 4]
                pts.append(('M%g %g H%g' % (x0, yc - v * amp, x0 + cw)) if not pts else ('V%g H%g' % (yc - v * amp, x0 + (c + 1) * cw)))
        else:
            prev = vals[0]
            pts.append('M%g %g' % (x0, yc - prev * amp))
            for c in range(n):
                v = vals[c + 1]
                xs = x0 + c * cw
                if slw == 0:
                    pts.append('V%g H%g' % (yc - v * amp, xs + cw))
                else:
                    for k in range(1, 13):
                        t = k / 12
                        s = t * t * (3 - 2 * t)
                        pts.append('L%g %g' % (xs + t * slw * cw, yc - (prev + (v - prev) * s) * amp))
                    pts.append('H%g' % (xs + cw))
                prev = v
        f.path(' '.join(pts), INK, 1.5, join='round')
        if slw is None:
            f.path('M%g %gv-4H%gv4' % (x0 + 1, yc + amp + 10, x0 + 4 * cw - 2), AMB, 1)
            f.path('M%g %gv-4H%gv4' % (x0 + 4 * cw + 1, yc + amp + 10, x1 - 2), AMB, 1)
            f.text(x0 + 2 * cw, yc + amp + 21, 'LEN 4', AMB, 8, 'middle')
            f.text(x0 + 6 * cw, yc + amp + 21, 'LES MÊMES', AMB, 8, 'middle')
    f.save()


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for fn in (plock, rolls, jump, poly, patterns, looper, ram, fx, path, lfo):
        fn()
    print('figures:', ', '.join(sorted(os.listdir(OUT))))
