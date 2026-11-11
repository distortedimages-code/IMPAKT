#!/usr/bin/env python3
"""Draws the DMG window's background (packaging/mac/dmg-background.png and @2x), in IMPAKT's panel colours, with its logo
(brand/impakt-logo-light.png).
The icon places it is drawn around are those of packaging/mac/dmg_settings.py (keep them together).
Needs Pillow and fontTools (the UI's fonts in web/fonts are woff2: converted on the fly)."""
import io, os, sys
from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = os.path.join(HERE, '..', '..', 'web', 'fonts')
W, H = 720, 540
PANEL, INK, INK2, INK3, LINE, HOT, RED = '#D9DCDA', '#141617', '#464D4B', '#737B78', '#ABB1AF', '#FFC21A', '#E85305'
# the icons' centres (points), as in dmg_settings.py
PKG = (150, 250)
ROWS = [(410, 177, 600), (410, 267, 600), (410, 357, 600), (410, 447, 600)]   # (file x, y, folder x)


def font(name, size):
    t = TTFont(os.path.join(FONTS, name)); t.flavor = None
    b = io.BytesIO(); t.save(b); b.seek(0)
    return ImageFont.truetype(b, size)


def draw(scale):
    im = Image.new('RGB', (W * scale, H * scale), PANEL)
    d = ImageDraw.Draw(im)
    s = lambda v: int(round(v * scale))
    disp = font('big-shoulders-display-latin-900-normal.woff2', s(46))
    px = font('silkscreen-latin-400-normal.woff2', s(8))
    lab = font('sofia-sans-extra-condensed-latin-700-normal.woff2', s(17))
    small = font('sofia-sans-extra-condensed-latin-500-normal.woff2', s(14))
    # the logo (brand/impakt-logo-light.png, rendered from its SVG), the formats on the right, the line under them
    logo = Image.open(os.path.join(HERE, '..', '..', 'brand', 'impakt-logo-light.png')).convert('RGBA')
    lw = s(250); logo = logo.resize((lw, round(logo.height * lw / logo.width)), Image.LANCZOS)
    im.paste(logo, (s(30), s(20)), logo)
    t = 'VST3  ·  AU  ·  APP'
    d.text((s(W - 34) - d.textlength(t, font=px), s(74)), t, font=px, fill=INK2)
    d.line((s(24), s(100), s(W - 24), s(100)), fill=LINE, width=max(1, s(1)))
    # left: the installer, in its box
    d.rounded_rectangle((s(PKG[0] - 92), s(PKG[1] - 112), s(PKG[0] + 92), s(PKG[1] + 108)), radius=s(10), outline=LINE, width=max(1, s(1)))
    t = 'OR THE INSTALLER'
    w = d.textlength(t, font=px)
    d.text((s(PKG[0]) - w / 2, s(PKG[1] - 96)), t, font=px, fill=INK2)
    for k, line in enumerate(['Double-click to install', 'everything. macOS may ask', 'you to allow it once.']):
        w = d.textlength(line, font=small)
        d.text((s(PKG[0]) - w / 2, s(PKG[1] + 54 + 15 * k)), line, font=small, fill=INK2)
    # right, recommended (a yellow key, as CONTROL ALL's): each one onto its folder, then the read-me's Terminal command
    t = 'RECOMMENDED'
    kw = d.textlength(t, font=px) / scale + 16
    d.rounded_rectangle((s(330), s(106), s(330 + kw), s(120)), radius=s(3), fill=HOT)
    d.text((s(338), s(109)), t, font=px, fill=INK)
    d.text((s(330 + kw + 8), s(109)), 'DRAG EACH ONE ONTO ITS FOLDER', font=px, fill=INK2)
    d.text((s(330), s(125)), "THEN PASTE LISEZ-MOI'S COMMAND IN TERMINAL", font=px, fill=INK2)
    # the last row: the folder IMPAKT holds CHIMERA's sounds
    t2 = "CHIMERA'S SOUNDS"
    d.text((s(410) - d.textlength(t2, font=px) / 2, s(ROWS[-1][1] + 58)), t2, font=px, fill=INK3)
    for (fx, y, gx) in ROWS:
        a, b = fx + 48, gx - 48
        d.line((s(a), s(y - 8), s(b), s(y - 8)), fill=INK3, width=max(1, s(2)))
        d.polygon([(s(b + 2), s(y - 8)), (s(b - 8), s(y - 14)), (s(b - 8), s(y - 2))], fill=INK3)
    for k, line in enumerate(['Quit your music software', 'before installing.']):
        w = d.textlength(line, font=small)
        d.text((s(PKG[0]) - w / 2, s(PKG[1] + 122 + 16 * k)), line, font=small, fill=INK3)
    # under it, the read-me: the Terminal command
    t = 'THE TERMINAL COMMAND'
    d.text((s(PKG[0]) - d.textlength(t, font=px) / 2, s(422)), t, font=px, fill=INK2)
    return im


for scale, name in ((1, 'dmg-background.png'), (2, 'dmg-background@2x.png')):
    draw(scale).save(os.path.join(sys.argv[1] if len(sys.argv) > 1 else HERE, name), optimize=True)
    print(name)
