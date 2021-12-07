MRuby::Build.new do |conf|
  # load specific toolchain settings
  conf.toolchain

  # include the GEM box
  conf.gembox File.expand_path('tic', File.dirname(__FILE__))

  # C compiler settings
  conf.cc do |cc|
    cc.flags << '-fPIC' unless ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']

    case ENV['BUILD_CONFIG'].upcase
    when /DEBUG/
      unless ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
        cc.flags << '-g'
      end
    else
      if ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
        cc.flags << '/O1'
      else
        cc.flags << '-Os'
      end
    end
  end

  # mrbc settings
  # conf.mrbc do |mrbc|
  #   mrbc.compile_options = "-g -B%{funcname} -o-" # The -g option is required for line numbers
  # end

  # Linker settings
  # conf.linker do |linker|
  #   linker.command = ENV['LD'] || 'gcc'
  #   linker.flags = [ENV['LDFLAGS'] || []]
  #   linker.flags_before_libraries = []
  #   linker.libraries = %w()
  #   linker.flags_after_libraries = []
  #   linker.library_paths = []
  #   linker.option_library = '-l%s'
  #   linker.option_library_path = '-L%s'
  #   linker.link_options = "%{flags} -o "%{outfile}" %{objs} %{libs}"
  # end

  # Archiver settings
  # conf.archiver do |archiver|
  #   archiver.command = ENV['AR'] || 'ar'
  #   archiver.archive_options = 'rs "%{outfile}" %{objs}'
  # end

  # Parser generator settings
  # conf.yacc do |yacc|
  #   yacc.command = ENV['YACC'] || 'bison'
  #   yacc.compile_options = %q[-o "%{outfile}" "%{infile}"]
  # end

  # gperf settings
  # conf.gperf do |gperf|
  #   gperf.command = 'gperf'
  #   gperf.compile_options = %q[-L ANSI-C -C -p -j1 -i 1 -g -o -t -N mrb_reserved_word -k"1,3,$" "%{infile}" > "%{outfile}"]
  # end

  # file extensions
  conf.exts do |exts|
    exts.object = '.o'
    exts.library = '.a'
    # exts.executable = '' # '.exe' if Windows
  end

  # file separator
  # conf.file_separator = '/'

  # Turn on `enable_debug` for better debugging
  conf.enable_debug if /DEBUG/ =~ ENV['BUILD_CONFIG'].upcase
  conf.enable_bintest
  conf.enable_test
end
