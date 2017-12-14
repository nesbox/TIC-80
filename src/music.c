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

#include "music.h"
#include "history.h"

#define TRACKER_ROWS (MUSIC_PATTERN_ROWS / 4)
#define CHANNEL_COLS 8
#define TRACKER_COLS (TIC_SOUND_CHANNELS * CHANNEL_COLS)

enum
{
	ColumnNote = 0,
	ColumnSemitone,
	ColumnOctave,
	ColumnSfxHi,
	ColumnSfxLow,
	ColumnVolume,
	ColumnEffect,
	ColumnParameter,
};

static void undo(Music* music)
{
	history_undo(music->history);
}

static void redo(Music* music)
{
	history_redo(music->history);
}

const tic_music_pos* getMusicPos(Music* music)
{
	return &music->tic->ram.music_pos;
}

static void drawEditbox(Music* music, s32 x, s32 y, s32 value, void(*set)(Music*, s32, s32 channel), s32 channel)
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

	{
		SDL_Rect rect = { x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT };

		bool over = false;
		if (checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);
			over = true;

			if (checkMouseClick(&rect, SDL_BUTTON_LEFT))
				set(music, -1, channel);
		}

		drawBitIcon(rect.x, rect.y, LeftArrow, (over ? tic_color_light_blue : tic_color_dark_gray));
	}

	{
		x += TIC_FONT_WIDTH;

		SDL_Rect rect = { x-1, y-1, TIC_FONT_WIDTH*2+1, TIC_FONT_HEIGHT+1 };

		if (checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);

			if (checkMouseClick(&rect, SDL_BUTTON_LEFT))
			{
				music->tracker.row = -1;
				music->tracker.col = channel * CHANNEL_COLS;

				s32 mx = getMouseX() - rect.x;
				music->tracker.patternCol = mx / TIC_FONT_WIDTH;
			}
		}

		music->tic->api.rect(music->tic, rect.x, rect.y, rect.w, rect.h, (tic_color_black));

		if(music->tracker.row == -1 && music->tracker.col / CHANNEL_COLS == channel)
		{
			music->tic->api.rect(music->tic, x - 1 + music->tracker.patternCol * TIC_FONT_WIDTH, y - 1, TIC_FONT_WIDTH + 1, TIC_FONT_HEIGHT + 1, (tic_color_red));
		}

		char val[] = "99";
		sprintf(val, "%02i", value);
		music->tic->api.fixed_text(music->tic, val, x, y, (tic_color_white));
	}

	{
		x += 2*TIC_FONT_WIDTH;

		SDL_Rect rect = { x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT };

		bool over = false;
		if (checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);
			over = true;

			if (checkMouseClick(&rect, SDL_BUTTON_LEFT))
				set(music, +1, channel);
		}

		drawBitIcon(rect.x, rect.y, RightArrow, (over ? tic_color_light_blue : tic_color_dark_gray));
	}
}

static void drawSwitch(Music* music, s32 x, s32 y, const char* label, s32 value, void(*set)(Music*, s32, void* data), void* data)
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

	music->tic->api.text(music->tic, label, x, y, (tic_color_white));

	{
		x += (s32)strlen(label)*TIC_FONT_WIDTH;

		SDL_Rect rect = { x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT };

		if (checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);

			if (checkMouseClick(&rect, SDL_BUTTON_LEFT))
				set(music, -1, data);
		}

		drawBitIcon(rect.x, rect.y, LeftArrow, (tic_color_dark_gray));
	}

	{
		char val[] = "999";
		sprintf(val, "%02i", value);
		music->tic->api.fixed_text(music->tic, val, x += TIC_FONT_WIDTH, y, (tic_color_white));
	}

	{
		x += (value > 99 ? 3 : 2)*TIC_FONT_WIDTH;

		SDL_Rect rect = { x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT };

		if (checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);

			if (checkMouseClick(&rect, SDL_BUTTON_LEFT))
				set(music, +1, data);
		}

		drawBitIcon(rect.x, rect.y, RightArrow, (tic_color_dark_gray));
	}
}

static tic_track* getTrack(Music* music)
{
	return &music->src->tracks.data[music->track];
}

static s32 getRows(Music* music)
{
	tic_track* track = getTrack(music);

	return MUSIC_PATTERN_ROWS - track->rows;
}

static void updateScroll(Music* music)
{
	s32 rows = getRows(music);

	if (music->tracker.scroll < 0)
		music->tracker.scroll = 0;

	if (music->tracker.scroll > rows - TRACKER_ROWS)
		music->tracker.scroll = rows - TRACKER_ROWS;
}

static void updateTracker(Music* music)
{
	s32 row = music->tracker.row;

	if (row < music->tracker.scroll) music->tracker.scroll = row;
	else if (row >= music->tracker.scroll + TRACKER_ROWS)
		music->tracker.scroll = row - TRACKER_ROWS + 1;

	{
		s32 rows = getRows(music);
		if (music->tracker.row >= rows) music->tracker.row = rows - 1;
	}

	updateScroll(music);
}

static void upRow(Music* music)
{
	if (music->tracker.row > -1)
	{
		music->tracker.row--;
		updateTracker(music);
	}
}

static void downRow(Music* music)
{
	const tic_music_pos* pos = getMusicPos(music);
	if(pos->track == music->track && music->tracker.follow) return;

	if (music->tracker.row < getRows(music) - 1)
	{
		music->tracker.row++;
		updateTracker(music);
	}
}

static void leftCol(Music* music)
{
	if (music->tracker.col > 0)
	{
		music->tracker.col--;
		updateTracker(music);
	}
}

static void rightCol(Music* music)
{
	if (music->tracker.col < TRACKER_COLS - 1)
	{
		music->tracker.col++;
		updateTracker(music);
	}
}

static void goHome(Music* music)
{
	music->tracker.col -= music->tracker.col % CHANNEL_COLS;
}

static void goEnd(Music* music)
{
	music->tracker.col -= music->tracker.col % CHANNEL_COLS;
	music->tracker.col += CHANNEL_COLS-1;
}

static void pageUp(Music* music)
{
	music->tracker.row -= TRACKER_ROWS;
	if(music->tracker.row < 0) 
		music->tracker.row = 0;

	updateTracker(music);
}

static void pageDown(Music* music)
{
	if (music->tracker.row < getRows(music) - 1)

	music->tracker.row += TRACKER_ROWS;
	s32 rows = getRows(music);

	if(music->tracker.row >= rows) 
		music->tracker.row = rows-1;
	
	updateTracker(music);
}

static void doTab(Music* music)
{
	s32 channel = (music->tracker.col / CHANNEL_COLS + 1) % TIC_SOUND_CHANNELS;

	music->tracker.col = channel * CHANNEL_COLS + music->tracker.col % CHANNEL_COLS;

	updateTracker(music);
}

static void upFrame(Music* music)
{
	music->tracker.frame--;

	if(music->tracker.frame < 0)
		music->tracker.frame = 0;
}

static void downFrame(Music* music)
{
	music->tracker.frame++;

	if(music->tracker.frame >= MUSIC_FRAMES)
		music->tracker.frame = MUSIC_FRAMES-1;
}

static bool checkPlayFrame(Music* music, s32 frame)
{
	const tic_music_pos* pos = getMusicPos(music);

	return pos->track == music->track &&
		pos->frame == frame;
}

static bool checkPlayRow(Music* music, s32 row)
{
	const tic_music_pos* pos = getMusicPos(music);

	return checkPlayFrame(music, music->tracker.frame) && pos->row == row;
}

static tic_track_pattern* getPattern(Music* music, s32 channel)
{
	s32 patternId = tic_tool_get_pattern_id(getTrack(music), music->tracker.frame, channel);

	return patternId ? &music->src->patterns.data[patternId - PATTERN_START] : NULL;
}

static tic_track_pattern* getChannelPattern(Music* music)
{
	s32 channel = music->tracker.col / CHANNEL_COLS;

	return getPattern(music, channel);
}

static s32 getNote(Music* music)
{
	tic_track_pattern* pattern = getChannelPattern(music);

	return pattern->rows[music->tracker.row].note - NoteStart;
}

static s32 getOctave(Music* music)
{
	tic_track_pattern* pattern = getChannelPattern(music);

	return pattern->rows[music->tracker.row].octave;
}

static s32 getVolume(Music* music)
{
	tic_track_pattern* pattern = getChannelPattern(music);

	return pattern->rows[music->tracker.row].volume;
}

static s32 getSfxId(const tic_track_pattern* pattern, s32 row)
{
	return (pattern->rows[row].sfxhi << MUSIC_SFXID_LOW_BITS) | pattern->rows[row].sfxlow;
}

static void setSfxId(tic_track_pattern* pattern, s32 row, s32 sfx)
{
	if(sfx >= SFX_COUNT) sfx = SFX_COUNT-1;

	pattern->rows[row].sfxhi = (sfx & 0b00100000) >> MUSIC_SFXID_LOW_BITS;
	pattern->rows[row].sfxlow = sfx & 0b00011111;
}

static s32 getSfx(Music* music)
{
	tic_track_pattern* pattern = getChannelPattern(music);

	return getSfxId(pattern, music->tracker.row);
}

static void setSfx(Music* music, s32 sfx)
{
	tic_track_pattern* pattern = getChannelPattern(music);

	setSfxId(pattern, music->tracker.row, sfx);

	music->tracker.last.sfx = getSfxId(pattern, music->tracker.row);
}

static void playNote(Music* music)
{

	if (getChannelPattern(music))
	{
		s32 note = getNote(music);

		if (note >= 0 && music->tracker.note != note)
		{
			music->tracker.note = note;

			s32 channel = music->tracker.col / CHANNEL_COLS;
			music->tic->api.sfx_stop(music->tic, channel);
			music->tic->api.sfx_ex(music->tic, getSfx(music), note, getOctave(music), -1, channel, MAX_VOLUME - getVolume(music), 0);
		}
	}
}

static void setStopNote(Music* music)
{
	tic_track_pattern* pattern = getChannelPattern(music);

	pattern->rows[music->tracker.row].note = NoteStop;
	pattern->rows[music->tracker.row].octave = 0;
}

static void setNote(Music* music, s32 note, s32 octave, s32 volume, s32 sfx)
{
	tic_track_pattern* pattern = getChannelPattern(music);

	pattern->rows[music->tracker.row].note = note + NoteStart;
	pattern->rows[music->tracker.row].octave = octave;
	pattern->rows[music->tracker.row].volume = volume;
	setSfxId(pattern, music->tracker.row, sfx);

	playNote(music);
}

static void setOctave(Music* music, s32 octave)
{
	tic_track_pattern* pattern = getChannelPattern(music);

	pattern->rows[music->tracker.row].octave = octave;

	music->tracker.last.octave = octave;

	playNote(music);
}

static void setVolume(Music* music, s32 volume)
{
	tic_track_pattern* pattern = getChannelPattern(music);

	pattern->rows[music->tracker.row].volume = volume;

	music->tracker.last.volume = volume;

	playNote(music);

}

static void playFrameRow(Music* music)
{
	music->tic->api.music_frame(music->tic, music->track, music->tracker.frame, music->tracker.row, true);
}

static void playFrame(Music* music)
{
	music->tic->api.music_frame(music->tic, music->track, music->tracker.frame, -1, true);
}

static void playTrack(Music* music)
{
	music->tic->api.music(music->tic, music->track, -1, -1, true);
}

static void stopTrack(Music* music)
{
	music->tic->api.music(music->tic, -1, -1, -1, false);
}

static void resetSelection(Music* music)
{
	music->tracker.select.start = (SDL_Point){-1, -1};
	music->tracker.select.rect = (SDL_Rect){0, 0, 0, 0};
}

static void deleteSelection(Music* music)
{
	tic_track_pattern* pattern = getChannelPattern(music);

	if(pattern)
	{
		SDL_Rect rect = music->tracker.select.rect;

		if(rect.h <= 0)
		{
			rect.y = music->tracker.row;
			rect.h = 1;
		}

		enum{RowSize = sizeof(tic_track_pattern) / MUSIC_PATTERN_ROWS};
		SDL_memset(&pattern->rows[rect.y], 0, RowSize * rect.h);
	}
}

typedef struct
{
	u8 size;
} ClipboardHeader;

static void copyToClipboard(Music* music, bool cut)
{
	tic_track_pattern* pattern = getChannelPattern(music);

	if(pattern)
	{
		SDL_Rect rect = music->tracker.select.rect;

		if(rect.h <= 0)
		{
			rect.y = music->tracker.row;
			rect.h = 1;
		}

		ClipboardHeader header = {rect.h};

		enum{RowSize = sizeof(tic_track_pattern) / MUSIC_PATTERN_ROWS, HeaderSize = sizeof(ClipboardHeader)};

		s32 size = rect.h * RowSize + HeaderSize;
		u8* data = SDL_malloc(size);

		if(data)
		{
			SDL_memcpy(data, &header, HeaderSize);
			SDL_memcpy(data + HeaderSize, &pattern->rows[rect.y], RowSize * rect.h);

			toClipboard(data, size, true);

			SDL_free(data);

			if(cut)
			{
				deleteSelection(music);
				history_add(music->history);
			}

			resetSelection(music);
		}		
	}
}

static void copyFromClipboard(Music* music)
{
	tic_track_pattern* pattern = getChannelPattern(music);

	if(pattern && SDL_HasClipboardText())
	{
		char* clipboard = SDL_GetClipboardText();

		if(clipboard)
		{
			s32 size = strlen(clipboard)/2;

			enum{RowSize = sizeof(tic_track_pattern) / MUSIC_PATTERN_ROWS, HeaderSize = sizeof(ClipboardHeader)};

			if(size > HeaderSize)
			{
				u8* data = SDL_malloc(size);

				str2buf(clipboard, strlen(clipboard), data, true);

				ClipboardHeader header = {0};

				SDL_memcpy(&header, data, HeaderSize);

				if(header.size * RowSize == size - HeaderSize)
				{
					if(header.size + music->tracker.row > MUSIC_PATTERN_ROWS)
						header.size = MUSIC_PATTERN_ROWS - music->tracker.row;

					SDL_memcpy(&pattern->rows[music->tracker.row], data + HeaderSize, header.size * RowSize);
					history_add(music->history);
				}

				SDL_free(data);
			}

			SDL_free(clipboard);
		}
	}
}

static void setChannelPatternValue(Music* music, s32 patternId, s32 channel)
{
	tic_track* track = getTrack(music);
	s32 frame = music->tracker.frame;

	u32 patternData = 0;
	for(s32 b = 0; b < TRACK_PATTERNS_SIZE; b++)
		patternData |= track->data[frame * TRACK_PATTERNS_SIZE + b] << (BITS_IN_BYTE * b);

	s32 shift = channel * TRACK_PATTERN_BITS;

	if(patternId < 0) patternId = MUSIC_PATTERNS;
	if(patternId > MUSIC_PATTERNS) patternId = 0;

	patternData &= ~(TRACK_PATTERN_MASK << shift);
	patternData |= patternId << shift;

	for(s32 b = 0; b < TRACK_PATTERNS_SIZE; b++)
		track->data[frame * TRACK_PATTERNS_SIZE + b] = (patternData >> (b * BITS_IN_BYTE)) & 0xff;

	history_add(music->history);
}

static void prevPattern(Music* music)
{
	s32 channel = music->tracker.col / CHANNEL_COLS;

	if (channel > 0)
	{
		music->tracker.col = (channel-1) * CHANNEL_COLS;
		music->tracker.patternCol = 1;
	}
}

static void nextPattern(Music* music)
{
	s32 channel = music->tracker.col / CHANNEL_COLS;

	if (channel < TIC_SOUND_CHANNELS-1)
	{
		music->tracker.col = (channel+1) * CHANNEL_COLS;
		music->tracker.patternCol = 0;
	}
}

static void patternColLeft(Music* music)
{
	if(music->tracker.patternCol > 0)
		music->tracker.patternCol--;
	else prevPattern(music);
}

static void patternColRight(Music* music)
{
	if(music->tracker.patternCol < 1)
		music->tracker.patternCol++;
	else nextPattern(music);
}

static void checkSelection(Music* music)
{
	if(music->tracker.select.start.x < 0 || music->tracker.select.start.y < 0)
	{
		music->tracker.select.start.x = music->tracker.col;
		music->tracker.select.start.y = music->tracker.row;
	}
}

static void updateSelection(Music* music)
{
	s32 rl = SDL_min(music->tracker.col, music->tracker.select.start.x);
	s32 rt = SDL_min(music->tracker.row, music->tracker.select.start.y);
	s32 rr = SDL_max(music->tracker.col, music->tracker.select.start.x);
	s32 rb = SDL_max(music->tracker.row, music->tracker.select.start.y);

	SDL_Rect* rect = &music->tracker.select.rect;
	*rect = (SDL_Rect){rl, rt, rr - rl + 1, rb - rt + 1};

	if(rect->x % CHANNEL_COLS + rect->w > CHANNEL_COLS)
		resetSelection(music);
}

static void processTrackerKeydown(Music* music, SDL_Keysym* keysum)
{
	SDL_Keycode keycode = keysum->sym;
	SDL_Scancode scancode = keysum->scancode;

	bool shift = SDL_GetModState() & KMOD_SHIFT;

	if(shift)
	{
		switch (keycode)
		{
		case SDLK_UP:
		case SDLK_DOWN:
		case SDLK_LEFT:
		case SDLK_RIGHT:
		case SDLK_HOME:
		case SDLK_END:
		case SDLK_PAGEUP:
		case SDLK_PAGEDOWN:
		case SDLK_TAB:
			checkSelection(music);
		}
	}

	switch (keycode)
	{
	case SDLK_UP: 			upRow(music); break;
	case SDLK_DOWN:			downRow(music); break;
	case SDLK_LEFT: 		leftCol(music); break;
	case SDLK_RIGHT:		rightCol(music); break;
	case SDLK_HOME: 		goHome(music); break;
	case SDLK_END: 			goEnd(music); break;
	case SDLK_PAGEUP: 		pageUp(music); break;
	case SDLK_PAGEDOWN: 	pageDown(music); break;
	case SDLK_TAB: 			doTab(music); break;
	case SDLK_DELETE:
		deleteSelection(music);
		history_add(music->history);
		downRow(music);
		break;
	case SDLK_SPACE:	playNote(music); break;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		{
		const tic_music_pos* pos = getMusicPos(music);
		pos->track < 0
			? (shift ? playFrameRow(music) : playFrame(music))
			: stopTrack(music);        
		}
		break;
	}

	if(shift)
	{
		switch (keycode)
		{
		case SDLK_UP:
		case SDLK_DOWN:
		case SDLK_LEFT:
		case SDLK_RIGHT:
		case SDLK_HOME:
		case SDLK_END:
		case SDLK_PAGEUP:
		case SDLK_PAGEDOWN:
		case SDLK_TAB:
			updateSelection(music);
		}
	}
	else resetSelection(music);

	static const SDL_Scancode Piano[] =
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

		// octave +1
		SDL_SCANCODE_Q,
		SDL_SCANCODE_2,
		SDL_SCANCODE_W,
		SDL_SCANCODE_3,
		SDL_SCANCODE_E,
		SDL_SCANCODE_R,
		SDL_SCANCODE_5,
		SDL_SCANCODE_T,
		SDL_SCANCODE_6,
		SDL_SCANCODE_Y,
		SDL_SCANCODE_7,
		SDL_SCANCODE_U,
	};

	if (getChannelPattern(music))
	{
		s32 col = music->tracker.col % CHANNEL_COLS;

		switch (col)
		{
		case ColumnNote:
		case ColumnSemitone:
			if (scancode == SDL_SCANCODE_1 || scancode == SDL_SCANCODE_A)
			{
				setStopNote(music);
				downRow(music);
			}
			else
			{
				tic_track_pattern* pattern = getChannelPattern(music);

				for (s32 i = 0; i < COUNT_OF(Piano); i++)
				{
					if (scancode == Piano[i])
					{
						s32 note = i % NOTES;

						if(pattern->rows[music->tracker.row].note > NoteNone)
						{
							pattern->rows[music->tracker.row].note = note + NoteStart;
							playNote(music);
						}
						else
						{
							s32 octave = i / NOTES + music->tracker.last.octave;
							s32 volume = music->tracker.last.volume;
							s32 sfx = music->tracker.last.sfx;
							setNote(music, note, octave, volume, sfx);
						}

						downRow(music);

						break;
					}
				
				}
			}
			break;
		case ColumnOctave:
			if(getNote(music) >= 0)
			{
				s32 octave = -1;
				if (keycode >= SDLK_1 && keycode <= SDLK_8) octave = keycode - SDLK_1;
				if (keycode >= SDLK_KP_1 && keycode <= SDLK_KP_8) octave = keycode - SDLK_KP_1;

				if(octave >= 0)
				{
					setOctave(music, octave);
					downRow(music);
				}
			}
			break;
		case ColumnSfxHi:
		case ColumnSfxLow:
			if(getNote(music) >= 0)
			{
				s32 val = -1;
							
				if (keycode >= SDLK_0 && keycode <= SDLK_9) val = keycode - SDLK_0;
				if (keycode >= SDLK_KP_1 && keycode <= SDLK_KP_9) val = keycode - SDLK_KP_1 + 1;
				if (keycode == SDLK_KP_0) val = 0;

				if(val >= 0)
				{
					enum {Base = 10};
					s32 sfx = getSfx(music);

					sfx = col == 3 
						? val * Base + sfx % Base
						: sfx / Base * Base + val % Base;

					setSfx(music, sfx);

					if(col == 3) rightCol(music);
					else downRow(music), leftCol(music);
				}
			}
			break;
		case ColumnVolume:
			if (getNote(music) >= 0)
			{
				s32 val = -1;
							
				if(keycode >= SDLK_0 && keycode <= SDLK_9) val = keycode - SDLK_0;
				if(keycode >= SDLK_a && keycode <= SDLK_f) val = keycode - SDLK_a + 10;
				if(keycode >= SDLK_KP_1 && keycode <= SDLK_KP_9) val = keycode - SDLK_KP_1 + 1;
				if(keycode == SDLK_KP_0) val = 0;

				if(val >= 0)
				{
					setVolume(music, MAX_VOLUME - val);
					downRow(music);
				}
			}
			break;
		}

		history_add(music->history);
	}
}

static void processPatternKeydown(Music* music, SDL_Keysym* keysum)
{
	SDL_Keycode keycode = keysum->sym;

	s32 channel = music->tracker.col / CHANNEL_COLS;

	switch (keycode)
	{
	case SDLK_DELETE: setChannelPatternValue(music, 0, channel); break;
	case SDLK_TAB: nextPattern(music); break;
	case SDLK_DOWN:
	case SDLK_KP_ENTER:
	case SDLK_RETURN: music->tracker.row = music->tracker.scroll; break;
	case SDLK_LEFT: patternColLeft(music); break;
	case SDLK_RIGHT: patternColRight(music); break;
	default:
		{
			s32 val = -1;

			if(keycode >= SDLK_0 && keycode <= SDLK_9) val = keycode - SDLK_0;
			if(keycode >= SDLK_KP_1 && keycode <= SDLK_KP_9) val = keycode - SDLK_KP_1 + 1;
			if(keycode == SDLK_KP_0) val = 0;


			if(val >= 0)
			{
				enum {Base = 10};
				s32 patternId = tic_tool_get_pattern_id(getTrack(music), music->tracker.frame, channel);

				patternId = music->tracker.patternCol == 0
					? val * Base + patternId % Base
					: patternId / Base * Base + val % Base;

				if(patternId <= MUSIC_PATTERNS)
				{
					setChannelPatternValue(music, patternId, channel);

					if(music->tracker.patternCol == 0)
						patternColRight(music);		 				
				}
			}
		}
	}
}

static void selectAll(Music* music)
{
	resetSelection(music);

	s32 col = music->tracker.col - music->tracker.col % CHANNEL_COLS;

	music->tracker.select.start = (SDL_Point){col, 0};
	music->tracker.col = col + CHANNEL_COLS-1;
	music->tracker.row = MUSIC_PATTERN_ROWS-1;

	updateSelection(music);
}

static void processKeydown(Music* music, SDL_Keysym* keysum)
{
	SDL_Keycode keycode = keysum->sym;

	switch(getClipboardEvent(keycode))
	{
	case TIC_CLIPBOARD_CUT: copyToClipboard(music, true); break;
	case TIC_CLIPBOARD_COPY: copyToClipboard(music, false); break;
	case TIC_CLIPBOARD_PASTE: copyFromClipboard(music); break;
	default: break;
	}

	SDL_Keymod keymod = SDL_GetModState();

	if (keymod & TIC_MOD_CTRL)
	{
		switch (keycode)
		{
		case SDLK_a:	selectAll(music); break;
		case SDLK_z: 	undo(music); break;
		case SDLK_y: 	redo(music); break;
		case SDLK_UP: 	upFrame(music); break;
		case SDLK_DOWN:	downFrame(music); break;
		}
	}
	else
	{
		music->tracker.row >= 0 
			? processTrackerKeydown(music, keysum)
			: processPatternKeydown(music, keysum);
	}
}

static void processKeyup(Music* music, SDL_Keysym* keysum)
{
	music->tracker.note = -1;
	s32 channel = music->tracker.col / CHANNEL_COLS;
	music->tic->api.sfx_stop(music->tic, channel);
}

static void setIndex(Music* music, s32 delta, void* data)
{
	music->track += delta;
}

static void setTempo(Music* music, s32 delta, void* data)
{
	enum
	{
		Step = 10,
		Min = 40-DEFAULT_TEMPO,
		Max = 250-DEFAULT_TEMPO,
	};

	tic_track* track = getTrack(music);

	s32 tempo = track->tempo;
	tempo += delta * Step;

	if (tempo > Max) tempo = Max;
	if (tempo < Min) tempo = Min;

	track->tempo = tempo;

	history_add(music->history);
}

static void setSpeed(Music* music, s32 delta, void* data)
{
	enum
	{
		Step = 1,
		Min = 1-DEFAULT_SPEED,
		Max = 31-DEFAULT_SPEED,
	};

	tic_track* track = getTrack(music);

	s32 speed = track->speed;
	speed += delta * Step;

	if (speed > Max) speed = Max;
	if (speed < Min) speed = Min;

	track->speed = speed;

	history_add(music->history);
}

static void setRows(Music* music, s32 delta, void* data)
{
	enum
	{
		Step = 1,
		Min = 0,
		Max = MUSIC_PATTERN_ROWS - TRACKER_ROWS,
	};

	tic_track* track = getTrack(music);
	s32 rows = track->rows;
	rows -= delta * Step;

	if (rows < Min) rows = Min;
	if (rows > Max) rows = Max;

	track->rows = rows;

	updateTracker(music);

	history_add(music->history);
}

static void drawTopPanel(Music* music, s32 x, s32 y)
{
	tic_track* track = getTrack(music);

	drawSwitch(music, x, y, "TRACK", music->track, setIndex, NULL);
	drawSwitch(music, x += TIC_FONT_WIDTH * 10, y, "TEMPO", track->tempo + DEFAULT_TEMPO, setTempo, NULL);
	drawSwitch(music, x += TIC_FONT_WIDTH * 11, y, "SPD", track->speed + DEFAULT_SPEED, setSpeed, NULL);
	drawSwitch(music, x += TIC_FONT_WIDTH * 8, y, "ROWS", MUSIC_PATTERN_ROWS - track->rows, setRows, NULL);
}

static void drawTrackerFrames(Music* music, s32 x, s32 y)
{
	enum
	{
		Border = 1,
		Width = TIC_FONT_WIDTH * 2 + Border,
	};

	{
		SDL_Rect rect = { x - Border, y - Border, Width, MUSIC_FRAMES * TIC_FONT_HEIGHT + Border };

		if (checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);

			if (checkMouseDown(&rect, SDL_BUTTON_LEFT))
			{
				s32 my = getMouseY() - rect.y - Border;
				music->tracker.frame = my / TIC_FONT_HEIGHT;
			}
		}

		music->tic->api.rect(music->tic, rect.x, rect.y, rect.w, rect.h, (tic_color_black));
	}

	for (s32 i = 0; i < MUSIC_FRAMES; i++)
	{
		if (checkPlayFrame(music, i))
		{
			static const u8 Icon[] =
			{
				0b00000000,
				0b01000000,
				0b01100000,
				0b01110000,
				0b01100000,
				0b01000000,
				0b00000000,
				0b00000000,
			};

			drawBitIcon(x - TIC_FONT_WIDTH, y - 1 + i*TIC_FONT_HEIGHT, Icon, (tic_color_white));
		}

		if (i == music->tracker.frame)
		{
			music->tic->api.rect(music->tic, x - 1, y - 1 + i*TIC_FONT_HEIGHT, Width, TIC_FONT_HEIGHT + 1, (tic_color_white));
		}

		char buf[] = "99";
		sprintf(buf, "%02i", i);
		music->tic->api.fixed_text(music->tic, buf, x, y + i*TIC_FONT_HEIGHT, (tic_color_dark_gray));
	}

	if(music->tracker.row >= 0)
	{
		char buf[] = "99";
		sprintf(buf, "%02i", music->tracker.row);
		music->tic->api.fixed_text(music->tic, buf, x, y - 9, (tic_color_white));
	}
}

static void setChannelPattern(Music* music, s32 delta, s32 channel)
{
	tic_track* track = getTrack(music);
	s32 frame = music->tracker.frame;

	u32 patternData = 0;
	for(s32 b = 0; b < TRACK_PATTERNS_SIZE; b++)
		patternData |= track->data[frame * TRACK_PATTERNS_SIZE + b] << (BITS_IN_BYTE * b);

	s32 shift = channel * TRACK_PATTERN_BITS;
	s32 patternId = (patternData >> shift) & TRACK_PATTERN_MASK;

	setChannelPatternValue(music, patternId + delta, channel);
}

static void drawTrackerChannel(Music* music, s32 x, s32 y, s32 channel)
{
	tic_mem* tic = music->tic;

	enum
	{
		Border = 1,
		Rows = TRACKER_ROWS,
		Width = TIC_FONT_WIDTH * 8 + Border,
	};

	SDL_Rect rect = {x - Border, y - Border, Width, Rows*TIC_FONT_HEIGHT + Border};

	if(checkMousePos(&rect))
	{
		setCursor(SDL_SYSTEM_CURSOR_HAND);

		if(checkMouseDown(&rect, SDL_BUTTON_LEFT))
		{
			s32 mx = getMouseX() - rect.x - Border;
			s32 my = getMouseY() - rect.y - Border;

			s32 col = music->tracker.col = channel * CHANNEL_COLS + mx / TIC_FONT_WIDTH;
			s32 row = music->tracker.row = my / TIC_FONT_HEIGHT + music->tracker.scroll;

			if(music->tracker.select.drag)
			{
				updateSelection(music);
			}
			else
			{
				resetSelection(music);
				music->tracker.select.start = (SDL_Point){col, row};

				music->tracker.select.drag = true;
			}
		}
	}

	if(music->tracker.select.drag)
	{
		SDL_Rect rect = {0, 0, TIC80_WIDTH, TIC80_HEIGHT};
		if(!checkMouseDown(&rect, SDL_BUTTON_LEFT))
		{
			music->tracker.select.drag = false;
		}
	}

	music->tic->api.rect(music->tic, rect.x, rect.y, rect.w, rect.h, (tic_color_black));

	s32 start = music->tracker.scroll;
	s32 end = start + Rows;
	bool selectedChannel = music->tracker.select.rect.x / CHANNEL_COLS == channel;

	tic_track_pattern* pattern = getPattern(music, channel);

	for (s32 i = start, pos = 0; i < end; i++, pos++)
	{
		s32 rowy = y + pos*TIC_FONT_HEIGHT;

		if (i == music->tracker.row)
		{
			music->tic->api.rect(music->tic, x - 1, rowy - 1, Width, TIC_FONT_HEIGHT + 1, (tic_color_dark_red));
		}

		// draw selection
		if (selectedChannel)
		{
			SDL_Rect rect = music->tracker.select.rect;
			if (rect.h > 1 && i >= rect.y && i < rect.y + rect.h)
			{
				s32 sx = x - 1;
				tic->api.rect(tic, sx, rowy - 1, CHANNEL_COLS * TIC_FONT_WIDTH + 1, TIC_FONT_HEIGHT + 1, (tic_color_yellow));
			}
		}

		if (checkPlayRow(music, i))
		{
			music->tic->api.rect(music->tic, x - 1, rowy - 1, Width, TIC_FONT_HEIGHT + 1, (tic_color_white));
		}

		char rowStr[] = "--------";

		if (pattern)
		{
			static const char* Notes[] = SFX_NOTES;

			s32 note = pattern->rows[i].note;
			s32 octave = pattern->rows[i].octave;
			s32 sfx = getSfxId(pattern, i);
			s32 volume = MAX_VOLUME - pattern->rows[i].volume;

			if (note == NoteStop)
			{
				strcpy(rowStr, "      ");
			}
			else
			{
				if (note >= NoteStart)
					sprintf(rowStr, "%s%i%02i%01X--", Notes[note - NoteStart], octave + 1, sfx, volume & 0xf);

				const u8 Colors[] = { (tic_color_light_green), (tic_color_orange), (tic_color_blue), (tic_color_gray) };
				const u8 DarkColors[] = { (tic_color_green), (tic_color_brown), (tic_color_dark_blue), (tic_color_dark_gray) };
				static u8 ColorIndexes[] = { 0, 0, 0, 1, 1, 2, 3, 3 };

				bool beetRow = i % NOTES_PER_BEET == 0;

				for (s32 c = 0, colx = x; c < sizeof rowStr - 1; c++, colx += TIC_FONT_WIDTH)
				{
					char sym = rowStr[c];
					const u8* colors = beetRow || sym != '-' ? Colors : DarkColors;

					music->tic->api.draw_char(music->tic, sym, colx, rowy, colors[ColorIndexes[c]]);
				}
			}
		}
		else music->tic->api.fixed_text(music->tic, rowStr, x, rowy, (tic_color_dark_gray));

		if (i == music->tracker.row)
		{
			if (music->tracker.col / CHANNEL_COLS == channel)
			{
				s32 col = music->tracker.col % CHANNEL_COLS;
				s32 colx = x - 1 + col * TIC_FONT_WIDTH;
				music->tic->api.rect(music->tic, colx, rowy - 1, TIC_FONT_WIDTH + 1, TIC_FONT_HEIGHT + 1, (tic_color_red));
				music->tic->api.draw_char(music->tic, rowStr[col], colx + 1, rowy, (tic_color_black));
			}
		}

		if (i % NOTES_PER_BEET == 0)
			music->tic->api.pixel(music->tic, x - 4, y + pos*TIC_FONT_HEIGHT + 2, (tic_color_dark_gray));
	}
}

static void drawTumbler(Music* music, s32 x, s32 y, s32 index)
{
	tic_mem* tic = music->tic;

	enum{On=36, Off = 52, Size=5, Chroma=14};
	
	SDL_Rect rect = {x, y, Size, Size};

	if(checkMousePos(&rect))
	{
		setCursor(SDL_SYSTEM_CURSOR_HAND);

		showTooltip("on/off channel");

		if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
		{
			if (SDL_GetModState() & KMOD_CTRL)
			{
				for (s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
					music->tracker.patterns[i] = i == index;
			}
			else music->tracker.patterns[index] = !music->tracker.patterns[index];
		}
	}

	u8 color = Chroma;
	tic->api.sprite(tic, &tic->config.bank0.tiles, music->tracker.patterns[index] ? On : Off, x, y, &color, 1);
}

static void drawTracker(Music* music, s32 x, s32 y)
{
	drawTrackerFrames(music, x, y);

	x += TIC_FONT_WIDTH * 3;

	enum{ChannelWidth = TIC_FONT_WIDTH * 9};

	for (s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
	{
		s32 patternId = tic_tool_get_pattern_id(getTrack(music), music->tracker.frame, i);
		drawEditbox(music, x + ChannelWidth * i + 2*TIC_FONT_WIDTH, y - 9, patternId, setChannelPattern, i);
		drawTumbler(music, x + ChannelWidth * i + 7*TIC_FONT_WIDTH, y - 9, i);
	}

	for (s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
		drawTrackerChannel(music, x + ChannelWidth * i, y, i);
}

static void enableFollowMode(Music* music)
{
	music->tracker.follow = !music->tracker.follow;
}

static void drawPlayButtons(Music* music)
{
	static const u8 Icons[] =
	{
		0b00000000,
		0b01110000,
		0b11111000,
		0b11111000,
		0b11111000,
		0b01110000,
		0b00000000,
		0b00000000,

		0b00000000,
		0b01010000,
		0b01011000,
		0b01011100,
		0b01011000,
		0b01010000,
		0b00000000,
		0b00000000,

		0b00000000,
		0b00100000,
		0b00110000,
		0b00111000,
		0b00110000,
		0b00100000,
		0b00000000,
		0b00000000,

		0b00000000,
		0b01111100,
		0b01111100,
		0b01111100,
		0b01111100,
		0b01111100,
		0b00000000,
		0b00000000,
	};

	enum { Offset = TIC80_WIDTH - 52, Width = 7, Height = 7, Rows = 8, Count = sizeof Icons / Rows };

	for (s32 i = 0; i < Count; i++)
	{
		SDL_Rect rect = { Offset + Width * i, 0, Width, Height };

		bool over = false;

		if (checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);
			over = true;

			static const char* Tooltips[] = { "RECORD MUSIC", "PLAY FRAME [enter]", "PLAY TRACK", "STOP [enter]" };
			showTooltip(Tooltips[i]);

			static void(*const Handlers[])(Music*) = { enableFollowMode, playFrame, playTrack, stopTrack };

			if (checkMouseClick(&rect, SDL_BUTTON_LEFT))
				Handlers[i](music);
		}

		if(i == 0 && music->tracker.follow)
			drawBitIcon(rect.x, rect.y, Icons + i*Rows, over ? (tic_color_peach) : (tic_color_red));
		else
			drawBitIcon(rect.x, rect.y, Icons + i*Rows, over ? (tic_color_dark_gray) : (tic_color_light_blue));
	}
}

static void drawModeTabs(Music* music)
{
	static const u8 Icons[] =
	{
		0b00000000,
		0b01111111,
		0b01010101,
		0b01010101,
		0b01000001,
		0b01111111,
		0b00000000,
		0b00000000,

		0b00000000,
		0b01011011,
		0b00000000,
		0b01011011,
		0b00000000,
		0b01011011,
		0b00000000,
		0b00000000,
	};

	enum { Width = 9, Height = 7, Rows = 8, Count = sizeof Icons / Rows };

	for (s32 i = 0; i < Count; i++)
	{
		SDL_Rect rect = { TIC80_WIDTH - Width * (Count - i), 0, Width, Height };

		bool over = false;

		static const s32 Tabs[] = { MUSIC_PIANO_TAB, MUSIC_TRACKER_TAB };

		if (checkMousePos(&rect))
		{
			setCursor(SDL_SYSTEM_CURSOR_HAND);
			over = true;

			static const char* Tooltips[] = { "PIANO MODE", "TRACKER MODE" };
			showTooltip(Tooltips[i]);

			if (checkMouseClick(&rect, SDL_BUTTON_LEFT))
				music->tab = Tabs[i];
		}

		if (music->tab == Tabs[i])
			music->tic->api.rect(music->tic, rect.x, rect.y, rect.w, rect.h, (tic_color_gray));

		drawBitIcon(rect.x, rect.y, Icons + i*Rows, music->tab == Tabs[i] ? (tic_color_white) : over ? (tic_color_dark_gray) : (tic_color_light_blue));
	}
}

static void drawMusicToolbar(Music* music)
{
	music->tic->api.rect(music->tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, (tic_color_white));

	drawPlayButtons(music);
	drawModeTabs(music);
}

static void drawPianoLayout(Music* music)
{
	SDL_Event* event = NULL;
	while ((event = pollEvent())){}

	music->tic->api.clear(music->tic, (tic_color_gray));

	static const char Wip[] = "PIANO MODE - WORK IN PROGRESS...";
	music->tic->api.fixed_text(music->tic, Wip, (TIC80_WIDTH - (sizeof Wip - 1) * TIC_FONT_WIDTH) / 2, TIC80_HEIGHT / 2, (tic_color_white));
}

static void scrollNotes(Music* music, s32 delta)
{
	tic_track_pattern* pattern = getChannelPattern(music);

	if(pattern)
	{
		SDL_Rect rect = music->tracker.select.rect;

		if(rect.h <= 0)
		{
			rect.y = music->tracker.row;
			rect.h = 1;
		}
		
		for(s32 i = rect.y; i < rect.y + rect.h; i++)
		{
			s32 note = pattern->rows[i].note + pattern->rows[i].octave * NOTES - NoteStart;

			note += delta;

			if(note >= 0 && note < NOTES*OCTAVES)
			{
				pattern->rows[i].note = note % NOTES + NoteStart;
				pattern->rows[i].octave = note / NOTES;
			}
		}

		history_add(music->history);
	}
}

static void drawTrackerLayout(Music* music)
{
	SDL_Event* event = NULL;
	while ((event = pollEvent()))
	{
		switch (event->type)
		{
		case SDL_MOUSEWHEEL:
			if(SDL_GetModState() & TIC_MOD_CTRL)
			{
				scrollNotes(music, event->wheel.y > 0 ? 1 : -1);
			}
			else
			{		
				enum{Scroll = NOTES_PER_BEET};
				s32 delta = event->wheel.y > 0 ? -Scroll : Scroll;

				music->tracker.scroll += delta;

				updateScroll(music);
			}
			break;
		case SDL_KEYDOWN:
			processKeydown(music, &event->key.keysym);
			break;
		case SDL_KEYUP:
			processKeyup(music, &event->key.keysym);
			break;
		}
	}

	if(music->tracker.follow)
	{
		const tic_music_pos* pos = getMusicPos(music);

		if(pos->track == music->track && 
			music->tracker.row >= 0 &&
			pos->row >= 0)
		{
			music->tracker.frame = pos->frame;
			music->tracker.row = pos->row;
			updateTracker(music);
		}
	}

	music->tic->api.clear(music->tic, (tic_color_gray));

	drawTopPanel(music, 6, TOOLBAR_SIZE + 3);
	drawTracker(music, 6, 35);
}

static void tick(Music* music)
{
	tic_mem* tic = music->tic;

	for (s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
		if(!music->tracker.patterns[i])
			tic->ram.registers[i].volume = 0;

	switch (music->tab)
	{
	case MUSIC_TRACKER_TAB: drawTrackerLayout(music); break;
	case MUSIC_PIANO_TAB: drawPianoLayout(music); break;
	}

	drawMusicToolbar(music);
	drawToolbar(music->tic, (tic_color_gray), false);
}

static void onStudioEvent(Music* music, StudioEvent event)
{
	switch (event)
	{
	case TIC_TOOLBAR_CUT: copyToClipboard(music, true); break;
	case TIC_TOOLBAR_COPY: copyToClipboard(music, false); break;
	case TIC_TOOLBAR_PASTE: copyFromClipboard(music); break;
	case TIC_TOOLBAR_UNDO: undo(music); break;
	case TIC_TOOLBAR_REDO: redo(music); break;
	default: break;
	}
}

void initMusic(Music* music, tic_mem* tic, tic_music* src)
{
	if (music->history) history_delete(music->history);

	*music = (Music)
	{
		.tic = tic,
		.tick = tick,
		.src = src,
		.track = 0,
		.tracker =
		{
			.follow = false,
			.frame = 0,
			.col = 0,
			.row = 0,
			.scroll = 0,
			.note = -1,

			.last =
			{
				.octave = 3,
				.sfx = 0,
				.volume = 0,
			},

			.patterns = {true, true, true, true},
			.select = 
			{
				.start = {0, 0},
				.rect = {0, 0, 0, 0},
				.drag = false,
			},
		},

		.tab = MUSIC_TRACKER_TAB,
		.history = history_create(src, sizeof(tic_music)),
		.event = onStudioEvent,
	};

	resetSelection(music);
}
