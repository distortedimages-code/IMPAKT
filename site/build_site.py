#!/usr/bin/env python3
"""IMPAKT's site (GitHub Pages): the home page with the downloads, the install guide, the manual (docs/manuel.md),
the versions (CHANGELOG.md) and the web version (app/: dist/local, made by tools/build_web.py).

  python3 site/build_site.py [out]        (default site/out; needs: python-markdown)

The look is the instrument's own: its panel greys, its black screens, its fonts (web/fonts, SIL OFL), its red LED
and its warm yellow key."""
import html, json, os, re, shutil, sys
import markdown

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SITE = os.path.join(ROOT, 'site')
OUT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(SITE, 'out')
REPO = 'distortedimages-code/IMPAKT'
GH = 'https://github.com/' + REPO
DL = GH + '/releases/latest/download/'
VERSION = open(os.path.join(ROOT, 'VERSION'), encoding='utf-8').read().strip()
ASSETS = {'mac': 'IMPAKT-macOS.dmg', 'win': 'IMPAKT-Windows-Setup.exe', 'linux': 'IMPAKT-Linux.tar.gz'}


def gh_slug(value, separator='-'):
    """Heading ids as GitHub makes them (the manual's own links use them): lower case, punctuation gone, spaces to -."""
    v = re.sub(r'<[^>]+>', '', value).strip().lower()
    v = re.sub(r'[^\w\- ]', '', v)
    return v.replace(' ', separator)


def md(text):
    return markdown.markdown(text, extensions=['tables', 'fenced_code', 'sane_lists', 'toc'],
                             extension_configs={'toc': {'slugify': gh_slug, 'separator': '-'}})


def brand(name, cls, label=None):
    """The logo (brand/, traced from the artwork), inline: its ink follows the page's (currentColor)."""
    svg = open(os.path.join(ROOT, 'brand', 'impakt-%s.svg' % name), encoding='utf-8').read().strip()
    svg = re.sub(r' aria-label="[^"]*"', '', svg).replace(' role="img"', '')
    attrs = 'class="%s" ' % cls + ('role="img" aria-label="%s" ' % label if label else 'aria-hidden="true" focusable="false" ')
    return svg.replace('<svg ', '<svg ' + attrs, 1)


def page(name, title, body, desc, cls=''):
    nav = [('index.html', 'IMPAKT'), ('install.html', 'Installer'), ('manuel.html', 'Manuel'), ('versions.html', 'Versions')]
    links = ''.join('<a href="%s"%s>%s</a>' % (h, ' aria-current="page"' if h == name else '', l) for h, l in nav[1:])
    return '''<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>%(title)s</title>
<meta name="description" content="%(desc)s">
<meta property="og:title" content="%(title)s">
<meta property="og:description" content="%(desc)s">
<link rel="stylesheet" href="style.css">
<link rel="icon" href="favicon.svg" type="image/svg+xml">
</head>
<body class="%(cls)s">
<header class="top">
  <a class="mark" href="index.html" aria-label="IMPAKT, accueil">%(mark)s</a>
  <nav class="nav" aria-label="Pages">%(links)s<a href="%(gh)s">Code</a><a class="play" href="app/">Jouer</a></nav>
</header>
%(body)s
<footer class="foot">
  <p><b>IMPAKT</b> <span data-version>%(ver)s</span> · logiciel libre sous licence <a href="%(gh)s/blob/main/LICENSE">GNU GPL v3</a>,
  construit avec JUCE (AGPLv3). Le code, les versions et les échanges sont sur <a href="%(gh)s">GitHub</a>.</p>
  <p>Les crédits et les licences du code repris (Mutable Instruments, BYOD, EaseLFO, minimp3) sont dans le <a href="manuel.html#licences">manuel</a>.</p>
</footer>
<script src="site.js"></script>
</body>
</html>
''' % dict(title=html.escape(title), desc=html.escape(desc), cls=cls, links=links, gh=GH, body=body, ver=VERSION, mark=brand('wordmark', 'wm'))


# ------------------------------------------------------------------ the manual
manual_md = open(os.path.join(ROOT, 'docs', 'manuel.md'), encoding='utf-8').read()
# the machines, from its table (name, idea)
machines = []
m = re.search(r'^## Les \d+ machines\s*\n\s*\n\| Machine \|.*?\n\|[- |]+\|\n((?:\|.*\n)+)', manual_md, re.M)
for row in (m.group(1).splitlines() if m else []):
    cells = [c.strip() for c in row.strip().strip('|').split('|')]
    if len(cells) >= 2:
        idea = re.sub(r'\*\*|`', '', cells[1])
        idea = re.split(r' ?[(;:] ', idea)[0]
        machines.append((cells[0], idea[:90]))
body_md = re.sub(r'^# IMPAKT\s*\n', '', manual_md, count=1)
manual_html = md(body_md)
# wide tables scroll on their own
manual_html = manual_html.replace('<table>', '<div class="tbl"><table>').replace('</table>', '</table></div>')
toc = ''.join('<li><a href="#%s">%s</a></li>' % (gh_slug(h), html.escape(re.sub(r'[*`]', '', h)))
              for h in re.findall(r'^## (.+)$', body_md, re.M))

manual_page = '''<main class="manual">
  <aside class="toc" aria-label="Sommaire">
    <details open><summary>Sommaire</summary><ol>%s</ol></details>
  </aside>
  <article class="doc">
    <h1>Manuel</h1>
    <p class="lede">Tout ce qu'IMPAKT contient, machine par machine, réglage par réglage. L'interface est en anglais :
    le manuel cite ses libellés tels qu'ils sont à l'écran.</p>
    %s
  </article>
</main>''' % (toc, manual_html)

# ------------------------------------------------------------------ the versions
changes_md = open(os.path.join(ROOT, 'CHANGELOG.md'), encoding='utf-8').read()
changes_md = re.sub(r'^# .*\n', '', changes_md, count=1)
versions_page = '''<main class="narrow">
  <h1>Versions</h1>
  <p class="lede">Ce qui change d'une version à l'autre. Le plugin et l'application le signalent d'eux-mêmes
  (OPTION &gt; Updates) ; les fichiers de chaque version sont sur <a href="%s/releases">GitHub</a>.</p>
  <div class="doc">%s</div>
</main>''' % (GH, md(changes_md))

# ------------------------------------------------------------------ the home page
grid = ''.join('<li title="%s">%s</li>' % (html.escape(i), html.escape(n)) for n, i in machines)
home = '''<main>
  <section class="hero">
    <div class="pitch">
      %(logo)s
      <h1>Une boîte à rythme de synthèse, huit pistes, %(n)d machines.</h1>
      <p>Des kicks analogiques aux voix chantées, des cordes grattées à la radio ondes courtes : chaque piste choisit sa
      machine, chaque pas peut tout changer. Séquenceur à p-locks, patterns enchaînés, effets séquencés et section master,
      en VST3, en Audio Unit, en application et dans le navigateur.</p>
      <div class="keys" id="dl">
        <a class="key hot" data-os="mac" href="%(dl)s%(mac)s"><b>macOS</b><small>Apple Silicon et Intel · DMG</small></a>
        <a class="key" data-os="win" href="%(dl)s%(win)s"><b>Windows</b><small>10 et 11 · installeur</small></a>
        <a class="key" data-os="linux" href="%(dl)s%(linux)s"><b>Linux</b><small>x64 · archive</small></a>
      </div>
      <p class="after"><a href="install.html">Comment installer</a> · <a href="app/">Jouer tout de suite dans le navigateur</a>
      · <span class="rel" data-release>version %(ver)s</span></p>
    </div>
    <figure class="lcd" aria-label="Les %(n)d machines">
      <figcaption><span>MACHINES</span><span>%(n)d</span></figcaption>
      <ol class="mgrid">%(grid)s</ol>
    </figure>
  </section>
  <section class="rows">
    <article><h2>Le pas qu'on lève</h2><p>Tous les réglages se verrouillent par pas : la machine, le filtre, les effets.
    Jusqu'à 64 pas par piste, chacune avec sa longueur et sa vitesse, 128 patterns à enchaîner.</p>
    <a href="manuel.html#p-lock--le-pas-quon-lève">P-lock</a></article>
    <article><h2>Des machines qui ne se ressemblent pas</h2><p>Analogique virtuel, FM, tables d'ondes en cube, accords qui
    respirent, une guitare devant un ampli qui hurle, un échantillonneur en direct, un sampler qui greffe une attaque sur
    un autre corps, quatre récepteurs radio.</p>
    <a href="manuel.html#les-47-machines">Les machines</a></article>
    <article><h2>Des effets qui jouent</h2><p>Les machines FX passent les pistes voisines dans une réverb, un délai à
    bande, un buffer tordu ou un méta-modulateur, séquencés par leurs trigs. Au bout : EQ, compresseur, grain, OTT, glitch.</p>
    <a href="manuel.html#les-machines-fx--des-effets-séquencés-façon-monomachine">Les machines FX</a></article>
  </section>
</main>''' % dict(n=len(machines), grid=grid, dl=DL, ver=VERSION, logo=brand('logo', 'biglogo', 'IMPAKT, music synthesiser'), **ASSETS)

# ------------------------------------------------------------------ the install guide
install = open(os.path.join(SITE, 'install.html.in'), encoding='utf-8').read()
install = install.replace('@DL@', DL).replace('@GH@', GH).replace('@VERSION@', VERSION)
for k, v in ASSETS.items():
    install = install.replace('@%s@' % k.upper(), v)

# ------------------------------------------------------------------ write
if os.path.isdir(OUT):
    shutil.rmtree(OUT)
os.makedirs(os.path.join(OUT, 'fonts'))
pages = [('index.html', 'IMPAKT — boîte à rythme de synthèse', home, 'IMPAKT, une boîte à rythme de synthèse à 8 pistes et %d machines : VST3, Audio Unit, application et navigateur. Logiciel libre.' % len(machines), 'home'),
         ('install.html', 'Installer IMPAKT', install, 'Installer IMPAKT sur Mac, Windows ou Linux, le mettre à jour, le retirer.', 'guide'),
         ('manuel.html', 'Manuel d’IMPAKT', manual_page, 'Le manuel complet d’IMPAKT : les machines, le séquenceur, les effets, le master.', 'man'),
         ('versions.html', 'Versions d’IMPAKT', versions_page, 'Les versions d’IMPAKT et leurs nouveautés.', 'vers')]
for name, title, body, desc, cls in pages:
    open(os.path.join(OUT, name), 'w', encoding='utf-8').write(page(name, title, body, desc, cls))
for f in ('style.css', 'site.js', 'favicon.svg'):
    shutil.copy(os.path.join(SITE, f), os.path.join(OUT, f))
for f in ('big-shoulders-display-latin-900-normal.woff2', 'sofia-sans-extra-condensed-latin-500-normal.woff2',
          'sofia-sans-extra-condensed-latin-700-normal.woff2', 'silkscreen-latin-400-normal.woff2', 'martian-mono-latin-wdth-normal.woff2'):
    shutil.copy(os.path.join(ROOT, 'web', 'fonts', f), os.path.join(OUT, 'fonts', f))
# the web version, as tools/build_web.py made it (a complete page, its worklet, its JS engine, CHIMERA's bank)
app = os.path.join(ROOT, 'dist', 'local')
if os.path.exists(os.path.join(app, 'index.html')):
    shutil.copytree(app, os.path.join(OUT, 'app'))
else:
    print('warning: dist/local is missing (run tools/build_web.py): the site has no app/')
open(os.path.join(OUT, 'version.json'), 'w').write(json.dumps({'version': VERSION, 'assets': {k: DL + v for k, v in ASSETS.items()}}) + '\n')
open(os.path.join(OUT, '.nojekyll'), 'w').close()
print('site:', OUT, '·', len(machines), 'machines ·', ', '.join(sorted(os.listdir(OUT))))
