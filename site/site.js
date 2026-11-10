// IMPAKT's site: the visitor's system first (the downloads, the guide), the release really published, copy buttons,
// the top bar always within reach, the manual's contents following the reading.
(function () {
  const ua = navigator.userAgent || '', plat = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || '';
  const os = /Win/i.test(plat) || /Windows/i.test(ua) ? 'win' : /Linux/i.test(plat) && !/Android/i.test(ua) ? 'linux'
    : /Mac/i.test(plat) && !/iPhone|iPad/i.test(ua) && navigator.maxTouchPoints < 2 ? 'mac' : /iPhone|iPad|Android/i.test(ua) ? 'web' : 'mac';

  // home: the key of this system lit (and first)
  const keys = document.getElementById('dl');
  if (keys && os !== 'web') {
    keys.querySelectorAll('.key').forEach(k => k.classList.toggle('hot', k.dataset.os === os));
    const mine = keys.querySelector('[data-os="' + os + '"]');
    if (mine) keys.prepend(mine);
  }

  // the guide: its tabs, opened on this system (or the one in the address: #mac, #win, #linux, #web)
  const tabs = [...document.querySelectorAll('[role="tab"]')];
  const show = id => tabs.forEach(t => {
    const on = t.id === 't-' + id;
    t.setAttribute('aria-selected', String(on)); t.tabIndex = on ? 0 : -1;
    document.getElementById(t.getAttribute('aria-controls')).hidden = !on;
  });
  if (tabs.length) {
    const fromHash = (location.hash || '').slice(1);
    show(['mac', 'win', 'linux', 'web'].includes(fromHash) ? fromHash : os);
    tabs.forEach((t, i) => {
      t.addEventListener('click', () => show(t.id.slice(2)));
      t.addEventListener('keydown', e => {
        const d = e.key === 'ArrowRight' ? 1 : e.key === 'ArrowLeft' ? -1 : 0;
        if (!d) return;
        const n = tabs[(i + d + tabs.length) % tabs.length]; show(n.id.slice(2)); n.focus();
      });
    });
  }

  // copy buttons beside the commands
  document.querySelectorAll('.cmd .copy').forEach(b => b.addEventListener('click', () => {
    const text = b.parentNode.querySelector('code').textContent;
    const done = () => { b.textContent = 'Copié'; setTimeout(() => { b.textContent = 'Copier'; }, 1500); };
    if (navigator.clipboard) navigator.clipboard.writeText(text).then(done, () => {});
  }));

  // the release really published (its version and date), from GitHub
  const rel = document.querySelector('[data-release]');
  if (rel) fetch('https://api.github.com/repos/distortedimages-code/IMPAKT/releases/latest')
    .then(r => r.ok ? r.json() : null).then(j => {
      if (!j || !j.tag_name) return;
      const d = new Date(j.published_at);
      rel.textContent = 'version ' + j.tag_name.replace(/^v/, '') + ' du ' + d.toLocaleDateString('fr-FR', { day: 'numeric', month: 'long', year: 'numeric' });
    }).catch(() => {});

  // the top bar stays at the top: its height, for the anchors and the contents under it
  const bar = document.querySelector('.top');
  if (bar) {
    const setH = () => document.documentElement.style.setProperty('--hh', bar.offsetHeight + 'px');
    setH();
    if ('ResizeObserver' in window) new ResizeObserver(setH).observe(bar); else addEventListener('resize', setH);
  }

  // the manual: the part being read lit in the contents (kept in view in its column); on a phone, the contents open
  // from the bar's Sommaire key and close on a choice, Escape or a tap elsewhere
  const toc = document.querySelector('.toc'), btn = document.querySelector('.tocbtn');
  if (toc && btn) {
    const open = on => { toc.classList.toggle('open', on); btn.setAttribute('aria-expanded', String(on)); };
    btn.addEventListener('click', e => { e.stopPropagation(); open(!toc.classList.contains('open')); });
    toc.addEventListener('click', e => { if (e.target.closest('a')) open(false); });
    document.addEventListener('keydown', e => { if (e.key === 'Escape' && toc.classList.contains('open')) { open(false); btn.focus(); } });
    document.addEventListener('pointerdown', e => { if (toc.classList.contains('open') && !toc.contains(e.target) && e.target !== btn) open(false); });
  }
  if (toc && 'IntersectionObserver' in window) {
    const links = new Map([...toc.querySelectorAll('a')].map(a => [decodeURIComponent(a.hash.slice(1)), a]));
    let cur = null;
    const io = new IntersectionObserver(es => es.forEach(e => {
      if (!e.isIntersecting) return;
      const a = links.get(e.target.id); if (!a || a === cur) return;
      if (cur) cur.classList.remove('here'); cur = a; a.classList.add('here');
      if (toc.scrollHeight > toc.clientHeight && (a.offsetTop < toc.scrollTop || a.offsetTop > toc.scrollTop + toc.clientHeight - 40))
        toc.scrollTop = a.offsetTop - toc.clientHeight / 3;
    }), { rootMargin: '-80px 0px -70% 0px' });
    document.querySelectorAll('.doc h2[id], .doc h3[id]').forEach(h => { if (links.has(h.id)) io.observe(h); });
  }

  // the screen of machines: one lit at a time, slowly, like the picker (unless motion is unwelcome)
  const grid = document.querySelector('.mgrid');
  if (grid && !matchMedia('(prefers-reduced-motion: reduce)').matches) {
    const items = [...grid.children]; let k = -1;
    setInterval(() => { if (k >= 0) items[k].classList.remove('on'); k = (k + 1 + Math.floor(Math.random() * 5)) % items.length; items[k].classList.add('on'); }, 900);
  }
})();
