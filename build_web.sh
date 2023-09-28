cd build
emcmake cmake -DBUILD_SDLGPU=On -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug
cp bin/tic80.js webapp/tic80.js
cp bin/tic80.wasm webapp/tic80.wasm
python -m http.server 8008 --directory webapp