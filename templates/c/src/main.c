#include "tic80.h"

#define max(a, b) (a > b) ? a : b
#define min(a, b) (a < b) ? a : b

// From WASI libc:
WASM_IMPORT("snprintf")
int snprintf(char* s, size_t n, const char* format, ...);

int t, x, y;
const char* m = "HELLO WORLD FROM C!";
int r = 0;
Mouse md;
uint8_t transcolors = { 14 };

WASM_EXPORT("BOOT")
void BOOT() {
    t = 1;
    x = 96;
    y = 24;
}

WASM_EXPORT("TIC")
void TIC() {
    cls(13);

    // The standard demo.
    if (btn(0) > 0) { y--; }
    if (btn(1) > 0) { y++; }
    if (btn(2) > 0) { x--; }
    if (btn(3) > 0) { x++; }

    spr(1+t%60/30*2, x, y, &transcolors, 1, 3, 0, 0, 2, 2);
    print(m, 60, 84, 15, 1, 1, 0);
    t++;

    // Mouse example demonstrating use of libc function.
    mouse(&md);
    if (md.left) { r = r + 2; }
    r--;
    r = max(0, min(32, r));
    line(md.x, 0, md.x, 136, 11);
    line(0, md.y, 240, md.y, 11);
    circ(md.x, md.y, r, 11);

    const int BUFSIZ = 10;
    char buf[BUFSIZ];
    snprintf(buf, BUFSIZ, "(%03d,%03d)", md.x, md.y);
    print(buf, 3, 3, 15, 0, 1, 1);
}