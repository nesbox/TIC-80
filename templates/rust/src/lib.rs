mod alloc;
mod tic80;

use tic80::*;

static mut T: i32 = 0;
static mut X: i32 = 96;
static mut Y: i32 = 24;

#[export_name = "TIC"]
pub fn tic() {
    if btn(0) {
        unsafe { Y -= 1 }
    }
    if btn(1) {
        unsafe { Y += 1 }
    }
    if btn(2) {
        unsafe { X -= 1 }
    }
    if btn(3) {
        unsafe { X += 1 }
    }

    cls(13);
    unsafe {
        spr(1 + T % 60 / 30 * 2, X, Y, &[14], 3, 0, 0, 2, 2);
    }
    print("HELLO WORLD!", 84, 84, 15, false, 1, false);

    unsafe {
        T += 1;
    }
}
