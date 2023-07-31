
#include "core/core.h"

#if defined(TIC_BUILD_WITH_PYTHON)

#include "pocketpy_c.h"
#include <stdio.h>
#include <string.h>

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
            buffer[i] = list_val;
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

    if (key_id >= tic_keys_count) {
        pkpy_error(vm, "tic80-panic!", "unknown keyboard code\n");
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
        pkpy_error(vm, "tic80-panic!", "unknown keyboard code\n");
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
        pkpy_error(vm, "tic80-panic!", "invalid music track index\n");

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
        pkpy_error(vm, "tic80-panic!", "invalid persistent tic index\n");
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
    char* text = NULL;
    int x;
    int y;
    int color;
    bool fixed;
    int scale;
    bool small;
    bool alt;

    pkpy_to_string(vm, 0, &text);
    pkpy_to_int(vm, 1, &x);
    pkpy_to_int(vm, 2, &y);
    pkpy_to_int(vm, 3, &color);
    pkpy_to_bool(vm, 4, &fixed);
    pkpy_to_int(vm, 5, &scale);
    pkpy_to_bool(vm, 6, &small);
    pkpy_to_bool(vm, 7, &alt);
    get_core(vm, (tic_core**) &tic);
    if(pkpy_check_error(vm)) {
        if (text != NULL) free(text);
        return 0;
    }

    s32 size = tic_api_print(tic, text, x, y, color, fixed, scale, alt);
    
    pkpy_push_int(vm, size);

    free(text);
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
        pkpy_to_string(vm, 1, &string_note);
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
    int note, octave;

    if (parse_note) {
        if(!tic_tool_parse_note(string_note, &note, &octave)) {
            pkpy_error(vm, "tic80-panic!", "invalid note, should like C#4\n");
            goto cleanup; //error in future;
        }
            
    } else {
        note = int_note % NOTES;
        octave = int_note/ NOTES;
    }

    if (channel < 0 || channel >= TIC_SOUND_CHANNELS) {
        pkpy_error(vm, "tic80-panic!", "unknown channel\n");
        goto cleanup;
    }

    if (sfx_id >= SFX_COUNT) {
        pkpy_error(vm, "tic80-panic!", "unknown sfx index\n");
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
        pkpy_error(vm, "tic80-panic!", "sync() error, invalid bank\n");
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

    pkpy_push_function(vm, py_trace, 2);
    pkpy_set_global(vm, "_trace");

    pkpy_push_function(vm, py_cls, 1);
    pkpy_set_global(vm, "_cls");

    pkpy_push_function(vm, py_btn, 1);
    pkpy_set_global(vm, "_btn");

    pkpy_push_function(vm, py_btnp, 3);
    pkpy_set_global(vm, "_btnp");

    pkpy_push_function(vm, py_circ, 4);
    pkpy_set_global(vm, "_circ");

    pkpy_push_function(vm, py_circb, 4);
    pkpy_set_global(vm, "_circb");

    pkpy_push_function(vm, py_clip, 4);
    pkpy_set_global(vm, "_clip");

    pkpy_push_function(vm, py_elli, 5);
    pkpy_set_global(vm, "_elli");

    pkpy_push_function(vm, py_ellib, 5);
    pkpy_set_global(vm, "_ellib");

    pkpy_push_function(vm, py_exit, 0);
    pkpy_set_global(vm, "_exit");

    pkpy_push_function(vm, py_fget, 2);
    pkpy_set_global(vm, "_fget");

    pkpy_push_function(vm, py_fset, 3);
    pkpy_set_global(vm, "_fset");

    pkpy_push_function(vm, py_font, 9);
    pkpy_set_global(vm, "_font");

    pkpy_push_function(vm, py_key, 1);
    pkpy_set_global(vm, "_key");

    pkpy_push_function(vm, py_keyp, 3);
    pkpy_set_global(vm, "_keyp");

    pkpy_push_function(vm, py_line, 5);
    pkpy_set_global(vm, "_line");

    pkpy_push_function(vm, py_map, 9);
    pkpy_set_global(vm, "_map");

    pkpy_push_function(vm, py_memcpy, 3);
    pkpy_set_global(vm, "_memcpy");

    pkpy_push_function(vm, py_memset, 3);
    pkpy_set_global(vm, "_memset");

    pkpy_push_function(vm, py_mget, 2);
    pkpy_set_global(vm, "_mget");

    pkpy_push_function(vm, py_mouse, 0);
    pkpy_set_global(vm, "_mouse");

    pkpy_push_function(vm, py_mset, 3);
    pkpy_set_global(vm, "_mset");

    pkpy_push_function(vm, py_music, 7);
    pkpy_set_global(vm, "_music");

    pkpy_push_function(vm, py_peek, 2);
    pkpy_set_global(vm, "_peek");

    pkpy_push_function(vm, py_peek1, 1);
    pkpy_set_global(vm, "_peek1");

    pkpy_push_function(vm, py_peek2, 1);
    pkpy_set_global(vm, "_peek2");

    pkpy_push_function(vm, py_peek4, 1);
    pkpy_set_global(vm, "_peek4");

    pkpy_push_function(vm, py_pix, 3);
    pkpy_set_global(vm, "_pix");

    pkpy_push_function(vm, py_pmem, 2);
    pkpy_set_global(vm, "_pmem");

    pkpy_push_function(vm, py_poke, 3);
    pkpy_set_global(vm, "_poke");

    pkpy_push_function(vm, py_poke1, 2);
    pkpy_set_global(vm, "_poke1");

    pkpy_push_function(vm, py_poke2, 2);
    pkpy_set_global(vm, "_poke2");

    pkpy_push_function(vm, py_poke4, 2);
    pkpy_set_global(vm, "_poke4");

    pkpy_push_function(vm, py_print, 8);
    pkpy_set_global(vm, "_print");

    pkpy_push_function(vm, py_rect, 5);
    pkpy_set_global(vm, "_rect");

    pkpy_push_function(vm, py_rectb, 5);
    pkpy_set_global(vm, "_rectb");

    pkpy_push_function(vm, py_reset, 0);
    pkpy_set_global(vm, "_reset");
    
    pkpy_push_function(vm, py_sfx, 6);
    pkpy_set_global(vm, "_sfx");

    pkpy_push_function(vm, py_spr, 9);
    pkpy_set_global(vm, "_spr");

    pkpy_push_function(vm, py_sync, 3);
    pkpy_set_global(vm, "_sync");

    pkpy_push_function(vm, py_time, 0);
    pkpy_set_global(vm, "_time");

    pkpy_push_function(vm, py_tri, 7);
    pkpy_set_global(vm, "_tri");

    pkpy_push_function(vm, py_trib, 7);
    pkpy_set_global(vm, "_trib");

    pkpy_push_function(vm, py_tstamp, 0);
    pkpy_set_global(vm, "_tstamp");

    pkpy_push_function(vm, py_ttri, 17);
    pkpy_set_global(vm, "_ttri");

    pkpy_push_function(vm, py_vbank, 1);
    pkpy_set_global(vm, "_vbank");

    if(pkpy_check_error(vm))
        return false;

    return true;
}

static bool setup_py_bindings(pkpy_vm* vm) {
    pkpy_vm_run(vm, "def trace(message, color=15) : return _trace(str(message), int(color))");

    pkpy_vm_run(vm, "def cls(color=0) : return _cls(int(color))");

    //lua api does this for btn
    pkpy_vm_run(vm, "def btn(id) : return _btn(id)");
    pkpy_vm_run(vm, "def btnp(id, hold=-1, period=-1) : return _btnp(int(id), int(hold), int(period))");

    //even if there are no keyword args, this also gives us argument count checks
    pkpy_vm_run(vm, "def circ(x, y, radius, color) : return _circ(int(x), int(y), int(radius), int(color))");
    pkpy_vm_run(vm, "def circb(x, y, radius, color) : return _circb(int(x), int(y), int(radius), int(color))\n");

    pkpy_vm_run(vm, "def elli(x, y, a, b, color) : return _elli(int(x), int(y), int(radius), int(color))");
    pkpy_vm_run(vm, "def ellib(x, y, a, b, color) : return _ellib(int(x), int(y), int(radius), int(color))");

    pkpy_vm_run(vm, "def clip(x, y, width, height) : return _clip(int(x), int(y), int(width), int(height))");
    pkpy_vm_run(vm, "def exit() : return _exit()\n");

    pkpy_vm_run(vm, "def fget(sprite_id, flag) : return _fget(int(sprite_id), int(flag))");
    pkpy_vm_run(vm, "def fset(sprite_id, flag, b) : return _fset(int(sprite_id), int(flag), bool(b))");

    pkpy_vm_run(vm, 
        "def font(text, x, y, chromakey, char_width=8, char_height=8, fixed=False, scale=1, alt=False) : " 
        "return _font(str(text), int(x), int(y), chromakey, int(char_width), int(char_height), bool(fixed), int(scale), bool(alt))"
    );

    pkpy_vm_run(vm, "def key(code=-1) : return _key(code)");
    pkpy_vm_run(vm, "def keyp(code=-1, hold=-1, period=-17) : return _keyp(int(code), int(hold), int(period))");

    pkpy_vm_run(vm, "def line(x0, y0, x1, y1, color) : _line(int(x0), int(y0), int(x1), int(y1), int(color))");
    pkpy_vm_run(vm, 
        "def map(x=0, y=0, w=30, h=17, sx=0, sy=0, colorkey=-1, scale=1, remap=None) : "
        " return _map(int(x),int(y),int(w),int(h),int(sx),int(sy),colorkey,int(scale),remap)"
    );

    pkpy_vm_run(vm, "def memcpy(dest, source, size) : return _memcpy(int(dest), int(source), int(size))");
    pkpy_vm_run(vm, "def memset(dest, value, size) : return _memset(int(dest), int(value), int(size))");

    pkpy_vm_run(vm, "def mget(x, y) : return _mget(int(x), int(y))");
    pkpy_vm_run(vm, "def mset(x, y, tile_id) : return _mset(int(x), int(y), int(tile_id))");

    pkpy_vm_run(vm, "def mouse() : return _mouse()");

    pkpy_vm_run(vm, 
        "def music(track=-1, frame=-1, row=-1, loop=True, sustain=False, tempo=-1, speed=-1) :"
        "return _music(int(track), int(frame), int(row), bool(loop), bool(sustain), int(tempo), int(speed))"
    );

    pkpy_vm_run(vm, "def peek(addr, bits=8) : return _peek(int(addr), int(bits)) ");
    pkpy_vm_run(vm, "def peek1(addr) : return _peek1(int(addr)) ");
    pkpy_vm_run(vm, "def peek2(addr) : return _peek2(int(addr)) ");
    pkpy_vm_run(vm, "def peek4(addr) : return _peek4(int(addr)) ");

    pkpy_vm_run(vm, "def pix(x, y, color=None) : return _pix(int(x), int(y), color)");

    pkpy_vm_run(vm, "def pmem(index, value=None) : return _pmem(int(index), value)");

    pkpy_vm_run(vm, "def poke(addr, value, bits=8) : return _poke(int(addr), int(value), int(bits)) ");
    pkpy_vm_run(vm, "def poke1(addr, value) : return _poke1(int(addr), int(value)) ");
    pkpy_vm_run(vm, "def poke2(addr, value) : return _poke2(int(addr), int(value)) ");
    pkpy_vm_run(vm, "def poke4(addr, value) : return _poke4(int(addr), int(value)) ");

    pkpy_vm_run(vm, 
        "def print(text, x=0, y=0, color=15, fixed=False, scale=1, smallfont=False, alt=False) :"
        " return _print(str(text), int(x), int(y), int(color), bool(fixed), int(scale), bool(smallfont), bool(alt))"
    );

    pkpy_vm_run(vm, "def rect(x, y, w, h, color) : return _rect(int(x),int(y),int(w),int(h),int(color))");
    pkpy_vm_run(vm, "def rectb(x, y, w, h, color) : return _rectb(int(x),int(y),int(w),int(h),int(color))");

    pkpy_vm_run(vm, "def reset() : return _reset()");
    
    pkpy_vm_run(vm, 
        "def sfx(id, note=-1, duration=-1, channel=0, volume=15, speed=0) : " 
        "return _sfx(id, int(note), int(duration), int(channel), int(volume), int(speed))"
    );

    pkpy_vm_run(vm, 
        "def spr(id, x, y, colorkey=-1, scale=1, flip=0, rotate=0, w=1, h=1) : "
        "return _spr(int(id), int(x), int(y), colorkey, int(scale), int(flip), int(rotate), int(w), int(h))"
    );

    pkpy_vm_run(vm, "def sync(mask=0, bank=0, tocart=False) : return _sync(int(mask), int(bank), bool(tocart))");

    pkpy_vm_run(vm, "def time() : return _time()");
    pkpy_vm_run(vm, "def tstamp() : return _tstamp()");

    pkpy_vm_run(vm, 
        "def tri(x1, y1, x2, y2, x3, y3, color) : "
        "return _tri(float(x1), float(y1), float(x2), float(y2), float(x3), float(y3), int(color))"
    );
    pkpy_vm_run(vm, 
        "def trib(x1, y1, x2, y2, x3, y3, color) : "
        "return _trib(float(x1), float(y1), float(x2), float(y2), float(x3), float(y3), int(color))"
    );

    pkpy_vm_run(vm,
        "def ttri(x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, texsrc=0, chromakey=-1, z1=0, z2=0, z3=0) : "
        "return _ttri(float(x1),float(y1),float(x2),float(y2),float(x3),float(y3),float(u1),float(v1),float(u2),float(v2),float(u3),float(v3),int(texsrc),chromakey,float(z1),float(z2),float(z3))"
    );

    pkpy_vm_run(vm, "def vbank(bank=None) : return _vbank(bank)");

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

    if (pkpy_check_global(core->currentVM, TIC_FN))
    {
        pkpy_get_global(core->currentVM, TIC_FN);
        if(!pkpy_call(core->currentVM, 0))
            report_error(core, "error while running TIC\n");
    }
}
void callPythonBoot(tic_mem* tic) {
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) 
        return;

    if (pkpy_check_global(core->currentVM, BOOT_FN))
    {
        pkpy_get_global(core->currentVM, BOOT_FN);
        if(!pkpy_call(core->currentVM, 0))
            report_error(core, "error while running BOOT\n");
    }
}

void callPythonScanline(tic_mem* tic, s32 row, void* data) {
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) 
        return;

    if (pkpy_check_global(core->currentVM, SCN_FN))
    {
        pkpy_get_global(core->currentVM, SCN_FN);
        pkpy_push_int(core->currentVM, row);
        if(!pkpy_call(core->currentVM, 1))
            report_error(core, "error while running SCN\n");
    }
}

void callPythonBorder(tic_mem* tic, s32 row, void* data) {
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) 
        return;

    if (pkpy_check_global(core->currentVM, BDR_FN))
    {
        pkpy_get_global(core->currentVM, BDR_FN);
        pkpy_push_int(core->currentVM, row);
        if(!pkpy_call(core->currentVM, 1))
            report_error(core, "error while running BDR\n");
    }
}

void callPythonMenu(tic_mem* tic, s32 index, void* data) {
    tic_core* core = (tic_core*)tic;
    if (!core->currentVM) 
        return;

    if (pkpy_check_global(core->currentVM, MENU_FN))
    {
        pkpy_get_global(core->currentVM, MENU_FN);
        pkpy_push_int(core->currentVM, index);
        if(!pkpy_call(core->currentVM, 1))
            report_error(core, "error while running MENU\n");
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
    else { *code++; goto comment; }

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
    printf("TODO: python eval not yet implemented\n.");
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
