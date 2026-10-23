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
#include <string.h>

// The tests point these at a counting allocator; the studio uses the defaults.
#if defined(HISTORY_TEST_ALLOC)
void* history_test_malloc(size_t size);
void history_test_free(void* ptr);

#define HISTORY_MALLOC history_test_malloc
#define HISTORY_FREE   history_test_free
#else
#define HISTORY_MALLOC malloc
#define HISTORY_FREE   free
#endif

// Stored deltas stay under this budget; the oldest steps are dropped first.
#ifndef HISTORY_MAX_BYTES
#define HISTORY_MAX_BYTES (64ull * 1024 * 1024)
#endif

// One undo step: the XOR of the states on its two sides over [start, end).
typedef struct Item
{
    struct Item* prev;
    struct Item* next;

    u8* delta;
    u32 start;
    u32 end;
} Item;

struct History
{
    void* data;
    u32 size;

    // The last committed state: what a new commit is diffed against.
    u8* state;

    Item* first;
    Item* current;
    u64 bytes;
};

// Frees a chain and returns the delta bytes it held.
static u64 items_delete(Item* item)
{
    u64 bytes = 0;

    while(item)
    {
        Item* next = item->next;

        bytes += item->end - item->start;

        HISTORY_FREE(item->delta);
        HISTORY_FREE(item);

        item = next;
    }

    return bytes;
}

static void apply(const Item* item, u8* state)
{
    for(u32 i = item->start; i < item->end; i++)
        state[i] ^= item->delta[i - item->start];
}

History* history_create(void* data, u32 size)
{
    History* history = HISTORY_MALLOC(sizeof(History));

    if(!history) return NULL;

    // One byte for an empty buffer, so no copy or delta ever sees NULL.
    history->state = HISTORY_MALLOC(size ? size : 1);
    history->current = HISTORY_MALLOC(sizeof(Item));

    if(!history->state || !history->current)
    {
        HISTORY_FREE(history->state);
        HISTORY_FREE(history->current);
        HISTORY_FREE(history);

        return NULL;
    }

    history->data = data;
    history->size = size;

    if(size) memcpy(history->state, data, size);

    history->current->prev = NULL;
    history->current->next = NULL;
    history->current->delta = NULL;
    history->current->start = 0;
    history->current->end = 0;

    history->first = history->current;
    history->bytes = 0;

    return history;
}

void history_delete(History* history)
{
    if(history)
    {
        items_delete(history->first);

        HISTORY_FREE(history->state);
        HISTORY_FREE(history);
    }
}

bool history_add(History* history)
{
    if(!history) return false;

    const u8* state = history->state;
    const u8* data = history->data;
    u32 size = history->size;

    u32 start = 0;
    while(start < size && state[start] == data[start])
        start++;

    if(start == size) return false;

    u32 end = size;
    while(end > start && state[end - 1] == data[end - 1])
        end--;

    u8* delta = HISTORY_MALLOC(end - start);

    if(!delta) return false;

    for(u32 i = start; i < end; i++)
        delta[i - start] = state[i] ^ data[i];

    Item* item = HISTORY_MALLOC(sizeof(Item));

    if(!item)
    {
        HISTORY_FREE(delta);
        return false;
    }

    item->prev = history->current;
    item->next = NULL;
    item->delta = delta;
    item->start = start;
    item->end = end;

    // A new step replaces the redo branch it was made after.
    history->bytes -= items_delete(history->current->next);
    history->current->next = item;
    history->current = item;
    history->bytes += item->end - item->start;

    // Keep the deltas under the budget, dropping the oldest steps; the
    // current step is always kept, even when it exceeds the budget alone.
    while(history->bytes > HISTORY_MAX_BYTES && history->first != history->current)
    {
        Item* first = history->first;

        history->first = first->next;
        history->first->prev = NULL;
        history->bytes -= first->end - first->start;

        first->next = NULL;
        HISTORY_FREE(first->delta);
        HISTORY_FREE(first);
    }

    if(size) memcpy(history->state, history->data, size);

    return true;
}

void history_undo(History* history)
{
    if(history)
    {
        if(history->current->prev)
        {
            apply(history->current, history->state);
            history->current = history->current->prev;
        }

        if(history->size) memcpy(history->data, history->state, history->size);
    }
}

void history_redo(History* history)
{
    if(history)
    {
        if(history->current->next)
        {
            history->current = history->current->next;
            apply(history->current, history->state);
        }

        if(history->size) memcpy(history->data, history->state, history->size);
    }
}
