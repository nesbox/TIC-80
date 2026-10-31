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

#include "clipboard.h"
#include "tools.h"
#include "system.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void toClipboard(const void* data, s32 size, bool flip)
{
    if(data)
    {
        enum
        {
            Len = 2
        };

        char* clipboard = (char*)malloc(size * Len + 1);

        if(clipboard)
        {

            tic_tool_buf2str(data, size, clipboard, flip);
            tic_sys_clipboard_set(clipboard);
            free(clipboard);
        }
    }
}

static void removeWhiteSpaces(char* str)
{
    s32 i = 0;
    s32 len = (s32)strlen(str);

    for(s32 j = 0; j < len; j++)
        if(!isspace((u8)str[j]))
            str[i++] = str[j];

    str[i] = '\0';
}

bool fromClipboard(void* data, s32 size, bool flip, bool remove_white_spaces, bool sameSize)
{
    if(tic_sys_clipboard_has())
    {
        char* clipboard = tic_sys_clipboard_get();

        if(remove_white_spaces)
            removeWhiteSpaces(clipboard);

        bool valid = sameSize
                         ? strlen(clipboard) == (size_t)size * 2
                         : strlen(clipboard) <= (size_t)size * 2;

        if(valid) tic_tool_str2buf(clipboard, (s32)strlen(clipboard), data, flip);

        tic_sys_clipboard_free(clipboard);

        return valid;
    }

    return false;
}
