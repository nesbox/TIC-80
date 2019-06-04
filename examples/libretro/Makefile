target: build
	cp -f ../../lib/tic80_libretro* .

../../3rd-party/squirrel/squirrel/sqapi.cpp:
	git submodule update --init --recursive

build: ../../3rd-party/squirrel/squirrel/sqapi.cpp
	cd ../.. && cmake . && make

clean:
	rm -f tic80_libretro.so
