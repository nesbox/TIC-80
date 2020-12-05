// MIT License

// Copyright (c) 2020 Rob Loach @RobLoach

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

#if defined(TIC_BUILD_WITH_GRAVITY)

#include <string.h>
#include "gravity_compiler.h"
#include "gravity_macros.h"
#include "gravity_vmmacros.h"
#include "gravity_core.h"
#include "gravity_vm.h"
#include "gravity_delegate.h"
#include "gravity_memory.h"

#define GRAVITY_VERIFY_ARGS(function_name, min_arguments, max_arguments) \
    gravity_delegate_t* delegate = gravity_vm_delegate(vm); \
    if (delegate == NULL) { \
        printf("No delegate provided\n"); \
        RETURN_NOVALUE(); \
    } \
    tic_mem* tic = (tic_mem*)delegate->xdata; \
    if (tic == NULL) { \
        printf("No xdata provided\n"); \
        RETURN_NOVALUE(); \
    } \
    tic_core* core = (tic_core*)tic; \
    if ((nargs - 1) < (min_arguments)) { \
        do { \
            char buffer[1024];\
            snprintf(buffer, sizeof buffer, "%s() requires at least %i arguments, %i passed", function_name, min_arguments, nargs - 1); \
            core->data->error(core->data->data, buffer); \
            RETURN_NOVALUE(); \
        } while(0); \
    } \
    if ((nargs - 1) > (max_arguments)) { \
        do { \
            char buffer[1024];\
            snprintf(buffer, sizeof buffer, "%s() requires a maximum of %i arguments, %i passed", function_name, max_arguments, nargs - 1); \
            core->data->error(core->data->data, buffer); \
            RETURN_NOVALUE(); \
        } while(0); \
    }

static void gravity_error(gravity_vm *vm, error_type_t error_type, const char *message, error_desc_t error_desc, void *xdata) {
    tic_core* core = (tic_core*)xdata;
    char buffer[1024];
    const char *type = "N/A";

    if (!core) {
        return;
    }

    switch (error_type) {
        case GRAVITY_ERROR_NONE: type = "NONE"; break;
        case GRAVITY_ERROR_SYNTAX: type = "SYNTAX"; break;
        case GRAVITY_ERROR_SEMANTIC: type = "SEMANTIC"; break;
        case GRAVITY_ERROR_RUNTIME: type = "RUNTIME"; break;
        case GRAVITY_WARNING: type = "WARNING"; break;
        case GRAVITY_ERROR_IO: type = "I/O"; break;
    }

    snprintf(buffer, sizeof buffer, "%s: line %d, column %d: %s", type, error_desc.lineno, error_desc.colno, message);
    core->data->error(core->data->data, buffer);
}

static void gravity_log(gravity_vm *vm, const char *message, void *xdata) {
    tic_core* core = (tic_core*)xdata;
    if (core && message) {
        core->data->trace(core->data->data, message, TIC_DEFAULT_COLOR);
    }
}

static void gravity_close(tic_mem* tic) {
    tic_core* core = (tic_core*)tic;
    if (core) {
        gravity_vm* vm = core->gravity;
        if (vm) {
            printf("gravity_vm_free: Begin\n");
            gravity_vm_free(vm);
            printf("gravity_vm_free: Finished\n");
            gravity_core_free();
            core->gravity = NULL;
        }
    }
}

/**
 * Functions
 */
static bool gravity_print(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    GRAVITY_VERIFY_ARGS("print", 1, 7);
    char* text = VALUE_AS_CSTRING(args[1]);
    int x = 0;
    int y = 0;
    u8 color = TIC_DEFAULT_COLOR;
    bool fixed = false;
    s32 scale = 1;
    bool smallfont = false;
    
    if (nargs > 2) {
        x = VALUE_AS_INT(args[2]);
    }
    if (nargs > 3) {
        y = VALUE_AS_INT(args[3]);
    }
    if (nargs > 4) {
        color = VALUE_AS_INT(args[4]);
    }
    if (nargs > 5) {
        fixed = VALUE_AS_BOOL(args[5]);
    }
    if (nargs > 6) {
        scale = VALUE_AS_INT(args[6]);
    }
    if (nargs > 7) {
        smallfont = VALUE_AS_BOOL(args[7]);
    }

    s32 output = tic_api_print(tic, text, x, y, color, fixed, scale, smallfont);
    gravity_value_t rt = VALUE_FROM_INT(output);
    RETURN_VALUE(rt, rindex);
}

static bool gravity_btn(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    // GRAVITY_VERIFY_ARGS("btn", 1, 1);
    // int id = VALUE_AS_INT(args[1]);
    // printf("btn(%i): Start\n", id);
    // bool output = tic_api_btn(tic, id) > 0;
    // printf("btn(%i): End\n", id);
    // gravity_value_t outputValue = VALUE_FROM_BOOL(output);
    // RETURN_VALUE(outputValue, rindex);

    printf("btn()\n");
    GRAVITY_VERIFY_ARGS("btn", 0, 1);

    if (nargs <= 1) {
        RETURN_VALUE(VALUE_FROM_INT(core->memory.ram.input.gamepads.data), rindex);
    }

    int64_t index = VALUE_AS_INT(args[1]);
    s32 i = index & 0x1f;

    // TODO(RobLoach): Fix segment fault happening here on core->memory.ram.input.gamepads.data
    printf("btn(%i): Start\n", i);
    printf("btn otutput: %i\n", core->memory.ram.input.gamepads.data);
    if (core->memory.ram.input.gamepads.data <= 0) {
        RETURN_VALUE(VALUE_FROM_BOOL(false), rindex);
    }
    bool output = core->memory.ram.input.gamepads.data & (1 << i);
    printf("btn(%i): %d End\n", i, output);
    RETURN_VALUE(VALUE_FROM_BOOL(output), rindex);

}

static bool gravity_cls(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    GRAVITY_VERIFY_ARGS("cls", 1, 1);
    tic_api_cls(tic, VALUE_AS_INT(args[1]));
    RETURN_NOVALUE();
}

static bool gravity_spr(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    GRAVITY_VERIFY_ARGS("spr", 3, 9);
    s32 index = VALUE_AS_INT(args[1]);
    s32 x = VALUE_AS_INT(args[2]);
    s32 y = VALUE_AS_INT(args[3]);
    u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;
    s32 scale = 1;
    tic_flip flip = 0;
    tic_rotate rotate = 0;
    s32 w = 0;
    s32 h = 0;
    if (nargs > 4) {
        // TODO: Allow retrieving color arrays.
        colors[0] = VALUE_AS_INT(args[4]);
        count = 1;
    }
    if (nargs > 5) {
        scale = VALUE_AS_INT(args[5]);
    }
    if (nargs > 6) {
        flip = (tic_flip)VALUE_AS_INT(args[6]);
    }
    if (nargs > 7) {
        scale = (tic_rotate)VALUE_AS_INT(args[7]);
    }
    if (nargs > 8) {
        w = VALUE_AS_INT(args[8]);
    }
    if (nargs > 9) {
        h = VALUE_AS_INT(args[9]);
    }

    tic_api_spr(tic, index, x, y, w, h, colors, count, scale, flip, rotate);
    RETURN_NOVALUE();
}

static bool gravity_trace(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    GRAVITY_VERIFY_ARGS("trace", 1, 2);
    const char* text = VALUE_AS_CSTRING(args[1]);
    u8 color = TIC_DEFAULT_COLOR;
    if (nargs > 2 && VALUE_ISA_INT(args[2])) {
        color = VALUE_AS_INT(args[2]);
    }
    tic_api_trace(tic, text, color);
    RETURN_NOVALUE();
}

static void gravity_setup(tic_mem* tic, gravity_vm *vm) {
    gravity_vm_setvalue(vm, "print", NEW_CLOSURE_VALUE(gravity_print));
    gravity_vm_setvalue(vm, "btn", NEW_CLOSURE_VALUE(gravity_btn));
    gravity_vm_setvalue(vm, "cls", NEW_CLOSURE_VALUE(gravity_cls));
    gravity_vm_setvalue(vm, "spr", NEW_CLOSURE_VALUE(gravity_spr));
    gravity_vm_setvalue(vm, "trace", NEW_CLOSURE_VALUE(gravity_trace));
}

/**
 * Provides some initial gravity pre-code to ease the TIC-80 integration.
 */
static const char* gravity_precode(void *xdata) {
  return "extern var btn;"
         "extern var cls;"
         "extern var print;"
         "extern var spr;"
         "extern var trace;";
}

/**
 * Initialization
 */
static bool gravity_init(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    gravity_close(tic);

    // Put together the Gravity options.
    gravity_delegate_t delegate = {
        .error_callback = gravity_error,
        .log_callback = gravity_log,
        .report_null_errors = true,
        .xdata = tic,
        .precode_callback = gravity_precode
    };

    // Construct the compiler.
    gravity_compiler_t *compiler = gravity_compiler_create(&delegate);
    if (!compiler) {
        core->data->error(core->data->data, "Error creating compiler.");
        return false;
    }

    // Interpret the code into the compiler.
    gravity_closure_t *closure = gravity_compiler_run(compiler, code, strlen(code), 0, true, true);
    if (!closure) {
        gravity_compiler_free(compiler);
        return false;
    }

    // Set up the virtual machine.
    gravity_vm *vm = gravity_vm_new(&delegate);
    if (!vm) {
        gravity_compiler_free(compiler);
        return false;
    }

    // Transfer the memory to the virtual machine, and clean up the compiler.
    gravity_compiler_transfer(compiler, vm);
    gravity_compiler_free(compiler);

    // Setup the TIC-80 API.
    gravity_setup(tic, vm);

    // Load the closure into the virtual machine context.
    gravity_vm_loadclosure(vm, closure);
    gravity_closure_free(vm, closure);

    // Set up the virtual machine as the active TIC-80 Gravity context.
    core->gravity = vm;

    return true;
}

static void gravity_tick(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    if (!core)
        return;

    gravity_vm *vm = core->gravity;
    if (!vm)
        return;

    // TODO: Move this to initGravity() to save time.
    gravity_value_t tic_function = gravity_vm_getvalue(vm, "TIC", 3);
    if(!VALUE_ISA_CLOSURE(tic_function))
    {
        core->data->error(core->data->data, "func TIC() not found");
        return;
    }

    gravity_closure_t *tic_closure = VALUE_AS_CLOSURE(tic_function);
    gravity_vm_runclosure(vm, tic_closure, VALUE_FROM_NULL, NULL, 0);
}

static void gravity_scanline(tic_mem* tic, s32 row, void* data)
{
    tic_core* core = (tic_core*)tic;
    if (!core)
        return;

    gravity_vm *vm = core->gravity;
    if (!vm)
        return;

    // TODO: Move this to initGravity() to save time.
    gravity_value_t scn_function = gravity_vm_getvalue(vm, SCN_FN, strlen(SCN_FN));
    if (!VALUE_ISA_CLOSURE(scn_function))
        return;

    // convert function to closure
    gravity_closure_t *scn_closure = VALUE_AS_CLOSURE(scn_function);
    gravity_value_t p1 = VALUE_FROM_INT(row);
    gravity_value_t params[] = {p1};
    gravity_vm_runclosure(vm, scn_closure, VALUE_FROM_NULL, params, 1);
}

static void gravity_overline(tic_mem* tic, void* data)
{
    tic_core* core = (tic_core*)tic;
    if (!core)
        return;
    gravity_vm *vm = core->gravity;
    if (!vm)
        return;

    // TODO: Move this to initGravity() to save time.
    gravity_value_t ovr_function = gravity_vm_getvalue(vm, OVR_FN, strlen(OVR_FN));
    if(!VALUE_ISA_CLOSURE(ovr_function)) {
        return;
    }

    gravity_closure_t *ovr_closure = VALUE_AS_CLOSURE(ovr_function);
    gravity_vm_runclosure(vm, ovr_closure, VALUE_FROM_NULL, NULL, 0);
}

static const char* const GravityKeywords [] =
{
    "class", "var", "func", "return" "if", "is", "String", "Int",
    "Object", "Float", "Bool", "Null", "Class", "Function", "Fiber",
    "List", "Map", "Range", "enum", "count", "System", "push",
    "contains", "join", "map", "filter", "reduce", "sort", "hasKey",
    "keys", "disassemble", "static", "print", "bind", "unbind",
    "public", "private", "in", "loop", "call", "isDone", "put", "Math",
    "random", "true", "false", "init"
};

static const tic_script_config GravitySyntaxConfig =
{
    .init               = gravity_init,
    .close              = gravity_close,
    .tick               = gravity_tick,
    .scanline           = gravity_scanline,
    .overline           = gravity_overline,

    .getOutline         = NULL, // TODO: Implement getGravityOutline()
    .eval               = NULL, // TODO: Implement callGravityEval()

    .blockCommentStart  = "/*",
    .blockCommentEnd    = "*/",
    .blockCommentStart2 = NULL,
    .blockCommentEnd2   = NULL,
    .blockStringStart   = NULL,
    .blockStringEnd     = NULL,
    .singleComment      = "//",

    .keywords           = GravityKeywords,
    .keywordsCount      = COUNT_OF(GravityKeywords),
};

const tic_script_config* getGravityScriptConfig()
{
    return &GravitySyntaxConfig;
}


#endif /* defined(TIC_BUILD_WITH_GRAVITY) */
