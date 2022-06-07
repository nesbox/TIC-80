#[allow(dead_code)]
extern "C" {
    pub fn btn(id: i32) -> i32;
    pub fn cls(color: i32);
    pub fn print(text: *const u8, x: i32, y: i32, color: i32, fixed: bool, scale: i32, smallfont: bool) -> i32;
    pub fn spr(id: i32, x: i32, y: i32, trans_colors: *const u8, color_count: i32, scale: i32, flip: i32, rotate: i32, w: i32, h: i32);
}
