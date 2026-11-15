// SPDX-License-Identifier: GPL-3.0-only
// MRC: the phone remote's service worker -- makes the page installable and opens it offline.
// Network first, so a new site build is picked up at once; the cache is only the fallback.
const CACHE = "mrc-remote-v2";
const SHELL = ["./", "manifest.webmanifest", "icon-192.png", "icon-512.png", "../fonts/InterTight.ttf"];
self.addEventListener("install", (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(SHELL)).then(() => self.skipWaiting()));
});
self.addEventListener("activate", (e) => {
  e.waitUntil(caches.keys().then((ks) => Promise.all(ks.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
    .then(() => self.clients.claim()));
});
self.addEventListener("fetch", (e) => {
  if (e.request.method !== "GET") return;
  e.respondWith(fetch(e.request).then((r) => {
    if (r.ok && new URL(e.request.url).origin === location.origin) {
      const copy = r.clone();
      caches.open(CACHE).then((c) => c.put(e.request, copy));
    }
    return r;
  }).catch(() => caches.match(e.request, { ignoreSearch: true })));
});
