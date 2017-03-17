class IntervalCheckFormula < Formula
  homepage "https://github.com/AdamSimpson/IntervalCheck.git"
  url "none"

  concern for_version("dev") do
    included do
      url "none"

      module_commands do
        commands = [ "unload PrgEnv-gnu PrgEnv-pgi PrgEnv-cray PrgEnv-intel" ]

        commands << "load PrgEnv-gnu" if build_name =~ /gnu/
        commands << "swap gcc gcc/#{$1}" if build_name =~ /gnu([\d\.]+)/

        commands << "load cudatoolkit"
        commands << "load dynamic-link"
        commands << "load cmake3"
        commands << "load git"

        commands
      end

      def install
        module_list

	Dir.chdir "#{prefix}"
        system "rm -rf source"
        system "git clone https://github.com/AdamSimpson/IntervalCheck.git source"

	Dir.chdir "#{prefix}/source"

        system "rm -rf build; mkdir build"
	Dir.chdir "build"
        system "cmake -DCMAKE_INSTALL_PREFIX=#{prefix} .."
        system "make"
        system "make install"
      end
    end
  end

  modulefile <<-MODULEFILE.strip_heredoc
    #%Module
    proc ModulesHelp { } {
       puts stderr "<%= @package.name %> <%= @package.version %>"
       puts stderr ""
    }
    # One line description
    module-whatis "<%= @package.name %> <%= @package.version %>"

    set PREFIX <%= @package.prefix %>

    prepend-path LD_LIBRARY_PATH $PREFIX/lib
    prepend-path LD_LIBRARY_PATH $PREFIX/plugins/lib
    setenv IC_PRELOAD $PREFIX/lib/libIntervalCheck.so


    prepend-path PATH $PREFIX/bin
  MODULEFILE
end
