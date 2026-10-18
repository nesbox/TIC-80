load File.expand_path('android_toolchain.rake', File.dirname(__FILE__))

MRuby::Build.new do |conf|
  if ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
    toolchain :visualcpp
  else
    conf.toolchain
  end

  conf.build_mrbc_exec
  conf.disable_libmruby
end

MRuby::CrossBuild.new('target') do |conf|
  ARCH = ENV['ANDROID_ARCH']
  PLATFORM = ENV['ANDROID_PLATFORM']

  toolchain :tic_android, arch: ARCH, platform: PLATFORM

  conf.gembox File.expand_path('tic', File.dirname(__FILE__))

  conf.cc do |cc|
    cc.flags << '-fPIC'
  end

  conf.enable_debug if /DEBUG/ =~ ENV["BUILD_TYPE"]
  conf.enable_bintest
  conf.enable_test
end
