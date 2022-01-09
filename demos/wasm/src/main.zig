const tic = @import("tic80.zig");

var initDone = false;

fn INIT() void {
}

const TICGuy = struct {
    x : i32 = 96,
    y : i32 = 24,
};

var t : u16 = 0;
var mascot : TICGuy = .{};

export fn TIC() void {
    if (!initDone) { INIT(); }

    tic.sync(.{
        .sections = .{.tiles = true},
        .bank = 1,
        .toCartridge = true
    });

    if (tic.btn(0)) {
        mascot.y -= 1;
    } 
    if (tic.btn(1)) {
        mascot.y +=1;
    } 
    if (tic.btn(2)) {
        mascot.x -= 1;
    } 
    if (tic.btn(3)) {
        mascot.x += 1;
    } 

    tic.cls(13);
    tic.spr(@as(i32, 1+t%60/30*2),mascot.x,mascot.y,.{ 
        .transparent = &.{14},
        .scale = 3,
        .w = 2,
        .h = 2,
    });
    _ = tic.print("HELLO WORLD!", 84, 84, .{.fixed = true});

    t += 1;
}

