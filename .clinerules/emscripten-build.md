## Brief overview
  Guidelines for building TIC-80 with Emscripten support, ensuring proper configuration for web deployment with IDBFS.

## Emscripten Configuration
  - Use `cd build && emcmake cmake -G Ninja .. --fresh` in the build folder to configure for Emscripten, ensuring asset files are properly available for cross-compilation.

## Project-Specific Notes
  - This rule applies to TIC-80 projects requiring Emscripten builds for web platforms.
