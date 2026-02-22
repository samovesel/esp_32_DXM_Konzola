// sw.js — Service Worker za DMX Konzola PWA
// Cachira persona UI datoteke za offline zagon
// Strategija: cache-first za persona statiko, network-only za API/WS/Pro UI

var CACHE_NAME = 'dmx-pwa-v2';

var CACHE_URLS = [
    '/portal',
    '/sound-eng',
    '/theater',
    '/dj',
    '/staff',
    '/event',
    '/busker',
    '/persona-core.js',
    '/sw.js',
    '/icon-192.png',
    '/icon-512.png'
];

// Install: pre-cache vseh persona datotek
self.addEventListener('install', function(event) {
    event.waitUntil(
        caches.open(CACHE_NAME).then(function(cache) {
            console.log('[SW] Pre-caching persona files');
            return cache.addAll(CACHE_URLS);
        }).then(function() {
            return self.skipWaiting();
        })
    );
});

// Activate: pobriši stare cache-e
self.addEventListener('activate', function(event) {
    event.waitUntil(
        caches.keys().then(function(names) {
            return Promise.all(
                names.filter(function(name) {
                    return name.startsWith('dmx-pwa-') && name !== CACHE_NAME;
                }).map(function(name) {
                    console.log('[SW] Deleting old cache:', name);
                    return caches.delete(name);
                })
            );
        }).then(function() {
            return self.clients.claim();
        })
    );
});

// Fetch: cache-first za persona datoteke, network-only za ostalo
self.addEventListener('fetch', function(event) {
    var url = new URL(event.request.url);

    // Samo GET requesti
    if (event.request.method !== 'GET') return;

    // Preskoči WebSocket, API klice, Pro UI
    if (url.pathname.startsWith('/api/') || url.pathname === '/ws' || url.pathname === '/') return;

    // Preveri ali je ta URL v cache seznamu
    var shouldCache = CACHE_URLS.indexOf(url.pathname) !== -1;
    if (!shouldCache) return;

    event.respondWith(
        caches.match(event.request).then(function(cached) {
            if (cached) {
                // Vrni iz cache-a, ampak tudi posodobi cache v ozadju
                fetch(event.request).then(function(response) {
                    if (response && response.status === 200) {
                        caches.open(CACHE_NAME).then(function(cache) {
                            cache.put(event.request, response);
                        });
                    }
                }).catch(function() {});
                return cached;
            }
            // Ni v cache-u — fetch in shrani
            return fetch(event.request).then(function(response) {
                if (!response || response.status !== 200) return response;
                var clone = response.clone();
                caches.open(CACHE_NAME).then(function(cache) {
                    cache.put(event.request, clone);
                });
                return response;
            });
        })
    );
});
