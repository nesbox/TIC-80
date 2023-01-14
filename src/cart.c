// MIT License

// Copyright (c) 2020 Vadim Grigoruk @nesbox // grigoruk@gmail.com

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

#include "retro_endianness.h"
#include "cart.h"

#if defined(BUILD_DEPRECATED)
#include "tools.h"
#include "ext/gif.h"
#endif

#include <string.h>
#include <stdlib.h>
#include "tic_assert.h"
#include "tools.h"

typedef enum
{
    CHUNK_DUMMY,        // 0
    CHUNK_TILES,        // 1
    CHUNK_SPRITES,      // 2
    CHUNK_COVER_DEP,    // 3 - deprecated chunk
    CHUNK_MAP,          // 4
    CHUNK_CODE,         // 5
    CHUNK_FLAGS,        // 6
    CHUNK_TEMP2,        // 7
    CHUNK_TEMP3,        // 8
    CHUNK_SAMPLES,      // 9
    CHUNK_WAVEFORM,     // 10
    CHUNK_TEMP4,        // 11
    CHUNK_PALETTE,      // 12
    CHUNK_PATTERNS_DEP, // 13 - deprecated chunk
    CHUNK_MUSIC,        // 14
    CHUNK_PATTERNS,     // 15
    CHUNK_CODE_ZIP,     // 16
    CHUNK_DEFAULT,      // 17
    CHUNK_SCREEN,       // 18
    CHUNK_BINARY,       // 19
    CHUNK_LANG,         // 20
} ChunkType;

typedef struct
{
#if RETRO_IS_BIG_ENDIAN
    u32 bank:TIC_BANK_BITS;
    u32 type:5; // ChunkType
#else
    u32 type:5; // ChunkType
    u32 bank:TIC_BANK_BITS;
#endif
    u32 size:TIC_BANKSIZE_BITS; // max chunk size is 64K
    u32 temp:8;
} Chunk;

static_assert(sizeof(Chunk) == 4, "tic_chunk_size");

static const u8 Sweetie16[] = {0x1a, 0x1c, 0x2c, 0x5d, 0x27, 0x5d, 0xb1, 0x3e, 0x53, 0xef, 0x7d, 0x57, 0xff, 0xcd, 0x75, 0xa7, 0xf0, 0x70, 0x38, 0xb7, 0x64, 0x25, 0x71, 0x79, 0x29, 0x36, 0x6f, 0x3b, 0x5d, 0xc9, 0x41, 0xa6, 0xf6, 0x73, 0xef, 0xf7, 0xf4, 0xf4, 0xf4, 0x94, 0xb0, 0xc2, 0x56, 0x6c, 0x86, 0x33, 0x3c, 0x57};
static const u8 Waveforms[] = {0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe, 0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01, 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe, 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe};

static s32 chunkSize(const Chunk* chunk)
{
    return chunk->size == 0 && (chunk->type == CHUNK_CODE || chunk->type == CHUNK_BINARY) ? TIC_BANK_SIZE : retro_le_to_cpu16(chunk->size);
}

void tic_cart_load(tic_cartridge* cart, const u8* buffer, s32 size)
{
    memset(cart, 0, sizeof(tic_cartridge));
    const u8* end = buffer + size;
    u8 *chunk_cart = NULL;

    // check if this cartridge is in PNG format
    if (!memcmp(buffer, "\x89PNG", 4))
    {
        s32 siz;
        const u8* ptr = buffer + 8;
        // iterate on chunks until we find a cartridge
        while (ptr < end)
        {
            siz = ((ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3]);
            if (!memcmp(ptr + 4, "caRt", 4) && siz > 0)
            {
                chunk_cart = malloc(sizeof(tic_cartridge));
                if (chunk_cart)
                {
                    size = tic_tool_unzip(chunk_cart, sizeof(tic_cartridge), ptr + 8, siz);
                    buffer = chunk_cart;
                    end = buffer + size;
                }
                break;
            }
            ptr += siz + 12;
        }
        // error, no TIC-80 cartridge chunk in PNG???
        if (!chunk_cart)
            return;
    }

#define LOAD_CHUNK(to) memcpy(&to, ptr, MIN(sizeof(to), chunk->size ? retro_le_to_cpu16(chunk->size) : TIC_BANK_SIZE))

    // load palette chunk first
    {
        const u8* ptr = buffer;
        while (ptr < end)
        {
            const Chunk* chunk = (Chunk*)ptr;
            ptr += sizeof(Chunk);

            switch (chunk->type)
            {
            case CHUNK_PALETTE:
                LOAD_CHUNK(cart->banks[chunk->bank].palette);
                break;
            case CHUNK_DEFAULT:
                memcpy(&cart->banks[chunk->bank].palette, Sweetie16, sizeof Sweetie16);
                memcpy(&cart->banks[chunk->bank].sfx.waveforms, Waveforms, sizeof Waveforms);
                break;
            default: break;
            }

            ptr += chunkSize(chunk);
        }

#if defined(BUILD_DEPRECATED)
        // workaround to support ancient carts without palette
        // load DB16 palette if it not exists
        if (EMPTY(cart->bank0.palette.vbank0.data))
        {
            static const u8 DB16[] = { 0x14, 0x0c, 0x1c, 0x44, 0x24, 0x34, 0x30, 0x34, 0x6d, 0x4e, 0x4a, 0x4e, 0x85, 0x4c, 0x30, 0x34, 0x65, 0x24, 0xd0, 0x46, 0x48, 0x75, 0x71, 0x61, 0x59, 0x7d, 0xce, 0xd2, 0x7d, 0x2c, 0x85, 0x95, 0xa1, 0x6d, 0xaa, 0x2c, 0xd2, 0xaa, 0x99, 0x6d, 0xc2, 0xca, 0xda, 0xd4, 0x5e, 0xde, 0xee, 0xd6 };
            memcpy(cart->bank0.palette.vbank0.data, DB16, sizeof DB16);
        }
#endif
    }

    struct CodeChunk {s32 size; const char* data;} code[TIC_BANKS] = {0};
    struct BinaryChunk {s32 size; const char* data;} binary[TIC_BINARY_BANKS] = {0};

    {
        const u8* ptr = buffer;
        while(ptr < end)
        {
            const Chunk* chunk = (Chunk*)ptr;
            ptr += sizeof(Chunk);

            switch(chunk->type)
            {
            case CHUNK_TILES:       LOAD_CHUNK(cart->banks[chunk->bank].tiles);             break;
            case CHUNK_SPRITES:     LOAD_CHUNK(cart->banks[chunk->bank].sprites);           break;
            case CHUNK_MAP:         LOAD_CHUNK(cart->banks[chunk->bank].map);               break;
            case CHUNK_SAMPLES:     LOAD_CHUNK(cart->banks[chunk->bank].sfx.samples);       break;
            case CHUNK_WAVEFORM:    LOAD_CHUNK(cart->banks[chunk->bank].sfx.waveforms);     break;
            case CHUNK_MUSIC:       LOAD_CHUNK(cart->banks[chunk->bank].music.tracks);      break;
            case CHUNK_PATTERNS:    LOAD_CHUNK(cart->banks[chunk->bank].music.patterns);    break;
            case CHUNK_FLAGS:       LOAD_CHUNK(cart->banks[chunk->bank].flags);             break;
            case CHUNK_SCREEN:      LOAD_CHUNK(cart->banks[chunk->bank].screen);            break;
            case CHUNK_LANG:        LOAD_CHUNK(cart->lang);                                 break;
            case CHUNK_BINARY:      
                binary[chunk->bank] = (struct BinaryChunk){chunkSize(chunk), ptr};
                break;
            case CHUNK_CODE:        
                code[chunk->bank] = (struct CodeChunk){chunkSize(chunk), ptr};
                break;
#if defined(BUILD_DEPRECATED)
            case CHUNK_CODE_ZIP:
                tic_tool_unzip(cart->code.data, TIC_CODE_SIZE, ptr, retro_le_to_cpu16(chunk->size));
                break;
            case CHUNK_COVER_DEP:
                {
                    // workaround to load deprecated cover section
                    gif_image* image = gif_read_data(ptr, retro_le_to_cpu16(chunk->size));

                    if (image)
                    {
                        if(image->width == TIC80_WIDTH && image->height == TIC80_HEIGHT)
                            for (s32 i = 0; i < TIC80_WIDTH * TIC80_HEIGHT; i++)
                                tic_tool_poke4(cart->bank0.screen.data, i, 
                                    tic_nearest_color(cart->bank0.palette.vbank0.colors, (const tic_rgb*)&image->palette[image->buffer[i]], TIC_PALETTE_SIZE));

                        gif_close(image);
                    }
                }
                break;
            case CHUNK_PATTERNS_DEP: 
                {
                    // workaround to load deprecated music patterns section
                    // and automatically convert volume value to a command
                    tic_patterns* ptrns = &cart->banks[chunk->bank].music.patterns;
                    LOAD_CHUNK(*ptrns);
                    for(s32 i = 0; i < MUSIC_PATTERNS; i++)
                        for(s32 r = 0; r < MUSIC_PATTERN_ROWS; r++)
                        {
                            tic_track_row* row = &ptrns->data[i].rows[r];
                            if(row->note >= NoteStart && row->command == tic_music_cmd_empty)
                            {
                                row->command = tic_music_cmd_volume;
                                row->param2 = row->param1 = MAX_VOLUME - row->param1;
                            }
                        }
                }
                break;
#endif
            default: break;
            }

            ptr += chunkSize(chunk);
        }
#undef LOAD_CHUNK

        {
            u32 total_size = 0;
            char* ptr = cart->binary.data;
            RFOR(const struct BinaryChunk*, chunk, binary)
                if (chunk->size)
                {
                    memcpy(ptr, chunk->data, chunk->size);
                    ptr += chunk->size;
                    total_size += chunk->size;
                }
            cart->binary.size = total_size;
        }

        if (!*cart->code.data)
        {
            char* ptr = cart->code.data;
            RFOR(const struct CodeChunk*, chunk, code)
                if (chunk->data)
                {
                    memcpy(ptr, chunk->data, chunk->size);
                    ptr += chunk->size;
                }
        }
    }
    // if we have allocated the buffer from a PNG chunk
    if (chunk_cart)
        free(chunk_cart);
}


static s32 calcBufferSize(const void* buffer, s32 size)
{
    const u8* ptr = (u8*)buffer + size - 1;
    const u8* end = (u8*)buffer;

    while(ptr >= end)
    {
        if(*ptr) break;

        ptr--;
        size--;
    }

    return size;
}

static u8* saveFixedChunk(u8* buffer, ChunkType type, const void* from, s32 size, s32 bank)
{
    if(size)
    {
        Chunk chunk = {.type = type, .bank = bank, .size = retro_le_to_cpu16(size), .temp = 0};
        memcpy(buffer, &chunk, sizeof(Chunk));
        buffer += sizeof(Chunk);

        memcpy(buffer, from, size);
        buffer += size;
    }

    return buffer;
}

static u8* saveChunk(u8* buffer, ChunkType type, const void* from, s32 size, s32 bank)
{
    s32 chunkSize = calcBufferSize(from, size);

    return saveFixedChunk(buffer, type, from, chunkSize, bank);
}

s32 tic_cart_save(const tic_cartridge* cart, u8* buffer)
{
    u8* start = buffer;

#define SAVE_CHUNK(ID, FROM, BANK) saveChunk(buffer, ID, &FROM, sizeof(FROM), BANK)

    tic_waveforms defaultWaveforms = {0};
    tic_palettes defaultPalettes = {0};

    memcpy(&defaultWaveforms, Waveforms, sizeof Waveforms);
    memcpy(&defaultPalettes, Sweetie16, sizeof Sweetie16);

    for(s32 i = 0; i < TIC_BANKS; i++)
    {
        if(memcmp(&cart->banks[i].sfx.waveforms, &defaultWaveforms, sizeof defaultWaveforms) == 0
            && memcmp(&cart->banks[i].palette, &defaultPalettes, sizeof defaultPalettes) == 0)
        {
            Chunk chunk = {.type = CHUNK_DEFAULT, .bank = i, .size = 0, .temp = 0};
            memcpy(buffer, &chunk, sizeof chunk);
            buffer += sizeof chunk;
        }
        else
        {
            buffer = SAVE_CHUNK(CHUNK_PALETTE, cart->banks[i].palette, i);
            buffer = SAVE_CHUNK(CHUNK_WAVEFORM, cart->banks[i].sfx.waveforms, i);
        }

        buffer = SAVE_CHUNK(CHUNK_TILES,    cart->banks[i].tiles,           i);
        buffer = SAVE_CHUNK(CHUNK_SPRITES,  cart->banks[i].sprites,         i);
        buffer = SAVE_CHUNK(CHUNK_MAP,      cart->banks[i].map,             i);
        buffer = SAVE_CHUNK(CHUNK_SAMPLES,  cart->banks[i].sfx.samples,     i);
        buffer = SAVE_CHUNK(CHUNK_PATTERNS, cart->banks[i].music.patterns,  i);
        buffer = SAVE_CHUNK(CHUNK_MUSIC,    cart->banks[i].music.tracks,    i);
        buffer = SAVE_CHUNK(CHUNK_FLAGS,    cart->banks[i].flags,           i);
        buffer = SAVE_CHUNK(CHUNK_SCREEN,   cart->banks[i].screen,          i);
    }

    const char* ptr;
    if (cart->binary.size) 
    {
        ptr = cart->binary.data;
        s32 remaining = cart->binary.size;
        for (s32 i = cart->binary.size / TIC_BANK_SIZE; i >= 0; --i, ptr += TIC_BANK_SIZE) 
        {
            buffer = saveFixedChunk(buffer, CHUNK_BINARY, ptr, MIN(remaining, TIC_BANK_SIZE), i);
            remaining -= TIC_BANK_SIZE;
        }
    }

    ptr = cart->code.data;
    for(s32 i = strlen(ptr) / TIC_BANK_SIZE; i >= 0; --i, ptr += TIC_BANK_SIZE)
        buffer = saveFixedChunk(buffer, CHUNK_CODE, ptr, MIN(strlen(ptr), TIC_BANK_SIZE), i);

    if(cart->lang)
        SAVE_CHUNK(CHUNK_LANG, cart->lang, 0);

#undef SAVE_CHUNK

    return (s32)(buffer - start);
}
