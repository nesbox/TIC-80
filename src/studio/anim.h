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

typedef enum
{
    AnimLinear,
    AnimEaseIn,
    AnimEaseOut,
    AnimEaseInOut,
    AnimEaseInCubic,
    AnimEaseOutCubic,
    AnimEaseInOutCubic,
    AnimEaseInQuart,
    AnimEaseOutQuart,
    AnimEaseInOutQuart,
    AnimEaseInQuint,
    AnimEaseOutQuint,
    AnimEaseInOutQuint,
    AnimEaseInSine,
    AnimEaseOutSine,
    AnimEaseInOutSine,
    AnimEaseInExpo,
    AnimEaseOutExpo,
    AnimEaseInOutExpo,
    AnimEaseInCirc,
    AnimEaseOutCirc,
    AnimEaseInOutCirc,
    AnimEaseInBack,
    AnimEaseOutBack,
    AnimEaseInOutBack,
    AnimEaseInElastic,
    AnimEaseOutElastic,
    AnimEaseInOutElastic,
    AnimEaseInBounce,
    AnimEaseOutBounce,
    AnimEaseInOutBounce,
} AnimEffect;

typedef struct
{
    s32 start;
    s32 end;
    s32 time;

    s32 *value;

    AnimEffect effect;
} Anim;

typedef struct
{
    void(*done)(void *data);

    s32 time;
    s32 tick;

    s32 count;
    Anim* items;
} Movie;

#define MOVIE_DEF(TIME, DONE, ...)              \
{                                               \
    .time = TIME,                               \
    .done = DONE,                               \
    .count = COUNT_OF(((Anim[])__VA_ARGS__)),   \
    .items = MOVE((Anim[])__VA_ARGS__),         \
}

void processAnim(Movie* movie, void* data);
Movie* resetMovie(Movie* movie);
