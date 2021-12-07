MRuby::Build.new do |conf|
  # load specific toolchain settings
  conf.toolchain

  # include the GEM box
  conf.gembox 'default'

  conf.enable_bintest
  conf.enable_test
end

MRuby::Toolchain.new('target') do |conf|
  toolchain :clang
  
  conf.cc.command = ENV['TARGET_CC'] || 'cc'
  conf.cxx.command = ENV['TARGET_CXX'] || 'cxx'
  conf.linker.command = ENV['TARGET_LD'] || 'ld'
  conf.archiver.command = ENV['TARGET_AR'] || 'ar'
end

MRuby::CrossBuild.new('target') do |conf|
  # load specific toolchain settings
  toolchain :target

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
