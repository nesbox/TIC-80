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
#include <Rinternals.h>
#include <Rembedded.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

tic_core *getTICCore(tic_mem* tic, const char* code) {
  tic_core *core;
  while (core == NULL && (!initR(tic, code))) core = (castTicMemory)->currentVM;
  return core;
}

#define killer(x)																								\
	if ((tic_core *core = (castTicMemory)->currentVM) != NULL) {	\
		Rf_endEmbeddedR(x);																					\
		core->currentVM = NULL;																			\
	}

static bool initR(tic_mem *tic, const char *code) {
	killer(0);

	int tries = 1;

tryOnceMoreOnly:
	/* embdRAV: embedded R argument vector. */
	char *embdRAV[]= { "REmbeddedInTIC80", "--silent" };
	/* NOTE: rcore should be an integer; TIC-80 won't be the any the wiser. */
	void *rcore = core = \
		Rf_initEmbeddedR(sizeof(embdRAV)/sizeof(embdRAV[0]), embdRAV);

	int rc = *((int *) rcore);
	if (rc == 0 || rc == 1 || rc == NULL)
		return (bool) rc;
	else if (tries--)
		goto tryOnceMoreOnly;

	return false;
}

static void closeR(tic_mem *tic) {
	killer(0);
}
static void callRfn_TIC80() {
	/* if (exists("TIC-80") && is.function(`TIC-80`)) `TIC-80`() */
	Rf_eval(Rf_mkString("if (exists(\"TIC-80\") && is.function(`TIC-80`)) "\
											"`TIC-80`()"));
}
#define defineCallRFn_(x)																								\
  static void callRfn_##x(tic_mem *tic) {																\
    Rf_eval(Rf_mkString("if (exists(\""#x"\") && is.function("#x")) "#x"()")); \
  }
/* if (exists("x") && is.function(x)) x() */
defineCallRFn_("MENU")
defineCallRFn_("BDR")
defineCallRFn_("BOOT")
defineCallRFn_("SCN")
#undef defineCallRFn_

<<SYNTAX HIGHLIGHTING>>
<<OUTLINE GENERATING>>

TIC_EXPORT const tic_script EXPORT_SCRIPT(R) =
{
	/* The first five members of the struct have the sum total following size. */
	/* sizeof(u8) + 3 * sizeof(char *)  */
	.id                     = 666,
	.name                   = "r",
	.fileExtension          = ".r",
	.projectComment         = "##",
	{
		.init                 = initR,
		.close                = closeR,
		.tick                 = callRfn_TIC80,
		.boot                 = callRfn_BOOT,

		.callback             =
		{
			.scanline           = callRfn_SCN,
			.border             = callRfn_Border, /* TODO */
			.menu               = callRfn_Menu, /* TODO*/
		},
	},

	.getOutline             = getROutline, /* TODO */
	.eval                   = mrEMachine,

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
	.api_keywords           = RAPIKeywords, /* TODO */
	.api_keywordsCount      = COUNT_OF(RAPIKeywords),
	.useStructuredEdition   = false,

	.keywords               = RKeywords,
	.keywordsCount          = COUNT_OF(RKeywords),

	.demo = {DemoRom, sizeof DemoRom}, /* TODO */
	.mark = {MarkRom, sizeof MarkRom, "rmark.tic"}, /* TODO*/
};
