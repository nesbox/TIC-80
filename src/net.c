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

#include <stdlib.h>
#include <stdio.h>

#include <curl/curl.h>

struct Net
{
	struct Curl_easy* curl;
};

typedef struct
{
    void* buffer;
    s32 size;
} CurlData;

static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	CurlData* data = (CurlData*)userp;

	const size_t total = size * nmemb;
	data->buffer = realloc(data->buffer, data->size + total);
	memcpy((u8*)data->buffer + data->size, contents, total);
	data->size += total;

	return total;
}

void* netGetRequest(Net* net, const char* path, s32* size)
{
	CurlData data = {NULL, 0};

	if(net->curl)
	{
		char url[FILENAME_MAX] = "http://" TIC_HOST;
		strcat(url, path);
		
		curl_easy_setopt(net->curl, CURLOPT_URL, url);
		curl_easy_setopt(net->curl, CURLOPT_WRITEDATA, &data);

		if(curl_easy_perform(net->curl) != CURLE_OK)
			return NULL;
	}

	*size = data.size;

    return data.buffer;
}

Net* createNet()
{
	Net* net = (Net*)malloc(sizeof(Net));

	*net = (Net)
	{
		.curl = curl_easy_init()
	};

	curl_easy_setopt(net->curl, CURLOPT_WRITEFUNCTION, writeCallback);

	return net;
}

void closeNet(Net* net)
{
	if(net->curl)
		curl_easy_cleanup(net->curl);

	free(net);
}
