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
    mp_obj_t TIC_fun;
    mp_obj_t OVR_fun;
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
mp_state_ctx_t mp_state_ctx = {};

uint mp_import_stat(const char *path) {
    fprintf(stderr, "mp_import_stat(%s)\n", path);
    // struct stat st;
    // if (stat(path, &st) == 0) {
    //     if (S_ISDIR(st.st_mode)) {
    //         return MP_IMPORT_STAT_DIR;
    //     } else if (S_ISREG(st.st_mode)) {
    //         return MP_IMPORT_STAT_FILE;
    //     }
    // }
    return MP_IMPORT_STAT_NO_EXIST;
}

void nlr_jump_fail(void *val) {
    fprintf(stderr, "FATAL: uncaught NLR %p\n", val);
    exit(1);
}


static bool initPython(tic_mem* tic, const char* code)
{
    fprintf(stderr, "initPython(%p, %s)\n", tic, code);

    mp_stack_set_limit(40000 * (BYTES_PER_WORD / 4));
    gc_init(heap, heap + sizeof(heap));
    mp_init();

    tic_core* core = (tic_core*)tic;
	core->python = &python_vm;
	memset(&python_vm, 0, sizeof(PythonVM));

    // Note: nlr_push/pop mechanism is like try/catch for exceptions
    // but uses setjmp (or similar). The body of this if statement will
    // run as normal, but if a micropython exception happens, then
    // execution will jump back to this if statement and the else clause
    // will run - printing the exception.
	nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr src_name = 1/*MP_QSTR_*/;
        mp_lexer_t *lex = mp_lexer_new_from_str_len(src_name, code, strlen(code), false);
        mp_parse_tree_t pt = mp_parse(lex, MP_PARSE_FILE_INPUT);
        mp_obj_t module_fun = mp_compile(&pt, src_name, false);
        mp_call_function_0(module_fun);
        python_vm.module_fun = module_fun;

        // try/catch in case mp_load_global fails (e.g. TIC not found).
        if (nlr_push(&nlr) == 0) {
            python_vm.TIC_fun = mp_load_global(qstr_from_str("TIC"));
            nlr_pop();
        }else{
            fprintf(stderr, "No TIC function found?\n");
            mp_obj_print_exception(&mp_plat_print, ((mp_obj_t)nlr.ret_val));
        }

        // qstr xstr = qstr_from_str("x");
        // fprintf(stderr, "xstr = %d\n", (int)xstr);
        // mp_obj_t xobj = mp_load_global(xstr);
        // fprintf(stderr, "xobj = %p\n", xobj);
        // mp_int_t x = mp_obj_get_int(xobj);
        // fprintf(stderr, "x = %d\n", (int)x);

        // mp_int_t y = mp_obj_get_int(mp_load_global(qstr_from_str("y")));
        // fprintf(stderr, "y = %d\n", (int)y);

        // const char* foo = mp_obj_str_get_str(mp_load_global(qstr_from_str("foo")));
        // fprintf(stderr, "foo = %s\n", foo);

        nlr_pop();
        return true;
    } else {
        // uncaught exception
        mp_obj_print_exception(&mp_plat_print, ((mp_obj_t)nlr.ret_val));
        return false;
    }
	return true;
}

static void closePython(tic_mem* tic)
{
    // fprintf(stderr, "closePython(%p)\n", tic);
    mp_deinit();
}

static void callPythonTick(tic_mem* tic)
{
    // fprintf(stderr, "callPythonTick(%p)\n", tic);
    if (python_vm.TIC_fun != 0) {
        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            mp_call_function_0(python_vm.TIC_fun);
            nlr_pop();
        }else{
            // uncaught exception
            mp_obj_print_exception(&mp_plat_print, ((mp_obj_t)nlr.ret_val));
        }
    }
}

static void callPythonScanline(tic_mem* tic, s32 row, void* data)
{
    // fprintf(stderr, "callPythonScanline(%p, %d)\n", tic, row);
}

static void callPythonOverline(tic_mem* tic, void* data)
{
    // fprintf(stderr, "callPythonOverline(%p)\n", tic);
    // python_vm.OVR_fun = mp_load_global(qstr_from_str("OVR"));
    // fprintf(stderr, "OVR = %p\n", python_vm.OVR_fun);
}

static const tic_outline_item* getPythonOutline(const char* code, s32* size)
{
    fprintf(stderr, "NOT IMPLEMENTED getPythonOutline(...)\n");
	*size = 0;
    static tic_outline_item* items = NULL;
	return items;
}

static void evalPython(tic_mem* tic, const char* code)
{
    fprintf(stderr, "NOT IMPLEMENTED evalPython(%p, %s)\n", tic, code);
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
