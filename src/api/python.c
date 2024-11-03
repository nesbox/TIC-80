#include "core/core.h"

#include <stdio.h>
#include <string.h>
#include "pocketpy.h"
#include "pocketpy/objects/base.h"

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
} N;

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
    ++index;
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
//API is what u bind to module "__main__" in pkpy
//when python use func like btn(id: int)
//it's equal to py_btn(1, [id]), which passes the arg id
//That's an interpreter

//Structure of APIs:
//claim args
//check arg type
//claim tic_core + get arg
//check vm exceptions
//use api in C
//if needed, write back result to python

static bool py_btn(int argc, py_Ref argv)
{
    int button_id;
    tic_core* core;
    get_core(&core);
    tic_mem* tic = (tic_mem*)core;

    PY_CHECK_ARG_TYPE(0, tp_int); //check argv[0] type
    button_id = py_toint(py_arg(0));

    if (py_checkexc(0)) return false;
    //or get button_id from argv[0]?
    //button_id = (int)py_toint(py_peek(-1));
    bool pressed = core->api.btn(tic, button_id & 0x1f);

    py_newbool(py_retval(), pressed);
    return true;
}

static bool py_btnp(int argc, py_Ref argv)
{

    int button_id;
    int hold;
    int period;

    PY_CHECK_ARG_TYPE(0, tp_int);
    //button_id = py_toint(py_peek(-1));
    //hold = py_toint(py_peek(-2));
    //period = py_toint(py_peek(-3));
    button_id = py_toint(py_arg(0));
    hold = py_toint(py_arg(1));
    period = py_toint(py_arg(2));
    tic_core* core;
    get_core(&core);
    tic_mem* tic = (tic_mem*)core;
    if (py_checkexc(0)) return false;

    bool pressed = core->api.btnp(tic, button_id, hold, period);
    py_newbool(py_retval(), pressed);
    return true;
}

static void py_cls(int argc, py_Ref argv)
{
    int color;
    PY_CHECK_ARG_TYPE(0, tp_int);
    tic_core* core;
    get_core(&core);
    tic_mem* tic = (tic_mem*)core;
    if (py_checkexc(0)) return;

    color = py_toint(py_arg(0));
    core->api.cls(tic, color);
    return;
}

static void py_spr(int argc, py_Ref argv)
{
    int spr_id;
    int x;
    int y;
    int color_count;
    int scale;
    int flip;
    int rotate;
    int w;
    int h;
    char colors[16];
    for (int i = 0; i < 9; i++)
        PY_CHECK_ARG_TYPE(i, tp_int);
    tic_core* core;
    get_core(&core);
    tic_mem* tic = (tic_mem*)core;
    spr_id = py_toint(py_arg(0));
    x = py_toint(py_arg(1));
    y = py_toint(py_arg(2));
    scale = py_toint(py_arg(4));
    flip = py_toint(py_arg(5));
    rotate = py_toint(py_arg(6));
    w = py_toint(py_arg(7));
    h = py_toint(py_arg(8));
    color_count = prepare_colorindex(3, colors);
    if (py_checkexc(0)) return;

    core->api.spr(tic, spr_id, x, y, w, h, colors, color_count, scale, flip, rotate);
}

static void py_print(int argc, py_Ref argv)
{
    const char* str;
    int x, y, color, scale;
    bool fixed, alt;

    PY_CHECK_ARG_TYPE(0, tp_str);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    PY_CHECK_ARG_TYPE(3, tp_int);
    PY_CHECK_ARG_TYPE(4, tp_bool);
    PY_CHECK_ARG_TYPE(5, tp_int);
    PY_CHECK_ARG_TYPE(6, tp_bool);

    str = py_tostr(py_arg(0));
    x = py_toint(py_arg(1));
    y = py_toint(py_arg(2));
    color = py_toint(py_arg(3));
    fixed = py_tobool(py_arg(4));
    scale = py_toint(py_arg(5));
    alt = py_tobool(py_arg(6));
    tic_core* core;
    get_core(&core);
    tic_mem* tic = (tic_mem*)core;
    if (py_checkexc(0)) return;

    s32 ps = core->api.print(tic, str, x, y, color, fixed, scale, alt);
    py_newint(py_retval(), ps);
}

/*************TIC-80 MISC BEGIN****************/

static void throw_error(tic_core* core, const char* msg)
{
    core->data->error(core->data->data, py_formatexc());
}

static bool bind_pkpy_v2()
{
    py_GlobalRef mod = py_getmodule("__main__");
    py_bind(mod, "btn(id: int) -> bool", py_btn);
    py_bind(mod, "btnp(id: int, hold=-1, period=-1) -> bool", py_btnp);
    py_bind(mod, "cls(color=0)", py_cls);
    py_bind(mod, "spr(id: int, x: int, y: int, colorkey=-1, scale=1, flip=0, rotate=0, w=1, h=1)",
            py_spr);
    py_bind(mod, "print(text, x=0, y=0, color=15, fixed=False, scale=1, alt=False)", py_print);
    return true;
}

void close_pkpy_v2(tic_mem* tic)
{
    tic_core* core = (tic_core*)tic;
    if (core->currentVM)
    {
        core->currentVM = NULL;
        py_resetvm();
        //py_initialize();//reset
    }
}

static bool init_pkpy_v2(tic_mem* tic, const char* code)
{
    //maybe some config here
    py_initialize();
    tic_core* core = (tic_core*)tic;
    close_pkpy_v2(tic);
    N._tic_core = py_name("_tic_core");
    N.len = py_name("len");
    N.__getitem__ = py_name("__getitem__");
    N.TIC = py_name("TIC");
    N.BOOT = py_name("BOOT");
    N.SCN = py_name("SCN");
    N.BDR = py_name("BDR");
    N.MENU = py_name("MENU");

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
    if (!core->currentVM) return; //no vm

    py_GlobalRef py_tick = py_getglobal(N.TIC);
    if (!py_tick) return;
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

    py_GlobalRef py_boot = py_getglobal(N.BOOT);
    if (!py_boot) return;
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

    py_GlobalRef py_scn = py_getglobal(N.SCN);
    if (!py_scn) return;
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

    py_GlobalRef py_bdr = py_getglobal(N.BDR);
    if (!py_bdr) return;
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

    py_GlobalRef py_menu = py_getglobal(N.MENU);
    if (!py_menu) return;
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
