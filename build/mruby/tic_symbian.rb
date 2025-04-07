MRuby::Build.new do |conf|
  if ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
    toolchain :visualcpp
  else
    conf.toolchain
  end

  conf.build_mrbc_exec
  conf.disable_libmruby
  conf.disable_presym
end

MRuby::CrossBuild.new('target') do |conf|
  toolchain ENV['MRUBY_TOOLCHAIN']

  conf.gembox File.expand_path('tic', File.dirname(__FILE__))

  conf.cc do |cc|
    cc.defines << "__PRODUCT_INCLUDE__=\\\"#{ENV["SYMBIAN_PRODUCT_INCLUDE"]}\\\""
    cc.command = ENV["TARGET_CC"] || 'cc'
    cc.flags = [ENV["TARGET_CFLAGS"] || %w()]
  end

  conf.linker do |linker|
    linker.command = ENV['TARGET_CC'] || 'cc'
    linker.flags = [ENV['TARGET_LDFLAGS'] || %w()]
  end

  conf.archiver do |archiver|
    archiver.command = ENV['TARGET_AR'] || 'ar'
  end

  conf.exts do |exts|
    exts.object = '.o'
    exts.library = '.a'
  end

  conf.enable_debug if /DEBUG/ =~ ENV["BUILD_TYPE"]
  conf.enable_bintest
  conf.enable_test
end
