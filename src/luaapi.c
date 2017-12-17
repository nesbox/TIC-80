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

#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "machine.h"
#include "ext/moonscript.h"

#define LUA_LOC_STACK 1E8 // 100.000.000

static const char TicMachine[] = "_TIC80";

s32 luaopen_lpeg(lua_State *L);

// !TODO: get rid of this wrap
static s32 getLuaNumber(lua_State* lua, s32 index)
{
	return (s32)lua_tonumber(lua, index);
}

static void registerLuaFunction(tic_machine* machine, lua_CFunction func, const char *name)
{
	lua_pushcfunction(machine->lua, func);
	lua_setglobal(machine->lua, name);
}

static tic_machine* getLuaMachine(lua_State* lua)
{
	lua_getglobal(lua, TicMachine);
	tic_machine* machine = lua_touserdata(lua, -1);
	lua_pop(lua, 1);
	return machine;
}

static s32 lua_peek(lua_State* lua)
{
	tic_machine* machine = getLuaMachine(lua);

	// check number of args
	s32 address = getLuaNumber(lua, 1);

	if(address >=0 && address < sizeof(tic_ram))
	{
		lua_pushinteger(lua, *((u8*)&machine->memory.ram + address));
		return 1;
	}

	return 0;
}

static s32 lua_poke(lua_State* lua)
{
	tic_machine* machine = getLuaMachine(lua);

	s32 address = getLuaNumber(lua, 1);
	u8 value = getLuaNumber(lua, 2) & 0xff;

	if(address >=0 && address < sizeof(tic_ram))
	{
		*((u8*)&machine->memory.ram + address) = value;
	}

	return 0;
}

static s32 lua_peek4(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top == 1)
	{
		s32 address = getLuaNumber(lua, 1);

		if(address >= 0 && address < sizeof(tic_ram)*2)
		{
			lua_pushinteger(lua, tic_tool_peek4((u8*)&getLuaMachine(lua)->memory.ram, address));
			return 1;
		}		
	}
	else luaL_error(lua, "invalid parameters, peek4(addr)\n");

	return 0;
}

static s32 lua_poke4(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top == 2)
	{
		s32 address = getLuaNumber(lua, 1);
		u8 value = getLuaNumber(lua, 2);

		if(address >= 0 && address < sizeof(tic_ram)*2)
		{
			tic_tool_poke4((u8*)&getLuaMachine(lua)->memory.ram, address, value);
		}
	}
	else luaL_error(lua, "invalid parameters, poke4(addr,value)\n");

	return 0;
}

static s32 lua_cls(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	tic_mem* memory = (tic_mem*)getLuaMachine(lua);

	memory->api.clear(memory, top == 1 ? getLuaNumber(lua, 1) : 0);

	return 0;
}

static s32 lua_pix(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top >= 2)
	{
		s32 x = getLuaNumber(lua, 1);
		s32 y = getLuaNumber(lua, 2);
		
		tic_mem* memory = (tic_mem*)getLuaMachine(lua);

		if(top >= 3)
		{
			s32 color = getLuaNumber(lua, 3);
			memory->api.pixel(memory, x, y, color);
		}
		else
		{
			lua_pushinteger(lua, memory->api.get_pixel(memory, x, y));
			return 1;
		}

	}
	else luaL_error(lua, "invalid parameters, pix(x y [color])\n");

	return 0;
}

static s32 lua_line(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top == 5)
	{
		s32 x0 = getLuaNumber(lua, 1);
		s32 y0 = getLuaNumber(lua, 2);
		s32 x1 = getLuaNumber(lua, 3);
		s32 y1 = getLuaNumber(lua, 4);
		s32 color = getLuaNumber(lua, 5);

		tic_mem* memory = (tic_mem*)getLuaMachine(lua);

		memory->api.line(memory, x0, y0, x1, y1, color);
	}
	else luaL_error(lua, "invalid parameters, line(x0,y0,x1,y1,color)\n");

	return 0;
}

static s32 lua_rect(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top == 5)
	{
		s32 x = getLuaNumber(lua, 1);
		s32 y = getLuaNumber(lua, 2);
		s32 w = getLuaNumber(lua, 3);
		s32 h = getLuaNumber(lua, 4);
		s32 color = getLuaNumber(lua, 5);

		tic_mem* memory = (tic_mem*)getLuaMachine(lua);

		memory->api.rect(memory, x, y, w, h, color);
	}
	else luaL_error(lua, "invalid parameters, rect(x,y,w,h,color)\n");

	return 0;
}

static s32 lua_rectb(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top == 5)
	{
		s32 x = getLuaNumber(lua, 1);
		s32 y = getLuaNumber(lua, 2);
		s32 w = getLuaNumber(lua, 3);
		s32 h = getLuaNumber(lua, 4);
		s32 color = getLuaNumber(lua, 5);

		tic_mem* memory = (tic_mem*)getLuaMachine(lua);

		memory->api.rect_border(memory, x, y, w, h, color);
	}
	else luaL_error(lua, "invalid parameters, rectb(x,y,w,h,color)\n");

	return 0;
}

static s32 lua_circ(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top == 4)
	{
		s32 radius = getLuaNumber(lua, 3);
		if(radius < 0) return 0;
		
		s32 x = getLuaNumber(lua, 1);
		s32 y = getLuaNumber(lua, 2);
		s32 color = getLuaNumber(lua, 4);

		tic_mem* memory = (tic_mem*)getLuaMachine(lua);

		memory->api.circle(memory, x, y, radius, color);
	}
	else luaL_error(lua, "invalid parameters, circ(x,y,radius,color)\n");

	return 0;
}

static s32 lua_circb(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top == 4)
	{
		s32 radius = getLuaNumber(lua, 3);
		if(radius < 0) return 0;

		s32 x = getLuaNumber(lua, 1);
		s32 y = getLuaNumber(lua, 2);
		s32 color = getLuaNumber(lua, 4);

		tic_mem* memory = (tic_mem*)getLuaMachine(lua);

		memory->api.circle_border(memory, x, y, radius, color);
	}
	else luaL_error(lua, "invalid parameters, circb(x,y,radius,color)\n");

	return 0;
}

static s32 lua_tri(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top == 7)
	{
		s32 pt[6];

		for(s32 i = 0; i < COUNT_OF(pt); i++)
			pt[i] = getLuaNumber(lua, i+1);
		
		s32 color = getLuaNumber(lua, 7);

		tic_mem* memory = (tic_mem*)getLuaMachine(lua);

		memory->api.tri(memory, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);
	}
	else luaL_error(lua, "invalid parameters, tri(x1,y1,x2,y2,x3,y3,color)\n");

	return 0;
}

static s32 lua_textri(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if (top >= 12)
	{
		float pt[12];

		for (s32 i = 0; i < COUNT_OF(pt); i++)
			pt[i] = (float)lua_tonumber(lua, i + 1);

		tic_mem* memory = (tic_mem*)getLuaMachine(lua);
		u8 chroma = 0xff;
		bool use_map = false;

		//	check for use map 
		if (top >= 13)
			use_map = lua_toboolean(lua, 13);
		//	check for chroma 
		if (top >= 14)
			chroma = (u8)getLuaNumber(lua, 14);

		memory->api.textri(memory, pt[0], pt[1],	//	xy 1
									pt[2], pt[3],	//	xy 2
									pt[4], pt[5],	//  xy 3
									pt[6], pt[7],	//	uv 1
									pt[8], pt[9],	//	uv 2
									pt[10], pt[11], //  uv 3
									use_map,		// use map
									chroma);		// chroma
	}
	else luaL_error(lua, "invalid parameters, textri(x1,y1,x2,y2,x3,y3,u1,v1,u2,v2,u3,v3,[use_map=false],[chroma=off])\n");
	return 0;
}


static s32 lua_clip(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top == 0)
	{
		tic_mem* memory = (tic_mem*)getLuaMachine(lua);

		memory->api.clip(memory, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
	}
	else if(top == 4)
	{
		s32 x = getLuaNumber(lua, 1);
		s32 y = getLuaNumber(lua, 2);
		s32 w = getLuaNumber(lua, 3);
		s32 h = getLuaNumber(lua, 4);

		tic_mem* memory = (tic_mem*)getLuaMachine(lua);

		memory->api.clip((tic_mem*)getLuaMachine(lua), x, y, w, h);
	}
	else luaL_error(lua, "invalid parameters, use clip(x,y,w,h) or clip()\n");

	return 0;
}

static s32 lua_btnp(lua_State* lua)
{
	tic_machine* machine = getLuaMachine(lua);
	tic_mem* memory = (tic_mem*)machine;

	if(machine->memory.input == tic_gamepad_input)
	{
		s32 top = lua_gettop(lua);

		if (top == 0)
		{
			lua_pushinteger(lua, memory->api.btnp(memory, -1, -1, -1));
		}
		else if(top == 1)
		{
			s32 index = getLuaNumber(lua, 1) & 0xf;

			lua_pushboolean(lua, memory->api.btnp(memory, index, -1, -1));
		}
		else if (top == 3)
		{
			s32 index = getLuaNumber(lua, 1) & 0xf;
			u32 hold = getLuaNumber(lua, 2);
			u32 period = getLuaNumber(lua, 3);

			lua_pushboolean(lua, memory->api.btnp(memory, index, hold, period));
		}
		else luaL_error(lua, "invalid params, btnp [ id [ hold period ] ]\n");

		return 1;
	}
	else luaL_error(lua, "gamepad input not declared in metadata\n");

	return 0;
}

static s32 lua_btn(lua_State* lua)
{
	tic_machine* machine = getLuaMachine(lua);

	if(machine->memory.input == tic_gamepad_input)
	{
		s32 top = lua_gettop(lua);

		if (top == 0)
		{
			lua_pushinteger(lua, machine->memory.ram.vram.input.gamepad.data);
		}
		else if (top == 1)
		{
			s32 index = getLuaNumber(lua, 1) & 0xf;
			lua_pushboolean(lua, machine->memory.ram.vram.input.gamepad.data & (1 << index));
		}
		else luaL_error(lua, "invalid params, btn [ id ]\n");

		return 1;		
	}
	else luaL_error(lua, "gamepad input not declared in metadata\n");

	return 0;
}

static s32 lua_spr(lua_State* lua)
{
	s32 top = lua_gettop(lua);

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

	if(top >= 1) 
	{
		index = getLuaNumber(lua, 1);

		if(top >= 3)
		{
			x = getLuaNumber(lua, 2);
			y = getLuaNumber(lua, 3);

			if(top >= 4)
			{
				if(lua_istable(lua, 4))
				{
					for(s32 i = 1; i <= TIC_PALETTE_SIZE; i++)
					{
						lua_rawgeti(lua, 4, i);
						if(lua_isnumber(lua, -1))
						{
							colors[i-1] = getLuaNumber(lua, -1);
							count++;
							lua_pop(lua, 1);
						}
						else
						{
							lua_pop(lua, 1);
							break;
						}
					}
				}
				else 
				{
					colors[0] = getLuaNumber(lua, 4);
					count = 1;
				}

				if(top >= 5)
				{
					scale = getLuaNumber(lua, 5);

					if(top >= 6)
					{
						flip = getLuaNumber(lua, 6);

						if(top >= 7)
						{
							rotate = getLuaNumber(lua, 7);

							if(top >= 9)
							{
								w = getLuaNumber(lua, 8);
								h = getLuaNumber(lua, 9);
							}
						}
					}
				}
			}
		}
	}

	tic_mem* memory = (tic_mem*)getLuaMachine(lua);

	memory->api.sprite_ex(memory, &memory->ram.tiles, index, x, y, w, h, colors, count, scale, flip, rotate);

	return 0;
}

static s32 lua_mget(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top == 2)
	{
		s32 x = getLuaNumber(lua, 1);
		s32 y = getLuaNumber(lua, 2);

		tic_mem* memory = (tic_mem*)getLuaMachine(lua);

		u8 value = memory->api.map_get(memory, &memory->ram.map, x, y);
		lua_pushinteger(lua, value);
		return 1;
	}
	else luaL_error(lua, "invalid params, mget(x,y)\n");

	return 0;
}

static s32 lua_mset(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top == 3)
	{
		s32 x = getLuaNumber(lua, 1);
		s32 y = getLuaNumber(lua, 2);
		u8 val = getLuaNumber(lua, 3);

		tic_mem* memory = (tic_mem*)getLuaMachine(lua);

		memory->api.map_set(memory, &memory->ram.map, x, y, val);
	}
	else luaL_error(lua, "invalid params, mget(x,y)\n");

	return 0;
}

typedef struct
{
	lua_State* lua;
	s32 reg;
} RemapData;

static void remapCallback(void* data, s32 x, s32 y, RemapResult* result)
{
	RemapData* remap = (RemapData*)data;
	lua_State* lua = remap->lua;

	lua_rawgeti(lua, LUA_REGISTRYINDEX, remap->reg);
	lua_pushinteger(lua, result->index);
	lua_pushinteger(lua, x);
	lua_pushinteger(lua, y);
	lua_pcall(lua, 3, 3, 0);

	result->index = getLuaNumber(lua, -3);
	result->flip = getLuaNumber(lua, -2);
	result->rotate = getLuaNumber(lua, -1);
}

static s32 lua_map(lua_State* lua)
{
	s32 x = 0;
	s32 y = 0;
	s32 w = TIC_MAP_SCREEN_WIDTH;
	s32 h = TIC_MAP_SCREEN_HEIGHT;
	s32 sx = 0;
	s32 sy = 0;
	u8 chromakey = -1;
	s32 scale = 1;

	s32 top = lua_gettop(lua);

	if(top >= 2) 
	{
		x = getLuaNumber(lua, 1);
		y = getLuaNumber(lua, 2);

		if(top >= 4)
		{
			w = getLuaNumber(lua, 3);
			h = getLuaNumber(lua, 4);

			if(top >= 6)
			{
				sx = getLuaNumber(lua, 5);
				sy = getLuaNumber(lua, 6);

				if(top >= 7)
				{
					chromakey = getLuaNumber(lua, 7);

					if(top >= 8)
					{
						scale = getLuaNumber(lua, 8);

						if(top >= 9)
						{
							if (lua_isfunction(lua, 9))
							{
								s32 remap = luaL_ref(lua, LUA_REGISTRYINDEX);

								RemapData data = {lua, remap};

								tic_mem* memory = (tic_mem*)getLuaMachine(lua);

								memory->api.remap(memory, &memory->ram.map, &memory->ram.tiles, x, y, w, h, sx, sy, chromakey, scale, remapCallback, &data);

								luaL_unref(lua, LUA_REGISTRYINDEX, data.reg);

								return 0;
							}							
						}
					}
				}
			}
		}
	}

	tic_mem* memory = (tic_mem*)getLuaMachine(lua);

	memory->api.map((tic_mem*)getLuaMachine(lua), &memory->ram.map, &memory->ram.tiles, x, y, w, h, sx, sy, chromakey, scale);

	return 0;
}

static s32 lua_music(lua_State* lua)
{
	s32 top = lua_gettop(lua);
	tic_mem* memory = (tic_mem*)getLuaMachine(lua);

	if(top == 0) memory->api.music(memory, -1, 0, 0, false);
	else if(top >= 1)
	{
		memory->api.music(memory, -1, 0, 0, false);

		s32 track = getLuaNumber(lua, 1);
		s32 frame = -1;
		s32 row = -1;
		bool loop = true;

		if(top >= 2)
		{
			frame = getLuaNumber(lua, 2);

			if(top >= 3)
			{
				row = getLuaNumber(lua, 3);

				if(top >= 4)
				{
					loop = lua_toboolean(lua, 4);
				}
			}
		}

		memory->api.music(memory, track, frame, row, loop);
	}
	else luaL_error(lua, "invalid params, use music(track)\n");

	return 0;
}

static s32 lua_sfx(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top >= 1)
	{
		tic_mem* memory = (tic_mem*)getLuaMachine(lua);

		s32 note = -1;
		s32 octave = -1;
		s32 duration = -1;
		s32 channel = 0;
		s32 volume = MAX_VOLUME;
		s32 speed = SFX_DEF_SPEED;

		s32 index = getLuaNumber(lua, 1);

		if(index < SFX_COUNT)
		{
			if (index >= 0)
			{
				tic_sample* effect = memory->ram.sfx.samples.data + index;

				note = effect->note;
				octave = effect->octave;
				speed = effect->speed;
			}

			if(top >= 2)
			{
				if(lua_isinteger(lua, 2))
				{
					s32 id = getLuaNumber(lua, 2);
					note = id % NOTES;
					octave = id / NOTES;
				}
				else if(lua_isstring(lua, 2))
				{
					const char* noteStr = lua_tostring(lua, 2);

					if(!tic_tool_parse_note(noteStr, &note, &octave))
					{
						luaL_error(lua, "invalid note, should be like C#4\n");
						return 0;
					}
				}

				if(top >= 3)
				{
					duration = getLuaNumber(lua, 3);

					if(top >= 4)
					{
						channel = getLuaNumber(lua, 4);

						if(top >= 5)
						{
							volume = getLuaNumber(lua, 5);

							if(top >= 6)
							{
								speed = getLuaNumber(lua, 6);
							}
						}
					}					
				}
			}

			if (channel >= 0 && channel < TIC_SOUND_CHANNELS)
			{
				memory->api.sfx_stop(memory, channel);
				memory->api.sfx_ex(memory, index, note, octave, duration, channel, volume & 0xf, speed);
			}
			else luaL_error(lua, "unknown channel\n");
		}
		else luaL_error(lua, "unknown sfx index\n");
	}
	else luaL_error(lua, "invalid sfx params\n");

	return 0;
}

static s32 lua_sync(lua_State* lua)
{
	tic_mem* memory = (tic_mem*)getLuaMachine(lua);

	bool toCart = false;
	u32 mask = 0;
	s32 bank = 0;

	if(lua_gettop(lua) >= 1)
	{
		mask = getLuaNumber(lua, 1);

		if(lua_gettop(lua) >= 2)
		{
			bank = getLuaNumber(lua, 2);

			if(lua_gettop(lua) >= 3)
			{
				toCart = lua_toboolean(lua, 3);
			}
		}
	}

	if(bank >= 0 && bank < TIC_BANKS)
		memory->api.sync(memory, mask, bank, toCart);
	else
		luaL_error(lua, "sync() error, invalid bank");

	return 0;
}

static s32 lua_memcpy(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top == 3)
	{
		s32 dest = getLuaNumber(lua, 1);
		s32 src = getLuaNumber(lua, 2);
		s32 size = getLuaNumber(lua, 3);
                s32 bound = sizeof(tic_ram) - size;

		if(size >= 0 && size <= sizeof(tic_ram) && dest >= 0 && src >= 0 && dest <= bound && src <= bound)
		{
			u8* base = (u8*)&getLuaMachine(lua)->memory;
			memcpy(base + dest, base + src, size);
			return 0;
		}
	}

	luaL_error(lua, "invalid params, memcpy(dest,src,size)\n");

	return 0;
}

static s32 lua_memset(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top == 3)
	{
		s32 dest = getLuaNumber(lua, 1);
		u8 value = getLuaNumber(lua, 2);
		s32 size = getLuaNumber(lua, 3);
		s32 bound = sizeof(tic_ram) - size;

		if(size >= 0 && size <= sizeof(tic_ram) && dest >= 0 && dest <= bound)
		{
			u8* base = (u8*)&getLuaMachine(lua)->memory;
			memset(base + dest, value, size);
			return 0;
		}
	}

	luaL_error(lua, "invalid params, memset(dest,val,size)\n");

	return 0;
}

static const char* printString(lua_State* lua, s32 index)
{
	lua_getglobal(lua, "tostring");
	lua_pushvalue(lua, -1);
	lua_pushvalue(lua, index);
	lua_call(lua, 1, 1);

	const char* text = lua_tostring(lua, -1);

	lua_pop(lua, 2);

	return text;
}

static s32 lua_font(lua_State* lua)
{
	tic_mem* memory = (tic_mem*)getLuaMachine(lua);
	s32 top = lua_gettop(lua);

	if(top >= 1)
	{
		const char* text = printString(lua, 1);
		s32 x = 0;
		s32 y = 0;
		s32 width = TIC_SPRITESIZE;
		s32 height = TIC_SPRITESIZE;
		u8 chromakey = 0;
		bool fixed = false;
		s32 scale = 1;

		if(top >= 3)
		{
			x = getLuaNumber(lua, 2);
			y = getLuaNumber(lua, 3);

			if(top >= 4)
			{
				chromakey = getLuaNumber(lua, 4);

				if(top >= 6)
				{
					width = getLuaNumber(lua, 5);
					height = getLuaNumber(lua, 6);

					if(top >= 7)
					{
						fixed = lua_toboolean(lua, 7);

						if(top >= 8)
						{
							scale = getLuaNumber(lua, 8);
						}
					}
				}
			}
		}

		if(scale == 0)
		{
			lua_pushinteger(lua, 0);
			return 1;
		}

		s32 size = drawText(memory, text, x, y, width, height, chromakey, scale, fixed ? drawSpriteFont : drawFixedSpriteFont);

		lua_pushinteger(lua, size);


		return 1;
	}

	return 0;
}

static s32 lua_print(lua_State* lua)
{
	s32 top = lua_gettop(lua);

	if(top >= 1) 
	{
		tic_mem* memory = (tic_mem*)getLuaMachine(lua);

		s32 x = 0;
		s32 y = 0;
		s32 color = TIC_PALETTE_SIZE-1;
		bool fixed = false;
		s32 scale = 1;

		const char* text = printString(lua, 1);

		if(top >= 3)
		{
			x = getLuaNumber(lua, 2);
			y = getLuaNumber(lua, 3);

			if(top >= 4)
			{
				color = getLuaNumber(lua, 4) % TIC_PALETTE_SIZE;

				if(top >= 5)
				{
					fixed = lua_toboolean(lua, 5);

					if(top >= 6)
					{
						scale = getLuaNumber(lua, 6);
					}
				}
			}
		}

		if(scale == 0)
		{
			lua_pushinteger(lua, 0);
			return 1;
		}

		s32 size = memory->api.text_ex(memory, text ? text : "nil", x, y, color, fixed, scale);

		lua_pushinteger(lua, size);

		return 1;
	}

	return 0;
}

static s32 lua_trace(lua_State *lua)
{
	s32 top = lua_gettop(lua);
	tic_machine* machine = getLuaMachine(lua);


	if(top >= 1)
	{
		const char* text = printString(lua, 1);
		u8 color = tic_color_white;

		if(top >= 2)
		{
			color = getLuaNumber(lua, 2);
		}

		machine->data->trace(machine->data->data, text ? text : "nil", color);
	}

	return 0;
}

static s32 lua_pmem(lua_State *lua)
{
	s32 top = lua_gettop(lua);
	tic_machine* machine = getLuaMachine(lua);
	tic_mem* memory = &machine->memory;

	if(top >= 1)
	{
		u32 index = getLuaNumber(lua, 1);

		if(index < TIC_PERSISTENT_SIZE)
		{
			s32 val = memory->ram.persistent.data[index];

			if(top >= 2)
			{
				memory->ram.persistent.data[index] = getLuaNumber(lua, 2);
			}

			lua_pushinteger(lua, val);

			return 1;			
		}
		luaL_error(lua, "invalid persistent memory index\n");
	}
	else luaL_error(lua, "invalid params, pmem(index [val]) -> val\n");

	return 0;
}

static s32 lua_time(lua_State *lua)
{
	tic_mem* memory = (tic_mem*)getLuaMachine(lua);
	
	lua_pushnumber(lua, memory->api.time(memory));

	return 1;
}

static s32 lua_exit(lua_State *lua)
{
	tic_machine* machine = getLuaMachine(lua);

	machine->data->exit(machine->data->data);
	
	return 0;
}

static s32 lua_mouse(lua_State *lua)
{
	tic_machine* machine = getLuaMachine(lua);

	if(machine->memory.input == tic_mouse_input)
	{
		u16 data = machine->memory.ram.vram.input.gamepad.data;

		lua_pushinteger(lua, (data & 0x7fff) % TIC80_WIDTH);
		lua_pushinteger(lua, (data & 0x7fff) / TIC80_WIDTH);
		lua_pushboolean(lua, data >> 15);

		return 3;		
	}
	else luaL_error(lua, "mouse input not declared in metadata\n");

	return 0;
}

static s32 lua_dofile(lua_State *lua)
{
	luaL_error(lua, "unknown method: \"dofile\"\n");

	return 0;
}

static s32 lua_loadfile(lua_State *lua)
{
	luaL_error(lua, "unknown method: \"loadfile\"\n");

	return 0;
}


static void setloaded(lua_State* l, char* name)
{
	s32 top = lua_gettop(l);
	lua_getglobal(l, "package");
	lua_getfield(l, -1, "loaded");
	lua_getfield(l, -1, name);
	if (lua_isnil(l, -1)) {
		lua_pop(l, 1);
		lua_pushvalue(l, top);
		lua_setfield(l, -2, name);
	}

	lua_settop(l, top);
}

static const char* const ApiKeywords[] = API_KEYWORDS;
static const lua_CFunction ApiFunc[] = 
{
	NULL, NULL, NULL, lua_print, lua_cls, lua_pix, lua_line, lua_rect, 
	lua_rectb, lua_spr, lua_btn, lua_btnp, lua_sfx, lua_map, lua_mget, 
	lua_mset, lua_peek, lua_poke, lua_peek4, lua_poke4, lua_memcpy, 
	lua_memset, lua_trace, lua_pmem, lua_time, lua_exit, lua_font, lua_mouse, 
	lua_circ, lua_circb, lua_tri, lua_textri, lua_clip, lua_music, lua_sync
};

STATIC_ASSERT(api_func, COUNT_OF(ApiKeywords) == COUNT_OF(ApiFunc));

static void checkForceExit(lua_State *lua, lua_Debug *luadebug)
{
	tic_machine* machine = getLuaMachine(lua);

	tic_tick_data* tick = machine->data;

	if(tick->forceExit && tick->forceExit(tick->data))
		luaL_error(lua, "script execution was interrupted");
}

static void initAPI(tic_machine* machine)
{
	lua_pushlightuserdata(machine->lua, machine);
	lua_setglobal(machine->lua, TicMachine);

	for (s32 i = 0; i < COUNT_OF(ApiFunc); i++)
		if (ApiFunc[i])
			registerLuaFunction(machine, ApiFunc[i], ApiKeywords[i]);

	registerLuaFunction(machine, lua_dofile, "dofile");
	registerLuaFunction(machine, lua_loadfile, "loadfile");

	lua_sethook(machine->lua, &checkForceExit, LUA_MASKCOUNT, LUA_LOC_STACK);
}

void closeLua(tic_machine* machine)
{
	if(machine->lua)
	{
		lua_close(machine->lua);
		machine->lua = NULL;
	}
}

bool initLua(tic_machine* machine, const char* code)
{
	closeLua(machine);

	lua_State* lua = machine->lua = luaL_newstate();

	static const luaL_Reg loadedlibs[] =
	{
		{ "_G", luaopen_base },
		//{ LUA_LOADLIBNAME, luaopen_package },
		{ LUA_COLIBNAME, luaopen_coroutine },
		{ LUA_TABLIBNAME, luaopen_table },
		// {LUA_IOLIBNAME, luaopen_io},
		// {LUA_OSLIBNAME, luaopen_os},
		{ LUA_STRLIBNAME, luaopen_string },
		{ LUA_MATHLIBNAME, luaopen_math },
		// {LUA_UTF8LIBNAME, luaopen_utf8},
		//{ LUA_DBLIBNAME, luaopen_debug },
		// #if defined(LUA_COMPAT_BITLIB)
		// {LUA_BITLIBNAME, luaopen_bit32},
		// #endif
		{ NULL, NULL }
	};

	for (const luaL_Reg *lib = loadedlibs; lib->func; lib++)
	{
		luaL_requiref(lua, lib->name, lib->func, 1);
		lua_pop(lua, 1);
	}

	initAPI(machine);

	{
		lua_State* lua = machine->lua;

		lua_settop(lua, 0);

		if(luaL_loadstring(lua, code) != LUA_OK || lua_pcall(lua, 0, LUA_MULTRET, 0) != LUA_OK)
		{
			machine->data->error(machine->data->data, lua_tostring(lua, -1));
			return false;
		}
	}

	return true;
}

#define MOON_CODE(...) #__VA_ARGS__

static const char* execute_moonscript_src = MOON_CODE(
	local fn, err = require('moonscript.base').loadstring(...)

	if not fn then
		return err
	end
	return fn()
);

bool initMoonscript(tic_machine* machine, const char* code)
{
	closeLua(machine);

	lua_State* lua = machine->lua = luaL_newstate();

	static const luaL_Reg loadedlibs[] =
	{
		{ "_G", luaopen_base },
		{ LUA_LOADLIBNAME, luaopen_package },
		{ LUA_COLIBNAME, luaopen_coroutine },
		{ LUA_TABLIBNAME, luaopen_table },
		{ LUA_STRLIBNAME, luaopen_string },
		{ LUA_MATHLIBNAME, luaopen_math },
		{ LUA_DBLIBNAME, luaopen_debug },
		{ NULL, NULL }
	};

	for (const luaL_Reg *lib = loadedlibs; lib->func; lib++)
	{
		luaL_requiref(lua, lib->name, lib->func, 1);
		lua_pop(lua, 1);
	}

	luaopen_lpeg(lua);
	setloaded(lua, "lpeg");

	initAPI(machine);

	{
		lua_State* moon = machine->lua;

		lua_settop(moon, 0);

		if (luaL_loadbuffer(moon, (const char *)moonscript_lua, moonscript_lua_len, "moonscript.lua") != LUA_OK)
		{
			machine->data->error(machine->data->data, "failed to load moonscript.lua");
			return false;
		}

		lua_call(moon, 0, 0);

		if (luaL_loadbuffer(moon, execute_moonscript_src, strlen(execute_moonscript_src), "execute_moonscript") != LUA_OK)
		{
			machine->data->error(machine->data->data, "failed to load moonscript compiler");
			return false;
		}

		lua_pushstring(moon, code);
		if (lua_pcall(moon, 1, 1, 0) != LUA_OK)
		{
			const char* msg = lua_tostring(moon, -1);

			if (msg)
			{
				machine->data->error(machine->data->data, msg);
				return false;
			}
		}
	}

	return true;
}

void callLuaTick(tic_machine* machine)
{
 	const char* TicFunc = ApiKeywords[0];

 	lua_State* lua = machine->lua;

 	if(lua)
 	{
		lua_getglobal(lua, TicFunc);
		if(lua_isfunction(lua, -1)) 
		{
			if(lua_pcall(lua, 0, 0, 0) != LUA_OK)	
				machine->data->error(machine->data->data, lua_tostring(lua, -1));
		}
		else 
		{		
			lua_pop(lua, 1);
			machine->data->error(machine->data->data, "'function TIC()...' isn't found :(");
		} 		
 	}
}

void callLuaScanline(tic_mem* memory, s32 row, void* data)
{
	tic_machine* machine = (tic_machine*)memory;
	lua_State* lua = machine->lua;

	if (lua)
	{
		const char* ScanlineFunc = ApiKeywords[1];

		lua_getglobal(lua, ScanlineFunc);
		if(lua_isfunction(lua, -1)) 
		{
			lua_pushinteger(lua, row);
			if(lua_pcall(lua, 1, 0, 0) != LUA_OK)
				machine->data->error(machine->data->data, lua_tostring(lua, -1));
		}
		else lua_pop(lua, 1);
	}
}

void callLuaOverlap(tic_mem* memory, void* data)
{
	tic_machine* machine = (tic_machine*)memory;
	lua_State* lua = machine->lua;

	if (lua)
	{
		const char* OvrFunc = ApiKeywords[2];

		lua_getglobal(lua, OvrFunc);
		if(lua_isfunction(lua, -1)) 
		{
			if(lua_pcall(lua, 0, 0, 0) != LUA_OK)
				machine->data->error(machine->data->data, lua_tostring(lua, -1));
		}
		else lua_pop(lua, 1);
	}

}
