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
})();
