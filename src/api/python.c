#include "core/core.h"

#include "pocketpy.h"
#include "pocketpy/objects/base.h"
#include <stdio.h>
#include <string.h>

/***************DEBUG SESSION*************/
//cmake -A x64 -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SDLGPU=On -DBUILD_STATIC=On -DBUILD_WITH_ALL=Off -DBUILD_WITH_PYTHON=On ..
//for debug
//cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SDLGPU=On -DBUILD_WITH_ALL=On ..
//for release
extern bool parse_note(const char* noteStr, s32* note, s32* octave);

struct names
{
    py_Name _tic_core;
    py_Name len;
    py_Name __getitem__;
    py_Name TIC;
    py_Name BOOT;
    py_Name SCN;
    py_Name BDR;
    py_Name MENU;
}N;

//set value of "name" to stack.top
static void pkpy_setglobal_2(const char* name)
{
    py_setglobal(py_name(name), py_peek(-1));
}

static bool get_core(tic_core** core)
{
    py_ItemRef pycore = py_getglobal(N._tic_core);
    if (!pycore) return false;
    *core = (tic_core*)py_toint(pycore);
    return true;
}

//set _tic_core to the given core
static bool setup_core(tic_core* core)
{
    py_newint(py_retval(), (py_i64)core);
    py_setglobal(N._tic_core, py_retval());
    return true;
}

//index should be a positive index
//_NOTICE: py_peek(-1) takes stack.top, but pkpy.v1 takes stack.top with index 0
static int prepare_colorindex(int index, u8* buffer)
{
    if (py_istype(py_peek(-index), tp_int))
    {
        int value;
        value = (int)py_toint(py_peek(-index));

        if (value == -1)
            return 0;
        else
        {
            buffer[0] = value;
            return 1;
        }
    }
    else
    { //should be a list then
        int list_len = py_list_len(py_peek(-index));
        list_len = (list_len < TIC_PALETTE_SIZE) ? (list_len) : (TIC_PALETTE_SIZE);
        for (int i = 0; i < list_len; i++)
        {
            py_ItemRef pylist_item = py_list_getitem(py_peek(-index), i);
            buffer[i] = py_toint(pylist_item);
        }
    }
}

/*****************TIC-80 API BEGIN*****************/

static bool py_btn(int argc, py_Ref argv)
{
    int button_id;
    tic_core* core;
    get_core(&core);
    tic_mem* tic = (tic_mem*)core;

    PY_CHECK_ARG_TYPE(0, tp_int);
    //or get button_id from argv[0]?
    button_id = (int)py_toint(py_peek(-1));
    bool pressed = core->api.btn(tic, button_id & 0x1f);

    py_newbool(py_retval(), pressed);
    py_push(py_retval());
    return 1;
}

static bool py_btnp(int argc, py_Ref argv)
{

    int button_id;
    int hold;
    int period;

    button_id = py_toint(py_peek(-1));
    hold = py_toint(py_peek(-2));
    period = py_toint(py_peek(-3));
    tic_core* core;
    get_core(&core);
    tic_mem* tic = (tic_mem*)core;

    bool pressed = core->api.btnp(tic, button_id, hold, period);
    py_newbool(py_retval(), pressed);
    py_push(py_retval());
    return 1;
}

/*************TIC-80 MISC BEGIN****************/

static void throw_error(tic_core* core, const char* msg)
{
    core->data->error(core->data->data, msg);
    
}

static bool bind_pkpy_v2()
{
    py_GlobalRef mod = py_getmodule("__main__");
    py_bind(mod, "btn(id: int) -> bool", py_btn);
    py_bind(mod, "btnp(id: int, hold=-1, period=-1) -> bool", py_btnp);
    
    return true;
}

void close_pkpy_v2(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    if (core->currentVM)
    {
        core->currentVM = NULL;
        py_finalize();
    }
}

static bool init_pkpy_v2(tic_mem* tic, const char *code)
{
    //maybe some config here
    //N._tic_core = py_name("_tic_core");
    //N.len = py_name("len");
    //N.__getitem__ = py_name("__getitem__");
    //N.TIC = py_name("TIC");
    //N.BOOT = py_name("BOOT");
    //N.SCN = py_name("SCN");
    //N.BDR = py_name("BDR");
    //N.MENU = py_name("MENU");


    tic_core* core = (tic_core*)tic;
    close_pkpy_v2(tic);
    py_initialize();
    setup_core(core);
    core->currentVM = pk_current_vm;
    if (!bind_pkpy_v2())
    {
        //throw error
        throw_error(core, "Binding func failed!");
        return false;
    }
    if (!py_exec(code, "main.py", EXEC_MODE, NULL))
    {
        //throw error
        throw_error(core, "Executing code failed!");
        return false;
    }
    return true;
}

void tick_pkpy_v2(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) return;//no vm

    py_GlobalRef py_tick = py_getglobal(N.TIC);
    py_push(py_tick);
    py_pushnil();
    if (!py_vectorcall(0, 0))
    {
        throw_error(core, "TIC running error!");
    }
    else
    {
        auto test = py_retval();
    }
}

void boot_pkpy_v2(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) return; //no vm

    py_GlobalRef py_boot = py_getglobal(py_name("BOOT"));
    py_push(py_boot);
    py_pushnil();
    if (!py_vectorcall(0, 0))
    {
        throw_error(core, "BOOT running error!");
    }
    else
    {
        auto test = py_retval();
    }
}

void callback_scanline(tic_mem* tic, s32 row, void* data)
{
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) return; //no vm

    py_GlobalRef py_scn = py_getglobal(py_name("SCN"));
    py_push(py_scn);
    py_pushnil();
    py_Ref py_row = (py_Ref)malloc(sizeof(py_Ref));
    py_newint(py_row, row);
    py_push(py_row);
    if (!py_vectorcall(1, 0))
    {
        throw_error(core, "SCANLINE running error!");
    }
    else
    {
        auto test = py_retval();
    }
    free(py_row);
}

void callback_border(tic_mem* tic, s32 row, void* data)
{
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) return; //no vm

    py_GlobalRef py_bdr = py_getglobal(py_name("BDR"));
    py_push(py_bdr);
    py_pushnil();
    py_Ref py_row = (py_Ref)malloc(sizeof(py_Ref));
    py_newint(py_row, row);
    py_push(py_row);
    if (!py_vectorcall(1, 0))
    {
        throw_error(core, "BORDER running error!");
    }
    else
    {
        auto test = py_retval();
    }
    free(py_row);
}
void callback_menu(tic_mem* tic, s32 index, void* data)
{
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) return; //no vm

    py_GlobalRef py_menu = py_getglobal(py_name("MENU"));
    py_push(py_menu);
    py_pushnil();
    py_Ref py_idx = (py_Ref)malloc(sizeof(py_Ref));
    py_newint(py_idx, index);
    py_push(py_idx);
    if (!py_vectorcall(1, 0))
    {
        throw_error(core, "MENU running error!");
    }
    else
    {
        auto test = py_retval();
    }
    free(py_idx);
}


static const char* const PythonKeywords[] =
    {
        "is not", "not in", "yield from",
        "class", "import", "as", "def", "lambda", "pass", "del", "from", "with", "yield",
        "None", "in", "is", "and", "or", "not", "True", "False", "global", "try", "except", "finally",
        "while", "for", "if", "elif", "else", "break", "continue", "return", "assert", "raise"};

static const u8 DemoRom[] =
    {
#include "../build/assets/pythondemo.tic.dat"
};

static const u8 MarkRom[] =
    {
#include "../build/assets/pythonmark.tic.dat"
};

TIC_EXPORT const tic_script EXPORT_SCRIPT(Python) =
    {
        .id = 20,
        .name = "python",
        .fileExtension = ".py",
        .projectComment = "#",
        .init = init_pkpy_v2,
        .close = close_pkpy_v2,
        .tick = tick_pkpy_v2,
        .boot = boot_pkpy_v2,

        .callback =
            {
                .scanline = callback_scanline,
                .border = callback_border,
                .menu = callback_menu,
            },

        .getOutline = NULL,
        .eval = NULL,
        //above is a must need
        .blockCommentStart = NULL,
        .blockCommentEnd = NULL,
        .blockCommentStart2 = NULL,
        .blockCommentEnd2 = NULL,
        .singleComment = "#",
        .blockStringStart = NULL,
        .blockStringEnd = NULL,
        .blockEnd = NULL,
        .stdStringStartEnd = "'\"",

        .keywords = PythonKeywords,
        .keywordsCount = COUNT_OF(PythonKeywords),

        .demo = {DemoRom, sizeof DemoRom},
        .mark = {MarkRom, sizeof MarkRom, "pythonmark.tic"},
};
