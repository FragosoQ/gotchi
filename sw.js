// Ergo'Gotchi — Service Worker (PWA offline)
// Estratégia: index network-first (updates chegam), CDN/modelo cache-first.
const CACHE = 'ergogotchi-v3';
const CORE = ['./', './index.html', './manifest.webmanifest',
              './icons/icon-192.png', './icons/icon-512.png'];

self.addEventListener('install', e => {
  e.waitUntil(caches.open(CACHE).then(c => c.addAll(CORE)).then(() => self.skipWaiting()));
});
self.addEventListener('activate', e => {
  e.waitUntil(caches.keys()
    .then(ks => Promise.all(ks.filter(k => k !== CACHE).map(k => caches.delete(k))))
    .then(() => self.clients.claim()));
});
self.addEventListener('fetch', e => {
  if (e.request.method !== 'GET') return;
  const u = new URL(e.request.url);

  // NUNCA intercetar a rede local (colar ESP32, Ollama) — sempre direto
  if (/^(192\.168\.|10\.|172\.(1[6-9]|2\d|3[01])\.|localhost$|127\.)/.test(u.hostname)) return;

  // navegação: network-first com fallback ao cache (funciona offline)
  if (e.request.mode === 'navigate') {
    e.respondWith(
      fetch(e.request).then(r => {
        const cp = r.clone();
        caches.open(CACHE).then(c => c.put('./index.html', cp));
        return r;
      }).catch(() => caches.match('./index.html'))
    );
    return;
  }

  // Só cache-first para hosts CONHECIDOS (MediaPipe, fontes, próprios).
  // Tudo o resto (ex.: pesos do WebLLM no huggingface) passa DIRETO,
  // sem interceção — o WebLLM gere a sua própria cache.
  const cacheable = u.origin === location.origin ||
    ['cdn.jsdelivr.net', 'esm.run', 'storage.googleapis.com',
     'fonts.googleapis.com', 'fonts.gstatic.com'].includes(u.hostname);
  if (!cacheable) return;
  e.respondWith(
    caches.match(e.request).then(hit => hit || fetch(e.request).then(r => {
      if (r.ok) {
        const cp = r.clone();
        caches.open(CACHE).then(c => c.put(e.request, cp));
      }
      return r;
    }))
  );
});
