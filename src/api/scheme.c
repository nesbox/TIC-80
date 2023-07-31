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

#include "core/core.h"

#if defined(TIC_BUILD_WITH_SCHEME)

//#define USE_FOREIGN_POINTER

#include <ctype.h>
#include <s7.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* TicCore = "_TIC80";
static const char* TicCoreType = "_TIC80Type";

tic_core* getSchemeCore(s7_scheme* sc)
{
    return s7_c_pointer(s7_name_to_value(sc, TicCore));
}

s7_pointer scheme_print(s7_scheme* sc, s7_pointer args)
{
    //print(text x=0 y=0 color=15 fixed=false scale=1 smallfont=false) -> width
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const char* text = s7_string(s7_car(args));
    const int argn = s7_list_length(sc, args);
    const s32 x = argn > 1 ? s7_integer(s7_cadr(args)) : 0;
    const s32 y = argn > 2 ? s7_integer(s7_caddr(args)) : 0;
    const u8 color = argn > 3 ? s7_integer(s7_cadddr(args)) : 15;
    const bool fixed = argn > 4 ? s7_boolean(sc, s7_list_ref(sc, args, 4)) : false;
    const s32 scale = argn > 5 ? s7_integer(s7_list_ref(sc, args, 5)) : 1;
    const bool alt = argn > 6 ? s7_boolean(sc, s7_list_ref(sc, args, 6)) : false;
    const s32 result = tic_api_print(tic, text, x, y, color, fixed, scale, alt);
    return s7_make_integer(sc, result);
}
s7_pointer scheme_cls(s7_scheme* sc, s7_pointer args)
{
    // cls(color=0)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const int argn = s7_list_length(sc, args);
    const u8 color = (args > 0) ? s7_integer(s7_car(args)) : 0;
    tic_api_cls(tic, color);
    return s7_nil(sc);
}
s7_pointer scheme_pix(s7_scheme* sc, s7_pointer args)
{
    // pix(x y color)\npix(x y) -> color
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 x = s7_integer(s7_car(args));
    const s32 y = s7_integer(s7_cadr(args));

    const int argn = s7_list_length(sc, args);
    if (argn == 3)
    {
        const u8 color = s7_integer(s7_caddr(args));
        tic_api_pix(tic, x, y, color, false);
        return s7_nil(sc);
    }
    else{
        return s7_make_integer(sc, tic_api_pix(tic, x, y, 0, true));
    }
    
}
s7_pointer scheme_line(s7_scheme* sc, s7_pointer args)
{
    // line(x0 y0 x1 y1 color)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 x0 = s7_integer(s7_car(args));
    const s32 y0 = s7_integer(s7_cadr(args));
    const s32 x1 = s7_integer(s7_caddr(args));
    const s32 y1 = s7_integer(s7_cadddr(args));
    const u8 color = s7_integer(s7_list_ref(sc, args, 4));
    tic_api_line(tic, x0, y0, x1, y1, color);
    return s7_nil(sc);
}
s7_pointer scheme_rect(s7_scheme* sc, s7_pointer args)
{
    // rect(x y w h color)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 x = s7_integer(s7_car(args));
    const s32 y = s7_integer(s7_cadr(args));
    const s32 w = s7_integer(s7_caddr(args));
    const s32 h = s7_integer(s7_cadddr(args));
    const u8 color = s7_integer(s7_list_ref(sc, args, 4));
    tic_api_rect(tic, x, y, w, h, color);
    return s7_nil(sc);
}
s7_pointer scheme_rectb(s7_scheme* sc, s7_pointer args)
{
    // rectb(x y w h color)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 x = s7_integer(s7_car(args));
    const s32 y = s7_integer(s7_cadr(args));
    const s32 w = s7_integer(s7_caddr(args));
    const s32 h = s7_integer(s7_cadddr(args));
    const u8 color = s7_integer(s7_list_ref(sc, args, 4));
    tic_api_rectb(tic, x, y, w, h, color);
    return s7_nil(sc);
}

void parseTransparentColorsArg(s7_scheme* sc, s7_pointer colorkey, u8* out_transparent_colors, u8* out_count)
{
    *out_count = 0;
    if (s7_is_list(sc, colorkey))
    {
        const s32 arg_color_count = s7_list_length(sc, colorkey);
        const u8 color_count = arg_color_count < TIC_PALETTE_SIZE ? (u8)arg_color_count : TIC_PALETTE_SIZE;
        for (u8 i=0; i<color_count; ++i)
        {
            s7_pointer c = s7_list_ref(sc, colorkey, i);
            out_transparent_colors[i] = s7_is_integer(c) ? s7_integer(c) : 0;
            ++(*out_count);
        }
    }
    else if (s7_is_integer(colorkey))
    {
        out_transparent_colors[0] = (u8)s7_integer(colorkey);
        *out_count = 1;
    }
}

s7_pointer scheme_spr(s7_scheme* sc, s7_pointer args)
{
    // spr(id x y colorkey=-1 scale=1 flip=0 rotate=0 w=1 h=1)
    const int argn      = s7_list_length(sc, args);
    tic_mem* tic        = (tic_mem*)getSchemeCore(sc);
    const s32 id        = s7_integer(s7_car(args));
    const s32 x         = s7_integer(s7_cadr(args));
    const s32 y         = s7_integer(s7_caddr(args));

    static u8 trans_colors[TIC_PALETTE_SIZE];
    u8 trans_count = 0;
    if (argn > 3)
    {
        s7_pointer colorkey = s7_cadddr(args);
        parseTransparentColorsArg(sc, colorkey, trans_colors, &trans_count);
    }

    const s32 scale     = argn > 4 ? s7_integer(s7_list_ref(sc, args, 4)) : 1;
    const s32 flip      = argn > 5 ? s7_integer(s7_list_ref(sc, args, 5)) : 0;
    const s32 rotate    = argn > 6 ? s7_integer(s7_list_ref(sc, args, 6)) : 0;
    const s32 w         = argn > 7 ? s7_integer(s7_list_ref(sc, args, 7)) : 1;
    const s32 h         = argn > 8 ? s7_integer(s7_list_ref(sc, args, 8)) : 1;
    tic_api_spr(tic, id, x, y, w, h, trans_colors, trans_count, scale, (tic_flip)flip, (tic_rotate) rotate);
    return s7_nil(sc);
}
s7_pointer scheme_btn(s7_scheme* sc, s7_pointer args)
{
    // btn(id) -> pressed
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 id = s7_integer(s7_car(args));
    
    return s7_make_boolean(sc, tic_api_btn(tic, id));
}
s7_pointer scheme_btnp(s7_scheme* sc, s7_pointer args)
{
    // btnp(id hold=-1 period=-1) -> pressed
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 id = s7_integer(s7_car(args));

    const int argn = s7_list_length(sc, args);
    const s32 hold = argn > 1 ? s7_integer(s7_cadr(args)) : -1;
    const s32 period = argn > 2 ? s7_integer(s7_caddr(args)) : -1;
    
    return s7_make_boolean(sc, tic_api_btnp(tic, id, hold, period));
}

u8 get_note_base(char c) {
    switch (c) {
    case 'C': return 0;
    case 'D': return 2;
    case 'E': return 4;
    case 'F': return 5;
    case 'G': return 7;
    case 'A': return 9;
    case 'B': return 11;
    default:  return 255;
    }
}

u8 get_note_modif(char c) {
    switch (c) {
    case '-': return 0;
    case '#': return 1;
    default:  return 255;
    }
}

u8 get_note_octave(char c) {
    if (c >= '0' && c <= '8')
        return c - '0';
    else
        return 255;
}

s7_pointer scheme_sfx(s7_scheme* sc, s7_pointer args)
{
    // sfx(id note=-1 duration=-1 channel=0 volume=15 speed=0)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 id = s7_integer(s7_car(args));
    
    const int argn = s7_list_length(sc, args);
    int note = -1;
    int octave = -1;
    if (argn > 1) {
        s7_pointer note_ptr = s7_cadr(args);
        if (s7_is_integer(note_ptr)) {
            const s32 raw_note = s7_integer(note_ptr);
            if (raw_note >= 0 || raw_note <= 95) {
                note = raw_note % 12;
                octave = raw_note / 12;
            }
            /* else { */
            /*     char buffer[100]; */
            /*     snprintf(buffer, 99, "Invalid sfx note given: %d\n", raw_note); */
            /*     tic->data->error(tic->data->data, buffer); */
            /* } */
        } else if (s7_is_string(note_ptr)) {
            const char* note_str = s7_string(note_ptr);
            const u8 len = s7_string_length(note_ptr);
            if (len == 3) {
                const u8 modif = get_note_modif(note_str[1]);
                note = get_note_base(note_str[0]);
                octave = get_note_octave(note_str[2]);
                if (note < 255 || modif < 255 || octave < 255) {
                    note = note + modif;
                } else {
                    note = octave = 255;
                }
            }
            /* if (note == 255 || octave == 255) { */
            /*     char buffer[100]; */
            /*     snprintf(buffer, 99, "Invalid sfx note given: %s\n", note_str); */
            /*     tic->data->error(tic->data->data, buffer); */
            /* } */
        }
    }
    
    const s32 duration = argn > 2 ? s7_integer(s7_caddr(args)) : -1;
    const s32 channel = argn > 3 ? s7_integer(s7_cadddr(args)) : 0;

    s32 volumes[TIC80_SAMPLE_CHANNELS] = {MAX_VOLUME, MAX_VOLUME};
    if (argn > 4) {
        s7_pointer volume_arg = s7_list_ref(sc, args, 4);
        if (s7_is_integer(volume_arg)) {
            volumes[0] = volumes[1] = s7_integer(volume_arg) & 0xF;
        } else if (s7_is_list(sc, volume_arg) && s7_list_length(sc, volume_arg) == 2) {
            volumes[0] = s7_integer(s7_car(volume_arg)) & 0xF;
            volumes[1] = s7_integer(s7_cadr(volume_arg)) & 0xF;
        }
    }
    const s32 speed = argn > 5 ? s7_integer(s7_list_ref(sc, args, 5)) : 0;

    tic_api_sfx(tic, id, note, octave, duration, channel, volumes[0], volumes[1], speed);
    return s7_nil(sc);
}

typedef struct
{
    s7_scheme* sc;
    s7_pointer callback;
} RemapData;

static void remapCallback(void* data, s32 x, s32 y, RemapResult* result)
{
    RemapData* remap = (RemapData*)data;
    s7_scheme* sc = remap->sc;

    // (callback index x y) -> (list index flip rotate)
    s7_pointer callbackResult = s7_call(sc, remap->callback,
                                        s7_cons(sc, s7_make_integer(sc, result->index),
                                                s7_cons(sc, s7_make_integer(sc, x),
                                                        s7_cons(sc, s7_make_integer(sc, y),
                                                                s7_nil(sc)))));

    if (s7_is_list(sc, callbackResult) && s7_list_length(sc, callbackResult) == 3)
    {
        result->index = s7_integer(s7_car(callbackResult));
        result->flip = (tic_flip)s7_integer(s7_cadr(callbackResult));
        result->rotate = (tic_rotate)s7_integer(s7_caddr(callbackResult));
    }
}

s7_pointer scheme_map(s7_scheme* sc, s7_pointer args)
{
    // map(x=0 y=0 w=30 h=17 sx=0 sy=0 colorkey=-1 scale=1 remap=nil)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 x = s7_integer(s7_car(args));
    const s32 y = s7_integer(s7_cadr(args));
    const s32 w = s7_integer(s7_caddr(args));
    const s32 h = s7_integer(s7_cadddr(args));
    const s32 sx = s7_integer(s7_list_ref(sc, args, 4));
    const s32 sy = s7_integer(s7_list_ref(sc, args, 5));

    const int argn = s7_list_length(sc, args);

    static u8 trans_colors[TIC_PALETTE_SIZE];
    u8 trans_count = 0;
    if (argn > 6) {
        s7_pointer colorkey = s7_list_ref(sc, args, 6);
        parseTransparentColorsArg(sc, colorkey, trans_colors, &trans_count);
    }

    const s32 scale = argn > 7 ? s7_integer(s7_list_ref(sc, args, 7)) : 1;

    RemapFunc remap = NULL;
    RemapData data;
    if (argn > 8)
    {
        remap = remapCallback;
        data.sc = sc;
        data.callback = s7_list_ref(sc, args, 8);
    }
    tic_api_map(tic, x, y, w, h, sx, sy, trans_colors, trans_count, scale, remap, &data);
    return s7_nil(sc);
}
s7_pointer scheme_mget(s7_scheme* sc, s7_pointer args)
{
    // mget(x y) -> tile_id
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 x = s7_integer(s7_car(args));
    const s32 y = s7_integer(s7_cadr(args));
    return s7_make_integer(sc, tic_api_mget(tic, x, y));
}
s7_pointer scheme_mset(s7_scheme* sc, s7_pointer args)
{
    // mset(x y tile_id)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 x = s7_integer(s7_car(args));
    const s32 y = s7_integer(s7_cadr(args));
    const u8 tile_id = s7_integer(s7_caddr(args));
    tic_api_mset(tic, x, y, tile_id);
    return s7_nil(sc);
}
s7_pointer scheme_peek(s7_scheme* sc, s7_pointer args)
{
    // peek(addr bits=8) -> value
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 addr = s7_integer(s7_car(args));
    const int argn = s7_list_length(sc, args);
    const s32 bits = argn > 1 ? s7_integer(s7_cadr(args)) : 8;
    return s7_make_integer(sc, tic_api_peek(tic, addr, bits));
}
s7_pointer scheme_poke(s7_scheme* sc, s7_pointer args)
{
    // poke(addr value bits=8)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 addr = s7_integer(s7_car(args));
    const s32 value = s7_integer(s7_cadr(args));
    const int argn = s7_list_length(sc, args);
    const s32 bits = argn > 2 ? s7_integer(s7_caddr(args)) : 8;
    tic_api_poke(tic, addr, value, bits);
    return s7_nil(sc);
}
s7_pointer scheme_peek1(s7_scheme* sc, s7_pointer args)
{
    // peek1(addr) -> value
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 addr = s7_integer(s7_car(args));
    return s7_make_integer(sc, tic_api_peek1(tic, addr));
}
s7_pointer scheme_poke1(s7_scheme* sc, s7_pointer args)
{
    // poke1(addr value)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 addr = s7_integer(s7_car(args));
    const s32 value = s7_integer(s7_cadr(args));
    tic_api_poke1(tic, addr, value);
    return s7_nil(sc);
}
s7_pointer scheme_peek2(s7_scheme* sc, s7_pointer args)
{
    // peek2(addr) -> value
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 addr = s7_integer(s7_car(args));
    return s7_make_integer(sc, tic_api_peek2(tic, addr));
}
s7_pointer scheme_poke2(s7_scheme* sc, s7_pointer args)
{
    // poke2(addr value)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 addr = s7_integer(s7_car(args));
    const s32 value = s7_integer(s7_cadr(args));
    tic_api_poke2(tic, addr, value);
    return s7_nil(sc);
}
s7_pointer scheme_peek4(s7_scheme* sc, s7_pointer args)
{
    // peek4(addr) -> value
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 addr = s7_integer(s7_car(args));
    return s7_make_integer(sc, tic_api_peek4(tic, addr));
}
s7_pointer scheme_poke4(s7_scheme* sc, s7_pointer args)
{
    // poke4(addr value)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 addr = s7_integer(s7_car(args));
    const s32 value = s7_integer(s7_cadr(args));
    tic_api_poke4(tic, addr, value);
    return s7_nil(sc);
}
s7_pointer scheme_memcpy(s7_scheme* sc, s7_pointer args)
{
    // memcpy(dest source size)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 dest = s7_integer(s7_car(args));
    const s32 source = s7_integer(s7_cadr(args));
    const s32 size = s7_integer(s7_caddr(args));
    tic_api_memcpy(tic, dest, source, size);
    return s7_nil(sc);
}
s7_pointer scheme_memset(s7_scheme* sc, s7_pointer args)
{
    // memset(dest value size)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 dest = s7_integer(s7_car(args));
    const s32 value = s7_integer(s7_cadr(args));
    const s32 size = s7_integer(s7_caddr(args));
    tic_api_memset(tic, dest, value, size);
    return s7_nil(sc);
}
s7_pointer scheme_trace(s7_scheme* sc, s7_pointer args)
{
    // trace(message color=15)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const char* msg = s7_string(s7_car(args));
    const int argn = s7_list_length(sc, args);
    const s32 color = argn > 1 ? s7_integer(s7_cadr(args)) : 15;
    tic_api_trace(tic, msg, color);
    return s7_nil(sc);
}
s7_pointer scheme_pmem(s7_scheme* sc, s7_pointer args)
{
    // pmem(index value)
    // pmem(index) -> value
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 index = s7_integer(s7_car(args));
    const int argn = s7_list_length(sc, args);
    s32 value = 0;
    bool shouldSet = false;
    if (argn > 1)
    {
        value = s7_integer(s7_cadr(args));
        shouldSet = true;
    }
    return s7_make_integer(sc, (s32)tic_api_pmem(tic, index, value, shouldSet));
}
s7_pointer scheme_time(s7_scheme* sc, s7_pointer args)
{
    // time() -> ticks
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    return s7_make_real(sc, tic_api_time(tic));
}
s7_pointer scheme_tstamp(s7_scheme* sc, s7_pointer args)
{
    // tstamp() -> timestamp
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    return s7_make_integer(sc, tic_api_tstamp(tic));
}
s7_pointer scheme_exit(s7_scheme* sc, s7_pointer args)
{
    // exit()
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    tic_api_exit(tic);
    return s7_nil(sc);
}
s7_pointer scheme_font(s7_scheme* sc, s7_pointer args)
{
    // font(text x y chromakey char_width char_height fixed=false scale=1 alt=false) -> width
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const char* text = s7_string(s7_car(args));
    const s32 x = s7_integer(s7_cadr(args));
    const s32 y = s7_integer(s7_caddr(args));

    static u8 trans_colors[TIC_PALETTE_SIZE];
    u8 trans_count = 0;
    s7_pointer colorkey = s7_cadddr(args);
    parseTransparentColorsArg(sc, colorkey, trans_colors, &trans_count);

    const s32 w = s7_integer(s7_list_ref(sc, args, 4));
    const s32 h = s7_integer(s7_list_ref(sc, args, 5));
    const int argn = s7_list_length(sc, args);
    const s32 fixed = argn > 6 ? s7_boolean(sc, s7_list_ref(sc, args, 6)) : false;
    const s32 scale = argn > 7 ? s7_integer(s7_list_ref(sc, args, 7)) : 1;
    const s32 alt = argn > 8 ? s7_boolean(sc, s7_list_ref(sc, args, 8)) : false;

    return s7_make_integer(sc, tic_api_font(tic, text, x, y, trans_colors, trans_count, w, h, fixed, scale, alt));
}
s7_pointer scheme_mouse(s7_scheme* sc, s7_pointer args)
{
    // mouse() -> x y left middle right scrollx scrolly
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const tic_point point = tic_api_mouse(tic);
    const tic80_mouse* mouse = &((tic_core*)tic)->memory.ram->input.mouse;
    
    return
        s7_cons(sc, s7_make_integer(sc, point.x),
                s7_cons(sc, s7_make_integer(sc, point.y),
                        s7_cons(sc, s7_make_integer(sc, mouse->left),
                                s7_cons(sc, s7_make_integer(sc, mouse->middle),
                                        s7_cons(sc, s7_make_integer(sc, mouse->right),
                                                s7_cons(sc, s7_make_integer(sc, mouse->scrollx),
                                                        s7_cons(sc, s7_make_integer(sc, mouse->scrolly),
                                                                s7_nil(sc))))))));
}
s7_pointer scheme_circ(s7_scheme* sc, s7_pointer args)
{
    // circ(x y radius color)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 x = s7_integer(s7_car(args));
    const s32 y = s7_integer(s7_cadr(args));
    const s32 radius = s7_integer(s7_caddr(args));
    const s32 color = s7_integer(s7_cadddr(args));
    tic_api_circ(tic, x, y, radius, color);
    return s7_nil(sc);
}
s7_pointer scheme_circb(s7_scheme* sc, s7_pointer args)
{
    // circb(x y radius color)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 x = s7_integer(s7_car(args));
    const s32 y = s7_integer(s7_cadr(args));
    const s32 radius = s7_integer(s7_caddr(args));
    const s32 color = s7_integer(s7_cadddr(args));
    tic_api_circb(tic, x, y, radius, color);
    return s7_nil(sc);
}
s7_pointer scheme_elli(s7_scheme* sc, s7_pointer args)
{
    // elli(x y a b color)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 x = s7_integer(s7_car(args));
    const s32 y = s7_integer(s7_cadr(args));
    const s32 a = s7_integer(s7_caddr(args));
    const s32 b = s7_integer(s7_cadddr(args));
    const s32 color = s7_integer(s7_list_ref(sc, args, 4));
    tic_api_elli(tic, x, y, a, b, color);
    return s7_nil(sc);
}
s7_pointer scheme_ellib(s7_scheme* sc, s7_pointer args)
{
    // ellib(x y a b color)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 x = s7_integer(s7_car(args));
    const s32 y = s7_integer(s7_cadr(args));
    const s32 a = s7_integer(s7_caddr(args));
    const s32 b = s7_integer(s7_cadddr(args));
    const s32 color = s7_integer(s7_list_ref(sc, args, 4));
    tic_api_ellib(tic, x, y, a, b, color);
    return s7_nil(sc);
}
s7_pointer scheme_tri(s7_scheme* sc, s7_pointer args)
{
    // tri(x1 y1 x2 y2 x3 y3 color)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 x1 = s7_integer(s7_car(args));
    const s32 y1 = s7_integer(s7_cadr(args));
    const s32 x2 = s7_integer(s7_caddr(args));
    const s32 y2 = s7_integer(s7_cadddr(args));
    const s32 x3 = s7_integer(s7_list_ref(sc, args, 4));
    const s32 y3 = s7_integer(s7_list_ref(sc, args, 5));
    const s32 color = s7_integer(s7_list_ref(sc, args, 6));
    tic_api_tri(tic, x1, y1, x2, y2, x3, y3, color);
    return s7_nil(sc);
}
s7_pointer scheme_trib(s7_scheme* sc, s7_pointer args)
{
    // trib(x1 y1 x2 y2 x3 y3 color)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 x1 = s7_integer(s7_car(args));
    const s32 y1 = s7_integer(s7_cadr(args));
    const s32 x2 = s7_integer(s7_caddr(args));
    const s32 y2 = s7_integer(s7_cadddr(args));
    const s32 x3 = s7_integer(s7_list_ref(sc, args, 4));
    const s32 y3 = s7_integer(s7_list_ref(sc, args, 5));
    const s32 color = s7_integer(s7_list_ref(sc, args, 6));
    tic_api_trib(tic, x1, y1, x2, y2, x3, y3, color);
    return s7_nil(sc);
}
s7_pointer scheme_ttri(s7_scheme* sc, s7_pointer args)
{
    // ttri(x1 y1 x2 y2 x3 y3 u1 v1 u2 v2 u3 v3 texsrc=0 chromakey=-1 z1=0 z2=0 z3=0)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 x1 = s7_integer(s7_car(args));
    const s32 y1 = s7_integer(s7_cadr(args));
    const s32 x2 = s7_integer(s7_caddr(args));
    const s32 y2 = s7_integer(s7_cadddr(args));
    const s32 x3 = s7_integer(s7_list_ref(sc, args, 4));
    const s32 y3 = s7_integer(s7_list_ref(sc, args, 5));
    const s32 u1 = s7_integer(s7_list_ref(sc, args, 6));
    const s32 v1 = s7_integer(s7_list_ref(sc, args, 7));
    const s32 u2 = s7_integer(s7_list_ref(sc, args, 8));
    const s32 v2 = s7_integer(s7_list_ref(sc, args, 9));
    const s32 u3 = s7_integer(s7_list_ref(sc, args, 10));
    const s32 v3 = s7_integer(s7_list_ref(sc, args, 11));

    const int argn = s7_list_length(sc, args);
    const tic_texture_src texsrc = (tic_texture_src)(argn > 12 ? s7_integer(s7_list_ref(sc, args, 12)) : 0);
    
    static u8 trans_colors[TIC_PALETTE_SIZE];
    u8 trans_count = 0;

    if (argn > 13)
    {
        s7_pointer colorkey = s7_list_ref(sc, args, 13);
        parseTransparentColorsArg(sc, colorkey, trans_colors, &trans_count);
    }

    bool depth = argn > 14 ? true : false;
    const s32 z1 = argn > 14 ? s7_integer(s7_list_ref(sc, args, 14)) : 0;
    const s32 z2 = argn > 15 ? s7_integer(s7_list_ref(sc, args, 15)) : 0;
    const s32 z3 = argn > 16 ? s7_integer(s7_list_ref(sc, args, 16)) : 0;

    tic_api_ttri(tic, x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, texsrc, trans_colors, trans_count, z1, z2, z3, depth);
    return s7_nil(sc);
}
s7_pointer scheme_clip(s7_scheme* sc, s7_pointer args)
{
    // clip(x y width height)
    // clip()
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const int argn = s7_list_length(sc, args);
    if (argn != 4) {
        tic_api_clip(tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
    } else {
        const s32 x = s7_integer(s7_car(args));
        const s32 y = s7_integer(s7_cadr(args));
        const s32 w = s7_integer(s7_caddr(args));
        const s32 h = s7_integer(s7_cadddr(args));
        tic_api_clip(tic, x, y, w, h);
    }
    return s7_nil(sc);
}
s7_pointer scheme_music(s7_scheme* sc, s7_pointer args)
{
    // music(track=-1 frame=-1 row=-1 loop=true sustain=false tempo=-1 speed=-1)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const int argn = s7_list_length(sc, args);
    const s32 track = argn > 0 ? s7_integer(s7_car(args)) : -1;
    const s32 frame = argn > 1 ? s7_integer(s7_cadr(args)) : -1;
    const s32 row = argn > 2 ? s7_integer(s7_caddr(args)) : -1;
    const bool loop = argn > 3 ? s7_boolean(sc, s7_cadddr(args)) : true;
    const bool sustain = argn > 4 ? s7_boolean(sc, s7_list_ref(sc, args, 4)) : false;
    const s32 tempo = argn > 5 ? s7_integer(s7_list_ref(sc, args, 5)) : -1;
    const s32 speed = argn > 6 ? s7_integer(s7_list_ref(sc, args, 6)) : -1;
    tic_api_music(tic, track, frame, row, loop, sustain, tempo, speed);
    return s7_nil(sc);
}
s7_pointer scheme_sync(s7_scheme* sc, s7_pointer args)
{
    // sync(mask=0 bank=0 tocart=false)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const int argn = s7_list_length(sc, args);
    const u32 mask = argn > 0 ? (u32)s7_integer(s7_car(args)) : 0;
    const s32 bank = argn > 1 ? s7_integer(s7_cadr(args)) : 0;
    const bool tocart = argn > 2 ? s7_boolean(sc, s7_caddr(args)) : false;
    tic_api_sync(tic, mask, bank, tocart);
    return s7_nil(sc);
}
s7_pointer scheme_vbank(s7_scheme* sc, s7_pointer args)
{
    // vbank(bank) -> prev
    // vbank() -> prev
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const int argn = s7_list_length(sc, args);

    const s32 prev = ((tic_core*)tic)->state.vbank.id;
    if (argn == 1) {
        const s32 bank = s7_integer(s7_car(args));
        tic_api_vbank(tic, bank);
    }
    return s7_make_integer(sc, prev);
}
s7_pointer scheme_reset(s7_scheme* sc, s7_pointer args)
{
    // reset()
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    tic_api_reset(tic);
    return s7_nil(sc);
}
s7_pointer scheme_key(s7_scheme* sc, s7_pointer args)
{
    //key(code=-1) -> pressed
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const int argn = s7_list_length(sc, args);
    const tic_key code = argn > 0 ? s7_integer(s7_car(args)) : -1;
    return s7_make_boolean(sc, tic_api_key(tic, code));
}
s7_pointer scheme_keyp(s7_scheme* sc, s7_pointer args)
{
    // keyp(code=-1 hold=-1 period=-1) -> pressed
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const int argn = s7_list_length(sc, args);
    const tic_key code = argn > 0 ? s7_integer(s7_car(args)) : -1;
    const s32 hold = argn > 1 ? s7_integer(s7_cadr(args)) : -1;
    const s32 period = argn > 2 ? s7_integer(s7_caddr(args)) : -1;
    return s7_make_boolean(sc, tic_api_keyp(tic, code, hold, period));
}
s7_pointer scheme_fget(s7_scheme* sc, s7_pointer args)
{
    // fget(sprite_id flag) -> bool
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 sprite_id = s7_integer(s7_car(args));
    const u8 flag = s7_integer(s7_cadr(args));
    return s7_make_boolean(sc, tic_api_fget(tic, sprite_id, flag));
}
s7_pointer scheme_fset(s7_scheme* sc, s7_pointer args)
{
    // fset(sprite_id flag bool)
    tic_mem* tic = (tic_mem*)getSchemeCore(sc);
    const s32 sprite_id = s7_integer(s7_car(args));
    const u8 flag = s7_integer(s7_cadr(args));
    const bool val = s7_boolean(sc, s7_caddr(args));
    tic_api_fset(tic, sprite_id, flag, val);
    return s7_nil(sc); 
}

static void initAPI(tic_core* core)
{
    s7_scheme* sc = core->currentVM;

#define API_FUNC_DEF(name, desc, helpstr, count, reqcount, ...) \
    {scheme_ ## name, desc  "\n" helpstr, count, reqcount, "t80::" #name},
    
    static const struct{s7_function func; const char* helpstr; int count; int reqcount; const char* name;} ApiItems[] =
        {TIC_API_LIST(API_FUNC_DEF)};
    
#undef API_FUNC_DEF

    for (s32 i = 0; i < COUNT_OF(ApiItems); i++) {
        s7_define_function(sc,
                           ApiItems[i].name,
                           ApiItems[i].func,
                           ApiItems[i].reqcount,
                           ApiItems[i].count - ApiItems[i].reqcount, // opt count
                           false, // rest_arg
                           ApiItems[i].helpstr);
    }
}

static void closeScheme(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;

    if(core->currentVM)
    {
        s7_free(core->currentVM);
        core->currentVM = NULL;
    }
}

s7_pointer scheme_error_handler(s7_scheme* sc, s7_pointer args)
{
    tic_core* tic = getSchemeCore(sc);
    if (tic->data) {
        tic->data->error(tic->data->data, s7_string(s7_car(args)));
    }
    return s7_nil(sc);
}

static const char* ticFnName = "TIC";

static const char* defstructStr = "  \n\
(define-macro (defstruct name . args) \n\
  (define constructor-name (string->symbol (string-append \"make-\" (symbol->string name)))) \n\
  (define (getter-name argname) (string->symbol (string-append (symbol->string name) \"-\" (symbol->string argname)))) \n\
  (define (setter-name argname) (string->symbol (string-append (symbol->string name) \"-set-\" (symbol->string argname) \"!\"))) \n\
  `(begin (define (,constructor-name . ctr-args) \n\
            (define (list-ref-default l pos def) (if (< pos (length l)) (list-ref l pos) def)) \n\
            (vector ,@(let loop ((i (- (length args) 1)) (inits '())) \n\
                        (if (>= i 0) \n\
                            (loop (- i 1) (cons `(list-ref-default ctr-args ,i ,(let ((a (list-ref args i))) \n\
                                                                                  (if (list? a) (list-ref a 1) #f))) \n\
                                                inits)) \n\
                            inits)))) \n\
          ,@(let loop ((i 0) (remaining-args args) (functions '())) \n\
              (if (not (null? remaining-args)) \n\
                  (let ((argname (if (list? (car remaining-args)) (caar remaining-args) (car remaining-args)))) \n\
                    (loop (+ i 1) \n\
                          (cdr remaining-args) \n\
                          (cons `(begin (define (,(getter-name argname) obj) (vector-ref obj ,i)) \n\
                                        (define (,(setter-name argname) obj val) (vector-set! obj ,i val))) \n\
                                functions))) \n\
                  (reverse functions))))) \n\
";

static bool initScheme(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    closeScheme(tic);

    s7_scheme* sc = core->currentVM = s7_init();
    initAPI(core);

    s7_define_function(sc, "__TIC_ErrorHandler", scheme_error_handler, 1, 0, 0, NULL);
    s7_eval_c_string(sc, "(set! (hook-functions *error-hook*)                    \n\
                            (list (lambda (hook)                                 \n\
                                    (__TIC_ErrorHandler                          \n\
                                      (format #f \"~s: ~a\n--STACKTRACE--\n~a\" ((owlet) 'error-type) (apply format #f (hook 'data)) (stacktrace)))   \n\
                                    (set! (hook 'result) #f))))");
    s7_eval_c_string(sc, defstructStr);

    s7_define_variable(sc, TicCore, s7_make_c_pointer(sc, core));
    s7_load_c_string(sc, code, strlen(code));
    

    const bool isTicDefined = s7_is_defined(sc, ticFnName);
    if (!isTicDefined) {
        if (core->data) {
            core->data->error(core->data->data, "TIC function is not defined");
        }
    }

    return true;
}

static void callSchemeTick(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    s7_scheme* sc = core->currentVM;

    const bool isTicDefined = s7_is_defined(sc, ticFnName);
    if (isTicDefined) {
        s7_call(sc, s7_name_to_value(sc, ticFnName), s7_nil(sc));
    }
}

static void callSchemeBoot(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    s7_scheme* sc = core->currentVM;

    static const char* bootFnName = "BOOT";
    const bool isBootDefined = s7_is_defined(sc, bootFnName);
    if (isBootDefined) {
        s7_call(sc, s7_name_to_value(sc, "BOOT"), s7_nil(sc));
    }
}

static void callSchemeScanline(tic_mem* tic, s32 row, void* data)
{
    tic_core* core = (tic_core*)tic;
    s7_scheme* sc = core->currentVM;

    static const char* scnFnName = "SCN";
    const bool isScnDefined = s7_is_defined(sc, scnFnName);
    if (isScnDefined) {
        s7_call(sc, s7_name_to_value(sc, scnFnName), s7_cons(sc, s7_make_integer(sc, row), s7_nil(sc)));
    }
}

static void callSchemeBorder(tic_mem* tic, s32 row, void* data)
{
    tic_core* core = (tic_core*)tic;
    s7_scheme* sc = core->currentVM;

    static const char* bdrFnName = "BDR";
    bool isBdrDefined = s7_is_defined(sc, bdrFnName);
    if (isBdrDefined) {
        s7_call(sc, s7_name_to_value(sc, bdrFnName), s7_cons(sc, s7_make_integer(sc, row), s7_nil(sc)));
    }
}

static void callSchemeMenu(tic_mem* tic, s32 index, void* data)
{
    tic_core* core = (tic_core*)tic;
    s7_scheme* sc = core->currentVM;

    static const char* menuFnName = "MENU";
    bool isMenuDefined = s7_is_defined(sc, menuFnName);
    if (isMenuDefined) {
        s7_call(sc, s7_name_to_value(sc, menuFnName), s7_cons(sc, s7_make_integer(sc, index), s7_nil(sc)));
    }
}

static const char* const SchemeKeywords [] =
{
    "define", "lambda", "begin", "set!", "=", "<", "<=", ">", ">=", "+", "*",
    "/", "'", "`", "`@", "define-macro", "let", "let*", "letrec", "defstruct",
    "if", "cond", "floor", "ceiling", "sin", "cos", "log", "sqrt", "abs"
    "logand", "logior", "logxor", "lognot", "logbit?", "sinh", "cosh", "tanh",
    "asinh", "acosh", "atanh", "nan?", "infinite?", "nan",
    "exp", "expt", "tan", "acos", "asin", "atan", "truncate", "round",
    "exact->inexact", "inexact->exact", "exact?", "inexact?",
    "modulo", "quotient", "remainder", "gcd", "lcm", "and", "or",
    "eq?", "eqv?", "equal?", "equivalent?", "boolean?", "pair?",
    "cons", "car", "cdr", "set-car!", "set-cdr!", "cadr", "cddr",
    "cdar", "caar", "caadr", "caddr", "cadar", "caaar", "cdadr",
    "cdddr", "caddar", "cadaar", "cdaadr", "cdaddr", "cdadar",
    "cdaaar", "cddadr", "cddddr", "cdddar", "cddaar", "list?"
    "proper-list?", "length", "make-list", "list", "reverse",
    "map", "for-each", "list->vector", "list->string", "list-ref", "null?",
    "not", "number->string", "odd?", "even?", "zero?",
    "append", "list-ref", "assoc", "assq", "member", "memq", "memv",
    "tree-memq", "string?", "string-length", "character?",
    "number?", "integer?", "real?", "rational?", "rationalize",
    "numerator", "denominator", "random", "random-state",
    "random-state->list", "random-state?", "complex?",
    "real-part", "imag-part", "number->string", "vector?",
    "vector-length", "float-vector?", "int-vector?",
    "byte-vector?", "vector-ref", "vector-set!", "make-vector",
    "vector-fill!", "vector->list", "make-hash-table",
    "hash-table-ref", "hash-table-set!", "hash-code",
    "hook-functions", "open-input-string", "open-output-string",
    "get-output-string", "read-char", "peek-char", "read",
    "newline", "write-char", "write", "display", "format",
    "syntax?", "symbol?", "symbol->string", "string->symbol",
    "gensym", "keyword?", "string->keyword", "keyword->string",
    "rootlet", "curlet", "outlet", "sublet", "inlet", "let->list",
    "let?", "let-ref", "openlet", "openlet?"
};

static inline bool scheme_isalnum(char c)
{
    return isalnum(c) || c == '_' || c == '-' || c == ':' || c == '#' || c == '!'
        || c == '+' || c == '=' || c == '&' || c == '^' || c == '%' || c == '$' || c == '@';
}

static const tic_outline_item* getSchemeOutline(const char* code, s32* size)
{
    enum{Size = sizeof(tic_outline_item)};
    *size = 0;

    static tic_outline_item* items = NULL;

    if(items)
    {
        free(items);
        items = NULL;
    }

    const char* ptr = code;

    while(true)
    {
        static const char FuncString[] = "(define (";

        ptr = strstr(ptr, FuncString);

        if(ptr)
        {
            ptr += sizeof FuncString - 1;

            const char* start = ptr;
            const char* end = start;

            while(*ptr)
            {
                char c = *ptr;

                if(scheme_isalnum(c));
                else
                {
                    end = ptr;
                    break;
                }
                ptr++;
            }

            if(end > start)
            {
                items = realloc(items, (*size + 1) * Size);

                items[*size].pos = start;
                items[*size].size = (s32)(end - start);

                (*size)++;
            }
        }
        else break;
    }

    return items;
}

void evalScheme(tic_mem* tic, const char* code) {
    tic_core* core = (tic_core*)tic;
    s7_scheme* sc = core->currentVM;

    // make sure that the Scheme interpreter is initialized.
    if (sc == NULL)
    {
        if (!initScheme(tic, ""))
            return;
        sc = core->currentVM;
    }
    
    s7_eval_c_string(sc, code);
}

static const char* SchemeAPIKeywords[] = {
#define TIC_CALLBACK_DEF(name, ...) #name,
        TIC_CALLBACK_LIST(TIC_CALLBACK_DEF)
#undef  TIC_CALLBACK_DEF

#define API_KEYWORD_DEF(name, ...) "t80::" #name,
        TIC_API_LIST(API_KEYWORD_DEF)
#undef  API_KEYWORD_DEF
};

tic_script_config SchemeSyntaxConfig =
{
    .id                     = 19,
    .name                   = "scheme",
    .fileExtension          = ".scm",
    .projectComment         = ";;",
    {
      .init                 = initScheme,
      .close                = closeScheme,
      .tick                 = callSchemeTick,
      .boot                 = callSchemeBoot,

      .callback             =
      {
        .scanline           = callSchemeScanline,
        .border             = callSchemeBorder,
        .menu               = callSchemeMenu,
      },
    },

    .getOutline             = getSchemeOutline,
    .eval                   = evalScheme,

    .blockCommentStart      = NULL,
    .blockCommentEnd        = NULL,
    .blockCommentStart2     = NULL,
    .blockCommentEnd2       = NULL,
    .singleComment          = ";;",
    .blockStringStart       = "\"",
    .blockStringEnd         = "\"",
    .stdStringStartEnd      = "\"",
    .blockEnd               = NULL,
    .lang_isalnum           = scheme_isalnum,
    .api_keywords           = SchemeAPIKeywords,
    .api_keywordsCount      = COUNT_OF(SchemeAPIKeywords),
    .useStructuredEdition   = true,

    .keywords               = SchemeKeywords,
    .keywordsCount          = COUNT_OF(SchemeKeywords),
};

#endif /* defined(TIC_BUILD_WITH_SCHEME) */
