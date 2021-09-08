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

#include "tic.h"

// convenience macros to loop languages
#define FOR_EACH_LANG(ln) for (tic_script_config** conf = Languages ; *conf != NULL; conf++ ) { tic_script_config* ln = *conf;
#define FOR_EACH_LANG_END }


typedef struct { u8 index; tic_flip flip; tic_rotate rotate; } RemapResult;
typedef void(*RemapFunc)(void*, s32 x, s32 y, RemapResult* result);

typedef void(*TraceOutput)(void*, const char*, u8 color);
typedef void(*ErrorOutput)(void*, const char*);
typedef void(*ExitCallback)(void*);
typedef bool(*CheckForceExit)(void*);

typedef struct
{
    TraceOutput trace;
    ErrorOutput error;
    ExitCallback exit;
    CheckForceExit forceExit;
    
    u64 (*counter)(void*);
    u64 (*freq)(void*);
    u64 start;

    void* data;
} tic_tick_data;

typedef struct tic_mem tic_mem;
typedef void(*tic_tick)(tic_mem* memory);
typedef void(*tic_scanline)(tic_mem* memory, s32 row, void* data);
typedef void(*tic_border)(tic_mem* memory, s32 row, void* data);
typedef void(*tic_overline)(tic_mem* memory, void* data);

typedef struct
{
    const char* pos;
    s32 size;
} tic_outline_item;

typedef struct
{
    tic_scanline scanline;
    tic_overline overline;
    tic_border border;
    void* data;
} tic_blit_callback;

typedef struct
{
    const char* name;
    const char* fileExtension;
    const char* projectComment;
    const void* demoRom;
    const s32   demoRomSize;
    const void* markRom;
    const s32   markRomSize;
    struct
    {
        bool(*init)(tic_mem* memory, const char* code);
        void(*close)(tic_mem* memory);

        tic_tick tick;
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

    const char* const * keywords;
    s32 keywordsCount;
} tic_script_config;


extern tic_script_config* Languages[];

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
    macro(tiles,    tiles,          0) \
    macro(sprites,  sprites,        1) \
    macro(map,      map,            2) \
    macro(sfx,      sfx,            3) \
    macro(music,    music,          4) \
    macro(palette,  vram.palette,   5) \
    macro(flags,    flags,          6) \
    macro(screen,   vram.screen,    7)

enum
{
#define TIC_SYNC_DEF(NAME, _, INDEX) tic_sync_##NAME = 1 << INDEX,
    TIC_SYNC_LIST(TIC_SYNC_DEF)
#undef TIC_SYNC_DEF
};

#define TIC_FN "TIC"
#define SCN_FN "SCN"
#define OVR_FN "OVR"
#define BDR_FN "BDR"

#define TIC_CALLBACK_LIST(macro)                                                                                        \
    macro(TIC_FN, TIC_FN "()", "Main function. It's called at " DEF2STR(TIC80_FRAMERATE)                                \
        "fps (" DEF2STR(TIC80_FRAMERATE) " times every second).")                                                       \
    macro(SCN_FN, SCN_FN "(row)", "Allows you to execute code between the drawing of each scanline, "                   \
        "for example, to manipulate the palette.")                                                                      \
    macro(BDR_FN, BDR_FN "(row)", "Allows you to execute code between the drawing of each fullscreen scanline, "        \
        "for example, to manipulate the palette.")                                                                      \
    macro(OVR_FN, OVR_FN "()", "Called after each frame;"                                                               \
        "draw calls from this function ignore palette swap and screen offset.")

// API DEFINITION TABLE
//  macro
//  (
//      definition
//      help
//      parameters count
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
        u8,                                                                                                             \
        tic_mem*, s32 x, s32 y, u8 color, bool get)                                                                     \
                                                                                                                        \
                                                                                                                        \
    macro(line,                                                                                                         \
        "line(x0 y0 x1 y1 color)",                                                                                      \
                                                                                                                        \
        "Draws a straight line from point (x0,y0) to point (x1,y1) in the specified color.",                            \
        5,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 x1, s32 y1, s32 x2, s32 y2, u8 color)                                                             \
                                                                                                                        \
                                                                                                                        \
    macro(rect,                                                                                                         \
        "rect(x y w h color)",                                                                                          \
                                                                                                                        \
        "This function draws a filled rectangle of the desired size and color at the specified position.\n"             \
        "If you only need to draw the the border or outline of a rectangle (ie not filled) see `rectb()`.",             \
        5,                                                                                                              \
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
        void,                                                                                                           \
        tic_mem*, s32 index, s32 x, s32 y, s32 w, s32 h,                                                                \
        u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)                                             \
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
        "The map function’s last parameter is a powerful callback function "                                            \
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
        void,                                                                                                           \
        tic_mem*, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy,                                                  \
        u8* colors, s32 count, s32 scale, RemapFunc remap, void* data)                                                  \
                                                                                                                        \
                                                                                                                        \
    macro(mget,                                                                                                         \
        "mget(x y) -> tile_id",                                                                                         \
                                                                                                                        \
        "Gets the sprite id at the given x and y map coordinate.",                                                      \
        2,                                                                                                              \
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
        void,                                                                                                           \
        tic_mem*, s32 x, s32 y, u8 value)                                                                               \
                                                                                                                        \
                                                                                                                        \
    macro(peek,                                                                                                         \
        "peek(addr res=8) -> value",                                                                                    \
                                                                                                                        \
        "This function allows to read the memory from TIC.\n"                                                           \
        "It's useful to access resources created with the integrated tools like sprite, maps, sounds, "                 \
        "cartridges data?\n"                                                                                            \
        "Never dream to sound a sprite?\n"                                                                              \
        "Address are in hexadecimal format but values are decimal.\n"                                                   \
        "To write to a memory address, use `poke()`.\n"                                                                 \
        "`res` allowed to be 1,2,4,8.",                                                                                 \
        2,                                                                                                              \
        u8,                                                                                                             \
        tic_mem*, s32 address, s32 res)                                                                                 \
                                                                                                                        \
                                                                                                                        \
    macro(poke,                                                                                                         \
        "poke(addr value res=8)",                                                                                       \
                                                                                                                        \
        "This function allows you to write a single byte to any address in TIC's RAM.\n"                                \
        "The address should be specified in hexadecimal format, the value in decimal.\n"                                \
        "`res` allowed to be 1,2,4,8.",                                                                                 \
        3,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 address, u8 value, s32 res)                                                                       \
                                                                                                                        \
                                                                                                                        \
    macro(peek4,                                                                                                        \
        "peek4(addr) -> value",                                                                                         \
                                                                                                                        \
        "This function enables you to read 4bit values from TIC's RAM.\n"                                               \
        "The address should be specified in hexadecimal format.",                                                       \
        1,                                                                                                              \
        u8,                                                                                                             \
        tic_mem*, s32 address)                                                                                          \
                                                                                                                        \
                                                                                                                        \
    macro(poke4,                                                                                                        \
        "poke4(addr value)",                                                                                            \
                                                                                                                        \
        "This function allows you to write to the virtual RAM of TIC.\n"                                                \
        "It differs from `poke()` in that it divides memory in groups of 4 bits.\n"                                     \
        "Therefore, to address the high nibble of position 0x4000 you should pass 0x8000 as addr4, "                    \
        "and to access the low nibble (rightmost 4 bits) you would pass 0x8001.\n"                                      \
        "The address should be specified in hexadecimal format, and values should be given in decimal.",                \
        2,                                                                                                              \
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
        s32,                                                                                                            \
        tic_mem*)                                                                                                       \
                                                                                                                        \
                                                                                                                        \
    macro(exit,                                                                                                         \
        "exit()",                                                                                                       \
                                                                                                                        \
        "Interrupts program execution and returns to the console when the TIC function ends.",                          \
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
        s32,                                                                                                            \
        tic_mem*, const char* text, s32 x, s32 y,                                                                       \
        u8 chromakey, s32 w, s32 h, bool fixed, s32 scale, bool alt)                                                    \
                                                                                                                        \
                                                                                                                        \
    macro(mouse,                                                                                                        \
        "mouse() -> x y left middle right scrollx scrolly",                                                             \
                                                                                                                        \
        "This function returns the mouse coordinates and a boolean value for the state of each mouse button,"           \
        "with true indicating that a button is pressed.",                                                               \
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
        void,                                                                                                           \
        tic_mem*, s32 x, s32 y, s32 a, s32 b, u8 color)                                                                 \
                                                                                                                        \
                                                                                                                        \
    macro(tri,                                                                                                          \
        "tri(x1 y1 x2 y2 x3 y3 color)",                                                                                 \
                                                                                                                        \
        "This function draws a triangle filled with color, using the supplied vertices.",                               \
        7,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, u8 color)                                             \
                                                                                                                        \
    macro(trib,                                                                                                         \
        "trib(x1 y1 x2 y2 x3 y3 color)",                                                                                \
                                                                                                                        \
        "This function draws a triangle border with color, using the supplied vertices.",                               \
        7,                                                                                                              \
        void,                                                                                                           \
        tic_mem*, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, u8 color)                                             \
                                                                                                                        \
                                                                                                                        \
    macro(textri,                                                                                                       \
        "textri(x1 y1 x2 y2 x3 y3 u1 v1 u2 v2 u3 v3 use_map=false chromakey=-1)",                                       \
                                                                                                                        \
        "It renders a triangle filled with texture from image ram or map ram.\n"                                        \
        "Use in 3D graphics.\n"                                                                                         \
        "This function does not perform perspective correction, so it is not generally suitable for 3D graphics "       \
        "(except in some constrained scenarios).\n"                                                                     \
        "In particular, if the vertices in the triangle have different 3D depth, you may see some distortion.\n"        \
        "These can be thought of as the window inside image ram (sprite sheet), or map ram.\n"                          \
        "Note that the sprite sheet or map in this case is treated as a single large image, "                           \
        "with U and V addressing its pixels directly, rather than by sprite ID.\n"                                      \
        "So for example the top left corner of sprite #2 would be located at u=16, v=0.",                               \
        14,                                                                                                             \
        void,                                                                                                           \
        tic_mem*, float x1, float y1, float x2, float y2, float x3, float y3,                                           \
        float u1, float v1, float u2, float v2, float u3, float v3, bool use_map, u8* colors, s32 count)                \
                                                                                                                        \
                                                                                                                        \
    macro(clip,                                                                                                         \
        "clip(x y width height)\nclip()",                                                                               \
                                                                                                                        \
        "This function limits drawing to a clipping region or `viewport` defined by x,y,w,h.\n"                         \
        "Things drawn outside of this area will not be visible.\n"                                                      \
        "Calling clip() with no parameters will reset the drawing area to the entire screen.",                          \
        4,                                                                                                              \
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
        void,                                                                                                           \
        tic_mem*, u32 mask, s32 bank, bool toCart)                                                                      \
                                                                                                                        \
                                                                                                                        \
    macro(reset,                                                                                                        \
        "reset()",                                                                                                      \
                                                                                                                        \
        "Resets the cartridge. To return to the console, see the `exit()`.",                                            \
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
        bool,                                                                                                           \
        tic_mem*, tic_key key, s32 hold, s32 period)                                                                    \
                                                                                                                        \
                                                                                                                        \
    macro(fget,                                                                                                         \
        "fget(sprite_id flag) -> bool",                                                                                 \
                                                                                                                        \
        "Returns true if the specified flag of the sprite is set. See `fset()` for more details.",                      \
        2,                                                                                                              \
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
        void,                                                                                                           \
        tic_mem*, s32 index, u8 flag, bool value)

#define TIC_API_DEF(name, _, __, ___, ret, ...) ret tic_api_##name(__VA_ARGS__);
TIC_API_LIST(TIC_API_DEF)
#undef TIC_API_DEF

struct tic_mem
{
    tic_ram             ram;
    tic_cartridge       cart;

    char saveid[TIC_SAVEID_SIZE];

    union
    {
        struct
        {
            u8 gamepad:1;
            u8 mouse:1;
            u8 keyboard:1;
        };

        u8 data;
    } input;

    struct
    {
        s16* buffer;
        s32 size;
    } samples;

#if defined(_3DS)
    u32 *screen;
#else
    u32 screen[TIC80_FULLWIDTH * TIC80_FULLHEIGHT];
#endif
    tic80_pixel_color_format screen_format;
};

tic_mem* tic_core_create(s32 samplerate);
void tic_core_close(tic_mem* memory);
void tic_core_pause(tic_mem* memory);
void tic_core_resume(tic_mem* memory);
void tic_core_tick_start(tic_mem* memory);
void tic_core_tick(tic_mem* memory, tic_tick_data* data);
void tic_core_tick_end(tic_mem* memory);
void tic_core_blit(tic_mem* tic, tic80_pixel_color_format fmt);

void tic_core_blit_ex(tic_mem* tic, tic80_pixel_color_format fmt, tic_blit_callback clb);
const tic_script_config* tic_core_script_config(tic_mem* memory);

typedef struct
{
    tic80 tic;
    tic_mem* memory;
    tic_tick_data tickData;
    u64 tick_counter;
} tic80_local;
