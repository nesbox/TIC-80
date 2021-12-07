MRuby::Build.new do |conf|
  # load specific toolchain settings
  conf.toolchain

  # include the GEM box
  conf.gembox 'default'

  conf.enable_bintest
  conf.enable_test
end

MRuby::Toolchain.new('emscripten') do |conf|
  toolchain :clang

  conf.cc.command = ENV['TARGET_CC'] || 'emcc'
  conf.cxx.command = ENV['TARGET_CXX'] || 'emcc'
  conf.linker.command = ENV['TARGET_LD'] || 'emcc'
  conf.archiver.command = ENV['TARGET_AR'] || 'emar'
end

BuildClazz = (ENV['MRUBY_TOOLCHAIN'] || '').empty? ? MRuby::Build : MRuby::CrossBuild

BuildClazz.new('target') do |conf|
  # load specific toolchain settings
  if (ENV['MRUBY_TOOLCHAIN'] || '').empty?
    conf.toolchain
  else
    toolchain ENV['MRUBY_TOOLCHAIN']
  end

  # include the GEM box
  conf.gembox File.expand_path('tic', File.dirname(__FILE__))

  # C compiler settings
  conf.cc do |cc|
    cc.flags = [ENV["TARGET_CFLAGS"] || %w()]
    unless ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
      cc.flags << '-fPIC' 
    end
  end

  # Linker settings
  conf.linker do |linker|
    linker.flags = [ENV['TARGET_LDFLAGS'] || []]
  end

  # file extensions
  conf.exts do |exts|
    exts.object = '.o'
    exts.library = '.a'
    # exts.executable = '' # '.exe' if Windows
  end

  # Turn on `enable_debug` for better debugging
  conf.enable_debug if /DEBUG/ =~ ENV["BUILD_TYPE"]
  conf.enable_bintest
  conf.enable_test
end
