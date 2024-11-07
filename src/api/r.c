/* MIT License
 *
 * Copyright © 2024 Bryce Carson
 *
 * GitHub: bryce-carson
 * Email: bryce.a.carson@gmail.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "core/core.h"

#define R_NO_REMAP
#include <R.h>

#if !defined R_INTERNALS_H_
#include <Rinternals.h> /* defines LibExtern SEXP R_GlobalEnv */
#endif

#include <Rembedded.h>
#include <Rinterface.h>
#include <R_ext/Parse.h>
#include <Rconfig.h>
#include <Rdefines.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api/renv.h"

/* API */
bool R_initialized_once = false;
extern int R_running_as_main_program;   /* location within The R sources: ../unix/system.c */
static bool initR(tic_mem *tic, const char *code);
tic_mem *mem;
static SEXP RTicRam; /* An external pointer (to an object of type tic_mem) to the object with the symbol "mem". */
#define TICAPI(CMD, ...) ((tic_core *)RTicRam)->api.CMD(R_ExternalPtrAddr(RTicRam) __VA_OPT__(,) __VA_ARGS__)
#define drIntp(x)  *((int   *)x)
#define drDblp(x)  *((int   *)x)
#define drLglp(x)  *((int   *)x)
#define drCrap(x)  *((char **)x)
#define ARGS(x) Rf_elt(args, x)
#define RSTRT(x) SEXP x(SEXP args) { int protected_count = 0; const int argn = Rf_length(args);
#define RUNP UNPROTECT(protected_count);
#define REND }
#define ProtectAndIncrement(x) (++protected_count, PROTECT(x))
#define psint(x) ProtectAndIncrement(Rf_ScalarInteger(x))
#define pslgl(x) ProtectAndIncrement(Rf_ScalarLogical(x))
#define psstr(x) ProtectAndIncrement(Rf_ScalarString(x))
#define EVALG(x) R_ParseEvalString(x, R_GlobalEnv)
void parseTransparentColorsArg(/* LISTSXP */SEXP colorkey,
                               u8* out_transparent_colors,
                               u8* out_count)
{
  *out_count = 0;
  if ((bool) Rf_isList(colorkey))
  {
    const u8 arg_color_count = Rf_length(colorkey);
    const u8 color_count = arg_color_count < TIC_PALETTE_SIZE ? (u8)arg_color_count : TIC_PALETTE_SIZE;
    for (int i=0; i<color_count; ++i)
    {
      SEXP c = VECTOR_ELT(colorkey, i);
      out_transparent_colors[i] = (bool) Rf_isInteger(c) ? drIntp(c) : 0;
      ++(*out_count);
    }
  }
  else if ((bool) Rf_isInteger(colorkey))
  {
    out_transparent_colors[0] = (u8) drIntp(colorkey);
    *out_count = 1;
  }
}
SEXP r_circ(SEXP args) {
  // circ(x y radius color)
  const s32 x      = drIntp(ARGS(1));
  const s32 y      = drIntp(ARGS(2));
  const s32 radius = drIntp(ARGS(3));
  const s32 color  = drIntp(ARGS(4));

  /* RTICAPI.circ(RTICRAM, x, y, radius, color); */
  TICAPI(circ, x, y, radius, color);
  return R_NilValue;
}

SEXP r_circb(SEXP args) {
  // circb(x y radius color)
  const s32 x      = drIntp(ARGS(1));
  const s32 y      = drIntp(ARGS(2));
  const s32 radius = drIntp(ARGS(3));
  const s32 color  = drIntp(ARGS(4));

  TICAPI(circb, x, y, radius, color);
  return R_NilValue;
}
SEXP r_elli(SEXP args)
{
  // elli(x y a b color)
  const s32 x     = drIntp(ARGS(1));
  const s32 y     = drIntp(ARGS(2));
  const s32 a     = drIntp(ARGS(3));
  const s32 b     = drIntp(ARGS(4));
  const s32 color = drIntp(ARGS(5));
  TICAPI(elli, x, y, a, b, color);
  return R_NilValue;
}

SEXP r_ellib(SEXP args)
{
  // ellib(x y a b color)
  const s32 x     = drIntp(ARGS(1));
  const s32 y     = drIntp(ARGS(2));
  const s32 a     = drIntp(ARGS(3));
  const s32 b     = drIntp(ARGS(4));
  const s32 color = drIntp(ARGS(5));
  TICAPI(ellib, x, y, a, b, color);
  return R_NilValue;
}
SEXP r_tri(SEXP args)
{
  // tri(x1 y1 x2 y2 x3 y3 color)
  const s32 x1    = drIntp(ARGS(1));
  const s32 y1    = drIntp(ARGS(2));
  const s32 x2    = drIntp(ARGS(3));
  const s32 y2    = drIntp(ARGS(4));
  const s32 x3    = drIntp(ARGS(5));
  const s32 y3    = drIntp(ARGS(6));
  const s32 color = drIntp(ARGS(7));

  TICAPI(tri, x1, y1, x2, y2, x3, y3, color);
  return R_NilValue;
}

SEXP r_trib(SEXP args)
{
  // trib(x1 y1 x2 y2 x3 y3 color)
  const s32 x1    = drIntp(ARGS(1));
  const s32 y1    = drIntp(ARGS(2));
  const s32 x2    = drIntp(ARGS(4));
  const s32 y2    = drIntp(ARGS(5));
  const s32 x3    = drIntp(ARGS(6));
  const s32 y3    = drIntp(ARGS(7));
  const s32 color = drIntp(ARGS(8));
  TICAPI(tri, x1, y1, x2, y2, x3, y3, color);
  return R_NilValue;
}

SEXP r_ttri(SEXP args)
{
  /* ttri(x1 y1 x2 y2 x3 y3 u1 v1 u2 v2 u3 v3
          texsrc=0 chromakey=-1 z1=0 z2=0 z3=0)
     ⮑ nil */
  const s32 x1 = drIntp(ARGS(1));
  const s32 y1 = drIntp(ARGS(2));
  const s32 x2 = drIntp(ARGS(3));
  const s32 y2 = drIntp(ARGS(4));
  const s32 x3 = drIntp(ARGS(5));
  const s32 y3 = drIntp(ARGS(6));
  const s32 u1 = drIntp(ARGS(7));
  const s32 v1 = drIntp(ARGS(8));
  const s32 u2 = drIntp(ARGS(9));
  const s32 v2 = drIntp(ARGS(10));
  const s32 u3 = drIntp(ARGS(11));
  const s32 v3 = drIntp(ARGS(12));

  const int argn = Rf_length(args);
  const tic_texture_src texsrc = (tic_texture_src)(argn > 12 ? drIntp(ARGS(13)) : 0);

  static u8 trans_colors[TIC_PALETTE_SIZE];
  u8 trans_count = 0;

  if (argn > 13)
  {
    /* Scheme uses expects this to be a list, so I will too? Properly, however,
     * it should be an expression type... which would be `{`. */
    SEXP colorkey = ARGS(14);
    parseTransparentColorsArg(colorkey, trans_colors, &trans_count);
  }

  bool depth = argn > 14 ? true : false;
  const s32 z1 = argn > 14 ? drIntp(ARGS(15)) : 0;
  const s32 z2 = argn > 15 ? drIntp(ARGS(16)) : 0;
  const s32 z3 = argn > 16 ? drIntp(ARGS(17)) : 0;

  TICAPI(ttri, x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3,
         texsrc, trans_colors, trans_count, z1, z2, z3, depth);
  return R_NilValue;
}
SEXP r_clip(SEXP args)
{
  // clip(x y width height)
  // clip()
  const int argn = Rf_length(args);
  if (argn != 4) {
    TICAPI(clip, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
  } else {
    const s32 x = drIntp(ARGS(1));
    const s32 y = drIntp(ARGS(2));
    const s32 w = drIntp(ARGS(3));
    const s32 h = drIntp(ARGS(4));
    TICAPI(clip, x, y, w, h);
  }
  return R_NilValue;
}
SEXP r_font(SEXP args)
{
  // font(text x y chromakey char_width char_height fixed=false scale=1 alt=false) -> width
  const char* text = drCrap(ARGS(1));
  const s32 x      = drIntp(ARGS(2));
  const s32 y      = drIntp(ARGS(3));

  static u8 trans_colors[TIC_PALETTE_SIZE];
  u8 trans_count = 0;
  SEXP colorkey = ARGS(4);
  parseTransparentColorsArg(colorkey, trans_colors, &trans_count);

  const s32 w = drIntp(ARGS(5));
  const s32 h = drIntp(ARGS(6));

  const int argn = Rf_length(args);
  const s32 fixed = argn > 6 ? drLglp(ARGS(7)) : false;
  const s32 scale = argn > 7 ? drIntp(ARGS(8)) : 1;
  const s32 alt   = argn > 6 ? drLglp(ARGS(9)) : false;

  return Rf_ScalarInteger(TICAPI(font, text, x, y, trans_colors, trans_count, w, h, fixed, scale, alt));
}
SEXP r_spr(SEXP args) {
  // spr(id x y colorkey=-1 scale=1 flip=0 rotate=0 w=1 h=1)
  static u8 trans_colors[TIC_PALETTE_SIZE];
  u8 trans_count = 0;

  const s32 id = drIntp(ARGS(1));
  const s32 x  = drIntp(ARGS(2));
  const s32 y  = drIntp(ARGS(3));

  const int argn = Rf_length(args);

  if (argn > 3)
  {
    /* DONE: DOES NOT NEED protection and unprotection becausse the LISTSXP is a
     * part of the args. */
    SEXP colorkey = ARGS(4);
    parseTransparentColorsArg(
      /*Within the Scheme API*/ colorkey, /*is a LIST, so here it should be a LISTSXP.*/
      trans_colors,
      &trans_count
    );
  }
  const s32 scale     = argn > 4 ? drIntp(ARGS(5)) : 1;
  const s32 flip      = argn > 5 ? drIntp(ARGS(6)) : 0;
  const s32 rotate    = argn > 6 ? drIntp(ARGS(7)) : 0;
  const s32 w         = argn > 7 ? drIntp(ARGS(8)) : 1;
  const s32 h         = argn > 8 ? drIntp(ARGS(9)) : 1;
  TICAPI(spr, id, x, y,
         w, h, trans_colors, trans_count, scale,
         (tic_flip) flip,
         (tic_rotate) rotate);
  return R_NilValue;
}
SEXP r_print(SEXP args) {
  /* print(text x=0 y=0 color=15 fixed=false scale=1 smallfont=false)
   * ⮑ width*/
  const int argn = Rf_length(args);
  const char *text  = (const char *) drCrap(ARGS(1));
  const s32   x     = argn > 1 ? drIntp(ARGS(2)): 0;
  const s32   y     = argn > 2 ? drIntp(ARGS(3)): 0;
  const u8    color = argn > 3 ? drIntp(ARGS(4)): 15;
  const bool  fixed = argn > 4 ? drIntp(ARGS(5)): false;
  const s32   scale = argn > 5 ? drIntp(ARGS(6)): 1;
  const bool  alt   = argn > 6 ? drIntp(ARGS(7)): false;

  return Rf_ScalarLogical(TICAPI(print, text, x, y, color, fixed, scale, alt));
}
SEXP r_cls(SEXP args) {
  /* cls(color=0)
   * ⮑ nil */
  const int argn = Rf_length(args);
  const u8 color = (argn > 0) ? drIntp(ARGS(1)) : 0;
  TICAPI(cls, color);

  return R_NilValue;
}
SEXP r_pix(SEXP args) {
  /* pix(x y color = 0)
	 * ⮑ nil
	 * pix(x y)
   * ⮑ color */
  const s32 x = drIntp(ARGS(1));
  const s32 y = drIntp(ARGS(2));

  const int argn = Rf_length(args);
  if (argn == 3) {
    const u8 color = drIntp(ARGS(3));
    TICAPI(pix, x, y, color, false);
    return R_NilValue;
  } else {
    return Rf_ScalarInteger(TICAPI(pix, x, y, 0, true));
  }
}
SEXP r_line(SEXP args) {
  /* line(x0 y0 x1 y1 color)
	 * ⮑ nil */
  const s32 x0    = drIntp(ARGS(1));
  const s32 y0    = drIntp(ARGS(2));
  const s32 x1    = drIntp(ARGS(3));
  const s32 y1    = drIntp(ARGS(4));
  const u8  color = drIntp(ARGS(5));

  TICAPI(line, x0, y0, x1, y1, color);
  return R_NilValue;
}
SEXP r_rect(SEXP args)
{
  // rect(x y w h color)
  const s32 x     = drIntp(ARGS(1));
  const s32 y     = drIntp(ARGS(2));
  const s32 w     = drIntp(ARGS(3));
  const s32 h     = drIntp(ARGS(4));
  const u8  color = drIntp(ARGS(5));
  TICAPI(rect, x, y, w, h, color);
  return R_NilValue;
}

SEXP r_rectb(SEXP args)
{
  // rectb(x y w h color)
  const s32 x     = drIntp(ARGS(1));
  const s32 y     = drIntp(ARGS(2));
  const s32 w     = drIntp(ARGS(3));
  const s32 h     = drIntp(ARGS(4));
  const u8  color = drIntp(ARGS(5));
  TICAPI(rectb, x, y, w, h, color);
  return R_NilValue;
}
typedef struct
{
  tic_core *core;
  SEXP callback;
} RemapData;

static void remapCallback(void* data, s32 x, s32 y, RemapResult* result)
{
  RemapData* remap = (RemapData*) data;
  tic_core* core = remap->core;

  /* NOTE: Call the callback function. */
  // callback(index x y) -> list(index flip rotate)
  /* callback(index, x, y) */
  if (Rf_isFunction(remap->callback)) {
    SEXP callbackResult = Rf_lang4(remap->callback,
                                   Rf_ScalarInteger(result->index),
                                   Rf_ScalarInteger(x),
                                   Rf_ScalarInteger(y));

    if (Rf_isList(callbackResult) && Rf_length(callbackResult) == 3)
    {
      result->index  =              drIntp(Rf_elt(callbackResult, 1));
      result->flip   = (tic_flip)   drIntp(Rf_elt(callbackResult, 2));
      result->rotate = (tic_rotate) drIntp(Rf_elt(callbackResult, 3));
    }
  }
}

SEXP r_map(SEXP args)
{
  // map(x=0 y=0 w=30 h=17 sx=0 sy=0 colorkey=-1 scale=1 remap=nil)
  const s32 x  = drIntp(ARGS(1));
  const s32 y  = drIntp(ARGS(2));
  const s32 w  = drIntp(ARGS(3));
  const s32 h  = drIntp(ARGS(4));
  const s32 sx = drIntp(ARGS(5));
  const s32 sy = drIntp(ARGS(6));

  const int argn = Rf_length(args);

  static u8 trans_colors[TIC_PALETTE_SIZE];
  u8 trans_count = 0;
  if (argn > 6) {
    SEXP colorkey = ARGS(7);
    parseTransparentColorsArg(colorkey, trans_colors, &trans_count);
  }

  const s32 scale = argn > 7 ? drIntp(ARGS(8)) : 1;

  RemapFunc remap = NULL;
  RemapData data;
  if (argn > 8)
  {
    remap = remapCallback;
    data.core = ((tic_core *)RTicRam);
    data.callback = ARGS(9);
  }
  TICAPI(map, x, y, w, h, sx, sy, trans_colors, trans_count, scale, remap, &data);
  return R_NilValue;
}
SEXP r_key(SEXP args)
{
  //key(code=-1) -> pressed
  const int argn = Rf_length(args);
  const tic_key code = argn > 0 ? drIntp(ARGS(1)) : -1;
  return Rf_ScalarLogical(TICAPI(key, code));
}

SEXP r_keyp(SEXP args)
{
  // keyp(code=-1 hold=-1 period=-1) -> pressed
  const int argn = Rf_length(args);
  const tic_key code = argn > 0 ? drIntp(ARGS(1)) : -1;
  const s32 hold     = argn > 1 ? drIntp(ARGS(2)) : -1;
  const s32 period   = argn > 2 ? drIntp(ARGS(3)) : -1;
  return Rf_ScalarLogical(TICAPI(keyp, code, hold, period));
}
/* This API function does not use the convenience macros because it doesn't need
 * to protect any values from garbage collection, because every use of an R API
 * call is to manipulate a C value which R does not control the memory of. */
SEXP r_mouse(SEXP args)
{
  /* mouse()
	 * ⮑ x y left middle right scrollx scrolly */
	tic_mem *tic = (tic_mem *) RTicRam;
	tic_core* core = (tic_core *)tic;

  const tic_point point = core->api.mouse(tic);
  const tic80_mouse* mouse = &((tic_core*)tic)->memory.ram->input.mouse;

  return CONS(Rf_ScalarInteger(point.x),
              Rf_list6(Rf_ScalarInteger(point.y),
                    Rf_ScalarInteger(mouse->left),
                    Rf_ScalarInteger(mouse->middle),
                    Rf_ScalarInteger(mouse->right),
                    Rf_ScalarInteger(mouse->scrollx),
                    Rf_ScalarInteger(mouse->scrolly)));
}
SEXP r_btn(SEXP args)
{
  // btn(id) -> pressed
  return Rf_ScalarLogical(TICAPI(btn, (s32) drIntp(ARGS(1))));
}

SEXP r_btnp(SEXP args)
{
  // btnp(id hold=-1 period=-1) -> pressed
  const s32 id = drIntp(ARGS(1));
  const int argn = Rf_length(args);
  const s32 hold = argn > 1 ? drIntp(ARGS(2)) : -1;
  const s32 period = argn > 2 ? drIntp(ARGS(3)) : -1;

  return Rf_ScalarLogical(TICAPI(btnp, id, hold, period));
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

SEXP r_music(SEXP args)
{
  // music(track=-1 frame=-1 row=-1 loop=true sustain=false tempo=-1 speed=-1)
  const int argn     = Rf_length(args);
  const s32  track   = argn > 0 ? drIntp(ARGS(1)) : -1;
  const s32  frame   = argn > 1 ? drIntp(ARGS(2)) : -1;
  const s32  row     = argn > 2 ? drIntp(ARGS(3)) : -1;
  const bool loop    = argn > 3 ? drLglp(ARGS(4)) : true;
  const bool sustain = argn > 4 ? drLglp(ARGS(5)) : false;
  const s32  tempo   = argn > 5 ? drIntp(ARGS(6)) : -1;
  const s32  speed   = argn > 6 ? drIntp(ARGS(7)) : -1;
  TICAPI(music, track, frame, row, loop, sustain, tempo, speed);
  return R_NilValue;
}

SEXP r_sfx(SEXP a, SEXP args)
{
  // sfx(id note=-1 duration=-1 channel=0 volume=15 speed=0)
  const s32 id = drIntp(ARGS(1));
  const int argn = Rf_length(args);
  int note = -1;
  int octave = -1;
  if (argn > 1) {
    SEXP note_ptr = ARGS(2);
    if (Rf_isInteger(note_ptr)) {
      const s32 raw_note = drIntp(note_ptr);
      if (raw_note >= 0 || raw_note <= 95) {
        note = raw_note % 12;
        octave = raw_note / 12;
      }
      /* else { */
      /*     char buffer[100]; */
      /*     snprintf(buffer, 99, "Invalid sfx note given: %d\n", raw_note); */
      /*     tic->data->error(tic->data->data, buffer); */
      /* } */
    } else if (/*I don't see the function*/ Rf_isString(note_ptr) /*documented in the info manual, but apparently it exists!*/) {
      const char* note_str = drCrap(note_ptr);
      const u8 len = Rf_length(note_ptr);
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

  const s32 duration = argn > 2 ? drIntp(ARGS(3)) : -1;
  const s32 channel  = argn > 3 ? drIntp(ARGS(4)) : 0;

  s32 volumes[TIC80_SAMPLE_CHANNELS] = { MAX_VOLUME, MAX_VOLUME };
  if (argn > 4) {
    SEXP volume_arg = ARGS(5);
    if (Rf_isInteger(volume_arg)) {
      volumes[0] = volumes[1] = drIntp(volume_arg) & 0xF;
    } else if (Rf_isList(volume_arg) && Rf_length(volume_arg) == 2) {
      volumes[0] = drIntp(CADR(volume_arg)) & 0xF;
      volumes[1] = drIntp(CADDR(volume_arg)) & 0xF;
    }
  }
  const s32 speed = argn > 5 ? drIntp(ARGS(6)) : 0;

  TICAPI(sfx, id, note, octave, duration, channel, volumes[0], volumes[1], speed);
  return R_NilValue;
}
SEXP r_sync(SEXP args)
{
  // sync(mask=0 bank=0 tocart=false)
  const int argn = Rf_length(args);
  const u32 mask    = argn > 0 ? (u32) drIntp(ARGS(1)) : 0;
  const s32 bank    = argn > 1 ? drIntp(ARGS(2)) :       0;
  const bool tocart = argn > 2 ? drIntp(ARGS(3)) :       false;
  TICAPI(sync, mask, bank, tocart);
  return R_NilValue;
}
SEXP r_vbank(SEXP args)
{
  // vbank(bank) -> prev
  // vbank() -> prev
  const int argn = Rf_length(args);
  const s32 prev = ((tic_core *)RTicRam)->state.vbank.id;
  if (argn == 1) {
    const s32 bank = drIntp(ARGS(1));
    TICAPI(vbank, bank);
  }
  return Rf_ScalarInteger(prev);
}
SEXP r_peek(SEXP args)
{
  // peek(addr bits=8) -> value
  const s32 addr = drIntp(ARGS(1));
  const int argn = Rf_length(args);
  const s32 bits = argn > 1 ? drIntp(ARGS(2)) : 8;
  return Rf_ScalarInteger(TICAPI(peek, addr, bits));
}
SEXP r_poke(SEXP args)
{
  // poke(addr value bits=8)
  const s32 addr = drIntp(ARGS(1));
  const s32 value = drIntp(ARGS(2));
  const int argn = Rf_length(args);
  const s32 bits = argn > 2 ? drIntp(ARGS(3)) : 8;
  TICAPI(poke, addr, value, bits);
  return R_NilValue;
}
SEXP r_peek1(SEXP args)
{
  // peek1(addr) -> value
  const s32 addr = drIntp(ARGS(1));
  return Rf_ScalarInteger(TICAPI(peek1, addr));
}
SEXP r_poke1(SEXP args)
{
  // poke1(addr value)
  const s32 addr  = drIntp(ARGS(1));
  const s32 value = drIntp(ARGS(2));
  TICAPI(poke1, addr, value);
  return R_NilValue;
}
SEXP r_peek2(SEXP args)
{
  // peek2(addr) -> value
  const s32 addr = drIntp(ARGS(1));
  return Rf_ScalarInteger(TICAPI(peek2, addr));
}
SEXP r_poke2(SEXP args)
{
  // poke2(addr value)
  const s32 addr  = drIntp(ARGS(1));
  const s32 value = drIntp(ARGS(2));
  TICAPI(poke2, addr, value);
  return R_NilValue;
}
SEXP r_peek4(SEXP args)
{
  // peek4(addr) -> value
  const s32 addr = drIntp(ARGS(1));
  return Rf_ScalarInteger(TICAPI(peek4, addr));
}
SEXP r_poke4(SEXP args)
{
  // poke(addr value)
  const s32 addr  = drIntp(ARGS(1));
  const s32 value = drIntp(ARGS(2));
  TICAPI(poke4, addr, value);
  return R_NilValue;
}
SEXP r_memcpy(SEXP args)
{
  // memcpy(dest source size)
  const s32 dest   = drIntp(ARGS(1));
  const s32 source = drIntp(ARGS(2));
  const s32 size   = drIntp(ARGS(3));
  TICAPI(memcpy, dest, source, size);
  return R_NilValue;
}
SEXP r_memset(SEXP args)
{
  // memset(dest value size)
  const s32 dest   = drIntp(ARGS(1));
  const s32 value = drIntp(ARGS(2));
  const s32 size   = drIntp(ARGS(3));
  TICAPI(memset, dest, value, size);
  return R_NilValue;
}
SEXP r_pmem(SEXP args)
{
  // pmem(index value)
  // pmem(index) -> value
  const s32 index = drIntp(ARGS(1));
  s32 value = 0;
  bool shouldSet = false;
  if (Rf_length(args) > 1)
  {
    value = drIntp(ARGS(2));
    shouldSet = true;
  }
  return Rf_ScalarInteger(TICAPI(pmem, index, value, shouldSet));
}
SEXP r_fget(SEXP args)
{
  // fget(sprite_id flag) -> bool
  const s32 sprite_id = drIntp(ARGS(1));
  const u8 flag       = drIntp(ARGS(2));
  return Rf_ScalarLogical(TICAPI(fget, sprite_id, flag));
}

SEXP r_fset(SEXP args)
{
  // fset(sprite_id flag bool)
  const s32  sprite_id = drIntp(ARGS(1));
  const u8   flag      = drIntp(ARGS(2));
  const bool val       = drLglp(ARGS(3));
  TICAPI(fset, sprite_id, flag, val);
  return R_NilValue;
}
SEXP r_mget(SEXP args)
{
  // mget(x y) -> tile_id
  const s32 x = drIntp(ARGS(1));
  const s32 y = drIntp(ARGS(2));
  return Rf_ScalarInteger(TICAPI(mget, x, y));
}

SEXP r_mset(SEXP args)
{
  // mset(x y tile_id)
  const s32 x       = drIntp(ARGS(1));
  const s32 y       = drIntp(ARGS(2));
  const u8  tile_id = drIntp(ARGS(3));
  TICAPI(mset, x, y, tile_id);
  return R_NilValue;
}
SEXP r_reset(SEXP args)
{
  // reset()
  TICAPI(reset);
  return R_NilValue;
}
SEXP r_trace(SEXP args)
{
  // trace(message color=15)
  const char* msg   = drCrap(ARGS(1));
  const s32   color = Rf_length(args) > 1 ? drIntp(ARGS(2)) : 15;
  TICAPI(trace, msg, color);
  return R_NilValue;
}
SEXP r_time(SEXP args)
{
  // time() -> ticks
  return Rf_ScalarInteger(TICAPI(time));
}
SEXP r_tstamp(SEXP args)
{
  // tstamp() -> timestamp
  return Rf_ScalarInteger(TICAPI(tstamp));
}
SEXP r_exit(SEXP args)
{
  // exit()
  TICAPI(exit);
  return R_NilValue;
}
SEXP r_fft(SEXP args)
{
  // fft(int start_freq, int end_freq=-1) -> float_value
  const int argn = Rf_length(args);
  const s32 start_freq = argn > 0 ? drIntp(ARGS(1)) : -1;
  const s32 end_freq   = argn > 1 ? drIntp(ARGS(2)) : -1;
  return Rf_ScalarReal(TICAPI(fft, start_freq, end_freq));
}
SEXP r_ffts(SEXP args)
{
  // ffts(int start_freq, int end_freq=-1) -> float_value
  const int argn = Rf_length(args);
  const s32 start_freq = argn > 0 ? drIntp(ARGS(1)) : -1;
  const s32 end_freq   = argn > 1 ? drIntp(ARGS(2)) : -1;
  return Rf_ScalarReal(TICAPI(ffts, start_freq, end_freq));
}

void initializeRExternalMethodsDefinedInC(void) {
  static R_ExternalMethodDef RExternalMethodsDefinedInC[] = {
    { "t80.btn",   (DL_FUNC) &r_btn,    -1 },
    { "t80.btnp",  (DL_FUNC) &r_btnp,   -1 },
    { "t80.circ",  (DL_FUNC) &r_circ,   -1 },
    { "t80.circb", (DL_FUNC) &r_circb,  -1 },
    { "t80.clip",  (DL_FUNC) &r_clip,   -1 },
    { "t80.cls",   (DL_FUNC) &r_cls,    -1 },
    { "t80.elli",  (DL_FUNC) &r_elli,   -1 },
    { "t80.ellib", (DL_FUNC) &r_ellib,  -1 },
    { "t80.exit",  (DL_FUNC) &r_exit,   -1 },
    { "t80.fget",  (DL_FUNC) &r_fget,   -1 },
    { "t80.font",  (DL_FUNC) &r_font,   -1 },
    { "t80.fset",  (DL_FUNC) &r_fset,   -1 },
    { "t80.key",   (DL_FUNC) &r_key,    -1 },
    { "t80.keyp",  (DL_FUNC) &r_keyp,   -1 },
    { "t80.line",  (DL_FUNC) &r_line,   -1 },
    { "t80.map",   (DL_FUNC) &r_map,    -1 },
    { "t80.memcpy",(DL_FUNC) &r_memcpy, -1 },
    { "t80.memset",(DL_FUNC) &r_memset, -1 },
    { "t80.mget",  (DL_FUNC) &r_mget,   -1 },
    { "t80.mouse", (DL_FUNC) &r_mouse,  -1 },
    { "t80.mset",  (DL_FUNC) &r_mset,   -1 },
    { "t80.music", (DL_FUNC) &r_music,  -1 },
    { "t80.peek",  (DL_FUNC) &r_peek,   -1 },
    { "t80.peek1", (DL_FUNC) &r_peek1,  -1 },
    { "t80.peek2", (DL_FUNC) &r_peek2,  -1 },
    { "t80.peek4", (DL_FUNC) &r_peek4,  -1 },
    { "t80.pix",   (DL_FUNC) &r_pix,    -1 },
    { "t80.pmem",  (DL_FUNC) &r_pmem,   -1 },
    { "t80.poke",  (DL_FUNC) &r_poke,   -1 },
    { "t80.poke1", (DL_FUNC) &r_poke1,  -1 },
    { "t80.poke2", (DL_FUNC) &r_poke2,  -1 },
    { "t80.poke4", (DL_FUNC) &r_poke4,  -1 },
    { "t80.print", (DL_FUNC) &r_print,  -1 },
    { "t80.rect",  (DL_FUNC) &r_rect,   -1 },
    { "t80.rectb", (DL_FUNC) &r_rectb,  -1 },
    { "t80.reset", (DL_FUNC) &r_reset,  -1 },
    { "t80.sfx",   (DL_FUNC) &r_sfx,    -1 },
    { "t80.spr",   (DL_FUNC) &r_spr,    -1 },
    { "t80.sync",  (DL_FUNC) &r_sync,   -1 },
    { "t80.time",  (DL_FUNC) &r_time,   -1 },
    { "t80.trace", (DL_FUNC) &r_trace,  -1 },
    { "t80.tri",   (DL_FUNC) &r_tri,    -1 },
    { "t80.trib",  (DL_FUNC) &r_trib,   -1 },
    { "t80.tstamp",(DL_FUNC) &r_tstamp, -1 },
    { "t80.ttri",  (DL_FUNC) &r_ttri,   -1 },
    { "t80.vbank", (DL_FUNC) &r_vbank,  -1 }
  };

  DllInfo *embeddingProgramDllInfo = R_getEmbeddingDllInfo();
  R_registerRoutines(embeddingProgramDllInfo, NULL, NULL, NULL, RExternalMethodsDefinedInC);
}

void evalR(tic_mem *memory, const char *code) {
	setEnvironmentVariablesIfUnset();
	if (!R_initialized_once) {
		initR(memory, code);
		R_initialized_once = true;
	}

	if (R_initialized_once) {
    SEXP RESULT = R_ParseEvalString(code, R_GlobalEnv);
  }
}
void R_CleanUp(Rboolean saveact, int status, int RunLast) { ; }

void R_Suicide(const char *message)
{
  char  pp[1024];
  snprintf(pp, 1024, "Fatal error, but can't kermit: %s\n"
           "Execute me, please.\n", message);
  R_ShowMessage(pp);
  exit(1);

  while(1);
}

static tic_mem *tic_memory_static;

void R_ShowMessage(const char *s) {
/* Always use the sixteenth color. */
  ((tic_core *) tic_memory_static)->api.trace(tic_memory_static, s, 15);
}

static bool R_Initialized = false;

static void closeR(tic_mem *tic) {
  if (R_Initialized) {
    Rf_endEmbeddedR(0);

    /* Set the currentVM to NULL if it is not already null. */
    tic_core *core;
    if ((core = (((tic_core *) tic))->currentVM) != NULL) {
      core->currentVM = NULL;
    }
  }
}

/* This function is called with code, which is the entirety of the studio
   ,* editor's code buffer (i.e. the entire game code as one string). */
static bool initR(tic_mem *tic, const char *code) {
  closeR(tic);

  /* embdRAV: embedded R argument vector. */
  static char *embdRAV[] = { "TIC-80", "--quiet", "--vanilla" };

  /* Without this nothing in R will work. */
  setEnvironmentVariablesIfUnset();

  R_running_as_main_program = 0;

  if (!R_Initialized) {
    R_Initialized = (bool) Rf_initEmbeddedR(sizeof(embdRAV)/sizeof(embdRAV[0]),
                                            embdRAV);
    R_running_as_main_program = 0;
    /* Declared in Rinterface.h, defined in Rf_initEmbeddedR. */
    R_Interactive = false;
    RTicRam = R_MakeExternalPtr((void *) tic, NULL, NULL);
    initializeRExternalMethodsDefinedInC();

    /* Ensure BOOT is defined. If the user has defined it this no-op version
     * will be overwritten (as intended). */
    EVALG("BOOT <- function() `{`");

    /* Manually create a vector of type string, parse it, then evaluate it
     * expression by expression. */
    SEXP code_sexp, code_expr, code_result = R_NilValue;
    ParseStatus code_parse_status;
    code_sexp = PROTECT(Rf_allocVector(STRSXP, 1));
    SET_STRING_ELT(code_sexp, 0, Rf_mkChar(code));
    code_expr = PROTECT(R_ParseVector(code_sexp, -1, &code_parse_status, R_NilValue));
    if (code_parse_status != PARSE_OK) {
      UNPROTECT(2);
      Rf_error("Invalid call %s", code);
    }
    for (int i = 0; i < Rf_length(code_expr); i++) {
      Rf_eval(VECTOR_ELT(code_expr, i), R_GlobalEnv);
    }
    UNPROTECT(2);

    /* MAYBE: this might not be appropriate; do other APIs call BOOT? I should
     * also use the function defined through macro expansion to call the BOOT
     * function if that is appropriate. */
    EVALG("if (print(exists(\"BOOT\")) && is.function(BOOT)) { BOOT() } "
          "else { BOOT <- function() `{` }");
  }

  return R_Initialized;
}

static void callRFn_TIC80(tic_mem* tic) {
  SEXP exists = EVALG("if (exists(\"TIC-80\") && is.function(`TIC-80`)) "
                                    "`TIC-80`()");
}
#define defineCallRFnInEnvironment_(f, e, ...)                          \
  static void callRFn_##f(tic_mem *tic __VA_OPT__(,) __VA_ARGS__) {     \
    R_ParseEvalString("if (exists(\""#f"\") && is.function(`"#f"`)) "   \
                      "`"#f"`() else stop(\""#f" is not a defined function!\")", \
                      e);                                               \
  }
/* i.e., if (exists("f") && is.function(`f`)) `f`(), allowing call of syntactic
 * and non-syntactic names. */
#define defineCallRFn_(f, ...) defineCallRFnInEnvironment_(f, R_GlobalEnv __VA_OPT__(,) __VA_ARGS__)
defineCallRFn_(BOOT)
/* s32 row/index, void *data as well as the tic_mem *tic parameters. */
defineCallRFn_(MENU, s32 index, void *data)
defineCallRFn_(BDR, s32 row, void *data)
defineCallRFn_(SCN, s32 row, void *data)
#undef defineCallRFn_
#undef defineCallRFnInEnvironment_

/* Syntax highlighting and what-not */
static const char* const RKeywords [] =
{
	"if", "else", "repeat", "while", "function", "for", "in", "next", "break",
	"TRUE", "FALSE", "NULL", "Inf", "NaN", "NA", "NA_integer_", "NA_real_",
	"NA_complex_", "NA_character_",
	/* et cetera, see ?dots */
	"...", "..1", "..2", "..3", "..4", "..5", "..6", "..7", "..8", "..9",
};

/* A naive edit of the Python function to check if a character is a valid
 * character within an identifier. */
static bool r_isalnum(char c) {
  return (
    (c >= 'a' && c <= 'z')
    || (c >= 'A' && c <= 'Z')
    || (c >= '0' && c <= '9')
    || (c == '_') || (c == '.')
    );
}

static const tic_outline_item* getROutline(const char* code, s32* size)
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
    static const char FuncString[] = "<- function(";

    ptr = strstr(ptr, FuncString);

    if(ptr)
    {
      ptr += sizeof FuncString - 1;

      const char* start = ptr;
      const char* end = start;

      while(*ptr)
      {
        char c = *ptr;

        if(r_isalnum(c));
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

static const char* RAPIKeywords[] = {
#define TIC_CALLBACK_DEF(name, ...) #name,
  TIC_CALLBACK_LIST(TIC_CALLBACK_DEF)
#undef  TIC_CALLBACK_DEF

#define API_KEYWORD_DEF(name, ...) #name,
  TIC_API_LIST(API_KEYWORD_DEF)
#undef  API_KEYWORD_DEF
};

static const u8 DemoRom[] =
{
  /* Automatically built from ../../demos/rdemo.r */
#include "../build/assets/rdemo.tic.dat"
};

static const u8 MarkRom[] =
{
  /* Automatically built from ../../demos/bunny/rbenchmark.r */
#include "../build/assets/rmark.tic.dat"
};
/* DEFAULT visibility*/
/* EXPORT_SCRIPT -> RScriptConfig if static, else ScriptConfig */
TIC_EXPORT const tic_script EXPORT_SCRIPT(R) =
{
	/* The first five members of the struct have the sum total following
	 * size. */
	/* sizeof(u8) + 3 * sizeof(char *)  */
	/* R's id is determined by counting up from 10, beginning with Lua, all of
		 the other languages TIC-80 supports. Python was the 10th langauge supported,
		 with .id 20. */
	.id                     = 21,
	.name                   = "r",
	.fileExtension          = ".r",
	.projectComment         = "##",
	{
		.init                 = initR,

		.close                = closeR,
		.tick                 = callRFn_TIC80,
		.boot                 = callRFn_BOOT,

		.callback             =
		{
			.scanline           = callRFn_SCN,
			.border             = callRFn_BDR,
			.menu               = callRFn_MENU,
		},
	},

	.getOutline             = getROutline,
	.eval                   = evalR,

	.blockCommentStart      = NULL,
	.blockCommentEnd        = NULL,
	.blockCommentStart2     = NULL,
	.blockCommentEnd2       = NULL,
	.singleComment          = "##",
	.blockStringStart       = "\"",
	.blockStringEnd         = "\"",
	.stdStringStartEnd      = "\"",
	.blockEnd               = NULL,
	.lang_isalnum           = r_isalnum,
	.api_keywords           = RAPIKeywords,
	.api_keywordsCount      = COUNT_OF(RAPIKeywords),
	.useStructuredEdition   = false,

	.keywords               = RKeywords,
	.keywordsCount          = COUNT_OF(RKeywords),

	.demo = {DemoRom, sizeof DemoRom},
	.mark = {MarkRom, sizeof MarkRom, "rmark.tic"},
};
