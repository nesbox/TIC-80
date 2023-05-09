
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

static u8 colorindex_buffer[TIC_PALETTE_SIZE];

//index should be a postive index
static int prepare_colorindex(pkpy_vm* vm, int index) 
{
    if (pkpy_is_int(vm, index)) 
    {
        int value;
        pkpy_to_int(vm, index, &value);

        if (value == -1)
            return 0;
        else
        {
            colorindex_buffer[0] = value;
            return 1;
        }
    } 
    else 
    { //should be a list then
        pkpy_get_global(vm, "len");
        pkpy_push(vm, index); //get the list
        pkpy_call(vm, 1);

        int list_len;
        pkpy_to_int(vm, -1, &list_len);
        pkpy_pop(vm, 1);

        list_len = (list_len < TIC_PALETTE_SIZE)?(list_len):(TIC_PALETTE_SIZE);

        for(int i = 0; i < list_len; i++) 
        {
            int list_val;
            pkpy_push(vm, index);
            pkpy_push_int(vm, i);
            pkpy_call_method(vm, "__getitem__", 1);
            pkpy_to_int(vm, -1, &list_val);
            colorindex_buffer[i] = list_val;
            pkpy_pop(vm, 1);
        }

        return list_len;
    }
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

static int py_btnp(pkpy_vm* vm) 
{
    tic_mem* tic;
    int button_id;
    int hold;
    int period;

    pkpy_to_int(vm, 0, &button_id);
    pkpy_to_int(vm, 1, &hold);
    pkpy_to_int(vm, 2, &period);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    bool pressed = tic_api_btnp(tic, button_id, hold, period);
    pkpy_push_bool(vm, pressed);
    return 1;
}

static int py_circ(pkpy_vm* vm) 
{
    tic_mem* tic;
    int x;
    int y;
    int radius;
    int color;

    pkpy_to_int(vm, 0, &x);
    pkpy_to_int(vm, 1, &y);
    pkpy_to_int(vm, 2, &radius);
    pkpy_to_int(vm, 3, &color);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    tic_api_circ(tic, x, y, radius, color);
    return 0;
}

static int py_circb(pkpy_vm* vm) 
{
    tic_mem* tic;
    int x;
    int y;
    int radius;
    int color;

    pkpy_to_int(vm, 0, &x);
    pkpy_to_int(vm, 1, &y);
    pkpy_to_int(vm, 2, &radius);
    pkpy_to_int(vm, 3, &color);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    tic_api_circb(tic, x, y, radius, color);
    return 0;
}

static int py_elli(pkpy_vm* vm) 
{
    tic_mem* tic;
    int x;
    int y;
    int a;
    int b;
    int color;

    pkpy_to_int(vm, 0, &x);
    pkpy_to_int(vm, 1, &y);
    pkpy_to_int(vm, 2, &a);
    pkpy_to_int(vm, 3, &b);
    pkpy_to_int(vm, 4, &color);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    tic_api_elli(tic, x, y, a, b, color);
    return 0;
}

static int py_ellib(pkpy_vm* vm) 
{
    tic_mem* tic;
    int x;
    int y;
    int a;
    int b;
    int color;

    pkpy_to_int(vm, 0, &x);
    pkpy_to_int(vm, 1, &y);
    pkpy_to_int(vm, 2, &a);
    pkpy_to_int(vm, 3, &b);
    pkpy_to_int(vm, 4, &color);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    tic_api_ellib(tic, x, y, a, b, color);
    return 0;
}

static int py_clip(pkpy_vm* vm) 
{
    tic_mem* tic;
    int x;
    int y;
    int w;
    int h;

    pkpy_to_int(vm, 0, &x);
    pkpy_to_int(vm, 1, &y);
    pkpy_to_int(vm, 2, &w);
    pkpy_to_int(vm, 3, &h);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    tic_api_clip(tic, x, y, w, h);
    return 0;
}

static int py_exit(pkpy_vm* vm) 
{
    tic_mem* tic;

    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    tic_api_exit(tic);
    return 0;
}

static int py_fget(pkpy_vm* vm) 
{
    tic_mem* tic;
    int sprite_id;
    unsigned flag;

    pkpy_to_int(vm, 0, &sprite_id);
    pkpy_to_int(vm, 1, &flag);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    bool set = tic_api_fget(tic, sprite_id, flag);
    pkpy_push_bool(vm, set);
    return 1;
}

static int py_fset(pkpy_vm* vm) 
{
    tic_mem* tic;
    int sprite_id;
    unsigned flag;
    bool set_to;

    pkpy_to_int(vm, 0, &sprite_id);
    pkpy_to_int(vm, 1, &flag);
    pkpy_to_bool(vm, 2, &set_to);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    tic_api_fset(tic, sprite_id, flag, set_to);
    return 0;
}

static int py_font(pkpy_vm* vm) 
{
    tic_mem* tic;
    char* text = NULL;
    int x;
    int y;
    int width;
    int height;
    int chromakey_raw;
    bool fixed;
    int scale;
    bool alt;

    pkpy_to_string(vm, 0, &text);
    pkpy_to_int(vm, 1, &x);
    pkpy_to_int(vm, 2, &y);
    pkpy_to_int(vm, 3, &width);
    pkpy_to_int(vm, 4, &height);
    pkpy_to_int(vm, 5, &chromakey_raw);
    pkpy_to_bool(vm, 6, &fixed);
    pkpy_to_int(vm, 7, &scale);
    pkpy_to_bool(vm, 8, &alt);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) {
        if (text != NULL) free(text);
        return 0;
    }

    if (scale == 0) 
    {
        pkpy_push_int(vm, 0);
    } 
    else 
    {
        u8 chromakey = (u8) chromakey_raw;
        s32 size = tic_api_font(tic, text, x, y, &chromakey, 1, width, height, fixed, scale, alt);
        pkpy_push_int(vm, size);
    }

    free(text);
    return 1;
}

static int py_key(pkpy_vm* vm) 
{
    tic_mem* tic;
    int key_id;

    pkpy_to_int(vm, 0, &key_id);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    bool pressed = tic_api_key(tic, key_id);
    pkpy_push_bool(vm, pressed);
    return 1;
}

static int py_keyp(pkpy_vm* vm) 
{
    tic_mem* tic;
    int key_id;
    int hold;
    int period;

    pkpy_to_int(vm, 0, &key_id);
    pkpy_to_int(vm, 1, &hold);
    pkpy_to_int(vm, 2, &period);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    bool pressed = tic_api_keyp(tic, key_id, hold, period);
    pkpy_push_bool(vm, pressed);
    return 1;
}

static int py_line(pkpy_vm* vm) 
{
    tic_mem* tic;
    double x0;
    double y0;
    double x1;
    double y1;
    int color;

    pkpy_to_float(vm, 0, &x0);
    pkpy_to_float(vm, 1, &y0);
    pkpy_to_float(vm, 2, &x1);
    pkpy_to_float(vm, 3, &y1);
    pkpy_to_int(vm, 4, &color);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    tic_api_line(tic, x0, y0, x1, y1, color);
    return 0;
}

static void remap_callback(void* data, s32 x, s32 y, RemapResult* result) {
    pkpy_vm* vm = data;

    pkpy_push(vm, -1); //get copy of remap callable
    pkpy_push_int(vm, x);
    pkpy_push_int(vm, y);
    pkpy_call(vm, 2);


    int index, flip, rotate;
    pkpy_to_int(vm, -3, &index);
    pkpy_to_int(vm, -2, &flip);
    pkpy_to_int(vm, -1, &rotate);
    pkpy_pop(vm, 3); //reset stack for next remap_callback call

    result->index = (u8) index;
    result->flip = flip;
    result->rotate = rotate;
}

static int py_map(pkpy_vm* vm) 
{
    tic_mem* tic;
    int x;
    int y;
    int w;
    int h;
    int sx;
    int sy;
    int color_count;
    int scale;
    bool used_remap;


    pkpy_to_int(vm, 0, &x);
    pkpy_to_int(vm, 1, &y);
    pkpy_to_int(vm, 2, &w);
    pkpy_to_int(vm, 3, &h);
    pkpy_to_int(vm, 4, &sx);
    pkpy_to_int(vm, 5, &sy);
    color_count = prepare_colorindex(vm, 6);
    pkpy_to_int(vm, 7, &scale);
    used_remap = !pkpy_is_none(vm, 8);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    //last element on the stack should be the function, so no need to adjust anything
    if (used_remap) 
        tic_api_map(tic, x, y, w, h, sx, sy, colorindex_buffer, color_count, scale, remap_callback, vm);
    else 
        tic_api_map(tic, x, y, w, h, sx, sy, colorindex_buffer, color_count, scale, NULL, NULL);

    return 0;
}

static int py_memcpy(pkpy_vm* vm) {
    
    tic_mem* tic;
    int dest;
    int src;
    int size;

    pkpy_to_int(vm, 0, &dest);
    pkpy_to_int(vm, 1, &src);
    pkpy_to_int(vm, 2, &size);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    tic_api_memcpy(tic, dest, src, size);

    return 0;
}

static int py_memset(pkpy_vm* vm) {
    
    tic_mem* tic;
    int dest;
    int value;
    int size;

    pkpy_to_int(vm, 0, &dest);
    pkpy_to_int(vm, 1, &value);
    pkpy_to_int(vm, 2, &size);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    tic_api_memset(tic, dest, value, size);

    return 0;
}

static int py_mget(pkpy_vm* vm) {
    
    tic_mem* tic;
    int x;
    int y;

    pkpy_to_int(vm, 0, &x);
    pkpy_to_int(vm, 1, &y);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    int value = tic_api_mget(tic, x, y);
    pkpy_push_int(vm, value);

    return 1;
}

static int py_mset(pkpy_vm* vm) {
    
    tic_mem* tic;
    int x;
    int y;
    int tile_id;

    pkpy_to_int(vm, 0, &x);
    pkpy_to_int(vm, 1, &y);
    pkpy_to_int(vm, 2, &tile_id);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    tic_api_mset(tic, x, y, tile_id);

    return 0;
}


static int py_mouse(pkpy_vm* vm) {
    
    tic_core* core;
    get_core(vm, &core);
    if(pkpy_check_error(vm)) 
        return 0;

    tic_point pos = tic_api_mouse((tic_mem*)core);

    const tic80_mouse* mouse = &core->memory.ram->input.mouse;

    pkpy_push_int(vm, pos.x);
    pkpy_push_int(vm, pos.y);
    pkpy_push_bool(vm, mouse->left);
    pkpy_push_bool(vm, mouse->middle);
    pkpy_push_bool(vm, mouse->right);
    pkpy_push_int(vm, mouse->scrollx);
    pkpy_push_int(vm, mouse->scrolly);

    return 7;
}

static int py_music(pkpy_vm* vm) {
    
    tic_mem* tic;
    int track;
    int frame;
    int row;
    bool loop;
    bool sustain;
    int tempo;
    int speed;

    pkpy_to_int(vm, 0, &track);
    pkpy_to_int(vm, 1, &frame);
    pkpy_to_int(vm, 2, &row);
    pkpy_to_bool(vm, 3, &loop);
    pkpy_to_bool(vm, 4, &sustain);
    pkpy_to_int(vm, 5, &tempo);
    pkpy_to_int(vm, 6, &speed);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    //stop the music first I guess
    tic_api_music(tic, -1, 0, 0, false, false, -1, -1);
    if (track >= 0)
        tic_api_music(tic, track, frame, row, loop, sustain, tempo, speed);

    return 0;
}

static int py_peek(pkpy_vm* vm) {
    
    tic_mem* tic;
    int address;
    int bits;

    pkpy_to_int(vm, 0, &address);
    pkpy_to_int(vm, 1, &bits);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    int value = tic_api_peek(tic, address, bits);
    pkpy_push_int(vm, value);

    return 1;
}

static int py_peek1(pkpy_vm* vm) {
    
    tic_mem* tic;
    int address;

    pkpy_to_int(vm, 0, &address);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    int value = tic_api_peek1(tic, address);
    pkpy_push_int(vm, value);

    return 1;
}

static int py_peek2(pkpy_vm* vm) {
    
    tic_mem* tic;
    int address;

    pkpy_to_int(vm, 0, &address);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    int value = tic_api_peek2(tic, address);
    pkpy_push_int(vm, value);

    return 1;
}

static int py_peek4(pkpy_vm* vm) {
    
    tic_mem* tic;
    int address;

    pkpy_to_int(vm, 0, &address);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    int value = tic_api_peek4(tic, address);
    pkpy_push_int(vm, value);

    return 1;
}

static int py_pix(pkpy_vm* vm) {
    tic_mem* tic;
    int x;
    int y;
    int color = -1;


    pkpy_to_int(vm, 0, &x);
    pkpy_to_int(vm, 1, &y);
    if (pkpy_is_int(vm, 2))
        pkpy_to_int(vm, 2, &color);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    if(color >= 0) { //set the pixel
        tic_api_pix(tic, x, y, color, false);
        return 0;
    } else { //get the pixel
        int value = tic_api_pix(tic, x, y, 0, true);
        pkpy_push_int(vm, value);
        return 1;
    }
}

static int py_pmem(pkpy_vm* vm) {
    tic_mem* tic;
    int index;
    bool provided_value = false;
    int value;

    pkpy_to_int(vm, 0, &index);
    if (!pkpy_is_none(vm, 1)) {
        provided_value = true;
        pkpy_to_int(vm, 1, &value);
    }
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    int stored = tic_api_pmem(tic, index, 0, false);
    pkpy_push_int(vm, stored);

    if(provided_value)  //set the value
        tic_api_pmem(tic, index, value, true);

    return 1;
}



static bool setup_c_bindings(pkpy_vm* vm) {

    pkpy_push_function(vm, py_trace);
    pkpy_set_global(vm, "_trace");

    pkpy_push_function(vm, py_cls);
    pkpy_set_global(vm, "_cls");

    pkpy_push_function(vm, py_btn);
    pkpy_set_global(vm, "_btn");

    pkpy_push_function(vm, py_btnp);
    pkpy_set_global(vm, "_btnp");

    pkpy_push_function(vm, py_circ);
    pkpy_set_global(vm, "_circ");

    pkpy_push_function(vm, py_circb);
    pkpy_set_global(vm, "_circb");

    pkpy_push_function(vm, py_clip);
    pkpy_set_global(vm, "_clip");

    pkpy_push_function(vm, py_elli);
    pkpy_set_global(vm, "_elli");

    pkpy_push_function(vm, py_ellib);
    pkpy_set_global(vm, "_ellib");

    pkpy_push_function(vm, py_exit);
    pkpy_set_global(vm, "_exit");

    pkpy_push_function(vm, py_fget);
    pkpy_set_global(vm, "_fget");

    pkpy_push_function(vm, py_fget);
    pkpy_set_global(vm, "_fset");

    pkpy_push_function(vm, py_font);
    pkpy_set_global(vm, "_font");

    pkpy_push_function(vm, py_key);
    pkpy_set_global(vm, "_key");

    pkpy_push_function(vm, py_keyp);
    pkpy_set_global(vm, "_keyp");

    pkpy_push_function(vm, py_line);
    pkpy_set_global(vm, "_line");

    pkpy_push_function(vm, py_map);
    pkpy_set_global(vm, "_map");

    pkpy_push_function(vm, py_memcpy);
    pkpy_set_global(vm, "_memcpy");

    pkpy_push_function(vm, py_memset);
    pkpy_set_global(vm, "_memset");

    pkpy_push_function(vm, py_mget);
    pkpy_set_global(vm, "_mget");

    pkpy_push_function(vm, py_mouse);
    pkpy_set_global(vm, "_mouse");

    pkpy_push_function(vm, py_mset);
    pkpy_set_global(vm, "_mset");

    pkpy_push_function(vm, py_music);
    pkpy_set_global(vm, "_music");

    pkpy_push_function(vm, py_peek);
    pkpy_set_global(vm, "_peek");

    pkpy_push_function(vm, py_peek1);
    pkpy_set_global(vm, "_peek1");

    pkpy_push_function(vm, py_peek1);
    pkpy_set_global(vm, "_peek2");

    pkpy_push_function(vm, py_peek1);
    pkpy_set_global(vm, "_peek4");

    pkpy_push_function(vm, py_pix);
    pkpy_set_global(vm, "_pix");

    pkpy_push_function(vm, py_pmem);
    pkpy_set_global(vm, "_pmem");

    if(pkpy_check_error(vm))
        return false;

    return true;
}

static bool setup_py_bindings(pkpy_vm* vm) {
    pkpy_vm_run(vm, "def trace(message, color=15) : return _trace(message, color)");

    pkpy_vm_run(vm, "def cls(color=0) : return _cls(color)");

    //lua api does this for btn
    pkpy_vm_run(vm, "def btn(id) : return _btn(id)");
    pkpy_vm_run(vm, "def btnp(id, hold=-1, period=-1) : return _btnp(id, hold, period)");

    //even if there are no keyword args, this also gives us argument count checks
    pkpy_vm_run(vm, "def circ(x, y, radius, color) : return _circ(x, y, radius, color)");
    pkpy_vm_run(vm, "def circb(x, y, radius, color) : return _circb(x, y, radius, color)\n");

    pkpy_vm_run(vm, "def elli(x, y, a, b, color) : return _elli(x, y, radius, color)");
    pkpy_vm_run(vm, "def ellib(x, y, a, b, color) : return _ellib(x, y, radius, color)");

    pkpy_vm_run(vm, "def clip(x, y, width, height) : return _clip(x, y, width, height)");
    pkpy_vm_run(vm, "def exit() : return _exit()\n");

    pkpy_vm_run(vm, "def fget(sprite_id, flag) : return _fget(sprite_id, flag)");
    pkpy_vm_run(vm, "def fset(sprite_id, flag, bool) : return _fset(sprite_id, flag, bool)");

    pkpy_vm_run(vm, 
        "def font(text, x, y, chromakey, char_width, char_height, fixed=False, scale=1, alt=False) : " 
        "return _font(text, x, y, chromakey, char_width, char_height, fixed, scale, alt)"
    );

    pkpy_vm_run(vm, "def key(code=-1) : return _key(code)");
    pkpy_vm_run(vm, "def keyp(code=-1, hold=-1, period=-17) : return _keyp(code, hold, period)");

    pkpy_vm_run(vm, "def line(x0, y0, x1, y1, color) : _line(x0, y0, x1, y1, color)");
    pkpy_vm_run(vm, 
        "def map(x=0, y=0, w=30, h=17, sx=0, sy=0, colorkey=-1, scale=1, remap=None) : "
        " return _map(x,y,w,h,sx,sy,colorkey,scale,remap)"
    );

    pkpy_vm_run(vm, "def memcpy(dest, source, size) : return _memcpy(dest, source, size)");
    pkpy_vm_run(vm, "def memset(dest, value, size) : return _memset(dest, value, size)");

    pkpy_vm_run(vm, "def mget(x, y) : return _mget(x, y)");
    pkpy_vm_run(vm, "def mset(x, y, tile_id) : return _mset(x, y, tile_id)");

    pkpy_vm_run(vm, "def mouse() : return _mouse()");

    pkpy_vm_run(vm, 
        "def music(track=-1, frame=-1, row=-1, loop=True, sustain=False, tempo=-1, speed=-1) :"
        "return _music(track, frame, row, loop, sustain, tempo, speed)"
    );

    pkpy_vm_run(vm, "def peek(addr, bits=8) : return _peek(addr, bits) ");
    pkpy_vm_run(vm, "def peek1(addr) : return _peek1(addr) ");
    pkpy_vm_run(vm, "def peek2(addr) : return _peek2(addr) ");
    pkpy_vm_run(vm, "def peek4(addr) : return _peek4(addr) ");

    pkpy_vm_run(vm, "def pix(x, y, color=None) : return _pix(x, y, color)");

    pkpy_vm_run(vm, "def pmem(index, value=None) : return _pmem(index, value)");

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
