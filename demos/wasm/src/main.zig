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

    tic.sync(.{
        .sections = .{.tiles = true},
        .bank = 1,
        .toCartridge = true
    });

    tic.map(.{.transparent = &.{1,2}});

    tic.note("C#", .{ .sfx = 2});

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

    tic.note("C6", .{.sfx= 26});

    tic.cls(13);
    // var trans_color = [_]u8 {14};
    // tic.spr(@as(i32, 1+t%60/30*2),mascot.x,mascot.y,&trans_color,1,3,0,0,2,2);
    tic.spr(@as(i32, 1+t%60/30*2),mascot.x,mascot.y,.{ 
        .transparent = &.{14},
        .scale = 3,
        .flip = .no, .rotate= .no,
        .w = 2,
        .h = 2,
    });
    // }&trans_color,1,3,0,0,2,2);
    _ = tic.print("HELLO WORLD!", 84, 84, .{.fixed = true});

    // cls(13)
    // spr()
    // print("HELLO WORLD!",84,84)
    t += 1;
}

