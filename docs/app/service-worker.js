/**
 * Maximum Trainer — Service Worker
 *
 * Caches the WASM application shell and assets for offline use.
 * Cache key includes the SW version so that new deployments bust the cache
 * automatically.
 *
 * Cache strategy:
 *   - App shell assets (JS, WASM, HTML, icons): Cache-first with network fallback.
 *   - Uncached requests: Network-first.
 */

const SW_URL = new URL(self.location.href);
const SW_VERSION =
  (SW_URL.searchParams.get('v') || SW_URL.searchParams.get('version') || 'v1')
    .replace(/[^a-zA-Z0-9._-]/g, '-');
const CACHE_NAME = `maximum-trainer-${SW_VERSION}`;

// Assets to pre-cache at install time.
// The WASM/JS assets are large — we only pre-cache the thin shell files and
// icons; the WASM binary is cached on first fetch (runtime caching below).
const PRECACHE_ASSETS = [
  './',
  './index.html',
  './logger.js',
  './qtloader.js',
  './manifest.json',
  '../assets/images/main_icon.png',
];

// ── Install: pre-cache shell assets ──────────────────────────────────────────
self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME).then((cache) => {
      // Individually fetch and cache each asset; ignore failures for assets
      // that may not yet exist in the current deployment (e.g. WASM binary).
      return Promise.allSettled(
        PRECACHE_ASSETS.map((url) =>
          cache.add(url).catch((err) => {
            console.warn('[SW] Pre-cache failed for', url, err);
          })
        )
      );
    }).then(() => self.skipWaiting())
  );
});

// ── Activate: delete old caches ───────────────────────────────────────────────
self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(
        keys
          .filter((key) => key !== CACHE_NAME)
          .map((key) => {
            console.log('[SW] Deleting old cache:', key);
            return caches.delete(key);
          })
      )
    ).then(() => self.clients.claim())
  );
});

// ── Fetch: cache-first for same-origin requests ───────────────────────────────
self.addEventListener('fetch', (event) => {
  // Only intercept GET requests to same origin.
  if (event.request.method !== 'GET') return;

  const url = new URL(event.request.url);
  if (url.origin !== self.location.origin) return;

  event.respondWith(
    caches.match(event.request).then((cached) => {
      if (cached) {
        // Return cached version and update cache in background for
        // HTML/JS files so the next load gets fresh content.
        const isRevalidatable =
          event.request.url.endsWith('.html') ||
          event.request.url.endsWith('.js');

        if (isRevalidatable) {
          const networkUpdate = fetch(event.request)
            .then((response) => {
              if (response && response.status === 200) {
                caches.open(CACHE_NAME).then((cache) =>
                  cache.put(event.request, response.clone())
                );
              }
              return response;
            })
            .catch(() => null);

          // Return stale-while-revalidate
          void networkUpdate;
        }

        return cached;
      }

      // Not in cache — fetch from network and cache the response.
      return fetch(event.request).then((response) => {
        if (!response || response.status !== 200 || response.type === 'opaque') {
          return response;
        }

        // Cache WASM, JS, and image assets for offline use.
        const shouldCache =
          event.request.url.endsWith('.wasm') ||
          event.request.url.endsWith('.js')   ||
          event.request.url.endsWith('.html') ||
          event.request.url.endsWith('.png')  ||
          event.request.url.endsWith('.svg')  ||
          event.request.url.endsWith('.json');

        if (shouldCache) {
          caches.open(CACHE_NAME).then((cache) =>
            cache.put(event.request, response.clone())
          );
        }

        return response;
      }).catch(() => {
        // Network failed and not in cache — return a minimal offline page
        // for navigation requests.
        if (event.request.mode === 'navigate') {
          return caches.match('./index.html');
        }
        return new Response('', { status: 503, statusText: 'Service Unavailable' });
      });
    })
  );
});
