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

#define DEFAULT_CHANNEL 0

enum 
{
	SFX_WAVE_PANEL,
	SFX_VOLUME_PANEL,
	SFX_CHORD_PANEL,
	SFX_PITCH_PANEL,
};

static tic_sample* getEffect(Sfx* sfx)
{
	return sfx->src->samples.data + sfx->index;
}

static tic_waveform* getWaveformById(Sfx* sfx, s32 i)
{
	return &sfx->src->waveforms.items[i];
}

static void drawPanelBorder(tic_mem* tic, s32 x, s32 y, s32 w, s32 h, tic_color color)
{
	tic->api.rect(tic, x, y, w, h, color);

	tic->api.rect(tic, x, y-1, w, 1, tic_color_15);
	tic->api.rect(tic, x-1, y, 1, h, tic_color_15);
	tic->api.rect(tic, x, y+h, w, 1, tic_color_13);
	tic->api.rect(tic, x+w, y, 1, h, tic_color_13);
}

static void drawCanvasLeds(Sfx* sfx, s32 x, s32 y, s32 canvasTab)
{
	tic_mem* tic = sfx->tic;

	enum 
	{
		Cols = SFX_TICKS, Rows = 16, 
		Gap = 1, LedWidth = 3 + Gap, LedHeight = 1 + Gap,
		Width = LedWidth * Cols + Gap, 
		Height = LedHeight * Rows + Gap
	};

	tic->api.rect(tic, x, y, Width, Height, tic_color_15);

	for(s32 i = 0; i < Height; i += LedHeight)
		tic->api.rect(tic, x, y + i, Width, Gap, tic_color_0);

	for(s32 i = 0; i < Width; i += LedWidth)
		tic->api.rect(tic, x + i, y, Gap, Height, tic_color_0);

	{
		tic_sfx_pos pos = tic->api.sfx_pos(tic, DEFAULT_CHANNEL);
		s32 tickIndex = *(pos.data + canvasTab);

		if(tickIndex >= 0)
			tic->api.rect(tic, x + tickIndex * LedWidth, y, LedWidth + 1, Height, tic_color_12);
	}

	tic_rect rect = {x, y, Width - Gap, Height - Gap};

	tic_sample* effect = getEffect(sfx);
	tic_rect border = {-1};

	if(checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);

		s32 mx = getMouseX() - x;
		s32 my = getMouseY() - y;
		mx /= LedWidth;
		s32 vy = my /= LedHeight;
		border = (tic_rect){x + mx * LedWidth + Gap, y + my * LedHeight + Gap, LedWidth - Gap, LedHeight - Gap};

		switch(canvasTab)
		{
		case SFX_VOLUME_PANEL: vy = MAX_VOLUME - my; break;
		case SFX_CHORD_PANEL:  vy = my = Rows - my - 1; break;
		case SFX_PITCH_PANEL:  vy = my = Rows / 2 - my - 1; break;
		default: break;
		}

		{
			static const char Format[] = "[x:%02i y:%02i]";
			char buf[sizeof Format];
			sprintf(buf, Format, mx, vy);
			showTooltip(buf);
		}

		if(checkMouseDown(&rect, tic_mouse_left))
		{
			switch(canvasTab)
			{
			case SFX_VOLUME_PANEL: effect->data[mx].volume = my; break;
			case SFX_CHORD_PANEL:  effect->data[mx].chord = my; break;
			case SFX_PITCH_PANEL:  effect->data[mx].pitch = my; break;
			default: break;
			}

			history_add(sfx->history);
		}
	}

	for(s32 i = 0; i < Cols; i++)
	{
		switch(canvasTab)
		{
		case SFX_VOLUME_PANEL:
			for(s32 j = 1, start = Height - LedHeight, value = Rows - effect->data[i].volume; j <= value; j++, start -= LedHeight)
				tic->api.rect(tic, x + i * LedWidth + Gap, y + start, LedWidth-Gap, LedHeight-Gap, j == value ? tic_color_9 : tic_color_10);				
			break;

		case SFX_CHORD_PANEL:
			for(s32 j = 1, start = Height - LedHeight, value = effect->data[i].chord + 1; j <= value; j++, start -= LedHeight)
				tic->api.rect(tic, x + i * LedWidth + Gap, y + start, LedWidth-Gap, LedHeight-Gap, j == value ? tic_color_6 : tic_color_5);
			break;

		case SFX_PITCH_PANEL:
			for(s32 value = effect->data[i].pitch, j = MIN(0, value); j <= MAX(0, value); j++)
				tic->api.rect(tic, x + i * LedWidth + Gap, y + (Height / 2 - (j + 1) * LedHeight + Gap),
					LedWidth-Gap, LedHeight-Gap, j == value ? tic_color_3 : tic_color_4);
			break;
		}
	}

	{
		tic_sound_loop* loop = effect->loops + canvasTab;
		if(loop->size > 0)
			for(s32 r = 0; r < Rows; r++)
			{
				tic->api.rect(tic, x + loop->start * LedWidth + 2, y + Gap + r * LedHeight, 1, 1, tic_color_12);
				tic->api.rect(tic, x + (loop->start + loop->size-1) * LedWidth + 2, y + Gap + r * LedHeight, 1, 1, tic_color_12);	
			}
	}

	if(border.x >= 0)
		tic->api.rect_border(tic, border.x, border.y, border.w, border.h, tic_color_12);
}

static void drawVolumeStereo(Sfx* sfx, s32 x, s32 y)
{
	tic_mem* tic = sfx->tic;

	enum {Width = TIC_ALTFONT_WIDTH-1, Height = TIC_FONT_HEIGHT};

	tic_sample* effect = getEffect(sfx);

	{
		tic_rect rect = {x, y, Width, Height};

		bool hover = false;
		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);
			hover = true;

			showTooltip("left stereo");

			if(checkMouseClick(&rect, tic_mouse_left))
				effect->stereo_left = ~effect->stereo_left;
		}

		tic->api.text(tic, "L", rect.x, rect.y, effect->stereo_left ? hover ? tic_color_14 : tic_color_15 : tic_color_5, true);
	}

	{
		tic_rect rect = {x + 4, y, Width, Height};

		bool hover = false;
		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);
			hover = true;

			showTooltip("right stereo");

			if(checkMouseClick(&rect, tic_mouse_left))
				effect->stereo_right = ~effect->stereo_right;
		}

		tic->api.text(tic, "R", rect.x, rect.y, effect->stereo_right ? hover ? tic_color_14 : tic_color_15 : tic_color_5, true);
	}
}

static void drawArppeggioSwitch(Sfx* sfx, s32 x, s32 y)
{
	tic_mem* tic = sfx->tic;

	static const char Label[] = "DOWN";

	enum {Width = (sizeof Label - 1) * TIC_ALTFONT_WIDTH - 1, Height = TIC_FONT_HEIGHT};

	tic_sample* effect = getEffect(sfx);

	{
		tic_rect rect = {x, y, Width, Height};

		bool hover = false;
		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);
			hover = true;

			showTooltip("up/down arpeggio");

			if(checkMouseClick(&rect, tic_mouse_left))
				effect->reverse = ~effect->reverse;
		}

		tic->api.text(tic, Label, rect.x, rect.y, effect->reverse ? tic_color_5 : hover ? tic_color_14 : tic_color_15, true);
	}
}

static void drawPitchSwitch(Sfx* sfx, s32 x, s32 y)
{
	tic_mem* tic = sfx->tic;

	static const char Label[] = "x16";

	enum {Width = (sizeof Label - 1) * TIC_ALTFONT_WIDTH - 1, Height = TIC_FONT_HEIGHT};

	tic_sample* effect = getEffect(sfx);

	{
		tic_rect rect = {x, y, Width, Height};

		bool hover = false;
		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);
			hover = true;

			showTooltip("x16 pitch");

			if(checkMouseClick(&rect, tic_mouse_left))
				effect->pitch16x = ~effect->pitch16x;
		}

		tic->api.text(tic, Label, rect.x, rect.y, effect->pitch16x ? tic_color_5 : hover ? tic_color_14 : tic_color_15, true);
	}
}

static void drawCanvas(Sfx* sfx, s32 x, s32 y, s32 canvasTab)
{
	tic_mem* tic = sfx->tic;

	enum 
	{
		Width = 147, Height = 33
	};

	drawPanelBorder(tic, x, y, Width, Height, tic_color_0);

	static const char* Labels[] = {"", "VOLUME", "ARPEGG", "PITCH"};

	switch(canvasTab)
	{
	case SFX_VOLUME_PANEL:	drawVolumeStereo(sfx, x + 2, y + 9); break;
	case SFX_CHORD_PANEL:	drawArppeggioSwitch(sfx, x + 2, y + 9); break;
	case SFX_PITCH_PANEL:	drawPitchSwitch(sfx, x + 2, y + 9); break;
	}

	tic->api.text(tic, Labels[canvasTab], x + 2, y + 2, tic_color_15, true);
	tic->api.text(tic, "LOOP:", x + 2, y + 20, tic_color_15, true);

	static const u8 LeftArrow[] =
	{
		0b00100000,
		0b01100000,
		0b11100000,
		0b01100000,
		0b00100000,
		0b00000000,
		0b00000000,
		0b00000000,
	};

	static const u8 RightArrow[] =
	{
		0b10000000,
		0b11000000,
		0b11100000,
		0b11000000,
		0b10000000,
		0b00000000,
		0b00000000,
		0b00000000,
	};

	enum
	{
		ArrowWidth = 3, ArrowHeight = 5
	};

	tic_sample* effect = getEffect(sfx);
	static const char SetLoopPosLabel[] = "set loop start";
	static const char SetLoopSizeLabel[] = "set loop size";

	{
		tic_rect rect = {x + 2, y + 27, ArrowWidth, ArrowHeight};
		bool hover = false;

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);
			hover = true;

			showTooltip(SetLoopPosLabel);

			if(checkMouseClick(&rect, tic_mouse_left))
			{
				effect->loops[canvasTab].start--;
				history_add(sfx->history);
			}
		}

		drawBitIcon(rect.x, rect.y, LeftArrow, hover ? tic_color_14 : tic_color_15);
	}

	{
		tic_rect rect = {x + 10, y + 27, ArrowWidth, ArrowHeight};
		bool hover = false;

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);
			hover = true;

			showTooltip(SetLoopPosLabel);

			if(checkMouseClick(&rect, tic_mouse_left))
			{
				effect->loops[canvasTab].start++;
				history_add(sfx->history);
			}
		}

		drawBitIcon(rect.x, rect.y, RightArrow, hover ? tic_color_14 : tic_color_15);
	}

	{
		char buf[] = "0";
		sprintf(buf, "%X", effect->loops[canvasTab].start);
		tic->api.text(tic, buf, x + 6, y + 27, tic_color_14, true);
	}

	{
		tic_rect rect = {x + 14, y + 27, ArrowWidth, ArrowHeight};
		bool hover = false;

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);
			hover = true;
			showTooltip(SetLoopSizeLabel);

			if(checkMouseClick(&rect, tic_mouse_left))
			{
				effect->loops[canvasTab].size--;
				history_add(sfx->history);
			}
		}

		drawBitIcon(rect.x, rect.y, LeftArrow, hover ? tic_color_14 : tic_color_15);
	}

	{
		tic_rect rect = {x + 22, y + 27, ArrowWidth, ArrowHeight};
		bool hover = false;

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);
			hover = true;
			showTooltip(SetLoopSizeLabel);

			if(checkMouseClick(&rect, tic_mouse_left))
			{
				effect->loops[canvasTab].size++;
				history_add(sfx->history);
			}
		}

		drawBitIcon(rect.x, rect.y, RightArrow, hover ? tic_color_14 : tic_color_15);
	}

	{
		char buf[] = "0";
		sprintf(buf, "%X", effect->loops[canvasTab].size);
		tic->api.text(tic, buf, x + 18, y + 27, tic_color_14, true);
	}

	drawCanvasLeds(sfx, x + 26, y, canvasTab);
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

static void resetSfx(Sfx* sfx)
{
	tic_sample* effect = getEffect(sfx);
	memset(effect, 0, sizeof(tic_sample));

	history_add(sfx->history);
}

static void cutToClipboard(Sfx* sfx)
{
	copyToClipboard(sfx);
	resetSfx(sfx);
}

static void copyFromClipboard(Sfx* sfx)
{
	tic_sample* effect = getEffect(sfx);

	if(fromClipboard(effect, sizeof(tic_sample), true, false))
		history_add(sfx->history);
}

static void processKeyboard(Sfx* sfx)
{
	tic_mem* tic = sfx->tic;

	if(tic->ram.input.keyboard.data == 0) return;

	bool ctrl = tic->api.key(tic, tic_key_ctrl);

	s32 keyboardButton = -1;

	static const s32 Keycodes[] = 
	{
		tic_key_z,
		tic_key_s,
		tic_key_x,
		tic_key_d,
		tic_key_c,
		tic_key_v,
		tic_key_g,
		tic_key_b,
		tic_key_h,
		tic_key_n,
		tic_key_j,
		tic_key_m,
	};

	if(ctrl)
	{

	}
	else
	{
		for(int i = 0; i < COUNT_OF(Keycodes); i++)
			if(tic->api.key(tic, Keycodes[i]))
				keyboardButton = i;        
	}

	tic_sample* effect = getEffect(sfx);

	if(keyboardButton >= 0)
	{
		effect->note = keyboardButton;
		sfx->play.active = true;
	}

	if(tic->api.key(tic, tic_key_space))
		sfx->play.active = true;
}

static void processEnvelopesKeyboard(Sfx* sfx)
{
	tic_mem* tic = sfx->tic;
	bool ctrl = tic->api.key(tic, tic_key_ctrl);

	switch(getClipboardEvent())
	{
	case TIC_CLIPBOARD_CUT: cutToClipboard(sfx); break;
	case TIC_CLIPBOARD_COPY: copyToClipboard(sfx); break;
	case TIC_CLIPBOARD_PASTE: copyFromClipboard(sfx); break;
	default: break;
	}

	if(ctrl)
	{
		if(keyWasPressed(tic_key_z))        undo(sfx);
		else if(keyWasPressed(tic_key_y))   redo(sfx);
	}

	else if(keyWasPressed(tic_key_left))    sfx->index--;
	else if(keyWasPressed(tic_key_right))   sfx->index++;
	else if(keyWasPressed(tic_key_delete))  resetSfx(sfx);
}

static void drawWaves(Sfx* sfx, s32 x, s32 y)
{
	tic_mem* tic = sfx->tic;

	enum{Width = 10, Height = 6, MarginRight = 6, MarginBottom = 4, Cols = 4, Rows = 4, Scale = 4};

	for(s32 i = 0; i < WAVES_COUNT; i++)
	{
		s32 xi = i % Cols;
		s32 yi = i / Cols;

		tic_rect rect = {x + xi * (Width + MarginRight), y + yi * (Height + MarginBottom), Width, Height};
		tic_sample* effect = getEffect(sfx);
		bool hover = false;

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);

			hover = true;

			{
				static const char Format[] = "select wave #%02i";
				char buf[sizeof Format];
				sprintf(buf, Format, i);
				showTooltip(buf);
			}

			if(checkMouseClick(&rect, tic_mouse_left))
			{
				for(s32 c = 0; c < SFX_TICKS; c++)
					effect->data[c].wave = i;

				history_add(sfx->history);
			}
		}

		bool sel = i == effect->data->wave;

		tic_sfx_pos pos = tic->api.sfx_pos(tic, DEFAULT_CHANNEL);
		bool active = *pos.data < 0 ? false : i == effect->data[*pos.data].wave;

		drawPanelBorder(tic, rect.x, rect.y, rect.w, rect.h, active ? tic_color_3 : sel ? tic_color_5 : tic_color_0);

		// draw tiny wave previews
		{
			tic_waveform* wave = getWaveformById(sfx, i);

			for(s32 i = 0; i < WAVE_VALUES/Scale; i++)
			{
				s32 value = tic_tool_peek4(wave->data, i*Scale)/Scale;
				tic->api.pixel(tic, rect.x + i+1, rect.y + Height - value - 2, active ? tic_color_2 : sel ? tic_color_7 : hover ? tic_color_13 : tic_color_12);
			}

			// draw flare
			if(sel || active)
			{
				tic->api.rect(tic, rect.x + rect.w - 2, rect.y, 2, 1, tic_color_12);
				tic->api.pixel(tic, rect.x + rect.w - 1, rect.y + 1, tic_color_12);
			}
		}

	}
}

static void drawWavePanel(Sfx* sfx, s32 x, s32 y)
{
	tic_mem* tic = sfx->tic;

	enum {Width = 73, Height = 83, Round = 2};

	typedef struct {s32 x; s32 y; s32 x1; s32 y1; tic_color color;} Edge; 
	static const Edge Edges[] = 
	{
		{Width, Round, Width, Height - Round, tic_color_15},
		{Round, Height, Width - Round, Height, tic_color_15},
		{Width - Round, Height, Width, Height - Round, tic_color_15},
		{Width - Round, 0, Width, Round, tic_color_15},
		{0, Height - Round, Round, Height, tic_color_15},
		{Round, 0, Width - Round, 0, tic_color_13},
		{0, Round, 0, Height - Round, tic_color_13},
		{0, Round, Round, 0, tic_color_12},
	};

	for(const Edge* edge = Edges; edge < Edges + COUNT_OF(Edges); edge++)
		tic->api.line(tic, x + edge->x, y + edge->y, x + edge->x1, y + edge->y1, edge->color);

	// draw current wave shape
	{
		enum {Scale = 2, MaxValue = (1 << WAVE_VALUE_BITS) - 1};

		tic_rect rect = {x + 5, y + 5, 64, 32};
		tic_sample* effect = getEffect(sfx);
		tic_waveform* wave = getWaveformById(sfx, effect->data->wave);

		drawPanelBorder(tic, rect.x - 1, rect.y - 1, rect.w + 2, rect.h + 2, tic_color_5);

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);

			s32 cx = (getMouseX() - rect.x) / Scale;
			s32 cy = MaxValue - (getMouseY() - rect.y) / Scale;

			{
				static const char Format[] = "[x=%02i y=%02i]";
				char buf[sizeof Format];
				sprintf(buf, Format, cx, cy);
				showTooltip(buf);
			}

			enum {Border = 1};
			tic->api.rect_border(tic, rect.x + cx*Scale - Border, 
				rect.y + (MaxValue - cy) * Scale - Border, Scale + Border*2, Scale + Border*2, tic_color_7);

			if(checkMouseDown(&rect, tic_mouse_left))
			{
				if(tic_tool_peek4(wave->data, cx) != cy)
				{
					tic_tool_poke4(wave->data, cx, cy);
					history_add(sfx->history);
				}
			}
		}		

		for(s32 i = 0; i < WAVE_VALUES; i++)
		{
			s32 value = tic_tool_peek4(wave->data, i);
			tic->api.rect(tic, rect.x + i*Scale, rect.y + (MaxValue - value) * Scale, Scale, Scale, tic_color_7);
		}

		// draw flare
		{
			tic->api.rect(tic, rect.x + 59, rect.y + 2, 4, 1, tic_color_12);
			tic->api.rect(tic, rect.x + 62, rect.y + 2, 1, 3, tic_color_12);
		}
	}

	drawWaves(sfx, x + 8, y + 43);
}

static void drawPianoOctave(Sfx* sfx, s32 x, s32 y, s32 octave)
{
	tic_mem* tic = sfx->tic;

	enum 
	{
		Gap = 1, WhiteShadow = 1,
		WhiteWidth = 3, WhiteHeight = 8, WhiteCount = 7, WhiteWidthGap = WhiteWidth + Gap,
		BlackWidth = 3, BlackHeight = 4, BlackCount = 6, 
		BlackOffset = WhiteWidth - (BlackWidth - Gap) / 2,
		Width = WhiteCount * WhiteWidthGap - Gap,
		Height = WhiteHeight
	};

	tic_rect rect = {x, y, Width, Height};

	typedef struct{s32 note; tic_rect rect; bool white;} PianoBtn;
	static const PianoBtn Buttons[] = 
	{
		{0, WhiteWidthGap * 0, 0, WhiteWidth, WhiteHeight, true},
		{2, WhiteWidthGap * 1, 0, WhiteWidth, WhiteHeight, true},
		{4, WhiteWidthGap * 2, 0, WhiteWidth, WhiteHeight, true},
		{5, WhiteWidthGap * 3, 0, WhiteWidth, WhiteHeight, true},
		{7, WhiteWidthGap * 4, 0, WhiteWidth, WhiteHeight, true},
		{9, WhiteWidthGap * 5, 0, WhiteWidth, WhiteHeight, true},
		{11, WhiteWidthGap * 6, 0, WhiteWidth, WhiteHeight, true},

		{1, WhiteWidthGap * 0 + BlackOffset, 0, BlackWidth, BlackHeight, false},
		{3, WhiteWidthGap * 1 + BlackOffset, 0, BlackWidth, BlackHeight, false},
		{6, WhiteWidthGap * 3 + BlackOffset, 0, BlackWidth, BlackHeight, false},
		{8, WhiteWidthGap * 4 + BlackOffset, 0, BlackWidth, BlackHeight, false},
		{10, WhiteWidthGap * 5 + BlackOffset, 0, BlackWidth, BlackHeight, false},
	};

	tic_sample* effect = getEffect(sfx);

	s32 hover = -1;

	if(checkMousePos(&rect))
	{
		for(s32 i = COUNT_OF(Buttons)-1; i >= 0; i--)
		{
			const PianoBtn* btn = Buttons + i;
			tic_rect btnRect = btn->rect;
			btnRect.x += x;
			btnRect.y += y;

			if(checkMousePos(&btnRect))
			{
				setCursor(tic_cursor_hand);

				hover = btn->note;

				{
					static const char* Notes[] = SFX_NOTES;
					static const char Format[] = "play %s%i note";
					char buf[sizeof Format];
					sprintf(buf, Format, Notes[btn->note], octave + 1);
					showTooltip(buf);
				}

				if(checkMouseDown(&rect, tic_mouse_left))
				{
					effect->note = btn->note;
					effect->octave = octave;
					sfx->play.active = true;

					history_add(sfx->history);
				}

				break;
			}
		}
	}

	bool active = sfx->play.active && effect->octave == octave;

	tic->api.rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_15);

	for(s32 i = 0; i < COUNT_OF(Buttons); i++)
	{
		const PianoBtn* btn = Buttons + i;
		const tic_rect* rect = &btn->rect;
		tic->api.rect(tic, x + rect->x, y + rect->y, rect->w, rect->h, 
			active && effect->note == btn->note ? tic_color_2 : 
				btn->white 
					? hover == btn->note ? tic_color_13 : tic_color_12 
					: hover == btn->note ? tic_color_15 : tic_color_0);

		if(btn->white)
			tic->api.rect(tic, x + rect->x, y + (WhiteHeight - WhiteShadow), WhiteWidth, WhiteShadow, tic_color_0);

		// draw current note marker
		if(effect->octave == octave && effect->note == btn->note)
			tic->api.rect(tic, x + rect->x + 1, y + rect->y + rect->h - 3, 1, 1, tic_color_2);
	}
}

static void drawPiano(Sfx* sfx, s32 x, s32 y)
{
	tic_mem* tic = sfx->tic;

	enum {Width = 29};

	for(s32 i = 0; i < OCTAVES; i++)
	{
		drawPianoOctave(sfx, x + Width*i, y, i);
	}
}

static void drawSpeedPanel(Sfx* sfx, s32 x, s32 y)
{
	tic_mem* tic = sfx->tic;

	enum 
	{
		Count = 8, Gap = 1, ColWidth = 1, ColWidthGap = ColWidth + Gap, 
		Width = Count * ColWidthGap - Gap, Height = 5,
		MaxSpeed = (1 << SFX_SPEED_BITS) / 2
	};

	tic_rect rect = {x + 13, y, Width, Height};
	tic_sample* effect = getEffect(sfx);
	s32 hover = -1;

	if(checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);

		s32 spd = (getMouseX() - rect.x) / ColWidthGap;
		hover = spd;

		{
			static const char Format[] = "set speed to %i";
			char buf[sizeof Format];
			sprintf(buf, Format, spd);
			showTooltip(buf);
		}

		if(checkMouseDown(&rect, tic_mouse_left))
		{
			effect->speed = spd - MaxSpeed;
			history_add(sfx->history);
		}
	}

	tic->api.text(tic, "SPD", x, y, tic_color_15, true);

	for(s32 i = 0; i < Count; i++)
		tic->api.rect(tic, rect.x + i * ColWidthGap, rect.y, ColWidth, rect.h, i - MaxSpeed <= effect->speed ? tic_color_5 : hover == i ? tic_color_14 : tic_color_15);
}

static void drawSelectorPanel(Sfx* sfx, s32 x, s32 y)
{
	tic_mem* tic = sfx->tic;

	enum
	{
		Size = 3, Gap = 1, SizeGap = Size + Gap, 
		GroupGap = 2, Groups = 4, Cols = 4, Rows = SFX_COUNT / (Cols * Groups),
		GroupWidth = Cols * SizeGap - Gap,
		Width = (GroupWidth + GroupGap) * Groups - GroupGap, Height = Rows * SizeGap - Gap
	};

	tic_rect rect = {x, y, Width, Height};
	s32 hover = -1;

	if(checkMousePos(&rect))
		for(s32 g = 0, i = 0; g < Groups; g++)
			for(s32 r = 0; r < Rows; r++)
				for(s32 c = 0; c < Cols; c++, i++)
				{
					tic_rect rect = {x + c * SizeGap + g * (GroupWidth + GroupGap), y + r * SizeGap, SizeGap, SizeGap};

					if(checkMousePos(&rect))
					{
						setCursor(tic_cursor_hand);
						hover = i;

						{
							static const char Format[] = "edit sfx #%02i";
							char buf[sizeof Format];
							sprintf(buf, Format, hover);
							showTooltip(buf);
						}

						if(checkMouseClick(&rect, tic_mouse_left))
							sfx->index = i;

						goto draw;
					}
				}
draw:

	for(s32 g = 0, i = 0; g < Groups; g++)
		for(s32 r = 0; r < Rows; r++)
			for(s32 c = 0; c < Cols; c++, i++)
			{
				static const u8 EmptyEffect[sizeof(tic_sample)] = {0};
				bool empty = memcmp(sfx->src->samples.data + i, EmptyEffect, sizeof EmptyEffect) == 0;

				tic->api.rect(tic, x + c * SizeGap + g * (GroupWidth + GroupGap), y + r * SizeGap, Size, Size, 
					sfx->index == i ? tic_color_5 : hover == i ? tic_color_14 : empty ? tic_color_15 : tic_color_13);
			}
}

static void drawSelector(Sfx* sfx, s32 x, s32 y)
{
	tic_mem* tic = sfx->tic;

	enum {Width = 70, Height = 25};

	drawPanelBorder(tic, x, y, Width, Height, tic_color_0);

	{
		char buf[] = "00";
		sprintf(buf, "%02i", sfx->index);
		tic->api.text(tic, buf, x + 20, y + 2, tic_color_5, true);
		tic->api.text(tic, "IDX", x + 6, y + 2, tic_color_15, true);
	}

	drawSpeedPanel(sfx, x + 40, y + 2);
	drawSelectorPanel(sfx, x + 2, y + 9);
}

static void tick(Sfx* sfx)
{
	tic_mem* tic = sfx->tic;

	sfx->play.active = false;

	processKeyboard(sfx);
	processEnvelopesKeyboard(sfx);

	tic->api.clear(tic, tic_color_14);

	drawSelector(sfx, 9, 12);
	drawWavePanel(sfx, 7, 41);

	drawCanvas(sfx, 88, 12, SFX_VOLUME_PANEL);
	drawCanvas(sfx, 88, 51, SFX_CHORD_PANEL);
	drawCanvas(sfx, 88, 90, SFX_PITCH_PANEL);

	drawPiano(sfx, 5, 127);

	drawToolbar(tic, true);

	playSound(sfx);
}

static void onStudioEvent(Sfx* sfx, StudioEvent event)
{
	switch(event)
	{
	case TIC_TOOLBAR_CUT: 	cutToClipboard(sfx); break;
	case TIC_TOOLBAR_COPY: 	copyToClipboard(sfx); break;
	case TIC_TOOLBAR_PASTE: copyFromClipboard(sfx); break;
	case TIC_TOOLBAR_UNDO: 	undo(sfx); break;
	case TIC_TOOLBAR_REDO: 	redo(sfx); break;
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

		.history = history_create(src, sizeof(tic_sfx)),
		.event = onStudioEvent,
	};
}
