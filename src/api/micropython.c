// micropython.c

#include "core/core.h"

#if defined(TIC_BUILD_WITH_PYTHON)

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "py/compile.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/stackctrl.h"

struct PythonVM {
	mp_obj_t module_fun;
};

typedef struct PythonVM PythonVM;

#if 0

mp_obj_t execute_from_str(const char *str) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr src_name = 1/*MP_QSTR_*/;
        mp_lexer_t *lex = mp_lexer_new_from_str_len(src_name, str, strlen(str), false);
        mp_parse_tree_t pt = mp_parse(lex, MP_PARSE_FILE_INPUT);
        mp_obj_t module_fun = mp_compile(&pt, src_name, false);
        mp_call_function_0(module_fun);
        nlr_pop();
        return 0;
    } else {
        // uncaught exception
        return (mp_obj_t)nlr.ret_val;
    }
}

int py_main() {
    // Initialized stack limit
    mp_stack_set_limit(40000 * (BYTES_PER_WORD / 4));
    // Initialize heap
    gc_init(heap, heap + sizeof(heap));
    // Initialize interpreter
    mp_init();

    const char str[] = "print('Hello world of easy embedding!')";
    if (execute_from_str(str)) {
        printf("Error\n");
    }
}

uint mp_import_stat(const char *path) {
    return MP_IMPORT_STAT_NO_EXIST;
}

void nlr_jump_fail(void *val) {
    printf("FATAL: uncaught NLR %p\n", val);
    exit(1);
}
#endif

static PythonVM python_vm;
static char heap[16384];

static bool initPython(tic_mem* tic, const char* code)
{
    // mp_stack_set_limit(40000 * (BYTES_PER_WORD / 4));
    // gc_init(heap, heap + sizeof(heap));
    // mp_init();

    tic_core* core = (tic_core*)tic;
	core->python = &python_vm;
	memset(&python_vm, sizeof(PythonVM), 0);

	// nlr_buf_t nlr;
 //    if (nlr_push(&nlr) == 0) {
 //        qstr src_name = 1/*MP_QSTR_*/;
 //        mp_lexer_t *lex = mp_lexer_new_from_str_len(src_name, code, strlen(code), false);
 //        mp_parse_tree_t pt = mp_parse(lex, MP_PARSE_FILE_INPUT);
 //        mp_obj_t module_fun = mp_compile(&pt, src_name, false);
 //        mp_call_function_0(module_fun);
 //        python_vm.module_fun = module_fun;
 //        nlr_pop();
 //        return true;
 //    } else {
 //        // uncaught exception
 //        //return (mp_obj_t)nlr.ret_val;
 //        return false;
 //    }
	return true;
}

static void closePython(tic_mem* tic)
{
	;
}

static void callPythonTick(tic_mem* tic)
{
	;
}

static void callPythonScanline(tic_mem* tic, s32 row, void* data)
{
	;
}

static void callPythonOverline(tic_mem* tic, void* data)
{
	;
}

static const tic_outline_item* getPythonOutline(const char* code, s32* size)
{
	*size = 0;
    static tic_outline_item* items = NULL;
	return items;
}

static void evalPython(tic_mem* tic, const char* code)
{
	;
}

static const char* const PythonKeywords [] =
{
	"and",
	"as",
	"assert",
	"break",
	"class",
	"continue",
	"def",
	"del",
	"elif",
	"else",
	"except",
	"False",
	"finally",
	"for",
	"from",
	"global",
	"if",
	"import",
	"in",
	"is",
	"lambda",
	"None",
	"nonlocal",
	"not",
	"or",
	"pass",
	"raise",
	"return",
	"True",
	"try",
	"while",
	"with",
	"yield"
	// await
	// async
};

static const tic_script_config PythonSyntaxConfig = 
{
    .init               = initPython,
    .close              = closePython,
    .tick               = callPythonTick,
    .scanline           = callPythonScanline,
    .overline           = callPythonOverline,

    .getOutline         = getPythonOutline,
    .eval               = evalPython,

    .blockCommentStart  = "\"\"\"",
    .blockCommentEnd    = "\"\"\"",
    .blockCommentStart2 = NULL,
    .blockCommentEnd2   = NULL,
    .blockStringStart   = "\"\"\"",
    .blockStringEnd     = "\"\"\"",
    .singleComment      = "#",

    .keywords           = PythonKeywords,
    .keywordsCount      = COUNT_OF(PythonKeywords),
};

const tic_script_config* getPythonScriptConfig()
{
    return &PythonSyntaxConfig;
}

#endif
