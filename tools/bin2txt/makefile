OPT=-O3 -Wall -std=c99
SRC=bin2txt.c
3RD_PARTY = ../../3rd-party
INCLUDE=-I$(3RD_PARTY)/zlib-1.2.11
PRE_BUILT = $(3RD_PARTY)/pre-built
LIB=-lz

all: windows

windows: $(SRC)
	gcc $(OPT) $(SRC) $(INCLUDE) -L$(PRE_BUILT)/mingw $(LIB) -o bin2txt.exe

linux: $(SRC)
	gcc $(OPT) $(SRC) $(INCLUDE) $(LIB) -o bin2txt

macosx: $(SRC)
	gcc $(OPT) $(SRC) $(INCLUDE) $(LIB) -o bin2txt
