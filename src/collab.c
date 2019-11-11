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

#include "collab.h"
#include "studio.h"

struct
{
    tic_mem *tic;

	u8* stream;
	s32 streamPos;
	s32 streamSize;
} impl;

struct Collab
{
	s32 offset;
	s32 size;
	s32 count;
	u8* changedBits;
	bool anyChanged;
};

struct Collab* collab_create(s32 offset, s32 size, s32 count)
{
	Collab* collab = (Collab*)malloc(sizeof(Collab));

	collab->offset = offset;
	collab->size = size;
	collab->count = count;

	collab->changedBits = (u8*)malloc(BITARRAY_SIZE(count));
	memset(collab->changedBits, 0, BITARRAY_SIZE(count));

	collab->anyChanged = false;

	return collab;
}

void collab_delete(Collab* collab)
{
	free(collab->changedBits);
}

void collab_diff(Collab* collab, tic_mem* tic)
{
	collab->anyChanged = false;

    u8* cart = (u8*)&tic->cart + collab->offset;
    u8* server = (u8*)&tic->collab + collab->offset;

    for(s32 index = 0; index < collab->count; index++)
    {
        bool diff = memcmp(cart, server, collab->size);

        if(diff)
            collab->anyChanged = true;

        tic_tool_poke1(collab->changedBits, index, diff);

        cart += collab->size;
        server += collab->size;
    }
}

void* collab_data(Collab *collab, tic_mem *tic, s32 index)
{
    return (u8*)&tic->collab + collab->offset + collab->size * index;
}

bool collab_isChanged(Collab* collab, s32 index)
{
	return tic_tool_peek1(collab->changedBits, index);
}

void collab_setChanged(Collab* collab, s32 index, u8 value)
{
	tic_tool_poke1(collab->changedBits, index, value);
}

bool collab_anyChanged(Collab* collab)
{
	return collab->anyChanged;
}

void getCollabData(const char* path, void *dest, s32 destSize)
{
	char url[256];
	snprintf(url, sizeof(url), "%s%s", getCollabUrl(), path);

	s32 size;
	void *buffer = getSystem()->getUrlRequest(url, &size);

	if(buffer)
	{
		if(size == destSize)
			memcpy(dest, buffer, destSize);
		
		free(buffer);
	}
}

void collab_put(Collab* collab, tic_mem* tic)
{
    u8* data = (u8*)&tic->cart + collab->offset;
    s32 size = collab->size * collab->count;

    char url[1024];
    snprintf(url, sizeof(url), "%s/?offset=%d&size=%d", getCollabUrl(), collab->offset, size);
	getSystem()->putUrlRequest(url, data, size);
}

void collab_get(Collab* collab, tic_mem* tic)
{
    u8* data = (u8*)&tic->cart + collab->offset;
    s32 size = collab->size * collab->count;

    char url[1024];
    snprintf(url, sizeof(url), "/?offset=%d&size=%d", collab->offset, size);
    getCollabData(url, data, size);
}

void collab_putRange(Collab* collab, tic_mem* tic, s32 first, s32 count)
{
    s32 offset = collab->offset + collab->size * first;
    s32 size = count * collab->size;
    u8* data = (u8*)&tic->cart + offset;

    char url[1024];
    snprintf(url, sizeof(url), "%s/?offset=%d&size=%d", getCollabUrl(), offset, size);
	getSystem()->putUrlRequest(url, data, size);
}

void collab_getRange(Collab* collab, tic_mem* tic, s32 first, s32 count)
{
    s32 offset = collab->offset + collab->size * first;
    s32 size = count * collab->size;
    u8* data = (u8*)&tic->cart + offset;

    char url[1024];
    snprintf(url, sizeof(url), "/?offset=%d&size=%d", offset, size);
    getCollabData(url, data, size);
}

void collab_putInitialData(tic_mem *tic)
{
    char url[1024];
    snprintf(url, sizeof(url), "%s/?offset=0&size=%d", getCollabUrl(), (int)sizeof(tic_cartridge));
	getSystem()->putUrlRequest(url, &tic->cart, sizeof(tic_cartridge));
}

typedef struct
{
	s32 offset;
	s32 size;
} StreamChunk;

static void collab_streamCallback(u8* buffer, s32 size, void* data)
{
	if(impl.streamPos + size > impl.streamSize)
	{
		impl.streamSize = impl.streamPos + size;
		impl.stream = realloc(impl.stream, impl.streamSize);
	}

	memcpy(impl.stream + impl.streamPos, buffer, size);
	impl.streamPos += size;

	while(impl.streamPos >= sizeof(StreamChunk))
	{
		StreamChunk* chunk = (StreamChunk*)impl.stream;

		if(chunk->offset > sizeof(tic_cartridge))
			continue;
		if(chunk->offset + chunk->size > sizeof(tic_cartridge))
			chunk->size = sizeof(tic_cartridge) - chunk->offset;

		char url[1024];
		snprintf(url, sizeof(url), "/?offset=%d&size=%d", chunk->offset, chunk->size);
		getCollabData(url, (u8*)&impl.tic->collab + chunk->offset, chunk->size);

		memmove(impl.stream, impl.stream + sizeof(StreamChunk), impl.streamPos - sizeof(StreamChunk));
		impl.streamPos -= sizeof(StreamChunk);
	}

    onCollabChanges();
}

void collab_startChangesStream(tic_mem *tic)
{
    impl.tic = tic;

	char url[1024];
	snprintf(url, sizeof(url), "%s/?watch=1", getCollabUrl());
	getSystem()->getUrlStream(url, collab_streamCallback, NULL);
}

void collab_stopChangesStream()
{
	getSystem()->getUrlStream("", NULL, NULL);
}
