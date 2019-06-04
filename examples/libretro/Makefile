target: build
	cp -f ../../lib/tic80_libretro* .

../../3rd-party/squirrel/squirrel/sqapi.cpp:
	git submodule update --init --recursive

build: ../../3rd-party/squirrel/squirrel/sqapi.cpp
	cd ../.. && cmake . -DBUILD_SOKOL=OFF -DBUILD_SDL=OFF && make

clean:
	rm -f tic80_libretro.so
