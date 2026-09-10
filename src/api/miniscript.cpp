// MIT License

// Copyright (c) 2026 Robert Miles @MineRobber9000 // https://khuxkm.tilde.team

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/core.h"
#include "studio/system.h"
#include "miniscript.h"
extern "C" bool parse_note(const char* noteStr, s32* note, s32* octave);

using namespace MiniScript;

/* 
    The harness is designed to allow both the typical TIC-80 callback style as
    well as the MiniScript Zen(tm) "event pump" style.
    
    Essentially, if you write your TIC-80 script in the callback style, this
    code on the bottom of it will turn it into "event pump" style, which means
    the MiniScript implementation can be written as if all games are "event
    pump" style.
   
    However, if you write your code as "event pump" style, then the harness will
    simply get out of your way and let you do your thing.
*/
static const char* miniscript_pre_harness_code = R"(globals.yield=function()
    globals.yield = @intrinsics.yield
    return intrinsics.yield
end function
)";
static const char* miniscript_post_harness_code = R"(
if @globals.yield == @intrinsics.yield then return
if globals.hasIndex("BOOT") then BOOT
while true
    TIC
    yield
end while)";
static const String miniscript_pre_harness() {
    static String str = []{ String s(miniscript_pre_harness_code); GCManager::AddRoot(s); return s; }();
    return str;
}
static const String miniscript_post_harness() {
    static String str = []{ String s(miniscript_post_harness_code); GCManager::AddRoot(s); return s; }();
    return str;
}

static bool _miniscriptInitialized = false;

struct TICMiniScriptState {
    Interpreter interpreter;

    bool error = false;
    String errorString = "";

    TICMiniScriptState(tic_core *core) {
        interpreter = Interpreter::New();
        interpreter.set_hostData(core);
        interpreter.set_errorOutput([this](String s, Boolean lineBreak = true) {
            this->error = true;
            this->errorString += s;
            if (lineBreak) this->errorString += "\n";
        });
    }
};

static tic_core *getCore(Context context) {
    return static_cast<tic_core*>(context.GetInterpreter().hostData());
}

#define INTRINSIC_LAMBDA [](MiniScript::Context context, MiniScript::IntrinsicResult partialResult) -> MiniScript::IntrinsicResult

struct RemapData {
    Interpreter interpreter;
    Value callback;
};

static void remapCallback(void *data, s32 x, s32 y, RemapResult *result) {
    // Retrieve VM and callback
    RemapData *remap = static_cast<RemapData*>(data);
    auto interpreter = remap->interpreter;
    auto callback = remap->callback;
    // Arguments: (tile, x, y)
    ValueList arguments;
    arguments.Add(Value(result->index));
    arguments.Add(Value(x));
    arguments.Add(Value(y));
    auto resultValue = interpreter.RunFunction(callback, arguments);
    if (resultValue.IsNumber()) {
        result->index = resultValue.IntValue() & 0xFF;
    } else if (resultValue.IsList()) {
        s32 count = resultValue.ListCount();
        if (count>=1) result->index = resultValue.ListGet(0).IntValue() & 0xFF;
        if (count>=2) result->flip = static_cast<tic_flip>(resultValue.ListGet(1).IntValue() & 0x3);
        if (count>=3) result->rotate = static_cast<tic_rotate>(resultValue.ListGet(2).IntValue() & 0x3);
    }
}

static void TIC80Intrinsics(ValueDict& tic80Module) {
    Intrinsic i;

    // btn
    i = Intrinsic::Create("");
    i.AddParam("id", Value::Null);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        Value idValue = context.GetArg(0);

        if (idValue.IsNull()) {
            return IntrinsicResult(core->api.btn(tic, -1));
        } else {
            bool pressed = core->api.btn(tic, idValue.IntValue() & 0x1f);
            return IntrinsicResult(pressed);
        }
    });
    tic80Module.SetValue("btn", i.GetFunc());

    // btnp
    i = Intrinsic::Create("");
    i.AddParam("id", Value::Null);
    i.AddParam("hold", Value::Null);
    i.AddParam("period", Value::Null);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        Value idValue = context.GetArg(0);
        Value holdValue = context.GetArg(1);
        Value periodValue = context.GetArg(2);

        if (idValue.IsNull()) {
            return IntrinsicResult(core->api.btnp(tic, -1, -1, -1));
        } else if (holdValue.IsNull() || periodValue.IsNull()) {
            bool pressed = core->api.btnp(tic, idValue.IntValue() & 0x1f, -1, -1);
            return IntrinsicResult(pressed);
        } else {
            bool pressed = core->api.btnp(tic, idValue.IntValue() & 0x1f, holdValue.IntValue(), periodValue.IntValue());
            return IntrinsicResult(pressed);
        }
    });
    tic80Module.SetValue("btnp", i.GetFunc());

    // circ
    i = Intrinsic::Create("");
    i.AddParam("x");
    i.AddParam("y");
    i.AddParam("radius");
    i.AddParam("color");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 x = context.GetArg(0).IntValue();
        s32 y = context.GetArg(1).IntValue();
        s32 radius = context.GetArg(2).IntValue();
        u8 color = context.GetArg(3).IntValue() & 0xF;

        core->api.circ(tic, x, y, radius, color);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("circ", i.GetFunc());

    // circb
    i = Intrinsic::Create("");
    i.AddParam("x");
    i.AddParam("y");
    i.AddParam("radius");
    i.AddParam("color");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 x = context.GetArg(0).IntValue();
        s32 y = context.GetArg(1).IntValue();
        s32 radius = context.GetArg(2).IntValue();
        u8 color = context.GetArg(3).IntValue() & 0xF;

        core->api.circb(tic, x, y, radius, color);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("circb", i.GetFunc());

    // clip
    i = Intrinsic::Create("");
    i.AddParam("x", Value::Null);
    i.AddParam("y", Value::Null);
    i.AddParam("w", Value::Null);
    i.AddParam("h", Value::Null);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        Value xValue = context.GetArg(0);
        Value yValue = context.GetArg(1);
        Value wValue = context.GetArg(2);
        Value hValue = context.GetArg(3);

        if (xValue.IsNull() && yValue.IsNull() && wValue.IsNull() && hValue.IsNull()) {
            core->api.clip(tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
        } else if (xValue.IsNumber() && yValue.IsNumber() && wValue.IsNumber() && hValue.IsNumber()) {
            core->api.clip(tic, xValue.IntValue(), yValue.IntValue(), wValue.IntValue(), hValue.IntValue());
        } else {
            context.vm.RaiseRuntimeError("invalid call to clip(); either use clip() or clip(x,y,w,h)");
        }
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("clip", i.GetFunc());

    // cls
    i = Intrinsic::Create("");
    i.AddParam("color", 0);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        u8 color = context.GetArg(0).IntValue() & 0xF;

        core->api.cls(tic, color);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("cls", i.GetFunc());

    // elli
    i = Intrinsic::Create("");
    i.AddParam("x");
    i.AddParam("y");
    i.AddParam("a");
    i.AddParam("b");
    i.AddParam("color");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 x = context.GetArg(0).IntValue();
        s32 y = context.GetArg(1).IntValue();
        s32 aRadius = context.GetArg(2).IntValue();
        s32 bRadius = context.GetArg(3).IntValue();
        u8 color = context.GetArg(4).IntValue() & 0xF;

        core->api.elli(tic, x, y, aRadius, bRadius, color);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("elli", i.GetFunc());

    // ellib
    i = Intrinsic::Create("");
    i.AddParam("x");
    i.AddParam("y");
    i.AddParam("a");
    i.AddParam("b");
    i.AddParam("color");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 x = context.GetArg(0).IntValue();
        s32 y = context.GetArg(1).IntValue();
        s32 aRadius = context.GetArg(2).IntValue();
        s32 bRadius = context.GetArg(3).IntValue();
        u8 color = context.GetArg(4).IntValue() & 0xF;

        core->api.ellib(tic, x, y, aRadius, bRadius, color);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("ellib", i.GetFunc());

    // exit
    i = Intrinsic::Create("");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;

        core->api.exit(tic);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("exit", i.GetFunc());

    // fget
    i = Intrinsic::Create("");
    i.AddParam("index");
    i.AddParam("flag");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 index = context.GetArg(0).IntValue();
        u8 flag = context.GetArg(1).IntValue() & 0x7;

        return IntrinsicResult(core->api.fget(tic, index, flag));
    });
    tic80Module.SetValue("fget", i.GetFunc());

    // fset
    i = Intrinsic::Create("");
    i.AddParam("index");
    i.AddParam("flag");
    i.AddParam("value");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 index = context.GetArg(0).IntValue();
        u8 flag = context.GetArg(1).IntValue() & 0x7;
        bool value = context.GetArg(2).BoolValue();

        core->api.fset(tic, index, flag, value);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("fset", i.GetFunc());

    // font
    i = Intrinsic::Create("");
    i.AddParam("text");
    i.AddParam("x", 0);
    i.AddParam("y", 0);
    i.AddParam("transcolor", 0);
    i.AddParam("width", 8);
    i.AddParam("height", 8);
    i.AddParam("fixed", false);
    i.AddParam("scale", 1);
    i.AddParam("alt", false);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;

        const char *text = context.GetArg(0).c_str();
        s32 x = context.GetArg(1).IntValue();
        s32 y = context.GetArg(2).IntValue();
        u8 transcolor = context.GetArg(3).IntValue() & 0xF;
        s32 width = context.GetArg(4).IntValue();
        s32 height = context.GetArg(5).IntValue();
        bool fixed = context.GetArg(6).BoolValue();
        s32 scale = context.GetArg(7).IntValue();
        bool alt = context.GetArg(8).BoolValue();

        core->api.font(tic, text, x, y, &transcolor, 1, width, height, fixed, scale, alt);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("font", i.GetFunc());

    // key
    i = Intrinsic::Create("");
    i.AddParam("code", Value::Null);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        Value codeValue = context.GetArg(0);

        if (codeValue.IsNull()) {
            return IntrinsicResult(core->api.key(tic, tic_key_unknown));
        } else {
            tic_key code = codeValue.IntValue();
            if (code < tic_keys_count) {
                return IntrinsicResult(core->api.key(tic, code));
            }
            context.vm.RaiseRuntimeError("unknown keyboard code");
            return IntrinsicResult::Null;
        }
    });
    tic80Module.SetValue("key", i.GetFunc());

    // keyp
    i = Intrinsic::Create("");
    i.AddParam("code", Value::Null);
    i.AddParam("hold", Value::Null);
    i.AddParam("period", Value::Null);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        Value codeValue = context.GetArg(0);
        Value holdValue = context.GetArg(1);
        Value periodValue = context.GetArg(2);

        if (codeValue.IsNull()) {
            return IntrinsicResult(core->api.keyp(tic, tic_key_unknown, -1, -1));
        } else {
            tic_key code = codeValue.IntValue();
            if (code >= tic_keys_count) {
                context.vm.RaiseRuntimeError("unknown keyboard code");
                return IntrinsicResult::Null;
            }
            if (holdValue.IsNull() || periodValue.IsNull()) {
                bool pressed = core->api.btnp(tic, code, -1, -1);
                return IntrinsicResult(pressed);
            } else {
                bool pressed = core->api.btnp(tic, code, holdValue.IntValue(), periodValue.IntValue());
                return IntrinsicResult(pressed);
            }
        }
    });
    tic80Module.SetValue("keyp", i.GetFunc());

    // line
    i = Intrinsic::Create("");
    i.AddParam("x0");
    i.AddParam("y0");
    i.AddParam("x1");
    i.AddParam("y1");
    i.AddParam("color");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        float x0 = context.GetArg(0).FloatValue();
        float y0 = context.GetArg(1).FloatValue();
        float x1 = context.GetArg(2).FloatValue();
        float y1 = context.GetArg(3).FloatValue();
        u8 color = context.GetArg(4).IntValue() & 0xF;

        core->api.line(tic, x0, y0, x1, y1, color);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("line", i.GetFunc());

    // map
    i = Intrinsic::Create("");
    i.AddParam("x", 0);
    i.AddParam("y", 0);
    i.AddParam("w", TIC_MAP_SCREEN_WIDTH);
    i.AddParam("h", TIC_MAP_SCREEN_HEIGHT);
    i.AddParam("sx", 0);
    i.AddParam("sy", 0);
    i.AddParam("colorkey", Value::Null);
    i.AddParam("scale", 1);
    i.AddParam("remap", Value::Null);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 x = context.GetArg(0).IntValue();
        s32 y = context.GetArg(1).IntValue();
        s32 w = context.GetArg(2).IntValue();
        s32 h = context.GetArg(3).IntValue();
        s32 sx = context.GetArg(4).IntValue();
        s32 sy = context.GetArg(5).IntValue();
        s32 scale = context.GetArg(7).IntValue();

        // Handle colorkey
        static u8 colors[TIC_PALETTE_SIZE];
        s32 count = 0;
        Value chromakey = context.GetArg(6);
        if (chromakey.IsList()) {
            count = chromakey.ListCount();
            if (count > TIC_PALETTE_SIZE) count = TIC_PALETTE_SIZE;
            for (int i=0; i<count; ++i) {
                colors[i] = chromakey.ListGet(i).IntValue() & 0xF;
            }
        } else if (chromakey.IsNumber()) {
            colors[count++] = chromakey.IntValue();
        }

        // Handle remap
        Value remap = context.GetArg(8);
        if (remap.IsFuncRef()) {
            RemapData data(context.GetInterpreter(), remap);
            core->api.map(tic, x, y, w, h, sx, sy, colors, count, scale, remapCallback, static_cast<void*>(&data));
        } else if (remap.IsNull()) {
            core->api.map(tic, x, y, w, h, sx, sy, colors, count, scale, NULL, NULL);
        } else {
            context.vm.RaiseRuntimeError("remap must be funcRef or null");
        }

        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("map", i.GetFunc());

    // memcpy
    i = Intrinsic::Create("");
    i.AddParam("to");
    i.AddParam("from");
    i.AddParam("length");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 dst = context.GetArg(0).IntValue();
        s32 src = context.GetArg(1).IntValue();
        s32 len = context.GetArg(2).IntValue();

        core->api.memcpy(tic, dst, src, len);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("memcpy", i.GetFunc());

    // memset
    i = Intrinsic::Create("");
    i.AddParam("addr");
    i.AddParam("value");
    i.AddParam("length");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 addr = context.GetArg(0).IntValue();
        u8 val = context.GetArg(1).IntValue() & 0xFF;
        s32 len = context.GetArg(2).IntValue();

        core->api.memset(tic, addr, val, len);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("memset", i.GetFunc());

    // mget
    i = Intrinsic::Create("");
    i.AddParam("x");
    i.AddParam("y");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 x = context.GetArg(0).IntValue();
        s32 y = context.GetArg(1).IntValue();

        return IntrinsicResult(core->api.mget(tic, x, y));
    });
    tic80Module.SetValue("mget", i.GetFunc());

    // mset
    i = Intrinsic::Create("");
    i.AddParam("x");
    i.AddParam("y");
    i.AddParam("tile_id");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 x = context.GetArg(0).IntValue();
        s32 y = context.GetArg(1).IntValue();
        u8 tile_id = context.GetArg(2).IntValue() & 0xFF;

        core->api.mset(tic, x, y, tile_id);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("mset", i.GetFunc());

    // mouse
    i = Intrinsic::Create("");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        ValueDict result;

        {
            tic_point pos = core->api.mouse(tic);
            result.SetValue("x", Value(pos.x));
            result.SetValue("y", Value(pos.y));
        }

        const tic80_mouse* mouse = &core->memory.ram->input.mouse;
        result.SetValue("left", Value((bool)mouse->left));
        result.SetValue("middle", Value((bool)mouse->middle));
        result.SetValue("right", Value((bool)mouse->right));
        result.SetValue("scrollx", Value(mouse->scrollx));
        result.SetValue("scrolly", Value(mouse->scrolly));

        return IntrinsicResult(DynamicMap(result));
    });
    tic80Module.SetValue("mouse", i.GetFunc());

    // music
    i = Intrinsic::Create("");
    i.AddParam("track", -1);
    i.AddParam("frame", -1);
    i.AddParam("row", -1);
    i.AddParam("loop", false);
    i.AddParam("sustain", false);
    i.AddParam("tempo", -1);
    i.AddParam("speed", -1);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 track = context.GetArg(0).IntValue();
        if (track > MUSIC_TRACKS - 1) {
            context.vm.RaiseRuntimeError("invalid music track index");
            return IntrinsicResult::Null;
        }
        s32 frame = context.GetArg(1).IntValue();
        s32 row = context.GetArg(2).IntValue();
        bool loop = context.GetArg(3).BoolValue();
        bool sustain = context.GetArg(4).BoolValue();
        s32 tempo = context.GetArg(5).IntValue();
        s32 speed = context.GetArg(6).IntValue();

        core->api.music(tic, -1, 0, 0, false, false, -1, -1);
        core->api.music(tic, track, frame, row, loop, sustain, tempo, speed);

        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("music", i.GetFunc());

    // peek
    i = Intrinsic::Create("");
    i.AddParam("addr");
    i.AddParam("bits", BITS_IN_BYTE);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 addr = context.GetArg(0).IntValue();
        s32 bits = context.GetArg(1).IntValue();

        return IntrinsicResult(core->api.peek(tic, addr, bits));
    });
    tic80Module.SetValue("peek", i.GetFunc());

    // peek1
    i = Intrinsic::Create("");
    i.AddParam("addr");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 addr = context.GetArg(0).IntValue();

        return IntrinsicResult(core->api.peek1(tic, addr));
    });
    tic80Module.SetValue("peek1", i.GetFunc());

    // peek2
    i = Intrinsic::Create("");
    i.AddParam("addr");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 addr = context.GetArg(0).IntValue();

        return IntrinsicResult(core->api.peek2(tic, addr));
    });
    tic80Module.SetValue("peek2", i.GetFunc());

    // peek4
    i = Intrinsic::Create("");
    i.AddParam("addr");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 addr = context.GetArg(0).IntValue();

        return IntrinsicResult(core->api.peek4(tic, addr));
    });
    tic80Module.SetValue("peek4", i.GetFunc());

    // pix
    i = Intrinsic::Create("");
    i.AddParam("x");
    i.AddParam("y");
    i.AddParam("color", Value::Null);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 x = context.GetArg(0).IntValue();
        s32 y = context.GetArg(1).IntValue();
        Value colorValue = context.GetArg(2);

        if (colorValue.IsNull()) {
            return IntrinsicResult(core->api.pix(tic, x, y, 0, true));
        } else {
            u8 color = colorValue.IntValue() & 0xF;
            core->api.pix(tic, x, y, color, false);
            return IntrinsicResult::Null;
        }
    });
    tic80Module.SetValue("pix", i.GetFunc());

    // pmem
    i = Intrinsic::Create("");
    i.AddParam("index");
    i.AddParam("val32", Value::Null);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 index = context.GetArg(0).IntValue();
        Value val32 = context.GetArg(1);

        return IntrinsicResult(core->api.pmem(tic, index, val32.IntValue(), !val32.IsNull()));
    });
    tic80Module.SetValue("pmem", i.GetFunc());

    // poke
    i = Intrinsic::Create("");
    i.AddParam("addr");
    i.AddParam("val");
    i.AddParam("bits", BITS_IN_BYTE);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 addr = context.GetArg(0).IntValue();
        u8 value = context.GetArg(1).IntValue() & 0xFF;
        s32 bits = context.GetArg(2).IntValue();

        core->api.poke(tic, addr, value, bits);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("poke", i.GetFunc());

    // poke1
    i = Intrinsic::Create("");
    i.AddParam("addr");
    i.AddParam("val");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 addr = context.GetArg(0).IntValue();
        u8 value = context.GetArg(1).IntValue() & 0x1;

        core->api.poke1(tic, addr, value);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("poke1", i.GetFunc());

    // poke2
    i = Intrinsic::Create("");
    i.AddParam("addr");
    i.AddParam("val");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 addr = context.GetArg(0).IntValue();
        u8 value = context.GetArg(1).IntValue() & 0x3;

        core->api.poke2(tic, addr, value);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("poke2", i.GetFunc());

    // poke4
    i = Intrinsic::Create("");
    i.AddParam("addr");
    i.AddParam("val");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 addr = context.GetArg(0).IntValue();
        u8 value = context.GetArg(1).IntValue() & 0xF;

        core->api.poke4(tic, addr, value);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("poke4", i.GetFunc());

    // print
    i = Intrinsic::Create("");
    i.AddParam("text");
    i.AddParam("x", 0);
    i.AddParam("y", 0);
    i.AddParam("color", 15);
    i.AddParam("fixed", false);
    i.AddParam("scale", 1);
    i.AddParam("smallfont", false);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;

        const char *text = context.GetArg(0).c_str();
        s32 x = context.GetArg(1).IntValue();
        s32 y = context.GetArg(2).IntValue();
        u8 color = context.GetArg(3).IntValue() & 0xF;
        bool fixed = context.GetArg(4).BoolValue();
        s32 scale = context.GetArg(5).IntValue();
        bool smallfont = context.GetArg(6).BoolValue();

        core->api.print(tic, text, x, y, color, fixed, scale, smallfont);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("print", i.GetFunc());

    // rect
    i = Intrinsic::Create("");
    i.AddParam("x");
    i.AddParam("y");
    i.AddParam("width");
    i.AddParam("height");
    i.AddParam("color");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 x = context.GetArg(0).IntValue();
        s32 y = context.GetArg(1).IntValue();
        s32 width = context.GetArg(2).IntValue();
        s32 height = context.GetArg(3).IntValue();
        u8 color = context.GetArg(4).IntValue() & 0xF;

        core->api.rect(tic, x, y, width, height, color);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("rect", i.GetFunc());

    // rectb
    i = Intrinsic::Create("");
    i.AddParam("x");
    i.AddParam("y");
    i.AddParam("width");
    i.AddParam("height");
    i.AddParam("color");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 x = context.GetArg(0).IntValue();
        s32 y = context.GetArg(1).IntValue();
        s32 width = context.GetArg(2).IntValue();
        s32 height = context.GetArg(3).IntValue();
        u8 color = context.GetArg(4).IntValue() & 0xF;

        core->api.rectb(tic, x, y, width, height, color);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("rectb", i.GetFunc());

    // reset
    i = Intrinsic::Create("");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        
        core->state.initialized = false;

        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("reset", i.GetFunc());

    // sfx
    i = Intrinsic::Create("");
    i.AddParam("id");
    i.AddParam("note", Value::Null);
    i.AddParam("duration", -1);
    i.AddParam("channel", 0);
    i.AddParam("volume", MAX_VOLUME);
    i.AddParam("speed", Value::Null);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 index = context.GetArg(0).IntValue();
        if (index >= SFX_COUNT) {
            context.vm.RaiseRuntimeError("unknown sfx index");
            return IntrinsicResult::Null;
        }
        Value noteValue = context.GetArg(1);
        s32 duration = context.GetArg(2).IntValue();
        s32 channel = context.GetArg(3).IntValue();
        Value volumeValue = context.GetArg(4);
        Value speedValue = context.GetArg(5);

        s32 note = -1;
        s32 octave = -1;
        s32 speed = SFX_DEF_SPEED;
        if (index > 0) {
            tic_sample* effect = tic->ram->sfx.samples.data + index;

            note = effect->note;
            octave = effect->octave;
            speed = effect->speed;
        }
        if (noteValue.IsNumber()) {
            s32 n = noteValue.IntValue();
            note = n % NOTES;
            octave = n / NOTES;
        } else if (noteValue.IsString()) {
            const char *notestr = noteValue.c_str();
            if (!parse_note(notestr, &note, &octave)) {
                context.vm.RaiseRuntimeError("invalid note, should be like C#4");
                return IntrinsicResult::Null;
            }
        }
        if (!speedValue.IsNull()) speed = speedValue.IntValue();

        s32 volumes[TIC80_SAMPLE_CHANNELS] = {MAX_VOLUME, MAX_VOLUME};
        if (volumeValue.IsNumber()) {
            volumes[0] = volumes[1] = volumeValue.IntValue();
        } else if (volumeValue.IsList() && volumeValue.ListCount() >= TIC80_SAMPLE_CHANNELS) {
            for (int i=0; i<TIC80_SAMPLE_CHANNELS; ++i) {
                volumes[i] = volumeValue.ListGet(i).IntValue();
            }
        }

        if (channel < 0 || channel >= TIC_SOUND_CHANNELS) {
            context.vm.RaiseRuntimeError("unknown channel");
            return IntrinsicResult::Null;
        }

        core->api.sfx(tic, index, note, octave, duration, channel, volumes[0] & 0xF, volumes[1] & 0xF, speed);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("sfx", i.GetFunc());

    // spr
    i = Intrinsic::Create("");
    i.AddParam("index", 0);
    i.AddParam("x", 0);
    i.AddParam("y", 0);
    i.AddParam("colorkey", Value::Null);
    i.AddParam("scale", 1);
    i.AddParam("flip", tic_no_flip);
    i.AddParam("rotate", tic_no_rotate);
    i.AddParam("w", 1);
    i.AddParam("h", 1);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        s32 index = context.GetArg(0).IntValue();
        s32 x = context.GetArg(1).IntValue();
        s32 y = context.GetArg(2).IntValue();
        Value chromakey = context.GetArg(3);
        s32 scale = context.GetArg(4).IntValue();
        tic_flip flip = static_cast<tic_flip>(context.GetArg(5).IntValue());
        tic_rotate rotate = static_cast<tic_rotate>(context.GetArg(6).IntValue());
        s32 w = context.GetArg(7).IntValue();
        s32 h = context.GetArg(8).IntValue();

        // Handle colorkey
        static u8 trans_colors[TIC_PALETTE_SIZE];
        s32 trans_count = 0;
        if (chromakey.IsList()) {
            trans_count = chromakey.ListCount();
            if (trans_count > TIC_PALETTE_SIZE) trans_count = TIC_PALETTE_SIZE;
            for (int i=0; i<trans_count; ++i) {
                trans_colors[i] = chromakey.ListGet(i).IntValue() & 0xF;
            }
        } else if (chromakey.IsNumber()) {
            trans_colors[trans_count++] = chromakey.IntValue();
        }

        core->api.spr(tic, index, x, y, w, h, trans_colors, trans_count, scale, flip, rotate);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("spr", i.GetFunc());

    // sync
    i = Intrinsic::Create("");
    i.AddParam("mask", 0);
    i.AddParam("bank", 0);
    i.AddParam("tocart", false);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        u32 mask = context.GetArg(0).UIntValue();
        s32 bank = context.GetArg(1).IntValue();
        bool tocart = context.GetArg(2).BoolValue();

        if (bank >= 0 && bank < TIC_BANKS) {
            core->api.sync(tic, mask, bank, tocart);
        } else {
            context.vm.RaiseRuntimeError("sync() error, invalid bank");
        }

        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("sync", i.GetFunc());

    // ttri
    i = Intrinsic::Create("");
    i.AddParam("x1");
    i.AddParam("y1");
    i.AddParam("x2");
    i.AddParam("y2");
    i.AddParam("x3");
    i.AddParam("y3");
    i.AddParam("u1");
    i.AddParam("v1");
    i.AddParam("u2");
    i.AddParam("v2");
    i.AddParam("u3");
    i.AddParam("v3");
    i.AddParam("texsrc", Value::Null);
    i.AddParam("chromakey", Value::Null);
    i.AddParam("z1", Value::Null);
    i.AddParam("z2", Value::Null);
    i.AddParam("z3", Value::Null);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        float pt[12];
        for (int i=0; i<12; ++i) {
            pt[i] = context.GetArg(i).FloatValue();
        }

        tic_texture_src texsrc = tic_tiles_texture;
        Value texsrcValue = context.GetArg(12);
        if (!texsrcValue.IsNull()) {
            texsrc = static_cast<tic_texture_src>(texsrcValue.IntValue());
        }

        static u8 colors[TIC_PALETTE_SIZE];
        s32 count = 0;
        Value chromakey = context.GetArg(13);
        if (chromakey.IsList()) {
            count = chromakey.ListCount();
            if (count > TIC_PALETTE_SIZE) count = TIC_PALETTE_SIZE;
            for (int i=0; i<count; ++i) {
                colors[i] = chromakey.ListGet(i).IntValue() & 0xF;
            }
        } else if (chromakey.IsNumber()) {
            colors[count++] = chromakey.IntValue();
        }

        float z[3] = { 0 };
        bool depth = true;
        for (int i=0; i<3; ++i) {
            auto arg = context.GetArg(14+i);
            z[i] = arg.FloatValue();
            depth = depth && !arg.IsNull();
        }

        core->api.ttri(tic, pt[0], pt[1],   //  xy 1
                            pt[2], pt[3],   //  xy 2
                            pt[4], pt[5],   //  xy 3
                            pt[6], pt[7],   //  uv 1
                            pt[8], pt[9],   //  uv 2
                            pt[10], pt[11], //  uv 3
                            texsrc,            // texture source
                            colors, count,  // chroma
                            z[0], z[1], z[2], depth); // depth

        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("ttri", i.GetFunc());

    // time
    i = Intrinsic::Create("");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;

        return IntrinsicResult(core->api.time(tic));
    });
    tic80Module.SetValue("time", i.GetFunc());

    // trace
    i = Intrinsic::Create("");
    i.AddParam("message");
    i.AddParam("color", 15);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        const char *message = context.GetArg(0).c_str();
        u8 color = context.GetArg(1).IntValue() & 0xF;

        core->api.trace(tic, message, color);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("trace", i.GetFunc());

    // tri
    i = Intrinsic::Create("");
    i.AddParam("x1");
    i.AddParam("y1");
    i.AddParam("x2");
    i.AddParam("y2");
    i.AddParam("x3");
    i.AddParam("y3");
    i.AddParam("color");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        float pt[6];
        for (int i=0; i<6; ++i) {
            pt[i] = context.GetArg(i).FloatValue();
        }
        u8 color = context.GetArg(6).IntValue() & 0xF;

        core->api.tri(tic, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("tri", i.GetFunc());

    // trib
    i = Intrinsic::Create("");
    i.AddParam("x1");
    i.AddParam("y1");
    i.AddParam("x2");
    i.AddParam("y2");
    i.AddParam("x3");
    i.AddParam("y3");
    i.AddParam("color");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        float pt[6];
        for (int i=0; i<6; ++i) {
            pt[i] = context.GetArg(i).FloatValue();
        }
        u8 color = context.GetArg(6).IntValue() & 0xF;

        core->api.trib(tic, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);
        return IntrinsicResult::Null;
    });
    tic80Module.SetValue("trib", i.GetFunc());

    // tstamp
    i = Intrinsic::Create("");
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;

        return IntrinsicResult(core->api.tstamp(tic));
    });
    tic80Module.SetValue("tstamp", i.GetFunc());

    // vbank
    i = Intrinsic::Create("");
    i.AddParam("bank", Value::Null);
    i.set_Code(INTRINSIC_LAMBDA {
        tic_core *core = getCore(context);
        tic_mem *tic = (tic_mem*)core;
        Value bankValue = context.GetArg(0);

        s32 prev = core->state.vbank.id;

        if (!bankValue.IsNull()) {
            core->api.vbank(tic, bankValue.IntValue());
        }

        return IntrinsicResult(prev);
    });
    tic80Module.SetValue("vbank", i.GetFunc());
}

void StaticInitMiniScript() {
    if (_miniscriptInitialized) return;
    GCManager::Init();
	MiniScript::value_init_constants();
	ErrorTypes::Init();

    MiniScript::hostName = "TIC-80";
    MiniScript::hostVersion = DEF2STR(TIC_VERSION);
    MiniScript::hostInfo = "https://tic80.com/";

    auto f = Intrinsic::Create("tic80");
    f.set_Code(INTRINSIC_LAMBDA {
        static ValueDict tic80Module;
        static Value tic80ModuleValue;

        if (tic80ModuleValue.IsNull()) {
            TIC80Intrinsics(tic80Module);
            tic80ModuleValue = GCManager::NewMapFromDict(tic80Module);
            GCManager::AddRoot(tic80ModuleValue);
        }

        return IntrinsicResult(tic80ModuleValue);
    });

    _miniscriptInitialized = true;
}

static bool initMiniScript(tic_mem *tic, const char *code) {
    tic_core *core = (tic_core*)tic;

    // Do MiniScript initialization work if we haven't already
    if (!_miniscriptInitialized) {
        StaticInitMiniScript();
    }

    // Discard previous state if it exists
    if (core->currentVM) {
        delete static_cast<TICMiniScriptState*>(core->currentVM);
        core->currentVM = nullptr;
    }

    // Create VM state
    auto* vm = new TICMiniScriptState(core);
    core->currentVM = vm;

    // Load code
    vm->interpreter.Reset(miniscript_pre_harness());
    vm->interpreter.RunUntilDone(0.1, true);
    vm->interpreter.ResetPreservingGlobals(code + miniscript_post_harness());
    vm->interpreter.Compile();
    if (vm->error) {
        core->data->error(core->data->data, vm->errorString.c_str());
        return false;
    }

    return true;
}

static void closeMiniScript(tic_mem *tic) {
    tic_core *core = (tic_core*)tic;

    if (core->currentVM) {
        delete static_cast<TICMiniScriptState*>(core->currentVM);
        core->currentVM = nullptr;
    }
}

static void callMiniScriptBoot(tic_mem *tic) {
    // Do nothing. The harness will call `BOOT` before it first calls `TIC`,
    // assuming `BOOT` exists.
}

static void callMiniScriptTick(tic_mem *tic) {
    tic_core *core = (tic_core*)tic;

    // retrieve TICMiniScriptState
    auto *vm = static_cast<TICMiniScriptState*>(core->currentVM);
    if (!vm) return;

    // Run code for max 0.1 seconds (means the worst the user can end up with is 10FPS)
    vm->interpreter.RunUntilDone(0.1, true);

    // If the user's code errored, display the error.
    if (vm->error) {
        core->data->error(core->data->data, vm->errorString.c_str());
    }

    // If the VM has terminated, exit.
    if (vm->interpreter.Done()) {
        core->data->exit(core->data->data);
    }
}

static void callMiniScriptCallback(tic_mem *tic, Value name, s32 arg) {
    tic_core *core = (tic_core*)tic;

    // retrieve TICMiniScriptState
    auto *vm = static_cast<TICMiniScriptState*>(core->currentVM);
    if (!vm) return;

    // Retrieve the callback
    auto globals = vm->interpreter.vm().GetGlobals();
    auto slot = globals.Find(name);
    if (slot==-1) return;
    Value callback = globals.ValueAtSlot(slot);
    if (callback.IsUnassigned()) return;

    // If it's assigned to something that isn't a function, error
    if (!callback.IsFuncRef()) {
        core->data->error(core->data->data, (name.ToString() + " isn't a function reference").c_str());
    }

    // Construct the argument and call the function
    ValueList arguments;
    arguments.Add(Value(arg));
    auto ignored = vm->interpreter.RunFunction(callback, arguments);

    // If the user's code errored, display the error.
    if (vm->error) {
        core->data->error(core->data->data, vm->errorString.c_str());
    }

    // If the VM has terminated, exit.
    if (vm->interpreter.Done()) {
        core->data->exit(core->data->data);
    }
}

static void callMiniScriptScanline(tic_mem *tic, s32 row, void* data) {
    static Value name = Value::make_string("SCN");
    callMiniScriptCallback(tic, name, row);
}

static void callMiniScriptBorder(tic_mem *tic, s32 row, void* data) {
    static Value name = Value::make_string("BDR");
    callMiniScriptCallback(tic, name, row);
}

static void callMiniScriptMenu(tic_mem *tic, s32 index, void* data) {
    static Value name = Value::make_string("MENU");
    callMiniScriptCallback(tic, name, index);
}

static const char* const MiniScriptKeywords [] =
{
    "and", "break", "continue", "else", "end", "false", "for", "function",
    "if", "in", "isa", "new", "not", "null", "or", "return", "self", "super",
    "true", "then", "while"
};

static const u8 DemoRom[] =
{
    #include "../build/assets/miniscriptdemo.tic.dat"
};

static const u8 MarkRom[] =
{
    #include "../build/assets/miniscriptmark.tic.dat"
};

extern "C" TIC_EXPORT const tic_script EXPORT_SCRIPT(MiniScript) =
{
    .id                     = 0x55,
    .name                   = "miniscript",
    .fileExtension          = ".ms",
    .projectComment         = "//",
    .init                 = initMiniScript,
    .close                = closeMiniScript,
    .tick                 = callMiniScriptTick,
    .boot                 = callMiniScriptBoot,

    .callback             =
    {
        .scanline           = callMiniScriptScanline,
        .border             = callMiniScriptBorder,
        .menu               = callMiniScriptMenu,
    },

    .stdStringStartEnd      = "\"",
    .singleComment          = "//",

    .keywords               = MiniScriptKeywords,
    .keywordsCount          = COUNT_OF(MiniScriptKeywords),

    .demo = { DemoRom, sizeof(DemoRom) },
    .mark = { MarkRom, sizeof(MarkRom), "miniscriptmark.tic" },
};