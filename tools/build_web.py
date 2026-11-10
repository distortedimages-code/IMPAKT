#!/usr/bin/env python3
"""Assemble the IMPAKT web UI.

Outputs
  dist/web/index.html          page body for the hosted artifact (no doctype; wasm embedded)
  dist/web/impakt-worklet.js   AudioWorklet processor
  dist/web/impakt-asm.js       plain-JS engine fallback (wasm2js), loaded only if WebAssembly is blocked
  dist/local/                  same files as a complete HTML document, for local testing
  plugin/Source/ui/index.html  complete document for the JUCE WebView (engine runs natively)
"""
import base64, json, os, re, shutil, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

tpl = open('web/src/index.template.html', encoding='utf-8').read()

# Embedded fonts (SIL OFL) for the plugin / offline copies; the hosted page links Google Fonts.
FONTS = [('Big Shoulders Display', 800, 'big-shoulders-display-latin-800-normal.woff2', None),
         ('Big Shoulders Display', 900, 'big-shoulders-display-latin-900-normal.woff2', None),
         ('Martian Mono', '100 800', 'martian-mono-latin-wdth-normal.woff2', '75% 112.5%'),  # variable: weight and width
         ('Saira', '100 900', 'saira-latin-wdth-normal.woff2', '50% 125%'),               # variable: weight and width (the wide cut at 125%)
         ('Saira', '100 900', 'saira-latin-wdth-italic.woff2', '50% 125%', 'italic'),     # its italic: the FX page's vertical page names
         ('Silkscreen', 400, 'silkscreen-latin-400-normal.woff2', None),                  # the screens' pixels
         ('Sofia Sans Extra Condensed', 500, 'sofia-sans-extra-condensed-latin-500-normal.woff2', None),
         ('Sofia Sans Extra Condensed', 700, 'sofia-sans-extra-condensed-latin-700-normal.woff2', None),
         ('Sofia Sans Extra Condensed', 800, 'sofia-sans-extra-condensed-latin-800-normal.woff2', None)]
font_css = '<style>\n' + '\n'.join(
    "@font-face{font-family:'%s';font-style:%s;font-weight:%s;%sfont-display:swap;src:url(data:font/woff2;base64,%s) format('woff2')}"
    % (t[0], t[4] if len(t) > 4 else 'normal', t[1], 'font-stretch:%s;' % t[3] if t[3] else '', base64.b64encode(open('web/fonts/' + t[2], 'rb').read()).decode())
    for t in FONTS) + '\n</style>'
LINKS = re.compile(r'<link rel="preconnect"[^>]*>\s*<link rel="preconnect"[^>]*>\s*<link rel="stylesheet" href="https://fonts.googleapis.com[^>]*>')
assert LINKS.search(tpl)
# the logo (brand/: traced from the artwork), inline where the page asks for it
def brand_svg(name, cls):
    svg = open('brand/impakt-%s.svg' % name, encoding='utf-8').read().strip()
    return svg.replace('<svg ', '<svg class="%s" aria-hidden="true" focusable="false" ' % cls, 1).replace(' role="img"', '').replace(re.search(r' aria-label="[^"]*"', svg).group(0), '')
tpl = tpl.replace('<!--BRAND:wordmark-->', brand_svg('wordmark', 'wm'))
head, body = tpl.split('<!--BODY-->', 1)
app_js = '\n'.join(open('web/src/' + f, encoding='utf-8').read() for f in ('chimera_names.js', 'presets.js', 'preset_levels.js', 'preset_tone.js', 'lfo_tables.js', 'app.js'))
# CHIMERA's bank (v66): its index in the page (the names, the map, the outlines; the engine's copy is posted from it),
# its sounds beside it, fetched the first time a track plays CHIMERA: each bank/chimera.N.bin as text, its base64 in
# parts of 9 MB (bank/chimera.N.P.txt: where the page is hosted, only text, media and web files are served). CHIM_PARTS:
# each file's length and how many parts
chim_idx = base64.b64encode(open('bank/chimera.idx', 'rb').read()).decode() if os.path.exists('bank/chimera.idx') else ''
BANK_PART = 9 * 1000 * 1000
chim_parts = []
while os.path.exists('bank/chimera.%d.bin' % len(chim_parts)):
    n = os.path.getsize('bank/chimera.%d.bin' % len(chim_parts))
    chim_parts.append([n, (n + BANK_PART - 1) // BANK_PART])
VERSION = open('VERSION', encoding='utf-8').read().strip()
body = body.replace('<!--SCRIPT-->', '<script>\nconst IMPAKT_VERSION = "' + VERSION + '";\nconst WASM_B64 = "__WASM_B64__";\nconst WASM_SIMD_B64 = "__WASM_SIMD_B64__";\nconst CHIM_IDX_B64 = "' + chim_idx + '";\nconst CHIM_PARTS = ' + json.dumps(chim_parts) + ';\n</script>\n<script>\n' + app_js + '</script>')
wasm_b64 = base64.b64encode(open('web/impakt.wasm', 'rb').read()).decode()
simd_b64 = base64.b64encode(open('web/impakt-simd.wasm', 'rb').read()).decode()

def doc(head, body, lang='en'):
    return ('<!doctype html>\n<html lang="%s">\n<head>\n<meta charset="utf-8">\n'
            '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n'
            '<style>html,body{margin:0}[hidden]{display:none!important}</style>\n%s\n</head>\n<body>\n%s\n</body>\n</html>\n') % (lang, head, body)

# wasm2js fallback (binaryen: `npm install binaryen`, or set WASM2JS=/path/to/wasm2js)
wasm2js = os.environ.get('WASM2JS') or shutil.which('wasm2js') or os.path.join(ROOT, 'node_modules/.bin/wasm2js')
os.makedirs('build', exist_ok=True)
if os.path.exists(wasm2js):
    subprocess.check_call([wasm2js, '--enable-bulk-memory', '--enable-nontrapping-float-to-int', '--enable-sign-ext',
                           '-O3', 'web/impakt-lite.wasm' if os.path.exists('web/impakt-lite.wasm') else 'web/impakt.wasm', '-o', 'build/impakt_wasm2js.mjs'])
    src = open('build/impakt_wasm2js.mjs', encoding='utf-8').read()
    src = re.sub(r'^export var .*$', '', src, flags=re.M)
    src = re.sub(r'^import .*$', '', src, flags=re.M)
    asm = ('// IMPAKT engine, plain-JS build (wasm2js) used when WebAssembly is unavailable.\n'
           'function impaktAsm() {\n' + src + '\nreturn retasmFunc;\n}\nglobalThis.impaktAsm = impaktAsm;\n')
else:
    print('wasm2js not found: keeping the existing dist/web/impakt-asm.js (may be out of date)')
    asm = open('dist/web/impakt-asm.js', encoding='utf-8').read()

web_body = body.replace('__WASM_B64__', wasm_b64).replace('__WASM_SIMD_B64__', simd_b64)
for d in ('dist/web', 'dist/local'):
    os.makedirs(d, exist_ok=True)
    if chim_parts:
        shutil.rmtree(d + '/bank', ignore_errors=True); os.makedirs(d + '/bank')
        for k, (n, np_) in enumerate(chim_parts):
            data = open('bank/chimera.%d.bin' % k, 'rb').read()
            for p in range(np_):
                open(d + '/bank/chimera.%d.%d.txt' % (k, p), 'wb').write(base64.b64encode(data[p * BANK_PART:(p + 1) * BANK_PART]))
    shutil.copy('web/src/impakt-worklet.js', d + '/impakt-worklet.js')
    open(d + '/impakt-asm.js', 'w', encoding='utf-8').write(asm)
open('dist/web/index.html', 'w', encoding='utf-8').write(head + web_body)
open('dist/local/index.html', 'w', encoding='utf-8').write(doc(LINKS.sub(font_css, head), web_body))

def asciify(html):
    """Make the page pure ASCII: some WebView resource handlers ignore the charset."""
    out, i = [], 0
    for m in re.finditer(r'(<style>.*?</style>)|(<script>.*?</script>)', html, flags=re.S):
        out.append(re.sub(r'[^\x00-\x7f]', lambda c: '&#x%X;' % ord(c.group()), html[i:m.start()]))
        block = m.group()
        if m.group(1):
            block = re.sub(r'[^\x00-\x7f]', lambda c: '\\%06X ' % ord(c.group()), block)
        else:
            block = re.sub(r'[^\x00-\x7f]', lambda c: '\\u%04X' % ord(c.group()), block)
        out.append(block)
        i = m.end()
    out.append(re.sub(r'[^\x00-\x7f]', lambda c: '&#x%X;' % ord(c.group()), html[i:]))
    res = ''.join(out)
    assert all(ord(ch) < 128 for ch in res)
    return res

os.makedirs('plugin/Source/ui', exist_ok=True)
open('plugin/Source/ui/index.html', 'w', encoding='ascii').write(asciify(doc(LINKS.sub(font_css, head), body.replace('__WASM_B64__', '').replace('__WASM_SIMD_B64__', ''))))

for f in ('dist/web/index.html', 'dist/web/impakt-worklet.js', 'dist/web/impakt-asm.js', 'plugin/Source/ui/index.html'):
    print('%-32s %8d bytes' % (f, os.path.getsize(f)))

