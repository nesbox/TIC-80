CMAKE ?= cmake

# Build the libretro core.
target: build
	cp -f ../../lib/tic80_libretro* .

# submodules
../../3rd-party/squirrel/squirrel/sqapi.cpp:
	git submodule update --init --recursive

# cmake
../../Makefile: ../../3rd-party/squirrel/squirrel/sqapi.cpp
	cd ../.. && $(CMAKE) . -DBUILD_SOKOL=OFF -DBUILD_SDL=OFF

# make
build: ../../Makefile
	$(MAKE) -C ../..

clean:
	rm -f *.so
	rm -f *.dll
