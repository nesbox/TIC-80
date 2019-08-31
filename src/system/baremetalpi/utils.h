#ifndef _utils_h
#define _utils_h

#include <tic80_types.h>
#include <stdio.h>
#include <cstring>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <cstring>
#include <cerrno>
#include <unistd.h>

//define dbg(...) printf(__VA_ARGS__)
#define dbg(...)



void* loadFile(const char *filename, u32* size);
char *strdup (const char *s);

#endif

