MRuby::Toolchain.new('emscripten') do |conf|
  toolchain :clang
  
  conf.cc.command = 'emcc'
  conf.cxx.command = 'emcc'
  conf.linker.command = 'emcc'
  conf.archiver.command = 'emar'
end

MRuby::CrossBuild.new('wasm') do |conf|
  toolchain :emscripten

  # include the GEM box
  conf.gembox 'default'
  
  conf.enable_bintest
  conf.enable_test
end
