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

// TODO: Remove stdio.
#include <stdio.h>
#include "gravity_compiler.h"
#include "gravity_macros.h"
#include "gravity_core.h"
#include "gravity_vm.h"
#include "gravity_delegate.h"

// error callback
static void gravityReportError(gravity_vm *vm, error_type_t error_type, const char *message, error_desc_t error_desc, void *xdata) {
    const char *type = "N/A";
    switch (error_type) {
        case GRAVITY_ERROR_NONE: type = "NONE"; break;
        case GRAVITY_ERROR_SYNTAX: type = "SYNTAX"; break;
        case GRAVITY_ERROR_SEMANTIC: type = "SEMANTIC"; break;
        case GRAVITY_ERROR_RUNTIME: type = "RUNTIME"; break;
        case GRAVITY_WARNING: type = "WARNING"; break;
        case GRAVITY_ERROR_IO: type = "I/O"; break;
    }

    // TODO: Replace printf with actual error report.
    if (error_type == GRAVITY_ERROR_RUNTIME) {
      printf("RUNTIME ERROR: ");
    }
    else {
      printf("%s ERROR on %d (%d,%d): ", type, error_desc.fileid, error_desc.lineno, error_desc.colno);
    }
    printf("%s\n", message);
}

static void closeGravity(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;

    if(core->gravity)
    {
        gravity_vm_free(core->gravity);
        gravity_core_free();
        core->gravity = NULL;
    }
}

static bool initGravity(tic_mem* tic, const char* code)
{
    tic_core* core = (tic_core*)tic;
    closeGravity(tic);

    gravity_delegate_t delegate = {
        .error_callback = report_error,
        .xdata = tic
    };

    // compile source into a closure
    gravity_compiler_t *compiler = gravity_compiler_create(&delegate);
    gravity_closure_t *closure = gravity_compiler_run(compiler, code, strlen(code), false, true);
    if (!closure) {
        gravity_compiler_free(compiler);
        return false;
    }

    // setup a new VM and a new fiber
    gravity_vm *vm = gravity_vm_new(&delegate);
    if (!vm) {
      gravity_compiler_free(compiler);
      return false;
    }

    // transfer memory from compiler to VM and then free compiler
    gravity_compiler_transfer(compiler, vm);
    gravity_compiler_free(compiler);

    // load closure into VM context
    gravity_vm_loadclosure(vm, closure);

    /*duk_context* duktape = core->gravity;

    if (duk_pcompile_string(duktape, 0, code) != 0 || duk_peval_string(duktape, code) != 0)
    {
        core->data->error(core->data->data, duk_safe_to_stacktrace(duktape, -1));
        duk_pop(duktape);
        return false;
    }*/

    core->gravity = vm;

    return true;
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
    .init               = initGravity,
    .close              = closeGravity,
    .tick               = callGravityTick,
    .scanline           = callGravityScanline,
    .overline           = callGravityOverline,

    .getOutline         = getGravityOutline,
    .eval               = evalGravity,

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
