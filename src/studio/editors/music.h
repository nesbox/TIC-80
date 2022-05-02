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

#pragma once

#include "studio/studio.h"

typedef struct Music Music;

struct Music
{
    Studio* studio;
    tic_mem* tic;
    tic_music* src;

    u8 track;
    s32 frame;

    struct
    {
        s32 pos;
        s32 start;
        bool active;

    } scroll;

    bool beat34;
    bool follow;
    bool sustain;
    bool loop;
    bool on[TIC_SOUND_CHANNELS];

    struct
    {
        s32 octave;
        s32 sfx;
    } last;

    struct
    {
        s32 col;
        tic_point edit;

        struct
        {
            tic_point start;
            tic_rect rect;
            bool drag;
        } select;
    } tracker;

    struct
    {
        s32 col;
        tic_point edit;
        s8 note[TIC_SOUND_CHANNELS];
    } piano;

    enum
    {
        MUSIC_TRACKER_TAB,
        MUSIC_PIANO_TAB,
        MUSIC_TAB_COUNT,
    } tab;

    struct
    {
        const char* label;
        s32 x;
        s32 start;
    } drag;

    u32 tickCounter;

    struct History* history;
    
    void(*tick)(Music*);
    void(*event)(Music*, StudioEvent);
};

void initMusic(Music*, Studio* studio, tic_music* src);
void freeMusic(Music* music);
