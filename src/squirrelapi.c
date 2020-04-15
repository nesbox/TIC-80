// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

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

#include "machine.h"

#if defined(TIC_BUILD_WITH_SQUIRREL)

//#define USE_FOREIGN_POINTER
//#define CHECK_FORCE_EXIT

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <squirrel.h>
#include <sqstdmath.h>
#include <sqstdstring.h>
#include <sqstdblob.h>
#include <ctype.h>

static const char TicMachine[] = "_TIC80";


// !TODO: get rid of this wrap
static s32 getSquirrelNumber(HSQUIRRELVM vm, s32 index)
{
    SQInteger i;
    if (SQ_SUCCEEDED(sq_getinteger(vm, index, &i)))
        return (s32)i;
    
    SQFloat f;
    if (SQ_SUCCEEDED(sq_getfloat(vm, index, &f)))
        return (s32)f;
    
    return 0;
}

static void registerSquirrelFunction(tic_machine* machine, SQFUNCTION func, const char *name)
{
    sq_pushroottable(machine->squirrel);
    sq_pushstring(machine->squirrel, name, -1);
    sq_newclosure(machine->squirrel, func, 0);
    sq_newslot(machine->squirrel, -3, SQTrue);
    sq_poptop(machine->squirrel); // remove root table.
}

static tic_machine* getSquirrelMachine(HSQUIRRELVM vm)
{
#if USE_FOREIGN_POINTER
    return (tic_machine*)sq_getforeignpointer(vm);
#else
    sq_pushregistrytable(vm);
    sq_pushstring(vm, TicMachine, -1);
    if (SQ_FAILED(sq_get(vm, -2)))
    {
        fprintf(stderr, "FATAL ERROR: TicMachine not found!\n");
        abort();
    }
    SQUserPointer ptr;
    if (SQ_FAILED(sq_getuserpointer(vm, -1, &ptr)))
    {
        fprintf(stderr, "FATAL ERROR: Cannot get user pointer for TicMachine!\n");
        abort();
    }
    tic_machine* machine = (tic_machine*)ptr;
    sq_pop(vm, 2); // user pointer and registry table.
    return machine;
#endif
}

void squirrel_compilerError(HSQUIRRELVM vm, const SQChar* desc, const SQChar* source, 
                             SQInteger line, SQInteger column)
{
    tic_machine* machine = getSquirrelMachine(vm);
    char buffer[1024];
    snprintf(buffer, 1023, "%.40s line %.6d column %.6d: %s\n", source, (int)line, (int)column, desc);
    
    if (machine->data)
        machine->data->error(machine->data->data, buffer);
}

static SQInteger squirrel_errorHandler(HSQUIRRELVM vm)
{
    tic_machine* machine = getSquirrelMachine(vm);

    SQStackInfos si;
    SQInteger level = 0;
    while (SQ_SUCCEEDED(sq_stackinfos(vm, level, &si)))
    {
        char buffer[100];
        snprintf(buffer, 99, "%.40s %.40s %.6d\n", si.funcname, si.source, (int)si.line);
        
        if (machine->data)
            machine->data->error(machine->data->data, buffer);
        ++level;
    }

    return 0;
}


static SQInteger squirrel_peek(HSQUIRRELVM vm)
{
    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

    // check number of args
    if (sq_gettop(vm) != 2)
        return sq_throwerror(vm, "invalid parameters, peek(address)");
    s32 address = getSquirrelNumber(vm, 2);

    sq_pushinteger(vm, tic_api_peek(tic, address));
    return 1;
}

static SQInteger squirrel_poke(HSQUIRRELVM vm)
{
    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

    if (sq_gettop(vm) != 3)
        return sq_throwerror( vm, "invalid parameters, poke(address,value)" );

    s32 address = getSquirrelNumber(vm, 2);
    u8 value = getSquirrelNumber(vm, 3);

    tic_api_poke(tic, address, value);

    return 0;
}

static SQInteger squirrel_peek4(HSQUIRRELVM vm)
{
    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

    // check number of args
    if (sq_gettop(vm) != 2)
        return sq_throwerror(vm, "invalid parameters, peek4(address)");
    s32 address = getSquirrelNumber(vm, 2);

    sq_pushinteger(vm, tic_api_peek4(tic, address));
    return 1;
}

static SQInteger squirrel_poke4(HSQUIRRELVM vm)
{
    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

    if (sq_gettop(vm) != 3)
        return sq_throwerror( vm, "invalid parameters, poke4(address,value)" );

    s32 address = getSquirrelNumber(vm, 2);
    u8 value = getSquirrelNumber(vm, 3);

    tic_api_poke4(tic, address, value);

    return 0;
}

static SQInteger squirrel_cls(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

    tic_api_cls(tic, top == 2 ? getSquirrelNumber(vm, 2) : 0);

    return 0;
}

static SQInteger squirrel_pix(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if(top >= 3)
    {
        s32 x = getSquirrelNumber(vm, 2);
        s32 y = getSquirrelNumber(vm, 3);
        
        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

        if(top >= 4)
        {
            s32 color = getSquirrelNumber(vm, 4);
            tic_api_pix(tic, x, y, color, false);
        }
        else
        {
            sq_pushinteger(vm, tic_api_pix(tic, x, y, 0, true));
            return 1;
        }

    }
    else return sq_throwerror(vm, "invalid parameters, pix(x y [color])\n");

    return 0;
}



static SQInteger squirrel_line(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if(top == 6)
    {
        s32 x0 = getSquirrelNumber(vm, 2);
        s32 y0 = getSquirrelNumber(vm, 3);
        s32 x1 = getSquirrelNumber(vm, 4);
        s32 y1 = getSquirrelNumber(vm, 5);
        s32 color = getSquirrelNumber(vm, 6);

        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

        tic_api_line(tic, x0, y0, x1, y1, color);
    }
    else return sq_throwerror(vm, "invalid parameters, line(x0,y0,x1,y1,color)\n");

    return 0;
}

static SQInteger squirrel_rect(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if(top == 6)
    {
        s32 x = getSquirrelNumber(vm, 2);
        s32 y = getSquirrelNumber(vm, 3);
        s32 w = getSquirrelNumber(vm, 4);
        s32 h = getSquirrelNumber(vm, 5);
        s32 color = getSquirrelNumber(vm, 6);

        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

        tic_api_rect(tic, x, y, w, h, color);
    }
    else return sq_throwerror(vm, "invalid parameters, rect(x,y,w,h,color)\n");

    return 0;
}

static SQInteger squirrel_rectb(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if(top == 6)
    {
        s32 x = getSquirrelNumber(vm, 2);
        s32 y = getSquirrelNumber(vm, 3);
        s32 w = getSquirrelNumber(vm, 4);
        s32 h = getSquirrelNumber(vm, 5);
        s32 color = getSquirrelNumber(vm, 6);

        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

        tic_api_rectb(tic, x, y, w, h, color);
    }
    else return sq_throwerror(vm, "invalid parameters, rectb(x,y,w,h,color)\n");

    return 0;
}

static SQInteger squirrel_circ(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if(top == 5)
    {
        s32 radius = getSquirrelNumber(vm, 4);
        if(radius < 0) return 0;
        
        s32 x = getSquirrelNumber(vm, 2);
        s32 y = getSquirrelNumber(vm, 3);
        s32 color = getSquirrelNumber(vm, 5);

        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

        tic_api_circ(tic, x, y, radius, color);
    }
    else return sq_throwerror(vm, "invalid parameters, circ(x,y,radius,color)\n");

    return 0;
}

static SQInteger squirrel_circb(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if(top == 5)
    {
        s32 radius = getSquirrelNumber(vm, 4);
        if(radius < 0) return 0;

        s32 x = getSquirrelNumber(vm, 2);
        s32 y = getSquirrelNumber(vm, 3);
        s32 color = getSquirrelNumber(vm, 5);

        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

        tic_api_circb(tic, x, y, radius, color);
    }
    else return sq_throwerror(vm, "invalid parameters, circb(x,y,radius,color)\n");

    return 0;
}

static SQInteger squirrel_tri(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if(top == 8)
    {
        s32 pt[6];

        for(s32 i = 0; i < COUNT_OF(pt); i++)
            pt[i] = getSquirrelNumber(vm, i+2);
        
        s32 color = getSquirrelNumber(vm, 8);

        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

        tic_api_tri(tic, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);
    }
    else return sq_throwerror(vm, "invalid parameters, tri(x1,y1,x2,y2,x3,y3,color)\n");

    return 0;
}

static SQInteger squirrel_textri(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if (top >= 13)
    {
        float pt[12];

        for (s32 i = 0; i < COUNT_OF(pt); i++)
        {
            SQFloat f = 0.0;
            sq_getfloat(vm, i + 2, &f);
            pt[i] = (float)f;
        }

        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);
        u8 chroma = 0xff;
        bool use_map = false;

        //  check for use map 
        if (top >= 14)
        {
            SQBool b = SQFalse;
            sq_getbool(vm, 14, &b);
            use_map = (b != SQFalse);
        }
        //  check for chroma 
        if (top >= 15)
            chroma = (u8)getSquirrelNumber(vm, 15);

        tic_api_textri(tic, pt[0], pt[1],   //  xy 1
                                    pt[2], pt[3],   //  xy 2
                                    pt[4], pt[5],   //  xy 3
                                    pt[6], pt[7],   //  uv 1
                                    pt[8], pt[9],   //  uv 2
                                    pt[10], pt[11], //  uv 3
                                    use_map,        // use map
                                    chroma);        // chroma
    }
    else return sq_throwerror(vm, "invalid parameters, textri(x1,y1,x2,y2,x3,y3,u1,v1,u2,v2,u3,v3,[use_map=false],[chroma=off])\n");
    return 0;
}


static SQInteger squirrel_clip(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if(top == 1)
    {
        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

        tic_api_clip(tic, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
    }
    else if(top == 5)
    {
        s32 x = getSquirrelNumber(vm, 2);
        s32 y = getSquirrelNumber(vm, 3);
        s32 w = getSquirrelNumber(vm, 4);
        s32 h = getSquirrelNumber(vm, 5);

        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

        tic_api_clip((tic_mem*)getSquirrelMachine(vm), x, y, w, h);
    }
    else return sq_throwerror(vm, "invalid parameters, use clip(x,y,w,h) or clip()\n");

    return 0;
}

static SQInteger squirrel_btnp(HSQUIRRELVM vm)
{
    tic_machine* machine = getSquirrelMachine(vm);
    tic_mem* tic = (tic_mem*)machine;

    SQInteger top = sq_gettop(vm);

    if (top == 1)
    {
        sq_pushinteger(vm, tic_api_btnp(tic, -1, -1, -1));
    }
    else if(top == 2)
    {
        s32 index = getSquirrelNumber(vm, 2) & 0x1f;

        sq_pushbool(vm, (tic_api_btnp(tic, index, -1, -1) ? SQTrue : SQFalse));
    }
    else if (top == 4)
    {
        s32 index = getSquirrelNumber(vm, 2) & 0x1f;
        u32 hold = getSquirrelNumber(vm, 3);
        u32 period = getSquirrelNumber(vm, 4);

        sq_pushbool(vm, (tic_api_btnp(tic, index, hold, period) ? SQTrue : SQFalse));
    }
    else
    {
        return sq_throwerror(vm, "invalid params, btnp [ id [ hold period ] ]\n");
    }

    return 1;
}

static SQInteger squirrel_btn(HSQUIRRELVM vm)
{
    tic_machine* machine = getSquirrelMachine(vm);

    SQInteger top = sq_gettop(vm);

    if (top == 1)
    {
        sq_pushinteger(vm, machine->memory.ram.input.gamepads.data);
    }
    else if (top == 2)
    {
        u32 index = getSquirrelNumber(vm, 2) & 0x1f;
        bool pressed = (machine->memory.ram.input.gamepads.data & (1 << index)) != 0;
        sq_pushbool(vm, pressed ? SQTrue : SQFalse);
    }
    else
    {
        return sq_throwerror(vm, "invalid params, btn [ id ]\n");
    } 

    return 1;
}

static SQInteger squirrel_spr(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    s32 index = 0;
    s32 x = 0;
    s32 y = 0;
    s32 w = 1;
    s32 h = 1;
    s32 scale = 1;
    tic_flip flip = tic_no_flip;
    tic_rotate rotate = tic_no_rotate;
    static u8 colors[TIC_PALETTE_SIZE];
    s32 count = 0;

    if(top >= 2) 
    {
        index = getSquirrelNumber(vm, 2);

        if(top >= 4)
        {
            x = getSquirrelNumber(vm, 3);
            y = getSquirrelNumber(vm, 4);

            if(top >= 5)
            {
                if(OT_ARRAY == sq_gettype(vm, 5))
                {
                    for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
                    {
                        sq_pushinteger(vm, (SQInteger)i);
                        sq_rawget(vm, 5);
                        if(sq_gettype(vm, -1) & (OT_FLOAT|OT_INTEGER))
                        {
                            colors[i-1] = getSquirrelNumber(vm, -1);
                            count++;
                            sq_poptop(vm);
                        }
                        else
                        {
                            sq_poptop(vm);
                            break;
                        }
                    }
                }
                else 
                {
                    colors[0] = getSquirrelNumber(vm, 5);
                    count = 1;
                }

                if(top >= 6)
                {
                    scale = getSquirrelNumber(vm, 6);

                    if(top >= 7)
                    {
                        flip = getSquirrelNumber(vm, 7);

                        if(top >= 8)
                        {
                            rotate = getSquirrelNumber(vm, 8);

                            if(top >= 10)
                            {
                                w = getSquirrelNumber(vm, 9);
                                h = getSquirrelNumber(vm, 10);
                            }
                        }
                    }
                }
            }
        }
    }

    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

    tic_api_spr(tic, &tic->ram.tiles, index, x, y, w, h, colors, count, scale, flip, rotate);

    return 0;
}

static SQInteger squirrel_mget(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if(top == 3)
    {
        s32 x = getSquirrelNumber(vm, 2);
        s32 y = getSquirrelNumber(vm, 3);

        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

        u8 value = tic_api_mget(tic, &tic->ram.map, x, y);
        sq_pushinteger(vm, value);
        return 1;
    }
    else return sq_throwerror(vm, "invalid params, mget(x,y)\n");

    return 0;
}

static SQInteger squirrel_mset(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if(top == 4)
    {
        s32 x = getSquirrelNumber(vm, 2);
        s32 y = getSquirrelNumber(vm, 3);
        u8 val = getSquirrelNumber(vm, 4);

        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

        tic_api_mset(tic, &tic->ram.map, x, y, val);
    }
    else return sq_throwerror(vm, "invalid params, mget(x,y)\n");

    return 0;
}

typedef struct
{
    HSQUIRRELVM vm;
    HSQOBJECT reg;
} RemapData;

static void remapCallback(void* data, s32 x, s32 y, RemapResult* result)
{
    RemapData* remap = (RemapData*)data;
    HSQUIRRELVM vm = remap->vm;

    SQInteger top = sq_gettop(vm);
    
    sq_pushobject(vm, remap->reg);
    sq_pushroottable(vm);
    sq_pushinteger(vm, result->index);
    sq_pushinteger(vm, x);
    sq_pushinteger(vm, y);
    //lua_pcall(lua, 3, 3, 0);

    if (SQ_SUCCEEDED(sq_call(vm, 4, SQTrue, SQTrue)))
    {
        sq_pushinteger(vm, 0);
        if (SQ_SUCCEEDED(sq_get(vm, -2)))
        {
            result->index = getSquirrelNumber(vm, -1);
            sq_poptop(vm);
            sq_pushinteger(vm, 1);
            if (SQ_SUCCEEDED(sq_get(vm, -2)))
            {
                result->flip = getSquirrelNumber(vm, -1);
                sq_poptop(vm);
                sq_pushinteger(vm, 2);
                if (SQ_SUCCEEDED(sq_get(vm, -2)))
                {
                    result->rotate = getSquirrelNumber(vm, -1);
                    sq_poptop(vm);
                }
            }
        }
    }

    sq_settop(vm, top);
}

static SQInteger squirrel_map(HSQUIRRELVM vm)
{
    s32 x = 0;
    s32 y = 0;
    s32 w = TIC_MAP_SCREEN_WIDTH;
    s32 h = TIC_MAP_SCREEN_HEIGHT;
    s32 sx = 0;
    s32 sy = 0;
    u8 chromakey = -1;
    s32 scale = 1;

    SQInteger top = sq_gettop(vm);

    if(top >= 3) 
    {
        x = getSquirrelNumber(vm, 2);
        y = getSquirrelNumber(vm, 3);

        if(top >= 5)
        {
            w = getSquirrelNumber(vm, 4);
            h = getSquirrelNumber(vm, 5);

            if(top >= 7)
            {
                sx = getSquirrelNumber(vm, 6);
                sy = getSquirrelNumber(vm, 7);

                if(top >= 8)
                {
                    chromakey = getSquirrelNumber(vm, 8);

                    if(top >= 9)
                    {
                        scale = getSquirrelNumber(vm, 9);

                        if(top >= 10)
                        {
                            SQObjectType type = sq_gettype(vm, 10);
                            if (type & (OT_CLOSURE|OT_NATIVECLOSURE|OT_INSTANCE))
                            {
                                RemapData data = {vm};
                                sq_resetobject(&data.reg);
                                sq_getstackobj(vm, 10, &data.reg);
                                sq_addref(vm, &data.reg);

                                tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

                                tic_api_map(tic, &tic->ram.map, &tic->ram.tiles, x, y, w, h, sx, sy, chromakey, scale, remapCallback, &data);

                                //luaL_unref(lua, LUA_REGISTRYINDEX, data.reg);
                                sq_release(vm, &data.reg);

                                return 0;
                            }                           
                        }
                    }
                }
            }
        }
    }

    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

    tic_api_map((tic_mem*)getSquirrelMachine(vm), &tic->ram.map, &tic->ram.tiles, x, y, w, h, sx, sy, chromakey, scale, NULL, NULL);

    return 0;
}

static SQInteger squirrel_music(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);
    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

    if(top == 1) tic_api_music(tic, -1, 0, 0, false);
    else if(top >= 2)
    {
        tic_api_music(tic, -1, 0, 0, false);

        s32 track = getSquirrelNumber(vm, 2);
        s32 frame = -1;
        s32 row = -1;
        bool loop = true;

        if(top >= 3)
        {
            frame = getSquirrelNumber(vm, 3);

            if(top >= 4)
            {
                row = getSquirrelNumber(vm, 4);

                if(top >= 5)
                {
                    SQBool b = SQFalse;
                    sq_getbool(vm, 5, &b);
                    loop = (b != SQFalse);
                }
            }
        }

        tic_api_music(tic, track, frame, row, loop);
    }
    else return sq_throwerror(vm, "invalid params, use music(track)\n");

    return 0;
}

static SQInteger squirrel_sfx(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if(top >= 2)
    {
        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

        s32 note = -1;
        s32 octave = -1;
        s32 duration = -1;
        s32 channel = 0;
        s32 volume = MAX_VOLUME;
        s32 speed = SFX_DEF_SPEED;

        s32 index = getSquirrelNumber(vm, 2);

        if(index < SFX_COUNT)
        {
            if (index >= 0)
            {
                tic_sample* effect = tic->ram.sfx.samples.data + index;

                note = effect->note;
                octave = effect->octave;
                speed = effect->speed;
            }

            if(top >= 3)
            {
                if(sq_gettype(vm, 3) & (OT_INTEGER|OT_FLOAT))
                {
                    s32 id = getSquirrelNumber(vm, 3);
                    note = id % NOTES;
                    octave = id / NOTES;
                }
                else if(sq_gettype(vm, 3) == OT_STRING)
                {
                    const SQChar* str;
                    sq_getstring(vm, 3, &str);
                    const char* noteStr = (const char*)str;

                    if(!tic_tool_parse_note(noteStr, &note, &octave))
                    {
                        return sq_throwerror(vm, "invalid note, should be like C#4\n");
                    }
                }

                if(top >= 4)
                {
                    duration = getSquirrelNumber(vm, 4);

                    if(top >= 5)
                    {
                        channel = getSquirrelNumber(vm, 5);

                        if(top >= 6)
                        {
                            volume = getSquirrelNumber(vm, 6);

                            if(top >= 7)
                            {
                                speed = getSquirrelNumber(vm, 7);
                            }
                        }
                    }                   
                }
            }

            if (channel >= 0 && channel < TIC_SOUND_CHANNELS)
            {
                tic_api_sfx(tic, index, note, octave, duration, channel, volume & 0xf, speed);
            }
            else return sq_throwerror(vm, "unknown channel\n");
        }
        else return sq_throwerror(vm, "unknown sfx index\n");
    }
    else return sq_throwerror(vm, "invalid sfx params\n");

    return 0;
}

static SQInteger squirrel_sync(HSQUIRRELVM vm)
{
    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

    bool toCart = false;
    u32 mask = 0;
    s32 bank = 0;

    if(sq_gettop(vm) >= 2)
    {
        mask = getSquirrelNumber(vm, 2);

        if(sq_gettop(vm) >= 3)
        {
            bank = getSquirrelNumber(vm, 3);

            if(sq_gettop(vm) >= 4)
            {
                SQBool b = SQFalse;
                sq_getbool(vm, 4, &b);
                toCart = (b != SQFalse);
            }
        }
    }

    if(bank >= 0 && bank < TIC_BANKS)
        tic_api_sync(tic, mask, bank, toCart);
    else
        return sq_throwerror(vm, "sync() error, invalid bank");

    return 0;
}

static SQInteger squirrel_reset(HSQUIRRELVM vm)
{
    tic_machine* machine = getSquirrelMachine(vm);

    machine->state.initialized = false;

    return 0;
}

static SQInteger squirrel_key(HSQUIRRELVM vm)
{
    tic_machine* machine = getSquirrelMachine(vm);
    tic_mem* tic = &machine->memory;

    SQInteger top = sq_gettop(vm);

    if (top == 1)
    {
        sq_pushbool(vm, tic_api_key(tic, tic_key_unknown) ? SQTrue : SQFalse);
    }
    else if (top == 2)
    {
        tic_key key = getSquirrelNumber(vm, 2);

        if(key < tic_key_escape)
            sq_pushbool(vm, tic_api_key(tic, key) ? SQTrue : SQFalse);
        else
        {
            return sq_throwerror(vm, "unknown keyboard code\n");
        }
    }
    else
    {
        return sq_throwerror(vm, "invalid params, key [code]\n");
    } 

    return 1;
}

static SQInteger squirrel_keyp(HSQUIRRELVM vm)
{
    tic_machine* machine = getSquirrelMachine(vm);
    tic_mem* tic = &machine->memory;

    SQInteger top = sq_gettop(vm);

    if (top == 1)
    {
        sq_pushbool(vm, tic_api_keyp(tic, tic_key_unknown, -1, -1) ? SQTrue : SQFalse);
    }
    else
    {
        tic_key key = getSquirrelNumber(vm, 2);

        if(key >= tic_key_escape)
        {
            return sq_throwerror(vm, "unknown keyboard code\n");
        }
        else
        {
            if(top == 2)
            {
                sq_pushbool(vm, tic_api_keyp(tic, key, -1, -1) ? SQTrue : SQFalse);
            }
            else if(top == 4)
            {
                u32 hold = getSquirrelNumber(vm, 3);
                u32 period = getSquirrelNumber(vm, 4);

                sq_pushbool(vm, tic_api_keyp(tic, key, hold, period) ? SQTrue : SQFalse);
            }
            else
            {
                return sq_throwerror(vm, "invalid params, keyp [ code [ hold period ] ]\n");
            }
        }
    }

    return 1;
}

static SQInteger squirrel_memcpy(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if(top == 4)
    {
        s32 dest = getSquirrelNumber(vm, 2);
        s32 src = getSquirrelNumber(vm, 3);
        s32 size = getSquirrelNumber(vm, 4);

        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);
        tic_api_memcpy(tic, dest, src, size);
        return 0;
    }

    return sq_throwerror(vm, "invalid params, memcpy(dest,src,size)\n");
}

static SQInteger squirrel_memset(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if(top == 4)
    {
        s32 dest = getSquirrelNumber(vm, 2);
        u8 value = getSquirrelNumber(vm, 3);
        s32 size = getSquirrelNumber(vm, 4);

        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);
        tic_api_memset(tic, dest, value, size);
        return 0;
    }

    return sq_throwerror(vm, "invalid params, memset(dest,val,size)\n");
}

// NB we leave the string on the stack so that the char* pointer remains valid.
static const char* printString(HSQUIRRELVM vm, s32 index)
{
    const SQChar* text = "";
    if (SQ_SUCCEEDED(sq_tostring(vm, index)))
    {
        sq_getstring(vm, -1, &text);
    }
    
    return (const char*)text;
}

static SQInteger squirrel_font(HSQUIRRELVM vm)
{
    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);
    SQInteger top = sq_gettop(vm);

    if(top >= 2)
    {
        const char* text = printString(vm, 2);
        s32 x = 0;
        s32 y = 0;
        s32 width = TIC_SPRITESIZE;
        s32 height = TIC_SPRITESIZE;
        u8 chromakey = 0;
        bool fixed = false;
                bool alt = false;
        s32 scale = 1;

        if(top >= 4)
        {
            x = getSquirrelNumber(vm, 3);
            y = getSquirrelNumber(vm, 4);

            if(top >= 5)
            {
                chromakey = getSquirrelNumber(vm, 5);

                if(top >= 7)
                {
                    width = getSquirrelNumber(vm, 6);
                    height = getSquirrelNumber(vm, 7);

                    if(top >= 8)
                    {
                        SQBool b = SQFalse;
                        sq_getbool(vm, 8, &b);
                        fixed = (b != SQFalse);

                        if(top >= 9)
                        {
                            scale = getSquirrelNumber(vm, 9);
                                                        
                                                        if (top >= 10)
                                                        {
                                                            SQBool b = SQFalse;
                                                            sq_getbool(vm, 10, &b);
                                                            alt = (b != SQFalse);
                                                        }
                                                        
                        }
                    }
                }
            }
        }

        if(scale == 0)
        {
            sq_pushinteger(vm, 0);
            return 1;
        }

        s32 size = tic_api_font(tic, text, x, y, chromakey, width, height, fixed, scale, alt);

        sq_pushinteger(vm, size);
        return 1;
    }

    return 0;
}

static SQInteger squirrel_print(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);

    if(top >= 2) 
    {
        tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

        s32 x = 0;
        s32 y = 0;
        s32 color = TIC_PALETTE_SIZE-1;
        bool fixed = false;
                bool alt = false;
        s32 scale = 1;

        const char* text = printString(vm, 2);

        if(top >= 4)
        {
            x = getSquirrelNumber(vm, 3);
            y = getSquirrelNumber(vm, 4);

            if(top >= 5)
            {
                color = getSquirrelNumber(vm, 5) % TIC_PALETTE_SIZE;

                if(top >= 6)
                {
                    SQBool b = SQFalse;
                    sq_getbool(vm, 5, &b);
                    fixed = (b != SQFalse);

                    if(top >= 7)
                    {
                        scale = getSquirrelNumber(vm, 7);
                                                
                                                if (top >= 8)
                                                {
                                                    SQBool b = SQFalse;
                                                    sq_getbool(vm, 8, &b);
                                                    alt = (b != SQFalse);
                                                }
                    }
                }
            }
        }

        if(scale == 0)
        {
            sq_pushinteger(vm, 0);
            return 1;
        }

        s32 size = tic_api_print(tic, text ? text : "nil", x, y, color, fixed, scale, alt);

        sq_pushinteger(vm, size);

        return 1;
    }

    return 0;
}

static SQInteger squirrel_trace(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);
    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

    if(top >= 2)
    {
        const char* text = printString(vm, 2);
        u8 color = tic_color_12;

        if(top >= 3)
        {
            color = getSquirrelNumber(vm, 3);
        }

        tic_api_trace(tic, text, color);
    }

    return 0;
}

static SQInteger squirrel_pmem(HSQUIRRELVM vm)
{
    SQInteger top = sq_gettop(vm);
    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

    if(top >= 2)
    {
        u32 index = getSquirrelNumber(vm, 2);

        if(index < TIC_PERSISTENT_SIZE)
        {
            u32 val = tic_api_pmem(tic, index, 0, false);

            if(top >= 3)
            {
                SQInteger i = 0;
                sq_getinteger(vm, 3, &i);
                tic_api_pmem(tic, index, (u32)i, true);
            }

            sq_pushinteger(vm, val);

            return 1;           
        }
        return sq_throwerror(vm, "invalid persistent tic index\n");
    }
    else return sq_throwerror(vm, "invalid params, pmem(index [val]) -> val\n");

    return 0;
}

static SQInteger squirrel_time(HSQUIRRELVM vm)
{
    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);
    
    sq_pushfloat(vm, (SQFloat)(tic_api_time(tic)));

    return 1;
}

static SQInteger squirrel_tstamp(HSQUIRRELVM vm)
{
    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);
    
    sq_pushinteger(vm, (SQFloat)(tic_api_tstamp(tic)));

    return 1;
}

static SQInteger squirrel_exit(HSQUIRRELVM vm)
{
    tic_api_exit((tic_mem*)getSquirrelMachine(vm));
    
    return 0;
}

static SQInteger squirrel_mouse(HSQUIRRELVM vm)
{
    tic_machine* machine = getSquirrelMachine(vm);

    const tic80_mouse* mouse = &machine->memory.ram.input.mouse;

    sq_newarray(vm, 7);
    sq_pushinteger(vm, mouse->x);
    sq_arrayappend(vm, -2);
    sq_pushinteger(vm, mouse->y);
    sq_arrayappend(vm, -2);
    sq_pushbool(vm, mouse->left ? SQTrue : SQFalse);
    sq_arrayappend(vm, -2);
    sq_pushbool(vm, mouse->middle ? SQTrue : SQFalse);
    sq_arrayappend(vm, -2);
    sq_pushbool(vm, mouse->right ? SQTrue : SQFalse);
    sq_arrayappend(vm, -2);
    sq_pushinteger(vm, mouse->scrollx);
    sq_arrayappend(vm, -2);
    sq_pushinteger(vm, mouse->scrolly);
    sq_arrayappend(vm, -2);

    return 1;
}

static SQInteger squirrel_fget(HSQUIRRELVM vm)
{
    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

    SQInteger top = sq_gettop(vm);

    if(top >= 2)
    {
        u32 index = getSquirrelNumber(vm, 2);

        if(top >= 3)
        {
            u32 flag = getSquirrelNumber(vm, 3);
            sq_pushbool(vm, tic_api_fget(tic, index, flag));
            return 1;
        }
    }

    sq_throwerror(vm, "invalid params, fget(index, flag) -> val\n");

    return 0;
}

static SQInteger squirrel_fset(HSQUIRRELVM vm)
{
    tic_mem* tic = (tic_mem*)getSquirrelMachine(vm);

    SQInteger top = sq_gettop(vm);

    if(top >= 2)
    {
        u32 index = getSquirrelNumber(vm, 2);

        if(top >= 3)
        {
            u32 flag = getSquirrelNumber(vm, 3);

            if(top >= 4)
            {
                SQBool value = SQFalse;
                sq_getbool(vm, 4, &value);

                tic_api_fset(tic, index, flag, value);
                return 0;               
            }
        }
    }

    sq_throwerror(vm, "invalid params, fset(index, flag, value)\n");

    return 0;
}

static SQInteger squirrel_dofile(HSQUIRRELVM vm)
{
    return sq_throwerror(vm, "unknown method: \"dofile\"\n");
}

static SQInteger squirrel_loadfile(HSQUIRRELVM vm)
{
    return sq_throwerror(vm, "unknown method: \"loadfile\"\n");
}

static void squirrel_open_builtins(HSQUIRRELVM vm)
{
    sq_pushroottable(vm);
    sqstd_register_mathlib(vm);
    sqstd_register_stringlib(vm);
    sqstd_register_bloblib(vm);
    sq_poptop(vm);
}

static void checkForceExit(HSQUIRRELVM vm, SQInteger type, const SQChar* sourceName, SQInteger line, const SQChar* functionName)
{
    tic_machine* machine = getSquirrelMachine(vm);
    tic_tick_data* tick = machine->data;

    if(tick && tick->forceExit && tick->forceExit(tick->data))
        sq_throwerror(vm, "script execution was interrupted");
}

static void initAPI(tic_machine* machine)
{
    HSQUIRRELVM vm = machine->squirrel;
    
    sq_setcompilererrorhandler(vm, squirrel_compilerError);
        
    sq_pushregistrytable(vm);
    sq_pushstring(vm, TicMachine, -1);
    sq_pushuserpointer(machine->squirrel, machine);
    sq_newslot(vm, -3, SQTrue);
    sq_poptop(vm);

#if USE_FOREIGN_POINTER
    sq_setforeignptr(vm, machine);
#endif

#define API_FUNC_DEF(name, ...) {squirrel_ ## name, #name},
    static const struct{SQFUNCTION func; const char* name;} ApiItems[] = {TIC_API_LIST(API_FUNC_DEF)};
#undef API_FUNC_DEF

    for (s32 i = 0; i < COUNT_OF(ApiItems); i++)
        registerSquirrelFunction(machine, ApiItems[i].func, ApiItems[i].name);

    registerSquirrelFunction(machine, squirrel_dofile, "dofile");
    registerSquirrelFunction(machine, squirrel_loadfile, "loadfile");

#if CHECK_FORCE_EXIT
    sq_setnativedebughook(vm, checkForceExit);
#endif
    sq_enabledebuginfo(vm, SQTrue);

}

static void closeSquirrel(tic_mem* tic)
{
    tic_machine* machine = (tic_machine*)tic;

    if(machine->squirrel)
    {
        sq_close(machine->squirrel);
        machine->squirrel = NULL;
    }
}

static bool initSquirrel(tic_mem* tic, const char* code)
{
    tic_machine* machine = (tic_machine*)tic;

    closeSquirrel(tic);

    HSQUIRRELVM vm = machine->squirrel = sq_open(100);
    squirrel_open_builtins(vm);

    sq_newclosure(vm, squirrel_errorHandler, 0);
    sq_seterrorhandler(vm);

    initAPI(machine);

    {
        HSQUIRRELVM vm = machine->squirrel;

        sq_settop(vm, 0);

        if((SQ_FAILED(sq_compilebuffer(vm, code, strlen(code), "squirrel", SQTrue))) || 
            (sq_pushroottable(vm), false) ||
            (SQ_FAILED(sq_call(vm, 1, SQFalse, SQTrue))))
        {
            sq_getlasterror(vm);
            sq_tostring(vm, -1);
            const SQChar* errorString = "unknown error";
            sq_getstring(vm, -1, &errorString);

            if (machine->data)
                machine->data->error(machine->data->data, errorString);
            
            sq_pop(vm, 2); // error and error string

            return false;
        }
    }

    return true;
}

static void callSquirrelTick(tic_mem* tic)
{
    tic_machine* machine = (tic_machine*)tic;

    HSQUIRRELVM vm = machine->squirrel;

    if(vm)
    {
        sq_pushroottable(vm);
        sq_pushstring(vm, TIC_FN, -1);

        if (SQ_SUCCEEDED(sq_get(vm, -2)))
        {
            sq_pushroottable(vm);
            if(SQ_FAILED(sq_call(vm, 1, SQFalse, SQTrue)))
            {
                sq_getlasterror(vm);
                sq_tostring(vm, -1);
                const SQChar* errorString = "unknown error";
                sq_getstring(vm, -1, &errorString);

                if (machine->data)
                    machine->data->error(machine->data->data, errorString);
                sq_pop(vm, 3); // remove string, error and root table.
            }
        }
        else 
        {       
            sq_pop(vm, 1);
            if (machine->data)
                machine->data->error(machine->data->data, "'function TIC()...' isn't found :(");
        }
    }
}

static void callSquirrelScanlineName(tic_mem* tic, s32 row, void* data, const char* name)
{
    tic_machine* machine = (tic_machine*)tic;
    HSQUIRRELVM vm = machine->squirrel;

    if (vm)
    {
        sq_pushroottable(vm);
        sq_pushstring(vm, name, -1);
        if (SQ_SUCCEEDED(sq_get(vm, -2)))
        {
            sq_pushroottable(vm);
            sq_pushinteger(vm, row);

            if(SQ_FAILED(sq_call(vm, 2, SQFalse, SQTrue)))
            {
                sq_getlasterror(vm);
                sq_tostring(vm, -1);

                const SQChar* errorString = "unknown error";
                sq_getstring(vm, -1, &errorString);
                if (machine->data)
                    machine->data->error(machine->data->data, errorString);
                sq_pop(vm, 3); // error string, error and root table
            }
        }
        else sq_poptop(vm);
    }
}

static void callSquirrelScanline(tic_mem* tic, s32 row, void* data)
{
    callSquirrelScanlineName(tic, row, data, SCN_FN);

    // try to call old scanline
    callSquirrelScanlineName(tic, row, data, "scanline");
}

static void callSquirrelOverline(tic_mem* tic, void* data)
{
    tic_machine* machine = (tic_machine*)tic;
    HSQUIRRELVM vm = machine->squirrel;

    if (vm)
    {
        const char* OvrFunc = OVR_FN;

        sq_pushroottable(vm);
        sq_pushstring(vm, OvrFunc, -1);
        
        if(SQ_SUCCEEDED(sq_get(vm, -2))) 
        {
            sq_pushroottable(vm);
            if(SQ_FAILED(sq_call(vm, 1, SQFalse, SQTrue)))
            {
                sq_getlasterror(vm);
                sq_tostring(vm, -1);
                const SQChar* errorString = "unknown error";
                sq_getstring(vm, -1, &errorString);
                if (machine->data)
                    machine->data->error(machine->data->data, errorString);
                sq_pop(vm, 3);
            }
        }
        else sq_poptop(vm);
    }

}

static const char* const SquirrelKeywords [] =
{
    "base", "break", "case", "catch", "class", "clone",
    "continue", "const", "default", "delete", "else", "enum",
    "extends", "for", "foreach", "function", "if", "in",
    "local", "null", "resume", "return", "switch", "this",
    "throw", "try", "typeof", "while", "yield", "constructor",
    "instanceof", "true", "false", "static", "__LINE__", "__FILE__"
};

static inline bool isalnum_(char c) {return isalnum(c) || c == '_';}

static const tic_outline_item* getSquirrelOutline(const char* code, s32* size)
{
    enum{Size = sizeof(tic_outline_item)};

    *size = 0;

    static tic_outline_item* items = NULL;

    if(items)
    {
        free(items);
        items = NULL;
    }

    const char* ptr = code;

    while(true)
    {
        static const char FuncString[] = "function ";

        ptr = strstr(ptr, FuncString);

        if(ptr)
        {
            ptr += sizeof FuncString - 1;

            const char* start = ptr;
            const char* end = start;

            while(*ptr)
            {
                char c = *ptr;

                if(isalnum_(c) || c == ':');
                else if(c == '(')
                {
                    end = ptr;
                    break;
                }
                else break;

                ptr++;
            }

            if(end > start)
            {
                items = items ? realloc(items, (*size + 1) * Size) : malloc(Size);

                items[*size].pos = (s32)(start - code);
                items[*size].size = (s32)(end - start);

                (*size)++;
            }
        }
        else break;
    }

    return items;
}

void evalSquirrel(tic_mem* tic, const char* code) {
    tic_machine* machine = (tic_machine*)tic;
    HSQUIRRELVM vm = machine->squirrel;

    // make sure that the Squirrel interpreter is initialized.
    if (vm == NULL)
    {
        if (!initSquirrel(tic, ""))
            return;
        vm = machine->squirrel;
    }
    
    sq_settop(vm, 0);

    if((SQ_FAILED(sq_compilebuffer(vm, code, strlen(code), "squirrel", SQTrue))) || 
        (sq_pushroottable(vm), false) ||
        (SQ_FAILED(sq_call(vm, 1, SQFalse, SQTrue))))
    {
        sq_getlasterror(vm);
        sq_tostring(vm, -1);
        const SQChar* errorString = "unknown error";
        sq_getstring(vm, -1, &errorString);
        if (machine->data)
            machine->data->error(machine->data->data, errorString);
    }

    sq_settop(vm, 0);
}

static const tic_script_config SquirrelSyntaxConfig = 
{
    .init               = initSquirrel,
    .close              = closeSquirrel,
    .tick               = callSquirrelTick,
    .scanline           = callSquirrelScanline,
    .overline           = callSquirrelOverline,

    .getOutline         = getSquirrelOutline,
    .eval               = evalSquirrel,

    .blockCommentStart  = "/*",
    .blockCommentEnd    = "*/",
    .singleComment      = "//",
    .blockStringStart   = "@\"",
    .blockStringEnd     = "\"",

    .keywords           = SquirrelKeywords,
    .keywordsCount      = COUNT_OF(SquirrelKeywords),
};

const tic_script_config* getSquirrelScriptConfig()
{
    return &SquirrelSyntaxConfig;
}

#endif /* defined(TIC_BUILD_WITH_SQUIRREL) */
