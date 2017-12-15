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
#include "tools.h"

#include "ext/duktape/duktape.h"

static const char TicMachine[] = "_TIC80";

void closeJavascript(tic_machine* machine)
{
	if(machine->js)
	{
		duk_destroy_heap(machine->js);
		machine->js = NULL;
	}
}

static tic_machine* getDukMachine(duk_context* duk)
{
	duk_push_global_stash(duk);
	duk_get_prop_string(duk, -1, TicMachine);
	tic_machine* machine = duk_to_pointer(duk, -1);
	duk_pop_2(duk);

	return machine;
}

static duk_ret_t duk_print(duk_context* duk)
{
	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	const char* text = duk_is_null_or_undefined(duk, 0) ? "" : duk_to_string(duk, 0);
	s32 x = duk_is_null_or_undefined(duk, 1) ? 0 : duk_to_int(duk, 1);
	s32 y = duk_is_null_or_undefined(duk, 2) ? 0 : duk_to_int(duk, 2);
	s32 color = duk_is_null_or_undefined(duk, 3) ? (TIC_PALETTE_SIZE-1) : duk_to_int(duk, 3);
	bool fixed = duk_is_null_or_undefined(duk, 4) ? false : duk_to_boolean(duk, 4);
	s32 scale = duk_is_null_or_undefined(duk, 5) ? 1 : duk_to_int(duk, 5);

	s32 size = memory->api.text_ex(memory, text ? text : "nil", x, y, color, fixed, scale);

	duk_push_uint(duk, size);

	return 1;
}

static duk_ret_t duk_cls(duk_context* duk)
{
	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	memory->api.clear(memory, duk_is_null_or_undefined(duk, 0) ? 0 : duk_to_int(duk, 0));

	return 0;
}

static duk_ret_t duk_pix(duk_context* duk)
{
	s32 x = duk_to_int(duk, 0);
	s32 y = duk_to_int(duk, 1);

	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	if(duk_is_null_or_undefined(duk, 2))
	{
		duk_push_uint(duk, memory->api.get_pixel(memory, x, y));
		return 1;
	}
	else
	{
		s32 color = duk_to_int(duk, 2);
		memory->api.pixel(memory, x, y, color);
	}

	return 0;
}

static duk_ret_t duk_line(duk_context* duk)
{
	s32 x0 = duk_to_int(duk, 0);
	s32 y0 = duk_to_int(duk, 1);
	s32 x1 = duk_to_int(duk, 2);
	s32 y1 = duk_to_int(duk, 3);
	s32 color = duk_to_int(duk, 4);

	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	memory->api.line(memory, x0, y0, x1, y1, color);

	return 0;
}

static duk_ret_t duk_rect(duk_context* duk)
{
	s32 x = duk_to_int(duk, 0);
	s32 y = duk_to_int(duk, 1);
	s32 w = duk_to_int(duk, 2);
	s32 h = duk_to_int(duk, 3);
	s32 color = duk_to_int(duk, 4);

	tic_mem* memory = (tic_mem*)getDukMachine(duk);
	memory->api.rect(memory, x, y, w, h, color);

	return 0;
}

static duk_ret_t duk_rectb(duk_context* duk)
{
	s32 x = duk_to_int(duk, 0);
	s32 y = duk_to_int(duk, 1);
	s32 w = duk_to_int(duk, 2);
	s32 h = duk_to_int(duk, 3);
	s32 color = duk_to_int(duk, 4);

	tic_mem* memory = (tic_mem*)getDukMachine(duk);
	memory->api.rect_border(memory, x, y, w, h, color);

	return 0;
}

static duk_ret_t duk_spr(duk_context* duk)
{
	static u8 colors[TIC_PALETTE_SIZE];
	s32 count = 0;

	s32 index = duk_is_null_or_undefined(duk, 0) ? 0						: duk_to_int(duk, 0);
	s32 x = duk_is_null_or_undefined(duk, 1) ? 0							: duk_to_int(duk, 1);
	s32 y = duk_is_null_or_undefined(duk, 2) ? 0							: duk_to_int(duk, 2);

	{
		if(!duk_is_null_or_undefined(duk, 3))
		{
			if(duk_is_array(duk, 3))
			{
				for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
				{
					duk_get_prop_index(duk, 3, i);
					if(duk_is_null_or_undefined(duk, -1))
					{
						duk_pop(duk);
						break;
					}
					else
					{
						colors[i] = duk_to_int(duk, -1);
						count++;
						duk_pop(duk);
					}					
				}
			}
			else
			{
				colors[0] = duk_to_int(duk, 3);
				count = 1;
			}
		}
	}

	s32 scale = duk_is_null_or_undefined(duk, 4) ? 1						: duk_to_int(duk, 4);
	tic_flip flip = duk_is_null_or_undefined(duk, 5) ? tic_no_flip			: duk_to_int(duk, 5);
	tic_rotate rotate = duk_is_null_or_undefined(duk, 6) ? tic_no_rotate	: duk_to_int(duk, 6);
	s32 w = duk_is_null_or_undefined(duk, 7) ? 1							: duk_to_int(duk, 7);
	s32 h = duk_is_null_or_undefined(duk, 8) ? 1							: duk_to_int(duk, 8);

	tic_mem* memory = (tic_mem*)getDukMachine(duk);
	memory->api.sprite_ex(memory, &memory->ram.tiles, index, x, y, w, h, colors, count, scale, flip, rotate);

	return 0;
}

static duk_ret_t duk_btn(duk_context* duk)
{
	tic_machine* machine = getDukMachine(duk);

	if(machine->memory.input == tic_gamepad_input)
	{
		if (duk_is_null_or_undefined(duk, 0))
		{
			duk_push_uint(duk, machine->memory.ram.vram.input.gamepad.data);
		}
		else
		{
			s32 index = duk_to_int(duk, 0) & 0xf;
			duk_push_boolean(duk, machine->memory.ram.vram.input.gamepad.data & (1 << index));
		}

		return 1;		
	}
	else duk_error(duk, DUK_ERR_ERROR, "gamepad input not declared in metadata\n");

	return 0;
}

static duk_ret_t duk_btnp(duk_context* duk)
{
	tic_machine* machine = getDukMachine(duk);
	tic_mem* memory = (tic_mem*)machine;

	if(machine->memory.input == tic_gamepad_input)
	{
		if (duk_is_null_or_undefined(duk, 0))
		{
			duk_push_uint(duk, memory->api.btnp(memory, -1, -1, -1));
		}
		else if(duk_is_null_or_undefined(duk, 1) && duk_is_null_or_undefined(duk, 2))
		{
			s32 index = duk_to_int(duk, 0) & 0xf;

			duk_push_boolean(duk, memory->api.btnp(memory, index, -1, -1));
		}
		else
		{
			s32 index = duk_to_int(duk, 0) & 0xf;
			u32 hold = duk_to_int(duk, 1);
			u32 period = duk_to_int(duk, 2);

			duk_push_boolean(duk, memory->api.btnp(memory, index, hold, period));
		}

		return 1;
	}
	else duk_error(duk, DUK_ERR_ERROR, "gamepad input not declared in metadata\n");

	return 0;
}

static duk_ret_t duk_sfx(duk_context* duk)
{
	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	s32 index = duk_is_null_or_undefined(duk, 0) ? -1 : duk_to_int(duk, 0);

	s32 note = -1;
	s32 octave = -1;
	s32 speed = SFX_DEF_SPEED;

	if (index < SFX_COUNT)
	{
		if(index >= 0)
		{
			tic_sample* effect = memory->ram.sfx.samples.data + index;

			note = effect->note;
			octave = effect->octave;
			speed = effect->speed;

			if(!duk_is_null_or_undefined(duk, 1))
			{
				if(duk_is_string(duk, 1))
				{
					const char* noteStr = duk_to_string(duk, 1);

					if(!tic_tool_parse_note(noteStr, &note, &octave))
					{
						duk_error(duk, DUK_ERR_ERROR, "invalid note, should be like C#4\n");
						return 0;
					}
				}
				else
				{
					s32 id = duk_to_int(duk, 1);
					note = id % NOTES;
					octave = id / NOTES;
				}
			}
		}
	}
	else
	{
		duk_error(duk, DUK_ERR_ERROR, "unknown sfx index\n");
		return 0;
	}

	s32 duration = duk_is_null_or_undefined(duk, 2) ? -1 : duk_to_int(duk, 2);
	s32 channel = duk_is_null_or_undefined(duk, 3) ? 0 : duk_to_int(duk, 3);
	s32 volume = duk_is_null_or_undefined(duk, 4) ? MAX_VOLUME : duk_to_int(duk, 4);

	if(!duk_is_null_or_undefined(duk, 5))
		speed = duk_to_int(duk, 5);

	if (channel >= 0 && channel < TIC_SOUND_CHANNELS)
	{
		memory->api.sfx_stop(memory, channel);
		memory->api.sfx_ex(memory, index, note, octave, duration, channel, volume & 0xf, speed);
	}
	else duk_error(duk, DUK_ERR_ERROR, "unknown channel\n");

	return 0;
}

typedef struct
{
	duk_context* duk;
	void* remap;
} RemapData;

static void remapCallback(void* data, s32 x, s32 y, RemapResult* result)
{

	RemapData* remap = (RemapData*)data;
	duk_context* duk = remap->duk;

	duk_push_heapptr(duk, remap->remap);
	duk_push_int(duk, result->index);
	duk_push_int(duk, x);
	duk_push_int(duk, y);
	duk_pcall(duk, 3);

	if(duk_is_array(duk, -1))
	{
		duk_get_prop_index(duk, -1, 0);
		result->index = duk_to_int(duk, -1);
		duk_pop(duk);

		duk_get_prop_index(duk, -1, 1);
		result->flip = duk_to_int(duk, -1);
		duk_pop(duk);

		duk_get_prop_index(duk, -1, 2);
		result->rotate = duk_to_int(duk, -1);
		duk_pop(duk);
	}
	else
	{
		result->index = duk_to_int(duk, -1);		
	}

	duk_pop(duk);
}

static duk_ret_t duk_map(duk_context* duk)
{
	s32 x = duk_is_null_or_undefined(duk, 0) ? 0 : duk_to_int(duk, 0);
	s32 y = duk_is_null_or_undefined(duk, 1) ? 0 : duk_to_int(duk, 1);
	s32 w = duk_is_null_or_undefined(duk, 2) ? TIC_MAP_SCREEN_WIDTH : duk_to_int(duk, 2);
	s32 h = duk_is_null_or_undefined(duk, 3) ? TIC_MAP_SCREEN_HEIGHT : duk_to_int(duk, 3);
	s32 sx = duk_is_null_or_undefined(duk, 4) ? 0 : duk_to_int(duk, 4);
	s32 sy = duk_is_null_or_undefined(duk, 5) ? 0 : duk_to_int(duk, 5);
	u8 chromakey = duk_is_null_or_undefined(duk, 6) ? -1 : duk_to_int(duk, 6);
	s32 scale = duk_is_null_or_undefined(duk, 7) ? 1 : duk_to_int(duk, 7);

	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	if (duk_is_null_or_undefined(duk, 8))
		memory->api.map(memory, &memory->ram.map, &memory->ram.tiles, x, y, w, h, sx, sy, chromakey, scale);
	else
	{
		void* remap = duk_get_heapptr(duk, 8);

	 	RemapData data = {duk, remap};

	 	memory->api.remap((tic_mem*)getDukMachine(duk), &memory->ram.map, &memory->ram.tiles, x, y, w, h, sx, sy, chromakey, scale, remapCallback, &data);
	}

	return 0;
}

static duk_ret_t duk_mget(duk_context* duk)
{
	s32 x = duk_is_null_or_undefined(duk, 0) ? 0 : duk_to_int(duk, 0);
	s32 y = duk_is_null_or_undefined(duk, 1) ? 0 : duk_to_int(duk, 1);

	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	u8 value = memory->api.map_get(memory, &memory->ram.map, x, y);
	duk_push_uint(duk, value);
	return 1;
}

static duk_ret_t duk_mset(duk_context* duk)
{
	s32 x = duk_is_null_or_undefined(duk, 0) ? 0 : duk_to_int(duk, 0);
	s32 y = duk_is_null_or_undefined(duk, 1) ? 0 : duk_to_int(duk, 1);
	u8 value = duk_is_null_or_undefined(duk, 2) ? 0 : duk_to_int(duk, 2);

	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	memory->api.map_set(memory, &memory->ram.map, x, y, value);

	return 1;
}

static duk_ret_t duk_peek(duk_context* duk)
{

	s32 address = duk_to_int(duk, 0);

	if(address >= 0 && address < sizeof(tic_ram))
	{
		tic_machine* machine = getDukMachine(duk);
		duk_push_uint(duk, *((u8*)&machine->memory.ram + address));
		return 1;
	}

	return 0;
}

static duk_ret_t duk_poke(duk_context* duk)
{

	s32 address = duk_to_int(duk, 0);
	u8 value = duk_to_int(duk, 1) & 0xff;

	if(address >= 0 && address < sizeof(tic_ram))
	{
		tic_machine* machine = getDukMachine(duk);
		*((u8*)&machine->memory.ram + address) = value;
	}

	return 0;
}

static duk_ret_t duk_peek4(duk_context* duk)
{

	s32 address = duk_to_int(duk, 0);

	if(address >= 0 && address < sizeof(tic_ram)*2)
	{
		tic_mem* memory = (tic_mem*)getDukMachine(duk);

		duk_push_uint(duk, tic_tool_peek4((u8*)&memory->ram, address));
		return 1;
	}

	return 0;
}

static duk_ret_t duk_poke4(duk_context* duk)
{

	s32 address = duk_to_int(duk, 0);
	u8 value = duk_to_int(duk, 1) & 0xff;

	if(address >= 0 && address < sizeof(tic_ram)*2)
	{
		tic_mem* memory = (tic_mem*)getDukMachine(duk);

		tic_tool_poke4((u8*)&memory->ram, address, value);
	}

	return 0;
}

static duk_ret_t duk_memcpy(duk_context* duk)
{
	s32 dest = duk_to_int(duk, 0);
	s32 src = duk_to_int(duk, 1);
	s32 size = duk_to_int(duk, 2);
	s32 bound = sizeof(tic_ram) - size;

	if(size >= 0 && size <= sizeof(tic_ram) && dest >= 0 && src >= 0 && dest <= bound && src <= bound)
	{
		u8* base = (u8*)&getDukMachine(duk)->memory;
		memcpy(base + dest, base + src, size);
	}

	return 0;
}

static duk_ret_t duk_memset(duk_context* duk)
{
	s32 dest = duk_to_int(duk, 0);
	u8 value = duk_to_int(duk, 1);
	s32 size = duk_to_int(duk, 2);
	s32 bound = sizeof(tic_ram) - size;

	if(size >= 0 && size <= sizeof(tic_ram) && dest >= 0 && dest <= bound)
	{
		u8* base = (u8*)&getDukMachine(duk)->memory;
		memset(base + dest, value, size);
	}

	return 0;
}

static duk_ret_t duk_trace(duk_context* duk)
{
	tic_machine* machine = getDukMachine(duk);

	const char* text = duk_is_null_or_undefined(duk, 0) ? "" : duk_to_string(duk, 0);
	u8 color = duk_is_null_or_undefined(duk, 1) ? tic_color_white : duk_to_int(duk, 1);

	machine->data->trace(machine->data->data, text ? text : "nil", color);

	return 0;
}

static duk_ret_t duk_pmem(duk_context* duk)
{
	tic_machine* machine = getDukMachine(duk);
	tic_mem* memory = &machine->memory;

	u32 index = duk_to_int(duk, 0);

	if(index < TIC_PERSISTENT_SIZE)
	{
		s32 val = memory->ram.persistent.data[index];

		if(!duk_is_null_or_undefined(duk, 1))
			memory->ram.persistent.data[index] = duk_to_int(duk, 1);
		
		duk_push_int(duk, val);

		return 1;
	}
	else duk_error(duk, DUK_ERR_ERROR, "invalid persistent memory index\n");

	return 0;
}

static duk_ret_t duk_time(duk_context* duk)
{
	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	duk_push_number(duk, memory->api.time(memory));

	return 1;
}

static duk_ret_t duk_exit(duk_context* duk)
{
	tic_machine* machine = getDukMachine(duk);

	machine->data->exit(machine->data->data);

	return 0;
}

static duk_ret_t duk_font(duk_context* duk)
{
	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	const char* text = duk_to_string(duk, 0);
	s32 x = duk_to_int(duk, 1);
	s32 y = duk_to_int(duk, 2);
	u8 chromakey = duk_to_int(duk, 3);
	s32 width =  duk_is_null_or_undefined(duk, 4) ? TIC_SPRITESIZE : duk_to_int(duk, 4);
	s32 height =  duk_is_null_or_undefined(duk, 5) ? TIC_SPRITESIZE : duk_to_int(duk, 5);
	bool fixed = duk_is_null_or_undefined(duk, 6) ? false : duk_to_boolean(duk, 6);
	s32 scale = duk_is_null_or_undefined(duk, 7) ? 1 : duk_to_int(duk, 7);

	if(scale == 0)
	{
		duk_push_int(duk, 0);
		return 1;
	}

	s32 size = drawText(memory, text, x, y, width, height, chromakey, scale, fixed ? drawSpriteFont : drawFixedSpriteFont);

	duk_push_int(duk, size);

	return 1;
}

static duk_ret_t duk_mouse(duk_context* duk)
{
	tic_machine* machine = getDukMachine(duk);

	if(machine->memory.input == tic_mouse_input)
	{
		u16 data = machine->memory.ram.vram.input.gamepad.data;

		duk_idx_t idx = duk_push_array(duk);
		duk_push_int(duk, (data & 0x7fff) % TIC80_WIDTH);
		duk_put_prop_index(duk, idx, 0);
		duk_push_int(duk, (data & 0x7fff) / TIC80_WIDTH);
		duk_put_prop_index(duk, idx, 1);
		duk_push_int(duk, data >> 15);
		duk_put_prop_index(duk, idx, 2);

		return 1;
	}
	else duk_error(duk, DUK_ERR_ERROR, "mouse input not declared in metadata\n");

	return 0;
}

static duk_ret_t duk_circ(duk_context* duk)
{
	s32 radius = duk_to_int(duk, 2);
	if(radius < 0) return 0;
	
	s32 x = duk_to_int(duk, 0);
	s32 y = duk_to_int(duk, 1);
	s32 color = duk_to_int(duk, 3);

	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	memory->api.circle(memory, x, y, radius, color);

	return 0;
}

static duk_ret_t duk_circb(duk_context* duk)
{
	s32 radius = duk_to_int(duk, 2);
	if(radius < 0) return 0;
	
	s32 x = duk_to_int(duk, 0);
	s32 y = duk_to_int(duk, 1);
	s32 color = duk_to_int(duk, 3);

	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	memory->api.circle_border(memory, x, y, radius, color);

	return 0;
}

static duk_ret_t duk_tri(duk_context* duk)
{
	s32 pt[6];

	for(s32 i = 0; i < COUNT_OF(pt); i++)
		pt[i] = duk_to_int(duk, i);
	
	s32 color = duk_to_int(duk, 6);

	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	memory->api.tri(memory, pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], color);

	return 0;
}

static duk_ret_t duk_textri(duk_context* duk)
{
	float pt[12];

	for (s32 i = 0; i < COUNT_OF(pt); i++)
		pt[i] = (float)duk_to_number(duk, i);
	tic_mem* memory = (tic_mem*)getDukMachine(duk);
	bool use_map = duk_is_null_or_undefined(duk, 12) ? false : duk_to_boolean(duk, 12);
	u8 chroma = duk_is_null_or_undefined(duk, 13) ? 0xff : duk_to_int(duk, 13);

	memory->api.textri(memory, pt[0], pt[1],	//	xy 1
						pt[2], pt[3],	//	xy 2
						pt[4], pt[5],	//  xy 3
						pt[6], pt[7],	//	uv 1
						pt[8], pt[9],	//	uv 2
						pt[10], pt[11],//  uv 3
						use_map, // usemap
						chroma);	//	chroma
	
	return 0;
}


static duk_ret_t duk_clip(duk_context* duk)
{
	s32 x = duk_to_int(duk, 0);
	s32 y = duk_to_int(duk, 1);
	s32 w = duk_is_null_or_undefined(duk, 2) ? TIC80_WIDTH : duk_to_int(duk, 2);
	s32 h = duk_is_null_or_undefined(duk, 3) ? TIC80_HEIGHT : duk_to_int(duk, 3);

	tic_mem* memory = (tic_mem*)getDukMachine(duk);
	
	memory->api.clip(memory, x, y, w, h);

	return 0;
}

static duk_ret_t duk_music(duk_context* duk)
{
	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	s32 track = duk_is_null_or_undefined(duk, 0) ? -1 : duk_to_int(duk, 0);
	memory->api.music(memory, -1, 0, 0, false);

	if(track >= 0)
	{
		s32 frame = duk_is_null_or_undefined(duk, 1) ? -1 : duk_to_int(duk, 1);
		s32 row = duk_is_null_or_undefined(duk, 2) ? -1 : duk_to_int(duk, 2);
		bool loop = duk_is_null_or_undefined(duk, 3) ? true : duk_to_boolean(duk, 3);

		memory->api.music(memory, track, frame, row, loop);
	}

	return 0;
}

static duk_ret_t duk_sync(duk_context* duk)
{
	tic_mem* memory = (tic_mem*)getDukMachine(duk);

	const char* section = duk_is_null_or_undefined(duk, 0) ? NULL : duk_to_string(duk, 0);
	s32 bank = duk_is_null_or_undefined(duk, 1) ? 0 : duk_to_int(duk, 1);
	bool toCart = duk_is_null_or_undefined(duk, 2) ? false : duk_to_boolean(duk, 2);

	if(bank >= 0 && bank < TIC_BANKS)
		memory->api.sync(memory, section, bank, toCart);
	else
		duk_error(duk, DUK_ERR_ERROR, "sync() error, invalid bank");

	return 0;
}

static const char* const ApiKeywords[] = API_KEYWORDS;
static const struct{duk_c_function func; s32 params;} ApiFunc[] = 
{
	{NULL, 0},
	{NULL, 1},
	{NULL, 0},
	{duk_print, 6},
	{duk_cls, 1},
	{duk_pix, 3},
	{duk_line, 5},
	{duk_rect, 5},
	{duk_rectb, 5},
	{duk_spr, 9},
	{duk_btn, 1},
	{duk_btnp, 3},
	{duk_sfx, 6},
	{duk_map, 9},
	{duk_mget, 2},
	{duk_mset, 3},
	{duk_peek, 1},
	{duk_poke, 2},
	{duk_peek4, 1},
	{duk_poke4, 2},
	{duk_memcpy, 3},
	{duk_memset, 3},
	{duk_trace, 2},
	{duk_pmem, 2},
	{duk_time, 0},
	{duk_exit, 0},
	{duk_font, 8},
	{duk_mouse, 0},
	{duk_circ, 4},
	{duk_circb, 4},
	{duk_tri, 7},
	{duk_textri,14},
	{duk_clip, 4},
	{duk_music, 4},
	{duk_sync, 3},
};

static void initDuktape(tic_machine* machine)
{
	closeJavascript(machine);

	duk_context* duk = machine->js = duk_create_heap_default();

	{
		duk_push_global_stash(duk);
		duk_push_pointer(duk, machine);
		duk_put_prop_string(duk, -2, TicMachine);
		duk_pop(duk);
	}

	for (s32 i = 0; i < COUNT_OF(ApiFunc); i++)
		if (ApiFunc[i].func)
		{
			duk_push_c_function(machine->js, ApiFunc[i].func, ApiFunc[i].params);
			duk_put_global_string(machine->js, ApiKeywords[i]);
		}
}

bool initJavascript(tic_machine* machine, const char* code)
{
	initDuktape(machine);
	duk_context* duktape = machine->js;

	if (duk_pcompile_string(duktape, 0, code) != 0 || duk_peval_string(duktape, code) != 0)
	{					
		machine->data->error(machine->data->data, duk_safe_to_string(duktape, -1));
		duk_pop(duktape);
		return false;
	}

	return true;
}

void callJavascriptTick(tic_machine* machine)
{
	const char* TicFunc = ApiKeywords[0];

	duk_context* duk = machine->js;

	if(duk)
	{
		if(duk_get_global_string(duk, TicFunc))
		{
			if(duk_pcall(duk, 0) != 0)
			{
				machine->data->error(machine->data->data, duk_safe_to_string(duk, -1));
				duk_pop(duk);
			}
		}
		else
		{
			machine->data->error(machine->data->data, "'function TIC()...' isn't found :(");
		}		
	}
}

void callJavascriptScanline(tic_mem* memory, s32 row, void* data)
{
	tic_machine* machine = (tic_machine*)memory;
	duk_context* duk = machine->js;

	const char* ScanlineFunc = ApiKeywords[1];

	if(duk_get_global_string(duk, ScanlineFunc)) 
	{
		duk_push_int(duk, row);

		if(duk_pcall(duk, 1) != 0)
		{
			machine->data->error(machine->data->data, duk_safe_to_string(duk, -1));
			duk_pop(duk);
		}
		else duk_pop(duk);
	}
	else duk_pop(duk);
}

void callJavascriptOverlap(tic_mem* memory, void* data)
{
	tic_machine* machine = (tic_machine*)memory;
	duk_context* duk = machine->js;

	const char* OvrFunc = ApiKeywords[2];

	if(duk_get_global_string(duk, OvrFunc)) 
	{
		if(duk_pcall(duk, 0) != 0)
		{
			machine->data->error(machine->data->data, duk_safe_to_string(duk, -1));
			duk_pop(duk);
		}
		else duk_pop(duk);
	}
	else duk_pop(duk);
}