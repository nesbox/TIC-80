target: build
	cp ../../lib/tic80_libretro* .

build: libretro-common
	cd ../.. && cmake . && make

libretro-common:
	git submodule update --init --recursive
