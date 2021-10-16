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
#include "ext/history.h"

#include <ctype.h>

#define TRACKER_ROWS (MUSIC_PATTERN_ROWS / 4)
#define CHANNEL_COLS 8
#define TRACKER_COLS (TIC_SOUND_CHANNELS * CHANNEL_COLS)
#define PIANO_PATTERN_HEADER 10

#define CMD_STRING(_, c, ...) DEF2STR(c)
static const char MusicCommands[] = MUSIC_CMD_LIST(CMD_STRING);
#undef CMD_STRING

enum PianoEditColumns
{
    PianoChannel1Column = 0,
    PianoChannel2Column,
    PianoChannel3Column,
    PianoChannel4Column,
    PianoSfxColumn,
    PianoXYColumn,

    PianoColumnsCount
};

enum
{
    ColumnNote = 0,
    ColumnSemitone,
    ColumnOctave,
    ColumnSfxHi,
    ColumnSfxLow,
    ColumnCommand,
    ColumnParameter1,
    ColumnParameter2,
};

static void undo(Music* music)
{
    history_undo(music->history);
}

static void redo(Music* music)
{
    history_redo(music->history);
}

static const tic_sound_state* getMusicPos(Music* music)
{
    return &music->tic->ram.sound_state;
}

static void drawEditPanel(Music* music, s32 x, s32 y, s32 w, s32 h)
{
    tic_mem* tic = music->tic;

    tic_api_rect(tic, x, y-1, w, 1, tic_color_dark_grey);
    tic_api_rect(tic, x-1, y, 1, h, tic_color_dark_grey);
    tic_api_rect(tic, x, y+h, w, 1, tic_color_light_grey);
    tic_api_rect(tic, x+w, y, 1, h, tic_color_light_grey);

    tic_api_rect(tic, x, y, w, h, tic_color_black);
}

static void drawEditbox(Music* music, s32 x, s32 y, s32 value, void(*set)(Music*, s32, s32 channel), s32 channel)
{
    tic_mem* tic = music->tic;
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
        0b00100000,
        0b00110000,
        0b00111000,
        0b00110000,
        0b00100000,
        0b00000000,
        0b00000000,
        0b00000000,
    };

    {
        tic_rect rect = { x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT };

        bool over = false;
        bool down = false;
        if (checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            if (checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if (checkMouseClick(&rect, tic_mouse_left))
                set(music, -1, channel);
        }

        drawBitIcon(rect.x, rect.y + (down ? 2 : 1), LeftArrow, tic_color_black);
        drawBitIcon(rect.x, rect.y + (down ? 1 : 0), LeftArrow, (over ? tic_color_light_grey : tic_color_dark_grey));
    }

    {
        x += TIC_FONT_WIDTH;

        tic_rect rect = { x-1, y-1, TIC_FONT_WIDTH*2+1, TIC_FONT_HEIGHT+1 };

        if (checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            showTooltip("select pattern ID");

            if (checkMouseClick(&rect, tic_mouse_left))
            {
                music->tracker.edit.y = -1;
                music->tracker.edit.x = channel * CHANNEL_COLS;

                s32 mx = tic_api_mouse(tic).x - rect.x;
                music->tracker.col = mx / TIC_FONT_WIDTH;
            }
        }

        drawEditPanel(music, rect.x, rect.y, rect.w, rect.h);

        if(music->tracker.edit.y == -1 && music->tracker.edit.x / CHANNEL_COLS == channel)
        {
            tic_api_rect(music->tic, x - 1 + music->tracker.col * TIC_FONT_WIDTH, y - 1, TIC_FONT_WIDTH + 1, TIC_FONT_HEIGHT + 1, tic_color_red);
        }

        char val[] = "99";
        sprintf(val, "%02i", value);
        tic_api_print(music->tic, val, x, y, tic_color_white, true, 1, false);
    }

    {
        x += 2*TIC_FONT_WIDTH;

        tic_rect rect = { x, y, TIC_FONT_WIDTH, TIC_FONT_HEIGHT };

        bool over = false;
        bool down = false;
        if (checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            if (checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if (checkMouseClick(&rect, tic_mouse_left))
                set(music, +1, channel);
        }

        drawBitIcon(rect.x, rect.y + (down ? 2 : 1), RightArrow, tic_color_black);
        drawBitIcon(rect.x, rect.y + (down ? 1 : 0), RightArrow, (over ? tic_color_light_grey : tic_color_dark_grey));
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

    enum {ArrowWidth = 5};

    tic_api_print(music->tic, label, x, y+1, tic_color_black, true, 1, false);
    tic_api_print(music->tic, label, x, y, tic_color_white, true, 1, false);

    {
        x += (s32)strlen(label)*TIC_FONT_WIDTH;

        tic_rect rect = { x, y, ArrowWidth, TIC_FONT_HEIGHT };

        bool over = false;
        bool down = false;
        if (checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            over = true;

            if (checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if (checkMouseClick(&rect, tic_mouse_left))
                set(music, -1, data);
        }

        drawBitIcon(rect.x, rect.y + (down ? 2 : 1), LeftArrow, tic_color_black);
        drawBitIcon(rect.x, rect.y + (down ? 1 : 0), LeftArrow, over ? tic_color_light_grey : tic_color_dark_grey);
    }

    {
        char val[] = "999";
        sprintf(val, "%02i", value);
        tic_api_print(music->tic, val, x + ArrowWidth, y+1, tic_color_black, true, 1, false);
        tic_api_print(music->tic, val, x += ArrowWidth, y, tic_color_yellow, true, 1, false);
    }

    {
        x += (value > 99 ? 3 : 2)*TIC_FONT_WIDTH-1;

        tic_rect rect = { x, y, ArrowWidth, TIC_FONT_HEIGHT };

        bool over = false;
        bool down = false;
        if (checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            over = true;

            if (checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if (checkMouseClick(&rect, tic_mouse_left))
                set(music, +1, data);
        }

        drawBitIcon(rect.x, rect.y + (down ? 2 : 1), RightArrow, tic_color_black);
        drawBitIcon(rect.x, rect.y + (down ? 1 : 0), RightArrow, over ? tic_color_light_grey : tic_color_dark_grey);
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
    music->scroll.pos = CLAMP(music->scroll.pos, 0, getRows(music) - TRACKER_ROWS);
}

static void updateTracker(Music* music)
{
    s32 row = music->tracker.edit.y;

    enum{Threshold = TRACKER_ROWS / 2};
    music->scroll.pos = CLAMP(music->scroll.pos, row - (TRACKER_ROWS - Threshold), row - Threshold);

    {
        s32 rows = getRows(music);
        if (music->tracker.edit.y >= rows) music->tracker.edit.y = rows - 1;
    }

    updateScroll(music);
}

static void upRow(Music* music)
{
    if (music->tracker.edit.y > -1)
    {
        music->tracker.edit.y--;
        updateTracker(music);
    }
}

static void downRow(Music* music)
{
    const tic_sound_state* pos = getMusicPos(music);
    // Don't move the cursor if the track is being played/recorded
    if(pos->music.track == music->track && music->follow) return;

    if (music->tracker.edit.y < getRows(music) - 1)
    {
        music->tracker.edit.y++;
        updateTracker(music);
    }
}

static void leftCol(Music* music)
{
    if (music->tracker.edit.x > 0)
    {
        music->tracker.edit.x--;
        updateTracker(music);
    }
}

static void rightCol(Music* music)
{
    if (music->tracker.edit.x < TRACKER_COLS - 1)
    {
        music->tracker.edit.x++;
        updateTracker(music);
    }
}

static void goHome(Music* music)
{
    music->tracker.edit.x -= music->tracker.edit.x % CHANNEL_COLS;
}

static void goEnd(Music* music)
{
    music->tracker.edit.x -= music->tracker.edit.x % CHANNEL_COLS;
    music->tracker.edit.x += CHANNEL_COLS-1;
}

static void pageUp(Music* music)
{
    music->tracker.edit.y -= TRACKER_ROWS;
    if(music->tracker.edit.y < 0) 
        music->tracker.edit.y = 0;

    updateTracker(music);
}

static void pageDown(Music* music)
{
    if (music->tracker.edit.y < getRows(music) - 1)

    music->tracker.edit.y += TRACKER_ROWS;
    s32 rows = getRows(music);

    if(music->tracker.edit.y >= rows) 
        music->tracker.edit.y = rows-1;
    
    updateTracker(music);
}

static void doTab(Music* music)
{
    s32 channel = (music->tracker.edit.x / CHANNEL_COLS + 1) % TIC_SOUND_CHANNELS;

    music->tracker.edit.x = channel * CHANNEL_COLS + music->tracker.edit.x % CHANNEL_COLS;

    updateTracker(music);
}

static void upFrame(Music* music)
{
    music->frame--;

    if(music->frame < 0)
        music->frame = 0;
}

static void downFrame(Music* music)
{
    music->frame++;

    if(music->frame >= MUSIC_FRAMES)
        music->frame = MUSIC_FRAMES-1;
}

static bool checkPlayFrame(Music* music, s32 frame)
{
    const tic_sound_state* pos = getMusicPos(music);

    return pos->music.track == music->track &&
        pos->music.frame == frame;
}

static bool checkPlayRow(Music* music, s32 row)
{
    const tic_sound_state* pos = getMusicPos(music);

    return checkPlayFrame(music, music->frame) && pos->music.row == row;
}

static tic_track_pattern* getFramePattern(Music* music, s32 channel, s32 frame)
{
    s32 patternId = tic_tool_get_pattern_id(getTrack(music), frame, channel);

    return patternId ? &music->src->patterns.data[patternId - PATTERN_START] : NULL;
}

static tic_track_pattern* getPattern(Music* music, s32 channel)
{
    return getFramePattern(music, channel, music->frame);
}

static tic_track_pattern* getChannelPattern(Music* music)
{
    s32 channel = music->tracker.edit.x / CHANNEL_COLS;

    return getPattern(music, channel);
}

static s32 getNote(Music* music)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    return pattern->rows[music->tracker.edit.y].note - NoteStart;
}

static s32 getOctave(Music* music)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    return pattern->rows[music->tracker.edit.y].octave;
}

static s32 getSfx(Music* music)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    return tic_tool_get_track_row_sfx(&pattern->rows[music->tracker.edit.y]);
}

static inline tic_music_state getMusicState(Music* music)
{
    return music->tic->ram.sound_state.flag.music_state;
}

static inline void setMusicState(Music* music, tic_music_state state)
{
    music->tic->ram.sound_state.flag.music_state = state;
}

static void playNote(Music* music, const tic_track_row* row)
{
    tic_mem* tic = music->tic;

    if(getMusicState(music) == tic_music_stop && row->note >= NoteStart)
    {
        s32 channel = music->piano.col;
        sfx_stop(tic, channel);
        tic_api_sfx(tic, tic_tool_get_track_row_sfx(row), row->note - NoteStart, row->octave, TIC80_FRAMERATE / 4, channel, MAX_VOLUME, MAX_VOLUME, 0);
    }
}

static void setSfx(Music* music, s32 sfx)
{
    tic_track_pattern* pattern = getChannelPattern(music);
    tic_track_row* row = &pattern->rows[music->tracker.edit.y];

    tic_tool_set_track_row_sfx(row, sfx);

    music->last.sfx = tic_tool_get_track_row_sfx(&pattern->rows[music->tracker.edit.y]);

    playNote(music, row);
}

static void setStopNote(Music* music)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    pattern->rows[music->tracker.edit.y].note = NoteStop;
    pattern->rows[music->tracker.edit.y].octave = 0;
}

static void setNote(Music* music, s32 note, s32 octave, s32 sfx)
{
    tic_track_pattern* pattern = getChannelPattern(music);
    tic_track_row* row = &pattern->rows[music->tracker.edit.y];
    row->note = note + NoteStart;
    row->octave = octave;
    tic_tool_set_track_row_sfx(row, sfx);

    playNote(music, row);
}

static void setOctave(Music* music, s32 octave)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    tic_track_row* row = &pattern->rows[music->tracker.edit.y];
    row->octave = octave;

    music->last.octave = octave;

    playNote(music, row);
}

static void setCommandDefaults(tic_track_row* row)
{
    switch(row->command)
    {
    case tic_music_cmd_volume:
        row->param2 = row->param1 = MAX_VOLUME;
        break;
    case tic_music_cmd_pitch:
        row->param1 = PITCH_DELTA >> 4;
        row->param2 = PITCH_DELTA & 0xf;
    default: break;
    }
}

static void setCommand(Music* music, tic_music_command command)
{
    tic_track_pattern* pattern = getChannelPattern(music);
    tic_track_row* row = &pattern->rows[music->tracker.edit.y];

    tic_music_command prev = row->command;
    row->command = command;

    if(prev == tic_music_cmd_empty)
        setCommandDefaults(row);
}

static void setParam1(Music* music, u8 value)
{
    tic_track_pattern* pattern = getChannelPattern(music);
    tic_track_row* row = &pattern->rows[music->tracker.edit.y];

    row->param1 = value;
}

static void setParam2(Music* music, u8 value)
{
    tic_track_pattern* pattern = getChannelPattern(music);
    tic_track_row* row = &pattern->rows[music->tracker.edit.y];

    row->param2 = value;
}

static void playFrameRow(Music* music)
{
    tic_mem* tic = music->tic;

    tic_api_music(tic, music->track, music->frame, music->tracker.edit.y, true, music->sustain);
    
    setMusicState(music, tic_music_play_frame);
}

static void playFrame(Music* music)
{
    tic_mem* tic = music->tic;

    tic_api_music(tic, music->track, music->frame, -1, true, music->sustain);

    setMusicState(music, tic_music_play_frame);
}

static void playTrack(Music* music)
{
    tic_api_music(music->tic, music->track, -1, -1, true, music->sustain);
}

static void stopTrack(Music* music)
{
    tic_api_music(music->tic, -1, -1, -1, false, music->sustain);
}

static void toggleFollowMode(Music* music)
{
    music->follow = !music->follow;
}

static void toggleSustainMode(Music* music)
{
    music->tic->ram.sound_state.flag.music_sustain = !music->sustain;
    music->sustain = !music->sustain;
}

static void resetSelection(Music* music)
{
    music->tracker.select.start = (tic_point){-1, -1};
    music->tracker.select.rect = (tic_rect){0, 0, 0, 0};
}

static void deleteSelection(Music* music)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    if(pattern)
    {
        tic_rect rect = music->tracker.select.rect;

        if(rect.h <= 0)
        {
            rect.y = music->tracker.edit.y;
            rect.h = 1;
        }

        enum{RowSize = sizeof(tic_track_pattern) / MUSIC_PATTERN_ROWS};
        memset(&pattern->rows[rect.y], 0, RowSize * rect.h);
    }
}

typedef struct
{
    u8 size;
} ClipboardHeader;

static void copyPianoToClipboard(Music* music, bool cut)
{
    tic_track_pattern* pattern = getFramePattern(music, music->piano.col, music->frame);

    if(pattern)
    {
        ClipboardHeader header = {MUSIC_PATTERN_ROWS};

        enum{HeaderSize = sizeof(ClipboardHeader), Size = sizeof(tic_track_pattern) + HeaderSize};

        u8* data = malloc(Size);

        if(data)
        {
            memcpy(data, &header, HeaderSize);
            memcpy(data + HeaderSize, pattern->rows, sizeof(tic_track_pattern));

            toClipboard(data, Size, true);

            free(data);

            if(cut)
            {
                memset(pattern->rows, 0, sizeof(tic_track_pattern));
                history_add(music->history);
            }
        }       
    }
}

static void copyPianoFromClipboard(Music* music)
{
    tic_track_pattern* pattern = getFramePattern(music, music->piano.col, music->frame);

    if(pattern && tic_sys_clipboard_has())
    {
        char* clipboard = tic_sys_clipboard_get();

        if(clipboard)
        {
            s32 size = (s32)strlen(clipboard)/2;

            enum{RowSize = sizeof(tic_track_pattern) / MUSIC_PATTERN_ROWS, HeaderSize = sizeof(ClipboardHeader)};

            if(size > HeaderSize)
            {
                u8* data = malloc(size);

                tic_tool_str2buf(clipboard, (s32)strlen(clipboard), data, true);

                ClipboardHeader header = {0};

                memcpy(&header, data, HeaderSize);

                if(size == header.size * RowSize + HeaderSize 
                    && size == sizeof(tic_track_pattern) + HeaderSize)
                {
                    memcpy(pattern->rows, data + HeaderSize, header.size * RowSize);
                    history_add(music->history);
                }

                free(data);
            }

            tic_sys_clipboard_free(clipboard);
        }
    }
}

static void copyTrackerToClipboard(Music* music, bool cut)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    if(pattern)
    {
        tic_rect rect = music->tracker.select.rect;

        if(rect.h <= 0)
        {
            rect.y = music->tracker.edit.y;
            rect.h = 1;
        }

        ClipboardHeader header = {rect.h};

        enum{RowSize = sizeof(tic_track_pattern) / MUSIC_PATTERN_ROWS, HeaderSize = sizeof(ClipboardHeader)};

        s32 size = rect.h * RowSize + HeaderSize;
        u8* data = malloc(size);

        if(data)
        {
            memcpy(data, &header, HeaderSize);
            memcpy(data + HeaderSize, &pattern->rows[rect.y], RowSize * rect.h);

            toClipboard(data, size, true);

            free(data);

            if(cut)
            {
                deleteSelection(music);
                history_add(music->history);
            }

            resetSelection(music);
        }       
    }
}

static void copyTrackerFromClipboard(Music* music)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    if(pattern && tic_sys_clipboard_has())
    {
        char* clipboard = tic_sys_clipboard_get();

        if(clipboard)
        {
            s32 size = (s32)strlen(clipboard)/2;

            enum{RowSize = sizeof(tic_track_pattern) / MUSIC_PATTERN_ROWS, HeaderSize = sizeof(ClipboardHeader)};

            if(size > HeaderSize)
            {
                u8* data = malloc(size);

                tic_tool_str2buf(clipboard, (s32)strlen(clipboard), data, true);

                ClipboardHeader header = {0};

                memcpy(&header, data, HeaderSize);

                if(header.size * RowSize == size - HeaderSize)
                {
                    if(header.size + music->tracker.edit.y > MUSIC_PATTERN_ROWS)
                        header.size = MUSIC_PATTERN_ROWS - music->tracker.edit.y;

                    memcpy(&pattern->rows[music->tracker.edit.y], data + HeaderSize, header.size * RowSize);
                    history_add(music->history);
                }

                free(data);
            }

            tic_sys_clipboard_free(clipboard);
        }
    }
}

static void copyToClipboard(Music* music, bool cut)
{
    switch (music->tab)
    {
    case MUSIC_TRACKER_TAB: copyTrackerToClipboard(music, cut); break;
    case MUSIC_PIANO_TAB: copyPianoToClipboard(music, cut); break;
    }
}

static void copyFromClipboard(Music* music)
{
    switch (music->tab)
    {
    case MUSIC_TRACKER_TAB: copyTrackerFromClipboard(music); break;
    case MUSIC_PIANO_TAB: copyPianoFromClipboard(music); break;
    }
}

static void setChannelPatternValue(Music* music, s32 patternId, s32 frame, s32 channel)
{
    tic_track* track = getTrack(music);

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
    s32 channel = music->tracker.edit.x / CHANNEL_COLS;

    if (channel > 0)
    {
        music->tracker.edit.x = (channel-1) * CHANNEL_COLS;
        music->tracker.col = 1;
    }
}

static void nextPattern(Music* music)
{
    s32 channel = music->tracker.edit.x / CHANNEL_COLS;

    if (channel < TIC_SOUND_CHANNELS-1)
    {
        music->tracker.edit.x = (channel+1) * CHANNEL_COLS;
        music->tracker.col = 0;
    }
}

static void colLeft(Music* music)
{
    if(music->tracker.col > 0)
        music->tracker.col--;
    else prevPattern(music);
}

static void colRight(Music* music)
{
    if(music->tracker.col < 1)
        music->tracker.col++;
    else nextPattern(music);
}

static void checkSelection(Music* music)
{
    if(music->tracker.select.start.x < 0 || music->tracker.select.start.y < 0)
    {
        music->tracker.select.start.x = music->tracker.edit.x;
        music->tracker.select.start.y = music->tracker.edit.y;
    }
}

static void updateSelection(Music* music)
{
    s32 rl = MIN(music->tracker.edit.x, music->tracker.select.start.x);
    s32 rt = MIN(music->tracker.edit.y, music->tracker.select.start.y);
    s32 rr = MAX(music->tracker.edit.x, music->tracker.select.start.x);
    s32 rb = MAX(music->tracker.edit.y, music->tracker.select.start.y);

    tic_rect* rect = &music->tracker.select.rect;
    *rect = (tic_rect){rl, rt, rr - rl + 1, rb - rt + 1};

    if(rect->x % CHANNEL_COLS + rect->w > CHANNEL_COLS)
        resetSelection(music);
}

static s32 getDigit(s32 pos, s32 val)
{
    enum {Base = 10};

    s32 div = 1;
    while(pos--) div *= Base;

    return val / div % Base;
}

static s32 setDigit(s32 pos, s32 val, s32 digit)
{
    enum {Base = 10};

    s32 div = 1;
    while(pos--) div *= Base;

    return val - (val / div % Base - digit) * div;
}

static s32 sym2dec(char sym)
{
    s32 val = -1;
    if (sym >= '0' && sym <= '9') val = sym - '0';
    return val;
}

static s32 sym2hex(char sym)
{
    s32 val = sym2dec(sym);
    if (sym >= 'a' && sym <= 'f') val = sym - 'a' + 10;

    return val;
}

static void processTrackerKeyboard(Music* music)
{
    tic_mem* tic = music->tic;

    if(tic->ram.input.keyboard.data == 0)
        return;

    if(tic_api_key(tic, tic_key_ctrl) || tic_api_key(tic, tic_key_alt))
        return;

    bool shift = tic_api_key(tic, tic_key_shift);

    if(shift)
    {
        if(keyWasPressed(tic_key_up)
            || keyWasPressed(tic_key_down)
            || keyWasPressed(tic_key_left)
            || keyWasPressed(tic_key_right)
            || keyWasPressed(tic_key_home)
            || keyWasPressed(tic_key_end)
            || keyWasPressed(tic_key_pageup)
            || keyWasPressed(tic_key_pagedown)
            || keyWasPressed(tic_key_tab))
        {
            checkSelection(music);
        }
    }

    if(keyWasPressed(tic_key_up))               upRow(music);
    else if(keyWasPressed(tic_key_down))        downRow(music);
    else if(keyWasPressed(tic_key_left))        leftCol(music);
    else if(keyWasPressed(tic_key_right))       rightCol(music);
    else if(keyWasPressed(tic_key_home))        goHome(music);
    else if(keyWasPressed(tic_key_end))         goEnd(music);
    else if(keyWasPressed(tic_key_pageup))      pageUp(music);
    else if(keyWasPressed(tic_key_pagedown))    pageDown(music);
    else if(keyWasPressed(tic_key_tab))         doTab(music);
    else if(keyWasPressed(tic_key_delete))      
    {
        deleteSelection(music);
        history_add(music->history);
        downRow(music);
    }
    else if(keyWasPressed(tic_key_space)) 
    {
        const tic_track_pattern* pattern = getChannelPattern(music);
        if(pattern)
        {
            const tic_track_row* row = &pattern->rows[music->tracker.edit.y];
            playNote(music, row);            
        }
    }

    if(shift)
    {
        if(keyWasPressed(tic_key_up)
            || keyWasPressed(tic_key_down)
            || keyWasPressed(tic_key_left)
            || keyWasPressed(tic_key_right)
            || keyWasPressed(tic_key_home)
            || keyWasPressed(tic_key_end)
            || keyWasPressed(tic_key_pageup)
            || keyWasPressed(tic_key_pagedown)
            || keyWasPressed(tic_key_tab))
        {
            updateSelection(music);
        }
    }
    else resetSelection(music);

    static const tic_keycode Piano[] =
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

        // octave +1
        tic_key_q,
        tic_key_2,
        tic_key_w,
        tic_key_3,
        tic_key_e,
        tic_key_r,
        tic_key_5,
        tic_key_t,
        tic_key_6,
        tic_key_y,
        tic_key_7,
        tic_key_u,
        
        // extra keys
        tic_key_i,
        tic_key_9,
        tic_key_o,
        tic_key_0,
        tic_key_p,
    };

    if (getChannelPattern(music))
    {
        s32 col = music->tracker.edit.x % CHANNEL_COLS;

        switch (col)
        {
        case ColumnNote:
        case ColumnSemitone:
            if (keyWasPressed(tic_key_1) || keyWasPressed(tic_key_a))
            {
                setStopNote(music);
                downRow(music);
            }
            else
            {
                tic_track_pattern* pattern = getChannelPattern(music);

                for (s32 i = 0; i < COUNT_OF(Piano); i++)
                {
                    if (keyWasPressed(Piano[i]))
                    {
                        s32 note = i % NOTES;
                        s32 octave = i / NOTES + music->last.octave;
                        s32 sfx = music->last.sfx;
                        setNote(music, note, octave, sfx);

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

                char sym = getKeyboardText();

                if(sym >= '1' && sym <= '8') octave = sym - '1';

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
                s32 val = sym2dec(getKeyboardText());
                            
                if(val >= 0)
                {
                    s32 sfx = setDigit(col == 3 ? 1 : 0, getSfx(music), val);

                    setSfx(music, sfx);

                    if(col == 3) rightCol(music);
                    else downRow(music), leftCol(music);
                }
            }
            break;
        case ColumnCommand:
            {
                char sym = getKeyboardText();

                if(sym)
                {
                    const char* val = strchr(MusicCommands, toupper(sym));
                                
                    if(val)
                        setCommand(music, val - MusicCommands);
                }
            }
            break;
        case ColumnParameter1:
        case ColumnParameter2:
            {
                s32 val = sym2hex(getKeyboardText());

                if(val >= 0)
                {
                    col == ColumnParameter1
                        ? setParam1(music, val)
                        : setParam2(music, val);
                }
            }
            break;          
        }

        history_add(music->history);
    }
}

static void processPatternKeyboard(Music* music)
{
    tic_mem* tic = music->tic;
    s32 channel = music->tracker.edit.x / CHANNEL_COLS;

    if(tic_api_key(tic, tic_key_ctrl) || tic_api_key(tic, tic_key_alt))
        return;

    if(keyWasPressed(tic_key_delete))       setChannelPatternValue(music, 0, music->frame, channel);
    else if(keyWasPressed(tic_key_tab))     nextPattern(music);
    else if(keyWasPressed(tic_key_left))    colLeft(music);
    else if(keyWasPressed(tic_key_right))   colRight(music);
    else if(keyWasPressed(tic_key_down) 
        || keyWasPressed(tic_key_return)) 
        music->tracker.edit.y = music->scroll.pos;
    else
    {
        s32 val = sym2dec(getKeyboardText());

        if(val >= 0)
        {
            s32 pattern = setDigit(1 - music->tracker.col & 1, tic_tool_get_pattern_id(getTrack(music), 
                music->frame, channel), val);

            if(pattern <= MUSIC_PATTERNS)
            {
                setChannelPatternValue(music, pattern, music->frame, channel);

                if(music->tracker.col == 0)
                    colRight(music);                     
            }
        }
    }
}

static void updatePianoEditPos(Music* music)
{
    music->piano.edit.x = CLAMP(music->piano.edit.x, 0, PianoColumnsCount * 2 - 1);

    switch(music->piano.edit.x / 2)
    {
    case PianoSfxColumn:
    case PianoXYColumn:
        if(music->piano.edit.y < 0)
            music->scroll.pos += music->piano.edit.y;

        if(music->piano.edit.y > TRACKER_ROWS-1)
            music->scroll.pos += music->piano.edit.y - (TRACKER_ROWS - 1);

        updateScroll(music);
        break;
    }

    music->piano.edit.y = CLAMP(music->piano.edit.y, 0, MUSIC_FRAMES-1);
}

static void updatePianoEditCol(Music* music)
{
    if(music->piano.edit.x & 1)
    {
        music->piano.edit.x--;
        music->piano.edit.y++;
    }
    else music->piano.edit.x++;

    updatePianoEditPos(music);
}

static inline s32 rowIndex(Music* music, s32 row)
{
    return row + music->scroll.pos;
}

static tic_track_row* getPianoRow(Music* music)
{
    tic_track_pattern* pattern = getFramePattern(music, music->piano.col, music->frame);
    return pattern ? &pattern->rows[rowIndex(music, music->piano.edit.y)] : NULL;
}

static s32 getPianoValue(Music* music)
{
    s32 col = music->piano.edit.x / 2;
    tic_track_row* row = getPianoRow(music);

    switch(col)
    {
    case PianoChannel1Column:
    case PianoChannel2Column:
    case PianoChannel3Column:
    case PianoChannel4Column:
        return getDigit(1 - music->piano.edit.x & 1, tic_tool_get_pattern_id(getTrack(music), music->piano.edit.y, col));

    case PianoSfxColumn:
        return row && row->note >= NoteStart ? getDigit(1 - music->piano.edit.x & 1, tic_tool_get_track_row_sfx(row)) : -1;

    case PianoXYColumn:
        return row && row->command > tic_music_cmd_empty ? (music->piano.edit.x & 1 ? row->param2 : row->param1) : -1;
    }

    return -1;
}

static void setPianoValue(Music* music, char sym)
{
    s32 col = music->piano.edit.x / 2;
    s32 dec = sym2dec(sym);
    s32 hex = sym2hex(sym);
    tic_track_row* row = getPianoRow(music);

    switch(col)
    {
    case PianoChannel1Column:
    case PianoChannel2Column:
    case PianoChannel3Column:
    case PianoChannel4Column:
        if(dec >= 0)
        {
            s32 pattern = setDigit(1 - music->piano.edit.x & 1, 
                tic_tool_get_pattern_id(getTrack(music), music->piano.edit.y, col), dec);

            if(pattern <= MUSIC_PATTERNS)
            {
                setChannelPatternValue(music, pattern, music->piano.edit.y, col);
                updatePianoEditCol(music);
            }
        }
        break;
    case PianoSfxColumn:
        if(row && row->note >= NoteStart && dec >= 0)
        {
            s32 sfx = setDigit(1 - music->piano.edit.x & 1, tic_tool_get_track_row_sfx(row), dec);
            tic_tool_set_track_row_sfx(row, sfx);
            history_add(music->history);

            music->last.sfx = tic_tool_get_track_row_sfx(row);

            updatePianoEditCol(music);
            playNote(music, row);
        }
        break;

    case PianoXYColumn:
        if(row && row->command > tic_music_cmd_empty && hex >= 0)
        {
            if(music->piano.edit.x & 1)
                row->param2 = hex;
            else row->param1 = hex;

            history_add(music->history);

            updatePianoEditCol(music);
        }
        break;
    }
}

static void processPianoKeyboard(Music* music)
{
    tic_mem* tic = music->tic;

    if(keyWasPressed(tic_key_up)) music->piano.edit.y--;
    else if(keyWasPressed(tic_key_down)) music->piano.edit.y++;
    else if(keyWasPressed(tic_key_left)) music->piano.edit.x--;
    else if(keyWasPressed(tic_key_right)) music->piano.edit.x++;
    else if(keyWasPressed(tic_key_home)) music->piano.edit.x = PianoChannel1Column;
    else if(keyWasPressed(tic_key_end)) music->piano.edit.x = PianoColumnsCount*2+1;
    else if(keyWasPressed(tic_key_pageup)) music->piano.edit.y -= TRACKER_ROWS;
    else if(keyWasPressed(tic_key_pagedown)) music->piano.edit.y += TRACKER_ROWS;

    updatePianoEditPos(music);

    if(keyWasPressed(tic_key_delete))
    {
        s32 col = music->piano.edit.x / 2;
        switch(col)
        {
        case PianoChannel1Column:
        case PianoChannel2Column:
        case PianoChannel3Column:
        case PianoChannel4Column:
            setChannelPatternValue(music, 00, music->piano.edit.y, col);
            break;
        case PianoSfxColumn:
            {
                tic_track_row* row = getPianoRow(music);
                if(row)
                {
                    tic_tool_set_track_row_sfx(row, 0);
                    history_add(music->history);
                }
            }
            break;
        case PianoXYColumn:
            {
                tic_track_row* row = getPianoRow(music);
                if(row)
                {
                    row->param1 = row->param2 = 0;
                    history_add(music->history);
                }
            }
            break;
        }
    }

    if(getKeyboardText())
        setPianoValue(music, getKeyboardText());
}

static void selectAll(Music* music)
{
    resetSelection(music);

    s32 col = music->tracker.edit.x - music->tracker.edit.x % CHANNEL_COLS;

    music->tracker.select.start = (tic_point){col, 0};
    music->tracker.edit.x = col + CHANNEL_COLS-1;
    music->tracker.edit.y = MUSIC_PATTERN_ROWS-1;

    updateSelection(music);
}

static void processKeyboard(Music* music)
{
    tic_mem* tic = music->tic;

    switch(getClipboardEvent())
    {
    case TIC_CLIPBOARD_CUT: copyToClipboard(music, true); break;
    case TIC_CLIPBOARD_COPY: copyToClipboard(music, false); break;
    case TIC_CLIPBOARD_PASTE: copyFromClipboard(music); break;
    default: break;
    }

    bool ctrl = tic_api_key(tic, tic_key_ctrl);
    bool shift = tic_api_key(tic, tic_key_shift);

    if (ctrl)
    {
        if(keyWasPressed(tic_key_a))            selectAll(music);
        else if(keyWasPressed(tic_key_z))       undo(music);
        else if(keyWasPressed(tic_key_y))       redo(music);
        else if(keyWasPressed(tic_key_up))      upFrame(music);
        else if(keyWasPressed(tic_key_down))    downFrame(music);
        else if(keyWasPressed(tic_key_f))       toggleFollowMode(music);
    }
    else
    {
        if(keyWasPressed(tic_key_return))
        {
            const tic_sound_state* pos = getMusicPos(music);
            pos->music.track < 0
                ? (shift && music->tab == MUSIC_TRACKER_TAB
                    ? playFrameRow(music) 
                    : playFrame(music))
                : stopTrack(music);
        }

        switch (music->tab)
        {
        case MUSIC_TRACKER_TAB:
            music->tracker.edit.y >= 0 
                ? processTrackerKeyboard(music)
                : processPatternKeyboard(music);
            break;
        case MUSIC_PIANO_TAB:
            processPianoKeyboard(music);
            break;
        }
    }
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
    drawSwitch(music, x += TIC_FONT_WIDTH * 9, y, "TEMPO", track->tempo + DEFAULT_TEMPO, setTempo, NULL);
    drawSwitch(music, x += TIC_FONT_WIDTH * 10, y, "SPD", track->speed + DEFAULT_SPEED, setSpeed, NULL);
    drawSwitch(music, x += TIC_FONT_WIDTH * 7, y, "ROWS", MUSIC_PATTERN_ROWS - track->rows, setRows, NULL);
}

static void drawTrackerFrames(Music* music, s32 x, s32 y)
{
    tic_mem* tic = music->tic;
    enum
    {
        Border = 1,
        Width = TIC_FONT_WIDTH * 2 + Border,
    };

    {
        tic_rect rect = { x - Border, y - Border, Width, MUSIC_FRAMES * TIC_FONT_HEIGHT + Border };

        if (checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            showTooltip("select frame");

            if (checkMouseDown(&rect, tic_mouse_left))
            {
                s32 my = tic_api_mouse(tic).y - rect.y - Border;
                music->frame = my / TIC_FONT_HEIGHT;
            }
        }

        drawEditPanel(music, rect.x, rect.y, rect.w, rect.h);
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

            drawBitIcon(x - TIC_FONT_WIDTH-1, y + i*TIC_FONT_HEIGHT, Icon, tic_color_black);
            drawBitIcon(x - TIC_FONT_WIDTH-1, y - 1 + i*TIC_FONT_HEIGHT, Icon, tic_color_white);
        }

        char buf[] = "99";
        sprintf(buf, "%02i", i);

        tic_api_print(music->tic, buf, x, y + i*TIC_FONT_HEIGHT, i == music->frame ? tic_color_white : tic_color_grey, true, 1, false);
    }

    if(music->tracker.edit.y >= 0)
    {
        char buf[] = "99";
        sprintf(buf, "%02i", music->tracker.edit.y);
        tic_api_print(music->tic, buf, x, y - 10, tic_color_black, true, 1, false);
        tic_api_print(music->tic, buf, x, y - 11, tic_color_white, true, 1, false);
    }
}

static void setChannelPattern(Music* music, s32 delta, s32 channel)
{
    tic_track* track = getTrack(music);
    s32 frame = music->frame;

    u32 patternData = 0;
    for(s32 b = 0; b < TRACK_PATTERNS_SIZE; b++)
        patternData |= track->data[frame * TRACK_PATTERNS_SIZE + b] << (BITS_IN_BYTE * b);

    s32 shift = channel * TRACK_PATTERN_BITS;
    s32 patternId = (patternData >> shift) & TRACK_PATTERN_MASK;

    setChannelPatternValue(music, patternId + delta, music->frame, channel);
}

static inline void drawChar(tic_mem* tic, char symbol, s32 x, s32 y, u8 color, bool alt)
{
    tic_api_print(tic, (char[]){symbol, '\0'}, x, y, color, true, 1, alt);
}

static inline bool noteBeat(Music* music, s32 row)
{
    return row % (music->beat34 ? 3 : 4) == 0;
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

    tic_rect rect = {x - Border, y - Border, Width, Rows*TIC_FONT_HEIGHT + Border};

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        if(checkMouseDown(&rect, tic_mouse_left))
        {
            s32 mx = tic_api_mouse(tic).x - rect.x - Border;
            s32 my = tic_api_mouse(tic).y - rect.y - Border;

            s32 col = music->tracker.edit.x = channel * CHANNEL_COLS + mx / TIC_FONT_WIDTH;
            s32 row = music->tracker.edit.y = my / TIC_FONT_HEIGHT + music->scroll.pos;

            if(music->tracker.select.drag)
            {
                updateSelection(music);
            }
            else
            {
                resetSelection(music);
                music->tracker.select.start = (tic_point){col, row};

                music->tracker.select.drag = true;
            }
        }
    }

    if(music->tracker.select.drag)
    {
        tic_rect rect = {0, 0, TIC80_WIDTH, TIC80_HEIGHT};
        if(!checkMouseDown(&rect, tic_mouse_left))
        {
            music->tracker.select.drag = false;
        }
    }

    drawEditPanel(music, rect.x, rect.y, rect.w, rect.h);

    s32 start = music->scroll.pos;
    s32 end = start + Rows;
    bool selectedChannel = music->tracker.select.rect.x / CHANNEL_COLS == channel;

    tic_track_pattern* pattern = getPattern(music, channel);

    for (s32 i = start, pos = 0; i < end; i++, pos++)
    {
        s32 rowy = y + pos*TIC_FONT_HEIGHT;

        if (i == music->tracker.edit.y)
        {
            tic_api_rect(music->tic, x - 1, rowy - 1, Width, TIC_FONT_HEIGHT + 1, tic_color_dark_grey);
        }

        // draw selection
        if (selectedChannel)
        {
            tic_rect rect = music->tracker.select.rect;
            if (rect.h > 1 && i >= rect.y && i < rect.y + rect.h)
            {
                s32 sx = x - 1;
                tic_api_rect(tic, sx, rowy - 1, CHANNEL_COLS * TIC_FONT_WIDTH + 1, TIC_FONT_HEIGHT + 1, tic_color_grey);
            }
        }

        if (checkPlayRow(music, i))
        {
            tic_api_rect(music->tic, x - 1, rowy - 1, Width, TIC_FONT_HEIGHT + 1, tic_color_white);
        }

        char rowStr[] = "--------";

        if (pattern)
        {
            static const char* Notes[] = SFX_NOTES;

            const tic_track_row* row = &pattern->rows[i];
            s32 note = row->note;
            s32 octave = row->octave;
            s32 sfx = tic_tool_get_track_row_sfx(&pattern->rows[i]);

            if (note == NoteStop)
                strcpy(rowStr, "     ---");

            if (note >= NoteStart)
                sprintf(rowStr, "%s%i%02i---", Notes[note - NoteStart], octave + 1, sfx);

            if(row->command > tic_music_cmd_empty)
                sprintf(rowStr+5, "%c%01X%01X", MusicCommands[row->command], row->param1, row->param2);

            const u8 Colors[] = { tic_color_light_green, tic_color_yellow, tic_color_light_blue };
            const u8 DarkColors[] = { tic_color_green, tic_color_orange, tic_color_blue };
            static u8 ColorIndexes[] = { 0, 0, 0, 1, 1, 2, 2, 2 };

            bool beetRow = noteBeat(music, i);

            for (s32 c = 0, colx = x; c < sizeof rowStr - 1; c++, colx += TIC_FONT_WIDTH)
            {
                char sym = rowStr[c];
                const u8* colors = beetRow || sym != '-' ? Colors : DarkColors;

                drawChar(music->tic, sym, colx, rowy, colors[ColorIndexes[c]], false);
            }
        }
        else tic_api_print(music->tic, rowStr, x, rowy, i == music->tracker.edit.y ? tic_color_black : tic_color_dark_grey, true, 1, false);

        if (i == music->tracker.edit.y)
        {
            if (music->tracker.edit.x / CHANNEL_COLS == channel)
            {
                s32 col = music->tracker.edit.x % CHANNEL_COLS;
                s32 colx = x - 1 + col * TIC_FONT_WIDTH;
                tic_api_rect(music->tic, colx, rowy - 1, TIC_FONT_WIDTH + 1, TIC_FONT_HEIGHT + 1, tic_color_red);
                drawChar(music->tic, rowStr[col], colx + 1, rowy, tic_color_black, false);
            }
        }

        if (noteBeat(music, i))
            tic_api_pix(music->tic, x - 4, y + pos*TIC_FONT_HEIGHT + 2, tic_color_black, false);
    }
}

static void drawTumbler(Music* music, s32 x, s32 y, s32 index)
{
    tic_mem* tic = music->tic;

    enum{On=36, Off = 52, Width=7, Height=3};
    
    tic_rect rect = {x, y, Width, Height};

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        showTooltip("on/off channel");

        if(checkMouseClick(&rect, tic_mouse_left))
        {
            if (tic_api_key(tic, tic_key_ctrl))
            {
                for (s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
                    music->on[i] = i == index;
            }
            else music->on[index] = !music->on[index];
        }
    }

    drawEditPanel(music, x, y, Width, Height);

    u8 color = tic_color_black;
    tiles2ram(&tic->ram, &getConfig()->cart->bank0.tiles);
    tic_api_spr(tic, music->on[index] ? On : Off, x, y, 1, 1, &color, 1, 1, tic_no_flip, tic_no_rotate);
}

static void drawTrackerLayout(Music* music, s32 x, s32 y)
{
    drawTrackerFrames(music, x, y);

    x += TIC_FONT_WIDTH * 3;

    enum{ChannelWidth = TIC_FONT_WIDTH * 9};

    for (s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
    {
        s32 patternId = tic_tool_get_pattern_id(getTrack(music), music->frame, i);
        drawEditbox(music, x + ChannelWidth * i + 2*TIC_FONT_WIDTH, y - 12, patternId, setChannelPattern, i);
        drawTumbler(music, x + ChannelWidth * i + 7*TIC_FONT_WIDTH-1, y - 11, i);
    }

    for (s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
        drawTrackerChannel(music, x + ChannelWidth * i, y, i);
}

static void drawPlayButtons(Music* music)
{
    static const u8 Icons[] =
    {
        0b00000000,
        0b00100000,
        0b00010000,
        0b10111000,
        0b00010000,
        0b00100000,
        0b00000000,
        0b00000000,

        0b00000000,
        0b11110000,
        0b00001000,
        0b10001000,
        0b10000000,
        0b01111000,
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

    enum { Offset = TIC80_WIDTH - 54, Width = 7, Height = 7, Rows = 8, Count = sizeof Icons / Rows };

    for (s32 i = 0; i < Count; i++)
    {
        tic_rect rect = { Offset + Width * i, 0, Width, Height };

        bool over = false;

        if (checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            static const char* Tooltips[] = { "FOLLOW [ctrl+f]", "SUSTAIN NOTES", "PLAY FRAME [enter]", "PLAY TRACK", "STOP [enter]" };
            showTooltip(Tooltips[i]);

            static void(*const Handlers[])(Music*) = { toggleFollowMode, toggleSustainMode, playFrame, playTrack, stopTrack };

            if (checkMouseClick(&rect, tic_mouse_left))
                Handlers[i](music);
        }

        if(i == 0 && music->follow)
            drawBitIcon(rect.x, rect.y, Icons + i*Rows, tic_color_green);
        else if(i == 1 && music->sustain)
            drawBitIcon(rect.x, rect.y, Icons + i*Rows, tic_color_green);
        else
            drawBitIcon(rect.x, rect.y, Icons + i*Rows, over ? tic_color_grey : tic_color_light_grey);
    }
}

static void drawModeTabs(Music* music)
{
    static const u8 Icons[] =
    {
        0b00000000,
        0b01111100,
        0b00000000,
        0b01011100,
        0b01011100,
        0b01011100,
        0b00000000,
        0b00000000,

        0b00000000,
        0b01111100,
        0b00000000,
        0b01010100,
        0b01010100,
        0b01010100,
        0b00000000,
        0b00000000,
    };

    enum { Width = 7, Height = 7, Rows = 8, Count = sizeof Icons / Rows };

    for (s32 i = 0; i < Count; i++)
    {
        tic_rect rect = { TIC80_WIDTH - Width * (Count - i), 0, Width, Height };


        static const s32 Tabs[] = { MUSIC_PIANO_TAB, MUSIC_TRACKER_TAB };

        bool over = false;

        if (checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            static const char* Tooltips[] = { "PIANO MODE", "TRACKER MODE" };
            showTooltip(Tooltips[i]);

            if (checkMouseClick(&rect, tic_mouse_left))
                music->tab = Tabs[i];
        }

        if (music->tab == Tabs[i])
        {
            tic_api_rect(music->tic, rect.x, rect.y, rect.w, rect.h, tic_color_grey);
            drawBitIcon(rect.x, rect.y + 1, Icons + i*Rows, tic_color_black);
        }

        drawBitIcon(rect.x, rect.y, Icons + i*Rows, music->tab == Tabs[i] ? tic_color_white : over ? tic_color_grey : tic_color_light_grey);
    }
}

static void drawMusicToolbar(Music* music)
{
    tic_api_rect(music->tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, tic_color_white);

    drawPlayButtons(music);
    drawModeTabs(music);
}

static void drawPianoCursor(Music* music, s32 x, s32 y, const char* val)
{
    tic_mem* tic = music->tic;

    s32 subCol = music->piano.edit.x & 1;
    tic_point pos = {x + subCol * TIC_FONT_WIDTH, y};
    tic_api_rect(tic, pos.x - 1, pos.y - 1, TIC_FONT_WIDTH + 1, TIC_FONT_HEIGHT + 1, tic_color_red);
    tic_api_print(tic, (char[]){val[subCol], '\0'}, pos.x, pos.y, tic_color_black, true, 1, false);
}

static const char* getPatternLabel(Music* music, s32 frame, s32 channel)
{
    static char index[sizeof "--"];

    strcpy(index, "--");

    s32 pattern = tic_tool_get_pattern_id(getTrack(music), frame, channel);

    if(pattern)
        sprintf(index, "%02i", pattern);

    return index;
}

static void drawPianoFrames(Music* music, s32 x, s32 y)
{
    tic_mem* tic = music->tic;

    enum {Width = 66, Height = 106, Header = 10, ColWidth = TIC_FONT_WIDTH * 2 + 1};

    drawEditPanel(music, x, y, Width, Height);

    tic_api_print(tic, "FRM", x + 1, y + 2, tic_color_grey, true, 1, true);

    {
        const tic_sound_state* pos = getMusicPos(music);
        s32 playFrame = pos->music.track == music->track ? pos->music.frame : -1;

        char index[] = "99";
        for(s32 i = 0; i < MUSIC_FRAMES; i++)
        {
            sprintf(index, "%02i", i);
            tic_api_print(tic, index, x + 1, y + Header + i * TIC_FONT_HEIGHT, playFrame == i ? tic_color_white : music->frame == i? tic_color_grey : tic_color_dark_grey, true, 1, false);
        }

        if(playFrame >= 0)
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

            drawBitIcon(x - TIC_ALTFONT_WIDTH, y + playFrame * TIC_FONT_HEIGHT + Header, Icon, tic_color_black);
            drawBitIcon(x - TIC_ALTFONT_WIDTH, y + playFrame * TIC_FONT_HEIGHT + (Header - 1), Icon, tic_color_white);                
        }
    }

    x += ColWidth + 1;

    {
        tic_rect rect = {x, y + Header - 1, (TIC_FONT_WIDTH * 2 + 1) * TIC_SOUND_CHANNELS, MUSIC_FRAMES * TIC_FONT_HEIGHT + 1};

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            if(checkMouseClick(&rect, tic_mouse_left))
            {
                s32 col = (tic_api_mouse(tic).x - rect.x) * TIC_SOUND_CHANNELS / rect.w;
                s32 row = (tic_api_mouse(tic).y - rect.y) * MUSIC_FRAMES / rect.h;

                // move edit cursor if pattern already selected only 
                if(col == music->piano.col && row == music->frame)
                {
                    music->piano.edit.x = (tic_api_mouse(tic).x - rect.x) * TIC_SOUND_CHANNELS * 2 / rect.w;
                    music->piano.edit.y = row;
                }

                music->piano.col = col;

                if(getMusicState(music) == tic_music_stop || !music->follow)
                    music->frame = row;
            }
        }
    }

    for(s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
    {
        tic_api_rect(tic, x + c * ColWidth, y + 1, 
            ColWidth, MUSIC_FRAMES * TIC_FONT_HEIGHT + (Header - 1), c & 1 ? tic_color_black : tic_color_dark_grey);

        tic_api_print(tic, (char[]){'1' + c, '\0'}, x + (ColWidth - (TIC_ALTFONT_WIDTH - 1)) / 2 + c * ColWidth, y + 2, 
            tic_color_grey, true, 1, true);


        for(s32 i = 0; i < MUSIC_FRAMES; i++)
        {
            const char* index = getPatternLabel(music, i, c);

            tic_point pos = {x + 1 + c * ColWidth, y + Header + i * TIC_FONT_HEIGHT};
            tic_api_print(tic, index, pos.x, pos.y, c & 1 ? tic_color_dark_grey : tic_color_grey, true, 1, false);
        }

        drawTumbler(music, x + 3 + c * ColWidth, y + 110, c);
    }

    {
        const char* index = getPatternLabel(music, music->frame, music->piano.col);

        tic_point pos = {x + 1 + music->piano.col * ColWidth, y + Header + music->frame * TIC_FONT_HEIGHT};
        tic_api_print(tic, index, pos.x, pos.y + 1, tic_color_black, true, 1, false);
        tic_api_print(tic, index, pos.x, pos.y, tic_color_white, true, 1, false);

    }

    // draw edit cursor
    switch(music->piano.edit.x / 2)
    {
    case PianoChannel1Column:
    case PianoChannel2Column:
    case PianoChannel3Column:
    case PianoChannel4Column:
        {
            const char* label = getPatternLabel(music, music->piano.edit.y, music->piano.edit.x / 2);
            drawPianoCursor(music, x + 1 + music->piano.edit.x / 2 * ColWidth, y + Header + music->piano.edit.y * TIC_FONT_HEIGHT, label);
        }
    }
}

static void drawStereoSeparator(Music* music, s32 x, s32 y)
{
    tic_mem* tic = music->tic;
    static u8 Colors[] = 
    {
        tic_color_dark_green, tic_color_green, tic_color_light_green, tic_color_light_green
    };

    for(s32 i = 0; i < COUNT_OF(Colors); i++)
        tic_api_rect(tic, x + i * 4, y, 4, 1, Colors[i]);

    for(s32 i = COUNT_OF(Colors)-1; i >= 0; i--)
        tic_api_rect(tic, x + 27 - i * 4, y, 4, 1, Colors[i]);
}

static void drawPianoRoll(Music* music, s32 x, s32 y)
{
    tic_mem* tic = music->tic;

    static const struct Button {u8 note; u8 up; u8 down; u8 offset; u8 flip;} Buttons[] = 
    {
        {0, 39, 40, 0, tic_no_flip},
        {2, 39, 40, 3, tic_horz_flip},
        {2, 39, 40, 8, tic_no_flip},
        {4, 39, 40, 11, tic_horz_flip},
        {5, 39, 40, 20, tic_no_flip},
        {7, 39, 40, 23, tic_horz_flip},
        {7, 39, 40, 28, tic_no_flip},
        {9, 39, 40, 31, tic_horz_flip},
        {9, 39, 40, 36, tic_no_flip},
        {11, 39, 40, 39, tic_horz_flip},
        {1, 41, 42, 0, tic_no_flip},
        {3, 41, 42, 8, tic_no_flip},
        {6, 41, 42, 20, tic_no_flip},
        {8, 41, 42, 28, tic_no_flip},
        {10, 41, 42, 36, tic_no_flip},
    };

    tiles2ram(&tic->ram, &getConfig()->cart->bank0.tiles);

    for(s32 i = 0; i < COUNT_OF(Buttons); i++)
    {
        const struct Button* btn = &Buttons[i];
        tic_api_spr(tic, btn->note == music->piano.note[music->piano.col] ? btn->down : btn->up, 
            x + btn->offset, y, 1, 1, (u8[]){tic_color_orange}, 1, 1, btn->flip, tic_no_rotate);
    }
}

static void drawPianoRowColumn(Music* music, s32 x, s32 y)
{
    tic_mem* tic = music->tic;
    const tic_track_pattern* pattern = getFramePattern(music, music->piano.col, music->frame);

    enum{Header = PIANO_PATTERN_HEADER};

    tic_rect rect = {x, y, TIC_FONT_WIDTH*2 + 1, TRACKER_ROWS*TIC_FONT_HEIGHT + Header};

    if(checkMousePos(&rect))
    {
        if(checkMouseDown(&rect, tic_mouse_left) || checkMouseDown(&rect, tic_mouse_right))
        {
            setCursor(tic_cursor_hand);

            if(music->scroll.active)
            {
                music->scroll.pos = (music->scroll.start - tic_api_mouse(tic).y) / TIC_FONT_HEIGHT;
                updateScroll(music);
            }
            else
            {
                music->scroll.active = true;
                music->scroll.start = tic_api_mouse(tic).y + music->scroll.pos * TIC_FONT_HEIGHT;
            }            
        }
        else music->scroll.active = false;
    }

    tic_api_print(tic, "ROW", x + 1, y + 2, tic_color_grey, true, 1, true);

    for(s32 r = 0; r < TRACKER_ROWS; r++)
    {
        s32 index = rowIndex(music, r);

        char label[sizeof "00"];
        sprintf(label, "%02i", index);
        tic_api_print(tic, label, x + 1, y + Header + r * TIC_FONT_HEIGHT, pattern && noteBeat(music, index) ? tic_color_grey : tic_color_dark_grey, true, 1, false);
    }
}

static void drawPianoNoteStatus(Music* music, s32 x, s32 y, s32 xpos, s32 ypos)
{
    tic_mem* tic = music->tic;

    enum{Header = PIANO_PATTERN_HEADER, NoteWidth = 4, NoteHeight = TIC_FONT_HEIGHT};

    tic_rect rect = {x, y + Header, NoteWidth * NOTES - 1, NoteHeight * TRACKER_ROWS - 1};

    if(checkMousePos(&rect))
    {
        {
            static const char Notes[] = "C D EF G A B";
            tic_api_print(tic, Notes, xpos, ypos, tic_color_dark_grey, true, 1, true);            
        }

        showTooltip("set note");

        static const char* Notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        static const s32 Offsets[] = {0, 0, 2, 2, 4, 5, 5, 7, 7, 9, 9, 11};
        s32 note = (tic_api_mouse(tic).x - rect.x) / NoteWidth;

        tic_api_print(tic, Notes[note], xpos + Offsets[note] * NoteWidth, ypos, tic_color_yellow, true, 1, true);
    }
}

static void drawPianoNoteColumn(Music* music, s32 x, s32 y)
{
    tic_mem* tic = music->tic;

    enum{Header = PIANO_PATTERN_HEADER, NoteWidth = 4, NoteHeight = TIC_FONT_HEIGHT};

    drawPianoRoll(music, x, y + 1);

    tic_track_pattern* pattern = getFramePattern(music, music->piano.col, music->frame);
    if(pattern)
    {
        drawPianoNoteStatus(music, x, y, 87, 129);

        for(s32 r = 0; r < TRACKER_ROWS; r++)
        {
            s32 index = rowIndex(music, r);

            tic_track_row* row = &pattern->rows[index];

            // draw piano roll
            for(s32 n = 0; n < NOTES; n++)
            {
                tic_rect rect = {x + n * NoteWidth, y + Header + r * NoteHeight, NoteWidth - 1, NoteHeight - 1};

                bool over = false;
                if(checkMousePos(&rect))
                {
                    setCursor(tic_cursor_hand);
                    over = true;

                    if(checkMouseClick(&rect, tic_mouse_left))
                    {
                        switch(row->note)
                        {
                        case NoteNone:
                            row->note = NoteStart + n;
                            row->octave = music->last.octave;
                            tic_tool_set_track_row_sfx(row, music->last.sfx);
                            playNote(music, row);
                            break;
                        case NoteStop:
                            row->note = NoteNone;
                            row->octave = 0;
                            break;
                        default:
                            if(row->note - NoteStart == n)
                            {
                                row->note = NoteStop;
                                row->octave = 0;
                            }
                            else
                            {
                                row->note = NoteStart + n;
                                playNote(music, row);
                            }
                        }

                        history_add(music->history);
                    }
                    else if(checkMouseClick(&rect, tic_mouse_right))
                    {
                        switch(row->note)
                        {
                        case NoteNone:
                            row->note = NoteStop;
                            row->octave = 0;
                            break;
                        case NoteStop:
                            row->note = NoteStart + n;
                            row->octave = music->last.octave;
                            tic_tool_set_track_row_sfx(row, music->last.sfx);
                            playNote(music, row);
                            break;
                        default:
                            if(row->note - NoteStart == n)
                            {
                                row->note = NoteNone;
                                row->octave = 0;
                            }
                            else
                            {
                                row->note = NoteStart + n;
                                playNote(music, row);
                            }
                        }

                        history_add(music->history);
                    }
                }

                if(row->note == NoteStop)
                {
                    // draw stop note
                    tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_dark_grey);
                    tic_api_rect(tic, x + 1 + n * NoteWidth, y + r * NoteHeight + (Header + NoteHeight / 2 - 1), 1, 1, tic_color_red);
                }
                else tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, over ? tic_color_grey : tic_color_dark_grey);
            }

            if(noteBeat(music, index) && row->note != NoteStop)
            {
                static u8 Colors[NOTES] = 
                {
                    tic_color_grey, tic_color_grey, tic_color_light_grey, 
                    tic_color_light_grey, tic_color_light_grey, tic_color_white, 
                    tic_color_white, tic_color_light_grey, tic_color_light_grey, 
                    tic_color_light_grey, tic_color_grey, tic_color_grey
                };

                for(s32 i = 0; i < COUNT_OF(Colors); i++)
                    tic_api_rect(tic, x + i * NoteWidth, y + r * NoteHeight + (Header + NoteHeight / 2 - 1), NoteWidth - 1, 1, Colors[i]);
            }

            if (row->note >= NoteStart)
                tic_api_rect(tic, x + (row->note - NoteStart) * NoteWidth, y + Header + r * NoteHeight, NoteWidth - 1, NoteHeight - 1, tic_color_light_green);
        }
    }
    else
        for(s32 r = 0; r < TRACKER_ROWS; r++)
            for(s32 i = 0; i < NOTES; i++)
                tic_api_rect(tic, x + i * NoteWidth, y + r * NoteHeight + (Header + NoteHeight / 2 - 1), NoteWidth - 1, 1, tic_color_dark_grey);
}

static void drawPianoOctaveStatus(Music* music, s32 x, s32 y, s32 xpos, s32 ypos)
{
    tic_mem* tic = music->tic;

    enum{Header = PIANO_PATTERN_HEADER, OctaveWidth = 4, OctaveHeight = TIC_FONT_HEIGHT};

    tic_rect rect = {x, y + Header, OctaveWidth * OCTAVES - 1, OctaveHeight * TRACKER_ROWS - 1};

    if(checkMousePos(&rect))
    {
        s32 octave = (tic_api_mouse(tic).x - rect.x) / OctaveWidth;
        s32 r = (tic_api_mouse(tic).y - rect.y) / OctaveHeight;

        tic_track_pattern* pattern = getFramePattern(music, music->piano.col, music->frame);
        const tic_track_row* row = &pattern->rows[rowIndex(music, r)];

        if(row->note >= NoteStart)
        {
            showTooltip("set octave");

            tic_api_print(tic, "12345678", xpos, ypos, tic_color_dark_grey, true, 1, true);
            tic_api_print(tic, (char[]){octave + '1', '\0'}, xpos + octave * OctaveWidth, ypos, tic_color_yellow, true, 1, true);
        }
    }
}

static void drawPianoOctaveColumn(Music* music, s32 x, s32 y)
{
    tic_mem* tic = music->tic;

    enum{Header = PIANO_PATTERN_HEADER, OctaveWidth = 4, OctaveHeight = TIC_FONT_HEIGHT};

    tic_api_print(tic, "OCTAVE", x + 4, y + 2, tic_color_grey, true, 1, true);
    drawStereoSeparator(music, x, y + 8);

    tic_track_pattern* pattern = getFramePattern(music, music->piano.col, music->frame);
    if(pattern)
    {
        drawPianoOctaveStatus(music, x, y, 136, 129);

        for(s32 r = 0; r < TRACKER_ROWS; r++)
        {
            s32 index = rowIndex(music, r);
            tic_track_row* row = &pattern->rows[index];

            if(row->note >= NoteStart)
            {
                for(s32 n = 0; n < OCTAVES; n++)
                {
                    tic_rect rect = {x + n * OctaveWidth, y + Header + r * OctaveHeight, OctaveWidth - 1, OctaveHeight - 1};

                    bool over = false;
                    if(checkMousePos(&rect))
                    {
                        setCursor(tic_cursor_hand);
                        over = true;

                        if(checkMouseClick(&rect, tic_mouse_left))
                        {
                            music->last.octave = row->octave = n;
                            history_add(music->history);
                            playNote(music, row);
                        }
                    }

                    tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, over ? tic_color_grey : tic_color_dark_grey);
                }
            }
            else
                for(s32 i = 0; i < OCTAVES; i++)
                    tic_api_rect(tic, x + i * OctaveWidth, y + r * OctaveHeight + (Header + OctaveHeight / 2 - 1), OctaveWidth - 1, 1, tic_color_dark_grey);                

            if(noteBeat(music, index))
            {
                static u8 Colors[OCTAVES] = 
                {
                    tic_color_grey, tic_color_grey, tic_color_light_grey, tic_color_white,
                    tic_color_white, tic_color_light_grey, tic_color_grey, tic_color_grey
                };

                for(s32 i = 0; i < COUNT_OF(Colors); i++)
                    tic_api_rect(tic, x + i * OctaveWidth, y + r * OctaveHeight + (Header + OctaveHeight / 2 - 1), OctaveWidth - 1, 1, Colors[i]);
            }

            if(row->note >= NoteStart)
                tic_api_rect(tic, x + row->octave * OctaveWidth, y + Header + r * OctaveHeight, OctaveWidth - 1, OctaveHeight - 1, tic_color_orange);
        }
    }
    else 
        for(s32 r = 0; r < TRACKER_ROWS; r++)
            for(s32 i = 0; i < OCTAVES; i++)
                tic_api_rect(tic, x + i * OctaveWidth, y + r * OctaveHeight + (Header + OctaveHeight / 2 - 1), OctaveWidth - 1, 1, tic_color_dark_grey);
}

static void drawPianoSfxColumn(Music* music, s32 x, s32 y)
{
    tic_mem* tic = music->tic;

    enum{Header = PIANO_PATTERN_HEADER};

    {
        tic_rect rect = {x, y + Header - 1, TIC_FONT_WIDTH * 2, TIC_FONT_HEIGHT * MUSIC_FRAMES};

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            showTooltip("set sfx");

            if(checkMouseClick(&rect, tic_mouse_left))
            {
                music->piano.edit.x = PianoSfxColumn * 2 + (tic_api_mouse(tic).x - rect.x) / TIC_FONT_WIDTH;
                music->piano.edit.y = (tic_api_mouse(tic).y - rect.y) / TIC_FONT_HEIGHT;
            }
        }
    }

    tic_api_rect(tic, x, y + 1, TIC_FONT_WIDTH*2 + 1, Header + TRACKER_ROWS * TIC_FONT_HEIGHT - 1, tic_color_dark_grey);
    tic_api_print(tic, "SFX", x + 1, y + 2, tic_color_grey, true, 1, true);

    const tic_track_pattern* pattern = getFramePattern(music, music->piano.col, music->frame);
    if(pattern)
    {
        for(s32 r = 0; r < TRACKER_ROWS; r++)
        {
            s32 index = rowIndex(music, r);
            const tic_track_row* row = &pattern->rows[index];

            if (row->note >= NoteStart)
            {
                tic_rect rect = {x, y + Header + r * TIC_FONT_HEIGHT - 1, TIC_FONT_WIDTH*2+1, TIC_FONT_HEIGHT+1};

                char sfx[sizeof "00"];
                sprintf(sfx, "%02i", tic_tool_get_track_row_sfx(row));
                tic_api_print(tic, sfx, rect.x + 1, rect.y + 2, tic_color_black, true, 1, false);
                tic_api_print(tic, sfx, rect.x + 1, rect.y + 1, tic_color_yellow, true, 1, false);
            }
            else
                tic_api_print(tic, "--", 
                    x + 1, y + Header + r * TIC_FONT_HEIGHT, 
                    noteBeat(music, index) ? tic_color_light_grey : tic_color_grey, true, 1, false);
        }        
    }
    else
        for(s32 r = 0; r < TRACKER_ROWS; r++)
            tic_api_print(tic, "--", x + 1, y + Header + r * TIC_FONT_HEIGHT, tic_color_grey, true, 1, false);

    if(music->piano.edit.x / 2 == PianoSfxColumn)
    {
        char sfx[] = "--";
        const tic_track_row* row = getPianoRow(music);
        if (row && row->note >= NoteStart)
            sprintf(sfx, "%02i", tic_tool_get_track_row_sfx(row));

        drawPianoCursor(music, x + 1, y + Header + music->piano.edit.y * TIC_FONT_HEIGHT, sfx);
    }
}

static void drawPianoCommandColumn(Music* music, s32 x, s32 y)
{
    tic_mem* tic = music->tic;

    enum{Header = PIANO_PATTERN_HEADER};

    tic_track_pattern* pattern = getFramePattern(music, music->piano.col, music->frame);
    tic_music_command command = tic_music_cmd_empty;
    s32 overRow = -1;
    if(pattern)
    {
        tic_rect rect = {x, y + Header - 1, TIC_FONT_WIDTH * (tic_music_cmd_count - 1), TIC_FONT_HEIGHT * MUSIC_FRAMES};

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            showTooltip("set command");

            command = (tic_api_mouse(tic).x - rect.x) / TIC_FONT_WIDTH + 1;
            overRow = (tic_api_mouse(tic).y - rect.y) / TIC_FONT_HEIGHT;

            if(command != tic_music_cmd_empty)
            {
                #define MUSIC_CMD_HINT(_, letter, hint) "[" #letter "] " hint,
                static const char* Hints[] = 
                {
                    MUSIC_CMD_LIST(MUSIC_CMD_HINT)
                };
                #undef MUSIC_CMD_HINT

                tic_api_print(tic, Hints[command], 73, 129, tic_color_yellow, false, 1, true);
            }

            if(checkMouseClick(&rect, tic_mouse_left))
            {
                tic_track_row* row = &pattern->rows[rowIndex(music, overRow)];

                row->command = command != row->command ? command : tic_music_cmd_empty;

                if(row->command == tic_music_cmd_empty)
                    row->param1 = row->param2 = 0;
                else
                    setCommandDefaults(row);

                history_add(music->history);
            }
        }
    }

    tic_api_print(tic, "COMMAND", x + 8, y + 2, tic_color_grey, true, 1, true);
    drawStereoSeparator(music, x + 6, y + 8);

    if(pattern)
    {
        for(s32 r = 0; r < TRACKER_ROWS; r++)
        {
            s32 index = rowIndex(music, r);
            const tic_track_row* row = &pattern->rows[index];

            tic_api_print(tic, MusicCommands + 1, 
                x + 1, y + Header + r * TIC_FONT_HEIGHT, 
                noteBeat(music, index) ? tic_color_grey : tic_color_dark_grey, true, 1, false);

            if(overRow == r && command > tic_music_cmd_empty)
                tic_api_print(tic, (char[]){MusicCommands[command], '\0'}, 
                    x + 1 + (command - 1) * TIC_FONT_WIDTH, y + Header + r * TIC_FONT_HEIGHT, 
                    noteBeat(music, index) ? tic_color_light_grey : tic_color_grey, true, 1, false);

            if(row->command > tic_music_cmd_empty)
                tic_api_print(tic, (char[]){MusicCommands[row->command], '\0'}, 
                    x + 1 + (row->command - 1) * TIC_FONT_WIDTH, y + Header + r * TIC_FONT_HEIGHT, 
                    tic_color_light_blue, true, 1, false);
        }        
    }
    else
        for(s32 r = 0; r < TRACKER_ROWS; r++)
            tic_api_print(tic, MusicCommands + 1, 
                x + 1, y + Header + r * TIC_FONT_HEIGHT, 
                tic_color_dark_grey, true, 1, false);
}

static void drawPianoXYColumn(Music* music, s32 x, s32 y)
{
    tic_mem* tic = music->tic;

    enum{Header = PIANO_PATTERN_HEADER};

    const tic_track_pattern* pattern = getFramePattern(music, music->piano.col, music->frame);
    {
        tic_rect rect = {x, y + Header - 1, TIC_FONT_WIDTH * 2, TIC_FONT_HEIGHT * MUSIC_FRAMES};

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            showTooltip("set command XY");

            if(pattern)
            {
                s32 r = (tic_api_mouse(tic).y - rect.y) / TIC_FONT_HEIGHT;
                const tic_track_row* row = &pattern->rows[rowIndex(music, r)];

                if(row->command != tic_music_cmd_empty)
                {
                    char val[sizeof "XY=000"];
                    sprintf(val, "XY=%03i", (row->param1 << 4) | row->param2);
                    tic_api_print(tic, val, 213, 129, tic_color_yellow, false, 1, true);
                }                
            }

            if(checkMouseClick(&rect, tic_mouse_left))
            {
                music->piano.edit.x = PianoXYColumn * 2 + (tic_api_mouse(tic).x - rect.x) / TIC_FONT_WIDTH;
                music->piano.edit.y = (tic_api_mouse(tic).y - rect.y) / TIC_FONT_HEIGHT;
            }
        }
    }

    tic_api_rect(tic, x, y + 1, TIC_FONT_WIDTH*2 + 1, Header + TRACKER_ROWS * TIC_FONT_HEIGHT - 1, tic_color_dark_grey);
    tic_api_print(tic, "X", x + 2, y + 2, tic_color_grey, true, 1, true);
    tic_api_print(tic, "Y", x + 8, y + 2, tic_color_grey, true, 1, true);

    if(pattern)
    {

        for(s32 r = 0; r < TRACKER_ROWS; r++)
        {
            s32 index = rowIndex(music, r);
            const tic_track_row* row = &pattern->rows[index];

            tic_music_command command = MusicCommands[row->command];

            if(row->command > tic_music_cmd_empty)
            {
                char xy[sizeof "00"];
                tic_api_print(tic, xy, x + 1, y + Header + r * TIC_FONT_HEIGHT, tic_color_dark_grey, true, 1, false);
                sprintf(xy, "%01X%01X", row->param1, row->param2);
                tic_api_print(tic, xy, x + 1, y + Header + r * TIC_FONT_HEIGHT + 1, tic_color_black, true, 1, false);
                tic_api_print(tic, xy, x + 1, y + Header + r * TIC_FONT_HEIGHT, tic_color_light_blue, true, 1, false);
            }
            else
                tic_api_print(tic, "--", 
                    x + 1, y + Header + r * TIC_FONT_HEIGHT, 
                    noteBeat(music, index) ? tic_color_light_grey : tic_color_grey, true, 1, false);
        }        
    }
    else
        for(s32 r = 0; r < TRACKER_ROWS; r++)
            tic_api_print(tic, "--", x + 1, y + Header + r * TIC_FONT_HEIGHT, tic_color_grey, true, 1, false);

    if(music->piano.edit.x / 2 == PianoXYColumn)
    {
        char xy[] = "--";
        const tic_track_row* row = getPianoRow(music);
        if(row && row->command > tic_music_cmd_empty)
            sprintf(xy, "%01X%01X", row->param1, row->param2);

        drawPianoCursor(music, x + 1, y + Header + music->piano.edit.y * TIC_FONT_HEIGHT, xy);
    }
}

static void drawPianoPattern(Music* music, s32 x, s32 y)
{
    tic_mem* tic = music->tic;

    enum{Width = 164, Height = 106};
    drawEditPanel(music, x, y, Width, Height);

    // draw playing row
    if(checkPlayFrame(music, music->frame))
    {
        const tic_sound_state* pos = getMusicPos(music);
        s32 index = pos->music.row - music->scroll.pos;

        if(index >= 0 && index < TRACKER_ROWS)
            tic_api_rect(tic, x, y + PIANO_PATTERN_HEADER + index * TIC_FONT_HEIGHT - 1, Width, TIC_FONT_HEIGHT + 1, tic_color_light_grey);
    }

    drawPianoRowColumn(music,       x, y);
    drawPianoNoteColumn(music,      x + 14, y);
    drawPianoOctaveColumn(music,    x + 63, y);
    drawPianoSfxColumn(music,       x + 95, y);
    drawPianoCommandColumn(music,   x + 108, y);
    drawPianoXYColumn(music,        x + 151, y);
}

static void drawBeatButton(Music* music, s32 x, s32 y)
{
    tic_mem* tic = music->tic;

    static const char Label44[] = "4/4";
    static const char Label34[] = "3/4";

    tic_rect rect = {x, y, (sizeof Label44 - 1) * TIC_ALTFONT_WIDTH - 1, TIC_FONT_HEIGHT - 1};

    bool down = false;
    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);
        showTooltip(music->beat34 ? "set 4 quarter note" : "set 3 quarter note");

        if(checkMouseDown(&rect, tic_mouse_left))
            down = true;

        if(checkMouseClick(&rect, tic_mouse_left))
            music->beat34 = !music->beat34;
    }

    tic_api_print(tic, music->beat34 ? Label34 : Label44, x, y + 1, tic_color_black, true, 1, true);
    tic_api_print(tic, music->beat34 ? Label34 : Label44, x, y + (down ? 1 : 0), tic_color_white, true, 1, true);
}

static void drawPianoLayout(Music* music)
{
    drawPianoFrames(music, 3, 20);
    drawPianoPattern(music, 73, 20);
    drawBeatButton(music, 4, 129);
}

static void scrollNotes(Music* music, s32 delta)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    if(pattern)
    {
        tic_rect rect = music->tracker.select.rect;

        if(rect.h <= 0)
        {
            rect.y = music->tracker.edit.y;
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

static void drawWaveform(Music* music, s32 x, s32 y)
{
    tic_mem* tic = music->tic;

    enum{Width = 32, Height = 8, WaveRows = 1 << WAVE_VALUE_BITS};

    drawEditPanel(music, x, y, Width, Height);

    // detect playing channels
    s32 channels = 0;
    for(s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
        if(music->on[c] && tic->ram.registers[c].volume)
            channels++;

    if(channels)
    {
        for(s32 i = 0; i < WAVE_VALUES; i++)
        {
            s32 lamp = 0, ramp = 0;

            for(s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
            {
                s32 amp = calcWaveAnimation(tic, i + music->tickCounter, c) / channels;

                lamp += amp * tic_tool_peek4(&tic->ram.stereo.data, c*2);
                ramp += amp * tic_tool_peek4(&tic->ram.stereo.data, c*2 + 1);
            }

            lamp /= WAVE_MAX_VALUE * WAVE_MAX_VALUE;
            ramp /= WAVE_MAX_VALUE * WAVE_MAX_VALUE;

            tic_api_rect(tic, x + i, y + (Height-1) - ramp * Height / WaveRows, 1, 1, tic_color_yellow);
            tic_api_rect(tic, x + i, y + (Height-1) - lamp * Height / WaveRows, 1, 1, tic_color_light_green);
        }
    }
}

static void updatePianoRollState(Music* music)
{
    if(getMusicState(music) != tic_music_stop)
    {
        s32 channel = music->piano.col;
        const tic_sound_state* pos = getMusicPos(music);

        for(s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
        {
            const tic_track_pattern* pattern = getFramePattern(music, c, pos->music.frame);
            if(pattern)
            {
                const tic_track_row* row = &pattern->rows[pos->music.row];

                if(pos->music.row == 0 && !music->sustain)
                    music->piano.note[c] = -1;

                if(row->note >= NoteStart)
                    music->piano.note[c] = row->note - NoteStart;
                else if(row->note == NoteStop || (row->note == NoteNone && pos->music.row == 0 && !music->sustain))
                    music->piano.note[c] = -1;
            }
            else if(!music->sustain)
                music->piano.note[c] = -1;
        }
    }
    else memset(music->piano.note, -1, sizeof music->piano.note);
}

static void tick(Music* music)
{
    tic_mem* tic = music->tic;

    // process scroll
    {
        tic80_input* input = &tic->ram.input;

        if(input->mouse.scrolly)
        {
            if(tic_api_key(tic, tic_key_ctrl))
            {
                scrollNotes(music, input->mouse.scrolly > 0 ? 1 : -1);
            }
            else
            {       
                enum{Scroll = NOTES_PER_BEAT};
                s32 delta = input->mouse.scrolly > 0 ? -Scroll : Scroll;

                music->scroll.pos += delta;

                updateScroll(music);
            }
        }
    }

    processKeyboard(music);

    if(music->follow)
    {
        const tic_sound_state* pos = getMusicPos(music);

        if(pos->music.track == music->track && 
            music->tracker.edit.y >= 0 &&
            pos->music.row >= 0)
        {
            music->frame = pos->music.frame;
            music->tracker.edit.y = pos->music.row;
            updateTracker(music);
        }
    }

    for (s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
        if(!music->on[i])
            tic->ram.registers[i].volume = 0;

    updatePianoRollState(music);

    tic_api_cls(music->tic, tic_color_grey);
    drawTopPanel(music, 2, TOOLBAR_SIZE + 3);
    drawWaveform(music, 205, 9);

    switch (music->tab)
    {
    case MUSIC_TRACKER_TAB: drawTrackerLayout(music, 7, 35); break;
    case MUSIC_PIANO_TAB: drawPianoLayout(music); break;
    }

    drawMusicToolbar(music);
    drawToolbar(music->tic, false);

    music->tickCounter++;
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
        .beat34 = false,
        .frame = 0,
        .follow = true,
        .sustain = false,
        .scroll = 
        {
            .pos = 0,
            .start = 0,
            .active = false,
        },
        .last =
        {
            .octave = 3,
            .sfx = 0,
        },
        .on = {true, true, true, true},
        .tracker =
        {
            .edit = {0, 0},

            .select = 
            {
                .start = {0, 0},
                .rect = {0, 0, 0, 0},
                .drag = false,
            },
        },

        .piano =
        {
            .col = 0,
            .edit = {0, 0},
            .note = {-1, -1, -1, -1},
        },

        .tickCounter = 0,
        .tab = MUSIC_PIANO_TAB,
        .history = history_create(src, sizeof(tic_music)),
        .event = onStudioEvent,
    };

    resetSelection(music);
}

void freeMusic(Music* music)
{
    history_delete(music->history);
    free(music);
}
