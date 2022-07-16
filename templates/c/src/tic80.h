#pragma once

#include <stdbool.h>
#include <stdint.h>

#define WASM_EXPORT(name) __attribute__((export_name(name)))
#define WASM_IMPORT(name) __attribute__((import_name(name)))

enum KEYCODES {
    KEY_NULL,
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_MINUS,
	KEY_EQUALS,
	KEY_LEFTBRACKET,
	KEY_RIGHTBRACKET,
	KEY_BACKSLASH,
	KEY_SEMICOLON,
	KEY_APOSTROPHE,
	KEY_GRAVE,
	KEY_COMMA,
	KEY_PERIOD,
	KEY_SLASH,
	KEY_SPACE,
	KEY_TAB,
	KEY_RETURN,
	KEY_BACKSPACE,
	KEY_DELETE,
	KEY_INSERT,
	KEY_PAGEUP,
	KEY_PAGEDOWN,
	KEY_HOME,
	KEY_END,
	KEY_UP,
	KEY_DOWN,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_CAPSLOCK,
	KEY_CTRL,
	KEY_SHIFT,
	KEY_ALT
};

enum BUTTON_CODES
{
    BUTTON_CODE_P1_UP,
    BUTTON_CODE_P1_DOWN,
    BUTTON_CODE_P1_LEFT,
    BUTTON_CODE_P1_RIGHT,
    BUTTON_CODE_P1_A,
    BUTTON_CODE_P1_B,
    BUTTON_CODE_P1_X,
    BUTTON_CODE_P1_Y,
    BUTTON_CODE_P2_UP,
    BUTTON_CODE_P2_DOWN,
    BUTTON_CODE_P2_LEFT,
    BUTTON_CODE_P2_RIGHT,
    BUTTON_CODE_P2_A,
    BUTTON_CODE_P2_B,
    BUTTON_CODE_P2_X,
    BUTTON_CODE_P2_Y,
    BUTTON_CODE_P3_UP,
    BUTTON_CODE_P3_DOWN,
    BUTTON_CODE_P3_LEFT,
    BUTTON_CODE_P3_RIGHT,
    BUTTON_CODE_P3_A,
    BUTTON_CODE_P3_B,
    BUTTON_CODE_P3_X,
    BUTTON_CODE_P3_Y,
    BUTTON_CODE_P4_UP,
    BUTTON_CODE_P4_DOWN,
    BUTTON_CODE_P4_LEFT,
    BUTTON_CODE_P4_RIGHT,
    BUTTON_CODE_P4_A,
    BUTTON_CODE_P4_B,
    BUTTON_CODE_P4_X,
    BUTTON_CODE_P4_Y
};

typedef struct {
    uint8_t SCREEN[16320];
    uint8_t PALETTE[48];
    uint8_t PALETTE_MAP[8];
    uint8_t BORDER_COLOR;
    uint8_t SCREEN_OFFSET_X;
    uint8_t SCREEN_OFFSET_Y;
    uint8_t MOUSE_CURSOR;
    uint8_t BLIT_SEGMENT;
    uint8_t RESERVED[3];
} VRAM;

typedef struct {
    short x; short y;
    char scrollx; char scrolly;
    bool left; bool middle; bool right;
} MouseData;

#define TILE_SIZE 8
#define WIDTH 240
#define HEIGHT 136

// Constants.
const uint32_t TILES_SIZE = 8192;
const uint32_t SPRITES_SIZE = 8192;
const uint32_t MAP_SIZE = 32640;
const uint32_t GAMEPADS_SIZE = 4;
const uint32_t MOUSE_SIZE = 4;
const uint32_t KEYBOARD_SIZE = 4;
const uint32_t SFX_STATE_SIZE = 16;
const uint32_t SOUND_REGISTERS_SIZE = 72;
const uint32_t WAVEFORMS_SIZE = 256;
const uint32_t SFX_SIZE = 4224;
const uint32_t MUSIC_PATTERNS_SIZE = 11520;
const uint32_t MUSIC_TRACKS_SIZE = 408;
const uint32_t SOUND_STATE_SIZE = 4;
const uint32_t STEREO_VOLUME_SIZE = 4;
const uint32_t PERSISTENT_MEMORY_SIZE = 1024;
const uint32_t SPRITE_FLAGS_SIZE = 512;
const uint32_t SYSTEM_FONT_SIZE = 2048;
const uint32_t WASM_FREE_RAM_SIZE = 163840; // 160kb

// Pointers.
VRAM* FRAMEBUFFER = (VRAM*)0;
uint8_t* TILES = (uint8_t*)0x04000;
uint8_t* SPRITES = (uint8_t*)0x06000;
uint8_t* MAP = (uint8_t*)0x08000;
uint8_t* GAMEPADS = (uint8_t*)0x0FF80;
uint8_t* MOUSE = (uint8_t*)0x0FF84;
uint8_t* KEYBOARD = (uint8_t*)0x0FF88;
uint8_t* SFX_STATE = (uint8_t*)0x0FF8C;
uint8_t* SOUND_REGISTERS = (uint8_t*)0x0FF9C;
uint8_t* WAVEFORMS = (uint8_t*)0x0FFE4;
uint8_t* SFX = (uint8_t*)0x100E4;
uint8_t* MUSIC_PATTERNS = (uint8_t*)0x11164;
uint8_t* MUSIC_TRACKS = (uint8_t*)0x13E64;
uint8_t* SOUND_STATE = (uint8_t*)0x13FFC;
uint8_t* STEREO_VOLUME = (uint8_t*)0x14000;
uint8_t* PERSISTENT_MEMORY = (uint8_t*)0x14004;
uint8_t* SPRITE_FLAGS = (uint8_t*)0x14404;
uint8_t* SYSTEM_FONT = (uint8_t*)0x14604;
uint8_t* WASM_FREE_RAM = (uint8_t*)0x18000; // 160kb

// Functions.
WASM_IMPORT("btn")
int32_t btn(int32_t id);

WASM_IMPORT("btnp")
bool btnp(int32_t id, int32_t hold, int32_t period);

WASM_IMPORT("circ")
void circ(int32_t x, int32_t y, int32_t radius, int32_t color);

WASM_IMPORT("circb")
void circb(int32_t x, int32_t y, int32_t radius, int32_t color);

WASM_IMPORT("clip")
void clip(int32_t x, int32_t y, int32_t w, int32_t h);

WASM_IMPORT("cls")
void cls(int32_t color);

WASM_IMPORT("exit")
void exit();

WASM_IMPORT("elli")
void elli(int32_t x, int32_t y, int32_t a, int32_t b, int32_t color);

WASM_IMPORT("ellib")
void ellib(int32_t x, int32_t y, int32_t a, int32_t b, int32_t color);

WASM_IMPORT("fget")
bool fget(int32_t id, uint8_t flag);

WASM_IMPORT("font")
int32_t font(const char* text, int32_t x, int32_t y, uint32_t* transcolors, int32_t colorcount, int32_t width, int32_t height, bool fixed, int32_t scale, bool alt);

WASM_IMPORT("fset")
bool fset(int32_t id, uint8_t flag, bool value);

WASM_IMPORT("key")
bool key(int32_t keycode);

WASM_IMPORT("keyp")
bool keyp(int32_t keycode, int32_t hold, int32_t period);

WASM_IMPORT("line")
void line(float x0, float y0, float x1, float y1, int8_t color);

WASM_IMPORT("map")
void map(int32_t x, int32_t y, int32_t w, int32_t h, int32_t sx, int32_t sy, uint32_t* transcolors, int32_t colorcount, int32_t scale, int32_t remap);

WASM_IMPORT("mget")
int32_t mget(int32_t x, int32_t y);

WASM_IMPORT("mouse")
void mouse(MouseData* data);

WASM_IMPORT("mset")
void mset(int32_t x, int32_t y, int32_t value);

WASM_IMPORT("music")
void music(int32_t track, int32_t frame, int32_t row, bool loop, bool sustain, int32_t tempo, int32_t speed);

WASM_IMPORT("peek")
uint8_t peek(int32_t addr, int32_t bits);

WASM_IMPORT("peek4")
uint8_t peek4(uint32_t addr4);

WASM_IMPORT("peek2")
uint8_t peek2(uint32_t addr2);

WASM_IMPORT("peek1")
uint8_t peek1(uint32_t bitaddr);

WASM_IMPORT("pix")
void pix(int32_t x, int32_t y, int32_t color);

WASM_IMPORT("pmem")
uint32_t pmem(uint32_t index, uint32_t value);

WASM_IMPORT("poke")
void poke(int32_t addr, int8_t value, int8_t bits);

WASM_IMPORT("poke4")
void poke4(int32_t addr4, int8_t value);

WASM_IMPORT("poke2")
void poke2(int32_t addr2, int8_t value);

WASM_IMPORT("poke1")
void poke1(int32_t bitaddr, int8_t value);

WASM_IMPORT("print")
int32_t print(const char* txt, int32_t x, int32_t y, int32_t color, int32_t fixed, int32_t scale, int32_t alt);

WASM_IMPORT("rect")
void rect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t color);

WASM_IMPORT("rectb")
void rectb(int32_t x, int32_t y, int32_t w, int32_t h, int32_t color);

WASM_IMPORT("reset")
void reset();

WASM_IMPORT("sfx")
void sfx(int32_t id, int32_t note, int32_t octave, int32_t duration, int32_t channel, int32_t volumeLeft, int32_t volumeRight, int32_t speed);

WASM_IMPORT("spr")
void spr(int32_t id, int32_t x, int32_t y, uint8_t* transcolors, uint32_t colorcount, int32_t scale, int32_t flip, int32_t rotate, int32_t w, int32_t h);

WASM_IMPORT("sync")
void sync(int32_t mask, int32_t bank, bool tocart);

WASM_IMPORT("trace")
void trace(const char* txt, int32_t color);

WASM_IMPORT("ttri")
void ttri(float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float u3, float v3, int32_t texsrc, uint32_t* transcolors, int32_t colorcount, float z1, float z2, float z3, bool persp);

WASM_IMPORT("tri")
void tri(float x1, float y1, float x2, float y2, float x3, float y3, int32_t color);

WASM_IMPORT("trib")
void trib(float x1, float y1, float x2, float y2, float x3, float y3, int32_t color);

WASM_IMPORT("time")
float time();

WASM_IMPORT("tstamp")
int32_t tstamp();

WASM_IMPORT("vbank")
int32_t vbank(int32_t bank);