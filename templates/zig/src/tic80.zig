const std = @import("std");
// comptime std.testing.refAllDecls;

// comptime {
//   inline for(std.meta.declarations(@This())) |decl| {
//     _ = decl;
//   }
// }

// types

const Coord_i8 = extern struct {
  x: i8,
  y: i8,
};

const RGBColor = extern struct {
    r: u8,
    g: u8,
    b: u8,
};

// ------------------------
// HARDWARE REGISTERS / RAM

pub const WIDTH: u32 = 240;
pub const HEIGHT: u32 = 136;

pub const FRAMEBUFFER: *allowzero volatile [16320]u8 = @intToPtr(*allowzero volatile [16320]u8, 0);
pub const TILES : *[8192]u8 = @intToPtr(*[8192]u8, 0x4000);
pub const SPRITES : *[8192]u8 = @intToPtr(*[8192]u8, 0x6000);
pub const MAP : *[32640]u8 = @intToPtr(*[32640]u8, 0x8000);
pub const GAMEPADS : *[4]u8 = @intToPtr(*[4]u8, 0xFF80);
pub const MOUSE : *[4]u8 = @intToPtr(*[4]u8, 0xFF84);
pub const KEYBOARD : *[4]u8 = @intToPtr(*[4]u8, 0xFF88);
pub const SFX_STATE: *[16]u8 = @intToPtr(*[16]u8, 0xFF8C);
pub const SOUND_REGISTERS: *[72]u8 = @intToPtr(*[72]u8, 0xFF9C);
pub const WAVEFORMS: *[256]u8 =@intToPtr(*[256]u8, 0xFFE4);
pub const SFX: *[4224]u8 =@intToPtr(*[4224]u8, 0x100E4);
pub const MUSIC_PATTERNS: *[11520]u8 =@intToPtr(*[11520]u8, 0x11164);
pub const MUSIC_TRACKS: *[408]u8 =@intToPtr(*[408]u8, 0x13E64);
pub const SOUND_STATE: *[4]u8 =@intToPtr(*[4]u8, 0x13FFC);
pub const STEREO_VOLUME: *[4]u8 =@intToPtr(*[4]u8, 0x14000);
pub const PERSISTENT_RAM: *[1024]u8 =@intToPtr(*[1024]u8, 0x14004);
pub const PERSISTENT_RAM_u32: *[256]u32 =@intToPtr(*[256]u32, 0x14004);
pub const SPRITE_FLAGS: *[512]u8 =@intToPtr(*[512]u8, 0x14404);
pub const SYSTEM_FONT: *[2048]u8 =@intToPtr(*[2048]u8, 0x14604);

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

pub const PALETTE: *[16]RGBColor = @intToPtr(*[16]RGBColor, 0x3FC0);
pub const PALETTE_u8: *[48]u8 = @intToPtr(*[48]u8, 0x3FC0);
pub const PALETTE_MAP: *PaletteMap = @intToPtr(*PaletteMap, 0x3FF0);
pub const PALETTE_MAP_u8: *[8]u8 = @intToPtr(*[8]u8, 0x3FF0);
// TODO: this doesn't work unless it's packed, which I dno't think we can do?
// pub const PALETTE_MAP: *[16]u4 = @intToPtr(*[16]u4, 0x3FF0);
pub const BORDER_COLOR: *u8 = @intToPtr(*u8, 0x3FF8);
pub const SCREEN_OFFSET: *Coord_i8 = @intToPtr(*Coord_i8, 0x3FF9);
pub const MOUSE_CURSOR: *u8 = @intToPtr(*u8, 0x3FFB);
pub const BLIT_SEGMENT: *u8 = @intToPtr(*u8, 0x3FFC);

// vbank 1
pub const OVR_TRANSPARENCY: *u8 = @intToPtr(*u8, 0x3FF8);


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

pub extern fn btn(id: i32) i32;
pub extern fn btnp(id: i32, hold: i32, period: i32 ) bool;
pub extern fn key(keycode: i32) bool;
pub extern fn keyp(keycode: i32, hold: i32, period: i32 ) bool;
// mouse -> x y left middle right scrollx scrolly
pub extern fn mouse(data: *MouseData) void;


// -----------------
// DRAW / DRAW UTILS

pub extern fn clip(x: i32, y: i32, w: i32, h: i32) void;
pub extern fn cls(color: i32) void; // from WASM
pub extern fn circ(x: i32, y: i32, radius: i32, color: i32) void;
pub extern fn circb(x: i32, y: i32, radius: i32, color: i32) void;
pub extern fn elli(x: i32, y: i32, a: i32, b: i32, color: i32) void;
pub extern fn ellib(x: i32, y: i32, a: i32, b: i32, color: i32) void;
pub extern fn fget(id: i32, flag: u8) bool;
pub extern fn fset(id: i32, flag: u8, value: bool) bool;
pub extern fn line(x0: i32, y0: i32, x1: i32, y1: i32, color: i32) void;
// map [x=0 y=0] [w=30 h=17] [sx=0 sy=0] [colorkey=-1] [scale=1] [remap=nil]
// TODO: remap should be what????
pub extern fn map(x: i32, y: i32, w: i32, h: i32, sx: i32, sy: i32, trans_colors: ?[*]u8, colorCount: i32, scale: i32, remap: i32) void;
pub extern fn mget(x: i32, y:i32) i32;
pub extern fn mset(x: i32, y:i32, value: bool) void;
pub extern fn pix(x: i32, y:i32, color: i32) void;
pub extern fn rect(x: i32, y: i32, w: i32, h:i32, color: i32) void;
pub extern fn rectb(x: i32, y: i32, w: i32, h:i32, color: i32) void;
pub extern fn spr(id: i32, x: i32, y: i32, trans_colors: [*]u8, colorCount: i32, scale: i32, flip: i32, rotate: i32, w: i32, h: i32) void;
pub extern fn tri(x1: i32, y1: i32, x2: i32, y2: i32, x3: i32, y3: i32, color: i32) void;
pub extern fn trib(x1: i32, y1: i32, x2: i32, y2: i32, x3: i32, y3: i32, color: i32) void;
// textri x1 y1 x2 y2 x3 y3 u1 v1 u2 v2 u3 v3 [use_map=false] [trans=-1]
pub extern fn textri(x1: i32, y1: i32, x2: i32, y2: i32, x3: i32, y3: i32, u1: i32, v1: i32, u2: i32, v2: i32, u3: i32, v3: i32, use_map: bool, trans_colors: ?[*]u8, colorCount: i32) void;


// ----
// TEXT

// print text [x=0 y=0] [color=15] [fixed=false] [scale=1] [smallfont=false] -> text width
pub extern fn print(text: [*:0]const u8, x: i32, y: i32, color: i32, fixed: bool, scale: i32, smallfont: bool) i32;
//  font text, x, y, [transcolor], [char width], [char height], [fixed=false], [scale=1] -> text width
pub extern fn font(text: [*:0]const u8, x: u32, y: i32, transcolor: i32, char_width: i32, char_height: i32, fixed: bool, scale: i32) i32;


// -----
// AUDIO

pub extern fn music(track: i32, frame: i32, row: i32, loop: bool, sustain: bool, tempo: i32, speed: i32) void;
// sfx id [note] [duration=-1] [channel=0] [volume=15] [speed=0]
pub extern fn sfx(id: i32, note: i32, duration: i32, channel: i32, volume: i32, speed: i32) void;


// ------
// MEMORY

// pmem index -1 -> val
// pmem index val -> val
pub extern fn pmem(index: u32, value: i64) u32;
pub extern fn memcpy(to: u32, from: u32, length: u32) void;
pub extern fn memset(addr: u32, value: u8, length: u32) void;

pub extern fn poke(addr: u32, value: u8, bits: i32) void;
pub extern fn poke4(addr4: u32, value: u8) void;
pub extern fn poke2(addr2: u32, value: u8) void;
pub extern fn poke1(bitaddr: u32, value: u8) void;

pub extern fn peek(addr: u32, bits: i32) u8;
pub extern fn peek4(addr4: u32) u8;
pub extern fn peek2(addr2: u32) u8;
pub extern fn peek1(bitaddr: u32) u8;
pub extern fn vbank(bank: i32) u8;


// -----
// UTILS

// time -> ticks (ms) elapsed since game start
pub extern fn time() f32;
pub extern fn tstamp() u64;
pub extern fn sync(mask: i32, bank: i32, tocart: bool) void;
pub extern fn trace(text: [*]u8, color: i32) void;

// ------
// SYSTEM
pub extern fn exit() void;
pub extern fn reset() void;