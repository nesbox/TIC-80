OPT=-O3 -Wall -std=c99
SRC=bin2txt.c
INCLUDE=-I../../include/zlib
LIB=-lz

all: windows

windows: $(SRC)
	gcc $(OPT) $(SRC) $(INCLUDE) -L../../lib/mingw $(LIB) -o bin2txt.exe

linux32: $(SRC)
	gcc $(OPT) $(SRC) $(INCLUDE) -L../../lib/linux32 $(LIB) -o bin2txt

linux64: $(SRC)
	gcc $(OPT) $(SRC) $(INCLUDE) -L../../lib/linux $(LIB) -o bin2txt

macosx: $(SRC)
	gcc $(OPT) $(SRC) $(INCLUDE) -L../../lib/macos $(LIB) -o bin2txt
