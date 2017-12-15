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

#include "sfx.h"
#include "history.h"

#define CANVAS_SIZE 6
#define CANVAS_COLS (SFX_TICKS)
#define CANVAS_ROWS (16)
#define CANVAS_WIDTH (CANVAS_COLS * CANVAS_SIZE)
#define CANVAS_HEIGHT (CANVAS_ROWS * CANVAS_SIZE)

#define PIANO_BUTTON_WIDTH 9
#define PIANO_BUTTON_HEIGHT 16
#define PIANO_WHITE_BUTTONS 7
#define PIANO_WIDTH ((PIANO_BUTTON_WIDTH+1)*PIANO_WHITE_BUTTONS)
#define PIANO_HEIGHT (PIANO_BUTTON_HEIGHT)

#define DEFAULT_CHANNEL 0

static void drawSwitch(Sfx* sfx, s32 x, s32 y, const char* label, s32 value, void(*set)(Sfx*, s32))
{
	static const u8 LeftArrow[] = 
	{
		0b00010000,
		0b00110000,
		0b01110000,
		0b00110000,
		0b00010000,
		0b00000000,
		0b00000000,
		0b00000000,
	};

	static const u8 RightArrow[] = 
	{
		0b01000000,
		0b01100000,
		0b01110000,
		0b01100000,
		0b01000000,
		0b00000000,
		0b00000000,
		0b00000000,
	};

	sfx->tic->api.text(sfx->tic, label, x, y, (tic_color_white));

	{
		x += (s32)strlen(label)*TIC_FONT_WIDTH;

		SDL_Rect rect = {x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT};

		if(checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);

			if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
				set(sfx, -1);
		}

		drawBitIcon(rect.x, rect.y, LeftArrow, (tic_color_dark_gray));
	}

	{
		char val[] = "99";
		sprintf(val, "%02i", value);
		sfx->tic->api.fixed_text(sfx->tic, val, x += TIC_FONT_WIDTH, y, (tic_color_white));
	}

	{
		x += 2*TIC_FONT_WIDTH;

		SDL_Rect rect = {x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT};

		if(checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);

			if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
				set(sfx, +1);
		}

		drawBitIcon(rect.x, rect.y, RightArrow, (tic_color_dark_gray));
	}
}

static tic_sample* getEffect(Sfx* sfx)
{
	return sfx->src->samples.data + sfx->index;
}

static void setIndex(Sfx* sfx, s32 delta)
{
	sfx->index += delta;
}

static void setSpeed(Sfx* sfx, s32 delta)
{
	tic_sample* effect = getEffect(sfx);

	effect->speed += delta;

	history_add(sfx->history);
}

static void drawTopPanel(Sfx* sfx, s32 x, s32 y)
{
	const s32 Gap = 8*TIC_FONT_WIDTH;

	drawSwitch(sfx, x, y, "IDX", sfx->index, setIndex);

	tic_sample* effect = getEffect(sfx);

	drawSwitch(sfx, x += Gap, y, "SPD", effect->speed, setSpeed);
}

static void setLoopStart(Sfx* sfx, s32 delta)
{
	tic_sample* effect = getEffect(sfx);
	tic_sound_loop* loop = effect->loops + sfx->canvasTab;

	loop->start += delta;

	history_add(sfx->history);
}

static void setLoopSize(Sfx* sfx, s32 delta)
{
	tic_sample* effect = getEffect(sfx);
	tic_sound_loop* loop = effect->loops + sfx->canvasTab;

	loop->size += delta;

	history_add(sfx->history);
}

static void drawLoopPanel(Sfx* sfx, s32 x, s32 y)
{
	sfx->tic->api.text(sfx->tic, "LOOP:", x, y, (tic_color_dark_gray));

	enum {Gap = 2};

	tic_sample* effect = getEffect(sfx);
	tic_sound_loop* loop = effect->loops + sfx->canvasTab;

	drawSwitch(sfx, x, y += Gap + TIC_FONT_HEIGHT, "", loop->size, setLoopSize);
	drawSwitch(sfx, x, y += Gap + TIC_FONT_HEIGHT, "", loop->start, setLoopStart);
}

static tic_waveform* getWaveformById(Sfx* sfx, s32 i)
{
	return &sfx->src->waveform.envelopes[i];
}

static tic_waveform* getWaveform(Sfx* sfx)
{
	return getWaveformById(sfx, sfx->waveform.index);
}

static void drawWaveButtons(Sfx* sfx, s32 x, s32 y)
{
	static const u8 EmptyIcon[] = 
	{
		0b01110000,
		0b10001000,
		0b10001000,
		0b10001000,
		0b01110000,
		0b00000000,
		0b00000000,
		0b00000000,
	};

	static const u8 FullIcon[] = 
	{
		0b01110000,
		0b11111000,
		0b11111000,
		0b11111000,
		0b01110000,
		0b00000000,
		0b00000000,
		0b00000000,
	};

	enum {Scale = 4, Width = 10, Height = 5, Gap = 1, Count = ENVELOPES_COUNT, Rows = Count, HGap = 2};

	for(s32 i = 0; i < Count; i++)
	{
		SDL_Rect rect = {x, y + (Count - i - 1)*(Height+Gap), Width, Height};

		bool over = false;

		if(checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);
			over = true;

			if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
			{
				sfx->waveform.index = i;
				sfx->tab = SFX_WAVEFORM_TAB;
				return;
			}
		}

		bool active = false;
		if(sfx->play.active)
		{
			tic_sfx_pos pos = sfx->tic->api.sfx_pos(sfx->tic, DEFAULT_CHANNEL);

			if(pos.wave >= 0 && getEffect(sfx)->data[pos.wave].wave == i)
				active = true;
		}

		sfx->tic->api.rect(sfx->tic, rect.x, rect.y, rect.w, rect.h, 
			active ? (tic_color_red) : over ? (tic_color_gray) : (tic_color_dark_gray));

		{
			enum{Size = 5};
			SDL_Rect iconRect = {x+Width+HGap, y + (Count - i - 1)*(Height+Gap), Size, Size};

			bool over = false;
			if(checkMousePos(&iconRect))
			{
				setCursor(SDL_SYSTEM_CURSOR_HAND);
				over = true;

				if(checkMouseClick(&iconRect, SDL_BUTTON_LEFT))
				{
					tic_sample* effect = getEffect(sfx);
					for(s32 c = 0; c < SFX_TICKS; c++)
						effect->data[c].wave = i;
				}
			}

			drawBitIcon(iconRect.x, iconRect.y, EmptyIcon, (over ? tic_color_gray : tic_color_dark_gray));
		}

		{
			tic_waveform* wave = getWaveformById(sfx, i);

			for(s32 i = 0; i < ENVELOPE_VALUES/Scale; i++)
			{
				s32 value = tic_tool_peek4(wave->data, i*Scale)/Scale;
				sfx->tic->api.pixel(sfx->tic, rect.x + i+1, rect.y + Height - value - 2, (tic_color_white));
			}
		}
	}

	// draw full icon
	{
		tic_sample* effect = getEffect(sfx);
		u8 start = effect->data[0].wave;
		bool full = true;
		for(s32 c = 1; c < SFX_TICKS; c++)
			if(effect->data[c].wave != start)
			{
				full = false;
				break;
			}

		if(full)
			drawBitIcon(x+Width+HGap, y + (Count - start - 1)*(Height+Gap), FullIcon, (tic_color_white));
	}
}

static void drawCanvasTabs(Sfx* sfx, s32 x, s32 y)
{
	static const char* Labels[] = {"WAVE", "VOLUME", "ARPEGG", "PITCH"};

	enum {Height = TIC_FONT_HEIGHT+2};

	for(s32 i = 0, sy = y; i < COUNT_OF(Labels); sy += Height, i++)
	{
		s32 size = sfx->tic->api.text(sfx->tic, Labels[i], 0, -TIC_FONT_HEIGHT, (tic_color_black));

		SDL_Rect rect = {x - size, sy, size, TIC_FONT_HEIGHT};

		if(checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);

			if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
			{
				sfx->canvasTab = i;
			}
		}

		sfx->tic->api.text(sfx->tic, Labels[i], rect.x, rect.y, i == sfx->canvasTab ? (tic_color_white) : (tic_color_dark_gray));
	}

	tic_sample* effect = getEffect(sfx);

	switch(sfx->canvasTab)
	{
	case SFX_PITCH_TAB:
		{
			static const char Label[] = "x16";
			enum{Width = (sizeof Label - 1) * TIC_FONT_WIDTH};
			SDL_Rect rect = {(x - Width)/2, y + Height * 6, Width, TIC_FONT_HEIGHT};

			if(checkMousePos(&rect))
			{
				setCursor(SDL_SYSTEM_CURSOR_HAND);

				if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
					effect->pitch16x++;
			}

			sfx->tic->api.fixed_text(sfx->tic, Label, rect.x, rect.y, (effect->pitch16x ? tic_color_white : tic_color_dark_gray));			
		}
		break;
	case SFX_ARPEGGIO_TAB:
		{
			static const char Label[] = "DOWN";
			enum{Width = (sizeof Label - 1) * TIC_FONT_WIDTH};
			SDL_Rect rect = {(x - Width)/2, y + Height * 6, Width, TIC_FONT_HEIGHT};

			if(checkMousePos(&rect))
			{
				setCursor(SDL_SYSTEM_CURSOR_HAND);

				if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
					effect->reverse++;
			}

			sfx->tic->api.text(sfx->tic, Label, rect.x, rect.y, (effect->reverse ? tic_color_white : tic_color_dark_gray));
		}	
		break;
	default: break;
	}
}

static void drawCanvas(Sfx* sfx, s32 x, s32 y)
{
	sfx->tic->api.rect(sfx->tic, x, y, CANVAS_WIDTH, CANVAS_HEIGHT, (tic_color_dark_red));

	for(s32 i = 0; i < CANVAS_HEIGHT; i += CANVAS_SIZE)
		sfx->tic->api.line(sfx->tic, x, y + i, x + CANVAS_WIDTH, y + i, TIC_COLOR_BG);

	for(s32 i = 0; i < CANVAS_WIDTH; i += CANVAS_SIZE)
		sfx->tic->api.line(sfx->tic, x + i, y, x + i, y + CANVAS_HEIGHT, TIC_COLOR_BG);

	{
		tic_sfx_pos pos = sfx->tic->api.sfx_pos(sfx->tic, DEFAULT_CHANNEL);
		s32 tickIndex = *(pos.data + sfx->canvasTab);

		if(tickIndex >= 0)
			sfx->tic->api.rect(sfx->tic, x + tickIndex * CANVAS_SIZE, y, CANVAS_SIZE + 1, CANVAS_HEIGHT + 1, (tic_color_white));
	}

	SDL_Rect rect = {x, y, CANVAS_WIDTH, CANVAS_HEIGHT};

	tic_sample* effect = getEffect(sfx);

	if(checkMousePos(&rect))
	{
		setCursor(SDL_SYSTEM_CURSOR_HAND);

		s32 mx = getMouseX() - x;
		s32 my = getMouseY() - y;

		mx -= mx % CANVAS_SIZE;
		my -= my % CANVAS_SIZE;

		if(checkMouseDown(&rect, SDL_BUTTON_LEFT))
		{
			mx /= CANVAS_SIZE;
			my /= CANVAS_SIZE;

			switch(sfx->canvasTab)
			{
			case SFX_VOLUME_TAB: 	effect->data[mx].volume = my; break;
			case SFX_ARPEGGIO_TAB: 	effect->data[mx].arpeggio = CANVAS_ROWS - my - 1; break;
			case SFX_PITCH_TAB: 	effect->data[mx].pitch = CANVAS_ROWS / 2 - my - 1; break;
			case SFX_WAVE_TAB: 		effect->data[mx].wave = CANVAS_ROWS - my - 1; break;
			default: break;
			}

			history_add(sfx->history);
		}
	}

	for(s32 i = 0; i < CANVAS_COLS; i++)
	{
		switch(sfx->canvasTab)
		{
		case SFX_VOLUME_TAB:
			{
				for(s32 j = 1, start = CANVAS_HEIGHT - CANVAS_SIZE; j <= CANVAS_ROWS - effect->data[i].volume; j++, start -= CANVAS_SIZE)
					sfx->tic->api.rect(sfx->tic, x + i * CANVAS_SIZE + 1, y + 1 + start, CANVAS_SIZE-1, CANVAS_SIZE-1, (tic_color_red));				
			}
			break;
		case SFX_ARPEGGIO_TAB:
			{
				sfx->tic->api.rect(sfx->tic, x + i * CANVAS_SIZE + 1, 
					y + 1 + (CANVAS_HEIGHT - (effect->data[i].arpeggio+1)*CANVAS_SIZE), CANVAS_SIZE-1, CANVAS_SIZE-1, (tic_color_red));
			}
			break;
		case SFX_PITCH_TAB:
			{
				for(s32 j = SDL_min(0, effect->data[i].pitch); j <= SDL_max(0, effect->data[i].pitch); j++)
					sfx->tic->api.rect(sfx->tic, x + i * CANVAS_SIZE + 1, y + 1 + (CANVAS_HEIGHT/2 - (j+1)*CANVAS_SIZE), 
						CANVAS_SIZE-1, CANVAS_SIZE-1, (tic_color_red));
			}
			break;
		case SFX_WAVE_TAB:
			{
				sfx->tic->api.rect(sfx->tic, x + i * CANVAS_SIZE + 1, 
					y + 1 + (CANVAS_HEIGHT - (effect->data[i].wave+1)*CANVAS_SIZE), CANVAS_SIZE-1, CANVAS_SIZE-1, (tic_color_red));
			}
			break;
		default: break;
		}
	}

	{
		tic_sound_loop* loop = effect->loops + sfx->canvasTab;
		if(loop->start > 0 || loop->size > 0)
		{
			for(s32 i = 0; i < loop->size; i++)
				sfx->tic->api.rect(sfx->tic, x + (loop->start+i) * CANVAS_SIZE+1, y + CANVAS_HEIGHT - 2, CANVAS_SIZE-1, 2, (tic_color_yellow));
		}
	}
}

static void drawPiano(Sfx* sfx, s32 x, s32 y)
{
	tic_sample* effect = getEffect(sfx);

	static const s32 ButtonIndixes[] = {0, 2, 4, 5, 7, 9, 11, 1, 3, -1, 6, 8, 10};

	SDL_Rect buttons[COUNT_OF(ButtonIndixes)];

	for(s32 i = 0; i < COUNT_OF(buttons); i++)
	{
		buttons[i] = i < PIANO_WHITE_BUTTONS 
			? (SDL_Rect){x + (PIANO_BUTTON_WIDTH+1)*i, y, PIANO_BUTTON_WIDTH + 1, PIANO_BUTTON_HEIGHT}
			: (SDL_Rect){x + (7 + 3) * (i - PIANO_WHITE_BUTTONS) + 6, y, 7, 8};
	}

	SDL_Rect rect = {x, y, PIANO_WIDTH, PIANO_HEIGHT};

	if(checkMousePos(&rect))
	{
		setCursor(SDL_SYSTEM_CURSOR_HAND);

		static const char* Tooltips[] = {"C [z]", "C# [s]", "D [x]", "D# [d]", "E [c]", "F [v]", "F# [g]", "G [b]", "G# [h]", "A [n]", "A# [j]", "B [m]" };

		for(s32 i = COUNT_OF(buttons) - 1; i >= 0; i--)
		{
			SDL_Rect* rect = buttons + i;

			if(checkMousePos(rect))
				if(ButtonIndixes[i] >= 0)
				{
					showTooltip(Tooltips[ButtonIndixes[i]]);
					break;
				}
		}

		if(checkMouseDown(&rect, SDL_BUTTON_LEFT))
		{
			for(s32 i = COUNT_OF(buttons) - 1; i >= 0; i--)
			{
				SDL_Rect* rect = buttons + i;
				s32 index = ButtonIndixes[i];

				if(index >= 0)
				{
					if(checkMousePos(rect))
					{
						effect->note = index;
						sfx->play.active = true;
						break;
					}
				}
			}			
		}
	}

	for(s32 i = 0; i < COUNT_OF(buttons); i++)
	{
		SDL_Rect* rect = buttons + i;
		bool white = i < PIANO_WHITE_BUTTONS;
		s32 index = ButtonIndixes[i];

		if(index >= 0)
			sfx->tic->api.rect(sfx->tic, rect->x, rect->y, rect->w - (white ? 1 : 0), rect->h, 
				(sfx->play.active && effect->note == index ? tic_color_red : white ? tic_color_white : tic_color_black));
	}
}

static void drawOctavePanel(Sfx* sfx, s32 x, s32 y)
{
	tic_sample* effect = getEffect(sfx);

	static const char Label[] = "OCT";
	sfx->tic->api.text(sfx->tic, Label, x, y, (tic_color_white));

	x += sizeof(Label)*TIC_FONT_WIDTH;

	enum {Gap = 5};

	for(s32 i = 0; i < OCTAVES; i++)
	{
		SDL_Rect rect = {x + i * (TIC_FONT_WIDTH + Gap), y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT};

		if(checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);

			if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
			{
				effect->octave = i;
			}
		}

		sfx->tic->api.draw_char(sfx->tic, i + '1', rect.x, rect.y, (i == effect->octave ? tic_color_white : tic_color_dark_gray));
	}
}

static void playSound(Sfx* sfx)
{
	if(sfx->play.active)
	{
		tic_sample* effect = getEffect(sfx);

		if(sfx->play.note != effect->note)
		{
			sfx->play.note = effect->note;
			sfx->tic->api.sfx_stop(sfx->tic, DEFAULT_CHANNEL);
			sfx->tic->api.sfx(sfx->tic, sfx->index, effect->note, effect->octave, -1, DEFAULT_CHANNEL);
		}
	}
	else
	{
		sfx->play.note = -1;
		sfx->tic->api.sfx_stop(sfx->tic, DEFAULT_CHANNEL);
	}
}

static void undo(Sfx* sfx)
{
	history_undo(sfx->history);
}

static void redo(Sfx* sfx)
{
	history_redo(sfx->history);
}

static void copyToClipboard(Sfx* sfx)
{
	tic_sample* effect = getEffect(sfx);
	toClipboard(effect, sizeof(tic_sample), true);
}

static void copyWaveToClipboard(Sfx* sfx)
{
	tic_waveform* wave = getWaveform(sfx);
	toClipboard(wave, sizeof(tic_waveform), true);
}

static void resetSfx(Sfx* sfx)
{
	tic_sample* effect = getEffect(sfx);
	memset(effect, 0, sizeof(tic_sample));

	history_add(sfx->history);
}

static void resetWave(Sfx* sfx)
{
	tic_waveform* wave = getWaveform(sfx);
	memset(wave, 0, sizeof(tic_waveform));

	history_add(sfx->history);
}

static void cutToClipboard(Sfx* sfx)
{
	copyToClipboard(sfx);
	resetSfx(sfx);
}

static void cutWaveToClipboard(Sfx* sfx)
{
	copyWaveToClipboard(sfx);
	resetWave(sfx);
}

static void copyFromClipboard(Sfx* sfx)
{
	tic_sample* effect = getEffect(sfx);

	if(fromClipboard(effect, sizeof(tic_sample), true, false))
		history_add(sfx->history);
}

static void copyWaveFromClipboard(Sfx* sfx)
{
	tic_waveform* wave = getWaveform(sfx);

	if(fromClipboard(wave, sizeof(tic_waveform), true, false))
		history_add(sfx->history);
}

static void processKeyboard(Sfx* sfx)
{
	s32 keyboardButton = -1;

	static const s32 Scancodes[] = 
	{
		SDL_SCANCODE_Z,
		SDL_SCANCODE_S,
		SDL_SCANCODE_X,
		SDL_SCANCODE_D,
		SDL_SCANCODE_C,
		SDL_SCANCODE_V,
		SDL_SCANCODE_G,
		SDL_SCANCODE_B,
		SDL_SCANCODE_H,
		SDL_SCANCODE_N,
		SDL_SCANCODE_J,
		SDL_SCANCODE_M,
	};

	SDL_Keymod keymod = SDL_GetModState();

	if(keymod & TIC_MOD_CTRL)
	{

	}
	else
	{
		for(int i = 0; i < COUNT_OF(Scancodes); i++)
			if(getKeyboard()[Scancodes[i]])
				keyboardButton = i;        
	}

	tic_sample* effect = getEffect(sfx);

	if(keyboardButton >= 0)
	{
		effect->note = keyboardButton;
		sfx->play.active = true;
	}

	if(getKeyboard()[SDL_SCANCODE_SPACE])
		sfx->play.active = true;
}

static void processKeydown(Sfx* sfx, SDL_Keycode keycode)
{
	switch(getClipboardEvent(keycode))
	{
	case TIC_CLIPBOARD_CUT: cutToClipboard(sfx); break;
	case TIC_CLIPBOARD_COPY: copyToClipboard(sfx); break;
	case TIC_CLIPBOARD_PASTE: copyFromClipboard(sfx); break;
	default: break;
	}

	SDL_Keymod keymod = SDL_GetModState();

	if(keymod & TIC_MOD_CTRL)
	{
		switch(keycode)
		{
		case SDLK_z: 	undo(sfx); break;
		case SDLK_y: 	redo(sfx); break;
		}
	}

	switch(keycode)
	{
	case SDLK_TAB: sfx->tab = SFX_WAVEFORM_TAB; break;
	case SDLK_LEFT: sfx->index--; break;
	case SDLK_RIGHT: sfx->index++; break;
	case SDLK_DELETE: resetSfx(sfx); break;
	}
}

static void processWaveformKeydown(Sfx* sfx, SDL_Keycode keycode)
{
	switch(getClipboardEvent(keycode))
	{
	case TIC_CLIPBOARD_CUT: cutWaveToClipboard(sfx); break;
	case TIC_CLIPBOARD_COPY: copyWaveToClipboard(sfx); break;
	case TIC_CLIPBOARD_PASTE: copyWaveFromClipboard(sfx); break;
	default: break;
	}

	SDL_Keymod keymod = SDL_GetModState();

	if(keymod & TIC_MOD_CTRL)
	{
		switch(keycode)
		{
		case SDLK_z: 	undo(sfx); break;
		case SDLK_y: 	redo(sfx); break;
		}
	}

	switch(keycode)
	{
	case SDLK_TAB: sfx->tab = SFX_ENVELOPES_TAB; break;
	case SDLK_LEFT: sfx->waveform.index--; break;
	case SDLK_RIGHT: sfx->waveform.index++; break;
	case SDLK_DELETE: resetWave(sfx); break;
	}
}

static void drawModeTabs(Sfx* sfx)
{
	static const u8 Icons[] =
	{
		0b00000000,
		0b00110000,
		0b01001001,
		0b01001001,
		0b01001001,
		0b00000110,
		0b00000000,
		0b00000000,

		0b00000000,
		0b01000000,
		0b01000100,
		0b01010100,
		0b01010101,
		0b01010101,
		0b00000000,
		0b00000000,
	};

	enum { Width = 9, Height = 7, Rows = 8, Count = sizeof Icons / Rows };

	for (s32 i = 0; i < Count; i++)
	{
		SDL_Rect rect = { TIC80_WIDTH - Width * (Count - i), 0, Width, Height };

		bool over = false;

		static const s32 Tabs[] = { SFX_WAVEFORM_TAB, SFX_ENVELOPES_TAB };

		if (checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);
			over = true;

			static const char* Tooltips[] = { "WAVEFORMS [tab]", "ENVELOPES [tab]" };
			showTooltip(Tooltips[i]);

			if (checkMouseClick(&rect, SDL_BUTTON_LEFT))
				sfx->tab = Tabs[i];
		}

		if (sfx->tab == Tabs[i])
			sfx->tic->api.rect(sfx->tic, rect.x, rect.y, rect.w, rect.h, (tic_color_black));

		drawBitIcon(rect.x, rect.y, Icons + i*Rows, (sfx->tab == Tabs[i] ? tic_color_white : over ? tic_color_dark_gray : tic_color_light_blue));
	}
}

static void drawSfxToolbar(Sfx* sfx)
{
	sfx->tic->api.rect(sfx->tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, (tic_color_white));

	enum{Width = 3 * TIC_FONT_WIDTH};
	s32 x = TIC80_WIDTH - Width - TIC_SPRITESIZE*3;
	s32 y = 1;

	SDL_Rect rect = {x, y, Width, TIC_FONT_HEIGHT};
	bool over = false;

	if(checkMousePos(&rect))
	{
		setCursor(SDL_SYSTEM_CURSOR_HAND);
		over = true;

		showTooltip("PLAY SFX [space]");

		if(checkMouseDown(&rect, SDL_BUTTON_LEFT))
		{
			sfx->play.active = true;
		}
	}

	tic_sample* effect = getEffect(sfx);

	{
		static const char* Notes[] = SFX_NOTES;
		char buf[] = "C#4";
		sprintf(buf, "%s%i", Notes[effect->note], effect->octave+1);

		sfx->tic->api.fixed_text(sfx->tic, buf, x, y, (over ? tic_color_dark_gray : tic_color_light_blue));
	}

	drawModeTabs(sfx);
}

static void envelopesTick(Sfx* sfx)
{
	SDL_Event* event = NULL;
	while ((event = pollEvent()))
	{
		switch(event->type)
		{
		case SDL_KEYDOWN:
			processKeydown(sfx, event->key.keysym.sym);
			break;
		}
	}

	processKeyboard(sfx);

	sfx->tic->api.clear(sfx->tic, TIC_COLOR_BG);

	enum{ Gap = 3, Start = 40};
	drawPiano(sfx, Start, TIC80_HEIGHT - PIANO_HEIGHT - Gap);

	drawSfxToolbar(sfx);
	drawToolbar(sfx->tic, TIC_COLOR_BG, false);

	drawTopPanel(sfx, Start, TOOLBAR_SIZE + Gap);
	drawCanvasTabs(sfx, Start-Gap, TOOLBAR_SIZE + Gap + TIC_FONT_HEIGHT+2);
	if(sfx->canvasTab == SFX_WAVE_TAB)
		drawWaveButtons(sfx, Start + CANVAS_WIDTH + Gap-1, TOOLBAR_SIZE + Gap + TIC_FONT_HEIGHT+2);

	drawLoopPanel(sfx, Gap, TOOLBAR_SIZE + Gap + TIC_FONT_HEIGHT+92);
	drawCanvas(sfx, Start-1, TOOLBAR_SIZE + Gap + TIC_FONT_HEIGHT + 1);
	drawOctavePanel(sfx, Start + Gap + PIANO_WIDTH + Gap-1, TIC80_HEIGHT - TIC_FONT_HEIGHT - (PIANO_HEIGHT - TIC_FONT_HEIGHT)/2 - Gap);
}

static void drawWaveformBar(Sfx* sfx, s32 x, s32 y)
{
	enum 
	{
		Border = 2,
		Scale = 2,
		Width = ENVELOPE_VALUES/Scale + Border, 
		Height = CANVAS_HEIGHT/CANVAS_SIZE/Scale + Border,
		Gap = 3,
		Rows = 2,
		Cols = ENVELOPES_COUNT/Rows,
	};

	for(s32 i = 0; i < ENVELOPES_COUNT; i++)
	{
		SDL_Rect rect = {x + (i%Cols)*(Width+Gap), y + (i/Cols)*(Height+Gap), Width, Height};

		if(checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);

			if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
				sfx->waveform.index = i;
		}

		bool active = false;
		if(sfx->play.active)
		{
			tic_sfx_pos pos = sfx->tic->api.sfx_pos(sfx->tic, DEFAULT_CHANNEL);

			if(pos.wave >= 0 && getEffect(sfx)->data[pos.wave].wave == i)
				active = true;
		}

		sfx->tic->api.rect(sfx->tic, rect.x, rect.y, rect.w, rect.h, (active ? tic_color_red : tic_color_white));

		if(sfx->waveform.index == i)
			sfx->tic->api.rect_border(sfx->tic, rect.x-2, rect.y-2, rect.w+4, rect.h+4, (tic_color_white));

		{
			tic_waveform* wave = getWaveformById(sfx, i);

			for(s32 i = 0; i < ENVELOPE_VALUES/Scale; i++)
			{
				s32 value = tic_tool_peek4(wave->data, i*Scale)/Scale;
				sfx->tic->api.pixel(sfx->tic, rect.x + i+1, rect.y + Height - value - 2, (tic_color_black));
			}
		}
	}
}

static void drawWaveformCanvas(Sfx* sfx, s32 x, s32 y)
{
	enum {Rows = CANVAS_ROWS, Width = ENVELOPE_VALUES * CANVAS_SIZE, Height = CANVAS_HEIGHT};

	SDL_Rect rect = {x, y, Width, Height};

	sfx->tic->api.rect(sfx->tic, rect.x, rect.y, rect.w, rect.h, (tic_color_dark_red));

	for(s32 i = 0; i < Height; i += CANVAS_SIZE)
		sfx->tic->api.line(sfx->tic, rect.x, rect.y + i, rect.x + Width, rect.y + i, TIC_COLOR_BG);

	for(s32 i = 0; i < Width; i += CANVAS_SIZE)
		sfx->tic->api.line(sfx->tic, rect.x + i, rect.y, rect.x + i, rect.y + Width, TIC_COLOR_BG);

	if(checkMousePos(&rect))
	{
		setCursor(SDL_SYSTEM_CURSOR_HAND);

		if(checkMouseDown(&rect, SDL_BUTTON_LEFT))
		{
			s32 mx = getMouseX() - x;
			s32 my = getMouseY() - y;

			mx -= mx % CANVAS_SIZE;
			my -= my % CANVAS_SIZE;

			mx /= CANVAS_SIZE;
			my /= CANVAS_SIZE;

			tic_waveform* wave = getWaveform(sfx);

			tic_tool_poke4(wave->data, mx, Rows - my - 1);

			history_add(sfx->history);
		}
	}

	tic_waveform* wave = getWaveform(sfx);

	for(s32 i = 0; i < ENVELOPE_VALUES; i++)
	{
		s32 value = tic_tool_peek4(wave->data, i);
		sfx->tic->api.rect(sfx->tic, x + i * CANVAS_SIZE + 1, 
			y + 1 + (Height - (value+1)*CANVAS_SIZE), CANVAS_SIZE-1, CANVAS_SIZE-1, (tic_color_red));
	}
}

static void waveformTick(Sfx* sfx)
{
	SDL_Event* event = NULL;
	while ((event = pollEvent()))
	{
		switch(event->type)
		{
		case SDL_KEYDOWN:
			processWaveformKeydown(sfx, event->key.keysym.sym);
			break;
		}
	}

	processKeyboard(sfx);

	sfx->tic->api.clear(sfx->tic, TIC_COLOR_BG);

	drawSfxToolbar(sfx);
	drawToolbar(sfx->tic, TIC_COLOR_BG, false);

	drawWaveformCanvas(sfx, 23, 11);
	drawWaveformBar(sfx, 36, 110);
}

static void tick(Sfx* sfx)
{
	sfx->play.active = false;

	switch(sfx->tab)
	{
	case SFX_WAVEFORM_TAB: waveformTick(sfx); break;
	case SFX_ENVELOPES_TAB: envelopesTick(sfx); break;
	}

	playSound(sfx);
}

static void onStudioEnvelopeEvent(Sfx* sfx, StudioEvent event)
{
	switch(event)
	{
	case TIC_TOOLBAR_CUT: cutToClipboard(sfx); break;
	case TIC_TOOLBAR_COPY: copyToClipboard(sfx); break;
	case TIC_TOOLBAR_PASTE: copyFromClipboard(sfx); break;
	case TIC_TOOLBAR_UNDO: undo(sfx); break;
	case TIC_TOOLBAR_REDO: redo(sfx); break;
	default: break;
	}
}

static void onStudioWaveformEvent(Sfx* sfx, StudioEvent event)
{
	switch(event)
	{
	case TIC_TOOLBAR_CUT: cutWaveToClipboard(sfx); break;
	case TIC_TOOLBAR_COPY: copyWaveToClipboard(sfx); break;
	case TIC_TOOLBAR_PASTE: copyWaveFromClipboard(sfx); break;
	case TIC_TOOLBAR_UNDO: undo(sfx); break;
	case TIC_TOOLBAR_REDO: redo(sfx); break;
	default: break;
	}
}

static void onStudioEvent(Sfx* sfx, StudioEvent event)
{
	switch(sfx->tab)
	{
	case SFX_WAVEFORM_TAB: onStudioWaveformEvent(sfx, event); break;
	case SFX_ENVELOPES_TAB: onStudioEnvelopeEvent(sfx, event); break;
	default: break;
	}
}

void initSfx(Sfx* sfx, tic_mem* tic, tic_sfx* src)
{
	if(sfx->history) history_delete(sfx->history);
	
	*sfx = (Sfx)
	{
		.tic = tic,
		.tick = tick,
		.src = src,
		.index = 0,
		.play = 
		{
			.note = -1,
			.active = false,
		},
		.waveform = 
		{
			.index = 0,
		},
		.canvasTab = SFX_WAVE_TAB,
		.tab = SFX_ENVELOPES_TAB,
		.history = history_create(src, sizeof(tic_sfx)),
		.event = onStudioEvent,
	};
}