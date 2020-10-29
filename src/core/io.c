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

#include "api.h"
#include "core.h"

STATIC_ASSERT(tic80_input, sizeof(tic80_input) == 12);

static bool isKeyPressed(const tic80_keyboard* input, tic_key key)
{
    for (s32 i = 0; i < TIC80_KEY_BUFFER; i++)
        if (input->keys[i] == key)
            return true;

    return false;
}

u32 tic_api_btnp(tic_mem* tic, s32 index, s32 hold, s32 period)
{
    tic_core* core = (tic_core*)tic;

    if (index < 0)
    {
        return (~core->state.gamepads.previous.data) & core->memory.ram.input.gamepads.data;
    }
    else if (hold < 0 || period < 0)
    {
        return ((~core->state.gamepads.previous.data) & core->memory.ram.input.gamepads.data) & (1 << index);
    }

    tic80_gamepads previous;

    previous.data = core->state.gamepads.holds[index] >= (u32)hold
        ? period && core->state.gamepads.holds[index] % period ? core->state.gamepads.previous.data : 0
        : core->state.gamepads.previous.data;

    return ((~previous.data) & core->memory.ram.input.gamepads.data) & (1 << index);
}

u32 tic_api_btn(tic_mem* tic, s32 index)
{
    tic_core* core = (tic_core*)tic;

    if (index < 0)
    {
        return (~core->state.gamepads.previous.data) & core->memory.ram.input.gamepads.data;
    }
    else
    {
        return ((~core->state.gamepads.previous.data) & core->memory.ram.input.gamepads.data) & (1 << index);
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
    tic_core* core = (tic_core*)tic;

    if (key > tic_key_unknown)
    {
        bool prevDown = hold >= 0 && period >= 0 && core->state.keyboard.holds[key] >= (u32)hold
            ? period && core->state.keyboard.holds[key] % period
            ? isKeyPressed(&core->state.keyboard.previous, key)
            : false
            : isKeyPressed(&core->state.keyboard.previous, key);

        bool down = isKeyPressed(&tic->ram.input.keyboard, key);

        return !prevDown && down;
    }

    for (s32 i = 0; i < TIC80_KEY_BUFFER; i++)
    {
        tic_key key = tic->ram.input.keyboard.keys[i];

        if (key)
        {
            bool wasPressed = false;

            for (s32 p = 0; p < TIC80_KEY_BUFFER; p++)
            {
                if (core->state.keyboard.previous.keys[p] == key)
                {
                    wasPressed = true;
                    break;
                }
            }

            if (!wasPressed)
                return true;
        }
    }

    return false;
}

tic_point tic_api_mouse(tic_mem* memory)
{
    return (tic_point){memory->ram.input.mouse.x - TIC80_OFFSET_LEFT, memory->ram.input.mouse.y - TIC80_OFFSET_TOP};
}

void tic_core_tick_io(tic_mem* memory)
{
    tic_core* core = (tic_core*)memory;

    // process gamepad
    for (s32 i = 0; i < COUNT_OF(core->state.gamepads.holds); i++)
    {
        u32 mask = 1 << i;
        u32 prevDown = core->state.gamepads.previous.data & mask;
        u32 down = memory->ram.input.gamepads.data & mask;

        u32* hold = &core->state.gamepads.holds[i];
        if (prevDown && prevDown == down) (*hold)++;
        else *hold = 0;
    }

    // process keyboard
    for (s32 i = 0; i < tic_keys_count; i++)
    {
        bool prevDown = isKeyPressed(&core->state.keyboard.previous, i);
        bool down = isKeyPressed(&memory->ram.input.keyboard, i);

        u32* hold = &core->state.keyboard.holds[i];

        if (prevDown && down) (*hold)++;
        else *hold = 0;
    }
}