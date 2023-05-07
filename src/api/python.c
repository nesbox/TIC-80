
#include "core/core.h"

#if defined(TIC_BUILD_WITH_PYTHON)

#include "pocketpy_c.h"
#include <stdio.h>

static bool get_core(pkpy_vm* vm, tic_core** core) 
{
    pkpy_get_global(vm, "_tic_core");
    return pkpy_to_voidp(vm, -1, (void**) core);
}

static bool setup_core(pkpy_vm* vm, tic_core* core) 
{
    if (!pkpy_push_voidp(vm, core)) return false;
    if (!pkpy_set_global(vm, "_tic_core")) return false;
    return true;
}

static int py_trace(pkpy_vm* vm) 
{
    tic_mem* tic;
    char* message = NULL;
    int color;

    pkpy_to_string(vm, 0, &message);
    pkpy_to_int(vm, 1, &color);
    get_core(vm, (tic_core**) &tic);
    if (pkpy_check_error(vm)) 
    {
        if (message != NULL) free(message);
        return 0;
    }

    tic_api_trace(tic, message, (u8) color);
    free(message);
    return 0;
}

static int py_cls(pkpy_vm* vm) 
{
    tic_mem* tic;
    int color;

    pkpy_to_int(vm, 0, &color);
    get_core(vm, (tic_core**) &tic);
    if (pkpy_check_error(vm)) 
        return 0;

    tic_api_cls(tic, (u8) color);
    return 0;
}

static int py_btn(pkpy_vm* vm) 
{
    tic_mem* tic;
    int button_id;

    get_core(vm, (tic_core**) &tic);
    pkpy_to_int(vm, 0, &button_id);
    if(pkpy_check_error(vm))
        return 0;

    bool pressed = tic_api_btn(tic, button_id & 0x1f);
    pkpy_push_bool(vm, pressed);
    return 1;
}

static bool setup_c_bindings(pkpy_vm* vm) {

    pkpy_push_function(vm, py_trace);
    pkpy_set_global(vm, "_trace");

    pkpy_push_function(vm, py_cls);
    pkpy_set_global(vm, "_cls");

    pkpy_push_function(vm, py_btn);
    pkpy_set_global(vm, "_btn");

    if(pkpy_check_error(vm))
        return false;

    return true;
}

static bool setup_py_bindings(pkpy_vm* vm) {
    pkpy_vm_run(vm, "def trace(message, color=15) : return _trace(message, color)\n");

    pkpy_vm_run(vm, "def cls(color=0) : return _cls(color)\n");

    //lua api does this for btn
    pkpy_vm_run(vm, "def btn(id=-1) : return _btn(id)");

    if(pkpy_check_error(vm))
        return false;

    return true;
}

void closePython(tic_mem* tic) 
{
    tic_core* core = (tic_core*)tic;

    if (core->currentVM)
    {
        pkpy_vm_destroy(core->currentVM);
        core->currentVM = NULL;
    }
}

static void report_error(tic_core* core, char* prefix) {
    pkpy_vm* vm = core->currentVM;
    core->data->error(core->data->data, prefix);
    char* message;
    if (!pkpy_clear_error(vm, &message)) 
        core->data->error(core->data->data, "error was thrown but not register (pocketpy c binding bug)");
    else
        core->data->error(core->data->data, message);
}

static bool initPython(tic_mem* tic, const char* code) 
{
    closePython(tic);
    tic_core* core = (tic_core*)tic;

    pkpy_vm* vm = pkpy_vm_create(false, false);

    core->currentVM = vm;

    if (!setup_core(vm, core)) {
        report_error(core, "problem binding tic_core (pocketpy c binding bug)\n");
        return false;
    }
    if (!setup_c_bindings(vm))
    {
        report_error(core, "problem setting up the c bindings (pocketpy c binding bug\n");
        return false;
    }
    if (!setup_py_bindings(vm))
    {
        report_error(core, "problem setting up the py bindings (pocketpy c binding bug\n");
        return false;
    }

    if(!pkpy_vm_run(vm, code)) 
    {
        report_error(core, "error while processing the main code\n");

        return false;
    }

    return true;
}

void callPythonTick(tic_mem* tic) 
{
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) 
        return;

    if (pkpy_check_global(core->currentVM, "TIC"))
    {
        pkpy_get_global(core->currentVM, "TIC");
        if(!pkpy_call(core->currentVM, 0))
            report_error(core, "error while running TIC\n");
    }
}
void callPythonBoot(tic_mem* tic) {
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) 
        return;

    if (pkpy_check_global(core->currentVM, "BOOT"))
    {
        pkpy_get_global(core->currentVM, "BOOT");
        if(!pkpy_call(core->currentVM, 0))
            report_error(core, "error while running BOOT\n");
    }
}
void callPythonScanline(tic_mem* tic, s32 row, void* data) {}
void callPythonBorder(tic_mem* tic, s32 row, void* data) {}
void callPythonMenu(tic_mem* tic, s32 row, void* data) {}

static const tic_outline_item* getPythonOutline(const char* code, s32* size) 
{
    return NULL;
}

void evalPython(tic_mem* tic, const char* code) 
{
    printf("TODO: python eval not yet implemented\n.");
}


static const char* const PythonKeywords[] =
{
    "def",
};


const tic_script_config PythonSyntaxConfig =
{
    .id                 = 20,
    .name               = "python",
    .fileExtension      = ".py",
    .projectComment     = "#",
    .init               = initPython,
    .close              = closePython,
    .tick               = callPythonTick,
    .boot               = callPythonBoot,

    .callback           =
    {
        .scanline       = callPythonScanline,
        .border         = callPythonBorder,
        .menu           = callPythonMenu,
    },

    .getOutline         = getPythonOutline,
    .eval               = evalPython,

    .blockCommentStart  = NULL,
    .blockCommentEnd    = NULL,
    .blockCommentStart2 = NULL,
    .blockCommentEnd2   = NULL,
    .singleComment      = "#",
    .blockStringStart   = NULL,
    .blockStringEnd     = NULL,
    .blockEnd           = NULL,
    .stdStringStartEnd  = "'\"",

    .keywords           = PythonKeywords,
    .keywordsCount      = COUNT_OF(PythonKeywords),
};

#endif/* defined(TIC_BUILD_WITH_PYTHON) */
