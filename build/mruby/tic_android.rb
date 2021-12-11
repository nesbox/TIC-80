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
  ARCH = ENV['ANDROID_ARCH']
  PLATFORM = ENV['ANDROID_PLATFORM']
  API = ENV['ANDROID_PLATFORM'].rpartition('-').last

  toolchain :android, arch: ARCH, platform: PLATFORM

  conf.gembox File.expand_path('tic', File.dirname(__FILE__))

  conf.cc do |cc|
    cc.flags << '-fPIC'
    cc.defines << '__ANDROID__'
    cc.defines << "__ANDROID_API__=#{API}"
  end

  conf.enable_debug if /DEBUG/ =~ ENV["BUILD_TYPE"]
  conf.enable_bintest
  conf.enable_test
end
