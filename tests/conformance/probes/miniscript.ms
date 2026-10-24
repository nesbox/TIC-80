// API conformance probes for miniscript — the same sections as lua.lua, in the
// same order; that file is the reference and carries the protocol notes.
//
// The api is the tic80 module, so every call is tic80.name; the prelude traces
// the marker, answers "!missing" for a name the module does not carry
// (hasIndex), and runs the body function it is handed. MiniScript has no
// try/catch: a call the binding refuses would end the script and fail the
// measurement, and none of the reference's calls refuses here — the module
// coerces what lua coerces and defaults what lua defaults, which is what the
// forty-four divergences below are.

probe = function(id, f)
    tic80.trace("#" + id)

    name = id.split("/")[0]

    if not tic80.hasIndex(name) then
        tic80.trace("!missing")
        return
    end if

    f
end function

// ---- cls

p_cls_default = function
    tic80.pix(10, 10, 4)
    tic80.cls()
    tic80.pix(10, 10)
end function

probe("cls/default", @p_cls_default)

p_cls_color = function
    tic80.cls(7)
    tic80.pix(4, 4)
    tic80.pix(235, 131)
end function

probe("cls/color", @p_cls_color)

p_cls_wrong_type = function
    tic80.cls(6)
    tic80.cls("x")
    tic80.pix(0, 0)
end function

probe("cls/wrong-type", @p_cls_wrong_type)

p_cls_out_of_range = function
    tic80.pix(0, 0, 6)
    tic80.cls(99)
    tic80.pix(0, 0)
end function

probe("cls/out-of-range", @p_cls_out_of_range)

// ---- print

p_print_width = function
    tic80.print("A", 5, 5, 15)
end function

probe("print/width", @p_print_width)

p_print_defaults = function
    tic80.print("A")
end function

probe("print/defaults", @p_print_defaults)

p_print_scale = function
    tic80.print("A", 5, 5, 15, false, 2)
end function

probe("print/scale", @p_print_scale)

p_print_fixed = function
    tic80.print("ii", 0, 0, 15, true)
    tic80.print("ww", 0, 10, 15, true)
end function

probe("print/fixed", @p_print_fixed)

p_print_no_args = function
    tic80.print()
    tic80.trace("done")
end function

probe("print/no-args", @p_print_no_args)

// ---- pix

p_pix_write = function
    tic80.cls(0)
    tic80.pix(10, 10, 4)
    tic80.pix(10, 10)
end function

probe("pix/write", @p_pix_write)

p_pix_read = function
    tic80.cls(2)
    tic80.pix(3, 4)
end function

probe("pix/read", @p_pix_read)

p_pix_missing_y = function
    tic80.cls(5)
    tic80.pix(1)
    tic80.pix(1, 0)
    tic80.pix(0, 0)
end function

probe("pix/missing-y", @p_pix_missing_y)

p_pix_no_args = function
    tic80.cls(3)
    tic80.pix()
    tic80.pix(0, 0)
end function

probe("pix/no-args", @p_pix_no_args)

p_pix_float_coords = function
    tic80.cls(0)
    tic80.pix(10.7, 11.7, 4)
    tic80.pix(10, 11)
    tic80.pix(11, 11)
    tic80.pix(10, 12)
end function

probe("pix/float-coords", @p_pix_float_coords)

p_pix_string_arg = function
    tic80.cls(0)
    tic80.pix("1", 2, 4)
    tic80.pix(1, 2)
end function

probe("pix/string-arg", @p_pix_string_arg)

p_pix_out_of_bounds = function
    tic80.pix(300, 300, 4)
    tic80.pix(300, 300)
end function

probe("pix/out-of-bounds", @p_pix_out_of_bounds)

// ---- line

p_line_diagonal = function
    tic80.cls(0)
    tic80.line(0, 0, 5, 5, 6)
    tic80.pix(0, 0)
    tic80.pix(3, 3)
    tic80.pix(5, 5)
    tic80.pix(5, 0)
end function

probe("line/diagonal", @p_line_diagonal)

p_line_floats = function
    tic80.cls(0)
    tic80.line(0.4, 1.6, 5.4, 1.6, 6)
    tic80.pix(0, 1)
    tic80.pix(0, 2)
    tic80.pix(5, 1)
    tic80.pix(5, 2)
end function

probe("line/floats", @p_line_floats)

p_line_missing_color = function
    tic80.cls(0)
    tic80.line(0, 0, 5, 5)
    tic80.pix(0, 0)
end function

probe("line/missing-color", @p_line_missing_color)

p_line_one_point = function
    tic80.cls(0)
    tic80.line(3, 3, 3, 3, 6)
    tic80.pix(3, 3)
end function

probe("line/one-point", @p_line_one_point)

// ---- rect

p_rect_fill = function
    tic80.cls(0)
    tic80.rect(1, 1, 3, 3, 6)
    tic80.pix(1, 1)
    tic80.pix(3, 3)
    tic80.pix(4, 4)
    tic80.pix(0, 0)
end function

probe("rect/fill", @p_rect_fill)

p_rect_zero_size = function
    tic80.cls(0)
    tic80.rect(5, 5, 0, 0, 6)
    tic80.pix(5, 5)
end function

probe("rect/zero-size", @p_rect_zero_size)

p_rect_negative_size = function
    tic80.cls(0)
    tic80.rect(5, 5, -2, -2, 6)
    tic80.pix(5, 5)
    tic80.pix(4, 4)
    tic80.pix(3, 3)
end function

probe("rect/negative-size", @p_rect_negative_size)

p_rect_missing_color = function
    tic80.cls(0)
    tic80.rect(1, 1, 2, 2)
    tic80.pix(1, 1)
end function

probe("rect/missing-color", @p_rect_missing_color)

// ---- rectb

p_rectb_border = function
    tic80.cls(0)
    tic80.rectb(1, 1, 4, 4, 6)
    tic80.pix(1, 1)
    tic80.pix(2, 2)
    tic80.pix(4, 4)
    tic80.pix(5, 5)
end function

probe("rectb/border", @p_rectb_border)

p_rectb_zero_size = function
    tic80.cls(0)
    tic80.rectb(5, 5, 0, 0, 6)
    tic80.pix(5, 5)
end function

probe("rectb/zero-size", @p_rectb_zero_size)

p_rectb_one_wide = function
    tic80.cls(0)
    tic80.rectb(2, 2, 1, 1, 6)
    tic80.pix(2, 2)
    tic80.pix(3, 3)
end function

probe("rectb/one-wide", @p_rectb_one_wide)

p_rectb_missing_color = function
    tic80.cls(0)
    tic80.rectb(1, 1, 2, 2)
    tic80.pix(1, 1)
end function

probe("rectb/missing-color", @p_rectb_missing_color)

// ---- rrect

p_rrect_fill = function
    tic80.cls(0)
    tic80.rrect(10, 10, 20, 20, 6, 4)
    tic80.pix(20, 20)
    tic80.pix(10, 20)
    tic80.pix(14, 10)
    tic80.pix(10, 10)
end function

probe("rrect/fill", @p_rrect_fill)

p_rrect_zero_radius = function
    tic80.cls(0)
    tic80.rrect(10, 10, 20, 20, 6, 0)
    tic80.pix(10, 10)
    tic80.pix(29, 29)
    tic80.pix(30, 30)
end function

probe("rrect/zero-radius", @p_rrect_zero_radius)

p_rrect_exponent = function
    tic80.cls(0)
    tic80.rrect(10, 10, 20, 20, 6, 4, 4)
    tic80.pix(12, 10)
    tic80.pix(11, 10)
    tic80.pix(10, 11)
    tic80.pix(10, 12)
end function

probe("rrect/exponent", @p_rrect_exponent)

p_rrect_missing_radius = function
    tic80.cls(0)
    tic80.rrect(10, 10, 20, 20, 6)
    tic80.pix(10, 10)
end function

probe("rrect/missing-radius", @p_rrect_missing_radius)

// ---- rrectb

p_rrectb_border = function
    tic80.cls(0)
    tic80.rrectb(10, 10, 20, 20, 6, 4)
    tic80.pix(10, 20)
    tic80.pix(20, 10)
    tic80.pix(20, 20)
    tic80.pix(10, 10)
end function

probe("rrectb/border", @p_rrectb_border)

p_rrectb_zero_radius = function
    tic80.cls(0)
    tic80.rrectb(10, 10, 20, 20, 6, 0)
    tic80.pix(10, 10)
    tic80.pix(29, 10)
    tic80.pix(20, 20)
end function

probe("rrectb/zero-radius", @p_rrectb_zero_radius)

p_rrectb_exponent = function
    tic80.cls(0)
    tic80.rrectb(10, 10, 20, 20, 6, 4, 4)
    tic80.pix(11, 10)
    tic80.pix(12, 11)
    tic80.pix(10, 11)
    tic80.pix(12, 10)
end function

probe("rrectb/exponent", @p_rrectb_exponent)

p_rrectb_missing_radius = function
    tic80.cls(0)
    tic80.rrectb(10, 10, 20, 20, 6)
    tic80.pix(10, 10)
end function

probe("rrectb/missing-radius", @p_rrectb_missing_radius)

// ---- mget

p_mget_zero = function
    tic80.mget(0, 0)
end function

probe("mget/zero", @p_mget_zero)

p_mget_missing_y = function
    tic80.mget(3)
end function

probe("mget/missing-y", @p_mget_missing_y)

p_mget_string_arg = function
    tic80.mget("x", 0)
end function

probe("mget/string-arg", @p_mget_string_arg)

// ---- mset

p_mset_basic = function
    tic80.mset(1, 2, 7)
    tic80.mget(1, 2)
end function

probe("mset/basic", @p_mset_basic)

p_mset_out_of_range_tile = function
    tic80.mset(5, 5, 999)
    tic80.mget(5, 5)
end function

probe("mset/out-of-range-tile", @p_mset_out_of_range_tile)

p_mset_missing_tile = function
    tic80.mset(6, 6, 7)
    tic80.mset(6, 6)
    tic80.mget(6, 6)
end function

probe("mset/missing-tile", @p_mset_missing_tile)

// ---- peek

p_peek_zero = function
    tic80.poke(8, 0)
    tic80.peek(8)
end function

probe("peek/zero", @p_peek_zero)

p_peek_missing_addr = function
    tic80.peek()
end function

probe("peek/missing-addr", @p_peek_missing_addr)

p_peek_out_of_bounds = function
    tic80.peek(1000000)
end function

probe("peek/out-of-bounds", @p_peek_out_of_bounds)

p_peek_negative_addr = function
    tic80.peek(-1)
end function

probe("peek/negative-addr", @p_peek_negative_addr)

// ---- poke

p_poke_byte = function
    tic80.poke(8, 42)
    tic80.peek(8)
end function

probe("poke/byte", @p_poke_byte)

p_poke_nibble = function
    tic80.poke(32, 9, 4)
    tic80.peek(32, 4)
end function

probe("poke/nibble", @p_poke_nibble)

p_poke_missing_value = function
    tic80.poke(40, 7)
    tic80.poke(40)
    tic80.peek(40)
end function

probe("poke/missing-value", @p_poke_missing_value)

p_poke_out_of_bounds = function
    tic80.poke(1000000, 1)
end function

probe("poke/out-of-bounds", @p_poke_out_of_bounds)

// ---- trace

p_trace_basic = function
    tic80.trace("hello")
end function

probe("trace/basic", @p_trace_basic)

p_trace_color = function
    tic80.trace("hello", 5)
end function

probe("trace/color", @p_trace_color)

// ---- spr

p_spr_basic = function
    tic80.cls(0)
    tic80.spr(1, 10, 10)
    tic80.pix(10, 10)
end function

probe("spr/basic", @p_spr_basic)

p_spr_colorkey = function
    tic80.cls(0)
    tic80.spr(1, 20, 20, 4)
    tic80.pix(20, 20)
end function

probe("spr/colorkey", @p_spr_colorkey)

p_spr_scale = function
    tic80.cls(0)
    tic80.spr(1, 30, 30, -1, 2)
    tic80.pix(30, 30)
end function

probe("spr/scale", @p_spr_scale)

p_spr_flip_rotate = function
    tic80.cls(0)
    tic80.spr(1, 40, 40, -1, 1, 3, 2)
    tic80.pix(40, 40)
end function

probe("spr/flip-rotate", @p_spr_flip_rotate)

// ---- map

p_map_defaults = function
    tic80.cls(0)
    tic80.map()
    tic80.pix(0, 0)
end function

probe("map/defaults", @p_map_defaults)

p_map_size = function
    tic80.cls(0)
    tic80.map(0, 0, 4, 4)
    tic80.pix(0, 0)
end function

probe("map/size", @p_map_size)

p_map_scale = function
    tic80.cls(0)
    tic80.map(0, 0, 4, 4, 0, 0, -1, 2)
    tic80.pix(0, 0)
end function

probe("map/scale", @p_map_scale)

// ---- btn

p_btn_pressed = function
    tic80.btn(0)
    tic80.btn(4)
end function

probe("btn/pressed", @p_btn_pressed)

p_btn_missing_id = function
    tic80.btn()
end function

probe("btn/missing-id", @p_btn_missing_id)

// ---- btnp

p_btnp_pressed = function
    tic80.btnp(0)
end function

probe("btnp/pressed", @p_btnp_pressed)

p_btnp_hold_period = function
    tic80.btnp(0, 10, 5)
end function

probe("btnp/hold-period", @p_btnp_hold_period)

// ---- btnr

p_btnr_released = function
    tic80.btnr(0)
end function

probe("btnr/released", @p_btnr_released)

p_btnr_missing_id = function
    tic80.btnr()
end function

probe("btnr/missing-id", @p_btnr_missing_id)

// ---- key

p_key_no_key = function
    tic80.key()
end function

probe("key/no-key", @p_key_no_key)

p_key_code = function
    tic80.key(1)
end function

probe("key/code", @p_key_code)

// ---- keyp

p_keyp_no_key = function
    tic80.keyp()
end function

probe("keyp/no-key", @p_keyp_no_key)

p_keyp_hold_period = function
    tic80.keyp(1, 10, 5)
end function

probe("keyp/hold-period", @p_keyp_hold_period)

// ---- keyr

p_keyr_no_key = function
    tic80.keyr()
end function

probe("keyr/no-key", @p_keyr_no_key)

p_keyr_code = function
    tic80.keyr(1)
end function

probe("keyr/code", @p_keyr_code)

// ---- mouse

p_mouse_idle = function
    tic80.mouse()
end function

probe("mouse/idle", @p_mouse_idle)

// ---- circ

p_circ_fill = function
    tic80.cls(0)
    tic80.circ(20, 20, 5, 6)
    tic80.pix(20, 20)
    tic80.pix(25, 20)
end function

probe("circ/fill", @p_circ_fill)

p_circ_zero_radius = function
    tic80.cls(0)
    tic80.circ(30, 30, 0, 6)
    tic80.pix(30, 30)
end function

probe("circ/zero-radius", @p_circ_zero_radius)

// ---- circb

p_circb_border = function
    tic80.cls(0)
    tic80.circb(20, 20, 5, 6)
    tic80.pix(20, 20)
    tic80.pix(25, 20)
end function

probe("circb/border", @p_circb_border)

p_circb_missing_color = function
    tic80.cls(0)
    tic80.circb(20, 20, 5)
    tic80.pix(25, 20)
end function

probe("circb/missing-color", @p_circb_missing_color)

// ---- elli

p_elli_fill = function
    tic80.cls(0)
    tic80.elli(40, 40, 6, 3, 6)
    tic80.pix(40, 40)
    tic80.pix(46, 40)
end function

probe("elli/fill", @p_elli_fill)

p_elli_zero_radius = function
    tic80.cls(0)
    tic80.elli(40, 40, 0, 0, 6)
    tic80.pix(40, 40)
end function

probe("elli/zero-radius", @p_elli_zero_radius)

// ---- ellib

p_ellib_border = function
    tic80.cls(0)
    tic80.ellib(40, 40, 6, 3, 6)
    tic80.pix(40, 40)
    tic80.pix(46, 40)
end function

probe("ellib/border", @p_ellib_border)

p_ellib_missing_color = function
    tic80.cls(0)
    tic80.ellib(40, 40, 6, 3)
    tic80.pix(46, 40)
end function

probe("ellib/missing-color", @p_ellib_missing_color)

// ---- paint

p_paint_fill = function
    tic80.cls(0)
    tic80.paint(5, 5, 6)
    tic80.pix(0, 0)
    tic80.pix(239, 135)
end function

probe("paint/fill", @p_paint_fill)

p_paint_missing_color = function
    tic80.cls(0)
    tic80.paint(5, 5)
    tic80.pix(0, 0)
end function

probe("paint/missing-color", @p_paint_missing_color)

// ---- tri

p_tri_fill = function
    tic80.cls(0)
    tic80.tri(0, 0, 10, 0, 0, 10, 6)
    tic80.pix(0, 0)
    tic80.pix(5, 0)
    tic80.pix(10, 10)
end function

probe("tri/fill", @p_tri_fill)

p_tri_missing_color = function
    tic80.cls(0)
    tic80.tri(0, 0, 10, 0, 0, 10)
    tic80.pix(0, 0)
end function

probe("tri/missing-color", @p_tri_missing_color)

// ---- trib

p_trib_border = function
    tic80.cls(0)
    tic80.trib(0, 0, 10, 0, 0, 10, 6)
    tic80.pix(0, 0)
    tic80.pix(5, 0)
end function

probe("trib/border", @p_trib_border)

p_trib_missing_color = function
    tic80.cls(0)
    tic80.trib(0, 0, 10, 0, 0, 10)
    tic80.pix(0, 0)
end function

probe("trib/missing-color", @p_trib_missing_color)

// ---- ttri

p_ttri_basic = function
    tic80.cls(0)
    tic80.ttri(0, 0, 10, 0, 0, 10, 0, 0, 1, 0, 0, 1)
    tic80.pix(0, 0)
    tic80.pix(5, 0)
end function

probe("ttri/basic", @p_ttri_basic)

p_ttri_chromakey = function
    tic80.cls(0)
    tic80.ttri(0, 0, 10, 0, 0, 10, 0, 0, 1, 0, 0, 1, 0, 4)
    tic80.pix(0, 0)
end function

probe("ttri/chromakey", @p_ttri_chromakey)

// ---- clip

p_clip_set = function
    tic80.cls(0)
    tic80.clip(2, 2, 5, 5)
    tic80.rect(0, 0, 10, 10, 6)
    tic80.pix(0, 0)
    tic80.pix(2, 2)
    tic80.clip()
end function

probe("clip/set", @p_clip_set)

p_clip_reset = function
    tic80.cls(0)
    tic80.clip()
    tic80.rect(0, 0, 2, 2, 6)
    tic80.pix(0, 0)
end function

probe("clip/reset", @p_clip_reset)

// ---- font

p_font_text = function
    tic80.cls(0)
    tic80.font("ab", 0, 0, -1, 6, 6)
    tic80.pix(0, 0)
end function

probe("font/text", @p_font_text)

// ---- sfx

p_sfx_basic = function
    tic80.sfx(0)
end function

probe("sfx/basic", @p_sfx_basic)

p_sfx_note = function
    tic80.sfx(0, 48)
end function

probe("sfx/note", @p_sfx_note)

p_sfx_channel = function
    tic80.sfx(0, 48, -1, 1)
end function

probe("sfx/channel", @p_sfx_channel)

p_sfx_missing_id = function
    tic80.sfx()
end function

probe("sfx/missing-id", @p_sfx_missing_id)

// ---- music

p_music_basic = function
    tic80.music(0)
end function

probe("music/basic", @p_music_basic)

p_music_frame = function
    tic80.music(0, 0, 0, true, false)
end function

probe("music/frame", @p_music_frame)

// ---- fget

p_fget_out_of_range = function
    tic80.fget(500, 0)
end function

probe("fget/out-of-range", @p_fget_out_of_range)

// ---- fset

p_fset_roundtrip = function
    tic80.fset(0, 0, true)
    tic80.fget(0, 0)
end function

probe("fset/roundtrip", @p_fset_roundtrip)

p_fset_missing_bool = function
    tic80.fset(0, 1)
    tic80.fget(0, 1)
end function

probe("fset/missing-bool", @p_fset_missing_bool)

// ---- peek1

p_peek1_read = function
    tic80.peek1(16)
end function

probe("peek1/read", @p_peek1_read)

// ---- poke1

p_poke1_roundtrip = function
    tic80.poke1(16, 7)
    tic80.peek1(16)
end function

probe("poke1/roundtrip", @p_poke1_roundtrip)

// ---- peek2

p_peek2_read = function
    tic80.peek2(24)
end function

probe("peek2/read", @p_peek2_read)

// ---- poke2

p_poke2_roundtrip = function
    tic80.poke2(24, 300)
    tic80.peek2(24)
end function

probe("poke2/roundtrip", @p_poke2_roundtrip)

// ---- peek4

p_peek4_read = function
    tic80.peek4(64)
end function

probe("peek4/read", @p_peek4_read)

// ---- poke4

p_poke4_roundtrip = function
    tic80.poke4(64, 9)
    tic80.peek4(64)
end function

probe("poke4/roundtrip", @p_poke4_roundtrip)

// ---- memset

p_memset_bytes = function
    tic80.memset(48, 7, 2)
    tic80.peek(48)
    tic80.peek(49)
end function

probe("memset/bytes", @p_memset_bytes)

// ---- memcpy

p_memcpy_copy = function
    tic80.poke(56, 1)
    tic80.poke(57, 2)
    tic80.memcpy(60, 56, 2)
    tic80.peek(60)
    tic80.peek(61)
end function

probe("memcpy/copy", @p_memcpy_copy)

// ---- pmem

p_pmem_read = function
    tic80.pmem(0)
end function

probe("pmem/read", @p_pmem_read)

p_pmem_write_read = function
    tic80.pmem(0, 42)
    tic80.pmem(0)
end function

probe("pmem/write-read", @p_pmem_write_read)

p_pmem_out_of_range = function
    tic80.pmem(1000)
end function

probe("pmem/out-of-range", @p_pmem_out_of_range)

// ---- time

p_time_stub = function
    tic80.time()
end function

probe("time/stub", @p_time_stub)

// ---- tstamp

p_tstamp_stub = function
    tic80.tstamp()
end function

probe("tstamp/stub", @p_tstamp_stub)

// ---- fft

p_fft_sample = function
    tic80.fft(0)
end function

probe("fft/sample", @p_fft_sample)

// ---- ffts

p_ffts_spectrum = function
    tic80.ffts(0)
end function

probe("ffts/spectrum", @p_ffts_spectrum)

// ---- fftr

p_fftr_range = function
    tic80.fftr(0, 100)
end function

probe("fftr/range", @p_fftr_range)

// ---- fftrs

p_fftrs_range = function
    tic80.fftrs(0, 100)
end function

probe("fftrs/range", @p_fftrs_range)

// ---- vqt

p_vqt_read = function
    tic80.vqt(0)
end function

probe("vqt/read", @p_vqt_read)

// ---- vqts

p_vqts_read = function
    tic80.vqts(0)
end function

probe("vqts/read", @p_vqts_read)

// ---- vqtr

p_vqtr_read = function
    tic80.vqtr(0)
end function

probe("vqtr/read", @p_vqtr_read)

// ---- vqtrs

p_vqtrs_read = function
    tic80.vqtrs(0)
end function

probe("vqtrs/read", @p_vqtrs_read)

// ---- vqtw

p_vqtw_read = function
    tic80.vqtw(0)
end function

probe("vqtw/read", @p_vqtw_read)

// ---- vqtsw

p_vqtsw_read = function
    tic80.vqtsw(0)
end function

probe("vqtsw/read", @p_vqtsw_read)

// ---- vqtrw

p_vqtrw_read = function
    tic80.vqtrw(0)
end function

probe("vqtrw/read", @p_vqtrw_read)

// ---- vqtrsw

p_vqtrsw_read = function
    tic80.vqtrsw(0)
end function

probe("vqtrsw/read", @p_vqtrsw_read)

// ---- vbank

p_vbank_switch = function
    tic80.vbank(1)
    tic80.vbank(0)
end function

probe("vbank/switch", @p_vbank_switch)

p_vbank_no_args = function
    tic80.vbank()
    tic80.trace("done")
end function

probe("vbank/no-args", @p_vbank_no_args)

// ---- sync

p_sync_defaults = function
    tic80.sync()
end function

probe("sync/defaults", @p_sync_defaults)

p_sync_mask = function
    tic80.sync(32767, 0, false)
end function

probe("sync/mask", @p_sync_mask)

// ---- exit

p_exit_call = function
    tic80.exit()
end function

probe("exit/call", @p_exit_call)

// ---- reset

p_reset_call = function
    tic80.reset()
    tic80.trace("done")
end function

probe("reset/call", @p_reset_call)

// The engine runs a cart's TIC every tick; the probes live at the top level.
TIC = function
end function
