// based on WASM-4 template: https://wasm4.org/docs
// & zig tic-80 template

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define WASM_EXPORT(name) __attribute__((export_name(name)))
#define WASM_IMPORT(name) __attribute__((import_name(name)))

// ┌───────────────────────────────────────────────────────────────────────────┐
// │                                                                           │
// │ Callbacks                                                                 │
// │                                                                           │
// └───────────────────────────────────────────────────────────────────────────┘

WASM_EXPORT("TIC") void TIC ();
WASM_EXPORT("BDR") void BDR (uint32_t row);
WASM_EXPORT("SCN") void SCN (uint32_t row);
WASM_EXPORT("OVR") void OVR ();

// ┌───────────────────────────────────────────────────────────────────────────┐
// │                                                                           │
// │ Platform Constants                                                        │
// │                                                                           │
// └───────────────────────────────────────────────────────────────────────────┘

#define WIDTH 240
#define HEIGHT 136

// ┌───────────────────────────────────────────────────────────────────────────┐
// │                                                                           │
// │ Memory Addresses                                                          │
// │                                                                           │
// └───────────────────────────────────────────────────────────────────────────┘

#define FRAMEBUFFER        ((uint8_t*) 0)
#define TILES              ((uint8_t*) 0x4000)
#define SPRITES            ((uint8_t*) 0x6000)
#define MAP                ((uint8_t*) 0x8000)
#define GAMEPADS           ((uint8_t*) 0xFF80)
#define MOUSE              ((uint8_t*) 0xFF84)
#define KEYBOARD           ((uint8_t*) 0xFF88)
#define SFX_STATE          ((uint8_t*) 0xFF8C)
#define SOUND_REGISTERS    ((uint8_t*) 0xFF9C)
#define WAVEFORMS          ((uint8_t*) 0xFFE4)
#define SFX                ((uint8_t*) 0x100E4)
#define MUSIC_PATTERNS     ((uint8_t*) 0x11164)
#define MUSIC_TRACKS       ((uint8_t*) 0x13E64)
#define SOUND_STATE        ((uint8_t*) 0x13FFC)
#define STEREO_VOLUME      ((uint8_t*) 0x14000)
#define PERSISTENT_RAM     ((uint8_t*) 0x14004)
#define PERSISTENT_RAM_u32 ((uint8_t*) 0x14004)
#define SPRITE_FLAGS       ((uint8_t*) 0x14404)
#define SYSTEM_FONT        ((uint8_t*) 0x14604)


#define PALETTE            ((uint8_t*) 0x3FC0)
#define PALETTE_MAP        ((uint8_t*) 0x3FF0)
#define BORDER_COLOR       ((uint8_t*) 0x3FF8)
#define SCREEN_OFFSET      ((uint8_t*) 0x3FF9)
#define MOUSE_CURSOR       ((uint8_t*) 0x3FFB)
#define BLIT_SEGMENT       ((uint8_t*) 0x3FFC)

// vbank 1
#define OVR_TRANSPARENCY   ((uint8_t*) 0x3FF8)



// ┌───────────────────────────────────────────────────────────────────────────┐
// │                                                                           │
// │ API                                                                       │
// │                                                                           │
// └───────────────────────────────────────────────────────────────────────────┘

// INPUT

typedef struct {
  int16_t x;
  int16_t y;
  int8_t scrollx;
  int8_t scrolly;
  bool left;
  bool middle;
  bool right;
} MouseData;

WASM_IMPORT("btn")
uint32_t btn(int32_t id);

WASM_IMPORT("btnp")
uint32_t btnp(int32_t id , int32_t hold, int32_t period);

WASM_IMPORT("key")
bool key(int32_t keycode);

WASM_IMPORT("keyp")
bool keyp(int32_t keycode, int32_t hold, int32_t period);

WASM_IMPORT("mouse")
void mouse(MouseData* data);


// DRAW / DRAW UTILS

WASM_IMPORT("clip")
void clip(int32_t x, int32_t y, int32_t w, int32_t h);

WASM_IMPORT("cls")
void cls(uint8_t color);

WASM_IMPORT("circ")
void circ(int32_t x, int32_t y, int32_t radius, uint8_t color);

WASM_IMPORT("circb")
void circb(int32_t x, int32_t y, int32_t radius, uint8_t color);

WASM_IMPORT("elli")
void elli(int32_t x, int32_t y, int32_t a, int32_t b, uint8_t color);

WASM_IMPORT("ellib")
void ellib(int32_t x, int32_t y, int32_t a, int32_t b,  uint8_t color);

WASM_IMPORT("fget")
bool fget(int32_t id, uint8_t flag);

WASM_IMPORT("fset")
void fset(int32_t id, uint8_t flag, bool value);

WASM_IMPORT("line")
void line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color);

WASM_IMPORT("map")
void map(int32_t x, int32_t y, int32_t w, int32_t h, int32_t sx, int32_t sy,
	  uint8_t* trans_colors, uint8_t colorCount,  int32_t scale,  int32_t remap);
// todo: why is remap a uint32_t?

WASM_IMPORT("mget")
uint8_t mget(int32_t x, int32_t y);

WASM_IMPORT("mset")
void mset(int32_t x, int32_t y, uint8_t value);

WASM_IMPORT("pix")
void pix(int32_t x, int32_t y, int8_t color);
// TODO: BROKEN if color == -1 this should return color

WASM_IMPORT("rect")
void rect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t color);

WASM_IMPORT("rectb")
void rectb(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t color);

WASM_IMPORT("spr")
void spr(int32_t id, int32_t x, int32_t y, uint8_t* trans_colors, int32_t colorCount,
	 int32_t scale, int32_t flip, int32_t rotate, int32_t w, int32_t h);

WASM_IMPORT("tri")
void tri(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
	 int32_t x3, int32_t y3, uint8_t color);

WASM_IMPORT("trib")
void trib(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
	  int32_t x3, int32_t y3, uint8_t color);

WASM_IMPORT("textri")
void textri(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3,
	    int32_t u1, int32_t v1, int32_t u2, int32_t v2, int32_t u3, int32_t v3,
	    bool use_map, uint8_t* trans_colors, int8_t colorCount);


// ----
// TEXT

// print text [x=0 y=0] [color=15] [fixed=false] [scale=1] [smallfont=false] -> text width
WASM_IMPORT("print")
int32_t print(const char *text, int32_t x, int32_t y,
	      int32_t color, bool fixed, int32_t scale, int32_t smallfont);
// font text, x, y, [transcolor], [char width], [char height], [fixed=false], [scale=1] -> text width
WASM_IMPORT("font")
int32_t font(const char *text, uint32_t x, int32_t y, int32_t transcolor,
	 int32_t char_width, int32_t char_height, bool fixed, int32_t scale);

// -----
// AUDIO

WASM_IMPORT("music")
void music(int32_t track, int32_t frame, int32_t row,
	   bool loop, bool sustain, int32_t tempo, int32_t speed);
// sfx id [note] [duration=-1] [channel=0]
//        [volumeleft=15] [volumeright=15] [speed=0]
WASM_IMPORT("sfx")
void sfx(int32_t id, int32_t note, int32_t duration, int32_t channel,
	 int32_t volume_left, int32_t volume_right, int32_t speed);


// ------
// MEMORY

// pmem index -1 -> val
// pmem index val -> val
WASM_IMPORT("pmem")
uint32_t pmem(int32_t index, uint32_t value);
WASM_IMPORT("memcpy")
void tic_memcpy(uint32_t to, uint32_t from, uint32_t length);
WASM_IMPORT("memset")
void tic_memset(uint32_t addr, uint8_t value, uint32_t length);

WASM_IMPORT("poke")
void poke(int32_t addr, uint8_t value, int32_t bits);
WASM_IMPORT("poke4")
void poke4(int32_t addr4, uint8_t value);
WASM_IMPORT("poke2")
void poke2(int32_t addr2, uint8_t value);
WASM_IMPORT("poke1")
void poke1(int32_t bitaddr, uint8_t value);

WASM_IMPORT("peek")
uint8_t peek(int32_t addr, int32_t bits);
WASM_IMPORT("peek4")
uint8_t peek4(int32_t addr4);
WASM_IMPORT("peek2")
uint8_t peek2(int32_t addr2);
WASM_IMPORT("peek1")
uint8_t peek1(int32_t bitaddr);

WASM_IMPORT("vbank")
int32_t vbank(int32_t bank);


// -----
// UTILS

// time -> ticks (ms) elapsed since game start
WASM_IMPORT("time")
float time();

WASM_IMPORT("tstamp")
uint64_t tstamp();

WASM_IMPORT("sync")
void sync(int32_t mask, int8_t bank, int8_t tocart);
// TODO should tocart be bool?

WASM_IMPORT("trace")
void trace(const char *text, int32_t color);

// ------
// SYSTEM
WASM_IMPORT("exit")
void exit();

WASM_IMPORT("reset")
void reset();

/** Prints a message to the debug console. */
//WASM_IMPORT("trace") void trace (const char* str);

/** Prints a message to the debug console. */
//__attribute__((__format__ (__printf__, 1, 2)))
//WASM_IMPORT("tracef") void tracef (const char* fmt, ...);
