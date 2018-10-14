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

#if defined(TIC_BUILD_WITH_LUA)

#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <ctype.h>

#define LUA_LOC_STACK 1E8 // 100.000.000

static const char TicMachine[] = "_TIC80";

s32 luaopen_lpeg(lua_State *lua);

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

	s32 top = lua_gettop(lua);

	if (top == 0)
	{
		lua_pushinteger(lua, memory->api.btnp(memory, -1, -1, -1));
	}
	else if(top == 1)
	{
		s32 index = getLuaNumber(lua, 1) & 0x1f;

		lua_pushboolean(lua, memory->api.btnp(memory, index, -1, -1));
	}
	else if (top == 3)
	{
		s32 index = getLuaNumber(lua, 1) & 0x1f;
		u32 hold = getLuaNumber(lua, 2);
		u32 period = getLuaNumber(lua, 3);

		lua_pushboolean(lua, memory->api.btnp(memory, index, hold, period));
	}
	else
	{
		luaL_error(lua, "invalid params, btnp [ id [ hold period ] ]\n");
		return 0;
	}

	return 1;
}

static s32 lua_btn(lua_State* lua)
{
	tic_machine* machine = getLuaMachine(lua);

	s32 top = lua_gettop(lua);

	if (top == 0)
	{
		lua_pushinteger(lua, machine->memory.ram.input.gamepads.data);
	}
	else if (top == 1)
	{
		u32 index = getLuaNumber(lua, 1) & 0x1f;
		lua_pushboolean(lua, machine->memory.ram.input.gamepads.data & (1 << index));
	}
	else
	{
		luaL_error(lua, "invalid params, btn [ id ]\n");
		return 0;
	} 

	return 1;
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

static s32 lua_reset(lua_State* lua)
{
	tic_machine* machine = getLuaMachine(lua);

	machine->state.initialized = false;

	return 0;
}

static s32 lua_key(lua_State* lua)
{
	tic_machine* machine = getLuaMachine(lua);
	tic_mem* tic = &machine->memory;

	s32 top = lua_gettop(lua);

	if (top == 0)
	{
		lua_pushboolean(lua, tic->api.key(tic, tic_key_unknown));
	}
	else if (top == 1)
	{
		tic_key key = getLuaNumber(lua, 1);

		if(key < tic_key_escape)
			lua_pushboolean(lua, tic->api.key(tic, key));
		else
		{
			luaL_error(lua, "unknown keyboard code\n");
			return 0;
		}
	}
	else
	{
		luaL_error(lua, "invalid params, key [code]\n");
		return 0;
	} 

	return 1;
}

static s32 lua_keyp(lua_State* lua)
{
	tic_machine* machine = getLuaMachine(lua);
	tic_mem* tic = &machine->memory;

	s32 top = lua_gettop(lua);

	if (top == 0)
	{
		lua_pushboolean(lua, tic->api.keyp(tic, tic_key_unknown, -1, -1));
	}
	else
	{
		tic_key key = getLuaNumber(lua, 1);

		if(key >= tic_key_escape)
		{
			luaL_error(lua, "unknown keyboard code\n");
		}
		else
		{
			if(top == 1)
			{
				lua_pushboolean(lua, tic->api.keyp(tic, key, -1, -1));
			}
			else if(top == 3)
			{
				u32 hold = getLuaNumber(lua, 2);
				u32 period = getLuaNumber(lua, 3);

				lua_pushboolean(lua, tic->api.keyp(tic, key, hold, period));
			}
			else
			{
				luaL_error(lua, "invalid params, keyp [ code [ hold period ] ]\n");
				return 0;
			}
		}
	}

	return 1;
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
		bool alt = false;

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

							if(top >= 9)
							{
								alt = lua_toboolean(lua, 9);
							}
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

		s32 size = drawText(memory, text, x, y, width, height, chromakey, scale, fixed ? drawSpriteFont : drawFixedSpriteFont, alt);

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
		bool alt = false;

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

						if(top >= 7)
						{
							alt = lua_toboolean(lua, 7);
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

		s32 size = memory->api.text_ex(memory, text ? text : "nil", x, y, color, fixed, scale, alt);

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
			u32 val = memory->persistent.data[index];

			if(top >= 2)
			{
				memory->persistent.data[index] = lua_tointeger(lua, 2);
				machine->data->syncPMEM = true;
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

	const tic80_mouse* mouse = &machine->memory.ram.input.mouse;

	lua_pushinteger(lua, mouse->x);
	lua_pushinteger(lua, mouse->y);
	lua_pushboolean(lua, mouse->left);
	lua_pushboolean(lua, mouse->middle);
	lua_pushboolean(lua, mouse->right);

	return 5;
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

static void lua_open_builtins(lua_State *lua)
{
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
}

static const char* const ApiKeywords[] = API_KEYWORDS;
static const lua_CFunction ApiFunc[] = 
{
	NULL, NULL, NULL, lua_print, lua_cls, lua_pix, lua_line, lua_rect, 
	lua_rectb, lua_spr, lua_btn, lua_btnp, lua_sfx, lua_map, lua_mget, 
	lua_mset, lua_peek, lua_poke, lua_peek4, lua_poke4, lua_memcpy, 
	lua_memset, lua_trace, lua_pmem, lua_time, lua_exit, lua_font, lua_mouse, 
	lua_circ, lua_circb, lua_tri, lua_textri, lua_clip, lua_music, lua_sync, lua_reset,
	lua_key, lua_keyp
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

static void closeLua(tic_mem* tic)
{
	tic_machine* machine = (tic_machine*)tic;

	if(machine->lua)
	{
		lua_close(machine->lua);
		machine->lua = NULL;
	}
}

static bool initLua(tic_mem* tic, const char* code)
{
	tic_machine* machine = (tic_machine*)tic;

	closeLua(tic);

	lua_State* lua = machine->lua = luaL_newstate();
	lua_open_builtins(lua);

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

/*
** Message handler which appends stract trace to exceptions.
** This function was extractred from lua.c.
*/
static s32 msghandler (lua_State *lua) 
{
	const char *msg = lua_tostring(lua, 1);
	if (msg == NULL) /* is error object not a string? */
	{
		if (luaL_callmeta(lua, 1, "__tostring") &&  /* does it have a metamethod */
			lua_type(lua, -1) == LUA_TSTRING)  /* that produces a string? */
			return 1;  /* that is the message */
		else
			msg = lua_pushfstring(lua, "(error object is a %s value)", luaL_typename(lua, 1));
	}
	luaL_traceback(lua, lua, msg, 1);  /* append a standard traceback */
	return 1;  /* return the traceback */
}

/*
** Interface to 'lua_pcall', which sets appropriate message handler function.
** Please use this function for all top level lua functions.
** This function was extractred from lua.c (and stripped of signal handling)
*/
static s32 docall (lua_State *lua, s32 narg, s32 nres) 
{
	s32 status = 0;
	s32 base = lua_gettop(lua) - narg;  /* function index */
	lua_pushcfunction(lua, msghandler);  /* push message handler */
	lua_insert(lua, base);  /* put it under function and args */
	status = lua_pcall(lua, narg, nres, base);
	lua_remove(lua, base);  /* remove message handler from the stack */
	return status;
}

static void callLuaTick(tic_mem* tic)
{
	tic_machine* machine = (tic_machine*)tic;

	const char* TicFunc = ApiKeywords[0];

	lua_State* lua = machine->lua;

	if(lua)
	{
		lua_getglobal(lua, TicFunc);
		if(lua_isfunction(lua, -1)) 
		{
			if(docall(lua, 0, 0) != LUA_OK)	
				machine->data->error(machine->data->data, lua_tostring(lua, -1));
		}
		else 
		{		
			lua_pop(lua, 1);
			machine->data->error(machine->data->data, "'function TIC()...' isn't found :(");
		}
	}
}

static void callLuaScanlineName(tic_mem* memory, s32 row, void* data, const char* name)
{
	tic_machine* machine = (tic_machine*)memory;
	lua_State* lua = machine->lua;

	if (lua)
	{
		lua_getglobal(lua, name);
		if(lua_isfunction(lua, -1))
		{
			lua_pushinteger(lua, row);
			if(docall(lua, 1, 0) != LUA_OK)
				machine->data->error(machine->data->data, lua_tostring(lua, -1));
		}
		else lua_pop(lua, 1);
	}
}

static void callLuaScanline(tic_mem* memory, s32 row, void* data)
{
	callLuaScanlineName(memory, row, data, ApiKeywords[1]);

	// try to call old scanline
	callLuaScanlineName(memory, row, data, "scanline");
}

static void callLuaOverline(tic_mem* memory, void* data)
{
	tic_machine* machine = (tic_machine*)memory;
	lua_State* lua = machine->lua;

	if (lua)
	{
		const char* OvrFunc = ApiKeywords[2];

		lua_getglobal(lua, OvrFunc);
		if(lua_isfunction(lua, -1)) 
		{
			if(docall(lua, 0, 0) != LUA_OK)
				machine->data->error(machine->data->data, lua_tostring(lua, -1));
		}
		else lua_pop(lua, 1);
	}

}

static const char* const LuaKeywords [] =
{
	"and", "break", "do", "else", "elseif",
	"end", "false", "for", "function", "goto", "if",
	"in", "local", "nil", "not", "or", "repeat",
	"return", "then", "true", "until", "while"
};

static inline bool isalnum_(char c) {return isalnum(c) || c == '_';}

static const tic_outline_item* getLuaOutline(const char* code, s32* size)
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

				items[*size].pos = start - code;
				items[*size].size = end - start;

				(*size)++;
			}
		}
		else break;
	}

	return items;
}

static void evalLua(tic_mem* tic, const char* code) {
	tic_machine* machine = (tic_machine*)tic;
	lua_State* lua = machine->lua;

	if (!lua) return;

	lua_settop(lua, 0);

	if(luaL_loadstring(lua, code) != LUA_OK || lua_pcall(lua, 0, LUA_MULTRET, 0) != LUA_OK)
	{
		machine->data->error(machine->data->data, lua_tostring(lua, -1));
	}
}

static const tic_script_config LuaSyntaxConfig = 
{
	.init 				= initLua,
	.close 				= closeLua,
	.tick 				= callLuaTick,
	.scanline 			= callLuaScanline,
	.overline 			= callLuaOverline,

	.getOutline			= getLuaOutline,
	.parse 				= parseCode,
	.eval				= evalLua,

	.blockCommentStart 	= "--[[",
	.blockCommentEnd 	= "]]",
	.singleComment 		= "--",
	.blockStringStart	= "[[",
	.blockStringEnd		= "]]",

	.keywords 			= LuaKeywords,
	.keywordsCount 		= COUNT_OF(LuaKeywords),
	
	.api 				= ApiKeywords,
	.apiCount 			= COUNT_OF(ApiKeywords),
};

const tic_script_config* getLuaScriptConfig()
{
	return &LuaSyntaxConfig;
}

#if defined(TIC_BUILD_WITH_MOON)

#include "moonscript.h"

#define MOON_CODE(...) #__VA_ARGS__

static const char* execute_moonscript_src = MOON_CODE(
	local fn, err = require('moonscript.base').loadstring(...)

	if not fn then
		return err
	end
	return fn()
);

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

static bool initMoonscript(tic_mem* tic, const char* code)
{
	tic_machine* machine = (tic_machine*)tic;
	closeLua(tic);

	lua_State* lua = machine->lua = luaL_newstate();
	lua_open_builtins(lua);

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

static const char* const MoonKeywords [] =
{
	"false", "true", "nil", "return",
	"break", "continue", "for", "while",
	"if", "else", "elseif", "unless", "switch",
	"when", "and", "or", "in", "do",
	"not", "super", "try", "catch",
	"with", "export", "import", "then",
	"from", "class", "extends", "new"
};

static const tic_outline_item* getMoonOutline(const char* code, s32* size)
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
		static const char FuncString[] = "=->";

		ptr = strstr(ptr, FuncString);

		if(ptr)
		{
			const char* end = ptr;

			ptr += sizeof FuncString - 1;

			while(end >= code && !isalnum_(*end))  end--;

			const char* start = end;

			for (const char* val = start-1; val >= code && (isalnum_(*val)); val--, start--);

			if(end > start)
			{
				items = items ? realloc(items, (*size + 1) * Size) : malloc(Size);

				items[*size].pos = start - code;
				items[*size].size = end - start + 1;

				(*size)++;
			}
		}
		else break;
	}

	return items;
}

static const tic_script_config MoonSyntaxConfig = 
{
	.init 				= initMoonscript,
	.close 				= closeLua,
	.tick 				= callLuaTick,
	.scanline 			= callLuaScanline,
	.overline 			= callLuaOverline,

	.getOutline			= getMoonOutline,
	.parse 				= parseCode,
	.eval				= NULL,

	.blockCommentStart 	= NULL,
	.blockCommentEnd 	= NULL,
	.blockStringStart	= NULL,
	.blockStringEnd		= NULL,
	.singleComment 		= "--",

	.keywords 			= MoonKeywords,
	.keywordsCount 		= COUNT_OF(MoonKeywords),

	.api 				= ApiKeywords,
	.apiCount 			= COUNT_OF(ApiKeywords),
};

const tic_script_config* getMoonScriptConfig()
{
	return &MoonSyntaxConfig;
}

#endif /* defined(TIC_BUILD_WITH_MOON) */

#if defined(TIC_BUILD_WITH_FENNEL)

#include "fennel.h"

#define FENNEL_CODE(...) #__VA_ARGS__

static const char* execute_fennel_src = FENNEL_CODE(
  local ok, msg = pcall(require('fennel').eval, ..., {filename="game", correlate=true})
  if(not ok) then return msg end
);

static bool initFennel(tic_mem* tic, const char* code)
{
	tic_machine* machine = (tic_machine*)tic;
	closeLua(tic);

	lua_State* lua = machine->lua = luaL_newstate();
	lua_open_builtins(lua);

	initAPI(machine);

	{
		lua_State* fennel = machine->lua;

		lua_settop(fennel, 0);

		if (luaL_loadbuffer(fennel, (const char *)fennel_lua, fennel_lua_len, "fennel.lua") != LUA_OK)
		{
			machine->data->error(machine->data->data, "failed to load fennel compiler");
			return false;
		}

		lua_call(fennel, 0, 0);

		if (luaL_loadbuffer(fennel, execute_fennel_src, strlen(execute_fennel_src), "execute_fennel") != LUA_OK)
		{
			machine->data->error(machine->data->data, "failed to load fennel compiler");
			return false;
		}

		lua_pushstring(fennel, code);
        lua_call(fennel, 1, 1);
        const char* err = lua_tostring(fennel, -1);

        if (err)
		{
		   	machine->data->error(machine->data->data, err);
	   		return false;
		}
	}

	return true;
}

static const char* const FennelKeywords [] =
{
	"do", "values", "if", "when", "each", "for", "fn", "lambda", "partial",
	"while", "set", "global", "var", "local", "let", "tset",
	"or", "and", "true", "false", "nil", "#", ":", "->", "->>"
};

static const tic_outline_item* getFennelOutline(const char* code, s32* size)
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
		static const char FuncString[] = "(fn ";

		ptr = strstr(ptr, FuncString);

		if(ptr)
		{
			ptr += sizeof FuncString - 1;

			const char* start = ptr;
			const char* end = start;

			while(*ptr)
			{
				char c = *ptr;

				if(c == ' ' || c == '\t' || c == '\n' || c == '[')
				{
					end = ptr;
					break;
				}

				ptr++;
			}

			if(end > start)
			{
				items = items ? realloc(items, (*size + 1) * Size) : malloc(Size);

				items[*size].pos = start - code;
				items[*size].size = end - start;

				(*size)++;
			}
		}
		else break;
	}

	return items;
}

static void evalFennel(tic_mem* tic, const char* code) {
	tic_machine* machine = (tic_machine*)tic;
	lua_State* fennel = machine->lua;

	lua_settop(fennel, 0);

	if (luaL_loadbuffer(fennel, execute_fennel_src, strlen(execute_fennel_src), "execute_fennel") != LUA_OK)
	{
		machine->data->error(machine->data->data, "failed to load fennel compiler");
	}

	lua_pushstring(fennel, code);
	lua_call(fennel, 1, 1);
	const char* err = lua_tostring(fennel, -1);

	if (err)
	{
		machine->data->error(machine->data->data, err);
	}
}


static const tic_script_config FennelSyntaxConfig =
{
	.init 				= initFennel,
	.close 				= closeLua,
	.tick 				= callLuaTick,
	.scanline 			= callLuaScanline,
	.overline 			= callLuaOverline,

	.getOutline			= getFennelOutline,
	.parse 				= parseCode,
	.eval				= evalFennel,

	.blockCommentStart 	= NULL,
	.blockCommentEnd 	= NULL,
	.blockStringStart	= NULL,
	.blockStringEnd		= NULL,
	.singleComment 		= ";",

	.keywords 			= FennelKeywords,
	.keywordsCount 		= COUNT_OF(FennelKeywords),

	.api 				= ApiKeywords,
	.apiCount 			= COUNT_OF(ApiKeywords),
};

const tic_script_config* getFennelConfig()
{
	return &FennelSyntaxConfig;
}

#endif /* defined(TIC_BUILD_WITH_FENNEL) */

#endif /* defined(TIC_BUILD_WITH_LUA) */
