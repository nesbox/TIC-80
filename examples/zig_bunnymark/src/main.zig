// const w4 = @import("wasm4.zig");

// const smiley = [8]u8{
//     0b11000011,
//     0b10000001,
//     0b00100100,
//     0b00100100,
//     0b00000000,
//     0b00100100,
//     0b10011001,
//     0b11000011,
// };

// export fn update() void {
//     w4.DRAW_COLORS.* = 2;
//     w4.text("Hello from Zig!", 10, 10);

//     const gamepad = w4.GAMEPAD1.*;
//     if (gamepad & w4.BUTTON_1 != 0) {
//         w4.DRAW_COLORS.* = 4;
//     }

//     w4.blit(&smiley, 76, 76, 8, 8, w4.BLIT_1BPP);
//     w4.text("Press X to blink", 16, 90);
// }

// title: Bunnymark in Lua
// author: Josh Goebel
// desc: Benchmarking tool to see how many bunnies can fly around the screen, using WASM.
// input: gamepad
// script: wasm
// version: 0.1

const tic = @import("tic80.zig");
const std = @import("std");
const RndGen = std.rand.DefaultPrng;
var rnd : std.rand.Random = undefined;

const screenWidth = 240;
const screenHeight = 136;
const toolbarHeight = 6;
var t : u32 = 0;

// function randomFloat(lower, greater)
// 	return (math.random() * (greater - lower)) + lower;
// end

fn randomFloat(lower: f32, greater: f32) f32 {
    return rnd.float(f32) * (greater-lower) + lower;
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
        var colors = [_]u8 { 1 };
        tic.spr(self.sprite, @floatToInt(i32,self.x), @floatToInt(i32,self.y), &colors, 1, 1, 0, 0, 4, 4);
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

// Bunny = {width=0,height=0,x=0,y=0,speedX=0,speedY=0,sprite=0}

// function Bunny:new(o)
// 	o = o or {}
// 	setmetatable(o, self)
// 	self.__index = self
// 	self.width = 26
// 	self.height = 32
// 	self.x = math.random(0, screenWidth - self.width)
// 	self.y = math.random(0, screenHeight - self.height)
// 	self.speedX = randomFloat(-100, 100) / 60
// 	self.speedY = randomFloat(-100, 100) / 60
// 	self.sprite = 1
// 	return o
// end

// function Bunny:draw()
//   spr(self.sprite, self.x, self.y, 1, 1, 0, 0, 4, 4)
// end

// function Bunny:update()
// 	self.x = self.x + self.speedX
// 	self.y = self.x + self.speedY

// 	if (self.x + self.width > screenWidth) then
// 		self.x = screenWidth - self.width
// 		self.speedX = self.speedX * -1
// 	end
// 	if (self.x < 0) then
// 		self.x = 0
// 		self.speedX = self.speedX * -1
// 	end
// 	if (self.y + self.height > screenHeight) then
// 		self.y = screenHeight - self.height
// 		self.speedY = self.speedY * -1
// 	end
// 	if (self.y < toolbarHeight) then
// 		self.y = toolbarHeight
// 		self.speedY = self.speedY * -1
// 	end
// end

// FPS = {}

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


// function FPS:new(o)
// 	o = o or {}
// 	setmetatable(o, self)
// 	self.__index = self
// 	self.value = 0
// 	self.frames = 0
// 	self.lastTime = 0
// 	return FPS
// end

// function FPS:getValue()
// 	if (time() - self.lastTime <= 1000) then
// 		self.frames = self.frames + 1
// 	else
// 		self.value = self.frames
// 		self.frames = 0
// 		self.lastTime = time()
// 	end
// 	return self.value
// end


const MAX_BUNNIES = 1200;
var fps : FPS = undefined;
var bunnyCount : usize = 0;
var bunnies : [MAX_BUNNIES]Bunny = undefined;

// bunnies = {}
// table.insert(bunnies, Bunny:new())

var started = false;

fn addBunny() void {
    if (bunnyCount >= MAX_BUNNIES) return;

    bunnies[bunnyCount].initBunny();
    bunnyCount += 1;
}

fn removeBunny() void {
    if (bunnyCount==0) return;

    bunnyCount -= 1;
}

fn start() void {
    rnd = RndGen.init(0).random();
    fps.initFPS();
    addBunny();
    started = true;

    // tic.ZERO = tic.FIRST + 1;
}


export fn testscreen() void {
    var i : usize = 0;

    while (i<2000) {
    // tic.ZERO.* = 0x99;
    tic.FRAMEBUFFER[i]=0x56;
    // tic.FRAMEBUFFER2.*[i]=0x67;
    // tic.ZERO[i]= 0x56;
    // bunnies[i].draw();
    i += 1;
    }
}

// function TIC()
export fn TIC() void {
    if (!started) { start(); }

// 	-- music
// 	if t == 0 then
// 		music(0)
// 	end
// 	if t == 6*64*2.375 then
// 		music(1)
// 	end
// 	t = t + 1
    if (t==0) {
        tic.music(0,-1,-1,true,false,-1,-1);
    }
    if (t == 6*64*2.375) {
        tic.music(1,-1,-1,true,false,-1,-1);
    }
    t = t + 1;

// 	-- Input
// 	if btn(0) then
// 		for i = 1, 5 do
// 			table.insert(bunnies, Bunny:new())
// 		end
// 	end

if (tic.btn(0) != 0) {
    var i : i32 = 0;
    while (i<5) {
        addBunny();
        i+=1;
    }
}

// 	if btn(1) then
// 		for i = 1, 5 do
// 			if next(bunnies) ~= nil then
// 				table.remove(bunnies, i0)
// 			end
// 		end
// 	end

if (tic.btn(1) != 0) {
    var i : i32 = 0;
    while (i<5) {
        removeBunny();
        i+=1;
    }
}

// 	-- Update
// 	for i, item in pairs(bunnies) do
// 		item:update()
// 	end
var i : u32 = 0;
while (i<bunnyCount) {
    bunnies[i].update();
    i += 1;
}

// 	-- Draw
// 	cls(15)
// 	for i, item in pairs(bunnies) do
// 		item:draw()
// 	end
tic.cls(15);
i = 0;
while (i<bunnyCount) {
    bunnies[i].draw();
    i += 1;
}


// 	rect(0, 0, screenWidth, toolbarHeight, 0)
tic.rect(0, 0, screenWidth, toolbarHeight, 0);

var buff : [100]u8 = undefined;
_ = std.fmt.bufPrintZ(&buff, "Bunnies: {any}", .{bunnyCount}) catch unreachable;
_ = tic.print(&buff, 1, 0, 11, false, 1, false);



// 	print("Bunnies: " .. #bunnies, 1, 0, 11, false, 1, false)
// 	print("FPS: " .. fps:getValue(), screenWidth / 2, 0, 11, false, 1, false)
_ = std.fmt.bufPrintZ(&buff, "FPS: {any}", .{fps.getValue()}) catch unreachable;
_ = tic.print(&buff, screenWidth / 2, 0, 11, false, 1, false);
// end


// testscreen();
}

