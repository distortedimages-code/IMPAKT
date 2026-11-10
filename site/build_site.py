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


PHOTOS = {
    'etabli-vu-de-haut': 'Deux personnes en blouse grise penchées sur un établi : une carte électronique qu\u2019on soude, un multimètre, des composants triés, un cahier de mesures.',
    'enveloppe-ecran': 'Une femme regarde, menton dans la main, un écran où s\u2019affichent une forme d\u2019onde et une enveloppe orange.',
    'soudure-gros-plan': 'Gros plan sur une soudure : le fer et la pince posés sur une carte, entre des boutons et des câbles orange.',
    'analyse-spectre': 'Un technicien règle un oscilloscope ; à côté, un analyseur affiche un spectre.',
    'table-equipe': 'Quatre personnes autour d\u2019une table couverte de schémas et d\u2019appareils de mesure, un système modulaire au fond.',
    'atelier-oscilloscope': 'Un homme soude une carte de synthétiseur, la main sur les boutons d\u2019un oscilloscope qui affiche une sinusoïde.',
    'soudure-miroir': 'Un homme soude une carte devant un panneau de métal où il se reflète.',
    'patch-modulaire': 'Une femme câble un synthétiseur modulaire dans un atelier ; un écran affiche une forme d\u2019onde jaune.',
    'ecoute-casques': 'Trois personnes au casque écoutent devant des écrans d\u2019analyse et des machines aux LED orange.',
}


def photo(name, cls='photo', eager=False):
    """A photo of the workshop (site/img: 1600 and 800 pixels wide, WebP)."""
    return ('<img class="%s" src="img/%s-1600.webp" srcset="img/%s-800.webp 800w, img/%s-1600.webp 1600w" '
            'sizes="(max-width: 900px) 100vw, 1240px" width="1600" height="900" alt="%s"%s>') % (
        cls, name, name, name, html.escape(PHOTOS[name]), '' if eager else ' loading="lazy" decoding="async"')


def page(name, title, body, desc, cls='', extra=''):
    nav = [('index.html', 'IMPAKT'), ('machines.html', 'Machines'), ('install.html', 'Installer'), ('manuel.html', 'Manuel'), ('versions.html', 'Versions')]
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
<script src="site.js"></script>%(extra)s
</body>
</html>
''' % dict(title=html.escape(title), desc=html.escape(desc), cls=cls, links=links, gh=GH, body=body, ver=VERSION, mark=brand('wordmark', 'wm'), extra=extra)


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
    %s
    <h1>Manuel</h1>
    <p class="lede">Tout ce qu'IMPAKT contient, machine par machine, réglage par réglage. L'interface est en anglais :
    le manuel cite ses libellés tels qu'ils sont à l'écran.</p>
    %s
  </article>
</main>''' % (toc, photo('patch-modulaire', 'photo banner', True), manual_html)

# ------------------------------------------------------------------ the versions
changes_md = open(os.path.join(ROOT, 'CHANGELOG.md'), encoding='utf-8').read()
changes_md = re.sub(r'^# .*\n', '', changes_md, count=1)
versions_page = '''<main class="narrow">
  %s
  <h1>Versions</h1>
  <p class="lede">Ce qui change d'une version à l'autre. Le plugin et l'application le signalent d'eux-mêmes
  (OPTION &gt; Updates) ; les fichiers de chaque version sont sur <a href="%s/releases">GitHub</a>.</p>
  <div class="doc">%s</div>
</main>''' % (photo('ecoute-casques', 'photo banner', True), GH, md(changes_md))

# ------------------------------------------------------------------ the machines
# their names and families as the instrument has them (web/src/app.js), their sounds as the engine plays them
# (site/machines-data.json: tools/render_test site), what they refer to (site/machines-refs.json)
app_js = open(os.path.join(ROOT, 'web', 'src', 'app.js'), encoding='utf-8').read()
mach_block = app_js[app_js.index('const MACH = ['):app_js.index('\n];', app_js.index('const MACH = ['))]
MACHS = re.findall(r"^  \{ id: '([^']+)', long: '([^']+)'", mach_block, re.M)
groups_block = app_js[app_js.index('const GROUPS = ['):app_js.index('\n];', app_js.index('const GROUPS = ['))]
GROUPS = [(g, re.findall(r"'([^']+)'", ids)) for g, ids in re.findall(r"\['([^']+)', \[([^\]]+)\]\]", groups_block)]
sounds = json.load(open(os.path.join(SITE, 'machines-data.json'), encoding='utf-8'))
refs_path = os.path.join(SITE, 'machines-refs.json')
refs = json.load(open(refs_path, encoding='utf-8')) if os.path.exists(refs_path) else {}
FAMILY_FR = {'KICK': 'Grosses caisses', 'SNARE + PERC': 'Caisses claires et percussions', 'METAL + NOISE': 'Métal et bruit',
             'LOOP + TEXTURE': 'Boucles et textures', 'SYNTH': 'Synthés', 'CHORDS + VOICE': 'Accords et voix', 'FX': 'Machines d’effets'}
FAMILY_LEDE = {'FX': 'Elles ne jouent pas de son à elles : elles passent les pistes à leur gauche dans un effet que leurs trigs séquencent. Leur écran montre une caisse claire passée dans l’effet.'}
sections = {gh_slug(h.split(' : ')[0].strip()): gh_slug(h) for h in re.findall(r'^## (.+)$', body_md, re.M)}


def mslug(n):
    return re.sub(r'[^a-z0-9]+', '-', n.lower().replace('µ', 'u').replace('·', '-')).strip('-')


def machine_card(i, mid, long):
    snd = sounds.get(str(i), {})
    r = refs.get(mid, {})
    sec = sections.get(gh_slug(mid))
    more = '<a href="manuel.html#%s">Dans le manuel</a>' % (sec if sec else 'les-47-machines')
    src = ''.join('<a href="%s" rel="noopener">%s</a>' % (html.escape(u), html.escape(re.sub(r'^https?://(www\.)?', '', u).split('/')[0])) for u in r.get('sources', []))
    return ('<article class="mcard" id="%s">'
            '<canvas class="scope" data-m="%d" width="320" height="96" aria-hidden="true"></canvas>'
            '<div class="mtext"><h3>%s</h3><p class="mlong">%s</p>%s%s'
            '<p class="mlinks">%s%s</p></div></article>') % (
        mslug(mid), i, html.escape(mid), html.escape(long),
        '<p class="mref">%s</p>' % html.escape(r['ref']) if r.get('ref') else '',
        '<p>%s</p>' % html.escape(r['text']) if r.get('text') else '',
        more, ('<span class="msrc">Sources : %s</span>' % src) if src else '')


index_of = {mid: i for i, (mid, _) in enumerate(MACHS)}
long_of = dict(MACHS)
fam_html = ''
for g, ids in GROUPS:
    cards = ''.join(machine_card(index_of[m], m, long_of[m]) for m in ids if m in index_of)
    fam_html += '<section class="family" id="%s"><h2>%s</h2>%s<div class="mcards">%s</div></section>' % (
        mslug(g), FAMILY_FR.get(g, g.title()), '<p class="flede">%s</p>' % FAMILY_LEDE[g] if g in FAMILY_LEDE else '', cards)
fam_nav = ''.join('<a href="#%s">%s</a>' % (mslug(g), FAMILY_FR.get(g, g)) for g, _ in GROUPS)
scope_data = {str(i): {'d': sounds[str(i)]['dur'], 'e': sounds[str(i)]['env'], 's': sounds[str(i)]['scope']} for i in range(len(MACHS)) if str(i) in sounds}
machines_page = '''<main class="machines">
  <header class="mhead">
    %s
    <h1>Les %d machines</h1>
    <p class="lede">Chaque piste d'IMPAKT choisit une machine. Leurs écrans dessinent leur vrai son, tel que le moteur le
    joue avec leurs réglages par défaut ; leurs textes disent d'où elles viennent : les instruments, les musiciens et les
    techniques auxquels elles font référence.</p>
    <nav class="fams" aria-label="Familles">%s</nav>
  </header>
  %s
</main>
<script type="application/json" id="scopes">%s</script>''' % (photo('atelier-oscilloscope', 'photo banner', True), len(MACHS), fam_nav, fam_html, json.dumps(scope_data, separators=(',', ':')))

# ------------------------------------------------------------------ the home page
grid = ''.join('<li><a href="machines.html#%s" title="%s">%s</a></li>' % (re.sub(r'[^a-z0-9]+', '-', n.lower().replace('µ', 'u').replace('·', '-')).strip('-'), html.escape(i), html.escape(n)) for n, i in machines)
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
  <figure class="band">%(band)s</figure>
  <section class="rows">
    <article>%(ph1)s<h2>Le pas qu'on lève</h2><p>Tous les réglages se verrouillent par pas : la machine, le filtre, les effets.
    Jusqu'à 64 pas par piste, chacune avec sa longueur et sa vitesse, 128 patterns à enchaîner.</p>
    <a href="manuel.html#p-lock--le-pas-quon-lève">P-lock</a></article>
    <article>%(ph2)s<h2>Des machines qui ne se ressemblent pas</h2><p>Analogique virtuel, FM, tables d'ondes en cube, accords qui
    respirent, une guitare devant un ampli qui hurle, un échantillonneur en direct, un sampler qui greffe une attaque sur
    un autre corps, quatre récepteurs radio.</p>
    <a href="manuel.html#les-47-machines">Les machines</a></article>
    <article>%(ph3)s<h2>Des effets qui jouent</h2><p>Les machines FX passent les pistes voisines dans une réverb, un délai à
    bande, un buffer tordu ou un méta-modulateur, séquencés par leurs trigs. Au bout : EQ, compresseur, grain, OTT, glitch.</p>
    <a href="manuel.html#les-machines-fx--des-effets-séquencés-façon-monomachine">Les machines FX</a></article>
  </section>
  <section class="open">
    %(ph4)s
    <div>
      <h2>Libre, de bout en bout</h2>
      <p>IMPAKT est un logiciel libre, sous licence GNU GPL v3. Son code, ses versions et ce site se construisent en
      public : chaque version est compilée pour Mac, Windows et Linux à partir du même code, et le plugin la propose
      de lui-même dès qu'elle sort.</p>
      <p><a href="%(gh)s">Le code sur GitHub</a> · <a href="machines.html">Les machines et leur histoire</a></p>
    </div>
  </section>
</main>''' % dict(n=len(machines), grid=grid, dl=DL, ver=VERSION, logo=brand('logo', 'biglogo', 'IMPAKT, music synthesiser'), gh=GH,
          band=photo('etabli-vu-de-haut', 'photo wide'), ph1=photo('enveloppe-ecran'), ph2=photo('soudure-gros-plan'), ph3=photo('analyse-spectre'),
          ph4=photo('table-equipe'), **ASSETS)

# ------------------------------------------------------------------ the install guide
install = open(os.path.join(SITE, 'install.html.in'), encoding='utf-8').read()
install = install.replace('@PHOTO@', photo('soudure-miroir', 'photo banner', True)).replace('@DL@', DL).replace('@GH@', GH).replace('@VERSION@', VERSION)
for k, v in ASSETS.items():
    install = install.replace('@%s@' % k.upper(), v)

# ------------------------------------------------------------------ write
if os.path.isdir(OUT):
    shutil.rmtree(OUT)
os.makedirs(os.path.join(OUT, 'fonts'))
pages = [('index.html', 'IMPAKT — boîte à rythme de synthèse', home, 'IMPAKT, une boîte à rythme de synthèse à 8 pistes et %d machines : VST3, Audio Unit, application et navigateur. Logiciel libre.' % len(machines), 'home'),
         ('machines.html', 'Les machines d’IMPAKT', machines_page, 'Les %d machines d’IMPAKT : leur son, et les instruments, les musiciens et les techniques auxquels elles font référence.' % len(MACHS), 'mach', '<script src="machines.js"></script>'),
         ('install.html', 'Installer IMPAKT', install, 'Installer IMPAKT sur Mac, Windows ou Linux, le mettre à jour, le retirer.', 'guide'),
         ('manuel.html', 'Manuel d’IMPAKT', manual_page, 'Le manuel complet d’IMPAKT : les machines, le séquenceur, les effets, le master.', 'man'),
         ('versions.html', 'Versions d’IMPAKT', versions_page, 'Les versions d’IMPAKT et leurs nouveautés.', 'vers')]
for row in pages:
    name, title, body, desc, cls = row[:5]
    open(os.path.join(OUT, name), 'w', encoding='utf-8').write(page(name, title, body, desc, cls, row[5] if len(row) > 5 else ''))
shutil.copytree(os.path.join(SITE, 'img'), os.path.join(OUT, 'img'))
for f in ('style.css', 'site.js', 'machines.js', 'favicon.svg'):
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
