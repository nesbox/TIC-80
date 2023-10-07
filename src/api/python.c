#include "core/core.h"

#if defined(TIC_BUILD_WITH_PYTHON)

#include "pocketpy_c.h"
#include <stdio.h>
#include <string.h>

struct CachedNames{
    pkpy_CName _tic_core;
    pkpy_CName len;
    pkpy_CName __getitem__;
    pkpy_CName TIC;
    pkpy_CName BOOT;
    pkpy_CName SCN;
    pkpy_CName BDR;
    pkpy_CName MENU;
} N;

// duplicate a pkpy_CString to a null-terminated c string
static char* cstrdup(pkpy_CString cs){
    char* s = (char*)malloc(cs.size + 1);
    memcpy(s, cs.data, cs.size);
    s[cs.size] = '\0';
    return s;
}

static void pkpy_setglobal_2(pkpy_vm* vm, const char* name){
    pkpy_setglobal(vm, pkpy_name(name));
}

static bool get_core(pkpy_vm* vm, tic_core** core) 
{
    bool ok = pkpy_getglobal(vm, N._tic_core);
    if(!ok) return false;
    ok = pkpy_to_voidp(vm, -1, (void**) core);
    pkpy_pop_top(vm);
    return ok;
}

static bool setup_core(pkpy_vm* vm, tic_core* core) 
{
    if (!pkpy_push_voidp(vm, core)) return false;
    return pkpy_setglobal(vm, N._tic_core);
}

//index should be a positive index
static int prepare_colorindex(pkpy_vm* vm, int index, u8 * buffer) 
{
    if (pkpy_is_int(vm, index)) 
    {
        int value;
        pkpy_to_int(vm, index, &value);

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
        pkpy_getglobal(vm, N.len);
        pkpy_push_null(vm);
        pkpy_dup(vm, index); //get the list
        pkpy_vectorcall(vm, 1);

        int list_len;
        pkpy_to_int(vm, -1, &list_len);
        pkpy_pop_top(vm);

        list_len = (list_len < TIC_PALETTE_SIZE)?(list_len):(TIC_PALETTE_SIZE);

        for(int i = 0; i < list_len; i++) 
        {
            int list_val;
            pkpy_dup(vm, index); //get the list
            pkpy_get_unbound_method(vm, N.__getitem__);
            pkpy_push_int(vm, i);
            pkpy_vectorcall(vm, 1);
            pkpy_to_int(vm, -1, &list_val);
            buffer[i] = list_val;
            pkpy_pop_top(vm);
        }

        return list_len;
    }
}


static int py_trace(pkpy_vm* vm) 
{
    tic_mem* tic;
    pkpy_CString message;
    int color;

    pkpy_dup(vm, 0);
    pkpy_py_str(vm);
    pkpy_to_string(vm, -1, &message);
    pkpy_pop_top(vm);

    pkpy_to_int(vm, 1, &color);
    get_core(vm, (tic_core**) &tic);
    if (pkpy_check_error(vm)) 
    {
        return 0;
    }

    char* message_s = cstrdup(message);
    tic_api_trace(tic, message_s, (u8) color);
    free(message_s);
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
    int flag;

    pkpy_to_int(vm, 0, &sprite_id);
    pkpy_to_int(vm, 1, &flag);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    bool set = tic_api_fget(tic, sprite_id, (u8)flag);
    pkpy_push_bool(vm, set);
    return 1;
}

static int py_fset(pkpy_vm* vm) 
{
    tic_mem* tic;
    int sprite_id;
    int flag;
    bool set_to;

    pkpy_to_int(vm, 0, &sprite_id);
    pkpy_to_int(vm, 1, &flag);
    pkpy_to_bool(vm, 2, &set_to);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    tic_api_fset(tic, sprite_id, (u8)flag, set_to);
    return 0;
}

static int py_font(pkpy_vm* vm) 
{
    tic_mem* tic;
    pkpy_CString text;
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
        return 0;
    }

    if (scale == 0) 
    {
        pkpy_push_int(vm, 0);
    } 
    else 
    {
        u8 chromakey = (u8) chromakey_raw;
        char* text_s = cstrdup(text);
        s32 size = tic_api_font(tic, text_s, x, y, &chromakey, 1, width, height, fixed, scale, alt);
        free(text_s);
        pkpy_push_int(vm, size);
    }

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

    if (key_id >= tic_keys_count) {
        pkpy_error(vm, "tic80-panic!", pkpy_string("unknown keyboard code\n"));
        return 0;
    }

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

    if (key_id >= tic_keys_count) {
        pkpy_error(vm, "tic80-panic!", pkpy_string("unknown keyboard code\n"));
        return 0;
    }

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

    pkpy_dup(vm, -1); //get copy of remap callable
    pkpy_push_null(vm);
    pkpy_push_int(vm, x);
    pkpy_push_int(vm, y);
    pkpy_vectorcall(vm, 2);

    pkpy_unpack_sequence(vm, 3);
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

    static u8 colors[TIC_PALETTE_SIZE];

    pkpy_to_int(vm, 0, &x);
    pkpy_to_int(vm, 1, &y);
    pkpy_to_int(vm, 2, &w);
    pkpy_to_int(vm, 3, &h);
    pkpy_to_int(vm, 4, &sx);
    pkpy_to_int(vm, 5, &sy);
    color_count = prepare_colorindex(vm, 6, colors);
    pkpy_to_int(vm, 7, &scale);
    used_remap = !pkpy_is_none(vm, 8);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    //last element on the stack should be the function, so no need to adjust anything
    if (used_remap) 
        tic_api_map(tic, x, y, w, h, sx, sy, colors, color_count, scale, remap_callback, vm);
    else 
        tic_api_map(tic, x, y, w, h, sx, sy, colors, color_count, scale, NULL, NULL);

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

    if (track > MUSIC_TRACKS - 1 )
        pkpy_error(vm, "tic80-panic!", pkpy_string("invalid music track index\n"));

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

    if (index >= TIC_PERSISTENT_SIZE) {
        pkpy_error(vm, "tic80-panic!", pkpy_string("invalid persistent tic index\n"));
        return 0;
    }

    int stored = tic_api_pmem(tic, index, 0, false);
    pkpy_push_int(vm, stored);

    if(provided_value)  //set the value
        tic_api_pmem(tic, index, value, true);

    return 1;
}

static int py_poke(pkpy_vm* vm) {
    
    tic_mem* tic;
    int address;
    int value;
    int bits;

    pkpy_to_int(vm, 0, &address);
    pkpy_to_int(vm, 1, &value);
    pkpy_to_int(vm, 2, &bits);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    tic_api_poke(tic, address, value, bits);

    return 0;
}

static int py_poke1(pkpy_vm* vm) {
    
    tic_mem* tic;
    int address;
    int value;

    pkpy_to_int(vm, 0, &address);
    pkpy_to_int(vm, 1, &value);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    tic_api_poke1(tic, address, value);

    return 0;
}

static int py_poke2(pkpy_vm* vm) {
    
    tic_mem* tic;
    int address;
    int value;

    pkpy_to_int(vm, 0, &address);
    pkpy_to_int(vm, 1, &value);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    tic_api_poke2(tic, address, value);

    return 0;
}

static int py_poke4(pkpy_vm* vm) {
    
    tic_mem* tic;
    int address;
    int value;

    pkpy_to_int(vm, 0, &address);
    pkpy_to_int(vm, 1, &value);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    tic_api_poke4(tic, address, value);

    return 0;
}

static int py_print(pkpy_vm* vm) {
    
    tic_mem* tic;
    pkpy_CString text;
    int x;
    int y;
    int color;
    bool fixed;
    int scale;
    bool small_;
    bool alt;

    pkpy_dup(vm, 0);
    pkpy_py_str(vm);
    pkpy_to_string(vm, -1, &text);
    pkpy_pop_top(vm);

    pkpy_to_int(vm, 1, &x);
    pkpy_to_int(vm, 2, &y);
    pkpy_to_int(vm, 3, &color);
    pkpy_to_bool(vm, 4, &fixed);
    pkpy_to_int(vm, 5, &scale);
    pkpy_to_bool(vm, 6, &small_);
    pkpy_to_bool(vm, 7, &alt);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) {
        return 0;
    }

    char* text_s = cstrdup(text);
    s32 size = tic_api_print(tic, text_s, x, y, color, fixed, scale, alt);
    free(text_s);
    
    pkpy_push_int(vm, size);
    return 1;
}

static int py_rect(pkpy_vm* vm) 
{
    tic_mem* tic;
    int x;
    int y;
    int w;
    int h;
    int color;

    pkpy_to_int(vm, 0, &x);
    pkpy_to_int(vm, 1, &y);
    pkpy_to_int(vm, 2, &w);
    pkpy_to_int(vm, 3, &h);
    pkpy_to_int(vm, 4, &color);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    tic_api_rect(tic, x, y, w, h, color);
    return 0;
}

static int py_rectb(pkpy_vm* vm) 
{
    tic_mem* tic;
    int x;
    int y;
    int w;
    int h;
    int color;

    pkpy_to_int(vm, 0, &x);
    pkpy_to_int(vm, 1, &y);
    pkpy_to_int(vm, 2, &w);
    pkpy_to_int(vm, 3, &h);
    pkpy_to_int(vm, 4, &color);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    tic_api_rectb(tic, x, y, w, h, color);
    return 0;
}

static int py_sfx(pkpy_vm* vm) 
{
    tic_mem* tic;
    int sfx_id;

    bool parse_note = false;
    char* string_note = NULL;
    int int_note;

    int duration;
    int channel;
    int volume;
    int speed;

    pkpy_to_int(vm, 0, &sfx_id);

    if (pkpy_is_string(vm, 1)) {
        parse_note = true;
        pkpy_CString tmp;
        pkpy_to_string(vm, 1, &tmp);
        if(tmp.size) string_note = cstrdup(tmp);
    } else {
        pkpy_to_int(vm, 1, &int_note);
    }

    pkpy_to_int(vm, 2, &duration);
    pkpy_to_int(vm, 3, &channel);
    pkpy_to_int(vm, 4, &volume);
    pkpy_to_int(vm, 5, &speed);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        goto cleanup;
    s32 note, octave;

    if (parse_note) {
        if(!tic_tool_parse_note(string_note, &note, &octave)) {
            pkpy_error(vm, "tic80-panic!", pkpy_string("invalid note, should like C#4\n"));
            goto cleanup; //error in future;
        }
            
    } else {
        note = int_note % NOTES;
        octave = int_note/ NOTES;
    }

    if (channel < 0 || channel >= TIC_SOUND_CHANNELS) {
        pkpy_error(vm, "tic80-panic!", pkpy_string("unknown channel\n"));
        goto cleanup;
    }

    if (sfx_id >= SFX_COUNT) {
        pkpy_error(vm, "tic80-panic!", pkpy_string("unknown sfx index\n"));
        goto cleanup;
    }


    //for now we won't support two channel volumes
    tic_api_sfx(tic, sfx_id, note, octave, duration, channel, volume & 0xf, volume & 0xf, speed);

cleanup :
    if (string_note != NULL) free(string_note);
    return 0;
}

static int py_spr(pkpy_vm* vm) 
{
    tic_mem* tic;
    int spr_id;
    int x;
    int y;
    int color_count;
    int scale;
    int flip;
    int rotate;
    int w;
    int h;

    static u8 colors[TIC_PALETTE_SIZE];

    pkpy_to_int(vm, 0, &spr_id);
    pkpy_to_int(vm, 1, &x);
    pkpy_to_int(vm, 2, &y);
    color_count = prepare_colorindex(vm, 3, colors);
    pkpy_to_int(vm, 4, &scale);
    pkpy_to_int(vm, 5, &flip);
    pkpy_to_int(vm, 6, &rotate);
    pkpy_to_int(vm, 7, &w);
    pkpy_to_int(vm, 8, &h);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    tic_api_spr(tic, spr_id, x, y, w, h, colors, color_count, scale, flip, rotate);

    return 0;
}

static int py_reset(pkpy_vm* vm) {
    tic_core* core;
    get_core(vm, &core);
    if(pkpy_check_error(vm)) 
        return 0;

    core->state.initialized = false;

    return 0;
}

static int py_sync(pkpy_vm* vm) 
{
    tic_mem* tic;
    int mask;
    int bank;
    bool tocart;

    pkpy_to_int(vm, 0, &mask);
    pkpy_to_int(vm, 1, &bank);
    pkpy_to_bool(vm, 2, &tocart);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    if (bank < 0 || bank >= TIC_BANKS) {
        pkpy_error(vm, "tic80-panic!", pkpy_string("sync() error, invalid bank\n"));
        return 0;
    }


    tic_api_sync(tic, mask, bank, tocart);
    return 0;
}

static int py_time(pkpy_vm* vm) 
{
    tic_mem* tic;
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    int time = tic_api_time(tic);
    pkpy_push_int(vm, time);
    return 1;
}

static int py_tstamp(pkpy_vm* vm) 
{
    tic_mem* tic;
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    int tstamp = tic_api_tstamp(tic);
    pkpy_push_int(vm, tstamp);
    return 1;
}

static int py_tri(pkpy_vm* vm) 
{
    tic_mem* tic;
    double x1;
    double y1;
    double x2;
    double y2;
    double x3;
    double y3;
    int color;

    pkpy_to_float(vm, 0, &x1);
    pkpy_to_float(vm, 1, &y1);
    pkpy_to_float(vm, 2, &x2);
    pkpy_to_float(vm, 3, &y2);
    pkpy_to_float(vm, 4, &x3);
    pkpy_to_float(vm, 5, &y3);
    pkpy_to_int(vm, 6, &color);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    tic_api_tri(tic, x1, y1, x2, y2, x3, y3, color);
    return 0;
}

static int py_trib(pkpy_vm* vm) 
{
    tic_mem* tic;
    double x1;
    double y1;
    double x2;
    double y2;
    double x3;
    double y3;
    int color;

    pkpy_to_float(vm, 0, &x1);
    pkpy_to_float(vm, 1, &y1);
    pkpy_to_float(vm, 2, &x2);
    pkpy_to_float(vm, 3, &y2);
    pkpy_to_float(vm, 4, &x3);
    pkpy_to_float(vm, 5, &y3);
    pkpy_to_int(vm, 6, &color);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm))
        return 0;

    tic_api_trib(tic, x1, y1, x2, y2, x3, y3, color);
    return 0;
}

static int py_ttri(pkpy_vm* vm) 
{
    tic_mem* tic;
    double x1;
    double y1;
    double x2;
    double y2;
    double x3;
    double y3;
    double u1;
    double v1;
    double u2;
    double v2;
    double u3;
    double v3;
    int texsrc;
    int color_count;
    double z1;
    double z2;
    double z3;

    static u8 colors[TIC_PALETTE_SIZE];

    pkpy_to_float(vm, 0, &x1);
    pkpy_to_float(vm, 1, &y1);
    pkpy_to_float(vm, 2, &x2);
    pkpy_to_float(vm, 3, &y2);
    pkpy_to_float(vm, 4, &x3);
    pkpy_to_float(vm, 5, &y3);

    pkpy_to_float(vm, 6, &u1);
    pkpy_to_float(vm, 7, &v1);
    pkpy_to_float(vm, 8, &u2);
    pkpy_to_float(vm, 9, &v2);
    pkpy_to_float(vm, 10, &u3);
    pkpy_to_float(vm, 11, &v3);

    pkpy_to_int(vm, 12, &texsrc);
    color_count = prepare_colorindex(vm, 13, colors);

    pkpy_to_float(vm, 14, &z1);
    pkpy_to_float(vm, 15, &z2);
    pkpy_to_float(vm, 16, &z3);

    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) 
        return 0;

    tic_api_ttri(
        tic, 
        x1,y1,x2,y2,x3,y3, 
        u1,v1,u2,v2,u3,v3, 
        texsrc, 
        colors,color_count,  
        z1,z2,z3,
        z1 != 0 || z2 != 0 || z3 != 0
    );

    return 0;
}

static int py_vbank(pkpy_vm* vm) {
    tic_core* core;

    int bank_id = -1;

    if (!pkpy_is_none(vm, 0))
        pkpy_to_int(vm, 0, &bank_id);
    get_core(vm, &core);
    if(pkpy_check_error(vm)) 
        return 0;

    tic_mem* tic = (tic_mem*) core;

    s32 prev = core->state.vbank.id;

    if (bank_id >= 0)
        tic_api_vbank(tic, bank_id);

    pkpy_push_int(vm, prev);
    return 1;
}

static bool setup_c_bindings(pkpy_vm* vm) {
    pkpy_push_function(vm, "btn(id: int) -> bool", py_btn);
    pkpy_setglobal_2(vm, "btn");
    pkpy_push_function(vm, "btnp(id: int, hold=-1, period=-1) -> bool", py_btnp);
    pkpy_setglobal_2(vm, "btnp");

    pkpy_push_function(vm, "circ(x: int, y: int, radius: int, color: int)", py_circ);
    pkpy_setglobal_2(vm, "circ");
    pkpy_push_function(vm, "circb(x: int, y: int, radius: int, color: int)", py_circb);
    pkpy_setglobal_2(vm, "circb");

    pkpy_push_function(vm, "clip(x: int, y: int, width: int, height: int)", py_clip);
    pkpy_setglobal_2(vm, "clip");

    pkpy_push_function(vm, "cls(color=0)", py_cls);
    pkpy_setglobal_2(vm, "cls");

    pkpy_push_function(vm, "elli(x: int, y: int, a: int, b: int, color: int)", py_elli);
    pkpy_setglobal_2(vm, "elli");
    pkpy_push_function(vm, "ellib(x: int, y: int, a: int, b: int, color: int)", py_ellib);
    pkpy_setglobal_2(vm, "ellib");

    pkpy_push_function(vm, "exit()", py_exit);
    pkpy_setglobal_2(vm, "exit");

    pkpy_push_function(vm, "fget(sprite_id: int, flag: int) -> bool", py_fget);
    pkpy_setglobal_2(vm, "fget");
    pkpy_push_function(vm, "fset(sprite_id: int, flag: int, b: bool)", py_fset);
    pkpy_setglobal_2(vm, "fset");

    pkpy_push_function(vm, "font(text: str, x: int, y: int, chromakey: int, char_width=8, char_height=8, fixed=False, scale=1, alt=False) -> int", py_font);
    pkpy_setglobal_2(vm, "font");

    pkpy_push_function(vm, "key(code=-1) -> bool", py_key);
    pkpy_setglobal_2(vm, "key");
    pkpy_push_function(vm, "keyp(code=-1, hold=-1, period=-17) -> int", py_keyp);
    pkpy_setglobal_2(vm, "keyp");

    pkpy_push_function(vm, "line(x0: int, y0: int, x1: int, y1: int, color: int)", py_line);
    pkpy_setglobal_2(vm, "line");

    pkpy_push_function(vm, "map(x=0, y=0, w=30, h=17, sx=0, sy=0, colorkey=-1, scale=1, remap=None)", py_map);
    pkpy_setglobal_2(vm, "map");

    pkpy_push_function(vm, "memcpy(dest: int, source: int, size: int)", py_memcpy);
    pkpy_setglobal_2(vm, "memcpy");
    pkpy_push_function(vm, "memset(dest: int, value: int, size: int)", py_memset);
    pkpy_setglobal_2(vm, "memset");

    pkpy_push_function(vm, "mget(x: int, y: int) -> int", py_mget);
    pkpy_setglobal_2(vm, "mget");
    pkpy_push_function(vm, "mset(x: int, y: int, tile_id: int)", py_mset);
    pkpy_setglobal_2(vm, "mset");

    pkpy_push_function(vm, "mouse() -> tuple[int, int, bool, bool, bool, int, int]", py_mouse);
    pkpy_setglobal_2(vm, "mouse");

    pkpy_push_function(vm, "music(track=-1, frame=-1, row=-1, loop=True, sustain=False, tempo=-1, speed=-1)", py_music);
    pkpy_setglobal_2(vm, "music");

    pkpy_push_function(vm, "peek(addr: int, bits=8) -> int", py_peek);
    pkpy_setglobal_2(vm, "peek");
    pkpy_push_function(vm, "peek1(addr: int) -> int", py_peek1);
    pkpy_setglobal_2(vm, "peek1");
    pkpy_push_function(vm, "peek2(addr: int) -> int", py_peek2);
    pkpy_setglobal_2(vm, "peek2");
    pkpy_push_function(vm, "peek4(addr: int) -> int", py_peek4);
    pkpy_setglobal_2(vm, "peek4");

    pkpy_push_function(vm, "pix(x: int, y: int, color: int=None) -> int | None", py_pix);
    pkpy_setglobal_2(vm, "pix");

    pkpy_push_function(vm, "pmem(index: int, value: int=None) -> int", py_pmem);
    pkpy_setglobal_2(vm, "pmem");

    pkpy_push_function(vm, "poke(addr: int, value: int, bits=8)", py_poke);
    pkpy_setglobal_2(vm, "poke");
    pkpy_push_function(vm, "poke1(addr: int, value: int)", py_poke1);
    pkpy_setglobal_2(vm, "poke1");
    pkpy_push_function(vm, "poke2(addr: int, value: int)", py_poke2);
    pkpy_setglobal_2(vm, "poke2");
    pkpy_push_function(vm, "poke4(addr: int, value: int)", py_poke4);
    pkpy_setglobal_2(vm, "poke4");

    pkpy_push_function(vm, "print(text, x=0, y=0, color=15, fixed=False, scale=1, smallfont=False, alt=False)", py_print);
    pkpy_setglobal_2(vm, "print");

    pkpy_push_function(vm, "rect(x: int, y: int, w: int, h: int, color: int)", py_rect);
    pkpy_setglobal_2(vm, "rect");
    pkpy_push_function(vm, "rectb(x: int, y: int, w: int, h: int, color: int)", py_rectb);
    pkpy_setglobal_2(vm, "rectb");

    pkpy_push_function(vm, "reset()", py_reset);
    pkpy_setglobal_2(vm, "reset");

    pkpy_push_function(vm, "sfx(id: int, note=-1, duration=-1, channel=0, volume=15, speed=0)", py_sfx);
    pkpy_setglobal_2(vm, "sfx");

    pkpy_push_function(vm, "spr(id: int, x: int, y: int, colorkey=-1, scale=1, flip=0, rotate=0, w=1, h=1)", py_spr);
    pkpy_setglobal_2(vm, "spr");

    pkpy_push_function(vm, "sync(mask=0, bank=0, tocart=False)", py_sync);
    pkpy_setglobal_2(vm, "sync");

    pkpy_push_function(vm, "ttri(x1: float, y1: float, x2: float, y2: float, x3: float, y3: float, u1: float, v1: float, u2: float, v2: float, u3: float, v3: float, texsrc=0, chromakey=-1, z1=0.0, z2=0.0, z3=0.0)", py_ttri);
    pkpy_setglobal_2(vm, "ttri");

    pkpy_push_function(vm, "time() -> int", py_time);
    pkpy_setglobal_2(vm, "time");

    pkpy_push_function(vm, "trace(message, color=15)", py_trace);
    pkpy_setglobal_2(vm, "trace");

    pkpy_push_function(vm, "tri(x1: float, y1: float, x2: float, y2: float, x3: float, y3: float, color: int)", py_tri);
    pkpy_setglobal_2(vm, "tri");
    pkpy_push_function(vm, "trib(x1: float, y1: float, x2: float, y2: float, x3: float, y3: float, color: int)", py_trib);
    pkpy_setglobal_2(vm, "trib");

    pkpy_push_function(vm, "tstamp() -> int", py_tstamp);
    pkpy_setglobal_2(vm, "tstamp");

    pkpy_push_function(vm, "vbank(bank: int=None) -> int", py_vbank);
    pkpy_setglobal_2(vm, "vbank");

    if(pkpy_check_error(vm))
        return false;

    return true;
}

void closePython(tic_mem* tic) 
{
    tic_core* core = (tic_core*)tic;

    if (core->currentVM)
    {
        pkpy_delete_vm(core->currentVM);
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
    N._tic_core = pkpy_name("_tic_core");
    N.len = pkpy_name("len");
    N.__getitem__ = pkpy_name("__getitem__");
    N.TIC = pkpy_name("TIC");
    N.BOOT = pkpy_name("BOOT");
    N.SCN = pkpy_name("SCN");
    N.BDR = pkpy_name("BDR");
    N.MENU = pkpy_name("MENU");

    closePython(tic);
    tic_core* core = (tic_core*)tic;

    pkpy_vm* vm = pkpy_new_vm(false);

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

    if(!pkpy_exec(vm, code)) 
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

    if(!pkpy_getglobal(core->currentVM, N.TIC)) return;

    pkpy_push_null(core->currentVM);
    if(!pkpy_vectorcall(core->currentVM, 0)){
        report_error(core, "error while running TIC\n");
    }else{
        pkpy_pop_top(core->currentVM);
    }
}
void callPythonBoot(tic_mem* tic) {
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) 
        return;

    if(!pkpy_getglobal(core->currentVM, N.BOOT)) return;
    
    pkpy_push_null(core->currentVM);
    if(!pkpy_vectorcall(core->currentVM, 0)){
        report_error(core, "error while running BOOT\n");
    }else{
        pkpy_pop_top(core->currentVM);
    }
}

void callPythonScanline(tic_mem* tic, s32 row, void* data) {
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) 
        return;

    if(!pkpy_getglobal(core->currentVM, N.SCN)) return;

    pkpy_push_null(core->currentVM);
    pkpy_push_int(core->currentVM, row);
    if(!pkpy_vectorcall(core->currentVM, 1)){
        report_error(core, "error while running SCN\n");
    }else{
        pkpy_pop_top(core->currentVM);
    }
}

void callPythonBorder(tic_mem* tic, s32 row, void* data) {
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) 
        return;

    if(!pkpy_getglobal(core->currentVM, N.BDR)) return;

    pkpy_push_null(core->currentVM);
    pkpy_push_int(core->currentVM, row);
    if(!pkpy_vectorcall(core->currentVM, 1)){
        report_error(core, "error while running BDR\n");
    }else{
        pkpy_pop_top(core->currentVM);
    }
}

void callPythonMenu(tic_mem* tic, s32 index, void* data) {
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) 
        return;

    if(pkpy_getglobal(core->currentVM, N.MENU)) return;

    pkpy_push_null(core->currentVM);
    pkpy_push_int(core->currentVM, index);
    if(!pkpy_vectorcall(core->currentVM, 1)){
        report_error(core, "error while running MENU\n");
    }else{
        pkpy_pop_top(core->currentVM);
    }
}

static bool is_alnum_(char c) {
    return (
        (c >= 'a' && c <= 'z') 
        || (c >= 'A' && c <= 'Z') 
        || (c >= '0' && c <= '9') 
        || (c == '_')
    );
}

static const tic_outline_item* getPythonOutline(const char* code, s32* size) 
{

    *size = 0;
    static tic_outline_item* items = NULL;
    if (items) 
    {
        free(items);
        items = NULL;
    }

    const char* name_start;

start:
    if (*code == 0) goto end;
    else if (strncmp(code, "def", 3) == 0) { code += 3; goto next_word_ws; } 
    else if (*code == '#') { code++; goto comment; }
    else if (strncmp(code, "\"\"\"", 3) == 0) { code += 3; goto multiline_string; }
    else if (*code == '"') { code++; goto string; }
    else { code++; goto start; }

next_word_ws :
    if (*code == 0) goto end;
    else if (*code=='\t' || *code==' ' || *code=='\n') { code++; goto next_word_ws; }
    else if (is_alnum_(*code)) { name_start = code; code++; goto next_word; }
    else goto start;

next_word :
    if (is_alnum_(*code)) { code++; goto next_word; }
    else //store item
    { 
        items = realloc(items, (*size + 1) * sizeof(tic_outline_item));
        items[*size].pos = name_start;
        items[*size].size = (s32)(code - name_start);
        (*size)++;
        goto start;
    }

comment :
    if (*code == 0) goto end;
    else if (*code == '\n') { code++; goto start; }
    else { code++; goto comment; }

string :
    if (*code == 0) goto end;
    else if (*code == '"') { code++; goto start; }
    else if (*code == '\\') { code++; goto string_escape; }
    else { code++; goto string; }

string_escape :
    if (*code == 0) goto end;
    else { code++; goto string; }

multiline_string :
    if (*code == 0) goto end;
    else if (*code == '\\') { code++; goto multiline_string_escape; }
    else if (strncmp(code, "\"\"\"", 3) == 0) { code += 3; goto start; }
    else {code++; goto string; }

multiline_string_escape :
    if (*code == 0) goto end;
    else { code++; goto multiline_string; }

end:
    return items;

}

void evalPython(tic_mem* tic, const char* code) 
{
    tic_core* core = (tic_core*)tic;
    pkpy_vm* vm = core->currentVM;
    if (!vm) return;

    if(!pkpy_exec(vm, code)) 
    {
        report_error(core, "error while evaluating the code\n");
    }
}


static const char* const PythonKeywords[] =
{
    "is not", "not in", "yield from",
    "class", "import", "as", "def", "lambda", "pass", "del", "from", "with", "yield",
    "None", "in", "is", "and", "or", "not", "True", "False", "global", "try", "except", "finally",
    "while", "for", "if", "elif", "else", "break", "continue", "return", "assert", "raise"
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
