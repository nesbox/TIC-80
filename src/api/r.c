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
#include <Rconfig.h>
#include <Rdefines.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api/renv.h"

/* API */
#define sixth CAR(nthcdr(6))
#define seventh CAR(nthcdr(7))
#define eighth CAR(nthcdr(8))
#define ninth CAR(nthcdr(9))

bool R_initialized_once = false;
extern int R_running_as_main_program;   /* location within The R sources: ../unix/system.c */
static bool initR(tic_mem *tic, const char *code);
#define RSTRT(x) SEXP x(SEXP args) { int protected_count = 0; const int argn = length(args);
#define RUNP UNPROTECT(protected_count);
#define REND }

#define ProtectAndIncrement(x) (++protected_count, PROTECT(x))
#define psint(x) ProtectAndIncrement(ScalarInteger(x))
#define pslgl(x) ProtectAndIncrement(ScalarLogical(x))
#define psstr(x) ProtectAndIncrement(ScalarString(x))
#define CADNR(x) CAR(nthcdr(x))
RSTRT(r_print)
  /* print(text x=0 y=0 color=15 fixed=false scale=1 smallfont=false)
   * ⮑ width*/
  const char* text = psstr(first);
  const s32  x     = argn > 1 ? psint(second)         : 0;
  const s32  y     = argn > 2 ? psint(third)          : 0;
  const u8   color = argn > 3 ? psint(fourth)         : 15;
  const bool fixed = argn > 4 ? pslgl(fifth)          : false;
  const s32  scale = argn > 5 ? psint(sixth)          : 1;
  const bool alt   = argn > 6 ? pslgl(seventh)        : false;

  RUNP;

  return ScalarLogical(core->api.print(R_ExternalPtrAddr(RTicRam), text, x, y, color, fixed, scale, alt));
REND
RSTRT(r_cls)
  /* cls(color=0)
   * ⮑ nil */

  const u8 color = (argn > 0) ? psInt(first) : 0;

  core->api.cls(R_ExternalPtrAddr(RTicMem), color);

  RUNP;

  return R_NilValue
REND
RSTRT(r_pix)
  /* pix(x y color = 0)
	 * ⮑ nil
	 * pix(x y)
   * ⮑ color */
  const s32 x = psint(first);
  const s32 y = psint(second);

  if (argn == 3) {
    const u8 color = psint(third);
    core->api.pix(tic, x, y, color, false);
		RUNP;
    return R_NilValue;
  } else {
		RUNP;
    return ScalarInteger(core->api.pix(R_ExternalPtrAddr(RTicMem), x, y, 0, true));
  }
REND
RSTRT(r_line)
  /* line(x0 y0 x1 y1 color)
	 * ⮑ nil */
  const s32 x0    = psint(first);
  const s32 y0    = psint(second);
  const s32 x1    = psint(third);
  const s32 y1    = psint(fourth);
  const u8  color = psint(fifth);

  core->api.line(R_ExternalPtrAddr(RTicMem), x0, y0, x1, y1, color);
	RUNP;
  return R_NilValue;
REND
SEXP r_rect(SEXP args)
{
  // rect(x y w h color)
	int protected_count = 0;
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 x = ProtInc(ScalarInteger(CADR(args)));
  const s32 y = ProtInc(ScalarInteger(CADDR(args)));
  const s32 w = ProtInc(ScalarInteger(CADDDR(args)));
  const s32 h = ProtInc(ScalarInteger(CADDDDR(args)));
  const u8 color = ScalarInteger(s7_list_ref(sc, args, 4));
  core->api.rect(tic, x, y, w, h, color);
	UNPROTECT(protected_count);
  return R_NilValue;
}
SEXP r_rectb(SEXP args)
{
  // rectb(x y w h color)
	int protected_count = 0;
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 x    = ProtInc(ScalarInteger(first));
  const s32 y    = ProtInc(ScalarInteger(second));
  const s32 w    = ProtInc(ScalarInteger(third));
  const s32 h    = ProtInc(ScalarInteger(fourth));
  const u8 color = ProtInc(ScalarInteger(fifth));
  core->api.rectb(tic, x, y, w, h, color);
	UNPROTECT(protected_count);
  return R_NilValue;
}
void parseTransparentColorsArg(SEXP colorkey, u8* out_transparent_colors, u8* out_count)
{
  *out_count = 0;
  if (s7_is_list(sc, colorkey))
  {
    const s32 arg_color_count = length(colorkey);
    const u8 color_count = arg_color_count < TIC_PALETTE_SIZE ? (u8)arg_color_count : TIC_PALETTE_SIZE;
    for (u8 i=0; i<color_count; ++i)
    {
      SEXP c = s7_list_ref(sc, colorkey, i);
      out_transparent_colors[i] = s7_is_integer(c) ? ScalarInteger(c) : 0;
      ++(*out_count);
    }
  }
  else if (s7_is_integer(colorkey))
  {
    out_transparent_colors[0] = (u8)ScalarInteger(colorkey);
    *out_count = 1;
  }
}
SEXP r_spr(SEXP args)
{
  // spr(id x y colorkey=-1 scale=1 flip=0 rotate=0 w=1 h=1)
	int protected_count = 0;
  const int argn      = length(args);
  tic_core* core      = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 id        = ProtInc(ScalarInteger(first));
  const s32 x         = ProtInc(ScalarInteger(second));
  const s32 y         = ProtInc(ScalarInteger(third));

  static u8 trans_colors[TIC_PALETTE_SIZE];
  u8 trans_count = 0;
  if (argn > 3)
  {
    SEXP colorkey = ProtInc(fourth);
    parseTransparentColorsArg(sc, colorkey, trans_colors, &trans_count);
  }

  const s32 scale     = argn > 4 ? ProtInc(ScalarInteger(fourth           ) ) : 1;
  const s32 flip      = argn > 5 ? ProtInc(ScalarInteger(fifth            ) ) : 0;
  const s32 rotate    = argn > 6 ? ProtInc(ScalarInteger(CAR(nthcdr(6 ) ) ) ) : 0;
  const s32 w         = argn > 7 ? ProtInc(ScalarInteger(CAR(nthcdr(7 ) ) ) ) : 1;
  const s32 h         = argn > 8 ? ProtInc(ScalarInteger(CAR(nthcdr(8 ) ) ) ) : 1;
  core->api.spr(tic, id, x, y, w, h, trans_colors, trans_count, scale, (tic_flip)flip, (tic_rotate) rotate);
	UNPROTECT(protected_count);
  return R_NilValue
}
SEXP r_btn(SEXP args)
{
  // btn(id) -> pressed
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 id = ScalarInteger(CADR(args));

  return s7_make_boolean(sc, core->api.btn(tic, id));
}
SEXP r_btnp(SEXP args)
{
  // btnp(id hold=-1 period=-1) -> pressed
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 id = ScalarInteger(CADR(args));

  const int argn = length(args);
  const s32 hold = argn > 1 ? ScalarInteger(CADDR(args)) : -1;
  const s32 period = argn > 2 ? ScalarInteger(CADDDR(args)) : -1;

  return s7_make_boolean(sc, core->api.btnp(tic, id, hold, period));
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
typedef struct
{
  s7_scheme* sc;
  SEXP callback;
} RemapData;
SEXP r_sfx(SEXP a, SEXP args)
{
  // sfx(id note=-1 duration=-1 channel=0 volume=15 speed=0)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 id = ScalarInteger(CADR(args));

  const int argn = length(args);
  int note = -1;
  int octave = -1;
  if (argn > 1) {
    SEXP note_ptr = CADDR(args);
    if (s7_is_integer(note_ptr)) {
      const s32 raw_note = ScalarInteger(note_ptr);
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
      const char* note_str = ScalarString(note_ptr);
      const u8 len = ScalarString_length(note_ptr);
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

  const s32 duration = argn > 2 ? ScalarInteger(CADDDR(args)) : -1;
  const s32 channel = argn > 3 ? ScalarInteger(CADDDDR(args)) : 0;

  s32 volumes[TIC80_SAMPLE_CHANNELS] = {MAX_VOLUME, MAX_VOLUME};
  if (argn > 4) {
    SEXP volume_arg = s7_list_ref(sc, args, 4);
    if (s7_is_integer(volume_arg)) {
      volumes[0] = volumes[1] = ScalarInteger(volume_arg) & 0xF;
    } else if (s7_is_list(sc, volume_arg) && length(volume_arg) == 2) {
      volumes[0] = ScalarInteger(CADR(volume_arg)) & 0xF;
      volumes[1] = ScalarInteger(CADDR(volume_arg)) & 0xF;
    }
  }
  const s32 speed = argn > 5 ? ScalarInteger(s7_list_ref(sc, args, 5)) : 0;

  core->api.sfx(tic, id, note, octave, duration, channel, volumes[0], volumes[1], speed);
  return R_NilValue
}
static void remapCallback(void* data, s32 x, s32 y, RemapResult* result)
{
  RemapData* remap = (RemapData*)data;
  s7_scheme* sc = remap->sc;

  // (callback index x y) -> (list index flip rotate)
  SEXP callbackResult = s7_call(sc, remap->callback,
                                s7_cons(sc, s7_make_integer(sc, result->index),
                                        s7_cons(sc, s7_make_integer(sc, x),
                                                s7_cons(sc, s7_make_integer(sc, y),
                                                        R_NilValue))));

  if (s7_is_list(sc, callbackResult) && length(callbackResult) == 3)
  {
    result->index = ScalarInteger(CADR(callbackResult));
    result->flip = (tic_flip)ScalarInteger(CADDR(callbackResult));
    result->rotate = (tic_rotate)ScalarInteger(CADDDR(callbackResult));
  }
}
SEXP r_map(SEXP args)
{
  // map(x=0 y=0 w=30 h=17 sx=0 sy=0 colorkey=-1 scale=1 remap=nil)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 x = ScalarInteger(CADR(args));
  const s32 y = ScalarInteger(CADDR(args));
  const s32 w = ScalarInteger(CADDDR(args));
  const s32 h = ScalarInteger(CADDDDR(args));
  const s32 sx = ScalarInteger(s7_list_ref(sc, args, 4));
  const s32 sy = ScalarInteger(s7_list_ref(sc, args, 5));

  const int argn = length(args);

  static u8 trans_colors[TIC_PALETTE_SIZE];
  u8 trans_count = 0;
  if (argn > 6) {
    SEXP colorkey = s7_list_ref(sc, args, 6);
    parseTransparentColorsArg(sc, colorkey, trans_colors, &trans_count);
  }

  const s32 scale = argn > 7 ? ScalarInteger(s7_list_ref(sc, args, 7)) : 1;

  RemapFunc remap = NULL;
  RemapData data;
  if (argn > 8)
  {
    remap = remapCallback;
    data.sc = sc;
    data.callback = s7_list_ref(sc, args, 8);
  }
  core->api.map(tic, x, y, w, h, sx, sy, trans_colors, trans_count, scale, remap, &data);
  return R_NilValue
}
SEXP r_mget(SEXP args)
{
  // mget(x y) -> tile_id
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 x = ScalarInteger(CADR(args));
  const s32 y = ScalarInteger(CADDR(args));
  return s7_make_integer(sc, core->api.mget(tic, x, y));
}
SEXP r_mset(SEXP args)
{
  // mset(x y tile_id)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 x = ScalarInteger(CADR(args));
  const s32 y = ScalarInteger(CADDR(args));
  const u8 tile_id = ScalarInteger(CADDDR(args));
  core->api.mset(tic, x, y, tile_id);
  return R_NilValue
}
SEXP r_peek(SEXP args)
{
  // peek(addr bits=8) -> value
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 addr = ScalarInteger(CADR(args));
  const int argn = length(args);
  const s32 bits = argn > 1 ? ScalarInteger(CADDR(args)) : 8;
  return s7_make_integer(sc, core->api.peek(tic, addr, bits));
}
SEXP r_poke(SEXP args)
{
  // poke(addr value bits=8)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 addr = ScalarInteger(CADR(args));
  const s32 value = ScalarInteger(CADDR(args));
  const int argn = length(args);
  const s32 bits = argn > 2 ? ScalarInteger(CADDDR(args)) : 8;
  core->api.poke(tic, addr, value, bits);
  return R_NilValue
}
SEXP r_peek1(SEXP args)
{
  // peek1(addr) -> value
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 addr = ScalarInteger(CADR(args));
  return s7_make_integer(sc, core->api.peek1(tic, addr));
}
SEXP r_poke1(SEXP args)
{
  // poke1(addr value)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 addr = ScalarInteger(CADR(args));
  const s32 value = ScalarInteger(CADDR(args));
  core->api.poke1(tic, addr, value);
  return R_NilValue
}
SEXP r_peek2(SEXP args)
{
  // peek2(addr) -> value
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 addr = ScalarInteger(CADR(args));
  return s7_make_integer(sc, core->api.peek2(tic, addr));
}
SEXP r_poke2(SEXP args)
{
  // poke2(addr value)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 addr = ScalarInteger(CADR(args));
  const s32 value = ScalarInteger(CADDR(args));
  core->api.poke2(tic, addr, value);
  return R_NilValue
}
SEXP r_peek4(SEXP args)
{
  // peek4(addr) -> value
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 addr = ScalarInteger(CADR(args));
  return s7_make_integer(sc, core->api.peek4(tic, addr));
}
SEXP r_poke4(SEXP args)
{
  // poke4(addr value)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 addr = ScalarInteger(CADR(args));
  const s32 value = ScalarInteger(CADDR(args));
  core->api.poke4(tic, addr, value);
  return R_NilValue
}
SEXP r_memcpy(SEXP args)
{
  // memcpy(dest source size)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 dest = ScalarInteger(CADR(args));
  const s32 source = ScalarInteger(CADDR(args));
  const s32 size = ScalarInteger(CADDDR(args));

  core->api.memcpy(tic, dest, source, size);
  return R_NilValue
}
SEXP r_memset(SEXP args)
{
  // memset(dest value size)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 dest = ScalarInteger(CADR(args));
  const s32 value = ScalarInteger(CADDR(args));
  const s32 size = ScalarInteger(CADDDR(args));

  core->api.memset(tic, dest, value, size);
  return R_NilValue
}
SEXP r_trace(SEXP args)
{
  // trace(message color=15)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const char* msg = ScalarString(CADR(args));
  const int argn = length(args);
  const s32 color = argn > 1 ? ScalarInteger(CADDR(args)) : 15;
  core->api.trace(tic, msg, color);
  return R_NilValue
}
SEXP r_pmem(SEXP args)
{
  // pmem(index value)
  // pmem(index) -> value
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 index = ScalarInteger(CADR(args));
  const int argn = length(args);
  s32 value = 0;
  bool shouldSet = false;
  if (argn > 1)
  {
    value = ScalarInteger(CADDR(args));
    shouldSet = true;
  }
  return s7_make_integer(sc, (s32)core->api.pmem(tic, index, value, shouldSet));
}
SEXP r_time(SEXP args)
{
  // time() -> ticks
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  return s7_make_real(sc, core->api.time(tic));
}
SEXP r_tstamp(SEXP args)
{
  // tstamp() -> timestamp
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  return s7_make_integer(sc, core->api.tstamp(tic));
}
SEXP r_exit(SEXP args)
{
  // exit()
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  core->api.exit(tic);
  return R_NilValue
}
SEXP r_font(SEXP args)
{
  // font(text x y chromakey char_width char_height fixed=false scale=1 alt=false) -> width
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const char* text = ScalarString(CADR(args));
  const s32 x = ScalarInteger(CADDR(args));
  const s32 y = ScalarInteger(CADDDR(args));

  static u8 trans_colors[TIC_PALETTE_SIZE];
  u8 trans_count = 0;
  SEXP colorkey = CADDDDR(args);
  parseTransparentColorsArg(sc, colorkey, trans_colors, &trans_count);

  const s32 w = ScalarInteger(s7_list_ref(sc, args, 4));
  const s32 h = ScalarInteger(s7_list_ref(sc, args, 5));
  const int argn = length(args);
  const s32 fixed = argn > 6 ? ScalarLogical(sc, s7_list_ref(sc, args, 6)) : false;
  const s32 scale = argn > 7 ? ScalarInteger(s7_list_ref(sc, args, 7)) : 1;
  const s32 alt = argn > 8 ? ScalarLogical(sc, s7_list_ref(sc, args, 8)) : false;

  return s7_make_integer(sc, core->api.font(tic, text, x, y, trans_colors, trans_count, w, h, fixed, scale, alt));
}
/* This API function does not use the convenience macros because it doesn't need
 * to protect any values from garbage collection, because every use of an R API
 * call is to manipulate a C value which R does not control the memory of. */
SEXP r_mouse(SEXP args)
{
  /* mouse()
	 * ⮑ x y left middle right scrollx scrolly */
	tic_mem *tic = (tic_mem *)R_ExternalPtrAddr(RTicMem);
	tic_core* core = (tic_core *)tic;

  const tic_point point = core->api.mouse(tic);
  const tic80_mouse* mouse = &((tic_core*)tic)->memory.ram->input.mouse;

  return CONS(ScalarInteger(point.x),
              list6(ScalarInteger(point.y),
                    ScalarInteger(mouse->left),
                    ScalarInteger(mouse->middle),
                    ScalarInteger(mouse->right),
                    ScalarInteger(mouse->scrollx),
                    ScalarInteger(mouse->scrolly)));
}
RSTRT(r_circ)
  // circ(x y radius color)
  const s32 x = psint(first);
  const s32 y = psint(second);
  const s32 radius = psint(third);
  const s32 color = psint(fourth);

  core->api.circ(tic, x, y, radius, color);

  RUNP;
  return R_NilValue;
REND
SEXP r_circb(SEXP args)
{
  // circb(x y radius color)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 x = ScalarInteger(CADR(args));
  const s32 y = ScalarInteger(CADDR(args));
  const s32 radius = ScalarInteger(CADDDR(args));
  const s32 color = ScalarInteger(CADDDDR(args));
  core->api.circb(tic, x, y, radius, color);
  return R_NilValue
}
SEXP r_elli(SEXP args)
{
  // elli(x y a b color)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 x = ScalarInteger(CADR(args));
  const s32 y = ScalarInteger(CADDR(args));
  const s32 a = ScalarInteger(CADDDR(args));
  const s32 b = ScalarInteger(CADDDDR(args));
  const s32 color = ScalarInteger(s7_list_ref(sc, args, 4));
  core->api.elli(tic, x, y, a, b, color);
  return R_NilValue
}
SEXP r_ellib(SEXP args)
{
  // ellib(x y a b color)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 x = ScalarInteger(CADR(args));
  const s32 y = ScalarInteger(CADDR(args));
  const s32 a = ScalarInteger(CADDDR(args));
  const s32 b = ScalarInteger(CADDDDR(args));
  const s32 color = ScalarInteger(s7_list_ref(sc, args, 4));
  core->api.ellib(tic, x, y, a, b, color);
  return R_NilValue
}
SEXP r_tri(SEXP args)
{
  // tri(x1 y1 x2 y2 x3 y3 color)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 x1 = ScalarInteger(CADR(args));
  const s32 y1 = ScalarInteger(CADDR(args));
  const s32 x2 = ScalarInteger(CADDDR(args));
  const s32 y2 = ScalarInteger(CADDDDR(args));
  const s32 x3 = ScalarInteger(s7_list_ref(sc, args, 4));
  const s32 y3 = ScalarInteger(s7_list_ref(sc, args, 5));
  const s32 color = ScalarInteger(s7_list_ref(sc, args, 6));
  core->api.tri(tic, x1, y1, x2, y2, x3, y3, color);
  return R_NilValue
}
SEXP r_trib(SEXP args)
{
  // trib(x1 y1 x2 y2 x3 y3 color)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 x1 = ScalarInteger(CADR(args));
  const s32 y1 = ScalarInteger(CADDR(args));
  const s32 x2 = ScalarInteger(CADDDR(args));
  const s32 y2 = ScalarInteger(CADDDDR(args));
  const s32 x3 = ScalarInteger(s7_list_ref(sc, args, 4));
  const s32 y3 = ScalarInteger(s7_list_ref(sc, args, 5));
  const s32 color = ScalarInteger(s7_list_ref(sc, args, 6));
  core->api.trib(tic, x1, y1, x2, y2, x3, y3, color);
  return R_NilValue
}
SEXP r_ttri(SEXP args)
{
  // ttri(x1 y1 x2 y2 x3 y3 u1 v1 u2 v2 u3 v3 texsrc=0 chromakey=-1 z1=0 z2=0 z3=0)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 x1 = ScalarInteger(CADR(args));
  const s32 y1 = ScalarInteger(CADDR(args));
  const s32 x2 = ScalarInteger(CADDDR(args));
  const s32 y2 = ScalarInteger(CADDDDR(args));
  const s32 x3 = ScalarInteger(s7_list_ref(sc, args, 4));
  const s32 y3 = ScalarInteger(s7_list_ref(sc, args, 5));
  const s32 u1 = ScalarInteger(s7_list_ref(sc, args, 6));
  const s32 v1 = ScalarInteger(s7_list_ref(sc, args, 7));
  const s32 u2 = ScalarInteger(s7_list_ref(sc, args, 8));
  const s32 v2 = ScalarInteger(s7_list_ref(sc, args, 9));
  const s32 u3 = ScalarInteger(s7_list_ref(sc, args, 10));
  const s32 v3 = ScalarInteger(s7_list_ref(sc, args, 11));

  const int argn = length(args);
  const tic_texture_src texsrc = (tic_texture_src)(argn > 12 ? ScalarInteger(s7_list_ref(sc, args, 12)) : 0);

  static u8 trans_colors[TIC_PALETTE_SIZE];
  u8 trans_count = 0;

  if (argn > 13)
  {
    SEXP colorkey = s7_list_ref(sc, args, 13);
    parseTransparentColorsArg(sc, colorkey, trans_colors, &trans_count);
  }

  bool depth = argn > 14 ? true : false;
  const s32 z1 = argn > 14 ? ScalarInteger(s7_list_ref(sc, args, 14)) : 0;
  const s32 z2 = argn > 15 ? ScalarInteger(s7_list_ref(sc, args, 15)) : 0;
  const s32 z3 = argn > 16 ? ScalarInteger(s7_list_ref(sc, args, 16)) : 0;

  core->api.ttri(tic, x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, texsrc, trans_colors, trans_count, z1, z2, z3, depth);
  return R_NilValue
}
SEXP r_clip(SEXP args)
{
  // clip(x y width height)
  // clip()
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const int argn = length(args);
  if (argn != 4) {
    core->api.clip(tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
  } else {
    const s32 x = ScalarInteger(CADR(args));
    const s32 y = ScalarInteger(CADDR(args));
    const s32 w = ScalarInteger(CADDDR(args));
    const s32 h = ScalarInteger(CADDDDR(args));
    core->api.clip(tic, x, y, w, h);
  }
  return R_NilValue
}
SEXP r_music(SEXP args)
{
  // music(track=-1 frame=-1 row=-1 loop=true sustain=false tempo=-1 speed=-1)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const int argn = length(args);
  const s32 track = argn > 0 ? ScalarInteger(CADR(args)) : -1;
  const s32 frame = argn > 1 ? ScalarInteger(CADDR(args)) : -1;
  const s32 row = argn > 2 ? ScalarInteger(CADDDR(args)) : -1;
  const bool loop = argn > 3 ? ScalarLogical(sc, CADDDDR(args)) : true;
  const bool sustain = argn > 4 ? ScalarLogical(sc, s7_list_ref(sc, args, 4)) : false;
  const s32 tempo = argn > 5 ? ScalarInteger(s7_list_ref(sc, args, 5)) : -1;
  const s32 speed = argn > 6 ? ScalarInteger(s7_list_ref(sc, args, 6)) : -1;
  core->api.music(tic, track, frame, row, loop, sustain, tempo, speed);
  return R_NilValue
}
SEXP r_sync(SEXP args)
{
  // sync(mask=0 bank=0 tocart=false)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const int argn = length(args);
  const u32 mask = argn > 0 ? (u32)ScalarInteger(CADR(args)) : 0;
  const s32 bank = argn > 1 ? ScalarInteger(CADDR(args)) : 0;
  const bool tocart = argn > 2 ? ScalarLogical(sc, CADDDR(args)) : false;
  core->api.sync(tic, mask, bank, tocart);
  return R_NilValue
}
SEXP r_vbank(SEXP args)
{
  // vbank(bank) -> prev
  // vbank() -> prev
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const int argn = length(args);

  const s32 prev = ((tic_core*)tic)->state.vbank.id;
  if (argn == 1) {
    const s32 bank = ScalarInteger(CADR(args));
    core->api.vbank(tic, bank);
  }
  return s7_make_integer(sc, prev);
}
SEXP r_reset(SEXP args)
{
  // reset()
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  core->api.reset(tic);
  return R_NilValue
}
SEXP r_key(SEXP args)
{
  //key(code=-1) -> pressed
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const int argn = length(args);
  const tic_key code = argn > 0 ? ScalarInteger(CADR(args)) : -1;
  return s7_make_boolean(sc, core->api.key(tic, code));
}
SEXP r_keyp(SEXP args)
{
  // keyp(code=-1 hold=-1 period=-1) -> pressed
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const int argn = length(args);
  const tic_key code = argn > 0 ? ScalarInteger(CADR(args)) : -1;
  const s32 hold = argn > 1 ? ScalarInteger(CADDR(args)) : -1;
  const s32 period = argn > 2 ? ScalarInteger(CADDDR(args)) : -1;
  return s7_make_boolean(sc, core->api.keyp(tic, code, hold, period));
}
SEXP r_fget(SEXP args)
{
  // fget(sprite_id flag) -> bool
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 sprite_id = ScalarInteger(CADR(args));
  const u8 flag = ScalarInteger(CADDR(args));
  return s7_make_boolean(sc, core->api.fget(tic, sprite_id, flag));
}
SEXP r_fset(SEXP args)
{
  // fset(sprite_id flag bool)
  tic_core* core = R_GlobalEnv; tic_mem* tic = (tic_mem*)core;
  const s32 sprite_id = ScalarInteger(CADR(args));
  const u8 flag = ScalarInteger(CADDR(args));
  const bool val = ScalarLogical(sc, CADDDR(args));
  core->api.fset(tic, sprite_id, flag, val);
  return R_NilValue
}
SEXP r_fft(SEXP args)
{
  // fft(int start_freq, int end_freq=-1) -> float_value
  tic_core* core = R_GlobalEnv;
  tic_mem* tic = (tic_mem*)core;

  const int argn = length(args);
  const s32 start_freq = argn > 0 ? ScalarInteger(CADR(args)) : -1;
  const s32 end_freq = argn > 1 ? ScalarInteger(CADDR(args)) : -1;

  return s7_make_real(sc, core->api.fft(tic, start_freq, end_freq));
}
SEXP r_ffts(SEXP args)
{
  // ffts(int start_freq, int end_freq=-1) -> float_value
  tic_core* core = R_GlobalEnv;
  tic_mem* tic = (tic_mem*)core;
  const int argn = length(args);
  const s32 start_freq = argn > 0 ? ScalarInteger(CADR(args)) : -1;
  const s32 end_freq = argn > 1 ? ScalarInteger(CADDR(args)) : -1;

  return s7_make_real(sc, core->api.ffts(tic, start_freq, end_freq));
}

static const R_ExternalMethodDef RExternalMethodsDefinedInC[] = {
 { "btn",   (DL_FUNC) &r_btn,    -1 },
 { "btnp",  (DL_FUNC) &r_btnp,   -1 },
 { "circ",  (DL_FUNC) &r_circ,   -1 },
 { "circb", (DL_FUNC) &r_circb,  -1 },
 { "clip",  (DL_FUNC) &r_clip,   -1 },
 { "cls",   (DL_FUNC) &r_cls,    -1 },
 { "elli",  (DL_FUNC) &r_elli,   -1 },
 { "ellib", (DL_FUNC) &r_ellib,  -1 },
 { "exit",  (DL_FUNC) &r_exit,   -1 },
 { "fget",  (DL_FUNC) &r_fget,   -1 },
 { "font",  (DL_FUNC) &r_font,   -1 },
 { "fset",  (DL_FUNC) &r_fset,   -1 },
 { "key",   (DL_FUNC) &r_key,    -1 },
 { "keyp",  (DL_FUNC) &r_keyp,   -1 },
 { "line",  (DL_FUNC) &r_line,   -1 },
 { "map",   (DL_FUNC) &r_map,    -1 },
 { "memcpy",(DL_FUNC) &r_memcpy, -1 },
 { "memset",(DL_FUNC) &r_memset, -1 },
 { "mget",  (DL_FUNC) &r_mget,   -1 },
 { "mouse", (DL_FUNC) &r_mouse,  -1 },
 { "mset",  (DL_FUNC) &r_mset,   -1 },
 { "music", (DL_FUNC) &r_music,  -1 },
 { "peek",  (DL_FUNC) &r_peek,   -1 },
 { "peek1", (DL_FUNC) &r_peek1,  -1 },
 { "peek2", (DL_FUNC) &r_peek2,  -1 },
 { "peek4", (DL_FUNC) &r_peek4,  -1 },
 { "pix",   (DL_FUNC) &r_pix,    -1 },
 { "pmem",  (DL_FUNC) &r_pmem,   -1 },
 { "poke",  (DL_FUNC) &r_poke,   -1 },
 { "poke1", (DL_FUNC) &r_poke1,  -1 },
 { "poke2", (DL_FUNC) &r_poke2,  -1 },
 { "poke4", (DL_FUNC) &r_poke4,  -1 },
 { "print", (DL_FUNC) &r_print,  -1 },
 { "rect",  (DL_FUNC) &r_rect,   -1 },
 { "rectb", (DL_FUNC) &r_rectb,  -1 },
 { "reset", (DL_FUNC) &r_reset,  -1 },
 { "sfx",   (DL_FUNC) &r_sfx,    -1 },
 { "spr",   (DL_FUNC) &r_spr,    -1 },
 { "sync",  (DL_FUNC) &r_sync,   -1 },
 { "time",  (DL_FUNC) &r_time,   -1 },
 { "trace", (DL_FUNC) &r_trace,  -1 },
 { "tri",   (DL_FUNC) &r_tri,    -1 },
 { "trib",  (DL_FUNC) &r_trib,   -1 },
 { "tstamp",(DL_FUNC) &r_tstamp, -1 },
 { "ttri",  (DL_FUNC) &r_ttri,   -1 },
 { "vbank", (DL_FUNC) &r_vbank,  -1 },

 { NULL, NULL, 0, NULL }
};

R_registerRoutines(R_getEmbeddingDllInfo(), cMethods, NULL, NULL, NULL);

void evalR(tic_mem *memory, const char *code) {
	setEnvironmentVariablesIfUnset();
	if (!R_initialized_once) {
		initR(memory, code);
		R_initialized_once = true;
	}
	SEXP RESULT;
	if (R_initialized_once) RESULT = Rf_eval(CSTR2LANGSXP(code), R_GlobalEnv);
	#if defined ebug /* -DEBUG=ON */
	Rprintf("%s", RESULT);
	#endif
}
void R_CleanUp(Rboolean saveact, int status, int RunLast) { ; }

void R_Suicide(const char *message)
{
  char  pp[1024];
  snprintf(pp, 1024, "Fatal error, but can't kermit: %s\n"\
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

/* This function is called with code, which is the entirety of the studio
   ,* editor's code buffer (i.e. the entire game code as one string). */
static bool initR(tic_mem *tic, const char *code) {
  tic_memory_static = tic;

  closeR(tic);

  /* embdRAV: embedded R argument vector. */
  static char *embdRAV[] = { "TIC-80", "--quiet", "--vanilla" };

  /* Without this nothing in R will work. */
  setEnvironmentVariablesIfUnset();
  static bool R_Initialized = false;

  R_running_as_main_program = 0;

  if (!R_Initialized) {
    R_Initialized = (bool) Rf_initEmbeddedR(sizeof(embdRAV)/sizeof(embdRAV[0]), \
                                            embdRAV);
    R_running_as_main_program = 0;
    /* Declared in Rinterface.h, defined in Rf_initEmbeddedR. */
    R_Interactive = false;
  }

  static const R_CMethodDef cMethods[] = {
   { "btn",   (DL_FUNC) &r_btn,    -1 },
   { "btnp",  (DL_FUNC) &r_btnp,   -1 },
   { "circ",  (DL_FUNC) &r_circ,   -1 },
   { "circb", (DL_FUNC) &r_circb,  -1 },
   { "clip",  (DL_FUNC) &r_clip,   -1 },
   { "cls",   (DL_FUNC) &r_cls,    -1 },
   { "elli",  (DL_FUNC) &r_elli,   -1 },
   { "ellib", (DL_FUNC) &r_ellib,  -1 },
   { "exit",  (DL_FUNC) &r_exit,   -1 },
   { "fget",  (DL_FUNC) &r_fget,   -1 },
   { "font",  (DL_FUNC) &r_font,   -1 },
   { "fset",  (DL_FUNC) &r_fset,   -1 },
   { "key",   (DL_FUNC) &r_key,    -1 },
   { "keyp",  (DL_FUNC) &r_keyp,   -1 },
   { "line",  (DL_FUNC) &r_line,   -1 },
   { "map",   (DL_FUNC) &r_map,    -1 },
   { "memcpy",(DL_FUNC) &r_memcpy, -1 },
   { "memset",(DL_FUNC) &r_memset, -1 },
   { "mget",  (DL_FUNC) &r_mget,   -1 },
   { "mouse", (DL_FUNC) &r_mouse,  -1 },
   { "mset",  (DL_FUNC) &r_mset,   -1 },
   { "music", (DL_FUNC) &r_music,  -1 },
   { "peek",  (DL_FUNC) &r_peek,   -1 },
   { "peek1", (DL_FUNC) &r_peek1,  -1 },
   { "peek2", (DL_FUNC) &r_peek2,  -1 },
   { "peek4", (DL_FUNC) &r_peek4,  -1 },
   { "pix",   (DL_FUNC) &r_pix,    -1 },
   { "pmem",  (DL_FUNC) &r_pmem,   -1 },
   { "poke",  (DL_FUNC) &r_poke,   -1 },
   { "poke1", (DL_FUNC) &r_poke1,  -1 },
   { "poke2", (DL_FUNC) &r_poke2,  -1 },
   { "poke4", (DL_FUNC) &r_poke4,  -1 },
   { "print", (DL_FUNC) &r_print,  -1 },
   { "rect",  (DL_FUNC) &r_rect,   -1 },
   { "rectb", (DL_FUNC) &r_rectb,  -1 },
   { "reset", (DL_FUNC) &r_reset,  -1 },
   { "sfx",   (DL_FUNC) &r_sfx,    -1 },
   { "spr",   (DL_FUNC) &r_spr,    -1 },
   { "sync",  (DL_FUNC) &r_sync,   -1 },
   { "time",  (DL_FUNC) &r_time,   -1 },
   { "trace", (DL_FUNC) &r_trace,  -1 },
   { "tri",   (DL_FUNC) &r_tri,    -1 },
   { "trib",  (DL_FUNC) &r_trib,   -1 },
   { "tstamp",(DL_FUNC) &r_tstamp, -1 },
   { "ttri",  (DL_FUNC) &r_ttri,   -1 },
   { "vbank", (DL_FUNC) &r_vbank,  -1 },
  
   { NULL, NULL, 0, NULL }
  };
  
  R_registerRoutines(R_getEmbeddingDllInfo(), cMethods, NULL, NULL, NULL);

  Rf_eval(CSTR2LANGSXP("if (exists(\"BOOT\") && is.function(BOOT)) { BOOT() } " \
                       "else { BOOT <- function() `{`; ## Tricky NOP. }"),
          R_GlobalEnv);

  return R_Initialized;
}

static void closeR(tic_mem *tic) {
  tic_core *core;
  if ((core = (((tic_core *) tic))->currentVM) != NULL) {
    Rf_endEmbeddedR(0);
    core->currentVM = NULL;
  }
}
static void callRFn_TIC80(tic_mem* tic) {
#if !defined R_INTERNALS_H_
#error "R_GlobalEnv not defined because Rinternals.h not properly included... somehow."
#endif
	/* if (exists("TIC-80") && is.function(`TIC-80`)) `TIC-80`() */
	Rf_eval(Rf_mkString("if (exists(\"TIC-80\") && is.function(`TIC-80`)) "\
											"`TIC-80`()"),
					R_GlobalEnv);
}
#define defineCallRFnInEnvironment_(f, e, ...)                          \
  static void callRFn_##f(tic_mem *tic, ##__VA_ARGS__) {                \
    Rf_eval(Rf_mkString("if (exists(\""#f"\") && is.function(`"#f"`)) " \
                        "`"#f"`() else stop(\""#f" is not a defined function!\")"),\
            e);                                                         \
  }
/* i.e., if (exists("f") && is.function(`f`)) `f`(), allowing call of syntactic
 * and non-syntactic names. */
#define defineCallRFn_(f, ...) defineCallRFnInEnvironment_(f, R_GlobalEnv, ...)
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
