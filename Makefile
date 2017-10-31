CC=gcc
OPT=-O3 -Wall -std=c99
OPT_ARM=-D__ARM_LINUX__

RM= rm -f

INCLUDES= \
	-Iinclude/lua \
	-Iinclude/zlib \
	-Iinclude/gif \
	-Iinclude/sdl2 \
	-Iinclude/tic80

MINGW_LINKER_FLAGS= \
	-Llib/mingw \
	-lmingw32 \
	-lSDL2main \
	-lSDL2 \
	-lz \
	-lgif \
	-llua \
	-lcomdlg32 \
	-lws2_32 \
	-mwindows

LINUX_INCLUDES= \
	`pkg-config --cflags gtk+-3.0`

LINUX64_LIBS= \
	`pkg-config --libs gtk+-3.0` \
	-Llib/linux

LINUX32_LIBS= \
	`pkg-config --libs gtk+-3.0` \
	-Llib/linux32

LINUX_ARM_LIBS= \
	-Llib/arm

LINUX_LINKER_FLAGS= \
	-D_GNU_SOURCE \
	-lSDL2 \
	-llua \
	-ldl \
	-lm \
	-lpthread \
	-lrt \
	-lz \
	-lgif

MINGW_OUTPUT=bin/tic.exe

EMS_CC=emcc
EMS_OPT= \
	-D_GNU_SOURCE \
	-Wno-typedef-redefinition \
	-s USE_SDL=2 \
	-s TOTAL_MEMORY=67108864 \
	--llvm-lto 1 \
	--memory-init-file 0 \
	--pre-js lib/emscripten/prejs.js

EMS_LINKER_FLAGS= \
	-Llib/emscripten \
	-llua \
	-lgif \
	-lz

MACOSX_OPT= \
	-mmacosx-version-min=10.6 \
	-Wno-typedef-redefinition \
	-D_THREAD_SAFE \
	-D_GNU_SOURCE

MACOSX_LIBS= \
	-Llib/macos \
	-L/usr/local/lib \
	-lSDL2 -lm -liconv -lobjc -llua -lz -lgif \
	-Wl,-framework,CoreAudio \
	-Wl,-framework,AudioToolbox \
	-Wl,-framework,ForceFeedback \
	-Wl,-framework,CoreVideo \
	-Wl,-framework,Cocoa \
	-Wl,-framework,Carbon \
	-Wl,-framework,IOKit

SOURCES=\
	src/studio.c \
	src/console.c \
	src/run.c \
	src/ext/file_dialog.c \
	src/ext/md5.c \
	src/ext/gif.c \
	src/ext/net/SDLnet.c \
	src/ext/net/SDLnetTCP.c \
	src/ext/net/SDLnetselect.c \
	src/fs.c \
	src/tools.c \
	src/start.c \
	src/sprite.c \
	src/map.c \
	src/sfx.c \
	src/music.c \
	src/history.c \
	src/world.c \
	src/config.c \
	src/keymap.c \
	src/code.c \
	src/dialog.c \
	src/menu.c \
	src/net.c \
	src/surf.c

SOURCES_EXT= \
	src/html.c

DEMO_ASSETS= \
	bin/assets/fire.tic.dat \
	bin/assets/p3d.tic.dat \
	bin/assets/palette.tic.dat \
	bin/assets/quest.tic.dat \
	bin/assets/sfx.tic.dat \
	bin/assets/music.tic.dat \
	bin/assets/font.tic.dat \
	bin/assets/tetris.tic.dat \
	bin/assets/jsdemo.tic.dat \
	bin/assets/luademo.tic.dat \
	bin/assets/moondemo.tic.dat \
	bin/assets/config.tic.dat

all: run

TIC80_H = include/tic80/tic80_types.h include/tic80/tic80.h include/tic80/tic80_config.h src/tic.h src/ticapi.h src/machine.h

TIC_H= src/*.h \
	src/ext/*.h

bin/studio.o: src/studio.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/console.o: src/console.c $(TIC80_H) $(TIC_H) $(DEMO_ASSETS)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/run.o: src/run.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/file_dialog.o: src/ext/file_dialog.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) $(LINUX_INCLUDES) -c -o $@

bin/md5.o: src/ext/md5.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/gif.o: src/ext/gif.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/SDLnet.o: src/ext/net/SDLnet.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/SDLnetTCP.o: src/ext/net/SDLnetTCP.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/SDLnetselect.o: src/ext/net/SDLnetselect.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/fs.o: src/fs.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/tools.o: src/tools.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/start.o: src/start.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/sprite.o: src/sprite.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/map.o: src/map.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/sfx.o: src/sfx.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/music.o: src/music.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/history.o: src/history.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/world.o: src/world.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/config.o: src/config.c $(TIC80_H) $(TIC_H) $(DEMO_ASSETS)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/keymap.o: src/keymap.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/code.o: src/code.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/net.o: src/net.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/dialog.o: src/dialog.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/menu.o: src/menu.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/surf.o: src/surf.c $(TIC80_H) $(TIC_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

TIC_O=\
	bin/studio.o \
	bin/console.o \
	bin/run.o \
	bin/file_dialog.o \
	bin/md5.o \
	bin/gif.o \
	bin/SDLnet.o \
	bin/SDLnetTCP.o \
	bin/SDLnetselect.o \
	bin/fs.o \
	bin/tools.o \
	bin/start.o \
	bin/sprite.o \
	bin/map.o \
	bin/sfx.o \
	bin/music.o \
	bin/history.o \
	bin/world.o \
	bin/config.o \
	bin/keymap.o \
	bin/code.o \
	bin/net.o \
	bin/dialog.o \
	bin/menu.o \
	bin/surf.o

bin/tic80.o: src/tic80.c $(TIC80_H)
	$(CC) $< $(OPT) $(INCLUDES) -DTIC80_SHARED -c -o $@

bin/tic.o: src/tic.c $(TIC80_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/blip_buf.o: src/ext/blip_buf.c $(TIC80_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/jsapi.o: src/jsapi.c $(TIC80_H)
	$(CC) $< $(OPT) $(INCLUDES) -c -o $@

bin/luaapi.o: src/luaapi.c $(TIC80_H)
	$(CC) $< $(OPT) -fPIC $(INCLUDES) -c -o $@

bin/duktape.o: src/ext/duktape/duktape.c $(TIC80_H)
	$(CC) $< $(OPT) -fPIC $(INCLUDES) -c -o $@

TIC80_SRC = src/tic80.c src/tic.c src/ext/blip_buf.c src/jsapi.c src/luaapi.c src/ext/duktape/duktape.c
TIC80_O = bin/tic80.o bin/tic.o bin/tools.o bin/blip_buf.o bin/jsapi.o bin/luaapi.o bin/duktape.o bin/gif.o
TIC80_A = bin/libtic80.a
TIC80_DLL = bin/tic80.dll

$(TIC80_DLL): $(TIC80_O)
	$(CC) $(OPT) -shared $(TIC80_O) -Llib/mingw -llua -lgif -Wl,--out-implib,$(TIC80_A) -o $@

emscripten:
	$(EMS_CC) $(SOURCES) $(TIC80_SRC) $(SOURCES_EXT) $(OPT) $(INCLUDES) $(EMS_OPT) $(EMS_LINKER_FLAGS) -o build/html/tic.js

mingw: $(DEMO_ASSETS) $(TIC80_DLL) $(TIC_O) bin/html.o bin/res.o
	$(CC) $(TIC_O) bin/html.o bin/res.o $(TIC80_A) $(OPT) $(INCLUDES) $(MINGW_LINKER_FLAGS) -o $(MINGW_OUTPUT)

run: mingw
	$(MINGW_OUTPUT)

linux64:
	$(CC) $(LINUX_INCLUDES) $(SOURCES) $(TIC80_SRC) $(SOURCES_EXT) $(OPT) $(INCLUDES) $(LINUX64_LIBS) $(LINUX_LINKER_FLAGS) -flto -o bin/tic

linux32:
	$(CC) $(LINUX_INCLUDES) $(SOURCES) $(TIC80_SRC) $(SOURCES_EXT) $(OPT) $(INCLUDES) $(LINUX32_LIBS) $(LINUX_LINKER_FLAGS) -flto -o bin/tic

arm:
	$(CC) $(OPT_ARM) $(SOURCES) $(TIC80_SRC) $(OPT) $(INCLUDES) $(LINUX_ARM_LIBS) $(LINUX_LINKER_FLAGS) -flto -o bin/tic

macosx:
	$(CC) $(SOURCES) $(TIC80_SRC) $(SOURCES_EXT) src/ext/file_dialog.m $(OPT) $(MACOSX_OPT) $(INCLUDES) $(MACOSX_LIBS) -o bin/tic

bin/res.o: lib/mingw/res.rc lib/mingw/icon.ico
	windres $< $@

BIN2TXT= tools/bin2txt/bin2txt

bin/html.o: src/html.c build/html/index.html build/html/tic.js
	$(BIN2TXT) build/html/index.html bin/assets/index.html.dat -z
	$(BIN2TXT) build/html/tic.js bin/assets/tic.js.dat -z
	$(CC) -c src/html.c $(OPT) $(INCLUDES) -o $@

bin/assets/config.tic.dat: config.tic
	$(BIN2TXT) $< $@ -z

bin/assets/fire.tic.dat: demos/fire.tic
	$(BIN2TXT) $< $@ -z

bin/assets/p3d.tic.dat: demos/p3d.tic
	$(BIN2TXT) $< $@ -z

bin/assets/palette.tic.dat: demos/palette.tic
	$(BIN2TXT) $< $@ -z

bin/assets/quest.tic.dat: demos/quest.tic
	$(BIN2TXT) $< $@ -z

bin/assets/sfx.tic.dat: demos/sfx.tic
	$(BIN2TXT) $< $@ -z

bin/assets/font.tic.dat: demos/font.tic
	$(BIN2TXT) $< $@ -z

bin/assets/music.tic.dat: demos/music.tic
	$(BIN2TXT) $< $@ -z

bin/assets/tetris.tic.dat: demos/tetris.tic
	$(BIN2TXT) $< $@ -z

bin/assets/jsdemo.tic.dat: demos/jsdemo.tic
	$(BIN2TXT) $< $@ -z

bin/assets/luademo.tic.dat: demos/luademo.tic
	$(BIN2TXT) $< $@ -z

bin/assets/moondemo.tic.dat: demos/moondemo.tic
	$(BIN2TXT) $< $@ -z

clean: $(TIC_O) $(TIC80_O)
	$(RM) $(TIC_O) $(TIC80_O)
