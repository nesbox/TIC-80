// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "retro_endianness.h"
#include "tic.h"
#include "time.h"

// convenience macros to loop languages
#define FOR_EACH_LANG(ln) for (tic_script_config** conf = Languages ; *conf != NULL; conf++ ) { tic_script_config* ln = *conf;
#define FOR_EACH_LANG_END }


typedef struct { u8 index; tic_flip flip; tic_rotate rotate; } RemapResult;
typedef void(*RemapFunc)(void*, s32 x, s32 y, RemapResult* result);

typedef void(*TraceOutput)(void*, const char*, u8 color);
typedef void(*ErrorOutput)(void*, const char*);
typedef void(*ExitCallback)(void*);

typedef struct
{
    TraceOutput trace;
    ErrorOutput error;
    ExitCallback exit;
    
    u64 (*counter)(void*);
    u64 (*freq)(void*);
    u64 start;

    void* data;
} tic_tick_data;

typedef struct tic_mem tic_mem;
typedef void(*tic_tick)(tic_mem* memory);
typedef void(*tic_boot)(tic_mem* memory);
typedef void(*tic_scanline)(tic_mem* memory, s32 row, void* data);
typedef void(*tic_border)(tic_mem* memory, s32 row, void* data);
typedef void(*tic_gamemenu)(tic_mem* memory, s32 index, void* data);

typedef struct
{
    const char* pos;
    s32 size;
} tic_outline_item;

typedef struct
{
    tic_scanline    scanline;
    tic_border      border;
    tic_gamemenu    menu;
    void* data;
} tic_blit_callback;

typedef struct
{
    u8 id;
    const char* name;
    const char* fileExtension;
    const char* projectComment;
    struct
    {
        bool(*init)(tic_mem* memory, const char* code);
        void(*close)(tic_mem* memory);

        tic_tick tick;
        tic_boot boot;
        tic_blit_callback callback;
    };

    const tic_outline_item* (*getOutline)(const char* code, s32* size);
    void (*eval)(tic_mem* tic, const char* code);

    const char* blockCommentStart;
    const char* blockCommentEnd;
    const char* blockCommentStart2;
    const char* blockCommentEnd2;
    const char* blockStringStart;
    const char* blockStringEnd;
    const char* singleComment;
    const char* blockEnd;

    const char* const * keywords;
    s32 keywordsCount;
} tic_script_config;

extern tic_script_config* Languages[];

typedef enum
{
    tic_tiles_texture,
    tic_map_texture,
    tic_vbank_texture,
} tic_texture_src;

typedef struct
{
    s32 x, y;
} tic_point;

typedef struct
{
    s32 x, y, w, h;
} tic_rect;

//                  SYNC DEFINITION TABLE
//       .--------------------------------- - - - 
//       | CART    | RAM           | INDEX
//       |---------+---------------+------- - - - 
//       |         |               |
#define TIC_SYNC_LIST(macro) \
    macro(tiles,    tiles,        0) \
    macro(sprites,  sprites,      1) \
    macro(map,      map,          2) \
    macro(sfx,      sfx,          3) \
    macro(music,    music,        4) \
    macro(palette,  vram.palette, 5) \
    macro(flags,    flags,        6) \
    macro(screen,   vram.screen,  7)

enum
{
#define TIC_SYNC_DEF(NAME, _, INDEX) tic_sync_##NAME = 1 << INDEX,
    TIC_SYNC_LIST(TIC_SYNC_DEF)
#undef TIC_SYNC_DEF
};

#define TIC_FN  "TIC"
#define BOOT_FN "BOOT"
#define SCN_FN  "SCN"
#define OVR_FN  "OVR" // deprecated since v1.0
#define BDR_FN  "BDR"
#define MENU_FN "MENU"

#define TIC_CALLBACK_LIST(macro)                                                                                    \
    macro(TIC, TIC_FN "()", "Main function. It's called at " DEF2STR(TIC80_FRAMERATE)                               \
        "fps (" DEF2STR(TIC80_FRAMERATE) " times every second).")                                                   \
    macro(BOOT, BOOT_FN, "Startup function.")                                                                       \
    macro(MENU, MENU_FN "(index)", "Game Menu handler.")                                                            \
    macro(SCN, SCN_FN "(row)", "Allows you to execute code between the drawing of each scanline, "                  \
        "for example, to manipulate the palette.")                                                                  \
    macro(BDR, BDR_FN "(row)", "Allows you to execute code between the drawing of each fullscreen scanline, "       \
        "for example, to manipulate the palette.")

// API DEFINITION TABLE
//  macro
//  (
//      definition
//      help
//      parameters count
//      required parameters count
//      callback?
//      return type
//      function parameters
//  )

#define TIC_API_LIST(macro)                                                                                             \
    macro(print,                                                                                                        \
        "print(text x=0 y=0 color=15 fixed=false scale=1 smallfont=false) -> width",                                    \
                                                                                                                        \
        "This will simply print text to the screen using the font defined in config.\n"                                 \
        "When set to true, the fixed width option ensures that each character "                                         \
        "will be printed in a `box` of the same size, "                                                                 \
        "so the character `i` will occupy the same width as the character `w` for example.\n"                           \
        "When fixed width is false, there will be a single space between each character.\n"                             \
        "\nTips:\n"                                                                                                     \
        "- To use a custom rastered font, check out `font()`.\n"                                                        \
        "- To print to the console, check out `trace()`.",                                                              \
        7,                                                                                                              \
        1,                                                                                                              \
        0,                                                                                                              \
        s32,                                                                                                            \
        tic_mem*, const char* text, s32 x, s32 y, u8 color, bool fixed, s32 scale, bool alt)                            \
                                                                                                                        \
                                                                                                                        \
    macro(cls,                                                                                                          \
        "cls(color=0)",                                                                                                 \
                                                                                                                        \
        "Clear the screen.\n"                                                                                           \
        "When called this function clear all the screen using the color passed as argument.\n"                          \
        "If no parameter is passed first color (0) is used.",                                                           \
        1,                                                                                                              \
        0,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, u8 color)                                                                                             \
                                                                                                                        \
                                                                                                                        \
    macro(pix,                                                                                                          \
        "pix(x y color)\npix(x y) -> color",                                                                            \
                                                                                                                        \
        "This function can read or write pixel color values.\n"                                                         \
        "When called with a color parameter, the pixel at the specified coordinates is set to that color.\n"            \
        "Calling the function without a color parameter returns the color of the pixel at the specified position.",     \
        3,                                                                                                              \
        2,                                                                                                              \
        0,                                                                                                              \
        u8,                                                                                                             \
        tic_mem*, s32 x, s32 y, u8 color, bool get)                                                                     \
                                                                                                                        \
                                                                                                                        \
    macro(line,                                                                                                         \
        "line(x0 y0 x1 y1 color)",                                                                                      \
                                                                                                                        \
        "Draws a straight line from point (x0,y0) to point (x1,y1) in the specified color.",                            \
        5,                                                                                                              \
        5,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, float x1, float y1, float x2, float y2, u8 color)                                                     \
                                                                                                                        \
                                                                                                                        \
    macro(rect,                                                                                                         \
        "rect(x y w h color)",                                                                                          \
                                                                                                                        \
        "This function draws a filled rectangle of the desired size and color at the specified position.\n"             \
        "If you only need to draw the the border or outline of a rectangle (ie not filled) see `rectb()`.",             \
        5,                                                                                                              \
        5,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 x, s32 y, s32 width, s32 height, u8 color)                                                        \
                                                                                                                        \
                                                                                                                        \
    macro(rectb,                                                                                                        \
        "rectb(x y w h color)",                                                                                         \
                                                                                                                        \
        "This function draws a one pixel thick rectangle border at the position requested.\n"                           \
        "If you need to fill the rectangle with a color, see `rect()` instead.",                                        \
        5,                                                                                                              \
        5,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 x, s32 y, s32 width, s32 height, u8 color)                                                        \
                                                                                                                        \
                                                                                                                        \
    macro(spr,                                                                                                          \
        "spr(id x y colorkey=-1 scale=1 flip=0 rotate=0 w=1 h=1)",                                                      \
                                                                                                                        \
        "Draws the sprite number index at the x and y coordinate.\n"                                                    \
        "You can specify a colorkey in the palette which will be used as the transparent color "                        \
        "or use a value of -1 for an opaque sprite.\n"                                                                  \
        "The sprite can be scaled up by a desired factor. For example, "                                                \
        "a scale factor of 2 means an 8x8 pixel sprite is drawn to a 16x16 area of the screen.\n"                       \
        "You can flip the sprite where:\n"                                                                              \
        "- 0 = No Flip\n"                                                                                               \
        "- 1 = Flip horizontally\n"                                                                                     \
        "- 2 = Flip vertically\n"                                                                                       \
        "- 3 = Flip both vertically and horizontally\n"                                                                 \
        "When you rotate the sprite, it's rotated clockwise in 90 steps:\n"                                             \
        "- 0 = No rotation\n"                                                                                           \
        "- 1 = 90 rotation\n"                                                                                           \
        "- 2 = 180 rotation\n"                                                                                          \
        "- 3 = 270 rotation\n"                                                                                          \
        "You can draw a composite sprite (consisting of a rectangular region of sprites from the sprite sheet) "        \
        "by specifying the `w` and `h` parameters (which default to 1).",                                               \
        9,                                                                                                              \
        3,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 index, s32 x, s32 y, s32 w, s32 h,                                                                \
        u8* trans_colors, u8 trans_count, s32 scale, tic_flip flip, tic_rotate rotate)                                             \
                                                                                                                        \
                                                                                                                        \
    macro(btn,                                                                                                          \
        "btn(id) -> pressed",                                                                                           \
                                                                                                                        \
        "This function allows you to read the status of one of the buttons attached to TIC.\n"                          \
        "The function returns true if the key with the supplied id is currently in the pressed state.\n"                \
        "It remains true for as long as the key is held down.\n"                                                        \
        "If you want to test if a key was just pressed, use `btnp()` instead.",                                         \
        1,                                                                                                              \
        1,                                                                                                              \
        0,                                                                                                              \
        u32,                                                                                                            \
        tic_mem*, s32 id)                                                                                               \
                                                                                                                        \
                                                                                                                        \
    macro(btnp,                                                                                                         \
        "btnp(id hold=-1 period=-1) -> pressed",                                                                        \
                                                                                                                        \
        "This function allows you to read the status of one of TIC's buttons.\n"                                        \
        "It returns true only if the key has been pressed since the last frame.\n"                                      \
        "You can also use the optional hold and period parameters "                                                     \
        "which allow you to check if a button is being held down.\n"                                                    \
        "After the time specified by hold has elapsed, "                                                                \
        "btnp will return true each time period is passed if the key is still down.\n"                                  \
        "For example, to re-examine the state of button `0` after 2 seconds "                                           \
        "and continue to check its state every 1/10th of a second, you would use btnp(0, 120, 6).\n"                    \
        "Since time is expressed in ticks and TIC runs at 60 frames per second, "                                       \
        "we use the value of 120 to wait 2 seconds and 6 ticks (ie 60/10) as the interval for re-checking.",            \
        3,                                                                                                              \
        1,                                                                                                              \
        0,                                                                                                              \
        u32,                                                                                                            \
        tic_mem*, s32 id, s32 hold, s32 period)                                                                         \
                                                                                                                        \
                                                                                                                        \
    macro(sfx,                                                                                                          \
        "sfx(id note=-1 duration=-1 channel=0 volume=15 speed=0)",                                                      \
                                                                                                                        \
        "This function will play the sound with `id` created in the sfx editor.\n"                                      \
        "Calling the function with id set to -1 will stop playing the channel.\n"                                       \
        "The note can be supplied as an integer between 0 and 95 (representing 8 octaves of 12 notes each) "            \
        "or as a string giving the note name and octave.\n"                                                             \
        "For example, a note value of `14` will play the note `D` in the second octave.\n"                              \
        "The same note could be specified by the string `D-2`.\n"                                                       \
        "Note names consist of two characters, "                                                                        \
        "the note itself (in upper case) followed by `-` to represent the natural note or `#` to represent a sharp.\n"  \
        "There is no option to indicate flat values.\n"                                                                 \
        "The available note names are therefore: C-, C#, D-, D#, E-, F-, F#, G-, G#, A-, A#, B-.\n"                     \
        "The `octave` is specified using a single digit in the range 0 to 8.\n"                                         \
        "The `duration` specifies how many ticks to play the sound for since TIC-80 runs at 60 frames per second, "     \
        "a value of 30 represents half a second.\n"                                                                     \
        "A value of -1 will play the sound continuously.\n"                                                             \
        "The `channel` parameter indicates which of the four channels to use. Allowed values are 0 to 3.\n"             \
        "The `volume` can be between 0 and 15.\n"                                                                       \
        "The `speed` in the range -4 to 3 can be specified and means how many `ticks+1` to play each step, "            \
        "so speed==0 means 1 tick per step.",                                                                           \
        6,                                                                                                              \
        1,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 index, s32 note, s32 octave,                                                                      \
        s32 duration, s32 channel, s32 left, s32 right, s32 speed)                                                      \
                                                                                                                        \
                                                                                                                        \
    macro(map,                                                                                                          \
        "map(x=0 y=0 w=30 h=17 sx=0 sy=0 colorkey=-1 scale=1 remap=nil)",                                               \
                                                                                                                        \
        "The map consists of cells of 8x8 pixels, each of which can be filled with a sprite using the map editor.\n"    \
        "The map can be up to 240 cells wide by 136 deep.\n"                                                            \
        "This function will draw the desired area of the map to a specified screen position.\n"                         \
        "For example, map(5,5,12,10,0,0) will draw a 12x10 section of the map, "                                        \
        "starting from map co-ordinates (5,5) to screen position (0,0).\n"                                              \
        "The map function's last parameter is a powerful callback function "                                            \
        "for changing how map cells (sprites) are drawn when map is called.\n"                                          \
        "It can be used to rotate, flip and replace sprites while the game is running.\n"                               \
        "Unlike mset, which saves changes to the map, this special function can be used to create "                     \
        "animated tiles or replace them completely.\n"                                                                  \
        "Some examples include changing sprites to open doorways, "                                                     \
        "hiding sprites used to spawn objects in your game and even to emit the objects themselves.\n"                  \
        "The tilemap is laid out sequentially in RAM - writing 1 to 0x08000 "                                           \
        "will cause tile(sprite) #1 to appear at top left when map() is called.\n"                                      \
        "To set the tile immediately below this we need to write to 0x08000 + 240, ie 0x080F0.",                        \
        9,                                                                                                              \
        0,                                                                                                              \
        1,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy,                                                  \
        u8* trans_colors, u8 trans_count, s32 scale, RemapFunc remap, void* data)                                                  \
                                                                                                                        \
                                                                                                                        \
    macro(mget,                                                                                                         \
        "mget(x y) -> tile_id",                                                                                         \
                                                                                                                        \
        "Gets the sprite id at the given x and y map coordinate.",                                                      \
        2,                                                                                                              \
        2,                                                                                                              \
        0,                                                                                                              \
        u8,                                                                                                             \
        tic_mem*, s32 x, s32 y)                                                                                         \
                                                                                                                        \
                                                                                                                        \
    macro(mset,                                                                                                         \
        "mset(x y tile_id)",                                                                                            \
                                                                                                                        \
        "This function will change the tile at the specified map coordinates.\n"                                        \
        "By default, changes made are only kept while the current game is running.\n"                                   \
        "To make permanent changes to the map, see `sync()`.\n"                                                         \
        "Related: `map()` `mget()` `sync()`.",                                                                          \
        3,                                                                                                              \
        3,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 x, s32 y, u8 value)                                                                               \
                                                                                                                        \
                                                                                                                        \
    macro(peek,                                                                                                         \
        "peek(addr bits=8) -> value",                                                                                   \
                                                                                                                        \
        "This function allows to read the memory from TIC.\n"                                                           \
        "It's useful to access resources created with the integrated tools like sprite, maps, sounds, "                 \
        "cartridges data?\n"                                                                                            \
        "Never dream to sound a sprite?\n"                                                                              \
        "Address are in hexadecimal format but values are decimal.\n"                                                   \
        "To write to a memory address, use `poke()`.\n"                                                                 \
        "`bits` allowed to be 1,2,4,8.",                                                                                \
        2,                                                                                                              \
        1,                                                                                                              \
        0,                                                                                                              \
        u8,                                                                                                             \
        tic_mem*, s32 address, s32 bits)                                                                                \
                                                                                                                        \
                                                                                                                        \
    macro(poke,                                                                                                         \
        "poke(addr value bits=8)",                                                                                      \
                                                                                                                        \
        "This function allows you to write a single byte to any address in TIC's RAM.\n"                                \
        "The address should be specified in hexadecimal format, the value in decimal.\n"                                \
        "`bits` allowed to be 1,2,4,8.",                                                                                \
        3,                                                                                                              \
        2,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 address, u8 value, s32 bits)                                                                      \
                                                                                                                        \
                                                                                                                        \
    macro(peek1,                                                                                                        \
        "peek1(addr) -> value",                                                                                         \
                                                                                                                        \
        "This function enables you to read single bit values from TIC's RAM.\n"                                         \
        "The address is often specified in hexadecimal format.",                                                        \
        1,                                                                                                              \
        1,                                                                                                              \
        0,                                                                                                              \
        u8,                                                                                                             \
        tic_mem*, s32 address)                                                                                          \
                                                                                                                        \
                                                                                                                        \
    macro(poke1,                                                                                                        \
        "poke1(addr value)",                                                                                            \
                                                                                                                        \
        "This function allows you to write single bit values directly to RAM.\n"                                        \
        "The address is often specified in hexadecimal format.",                                                        \
        2,                                                                                                              \
        2,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 address, u8 value)                                                                                \
                                                                                                                        \
                                                                                                                        \
    macro(peek2,                                                                                                        \
        "peek2(addr) -> value",                                                                                         \
                                                                                                                        \
        "This function enables you to read two bits values from TIC's RAM.\n"                                           \
        "The address is often specified in hexadecimal format.",                                                        \
        1,                                                                                                              \
        1,                                                                                                              \
        0,                                                                                                              \
        u8,                                                                                                             \
        tic_mem*, s32 address)                                                                                          \
                                                                                                                        \
                                                                                                                        \
    macro(poke2,                                                                                                        \
        "poke2(addr value)",                                                                                            \
                                                                                                                        \
        "This function allows you to write two bits values directly to RAM.\n"                                          \
        "The address is often specified in hexadecimal format.",                                                        \
        2,                                                                                                              \
        2,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 address, u8 value)                                                                                \
                                                                                                                        \
                                                                                                                        \
    macro(peek4,                                                                                                        \
        "peek4(addr) -> value",                                                                                         \
                                                                                                                        \
        "This function enables you to read values from TIC's RAM.\n"                                                    \
        "The address is often specified in hexadecimal format.\n"                                                       \
        "See 'poke4()' for detailed information on how nibble addressing compares with byte addressing.",               \
        1,                                                                                                              \
        1,                                                                                                              \
        0,                                                                                                              \
        u8,                                                                                                             \
        tic_mem*, s32 address)                                                                                          \
                                                                                                                        \
                                                                                                                        \
    macro(poke4,                                                                                                        \
        "poke4(addr value)",                                                                                            \
                                                                                                                        \
        "This function allows you to write directly to RAM.\n"                                                          \
        "The address is often specified in hexadecimal format.\n"                                                       \
        "For both peek4 and poke4 RAM is addressed in 4 bit segments (nibbles).\n"                                      \
        "Therefore, to access the the RAM at byte address 0x4000\n"                                                     \
        "you would need to access both the 0x8000 and 0x8001 nibble addresses.",                                        \
        2,                                                                                                              \
        2,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 address, u8 value)                                                                                \
                                                                                                                        \
                                                                                                                        \
    macro(memcpy,                                                                                                       \
        "memcpy(dest source size)",                                                                                     \
                                                                                                                        \
        "This function allows you to copy a continuous block of TIC's 96K RAM from one address to another.\n"           \
        "Addresses are specified are in hexadecimal format, values are decimal.",                                       \
        3,                                                                                                              \
        3,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 dst, s32 src, s32 size)                                                                           \
                                                                                                                        \
                                                                                                                        \
    macro(memset,                                                                                                       \
        "memset(dest value size)",                                                                                      \
                                                                                                                        \
        "This function allows you to set a continuous block of any part of TIC's RAM to the same value.\n"              \
        "The address is specified in hexadecimal format, the value in decimal.",                                        \
        3,                                                                                                              \
        3,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 dst, u8 val, s32 size)                                                                            \
                                                                                                                        \
                                                                                                                        \
    macro(trace,                                                                                                        \
        "trace(message color=15)",                                                                                      \
                                                                                                                        \
        "This is a service function, useful for debugging your code.\n"                                                 \
        "It prints the message parameter to the console in the (optional) color specified.\n"                           \
        "\nTips:\n"                                                                                                     \
        "- The Lua concatenator for strings is .. (two points).\n"                                                      \
        "- Use console cls command to clear the output from trace.",                                                    \
        2,                                                                                                              \
        1,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, const char* text, u8 color)                                                                           \
                                                                                                                        \
                                                                                                                        \
    macro(pmem,                                                                                                         \
        "pmem(index value)\npmem(index) -> value",                                                                      \
                                                                                                                        \
        "This function allows you to save and retrieve data in one of the 256 individual 32-bit slots "                 \
        "available in the cartridge's persistent memory.\n"                                                             \
        "This is useful for saving high-scores, level advancement or achievements.\n"                                   \
        "The data is stored as unsigned 32-bit integers (from 0 to 4294967295).\n"                                      \
        "\nTips:\n"                                                                                                     \
        "- pmem depends on the cartridge hash (md5), so don't change your lua script if you want to keep the data.\n"   \
        "- Use `saveid:` with a personalized string in the header metadata to override the default MD5 calculation.\n"  \
        "This allows the user to update a cart without losing their saved data.",                                       \
        2,                                                                                                              \
        1,                                                                                                              \
        0,                                                                                                              \
        u32,                                                                                                            \
        tic_mem*, s32 index, u32 value, bool get)                                                                       \
                                                                                                                        \
                                                                                                                        \
    macro(time,                                                                                                         \
        "time() -> ticks",                                                                                              \
                                                                                                                        \
        "This function returns the number of milliseconds elapsed since the cartridge began execution.\n"               \
        "Useful for keeping track of time, animating items and triggering events.",                                     \
        0,                                                                                                              \
        0,                                                                                                              \
        0,                                                                                                              \
        double,                                                                                                         \
        tic_mem*)                                                                                                       \
                                                                                                                        \
                                                                                                                        \
    macro(tstamp,                                                                                                       \
        "tstamp() -> timestamp",                                                                                        \
                                                                                                                        \
        "This function returns the number of seconds elapsed since January 1st, 1970.\n"                                \
        "Useful for creating persistent games which evolve over time between plays.",                                   \
        0,                                                                                                              \
        0,                                                                                                              \
        0,                                                                                                              \
        s32,                                                                                                            \
        tic_mem*)                                                                                                       \
                                                                                                                        \
                                                                                                                        \
    macro(exit,                                                                                                         \
        "exit()",                                                                                                       \
                                                                                                                        \
        "Interrupts program execution and returns to the console when the TIC function ends.",                          \
        0,                                                                                                              \
        0,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*)                                                                                                       \
                                                                                                                        \
                                                                                                                        \
    macro(font,                                                                                                         \
        "font(text x y chromakey char_width char_height fixed=false scale=1) -> width",                                 \
                                                                                                                        \
        "Print string with font defined in foreground sprites.\n"                                                       \
        "To simply print to the screen, check out `print()`.\n"                                                         \
        "To print to the console, check out `trace()`.",                                                                \
        8,                                                                                                              \
        6,                                                                                                              \
        0,                                                                                                              \
        s32,                                                                                                            \
        tic_mem*, const char* text, s32 x, s32 y,                                                                       \
        u8* trans_colors, u8 trans_count, s32 w, s32 h, bool fixed, s32 scale, bool alt)                                                    \
                                                                                                                        \
                                                                                                                        \
    macro(mouse,                                                                                                        \
        "mouse() -> x y left middle right scrollx scrolly",                                                             \
                                                                                                                        \
        "This function returns the mouse coordinates and a boolean value for the state of each mouse button,"           \
        "with true indicating that a button is pressed.",                                                               \
        0,                                                                                                              \
        0,                                                                                                              \
        0,                                                                                                              \
        tic_point,                                                                                                      \
        tic_mem*)                                                                                                       \
                                                                                                                        \
                                                                                                                        \
    macro(circ,                                                                                                         \
        "circ(x y radius color)",                                                                                       \
                                                                                                                        \
        "This function draws a filled circle of the desired radius and color with its center at x, y.\n"                \
        "It uses the Bresenham algorithm.",                                                                             \
        4,                                                                                                              \
        4,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 x, s32 y, s32 radius, u8 color)                                                                   \
                                                                                                                        \
                                                                                                                        \
    macro(circb,                                                                                                        \
        "circb(x y radius color)",                                                                                      \
                                                                                                                        \
        "Draws the circumference of a circle with its center at x, y using the radius and color requested.\n"           \
        "It uses the Bresenham algorithm.",                                                                             \
        4,                                                                                                              \
        4,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 x, s32 y, s32 radius, u8 color)                                                                   \
                                                                                                                        \
                                                                                                                        \
    macro(elli,                                                                                                         \
        "elli(x y a b color)",                                                                                          \
                                                                                                                        \
        "This function draws a filled ellipse of the desired a, b radiuses and color with its center at x, y.\n"        \
        "It uses the Bresenham algorithm.",                                                                             \
        5,                                                                                                              \
        5,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 x, s32 y, s32 a, s32 b, u8 color)                                                                 \
                                                                                                                        \
                                                                                                                        \
    macro(ellib,                                                                                                        \
        "ellib(x y a b color)",                                                                                         \
                                                                                                                        \
        "This function draws an ellipse border with the desired radiuses a b and color with its center at x, y.\n"      \
        "It uses the Bresenham algorithm.",                                                                             \
        5,                                                                                                              \
        5,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 x, s32 y, s32 a, s32 b, u8 color)                                                                 \
                                                                                                                        \
                                                                                                                        \
    macro(tri,                                                                                                          \
        "tri(x1 y1 x2 y2 x3 y3 color)",                                                                                 \
                                                                                                                        \
        "This function draws a triangle filled with color, using the supplied vertices.",                               \
        7,                                                                                                              \
        7,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, float x1, float y1, float x2, float y2, float x3, float y3, u8 color)                                 \
                                                                                                                        \
    macro(trib,                                                                                                         \
        "trib(x1 y1 x2 y2 x3 y3 color)",                                                                                \
                                                                                                                        \
        "This function draws a triangle border with color, using the supplied vertices.",                               \
        7,                                                                                                              \
        7,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, float x1, float y1, float x2, float y2, float x3, float y3, u8 color)                                 \
                                                                                                                        \
                                                                                                                        \
    macro(ttri,                                                                                                       \
        "ttri(x1 y1 x2 y2 x3 y3 u1 v1 u2 v2 u3 v3 texsrc=0 chromakey=-1 z1=0 z2=0 z3=0)",                             \
                                                                                                                        \
        "It renders a triangle filled with texture from image ram, map ram or vbank.\n"                                 \
        "Use in 3D graphics.\n"                                                                                         \
        "In particular, if the vertices in the triangle have different 3D depth, you may see some distortion.\n"        \
        "These can be thought of as the window inside image ram (sprite sheet), map ram or another vbank.\n"            \
        "Note that the sprite sheet or map in this case is treated as a single large image, "                           \
        "with U and V addressing its pixels directly, rather than by sprite ID.\n"                                      \
        "So for example the top left corner of sprite #2 would be located at u=16, v=0.",                               \
        17,                                                                                                             \
        12,                                                                                                             \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, float x1, float y1, float x2, float y2, float x3, float y3,                                           \
        float u1, float v1, float u2, float v2, float u3, float v3, tic_texture_src texsrc, u8* colors, s32 count,      \
        float z1, float z2, float z3, bool depth)                                                                       \
                                                                                                                        \
                                                                                                                        \
    macro(clip,                                                                                                         \
        "clip(x y width height)\nclip()",                                                                               \
                                                                                                                        \
        "This function limits drawing to a clipping region or `viewport` defined by x,y,w,h.\n"                         \
        "Things drawn outside of this area will not be visible.\n"                                                      \
        "Calling clip() with no parameters will reset the drawing area to the entire screen.",                          \
        4,                                                                                                              \
        4,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 x, s32 y, s32 width, s32 height)                                                                  \
                                                                                                                        \
                                                                                                                        \
    macro(music,                                                                                                        \
        "music(track=-1 frame=-1 row=-1 loop=true sustain=false tempo=-1 speed=-1)",                                    \
                                                                                                                        \
        "This function starts playing a track created in the Music Editor.\n"                                           \
        "Call without arguments to stop the music.",                                                                    \
        7,                                                                                                              \
        0,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 track, s32 frame, s32 row, bool loop, bool sustain, s32 tempo, s32 speed)                         \
                                                                                                                        \
                                                                                                                        \
    macro(sync,                                                                                                         \
        "sync(mask=0 bank=0 tocart=false)",                                                                             \
                                                                                                                        \
        "The pro version of TIC-80 contains 8 memory banks.\n"                                                          \
        "To switch between these banks, sync can be used to either load contents from a memory bank to runtime, "       \
        "or save contents from the active runtime to a bank.\n"                                                         \
        "The function can only be called once per frame."                                                               \
        "If you have manipulated the runtime memory (e.g. by using mset), "                                             \
        "you can reset the active state by calling sync(0,0,false).\n"                                                  \
        "This resets the whole runtime memory to the contents of bank 0."                                               \
        "Note that sync is not used to load code from banks; this is done automatically.",                              \
        3,                                                                                                              \
        0,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, u32 mask, s32 bank, bool toCart)                                                                      \
                                                                                                                        \
                                                                                                                        \
    macro(vbank,                                                                                                        \
        "vbank(bank) -> prev\nvbank() -> prev",                                                                         \
                                                                                                                        \
        "VRAM contains 2x16K memory chips, use vbank(0) or vbank(1) to switch between them.",                           \
        1,                                                                                                              \
        1,                                                                                                              \
        0,                                                                                                              \
        s32,                                                                                                            \
        tic_mem*, s32 bank)                                                                                             \
                                                                                                                        \
                                                                                                                        \
    macro(reset,                                                                                                        \
        "reset()",                                                                                                      \
                                                                                                                        \
        "Resets the cartridge. To return to the console, see the `exit()`.",                                            \
        0,                                                                                                              \
        0,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*)                                                                                                       \
                                                                                                                        \
                                                                                                                        \
    macro(key,                                                                                                          \
        "key(code=-1) -> pressed",                                                                                      \
                                                                                                                        \
        "The function returns true if the key denoted by keycode is pressed.",                                          \
        1,                                                                                                              \
        0,                                                                                                              \
        0,                                                                                                              \
        bool,                                                                                                           \
        tic_mem*, tic_key key)                                                                                          \
                                                                                                                        \
                                                                                                                        \
    macro(keyp,                                                                                                         \
        "keyp(code=-1 hold=-1 period=-1) -> pressed",                                                                   \
                                                                                                                        \
        "This function returns true if the given key is pressed but wasn't pressed in the previous frame.\n"            \
        "Refer to `btnp()` for an explanation of the optional hold and period parameters.",                             \
        3,                                                                                                              \
        0,                                                                                                              \
        0,                                                                                                              \
        bool,                                                                                                           \
        tic_mem*, tic_key key, s32 hold, s32 period)                                                                    \
                                                                                                                        \
                                                                                                                        \
    macro(fget,                                                                                                         \
        "fget(sprite_id flag) -> bool",                                                                                 \
                                                                                                                        \
        "Returns true if the specified flag of the sprite is set. See `fset()` for more details.",                      \
        2,                                                                                                              \
        2,                                                                                                              \
        0,                                                                                                              \
        bool,                                                                                                           \
        tic_mem*, s32 index, u8 flag)                                                                                   \
                                                                                                                        \
                                                                                                                        \
    macro(fset,                                                                                                         \
        "fset(sprite_id flag bool)",                                                                                    \
                                                                                                                        \
        "Each sprite has eight flags which can be used to store information or signal different conditions.\n"          \
        "For example, flag 0 might be used to indicate that the sprite is invisible, "                                  \
        "flag 6 might indicate that the flag should be draw scaled etc.\n"                                              \
        "See algo `fget()`.",                                                                                           \
        3,                                                                                                              \
        3,                                                                                                              \
        0,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 index, u8 flag, bool value)

#define TIC_API_DEF(name, _, __, ___, ____, _____, ret, ...) ret tic_api_##name(__VA_ARGS__);
TIC_API_LIST(TIC_API_DEF)
#undef TIC_API_DEF

struct tic_mem
{
    tic80           product;
    tic_ram*             ram;
    tic_cartridge       cart;

    tic_ram*        base_ram;

    char saveid[TIC_SAVEID_SIZE];

    union
    {
        struct
        {
#if RETRO_IS_BIG_ENDIAN
            u8 padded:5;
            u8 keyboard:1;
            u8 mouse:1;
            u8 gamepad:1;
#else
            u8 gamepad:1;
            u8 mouse:1;
            u8 keyboard:1;
            u8 padded:5;
#endif
        };

        u8 data;
    } input;
};

tic_mem* tic_core_create(s32 samplerate, tic80_pixel_color_format format);
void tic_core_close(tic_mem* memory);
void tic_core_pause(tic_mem* memory);
void tic_core_resume(tic_mem* memory);
void tic_core_tick_start(tic_mem* memory);
void tic_core_tick(tic_mem* memory, tic_tick_data* data);
void tic_core_tick_end(tic_mem* memory);
void tic_core_synth_sound(tic_mem* tic);
void tic_core_blit(tic_mem* tic);
void tic_core_blit_ex(tic_mem* tic, tic_blit_callback clb);
const tic_script_config* tic_core_script_config(tic_mem* memory);

#define VBANK(tic, bank)                                \
    bool MACROVAR(_bank_) = tic_api_vbank(tic, bank);   \
    SCOPE(tic_api_vbank(tic, MACROVAR(_bank_)))
