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

#include "json.h"

#define JSMN_STATIC
#define JSMN_PARENT_LINKS
#include <jsmn.h>

#include <stdio.h>

static struct
{
    jsmn_parser p;
    jsmntok_t* t;
    s32 tcount, count;
    const char* json;
} state = {.tcount = 128};

bool json_parse(const char *json, s32 size)
{
    if(state.t == NULL)
        state.t = malloc(state.tcount * sizeof *state.t);

    jsmn_init(&state.p);

    state.json = json;

    while((state.count = jsmn_parse(&state.p, state.json, size, state.t, state.tcount)) == JSMN_ERROR_NOMEM)
        state.t = realloc(state.t, (state.tcount *= 2) * sizeof *state.t);

    return state.count >= 0 && state.t[0].type == JSMN_OBJECT;
}

static s32 jsoneq(const char *json, const jsmntok_t *tok, const char *s) 
{
    if (tok->type & JSMN_STRING 
        && strlen(s) == tok->end - tok->start 
        && strncmp(json + tok->start, s, tok->end - tok->start) == 0) 
        return 0;

    return -1;
}

static s32 getJsonItem(const char *var, s32 parent, jsmntype_t type)
{
    for(s32 i = parent; i < state.count; i++)
        if(jsoneq(state.json, &state.t[i], var) == 0 && state.t[i + 1].type & type)
            return i + 1;

    return 0;
}

bool json_bool(const char *var, s32 parent)
{
    const jsmntok_t *t = &state.t[getJsonItem(var, parent, JSMN_PRIMITIVE)];
    return strncmp(state.json + t->start, "true", t->end - t->start) == 0;
}

s32 json_int(const char *var, s32 parent)
{
    return atoi(state.json + state.t[getJsonItem(var, parent, JSMN_PRIMITIVE)].start);
}

bool json_string(const char *var, s32 parent, char* value, s32 size)
{
    const jsmntok_t* t = &state.t[getJsonItem(var, parent, JSMN_STRING)];
    return snprintf(value, size, "%.*s", t->end - t->start, state.json + t->start) > 0;
}

s32 json_array(const char *var, s32 parent)
{
    return getJsonItem(var, parent, JSMN_ARRAY);
}

s32 json_array_size(s32 array)
{
    s32 count = 0;

    for(s32 i = state.t[array].parent; i < state.count; i++)
        if(state.t[i].parent == array)
            count++;

    return count;
}

s32 json_array_item(s32 array, s32 index)
{
    for(s32 i = state.t[array].parent, count = 0; i < state.count; i++)
        if(state.t[i].parent == array && index == count++)
            return i;

    return 0;
}

s32 json_object(const char *var, s32 parent)
{
    return getJsonItem(var, parent, JSMN_OBJECT);
}
