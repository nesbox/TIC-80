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

#include "keyboard.h"
#include "system.h"

bool keyboard_pressed(tic_mem* tic, tic_key key)
{
    return tic_api_keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD);
}

bool keyboard_pressed_once(tic_mem* tic, s32 key)
{
    return tic_api_keyp(tic, (tic_key)(key), -1, -1);
}

bool keyboard_enter(tic_mem* tic, s32 hold, s32 period)
{
    return tic_api_keyp(tic, tic_key_return, hold, period) ||
           tic_api_keyp(tic, tic_key_numpadenter, hold, period);
}

char keyboard_text(tic_mem* tic)
{
    // Ctrl-based shortcuts should not also inject printable characters.
    if(tic_api_key(tic, tic_key_ctrl) && !tic_api_key(tic, tic_key_alt))
        return '\0';

    char text;
    if(!tic_sys_keyboard_text(&text))
    {
        tic80_input* input = &tic->ram->input;

#ifdef KEYBOARD_LAYOUT_ES
        // US KEYS:                     " abcdefghijklmnopqrstuvwxyz0123456789-=[]\\;'`,./ ";
        static const char Symbols[] =   " abcdefghijklmnopqrstuvwxyz0123456789'!`+cn'o,.- ";
        static const char Shift[] =     " ABCDEFGHIJKLMNOPQRSTUVWXYZ=!\" $%&/()??^*CN\"a;:_ ";
        static const char Alt[] =       "                            |@#        []} {\\    ";
#else
        static const char Symbols[] =   " abcdefghijklmnopqrstuvwxyz0123456789-=[]\\;'`,./ ";
        static const char Shift[] =     " ABCDEFGHIJKLMNOPQRSTUVWXYZ)!@#$%^&*(_+{}|:\"~<>? ";
        static const char Alt[] =       "                                                  ";
#endif

        enum{Count = sizeof Symbols};

        for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
        {
            tic_key key = input->keyboard.keys[i];

            if(key > 0 && key < Count && tic_api_keyp(tic, key, KEYBOARD_HOLD, KEYBOARD_PERIOD))
            {
                bool caps = tic_api_key(tic, tic_key_capslock);
                bool shift = tic_api_key(tic, tic_key_shift);
                bool alt = tic_api_key(tic, tic_key_alt);

                if (caps && key >= tic_key_a && key <= tic_key_z) shift = !shift;

                return shift ? Shift[key]
                    : alt ? Alt[key]
                    : Symbols[key];
            }
        }

        return '\0';
    }

    return text;
}

ClipboardEvent keyboard_clipboard(tic_mem* tic)
{
    bool shift = tic_api_key(tic, tic_key_shift);
    bool ctrl = tic_api_key(tic, tic_key_ctrl);

    if(ctrl)
    {
        if(keyboard_pressed(tic, tic_key_insert) || keyboard_pressed(tic, tic_key_c)) return TIC_CLIPBOARD_COPY;
        else if(keyboard_pressed(tic, tic_key_x)) return TIC_CLIPBOARD_CUT;
        else if(keyboard_pressed(tic, tic_key_v)) return TIC_CLIPBOARD_PASTE;
    }
    else if(shift)
    {
        if(keyboard_pressed(tic, tic_key_delete)) return TIC_CLIPBOARD_CUT;
        else if(keyboard_pressed(tic, tic_key_insert)) return TIC_CLIPBOARD_PASTE;
    }

    return TIC_CLIPBOARD_NONE;
}
