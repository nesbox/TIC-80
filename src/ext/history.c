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

#include "history.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    u8* buffer;
    u32 start;
    u32 end;
} Data;

typedef struct Item Item;

struct Item
{
    Item* next;
    Item* prev;

    Data data;
};

static void list_delete(Item* list, Item* from)
{
    Item* it = from;

    while(it)
    {
        Item* next = it->next;

        if(it->data.buffer) free(it->data.buffer);
        
        free(it);

        it = next;
    }
}

static Item* list_insert(Item* list, Data* data)
{
    Item* item = (Item*)malloc(sizeof(Item));
    item->next = NULL;
    item->prev = NULL;
    item->data = *data;

    if(list)
    {
        list_delete(list, list->next);

        list->next = item;
        item->prev = list;
    }

    return item;
}

static Item* list_first(Item* list)
{
    Item* it = list;
    while(it->prev) it = it->prev;

    return it;
}

struct History
{
    Item* list;

    u32 size;
    u8* state;

    void* data;
};

History* history_create(void* data, u32 size)
{
    History* history = (History*)malloc(sizeof(History));
    history->data = data;

    history->list = NULL;
    history->size = size;

    history->state = malloc(size);
    memcpy(history->state, data, history->size);

    // empty diff
    history->list = list_insert(history->list, &(Data){NULL, 0, 0});

    return history;
}

void history_delete(History* history)
{
    if(history)
    {
        free(history->state);

        list_delete(history->list, list_first(history->list));

        free(history);
    }
}

static void history_diff(History* history, Data* data)
{
    for (u32 i = data->start, k = 0; i < data->end; ++i, ++k)
        history->state[i] ^= data->buffer[k];
}

static u32 trim_left(u8* data, u32 size)
{
    for(u32 i = 0; i < size; i++)
        if(data[i]) return i;

    return size;
}

static u32 trim_right(u8* data, u32 size)
{
    for(u32 i = 0; i < size; i++)
        if(data[size - i - 1]) return size - i;

    return 0;
}

bool history_add(History* history)
{
    if (memcmp(history->state, history->data, history->size) == 0) return false;

    history_diff(history, &(Data){history->data, 0, history->size});

    {
        Data data;
        data.start = trim_left(history->state, history->size);
        data.end = trim_right(history->state, history->size);
        u32 size = data.end - data.start;
        data.buffer = malloc(size);

        memcpy(data.buffer, (u8*)history->state + data.start, size);

        history->list = list_insert(history->list, &data);
    }

    memcpy(history->state, history->data, history->size);

    return true;
}

void history_undo(History* history)
{
    if(history->list->prev)
    {
        history_diff(history, &history->list->data);

        history->list = history->list->prev;
    }

    memcpy(history->data, history->state, history->size);
}

void history_redo(History* history)
{
    if(history->list->next)
    {
        history->list = history->list->next;

        history_diff(history, &history->list->data);
    }

    memcpy(history->data, history->state, history->size);
}
