import std.algorithm: min, max;
import tic80;

extern(C):

// From WASI libc:
int  snprintf(scope char* s, size_t n, scope const char* format, scope const ...);

int t, x, y;
const char* m = "HELLO WORLD FROM D!";
int r = 0;
MouseData md;

void BOOT() {
  t = 1;
  x = 96;
  y = 24;
}

void TIC() {
  cls(13);

  // The standard demo.
  if (btn(0) > 0) { y--; }
  if (btn(1) > 0) { y++; }
  if (btn(2) > 0) { x--; }
  if (btn(3) > 0) { x++; }

  spr(1+t%60/30*2, x, y, null, 0, 3, 0, 0, 2, 2);
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

  const BUFSIZ = 10;
  char[BUFSIZ] buf;
  char* bufptr = cast(char*)buf;
  snprintf(bufptr, BUFSIZ, "(%03d,%03d)", md.x, md.y);
  print(bufptr, 3, 3, 15, 0, 1, 1);
}
