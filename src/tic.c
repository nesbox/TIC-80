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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stddef.h>
#include <time.h>

#ifdef _3DS
#include <3ds.h>
#endif

#include "ticapi.h"
#include "tools.h"
#include "tilesheet.h"
#include "machine.h"
#include "ext/gif.h"
#include "cart.h"

#define CLOCKRATE (255<<13)
#define ENVELOPE_FREQ_SCALE 2
#define SECONDS_PER_MINUTE 60
#define NOTES_PER_MUNUTE (TIC80_FRAMERATE / NOTES_PER_BEAT * SECONDS_PER_MINUTE)
#define PIANO_START 8
#define TRANSPARENT_COLOR 255

STATIC_ASSERT(tic_bank_bits, TIC_BANK_BITS == 3);
STATIC_ASSERT(tic_map, sizeof(tic_map) < 1024*32);
STATIC_ASSERT(tic_sample, sizeof(tic_sample) == 66);
STATIC_ASSERT(tic_track_pattern, sizeof(tic_track_pattern) == 3*MUSIC_PATTERN_ROWS);
STATIC_ASSERT(tic_track, sizeof(tic_track) == 3*MUSIC_FRAMES+3);
STATIC_ASSERT(tic_vram, sizeof(tic_vram) == TIC_VRAM_SIZE);
STATIC_ASSERT(tic_ram, sizeof(tic_ram) == TIC_RAM_SIZE);
STATIC_ASSERT(tic_sound_register, sizeof(tic_sound_register) == 16+2);
STATIC_ASSERT(tic80_input, sizeof(tic80_input) == 12);
STATIC_ASSERT(tic_music_cmd_count, tic_music_cmd_count == 1 << MUSIC_CMD_BITS);
STATIC_ASSERT(tic_sound_state_size, sizeof(tic_sound_state) == 4);

static const u16 NoteFreqs[] = {0x10, 0x11, 0x12, 0x13, 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1c, 0x1d, 0x1f, 0x21, 0x23, 0x25, 0x27, 0x29, 0x2c, 0x2e, 0x31, 0x34, 0x37, 0x3a, 0x3e, 0x41, 0x45, 0x49, 0x4e, 0x52, 0x57, 0x5c, 0x62, 0x68, 0x6e, 0x75, 0x7b, 0x83, 0x8b, 0x93, 0x9c, 0xa5, 0xaf, 0xb9, 0xc4, 0xd0, 0xdc, 0xe9, 0xf7, 0x106, 0x115, 0x126, 0x137, 0x14a, 0x15d, 0x172, 0x188, 0x19f, 0x1b8, 0x1d2, 0x1ee, 0x20b, 0x22a, 0x24b, 0x26e, 0x293, 0x2ba, 0x2e4, 0x310, 0x33f, 0x370, 0x3a4, 0x3dc, 0x417, 0x455, 0x497, 0x4dd, 0x527, 0x575, 0x5c8, 0x620, 0x67d, 0x6e0, 0x749, 0x7b8, 0x82d, 0x8a9, 0x92d, 0x9b9, 0xa4d, 0xaea, 0xb90, 0xc40, 0xcfa, 0xdc0, 0xe91, 0xf6f, 0x105a, 0x1153, 0x125b, 0x1372, 0x149a, 0x15d4, 0x1720, 0x1880};
STATIC_ASSERT(count_of_freqs, COUNT_OF(NoteFreqs) == NOTES*OCTAVES + PIANO_START);

static inline s32 getTempo(const tic_track* track) { return track->tempo + DEFAULT_TEMPO; }
static inline s32 getSpeed(const tic_track* track) { return track->speed + DEFAULT_SPEED; }

static s32 tick2row(const tic_track* track, s32 tick)
{
    // BPM = tempo * 6 / speed
    return tick * getTempo(track) * DEFAULT_SPEED / getSpeed(track) / NOTES_PER_MUNUTE;
}

static s32 row2tick(const tic_track* track, s32 row)
{
    return row * getSpeed(track) * NOTES_PER_MUNUTE / getTempo(track) / DEFAULT_SPEED;
}

static inline s32 param2val(const tic_track_row* row)
{
    return (row->param1 << 4) | row->param2;
}

static void update_amp(blip_buffer_t* blip, tic_sound_register_data* data, s32 new_amp )
{
    s32 delta = new_amp - data->amp;
    data->amp += delta;
    blip_add_delta( blip, data->time, delta );
}

static inline s32 freq2period(s32 freq)
{
    enum
    {
        MinPeriodValue = 10,
        MaxPeriodValue = 4096,
        Rate = CLOCKRATE * ENVELOPE_FREQ_SCALE / WAVE_VALUES
    };

    if(freq == 0) return MaxPeriodValue;

    return CLAMP(Rate / freq - 1, MinPeriodValue, MaxPeriodValue);
}

static inline s32 getAmp(const tic_sound_register* reg, s32 amp)
{
    enum{AmpMax = (u16)-1/2};
    return (amp * AmpMax / MAX_VOLUME) * reg->volume / MAX_VOLUME / TIC_SOUND_CHANNELS;
}

static void runEnvelope(blip_buffer_t* blip, const tic_sound_register* reg, tic_sound_register_data* data, s32 end_time, u8 volume)
{
    s32 period = freq2period(reg->freq * ENVELOPE_FREQ_SCALE);

    for ( ; data->time < end_time; data->time += period )
    {
        data->phase = (data->phase + 1) % WAVE_VALUES;

        update_amp(blip, data, getAmp(reg, tic_tool_peek4(reg->waveform.data, data->phase) * volume / MAX_VOLUME));
    }
}

static void runNoise(blip_buffer_t* blip, const tic_sound_register* reg, tic_sound_register_data* data, s32 end_time, u8 volume)
{
    // phase is noise LFSR, which must never be zero 
    if ( data->phase == 0 )
        data->phase = 1;
    
    s32 period = freq2period(reg->freq);

    for ( ; data->time < end_time; data->time += period )
    {
        data->phase = ((data->phase & 1) * (0b11 << 13)) ^ (data->phase >> 1);
        update_amp(blip, data, getAmp(reg, (data->phase & 1) ? volume : 0));
    }
}

static void resetBlitSegment(tic_mem* memory)
{
    memory->ram.vram.blit.segment = 2;
}

static tic_tilesheet getTileSheetFromSegment(tic_mem* memory, u8 segment)
{
    u8* src;
    switch(segment){
        case 0:
        case 1: 
            src = (u8*) &memory->ram.font.data; break;
        default:
            src = (u8*) &memory->ram.tiles.data; break;
    }

    return getTileSheet(segment, src);
}

static void resetPalette(tic_mem* memory)
{
    static const u8 DefaultMapping[] = {16, 50, 84, 118, 152, 186, 220, 254};
    memcpy(memory->ram.vram.palette.data, memory->cart.bank0.palette.scn.data, sizeof(tic_palette));
    memcpy(memory->ram.vram.mapping, DefaultMapping, sizeof DefaultMapping);
}

static u8* getPalette(tic_mem* tic, u8* colors, u8 count)
{
    static u8 mapping[TIC_PALETTE_SIZE];
    for (s32 i = 0; i < TIC_PALETTE_SIZE; i++) mapping[i] = tic_tool_peek4(tic->ram.vram.mapping, i);
    for (s32 i = 0; i < count; i++) mapping[colors[i]] = TRANSPARENT_COLOR;
    return mapping;
}

static inline u8 mapColor(tic_mem* tic, u8 color)
{
    return tic_tool_peek4(tic->ram.vram.mapping, color & 0xf);
}

static void setPixelDma(tic_mem* tic, s32 x, s32 y, u8 color)
{
    tic_tool_poke4(tic->ram.vram.screen.data, y * TIC80_WIDTH + x, color);
}

static inline u32* getOvrAddr(tic_mem* tic, s32 x, s32 y)
{
    enum {Top = (TIC80_FULLHEIGHT-TIC80_HEIGHT)/2};
    enum {Left = (TIC80_FULLWIDTH-TIC80_WIDTH)/2};

    return tic->screen + x + (y << TIC80_FULLWIDTH_BITS) + (Left + Top * TIC80_FULLWIDTH);
}

static void setPixelOvr(tic_mem* tic, s32 x, s32 y, u8 color)
{
    tic_machine* machine = (tic_machine*)tic;
    
    *getOvrAddr(tic, x, y) = *(machine->state.ovr.raw + color);
}

static u8 getPixelOvr(tic_mem* tic, s32 x, s32 y)
{
    tic_machine* machine = (tic_machine*)tic;
    
    u32 color = *getOvrAddr(tic, x, y);
    u32* pal = machine->state.ovr.raw;

    for(s32 i = 0; i < TIC_PALETTE_SIZE; i++, pal++)
        if(*pal == color)
            return i;

    return 0;
}

static u8 getPixelDma(tic_mem* tic, s32 x, s32 y)
{
    tic_machine* machine = (tic_machine*)tic;

    return tic_tool_peek4(machine->memory.ram.vram.screen.data, y * TIC80_WIDTH + x);
}

static void setPixel(tic_machine* machine, s32 x, s32 y, u8 color)
{
    if(x < machine->state.clip.l || y < machine->state.clip.t || x >= machine->state.clip.r || y >= machine->state.clip.b) return;

    machine->state.setpix(&machine->memory, x, y, color);
}

static u8 getPixel(tic_machine* machine, s32 x, s32 y)
{
    if(x < 0 || y < 0 || x >= TIC80_WIDTH || y >= TIC80_HEIGHT) return 0;

    return machine->state.getpix(&machine->memory, x, y);
}

static void drawHLineDma(tic_mem* memory, s32 xl, s32 xr, s32 y, u8 color)
{
    color = color << 4 | color;
    if (xl >= xr) return;
    if (xl & 1) {
        tic_tool_poke4(&memory->ram.vram.screen.data, y * TIC80_WIDTH + xl, color);
        xl++;
    }
    s32 count = (xr - xl) >> 1;
    u8 *screen = memory->ram.vram.screen.data + ((y * TIC80_WIDTH + xl) >> 1);
    for(s32 i = 0; i < count; i++) *screen++ = color;
    if (xr & 1) {
        tic_tool_poke4(&memory->ram.vram.screen.data, y * TIC80_WIDTH + xr - 1, color);
    }
}

static void drawHLineOvr(tic_mem* tic, s32 x1, s32 x2, s32 y, u8 color)
{
    tic_machine* machine = (tic_machine*)tic;
    u32 final_color = *(machine->state.ovr.raw + color);
    for(s32 x = x1; x < x2; ++x) {
        *getOvrAddr(tic, x, y) = final_color;
    }
}


#define EARLY_CLIP(x, y, width, height) \
    ( \
        (((y)+(height)-1) < machine->state.clip.t) \
        || (((x)+(width)-1) < machine->state.clip.l) \
        || ((y) >= machine->state.clip.b) \
        || ((x) >= machine->state.clip.r) \
    )

static void drawHLine(tic_machine* machine, s32 x, s32 y, s32 width, u8 color)
{
    if(y < machine->state.clip.t || machine->state.clip.b <= y) return;

    s32 xl = MAX(x, machine->state.clip.l);
    s32 xr = MIN(x + width, machine->state.clip.r);

    machine->state.drawhline(&machine->memory, xl, xr, y, color);
}

static void drawVLine(tic_machine* machine, s32 x, s32 y, s32 height, u8 color)
{
    if(x < machine->state.clip.l || machine->state.clip.r <= x) return;

    s32 yl = y < 0 ? 0 : y;
    s32 yr = y + height >= TIC80_HEIGHT ? TIC80_HEIGHT : y + height;

    for(s32 i = yl; i < yr; ++i)
        setPixel(machine, x, i, color);
}

static void drawRect(tic_machine* machine, s32 x, s32 y, s32 width, s32 height, u8 color)
{
    for(s32 i = y; i < y + height; ++i)
        drawHLine(machine, x, i, width, color);
}

static void drawRectBorder(tic_machine* machine, s32 x, s32 y, s32 width, s32 height, u8 color)
{
    drawHLine(machine, x, y, width, color);
    drawHLine(machine, x, y + height - 1, width, color);

    drawVLine(machine, x, y, height, color);
    drawVLine(machine, x + width - 1, y, height, color);
}

#define DRAW_TILE_BODY(X, Y) do {\
    for(s32 py=sy; py < ey; py++, y++) \
    { \
        s32 xx = x; \
        for(s32 px=sx; px < ex; px++, xx++) \
        { \
            u8 color = mapping[getTilePixel(tile, (X), (Y))];\
            if(color != TRANSPARENT_COLOR) machine->state.setpix(&machine->memory, xx, y, color); \
        } \
    } \
    } while(0)

#define REVERT(X) (TIC_SPRITESIZE - 1 - (X))

static void drawTile(tic_machine* machine, tic_tileptr* tile, s32 x, s32 y, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)
{
    u8* mapping = getPalette(&machine->memory, colors, count);

    rotate &= 0b11;
    u32 orientation = flip & 0b11;

    if(rotate == tic_90_rotate) orientation ^= 0b001;
    else if(rotate == tic_180_rotate) orientation ^= 0b011;
    else if(rotate == tic_270_rotate) orientation ^= 0b010;
    if (rotate == tic_90_rotate || rotate == tic_270_rotate) orientation |= 0b100;

    if (scale == 1) {
        // the most common path
        s32 sx, sy, ex, ey;
        sx = machine->state.clip.l - x; if (sx < 0) sx = 0;
        sy = machine->state.clip.t - y; if (sy < 0) sy = 0;
        ex = machine->state.clip.r - x; if (ex > TIC_SPRITESIZE) ex = TIC_SPRITESIZE;
        ey = machine->state.clip.b - y; if (ey > TIC_SPRITESIZE) ey = TIC_SPRITESIZE;
        y += sy;
        x += sx;
        switch (orientation) {
            case 0b100: DRAW_TILE_BODY(py, px); break;
            case 0b110: DRAW_TILE_BODY(REVERT(py), px); break;
            case 0b101: DRAW_TILE_BODY(py, REVERT(px)); break;
            case 0b111: DRAW_TILE_BODY(REVERT(py), REVERT(px)); break;
            case 0b000: DRAW_TILE_BODY(px, py); break;
            case 0b010: DRAW_TILE_BODY(px, REVERT(py)); break;
            case 0b001: DRAW_TILE_BODY(REVERT(px), py); break;
            case 0b011: DRAW_TILE_BODY(REVERT(px), REVERT(py)); break;
            default: assert(!"Unknown value of orientation in drawTile");
        }
        return;
    }

    if (EARLY_CLIP(x, y, TIC_SPRITESIZE * scale, TIC_SPRITESIZE * scale)) return;

    for(s32 py=0; py < TIC_SPRITESIZE; py++, y+=scale)
    {
        s32 xx = x;
        for(s32 px=0; px < TIC_SPRITESIZE; px++, xx+=scale)
        {
            s32 i;
            s32 ix = orientation & 0b001 ? TIC_SPRITESIZE - px - 1: px;
            s32 iy = orientation & 0b010 ? TIC_SPRITESIZE - py - 1: py;
            if(orientation & 0b100) {
                s32 tmp = ix; ix=iy; iy=tmp;
            }
            u8 color = mapping[getTilePixel(tile, ix, iy)];
            if(color != TRANSPARENT_COLOR) drawRect(machine, xx, y, scale, scale, color);
        }
    }
}

#undef DRAW_TILE_BODY
#undef REVERT

static void drawSprite(tic_machine* machine, s32 index, s32 x, s32 y, s32 w, s32 h, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)
{
    tic_tilesheet sheet = getTileSheetFromSegment(&machine->memory, machine->memory.ram.vram.blit.segment);
    if ( w == 1 && h == 1){
        tic_tileptr tile = getTile(&sheet, index, false);
        drawTile(machine, &tile, x, y, colors, count, scale, flip, rotate);
    }
    else
    {
        s32 step = TIC_SPRITESIZE * scale;
        s32 cols = sheet.segment->sheet_width;

        const tic_flip vert_horz_flip = tic_horz_flip | tic_vert_flip;

        if (EARLY_CLIP(x, y, w * step, h * step)) return;

        for(s32 i = 0; i < w; i++)
        {
            for(s32 j = 0; j < h; j++)
            {
                s32 mx = i;
                s32 my = j;

                if(flip == tic_horz_flip || flip == vert_horz_flip) mx = w-1-i;
                if(flip == tic_vert_flip || flip == vert_horz_flip) my = h-1-j;

                if (rotate == tic_180_rotate)
                {
                    mx = w-1-mx;
                    my = h-1-my;
                }
                else if(rotate == tic_90_rotate)
                {
                    if(flip == tic_no_flip || flip == vert_horz_flip) my = h-1-my;
                    else mx = w-1-mx;
                }
                else if(rotate == tic_270_rotate)
                {
                    if(flip == tic_no_flip || flip == vert_horz_flip) mx = w-1-mx;
                    else my = h-1-my;
                }

                enum {Cols = TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE};


                tic_tileptr tile = getTile(&sheet, index + mx+my*cols, false);
                if(rotate==0 || rotate==2)
                    drawTile(machine, &tile, x+i*step, y+j*step, colors, count, scale, flip, rotate);
                else
                    drawTile(machine, &tile, x+j*step, y+i*step, colors, count, scale, flip, rotate);
            }
        }
    }
}

static void drawMap(tic_machine* machine, const tic_map* src, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8* colors, s32 count, s32 scale, RemapFunc remap, void* data)
{
    const s32 size = TIC_SPRITESIZE * scale;

    tic_tilesheet sheet = getTileSheetFromSegment(&machine->memory, machine->memory.ram.vram.blit.segment);

    for(s32 j = y, jj = sy; j < y + height; j++, jj += size)
        for(s32 i = x, ii = sx; i < x + width; i++, ii += size)
        {
            s32 mi = i;
            s32 mj = j;

            while(mi < 0) mi += TIC_MAP_WIDTH;
            while(mj < 0) mj += TIC_MAP_HEIGHT;
            while(mi >= TIC_MAP_WIDTH) mi -= TIC_MAP_WIDTH;
            while(mj >= TIC_MAP_HEIGHT) mj -= TIC_MAP_HEIGHT;
            
            s32 index = mi + mj * TIC_MAP_WIDTH;
            RemapResult retile = { *(src->data + index), tic_no_flip, tic_no_rotate };

            if (remap)
                remap(data, mi, mj, &retile);

            tic_tileptr tile = getTile(&sheet, retile.index, true);
            drawTile(machine, &tile, ii, jj, colors, count, scale, retile.flip, retile.rotate);
        }
}

static s32 drawChar(tic_machine* machine, tic_tileptr* font_char, s32 x, s32 y, s32 scale, bool fixed, u8* mapping)
{
    enum {Size = TIC_SPRITESIZE};

    s32 j=0, start=0, end=Size;

    if (!fixed) {
        for(s32 i=0; i < Size; i++) {
            for(j=0; j < Size; j++)
                if(mapping[getTilePixel(font_char, i, j)] != TRANSPARENT_COLOR) break;
            if (j < Size) break; else start++;
        }
        for(s32 i=Size-1; i>=start; i--) {
            for(j=0; j < Size; j++)
                if(mapping[getTilePixel(font_char, i, j)] != TRANSPARENT_COLOR) break;
            if (j < Size) break; else end--;
        }
    }
    s32 width = end - start;

    if (EARLY_CLIP(x, y, Size * scale, Size * scale)) return width;

    s32 colStart = start, colStep = 1, rowStart = 0 , rowStep = 1;

    for(s32 i=0, col=colStart, xs = x; i < width; i++, col+=colStep, xs+=scale)
    {
        for(s32 j = 0, row=rowStart, ys = y; j < Size; j++, row+=rowStep, ys+=scale)
        {
            u8 color = getTilePixel(font_char, col, row);
            if(mapping[color] != TRANSPARENT_COLOR)
                drawRect(machine, xs, ys, scale, scale, mapping[color]);
        }
    }
    return width;
}

static s32 drawText(tic_machine* machine, tic_tilesheet* font_face, const char* text, s32 x, s32 y, s32 width, s32 height, bool fixed, u8* mapping, s32 scale, bool alt)
{
    s32 pos = x;
    s32 MAX = x;
    char sym = 0;

    while((sym = *text++))
    {
        if(sym == '\n')
        {
            if(pos > MAX)
                MAX = pos;

            pos = x;
            y += height * scale;
        }
        else {
            tic_tileptr font_char = getTile(font_face, alt*TIC_FONT_CHARS/2 + sym, true);
            s32 size = drawChar(machine, &font_char, pos, y, scale, fixed, mapping);
            pos += ((!fixed && size) ? size + 1 : width) * scale;
        }
    }

    return pos > MAX ? pos - x : MAX - x;
}

static void resetSfxPos(tic_channel_data* channel)
{
    memset(channel->pos->data, -1, sizeof(tic_sfx_pos));
    channel->tick = -1;
}

static void setChannelData(tic_mem* memory, s32 index, s32 note, s32 octave, s32 duration, tic_channel_data* channel, s32 volumeLeft, s32 volumeRight, s32 speed)
{
    tic_machine* machine = (tic_machine*)memory;

    channel->volume.left = volumeLeft;
    channel->volume.right = volumeRight;

    if(index >= 0)
    {
        struct {s8 speed:SFX_SPEED_BITS;} temp = {speed};
        channel->speed = speed == temp.speed ? speed : memory->ram.sfx.samples.data[index].speed;
    }

    channel->note = note + octave * NOTES;
    channel->duration = duration;
    channel->index = index;

    resetSfxPos(channel);
}

static void setMusicChannelData(tic_mem* memory, s32 index, s32 note, s32 octave, s32 left, s32 right, s32 channel)
{
    tic_machine* machine = (tic_machine*)memory;
    setChannelData(memory, index, note, octave, -1, &machine->state.music.channels[channel], left, right, SFX_DEF_SPEED);
}

static void setSfxChannelData(tic_mem* memory, s32 index, s32 note, s32 octave, s32 duration, s32 channel, s32 left, s32 right, s32 speed)
{
    tic_machine* machine = (tic_machine*)memory;
    setChannelData(memory, index, note, octave, duration, &machine->state.sfx.channels[channel], left, right, speed);
}

static void resetMusicChannels(tic_mem* memory)
{
    for (s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
        setMusicChannelData(memory, -1, 0, 0, 0, 0, c);

    tic_machine* machine = (tic_machine*)memory;
    memset(machine->state.music.commands, 0, sizeof machine->state.music.commands);
    memset(&machine->state.music.jump, 0, sizeof(tic_jump_command));
}

static void setMusic(tic_machine* machine, s32 index, s32 frame, s32 row, bool loop, bool sustain)
{
    tic_mem* memory = (tic_mem*)machine;

    memory->ram.sound_state.music.track = index;

    if(index < 0)
    {
        memory->ram.sound_state.flag.music_state = tic_music_stop;
        resetMusicChannels(memory);
    }
    else
    {
        for (s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
            setMusicChannelData(memory, -1, 0, 0, MAX_VOLUME, MAX_VOLUME, c);

        memory->ram.sound_state.music.row = row;
        memory->ram.sound_state.music.frame = frame < 0 ? 0 : frame;
        memory->ram.sound_state.flag.music_loop = loop;
        memory->ram.sound_state.flag.music_sustain = sustain;
        memory->ram.sound_state.flag.music_state = tic_music_play;

        const tic_track* track = &memory->ram.music.tracks.data[index];
        machine->state.music.ticks = row >= 0 ? row2tick(track, row) : 0;
    }
}

void tic_api_music(tic_mem* memory, s32 index, s32 frame, s32 row, bool loop, bool sustain)
{
    tic_machine* machine = (tic_machine*)memory;

    setMusic(machine, index, frame, row, loop, sustain);

    if(index >= 0)
        memory->ram.sound_state.flag.music_state = tic_music_play;
}

static void stopMusic(tic_mem* memory)
{
    tic_api_music(memory, -1, 0, 0, false, false);
}

static void soundClear(tic_mem* memory)
{
    tic_machine* machine = (tic_machine*)memory;

    for(s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
    {
        static const tic_channel_data EmptyChannel =
        { 
            .tick = -1,
            .pos = NULL,
            .index = -1,
            .note = 0,
            .volume = {0, 0},
            .speed = 0,
            .duration = -1,
        };

        memcpy(&machine->state.music.channels[i], &EmptyChannel, sizeof EmptyChannel);
        memcpy(&machine->state.sfx.channels[i], &EmptyChannel, sizeof EmptyChannel);

        memset(machine->state.sfx.channels[i].pos = &memory->ram.sfxpos[i], -1, sizeof(tic_sfx_pos));
        memset(machine->state.music.channels[i].pos = &machine->state.music.sfxpos[i], -1, sizeof(tic_sfx_pos));
    }

    memset(&memory->ram.registers, 0, sizeof memory->ram.registers);
    memset(memory->samples.buffer, 0, memory->samples.size);

    stopMusic(memory);
}

static void updateSaveid(tic_mem* memory);

void tic_api_clip(tic_mem* memory, s32 x, s32 y, s32 width, s32 height)
{
    tic_machine* machine = (tic_machine*)memory;

    machine->state.clip.l = x;
    machine->state.clip.t = y;
    machine->state.clip.r = x + width;
    machine->state.clip.b = y + height;

    if(machine->state.clip.l < 0) machine->state.clip.l = 0;
    if(machine->state.clip.t < 0) machine->state.clip.t = 0;
    if(machine->state.clip.r > TIC80_WIDTH) machine->state.clip.r = TIC80_WIDTH;
    if(machine->state.clip.b > TIC80_HEIGHT) machine->state.clip.b = TIC80_HEIGHT;
}

void tic_api_reset(tic_mem* memory)
{
    resetPalette(memory);
    resetBlitSegment(memory);

    memset(&memory->ram.vram.vars, 0, sizeof memory->ram.vram.vars);
    
    tic_api_clip(memory, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);

    soundClear(memory);

    tic_machine* machine = (tic_machine*)memory;
    machine->state.initialized = false;
    machine->state.scanline = NULL;
    machine->state.ovr.callback = NULL;

    machine->state.setpix = setPixelDma;
    machine->state.getpix = getPixelDma;
    machine->state.drawhline = drawHLineDma;

    updateSaveid(memory);
}

void tic_core_pause(tic_mem* memory)
{
    tic_machine* machine = (tic_machine*)memory;

    memcpy(&machine->pause.state, &machine->state, sizeof(tic_machine_state_data));
    memcpy(&machine->pause.ram, &memory->ram, sizeof(tic_ram));
    memset(&machine->state.ovr, 0, sizeof machine->state.ovr);

    if (machine->data)
    {
        machine->pause.time.start = machine->data->start;
        machine->pause.time.paused = machine->data->counter();
    }
}

void tic_core_resume(tic_mem* memory)
{
    tic_machine* machine = (tic_machine*)memory;

    if (machine->data)
    {
        memcpy(&machine->state, &machine->pause.state, sizeof(tic_machine_state_data));
        memcpy(&memory->ram, &machine->pause.ram, sizeof(tic_ram));

        machine->data->start = machine->pause.time.start + machine->data->counter() - machine->pause.time.paused;
    }
}

void tic_core_close(tic_mem* memory)
{
    tic_machine* machine = (tic_machine*)memory;

    machine->state.initialized = false;

#if defined(TIC_BUILD_WITH_SQUIRREL)
    getSquirrelScriptConfig()->close(memory);
#endif

#if defined(TIC_BUILD_WITH_LUA)
    getLuaScriptConfig()->close(memory);

#   if defined(TIC_BUILD_WITH_MOON)
    getMoonScriptConfig()->close(memory);
#   endif

#   if defined(TIC_BUILD_WITH_FENNEL)
    getFennelConfig()->close(memory);
#   endif

#endif /* defined(TIC_BUILD_WITH_LUA) */


#if defined(TIC_BUILD_WITH_JS)
    getJsScriptConfig()->close(memory);
#endif

#if defined(TIC_BUILD_WITH_WREN)
    getWrenScriptConfig()->close(memory);
#endif

    blip_delete(machine->blip.left);
    blip_delete(machine->blip.right);

    free(memory->samples.buffer);
    free(machine);
}

///////////////////////////////////////////////////////////////////////////////
// API ////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void tic_api_rect(tic_mem* memory, s32 x, s32 y, s32 width, s32 height, u8 color)
{
    tic_machine* machine = (tic_machine*)memory;

    drawRect(machine, x, y, width, height, mapColor(memory, color));
}

void tic_api_cls(tic_mem* memory, u8 color)
{
    static const tic_clip_data EmptyClip = {0, 0, TIC80_WIDTH, TIC80_HEIGHT};

    tic_machine* machine = (tic_machine*)memory;

    if(memcmp(&machine->state.clip, &EmptyClip, sizeof(tic_clip_data)) == 0)
    {
        color &= 0b00001111;
        memset(memory->ram.vram.screen.data, color | (color << TIC_PALETTE_BPP), sizeof(memory->ram.vram.screen.data));     
    }
    else
    {
        tic_api_rect(memory, machine->state.clip.l, machine->state.clip.t, machine->state.clip.r - machine->state.clip.l, machine->state.clip.b - machine->state.clip.t, color);
    }
}

s32 tic_api_font(tic_mem* memory, const char* text, s32 x, s32 y, u8 chromakey, s32 w, s32 h, bool fixed, s32 scale, bool alt)
{
    u8* mapping = getPalette(memory, &chromakey, 1);

    // Compatibility : flip top and bottom of the spritesheet
    // to preserve tic_api_font's default target
    u8 segment = memory->ram.vram.blit.segment >> 1;
    u8 flipmask = 1; while (segment >>= 1) flipmask<<=1;

    tic_tilesheet font_face = getTileSheetFromSegment(memory, memory->ram.vram.blit.segment ^ flipmask);
    return drawText((tic_machine*)memory, &font_face, text, x, y, w, h, fixed, mapping, scale, alt);
}

s32 tic_api_print(tic_mem* memory, const char* text, s32 x, s32 y, u8 color, bool fixed, s32 scale, bool alt)
{
    u8 mapping[] = {255, color};
    tic_tilesheet font_face = getTileSheetFromSegment(memory, 1);
    // Compatibility : print uses reduced width for non-fixed space
    u8 width = alt ? TIC_ALTFONT_WIDTH : TIC_FONT_WIDTH;
    if (!fixed) width -= 2;
    return drawText((tic_machine*)memory, &font_face, text, x, y, width, TIC_FONT_HEIGHT, fixed, mapping, scale, alt);
}

void tic_api_spr(tic_mem* memory, s32 index, s32 x, s32 y, s32 w, s32 h, u8* colors, s32 count, s32 scale, tic_flip flip, tic_rotate rotate)
{
    drawSprite((tic_machine*)memory, index, x, y, w, h, colors, count, scale, flip, rotate);
}

static inline u8* getFlag(tic_mem* memory, s32 index, u8 flag)
{
    static u8 stub = 0;
    if(index >= TIC_FLAGS || flag >= BITS_IN_BYTE)
        return &stub;

    return memory->ram.flags.data + index;
}

bool tic_api_fget(tic_mem* memory, s32 index, u8 flag)
{
    return *getFlag(memory, index, flag) & (1 << flag);
}

void tic_api_fset(tic_mem* memory, s32 index, u8 flag, bool value)
{
    if(value)
        *getFlag(memory, index, flag) |= (1 << flag);
    else 
        *getFlag(memory, index, flag) &= ~(1 << flag);
}

u8 tic_api_pix(tic_mem* memory, s32 x, s32 y, u8 color, bool get)
{
    tic_machine* machine = (tic_machine*)memory;

    if(get) return getPixel(machine, x, y);

    setPixel(machine, x, y, mapColor(memory, color));
    return 0;
}

void tic_api_rectb(tic_mem* memory, s32 x, s32 y, s32 width, s32 height, u8 color)
{
    tic_machine* machine = (tic_machine*)memory;

    drawRectBorder(machine, x, y, width, height, mapColor(memory, color));
}

static struct
{
    s16 Left[TIC80_HEIGHT];
    s16 Right[TIC80_HEIGHT];    
    s32 ULeft[TIC80_HEIGHT];
    s32 VLeft[TIC80_HEIGHT];
} SidesBuffer;

static void initSidesBuffer()
{
    for(s32 i = 0; i < COUNT_OF(SidesBuffer.Left); i++)
        SidesBuffer.Left[i] = TIC80_WIDTH, SidesBuffer.Right[i] = -1;   
}

static void setSidePixel(s32 x, s32 y)
{
    if(y >= 0 && y < TIC80_HEIGHT)
    {
        if(x < SidesBuffer.Left[y]) SidesBuffer.Left[y] = x;
        if(x > SidesBuffer.Right[y]) SidesBuffer.Right[y] = x;
    }
}

static void setSideTexPixel(s32 x, s32 y, float u, float v)
{
    s32 yy = y;
    if (yy >= 0 && yy < TIC80_HEIGHT)
    {
        if (x < SidesBuffer.Left[yy])
        {
            SidesBuffer.Left[yy] = x;
            SidesBuffer.ULeft[yy] = u*65536.0f;
            SidesBuffer.VLeft[yy] = v*65536.0f;
        }
        if (x > SidesBuffer.Right[yy])
        {
            SidesBuffer.Right[yy] = x;
        }
    }
}

void tic_api_circ(tic_mem* memory, s32 xm, s32 ym, s32 radius, u8 color)
{
    tic_machine* machine = (tic_machine*)memory;

    initSidesBuffer();

    s32 r = radius;
    s32 x = -r, y = 0, err = 2-2*r;
    do 
    {
        setSidePixel(xm-x, ym+y);
        setSidePixel(xm-y, ym-x);
        setSidePixel(xm+x, ym-y);
        setSidePixel(xm+y, ym+x);

        r = err;
        if (r <= y) err += ++y*2+1;
        if (r > x || err > y) err += ++x*2+1;
    } while (x < 0);

    s32 yt = MAX(machine->state.clip.t, ym-radius);
    s32 yb = MIN(machine->state.clip.b, ym+radius+1);
    u8 final_color = mapColor(&machine->memory, color);
    for(s32 y = yt; y < yb; y++) {
        s32 xl = MAX(SidesBuffer.Left[y], machine->state.clip.l);
        s32 xr = MIN(SidesBuffer.Right[y]+1, machine->state.clip.r);
        machine->state.drawhline(&machine->memory, xl, xr, y, final_color);
    }
}

void tic_api_circb(tic_mem* memory, s32 xm, s32 ym, s32 radius, u8 color)
{
    tic_machine* machine = (tic_machine*)memory;
    u8 final_color = mapColor(memory, color);
    s32 r = radius;
    s32 x = -r, y = 0, err = 2-2*r;
    do {
        setPixel(machine, xm-x, ym+y, final_color);
        setPixel(machine, xm-y, ym-x, final_color);
        setPixel(machine, xm+x, ym-y, final_color);
        setPixel(machine, xm+y, ym+x, final_color);
        r = err;
        if (r <= y) err += ++y*2+1;
        if (r > x || err > y) err += ++x*2+1;
    } while (x < 0);
}

typedef void(*linePixelFunc)(tic_mem* memory, s32 x, s32 y, u8 color);
static void ticLine(tic_mem* memory, s32 x0, s32 y0, s32 x1, s32 y1, u8 color, linePixelFunc func)
{
    if(y0 > y1)
    {
        SWAP(x0, x1, s32);
        SWAP(y0, y1, s32);
    }

    s32 dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    s32 dy = y1 - y0; 
    s32 err = (dx > dy ? dx : -dy) / 2, e2;

    for(;;)
    {
        func(memory, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = err;
        if (e2 >-dx) { err -= dy; x0 += sx; }
        if (e2 < dy) { err += dx; y0++; }
    }
}

static void triPixelFunc(tic_mem* memory, s32 x, s32 y, u8 color)
{
    setSidePixel(x, y);
}

void tic_api_tri(tic_mem* memory, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, u8 color)
{
    tic_machine* machine = (tic_machine*)memory;

    initSidesBuffer();

    ticLine(memory, x1, y1, x2, y2, color, triPixelFunc);
    ticLine(memory, x2, y2, x3, y3, color, triPixelFunc);
    ticLine(memory, x3, y3, x1, y1, color, triPixelFunc);

    u8 final_color = mapColor(&machine->memory, color);
    s32 yt = MAX(machine->state.clip.t, MIN(y1, MIN(y2, y3)));
    s32 yb = MIN(machine->state.clip.b, MAX(y1, MAX(y2, y3)) + 1);

    for(s32 y = yt; y < yb; y++) {
        s32 xl = MAX(SidesBuffer.Left[y], machine->state.clip.l);
        s32 xr = MIN(SidesBuffer.Right[y]+1, machine->state.clip.r);
        machine->state.drawhline(&machine->memory, xl, xr, y, final_color);
    }
}


typedef struct
{
    float x, y, u, v;
} TexVert;


static void ticTexLine(tic_mem* memory, TexVert *v0, TexVert *v1)
{
    TexVert *top = v0;
    TexVert *bot = v1;

    if (bot->y < top->y)
    {
        top = v1;
        bot = v0;
    }

    float dy = bot->y - top->y;
    float step_x = (bot->x - top->x);
    float step_u = (bot->u - top->u);
    float step_v = (bot->v - top->v);

    if ((s32)dy != 0)
    {
        step_x /= dy;
        step_u /= dy;
        step_v /= dy;
    }

    float x = top->x;
    float y = top->y;
    float u = top->u;
    float v = top->v;

    if(y < .0f)
    {
        y = .0f - y;

        x += step_x * y;
        u += step_u * y;
        v += step_v * y;

        y = .0f;
    }

    s32 botY = bot->y;
    if(botY > TIC80_HEIGHT)
        botY = TIC80_HEIGHT;

    for (; y <botY; ++y)
    {
        setSideTexPixel(x, y, u, v);
        x += step_x;
        u += step_u;
        v += step_v;
    }
}

static void drawTexturedTriangle(tic_machine* machine, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float u3, float v3, bool use_map, u8* colors, s32 count)
{
    tic_mem* memory = &machine->memory;
    u8* mapping = getPalette(memory, colors, count);
    TexVert V0, V1, V2;

    const u8* map = memory->ram.map.data;
    tic_tilesheet sheet = getTileSheetFromSegment(memory, memory->ram.vram.blit.segment);

    V0.x = x1;  V0.y = y1;  V0.u = u1;  V0.v = v1;
    V1.x = x2;  V1.y = y2;  V1.u = u2;  V1.v = v2;
    V2.x = x3;  V2.y = y3;  V2.u = u3;  V2.v = v3;

    //  calculate the slope of the surface 
    //  use floats here 
    double denom = (V0.x - V2.x) * (V1.y - V2.y) - (V1.x - V2.x) * (V0.y - V2.y);
    if (denom == 0.0)
    {
        return;
    }
    double id = 1.0 / denom;
    float dudx, dvdx;
    //  this is the UV slope across the surface
    dudx = ((V0.u - V2.u) * (V1.y - V2.y) - (V1.u - V2.u) * (V0.y - V2.y)) * id;
    dvdx = ((V0.v - V2.v) * (V1.y - V2.y) - (V1.v - V2.v) * (V0.y - V2.y)) * id;
    //  convert to fixed
    s32 dudxs = dudx * 65536.0f;
    s32 dvdxs = dvdx * 65536.0f;
    //  fill the buffer 
    initSidesBuffer();
    //  parse each line and decide where in the buffer to store them ( left or right ) 
    ticTexLine(memory, &V0, &V1);
    ticTexLine(memory, &V1, &V2);
    ticTexLine(memory, &V2, &V0);

    for (s32 y = 0; y < TIC80_HEIGHT; y++)
    {
        //  if it's backwards skip it
        s32 width = SidesBuffer.Right[y] - SidesBuffer.Left[y];
        //  if it's off top or bottom , skip this line
        if ((y < machine->state.clip.t) || (y > machine->state.clip.b))
            width = 0;
        if (width > 0)
        {
            s32 u = SidesBuffer.ULeft[y];
            s32 v = SidesBuffer.VLeft[y];
            s32 left = SidesBuffer.Left[y];
            s32 right = SidesBuffer.Right[y];
            //  check right edge, and CLAMP it
            if (right > machine->state.clip.r)
                right = machine->state.clip.r;
            //  check left edge and offset UV's if we are off the left 
            if (left < machine->state.clip.l)
            {
                s32 dist = machine->state.clip.l - SidesBuffer.Left[y];
                u += dudxs * dist;
                v += dvdxs * dist;
                left = machine->state.clip.l;
            }
            //  are we drawing from the map . ok then at least check before the inner loop
            if (use_map == true)
            {
                for (s32 x = left; x < right; ++x)
                {
                    enum { MapWidth = TIC_MAP_WIDTH * TIC_SPRITESIZE, MapHeight = TIC_MAP_HEIGHT * TIC_SPRITESIZE };
                    s32 iu = (u >> 16) % MapWidth;
                    s32 iv = (v >> 16) % MapHeight;

                    while (iu < 0) iu += MapWidth;
                    while (iv < 0) iv += MapHeight;

                    u8 tileindex = map[(iv >> 3) * TIC_MAP_WIDTH + (iu >> 3)];
                    tic_tileptr tile = getTile(&sheet, tileindex, true);

                    u8 color = mapping[getTilePixel(&tile, iu & 7, iv & 7)];
                    if (color != TRANSPARENT_COLOR)
                        setPixel(machine, x, y, color);
                    u += dudxs;
                    v += dvdxs;
                }
            }
            else
            {
                //  direct from tile ram 
                for (s32 x = left; x < right; ++x)
                {
                    enum{SheetWidth = TIC_SPRITESHEET_SIZE, SheetHeight = TIC_SPRITESHEET_SIZE * TIC_SPRITE_BANKS};
                    s32 iu = (u>>16) & (SheetWidth - 1);
                    s32 iv = (v>>16) & (SheetHeight - 1);

                    u8 color = mapping[getTileSheetPixel(&sheet, iu, iv)];
                    if (color != TRANSPARENT_COLOR)
                        setPixel(machine, x, y, color);
                    u += dudxs;
                    v += dvdxs;
                }
            }
        }
    }
}

void tic_api_textri(tic_mem* memory, float x1, float y1, float x2, float y2, float x3, float y3, float u1, float v1, float u2, float v2, float u3, float v3, bool use_map, u8* colors, s32 count)
{
    drawTexturedTriangle((tic_machine*)memory, x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, use_map, colors, count);
}

void tic_api_map(tic_mem* memory, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8* colors, s32 count, s32 scale, RemapFunc remap, void* data)
{
    drawMap((tic_machine*)memory, &memory->ram.map, x, y, width, height, sx, sy, colors, count, scale, remap, data);
}

void tic_api_mset(tic_mem* memory, s32 x, s32 y, u8 value)
{
    if(x < 0 || x >= TIC_MAP_WIDTH || y < 0 || y >= TIC_MAP_HEIGHT) return;

    tic_map* src = &memory->ram.map;
    *(src->data + y * TIC_MAP_WIDTH + x) = value;
}

u8 tic_api_mget(tic_mem* memory, s32 x, s32 y)
{
    if(x < 0 || x >= TIC_MAP_WIDTH || y < 0 || y >= TIC_MAP_HEIGHT) return 0;
    
    const tic_map* src = &memory->ram.map;
    return *(src->data + y * TIC_MAP_WIDTH + x);
}

static inline void setLinePixel(tic_mem* tic, s32 x, s32 y, u8 color)
{
    setPixel((tic_machine*)tic, x, y, color);
}

void tic_api_line(tic_mem* memory, s32 x0, s32 y0, s32 x1, s32 y1, u8 color)
{
    ticLine(memory, x0, y0, x1, y1, mapColor(memory, color), setLinePixel);
}

static s32 calcLoopPos(const tic_sound_loop* loop, s32 pos)
{
    s32 offset = 0;

    if(loop->size > 0)
    {
        for(s32 i = 0; i < pos; i++)
        {
            if(offset < (loop->start + loop->size-1))
                offset++;
            else offset = loop->start;
        }
    }
    else offset = pos >= SFX_TICKS ? SFX_TICKS - 1 : pos;

    return offset;
}

static void sfx(tic_mem* memory, s32 index, s32 note, s32 pitch, tic_channel_data* channel, tic_sound_register* reg, s32 channelIndex)
{
    tic_machine* machine = (tic_machine*)memory;

    if(channel->duration > 0)
        channel->duration--;

    if(index < 0 || channel->duration == 0)
    {
        resetSfxPos(channel);
        return;
    }

    const tic_sample* effect = &memory->ram.sfx.samples.data[index];
    s32 pos = tic_tool_sfx_pos(channel->speed, ++channel->tick);

    for(s32 i = 0; i < sizeof(tic_sfx_pos); i++)
        *(channel->pos->data+i) = calcLoopPos(effect->loops + i, pos);

    u8 volume = MAX_VOLUME - effect->data[channel->pos->volume].volume;

    if(volume > 0)
    {
        s8 arp = effect->data[channel->pos->chord].chord * (effect->reverse ? -1 : 1);
        if(arp) note += arp;

        note = CLAMP(note, 0, COUNT_OF(NoteFreqs)-1);

        reg->freq = NoteFreqs[note] + effect->data[channel->pos->pitch].pitch * (effect->pitch16x ? 16 : 1) + pitch;
        reg->volume = volume;

        u8 wave = effect->data[channel->pos->wave].wave;
        const tic_waveform* waveform = &memory->ram.sfx.waveforms.items[wave];
        memcpy(reg->waveform.data, waveform->data, sizeof(tic_waveform));

        tic_tool_poke4(&memory->ram.stereo.data, channelIndex*2, channel->volume.left * !effect->stereo_left);
        tic_tool_poke4(&memory->ram.stereo.data, channelIndex*2+1, channel->volume.right * !effect->stereo_right);
    }
}

static void processMusic(tic_mem* memory)
{
    tic_machine* machine = (tic_machine*)memory;
    tic_sound_state* sound_state = &memory->ram.sound_state;

    if(sound_state->flag.music_state == tic_music_stop) return;

    const tic_track* track = &memory->ram.music.tracks.data[sound_state->music.track];
    s32 row = tick2row(track, machine->state.music.ticks);
    tic_jump_command* jumpCmd = &machine->state.music.jump;

    if (row != sound_state->music.row 
        && jumpCmd->active)
    {
        sound_state->music.frame = jumpCmd->frame;
        sound_state->music.row = jumpCmd->beat * NOTES_PER_BEAT;
        machine->state.music.ticks = row2tick(track, sound_state->music.row);
        memset(jumpCmd, 0, sizeof(tic_jump_command));
    }

    s32 rows = MUSIC_PATTERN_ROWS - track->rows;
    if (row >= rows)
    {
        row = 0;
        machine->state.music.ticks = 0;

        // If music is in sustain mode, we only reset the channels if the music stopped.
        // Otherwise, we reset it on every new frame.
        if(sound_state->flag.music_state == tic_music_stop || !sound_state->flag.music_sustain)
        {
          resetMusicChannels(memory);

          for (s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
              setMusicChannelData(memory, -1, 0, 0, MAX_VOLUME, MAX_VOLUME, c);
        }

        if(sound_state->flag.music_state == tic_music_play)
        {
            sound_state->music.frame++;

            if(sound_state->music.frame >= MUSIC_FRAMES)
            {
                if(sound_state->flag.music_loop)
                    sound_state->music.frame = 0;
                else
                {
                    stopMusic(memory);
                    return;
                }
            }
            else
            {
                s32 val = 0;
                for (s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
                    val += tic_tool_get_pattern_id(track, sound_state->music.frame, c);

                // empty frame detected
                if(!val)
                {
                    if(sound_state->flag.music_loop)
                        sound_state->music.frame = 0;
                    else
                    {
                        stopMusic(memory);
                        return;
                    }                   
                }
            }
        }
        else if(sound_state->flag.music_state == tic_music_play_frame)
        {
            if(!sound_state->flag.music_loop)
            {
                stopMusic(memory);
                return;
            }
        }
    }

    if (row != sound_state->music.row)
    {
        sound_state->music.row = row;

        for (s32 c = 0; c < TIC_SOUND_CHANNELS; c++)
        {
            s32 patternId = tic_tool_get_pattern_id(track, sound_state->music.frame, c);
            if (!patternId) continue;

            const tic_track_pattern* pattern = &memory->ram.music.patterns.data[patternId - PATTERN_START];
            const tic_track_row* trackRow = &pattern->rows[sound_state->music.row];
            tic_channel_data* channel = &machine->state.music.channels[c];
            tic_command_data* cmdData = &machine->state.music.commands[c];

            if(trackRow->command == tic_music_cmd_delay)
            {
                cmdData->delay.row = trackRow;
                cmdData->delay.ticks = param2val(trackRow);
                trackRow = NULL;
            }
            
            if(cmdData->delay.row && cmdData->delay.ticks == 0)
            {
                trackRow = cmdData->delay.row;
                cmdData->delay.row = NULL;
            }

            if(trackRow)
            {
                // reset commands data
                if(trackRow->note)
                {
                    cmdData->slide.tick = 0;
                    cmdData->slide.note = channel->note;
                }

                if(trackRow->note == NoteStop)
                    setMusicChannelData(memory, -1, 0, 0, channel->volume.left, channel->volume.right, c);
                else if (trackRow->note >= NoteStart)
                    setMusicChannelData(memory, tic_tool_get_track_row_sfx(trackRow), trackRow->note - NoteStart, trackRow->octave, 
                        channel->volume.left, channel->volume.right, c);

                switch(trackRow->command)
                {
                case tic_music_cmd_volume:
                    channel->volume.left = trackRow->param1;
                    channel->volume.right = trackRow->param2;
                    break;

                case tic_music_cmd_chord:
                    cmdData->chord.tick = 0;
                    cmdData->chord.note1 = trackRow->param1;
                    cmdData->chord.note2 = trackRow->param2;
                    break;

                case tic_music_cmd_jump:
                    machine->state.music.jump.active = true;
                    machine->state.music.jump.frame = trackRow->param1;
                    machine->state.music.jump.beat = trackRow->param2;
                    break;

                case tic_music_cmd_vibrato:
                    cmdData->vibrato.tick = 0;
                    cmdData->vibrato.period = trackRow->param1;
                    cmdData->vibrato.depth = trackRow->param2;
                    break;

                case tic_music_cmd_slide:
                    cmdData->slide.duration = param2val(trackRow);
                    break;

                case tic_music_cmd_pitch:
                    cmdData->finepitch.value = param2val(trackRow) - PITCH_DELTA;
                    break;

                default: break;
                }
            }
        }
    }

    for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i )
    {
        tic_channel_data* channel = &machine->state.music.channels[i];
        tic_command_data* cmdData = &machine->state.music.commands[i];
        
        if(channel->index >= 0)
        {
            s32 note = channel->note;
            s32 pitch = 0;
            
            // process chord commmand
            {
                s32 chord[] = 
                {
                    0, 
                    cmdData->chord.note1, 
                    cmdData->chord.note2
                };

                note += chord[cmdData->chord.tick % (cmdData->chord.note2 == 0 ? 2 : 3)];
            }

            // process vibrato commmand
            if(cmdData->vibrato.period && cmdData->vibrato.depth)
            {
                static const s32 VibData[] = {0x0, 0x31f1, 0x61f8, 0x8e3a, 0xb505, 0xd4db, 0xec83, 0xfb15, 0x10000, 0xfb15, 0xec83, 0xd4db, 0xb505, 0x8e3a, 0x61f8, 0x31f1, 0x0, 0xffffce0f, 0xffff9e08, 0xffff71c6, 0xffff4afb, 0xffff2b25, 0xffff137d, 0xffff04eb, 0xffff0000, 0xffff04eb, 0xffff137d, 0xffff2b25, 0xffff4afb, 0xffff71c6, 0xffff9e08, 0xffffce0f};
                STATIC_ASSERT(VibData, COUNT_OF(VibData) == 32);

                s32 p = cmdData->vibrato.period << 1;
                pitch += (VibData[(cmdData->vibrato.tick % p) * COUNT_OF(VibData) / p] * cmdData->vibrato.depth) >> 16;
            }

            // process slide command
            if(cmdData->slide.tick < cmdData->slide.duration)
                pitch += (NoteFreqs[channel->note] - NoteFreqs[note = cmdData->slide.note]) * cmdData->slide.tick / cmdData->slide.duration;

            pitch += cmdData->finepitch.value;

            sfx(memory, channel->index, note, pitch, channel, &memory->ram.registers[i], i);
        }

        ++cmdData->chord.tick;
        ++cmdData->vibrato.tick;
        ++cmdData->slide.tick;

        if(cmdData->delay.ticks)
            cmdData->delay.ticks--;
    }

    machine->state.music.ticks++;
}

static bool isKeyPressed(const tic80_keyboard* input, tic_key key)
{
    for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
        if(input->keys[i] == key)
            return true;

    return false;
}

void tic_core_tick_start(tic_mem* memory)
{
    tic_machine* machine = (tic_machine*)memory;

    for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i )
        memset(&memory->ram.registers[i], 0, sizeof(tic_sound_register));

    memory->ram.stereo.data = -1;

    processMusic(memory);

    for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i )
    {
        tic_channel_data* c = &machine->state.sfx.channels[i];
        
        if(c->index >= 0)
            sfx(memory, c->index, c->note, 0, c, &memory->ram.registers[i], i);
    }

    // process gamepad
    for(s32 i = 0; i < COUNT_OF(machine->state.gamepads.holds); i++)
    {
        u32 mask = 1 << i;
        u32 prevDown = machine->state.gamepads.previous.data & mask;
        u32 down = memory->ram.input.gamepads.data & mask;

        u32* hold = &machine->state.gamepads.holds[i];
        if(prevDown && prevDown == down) (*hold)++;
        else *hold = 0;
    }

    // process keyboard
    for(s32 i = 0; i < tic_keys_count; i++)
    {
        bool prevDown = isKeyPressed(&machine->state.keyboard.previous, i);
        bool down = isKeyPressed(&memory->ram.input.keyboard, i);

        u32* hold = &machine->state.keyboard.holds[i];

        if(prevDown && down) (*hold)++;
        else *hold = 0;
    }

    machine->state.setpix = setPixelDma;
    machine->state.getpix = getPixelDma;
    machine->state.synced = 0;
    machine->state.drawhline = drawHLineDma;
}

static void stereo_tick_end(tic_mem* memory, tic_sound_register_data* registers, blip_buffer_t* blip, u8 stereoRight)
{
    enum {EndTime = CLOCKRATE / TIC80_FRAMERATE};
    for (s32 i = 0; i < TIC_SOUND_CHANNELS; ++i )
    {
        u8 volume = tic_tool_peek4(&memory->ram.stereo.data, stereoRight + i*2);

        const tic_sound_register* reg = &memory->ram.registers[i];
        tic_sound_register_data* data = registers + i;

        tic_tool_is_noise(&reg->waveform)
            ? runNoise(blip, reg, data, EndTime, volume)
            : runEnvelope(blip, reg, data, EndTime, volume);

        data->time -= EndTime;
    }
    
    blip_end_frame(blip, EndTime);
}

void tic_core_tick_end(tic_mem* memory)
{
    tic_machine* machine = (tic_machine*)memory;
    tic80_input* input = &machine->memory.ram.input;

    machine->state.gamepads.previous.data = input->gamepads.data;
    machine->state.keyboard.previous.data = input->keyboard.data;

    stereo_tick_end(memory, machine->state.registers.left, machine->blip.left, 0);
    stereo_tick_end(memory, machine->state.registers.right, machine->blip.right, 1);

    blip_read_samples(machine->blip.left, machine->memory.samples.buffer, machine->samplerate / TIC80_FRAMERATE, TIC_STEREO_CHANNELS);
    blip_read_samples(machine->blip.right, machine->memory.samples.buffer + 1, machine->samplerate / TIC80_FRAMERATE, TIC_STEREO_CHANNELS);

    machine->state.setpix = setPixelOvr;
    machine->state.getpix = getPixelOvr;
    machine->state.drawhline = drawHLineOvr;
}

void tic_api_sfx(tic_mem* memory, s32 index, s32 note, s32 octave, s32 duration, s32 channel, s32 volume, s32 speed)
{
    tic_machine* machine = (tic_machine*)memory;
    setSfxChannelData(memory, index, note, octave, duration, channel, volume, volume, speed);    
}

static void initCover(tic_mem* tic)
{
    const tic_cover_image* cover = &tic->cart.cover;

    if(cover->size)
    {
        gif_image* image = gif_read_data(cover->data, cover->size);

        if (image)
        {
            if (image->width == TIC80_WIDTH && image->height == TIC80_HEIGHT)
            {
                enum { Size = TIC80_WIDTH * TIC80_HEIGHT };

                for (s32 i = 0; i < Size; i++)
                {
                    const gif_color* c = &image->palette[image->buffer[i]];
                    u8 color = tic_tool_find_closest_color(tic->cart.bank0.palette.scn.colors, c);
                    tic_tool_poke4(tic->ram.vram.screen.data, i, color);
                }
            }

            gif_close(image);
        }
    }
}

void tic_api_sync(tic_mem* tic, u32 mask, s32 bank, bool toCart)
{
    tic_machine* machine = (tic_machine*)tic;

    static const struct {s32 bank; s32 ram; s32 size;} Sections[] = 
    {
        {offsetof(tic_bank, tiles),         offsetof(tic_ram, tiles),           sizeof(tic_tiles)   },
        {offsetof(tic_bank, sprites),       offsetof(tic_ram, sprites),         sizeof(tic_tiles)   },
        {offsetof(tic_bank, map),           offsetof(tic_ram, map),             sizeof(tic_map)     },
        {offsetof(tic_bank, sfx),           offsetof(tic_ram, sfx),             sizeof(tic_sfx)     },
        {offsetof(tic_bank, music),         offsetof(tic_ram, music),           sizeof(tic_music)   },
        {offsetof(tic_bank, palette.scn),   offsetof(tic_ram, vram.palette),    sizeof(tic_palette) },
        {offsetof(tic_bank, flags),         offsetof(tic_ram, flags),           sizeof(tic_flags)   },
    };

    enum{Count = COUNT_OF(Sections), Mask = (1 << Count) - 1};

    if(mask == 0) mask = Mask;
    
    mask &= ~machine->state.synced & Mask;

    assert(bank >= 0 && bank < TIC_BANKS);

    for(s32 i = 0; i < Count; i++)
    {
        if(mask & (1 << i))
            toCart
                ? memcpy((u8*)&tic->cart.banks[bank] + Sections[i].bank, (u8*)&tic->ram + Sections[i].ram, Sections[i].size)
                : memcpy((u8*)&tic->ram + Sections[i].ram, (u8*)&tic->cart.banks[bank] + Sections[i].bank, Sections[i].size);
    }

    // copy OVR palette
    {
        enum {PaletteIndex = 5};

        if(mask & (1 << PaletteIndex))
            toCart
                ? memcpy(&tic->cart.banks[bank].palette.ovr, &machine->state.ovr.palette, sizeof(tic_palette))
                : memcpy(&machine->state.ovr.palette, &tic->cart.banks[bank].palette.ovr, sizeof(tic_palette));
    }

    machine->state.synced |= mask;
}

static void cart2ram(tic_mem* memory)
{
    static const u8 Font[] = 
    {
        #include "font.inl"
    };

    memcpy(memory->ram.font.data, Font, sizeof Font);

    tic_api_sync(memory, 0, 0, false);
    initCover(memory);
}

static const char* readMetatag(const char* code, const char* tag, const char* comment)
{
    const char* start = NULL;

    {
        static char format[] = "%s %s:";

        char* tagBuffer = malloc(strlen(format) + strlen(tag));

        if(tagBuffer)
        {
            sprintf(tagBuffer, format, comment, tag);
            if((start = strstr(code, tagBuffer)))
                start += strlen(tagBuffer);
            free(tagBuffer);            
        }
    }

    if(start)
    {
        const char* end = strstr(start, "\n");

        if(end)
        {
            while(*start <= ' ' && start < end) start++;
            while(*(end-1) <= ' ' && end > start) end--;

            const s32 size = (s32)(end-start);

            char* value = (char*)malloc(size+1);

            if(value)
            {
                memset(value, 0, size+1);
                memcpy(value, start, size);

                return value;               
            }
        }
    }

    return NULL;
}

static bool compareMetatag(const char* code, const char* tag, const char* value, const char* comment)
{
    bool result = false;

    const char* str = readMetatag(code, tag, comment);

    if(str)
    {
        result = strcmp(str, value) == 0;
        free((void*)str);
    }

    return result;
}

const tic_script_config* tic_core_script_config(tic_mem* memory)
{
    const char* code = memory->cart.code.data;

#if defined(TIC_BUILD_WITH_MOON)
    if(compareMetatag(code, "script", "moon", getMoonScriptConfig()->singleComment) ||
        compareMetatag(code, "script", "moonscript", getMoonScriptConfig()->singleComment)) 
        return getMoonScriptConfig();
#endif

#if defined(TIC_BUILD_WITH_FENNEL)
    if(compareMetatag(code, "script", "fennel", getFennelConfig()->singleComment))
        return getFennelConfig();
#endif

#if defined(TIC_BUILD_WITH_JS)
    if(compareMetatag(code, "script", "js", getJsScriptConfig()->singleComment) ||
        compareMetatag(code, "script", "javascript", getJsScriptConfig()->singleComment)) 
        return getJsScriptConfig();
#endif

#if defined(TIC_BUILD_WITH_WREN)
    if(compareMetatag(code, "script", "wren", getWrenScriptConfig()->singleComment)) 
        return getWrenScriptConfig();
#endif

#if defined(TIC_BUILD_WITH_SQUIRREL)
    if (compareMetatag(code, "script", "squirrel", getSquirrelScriptConfig()->singleComment))
        return getSquirrelScriptConfig();
#endif

#if defined(TIC_BUILD_WITH_LUA)
    return getLuaScriptConfig();
#elif defined(TIC_BUILD_WITH_JS)
    return getJsScriptConfig();
#elif defined(TIC_BUILD_WITH_WREN)
    return getWrenScriptConfig();
#elif defined(TIC_BUILD_WITH_SQUIRREL)
    return getSquirrelScriptConfig();
#endif
}

static void updateSaveid(tic_mem* memory)
{
    memset(memory->saveid, 0, sizeof memory->saveid);
    const char* saveid = readMetatag(memory->cart.code.data, "saveid", tic_core_script_config(memory)->singleComment);
    if(saveid)
    {
        strncpy(memory->saveid, saveid, TIC_SAVEID_SIZE-1);
        free((void*)saveid);
    }
}

void tic_core_tick(tic_mem* tic, tic_tick_data* data)
{
    tic_machine* machine = (tic_machine*)tic;

    machine->data = data;
    
    if(!machine->state.initialized)
    {
        const char* code = tic->cart.code.data;

        bool done = false;
        const tic_script_config* config = NULL;

        if(strlen(code))
        {
            config = tic_core_script_config(tic);
            cart2ram(tic);

            machine->state.synced = 0;
            tic->input.data = 0;
            
            if(compareMetatag(code, "input", "mouse", config->singleComment))
                tic->input.mouse = 1;
            else if(compareMetatag(code, "input", "gamepad", config->singleComment))
                tic->input.gamepad = 1;
            else if(compareMetatag(code, "input", "keyboard", config->singleComment))
                tic->input.keyboard = 1;
            else tic->input.data = -1;  // default is all enabled

            data->start = data->counter();
            
            done = config->init(tic, code);
        }
        else
        {
            machine->data->error(machine->data->data, "the code is empty");
        }

        if(done)
        {
            machine->state.tick = config->tick;
            machine->state.scanline = config->scanline;
            machine->state.ovr.callback = config->overline;

            machine->state.initialized = true;
        }
        else return;
    }

    {
        if(!tic->input.keyboard)
            ZEROMEM(tic->ram.input.keyboard);

        if(!tic->input.gamepad)
            ZEROMEM(tic->ram.input.gamepads);

        if(!tic->input.mouse)
            ZEROMEM(tic->ram.input.mouse);
    }

    machine->state.tick(tic);
}

double tic_api_time(tic_mem* memory)
{
    tic_machine* machine = (tic_machine*)memory;
    return (double)((machine->data->counter() - machine->data->start)*1000)/machine->data->freq();
}

s32 tic_api_tstamp(tic_mem* memory)
{
    tic_machine* machine = (tic_machine*)memory;
    return (s32)time(NULL);
}

u32 tic_api_btnp(tic_mem* tic, s32 index, s32 hold, s32 period)
{
    tic_machine* machine = (tic_machine*)tic;

    if(index < 0)
    {
        return (~machine->state.gamepads.previous.data) & machine->memory.ram.input.gamepads.data;
    }
    else if(hold < 0 || period < 0)
    {
        return ((~machine->state.gamepads.previous.data) & machine->memory.ram.input.gamepads.data) & (1 << index);
    }

    tic80_gamepads previous;
    
    previous.data = machine->state.gamepads.holds[index] >= hold 
        ? period && machine->state.gamepads.holds[index] % period ? machine->state.gamepads.previous.data : 0
        : machine->state.gamepads.previous.data;

    return ((~previous.data) & machine->memory.ram.input.gamepads.data) & (1 << index);
}

u32 tic_api_btn(tic_mem* tic, s32 index)
{
    tic_machine* machine = (tic_machine*)tic;

    if(index < 0)
    {
        return (~machine->state.gamepads.previous.data) & machine->memory.ram.input.gamepads.data;
    }
    else
    {
        return ((~machine->state.gamepads.previous.data) & machine->memory.ram.input.gamepads.data) & (1 << index);
    }
}

bool tic_api_key(tic_mem* tic, tic_key key)
{
    return key > tic_key_unknown 
        ? isKeyPressed(&tic->ram.input.keyboard, key) 
        : tic->ram.input.keyboard.data;
}

bool tic_api_keyp(tic_mem* tic, tic_key key, s32 hold, s32 period)
{
    tic_machine* machine = (tic_machine*)tic;

    if(key > tic_key_unknown)
    {           
        bool prevDown = hold >= 0 && period >= 0 && machine->state.keyboard.holds[key] >= hold
            ? period && machine->state.keyboard.holds[key] % period 
                ? isKeyPressed(&machine->state.keyboard.previous, key) 
                : false
            : isKeyPressed(&machine->state.keyboard.previous, key);

        bool down = isKeyPressed(&tic->ram.input.keyboard, key);

        return !prevDown && down;
    }

    for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
    {
        tic_key key = tic->ram.input.keyboard.keys[i];

        if(key)
        {
            bool wasPressed = false;

            for(s32 p = 0; p < TIC80_KEY_BUFFER; p++)
            {
                if(machine->state.keyboard.previous.keys[p] == key)
                {
                    wasPressed = true;
                    break;
                }
            }

            if(!wasPressed)
                return true;
        }
    }

    return false;
}

// copied from SDL2
static inline void memset4(void *dst, u32 val, u32 dwords)
{
#if defined(__GNUC__) && defined(i386)
    s32 u0, u1, u2;
    __asm__ __volatile__ (
        "cld \n\t"
        "rep ; stosl \n\t"
        : "=&D" (u0), "=&a" (u1), "=&c" (u2)
        : "0" (dst), "1" (val), "2" (dwords)
        : "memory"
    );
#else
    u32 _n = (dwords + 3) / 4;
    u32 *_p = (u32*)dst;
    u32 _val = (val);
    if (dwords == 0)
        return;
    switch (dwords % 4)
    {
        case 0: do {    *_p++ = _val;
        case 3:         *_p++ = _val;
        case 2:         *_p++ = _val;
        case 1:         *_p++ = _val;
        } while ( --_n );
    }
#endif
}

void tic_core_blit_ex(tic_mem* tic, tic80_pixel_color_format fmt, tic_scanline scanline, tic_overline overline, void* data)
{
    // init OVR palette
    {
        tic_machine* machine = (tic_machine*)tic;

        const tic_palette* ovr = &machine->state.ovr.palette;
        bool ovrEmpty = true;
        for(s32 i = 0; i < sizeof(tic_palette); i++)
            if(ovr->data[i])
                ovrEmpty = false;

        memcpy(machine->state.ovr.raw, tic_tool_palette_blit(ovrEmpty ? &tic->ram.vram.palette : ovr, fmt), sizeof machine->state.ovr.raw);
    }

    if(scanline)
        scanline(tic, 0, data);

    const u32* pal = tic_tool_palette_blit(&tic->ram.vram.palette, fmt);

    enum {Top = (TIC80_FULLHEIGHT-TIC80_HEIGHT)/2, Bottom = Top};
    enum {Left = (TIC80_FULLWIDTH-TIC80_WIDTH)/2, Right = Left};

    u32* out = tic->screen;

    memset4(&out[0 * TIC80_FULLWIDTH], pal[tic->ram.vram.vars.border], TIC80_FULLWIDTH*Top);

    u32* rowPtr = out + (Top*TIC80_FULLWIDTH);
    for(s32 r = 0; r < TIC80_HEIGHT; r++, rowPtr += TIC80_FULLWIDTH)
    {
        u32 *colPtr = rowPtr + Left;
        memset4(rowPtr, pal[tic->ram.vram.vars.border], Left);

        s32 pos = (r + tic->ram.vram.vars.offset.y + TIC80_HEIGHT) % TIC80_HEIGHT * TIC80_WIDTH >> 1;

        u32 x = (-tic->ram.vram.vars.offset.x + TIC80_WIDTH) % TIC80_WIDTH;
        for(s32 c = 0; c < TIC80_WIDTH / 2; c++)
        {
            u8 val = ((u8*)tic->ram.vram.screen.data)[pos + c];
            *(colPtr + (x++ % TIC80_WIDTH)) = pal[val & 0xf];
            *(colPtr + (x++ % TIC80_WIDTH)) = pal[val >> 4];
        }

        memset4(rowPtr + (TIC80_FULLWIDTH-Right), pal[tic->ram.vram.vars.border], Right);
            
        if(scanline && (r < TIC80_HEIGHT-1))
        {
            scanline(tic, r+1, data);
            pal = tic_tool_palette_blit(&tic->ram.vram.palette, fmt);
        }
    }

    memset4(&out[(TIC80_FULLHEIGHT-Bottom) * TIC80_FULLWIDTH], pal[tic->ram.vram.vars.border], TIC80_FULLWIDTH*Bottom);

    if(overline)
        overline(tic, data);

}

static inline void scanline(tic_mem* memory, s32 row, void* data)
{
    tic_machine* machine = (tic_machine*)memory;

    if(machine->state.initialized)
        machine->state.scanline(memory, row, data);
}

static inline void overline(tic_mem* memory, void* data)
{
    tic_machine* machine = (tic_machine*)memory;

    if(machine->state.initialized)
        machine->state.ovr.callback(memory, data);
}

void tic_core_blit(tic_mem* tic, tic80_pixel_color_format fmt)
{
    tic_core_blit_ex(tic, fmt, scanline, overline, NULL);
}

u8 tic_api_peek(tic_mem* memory, s32 address)
{
    if(address >=0 && address < sizeof(tic_ram))
        return *((u8*)&memory->ram + address);

    return 0;
}

void tic_api_poke(tic_mem* memory, s32 address, u8 value)
{
    if(address >=0 && address < sizeof(tic_ram))
        *((u8*)&memory->ram + address) = value;
}

u8 tic_api_peek4(tic_mem* memory, s32 address)
{
    if(address >=0 && address < sizeof(tic_ram)*2)
        return tic_tool_peek4((u8*)&memory->ram, address);

    return 0;
}

void tic_api_poke4(tic_mem* memory, s32 address, u8 value)
{
    if(address >=0 && address < sizeof(tic_ram)*2)
        tic_tool_poke4((u8*)&memory->ram, address, value);
}

void tic_api_memcpy(tic_mem* memory, s32 dst, s32 src, s32 size)
{
    s32 bound = sizeof(tic_ram) - size;

    if(size >= 0 
        && size <= sizeof(tic_ram) 
        && dst >= 0 
        && src >= 0 
        && dst <= bound 
        && src <= bound)
    {
        u8* base = (u8*)&memory->ram;
        memcpy(base + dst, base + src, size);
    }
}

void tic_api_memset(tic_mem* memory, s32 dst, u8 val, s32 size)
{
    s32 bound = sizeof(tic_ram) - size;

    if(size >= 0 
        && size <= sizeof(tic_ram) 
        && dst >= 0 
        && dst <= bound)
    {
        u8* base = (u8*)&memory->ram;
        memset(base + dst, val, size);
    }
}

void tic_api_trace(tic_mem* memory, const char* text, u8 color)
{
    tic_machine* machine = (tic_machine*)memory;
    machine->data->trace(machine->data->data, text ? text : "nil", color);
}

u32 tic_api_pmem(tic_mem* tic, s32 index, u32 value, bool set)
{
    u32 old = tic->ram.persistent.data[index];

    if(set)
        tic->ram.persistent.data[index] = value;

    return old;
}

void tic_api_exit(tic_mem* tic)
{
    tic_machine* machine = (tic_machine*)tic;
    machine->data->exit(machine->data->data);
}

void tic_api_mouse(tic_mem* memory) {}

tic_mem* tic_core_create(s32 samplerate)
{
    tic_machine* machine = (tic_machine*)malloc(sizeof(tic_machine));
    memset(machine, 0, sizeof(tic_machine));

    if (machine != (tic_machine*)&machine->memory)
    {
        free(machine);
        return NULL;
    }

    machine->memory.screen_format = TIC80_PIXEL_COLOR_RGBA8888;
    machine->samplerate = samplerate;
#ifdef _3DS
    // To feed texture data directly to the 3DS GPU, linearly allocated memory is required, which is
    // not guaranteed by malloc.
    // Additionally, allocate TIC80_FULLHEIGHT + 1 lines to minimize glitches in linear scaling mode.
    machine->memory.screen = linearAlloc(TIC80_FULLWIDTH * (TIC80_FULLHEIGHT + 1) * sizeof(u32));
#endif
    machine->memory.samples.size = samplerate * TIC_STEREO_CHANNELS / TIC80_FRAMERATE * sizeof(s16);
    machine->memory.samples.buffer = malloc(machine->memory.samples.size);

    machine->blip.left = blip_new(samplerate / 10);
    machine->blip.right = blip_new(samplerate / 10);

    blip_set_rates(machine->blip.left, CLOCKRATE, samplerate);
    blip_set_rates(machine->blip.right, CLOCKRATE, samplerate);

    tic_api_reset(&machine->memory);

    return &machine->memory;
}
