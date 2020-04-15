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

#include <ctype.h>

#define TRACKER_ROWS (MUSIC_PATTERN_ROWS / 4)
#define CHANNEL_COLS 8
#define TRACKER_COLS (TIC_SOUND_CHANNELS * CHANNEL_COLS)

#define CMD_STRING(_, c, ...) DEF2STR(c)
static const char *MusicCommands = MUSIC_CMD_LIST(CMD_STRING);
#undef CMD_STRING

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

    tic_api_rect(tic, x, y-1, w, 1, tic_color_15);
    tic_api_rect(tic, x-1, y, 1, h, tic_color_15);
    tic_api_rect(tic, x, y+h, w, 1, tic_color_13);
    tic_api_rect(tic, x+w, y, 1, h, tic_color_13);
}

static void drawEditbox(Music* music, s32 x, s32 y, s32 value, void(*set)(Music*, s32, s32 channel), s32 channel)
{
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

        drawBitIcon(rect.x, rect.y + (down ? 2 : 1), LeftArrow, tic_color_0);
        drawBitIcon(rect.x, rect.y + (down ? 1 : 0), LeftArrow, (over ? tic_color_13 : tic_color_15));
    }

    {
        x += TIC_FONT_WIDTH;

        tic_rect rect = { x-1, y-1, TIC_FONT_WIDTH*2+1, TIC_FONT_HEIGHT+1 };

        if (checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            if (checkMouseClick(&rect, tic_mouse_left))
            {
                music->tracker.row = -1;
                music->tracker.col = channel * CHANNEL_COLS;

                s32 mx = getMouseX() - rect.x;
                music->tracker.patternCol = mx / TIC_FONT_WIDTH;
            }
        }

        tic_api_rect(music->tic, rect.x, rect.y, rect.w, rect.h, tic_color_0);
        drawEditPanel(music, rect.x, rect.y, rect.w, rect.h);

        if(music->tracker.row == -1 && music->tracker.col / CHANNEL_COLS == channel)
        {
            tic_api_rect(music->tic, x - 1 + music->tracker.patternCol * TIC_FONT_WIDTH, y - 1, TIC_FONT_WIDTH + 1, TIC_FONT_HEIGHT + 1, tic_color_2);
        }

        char val[] = "99";
        sprintf(val, "%02i", value);
        tic_api_print(music->tic, val, x, y, tic_color_12, true, 1, false);
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

        drawBitIcon(rect.x, rect.y + (down ? 2 : 1), RightArrow, tic_color_0);
        drawBitIcon(rect.x, rect.y + (down ? 1 : 0), RightArrow, (over ? tic_color_13 : tic_color_15));
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

    tic_api_print(music->tic, label, x, y+1, tic_color_0, true, 1, false);
    tic_api_print(music->tic, label, x, y, tic_color_12, true, 1, false);

    {
        x += (s32)strlen(label)*TIC_FONT_WIDTH;

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
                set(music, -1, data);
        }

        drawBitIcon(rect.x, rect.y + (down ? 2 : 1), LeftArrow, tic_color_0);
        drawBitIcon(rect.x, rect.y + (down ? 1 : 0), LeftArrow, over ? tic_color_13 : tic_color_15);
    }

    {
        char val[] = "999";
        sprintf(val, "%02i", value);
        tic_api_print(music->tic, val, x + TIC_FONT_WIDTH, y+1, tic_color_0, true, 1, false);
        tic_api_print(music->tic, val, x += TIC_FONT_WIDTH, y, tic_color_12, true, 1, false);
    }

    {
        x += (value > 99 ? 3 : 2)*TIC_FONT_WIDTH;

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
                set(music, +1, data);
        }

        drawBitIcon(rect.x, rect.y + (down ? 2 : 1), RightArrow, tic_color_0);
        drawBitIcon(rect.x, rect.y + (down ? 1 : 0), RightArrow, over ? tic_color_13 : tic_color_15);
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
    const tic_sound_state* pos = getMusicPos(music);
    if(pos->music.track == music->track && music->tracker.follow) return;

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
    const tic_sound_state* pos = getMusicPos(music);

    return pos->music.track == music->track &&
        pos->music.frame == frame;
}

static bool checkPlayRow(Music* music, s32 row)
{
    const tic_sound_state* pos = getMusicPos(music);

    return checkPlayFrame(music, music->tracker.frame) && pos->music.row == row;
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

static void setSfxId(tic_track_pattern* pattern, s32 row, s32 sfx)
{
    if(sfx >= SFX_COUNT) sfx = SFX_COUNT-1;

    tic_tool_set_track_row_sfx(&pattern->rows[row], sfx);
}

static s32 getSfx(Music* music)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    return tic_tool_get_track_row_sfx(&pattern->rows[music->tracker.row]);
}

static void setSfx(Music* music, s32 sfx)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    setSfxId(pattern, music->tracker.row, sfx);

    music->tracker.last.sfx = tic_tool_get_track_row_sfx(&pattern->rows[music->tracker.row]);
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

            sfx_stop(music->tic, channel);
            tic_api_sfx(music->tic, getSfx(music), note, getOctave(music), -1, channel, MAX_VOLUME, 0);
        }
    }
}

static void setStopNote(Music* music)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    pattern->rows[music->tracker.row].note = NoteStop;
    pattern->rows[music->tracker.row].octave = 0;
}

static void setNote(Music* music, s32 note, s32 octave, s32 sfx)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    pattern->rows[music->tracker.row].note = note + NoteStart;
    pattern->rows[music->tracker.row].octave = octave;
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

static void setCommand(Music* music, tic_music_command command)
{
    tic_track_pattern* pattern = getChannelPattern(music);
    tic_track_row* row = &pattern->rows[music->tracker.row];

    if(row->command == tic_music_cmd_empty)
    {
        switch(command)
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

    row->command = command;

    playNote(music);
}

static void setParam1(Music* music, u8 value)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    pattern->rows[music->tracker.row].param1 = value;

    playNote(music);
}

static void setParam2(Music* music, u8 value)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    pattern->rows[music->tracker.row].param2 = value;

    playNote(music);
}

static void playFrameRow(Music* music)
{
    tic_mem* tic = music->tic;

    tic_api_music(tic, music->track, music->tracker.frame, music->tracker.row, true);
    
    if(music->track >= 0)
        tic->ram.sound_state.flag.music_state = tic_music_play_frame;
}

static void playFrame(Music* music)
{
    tic_mem* tic = music->tic;

    tic_api_music(tic, music->track, music->tracker.frame, -1, true);

    if(music->track >= 0)
        tic->ram.sound_state.flag.music_state = tic_music_play_frame;
}

static void playTrack(Music* music)
{
    tic_api_music(music->tic, music->track, -1, -1, true);
}

static void stopTrack(Music* music)
{
    tic_api_music(music->tic, -1, -1, -1, false);
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
            rect.y = music->tracker.row;
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

static void copyToClipboard(Music* music, bool cut)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    if(pattern)
    {
        tic_rect rect = music->tracker.select.rect;

        if(rect.h <= 0)
        {
            rect.y = music->tracker.row;
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

static void copyFromClipboard(Music* music)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    if(pattern && getSystem()->hasClipboardText())
    {
        char* clipboard = getSystem()->getClipboardText();

        if(clipboard)
        {
            s32 size = strlen(clipboard)/2;

            enum{RowSize = sizeof(tic_track_pattern) / MUSIC_PATTERN_ROWS, HeaderSize = sizeof(ClipboardHeader)};

            if(size > HeaderSize)
            {
                u8* data = malloc(size);

                str2buf(clipboard, strlen(clipboard), data, true);

                ClipboardHeader header = {0};

                memcpy(&header, data, HeaderSize);

                if(header.size * RowSize == size - HeaderSize)
                {
                    if(header.size + music->tracker.row > MUSIC_PATTERN_ROWS)
                        header.size = MUSIC_PATTERN_ROWS - music->tracker.row;

                    memcpy(&pattern->rows[music->tracker.row], data + HeaderSize, header.size * RowSize);
                    history_add(music->history);
                }

                free(data);
            }

            getSystem()->freeClipboardText(clipboard);
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
    s32 rl = MIN(music->tracker.col, music->tracker.select.start.x);
    s32 rt = MIN(music->tracker.row, music->tracker.select.start.y);
    s32 rr = MAX(music->tracker.col, music->tracker.select.start.x);
    s32 rb = MAX(music->tracker.row, music->tracker.select.start.y);

    tic_rect* rect = &music->tracker.select.rect;
    *rect = (tic_rect){rl, rt, rr - rl + 1, rb - rt + 1};

    if(rect->x % CHANNEL_COLS + rect->w > CHANNEL_COLS)
        resetSelection(music);
}

static void processTrackerKeyboard(Music* music)
{
    tic_mem* tic = music->tic;

    if(tic->ram.input.keyboard.data == 0)
    {
        music->tracker.note = -1;
        s32 channel = music->tracker.col / CHANNEL_COLS;

        sfx_stop(tic, channel);
        return;
    }

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

    if(keyWasPressed(tic_key_up))           upRow(music);
    else if(keyWasPressed(tic_key_down))    downRow(music);
    else if(keyWasPressed(tic_key_left))    leftCol(music);
    else if(keyWasPressed(tic_key_right))   rightCol(music);
    else if(keyWasPressed(tic_key_home))    goHome(music);
    else if(keyWasPressed(tic_key_end))         goEnd(music);
    else if(keyWasPressed(tic_key_pageup))  pageUp(music);
    else if(keyWasPressed(tic_key_pagedown)) pageDown(music);
    else if(keyWasPressed(tic_key_tab))         doTab(music);
    else if(keyWasPressed(tic_key_delete))      
    {
        deleteSelection(music);
        history_add(music->history);
        downRow(music);
    }
    else if(keyWasPressed(tic_key_space)) playNote(music);
    else if(keyWasPressed(tic_key_return))
    {
        const tic_sound_state* pos = getMusicPos(music);
        pos->music.track < 0
            ? (shift ? playFrameRow(music) : playFrame(music))
            : stopTrack(music);        
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
        s32 col = music->tracker.col % CHANNEL_COLS;

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

                        if(pattern->rows[music->tracker.row].note > NoteNone)
                        {
                            pattern->rows[music->tracker.row].note = note + NoteStart;
                            playNote(music);
                        }
                        else
                        {
                            s32 octave = i / NOTES + music->tracker.last.octave;
                            s32 sfx = music->tracker.last.sfx;
                            setNote(music, note, octave, sfx);
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
                s32 val = -1;

                char sym = getKeyboardText();
                            
                if (sym >= '0' && sym <= '9') val = sym - '0';

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
        case ColumnCommand:
            {
                char sym = getKeyboardText();

                if(sym)
                {
                    const char* val = strchr(MusicCommands, sym);
                                
                    if(val)
                        setCommand(music, val - MusicCommands);
                }
            }
            break;
        case ColumnParameter1:
        case ColumnParameter2:
            {
                s32 val = -1;

                char sym = getKeyboardText();

                if (sym >= '0' && sym <= '9') val = sym - '0';
                if (sym >= 'a' && sym <= 'f') val = sym - 'a' + 10;

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
    s32 channel = music->tracker.col / CHANNEL_COLS;

    if(tic_api_key(tic, tic_key_ctrl) || tic_api_key(tic, tic_key_alt))
        return;

    if(keyWasPressed(tic_key_delete))       setChannelPatternValue(music, 0, channel);
    else if(keyWasPressed(tic_key_tab))     nextPattern(music);
    else if(keyWasPressed(tic_key_left))    patternColLeft(music);
    else if(keyWasPressed(tic_key_right))   patternColRight(music);
    else if(keyWasPressed(tic_key_down) 
        || keyWasPressed(tic_key_return)) 
        music->tracker.row = music->tracker.scroll;
    else
    {
        s32 val = -1;

        char sym = getKeyboardText();

        if(sym >= '0' && sym <= '9') val = sym - '0';

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

static void selectAll(Music* music)
{
    resetSelection(music);

    s32 col = music->tracker.col - music->tracker.col % CHANNEL_COLS;

    music->tracker.select.start = (tic_point){col, 0};
    music->tracker.col = col + CHANNEL_COLS-1;
    music->tracker.row = MUSIC_PATTERN_ROWS-1;

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

    if (ctrl)
    {
        if(keyWasPressed(tic_key_a))        selectAll(music);
        else if(keyWasPressed(tic_key_z))   undo(music);
        else if(keyWasPressed(tic_key_y))   redo(music);
        else if(keyWasPressed(tic_key_up))  upFrame(music);
        else if(keyWasPressed(tic_key_down)) downFrame(music);
    }
    else
    {
        music->tracker.row >= 0 
            ? processTrackerKeyboard(music)
            : processPatternKeyboard(music);
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
        tic_rect rect = { x - Border, y - Border, Width, MUSIC_FRAMES * TIC_FONT_HEIGHT + Border };

        if (checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            if (checkMouseDown(&rect, tic_mouse_left))
            {
                s32 my = getMouseY() - rect.y - Border;
                music->tracker.frame = my / TIC_FONT_HEIGHT;
            }
        }

        tic_api_rect(music->tic, rect.x, rect.y, rect.w, rect.h, tic_color_0);
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

            drawBitIcon(x - TIC_FONT_WIDTH-1, y + i*TIC_FONT_HEIGHT, Icon, tic_color_0);
            drawBitIcon(x - TIC_FONT_WIDTH-1, y - 1 + i*TIC_FONT_HEIGHT, Icon, tic_color_12);
        }

        char buf[] = "99";
        sprintf(buf, "%02i", i);

        if (i == music->tracker.frame)
        {
            tic_api_rect(music->tic, x - 1, y - 1 + i*TIC_FONT_HEIGHT, Width, TIC_FONT_HEIGHT + 1, tic_color_2);
        }

        tic_api_print(music->tic, buf, x, y + i*TIC_FONT_HEIGHT, i == music->tracker.frame ? tic_color_0 : tic_color_13, true, 1, false);
    }

    if(music->tracker.row >= 0)
    {
        char buf[] = "99";
        sprintf(buf, "%02i", music->tracker.row);
        tic_api_print(music->tic, buf, x, y - 10, tic_color_0, true, 1, false);
        tic_api_print(music->tic, buf, x, y - 11, tic_color_12, true, 1, false);
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

static inline void drawChar(tic_mem* tic, char symbol, s32 x, s32 y, u8 color, bool alt)
{
    tic_api_print(tic, (char[]){symbol, '\0'}, x, y, color, true, 1, alt);
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

    tic_api_rect(music->tic, rect.x, rect.y, rect.w, rect.h, tic_color_0);
    drawEditPanel(music, rect.x, rect.y, rect.w, rect.h);

    s32 start = music->tracker.scroll;
    s32 end = start + Rows;
    bool selectedChannel = music->tracker.select.rect.x / CHANNEL_COLS == channel;

    tic_track_pattern* pattern = getPattern(music, channel);

    for (s32 i = start, pos = 0; i < end; i++, pos++)
    {
        s32 rowy = y + pos*TIC_FONT_HEIGHT;

        if (i == music->tracker.row)
        {
            tic_api_rect(music->tic, x - 1, rowy - 1, Width, TIC_FONT_HEIGHT + 1, tic_color_15);
        }

        // draw selection
        if (selectedChannel)
        {
            tic_rect rect = music->tracker.select.rect;
            if (rect.h > 1 && i >= rect.y && i < rect.y + rect.h)
            {
                s32 sx = x - 1;
                tic_api_rect(tic, sx, rowy - 1, CHANNEL_COLS * TIC_FONT_WIDTH + 1, TIC_FONT_HEIGHT + 1, tic_color_14);
            }
        }

        if (checkPlayRow(music, i))
        {
            tic_api_rect(music->tic, x - 1, rowy - 1, Width, TIC_FONT_HEIGHT + 1, tic_color_12);
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
                sprintf(rowStr+5, "%c%01X%01X", toupper(MusicCommands[row->command]), row->param1, row->param2);

            const u8 Colors[] = { tic_color_5, tic_color_4, tic_color_10 };
            const u8 DarkColors[] = { tic_color_6, tic_color_3, tic_color_9 };
            static u8 ColorIndexes[] = { 0, 0, 0, 1, 1, 2, 2, 2 };

            bool beetRow = i % NOTES_PER_BEET == 0;

            for (s32 c = 0, colx = x; c < sizeof rowStr - 1; c++, colx += TIC_FONT_WIDTH)
            {
                char sym = rowStr[c];
                const u8* colors = beetRow || sym != '-' ? Colors : DarkColors;

                drawChar(music->tic, sym, colx, rowy, colors[ColorIndexes[c]], false);
            }
        }
        else tic_api_print(music->tic, rowStr, x, rowy, i == music->tracker.row ? tic_color_0 : tic_color_15, true, 1, false);

        if (i == music->tracker.row)
        {
            if (music->tracker.col / CHANNEL_COLS == channel)
            {
                s32 col = music->tracker.col % CHANNEL_COLS;
                s32 colx = x - 1 + col * TIC_FONT_WIDTH;
                tic_api_rect(music->tic, colx, rowy - 1, TIC_FONT_WIDTH + 1, TIC_FONT_HEIGHT + 1, tic_color_2);
                drawChar(music->tic, rowStr[col], colx + 1, rowy, tic_color_0, false);
            }
        }

        if (i % NOTES_PER_BEET == 0)
            tic_api_pix(music->tic, x - 4, y + pos*TIC_FONT_HEIGHT + 2, tic_color_0, false);
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
                    music->tracker.patterns[i] = i == index;
            }
            else music->tracker.patterns[index] = !music->tracker.patterns[index];
        }
    }

    drawEditPanel(music, x, y, Width, Height);
    tic_api_rect(tic, x, y, Width, Height, tic_color_0);

    u8 color = tic_color_0;
    tic_api_spr(tic, &getConfig()->cart->bank0.tiles, music->tracker.patterns[index] ? On : Off, x, y, 1, 1, &color, 1, 1, tic_no_flip, tic_no_rotate);
}

static void drawTracker(Music* music, s32 x, s32 y)
{
    drawTrackerFrames(music, x, y);

    x += TIC_FONT_WIDTH * 3;

    enum{ChannelWidth = TIC_FONT_WIDTH * 9};

    for (s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
    {
        s32 patternId = tic_tool_get_pattern_id(getTrack(music), music->tracker.frame, i);
        drawEditbox(music, x + ChannelWidth * i + 2*TIC_FONT_WIDTH, y - 11, patternId, setChannelPattern, i);
        drawTumbler(music, x + ChannelWidth * i + 7*TIC_FONT_WIDTH-1, y - 10, i);
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
        tic_rect rect = { Offset + Width * i, 0, Width, Height };

        bool over = false;

        if (checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            static const char* Tooltips[] = { "RECORD MUSIC", "PLAY FRAME [enter]", "PLAY TRACK", "STOP [enter]" };
            showTooltip(Tooltips[i]);

            static void(*const Handlers[])(Music*) = { enableFollowMode, playFrame, playTrack, stopTrack };

            if (checkMouseClick(&rect, tic_mouse_left))
                Handlers[i](music);
        }

        if(i == 0 && music->tracker.follow)
            drawBitIcon(rect.x, rect.y, Icons + i*Rows, over ? tic_color_2 : tic_color_2);
        else
            drawBitIcon(rect.x, rect.y, Icons + i*Rows, over ? tic_color_14 : tic_color_13);
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
            tic_api_rect(music->tic, rect.x, rect.y, rect.w, rect.h, tic_color_14);
            drawBitIcon(rect.x, rect.y + 1, Icons + i*Rows, tic_color_0);
        }

        drawBitIcon(rect.x, rect.y, Icons + i*Rows, music->tab == Tabs[i] ? tic_color_12 : over ? tic_color_14 : tic_color_13);
    }
}

static void drawMusicToolbar(Music* music)
{
    tic_api_rect(music->tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, tic_color_12);

    drawPlayButtons(music);
    drawModeTabs(music);
}

static void drawPianoLayout(Music* music)
{
    tic_api_cls(music->tic, tic_color_14);

    static const char Wip[] = "PIANO MODE - WORK IN PROGRESS...";
    tic_api_print(music->tic, Wip, (TIC80_WIDTH - (sizeof Wip - 1) * TIC_FONT_WIDTH) / 2, TIC80_HEIGHT / 2, tic_color_12, true, 1, false);
}

static void scrollNotes(Music* music, s32 delta)
{
    tic_track_pattern* pattern = getChannelPattern(music);

    if(pattern)
    {
        tic_rect rect = music->tracker.select.rect;

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
                enum{Scroll = NOTES_PER_BEET};
                s32 delta = input->mouse.scrolly > 0 ? -Scroll : Scroll;

                music->tracker.scroll += delta;

                updateScroll(music);
            }
        }
    }

    processKeyboard(music);

    if(music->tracker.follow)
    {
        const tic_sound_state* pos = getMusicPos(music);

        if(pos->music.track == music->track && 
            music->tracker.row >= 0 &&
            pos->music.row >= 0)
        {
            music->tracker.frame = pos->music.frame;
            music->tracker.row = pos->music.row;
            updateTracker(music);
        }
    }

    tic_api_cls(music->tic, tic_color_14);

    drawTopPanel(music, 7, TOOLBAR_SIZE + 4);
    drawTracker(music, 7, 35);
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
    drawToolbar(music->tic, false);
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
