#include "core/core.h"

#include "pocketpy.h"
#include <stdio.h>
#include <string.h>

/***************DEBUG SESSION*************/
//cmake -A x64 -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SDLGPU=On -DBUILD_STATIC=On -DBUILD_WITH_ALL=Off -DBUILD_WITH_PYTHON=On ..
//
//cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SDLGPU=On -DBUILD_WITH_ALL=On ..
//
extern bool parse_note(const char* noteStr, s32* note, s32* octave);

struct names
{
    py_Name _tic_core;
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

    tic_core* core = (tic_core*)tic;
    close_pkpy_v2(tic);
    py_initialize();
    setup_core(core);
    if (!bind_pkpy_v2())
    {
        //throw error

        return false;
    }
    if (!py_exec(code, "main.py", EXEC_MODE, NULL))
    {
        //throw error

        return false;
    }
    return true;
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
        .tick = NULL,
        .boot = NULL,

        .callback =
            {
                .scanline = NULL,
                .border = NULL,
                .menu = NULL,
            },

        .getOutline = NULL,
        .eval = NULL,

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
