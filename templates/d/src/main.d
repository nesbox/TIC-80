import tic80;

extern(C):

int t, x, y;
const char* m = "HELLO WORLD FROM D!";

void BOOT() {
  t = 1;
  x = 96;
  y = 24;
}

void TIC() {
  if (btn(0) > 0) { y--; }
  if (btn(1) > 0) { y++; }
  if (btn(2) > 0) { x--; }
  if (btn(3) > 0) { x++; }

  cls(13);
  spr(1+t%60/30*2, x, y, null, 0, 3, 0, 0, 2, 2);
  print(m, 60, 84, 15, 1, 1, 0);
  t++;
}
