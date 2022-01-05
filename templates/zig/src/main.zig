const tic = @import("tic80.zig");

var started = false;

fn start() void {
}

const TICGuy = struct {
    x : i32 = 96,
    y : i32 = 24,
};

var t : u16 = 0;
var mascot : TICGuy = .{};

export fn TIC() void {
    if (!started) { start(); }

    if (tic.btn(0) != 0) {
        mascot.y -= 1;
    } 
    if (tic.btn(1) != 0) {
        mascot.y +=1;
    } 
    if (tic.btn(2) != 0) {
        mascot.x -= 1;
    } 
    if (tic.btn(3) != 0) {
        mascot.x += 1;
    } 

    tic.cls(13);
    var trans_color = [_]u8 {14};
    tic.spr(@as(i32, 1+t%60/30*2),mascot.x,mascot.y,&trans_color,1,3,0,0,2,2);
    _ = tic.print("HELLO WORLD!", 84, 84, 15, true, 1, false);

    // cls(13)
    // spr()
    // print("HELLO WORLD!",84,84)
    t += 1;
}

