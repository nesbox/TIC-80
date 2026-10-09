/* The recording half of the harness: every function in TIC_API_LIST gets a trap
 * that forwards to the real one and appends a line to the record.
 *
 * The real pointers and the installation are both generated from TIC_API_LIST, so
 * a signature that no longer matches the prototype fails to compile on assignment,
 * and a function added to the table fails to compile until it has a trap here. Only
 * the trap bodies are written by hand, because C cannot name the parameters of a
 * variadic declaration.
 */
#include "traps.h"


#include <stdio.h>

#define API_FUNC_DEF(name, _, __, ___, ____, _____, ret, ...) \
    static ret (*Real_##name)(__VA_ARGS__);
TIC_API_LIST(API_FUNC_DEF)
#undef API_FUNC_DEF

#if defined BUILD_DEPRECATED
// Outside TIC_API_LIST: the deprecated entry is assigned by hand in core.c.
static void (*Real_textri)(tic_mem*, float, float, float, float, float, float,
    float, float, float, float, float, float, bool, u8*, s32);
#endif

static const char* b2s(bool value)
{
    return value ? "true" : "false";
}

// A count of zero means the pointer is never read, so an empty array renders the
// same whether the binding passed NULL or a scratch buffer — which it does either
// way. A null pointer with a non-zero count is the divergent case, and says so.
static const char* arrayText(u8* values, u8 count)
{
    static char buffer[80];
    s32 used = 0;

    if(count == 0)
        return "0:[]";

    if(!values)
        return "<null>";

    used += snprintf(buffer + used, sizeof buffer - used, "%u:[", count);

    for(u8 i = 0; i < count; i++)
    {
        if(used > (s32)sizeof buffer - 10)
            break;

        used += snprintf(buffer + used, sizeof buffer - used, "%s%u", i ? "," : "", values[i]);
    }

    if(used > (s32)sizeof buffer - 4)
        used = sizeof buffer - 4;

    snprintf(buffer + used, sizeof buffer - used, "]");

    return buffer;
}

static s32 trap_print(tic_mem* tic, const char* text, s32 x, s32 y, u8 color, bool fixed, s32 scale, bool alt)
{
    s32 width = Real_print(tic, text, x, y, color, fixed, scale, alt);

    api_record("print(\"%s\",%d,%d,%u,%s,%d,%s) -> %d",
        text ? text : "<null>", x, y, (unsigned)color, b2s(fixed), scale, b2s(alt), width);

    return width;
}

static void trap_cls(tic_mem* tic, u8 color)
{
    Real_cls(tic, color);
    api_record("cls(%u)", (unsigned)color);
}

static u8 trap_pix(tic_mem* tic, s32 x, s32 y, u8 color, bool get)
{
    u8 value = Real_pix(tic, x, y, color, get);

    api_record("pix(%d,%d,%u,%s) -> %u", x, y, (unsigned)color, b2s(get), (unsigned)value);

    return value;
}

static void trap_line(tic_mem* tic, float x1, float y1, float x2, float y2, u8 color)
{
    Real_line(tic, x1, y1, x2, y2, color);
    api_record("line(%g,%g,%g,%g,%u)", x1, y1, x2, y2, (unsigned)color);
}

static void trap_rect(tic_mem* tic, s32 x, s32 y, s32 width, s32 height, u8 color)
{
    Real_rect(tic, x, y, width, height, color);
    api_record("rect(%d,%d,%d,%d,%u)", x, y, width, height, (unsigned)color);
}

static void trap_rectb(tic_mem* tic, s32 x, s32 y, s32 width, s32 height, u8 color)
{
    Real_rectb(tic, x, y, width, height, color);
    api_record("rectb(%d,%d,%d,%d,%u)", x, y, width, height, (unsigned)color);
}

static void trap_spr(tic_mem* tic, s32 index, s32 x, s32 y, s32 w, s32 h, u8* trans_colors, u8 trans_count, s32 scale, tic_flip flip, tic_rotate rotate)
{
    Real_spr(tic, index, x, y, w, h, trans_colors, trans_count, scale, flip, rotate);

    api_record("spr(%d,%d,%d,%d,%d,%s,%d,%d,%d)",
        index, x, y, w, h, arrayText(trans_colors, trans_count), scale, (int)flip, (int)rotate);
}

static u32 trap_btn(tic_mem* tic, s32 id)
{
    u32 pressed = Real_btn(tic, id);

    api_record("btn(%d) -> %u", id, pressed);

    return pressed;
}

static u32 trap_btnp(tic_mem* tic, s32 id, s32 hold, s32 period)
{
    u32 pressed = Real_btnp(tic, id, hold, period);

    api_record("btnp(%d,%d,%d) -> %u", id, hold, period, pressed);

    return pressed;
}

static void trap_sfx(tic_mem* tic, s32 index, s32 note, s32 octave, s32 duration, s32 channel, s32 left, s32 right, s32 speed)
{
    Real_sfx(tic, index, note, octave, duration, channel, left, right, speed);
    api_record("sfx(%d,%d,%d,%d,%d,%d,%d,%d)", index, note, octave, duration, channel, left, right, speed);
}

static void trap_map(tic_mem* tic, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8* trans_colors, u8 trans_count, s32 scale, RemapFunc remap, void* data)
{
    Real_map(tic, x, y, width, height, sx, sy, trans_colors, trans_count, scale, remap, data);

    // A function pointer means nothing across languages, but whether one was
    // passed at all does: it is what makes map() call back for a remapped tile.
    api_record("map(%d,%d,%d,%d,%d,%d,%s,%d,remap:%s)",
        x, y, width, height, sx, sy, arrayText(trans_colors, trans_count), scale, remap ? "yes" : "no");
}

static u8 trap_mget(tic_mem* tic, s32 x, s32 y)
{
    u8 value = Real_mget(tic, x, y);

    api_record("mget(%d,%d) -> %u", x, y, (unsigned)value);

    return value;
}

static void trap_mset(tic_mem* tic, s32 x, s32 y, u8 value)
{
    Real_mset(tic, x, y, value);
    api_record("mset(%d,%d,%u)", x, y, (unsigned)value);
}

static u8 trap_peek(tic_mem* tic, s32 address, s32 bits)
{
    u8 value = Real_peek(tic, address, bits);

    api_record("peek(%d,%d) -> %u", address, bits, (unsigned)value);

    return value;
}

static void trap_poke(tic_mem* tic, s32 address, u8 value, s32 bits)
{
    Real_poke(tic, address, value, bits);
    api_record("poke(%d,%u,%d)", address, (unsigned)value, bits);
}

static u8 trap_peek1(tic_mem* tic, s32 address)
{
    u8 value = Real_peek1(tic, address);

    api_record("peek1(%d) -> %u", address, (unsigned)value);

    return value;
}

static void trap_poke1(tic_mem* tic, s32 address, u8 value)
{
    Real_poke1(tic, address, value);
    api_record("poke1(%d,%u)", address, (unsigned)value);
}

static u8 trap_peek2(tic_mem* tic, s32 address)
{
    u8 value = Real_peek2(tic, address);

    api_record("peek2(%d) -> %u", address, (unsigned)value);

    return value;
}

static void trap_poke2(tic_mem* tic, s32 address, u8 value)
{
    Real_poke2(tic, address, value);
    api_record("poke2(%d,%u)", address, (unsigned)value);
}

static u8 trap_peek4(tic_mem* tic, s32 address)
{
    u8 value = Real_peek4(tic, address);

    api_record("peek4(%d) -> %u", address, (unsigned)value);

    return value;
}

static void trap_poke4(tic_mem* tic, s32 address, u8 value)
{
    Real_poke4(tic, address, value);
    api_record("poke4(%d,%u)", address, (unsigned)value);
}

static void trap_memcpy(tic_mem* tic, s32 dst, s32 src, s32 size)
{
    Real_memcpy(tic, dst, src, size);
    api_record("memcpy(%d,%d,%d)", dst, src, size);
}

static void trap_memset(tic_mem* tic, s32 dst, u8 val, s32 size)
{
    Real_memset(tic, dst, val, size);
    api_record("memset(%d,%u,%d)", dst, (unsigned)val, size);
}

static void trap_trace(tic_mem* tic, const char* text, u8 color)
{
    Real_trace(tic, text, color);
    api_record("trace(\"%s\",%u)", text ? text : "<null>", (unsigned)color);
}

static u32 trap_pmem(tic_mem* tic, s32 index, u32 value, bool set)
{
    u32 result = Real_pmem(tic, index, value, set);

    api_record("pmem(%d,%u,%s) -> %u", index, value, b2s(set), result);

    return result;
}

static double trap_time(tic_mem* tic)
{
    double value = Real_time(tic);

    api_record("time() -> %g", value);

    return value;
}

static s32 trap_tstamp(tic_mem* tic)
{
    s32 stamp = Real_tstamp(tic);

    // time(NULL) by construction, so the value is never the same twice and cannot
    // be compared; that the call was made, and with what, still can.
    api_record("tstamp() -> <wall clock>");

    return stamp;
}

static void trap_exit(tic_mem* tic)
{
    Real_exit(tic);
    api_record("exit()");
}

static s32 trap_font(tic_mem* tic, const char* text, s32 x, s32 y, u8* trans_colors, u8 trans_count, s32 w, s32 h, bool fixed, s32 scale, bool alt)
{
    s32 width = Real_font(tic, text, x, y, trans_colors, trans_count, w, h, fixed, scale, alt);

    api_record("font(\"%s\",%d,%d,%s,%d,%d,%s,%d,%s) -> %d",
        text ? text : "<null>", x, y, arrayText(trans_colors, trans_count), w, h, b2s(fixed), scale, b2s(alt), width);

    return width;
}

static tic_point trap_mouse(tic_mem* tic)
{
    tic_point point = Real_mouse(tic);

    api_record("mouse() -> (%d,%d)", point.x, point.y);

    return point;
}

static void trap_circ(tic_mem* tic, s32 x, s32 y, s32 radius, u8 color)
{
    Real_circ(tic, x, y, radius, color);
    api_record("circ(%d,%d,%d,%u)", x, y, radius, (unsigned)color);
}

static void trap_circb(tic_mem* tic, s32 x, s32 y, s32 radius, u8 color)
{
    Real_circb(tic, x, y, radius, color);
    api_record("circb(%d,%d,%d,%u)", x, y, radius, (unsigned)color);
}

static void trap_elli(tic_mem* tic, s32 x, s32 y, s32 a, s32 b, u8 color)
{
    Real_elli(tic, x, y, a, b, color);
    api_record("elli(%d,%d,%d,%d,%u)", x, y, a, b, (unsigned)color);
}

static void trap_ellib(tic_mem* tic, s32 x, s32 y, s32 a, s32 b, u8 color)
{
    Real_ellib(tic, x, y, a, b, color);
    api_record("ellib(%d,%d,%d,%d,%u)", x, y, a, b, (unsigned)color);
}

static void trap_paint(tic_mem* tic, s32 x, s32 y, u8 color, u8 bordercolor)
{
    Real_paint(tic, x, y, color, bordercolor);
    api_record("paint(%d,%d,%u,%u)", x, y, (unsigned)color, (unsigned)bordercolor);
}

static void trap_tri(tic_mem* tic, float x1, float y1, float x2, float y2, float x3, float y3, u8 color)
{
    Real_tri(tic, x1, y1, x2, y2, x3, y3, color);
    api_record("tri(%g,%g,%g,%g,%g,%g,%u)", x1, y1, x2, y2, x3, y3, (unsigned)color);
}

static void trap_trib(tic_mem* tic, float x1, float y1, float x2, float y2, float x3, float y3, u8 color)
{
    Real_trib(tic, x1, y1, x2, y2, x3, y3, color);
    api_record("trib(%g,%g,%g,%g,%g,%g,%u)", x1, y1, x2, y2, x3, y3, (unsigned)color);
}

static void trap_ttri(tic_mem* tic, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float u3, float v3, tic_texture_src texsrc, u8* colors, s32 count, float z1, float z2, float z3, bool depth)
{
    Real_ttri(tic, x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, texsrc, colors, count, z1, z2, z3, depth);

    api_record("ttri(%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%d,%u:[...],%d,%g,%g,%g,%s)",
        x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3,
        (int)texsrc, (unsigned)count, count, z1, z2, z3, b2s(depth));
}

static void trap_clip(tic_mem* tic, s32 x, s32 y, s32 width, s32 height)
{
    Real_clip(tic, x, y, width, height);
    api_record("clip(%d,%d,%d,%d)", x, y, width, height);
}

static void trap_music(tic_mem* tic, s32 track, s32 frame, s32 row, bool loop, bool sustain, s32 tempo, s32 speed)
{
    Real_music(tic, track, frame, row, loop, sustain, tempo, speed);
    api_record("music(%d,%d,%d,%s,%s,%d,%d)", track, frame, row, b2s(loop), b2s(sustain), tempo, speed);
}

static void trap_sync(tic_mem* tic, u32 mask, s32 bank, bool toCart)
{
    Real_sync(tic, mask, bank, toCart);
    api_record("sync(%u,%d,%s)", mask, bank, b2s(toCart));
}

static s32 trap_vbank(tic_mem* tic, s32 bank)
{
    s32 result = Real_vbank(tic, bank);

    api_record("vbank(%d) -> %d", bank, result);

    return result;
}

static void trap_reset(tic_mem* tic)
{
    Real_reset(tic);
    api_record("reset()");
}

static bool trap_key(tic_mem* tic, tic_key key)
{
    bool down = Real_key(tic, key);

    api_record("key(%d) -> %s", (int)key, b2s(down));

    return down;
}

static bool trap_keyp(tic_mem* tic, tic_key key, s32 hold, s32 period)
{
    bool down = Real_keyp(tic, key, hold, period);

    api_record("keyp(%d,%d,%d) -> %s", (int)key, hold, period, b2s(down));

    return down;
}

static bool trap_fget(tic_mem* tic, s32 index, u8 flag)
{
    bool value = Real_fget(tic, index, flag);

    api_record("fget(%d,%u) -> %s", index, (unsigned)flag, b2s(value));

    return value;
}

static void trap_fset(tic_mem* tic, s32 index, u8 flag, bool value)
{
    Real_fset(tic, index, flag, value);
    api_record("fset(%d,%u,%s)", index, (unsigned)flag, b2s(value));
}

static double trap_fft(tic_mem* tic, s32 startFreq, s32 endFreq)
{
    double value = Real_fft(tic, startFreq, endFreq);

    api_record("fft(%d,%d) -> %g", startFreq, endFreq, value);

    return value;
}

static double trap_ffts(tic_mem* tic, s32 startFreq, s32 endFreq)
{
    double value = Real_ffts(tic, startFreq, endFreq);

    api_record("ffts(%d,%d) -> %g", startFreq, endFreq, value);

    return value;
}

static double trap_vqt(tic_mem* tic, s32 bin)
{
    double value = Real_vqt(tic, bin);

    api_record("vqt(%d) -> %g", bin, value);

    return value;
}

static double trap_vqts(tic_mem* tic, s32 bin)
{
    double value = Real_vqts(tic, bin);

    api_record("vqts(%d) -> %g", bin, value);

    return value;
}

static double trap_vqtr(tic_mem* tic, s32 bin)
{
    double value = Real_vqtr(tic, bin);

    api_record("vqtr(%d) -> %g", bin, value);

    return value;
}

static double trap_vqtrs(tic_mem* tic, s32 bin)
{
    double value = Real_vqtrs(tic, bin);

    api_record("vqtrs(%d) -> %g", bin, value);

    return value;
}

static double trap_vqtw(tic_mem* tic, s32 bin)
{
    double value = Real_vqtw(tic, bin);

    api_record("vqtw(%d) -> %g", bin, value);

    return value;
}

static double trap_vqtsw(tic_mem* tic, s32 bin)
{
    double value = Real_vqtsw(tic, bin);

    api_record("vqtsw(%d) -> %g", bin, value);

    return value;
}

static double trap_vqtrw(tic_mem* tic, s32 bin)
{
    double value = Real_vqtrw(tic, bin);

    api_record("vqtrw(%d) -> %g", bin, value);

    return value;
}

static double trap_vqtrsw(tic_mem* tic, s32 bin)
{
    double value = Real_vqtrsw(tic, bin);

    api_record("vqtrsw(%d) -> %g", bin, value);

    return value;
}

static double trap_fftr(tic_mem* tic, s32 startFreq, s32 endFreq)
{
    double value = Real_fftr(tic, startFreq, endFreq);

    api_record("fftr(%d,%d) -> %g", startFreq, endFreq, value);

    return value;
}

static double trap_fftrs(tic_mem* tic, s32 startFreq, s32 endFreq)
{
    double value = Real_fftrs(tic, startFreq, endFreq);

    api_record("fftrs(%d,%d) -> %g", startFreq, endFreq, value);

    return value;
}

#if defined BUILD_DEPRECATED
static void trap_textri(tic_mem* tic, float x1, float y1, float x2, float y2, float x3, float y3,
    float u1, float v1, float u2, float v2, float u3, float v3, bool use_map, u8* colors, s32 count)
{
    Real_textri(tic, x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, use_map, colors, count);

    api_record("textri(%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%s,%s)",
        x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, b2s(use_map), arrayText(colors, count));
}
#endif

void traps_install(tic_core* core)
{
#define API_FUNC_DEF(name, ...)         \
    Real_##name = core->api.name;       \
    core->api.name = trap_##name;

    TIC_API_LIST(API_FUNC_DEF)

#undef API_FUNC_DEF

#if defined BUILD_DEPRECATED
    Real_textri = core->api.textri;
    core->api.textri = trap_textri;
#endif
}
