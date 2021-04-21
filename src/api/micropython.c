// micropython.c

#include "core/core.h"

#if defined(TIC_BUILD_WITH_PYTHON)

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "py/compile.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/stackctrl.h"

struct PythonVM {
    tic_mem* mem;
    mp_obj_t module_fun;
    mp_obj_t TIC_fun;
    mp_obj_t SCN_fun;
    mp_obj_t OVR_fun;
};

typedef struct PythonVM PythonVM;

static PythonVM python_vm;
static char heap[16384];
mp_state_ctx_t mp_state_ctx = {};

uint mp_import_stat(const char *path) {
    fprintf(stderr, "mp_import_stat(%s)\n", path);
    // struct stat st;
    // if (stat(path, &st) == 0) {
    //     if (S_ISDIR(st.st_mode)) {
    //         return MP_IMPORT_STAT_DIR;
    //     } else if (S_ISREG(st.st_mode)) {
    //         return MP_IMPORT_STAT_FILE;
    //     }
    // }
    return MP_IMPORT_STAT_NO_EXIST;
}

void nlr_jump_fail(void *val) {
    fprintf(stderr, "FATAL: uncaught NLR %p\n", val);
    exit(1);
}

////////// Python TIC-80 API /////////

// cls [color=0]
STATIC mp_obj_t python_cls(size_t n_args, const mp_obj_t *args) {
    tic_api_cls(python_vm.mem, n_args == 1 ? mp_obj_get_int(args[0]) : 0);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(cls_obj, 0, 1, python_cls);

// spr id x y [colorkey=-1] [scale=1] [flip=0] [rotate=0] [w=1 h=1]
STATIC mp_obj_t python_spr(size_t n_args, const mp_obj_t *args) {
    s32 index = 0;
    s32 x = 0;
    s32 y = 0;
    s32 w = 1;
    s32 h = 1;
    s32 scale = 1;
    tic_flip flip = tic_no_flip;
    tic_rotate rotate = tic_no_rotate;
    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;

    if(n_args >= 1)
    {
        index = mp_obj_get_int(args[0]);

        if(n_args >= 3)
        {
            x = mp_obj_get_int(args[1]);
            y = mp_obj_get_int(args[2]);

            if(n_args >= 4)
            {
                // spr's colorkey argument can be a list or a number
                mp_obj_t* colorkey = args[3];
                if(mp_obj_is_type(colorkey, &mp_type_tuple) || mp_obj_is_type(colorkey, &mp_type_list))
                {
                    size_t num_entries=0;
                    mp_obj_t* entries=0;
                    mp_obj_get_array(colorkey, &num_entries, &entries);
                    for(s32 i = 1; i <= TIC_PALETTE_SIZE && i <= num_entries; i++)
                    {
                        mp_int_t entry = mp_obj_get_int(entries[i-1]);
                        colors[i-1] = entry;
                        count++;
                    }
                }
                else
                {
                    colors[0] = mp_obj_get_int(colorkey);
                    count = 1;
                }

                if(n_args >= 5)
                {
                    scale = mp_obj_get_int(args[4]);

                    if(n_args >= 6)
                    {
                        flip = mp_obj_get_int(args[5]);

                        if(n_args >= 7)
                        {
                            rotate = mp_obj_get_int(args[6]);

                            if(n_args >= 9)
                            {
                                w = mp_obj_get_int(args[7]);
                                h = mp_obj_get_int(args[8]);
                            }
                        }
                    }
                }
            }
        }
    }

    tic_api_spr(python_vm.mem, index, x, y, w, h, colors, count, scale, flip, rotate);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(spr_obj, 1, 9, python_spr);

// print text [x=0 y=0] [color=12] [fixed=false] [scale=1] [smallfont=false] -> text width
STATIC mp_obj_t python_print(size_t n_args, const mp_obj_t *args) {
    if(n_args >= 1) 
    {
        s32 x = 0;
        s32 y = 0;
        s32 color = TIC_DEFAULT_COLOR;
        bool fixed = false;
        s32 scale = 1;
        bool alt = false;

        const char* text = mp_obj_str_get_str(args[0]);

        if(n_args >= 3)
        {
            x = mp_obj_get_int(args[1]);
            y = mp_obj_get_int(args[2]);

            if(n_args >= 4)
            {
                color = mp_obj_get_int(args[3]) % TIC_PALETTE_SIZE;

                if(n_args >= 5)
                {
                    fixed = mp_obj_get_int(args[4]) != 0; // bool

                    if(n_args >= 6)
                    {
                        scale = mp_obj_get_int(args[5]);

                        if(n_args >= 7)
                        {
                            alt = mp_obj_get_int(args[6]) != 0; // bool
                        }
                    }
                }
            }
        }

        if(scale == 0)
        {
            return MP_OBJ_NEW_SMALL_INT(0);
        }

        s32 size = tic_api_print(python_vm.mem, text ? text : "nil", x, y, color, fixed, scale, alt);

        return MP_OBJ_NEW_SMALL_INT(size);
    }

    return MP_OBJ_NEW_SMALL_INT(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(print_obj, 1, 7, python_print);

// pix x y color
// pix x y -> color
STATIC mp_obj_t python_pix(size_t n_args, const mp_obj_t *args) {
    if(n_args >= 2)
    {
        s32 x = mp_obj_get_int(args[0]);
        s32 y = mp_obj_get_int(args[1]);

        if(n_args >= 3)
        {
            s32 color = mp_obj_get_int(args[2]);
            tic_api_pix(python_vm.mem, x, y, color, false);
        }
        else
        {
            return MP_OBJ_NEW_SMALL_INT(tic_api_pix(python_vm.mem, x, y, 0, true));
        }

    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pix_obj, 2, 3, python_pix);

// line x0 y0 x1 y1 color
STATIC mp_obj_t python_line(size_t n_args, const mp_obj_t *args) {
    s32 x0 = mp_obj_get_int(args[0]);
    s32 y0 = mp_obj_get_int(args[1]);
    s32 x1 = mp_obj_get_int(args[2]);
    s32 y1 = mp_obj_get_int(args[3]);
    s32 color = mp_obj_get_int(args[4]);

    tic_api_line(python_vm.mem, x0, y0, x1, y1, color);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(line_obj, 5, 5, python_line);

// rect x y w h color
STATIC mp_obj_t python_rect(size_t n_args, const mp_obj_t *args) {
    s32 x = mp_obj_get_int(args[0]);
    s32 y = mp_obj_get_int(args[1]);
    s32 w = mp_obj_get_int(args[2]);
    s32 h = mp_obj_get_int(args[3]);
    s32 color = mp_obj_get_int(args[4]);

    tic_api_rect(python_vm.mem, x, y, w, h, color);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rect_obj, 5, 5, python_rect);

// rectb x y w h color
STATIC mp_obj_t python_rectb(size_t n_args, const mp_obj_t *args) {
    s32 x = mp_obj_get_int(args[0]);
    s32 y = mp_obj_get_int(args[1]);
    s32 w = mp_obj_get_int(args[2]);
    s32 h = mp_obj_get_int(args[3]);
    s32 color = mp_obj_get_int(args[4]);

    tic_api_rectb(python_vm.mem, x, y, w, h, color);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rectb_obj, 5, 5, python_rectb);

// btn [id] -> pressed
STATIC mp_obj_t python_btn(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        return mp_obj_new_int(python_vm.mem->ram.input.gamepads.data);
    }else{
        mp_int_t index = mp_obj_get_int(args[0]) & 0x1f;
        u32 r = python_vm.mem->ram.input.gamepads.data & (1 << index);
        return mp_obj_new_bool(r);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(btn_obj, 0, 1, python_btn);

// btnp [[id, [hold], [period] ] -> pressed
STATIC mp_obj_t python_btnp(size_t n_args, const mp_obj_t *args) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(btnp_obj, 0, 3, python_btnp);

// sfx id [note] [duration=-1] [channel=0] [volume=15] [speed=0]
STATIC mp_obj_t python_sfx(size_t n_args, const mp_obj_t *args) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sfx_obj, 1, 6, python_sfx);

// map [x=0 y=0] [w=30 h=17] [sx=0 sy=0] [colorkey=-1] [scale=1] [remap=nil]
STATIC mp_obj_t python_map(size_t n_args, const mp_obj_t *args) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(map_obj, 0, 9, python_map);

// mget x y -> tile_id
STATIC mp_obj_t python_mget(mp_obj_t x_in, mp_obj_t y_in) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mget_obj, python_mget);

// mset x y tile_id
STATIC mp_obj_t python_mset(mp_obj_t x_in, mp_obj_t y_in, mp_obj_t tile_id_in) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mset_obj, python_mset);

// peek addr
STATIC mp_obj_t python_peek(mp_obj_t addr_in) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(peek_obj, python_peek);

// poke addr val
STATIC mp_obj_t python_poke(mp_obj_t addr_in, mp_obj_t val_in) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(poke_obj, python_poke);

// peek4 addr4
STATIC mp_obj_t python_peek4(mp_obj_t addr4_in) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(peek4_obj, python_peek4);

// poke4 addr4 val
STATIC mp_obj_t python_poke4(mp_obj_t addr4_in, mp_obj_t val_in) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(poke4_obj, python_poke4);

// memcpy to from length
STATIC mp_obj_t python_memcpy(mp_obj_t to_in, mp_obj_t from_in, mp_obj_t length_in) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(memcpy_obj, python_memcpy);

// memset addr value length
STATIC mp_obj_t python_memset(mp_obj_t addr_in, mp_obj_t value_in, mp_obj_t length_in) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(memset_obj, python_memset);

// trace message [color]
STATIC mp_obj_t python_trace(size_t n_args, const mp_obj_t *args) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(trace_obj, 1, 2, python_trace);

// pmem index [val] -> val
STATIC mp_obj_t python_pmem(size_t n_args, const mp_obj_t *args) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pmem_obj, 1, 2, python_pmem);

// time
STATIC mp_obj_t python_time() {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(time_obj, python_time);

// tstamp
STATIC mp_obj_t python_tstamp() {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(tstamp_obj, python_tstamp);

// exit
STATIC mp_obj_t python_exit() {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(exit_obj, python_exit);

// font text, x, y, [transcolor], [char width], [char height], [fixed=false], [scale=1] -> text width
STATIC mp_obj_t python_font(size_t n_args, const mp_obj_t *args) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(font_obj, 3, 8, python_font);

// mouse
STATIC mp_obj_t python_mouse() {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mouse_obj, python_mouse);

// circ x y radius color
STATIC mp_obj_t python_circ(size_t n_args, const mp_obj_t *args) {
    mp_obj_t x_in = args[0];
    mp_obj_t y_in = args[1];
    mp_obj_t radius_in = args[2];
    mp_obj_t color_in = args[3];
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(circ_obj, 4, 4, python_circ);

// circb x y radius color
STATIC mp_obj_t python_circb(size_t n_args, const mp_obj_t *args) {
    mp_obj_t x_in = args[0];
    mp_obj_t y_in = args[1];
    mp_obj_t radius_in = args[2];
    mp_obj_t color_in = args[3];
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(circb_obj, 4, 4, python_circb);

// tri x1 y1 x2 y2 x3 y3 color
STATIC mp_obj_t python_tri(size_t n_args, const mp_obj_t *args) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tri_obj, 7, 7, python_tri);

// textri x1 y1 x2 y2 x3 y3 u1 v1 u2 v2 u3 v3 [use_map=false] [trans=-1]
STATIC mp_obj_t python_textri(size_t n_args, const mp_obj_t *args) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(textri_obj, 12, 14, python_textri);

// clip [x y width height]
STATIC mp_obj_t python_clip(size_t n_args, const mp_obj_t *args) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(clip_obj, 0, 4, python_clip);

// music [track=-1] [frame=-1] [row=-1] [loop=true] [sustain=false]
STATIC mp_obj_t python_music(size_t n_args, const mp_obj_t *args) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(music_obj, 0, 5, python_music);

// sync [mask=0] [bank=0] [tocart=false]
STATIC mp_obj_t python_sync(size_t n_args, const mp_obj_t *args) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sync_obj, 0, 3, python_sync);

// reset
STATIC mp_obj_t python_reset() {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(reset_obj, python_reset);

// key [code] -> pressed
STATIC mp_obj_t python_key(size_t n_args, const mp_obj_t *args) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(key_obj, 0, 1, python_key);

// keyp [code [hold period] ] -> pressed
STATIC mp_obj_t python_keyp(size_t n_args, const mp_obj_t *args) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(keyp_obj, 0, 2, python_keyp);

// fget sprite_id flag -> bool
STATIC mp_obj_t python_fget(mp_obj_t sprite_id_in, mp_obj_t flag_in) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(fget_obj, python_fget);

// fset sprite_id flag bool
STATIC mp_obj_t python_fset(mp_obj_t sprite_id_in, mp_obj_t flag_in, mp_obj_t bool_in) {
    fprintf(stderr, "warning: not implemented\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(fset_obj, python_fset);

static bool initPython(tic_mem* tic, const char* code)
{
    mp_stack_set_limit(40000 * (BYTES_PER_WORD / 4));
    gc_init(heap, heap + sizeof(heap));
    mp_init();

    tic_core* core = (tic_core*)tic;
	core->python = &python_vm;

	memset(&python_vm, 0, sizeof(PythonVM));
    python_vm.mem = tic;

    // Note: nlr_push/pop mechanism is like try/catch for exceptions
    // but uses setjmp (or similar). The body of this if statement will
    // run as normal, but if a micropython exception happens, then
    // execution will jump back to this if statement and the else clause
    // will run - printing the exception.
	nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr src_name = 1/*MP_QSTR_*/;
        mp_lexer_t *lex = mp_lexer_new_from_str_len(src_name, code, strlen(code), false);
        mp_parse_tree_t pt = mp_parse(lex, MP_PARSE_FILE_INPUT);
        mp_obj_t module_fun = mp_compile(&pt, src_name, false);
        mp_call_function_0(module_fun);
        python_vm.module_fun = module_fun;

        // try/catch in case mp_load_global fails (e.g. TIC not found).
        if (nlr_push(&nlr) == 0) {
            python_vm.TIC_fun = mp_load_global(qstr_from_str("TIC"));
            nlr_pop();
        }else{
            // TIC is required, so complain about it.
            fprintf(stderr, "No TIC function found?\n");
            mp_obj_print_exception(&mp_plat_print, ((mp_obj_t)nlr.ret_val));
        }

        if (nlr_push(&nlr) == 0) {
            python_vm.SCN_fun = mp_load_global(qstr_from_str("SCN"));
            nlr_pop();
        }else{
            // SCN is optional, so don't complain about it.
            // fprintf(stderr, "No SCN function found.\n");
            // mp_obj_print_exception(&mp_plat_print, ((mp_obj_t)nlr.ret_val));
        }

        if (nlr_push(&nlr) == 0) {
            python_vm.OVR_fun = mp_load_global(qstr_from_str("OVR"));
            nlr_pop();
        }else{
            // OVR is optional, so don't complain about it.
            // fprintf(stderr, "No OVR function found.\n");
            // mp_obj_print_exception(&mp_plat_print, ((mp_obj_t)nlr.ret_val));
        }

        // Instead of manually listing each of these...
        // mp_store_global(qstr_from_str("btn"), MP_OBJ_FROM_PTR(&btn_obj));
        // mp_store_global(qstr_from_str("cls"), MP_OBJ_FROM_PTR(&cls_obj));
        // mp_store_global(qstr_from_str("spr"), MP_OBJ_FROM_PTR(&spr_obj));
        // We use the TIC_API_LIST macro to generate them all.
        // This also helps to remind us to implement a new wrapper if/when
        // some new API is added.
#define API_FUNC_DEF(name, ...) {mp_store_global(qstr_from_str(#name), MP_OBJ_FROM_PTR(&name ## _obj));}
    TIC_API_LIST(API_FUNC_DEF)
#undef API_FUNC_DEF

        // qstr xstr = qstr_from_str("x");
        // fprintf(stderr, "xstr = %d\n", (int)xstr);
        // mp_obj_t xobj = mp_load_global(xstr);
        // fprintf(stderr, "xobj = %p\n", xobj);
        // mp_int_t x = mp_obj_get_int(xobj);
        // fprintf(stderr, "x = %d\n", (int)x);

        // mp_int_t y = mp_obj_get_int(mp_load_global(qstr_from_str("y")));
        // fprintf(stderr, "y = %d\n", (int)y);

        // const char* foo = mp_obj_str_get_str(mp_load_global(qstr_from_str("foo")));
        // fprintf(stderr, "foo = %s\n", foo);

        nlr_pop();
        return true;
    } else {
        // uncaught exception
        mp_obj_print_exception(&mp_plat_print, ((mp_obj_t)nlr.ret_val));
        return false;
    }
	return true;
}

static void closePython(tic_mem* tic)
{
    mp_deinit();
}

static void callPythonTick(tic_mem* tic)
{
    if (python_vm.TIC_fun != 0) {
        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            mp_call_function_0(python_vm.TIC_fun);
            nlr_pop();
        }else{
            // uncaught exception
            mp_obj_print_exception(&mp_plat_print, ((mp_obj_t)nlr.ret_val));
        }
    }
}

static void callPythonScanline(tic_mem* tic, s32 row, void* data)
{
    if (python_vm.SCN_fun != 0) {
        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            mp_call_function_1(python_vm.SCN_fun, MP_OBJ_NEW_SMALL_INT(row));
            nlr_pop();
        }else{
            // uncaught exception
            mp_obj_print_exception(&mp_plat_print, ((mp_obj_t)nlr.ret_val));
        }
    }
}

static void callPythonOverline(tic_mem* tic, void* data)
{
    if (python_vm.OVR_fun != 0) {
        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            mp_call_function_0(python_vm.OVR_fun);
            nlr_pop();
        }else{
            // uncaught exception
            mp_obj_print_exception(&mp_plat_print, ((mp_obj_t)nlr.ret_val));
        }
    }
}

static const tic_outline_item* getPythonOutline(const char* code, s32* size)
{
    fprintf(stderr, "NOT IMPLEMENTED getPythonOutline(...)\n");
	*size = 0;
    static tic_outline_item* items = NULL;
	return items;
}

static void evalPython(tic_mem* tic, const char* code)
{
    fprintf(stderr, "NOT IMPLEMENTED evalPython(%p, %s)\n", tic, code);
}

static const char* const PythonKeywords [] =
{
	"and",
	"as",
	"assert",
	"break",
	"class",
	"continue",
	"def",
	"del",
	"elif",
	"else",
	"except",
	"False",
	"finally",
	"for",
	"from",
	"global",
	"if",
	"import",
	"in",
	"is",
	"lambda",
	"None",
	"nonlocal",
	"not",
	"or",
	"pass",
	"raise",
	"return",
	"True",
	"try",
	"while",
	"with",
	"yield"
	// await
	// async
};

static const tic_script_config PythonSyntaxConfig = 
{
    .init               = initPython,
    .close              = closePython,
    .tick               = callPythonTick,
    .scanline           = callPythonScanline,
    .overline           = callPythonOverline,

    .getOutline         = getPythonOutline,
    .eval               = evalPython,

    .blockCommentStart  = "\"\"\"",
    .blockCommentEnd    = "\"\"\"",
    .blockCommentStart2 = NULL,
    .blockCommentEnd2   = NULL,
    .blockStringStart   = "\"\"\"",
    .blockStringEnd     = "\"\"\"",
    .singleComment      = "#",

    .keywords           = PythonKeywords,
    .keywordsCount      = COUNT_OF(PythonKeywords),
};

const tic_script_config* getPythonScriptConfig()
{
    return &PythonSyntaxConfig;
}

#endif
