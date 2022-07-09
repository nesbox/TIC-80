// Because this isn't in a separate crate, we have to allow unused code to silence the warnings.
#![allow(dead_code, unused_macros)]

use std::ffi::CString;

pub use sys::MouseInput;

// Constants
pub const WIDTH: i32 = 240;
pub const HEIGHT: i32 = 136;

// TIC-80 RAM

pub const FRAMEBUFFER: *mut [u8; 16320] = 0x00000 as *mut [u8; 16320];
pub const TILES: *mut [u8; 8192] = 0x04000 as *mut [u8; 8192];
pub const SPRITES: *mut [u8; 8192] = 0x06000 as *mut [u8; 8192];
pub const MAP: *mut [u8; 32640] = 0x08000 as *mut [u8; 32640];
pub const GAMEPADS: *mut [u8; 4] = 0x0FF80 as *mut [u8; 4];
pub const MOUSE: *mut [u8; 4] = 0x0FF84 as *mut [u8; 4];
pub const KEYBOARD: *mut [u8; 4] = 0x0FF88 as *mut [u8; 4];
pub const SFX_STATE: *mut [u8; 16] = 0x0FF8C as *mut [u8; 16];
pub const SOUND_REGISTERS: *mut [u8; 72] = 0x0FF9C as *mut [u8; 72];
pub const WAVEFORMS: *mut [u8; 256] = 0x0FFE4 as *mut [u8; 256];
pub const SFX: *mut [u8; 4224] = 0x100E4 as *mut [u8; 4224];
pub const MUSIC_PATTERNS: *mut [u8; 11520] = 0x11164 as *mut [u8; 11520];
pub const MUSIC_TRACKS: *mut [u8; 408] = 0x13E64 as *mut [u8; 408];
pub const SOUND_STATE: *mut [u8; 4] = 0x13FFC as *mut [u8; 4];
pub const STEREO_VOLUME: *mut [u8; 4] = 0x14000 as *mut [u8; 4];
pub const PERSISTENT_RAM: *mut [u8; 1024] = 0x14004 as *mut [u8; 1024];
pub const SPRITE_FLAGS: *mut [u8; 512] = 0x14404 as *mut [u8; 512];
pub const SYSTEM_FONT: *mut [u8; 2048] = 0x14604 as *mut [u8; 2048];

// The functions in the sys module follow the signatures as given in wasm.c.
// The wrapper functions are designed to be similar to the usual TIC-80 api.
pub mod sys {
    #[derive(Default)]
    #[repr(C)]
    pub struct MouseInput {
        pub x: i16,
        pub y: i16,
        pub scroll_x: i8,
        pub scroll_y: i8,
        pub left: bool,
        pub middle: bool,
        pub right: bool,
    }

    extern "C" {
        pub fn btn(index: i32) -> i32;
        pub fn btnp(index: i32, hold: i32, period: i32) -> bool;
        pub fn clip(x: i32, y: i32, width: i32, height: i32);
        pub fn cls(color: u8);
        pub fn circ(x: i32, y: i32, radius: i32, color: u8);
        pub fn circb(x: i32, y: i32, radius: i32, color: u8);
        pub fn elli(x: i32, y: i32, a: i32, b: i32, color: u8);
        pub fn ellib(x: i32, y: i32, a: i32, b: i32, color: u8);
        pub fn exit();
        pub fn fget(sprite_index: i32, flag: i8) -> bool;
        pub fn fset(sprite_index: i32, flag: i8, value: bool);
        pub fn font(
            text: *const u8,
            x: i32,
            y: i32,
            trans_colors: *const u8,
            trans_count: i8,
            char_width: i8,
            char_height: i8,
            fixed: bool,
            scale: i32,
            alt: bool,
        ) -> i32;
        pub fn key(index: i32) -> bool;
        pub fn keyp(index: i32, hold: i32, period: i32) -> bool;
        pub fn line(x0: f32, y0: f32, x1: f32, y1: f32, color: u8);
        // `remap` is not yet implemented by the TIC-80 WASM runtime, so for now its type is a raw i32.
        pub fn map(
            x: i32,
            y: i32,
            w: i32,
            h: i32,
            sx: i32,
            sy: i32,
            trans_colors: *const u8,
            color_count: i8,
            scale: i8,
            remap: i32,
        );
        // These clash with rustc builtins, so they are reimplemented in the wrappers.
        // pub fn memcpy(dest: i32, src: i32, length: i32);
        // pub fn memset(address: i32, value: i32, length: i32);
        pub fn mget(x: i32, y: i32) -> i32;
        pub fn mset(x: i32, y: i32, value: i32);
        pub fn mouse(mouse: *mut MouseInput);
        pub fn music(
            track: i32,
            frame: i32,
            row: i32,
            repeat: bool,
            sustain: bool,
            tempo: i32,
            speed: i32,
        );
        pub fn pix(x: i32, y: i32, color: i8) -> u8;
        pub fn peek(address: i32, bits: u8) -> u8;
        pub fn peek4(address: i32) -> u8;
        pub fn peek2(address: i32) -> u8;
        pub fn peek1(address: i32) -> u8;
        pub fn pmem(address: i32, value: i64) -> i32;
        pub fn poke(address: i32, value: u8, bits: u8);
        pub fn poke4(address: i32, value: u8);
        pub fn poke2(address: i32, value: u8);
        pub fn poke1(address: i32, value: u8);
        pub fn print(
            text: *const u8,
            x: i32,
            y: i32,
            color: i32,
            fixed: bool,
            scale: i32,
            alt: bool,
        ) -> i32;
        pub fn rect(x: i32, y: i32, w: i32, h: i32, color: u8);
        pub fn rectb(x: i32, y: i32, w: i32, h: i32, color: u8);
        pub fn sfx(
            sfx_id: i32,
            note: i32,
            octave: i32,
            duration: i32,
            channel: i32,
            volume_left: i32,
            volume_right: i32,
            speed: i32,
        );
        pub fn spr(
            id: i32,
            x: i32,
            y: i32,
            trans_colors: *const u8,
            color_count: i8,
            scale: i32,
            flip: i32,
            rotate: i32,
            w: i32,
            h: i32,
        );
        pub fn sync(mask: i32, bank: u8, to_cart: bool);
        pub fn time() -> f32;
        pub fn tstamp() -> u32;
        pub fn trace(text: *const u8, color: u8);
        pub fn tri(x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32, color: u8);
        pub fn trib(x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32, color: u8);
        pub fn ttri(
            x1: f32,
            y1: f32,
            x2: f32,
            y2: f32,
            x3: f32,
            y3: f32,
            u1: f32,
            v1: f32,
            u2: f32,
            v2: f32,
            u3: f32,
            v3: f32,
            tex_src: i32,
            trans_colors: *const u8,
            color_count: i8,
            z1: f32,
            z2: f32,
            z3: f32,
            depth: bool,
        );
        pub fn vbank(bank: u8) -> u8;
    }
}

// Input

pub fn btn(index: i32) -> bool {
    unsafe { sys::btn(index) != 0 }
}

pub fn btn_bits() -> u32 {
    unsafe { sys::btn(-1) as u32 }
}

pub fn btnp(index: i32, hold: i32, period: i32) -> bool {
    unsafe { sys::btnp(index, hold, period) }
}

pub fn key(index: i32) -> bool {
    unsafe { sys::key(index) }
}

pub fn keyp(index: i32, hold: i32, period: i32) -> bool {
    unsafe { sys::keyp(index, hold, period) }
}

pub fn mouse() -> MouseInput {
    let mut input = MouseInput::default();
    unsafe {
        sys::mouse(&mut input as *mut _);
    }
    input
}

// Audio

pub struct MusicOptions {
    pub frame: i32,
    pub row: i32,
    pub repeat: bool,
    pub sustain: bool,
    pub tempo: i32,
    pub speed: i32,
}

impl Default for MusicOptions {
    fn default() -> Self {
        Self {
            frame: -1,
            row: -1,
            repeat: true,
            sustain: false,
            tempo: -1,
            speed: -1,
        }
    }
}

pub fn music(track: i32, opts: MusicOptions) {
    unsafe {
        sys::music(
            track,
            opts.frame,
            opts.row,
            opts.repeat,
            opts.sustain,
            opts.tempo,
            opts.speed,
        )
    }
}

pub struct SfxOptions {
    pub note: i32,
    pub octave: i32,
    pub duration: i32,
    pub channel: i32,
    pub volume_left: i32,
    pub volume_right: i32,
    pub speed: i32,
}

impl Default for SfxOptions {
    fn default() -> Self {
        Self {
            note: -1,
            octave: -1,
            duration: -1,
            channel: 0,
            volume_left: 15,
            volume_right: 15,
            speed: 0,
        }
    }
}

pub fn sfx(sfx_id: i32, opts: SfxOptions) {
    unsafe {
        sys::sfx(
            sfx_id,
            opts.note,
            opts.octave,
            opts.duration,
            opts.channel,
            opts.volume_left,
            opts.volume_right,
            opts.speed,
        )
    }
}

// Graphics

pub fn cls(color: u8) {
    unsafe { sys::cls(color) }
}

pub fn clip(x: i32, y: i32, width: i32, height: i32) {
    unsafe { sys::clip(x, y, width, height) }
}

pub fn circ(x: i32, y: i32, radius: i32, color: u8) {
    unsafe { sys::circ(x, y, radius, color) }
}

pub fn circb(x: i32, y: i32, radius: i32, color: u8) {
    unsafe { sys::circb(x, y, radius, color) }
}

pub fn elli(x: i32, y: i32, a: i32, b: i32, color: u8) {
    unsafe { sys::elli(x, y, a, b, color) }
}

pub fn ellib(x: i32, y: i32, a: i32, b: i32, color: u8) {
    unsafe { sys::ellib(x, y, a, b, color) }
}

pub fn line(x0: f32, y0: f32, x1: f32, y1: f32, color: u8) {
    unsafe { sys::line(x0, y0, x1, y1, color) }
}

pub fn pix(x: i32, y: i32, color: u8) {
    unsafe {
        sys::pix(x, y, color as i8);
    }
}

pub fn get_pix(x: i32, y: i32) -> u8 {
    unsafe { sys::pix(x, y, -1) }
}

pub fn rect(x: i32, y: i32, w: i32, h: i32, color: u8) {
    unsafe { sys::rect(x, y, w, h, color) }
}

pub fn rectb(x: i32, y: i32, w: i32, h: i32, color: u8) {
    unsafe { sys::rectb(x, y, w, h, color) }
}

pub fn tri(x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32, color: u8) {
    unsafe { sys::tri(x1, y1, x2, y2, x3, y3, color) }
}

pub fn trib(x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32, color: u8) {
    unsafe { sys::trib(x1, y1, x2, y2, x3, y3, color) }
}

pub enum TextureSource {
    Tiles,
    Map,
    VBank1,
}

pub struct TTriOptions<'a> {
    pub texture_src: TextureSource,
    pub transparent: &'a [u8],
    pub z1: f32,
    pub z2: f32,
    pub z3: f32,
    pub depth: bool,
}

impl Default for TTriOptions<'_> {
    fn default() -> Self {
        Self {
            texture_src: TextureSource::Tiles,
            transparent: &[],
            z1: 0.0,
            z2: 0.0,
            z3: 0.0,
            depth: false,
        }
    }
}

pub fn ttri(
    x1: f32,
    y1: f32,
    x2: f32,
    y2: f32,
    x3: f32,
    y3: f32,
    u1: f32,
    v1: f32,
    u2: f32,
    v2: f32,
    u3: f32,
    v3: f32,
    opts: TTriOptions,
) {
    unsafe {
        sys::ttri(
            x1,
            y1,
            x2,
            y2,
            x3,
            y3,
            u1,
            v1,
            u2,
            v2,
            u3,
            v3,
            opts.texture_src as i32,
            opts.transparent.as_ptr(),
            opts.transparent.len() as i8,
            opts.z1,
            opts.z2,
            opts.z3,
            opts.depth,
        )
    }
}

pub struct MapOptions<'a> {
    pub x: i32,
    pub y: i32,
    pub w: i32,
    pub h: i32,
    pub sx: i32,
    pub sy: i32,
    pub transparent: &'a [u8],
    pub scale: i8,
}

impl Default for MapOptions<'_> {
    fn default() -> Self {
        Self {
            x: 0,
            y: 0,
            w: 30,
            h: 17,
            sx: 0,
            sy: 0,
            transparent: &[],
            scale: 1,
        }
    }
}

pub fn map(opts: MapOptions) {
    unsafe {
        sys::map(
            opts.x,
            opts.y,
            opts.w,
            opts.h,
            opts.sx,
            opts.sy,
            opts.transparent.as_ptr(),
            opts.transparent.len() as i8,
            opts.scale,
            0,
        )
    }
}

pub fn mget(x: i32, y: i32) -> i32 {
    unsafe { sys::mget(x, y) }
}

pub fn mset(x: i32, y: i32, value: i32) {
    unsafe { sys::mset(x, y, value) }
}

pub enum Flip {
    None,
    Horizontal,
    Vertical,
    Both,
}

pub enum Rotate {
    None,
    By90,
    By180,
    By270,
}

pub struct SpriteOptions<'a> {
    pub transparent: &'a [u8],
    pub scale: i32,
    pub flip: Flip,
    pub rotate: Rotate,
    pub w: i32,
    pub h: i32,
}

impl Default for SpriteOptions<'_> {
    fn default() -> Self {
        Self {
            transparent: &[],
            scale: 1,
            flip: Flip::None,
            rotate: Rotate::None,
            w: 1,
            h: 1,
        }
    }
}

pub fn spr(id: i32, x: i32, y: i32, opts: SpriteOptions) {
    unsafe {
        sys::spr(
            id,
            x,
            y,
            opts.transparent.as_ptr(),
            opts.transparent.len() as i8,
            opts.scale,
            opts.flip as i32,
            opts.rotate as i32,
            opts.w,
            opts.h,
        )
    }
}

pub fn fget(sprite_index: i32, flag: i8) -> bool {
    unsafe { sys::fget(sprite_index, flag) }
}

pub fn fset(sprite_index: i32, flag: i8, value: bool) {
    unsafe { sys::fset(sprite_index, flag, value) }
}

// Text Output
// The *_raw functions require a null terminated string reference.
// The *_alloc functions can handle any AsRef<str> type, but require the overhead of allocation.
// The macros will avoid the allocation if passed a string literal by adding the null terminator at compile time.

pub struct PrintOptions {
    color: i32,
    fixed: bool,
    scale: i32,
    small_font: bool,
}

impl Default for PrintOptions {
    fn default() -> Self {
        Self {
            color: 15,
            fixed: false,
            scale: 1,
            small_font: false,
        }
    }
}

pub fn print_raw(text: &str, x: i32, y: i32, opts: PrintOptions) -> i32 {
    unsafe {
        sys::print(
            text.as_ptr(),
            x,
            y,
            opts.color,
            opts.fixed,
            opts.scale,
            opts.small_font,
        )
    }
}

pub fn print_alloc(text: impl AsRef<str>, x: i32, y: i32, opts: PrintOptions) -> i32 {
    let text = CString::new(text.as_ref()).unwrap();
    unsafe {
        sys::print(
            text.as_ptr() as *const u8,
            x,
            y,
            opts.color,
            opts.fixed,
            opts.scale,
            opts.small_font,
        )
    }
}

// "use tic80::*" causes this to shadow std::print, but that isn't useful here anyway.
#[macro_export]
macro_rules! print {
    ($text: literal, $($args: expr), *) => {
        $crate::tic80::print_raw(concat!($text, "\0"), $($args), *);
    };
    ($text: expr, $($args: expr), *) => {
        $crate::tic80::print_alloc($text, $($args), *);
    };
}

pub struct FontOptions<'a> {
    transparent: &'a [u8],
    char_width: i8,
    char_height: i8,
    fixed: bool,
    scale: i32,
    alt_font: bool,
}

impl Default for FontOptions<'_> {
    fn default() -> Self {
        Self {
            transparent: &[],
            char_width: 8,
            char_height: 8,
            fixed: false,
            scale: 1,
            alt_font: false,
        }
    }
}

pub fn font_raw(text: &str, x: i32, y: i32, opts: FontOptions) -> i32 {
    unsafe {
        sys::font(
            text.as_ptr(),
            x,
            y,
            opts.transparent.as_ptr(),
            opts.transparent.len() as i8,
            opts.char_width,
            opts.char_height,
            opts.fixed,
            opts.scale,
            opts.alt_font,
        )
    }
}

pub fn font_alloc(text: impl AsRef<str>, x: i32, y: i32, opts: FontOptions) -> i32 {
    let text = CString::new(text.as_ref()).unwrap();
    unsafe {
        sys::font(
            text.as_ptr() as *const u8,
            x,
            y,
            opts.transparent.as_ptr(),
            opts.transparent.len() as i8,
            opts.char_width,
            opts.char_height,
            opts.fixed,
            opts.scale,
            opts.alt_font,
        )
    }
}

// Print a string, avoiding allocation if a literal is passed.
// NOTE: "use tic80::*" causes this to shadow std::print, but that isn't useful here anyway.
#[macro_export]
macro_rules! font {
    ($text: literal, $($args: expr), *) => {
        $crate::tic80::font_raw(concat!($text, "\0"), $($args), *);
    };
    ($text: expr, $($args: expr), *) => {
        $crate::tic80::font_alloc($text, $($args), *);
    };
}

pub fn trace_alloc(text: impl AsRef<str>, color: u8) {
    let text = CString::new(text.as_ref()).unwrap();
    unsafe { sys::trace(text.as_ptr() as *const u8, color) }
}

#[macro_export]
macro_rules! trace {
    ($text: literal, $color: expr) => {
        unsafe { crate::tic80::sys::trace(concat!($text, "\0").as_ptr(), $color) }
    };
    ($text: expr, $color: expr) => {
        crate::tic80::trace_alloc($text, $color);
    };
}

// Memory Access
// These functions (except pmem) are unsafe, because they can be used to access
// any of the WASM linear memory being used by the program.

pub unsafe fn memcpy(dest: i32, src: i32, length: usize) {
    core::ptr::copy(src as *const u8, dest as *mut u8, length)
}

pub unsafe fn memset(address: i32, value: u8, length: usize) {
    core::ptr::write_bytes(address as *mut u8, value, length)
}

pub unsafe fn peek(address: i32) -> u8 {
    sys::peek(address, 8)
}

pub unsafe fn peek4(address: i32) -> u8 {
    sys::peek4(address)
}

pub unsafe fn peek2(address: i32) -> u8 {
    sys::peek2(address)
}

pub unsafe fn peek1(address: i32) -> u8 {
    sys::peek1(address)
}

pub unsafe fn poke(address: i32, value: u8) {
    sys::poke(address, value, 8);
}

pub unsafe fn poke4(address: i32, value: u8) {
    sys::poke4(address, value);
}

pub unsafe fn poke2(address: i32, value: u8) {
    sys::poke2(address, value);
}

pub unsafe fn poke1(address: i32, value: u8) {
    sys::poke1(address, value);
}

pub unsafe fn sync(mask: i32, bank: u8, to_cart: bool) {
    sys::sync(mask, bank, to_cart);
}

pub unsafe fn vbank(bank: u8) {
    sys::vbank(bank);
}

pub fn pmem_set(address: i32, value: i32) {
    unsafe {
        sys::pmem(address, value as i64);
    }
}

pub fn pmem_get(address: i32) -> i32 {
    unsafe { sys::pmem(address, -1) }
}

// System Functions
pub fn exit() {
    unsafe { sys::exit() }
}

pub fn time() -> f32 {
    unsafe { sys::time() }
}

pub fn tstamp() -> u32 {
    unsafe { sys::tstamp() }
}
