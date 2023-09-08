// title: Bunnymark in Lua
// author: Josh Goebel
// desc: Benchmarking tool to see how many bunnies can fly around the screen, using WASM.
// input: gamepad
// script: wasm
// version: 0.1

const tic = @import("tic80.zig");
const std = @import("std");
const RndGen = std.rand.DefaultPrng;
var rnd: std.rand.Random = undefined;

const screenWidth = 240;
const screenHeight = 136;
const toolbarHeight = 6;
var t: u32 = 0;

fn randomFloat(lower: f32, greater: f32) f32 {
    return rnd.float(f32) * (greater - lower) + lower;
}

const Bunny = struct {
    width: f16 = 0,
    height: f16 = 0,
    x: f32 = 0,
    y: f32 = 0,
    speedX: f32 = 0,
    speedY: f32 = 0,
    sprite: u8 = 0,

    fn initBunny(self: *Bunny) void {
        self.width = 26;
        self.height = 32;
        self.sprite = 1;
        self.x = randomFloat(0, screenWidth - self.width);
        self.y = randomFloat(0, screenHeight - self.height);
        self.speedX = randomFloat(-100, 100) / 60;
        self.speedY = randomFloat(-100, 100) / 60;
    }

    fn draw(self: Bunny) void {
        tic.spr(self.sprite, @intFromFloat(self.x), @intFromFloat(self.y), .{ .transparent = &.{1}, .w = 4, .h = 4 });
    }

    fn update(self: *Bunny) void {
        self.x += self.speedX;
        self.y += self.speedY;
        if (self.x + self.width > screenWidth) {
            self.x = screenWidth - self.width;
            self.speedX = self.speedX * -1;
        }
        if (self.x < 0) {
            self.x = 0;
            self.speedX = self.speedX * -1;
        }
        if (self.y + self.height > screenHeight) {
            self.y = screenHeight - self.height;
            self.speedY = self.speedY * -1;
        }
        if (self.y < toolbarHeight) {
            self.y = toolbarHeight;
            self.speedY = self.speedY * -1;
        }
    }
};

const FPS = struct {
    value: u32 = 0,
    frames: u32 = 0,
    lastTime: f32 = 0,

    fn initFPS(self: *FPS) void {
        self.value = 0;
        self.frames = 0;
        self.lastTime = 0;
    }

    fn getValue(self: *FPS) u32 {
        if (tic.time() - self.lastTime <= 1000) {
            self.frames += 1;
        } else {
            self.value = self.frames;
            self.frames = 0;
            self.lastTime = tic.time();
        }
        return self.value;
    }
};

const MAX_BUNNIES = 1200;
var fps: FPS = undefined;
var bunnyCount: usize = 0;
var bunnies: [MAX_BUNNIES]Bunny = undefined;

fn addBunny() void {
    if (bunnyCount >= MAX_BUNNIES) return;

    bunnies[bunnyCount].initBunny();
    bunnyCount += 1;
}

fn removeBunny() void {
    if (bunnyCount == 0) return;

    bunnyCount -= 1;
}

export fn testscreen() void {
    var i: usize = 0;

    while (i < 2000) {
        // tic.ZERO.* = 0x99;
        tic.FRAMEBUFFER[i] = 0x56;
        // tic.FRAMEBUFFER2.*[i]=0x67;
        // tic.ZERO[i]= 0x56;
        // bunnies[i].draw();
        i += 1;
    }
}

export fn BOOT() void {
    var xoshiro = RndGen.init(0);
    rnd = xoshiro.random();
    fps.initFPS();
    addBunny();
}

export fn TIC() void {
    if (t == 0) {
        tic.music(0, .{});
    }
    if (t == 6 * 64 * 2.375) {
        tic.music(1, .{});
    }
    t = t + 1;

    if (tic.btn(0)) {
        var i: i32 = 0;
        while (i < 5) {
            addBunny();
            i += 1;
        }
    }

    if (tic.btn(1)) {
        var i: i32 = 0;
        while (i < 5) {
            removeBunny();
            i += 1;
        }
    }

    // 	-- Update
    var i: u32 = 0;
    while (i < bunnyCount) {
        bunnies[i].update();
        i += 1;
    }

    // 	-- Draw
    tic.cls(15);
    i = 0;
    while (i < bunnyCount) {
        bunnies[i].draw();
        i += 1;
    }

    tic.rect(0, 0, screenWidth, toolbarHeight, 9);

    _ = tic.printf("Bunnies: {d}", .{bunnyCount}, 1, 0, .{ .color = 11 });
    _ = tic.printf("FPS: {d:.4}", .{fps.getValue()}, screenWidth / 2, 0, .{ .color = 11 });
    _ = tic.print("hello people", 10, 10, .{ .color = 11 });

    // var x : u32 = 100000000;
    // tic.FRAMEBUFFER[x] = 56;

    // testscreen();
}

export fn BDR() void {}

export fn OVR() void {}
