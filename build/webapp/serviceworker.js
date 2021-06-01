const version = 'tic80-v1'
const assets = [
	'index.html',
	'tic80.js',
	'tic80.wasm',
	'tic80-180.png',
	'tic80-192.png',
	'tic80-512.png',
	'serviceworker.js',
	'tic80.webmanifest'
]

self.addEventListener('install', function(event) {
  console.log('serviceworker installing')
  caches.open(version)
    .then(function(cache) {
      return cache.addAll(assets);
    })
});

self.addEventListener('fetch', function(event) {
  event.respondWith(
    caches.match(event.request)
      .then(function(response) {
        if (response) {
          return response;
        }
        return fetch(event.request);
      }
    )
  );
});

