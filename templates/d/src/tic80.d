module tic80;

extern(C):

struct MouseData {
    short x; short y; 
    short scrollx; short scrolly;
    bool left; bool middle; bool right;
}

const int WIDTH = 240;
const int HEIGHT = 136;

int btn(int id);
bool btnp(int id, int hold, int period);
void circ(int x, int y, int radius, int color);
void circb(int x, int y, int radius, int color);
void clip(int x, int y, int w, int h);
void cls(int color);
void exit();
void elli(int x, int y, int a, int b, int color);
void ellib(int x, int y, int a, int b, int color);
bool fget(int id, ubyte flag);
int font(char* text, int x, int y, ubyte transcolors, int colorcount, int width, int height, bool fixed, int scale);
bool fset(int id, ubyte flag, bool value);
bool key(int keycode);
bool keyp(int keycode, int hold, int period);
void line(int x0, int y0, int x1, int y1, int color);
void map(int x, int y, int w, int h, int sx, int sy, ubyte transcolors, int colorcount, int scale, int remap);
void memcpy(uint copyto, uint copyfrom, uint length);
void memset(uint addr, ubyte value, uint length);
int mget(int x, int y);
void mouse(MouseData* data);
void mset(int x, int y, bool value);
void music(int track, int frame, int row, bool loop, bool sustain, int tempo, int speed);
ubyte peek(int addr, int bits);
int print(const char* txt, int x, int y, int color, int fixed, int scale, int alt);
ubyte peek4(uint addr4);
ubyte peek2(uint addr2);
ubyte peek1(uint bitaddr);
void pix(int x, int y, int color);
uint pmem(uint index, uint value);
void poke(uint addr, ubyte value, int bits);
void poke4(uint addr4, ubyte value);
void poke2(uint addr2, ubyte value);
void poke1(uint bitaddr, ubyte value);
void rect(int x, int y, int w, int h, int color);
void rectb(int x, int y, int w, int h, int color);
void reset();
void sfx(int id, int note, int octave, int duration, int channel, int volumeLeft, int volumeRight, int speed);
void spr(int id, int x, int y, uint* transcolors, uint colorcount, int scale, int flip, int rotate, int w, int h);
void sync(int mask, int bank, bool tocart);
void trace(const char* txt, int color);
void textri(float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float u3, float v3, int texsrc, ubyte transcolors, int colorcount, float z1, float z2, float z3, bool persp);
void tri(float x1, float y1, float x2, float y2, float x3, float y3, int color);
void trib(float x1, float y1, float x2, float y2, float x3, float y3, int color);
int time();
int tstamp();
int vbank(int bank);

