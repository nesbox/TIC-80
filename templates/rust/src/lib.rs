mod alloc;
mod tic80;

use tic80::*;

static mut T: i32 = 0;
static mut X: i32 = 96;
static mut Y: i32 = 24;

#[export_name = "TIC"]
pub fn tic() {
    unsafe {
        if btn(0) {
            Y -= 1;
        }
        if btn(1) {
            Y += 1;
        }
        if btn(2) {
            X -= 1;
        }
        if btn(3) {
            X += 1;
        }
    }

    unsafe {
        sys::cls(13);
        sys::spr(1 + T % 60 / 30 * 2, X, Y, [14].as_ptr(), 1, 3, 0, 0, 2, 2);
        sys::print("HELLO WORLD!\0".as_ptr(), 84, 84, 15, false, 1, false);
    }

    unsafe {
        T += 1;
    }
}
