// MIT License

// Copyright (c) 2024 TIC-80 contributors

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

// Forth language integration for TIC-80 using pForth.
//
// This file serves three roles:
//   1. pforth I/O layer: routes pforth output to TIC-80 trace, stubs file I/O.
//   2. Replacement for pForth's pfcustom.c: defines CustomFunctionTable[] and
//      CompileCustomFunctions() which register TIC-80 API words in the Forth
//      dictionary.
//   3. TIC-80 tic_script implementation: lifecycle (init/close/tick/boot/etc.)
//      and the exported tic_script descriptor.
//
// Stack convention used by all wrappers:
//   All words are registered with NumParams=0. Each wrapper pops its own
//   arguments from the pforth data stack with pfPopFromStack(). Arguments are
//   popped in reverse order (TOS = last/rightmost parameter in Forth notation).
//   Example: for ( x y color -- ), pop color first, then y, then x.

#include "core/core.h"

// pf_all.h is pforth's internal umbrella header; it defines Err, cell_t,
// CFunc0 and everything needed by pfcustom.c replacements.
// Undefine TIC-80's MIN/MAX first to avoid redefinition warnings from
// pforth's pf_guts.h, which defines its own versions.
#undef MIN
#undef MAX
#include "pf_all.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern bool parse_note(const char* noteStr, s32* note, s32* octave);

// =============================================================================
// pforth I/O layer
//
// Routes all pforth terminal output to the TIC-80 trace callback.
// File I/O is stubbed out (not needed for cartridge execution).
// =============================================================================

#define FORTH_IO_BUF 256

static char           gOutBuf[FORTH_IO_BUF];
static int            gOutLen   = 0;
static tic_tick_data* gTickData = NULL;

static void flushOut(void)
{
    if (gOutLen > 0 && gTickData && gTickData->trace)
    {
        gOutBuf[gOutLen] = '\0';
        gTickData->trace(gTickData->data, gOutBuf, 15);
        gOutLen = 0;
    }
}

static void forthInitIO(tic_tick_data* tickData)
{
    gTickData = tickData;
    gOutLen   = 0;
}

static void forthTermIO(void)
{
    flushOut();
    gTickData = NULL;
}

// Returns buffered output as a C string; used for error reporting.
static const char* forthGetOutputBuffer(void)
{
    gOutBuf[gOutLen] = '\0';
    return gOutBuf;
}

static void forthClearOutputBuffer(void)
{
    gOutLen = 0;
}

static void forthFlushOutput(void)
{
    flushOut();
}

// ---- pforth terminal I/O callbacks ------------------------------------------

int sdTerminalOut(char c)
{
    if (gOutLen < FORTH_IO_BUF - 1)
        gOutBuf[gOutLen++] = c;

    if (c == '\n' || gOutLen >= FORTH_IO_BUF - 1)
        flushOut();

    return 0;
}

int sdTerminalEcho(char c)
{
    return sdTerminalOut(c);
}

int sdTerminalIn(void)
{
    return -1;
}

int sdTerminalFlush(void)
{
    flushOut();
    return 0;
}

int sdQueryTerminal(void)
{
    return 0;
}

void sdTerminalInit(void) {}
void sdTerminalTerm(void) {}

cell_t sdSleepMillis(cell_t msec)
{
    (void)msec;
    return 0;
}

// =============================================================================
// Global state
// pforth is single-instance (global variables in its C implementation), so one
// global pointer to the active TIC-80 core is sufficient.
// =============================================================================

static tic_core*   gForthCore = NULL;
static PForthTask  gForthTask = NULL;

// Scratch buffer for converting Forth counted strings ( c-addr u ) to C strings
#define FORTH_STR_BUF 1024
static char gStrBuf[FORTH_STR_BUF];

// pfInterpretText is limited to TIB_SIZE (256) characters per call, but pforth
// preserves compiler state (gVarState, dictionary position) across calls, so
// feeding one line at a time works correctly for multi-line definitions.
static ThrowCode forthInterpretLines(const char* source)
{
    char line[TIB_SIZE];

    const char* p = source;
    while (*p)
    {
        const char* end = strchr(p, '\n');
        if (!end) end = p + strlen(p);

        // Strip trailing CR so Windows line endings (\r\n) work too.
        const char* lineEnd = end;
        if (lineEnd > p && *(lineEnd - 1) == '\r') lineEnd--;

        size_t len = (size_t)(lineEnd - p);
        if (len >= TIB_SIZE) len = TIB_SIZE - 1;

        memcpy(line, p, len);
        line[len] = '\0';

        if (len > 0)
        {
            ThrowCode r = pfInterpretText(line);
            if (r != 0) return r;
        }

        p = (*end == '\n') ? end + 1 : end;
    }
    return 0;
}

static const char* forthCountedToC(cell_t addr, cell_t len)
{
    if (len < 0) len = 0;
    if (len >= FORTH_STR_BUF) len = FORTH_STR_BUF - 1;
    memcpy(gStrBuf, (const char*)(uintptr_t)addr, (size_t)len);
    gStrBuf[len] = '\0';
    return gStrBuf;
}

// =============================================================================
// TIC-80 API wrappers
//
// Each wrapper function is registered with NumParams=0 in CompileCustomFunctions
// and therefore receives no C arguments. It pops all Forth stack arguments
// manually via pfPopFromStack().
//
// Return value: non-zero values are pushed to the data stack by CreateGlueToC
// when C_RETURNS_VALUE is used; void wrappers return 0 but are registered with
// C_RETURNS_VOID so no push occurs.
// =============================================================================

// cls ( color -- )
static cell_t tic_forth_cls(void)
{
    u8 color = (u8)pfPopFromStack();
    gForthCore->api.cls((tic_mem*)gForthCore, color);
    return 0;
}

// print ( c-addr u x y color fixed scale alt -- width )
static cell_t tic_forth_print(void)
{
    bool    alt   = (bool)pfPopFromStack();
    s32     scale = (s32)pfPopFromStack();
    bool    fixed = (bool)pfPopFromStack();
    u8      color = (u8)pfPopFromStack();
    s32     y     = (s32)pfPopFromStack();
    s32     x     = (s32)pfPopFromStack();
    cell_t  len   = pfPopFromStack();
    cell_t  addr  = pfPopFromStack();
    const char* text = forthCountedToC(addr, len);
    return (cell_t)gForthCore->api.print((tic_mem*)gForthCore,
                                         text, x, y, color, fixed, scale, alt);
}

// pix ( x y -- color )   read pixel
static cell_t tic_forth_pix_get(void)
{
    s32 y = (s32)pfPopFromStack();
    s32 x = (s32)pfPopFromStack();
    return (cell_t)gForthCore->api.pix((tic_mem*)gForthCore, x, y, 0, true);
}

// pix! ( x y color -- )  write pixel
static cell_t tic_forth_pix_set(void)
{
    u8  color = (u8)pfPopFromStack();
    s32 y     = (s32)pfPopFromStack();
    s32 x     = (s32)pfPopFromStack();
    gForthCore->api.pix((tic_mem*)gForthCore, x, y, color, false);
    return 0;
}

// line ( x0 y0 x1 y1 color -- )
static cell_t tic_forth_line(void)
{
    u8    color = (u8)pfPopFromStack();
    float y1    = (float)(s32)pfPopFromStack();
    float x1    = (float)(s32)pfPopFromStack();
    float y0    = (float)(s32)pfPopFromStack();
    float x0    = (float)(s32)pfPopFromStack();
    gForthCore->api.line((tic_mem*)gForthCore, x0, y0, x1, y1, color);
    return 0;
}

// rect ( x y w h color -- )
static cell_t tic_forth_rect(void)
{
    u8  color  = (u8)pfPopFromStack();
    s32 height = (s32)pfPopFromStack();
    s32 width  = (s32)pfPopFromStack();
    s32 y      = (s32)pfPopFromStack();
    s32 x      = (s32)pfPopFromStack();
    gForthCore->api.rect((tic_mem*)gForthCore, x, y, width, height, color);
    return 0;
}

// rectb ( x y w h color -- )
static cell_t tic_forth_rectb(void)
{
    u8  color  = (u8)pfPopFromStack();
    s32 height = (s32)pfPopFromStack();
    s32 width  = (s32)pfPopFromStack();
    s32 y      = (s32)pfPopFromStack();
    s32 x      = (s32)pfPopFromStack();
    gForthCore->api.rectb((tic_mem*)gForthCore, x, y, width, height, color);
    return 0;
}

// spr ( id x y colorkey scale flip rotate w h -- )
// colorkey: -1 = opaque, 0..15 = transparent color index
static cell_t tic_forth_spr(void)
{
    s32      h        = (s32)pfPopFromStack();
    s32      w        = (s32)pfPopFromStack();
    tic_rotate rotate = (tic_rotate)pfPopFromStack();
    tic_flip flip     = (tic_flip)pfPopFromStack();
    s32      scale    = (s32)pfPopFromStack();
    s32      colorkey = (s32)pfPopFromStack();
    s32      y        = (s32)pfPopFromStack();
    s32      x        = (s32)pfPopFromStack();
    s32      id       = (s32)pfPopFromStack();

    static u8 trans[1];
    u8  count = 0;
    if (colorkey >= 0) { trans[0] = (u8)colorkey; count = 1; }

    gForthCore->api.spr((tic_mem*)gForthCore,
                        id, x, y, w, h, trans, count, scale, flip, rotate);
    return 0;
}

// btn ( id -- pressed )
static cell_t tic_forth_btn(void)
{
    s32 id = (s32)pfPopFromStack();
    return (cell_t)gForthCore->api.btn((tic_mem*)gForthCore, id);
}

// btnp ( id hold period -- pressed )
static cell_t tic_forth_btnp(void)
{
    s32 period = (s32)pfPopFromStack();
    s32 hold   = (s32)pfPopFromStack();
    s32 id     = (s32)pfPopFromStack();
    return (cell_t)gForthCore->api.btnp((tic_mem*)gForthCore, id, hold, period);
}

// sfx ( id note octave duration channel volume speed -- )
// note: 0..11 (C=0..B=11), octave: 0..8, or both -1 to use stored note
static cell_t tic_forth_sfx(void)
{
    s32 speed    = (s32)pfPopFromStack();
    s32 volume   = (s32)pfPopFromStack();
    s32 channel  = (s32)pfPopFromStack();
    s32 duration = (s32)pfPopFromStack();
    s32 octave   = (s32)pfPopFromStack();
    s32 note     = (s32)pfPopFromStack();
    s32 id       = (s32)pfPopFromStack();
    gForthCore->api.sfx((tic_mem*)gForthCore,
                        id, note, octave, duration, channel, volume, volume, speed);
    return 0;
}

// map ( cellx celly cellw cellh sx sy colorkey scale -- )
// No remap callback in this version.
static cell_t tic_forth_map(void)
{
    s32      scale    = (s32)pfPopFromStack();
    s32      colorkey = (s32)pfPopFromStack();
    s32      sy       = (s32)pfPopFromStack();
    s32      sx       = (s32)pfPopFromStack();
    s32      cellh    = (s32)pfPopFromStack();
    s32      cellw    = (s32)pfPopFromStack();
    s32      celly    = (s32)pfPopFromStack();
    s32      cellx    = (s32)pfPopFromStack();

    static u8 trans[1];
    u8 count = 0;
    if (colorkey >= 0) { trans[0] = (u8)colorkey; count = 1; }

    gForthCore->api.map((tic_mem*)gForthCore,
                        cellx, celly, cellw, cellh, sx, sy,
                        trans, count, scale, NULL, NULL);
    return 0;
}

// mget ( x y -- tile_id )
static cell_t tic_forth_mget(void)
{
    s32 y = (s32)pfPopFromStack();
    s32 x = (s32)pfPopFromStack();
    return (cell_t)gForthCore->api.mget((tic_mem*)gForthCore, x, y);
}

// mset ( x y tile_id -- )
static cell_t tic_forth_mset(void)
{
    u8  tile = (u8)pfPopFromStack();
    s32 y    = (s32)pfPopFromStack();
    s32 x    = (s32)pfPopFromStack();
    gForthCore->api.mset((tic_mem*)gForthCore, x, y, tile);
    return 0;
}

// peek ( addr bits -- value )
static cell_t tic_forth_peek(void)
{
    s32 bits = (s32)pfPopFromStack();
    s32 addr = (s32)pfPopFromStack();
    return (cell_t)gForthCore->api.peek((tic_mem*)gForthCore, addr, bits);
}

// poke ( addr value bits -- )
static cell_t tic_forth_poke(void)
{
    s32 bits  = (s32)pfPopFromStack();
    u8  value = (u8)pfPopFromStack();
    s32 addr  = (s32)pfPopFromStack();
    gForthCore->api.poke((tic_mem*)gForthCore, addr, value, bits);
    return 0;
}

// peek1 ( addr -- value )
static cell_t tic_forth_peek1(void)
{
    s32 addr = (s32)pfPopFromStack();
    return (cell_t)gForthCore->api.peek1((tic_mem*)gForthCore, addr);
}

// poke1 ( addr value -- )
static cell_t tic_forth_poke1(void)
{
    u8  value = (u8)pfPopFromStack();
    s32 addr  = (s32)pfPopFromStack();
    gForthCore->api.poke1((tic_mem*)gForthCore, addr, value);
    return 0;
}

// peek2 ( addr -- value )
static cell_t tic_forth_peek2(void)
{
    s32 addr = (s32)pfPopFromStack();
    return (cell_t)gForthCore->api.peek2((tic_mem*)gForthCore, addr);
}

// poke2 ( addr value -- )
static cell_t tic_forth_poke2(void)
{
    u8  value = (u8)pfPopFromStack();
    s32 addr  = (s32)pfPopFromStack();
    gForthCore->api.poke2((tic_mem*)gForthCore, addr, value);
    return 0;
}

// peek4 ( addr -- value )
static cell_t tic_forth_peek4(void)
{
    s32 addr = (s32)pfPopFromStack();
    return (cell_t)gForthCore->api.peek4((tic_mem*)gForthCore, addr);
}

// poke4 ( addr value -- )
static cell_t tic_forth_poke4(void)
{
    u8  value = (u8)pfPopFromStack();
    s32 addr  = (s32)pfPopFromStack();
    gForthCore->api.poke4((tic_mem*)gForthCore, addr, value);
    return 0;
}

// memcpy ( dst src size -- )
static cell_t tic_forth_memcpy(void)
{
    s32 size = (s32)pfPopFromStack();
    s32 src  = (s32)pfPopFromStack();
    s32 dst  = (s32)pfPopFromStack();
    gForthCore->api.memcpy((tic_mem*)gForthCore, dst, src, size);
    return 0;
}

// memset ( dst value size -- )
static cell_t tic_forth_memset(void)
{
    s32 size  = (s32)pfPopFromStack();
    u8  value = (u8)pfPopFromStack();
    s32 dst   = (s32)pfPopFromStack();
    gForthCore->api.memset((tic_mem*)gForthCore, dst, value, size);
    return 0;
}

// trace ( c-addr u color -- )
static cell_t tic_forth_trace(void)
{
    u8     color = (u8)pfPopFromStack();
    cell_t len   = pfPopFromStack();
    cell_t addr  = pfPopFromStack();
    const char* text = forthCountedToC(addr, len);
    gForthCore->api.trace((tic_mem*)gForthCore, text, color);
    return 0;
}

// pmem ( index -- value )  get persistent memory slot
static cell_t tic_forth_pmem_get(void)
{
    s32 index = (s32)pfPopFromStack();
    return (cell_t)gForthCore->api.pmem((tic_mem*)gForthCore, index, 0, false);
}

// pmem! ( value index -- )  set persistent memory slot
static cell_t tic_forth_pmem_set(void)
{
    s32 index = (s32)pfPopFromStack();
    u32 value = (u32)pfPopFromStack();
    gForthCore->api.pmem((tic_mem*)gForthCore, index, value, true);
    return 0;
}

// time ( -- ms )
static cell_t tic_forth_time(void)
{
    return (cell_t)(s32)gForthCore->api.time((tic_mem*)gForthCore);
}

// tstamp ( -- seconds )
static cell_t tic_forth_tstamp(void)
{
    return (cell_t)gForthCore->api.tstamp((tic_mem*)gForthCore);
}

// exit ( -- )
static cell_t tic_forth_exit(void)
{
    gForthCore->api.exit((tic_mem*)gForthCore);
    return 0;
}

// font ( c-addr u x y chromakey cw ch fixed scale alt -- width )
static cell_t tic_forth_font(void)
{
    bool    alt       = (bool)pfPopFromStack();
    s32     scale     = (s32)pfPopFromStack();
    bool    fixed     = (bool)pfPopFromStack();
    s32     ch        = (s32)pfPopFromStack();
    s32     cw        = (s32)pfPopFromStack();
    u8      chromakey = (u8)pfPopFromStack();
    s32     y         = (s32)pfPopFromStack();
    s32     x         = (s32)pfPopFromStack();
    cell_t  len       = pfPopFromStack();
    cell_t  addr      = pfPopFromStack();
    const char* text  = forthCountedToC(addr, len);

    static u8 trans[1];
    trans[0] = chromakey;

    return (cell_t)gForthCore->api.font((tic_mem*)gForthCore,
                                        text, x, y, trans, 1, cw, ch, fixed, scale, alt);
}

// mouse ( -- x y left mid right scrollx scrolly )
// All 7 values are pushed onto the stack.
static cell_t tic_forth_mouse(void)
{
    tic_point pos = gForthCore->api.mouse((tic_mem*)gForthCore);
    const tic80_mouse* m = &gForthCore->memory.ram->input.mouse;

    pfPushToStack((cell_t)pos.x);
    pfPushToStack((cell_t)pos.y);
    pfPushToStack((cell_t)m->left);
    pfPushToStack((cell_t)m->middle);
    pfPushToStack((cell_t)m->right);
    pfPushToStack((cell_t)m->scrollx);
    pfPushToStack((cell_t)m->scrolly);
    return 0;  // C_RETURNS_VOID: the 7 pushes above are the return values
}

// circ ( x y radius color -- )
static cell_t tic_forth_circ(void)
{
    u8  color  = (u8)pfPopFromStack();
    s32 radius = (s32)pfPopFromStack();
    s32 y      = (s32)pfPopFromStack();
    s32 x      = (s32)pfPopFromStack();
    gForthCore->api.circ((tic_mem*)gForthCore, x, y, radius, color);
    return 0;
}

// circb ( x y radius color -- )
static cell_t tic_forth_circb(void)
{
    u8  color  = (u8)pfPopFromStack();
    s32 radius = (s32)pfPopFromStack();
    s32 y      = (s32)pfPopFromStack();
    s32 x      = (s32)pfPopFromStack();
    gForthCore->api.circb((tic_mem*)gForthCore, x, y, radius, color);
    return 0;
}

// elli ( x y a b color -- )
static cell_t tic_forth_elli(void)
{
    u8  color = (u8)pfPopFromStack();
    s32 b     = (s32)pfPopFromStack();
    s32 a     = (s32)pfPopFromStack();
    s32 y     = (s32)pfPopFromStack();
    s32 x     = (s32)pfPopFromStack();
    gForthCore->api.elli((tic_mem*)gForthCore, x, y, a, b, color);
    return 0;
}

// ellib ( x y a b color -- )
static cell_t tic_forth_ellib(void)
{
    u8  color = (u8)pfPopFromStack();
    s32 b     = (s32)pfPopFromStack();
    s32 a     = (s32)pfPopFromStack();
    s32 y     = (s32)pfPopFromStack();
    s32 x     = (s32)pfPopFromStack();
    gForthCore->api.ellib((tic_mem*)gForthCore, x, y, a, b, color);
    return 0;
}

// paint ( x y color bordercolor -- )
static cell_t tic_forth_paint(void)
{
    u8  bordercolor = (u8)pfPopFromStack();
    u8  color       = (u8)pfPopFromStack();
    s32 y           = (s32)pfPopFromStack();
    s32 x           = (s32)pfPopFromStack();
    gForthCore->api.paint((tic_mem*)gForthCore, x, y, color, bordercolor);
    return 0;
}

// tri ( x1 y1 x2 y2 x3 y3 color -- )
// Integer coordinates are cast to float (sub-pixel precision not required here).
static cell_t tic_forth_tri(void)
{
    u8    color = (u8)pfPopFromStack();
    float y3    = (float)(s32)pfPopFromStack();
    float x3    = (float)(s32)pfPopFromStack();
    float y2    = (float)(s32)pfPopFromStack();
    float x2    = (float)(s32)pfPopFromStack();
    float y1    = (float)(s32)pfPopFromStack();
    float x1    = (float)(s32)pfPopFromStack();
    gForthCore->api.tri((tic_mem*)gForthCore, x1, y1, x2, y2, x3, y3, color);
    return 0;
}

// trib ( x1 y1 x2 y2 x3 y3 color -- )
static cell_t tic_forth_trib(void)
{
    u8    color = (u8)pfPopFromStack();
    float y3    = (float)(s32)pfPopFromStack();
    float x3    = (float)(s32)pfPopFromStack();
    float y2    = (float)(s32)pfPopFromStack();
    float x2    = (float)(s32)pfPopFromStack();
    float y1    = (float)(s32)pfPopFromStack();
    float x1    = (float)(s32)pfPopFromStack();
    gForthCore->api.trib((tic_mem*)gForthCore, x1, y1, x2, y2, x3, y3, color);
    return 0;
}

// ttri ( x1 y1 x2 y2 x3 y3 u1 v1 u2 v2 u3 v3 texsrc colorkey z1 z2 z3 depth -- )
// texsrc: 0=tiles, 1=map, 2=vbank; colorkey: -1=opaque, 0..15=transparent index
static cell_t tic_forth_ttri(void)
{
    bool     depth    = (bool)pfPopFromStack();
    float    z3       = (float)(s32)pfPopFromStack();
    float    z2       = (float)(s32)pfPopFromStack();
    float    z1       = (float)(s32)pfPopFromStack();
    s32      colorkey = (s32)pfPopFromStack();
    tic_texture_src texsrc = (tic_texture_src)pfPopFromStack();
    float    v3       = (float)(s32)pfPopFromStack();
    float    u3       = (float)(s32)pfPopFromStack();
    float    v2       = (float)(s32)pfPopFromStack();
    float    u2       = (float)(s32)pfPopFromStack();
    float    v1       = (float)(s32)pfPopFromStack();
    float    u1       = (float)(s32)pfPopFromStack();
    float    y3       = (float)(s32)pfPopFromStack();
    float    x3       = (float)(s32)pfPopFromStack();
    float    y2       = (float)(s32)pfPopFromStack();
    float    x2       = (float)(s32)pfPopFromStack();
    float    y1       = (float)(s32)pfPopFromStack();
    float    x1       = (float)(s32)pfPopFromStack();

    static u8 trans[1];
    u8 count = 0;
    if (colorkey >= 0) { trans[0] = (u8)colorkey; count = 1; }

    gForthCore->api.ttri((tic_mem*)gForthCore,
                         x1, y1, x2, y2, x3, y3,
                         u1, v1, u2, v2, u3, v3,
                         texsrc, trans, count, z1, z2, z3, depth);
    return 0;
}

// clip ( x y w h -- )
static cell_t tic_forth_clip(void)
{
    s32 h = (s32)pfPopFromStack();
    s32 w = (s32)pfPopFromStack();
    s32 y = (s32)pfPopFromStack();
    s32 x = (s32)pfPopFromStack();
    gForthCore->api.clip((tic_mem*)gForthCore, x, y, w, h);
    return 0;
}

// clip0 ( -- )  reset clip region to full screen
static cell_t tic_forth_clip0(void)
{
    gForthCore->api.clip((tic_mem*)gForthCore, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
    return 0;
}

// music ( track frame row loop sustain tempo speed -- )
// Pass -1 for any value to use defaults.  Call with track=-1 to stop.
static cell_t tic_forth_music(void)
{
    s32  speed   = (s32)pfPopFromStack();
    s32  tempo   = (s32)pfPopFromStack();
    bool sustain = (bool)pfPopFromStack();
    bool loop    = (bool)pfPopFromStack();
    s32  row     = (s32)pfPopFromStack();
    s32  frame   = (s32)pfPopFromStack();
    s32  track   = (s32)pfPopFromStack();
    gForthCore->api.music((tic_mem*)gForthCore,
                          track, frame, row, loop, sustain, tempo, speed);
    return 0;
}

// sync ( mask bank tocart -- )
static cell_t tic_forth_sync(void)
{
    bool toCart = (bool)pfPopFromStack();
    s32  bank   = (s32)pfPopFromStack();
    u32  mask   = (u32)pfPopFromStack();
    gForthCore->api.sync((tic_mem*)gForthCore, mask, bank, toCart);
    return 0;
}

// vbank ( bank -- prev )
static cell_t tic_forth_vbank(void)
{
    s32 bank = (s32)pfPopFromStack();
    return (cell_t)gForthCore->api.vbank((tic_mem*)gForthCore, bank);
}

// reset ( -- )
static cell_t tic_forth_reset(void)
{
    gForthCore->api.reset((tic_mem*)gForthCore);
    return 0;
}

// key ( keycode -- pressed )
static cell_t tic_forth_key(void)
{
    tic_key k = (tic_key)pfPopFromStack();
    return (cell_t)gForthCore->api.key((tic_mem*)gForthCore, k);
}

// keyp ( keycode hold period -- pressed )
static cell_t tic_forth_keyp(void)
{
    s32     period = (s32)pfPopFromStack();
    s32     hold   = (s32)pfPopFromStack();
    tic_key k      = (tic_key)pfPopFromStack();
    return (cell_t)gForthCore->api.keyp((tic_mem*)gForthCore, k, hold, period);
}

// fget ( sprite_id flag -- bool )
static cell_t tic_forth_fget(void)
{
    u8  flag = (u8)pfPopFromStack();
    s32 id   = (s32)pfPopFromStack();
    return (cell_t)gForthCore->api.fget((tic_mem*)gForthCore, id, flag);
}

// fset ( sprite_id flag value -- )
static cell_t tic_forth_fset(void)
{
    bool value = (bool)pfPopFromStack();
    u8   flag  = (u8)pfPopFromStack();
    s32  id    = (s32)pfPopFromStack();
    gForthCore->api.fset((tic_mem*)gForthCore, id, flag, value);
    return 0;
}

// fft ( start_freq end_freq -- value )  value scaled to 0..65535
static cell_t tic_forth_fft(void)
{
    s32 endFreq   = (s32)pfPopFromStack();
    s32 startFreq = (s32)pfPopFromStack();
    double v = gForthCore->api.fft((tic_mem*)gForthCore, startFreq, endFreq);
    return (cell_t)(s32)(v * 65535.0);
}

// ffts ( start_freq end_freq -- value )  smoothed, value scaled to 0..65535
static cell_t tic_forth_ffts(void)
{
    s32 endFreq   = (s32)pfPopFromStack();
    s32 startFreq = (s32)pfPopFromStack();
    double v = gForthCore->api.ffts((tic_mem*)gForthCore, startFreq, endFreq);
    return (cell_t)(s32)(v * 65535.0);
}


// =============================================================================
// pForth custom function table
//
// IMPORTANT: the order here MUST match the index assignments in
// CompileCustomFunctions() below. Adding a function requires updating both.
// =============================================================================

CFunc0 CustomFunctionTable[] =
{
    (CFunc0)tic_forth_cls,          //  0  CLS
    (CFunc0)tic_forth_print,        //  1  PRINT
    (CFunc0)tic_forth_pix_get,      //  2  PIX
    (CFunc0)tic_forth_pix_set,      //  3  PIX!
    (CFunc0)tic_forth_line,         //  4  LINE
    (CFunc0)tic_forth_rect,         //  5  RECT
    (CFunc0)tic_forth_rectb,        //  6  RECTB
    (CFunc0)tic_forth_spr,          //  7  SPR
    (CFunc0)tic_forth_btn,          //  8  BTN
    (CFunc0)tic_forth_btnp,         //  9  BTNP
    (CFunc0)tic_forth_sfx,          // 10  SFX
    (CFunc0)tic_forth_map,          // 11  MAP
    (CFunc0)tic_forth_mget,         // 12  MGET
    (CFunc0)tic_forth_mset,         // 13  MSET
    (CFunc0)tic_forth_peek,         // 14  PEEK
    (CFunc0)tic_forth_poke,         // 15  POKE
    (CFunc0)tic_forth_peek1,        // 16  PEEK1
    (CFunc0)tic_forth_poke1,        // 17  POKE1
    (CFunc0)tic_forth_peek2,        // 18  PEEK2
    (CFunc0)tic_forth_poke2,        // 19  POKE2
    (CFunc0)tic_forth_peek4,        // 20  PEEK4
    (CFunc0)tic_forth_poke4,        // 21  POKE4
    (CFunc0)tic_forth_memcpy,       // 22  MEMCPY
    (CFunc0)tic_forth_memset,       // 23  MEMSET
    (CFunc0)tic_forth_trace,        // 24  TRACE
    (CFunc0)tic_forth_pmem_get,     // 25  PMEM
    (CFunc0)tic_forth_pmem_set,     // 26  PMEM!
    (CFunc0)tic_forth_time,         // 27  TIME
    (CFunc0)tic_forth_tstamp,       // 28  TSTAMP
    (CFunc0)tic_forth_exit,         // 29  EXITGAME (Forth word; C API is exit)
    (CFunc0)tic_forth_font,         // 30  FONT
    (CFunc0)tic_forth_mouse,        // 31  MOUSE
    (CFunc0)tic_forth_circ,         // 32  CIRC
    (CFunc0)tic_forth_circb,        // 33  CIRCB
    (CFunc0)tic_forth_elli,         // 34  ELLI
    (CFunc0)tic_forth_ellib,        // 35  ELLIB
    (CFunc0)tic_forth_paint,        // 36  PAINT
    (CFunc0)tic_forth_tri,          // 37  TRI
    (CFunc0)tic_forth_trib,         // 38  TRIB
    (CFunc0)tic_forth_ttri,         // 39  TTRI
    (CFunc0)tic_forth_clip,         // 40  CLIP
    (CFunc0)tic_forth_clip0,        // 41  CLIP0
    (CFunc0)tic_forth_music,        // 42  MUSIC
    (CFunc0)tic_forth_sync,         // 43  SYNC
    (CFunc0)tic_forth_vbank,        // 44  VBANK
    (CFunc0)tic_forth_reset,        // 45  RESET
    (CFunc0)tic_forth_key,          // 46  KEYPRESSED (Forth word; C API is key)
    (CFunc0)tic_forth_keyp,         // 47  KEYP
    (CFunc0)tic_forth_fget,         // 48  FGET
    (CFunc0)tic_forth_fset,         // 49  FSET
    (CFunc0)tic_forth_fft,          // 50  FFT
    (CFunc0)tic_forth_ffts,         // 51  FFTS
};

// Called during dictionary build (pfBuildDictionary) and manually after
// pfLoadStaticDictionary to add TIC-80 API words to the dictionary.
// Index values MUST match the CustomFunctionTable order above.
Err CompileCustomFunctions(void)
{
    int i = 0;
    // All words use NumParams=0: wrappers pop arguments themselves.
    if (CreateGlueToC("CLS",    i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("PRINT",  i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("PIX",    i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("PIX!",   i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("LINE",   i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("RECT",   i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("RECTB",  i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("SPR",    i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("BTN",    i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("BTNP",   i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("SFX",    i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("MAP",    i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("MGET",   i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("MSET",   i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("PEEK",   i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("POKE",   i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("PEEK1",  i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("POKE1",  i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("PEEK2",  i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("POKE2",  i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("PEEK4",  i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("POKE4",  i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("MEMCPY", i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("MEMSET", i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("TRACE",  i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("PMEM",   i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("PMEM!",  i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("TIME",   i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("TSTAMP", i++, C_RETURNS_VALUE, 0) < 0) return -1;
    // Renamed from "EXIT": EXIT is a core Forth word (return from the current
    // definition). Binding the TIC-80 quit-to-console API to that name shadowed
    // it, so any "... IF ... EXIT THEN ..." early-return silently quit the cart.
    if (CreateGlueToC("EXITGAME", i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("FONT",   i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("MOUSE",  i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("CIRC",   i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("CIRCB",  i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("ELLI",   i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("ELLIB",  i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("PAINT",  i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("TRI",    i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("TRIB",   i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("TTRI",   i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("CLIP",   i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("CLIP0",  i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("MUSIC",  i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("SYNC",   i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("VBANK",  i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("RESET",  i++, C_RETURNS_VOID,  0) < 0) return -1;
    // Renamed from "KEY": KEY is a standard Forth word (read one character of
    // input). Use KEYPRESSED for TIC-80's "is this key down?" query.
    if (CreateGlueToC("KEYPRESSED", i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("KEYP",   i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("FGET",   i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("FSET",   i++, C_RETURNS_VOID,  0) < 0) return -1;
    if (CreateGlueToC("FFT",    i++, C_RETURNS_VALUE, 0) < 0) return -1;
    if (CreateGlueToC("FFTS",   i++, C_RETURNS_VALUE, 0) < 0) return -1;
    return 0;
}

// Required when PF_NO_GLOBAL_INIT is defined (rare embedded loaders).
Err LoadCustomFunctionTable(void)
{
    return 0;
}

// =============================================================================
// Error reporting helpers
// =============================================================================

static void reportForthError(tic_core* core, ThrowCode code)
{
    if (!core->data) return;

    const char* msg = forthGetOutputBuffer();
    if (msg && msg[0])
        core->data->error(core->data->data, msg);
    else
    {
        // pforth throw codes are negative integers; format a basic message.
        char buf[64];
        snprintf(buf, sizeof(buf), "Forth error: THROW %ld", (long)code);
        core->data->error(core->data->data, buf);
    }
    forthClearOutputBuffer();
}

// =============================================================================
// TIC-80 callback helpers
// =============================================================================

// Validate that the data stack depth is exactly 'expected' after a Forth call.
// Stack imbalance in SCN (called 240 times/frame) would corrupt state.
static void checkStackBalance(tic_core* core, cell_t before, const char* name)
{
    cell_t after = pfGetStackDepth();
    if (after != before && core->data)
    {
        char buf[128];
        cell_t delta = after - before;
        if (delta > 0)
            snprintf(buf, sizeof(buf),
                     "Forth: stack overflow in %s (%ld extra item%s left)",
                     name, (long)delta, delta == 1 ? "" : "s");
        else
            snprintf(buf, sizeof(buf),
                     "Forth: stack underflow in %s (%ld item%s missing)",
                     name, (long)-delta, -delta == 1 ? "" : "s");
        core->data->error(core->data->data, buf);
        // Discard leftover stack items to avoid cascading errors.
        while (pfGetStackDepth() > before)
            pfPopFromStack();
    }
}

static void callForthWord(tic_mem* tic, const char* name, s32 param, bool hasParam)
{
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) return;

    cell_t depthBefore = pfGetStackDepth();

    if (hasParam)
        pfPushToStack((cell_t)param);

    forthClearOutputBuffer();
    ThrowCode result = pfExecIfDefined(name);
    if (result != 0)
        reportForthError(core, result);

    cell_t depthAfter = pfGetStackDepth();

    // If the word was not defined, pfExecIfDefined is a no-op and the pushed
    // param is still on the stack (depthAfter == depthBefore + 1).  That is
    // not a user error — silently clean up and return.
    bool paramLeaked = hasParam && (depthAfter == depthBefore + 1);
    if (paramLeaked)
    {
        pfPopFromStack();
        return;
    }

    // Any other imbalance is a real stack error in the user's word.
    checkStackBalance(core, depthBefore, name);
}

// =============================================================================
// TIC-80 language lifecycle
// =============================================================================

static void closeForth(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    if (core->currentVM)
    {
        forthTermIO();
        // pfTerminate() frees both the current task and the dictionary.
        // Do NOT call pfDeleteTask separately — that would double-free.
        pfTerminate();
        gForthTask     = NULL;
        core->currentVM = NULL;
        gForthCore     = NULL;
    }
}

static bool initForth(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    closeForth(tic);

    gForthCore = core;
    forthInitIO(core->data);
    pfSetQuiet(1);

    // pfInitialize(NULL, 0, NULL):
    //   - calls pfInitSystem() which sets gVarBase=10 and inits the allocator
    //   - creates the execution task and calls pfSetCurrentTask
    //   - loads the pre-compiled static dictionary (pfdicdat.h)
    //   - runs AUTO.INIT if defined (no-op in standard pforth)
    // Using pfInitialize instead of the individual calls is required because
    // pfInitSystem() is private to pf_core.c.
    ThrowCode initResult = pfInitialize(NULL, 0, NULL);
    if (initResult != 0)
    {
        if (core->data)
            core->data->error(core->data->data, "Forth: pfInitialize failed");
        gForthCore = NULL;
        forthTermIO();
        return false;
    }

    gForthTask      = pfGetCurrentTask();
    core->currentVM = gForthTask;

    // Add TIC-80 API words to the already-loaded dictionary.
    if (CompileCustomFunctions() < 0)
    {
        closeForth(tic);
        if (core->data)
            core->data->error(core->data->data,
                              "Forth: failed to compile API words");
        return false;
    }

    // Interpret the cartridge source code.
    forthClearOutputBuffer();
    ThrowCode result = forthInterpretLines(code);
    if (result != 0)
    {
        reportForthError(core, result);
        closeForth(tic);
        return false;
    }

    return true;
}

static void callForthTick(tic_mem* tic)
{
    callForthWord(tic, TIC_FN,  0,     false);
    forthFlushOutput();  // flush '.' output that has no trailing CR
}

static void callForthBoot(tic_mem* tic)
{
    callForthWord(tic, BOOT_FN, 0,     false);
}

static void callForthScanline(tic_mem* tic, s32 row, void* data)
{
    (void)data;
    callForthWord(tic, SCN_FN,  row,   true);
}

static void callForthBorder(tic_mem* tic, s32 row, void* data)
{
    (void)data;
    callForthWord(tic, BDR_FN,  row,   true);
}

static void callForthMenu(tic_mem* tic, s32 index, void* data)
{
    (void)data;
    callForthWord(tic, MENU_FN, index, true);
}

static void evalForth(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM)
    {
        if (!initForth(tic, ""))
            return;
    }
    forthClearOutputBuffer();
    ThrowCode result = forthInterpretLines(code);
    if (result != 0)
        reportForthError(core, result);
}

// =============================================================================
// Editor outline: extract word names from ': NAME ... ;' definitions
// =============================================================================

static const tic_outline_item* getForthOutline(const char* code, s32* size)
{
    // Pattern: lines starting with ': ' introduce a new word definition.
    static tic_outline_item items[128];
    *size = 0;

    const char* p = code;
    while (*p)
    {
        // Skip to next line that starts with ': '
        while (*p && !(*p == ':' && (p == code || *(p-1) == '\n')))
            p++;
        if (!*p) break;

        p++;  // skip ':'
        while (*p == ' ' || *p == '\t') p++;

        const char* start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
            p++;
        const char* end = p;

        if (end > start && *size < 128)
        {
            items[*size].pos  = start;
            items[*size].size = (s32)(end - start);
            (*size)++;
        }

        // Skip to end of line.
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }

    return *size ? items : NULL;
}

// =============================================================================
// Keyword lists for syntax highlighting
// =============================================================================

static const char* const ForthKeywords[] = {
    // ANS Forth core control flow
    "IF", "THEN", "ELSE", "BEGIN", "UNTIL", "WHILE", "REPEAT", "DO", "LOOP",
    "+LOOP", "LEAVE", "EXIT", "RECURSE",
    // Defining words
    ":", ";", "VARIABLE", "CONSTANT", "CREATE", "DOES>", "VALUE", "TO",
    // Stack manipulation
    "DUP", "DROP", "SWAP", "OVER", "ROT", "-ROT", "NIP", "TUCK",
    "2DUP", "2DROP", "2SWAP", "2OVER", "PICK", "ROLL",
    // Arithmetic
    "+", "-", "*", "/", "MOD", "/MOD", "*/", "*/MOD",
    "MAX", "MIN", "ABS", "NEGATE", "1+", "1-", "2*", "2/",
    // Logical / bitwise
    "AND", "OR", "XOR", "INVERT", "LSHIFT", "RSHIFT",
    // Comparison
    "=", "<>", "<", ">", "<=", ">=", "0=", "0<", "0>",
    // Memory
    "@", "!", "C@", "C!", "+!", "W@", "W!", "MOVE",
    // I/O
    ".", ".\"", "EMIT", "CR", "SPACE", "SPACES", "TYPE",
    // String
    "S\"", "C\"", "CHAR",
    // Misc
    "TRUE", "FALSE", "CELL", "HERE", "ALLOT", "CELLS", "CHARS",
    "LITERAL", "POSTPONE", "IMMEDIATE",
};

static const char* ForthAPIKeywords[] = {
#define TIC_CALLBACK_DEF(name, ...) #name,
    TIC_CALLBACK_LIST(TIC_CALLBACK_DEF)
#undef TIC_CALLBACK_DEF
    // TIC-80 API words in UPPERCASE (Forth convention)
    "CLS", "PRINT", "PIX", "PIX!", "LINE", "RECT", "RECTB",
    "SPR", "BTN", "BTNP", "SFX", "MAP", "MGET", "MSET",
    "PEEK", "POKE", "PEEK1", "POKE1", "PEEK2", "POKE2", "PEEK4", "POKE4",
    "MEMCPY", "MEMSET", "TRACE", "PMEM", "PMEM!", "TIME", "TSTAMP", "EXITGAME",
    "FONT", "MOUSE", "CIRC", "CIRCB", "ELLI", "ELLIB", "PAINT",
    "TRI", "TRIB", "TTRI", "CLIP", "CLIP0",
    "MUSIC", "SYNC", "VBANK", "RESET",
    "KEYPRESSED", "KEYP", "FGET", "FSET", "FFT", "FFTS",
};

// =============================================================================
// =============================================================================

static const u8 DemoRom[] =
{
    #include "../build/assets/forthdemo.tic.dat"
};

static const u8 MarkRom[] =
{
    #include "../build/assets/forthmark.tic.dat"
};

// =============================================================================
// tic_script descriptor
// =============================================================================

TIC_EXPORT const tic_script EXPORT_SCRIPT(Forth) =
{
    .id               = 21,
    .name             = "forth",
    .fileExtension    = ".fth",
    .projectComment   = "\\",
    {
        .init         = initForth,
        .close        = closeForth,
        .tick         = callForthTick,
        .boot         = callForthBoot,
        .callback     =
        {
            .scanline = callForthScanline,
            .border   = callForthBorder,
            .menu     = callForthMenu,
        },
    },
    .getOutline       = getForthOutline,
    .eval             = evalForth,

    .blockCommentStart  = NULL,
    .blockCommentEnd    = NULL,
    .blockCommentStart2 = NULL,
    .blockCommentEnd2   = NULL,
    .singleComment      = "\\",
    .blockStringStart   = NULL,
    .blockStringEnd     = NULL,
    .stdStringStartEnd  = NULL,
    .blockEnd           = NULL,

    .keywords         = ForthKeywords,
    .keywordsCount    = COUNT_OF(ForthKeywords),

    .api_keywords     = ForthAPIKeywords,
    .api_keywordsCount = COUNT_OF(ForthAPIKeywords),

    .demo = { DemoRom, sizeof DemoRom, "forthdemo.tic" },
    .mark = { MarkRom, sizeof MarkRom, "forthmark.tic" },
};
