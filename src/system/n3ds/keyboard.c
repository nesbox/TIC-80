// MIT License

// Copyright (c) 2020 Adrian "asie" Siekierka

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

#include "system.h"
#include "keyboard.h"
#include "utils.h"

typedef struct
{
	u16 x, y, w, h;
	tic_keycode keycode;
	u8 flags;
} touch_area_t;

#define TOUCH_AREA_MODIFIER 1
#define TOUCH_AREA_MOD_TOGGLE 2

static const touch_area_t touch_areas[] = {
  { 2, 97, 22, 16, tic_key_escape, 0 },
  { 23, 97, 22, 16, tic_key_f1, 0 },
  { 44, 97, 22, 16, tic_key_f2, 0 },
  { 65, 97, 22, 16, tic_key_f3, 0 },
  { 86, 97, 22, 16, tic_key_f4, 0 },
  { 107, 97, 22, 16, tic_key_f5, 0 },
  { 128, 97, 22, 16, tic_key_f6, 0 },
  { 149, 97, 22, 16, tic_key_f7, 0 },
  { 170, 97, 22, 16, tic_key_f8, 0 },
  { 191, 97, 22, 16, tic_key_f9, 0 },
  { 212, 97, 22, 16, tic_key_f10, 0 },
  { 233, 97, 22, 16, tic_key_f11, 0 },
  { 254, 97, 22, 16, tic_key_f12, 0 },
  { 275, 97, 22, 16, tic_key_insert, 0 },
  { 296, 97, 22, 16, tic_key_delete, 0 },

  { 2, 112, 22, 22, tic_key_grave, 0 },
  { 23, 112, 22, 22, tic_key_1, 0 },
  { 44, 112, 22, 22, tic_key_2, 0 },
  { 65, 112, 22, 22, tic_key_3, 0 },
  { 86, 112, 22, 22, tic_key_4, 0 },
  { 107, 112, 22, 22, tic_key_5, 0 },
  { 128, 112, 22, 22, tic_key_6, 0 },
  { 149, 112, 22, 22, tic_key_7, 0 },
  { 170, 112, 22, 22, tic_key_8, 0 },
  { 191, 112, 22, 22, tic_key_9, 0 },
  { 212, 112, 22, 22, tic_key_0, 0 },
  { 233, 112, 22, 22, tic_key_minus, 0 },
  { 254, 112, 22, 22, tic_key_equals, 0 },
  { 275, 112, 43, 22, tic_key_backspace, 0 },

  { 2, 133, 34, 22, tic_key_tab, 0 },
  { 35, 133, 22, 22, tic_key_q, 0 },
  { 56, 133, 22, 22, tic_key_w, 0 },
  { 77, 133, 22, 22, tic_key_e, 0 },
  { 98, 133, 22, 22, tic_key_r, 0 },
  { 119, 133, 22, 22, tic_key_t, 0 },
  { 140, 133, 22, 22, tic_key_y, 0 },
  { 161, 133, 22, 22, tic_key_u, 0 },
  { 182, 133, 22, 22, tic_key_i, 0 },
  { 203, 133, 22, 22, tic_key_o, 0 },
  { 224, 133, 22, 22, tic_key_p, 0 },
  { 245, 133, 22, 22, tic_key_leftbracket, 0 },
  { 266, 133, 22, 22, tic_key_rightbracket, 0 },
  { 287, 133, 31, 22, tic_key_backslash, 0 },

  { 2, 154, 41, 22, tic_key_ctrl, TOUCH_AREA_MODIFIER },
  { 42, 154, 22, 22, tic_key_a, 0 },
  { 63, 154, 22, 22, tic_key_s, 0 },
  { 84, 154, 22, 22, tic_key_d, 0 },
  { 105, 154, 22, 22, tic_key_f, 0 },
  { 126, 154, 22, 22, tic_key_g, 0 },
  { 147, 154, 22, 22, tic_key_h, 0 },
  { 168, 154, 22, 22, tic_key_j, 0 },
  { 189, 154, 22, 22, tic_key_k, 0 },
  { 210, 154, 22, 22, tic_key_l, 0 },
  { 231, 154, 22, 22, tic_key_semicolon, 0 },
  { 252, 154, 22, 22, tic_key_apostrophe, 0 },
  { 273, 154, 45, 22, tic_key_return, 0 },

  { 2, 175, 54, 22, tic_key_shift, TOUCH_AREA_MODIFIER },
  { 55, 175, 22, 22, tic_key_z, 0 },
  { 76, 175, 22, 22, tic_key_x, 0 },
  { 97, 175, 22, 22, tic_key_c, 0 },
  { 118, 175, 22, 22, tic_key_v, 0 },
  { 139, 175, 22, 22, tic_key_b, 0 },
  { 160, 175, 22, 22, tic_key_n, 0 },
  { 181, 175, 22, 22, tic_key_m, 0 },
  { 202, 175, 22, 22, tic_key_comma, 0 },
  { 223, 175, 22, 22, tic_key_period, 0 },
  { 244, 175, 22, 22, tic_key_slash, 0 },
  { 265, 175, 53, 22, tic_key_shift, TOUCH_AREA_MODIFIER },

  { 2, 196, 28, 22, tic_key_capslock, TOUCH_AREA_MODIFIER | TOUCH_AREA_MOD_TOGGLE },
  { 29, 196, 37, 22, tic_key_alt, TOUCH_AREA_MODIFIER },
  { 65, 196, 142, 22, tic_key_space, 0 },
  { 206, 196, 28, 22, tic_key_alt, TOUCH_AREA_MODIFIER },
  { 233, 196, 22, 22, tic_key_home, 0 },
  { 254, 196, 22, 22, tic_key_up, 0 },
  { 275, 196, 22, 22, tic_key_end, 0 },
  { 296, 196, 22, 22, tic_key_pageup, 0 },

  { 233, 217, 22, 22, tic_key_left, 0 },
  { 254, 217, 22, 22, tic_key_down, 0 },
  { 275, 217, 22, 22, tic_key_right, 0 },
  { 296, 217, 22, 22, tic_key_pagedown, 0 }
};

#define touch_areas_len (sizeof(touch_areas) / sizeof(touch_area_t))

extern void n3ds_draw_texture(C3D_Tex* tex, int x, int y, int tx, int ty, int width, int height, int twidth, int theight,
float cmul);

void n3ds_keyboard_init(tic_n3ds_keyboard *kbd) {
	memset(kbd, 0, sizeof(tic_n3ds_keyboard));
	if (!ctr_load_png(&kbd->tex, "romfs:/kbd_display.png", TEXTURE_TARGET_VRAM))
	{
		consoleInit(GFX_BOTTOM, NULL);
	}
	kbd->render_dirty = true;
}

void n3ds_keyboard_free(tic_n3ds_keyboard *kbd) {
	C3D_TexDelete(&kbd->tex);
}

static void n3ds_keyboard_draw_pressed(tic_n3ds_keyboard *kbd, int pos) {
	const touch_area_t* area = &touch_areas[pos];
	n3ds_draw_texture(&(kbd->tex), area->x, area->y,
		area->x, 16 + (area->y - 1),
		area->w, area->h - 1,
		area->w, area->h - 1, 0.5f);
}

void n3ds_keyboard_draw(tic_n3ds_keyboard *kbd) {
	if (kbd->tex.data != NULL)
	{
		n3ds_draw_texture(&(kbd->tex), 0, 0, 0, 16, 320, 240, 320, 240, 1.0f);
		for(int i = 0; i < kbd->kd_count; i++)
		{
			n3ds_keyboard_draw_pressed(kbd, kbd->kd[i]);
		}
	}
}

static inline
bool n3ds_key_touched(touchPosition* pos, const touch_area_t* area)
{
  return pos->px >= area->x && pos->py >= area->y
    && pos->px < (area->x + area->w)
    && pos->py < (area->y + area->h);
}

#define MAP_BUTTON_KEY(keymask, tickey) \
	if ((key_held & (keymask)) && !state[(tickey)] && (buffer_pos < TIC80_KEY_BUFFER)) \
	{ \
		tic_kbd->keys[buffer_pos++] = (tickey); \
		state[(tickey)] = true; \
	}

void n3ds_keyboard_update(tic_n3ds_keyboard *kbd, tic_mem *tic, char *chcode) {
	u32 key_down, key_up, key_held;
	touchPosition touch;
	const touch_area_t* area;
	bool changed = false;
	bool state[tic_keys_count];
	int i, j;

	tic80_keyboard *tic_kbd = &tic->ram.input.keyboard;

	memset(&state, 0, sizeof(state));
	key_down = hidKeysDown();
	key_held = hidKeysHeld();
	key_up = hidKeysUp();

	changed = false;
	if((key_down | key_up) & KEY_TOUCH)
	{
		hidTouchRead(&touch);

		if(key_down & KEY_TOUCH)
		{
			for(i = 0; i < touch_areas_len; i++)
			{
				area = &touch_areas[i];
				if(n3ds_key_touched(&touch, area))
				{
					int set = -1;
					for(j = 0; j < kbd->kd_count; j++)
					{
						if(kbd->kd[j] == i)
						{
							set = j;
							break;
						}
					}

					if(set < 0)
					{
						kbd->kd[kbd->kd_count++] = i;
						changed = true;
					}
					else if(area->flags & TOUCH_AREA_MODIFIER)
					{
						// allow de-pressing modifiers
						kbd->kd[set] = 0;
						for (j = set + 1; j < kbd->kd_count; j++) {
							kbd->kd[j - 1] = kbd->kd[j];
							kbd->kd[j] = 0;
						}
						kbd->kd_count--;
						changed = true;
					}

					break;
				}
			}
		}
		else if(key_up & KEY_TOUCH)
		{
			if(kbd->kd_count > 0 && !(touch_areas[kbd->kd[kbd->kd_count - 1]].flags & TOUCH_AREA_MODIFIER)) {
				for(i = 0, j = 0; i < kbd->kd_count; i++) {
					if(touch_areas[kbd->kd[i]].flags & TOUCH_AREA_MOD_TOGGLE) {
						kbd->kd[j++] = kbd->kd[i];
					}
				}
				kbd->kd_count = j;
				changed = true;
			}
		}
	}

	changed |= (key_down | key_up) & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B | KEY_X | KEY_Y);

	if (changed) {
		// apply to TIC-80
		kbd->render_dirty = true;
		
		tic_kbd->data = 0;
		int buffer_pos = 0;
		for(i = 0; i < kbd->kd_count && buffer_pos < TIC80_KEY_BUFFER; i++)
		{
			tic_keycode curr_keycode = touch_areas[kbd->kd[i]].keycode;
			if(!state[curr_keycode])
			{
				tic_kbd->keys[buffer_pos++] = curr_keycode;
				state[curr_keycode] = true;
			}
		}

		MAP_BUTTON_KEY(KEY_UP, tic_key_up);
		MAP_BUTTON_KEY(KEY_DOWN, tic_key_down);
		MAP_BUTTON_KEY(KEY_LEFT, tic_key_left);
		MAP_BUTTON_KEY(KEY_RIGHT, tic_key_right);
		MAP_BUTTON_KEY(KEY_A, tic_key_z);
		MAP_BUTTON_KEY(KEY_B, tic_key_x);
		MAP_BUTTON_KEY(KEY_X, tic_key_a);
		MAP_BUTTON_KEY(KEY_Y, tic_key_s);

		// TODO: merge with sdlgpu.c
		if (chcode != NULL)
		{
			*chcode = 0;
			if (buffer_pos > 0)
			{
				static const char Symbols[] =   " abcdefghijklmnopqrstuvwxyz0123456789-=[]\\;'`,./ ";
				static const char Shift[] =     " ABCDEFGHIJKLMNOPQRSTUVWXYZ)!@#$%^&*(_+{}|:\"~<>? ";

				enum{Count = sizeof Symbols};

				for(s32 i = 0; i < buffer_pos; i++)
				{
					tic_key key = tic_kbd->keys[i];

					if(key > 0 && key < Count && tic_api_keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD))
					{
						bool caps = tic_api_key(tic, tic_key_capslock);
						bool shift = tic_api_key(tic, tic_key_shift);

						*chcode = caps
							? key >= tic_key_a && key <= tic_key_z 
								? shift ? Symbols[key] : Shift[key]
								: shift ? Shift[key] : Symbols[key]
							: shift ? Shift[key] : Symbols[key];

						break;
					}
				}
			}
		}
	}
}

void n3ds_gamepad_update(tic_n3ds_keyboard *kbd, tic_mem *tic) {
	tic80_gamepads *tic_pad = &tic->ram.input.gamepads;
	tic80_mouse *tic_mouse = &tic->ram.input.mouse;
	u32 key_held = hidKeysHeld();
	u64 curr_clock = svcGetSystemTick();
	circlePosition cstick;

	// gamepad
	tic_pad->first.data = 0;
	tic_pad->first.up = (key_held & KEY_UP) != 0;
	tic_pad->first.down = (key_held & KEY_DOWN) != 0;
	tic_pad->first.left = (key_held & KEY_LEFT) != 0;
	tic_pad->first.right = (key_held & KEY_RIGHT) != 0;
	tic_pad->first.a = (key_held & KEY_A) != 0;
	tic_pad->first.b = (key_held & KEY_B) != 0;
	tic_pad->first.x = (key_held & KEY_X) != 0;
	tic_pad->first.y = (key_held & KEY_Y) != 0;

	// mouse scroll
	tic_mouse->scrollx = 0;
	tic_mouse->scrolly = 0;
	
	if (curr_clock >= kbd->scroll_debounce) {		
		if(key_held & KEY_CSTICK_UP)
			tic_mouse->scrolly = 1;
		else if(key_held & KEY_CSTICK_DOWN)
			tic_mouse->scrolly = -1;

		if(key_held & KEY_CSTICK_LEFT)
			tic_mouse->scrollx = -1;
		else if(key_held & KEY_CSTICK_RIGHT)
			tic_mouse->scrollx = 1;

		if(tic_mouse->scrollx != 0 || tic_mouse->scrolly != 0)
		{
			hidCstickRead(&cstick);
			int dmax = cstick.dx > cstick.dy ? cstick.dx : cstick.dy;
			float delay = 250.0f / (1 + (dmax / 22));

			kbd->scroll_debounce = curr_clock + (u64) (delay * CPU_TICKS_PER_MSEC);
		}
	}
}