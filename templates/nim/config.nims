import std/os

let wasi = getEnv("WASI_SDK_PATH")
if wasi == "" and not defined(nimsuggest):
  echo ""
  echo "Error:"
  echo "Download the WASI SDK (https://github.com/WebAssembly/wasi-sdk) and set the $WASI_SDK_PATH environment variable!"
  echo ""
  quit(-1)

switch("cpu", "wasm32")
switch("cc", "clang")
switch("threads", "off")

# ARC is much more embedded-friendly
switch("gc", "arc")
# Treats defects as errors, results in smaller binary size
switch("panics", "on")

# Use the common ANSI C target
switch("os", "any")
# Needed for os:any
switch("define", "posix")

# WASM has no signal handlers
switch("define", "noSignalHandler")
# The only entrypoints are start and update
switch("noMain")
# Use malloc instead of Nim's memory allocator, makes binary size much smaller
switch("define", "useMalloc")

switch("clang.exe", wasi / "bin" / "clang")
switch("clang.linkerexe", wasi / "bin" / "clang")

switch("passC", "--sysroot=" & (wasi / "share" / "wasi-sysroot"))

switch("passL", "-Wl,-zstack-size=8192,--no-entry,--import-memory -mexec-model=reactor -Wl,--initial-memory=262144,--max-memory=262144,--global-base=98304")

when not defined(release):
  switch("assertions", "off")
  switch("checks", "off")
  switch("stackTrace", "off")
  switch("lineTrace", "off")
  switch("lineDir", "off")
  switch("debugger", "native")
  # This is needed to remove all unused Nim functions that will in turn import
  # WASI stuff that is not available in WASM-4
  switch("passL", "-Wl,--gc-sections")
else:
  switch("opt", "size")
  switch("passC", "-flto")
  switch("passL", "-Wl,--strip-all,--gc-sections,--lto-O3")
