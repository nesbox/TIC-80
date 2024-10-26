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
bool R_initialized_once = false;
extern int R_running_as_main_program;   /* location within The R sources: ../unix/system.c */
static bool initR(tic_mem *tic, const char *code);
<<define btn>>
<<define btnp>>
<<define circ>>
<<define circb>>
<<define clip>>
<<define cls>>
<<define elli>>
<<define ellib>>
<<define exit>>
<<define fget>>
<<define font>>
<<define fset>>
<<define key>>
<<define keyp>>
<<define line>>
<<define map>>
<<define memcpy>>
<<define memset>>
<<define mget>>
<<define mouse>>
<<define mset>>
<<define music>>
<<define peek>>
<<define peek1>>
<<define peek2>>
<<define peek4>>
<<define pix>>
<<define pmem>>
<<define poke>>
<<define poke1>>
<<define poke2>>
<<define poke4>>
<<define print>>
<<define rect>>
<<define rectb>>
<<define reset>>
<<define sfx>>
<<define spr>>
<<define sync>>
<<define time>>
<<define trace>>
<<define tri>>
<<define trib>>
<<define tstamp>>
<<define ttri>>
<<define vbank>>

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
