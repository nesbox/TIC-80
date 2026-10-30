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

#define _USE_MATH_DEFINES

#include "anim.h"

#include <math.h>

static inline s32 lerp(s32 a, s32 b, float d)
{
    return (s32)((b - a) * d + a);
}

static inline float bounceOut(float x)
{
    const float n1 = 7.5625;
    const float d1 = 2.75;

    if(x < 1 / d1)
        return n1 * x * x;
    else if(x < 2 / d1)
    {
        x -= (float)(1.5 / d1);
        return (float)(n1 * x * x + 0.75);
    }
    else if(x < 2.5 / d1)
    {
        x -= (float)(2.25 / d1);
        return (float)(n1 * x * x + 0.9375);
    }
    else
    {
        x -= (float)(2.625 / d1);
        return (float)(n1 * x * x + 0.984375);
    }
}

static inline float animEffect(AnimEffect effect, float x)
{
    const float c1 = 1.70158f;
    const float c2 = c1 * 1.525f;
    const float c3 = c1 + 1;
    const float c4 = (float)((2 * M_PI) / 3);
    const float c5 = (float)((2 * M_PI) / 4.5);

    switch(effect)
    {
        case AnimLinear:
            return x;

        case AnimEaseIn:
            return x * x;

        case AnimEaseOut:
            return 1 - (1 - x) * (1 - x);

        case AnimEaseInOut:
            return x < 0.5 ? 2 * x * x : (float)(1 - pow(-2 * x + 2, 2) / 2);

        case AnimEaseInCubic:
            return x * x * x;

        case AnimEaseOutCubic:
            return (float)(1 - pow(1 - x, 3));

        case AnimEaseInOutCubic:
            return x < 0.5 ? 4 * x * x * x : (float)(1 - pow(-2 * x + 2, 3) / 2);

        case AnimEaseInQuart:
            return x * x * x * x;

        case AnimEaseOutQuart:
            return (float)(1 - pow(1 - x, 4));

        case AnimEaseInOutQuart:
            return x < 0.5 ? 8 * x * x * x * x : (float)(1 - pow(-2 * x + 2, 4) / 2);

        case AnimEaseInQuint:
            return x * x * x * x * x;

        case AnimEaseOutQuint:
            return (float)(1 - pow(1 - x, 5));

        case AnimEaseInOutQuint:
            return x < 0.5 ? 16 * x * x * x * x * x : (float)(1 - pow(-2 * x + 2, 5) / 2);

        case AnimEaseInSine:
            return (float)(1 - cos((x * M_PI) / 2));

        case AnimEaseOutSine:
            return (float)(sin((x * M_PI) / 2));

        case AnimEaseInOutSine:
            return (float)(-(cos(M_PI * x) - 1) / 2);

        case AnimEaseInExpo:
            return x == 0 ? 0 : (float)(pow(2, 10 * x - 10));

        case AnimEaseOutExpo:
            return x == 1 ? 1 : (float)(1 - pow(2, -10 * x));

        case AnimEaseInOutExpo:
            return (float)(x == 0
                               ? 0
                           : x == 1
                               ? 1
                           : x < 0.5
                               ? pow(2, 20 * x - 10) / 2
                               : (2 - pow(2, -20 * x + 10)) / 2);

        case AnimEaseInCirc:
            return (float)(1 - sqrt(1 - pow(x, 2)));

        case AnimEaseOutCirc:
            return (float)(sqrt(1 - pow(x - 1, 2)));

        case AnimEaseInOutCirc:
            return x < 0.5
                       ? (float)((1 - sqrt(1 - pow(2 * x, 2))) / 2)
                       : (float)((sqrt(1 - pow(-2 * x + 2, 2)) + 1) / 2);

        case AnimEaseInBack:
            return c3 * x * x * x - c1 * x * x;

        case AnimEaseOutBack:
            return (float)(1 + c3 * pow(x - 1, 3) + c1 * pow(x - 1, 2));

        case AnimEaseInOutBack:
            return x < 0.5
                       ? (float)((pow(2 * x, 2) * ((c2 + 1) * 2 * x - c2)) / 2)
                       : (float)((pow(2 * x - 2, 2) * ((c2 + 1) * (x * 2 - 2) + c2) + 2) / 2);

        case AnimEaseInElastic:
            return (float)(x == 0
                               ? 0
                           : x == 1
                               ? 1
                               : -pow(2, 10 * x - 10) * sin((x * 10 - 10.75) * c4));

        case AnimEaseOutElastic:
            return (float)(x == 0
                               ? 0
                           : x == 1
                               ? 1
                               : pow(2, -10 * x) * sin((x * 10 - 0.75) * c4) + 1);

        case AnimEaseInOutElastic:
            return (float)(x == 0
                               ? 0
                           : x == 1
                               ? 1
                           : x < 0.5
                               ? -(pow(2, 20 * x - 10) * sin((20 * x - 11.125) * c5)) / 2
                               : (pow(2, -20 * x + 10) * sin((20 * x - 11.125) * c5)) / 2 + 1);

        case AnimEaseInBounce:
            return 1 - bounceOut(1 - x);

        case AnimEaseOutBounce:
            return bounceOut(x);

        case AnimEaseInOutBounce:
            return x < 0.5
                       ? (1 - bounceOut(1 - 2 * x)) / 2
                       : (1 + bounceOut(2 * x - 1)) / 2;
    }

    return x;
}

static void animTick(Movie* movie)
{
    for(Anim *it = movie->items, *end = it + movie->count; it != end; ++it)
    {
        float tick = (float)(movie->tick < it->time ? movie->tick : it->time);
        *it->value = lerp(it->start, it->end, animEffect(it->effect, tick / it->time));
    }
}

void processAnim(Movie* movie, void* data)
{
    animTick(movie);

    if(movie->tick == movie->time)
        movie->done(data);

    movie->tick++;
}

Movie* resetMovie(Movie* movie)
{
    movie->tick = 0;
    animTick(movie);

    return movie;
}
