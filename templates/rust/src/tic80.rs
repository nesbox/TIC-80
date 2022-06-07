// Because this isn't in a separate crate, we have to allow unused code to silence the warnings.
#![allow(dead_code)]

pub use sys::*;

mod sys {
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
        pub fn cls(color: i8);
        pub fn circ(x: i32, y: i32, radius: i32, color: i8);
        pub fn circb(x: i32, y: i32, radius: i32, color: i8);
        pub fn elli(x: i32, y: i32, a: i32, b: i32, color: i8);
        pub fn ellib(x: i32, y: i32, a: i32, b: i32, color: i8);
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
            scale: i8,
            alt: bool,
        ) -> i32;
        pub fn key(index: i32) -> i32;
        pub fn keyp(index: i32, hold: i32, period: i32) -> i32;
        pub fn line(x0: f32, y0: f32, x1: f32, y1: f32, color: i8);
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
        pub fn memcpy(dest: i32, src: i32, length: i32);
        pub fn memset(address: i32, value: i32, length: i32);
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
        pub fn pix(x: i32, y: i32, color: i8) -> i32;
        pub fn peek(address: i32, bits: i8) -> i8;
        pub fn peek4(address: i32) -> i8;
        pub fn peek2(address: i32) -> i8;
        pub fn peek1(address: i32) -> i8;
        pub fn pmem(address: i32, value: i64) -> i32;
        pub fn poke(address: i32, value: i8, bits: i8);
        pub fn poke4(address: i32, value: i8);
        pub fn poke2(address: i32, value: i8);
        pub fn poke1(address: i32, value: i8);
        pub fn print(
            text: *const u8,
            x: i32,
            y: i32,
            color: i32,
            fixed: bool,
            scale: i32,
            alt: bool,
        ) -> i32;
        pub fn rect(x: i32, y: i32, w: i32, h: i32, color: i8);
        pub fn rectb(x: i32, y: i32, w: i32, h: i32, color: i8);
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
        pub fn sync(mask: i32, bank: i8, to_cart: i8);
        pub fn time() -> f32;
        pub fn tstamp() -> u32;
        pub fn trace(text: *const u8, color: i32);
        pub fn tri(x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32, color: i8);
        pub fn trib(x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32, color: i8);
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
        pub fn vbank(bank: i8) -> i8;
    }
}
