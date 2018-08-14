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

#include "net.h"
#include "tic.h"
#include "SDL_net.h"

#include <stdlib.h>
#include <stdio.h>

struct Net
{
	struct
	{
		u8* buffer;
		s32 size;
		char path[FILENAME_MAX];
	} cache;
};


typedef void(*NetResponse)(u8* buffer, s32 size, void* data);

#if defined(__EMSCRIPTEN__)

static void getRequest(Net* net, const char* path, NetResponse callback, void* data)
{
	callback(NULL, 0, data);
}

#else

static void netClearCache(Net* net)
{
	if(net->cache.buffer)
		free(net->cache.buffer);

	net->cache.buffer = NULL;
	net->cache.size = 0;
	memset(net->cache.path, 0, sizeof net->cache.path);
}

typedef struct
{
	u8* data;
	s32 size;
}Buffer;

static Buffer httpRequest(const char* path, s32 timeout)
{
	Buffer buffer = {.data = NULL, .size = 0};

	{
		IPaddress ip;

		if (SDLNet_ResolveHost(&ip, TIC_HOST, 80) >= 0)
		{
			TCPsocket sock = SDLNet_TCP_Open(&ip);

			if (sock)
			{
				SDLNet_SocketSet set = SDLNet_AllocSocketSet(1);

				if(set)
				{
					SDLNet_TCP_AddSocket(set, sock);

					{
						char message[FILENAME_MAX];
						memset(message, 0, sizeof message);
						sprintf(message, "GET %s HTTP/1.0\r\nHost: " TIC_HOST "\r\n\r\n", path);
						SDLNet_TCP_Send(sock, message, (s32)strlen(message) + 1);
					}

					if(SDLNet_CheckSockets(set, timeout) == 1 && SDLNet_SocketReady(sock))
					{
						enum {Size = 4*1024+1};
						buffer.data = malloc(Size);
						s32 size = 0;

						for(;;)
						{
							size = SDLNet_TCP_Recv(sock, buffer.data + buffer.size, Size-1);

							if(size > 0)
							{
								buffer.size += size;
								buffer.data = realloc(buffer.data, buffer.size + Size);
							}
							else break;
						}

						buffer.data[buffer.size] = '\0';
					}
					
					SDLNet_FreeSocketSet(set);
				}

				SDLNet_TCP_Close(sock);
			}
		}
	}

	return buffer;
}

static void getRequest(Net* net, const char* path, NetResponse callback, void* data)
{
	if(strcmp(net->cache.path, path) == 0)
	{
		callback(net->cache.buffer, net->cache.size, data);
	}
	else
	{
		netClearCache(net);

		bool done = false;

		enum {Timeout = 3000};
		Buffer buffer = httpRequest(path, Timeout);

		if(buffer.data && buffer.size)
		{
			if(strstr((char*)buffer.data, "200 OK"))
			{
				s32 contentLength = 0;

				{
					static const char ContentLength[] = "Content-Length:";

					char* start = strstr((char*)buffer.data, ContentLength);

					if(start)
						contentLength = atoi(start + sizeof(ContentLength));
				}

				static const char Start[] = "\r\n\r\n";
				u8* start = (u8*)strstr((char*)buffer.data, Start);

				if(start)
				{
					strcpy(net->cache.path, path);
					net->cache.size = contentLength ? contentLength : buffer.size - (s32)(start - buffer.data);
					net->cache.buffer = (u8*)malloc(net->cache.size);
					memcpy(net->cache.buffer, start + sizeof Start - 1, net->cache.size);
					callback(net->cache.buffer, net->cache.size, data);
					done = true;
				}
			}

			free(buffer.data);
		}

		if(!done)
			callback(NULL, 0, data);
	}
}

#endif

typedef struct
{
	void* buffer;
	s32* size;
} NetGetData;

static void onGetResponse(u8* buffer, s32 size, void* data)
{
	NetGetData* netGetData = (NetGetData*)data;

	netGetData->buffer = malloc(size);
	*netGetData->size = size;
	memcpy(netGetData->buffer, buffer, size);
}

void* netGetRequest(Net* net, const char* path, s32* size)
{
	NetGetData netGetData = {NULL, size};
	getRequest(net, path, onGetResponse, &netGetData);

	return netGetData.buffer;
}

Net* createNet()
{
	SDLNet_Init();

	Net* net = (Net*)malloc(sizeof(Net));

	*net = (Net)
	{
		.cache = 
		{
			.buffer = NULL,
			.size = 0,
			.path = {0},
		},
	};

	return net;
}

void closeNet(Net* net)
{
	free(net);
	
	SDLNet_Quit();
}