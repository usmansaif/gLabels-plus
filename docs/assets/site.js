// gLabels Plus website
(function () {
  var $ = function (s, r) { return (r || document).querySelector(s); };
  var $$ = function (s, r) { return Array.prototype.slice.call((r || document).querySelectorAll(s)); };

  // Mobile navigation
  var toggle = $('.nav-toggle'), nav = $('#nav');
  if (toggle && nav) {
    toggle.addEventListener('click', function () {
      var open = nav.classList.toggle('open');
      toggle.setAttribute('aria-expanded', open);
    });
    nav.addEventListener('click', function (e) {
      if (e.target.closest('a')) { nav.classList.remove('open'); toggle.setAttribute('aria-expanded', 'false'); }
    });
  }

  // Screenshot lightbox
  var lb = $('#lightbox');
  if (lb) {
    var lbImg = $('img', lb);
    $$('.shot a, .hero-shot').forEach(function (el) {
      el.addEventListener('click', function (e) {
        var img = $('img', el);
        if (!img || e.target.closest('.release')) return;
        e.preventDefault();
        lbImg.src = img.src; lbImg.alt = img.alt; lb.hidden = false;
      });
    });
    var closeLb = function () { lb.hidden = true; lbImg.src = ''; };
    lb.addEventListener('click', closeLb);
    document.addEventListener('keydown', function (e) { if (e.key === 'Escape' && !lb.hidden) closeLb(); });
  }

  // Reveal on scroll
  if ('IntersectionObserver' in window) {
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (en) { if (en.isIntersecting) { en.target.classList.add('in'); io.unobserve(en.target); } });
    }, { threshold: 0.08 });
    $$('.card, .shot, .donate, .preset, .ticks li, .faq details, .table-wrap, .steps li, .keys li').forEach(function (el) {
      el.classList.add('reveal'); io.observe(el);
    });
  }

  // Donation email (assembled at runtime to keep it away from simple scrapers)
  $$('.js-mail').forEach(function (a) {
    var addr = a.dataset.u + '@' + a.dataset.d;
    a.href = 'mailto:' + addr + '?subject=' + encodeURIComponent(a.dataset.s || '');
    $$('.js-mail-text').forEach(function (t) {
      t.textContent = '';
      var link = document.createElement('a'); link.href = a.href; link.textContent = addr; t.appendChild(link);
    });
  });

  // Barcode preset filters
  var chips = $$('.filters button');
  chips.forEach(function (b) {
    b.addEventListener('click', function () {
      chips.forEach(function (c) { c.classList.toggle('on', c === b); c.setAttribute('aria-pressed', c === b); });
      $$('.preset').forEach(function (p) { p.hidden = b.dataset.f !== 'all' && p.dataset.size !== b.dataset.f; });
    });
  });

  // Copy build commands
  $$('.copy').forEach(function (btn) {
    btn.addEventListener('click', function () {
      var code = $('code', btn.closest('.term')).textContent;
      navigator.clipboard.writeText(code).then(function () {
        btn.textContent = 'Copied';
        setTimeout(function () { btn.textContent = 'Copy'; }, 1500);
      });
    });
  });

  // Open a FAQ answer when its #anchor is linked
  var year = $('#year'); if (year) year.textContent = new Date().getFullYear();
  // Download page: point every button at the newest GitHub release. The links
  // written at build time stay as the fallback if the API cannot be reached.
  var dlLinks = $$('a[data-asset]');
  if (dlLinks.length && window.fetch) {
    var ASSETS = {
      deb: /_amd64\.deb$/,
      rpm: /\.x86_64\.rpm$/,
      appimage: /\.AppImage$/,
      linux_tar: /-linux-x86_64\.tar\.gz$/,
      win_exe: /-windows-x64\.exe$/,
      win_zip: /-windows-x64\.zip$/,
      mac_dmg: /-macos-arm64\.dmg$/,
      mac_tar: /-macos-arm64\.tar\.gz$/
    };
    var CACHE_KEY = 'glp-latest-release';
    var applyRelease = function (rel) {
      if (!rel || !rel.assets) return;
      dlLinks.forEach(function (a) {
        var re = ASSETS[a.getAttribute('data-asset')];
        var hit = re && rel.assets.filter(function (x) { return re.test(x.name); })[0];
        if (hit) a.href = hit.browser_download_url;
      });
      var tagLink = $('a[data-release-link]'), tagText = $('[data-release-tag]');
      if (tagLink && rel.html_url) tagLink.href = rel.html_url;
      if (tagText && rel.tag_name) tagText.textContent = rel.tag_name;
    };
    var cached = null;
    try { cached = JSON.parse(sessionStorage.getItem(CACHE_KEY)); } catch (e) {}
    if (cached) applyRelease(cached);
    else {
      fetch('https://api.github.com/repos/usmansaif/gLabels-plus/releases/latest', { headers: { Accept: 'application/vnd.github+json' } })
        .then(function (r) { return r.ok ? r.json() : null; })
        .then(function (rel) {
          if (!rel) return;
          var slim = { tag_name: rel.tag_name, html_url: rel.html_url,
            assets: (rel.assets || []).map(function (x) { return { name: x.name, browser_download_url: x.browser_download_url }; }) };
          try { sessionStorage.setItem(CACHE_KEY, JSON.stringify(slim)); } catch (e) {}
          applyRelease(slim);
        })
        .catch(function () {});
    }
  }
})();
