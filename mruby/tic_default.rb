MRuby::Build.new do |conf|
  if ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
    toolchain :visualcpp
  else
    conf.toolchain
  end

  conf.gembox 'default'

  conf.enable_bintest
  conf.enable_test
end

MRuby::CrossBuild.new('target') do |conf|
  toolchain ENV['MRUBY_TOOLCHAIN']

  conf.gembox File.expand_path('tic', File.dirname(__FILE__))

  conf.cc do |cc|
    cc.command = ENV["TARGET_CC"] || 'cc'
    cc.flags = [ENV["TARGET_CFLAGS"] || %w()]
    unless ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
      cc.flags << '-fPIC' 
    end
  end

  conf.linker do |linker|
    linker.command = ENV['TARGET_LD'] || 'ld'
    linker.flags = [ENV['TARGET_LDFLAGS'] || %w()]
  end

  conf.archiver do |archiver|
    archiver.command = ENV['TARGET_AR'] || 'ar'
  end

  conf.exts do |exts|
    exts.object = '.o'
    exts.library = '.a'
    # exts.executable = '' # '.exe' if Windows
  end

  conf.enable_debug if /DEBUG/ =~ ENV["BUILD_TYPE"]
  conf.enable_bintest
  conf.enable_test
end
