const CACHE = "rsvpnano-web-v3";
const SHELL = ["./", "./index.html", "./manifest.webmanifest", "./icon.svg", "./icon-maskable.svg", "./splash.svg", "./splash_light.svg", "./favicon.svg"];
const development = ["localhost", "127.0.0.1", "[::1]"].includes(self.location.hostname) || self.location.hostname.endsWith(".localhost");

if (development) {
  self.addEventListener("install", event => event.waitUntil(self.skipWaiting()));
  self.addEventListener("activate", event => {
    event.waitUntil(Promise.all([
      self.registration.unregister(),
      caches.keys().then(keys => Promise.all(keys.map(key => caches.delete(key)))),
    ]).then(() => self.clients.matchAll()).then(clients => clients.forEach(client => client.navigate(client.url))));
  });
} else self.addEventListener("install", event => {
  event.waitUntil(
    fetch("./asset-manifest.json", { cache: "no-store" })
      .then(response => response.ok ? response.json() : SHELL)
      .catch(() => SHELL)
      .then(assets => caches.open(CACHE).then(cache => cache.addAll(assets)))
  );
  self.skipWaiting();
});

self.addEventListener("activate", event => {
  if (development) return;
  event.waitUntil(caches.keys().then(keys => Promise.all(keys.filter(key => key !== CACHE).map(key => caches.delete(key)))));
  self.clients.claim();
});

self.addEventListener("fetch", event => {
  if (development) return;
  if (event.request.method !== "GET") return;
  const url = new URL(event.request.url);
  if (url.origin !== self.location.origin) return;

  const networkFirst = event.request.mode === "navigate" ||
    url.pathname.includes("/firmware/") ||
    /\.(?:html|js|wasm)$/.test(url.pathname);
  event.respondWith(networkFirst
    ? fetch(event.request).then(response => {
        if (response.ok) caches.open(CACHE).then(cache => cache.put(event.request, response.clone()));
        return response;
      }).catch(() => caches.match(event.request))
    : caches.match(event.request).then(cached => cached || fetch(event.request).then(response => {
        if (response.ok) caches.open(CACHE).then(cache => cache.put(event.request, response.clone()));
        return response;
      }))
  );
});
