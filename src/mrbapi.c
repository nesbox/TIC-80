// MIT License

// Copyright (c) Jeremiasz Nelz <remi6397@gmail.com>

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

#if defined(TIC_BUILD_WITH_MRUBY)

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mruby.h>
#include "mruby/compile.h"
#include "mruby/error.h"
#include "mruby/throw.h"
#include "mruby/array.h"
#include "mruby/hash.h"
#include "mruby/variable.h"
#include "mruby/value.h"
#include "mruby/string.h"

static tic_machine* CurrentMachine = NULL;
static inline tic_machine* getMRubyMachine(mrb_state* mrb)
{
	return CurrentMachine;
}

static mrb_value mrb_peek(mrb_state* mrb, mrb_value self)
{
	tic_machine* machine = getMRubyMachine(mrb);

	mrb_int address;
	mrb_get_args(mrb, "i", &address);

	if(address >=0 && address < sizeof(tic_ram))
	{
		return mrb_fixnum_value(*((u8*)&machine->memory.ram + address));
	}
	else
	{
		mrb_raise(mrb, E_ARGUMENT_ERROR, "address not in range");
	}

	return mrb_nil_value();
}

static mrb_value mrb_poke(mrb_state* mrb, mrb_value self)
{
	tic_machine* machine = getMRubyMachine(mrb);

	mrb_int address, value;
	mrb_get_args(mrb, "ii", &address, &value);

	if(address >=0 && address < sizeof(tic_ram))
	{
		*((u8*)&machine->memory.ram + address) = value;
	}
	else
	{
		mrb_raise(mrb, E_ARGUMENT_ERROR, "address not in range");
	}

	return mrb_nil_value();
}

static mrb_value mrb_peek4(mrb_state* mrb, mrb_value self)
{
	mrb_int address;
	mrb_get_args(mrb, "i", &address);

	if(address >= 0 && address < sizeof(tic_ram)*2)
	{
		return mrb_fixnum_value(tic_tool_peek4((u8*)&getMRubyMachine(mrb)->memory.ram, address));
	}
	else
	{
		mrb_raise(mrb, E_ARGUMENT_ERROR, "address not in range");
	}

	return mrb_nil_value();
}

static mrb_value mrb_poke4(mrb_state* mrb, mrb_value self)
{
	mrb_int address, value;
	mrb_get_args(mrb, "ii", &address, &value);

	if(address >= 0 && address < sizeof(tic_ram)*2)
	{
		tic_tool_poke4((u8*)&getMRubyMachine(mrb)->memory.ram, address, value);
	}
	else
	{
		mrb_raise(mrb, E_ARGUMENT_ERROR, "address not in range");
	}

	return mrb_nil_value();
}

static mrb_value mrb_cls(mrb_state* mrb, mrb_value self)
{
	mrb_int color = 0;
	mrb_get_args(mrb, "|i", &color);

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	memory->api.clear(memory, color);

	return mrb_nil_value();
}

static mrb_value mrb_pix(mrb_state* mrb, mrb_value self)
{
	mrb_int x, y, color;
	mrb_int argc = mrb_get_args(mrb, "ii|i", &x, &y, &color);

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	if(argc == 3)
	{
		memory->api.pixel(memory, x, y, color);
		return mrb_nil_value();
	}
	else
	{
		return mrb_fixnum_value(memory->api.get_pixel(memory, x, y));
	}
}

static mrb_value mrb_line(mrb_state* mrb, mrb_value self)
{
	mrb_int x0, y0, x1, y1, color;
	mrb_get_args(mrb, "iiiii", &x0, &y0, &x1, &y1, &color);

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	memory->api.line(memory, x0, y0, x1, y1, color);

	return mrb_nil_value();
}

static mrb_value mrb_rect(mrb_state* mrb, mrb_value self)
{
	mrb_int x, y, w, h, color;
	mrb_get_args(mrb, "iiiii", &x, &y, &w, &h, &color);

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	memory->api.rect(memory, x, y, w, h, color);

	return mrb_nil_value();
}

static mrb_value mrb_rectb(mrb_state* mrb, mrb_value self)
{
	mrb_int x, y, w, h, color;
	mrb_get_args(mrb, "iiiii", &x, &y, &w, &h, &color);

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	memory->api.rect_border(memory, x, y, w, h, color);

	return mrb_nil_value();
}

static mrb_value mrb_circ(mrb_state* mrb, mrb_value self)
{
	mrb_int x, y, radius, color;
	mrb_get_args(mrb, "iiii", &x, &y, &radius, &color);

	if(radius >= 0) {
		tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

		memory->api.circle(memory, x, y, radius, color);
	} else {
		mrb_raise(mrb, E_ARGUMENT_ERROR, "radius must be greater than or equal 0");
	}

	return mrb_nil_value();
}

static mrb_value mrb_circb(mrb_state* mrb, mrb_value self)
{
	mrb_int x, y, radius, color;
	mrb_get_args(mrb, "iiii", &x, &y, &radius, &color);

	if(radius >= 0) {
		tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

		memory->api.circle_border(memory, x, y, radius, color);
	} else {
		mrb_raise(mrb, E_ARGUMENT_ERROR, "radius must be greater than or equal 0");
	}

	return mrb_nil_value();
}

static mrb_value mrb_tri(mrb_state* mrb, mrb_value self)
{
	mrb_int x1, y1, x2, y2, x3, y3, color;
	mrb_get_args(mrb, "iiiiiii", &x1, &y1, &x2, &y2, &x3, &y3, &color);

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	memory->api.tri(memory, x1, y1, x2, y2, x3, y3, color);

	return mrb_nil_value();
}

static mrb_value mrb_textri(mrb_state* mrb, mrb_value self)
{
	mrb_bool use_map = false;
	mrb_int chroma = 0xff;
	mrb_int x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3;
	mrb_int argc = mrb_get_args(mrb, "iiiiiiiiiiii|bi",
			&x1, &y1, &x2, &y2, &x3, &y3,
			&u1, &v1, &u2, &v2, &u3, &v3,
			&use_map, &chroma);

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	memory->api.textri(memory,
						x1, y1, x2, y2, x3, y3,
						u1, v1, u2, v2, u3, v3,
						use_map, chroma);

	return mrb_nil_value();
}


static mrb_value mrb_clip(mrb_state* mrb, mrb_value self)
{
	mrb_int x, y, w, h;
	mrb_int argc = mrb_get_args(mrb, "|iiii", &x, &y, &w, &h);

	if(argc == 0)
	{
		tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

		memory->api.clip(memory, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);
	}
	else if(argc == 4)
	{
		tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

		memory->api.clip((tic_mem*)getMRubyMachine(mrb), x, y, w, h);
	}
	else mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid parameters, use clip(x,y,w,h) or clip()");

	return mrb_nil_value();
}

static mrb_value mrb_btnp(mrb_state* mrb, mrb_value self)
{
	tic_machine* machine = getMRubyMachine(mrb);
	tic_mem* memory = (tic_mem*)machine;

	mrb_int index, hold, period;
	mrb_int argc = mrb_get_args(mrb, "|iii", &index, &hold, &period);

	index &= 0x1f;

	if (argc == 0)
	{
		return mrb_fixnum_value(memory->api.btnp(memory, -1, -1, -1));
	}
	else if(argc == 1)
	{
		return mrb_bool_value(memory->api.btnp(memory, index, -1, -1));
	}
	else if (argc == 3)
	{
		return mrb_bool_value(memory->api.btnp(memory, index, hold, period));
	}
	else
	{
		mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid params, btnp [ id [ hold period ] ]");
		return mrb_nil_value();
	}
}

static mrb_value mrb_btn(mrb_state* mrb, mrb_value self)
{
	tic_machine* machine = getMRubyMachine(mrb);

	mrb_int index, hold, period;
	mrb_int argc = mrb_get_args(mrb, "|i", &index, &hold, &period);

	index &= 0x1f;

	if (argc == 0)
	{
		return mrb_fixnum_value(machine->memory.ram.input.gamepads.data);
	}
	else if (argc == 1)
	{
		return mrb_bool_value(machine->memory.ram.input.gamepads.data & (1 << index));
	}
	else
	{
		mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid params, btn [ id ]\n");
		return mrb_nil_value();
	}
}

static mrb_value mrb_spr(mrb_state* mrb, mrb_value self)
{
	mrb_int index, x, y;
	mrb_int w = 1, h = 1, scale = 1;
	mrb_int flip = tic_no_flip, rotate = tic_no_rotate;
	mrb_value colors_obj;
	static u8 colors[TIC_PALETTE_SIZE];
	mrb_int count = 0;

	mrb_int argc = mrb_get_args(mrb, "iii|oiiiii", &index, &x, &y, &colors_obj, &scale, &flip, &rotate, &w, &h);

	if(mrb_array_p(colors_obj))
	{
		for(; count < TIC_PALETTE_SIZE && count < ARY_LEN(RARRAY(colors_obj)); count++) // HACK
			colors[count] = (u8) mrb_int(mrb, mrb_ary_entry(colors_obj, count));
		count++;
	}
	else if(mrb_fixnum_p(colors_obj))
	{
		colors[0] = mrb_int(mrb, colors_obj);
		count = 1;
	}
	else
	{
		mrb_raise(mrb, E_ARGUMENT_ERROR, "color must be either an array or a palette index");
		return mrb_nil_value();
	}

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	memory->api.sprite_ex(memory, &memory->ram.tiles, index, x, y, w, h, colors, count, scale, flip, rotate);

	return mrb_nil_value();
}

static mrb_value mrb_mget(mrb_state* mrb, mrb_value self)
{
	mrb_int x, y;
	mrb_get_args(mrb, "ii", &x, &y);

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	u8 value = memory->api.map_get(memory, &memory->ram.map, x, y);
	return mrb_fixnum_value(value);
}

static mrb_value mrb_mset(mrb_state* mrb, mrb_value self)
{
	mrb_int x, y, val;
	mrb_get_args(mrb, "iii", &x, &y, &val);

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	memory->api.map_set(memory, &memory->ram.map, x, y, val);

	return mrb_nil_value();
}

typedef struct
{
	mrb_state* mrb;
	mrb_value block;
} RemapData;

static void remapCallback(void* data, s32 x, s32 y, RemapResult* result)
{
	RemapData* remap = (RemapData*)data;
	mrb_state* mrb = remap->mrb;

	mrb_value vals[] = { result->index, x, y };
	mrb_value ary = mrb_yield_argv(mrb, remap->block, 3, vals);

	result->index = mrb_int(mrb, mrb_ary_entry(ary, 0));
	result->flip = mrb_int(mrb, mrb_ary_entry(ary, 1));
	result->rotate = mrb_int(mrb, mrb_ary_entry(ary, 2));
}

static mrb_value mrb_map(mrb_state* mrb, mrb_value self)
{
	mrb_int x = 0;
	mrb_int y = 0;
	mrb_int w = TIC_MAP_SCREEN_WIDTH;
	mrb_int h = TIC_MAP_SCREEN_HEIGHT;
	mrb_int sx = 0;
	mrb_int sy = 0;
	mrb_int chromakey = -1;
	mrb_int scale = 1;
	mrb_value block;

	mrb_int argc = mrb_get_args(mrb, "|iiiiiiii&", &x, &y, &w, &h, &sx, &sy, &chromakey, &scale, &block);

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	if (mrb_proc_p(block))
	{
		RemapData data = { mrb, block };
		memory->api.remap(memory, &memory->ram.map, &memory->ram.tiles, x, y, w, h, sx, sy, chromakey, scale, remapCallback, &data);
	}
	else
	{
		memory->api.map(memory, &memory->ram.map, &memory->ram.tiles, x, y, w, h, sx, sy, chromakey, scale);
	}
}

static mrb_value mrb_music(mrb_state* mrb, mrb_value self)
{
	s32 track;
	s32 frame = -1;
	s32 row = -1;
	bool loop = true;

	mrb_int argc = mrb_get_args(mrb, "|iiib", &track, &frame, &row, &loop);

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	if(argc == 0) memory->api.music(memory, -1, 0, 0, false);
	else
	{
		memory->api.music(memory, -1, 0, 0, false);

		memory->api.music(memory, track, frame, row, loop);
	}

	return mrb_nil_value();
}

static mrb_value mrb_sfx(mrb_state* mrb, mrb_value self)
{
	mrb_int index;

	mrb_value note_obj;
	s32 note = -1;
	mrb_int octave = -1;
	mrb_int duration = -1;
	mrb_int channel = 0;
	mrb_int volume = MAX_VOLUME;
	mrb_int speed = SFX_DEF_SPEED;

	mrb_int argc = mrb_get_args(mrb, "i|oiiii", &index, &note_obj, &duration, &channel, &volume, &speed);

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	if(index < SFX_COUNT)
	{
		if (index >= 0)
		{
			tic_sample* effect = memory->ram.sfx.samples.data + index;

			note = effect->note;
			octave = effect->octave;
			speed = effect->speed;
		}

		if (argc >= 2)
		{
			if(mrb_fixnum_p(note_obj))
			{
				mrb_int id = mrb_int(mrb, note_obj);
				note = id % NOTES;
				octave = id / NOTES;
			}
			else if(mrb_string_p(note_obj))
			{
				const char* noteStr = mrb_str_to_cstr(mrb, mrb_funcall(mrb, note_obj, "to_s", 0));

				s32 octave32;
				if(!tic_tool_parse_note(noteStr, &note, &octave32))
				{
					mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid note, should be like C#4");
					return mrb_nil_value();
				}
				octave = octave32;
			}
			else
			{
				mrb_raise(mrb, E_ARGUMENT_ERROR, "note must be either an integer number or a string like \"C#4\"");
				return mrb_nil_value();
			}
		}

		if (channel >= 0 && channel < TIC_SOUND_CHANNELS)
		{
			memory->api.sfx_stop(memory, channel);
			memory->api.sfx_ex(memory, index, note, octave, duration, channel, volume & 0xf, speed);
		}
		else mrb_raise(mrb, E_ARGUMENT_ERROR, "unknown channel");
	}
	else mrb_raise(mrb, E_ARGUMENT_ERROR, "unknown sfx index");

	return mrb_nil_value();
}

static mrb_value mrb_sync(mrb_state* mrb, mrb_value self)
{
	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	mrb_int mask = 0;
	mrb_int bank = 0;
	mrb_bool toCart = false;

	mrb_int argc = mrb_get_args(mrb, "|iib", &mask, &bank, &toCart);

	if(bank >= 0 && bank < TIC_BANKS)
		memory->api.sync(memory, mask, bank, toCart);
	else
		mrb_raise(mrb, E_ARGUMENT_ERROR, "sync() error, invalid bank");

	return mrb_nil_value();
}

static mrb_value mrb_reset(mrb_state* mrb, mrb_value self)
{
	tic_machine* machine = getMRubyMachine(mrb);

	machine->state.initialized = false;

	return mrb_nil_value();
}

static mrb_value mrb_key(mrb_state* mrb, mrb_value self)
{
	tic_machine* machine = getMRubyMachine(mrb);
	tic_mem* tic = &machine->memory;

	mrb_int key;
	mrb_int argc = mrb_get_args(mrb, "|i", &key);

	if (argc == 0)
		return mrb_bool_value(tic->api.key(tic, tic_key_unknown));
	else
	{
		if(key < tic_key_escape)
			return mrb_bool_value(tic->api.key(tic, key));
		else
		{
			mrb_raise(mrb, E_ARGUMENT_ERROR, "unknown keyboard code");
			return mrb_nil_value();
		}
	}
}

static mrb_value mrb_keyp(mrb_state* mrb, mrb_value self)
{
	tic_machine* machine = getMRubyMachine(mrb);
	tic_mem* memory = (tic_mem*)machine;

	mrb_int key, hold, period;
	mrb_int argc = mrb_get_args(mrb, "|iii", &key, &hold, &period);

	if (argc == 0)
	{
		return mrb_fixnum_value(memory->api.keyp(memory, -1, -1, -1));
	}
	else if (key >= tic_key_escape)
	{
		mrb_raise(mrb, E_ARGUMENT_ERROR, "unknown keyboard code");
	}
	else if(argc == 1)
	{
		return mrb_bool_value(memory->api.keyp(memory, key, -1, -1));
	}
	else if (argc == 3)
	{
		return mrb_bool_value(memory->api.keyp(memory, key, hold, period));
	}
	else
	{
		mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid params, btnp [ id [ hold period ] ]");
		return mrb_nil_value();
	}
}

static mrb_value mrb_memcpy(mrb_state* mrb, mrb_value self)
{
	mrb_int dest, src, size;
	mrb_int argc = mrb_get_args(mrb, "iii", &dest, &src, &size);

	s32 bound = sizeof(tic_ram) - size;

	if(size >= 0 && size <= sizeof(tic_ram) && dest >= 0 && src >= 0 && dest <= bound && src <= bound)
	{
		u8* base = (u8*)&getMRubyMachine(mrb)->memory;
		memcpy(base + dest, base + src, size);
	}
	else
	{
		mrb_raise(mrb, E_ARGUMENT_ERROR, "memory address not in range!");
	}

	return mrb_nil_value();
}

static mrb_value mrb_memset(mrb_state* mrb, mrb_value self)
{
	mrb_int dest, value, size;
	mrb_int argc = mrb_get_args(mrb, "iii", &dest, &value, &size);

	s32 bound = sizeof(tic_ram) - size;

	if(size >= 0 && size <= sizeof(tic_ram) && dest >= 0 && dest <= bound)
	{
		u8* base = (u8*)&getMRubyMachine(mrb)->memory;
		memset(base + dest, value, size);
	}
	else
	{
		mrb_raise(mrb, E_ARGUMENT_ERROR, "memory address not in range!");
	}

	return mrb_nil_value();
}

static char* mrb_value_to_cstr(mrb_state* mrb, mrb_value value)
{
	mrb_value str = mrb_funcall(mrb, value, "to_s", 0);
	return mrb_str_to_cstr(mrb, str);
}

static mrb_value mrb_font(mrb_state* mrb, mrb_value self)
{
	mrb_value text_obj;
	mrb_int x = 0;
	mrb_int y = 0;
	mrb_int width = TIC_SPRITESIZE;
	mrb_int height = TIC_SPRITESIZE;
	mrb_int chromakey = 0;
	mrb_bool fixed = false;
	mrb_int scale = 1;
	mrb_bool alt = false;
	mrb_get_args(mrb, "S|iiiiibib",
			&text_obj, &x, &y, &chromakey,
			&width, &height, &fixed, &scale, &alt);

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	if(scale == 0)
		return mrb_fixnum_value(0);

	const char* text = mrb_value_to_cstr(mrb, text_obj);

	s32 size = drawText(memory, text, x, y, width, height, chromakey, scale, fixed ? drawSpriteFont : drawFixedSpriteFont, alt);
	return mrb_fixnum_value(size);
}

static mrb_value mrb_print(mrb_state* mrb, mrb_value self)
{
	mrb_value text_obj;
	mrb_int x = 0;
	mrb_int y = 0;
	mrb_int color = TIC_PALETTE_SIZE-1;
	mrb_bool fixed = false;
	mrb_int scale = 1;
	mrb_bool alt = false;
	mrb_get_args(mrb, "S|iiib",
			&text_obj, &x, &y, &color,
			&fixed, &scale, &alt);

	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);

	if(scale == 0)
		return mrb_fixnum_value(0);

	const char* text = mrb_str_to_cstr(mrb, text_obj);

	s32 size = memory->api.text_ex(memory, text, x, y, color, fixed, scale, alt);
	return mrb_fixnum_value(size);
}

static mrb_value mrb_trace(mrb_state *mrb, mrb_value self)
{
	mrb_value text_obj;
	mrb_int color = tic_color_12;
	mrb_get_args(mrb, "S|i", &text_obj, &color);

	tic_machine* machine = getMRubyMachine(mrb);

	const char* text = mrb_value_to_cstr(mrb, text_obj);
	machine->data->trace(machine->data->data, text, color);
}

static mrb_value mrb_pmem(mrb_state *mrb, mrb_value self)
{
	mrb_int index, value;
	mrb_int argc = mrb_get_args(mrb, "i|i", &index, &value);

	tic_machine* machine = getMRubyMachine(mrb);
	tic_mem* memory = &machine->memory;

	if(index < TIC_PERSISTENT_SIZE)
	{
		u32 val = memory->ram.persistent.data[index];

		if(argc == 2)
		{
			memory->ram.persistent.data[index] = value;
			machine->data->syncPMEM = true;
		}

		return mrb_fixnum_value(val);
	}
	else
	{
		mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid persistent memory index");

		return mrb_nil_value();
	}
}

static mrb_value mrb_time(mrb_state *mrb, mrb_value self)
{
	tic_mem* memory = (tic_mem*)getMRubyMachine(mrb);
	return mrb_float_value(mrb, memory->api.time(memory));
}

static mrb_value mrb_exit(mrb_state *mrb, mrb_value self)
{
	tic_machine* machine = getMRubyMachine(mrb);
	machine->data->exit(machine->data->data);

	return mrb_nil_value();
}

static mrb_value mrb_mouse(mrb_state *mrb, mrb_value self)
{
	mrb_value sym_x = mrb_symbol_value(mrb_intern_cstr(mrb, "x"));
	mrb_value sym_y = mrb_symbol_value(mrb_intern_cstr(mrb, "y"));
	mrb_value sym_left = mrb_symbol_value(mrb_intern_cstr(mrb, "left"));
	mrb_value sym_middle = mrb_symbol_value(mrb_intern_cstr(mrb, "middle"));
	mrb_value sym_right = mrb_symbol_value(mrb_intern_cstr(mrb, "right"));

	tic_machine* machine = getMRubyMachine(mrb);

	const tic80_mouse* mouse = &machine->memory.ram.input.mouse;

	mrb_value hash = mrb_hash_new(mrb);

	mrb_hash_set(mrb, hash, sym_x, mrb_fixnum_value(mouse->x));
	mrb_hash_set(mrb, hash, sym_y, mrb_fixnum_value(mouse->y));
	mrb_hash_set(mrb, hash, sym_left, mrb_bool_value(mouse->left));
	mrb_hash_set(mrb, hash, sym_middle, mrb_bool_value(mouse->middle));
	mrb_hash_set(mrb, hash, sym_right, mrb_bool_value(mouse->right));

	return hash;
}

static const char* const ApiKeywords[] = API_KEYWORDS;
static const mrb_func_t ApiFunc[] =
{
	NULL, NULL, NULL, mrb_print, mrb_cls, mrb_pix, mrb_line, mrb_rect,
	mrb_rectb, mrb_spr, mrb_btn, mrb_btnp, mrb_sfx, mrb_map, mrb_mget,
	mrb_mset, mrb_peek, mrb_poke, mrb_peek4, mrb_poke4, mrb_memcpy,
	mrb_memset, mrb_trace, mrb_pmem, mrb_time, mrb_exit, mrb_font, mrb_mouse,
	mrb_circ, mrb_circb, mrb_tri, mrb_textri, mrb_clip, mrb_music, mrb_sync, mrb_reset,
	mrb_key, mrb_keyp
};
static const mrb_aspec ApiParam[] =
{
	-1, -1, -1,
	MRB_ARGS_REQ(1)|MRB_ARGS_OPT(4), //print
	MRB_ARGS_OPT(1),
	MRB_ARGS_REQ(2)|MRB_ARGS_OPT(1),
	MRB_ARGS_REQ(5),
	MRB_ARGS_REQ(5),
	MRB_ARGS_REQ(5), //rectb
	MRB_ARGS_REQ(3)|MRB_ARGS_OPT(6),
	MRB_ARGS_OPT(1),
	MRB_ARGS_OPT(3),
	MRB_ARGS_REQ(1)|MRB_ARGS_OPT(5),
	MRB_ARGS_OPT(8)|MRB_ARGS_BLOCK(),
	MRB_ARGS_REQ(2),
	MRB_ARGS_REQ(3), // mset
	MRB_ARGS_REQ(1),
	MRB_ARGS_REQ(2),
	MRB_ARGS_REQ(1),
	MRB_ARGS_REQ(2),
	MRB_ARGS_REQ(3),
	MRB_ARGS_REQ(3), // memset
	MRB_ARGS_NONE(),
	MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1),
	MRB_ARGS_NONE(),
	MRB_ARGS_NONE(),
	MRB_ARGS_REQ(1)|MRB_ARGS_OPT(8),
	MRB_ARGS_NONE(),
	MRB_ARGS_REQ(4), // circ
	MRB_ARGS_REQ(3),
	MRB_ARGS_REQ(7),
	MRB_ARGS_REQ(12)|MRB_ARGS_OPT(2),
	MRB_ARGS_OPT(4),
	MRB_ARGS_OPT(4),
	MRB_ARGS_OPT(3),
	MRB_ARGS_NONE(),
	MRB_ARGS_OPT(1), // key
	MRB_ARGS_OPT(3)
};

STATIC_ASSERT(api_func, COUNT_OF(ApiKeywords) == COUNT_OF(ApiFunc));
STATIC_ASSERT(api_func, COUNT_OF(ApiKeywords) == COUNT_OF(ApiParam));

static void checkForceExit(mrb_state *mrb)
{
	tic_machine* machine = getMRubyMachine(mrb);

	tic_tick_data* tick = machine->data;

	if(tick->forceExit && tick->forceExit(tick->data))
		mrb_raise(mrb, E_RUNTIME_ERROR, "script execution was interrupted");
}

static void closeMRuby(tic_mem* tic)
{
	tic_machine* machine = (tic_machine*)tic;

	if(machine->mrb)
	{
		mrbc_context_free(machine->mrb, machine->mrb_cxt);
		mrb_close(machine->mrb);
		machine->mrb_cxt = NULL;
		machine->mrb = NULL;
		CurrentMachine = NULL;
	}
}

static mrb_bool catcherr(tic_machine* machine)
{
	mrb_state* mrb = machine->mrb;
	if (mrb->exc)
	{
		mrb_value ex = mrb_obj_value(mrb->exc);
		mrb_value msg = mrb_funcall(mrb, ex, "inspect", 0);
		char* cmsg = mrb_str_to_cstr(mrb, msg);
		machine->data->error(machine->data->data, cmsg);
		mrb->exc = NULL;

		return false;
	}

	return true;
}

static bool initMRuby(tic_mem* tic, const char* code)
{
	tic_machine* machine = (tic_machine*)tic;

	closeMRuby(tic);

	CurrentMachine = machine;

	mrb_state* mrb = machine->mrb = mrb_open();
	mrbc_context* mrb_cxt = machine->mrb_cxt = mrbc_context_new(mrb);
	mrb_cxt->capture_errors = 1;
	mrbc_filename(mrb, mrb_cxt, "user code");

	for (s32 i = 0; i < COUNT_OF(ApiFunc); i++)
		if (ApiFunc[i])
			mrb_define_method(machine->mrb, mrb->kernel_module, ApiKeywords[i], ApiFunc[i], ApiParam[i]);

	mrb_load_string_cxt(mrb, code, mrb_cxt);
	return catcherr(machine);
}

static void evalMRuby(tic_mem* tic, const char* code) {
	tic_machine* machine = (tic_machine*)tic;

	mrb_state* mrb = machine->mrb = mrb_open();

	if (!mrb)
		return;

	if (machine->mrb_cxt)
	{
		mrbc_context_free(mrb, machine->mrb_cxt);
	}

	mrbc_context* mrb_cxt = machine->mrb_cxt = mrbc_context_new(mrb);
	mrb_load_string_cxt(mrb, code, mrb_cxt);
	catcherr(machine);
}

static void callMRubyTick(tic_mem* tic)
{
	tic_machine* machine = (tic_machine*)tic;
	const char* TicFunc = ApiKeywords[0];

	mrb_state* mrb = machine->mrb;

	if(mrb)
	{
		if (mrb_respond_to(mrb, mrb_top_self(mrb), mrb_intern_cstr(mrb, TicFunc)))
		{
			mrb_funcall(mrb, mrb_top_self(mrb), TicFunc, 0);
			catcherr(machine);
		}
		else
		{
			machine->data->error(machine->data->data, "'def TIC...' isn't found :(");
		}
	}
}

static void callMRubyScanlineName(tic_mem* memory, s32 row, void* data, const char* name)
{
	tic_machine* machine = (tic_machine*)memory;
	mrb_state* mrb = machine->mrb;

	if (mrb && mrb_respond_to(mrb, mrb_top_self(mrb), mrb_intern_cstr(mrb, name)))
	{
		mrb_funcall(mrb, mrb_top_self(mrb), name, 1, mrb_fixnum_value(row));
		catcherr(machine);
	}
}

static void callMRubyScanline(tic_mem* memory, s32 row, void* data)
{
	callMRubyScanlineName(memory, row, data, ApiKeywords[1]);

	callMRubyScanlineName(memory, row, data, "scanline");
}

static void callMRubyOverline(tic_mem* memory, void* data)
{
	tic_machine* machine = (tic_machine*)memory;
	mrb_state* mrb = machine->mrb;

	if (mrb)
	{
		const char* OvrFunc = ApiKeywords[2];

		if (mrb_respond_to(mrb, mrb_top_self(mrb), mrb_intern_cstr(mrb, OvrFunc))) {
			mrb_funcall(mrb, mrb_top_self(mrb), OvrFunc, 0);
			catcherr(machine);
		}
	}
}

/**
 * # External Resources
 * [Outdated official documentation regarding syntax](https://ruby-doc.org/docs/ruby-doc-bundle/Manual/man-1.4/syntax.html#resword)
 * [Ruby for dummies reserved word listing](https://www.dummies.com/programming/ruby/rubys-reserved-words/)
 */
static const char* const MRubyKeywords [] =
{
	"BEGIN", "class", "ensure", "nil", "self", "when",
	"END", "def", "false", "not", "super", "while",
	"alias", "defined", "for", "or", "then", "yield",
	"and", "do", "if", "redo", "true",
	"begin", "else", "in", "rescue", "undef",
	"break", "elsif", "module", "retry", "unless",
	"case", "end", "next", "return", "until",
	"__FILE__", "__LINE__"
};

static inline bool isalnum_(char c) {return isalnum(c) || c == '_' || c == '?' || c == '=' || c == '!';}

static const tic_outline_item* getMRubyOutline(const char* code, s32* size)
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
		static const char FuncString[] = "def ";

		ptr = strstr(ptr, FuncString);

		if(ptr)
		{
			ptr += sizeof FuncString - 1;

			const char* start = ptr;
			const char* end = start;

			while(*ptr)
			{
				char c = *ptr;

				if(isalnum_(c) || c == ' ');
				else if(c == '(' || c == '\n')
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

static const tic_script_config MRubySyntaxConfig =
{
	.init 				= initMRuby,
	.close 				= closeMRuby,
	.tick 				= callMRubyTick,
	.scanline 			= callMRubyScanline,
	.overline 			= callMRubyOverline,

	.getOutline			= getMRubyOutline,
	.parse 				= parseCode,
	.eval				= evalMRuby,

	.blockCommentStart 	= "=begin",
	.blockCommentEnd 	= "=end",
	.singleComment 		= "#",
	.blockStringStart	= NULL,
	.blockStringEnd		= NULL,

	.keywords 			= MRubyKeywords,
	.keywordsCount 		= COUNT_OF(MRubyKeywords),

	.api 				= ApiKeywords,
	.apiCount 			= COUNT_OF(ApiKeywords),
};

const tic_script_config* getMRubyScriptConfig()
{
	return &MRubySyntaxConfig;
}

#endif /* defined(TIC_BUILD_WITH_MRUBY) */
