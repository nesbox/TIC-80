// MIT License

// Copyright (c) 2017 @lb_ii // lolbot_iichan@mail.ru

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "machine.h"

#define BF_GLOBAL_MACRO_NAME "//GLOBAL\\"
#define BF_MAX_MACRO_NUM 1024
#define BF_MAX_API_PARAMS 258
#define BF_MAX_API_STRING 256
#define BF_MAX_API_RETURN 3
#define BF_MAX_MEMORY 65536
#define BF_MAX_CALL_RECURSION 16

typedef s16 bfcell;

struct bf_macro
{
	char* name;
	char* code;
};

struct bf_State
{
	//fixed size array of brainfuck data cells and working index
	struct
	{
		bfcell* items;
		s32 index;
	} data;

	//macros are stored here
	struct
	{
		struct bf_macro* items;
		s32 count;
	} macro;

	//buffer for API call id and params
	//grows as stack with '.', purges on ','
	struct
	{
		bfcell items[BF_MAX_API_PARAMS];
		s32 count;
	} call;

	//buffer for storing text API param
	//we need it because bfcell is 16-bit and text is 8-bit
	struct
	{
		bool used;
		char buffer[BF_MAX_API_STRING];
		s32 count;
	} text;
	
	//buffer for storing text API return values
	//grows as stack with bf_pushinteger()
	struct
	{
		bfcell items[BF_MAX_API_RETURN];
		s32 count;
	} result;

	//link to parent
	tic_machine* machine;
};
typedef struct bf_State bf_State;
typedef s32 (*bf_function) (bf_State *bf);



// Errors definitions

enum
{
	BfCodeOk = 0,
	BfCodeUnfinishedMacroDef,
	BfCodeUnfinishedMacroCall,
	BfCodeWrongMacroName,
	BfCodeTooManyMacros,
	BfCodeUnknownMacro,
	BfCodeTooManyApiParams,
	BfCodeNothingToCall,
	BfCodeUnknownCall,
	BfCodeUnimplementedCall,
	BfCodeUnmatchedBrackets,
	BfCodeTextTooLong,
	BfCodeTextNotEnough,
	BfCodeMacroCallOverflow,
	BfCodeMax,
};
static const char* BfCodeErr[] = {
	"No error",
	"ERROR: Macro definition is not complete",
	"ERROR: Macro call is not complete",
	"ERROR: Wrong macro name",
	"ERROR: Too many macros defined",
	"ERROR: Unknown macro called",
	"ERROR: Adding too many params to API call",
	"ERROR: No API selected to call",
	"ERROR: Unknown API called",
	"ERROR: API selected to call is not implemented yet",
	"ERROR: Unmatched loop brackets",
	"ERROR: Text line is too long",
	"ERROR: Text API requires string length and bytes values or zero cell and single integer value",
	"ERROR: Nested macro call overflow",
	"ERROR: Unknown error, sorry",
};



// wrappers to make copypaste from luaapi.c easier

tic_machine* getBfMachine(bf_State* bf)
{
	return bf->machine;
}

void bfL_error(bf_State* bf, const char* err)
{
	bf->machine->data->error(bf->machine->data->data, err);
}

s32 bf_gettop(bf_State* bf)
{
	if(!bf->text.used)
		return bf->call.count - 1;

	return bf->call.count - SDL_max(bf->text.count,1) - 1;
}

bfcell getBfNumber(bf_State* bf, s32 i)
{
	if(bf->text.used)
		i += SDL_max(bf->text.count,1);

	if(i >= bf->call.count)
		return 0;

	return bf->call.items[i];
}

s32 bf_tonumber(bf_State* bf, s32 i)
{
	return (s32)getBfNumber(bf,i);
}

bool bf_toboolean(bf_State* bf, s32 i)
{
	return (bool)getBfNumber(bf,i);
}

static const char* printString(bf_State* bf)
{
	if(!bf->text.used)
		return NULL;
	return bf->text.buffer;
}

void bf_pushinteger(bf_State* bf, s32 i)
{
	if(bf->result.count >= BF_MAX_API_RETURN)
		return;
	bf->result.items[bf->result.count] = i;
	bf->result.count++;
}

void bf_pushboolean(bf_State* bf, bool i)
{
	bf_pushinteger(bf, i?1:0);
}




// API functions

static s32 bf_peek(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 2)
	{
		s32 address = 0x4000*getBfNumber(bf, 1) + getBfNumber(bf, 2);

		if(address >=0 && address < sizeof(tic_ram))
		{
			bf_pushinteger(bf, *((u8*)&getBfMachine(bf)->memory.ram + address));
			return 1;
		}
	}
	else bfL_error(bf, "invalid parameters, peek(addr/0x4000,addr%0x4000)\n");

	return 0;
}

static s32 bf_poke(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 3)
	{
		s32 address = 0x4000*getBfNumber(bf, 1) + getBfNumber(bf, 2);
		u8 value = getBfNumber(bf, 3) & 0xff;

		if(address >=0 && address < sizeof(tic_ram))
		{
			*((u8*)&getBfMachine(bf)->memory.ram + address) = value;
		}
	}
	else bfL_error(bf, "invalid parameters, peek(addr/0x4000,addr%0x4000,value)\n");

	return 0;
}

static s32 bf_peek4(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 2)
	{
		s32 address = 0x8000*getBfNumber(bf, 1) + getBfNumber(bf, 2);

		if(address >= 0 && address < sizeof(tic_ram)*2)
		{
			bf_pushinteger(bf, tic_tool_peek4((u8*)&getBfMachine(bf)->memory.ram, address));
			return 1;
		}		
	}
	else bfL_error(bf, "invalid parameters, peek4(addr/0x8000,addr%0x8000)\n");

	return 0;
}

static s32 bf_poke4(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 3)
	{
		s32 address = 0x8000*getBfNumber(bf, 1) + getBfNumber(bf, 2);
		u8 value = getBfNumber(bf, 3) % 16;

		if(address >= 0 && address < sizeof(tic_ram)*2)
		{
			tic_tool_poke4((u8*)&getBfMachine(bf)->memory.ram, address, value);
		}
	}
	else bfL_error(bf, "invalid parameters, poke4(addr/0x8000,addr%0x8000,value)\n");

	return 0;
}

static s32 bf_cls(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	tic_mem* memory = (tic_mem*)getBfMachine(bf);

	memory->api.clear(memory, top == 1 ? getBfNumber(bf, 1) : 0);

	return 0;
}

static s32 bf_pix(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top >= 2)
	{
		s32 x = getBfNumber(bf, 1);
		s32 y = getBfNumber(bf, 2);
		
		tic_mem* memory = (tic_mem*)getBfMachine(bf);

		if(top >= 3)
		{
			s32 color = getBfNumber(bf, 3);
			memory->api.pixel(memory, x, y, color);
		}
		else
		{
			bf_pushinteger(bf, memory->api.get_pixel(memory, x, y));
			return 1;
		}

	}
	else bfL_error(bf, "invalid parameters, pix(x y [color])\n");

	return 0;
}

static s32 bf_line(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 5)
	{
		s32 x0 = getBfNumber(bf, 1);
		s32 y0 = getBfNumber(bf, 2);
		s32 x1 = getBfNumber(bf, 3);
		s32 y1 = getBfNumber(bf, 4);
		s32 color = getBfNumber(bf, 5);

		tic_mem* memory = (tic_mem*)getBfMachine(bf);

		memory->api.line(memory, x0, y0, x1, y1, color);
	}
	else bfL_error(bf, "invalid parameters, line(x0,y0,x1,y1,color)\n");

	return 0;
}

static s32 bf_rect(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 5)
	{
		s32 x = getBfNumber(bf, 1);
		s32 y = getBfNumber(bf, 2);
		s32 w = getBfNumber(bf, 3);
		s32 h = getBfNumber(bf, 4);
		s32 color = getBfNumber(bf, 5);

		tic_mem* memory = (tic_mem*)getBfMachine(bf);

		memory->api.rect(memory, x, y, w, h, color);
	}
	else bfL_error(bf, "invalid parameters, rect(x,y,w,h,color)\n");

	return 0;
}

static s32 bf_rectb(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 5)
	{
		s32 x = getBfNumber(bf, 1);
		s32 y = getBfNumber(bf, 2);
		s32 w = getBfNumber(bf, 3);
		s32 h = getBfNumber(bf, 4);
		s32 color = getBfNumber(bf, 5);

		tic_mem* memory = (tic_mem*)getBfMachine(bf);

		memory->api.rect_border(memory, x, y, w, h, color);
	}
	else bfL_error(bf, "invalid parameters, rectb(x,y,w,h,color)\n");

	return 0;
}

static s32 bf_circ(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 4)
	{
		s32 radius = getBfNumber(bf, 3);
		if(radius < 0) return 0;
		
		s32 x = getBfNumber(bf, 1);
		s32 y = getBfNumber(bf, 2);
		s32 color = getBfNumber(bf, 4);

		tic_mem* memory = (tic_mem*)getBfMachine(bf);

		memory->api.circle(memory, x, y, radius, color);
	}
	else bfL_error(bf, "invalid parameters, circ(x,y,radius,color)\n");

	return 0;
}

static s32 bf_circb(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 4)
	{
		s32 radius = getBfNumber(bf, 3);
		if(radius < 0) return 0;

		s32 x = getBfNumber(bf, 1);
		s32 y = getBfNumber(bf, 2);
		s32 color = getBfNumber(bf, 4);

		tic_mem* memory = (tic_mem*)getBfMachine(bf);

		memory->api.circle_border(memory, x, y, radius, color);
	}
	else bfL_error(bf, "invalid parameters, circb(x,y,radius,color)\n");

	return 0;
}

static s32 bf_tri(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 7)
	{
		s32 pt[6];

		for(s32 i = 0; i < COUNT_OF(pt); i++)
			pt[i] = getBfNumber(bf, i+1);
		
		s32 color = getBfNumber(bf, 7);

		tic_mem* memory = (tic_mem*)getBfMachine(bf);

		memory->api.tri(memory, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);
	}
	else bfL_error(bf, "invalid parameters, tri(x1,y1,x2,y2,x3,y3,color)\n");

	return 0;
}

static s32 bf_textri(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if (top >= 12)
	{
		float pt[12];

		for (s32 i = 0; i < COUNT_OF(pt); i++)
			pt[i] = (float)bf_tonumber(bf, i + 1);

		tic_mem* memory = (tic_mem*)getBfMachine(bf);
		u8 chroma = 0xff;
		bool use_map = false;

		//	check for use map 
		if (top >= 13)
			use_map = bf_toboolean(bf, 13);
		//	check for chroma 
		if (top >= 14)
			chroma = (u8)getBfNumber(bf, 14);

		memory->api.textri(memory, pt[0], pt[1],	//	xy 1
									pt[2], pt[3],	//	xy 2
									pt[4], pt[5],	//	xy 3
									pt[6], pt[7],	//	uv 1
									pt[8], pt[9],	//	uv 2
									pt[10], pt[11], //	uv 3
									use_map,		// use map
									chroma);		// chroma
	}
	else bfL_error(bf, "invalid parameters, textri(x1,y1,x2,y2,x3,y3,u1,v1,u2,v2,u3,v3,[use_map=false],[chroma=off])\n");
	return 0;
}


static s32 bf_clip(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 0)
	{
		tic_mem* memory = (tic_mem*)getBfMachine(bf);

		memory->api.clip(memory, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
	}
	else if(top == 4)
	{
		s32 x = getBfNumber(bf, 1);
		s32 y = getBfNumber(bf, 2);
		s32 w = getBfNumber(bf, 3);
		s32 h = getBfNumber(bf, 4);

		tic_mem* memory = (tic_mem*)getBfMachine(bf);

		memory->api.clip((tic_mem*)getBfMachine(bf), x, y, w, h);
	}
	else bfL_error(bf, "invalid parameters, use clip(x,y,w,h) or clip()\n");

	return 0;
}

static s32 bf_btnp(bf_State* bf)
{
	tic_machine* machine = getBfMachine(bf);
	tic_mem* memory = (tic_mem*)machine;

	if(machine->memory.input == tic_gamepad_input)
	{
		s32 top = bf_gettop(bf);

		if (top == 0)
		{
			bf_pushinteger(bf, memory->api.btnp(memory, -1, -1, -1));
		}
		else if(top == 1)
		{
			s32 index = getBfNumber(bf, 1) & 0xf;

			bf_pushboolean(bf, memory->api.btnp(memory, index, -1, -1));
		}
		else if (top == 3)
		{
			s32 index = getBfNumber(bf, 1) & 0xf;
			u32 hold = getBfNumber(bf, 2);
			u32 period = getBfNumber(bf, 3);

			bf_pushboolean(bf, memory->api.btnp(memory, index, hold, period));
		}
		else bfL_error(bf, "invalid params, btnp [ id [ hold period ] ]\n");

		return 1;
	}
	else bfL_error(bf, "gamepad input not declared in metadata\n");

	return 0;
}

static s32 bf_btn(bf_State* bf)
{
	tic_machine* machine = getBfMachine(bf);

	if(machine->memory.input == tic_gamepad_input)
	{
		s32 top = bf_gettop(bf);

		if (top == 0)
		{
			bf_pushinteger(bf, machine->memory.ram.vram.input.gamepad.data);
		}
		else if (top == 1)
		{
			s32 index = getBfNumber(bf, 1) & 0xf;
			bf_pushboolean(bf, machine->memory.ram.vram.input.gamepad.data & (1 << index));
		}
		else bfL_error(bf, "invalid params, btn [ id ]\n");

		return 1;		
	}
	else bfL_error(bf, "gamepad input not declared in metadata\n");

	return 0;
}

static s32 bf_spr(bf_State* bf)
{
	s32 top = bf_gettop(bf);

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
		index = getBfNumber(bf, 1);

		if(top >= 3)
		{
			x = getBfNumber(bf, 2);
			y = getBfNumber(bf, 3);

			if(top >= 4)
			{
				// TODO: transparent color list?!
				colors[0] = getBfNumber(bf, 4);
				count = 1;

				if(top >= 5)
				{
					scale = getBfNumber(bf, 5);

					if(top >= 6)
					{
						flip = getBfNumber(bf, 6);

						if(top >= 7)
						{
							rotate = getBfNumber(bf, 7);

							if(top >= 9)
							{
								w = getBfNumber(bf, 8);
								h = getBfNumber(bf, 9);
							}
						}
					}
				}
			}
		}
	}

	tic_mem* memory = (tic_mem*)getBfMachine(bf);

	memory->api.sprite_ex(memory, &memory->ram.tiles, index, x, y, w, h, colors, count, scale, flip, rotate);

	return 0;
}

static s32 bf_mget(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 2)
	{
		s32 x = getBfNumber(bf, 1);
		s32 y = getBfNumber(bf, 2);

		tic_mem* memory = (tic_mem*)getBfMachine(bf);

		u8 value = memory->api.map_get(memory, &memory->ram.map, x, y);
		bf_pushinteger(bf, value);
		return 1;
	}
	else bfL_error(bf, "invalid params, mget(x,y)\n");

	return 0;
}

static s32 bf_mset(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 3)
	{
		s32 x = getBfNumber(bf, 1);
		s32 y = getBfNumber(bf, 2);
		u8 val = getBfNumber(bf, 3);

		tic_mem* memory = (tic_mem*)getBfMachine(bf);

		memory->api.map_set(memory, &memory->ram.map, x, y, val);
	}
	else bfL_error(bf, "invalid params, mset(x,y)\n");

	return 0;
}

static s32 bf_map(bf_State* bf)
{
	s32 x = 0;
	s32 y = 0;
	s32 w = TIC_MAP_SCREEN_WIDTH;
	s32 h = TIC_MAP_SCREEN_HEIGHT;
	s32 sx = 0;
	s32 sy = 0;
	u8 chromakey = -1;
	s32 scale = 1;

	s32 top = bf_gettop(bf);

	if(top >= 2) 
	{
		x = getBfNumber(bf, 1);
		y = getBfNumber(bf, 2);

		if(top >= 4)
		{
			w = getBfNumber(bf, 3);
			h = getBfNumber(bf, 4);

			if(top >= 6)
			{
				sx = getBfNumber(bf, 5);
				sy = getBfNumber(bf, 6);

				if(top >= 7)
				{
					chromakey = getBfNumber(bf, 7);

					if(top >= 8)
					{
						scale = getBfNumber(bf, 8);

						if(top >= 9)
						{
							 bfL_error(bf, "remap API is not implemented yet\n"); // TODO: think of something
							 return 0;
						}
					}
				}
			}
		}
	}

	tic_mem* memory = (tic_mem*)getBfMachine(bf);

	memory->api.map((tic_mem*)getBfMachine(bf), &memory->ram.map, &memory->ram.tiles, x, y, w, h, sx, sy, chromakey, scale);

	return 0;
}

static s32 bf_music(bf_State* bf)
{
	s32 top = bf_gettop(bf);
	tic_mem* memory = (tic_mem*)getBfMachine(bf);

	if(top == 0) memory->api.music(memory, -1, 0, 0, false);
	else if(top >= 1)
	{
		memory->api.music(memory, -1, 0, 0, false);

		s32 track = getBfNumber(bf, 1);
		s32 frame = -1;
		s32 row = -1;
		bool loop = true;

		if(top >= 2)
		{
			frame = getBfNumber(bf, 2);

			if(top >= 3)
			{
				row = getBfNumber(bf, 3);

				if(top >= 4)
				{
					loop = bf_toboolean(bf, 4);
				}
			}
		}

		memory->api.music(memory, track, frame, row, loop);
	}
	else bfL_error(bf, "invalid params, use music(track)\n");

	return 0;
}

static s32 bf_sfx(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top >= 1)
	{
		tic_mem* memory = (tic_mem*)getBfMachine(bf);

		s32 note = -1;
		s32 octave = -1;
		s32 duration = -1;
		s32 channel = 0;
		s32 volume = MAX_VOLUME;
		s32 speed = SFX_DEF_SPEED;

		s32 index = getBfNumber(bf, 1);

		if(index < SFX_COUNT)
		{
			if (index >= 0)
			{
				tic_sound_effect* effect = memory->ram.sfx.data + index;

				note = effect->note;
				octave = effect->octave;
				speed = effect->speed;
			}

			if(top >= 2)
			{
				s32 id = getBfNumber(bf, 2);
				note = id % NOTES;
				octave = id / NOTES;

				if(top >= 3)
				{
					duration = getBfNumber(bf, 3);

					if(top >= 4)
					{
						channel = getBfNumber(bf, 4);

						if(top >= 5)
						{
							volume = getBfNumber(bf, 5);

							if(top >= 6)
							{
								speed = getBfNumber(bf, 6);
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
			else bfL_error(bf, "unknown channel\n");
		}
		else bfL_error(bf, "unknown sfx index\n");
	}
	else bfL_error(bf, "invalid sfx params\n");

	return 0;
}

static s32 bf_sync(bf_State* bf)
{
	tic_mem* memory = (tic_mem*)getBfMachine(bf);

	bool toCart = true;
	
	if(bf_gettop(bf) >= 1)
		toCart = bf_toboolean(bf, 1);

	memory->api.sync(memory, toCart);

	return 0;
}

static s32 bf_memcpy(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 5)
	{
		s32 dest = 0x4000*getBfNumber(bf, 1) + getBfNumber(bf, 2);
		s32 src = 0x4000*getBfNumber(bf, 3) + getBfNumber(bf, 4);
		s32 size = getBfNumber(bf, 5);
		s32 bound = sizeof(tic_ram) - size;

		if(size >= 0 && size <= sizeof(tic_ram) && dest >= 0 && src >= 0 && dest <= bound && src <= bound)
		{
			u8* base = (u8*)&getBfMachine(bf)->memory;
			memcpy(base + dest, base + src, size);
			return 0;
		}
	}

	bfL_error(bf, "invalid params, memcpy(dest/0x4000,dest%0x4000,src/0x4000,src%0x4000,size)\n");

	return 0;
}

static s32 bf_memset(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top == 4)
	{
		s32 dest = 0x4000*getBfNumber(bf, 1) + getBfNumber(bf, 2);
		u8 value = getBfNumber(bf, 3);
		s32 size = getBfNumber(bf, 4);
		s32 bound = sizeof(tic_ram) - size;

		if(size >= 0 && size <= sizeof(tic_ram) && dest >= 0 && dest <= bound)
		{
			u8* base = (u8*)&getBfMachine(bf)->memory;
			memset(base + dest, value, size);
			return 0;
		}
	}

	bfL_error(bf, "invalid params, memset(dest/0x4000,dest%0x4000,val,size)\n");

	return 0;
}

static s32 bf_font(bf_State* bf)
{
	tic_mem* memory = (tic_mem*)getBfMachine(bf);
	s32 top = bf_gettop(bf);

	if(top >= 1)
	{
		const char* text = printString(bf);
		s32 x = 0;
		s32 y = 0;
		s32 width = TIC_SPRITESIZE;
		s32 height = TIC_SPRITESIZE;
		u8 chromakey = 0;
		bool fixed = false;
		s32 scale = 1;

		if(top >= 3)
		{
			x = getBfNumber(bf, 2);
			y = getBfNumber(bf, 3);

			if(top >= 4)
			{
				chromakey = getBfNumber(bf, 4);

				if(top >= 6)
				{
					width = getBfNumber(bf, 5);
					height = getBfNumber(bf, 6);

					if(top >= 7)
					{
						fixed = bf_toboolean(bf, 7);

						if(top >= 8)
						{
							scale = getBfNumber(bf, 8);
						}
					}
				}
			}
		}

		if(scale == 0)
		{
			bf_pushinteger(bf, 0);
			return 1;
		}

		s32 size = drawText(memory, text, x, y, width, height, chromakey, scale, fixed ? drawSpriteFont : drawFixedSpriteFont);

		bf_pushinteger(bf, size);


		return 1;
	}

	return 0;
}

static s32 bf_print(bf_State* bf)
{
	s32 top = bf_gettop(bf);

	if(top >= 1) 
	{
		tic_mem* memory = (tic_mem*)getBfMachine(bf);

		s32 x = 0;
		s32 y = 0;
		s32 color = TIC_PALETTE_SIZE-1;
		bool fixed = false;
		s32 scale = 1;

		const char* text = printString(bf);

		if(top >= 3)
		{
			x = getBfNumber(bf, 2);
			y = getBfNumber(bf, 3);

			if(top >= 4)
			{
				color = getBfNumber(bf, 4) % TIC_PALETTE_SIZE;

				if(top >= 5)
				{
					fixed = bf_toboolean(bf, 5);

					if(top >= 6)
					{
						scale = getBfNumber(bf, 6);
					}
				}
			}
		}

		if(scale == 0)
		{
			bf_pushinteger(bf, 0);
			return 1;
		}

		s32 size = memory->api.text_ex(memory, text ? text : "nil", x, y, color, fixed, scale);

		bf_pushinteger(bf, size);

		return 1;
	}

	return 0;
}

static s32 bf_trace(bf_State *bf)
{
	s32 top = bf_gettop(bf);
	tic_machine* machine = getBfMachine(bf);


	if(top >= 1)
	{
		const char* text = printString(bf);
		u8 color = tic_color_white;

		if(top >= 2)
		{
			color = getBfNumber(bf, 2);
		}

		machine->data->trace(machine->data->data, text ? text : "nil", color);
	}

	return 0;
}

static s32 bf_pmem(bf_State *bf)
{
	s32 top = bf_gettop(bf);
	tic_machine* machine = getBfMachine(bf);
	tic_mem* memory = &machine->memory;

	if(top == 1 || top == 3)
	{
		u32 index = getBfNumber(bf, 1);

		if(index >= 0 && index < TIC_PERSISTENT_SIZE)
		{
			s32 val = memory->ram.persistent.data[index];

			if(top == 3)
			{
				s32 nval = 65536*getBfNumber(bf, 2) + getBfNumber(bf, 3);
				memory->ram.persistent.data[index] = nval;
			}

			bf_pushinteger(bf, val / 65536 );
			bf_pushinteger(bf, val % 65536 );

			return 2;			
		}
		bfL_error(bf, "invalid persistent memory index\n");
	}
	else bfL_error(bf, "invalid params, pmem(index [high low]) -> high low\n");

	return 0;
}

static s32 bf_time(bf_State *bf)
{
	tic_mem* memory = (tic_mem*)getBfMachine(bf);

	s32 time = memory->api.time(memory);
	bf_pushinteger(bf, time / 1000 );
	bf_pushinteger(bf, time % 1000 );

	return 2;
}

static s32 bf_exit(bf_State *bf)
{
	tic_machine* machine = getBfMachine(bf);

	machine->data->exit(machine->data->data);
	
	return 0;
}

static s32 bf_mouse(bf_State *bf)
{
	tic_machine* machine = getBfMachine(bf);

	if(machine->memory.input == tic_mouse_input)
	{
		u16 data = machine->memory.ram.vram.input.gamepad.data;

		bf_pushinteger(bf, (data & 0x7fff) % TIC80_WIDTH);
		bf_pushinteger(bf, (data & 0x7fff) / TIC80_WIDTH);
		bf_pushboolean(bf, data >> 15);

		return 3;		
	}
	else bfL_error(bf, "mouse input not declared in metadata\n");

	return 0;
}



// API functions list

static const char* const ApiKeywords[] = API_KEYWORDS;
static const bf_function ApiFunc[] = 
{
	NULL, NULL, NULL, bf_print, bf_cls, bf_pix, bf_line, bf_rect, 
	bf_rectb, bf_spr, bf_btn, bf_btnp, bf_sfx, bf_map, bf_mget, 
	bf_mset, bf_peek, bf_poke, bf_peek4, bf_poke4, bf_memcpy, 
	bf_memset, bf_trace, bf_pmem, bf_time, bf_exit, bf_font, bf_mouse, 
	bf_circ, bf_circb, bf_tri, bf_textri, bf_clip, bf_music, bf_sync, 
};

// demos checklist
// * bf_btnp:			bf_cls, bf_rect, bf_btnp
// * bf_circ:			bf_cls, bf_poke, bf_circ
// * bf_coordinates:	bf_print, bf_cls, bf_pix, bf_line
// * bf_demo:			bf_print, bf_cls, bf_spr, bf_btn
// * bf_exit:			bf_print, bf_cls, bf_btn, bf_exit
// * bf_font:			bf_cls, bf_font
// * bf_mouse2:			bf_cls, bf_mouse, bf_line, bf_circ, bf_print
// * bf_mset:			bf_cls, bf_map, bf_mset, bf_sync
// * bf_pmem:			bf_print, bf_cls, bf_pmem
// * bf_scanline:		scanline, bf_cls, bf_poke, bf_poke4
// * bf_sfx:			bf_print, bf_cls, bf_spr, bf_btnp, bf_sfx
// * bf_time:			bf_print, bf_cls, bf_time
// TODO: bf_rectb, bf_mget, bf_peek, bf_peek4, bf_memcpy, bf_memset, bf_trace, bf_circb, bf_tri, bf_textri, bf_clip, bf_music 



// brainfuck engine

s32 getBfMacro(bf_State* bf, const char* name, s32 namelen)
{
	for(s32 i=0;i<bf->macro.count;i++)
	{
		if(strlen(bf->macro.items[i].name)==namelen && strncmp(bf->macro.items[i].name,name,namelen)==0)
			return i;
	}
	return -1;
}

s32 extractBfCode(bf_State* bf, const char* code, size_t len, const char* name, size_t namelen)
{
	s32 err = 0;

	char* tmp = malloc(len+1); //beginning with buffer of the same size as original code
	size_t j = 0;

	bool comment = false;
	s32 macro_def_state = 0;
	const char* macro_def_name = NULL;
	const char* macro_def_code = NULL;
	bool macro_call = false;

	for(size_t i=0;i<len && !err;i++)
	{
		if(code[i] == '#')
			comment = true;
		if(code[i] == '\n')
			comment = false;
		if(!comment)
		{
			if(code[i] == '/')
			{
				if(macro_def_state==0)
					macro_def_name = &(code[i])+1;
				if(macro_def_state==1)
					macro_def_code = &(code[i])+1;
				if(macro_def_state==2)
				{
					err = err ? err : extractBfCode(bf, 
							macro_def_code, &(code[i])-macro_def_code, 
							macro_def_name, macro_def_code-macro_def_name-1);
				}
				macro_def_state = (macro_def_state+1)%3;
			}

			if(code[i] == '\\')
			{
				macro_call = !macro_call;
				err = err ? err : (macro_def_state==1 ? BfCodeWrongMacroName : 0);
			}

			if(!comment && !macro_def_state && (strchr("+-,.<>[]?\\",code[i])||macro_call))
			{
				tmp[j] = code[i];
				j++;
			}
		}
	}

	tmp[j] = 0;
	tmp = realloc(tmp,j+1); //reallocing smaller buffer

	err = err ? err : (macro_def_state ? BfCodeUnfinishedMacroDef : 0);
	err = err ? err : (macro_call ? BfCodeUnfinishedMacroCall : 0);
	err = err ? err : (bf->macro.count >= BF_MAX_MACRO_NUM ? BfCodeTooManyMacros : 0);
	if(err)
	{
		free(tmp);
		return err;
	}

	s32 macroId = getBfMacro(bf, name, namelen);
	if(macroId<0)
	{
		struct bf_macro* macro = &(bf->macro.items[bf->macro.count]);
		macro->name = malloc(namelen+1);
		strncpy(macro->name,name,namelen);
		macro->name[namelen] = 0;
		macro->code = tmp;

		bf->macro.count++;
	}
	else
	{
		free(bf->macro.items[macroId].code);
		bf->macro.items[macroId].code = tmp;
	}
	return 0;
}

s32 flatBfCode(bf_State* bf, const char* name, s32 namelen, s32 recursion)
{
	s32 macroId = getBfMacro(bf, name, namelen);
	if(macroId<0)
		return BfCodeUnknownMacro;

	struct bf_macro* macro = &(bf->macro.items[macroId]);
	char* macro_call_name=NULL;
	for(s32 i=0;i<strlen(macro->code);i++)
	{
		if(!macro_call_name)
		{
			if(macro->code[i]=='\\')
				macro_call_name = &(macro->code[i])+1;
		}
		else if(macro->code[i]=='\\')
		{
			if(recursion >= BF_MAX_CALL_RECURSION)
				return BfCodeMacroCallOverflow;

			s32 macro_call_len = &(macro->code[i])-macro_call_name;
			s32 err = flatBfCode(bf,macro_call_name,macro_call_len, recursion + 1);
			if(err)
				return err;

			char* inner = bf->macro.items[getBfMacro(bf, macro_call_name, macro_call_len)].code;
			macro->code = realloc(macro->code, strlen(macro->code)+strlen(inner)-macro_call_len+1);
			for(s32 j=strlen(macro->code)-i-1;j>=0;j--)
				macro->code[i+strlen(inner)-macro_call_len+j-1] = macro->code[i+j+1];
			for(s32 j=0;j<strlen(inner);j++)
				macro->code[i-macro_call_len+j-1] = inner[j];
			i -= macro_call_len+2;//FIXME
			macro_call_name = NULL;
		}
	}
	return 0;
}

s32 runBfCodeTrace(bf_State* bf)
{
	char tmp[16];
	sprintf(tmp,"%d:%d",bf->data.index,bf->data.items[bf->data.index]);
	bf->machine->data->trace(bf->machine->data->data, tmp, tic_color_white);
	return 0;
}

s32 runBfCodeLoop(bf_State* bf, char* code, s32* idx)
{
	s32 codelen = strlen(code);
	s32 depth = 0;
	s32 i = *idx;

	if(code[i]=='[' && bf->data.items[bf->data.index]==0)
	{
		for(i=i;i<codelen;i++)
		{
			if(code[i]=='[')
				depth++;
			if(code[i]==']')
				depth--;
			if(depth==0)
				break;
		}
	}
	else if(code[i]==']' && bf->data.items[bf->data.index]!=0)
	{
		for(i=i;i>=0;i--)
		{
			if(code[i]=='[')
				depth++;
			if(code[i]==']')
				depth--;
			if(depth==0)
				break;
		}
	}

	if(depth)
		return BfCodeUnmatchedBrackets;

	*idx = i;
	return 0;
}

s32 runBfCodeExec(bf_State* bf)
{
	if(!bf->call.count)
		return BfCodeNothingToCall;
	
	bfcell api = bf->call.items[0];
	if(api<0 || api>=COUNT_OF(ApiFunc))
		return BfCodeUnknownCall;
	if(!ApiFunc[api])
		return BfCodeUnimplementedCall;
	
	bf->text.used = ApiFunc[api]==bf_print || ApiFunc[api]==bf_font || ApiFunc[api]==bf_trace;
	if(bf->text.used)
	{
		bf->text.count = bf->call.items[1];

		if(bf->call.count < 3 || bf->text.count < 0 || bf->call.count < bf->text.count + 2)
			return BfCodeTextNotEnough;
		if(bf->text.count >= BF_MAX_API_STRING)
			return BfCodeTextTooLong;

		if(bf->text.count==0)
			sprintf(bf->text.buffer,"%d",bf->call.items[2]);
		else
		{
			for(s32 i=0; i<bf->text.count; i++)
				bf->text.buffer[i] = (char) bf->call.items[i+2];
			bf->text.buffer[bf->text.count] = 0;
		}
	}
	
	for(int k=0;k<ApiFunc[api](bf);k++)
		bf->data.items[(bf->data.index+k) % BF_MAX_MEMORY]=bf->result.items[k];
	
	bf->call.count=0;
	bf->result.count=0;
	return 0;
}

s32 runBfCode(bf_State* bf, const char* name, s32 namelen, s32 recursion)
{
	s32 macroId = getBfMacro(bf, name, namelen);
	if(macroId<0)
		return BfCodeUnknownMacro;

	char* code = bf->macro.items[macroId].code;
	s32 err = 0;
	s32 codelen = strlen(code);
	char* macro_call_name=NULL;
	for(s32 i=0;i<codelen && !err;i++)
	{
		if(!macro_call_name)
		{
			if(code[i]=='\\')
				macro_call_name = &(code[i])+1;
			if(code[i]=='>')
				bf->data.index = (bf->data.index+1) % BF_MAX_MEMORY;
			if(code[i]=='<')
				bf->data.index = (bf->data.index-1) % BF_MAX_MEMORY;
			if(code[i]=='+')
				bf->data.items[bf->data.index]++;
			if(code[i]=='-')
				bf->data.items[bf->data.index]--;
			if(code[i]=='[' || code[i]==']')
				err = runBfCodeLoop(bf, code, &i);
			if(code[i]=='?')
				err = runBfCodeTrace(bf);
			if(code[i]==',')
				err = runBfCodeExec(bf);
			if(code[i]=='.')
			{
				if(bf->call.count >= BF_MAX_API_PARAMS)
					return BfCodeTooManyApiParams;
				bf->call.items[bf->call.count] = bf->data.items[bf->data.index];
				bf->call.count++;
			}
		}
		else if(code[i]=='\\')
		{
			if(recursion >= BF_MAX_CALL_RECURSION)
				return BfCodeMacroCallOverflow;
			err = runBfCode(bf,macro_call_name,&(code[i])-macro_call_name, recursion + 1);
			macro_call_name = NULL;
		}
	}
	return err;
}

void registerBfFunction(bf_State* bf, const char* name, s32 idx)
{
	char code[1024];
	sprintf(code,"[-]");
	for(s32 i=0;i<idx;i++)
		code[i+3] = '+';
	sprintf(code+idx+3,".[-]");
	extractBfCode(bf,code,strlen(code),name,strlen(name));
}

s32 initBfLibrary(bf_State* bf)
{
	extractBfCode(bf,"",0,ApiKeywords[1],strlen(ApiKeywords[1]));
	extractBfCode(bf,"",0,ApiKeywords[2],strlen(ApiKeywords[2]));

	for (s32 i = 0; i < COUNT_OF(ApiFunc); i++)
		if (ApiFunc[i])
			registerBfFunction(bf, ApiKeywords[i], i);
	return 0;
}



// public functions

void closeBrainfuck(tic_machine* machine)
{
	if(machine->bf)
	{
    	for(s32 i=0;i<machine->bf->macro.count;i++)
    	{
    		free(machine->bf->macro.items[i].code);
			free(machine->bf->macro.items[i].name);
		}
		free(machine->bf->macro.items);
		free(machine->bf->data.items);
		free(machine->bf);
		machine->bf = NULL;
	}
}

bool initBrainfuck(tic_machine* machine, const char* code)
{
	closeBrainfuck(machine);

	machine->bf = calloc(1,sizeof(struct bf_State));
	machine->bf->macro.items = calloc(BF_MAX_MACRO_NUM,sizeof(struct bf_macro));
	machine->bf->data.items = calloc(BF_MAX_MEMORY,sizeof(bfcell));
	machine->bf->machine = machine;

	s32 res = initBfLibrary(machine->bf);
	if(!res)
		res = extractBfCode(machine->bf,code,strlen(code),BF_GLOBAL_MACRO_NAME,strlen(BF_GLOBAL_MACRO_NAME));
	if(!res)
		res = flatBfCode(machine->bf, BF_GLOBAL_MACRO_NAME, strlen(BF_GLOBAL_MACRO_NAME), 0);
	if(!res)
		res = flatBfCode(machine->bf, ApiKeywords[0], strlen(ApiKeywords[0]), 0);
	if(!res)
		res = flatBfCode(machine->bf, ApiKeywords[1], strlen(ApiKeywords[1]), 0);
	if(!res)
		res = flatBfCode(machine->bf, ApiKeywords[2], strlen(ApiKeywords[2]), 0);
	if(!res)
		res = runBfCode(machine->bf, BF_GLOBAL_MACRO_NAME, strlen(BF_GLOBAL_MACRO_NAME), 0);
	
	if(res)
	{
		machine->data->error(machine->data->data, BfCodeErr[res>BfCodeMax?BfCodeMax:res]);
		closeBrainfuck(machine);
		return false;
	}

	return true;
}

void callBrainfuckTick(tic_machine* machine)
{
	if(machine->bf)
	{
		machine->bf->data.index = 0;
		machine->bf->data.items[0] = 0;
		s32 res = runBfCode(machine->bf, ApiKeywords[0], strlen(ApiKeywords[0]), 0);
		if(res)
			machine->data->error(machine->data->data, BfCodeErr[res>BfCodeMax?BfCodeMax:res]);
	}
}

void callBrainfuckScanline(tic_mem* memory, s32 row, void* data)
{
	tic_machine* machine = (tic_machine*)memory;
	if(machine->bf)
	{
		const char* ScanlineFunc = ApiKeywords[1];

		machine->bf->data.index = 0;
		machine->bf->data.items[0] = row;
		s32 res = runBfCode(machine->bf, ScanlineFunc, strlen(ScanlineFunc), 0);
		if(res)
			machine->data->error(machine->data->data, BfCodeErr[res>BfCodeMax?BfCodeMax:res]);
	}
}

void callBrainfuckOverlap(tic_mem* memory, void* data)
{
	tic_machine* machine = (tic_machine*)memory;
	if(machine->bf)
	{
		const char* OvrFunc = ApiKeywords[2];

		machine->bf->data.index = 0;
		machine->bf->data.items[0] = 0;
		s32 res = runBfCode(machine->bf, OvrFunc, strlen(OvrFunc), 0);
		if(res)
			machine->data->error(machine->data->data, BfCodeErr[res>BfCodeMax?BfCodeMax:res]);
	}
}
