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

#include "export.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char* const ExportTypes[] =
    {
#define EXPORT_TYPE(name, label) #name,
        EXPORT_CMD_LIST(EXPORT_TYPE)
#undef EXPORT_TYPE
};

static const char* export_suffix(char* buffer, s32 size, s32 id, const ExportState* state)
{
    switch(id)
    {
        case Export_win: return "-win";
        case Export_linux: return "-linux";
        case Export_linuxarm: return "-linuxarm";
        case Export_mac: return "-macos";
        case Export_macintel: return "-macintel";
        case Export_html: return "-html";
        case Export_tiles: return "-tiles";
        case Export_sprites: return "-sprites";
        case Export_map: return "-map";
        case Export_mapimg: return "-map";
        case Export_screen: return "-screen";

        case Export_sfx:
            snprintf(buffer, size, "-sfx-%02i", state->sfx);
            return buffer;

        case Export_music:
            snprintf(buffer, size, "-music-%02i", state->music);
            return buffer;

        default:
            return "";
    }
}

#if defined(TIC80_PRO)
static s32 export_bank(s32 id, const ExportState* state)
{
    switch(id)
    {
        case Export_tiles:
        case Export_sprites: return state->spriteBank;

        case Export_map:
        case Export_mapimg:
        case Export_screen: return state->mapBank;

        case Export_music: return state->musicBank;

        default: return 0;
    }
}
#endif

// Appends to the command; false if the text no longer fits `size`.
static bool append(char* out, s32 size, s32* length, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    s32 written = vsnprintf(out + *length, size - *length, format, args);
    va_end(args);

    if(written < 0 || written >= size - *length)
        return false;

    *length += written;
    return true;
}

bool export_command(char* out, s32 size, s32 id, const ExportState* state)
{
    if(id < 0 || id >= Export_Count || size <= 0)
        return false;

    char filename[TICNAME_MAX + 16];

    if(id == Export_help)
        snprintf(filename, sizeof filename, "tic80-help");
    else
    {
        const char* cartName = state->cartName ? state->cartName : "";
        const char* base = cartName[0] && !strchr(cartName, ' ') ? cartName : "game";
        char suffix[16];

        snprintf(filename, sizeof filename, "%s%s", base,
                 export_suffix(suffix, sizeof suffix, id, state));
    }

    s32 length = snprintf(out, size, "export %s %s", ExportTypes[id], filename);

    if(length < 0 || length >= size)
        return false;

    switch(id)
    {
        case Export_tiles:
        case Export_sprites:
        case Export_map:
        case Export_mapimg:
#if defined(TIC80_PRO)
            if(!append(out, size, &length, " bank=%i", export_bank(id, state)))
                return false;
#endif
            break;

        case Export_screen:
#if defined(TIC80_PRO)
            if(!append(out, size, &length, " bank=%i", export_bank(id, state)))
                return false;
#endif
            if(!append(out, size, &length, " vbank=%i", state->spriteVbank1 ? 1 : 0))
                return false;
            break;

        case Export_sfx:
            if(!append(out, size, &length, " id=%i", state->sfx))
                return false;
            break;

        case Export_music:
            if(!append(out, size, &length, " id=%i", state->music))
                return false;

#if defined(TIC80_PRO)
            if(!append(out, size, &length, " bank=%i", export_bank(id, state)))
                return false;
#endif
            break;

        default:
            break;
    }

#if defined(TIC80_PRO)
    if(state->alone)
        switch(id)
        {
            case Export_win:
            case Export_linux:
            case Export_linuxarm:
            case Export_mac:
            case Export_macintel:
                if(!append(out, size, &length, " alone=1"))
                    return false;
                break;

            default:
                break;
        }
#endif

    return true;
}
