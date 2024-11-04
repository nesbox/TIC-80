#include "core/core.h"

#include <stdio.h>
#include <string.h>
#include "pocketpy.h"
#include "pocketpy/objects/base.h"

//!!! search NOTICE for v1 remains !!!

/***************DEBUG SESSION*************/
//cmake -A x64 -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SDLGPU=On -DBUILD_STATIC=On -DBUILD_WITH_ALL=Off -DBUILD_WITH_PYTHON=On ..
//for debug
//cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SDLGPU=On -DBUILD_WITH_ALL=On ..
//for release
extern bool parse_note(const char* noteStr, s32* note, s32* octave);
static bool py_throw_error(tic_core* core, const char* msg);

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
static bool pkpy_setglobal_2(const char* name)
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
static bool prepare_colorindex(py_Ref index, u8* buffer)
{
    //++index;
    if (py_istype(index, tp_int))
    {
        int value;
        value = py_toint(index);

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
        int list_len = py_list_len(index);
        list_len = (list_len < TIC_PALETTE_SIZE) ? (list_len) : (TIC_PALETTE_SIZE);
        for (int i = 0; i < list_len; i++)
        {
            py_ItemRef pylist_item = py_list_getitem(index, i);
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
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    PY_CHECK_ARG_TYPE(0, tp_int); //check argv[0] type
    button_id = py_toint(py_arg(0));

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
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    bool pressed = core->api.btnp(tic, button_id, hold, period);
    py_newbool(py_retval(), pressed);
    return true;
}

static bool py_cls(int argc, py_Ref argv)
{
    int color;
    PY_CHECK_ARG_TYPE(0, tp_int);
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    color = py_toint(py_arg(0));
    core->api.cls(tic, color);
    return true;
}

static bool py_spr(int argc, py_Ref argv)
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
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;
    spr_id = py_toint(py_arg(0));
    x = py_toint(py_arg(1));
    y = py_toint(py_arg(2));
    scale = py_toint(py_arg(4));
    flip = py_toint(py_arg(5));
    rotate = py_toint(py_arg(6));
    w = py_toint(py_arg(7));
    h = py_toint(py_arg(8));
    color_count = prepare_colorindex(py_arg(3), colors);

    core->api.spr(tic, spr_id, x, y, w, h, colors, color_count, scale, flip, rotate);
    return true;
}

static bool py_print(int argc, py_Ref argv)
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
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    s32 ps = core->api.print(tic, str, x, y, color, fixed, scale, alt);
    py_newint(py_retval(), ps);
    return true;
}

static bool py_circ(int argc, py_Ref argv)
{
    int x, y, radius, color;
    for (int i = 0; i < 4; i++)
        PY_CHECK_ARG_TYPE(i, tp_int);
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;
    x = py_toint(py_arg(0));
    y = py_toint(py_arg(1));
    radius = py_toint(py_arg(2));
    color = py_toint(py_arg(3));

    core->api.circ(tic, x, y, radius, color);
    return true;
}

static bool py_circb(int argc, py_Ref argv)
{
    int x, y, radius, color;
    for (int i = 0; i < 4; i++)
        PY_CHECK_ARG_TYPE(i, tp_int);
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;
    x = py_toint(py_arg(0));
    y = py_toint(py_arg(1));
    radius = py_toint(py_arg(2));
    color = py_toint(py_arg(3));

    core->api.circb(tic, x, y, radius, color);
    return true;
}

static bool py_clip(int argc, py_Ref argv)
{
    int x, y, width, height;
    for (int i = 0; i < 4; i++)
        PY_CHECK_ARG_TYPE(i, tp_int);
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;
    x = py_toint(py_arg(0));
    y = py_toint(py_arg(1));
    width = py_toint(py_arg(2));
    height = py_toint(py_arg(3));

    core->api.clip(tic, x, y, width, height);
    return true;
}

static bool py_elli(int argc, py_Ref argv)
{
    int x, y, a, b, color;
    for (int i = 0; i < 5; i++)
        PY_CHECK_ARG_TYPE(i, tp_int);
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;
    x = py_toint(py_arg(0));
    y = py_toint(py_arg(1));
    a = py_toint(py_arg(2));
    b = py_toint(py_arg(3));
    color = py_toint(py_arg(4));

    core->api.elli(tic, x, y, a, b, color);
    return true;
}

static bool py_ellib(int argc, py_Ref argv)
{
    int x, y, a, b, color;
    for (int i = 0; i < 5; i++)
        PY_CHECK_ARG_TYPE(i, tp_int);
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;
    x = py_toint(py_arg(0));
    y = py_toint(py_arg(1));
    a = py_toint(py_arg(2));
    b = py_toint(py_arg(3));
    color = py_toint(py_arg(4));

    core->api.ellib(tic, x, y, a, b, color);
    return true;
}

static bool py_exit(int argc, py_Ref argv)
{
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.exit(tic);
    return true;
}

static bool py_fget(int argc, py_Ref argv)
{
    int spid, flag;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;
    spid = py_toint(py_arg(0));
    flag = py_toint(py_arg(1));

    bool res = core->api.fget(tic, spid, flag);
    py_newbool(py_retval(), res);

    return true;
}

static bool py_fset(int argc, py_Ref argv)
{
    int spid, flag;
    bool b;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_bool);
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;
    spid = py_toint(py_arg(0));
    flag = py_toint(py_arg(1));
    b = py_tobool(py_arg(2));

    core->api.fset(tic, spid, flag, b);
    return true;
}

static bool py_font(int argc, py_Ref argv)
{
    const char* str;
    int x, y, chromakey, width, height, scale;
    bool fixed, alt;
    PY_CHECK_ARG_TYPE(0, tp_str);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    PY_CHECK_ARG_TYPE(3, tp_int);
    PY_CHECK_ARG_TYPE(4, tp_int);
    PY_CHECK_ARG_TYPE(5, tp_int);
    PY_CHECK_ARG_TYPE(6, tp_bool);
    PY_CHECK_ARG_TYPE(7, tp_int);
    PY_CHECK_ARG_TYPE(8, tp_bool);

    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;
    str = py_tostr(py_arg(0));
    x = py_toint(py_arg(1));
    y = py_toint(py_arg(2));
    chromakey = py_toint(py_arg(3));
    width = py_toint(py_arg(4));
    height = py_toint(py_arg(5));
    fixed = py_tobool(py_arg(6));
    scale = py_toint(py_arg(7));
    alt = py_tobool(py_arg(8));

    if (scale == 0)
    {
        py_newint(py_retval(), 0);
        return true;
    }

    s32 res = core->api.font(tic, str, x, y, &((u8)chromakey),
                             1, width, height, fixed, scale, alt);
    py_newint(py_retval(), res);

    return true;
}

static bool py_key(int argc, py_Ref argv)
{
    int code;
    PY_CHECK_ARG_TYPE(0, tp_int);
    code = py_toint(py_arg(0));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    if (code >= tic_keys_count)
    {
        return ValueError("unknown keyboard input");
    }

    bool pressed = core->api.key(tic, code);
    py_newbool(py_retval(), pressed);

    return true;
}

static bool py_keyp(int argc, py_Ref argv)
{
    int code, hold, period;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    code = py_toint(py_arg(0));
    hold = py_toint(py_arg(1));
    period = py_toint(py_arg(2));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    if (code >= tic_keys_count)
    {
        return ValueError("unknown keyboard input");
    }

    bool pressed = core->api.keyp(tic, code, hold, period);
    py_newbool(py_retval(), pressed);

    return true;
}

static bool py_line(int argc, py_Ref argv)
{
    int x0, y0, x1, y1, color;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    PY_CHECK_ARG_TYPE(3, tp_int);
    PY_CHECK_ARG_TYPE(4, tp_int);
    x0 = py_toint(py_arg(0));
    y0 = py_toint(py_arg(1));
    x1 = py_toint(py_arg(2));
    y1 = py_toint(py_arg(3));
    color = py_toint(py_arg(4));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.line(tic, x0, y0, x1, y1, color);
    return true;
}

//NOTICE: untested
static void remap_callback(void* data, s32 x, s32 y, RemapResult* res)
{
    py_push(py_peek(-1));
    py_pushnil();
    py_Ref x0 = (py_Ref)malloc(sizeof(py_TValue));
    py_Ref y0 = (py_Ref)malloc(sizeof(py_TValue));
    py_newint(x0, x);
    py_newint(y0, y);
    py_push(x0);
    py_push(y0);
    py_vectorcall(2, 0);

    py_Ref list = py_retval();
    if (!py_checktype(list, tp_list)) return;
    int index, flip, rotate;
    py_ItemRef item = py_list_getitem(list, 0);
    index = py_toint(item);
    item = py_list_getitem(list, 1);
    flip = py_toint(item);
    item = py_list_getitem(list, 2);
    rotate = py_toint(item);

    res->index = (u8)index;
    res->flip = flip;
    res->rotate = rotate;

    free(x0);
    free(y0);
}

static bool py_map(int argc, py_Ref argv)
{
    int x, y, w, h, sx, sy, colorkey, scale;
    bool use_remap;
    char colors[16];

    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    PY_CHECK_ARG_TYPE(3, tp_int);
    PY_CHECK_ARG_TYPE(4, tp_int);
    PY_CHECK_ARG_TYPE(5, tp_int);
    PY_CHECK_ARG_TYPE(7, tp_int);
    x = py_toint(py_arg(0));
    y = py_toint(py_arg(1));
    w = py_toint(py_arg(2));
    h = py_toint(py_arg(3));
    sx = py_toint(py_arg(4));
    sy = py_toint(py_arg(5));
    colorkey = prepare_colorindex(py_arg(6), colors);
    scale = py_toint(py_arg(7));
    use_remap = !py_isnone(py_arg(8));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    if (use_remap)
        core->api.map(tic, x, y, w, h, sx, sy, colors, colorkey, scale, remap_callback, pk_current_vm);
    else
        core->api.map(tic, x, y, w, h, sx, sy, colors, colorkey, scale, NULL, NULL);

    return true;
}

static bool py_memcpy(int argc, py_Ref argv)
{
    int dest, src, size;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    dest = py_toint(py_arg(0));
    src = py_toint(py_arg(1));
    size = py_toint(py_arg(2));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.memcpy(tic, dest, src, size);
    return true;
}

static bool py_memset(int argc, py_Ref argv)
{
    int dest, val, size;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    dest = py_toint(py_arg(0));
    val = py_toint(py_arg(1));
    size = py_toint(py_arg(2));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.memset(tic, dest, val, size);
    return true;
}

static bool py_mget(int argc, py_Ref argv)
{
    int x, y;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    x = py_toint(py_arg(0));
    y = py_toint(py_arg(1));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    int res = core->api.mget(tic, x, y);
    py_newint(py_retval(), res);
    return true;
}

static bool py_mset(int argc, py_Ref argv)
{
    int x, y, title_id;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    x = py_toint(py_arg(0));
    y = py_toint(py_arg(1));
    title_id = py_toint(py_arg(2));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.mset(tic, x, y, title_id);
    return true;
}

static bool py_mouse(int argc, py_Ref argv)
{
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    tic_point pos = core->api.mouse(tic);
    const tic80_mouse* mouse = &core->memory.ram->input.mouse;

    py_Ref res = (py_Ref)malloc(sizeof(py_TValue));
    py_newtuple(res, 7);
    py_Ref tmp = (py_Ref)malloc(sizeof(py_TValue));
    py_newint(tmp, pos.x);
    py_tuple_setitem(res, 0, tmp);
    py_newint(tmp, pos.y);
    py_tuple_setitem(res, 1, tmp);
    py_newint(tmp, mouse->left);
    py_tuple_setitem(res, 2, tmp);
    py_newint(tmp, mouse->middle);
    py_tuple_setitem(res, 3, tmp);
    py_newint(tmp, mouse->right);
    py_tuple_setitem(res, 4, tmp);
    py_newint(tmp, mouse->scrollx);
    py_tuple_setitem(res, 5, tmp);
    py_newint(tmp, mouse->scrolly);
    py_tuple_setitem(res, 6, tmp);
    py_assign(py_retval(), res);
    free(res);
    free(tmp);
    return true;
}

static bool py_music(int argc, py_Ref argv)
{
    int track, frame, row, tempo, speed;
    bool loop, sustain;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    PY_CHECK_ARG_TYPE(3, tp_bool);
    PY_CHECK_ARG_TYPE(4, tp_bool);
    PY_CHECK_ARG_TYPE(5, tp_int);
    PY_CHECK_ARG_TYPE(6, tp_int);
    track = py_toint(py_arg(0));
    frame = py_toint(py_arg(1));
    row = py_toint(py_arg(2));
    loop = py_tobool(py_arg(3));
    sustain = py_tobool(py_arg(4));
    tempo = py_toint(py_arg(5));
    speed = py_toint(py_arg(6));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    if (track > MUSIC_TRACKS - 1)
    {
        return ValueError("invalid music track number");
    }
    core->api.music(tic, -1, 0, 0, false, false, -1, -1);
    core->api.music(tic, track, frame, row, loop, sustain, tempo, speed);
    return true;
}

static bool pyy_peek(int argc, py_Ref argv)
{
    int addr, bits;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    addr = py_toint(py_arg(0));
    bits = py_toint(py_arg(1));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    int res = core->api.peek(tic, addr, bits);
    py_newint(py_retval(), res);
    return true;
}

static bool py_peek1(int argc, py_Ref argv)
{
    int addr;
    PY_CHECK_ARG_TYPE(0, tp_int);
    addr = py_toint(py_arg(0));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    int res = core->api.peek1(tic, addr);
    py_newint(py_retval(), res);
    return true;
}

static bool py_peek2(int argc, py_Ref argv)
{
    int addr;
    PY_CHECK_ARG_TYPE(0, tp_int);
    addr = py_toint(py_arg(0));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    int res = core->api.peek2(tic, addr);
    py_newint(py_retval(), res);
    return true;
}

static bool py_peek4(int argc, py_Ref argv)
{
    int addr;
    PY_CHECK_ARG_TYPE(0, tp_int);
    addr = py_toint(py_arg(0));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    int res = core->api.peek4(tic, addr);
    py_newint(py_retval(), res);
    return true;
}

static bool py_pix(int argc, py_Ref argv)
{
    int x, y, color;
    color = -1;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    if (!py_isnone(py_arg(2))) color = py_toint(py_arg(2));
    x = py_toint(py_arg(0));
    y = py_toint(py_arg(1));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    if (color >= 0) //set pixel
    {
        core->api.pix(tic, x, y, color, false);
        return false;
    }
    else //get pixel to retval
    {
        int res = core->api.pix(tic, x, y, 0, true);
        py_newint(py_retval(), res);
        return true;
    }
}

static bool py_pmem(int argc, py_Ref argv)
{
    int index, val;
    bool has_val = false;
    PY_CHECK_ARG_TYPE(0, tp_int);
    if (!py_isnone(py_arg(1)))
    {
        has_val = true;
        PY_CHECK_ARG_TYPE(1, tp_int);
        val = py_toint(py_arg(1));
    }
    index = py_toint(py_arg(0));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    if (index >= TIC_PERSISTENT_SIZE)
    {
        return ValueError("invalid tic persistent index");
    }

    int res = core->api.pmem(tic, index, 0, false);
    py_newint(py_retval(), res);

    if (has_val)
    {
        core->api.pmem(tic, index, val, true);
    }
    return true;
}

static bool py_poke(int argc, py_Ref argv)
{
    int addr, val, bits;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    addr = py_toint(py_arg(0));
    val = py_toint(py_arg(1));
    bits = py_toint(py_arg(2));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.poke(tic, addr, val, bits);
    return true;
}

static bool py_poke1(int argc, py_Ref argv)
{
    int addr, val;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    addr = py_toint(py_arg(0));
    val = py_toint(py_arg(1));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.poke1(tic, addr, val);
    return true;
}

static bool py_poke2(int argc, py_Ref argv)
{
    int addr, val;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    addr = py_toint(py_arg(0));
    val = py_toint(py_arg(1));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.poke2(tic, addr, val);
    return true;
}

static bool py_poke4(int argc, py_Ref argv)
{
    int addr, val;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    addr = py_toint(py_arg(0));
    val = py_toint(py_arg(1));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.poke4(tic, addr, val);
    return true;
}

static bool py_rect(int argc, py_Ref argv)
{
    int x, y, w, h, color;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    PY_CHECK_ARG_TYPE(3, tp_int);
    PY_CHECK_ARG_TYPE(4, tp_int);
    x = py_toint(py_arg(0));
    y = py_toint(py_arg(1));
    w = py_toint(py_arg(2));
    h = py_toint(py_arg(3));
    color = py_toint(py_arg(4));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.rect(tic, x, y, w, h, color);
    return true;
}

static bool py_rectb(int argc, py_Ref argv)
{
    int x, y, w, h, color;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_int);
    PY_CHECK_ARG_TYPE(3, tp_int);
    PY_CHECK_ARG_TYPE(4, tp_int);
    x = py_toint(py_arg(0));
    y = py_toint(py_arg(1));
    w = py_toint(py_arg(2));
    h = py_toint(py_arg(3));
    color = py_toint(py_arg(4));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.rectb(tic, x, y, w, h, color);
    return true;
}

static bool py_reset(int argc, py_Ref argv)
{
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.reset(tic);
    return true;
}

static bool py_sfx(int argc, py_Ref argv)
{
    int id, _note, duration, channel, volume, speed;
    bool _parse_note = false;
    const char* str_note;
    PY_CHECK_ARG_TYPE(0, tp_int);
    id = py_toint(py_arg(0));
    if (py_isstr(py_arg(1)))
    {
        _parse_note = true;
        str_note = py_tostr(py_arg(1));
    }
    else
    {
        PY_CHECK_ARG_TYPE(1, tp_int);
        _note = py_toint(py_arg(1));
    }
    PY_CHECK_ARG_TYPE(2, tp_int);
    PY_CHECK_ARG_TYPE(3, tp_int);
    PY_CHECK_ARG_TYPE(4, tp_int);
    PY_CHECK_ARG_TYPE(5, tp_int);
    duration = py_toint(py_arg(2));
    channel = py_toint(py_arg(3));
    volume = py_toint(py_arg(4));
    speed = py_toint(py_arg(5));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    s32 note, octave;
    if (_parse_note)
    {
        if (!parse_note(str_note, &note, &octave))
        {
            return ValueError("invalid note, should be like C#4");
        }
    }
    else
    {
        note = _note % NOTES;
        octave = _note / NOTES;
    }

    if (channel < 0 || channel >= TIC_SOUND_CHANNELS)
    {
        return ValueError("invalid channel");
    }

    if (id >= SFX_COUNT)
    {
        return ValueError("invalid sfx index");
    }

    core->api.sfx(tic, id, note, octave, duration, channel, volume & 0xf, volume & 0xf, speed);
    return true;
}

static bool py_sync(int argc, py_Ref argv)
{
    int mask, bank;
    bool tocart;
    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);
    PY_CHECK_ARG_TYPE(2, tp_bool);
    mask = py_toint(py_arg(0));
    bank = py_toint(py_arg(1));
    tocart = py_tobool(py_arg(2));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    if (bank < 0 || bank >= TIC_BANKS)
    {
        return ValueError("invalid sync bank");
    }

    core->api.sync(tic, mask, bank, tocart);
    return true;
}

static bool py_ttri(int argc, py_Ref argv)
{
    double x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3;
    int texsrc, chromakey;
    double z1, z2, z3;
    char colors[16];
    for (int i = 0; i < 12; i++)
        PY_CHECK_ARG_TYPE(i, tp_float);
    PY_CHECK_ARG_TYPE(12, tp_int);
    PY_CHECK_ARG_TYPE(14, tp_float);
    PY_CHECK_ARG_TYPE(15, tp_float);
    PY_CHECK_ARG_TYPE(16, tp_float);
    chromakey = prepare_colorindex(py_arg(13), colors);
    x1 = py_tofloat(py_arg(0));
    y1 = py_tofloat(py_arg(1));
    x2 = py_tofloat(py_arg(2));
    y2 = py_tofloat(py_arg(3));
    x3 = py_tofloat(py_arg(4));
    y3 = py_tofloat(py_arg(5));
    u1 = py_tofloat(py_arg(6));
    v1 = py_tofloat(py_arg(7));
    u2 = py_tofloat(py_arg(8));
    v2 = py_tofloat(py_arg(9));
    u3 = py_tofloat(py_arg(10));
    v3 = py_tofloat(py_arg(11));
    texsrc = py_toint(py_arg(12));
    z1 = py_tofloat(py_arg(14));
    z2 = py_tofloat(py_arg(15));
    z3 = py_tofloat(py_arg(16));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.ttri(tic,
                   x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3,
                   texsrc,
                   colors, chromakey,
                   z1, z2, z3,
                   z1 != 0 || z2 != 0 || z3 != 0);
    return true;
}

static bool py_time(int argc, py_Ref argv)
{
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    double time = core->api.time(tic);
    py_newfloat(py_retval(), time);
    return true;
}

static bool py_trace(int argc, py_Ref argv)
{
    int color;
    const char* msg;
    PY_CHECK_ARG_TYPE(0, tp_str);
    PY_CHECK_ARG_TYPE(1, tp_int);
    msg = py_tostr(py_arg(0));
    color = py_toint(py_arg(1));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.trace(tic, msg, (u8)color);
    return true;
}

static bool py_tri(int argc, py_Ref argv)
{
    int color;
    double x1, y1, x2, y2, x3, y3;
    for (int i = 0; i < 6; i++)
        PY_CHECK_ARG_TYPE(i, tp_float);
    PY_CHECK_ARG_TYPE(6, tp_int);
    x1 = py_tofloat(py_arg(0));
    y1 = py_tofloat(py_arg(1));
    x2 = py_tofloat(py_arg(2));
    y2 = py_tofloat(py_arg(3));
    x3 = py_tofloat(py_arg(4));
    y3 = py_tofloat(py_arg(5));
    color = py_toint(py_arg(6));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.tri(tic, x1, y1, x2, y3, x3, y3, (u8)color);
    return true;
}

static bool py_trib(int argc, py_Ref argv)
{
    int color;
    double x1, y1, x2, y2, x3, y3;
    for (int i = 0; i < 6; i++)
        PY_CHECK_ARG_TYPE(i, tp_float);
    PY_CHECK_ARG_TYPE(6, tp_int);
    x1 = py_tofloat(py_arg(0));
    y1 = py_tofloat(py_arg(1));
    x2 = py_tofloat(py_arg(2));
    y2 = py_tofloat(py_arg(3));
    x3 = py_tofloat(py_arg(4));
    y3 = py_tofloat(py_arg(5));
    color = py_toint(py_arg(6));
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    core->api.trib(tic, x1, y1, x2, y3, x3, y3, (u8)color);
    return true;
}

static bool py_tstamp(int argc, py_Ref argv)
{
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    int res = core->api.tstamp(tic);
    py_newint(py_retval(), res);
    return true;
}

static bool py_vbank(int argc, py_Ref argv)
{
    int bank = -1;
    if (!py_isnone(py_arg(0)))
    {
        bank = py_toint(py_arg(0));
    }
    tic_core* core;
    if (!get_core(&core)) return false;
    tic_mem* tic = (tic_mem*)core;

    s32 prev = core->state.vbank.id;
    if (bank >= 0)
    {
        core->api.vbank(tic, bank);
    }
    py_newint(py_retval(), prev);
    return true;
}

/*************TIC-80 MISC BEGIN****************/

static bool py_throw_error(tic_core* core, const char* msg)
{
    core->data->error(core->data->data, py_formatexc());
    return true;
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
    py_bind(mod, "circ(x: int, y: int, radius: int, color: int)", py_circ);
    py_bind(mod, "circb(x: int, y: int, radius: int, color: int)", py_circb);
    py_bind(mod, "clip(x: int, y: int, width: int, height: int)", py_clip);
    py_bind(mod, "elli(x: int, y: int, a: int, b: int, color: int)", py_elli);
    py_bind(mod, "ellib(x: int, y: int, a: int, b: int, color: int)", py_ellib);
    py_bind(mod, "exit()", py_exit);
    py_bind(mod, "fget(sprite_id: int, flag: int) -> bool", py_fget);
    py_bind(mod, "fset(sprite_id: int, flag: int, b: bool)", py_fset);
    py_bind(mod, "font(text: str, x: int, y: int, chromakey: int, char_width=8, char_height=8, fixed=False, scale=1, alt=False) -> int", py_font);
    py_bind(mod, "key(code=-1) -> bool", py_key);
    py_bind(mod, "keyp(code=-1, hold=-1, period=-17) -> int", py_keyp);
    py_bind(mod, "line(x0: int, y0: int, x1: int, y1: int, color: int)", py_line);
    py_bind(mod, "map(x=0, y=0, w=30, h=17, sx=0, sy=0, colorkey=-1, scale=1, remap=None)", py_map);
    py_bind(mod, "memcpy(dest: int, source: int, size: int)", py_memcpy);
    py_bind(mod, "memset(dest: int, value: int, size: int)", py_memset);
    py_bind(mod, "mget(x: int, y: int) -> int", py_mget);
    py_bind(mod, "mset(x: int, y: int, tile_id: int)", py_mset);
    py_bind(mod, "mouse() -> tuple[int, int, bool, bool, bool, int, int]", py_mouse);
    py_bind(mod, "music(track=-1, frame=-1, row=-1, loop=True, sustain=False, tempo=-1, speed=-1)", py_music);
    py_bind(mod, "peek(addr: int, bits=8) -> int", pyy_peek);
    py_bind(mod, "peek1(addr: int) -> int", py_peek1);
    py_bind(mod, "peek2(addr: int) -> int", py_peek2);
    py_bind(mod, "peek4(addr: int) -> int", py_peek4);
    py_bind(mod, "pix(x: int, y: int, color: int=None) -> int | None", py_pix);
    py_bind(mod, "pmem(index: int, value: int=None) -> int", py_pmem);
    py_bind(mod, "poke(addr: int, value: int, bits=8)", py_poke);
    py_bind(mod, "poke1(addr: int, value: int)", py_poke1);
    py_bind(mod, "poke2(addr: int, value: int)", py_poke2);
    py_bind(mod, "poke4(addr: int, value: int)", py_poke4);
    py_bind(mod, "rect(x: int, y: int, w: int, h: int, color: int)", py_rect);
    py_bind(mod, "rectb(x: int, y: int, w: int, h: int, color: int)", py_rectb);
    py_bind(mod, "reset()", py_reset);
    py_bind(mod, "sfx(id: int, note=-1, duration=-1, channel=0, volume=15, speed=0)", py_sfx);
    py_bind(mod, "sync(mask=0, bank=0, tocart=False)", py_sync);
    py_bind(mod, "ttri(x1: float, y1: float, x2: float, y2: float, x3: float, y3: float, u1: float, v1: float, u2: float, v2: float, u3: float, v3: float, texsrc=0, chromakey=-1, z1=0.0, z2=0.0, z3=0.0)", py_ttri);
    py_bind(mod, "time() -> int", py_time);
    py_bind(mod, "trace(message, color=15)", py_trace);
    py_bind(mod, "tri(x1: float, y1: float, x2: float, y2: float, x3: float, y3: float, color: int)", py_tri);
    py_bind(mod, "trib(x1: float, y1: float, x2: float, y2: float, x3: float, y3: float, color: int)", py_trib);
    py_bind(mod, "tstamp() -> int", py_tstamp);
    py_bind(mod, "vbank(bank: int=None) -> int", py_vbank);
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
        py_throw_error(core, "Binding func failed!");
        return false;
    }
    if (!py_exec(code, "main.py", EXEC_MODE, NULL))
    {
        //throw error
        py_throw_error(core, "Executing code failed!");
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
        py_throw_error(core, "TIC running error!");
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
        py_throw_error(core, "BOOT running error!");
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
    py_Ref py_row = (py_Ref)malloc(sizeof(py_TValue));
    py_newint(py_row, row);
    py_push(py_row);
    if (!py_vectorcall(1, 0))
    {
        py_throw_error(core, "SCANLINE running error!");
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
    py_Ref py_row = (py_Ref)malloc(sizeof(py_TValue));
    py_newint(py_row, row);
    py_push(py_row);
    if (!py_vectorcall(1, 0))
    {
        py_throw_error(core, "BORDER running error!");
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
    py_Ref py_idx = (py_Ref)malloc(sizeof(py_TValue));
    py_newint(py_idx, index);
    py_push(py_idx);
    if (!py_vectorcall(1, 0))
    {
        py_throw_error(core, "MENU running error!");
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
