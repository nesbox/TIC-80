# API conformance probes for python (pocketpy) — the same sections as lua.lua,
# in the same order; that file is the reference and carries the protocol
# notes.
#
# probe(id, f) traces the marker, then runs f; the runner records every API
# call with the values it arrived with, and a call the language refuses ends
# the probe with "!error" — the refusal is a finding of its own. Python's
# lambda cannot hold statements, so every probe body is a def and the call
# follows it.

def probe(id, f):
    trace('#' + id)

    name = id.split('/')[0]

    if name not in globals():
        trace('!missing')
        return

    try:
        f()
    except:
        trace('!error')



# ---- cls

def p_cls_default():
    pix(10, 10, 4)
    cls()
    pix(10, 10)

probe('cls/default', p_cls_default)

def p_cls_color():
    cls(7)
    pix(4, 4)
    pix(235, 131)

probe('cls/color', p_cls_color)

def p_cls_wrong_type():
    cls(6)
    cls('x')
    pix(0, 0)

probe('cls/wrong-type', p_cls_wrong_type)

def p_cls_out_of_range():
    pix(0, 0, 6)
    cls(99)
    pix(0, 0)

probe('cls/out-of-range', p_cls_out_of_range)

# ---- print

def p_print_width():
    print('A', 5, 5, 15)

probe('print/width', p_print_width)

def p_print_defaults():
    print('A')

probe('print/defaults', p_print_defaults)

def p_print_scale():
    print('A', 5, 5, 15, False, 2)

probe('print/scale', p_print_scale)

def p_print_fixed():
    print('ii', 0, 0, 15, True)
    print('ww', 0, 10, 15, True)

probe('print/fixed', p_print_fixed)

def p_print_no_args():
    print()
    trace('done')

probe('print/no-args', p_print_no_args)

# ---- pix

def p_pix_write():
    cls(0)
    pix(10, 10, 4)
    pix(10, 10)

probe('pix/write', p_pix_write)

def p_pix_read():
    cls(2)
    pix(3, 4)

probe('pix/read', p_pix_read)

def p_pix_missing_y():
    cls(5)
    pix(1)
    pix(1, 0)
    pix(0, 0)

probe('pix/missing-y', p_pix_missing_y)

def p_pix_no_args():
    cls(3)
    pix()
    pix(0, 0)

probe('pix/no-args', p_pix_no_args)

def p_pix_float_coords():
    cls(0)
    pix(10.7, 11.7, 4)
    pix(10, 11)
    pix(11, 11)
    pix(10, 12)

probe('pix/float-coords', p_pix_float_coords)

def p_pix_string_arg():
    cls(0)
    pix('1', 2, 4)
    pix(1, 2)

probe('pix/string-arg', p_pix_string_arg)

def p_pix_out_of_bounds():
    pix(300, 300, 4)
    pix(300, 300)

probe('pix/out-of-bounds', p_pix_out_of_bounds)

# ---- line

def p_line_diagonal():
    cls(0)
    line(0, 0, 5, 5, 6)
    pix(0, 0)
    pix(3, 3)
    pix(5, 5)
    pix(5, 0)

probe('line/diagonal', p_line_diagonal)

def p_line_floats():
    cls(0)
    line(0.4, 1.6, 5.4, 1.6, 6)
    pix(0, 1)
    pix(0, 2)
    pix(5, 1)
    pix(5, 2)

probe('line/floats', p_line_floats)

def p_line_missing_color():
    cls(0)
    line(0, 0, 5, 5)
    pix(0, 0)

probe('line/missing-color', p_line_missing_color)

def p_line_one_point():
    cls(0)
    line(3, 3, 3, 3, 6)
    pix(3, 3)

probe('line/one-point', p_line_one_point)

# ---- rect

def p_rect_fill():
    cls(0)
    rect(1, 1, 3, 3, 6)
    pix(1, 1)
    pix(3, 3)
    pix(4, 4)
    pix(0, 0)

probe('rect/fill', p_rect_fill)

def p_rect_zero_size():
    cls(0)
    rect(5, 5, 0, 0, 6)
    pix(5, 5)

probe('rect/zero-size', p_rect_zero_size)

def p_rect_negative_size():
    cls(0)
    rect(5, 5, -2, -2, 6)
    pix(5, 5)
    pix(4, 4)
    pix(3, 3)

probe('rect/negative-size', p_rect_negative_size)

def p_rect_missing_color():
    cls(0)
    rect(1, 1, 2, 2)
    pix(1, 1)

probe('rect/missing-color', p_rect_missing_color)

# ---- rectb

def p_rectb_border():
    cls(0)
    rectb(1, 1, 4, 4, 6)
    pix(1, 1)
    pix(2, 2)
    pix(4, 4)
    pix(5, 5)

probe('rectb/border', p_rectb_border)

def p_rectb_zero_size():
    cls(0)
    rectb(5, 5, 0, 0, 6)
    pix(5, 5)

probe('rectb/zero-size', p_rectb_zero_size)

def p_rectb_one_wide():
    cls(0)
    rectb(2, 2, 1, 1, 6)
    pix(2, 2)
    pix(3, 3)

probe('rectb/one-wide', p_rectb_one_wide)

def p_rectb_missing_color():
    cls(0)
    rectb(1, 1, 2, 2)
    pix(1, 1)

probe('rectb/missing-color', p_rectb_missing_color)

# ---- rrect

def p_rrect_fill():
    cls(0)
    rrect(10, 10, 20, 20, 6, 4)
    pix(20, 20)
    pix(10, 20)
    pix(14, 10)
    pix(10, 10)

probe('rrect/fill', p_rrect_fill)

def p_rrect_zero_radius():
    cls(0)
    rrect(10, 10, 20, 20, 6, 0)
    pix(10, 10)
    pix(29, 29)
    pix(30, 30)

probe('rrect/zero-radius', p_rrect_zero_radius)

def p_rrect_exponent():
    cls(0)
    rrect(10, 10, 20, 20, 6, 4, 4)
    pix(12, 10)
    pix(11, 10)
    pix(10, 11)
    pix(10, 12)

probe('rrect/exponent', p_rrect_exponent)

def p_rrect_missing_radius():
    cls(0)
    rrect(10, 10, 20, 20, 6)
    pix(10, 10)

probe('rrect/missing-radius', p_rrect_missing_radius)

# ---- rrectb

def p_rrectb_border():
    cls(0)
    rrectb(10, 10, 20, 20, 6, 4)
    pix(10, 20)
    pix(20, 10)
    pix(20, 20)
    pix(10, 10)

probe('rrectb/border', p_rrectb_border)

def p_rrectb_zero_radius():
    cls(0)
    rrectb(10, 10, 20, 20, 6, 0)
    pix(10, 10)
    pix(29, 10)
    pix(20, 20)

probe('rrectb/zero-radius', p_rrectb_zero_radius)

def p_rrectb_exponent():
    cls(0)
    rrectb(10, 10, 20, 20, 6, 4, 4)
    pix(11, 10)
    pix(12, 11)
    pix(10, 11)
    pix(12, 10)

probe('rrectb/exponent', p_rrectb_exponent)

def p_rrectb_missing_radius():
    cls(0)
    rrectb(10, 10, 20, 20, 6)
    pix(10, 10)

probe('rrectb/missing-radius', p_rrectb_missing_radius)

# ---- mget

def p_mget_zero():
    mget(0, 0)

probe('mget/zero', p_mget_zero)

def p_mget_missing_y():
    mget(3)

probe('mget/missing-y', p_mget_missing_y)

def p_mget_string_arg():
    mget('x', 0)

probe('mget/string-arg', p_mget_string_arg)

# ---- mset

def p_mset_basic():
    mset(1, 2, 7)
    mget(1, 2)

probe('mset/basic', p_mset_basic)

def p_mset_out_of_range_tile():
    mset(5, 5, 999)
    mget(5, 5)

probe('mset/out-of-range-tile', p_mset_out_of_range_tile)

def p_mset_missing_tile():
    mset(6, 6, 7)
    mset(6, 6)
    mget(6, 6)

probe('mset/missing-tile', p_mset_missing_tile)

# ---- peek

def p_peek_zero():
    poke(8, 0)
    peek(8)

probe('peek/zero', p_peek_zero)

def p_peek_missing_addr():
    peek()

probe('peek/missing-addr', p_peek_missing_addr)

def p_peek_out_of_bounds():
    peek(1000000)

probe('peek/out-of-bounds', p_peek_out_of_bounds)

def p_peek_negative_addr():
    peek(-1)

probe('peek/negative-addr', p_peek_negative_addr)

# ---- poke

def p_poke_byte():
    poke(8, 42)
    peek(8)

probe('poke/byte', p_poke_byte)

def p_poke_nibble():
    poke(32, 9, 4)
    peek(32, 4)

probe('poke/nibble', p_poke_nibble)

def p_poke_missing_value():
    poke(40, 7)
    poke(40)
    peek(40)

probe('poke/missing-value', p_poke_missing_value)

def p_poke_out_of_bounds():
    poke(1000000, 1)

probe('poke/out-of-bounds', p_poke_out_of_bounds)

# ---- trace

def p_trace_basic():
    trace('hello')

probe('trace/basic', p_trace_basic)

def p_trace_color():
    trace('hello', 5)

probe('trace/color', p_trace_color)

# ---- spr

def p_spr_basic():
    cls(0)
    spr(1, 10, 10)
    pix(10, 10)

probe('spr/basic', p_spr_basic)

def p_spr_colorkey():
    cls(0)
    spr(1, 20, 20, 4)
    pix(20, 20)

probe('spr/colorkey', p_spr_colorkey)

def p_spr_scale():
    cls(0)
    spr(1, 30, 30, -1, 2)
    pix(30, 30)

probe('spr/scale', p_spr_scale)

def p_spr_flip_rotate():
    cls(0)
    spr(1, 40, 40, -1, 1, 3, 2)
    pix(40, 40)

probe('spr/flip-rotate', p_spr_flip_rotate)

# ---- map

def p_map_defaults():
    cls(0)
    map()
    pix(0, 0)

probe('map/defaults', p_map_defaults)

def p_map_size():
    cls(0)
    map(0, 0, 4, 4)
    pix(0, 0)

probe('map/size', p_map_size)

def p_map_scale():
    cls(0)
    map(0, 0, 4, 4, 0, 0, -1, 2)
    pix(0, 0)

probe('map/scale', p_map_scale)

# ---- btn

def p_btn_pressed():
    btn(0)
    btn(4)

probe('btn/pressed', p_btn_pressed)

def p_btn_missing_id():
    btn()

probe('btn/missing-id', p_btn_missing_id)

# ---- btnp

def p_btnp_pressed():
    btnp(0)

probe('btnp/pressed', p_btnp_pressed)

def p_btnp_hold_period():
    btnp(0, 10, 5)

probe('btnp/hold-period', p_btnp_hold_period)

# ---- key

def p_key_no_key():
    key()

probe('key/no-key', p_key_no_key)

def p_key_code():
    key(1)

probe('key/code', p_key_code)

# ---- keyp

def p_keyp_no_key():
    keyp()

probe('keyp/no-key', p_keyp_no_key)

def p_keyp_hold_period():
    keyp(1, 10, 5)

probe('keyp/hold-period', p_keyp_hold_period)

# ---- mouse

def p_mouse_idle():
    mouse()

probe('mouse/idle', p_mouse_idle)

# ---- circ

def p_circ_fill():
    cls(0)
    circ(20, 20, 5, 6)
    pix(20, 20)
    pix(25, 20)

probe('circ/fill', p_circ_fill)

def p_circ_zero_radius():
    cls(0)
    circ(30, 30, 0, 6)
    pix(30, 30)

probe('circ/zero-radius', p_circ_zero_radius)

# ---- circb

def p_circb_border():
    cls(0)
    circb(20, 20, 5, 6)
    pix(20, 20)
    pix(25, 20)

probe('circb/border', p_circb_border)

def p_circb_missing_color():
    cls(0)
    circb(20, 20, 5)
    pix(25, 20)

probe('circb/missing-color', p_circb_missing_color)

# ---- elli

def p_elli_fill():
    cls(0)
    elli(40, 40, 6, 3, 6)
    pix(40, 40)
    pix(46, 40)

probe('elli/fill', p_elli_fill)

def p_elli_zero_radius():
    cls(0)
    elli(40, 40, 0, 0, 6)
    pix(40, 40)

probe('elli/zero-radius', p_elli_zero_radius)

# ---- ellib

def p_ellib_border():
    cls(0)
    ellib(40, 40, 6, 3, 6)
    pix(40, 40)
    pix(46, 40)

probe('ellib/border', p_ellib_border)

def p_ellib_missing_color():
    cls(0)
    ellib(40, 40, 6, 3)
    pix(46, 40)

probe('ellib/missing-color', p_ellib_missing_color)

# ---- paint

def p_paint_fill():
    cls(0)
    paint(5, 5, 6)
    pix(0, 0)
    pix(239, 135)

probe('paint/fill', p_paint_fill)

def p_paint_missing_color():
    cls(0)
    paint(5, 5)
    pix(0, 0)

probe('paint/missing-color', p_paint_missing_color)

# ---- tri

def p_tri_fill():
    cls(0)
    tri(0, 0, 10, 0, 0, 10, 6)
    pix(0, 0)
    pix(5, 0)
    pix(10, 10)

probe('tri/fill', p_tri_fill)

def p_tri_missing_color():
    cls(0)
    tri(0, 0, 10, 0, 0, 10)
    pix(0, 0)

probe('tri/missing-color', p_tri_missing_color)

# ---- trib

def p_trib_border():
    cls(0)
    trib(0, 0, 10, 0, 0, 10, 6)
    pix(0, 0)
    pix(5, 0)

probe('trib/border', p_trib_border)

def p_trib_missing_color():
    cls(0)
    trib(0, 0, 10, 0, 0, 10)
    pix(0, 0)

probe('trib/missing-color', p_trib_missing_color)

# ---- ttri

def p_ttri_basic():
    cls(0)
    ttri(0, 0, 10, 0, 0, 10, 0, 0, 1, 0, 0, 1)
    pix(0, 0)
    pix(5, 0)

probe('ttri/basic', p_ttri_basic)

def p_ttri_chromakey():
    cls(0)
    ttri(0, 0, 10, 0, 0, 10, 0, 0, 1, 0, 0, 1, 0, 4)
    pix(0, 0)

probe('ttri/chromakey', p_ttri_chromakey)

# ---- clip

def p_clip_set():
    cls(0)
    clip(2, 2, 5, 5)
    rect(0, 0, 10, 10, 6)
    pix(0, 0)
    pix(2, 2)
    clip()

probe('clip/set', p_clip_set)

def p_clip_reset():
    cls(0)
    clip()
    rect(0, 0, 2, 2, 6)
    pix(0, 0)

probe('clip/reset', p_clip_reset)

# ---- font

def p_font_text():
    cls(0)
    font('ab', 0, 0, -1, 6, 6)
    pix(0, 0)

probe('font/text', p_font_text)

# ---- sfx

def p_sfx_basic():
    sfx(0)

probe('sfx/basic', p_sfx_basic)

def p_sfx_note():
    sfx(0, 48)

probe('sfx/note', p_sfx_note)

def p_sfx_channel():
    sfx(0, 48, -1, 1)

probe('sfx/channel', p_sfx_channel)

def p_sfx_missing_id():
    sfx()

probe('sfx/missing-id', p_sfx_missing_id)

# ---- music

def p_music_basic():
    music(0)

probe('music/basic', p_music_basic)

def p_music_frame():
    music(0, 0, 0, True, False)

probe('music/frame', p_music_frame)

# ---- fget

def p_fget_out_of_range():
    fget(500, 0)

probe('fget/out-of-range', p_fget_out_of_range)

# ---- fset

def p_fset_roundtrip():
    fset(0, 0, True)
    fget(0, 0)

probe('fset/roundtrip', p_fset_roundtrip)

def p_fset_missing_bool():
    fset(0, 1)
    fget(0, 1)

probe('fset/missing-bool', p_fset_missing_bool)

# ---- peek1

def p_peek1_read():
    peek1(16)

probe('peek1/read', p_peek1_read)

# ---- poke1

def p_poke1_roundtrip():
    poke1(16, 7)
    peek1(16)

probe('poke1/roundtrip', p_poke1_roundtrip)

# ---- peek2

def p_peek2_read():
    peek2(24)

probe('peek2/read', p_peek2_read)

# ---- poke2

def p_poke2_roundtrip():
    poke2(24, 300)
    peek2(24)

probe('poke2/roundtrip', p_poke2_roundtrip)

# ---- peek4

def p_peek4_read():
    peek4(64)

probe('peek4/read', p_peek4_read)

# ---- poke4

def p_poke4_roundtrip():
    poke4(64, 9)
    peek4(64)

probe('poke4/roundtrip', p_poke4_roundtrip)

# ---- memset

def p_memset_bytes():
    memset(48, 7, 2)
    peek(48)
    peek(49)

probe('memset/bytes', p_memset_bytes)

# ---- memcpy

def p_memcpy_copy():
    poke(56, 1)
    poke(57, 2)
    memcpy(60, 56, 2)
    peek(60)
    peek(61)

probe('memcpy/copy', p_memcpy_copy)

# ---- pmem

def p_pmem_read():
    pmem(0)

probe('pmem/read', p_pmem_read)

def p_pmem_write_read():
    pmem(0, 42)
    pmem(0)

probe('pmem/write-read', p_pmem_write_read)

def p_pmem_out_of_range():
    pmem(1000)

probe('pmem/out-of-range', p_pmem_out_of_range)

# ---- time

def p_time_stub():
    time()

probe('time/stub', p_time_stub)

# ---- tstamp

def p_tstamp_stub():
    tstamp()

probe('tstamp/stub', p_tstamp_stub)

# ---- fft

def p_fft_sample():
    fft(0)

probe('fft/sample', p_fft_sample)

# ---- ffts

def p_ffts_spectrum():
    ffts(0)

probe('ffts/spectrum', p_ffts_spectrum)

# ---- fftr

def p_fftr_range():
    fftr(0, 100)

probe('fftr/range', p_fftr_range)

# ---- fftrs

def p_fftrs_range():
    fftrs(0, 100)

probe('fftrs/range', p_fftrs_range)

# ---- vqt

def p_vqt_read():
    vqt(0)

probe('vqt/read', p_vqt_read)

# ---- vqts

def p_vqts_read():
    vqts(0)

probe('vqts/read', p_vqts_read)

# ---- vqtr

def p_vqtr_read():
    vqtr(0)

probe('vqtr/read', p_vqtr_read)

# ---- vqtrs

def p_vqtrs_read():
    vqtrs(0)

probe('vqtrs/read', p_vqtrs_read)

# ---- vqtw

def p_vqtw_read():
    vqtw(0)

probe('vqtw/read', p_vqtw_read)

# ---- vqtsw

def p_vqtsw_read():
    vqtsw(0)

probe('vqtsw/read', p_vqtsw_read)

# ---- vqtrw

def p_vqtrw_read():
    vqtrw(0)

probe('vqtrw/read', p_vqtrw_read)

# ---- vqtrsw

def p_vqtrsw_read():
    vqtrsw(0)

probe('vqtrsw/read', p_vqtrsw_read)

# ---- vbank

def p_vbank_switch():
    vbank(1)
    vbank(0)

probe('vbank/switch', p_vbank_switch)

def p_vbank_no_args():
    vbank()
    trace('done')

probe('vbank/no-args', p_vbank_no_args)

# ---- sync

def p_sync_defaults():
    sync()

probe('sync/defaults', p_sync_defaults)

def p_sync_mask():
    sync(32767, 0, False)

probe('sync/mask', p_sync_mask)

# ---- exit

def p_exit_call():
    exit()

probe('exit/call', p_exit_call)

# ---- reset

def p_reset_call():
    reset()
    trace('done')

probe('reset/call', p_reset_call)

# The engine runs a cart's TIC every tick; the probes live at the top level.
def TIC():
    pass
