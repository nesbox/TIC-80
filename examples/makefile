CC=gcc
OPT=-O3 -Wall -std=c99
INCLUDES= \
	-I../include \
	-I../include/sdl2 \
	-I../include/tic80

LINKER_FLAGS= \
	-L../lib/mingw \
	-L../bin \
	-ltic80 \
	-lmingw32 -lSDL2main -lSDL2.dll \
	-mwindows

SOURCES=\
	sdl-renderer.c

SDL_RENDERER = ../bin/sdl-renderer.exe

all: $(SDL_RENDERER)

$(SDL_RENDERER): $(SOURCES)
	$(CC) $(INCLUDES) $< $(OPT) $(LINKER_FLAGS) -o $@
