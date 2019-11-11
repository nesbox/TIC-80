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
	struct
	{
		struct Curl_easy* curl;
	} get;

	struct
	{
		struct Curl_easy* curl;
	} put;

	struct
	{
		CURLM* multi;
		struct Curl_easy* curl;
		bool active;
		char url[1024];
		NetResponse callback;
		void* data;
		s32 retryCounter;
	} stream;
};

typedef struct
{
    u8* buffer;
    s32 size;
} CurlData;

static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	CurlData* data = (CurlData*)userp;

	const size_t total = size * nmemb;
	data->buffer = (u8*)realloc(data->buffer, data->size + total);
	memcpy(data->buffer + data->size, contents, total);
	data->size += total;

	return total;
}

void* netGetRequest(Net* net, const char* url, s32* size)
{
	CurlData data = {NULL, 0};

	if(net->get.curl)
	{
		curl_easy_setopt(net->get.curl, CURLOPT_URL, url);
		curl_easy_setopt(net->get.curl, CURLOPT_WRITEDATA, &data);

		if(curl_easy_perform(net->get.curl) != CURLE_OK)
			return NULL;
	}

	*size = data.size;

    return data.buffer;
}

static size_t readCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	CurlData* data = (CurlData*)userp;

	size_t total = MIN(size * nmemb, (size_t)data->size);

	memcpy((u8*)contents, data->buffer, total);

	data->buffer += total;
	data->size -= total;

	return total;
}

void netPutRequest(Net* net, const char *url, void *content, s32 size)
{
	CurlData data = {(u8*)content, size};

	struct curl_slist *headers = NULL;

	if(net->put.curl)
	{
		char contentLength[1024];
		snprintf(contentLength, sizeof(contentLength), "Content-Length: %d", size);
		headers = curl_slist_append(headers, contentLength);
		headers = curl_slist_append(headers, "Expect:");
		headers = curl_slist_append(headers, "Transfer-Encoding:");

		curl_easy_setopt(net->put.curl, CURLOPT_URL, url);
		curl_easy_setopt(net->put.curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(net->put.curl, CURLOPT_READDATA, &data);
		curl_easy_setopt(net->put.curl, CURLOPT_INFILESIZE, size);

		curl_easy_perform(net->put.curl);

		curl_slist_free_all(headers);
	}
}

static size_t streamCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	Net* net = (Net*)userdata;

	size_t total = size * nmemb;
	
	if(net->stream.callback)
		net->stream.callback(ptr, total, net->stream.data);

	return total;
}

static void netStartStream(Net* net)
{
	curl_easy_setopt(net->stream.curl, CURLOPT_URL, net->stream.url);
	curl_easy_setopt(net->stream.curl, CURLOPT_WRITEDATA, net);

	curl_multi_add_handle(net->stream.multi, net->stream.curl);
	net->stream.active = true;
}

static void netStopStream(Net *net)
{
	curl_multi_remove_handle(net->stream.multi, net->stream.curl);
	net->stream.active = false;
}

void netGetStream(Net* net, const char *url, NetResponse callback, void* data)
{
	if(net->stream.active)
		netStopStream(net);

	snprintf(net->stream.url, sizeof(net->stream.url), "%s", url);
	net->stream.callback = callback;
	net->stream.data = data;
	net->stream.retryCounter = 0;

	if(url[0] != '\0')
		netStartStream(net);
}

void netTick(Net *net)
{
	int running;
	curl_multi_perform(net->stream.multi, &running);

	int pending;
	CURLMsg *msg;
	while((msg = curl_multi_info_read(net->stream.multi, &pending)))
	{
		if(msg->easy_handle == net->stream.curl)
		{
			// Occurs in the case of a dropped connection, e.g. server restart or network loss.
			if(msg->msg == CURLMSG_DONE)
			{
				netStopStream(net);
				net->stream.retryCounter = 300;
			}
		}
	}

	if(!net->stream.active && net->stream.retryCounter > 0)
	{
		net->stream.retryCounter--;
		if(net->stream.retryCounter == 0)
			netStartStream(net);
	}
}

Net* createNet()
{
	Net* net = (Net*)malloc(sizeof(Net));

	*net = (Net)
	{
		.get =
		{
			.curl = curl_easy_init(),
		},
		.put = 
		{
			.curl = curl_easy_init(),
		},
		.stream = 
		{
			.multi = curl_multi_init(),
			.curl = curl_easy_init(),
			.active = false,
			.callback = NULL,
			.data = NULL,
		},
	};

	curl_easy_setopt(net->get.curl, CURLOPT_WRITEFUNCTION, writeCallback);

	curl_easy_setopt(net->put.curl, CURLOPT_PUT, 1);
	curl_easy_setopt(net->put.curl, CURLOPT_READFUNCTION, readCallback);

	curl_easy_setopt(net->stream.curl, CURLOPT_WRITEFUNCTION, streamCallback);

	// Make the OS check for dropped connections automatically:
	curl_easy_setopt(net->stream.curl, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(net->stream.curl, CURLOPT_TCP_KEEPIDLE, 120L);
	curl_easy_setopt(net->stream.curl, CURLOPT_TCP_KEEPINTVL, 60L);

	return net;
}

void closeNet(Net* net)
{
	if(net->get.curl)
		curl_easy_cleanup(net->get.curl);

	if(net->put.curl)
		curl_easy_cleanup(net->put.curl);

	if(net->stream.active)
		netStopStream(net);

	if(net->stream.curl)
		curl_easy_cleanup(net->stream.curl);

	if(net->stream.multi)
		curl_multi_cleanup(net->stream.multi);

	free(net);
}
