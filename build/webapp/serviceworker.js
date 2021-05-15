const version = 'tic80-v1'
const assets = [
	'/',
	'/index.html',
	'/tic80.js',
	'/tic80.wasm',
	'/tic80-180.png',
	'/tic80-192.png',
	'/tic80-512.png',
	'/serviceworker.js',
	'/tic80.webmanifest'
]

self.addEventListener('install', function(event) {
  console.log('serviceworker installing')
  caches.open(version)
    .then(function(cache) {
      console.log('service worker loading assets')
      return cache.addAll(assets);
    })
});

self.addEventListener('fetch', function(event) {
  event.respondWith(
    caches.match(event.request)
      .then(function(response) {
        if (response) {
        console.log(`service worker cache hit ${event.request}`)
          return response;
        }
        console.log(`service worker cache miss ${event.request}`)
        return fetch(event.request);
      }
    )
  );
});

console.log('service worker loaded')

