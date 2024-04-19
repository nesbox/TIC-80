// ****************************************************************************
// ****************************************************************************
//
//        DO NOT MODIFY THIS FILE.  THIS IS A COPY, NO THE ORIGINAL.
//
// ****************************************************************************
// ****************************************************************************
//
// It is merely a copy of `./templates/zig/src/tic80.zig`
//
// Modify that file then run `./tools/zig_sync` to update all libraries
//
// (yes even the original has this exact comment, be careful)

const std = @import("std");
comptime {
    std.testing.refAllDecls(@This());
}

// types

const MAX_STRING_SIZE = 200;
const Offset_i8 = extern struct {
    x: i8,
    y: i8,
};

const RGBColor = extern struct {
    r: u8,
    g: u8,
    b: u8,
};

const TextureSource = enum(i32) { TILES = 0, MAP, VBANK1 };

// ------------------------
// HARDWARE REGISTERS / RAM

pub const WIDTH: u32 = 240;
pub const HEIGHT: u32 = 136;

pub const FRAMEBUFFER: *allowzero volatile [16320]u8 = @as(*allowzero volatile [16320]u8, @ptrFromInt(0));
pub const TILES: *[8192]u8 = @as(*[8192]u8, @ptrFromInt(0x4000));
pub const SPRITES: *[8192]u8 = @as(*[8192]u8, @ptrFromInt(0x6000));
pub const MAP: *[32640]u8 = @as(*[32640]u8, @ptrFromInt(0x8000));
pub const GAMEPADS: *[4]u8 = @as(*[4]u8, @ptrFromInt(0xFF80));
pub const MOUSE: *[4]u8 = @as(*[4]u8, @ptrFromInt(0xFF84));
pub const KEYBOARD: *[4]u8 = @as(*[4]u8, @ptrFromInt(0xFF88));
pub const SFX_STATE: *[16]u8 = @as(*[16]u8, @ptrFromInt(0xFF8C));
pub const SOUND_REGISTERS: *[72]u8 = @as(*[72]u8, @ptrFromInt(0xFF9C));
pub const WAVEFORMS: *[256]u8 = @as(*[256]u8, @ptrFromInt(0xFFE4));
pub const SFX: *[4224]u8 = @as(*[4224]u8, @ptrFromInt(0x100E4));
pub const MUSIC_PATTERNS: *[11520]u8 = @as(*[11520]u8, @ptrFromInt(0x11164));
pub const MUSIC_TRACKS: *[408]u8 = @as(*[408]u8, @ptrFromInt(0x13E64));
pub const SOUND_STATE: *[4]u8 = @as(*[4]u8, @ptrFromInt(0x13FFC));
pub const STEREO_VOLUME: *[4]u8 = @as(*[4]u8, @ptrFromInt(0x14000));
pub const PERSISTENT_RAM: *[1024]u8 = @as(*[1024]u8, @ptrFromInt(0x14004));
pub const PERSISTENT_RAM_u32: *[256]u32 = @as(*[256]u32, @ptrFromInt(0x14004));
pub const SPRITE_FLAGS: *[512]u8 = @as(*[512]u8, @ptrFromInt(0x14404));
pub const SYSTEM_FONT: *[2048]u8 = @as(*[2048]u8, @ptrFromInt(0x14604));

// vbank 0

const PaletteMap = packed struct {
    color0: u4,
    color1: u4,
    color2: u4,
    color3: u4,
    color4: u4,
    color5: u4,
    color6: u4,
    color7: u4,
    color8: u4,
    color9: u4,
    color10: u4,
    color11: u4,
    color12: u4,
    color13: u4,
    color14: u4,
    color15: u4,
};

pub const PALETTE: *[16]RGBColor = @as(*[16]RGBColor, @ptrFromInt(0x3FC0));
pub const PALETTE_u8: *[48]u8 = @as(*[48]u8, @ptrFromInt(0x3FC0));
pub const PALETTE_MAP: *PaletteMap = @as(*PaletteMap, @ptrFromInt(0x3FF0));
pub const PALETTE_MAP_u8: *[8]u8 = @as(*[8]u8, @ptrFromInt(0x3FF0));
// TODO: this doesn't work unless it's packed, which I dno't think we can do?
// pub const PALETTE_MAP: *[16]u4 = @ptrFromInt(*[16]u4, 0x3FF0);
pub const BORDER_COLOR: *u8 = @as(*u8, @ptrFromInt(0x3FF8));
pub const SCREEN_OFFSET: *Offset_i8 = @as(*Offset_i8, @ptrFromInt(0x3FF9));
pub const MOUSE_CURSOR: *u8 = @as(*u8, @ptrFromInt(0x3FFB));
pub const BLIT_SEGMENT: *u8 = @as(*u8, @ptrFromInt(0x3FFC));

// vbank 1
pub const OVR_TRANSPARENCY: *u8 = @as(*u8, @ptrFromInt(0x3FF8));

// import the RAW api

pub const raw = struct {
    pub const Flip = enum(u32) {
        no = 0,
        horizontal = 1,
        vertical = 2,
        both = 3,
    };
    pub const Rotate = enum(u32) {
        no = 0,
        by90 = 1,
        by180 = 2,
        by270 = 3,
    };
    pub extern fn btn(id: i32) i32;
    pub extern fn btnp(id: i32, hold: i32, period: i32) bool;
    pub extern fn clip(x: i32, y: i32, w: i32, h: i32) void;
    pub extern fn cls(color: i32) void;
    pub extern fn circ(x: i32, y: i32, radius: i32, color: i32) void;
    pub extern fn circb(x: i32, y: i32, radius: i32, color: i32) void;
    pub extern fn exit() void;
    pub extern fn elli(x: i32, y: i32, a: i32, b: i32, color: i32) void;
    pub extern fn ellib(x: i32, y: i32, a: i32, b: i32, color: i32) void;
    pub extern fn fget(id: i32, flag: u8) bool;
    pub extern fn font(text: [*:0]u8, x: u32, y: i32, trans_colors: ?[*]const u8, color_count: i32, char_width: i32, char_height: i32, fixed: bool, scale: i32, alt: bool) i32;
    pub extern fn fset(id: i32, flag: u8, value: bool) bool;
    pub extern fn key(keycode: i32) bool;
    pub extern fn keyp(keycode: i32, hold: i32, period: i32) bool;
    pub extern fn line(x0: i32, y0: i32, x1: i32, y1: i32, color: i32) void;
    // pass struct by pointer SHOULD:tm JUST WORK
    pub extern fn map(x: i32, y: i32, w: i32, h: i32, sx: i32, sy: i32, trans_colors: ?[*]const u8, color_count: i32, scale: i32, remap: ?*const RemapArgs) void;
    pub extern fn memcpy(to: u32, from: u32, length: u32) void;
    pub extern fn memset(addr: u32, value: u8, length: u32) void;
    pub extern fn mget(x: i32, y: i32) i32;
    pub extern fn mouse(data: *MouseData) void;
    pub extern fn mset(x: i32, y: i32, tile_id: u32) void;
    pub extern fn music(track: i32, frame: i32, row: i32, loop: bool, sustain: bool, tempo: i32, speed: i32) void;
    pub extern fn peek(addr: u32, bits: i32) u8;
    pub extern fn peek4(addr4: u32) u8;
    pub extern fn peek2(addr2: u32) u8;
    pub extern fn peek1(bitaddr: u32) u8;
    pub extern fn pix(x: i32, y: i32, color: i32) void;
    pub extern fn pmem(index: u32, value: i64) u32;
    pub extern fn poke(addr: u32, value: u8, bits: i32) void;
    pub extern fn poke4(addr4: u32, value: u8) void;
    pub extern fn poke2(addr2: u32, value: u8) void;
    pub extern fn poke1(bitaddr: u32, value: u8) void;
    pub extern fn print(text: [*:0]const u8, x: i32, y: i32, color: i32, fixed: bool, scale: i32, smallfont: bool) i32;
    pub extern fn rect(x: i32, y: i32, w: i32, h: i32, color: i32) void;
    pub extern fn rectb(x: i32, y: i32, w: i32, h: i32, color: i32) void;
    pub extern fn reset() void;
    pub extern fn sfx(id: i32, note: i32, octave: i32, duration: i32, channel: i32, volumeLeft: i32, volumeRight: i32, speed: i32) void;
    pub extern fn spr(id: i32, x: i32, y: i32, trans_colors: ?[*]const u8, color_count: i32, scale: i32, flip: i32, rotate: i32, w: i32, h: i32) void;
    pub extern fn sync(mask: i32, bank: i32, tocart: bool) void;
    pub extern fn ttri(x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32, u1: f32, v1: f32, u2: f32, v2: f32, u3: f32, v3: f32, texture_source: i32, trans_colors: ?[*]const u8, color_count: i32, z1: f32, z2: f32, z3: f32, depth: bool) void;
    pub extern fn tri(x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32, color: i32) void;
    pub extern fn trib(x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32, color: i32) void;
    pub extern fn time() f32;
    pub extern fn trace(text: [*:0]const u8, color: i32) void;
    pub extern fn tstamp() u64;
    pub extern fn vbank(bank: i32) u8;
};

// -----
// INPUT

const MouseData = extern struct {
    x: i16,
    y: i16,
    scrollx: i8,
    scrolly: i8,
    left: bool,
    middle: bool,
    right: bool,
};

pub const key = raw.key;
// TODO: nicer API?
pub const keyp = raw.keyp;
pub const held = raw.btnp;

pub fn pressed(id: i32) bool {
    return raw.btnp(id, -1, -1);
}

pub fn btn(id: i32) bool {
    return raw.btn(id) != 0;
}

pub fn anybtn() bool {
    return raw.btn(-1) != 0;
}

// -----------------
// DRAW / DRAW UTILS

pub const clip = raw.clip;
pub fn noclip() void {
    raw.clip(0, 0, WIDTH, HEIGHT);
}
pub const cls = raw.cls;
pub const circ = raw.circ;
pub const circb = raw.circb;
pub const elli = raw.elli;
pub const ellib = raw.ellib;
pub const fget = raw.fget;
pub const fset = raw.fset;
pub const line = raw.line;
pub const mset = raw.mset;
pub const mget = raw.mget;
pub const mouse = raw.mouse;

// map [x=0 y=0] [w=30 h=17] [sx=0 sy=0] [colorkey=-1] [scale=1] [remap=nil]
// TODO: remap should be what????
// pub extern fn map(x: i32, y: i32, w: i32, h: i32, sx: i32, sy: i32, trans_colors: ?[*]u8, color_count: i32, scale: i32, remap: i32) void;

const RemapArgs = extern struct { remap: *const fn (?*anyopaque, i32, i32, *RemapInfo) callconv(.C) void, data: ?*anyopaque, res_ptr: *RemapInfo };
pub const RemapInfo = extern struct {
    index: u8,
    flip: raw.Flip,
    rotate: raw.Rotate,
};
const MapArgs = struct {
    x: i32 = 0,
    y: i32 = 0,
    w: i32 = 30,
    h: i32 = 17,
    sx: i32 = 0,
    sy: i32 = 0,
    transparent: []const u8 = &.{},
    scale: u8 = 1,
    remap: ?*const fn (i32, i32, *RemapInfo) void = null, // TODO
};

fn remap_wrapper(data: ?*anyopaque, x: i32, y: i32, info: *RemapInfo) callconv(.C) void {
    const fun: *const fn (i32, i32, *RemapInfo) void = @ptrCast(data orelse return);
    fun(x, y, info);
}
pub fn map(args: MapArgs) void {
    const color_count = @as(u8, @intCast(args.transparent.len));
    const colors = args.transparent.ptr;
    std.debug.assert(color_count < 16);
    // why?
    var remapinfo = .{ .index = undefined, .flip = undefined, .rotate = undefined };
    const remap_args: ?RemapArgs = if (args.remap) |it| .{ .remap = &remap_wrapper, .data = @ptrCast(@constCast(it)), .res_ptr = &remapinfo } else null;
    raw.map(args.x, args.y, args.w, args.h, args.sx, args.sy, colors, color_count, args.scale, if (remap_args) |it| &it else null);
}

pub fn pix(x: i32, y: i32, color: u8) void {
    raw.pix(x, y, color);
}

pub fn getpix(x: i32, y: i32) u8 {
    raw.pix(x, y, -1);
}

// pub extern fn spr(id: i32, x: i32, y: i32, trans_colors: [*]u8, color_count: i32, scale: i32, flip: i32, rotate: i32, w: i32, h: i32) void;

pub const Flip = enum(u2) {
    no = 0,
    horizontal = 1,
    vertical = 2,
    both = 3,
};

pub const Rotate = enum(u2) {
    no = 0,
    by90 = 1,
    by180 = 2,
    by270 = 3,
};

const SpriteArgs = struct {
    w: i32 = 1,
    h: i32 = 1,
    transparent: []const u8 = &.{},
    scale: u8 = 1,
    flip: Flip = Flip.no,
    rotate: Rotate = Rotate.no,
};

pub fn spr(id: i32, x: i32, y: i32, args: SpriteArgs) void {
    const color_count = @as(u8, @intCast(args.transparent.len));
    const colors = args.transparent.ptr;
    std.debug.assert(color_count < 16);
    raw.spr(id, x, y, colors, color_count, args.scale, @intFromEnum(args.flip), @intFromEnum(args.rotate), args.w, args.h);
}

pub const rect = raw.rect;
pub const rectb = raw.rectb;
pub const tri = raw.tri;
pub const trib = raw.trib;

const TextriArgs = struct {
    texture_source: TextureSource = TextureSource.TILES,
    transparent: []const u8 = &.{},
    z1: f32 = 0,
    z2: f32 = 0,
    z3: f32 = 0,
    depth: bool = false,
};

pub fn ttri(x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32, @"u1": f32, v1: f32, @"u2": f32, v2: f32, @"u3": f32, v3: f32, args: TextriArgs) void {
    const color_count = @as(u8, @intCast(args.transparent.len));
    const trans_colors = args.transparent.ptr;
    raw.ttri(x1, y1, x2, y2, x3, y3, @"u1", v1, @"u2", v2, @"u3", v3, @intFromEnum(args.texture_source), trans_colors, color_count, args.z1, args.z2, args.z3, args.depth);
}

// ----
// TEXT

const PrintArgs = struct {
    color: u8 = 15,
    fixed: bool = false,
    small_font: bool = false,
    scale: u8 = 1,
};

const FontArgs = struct { transparent: []const u8 = &.{}, char_width: u8, char_height: u8, fixed: bool = false, scale: u8 = 1, alt: bool = false };

/// Prints the text and returns the width of the text in pixels
pub fn print(text: []const u8, x: i32, y: i32, args: PrintArgs) i32 {
    const as_ptr: [*:0]const u8 = @as([*:0]const u8, @ptrCast(text));
    return raw.print(as_ptr, x, y, args.color, args.fixed, args.scale, args.small_font);
}

/// Prints the text using format and returns the width of the text in pixels
pub fn printf(comptime fmt: []const u8, fmtargs: anytype, x: i32, y: i32, args: PrintArgs) i32 {
    var buff: [MAX_STRING_SIZE:0]u8 = undefined;
    _ = std.fmt.bufPrintZ(&buff, fmt, fmtargs) catch unreachable;
    return raw.print(&buff, x, y, args.color, args.fixed, args.scale, args.small_font);
}

pub fn font(text: []const u8, x: u32, y: i32, args: FontArgs) i32 {
    const color_count = @as(u8, @intCast(args.transparent.len));
    const colors = args.transparent.ptr;

    const as_ptr: [*:0]const u8 = @as([*:0]const u8, @ptrCast(text));
    return raw.font(as_ptr, x, y, colors, color_count, args.char_width, args.char_height, args.fixed, args.scale, args.alt);
}

// -----
// AUDIO

// music [track=-1] [frame=-1] [row=-1] [loop=true] [sustain=false] [tempo=-1] [speed=-1]
const MusicArgs = struct {
    frame: i8 = -1,
    row: i8 = -1,
    loop: bool = true,
    sustain: bool = false,
    tempo: i8 = -1,
    speed: i8 = -1,
};

pub fn music(track: i32, args: MusicArgs) void {
    raw.music(track, args.frame, args.row, args.loop, args.sustain, args.tempo, args.speed);
}

pub fn nomusic() void {
    music(-1, .{});
}

// sfx id [note] [duration=-1] [channel=0] [volume=15] [speed=0]
// void tic_api_sfx(tic_mem* memory, s32 index, s32 note, s32 octave, s32 duration, s32 channel, s32 left, s32 right, s32 speed)
const MAX_VOLUME: i8 = 15;
const SfxArgs = struct {
    sfx: i8 = -1,
    note: i8 = -1,
    octave: i8 = -1,
    duration: i8 = -1,
    channel: i8 = 0,
    volume: i8 = 15,
    volumeLeft: i8 = 15,
    volumeRight: i8 = 15,
    speed: i8 = 0,
};

// pub extern fn sfx(id: i32, note: i32, duration: i32, channel: i32, volume: i32, speed: i32) void;

const SFX_NOTES = [_][]const u8{ "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };

fn parse_note(name: []const u8, noteArgs: *SfxArgs) bool {
    var note_signature: []const u8 = undefined;
    var octave: u8 = undefined;
    if (name.len == 2) {
        note_signature = &.{ name[0], '-' };
        octave = name[1];
    } else {
        note_signature = name[0..2];
        octave = name[2];
    }

    var i: i8 = 0;
    for (SFX_NOTES) |music_note| {
        if (std.mem.eql(u8, music_note, note_signature)) {
            noteArgs.note = i;
            noteArgs.octave = @as(i8, @intCast(octave - '1'));
            return true;
        }
        i += 1;
    }
    return false;
}

pub fn note(name: []const u8, args: SfxArgs) void {
    var largs: SfxArgs = args;
    if (parse_note(name, &largs)) {
        sfx(args.sfx, largs);
    }
}

pub fn sfx(id: i32, args: SfxArgs) void {
    var largs = args;
    if (args.volume != MAX_VOLUME) {
        largs.volumeLeft = args.volume;
        largs.volumeRight = args.volume;
    }
    raw.sfx(id, args.note, args.octave, args.duration, args.channel, largs.volumeLeft, largs.volumeRight, args.speed);
}

pub fn nosfx() void {
    sfx(-1, .{});
}

// ------
// MEMORY

pub fn pmemset(index: u32, value: u3) void {
    raw.pmem(index, value);
}

pub fn pmemget(index: u32) u32 {
    return raw.pmem(index, -1);
}

pub const memcpy = raw.memcpy;
pub const memset = raw.memset;

pub fn poke(addr: u32, value: u8) void {
    raw.poke(addr, value, 8);
}

pub const poke4 = raw.poke4;
pub const poke2 = raw.poke2;
pub const poke1 = raw.poke1;

pub fn peek(addr: u32) u8 {
    return raw.peek(addr, 8);
}
pub const peek4 = raw.peek4;
pub const peek2 = raw.peek2;
pub const peek1 = raw.peek1;
pub const vbank = raw.vbank;

// SYSTEM

pub const exit = raw.exit;
pub const reset = raw.reset;
pub fn trace(text: []const u8) void {
    const as_ptr: [*:0]const u8 = @as([*:0]const u8, @ptrCast(text));
    raw.trace(as_ptr, 15);
}

pub fn tracef(comptime fmt: []const u8, fmtargs: anytype) void {
    var buf: [MAX_STRING_SIZE:0]u8 = undefined;
    _ = std.fmt.bufPrintZ(&buf, fmt, fmtargs) catch unreachable;
    trace(&buf);
}

const SectionFlags = packed struct {
    // tiles   = 1<<0 -- 1
    // sprites = 1<<1 -- 2
    // map     = 1<<2 -- 4
    // sfx     = 1<<3 -- 8
    // music   = 1<<4 -- 16
    // palette = 1<<5 -- 32
    // flags   = 1<<6 -- 64
    // screen  = 1<<7 -- 128 (as of 0.90)
    tiles: bool = false,
    sprites: bool = false,
    map: bool = false,
    sfx: bool = false,
    music: bool = false,
    palette: bool = false,
    flags: bool = false,
    screen: bool = false,
};
// const SectionFlagsMask = packed union {
//     sections: SectionFlags,
//     value: u8
// };
const SyncArgs = struct {
    sections: SectionFlags,
    bank: u8,
    toCartridge: bool = false,
    fromCartridge: bool = false,
};

pub fn sync(args: SyncArgs) void {
    // var mask = SectionFlagsMask { .sections = args.sections };
    raw.sync(@as(u8, @bitCast(args.sections)), args.bank, args.toCartridge);
}

// TIME KEEPING

pub const time = raw.time;
pub const tstamp = raw.tstamp;
