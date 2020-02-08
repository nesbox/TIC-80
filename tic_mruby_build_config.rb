MRuby::Build.new do |conf|
  if ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
    toolchain :visualcpp
  else
    toolchain :gcc

    conf.cc.flags << "-fPIC"
  end

  conf.gembox 'default'

  conf.exts do |exts|
    exts.object = '.o'
    exts.executable = ''
    exts.library = '.a'
  end
end

MRuby::Build.new('host-debug') do |conf|
  if ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
    toolchain :visualcpp
  else
    toolchain :gcc

    conf.cc.flags << "-fPIC"
  end

  enable_debug

  conf.gembox 'default'
  conf.cc.defines = %w(MRB_ENABLE_DEBUG_HOOK)

  conf.gem :core => "mruby-bin-debugger"
end
